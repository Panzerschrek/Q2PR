#include "psr.h"
#include "fixed.h"
#include <math.h>

extern void* framebuffer_data;
extern unsigned char* screen_buffer;
extern unsigned char* back_screen_buffer;
extern depth_buffer_t* depth_buffer;
extern int screen_size_x;
extern int screen_size_y;


extern "C" void PRast_MakeGammaCorrection( const unsigned char* gamma_table )
{
	register unsigned char* d= screen_buffer;
	register unsigned char* d_end= d + screen_size_x* screen_size_y * 4;
#ifdef PSR_MASM32
	__asm
	{
		mov esi, d
		mov edi, d_end

		mov ebx, gamma_table

		do_correction:
		mov al, byte ptr[esi  ]
		xlat
		mov byte ptr[esi  ], al
		mov al, byte ptr[esi+1]
		xlat
		mov byte ptr[esi+1], al
		mov al, byte ptr[esi+2]
		xlat
		mov byte ptr[esi+2], al
		mov al, byte ptr[esi+4]
		xlat
		mov byte ptr[esi+4], al
		mov al, byte ptr[esi+5]
		xlat
		mov byte ptr[esi+5], al
		mov al, byte ptr[esi+6]
		xlat
		mov byte ptr[esi+6], al

		/*mov ebx, gamma_table
		do_correction:
		movzx ecx, byte ptr[esi  ]
		mov al, byte ptr[ebx+ecx]
		mov byte ptr[esi  ], al
		movzx ecx, byte ptr[esi+1]
		mov al, byte ptr[ebx+ecx]
		mov byte ptr[esi+1], al
		movzx ecx, byte ptr[esi+2]
		mov al, byte ptr[ebx+ecx]
		mov byte ptr[esi+2], al

		movzx ecx, byte ptr[esi+4]
		mov al, byte ptr[ebx+ecx]
		mov byte ptr[esi+4], al
		movzx ecx, byte ptr[esi+5]
		mov al, byte ptr[ebx+ecx]
		mov byte ptr[esi+5], al
		movzx ecx, byte ptr[esi+6]
		mov al, byte ptr[ebx+ecx]
		mov byte ptr[esi+6], al*/

		add esi, 8
		cmp esi, edi
		jnz do_correction
	}
#else
	for( ; d< d_end; d+=16 )
	{
		d[0 ]= gamma_table[d[0 ]]; d[1 ]= gamma_table[d[1 ]];
		d[2 ]= gamma_table[d[2 ]]; /*d[3 ]= gamma_table[d[3 ]];*/
		d[4 ]= gamma_table[d[4 ]]; d[5 ]= gamma_table[d[5 ]];
		d[6 ]= gamma_table[d[6 ]]; /*d[7 ]= gamma_table[d[7 ]];*/
		d[8 ]= gamma_table[d[8 ]]; d[9 ]= gamma_table[d[9 ]];
		d[10]= gamma_table[d[10]]; /*d[11]= gamma_table[d[11]];*/
		d[12]= gamma_table[d[12]]; d[13]= gamma_table[d[13]];
		d[14]= gamma_table[d[14]]; /*d[15]= gamma_table[d[15]];*/
	}
#endif
}

#define FOG_TABLE_SIZE 4096
unsigned char fog_table[FOG_TABLE_SIZE];


extern "C" void PRast_AddFullscreenExponentialFog( float fog_half_distance, const unsigned char* color )
{
	const int depth2table_convert_k= 65536/FOG_TABLE_SIZE;

	float neg_ln_05= -0.69f;
	float depth_convert_k= neg_ln_05 / ( 65536.0f * fog_half_distance );

	fog_table[0]= 255;
	for( int i= 1; i< FOG_TABLE_SIZE; i++ )
	{
		fixed16_t depth= Fixed16Div( PSR_MIN_ZMIN, i * depth2table_convert_k ); //real depth in fixed16_t format

		//exponential fog formula. fog_distance is distance, where fog intencity is half
		fog_table[i]= int( 255.0f * ( 1.0f - exp( float(depth) * depth_convert_k ) ) );
	}

	if( false )
	{
		for( int i= 0, i_end= screen_size_x * screen_size_y * 4; i!= i_end; i+=4 )
		{
			int d= depth_buffer[i>>2];
			int dd= d % depth2table_convert_k;
			unsigned char  k[2]= {
				fog_table[ d     / depth2table_convert_k ],
				fog_table[ (d-1) / depth2table_convert_k ] };

			int a= ( k[1] * dd + k[0] * ( depth2table_convert_k - dd ) ) / depth2table_convert_k;
			int inv_a= 255 - a;
			screen_buffer[i  ]= ( screen_buffer[i  ] * inv_a + color[0] * a )>>8;
			screen_buffer[i+1]= ( screen_buffer[i+1] * inv_a + color[1] * a )>>8;
			screen_buffer[i+2]= ( screen_buffer[i+2] * inv_a + color[2] * a )>>8;
		}
	}//if interpolate values from fog table
	else
	{
		for( int i= 0, i_end= screen_size_x * screen_size_y * 4; i!= i_end; i+=4 )
		{
			int a= fog_table[ depth_buffer[i>>2] / depth2table_convert_k ];
			int inv_a= 255 - a;
			screen_buffer[i  ]= ( screen_buffer[i  ] * inv_a + color[0] * a )>>8;
			screen_buffer[i+1]= ( screen_buffer[i+1] * inv_a + color[1] * a )>>8;
			screen_buffer[i+2]= ( screen_buffer[i+2] * inv_a + color[2] * a )>>8;
		}
	}//if nearest fetch from fog table
}

