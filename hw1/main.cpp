#include <windows.h>

#include <gl/gl.h>

#include <stdint.h>
#include <stdio.h>

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_rect_pack.h"
#include "stb_sprintf.h"
#include "stb_truetype.h"

#include "app.h"
#include "app.cpp"

// TODO: Find out what is causing the occasional flickering (happens sometimes on startup).
//       Probably something to do with OpenGL but don't know whether it is my fault or the AMD driver's fault.

void win_get_client_dim(HWND window, int *w, int *h)
{
	RECT rect;
	GetClientRect(window, &rect);

	*w = rect.right - rect.left;
	*h = rect.bottom - rect.top;
}

LRESULT win_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	LRESULT result = 0;

	switch(msg)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC device_ctx = BeginPaint(window, &ps);
				EndPaint(window, &ps);
			}break;

		default:
			result = DefWindowProc(window, msg, w_param, l_param);
	}

	return result;
}

int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd)
{
	WNDCLASSEX wc    = {};
	wc.cbSize        = sizeof(wc);
	wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = win_proc;
	wc.hInstance     = instance;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName  = "FacilityGenMenu";
	wc.lpszClassName = "FacitityGenClass";
	if(!RegisterClassEx(&wc))
	{
		return 1;
	}

	DWORD window_style = WS_OVERLAPPEDWINDOW;
	HWND  window       = CreateWindowEx(0, wc.lpszClassName, "Facility Generation", window_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, instance, NULL);
	if(!window)
	{
		return 1;
	}

	HDC device_ctx = GetDC(window);

	PIXELFORMATDESCRIPTOR desired_pixel_format_desc = {};

	desired_pixel_format_desc.nSize        = sizeof(desired_pixel_format_desc);
	desired_pixel_format_desc.nVersion     = 1;
	desired_pixel_format_desc.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	desired_pixel_format_desc.iPixelType   = PFD_TYPE_RGBA; 
	desired_pixel_format_desc.cColorBits   = 32;
	desired_pixel_format_desc.cAlphaBits   = 8;
	desired_pixel_format_desc.cDepthBits   = 24;
	desired_pixel_format_desc.cStencilBits = 8;
	desired_pixel_format_desc.iLayerType   = PFD_MAIN_PLANE;

	int suggested_pixel_format = ChoosePixelFormat(device_ctx, &desired_pixel_format_desc);
	if(!suggested_pixel_format)
	{
		return 1;
	}

	PIXELFORMATDESCRIPTOR suggested_pixel_format_desc = {};
	DescribePixelFormat(device_ctx, suggested_pixel_format, sizeof(suggested_pixel_format_desc), &suggested_pixel_format_desc);

	if(!SetPixelFormat(device_ctx, suggested_pixel_format, &suggested_pixel_format_desc))
	{
		return 1;
	}

	HGLRC gl_ctx = wglCreateContext(device_ctx);
	if(!gl_ctx)
	{
		return 1;
	}

	if(!wglMakeCurrent(device_ctx, gl_ctx))
	{
		return 1;
	}

	LARGE_INTEGER large_rng_seed;
	QueryPerformanceCounter(&large_rng_seed);

	InputState input = {};
	input.rng_seed   = 69;

	win_get_client_dim(window, &input.client_w, &input.client_h);

	AppState app = app_make("c:/windows/fonts/arial.ttf", &input);

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	ShowWindow(window, SW_SHOW);

	LARGE_INTEGER prev_count;
	QueryPerformanceCounter(&prev_count);

	for(;;)
	{
		MSG msg;
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
			{
				goto exit;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		win_get_client_dim(window, &input.client_w, &input.client_h);

		app_update(&app, &input);

		SwapBuffers(device_ctx);

		LARGE_INTEGER curr_count;
		QueryPerformanceCounter(&curr_count);

		LARGE_INTEGER elapsed_count;
		elapsed_count.QuadPart = curr_count.QuadPart - prev_count.QuadPart;

		input.elapsed_microsecs = (elapsed_count.QuadPart * 1000000) / frequency.QuadPart;

		prev_count = curr_count;
	}

exit:
	wglDeleteContext(gl_ctx);
	DeleteDC(device_ctx);
	DestroyWindow(window);
	return 0;
}
