#include "r_world_rendering.h"
#include "r_surf.h"

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
void InitBrushEntitiesRenderingParameters();
void DrawBrushEntity( entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos, bool is_aplha );
void DrawAliasEntity(  entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos );
void DrawBeam( entity_t* ent, m_Mat4* mat, m_Mat4* normal_mat, vec3_t cam_pos );
/*
----command buffer-------
*/
CommandBuffer command_buffer;//for vertex processing
CommandBuffer back_command_buffer;//for rasterization
qmutex_t frontend_framebuffer_mutex;
qsemaphore_t command_buffer_semaphore;
//qmutex_t backend_framebuffer_mutex;
//qmutex_t frontend_front_command_buffer_mutex;
//qmutex_t backend_front_command_buffer_mutex;
#define FRAMEBUFFER_MUTEX_NAME		"q2_panzer_soft_rast_framebuffer_mutex"
#define COMMAND_BUFFER_MUTEX_NAME	"q2_panzer_soft_rast_front_command_buffer_mutex"

bool need_terminate_backend_thread= true;
bool use_multithreading= false;
qthread_t backend_thread;


/*
----char buffer-------
*/
/*#define MAX_BUFFER_CHARS 8192
struct CharBuffer
{
	unsigned char char_buffer[MAX_BUFFER_CHARS];
	short char_buffer_coords[MAX_BUFFER_CHARS*2];
	int char_count;
};

CharBuffer* front_chars;
CharBuffer* back_chars;*/
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
//CURRENTLY UNUSED. now, all chars draw in order of PANZER_DrawChar callings
/*
void DrawChars( void* unused )
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
*/


