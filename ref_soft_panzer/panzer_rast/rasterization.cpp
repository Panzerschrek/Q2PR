﻿#include <math.h>
#include <stdlib.h>
#include "rendering_state.h"
#include "fixed.h"
#include "fast_math.h"
#include "texture.h"
#include "psr.h"
#include "psr_mmx.h"
#include "rasterization.h"

void* framebuffer_data= NULL;//raw pointer to allocated framebuffer data. 
unsigned char* screen_buffer= NULL;//main screen color buffer format - RGBA
unsigned char* back_screen_buffer= NULL;
depth_buffer_t* depth_buffer= NULL;
int screen_size_x, screen_size_y;



void PRast_Init( int screen_width, int screen_height )
{

    screen_size_x= screen_width;
    screen_size_y= screen_height;

    if( framebuffer_data != NULL )
		delete[] framebuffer_data;

	int color_buffer_size= screen_size_x * screen_size_y * 4;
	color_buffer_size+= (-color_buffer_size)&(PSR_FRAMEBUFFER_ALIGNMENT-1);

	int depth_buffer_size= screen_size_x * screen_size_y * sizeof(depth_buffer_t);
	depth_buffer_size+= (-depth_buffer_size)&(PSR_FRAMEBUFFER_ALIGNMENT-1);

	framebuffer_data= new char[ 2 * color_buffer_size + depth_buffer_size + PSR_FRAMEBUFFER_ALIGNMENT * 6 ];
	void* aligned_framebuffer_data= 
		(char*)framebuffer_data + ( (-((int)framebuffer_data)) & (PSR_FRAMEBUFFER_ALIGNMENT-1) );

    depth_buffer= (depth_buffer_t*)aligned_framebuffer_data;
    screen_buffer= ( (unsigned char*)depth_buffer) + depth_buffer_size + PSR_FRAMEBUFFER_ALIGNMENT;
	screen_buffer += (-((int)screen_buffer)) & (PSR_FRAMEBUFFER_ALIGNMENT-1);
	back_screen_buffer= screen_buffer + color_buffer_size + PSR_FRAMEBUFFER_ALIGNMENT;
	back_screen_buffer += (-((int)back_screen_buffer)) & (PSR_FRAMEBUFFER_ALIGNMENT-1);
}

void PRast_Shutdown()
{
	if( framebuffer_data != NULL )
		delete[] framebuffer_data;
}

void PRast_SetFramebuffer( unsigned char* buff )
{
	screen_buffer= buff;
}
unsigned char* PRast_GetFrontBuffer()
{
	return screen_buffer;
}


extern "C" unsigned char* PRast_SwapBuffers()
{
	unsigned char* tmp= screen_buffer;
	screen_buffer= back_screen_buffer;
	back_screen_buffer= tmp;
	return back_screen_buffer;
}


