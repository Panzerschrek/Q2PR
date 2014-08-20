#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "rasterization.h"
#include "fixed.h"
#include "fast_math.h"
#include "psr_mmx.h"
#include "math_lib/m_math.h"
#include "math_lib/vec.h"
#include "rendering_commands.h"

int SaveTextureTGA( const char* file_name, unsigned char* data, unsigned int sizeX, unsigned int sizeY )
{
    unsigned char  TGAheader[12]= {0,0,2,0,0,0,0,0,0,0,0,0};
    FILE* file_tga=fopen( file_name,"wb" );
    if (file_tga==NULL)
        return 1;
    unsigned char o_0=0;
    fwrite(TGAheader,1,12,file_tga);
    fwrite(&sizeX,1,2,file_tga);
    fwrite(&sizeY,1,2,file_tga);

    unsigned char bit=32;
    fwrite(&bit,1,1,file_tga);
    fwrite(&o_0,1,1,file_tga);
    int imagesize=sizeX*sizeY*4;
    unsigned char* data2=new unsigned char[imagesize];
    int step=bit/8;
    for (int i=0; i<imagesize; i+=step)
    {
        data2[i]=data[i+2];
        data2[i+1]=data[i+1];
        data2[i+2]=data[i];
    }
    fwrite(data2,1,imagesize,file_tga);
    fclose(file_tga);
    delete[] data2;

    return 0;
}

extern unsigned char* screen_buffer;


/*template< int x > int CompileTimeLog2Floor()
{
	return x <= 1
		? ( x == 0 ? -1 : 0 )
		: CompileTimeLog2Floor< (x>>1) >() + 1;
}

template< int x > int CompileTimeLog2Ceil()
{
	return x == 0 ? 1 : (
		x ==  ( 1 << CompileTimeLog2Floor<x>() )
		? CompileTimeLog2Floor<x>()
		: CompileTimeLog2Floor<x>() + 1 );
}*/




void DrawQuad( int k= 0 )
{
	using namespace VertexProcessing;
	char buff[256];

	m_Vec3 quad_init_vertices[]= { m_Vec3( 1.0f, 0.9f, 10.0f ), m_Vec3( 1.0f, -1.0f, 1.1f ), m_Vec3( -1.0f, -1.0f, 1.0f ), m_Vec3( -1.0f, 1.0f, 10.0f ) };
	fixed16_t quad_z[4];
	int quad_coord[4*2];

	for( int i= 0; i< 4; i++ )
	{
		quad_z[i]= int( quad_init_vertices[i].z * 65536.0f );
		quad_init_vertices[i]/= quad_init_vertices[i].z;
		quad_coord[i*2]= int( quad_init_vertices[i].x * 256.0f ) + 256;
		quad_coord[i*2+1]= int( quad_init_vertices[i].y * 256.0f ) + 256;
	}


	unsigned char colors[]= { 252,4,252,0, 4,4,252,0, 4,4,4,0, 252,4,4,0 };

	triangle_in_vertex_xy[0]= quad_coord[0];
	triangle_in_vertex_xy[1]= quad_coord[1];
	triangle_in_vertex_xy[2]= quad_coord[2];
	triangle_in_vertex_xy[3]= quad_coord[3];
	triangle_in_vertex_xy[4]= quad_coord[4];
	triangle_in_vertex_xy[5]= quad_coord[5];
	((int*)triangle_in_color)[0]= ((int*)colors)[0];
	((int*)triangle_in_color)[1]= ((int*)colors)[1];
	((int*)triangle_in_color)[2]= ((int*)colors)[2];
	triangle_in_vertex_z[0]= quad_z[0];
	triangle_in_vertex_z[1]= quad_z[1];
	triangle_in_vertex_z[2]= quad_z[2];
	

	char* b= buff;
	b+= ComIn_SetConstantBlendFactor( b, 16 );

	*((int*)b)= DRAW_TRIANGLE_FROM_BUFFER;
	DrawTriangleCall* call= (DrawTriangleCall*)( b + sizeof(int));

	call->DrawFromBufferFunc= DrawColoredTriangleFromBuffer;
	call->triangle_count= 2;
	call->vertex_size= sizeof(int) * 3 + 4;

	b+= sizeof(DrawTriangleCall) + sizeof(int);
	DrawColoredTriangleToBuffer( b );
	b+=16*4;

	triangle_in_vertex_xy[2]= quad_coord[6];
	triangle_in_vertex_xy[3]= quad_coord[7];
	((int*)triangle_in_color)[1]= ((int*)colors)[3];
	triangle_in_vertex_z[1]= quad_z[3];
	DrawColoredTriangleToBuffer( b);
	b+=16*4;

	//DrawColoredTriangleFromBuffer( buff );
	//DrawColoredTriangleFromBuffer( buff + 16*4 );
	ComOut_DoCommands( buff, b - buff );


}


