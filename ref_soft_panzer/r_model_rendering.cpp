#include "rendering.h"
#include "r_world_rendering.h"

#include "panzer_rast/math_lib/matrix.h"
#include "panzer_rast/rendering_state.h"


extern Texture* R_FindTexture( char* name );
extern Texture* R_FindTexture(image_t* image);
extern int R_GetImageIndex(image_t* image);

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

	float to_rad= 3.1415926535f / 180.0f;
	m_Mat4 rot_x, rot_y, rot_z, translate, result;
	translate.Identity();
	m_Vec3 v( ent->origin[0], ent->origin[1], ent->origin[2] );
	translate.Translate( v );

	rot_x.RotateX( -ent->angles[2] * to_rad );//check
	rot_z.RotateZ( -ent->angles[1] * to_rad );
	rot_y.RotateY( ent->angles[0] * to_rad );


	result= rot_y * rot_x * rot_z * translate * *mat;
	SetSurfaceMatrix(&result);

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

void DrawAliasEntity(  entity_t* ent, m_Mat4* mat, vec3_t cam_pos )
{
}