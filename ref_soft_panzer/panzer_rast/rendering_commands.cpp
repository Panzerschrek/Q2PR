#include "psr.h"
#include "rendering_commands.h"
#include "texture.h"
extern "C" void PRast_ClearColorBuffer( const unsigned char* color );
extern "C" void PRast_ClearDepthBuffer( depth_buffer_t value );
extern "C" void PRast_AlphaBlendColorBuffer( const unsigned char* color );
extern "C" void PRast_MakeGammaCorrection( const unsigned char* gamma_table );
extern "C" void PRast_SwapRedBlueLostAlphaInFramebuffer();
extern "C" void PRast_MirrorFramebufferVertical();
extern "C" void PRast_AddFullscreenExponentialFog( float half_distance, const unsigned char* color );

namespace Draw
{
void SetConstantAlpha( unsigned char a );
void SetConstantBlendFactor( unsigned char b );
void SetConstantLight( int l );
void SetConstantColor( const unsigned char* color );
void SetConstantTime( fixed16_t time );

void SetTexture( const Texture* t );
void SetTextureLod( const Texture* t, int lod );
void SetTextureRaw( const unsigned char* data, int size_x_log2, int size_y_log2 );
void SetTexturePalette( const Texture* t );
void SetTexturePaletteRaw( const unsigned char* palette );
void SetLightmap( const unsigned char* lightmap_data, int width );

}

/*
in functions return byte count of command data + length of command code
out functions return byte count of command parameters
*/


/*
-----in state change functions----------
*/
unsigned int ComIn_ClearColorBuffer( void* command_buffer, const unsigned char* color )
{
	*((int*)command_buffer)= COMMAND_CLEAR_COLOR_BUFFER;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((int*)command_buffer)= *((int*)color);
	return sizeof(int)*2;
}
unsigned int ComIn_ClearDepthBuffer( void* command_buffer, depth_buffer_t value )
{
	*((int*)command_buffer)= COMMAND_CLEAR_DEPTH_BUFFER;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((short*)command_buffer)= value;
	return sizeof(int)*2;
}
unsigned int ComIn_AlphaBlendColorBuffer( void* command_buffer, const unsigned char* color )
{
	*((int*)command_buffer)= COMMAND_ALPHA_BLEND_COLOR_BUFFER;
	*((int*)command_buffer + 1)= *((int*)color);
	return 4 + sizeof(int);
}

unsigned int ComIn_GammaCorrectColorBuffer( void* command_buffer, const unsigned char* gamma_table )
{
	*((int*)command_buffer)= COMMAD_GAMMA_CORRECT_COLOR_BUFFER;
	command_buffer = ((char*)command_buffer) + sizeof(int);
	*((const unsigned char**)command_buffer)= gamma_table;//put pointer

	return sizeof(int) + sizeof(unsigned char*);
}
unsigned int ComIn_SwapRedBlueInColorBuffer( void* command_buffer )
{
	*((int*)command_buffer)= COMMAND_SWAP_RED_BLUE_IN_COLOR_BUFFER;
	return sizeof(int);
}

unsigned int ComIn_MirrorVerticalColorBuffer( void* command_buffer )
{
	*((int*)command_buffer)= COMMAND_MIRROR_VERTICAL_COLOR_BUFFER;
	return sizeof(int);
}

unsigned int ComIn_AddExponentialFog( void* command_buffer, float half_distance, const unsigned char* color )
{
	*((int*)command_buffer)= COMMAND_ADD_EXPONENTIAL_FOG;
	command_buffer= ((int*)command_buffer) + 1;

	*((float*)command_buffer)= half_distance;
	command_buffer= ((int*)command_buffer) + 1;

	*((int*)command_buffer)= *((int*)color);

	return sizeof(int) + sizeof(float) + 4;
}


unsigned int ComIn_SetConstantAlpha( void* command_buffer, unsigned char alpha )
{
	*((int*)command_buffer)= COMMAND_SET_CONSTANT_ALPHA;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((unsigned char*)command_buffer)= alpha;
	return sizeof(int)*2;
}
unsigned int ComIn_SetConstantBlendFactor( void* command_buffer, unsigned char blend_factor )
{
	*((int*)command_buffer)= COMMAND_SET_CONSTANT_BLEND_FACTOR;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((unsigned char*)command_buffer)= blend_factor;
	return sizeof(int)*2;
}
unsigned int ComIn_SetConstantLight( void* command_buffer, int l )
{
	*((int*)command_buffer)= COMMAND_SET_CONSTANT_LIGHT;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((int*)command_buffer)= l;
	return sizeof(int)*2;
}
unsigned int ComIn_SetConstantColor( void* command_buffer, const unsigned char* color )
{
	*((int*)command_buffer)= COMMAND_SET_CONSTANT_COLOR;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((int*)command_buffer)= *((int*)color);
	return sizeof(int)*2;
}

