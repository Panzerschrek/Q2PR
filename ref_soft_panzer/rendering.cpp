#include "r_world_rendering.h"


#include "panzer_rast/math_lib/matrix.h"
#include "panzer_rast/rendering_state.h"


extern Texture* R_FindTexture( char* name );
extern Texture* R_FindTexture(image_t* image);
extern int R_GetImageIndex(image_t* image);

extern "C" void LoadTGA (char *name, byte **pic, int *width, int *height);
extern "C" struct image_s *PANZER_RegisterPic(char *name);
extern "C" void PANZER_DrawPic(int x, int y, char *name);

extern "C" swstate_t sw_state;


void DrawSpriteEntity( entity_t* ent, m_Mat4* mat, vec3_t cam_pos );
void DrawBrushEntity( entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos, bool is_aplha );
void DrawAliasEntity(  entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos );
void DrawBeam( entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos );
/*
----command buffer-------
*/
CommandBuffer command_buffer;//for vertex processing
CommandBuffer back_command_buffer;//for rasterization
qmutex_t frontend_framebuffer_mutex;
//qmutex_t backend_framebuffer_mutex;
qmutex_t frontend_front_command_buffer_mutex;
//qmutex_t backend_front_command_buffer_mutex;
#define FRAMEBUFFER_MUTEX_NAME		"q2_panzer_soft_rast_framebuffer_mutex"
#define COMMAND_BUFFER_MUTEX_NAME	"q2_panzer_soft_rast_front_command_buffer_mutex" 

bool need_terminate_backend_thread= true;
bool use_multithreading= false;
qthread_t backend_thread;


/*
----char buffer-------
*/
#define MAX_BUFFER_CHARS 8192
struct CharBuffer
{
	unsigned char char_buffer[MAX_BUFFER_CHARS];
	short char_buffer_coords[MAX_BUFFER_CHARS*2];
	int char_count;
};

CharBuffer* front_chars;
CharBuffer* back_chars;
Texture* console_tex= NULL;
void DrawCharString( int x, int y, const char* str );





/*
----------fps calculating---------
*/
struct fps_calc_t
{
	int last_fps_time;
	int last_frame_time;
	int last_fps;
	int frames;

	unsigned long long int long_frame_begin_time;
	unsigned long long int long_frontend_end_time;
	unsigned long long int long_frame_end_time;

	int total_frames;
	int start_time;
};
fps_calc_t fps_calc;
void CalculateAndShowFPS();



/*
-----------particles------------
*/
Texture particle_texture;
image_t particle_image;
void InitParticles();

/*
-----------matrices and vectors------------
*/
m_Mat4 world_final_matrix;
m_Mat4 world_normals_matrix;


//draw chars directly to front buffer
void DrawChars()
{
	image_t* img= PANZER_RegisterPic( "conchars" );
	console_tex= R_FindTexture(img);

	unsigned char* buff= PRast_GetFrontBuffer();

	for( int i= 0; i< back_chars->char_count; i++ )
	{
		if( back_chars->char_buffer_coords[i*2+1] >= vid.height - 8 || back_chars->char_buffer_coords[i*2+1] < 0 )
			continue;

		unsigned char* dst= buff
			+ (back_chars->char_buffer_coords[i*2]
			+  back_chars->char_buffer_coords[i*2+1]*vid.width )*4;

		unsigned int row= 15 - (back_chars->char_buffer[i]>>4);
		unsigned int col= back_chars->char_buffer[i]&15;
		const unsigned char* src= console_tex->GetData() + (col<<(3+2)) + (row<<(3+7+2));
		
		for( int y= 0; y< 8; y++, dst+=vid.width<<2, src+=128<<2)
		{
			if( src[0 +3] < 130 )
				((int*)dst)[0]= ((int*)src)[0];
			if( src[4 +3] < 130 )
				((int*)dst)[1]= ((int*)src)[1];
			if( src[8 +3] < 130 )
				((int*)dst)[2]= ((int*)src)[2];
			if( src[12+3] < 130 )
				((int*)dst)[3]= ((int*)src)[3];
			if( src[16+3] < 130 )
				((int*)dst)[4]= ((int*)src)[4];
			if( src[20+3] < 130 )
				((int*)dst)[5]= ((int*)src)[5];
			if( src[24+3] < 130 )
				((int*)dst)[6]= ((int*)src)[6];
			if( src[28+3] < 130 )
				((int*)dst)[7]= ((int*)src)[7];
		}
	}

	//back_chars->char_count= 0;
}


