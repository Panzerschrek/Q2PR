#include "r_light.h"
#include "panzer_rast/math_lib/matrix.h"

typedef struct lightmap_buffer_s
{
	unsigned char* buffer;
	int size;
	int current_pos;
}lightmap_buffer_t;


static lightmap_buffer_t lightmap_buffer;
static lightmap_buffer_t back_lightmap_buffer;
int	r_dlightframecount;

static dlight_t transformed_dlights[MAX_DLIGHTS];//transformed lights( saturation correction )

#define D_LIGHTMAP_LINEAR 0
#define D_LIGHTMAP_LINEAR_RGBS 1
#define D_LIGHTMAP_LINEAR_COLORED 2

//#define LIGHT_SCALER 0.25f
#define LIGHT_SCALER 0.125f
#define LIGHTMAP_SCALER 0.0625f// 1/16 lightmap per pixel

static int lightmap_type;
static bool is_colored_lightmap;

void TransformLights( m_Mat4* mat, int light_count )
{
	for( int i= 0; i< light_count; i++ )
	{
		*((m_Vec3*)transformed_dlights[i].origin)= *((m_Vec3*)transformed_dlights[i].origin) * *mat;
	}
}


float sign( float x )
{
	if( x > 0.0f )
		return 1.0f;
	else if ( x < 0.0f )
		return -1.0f;
	return 0.0f;
}
void ColorCorrectLights( int num_lights )
{
	float sat= r_dlights_saturation->value;
	float inv_sat= 1.0f - sat;
	dlight_t* l;
	int i;

	if( strcmp( r_lightmap_mode->string, "lightmap_linear_colored" ) == 0 )
	{
		is_colored_lightmap= true;
		lightmap_type= D_LIGHTMAP_LINEAR_COLORED;
	}
	/*else if( strcmp( r_lightmap_mode->string, "lightmap_linear_rgbs" ) == 0 )
	{
		is_colored_lightmap= true;
		lightmap_type= D_LIGHTMAP_LINEAR_RGBS;
	}*/
	else
	{
		is_colored_lightmap= false;
		lightmap_type= D_LIGHTMAP_LINEAR;
	}

	for( i= 0, l= transformed_dlights; i< num_lights; i++, l++ )
	{
		if( lightmap_type != D_LIGHTMAP_LINEAR )
		{
			float s= ( l->color[0] + l->color[1] + l->color[2] ) * 0.333333f;
			s*= inv_sat;
			float tmp_color[3]={
				l->color[0] * sat + s,
				l->color[1] * sat + s,
				l->color[2] * sat + s };
			l->color[0]= tmp_color[0];
			l->color[1]= tmp_color[1];
			l->color[2]= tmp_color[2];

			ColorFloatSwap( l->color );
		}
		else
			l->intensity*= ( l->color[0] + l->color[1] + l->color[2] ) * 0.3333333333f;
	}//for lights
}


void R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	if (node->contents != -1)
		return;

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;

//=====
//PGM
	i=light->intensity;
	if(i<0)
		i=-i;
//PGM
//=====

	float min_intensity= 2.0f;
	float max_dst=  sqrtf( fabs( ( LIGHT_SCALER * 256.0f / LIGHTMAP_SCALER ) * light->intensity ) / min_intensity );

	/*if (dist > i)	// PGM (dist > light->intensity)
	{
		R_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -i)	// PGM (dist < -light->intensity)
	{
		R_MarkLights (light, bit, node->children[1]);
		return;
	}*/
	if (dist > max_dst)	// PGM (dist > light->intensity)
	{
		R_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -max_dst)	// PGM (dist < -light->intensity)
	{
		R_MarkLights (light, bit, node->children[1]);
		return;
	}

