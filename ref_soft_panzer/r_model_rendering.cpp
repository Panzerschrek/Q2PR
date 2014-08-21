#include "rendering.h"
#include "r_world_rendering.h"

#include "panzer_rast/math_lib/matrix.h"
#include "panzer_rast/rendering_state.h"


extern Texture* R_FindTexture( char* name );
extern Texture* R_FindTexture(image_t* image);
extern int R_GetImageIndex(image_t* image);


void CalculateEntityMatrix( entity_t* ent, m_Mat4* mat_out )
{
	float to_rad= 3.1415926535f / 180.0f;
	m_Mat4 rot_x, rot_y, rot_z, translate;
	translate.Identity();
	m_Vec3 v_new( ent->origin[0], ent->origin[1], ent->origin[2] );
	m_Vec3 v_old( ent->oldorigin[0], ent->oldorigin[1], ent->oldorigin[2] );
	m_Vec3 origin= v_old * ent->backlerp + v_new * (1.0f-ent->backlerp);
	translate.Translate( origin );

	rot_x.RotateX( -ent->angles[2] * to_rad );//check
	rot_z.RotateZ( -ent->angles[1] * to_rad );
	rot_y.RotateY( ent->angles[0] * to_rad );


	*mat_out= rot_y * rot_x * rot_z * translate;
}

void DrawSpriteEntity( entity_t* ent, m_Mat4* mat, vec3_t cam_pos )
{
	model_t* model;
	model= ent->model;
	if(model==NULL)
		return;

	dsprite_t* sprite= (dsprite_t*) model->extradata;
	m_Vec3 pos( ent->origin[0], ent->origin[1], ent->origin[2] );
	pos= pos * *mat;
	if( pos.z<= PSR_MIN_ZMIN_FLOAT )
		return;

	char* buff= (char*)command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;
	
	int frame= ent->frame %sprite->numframes;
	Texture* tex;
	if( ent->skin == NULL )
		tex= R_FindTexture( model->skins[frame] );
	else
		tex= R_FindTexture( ent->skin );
	buff+= ComIn_SetTexture( buff, tex );

	*((int*)buff)= COMMAND_DRAW_SPRITE;
	buff+= sizeof(int);
	DrawSpriteCall* call= (DrawSpriteCall*)buff;
	call->DrawSpriteFunc= DrawWorldSprite;
	call->sprite_count= 1;
	buff+= sizeof(DrawSpriteCall);
	
	float inv_z= 1.0f / pos.z;
	pos.x= ( pos.x*inv_z+ 1.0f ) * float(vid.width)*0.5f;
	pos.y= ( pos.y*inv_z + 1.0f ) * float(vid.height)*0.5f;

	//fixeme - add correction for fov
	float sprite_radius[2]= { float(sprite->frames[0].width) * inv_z, float(sprite->frames[0].height) * inv_z };
	sprite_radius[0]*= float(vid.width)/320.0f;
	sprite_radius[1]*= float(vid.height)/240.0f;

	((int*)buff)[0]= int( pos.x - sprite_radius[0] );
	((int*)buff)[1]= int( pos.y - sprite_radius[1] );
	((int*)buff)[2]= int( pos.x + sprite_radius[0] );
	((int*)buff)[3]= int( pos.y + sprite_radius[1] );
	((int*)buff)[4]= int(pos.z*65536.0f);
	buff+= sizeof(int)*5;

	command_buffer.current_pos+= (buff-buff0);
}


