#ifndef RENDERING_SATE_H
#define RENDERING_SATE_H


#define DEPTH_RANGE_MIN 0
#define DEPTH_RANGE_MAX 536870912L//(1<<29)

enum ColorMode
{
	COLOR_CONSTANT, 
	COLOR_PER_VERTEX,
	COLOR_FROM_TEXTURE
};

enum TextureMode
{
	TEXTURE_NONE,// color mode - per vertex
	TEXTURE_NEAREST,
	TEXTURE_FAKE_FILTER, //add 1/2 of pixel size to texture coord for 1/2 of pixels
	TEXTURE_LINEAR,
	TEXTURE_NEAREST_MIPMAP,
	TEXTURE_FAKE_FILTER_MIPMAP,
	TEXTURE_RGBS_LINEAR,

	TEXTURE_PALETTIZED_NEAREST,
	TEXTURE_PALETTIZED_LINEAR,
	TEXTURE_PALETTIZED_FAKE_FILTER,
	TEXTURE_PALETTIZED_NEAREST_MIPMAP,
	TEXTURE_PALETTIZED_FAKE_FILTER_MIPMAP
};

enum LightingMode
{
	LIGHTING_NONE,
	LIGHTING_CONSTANT,
	LIGHTING_PER_VERTEX,
	LIGHTING_FROM_LIGHTMAP,
	LIGHTING_FROM_LIGHTMAP_OVERBRIGHT,// light= lightmap_light * constant_light
	LIGHTING_FROM_LIGHTMAP_OVERBRIGHT2// light= lightmap_light * 2
};

enum AdditionalLightingMode
{
	ADDITIONAL_LIGHTING_NONE,
	ADDITIONAL_LIGHTING_POINT
};

enum LightmapMode
{
	LIGHTMAP_NEAREST,
	LIGHTMAP_LINEAR,
	LIGHTMAP_FAKE_FILTER,
	LIGHTMAP_COLORED_NEAREST,
	LIGHTMAP_COLORED_LINEAR,
	LIGHTMAP_COLORED_RGBS_LINEAR,
	LIGHTMAP_COLORED_FAKE_FILTER
};

enum BlendingMode
{
	BLENDING_NONE,
	BLENDING_CONSTANT,
	BLENDING_AVG,// dst = ( src + dst ) / 2
	BLENDING_SRC_ALPHA,// dst= ( src * src.a + dst * ( 255 - src.a ) ) >> 8
	BLENDING_FAKE,// discard 1/2 of pixels
	BLENDING_ADD, //dst= src + dst
	BLENDING_MUL, //dst= ( src * dst ) >> 8
};

enum AlphaTestMode
{
	ALPHA_TEST_NONE,
	ALPHA_TEST_LESS,
	ALPHA_TEST_GREATER,
	ALPHA_TEST_EQUAL,
	ALPHA_TEST_NOT_EQUAL,
	ALPHA_TEST_DISCARD_LESS_HALF,// discard pixel if alpha less then half ( 128 )
	ALPHA_TEST_DISCARD_GREATER_HALF,
};

enum DepthTestMode
{
	DEPTH_TEST_NONE=0,
	DEPTH_TEST_ALWAYS=0,
	DEPTH_TEST_LESS,
	DEPTH_TEST_GREATER,
	DEPTH_TEST_EQUAL,
	DEPTH_TEST_NOT_EQUAL
};

#endif//RENDERING_SATE_H
