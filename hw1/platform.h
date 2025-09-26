#pragma once

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
	bool released;
	bool pressed;
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