void DrawBrushEntity(  entity_t* ent, m_Mat4* mat, vec3_t cam_pos, bool is_aplha )
{

	model_t* model= ent->model;
	int i;
	msurface_t* surf;
	
	if( model->firstmodelsurface == 0)//HACK
		return;

	triangle_draw_func_t func= GetWorldNearDrawFunc( is_aplha );

	m_Mat4 entity_mat, result_mat;
	CalculateEntityMatrix( ent, &entity_mat );
	result_mat= entity_mat * *mat;
	SetSurfaceMatrix(&result_mat);

	//for dynamic lighting of inline submodels
	//R_PushDlights(model);

	for( i= 0, surf= model->surfaces + model->firstmodelsurface; i< model->nummodelsurfaces; i++, surf++ )
	{
		/*mplane_t* plane= surf->plane;
		float dot= DotProduct(plane->normal, cam_pos) - plane->dist;
		if( (surf->flags&SURF_PLANEBACK) != NULL )
			dot= -dot;
		if( dot <= 0.0f )
			continue;*/

		if( (surf->flags & (SURF_DRAWSKYBOX|SURF_DRAWBACKGROUND|SURF_DRAWSKY)) !=0 )
			continue;
		int img_num= ent->frame % surf->texinfo->numframes;
		mtexinfo_t* texinfo= surf->texinfo;
		while( img_num != 0 )
		{
			texinfo= texinfo->next;
			img_num--;
		}
		if( (texinfo->flags&(SURF_NODRAW|SURF_WARP|SURF_FLOWING)) != 0 )
				continue;

		command_buffer.current_pos += 
			ComIn_SetLightmap( command_buffer.current_pos + (char*)command_buffer.buffer,
			L_GetSurfaceDynamicLightmap(surf), (surf->extents[0]>>4) + 1 );
		int cur_triangles= DrawWorldSurface( surf, func, func, texinfo, 0 );

		command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * sizeof(int)*7;
	}
}


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

