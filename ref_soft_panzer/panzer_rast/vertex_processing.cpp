
#include <math.h>
#include <stdlib.h>
#include "rendering_state.h"
#include "fixed.h"
#include "fast_math.h"
#include "psr.h"
#include "psr_mmx.h"
#include "rasterization.h"

namespace VertexProcessing
{



float width_f; //screen width * 65536
float height_f;
float width2_f; //screen width /2 * 65536
float height2_f;



/*universal vertex format ( with all attributes )
real vertex structures can has some of these
*/
struct Vertex
{
	float coord[3];
	char color[4];
	fixed16_t tex_coord[2];
	fixed16_t lightmap_tex_coord[2];
	unsigned short light;
	char normal[3];
};

//UNFINISHED
#if 0

int clip_vertices_array[ 32 * sizeof(Vertex)/sizeof(int) ];
int clip_nev_vertex_count;
//returns number of output triangles normal - TO screen center
template< int normal_x, int normal_y,
enum ColorMode color_mode,
enum TextureMode texture_mode,
enum LightingMode lighting_mode,
enum AdditionalEffectMode additional_effect_mode >
//input vertex - array of ints
int ClipTriangle( int point_x, int point_y,
	unsigned int** in_triangles, unsigned int** out_new_triangles,
	int in_triangle_count )
{
	 const int vertex_size= sizeof(int)*3 + ( (color_mode==COLOR_PER_VERTEX)? 4 : 0 ) +
                           ( (texture_mode==TEXTURE_NONE)? 0 : 2*sizeof(int) ) +
						   ( (lighting_mode==LIGHTING_FROM_LIGHTMAP)? 2*sizeof(int) : 0 ) +
                           ( (lighting_mode==LIGHTING_PER_VERTEX)? sizeof(int) : 0 );
	int dot[3];//dst from vertex to clip line
	for(int i= 0; i< 3; i++ )
		dot[i]=
			(in_triangles_indeces[i][0] - point_x) * normal_x +
			(in_triangles_indeces[i][1] - point_y) * normal_y;

	int vert_pos_bit_vector= int(dot[0]>0) | (int(dot[1]<0)<<1) | (int(dot[2]>0)<<2);

	if( vert_pos_bit_vector == (1+2+4) )
		return 1;
	if( vert_pos_bit_vector == 0 )
		return 0;
}
#endif

fixed16_t triangle_in_vertex_xy[ 2 * 3 ];//screen space x and y
fixed16_t triangle_in_vertex_z[3];//screen space z
PSR_ALIGN_4 unsigned char triangle_in_color[ 3 * 4 ];
int triangle_in_light[3];
fixed16_t triangle_in_tex_coord[ 2 * 3 ];
fixed16_t triangle_in_lightmap_tex_coord[ 2 * 3 ];



/*
this function bisect triangle, sort vertices in right order and put result to buffer
*/
template<
enum ColorMode color_mode,
enum TextureMode texture_mode,
enum LightingMode lighting_mode,
enum AdditionalEffectMode additional_effect_mode >
int DrawTriangleToBuffer( char* buff )//returns 0, if no output triangles
{
    const int vertex_size= sizeof(int)*3 + ( (color_mode==COLOR_PER_VERTEX)? 4 : 0 ) +
                           ( (texture_mode==TEXTURE_NONE)? 0 : 2*sizeof(int) ) +
						   ( (lighting_mode==LIGHTING_FROM_LIGHTMAP)? 2*sizeof(int) : 0 ) +
                           ( (lighting_mode==LIGHTING_PER_VERTEX)? sizeof(int) : 0 ) +
						   ( (lighting_mode==LIGHTING_PER_VERTEX_COLORED) ? 4 : 0 );


   // if( triangle_in_vertex_xy[1] == triangle_in_vertex_xy[3] && triangle_in_vertex_xy[1] == triangle_in_vertex_xy[5] )
    //    return 0;//nothing to draw, triangle is flat, works for far small triangles

    int vertex_indeces_from_upper[3]= { 0, 1, 2 };
    int vertex_y_from_upper[3]= { triangle_in_vertex_xy[1], triangle_in_vertex_xy[3], triangle_in_vertex_xy[5] };

    fixed16_t triangle_in_vertex_inv_z[]=
    {
        Fixed16Invert(triangle_in_vertex_z[0] ),
        Fixed16Invert(triangle_in_vertex_z[1] ),
        Fixed16Invert(triangle_in_vertex_z[2] )
    };

    //sort vertices from upper to lower, using bubble-sorting
	register int tmp;
    if( vertex_y_from_upper[0] < vertex_y_from_upper[1] )
    {
        tmp= vertex_y_from_upper[0];
        vertex_y_from_upper[0]= vertex_y_from_upper[1];
        vertex_y_from_upper[1]= tmp;
        tmp= vertex_indeces_from_upper[0];
        vertex_indeces_from_upper[0]= vertex_indeces_from_upper[1];
        vertex_indeces_from_upper[1]= tmp;
    }
    if( vertex_y_from_upper[0] < vertex_y_from_upper[2] )
    {
        tmp= vertex_y_from_upper[0];
        vertex_y_from_upper[0]= vertex_y_from_upper[2];
        vertex_y_from_upper[2]= tmp;
        tmp= vertex_indeces_from_upper[0];
        vertex_indeces_from_upper[0]= vertex_indeces_from_upper[2];
        vertex_indeces_from_upper[2]= tmp;
    }
    if( vertex_y_from_upper[1] < vertex_y_from_upper[2] )
    {
        tmp= vertex_y_from_upper[2];
        vertex_y_from_upper[2]= vertex_y_from_upper[1];
        vertex_y_from_upper[1]= tmp;
        tmp= vertex_indeces_from_upper[2];
        vertex_indeces_from_upper[2]= vertex_indeces_from_upper[1];
        vertex_indeces_from_upper[1]= tmp;
    }
    //end of sorting


	fixed16_t div= triangle_in_vertex_xy[ vertex_indeces_from_upper[0]*2 + 1 ] - triangle_in_vertex_xy[ vertex_indeces_from_upper[2]*2 + 1 ];
	if( div < 4 ) return 0;//triangle is so small
	fixed16_t k0= Fixed16Div( triangle_in_vertex_xy[ vertex_indeces_from_upper[0]*2 + 1 ] - triangle_in_vertex_xy[ vertex_indeces_from_upper[1]*2 + 1 ], div );
    fixed16_t k1= (1<<16) - k0;
	fixed16_t up_down_line_x=
		Fixed16Mul( triangle_in_vertex_xy[ vertex_indeces_from_upper[0]<<1 ], k1 ) +
		Fixed16Mul( triangle_in_vertex_xy[ vertex_indeces_from_upper[2]<<1 ], k0 );

    char* v= buff;
    //write lower vertex attributes
    ((int*)v)[0]= triangle_in_vertex_xy[ vertex_indeces_from_upper[2]<<1 ];
    ((int*)v)[1]= triangle_in_vertex_xy[ (vertex_indeces_from_upper[2]<<1)+1 ];
    ((int*)v)[2]= triangle_in_vertex_z[ vertex_indeces_from_upper[2] ];
    v+= 3 * sizeof(int);
    if( color_mode == COLOR_PER_VERTEX || lighting_mode == LIGHTING_PER_VERTEX_COLORED )
    {
        Byte4Copy( v, triangle_in_color + (vertex_indeces_from_upper[2]<<2) );
        v+=4;
    }
    if( texture_mode != TEXTURE_NONE )
    {
        ((int*)v)[0]= triangle_in_tex_coord[ vertex_indeces_from_upper[2]<<1 ];
        ((int*)v)[1]= triangle_in_tex_coord[ (vertex_indeces_from_upper[2]<<1)+1 ];
        v+=sizeof(fixed16_t)*2;
    }
    if( lighting_mode == LIGHTING_PER_VERTEX )
    {
        ((int*)v)[0]= triangle_in_light[ vertex_indeces_from_upper[2] ];
        v+= sizeof(fixed16_t);
    }
    if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
    {
        ((int*)v)[0]= triangle_in_lightmap_tex_coord[ vertex_indeces_from_upper[2]<<1 ];
        ((int*)v)[1]= triangle_in_lightmap_tex_coord[ (vertex_indeces_from_upper[2]<<1)+1 ];
        v+=sizeof(fixed16_t)*2;
    }

  //write middle vertices
    bool invert_vertex_order= triangle_in_vertex_xy[ vertex_indeces_from_upper[1]<<1 ] <= up_down_line_x;
    if( invert_vertex_order )
        v+= vertex_size;

    //write interpolated vertex
    fixed16_t final_z;
    ((int*)v)[0]= up_down_line_x;
    ((int*)v)[1]= triangle_in_vertex_xy[ (vertex_indeces_from_upper[1]<<1)+1 ];//y - from middle vertex
    ((int*)v)[2]= Fixed16Invert
                  ( Fixed16Mul( triangle_in_vertex_inv_z[ vertex_indeces_from_upper[0] ], k1 ) +
                    Fixed16Mul( triangle_in_vertex_inv_z[ vertex_indeces_from_upper[2] ], k0 ) );//interpolate inv_z

    final_z= ((int*)v)[2];
    v+= 3 * sizeof(int);
    if( color_mode == COLOR_PER_VERTEX || lighting_mode == LIGHTING_PER_VERTEX_COLORED )
    {
		fixed16_t inv_z0= triangle_in_vertex_inv_z[ vertex_indeces_from_upper[0] ];
        fixed16_t inv_z2= triangle_in_vertex_inv_z[ vertex_indeces_from_upper[2] ];
        for( int i= 0; i< 4; i++ )
        {
            //convert in color to fixed16_t format and divede by z
            fixed16_t div_c0= triangle_in_color[ i + (vertex_indeces_from_upper[0]<<2) ] * inv_z0;
            fixed16_t div_c2= triangle_in_color[ i + (vertex_indeces_from_upper[2]<<2) ] * inv_z2;
            ((unsigned char*)v)[i]= Fixed16MulResultToInt( ( Fixed16Mul( div_c0, k1 ) + Fixed16Mul( div_c2, k0 ) ), final_z );//make interpolation and write result
        }
        v+=4;
    }
    if( texture_mode != TEXTURE_NONE )
    {

        fixed16_t inv_z0= triangle_in_vertex_inv_z[ vertex_indeces_from_upper[0] ];
        fixed16_t inv_z2= triangle_in_vertex_inv_z[ vertex_indeces_from_upper[2] ];
        fixed16_t div_tc0= Fixed16Mul( triangle_in_tex_coord[ (vertex_indeces_from_upper[0]<<1) ], inv_z0 );
        fixed16_t div_tc2= Fixed16Mul( triangle_in_tex_coord[ (vertex_indeces_from_upper[2]<<1) ], inv_z2 );
        ((int*)v)[0]= Fixed16Mul( Fixed16Mul( div_tc0, k1 ) + Fixed16Mul( div_tc2, k0 ), final_z );
        div_tc0= Fixed16Mul( triangle_in_tex_coord[ 1+(vertex_indeces_from_upper[0]<<1) ], inv_z0 );
        div_tc2= Fixed16Mul( triangle_in_tex_coord[ 1+(vertex_indeces_from_upper[2]<<1) ], inv_z2 );
        ((int*)v)[1]= Fixed16Mul( Fixed16Mul( div_tc0, k1 ) + Fixed16Mul( div_tc2, k0 ), final_z );
        v+=2*sizeof(int);
    }
    if( lighting_mode == LIGHTING_PER_VERTEX )
    {
        fixed16_t div_l0= triangle_in_light[ vertex_indeces_from_upper[0] ] * triangle_in_vertex_inv_z[ vertex_indeces_from_upper[0] ];
        fixed16_t div_l2= triangle_in_light[ vertex_indeces_from_upper[2] ] * triangle_in_vertex_inv_z[ vertex_indeces_from_upper[2] ];
        ((int*)v)[0]= Fixed16MulResultToInt( Fixed16Mul( div_l0, k1 ) + Fixed16Mul( div_l2, k0 ), final_z );
        v+=sizeof(int);
    }
    if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
    {
        fixed16_t inv_z0= triangle_in_vertex_inv_z[ vertex_indeces_from_upper[0] ];
        fixed16_t inv_z2= triangle_in_vertex_inv_z[ vertex_indeces_from_upper[2] ];
        fixed16_t div_tc0= Fixed16Mul( triangle_in_lightmap_tex_coord[ (vertex_indeces_from_upper[0]<<1) ], inv_z0 );
        fixed16_t div_tc2= Fixed16Mul( triangle_in_lightmap_tex_coord[ (vertex_indeces_from_upper[2]<<1) ], inv_z2 );
        ((int*)v)[0]= Fixed16Mul( Fixed16Mul( div_tc0, k1 ) + Fixed16Mul( div_tc2, k0 ), final_z );
        div_tc0= Fixed16Mul( triangle_in_lightmap_tex_coord[ 1+(vertex_indeces_from_upper[0]<<1) ], inv_z0 );
        div_tc2= Fixed16Mul( triangle_in_lightmap_tex_coord[ 1+(vertex_indeces_from_upper[2]<<1) ], inv_z2 );
        ((int*)v)[1]= Fixed16Mul( Fixed16Mul( div_tc0, k1 ) + Fixed16Mul( div_tc2, k0 ), final_z );
        v+=2*sizeof(int);
    }

    if( invert_vertex_order )
        v-= 2*vertex_size;

    //write middle vertex
    ((int*)v)[0]= triangle_in_vertex_xy[ vertex_indeces_from_upper[1]<<1 ];
    ((int*)v)[1]= triangle_in_vertex_xy[ (vertex_indeces_from_upper[1]<<1)+1 ];
    ((int*)v)[2]= triangle_in_vertex_z[ vertex_indeces_from_upper[1] ];
    v+= 3 * sizeof(int);
    if( color_mode == COLOR_PER_VERTEX || lighting_mode == LIGHTING_PER_VERTEX_COLORED )
    {
        Byte4Copy( v, triangle_in_color + (vertex_indeces_from_upper[1]<<2) );
        v+=4;
    }
    if( texture_mode != TEXTURE_NONE )
    {
        ((int*)v)[0]= triangle_in_tex_coord[ vertex_indeces_from_upper[1]<<1 ];
        ((int*)v)[1]= triangle_in_tex_coord[ (vertex_indeces_from_upper[1]<<1)+1 ];
        v+=sizeof(fixed16_t)*2;
    }
    if( lighting_mode == LIGHTING_PER_VERTEX )
    {
        ((int*)v)[0]= triangle_in_light[ vertex_indeces_from_upper[1] ];
        v+= sizeof(fixed16_t);
    }
    if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
    {
        ((int*)v)[0]= triangle_in_lightmap_tex_coord[ vertex_indeces_from_upper[1]<<1 ];
        ((int*)v)[1]= triangle_in_lightmap_tex_coord[ (vertex_indeces_from_upper[1]<<1)+1  ];
        v+=sizeof(fixed16_t)*2;
    }
    if( invert_vertex_order )
        v+= vertex_size;


    //write upper vertex attributes
    ((int*)v)[0]= triangle_in_vertex_xy[ vertex_indeces_from_upper[0]<<1 ];
    ((int*)v)[1]= triangle_in_vertex_xy[ (vertex_indeces_from_upper[0]<<1)+1 ];
    ((int*)v)[2]= triangle_in_vertex_z[ vertex_indeces_from_upper[0] ];
    v+= 3 * sizeof(int);
    if( color_mode == COLOR_PER_VERTEX || lighting_mode == LIGHTING_PER_VERTEX_COLORED )
    {
        Byte4Copy( v, triangle_in_color + (vertex_indeces_from_upper[0]<<2) );
        v+=4;
    }
    if( texture_mode != TEXTURE_NONE )
    {
        ((int*)v)[0]= triangle_in_tex_coord[ vertex_indeces_from_upper[0]<<1 ];
        ((int*)v)[1]= triangle_in_tex_coord[ (vertex_indeces_from_upper[0]<<1)+1 ];
        v+=sizeof(fixed16_t)*2;
    }
    if( lighting_mode == LIGHTING_PER_VERTEX )
    {
        ((int*)v)[0]= triangle_in_light[ vertex_indeces_from_upper[0] ];
        v+= sizeof(fixed16_t);
    }
    if( lighting_mode == LIGHTING_FROM_LIGHTMAP )
    {
        ((int*)v)[0]= triangle_in_lightmap_tex_coord[ vertex_indeces_from_upper[0]<<1 ];
        ((int*)v)[1]= triangle_in_lightmap_tex_coord[ (vertex_indeces_from_upper[0]<<1)+1  ];
        v+=sizeof(fixed16_t)*2;
    }

	return 4;
}



int cull_passed_vertices[2];
int cull_lost_vertices[2];
int cull_new_vertices_neighbors[4];
float cull_new_vertices_interpolation_k[2];
//returns number of  culled vertices
int CullTriangleByZNearPlane( float z0, float z1, float z2 )
{
	bool  plane_pos[]= { z0 > PSR_MIN_ZMIN_FLOAT, z1 > PSR_MIN_ZMIN_FLOAT, z2 > PSR_MIN_ZMIN_FLOAT };
	if( plane_pos[0] & plane_pos[1] & plane_pos[2] )
		return 0;
	if( !( plane_pos[0] | plane_pos[1] | plane_pos[2] ) )
		return 3;

	int front_vertex_count= int(plane_pos[0]) + int(plane_pos[1]) + int(plane_pos[2]);

	if( front_vertex_count == 1 )
	{
		if( plane_pos[0] )
		{
			cull_passed_vertices[0]= 0;
			cull_lost_vertices[0]= 1;
			cull_lost_vertices[1]= 2;
			float edge_z_len= z0- z1;
			float forward_vertex_z_dst= z0 - PSR_MIN_ZMIN_FLOAT;
			cull_new_vertices_interpolation_k[0]= 1.0f - forward_vertex_z_dst / edge_z_len;
			edge_z_len= z0- z2;
			cull_new_vertices_interpolation_k[1]= 1.0f - forward_vertex_z_dst / edge_z_len;
		}
		else if( plane_pos[1] )
		{
			cull_passed_vertices[0]= 1;
			cull_lost_vertices[0]= 0;
			cull_lost_vertices[1]= 2;
			float edge_z_len= z1- z0;
			float forward_vertex_z_dst= z1 - PSR_MIN_ZMIN_FLOAT;
			cull_new_vertices_interpolation_k[0]= 1.0f - forward_vertex_z_dst / edge_z_len;
			edge_z_len= z1- z2;
			cull_new_vertices_interpolation_k[1]= 1.0f - forward_vertex_z_dst / edge_z_len;
		}
		else// if( plane_pos[2] )
		{
			cull_passed_vertices[0]= 2;
			cull_lost_vertices[0]= 0;
			cull_lost_vertices[1]= 1;
			float edge_z_len= z2- z0;
			float forward_vertex_z_dst= z2 - PSR_MIN_ZMIN_FLOAT;
			cull_new_vertices_interpolation_k[0]= 1.0f - forward_vertex_z_dst / edge_z_len;
			edge_z_len= z2- z1;
			cull_new_vertices_interpolation_k[1]= 1.0f - forward_vertex_z_dst / edge_z_len;
		}
		return 2;
	}
	else//if 2 forward vertices
	{
		if( !plane_pos[0] )
		{
			cull_passed_vertices[0]= 1;
			cull_passed_vertices[1]= 2;
			cull_lost_vertices[0]= 0;
			float edge_z_len= z0- z1;
			float forward_vertex_z_dst= z0 - PSR_MIN_ZMIN_FLOAT;
			cull_new_vertices_interpolation_k[0]= forward_vertex_z_dst / edge_z_len;
			edge_z_len= z0- z2;
			cull_new_vertices_interpolation_k[1]= forward_vertex_z_dst / edge_z_len;
		}
		else if( !plane_pos[1] )
		{
			cull_passed_vertices[0]= 0;
			cull_passed_vertices[1]= 2;
			cull_lost_vertices[0]= 1;
			float edge_z_len= z1- z0;
			float forward_vertex_z_dst= z1 - PSR_MIN_ZMIN_FLOAT;
			cull_new_vertices_interpolation_k[0]= forward_vertex_z_dst / edge_z_len;
			edge_z_len= z1- z2;
			cull_new_vertices_interpolation_k[1]= forward_vertex_z_dst / edge_z_len;
		}
		else// if( !plane_pos[2] )
		{
			cull_passed_vertices[0]= 0;
			cull_passed_vertices[1]= 1;
			cull_lost_vertices[0]= 2;
			float edge_z_len= z2- z0;
			float forward_vertex_z_dst= z2 - PSR_MIN_ZMIN_FLOAT;
			cull_new_vertices_interpolation_k[0]= forward_vertex_z_dst / edge_z_len;
			edge_z_len= z2- z1;
			cull_new_vertices_interpolation_k[1]= forward_vertex_z_dst / edge_z_len;
		}
		return 1;
	}
}


//member order in vertex attrib:

struct WorldVertexAttrib
{
	fixed16_t tc[4];
};


int DrawClipWorldTriangleToBuffer(
const float* v,//9 floats
WorldVertexAttrib* attribs )//3 * int_attribs
{

	float tmp_v[3*4];
	WorldVertexAttrib tmp_attrib[4];
	int vertices= CullTriangleByZNearPlane( v[2], v[5], v[8] );
	if( vertices == 0 )
		return 0;
	else if( vertices == 3 )
	{
	}
	else if( vertices == 2 )
	{
		float inv_interpolation_k= 1.0f - cull_new_vertices_interpolation_k[0];

		float interp;
		const float inv_zmin= 1.0f / PSR_MIN_ZMIN_FLOAT;
		interp= v[ cull_passed_vertices[0]*3 ] * cull_new_vertices_interpolation_k[0] +
				v[ cull_lost_vertices[0]*3 ] * inv_interpolation_k;
		tmp_v[0]=  ( inv_zmin * interp + 1.0f ) * width2_f;//interpolate x0
		interp= v[ cull_passed_vertices[0]*3+1 ] * cull_new_vertices_interpolation_k[0] +
				v[ cull_lost_vertices[0]*3+1 ] * inv_interpolation_k;
		tmp_v[1]=  ( inv_zmin * interp + 1.0f ) * height2_f;//interpolate y0

		interp= v[ cull_passed_vertices[0]*3 ] * cull_new_vertices_interpolation_k[0] +
				v[ cull_lost_vertices[1]*3 ] * inv_interpolation_k;
		tmp_v[3]=  ( inv_zmin * interp + 1.0f ) * width2_f;//interpolate x1
		interp= v[ cull_passed_vertices[0]*3+1 ] * cull_new_vertices_interpolation_k[0] +
				v[ cull_lost_vertices[1]*3+1 ] * inv_interpolation_k;
		tmp_v[4]=  ( inv_zmin * interp + 1.0f ) * height2_f;//interpolate y1

		tmp_v[2]= tmp_v[5]= PSR_MIN_ZMIN_FLOAT;

		int unmodifed_vertex_k= cull_passed_vertices[0]*3;
		float inv_interp_z= 1.0f / v [unmodifed_vertex_k+2];
		tmp_v[6]= (v[unmodifed_vertex_k*3] * inv_interp_z +1.0f) * width2_f;
		tmp_v[7]= (v[unmodifed_vertex_k+1] * inv_interp_z +1.0f) * height2_f;
		tmp_v[8]=  v[unmodifed_vertex_k+2];

		for( int i= 0; i< 4; i++ )
		{
			interp= float(attribs[ cull_passed_vertices[0] ].tc[i]) * cull_new_vertices_interpolation_k[0] +
					float(attribs[ cull_lost_vertices[0] ].tc[i]) * inv_interpolation_k;
			tmp_attrib[0].tc[i]= fixed16_t(interp);
			interp= float(attribs[ cull_passed_vertices[0] ].tc[i]) * cull_new_vertices_interpolation_k[0] +
					float(attribs[ cull_lost_vertices[1] ].tc[i]) * inv_interpolation_k;
			tmp_attrib[1].tc[i]= fixed16_t(interp);

			tmp_attrib[2].tc[i]= attribs[cull_passed_vertices[0]].tc[i];
		}

	}//if culled 2 vertices by nearZ plane
	else//if(vertices == 1 )
	{
	}

	return 3;
}


void DrawSpriteToBuffer( char* buff, int x0, int y0, int x1, int y1, fixed16_t depth )
{
	int* b= (int*)buff;
	b[0]= x0;
	b[1]= y0;
	b[2]= x1;
	b[3]= y1;
	b[4]= depth;
}

}//namespace VertexProcessing







