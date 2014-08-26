#include "r_world_rendering.h"
#include "panzer_rast/fast_math.h"

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

typedef struct alpha_surfaces_chain_s
{
	msurface_t* first_surface;
	msurface_t* last_surface;
	int surf_count;
}alpha_surfaces_chain_t;
alpha_surfaces_chain_t alpha_surfaces_chain;


float width2_f;
float height2_f;
float width_f;
float height_f;
m_Mat4 view_matrix;
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
	DrawWorldTriangleTextureNearestLightmapLiearRGBS,
	DrawWorldTriangleTextureNearestLightmapColoredLinear,
	DrawWorldTriangleTextureNearestLightmapLinear,
	DrawWorldTriangleTextureLinearLightmapLiearRGBS,
	DrawWorldTriangleTextureLinearLightmapColoredLinear,
	DrawWorldTriangleTextureLinearLightmapLinear,
	DrawWorldTriangleTextureFakeFilterLightmapLiearRGBS,
	DrawWorldTriangleTextureFakeFilterLightmapColoredLinear,
	DrawWorldTriangleTextureFakeFilterLightmapLinear,

	DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBS,
	DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinear,
	DrawWorldTriangleTextureNearestPalettizedLightmapLinear,
	DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBS,
	DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinear,
	DrawWorldTriangleTextureLinearPalettizedLightmapLinear,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBS,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinear,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinear,
};
triangle_draw_func_t world_triangles_draw_funcs_blend_table[]=
{
	DrawWorldTriangleTextureNearestLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureNearestLightmapColoredLinearBlend,
	DrawWorldTriangleTextureNearestLightmapLinearBlend,
	DrawWorldTriangleTextureLinearLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureLinearLightmapColoredLinearBlend,
	DrawWorldTriangleTextureLinearLightmapLinearBlend,
	DrawWorldTriangleTextureFakeFilterLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureFakeFilterLightmapColoredLinearBlend,
	DrawWorldTriangleTextureFakeFilterLightmapLinearBlend,

	DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinearBlend,
	DrawWorldTriangleTextureNearestPalettizedLightmapLinearBlend,
	DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBSBlend,
	DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinearBlend,
	DrawWorldTriangleTextureLinearPalettizedLightmapLinearBlend,
	DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBSBlend,
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

	if( strcmp( r_lightmap_mode->string, "lightmap_linear_rgbs" ) == 0 )
		y= 0;
	else if( strcmp( r_lightmap_mode->string, "lightmap_linear_colored" ) == 0 )
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


void InitTextureSurfacesChain()
{
	memset( &texture_surfaces_chain, 0, sizeof(texture_surfaces_chain_t) );
	memset( &alpha_surfaces_chain, 0, sizeof(alpha_surfaces_chain_t) );
}


void SetSurfaceMatrix( m_Mat4* mat )
{
	view_matrix= *mat;
}
void SetFov( float fov )
{
	fov_y_rad= fov;
	texels_in_pixel= tanf(fov*0.5f) * float(Q2_UNITS_PER_METER) * 2.0f / float(vid.height);
}


const int max_poly_vertices= 24;
mvertex_t* surface_vertices[max_poly_vertices];
m_Vec3 surface_final_vertices[max_poly_vertices];
bool surface_plane_pos[max_poly_vertices];
/*
0 - near plane
1, 2 - left, right
3, 4 - bottom, top
*/
mplane_t view_planes[8];
mvertex_t tmp_vertices_stack[max_poly_vertices + 8];
int tmp_vertices_stack_pos;


int GetSurfaceMipLevel( msurface_t* surf )
{
	float dst= 0;
	int front_vertex_count= 0;
	for( int i= 0; i< surf->numedges; i++ )
	{
		if( surface_final_vertices[i].z > PSR_MIN_ZMIN_FLOAT )
		{
			dst+= surface_final_vertices[i].z;
			front_vertex_count++;
		}
	}
	dst/= float(front_vertex_count);

	float* tex_basis_vec= surf->texinfo->vecs[0];
	float pixels_per_texel= dst * texels_in_pixel * sqrtf( DotProduct(tex_basis_vec,tex_basis_vec) );
	return FastIntLog2Clamp0( int(pixels_per_texel * 1.35f) );
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
	//else return 0;//HACK temporary

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
		
		float k0= fabs( DotProduct(v0,normal) - plane->dist );
		float k1= fabs( DotProduct(v1,normal) - plane->dist );
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
		for( int i= 2, j= splitted_edge[0]+1; i < vertex_count; i++, j++ )
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
	for( int i= 0; i< 5; i++ )
	{
		vertex_count= ClipFaceByPlane( vertex_count, view_planes + i );
		if( vertex_count == 0 )
			return 0;
	}

	return vertex_count;
}


void SetClipPlanes( mplane_t* planes, int count )
{
	for( int i= 0; i< count && i<8; i++ )
		view_planes[i]= planes[i];
}

//returns number of triangles
int DrawWorldSurface( msurface_t* surf, triangle_draw_func_t near_draw_func, triangle_draw_func_t far_draw_func, mtexinfo_t* texinfo, fixed16_t tex_coord_shift )
{
	int triangle_count= 0;

	int tc_u[max_poly_vertices];
	int tc_v[max_poly_vertices];
	int lightmap_tc_u[max_poly_vertices];
	int lightmap_tc_v[max_poly_vertices];
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
		tc_u[e]= int( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3])*65536.0f );
		tc_v[e]= int( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3])*65536.0f );
		lightmap_tc_u[e]= (tc_u[e] - (surf->texturemins[0]<<16) )>>4;
		lightmap_tc_v[e]= (tc_v[e] - (surf->texturemins[1]<<16) )>>4;

		((mvertex_t*)surface_final_vertices)[e]= *(surface_vertices[e]);
		surface_final_vertices[e]= surface_final_vertices[e] * view_matrix;
	}
	//texture coord morfing
	if( (surf->texinfo->flags&SURF_FLOWING) != 0 )
	{
		int tc_delta= ( int( -0.5f * r_newrefdef.time * float(texinfo->image->width) ) & (texinfo->image->width*4-1) )<<16;
		for(int e= 0; e< surf->numedges; e++ )
			tc_u[e]+= tc_delta;
	}

	//select texture and mip level
	int mip_level= GetSurfaceMipLevel( surf );
	Texture* tex= R_FindTexture( texinfo->image );
	if( mip_level >= MIPLEVELS ) mip_level= MIPLEVELS-1;
	command_buffer.current_pos += 
	ComIn_SetTextureLod( command_buffer.current_pos + (char*)command_buffer.buffer, tex, mip_level );

	bool no_lightmaps= (surf->flags&SURF_DRAWTURB)!= 0 || (surf->texinfo->flags&SURF_WARP)!= 0;
