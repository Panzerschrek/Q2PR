#include "r_world_rendering.h"


/*
----sky buffer-------
*/
Texture sky_textures[6];
image_t sky_images[6];
bool is_any_sky= false;
char current_sky_name[MAX_QPATH]= {0,0,0,0};
float sky_rotation_speed;
vec3_t sky_rotation_axis;

extern "C" void LoadTGA (char *name, byte **pic, int *width, int *height);


extern "C" void PANZER_SetSky(char *name, float rotate, vec3_t axis)
{
	sky_rotation_speed= rotate;
	VectorCopy( axis, sky_rotation_axis );
	VectorNormalize(sky_rotation_axis);

	if( strcmp( current_sky_name, name ) != 0 )
	{
		strcpy( current_sky_name, name );

		static const char*const suf[6] = {"rt", "up", "ft", "lf", "dn", "bk"};
		char	pathname[MAX_QPATH];
		for( int i= 0; i< 6; i++ )
		{
			Com_sprintf (pathname, sizeof(pathname), "env/%s%s.tga", name, suf[i]);
			if( sky_images[i].pixels[0] != NULL )
				free(sky_images[i].pixels[0]);
			LoadTGA( pathname, &sky_images[i].pixels[0], &sky_images[i].width, &sky_images[i].height );

			for( int j= 0; j< sky_images[i].width * sky_images[i].height*4; j+=4 )
				ColorByteSwap( sky_images[i].pixels[0] + j );

			sky_textures[i].Create( sky_images[i].width, sky_images[i].height, false, NULL, sky_images[i].pixels[0] );
		}
	}
	is_any_sky= true;
}

#define SKY_SIZE (Q2_UNITS_PER_METER*2)

static const float cube_vertices[]= 
{ 
	 float(SKY_SIZE),  float(SKY_SIZE),  float(SKY_SIZE),
	 float(SKY_SIZE), -float(SKY_SIZE),  float(SKY_SIZE),
	 float(SKY_SIZE),  float(SKY_SIZE), -float(SKY_SIZE),
	 float(SKY_SIZE), -float(SKY_SIZE), -float(SKY_SIZE),
	-float(SKY_SIZE),  float(SKY_SIZE),  float(SKY_SIZE),
	-float(SKY_SIZE), -float(SKY_SIZE),  float(SKY_SIZE),
	-float(SKY_SIZE),  float(SKY_SIZE), -float(SKY_SIZE),
	-float(SKY_SIZE), -float(SKY_SIZE), -float(SKY_SIZE)
};
		
static const int cube_faces_indeces[]= 
{
	3,1,0,2,//x+
	5,4,0,1,//z+
	7,5,1,3,//y-
	6,4,5,7,//x-
	6,7,3,2,//z-
	2,0,4,6,//y+ 
};

static const float cube_sides_texture_basises[]=
{
	 0.0f, -1.0f, 0.0f,  0.0f,  0.0f, -1.0f,//x+
	 0.0f, -1.0f, 0.0f,  1.0f,  0.0f, 0.0f,//z+
	-1.0f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f,//y-
	 0.0f,  1.0f, 0.0f,  0.0f,  0.0f, -1.0f,//x-
	 0.0f, -1.0f, 0.0f, -1.0f,  0.0f, 0.0f,//z-
	 1.0f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f,//y+
};

static float transformed_cube_vertices[ sizeof(cube_vertices)/sizeof(float) ];
static float transformed_cube_sides_texture_basises[ sizeof(cube_sides_texture_basises)/sizeof(float) ];


extern mvertex_t* surface_vertices[];
extern m_Vec3 surface_final_vertices[];

extern surfaces_chain_t sky_surfaces_chain;


static float		 sky_tex_scaler;
static int			 sky_tex_shift;
triangle_draw_func_t sky_draw_func;



