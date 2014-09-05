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

typedef struct surfaces_chain_s
{
	msurface_t* first_surface;
	msurface_t* last_surface;
	int surf_count;
}surfaces_chain_t;

typedef struct projection_rect_s
{
	float x_add;
	float y_add;
	float x_mul;
	float y_mul;

	//for particles clipping. values are integer
	float x_min;
	float x_max;
	float y_min;
	float y_max;
} projection_rect_t;

extern projection_rect_t vertex_projection;


void BuildSurfaceLists(m_Mat4* mat, vec3_t cam_pos );
void DrawWorldTextureChains();
void DrawWorldSurfaces();
void DrawWorldAlphaSurfaces();

triangle_draw_func_t GetWorldDrawFunc( int texture_mode, bool is_blending );

triangle_draw_func_t GetWorldNearDrawFunc( bool is_alpha );
triangle_draw_func_t GetWorldFarDrawFunc( bool is_alpha );
triangle_draw_func_t GetWorldNearDrawFuncNoLightmaps( bool is_alpha);
triangle_draw_func_t GetWorldFarDrawFuncNoLightmaps( bool is_alpha);

triangle_draw_func_t GetWorldCachedNearDrawFunc( bool is_alpha );
triangle_draw_func_t GetWorldCachedFarDrawFunc( bool is_alpha );

int DrawWorldSurface		( msurface_t* surf, triangle_draw_func_t near_draw_func, triangle_draw_func_t far_draw_func, mtexinfo_t* texinfo, fixed16_t texcoord_shift );
int DrawWorldCachedSurface	( msurface_t* surf, triangle_draw_func_t near_draw_func, triangle_draw_func_t far_draw_func, mtexinfo_t* texinfo, fixed16_t texcoord_shift );
void SetSurfaceMatrix( m_Mat4* mat, m_Mat4* normal_mat );
void SetFov( float fov );
void SetWorldFrame( int frame );
void InitFrustrumClipPlanes( m_Mat4* normal_mat, vec3_t transformed_cam_pos );
int  ClipFace( int vertex_coint );
mplane_t* SetAdditionalClipPlanes( int plane_count );

#endif//R_WORLD_RENDERING_H