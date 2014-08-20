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

typedef  void(*triangle_draw_func_t)(char*);


void DrawWorldNodes(m_Mat4* mat, vec3_t cam_pos );

triangle_draw_func_t GetWorldDrawFunc( int texture_mode, bool is_blending );

int DrawWorldSurface( msurface_t* surf, triangle_draw_func_t near_draw_func, triangle_draw_func_t far_draw_func, mtexinfo_t* texinfo, fixed16_t texcoord_shift );
void SetSurfaceMatrix( m_Mat4* mat );
void SetFov( float fov );

#endif//R_WORLD_RENDERING_H