extern "C" unsigned int __stdcall PR_MainBackEndFunc( void* unused_param )
{
	//front_command_buffer_mutex= Sys_OpenMutex( COMMAND_BUFFER_MUTEX_NAME );
	//framebuffer_mutex= Sys_OpenMutex( FRAMEBUFFER_MUTEX_NAME );
	while(1)
	{
		Sys_MutexLock( &frontend_framebuffer_mutex );
		ComOut_DoCommands( back_command_buffer.buffer, back_command_buffer.current_pos );
		Sys_MutexUnlock( &frontend_framebuffer_mutex );

		//Sys_MutexLock( &frontend_front_command_buffer_mutex );
		//wait for command buffer swapping
		//Sys_MutexUnlock( &frontend_front_command_buffer_mutex );
	}
	Sys_ExitThread();
	return 0;
}
extern "C" void PR_ResetTimers()
{
	fps_calc.total_frames= 0;
	fps_calc.start_time= clock();
}

extern "C" void PR_InitRendering()
{
	use_multithreading = r_use_multithreading->value != 0;

	if( use_multithreading )
	{
		Sys_CreateMutex( &frontend_front_command_buffer_mutex );
		Sys_CreateMutex( &frontend_framebuffer_mutex );
	}
	memset(&backend_thread, sizeof(qthread_t), 0 );

	memset( &fps_calc, 0, sizeof(fps_calc_t) );

	//front_chars= (CharBuffer*) malloc( sizeof(CharBuffer) );
	//back_chars= (CharBuffer*) malloc( sizeof(CharBuffer) );
	front_chars= new CharBuffer;
	back_chars= new CharBuffer;

	front_chars->char_count= 0;
	back_chars->char_count= 0;

	command_buffer.buffer= Com_ResizeCommandBuffer( NULL, 0, 1024*1024*3, &command_buffer.size );
	back_command_buffer.buffer= Com_ResizeCommandBuffer( NULL, 0, 1024*1024*3, &back_command_buffer.size );

	command_buffer.current_pos= 
		back_command_buffer.current_pos= 0;

	InitParticles();
}
extern "C" void PR_ShutdownRendering()
{

	if(use_multithreading)
	{
		Sys_DestroyThread(backend_thread);
		Sys_DestroyMutex(&frontend_framebuffer_mutex);
	//	Sys_DestroyMutex(&backend_framebuffer_mutex);
		Sys_DestroyMutex(&frontend_front_command_buffer_mutex);
	//	Sys_DestroyMutex(&backend_front_command_buffer_mutex);

		/*backend_thread.thread_handle= 0;
		backend_thread.thread_id= 0;
		frontend_framebuffer_mutex.mutex_handle= 0;
		backend_framebuffer_mutex.mutex_handle= 0;
		frontend_front_command_buffer_mutex.mutex_handle= 0;
		backend_front_command_buffer_mutex.mutex_handle= 0;*/

	}
	delete[] command_buffer.buffer;
	delete[] back_command_buffer.buffer;

	delete front_chars;
	delete back_chars;
}

extern "C" void PR_LockFramebuffer()
{
	if(use_multithreading)
	{
		Sys_MutexLock( &frontend_framebuffer_mutex );
	}
}

extern "C" void PR_UnlockFramebuffer()
{
	if(use_multithreading)
	{
		Sys_MutexUnlock( &frontend_framebuffer_mutex );
	}
}
extern "C" void PR_SetFrameBuffeer( unsigned char* buff )
{
	PRast_SetFramebuffer(buff);
}

