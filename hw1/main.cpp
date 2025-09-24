#define OEMRESOURCE // Needed for OCR_NORMAL
#include <windows.h>

#include <gl/gl.h>
#include <gl/glu.h>

#include <stdint.h>
#include <stdio.h>

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_rect_pack.h"
#include "stb_sprintf.h"
#include "stb_truetype.h"

#define array_count(arr) (sizeof(arr) / sizeof((arr)[0]))

unsigned int random(unsigned int *rng_seed)
{
	unsigned int x = *rng_seed;
	x ^= x << 6;
	x ^= x >> 21;
	x ^= x << 7;
	*rng_seed = x;

	return x;
}

struct StationLUT
{
	float r;
	float g;
	float b;

	int w;
	int h;
};

struct Station
{
	float r;
	float g;
	float b;

	int x;
	int y;
	int w;
	int h;
};

struct Font
{
	float  baseline_advance;
	GLuint texture;

	stbtt_packedchar char_data['~' - ' '];
};

const int BITMAP_W = 512;
const int BITMAP_H = 512;

unsigned char tmp_bitmap[BITMAP_W * BITMAP_H];
unsigned char ttf_buf[1 << 25];

Font load_font(const char *filename)
{
	Font result = {};

	FILE *file = fopen(filename, "rb");
	if(file)
	{
		fread(ttf_buf, sizeof(char), array_count(ttf_buf), file);

		stbtt_fontinfo font_info;
		if(stbtt_InitFont(&font_info, ttf_buf, stbtt_GetFontOffsetForIndex(ttf_buf, 0)))
		{
			int   font_pixel_size = 16;
			float font_scale      = stbtt_ScaleForPixelHeight(&font_info, font_pixel_size);

			int ascent, descent, line_gap;
			stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);

			result.baseline_advance = font_scale * (ascent - descent + line_gap);

			stbtt_pack_context pack_ctx;
			stbtt_PackBegin(&pack_ctx, tmp_bitmap, BITMAP_W, BITMAP_H, 0, 1, NULL);
			stbtt_PackSetOversampling(&pack_ctx, 2, 2);
			stbtt_PackFontRange(&pack_ctx, ttf_buf, 0, font_pixel_size, ' ', array_count(result.char_data), result.char_data);
			stbtt_PackEnd(&pack_ctx);

			glGenTextures(1, &result.texture);
			glBindTexture(GL_TEXTURE_2D, result.texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, BITMAP_W, BITMAP_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tmp_bitmap);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		fclose(file);
	}

	return result;
}

void draw_text(Font *font, float x, float y, float r, float g, float b, char *text)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, font->texture);

	glColor3f(r, g, b);

	glBegin(GL_QUADS);
	while(*text)
	{
		stbtt_aligned_quad quad;
		stbtt_GetPackedQuad(font->char_data, BITMAP_W, BITMAP_H, *text++ - ' ', &x, &y, &quad, false);

		glTexCoord2f(quad.s0, quad.t0); glVertex2f(quad.x0, quad.y0);
		glTexCoord2f(quad.s1, quad.t0); glVertex2f(quad.x1, quad.y0);
		glTexCoord2f(quad.s1, quad.t1); glVertex2f(quad.x1, quad.y1);
		glTexCoord2f(quad.s0, quad.t1); glVertex2f(quad.x0, quad.y1);
	}
	glEnd();
}

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

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	int client_w, client_h;
	win_get_client_dim(window, &client_w, &client_h);

	Font  font     = load_font("c:/windows/fonts/arial.ttf");
	float baseline = font.baseline_advance;

	LARGE_INTEGER large_rng_seed;
	QueryPerformanceCounter(&large_rng_seed);
	unsigned int rng_seed = large_rng_seed.LowPart;

	int map_w = 32;
	int map_h = 32;

	const int station_count = 4;

	Station    stations[station_count]    = {};
	StationLUT station_lut[station_count] = {
		{0, 0, 1, 8, 8},
		{0, 1, 0, 4, 4},
		{0, 1, 1, 2, 2},
		{1, 0, 0, 1, 1},
	};

	for(int station_idx = 0; station_idx < station_count; ++station_idx)
	{
		Station    *station = &stations[station_idx];
		StationLUT  lut     = station_lut[station_idx];

		float r = lut.r;
		float g = lut.g;
		float b = lut.b;
		int   w = lut.w;
		int   h = lut.h;

		station->r = r;
		station->g = g;
		station->b = b;
		station->x = random(&rng_seed) % (map_w - w);
		station->y = random(&rng_seed) % (map_h - h);
		station->w = w;
		station->h = h;
	}

	uint64_t elapsed_microsecs = 0;

	ShowWindow(window, SW_SHOW);

	LARGE_INTEGER prev_count;
	QueryPerformanceCounter(&prev_count);

	for(;;)
	{
		baseline = font.baseline_advance;

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

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, map_w, map_h, 0, 0, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glDisable(GL_TEXTURE_2D);

		glBegin(GL_QUADS);
		for(int station_idx = 0; station_idx < station_count; ++station_idx)
		{
			Station *station = &stations[station_idx];

			float r = station->r;
			float g = station->g;
			float b = station->b;

			int x = station->x;
			int y = station->y;
			int w = station->w;
			int h = station->h;

			glColor3f(r, g, b);

			glVertex2f(x,     y    );
			glVertex2f(x + w, y    );
			glVertex2f(x + w, y + h);
			glVertex2f(x,     y + h);
		}
		glEnd();

		glViewport(0, 0, client_w, client_h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, client_w, client_h, 0, 0, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		float dt = elapsed_microsecs / 1000000.0f;

		char text[256];
		stbsp_snprintf(text, sizeof(text), "%.4fs/f", dt);

		draw_text(&font, 0, baseline, 1, 1, 1, text);
		baseline += font.baseline_advance;

		SwapBuffers(device_ctx);

		LARGE_INTEGER curr_count;
		QueryPerformanceCounter(&curr_count);

		LARGE_INTEGER elapsed_count;
		elapsed_count.QuadPart = curr_count.QuadPart - prev_count.QuadPart;

		elapsed_microsecs = (elapsed_count.QuadPart * 1000000) / frequency.QuadPart;

		prev_count = curr_count;
	}

exit:
	wglDeleteContext(gl_ctx);
	ReleaseDC(window, device_ctx);
	DestroyWindow(window);
	return 0;
}
