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
m_Vec3 cam_pos;
float fov_y_rad;
float texels_in_pixel= 64.0f * 2.0f / 768.0f;// number of texels in screen, on surface on distance 1m.



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
	//magic number here. 64 - number of texels in Q2 meter
	texels_in_pixel= tan(fov*0.5f) * 64.0f * 2.0f / float(vid.height);
}

const int max_poly_vertices= 24;
mvertex_t* surface_vertices[max_poly_vertices];
m_Vec3 surface_final_vertices[max_poly_vertices];
int GetSurfaceMipLevel( int vertex_count, vec3_t normal )
{
	float dst= 0;
	int front_vertex_count= 0;
	for( int i= 0; i< vertex_count; i++ )
	{
		if( surface_final_vertices[i].z > PSR_MIN_ZMIN_FLOAT )
		{
			dst+= surface_final_vertices[i].z;
			front_vertex_count++;
		}
	}
	dst/= float(front_vertex_count);
	float pixels_per_texel= dst * texels_in_pixel;
	return FastIntLog2Clamp0( int(pixels_per_texel * 1.35f) );

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
	for(int e= 0; e< surf->numedges; e++ )
	{
		int ind= r_worldmodel->surfedges[e+surf->firstedge];
		if( ind > 0 )
			surface_vertices[e]= r_worldmodel->vertexes + r_worldmodel->edges[ind].v[0];
		else
			surface_vertices[e]= r_worldmodel->vertexes + r_worldmodel->edges[-ind].v[1];
		tc_u[e]= int( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3])*65536.0f );
		tc_v[e]= int( (DotProduct( surface_vertices[e]->position, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3])*65536.0f );
		lightmap_tc_u[e]= (tc_u[e] - (surf->texturemins[0]<<16) )>>4;
		lightmap_tc_v[e]= (tc_v[e] - (surf->texturemins[1]<<16) )>>4;

		((mvertex_t*)surface_final_vertices)[e]= *(surface_vertices[e]);
		surface_final_vertices[e]= surface_final_vertices[e] * view_matrix;
	}

	int mip_level= GetSurfaceMipLevel( surf->numedges, surf->plane->normal );
	Texture* tex= R_FindTexture( texinfo->image );
	if( mip_level > 3 ) mip_level= 3;
	command_buffer.current_pos += 
	ComIn_SetTextureLod( command_buffer.current_pos + (char*)command_buffer.buffer, tex, mip_level );


	char* buff= (char*) command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;

	((int*)buff)[0]= COMMAND_DRAW_TRIANGLE;
	buff+=sizeof(int);
	DrawTriangleCall* call= (DrawTriangleCall*)buff;
	call->DrawFromBufferFunc= near_draw_func;
	call->triangle_count= 0;
	call->vertex_size= sizeof(int)*3 + sizeof(int)*2 + sizeof(int)*2;
	buff+= sizeof(DrawTriangleCall);
	for( int t= 0; t< surf->numedges-2; t++ )
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
		if( final_vertices[0].x <= 0.0f || final_vertices[0].x >= width_f || final_vertices[0].y <= 0.0f || final_vertices[0].y >= height_f )
			continue;
		if( final_vertices[1].x <= 0.0f || final_vertices[1].x >= width_f || final_vertices[1].y <= 0.0f || final_vertices[1].y >= height_f )
			continue;
		if( final_vertices[2].x <= 0.0f || final_vertices[2].x >= width_f || final_vertices[2].y <= 0.0f || final_vertices[2].y >= height_f )
			continue;
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

		VertexProcessing::triangle_in_vertex_xy[0]= int(final_vertices[0].x);
		VertexProcessing::triangle_in_vertex_xy[1]= int(final_vertices[0].y);
		VertexProcessing::triangle_in_vertex_xy[2]= int(final_vertices[1].x);
		VertexProcessing::triangle_in_vertex_xy[3]= int(final_vertices[1].y);
		VertexProcessing::triangle_in_vertex_xy[4]= int(final_vertices[2].x);
		VertexProcessing::triangle_in_vertex_xy[5]= int(final_vertices[2].y);
		VertexProcessing::triangle_in_vertex_z[0]= fixed16_t(final_vertices[0].z*65536.0f);
		VertexProcessing::triangle_in_vertex_z[1]= fixed16_t(final_vertices[1].z*65536.0f);
		VertexProcessing::triangle_in_vertex_z[2]= fixed16_t(final_vertices[2].z*65536.0f);
		if( DrawWorldTriangleToBuffer( buff ) != 0 )
		{
			buff+= 4 * call->vertex_size;
			call->triangle_count++;
			triangle_count++;
		}
	}//for t

	return triangle_count;
}

