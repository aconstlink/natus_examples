
#include "main.h"

#include <natus/application/global.h>
#include <natus/application/app.h>
#include <natus/gfx/camera/pinhole_camera.h>
#include <natus/graphics/variable/variable_set.hpp>

#include <natus/format/global.h>
#include <natus/format/future_items.hpp>
#include <natus/io/database.h>

#include <natus/geometry/mesh/tri_mesh.h>
#include <natus/geometry/mesh/flat_tri_mesh.h>
#include <natus/geometry/3d/cube.h>

#include <natus/math/utility/angle.hpp>
#include <natus/math/utility/3d/transformation.hpp>
#include <natus/math/vector/vector3.hpp>
#include <natus/math/vector/vector4.hpp>
#include <natus/math/matrix/matrix4.hpp>

#include <natus/profiling/macros.h>

#include <thread>

namespace this_file
{
    using namespace natus::core::types ;

    class test_app : public natus::application::app
    {
        natus_this_typedefs( test_app ) ;

    private:

        natus::graphics::async_views_t _graphics ;
        
        natus::graphics::array_object_res_t _gpu_data = natus::graphics::array_object_t() ;

        natus::graphics::state_object_res_t _root_render_states ;

        natus::graphics::render_object_res_t _ro ;
        natus::graphics::geometry_object_res_t _go ;

        natus::graphics::variable_set_res_t _vs0 ;
        natus::graphics::variable_set_res_t _vs1 ;

        struct vertex { natus::math::vec3f_t pos ; } ;
        
        typedef std::chrono::high_resolution_clock __clock_t ;
        __clock_t::time_point _tp = __clock_t::now() ;

        natus::gfx::pinhole_camera_t _camera_0 ;

        int_t _max_textures = 3 ;
        int_t _used_texture = 0 ;

        natus::io::database_res_t _db ;

    public:

        test_app( void_t ) 
        {
            srand (time(NULL));

            natus::application::app::window_info_t wi ;
            #if 1
            app::window_async_t wid_async = this_t::create_window( "A Render Window", wi ) ;
            app::window_async_t wid_async2 = this_t::create_window( "A Render Window", wi,
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11 }) ;

            wid_async.window().position( 50, 50 ) ;
            wid_async.window().resize( 800, 800 ) ;
            wid_async2.window().position( 50 + 800, 50 ) ;
            wid_async2.window().resize( 800, 800 ) ;

            _graphics = natus::graphics::async_views_t( {wid_async.async(), wid_async2.async()} ) ;
            #else
            app::window_async_t wid_async = this_t::create_window( "A Render Window", wi, 
                { natus::graphics::backend_type::es3, natus::graphics::backend_type::d3d11 } ) ;
            _graphics = natus::graphics::async_views_t( {wid_async.async()} ) ;
            #endif

            _db = natus::io::database_t( natus::io::path_t( DATAPATH ), "./working", "data" ) ;
        }
        test_app( this_cref_t ) = delete ;
        test_app( this_rref_t rhv ) : app( ::std::move( rhv ) ) 
        {
            _graphics = std::move( rhv._graphics ) ;
            _camera_0 = std::move( rhv._camera_0 ) ;
            _db = std::move( rhv._db ) ;
        }
        virtual ~test_app( void_t ) 
        {}

        virtual natus::application::result on_event( window_id_t const, this_t::window_event_info_in_t wei )
        {
            _camera_0.perspective_fov( natus::math::angle<float_t>::degree_to_radian( 90.0f ),
                float_t(wei.w) / float_t(wei.h), 1.0f, 10000.0f ) ;

            return natus::application::result::ok ;
        }

    private:

        virtual natus::application::result on_init( void_t )
        { 
            {
                _camera_0.look_at( natus::math::vec3f_t( 2500.0f, 1000.0f, 1000.0f ),
                        natus::math::vec3f_t( 0.0f, 1.0f, 0.0f ), natus::math::vec3f_t( 0.0f, 0.0f, 0.0f )) ;
            }

            // root render states
            {
                natus::graphics::state_object_t so = natus::graphics::state_object_t(
                    "root_render_states" ) ;

                {
                    natus::graphics::render_state_sets_t rss ;

                    rss.depth_s.do_change = true ;
                    rss.depth_s.ss.do_activate = false ;
                    rss.depth_s.ss.do_depth_write = true ;

                    rss.polygon_s.do_change = true ;
                    rss.polygon_s.ss.do_activate = false ;
                    rss.polygon_s.ss.ff = natus::graphics::front_face::clock_wise ;
                    rss.polygon_s.ss.cm = natus::graphics::cull_mode::back ;
                    rss.polygon_s.ss.fm = natus::graphics::fill_mode::fill ;

                   
                    so.add_render_state_set( rss ) ;
                }

                _root_render_states = std::move( so ) ;
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( _root_render_states ) ;
                } ) ;
            }

            // geometry configuration
            {
                auto vb = natus::graphics::vertex_buffer_t()
                    .add_layout_element( natus::graphics::vertex_attribute::position, natus::graphics::type::tfloat, natus::graphics::type_struct::vec3 )
                    .resize( 4 ).update<vertex>( [=] ( vertex* array, size_t const ne )
                {
                    array[ 0 ].pos = natus::math::vec3f_t( -0.5f, -0.5f, 0.0f ) ;
                    array[ 1 ].pos = natus::math::vec3f_t( -0.5f, +0.5f, 0.0f ) ;
                    array[ 2 ].pos = natus::math::vec3f_t( +0.5f, +0.5f, 0.0f ) ;
                    array[ 3 ].pos = natus::math::vec3f_t( +0.5f, -0.5f, 0.0f ) ;
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

                natus::graphics::geometry_object_res_t geo = natus::graphics::geometry_object_t( "quad",
                    natus::graphics::primitive_type::triangles, std::move( vb ), std::move( ib ) ) ;

                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( geo ) ;
                } ) ;                
                _go = std::move( geo ) ;
            }

            // image configuration
            {
                natus::format::module_registry_res_t mod_reg = natus::format::global_t::registry() ;
                natus::format::future_item_t items[4] = 
                {
                    mod_reg->import_from( natus::io::location_t( "images.1_.png" ), _db ),
                    mod_reg->import_from( natus::io::location_t( "images.2_.png" ), _db ),
                    mod_reg->import_from( natus::io::location_t( "images.3_.png" ), _db ),
                    mod_reg->import_from( natus::io::location_t( "images.4.png" ), _db )
                } ;

                // taking all slices
                natus::graphics::image_t img ;

                // load each slice into the image
                for( size_t i=0; i<4; ++i )
                {
                    natus::format::image_item_res_t ii = items[i].get() ;
                    if( ii.is_valid() )
                    {
                        img.append( *ii->img ) ;
                    }
                }

                natus::graphics::image_object_res_t ires = natus::graphics::image_object_t( 
                    "image_array", std::move( img ) )
                    .set_type( natus::graphics::texture_type::texture_2d_array )
                    .set_wrap( natus::graphics::texture_wrap_mode::wrap_s, natus::graphics::texture_wrap_type::repeat )
                    .set_wrap( natus::graphics::texture_wrap_mode::wrap_t, natus::graphics::texture_wrap_type::repeat )
                    .set_filter( natus::graphics::texture_filter_mode::min_filter, natus::graphics::texture_filter_type::nearest )
                    .set_filter( natus::graphics::texture_filter_mode::mag_filter, natus::graphics::texture_filter_type::nearest );

                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( ires ) ;
                } ) ;
            }
            
            // shader configuration
            {
                natus::graphics::shader_object_t sc( "test_variable_array" ) ;

                // shaders : ogl 3.1
                {
                    natus::graphics::shader_set_t ss = natus::graphics::shader_set_t().

                        set_vertex_shader( natus::graphics::shader_t( R"(
                            #version 140
                            in vec3 in_pos ;
                            out vec2 var_tx ;

                            uniform int u_quad ; // in [0,1] left or right quad
                            
                            void main()
                            {
                                vec2 offset[2] = vec2[2]( vec2(-0.5, 0.0), vec2(0.5,0.0) ) ;
                                gl_Position = vec4( in_pos.xy * vec2(0.85) + offset[u_quad], 0.0, 1.0 ) ;
                                var_tx = sign( in_pos.xy ) * vec2( 0.5 ) + vec2( 0.5 ) ;

                            } )" ) ).

                        set_pixel_shader( natus::graphics::shader_t( R"(
                            #version 140
                            #extension GL_ARB_separate_shader_objects : enable
                            #extension GL_ARB_explicit_attrib_location : enable
                            
                            in vec2 var_tx ;

                            layout(location = 0 ) out vec4 out_color ;

                            uniform sampler2DArray u_tex ;
                            
                            uniform int u_quad ; // in [0,1] left or right quad
                            uniform int u_texture ; // in [0,3] choosing the sampler in u_tex

                            void main()
                            {    
                                vec2 uv = fract( var_tx * 2.0 ) ;
                                int quadrant = int( dot( floor(var_tx*2.0), vec2(1,2) ) ) ;
                                int idx = u_quad * u_texture + quadrant * ( 1 - u_quad ) ;
                                //out_color = vec4( floor(var_tx*2.0), 0.0, 1.0 ) ;
                                out_color = texture( u_tex, vec3( uv, float(idx) ) ) ;
                            } )" ) ) ;

                    sc.insert( natus::graphics::backend_type::gl3, std::move( ss ) ) ;
                }

                // shaders : es 3.0
                {
                    natus::graphics::shader_set_t ss = natus::graphics::shader_set_t().

                        set_vertex_shader( natus::graphics::shader_t( R"(
                            #version 300 es
                            in vec3 in_pos ;
                            in vec3 in_nrm ;
                            in vec2 in_tx ;
                            out vec3 var_nrm ;
                            out vec2 var_tx0 ;
                            out vec4 var_col ;
                            uniform mat4 u_proj ;
                            uniform mat4 u_view ;
                            uniform mat4 u_world ;
                            uniform sampler2D u_data ;

                            void main()
                            {
                                int idx = gl_VertexID / 24 ;
                                ivec2 wh = textureSize( u_data, 0 ) ;
                                vec4 pos_scl = texelFetch( u_data, 
                                     ivec2( ((idx*2) % wh.x), ((idx*2) / wh.x) ), 0 ) ;
                                var_col = texelFetch( u_data, 
                                     ivec2( (((idx*2)+1) % wh.x), (((idx*2)+1) / wh.x) ), 0 ) ;
                                var_tx0 = in_tx ;
                                vec4 pos = vec4(in_pos * vec3( pos_scl.w ),1.0 )  ;
                                pos = u_world * pos ;
                                pos += vec4(pos_scl.xyz*vec3(pos_scl.w*2.0f),0.0) ;
                                gl_Position = u_proj * u_view * pos ;
                                var_nrm = normalize( u_world * vec4( in_nrm, 0.0 ) ).xyz ;
                            } )" ) ).

                        set_pixel_shader( natus::graphics::shader_t( R"(
                            #version 300 es
                            precision mediump float ;
                            in vec2 var_tx0 ;
                            in vec3 var_nrm ;
                            in vec4 var_col ;
                            layout(location = 0 ) out vec4 out_color0 ;
                            layout(location = 1 ) out vec4 out_color1 ;
                            layout(location = 2 ) out vec4 out_color2 ;
                            uniform sampler2D u_tex ;
                            uniform vec4 u_color ;
                        
                            void main()
                            {    
                                out_color0 = u_color * texture( u_tex, var_tx0 ) * var_col ;
                                out_color1 = vec4( var_nrm, 1.0 ) ;
                                out_color2 = vec4( 
                                    vec3( dot( normalize( var_nrm ), normalize( vec3( 1.0, 1.0, 0.5) ) ) ), 
                                    1.0 ) ;
                            })" ) ) ;

                    sc.insert( natus::graphics::backend_type::es3, std::move( ss ) ) ;
                }

                // shaders : hlsl 11(5.0)
                {
                    natus::graphics::shader_set_t ss = natus::graphics::shader_set_t().

                        set_vertex_shader( natus::graphics::shader_t( R"(
                            cbuffer ConstantBuffer : register( b0 ) 
                            {
                                int u_quad ; // in [0,1] left or right quad
                            }

                            struct VS_INPUT
                            {
                                float4 in_pos : POSITION ; 
                            } ;

                            struct VS_OUTPUT
                            {
                                float4 pos : SV_POSITION ;
                                float2 tx : TEXCOORD0 ;
                            };

                            VS_OUTPUT VS( VS_INPUT input )
                            {
                                VS_OUTPUT output = (VS_OUTPUT)0 ;

                                float2 offset[2] = { float2(-0.5, 0.0), float2(0.5,0.0) };
                                float2 pos = input.in_pos.xy * float2(0.85, 0.85) + offset[u_quad] ;
                                output.pos = float4( pos, 0.0, 1.0 ) ;
                                output.tx = sign( input.in_pos.xy ) * float2(0.5,0.5) + float2(0.5,0.5);

                                return output;
                            } )" ) ).

                        set_pixel_shader( natus::graphics::shader_t( R"(
                            
                            Texture2DArray u_tex : register( t0 ) ;
                            SamplerState smp_u_tex : register( s0 ) ;

                            cbuffer ConstantBuffer : register( b0 ) 
                            {
                                int u_quad ; // in [0,1] left or right quad
                                int u_texture ; // in [0,3] choosing the sampler in u_tex
                            }

                            struct VS_OUTPUT
                            {
                                float4 Pos : SV_POSITION;
                                float2 tx : TEXCOORD0;
                            } ;

                            float4 PS( VS_OUTPUT input ) : SV_Target0
                            {
                                float2 uv = frac( input.tx * 2.0 ) ;
                                int quadrant = int( dot( floor(input.tx*float2(2,2)), float2(1,2) ) ) ;
                                //return float4( uv, 0.0, 1.0 ) ; 
                                int idx = u_quad * u_texture + quadrant *(1-u_quad) ;
                                return u_tex.Sample( smp_u_tex, float3( uv, float(idx)) ) ;
                                //return float4( uv, 0.0, 1.0) ;
                            } )" ) ) ;

                    sc.insert( natus::graphics::backend_type::d3d11, std::move( ss ) ) ;
                }

                // configure more details
                {
                    sc
                        .add_vertex_input_binding( natus::graphics::vertex_attribute::position, "in_pos" )
                        .add_input_binding( natus::graphics::binding_point::view_matrix, "u_view" )
                        .add_input_binding( natus::graphics::binding_point::projection_matrix, "u_proj" ) ;
                }

                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( sc ) ;
                } ) ;
            }

            // the quad render object
            {
                natus::graphics::render_object_t rc = natus::graphics::render_object_t( "quad" ) ;

                {
                    rc.link_geometry( "quad" ) ;
                    rc.link_shader( "test_variable_array" ) ;
                }

                // add variable set 0
                {
                    natus::graphics::variable_set_res_t vars = natus::graphics::variable_set_t() ;
                    
                    {
                        auto* var = vars->texture_variable( "u_tex" ) ;
                        var->set( "image_array" ) ;
                    }

                    {
                        auto * var = vars->data_variable<int32_t>("u_quad" ) ;
                        var->set( 0 ) ;
                    }

                    {
                        auto * var = vars->data_variable<int32_t>("u_texture" ) ;
                        var->set( 0 ) ;
                    }

                    _vs0 = vars ;
                    rc.add_variable_set( std::move( vars ) ) ;
                }

                {
                    natus::graphics::variable_set_res_t vars = natus::graphics::variable_set_t() ;
                    
                    {
                        auto* var = vars->texture_variable( "u_tex" ) ;
                        var->set( "image_array" ) ;
                    }

                    {
                        auto * var = vars->data_variable<int32_t>("u_quad" ) ;
                        var->set( 1 ) ;
                    }

                    {
                        auto * var = vars->data_variable<int32_t>("u_texture" ) ;
                        var->set( 0 ) ;
                    }

                    _vs1 = vars ;
                    rc.add_variable_set( std::move( vars ) ) ;
                }
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( rc ) ;
                } ) ;
                _ro = std::move( rc ) ;
            }
            
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_update( natus::application::app_t::update_data_in_t ) 
        { 
            NATUS_PROFILING_COUNTER_HERE( "Update Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_graphics( natus::application::app_t::render_data_in_t ) 
        { 
            _ro->for_each( [&] ( size_t const i, natus::graphics::variable_set_res_t const& vs )
            {
                {
                    auto* var = vs->data_variable< int32_t >( "u_texture" ) ;
                    var->set( _used_texture ) ;
                }
            } ) ;

            // render the root render state sets render object
            // this will set the root render states
            _graphics.for_each( [&]( natus::graphics::async_view_t a )
            {
                a.use( _root_render_states ) ;
            } ) ;

            _graphics.for_each( [&]( natus::graphics::async_view_t a )
            {
                // left quad
                {
                    natus::graphics::backend_t::render_detail_t detail ;
                    detail.varset = 0 ;
                    a.render( _ro, detail ) ;
                }
                // right quad
                {
                    natus::graphics::backend_t::render_detail_t detail ;
                    detail.varset = 1 ;
                    a.render( _ro, detail ) ;
                }
            } ) ;

            // render the root render state sets render object
            // this will set the root render states
            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.use( natus::graphics::state_object_t(), 10 ) ;
                } ) ;
            }

            NATUS_PROFILING_COUNTER_HERE( "Render Clock" ) ;

            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_tool( natus::tool::imgui_view_t imgui )
        {
            //if( !_do_tool ) return natus::application::result::no_imgui ;

            ImGui::Begin( "Config" ) ;

            if( ImGui::SliderInt( "Use Texture", &_used_texture, 0, _max_textures ) )
            {
            } 

            ImGui::End() ;
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