void BackendDrawChar( void* data )
{
	int param= *((int*)data);

	unsigned char char_code= param&255;

	int x0= (param>>8)&4095;
	int y0= (param>>(8+12))&4095;


	if( y0 >= vid.height - 8 || y0 < 0 )
		return;

	unsigned char* dst= PRast_GetFrontBuffer() + ( x0 + y0*vid.width )*4;

	unsigned int row= 15 - (char_code>>4);
	unsigned int col= char_code&15;
	const unsigned char* src= console_tex->GetData() + (col<<(3+2)) + (row<<(3+7+2));

#ifdef PSR_MASM32
	int dst_add= vid.width<<2;
	unsigned int transparent_color= d_8to24table[255];
	transparent_color|= 0xFF000000;
	__asm
	{
		mov edi, dst
		mov esi, src
		mov ecx, 8
		mov ebx, transparent_color
next_char_line:
		mov eax, dword ptr[esi]
		cmp eax, ebx
		jz c1
		mov dword ptr[edi], eax
		c1:
		mov edx, dword ptr[esi+4]
		cmp edx, ebx
		jz c2
		mov dword ptr[edi+4], edx
		c2:
		mov eax, dword ptr[esi+8]
		cmp eax, ebx
		jz c3
		mov dword ptr[edi+8], eax
		c3:
		mov edx, dword ptr[esi+12]
		cmp edx, ebx
		jz c4
		mov dword ptr[edi+12], edx
		c4:
		mov eax, dword ptr[esi+16]
		cmp eax, ebx
		jz c5
		mov dword ptr[edi+16], eax
		c5:
		mov edx, dword ptr[esi+20]
		cmp edx, ebx
		jz c6
		mov dword ptr[edi+20], edx
		c6:
		mov eax, dword ptr[esi+24]
		cmp eax, ebx
		jz c7
		mov dword ptr[edi+24], eax
		c7:
		mov edx, dword ptr[esi+28]
		cmp edx, ebx
		jz c8
		mov dword ptr[edi+28], edx
		c8:

		add edi, dst_add
		add esi, 128*4
		loop next_char_line
	}
#else
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
#endif
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

		Sys_SemaphoreWait( &command_buffer_semaphore );

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
		//Sys_CreateMutex( &frontend_front_command_buffer_mutex );
		Sys_CreateMutex( &frontend_framebuffer_mutex );
		Sys_CreateSemaphore( &command_buffer_semaphore, "q2_sem" );
	}
	memset(&backend_thread, sizeof(qthread_t), 0 );

	memset( &fps_calc, 0, sizeof(fps_calc_t) );

	//front_chars= new CharBuffer;
	//back_chars= new CharBuffer;

	//front_chars->char_count= 0;
	//back_chars->char_count= 0;

	command_buffer.buffer= Com_ResizeCommandBuffer( NULL, 0, 1024*1024*3, &command_buffer.size );
	back_command_buffer.buffer= Com_ResizeCommandBuffer( NULL, 0, 1024*1024*3, &back_command_buffer.size );

	command_buffer.current_pos=
		back_command_buffer.current_pos= 0;

	InitParticles();

	image_t* img= PANZER_RegisterPic( "conchars" );
	console_tex= R_FindTexture(img);
}
extern "C" void PR_ShutdownRendering()
{

	if(use_multithreading)
	{
		Sys_DestroyThread(backend_thread);
		Sys_DestroyMutex(&frontend_framebuffer_mutex);
	//	Sys_DestroyMutex(&backend_framebuffer_mutex);
		//Sys_DestroyMutex(&frontend_front_command_buffer_mutex);
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

	//delete front_chars;
	//delete back_chars;
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

	CalculateAndShowFPS();

	//char statistic_str[128];
	//sprintf( statistic_str, "command buffer size: %dkb\nparticles: %d", command_buffer.current_pos>>10, r_newrefdef.num_particles );
	//DrawCharString( 8, 16, statistic_str );

	//draw chars
	//command_buffer.current_pos+= ComIn_AddUserDefinedFunc(
	//		(char*)command_buffer.buffer + command_buffer.current_pos, DrawChars, 0 );
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

	if( use_multithreading )
	{
		Sys_MutexLock( &frontend_framebuffer_mutex );
		CommandBuffer tmp_com_buf= command_buffer;
		command_buffer= back_command_buffer;
		back_command_buffer= tmp_com_buf;

		//swap char buffers
		//CharBuffer* tmp= front_chars;
		//front_chars= back_chars;
		//back_chars= tmp;
		//front_chars->char_count= 0;
	}
	else
	{
		//CharBuffer* tmp= front_chars;
		//front_chars= back_chars;
		//back_chars= tmp;
		//front_chars->char_count= 0;
		ComOut_DoCommands( command_buffer.buffer, command_buffer.current_pos );
	}


	command_buffer.current_pos= 0;
}

extern "C" void PR_EndBufferSwapping()
{
	if(use_multithreading)
	{
		Sys_MutexUnlock( &frontend_framebuffer_mutex );
		Sys_SemaphoreRelease( &command_buffer_semaphore );
		//Sys_MutexUnlock( &frontend_front_command_buffer_mutex );

		if( backend_thread.thread_handle == 0 )
			backend_thread= Sys_CreateThread( PR_MainBackEndFunc, NULL );
	}
}

extern "C" void PR_BeginFrame()
{
	if(use_multithreading)
	{
		//Sys_MutexLock( &frontend_front_command_buffer_mutex );
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
	/*if( front_chars->char_count >= MAX_BUFFER_CHARS )
		return;
	int i= front_chars->char_count;
	c&= 255;
	front_chars->char_buffer[i]= c;
	front_chars->char_buffer_coords[i*2  ]= x;
	front_chars->char_buffer_coords[i*2+1]= vid.height - y - 8;
	front_chars->char_count++;*/
	char* buff= (char*) command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;

	buff+= ComIn_AddUserDefinedFunc( buff, BackendDrawChar, sizeof(int) );
	y= vid.height - y - 8;
	int param= c&255;
	param|= x<<8;
	param|= y<<(8+12);
	*((int*)buff)= param;

	buff+= sizeof(int);

	command_buffer.current_pos+= buff-buff0;

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
	/*
	unsigned long long int result;
	unsigned long long int* result_ptr= &result;
	__asm
	{
		mov ecx, result_ptr
		rdtsc
		mov dword ptr[ecx], eax
		mov dword ptr[ecx+4], edx
	}
	return result;*/
	return 42;
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
	sprintf( fps_str, "fps: %d\n%dms\ntotal: %3.2f\n",
		fps_calc.last_fps, dt * 1000 / CLOCKS_PER_SEC,
		(current_time == fps_calc.start_time) ? 60 : (float(CLOCKS_PER_SEC*fps_calc.total_frames)/float(current_time-fps_calc.start_time))
		);
	DrawCharString( vid.width - 14 * 8, 8, fps_str );

	fps_calc.frames++;
}


static char tile_pic_name[MAX_QPATH];
static int tile_draw_call_frame= -1;

extern "C" void PANZER_DrawTileClear(int x, int y, int w, int h, char *name)
{
	tile_draw_call_frame= r_framecount;
	strcpy( tile_pic_name, name );
}

void DrawTiles()
{
	if( r_framecount-1 != tile_draw_call_frame )//if in previous frame nothing calls PANZER_DrawTileClear
		return;

	//HACK - detection of fully filled screen ( if not need tiles rendering )
	if( r_newrefdef.x == 0 )
		return;

	image_t* img= PANZER_RegisterPic( tile_pic_name );
	if( img == NULL )
		return;

	char* buff= (char*)command_buffer.buffer;
	buff+= command_buffer.current_pos;
	char* buff0= buff;

	Texture* tex= R_FindTexture( img );
	buff+= ComIn_SetTexture( buff, tex );

	*((int*)buff)= COMMAND_DRAW_SPRITE;
	buff+= sizeof(int);

	DrawSpriteCall* call= (DrawSpriteCall*)buff;

	call->DrawSpriteFunc= DrawStretchTextureRGBANoAlphaTest;
	call->sprite_count= 0;

	buff+= sizeof(DrawSpriteCall);

	int y_tiles= vid.height / tex->OriginalSizeY();
	if( y_tiles * tex->OriginalSizeX() < vid.height ) y_tiles++;

	int x_tiles= r_newrefdef.x / tex->OriginalSizeX();
	if( x_tiles * tex->OriginalSizeX() < r_newrefdef.x ) x_tiles++;

	for( int y= 0; y< y_tiles; y++ )
	{
		for( int x= 0; x< x_tiles; x++ )
		{
			((int*)buff)[0]=	 x * tex->OriginalSizeX();
			((int*)buff)[1]=	 y * tex->OriginalSizeY();
			((int*)buff)[2]= (x+1) * tex->OriginalSizeX();
			((int*)buff)[3]= (y+1) * tex->OriginalSizeY();
			((int*)buff)[4]= 65536;
			 call->sprite_count++;
			 buff+= 5 * sizeof(int);
		}
		int x_shift= ( r_newrefdef.x + r_newrefdef.width );
		for( int x= 0; x< x_tiles; x++ )
		{
			((int*)buff)[0]=	 x * tex->OriginalSizeX() + x_shift;
			((int*)buff)[1]=	 y * tex->OriginalSizeY();
			((int*)buff)[2]= (x+1) * tex->OriginalSizeX() + x_shift;
			((int*)buff)[3]= (y+1) * tex->OriginalSizeY();
			((int*)buff)[4]= 65536;
			 call->sprite_count++;
			 buff+= 5 * sizeof(int);
		}
	}

	x_tiles= r_newrefdef.width / tex->OriginalSizeX();
	if( x_tiles * tex->OriginalSizeX() < r_newrefdef.width ) x_tiles++;

	y_tiles= r_newrefdef.y / tex->OriginalSizeY();
	if( y_tiles * tex->OriginalSizeY() < r_newrefdef.y ) y_tiles++;

	for( int y= 0; y< y_tiles; y++ )
	{
		int x_shift= r_newrefdef.x;
		for( int x= 0; x< x_tiles; x++ )
		{
			((int*)buff)[0]=	 x * tex->OriginalSizeX() + x_shift;
			((int*)buff)[1]=	 y * tex->OriginalSizeY();
			((int*)buff)[2]= (x+1) * tex->OriginalSizeX() + x_shift;
			((int*)buff)[3]= (y+1) * tex->OriginalSizeY();
			((int*)buff)[4]= 65536;
			 call->sprite_count++;
			 buff+= 5 * sizeof(int);
		}
		int y_shift= ( r_newrefdef.y + r_newrefdef.height );
		for( int x= 0; x< x_tiles; x++ )
		{
			((int*)buff)[0]=	 x * tex->OriginalSizeX() + x_shift;
			((int*)buff)[1]=	 y * tex->OriginalSizeY() + y_shift;
			((int*)buff)[2]= (x+1) * tex->OriginalSizeX() + x_shift;
			((int*)buff)[3]= (y+1) * tex->OriginalSizeY() + y_shift;
			((int*)buff)[4]= 65536;
			 call->sprite_count++;
			 buff+= 5 * sizeof(int);
		}
	}


	command_buffer.current_pos+= buff - buff0;
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

	unsigned char color[4];
	float init_sprite_radius = float(r_newrefdef.width)/( sin(fov_rad*0.5f) * 256.0f );

	/*command_buffer.current_pos+=
		ComIn_SetTexture( (char*)command_buffer.buffer + command_buffer.current_pos,
		&particle_texture );
	*/

	const float inv_65536= 1.0f / 65536.0f;
	for( int i= 0; i< count; i++ )
	{
		m_Vec3 pos( particles[i].origin[0],particles[i].origin[1], particles[i].origin[2] );
		pos = pos * *mat;
		if( pos.z <= PSR_MIN_ZMIN_FLOAT )
			continue;
		float inv_z = 1.0f / pos.z;

		pos.x= (pos.x * inv_z + vertex_projection.x_add) * vertex_projection.x_mul * inv_65536;
		pos.y= (pos.y * inv_z + vertex_projection.y_add) * vertex_projection.y_mul * inv_65536;
		if( pos.x <= vertex_projection.x_min || pos.x >= vertex_projection.x_max ||
			pos.y <= vertex_projection.y_min || pos.y >= vertex_projection.y_max )
			continue;

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


void DrawUnderwater()
{
	if( (r_newrefdef.rdflags&RDF_UNDERWATER) == 0 )
		return;

	static int underwater_frame= 0;
	static float prev_fog_color[3];

	float fog_color[3];
	unsigned char fog_color_scaled[4];

	float pos[3];
	VectorCopy( r_newrefdef.vieworg, pos );

	R_LightPoint( r_newrefdef.vieworg, fog_color );

	if( underwater_frame == r_framecount - 1 )
	{
		//blend current underwater fog color with color in previous frame
		float fade_per_second= 0.75f;
		float k = pow( fade_per_second, 1.0f / float(fps_calc.last_fps) );
		for( int i= 0; i< 3; i++ )
			prev_fog_color[i]= prev_fog_color[i] * k + fog_color[i] * (1.0f-k);
	}
	else
	{
		for( int i= 0; i< 3; i++ )
			prev_fog_color[i]= fog_color[i];
	}

	for( int i= 0; i< 3; i++ )
		fog_color_scaled[i]= int( 255.0f * ( prev_fog_color[i] > 1.0f ? 1.0f : prev_fog_color[i] ) );


	command_buffer.current_pos+=
		ComIn_AddExponentialFog( (char*)command_buffer.buffer + command_buffer.current_pos, 6.0f, fog_color_scaled );

	underwater_frame= r_framecount;
}

void DrawFullscreenBlend()
{
	unsigned char blend_color[]= {
			r_newrefdef.blend[0]*255.0f, r_newrefdef.blend[1]*255.0f, r_newrefdef.blend[2]*255.0f, r_newrefdef.blend[3]*255.0f };
		ColorByteSwap( blend_color );

	if( blend_color[3] > 4 )
		command_buffer.current_pos+= ComIn_AlphaBlendColorBuffer(
			(char*)command_buffer.buffer + command_buffer.current_pos,
			blend_color);
}

extern "C" void PANZER_RenderFrame(refdef_t *fd)
{
	r_newrefdef= *fd;
	InitPlayerFlashlight();
	SetWorldFrame( int(r_newrefdef.time*2.0f) );

	BeginSurfFrame();

	//set convertion coefficients
	vertex_projection.x_add= ( 2.0f * float(fd->x) + float(fd->width) ) / float( fd->width);
	vertex_projection.x_mul= float(fd->width)*0.5f * 65536.0f;
	vertex_projection.y_add= ( 2.0f * float(vid.height - fd->y - fd->height) + float(fd->height) ) / float( fd->height);
	vertex_projection.y_mul= float(fd->height)*0.5f * 65536.0f;

	vertex_projection.x_min= float(fd->x);
	vertex_projection.x_max= float(fd->x + fd->width);
	vertex_projection.y_min= float(vid.height - fd->y - fd->height);
	vertex_projection.y_max= float(vid.height - fd->y);


	//clear depth buffer
	command_buffer.current_pos+= ComIn_ClearDepthBuffer(
		(char*)command_buffer.buffer + command_buffer.current_pos, 0x0000 );

	//clear color buffer ( if need )
	if( r_clear_color_buffer->value /*|| (fd->rdflags&RDF_NOWORLDMODEL) != 0*/ )
	{
		static const unsigned char clear_color[]= { 255, 0, 255, 0 };
		command_buffer.current_pos+= ComIn_ClearColorBuffer(
			(char*)command_buffer.buffer + command_buffer.current_pos, clear_color );
	}

	//set time, palette
	command_buffer.current_pos+= ComIn_SetTexturePaletteRaw(
		(char*)command_buffer.buffer + command_buffer.current_pos, (unsigned char*)d_8to24table );
	command_buffer.current_pos+= ComIn_SetConstantTime(
		(char*)command_buffer.buffer + command_buffer.current_pos, int(r_newrefdef.time*65536.0f*64.0f) );


	//calculate matrices
	m_Mat4 pers, rot_x, rot_y, rot_z, result, scale, basis_change, shift, normal_mat, sky_mat, sky_rot, world_normals_to_view_space_mat;

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
	world_normals_to_view_space_mat= rot_z * rot_x * rot_y * basis_change;
	SetFov(fd->fov_y * to_rad);


	rot_x.RotateX( -fd->viewangles[0] * to_rad );
	rot_z.RotateZ( -(fd->viewangles[1]  + 90.0f )* to_rad );
	rot_y.RotateY( -fd->viewangles[2] * to_rad );
	normal_mat= rot_y * rot_x * rot_z;

	//back tiles
	DrawTiles();

	//world
	if( (fd->rdflags&RDF_NOWORLDMODEL) == 0 )
	{
		InitFrustrumClipPlanes(&normal_mat, r_newrefdef.vieworg);
		BuildSurfaceLists(&result, fd->vieworg);

		SetSurfaceMatrix(&result, &world_normals_to_view_space_mat);
		//DrawWorldTextureChains();
		DrawWorldSurfaces();
	}

	//solid entities
	InitBrushEntitiesRenderingParameters();
	SetSurfaceMatrix(&result, &normal_mat);
	DrawEntities(&result, &normal_mat, fd->vieworg, false );


	if( (fd->rdflags&RDF_NOWORLDMODEL) == 0 )
	{
		//sky
		static float sky_pos[]= { 0.0f, 0.0f, 0.0f };
		InitFrustrumClipPlanes(&normal_mat, sky_pos);
		DrawSkyBox(&sky_mat, fd->vieworg);

		//alpha surfaces
		InitFrustrumClipPlanes(&normal_mat, r_newrefdef.vieworg);
		SetSurfaceMatrix(&result, &world_normals_to_view_space_mat);
		DrawWorldAlphaSurfaces();
	}

	//alpha entities
	SetSurfaceMatrix(&result, &world_normals_to_view_space_mat);
	DrawEntities(&result, &normal_mat, fd->vieworg, true );

	//particles
	if( (fd->rdflags&RDF_NOWORLDMODEL) == 0 )
		DrawParticles( &result,  fd->particles, fd->num_particles, fd->fov_y * to_rad );

	DrawFullscreenBlend();
	DrawUnderwater();

	//}
	/*else
	{
		unsigned char clear_color[]= { 128, 32, 128, 32 };
	command_buffer.current_pos+= ComIn_ClearColorBuffer(
		(char*)command_buffer.buffer + command_buffer.current_pos, clear_color );
	}*/

	//if( use_multithreading)
	//	DrawCharString(8, 32, "multithreading" );
}
