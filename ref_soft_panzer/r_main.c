/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_main.c

#include "rendering.h"
#include "r_light.h"
#include "panzer_rast/rasterization.h"
#include "r_surf.h"

extern void PR_BeginFrame();
extern void PR_SwapCommandBuffers();
extern void PR_EndBufferSwapping();
extern void PR_ResetTimers();
extern void PR_SetFrameBuffeer(unsigned char* buff);
extern void PR_LockFramebuffer();
extern void PR_UnlockFramebuffer();


viddef_t	vid;//PANZER main framebuffer structure

refimport_t	ri;

unsigned	d_8to24table[256];//PANZER - maybe it is main palette
unsigned char cinematic_palette[256*4];

entity_t	r_worldentity;

char		skyname[MAX_QPATH];
float		skyrotate;
vec3_t		skyaxis;
image_t		*sky_images[6];

refdef_t	r_newrefdef;
model_t		*currentmodel;

model_t		*r_worldmodel= NULL;

byte		r_warpbuffer[WARP_WIDTH * WARP_HEIGHT];

swstate_t sw_state;

void		*colormap;
vec3_t		viewlightvec;
alight_t	r_viewlighting = {128, 192, viewlightvec};
float		r_time1;
int			r_numallocatededges;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp;

mvertex_t	*r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//flashlight_t player_flashlight;
//
// screen size info
//
oldrefdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		r_screenwidth;

float	verticalFieldOfView;
float	xOrigin, yOrigin;

mplane_t	screenedge[4];

//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount=-1;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

mleaf_t		*r_viewleaf;
int			r_viewcluster, r_oldviewcluster;

image_t  	*r_notexture_mip;

float	da_time1, da_time2, dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2;

void R_MarkLeaves (void);

cvar_t	*r_lefthand;
cvar_t	*sw_aliasstats;
cvar_t	*sw_allow_modex;
cvar_t	*sw_clearcolor;
cvar_t	*sw_drawflat;
cvar_t	*sw_draworder;
cvar_t	*sw_maxedges;
cvar_t	*sw_maxsurfs;
cvar_t  *sw_mode;
cvar_t	*sw_reportedgeout;
cvar_t	*sw_reportsurfout;
cvar_t  *sw_stipplealpha;
cvar_t	*sw_surfcacheoverride;
cvar_t	*sw_waterwarp;

cvar_t	*r_drawworld;
cvar_t	*r_drawentities;
cvar_t	*r_dspeeds;
cvar_t	*r_fullbright;
cvar_t  *r_lerpmodels;
cvar_t  *r_novis;

cvar_t	*r_speeds;
cvar_t	*r_lightlevel;	//FIXME HACK

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;

//PGM
cvar_t	*sw_lockpvs;
//PGM

cvar_t  *r_use_multithreading;
cvar_t  *r_lightmap_mode;
cvar_t  *r_texture_mode;
cvar_t  *r_lightmap_saturation;
cvar_t  *r_dlights_saturation;
cvar_t	*r_palettized_textures;
cvar_t	*r_interpolate_videos;
cvar_t	*r_clear_color_buffer;
cvar_t  *r_surface_caching;
cvar_t  *r_use_ddraw;

/*
after map switching some resources ( models and ther skins, textures )
may neeed. registration_sequence used as number of level number
*/
int registration_sequence= 1;

#define	STRINGER(x) "x"


#if	!id386

// r_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver


// d_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

fixed16_t	sadjust, tadjust, bbextents, bbextentt;

pixel_t			*cacheblock;
int				cachewidth;
pixel_t			*d_viewbuffer;
short			*d_pzbuffer;
unsigned int	d_zrowbytes;
unsigned int	d_zwidth;


#endif	// !id386




