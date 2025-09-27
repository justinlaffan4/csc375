#include "app.h"

const int MAP_W = 32;
const int MAP_H = 32;

const int BITMAP_W = 512;
const int BITMAP_H = 512;

int map[MAP_H][MAP_W];

void arena_init(Arena *arena, uint64_t reserve_size)
{
	mem_zero(arena, sizeof(*arena));

	arena->capacity = reserve_size;
	arena->base     = vmem_reserve(arena->capacity);
	assert(arena->base);
}

void arena_free(Arena *arena)
{
	vmem_decommit(arena->base, arena->commit_pos);
	vmem_release (arena->base);
}

void *arena_get_top(Arena *arena)
{
	void *result = (uint8_t *)arena->base + arena->curr_pos;
	return result;
}

void *arena_push(Arena *arena, uint64_t size, uint64_t allignment)
{
	void *result = NULL;

	size = align_up(size, allignment);

	uint64_t new_pos = arena->curr_pos + size;
	if(new_pos <= arena->capacity)
	{
		if(new_pos > arena->commit_pos)
		{
			uint64_t commit_size = align_up(new_pos - arena->commit_pos, vmem_page_size());
			if(vmem_commit((uint8_t *)arena->base + arena->commit_pos, commit_size))
			{
				arena->commit_pos += commit_size;
				result             = arena_get_top(arena);
				arena->curr_pos    = new_pos;
			}
		}else
		{
			result          = arena_get_top(arena);
			arena->curr_pos = new_pos;
		}
	}

	assert(result);

	mem_zero(result, size);

	return result;
}

void arena_pop_to(Arena *arena, uint64_t pos)
{
	if(pos < arena->curr_pos)
	{
		arena->curr_pos = pos;
#if 0
		uint64_t page_size    = vmem_page_size();
		uint64_t alligned_pos = align_up(arena->curr_pos, page_size);
		if(alligned_pos < arena->commit_pos)
		{
			uint64_t decommit_size = arena->commit_pos - alligned_pos;
			vmem_decommit((uint8_t *)arena->base + alligned_pos, decommit_size);
			arena->commit_pos -= decommit_size;
		}
#endif
	}
}

TmpArena tmp_arena_begin(Arena *arena)
{
	TmpArena result = {};
	result.arena    = arena;
	result.restore  = arena->curr_pos;
	
	return result;
}

void tmp_arena_end(TmpArena tmp)
{
	arena_pop_to(tmp.arena, tmp.restore);
}

ScratchArenas scratch_arenas_init()
{
	ScratchArenas result = {};
	for(int i = 0; i < array_count(result.arenas); ++i)
	{
		arena_init(&result.arenas[i]);
	}
	return result;
}

Arena *arena_get_scratch(Arena **conflicts, int count)
{
	Arena *result = NULL;

	thread_local ScratchArenas scratch = scratch_arenas_init();
	for(int arena_idx = 0; arena_idx < array_count(scratch.arenas); ++arena_idx)
	{
		Arena *arena = &scratch.arenas[arena_idx];

		bool is_conflict = false;
		for(int conflict_idx = 0; conflict_idx < count; ++conflict_idx)
		{
			Arena *conflict = conflicts[conflict_idx];
			if(conflict == arena)
			{
				is_conflict = true;
				break;
			}
		}

		if(!is_conflict)
		{
			result = arena;
			break;
		}
	}

	return result;
}

TmpArena arena_begin_scratch(Arena **conflicts, int count)
{
	Arena   *scratch = arena_get_scratch(conflicts, count);
	TmpArena result  = tmp_arena_begin(scratch);

	return result;
}

void arena_end_scratch(TmpArena scratch)
{
	tmp_arena_end(scratch);
}

unsigned int random(unsigned int *rng_seed)
{
	unsigned int x = *rng_seed;
	x ^= x << 6;
	x ^= x >> 21;
	x ^= x << 7;
	*rng_seed = x;

	return x;
}

