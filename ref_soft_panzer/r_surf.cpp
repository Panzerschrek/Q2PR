#include "r_surf.h"
#include "panzer_rast/fixed.h"
#include "panzer_rast/texture.h"


extern Texture* R_FindTexture(image_t* image);

struct
{
	void* data;
	void* current_pos;
	unsigned int allocated_size;
	int walkthrough_count; //number of cycles
	long long unsigned int current_pos_long;


}surfaces_cache;



void InitSurfaceCache()
{
	surfaces_cache.allocated_size= 32 * 1024 * 1024;
	surfaces_cache.data= malloc( surfaces_cache.allocated_size );
	surfaces_cache.current_pos= surfaces_cache.data;

	surfaces_cache.current_pos_long= 0;

	surfaces_cache.walkthrough_count= 0;
}


void ShutdownSurfaceCache()
{
	free( surfaces_cache.data );
	surfaces_cache.data= surfaces_cache.current_pos= NULL;
	surfaces_cache.allocated_size= 0;
}

void AllocSurface( msurface_t* surf )
{
	int width_log2= Log2Ceil( surf->extents[0] );
	int height_log2= Log2Ceil( surf->extents[1] );
	int width=  1<<width_log2;
	int height= 1<<height_log2; 

	unsigned int cache_pixels_size=
		width * height * 4 +
		width * height * 4/4 + 
		width * height * 4/16 +
		width * height * 4/64;

	unsigned int total_data_size= sizeof( panzer_surf_cache_s ) + cache_pixels_size;

	panzer_surf_cache_t* cached_surface;

	if( total_data_size + ((char*)surfaces_cache.current_pos) - ((char*)surfaces_cache.data) <= surfaces_cache.allocated_size )
	{
		cached_surface= (panzer_surf_cache_t*) surfaces_cache.current_pos;
		surfaces_cache.current_pos= (char*)surfaces_cache.current_pos + total_data_size;
		//surfaces_cache.current_pos_long+= total_data_size;
	}
	else//warp around
	{
		printf( "surface cache walkthrough_count: %d\n", surfaces_cache.walkthrough_count );
		//surfaces_cache.current_pos_long+= surfaces_cache.allocated_size - ( ((char*)surfaces_cache.current_pos) - ((char*)surfaces_cache.data) );
		surfaces_cache.walkthrough_count++;
		cached_surface= (panzer_surf_cache_t*) surfaces_cache.data;
		surfaces_cache.current_pos= (char*)surfaces_cache.data + total_data_size;
	}


	cached_surface->width= width;
	cached_surface->height= height;
	cached_surface->width_log2= width_log2;
	cached_surface->height_log2= height_log2;

	cached_surface->data[0]= ((unsigned char*)cached_surface) + sizeof(panzer_surf_cache_t);
	cached_surface->data[1]= cached_surface->data[0] + width * height * 4;
	cached_surface->data[2]= cached_surface->data[1] + width * height * 4 / 4;
	cached_surface->data[3]= cached_surface->data[2] + width * height * 4 / 16;
	for( int i= 0; i< 4; i++ )
		cached_surface->is_mips[i]= 0;
	cached_surface->image= NULL;

	//cached_surface->walkthrow_number= surfaces_cache.walkthrough_count;
	surf->surf_cache_walkthrough_number= surfaces_cache.walkthrough_count;
	//surf->surf_cache_generated_pos= surfaces_cache.current_pos_long;

	surf->cache= cached_surface;
}

bool IsCachedSurfaceValid( panzer_surf_cache_t* surf_cache, msurface_t* surf )
{
	if( surf_cache == NULL )
		return false;
	//if( surfaces_cache.current_pos > ((void*)surf_cache) && surf->surf_cache_walkthrough_number < surfaces_cache.walkthrough_count )
	//if( surfaces_cache.current_pos_long - surf->surf_cache_generated_pos >= surfaces_cache.allocated_size )
	unsigned long long int surf_distance= (unsigned long long int)surf->surf_cache_walkthrough_number;
	surf_distance= surf_distance * surfaces_cache.allocated_size;

	unsigned long long int cache_distance= (unsigned long long int)surfaces_cache.walkthrough_count;
	cache_distance= cache_distance * surfaces_cache.allocated_size;
	if( cache_distance - surf_distance >= surfaces_cache.allocated_size )
		return false;

	return true;
}

