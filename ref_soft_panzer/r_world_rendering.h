#ifndef R_WORLD_RENDERING_H
#define R_WORLD_RENDERING_H

#include "rendering.h"
#include "panzer_rast/psr.h"
#include "panzer_rast/rendering_commands.h"
#include "panzer_rast/rasterization.h"
#include "panzer_rast/vertex_processing.h"
#include "panzer_rast/texture.h"
#include "r_light.h"

#include "panzer_rast/math_lib/matrix.h"
#include "panzer_rast/rendering_state.h"


#define Q2_UNITS_PER_METER 64

typedef  void(*triangle_draw_func_t)(char*);


void BuildSurfaceLists(m_Mat4* mat, vec3_t cam_pos );
void DrawWorldTextureChains();
void DrawWorldAlphaSurfaces();

triangle_draw_func_t GetWorldDrawFunc( int texture_mode, bool is_blending );

triangle_draw_func_t GetWorldNearDrawFunc( bool is_alpha );
triangle_draw_func_t GetWorldFarDrawFunc( bool is_alpha );
triangle_draw_func_t GetWorldNearDrawFuncNoLightmaps( bool is_alpha);
triangle_draw_func_t GetWorldFarDrawFuncNoLightmaps( bool is_alpha);

int DrawWorldSurface( msurface_t* surf, triangle_draw_func_t near_draw_func, triangle_draw_func_t far_draw_func, mtexinfo_t* texinfo, fixed16_t texcoord_shift );
void SetSurfaceMatrix( m_Mat4* mat );
void SetFov( float fov );
void SetWorldFrame( int frame );
void InitFrustrumClipPlanes( m_Mat4* normal_mat, vec3_t transformed_cam_pos );

#endif//R_WORLD_RENDERING_H