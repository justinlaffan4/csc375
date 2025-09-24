#define OEMRESOURCE // Needed for OCR_NORMAL

#include <windows.h>

#include <gl/gl.h>
#include <gl/glu.h>

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
				BeginPaint(window, &ps);
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
	wc.lpszClassName = "FacilityGenWindowClass";
	wc.hCursor       = (HCURSOR)LoadImage(NULL, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
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

	PIXELFORMATDESCRIPTOR pixel_format_desc = {};

	pixel_format_desc.nSize        = sizeof(pixel_format_desc);
	pixel_format_desc.nVersion     = 1;
	pixel_format_desc.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_TYPE_RGBA;
	pixel_format_desc.iPixelType   = PFD_TYPE_RGBA; 
	pixel_format_desc.cColorBits   = 32;
	pixel_format_desc.cDepthBits   = 24;
	pixel_format_desc.cStencilBits = 8;
	pixel_format_desc.iLayerType   = PFD_MAIN_PLANE;

	int pixel_format = ChoosePixelFormat(device_ctx, &pixel_format_desc);
	if(!pixel_format)
	{
		return 1;
	}

	if(!SetPixelFormat(device_ctx, pixel_format, &pixel_format_desc))
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

	int client_w, client_h;
	win_get_client_dim(window, &client_w, &client_h);

	ShowWindow(window, SW_SHOW);

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

		win_get_client_dim(window, &client_w, &client_h);

		float aspect_ratio        = (float)client_w / (float)client_h;
		float target_aspect_ratio = 1.0f; // Perfectly square

		float viewport_x = 0;
		float viewport_y = 0;
		float viewport_w = client_w;
		float viewport_h = client_h;
		if(aspect_ratio > target_aspect_ratio)
		{
			viewport_w = client_h * target_aspect_ratio;
			viewport_h = client_h;

			viewport_x = (client_w - viewport_w) * 0.5f;
			viewport_y = 0;
		}else
		{
			viewport_w = client_w;
			viewport_h = client_w * (1 / target_aspect_ratio);

			viewport_x = 0;
			viewport_y = (client_h - viewport_h ) * 0.5f;
		}

		glViewport(viewport_x, viewport_y, viewport_w, viewport_h);

		int map_w = 32;
		int map_h = 32;

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, map_w, map_h, 0, 0, 1);

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		glBegin(GL_QUADS);
		{
			for(int y = 0; y < map_h; ++y)
			{
				int remainder_to_check = 0;
				if(y % 2 == 0)
				{
					remainder_to_check = 1;
				}

				for(int x = 0; x < map_w; ++x)
				{
					if(x % 2 == remainder_to_check)
					{
						glColor3f(0, 0, 1);
					}else
					{
						glColor3f(0, 1, 0);
					}

					float w = 1;
					float h = 1;

					glVertex2f(x,     y    );
					glVertex2f(x + w, y    );
					glVertex2f(x + w, y + h);
					glVertex2f(x,     y + h);
				}
			}
		}
		glEnd();

		SwapBuffers(device_ctx);
	}

exit:
	return 0;
}