extern "C" void PRast_MirrorFramebufferVertical()
{

#ifdef PSR_MASM32
	unsigned char* line0= screen_buffer;
	unsigned char* line1= screen_buffer + ((screen_size_x*(screen_size_y-1))<<2);
	unsigned int line_swap_count= screen_size_y>>1;
	int ssx_bytes= screen_size_x<<2;
	int ssx_bytes2= screen_size_x<<3;
	__asm
	{
		mov esi, line0
		mov edi, line1
		mov ecx, line_swap_count
		swap_line:
		push ecx

		mov ecx, ssx_bytes
		shr ecx, 5
		swap_bytes://swap per 8*4 bytes = 32 bytes = 8 pixels
		movq mm0, qword ptr[esi   ]
		movq mm1, qword ptr[esi+8 ]
		movq mm2, qword ptr[esi+16]
		movq mm3, qword ptr[esi+24]
		movq mm4, qword ptr[edi   ]
		movq mm5, qword ptr[edi+8 ]
		movq mm6, qword ptr[edi+16]
		movq mm7, qword ptr[edi+24]
		movq qword ptr[esi   ], mm4
		movq qword ptr[esi+8 ], mm5
		movq qword ptr[esi+16], mm6
		movq qword ptr[esi+24], mm7
		movq qword ptr[edi   ], mm0
		movq qword ptr[edi+8 ], mm1
		movq qword ptr[edi+16], mm2
		movq qword ptr[edi+24], mm3
		add esi, 32
		add edi, 32
		loop swap_bytes

		sub edi, ssx_bytes2//-= screen-size_x*4*2
		pop ecx
		loop swap_line
		emms
	}
#else
	register int tmp;
	int* d0;
	int* d1;
	d0= (int*)screen_buffer;
	d1= (int*)screen_buffer + (screen_size_y-1)*screen_size_x;
	for( int y= 0; y< screen_size_y>>1; y++, d0+= screen_size_x, d1-= screen_size_x )
		for( int x= 0; x< screen_size_x; x+=4 )
		{
			tmp= d0[x+0]; d0[x+0]= d1[x+0]; d1[x+0]= tmp;
			tmp= d0[x+1]; d0[x+1]= d1[x+1]; d1[x+1]= tmp;
			tmp= d0[x+2]; d0[x+2]= d1[x+2]; d1[x+2]= tmp;
			tmp= d0[x+3]; d0[x+3]= d1[x+3]; d1[x+3]= tmp;
										
	}
#endif
}



extern "C" void PRast_ClearColorBuffer( const unsigned char* color )
{

	int* d= (int*)screen_buffer;
	int* d_end= (int*)( screen_buffer + screen_size_x * screen_size_y * 4 );
	
#ifdef PSR_MASM32
	unsigned char clear_value[]= { color[0], color[1], color[2], color[3], color[0], color[1], color[2], color[3] };
	__asm
	{
		mov ecx, d
		mov edx, d_end
		movq mm0, qword ptr[ clear_value ]
		movq mm1, mm0
		movq mm2, mm0
		movq mm3, mm0
		movq mm4, mm0
		movq mm5, mm0
		movq mm6, mm0
		movq mm7, mm0
next64bytes:
		movq qword ptr[ecx +0], mm0
		movq qword ptr[ecx +8], mm1
		movq qword ptr[ecx+16], mm2
		movq qword ptr[ecx+24], mm3
		movq qword ptr[ecx+32], mm4
		movq qword ptr[ecx+40], mm5
		movq qword ptr[ecx+48], mm6
		movq qword ptr[ecx+56], mm7
		add ecx, 64
		cmp ecx, edx 
		jne next64bytes
		emms
	}
#else
	register int clear_value= color[0] | (color[1]<<8) | (color[2]<<16) | (color[3]<<24);
	for( ; d!=d_end; d+=8 )
	{
		d[0]= clear_value;
		d[1]= clear_value;
		d[2]= clear_value;
		d[3]= clear_value;
		d[4]= clear_value;
		d[5]= clear_value;
		d[6]= clear_value;
		d[7]= clear_value;
	}
#endif
}