static unsigned char water_lightmap[2*2*4];
void InitWaterLightmap()
{
	if( strcmp(r_lightmap_mode->string, "lightmap_linear_rgbs" ) == 0 )
		for( int i= 0; i< sizeof(water_lightmap); i+=4 )
		{
			water_lightmap[i+0]= 255;
			water_lightmap[i+1]= 255;
			water_lightmap[i+2]= 255;
			water_lightmap[i+3]= 192;
		}
	else
		memset(water_lightmap, 192, sizeof(water_lightmap));

}

void DrawAlphaSurfaces(vec3_t cam_pos )
{
	triangle_draw_func_t  draw_func = GetWorldDrawFunc(TEXTURE_FAKE_FILTER, true);
	msurface_t* surf= alpha_surfaces_chain.first_surface;
	int surf_cout= alpha_surfaces_chain.surf_count;

	::cam_pos.x= cam_pos[0];
	::cam_pos.y= cam_pos[1];
	::cam_pos.z= cam_pos[2];

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

		command_buffer.current_pos += 
		ComIn_SetLightmap( command_buffer.current_pos + (char*)command_buffer.buffer,
			L_GetSurfaceDynamicLightmap(surf), (surf->extents[0]>>4) + 1 );
		int cur_triangles= DrawWorldSurface(surf, draw_func, draw_func, surf->texinfo, 0 );

		command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * sizeof(int)*7;

next_surface:
		surf= surf->nextalphasurface;
		surf_cout--;
	};

}

void DrawTextureChains(vec3_t cam_pos)
{
	int tex_coord_shift;
	int tex_mode;
	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
	{
		tex_coord_shift= -32768;
		tex_mode= TEXTURE_LINEAR;
	}
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
	{
		tex_coord_shift= -32768;
		tex_mode= TEXTURE_FAKE_FILTER;
	}
	else
	{
		tex_coord_shift= 0;
		tex_mode= TEXTURE_NEAREST;
	}

	triangle_draw_func_t  draw_func = GetWorldDrawFunc(tex_mode, false);

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
			
			command_buffer.current_pos += 
			ComIn_SetLightmap( command_buffer.current_pos + (char*)command_buffer.buffer,
				L_GetSurfaceDynamicLightmap(surf), (surf->extents[0]>>4) + 1 );

			int cur_triangles= DrawWorldSurface(surf, draw_func, draw_func, surf->texinfo, tex_coord_shift );
			triangle_count+= cur_triangles;
			command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * sizeof(int)*7;

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
			if( (surf->texinfo->flags&(SURF_NODRAW|SURF_WARP|SURF_FLOWING)) != 0 )//temporary, discard water surfaces
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


void DrawWorldNodes(m_Mat4* mat, vec3_t cam_pos )
{
	float width2= float(vid.width)*0.5f;
	float height2= float(vid.height)*0.5f;
	float width= float(vid.width);
	float height= float(vid.height);

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

	width_f= float(vid.width);
	width2_f= width_f * 0.5f;
	height_f= float(vid.height);
	height2_f= height_f * 0.5f;

	view_matrix= *mat;
	DrawTextureChains(cam_pos);
	DrawAlphaSurfaces(cam_pos);
}