HWND hwnd;
HDC hdc;
HGLRC hrc;
WNDCLASSEX window_class;
int border_x, border_y;
int screen_x=  800, screen_y= 800;

void  StartWindow()
{
	  int border_size, top_border_size, bottom_border_size;
    static const char* WINDOW_NAME= "MW";

    window_class.cbSize = sizeof(WNDCLASSEX);
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = DefWindowProc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance =  0;
    window_class.hIcon = LoadIcon( 0 , IDI_APPLICATION);
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszMenuName = NULL;
    window_class.lpszClassName = WINDOW_NAME;
    window_class.hIconSm = LoadIcon(NULL, IDI_APPLICATION);


    if ( ! RegisterClassEx( &window_class ) )
        goto display_error;


    border_size=  GetSystemMetrics(SM_CXFIXEDFRAME);
    bottom_border_size= GetSystemMetrics(SM_CYFIXEDFRAME);
    top_border_size= bottom_border_size + GetSystemMetrics(SM_CYCAPTION);

    border_x= border_size * 2;
    border_y= top_border_size + bottom_border_size;
    hwnd  = CreateWindowEx(0,
                           WINDOW_NAME,
                           WINDOW_NAME,
                           /*WS_OVERLAPPED|WS_CAPTION|WS_MINIMIZEBOX|WS_SYSMENU*/WS_OVERLAPPEDWINDOW,
                           0,
                           0,
                           screen_x + border_size * 2,
                           screen_y + top_border_size + bottom_border_size,
                           NULL,
                           NULL,
                           /*h_instance*/0,
                           NULL);

    if ( ! hwnd )
        goto display_error;

    ShowWindow( hwnd, SW_SHOWNORMAL );
    hdc= GetDC( hwnd );

display_error:;
}


namespace Draw
{
	void FadeScreen();

}
void DrawColoredSprinte( int, int, int, int, const unsigned char* color  );
int main()
{

	Init( 512, 512 );
	unsigned char color[]= { 32, 128, 64, 0 };

	ClearColorBuffer( color );
	ClearDepthBufferByZValue( 6 * 65536 );
	DrawQuad();
	//Draw::FadeScreen();
	
	DrawColoredSprinte( 0, 32, 0, 32, color );
	DrawFill( 312, 217, 524, 471, 0 );
	/*StartWindow();
	BITMAPINFOHEADER header_info;
	BITMAPINFO bitmap_info;
	bitmap_info.bmiHeader.biSize= sizeof(BITMAPINFO);
	bitmap_info.bmiHeader.biWidth= 512;
	bitmap_info.bmiHeader.biHeight= 512;
	bitmap_info.bmiHeader.biPlanes= 1;
	bitmap_info.bmiHeader.biBitCount= 32;
	bitmap_info.bmiHeader.biCompression= BI_RGB;
	bitmap_info.bmiHeader.biSizeImage= 512*512 * 4;
	bitmap_info.bmiHeader.biClrUsed= 0;
	bitmap_info.bmiHeader.biClrImportant= 0;
	bitmap_info.bmiColors[0].rgbBlue= 255;
	bitmap_info.bmiColors[0].rgbGreen= 255;
	bitmap_info.bmiColors[0].rgbRed= 255;

	HBITMAP ddb= CreateDIBitmap( hdc, &bitmap_info.bmiHeader, CBM_INIT, screen_buffer,& bitmap_info, DIB_RGB_COLORS );
	int t=0;
	while(1)
	{
		 MSG msg;
		while (PeekMessage(&msg,NULL,0,0,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		InvalidateRect ( hwnd,NULL, false );
		PAINTSTRUCT ps;
		HDC hdc2= BeginPaint(hwnd, &ps );

		ClearColorBuffer( color );
		ClearDepthBuffer( 32767 );
		DrawQuad(t/10);
		t++;

		StretchDIBits( hdc, 0, 0, 512, 512,   0, 0, 512, 512, screen_buffer, &bitmap_info, DIB_RGB_COLORS, SRCCOPY );
		EndPaint( hwnd, &ps );
		


		Sleep(1);
	}*/
	SaveTextureTGA( "1.tga", screen_buffer, 512, 512 );

	return 0;
}

