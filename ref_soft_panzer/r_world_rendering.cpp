#include "r_world_rendering.h"
#include "panzer_rast/fast_math.h"
#include "r_surf.h"

extern "C" mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model);
extern "C" void R_MarkLeaves (void);

extern Texture* R_FindTexture( char* name );
extern Texture* R_FindTexture(image_t* image);
extern int R_GetImageIndex(image_t* image);

void DrawCharString( int x, int y, const char* str );



/*
--------surfaces chains-------------
*/
#define	MAX_RIMAGES	1024//if changed, must changed in r_image.cpp too
typedef struct texture_surfaces_chain_s
{
	msurface_t* first_surface[MAX_RIMAGES];
	msurface_t* last_surface[MAX_RIMAGES];
}texture_surfaces_chain_t;
texture_surfaces_chain_t texture_surfaces_chain;

surfaces_chain_t alpha_surfaces_chain;
surfaces_chain_t sky_surfaces_chain;
surfaces_chain_t world_surfaces_chain;

projection_rect_t vertex_projection;


m_Mat4 view_matrix;
m_Mat4 normal_matrix;//matrix to convert model normals to view space
float cam_pos[3];
float fov_y_rad;
float texels_in_pixel= 64.0f * 2.0f / 768.0f;// number of texels in screen, on surface on distance 1m.
int current_frame;


void SetWorldFrame( int frame )
{
	current_frame= frame;
}


triangle_draw_func_t world_triangles_draw_funcs_table[]=
{
	NULL,//DrawWorldTriangleTextureNearestLightmapLiearRGBS,
	DrawWorldTriangleTextureNearestLightmapColoredLinear,
	DrawWorldTriangleTextureNearestLightmapLinear,
	NULL,//DrawWorldTriangleTextureLinearLightmapLiearRGBS,
	DrawWorldTriangleTextureLinearLightmapColoredLinear,
	DrawWorldTriangleTextureLinearLightmapLinear,
	NULL,//DrawWorldTriangleTextureFakeFilterLightmapLiearRGBS,
	DrawWorldTriangleTextureFakeFilterLightmapColoredLinear,
	DrawWorldTriangleTextureFakeFilterLightmapLinear,

	NULL,//DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBS,
	DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinear,
	DrawWorldTriangleTextureNearestPalettizedLightmapLinear,
	NULL,//DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBS,
	DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinear,
	DrawWorldTriangleTextureLinearPalettizedLightmapLinear,
	NULL,//DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBS,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinear,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinear,
};
triangle_draw_func_t world_triangles_draw_funcs_blend_table[]=
{
	NULL,//DrawWorldTriangleTextureNearestLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureNearestLightmapColoredLinearBlend,
	DrawWorldTriangleTextureNearestLightmapLinearBlend,
	NULL,//DrawWorldTriangleTextureLinearLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureLinearLightmapColoredLinearBlend,
	DrawWorldTriangleTextureLinearLightmapLinearBlend,
	NULL,//DrawWorldTriangleTextureFakeFilterLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureFakeFilterLightmapColoredLinearBlend,
	DrawWorldTriangleTextureFakeFilterLightmapLinearBlend,

	NULL,//DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinearBlend,
	DrawWorldTriangleTextureNearestPalettizedLightmapLinearBlend,
	NULL,//DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinearBlend,
	DrawWorldTriangleTextureLinearPalettizedLightmapLinearBlend,
	NULL,//DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinearBlend,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinearBlend,
};

triangle_draw_func_t world_triangles_turbulence_draw_funcs_table[]=
{
	DrawWorldTriangleTextureNearestTurbulence,
	DrawWorldTriangleTextureLinearTurbulence,
	DrawWorldTriangleTextureFakeFilterTurbulence,
	DrawWorldTriangleTextureNearestTurbulenceBlend,
	DrawWorldTriangleTextureLinearTurbulenceBlend,
	DrawWorldTriangleTextureFakeFilterTurbulenceBlend,
	//PALETTIZED
	DrawWorldTriangleTextureNearestPalettizedTurbulence,
	DrawWorldTriangleTextureLinearPalettizedTurbulence,
	DrawWorldTriangleTextureFakeFilterPalettizedTurbulence,
	DrawWorldTriangleTextureNearestPalettizedTurbulenceBlend,
	DrawWorldTriangleTextureLinearPalettizedTurbulenceBlend,
	DrawWorldTriangleTextureFakeFilterPalettizedTurbulenceBlend
};


triangle_draw_func_t worlf_triangles_cache_draw_funcs_table[]=
{
	DrawWorldTriangleCachedTextureNearest,
	DrawWorldTriangleCachedTextureLinear,
	DrawWorldTriangleCachedTextureFakeFilter,
	DrawWorldTriangleCachedTextureNearestBlend,
	DrawWorldTriangleCachedTextureLinearBlend,
	DrawWorldTriangleCachedTextureFakeFilterBlend
};
/*
triangle_draw_func_t world_triangles_no_lightmap_draw_funcs_table[]=
{
	DrawWorldTriangleTextureNearest,
	DrawWorldTriangleTextureLinear,
	DrawWorldTriangleTextureFakeFilter,
	DrawWorldTriangleTextureNearestBlend,
	DrawWorldTriangleTextureLinearBlend,
	DrawWorldTriangleTextureFakeFilterBlend,
	//PALETTIZED
	DrawWorldTriangleTextureNearestPalettized,
	DrawWorldTriangleTextureLinearPalettized,
	DrawWorldTriangleTextureFakeFilterPalettized,
	DrawWorldTriangleTextureNearestPalettizedBlend,
	DrawWorldTriangleTextureLinearPalettizedBlend,
	DrawWorldTriangleTextureFakeFilterPalettizedBlend
};*/

