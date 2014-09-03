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

	//WARNING! rotation matrices and their order can be invalid!
	rot_x.RotateX( -ent->angles[2] * to_rad );
	rot_z.RotateZ( -ent->angles[1] * to_rad );
	rot_y.RotateY( ent->angles[0] * to_rad );

	*mat_out= rot_y * rot_x * rot_z * translate;
}

void CalcualteEntityInverseMatrix( entity_t* ent, m_Mat4* mat_out, m_Mat4* normals_mat_out= NULL )
{
	float to_rad= 3.1415926535f / 180.0f;
	m_Mat4 rot_x, rot_y, rot_z, translate;
	translate.Identity();
	m_Vec3 v_new( ent->origin[0], ent->origin[1], ent->origin[2] );
	m_Vec3 v_old( ent->oldorigin[0], ent->oldorigin[1], ent->oldorigin[2] );
	m_Vec3 origin= v_old * ent->backlerp + v_new * (1.0f-ent->backlerp);
	translate.Translate( -origin );

	//WARNING! rotation matrices and thier order can be invalid!
	rot_x.RotateX( ent->angles[2] * to_rad );
	rot_z.RotateZ( ent->angles[1] * to_rad );
	rot_y.RotateY( -ent->angles[0] * to_rad );

	*mat_out= translate * rot_z * rot_x * rot_y;

	if(normals_mat_out!=NULL)
		*normals_mat_out= rot_z * rot_x * rot_y;
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
	pos.x= ( pos.x*inv_z + vertex_projection.x_add ) * vertex_projection.x_mul;
	pos.y= ( pos.y*inv_z + vertex_projection.y_add ) * vertex_projection.y_mul;

	if( pos.x <= vertex_projection.x_min || pos.x >= vertex_projection.x_max ||
		pos.y <= vertex_projection.y_min || pos.y >= vertex_projection.y_max )
		return;
 
	//fixeme - add correction for fov
	float sprite_radius[2]= { float(sprite->frames[0].width) * inv_z, float(sprite->frames[0].height) * inv_z };
	sprite_radius[0]*= float(r_newrefdef.width)/320.0f;
	sprite_radius[1]*= float(r_newrefdef.height)/240.0f;

	((int*)buff)[0]= int( pos.x - sprite_radius[0] );
	((int*)buff)[1]= int( pos.y - sprite_radius[1] );
	((int*)buff)[2]= int( pos.x + sprite_radius[0] );
	((int*)buff)[3]= int( pos.y + sprite_radius[1] );
	((int*)buff)[4]= int(pos.z*65536.0f);
	buff+= sizeof(int)*5;

	command_buffer.current_pos+= (buff-buff0);
}


void DrawBrushEntity(  entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos, bool is_aplha )
{

	model_t* model= ent->model;
	int i;
	msurface_t* surf;
	
	if( model->firstmodelsurface == 0 || model->nummodelsurfaces == 0 )//HACK
		return;

	vec3_t cam_pos_model_space;
	m_Mat4 inverse_entity_matrix, inverse_normal_matrix;
	CalcualteEntityInverseMatrix( ent, &inverse_entity_matrix, &inverse_normal_matrix );
	//convert camera position to model space, to use backplane discarding
	*((m_Vec3*)cam_pos_model_space)= *((m_Vec3*)cam_pos) * inverse_entity_matrix;

	triangle_draw_func_t  near_draw_func= GetWorldNearDrawFunc(false);
	triangle_draw_func_t  far_draw_func= GetWorldFarDrawFunc(false);
	triangle_draw_func_t  near_draw_func_no_lightmap= GetWorldNearDrawFuncNoLightmaps(false);
	triangle_draw_func_t  far_draw_func_no_lightmap= GetWorldFarDrawFuncNoLightmaps(false);

	triangle_draw_func_t  near_draw_func_alpha= GetWorldNearDrawFunc(true);
	triangle_draw_func_t  far_draw_func_alpha= GetWorldFarDrawFunc(true);
	triangle_draw_func_t  near_draw_func_no_lightmap_alpha= GetWorldNearDrawFuncNoLightmaps(true);
	triangle_draw_func_t  far_draw_func_no_lightmap_alpha= GetWorldFarDrawFuncNoLightmaps(true);


	m_Mat4 entity_mat, result_mat, inverse_entity_mat, inverse_normal_mat; 
	CalculateEntityMatrix( ent, &entity_mat );
	result_mat= entity_mat * *mat;
	SetSurfaceMatrix(&result_mat, NULL);

	inverse_normal_matrix= *normal_mat * inverse_normal_matrix;
	InitFrustrumClipPlanes( &inverse_normal_matrix, cam_pos_model_space );
	

	//for dynamic lighting of inline submodels
	R_PushDlights(model, (float*)&inverse_entity_matrix);

	for( i= 0, surf= model->surfaces + model->firstmodelsurface; i< model->nummodelsurfaces; i++, surf++ )
	{
		if( (surf->flags & (SURF_DRAWSKYBOX|SURF_DRAWBACKGROUND|SURF_DRAWSKY)) !=0 )
			continue;

		mplane_t* plane= surf->plane;
		float dot= DotProduct(plane->normal, cam_pos_model_space) - plane->dist;
		if( (surf->flags&SURF_PLANEBACK) != NULL )
			dot= -dot;
		if( dot <= 0.0f )
			continue;

		int img_num= ent->frame % surf->texinfo->numframes;
		mtexinfo_t* texinfo= surf->texinfo;
		while( img_num != 0 )
		{
			texinfo= texinfo->next;
			img_num--;
		}
		if( (texinfo->flags&SURF_NODRAW) != 0 )
				continue;

		bool no_lightmap= (surf->flags&SURF_DRAWTURB)!= 0 || surf->samples == NULL;
		bool trans_surface= (texinfo->flags&(SURF_TRANS66|SURF_TRANS33)) != 0;
		if( no_lightmap )
		{
			int cur_triangles= DrawWorldSurface( surf,
				trans_surface ? near_draw_func_no_lightmap_alpha: near_draw_func_no_lightmap,
				trans_surface ? far_draw_func_no_lightmap_alpha: far_draw_func_no_lightmap,
				texinfo, 0 );
			command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * sizeof(int)*5;
		}
		else
		{
			command_buffer.current_pos += 
				ComIn_SetLightmap( command_buffer.current_pos + (char*)command_buffer.buffer,
				L_GetSurfaceDynamicLightmap(surf), (surf->extents[0]>>4) + 1 );
			int cur_triangles= DrawWorldSurface( surf,
				trans_surface ? near_draw_func_alpha : near_draw_func, 
				trans_surface ? far_draw_func_alpha : far_draw_func,
				texinfo, 0 );
			command_buffer.current_pos+= sizeof(int) + sizeof(DrawTriangleCall) + cur_triangles * 4 * sizeof(int)*7;
		}
	}
}


