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
#define abs(x) ((x) > 0 ? (x) : -(x))

// TODO: Find out what is causing the occasional flickering (happens sometimes on startup).
//       Probably something to do with OpenGL but don't know whether it is my fault or the AMD driver's fault.

unsigned int random(unsigned int *rng_seed)
{
	unsigned int x = *rng_seed;
	x ^= x << 6;
	x ^= x >> 21;
	x ^= x << 7;
	*rng_seed = x;

	return x;
}

struct AStarNode
{
	int x;
	int y;

	int g; // Distance from start to node
	int h; // Best distance (ignoring occlusions) from node to target

	bool visited;
	bool search;

	AStarNode *next;
};

struct StationType
{
	float r;
	float g;
	float b;

	int w;
	int h;

	int door_offset_x;
	int door_offset_y;
};

struct Station
{
	int type_idx;

	int x;
	int y;
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

	glBegin(GL_QUADS);

	glColor3f(r, g, b);

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

void draw_rect(float x, float y, float w, float h, float r, float g, float b)
{
	glColor3f(r, g, b);

	glVertex2f(x,     y    );
	glVertex2f(x + w, y    );
	glVertex2f(x + w, y + h);
	glVertex2f(x,     y + h);
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

	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	int client_w, client_h;
	win_get_client_dim(window, &client_w, &client_h);

	Font  font     = load_font("c:/windows/fonts/arial.ttf");
	float baseline = font.baseline_advance;

	LARGE_INTEGER large_rng_seed;
	QueryPerformanceCounter(&large_rng_seed);
	unsigned int rng_seed = 69;//large_rng_seed.LowPart;

	const int map_w = 128;
	const int map_h = 128;
	int       map[map_h][map_w] = {};

	const int desired_station_count = 24;
	Station stations[desired_station_count] = {};

	const int station_type_count = 4;
	StationType station_types[station_type_count] = {
		{0, 0, 1, 8, 8,  4, -1},
		{0, 1, 0, 4, 4,  4,  2},
		{0, 1, 1, 2, 2,  1,  2},
		{1, 0, 0, 1, 1, -1,  0},
	};

	int station_count = 0;
	for(int station_idx = 0; station_idx < desired_station_count; ++station_idx)
	{
		int         type_idx = station_idx % station_type_count;
		StationType type     = station_types[type_idx];

		int w = type.w;
		int h = type.h;

		int bound_x = map_w - w - 2;
		int bound_y = map_h - h - 2;

		if(bound_x > 0 && bound_y > 0)
		{
			int x = 0;
			int y = 0;

			int max_tries = 128;
			int try_count = 0;
regenerate_facility:
			if(try_count++ < max_tries)
			{
				// Account for door placement
				x = (random(&rng_seed) % bound_x) + 1;
				y = (random(&rng_seed) % bound_y) + 1;

				// Check for overlap (accounting for door) and regenerate the position if overlap exists
				for(int map_x = x - 1; map_x < x + w + 1; ++map_x)
				{
					for(int map_y = y - 1; map_y < y + h + 1; ++map_y)
					{
						if(map[map_y][map_x] == 1)
						{
							goto regenerate_facility;
						}
					}
				}
			}

			if(try_count < max_tries)
			{
				for(int map_x = x; map_x < x + w; ++map_x)
				{
					for(int map_y = y; map_y < y + h; ++map_y)
					{
						map[map_y][map_x] = 1;
					}
				}

				Station *station  = &stations[station_count++];
				station->type_idx = type_idx;
				station->x        = x;
				station->y        = y;
			}
		}
	}

	if(station_count != desired_station_count)
	{
		//return 1;
	}

	uint64_t elapsed_microsecs = 0;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

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

		glClearColor(0.1f, 0.1f, 0.1f, 1);
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
		glOrtho(0, map_w, map_h, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glDisable(GL_TEXTURE_2D);

		glBegin(GL_QUADS);
		for(int station_a_idx = 0; station_a_idx < station_count; ++station_a_idx)
		{
			Station *station_a = &stations[station_a_idx];
			StationType type_a = station_types[station_a->type_idx];

			int start_x = station_a->x + type_a.door_offset_x;
			int start_y = station_a->y + type_a.door_offset_y;

			for(int station_b_idx = station_a_idx; station_b_idx < station_count; ++station_b_idx)
			{
				if(station_a_idx != station_b_idx)
				{
					Station *station_b = &stations[station_b_idx];
					StationType type_b = station_types[station_b->type_idx];

					int target_x = station_b->x + type_b.door_offset_x;
					int target_y = station_b->y + type_b.door_offset_y;

					const int max_a_star_nodes = map_w * map_h;

					AStarNode a_star_nodes[map_h][map_w] = {};
					a_star_nodes[start_y][start_x]       = {start_x, start_y};

					int        search_count             = 1;
					AStarNode *search[max_a_star_nodes] = {&a_star_nodes[start_y][start_x]};

					while(search_count > 0 && search_count < max_a_star_nodes)
					{
						int curr_idx = 0;
						for(int search_idx = 0; search_idx < search_count; ++search_idx)
						{
							AStarNode *n    = search[search_idx];
							AStarNode *curr = search[curr_idx];

							int n_f    = n->g + n->h;
							int curr_f = curr->g + curr->h;
							if(n_f < curr_f || n_f == curr_f && n->h < curr->h)
							{
								curr_idx = search_idx;
							}
						}

						AStarNode *curr = search[curr_idx];

						if(curr->x == target_x && curr->y == target_y)
						{
							for(; curr; curr = curr->next)
							{
								draw_rect(curr->x, curr->y, 1, 1, 1, 1, 0);
							}
							break;
						}

						curr->visited = true;
						curr->search  = false;

						search[curr_idx] = search[--search_count]; // Unordered removal

						int neighbor_offsets_x[] = {1, 0, -1,  0};
						int neighbor_offsets_y[] = {0, 1,  0, -1};

						for(int neighbor_idx = 0; neighbor_idx < 4; ++neighbor_idx)
						{
							int neighbor_x = curr->x + neighbor_offsets_x[neighbor_idx];
							int neighbor_y = curr->y + neighbor_offsets_y[neighbor_idx];

							if(neighbor_x >= 0 && neighbor_x < map_w && neighbor_y >= 0 && neighbor_y < map_h && map[neighbor_y][neighbor_x] == 0)
							{
								AStarNode *neighbor = &a_star_nodes[neighbor_y][neighbor_x];

								if(!neighbor->visited)
								{
									if(!neighbor->search)
									{
										neighbor->x      = neighbor_x;
										neighbor->y      = neighbor_y;
										neighbor->g      = INT_MAX;
										neighbor->search = true;

										int manhattan_x        = abs(target_x - neighbor_x);
										int manhattan_y        = abs(target_y - neighbor_y);
										int manhattan_distance = manhattan_x + manhattan_y;

										neighbor->h = manhattan_distance;

										search[search_count++] = neighbor;
									}

									if(neighbor->search)
									{
										int g = curr->g + 1;
										if(g < neighbor->g)
										{
											neighbor->g    = g;
											neighbor->next = curr;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		for(int station_idx = 0; station_idx < station_count; ++station_idx)
		{
			Station *station = &stations[station_idx];
			StationType type = station_types[station->type_idx];

			float r = type.r;
			float g = type.g;
			float b = type.b;

			int x = station->x;
			int y = station->y;
			int w = type.w;
			int h = type.h;

			draw_rect(x, y, w, h, r, g, b);

			int door_x = x + type.door_offset_x;
			int door_y = y + type.door_offset_y;
			int door_w = 1;
			int door_h = 1;

			draw_rect(door_x, door_y, door_w, door_h, 1, 0, 1);
		}
		glEnd();

		glViewport(0, 0, client_w, client_h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, client_w, client_h, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		float dt = elapsed_microsecs / 1000000.0f;

		char text[256];

		stbsp_snprintf(text, sizeof(text), "%.4fs/f", dt);
		draw_text(&font, 0, baseline, 1, 1, 1, text);
		baseline += font.baseline_advance;

		stbsp_snprintf(text, sizeof(text), "%.0ff/s", 1 / dt);
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
	DeleteDC(device_ctx);
	DestroyWindow(window);
	return 0;
}