triangle_draw_func_t GetWorldDrawFunc( int texture_mode, bool is_blending )
{
	int x, y;
	if( texture_mode == TEXTURE_LINEAR )
		x= 1;
	else if( texture_mode == TEXTURE_FAKE_FILTER )
		x= 2;
	else
		x= 0;

	/*if( strcmp( r_lightmap_mode->string, "lightmap_linear_rgbs" ) == 0 )
		y= 0;
	else */if( strcmp( r_lightmap_mode->string, "lightmap_linear_colored" ) == 0 )
		y= 1;
	else
		y= 2;

	if( r_palettized_textures->value )
		y+= 9;
	if( is_blending )
		return world_triangles_draw_funcs_blend_table[y+x*3];
	else
		return world_triangles_draw_funcs_table[y+x*3];
}

triangle_draw_func_t GetWorldNearDrawFunc( bool is_alpha )
{
	int tex_mode;
	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
		tex_mode= TEXTURE_LINEAR;
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
		tex_mode= TEXTURE_FAKE_FILTER;
	else
		tex_mode= TEXTURE_NEAREST;
	return GetWorldDrawFunc( tex_mode, is_alpha );
}

triangle_draw_func_t GetWorldFarDrawFunc( bool is_alpha )
{
	return GetWorldDrawFunc( TEXTURE_NEAREST, is_alpha );
}


triangle_draw_func_t GetWorldNearDrawFuncNoLightmaps( bool is_alpha )
{
	int x, y;
	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
		x= 1;
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
		x= 2;
	else
		x= 0;
	if(is_alpha)
		y= 1;
	else y= 0;

	if( r_palettized_textures->value )
		x+= 6;
	//return world_triangles_no_lightmap_draw_funcs_table[ x + y*3 ];
	return world_triangles_turbulence_draw_funcs_table[ x + y*3 ];
}

triangle_draw_func_t GetWorldFarDrawFuncNoLightmaps( bool is_alpha )
{
	/*if(is_alpha)
	{
		if( r_palettized_textures->value )
			return DrawWorldTriangleTextureNearestPalettizedBlend;
		else
			return DrawWorldTriangleTextureNearestBlend;
	}
	else
	{
		if( r_palettized_textures->value )
			return DrawWorldTriangleTextureNearestPalettized;
		else
			return DrawWorldTriangleTextureNearest;
	}*/

	if(is_alpha)
	{
		if( r_palettized_textures->value )
			return DrawWorldTriangleTextureNearestPalettizedTurbulenceBlend;
		else
			return DrawWorldTriangleTextureNearestTurbulenceBlend;
	}
	else
	{
		if( r_palettized_textures->value )
			return DrawWorldTriangleTextureNearestPalettizedTurbulence;
		else
			return DrawWorldTriangleTextureNearestTurbulence;
	}
}


triangle_draw_func_t GetWorldCachedNearDrawFunc( bool is_blend )
{
	int x= 0;
	if( is_blend )
		x+= 3;

	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
		x+= 1;
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
		x+= 2;
	else
		x+= 0;

	return worlf_triangles_cache_draw_funcs_table[x];
}

triangle_draw_func_t GetWorldCachedFarDrawFunc( bool is_blend )
{
	int x= 0;
	if( is_blend )
		x+= 3;
	return worlf_triangles_cache_draw_funcs_table[x];
}