#define MAX_MODEL_POLYGON_VERTICES 16
struct ModelVertex
{
	float position[3];
	float st[2];
	float light[3];
};

ModelVertex* current_model_polygon[MAX_MODEL_POLYGON_VERTICES];
ModelVertex	 current_model_polygon_vertices[MAX_MODEL_POLYGON_VERTICES];
int model_tmp_vertices_stack_pos= 0;
bool model_vertex_position[ MAX_MODEL_POLYGON_VERTICES ];
extern mplane_t clip_planes[];


int ClipModelPolygonByPlane( int vertex_count, mplane_t* plane )
{
	float* normal= plane->normal;

	int discarded_vertex_count= 0;
	for( int i= 0; i< vertex_count; i++ )
	{
		if( DotProduct(normal, current_model_polygon[i]->position ) > plane->dist )
		{
			model_vertex_position[i]= true;
		}
		else
		{
			discarded_vertex_count++;
			model_vertex_position[i]= false;// false means discarded
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
		if( !model_vertex_position[i] )//if back vertex
		{
			if( model_vertex_position[next_vertex_index] )//if front vertex
				splitted_edge[0]= i;
		}
		else//if fron vertex
		{
			if(!model_vertex_position[next_vertex_index])//if back vertex
				splitted_edge[1]= i;
		}
	}

	ModelVertex* new_v[2]={  current_model_polygon_vertices + model_tmp_vertices_stack_pos, current_model_polygon_vertices + model_tmp_vertices_stack_pos + 1 };
	model_tmp_vertices_stack_pos+= 2;

	for( int i= 0; i< 2; i++ )
	{
		int v0_ind= splitted_edge[i];
		int v1_ind= splitted_edge[i]+1; if( v1_ind == vertex_count ) v1_ind= 0;
		float* v0= current_model_polygon[v0_ind]->position, *v1= current_model_polygon[v1_ind]->position;
		
		float k0= fabs( DotProduct(v0,normal) - plane->dist );
		float k1= fabs( DotProduct(v1,normal) - plane->dist );
		float inv_dst_sum= 1.0f / ( k0 + k1 );
		k0*= inv_dst_sum;
		k1*= inv_dst_sum;

		float* out_pos= new_v[i]->position;
		out_pos[0]= k1 * v0[0] + k0 * v1[0];
		out_pos[1]= k1 * v0[1] + k0 * v1[1];
		out_pos[2]= k1 * v0[2] + k0 * v1[2];
		new_v[i]->st[0]= k1 * current_model_polygon[v0_ind]->st[0] + k0 * current_model_polygon[v1_ind]->st[0];
		new_v[i]->st[1]= k1 * current_model_polygon[v0_ind]->st[1] + k0 * current_model_polygon[v1_ind]->st[1];
		new_v[i]->light[0]= k1 * current_model_polygon[v0_ind]->light[0] + k0 * current_model_polygon[v1_ind]->light[0];
		new_v[i]->light[1]= k1 * current_model_polygon[v0_ind]->light[1] + k0 * current_model_polygon[v1_ind]->light[1];
		new_v[i]->light[2]= k1 * current_model_polygon[v0_ind]->light[2] + k0 * current_model_polygon[v1_ind]->light[2];
	}

	if( discarded_vertex_count == 2 )
	{
		current_model_polygon[ splitted_edge[0] ]= new_v[0];
		int new_v_ind= splitted_edge[1]+1; if( new_v_ind == vertex_count ) new_v_ind= 0;
		current_model_polygon[new_v_ind]= new_v[1];
		return vertex_count;
	}
	else if( discarded_vertex_count > 2 )
	{
		ModelVertex* tmp_vertices[MAX_MODEL_POLYGON_VERTICES];
		for( int i= 0; i< vertex_count; i++ )
			tmp_vertices[i]= current_model_polygon[i];

		current_model_polygon[0]= new_v[1];
		current_model_polygon[1]= new_v[0];
		int new_vertex_count= vertex_count - discarded_vertex_count + 2;
		for( int i= 2, j= splitted_edge[0]+1; i < vertex_count; i++, j++ )
		{	
			current_model_polygon[i]= tmp_vertices[j%vertex_count];
		}
		return new_vertex_count;
	}	
	else//discard one vertex
	{
		int discarded_v_ind= splitted_edge[0];
		for( int i= vertex_count; i> discarded_v_ind; i-- )
			current_model_polygon[i]= current_model_polygon[i-1];
		
		current_model_polygon[discarded_v_ind  ]= new_v[1];
		current_model_polygon[discarded_v_ind+1]= new_v[0];
		return vertex_count + 1;
	}
}

#define NUMVERTEXNORMALS	162
float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

#define MODEL_MAX_VERTICES 2048
m_Vec3 current_model_vertices[ MODEL_MAX_VERTICES ];
union
{
	unsigned char colored_light[4];
	int light;
}current_model_vertex_lights[ MODEL_MAX_VERTICES ];


int ClipModelTriangle( dtriangle_t* triangle, dstvert_t* st )//returns number of output vertices
{
	model_tmp_vertices_stack_pos= 3;
	for( int i= 0; i< 3; i++ )//generate initial polygon from triangle
	{
		m_Vec3* v= &current_model_vertices[ triangle->index_xyz[i] ];
		current_model_polygon_vertices[i].position[0]= v->x;
		current_model_polygon_vertices[i].position[1]= v->y;
		current_model_polygon_vertices[i].position[2]= v->z;
		current_model_polygon_vertices[i].st[0]= float(st[ triangle->index_st[i] ].s) * 65536.0f;
		current_model_polygon_vertices[i].st[1]= float(st[ triangle->index_st[i] ].t) * 65536.0f;//prepare to convertion to fixed16_t format

		current_model_polygon_vertices[i].light[0]= float(current_model_vertex_lights[ triangle->index_xyz[i] ].colored_light[0] );
		current_model_polygon_vertices[i].light[1]= float(current_model_vertex_lights[ triangle->index_xyz[i] ].colored_light[1] );
		current_model_polygon_vertices[i].light[2]= float(current_model_vertex_lights[ triangle->index_xyz[i] ].colored_light[2] );

		current_model_polygon[i]= current_model_polygon_vertices + i;
	}
	int vertex_count= 3;

	for( int i= 0; i< 5; i++ )
	{
		vertex_count= ClipModelPolygonByPlane( vertex_count, clip_planes + i );
		if( vertex_count == 0 )
			return 0;
	}
	return vertex_count;
}

void CalculateModelVertices(  entity_t* ent )
{
	dmdl_t* model= (dmdl_t* )ent->model->extradata;

	int current_model_frame= ent->frame % model->num_frames;
	daliasframe_t* frame= (daliasframe_t*)( (char*)model + model->ofs_frames + current_model_frame * model->framesize );

	if( model->num_frames > 1 && ent->backlerp > 0.05f )
	{
		int prev_model_frame= ent->oldframe %model->num_frames;
		daliasframe_t* old_frame= (daliasframe_t*)( (char*)model + model->ofs_frames + prev_model_frame * model->framesize );
		float inv_iterp_k= ent->backlerp;
		float interp_k= 1.0f - inv_iterp_k;
		for( int i= 0; i< model->num_xyz; i++ )
		{
			current_model_vertices[i].x= ( float(frame->verts[i].v[0]) * frame->scale[0] + frame->translate[0] ) * interp_k + 
					( float(old_frame->verts[i].v[0]) * old_frame->scale[0] + old_frame->translate[0] ) * inv_iterp_k ;
			current_model_vertices[i].y= ( float(frame->verts[i].v[1]) * frame->scale[1] + frame->translate[1] ) * interp_k + 
					( float(old_frame->verts[i].v[1]) * old_frame->scale[1] + old_frame->translate[1] ) * inv_iterp_k ;
			current_model_vertices[i].z= ( float(frame->verts[i].v[2]) * frame->scale[2] + frame->translate[2] ) * interp_k +
					( float(old_frame->verts[i].v[2]) * old_frame->scale[2] + old_frame->translate[2] ) * inv_iterp_k ;
		}
	}
	else
	{
		for( int i= 0; i< model->num_xyz; i++ )
		{
			current_model_vertices[i].x= ( float(frame->verts[i].v[0]) * frame->scale[0] + frame->translate[0] );
			current_model_vertices[i].y= ( float(frame->verts[i].v[1]) * frame->scale[1] + frame->translate[1] );
			current_model_vertices[i].z= ( float(frame->verts[i].v[2]) * frame->scale[2] + frame->translate[2] );
		}
	}

	//add effects, like QuadDamage or Invulerability
	if ( (ent->flags & (RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE|RF_SHELL_DOUBLE|RF_SHELL_HALF_DAM) ) != 0 )
	{
		dtrivertx_t* in_v= frame->verts;
		for( m_Vec3* v= current_model_vertices, *v_end= current_model_vertices + model->num_xyz;
			v< v_end; v++, in_v++ )
		{
			int normal_index= in_v->lightnormalindex;
			v->x+= r_avertexnormals[ normal_index ][0] * POWERSUIT_SCALE;
			v->y+= r_avertexnormals[ normal_index ][1] * POWERSUIT_SCALE;
			v->z+= r_avertexnormals[ normal_index ][2] * POWERSUIT_SCALE;
		}
	}
}

enum AliasModelVisibilityType
{
	ALIAS_MODEL_NOT_VISIBLE,
	ALIAS_MODEL_HIT_FRUSTRUM_EDGE,
	ALIAS_MODEL_IN_FRUSTRUM
};

AliasModelVisibilityType IsModelVisible( entity_t* ent )
{
	dmdl_t* model= (dmdl_t* )ent->model->extradata;
	int current_model_frame= ent->frame % model->num_frames;

	daliasframe_t* frame= (daliasframe_t*)( (char*)model + model->ofs_frames + current_model_frame * model->framesize );

	float center[3];//model space model center
	float radius= (0.5f * 255.0f ) * sqrtf( DotProduct( frame->scale, frame->scale ) );
	float neg_radius= -radius;

	for( int i= 0; i< 3; i++ )
		center[i]= (255.0f * 0.5f) * frame->scale[i] + frame->translate[i];

	bool is_on_frustrum_edge= false;
	for( int i= 0; i< 5; i++ )
	{
		mplane_t* plane= clip_planes + i;
		float dst_to_clip_plane= DotProduct( center, plane->normal ) - plane->dist;
		if( dst_to_clip_plane < neg_radius )
			return ALIAS_MODEL_NOT_VISIBLE;
		else if( dst_to_clip_plane < radius )
			is_on_frustrum_edge= true;
	}
	if( is_on_frustrum_edge )
		return ALIAS_MODEL_HIT_FRUSTRUM_EDGE;
	else 
		return ALIAS_MODEL_IN_FRUSTRUM;
}

void CalculateModelLights( entity_t* ent, m_Mat4* light_transform_matrix, m_Mat4* light_transform_normal_matrix )
{
	dmdl_t* model= (dmdl_t* )ent->model->extradata;
	int current_model_frame= ent->frame % model->num_frames;
	int prev_model_frame= ent->oldframe %model->num_frames;
	daliasframe_t* frame= (daliasframe_t*)( (char*)model + model->ofs_frames + current_model_frame * model->framesize );


	dlight_t model_lights[ MAX_DLIGHTS ];
	int model_dlight_num= r_newrefdef.num_dlights;
	if( model_dlight_num > MAX_DLIGHTS )
		model_dlight_num= MAX_DLIGHTS;

	//m_Mat4 light_transform_matrix;//transformation matrix world_space => model_space
	//m_Mat4 light_transform_normal_matrix;

	//CalcualteEntityInverseMatrix( ent, &light_transform_matrix, &light_transform_normal_matrix );
	for( int i= 0; i< model_dlight_num; i++ )
	{
		m_Vec3 light_pos( r_newrefdef.dlights[i].origin[0], r_newrefdef.dlights[i].origin[1], r_newrefdef.dlights[i].origin[2] );
		*((m_Vec3*)model_lights[i].origin)= light_pos * *light_transform_matrix;
		model_lights[i].intensity= r_newrefdef.dlights[i].intensity;
		model_lights[i].color[0]=  r_newrefdef.dlights[i].color[0];
		model_lights[i].color[1]=  r_newrefdef.dlights[i].color[1];
		model_lights[i].color[2]=  r_newrefdef.dlights[i].color[2];
		ColorFloatSwap( &model_lights[i].color[0] );
	}	

	if( (ent->flags&RF_GLOW) != 0 )
	{
		//add sinwave light pulsation 1 cycle per second
		unsigned char light= (unsigned char)( ( sin( r_newrefdef.time * 2.0f * 3.1415926535f ) + 3.0f ) * 22.0f );
		int i_light= light | (light<<8) | (light<<16) | (light<<24);
		for( int i= 0; i< model->num_xyz; i++ )
			current_model_vertex_lights[i].light= i_light;
	}
	else if( (ent->flags&(RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_GREEN)) != 0 )
	{
		unsigned char light[4]= { 8,8,8,8 };
		if( (ent->flags&RF_SHELL_RED) != 0 )
			light[0]= 255/3;
		if( (ent->flags&RF_SHELL_GREEN) != 0 )
			light[1]= 255/3;
		if( (ent->flags&RF_SHELL_BLUE) != 0 )
			light[2]= 255/3;
		ColorByteSwap(light);
		for( int i= 0; i< model->num_xyz; i++ )
			current_model_vertex_lights[i].light= *((int*)light);
	}
	else
	{
		float ambient_light[4];
		R_LightPoint ( ent->origin, ambient_light);
		float final_ambient_light[3];


		float min_light= ((ent->flags&RF_MINLIGHT) != 0) ? 16.0f : 8.0f;
		final_ambient_light[0]= ambient_light[0] * 255.0f;
		if( final_ambient_light[0] < min_light ) final_ambient_light[0]= min_light;
		final_ambient_light[1]= ambient_light[1] * 255.0f;
		if( final_ambient_light[1] < min_light ) final_ambient_light[1]= min_light;
		final_ambient_light[2]= ambient_light[2] * 255.0f;
		if( final_ambient_light[2] < min_light ) final_ambient_light[2]= min_light;

		const float light_scaler= 0.125f * 255.0f;

		for( int i= 0; i< model->num_xyz; i++ )
		{
			m_Vec3 normal(	r_avertexnormals[frame->verts[i].lightnormalindex][0],
							r_avertexnormals[frame->verts[i].lightnormalindex][1],
							r_avertexnormals[frame->verts[i].lightnormalindex][2] );

			float light_scale_k= ( normal.z + 2.0f ) * 0.333f;
			float l;
			l= light_scale_k * final_ambient_light[0];
			if( l > 255.0f ) l= 255.0f;
				current_model_vertex_lights[i].colored_light[0]= (unsigned char)l;
			l= light_scale_k * final_ambient_light[1];
			if( l > 255.0f ) l= 255.0f;
				current_model_vertex_lights[i].colored_light[1]= (unsigned char)l;
			l= light_scale_k * final_ambient_light[2];
			if( l > 255.0f ) l= 255.0f;
				current_model_vertex_lights[i].colored_light[2]= (unsigned char)l;

			/*current_model_vertex_lights[i].colored_light[0]= (unsigned char)(light_scale_k * final_ambient_light[0]);
			current_model_vertex_lights[i].colored_light[1]= (unsigned char)(light_scale_k * final_ambient_light[1]);
			current_model_vertex_lights[i].colored_light[2]= (unsigned char)(light_scale_k * final_ambient_light[2]);*/
		
			for( int l= 0; l< model_dlight_num; l++ )
			{
				m_Vec3 vec_to_light(
					model_lights[l].origin[0] - current_model_vertices[i].x,
					model_lights[l].origin[1] - current_model_vertices[i].y,
					model_lights[l].origin[2] - current_model_vertices[i].z );

				float dot= vec_to_light * normal;
				if( dot > 0.0f )
				{
					float dst2= vec_to_light * vec_to_light;
					float dst= sqrtf(dst2);
					float intensity= light_scaler * model_lights[l].intensity * dot / dst2;
					if( intensity > 1.0f )
					{
						int lights[3];
						lights[0]= int(intensity* model_lights[l].color[0]) + current_model_vertex_lights[i].colored_light[0];
						if( lights[0] > 255 ) lights[0]= 255;
						lights[1]= int(intensity* model_lights[l].color[1]) + current_model_vertex_lights[i].colored_light[1];
						if( lights[1] > 255 ) lights[1]= 255;
						lights[2]= int(intensity* model_lights[l].color[2]) + current_model_vertex_lights[i].colored_light[2];
						if( lights[2] > 255 ) lights[2]= 255;

						current_model_vertex_lights[i].colored_light[0]= lights[0];
						current_model_vertex_lights[i].colored_light[1]= lights[1];
						current_model_vertex_lights[i].colored_light[2]= lights[2];
					}//if light not to far
				}//if dot > 0 
			}//for lights
		}//for vertices


		//transformed parameters
		/*m_Vec3 player_flashlight_pos;
		m_Vec3 player_flashlight_dir;
		player_flashlight_pos= *((m_Vec3*)player_flashlight.origin) * light_transform_matrix;
		player_flashlight_dir= *((m_Vec3*)player_flashlight.direction) * light_transform_normal_matrix;
		player_flashlight_dir= player_flashlight_dir;

		float min_flashlight_cos= cosf( player_flashlight.cone_radius );

		for( int i= 0; i< model->num_xyz; i++ )
		{
			m_Vec3 normal(	r_avertexnormals[frame->verts[i].lightnormalindex][0],
							r_avertexnormals[frame->verts[i].lightnormalindex][1],
							r_avertexnormals[frame->verts[i].lightnormalindex][2] );

			m_Vec3 vec_to_light(
					player_flashlight_pos.x - current_model_vertices[i].x,
					player_flashlight_pos.y - current_model_vertices[i].y,
					player_flashlight_pos.z - current_model_vertices[i].z );

			float dot= vec_to_light * normal;

			float dst2= vec_to_light * vec_to_light;
			float dst= sqrtf(dst2);

			float flashlight_dir_dot= ( vec_to_light * player_flashlight_dir ) / dst;
			if(  flashlight_dir_dot < min_flashlight_cos )
				continue;
			if( dot > 0.0f )
			{
				float intensity= light_scaler * player_flashlight.intensity * dot / dst2;
				int lights[3];
				if(intensity>= 1.0f )
				{
					lights[0]= int(intensity* player_flashlight.color[0]) + current_model_vertex_lights[i].colored_light[0];
					if( lights[0] > 255 ) lights[0]= 255;
					lights[1]= int(intensity* player_flashlight.color[1]) + current_model_vertex_lights[i].colored_light[1];
					if( lights[1] > 255 ) lights[1]= 255;
					lights[2]= int(intensity* player_flashlight.color[2]) + current_model_vertex_lights[i].colored_light[2];
					if( lights[2] > 255 ) lights[2]= 255;

					current_model_vertex_lights[i].colored_light[0]= lights[0];
					current_model_vertex_lights[i].colored_light[1]= lights[1];
					current_model_vertex_lights[i].colored_light[2]= lights[2];
				}

			}//if dot > 0 
		}//flashlight*/

	}//if default model lighting
}

triangle_draw_func_t model_draw_funcs_table[]=
{
	DrawModelTriangleTextureNearestLightingColored,
	DrawModelTriangleTextureLinearLightingColored,
	DrawModelTriangleTextureFakeFilterLightingColored,
	DrawModelTriangleTextureNearestLightingColoredBlend,
	DrawModelTriangleTextureLinearLightingColoredBlend,
	DrawModelTriangleTextureFakeFilterLightingColoredBlend,
	//fullbright
	DrawModelTriangleTextureNearest,
	DrawModelTriangleTextureLinear,
	DrawModelTriangleTextureFakeFilter,
	DrawModelTriangleTextureNearestBlend,
	DrawModelTriangleTextureLinearBlend,
	DrawModelTriangleTextureFakeFilterBlend,
};

triangle_draw_func_t GetModelDrawFunc( entity_t* ent )
{
	int coord= 0;
	if( (ent->flags&RF_FULLBRIGHT) != 0 )
		coord+= 6;
	if( (ent->flags&RF_TRANSLUCENT) != 0 && ent->alpha > 0.05f )
		coord+= 3;
	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
		coord+= 1;
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
		coord+= 2;
	else
		coord+= 0;

	return model_draw_funcs_table[ coord ];
}

triangle_draw_func_t GetModelDrawFuncDepthHack()
{
	if( strcmp( r_texture_mode->string, "texture_linear" ) == 0 )
		return DrawModelTriangleTextureLinearLightingColoredDepthHack;
	else if( strcmp( r_texture_mode->string, "texture_fake_filter" ) == 0 )
		return DrawModelTriangleTextureFakeFilterLightingColoredDepthHack;
	else
		return DrawModelTriangleTextureNearestLightingColoredDepthHack;
}

void DrawAliasEntity( entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos )
{
	if( (ent->flags&RF_WEAPONMODEL) != 0 )
	{
		if( r_lefthand->value != 0 && r_lefthand->value != 1 )
			return;
	}

	dmdl_t* model= (dmdl_t* )ent->model->extradata;


	m_Mat4 entity_mat, result_mat, hand_mat, inverse_mat, normal_inverse_mat, clip_lanes_normal_mat;
	float cam_pos_model_space[3];
	CalculateEntityMatrix( ent, &entity_mat );
	CalcualteEntityInverseMatrix( ent, &inverse_mat, &normal_inverse_mat );

	*((m_Vec3*)cam_pos_model_space)= *((m_Vec3*)cam_pos) * inverse_mat;
	clip_lanes_normal_mat= *normal_mat * normal_inverse_mat;
	InitFrustrumClipPlanes( &clip_lanes_normal_mat, cam_pos_model_space );
	AliasModelVisibilityType model_visibility_type= IsModelVisible( ent );
	if( model_visibility_type == ALIAS_MODEL_NOT_VISIBLE )
		return;
	
	bool is_fullbright= (ent->flags&RF_FULLBRIGHT) != 0;
	CalculateModelVertices( ent );
	if( !is_fullbright )
		CalculateModelLights( ent, &inverse_mat, &normal_inverse_mat );

	result_mat= entity_mat * *mat;
	if( (ent->flags&RF_WEAPONMODEL) != 0 )
	{
		//make weapoon model larger
		for( int i= 0; i< model->num_xyz; i++ )
			current_model_vertices[i]*= 2.0f;
		if( r_lefthand->value == 1 )
		{
			hand_mat.Identity();
			hand_mat[0]*= -1.0f;
			result_mat= result_mat * hand_mat;
		}
	}

	dtriangle_t* tris= (dtriangle_t*)( (char*)model + model->ofs_tris );
	dstvert_t* st= (dstvert_t*)( (char*)model + model->ofs_st );

	char* buff= (char*) command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;

	if( (ent->flags&RF_TRANSLUCENT) != 0 && ent->alpha > 0.05f )
		buff+= ComIn_SetConstantBlendFactor( buff, int(ent->alpha*245.0f) );

	Texture* tex;
	int current_texture_frame;
	if( model->num_skins != 0 )
	{
		current_texture_frame= ent->skinnum % model->num_skins;
		tex= R_FindTexture( ent->model->skins[current_texture_frame] );
	}
	else
		tex= R_FindTexture( ent->skin );//for player skins
	
	buff+= ComIn_SetTexture( buff, tex );
	int tex_y_shift= (tex->OriginalSizeY() - tex->SizeY())<<16;

	*((int*)buff)= COMMAND_DRAW_TRIANGLE;
	buff+= sizeof(int);

	DrawTriangleCall* call= (DrawTriangleCall*)buff;
	call->triangle_count= 0;
	call->vertex_size= is_fullbright ? ( sizeof(int)*3 + sizeof(int)*2 ): ( sizeof(int)*3 + sizeof(int)*2 + 4);
	call->DrawFromBufferFunc= ((ent->flags&RF_DEPTHHACK)==0) ? GetModelDrawFunc( ent ) : GetModelDrawFuncDepthHack();
	buff+= sizeof(DrawTriangleCall);

	if( model_visibility_type == ALIAS_MODEL_IN_FRUSTRUM )
	{
		for( m_Vec3* v= current_model_vertices, *v_end= current_model_vertices + model->num_xyz;
			v< v_end; v++ )
		{
			*v= *v * result_mat;
			float inv_z= 1.0f/ v->z;
			v->z*= 65536.0f;
			v->x= ( v->x * inv_z + vertex_projection.x_add ) * vertex_projection.x_mul;
			v->y= ( v->y * inv_z + vertex_projection.y_add ) * vertex_projection.y_mul;
		}
#define  Z_MIN_SCALED float(PSR_MIN_ZMIN)
		for( int t= 0; t< model->num_tris; t++, tris++ )
		{
			if( current_model_vertices[ tris->index_xyz[0] ].z <= Z_MIN_SCALED ||
				current_model_vertices[ tris->index_xyz[1] ].z <= Z_MIN_SCALED ||
				current_model_vertices[ tris->index_xyz[2] ].z <= Z_MIN_SCALED )
				continue;

			//bak face culling
			float v[4]= {	current_model_vertices[ tris->index_xyz[2] ].x - current_model_vertices[ tris->index_xyz[1] ].x,
							current_model_vertices[ tris->index_xyz[2] ].y - current_model_vertices[ tris->index_xyz[1] ].y,
							current_model_vertices[ tris->index_xyz[1] ].x - current_model_vertices[ tris->index_xyz[0] ].x,
							current_model_vertices[ tris->index_xyz[1] ].y - current_model_vertices[ tris->index_xyz[0] ].y };
			if( v[0] * v[3]  - v[2] * v[1] < 1.0f )
				continue;
			
			using namespace VertexProcessing;

			triangle_in_vertex_xy[0]= fixed16_t(current_model_vertices[ tris->index_xyz[0] ].x);
			triangle_in_vertex_xy[1]= fixed16_t(current_model_vertices[ tris->index_xyz[0] ].y);
			triangle_in_vertex_xy[2]= fixed16_t(current_model_vertices[ tris->index_xyz[1] ].x);
			triangle_in_vertex_xy[3]= fixed16_t(current_model_vertices[ tris->index_xyz[1] ].y);
			triangle_in_vertex_xy[4]= fixed16_t(current_model_vertices[ tris->index_xyz[2] ].x);
			triangle_in_vertex_xy[5]= fixed16_t(current_model_vertices[ tris->index_xyz[2] ].y);
			triangle_in_vertex_z[0]= fixed16_t(current_model_vertices[ tris->index_xyz[0] ].z);
			triangle_in_vertex_z[1]= fixed16_t(current_model_vertices[ tris->index_xyz[1] ].z);
			triangle_in_vertex_z[2]= fixed16_t(current_model_vertices[ tris->index_xyz[2] ].z);

			triangle_in_tex_coord[0]= st[tris->index_st[0]].s<<16;
			triangle_in_tex_coord[1]= tex_y_shift -(st[tris->index_st[0]].t<<16);
			triangle_in_tex_coord[2]= st[tris->index_st[1]].s<<16;
			triangle_in_tex_coord[3]= tex_y_shift -(st[tris->index_st[1]].t<<16);
			triangle_in_tex_coord[4]= st[tris->index_st[2]].s<<16;
			triangle_in_tex_coord[5]= tex_y_shift -(st[tris->index_st[2]].t<<16);
			*((int*)triangle_in_color)= current_model_vertex_lights[ tris->index_xyz[0] ].light;
			*((int*)triangle_in_color+1)= current_model_vertex_lights[ tris->index_xyz[1] ].light;
			*((int*)triangle_in_color+2)= current_model_vertex_lights[ tris->index_xyz[2] ].light;

			int draw_result;
			if(is_fullbright)
				draw_result= DrawFullbrightTexturedModelTriangleToBuffer(buff);
			else
				draw_result= DrawTexturedModelTriangleToBuffer( buff );
			if( draw_result != 0 )
			{
				buff+= 4 * call->vertex_size;
				call->triangle_count++;
			}
		}//for triangles
	}//if model fully in frustrum
	else
	{
		for( int p= 0; p< model->num_tris; p++, tris++ )
		{
			int vertex_count= ClipModelTriangle( tris, st );
			if( vertex_count == 0 )
				continue;
			for( int t= 0; t< vertex_count-2; t++ )
			{
				m_Vec3 coord[3];//screen space coord
				coord[0]= *((m_Vec3*)current_model_polygon[0  ]->position) * result_mat;
				coord[1]= *((m_Vec3*)current_model_polygon[t+1]->position) * result_mat;
				coord[2]= *((m_Vec3*)current_model_polygon[t+2]->position) * result_mat;
				if( coord[0].z < PSR_MIN_ZMIN_FLOAT || coord[1].z < PSR_MIN_ZMIN_FLOAT || coord[2].z < PSR_MIN_ZMIN_FLOAT )
					continue;

				for( int i= 0; i< 3; i++ )
				{
					float inv_z= 1.0f/ coord[i].z;
					coord[i].x= ( coord[i].x * inv_z + vertex_projection.x_add ) * vertex_projection.x_mul;
					coord[i].y= ( coord[i].y * inv_z + vertex_projection.y_add ) * vertex_projection.y_mul;
					coord[i].z*= 65536.0f;
				}
				//bak face culling
				float v[4]= { coord[2].x - coord[1].x, coord[2].y - coord[1].y, coord[1].x - coord[0].x, coord[1].y - coord[0].y };
				if( v[0] * v[3]  - v[2] * v[1] < 1.0f )
					continue;

				using namespace VertexProcessing;
				triangle_in_vertex_xy[0]= fixed16_t(coord[0].x);
				triangle_in_vertex_xy[1]= fixed16_t(coord[0].y);
				triangle_in_vertex_xy[2]= fixed16_t(coord[1].x);
				triangle_in_vertex_xy[3]= fixed16_t(coord[1].y);
				triangle_in_vertex_xy[4]= fixed16_t(coord[2].x);
				triangle_in_vertex_xy[5]= fixed16_t(coord[2].y);
				triangle_in_vertex_z[0]= fixed16_t(coord[0].z);
				triangle_in_vertex_z[1]= fixed16_t(coord[1].z);
				triangle_in_vertex_z[2]= fixed16_t(coord[2].z);
				triangle_in_tex_coord[0]= fixed16_t(current_model_polygon[0]->st[0]);
				triangle_in_tex_coord[1]= tex_y_shift - fixed16_t(current_model_polygon[0]->st[1]);
				triangle_in_tex_coord[2]= fixed16_t(current_model_polygon[t+1]->st[0]);
				triangle_in_tex_coord[3]= tex_y_shift - fixed16_t(current_model_polygon[t+1]->st[1]);
				triangle_in_tex_coord[4]= fixed16_t(current_model_polygon[t+2]->st[0]);
				triangle_in_tex_coord[5]= tex_y_shift - fixed16_t(current_model_polygon[t+2]->st[1]);
				if( !is_fullbright )
				{
					triangle_in_color[0 ]= (unsigned char)( current_model_polygon[0]->light[0] );
					triangle_in_color[1 ]= (unsigned char)( current_model_polygon[0]->light[1] );
					triangle_in_color[2 ]= (unsigned char)( current_model_polygon[0]->light[2] );
					triangle_in_color[4 ]= (unsigned char)( current_model_polygon[t+1]->light[0] );
					triangle_in_color[5 ]= (unsigned char)( current_model_polygon[t+1]->light[1] );
					triangle_in_color[6 ]= (unsigned char)( current_model_polygon[t+1]->light[2] );
					triangle_in_color[8 ]= (unsigned char)( current_model_polygon[t+2]->light[0] );
					triangle_in_color[9 ]= (unsigned char)( current_model_polygon[t+2]->light[1] );
					triangle_in_color[10]= (unsigned char)( current_model_polygon[t+2]->light[2] );	
				}

				int draw_result;
				if(is_fullbright)
					draw_result= DrawFullbrightTexturedModelTriangleToBuffer(buff);
				else
					draw_result= DrawTexturedModelTriangleToBuffer( buff );
				if( draw_result != 0 )
				{
					buff+= 4 * call->vertex_size;
					call->triangle_count++;
				}

			}//for triangle in polygon
		}//for initial model triangles
	}//if need cull model triangles

	command_buffer.current_pos+= buff- buff0;

}



#define MAX_BEAM_VERTICES 96
#define MAX_BEAM_TRIANGLES 96
static float beam_mesh[ MAX_BEAM_VERTICES ];
static int beam_indeces[ MAX_BEAM_TRIANGLES ];
static int beam_mesh_triangle_count;


void GenerateBeamMesh( entity_t* ent )
{
	m_Vec3 beam_vec( ent->origin[0] - ent->oldorigin[0], ent->origin[1] - ent->oldorigin[1], ent->origin[2] - ent->oldorigin[2] );

	int largest_component= 0;
	float larges_component_len= fabsf(beam_vec.x);
	if(larges_component_len < fabsf(beam_vec.y) )
	{
		larges_component_len= fabsf(beam_vec.y);
		largest_component= 1;
	}
	if(larges_component_len < fabsf(beam_vec.z) )
		largest_component= 2;

	
	const int beam_cylinder_edges= 10;
	float cylinder_radius= float(ent->frame) * 0.5f;
	float direction_sign= 1.0f;
	if( largest_component == 0 )
	{
		if( ent->origin[0] > ent->oldorigin[0] )
			direction_sign= -1.0f;
		for( int i= 0, j=0; i< beam_cylinder_edges; i++, j+=6 )
		{
			float a= 2.0f * 3.1415926535f * float(i)/float(beam_cylinder_edges);
			beam_mesh[j  ]= ent->origin[0];
			beam_mesh[j+1]= ent->origin[1] + cosf(a)*cylinder_radius;
			beam_mesh[j+2]= ent->origin[2] + sinf(a)*cylinder_radius * direction_sign;
			beam_mesh[j+3]= ent->oldorigin[0];
			beam_mesh[j+4]= ent->oldorigin[1] + cosf(a)*cylinder_radius;
			beam_mesh[j+5]= ent->oldorigin[2] + sinf(a)*cylinder_radius * direction_sign;
		}
	}
	else if( largest_component == 1 )
	{
		if( ent->origin[1] < ent->oldorigin[1] )
			direction_sign= -1.0f;
		for( int i= 0, j=0; i< beam_cylinder_edges; i++, j+=6 )
		{
			float a= 2.0f * 3.1415926535f * float(i)/float(beam_cylinder_edges);
			beam_mesh[j  ]= ent->origin[0] + cosf(a)*cylinder_radius;
			beam_mesh[j+1]= ent->origin[1];
			beam_mesh[j+2]= ent->origin[2] + sinf(a)*cylinder_radius * direction_sign;
			beam_mesh[j+3]= ent->oldorigin[0] + cosf(a)*cylinder_radius;
			beam_mesh[j+4]= ent->oldorigin[1];
			beam_mesh[j+5]= ent->oldorigin[2] + sinf(a)*cylinder_radius * direction_sign;
		}
	}
	else
	{
		if( ent->origin[2] < ent->oldorigin[2] )
			direction_sign= -1.0f;
		for( int i= 0, j=0; i< beam_cylinder_edges; i++, j+=6 )
		{
			float a= 2.0f * 3.1415926535f * float(i)/float(beam_cylinder_edges);
			beam_mesh[j  ]= ent->origin[0] + sinf(a)*cylinder_radius;
			beam_mesh[j+1]= ent->origin[1] + cosf(a)*cylinder_radius * direction_sign;
			beam_mesh[j+2]= ent->origin[2];
			beam_mesh[j+3]= ent->oldorigin[0] + sinf(a)*cylinder_radius;
			beam_mesh[j+4]= ent->oldorigin[1] + cosf(a)*cylinder_radius * direction_sign;
			beam_mesh[j+5]= ent->oldorigin[2];
		}
	}
	beam_mesh_triangle_count= beam_cylinder_edges * 2;

	for( int i= 0, j=0; i< beam_cylinder_edges; i++, j+=6 )
	{
		beam_indeces[j+0]= i*2+3;
		beam_indeces[j+1]= i*2+2;
		beam_indeces[j+2]= i*2+0;
		beam_indeces[j+3]= i*2+1;
		beam_indeces[j+4]= i*2+3;
		beam_indeces[j+5]= i*2+0;
	}
	int j= beam_cylinder_edges*6-6;
	beam_indeces[j+0]= 1;
	beam_indeces[j+1]= 0;
	//beam_indeces[j+2]= i*2+0;
	//beam_indeces[j+3]= i*2+1;
	beam_indeces[j+4]= 1;
	//beam_indeces[j+5]= i*2+0;
}

int ClipBeamTriangle( int* triangle_indeces )//returns number of output vertices
{
	model_tmp_vertices_stack_pos= 3;
	for( int i= 0; i< 3; i++ )//generate initial polygon from triangle
	{
		current_model_polygon_vertices[i].position[0]= beam_mesh[ triangle_indeces[i]*3   ];
		current_model_polygon_vertices[i].position[1]= beam_mesh[ triangle_indeces[i]*3+1 ];
		current_model_polygon_vertices[i].position[2]= beam_mesh[ triangle_indeces[i]*3+2 ];
		current_model_polygon[i]= current_model_polygon_vertices + i;
	}
	int vertex_count= 3;

	for( int i= 0; i< 5; i++ )
	{
		vertex_count= ClipModelPolygonByPlane( vertex_count, clip_planes + i );
		if( vertex_count == 0 )
			return 0;
	}
	return vertex_count;
}

void DrawBeam( entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos )
{
	InitFrustrumClipPlanes( normal_mat, cam_pos );

	GenerateBeamMesh(ent);
	
	char* buff= (char*) command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;

	unsigned char color[4];
	*((int*)color)= d_8to24table[ ent->skinnum&255 ];
	buff+= ComIn_SetConstantColor( buff, color );

	*((int*)buff)= COMMAND_DRAW_TRIANGLE;
	buff+= sizeof(int);
	DrawTriangleCall* call= (DrawTriangleCall*)buff;
	call->DrawFromBufferFunc= DrawBeamTriangle;
	call->triangle_count= 0;
	call->vertex_size= sizeof(int)*3;
	buff+= sizeof(DrawTriangleCall);

	for( int i= 0; i< beam_mesh_triangle_count; i++ )
	{
		int vertex_count= ClipBeamTriangle( beam_indeces + i*3 );
		if( vertex_count == 0 )
			continue;
		for( int t= 0; t< vertex_count - 2; t++ )
		{
			m_Vec3 pos[3];
			pos[0]= *((m_Vec3*)current_model_polygon[0  ]->position) * *mat;
			pos[1]= *((m_Vec3*)current_model_polygon[t+1]->position) * *mat;
			pos[2]= *((m_Vec3*)current_model_polygon[t+2]->position) * *mat;

			if( pos[0].z <= PSR_MIN_ZMIN_FLOAT || pos[1].z <= PSR_MIN_ZMIN_FLOAT || pos[2].z <= PSR_MIN_ZMIN_FLOAT )
			continue;

			for(int j= 0; j< 3; j++ )
			{
				float inv_z= 1.0f / pos[j].z;
				pos[j].x= ( pos[j].x * inv_z + vertex_projection.x_add ) * vertex_projection.x_mul;
				pos[j].y= ( pos[j].y * inv_z + vertex_projection.y_add ) * vertex_projection.y_mul;
			}

			//bak face culling
			float v[4]= { pos[2].x - pos[1].x, pos[2].y - pos[1].y, pos[1].x - pos[0].x, pos[1].y - pos[0].y };
			if( v[0] * v[3] - v[2] * v[1] < 1.0f )
				continue;

			using namespace VertexProcessing;
			triangle_in_vertex_xy[0]= fixed16_t(pos[0].x);
			triangle_in_vertex_xy[1]= fixed16_t(pos[0].y);
			triangle_in_vertex_xy[2]= fixed16_t(pos[1].x);
			triangle_in_vertex_xy[3]= fixed16_t(pos[1].y);
			triangle_in_vertex_xy[4]= fixed16_t(pos[2].x);
			triangle_in_vertex_xy[5]= fixed16_t(pos[2].y);
			triangle_in_vertex_z[0]= fixed16_t(pos[0].z*65536.0f);
			triangle_in_vertex_z[1]= fixed16_t(pos[1].z*65536.0f);
			triangle_in_vertex_z[2]= fixed16_t(pos[2].z*65536.0f);

			if( DrawBeamTriangleToBuffer(buff)!= 0 )
			{
				buff+= 4 * call->vertex_size;
				call->triangle_count++;
			}

		}//for clipped triangles
	}//for original beam mesh triangles

	command_buffer.current_pos+= buff - buff0;
}