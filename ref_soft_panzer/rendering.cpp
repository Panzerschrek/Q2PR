#include "r_world_rendering.h"


#include "panzer_rast/math_lib/matrix.h"
#include "panzer_rast/rendering_state.h"

#define Q2_UNITS_PER_METER 64

extern Texture* R_FindTexture( char* name );
extern Texture* R_FindTexture(image_t* image);
extern int R_GetImageIndex(image_t* image);

extern "C" void LoadTGA (char *name, byte **pic, int *width, int *height);
extern "C" struct image_s *PANZER_RegisterPic(char *name);
extern "C" void PANZER_DrawPic(int x, int y, char *name);

extern "C" swstate_t sw_state;


void DrawSpriteEntity( entity_t* ent, m_Mat4* mat, vec3_t cam_pos );
void DrawBrushEntity( entity_t* ent, m_Mat4* mat, vec3_t cam_pos, bool is_aplha );
void DrawAliasEntity(  entity_t* ent, m_Mat4* mat, vec3_t cam_pos );
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
----sky buffer-------
*/
Texture sky_textures[6];
image_t sky_images[6];
bool is_any_sky= false;




/*
----------fps calculating---------
*/
struct fps_calc_t
{
	int last_fps_time;
	int last_frame_time;
	int last_fps;
	int frames;

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
	use_multithreading = (bool)(int)r_use_multithreading->value;

	if( use_multithreading )
	{
		Sys_CreateMutex( &frontend_front_command_buffer_mutex );
		Sys_CreateMutex( &frontend_framebuffer_mutex );
	}
	memset(&backend_thread, sizeof(qthread_t), 0 );

	memset( &fps_calc, 0, sizeof(fps_calc_t) );

	front_chars= (CharBuffer*) malloc( sizeof(CharBuffer) );
	back_chars= (CharBuffer*) malloc( sizeof(CharBuffer) );
	front_chars->char_count= 0;
	back_chars->char_count= 0;

	memset( sky_images, sizeof(sky_images), 0 );

	command_buffer.buffer= Com_ResizeCommandBuffer( NULL, 0, 1024*1024*16, &command_buffer.size );
	back_command_buffer.buffer= Com_ResizeCommandBuffer( NULL, 0, 1024*1024*16, &back_command_buffer.size );

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