int GetTexCoordNearShift()
{
	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
		return -32768;
	else if(  strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
		return -32768+8192;
	else
		return 0;
}

void InitTextureSurfacesChain()
{
	memset( &texture_surfaces_chain, 0, sizeof(texture_surfaces_chain_t) );
	memset( &alpha_surfaces_chain, 0, sizeof(surfaces_chain_t) );
	memset( &sky_surfaces_chain, 0, sizeof(surfaces_chain_t) );
	memset( &world_surfaces_chain, 0, sizeof(surfaces_chain_t) );
	//sky_surfaces_chain.first_surface= sky_surfaces_chain.last_surface= NULL;
	//sky_surfaces_chain.surf_count= 0;
}


void SetSurfaceMatrix( m_Mat4* mat, m_Mat4* normal_mat )
{
	view_matrix= *mat;
	if( normal_mat != NULL )
		normal_matrix= *normal_mat;
}
void SetFov( float fov )
{
	fov_y_rad= fov;
	texels_in_pixel= tanf(fov*0.5f) * float(Q2_UNITS_PER_METER) * 2.0f / float(vid.height);
}


const int max_poly_vertices= 32;
mvertex_t* surface_vertices[max_poly_vertices];
m_Vec3 surface_final_vertices[max_poly_vertices];
bool surface_plane_pos[max_poly_vertices];
/*
0 - near plane
1, 2 - left, right
3, 4 - bottom, top
*/
mplane_t clip_planes[5+max_poly_vertices+2];
mvertex_t tmp_vertices_stack[max_poly_vertices + 16];
int clip_plane_count;
int tmp_vertices_stack_pos;


int GetSurfaceMipLevel( msurface_t* surf, int vert_count )
{
	float dst= 0;
	int front_vertex_count= 0;
	int neares_vertex_id= 0;
	for( int i= 0; i< vert_count; i++ )
	{
		if( surface_final_vertices[i].z > PSR_MIN_ZMIN_FLOAT )
		{
			dst+= surface_final_vertices[i].z;
			front_vertex_count++;
			neares_vertex_id= i;
		}
	}
	dst/= float(front_vertex_count);

	//calculate angle between vector from view pos to nearest vertex and surface normal
	float vec_to_cam[3];
	VectorSubtract( cam_pos, surface_vertices[neares_vertex_id]->position, vec_to_cam );
	VectorNormalize( vec_to_cam );
	float cos_dot= fabsf( DotProduct( vec_to_cam, surf->plane->normal ) );
	if( cos_dot < 0.4f ) cos_dot= 2.5f;
	else cos_dot= 1.0f / cos_dot;

	const float mip_shift= 1.3f;

	float* tex_basis_vec= surf->texinfo->vecs[0];
	float pixels_per_texel= cos_dot *  dst * texels_in_pixel * sqrtf( DotProduct(tex_basis_vec,tex_basis_vec) );
	return FastIntLog2Clamp0( int(pixels_per_texel * mip_shift) );
}



int ClipFaceByPlane( int vertex_count, mplane_t* plane )//returns new vertex count
{
	float* normal= plane->normal;

	int discarded_vertex_count= 0;
	for( int i= 0; i< vertex_count; i++ )
	{
		if( DotProduct(normal, surface_vertices[i]->position ) > plane->dist )
		{
			surface_plane_pos[i]= true;
		}
		else
		{
			discarded_vertex_count++;
			surface_plane_pos[i]= false;// false means discarded
		}
	}

	if( discarded_vertex_count == vertex_count )
		return 0;
	else if( discarded_vertex_count == 0 )
		return vertex_count;

	int splitted_edge[2];
	//0 - index of discarded vertex
	//1 - index of passed vertex
	for( int i= 0; i< vertex_count; i++ )
	{
		int next_vertex_index= i+1; if( next_vertex_index == vertex_count ) next_vertex_index= 0;
		if( !surface_plane_pos[i] )//if back vertex
		{
			if( surface_plane_pos[next_vertex_index] )//if front vertex
				splitted_edge[0]= i;
		}
		else//if fron vertex
		{
			if(!surface_plane_pos[next_vertex_index])//if back vertex
				splitted_edge[1]= i;
		}
	}

	mvertex_t* new_v[2]={  tmp_vertices_stack + tmp_vertices_stack_pos, tmp_vertices_stack + tmp_vertices_stack_pos + 1 };
	tmp_vertices_stack_pos+= 2;

	for( int i= 0; i< 2; i++ )
	{
		int v0_ind= splitted_edge[i];
		int v1_ind= splitted_edge[i]+1; if( v1_ind == vertex_count ) v1_ind= 0;
		float* v0= surface_vertices[v0_ind]->position, *v1= surface_vertices[v1_ind]->position;

		float k0= fabsf( DotProduct(v0,normal) - plane->dist );
		float k1= fabsf( DotProduct(v1,normal) - plane->dist );
		float inv_dst_sum= 1.0f / ( k0 + k1 );
		k0*= inv_dst_sum;
		k1*= inv_dst_sum;

		float* out_pos= new_v[i]->position;
		out_pos[0]= k1 * v0[0] + k0 * v1[0];
		out_pos[1]= k1 * v0[1] + k0 * v1[1];
		out_pos[2]= k1 * v0[2] + k0 * v1[2];
	}

	if( discarded_vertex_count == 2 )
	{
		surface_vertices[ splitted_edge[0] ]= new_v[0];
		int new_v_ind= splitted_edge[1]+1; if( new_v_ind == vertex_count ) new_v_ind= 0;
		surface_vertices[new_v_ind]= new_v[1];
		return vertex_count;
	}
	else if( discarded_vertex_count > 2 )
	{
		mvertex_t* tmp_vertices[max_poly_vertices];
		for( int i= 0; i< vertex_count; i++ )
			tmp_vertices[i]= surface_vertices[i];

		surface_vertices[0]= new_v[1];
		surface_vertices[1]= new_v[0];
		int new_vertex_count= vertex_count - discarded_vertex_count + 2;
		for( int i= 2, j= splitted_edge[0]+1; i < new_vertex_count; i++, j++ )
		{
			surface_vertices[i]= tmp_vertices[j%vertex_count];
		}
		return new_vertex_count;
	}
	else//discard one vertex
	{
		int discarded_v_ind= splitted_edge[0];
		for( int i= vertex_count; i> discarded_v_ind; i-- )
			surface_vertices[i]= surface_vertices[i-1];

		surface_vertices[discarded_v_ind  ]= new_v[1];
		surface_vertices[discarded_v_ind+1]= new_v[0];
		return vertex_count + 1;
	}
}

int ClipFace( int vertex_count )//returns new vertex count
{
	tmp_vertices_stack_pos= 0;
	for( int i= 0; i< clip_plane_count; i++ )
	{
		vertex_count= ClipFaceByPlane( vertex_count, clip_planes + i );
		if( vertex_count == 0 )
			return 0;
	}

	return vertex_count;
}


/*void SetClipPlanes( mplane_t* planes, int count )
{
	for( int i= 0; i< count && i<8; i++ )
		view_planes[i]= planes[i];
}*/

void InitFrustrumClipPlanes( m_Mat4* normal_mat, vec3_t transformed_cam_pos )
{
	//near clip plane
	const float to_rad = 3.1415926535f / 180.0f;
	float a;
	clip_planes[0].normal[0]= 0.0f; clip_planes[0].normal[1]= -1.0f; clip_planes[0].normal[2] = 0.0f;
	*((m_Mat4*)clip_planes[0].normal)= *((m_Mat4*)clip_planes[0].normal) * *normal_mat;

	vec3_t moved_cam_pos;
	VectorCopy( transformed_cam_pos, moved_cam_pos );
	float z_near= 1.4f * PSR_MIN_ZMIN_FLOAT * float(Q2_UNITS_PER_METER);
	moved_cam_pos[0]+= clip_planes[0].normal[0] * z_near;
	moved_cam_pos[1]+= clip_planes[0].normal[1] * z_near;
	moved_cam_pos[2]+= clip_planes[0].normal[2] * z_near;
	clip_planes[0].dist= DotProduct(moved_cam_pos, clip_planes[0].normal );

	const float angle_scaler= 1.01f;
	//top plane
	a= r_newrefdef.fov_y*0.5f*to_rad*angle_scaler;
	m_Vec3 tmp_normal( 0.0f, -sinf(a), -cosf(a) );
	*((m_Vec3*)clip_planes[3].normal)= tmp_normal * *normal_mat;
	clip_planes[3].dist= DotProduct(transformed_cam_pos, clip_planes[3].normal );
	//bottom plane
	a= r_newrefdef.fov_y*0.5f*to_rad*angle_scaler;
	tmp_normal.x= 0.0f; tmp_normal.y= -sinf(a); tmp_normal.z= cosf(a);
	*((m_Vec3*)clip_planes[4].normal)= tmp_normal * *normal_mat;
	clip_planes[4].dist= DotProduct(transformed_cam_pos, clip_planes[4].normal );
	//left plane
	a= r_newrefdef.fov_x*0.5f*to_rad*angle_scaler;
	tmp_normal.x= -cosf(a); tmp_normal.y= -sinf(a); tmp_normal.z= 0.0f;
	*((m_Vec3*)clip_planes[1].normal)= tmp_normal * *normal_mat;
	clip_planes[1].dist= DotProduct(transformed_cam_pos, clip_planes[1].normal );
	//right plane
	a= r_newrefdef.fov_x*0.5f*to_rad*angle_scaler;
	tmp_normal.x= cosf(a); tmp_normal.y= -sinf(a); tmp_normal.z= 0.0f;
	*((m_Vec3*)clip_planes[2].normal)= tmp_normal * *normal_mat;
	clip_planes[2].dist= DotProduct(transformed_cam_pos, clip_planes[2].normal );

	clip_plane_count= 5;
}

mplane_t* SetAdditionalClipPlanes( int plane_count )
{
	clip_plane_count= 5 + plane_count;
	return clip_planes + 5;
}


//returns number of triangles
int DrawWorldSurface( msurface_t* surf, triangle_draw_func_t near_draw_func, triangle_draw_func_t far_draw_func, mtexinfo_t* texinfo, fixed16_t tex_coord_shift )
{
	int triangle_count= 0;

	fixed16_t tc_u[max_poly_vertices];
	fixed16_t tc_v[max_poly_vertices];
	fixed16_t lightmap_tc_u[max_poly_vertices];
	fixed16_t lightmap_tc_v[max_poly_vertices];
	if( surf->numedges > max_poly_vertices )
		surf->numedges= max_poly_vertices;
	//get vertices
	for(int e= 0; e< surf->numedges; e++ )
	{
		int ind= r_worldmodel->surfedges[e+surf->firstedge];
		if( ind > 0 )
			surface_vertices[e]= r_worldmodel->vertexes + r_worldmodel->edges[ind].v[0];
		else
			surface_vertices[e]= r_worldmodel->vertexes + r_worldmodel->edges[-ind].v[1];
	}
	int final_vertex_count= ClipFace(surf->numedges);
	if( final_vertex_count == 0 )
		goto put_draw_command;
	for(int e= 0; e< final_vertex_count; e++ )
	{
		tc_u[e]= fixed16_t( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3])*65536.0f );
		tc_v[e]= fixed16_t( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3])*65536.0f );
		lightmap_tc_u[e]= (tc_u[e] - (surf->texturemins[0]<<16) )>>4;
		lightmap_tc_v[e]= (tc_v[e] - (surf->texturemins[1]<<16) )>>4;

		//((mvertex_t*)surface_final_vertices)[e]= *(surface_vertices[e]);
		surface_final_vertices[e]= *((m_Vec3*)surface_vertices[e]->position) * view_matrix;
		float inv_z= 1.0f / surface_final_vertices[e].z;
		surface_final_vertices[e].x= (surface_final_vertices[e].x * inv_z + vertex_projection.x_add ) * vertex_projection.x_mul;
		surface_final_vertices[e].y= (surface_final_vertices[e].y * inv_z + vertex_projection.y_add ) * vertex_projection.y_mul;
	}
	//texture coord morfing
	if( (surf->flags&SURF_FLOW) != 0 )
	{
		fixed16_t tc_delta= ( int( -0.5f * r_newrefdef.time * float(texinfo->image->width) ) & (texinfo->image->width*4-1) )<<16;
		for(int e= 0; e< final_vertex_count; e++ )
			tc_u[e]+= tc_delta;
	}

	//select texture and mip level
	int mip_level;
	mip_level= GetSurfaceMipLevel( surf, final_vertex_count );
	Texture* tex;
	tex= R_FindTexture( texinfo->image );
	if( mip_level >= MIPLEVELS ) mip_level= MIPLEVELS-1;
	command_buffer.current_pos +=
	ComIn_SetTextureLod( command_buffer.current_pos + (char*)command_buffer.buffer, tex, mip_level );