Font load_font(const char *filename)
{
	Font result = {};

	FILE *file = fopen(filename, "rb");
	if(file)
	{
		TmpArena scratch = arena_begin_scratch(NULL, 0);

		unsigned char *tmp_bitmap = arena_push_array(scratch.arena, BITMAP_W * BITMAP_H, unsigned char);
		unsigned char *ttf_buf    = arena_push_array(scratch.arena, 1 << 25, unsigned char);

		fread(ttf_buf, 1, 1 << 25, file);

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

		arena_end_scratch(scratch);
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

int a_star_node_cmp(AStarNode *a, AStarNode *b)
{
#if 0
	// Greedy A* path finding (not optimal path but potentially faster)
	int a_f = a->h;
	int b_f = b->h;
#else
	int a_f = a->g + a->h;
	int b_f = b->g + b->h;
#endif

	int result = 0;
	if(a_f == b_f)
	{
		result = a->h - b->h;
	}else
	{
		result = a_f - b_f;
	}

	return result;
}

int get_parent_idx(int idx)
{
	int result = (idx - 1) / 2;
	return result;
}

int get_l_child_idx(int idx)
{
	int result = (2 * idx + 1);
	return result;
}

int get_r_child_idx(int idx)
{
	int result = (2 * idx + 2);
	return result;
}

AStarBinaryHeap a_star_heap_make(int capacity, Arena *arena)
{
	AStarBinaryHeap result = {};
	result.node_capacity   = capacity;
	result.nodes           = arena_push_array(arena, capacity, AStarNode *);

	return result;
}

void a_star_heap_swap(AStarBinaryHeap *heap, int a, int b)
{
	swap(heap->nodes[a], heap->nodes[b]);

	heap->nodes[a]->heap_idx = a;
	heap->nodes[b]->heap_idx = b;
}

void a_star_heap_heapify_up(AStarBinaryHeap *heap, int idx)
{
	while(idx > 0 && a_star_node_cmp(heap->nodes[get_parent_idx(idx)], heap->nodes[idx]) > 0)
	{
		a_star_heap_swap(heap, get_parent_idx(idx), idx);
		idx = get_parent_idx(idx);
	}
}

void a_star_heap_insert(AStarBinaryHeap *heap, AStarNode *node)
{
	if(heap->node_count < heap->node_capacity)
	{
		node->heap_idx = heap->node_count++;

		// Insert at bottom of tree
		heap->nodes[node->heap_idx] = node;
		a_star_heap_heapify_up(heap, node->heap_idx);
	}
}

void a_star_heap_remove_min(AStarBinaryHeap *heap)
{
	int last_idx = heap->node_count - 1;
	int curr_idx = 0;
	a_star_heap_swap(heap, last_idx, curr_idx);
	--heap->node_count;

heapify_down:
	int l_idx = get_l_child_idx(curr_idx);
	int r_idx = get_r_child_idx(curr_idx);

	int smallest_idx = curr_idx;

	if(l_idx < heap->node_count && a_star_node_cmp(heap->nodes[l_idx], heap->nodes[smallest_idx]) < 0)
	{
		smallest_idx = l_idx;
	}

	if(r_idx < heap->node_count && a_star_node_cmp(heap->nodes[r_idx], heap->nodes[smallest_idx]) < 0)
	{
		smallest_idx = r_idx;
	}

	if(smallest_idx != curr_idx)
	{
		a_star_heap_swap(heap, smallest_idx, curr_idx);

		curr_idx = smallest_idx;
		goto heapify_down;
	}
}

void a_star_node_init(AStarNode *node, int x, int y, int target_x, int target_y, int g, AStarNode *parent)
{
	node->x      = x;
	node->y      = y;
	node->g      = g;
	node->opened = true;
	node->parent = parent;

	int manhattan_x        = abs(target_x - x);
	int manhattan_y        = abs(target_y - y);
	int manhattan_distance = manhattan_x + manhattan_y;

	node->h = manhattan_distance;
}

struct AStarNodeNeighbors
{
	int        count;
	AStarNode *items[4];
};

bool map_tile_walkable(int x, int y)
{
	bool result = x >= 0 && x < MAP_W && y >= 0 && y < MAP_H && map[y][x] == 0;
	return result;
}

void a_star_node_push_neighbor(AStarNodeNeighbors *neighbors, AStarNode *a_star_node_map, int x, int y)
{
	if(map_tile_walkable(x, y))
	{
		// TODO: We don't need to do this, we can just return the x and y points instead
		AStarNode *neighbor = &a_star_node_map[y * MAP_W + x];
		neighbor->x         = x;
		neighbor->y         = y;

		neighbors->items[neighbors->count++] = neighbor;
	}
}

void jps_push_jump_point(AStarNode *a_star_node_map, AStarBinaryHeap *open_list, int x, int y, int target_x, int target_y, AStarNode *parent, int distance_from_parent)
{
	AStarNode *jump_point = &a_star_node_map[y * MAP_W + x];

	if(!jump_point->closed)
	{
		int g = parent->g + distance_from_parent;

		if(!jump_point->opened)
		{
			a_star_node_init(jump_point, x, y, target_x, target_y, g, parent);
			a_star_heap_insert(open_list, jump_point);
		}else if(g < jump_point->g)
		{
			jump_point->g      = g;
			jump_point->parent = parent;

			a_star_heap_heapify_up(open_list, jump_point->heap_idx);
		}
	}
}

void jps_traverse_pos_x(AStarNode *a_star_node_map, AStarBinaryHeap *open_list, int start_x, int start_y, int target_x, int target_y, AStarNode *parent)
{
	for(int x = start_x; x < MAP_W; ++x)
	{
		draw_rect(x, start_y, 1, 1, 0, 1, 1);

		bool false_neighbor0 = map_tile_walkable(x, start_y + 1) && !map_tile_walkable(x - 1, start_y + 1);
		bool false_neighbor1 = map_tile_walkable(x, start_y - 1) && !map_tile_walkable(x - 1, start_y - 1);

		if(false_neighbor0 || false_neighbor1)
		{
			jps_push_jump_point(a_star_node_map, open_list, x, start_y, target_x, target_y, parent, x - start_x);
			break;
		}
	}
}

void jps_traverse_neg_x(AStarNode *a_star_node_map, AStarBinaryHeap *open_list, int start_x, int start_y, int target_x, int target_y, AStarNode *parent)
{
	for(int x = start_x; x >= 0; --x)
	{
		draw_rect(x, start_y, 1, 1, 0.5f, 1, 0.5f);

		bool false_neighbor0 = map_tile_walkable(x, start_y + 1) && !map_tile_walkable(x + 1, start_y + 1);
		bool false_neighbor1 = map_tile_walkable(x, start_y - 1) && !map_tile_walkable(x + 1, start_y - 1);

		if(false_neighbor0 || false_neighbor1)
		{
			jps_push_jump_point(a_star_node_map, open_list, x, start_y, target_x, target_y, parent, start_x - x);
			break;
		}
	}
}

AStarPath jps_path_find(int start_x, int start_y, int target_x, int target_y, int max_step_count, Arena *arena)
{
	AStarPath result = {};

	Arena *conflicts[] = {arena};
	TmpArena scratch   = arena_begin_scratch(conflicts, array_count(conflicts));

	AStarNode *a_star_node_map = arena_push_array(scratch.arena, MAP_W * MAP_H, AStarNode);

	AStarNode *start_node = &a_star_node_map[start_y * MAP_W + start_x];
	a_star_node_init(start_node, start_x, start_y, target_x, target_y, 0, NULL);

	AStarBinaryHeap open_list = a_star_heap_make(MAP_W * MAP_H, scratch.arena);
	a_star_heap_insert(&open_list, start_node);

	int step_count = 0;
	while(open_list.node_count > 0)
	{
		AStarNode *curr = open_list.nodes[0];

		if(step_count++ >= max_step_count || (curr->x == target_x && curr->y == target_y))
		{
			for(AStarNode *n = curr; n; n = n->parent)
			{
				result.tile_count++;
			}

			result.tiles = arena_push_array(arena, result.tile_count, AStarPathTile);

			int tile_idx = 0;
			for(AStarNode *n = curr; n; n = n->parent)
			{
				AStarPathTile *tile = &result.tiles[tile_idx++];
				tile->x             = n->x;
				tile->y             = n->y;
			}

			break;
		}

		a_star_heap_remove_min(&open_list);
		curr->closed = true;

		int neighbor_offsets_x[] = {1, 0, -1,  0};
		int neighbor_offsets_y[] = {0, 1,  0, -1};

		AStarNodeNeighbors neighbors = {};
		if(curr->parent)
		{
			int dx = curr->parent->x - curr->x;
			int dy = curr->parent->y - curr->y;

			// Normalize
			dx = dx / max(abs(dx), 1);
			dy = dy / max(abs(dy), 1);

			if(dx != 0)
			{
				a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x + dx, curr->y    );
				a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x,      curr->y + 1);
				a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x,      curr->y - 1);
			}else if(dy != 0)
			{
				a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x,     curr->y + dy);
				a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x + 1, curr->y     );
				a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x - 1, curr->y     );
			}
		}else
		{
			// Must be starting node therefore all neighbors are set
			a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x + 1, curr->y);
			a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x - 1, curr->y);
			a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x, curr->y + 1);
			a_star_node_push_neighbor(&neighbors, a_star_node_map, curr->x, curr->y - 1);
		}

		for(int neighbor_idx = 0; neighbor_idx < neighbors.count; ++neighbor_idx)
		{
			AStarNode *neighbor = neighbors.items[neighbor_idx];

			int from_x = curr->x;
			int from_y = curr->y;
			int to_x   = neighbor->x;
			int to_y   = neighbor->y;

			int dx = to_x - from_x;
			int dy = to_y - from_y;

			if(to_x == target_x && to_y == target_y)
			{
				int distance_from_curr = 0;
				if(dx != 0)
				{
					distance_from_curr = abs(dx);
				}else if(dy != 0)
				{
					distance_from_curr = abs(dy);
				}

				jps_push_jump_point(a_star_node_map, &open_list, to_x, to_y, target_x, target_y, curr, distance_from_curr);
			}else
			{
				if(dx > 0)
				{
					jps_traverse_pos_x(a_star_node_map, &open_list, to_x, to_y, target_x, target_y, curr);
				}else if(dx < 0)
				{
					jps_traverse_neg_x(a_star_node_map, &open_list, to_x, to_y, target_x, target_y, curr);
				}

				if(dy > 0)
				{
					for(int y = to_y; y < MAP_H; ++y)
					{
						draw_rect(to_x, y, 1, 1, 1.0f, 0.0f, 0.5f);

						bool false_neighbor0 = map_tile_walkable(to_x + 1, y) && !map_tile_walkable(from_x + 1, y + 1);
						bool false_neighbor1 = map_tile_walkable(to_x - 1, y) && !map_tile_walkable(from_x - 1, y + 1);

						if(false_neighbor0 || false_neighbor1)
						{
							jps_push_jump_point(a_star_node_map, &open_list, to_x, y, target_x, target_y, curr, y - to_y);
							break;
						}else
						{
							jps_traverse_pos_x(a_star_node_map, &open_list, to_x + 1, y, target_x, target_y, curr);
							jps_traverse_neg_x(a_star_node_map, &open_list, to_x - 1, y, target_x, target_y, curr);
						}
					}
				}else if(dy < 0)
				{
					for(int y = to_y; y >= 0; --y)
					{
						draw_rect(to_x, y, 1, 1, 1.0f, 0.5f, 0.5f);

						bool false_neighbor0 = map_tile_walkable(to_x + 1, y) && !map_tile_walkable(from_x + 1, y - 1);
						bool false_neighbor1 = map_tile_walkable(to_x - 1, y) && !map_tile_walkable(from_x - 1, y - 1);

						if(false_neighbor0 || false_neighbor1)
						{
							jps_push_jump_point(a_star_node_map, &open_list, to_x, y, target_x, target_y, curr, to_y - y);
							break;
						}else
						{
							jps_traverse_pos_x(a_star_node_map, &open_list, to_x + 1, y, target_x, target_y, curr);
							jps_traverse_neg_x(a_star_node_map, &open_list, to_x - 1, y, target_x, target_y, curr);
						}
					}
				}
			}
		}
	}

	arena_end_scratch(scratch);
	
	return result;
}

