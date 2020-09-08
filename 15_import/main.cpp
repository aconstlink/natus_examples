
#include "main.h"

#include <natus/application/global.h>
#include <natus/application/app.h>
#include <natus/gfx/camera/pinhole_camera.h>
#include <natus/graphics/variable/variable_set.hpp>
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

        app::window_async_t _wid_async ;
        
        natus::graphics::render_state_sets_res_t _render_states = natus::graphics::render_state_sets_t() ;
        natus::graphics::render_configuration_res_t _rc = natus::graphics::render_configuration_t() ;
        natus::graphics::geometry_configuration_res_t _gconfig = natus::graphics::geometry_configuration_t() ;
        natus::graphics::image_configuration_res_t _imgconfig = natus::graphics::image_configuration_t() ;

        struct vertex { natus::math::vec3f_t pos ; natus::math::vec2f_t tx ; } ;
        
        typedef ::std::chrono::high_resolution_clock __clock_t ;
        __clock_t::time_point _tp = __clock_t::now() ;

        natus::gfx::pinhole_camera_t _camera_0 ;

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
            }

            // geometry configuration
            {
                auto vb = natus::graphics::vertex_buffer_t()
                    .add_layout_element( natus::graphics::vertex_attribute::position, natus::graphics::type::tfloat, natus::graphics::type_struct::vec3 )
                    .add_layout_element( natus::graphics::vertex_attribute::texcoord0, natus::graphics::type::tfloat, natus::graphics::type_struct::vec2 )
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

                auto ib = natus::graphics::index_buffer_t().
                    set_layout_element( natus::graphics::type::tuint ).resize( 6 ).
                    update<uint_t>( [] ( uint_t* array, size_t const ne )
                {
                    array[ 0 ] = 0 ;
                    array[ 1 ] = 1 ;
                    array[ 2 ] = 2 ;

                    array[ 3 ] = 0 ;
                    array[ 4 ] = 2 ;
                    array[ 5 ] = 3 ;
                } ) ;

                _gconfig = natus::graphics::geometry_configuration_t( "quad",
                    natus::graphics::primitive_type::triangles, ::std::move( vb ), ::std::move( ib ) ) ;

                _wid_async.second.configure( _gconfig ) ;
            }

            // image configuration
            {
                natus::graphics::image_t img = natus::graphics::image_t( natus::graphics::image_t::dims_t( 100, 100 ) )
                    .update( [&]( natus::graphics::image_ptr_t, natus::graphics::image_t::dims_in_t dims, void_ptr_t data_in )
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

                _imgconfig = natus::graphics::image_configuration_t( "loaded_image", ::std::move( img ) )
                    .set_wrap( natus::graphics::texture_wrap_mode::wrap_s, natus::graphics::texture_wrap_type::repeat )
                    .set_wrap( natus::graphics::texture_wrap_mode::wrap_t, natus::graphics::texture_wrap_type::repeat )
                    .set_filter( natus::graphics::texture_filter_mode::min_filter, natus::graphics::texture_filter_type::nearest )
                    .set_filter( natus::graphics::texture_filter_mode::mag_filter, natus::graphics::texture_filter_type::nearest );

                _wid_async.second.configure( _imgconfig ) ;
            }

            // shader configuration
            {
                natus::graphics::shader_configuration_t sc( "quad" ) ;

                // shaders : ogl 3.0
                {
                    natus::graphics::shader_set_t ss = natus::graphics::shader_set_t().

                        set_vertex_shader( natus::graphics::shader_t( R"(
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

                        set_pixel_shader( natus::graphics::shader_t( R"(
                            #version 140
                            out vec4 out_color ;
                            in vec2 var_tx0 ;

                            uniform sampler2D u_tex ;
                        
                            void main()
                            {    
                                out_color = texture( u_tex, var_tx0 ) ;
                            } )" ) ) ;
                    
                    sc.insert( natus::graphics::backend_type::gl3, ::std::move(ss) ) ;
                }

                // shaders : es 3.0
                {
                    natus::graphics::shader_set_t ss = natus::graphics::shader_set_t().

                        set_vertex_shader( natus::graphics::shader_t( R"(
                            #version 300 es
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
                        
                        set_pixel_shader( natus::graphics::shader_t( R"(
                            #version 300 es
                            precision mediump float ;
                            out vec4 out_color ;
                            in vec2 var_tx0 ;

                            uniform sampler2D u_tex ;
                        
                            void main()
                            {    
                                out_color = texture( u_tex, var_tx0 ) ;
                            } )" ) ) ;

                    sc.insert( natus::graphics::backend_type::es3, ::std::move(ss) ) ;
                }

                // configure more details
                {
                    sc
                        .add_vertex_input_binding( natus::graphics::vertex_attribute::position, "in_pos" )
                        .add_vertex_input_binding( natus::graphics::vertex_attribute::texcoord0, "in_tx" )
                        .add_input_binding( natus::graphics::binding_point::view_matrix, "u_view" )
                        .add_input_binding( natus::graphics::binding_point::projection_matrix, "u_proj" ) ;
                }

                _wid_async.second.configure( sc ) ;
            }

            {
                _rc->link_geometry( "quad" ) ;
                _rc->link_shader( "quad" ) ;
            }

            // add variable set 0
            {
                natus::graphics::variable_set_res_t vars = natus::graphics::variable_set_t() ;
                {
                    auto * var = vars->texture_variable( "u_tex" ) ;
                    var->set( "loaded_image" ) ;
                }

                _rc->add_variable_set( ::std::move( vars ) ) ;
            }

            _wid_async.second.configure( _rc ) ;

            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_update( void_t ) 
        { 
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) ) ;

            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_render( void_t ) 
        { 
            // per frame update of variables
            _rc->for_each( [&] ( size_t const i, natus::graphics::variable_set_res_t const& vs )
            {
                {
                    auto* var = vs->data_variable< natus::math::mat4f_t >( "u_view" ) ;
                    var->set( _camera_0.mat_view() ) ;
                }

                {
                    auto* var = vs->data_variable< natus::math::mat4f_t >( "u_proj" ) ;
                    var->set( _camera_0.mat_proj() ) ;
                }
            } ) ;

            {
                natus::graphics::backend_t::render_detail_t detail ;
                detail.start = 0 ;
                detail.render_states = _render_states ;
                _wid_async.second.render( _rc, detail ) ;
            }
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_shutdown( void_t ) 
        { return natus::application::result::ok ; }
    };
    natus_res_typedef( test_app ) ;
}

int main( int argc, char ** argv )
{
    return natus::application::global_t::create_application( 
        this_file::test_app_res_t( this_file::test_app_t() ) )->exec() ;
}