put_draw_command:
	bool no_lightmaps= (surf->flags&SURF_DRAWTURB)!= 0 || (surf->texinfo->flags&SURF_WARP)!= 0;

	char* buff= (char*) command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;
	((int*)buff)[0]= COMMAND_DRAW_TRIANGLE;
	buff+=sizeof(int);
	DrawTriangleCall* call= (DrawTriangleCall*)buff;
	call->DrawFromBufferFunc= mip_level == 0 ? near_draw_func : far_draw_func;
	call->triangle_count= 0;
	if(no_lightmaps)
		call->vertex_size= sizeof(int)*3 + sizeof(int)*2;
	else
		call->vertex_size= sizeof(int)*3 + sizeof(int)*2 + sizeof(int)*2;
	buff+= sizeof(DrawTriangleCall);
	if( final_vertex_count == 0 )
		return 0;
	if( mip_level != 0 ) tex_coord_shift= 0;//no need texture shift for nearest textures
	for( int v= 0; v< final_vertex_count; v++ )
	{
		tc_u[v]= (tc_u[v]>>mip_level) + tex_coord_shift;
		tc_v[v]= (tc_v[v]>>mip_level) + tex_coord_shift;
		surface_final_vertices[v].z*= 65536.0f;//prepare to convertion to fixd16_t format
	}
	const float z_min_scaled= PSR_MIN_ZMIN_FLOAT * 65536.0f;
	for( int t= 0; t< final_vertex_count-2; t++ )
	{
		if( surface_final_vertices[0].z <= z_min_scaled || surface_final_vertices[t+1].z <= z_min_scaled || surface_final_vertices[t+2].z <= z_min_scaled )
			continue;
		using namespace VertexProcessing;
		triangle_in_tex_coord[0]= tc_u[0];;
		triangle_in_tex_coord[1]= tc_v[0];
		triangle_in_tex_coord[2]= tc_u[t+1];
		triangle_in_tex_coord[3]= tc_v[t+1];
		triangle_in_tex_coord[4]= tc_u[t+2];
		triangle_in_tex_coord[5]= tc_v[t+2];
		triangle_in_lightmap_tex_coord[0]= lightmap_tc_u[0];
		triangle_in_lightmap_tex_coord[1]= lightmap_tc_v[0];
		triangle_in_lightmap_tex_coord[2]= lightmap_tc_u[t+1];
		triangle_in_lightmap_tex_coord[3]= lightmap_tc_v[t+1];
		triangle_in_lightmap_tex_coord[4]= lightmap_tc_u[t+2];
		triangle_in_lightmap_tex_coord[5]= lightmap_tc_v[t+2];

		triangle_in_vertex_xy[0]= fixed16_t(surface_final_vertices[0].x);
		triangle_in_vertex_xy[1]= fixed16_t(surface_final_vertices[0].y);
		triangle_in_vertex_xy[2]= fixed16_t(surface_final_vertices[t+1].x);
		triangle_in_vertex_xy[3]= fixed16_t(surface_final_vertices[t+1].y);
		triangle_in_vertex_xy[4]= fixed16_t(surface_final_vertices[t+2].x);
		triangle_in_vertex_xy[5]= fixed16_t(surface_final_vertices[t+2].y);
		triangle_in_vertex_z[0]= fixed16_t(surface_final_vertices[0].z);
		triangle_in_vertex_z[1]= fixed16_t(surface_final_vertices[t+1].z);
		triangle_in_vertex_z[2]= fixed16_t(surface_final_vertices[t+2].z);

		int draw_to_buffer_result;
		if(no_lightmaps)
			draw_to_buffer_result= DrawWorldTriangleNoLightmapToBuffer(buff);
		else
			draw_to_buffer_result= DrawWorldTriangleToBuffer(buff);
		if( draw_to_buffer_result != 0 )
		{
			buff+= 4 * call->vertex_size;
			call->triangle_count++;
			triangle_count++;
		}
	}//for t

	return triangle_count;
}