namespace Draw
{


//fast sintable for fast sin ( or not fast )
const int sintable_size= 128;
const int sintable_convertion_k= (((long int) sintable_size )*0x100000000)  / PSR_2PI_FIXED16; //=sintable_size/(2*pi)

static fixed16_t sintable[ sintable_size ];
void InitSinTable()
{
    for( unsigned int i= 0; i< sintable_size; i++ )
        sintable[i]= fixed16_t( 65536.0f * sin( float(i) * ( PSR_2PI / float(sintable_size) ) ) );
}
fixed16_t FastSin( fixed16_t x )
{
    return sintable[ (sintable_size-1) & Fixed16MulResultToInt( x, sintable_convertion_k ) ];
}
fixed16_t FastCos( fixed16_t x )
{
    return sintable[ ( (sintable_size-1) & ( Fixed16MulResultToInt( x, sintable_convertion_k ) ) + (sintable_size/4) ) ];
}

/*
interpolate u/z, v/z, color/z, 1/z. Finaly,divide it by ( 1/z ) to calculate final values
*/


//texture data
const unsigned char* current_texture_data;
const unsigned char* current_texture_data_lods[ PSR_MAX_TEXTURE_SIZE_LOG2+1 ];
const unsigned char* current_texture_palette;//if texture palleted
int current_texture_size_x, current_texture_size_y;
int current_texture_size_x_log2, current_texture_size_y_log2;
int current_texture_size_x1, current_texture_size_y1; //x-1 and y-1
int current_texture_max_lod;
//lightmap data
const unsigned char* current_lightmap_data;
int current_lightmap_size_x, current_lightmap_size_y;
int current_lightmap_size_x_log2, current_lightmap_size_y_log2;
int current_lightmap_size_x1, current_lightmap_size_y1; //x-1 and y-1

//rendering state
int current_time;//for animated textures, etc.
unsigned char constant_alpha;
unsigned char constant_blend_factor;
unsigned char inv_constant_blend_factor;
int constant_light;
unsigned char constant_color[4];

void SetConstantAlpha( unsigned char a )
{
    constant_alpha= a;
}
void SetConstantBlendFactor( unsigned char b )
{
    constant_blend_factor= b;
    inv_constant_blend_factor= 255 - b;
}
void SetConstantLight( int l )
{
    constant_light= l;
}
void SetConstantColor( const unsigned char* color )
{
    Byte4Copy( constant_color, color );
}

void SetTexture( const Texture* t )
{
    current_texture_data= t->GetData();
    current_texture_size_x= t->SizeX();
    current_texture_size_y= t->SizeY();
    current_texture_size_x1= current_texture_size_x-1;
    current_texture_size_y1= current_texture_size_y-1;
    current_texture_size_x_log2= t->SizeXLog2();
    current_texture_size_y_log2= t->SizeYLog2();
}

void SetTextureLod( const Texture* t, int lod )
{
    /*current_texture_max_lod= t->MaxLod();
    int i;
    for( i= 0; i<= current_texture_max_lod; i++ )
        current_texture_data_lods[i]= t->GetLodData(i);
    for( ; i<= PSR_MAX_TEXTURE_SIZE_LOG2; i++ )
        current_texture_data_lods[i]= current_texture_data_lods[ current_texture_max_lod ];*/

	current_texture_data= t->GetLodData(lod);
    current_texture_size_x= t->SizeX()>>lod;
    current_texture_size_y= t->SizeY()>>lod;
    current_texture_size_x1= current_texture_size_x-1;
    current_texture_size_y1= current_texture_size_y-1;
    current_texture_size_x_log2= t->SizeXLog2() - lod;
    current_texture_size_y_log2= t->SizeYLog2() - lod;
}



void SetTexturePalette( const Texture* t )
{
	current_texture_palette= t->GetPalette();
}
void SetTexturePaletteRaw( const unsigned char* palette )
{
	current_texture_palette= palette;
}

void SetLightmap( const unsigned char* lightmap_data, int width )
{
	current_lightmap_data= lightmap_data;
	current_lightmap_size_x= width;
}

inline void TexelFetchNearest( int u, int v, unsigned char* out_color )
{
    int x, y;
    x= u & current_texture_size_x1;
    y= v & current_texture_size_y1;
    int s= x | ( y << current_texture_size_x_log2 );
    s<<= 2;
    Byte4Copy( out_color, current_texture_data + s );
}

inline void TexelFetchNearestNoWarp( int u, int v, unsigned char* out_color )
{
	int s= u + v * current_texture_size_x;
    s<<= 2;
    Byte4Copy( out_color, current_texture_data + s );
}

inline void TexelFetchNearestPlaettized( int u, int v, unsigned char* out_color )
{
    int x, y;
    x= u & current_texture_size_x1;
    y= v & current_texture_size_y1;
    int s= x | ( y << current_texture_size_x_log2 );
    int color_index= current_texture_data[s];

    Byte4Copy( out_color, current_texture_palette + (color_index<<2) );
}
inline void TexelFetchNearestPlaettizedNoWarp( int u, int v, unsigned char* out_color )
{
    int s= u + v * current_texture_size_x;
    int color_index= current_texture_data[s];
    Byte4Copy( out_color, current_texture_palette + (color_index<<2) );
}

inline void TexelFetchNearestMipmap( int u, int v, int lod, unsigned char* out_color )
{
    int x, y;
    //lod= FastIntClampToZero( lod );
	lod= FastIntClamp( 0, current_texture_max_lod, lod );
    x= ( u & current_texture_size_x1 )>> lod;
    y= ( v & current_texture_size_y1 )>> lod;
    int s= x | (  y << ( current_texture_size_x_log2 >> lod )  );
    s<<= 2;
    Byte4Copy( out_color, current_texture_data_lods[ lod ] +  s );
}


inline void TexelFetchNearestMipmapNoWarp( int u, int v, int lod, unsigned char* out_color )
{
    int x, y;
    lod= FastIntClampToZero( lod );
    x= u >> lod;
    y= v >> lod;
	int s= x + y * (current_texture_size_x>>lod);
    s<<= 2;
    Byte4Copy( out_color, current_texture_data_lods[ lod ] +  s );
}



inline void TexelFetchNearestMipmapPlaettized( int u, int v, int lod, unsigned char* out_color )
{
    int x, y;
    lod= FastIntClampToZero( lod );
    x= ( u & current_texture_size_x1 )>> lod;
    y= ( v & current_texture_size_y1 )>> lod;
    int s= x | ( ( y << current_texture_size_x_log2 >> lod ) );
    const unsigned char* tex_data= current_texture_data_lods[ lod ];
    int color_index= tex_data[s];
    Byte4Copy( out_color, current_texture_palette + (color_index<<2) );
}

inline void TexelFetchNearestMipmapPlaettizedNowarp( int u, int v, int lod, unsigned char* out_color )
{
    int x, y;
    lod= FastIntClampToZero( lod );
    x= u >> lod;
    y= v >> lod;
    int s= x + y * (current_texture_size_x>>lod);
    const unsigned char* tex_data= current_texture_data_lods[ lod ];
    int color_index= tex_data[s];
    Byte4Copy( out_color, current_texture_palette + (color_index<<2) );
}


inline void TexelFetchRGBSLinear( fixed16_t u, fixed16_t v, unsigned char* out_color )
{
    int x, y, x1, y1;
    x= u>>16;
    y= v>>16;
    x1= x+1;
    y1= y+1;
    fixed16_t dx, dy, dx1, dy1;
    dx= ( u & 0xFFFF )>>8;
    dy= ( v & 0xFFFF )>>8;
    dx1= 256 - dx;
    dy1= 256 - dy;
    x&= current_texture_size_x1;
    y&= current_texture_size_y1;
    x1&= current_texture_size_x1;
    y1&= current_texture_size_y1;

    int s;
    int s_color[4];
    int mixed_s_color[2];

    s= ( x | ( y << current_texture_size_x_log2 ) )<<2;
    out_color[0]= current_texture_data[s+0];
    out_color[1]= current_texture_data[s+1];
    out_color[2]= current_texture_data[s+2];

    s_color[0]= current_texture_data[s+3];
    s= ( ( x1 | ( y << current_texture_size_x_log2 ) )<<2 ) + 3;
    s_color[1]= current_texture_data[s];
    s= ( ( x | ( y1 << current_texture_size_x_log2 ) )<<2 ) + 3;
    s_color[2]= current_texture_data[s];
    s= ( ( x1 | ( y1 << current_texture_size_x_log2 ) )<<2 ) + 3;
    s_color[3]= current_texture_data[s];

    mixed_s_color[0]= s_color[0] * dx1 + s_color[1] * dx;
    mixed_s_color[1]= s_color[2] * dx1 + s_color[3] * dx;

    s= ( mixed_s_color[0] * dy1 + mixed_s_color[1] * dy )>>16;
    out_color[0]= ( out_color[0] * s )>> 8;
    out_color[1]= ( out_color[1] * s )>> 8;
    out_color[2]= ( out_color[2] * s )>> 8;

}

void TexelFetchLinear( fixed16_t u, fixed16_t v, unsigned char* out_color )
{
    int x, y, x1, y1;
    x= u>>16;
    y= v>>16;
    x1= x+1;
    y1= y+1;
    fixed16_t dx, dy, dx1, dy1;
    dx= ( u & 0xFFFF )>>8;//u - (x<<16);
    dy= ( v & 0xFFFF )>>8;//v - (y<<16);
    dx1= 256 - dx;
    dy1= 256 - dy;
    x&= current_texture_size_x1;
    y&= current_texture_size_y1;
    x1&= current_texture_size_x1;
    y1&= current_texture_size_y1;

    int s;
    int colors[16];
    int mixed_colors[8];

    s= x | ( y << current_texture_size_x_log2 );
    s<<= 2;
    colors[ 0]= current_texture_data[s+0];
    colors[ 1]= current_texture_data[s+1];
    colors[ 2]= current_texture_data[s+2];
    colors[ 3]= current_texture_data[s+3];
    s= x1 | ( y << current_texture_size_x_log2 );
    s<<= 2;
    colors[ 4]= current_texture_data[s+0];
    colors[ 5]= current_texture_data[s+1];
    colors[ 6]= current_texture_data[s+2];
    colors[ 7]= current_texture_data[s+3];
    s= x | ( y1 << current_texture_size_x_log2 );
    s<<= 2;
    colors[ 8]= current_texture_data[s+0];
    colors[ 9]= current_texture_data[s+1];
    colors[10]= current_texture_data[s+2];
    colors[11]= current_texture_data[s+3];
    s= x1 | ( y1 << current_texture_size_x_log2 );
    s<<= 2;
    colors[12]= current_texture_data[s+0];
    colors[13]= current_texture_data[s+1];
    colors[14]= current_texture_data[s+2];
    colors[15]= current_texture_data[s+3];

    mixed_colors[0]= ( colors[ 0] * dx1 + colors[ 4] * dx );// >> 16;
    mixed_colors[1]= ( colors[ 1] * dx1 + colors[ 5] * dx );// >> 16;
    mixed_colors[2]= ( colors[ 2] * dx1 + colors[ 6] * dx );// >> 16;
    mixed_colors[3]= ( colors[ 3] * dx1 + colors[ 7] * dx );// >> 16;
    mixed_colors[4]= ( colors[ 8] * dx1 + colors[12] * dx );// >> 16;
    mixed_colors[5]= ( colors[ 9] * dx1 + colors[13] * dx );// >> 16;
    mixed_colors[6]= ( colors[10] * dx1 + colors[14] * dx );// >> 16;
    mixed_colors[7]= ( colors[11] * dx1 + colors[15] * dx );// >> 16;
    out_color[0]= ( mixed_colors[0] * dy1 + mixed_colors[4] * dy ) >> 16;
    out_color[1]= ( mixed_colors[1] * dy1 + mixed_colors[5] * dy ) >> 16;
    out_color[2]= ( mixed_colors[2] * dy1 + mixed_colors[6] * dy ) >> 16;
    out_color[3]= ( mixed_colors[3] * dy1 + mixed_colors[7] * dy ) >> 16;
}

void TexelFetchLinearPlaettized( fixed16_t u, fixed16_t v, unsigned char* out_color )
{
    int x, y, x1, y1;
    x= u>>16;
    y= v>>16;
    x1= x+1;
    y1= y+1;
    fixed16_t dx, dy, dx1, dy1;
    dx= ( u & 0xFFFF )>>8;//u - (x<<16);
    dy= ( v & 0xFFFF )>>8;//v - (y<<16);
    dx1= 256 - dx;
    dy1= 256 - dy;
    x&= current_texture_size_x1;
    y&= current_texture_size_y1;
    x1&= current_texture_size_x1;
    y1&= current_texture_size_y1;

    int s;
    int colors[16];
    int mixed_colors[8];

    s= x | ( y << current_texture_size_x_log2 );
	s= current_texture_data[s]<<2;
    colors[ 0]= current_texture_palette[s+0];
    colors[ 1]= current_texture_palette[s+1];
    colors[ 2]= current_texture_palette[s+2];
    colors[ 3]= current_texture_palette[s+3];
    s= x1 | ( y << current_texture_size_x_log2 );
    s= current_texture_data[s]<<2;
    colors[ 4]= current_texture_palette[s+0];
    colors[ 5]= current_texture_palette[s+1];
    colors[ 6]= current_texture_palette[s+2];
    colors[ 7]= current_texture_palette[s+3];
    s= x | ( y1 << current_texture_size_x_log2 );
	s= current_texture_data[s]<<2;
    colors[ 8]= current_texture_palette[s+0];
    colors[ 9]= current_texture_palette[s+1];
    colors[10]= current_texture_palette[s+2];
    colors[11]= current_texture_palette[s+3];
    s= x1 | ( y1 << current_texture_size_x_log2 );
	s= current_texture_data[s]<<2;
    colors[12]= current_texture_palette[s+0];
    colors[13]= current_texture_palette[s+1];
    colors[14]= current_texture_palette[s+2];
    colors[15]= current_texture_palette[s+3];

    mixed_colors[0]= ( colors[ 0] * dx1 + colors[ 4] * dx );
    mixed_colors[1]= ( colors[ 1] * dx1 + colors[ 5] * dx );
    mixed_colors[2]= ( colors[ 2] * dx1 + colors[ 6] * dx );
    mixed_colors[3]= ( colors[ 3] * dx1 + colors[ 7] * dx );
    mixed_colors[4]= ( colors[ 8] * dx1 + colors[12] * dx );
    mixed_colors[5]= ( colors[ 9] * dx1 + colors[13] * dx );
    mixed_colors[6]= ( colors[10] * dx1 + colors[14] * dx );
    mixed_colors[7]= ( colors[11] * dx1 + colors[15] * dx );
    out_color[0]= ( mixed_colors[0] * dy1 + mixed_colors[4] * dy ) >> 16;
    out_color[1]= ( mixed_colors[1] * dy1 + mixed_colors[5] * dy ) >> 16;
    out_color[2]= ( mixed_colors[2] * dy1 + mixed_colors[6] * dy ) >> 16;
    out_color[3]= ( mixed_colors[3] * dy1 + mixed_colors[7] * dy ) >> 16;
}


inline unsigned char LightmapFetchNearest( int u, int v )
{
	int x= FastIntClampToZero(u);
    int y= FastIntClampToZero(v);
	return current_lightmap_data[ x + y * current_lightmap_size_x ];
}

inline unsigned char LightmapFetchLinear( fixed16_t u, fixed16_t v )
{
    int x, y, x1, y1;
    x= FastIntClampToZero(u>>16);
    y= FastIntClampToZero(v>>16);
	x1= x+1;
	y1= y+1;
    fixed16_t dx, dy, dx1, dy1;
    dx= ( u & 0xFFFF )>>8;
    dy= ( v & 0xFFFF )>>8;
    dx1= 256 - dx;
    dy1= 256 - dy;

    int lights[4];
    int mixed_lights[2];
	lights[0]= current_lightmap_data[ x + y * current_lightmap_size_x ];
	lights[1]= current_lightmap_data[ x1+ y * current_lightmap_size_x ];
	lights[2]= current_lightmap_data[ x + y1* current_lightmap_size_x ];
	lights[3]= current_lightmap_data[ x1+ y1* current_lightmap_size_x ];
    mixed_lights[0]= ( lights[0] * dx1 + lights[1] * dx );
    mixed_lights[1]= ( lights[2] * dx1 + lights[3] * dx );

    return ( mixed_lights[0] * dy1 + mixed_lights[1] * dy ) >> 16;
}

inline void LightmapFetchColoredNearest( int u, int v, unsigned char* out_lightmap )
{
    int x, y;
    //TODO: really need warp lightmap coord?
    x= u & current_lightmap_size_x1;
    y= v & current_lightmap_size_y1;
    int s= x | ( y << current_lightmap_size_x_log2 );
    s<<=2;

    Byte4Copy( out_lightmap, current_lightmap_data + s );
}

inline void LightmapFetchLinearRGBS( fixed16_t u, fixed16_t v, unsigned char* out_lightmap )
{
    int x, y, x1, y1;
    x= FastIntClampToZero(u>>16);
    y= FastIntClampToZero(v>>16);
    x1= x+1;
    y1= y+1;
    fixed16_t dx, dy, dx1, dy1;
    dx= ( u & 0xFFFF )>>8;
    dy= ( v & 0xFFFF )>>8;
    dx1= 256 - dx;
    dy1= 256 - dy;
 
    int s;
    int s_color[4];
    int mixed_s_color[2];

    s= ( x +  y * current_lightmap_size_x )<<2;
	Byte4Copy( out_lightmap, current_lightmap_data + s );//fetch rgb values

    s_color[0]= current_lightmap_data[s+3];
    s= ( ( x1 +  y * current_lightmap_size_x )<<2 ) + 3;
    s_color[1]= current_lightmap_data[s];
    s= ( ( x +  y1 * current_lightmap_size_x )<<2 ) + 3;
    s_color[2]= current_lightmap_data[s];
    s= ( ( x1 + y1 * current_lightmap_size_x )<<2 ) + 3;
    s_color[3]= current_lightmap_data[s];

    mixed_s_color[0]= s_color[0] * dx1 + s_color[1] * dx;
    mixed_s_color[1]= s_color[2] * dx1 + s_color[3] * dx;

    /*s= ( mixed_s_color[0] * dy1 + mixed_s_color[1] * dy )>>16;
    out_lightmap[0]= ( out_lightmap[0] * s )>> 8;
    out_lightmap[1]= ( out_lightmap[1] * s )>> 8;
    out_lightmap[2]= ( out_lightmap[2] * s )>> 8;*/
	s= mixed_s_color[0] * dy1 + mixed_s_color[1] * dy;
	out_lightmap[0]= ( out_lightmap[0] * s )>> 24;
    out_lightmap[1]= ( out_lightmap[1] * s )>> 24;
    out_lightmap[2]= ( out_lightmap[2] * s )>> 24;
}

void LightmapFetchColoredLinear( fixed16_t u, fixed16_t v, unsigned char* out_lightmap )
{
    int x, y, x1, y1;
    x= FastIntClampToZero(u>>16);
    y= FastIntClampToZero(v>>16);
    x1= x+1;
    y1= y+1;
    fixed16_t dx, dy, dx1, dy1;
    dx= ( u & 0xFFFF )>>8;
    dy= ( v & 0xFFFF )>>8;
    dx1= 256 - dx;
    dy1= 256 - dy;

    int s;
    int colors[16];
    int mixed_colors[8];

    s= x + y * current_lightmap_size_x;
    s<<= 2;
    colors[ 0]= current_lightmap_data[s+0];
    colors[ 1]= current_lightmap_data[s+1];
    colors[ 2]= current_lightmap_data[s+2];
    s= x1 + y * current_lightmap_size_x;
    s<<= 2;
    colors[ 4]= current_lightmap_data[s+0];
    colors[ 5]= current_lightmap_data[s+1];
    colors[ 6]= current_lightmap_data[s+2];
    s= x +  y1 * current_lightmap_size_x;
    s<<= 2;
    colors[ 8]= current_lightmap_data[s+0];
    colors[ 9]= current_lightmap_data[s+1];
    colors[10]= current_lightmap_data[s+2];
    s= x1 + y1 * current_lightmap_size_x;
    s<<= 2;
    colors[12]= current_lightmap_data[s+0];
    colors[13]= current_lightmap_data[s+1];
    colors[14]= current_lightmap_data[s+2];

    mixed_colors[0]= ( colors[ 0] * dx1 + colors[ 4] * dx );
    mixed_colors[1]= ( colors[ 1] * dx1 + colors[ 5] * dx );
    mixed_colors[2]= ( colors[ 2] * dx1 + colors[ 6] * dx );

    mixed_colors[4]= ( colors[ 8] * dx1 + colors[12] * dx );
    mixed_colors[5]= ( colors[ 9] * dx1 + colors[13] * dx );
    mixed_colors[6]= ( colors[10] * dx1 + colors[14] * dx );
    out_lightmap[0]= ( mixed_colors[0] * dy1 + mixed_colors[4] * dy ) >> 16;
    out_lightmap[1]= ( mixed_colors[1] * dy1 + mixed_colors[5] * dy ) >> 16;
    out_lightmap[2]= ( mixed_colors[2] * dy1 + mixed_colors[6] * dy ) >> 16;
}


//UNFINISHED
void DrawPixelsPalettized( int x, int y, int width, int height, int rows, int columns, unsigned char* data )
{
	/*int du= (columns<<16) / width;
	int dv= (rows<<16) / height;

	int x_begin= FastIntClampToZero(x);
	int y_begin= FastIntClampToZero(x);
	int x_end= FastIntMin( x, screen_size_x );
	int y_end= FastIntMin( y, screen_size_y );
	int u0= du * ( x_begin - x ); 
	int v0= dv * ( x_begin - x ); 

	for( int y0= y_begin, v= v0; y0< y_end; y0++ , v+=dv )
	{
		int* dst= ((int*)screen_buffer) + x0 + y*screen_size_x;
		char* src= data + u0 + v*;
		for( int x0= x_begin, u=u0; x0< x_end; x0++, u+=du, dst++ )
		{
			Byte4Copy( dst, current_texture_palette + (( current_text)<<2) );
		}
	}*/
}

template< int fade_shift >
void FadeScreen()
{
	int fade_and_table[]={ 
		0xFFFFFFFF, 0x7F7F7F7F, 0x3F3F3F3F, 0x1F1F1F1F, 0x0F0F0F0F, 0x07070707, 0x03030303, 0x01010101, 0x00000000 };
	int fade_and= fade_and_table[ fade_shift ];

	for( int i= 0, i_end= 4 * screen_size_x * screen_size_y; i!= i_end; i+=4 )
	{
		unsigned int c;
		Byte4Copy( &c, screen_buffer + i );
		c>>= fade_shift;
		c&= fade_and;//shift colors and set to zero fade_shift digits
		Byte4Copy( screen_buffer + i, &c );
	}
}

//texture mode can be only TEXTURE_NEAREST or TEXTURE_PALETTIZED_NEAREST
template< enum TextureMode texture_mode >
void DrawTexture( int x, int y )
{
	int x_begin= FastIntClampToZero(x);
	int x_end= FastIntMin( x + current_texture_size_x, screen_size_x );

	int y_begin= FastIntClampToZero(y);
	int y_end= FastIntMin( y + current_texture_size_y, screen_size_y );

	int tc_begin_x= x_begin - x;
	int tc_begin_y= y_begin - y;

	for( int j= y_begin, tc_y= tc_begin_y; j< y_end; j++, tc_y++ )
	{
		unsigned char* texels= screen_buffer + ( ( x_begin + j * screen_size_x )<<2);
		for( int i= x_begin, tc_final= tc_begin_x + (tc_y<<current_texture_size_x_log2) ;
			i< x_end; i++, tc_final++, texels+=4 )
		{
			if( texture_mode == TEXTURE_NEAREST )
				Byte4Copy( texels, current_texture_data + ( tc_final <<2 ) );
			else if( texture_mode == TEXTURE_PALETTIZED_NEAREST )
				Byte4Copy( texels, current_texture_palette + ( current_texture_data[ tc_final ] << 2 ) );
		}
	}
}


template< enum TextureMode texture_mode >
void DrawTextureRect( int x, int y, int u0, int v0, int u1, int v1 )
{
	int x_begin= FastIntClampToZero(x);
	int x_end= FastIntMin( x + (u1-u0), screen_size_x );

	int y_begin= FastIntClampToZero(y);
	int y_end= FastIntMin( y + (v1-v0), screen_size_y );

	int tc_begin_x= x_begin - x + u0;
	int tc_begin_y= y_begin - y + v0;

	for( int j= y_begin, tc_y= tc_begin_y; j< y_end; j++, tc_y++ )
	{
		unsigned char* texels= screen_buffer + ( ( x_begin + j * screen_size_x )<<2);
		for( int i= x_begin, tc_final= tc_begin_x + (tc_y<<current_texture_size_x_log2) ;
			i< x_end; i++, tc_final++, texels+=4 )
		{
			if( texture_mode == TEXTURE_NEAREST )
				Byte4Copy( texels, current_texture_data + ( tc_final <<2 ) );
			else if( texture_mode == TEXTURE_PALETTIZED_NEAREST )
				Byte4Copy( texels, current_texture_palette + ( current_texture_data[ tc_final ] << 2 ) );
		}
	}
}



template<
enum TextureMode texture_mode,
enum BlendingMode blending_mode,
enum AlphaTestMode alpha_test_mode,
enum LightingMode lighting_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawSprite( int x0, int y0, int x1, int y1, fixed16_t sprite_in_depth )
{
    int x_begin= FastIntClampToZero( x0 );
    int x_end= FastIntMin( x1, screen_size_x );
    int y= FastIntClampToZero( y0 );
    int y_end= FastIntMin( y1, screen_size_y );
    fixed16_t u, v, du, dv;
    int lod;
    if( x1 != x0 )
        du= ( current_texture_size_x << 16 ) / ( x1 - x0 );
    if( y1 != y0 )
        dv= ( current_texture_size_y << 16 ) / ( y1 - y0 );