unsigned int ComIn_SetConstantTime( void* command_buffer, fixed16_t time )
{
	*((int*)command_buffer)= COMMAND_SET_CONSTANT_TIME;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((int*)command_buffer)= time;
	return sizeof(int)*2;
}
unsigned int ComIn_SetTexture( void* command_buffer, const Texture* texture )
{
	*((int*)command_buffer)= COMMAND_SET_TEXTURE;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((const Texture**)command_buffer)= texture;
	return sizeof(Texture*) + sizeof(int);
}
unsigned int ComIn_SetTextureLod( void* command_buffer, const Texture* texture, int lod )
{
	((int*)command_buffer)[0]= COMMAND_SET_TEXTURE_LOD;
	((int*)command_buffer)[1]= lod; 
	command_buffer= ( (int*)command_buffer ) + 2;
	*((const Texture**)command_buffer)= texture;
	return sizeof(Texture*) + sizeof(int)*2;
}

unsigned int ComIn_SetTextureRaw( void* command_buffer, const unsigned char* data, int size_x_log2, int size_y_log2 )
{
	((int*)command_buffer)[0]= COMMAND_SET_TEXTURE_RAW;
	command_buffer= ( (int*)command_buffer ) + 1;

	*((const unsigned char**)command_buffer )= data;
	command_buffer= ((char*)command_buffer ) + sizeof(const unsigned char*);

	((short*)command_buffer)[0]= size_x_log2;
	((short*)command_buffer)[1]= size_y_log2;

	return sizeof(int) + sizeof(const unsigned char*) + sizeof(short)*2;
}

unsigned int ComIn_SetTexturePalette( void* command_buffer, const Texture* texture )
{
	*((int*)command_buffer)= COMMAND_SET_TEXTURE_PALETTE;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((const Texture**)command_buffer)= texture;
	return sizeof(Texture*) + sizeof(int);
}
unsigned int ComIn_SetTexturePaletteRaw( void* command_buffer, const unsigned char* palette )
{
	*((int*)command_buffer)= COMMAND_SET_TEXTURE_PALETTE_RAW;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((const unsigned char**)command_buffer)= palette;
	return sizeof(unsigned char*) + sizeof(int);
}

unsigned int ComIn_SetLightmap( void* command_buffer, const unsigned char* lightmap_data, int width )
{
	((int*)command_buffer)[0]= COMMAND_SET_LIGHTMAP;
	command_buffer= ( (int*)command_buffer ) + 1;
	*((const unsigned char**)command_buffer)= lightmap_data;
	command_buffer= ((char**)command_buffer) + 1;
	((int*)command_buffer)[0]= width;

	return sizeof(int) + sizeof(unsigned char*) + sizeof(int);
}

unsigned int ComIn_AddUserDefinedFunc( void* command_buffer, void (*func)(void*), int param_size )
{
	*((int*)command_buffer)= COMMAND_USER_DEFINED;
	command_buffer= ((char*)command_buffer) + 4;

	UserDefinedFuncCall* call= (UserDefinedFuncCall*)command_buffer;
	call->func= func;
	call->data_size= param_size;
	return sizeof(int) + sizeof(UserDefinedFuncCall);
}

/*
------------out functions----------------
*/

unsigned int ComOut_ClearColorBuffer( const void* command_buffer )
{
	PRast_ClearColorBuffer( (unsigned char*) command_buffer );
	return 4;//command has 4 byes in parameters
}
unsigned int ComOut_ClearDepthBuffer( const void* command_buffer )
{
	PRast_ClearDepthBuffer( *( (unsigned short*) command_buffer ) );
	return 4;//command has 4 byes in parameters ( really 2 bytes, but alignment )
}