void TransformSkybox()
{
	if( sky_rotation_speed< 0.01f )
	{
		memcpy( transformed_cube_vertices, cube_vertices, sizeof(transformed_cube_vertices ) );
		memcpy( transformed_cube_sides_texture_basises, cube_sides_texture_basises, sizeof(cube_sides_texture_basises) );
	}
	else
	{
		for( int i= 0; i< 8; i++ )
			RotatePointAroundVector( transformed_cube_vertices + i*3, sky_rotation_axis, cube_vertices + i*3, r_newrefdef.time * sky_rotation_speed );
		for( int i= 0; i< 12; i++ )
			RotatePointAroundVector( transformed_cube_sides_texture_basises + i*3, sky_rotation_axis, cube_sides_texture_basises + i*3, r_newrefdef.time * sky_rotation_speed  );
	}

}
void GetSkyDrawFuncAndTexParameters()
{
	sky_tex_scaler= float( sky_textures[0].SizeX() ) * (65536.0f * 0.5f  / float(SKY_SIZE) );
	sky_tex_scaler*= 1.0f - 2.0f / float( sky_textures[0].SizeX() );

	sky_tex_shift= (1<<16) * sky_textures[0].SizeX() / 2 - (1<<16);

	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
	{
		sky_draw_func= DrawSkyTriangleLinear;
		//sky_tex_shift-= (1<<15);
	}
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
	{
		sky_draw_func= DrawSkyTriangleFakeFilter;
		//sky_tex_shift-= (1<<15);
	}
	else
		sky_draw_func= DrawSkyTriangleNearest;
}	

void DrawSkyBoxFaces( const m_Mat4* rot_mat, float dst_to_skybox )
{
	fixed16_t tc_u[64];
	fixed16_t tc_v[64];

	int mip_level= 0;
	int tex_coord_shift= 0;

	m_Mat4 scale_mat;
	scale_mat.Identity();
	scale_mat.Scale( dst_to_skybox /float(SKY_SIZE) );
	m_Mat4 final_mat= scale_mat * *rot_mat;

	for( int i= 0; i< 6; i++ )//for faces
	{
		for( int v= 0; v< 4; v++ )
			surface_vertices[v]= (mvertex_t*)(transformed_cube_vertices + cube_faces_indeces[i*4+v]*3 );
		
		int face_final_vertices= ClipFace(4);
		if( face_final_vertices == 0 )
			continue;

		for( int v= 0; v< face_final_vertices; v++ )
		{
			const float* basis= transformed_cube_sides_texture_basises + i*6;
			tc_u[v]= fixed16_t( DotProduct( surface_vertices[v]->position, basis ) * sky_tex_scaler ) + sky_tex_shift;
			basis+= 3;
			tc_v[v]= fixed16_t( DotProduct( surface_vertices[v]->position, basis ) * sky_tex_scaler ) + sky_tex_shift;

			((mvertex_t*)surface_final_vertices)[v]= *(surface_vertices[v]);
			surface_final_vertices[v]= surface_final_vertices[v] * final_mat;
		}

		char* buff= (char*) command_buffer.buffer;
		buff+= command_buffer.current_pos;
		char* buff0= buff;

		buff+= ComIn_SetTexture( buff, sky_textures + i );

		((int*)buff)[0]= COMMAND_DRAW_TRIANGLE;
		buff+= sizeof(int);
		DrawTriangleCall* call= (DrawTriangleCall*)buff;
		call->DrawFromBufferFunc= sky_draw_func;
		call->triangle_count= 0;
		call->vertex_size= sizeof(int)*3 + sizeof(int)*2;
		buff+= sizeof(DrawTriangleCall);

		for( int t= 0; t< face_final_vertices-2; t++ )//for triangles of surface
		{
			m_Vec3 final_vertices[3];
			final_vertices[0]= surface_final_vertices[0  ];
			final_vertices[1]= surface_final_vertices[t+1];
			final_vertices[2]= surface_final_vertices[t+2];
			if( final_vertices[0].z <= PSR_MIN_ZMIN_FLOAT || final_vertices[1].z <= PSR_MIN_ZMIN_FLOAT || final_vertices[2].z <= PSR_MIN_ZMIN_FLOAT )
				continue;

			for( int j= 0; j< 3; j++ )
			{
				float inv_z= 1.0f / final_vertices[j].z;
				final_vertices[j].x= ( final_vertices[j].x * inv_z + vertex_projection.x_add ) * vertex_projection.x_mul;
				final_vertices[j].y= ( final_vertices[j].y * inv_z + vertex_projection.y_add ) * vertex_projection.y_mul;
			}
			using namespace VertexProcessing;
			triangle_in_tex_coord[0]= (tc_u[0]>>mip_level) + tex_coord_shift;
			triangle_in_tex_coord[1]= (tc_v[0]>>mip_level) + tex_coord_shift;
			triangle_in_tex_coord[2]= (tc_u[t+1]>>mip_level) + tex_coord_shift;
			triangle_in_tex_coord[3]= (tc_v[t+1]>>mip_level) + tex_coord_shift;
			triangle_in_tex_coord[4]= (tc_u[t+2]>>mip_level) + tex_coord_shift;
			triangle_in_tex_coord[5]= (tc_v[t+2]>>mip_level) + tex_coord_shift;
			
			triangle_in_vertex_xy[0]= fixed16_t(final_vertices[0].x);
			triangle_in_vertex_xy[1]= fixed16_t(final_vertices[0].y);
			triangle_in_vertex_xy[2]= fixed16_t(final_vertices[1].x);
			triangle_in_vertex_xy[3]= fixed16_t(final_vertices[1].y);
			triangle_in_vertex_xy[4]= fixed16_t(final_vertices[2].x);
			triangle_in_vertex_xy[5]= fixed16_t(final_vertices[2].y);
			triangle_in_vertex_z[0]= fixed16_t(final_vertices[0].z*65536.0f);
			triangle_in_vertex_z[1]= fixed16_t(final_vertices[1].z*65536.0f);
			triangle_in_vertex_z[2]= fixed16_t(final_vertices[2].z*65536.0f);
			if( DrawSkyTriangleToBuffer( buff ) != 0 )
			{
				buff+= 4 * call->vertex_size;
				call->triangle_count++;
			}

		}//for triangles
		command_buffer.current_pos+= buff-buff0;
	}//for skybox sides
}