	char fps_str[32];
	sprintf( fps_str, " fps: %d\n %dms \n total: %2.3f", fps_calc.last_fps, dt * 1000 / CLOCKS_PER_SEC,
		(current_time == fps_calc.start_time) ? 60 : (float(CLOCKS_PER_SEC*fps_calc.total_frames)/float(current_time-fps_calc.start_time)) );
	DrawCharString( vid.width - 8 * 16, 8, fps_str );

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

extern "C" void PANZER_SetSky(char *name, float rotate, vec3_t axis)
{
	static const char*const suf[6] = {"ft", "up", "lf", "bk", "dn", "rt"};
	static const int	r_skysideimage[6] = {5, 2, 4, 1, 0, 3};
	char	pathname[MAX_QPATH];

	for( int i= 0; i< 6; i++ )
	{
			Com_sprintf (pathname, sizeof(pathname), "env/%s%s.tga", name, suf[i]);
			if( sky_images[i].pixels[0] != NULL )
				free(sky_images[i].pixels[0]);
			LoadTGA( pathname, &sky_images[i].pixels[0], &sky_images[i].width, &sky_images[i].height );
			sky_textures[i].Create( sky_images[i].width, sky_images[i].height, false, NULL, sky_images[i].pixels[0] );
	}
	is_any_sky= true;
}



void DrawSkyBox( const m_Mat4* mat )
{
	using namespace VertexProcessing;
	static const float cube_vertices[]= { 
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f };
		
	static const int cube_triangles_indeces[]= {
	3,1,0, 3,0,2,//x+
	5,4,0, 5,0,1,//y+
	7,5,1, 7,1,3,//z-
	6,4,5, 6,5,7,//x-
	6,7,3, 6,3,2,//y-
	2,0,4, 2,4,6//z+ 
	 };
	static const int cube_triangles_tc[]= {
		1,1, 1,0, 0,0,  1,1, 0,0, 0,1,//check
		0,0, 0,1, 1,1,  0,0, 1,1, 1,0,//check
		1,1, 1,0, 0,0,  1,1, 0,0, 0,1,//check
		1,1, 1,0, 0,0,  1,1, 0,0, 0,1,//check
		0,0, 0,1, 1,1,  0,0, 1,1, 1,0,//check
		1,1, 1,0, 0,0,  1,1, 0,0, 0,1,//chrck
	};


	m_Vec3 transformed_vertices[8];
	float transformed_inv_w[8];
	m_Mat4 skybox_scale, final_mat;
	skybox_scale.Identity();
	skybox_scale.Scale( 64.0f * 16.0f );
	final_mat = skybox_scale * *mat;
	for( int i= 0; i< 8; i++ )
	{
		m_Vec3 v= *(((m_Vec3*)cube_vertices) + i);
		transformed_vertices[i]= v * final_mat;
		transformed_inv_w[i]= 1.0f / transformed_vertices[i].z;//1.0f / mat->Vec3MatMulW(v);
	}

	float vid_width2= float(vid.width) * 0.5f;
	float vid_height2= float(vid.height) * 0.5f;

	int tex_scaler= sky_textures[0].SizeXLog2() + 16;
	for( int i= 0; i< 12; i++ )
	{
		float interp;
		if( (i&1) == 0 )
		{	
			command_buffer.current_pos+= ComIn_SetTexture( 
			command_buffer.current_pos + (char*)command_buffer.buffer, &sky_textures[i>>1] );
		}
		int ind[]= { cube_triangles_indeces[i*3], cube_triangles_indeces[i*3+1], cube_triangles_indeces[i*3+2] };
		float z[]= { 1.0f / transformed_inv_w[ind[0]], 1.0f / transformed_inv_w[ind[1]], 1.0f / transformed_inv_w[ind[2]] };
		int culled_vertices= CullTriangleByZNearPlane( z[0], z[1], z[2] );
		if( culled_vertices == 3 )
			continue;

		((int*)((char*)command_buffer.buffer + command_buffer.current_pos))[0]= COMMAND_DRAW_TRIANGLE;
		command_buffer.current_pos+= sizeof(int);
		DrawTriangleCall* call= (DrawTriangleCall*)(((char*)command_buffer.buffer) + command_buffer.current_pos);
		call->triangle_count= 0;
		call->vertex_size= sizeof(int)*3 + sizeof(int)*2;
		call->DrawFromBufferFunc= DrawSkyTriangleFromBuffer;
		m_Vec3* v[]= { transformed_vertices + ind[0], transformed_vertices + ind[1], transformed_vertices + ind[2] };
		if( culled_vertices == 0 )
		{
			triangle_in_vertex_xy[0]= ( v[0]->x * transformed_inv_w[ind[0]] + 1.0f ) * vid_width2 *65536.0f;
			triangle_in_vertex_xy[1]= ( v[0]->y * transformed_inv_w[ind[0]] + 1.0f ) * vid_height2*65536.0f;
			triangle_in_vertex_xy[2]= ( v[1]->x * transformed_inv_w[ind[1]] + 1.0f ) * vid_width2 *65536.0f;
			triangle_in_vertex_xy[3]= ( v[1]->y * transformed_inv_w[ind[1]] + 1.0f ) * vid_height2*65536.0f;
			triangle_in_vertex_xy[4]= ( v[2]->x * transformed_inv_w[ind[2]] + 1.0f ) * vid_width2 *65536.0f;
			triangle_in_vertex_xy[5]= ( v[2]->y * transformed_inv_w[ind[2]] + 1.0f ) * vid_height2*65536.0f;
			triangle_in_vertex_z[0]= int( 65536.0f * z[0] );
			triangle_in_vertex_z[1]= int( 65536.0f * z[1] );
			triangle_in_vertex_z[2]= int( 65536.0f * z[2] );
			triangle_in_tex_coord[0]= (cube_triangles_tc[i*6  ])<<tex_scaler;
			triangle_in_tex_coord[1]= (cube_triangles_tc[i*6+1])<<tex_scaler;
			triangle_in_tex_coord[2]= (cube_triangles_tc[i*6+2])<<tex_scaler;
			triangle_in_tex_coord[3]= (cube_triangles_tc[i*6+3])<<tex_scaler;
			triangle_in_tex_coord[4]= (cube_triangles_tc[i*6+4])<<tex_scaler;
			triangle_in_tex_coord[5]= (cube_triangles_tc[i*6+5])<<tex_scaler;

			char* buff= ((char*)call) + sizeof(DrawTriangleCall);
			if( DrawSkyTriangleToBuffer(buff) != 0 )
				call->triangle_count++;
		}
		else if( culled_vertices == 2 )
		{
			//m_Vec3* front_v= transformed_vertices + ind[ cull_passed_vertices[0] ];
			triangle_in_vertex_xy[0]= ( v[ cull_passed_vertices[0] ]->x * transformed_inv_w[ind[cull_passed_vertices[0]]] + 1.0f ) * vid_width2;
			triangle_in_vertex_xy[1]= ( v[ cull_passed_vertices[0] ]->y * transformed_inv_w[ind[cull_passed_vertices[0]]] + 1.0f ) * vid_height2;
			triangle_in_vertex_z[0]= int( 65536.0f * z[cull_passed_vertices[0]]);
			triangle_in_tex_coord[0]= (cube_triangles_tc[i*6+cull_passed_vertices[0]*2  ])<<tex_scaler;
			triangle_in_tex_coord[1]= (cube_triangles_tc[i*6+cull_passed_vertices[0]*2+1])<<tex_scaler;
			
			const float inv_interp_z= 1.0f / PSR_MIN_ZMIN_FLOAT;
			interp= v[ cull_passed_vertices[0] ]->x * cull_new_vertices_interpolation_k[0] + 
					v[ cull_lost_vertices[0] ]->x * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_vertex_xy[2]= ( inv_interp_z * interp + 1.0f ) * vid_width2;
			interp= v[ cull_passed_vertices[0] ]->y * cull_new_vertices_interpolation_k[0] + 
					v[ cull_lost_vertices[0] ]->y * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_vertex_xy[3]= ( inv_interp_z * interp + 1.0f ) * vid_height2;
			interp= v[ cull_passed_vertices[0] ]->x * cull_new_vertices_interpolation_k[1] + 
					v[ cull_lost_vertices[1] ]->x * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_vertex_xy[4]= ( inv_interp_z * interp + 1.0f ) * vid_width2;
			interp= v[ cull_passed_vertices[0] ]->y * cull_new_vertices_interpolation_k[1] + 
					v[ cull_lost_vertices[1] ]->y * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_vertex_xy[5]= ( inv_interp_z * interp + 1.0f ) * vid_height2;

			triangle_in_vertex_z[1]= PSR_MIN_ZMIN;//int(interp_z*65536.0f);
			triangle_in_vertex_z[2]= PSR_MIN_ZMIN;//int(interp_z*65536.0f);

			float f_tex_scaler= float(1<<tex_scaler);
			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[0]*2  ]) * cull_new_vertices_interpolation_k[0] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[0]*2  ]) * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_tex_coord[2]= int(interp*f_tex_scaler);
			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[0]*2+1]) * cull_new_vertices_interpolation_k[0] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[0]*2+1]) * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_tex_coord[3]= int(interp*f_tex_scaler);
			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[0]*2  ]) * cull_new_vertices_interpolation_k[1] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[1]*2  ]) * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_tex_coord[4]= int(interp*f_tex_scaler);
			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[0]*2+1]) * cull_new_vertices_interpolation_k[1] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[1]*2+1]) * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_tex_coord[5]= int(interp*f_tex_scaler);


			char* buff= ((char*)call) + sizeof(DrawTriangleCall);
			//if( DrawSkyTriangleToBuffer(buff) != 0 )
			//	call->triangle_count++;
		}
		else//if( culled_vertices == 1 )
		{
			triangle_in_vertex_xy[0]= ( v[ cull_passed_vertices[0] ]->x * transformed_inv_w[ind[cull_passed_vertices[0]]] + 1.0f ) * vid_width2;
			triangle_in_vertex_xy[1]= ( v[ cull_passed_vertices[0] ]->y * transformed_inv_w[ind[cull_passed_vertices[0]]] + 1.0f ) * vid_height2;
			triangle_in_vertex_z[0]= int( 65536.0f * z[cull_passed_vertices[0]]);
			triangle_in_tex_coord[0]= (cube_triangles_tc[i*6+cull_passed_vertices[0]*2  ])<<tex_scaler;
			triangle_in_tex_coord[1]= (cube_triangles_tc[i*6+cull_passed_vertices[0]*2+1])<<tex_scaler;

			triangle_in_vertex_xy[2]= ( v[ cull_passed_vertices[1] ]->x * transformed_inv_w[ind[cull_passed_vertices[1]]] + 1.0f ) * vid_width2;
			triangle_in_vertex_xy[3]= ( v[ cull_passed_vertices[1] ]->y * transformed_inv_w[ind[cull_passed_vertices[1]]] + 1.0f ) * vid_height2;
			triangle_in_vertex_z[1]= int( 65536.0f * z[cull_passed_vertices[1]]);
			triangle_in_tex_coord[2]= (cube_triangles_tc[i*6+cull_passed_vertices[1]*2  ])<<tex_scaler;
			triangle_in_tex_coord[3]= (cube_triangles_tc[i*6+cull_passed_vertices[1]*2+1])<<tex_scaler;

			const float inv_interp_z= 1.0f / PSR_MIN_ZMIN_FLOAT;
			interp= v[ cull_passed_vertices[0] ]->x * cull_new_vertices_interpolation_k[0] + 
					v[ cull_lost_vertices[0] ]->x * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_vertex_xy[4]= ( inv_interp_z * interp + 1.0f ) * vid_width2;
			interp= v[ cull_passed_vertices[0] ]->y * cull_new_vertices_interpolation_k[0] + 
					v[ cull_lost_vertices[0] ]->y * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_vertex_xy[5]= ( inv_interp_z * interp + 1.0f ) * vid_height2;

			triangle_in_vertex_z[2]= PSR_MIN_ZMIN;//int(interp_z*65536.0f);

			float f_tex_scaler= float(1<<tex_scaler);
			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[0]*2  ]) * cull_new_vertices_interpolation_k[0] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[0]*2  ]) * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_tex_coord[4]= int(interp*f_tex_scaler);
			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[0]*2+1]) * cull_new_vertices_interpolation_k[0] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[0]*2+1]) * ( 1.0f - cull_new_vertices_interpolation_k[0] );
			triangle_in_tex_coord[5]= int(interp*f_tex_scaler);

			char* buff= ((char*)call) + sizeof(DrawTriangleCall);
			/*if( DrawSkyTriangleToBuffer(buff) != 0 )
			{
				buff+= call->vertex_size*4;
				call->triangle_count++;
			}*/

			interp= v[ cull_passed_vertices[1] ]->x * cull_new_vertices_interpolation_k[1] + 
					v[ cull_lost_vertices[0] ]->x * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_vertex_xy[0]= ( inv_interp_z * interp + 1.0f ) * vid_width2;
			interp= v[ cull_passed_vertices[1] ]->y * cull_new_vertices_interpolation_k[1] + 
					v[ cull_lost_vertices[0] ]->y * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_vertex_xy[1]= ( inv_interp_z * interp + 1.0f ) * vid_height2;

			triangle_in_vertex_z[0]= PSR_MIN_ZMIN;//int(interp_z[1]*65536.0f);

			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[1]*2  ]) * cull_new_vertices_interpolation_k[1] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[0]*2  ]) * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_tex_coord[0]= int(interp*f_tex_scaler);
			interp= float(cube_triangles_tc[i*6+cull_passed_vertices[1]*2+1]) * cull_new_vertices_interpolation_k[1] +
					float(cube_triangles_tc[i*6+  cull_lost_vertices[0]*2+1]) * ( 1.0f - cull_new_vertices_interpolation_k[1] );
			triangle_in_tex_coord[1]= int(interp*f_tex_scaler);
			
			//if( DrawSkyTriangleToBuffer(buff)!=0 )
			//	call->triangle_count++;
		}
		command_buffer.current_pos+= sizeof(DrawTriangleCall) + call->vertex_size * 4 * call->triangle_count;
	}//for skybox triangles


}