put_draw_command:
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
	for( int t= 0; t< final_vertex_count-2; t++ )
	{
		m_Vec3 final_vertices[3];
		final_vertices[0]= surface_final_vertices[0  ];
		final_vertices[1]= surface_final_vertices[t+1];
		final_vertices[2]= surface_final_vertices[t+2];
		if( final_vertices[0].z <= PSR_MIN_ZMIN_FLOAT || final_vertices[1].z <= PSR_MIN_ZMIN_FLOAT || final_vertices[2].z <= PSR_MIN_ZMIN_FLOAT )
			continue;
		if( final_vertices[0].z > 128.0f || final_vertices[1].z > 128.0f || final_vertices[2].z > 128.0f )
			continue;

		for( int j= 0; j< 3; j++ )
		{
			float inv_z= 1.0f / final_vertices[j].z;
			final_vertices[j].x= ( final_vertices[j].x * inv_z + 1.0f ) * width2_f;
			final_vertices[j].y= ( final_vertices[j].y * inv_z + 1.0f ) * height2_f;
		}
		//temporary, discard triangles outside the screen
		/*if( final_vertices[0].x <= 0.0f || final_vertices[0].x >= width_f || final_vertices[0].y <= 0.0f || final_vertices[0].y >= height_f )
			continue;
		if( final_vertices[1].x <= 0.0f || final_vertices[1].x >= width_f || final_vertices[1].y <= 0.0f || final_vertices[1].y >= height_f )
			continue;
		if( final_vertices[2].x <= 0.0f || final_vertices[2].x >= width_f || final_vertices[2].y <= 0.0f || final_vertices[2].y >= height_f )
			continue;*/
		VertexProcessing::triangle_in_tex_coord[0]= (tc_u[0]>>mip_level) + tex_coord_shift;
		VertexProcessing::triangle_in_tex_coord[1]= (tc_v[0]>>mip_level) + tex_coord_shift;
		VertexProcessing::triangle_in_tex_coord[2]= (tc_u[t+1]>>mip_level) + tex_coord_shift;
		VertexProcessing::triangle_in_tex_coord[3]= (tc_v[t+1]>>mip_level) + tex_coord_shift;
		VertexProcessing::triangle_in_tex_coord[4]= (tc_u[t+2]>>mip_level) + tex_coord_shift;
		VertexProcessing::triangle_in_tex_coord[5]= (tc_v[t+2]>>mip_level) + tex_coord_shift;
		VertexProcessing::triangle_in_lightmap_tex_coord[0]= lightmap_tc_u[0];
		VertexProcessing::triangle_in_lightmap_tex_coord[1]= lightmap_tc_v[0];
		VertexProcessing::triangle_in_lightmap_tex_coord[2]= lightmap_tc_u[t+1];
		VertexProcessing::triangle_in_lightmap_tex_coord[3]= lightmap_tc_v[t+1];
		VertexProcessing::triangle_in_lightmap_tex_coord[4]= lightmap_tc_u[t+2];
		VertexProcessing::triangle_in_lightmap_tex_coord[5]= lightmap_tc_v[t+2];

		VertexProcessing::triangle_in_vertex_xy[0]= fixed16_t(final_vertices[0].x);
		VertexProcessing::triangle_in_vertex_xy[1]= fixed16_t(final_vertices[0].y);
		VertexProcessing::triangle_in_vertex_xy[2]= fixed16_t(final_vertices[1].x);
		VertexProcessing::triangle_in_vertex_xy[3]= fixed16_t(final_vertices[1].y);
		VertexProcessing::triangle_in_vertex_xy[4]= fixed16_t(final_vertices[2].x);
		VertexProcessing::triangle_in_vertex_xy[5]= fixed16_t(final_vertices[2].y);
		VertexProcessing::triangle_in_vertex_z[0]= fixed16_t(final_vertices[0].z*65536.0f);
		VertexProcessing::triangle_in_vertex_z[1]= fixed16_t(final_vertices[1].z*65536.0f);
		VertexProcessing::triangle_in_vertex_z[2]= fixed16_t(final_vertices[2].z*65536.0f);

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


void DrawWorldAlphaSurfaces()
{
	triangle_draw_func_t  near_draw_func= GetWorldNearDrawFunc(true);
	triangle_draw_func_t  far_draw_func= GetWorldFarDrawFunc(true);
	triangle_draw_func_t  near_draw_func_no_lightmap= GetWorldNearDrawFuncNoLightmaps(true);
	triangle_draw_func_t  far_draw_func_no_lightmap= GetWorldFarDrawFuncNoLightmaps(true);

	msurface_t* surf= alpha_surfaces_chain.first_surface;
	int surf_cout= alpha_surfaces_chain.surf_count;
	command_buffer.current_pos += 
			ComIn_SetConstantBlendFactor( command_buffer.current_pos + (char*)command_buffer.buffer, 64*3 );
	while( surf_cout != 0 )
	{
		mplane_t* plane= surf->plane;
		float dot= DotProduct(plane->normal, cam_pos) - plane->dist;
		if( (surf->flags&SURF_PLANEBACK) != 0 )
			dot= -dot;
		if( dot <= 0.0f )
			goto next_surface;

		mtexinfo_t* texinfo= surf->texinfo;
		int tex_frame= current_frame % texinfo->numframes;
		int fr= tex_frame;
		while(fr)
		{
			texinfo= texinfo->next;
			fr--;
		}

		bool no_lightmaps= (surf->flags&SURF_DRAWTURB)!= 0 || (surf->texinfo->flags&SURF_WARP)!= 0 || surf->samples == NULL;
		if(!no_lightmaps)
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
			cur_triangles= DrawWorldSurface(surf, near_draw_func_no_lightmap, far_draw_func_no_lightmap, texinfo, 0 );
		}
		else
		{
			vert_size= sizeof(int)*3 + sizeof(int)*2 + sizeof(int)*2;
			cur_triangles= DrawWorldSurface(surf, near_draw_func, far_draw_func, texinfo, 0 );
		}
		command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * vert_size;

next_surface:
		surf= surf->nextalphasurface;
		surf_cout--;
	};

}

