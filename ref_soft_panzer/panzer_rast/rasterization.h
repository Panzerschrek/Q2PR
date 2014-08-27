#ifndef RASTERIZATION_H
#define RASTERIZATION_H
#include "psr.h"


#ifdef __cplusplus
extern "C"
{
#endif


void PRast_Init( int screen_width, int screen_height );
void PRast_Shutdown();
void PRast_SetFramebuffer( unsigned char* buff );
unsigned char* PRast_GetFrontBuffer();
unsigned char* PRast_SwapBuffers();
void PRast_SwapRedBlueLostAlphaInFramebuffer();
void PRast_SwapRedBlueInFramebuffer();
void PRast_MirrorFramebufferVertical();
void PRast_MakeGammaCorrection(const unsigned char* gamma_table);
//void ClearDepthBufferByZValue( fixed16_t z );
//void SwapBuffers();

//common draw functions
void ClearColorBuffer( const unsigned char* color );
void ClearDepthBuffer( depth_buffer_t value );
void AlphaBlendColorBuffer( const unsigned char* color );
void AlphaBlendColorBufferAndSwapRedBlueAlphaLost( const unsigned char* color );


extern int (*DrawWorldTriangleToBuffer)( char* buff );//draw textured lightmapped triangle
//world rendering functions
extern void (*DrawWorldTriangleTextureNearestLightmapLinear)(char*buff);//nearest textures
extern void (*DrawWorldTriangleTextureNearestLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureNearestLightmapColoredLinear)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapLinear)(char*buff);//liear textures
extern void (*DrawWorldTriangleTextureLinearLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapColoredLinear)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapLinear)(char*buff);//fake filter textures
extern void (*DrawWorldTriangleTextureFakeFilterLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapColoredLinear)(char*buff);
//world rendering functions with avg blend
extern void (*DrawWorldTriangleTextureNearestLightmapLinearBlend)(char*buff);//nearest textures
extern void (*DrawWorldTriangleTextureNearestLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureNearestLightmapColoredLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapLinearBlend)(char*buff);//liear textures
extern void (*DrawWorldTriangleTextureLinearLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapColoredLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapLinearBlend)(char*buff);//fake filter textures
extern void (*DrawWorldTriangleTextureFakeFilterLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapColoredLinearBlend)(char*buff);

//world rendering functions PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLinear)(char*buff);//nearest textures PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinear)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLinear)(char*buff);//liear textures PALETTIZED
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinear)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinear)(char*buff);//fake filter textures PALETTIZED
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinear)(char*buff);
//world rendering functions with avg blend PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLinearBlend)(char*buff);//nearest textures PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLinearBlend)(char*buff);//liear textures PALETTIZED
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinearBlend)(char*buff);//fake filter textures PALETTIZED
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinearBlend)(char*buff);


extern int (*DrawWorldTriangleNoLightmapToBuffer)(char* buff);//for turbulence and no_lightmap surfaces
//world rendering turblulence functions
extern void (*DrawWorldTriangleTextureNearestTurbulence)(char*buff);
extern void (*DrawWorldTriangleTextureLinearTurbulence)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterTurbulence)(char*buff);
extern void (*DrawWorldTriangleTextureNearestTurbulenceBlend)(char*buff);//with blend
extern void (*DrawWorldTriangleTextureLinearTurbulenceBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterTurbulenceBlend)(char*buff);
//world rendering turblulence functions PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettizedTurbulence)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedTurbulence)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedTurbulence)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedTurbulenceBlend)(char*buff);//with blend
extern void (*DrawWorldTriangleTextureLinearPalettizedTurbulenceBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedTurbulenceBlend)(char*buff);

/*
//world rendering functions without lightmaps
extern void (*DrawWorldTriangleTextureNearest)(char*buff);
extern void (*DrawWorldTriangleTextureLinear)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilter)(char*buff);
extern void (*DrawWorldTriangleTextureNearestBlend)(char*buff);//with blend
extern void (*DrawWorldTriangleTextureLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterBlend)(char*buff);
//world rendering functions without lightmaps PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettized)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettized)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettized)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedBlend)(char*buff);//with blend
extern void (*DrawWorldTriangleTextureLinearPalettizedBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedBlend)(char*buff);
*/


