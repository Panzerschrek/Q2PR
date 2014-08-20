#ifndef FIXED_H
#define FIXED_H
#include "psr.h"

inline fixed16_t Fixed16Round( fixed16_t x )
{
	return (x&0xFFFF8000) + (x&0x00008000);
}

inline fixed16_t Fixed16Floor( fixed16_t x )
{
	return x&0xFFFF0000;
}
inline fixed16_t Fixed16Ceil( fixed16_t x )
{
	return x + ((-x)&0xFFFF);
}

inline fixed16_t Fixed16Mul( fixed16_t a, fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		imul b
		shrd eax, edx, 16//result into eax
	}
#else
	long long int y= ((long long int)a) * b;
	return int(y>>16);
#endif
}

inline fixed16_t Fixed16Square( fixed16_t x )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, x
		imul eax
		shrd eax, edx, 16//result into eax
	}
#else
	long long int y= ((long long int)x) * x;
	return int(y>>16);
#endif
}


inline int Fixed16SquareResultToInt( fixed16_t x )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, x
		imul eax
		mov eax, edx
	}
#else
	long long int y= ((long long int)x) * x;
	return int(y>>32);
#endif
}

inline fixed16_t Fixed16Div( fixed16_t a, fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		cdq//write into edx sign of eax
		shld edx, eax, 16
		shl eax, 16
		idiv b//result in eax
	}
#else
	long long int y= a;
	y<<=16;
	return int( y / b );
#endif
}




inline fixed16_t Fixed16Invert( fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		xor eax, eax
		mov edx, 1
		idiv b//result in eax
	}
#else
	//long long int y(0x100000000L);
	long long int y(4294967296LL);
	return int( y / b );
#endif
}

inline int Fixed16MulResultToInt( fixed16_t a, fixed16_t b )
{
	#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		imul b
		mov eax, edx //result into eax
	}
#else
	long long int y= ((long long int)a) * b;
	return int(y>>32);
#endif
}
inline fixed16_t Fixed16MulResultShl2( fixed16_t a, fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		imul b
		shrd eax, edx, (16+2)//result into eax
	}
#else
	long long int y= ((long long int)a) * b;
	return int(y>>(16+2));
#endif
}

inline fixed16_t Fixed16MulResultShl3( fixed16_t a, fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		imul b
		shrd eax, edx, (16+3)//result into eax
	}
#else
	long long int y= ((long long int)a) * b;
	return int(y>>(16+3));
#endif
}

inline fixed16_t Fixed16MulResultShl4( fixed16_t a, fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		imul b
		shrd eax, edx, (16+4)//result into eax
	}
#else
	long long int y= ((long long int)a) * b;
	return int(y>>(16+4));
#endif
}

inline fixed16_t Fixed16MulResultShl5( fixed16_t a, fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		imul b
		shrd eax, edx, (16+5)//result into eax
	}
#else
	long long int y= ((long long int)a) * b;
	return int(y>>(16+5));
#endif
}

inline fixed16_t Fixed16MulResultShl6( fixed16_t a, fixed16_t b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		imul b
		shrd eax, edx, (16+6)//result into eax
	}
#else
	long long int y= ((long long int)a) * b;
	return int(y>>(16+6));
#endif
}


//returns a * k +  b * ( 1.0 - k )
inline fixed16_t Fixed16Lerp( fixed16_t a, fixed16_t b, fixed16_t k )
{
#ifdef PSR_MASM32
	__asm
	{
		// ebx - k, ecx - 1st multiplication result
		mov ebx, k
		mov eax, a
		imul ebx
		shrd eax, edx, 16
		mov ecx, eax
		mov eax, 65536
		sub eax, ebx
		imul b
		shrd eax, edx, 16
		add eax, ecx//result into eax
	}
#else
	return Fixed16Mul( a, k ) + Fixed16Mul( b, 65536 - k );
#endif
}

inline fixed16_t Fixed16Vec3Dot( fixed16_t* v1, fixed16_t* v2 )
{
#ifdef PSR_MASM32
	__asm
	{
		mov esi, v1
		mov edi, v2
		xor ecx, ecx

		mov eax, dword ptr[esi]
		imul dword ptr[edi]
		shrd eax, edx, 16
		add ecx, eax
		mov eax, dword ptr[esi+4]
		imul dword ptr[edi+4]
		shrd eax, edx, 16
		add ecx, eax
		mov eax, dword ptr[esi+8]
		imul dword ptr[edi+8]
		shrd eax, edx, 16
		add eax, ecx// add to reuslt of 3-rd multiplication previous sum. now, result into eax
	}
#else
	return Fixed16Mul( v1[0], v2[0] ) + Fixed16Mul( v1[1], v2[1] ) + Fixed16Mul( v1[2], v2[2] );
#endif
}

// returns 1.0 / dot( v, v )
inline fixed16_t Fixed16Vec3InvSqr( fixed16_t* v )
{
#ifdef PSR_MASM32
	__asm
	{
		mov esi, v
		xor ecx, ecx

		mov eax, dword ptr[esi]
		imul eax
		shrd eax, edx, 16
		add ecx, eax
		mov eax, dword ptr[esi+4]
		imul eax
		shrd eax, edx, 16
		add ecx, eax
		mov eax, dword ptr[esi+8]
		imul eax
		shrd eax, edx, 16
		add ecx, eax

		xor eax, eax
		mov edx, 1
		idiv ecx
	}
#else
	fixed16_t result= Fixed16Mul( v[0], v[0] ) + Fixed16Mul( v[1], v[1] ) + Fixed16Mul( v[2], v[2] );
	return Fixed16Invert( result );
#endif
}

// returns  x / dot( v, v )
inline fixed16_t Fixed16Vec3DivSqr( fixed16_t x, fixed16_t* v )
{
#ifdef PSR_MASM32
	__asm
	{
		mov esi, v
		xor ecx, ecx

		mov eax, dword ptr[esi]
		imul eax
		shrd eax, edx, 16
		add ecx, eax
		mov eax, dword ptr[esi+4]
		imul eax
		shrd eax, edx, 16
		add ecx, eax
		mov eax, dword ptr[esi+8]
		imul eax
		shrd eax, edx, 16
		add ecx, eax

		mov eax, x
		cdq//write into edx sign of eax
		shld edx, eax, 16
		shl eax, 16
		idiv ecx//result in eax
	}
#else
	fixed16_t result= Fixed16Mul( v[0], v[0] ) + Fixed16Mul( v[1], v[1] ) + Fixed16Mul( v[2], v[2] );
	return Fixed16Div( x, result );
#endif
}

#endif//FIXED_H