void DrawWorldTextureChains()
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
			{
				mplane_t* plane= surf->plane;
				float dot= DotProduct(plane->normal, cam_pos) - plane->dist;
				if( (surf->flags&SURF_PLANEBACK) == SURF_PLANEBACK )
					dot= -dot;
				if( dot <= 0.0f )
					goto next_surface;
			}
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

/*	char triangles_str[64];
	sprintf( triangles_str, "triangles: %d\n faces: %d", triangle_count, face_count );
	DrawCharString( 8, 64, triangles_str );*/
}

void DrawTree_r( mnode_t* node, vec3_t cam_pos )
{
	if( node->contents != -1 )
	{
		mleaf_t* leaf= (mleaf_t*)node;
		msurface_t** surfaces= leaf->firstmarksurface;
		for( int s= 0; s< leaf->nummarksurfaces; s++)
		{
			msurface_t* surf= surfaces[s];

			if( surf->texinfo == NULL )
				continue;
			if( (surf->flags & (SURF_DRAWSKYBOX|SURF_DRAWBACKGROUND|SURF_DRAWSKY)) !=0 )
				continue;
			if( (surf->texinfo->flags&(SURF_NODRAW)) != 0 )//temporary, discard water surfaces
				continue;
			if(surf->nextalphasurface != NULL )//surface already in list
				continue;

			if( (surf->texinfo->flags&(SURF_TRANS66|SURF_TRANS33)) !=0 )
			{
				if( alpha_surfaces_chain.first_surface == NULL )
				{
					alpha_surfaces_chain.first_surface= 
					alpha_surfaces_chain.last_surface= surf;
					alpha_surfaces_chain.surf_count= 1;
				}
				else
				{
					//Push surface to beginning of linked list. Becouse we need back2front alphasurfaces list,
					//but tree we walk front2back.
					surf->nextalphasurface= alpha_surfaces_chain.first_surface;
					alpha_surfaces_chain.first_surface= surf;
					alpha_surfaces_chain.surf_count++;
				}
				continue;
			}//if transparent

			//texture chain populating
			int tex_ind= R_GetImageIndex(surf->texinfo->image);
			if( texture_surfaces_chain.first_surface[tex_ind] == NULL )
			{
				texture_surfaces_chain.first_surface[tex_ind]=
				texture_surfaces_chain.last_surface[tex_ind]= surf;
			}
			else
			{
				texture_surfaces_chain.last_surface[tex_ind]->nextalphasurface= surf;
				texture_surfaces_chain.last_surface[tex_ind]= surf;
			}
			//surf->nextalphasurface= NULL;
		}
		return;
	}

	int front, back;
	float* normal= node->plane->normal;
	//DO NOT CHANGE THIS!!! THIS IS BSP SORTING ORDER
	if( DotProduct(normal, cam_pos) <= node->plane->dist )
	{
		front= 1;
		back= 0;
	}
	else
	{
		front= 0;
		back= 1;
	}
	
	if( node->children[front] != NULL )
		if( node->children[front]->visframe == r_visframecount )
			DrawTree_r( node->children[front], cam_pos );


	if( node->children[back] != NULL )
		if( node->children[back]->visframe == r_visframecount )
			DrawTree_r( node->children[back], cam_pos );
	
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
		for( int s= 0; s< leaf->nummarksurfaces; s++ )
		{
			msurface_t* surf= surfaces[s];
			surf->nextalphasurface= NULL;
			surf->visframe= r_visframecount;
			surf->dlightframe= 0;
		}
	}
	DrawTree_r( r_worldmodel->nodes, cam_pos );
	R_PushDlights( r_worldmodel );

	width_f= float(vid.width) *65536.0f;
	width2_f= width_f * 0.5f;
	height_f= float(vid.height) *65536.0f;
	height2_f= height_f * 0.5f;

	view_matrix= *mat;
}