    v= ( y - y0 ) * dv;//Fixed16Mul( ( y - y0 )<<16, * dv );

    if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
		|| texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP ||  texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP )
        lod= FastIntLog2Clamp0( FastIntMax( du, dv ) >> 16 );

    PSR_ALIGN_4 unsigned char color[4];
    unsigned char* pixels;
    depth_buffer_t* depth_p;
    depth_buffer_t depth_z= sprite_in_depth >> PSR_DEPTH_SCALER_LOG2;

    for( ; y< y_end; y++, v+=dv )
    {
        pixels= screen_buffer +(( x_begin + y * screen_size_x )<<2);
        depth_p= depth_buffer + ( x_begin + y * screen_size_x );
        u= ( x_begin - x0 ) * du;
        for( int x= x_begin; x< x_end; x++, pixels+=4, depth_p++, u+=du  )
        {
            if( depth_test_mode != DEPTH_TEST_NONE )
            {
                if( depth_test_mode == DEPTH_TEST_LESS )
                    if( depth_z >= *depth_p )
                        continue;
                if( depth_test_mode == DEPTH_TEST_GREATER )
                    if( depth_z <= *depth_p )
                        continue;
                if( depth_test_mode == DEPTH_TEST_EQUAL )
                    if( depth_z != *depth_p )
                        continue;
                if( depth_test_mode == DEPTH_TEST_NOT_EQUAL )
                    if( depth_z == *depth_p )
                        continue;
            }
            if( texture_mode == TEXTURE_NONE )
            {
               /* color[0]= constant_color[0];
                color[1]= constant_color[1];
                color[2]= constant_color[2];
                color[3]= constant_color[3];*/
				Byte4Copy( color, constant_color );
            }
            else if( texture_mode == TEXTURE_NEAREST )
                TexelFetchNearestNoWarp( u>>16, v>>16, color );
            else if( texture_mode == TEXTURE_LINEAR )
                TexelFetchLinear( u, v, color );
            else if( texture_mode == TEXTURE_NEAREST_MIPMAP )
                TexelFetchNearestMipmapNoWarp( u>>16, v>>16, lod, color );
            else if( texture_mode == TEXTURE_FAKE_FILTER )
            {
                int xx= ((y^x)&1)<<15;
                u+=xx;
                v+=xx;
                TexelFetchNearest( u>>16, v>>16, color );
                u-=xx;
                v-=xx;
            }
            else if( texture_mode == TEXTURE_FAKE_FILTER_MIPMAP )
            {
                int xx= ((y^x)&1)<<15;
                u+=xx;
                v+=xx;
                TexelFetchNearestMipmap( u>>16, v>>16, lod, color );
                u-=xx;
                v-=xx;
            }
            else if( texture_mode == TEXTURE_RGBS_LINEAR )
                TexelFetchRGBSLinear( u, v, color );
			//Plaettized textures
			else if( texture_mode == TEXTURE_PALETTIZED_NEAREST )
				TexelFetchNearestPlaettizedNoWarp( u>>16, v>>16, color );
			else if( texture_mode == TEXTURE_PALETTIZED_LINEAR )
				TexelFetchLinearPlaettized( u, v, color );
			else if( texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER )
            {
                int xx= ((y^x)&1)<<15;
                u+=xx;
                v+=xx;
                TexelFetchNearestPlaettized( u>>16, v>>16, color );
                u-=xx;
                v-=xx;
            }
			else if( texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP )
                TexelFetchNearestMipmapPlaettized( u>>16, v>>16, lod, color );
			else if( texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
            {
                int xx= ((y^x)&1)<<15;
                u+=xx;
                v+=xx;
                TexelFetchNearestMipmapPlaettized( u>>16, v>>16, lod, color );
                u-=xx;
                v-=xx;
            }


            if( alpha_test_mode != ALPHA_TEST_NONE )
            {
                if( alpha_test_mode == ALPHA_TEST_LESS )
                    if( color[3] > constant_alpha )
                        continue;
                if( alpha_test_mode == ALPHA_TEST_GREATER )
                    if( color[3] < constant_alpha )
                        continue;
                if( alpha_test_mode == ALPHA_TEST_EQUAL )
                    if( color[3] != constant_alpha )
                        continue;
                if( alpha_test_mode == ALPHA_TEST_NOT_EQUAL )
                    if( color[3] == constant_alpha )
                        continue;
				if( alpha_test_mode == ALPHA_TEST_DISCARD_LESS_HALF )
					if( color[3] < 128 )
						continue;
				if( alpha_test_mode == ALPHA_TEST_DISCARD_GREATER_HALF )
					if( color[3] > 128 )
						continue;
            }// if alpha test

            if( lighting_mode != LIGHTING_NONE )
            {
                if( lighting_mode == LIGHTING_CONSTANT )
                {
                    color[0]= FastIntUByteMulClamp255( constant_light, color[0] );
                    color[1]= FastIntUByteMulClamp255( constant_light, color[1] );
                    color[2]= FastIntUByteMulClamp255( constant_light, color[2] );
                }
            }// if lighting

            if( blending_mode != BLENDING_NONE )
            {
                if( blending_mode == BLENDING_CONSTANT )
                {
                    pixels[0]= ( color[0] * constant_blend_factor + pixels[0] * inv_constant_blend_factor ) >> 8;
                    pixels[1]= ( color[1] * constant_blend_factor + pixels[1] * inv_constant_blend_factor ) >> 8;
                    pixels[2]= ( color[2] * constant_blend_factor + pixels[2] * inv_constant_blend_factor ) >> 8;
                }
                else if( blending_mode == BLENDING_SRC_ALPHA )
                {
                    unsigned char inv_blend_factor= 255 - color[3];
                    pixels[0]= ( color[0] * color[3] + pixels[0] * inv_blend_factor ) >> 8;
                    pixels[1]= ( color[1] * color[3] + pixels[1] * inv_blend_factor ) >> 8;
                    pixels[2]= ( color[2] * color[3] + pixels[2] * inv_blend_factor ) >> 8;
                }
                else if( blending_mode == BLENDING_FAKE )
                {
                    int z= (x^y)&1;
                    if( z )
                        continue;//discard
                    /*pixels[0]= color[0];
                    pixels[1]= color[1];
                    pixels[2]= color[2];*/
                    Byte4Copy( pixels, color );
                }
                else if( blending_mode == BLENDING_ADD )
                {
                    pixels[0]= FastUByteAddClamp255( color[0], pixels[0] );
                    pixels[1]= FastUByteAddClamp255( color[1], pixels[1] );
                    pixels[2]= FastUByteAddClamp255( color[2], pixels[2] );
                }
                else if( blending_mode == BLENDING_MUL )
                {
                    pixels[0]= ( pixels[0] * color[0] )>>8;
                    pixels[1]= ( pixels[1] * color[1] )>>8;
                    pixels[2]= ( pixels[2] * color[2] )>>8;
                }
				else if( blending_mode == BLENDING_AVG )
                {
                    pixels[0]= ( pixels[0] + color[0] )>>1;
                    pixels[1]= ( pixels[1] + color[1] )>>1;
                    pixels[2]= ( pixels[2] + color[2] )>>1;
                }
            }// if is blending
            else
            {
                /*pixels[0]= color[0];
                pixels[1]= color[1];
                pixels[2]= color[2];*/
                Byte4Copy( pixels, color );
            }
            if( write_depth )
                *depth_p= depth_z;
        }//x
    }//y

}//DrawSprite

template<
enum TextureMode texture_mode,
enum BlendingMode blending_mode,
enum AlphaTestMode alpha_test_mode,
enum LightingMode lighting_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawSpriteFromBuffer( char* buff )
{
	int* b= (int*)buff;
	if( b[2] > b[0] && b[3] > b[1] )
	{
		DrawSprite
		<texture_mode, blending_mode, alpha_test_mode, lighting_mode, depth_test_mode, write_depth>
		( b[0], b[1], b[2], b[3], b[4] );
	}
}

//in per vertex:
static fixed16_t triangle_in_vertex_xy[ 2 * 3 +3 ];//screen space x and y
static fixed16_t triangle_in_vertex_z[3 +1];//screen space z
static PSR_ALIGN_4 unsigned char triangle_in_color[ 3 * 4 +4 ];
static int triangle_in_light[3];
static fixed16_t triangle_in_tex_coord[ 2 * 3 +2 ];
static fixed16_t triangle_in_lightmap_tex_coord[ 2 * 3 +2 ];


static fixed16_t scanline_z[ 1 + PSR_MAX_SCREEN_WIDTH / PSR_LINE_SEGMENT_SIZE ];
/*
in vertices:
.....0
...../\
..../..\
.../....\
../......\
1/________\2
x2 >= x1
y0 >= y1
y0 >= y2
y1 == y2
*/
template<
enum ColorMode color_mode,
enum TextureMode texture_mode,
enum BlendingMode blending_mode,
enum AlphaTestMode alpha_test_mode,
enum LightingMode lighting_mode,
enum LightmapMode lightmap_mode,
enum AdditionalEffectMode additional_lighting_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawTriangleUp()
{
    int y_begin= FastIntClampToZero( Fixed16CeilToInt(triangle_in_vertex_xy[3]) );
    int y_end= FastIntMin( screen_size_y-1, Fixed16FloorToInt(triangle_in_vertex_xy[1]) );

    fixed16_t x_left, x_right, dx_left, dx_right;
    fixed16_t color_left[4], d_color_left[4];
    fixed16_t inv_z_left, d_inv_z_left;
    fixed16_t tc_left[4], d_tc_left[4];// texture coord and lightmap coord
    fixed16_t lod_left, d_lod_left;
    fixed16_t light_left, d_light_left;

    fixed16_t line_color[4], d_line_color[4];
    fixed16_t line_tc[4], d_line_tc[4];// texture coord and lightmap coord
    fixed16_t line_light, d_line_light;
    fixed16_t line_lod, d_line_lod;
    fixed16_t line_inv_z, d_line_inv_z;

    {
		fixed16_t dy= triangle_in_vertex_xy[1] - triangle_in_vertex_xy[3];// triangle height
        fixed16_t dx= triangle_in_vertex_xy[4] - triangle_in_vertex_xy[2];// triangle width
		fixed16_t ddy= (y_begin<<16) - triangle_in_vertex_xy[3];// distance between triangle begin and lower screen border
		dx_left= Fixed16Div( triangle_in_vertex_xy[0] - triangle_in_vertex_xy[2], dy );
		x_left= triangle_in_vertex_xy[2] + Fixed16Mul( ddy, dx_left );
		dx_right= Fixed16Div( triangle_in_vertex_xy[0] - triangle_in_vertex_xy[4], dy );
        x_right= triangle_in_vertex_xy[4] + Fixed16Mul( ddy, dx_right );

        fixed16_t inv_vertex_z[3];
        for( int i= 0; i< 3; i++ )
		{
			
            inv_vertex_z[i]= Fixed16Invert( triangle_in_vertex_z[i] );
			//HACK
			//inv_vertex_z[i]= Fixed16Invert( FastIntMax( triangle_in_vertex_z[i], PSR_MIN_ZMIN ) );
		}
        if( color_mode == COLOR_PER_VERTEX )
        {
            for( int i= 0; i< 4; i++ )
            {
				color_left[i]= (triangle_in_color[i+4]<<PSR_COLOR_DELTA_MULTIPLER_LOG2) * inv_vertex_z[1];
                d_color_left[i]= Fixed16Div( (triangle_in_color[i]<<PSR_COLOR_DELTA_MULTIPLER_LOG2) * inv_vertex_z[0] - color_left[i], dy );
                d_line_color[i]= Fixed16Div( (triangle_in_color[i+8]<<PSR_COLOR_DELTA_MULTIPLER_LOG2) * inv_vertex_z[2] - color_left[i], dx );
                color_left[i]+= Fixed16Mul( ddy, d_color_left[i] );
            }
        }
        else if( color_mode == COLOR_FROM_TEXTURE )
        {
            for( int i= 0; i< 2; i++ )
            {
				tc_left[i]= Fixed16Mul( triangle_in_tex_coord[i+2], inv_vertex_z[1] );
                d_tc_left[i]= Fixed16Div( Fixed16Mul( triangle_in_tex_coord[i], inv_vertex_z[0] ) - tc_left[i], dy );
                d_line_tc[i]= Fixed16Div( Fixed16Mul( triangle_in_tex_coord[i+4], inv_vertex_z[2] ) - tc_left[i], dx );
                tc_left[i]+= Fixed16Mul( ddy, d_tc_left[i] );
            }
        }
        if( lighting_mode == LIGHTING_PER_VERTEX )
        {
			light_left= inv_vertex_z[1] * triangle_in_light[1];
            d_light_left= Fixed16Div( triangle_in_light[0] * inv_vertex_z[0] - light_left, dy );
            d_line_light= Fixed16Div( triangle_in_light[2] * inv_vertex_z[2] - light_left, dx );
            light_left+= Fixed16Mul( ddy, d_light_left );

        }
		else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
		{
			 for( int i= 0; i< 3; i++ )
            {
				color_left[i]= (triangle_in_color[i+4]) * inv_vertex_z[1];
                d_color_left[i]= Fixed16Div( triangle_in_color[i] * inv_vertex_z[0] - color_left[i], dy );
                d_line_color[i]= Fixed16Div( triangle_in_color[i+8] * inv_vertex_z[2] - color_left[i], dx );
                color_left[i]+= Fixed16Mul( ddy, d_color_left[i] );
            }
		}
        else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
        {
            for( int i= 0; i< 2; i++ )
            {
				tc_left[i+2]= Fixed16Mul( triangle_in_lightmap_tex_coord[i+2], inv_vertex_z[1] );
                d_tc_left[i+2]= Fixed16Div( Fixed16Mul( triangle_in_lightmap_tex_coord[i], inv_vertex_z[0] ) - tc_left[i+2], dy );
                d_line_tc[i+2]= Fixed16Div( Fixed16Mul( triangle_in_lightmap_tex_coord[i+4], inv_vertex_z[2] ) - tc_left[i+2], dx );
                tc_left[i+2]+= Fixed16Mul( ddy, d_tc_left[i+2] );
            }
        }//if use lightmaps

//shift invert z, becouse delta can be very short
        d_inv_z_left= Fixed16Div( ( inv_vertex_z[0] - inv_vertex_z[1] )<< PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, dy );
        inv_z_left= ( inv_vertex_z[1] << PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ) + Fixed16Mul( d_inv_z_left, ddy );
        d_line_inv_z= Fixed16Div( ( inv_vertex_z[2] - inv_vertex_z[1] )<< PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, dx );

        /*if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
			|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
        {
            fixed16_t du_dx, du_dy, dv_dx, dv_dy;
            //fixed16_t inv_dx= 65536 / ( triangle_in_vertex_xy[4] - triangle_in_vertex_xy[2] );
            fixed16_t inv_dy= 65536 / ( triangle_in_vertex_xy[1] - triangle_in_vertex_xy[5] );

            fixed16_t vertex0_tc[]= { Fixed16Mul( inv_vertex_z[0], triangle_in_tex_coord[0] ),
                                      Fixed16Mul( inv_vertex_z[0], triangle_in_tex_coord[1] )
                                    };

            //midle vertex between 1 and 2 with x= vertex[0].x
            int tmp_dx=  triangle_in_vertex_xy[0] - triangle_in_vertex_xy[2];
            fixed16_t vertex12_tc[]=
            {
                Fixed16Mul( triangle_in_tex_coord[2], inv_vertex_z[1] ) + d_line_tc[0] * tmp_dx,
                Fixed16Mul( triangle_in_tex_coord[3], inv_vertex_z[1] ) + d_line_tc[1] * tmp_dx
            };

            fixed16_t vertex12_inv_z= (inv_vertex_z[1]<<PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2) + ( d_line_inv_z * tmp_dx );
            fixed16_t d_colomn_inv_z= Fixed16Mul( (inv_vertex_z[0]<<PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2) - vertex12_inv_z, inv_dy );

            du_dx= d_line_tc[0];
            dv_dx= d_line_tc[1];
            du_dy= Fixed16Mul( vertex0_tc[0] - vertex12_tc[0], inv_dy );
            dv_dy= Fixed16Mul( vertex0_tc[1] - vertex12_tc[1], inv_dy );
            int lod[3];
            for( int i= 0; i< 3; i++ )
            {
                int v_du_dx= Fixed16MulResultToInt( du_dx - ( Fixed16Mul( triangle_in_tex_coord[i*2], d_line_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                int v_dv_dx= Fixed16MulResultToInt( dv_dx - ( Fixed16Mul( triangle_in_tex_coord[i*2+1], d_line_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                int v_du_dy= Fixed16MulResultToInt( du_dy - ( Fixed16Mul( triangle_in_tex_coord[i*2], d_colomn_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                int v_dv_dy= Fixed16MulResultToInt( dv_dy - ( Fixed16Mul( triangle_in_tex_coord[i*2+1], d_colomn_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                //lod[i]= log2( sqrt( max( du_dx^2 + dv_dx^2, du_dy^2 + dv_dy^2 ) ) )=
                //log2( max( du_dx^2 + dv_dx^2, du_dy^2 + dv_dy^2 ) )/2
                //this expressions returns lod*2 ( lod in format fixed1_t )
                lod[i]= FastIntLog2Clamp0( FastIntMax(
                                               v_du_dx * v_du_dx + v_dv_dx * v_dv_dx,
                                               v_du_dy * v_du_dy + v_dv_dy * v_dv_dy
                                           ) );
            }

            lod_left= lod[1] * inv_vertex_z[1];
            d_lod_left= ( lod[0] * inv_vertex_z[0] - lod_left ) /dy;
            lod_left+= Fixed16Mul( ddy, d_lod_left );
            d_line_lod= ( lod[2] * inv_vertex_z[2] - lod_left ) / dx;
            //correct results of multiplication of lod in format fixed1_t
            lod_left>>= 1;
            d_lod_left>>= 1;
            d_line_lod>>= 1;

        }*/
    }//calculate delta and begin values

