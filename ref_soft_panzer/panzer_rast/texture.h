#ifndef TEXTURE_H
#define TEXTURE_H
#include "psr.h"

//rgba texture
class Texture
{
public:

	enum ResizeMode
	{
		RESIZE_NO,
		RESIZE_STRETCH,
		RESIZE_FILL
	};
	Texture();
	/*allocate memory for texture, copy data from (*data) ( if it not null ),
	resize to upper POT size, build lods
	*/
	void Create( int width, int height, bool palettized, const unsigned char* pelette, const unsigned char* data, ResizeMode resize_mode= RESIZE_STRETCH, bool build_palettized_lods= false );
	int SizeX()const;
	int SizeY()const;
	int OriginalSizeX()const;
	int OriginalSizeY()const;
	int SizeXLog2()const;
	int SizeYLog2()const;
	int MaxLod()const;

	const unsigned char* GetData() const;
	const unsigned char* GetLodData( int lod ) const;
	const unsigned char* GetPalette() const;
	bool IsPlaettized()const;

	int Load( const char* file_name );
	void Convert2GRBS();
	void Convert2RGBAFromPlaettized();
	void SetColorKeyToAlpha( const unsigned char* color_key, unsigned char alpha );//work only for RGBA textures
	void FindAndReplaceColor( const unsigned char* color_key, const unsigned char* new_color );
	void SwapRedBlueChannles();
private:

	void GenLodPlaettized( int sx, int sy, unsigned char* in_data, unsigned char* out_data );//generate lod for Plaettized texture. method is slow, and alpha of lods undefined
	static void GenLod( int sx, int sy, const unsigned char* in_data, unsigned char* out_data );
	static void RGB2RGBS( int sx, int sy, unsigned char* in_out_data );
	
	int LoadTextureTGA( const char* filename );
	int LoadTexturePCX( const char* filename );
	int LoadTextureWAL( const char* filename );

	unsigned char FindNearestColorInPalette( unsigned char* color );

	//init inner texture data, copy in data and resize
	void ResizeToNearestPOTCeil( const unsigned char* in_data, ResizeMode resize_mode );

	unsigned char* data;
	unsigned char* lods_data[ PSR_MAX_TEXTURE_SIZE_LOG2 ];
	unsigned char* palette;
	int original_size_x, original_size_y;
	//size = 2^ceil( log2(original_size) )
	int size_x, size_y;
	int size_x_log2, size_y_log2;
	int max_lod;
	bool is_palettized_texture;
};


inline const unsigned char* Texture::GetData() const
{
	return data;
}

inline const unsigned char* Texture::GetPalette() const
{
	return palette;
}

inline const unsigned char* Texture::GetLodData( int lod ) const
{
	return lods_data[ lod ];
}

inline int Texture::SizeX()const
{
	return size_x;
}
inline int Texture::SizeY()const
{
	return size_y;
}

inline int Texture::OriginalSizeX()const
{
	return original_size_x;
}
inline int Texture::OriginalSizeY()const
{
	return original_size_y;
}

inline int Texture::MaxLod()const
{
	return max_lod;
}

inline int Texture::SizeXLog2()const
{
	return size_x_log2;
}
inline int Texture::SizeYLog2()const
{
	return size_y_log2;
}

inline bool Texture::IsPlaettized()const
{
	return is_palettized_texture;
}

#endif//TEXTURE_H