// mark the polygons
	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	R_MarkLights (light, bit, node->children[0]);
	R_MarkLights (light, bit, node->children[1]);
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (model_t *model, const float* lights_transform_mat )
{
	int		i;
	dlight_t	*l;

	int real_lights= r_newrefdef.num_dlights;
	if( real_lights > MAX_DLIGHTS ) real_lights= MAX_DLIGHTS;

	for( i= 0; i< real_lights; i++ )
		transformed_dlights[i]= r_newrefdef.dlights[i];

	ColorCorrectLights(real_lights);
	TransformLights( (m_Mat4*)lights_transform_mat, real_lights );


	r_dlightframecount = r_framecount;
	for (i=0, l = transformed_dlights ; i<r_newrefdef.num_dlights && i<MAX_DLIGHTS ; i++, l++)
	{
		R_MarkLights ( l, 1<<i,
			model->nodes + model->firstnode);
	}

}



void R_LightInit()
{
	const int buff_size= 2048*2048*4;

	lightmap_buffer.size= buff_size;
	lightmap_buffer.buffer= new unsigned char[buff_size];
	lightmap_buffer.current_pos= 0;
	memset(lightmap_buffer.buffer, 255, lightmap_buffer.size );

	back_lightmap_buffer.size= buff_size;
	back_lightmap_buffer.buffer= new unsigned char[buff_size];
	back_lightmap_buffer.current_pos= 0;
}



void GenerateLightmapColored( unsigned char* out, msurface_t* surf )
{
	int size_x, size_y;
	size_x= (surf->extents[0]>>4) + 1;
	size_y= (surf->extents[1]>>4) + 1;
	int ds= 4 * size_x * size_y;
	if( surf->styles[1] == 255 )
	{
		memcpy( out, surf->samples, ds );
		//for( int i= 0; i< ds; i+=4 )//make fullbright lightmap for tests
		//	out[i]= out[i+1]= out[i+2]= 255;
		return;
	}
	int samp= 0;
	for( int i= 0; i< ds; i+=4 )
			out[i]= out[i+1]= out[i+2]= 0;
	while( samp < MAXLIGHTMAPS && surf->styles[samp] != 255 )
	{
		int scale_values[3]= {
			int(r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[0] * 256.0f),
			int(r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[1] * 256.0f),
			int(r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[2] * 256.0f) };
		//stupid gamex86.dll can send negative values
		if( scale_values[0] < 0 ) scale_values[0]= 0;
		if( scale_values[1] < 0 ) scale_values[1]= 0;
		if( scale_values[2] < 0 ) scale_values[2]= 0;
		//ColorIntSwap( scale_values );
		unsigned char* in= surf->samples + ds*samp;
		for( int i= 0; i< ds; i+=4 )
		{
			int c;
			c= ((in[i  ] * scale_values[0])>>8) + out[i  ];
			if( c > 255 ) c= 255; out[i  ]= c;
			c= ((in[i+1] * scale_values[1])>>8) + out[i+1];
			if( c > 255 ) c= 255; out[i+1]= c;
			c= ((in[i+2] * scale_values[2])>>8) + out[i+2];
			if( c > 255 ) c= 255; out[i+2]= c;
		}
		samp++;
	}
}
void GenerateLightmapLinear( unsigned char* out, msurface_t* surf )
{
	int size_x, size_y;
	size_x= (surf->extents[0]>>4) + 1;
	size_y= (surf->extents[1]>>4) + 1;
	int ds= size_x * size_y;
	if( surf->styles[1] == 255 )
	{
		memcpy( out, surf->samples, ds );
		return;
	}

	int samp= 0;
	for( int i= 0; i< ds; i++ )
			out[i]= 0;
	while( samp < MAXLIGHTMAPS && surf->styles[samp] != 255 )
	{
		int scale_value=int( (
			r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[0] +
			r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[1] +
			r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[2]) * (256.0*0.3333f) );
		//stupid gamex86.dll can send negative values
		if( scale_value < 0 ) scale_value= 0;

		unsigned char* in= surf->samples + ds*samp;
		for( int i= 0; i< ds; i++ )
		{
			int c;
			c= ((in[i] * scale_value)>>8) + out[i];
			if( c > 255 ) c= 255; out[i]= c;
		}
		samp++;
	}
}

