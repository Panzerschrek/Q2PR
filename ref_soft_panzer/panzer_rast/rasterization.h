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


extern int (*DrawWorldTriangleToBuffer)( char* buff );
//world rendering functions
extern void (*DrawWorldTriangleTextureNearestLightmapLinear)(char*buff);
extern void (*DrawWorldTriangleTextureNearestLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureNearestLightmapColoredLinear)(char*buff);

extern void (*DrawWorldTriangleTextureLinearLightmapLinear)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapColoredLinear)(char*buff);

extern void (*DrawWorldTriangleTextureFakeFilterLightmapLinear)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapColoredLinear)(char*buff);

//world rendering functions with avg blend
extern void (*DrawWorldTriangleTextureNearestLightmapLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureNearestLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureNearestLightmapColoredLinearBlend)(char*buff);

extern void (*DrawWorldTriangleTextureLinearLightmapLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearLightmapColoredLinearBlend)(char*buff);

extern void (*DrawWorldTriangleTextureFakeFilterLightmapLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterLightmapColoredLinearBlend)(char*buff);



//world rendering functions PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLinear)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinear)(char*buff);

extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLinear)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinear)(char*buff);

extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinear)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBS)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinear)(char*buff);

//world rendering functions with avg blend PALETTIZED
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureNearestPalettizedLightmapColoredLinearBlend)(char*buff);

extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureLinearPalettizedLightmapColoredLinearBlend)(char*buff);

extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLinearBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapLiearRGBSBlend)(char*buff);
extern void (*DrawWorldTriangleTextureFakeFilterPalettizedLightmapColoredLinearBlend)(char*buff);


extern void (*DrawWorldSprite)(int x0, int y0, int x1, int y1, fixed16_t depth);

extern int (*DrawSkyTriangleToBuffer)( char* buff );
extern void (*DrawSkyTriangleFromBuffer)( char* buff );

//for model drawing
extern int (*DrawMonoLightedTexturedTriangleToBuffer)( char* buff );
extern void (*DrawMonoLightedTexturedTriangleFromBuffer)( char* buff );

//more cooler model drawing
extern int (*DrawVertexLightedTexturedTriangleToBuffer)( char* buff );
extern void (*DrawVertexLightedTexturedTriangleFromBuffer)( char* buff );

//main map triangles draw functions
extern int(*DrawGrayscaleLightmappedTexturedTriangleToBuffer)(char* buff );
extern void(*DrawGrayscaleLightmappedTexturedTriangleFromBuffer)(char* buff );

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