extern "C" void PRast_ClearDepthBuffer( depth_buffer_t value )
{
	//clear depth byffer per double word, not word
	unsigned int* d= (unsigned int*) depth_buffer;
	unsigned int* d_end= d + screen_size_x * screen_size_y * sizeof( depth_buffer_t )/sizeof(unsigned int);

	//fast clearing of depth buffer using mmx registers. or not fast. screen_size_x * screen_size_y must be dividable by 64
#ifdef PSR_MASM32
	depth_buffer_t clear_value[]= { value, value, value, value };
	__asm
	{
		mov ecx, d
		mov edx, d_end
		movq mm0, qword ptr[ clear_value ]
		movq mm1, mm0
		movq mm2, mm0
		movq mm3, mm0
		movq mm4, mm0
		movq mm5, mm0
		movq mm6, mm0
		movq mm7, mm0
next64bytes:
		movq qword ptr[ecx +0], mm0
		movq qword ptr[ecx +8], mm1
		movq qword ptr[ecx+16], mm2
		movq qword ptr[ecx+24], mm3
		movq qword ptr[ecx+32], mm4
		movq qword ptr[ecx+40], mm5
		movq qword ptr[ecx+48], mm6
		movq qword ptr[ecx+56], mm7
		add ecx, 64
		cmp ecx, edx 
		jne next64bytes
		emms

	}
#else
		register int clear_value= value | (value<<16);
		for( ; d!= d_end; d+=8 )
		{
			d[0]= clear_value;
			d[1]= clear_value;
			d[2]= clear_value;
			d[3]= clear_value;
			d[4]= clear_value;
			d[5]= clear_value;
			d[6]= clear_value;
			d[7]= clear_value;
		}
#endif


}