void DrawAliasEntity(  entity_t* ent, m_Mat4* mat, vec3_t cam_pos )
{
	float width_f= float(vid.width) * 65536.0f;
	float height_f= float(vid.height) * 65536.0f;
	float width2_f= width_f * 0.5f;
	float height2_f= height_f * 0.5f;

	dmdl_t* model= (dmdl_t* )ent->model->extradata;

	dtriangle_t* tris= (dtriangle_t*)( (char*)model + model->ofs_tris );
	dstvert_t* st= (dstvert_t*)( (char*)model + model->ofs_st );

	int current_model_frame= ent->frame % model->num_frames;
	int prev_model_frame= ent->oldframe %model->num_frames;
	daliasframe_t* frame= (daliasframe_t*)( (char*)model + model->ofs_frames + current_model_frame * model->framesize );
	daliasframe_t* old_frame= (daliasframe_t*)( (char*)model + model->ofs_frames + prev_model_frame * model->framesize );

	m_Mat4 entity_mat, result_mat;
	CalculateEntityMatrix( ent, &entity_mat );
	result_mat= entity_mat * *mat;

	char* buff= (char*) command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;

	int current_texture_frame= ent->skinnum % model->num_skins;
	Texture* tex= R_FindTexture( ent->model->skins[current_texture_frame] );
	buff+= ComIn_SetTexture( buff, tex );
	int tex_scaler[]= { 65536 * tex->SizeX() / tex->OriginalSizeX(), -65536 * tex->SizeY() / tex->OriginalSizeY() };

	*((int*)buff)= COMMAND_DRAW_TRIANGLE;
	buff+= sizeof(int);

	DrawTriangleCall* call= (DrawTriangleCall*)buff;
	call->triangle_count= 0;
	call->vertex_size= 3 * sizeof(int) + 2 * sizeof(int) + sizeof(int);
	call->DrawFromBufferFunc= DrawTexturedModelTriangleFromBuffer;
	buff+= sizeof(DrawTriangleCall);

	float inv_iterp_k= ent->backlerp;
	float interp_k= 1.0f - inv_iterp_k;
	for( int t= 0; t< model->num_tris; t++, tris++ )
	{
		m_Vec3 coord[3];//screen space coord
		int light[3];
		short vert_st[6];
		for( int i= 0; i< 3; i++ )//for 3 vertices
		{
			coord[i].x= ( float(frame->verts[ tris->index_xyz[i] ].v[0]) * frame->scale[0] + frame->translate[0] ) * interp_k + 
					( float(old_frame->verts[ tris->index_xyz[i] ].v[0]) * old_frame->scale[0] + old_frame->translate[0] ) * inv_iterp_k ;
			coord[i].y= ( float(frame->verts[ tris->index_xyz[i] ].v[1]) * frame->scale[1] + frame->translate[1] ) * interp_k + 
					( float(old_frame->verts[ tris->index_xyz[i] ].v[1]) * old_frame->scale[1] + old_frame->translate[1] ) * inv_iterp_k ;
			coord[i].z= ( float(frame->verts[ tris->index_xyz[i] ].v[2]) * frame->scale[2] + frame->translate[2] ) * interp_k +
					( float(old_frame->verts[ tris->index_xyz[i] ].v[2]) * old_frame->scale[2] + old_frame->translate[2] ) * inv_iterp_k ;
			coord[i]= coord[i] * result_mat;
			vert_st[i*2  ]= st[ tris->index_st[i] ].s;
			vert_st[i*2+1]= st[ tris->index_st[i] ].t;

			light[i]= int( r_avertexnormals [ frame->verts[ tris->index_xyz[i] ].lightnormalindex ][2] * 200.0f ) + 120.0f;
			if( light[i] < 32 ) light[i]= 32;
		}
		if( coord[0].z < PSR_MIN_ZMIN_FLOAT || coord[1].z < PSR_MIN_ZMIN_FLOAT || coord[2].z < PSR_MIN_ZMIN_FLOAT )
			continue;

		for( int i= 0; i< 3; i++ )
		{
			float inv_z= 1.0f/ coord[i].z;
			coord[i].x= ( coord[i].x * inv_z + 1.0f ) * width2_f;
			coord[i].y= ( coord[i].y * inv_z + 1.0f ) * height2_f;
		}
		if( coord[0].x < 0.0f || coord[0].x > width_f || coord[0].y < 0.0f || coord[0].y > height_f )
			continue;
		if( coord[1].x < 0.0f || coord[1].x > width_f || coord[1].y < 0.0f || coord[1].y > height_f )
			continue;
		if( coord[2].x < 0.0f || coord[2].x > width_f || coord[2].y < 0.0f || coord[2].y > height_f )
			continue;

		//bak face culling
		float v[4]= { coord[2].x - coord[1].x, coord[2].y - coord[1].y, coord[1].x - coord[0].x, coord[1].y - coord[0].y };
		if( v[0] * v[3] < v[2] * v[1] )
			continue;
		
		using namespace VertexProcessing;
		triangle_in_vertex_xy[0]= fixed16_t(coord[0].x);
		triangle_in_vertex_xy[1]= fixed16_t(coord[0].y);
		triangle_in_vertex_xy[2]= fixed16_t(coord[1].x);
		triangle_in_vertex_xy[3]= fixed16_t(coord[1].y);
		triangle_in_vertex_xy[4]= fixed16_t(coord[2].x);
		triangle_in_vertex_xy[5]= fixed16_t(coord[2].y);
		triangle_in_vertex_z[0]= fixed16_t(coord[0].z*65536.0f);
		triangle_in_vertex_z[1]= fixed16_t(coord[1].z*65536.0f);
		triangle_in_vertex_z[2]= fixed16_t(coord[2].z*65536.0f);
		triangle_in_tex_coord[0]= st[tris->index_st[0]].s*tex_scaler[0];
		triangle_in_tex_coord[1]= st[tris->index_st[0]].t*tex_scaler[1];
		triangle_in_tex_coord[2]= st[tris->index_st[1]].s*tex_scaler[0];
		triangle_in_tex_coord[3]= st[tris->index_st[1]].t*tex_scaler[1];
		triangle_in_tex_coord[4]= st[tris->index_st[2]].s*tex_scaler[0];
		triangle_in_tex_coord[5]= st[tris->index_st[2]].t*tex_scaler[1];
		triangle_in_light[0]= light[0];
		triangle_in_light[1]= light[1];
		triangle_in_light[2]= light[2];

		if( DrawTexturedModelTriangleToBuffer( buff ) != 0 )
		{
			buff+= 4 * call->vertex_size;
			call->triangle_count++;
		}
	}//for triangles

	command_buffer.current_pos+= buff- buff0;

}