/* 
================== 
GL_ScreenShot_f
================== 
*/  
void R_ScreenShot_f (void) 
{
	byte		*buffer;
	char		picname[80]; 
	char		checkname[MAX_OSPATH];
	int			i, c, temp;
	FILE		*f;

	unsigned char* dst, *src;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot", ri.FS_Gamedir());
	Sys_Mkdir (checkname);

// 
// find a file name to save it to 
// 
	strcpy(picname,"quake00.tga");

	for (i=0 ; i<=99 ; i++) 
	{ 
		picname[5] = i/10 + '0'; 
		picname[6] = i%10 + '0'; 
		Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/%s", ri.FS_Gamedir(), picname);
		f = fopen (checkname, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose (f);
	} 
	if (i==100) 
	{
		ri.Con_Printf (PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n"); 
		return;
 	}


	buffer = malloc(vid.width*vid.height*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = vid.width&255;
	buffer[13] = vid.width>>8;
	buffer[14] = vid.height&255;
	buffer[15] = vid.height>>8;
	buffer[16] = 24;	// pixel size

	//qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 
	src= vid.buffer;
	dst= buffer+18;
	for( i= 0; i<  vid.width * vid.height; i++, dst+=3, src+=4 )
	{
		dst[0]= src[0];
		dst[1]= src[1];
		dst[2]= src[2];
	}

	c = 18+vid.width*vid.height*3;
	// swap rgb to bgr 
	//PANZER - do not need it
	/*for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}*/

	f = fopen (checkname, "wb");
	fwrite (buffer, 1, c, f);
	fclose (f);

	free (buffer);
	ri.Con_Printf (PRINT_ALL, "Wrote %s\n", picname);
} 


void Draw_GetPalette (void)
{
	byte	*pal, *out;
	int		i;
	int		r, g, b;

	// get the palette and colormap
	LoadPCX ("pics/colormap.pcx", &vid.colormap, &pal, NULL, NULL);
	if (!vid.colormap)
		ri.Sys_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");
	vid.alphamap = vid.colormap + 64*256;

	out = (byte *)d_8to24table;
	for (i=0 ; i<256 ; i++, out+=4)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];

        out[0] = r;
        out[1] = g;
        out[2] = b;
		ColorByteSwap(out);
	}

	free (pal);
}



/*
================
Draw_BuildGammaTable
================
*/
void Draw_BuildGammaTable (void)
{
	int		i, inf;
	float	g;

	g = vid_gamma->value;

	if (g == 1.0)
	{
		for (i=0 ; i<256 ; i++)
			sw_state.gammatable[i] = i;
		return;
	}
	
	for (i=0 ; i<256 ; i++)
	{
		inf = (int)(255.0f * powf ( (i+0.5f)/255.5f , g ) + 0.5f);
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		sw_state.gammatable[i] = inf;
	}
}



void R_Flashlight_f(void)
{
}

void PANZER_Register (void)
{
	//sw_aliasstats = ri.Cvar_Get ("sw_polymodelstats", "0", 0);
	//sw_allow_modex = ri.Cvar_Get( "sw_allow_modex", "1", CVAR_ARCHIVE );
	//sw_clearcolor = ri.Cvar_Get ("sw_clearcolor", "2", 0);
	//sw_drawflat = ri.Cvar_Get ("sw_drawflat", "0", 0);
	//sw_draworder = ri.Cvar_Get ("sw_draworder", "0", 0);
	//sw_maxedges = ri.Cvar_Get ("sw_maxedges", STRINGER(MAXSTACKSURFACES), 0);
	//sw_maxsurfs = ri.Cvar_Get ("sw_maxsurfs", "0", 0);
	//sw_mipcap = ri.Cvar_Get ("sw_mipcap", "0", 0);
	//sw_mipscale = ri.Cvar_Get ("sw_mipscale", "1", 0);
	//sw_reportedgeout = ri.Cvar_Get ("sw_reportedgeout", "0", 0);
	//sw_reportsurfout = ri.Cvar_Get ("sw_reportsurfout", "0", 0);
	//sw_stipplealpha = ri.Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );
	//sw_surfcacheoverride = ri.Cvar_Get ("sw_surfcacheoverride", "0", 0);
	//sw_waterwarp = ri.Cvar_Get ("sw_waterwarp", "1", 0);
	sw_mode = ri.Cvar_Get( "sw_mode", "0", CVAR_ARCHIVE );

	r_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	//r_speeds = ri.Cvar_Get ("r_speeds", "0", 0);
	//r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0);
	//r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0);
	//r_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0);
	//r_dspeeds = ri.Cvar_Get ("r_dspeeds", "0", 0);
	//r_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", 0);
	//r_lerpmodels = ri.Cvar_Get( "r_lerpmodels", "1", 0 );
	r_novis = ri.Cvar_Get( "r_novis", "0", 0 );

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = ri.Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	r_lightmap_mode = ri.Cvar_Get( "r_lightmap_mode", "lightmap_linear_colored", CVAR_ARCHIVE ); r_lightmap_mode->modified= false;
	r_lightmap_saturation= ri.Cvar_Get( "r_lightmap_saturation", "0.7", CVAR_ARCHIVE ); r_lightmap_saturation->modified= false;
	r_dlights_saturation= ri.Cvar_Get( "r_dlights_saturation", "0.9", CVAR_ARCHIVE ); r_dlights_saturation->modified= false;
	r_texture_mode= ri.Cvar_Get( "r_texture_mode", "texture_nearest", CVAR_ARCHIVE ); r_texture_mode->modified= false;
	r_use_multithreading= ri.Cvar_Get( "r_use_multithreading", "0", CVAR_ARCHIVE ); r_use_multithreading->modified= false;
	r_palettized_textures= ri.Cvar_Get( "r_palettized_textures", "0", CVAR_ARCHIVE ); r_palettized_textures->modified= false;
	r_interpolate_videos= ri.Cvar_Get( "r_interpolate_videos", "1", CVAR_ARCHIVE );
	r_clear_color_buffer= ri.Cvar_Get( "r_clear_color_buffer", "0", CVAR_ARCHIVE );
	r_surface_caching= ri.Cvar_Get( "r_surface_caching", "1", CVAR_ARCHIVE );
	r_use_ddraw= ri.Cvar_Get( "r_use_ddraw", "1", CVAR_ARCHIVE );r_use_ddraw->modified= false;

	//ri.Cmd_AddCommand( "flashlight", R_Flashlight_f );
	//ri.Cmd_ExecuteText( 0, "bind f flashlight" );
	ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );

	sw_mode->modified = true; // force us to do mode specific stuff later
	vid_gamma->modified = true; // force us to rebuild the gamma table later

//PGM
	sw_lockpvs = ri.Cvar_Get ("sw_lockpvs", "0", 0);
//PGM

}