/*extern "C" void PRast_AlphaBlendColorBufferAndSwapRedBlueAlphaLost( const unsigned char* color )
{
	unsigned char* d = screen_buffer;
	unsigned char* d_end = screen_buffer + screen_size_x * screen_size_y * 4;
	unsigned char blend_factor = color[3];
	unsigned char inv_blend_factor= 255 - color[3];
	#ifdef PSR_MASM32
	__asm
	{
		movzx eax, blend_factor
		sub esp, 16
		mov word ptr[esp-6], ax
		mov word ptr[esp-4], ax
		mov word ptr[esp-2], ax
		mov word ptr[esp-0], ax
		not al
		mov word ptr[esp-14], ax
		mov word ptr[esp-12], ax
		mov word ptr[esp-10], ax
		mov word ptr[esp-8], ax

		//mm0 - dst color0 * inv_blend_factor
		mov edi, d
		mov esi, color
		movd mm3, dword ptr[esi]
		pxor mm1, mm1//zero register
		punpcklbw mm3, mm1// word size src color
		pmullw mm3, qword ptr[esp-6]// src_color*= blend_factor
		movq mm4, qword ptr[esp-14]

		mov esi, d_end
loop_point:
		movd mm0, dword ptr[edi]
		punpcklbw mm0, mm1// word size dst color
		pmullw mm0, mm4// dst_color*= inv_blend_factor
		paddw mm0, mm3//dst_color+= src color * blend_factor
		psrlw mm0, 8// /=256
		packuswb mm0, mm1//convert words to bytes. mm1 is zero
		movd eax, mm0
		bswap eax
		shr eax, 8
		mov dword ptr[edi], eax//write result

		movd mm0, dword ptr[edi+4]
		punpcklbw mm0, mm1
		pmullw mm0, mm4
		paddw mm0, mm3
		psrlw mm0, 8
		packuswb mm0, mm1
		movd eax, mm0
		bswap eax
		shr eax, 8
		mov dword ptr[edi+4], eax//write result

		movd mm0, dword ptr[edi+8]
		punpcklbw mm0, mm1
		pmullw mm0, mm4
		paddw mm0, mm3
		psrlw mm0, 8
		packuswb mm0, mm1
		movd eax, mm0
		bswap eax
		shr eax, 8
		mov dword ptr[edi+8], eax//write result

		movd mm0, dword ptr[edi+12]
		punpcklbw mm0, mm1
		pmullw mm0, mm4
		paddw mm0, mm3
		psrlw mm0, 8
		packuswb mm0, mm1
		movd eax, mm0
		bswap eax
		shr eax, 8
		mov dword ptr[edi+12], eax//write result

		add edi, 16
		cmp esi, edi
		jnz loop_point

		add esp, 16
		emms
	}
#else
	unsigned int premiltipled_color[]= { 
		 color[0] * blend_factor,
		 color[1] * blend_factor,
		 color[2] * blend_factor,
		 color[3] * blend_factor };
	for( ; d!= d_end; d+=16 )//cycle per 4 pixels
	{
		 d[0 ]= ( premiltipled_color[2] + d[2 ] * inv_blend_factor ) >> 8;
         d[1 ]= ( premiltipled_color[1] + d[1 ] * inv_blend_factor ) >> 8;
         d[2 ]= ( premiltipled_color[0] + d[0 ] * inv_blend_factor ) >> 8;
		 d[4 ]= ( premiltipled_color[2] + d[6 ] * inv_blend_factor ) >> 8;
         d[5 ]= ( premiltipled_color[1] + d[5 ] * inv_blend_factor ) >> 8;
         d[6 ]= ( premiltipled_color[0] + d[4 ] * inv_blend_factor ) >> 8;
		 d[8 ]= ( premiltipled_color[2] + d[10] * inv_blend_factor ) >> 8;
         d[9 ]= ( premiltipled_color[1] + d[9 ] * inv_blend_factor ) >> 8;
         d[10]= ( premiltipled_color[0] + d[8 ] * inv_blend_factor ) >> 8;
		 d[12]= ( premiltipled_color[2] + d[14] * inv_blend_factor ) >> 8;
         d[13]= ( premiltipled_color[1] + d[13] * inv_blend_factor ) >> 8;
         d[14]= ( premiltipled_color[0] + d[12] * inv_blend_factor ) >> 8;
	}
#endif
}*/

extern "C" void PRast_AlphaBlendColorBuffer( const unsigned char* color )
{
	unsigned char* d = screen_buffer;
	unsigned char* d_end = screen_buffer + screen_size_x * screen_size_y * 4;
	unsigned char blend_factor = color[3];
	unsigned char inv_blend_factor= 255 - color[3];
	#ifdef PSR_MASM32
	__asm
	{
		movzx eax, blend_factor
		sub esp, 16
		mov word ptr[esp-6], ax
		mov word ptr[esp-4], ax
		mov word ptr[esp-2], ax
		mov word ptr[esp-0], ax
		not al
		mov word ptr[esp-14], ax
		mov word ptr[esp-12], ax
		mov word ptr[esp-10], ax
		mov word ptr[esp-8], ax

		//mm0 - dst color0 * inv_blend_factor
		mov edi, d
		mov esi, color
		movd mm3, dword ptr[esi]
		pxor mm1, mm1//zero register
		punpcklbw mm3, mm1// word size src color
		pmullw mm3, qword ptr[esp-6]// src_color*= blend_factor
		movq mm4, qword ptr[esp-14]

		mov esi, d_end
loop_point:
		movd mm0, dword ptr[edi]
		punpcklbw mm0, mm1// word size dst color
		pmullw mm0, mm4// dst_color*= inv_blend_factor
		paddw mm0, mm3//dst_color+= src color * blend_factor
		psrlw mm0, 8// /=256
		packuswb mm0, mm1//convert words to bytes. mm1 is zero
		movd dword ptr[edi], mm0//write result

		movd mm0, dword ptr[edi+4]
		punpcklbw mm0, mm1
		pmullw mm0, mm4
		paddw mm0, mm3
		psrlw mm0, 8
		packuswb mm0, mm1
		movd dword ptr[edi+4], mm0

		movd mm0, dword ptr[edi+8]
		punpcklbw mm0, mm1
		pmullw mm0, mm4
		paddw mm0, mm3
		psrlw mm0, 8
		packuswb mm0, mm1
		movd dword ptr[edi+8], mm0

		movd mm0, dword ptr[edi+12]
		punpcklbw mm0, mm1
		pmullw mm0, mm4
		paddw mm0, mm3
		psrlw mm0, 8
		packuswb mm0, mm1
		movd dword ptr[edi+12], mm0

		add edi, 16
		cmp esi, edi
		jnz loop_point

		add esp, 16
		emms
	}
#else
	unsigned int premiltipled_color[]= { 
		 color[0] * blend_factor,
		 color[1] * blend_factor,
		 color[2] * blend_factor,
		 color[3] * blend_factor };
	for( ; d!= d_end; d+=16 )//cycle per 4 pixels
	{
		 d[0 ]= ( premiltipled_color[0] + d[0 ] * inv_blend_factor ) >> 8;
         d[1 ]= ( premiltipled_color[1] + d[1 ] * inv_blend_factor ) >> 8;
         d[2 ]= ( premiltipled_color[2] + d[2 ] * inv_blend_factor ) >> 8;
		 d[4 ]= ( premiltipled_color[0] + d[4 ] * inv_blend_factor ) >> 8;
         d[5 ]= ( premiltipled_color[1] + d[5 ] * inv_blend_factor ) >> 8;
         d[6 ]= ( premiltipled_color[2] + d[6 ] * inv_blend_factor ) >> 8;
		 d[8 ]= ( premiltipled_color[0] + d[8 ] * inv_blend_factor ) >> 8;
         d[9 ]= ( premiltipled_color[1] + d[9 ] * inv_blend_factor ) >> 8;
         d[10]= ( premiltipled_color[2] + d[10] * inv_blend_factor ) >> 8;
		 d[12]= ( premiltipled_color[0] + d[12] * inv_blend_factor ) >> 8;
         d[13]= ( premiltipled_color[1] + d[13] * inv_blend_factor ) >> 8;
         d[14]= ( premiltipled_color[2] + d[14] * inv_blend_factor ) >> 8;
	}
#endif
}


