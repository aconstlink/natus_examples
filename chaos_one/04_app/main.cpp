
#include "main.h"

#include <natus/application/global.h>
#include <natus/application/app.h>
#include <natus/gfx/camera/pinhole_camera.h>
#include <natus/gpu/variable/variable_set.hpp>
#include <natus/math/vector/vector3.hpp>
#include <natus/math/vector/vector4.hpp>
#include <natus/math/matrix/matrix4.hpp>

#include <thread>

namespace this_file
{
    using namespace natus::core::types ;

    class test_app : public natus::application::app
    {
        natus_this_typedefs( test_app ) ;

    private:

        app::wid_async_t _wid_async ;
        
        natus::gpu::render_state_sets_res_t _render_states = natus::gpu::render_state_sets_t() ;
        natus::gpu::render_configuration_res_t _rc = natus::gpu::render_configuration_t() ;
        natus::gpu::geometry_configuration_res_t _gconfig = natus::gpu::geometry_configuration_t() ;
        natus::gpu::image_configuration_res_t _imgconfig = natus::gpu::image_configuration_t() ;

        struct vertex { natus::math::vec3f_t pos ; natus::math::vec2f_t tx ; } ;
        
        typedef ::std::chrono::high_resolution_clock __clock_t ;
        __clock_t::time_point _tp = __clock_t::now() ;

        natus::gfx::pinhole_camera_t _camera_0 ;
        natus::gfx::pinhole_camera_t _camera_1 ;

    public:

        test_app( void_t ) 
        {
            natus::application::app::window_info_t wi ;
            _wid_async = this_t::create_window( "A Render Window", wi ) ;
        }
        test_app( this_cref_t ) = delete ;
        test_app( this_rref_t rhv ) : app( ::std::move( rhv ) ) 
        {
            _wid_async = ::std::move( rhv._wid_async ) ;
            _camera_0 = ::std::move( rhv._camera_0 ) ;
            _camera_1 = ::std::move( rhv._camera_1 ) ;
            _gconfig = ::std::move( rhv._gconfig ) ;
            _rc = ::std::move( rhv._rc) ;
        }
        virtual ~test_app( void_t ) 
        {}

    private:

        virtual natus::application::result on_init( void_t )
        { 
            {
                _camera_0.orthographic( 2.0f, 2.0f, 1.0f, 1000.0f ) ;
                _camera_1.orthographic( 2.0f, 2.0f, 1.0f, 1000.0f ) ;
            }

            // geometry configuration
            {
                auto vb = natus::gpu::vertex_buffer_t()
                    .add_layout_element( natus::gpu::vertex_attribute::position, natus::gpu::type::tfloat, natus::gpu::type_struct::vec3 )
                    .add_layout_element( natus::gpu::vertex_attribute::texcoord0, natus::gpu::type::tfloat, natus::gpu::type_struct::vec2 )
                    .resize( 4 ).update<vertex>( [=] ( vertex* array, size_t const ne )
                {
                    array[ 0 ].pos = natus::math::vec3f_t( -0.5f, -0.5f, 0.0f ) ;
                    array[ 1 ].pos = natus::math::vec3f_t( -0.5f, +0.5f, 0.0f ) ;
                    array[ 2 ].pos = natus::math::vec3f_t( +0.5f, +0.5f, 0.0f ) ;
                    array[ 3 ].pos = natus::math::vec3f_t( +0.5f, -0.5f, 0.0f ) ;

                    array[ 0 ].tx = natus::math::vec2f_t( -0.0f, -0.0f ) ;
                    array[ 1 ].tx = natus::math::vec2f_t( -0.0f, +1.0f ) ;
                    array[ 2 ].tx = natus::math::vec2f_t( +1.0f, +1.0f ) ;
                    array[ 3 ].tx = natus::math::vec2f_t( +1.0f, -0.0f ) ;
                } );

                auto ib = natus::gpu::index_buffer_t().
                    set_layout_element( natus::gpu::type::tuint ).resize( 6 ).
                    update<uint_t>( [] ( uint_t* array, size_t const ne )
                {
                    array[ 0 ] = 0 ;
                    array[ 1 ] = 1 ;
                    array[ 2 ] = 2 ;

                    array[ 3 ] = 0 ;
                    array[ 4 ] = 2 ;
                    array[ 5 ] = 3 ;
                } ) ;

                _gconfig = natus::gpu::geometry_configuration_t( "quad",
                    natus::gpu::primitive_type::triangles, ::std::move( vb ), ::std::move( ib ) ) ;

                _wid_async.second.configure( _gconfig ) ;
            }

            // image configuration
            {
                natus::gpu::image_t img = natus::gpu::image_t( natus::gpu::image_t::dims_t( 100, 100 ) )
                    .update( [&]( natus::gpu::image_ptr_t, natus::gpu::image_t::dims_in_t dims, void_ptr_t data_in )
                {
                    typedef natus::math::vector4< uint8_t > rgba_t ;
                    auto* data = reinterpret_cast< rgba_t * >( data_in ) ;

                    size_t const w = 5 ;

                    size_t i = 0 ; 
                    for( size_t y = 0; y < dims.y(); ++y )
                    {
                        bool_t const odd = ( y / w ) & 1 ;

                        for( size_t x = 0; x < dims.x(); ++x )
                        {
                            bool_t const even = ( x / w ) & 1 ;

                            data[ i++ ] = even || odd ? rgba_t( 255 ) : rgba_t( 0, 0, 0, 255 );
                            //data[ i++ ] = rgba_t(255) ;
                        }
                    }
                } ) ;

                _imgconfig = natus::gpu::image_configuration_t( "checker_board", ::std::move( img ) )
                    .set_wrap( natus::gpu::texture_wrap_mode::wrap_s, natus::gpu::texture_wrap_type::repeat )
                    .set_wrap( natus::gpu::texture_wrap_mode::wrap_t, natus::gpu::texture_wrap_type::repeat )
                    .set_filter( natus::gpu::texture_filter_mode::min_filter, natus::gpu::texture_filter_type::nearest )
                    .set_filter( natus::gpu::texture_filter_mode::mag_filter, natus::gpu::texture_filter_type::nearest );

                _wid_async.second.configure( _imgconfig ) ;
            }

            // shader configuration
            {
                natus::gpu::shader_configuration_t sc( "quad" ) ;

                // shaders : ogl 3.0
                {
                    natus::gpu::shader_set_t ss = natus::gpu::shader_set_t().

                        set_vertex_shader( natus::gpu::shader_t( R"(
                            #version 140
                            in vec3 in_pos ;
                            in vec2 in_tx ;
                            out vec2 var_tx0 ;
                            uniform mat4 u_proj ;
                            uniform mat4 u_view ;
                            
                            void main()
                            {
                                var_tx0 = in_tx ;
                                gl_Position = u_proj * u_view * vec4( in_pos, 1.0 ) ;
                            } )" ) ).

                        set_pixel_shader( natus::gpu::shader_t( R"(
                            #version 140
                            out vec4 out_color ;
                            in vec2 var_tx0 ;

                            uniform sampler2D u_tex ;
                            uniform vec4 u_color ;
                        
                            void main()
                            {    
                                out_color = u_color * texture( u_tex, var_tx0 ) ;
                            } )" ) ) ;
                    
                    sc.insert( natus::gpu::backend_type::gl3, ::std::move(ss) ) ;
                }

                // shaders : es 3.0
                {
                    natus::gpu::shader_set_t ss = natus::gpu::shader_set_t().

                        set_vertex_shader( natus::gpu::shader_t( R"(
                            #version 300 es
                            in vec3 in_pos ;
                            uniform mat4 u_proj ;
                            uniform mat4 u_view ;

                            void main()
                            {
                                gl_Position = u_proj * u_view * vec4( in_pos, 1.0 ) ;
                            } )" ) ).
                        
                        set_pixel_shader( natus::gpu::shader_t( R"(
                            #version 300 es
                            precision mediump float ;
                            out vec4 out_color ;

                            uniform vec4 u_color ;
                        
                            void main()
                            {    
                                out_color = u_color ;
                            } )" ) ) ;

                    sc.insert( natus::gpu::backend_type::es3, ::std::move(ss) ) ;
                }

                // configure more details
                {
                    sc
                        .add_vertex_input_binding( natus::gpu::vertex_attribute::position, "in_pos" )
                        .add_vertex_input_binding( natus::gpu::vertex_attribute::texcoord0, "in_tx" )
                        .add_input_binding( natus::gpu::binding_point::view_matrix, "u_view" )
                        .add_input_binding( natus::gpu::binding_point::projection_matrix, "u_proj" ) ;
                }

                _wid_async.second.configure( sc ) ;
            }

            {
                _rc->link_geometry( "quad" ) ;
                _rc->link_shader( "quad" ) ;
            }

            // add variable set 0
            {
                natus::gpu::variable_set_res_t vars = natus::gpu::variable_set_t() ;
                {
                    auto* var = vars->data_variable< natus::math::vec4f_t >( "u_color" ) ;
                    var->set( natus::math::vec4f_t( 1.0f, 0.0f, 0.0f, 1.0f ) ) ;
                }

                {
                    auto* var = vars->data_variable< float_t >( "u_time" ) ;
                    var->set( 0.0f ) ;
                }

                {
                    auto * var = vars->texture_variable( "u_tex" ) ;
                    var->set( "checker_board" ) ;
                }

                _rc->add_variable_set( ::std::move( vars ) ) ;
            }

            // add variable set 1
            {
                natus::gpu::variable_set_res_t vars = natus::gpu::variable_set_t() ;
                {
                    auto* var = vars->data_variable< natus::math::vec4f_t >( "u_color" ) ;
                    var->set( natus::math::vec4f_t( 0.0f, 1.0f, 0.0f, 1.0f ) ) ;
                }

                {
                    auto* var = vars->data_variable< float_t >( "u_time" ) ;
                    var->set( 0.0f ) ;
                }

                {
                    auto* var = vars->texture_variable( "u_tex" ) ;
                    var->set( "checker_board" ) ;
                }

                _rc->add_variable_set( ::std::move( vars ) ) ;
            }

            {
                natus::gpu::variable_set_res_t vars = natus::gpu::variable_set_t() ;
                {
                    auto* var = vars->data_variable< natus::math::vec4f_t >( "u_color" ) ;
                    var->set( natus::math::vec4f_t( 0.0f, 0.0f, 1.0f, 1.0f ) ) ;
                }

                {
                    auto* var = vars->data_variable< float_t >( "u_time" ) ;
                    var->set( 0.0f ) ;
                }

                {
                    auto* var = vars->texture_variable( "u_tex" ) ;
                    var->set( "checker_board" ) ;
                }

                _rc->add_variable_set( ::std::move( vars ) ) ;
            }

            _wid_async.second.configure( _rc ) ;

            return natus::application::result::ok ; 
        }

        float value = 0.0f ;
        size_t ucount = 0 ;

        virtual natus::application::result on_update( void_t ) 
        { 
            
            auto const dif = ::std::chrono::duration_cast< ::std::chrono::microseconds >( __clock_t::now() - _tp ) ;
            _tp = __clock_t::now() ;


            float_t const dt = float_t( double_t( dif.count() ) / ::std::chrono::microseconds::period().den ) ;
            
            if( value > 1.0f ) value = 0.0f ;
            value += natus::math::fn<float_t>::fract( dt ) ;

            {
                static float_t t = 0.0f ;
                t += dt * 0.1f ;

                if( t > 1.0f ) t = 0.0f ;
                
                static natus::math::vec3f_t tr ;
                tr.x( 1.0f * natus::math::fn<float_t>::sin( t * 2.0f * 3.14f ) ) ;

                _camera_0.translate_by( tr ) ;
            }

            {
                static float_t t = 0.0f ;
                t += dt * 0.1f ;

                if( t > 1.0f ) t = 0.0f ;

                static natus::math::vec3f_t tr ;
                tr.x( -1.0f * natus::math::fn<float_t>::sin( t * 2.0f * 3.14f ) ) ;

                _camera_1.translate_by( tr ) ;
            }

            ucount++ ;

            ::std::this_thread::sleep_for( ::std::chrono::milliseconds( 1 ) ) ;

            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_render( void_t ) 
        { 
            static float_t v = 0.0f ;
            v += 0.01f ;
            if( v > 1.0f ) v = 0.0f ;

            // per frame update of geometry 
            {
                _gconfig->vertex_buffer().update<vertex>( [=] ( vertex* array, size_t const ne )
                {
                    float_t const x = natus::math::fn<float_t>::abs( natus::math::fn<float_t>::sin( v * 3.14f * 2.0f ) * 0.5f ) ;

                    array[ 0 ].pos = natus::math::vec3f_t( -x, -0.5f, 0.0f ) ;
                    array[ 1 ].pos = natus::math::vec3f_t( -0.5f, +0.5f, 0.0f ) ;
                    array[ 2 ].pos = natus::math::vec3f_t( +x, +0.5f, 0.0f ) ;
                    array[ 3 ].pos = natus::math::vec3f_t( +0.5f, -0.5f, 0.0f ) ;
                } );
                _wid_async.second.update( _gconfig ) ;
            }

            // per frame update of variables
            _rc->for_each( [&] ( size_t const i, natus::gpu::variable_set_res_t const& vs )
            {
                {
                    auto* var = vs->data_variable< natus::math::mat4f_t >( "u_view" ) ;
                    var->set( _camera_0.mat_view() ) ;
                }

                {
                    auto* var = vs->data_variable< natus::math::mat4f_t >( "u_proj" ) ;
                    var->set( _camera_0.mat_proj() ) ;
                }

                if( i == 0 )
                {
                    auto* var = vs->data_variable< natus::math::vec4f_t >( "u_color" ) ;
                    var->set( natus::math::vec4f_t( v, 0.0f, 1.0f, 0.5f ) ) ;

                    {
                        auto* var = vs->data_variable< natus::math::mat4f_t >( "u_view" ) ;
                        var->set( _camera_1.mat_view() ) ;
                    }

                    {
                        auto* var = vs->data_variable< natus::math::mat4f_t >( "u_proj" ) ;
                        var->set( _camera_1.mat_proj() ) ;
                    }
                }
                else if( i == 1 )
                {
                    auto* var = vs->data_variable< natus::math::vec4f_t >( "u_color" ) ;
                    var->set( natus::math::vec4f_t( 0.0f, v, 1.0f, 1.0f ) ) ;
                }
                else if( i == 2 )
                {
                    auto* var = vs->data_variable< natus::math::vec4f_t >( "u_color" ) ;
                    var->set( natus::math::vec4f_t( v, v, 1.0f-v, 1.0f ) ) ;
                }
            } ) ;

            /*{
                natus::gpu::scissor_state_set ss ;
                ss.rect = natus::math::vec4ui_t( 0, 0, 200, 200 ) ;
                ss.do_scissor_test = true ;
                _render_states->scissor_s = ss ;
            }*/
            {
                natus::gpu::backend_t::render_detail_t detail ;
                detail.start = 0 ;
                //detail.num_elems = 3 ;
                detail.varset = 2 ;
                detail.render_states = _render_states ;
                _wid_async.second.render( _rc, detail ) ;
            }

            {
                natus::gpu::render_state_sets_res_t rs = natus::gpu::render_state_sets_t() ;

                {
                    natus::gpu::blend_state_set bs ;
                    bs.do_blend = true ;
                    bs.src_blend_factor = natus::gpu::blend_factor::one ;
                    bs.dst_blend_factor = natus::gpu::blend_factor::one_minus_src_alpha ;
                    rs->blend_s = bs ;
                }

                natus::gpu::backend_t::render_detail_t detail ;
                detail.start = 0 ;
                //detail.num_elems = 3 ;
                detail.varset = 0 ;
                detail.render_states = rs ;
                _wid_async.second.render( _rc, detail ) ;
            }

            // print calls from run-time per second
            {
                static __clock_t::time_point tp = __clock_t::now() ;

                auto const dif = ::std::chrono::duration_cast< ::std::chrono::microseconds >( __clock_t::now() - tp ) ;
                tp = __clock_t::now() ;

                float_t const sec = float_t( double_t( dif.count() ) / ::std::chrono::microseconds::period().den ) ;

                static size_t count = 0 ;
                count++ ;
                static float this_sec = 0.0f ;
                this_sec += sec ;
                if( this_sec > 1.0 )
                {
                    natus::log::global_t::status( "Update Count: " + ::std::to_string( ucount ) ) ;
                    natus::log::global_t::status( "Render Count: " + ::std::to_string( count ) ) ;
                    this_sec = 0.0f ;
                    count = 0 ;
                    ucount = 0 ;
                }
            }

            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_shutdown( void_t ) 
        { return natus::application::result::ok ; }
    };
    natus_soil_typedef( test_app ) ;
}

int main( int argc, char ** argv )
{
    return natus::application::global_t::create_application( 
        this_file::test_app_res_t( this_file::test_app_t() ) )->exec() ;
}