extern "C" void PR_SwapCommandBuffers()
{
	R_SwapLightmapBuffers();

	char statistic_str[128];
	sprintf( statistic_str, "command buffer size: %dkb\nparticles: %d", command_buffer.current_pos>>10, r_newrefdef.num_particles );
	DrawCharString( 8, 16, statistic_str );

	//draw chars
	command_buffer.current_pos+= ComIn_AddUserDefinedFunc( 
			(char*)command_buffer.buffer + command_buffer.current_pos, DrawChars );
	//swap color channels
	/*command_buffer.current_pos+= ComIn_SwapRedBlueInColorBuffer( 
				(char*)command_buffer.buffer + command_buffer.current_pos );*/

	//gamma correction
	if( ( vid_gamma->value < 0.95f || vid_gamma->value > 1.05f ) && !sw_state.hw_gamma_supported )
	{
		command_buffer.current_pos+= ComIn_GammaCorrectColorBuffer( 
			(char*)command_buffer.buffer + command_buffer.current_pos, sw_state.gammatable );
	}
	//mirror framebuffer vertical
	if( vid_fullscreen->value )
	{
		command_buffer.current_pos+= ComIn_MirrorVerticalColorBuffer( 
			(char*)command_buffer.buffer + command_buffer.current_pos );
	}

	CalculateAndShowFPS();

	if( use_multithreading )
	{
		Sys_MutexLock( &frontend_framebuffer_mutex );
		CommandBuffer tmp_com_buf= command_buffer;
		command_buffer= back_command_buffer;
		back_command_buffer= tmp_com_buf;

		//swap char buffers
		CharBuffer* tmp= front_chars;
		front_chars= back_chars;
		back_chars= tmp;
		front_chars->char_count= 0;
	}
	else
	{
		CharBuffer* tmp= front_chars;
		front_chars= back_chars;
		back_chars= tmp;
		front_chars->char_count= 0;
		ComOut_DoCommands( command_buffer.buffer, command_buffer.current_pos );
	}


	command_buffer.current_pos= 0;
}

extern "C" void PR_EndBufferSwapping()
{
	if(use_multithreading)
	{Sys_MutexUnlock( &frontend_framebuffer_mutex );
		Sys_MutexUnlock( &frontend_front_command_buffer_mutex );
		
		if( backend_thread.thread_handle == 0 )
			backend_thread= Sys_CreateThread( PR_MainBackEndFunc, NULL );
	}
}

extern "C" void PR_BeginFrame()
{
	if(use_multithreading)
	{
		Sys_MutexLock( &frontend_front_command_buffer_mutex );
	}
}

extern "C" void PANZER_DrawStretchPic(int x, int y, int w, int h, char *name)
{	
	int call_size= 48;
	if( command_buffer.current_pos + call_size > command_buffer.size )
		command_buffer.buffer=
		Com_ResizeCommandBuffer( command_buffer.buffer,
								command_buffer.current_pos,
								command_buffer.current_pos + call_size,
								&command_buffer.size );

	image_t* img= PANZER_RegisterPic( name );
	if( img == NULL )
		return;
	char	fullname[MAX_QPATH];
	if (name[0] != '/' && name[0] != '\\' )
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
	else
		strcpy(fullname, name+1);

	Texture* tex= R_FindTexture( img );
	command_buffer.current_pos+= ComIn_SetTexture( (char*)command_buffer.buffer + command_buffer.current_pos, tex );
	
	int* b2= (int*)((char*)command_buffer.buffer + command_buffer.current_pos );
	b2[0]= COMMAND_DRAW_SPRITE;
	command_buffer.current_pos+= sizeof(int);

	DrawSpriteCall* call= (DrawSpriteCall*) ( (char*)command_buffer.buffer + command_buffer.current_pos );
	if( img->transparent )
		call->DrawSpriteFunc= DrawStretchTextureRGBA;
	else 
		call->DrawSpriteFunc= DrawStretchTextureRGBANoAlphaTest;
	call->sprite_count= 1;
	command_buffer.current_pos+= sizeof(DrawSpriteCall);
	int* buff= (int*)((char*)command_buffer.buffer +  command_buffer.current_pos );
	buff[0]= x;
	buff[3]= vid.height - y;
	buff[2]= x + w;
	buff[1]= vid.height - ( y + h );
	buff[4]= 0;
	command_buffer.current_pos+= sizeof(int)*5;

}



