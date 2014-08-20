#ifndef PSR_MMX_H
#define PSR_MMX_H

#ifdef PSR_USE_MMX

#ifdef PSR_MASM32_INRINSINCS
#include <Intrin.h>
#endif

inline void PSR_MMX_Emms()
{
	#ifdef PSR_MASM32
	__asm emms
	#elif PSR_MASM32_INRINSINCS
	
	#else
	#endif//PSR_MASM_32
}

//add 4 bytes of src to dst bytes and clamp to 255
inline void PSR_MMX_AddColor( unsigned char* dst_color, unsigned char* src_color )
{
	#ifdef PSR_MASM32
	__asm
	{
		mov edi, dst_color
		mov esi, src_color
		movd mm0, dword ptr[edi]
		paddusb mm0, [esi]//unsigned add and clamp to 255
		movd dword ptr[edi], mm0
	}
	#elif PSR_MASM32_INRINSINCS
	__m64 dc, sc;
	dc= _mm_cvtsi32_si64( *((int*)dst_color) );
	sc= _mm_cvtsi32_si64( *((int*)src_color) );
	__m64 x= _m_paddsb( dc, sc );
	*((int*)dst_color)= _mm_cvtsi64_si32(x);
	#else
	#endif
}

//dst_color= ( dst_color * src_color )>>8
inline void PSR_MMX_MulColor( unsigned char *dst_color, unsigned char* src_color )
{
#ifdef PSR_MASM32
	__asm
	{
		mov edi, dst_color
		mov esi, src_color
		movd mm0, dword ptr[edi]
		pxor mm2, mm2//set zero
		movd mm1, dword ptr[esi]
		punpcklbw mm0, mm2//conver color bytes to words
		punpcklbw mm1, mm2
		pmullw mm0, mm1//make multiplication
		psrlw mm0, 8//devide by 256
		
		packuswb mm0, mm1//convert words to bytes
		movd dword ptr[edi], mm0
	}
#elif PSR_MASM32_INRINSINCS
	__m64 dc= _mm_cvtsi32_si64( *((int*)dst_color ) ),
		sc= _mm_cvtsi32_si64( *((int*)src_color ) );
	__m64 zero;
	int z= 0;
	zero= _mm_setzero_si64();
	dc= _m_punpcklbw( dc, zero );
	sc= _m_punpcklbw( sc, zero );
	dc= _m_pmullw( dc, sc );
	dc= _m_psrlwi( dc, 8 );
	dc= _m_packuswb( dc, zero );
	*((int*)dst_color)= _mm_cvtsi64_si32( dc );
#else
#endif
}

//return min( color * light / 255, 255 ). result of alha component undefined
inline void PSR_MMX_LightColor( unsigned char *dst_color, unsigned short light )
{
#ifdef PSR_MASM32
	__asm
	{
		//make 4d vector with light
		sub esp, 8
		mov ax, light
		mov word ptr[esp-6], ax
		mov word ptr[esp-4], ax
		mov word ptr[esp-2], ax
		//mov word ptr[esp-0], ax
		//convert bytes to words
		mov edi, dst_color
		pxor mm0, mm0
		movd mm1, dword ptr[edi]
		punpcklbw mm0, mm1// result words = dst_color<<8
		//calculate result
		pmulhuw mm0, qword ptr[esp-6]//highter parts of multiplication
		//convert words to bytes and save result
		packuswb mm0, mm1
		movd dword ptr[edi], mm0

		add esp, 8
	}
#elif PSR_MASM64
	unsigned short l[4]= { light, light, light, light };
	__m64 dc, zero;
	dc= _mm_cvtsi32_si64( *((int*)dst_color ) );
	zero= _mm_setzero_si64();
	dc= _m_punpcklbw( zero, dc );
	dc= _m_pmulhuw( dc, *((__m64*)l) );
	dc= _m_packuswb( dc, zero );
	*((int*)dst_color)= _mm_cvtsi64_si32( dc );
#else
#endif
}

