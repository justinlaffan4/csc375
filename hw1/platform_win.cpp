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
#include "common.h"
#include "path_find.h"

#include "app.cpp"
#include "common.cpp"
#include "path_find.cpp"

// TODO: Find out what is causing the occasional flickering (happens sometimes on startup).
//       Probably something to do with OpenGL but don't know whether it is my fault or the AMD driver's fault.

InputState input;

bool work_queue_do_work(WorkQueue *queue, int thread_idx)
{
	bool result = false;

	for(int expected_front = queue->front; expected_front != queue->back; )
	{
		int new_front       = (queue->front + 1) % array_count(queue->entries);
		int exchanged_front = InterlockedCompareExchange(&queue->front, new_front, expected_front);
		if(exchanged_front == expected_front)
		{
			WorkQueueEntry *entry = &queue->entries[exchanged_front];
			entry->callback(entry->user_params, thread_idx);

			InterlockedDecrement(&queue->entry_count);
			result = true;
			break;
		}
	}

	return result;
}

void work_queue_push_work(WorkQueue *queue, WorkQueueCallback callback, void *user_params)
{
	if(queue->entry_count < array_count(queue->entries))
	{
		WorkQueueEntry *entry = &queue->entries[queue->back];
		entry->callback       = callback;
		entry->user_params    = user_params;

		queue->back = (queue->back + 1) % array_count(queue->entries);

		InterlockedIncrement(&queue->entry_count);

		LONG prev_count;
		ReleaseSemaphore(queue->semaphore, 1, &prev_count);
	}
}

void work_queue_work_until_done(WorkQueue *queue, int thread_idx)
{
	while(queue->entry_count > 0)
	{
		work_queue_do_work(queue, thread_idx);
	}
}

DWORD WINAPI thread_proc(LPVOID param)
{
	ThreadInfo *info = (ThreadInfo *)param;

	DWORD id = GetCurrentThreadId();

	for(;;)
	{
		if(!work_queue_do_work(info->queue, info->idx))
		{
			WaitForSingleObject(info->queue->semaphore, INFINITE); // Put thread to sleep and wait for signal to wake up
		}
	}

	return 0;
}


uint64_t vmem_page_size()
{
	uint64_t result = 4096;

#if 0
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	result = (u64)info.dwPageSize;

	assert(is_pow2(result));
#endif

	return result;
}

void *vmem_reserve(uint64_t size)
{
	void *result = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
	return result;
}

bool vmem_commit(void *base, uint64_t size)
{
	assert(is_aligned(size, vmem_page_size()));

	bool result = VirtualAlloc(base, size, MEM_COMMIT, PAGE_READWRITE) != 0;
	return result;
}

void vmem_decommit(void *base, uint64_t size)
{
	assert(is_aligned(size, vmem_page_size()));

	VirtualFree(base, size, MEM_DECOMMIT);
}

void vmem_release(void *base)
{
	VirtualFree(base, 0, MEM_RELEASE);
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
			
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
			{
				uint32_t vk_code = (uint32_t)w_param;

				bool key_was_down  = (l_param & (1 << 30)) != 0;
				bool key_down      = (l_param & (1 << 31)) == 0;
				bool key_repeating = key_was_down == key_down;

				input.keys[vk_code].down     = key_down;
				input.keys[vk_code].held     = key_down;
				input.keys[vk_code].pressed  = key_down && !key_was_down;
				input.keys[vk_code].released = !key_down && key_was_down;
			}break;

		default:
			result = DefWindowProc(window, msg, w_param, l_param);
	}

	return result;
}

int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd)
{
	ThreadInfo thread_infos[THREAD_COUNT - 1] = {};
	int thread_count = array_count(thread_infos);

	WorkQueue work_queue = {};

	work_queue.semaphore = CreateSemaphore(NULL, 0, thread_count, NULL);

	for(int thread_idx = 0; thread_idx < thread_count; ++thread_idx)
	{
		ThreadInfo *info = &thread_infos[thread_idx];
		info->idx        = thread_idx + 1;
		info->queue      = &work_queue;

		DWORD id;
		HANDLE handle = CreateThread(NULL, 0, thread_proc, info, 0, &id);
		CloseHandle(handle);
	}

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
	unsigned int rng_seed = large_rng_seed.QuadPart;

	Arena permanent_arena = arena_make();
	Arena transient_arena = arena_make();

	AppState app = app_make("c:/windows/fonts/arial.ttf", rng_seed, &permanent_arena);

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	ShowWindow(window, SW_SHOW);

	LARGE_INTEGER prev_count;
	QueryPerformanceCounter(&prev_count);

	for(;;)
	{
		mem_zero(transient_arena.base, transient_arena.curr_pos);
		transient_arena.curr_pos = 0;

		for(int i = 0; i < array_count(input.keys); ++i)
		{
			input.keys[i].held     = false;
			input.keys[i].pressed  = false;
			input.keys[i].released = false;
		}

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

		app_update(&app, &input, &work_queue, &transient_arena);

		SwapBuffers(device_ctx);

		LARGE_INTEGER curr_count;
		QueryPerformanceCounter(&curr_count);

		LARGE_INTEGER elapsed_count;
		elapsed_count.QuadPart = curr_count.QuadPart - prev_count.QuadPart;

		input.elapsed_microsecs = (elapsed_count.QuadPart * 1000000) / frequency.QuadPart;

		prev_count = curr_count;
	}

exit:
#if 0
	wglDeleteContext(gl_ctx);
	DeleteDC(device_ctx);
	DestroyWindow(window);
#endif
	return 0;
}
