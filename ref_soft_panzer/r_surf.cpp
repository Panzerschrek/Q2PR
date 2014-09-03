#include "r_surf.h"
#include "panzer_rast/fixed.h"
#include "panzer_rast/texture.h"
#include "panzer_rast/rendering_state.h"
#include "r_light.h"

extern Texture* R_FindTexture(image_t* image);

struct
{
	void* data;
	void* current_pos;
	unsigned int allocated_size;
	int walkthrough_count; //number of cycles

	void* current_farme_data;
	void* current_frame_data_current_pos;
	unsigned int current_frame_data_size;
}surfaces_cache;

enum LightmapMode current_frame_lightmap_mode;
bool current_frame_texture_palettized;


void InitSurfaceCache()
{
	//formula of alculating cache size, with changed pixel format(1bit=>4bit)
	surfaces_cache.allocated_size= vid.width * vid.height * 3 * 4;//16 * 1024 * 1024;
	surfaces_cache.data= malloc( surfaces_cache.allocated_size );
	surfaces_cache.current_pos= surfaces_cache.data;

	surfaces_cache.walkthrough_count= 0;

	surfaces_cache.current_frame_data_size= 48 * 1024 * 1024;//really big cache, but we have gigabytes of memory now
	surfaces_cache.current_farme_data= malloc( surfaces_cache.current_frame_data_size );
}


void ShutdownSurfaceCache()
{
	free( surfaces_cache.data );
	surfaces_cache.data= surfaces_cache.current_pos= NULL;
	surfaces_cache.allocated_size= 0;

	free( surfaces_cache.current_farme_data );
	surfaces_cache.current_farme_data= NULL;
	surfaces_cache.current_frame_data_size= 0;
}

void BeginSurfFrame()
{
	surfaces_cache.current_frame_data_current_pos= surfaces_cache.current_farme_data;
	if( strcmp( r_lightmap_mode->string, "lightmap_linear_colored" ) == 0 )
		current_frame_lightmap_mode= LIGHTMAP_COLORED_LINEAR;
	else
		current_frame_lightmap_mode= LIGHTMAP_LINEAR;

	if( r_palettized_textures->value )
		current_frame_texture_palettized= true;
	else
		current_frame_texture_palettized= false;

}

void AllocSurface( msurface_t* surf, int mip )
{
	int width_log2= Log2Ceil( surf->extents[0] )-mip;
	int height_log2= Log2Ceil( surf->extents[1] )-mip;
	int width=  1<<width_log2;
	int height= surf->extents[1]>>mip; 

	unsigned int cache_pixels_size= width * height * 4;
	unsigned int total_data_size= sizeof( panzer_surf_cache_s ) + cache_pixels_size;

	panzer_surf_cache_t* cached_surface;

	if( total_data_size + ((char*)surfaces_cache.current_pos) - ((char*)surfaces_cache.data) <= surfaces_cache.allocated_size )
	{
		cached_surface= (panzer_surf_cache_t*) surfaces_cache.current_pos;
		surfaces_cache.current_pos= (char*)surfaces_cache.current_pos + total_data_size;
	}
	else//warp around
	{
		surfaces_cache.walkthrough_count++;
		cached_surface= (panzer_surf_cache_t*) surfaces_cache.data;
		surfaces_cache.current_pos= (char*)surfaces_cache.data + total_data_size;
	}

	cached_surface->width= width;
	cached_surface->height= height;
	cached_surface->width_log2= width_log2;
	cached_surface->height_log2= height_log2;
	cached_surface->mip= mip;

	cached_surface->data= ((unsigned char*)cached_surface) + sizeof(panzer_surf_cache_t);

	cached_surface->image= NULL;

	surf->surf_cache_walkthrough_number[mip]= surfaces_cache.walkthrough_count;
	surf->cache[mip]= cached_surface;
}

bool IsCachedSurfaceValid( panzer_surf_cache_t* surf_cache, msurface_t* surf, int mip )
{
	if( surf_cache == NULL )
		return false;
	if( surf->surf_cache_walkthrough_number[mip] == surfaces_cache.walkthrough_count )
		return true;
	if( surf->surf_cache_walkthrough_number[mip] <= surfaces_cache.walkthrough_count-2 )//make 2 or more cycles
		return false;
	if( surfaces_cache.current_pos > ((void*)surf_cache) )
		return false;
	return true;
}