void R_UnRegister (void)
{
	ri.Cmd_RemoveCommand( "flashlight" );
	ri.Cmd_RemoveCommand( "screenshot" );
}



/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/


void PANZER_Shutdown(void)
{
	extern void PR_ShutdownRendering();

	SWimp_RestoreHWGamma();

	R_UnRegister();
	PR_ShutdownRendering();
	PRast_Shutdown();
	SWimp_Shutdown();

	ShutdownSurfaceCache();
}
void PANZER_BeginRegistration(char *map)
{
	if(r_worldmodel!=NULL)
	{
		PR_BeginFrame();
		PR_SwapCommandBuffers();
		PR_EndBufferSwapping();
	}
	registration_sequence++;
	R_BeginRegistration(map);
	ResetSurfaceCache();
	//printf( "--------begin registration. map - \"%s\"\n", map );
}

struct model_s *PANZER_RegisterModel(char *name)
{
	model_t* mod= R_RegisterModel(name);
	//printf( "register model \"%s\"%s\n", name, mod == NULL ? " - failed" : "" );
	return mod;
}
struct image_s *PANZER_RegisterPic(char *name)
{
	image_t	*image;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
		image = R_FindImage (fullname, it_pic);
	}
	else
		image = R_FindImage (name+1, it_pic);

	return image;
}
struct image_s *PANZER_RegisterSkin(char *name)
{
	image_t	*image;
	image = R_FindImage (name, it_skin);
	//printf( "register skin \"%s\"%s\n", name, image == NULL ? " - failed" : "" );
	return image;
}

extern void PANZER_SetSky(char *name, float rotate, vec3_t axis);
void PANZER_EndRegistration(void)
{
	R_EndRegistration();
	PR_ResetTimers();
	//R_FreeUnusedImages();
	//printf( "--------end registration\n" );
}

extern void PANZER_RenderFrame(refdef_t *fd);


void PANZER_DrawGetPicSize(int *w, int *h, char *name)
{
	image_t* img= PANZER_RegisterPic( name );
	if( img != NULL )
	{
		*w= img->width;
		*h= img->height;
	}
	else
	{
		*w= *h= 0;
	}
}
extern void PANZER_DrawPic(int x, int y, char *name);
extern void PANZER_DrawChar(int x, int y, int c);
extern void PANZER_DrawTileClear(int x, int y, int w, int h, char *name);
extern void PANZER_DrawFill(int x, int y, int w, int h, int c);