//returns number of triangles
int DrawWorldCachedSurface( msurface_t* surf, triangle_draw_func_t near_draw_func, triangle_draw_func_t far_draw_func, mtexinfo_t* texinfo, fixed16_t tex_coord_shift )
{
	int triangle_count= 0;

	fixed16_t tc_u[max_poly_vertices];
	fixed16_t tc_v[max_poly_vertices];
	
	if( surf->numedges > max_poly_vertices )
		surf->numedges= max_poly_vertices;
	//get vertices
	for(int e= 0; e< surf->numedges; e++ )
	{
		int ind= r_worldmodel->surfedges[e+surf->firstedge];
		if( ind > 0 )
			surface_vertices[e]= r_worldmodel->vertexes + r_worldmodel->edges[ind].v[0];
		else
			surface_vertices[e]= r_worldmodel->vertexes + r_worldmodel->edges[-ind].v[1];
	}
	int final_vertex_count= ClipFace(surf->numedges);
	if( final_vertex_count == 0 )
		goto put_draw_command;

	for(int e= 0; e< final_vertex_count; e++ )
	{
		tc_u[e]= fixed16_t( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[0] )
			+ surf->texinfo->vecs[0][3])*65536.0f ) - (surf->texturemins[0]<<16);
		tc_v[e]= fixed16_t( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[1] )
			+ surf->texinfo->vecs[1][3])*65536.0f ) - (surf->texturemins[1]<<16);

		surface_final_vertices[e]= *((m_Vec3*)surface_vertices[e]->position) * view_matrix;
		float inv_z= 1.0f / surface_final_vertices[e].z;
		surface_final_vertices[e].x= (surface_final_vertices[e].x * inv_z + vertex_projection.x_add ) * vertex_projection.x_mul;
		surface_final_vertices[e].y= (surface_final_vertices[e].y * inv_z + vertex_projection.y_add ) * vertex_projection.y_mul;
	}

	//select mip level
	int mip_level;
	mip_level= GetSurfaceMipLevel( surf, final_vertex_count );

	if( mip_level >= MIPLEVELS ) mip_level= MIPLEVELS-1;
	panzer_surf_cache_t* cache= GenerateSurfaceCache( surf, texinfo->image, mip_level );
	command_buffer.current_pos +=
		ComIn_SetTextureRaw( command_buffer.current_pos + (char*)command_buffer.buffer, cache->current_frame_data,
		cache->width_log2, cache->height_log2 );