unsigned int ComOut_AlphaBlendColorBuffer( const void* command_buffer )
{
	PRast_AlphaBlendColorBuffer( (unsigned char*)command_buffer );
	return 4;
}
unsigned int ComOut_FadeColorBuffer( const void* command_buffer )
{
	DrawFadeScreenCall* call= (DrawFadeScreenCall*)command_buffer;
	call->func();
	return sizeof(DrawFadeScreenCall);
}
unsigned int ComOut_GammaCorrectColorBuffer( const void* command_buffer )
{
	PRast_MakeGammaCorrection( *((unsigned char**)command_buffer) );
	return sizeof(unsigned char*);
}

unsigned int ComOut_SwapRedBlueInColorBuffer( const void* command_buffer )
{
	PRast_SwapRedBlueLostAlphaInFramebuffer();
	return 0;
}
unsigned int ComOut_MirrorVerticalColorBuffer( const void* command_buffer )
{
	PRast_MirrorFramebufferVertical();
	return 0;
}

unsigned int ComOut_AddExponentialFog( const void* command_buffer )
{
	PRast_AddFullscreenExponentialFog( *((float*)command_buffer), ((unsigned char*)command_buffer + 4) );
	return sizeof(float)+4;
}

unsigned int ComOut_SetConstantAlpha( const void* command_buffer )
{
	Draw::SetConstantAlpha( *((unsigned char*)command_buffer) );
	return 4;
}
unsigned int ComOut_SetConstantBlendFactor( const void* command_buffer )
{
	Draw::SetConstantBlendFactor( *((unsigned char*)command_buffer) );
	return 4;
}
unsigned int ComOut_SetConstantLight( const void* command_buffer )
{
	Draw::SetConstantLight( *((int*)command_buffer) );
	return 4;
}
unsigned int ComOut_SetConstantColor( const void* command_buffer )
{
	Draw::SetConstantColor( ((unsigned char*)command_buffer) );
	return 4;
}
unsigned int ComOut_SetConstantTime( const void* command_buffer )
{
	Draw::SetConstantTime( *((int*)command_buffer) );
	return 4;
}

unsigned int ComOut_SetTexture( const void* command_buffer )
{
	Draw::SetTexture( *((Texture**)command_buffer) );
	return sizeof(Texture*);
}
unsigned int ComOut_SetTextureLod( const void* command_buffer )
{
	int lod= ((int*)command_buffer)[0];
	command_buffer= ((int*)command_buffer) + 1;
	Draw::SetTextureLod( *((Texture**)command_buffer), lod );
	return sizeof(Texture*) + sizeof(int);
}

unsigned int ComOut_SetTextureRaw( const void* command_buffer )
{
	const unsigned char* data= *((const unsigned char**)command_buffer);
	command_buffer= ((char*)command_buffer) + sizeof(const unsigned char*);

	Draw::SetTextureRaw( data, ((short*)command_buffer)[0], ((short*)command_buffer)[1] );

	return sizeof(const unsigned char*) + sizeof(short)*2;
}

unsigned int ComOut_SetTexturePalette( const void* command_buffer )
{
	Draw::SetTexturePalette( *((Texture**)command_buffer) );
	return sizeof(Texture*);
}
unsigned int ComOut_SetTexturePaletteRaw( const void* command_buffer )
{
	const unsigned char* palette= *( (const unsigned char**)command_buffer );
	Draw::SetTexturePaletteRaw( palette );
	return sizeof(unsigned char*);
}
unsigned int ComOut_SetLightmap( const void* command_buffer )
{
	const unsigned char* lightmap_data= *((const unsigned char**)command_buffer);
	command_buffer= ((unsigned char**)command_buffer) + 1;
	Draw::SetLightmap( lightmap_data, *((int*)command_buffer) );
	return sizeof(int) + sizeof(unsigned char*);
}

unsigned int ComOut_DrawTriangle( const void* command_buffer )
{
	DrawTriangleCall* call= (DrawTriangleCall*) command_buffer;
	char* buff= (char*) command_buffer;
	buff+= sizeof(DrawTriangleCall);

	for( int i= 0; i< call->triangle_count; i++ )
	{
		call->DrawFromBufferFunc( buff );
		buff+= call->vertex_size * 4;
	}
	return sizeof(DrawTriangleCall) + call->vertex_size * call->triangle_count * 4;
}

unsigned int ComOut_DrawLine( const void* command_buffer )
{
	DrawLineCall* call= (DrawLineCall*) command_buffer;
	char* buff= (char*) command_buffer;
	buff+= sizeof(DrawLineCall);

	for( int i= 0; i< call->line_count; i++ )
	{
		call->DrawFromBufferFunc( buff );
		buff+= call->vertex_size * 4;
	}
	return sizeof(DrawTriangleCall) + call->vertex_size * call->line_count * 2;
}