extern void PANZER_DrawStretchPic(int x, int y, int w, int h, char *name);

extern void PANZER_DrawFadeScreen(void);

void PANZER_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	int i, j, u, v;
	unsigned char* dst, *src;
	fixed16_t du= ((cols-2)<<16)/w;
	fixed16_t dv= ((rows-2)<<16)/h;

	if( r_interpolate_videos->value )
	{
		for( j= y+h-1, v= 65536; j>= y; j--, v+=dv )
			for( i= x, u= 65536, dst= vid.buffer + ((x + j*vid.width)<<2), src= data + (v>>16)*cols;
				i< x+w; i++, u+=du, dst+=4 )
			{
				//make linear interpolation
				int iu = u>>16;
				int iv=  v>>16;
				int ddu= (u&65535)>>8;
				int ddu1= 256 - ddu;
				int ddv= (v&65535)>>8;
				int ddv1= 256 - ddv;
				int colors[12];
				int ind;
				int coord= iu + iv*cols;
				ind= data[coord]<<2;
				colors[0 ]= cinematic_palette[ind  ];
				colors[1 ]= cinematic_palette[ind+1];
				colors[2 ]= cinematic_palette[ind+2];
				coord++; ind= data[coord]<<2;
				colors[3 ]= cinematic_palette[ind  ];
				colors[4 ]= cinematic_palette[ind+1];
				colors[5 ]= cinematic_palette[ind+2];
				coord+= cols; ind= data[coord]<<2;
				colors[9 ]= cinematic_palette[ind  ];
				colors[10]= cinematic_palette[ind+1];
				colors[11]= cinematic_palette[ind+2];
				coord--; ind= data[coord]<<2;
				colors[6 ]= cinematic_palette[ind  ];
				colors[7 ]= cinematic_palette[ind+1];
				colors[8 ]= cinematic_palette[ind+2];

				colors[0]= ( colors[0] * ddu1 + colors[ 3] * ddu );
				colors[1]= ( colors[1] * ddu1 + colors[ 4] * ddu );
				colors[2]= ( colors[2] * ddu1 + colors[ 5] * ddu );
				colors[3]= ( colors[6] * ddu1 + colors[ 9] * ddu );
				colors[4]= ( colors[7] * ddu1 + colors[10] * ddu );
				colors[5]= ( colors[8] * ddu1 + colors[11] * ddu );

				dst[0]= ( colors[0] * ddv1 + colors[3] * ddv ) >> 16;
				dst[1]= ( colors[1] * ddv1 + colors[4] * ddv ) >> 16;
				dst[2]= ( colors[2] * ddv1 + colors[5] * ddv ) >> 16;

			}
	}//if iterpolate
	else
	{
		for( j= y+h-1, v= 65536; j>= y; j--, v+=dv )
			for( i= x, u= 65536, dst= vid.buffer + ((x + j*vid.width)<<2), src= data + (v>>16)*cols;
				i< x+w; i++, u+=du, dst+=4 )
			{
				*((int*)dst)= ((int*)cinematic_palette)[ src[(u>>16)] ];  
			}
	}
}