/*
---------------OUT FUNCTIONS----------
*/

int (*DrawWorldTriangleToBuffer)(char* buff)= VertexProcessing::DrawTriangleToBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, LIGHTING_FROM_LIGHTMAP, ADDITIONAL_EFFECT_NONE >;
int (*DrawWorldTriangleNoLightmapToBuffer)(char* buff)= VertexProcessing::DrawTriangleToBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, LIGHTING_NONE, ADDITIONAL_EFFECT_NONE >;

int (*DrawWorldCachedTriangleToBuffer)(char* buff)= VertexProcessing::DrawTriangleToBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, LIGHTING_NONE, ADDITIONAL_EFFECT_NONE >;


int (*DrawBeamTriangleToBuffer)( char* buff )= VertexProcessing::DrawTriangleToBuffer
< COLOR_CONSTANT, TEXTURE_NONE, LIGHTING_NONE, ADDITIONAL_EFFECT_NONE >;

int (*DrawSkyTriangleToBuffer)( char* buff )= VertexProcessing::DrawTriangleToBuffer
<COLOR_FROM_TEXTURE, TEXTURE_NEAREST, LIGHTING_NONE, ADDITIONAL_EFFECT_NONE>;

int (*DrawTexturedModelTriangleToBuffer)(char*buff)= VertexProcessing::DrawTriangleToBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, LIGHTING_PER_VERTEX_COLORED, ADDITIONAL_EFFECT_NONE >;
int (*DrawFullbrightTexturedModelTriangleToBuffer)(char*buff)= VertexProcessing::DrawTriangleToBuffer
< COLOR_FROM_TEXTURE, TEXTURE_NEAREST, LIGHTING_NONE, ADDITIONAL_EFFECT_NONE >;


void (*DrawParticleSpriteToBuffer)( char* buff, int x0, int y0, int x1, int y1, fixed16_t depth )= VertexProcessing::DrawSpriteToBuffer;