//convert rgbs to colored
/*void GenerateLightmapRGBStoColored( unsigned char* out, msurface_t* surf )
{
	int size_x, size_y;
	size_x= (surf->extents[0]>>4) + 1;
	size_y= (surf->extents[1]>>4) + 1;
	int ds= 4 * size_x * size_y;
	if( surf->styles[1] == 255 )
	{
		memcpy( out, surf->samples, ds );
		for( int i=0; i< ds; i+=4 )
		{
			int s= out[i+3];
			out[i  ]= ( out[i  ] * s )/255;
			out[i+1]= ( out[i+1] * s )/255;
			out[i+2]= ( out[i+2] * s )/255;
		}return;
	}
	int samp= 0;
	for( int i= 0; i< ds; i+=4 )
			out[i]= out[i+1]= out[i+2]= 0;
	while( samp < MAXLIGHTMAPS && surf->styles[samp] != 255 )
	{
		int scale_values[3]= {
			int(r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[0] * 256.0f),
			int(r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[1] * 256.0f),
			int(r_newrefdef.lightstyles[ surf->styles[samp] ].rgb[2] * 256.0f) };
		//stupid gamex86.dll can send negative values
		if( scale_values[0] < 0 ) scale_values[0]= 0;
		if( scale_values[1] < 0 ) scale_values[1]= 0;
		if( scale_values[2] < 0 ) scale_values[2]= 0;

		ColorIntSwap( scale_values );
		unsigned char* in= surf->samples + ds*samp;
		for( int i= 0; i< ds; i+=4 )
		{
			int c;
			c= (( in[i+3] * in[i  ] * scale_values[0])/(255*256)) + out[i  ];
			if( c > 255 ) c= 255; out[i  ]= c;
			c= ((in[i+3] * in[i+1] * scale_values[1])/(255*256)) + out[i+1];
			if( c > 255 ) c= 255; out[i+1]= c;
			c= ((in[i+3] * in[i+2] * scale_values[2])/(255*256)) + out[i+2];
			if( c > 255 ) c= 255; out[i+2]= c;
		}
		samp++;
	}
}
*/
//convert colored to rgbs
/*
void ConverLightmapColored2RGBS( unsigned char* in_out, int size)
{
	for( int i=0; i< size*4; i+=4 )
	{
		int max= in_out[i+0];
		if( in_out[i+1] > max )
			max= in_out[i+1];
		if( in_out[i+2] > max )
			max= in_out[i+2];
		if( max == 0 )
			max= 1;
		in_out[i+0]= in_out[i+0]*255/max;
		in_out[i+1]= in_out[i+1]*255/max;
		in_out[i+2]= in_out[i+2]*255/max;
		in_out[i+3]= max;
	}
}
*/

void R_SwapLightmapBuffers()
{
	lightmap_buffer_t tmp= lightmap_buffer;
	lightmap_buffer= back_lightmap_buffer;
	back_lightmap_buffer= tmp;

	lightmap_buffer.current_pos= 0;
}