void PANZER_CinematicSetPalette( const unsigned char *palette)
{
	int i;
	if( palette == NULL )
	{
		memcpy( cinematic_palette, d_8to24table, 256*4 );
		return;
	}
	
	for( i= 0; i< 256; i++ )
	{
		cinematic_palette[i*4  ]= palette[i*3  ];
		cinematic_palette[i*4+1]= palette[i*3+1];
		cinematic_palette[i*4+2]= palette[i*3+2];
		ColorByteSwap( cinematic_palette + i*4 );
	}
}
void PANZER_BeginFrame( float camera_separation )
{
	r_framecount++;

	if( r_use_multithreading->modified || r_lightmap_mode->modified || r_lightmap_saturation->modified || 
		r_palettized_textures->modified || r_use_ddraw->modified )
	{
		r_use_multithreading->modified= false;
		r_lightmap_mode->modified= false;
		r_lightmap_saturation->modified= false;
		r_palettized_textures->modified= false;
		r_use_ddraw->modified= false;
		ri.Cmd_ExecuteText( 0, "vid_restart" );
	}
	if( sw_mode->modified || vid_fullscreen->modified )
	{
		PR_LockFramebuffer();

		SWimp_SetMode( &vid.width, &vid.height, sw_mode->value, vid_fullscreen->value );
		PRast_Init( vid.width, vid.height );//init framebuffers and depth buffer 
		PR_SetFrameBuffeer( vid.buffer );
		if( (((int)vid.buffer)&0xF) != 0 )
			ri.Con_Printf( PRINT_ALL, "warning, unaligned framebuffer: 0x%X\n", vid.buffer );
		else if ( (((int)vid.buffer)&7) != 0 )
			ri.Con_Printf( PRINT_ALL, "warning, FATAL unaligned framebuffer: 0x%X\n", vid.buffer );
		else
			ri.Con_Printf( PRINT_ALL, "all ok, framebuffer aligned: 0x%X\n", vid.buffer );
		sw_mode->modified = false;

		PR_UnlockFramebuffer();

		vid_fullscreen->modified= false;

		ResetSurfaceCache();
		ShutdownSurfaceCache();
		InitSurfaceCache();
	}
	if( vid_gamma->modified )
	{
		//clamp invalid gamma values
		if( vid_gamma->value < 0.5f )
			ri.Cvar_Set( "vid_gamma", "0.5" );
		else if( vid_gamma->value > 1.3f )
			ri.Cvar_Set( "vid_gamma", "1.3" );

		Draw_BuildGammaTable();
		if( sw_state.hw_gamma_supported )
			SWimp_SetHWGamma();
		vid_gamma->modified= false;
	}
}

void PANZER_EndFrame(void)
{
	PR_SwapCommandBuffers();//wait for backend thread
	SWimp_EndFrame();//send picture to device
	PR_EndBufferSwapping();//
}

extern void SWimp_AppActivate( qboolean active );
void PANZER_AppActivate( qboolean activate )
{
	SWimp_AppActivate(activate);
}


qboolean PANZER_Init ( void *hinstance, void *wndproc )
{
	extern PR_InitRendering();

	SWimp_SaveOldHWGamma();

	Draw_GetPalette();
	SWimp_OpenSystemConsole();
	R_LightInit();

	PANZER_Register();//register c_vars
	SWimp_Init( hinstance, wndproc );// write hinstance and wndproc

	PR_InitRendering();

	// create the window
	PANZER_BeginFrame( 0.0f );
	
	ri.Con_Printf (PRINT_ALL, "panzer_ref_soft version: "REF_VERSION"\n");

	InitSurfaceCache();

	return true;
}

refexport_t GetRefAPI (refimport_t rimp)
{
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = PANZER_BeginRegistration;
    re.RegisterModel = PANZER_RegisterModel;
    re.RegisterSkin = PANZER_RegisterSkin;
	re.RegisterPic = PANZER_RegisterPic;
	re.SetSky = PANZER_SetSky;
	re.EndRegistration = PANZER_EndRegistration;

	re.RenderFrame = PANZER_RenderFrame;

	re.DrawGetPicSize = PANZER_DrawGetPicSize;
	re.DrawPic = PANZER_DrawPic;
	re.DrawStretchPic = PANZER_DrawStretchPic;
	re.DrawChar = PANZER_DrawChar;
	re.DrawTileClear = PANZER_DrawTileClear;
	re.DrawFill = PANZER_DrawFill;
	re.DrawFadeScreen= PANZER_DrawFadeScreen;

	re.DrawStretchRaw = PANZER_DrawStretchRaw;

	re.Init = PANZER_Init;
	re.Shutdown = PANZER_Shutdown;

	re.CinematicSetPalette = PANZER_CinematicSetPalette;
	re.BeginFrame = PANZER_BeginFrame;
	re.EndFrame = PANZER_EndFrame;

	re.AppActivate = PANZER_AppActivate;

	return re;
}

#ifndef REF_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	ri.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	ri.Con_Printf (PRINT_ALL, "%s", text);
}

#endif


void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	mleaf_t	*leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && !r_novis->value && r_viewcluster != -1)
		return;
	
	// development aid to let you run around and see exactly where
	// the pvs ends
	if (sw_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);
	
	for (i=0,leaf=r_worldmodel->leafs ; i<r_worldmodel->numleafs ; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}

#if 0
	for (i=0 ; i<r_worldmodel->vis->numclusters ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&r_worldmodel->leafs[i];	// FIXME: cluster
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
#endif
}
