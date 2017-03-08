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
/*
** RW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** software refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** SWimp_EndFrame
** SWimp_Init
** SWimp_SetPalette
** SWimp_Shutdown
*/
#include "..\ref_soft_panzer\r_local.h"
#include "rw_win.h"
#include "winquake.h"


// Console variables that we need to access from this module

swwstate_t sww_state;


static HWND console_handle;
void SWimp_OpenSystemConsole()
{
	AllocConsole();
	freopen("conin$","r",stdin);
	freopen("conout$","w",stdout);
	freopen("conout$","w",stderr);
	console_handle = GetConsoleWindow();
	MoveWindow(console_handle,1,1,680,480,1);
	printf("[rw_imp.c] Console initialized.\n");
}


static unsigned short old_gamma[256*3];
void SWimp_SaveOldHWGamma()
{
	HDC desktop_hdc = GetDC( GetDesktopWindow() );
	if( GetDeviceGammaRamp( desktop_hdc, old_gamma ) == FALSE )
		sw_state.hw_gamma_supported= 0;
	else
		sw_state.hw_gamma_supported= 1;
	ReleaseDC( GetDesktopWindow(), desktop_hdc );
}

void SWimp_SetHWGamma()
{
	unsigned short gamma[3][256];
	int i;

	for( i= 0; i< 256; i++ )
	{
		gamma[0][i]=
		gamma[1][i]=
		gamma[2][i]= sw_state.gammatable[i]<<8;
	}

	if( SetDeviceGammaRamp( sww_state.hDC, gamma ) == FALSE )
		printf( "hw gamma change error!\n" );
}

void SWimp_RestoreHWGamma()
{
	if( sw_state.hw_gamma_supported )
	{
		HDC desktop_hdc = GetDC( GetDesktopWindow() );
		SetDeviceGammaRamp( desktop_hdc, old_gamma );
		ReleaseDC( GetDesktopWindow(), desktop_hdc );
	}
}


/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME "Quake 2"

void VID_CreateWindow( int width, int height, int stylebits, swwstate_t* state )
{
	static int is_class= 0;//PANZER HACK

	WNDCLASS		wc;
	RECT			r;
	cvar_t			*vid_xpos, *vid_ypos, *vid_fullscreen;
	int				x, y, w, h;
	int				exstyle;

	vid_xpos = ri.Cvar_Get ("vid_xpos", "0", 0);
	vid_ypos = ri.Cvar_Get ("vid_ypos", "0", 0);
	vid_fullscreen = ri.Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE );

	if ( vid_fullscreen->value )
		exstyle = WS_EX_TOPMOST;
	else
		exstyle = 0;

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)sww_state.wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = sww_state.hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_GRAYTEXT;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WINDOW_CLASS_NAME;


	if (!RegisterClass (&wc) && ! is_class )
		ri.Sys_Error (ERR_FATAL, "Couldn't register window class");
    //if (!RegisterClass (&wc) )
	//	ri.Sys_Error (ERR_FATAL, "Couldn't register window class");

	is_class= 1;//PANZER HACK

	r.left = 0;
	r.top = 0;
	r.right  = width;
	r.bottom = height;

	AdjustWindowRect (&r, stylebits, FALSE);

	w = r.right - r.left;
	h = r.bottom - r.top;
	x = vid_xpos->value;
	y = vid_ypos->value;

	state->hWnd = CreateWindowEx (
		exstyle,
		 WINDOW_CLASS_NAME,
		 "Quake 2",
		 stylebits,
		 x, y, w, h,
		 NULL,
		 NULL,
		 sww_state.hInstance,
		 NULL);

	if (!state->hWnd)
		ri.Sys_Error (ERR_FATAL, "Couldn't create window");
	
	ShowWindow( state->hWnd, SW_SHOWNORMAL );
	UpdateWindow( state->hWnd );
	SetForegroundWindow( state->hWnd );
	SetFocus( state->hWnd );

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (width, height);
}


/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/
int SWimp_Init( void *hInstance, void *wndProc )
{
	sww_state.hInstance = ( HINSTANCE ) hInstance;
	sww_state.wndproc = wndProc;

	return true;
}

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.  In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/
static qboolean SWimp_InitGraphics( qboolean fullscreen )
{
	// free resources in use
	SWimp_Shutdown ();

	// create a new window
	VID_CreateWindow (vid.width, vid.height, WINDOW_STYLE, &sww_state );

	// initialize the appropriate subsystem
	if ( !fullscreen )
	{
		if ( !Panzer_DIB_Init( &vid.buffer, &sww_state ) )
		{
			vid.buffer = 0;
			vid.rowbytes = 0;
			return false;
		}
	}
	else
	{
		if ( !DDRAW_Init( &vid.buffer, &vid.rowbytes ) )
		{
			vid.buffer = 0;
			vid.rowbytes = 0;

			return false;
		}
	}

	return true;
}

