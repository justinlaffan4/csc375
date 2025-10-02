#pragma once

#define work_queue_callback(name) void (name)(void *user_params, int thread_idx)
typedef work_queue_callback(*WorkQueueCallback);

struct WorkQueueEntry
{
	WorkQueueCallback  callback;
	void              *user_params;
};

// Single producer multiple consumer
struct WorkQueue
{
	HANDLE semaphore;

	volatile uint32_t front, back;
	volatile uint32_t entry_count;
	WorkQueueEntry    entries[256];
};

struct ThreadInfo
{
	int        idx;
	WorkQueue *queue;
};

void work_queue_push_work(WorkQueue *queue, WorkQueueCallback callback, void *user_params);
void work_queue_work_until_done(WorkQueue *queue, int thread_idx);

enum KeyMap
{
	KEY_LEFT    = VK_LEFT,
	KEY_RIGHT   = VK_RIGHT,
	KEY_UP      = VK_UP,
	KEY_DOWN    = VK_DOWN,
	KEY_SPACE   = VK_SPACE,
	KEY_CONTROL = VK_CONTROL,
	KEY_MENU    = VK_MENU,
	KEY_RETURN  = VK_RETURN,
	KEY_SHIFT   = VK_SHIFT,
	KEY_DELETE  = VK_DELETE,

	KEY_LBUTTON = VK_LBUTTON,
	KEY_MBUTTON = VK_MBUTTON,
	KEY_RBUTTON = VK_RBUTTON,

	KEY_F4 = VK_F4,
	KEY_F5 = VK_F5,
};

struct KeyState
{
	bool down;
	bool held;
	bool pressed;
	bool released;
};

struct InputState
{
	KeyState keys[256];

	int client_w;
	int client_h;

	uint64_t elapsed_microsecs;
};

uint64_t  vmem_page_size();
void     *vmem_reserve  (uint64_t size);
bool      vmem_commit   (void *base, uint64_t size);
void      vmem_decommit (void *base, uint64_t size);
void      vmem_release  (void *base);