inline void PSR_MMX_LightColor( unsigned char *dst_color, unsigned short* light/*4D vector*/ )
{
#ifdef PSR_MASM32
	__asm
	{
		mov edi, dst_color
		mov ebx, light
		pxor mm0, mm0
		movd mm1, dword ptr[edi]
		punpcklbw mm0, mm1
		pmulhuw mm0, qword ptr[ebx]

		packuswb mm0, mm1
		movd dword ptr[edi], mm0
	}
#elif PSR_MASM64
	
#else

#endif
}

// dst_color= ( dst_color * ( 255 - blend_factor ) + src_color * blend_factor )>>8 . result of alpha component undefined
inline void PSR_MMX_BlendColor( unsigned char* dst_color, unsigned char* src_color, unsigned char blend_factor )
{
#ifdef PSR_MASM32
	__asm
	{
		movzx eax, blend_factor
		sub esp, 16
		mov word ptr[esp-6], ax
		mov word ptr[esp-4], ax
		mov word ptr[esp-2], ax
		//mov word ptr[esp-0], ax
		not al
		mov word ptr[esp-14], ax
		mov word ptr[esp-12], ax
		mov word ptr[esp-10], ax
		//mov word ptr[esp-8], ax

		mov edi, dst_color
		mov esi, src_color
		movd mm0, dword ptr[edi]
		pxor mm1, mm1
		movd mm3, dword ptr[esi]
		punpcklbw mm0, mm1// word size dst color
		punpcklbw mm3, mm1// word size src color

		pmullw mm0, qword ptr[esp-14]// dst_color*= inv_blend_factor
		pmullw mm3, qword ptr[esp-6]// src_color*= blend_factor
		//add and devide by 256
		paddw mm0, mm3
		psrlw mm0, 8
		//convert words to bytes. mm1 is zero
		packuswb mm0, mm1
		movd dword ptr[edi], mm0

		add esp, 16
	}
#elif PSR_MASM64
	
#else

#endif
}

//dst_color= ( dst_color + src_color )/2
inline void PSR_MMX_AvgColor( unsigned char* dst_color, unsigned char* src_color )
{
#ifdef PSR_MASM32
	__asm
	{
		mov esi, src_color
		mov edi, dst_color
		
		movd mm0, dword ptr[edi]
		pavgb mm0, dword ptr[esi]
		movd dword ptr[edi], mm0
		
	}
#elif PSR_MASM64
	
#else

#endif
}

//returns  max( ( dst_color * lightmap * light )>>16, 255 ). result of alpha component undefined
inline void PSR_MMX_ColoredLightingOverbright( unsigned char* dst_color, unsigned char* lightmap, unsigned short light )
{
#ifdef PSR_MASM32
	__asm
	{
		sub esp, 8
		mov ax, light
		mov word ptr[esp-6], ax
		mov word ptr[esp-4], ax
		mov word ptr[esp-2], ax
		//mov word ptr[esp-0], ax

		mov edi, dst_color
		mov esi, lightmap
		//move color and lightmap data to registers and convert to word format
		movd mm0, dword ptr[edi]
		pxor mm3, mm3
		movd mm1, dword ptr[esi]
		punpcklbw mm0, mm3
		punpcklbw mm1, mm3
		//dst_color*= src_color
		pmullw mm0, mm1
		//dst_color= ( dst_color * light )>>16
		pmulhuw mm0, qword ptr[esp-6]

		packuswb mm0, mm3
		movd dword ptr[edi], mm0
		add esp, 8
	}
#elif PSR_MASM64
	
#else

#endif
}

inline void PSR_MMX_ColoredLightingOverbright( unsigned char* dst_color, unsigned char* lightmap, unsigned short* light/*4d vector*/ )
{
#ifdef PSR_MASM32
	__asm
	{
		mov ebx, light
		mov edi, dst_color
		mov esi, lightmap
		//move color and lightmap data to registers and convert to word format
		movd mm0, dword ptr[edi]
		pxor mm3, mm3
		movd mm1, dword ptr[esi]
		punpcklbw mm0, mm3
		punpcklbw mm1, mm3
		//dst_color*= src_color
		pmullw mm0, mm1
		//dst_color= ( dst_color * light )>>16
		pmulhuw mm0, qword ptr[ebx]

		packuswb mm0, mm3
		movd dword ptr[edi], mm0
	}
#elif PSR_MASM64
	
#else
#endif

}
#endif//PSR_USE_MMX

#endif//PSR_MMX_H