    for( int y= y_begin; y<= y_end; y++, x_left+= dx_left, x_right+= dx_right )//scan lines
    {
        int x_begin= FastIntClampToZero( Fixed16CeilToInt(x_left) );
        int x_end= FastIntMin( screen_size_x-1, Fixed16FloorToInt(x_right) );

        int s= x_begin + screen_size_x * y;
        depth_buffer_t* depth_p= depth_buffer + s;
        unsigned char* pixels= screen_buffer + (s<<2);

        fixed16_t ddx= (x_begin<<16) - x_left;
        if( color_mode == COLOR_PER_VERTEX )
        {
            for( int i= 0; i< 4; i++ )
                line_color[i]= color_left[i] + Fixed16Mul( ddx, d_line_color[i] );
        }
        else if( color_mode == COLOR_FROM_TEXTURE )
        {
            for( int i= 0; i< 2; i++ )
                line_tc[i]= tc_left[i] + Fixed16Mul( ddx, d_line_tc[i] );
            if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP )
                line_lod= lod_left + Fixed16Mul( ddx, d_line_lod );
        }
        if( lighting_mode == LIGHTING_PER_VERTEX )
            line_light= light_left + Fixed16Mul( ddx, d_line_light );
		else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
		{
			for( int i= 0; i< 3; i++ )
                line_color[i]= color_left[i] + Fixed16Mul( ddx, d_line_color[i] );
		}
        else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
        {
            for( int i= 2; i< 4; i++ )
                line_tc[i]= tc_left[i] + Fixed16Mul( ddx, d_line_tc[i] );
        }
        if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
			|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
			line_lod= lod_left + Fixed16Mul( ddx, d_line_lod );
        line_inv_z= inv_z_left + Fixed16Mul( ddx, d_line_inv_z );

#ifdef PSR_FAST_PERSECTIVE
        for( int x= x_begin>>PSR_LINE_SEGMENT_SIZE_LOG2, inv_z= line_inv_z - ( x_begin&(PSR_LINE_SEGMENT_SIZE-1) ) * d_line_inv_z;
                x<= ( x_end>>PSR_LINE_SEGMENT_SIZE_LOG2 ) +2;
                x++, inv_z+= ( PSR_LINE_SEGMENT_SIZE * d_line_inv_z )  )
        {
            
            scanline_z[x]= Fixed16Invert(FastIntMax( inv_z>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, 65536/PSR_MIN_ZMIN ) );
        }
#endif

        for( int x= x_begin; x <= x_end; x++, pixels+=4, depth_p++ )
        {
            PSR_ALIGN_4 unsigned char color[4];
#ifdef PSR_FAST_PERSECTIVE
            fixed16_t final_z;
            {
                int i= x>>PSR_LINE_SEGMENT_SIZE_LOG2, d= x & ( PSR_LINE_SEGMENT_SIZE-1);
                int d1= PSR_LINE_SEGMENT_SIZE - d;
                final_z= ( scanline_z[i] * d1 + scanline_z[i+1] * d )>> PSR_LINE_SEGMENT_SIZE_LOG2;
            }
#else
            fixed16_t final_z= Fixed16Invert( line_inv_z >> PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 );
			//HACK
			//fixed16_t final_z= Fixed16Invert( FastIntMax( line_inv_z >> PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, 65536/PSR_MIN_ZMIN ) );
#endif
            //scale z to convert to depth buffer format
            depth_buffer_t depth_z= final_z >> PSR_DEPTH_SCALER_LOG2;
            if( depth_test_mode != DEPTH_TEST_NONE )
            {
                if( depth_test_mode == DEPTH_TEST_LESS )
                    if( depth_z >= *depth_p )
                         goto next_pixel;
                if( depth_test_mode == DEPTH_TEST_GREATER )
                    if( depth_z <= *depth_p )
                         goto next_pixel;
                if( depth_test_mode == DEPTH_TEST_EQUAL )
                    if( depth_z != *depth_p )
                         goto next_pixel;
                if( depth_test_mode == DEPTH_TEST_NOT_EQUAL )
                    if( depth_z == *depth_p )
                         goto next_pixel;
            }

            if( color_mode == COLOR_CONSTANT )
				Byte4Copy( color, constant_color );
            else if( color_mode == COLOR_PER_VERTEX )
            {
                for( int i= 0; i< 4; i++ )
                    color[i]= Fixed16MulResultToInt( line_color[i], final_z )>>PSR_COLOR_DELTA_MULTIPLER_LOG2;
            }
            else if( color_mode == COLOR_FROM_TEXTURE )
            {
                if( texture_mode == TEXTURE_NEAREST )
                    TexelFetchNearest( Fixed16MulResultToInt( line_tc[0], final_z ),
                                      Fixed16MulResultToInt( line_tc[1], final_z ), color  );
                else if( texture_mode == TEXTURE_LINEAR )
                    TexelFetchLinear( Fixed16Mul( line_tc[0], final_z ), Fixed16Mul( line_tc[1] , final_z ), color );
                else if( texture_mode == TEXTURE_FAKE_FILTER )
                {
					const static int shift_table[]= { 16384,0, 32768,49152, 49152,32768, 0,16384 };
					int ind= (x&1)+((y&1)<<1);
					int uu= shift_table[ind+1];
					int vv= shift_table[ind];
                    TexelFetchNearest( ( Fixed16Mul( line_tc[0], final_z ) + uu )>> 16,
                                       ( Fixed16Mul( line_tc[1], final_z ) + vv )>> 16, color  );
                }
                else if( texture_mode == TEXTURE_NEAREST_MIPMAP )
                {
                    TexelFetchNearestMipmap( Fixed16MulResultToInt( line_tc[0], final_z ),
                                             Fixed16MulResultToInt( line_tc[1], final_z ), Fixed16MulResultToInt( line_lod, final_z ), color );
                }
                else if( texture_mode == TEXTURE_FAKE_FILTER_MIPMAP )
                {
                    int xx= ((y^x)&1)<<15;
                    int yy= xx;
                    TexelFetchNearestMipmap( ( Fixed16Mul( line_tc[0], final_z ) + yy )>> 16,
                                             ( Fixed16Mul( line_tc[1], final_z ) + xx )>> 16,
                                             Fixed16MulResultToInt( line_lod, final_z ), color );
                }
				else if( texture_mode == TEXTURE_PALETTIZED_NEAREST )
					TexelFetchNearestPlaettized(
					Fixed16MulResultToInt( line_tc[0], final_z ),
					Fixed16MulResultToInt( line_tc[1], final_z ), color );
				else if( texture_mode == TEXTURE_PALETTIZED_LINEAR )
					TexelFetchLinearPlaettized( Fixed16Mul( line_tc[0], final_z ),
					Fixed16Mul( line_tc[1] , final_z ), color );
				else if( texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER )
				{
					int xx= ((y^x)&1)<<15;
					TexelFetchNearestPlaettized( ( Fixed16Mul( line_tc[0], final_z ) + xx )>> 16,
                                       ( Fixed16Mul( line_tc[1], final_z ) + xx )>> 16, color );
				}
				else if( texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP )
					TexelFetchNearestMipmapPlaettized( Fixed16MulResultToInt( line_tc[0], final_z ),
                                             Fixed16MulResultToInt( line_tc[1], final_z ),
											 Fixed16MulResultToInt( line_lod, final_z ), color );
				else if( texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
				{
					int xx= ((y^x)&1)<<15;
					TexelFetchNearestMipmapPlaettized( ( Fixed16Mul( line_tc[0], final_z ) + xx )>> 16,
                                             ( Fixed16Mul( line_tc[1], final_z ) + xx )>> 16,
                                             Fixed16MulResultToInt( line_lod, final_z ), color );
				}
            }//color from texture

            if( alpha_test_mode != ALPHA_TEST_NONE )
            {
                if( alpha_test_mode == ALPHA_TEST_LESS )
                    if( color[3] > constant_alpha )
                        goto next_pixel;
                if( alpha_test_mode == ALPHA_TEST_GREATER )
                    if( color[3] < constant_alpha )
                        goto next_pixel;
                if( alpha_test_mode == ALPHA_TEST_EQUAL )
                    if( color[3] != constant_alpha )
                        goto next_pixel;
                if( alpha_test_mode == ALPHA_TEST_NOT_EQUAL )
                    if( color[3] == constant_alpha )
                        goto next_pixel;
				if( alpha_test_mode == ALPHA_TEST_DISCARD_LESS_HALF )
					if( color[3] < 128 )
						goto next_pixel;
				if( alpha_test_mode == ALPHA_TEST_DISCARD_GREATER_HALF )
					if( color[3] >= 128 )
						goto next_pixel;
            }// if alpha test

            //lighting
            if( lighting_mode == LIGHTING_CONSTANT )
            {
                color[0]= FastIntUByteMulClamp255( constant_light, color[0] );
                color[1]= FastIntUByteMulClamp255( constant_light, color[1] );
                color[2]= FastIntUByteMulClamp255( constant_light, color[2] );
            }
            else if( lighting_mode == LIGHTING_PER_VERTEX )
            {
                int final_light= FastIntClampToZero( Fixed16MulResultToInt( line_light, final_z ) );
                color[0]= FastIntUByteMulClamp255( final_light, color[0] );
                color[1]= FastIntUByteMulClamp255( final_light, color[1] );
                color[2]= FastIntUByteMulClamp255( final_light, color[2] );
            }
			else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
            {
				int final_light[3];
				final_light[0]= 3*Fixed16MulResultToInt( line_color[0], final_z );
				final_light[1]= 3*Fixed16MulResultToInt( line_color[1], final_z );
				final_light[2]= 3*Fixed16MulResultToInt( line_color[2], final_z );
                color[0]= FastIntUByteMulClamp255( final_light[0], color[0] );
                color[1]= FastIntUByteMulClamp255( final_light[1], color[1] );
                color[2]= FastIntUByteMulClamp255( final_light[2], color[2] );
            }
            else if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
            {
				if( lightmap_mode == LIGHTMAP_NEAREST )
				{
					int light= LightmapFetchNearest(Fixed16MulResultToInt(line_tc[2], final_z), Fixed16MulResultToInt(line_tc[3], final_z))<<1;
					color[0]= FastIntUByteMulClamp255(light,color[0]);
					color[1]= FastIntUByteMulClamp255(light,color[1]);
					color[2]= FastIntUByteMulClamp255(light,color[2]);
				}
				else if( lightmap_mode == LIGHTMAP_LINEAR )
				{
					int light= LightmapFetchLinear( Fixed16Mul(line_tc[2], final_z), Fixed16Mul(line_tc[3], final_z) )*3;
					color[0]= FastIntUByteMulClamp255(light,color[0]);
					color[1]= FastIntUByteMulClamp255(light,color[1]);
					color[2]= FastIntUByteMulClamp255(light,color[2]);
				}
				else if( lightmap_mode == LIGHTMAP_COLORED_RGBS_LINEAR )
				{
					unsigned char light[4];
					LightmapFetchLinearRGBS(Fixed16Mul(line_tc[2], final_z), Fixed16Mul(line_tc[3], final_z), light );
					color[0]= FastIntUByteMulClamp255(light[0]*3,color[0]);
					color[1]= FastIntUByteMulClamp255(light[1]*3,color[1]);
					color[2]= FastIntUByteMulClamp255(light[2]*3,color[2]);
				}
				else if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
				{
					unsigned char light[4];
					LightmapFetchColoredLinear(Fixed16Mul(line_tc[2], final_z), Fixed16Mul(line_tc[3], final_z), light );
					color[0]= FastIntUByteMulClamp255(light[0]*3,color[0]);
					color[1]= FastIntUByteMulClamp255(light[1]*3,color[1]);
					color[2]= FastIntUByteMulClamp255(light[2]*3,color[2]);
				}
            }
            else if( lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
            {
            }

            //blending
            if( blending_mode != BLENDING_NONE )
            {
                if( blending_mode == BLENDING_CONSTANT )
                {
                    pixels[0]= ( color[0] * constant_blend_factor + pixels[0] * inv_constant_blend_factor ) >> 8;
                    pixels[1]= ( color[1] * constant_blend_factor + pixels[1] * inv_constant_blend_factor ) >> 8;
                    pixels[2]= ( color[2] * constant_blend_factor + pixels[2] * inv_constant_blend_factor ) >> 8;
                }
                else if( blending_mode == BLENDING_AVG )
                {
                    pixels[0]= ( pixels[0] + color[0] )>>1;
                    pixels[1]= ( pixels[1] + color[1] )>>1;
                    pixels[2]= ( pixels[2] + color[2] )>>1;
                }
                else if( blending_mode == BLENDING_SRC_ALPHA )
                {
                    unsigned char inv_blend_factor= 255 - color[3];
                    pixels[0]= ( color[0] * color[3] + pixels[0] * inv_blend_factor ) >> 8;
                    pixels[1]= ( color[1] * color[3] + pixels[1] * inv_blend_factor ) >> 8;
                    pixels[2]= ( color[2] * color[3] + pixels[2] * inv_blend_factor ) >> 8;
                }
                else if( blending_mode == BLENDING_FAKE )
                {
                    int z= (x^y)&1;
                    if( z )
                        goto next_pixel;//discard
                    /*pixels[0]= color[0];
                    pixels[1]= color[1];
                    pixels[2]= color[2];*/
                    Byte4Copy( pixels, color );
                }
                else if( blending_mode == BLENDING_ADD )
                {
                    pixels[0]= FastUByteAddClamp255( color[0], pixels[0] );
                    pixels[1]= FastUByteAddClamp255( color[1], pixels[1] );
                    pixels[2]= FastUByteAddClamp255( color[2], pixels[2] );
                }
                else if( blending_mode == BLENDING_MUL )
                {
                    pixels[0]= ( pixels[0] * color[0] )>>8;
                    pixels[1]= ( pixels[1] * color[1] )>>8;
                    pixels[2]= ( pixels[2] * color[2] )>>8;
                }
            }// if is blending
            else
            {
                /*pixels[0]= color[0];
                pixels[1]= color[1];
                pixels[2]= color[2];*/
                Byte4Copy( pixels, color );
            }
            if( write_depth )
                *depth_p= depth_z;
next_pixel:
            if( color_mode == COLOR_PER_VERTEX )
            {
                line_color[0]+= d_line_color[0];
                line_color[1]+= d_line_color[1];
                line_color[2]+= d_line_color[2];
                line_color[3]+= d_line_color[3];
            }
            else if( color_mode == COLOR_FROM_TEXTURE )
            {
                line_tc[0]+= d_line_tc[0];
                line_tc[1]+= d_line_tc[1];
                if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
					|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
                    line_lod+= d_line_lod;
            }
            if( lighting_mode == LIGHTING_PER_VERTEX )
                line_light+= d_line_light;
			else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
			{
				line_color[0]+= d_line_color[0];
                line_color[1]+= d_line_color[1];
                line_color[2]+= d_line_color[2];
			}
            else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
            {
                line_tc[2]+= d_line_tc[2];
                line_tc[3]+= d_line_tc[3];
            }
#ifndef PSR_FAST_PERSECTIVE
            line_inv_z+= d_line_inv_z;
#endif
        }//for x

//next_line:
        if( color_mode == COLOR_PER_VERTEX )
        {
            color_left[0]+= d_color_left[0];
            color_left[1]+= d_color_left[1];
            color_left[2]+= d_color_left[2];
            color_left[3]+= d_color_left[3];
        }
        else if( color_mode == COLOR_FROM_TEXTURE )
        {
            tc_left[0]+= d_tc_left[0];
            tc_left[1]+= d_tc_left[1];
            if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
			|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
                lod_left+= d_lod_left;
        }
        if( lighting_mode == LIGHTING_PER_VERTEX )
            light_left+= d_light_left;
		else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
		{
			color_left[0]+= d_color_left[0];
            color_left[1]+= d_color_left[1];
            color_left[2]+= d_color_left[2];
		}
        else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
        {
            tc_left[2]+= d_tc_left[2];
            tc_left[3]+= d_tc_left[3];
        }
        inv_z_left+= d_inv_z_left;
    }//for y
}//drawTriangleUp


/*tempalte< enum TextureMode texture_mode, enum BlendingMode blending_mode >
void DrawTurbulentTriangleUp()
{
}*/


/*
in vertices:
1________2
.\....../.
..\..../..
...\../...
....\/....
....0.....
x2 >= x1
y0 <= y1
y0 <= y2
y1 == y2
*/

/*
WARNING! THIS CODE IS 81.765% COPYPASTE FROM DrawTriangleUp.
IF YOU WISH MIDIFY THIS FUNCTION, YOU MUST MODYFY  DrawTriangleUp TOO
*/
template<
enum ColorMode color_mode,
enum TextureMode texture_mode,
enum BlendingMode blending_mode,
enum AlphaTestMode alpha_test_mode,
enum LightingMode lighting_mode,
enum LightmapMode lightmap_mode,
enum AdditionalEffectMode additional_lighting_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawTriangleDown()
{
    int y_begin= FastIntClampToZero( Fixed16CeilToInt(triangle_in_vertex_xy[1]) );
    int y_end= FastIntMin( screen_size_y-1, Fixed16FloorToInt(triangle_in_vertex_xy[3]) );

