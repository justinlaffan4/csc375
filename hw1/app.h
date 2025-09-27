#pragma once

#include "platform.h"

#define array_count(arr) (sizeof(arr) / sizeof((arr)[0]))
#define abs(x) ((x) > 0 ? (x) : -(x))

#define kilobytes(n) ((n) * 1024llu)
#define megabytes(n) ((n) * 1024llu * 1024llu)
#define gigabytes(n) ((n) * 1024llu * 1024llu * 1024llu)

#define mem_zero(mem, size)                            memset((mem), 0, (size))
#define mem_copy_array(dst, dst_count, src, src_count) memcpy_s(dst, dst_count * sizeof(*dst), src, src_count * sizeof(*src))
#define mem_zero_array(mem, count)                     mem_zero(mem, count * sizeof(*mem))

#define swap(a, b) do { static_assert(sizeof(a) == sizeof(b)); unsigned char swap_tmp[sizeof(a)]; memcpy(swap_tmp, &a, sizeof(a)); memcpy(&a, &b, sizeof(a)); memcpy(&b, swap_tmp, sizeof(a)); } while(false)

// Only works for powers of 2
#define align_up_mask(x, m) (((x) + (m)) & ~(m)) 
#define align_up(x, a)      align_up_mask((x), (a) - 1)

#define align_down_mask(x, m) ((x) & ~(m)) 
#define align_down(x, a)      align_down_mask((x), (a) - 1)

#define is_aligned_mask(x, m) (((x) & (m)) == 0)
#define is_aligned(x, a)      is_aligned_mask((x), (a) - 1)

const int STATION_TYPE_COUNT = 4;
const int DESIRED_STATION_COUNT = 48;

struct Font
{
	float  baseline_advance;
	GLuint texture;

	stbtt_packedchar char_data['~' - ' '];
};

struct AStarNode
{
	int x;
	int y;

	int g; // Distance from start to node
	int h; // Best distance (ignoring occlusions) from node to target

	bool opened;
	bool closed;

	int heap_idx;

	AStarNode *parent;
};

struct AStarBinaryHeap
{
	int         node_capacity;
	int         node_count;
	AStarNode **nodes;
};

struct AStarPathTile
{
	int x;
	int y;
};

struct AStarPath
{
	int            tile_count;
	AStarPathTile *tiles;
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

struct AppState
{
	Font  font;
	float baseline;

	unsigned int rng_seed;

	StationType station_types[STATION_TYPE_COUNT];

	int     station_count;
	Station stations[DESIRED_STATION_COUNT];

#if 1
	int step_count;
	int station_b_idx;
#endif
};

struct Arena
{
	uint64_t  curr_pos;
	uint64_t  commit_pos;
	uint64_t  capacity;
	void     *base;
};

struct TmpArena
{
	uint64_t  restore;
	Arena    *arena;
};

struct ScratchArenas
{
	Arena arenas[4];
};

void  arena_init   (Arena *arena, uint64_t reserve_size = gigabytes(1));
void  arena_free   (Arena *arena);
void *arena_get_top(Arena *arena);
void *arena_push   (Arena *arena, uint64_t size, uint64_t allignment = sizeof(void *));
void  arena_pop_to (Arena *arena, uint64_t pos);

#define arena_push_array(arena, count, type) (type *)arena_push(arena, count * sizeof(type))

TmpArena tmp_arena_begin(Arena *arena);
void     tmp_arena_end  (TmpArena tmp);

ScratchArenas scratch_arenas_init();

Arena *arena_get_scratch(Arena **conflicts, int count);

TmpArena arena_begin_scratch(Arena **conflicts, int count);
void     arena_end_scratch  (TmpArena scratch);

unsigned int random(unsigned int *rng_seed);

Font load_font(const char *filename);

void draw_text(Font *font, float x, float y, float r, float g, float b, char *text);
void draw_rect(float x, float y, float w, float h, float r, float g, float b);

int a_star_node_compare_f_score(AStarNode *a, AStarNode *b);

AStarPath a_star_path_find_new(int start_x, int start_y, int target_x, int target_y, int step_count, Arena *arena);
AStarPath a_star_path_find_old(int start_x, int start_y, int target_x, int target_y, int step_count, Arena *arena);

AppState app_make  (const char *font_filename, unsigned int rng_seed);
void     app_update(AppState *app, InputState *input);