int IsSurfaceCachable( msurface_t* surf )
{
	if( surf->styles[1] != 255 )//if has dynamic lightmap
		return false;
	if( surf->dlightframe == r_dlightframecount )
		return false;

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

void GenerateSurfaceMip0( msurface_t* surf, panzer_surf_cache_t* surf_cache )
{
	current_lightmap_data= surf->samples;
	current_lightmap_size_x= (surf->extents[0]>>4) + 1;

	Texture* tex= R_FindTexture( surf_cache->image );
	const unsigned char* tex_data= tex->GetData();

	int tex_size_x1= tex->SizeX()-1;
	int tex_size_x_log2= tex->SizeXLog2();

	int min_x= surf->texturemins[0];
	int min_y= surf->texturemins[1];

	for( int y= 0; y< surf->extents[1]; y++ )
	{
		int tc_y= (y+min_y) & (tex->SizeY()-1);

		unsigned char* dst= surf_cache->data[0] + y * surf_cache->width * 4;
		const unsigned char* src= tex_data + (tc_y<<tex_size_x_log2);

		lightmap_dy=  (y<<(16-4-8)) & 255;
		lightmap_dy1= 256 - lightmap_dy;
		lightmap_y= y>>4;
		lightmap_y1= lightmap_y + 1;

		for( int x= 0; x< surf->extents[0]; x++, dst+= 4 )
		{
			unsigned char color[4];
			unsigned char light[4];

			int color_index= src[ (x+min_x)&tex_size_x1 ];
			((unsigned int*)color)[0]= d_8to24table[color_index];
			LightmapColoredFetch( x<<(16-4), light ); 

			int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
			c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
			c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;

			((unsigned int*)dst)[0]= ((unsigned int*)color)[0];
		}//for x
	}//for y
}

void GenerateSurfaceMip1( msurface_t* surf, panzer_surf_cache_t* surf_cache )
{
	current_lightmap_data= surf->samples;
	current_lightmap_size_x= (surf->extents[0]>>4) + 1;

	Texture* tex= R_FindTexture( surf_cache->image );
	const unsigned char* tex_data= tex->GetLodData(1);

	int tex_size_x1= tex->SizeX()/2 - 1;
	int tex_size_x_log2= tex->SizeXLog2()-1;

	int min_x= surf->texturemins[0]>>1;
	int min_y= surf->texturemins[1]>>1;

	for( int y= 0, y_end= surf->extents[1]>>1; y< y_end; y++ )
	{
		int tc_y= (y+min_y) & (tex->SizeY()/2-1);

		unsigned char* dst= surf_cache->data[1] + y * surf_cache->width * (4/2);
		const unsigned char* src= tex_data + (tc_y<<tex_size_x_log2);

		lightmap_dy=  (y<<(16-3-8)) & 255;
		lightmap_dy1= 256 - lightmap_dy;
		lightmap_y= y>>3;
		lightmap_y1= lightmap_y + 1;

		for( int x= 0, x_end= surf->extents[0]>>1; x< x_end; x++, dst+= 4 )
		{
			unsigned char color[4];
			unsigned char light[4];

			int color_index= src[ (x+min_x)&tex_size_x1 ];
			((unsigned int*)color)[0]= d_8to24table[color_index];
			LightmapColoredFetch( x<<(16-3), light ); 

			int c= (light[0] * 3 * color[0])>>8; if( c > 255 ) c= 255; color[0]= c;
			c= (light[1] * 3 * color[1])>>8; if( c > 255 ) c= 255; color[1]= c;
			c= (light[2] * 3 * color[2])>>8; if( c > 255 ) c= 255; color[2]= c;

			((unsigned int*)dst)[0]= ((unsigned int*)color)[0];
		}//for x
	}//for y
}

void GenerateSurfaceCache( msurface_t* surf, image_t* current_surf_image, int mip )
{
	panzer_surf_cache_t* surf_cache;
	if( !IsCachedSurfaceValid( surf->cache, surf ) )
		AllocSurface( surf );
	
	surf_cache= surf->cache;

	//if animated surface, or surface just yet allocated, or needed mip not builded
	if( surf_cache->image != current_surf_image || !surf_cache->is_mips[mip] )
	{
		surf_cache->image= current_surf_image;
		if( mip == 0 )
			GenerateSurfaceMip0( surf, surf_cache );
		else if( mip == 1 )
			GenerateSurfaceMip1( surf, surf_cache );
		else
			memset( surf_cache->data[mip], 128, (4 * surf_cache->width * surf_cache->height)>>(mip*2) );

		surf_cache->is_mips[mip]= 1;
	}
}