/*
** SWimp_EndFrame
**
** This does an implementation specific copy from the backbuffer to the
** front buffer.  In the Win32 case it uses BitBlt or BltFast depending
** on whether we're using DIB sections/GDI or DDRAW.
*/


extern void PRast_MirrorFramebufferVertical();
extern void PRast_MakeGammaCorrection(const unsigned char* gamma_table);

void SWimp_EndFrame (void)
{
	if ( !sw_state.fullscreen )
	{
		if ( sww_state.palettized )
		{
//			holdpal = SelectPalette(hdcScreen, hpalDIB, FALSE);
//			RealizePalette(hdcScreen);
		}

	    
		BitBlt( sww_state.hDC,
			    0, 0,
				vid.width,
				vid.height,
				sww_state.hdcDIBSection,
				0, 0,
				SRCCOPY );//main copy function
		
		return;
	}
	else
	{//DirectDraw magic here
		RECT r;
		HRESULT rval;
		DDSURFACEDESC ddsd;
		r.left = 0;
		r.top = 0;
		r.right = vid.width;
		r.bottom = vid.height;

		sww_state.lpddsOffScreenBuffer->lpVtbl->Unlock( sww_state.lpddsOffScreenBuffer, vid.buffer );

		if ( sww_state.modex )
		{
			if ( ( rval = sww_state.lpddsBackBuffer->lpVtbl->BltFast( sww_state.lpddsBackBuffer,
																	0, 0,
																	sww_state.lpddsOffScreenBuffer, 
																	&r, 
																	DDBLTFAST_WAIT ) ) == DDERR_SURFACELOST )
			{
				sww_state.lpddsBackBuffer->lpVtbl->Restore( sww_state.lpddsBackBuffer );
				sww_state.lpddsBackBuffer->lpVtbl->BltFast( sww_state.lpddsBackBuffer,
															0, 0,
															sww_state.lpddsOffScreenBuffer, 
															&r, 
															DDBLTFAST_WAIT );
			}


			if ( ( rval = sww_state.lpddsFrontBuffer->lpVtbl->Flip( sww_state.lpddsFrontBuffer,
															 NULL, DDFLIP_WAIT ) ) == DDERR_SURFACELOST )
			{
				sww_state.lpddsFrontBuffer->lpVtbl->Restore( sww_state.lpddsFrontBuffer );
				sww_state.lpddsFrontBuffer->lpVtbl->Flip( sww_state.lpddsFrontBuffer, NULL, DDFLIP_WAIT );
			}
		}
		else
		{
			if ( ( rval = sww_state.lpddsBackBuffer->lpVtbl->BltFast( sww_state.lpddsFrontBuffer,
																	0, 0,
																	sww_state.lpddsOffScreenBuffer, 
																	&r, 
																	DDBLTFAST_WAIT ) ) == DDERR_SURFACELOST )
			{
				sww_state.lpddsBackBuffer->lpVtbl->Restore( sww_state.lpddsFrontBuffer );
				sww_state.lpddsBackBuffer->lpVtbl->BltFast( sww_state.lpddsFrontBuffer,
															0, 0,
															sww_state.lpddsOffScreenBuffer, 
															&r, 
															DDBLTFAST_WAIT );
			}
		}

		memset( &ddsd, 0, sizeof( ddsd ) );
		ddsd.dwSize = sizeof( ddsd );
	
		sww_state.lpddsOffScreenBuffer->lpVtbl->Lock( sww_state.lpddsOffScreenBuffer, NULL, &ddsd, DDLOCK_WAIT, NULL );

		vid.buffer = ddsd.lpSurface;
		PRast_SetFramebuffer(vid.buffer);
		vid.rowbytes = ddsd.lPitch;
	}
}

/*
** SWimp_SetMode
*/
rserr_t SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	const char *win_fs[] = { "W", "FS" };
	rserr_t retval = rserr_ok;

	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d %s\n", *pwidth, *pheight, win_fs[fullscreen] );

	sww_state.initializing = true;
	if ( fullscreen )
	{
		if ( !SWimp_InitGraphics( 1 ) )
		{
			if ( SWimp_InitGraphics( 0 ) )
			{
				// mode is legal but not as fullscreen
				fullscreen = 0;
				retval = rserr_invalid_fullscreen;
			}
			else
			{
				// failed to set a valid mode in windowed mode
				retval = rserr_unknown;
			}
		}
	}
	else
	{
		// failure to set a valid mode in windowed mode
		if ( !SWimp_InitGraphics( fullscreen ) )
		{
			sww_state.initializing = true;
			return rserr_unknown;
		}
	}

	sw_state.fullscreen = fullscreen;
#if 0
	if ( retval != rserr_unknown )
	{
		if ( retval == rserr_invalid_fullscreen ||
			 ( retval == rserr_ok && !fullscreen ) )
		{
			SetWindowLong( sww_state.hWnd, GWL_STYLE, WINDOW_STYLE );
		}
	}