AStarPath a_star_path_find_new(int start_x, int start_y, int target_x, int target_y, int max_step_count, Arena *arena)
{
	AStarPath result = {};

	Arena *conflicts[] = {arena};
	TmpArena scratch   = arena_begin_scratch(conflicts, array_count(conflicts));

	AStarNode *a_star_node_map = arena_push_array(scratch.arena, MAP_W * MAP_H, AStarNode);

	AStarNode *start_node = &a_star_node_map[start_y * MAP_W + start_x];
	a_star_node_init(start_node, start_x, start_y, target_x, target_y, 0, NULL);

	AStarBinaryHeap open_list = a_star_heap_make(MAP_W * MAP_H, scratch.arena);
	a_star_heap_insert(&open_list, start_node);

	int step_count = 0;
	while(open_list.node_count > 0)
	{
		AStarNode *curr = open_list.nodes[0];

		if(step_count++ >= max_step_count || (curr->x == target_x && curr->y == target_y))
		{
			for(AStarNode *n = curr; n; n = n->parent)
			{
				result.tile_count++;
			}

			result.tiles = arena_push_array(arena, result.tile_count, AStarPathTile);

			int tile_idx = 0;
			for(AStarNode *n = curr; n; n = n->parent)
			{
				AStarPathTile *tile = &result.tiles[tile_idx++];
				tile->x             = n->x;
				tile->y             = n->y;
			}

			break;
		}

		a_star_heap_remove_min(&open_list);
		curr->closed = true;

		int neighbor_offsets_x[] = {1, 0, -1,  0};
		int neighbor_offsets_y[] = {0, 1,  0, -1};

		for(int neighbor_idx = 0; neighbor_idx < 4; ++neighbor_idx)
		{
			int neighbor_x = curr->x + neighbor_offsets_x[neighbor_idx];
			int neighbor_y = curr->y + neighbor_offsets_y[neighbor_idx];

			if(neighbor_x >= 0 && neighbor_x < MAP_W && neighbor_y >= 0 && neighbor_y < MAP_H && map[neighbor_y][neighbor_x] == 0)
			{
				AStarNode *neighbor = &a_star_node_map[neighbor_y * MAP_W + neighbor_x];

				if(!neighbor->closed)
				{
					int g = curr->g + 1;

					if(!neighbor->opened)
					{
						a_star_node_init(neighbor, neighbor_x, neighbor_y, target_x, target_y, g, curr);
						a_star_heap_insert(&open_list, neighbor);
					}else if(g < neighbor->g)
					{
						neighbor->g            = g;
						neighbor->parent = curr;

						a_star_heap_heapify_up(&open_list, neighbor->heap_idx);
					}
				}
			}
		}
	}

	arena_end_scratch(scratch);
	
	return result;
}