    fixed16_t x_left, x_right, dx_left, dx_right;
    fixed16_t color_left[4], d_color_left[4];
    fixed16_t inv_z_left, d_inv_z_left;
    fixed16_t tc_left[4], d_tc_left[4];// texture coord and lightmap coord
    fixed16_t lod_left, d_lod_left;
    fixed16_t light_left, d_light_left;

    fixed16_t line_color[4], d_line_color[4];
    fixed16_t line_tc[4], d_line_tc[4];// texture coord and lightmap coord
    fixed16_t line_light, d_line_light;
    fixed16_t line_lod, d_line_lod;
    fixed16_t line_inv_z, d_line_inv_z;

    {
		fixed16_t dy= triangle_in_vertex_xy[3] - triangle_in_vertex_xy[1];// triangle height
        fixed16_t dx= triangle_in_vertex_xy[4] - triangle_in_vertex_xy[2];// triangle width
		fixed16_t ddy= (y_begin<<16) - triangle_in_vertex_xy[1];// distance between triangle begin and lower screen border
		dx_left= Fixed16Div( triangle_in_vertex_xy[2] - triangle_in_vertex_xy[0], dy );
        x_left= triangle_in_vertex_xy[0] + Fixed16Mul( ddy, dx_left );
		dx_right= Fixed16Div( triangle_in_vertex_xy[4] - triangle_in_vertex_xy[0], dy );
        x_right= triangle_in_vertex_xy[0] + Fixed16Mul( ddy, dx_right );

        fixed16_t inv_vertex_z[3];
       for( int i= 0; i< 3; i++ )
		{
            inv_vertex_z[i]= Fixed16Invert( triangle_in_vertex_z[i] );
			//HACK
			//inv_vertex_z[i]= Fixed16Invert( FastIntMax( triangle_in_vertex_z[i], PSR_MIN_ZMIN ) );
		}
        if( color_mode == COLOR_PER_VERTEX )
        {
            for( int i= 0; i< 4; i++ )
            {
				color_left[i]= (triangle_in_color[i]<<PSR_COLOR_DELTA_MULTIPLER_LOG2) * inv_vertex_z[0];
                d_color_left[i]= Fixed16Div( (triangle_in_color[i+4]<<PSR_COLOR_DELTA_MULTIPLER_LOG2) * inv_vertex_z[1] - color_left[i], dy );
                d_line_color[i]= Fixed16Div( (triangle_in_color[i+8]<<PSR_COLOR_DELTA_MULTIPLER_LOG2) * inv_vertex_z[2] - (triangle_in_color[i+4]<<PSR_COLOR_DELTA_MULTIPLER_LOG2) * inv_vertex_z[1], dx );
                color_left[i]+= Fixed16Mul( ddy, d_color_left[i] );
            }
        }
        else if( color_mode == COLOR_FROM_TEXTURE )
        {
            for( int i= 0; i< 2; i++ )
            {
				tc_left[i]= Fixed16Mul( triangle_in_tex_coord[i], inv_vertex_z[0] );
                d_tc_left[i]= Fixed16Div( Fixed16Mul( triangle_in_tex_coord[i+2], inv_vertex_z[1] ) - tc_left[i] , dy );
                tc_left[i]+= Fixed16Mul( ddy, d_tc_left[i] );
                d_line_tc[i]= Fixed16Div( Fixed16Mul( triangle_in_tex_coord[i+4], inv_vertex_z[2] ) - Fixed16Mul( triangle_in_tex_coord[i+2], inv_vertex_z[1] ), dx );
            }
        }
        if( lighting_mode == LIGHTING_PER_VERTEX )
        {
			light_left= inv_vertex_z[0] * triangle_in_light[0];
            d_light_left= Fixed16Div( triangle_in_light[1] * inv_vertex_z[1] - light_left, dy );
            light_left+= Fixed16Mul( ddy, d_light_left );
            d_line_light= Fixed16Div( triangle_in_light[2] * inv_vertex_z[2] - triangle_in_light[1] * inv_vertex_z[1], dx );
        }
		else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
		{
			for( int i= 0; i< 3; i++ )
            {
				color_left[i]= triangle_in_color[i] * inv_vertex_z[0];
                d_color_left[i]= Fixed16Div( triangle_in_color[i+4] * inv_vertex_z[1] - color_left[i], dy );
                d_line_color[i]= Fixed16Div( triangle_in_color[i+8] * inv_vertex_z[2] - triangle_in_color[i+4] * inv_vertex_z[1], dx );
                color_left[i]+= Fixed16Mul( ddy, d_color_left[i] );
            }
		}
        else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
        {
            for( int i= 0; i< 2; i++ )
            {
				tc_left[i+2]= Fixed16Mul( triangle_in_lightmap_tex_coord[i], inv_vertex_z[0] );
                d_tc_left[i+2]= Fixed16Div( Fixed16Mul( triangle_in_lightmap_tex_coord[i+2], inv_vertex_z[1] ) - tc_left[i+2], dy );
                tc_left[i+2]+= Fixed16Mul( ddy, d_tc_left[i+2] );
                d_line_tc[i+2]= Fixed16Div( Fixed16Mul( triangle_in_lightmap_tex_coord[i+4], inv_vertex_z[2] ) - Fixed16Mul( triangle_in_lightmap_tex_coord[i+2], inv_vertex_z[1] ), dx );
            }
        }//if use lightmaps

		 //shift invert z, becouse delta can be very short
        d_inv_z_left= Fixed16Div( ( inv_vertex_z[1] - inv_vertex_z[0] )<< PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, dy );
        inv_z_left= ( inv_vertex_z[0] << PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ) + Fixed16Mul( d_inv_z_left, ddy );
        d_line_inv_z= Fixed16Div( ( inv_vertex_z[2] - inv_vertex_z[1] )<< PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, dx );

        /*if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
			|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
        {
            fixed16_t du_dx, du_dy, dv_dx, dv_dy;
            //fixed16_t inv_dx= 65536 / ( triangle_in_vertex_xy[4] - triangle_in_vertex_xy[2] );
            fixed16_t inv_dy= 65536 / ( triangle_in_vertex_xy[5] - triangle_in_vertex_xy[1] );

            fixed16_t vertex0_tc[]= { Fixed16Mul( inv_vertex_z[0], triangle_in_tex_coord[0] ),
                                      Fixed16Mul( inv_vertex_z[0], triangle_in_tex_coord[1] )
                                    };

            //midle vertex between 1 and 2 with x= vertex[0].x
            int tmp_dx=  triangle_in_vertex_xy[0] - triangle_in_vertex_xy[2];
            fixed16_t vertex12_tc[]=
            {
                Fixed16Mul( triangle_in_tex_coord[2], inv_vertex_z[1] ) + d_line_tc[0] * tmp_dx,
                Fixed16Mul( triangle_in_tex_coord[3], inv_vertex_z[1] ) + d_line_tc[1] * tmp_dx
            };

            fixed16_t vertex12_inv_z= (inv_vertex_z[1]<<PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2) + ( d_line_inv_z * tmp_dx );
            fixed16_t d_colomn_inv_z= Fixed16Mul( vertex12_inv_z - (inv_vertex_z[0]<<PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2), inv_dy );

            du_dx= d_line_tc[0];
            dv_dx= d_line_tc[1];
            du_dy= Fixed16Mul( vertex0_tc[0] - vertex12_tc[0], inv_dy );
            dv_dy= Fixed16Mul( vertex0_tc[1] - vertex12_tc[1], inv_dy );
            int lod[3];
            for( int i= 0; i< 3; i++ )
            {
                int v_du_dx= Fixed16MulResultToInt( du_dx - ( Fixed16Mul( triangle_in_tex_coord[i*2], d_line_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                int v_dv_dx= Fixed16MulResultToInt( dv_dx - ( Fixed16Mul( triangle_in_tex_coord[i*2+1], d_line_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                int v_du_dy= Fixed16MulResultToInt( du_dy - ( Fixed16Mul( triangle_in_tex_coord[i*2], d_colomn_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                int v_dv_dy= Fixed16MulResultToInt( dv_dy - ( Fixed16Mul( triangle_in_tex_coord[i*2+1], d_colomn_inv_z )>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 ), triangle_in_vertex_z[i] );
                //lod[i]= log2( sqrt( max( du_dx^2 + dv_dx^2, du_dy^2 + dv_dy^2 ) ) )=
                //log2( max( du_dx^2 + dv_dx^2, du_dy^2 + dv_dy^2 ) )/2
                //this expressions returns lod*2 ( lod in format fixed1_t )
                lod[i]= FastIntLog2Clamp0( FastIntMax(
                                               v_du_dx * v_du_dx + v_dv_dx * v_dv_dx,
                                               v_du_dy * v_du_dy + v_dv_dy * v_dv_dy
                                           ) );
            }

            lod_left= lod[0] * inv_vertex_z[0];
            d_lod_left= ( lod[1] * inv_vertex_z[1] - lod_left ) /dy;
            lod_left+= Fixed16Mul( ddy, d_lod_left );
            d_line_lod= ( lod[2] * inv_vertex_z[2] - lod[1] * lod[1] * inv_vertex_z[1] ) / dx;
            //correct results of multiplication of lod in format fixed1_t
            lod_left>>= 1;
            d_lod_left>>= 1;
            d_line_lod>>= 1;

        }*/
    }//calculate delta and begin values