extern "C" void PANZER_DrawPic(int x, int y, char *name)
{
	int call_size= 48;
	if( command_buffer.current_pos + call_size > command_buffer.size )
		command_buffer.buffer=
		Com_ResizeCommandBuffer( command_buffer.buffer,
								command_buffer.current_pos,
								command_buffer.current_pos + call_size,
								&command_buffer.size );

	image_t* img= PANZER_RegisterPic( name );
	if( img == NULL )
		return;
	char	fullname[MAX_QPATH];
	if ( name[0] != '/' && name[0] != '\\' )
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
	else
		strcpy(fullname, name+1);

	Texture* tex= R_FindTexture( img );
	command_buffer.current_pos+= ComIn_SetTexture( (char*)command_buffer.buffer + command_buffer.current_pos, tex );
	
	int* b2= (int*)((char*)command_buffer.buffer + command_buffer.current_pos );
	b2[0]= COMMAND_DRAW_SPRITE;
	command_buffer.current_pos+= sizeof(int);

	DrawSpriteCall* call= (DrawSpriteCall*) ( (char*)command_buffer.buffer + command_buffer.current_pos );
	if( img->transparent )
		call->DrawSpriteFunc= DrawStretchTextureRGBA;
	else 
		call->DrawSpriteFunc= DrawStretchTextureRGBANoAlphaTest;
	call->sprite_count= 1;
	command_buffer.current_pos+= sizeof(DrawSpriteCall);
	int* buff= (int*)((char*)command_buffer.buffer +  command_buffer.current_pos );
	buff[0]= x;
	buff[3]= vid.height - y;
	buff[2]= x + tex->OriginalSizeX();
	buff[1]= vid.height - ( y + tex->OriginalSizeY());
	buff[4]= 0;
	command_buffer.current_pos+= sizeof(int)*5;

}

extern "C" void PANZER_DrawFadeScreen()
{
	int* buff= (int*) ((char*)command_buffer.buffer + command_buffer.current_pos );
	buff[0]= COMMAND_FADE_COLOR_BUFFER;

	buff++;
	DrawFadeScreenCall* call= (DrawFadeScreenCall*)buff;
	call->func= FadeScreen;

	command_buffer.current_pos+= sizeof(int) + sizeof(DrawFadeScreenCall);
}

extern "C" void PANZER_DrawChar(int x, int y, int c)
{
	if(c == 32 || c == 32+128)
		return;
	if( front_chars->char_count >= MAX_BUFFER_CHARS )
		return;
	int i= front_chars->char_count;
	c&= 255;
	front_chars->char_buffer[i]= c;
	front_chars->char_buffer_coords[i*2  ]= x;
	front_chars->char_buffer_coords[i*2+1]= vid.height - y - 8;
	front_chars->char_count++;
}

void DrawCharString( int x, int y, const char* str )
{
	int x0 = x;
	while( *str )
	{
		if(*str == '\n')
		{
			x= x0;
			y+= 8;
			str++;
		}
		else
			PANZER_DrawChar( x+=8, y, *(str++) );
	}
}

long long unsigned int GetProcessorTickCount()
{
	unsigned long long int result;
	unsigned long long int* result_ptr= &result;
	__asm
	{
		mov ecx, result_ptr
		rdtsc
		mov dword ptr[ecx], eax
		mov dword ptr[ecx+4], edx
	}
	return result;
}