AStarPath a_star_path_find_old(int start_x, int start_y, int target_x, int target_y, int step_count, Arena *arena)
{
	AStarPath result = {};

	Arena *conflicts[] = {arena};
	TmpArena scratch   = arena_begin_scratch(conflicts, array_count(conflicts));

	static AStarNode a_star_node_map[MAP_H][MAP_W];
	memset(a_star_node_map, 0, MAP_W * MAP_H * sizeof(AStarNode));

	a_star_node_map[start_y][start_x] = {start_x, start_y};

	int         search_count = 0;
#if 1
	AStarNode **to_search    = arena_push_array(scratch.arena, MAP_W * MAP_H, AStarNode *);
#else
	AStarNode *to_search[MAP_H * MAP_W] = {};
#endif

	to_search[search_count++] = &a_star_node_map[start_y][start_x];

	int steps = 0;
	while(to_search)
	{
		int curr_idx = 0;
		for(int to_search_idx = 0; to_search_idx < search_count; ++to_search_idx)
		{
			AStarNode *n    = to_search[to_search_idx];
			AStarNode *curr = to_search[curr_idx];

			int n_f    = n->g + n->h;
			int curr_f = curr->g + curr->h;
			if(n_f < curr_f || n_f == curr_f && n->h < curr->h)
			{
				curr_idx = to_search_idx;
			}
		}

		AStarNode *curr = to_search[curr_idx];

		if(steps++ >= step_count || (curr->x == target_x && curr->y == target_y))
		{
			for(AStarNode *n = curr; n; n = n->parent)
			{
				result.tile_count++;
			}

			result.tiles = arena_push_array(arena, result.tile_count, AStarPathTile);

			int tile_idx = 0;
			for(AStarNode *n = curr; n; n = n->parent)
			{
				AStarPathTile *tile = &result.tiles[tile_idx++];
				tile->x             = n->x;
				tile->y             = n->y;
			}

			break;
		}

		to_search[curr_idx] = to_search[--search_count]; // Unordered removal
		curr->closed = true;

		int neighbor_offsets_x[] = {1, 0, -1,  0};
		int neighbor_offsets_y[] = {0, 1,  0, -1};

		for(int neighbor_idx = 0; neighbor_idx < 4; ++neighbor_idx)
		{
			int neighbor_x = curr->x + neighbor_offsets_x[neighbor_idx];
			int neighbor_y = curr->y + neighbor_offsets_y[neighbor_idx];

			if(neighbor_x >= 0 && neighbor_x < MAP_W && neighbor_y >= 0 && neighbor_y < MAP_H && map[neighbor_y][neighbor_x] == 0)
			{
				AStarNode *neighbor = &a_star_node_map[neighbor_y][neighbor_x];

				if(!neighbor->closed)
				{
					int g = curr->g + 1;

					if(!neighbor->opened)
					{
						neighbor->x            = neighbor_x;
						neighbor->y            = neighbor_y;
						neighbor->g            = g;
						neighbor->opened       = true;
						neighbor->parent = curr;

						int manhattan_x        = abs(target_x - neighbor_x);
						int manhattan_y        = abs(target_y - neighbor_y);
						int manhattan_distance = manhattan_x + manhattan_y;

						neighbor->h = manhattan_distance;

						to_search[search_count++] = neighbor;
					}else if(g < neighbor->g)
					{
						neighbor->g            = g;
						neighbor->parent = curr;
					}
				}
			}
		}
	}

	arena_end_scratch(scratch);
	
	return result;
}