unsigned char* L_GetSurfaceDynamicLightmap( msurface_t* surf )
{
	const float max_light= 512.0f;
	int size_x, size_y;
	size_x= (surf->extents[0]>>4) + 1;
	size_y= (surf->extents[1]>>4) + 1;


	if( surf->dlightframe == r_dlightframecount )
	{
		unsigned char* lightmap= lightmap_buffer.buffer + lightmap_buffer.current_pos;
		if( lightmap_type == D_LIGHTMAP_LINEAR_COLORED )
			GenerateLightmapColored( lightmap, surf );
		//else if( lightmap_type == D_LIGHTMAP_LINEAR_RGBS )
		//	GenerateLightmapRGBStoColored( lightmap, surf );
		else
			GenerateLightmapLinear( lightmap, surf );

		float surf_scale[2]={
			1.0f / sqrtf( DotProduct( surf->texinfo->vecs[0], surf->texinfo->vecs[0] ) ),
			1.0f / sqrtf( DotProduct( surf->texinfo->vecs[1], surf->texinfo->vecs[1] ) ) };
		for( int i= 0; i< r_newrefdef.num_dlights; i++ )
		{
			if( (surf->dlightbits&(1<<i)) == 0 )
				continue;

			mplane_t* plane= surf->plane;
			dlight_t* light= transformed_dlights + i;
			float dst_to_plane= DotProduct (light->origin, plane->normal) - plane->dist;
			if( (surf->flags&SURF_PLANEBACK) != 0 )
				dst_to_plane= -dst_to_plane;
			if(dst_to_plane< -0.01f)
				continue;
			dst_to_plane+= 1.0f;//hack, for lights on plane ( like blaster decals )

			float transformed_pos[3];//light position in plane space
			transformed_pos[0]= 0.0625f * ( DotProduct( light->origin, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0] );
			transformed_pos[1]= 0.0625f * ( DotProduct( light->origin, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1] );
			transformed_pos[2]= 0.0625f * dst_to_plane;

			for( int y= 0; y< size_y; y++ )
				for( int x= 0; x< size_x; x++ )
				{
					float texel_pos[]= {
						surf_scale[0] * ( float(x) - transformed_pos[0] ),
						surf_scale[1] * ( float(y) - transformed_pos[1] ) };
					float dst2= (texel_pos[0]*texel_pos[0] + texel_pos[1]*texel_pos[1] + transformed_pos[2]*transformed_pos[2]);
					float cos_dot= transformed_pos[2] / sqrtf(dst2);
					float intensity=  cos_dot * (LIGHT_SCALER * 256.0f) * fabs(light->intensity) / dst2;
					if( intensity > 1.0f )
					{
						if(intensity>max_light )
							intensity= max_light;
						if( is_colored_lightmap )
						{
							int new_light[3];
							int s= (x + y * size_x)<<2;
							new_light[0]= lightmap[s  ] + (int)(light->color[0] * intensity);
							if(new_light[0]>255) new_light[0]= 255;
							new_light[1]= lightmap[s+1] + (int)(light->color[1] * intensity);
							if(new_light[1]>255) new_light[1]= 255;
							new_light[2]= lightmap[s+2] + (int)(light->color[2] * intensity);
							if(new_light[2]>255) new_light[2]= 255;
							lightmap[s  ]= new_light[0];
							lightmap[s+1]= new_light[1];
							lightmap[s+2]= new_light[2];
						}//if colored lightmap
						else
						{
							int s= (x + y * size_x);
							int new_light= lightmap[s]+ (int)(intensity);
							if(new_light>255) new_light= 255;
							lightmap[s]= new_light;
						}//if grayscale lightmap
					}//if intencity > 1.0f

				}//for x
		}//for light

		//if( lightmap_type == D_LIGHTMAP_LINEAR_RGBS )
		//	ConverLightmapColored2RGBS( lightmap, size_x * size_y );

		lightmap_buffer.current_pos+= (is_colored_lightmap)? size_x * size_y * 4 : size_x * size_y;
		return lightmap;
	}
	else if( surf->styles[1] != 255 )
	{
		unsigned char* lightmap= lightmap_buffer.buffer + lightmap_buffer.current_pos;

		if( lightmap_type == D_LIGHTMAP_LINEAR_COLORED )
			GenerateLightmapColored( lightmap, surf );
		//else if( lightmap_type == D_LIGHTMAP_LINEAR_RGBS )
		//	GenerateLightmapRGBStoColored( lightmap, surf );
		else
			GenerateLightmapLinear( lightmap, surf );

		//if( lightmap_type == D_LIGHTMAP_LINEAR_RGBS )
		//	ConverLightmapColored2RGBS( lightmap, size_x * size_y );

		lightmap_buffer.current_pos+= (is_colored_lightmap)? size_x * size_y * 4 : size_x * size_y;
		return lightmap;
	}
	else
		return surf->samples;
}




/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t	pointcolor;
mplane_t		*lightplane;		// used as shadow plane
vec3_t			lightspot;

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	float		*scales;
	int			maps;
	float		samp;
	int			r;

	if (node->contents != -1)
		return -1;		// didn't hit anything

// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;

	if ( (back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);

	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	if (plane->type < 3)	// axial planes
		mid[plane->type] = plane->dist;

// go down front side
	r = RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something

	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing

// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&(SURF_DRAWTURB|SURF_DRAWSKY))
			continue;	// no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];
		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		int dds= ds&15; int ddt= dt&15;

		lightmap = surf->samples;
		pointcolor[0]= 0.0f; pointcolor[1]= 0.0f; pointcolor[2]= 0.0f;
		if (lightmap)
		{
			int lightmap_size= (is_colored_lightmap) ? 4 : 1;
			lightmap += ( dt * ((surf->extents[0]>>4)+1) + ds ) * lightmap_size;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					maps++)
			{
				if( is_colored_lightmap )
				{
					scales = r_newrefdef.lightstyles[surf->styles[maps]].rgb;
					float swapped_scales[3]= { scales[0], scales[1], scales[2] };

					//clamp negative values
					if( swapped_scales[0] < 0.0f ) swapped_scales[0]= 0.0f;
					if( swapped_scales[1] < 0.0f ) swapped_scales[1]= 0.0f;
					if( swapped_scales[2] < 0.0f ) swapped_scales[2]= 0.0f;

					ColorFloatSwap( swapped_scales );
					float light_scaler= 1.0f / 255.0f;
					//if( lightmap[3] != 0 )
					//	light_scaler*= float(lightmap[3]) * (1.0f/255.0f);//convertion from rgbs
					pointcolor[0]+= float(lightmap[0])* light_scaler * swapped_scales[0];
					pointcolor[1]+= float(lightmap[1])* light_scaler * swapped_scales[1];
					pointcolor[2]+= float(lightmap[2])* light_scaler * swapped_scales[2];
				}
				else
				{
					float l= (
						r_newrefdef.lightstyles[surf->styles[maps]].rgb[0] +
						r_newrefdef.lightstyles[surf->styles[maps]].rgb[1] +
						r_newrefdef.lightstyles[surf->styles[maps]].rgb[2]  ) * ( 1.0f / (3.0f * 255.0f ) );
					if( l < 0.0f )
						continue;
					l*= float(lightmap[0]);
					pointcolor[0]+= l;
					pointcolor[1]+= l;
					pointcolor[2]+= l;
				}
				lightmap+= ( ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1) ) * lightmap_size;
			}
			//take first lightmap only (temporary)
			/*if( is_colored_lightmap )
			{
				float light_scaler= 1.0f / 255.0f;
				if( lightmap[3] != 0 )
					light_scaler*= float(lightmap[3]) * (1.0f/255.0f);//convertion from rgbs
				pointcolor[0]+= float(lightmap[0])* light_scaler;
				pointcolor[1]+= float(lightmap[1])* light_scaler;
				pointcolor[2]+= float(lightmap[2])* light_scaler;
			}
			else
			{
				float l= float(lightmap[0]) * (1.0f/255.0f);
				pointcolor[0]+= l;
				pointcolor[1]+= l;
				pointcolor[2]+= l;
			}*/

		}//if is lightmap

		return 1;
	}//for surfaces

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

/*
===============
R_LightPoint
===============
*/
void R_LightPoint (vec3_t p, vec3_t color)
{
	vec3_t		end;
	float		r;
	//int			lnum;
	//dlight_t	*dl;
	//float		light;
	//vec3_t		dist;
	//float		add;

	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}


	static const float delta_vecs[]= {
		0.0f, 0.0f, 
		512.0f, 0.0f,
		-512.0f, 0.0f,
		0.0f, 512.0f,
		0.0f, -512.0f };

	float accamulated_color[]= { 0.0f, 0.0f, 0.0f };
	for( int i= 0; i< sizeof(delta_vecs)/sizeof(float)/2; i++ )
	{
		end[0] = p[0] + delta_vecs[i*2];
		end[1] = p[1] + delta_vecs[i*2+1];
		end[2] = p[2] - 2048.0f;

		r = RecursiveLightPoint (r_worldmodel->nodes, p, end);

		if (r == -1)
		{
			//VectorCopy (vec3_origin, color);
			pointcolor[0]= pointcolor[1]= pointcolor[2]= 0.5f;//PANZER - middle lighting
		}
		accamulated_color[0]+= pointcolor[0];
		accamulated_color[1]+= pointcolor[1];
		accamulated_color[2]+= pointcolor[2];
	}
	float m= 1.0f / float(sizeof(delta_vecs)/sizeof(float)/2);
	color[0]= accamulated_color[0] * m;
	color[1]= accamulated_color[1] * m;
	color[2]= accamulated_color[2] * m;

}