void InitParticles()
{
	LoadTGA( "pics/particle.tga", particle_image.pixels, &particle_image.width, &particle_image.height );
	particle_texture.Create( particle_image.width, particle_image.height, false, NULL, particle_image.pixels[0] );
}


void DrawParticles( const m_Mat4* mat, particle_t* particles, int count, float fov_rad )
{
	int call_size = 
		 8//set color call
		+8//call struct
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


void DrawEntities(m_Mat4* mat, vec3_t cam_pos, bool alpha_entities )
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
		if(model==NULL)
			continue;
		if( model->type == mod_sprite )
			DrawSpriteEntity( ent, mat, cam_pos );
		else if( model->type == mod_brush )
			DrawBrushEntity( ent, mat, cam_pos, alpha_entities );
		else if( model->type == mod_alias )
			DrawAliasEntity( ent, mat, cam_pos );
		else
		{
			printf( "unknown model type!\n" );
		}
	}
}

extern "C" void PANZER_RenderFrame(refdef_t *fd)
{
	r_newrefdef= *fd;
	r_framecount++;

	/*if( r_newrefdef.num_dlights < 32 )
	{
		dlight_t* light= r_newrefdef.dlights + r_newrefdef.num_dlights;
		VectorCopy( fd->vieworg, light->origin );
		light->intensity= 768.0f;
		light->color[0]= 1.0f;
		light->color[1]= 0.8f;
		light->color[2]= 0.7f;
		r_newrefdef.num_dlights++;
	}*/

	SetWorldFrame( int(r_newrefdef.time*2.0f) );

	command_buffer.current_pos+= ComIn_ClearDepthBuffer( 
		(char*)command_buffer.buffer + command_buffer.current_pos, 0xFFFF );

	if( is_any_sky )
	{
		unsigned char clear_color[]= { 0, 0, 0, 0 };
		command_buffer.current_pos+= ComIn_ClearColorBuffer(
			(char*)command_buffer.buffer + command_buffer.current_pos, clear_color );


		command_buffer.current_pos+= ComIn_SetTexturePaletteRaw(
			(char*)command_buffer.buffer + command_buffer.current_pos, (unsigned char*)d_8to24table );

		m_Mat4 pers, rot_x, rot_y, rot_z, result, scale, basis_change, shift;

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

		result=  scale * rot_z * rot_x * rot_y * basis_change * pers;

		result= shift * scale * rot_z * rot_x * rot_y * basis_change * pers;
		SetFov(fd->fov_y * to_rad);
		BuildSurfaceLists(&result, fd->vieworg);

		SetSurfaceMatrix(&result);
		DrawWorldTextureChains();

		SetSurfaceMatrix(&result);
		DrawEntities(&result, fd->vieworg, false );

		SetSurfaceMatrix(&result);
		DrawWorldAlphaSurfaces();

		SetSurfaceMatrix(&result);
		DrawEntities(&result, fd->vieworg, true );


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