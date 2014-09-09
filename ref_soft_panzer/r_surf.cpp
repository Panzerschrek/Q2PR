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

	void* current_farme_data[2];
	void* current_frame_data_current_pos;
	unsigned int current_frame_data_size[2];
}surfaces_cache;

enum LightmapMode current_frame_lightmap_mode;
bool current_frame_texture_palettized;


void InitSurfaceCache()
{
	int screen_size_multipler= 4;

	surfaces_cache.allocated_size= vid.width * vid.height * screen_size_multipler * 4;
	surfaces_cache.data= malloc( surfaces_cache.allocated_size );
	surfaces_cache.current_pos= surfaces_cache.data;

	surfaces_cache.walkthrough_count= 0;

	surfaces_cache.current_frame_data_size[1]=//really big cache, but we have gigabytes of memory now
	surfaces_cache.current_frame_data_size[0]= surfaces_cache.allocated_size * 2;
	surfaces_cache.current_farme_data[0]= malloc( surfaces_cache.current_frame_data_size[0] );
	surfaces_cache.current_farme_data[1]= malloc( surfaces_cache.current_frame_data_size[0] );
}


void ResetSurfaceCache()
{
	if( !r_worldmodel )
		return;
	for( int i= 0; i< r_worldmodel->numsurfaces; i++ )
	{
		for( int j= 0; j< MIPLEVELS; j++ )
			r_worldmodel->surfaces[i].cache[j]= NULL;
	}
}
void ShutdownSurfaceCache()
{
	free( surfaces_cache.data );
	surfaces_cache.data= surfaces_cache.current_pos= NULL;
	surfaces_cache.allocated_size= 0;

	free( surfaces_cache.current_farme_data[0] );
	free( surfaces_cache.current_farme_data[1] );
	surfaces_cache.current_farme_data[1]= surfaces_cache.current_farme_data[0]= NULL;
	surfaces_cache.current_frame_data_size[1]=
	surfaces_cache.current_frame_data_size[0]= 0;
}

void BeginSurfFrame()
{
	if( strcmp( r_lightmap_mode->string, "lightmap_linear_colored" ) == 0 )
		current_frame_lightmap_mode= LIGHTMAP_COLORED_LINEAR;
	else
		current_frame_lightmap_mode= LIGHTMAP_LINEAR;

	if( r_palettized_textures->value )
		current_frame_texture_palettized= true;
	else
		current_frame_texture_palettized= false;

	void* tmp= surfaces_cache.current_farme_data[0];
	surfaces_cache.current_farme_data[0]= surfaces_cache.current_farme_data[1];
	surfaces_cache.current_farme_data[1]= tmp;

	int tmp_s= surfaces_cache.current_frame_data_size[0];
	surfaces_cache.current_frame_data_size[0]= surfaces_cache.current_frame_data_size[1];
	surfaces_cache.current_frame_data_size[1]= tmp_s;

	surfaces_cache.current_frame_data_current_pos= surfaces_cache.current_farme_data[0];
}