int IsSurfaceCachable( msurface_t* surf )
{
	if( !r_surface_caching->value )
		return false;
	//if( surf->dlightframe == r_dlightframecount )
	//	return false;

	return true;
}

//inner LightmapFetch variables. Not changing in surface scanlines, and becouse maked globals
unsigned char* current_lightmap_data;
int current_lightmap_size_x;
int lightmap_dy, lightmap_dy1;
int lightmap_y, lightmap_y1;

void LightmapColoredFetch( fixed16_t u, unsigned char* out_lightmap )
{
	int x, x1;
    x= u>>16;
    x1= x+1;
    fixed16_t dx, dx1;
    dx= ( u & 0xFFFF )>>8;
    dx1= 256 - dx;

	int s;
    int colors[12];
    int mixed_colors[6];

    s= x + lightmap_y * current_lightmap_size_x;
    s<<= 2;
    colors[ 0]= current_lightmap_data[s+0];
    colors[ 1]= current_lightmap_data[s+1];
    colors[ 2]= current_lightmap_data[s+2];
    s= x1 + lightmap_y * current_lightmap_size_x;
    s<<= 2;
    colors[ 3]= current_lightmap_data[s+0];
    colors[ 4]= current_lightmap_data[s+1];
    colors[ 5]= current_lightmap_data[s+2];
    s= x +  lightmap_y1 * current_lightmap_size_x;
    s<<= 2;
    colors[ 6]= current_lightmap_data[s+0];
    colors[ 7]= current_lightmap_data[s+1];
    colors[ 8]= current_lightmap_data[s+2];
    s= x1 + lightmap_y1 * current_lightmap_size_x;
    s<<= 2;
    colors[ 9]= current_lightmap_data[s+0];
    colors[10]= current_lightmap_data[s+1];
    colors[11]= current_lightmap_data[s+2];

    mixed_colors[0]= ( colors[ 0] * dx1 + colors[ 3] * dx );
    mixed_colors[1]= ( colors[ 1] * dx1 + colors[ 4] * dx );
    mixed_colors[2]= ( colors[ 2] * dx1 + colors[ 5] * dx );

    mixed_colors[3]= ( colors[ 6] * dx1 + colors[ 9] * dx );
    mixed_colors[4]= ( colors[ 7] * dx1 + colors[10] * dx );
    mixed_colors[5]= ( colors[ 8] * dx1 + colors[11] * dx );
    out_lightmap[0]= ( mixed_colors[0] * lightmap_dy1 + mixed_colors[3] * lightmap_dy ) >> 16;
    out_lightmap[1]= ( mixed_colors[1] * lightmap_dy1 + mixed_colors[4] * lightmap_dy ) >> 16;
    out_lightmap[2]= ( mixed_colors[2] * lightmap_dy1 + mixed_colors[5] * lightmap_dy ) >> 16;
}

//grayscale lightmap
unsigned char LightmapFetch( fixed16_t u )
{
	int x, x1;
    x= u>>16;
    x1= x+1;
    fixed16_t dx, dx1;
    dx= ( u & 0xFFFF )>>8;
    dx1= 256 - dx;

    int lights[4];
    int mixed_lights[2];
	lights[0]= current_lightmap_data[ x + lightmap_y * current_lightmap_size_x ];
	lights[1]= current_lightmap_data[ x1+ lightmap_y * current_lightmap_size_x ];
	lights[2]= current_lightmap_data[ x + lightmap_y1* current_lightmap_size_x ];
	lights[3]= current_lightmap_data[ x1+ lightmap_y1* current_lightmap_size_x ];
    mixed_lights[0]= ( lights[0] * dx1 + lights[1] * dx );
    mixed_lights[1]= ( lights[2] * dx1 + lights[3] * dx );

	return ( mixed_lights[0] * lightmap_dy1 + mixed_lights[1] * lightmap_dy ) >> 16;
}