    for( int y= y_begin; y<= y_end; y++, x_left+= dx_left, x_right+= dx_right )//scan lines
    {
        int x_begin= FastIntClampToZero( Fixed16CeilToInt(x_left) );
        int x_end= FastIntMin( screen_size_x-1, Fixed16FloorToInt(x_right) );
        int s= x_begin + screen_size_x * y;
        depth_buffer_t* depth_p= depth_buffer + s;
        unsigned char* pixels= screen_buffer + (s<<2);

        fixed16_t ddx= (x_begin<<16) - x_left;
        if( color_mode == COLOR_PER_VERTEX )
        {
            for( int i= 0; i< 4; i++ )
                line_color[i]= color_left[i] + Fixed16Mul( ddx, d_line_color[i] );
        }
        else if( color_mode == COLOR_FROM_TEXTURE )
        {
            for( int i= 0; i< 2; i++ )
                line_tc[i]= tc_left[i] + Fixed16Mul( ddx, d_line_tc[i] );
            if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP )
                line_lod= lod_left + Fixed16Mul( ddx, d_line_lod );
        }
        if( lighting_mode == LIGHTING_PER_VERTEX )
            line_light= light_left + Fixed16Mul( ddx, d_line_light );
		else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
		{
			for( int i= 0; i< 3; i++ )
                line_color[i]= color_left[i] + Fixed16Mul( ddx, d_line_color[i] );
		}
        else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
        {
            for( int i= 2; i< 4; i++ )
                line_tc[i]= tc_left[i] + Fixed16Mul( ddx, d_line_tc[i] );
        }
		if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
			|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
			line_lod= lod_left + Fixed16Mul( ddx, d_line_lod );
        line_inv_z= inv_z_left + Fixed16Mul( ddx, d_line_inv_z );

#ifdef PSR_FAST_PERSECTIVE
        for( int x= x_begin>>PSR_LINE_SEGMENT_SIZE_LOG2, inv_z= line_inv_z - ( x_begin&(PSR_LINE_SEGMENT_SIZE-1) ) * d_line_inv_z;
                x<= ( x_end>>PSR_LINE_SEGMENT_SIZE_LOG2 ) +2;
                x++, inv_z+= ( PSR_LINE_SEGMENT_SIZE * d_line_inv_z )  )
        {
            scanline_z[x]= Fixed16Invert(FastIntMax( inv_z>>PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, 65536/PSR_MIN_ZMIN ) );
        }
#endif

        for( int x= x_begin; x <= x_end; x++, pixels+=4, depth_p++ )
        {
            PSR_ALIGN_4 unsigned char color[4];
#ifdef PSR_FAST_PERSECTIVE
            fixed16_t final_z;
            {
                int i= x>>PSR_LINE_SEGMENT_SIZE_LOG2, d= x & ( PSR_LINE_SEGMENT_SIZE-1);
                int d1= PSR_LINE_SEGMENT_SIZE - d;
                final_z= ( scanline_z[i] * d1 + scanline_z[i+1] * d )>> PSR_LINE_SEGMENT_SIZE_LOG2;
            }
#else
			fixed16_t final_z= Fixed16Invert( line_inv_z >> PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 );
			//HACK
			//fixed16_t final_z= Fixed16Invert( FastIntMax( line_inv_z >> PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2, 65536/PSR_MIN_ZMIN ) );
#endif
            //scale z to convert to depth buffer format
            depth_buffer_t depth_z= final_z >> PSR_DEPTH_SCALER_LOG2;
            if( depth_test_mode != DEPTH_TEST_NONE )
            {
                if( depth_test_mode == DEPTH_TEST_LESS )
                    if( depth_z >= *depth_p )
                        goto next_pixel;
                if( depth_test_mode == DEPTH_TEST_GREATER )
                    if( depth_z <= *depth_p )
                        goto next_pixel;
                if( depth_test_mode == DEPTH_TEST_EQUAL )
                    if( depth_z != *depth_p )
                         goto next_pixel;
                if( depth_test_mode == DEPTH_TEST_NOT_EQUAL )
                    if( depth_z == *depth_p )
                        goto next_pixel;
            }

            if( color_mode == COLOR_CONSTANT )
				Byte4Copy( color, constant_color );
            else if( color_mode == COLOR_PER_VERTEX )
            {
                for( int i= 0; i< 4; i++ )
                    color[i]= Fixed16MulResultToInt( line_color[i], final_z )>>PSR_COLOR_DELTA_MULTIPLER_LOG2;
            }
            else if( color_mode == COLOR_FROM_TEXTURE )
            {
                if( texture_mode == TEXTURE_NEAREST )
                    TexelFetchNearest( Fixed16MulResultToInt( line_tc[0], final_z ),
                                       Fixed16MulResultToInt( line_tc[1], final_z ), color  );
                else if( texture_mode == TEXTURE_LINEAR )
                    TexelFetchLinear( Fixed16Mul( line_tc[0], final_z ), Fixed16Mul( line_tc[1] , final_z ), color );
                else if( texture_mode == TEXTURE_FAKE_FILTER )
                {
                    const static int shift_table[]= { 16384,0, 32768,49152, 49152,32768, 0,16384 };
					int ind= (x&1)+((y&1)<<1);
					int uu= shift_table[ind+1];
					int vv= shift_table[ind];
                    TexelFetchNearest( ( Fixed16Mul( line_tc[0], final_z ) + uu )>> 16,
                                       ( Fixed16Mul( line_tc[1], final_z ) + vv )>> 16, color  );
                }
                else if( texture_mode == TEXTURE_NEAREST_MIPMAP )
                {
                    TexelFetchNearestMipmap( Fixed16MulResultToInt( line_tc[0], final_z ),
                                             Fixed16MulResultToInt( line_tc[1], final_z ), Fixed16MulResultToInt( line_lod, final_z ), color );
                }
                else if( texture_mode == TEXTURE_FAKE_FILTER_MIPMAP )
                {
                    int xx= ((y^x)&1)<<15;
                    int yy= xx;
                    TexelFetchNearestMipmap( ( Fixed16Mul( line_tc[0], final_z ) + yy )>> 16,
                                             ( Fixed16Mul( line_tc[1], final_z ) + xx )>> 16,
                                             Fixed16MulResultToInt( line_lod, final_z ), color );
                }
				else if( texture_mode == TEXTURE_PALETTIZED_NEAREST )
					TexelFetchNearestPlaettized(
					Fixed16MulResultToInt( line_tc[0], final_z ),
					Fixed16MulResultToInt( line_tc[1], final_z ), color );
				else if( texture_mode == TEXTURE_PALETTIZED_LINEAR )
					TexelFetchLinearPlaettized( Fixed16Mul( line_tc[0], final_z ), Fixed16Mul( line_tc[1] , final_z ), color );
				else if( texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER )
				{
					int xx= ((y^x)&1)<<15;
					TexelFetchNearestPlaettized( ( Fixed16Mul( line_tc[0], final_z ) + xx )>> 16,
                                       ( Fixed16Mul( line_tc[1], final_z ) + xx )>> 16, color );
				}
				else if( texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP )
					TexelFetchNearestMipmapPlaettized( Fixed16MulResultToInt( line_tc[0], final_z ),
                                             Fixed16MulResultToInt( line_tc[1], final_z ), Fixed16MulResultToInt( line_lod, final_z ), color );
                  
				else if( texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
				{
					int xx= ((y^x)&1)<<15;
					TexelFetchNearestMipmapPlaettized( ( Fixed16Mul( line_tc[0], final_z ) + xx )>> 16,
                                             ( Fixed16Mul( line_tc[1], final_z ) + xx )>> 16,
                                             Fixed16MulResultToInt( line_lod, final_z ), color );
				}
            }//color from texture

            if( alpha_test_mode != ALPHA_TEST_NONE )
            {
                if( alpha_test_mode == ALPHA_TEST_LESS )
                    if( color[3] > constant_alpha )
                        goto next_pixel;
                if( alpha_test_mode == ALPHA_TEST_GREATER )
                    if( color[3] < constant_alpha )
                        goto next_pixel;
                if( alpha_test_mode == ALPHA_TEST_EQUAL )
                    if( color[3] != constant_alpha )
                        goto next_pixel;
                if( alpha_test_mode == ALPHA_TEST_NOT_EQUAL )
                    if( color[3] == constant_alpha )
                        goto next_pixel;
				if( alpha_test_mode == ALPHA_TEST_DISCARD_LESS_HALF )
					if( color[3] < 128 )
						goto next_pixel;
				if( alpha_test_mode == ALPHA_TEST_DISCARD_GREATER_HALF )
					if( color[3] >= 128 )
						goto next_pixel;
            }// if alpha test

            //lighting
            if( lighting_mode == LIGHTING_CONSTANT )
            {
                color[0]= FastIntUByteMulClamp255( constant_light, color[0] );
                color[1]= FastIntUByteMulClamp255( constant_light, color[1] );
                color[2]= FastIntUByteMulClamp255( constant_light, color[2] );
            }
            else if( lighting_mode == LIGHTING_PER_VERTEX )
            {
                int final_light= FastIntClampToZero( Fixed16MulResultToInt( line_light, final_z ) );
                color[0]= FastIntUByteMulClamp255( final_light, color[0] );
                color[1]= FastIntUByteMulClamp255( final_light, color[1] );
                color[2]= FastIntUByteMulClamp255( final_light, color[2] );
            }
			else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
            {
				int final_light[3];
				final_light[0]= 3*Fixed16MulResultToInt( line_color[0], final_z );
				final_light[1]= 3*Fixed16MulResultToInt( line_color[1], final_z );
				final_light[2]= 3*Fixed16MulResultToInt( line_color[2], final_z );
                color[0]= FastIntUByteMulClamp255( final_light[0], color[0] );
                color[1]= FastIntUByteMulClamp255( final_light[1], color[1] );
                color[2]= FastIntUByteMulClamp255( final_light[2], color[2] );
            }
            else if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
            {
				if( lightmap_mode == LIGHTMAP_NEAREST )
				{
					int light= LightmapFetchNearest(Fixed16MulResultToInt(line_tc[2], final_z), Fixed16MulResultToInt(line_tc[3], final_z))<<1;
					color[0]= FastIntUByteMulClamp255(light,color[0]);
					color[1]= FastIntUByteMulClamp255(light,color[1]);
					color[2]= FastIntUByteMulClamp255(light,color[2]);
				}
				else if( lightmap_mode == LIGHTMAP_LINEAR )
				{
					int light= LightmapFetchLinear( Fixed16Mul(line_tc[2], final_z), Fixed16Mul(line_tc[3], final_z) )*3;
					color[0]= FastIntUByteMulClamp255(light,color[0]);
					color[1]= FastIntUByteMulClamp255(light,color[1]);
					color[2]= FastIntUByteMulClamp255(light,color[2]);
				}
				else if( lightmap_mode == LIGHTMAP_COLORED_RGBS_LINEAR )
				{
					unsigned char light[4];
					LightmapFetchLinearRGBS(Fixed16Mul(line_tc[2], final_z), Fixed16Mul(line_tc[3], final_z), light );
					color[0]= FastIntUByteMulClamp255(light[0]*3,color[0]);
					color[1]= FastIntUByteMulClamp255(light[1]*3,color[1]);
					color[2]= FastIntUByteMulClamp255(light[2]*3,color[2]);
				}
				else if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
				{
					unsigned char light[4];
					LightmapFetchColoredLinear(Fixed16Mul(line_tc[2], final_z), Fixed16Mul(line_tc[3], final_z), light );
					color[0]= FastIntUByteMulClamp255(light[0]*3,color[0]);
					color[1]= FastIntUByteMulClamp255(light[1]*3,color[1]);
					color[2]= FastIntUByteMulClamp255(light[2]*3,color[2]);
				}
            }
            else if( lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
            {
            }

            //blending
            if( blending_mode != BLENDING_NONE )
            {
                if( blending_mode == BLENDING_CONSTANT )
                {
                    pixels[0]= ( color[0] * constant_blend_factor + pixels[0] * inv_constant_blend_factor ) >> 8;
                    pixels[1]= ( color[1] * constant_blend_factor + pixels[1] * inv_constant_blend_factor ) >> 8;
                    pixels[2]= ( color[2] * constant_blend_factor + pixels[2] * inv_constant_blend_factor ) >> 8;
                }
                else if( blending_mode == BLENDING_AVG )
                {
                    pixels[0]= ( pixels[0] + color[0] )>>1;
                    pixels[1]= ( pixels[1] + color[1] )>>1;
                    pixels[2]= ( pixels[2] + color[2] )>>1;
                }
                else if( blending_mode == BLENDING_SRC_ALPHA )
                {
                    unsigned char inv_blend_factor= 255 - color[3];
                    pixels[0]= ( color[0] * color[3] + pixels[0] * inv_blend_factor ) >> 8;
                    pixels[1]= ( color[1] * color[3] + pixels[1] * inv_blend_factor ) >> 8;
                    pixels[2]= ( color[2] * color[3] + pixels[2] * inv_blend_factor ) >> 8;
                }
                else if( blending_mode == BLENDING_FAKE )
                {
                    int z= (x^y)&1;
                    if( z )
                        goto next_pixel;//discard
                    /*pixels[0]= color[0];
                    pixels[1]= color[1];
                    pixels[2]= color[2];*/
                    Byte4Copy( pixels, color );
                }
                else if( blending_mode == BLENDING_ADD )
                {
                    pixels[0]= FastUByteAddClamp255( color[0], pixels[0] );
                    pixels[1]= FastUByteAddClamp255( color[1], pixels[1] );
                    pixels[2]= FastUByteAddClamp255( color[2], pixels[2] );
                }
                else if( blending_mode == BLENDING_MUL )
                {
                    pixels[0]= ( pixels[0] * color[0] )>>8;
                    pixels[1]= ( pixels[1] * color[1] )>>8;
                    pixels[2]= ( pixels[2] * color[2] )>>8;
                }
            }// if is blending
            else
            {
                /*pixels[0]= color[0];
                pixels[1]= color[1];
                pixels[2]= color[2];*/
                Byte4Copy( pixels, color );
            }
            if( write_depth )
                *depth_p= depth_z;
next_pixel:
            if( color_mode == COLOR_PER_VERTEX )
            {
                line_color[0]+= d_line_color[0];
                line_color[1]+= d_line_color[1];
                line_color[2]+= d_line_color[2];
                line_color[3]+= d_line_color[3];
            }
            else if( color_mode == COLOR_FROM_TEXTURE )
            {
                line_tc[0]+= d_line_tc[0];
                line_tc[1]+= d_line_tc[1];
                if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
					|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
                    line_lod+= d_line_lod;
            }
            if( lighting_mode == LIGHTING_PER_VERTEX )
                line_light+= d_line_light;
			else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
			{
				line_color[0]+= d_line_color[0];
                line_color[1]+= d_line_color[1];
                line_color[2]+= d_line_color[2];
			}
            else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
            {
                line_tc[2]+= d_line_tc[2];
                line_tc[3]+= d_line_tc[3];
            }
#ifndef PSR_FAST_PERSECTIVE
            line_inv_z+= d_line_inv_z;
#endif
        }//for x

//next_line:
        if( color_mode == COLOR_PER_VERTEX )
        {
            color_left[0]+= d_color_left[0];
            color_left[1]+= d_color_left[1];
            color_left[2]+= d_color_left[2];
            color_left[3]+= d_color_left[3];
        }
        else if( color_mode == COLOR_FROM_TEXTURE )
        {
            tc_left[0]+= d_tc_left[0];
            tc_left[1]+= d_tc_left[1];
             if( texture_mode == TEXTURE_NEAREST_MIPMAP || texture_mode == TEXTURE_FAKE_FILTER_MIPMAP 
					|| texture_mode == TEXTURE_PALETTIZED_NEAREST_MIPMAP || texture_mode == TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP )
                lod_left+= d_lod_left;
        }
        if( lighting_mode == LIGHTING_PER_VERTEX )
            light_left+= d_light_left;
		else if( lighting_mode == LIGHTING_PER_VERTEX_COLORED )
		{
			color_left[0]+= d_color_left[0];
            color_left[1]+= d_color_left[1];
            color_left[2]+= d_color_left[2];
		}
        else if( lighting_mode == LIGHTING_FROM_LIGHTMAP || lighting_mode == LIGHTING_FROM_LIGHTMAP_OVERBRIGHT )
        {
            tc_left[2]+= d_tc_left[2];
            tc_left[3]+= d_tc_left[3];
        }
        inv_z_left+= d_inv_z_left;
    }//for y
}//DrawTriangleDown


/*
take triangle input variables from buffer and draw triangle
....0.....
..../\....
.../..\...
../....\..
1/______\2
.\....../.
..\..../..
...\../...
....\/....
....3.....

coord
color/tex_coord
light/lightmap_coord
*/


//UNFINISHED
template<
enum ColorMode color_mode,
enum TextureMode texture_mode,
enum BlendingMode blending_mode,
enum AlphaTestMode alpha_test_mode,
enum LightingMode lighting_mode,
enum LightmapMode lightmap_mode,
enum AdditionalEffectMode additional_lighting_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawTriangleSpansFromBuffer( char* buff )
{
	int y_begin= FastIntClampToZero( triangle_in_vertex_xy[1] );
	int y_end= FastIntMin( screen_size_x, triangle_in_vertex_xy[5] );
	int y_middle= FastIntClampToZero( FastIntMin( screen_size_x-1, triangle_in_vertex_xy[3] ) );