void DrawSkyBox( const m_Mat4* rot_mat, vec3_t cam_pos )
{	
	if( !is_any_sky )
		return;

	GetSkyDrawFuncAndTexParameters();
	TransformSkybox();

	msurface_t* surf= sky_surfaces_chain.first_surface;

	const int max_face_clip_edges= 16;
	mvertex_t surf_vertices[max_face_clip_edges];//face vertices relative cam_pos

	int surf_count= 0;
	while( surf_count < sky_surfaces_chain.surf_count )
	{
		float farthest_vert_dst2= 0.0f;
		for(int e= 0; e< surf->numedges  && e< max_face_clip_edges; e++ )
		{
			int ind= r_worldmodel->surfedges[e+surf->firstedge];
			if( ind > 0 )
				surf_vertices[e]= *(r_worldmodel->vertexes + r_worldmodel->edges[ind].v[0]);
			else
				surf_vertices[e]= *(r_worldmodel->vertexes + r_worldmodel->edges[-ind].v[1]);
			VectorSubtract( surf_vertices[e].position, cam_pos, surf_vertices[e].position );

			float dst2= DotProduct( surf_vertices[e].position, surf_vertices[e].position );
			if( dst2 > farthest_vert_dst2 )
				farthest_vert_dst2= dst2;
		}

		mplane_t* planes= SetAdditionalClipPlanes( surf->numedges );

		for( int e= 0; e< surf->numedges && e< max_face_clip_edges; e++, planes++ )
		{
			int v1= e+1; if( v1 >= surf->numedges) v1= 0;
			CrossProduct( surf_vertices[e].position, surf_vertices[v1].position, planes->normal );
			VectorNormalize( planes->normal );
			planes->dist= 0.0f;
		}

		//int color= d_8to24table[ (surf - r_worldmodel->surfaces)&255 ];
		//command_buffer.current_pos+= ComIn_SetConstantColor( (char*)command_buffer.buffer + command_buffer.current_pos, (unsigned char*) &color );
		DrawSkyBoxFaces( rot_mat, sqrtf(farthest_vert_dst2) );

next_surface:
		surf= surf->nextalphasurface;
		surf_count++;

	}//for sky surfaces


	SetAdditionalClipPlanes(0);
}