extern void (*DrawWorldSprite)(int x0, int y0, int x1, int y1, fixed16_t depth);

extern int (*DrawSkyTriangleToBuffer)( char* buff );
extern void (*DrawSkyTriangleNearest)( char* buff );
extern void (*DrawSkyTriangleLinear)( char* buff );
extern void (*DrawSkyTriangleFakeFilter)( char* buff );



extern int (*DrawTexturedModelTriangleToBuffer)(char*buff);
extern int (*DrawFullbrightTexturedModelTriangleToBuffer)(char*buff);
//default models functions
extern void (*DrawModelTriangleTextureNearestLightingColored)( char* buff );
extern void (*DrawModelTriangleTextureLinearLightingColored)( char* buff );
extern void (*DrawModelTriangleTextureFakeFilterLightingColored)( char* buff );
//default models functions with blend
extern void (*DrawModelTriangleTextureNearestLightingColoredBlend)( char* buff );
extern void (*DrawModelTriangleTextureLinearLightingColoredBlend)( char* buff );
extern void (*DrawModelTriangleTextureFakeFilterLightingColoredBlend)( char* buff );
//fullbright models
extern void (*DrawModelTriangleTextureNearest)( char* buff );
extern void (*DrawModelTriangleTextureLinear)( char* buff );
extern void (*DrawModelTriangleTextureFakeFilter)( char* buff );
//fullbright models with blend
extern void (*DrawModelTriangleTextureNearestBlend)( char* buff );
extern void (*DrawModelTriangleTextureLinearBlend)( char* buff );
extern void (*DrawModelTriangleTextureFakeFilterBlend)( char* buff );

//models functions withound blend and with deph hack
extern void (*DrawModelTriangleTextureNearestLightingColoredDepthHack)( char* buff );
extern void (*DrawModelTriangleTextureLinearLightingColoredDepthHack)( char* buff );
extern void (*DrawModelTriangleTextureFakeFilterLightingColoredDepthHack)( char* buff );

//for beams ( map lasers, bfg )
extern int  (*DrawBeamTriangleToBuffer)( char* buff );
extern void (*DrawBeamTriangle)( char* buff );

//in\out sprite drawing functions
extern void (*DrawParticleSpriteToBuffer)( char* buff, int x0, int y0, int x1, int y1, fixed16_t depth );
extern void (*DrawParticleSprite)(int x0, int y0, int x1, int y1, fixed16_t depth);
extern void (*DrawParticleSpriteNoBlend)(int x0, int y0, int x1, int y1, fixed16_t depth);

//based on Draw::DrawTexture and Draw::DrawTextureRectangle
extern void (*DrawTextureRGBA)( int x, int y);
extern void (*DrawTexturePalettized)( int x, int y);
extern void (*DrawTextureRectRGBA)( int x, int y, int u0, int v0, int u1, int v1 );
extern void (*DrawTextureRectPalettized)( int x, int y, int u0, int v0, int u1, int v1 );

//based on Draw::DrawSprite
extern void (*DrawStretchTextureRGBA)( int x0, int y0, int x1, int y1, fixed16_t depth );
extern void (*DrawStretchTextureRGBANoAlphaTest)( int x0, int y0, int x1, int y1, fixed16_t depth );
extern void (*DrawStretchTexturePalettized)( int x0, int y0, int x1, int y1, fixed16_t depth );
//fill rectangle with single color
extern void (*DrawFill)(  int x0, int y0, int x1, int y1, fixed16_t depth );

//divide all colors on screen
extern void (*FadeScreen)(void);


/*hack, for generation 1 mov instruction instead 3(4):
mov eax, dword ptr[src]
mov dword ptr [dst], eax
;instead
mov al, byte ptr[src]
mov byte ptr [dst], al
mov al, byte ptr[src+1]
mov byte ptr [dst+1], al
mov al, byte ptr[src+2]
mov byte ptr [dst+2], al
mov al, byte ptr[src+3]
mov byte ptr [dst+3], al
*/
#ifdef __cplusplus
inline void Byte4Copy( void* dst, const void* src )
{
    *((int*)dst)= *((int*)src);
}
#else
#define Byte4Copy( dst, src ) *((int*)(dst))=  *((int*)(src))
#endif



#ifdef __cplusplus
//extern "C"
}
#endif



#endif//RASTERIZATION