	for( int y= y_begin; y< y_middle; y++ )
	{
	}

	for( int y= y_middle; y< y_end; y++ )
	{
	}

}

template<
enum ColorMode color_mode,
enum TextureMode texture_mode,
enum BlendingMode blending_mode,
enum AlphaTestMode alpha_test_mode,
enum LightingMode lighting_mode,
enum LightmapMode lightmap_mode,
enum AdditionalEffectMode additional_lighting_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawTriangleFromBuffer( char* buff )
{
    char* v= buff;
    for( int i= 0; i< 3; i++ )
    {
        triangle_in_vertex_xy[i*2]= ((int*)v)[0];
        triangle_in_vertex_xy[i*2+1]= ((int*)v)[1];
        triangle_in_vertex_z[i]= ((int*)v)[2];
        v+= 3 * sizeof(int);
        if( color_mode == COLOR_PER_VERTEX || lighting_mode == LIGHTING_PER_VERTEX_COLORED )
        {
            Byte4Copy( triangle_in_color + (i<<2), v );
            v+= 4;
        }
        if( color_mode == COLOR_FROM_TEXTURE )
        {
            triangle_in_tex_coord[i*2+0]= ((int*)v)[0];
            triangle_in_tex_coord[i*2+1]= ((int*)v)[1];
            v+= sizeof(int) * 2;
        }
        if( lighting_mode == LIGHTING_PER_VERTEX )
        {
            triangle_in_light[i]= ((int*)v)[0];
            v+= sizeof(int);
        }
        else if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
        {
            triangle_in_lightmap_tex_coord[i*2+0]= ((int*)v)[0];
            triangle_in_lightmap_tex_coord[i*2+1]= ((int*)v)[1];
            v+= sizeof(int) * 2;
        }
    }//for
	int dx= Fixed16FloorToInt(triangle_in_vertex_xy[4]) - Fixed16CeilToInt(triangle_in_vertex_xy[2]);
	int dy= Fixed16FloorToInt(triangle_in_vertex_xy[3]) - Fixed16CeilToInt(triangle_in_vertex_xy[1]);
	//if triangle dimensions is very small, extend it to pixel size, to prevent integet overflow in delta calculations
	if( dx == 0 )
	{
		triangle_in_vertex_xy[2]= Fixed16Floor(triangle_in_vertex_xy[2])+2;
		triangle_in_vertex_xy[4]= Fixed16Ceil(triangle_in_vertex_xy[4])-2;
	}
	if( dx >=0 && dy>=0 )
	{
		if( dy == 0 )
		{
			triangle_in_vertex_xy[3]= Fixed16Ceil(triangle_in_vertex_xy[3])-2;
			triangle_in_vertex_xy[1]= Fixed16Floor(triangle_in_vertex_xy[1])+2;//lower vertex
		}
		DrawTriangleDown< color_mode, texture_mode, blending_mode,
		alpha_test_mode, lighting_mode, lightmap_mode, additional_lighting_mode,
		depth_test_mode, write_depth > ();
	}

    //lower vertex
    triangle_in_vertex_xy[0]= ((int*)v)[0];
    triangle_in_vertex_xy[1]= ((int*)v)[1];
    triangle_in_vertex_z[0]= ((int*)v)[2];
    v+= 3 * sizeof(int);
    if( color_mode == COLOR_PER_VERTEX || lighting_mode == LIGHTING_PER_VERTEX_COLORED )
    {
        Byte4Copy( triangle_in_color, v );
        v+= 4;
    }
    if( color_mode == COLOR_FROM_TEXTURE )
    {
        triangle_in_tex_coord[0]= ((int*)v)[0];
        triangle_in_tex_coord[1]= ((int*)v)[1];
        v+= sizeof(int) * 2;
    }
    if( lighting_mode == LIGHTING_PER_VERTEX )
    {
        triangle_in_light[0]= ((int*)v)[0];
        v+= sizeof(int);
    }
    else if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
    {
        triangle_in_lightmap_tex_coord[0]= ((int*)v)[0];
        triangle_in_lightmap_tex_coord[1]= ((int*)v)[1];
        v+= sizeof(int) * 2;
    }

	dy= Fixed16FloorToInt(triangle_in_vertex_xy[1]) - Fixed16CeilToInt(triangle_in_vertex_xy[3]);
	if( dx >=0 && dy>=0 )
	{
		//if triangle dimensions is very small, extend it to pixel size, to prevent integet overflow in delta calculations
		if( dy == 0 )
		{
			triangle_in_vertex_xy[3]= Fixed16Floor(triangle_in_vertex_xy[3])+2;
			triangle_in_vertex_xy[1]= Fixed16Ceil(triangle_in_vertex_xy[1])-2;//upper vertex
		}
		DrawTriangleUp< color_mode, texture_mode, blending_mode,
		alpha_test_mode, lighting_mode, lightmap_mode, additional_lighting_mode,
		depth_test_mode, write_depth > ();
	}
}


//line input variables
static int line_in_xy[ 2 * 2 ];
static fixed16_t line_in_z[ 2 ];
static int line_in_color[ 4 * 2 ];

template<
enum ColorMode color_mode,
enum BlendingMode blending_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawLineY() //if dy > dx
{
    if( line_in_xy[1] > line_in_xy[3] )
    {
        int tmp= line_in_xy[0];
        line_in_xy[0]= line_in_xy[2];
        line_in_xy[2]= tmp;
        tmp= line_in_xy[1];
        line_in_xy[1]= line_in_xy[3];
        line_in_xy[3]= tmp;
        tmp= line_in_z[0];
        line_in_z[0]= line_in_z[1];
        line_in_z[1]= tmp;

        tmp= ( (int*)line_in_color )[0];
        ( (int*)line_in_color )[0]= ( (int*)line_in_color )[1];
        ( (int*)line_in_color )[1]= tmp;
    }

    int y_begin= FastIntMax( line_in_xy[1], 0 );
    int y_end= FastIntMin( line_in_xy[3], screen_size_y );
    fixed16_t dx, x, inv_z, d_inv_z;
    dx= ( ( line_in_xy[2] - line_in_xy[0] ) << 16 ) / ( line_in_xy[3] - line_in_xy[1] );
    x= ( line_in_xy[0]<<16 ) + dx * ( y_begin  - line_in_xy[1] );

    fixed16_t color_delta[4], interpolated_color[4];
    if( color_mode == COLOR_PER_VERTEX )
    {
        fixed16_t inv_z1[2]= { Fixed16Invert( line_in_z[0] ), Fixed16Invert( line_in_z[1] ) };
        int dy= line_in_xy[3] - line_in_xy[1];
        for( int i= 0; i< 4; i++ )
        {
            interpolated_color[i]= line_in_color[i] * inv_z1[0];
            color_delta[i]=  line_in_color[i+4] * inv_z1[1] - interpolated_color[i];
            color_delta[i]/= dy;
            interpolated_color[i]+= color_delta[i] * ( y_begin - line_in_xy[1] );
        }
        d_inv_z= ( inv_z1[1] - inv_z1[0] );
        d_inv_z/= dy;
        inv_z= inv_z1[0] + d_inv_z * ( y_begin - line_in_xy[1] );
    }