put_draw_command:

	char* buff= (char*) command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;
	((int*)buff)[0]= COMMAND_DRAW_TRIANGLE;
	buff+=sizeof(int);
	DrawTriangleCall* call= (DrawTriangleCall*)buff;
	call->DrawFromBufferFunc= (mip_level == 0) ? near_draw_func : far_draw_func;
	call->triangle_count= 0;
	call->vertex_size= sizeof(int)*3 + sizeof(int)*2; // cached surface vertices has only coords and tex coords
	buff+= sizeof(DrawTriangleCall);
	if( final_vertex_count == 0 )
		return 0;
	if( mip_level != 0 ) tex_coord_shift= 0;//no need texture shift for nearest textures
	for( int v= 0; v< final_vertex_count; v++ )
	{
		tc_u[v]= (tc_u[v]>>mip_level) + tex_coord_shift + 65536;
		tc_v[v]= (tc_v[v]>>mip_level) + tex_coord_shift + 65536;//+65536 - skip surface border
		surface_final_vertices[v].z*= 65536.0f;//prepare to convertion to fixd16_t format
	}
	const float z_min_scaled= PSR_MIN_ZMIN_FLOAT * 65536.0f;
	for( int t= 0; t< final_vertex_count-2; t++ )
	{
		if( surface_final_vertices[0].z <= z_min_scaled || surface_final_vertices[t+1].z <= z_min_scaled || surface_final_vertices[t+2].z <= z_min_scaled )
			continue;
		using namespace VertexProcessing;
		triangle_in_tex_coord[0]= tc_u[0];
		triangle_in_tex_coord[1]= tc_v[0];
		triangle_in_tex_coord[2]= tc_u[t+1];
		triangle_in_tex_coord[3]= tc_v[t+1];
		triangle_in_tex_coord[4]= tc_u[t+2];
		triangle_in_tex_coord[5]= tc_v[t+2];
	
		triangle_in_vertex_xy[0]= fixed16_t(surface_final_vertices[0].x);
		triangle_in_vertex_xy[1]= fixed16_t(surface_final_vertices[0].y);
		triangle_in_vertex_xy[2]= fixed16_t(surface_final_vertices[t+1].x);
		triangle_in_vertex_xy[3]= fixed16_t(surface_final_vertices[t+1].y);
		triangle_in_vertex_xy[4]= fixed16_t(surface_final_vertices[t+2].x);
		triangle_in_vertex_xy[5]= fixed16_t(surface_final_vertices[t+2].y);
		triangle_in_vertex_z[0]= fixed16_t(surface_final_vertices[0].z);
		triangle_in_vertex_z[1]= fixed16_t(surface_final_vertices[t+1].z);
		triangle_in_vertex_z[2]= fixed16_t(surface_final_vertices[t+2].z);

		if( DrawWorldCachedTriangleToBuffer(buff) != 0 )
		{
			buff+= 4 * call->vertex_size;
			call->triangle_count++;
			triangle_count++;
		}
	}//for t

	return triangle_count;
}



void DrawWorldAlphaSurfaces()
{
	triangle_draw_func_t  near_draw_func= GetWorldNearDrawFunc(true);
	triangle_draw_func_t  far_draw_func= GetWorldFarDrawFunc(true);
	triangle_draw_func_t  near_draw_func_no_lightmap= GetWorldNearDrawFuncNoLightmaps(true);
	triangle_draw_func_t  far_draw_func_no_lightmap= GetWorldFarDrawFuncNoLightmaps(true);

	triangle_draw_func_t near_cache_draw_func= GetWorldCachedNearDrawFunc(true);
	triangle_draw_func_t far_cache_draw_func= GetWorldCachedFarDrawFunc(true);

	int tex_coord_shift= GetTexCoordNearShift();

	msurface_t* surf= alpha_surfaces_chain.first_surface;
	int surf_cout= alpha_surfaces_chain.surf_count;
	command_buffer.current_pos +=
			ComIn_SetConstantBlendFactor( command_buffer.current_pos + (char*)command_buffer.buffer, 64*3 );
	while( surf_cout != 0 )
	{
		/*mplane_t* plane= surf->plane;
		float dot= DotProduct(plane->normal, cam_pos) - plane->dist;
		if( (surf->flags&SURF_PLANEBACK) != 0 )
			dot= -dot;
		if( dot <= 0.0f )
			goto next_surface;*/

		mtexinfo_t* texinfo= surf->texinfo;
		int tex_frame= current_frame % texinfo->numframes;
		int fr= tex_frame;
		while(fr)
		{
			texinfo= texinfo->next;
			fr--;
		}

		bool no_lightmaps= (surf->flags&SURF_DRAWTURB)!= 0 || surf->samples == NULL;
		bool surf_cachable= IsSurfaceCachable(surf);
		if(!no_lightmaps && !surf_cachable)
		{
			command_buffer.current_pos +=
			ComIn_SetLightmap( command_buffer.current_pos + (char*)command_buffer.buffer,
				L_GetSurfaceDynamicLightmap(surf), (surf->extents[0]>>4) + 1 );
		}
		int cur_triangles;
		int vert_size;
		if(no_lightmaps)
		{
			vert_size= sizeof(int)*3 + sizeof(int)*2;//coord + tex_coord
			cur_triangles= DrawWorldSurface(surf, near_draw_func_no_lightmap, far_draw_func_no_lightmap, texinfo, tex_coord_shift );
		}
		else
		{
			if(surf_cachable)
			{
				cur_triangles= DrawWorldCachedSurface( surf,
					near_cache_draw_func, far_cache_draw_func,texinfo, tex_coord_shift );
				vert_size= sizeof(int)*3 + sizeof(int)*2;
			}
			else
			{
				vert_size= sizeof(int)*3 + sizeof(int)*2 + sizeof(int)*2;
				cur_triangles= DrawWorldSurface(surf, near_draw_func, far_draw_func, texinfo, tex_coord_shift );
			}
		}
		command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * vert_size;

next_surface:
		surf= surf->nextalphasurface;
		surf_cout--;
	};

}

