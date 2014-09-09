#ifndef FAST_MATH_H
#define FAST_MATH_H

#include "psr.h"
#include "fixed.h"

#ifdef PSR_MASM32_INRINSINCS
#include <Intrin.h>
#endif

//returns 0 if x<= border, else returns 1
inline int FastIntStep( int border, int x )
{
	return int( ( (unsigned int)( border - x ) )>>31 );
}

inline int FastIntClampToZero( int x )
{
	return x&( ~(x>>31) );
}

inline int FastIntMax( int a, int b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		cmp eax, b
		cmovl eax, b//move b to eax, if less. eax is result of function
	}
#else
	int s= b-a;
	s= s>>31;
	return (a&s) | (b&(~s));
#endif
}

inline int FastIntMax( int a, int b, int c )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		cmp eax, b
		cmovl eax, b//move b to eax, if less. eax is result of function
		cmp eax, c
		cmovl eax, c
	}
#else
	return FastIntMax( a, FastIntMax( b, c ) );
#endif
}

inline int FastIntMax( int a, int b, int c, int d )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		cmp eax, b
		cmovl eax, b//move b to eax, if less. eax is result of function
		cmp eax, c
		cmovl eax, c
		cmp eax, d
		cmovl eax, d
	}
#else
	return FastIntMax( FastIntMax( a, b ), FastIntMax( c, d ) );
#endif
}

inline int FastIntMin( int a, int b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		cmp eax, b
		cmovge eax, b
	}
#else
	int s= a-b;
	s= s>>31;
	return (a&s) | (b&(~s));
#endif
}

inline int FastIntMin( int a, int b, int c )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		cmp eax, b
		cmovge eax, b
		cmp eax, c
		cmovge eax, c
	}
#else
	return FastIntMin( a, FastIntMin( b, c ) );
#endif
}

inline int FastIntMin( int a, int b, int c, int d )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, a
		cmp eax, b
		cmovge eax, b
		cmp eax, c
		cmovge eax, c
		cmp eax, d
		cmovge eax, d
	}
#else
	return FastIntMin( FastIntMin( a, b ), FastIntMin( c, d ) );
#endif
}



inline int FastIntClamp( int min, int max, int x )//clamp x to edges. if min> max, result undefined
{
	#ifdef PSR_MASM32
	__asm
	{
		mov eax, x
		cmp eax, max
		cmovge eax, max//in eax now FastIntMin( x, max )
		cmp eax, min
		cmovl eax, min//result in eax
	}
#else
	return FastIntMax( FastIntMin( x, max ), min );
#endif
}

inline int FastIntLog2( int x ) //returns index of first nonzero bit. if zero - result undefined
{
#ifdef PSR_MASM32
	__asm
	{
		mov ebx, x
		bsr eax, ebx// scan bits to first nonzero and put it index into eax
	}
#else
	int log = -1;
	int i= 1;
	while( i <= x )
	{
		i<<=1;
		log++;
	}
	return log;
#endif
}
inline int FastIntLog2Clamp0( int x ) //returns index of first nonzero bit. if zero - result is zero
{
#ifdef PSR_MASM32
	__asm
	{
		mov ebx, x
		bsr eax, ebx// scan bits to first nonzero and put it index into eax
		cmovz eax, ebx// if x is zero, move zero to result
	}
#else
	int log = -1;
	int i= 1;
	while( i <= x )
	{
		i<<=1;
		log++;
	}
	if( log == -1 ) log = 0;
	return log;
#endif
}

inline int FastIntAbs( int x )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, x
		neg eax
		cmovs eax, x
	}
#else
	int n_x= - x;
	int s= n_x>>31;
	return (x&s) | (n_x&(~s));
	//return ( x > 0 ) ? x : -x;
#endif
}

inline int FastIntNegAbs( int x )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, x
		neg eax
		cmovns eax, x
	}
#else
	int n_x= - x;
	int s= n_x>>31;
	return (n_x&s) | (x&(~s));
	//return ( x < 0 ) ? x : -x;