template< int mip, enum LightmapMode lightmap_mode, bool texture_is_palettized >
void GenerateSurfaceMip( msurface_t* surf, panzer_surf_cache_t* surf_cache )
{
	Texture* tex= R_FindTexture( surf_cache->image );
	const unsigned char* tex_data= tex->GetLodData(mip);

	int tex_size_x1= (tex->SizeX()>>mip) - 1;
	int tex_size_x_log2= tex->SizeXLog2() - mip;

	int min_x= surf->texturemins[0]>>mip;
	int min_y= surf->texturemins[1]>>mip;

	for( int y= 0, y_end= surf->extents[1]>>mip; y< y_end; y++ )
	{
		int tc_y= (y+min_y) & ((tex->SizeY()>>mip)-1);

		unsigned char* dst= surf_cache->data + ( y * surf_cache->width * 4 );
		const unsigned char* src= tex_data + 
			(tc_y<<tex_size_x_log2) * ( texture_is_palettized? 1 : 4);

		lightmap_dy=  (y<<(16-4-8+mip)) & 255;
		lightmap_dy1= 256 - lightmap_dy;
		lightmap_y= y>>(4-mip);
		lightmap_y1= lightmap_y + 1;

		for( int x= 0, x_end= surf->extents[0]>>mip; x< x_end; x++, dst+= 4 )
		{
			unsigned char color[4];
			if( texture_is_palettized )
			{
				int color_index= src[ (x+min_x)&tex_size_x1 ];
				((unsigned int*)color)[0]= d_8to24table[color_index];
			}
			else
				*((int*)color)= ((int*)src)[ (x+min_x)&tex_size_x1 ];
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			{
				unsigned char light[4];
				LightmapColoredFetch( x<<(16-4+mip), light ); 

				int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
			}
			else
			{
				int light= LightmapFetch(  x<<(16-4+mip) );
				int c= (light * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
			}
			//mip select test
			/*if( mip == 1 )
				*((int*)color)= 0x00FF7F7F;
			else if( mip == 2 )
				*((int*)color)= 0x00FFFF7F;
			else if( mip == 2 )
				*((int*)color)= 0x007F7FFF;*/

			*((int*)dst)= *((int*)color);
		}//for x
	}//for y
}


void (*GenerateSurfaceMipFuncsColoredLighting[8])( msurface_t* surf, panzer_surf_cache_t* surf_cache )= {
	GenerateSurfaceMip< 0, LIGHTMAP_COLORED_LINEAR, true >,
	GenerateSurfaceMip< 1, LIGHTMAP_COLORED_LINEAR, true >,
	GenerateSurfaceMip< 2, LIGHTMAP_COLORED_LINEAR, true >,
	GenerateSurfaceMip< 3, LIGHTMAP_COLORED_LINEAR, true >,
	GenerateSurfaceMip< 0, LIGHTMAP_COLORED_LINEAR, false >,
	GenerateSurfaceMip< 1, LIGHTMAP_COLORED_LINEAR, false >,
	GenerateSurfaceMip< 2, LIGHTMAP_COLORED_LINEAR, false >,
	GenerateSurfaceMip< 3, LIGHTMAP_COLORED_LINEAR, false >};

void (*GenerateSurfaceMipFuncsGrayscaleLighting[8])( msurface_t* surf, panzer_surf_cache_t* surf_cache )= {
	GenerateSurfaceMip< 0, LIGHTMAP_LINEAR, true  >,
	GenerateSurfaceMip< 1, LIGHTMAP_LINEAR, true  >,
	GenerateSurfaceMip< 2, LIGHTMAP_LINEAR, true  >,
	GenerateSurfaceMip< 3, LIGHTMAP_LINEAR, true  >,
	GenerateSurfaceMip< 0, LIGHTMAP_LINEAR, false  >,
	GenerateSurfaceMip< 1, LIGHTMAP_LINEAR, false  >,
	GenerateSurfaceMip< 2, LIGHTMAP_LINEAR, false  >,
	GenerateSurfaceMip< 3, LIGHTMAP_LINEAR, false  >};

	

panzer_surf_cache_t* GenerateSurfaceCache( msurface_t* surf, image_t* current_surf_image, int mip )
{
	//if has dynamic light
	if( surf->dlightframe == r_dlightframecount )
	{		
		unsigned char* lightmap= L_GetSurfaceDynamicLightmap( surf );
		current_lightmap_data= lightmap;
		current_lightmap_size_x= (surf->extents[0]>>4) + 1;

		static panzer_surf_cache_t tmp_cache;

		tmp_cache.width_log2= Log2Ceil( surf->extents[0] )- mip;
		tmp_cache.height_log2= Log2Ceil( surf->extents[1] )- mip;
		tmp_cache.width= 1<< tmp_cache.width_log2;
		tmp_cache.height= 1<< tmp_cache.height_log2;

		tmp_cache.image= current_surf_image;
		tmp_cache.mip= mip;

		tmp_cache.current_frame_data= 
		tmp_cache.data= (unsigned char*)surfaces_cache.current_frame_data_current_pos;

		int func_num= mip;
		if( !current_frame_texture_palettized ) func_num+= 4;

		if( current_frame_lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			(GenerateSurfaceMipFuncsColoredLighting[func_num])( surf, &tmp_cache );
		else
			(GenerateSurfaceMipFuncsGrayscaleLighting[func_num])( surf, &tmp_cache);

		surfaces_cache.current_frame_data_current_pos=
			(char*)surfaces_cache.current_frame_data_current_pos + tmp_cache.width * ( surf->extents[1]>>mip ) * 4;
		return &tmp_cache;
	}
	//else - generate and cache surface

	panzer_surf_cache_t* surf_cache;
	if( !IsCachedSurfaceValid( surf->cache[mip], surf, mip ) )
		AllocSurface( surf, mip );
	
	surf_cache= surf->cache[mip];

	bool lightmap_valid= true;

	//check current lightmap
	for( int map= 0; map < MAXLIGHTMAPS && surf->styles[map]!=255; map++ )
	{
		int style= surf->styles[map];
		for( int i= 0; i< 3; i++ )
		{
			unsigned short cur_style= int( r_newrefdef.lightstyles[ style ].rgb[i]* 256.0f );
			if( cur_style != surf_cache->cached_light_styles[map].colored_light_scales[i] )
			{
				lightmap_valid= false;
				break;
			}
		}
	}

	//if animated surface, or surface just yet allocated, or lightmap changed
	if( surf_cache->image != current_surf_image || !lightmap_valid )
	{
		if( surf->styles[1] == 255 )//only 1 lightmap 
			current_lightmap_data= surf->samples;
		else
			current_lightmap_data= L_GetSurfaceDynamicLightmap( surf );

		current_lightmap_size_x= (surf->extents[0]>>4) + 1;

		surf_cache->image= current_surf_image;

		int func_num= mip;
		if( !current_frame_texture_palettized )
			func_num+= 4;

		if( current_frame_lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			(GenerateSurfaceMipFuncsColoredLighting[func_num])( surf, surf_cache );
		else
			(GenerateSurfaceMipFuncsGrayscaleLighting[func_num])( surf, surf_cache);

		for( int map= 0; map < MAXLIGHTMAPS && surf->styles[map]!=255; map++ )
		{
			int style= surf->styles[map];
			surf_cache->cached_light_styles[map].colored_light_scales[0]= int( r_newrefdef.lightstyles[ style ].rgb[0]* 256.0f );
			surf_cache->cached_light_styles[map].colored_light_scales[1]= int( r_newrefdef.lightstyles[ style ].rgb[1]* 256.0f );
			surf_cache->cached_light_styles[map].colored_light_scales[2]= int( r_newrefdef.lightstyles[ style ].rgb[2]* 256.0f );
		}
	}//if need regenerate surface

	//copy surface pixels to current frame pixel buffer
	int ds= surf_cache->width * ( surf->extents[1]>>mip ) * 4;
	surf_cache->current_frame_data= (unsigned char*)surfaces_cache.current_frame_data_current_pos;
	//this memcpy takes ~17% of frame time
	memcpy( surf_cache->current_frame_data, surf_cache->data, ds);
	surfaces_cache.current_frame_data_current_pos= (char*)surfaces_cache.current_frame_data_current_pos + ds;

	return surf_cache;
}