/*void DrawWorldTextureChains()
{
	int tex_coord_shift;
	int tex_mode;
	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
		tex_coord_shift= -32768;
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
		tex_coord_shift= -32768;

	else
		tex_coord_shift= 0;

	triangle_draw_func_t  near_draw_func= GetWorldNearDrawFunc(false);
	triangle_draw_func_t  far_draw_func= GetWorldFarDrawFunc(false);
	triangle_draw_func_t  near_draw_func_no_lightmap= GetWorldNearDrawFuncNoLightmaps(false);
	triangle_draw_func_t  far_draw_func_no_lightmap= GetWorldFarDrawFuncNoLightmaps(false);

	int triangle_count= 0, face_count= 0;
	for( int i= 0; i< MAX_RIMAGES; i++ )
	{
		msurface_t* surf= texture_surfaces_chain.first_surface[i];
		msurface_t* surf_last= texture_surfaces_chain.last_surface[i];
		if(surf == NULL)
			continue;
		while(1)
		{
			face_count++;

			bool no_lightmaps= (surf->flags&SURF_DRAWTURB)!= 0 || (surf->texinfo->flags&SURF_WARP)!= 0 || surf->samples == NULL;
			if(!no_lightmaps)
			{
				command_buffer.current_pos +=
				ComIn_SetLightmap( command_buffer.current_pos + (char*)command_buffer.buffer,
					L_GetSurfaceDynamicLightmap(surf), (surf->extents[0]>>4) + 1 );
			}

			mtexinfo_t* texinfo= surf->texinfo;
			int tex_frame= current_frame % texinfo->numframes;
			int fr= tex_frame;
			while(fr)
			{
				texinfo= texinfo->next;
				fr--;
			}

			int cur_triangles;
			int vert_size;
			if(no_lightmaps)
			{
				cur_triangles= DrawWorldSurface(surf, near_draw_func_no_lightmap, far_draw_func_no_lightmap, texinfo, tex_coord_shift );
				vert_size= sizeof(int)*3 + sizeof(int)*2;//cord + tex_coord
			}
			else
			{
				cur_triangles= DrawWorldSurface(surf, near_draw_func, far_draw_func, texinfo, tex_coord_shift );
				vert_size= sizeof(int)*3 + sizeof(int)*2 + sizeof(int)*2;//cord + tex_coord + lightmap_cooed
			}
			triangle_count+= cur_triangles;
			command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * vert_size;

next_surface:
			if( surf == surf_last )
				break;
			surf= surf->nextalphasurface;
		};//for chain
	}//for textures

}*/

void DrawWorldSurfaces()
{
	int tex_coord_shift= GetTexCoordNearShift();

	triangle_draw_func_t  near_draw_func= GetWorldNearDrawFunc(false);
	triangle_draw_func_t  far_draw_func= GetWorldFarDrawFunc(false);
	triangle_draw_func_t  near_draw_func_no_lightmap= GetWorldNearDrawFuncNoLightmaps(false);
	triangle_draw_func_t  far_draw_func_no_lightmap= GetWorldFarDrawFuncNoLightmaps(false);

	triangle_draw_func_t near_cache_draw_func= GetWorldCachedNearDrawFunc( false );
	triangle_draw_func_t far_cache_draw_func= GetWorldCachedFarDrawFunc( false );

	int triangle_count= 0, face_count= 0;

	msurface_t* surf= world_surfaces_chain.first_surface;
	while( face_count < world_surfaces_chain.surf_count )
	{
		bool no_lightmaps= (surf->flags&SURF_DRAWTURB)!= 0 || surf->samples == NULL;
		bool surf_cachable= IsSurfaceCachable( surf );
		if(!no_lightmaps && !surf_cachable)
		{
			command_buffer.current_pos +=
			ComIn_SetLightmap( command_buffer.current_pos + (char*)command_buffer.buffer,
				L_GetSurfaceDynamicLightmap(surf), (surf->extents[0]>>4) + 1 );
		}

		mtexinfo_t* texinfo= surf->texinfo;
		int tex_frame= current_frame % texinfo->numframes;
		int fr= tex_frame;
		while(fr)
		{
			texinfo= texinfo->next;
			fr--;
		}

		int cur_triangles;
		int vert_size;
		if(no_lightmaps)
		{
			cur_triangles= DrawWorldSurface(surf, near_draw_func_no_lightmap, far_draw_func_no_lightmap, texinfo, tex_coord_shift );
			vert_size= sizeof(int)*3 + sizeof(int)*2;//cord + tex_coord
		}
		else
		{
			if(surf_cachable)
			{
				cur_triangles= DrawWorldCachedSurface( surf,
					near_cache_draw_func, far_cache_draw_func,texinfo, tex_coord_shift );
				vert_size= sizeof(int)*3 + sizeof(int)*2;
			}
			else
			{
				cur_triangles= DrawWorldSurface(surf, near_draw_func, far_draw_func, texinfo, tex_coord_shift );
				vert_size= sizeof(int)*3 + sizeof(int)*2 + sizeof(int)*2;//cord + tex_coord + lightmap_cooed
			}
		}
		triangle_count+= cur_triangles;
		command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * vert_size;

		surf= surf->nextalphasurface;
		face_count++;
	}
}