#endif
}

inline int FastIntSign( int x )
{
#ifdef PSR_MASM32
	__asm
	{
		mov eax, x
		//cmp eax, 0
		or eax, eax
		mov ebx, 1
		cmovg eax, ebx
		mov ebx, 0FFFFFFFFh ;-1
		cmovl eax, ebx
	}
#else
	if( x > 0 )
		return 1;
	else if( x < 0 )
		return -1;
	else return 0;
#endif
}

// returns max( ( a * b )>>8, 255 ). if a is negative - result undefined
inline unsigned char FastIntUByteMulClamp255( int a, unsigned char b )
{
#ifdef PSR_MASM32
	__asm
	{
		movzx eax, b
		imul eax, a
		shr eax, 8 //final result in AL
		test eax, 0FFFFFF00h // test overflow
		mov ebx, 255
		cmovnz eax, ebx //correct result, if it > 255
	}
#else
	int c= ( a * int(b) )>>8;
	if( c > 255 ) c= 255;
	return (unsigned char)c;
#endif

}

//returns min( a + b, 255 )
inline unsigned char FastUByteAddClamp255( unsigned char a, unsigned char b )
{
#ifdef PSR_MASM32
	__asm
	{
		mov al, a
		add al, b
		mov ebx, 255
		cmovc eax, ebx //if overflow, result is 255
	}
#else
	unsigned int c= a + b;
	if( c > 255 ) c= 255;
	return (unsigned char)c;
#endif
}

//this method must be faster, becouse compiler by default uses 64/32 division. this uses 32/16
//but quotient MUST be stored in short, otherwise program will be crashed.
inline short FastIntShortDiv( int x, short y )
{
#ifdef PSR_MASM32
	__asm
	{
		mov edx, x
		mov eax, edx
		shr edx, 16
		idiv y// devide dx:ax/y and put result into ax
	}
#else
	return x/y;
#endif
}

inline unsigned short FastUIntUShortDiv( unsigned int x, unsigned short y )
{
#ifdef PSR_MASM32
	__asm
	{
		mov edx, x
		mov eax, edx
		shr edx, 16
		div y// devide dx:ax/y and put result into ax
	}
#else
	return x/y;
#endif
}

inline char FastShortByteDiv( short x, char y )
{
	#ifdef PSR_MASM32
	__asm
	{
		mov ax, x
		idiv y
	}
#else
	return x/y;
#endif
}

inline unsigned char FastUShortUByteDiv( unsigned short x, unsigned char y )
{
	#ifdef PSR_MASM32
	__asm
	{
		mov ax, x
		div y
	}
#else
	return x/y;
#endif
}

inline char FastByteDiv( char x, char y )
{
	#ifdef PSR_MASM32
	__asm
	{
		mov al, x
		cbw
		idiv y
	}
#else
	return x/y;
#endif
}

inline unsigned char FastUByteDiv( unsigned char x, unsigned char y )
{
	#ifdef PSR_MASM32
	__asm
	{
		xor eax, eax
		mov al, x
		div y
	}
#else
	return x/y;
#endif
}

//returns 2^( floor(e) ) * ( 1 + mod(e,1)*log(2) + (mod(e,1)*log(2))^2/2 + (mod(e,1)*log(2))^3/6 + (mod(e,1)*log(2))^4/24 )
/*inline Fixed16 FastFixed16Exp2( Fixed16 e )
{
	int i_exp= 1<<e.ToInt();

	Fixed16 delta; delta.x= e.x & 0xFFFF;
	Fixed16 r; r.x= 65536;
	Fixed16 ln2; ln2.x= 45426;
	delta*= ln2;
	Fixed16 d= delta;
	r+= delta;
	delta*= d;
	r.x+= delta.x >> 1;
	delta*= d;
	r.x+= delta.x /6;
	delta*= d;
	r.x+= delta.x /24;
	return Fixed16( i_exp ) * r;
}*/


#endif//FAST_MATH_H