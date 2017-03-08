#ifndef RENDERING_COMMANDS_H
#define RENDERING_COMMANDS_H

class Texture;

enum RenderingCommands
{
	//fullscreen buffer commands
	COMMAND_CLEAR_COLOR_BUFFER=0,
	COMMAND_CLEAR_DEPTH_BUFFER,
	COMMAND_ALPHA_BLEND_COLOR_BUFFER,
	COMMAND_FADE_COLOR_BUFFER,
	COMMAD_GAMMA_CORRECT_COLOR_BUFFER,
	COMMAND_SWAP_RED_BLUE_IN_COLOR_BUFFER,
	COMMAND_MIRROR_VERTICAL_COLOR_BUFFER,
	COMMAND_ADD_EXPONENTIAL_FOG,

	//state change commands
	COMMAND_SET_CONSTANT_ALPHA,
	COMMAND_SET_CONSTANT_BLEND_FACTOR,
	COMMAND_SET_CONSTANT_LIGHT,
	COMMAND_SET_CONSTANT_COLOR,
	COMMAND_SET_CONSTANT_TIME,

	//texture and lightmap change
	COMMAND_SET_TEXTURE,
	COMMAND_SET_TEXTURE_LOD,
	COMMAND_SET_TEXTURE_RAW,
	COMMAND_SET_TEXTURE_PALETTE,
	COMMAND_SET_TEXTURE_PALETTE_RAW,
	COMMAND_SET_LIGHTMAP,

	//draw commands with primitives
	COMMAND_DRAW_TRIANGLE,
	COMMAND_DRAW_LINE,
	COMMAND_DRAW_SPRITE,
	COMMAND_DRAW_TEXTURE,
	COMMAND_DRAW_TEXTURE_RECT,

	COMMAND_USER_DEFINED

};

struct DrawTriangleCall
{
	void (*DrawFromBufferFunc)(char*);
	short vertex_size;
	short triangle_count;
};

struct DrawLineCall
{
	void (*DrawFromBufferFunc)(char*);
	short vertex_size;
	short line_count;
};

struct DrawSpriteCall
{
	void (*DrawSpriteFunc)( int, int, int, int, fixed16_t );
	int sprite_count;
};

struct DrawTextureCall
{
	void (*DrawTextureFunc)( int, int );
	int draw_count;
};

struct DrawTextureRectCall
{
	void (*DrawTextureRectFunc)( int, int, int, int, int, int );
	int draw_count;
};

struct DrawFadeScreenCall
{
	void (*func)(void);
};

struct UserDefinedFuncCall
{
	void (*func)(void*);
	int data_size;
};


extern "C"
{

//functions write to command buffer commands and return length of command ( command code + papameters )
unsigned int ComIn_ClearColorBuffer( void* command_buffer, const unsigned char* color );
unsigned int ComIn_ClearDepthBuffer( void* command_buffer, depth_buffer_t value );
unsigned int ComIn_AlphaBlendColorBuffer( void* command_buffer, const unsigned char* color );
//fade color buffer - no IN func
unsigned int ComIn_GammaCorrectColorBuffer( void* command_buffer, const unsigned char* gamma_table );
unsigned int ComIn_SwapRedBlueInColorBuffer( void* command_buffer );
unsigned int ComIn_MirrorVerticalColorBuffer( void* command_buffer );
unsigned int ComIn_AddExponentialFog( void* command_buffer, float half_distance, const unsigned char* color );

unsigned int ComIn_SetConstantAlpha( void* command_buffer, unsigned char alpha );
unsigned int ComIn_SetConstantBlendFactor( void* command_buffer, unsigned char blend_factor );
unsigned int ComIn_SetConstantLight( void* command_buffer, int l );
unsigned int ComIn_SetConstantColor( void* command_buffer, const unsigned char* color );
unsigned int ComIn_SetConstantTime( void* command_buffer, fixed16_t time );

unsigned int ComIn_SetTexture( void* command_buffer, const Texture* texture );
unsigned int ComIn_SetTextureLod( void* command_buffer, const Texture* texture, int lod );
unsigned int ComIn_SetTextureRaw( void* command_buffer, const unsigned char* data, int size_x_log2, int size_y_log2 ); 
unsigned int ComIn_SetTexturePalette( void* command_buffer, const Texture* texture );
unsigned int ComIn_SetTexturePaletteRaw( void* command_buffer, const  unsigned char* palette );
unsigned int ComIn_SetLightmap( void* command_buffer, const unsigned char* lightmap_data, int width );

unsigned int ComIn_AddUserDefinedFunc( void* command_buffer, void (*func)(void*), int param_size );

void ComOut_DoCommands( const void* command_buffer, unsigned int buffer_size );


//returns new buffer
void* Com_ResizeCommandBuffer( const void* buffer, unsigned int old_size, unsigned int desired_size, unsigned int* out_new_buffer_size );


}//extern "C"


#endif//RENDERING_COMMANDS_H