void CalculateAndShowFPS()
{
	int current_time= clock();

	if(fps_calc.start_time == 0 )
		fps_calc.start_time= current_time;

	int dt= current_time - fps_calc.last_frame_time;
	if( current_time - fps_calc.last_fps_time > CLOCKS_PER_SEC )
	{
		fps_calc.last_fps_time= current_time;
		fps_calc.last_fps= fps_calc.frames;
		fps_calc.frames= 0;
	}
	fps_calc.last_frame_time= current_time;
	fps_calc.total_frames++;


	/*fps_calc.long_frame_end_time= GetProcessorTickCount();
	unsigned long long int all_frame_dt= fps_calc.long_frame_end_time - fps_calc.long_frame_begin_time;
	unsigned long long int frontend_dt= fps_calc.long_frontend_end_time - fps_calc.long_frame_begin_time;
	unsigned long long int div= (frontend_dt<<10)/all_frame_dt;*/
	int div= 512;

	//fps_calc.long_frame_begin_time= fps_calc.long_frame_end_time;



	char fps_str[128];
	sprintf( fps_str, "fps: %d\n%dms\ntotal: %2.3f\n",
		fps_calc.last_fps, dt * 1000 / CLOCKS_PER_SEC,
		(current_time == fps_calc.start_time) ? 60 : (float(CLOCKS_PER_SEC*fps_calc.total_frames)/float(current_time-fps_calc.start_time))
		);
	DrawCharString( vid.width - 14 * 8, 8, fps_str );

	fps_calc.frames++;
}


extern "C" void PANZER_DrawFill(int x, int y, int w, int h, int c)
{
	char* buff= (char*)command_buffer.buffer;
	buff+= command_buffer.current_pos;

	int color= d_8to24table[c&255];

	int com_size= ComIn_SetConstantColor( buff, (unsigned char*)&color );
	buff+= com_size;
	command_buffer.current_pos+= com_size;

	*((int*)buff)= COMMAND_DRAW_SPRITE;
	buff+= sizeof(int);
	command_buffer.current_pos+= sizeof(int);

	DrawSpriteCall* call= (DrawSpriteCall*)buff;
	call->sprite_count= 1;
	call->DrawSpriteFunc= DrawFill;
	buff+= sizeof(DrawSpriteCall);
	command_buffer.current_pos+= sizeof(DrawSpriteCall);
	
	((int*)buff)[0]= x;
	((int*)buff)[1]= vid.height-(y+h);
	((int*)buff)[2]= x+w;
	((int*)buff)[3]= vid.height-y;
	((int*)buff)[4]= 0;

	command_buffer.current_pos+= sizeof(int)*5;
}




void InitParticles()
{
	//LoadTGA( "pics/particle.tga", particle_image.pixels, &particle_image.width, &particle_image.height );
	//particle_texture.Create( particle_image.width, particle_image.height, false, NULL, particle_image.pixels[0] );
}


void DrawParticles( const m_Mat4* mat, particle_t* particles, int count, float fov_rad )
{
	int call_size = 
		 8//set color call
		+sizeof(DrawSpriteCall)//call struct
		+5*sizeof(int);//parameters
	if( command_buffer.current_pos + call_size * count > command_buffer.size )
		Com_ResizeCommandBuffer( command_buffer.buffer, command_buffer.size,
								command_buffer.size + call_size * count,
								&command_buffer.size );


	float width2= float(vid.width)*0.5f;
	float height2= float(vid.height)*0.5f;
	unsigned char color[4];
	float init_sprite_radius = float(vid.width)/( sin(fov_rad*0.5f) * 256.0f );

	/*command_buffer.current_pos+=
		ComIn_SetTexture( (char*)command_buffer.buffer + command_buffer.current_pos,
		&particle_texture );
	*/

	for( int i= 0; i< count; i++ )
	{
		m_Vec3 pos( particles[i].origin[0],particles[i].origin[1], particles[i].origin[2] );
		pos = pos * *mat;
		if( pos.z <= PSR_MIN_ZMIN_FLOAT )
			continue;
		float inv_z = 1.0f / pos.z;

		pos.x= (pos.x * inv_z + 1.0f) * width2;
		pos.y= (pos.y * inv_z + 1.0f) * height2;

		char* buff= (char*)command_buffer.buffer;
		buff+= command_buffer.current_pos;

		*((int*)color)= d_8to24table[particles[i].color];
		color[3]= (unsigned char)(particles[i].alpha * 255.0f);
		int com_size= ComIn_SetConstantColor( buff, color );

		command_buffer.current_pos+= com_size;
		buff+= com_size;

		*((int*)buff)= COMMAND_DRAW_SPRITE;
		command_buffer.current_pos+= sizeof(int);
		buff+= sizeof(int);

		DrawSpriteCall* call= (DrawSpriteCall*)buff;
		if( color[3] > 250 )
			call->DrawSpriteFunc= DrawParticleSpriteNoBlend;
		else
			call->DrawSpriteFunc= DrawParticleSprite;
		call->sprite_count= 1;

		command_buffer.current_pos+= sizeof(DrawSpriteCall);
		buff+= sizeof(DrawSpriteCall);

		float sprite_radius = init_sprite_radius * inv_z;
		if(sprite_radius<0.51f) sprite_radius= 0.51f;
		DrawParticleSpriteToBuffer( buff,
			int(pos.x-sprite_radius), int(pos.y-sprite_radius),
			int(pos.x+sprite_radius), int(pos.y+sprite_radius),
			fixed16_t(pos.z*65536.0f) );
		command_buffer.current_pos+= sizeof(int)*5;
	}
}