#endif
	//R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );
	sww_state.initializing = true;

	return retval;
}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/
void SWimp_Shutdown( void )
{
	ri.Con_Printf( PRINT_ALL, "Shutting down SW imp\n" );
	DIB_Shutdown( &sww_state );
	DDRAW_Shutdown();

	if ( sww_state.hWnd )
	{
		ri.Con_Printf( PRINT_ALL, "...destroying window\n" );
		ShowWindow( sww_state.hWnd, SW_SHOWNORMAL );	// prevents leaving empty slots in the taskbar
		DestroyWindow (sww_state.hWnd);
		sww_state.hWnd = NULL;
		UnregisterClass (WINDOW_CLASS_NAME, sww_state.hInstance);
	}
}

/*
** SWimp_AppActivate
*/
void SWimp_AppActivate( qboolean active )
{
	if ( active )
	{
		if ( sww_state.hWnd )
		{
			SetForegroundWindow( sww_state.hWnd );
			ShowWindow( sww_state.hWnd, SW_RESTORE );
		}
	}
	else
	{
		if ( sww_state.hWnd )
		{
			if ( sww_state.initializing )
				return;
			if ( vid_fullscreen->value )
				ShowWindow( sww_state.hWnd, SW_MINIMIZE );
		}
	}
}

//===============================================================================


/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	DWORD  flOldProtect;

	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_READWRITE, &flOldProtect))
 		ri.Sys_Error(ERR_FATAL, "Protection change failed\n");
}

/*
** Sys_SetFPCW
**
** For reference:
**
** 1
** 5               0
** xxxxRRPP.xxxxxxxx
**
** PP = 00 = 24-bit single precision
** PP = 01 = reserved
** PP = 10 = 53-bit double precision
** PP = 11 = 64-bit extended precision
**
** RR = 00 = round to nearest
** RR = 01 = round down (towards -inf, floor)
** RR = 10 = round up (towards +inf, ceil)
** RR = 11 = round to zero (truncate/towards 0)
**
*/
#if !id386
void Sys_SetFPCW (void)
{
}
#else
unsigned fpu_ceil_cw, fpu_chop_cw, fpu_full_cw, fpu_cw, fpu_pushed_cw;
unsigned fpu_sp24_cw, fpu_sp24_ceil_cw;

void Sys_SetFPCW( void )
{
	__asm xor eax, eax

	__asm fnstcw  word ptr fpu_cw
	__asm mov ax, word ptr fpu_cw

	__asm and ah, 0f0h
	__asm or  ah, 003h          ; round to nearest mode, extended precision
	__asm mov fpu_full_cw, eax

	__asm and ah, 0f0h
	__asm or  ah, 00fh          ; RTZ/truncate/chop mode, extended precision
	__asm mov fpu_chop_cw, eax

	__asm and ah, 0f0h
	__asm or  ah, 00bh          ; ceil mode, extended precision
	__asm mov fpu_ceil_cw, eax

	__asm and ah, 0f0h          ; round to nearest, 24-bit single precision
	__asm mov fpu_sp24_cw, eax

	__asm and ah, 0f0h          ; ceil mode, 24-bit single precision
	__asm or  ah, 008h          ; 
	__asm mov fpu_sp24_ceil_cw, eax
}
#endif


void Sys_CreateMutex( qmutex_t* mutex )
{
	int err;
	int val;
	val= InitializeCriticalSectionAndSpinCount( &(mutex->mutex_handle), 200 );
	if(val==0)
	{
		err= GetLastError();
		printf("critical section creating error!\n" );
	}
}

void Sys_DestroyMutex( qmutex_t* mutex )
{
	DeleteCriticalSection( &(mutex->mutex_handle) );
}

void Sys_MutexLock( qmutex_t* mutex )
{
	EnterCriticalSection( &(mutex->mutex_handle) );
}
void Sys_MutexUnlock( qmutex_t* mutex )
{
	LeaveCriticalSection( &(mutex->mutex_handle) );
}

qthread_t Sys_CreateThread( unsigned int (__stdcall*func)(void*), void* params )
{
	qthread_t result;
	result.thread_handle= CreateThread( 
		NULL, 0, func, params, 0, &result.thread_id );
	return result;
}

void Sys_ExitThread()
{
	ExitThread(0);
}

void Sys_DestroyThread(qthread_t thread)
{
	TerminateThread(thread.thread_handle,0);
}


void Sys_CreateSemaphore( qsemaphore_t* sem, const char* name )
{
	sem->sem_handle= CreateSemaphore( NULL, 1, 1, name );
}
void Sys_SemaphoreWait( qsemaphore_t* sem )
{
	WaitForSingleObject( sem->sem_handle, INFINITE );
}
void Sys_SemaphoreRelease( qsemaphore_t* sem )
{
	int result= ReleaseSemaphore( sem->sem_handle, 1, NULL );
	if( result == 0 )
	{
		//error here
	}
}