static int surfaces_pushed= 0;

void DrawTree_r( mnode_t* node, vec3_t cam_pos )
{
	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	if( node->contents == CONTENTS_NODE )
	{
		//reject node, if it behind frustrum plane. approximate node by sphere
		float node_center[3];
		float node_diagonal[3];
		for( int i= 0; i< 3; i++ )
		{
			node_center[i]= float(node->minmaxs[i] + node->minmaxs[i+3]) * 0.5f;
			node_diagonal[i]= float(node->minmaxs[i+3] - node->minmaxs[i]);
		}
		float neg_node_radius= -0.5f * sqrtf( DotProduct(node_diagonal,node_diagonal) );
		for( int i= 0; i< 5; i++ )
		{
			mplane_t* plane= clip_planes + i;
			float dst_to_clip_plane= DotProduct( node_center, plane->normal ) - plane->dist;
			if( dst_to_clip_plane < neg_node_radius )
				return;
		}

		int front, back;
		float* normal= node->plane->normal;
		//DO NOT CHANGE THIS!!! THIS IS BSP SORTING ORDER
		float dot= DotProduct(normal, cam_pos) - node->plane->dist;
		if( dot <= 0.0f )
			front= 1;
		else
			front= 0;
		back= 1 - front;

		if( node->children[front] != NULL )
			if( node->children[front]->visframe == r_visframecount )
				DrawTree_r( node->children[front], cam_pos );


		msurface_t* surf= r_worldmodel->surfaces + node->firstsurface;
		for( int s= 0; s< node->numsurfaces; s++, surf++ )
		{
			if( surf->texinfo == NULL )
				continue;

			float surf_sign= ( (surf->flags&SURF_PLANEBACK) == 0 ) ? 1.0f : -1.0f;
			if( dot * surf_sign < 0.0f )
				continue;

			surfaces_pushed++;
			if( (surf->flags & SURF_DRAWSKY) != 0 )
			{
				if( sky_surfaces_chain.first_surface == NULL )
					sky_surfaces_chain.first_surface= sky_surfaces_chain.last_surface= surf;
				else
				{
					sky_surfaces_chain.last_surface->nextalphasurface= surf;
					sky_surfaces_chain.last_surface= surf;
				}
				sky_surfaces_chain.surf_count++;
			}//if sky
			else if( (surf->texinfo->flags&(SURF_TRANS66|SURF_TRANS33)) !=0 )
			{
				if( alpha_surfaces_chain.first_surface == NULL )
				{
					alpha_surfaces_chain.first_surface=
					alpha_surfaces_chain.last_surface= surf;
				}
				else
				{
					//Push surface to beginning of linked list. Becouse we need back2front alphasurfaces list,
					//but tree we walk front2back.
					surf->nextalphasurface= alpha_surfaces_chain.first_surface;
					alpha_surfaces_chain.first_surface= surf;
				}
				alpha_surfaces_chain.surf_count++;
			}//if transparent
			else
			{
				if( world_surfaces_chain.first_surface == NULL )
				{
					world_surfaces_chain.last_surface= world_surfaces_chain.first_surface= surf;
				}
				else
				{
					world_surfaces_chain.last_surface->nextalphasurface= surf;
					world_surfaces_chain.last_surface= surf;
				}
				world_surfaces_chain.surf_count++;
			}//if default surface
		}//draw faces


		if( node->children[back] != NULL )
			if( node->children[back]->visframe == r_visframecount )
				DrawTree_r( node->children[back], cam_pos );

	}//if node, not leaf
}



void BuildSurfaceLists(m_Mat4* mat, vec3_t new_cam_pos )
{
	VectorCopy( new_cam_pos, cam_pos );

	int triangle_count= 0, face_count= 0;

	r_viewleaf = Mod_PointInLeaf (cam_pos, r_worldmodel);
	r_viewcluster = r_viewleaf->cluster;
	R_MarkLeaves();
	InitTextureSurfacesChain();

	for( int i= 0; i< r_worldmodel->numleafs; i++ )
	{
		if( r_worldmodel->leafs[i].visframe != r_visframecount )
			continue;
		mleaf_t* leaf= r_worldmodel->leafs + i;
		msurface_t** surfaces= leaf->firstmarksurface;

		if (r_newrefdef.areabits)
		{
			if (! (r_newrefdef.areabits[leaf->area>>3] & (1<<(leaf->area&7)) ) )
				continue;		// not visible
		}

		for( int s= 0; s< leaf->nummarksurfaces; s++ )
		{
			msurface_t* surf= surfaces[s];
			//surf->nextalphasurface= NULL;
			surf->visframe= r_visframecount;
			surf->dlightframe= 0;
		}
	}

	surfaces_pushed= 0;

	DrawTree_r( r_worldmodel->nodes, cam_pos );

	//char surfaces_statistic_str[64]; sprintf( surfaces_statistic_str, "surfaces pushed: %d\n", surfaces_pushed );
	//DrawCharString( 8, vid.height-16, surfaces_statistic_str );


	m_Mat4 worlmodel_lights_transform_mat;
	worlmodel_lights_transform_mat.Identity();
	R_PushDlights( r_worldmodel, (float*)&worlmodel_lights_transform_mat );

	view_matrix= *mat;
}