unsigned int ComOut_DrawSprite( const void* command_buffer )
{
	DrawSpriteCall* call= (DrawSpriteCall*)command_buffer;
	int* buff= (int*) ( (char*)command_buffer + sizeof(DrawSpriteCall) );
	for( int i= 0; i< call->sprite_count; i++ )
	{
		call->DrawSpriteFunc( buff[0], buff[1], buff[2], buff[3], buff[4] );
		buff+= 5;
	}
	return sizeof(DrawSpriteCall) + 5 * sizeof(int) * call->sprite_count;
}

unsigned int ComOut_DrawTexture( const void* command_buffer )
{
	DrawTextureCall* call= (DrawTextureCall*)command_buffer;
	int* buff= (int*) ( (char*)command_buffer + sizeof(DrawTextureCall) );
	for( int i= 0; i< call->draw_count; i++ )
	{
		call->DrawTextureFunc( buff[0], buff[1] );
		buff+=2;
	}
	return sizeof(DrawTextureCall) + call->draw_count * 2 * sizeof(int);
}

unsigned int ComOut_DrawTextureRect( const void* command_buffer )
{
	DrawTextureRectCall* call= (DrawTextureRectCall*)command_buffer;
	int* buff= (int*) ( (char*)command_buffer + sizeof(DrawTextureCall) );
	for( int i= 0; i< call->draw_count; i++ )
	{
		call->DrawTextureRectFunc( buff[0], buff[1], buff[2], buff[3], buff[4], buff[5] );
		buff+=6;
	}
	return sizeof(DrawTextureRectCall) + call->draw_count * 4 * sizeof(int);
}

unsigned int ComOut_CallUserDefineFunc( const void* command_buffer )
{
	UserDefinedFuncCall* call= (UserDefinedFuncCall*)command_buffer;
	call->func( (char*)command_buffer + sizeof(UserDefinedFuncCall) );
	return sizeof(UserDefinedFuncCall) + call->data_size;
}

static unsigned int (*Commands[])(const void*)= { 
ComOut_ClearColorBuffer,
ComOut_ClearDepthBuffer,
ComOut_AlphaBlendColorBuffer,
ComOut_FadeColorBuffer,
ComOut_GammaCorrectColorBuffer,
ComOut_SwapRedBlueInColorBuffer,
ComOut_MirrorVerticalColorBuffer,
ComOut_AddExponentialFog,

ComOut_SetConstantAlpha,
ComOut_SetConstantBlendFactor,
ComOut_SetConstantLight,
ComOut_SetConstantColor,
ComOut_SetConstantTime,

ComOut_SetTexture,
ComOut_SetTextureLod,
ComOut_SetTextureRaw,
ComOut_SetTexturePalette,
ComOut_SetTexturePaletteRaw,
ComOut_SetLightmap,

ComOut_DrawTriangle,
ComOut_DrawLine,
ComOut_DrawSprite,
ComOut_DrawTexture,
ComOut_DrawTextureRect,

ComOut_CallUserDefineFunc

};
static const unsigned int command_count= sizeof(Commands) / sizeof( unsigned int(*)(void*) ) ;


void ComOut_DoCommands( const void* command_buffer, unsigned int buffer_size )
{
	const void* command_buffer_end= (char*)command_buffer + buffer_size;

	while( command_buffer < command_buffer_end )
	{
		int command_id= *((int*)command_buffer);
		command_buffer= (char*)command_buffer + sizeof(int);
		if( command_id >= command_count || command_id< 0 )//fatal error, unknown command
			return;

		command_buffer= (char*)command_buffer + 
			Commands[ command_id ] ( command_buffer );
	}
}

//returns new buffer
void* Com_ResizeCommandBuffer( const void* buffer, unsigned int old_size, unsigned int desired_size, unsigned int* out_new_buffer_size )
{
	unsigned int new_size= desired_size + (desired_size>>2);//+25%
	if( new_size < PSR_MIN_COMMAND_BUFFER_SIZE )
		new_size= PSR_MIN_COMMAND_BUFFER_SIZE;//smallest command buffer size

	void* new_buff= new unsigned char[ new_size ];

	if( buffer != NULL )
		memcpy( new_buff, buffer, old_size < new_size ? old_size : new_size );

	*out_new_buffer_size= new_size;
	delete[] buffer;
	return new_buff;
}