extern "C"  void PRast_SwapRedBlueLostAlphaInFramebuffer()
{
	int* pix= (int*)screen_buffer;
	int* pix_end= (int*)screen_buffer + screen_size_x * screen_size_y;
	#ifdef PSR_MASM32
	__asm
	{
		mov esi, pix
		mov edi, pix_end
do_something:
		mov eax, dword ptr[esi   ]
		mov ebx, dword ptr[esi+ 4]
		mov ecx, dword ptr[esi+ 8]
		mov edx, dword ptr[esi+12]
		bswap eax
		bswap ebx
		bswap ecx
		bswap edx
		;; make ROR command, if need save alpha
		shr eax, 8
		shr ebx, 8
		shr ecx, 8
		shr edx, 8
		mov dword ptr[esi   ], eax
		mov dword ptr[esi +4], ebx
		mov dword ptr[esi +8], ecx
		mov dword ptr[esi+12], edx

		add esi, 16
		cmp esi, edi
		jnz do_something
	}
#else
	for( ; pix!= pix_end; pix++ )
	{
		unsigned char c[4];
		*((int*)c)= *pix;
		c[3]= c[0];
		c[0]= c[2];
		c[2]= c[3];
		*((int*)pix)= *((int*)c);
	}
#endif
}


extern "C"  void PRast_SwapRedBlueInFramebuffer()
{
	unsigned char* pix= screen_buffer;
	unsigned char* pix_end= screen_buffer + screen_size_x*screen_size_y * 4;
	#ifdef PSR_MASM32
	__asm
	{
		mov esi, pix
		mov edi, pix_end

do_something:
		mov eax, dword ptr[esi   ]
		mov ebx, dword ptr[esi+ 4]
		mov ecx, dword ptr[esi+ 8]
		mov edx, dword ptr[esi+16]
		bswap eax
		bswap ebx
		bswap ecx
		bswap edx
		
		ror eax, 8
		ror ebx, 8
		ror ecx, 8
		ror edx, 8
		mov dword ptr[esi   ], eax
		mov dword ptr[esi+ 4], ebx
		mov dword ptr[esi+ 8], ecx
		mov dword ptr[esi+16], edx

		add esi, 16
		cmp esi, edi
		jnz do_something
	}
#else
	for( ; pix!= pix_end; pix++ )
	{
		unsigned char c[4];
		unsigned char tmp;
		*((int*)c)= *pix;
		tmp= c[0];
		c[0]= c[2];
		c[2]= tmp;
		*((int*)pix)= *((int*)c);
	}
#endif
}