void Mat4RotateX(m_Mat4* mat, float a)
{
	mat->Identity();
	(*mat)[10]= cos(a);
	(*mat)[6]= -sin(a);
	(*mat)[9]= sin(a);
	(*mat)[5]= cos(a);
}

void Mat4RotateZ(m_Mat4* mat, float a)
{
	mat->Identity();
	(*mat)[5]= cos(a);
	(*mat)[1]= -sin(a);
	(*mat)[0]= cos(a);
	(*mat)[4]= sin(a);
}


void DrawEntities(m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos, bool alpha_entities )
{
	entity_t* ent;
	model_t* model;
	int i;
	for(  i= 0, ent= r_newrefdef.entities; i< r_newrefdef.num_entities; i++, ent++ )
	{
		if( alpha_entities )
		{
			if( (ent->flags&RF_TRANSLUCENT)==0 )
				continue;
		}
		else
		{
			if( (ent->flags&RF_TRANSLUCENT)!=0 )
				continue;
		}

		model= ent->model;
		if( (ent->flags&RF_BEAM) != 0 )
			DrawBeam( ent, mat, normal_mat, cam_pos );
		else if( model->type == mod_sprite )
			DrawSpriteEntity( ent, mat, cam_pos );
		else if( model->type == mod_brush )
			DrawBrushEntity( ent, mat, normal_mat, cam_pos, alpha_entities );
		else if( model->type == mod_alias )
			DrawAliasEntity( ent, mat, normal_mat,  cam_pos );
		else
		{
			printf( "unknown model type!\n" );
		}
	}
}


void InitPlayerFlashlight()
{
	VectorCopy( r_newrefdef.vieworg, player_flashlight.origin );

	player_flashlight.color[0]= player_flashlight.color[1]= player_flashlight.color[2]= 1.0f;
	player_flashlight.intensity= 768.0f;

	player_flashlight.cone_radius= 3.1415926535f / 24.0f;

	const float to_rad = 3.1415926535f / 180.0f;
	float a= (r_newrefdef.viewangles[1]  -180.0f )* to_rad;
	float b= r_newrefdef.viewangles[0] * to_rad;
	player_flashlight.direction[0]= cosf(a) * cosf(b);
	player_flashlight.direction[1]= sinf(a) * cosf(b);
	player_flashlight.direction[2]= sinf(b);
}



extern void DrawSkyBox( const m_Mat4* rot_mat, vec3_t cam_pos );