AppState app_make(const char *font_filename, unsigned int rng_seed)
{
	AppState result = {};
	result.font     = load_font(font_filename);
	result.baseline = result.font.baseline_advance;

	result.rng_seed = rng_seed;

	result.station_types[0] = {0, 0, 1, 8, 8,  4, -1};
	result.station_types[1] = {0, 1, 0, 4, 4,  4,  2};
	result.station_types[2] = {0, 1, 1, 2, 2,  1,  2};
	result.station_types[3] = {1, 0, 0, 1, 1, -1,  0};

	for(int station_idx = 0; station_idx < DESIRED_STATION_COUNT; ++station_idx)
	{
		int         type_idx = station_idx % STATION_TYPE_COUNT;
		StationType type     = result.station_types[type_idx];

		int w = type.w;
		int h = type.h;

		int bound_x = MAP_W - w - 2;
		int bound_y = MAP_H - h - 2;

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
				x = (random(&result.rng_seed) % bound_x) + 1;
				y = (random(&result.rng_seed) % bound_y) + 1;

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

				Station *station  = &result.stations[result.station_count++];
				station->type_idx = type_idx;
				station->x        = x;
				station->y        = y;
			}
		}
	}

	if(result.station_count != DESIRED_STATION_COUNT)
	{
	}

	return result;
}

