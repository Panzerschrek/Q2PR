#include "r_local.h"
#include "panzer_rast/texture.h"



extern "C" void R_LoadWal (char *name, image_t* image );



#define	MAX_RIMAGES	1024
image_t		r_images[MAX_RIMAGES];
image_t		white_image;
Texture		textures[MAX_RIMAGES];
Texture		white_texture;
int			numr_images;

void InitWhiteImage()
{
	const int white_tex_size= 16;

	white_image.height= white_tex_size;
	white_image.width= white_tex_size;
	white_image.pixels[0]= (byte*) malloc(white_tex_size*white_tex_size);

	static unsigned char white_img_data[ white_tex_size * white_tex_size * 4 ];
	memset( white_img_data, 128, sizeof(white_img_data) );

	white_texture.Create( white_tex_size, white_tex_size, false, NULL, white_img_data, Texture::RESIZE_NO );
}


image_t* R_FindFreeImage()
{
	for( int i= 0; i< MAX_RIMAGES; i++ )
	{
		if( r_images[i].registration_sequence == 0 )//free image
		{
			if( i == numr_images )
				numr_images++;
			return r_images + i;
		}
	}
	ri.Sys_Error (ERR_DROP, "MAX_RIMAGES");
	return NULL;
}



Texture* R_FindTexture(image_t* image)
{
	int i= image - r_images;
	if( i >=0 && i< MAX_RIMAGES )
		return textures + i;

	if( white_texture.GetData() == NULL )
		InitWhiteImage();
	return &white_texture;
}

Texture* R_FindTexture( char* name )
{
	for( int i= 0; i< numr_images; i++ )
		if( ! strcmp( name, r_images[i].name ) )
		{
			r_images[i].registration_sequence= registration_sequence;
			return textures + i;
		}

	if( white_texture.GetData() == NULL )
		InitWhiteImage();
	return &white_texture;
}

int R_GetImageIndex(image_t* image)
{
	int i= image - r_images;
	if(i<0)
		i= 0;
	if( i >= MAX_RIMAGES )
		i= MAX_RIMAGES-1;
	return i;
}


extern "C" image_t	*R_FindImage (char *name, imagetype_t type)
{
	bool need_resize_to_pot, need_convert_to_rgba;

	for( int i= 0; i< numr_images; i++ )
		if( ! strcmp( name, r_images[i].name ) )
		{
			r_images[i].registration_sequence= registration_sequence;
			return r_images + i;
		}


	image_t* img= R_FindFreeImage();	
	int len= strlen( name );
	if ( !strcmp(name+len-4, ".tga") )
	{
		need_resize_to_pot= true;
		need_convert_to_rgba= false;
	}
	else if ( !strcmp(name+len-4, ".pcx") )
	{
		LoadPCX( name, img->pixels, NULL, &img->width, &img->height );
		img->registration_sequence = registration_sequence;
		strcpy( img->name, name );
		img->type= type;
		if( type == it_skin )
			need_resize_to_pot= true;
		else
			need_resize_to_pot= false;
		need_convert_to_rgba= true;
	}
	else if( !strcmp(name+len-4, ".wal") )
	{
		R_LoadWal( name, img );
		img->registration_sequence = registration_sequence;
		need_resize_to_pot= true;
		need_convert_to_rgba= false;
	}
	else
		img= NULL;

	if( img != NULL )
	{
		img->transparent= false;
		for( int i= 0; i< img->width * img->height; i++ )
		{
			if( img->pixels[0][i] == 255 )//transparent
			{
				img->transparent= true;
				break;
			}
		}
		
		bool need_build_palettized_lods= r_palettized_textures->value != 0.0f && type == it_wall;
		int t= img - r_images;

		Texture::ResizeMode resize_mode;
		if( need_resize_to_pot )
		{
			if( type == it_skin )
				resize_mode= Texture::RESIZE_FILL;
			else
				resize_mode= Texture::RESIZE_STRETCH;
		}
		else
			resize_mode= Texture::RESIZE_NO;

		textures[t].Create( img->width, img->height,
			true, (unsigned char*)d_8to24table,
			img->pixels[0],
			resize_mode, need_build_palettized_lods );

		if( need_convert_to_rgba  || r_palettized_textures->value == 0.0f )
		{
			textures[t].Convert2RGBAFromPlaettized();
			if( img->transparent )
			{
				int alpha_color= d_8to24table[255];
				textures[t].SetColorKeyToAlpha( (unsigned char*)&alpha_color, 255 );
			}
		}
	}
	else
		return NULL;

	strcpy( img->name, name );

	return img;
}

extern "C" void R_FreeUnusedImages()
{
	for( int i= 0; i< numr_images; i++ )
	{
		image_t* img= r_images + i;

		if( img->type == it_pic )
			continue;
		if( img->registration_sequence == 0 )//free image
			continue;
		if( img->registration_sequence != registration_sequence )
		{
			img->registration_sequence = 0;
			free (img->pixels[0]);
			memset (img, 0, sizeof(image_t));
			//free this image
			//TODO - do free work
		}
		else
		{
		}
	}
}