    for( int y= y_begin; y< y_end; y++, x+= dx  )
    {
        int s;
        depth_buffer_t* depth_p;
        unsigned char* pixels;
        fixed16_t final_z;
        depth_buffer_t depth_z;

        if( x < 0 )
            goto next_pixel;
        if( (x>>16) >= screen_size_x )
            goto next_pixel;

        s= (x>>16) + y * screen_size_x;
        depth_p= depth_buffer + s;
        s<<= 2;
        pixels= screen_buffer + s;

        final_z= Fixed16Invert( inv_z );
        depth_z= final_z >> PSR_DEPTH_SCALER_LOG2;

        if( depth_test_mode != DEPTH_TEST_NONE )
        {
            if( depth_test_mode == DEPTH_TEST_LESS )
                if( depth_z >= *depth_p )
                    goto next_pixel;
            if( depth_test_mode == DEPTH_TEST_GREATER )
                if( depth_z <= *depth_p )
                    goto next_pixel;
            if( depth_test_mode == DEPTH_TEST_EQUAL )
                if( depth_z != *depth_p )
                    goto next_pixel;
            if( depth_test_mode == DEPTH_TEST_NOT_EQUAL )
                if( depth_z == *depth_p )
                    goto next_pixel;
        }

        PSR_ALIGN_4 unsigned char color[4];
        if( color_mode == COLOR_PER_VERTEX )
        {
            fixed16_t final_z= Fixed16Invert( inv_z );
            for( int i= 0; i< 4; i++ )
                color[i]= Fixed16MulResultToInt( interpolated_color[i], final_z );//( interpolated_color[i] * final_z ).ToInt();

        }
        else
        {
            color[0]= constant_color[0];
            color[1]= constant_color[1];
            color[2]= constant_color[2];
            color[3]= constant_color[3];
        }

        if( blending_mode != BLENDING_NONE )
        {
            if( blending_mode == BLENDING_CONSTANT )
            {
                pixels[0]= ( color[0] * constant_blend_factor + pixels[0] * inv_constant_blend_factor ) >> 8;
                pixels[1]= ( color[1] * constant_blend_factor + pixels[1] * inv_constant_blend_factor ) >> 8;
                pixels[2]= ( color[2] * constant_blend_factor + pixels[2] * inv_constant_blend_factor ) >> 8;
            }
            else if( blending_mode == BLENDING_SRC_ALPHA )
            {
                unsigned char inv_blend_factor= 255 - color[3];
                pixels[0]= ( color[0] * color[3] + pixels[0] * inv_blend_factor ) >> 8;
                pixels[1]= ( color[1] * color[3] + pixels[1] * inv_blend_factor ) >> 8;
                pixels[2]= ( color[2] * color[3] + pixels[2] * inv_blend_factor ) >> 8;
            }
            else if( blending_mode == BLENDING_FAKE )
            {
                if( (y&1) != 0 )
                    goto next_pixel;//discard
                pixels[0]= ( pixels[0] * color[0] )>>8;
                pixels[1]= ( pixels[1] * color[1] )>>8;
                pixels[2]= ( pixels[2] * color[2] )>>8;
            }
            else if( blending_mode == BLENDING_ADD )
            {
                pixels[0]= FastUByteAddClamp255( color[0], pixels[0] );
                pixels[1]= FastUByteAddClamp255( color[1], pixels[1] );
                pixels[2]= FastUByteAddClamp255( color[2], pixels[2] );
            }
            else if( blending_mode == BLENDING_MUL )
            {
                pixels[0]= ( pixels[0] * color[0] )>>8;
                pixels[1]= ( pixels[1] * color[1] )>>8;
                pixels[2]= ( pixels[2] * color[2] )>>8;
            }
        }// if is blending
        else
        {
            pixels[0]= color[0];
            pixels[1]= color[1];
            pixels[2]= color[2];
        }
        if( write_depth )
            *depth_p= depth_z;

next_pixel:
        if( color_mode == COLOR_PER_VERTEX )
        {
            interpolated_color[0]+= color_delta[0];
            interpolated_color[1]+= color_delta[1];
            interpolated_color[2]+= color_delta[2];
            interpolated_color[3]+= color_delta[3];
            inv_z+= d_inv_z;
        }
    }//for y
}



template<
enum ColorMode color_mode,
enum BlendingMode blending_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawLineX() //if dx > dy
{
    if( line_in_xy[0] > line_in_xy[2] )
    {
        int tmp= line_in_xy[0];
        line_in_xy[0]= line_in_xy[2];
        line_in_xy[2]= tmp;
        tmp= line_in_xy[1];
        line_in_xy[1]= line_in_xy[3];
        line_in_xy[3]= tmp;
        tmp= line_in_z[0];
        line_in_z[0]= line_in_z[1];
        line_in_z[1]= tmp;

        tmp= ( (int*)line_in_color )[0];
        ( (int*)line_in_color )[0]= ( (int*)line_in_color )[1];
        ( (int*)line_in_color )[1]= tmp;
    }

    int x_begin= FastIntMax( line_in_xy[0], 0 );
    int x_end= FastIntMin( line_in_xy[2], screen_size_x );
    fixed16_t dy, y, inv_z, d_inv_z;
    dy= ( ( line_in_xy[3] - line_in_xy[1] ) << 16 ) / ( line_in_xy[2] - line_in_xy[0] );
    y= ( line_in_xy[0]<<16 ) + dy * ( x_begin - line_in_xy[0] );

    fixed16_t color_delta[4], interpolated_color[4];
    if( color_mode == COLOR_PER_VERTEX )
    {
        fixed16_t inv_z1[2]= { Fixed16Invert( line_in_z[0] ), Fixed16Invert( line_in_z[1] ) };
        int dx= line_in_xy[2] - line_in_xy[0];
        for( int i= 0; i< 4; i++ )
        {
            interpolated_color[i]= line_in_color[i] * inv_z1[0];
            color_delta[i]= line_in_color[i+4] * inv_z1[1] - interpolated_color[i];
            color_delta[i]/= dx;
            interpolated_color[i]+= color_delta[i] * ( x_begin - line_in_xy[0] );
        }
        d_inv_z= ( inv_z1[1] - inv_z1[0] );
        d_inv_z/= dx;
        inv_z= inv_z1[0] + d_inv_z * ( x_begin - line_in_xy[0] );
    }


    for( int x= x_begin; x< x_end; x++, y+= dy  )
    {
        int s;
        depth_buffer_t* depth_p;
        unsigned char* pixels;
        fixed16_t final_z;
        depth_buffer_t depth_z;

        if( y < 0 )
            goto next_pixel;
        if( (y>>16) >= screen_size_y )
            goto next_pixel;

        s= x + (y>>16) * screen_size_x;
        depth_p= depth_buffer + s;
        s<<= 2;
        pixels= screen_buffer + s;

        final_z= Fixed16Invert( inv_z );
        depth_z= final_z >> PSR_DEPTH_SCALER_LOG2;

        if( depth_test_mode != DEPTH_TEST_NONE )
        {
            if( depth_test_mode == DEPTH_TEST_LESS )
                if( depth_z >= *depth_p )
                    goto next_pixel;
            if( depth_test_mode == DEPTH_TEST_GREATER )
                if( depth_z <= *depth_p )
                    goto next_pixel;
            if( depth_test_mode == DEPTH_TEST_EQUAL )
                if( depth_z != *depth_p )
                    goto next_pixel;
            if( depth_test_mode == DEPTH_TEST_NOT_EQUAL )
                if( depth_z == *depth_p )
                    goto next_pixel;
        }

        PSR_ALIGN_4 unsigned char color[4];
        if( color_mode == COLOR_PER_VERTEX )
        {
            for( int i= 0; i< 4; i++ )
                color[i]= Fixed16MulResultToInt( interpolated_color[i], final_z );
        }
        else
        {
            color[0]= constant_color[0];
            color[1]= constant_color[1];
            color[2]= constant_color[2];
            color[3]= constant_color[3];
        }

        if( blending_mode != BLENDING_NONE )
        {
            if( blending_mode == BLENDING_CONSTANT )
            {
                pixels[0]= ( color[0] * constant_blend_factor + pixels[0] * inv_constant_blend_factor ) >> 8;
                pixels[1]= ( color[1] * constant_blend_factor + pixels[1] * inv_constant_blend_factor ) >> 8;
                pixels[2]= ( color[2] * constant_blend_factor + pixels[2] * inv_constant_blend_factor ) >> 8;
            }
            else if( blending_mode == BLENDING_SRC_ALPHA )
            {
                unsigned char inv_blend_factor= 255 - color[3];
                pixels[0]= ( color[0] * color[3] + pixels[0] * inv_blend_factor ) >> 8;
                pixels[1]= ( color[1] * color[3] + pixels[1] * inv_blend_factor ) >> 8;
                pixels[2]= ( color[2] * color[3] + pixels[2] * inv_blend_factor ) >> 8;
            }
            else if( blending_mode == BLENDING_FAKE )
            {
                if( (x&1) != 0 )
                    goto next_pixel;//discard
                pixels[0]= ( pixels[0] * color[0] )>>8;
                pixels[1]= ( pixels[1] * color[1] )>>8;
                pixels[2]= ( pixels[2] * color[2] )>>8;
            }
            else if( blending_mode == BLENDING_ADD )
            {
                pixels[0]= FastUByteAddClamp255( color[0], pixels[0] );
                pixels[1]= FastUByteAddClamp255( color[1], pixels[1] );
                pixels[2]= FastUByteAddClamp255( color[2], pixels[2] );
            }
            else if( blending_mode == BLENDING_MUL )
            {
                pixels[0]= ( pixels[0] * color[0] )>>8;
                pixels[1]= ( pixels[1] * color[1] )>>8;
                pixels[2]= ( pixels[2] * color[2] )>>8;
            }
        }// if is blending
        else
        {
            pixels[0]= color[0];
            pixels[1]= color[1];
            pixels[2]= color[2];
        }
        if( write_depth )
            *depth_p= depth_z;

next_pixel:
        if( color_mode == COLOR_PER_VERTEX )
        {
            interpolated_color[0]+= color_delta[0];
            interpolated_color[1]+= color_delta[1];
            interpolated_color[2]+= color_delta[2];
            interpolated_color[3]+= color_delta[3];
            inv_z+= d_inv_z;
        }
    }//for x
}


template<
enum ColorMode color_mode,
enum BlendingMode blending_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawLine()
{
    if( FastIntAbs( line_in_xy[0] - line_in_xy[2] ) > FastIntAbs( line_in_xy[1] - line_in_xy[3] ) )
        DrawLineX< color_mode, blending_mode, depth_test_mode, write_depth >();
    else
        DrawLineY< color_mode, blending_mode, depth_test_mode, write_depth >();
}

template<
enum ColorMode color_mode,
enum BlendingMode blending_mode,
enum DepthTestMode depth_test_mode,
bool write_depth >
void DrawLineFromBuffer( char* buff )
{
	char* b= buff;

	line_in_xy[0]= ((int*)buff)[0];
	line_in_xy[1]= ((int*)buff)[1];
	line_in_z[0]= ((int*)buff)[2];
	b+= 3 * sizeof(int);
	if( color_mode == COLOR_PER_VERTEX )
	{
		Byte4Copy( line_in_color, b );
		b+=4;
	}

	line_in_xy[2]= ((int*)buff)[0];
	line_in_xy[3]= ((int*)buff)[1];
	line_in_z[1]= ((int*)buff)[2];
	b+= 3 * sizeof(int);
	if( color_mode == COLOR_PER_VERTEX )
	{
		Byte4Copy( line_in_color+4, b );
		b+=4;
	}
	DrawLine< color_mode, blending_mode, depth_test_mode, write_depth >();
}


}//namespace draw


extern "C"
{
//world rendering functions
void (*DrawWorldTriangleTextureNearestLightmapLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestLightmapLiearRGBS)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestLightmapColoredLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureLinearLightmapLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_LINEAR, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearLightmapLiearRGBS)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_LINEAR, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearLightmapColoredLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_LINEAR, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureFakeFilterLightmapLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_FAKE_FILTER, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterLightmapLiearRGBS)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_FAKE_FILTER, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterLightmapColoredLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_FAKE_FILTER, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

//world rendering functions with avg blend
void (*DrawWorldTriangleTextureNearestLightmapLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestLightmapLiearRGBSBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestLightmapColoredLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureLinearLightmapLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_LINEAR, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearLightmapLiearRGBSBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_LINEAR, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearLightmapColoredLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_LINEAR, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureFakeFilterLightmapLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_FAKE_FILTER, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterLightmapLiearRGBSBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_FAKE_FILTER, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterLightmapColoredLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_FAKE_FILTER, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;



//world rendering functions PALETTIZED
void (*DrawWorldTriangleTextureNearestPalettizedLightmapLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBS)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureLinearPalettizedLightmapLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_LINEAR, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBS)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_LINEAR, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_LINEAR, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_FAKE_FILTER, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBS)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_FAKE_FILTER, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinear)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_FAKE_FILTER, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

//world rendering functions with avg blend PALETTIZED
void (*DrawWorldTriangleTextureNearestPalettizedLightmapLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_NEAREST, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBSBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_NEAREST, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_NEAREST, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureLinearPalettizedLightmapLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_LINEAR, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBSBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_LINEAR, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_LINEAR, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;

void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_FAKE_FILTER, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBSBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_FAKE_FILTER, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_RGBS_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;
void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinearBlend)(char*buff)= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_PALETTIZED_FAKE_FILTER, BLENDING_AVG, ALPHA_TEST_NONE, LIGHTING_FROM_LIGHTMAP, LIGHTMAP_COLORED_LINEAR, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;



void (*DrawTexturedModelTriangleFromBuffer)( char* buff )= Draw::DrawTriangleFromBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_PER_VERTEX_COLORED, LIGHTMAP_NEAREST, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true >;


void (*DrawParticleSprite)(int x0, int y0, int x1, int y1, fixed16_t depth)= Draw::DrawSprite
< TEXTURE_NONE, BLENDING_CONSTANT, ALPHA_TEST_NONE, LIGHTING_NONE, DEPTH_TEST_LESS, true >;
void (*DrawParticleSpriteNoBlend)(int x0, int y0, int x1, int y1, fixed16_t depth)= Draw::DrawSprite
< TEXTURE_NONE, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_NONE, DEPTH_TEST_LESS, true >;

void (*DrawSkyTriangleFromBuffer)( char* buff ) = Draw::DrawTriangleFromBuffer
<COLOR_FROM_TEXTURE, TEXTURE_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_NONE, LIGHTMAP_NEAREST, ADDITIONAL_EFFECT_NONE, DEPTH_TEST_LESS, true>;

void (*DrawWorldSprite)(int x0, int y0, int x1, int y1, fixed16_t depth) =
Draw::DrawSprite<TEXTURE_NEAREST, BLENDING_AVG, ALPHA_TEST_DISCARD_GREATER_HALF, LIGHTING_NONE, DEPTH_TEST_LESS, true >;

void (*DrawTextureRGBA)( int x, int y)= Draw::DrawTexture< TEXTURE_NEAREST >;
void (*DrawTexturePalettized)( int x, int y)= Draw::DrawTexture< TEXTURE_PALETTIZED_NEAREST >;
void (*DrawTextureRectRGBA)( int x, int y, int u0, int v0, int u1, int v1 )= Draw::DrawTextureRect< TEXTURE_NEAREST >;
void (*DrawTextureRectPalettized)( int x, int y, int u0, int v0, int u1, int v1 )= Draw::DrawTextureRect< TEXTURE_PALETTIZED_NEAREST >;

void (*DrawStretchTextureRGBA)( int x0, int y0, int x1, int y1, fixed16_t depth )= Draw::DrawSprite<TEXTURE_NEAREST, BLENDING_NONE, ALPHA_TEST_DISCARD_GREATER_HALF, LIGHTING_NONE, DEPTH_TEST_NONE, false >;
void (*DrawStretchTextureRGBANoAlphaTest)( int x0, int y0, int x1, int y1, fixed16_t depth )= Draw::DrawSprite<TEXTURE_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_NONE, DEPTH_TEST_NONE, false >;
void (*DrawStretchTexturePalettized)( int x0, int y0, int x1, int y1, fixed16_t depth )= Draw::DrawSprite<TEXTURE_PALETTIZED_NEAREST, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_NONE, DEPTH_TEST_NONE, false >;
void (*DrawFill)( int x0, int y0, int x1, int y1, fixed16_t depth )= Draw::DrawSprite<TEXTURE_NONE, BLENDING_NONE, ALPHA_TEST_NONE, LIGHTING_NONE, DEPTH_TEST_NONE, false >;

void (*FadeScreen)(void)= Draw::FadeScreen<1>;
}//extern "C"