void app_update(AppState *app, InputState *input)
{
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	glClearColor(0.1f, 0.1f, 0.1f, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int client_w = input->client_w;
	int client_h = input->client_h;

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
	glOrtho(0, MAP_W, MAP_H, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

#if 1
	if(input->keys[KEY_LEFT].pressed)
	{
		app->step_count = max(app->step_count - 1, 0);
	}
	if(input->keys[KEY_RIGHT].pressed)
	{
		app->step_count = min(app->step_count + 1, 1024);
	}

	if(input->keys[KEY_UP].pressed)
	{
		app->station_b_idx = min(app->station_b_idx + 1, app->station_count - 1);
	}
	if(input->keys[KEY_DOWN].pressed)
	{
		app->station_b_idx = max(app->station_b_idx - 1, 0);
	}
#endif

	for(int station_a_idx = 0; station_a_idx < app->station_count; ++station_a_idx)
	{
		Station *station_a = &app->stations[station_a_idx];
		StationType type_a = app->station_types[station_a->type_idx];

		int start_x = station_a->x + type_a.door_offset_x;
		int start_y = station_a->y + type_a.door_offset_y;

		//for(int station_b_idx = station_a_idx + 1; station_b_idx < app->station_count; ++station_b_idx)
		{
			Station *station_b = &app->stations[app->station_b_idx];
			StationType type_b = app->station_types[station_b->type_idx];

			int target_x = station_b->x + type_b.door_offset_x;
			int target_y = station_b->y + type_b.door_offset_y;

			TmpArena scratch = arena_begin_scratch(NULL, 0);

			//AStarPath old_path = a_star_path_find_old(start_x, start_y, target_x, target_y, 2048, scratch.arena);
			AStarPath new_path = jps_path_find(target_x, target_y, start_x, start_y, app->step_count, scratch.arena);
#if 0
			int tile_count = 0;
			while(old_path && new_path)
			{
				if(old_path->x != new_path->x || old_path->y != new_path->y)
				{
					int breakpoint = 0;
				}

				old_path = old_path->next_in_path;
				new_path = new_path->next_in_path;

				tile_count++;
			}
#else
#if 1
			for(int tile_idx = 0; tile_idx < new_path.tile_count; ++tile_idx)
			{
				AStarPathTile *tile = &new_path.tiles[tile_idx];
				draw_rect(tile->x, tile->y, 1, 1, 1, 1, 1);
			}
#else
			for(int tile_idx = 0; tile_idx < old_path.tile_count; ++tile_idx)
			{
				AStarPathTile *tile = &old_path.tiles[tile_idx];
				draw_rect(tile->x, tile->y, 1, 1, 1, 1, 0);
			}
#endif
#endif
			arena_end_scratch(scratch);
		}
	}

	for(int station_idx = 0; station_idx < app->station_count; ++station_idx)
	{
		Station *station = &app->stations[station_idx];
		StationType type = app->station_types[station->type_idx];

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

	float dt = input->elapsed_microsecs / 1000000.0f;

	char text[256];

	app->baseline = app->font.baseline_advance;

	stbsp_snprintf(text, sizeof(text), "%.4fs/f", dt);
	draw_text(&app->font, 0, app->baseline, 1, 1, 1, text);
	app->baseline += app->font.baseline_advance;

	stbsp_snprintf(text, sizeof(text), "%.0ff/s", 1 / dt);
	draw_text(&app->font, 0, app->baseline, 1, 1, 1, text);
	app->baseline += app->font.baseline_advance;

#if 0
	stbsp_snprintf(text, sizeof(text), "Step Count: %d, Station B Index: %d", app->step_count, app->station_b_idx);
	draw_text(&app->font, 0, app->baseline, 1, 1, 1, text);
	app->baseline += app->font.baseline_advance;
#endif
}

