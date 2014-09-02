#ifndef PSR_H
#define PSR_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef DEFINE_FIXED16_T
typedef int fixed16_t;
#define DEFINE_FIXED16_T
#endif

typedef unsigned short depth_buffer_t;


#define PSR_PI_6	0.52359877559829887307710723054658f
#define PSR_PI_4	0.78539816339744830961566084581988f
#define PSR_PI_3	1.0471975511965977461542144610932f
#define PSR_PI_2	1.5707963267948966192313216916398f
#define PSR_PI		3.1415926535897932384626433832795f
#define PSR_2PI		6.283185307179586476925286766559f
#define PSR_RAD2GRAD 	57.295779513082320876798154814105f
#define PSR_GRAD2RAD 	0.017453292519943295769236907684886f

#define PSR_PI_6_FIXED16	34314L
#define PSR_PI_4_FIXED16	51471L
#define PSR_PI_3_FIXED16 	68629L
#define PSR_PI_2_FIXED16 	102943L
#define PSR_PI_FIXED16 		205887L
#define PSR_2PI_FIXED16 	411774L
#define PSR_RAD2GRAD_FIXED16	3754936L
#define PSR_GRAD2RAD_FIXED16	1143L

#define PSR_MASM32 1
//#define PSR_MASM32_INRINSINCS 1
//#define PSR_MASM64 1
//#define PSR_USE_MMX

#define PSR_MAX_SCREEN_WIDTH 4096
#define PSR_MAX_SCREEN_HEIGHT 4096

#define PSR_LINE_SEGMENT_SIZE 8
#define PSR_LINE_SEGMENT_SIZE_LOG2 3

#define PSR_MAX_TEXTURE_SIZE 2048
#define PSR_MAX_TEXTURE_SIZE_LOG2 11

#define PSR_MAX_NORMALIZED_LIGHT 127// color= ( normalized_light * 256 * color )/256

#define PSR_MIN_ZMIN_FLOAT 0.125f
#define PSR_MIN_ZMIN (65536/8)// minimal value of zmin in fixed16_t format
#define PSR_INV_MIN_ZMIN_INT 8
#define PSR_INV_MIN_ZMIN_INT_LOG2 3

#define PSR_MAX_ZMAX (1024)// maximal value of zmax in INT format

//if defined, z calculates every PSR_LINE_SEGMENT_SIZE pixel in scanline
//#define PSR_FAST_PERSECTIVE

#define PSR_MAX_DEPTH_BUFFER_VALUE 65536//max depth buffer value + 1

//coefficient for convertion of linear z from fixed16_t format to depth buffer format
#define PSR_DEPTH_SCALER  128//( PSR_MAX_DEPTH_BUFFER_VALUE * PSR_MAX_ZMAX / 65536 )
#define PSR_DEPTH_SCALER_LOG2 7
#define PSR_DEPTH_HACK_SCALER 1

//scale delta of line variables in fragment processing, becouse without it, deltas can be very small
#define PSR_INV_DEPTH_DELTA_MULTIPLER 64
#define PSR_INV_DEPTH_DELTA_MULTIPLER_LOG2 6
#define PSR_COLOR_DELTA_MULTIPLER_LOG2 4



#define PSR_MIN_COMMAND_BUFFER_SIZE 65536
#define PSR_MAX_COMMAND_BUFFER_SIZE (1024*16)// 16 mb - really big amount of vertices - hundreds of thousands


#define PSR_FRAMEBUFFER_ALIGNMENT 64
//compiler-dependent alignment derectives:
//for visual c++
#define PSR_ALIGN_4 __declspec(align(4))
#define PSR_ALIGN_8 __declspec(align(8))
#define PSR_ALIGN_16 __declspec(align(16))
//TODO for gcc and other compilers



#endif//PSR_H