void AllocSurface( msurface_t* surf, int mip )
{
	int width_log2= Log2Ceil( (surf->extents[0]>>mip)+2 );
	int height_log2= Log2Ceil( (surf->extents[1]>>mip)+2 );
	int width=  1<<width_log2;
	int height= (surf->extents[1]>>mip)+2; 

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
unsigned char* current_lightmap_data= NULL;
int current_lightmap_size_x;
int lightmap_dy, lightmap_dy1;// y * lightmap_width * lightmap_texel_size
int lightmap_y, lightmap_y1;
PSR_ALIGN_8 unsigned short lightmap_dy_v[4];
PSR_ALIGN_8 unsigned short lightmap_dy1_v[4];// dy\dy1 in vector form, form fast mmx multiplication


#ifdef PSR_MASM32
#define SURFACE_GEN_USE_MMX
#endif


inline void SetupColoredLightmapDYVectors()
{
#ifdef SURFACE_GEN_USE_MMX
	//*128 - for better precice
	lightmap_dy_v[0]= lightmap_dy_v[1]= lightmap_dy_v[2]= lightmap_dy * 128;
	lightmap_dy1_v[0]= lightmap_dy1_v[1]= lightmap_dy1_v[2]= lightmap_dy1 * 128;
#endif
}

//NOTE, uf uses MMX, out lightmap stored in MM4 register
inline void LightmapColoredFetch( fixed16_t u, unsigned short* out_lightmap )
{
	int x, x1;
    x= u>>8;
    x1= x+1;
    fixed16_t dx, dx1;
    dx= u & 0xFF;
    dx1= 256 - dx;
#ifdef SURFACE_GEN_USE_MMX
	PSR_ALIGN_8 unsigned short dx_v[4];
	dx_v[0]= dx_v[1]= dx_v[2]= dx;
	PSR_ALIGN_8 unsigned short dx1_v[4];
	dx1_v[0]= dx1_v[1]= dx1_v[2]= dx1;
   __asm
   {
	   /*
	   mm2 x:y
	   mm3 x1:y
	   mm4 x:y1
	   mm5 x1:y1
	   mm6: dx
	   mm7: dx1
	   */
		mov esi, current_lightmap_data
		add esi, lightmap_y
		mov eax, x
		movd mm2, dword ptr[ esi + eax*4]
		punpcklbw mm2, mm0

		mov eax, x1
		movd mm3, dword ptr[ esi + eax*4]
		punpcklbw mm3, mm0

		mov esi, current_lightmap_data
		add esi, lightmap_y1
		mov eax, x
		movd mm4, dword ptr[ esi + eax*4]
		punpcklbw mm4, mm0

		mov eax, x1
		movd mm5, dword ptr[ esi + eax*4]
		punpcklbw mm5, mm0

		movq mm6, qword ptr[dx1_v]
		movq mm7, qword ptr[dx_v]
		pmullw mm2, mm6
		pmullw mm3, mm7
		pmullw mm4, mm6
		pmullw mm5, mm7

		paddw mm2, mm3
		paddw mm4, mm5

		pmulhuw mm2, qword ptr[lightmap_dy1_v]
		pmulhuw mm4, qword ptr[lightmap_dy_v]

		paddw mm4, mm2
		psrlw mm4, 7//result into MM4
   }
#else SURFACE_GEN_USE_MMX
	int s;
    int colors[12];

    s= (x<<2) + lightmap_y;
    colors[ 0]= current_lightmap_data[s+0];
    colors[ 1]= current_lightmap_data[s+1];
    colors[ 2]= current_lightmap_data[s+2];
    s= (x1<<2)+ lightmap_y;
    colors[ 3]= current_lightmap_data[s+0];
    colors[ 4]= current_lightmap_data[s+1];
    colors[ 5]= current_lightmap_data[s+2];
    s= (x<<2) + lightmap_y1;
    colors[ 6]= current_lightmap_data[s+0];
    colors[ 7]= current_lightmap_data[s+1];
    colors[ 8]= current_lightmap_data[s+2];
    s= (x1<<2)+ lightmap_y1;
    colors[ 9]= current_lightmap_data[s+0];
    colors[10]= current_lightmap_data[s+1];
    colors[11]= current_lightmap_data[s+2];

    colors[0]= ( colors[ 0] * dx1 + colors[ 3] * dx );
    colors[1]= ( colors[ 1] * dx1 + colors[ 4] * dx );
    colors[2]= ( colors[ 2] * dx1 + colors[ 5] * dx );

    colors[3]= ( colors[ 6] * dx1 + colors[ 9] * dx );
    colors[4]= ( colors[ 7] * dx1 + colors[10] * dx );
    colors[5]= ( colors[ 8] * dx1 + colors[11] * dx );
    out_lightmap[0]= ( colors[0] * lightmap_dy1 + colors[3] * lightmap_dy ) >> 16;
    out_lightmap[1]= ( colors[1] * lightmap_dy1 + colors[4] * lightmap_dy ) >> 16;
    out_lightmap[2]= ( colors[2] * lightmap_dy1 + colors[5] * lightmap_dy ) >> 16;
#endif
}

//grayscale lightmap
inline unsigned char LightmapFetch( fixed16_t u )
{
	int x, x1;
    x= u>>8;
    x1= x+1;
    fixed16_t dx, dx1;
    dx= u& 0xFF;
    dx1= 256 - dx;

    int lights[4];

	lights[0]= current_lightmap_data[ x + lightmap_y  ];
	lights[1]= current_lightmap_data[ x1+ lightmap_y  ];
	lights[2]= current_lightmap_data[ x + lightmap_y1 ];
	lights[3]= current_lightmap_data[ x1+ lightmap_y1 ];
    lights[0]= ( lights[0] * dx1 + lights[1] * dx );
    lights[1]= ( lights[2] * dx1 + lights[3] * dx );

	return ( lights[0] * lightmap_dy1 + lights[1] * lightmap_dy ) >> 16;
}


static const unsigned short color_multipler[]= { 3*256,3*256,3*256,3*256 };

template< int mip, enum LightmapMode lightmap_mode, bool texture_is_palettized >
void GenerateSurfaceMip( msurface_t* surf, panzer_surf_cache_t* surf_cache )
{

#ifdef SURFACE_GEN_USE_MMX //initialize constant registers
	/*
	mm0 - always zero
	*/
	__asm pxor mm0, mm0
	__asm movq mm1, qword ptr[color_multipler]
#endif


	Texture* tex= R_FindTexture( surf_cache->image );
	const unsigned char* tex_data= tex->GetLodData(mip);

	int tex_size_x1= (tex->SizeX()>>mip) - 1;
	int tex_size_y1= (tex->SizeY()>>mip) - 1;
	int tex_size_x_log2= tex->SizeXLog2() - mip;

	int min_x= (surf->texturemins[0]>>mip) -1;
	int min_y= (surf->texturemins[1]>>mip) -1;//-1 for border

	const int lightmap_texel_size_log2= ( lightmap_mode == LIGHTMAP_COLORED_LINEAR ) ? 2 : 0;

	PSR_ALIGN_8 unsigned char color[4];
	
	for( int y= 1, y_end= (surf->extents[1]>>mip)+2; y< y_end; y++ )
	{
		int tc_y= (y+min_y) & tex_size_y1;

		unsigned char* dst= surf_cache->data + ( 1 + y * surf_cache->width )* 4;
		const unsigned char* src= tex_data + 
			(tc_y<<tex_size_x_log2) * ( texture_is_palettized? 1 : 4);

		//setup per-line constant lightmap fertch parameters
		lightmap_dy=  ((y-1)<<(16-4-8+mip)) & 255;
		lightmap_dy1= 256 - lightmap_dy;
		lightmap_y= ( current_lightmap_size_x * ((y-1)>>(4-mip)) ) <<lightmap_texel_size_log2;
		lightmap_y1= lightmap_y + (current_lightmap_size_x<<lightmap_texel_size_log2);
		if( lightmap_mode == LIGHTMAP_COLORED_LINEAR ) SetupColoredLightmapDYVectors();
		for( int x= 1, x_end= (surf->extents[0]>>mip)+2; x< x_end; x++, dst+= 4 )
		{
			if( texture_is_palettized )
			{
				int color_index= src[ (x+min_x)&tex_size_x1 ];
				((unsigned int*)color)[0]= d_8to24table[color_index];
			}
			else
				*((int*)color)= ((int*)src)[ (x+min_x)&tex_size_x1 ];
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			{
				unsigned short light[4];
				LightmapColoredFetch( (x-1)<<(8-4+mip), light ); 
#ifdef SURFACE_GEN_USE_MMX
				__asm
				{
					//mm4 - input lightmap
					movd mm2, dword ptr[color]
					punpcklbw mm2, mm0//convert color bytes to words
					pmullw mm2, mm4//*= lightmap
					pmulhuw mm2, mm1 //=( color * color_multipler )/ 256
					packuswb mm2, mm0
					movd dword ptr[color], mm2
				}
#else
				int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
#endif
			}
			else
			{
				int light= 3 * LightmapFetch( (x-1)<<(8-4+mip) );
				int c= (light * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
			}
			*((int*)dst)= *((int*)color);
		}//for x
	}//for y

	
	//generate lower border
	{
		//int y= 0;
		int tc_y= (0+min_y) & tex_size_y1;
		unsigned char* dst= surf_cache->data + ( 1 + 0 * surf_cache->width )* 4;
		const unsigned char* src= tex_data + 
				(tc_y<<tex_size_x_log2) * ( texture_is_palettized? 1 : 4);
		lightmap_dy=  0;
		lightmap_dy1= 256;
		lightmap_y= 0;
		lightmap_y1= lightmap_y + (current_lightmap_size_x<<lightmap_texel_size_log2);
		if( lightmap_mode == LIGHTMAP_COLORED_LINEAR ) SetupColoredLightmapDYVectors();
		for( int x= 1, x_end= (surf->extents[0]>>mip)+2; x< x_end; x++, dst+= 4 )
		{
			if( texture_is_palettized )
			{
				int color_index= src[ (x+min_x)&tex_size_x1 ];
				((unsigned int*)color)[0]= d_8to24table[color_index];
			}
			else
				*((int*)color)= ((int*)src)[ (x+min_x)&tex_size_x1 ];
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			{
				unsigned short light[4];
				LightmapColoredFetch( (x-1)<<(8-4+mip), light ); 
#ifdef SURFACE_GEN_USE_MMX
				__asm
				{
					//mm4 - input lightmap
					movd mm2, dword ptr[color]
					punpcklbw mm2, mm0//convert color bytes to words
					pmullw mm2, mm4//*= lightmap
					pmulhuw mm2, mm1 //=( color * color_multipler )/ 256
					packuswb mm2, mm0
					movd dword ptr[color], mm2
				}
#else
				int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
#endif
			}
			else
			{
				int light= 3 * LightmapFetch( (x-1)<<(8-4+mip) );
				int c= (light * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
			}
			*((int*)dst)= *((int*)color);
			//*((int*)dst)= 0x7f7f7f7f;
		}//for y
	}//lower border

	//generate upper border
	{
		int y= (surf->extents[1]>>mip)+1;
		int tc_y= (y+min_y) & tex_size_y1;
		unsigned char* dst= surf_cache->data + ( 1 + y * surf_cache->width )* 4;
		const unsigned char* src= tex_data + 
				(tc_y<<tex_size_x_log2) * ( texture_is_palettized? 1 : 4);
		lightmap_dy=  ((y-2)<<(16-4-8+mip)) & 255;
		lightmap_dy1= 256 - lightmap_dy;
		lightmap_y= ( ((y-2)>>(4-mip)) * current_lightmap_size_x )<<lightmap_texel_size_log2;
		lightmap_y1= lightmap_y + (current_lightmap_size_x<<lightmap_texel_size_log2);
		if( lightmap_mode == LIGHTMAP_COLORED_LINEAR ) SetupColoredLightmapDYVectors();
		for( int x= 1, x_end= (surf->extents[0]>>mip)+2; x< x_end; x++, dst+= 4 )
		{
			if( texture_is_palettized )
			{
				int color_index= src[ (x+min_x)&tex_size_x1 ];
				((unsigned int*)color)[0]= d_8to24table[color_index];
			}
			else
				*((int*)color)= ((int*)src)[ (x+min_x)&tex_size_x1 ];
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			{
				unsigned short light[4];
				LightmapColoredFetch( (x-1)<<(8-4+mip), light ); 
#ifdef SURFACE_GEN_USE_MMX
				__asm
				{
					//mm4 - input lightmap
					movd mm2, dword ptr[color]
					punpcklbw mm2, mm0//convert color bytes to words
					pmullw mm2, mm4//*= lightmap
					pmulhuw mm2, mm1 //=( color * color_multipler )/ 256
					packuswb mm2, mm0
					movd dword ptr[color], mm2
				}
#else
				int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
#endif
			}
			else
			{
				int light= 3 * LightmapFetch( (x-1)<<(8-4+mip) );
				int c= (light * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
			}
			*((int*)dst)= *((int*)color);
		}//for x
	}//upper border

	//left border
	{
		int x= 0;
		int tc_x= (x+min_x) & tex_size_x1; if( !texture_is_palettized ) tc_x*= 4;
		unsigned char* dst= surf_cache->data + ( x + (1<<surf_cache->width_log2) ) * 4;
		const unsigned char* src= tex_data + tc_x;
		for( int y= 1, y_end= (surf->extents[1]>>mip)+2; y< y_end; y++, dst+= 4 << surf_cache->width_log2 )
		{
			int tc_y= (y+min_y) & tex_size_y1;
	
			lightmap_dy=  ((y-1)<<(16-4-8+mip)) & 255;
			lightmap_dy1= 256 - lightmap_dy;
			lightmap_y= ( ((y-1)>>(4-mip)) * current_lightmap_size_x )<<lightmap_texel_size_log2;
			lightmap_y1= lightmap_y + (current_lightmap_size_x<<lightmap_texel_size_log2);
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR ) SetupColoredLightmapDYVectors();
			if( texture_is_palettized )
			{
				int color_index= src[ tc_y<<tex_size_x_log2 ];
				((unsigned int*)color)[0]= d_8to24table[color_index];
			}
			else
				*((int*)color)= ((int*)src)[ tc_y<<tex_size_x_log2 ];
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			{
				unsigned short light[4];
				LightmapColoredFetch( x<<(8-4+mip), light ); 
#ifdef SURFACE_GEN_USE_MMX
				__asm
				{
					//mm4 - input lightmap
					movd mm2, dword ptr[color]
					punpcklbw mm2, mm0//convert color bytes to words
					pmullw mm2, mm4//*= lightmap
					pmulhuw mm2, mm1 //=( color * color_multipler )/ 256
					packuswb mm2, mm0
					movd dword ptr[color], mm2
				}
#else
				int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
#endif			
			}
			else
			{
				int light= 3 * LightmapFetch( x<<(8-4+mip) );
				int c= (light * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
			}
			*((int*)dst)= *((int*)color);
		}
	}//left border

	//right border
	{
		int x= (surf->extents[0]>>mip)+1;
		int tc_x= (x+min_x) & tex_size_x1; if( !texture_is_palettized ) tc_x*= 4;
		unsigned char* dst= surf_cache->data + ( x + (1<<surf_cache->width_log2) ) * 4;
		const unsigned char* src= tex_data + tc_x;
		for( int y= 1, y_end= (surf->extents[1]>>mip)+2; y< y_end; y++, dst+= 4 << surf_cache->width_log2 )
		{
			int tc_y= (y+min_y) & tex_size_y1;
	
			lightmap_dy=  ((y-1)<<(16-4-8+mip)) & 255;
			lightmap_dy1= 256 - lightmap_dy;
			lightmap_y= ( ((y-1)>>(4-mip)) * current_lightmap_size_x )<<lightmap_texel_size_log2;
			lightmap_y1= lightmap_y + (current_lightmap_size_x<<lightmap_texel_size_log2);
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR ) SetupColoredLightmapDYVectors();
			if( texture_is_palettized )
			{
				int color_index= src[ tc_y<<tex_size_x_log2 ];
				((unsigned int*)color)[0]= d_8to24table[color_index];
			}
			else
				*((int*)color)= ((int*)src)[ tc_y<<tex_size_x_log2 ];
			if( lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			{
				unsigned short light[4];
				LightmapColoredFetch( (x-2)<<(8-4+mip), light ); 
#ifdef SURFACE_GEN_USE_MMX
				__asm
				{
					//mm4 - input lightmap
					movd mm2, dword ptr[color]
					punpcklbw mm2, mm0//convert color bytes to words
					pmullw mm2, mm4//*= lightmap
					pmulhuw mm2, mm1 //=( color * color_multipler )/ 256
					packuswb mm2, mm0
					movd dword ptr[color], mm2
				}
#else
				int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
#endif
			}
			else
			{
				int light= LightmapFetch( (x-2)<<(8-4+mip) );
				int c= (light * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
				c= (light * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
				c= (light * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;
			}
			*((int*)dst)= *((int*)color);
		}
	}//right border


	//calculate corners of surface, just take neigtbours borders pixels
	{
		int dst_coord[]= {
			0, 0,   surf_cache->width-1, 0,
			0, surf_cache->height-1, surf_cache->width-1, surf_cache->height-1 };
		static const int delta_vecs[]= {
			4,4,  -4,4,  4,-4,  -4,-4 };
		for( int c= 0; c< 8 ;c+=2 )
		{
			int dst_ind= (dst_coord[c] + (dst_coord[c+1]<<surf_cache->width_log2) ) << 2;
			int ind[]= { 
				dst_ind + delta_vecs[c],
				dst_ind + (delta_vecs[c+1]<<surf_cache->width_log2)
			};
			for( int i= 0; i< 3; i++ )
			{
				surf_cache->data[dst_ind+i]= (
					surf_cache->data[ i + ind[0] ] + 
					surf_cache->data[ i + ind[1] ] )>>1;
			}
		}
	}

#ifdef SURFACE_GEN_USE_MMX
	__asm emms
#endif

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

		tmp_cache.width_log2= Log2Ceil( (surf->extents[0]>>mip)+2 );
		tmp_cache.height_log2= Log2Ceil( (surf->extents[1]>>mip)+2 );
		tmp_cache.width= 1<< tmp_cache.width_log2;
		tmp_cache.height= 1<< tmp_cache.height_log2;

		tmp_cache.image= current_surf_image;
		tmp_cache.mip= mip;

		int ds= tmp_cache.width * ( (surf->extents[1]>>mip)+2 ) * 4;

		tmp_cache.current_frame_data= 
		tmp_cache.data= (unsigned char*)surfaces_cache.current_frame_data_current_pos;

		int func_num= mip;
		if( !current_frame_texture_palettized ) func_num+= 4;

		if( current_frame_lightmap_mode == LIGHTMAP_COLORED_LINEAR )
			(GenerateSurfaceMipFuncsColoredLighting[func_num])( surf, &tmp_cache );
		else
			(GenerateSurfaceMipFuncsGrayscaleLighting[func_num])( surf, &tmp_cache);

		surfaces_cache.current_frame_data_current_pos=
			(char*)surfaces_cache.current_frame_data_current_pos + ds;
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
	int ds= surf_cache->width * ( (surf->extents[1]>>mip) + 2 ) * 4;
	surf_cache->current_frame_data= (unsigned char*)surfaces_cache.current_frame_data_current_pos;
	//this memcpy takes ~17% of frame time
	memcpy( surf_cache->current_frame_data, surf_cache->data, ds);
	surfaces_cache.current_frame_data_current_pos= (char*)surfaces_cache.current_frame_data_current_pos + ds;

	return surf_cache;
}