extern "C" void PANZER_RenderFrame(refdef_t *fd)
{
	r_newrefdef= *fd;
	r_framecount++;
	/*if( r_newrefdef.num_dlights < 32 )
	{
		dlight_t* light= r_newrefdef.dlights + r_newrefdef.num_dlights;
		VectorCopy( fd->vieworg, light->origin );
		light->origin[0]+= 128.0f;
		light->intensity= 256.0f;
		light->color[0]= 0.3f;
		light->color[1]= 0.5f;
		light->color[2]= 1.0f;
		r_newrefdef.num_dlights++;
	}*/

	InitPlayerFlashlight();
	SetWorldFrame( int(r_newrefdef.time*2.0f) );

	command_buffer.current_pos+= ComIn_ClearDepthBuffer( 
		(char*)command_buffer.buffer + command_buffer.current_pos, 0xFFFF );

	if( (fd->rdflags&RDF_NOWORLDMODEL) == 0 )
	{
		if( r_clear_color_buffer->value )
		{
			unsigned char clear_color[]= { 245, 16, 251, 0 };
			command_buffer.current_pos+= ComIn_ClearColorBuffer(
				(char*)command_buffer.buffer + command_buffer.current_pos, clear_color );
		}

		command_buffer.current_pos+= ComIn_SetTexturePaletteRaw(
			(char*)command_buffer.buffer + command_buffer.current_pos, (unsigned char*)d_8to24table );
		command_buffer.current_pos+= ComIn_SetConstantTime(
			(char*)command_buffer.buffer + command_buffer.current_pos, int(r_newrefdef.time*65536.0f*64.0f) );

		m_Mat4 pers, rot_x, rot_y, rot_z, result, scale, basis_change, shift, normal_mat, sky_mat, sky_rot;

		const float to_rad = 3.1415926535f / 180.0f;
		m_Vec3 pos( -fd->vieworg[0], -fd->vieworg[1], -fd->vieworg[2] );
		shift.Identity();
		shift.Translate(pos);
	
		rot_x.RotateX( fd->viewangles[0] * to_rad );
		rot_z.RotateZ( (fd->viewangles[1]  + 90.0f )* to_rad );
		rot_y.RotateY( fd->viewangles[2] * to_rad );

		basis_change.Identity();
		basis_change[0]= -1.0f;
		basis_change[5]= 0.0f;
		basis_change[6]= -1.0f;
		basis_change[9]= 1.0f;
		basis_change[10]= 0.0f;

		scale.Identity();
		scale.Scale(1.0f/float(Q2_UNITS_PER_METER));
		pers.Identity();
		pers[0]= 1.0f / tan( fd->fov_x * to_rad * 0.5f );
		pers[5]= 1.0f / tan( fd->fov_y * to_rad * 0.5f );

		sky_mat= scale * rot_z * rot_x * rot_y * basis_change * pers;
		

		result= shift * scale * rot_z * rot_x * rot_y * basis_change * pers;
		SetFov(fd->fov_y * to_rad);

		
		rot_x.RotateX( -fd->viewangles[0] * to_rad );
		rot_z.RotateZ( -(fd->viewangles[1]  + 90.0f )* to_rad );
		rot_y.RotateY( -fd->viewangles[2] * to_rad );
		normal_mat= rot_y * rot_x * rot_z;

		InitFrustrumClipPlanes(&normal_mat, r_newrefdef.vieworg);
		BuildSurfaceLists(&result, fd->vieworg);

		SetSurfaceMatrix(&result);
		DrawWorldTextureChains();

		SetSurfaceMatrix(&result);
		DrawEntities(&result, &normal_mat, fd->vieworg, false );

		static float sky_pos[]= { 0.0f, 0.0f, 0.0f };
		InitFrustrumClipPlanes(&normal_mat, sky_pos);
		DrawSkyBox(&sky_mat, fd->vieworg);

		InitFrustrumClipPlanes(&normal_mat, r_newrefdef.vieworg);
		SetSurfaceMatrix(&result);
		DrawWorldAlphaSurfaces();

		SetSurfaceMatrix(&result);
		DrawEntities(&result, &normal_mat, fd->vieworg, true );


		DrawParticles( &result,  fd->particles, fd->num_particles, fd->fov_y * to_rad );


		unsigned char blend_color[]= {
			fd->blend[0]*255.0f, fd->blend[1]*255.0f, fd->blend[2]*255.0f, fd->blend[3]*255.0f };
		ColorByteSwap( blend_color );

		if( blend_color[3] > 4 )
			command_buffer.current_pos+= ComIn_AlphaBlendColorBuffer( 
				(char*)command_buffer.buffer + command_buffer.current_pos,
				blend_color);

	}
	else
	{
		unsigned char clear_color[]= { 128, 32, 128, 32 };
	command_buffer.current_pos+= ComIn_ClearColorBuffer(
		(char*)command_buffer.buffer + command_buffer.current_pos, clear_color );
	}

	if( use_multithreading)
		DrawCharString(8, 32, "multithreading" );
}