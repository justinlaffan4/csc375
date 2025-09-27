#include "path_find.h"

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

AStarPath a_star_path_find(int start_x, int start_y, int target_x, int target_y, int max_step_count, Arena *arena)
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

		if((step_count++ >= max_step_count) || (curr->x == target_x && curr->y == target_y))
		{
			for(AStarNode *n = curr; n; n = n->parent)
			{
				result.tile_count++;
			}

			result.tiles = arena_push_array(arena, result.tile_count, AStarPathTile);

			int tile_idx = result.tile_count - 1;
			for(AStarNode *n = curr; n; n = n->parent)
			{
				AStarPathTile *tile = &result.tiles[tile_idx--];
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
						neighbor->g      = g;
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

void a_star_push_path(AStarPaths *paths, AStarNode *node, Arena *arena)
{
	AStarPath *path = &paths->paths[paths->count++];

	for(AStarNode *n = node; n; n = n->parent)
	{
		path->tile_count++;
	}

	path->tiles = arena_push_array(arena, path->tile_count, AStarPathTile);

	int tile_idx = path->tile_count - 1;
	for(AStarNode *n = node; n; n = n->parent)
	{
		AStarPathTile *tile = &path->tiles[tile_idx--];
		tile->x             = n->x;
		tile->y             = n->y;
	}
}

AStarPaths a_star_path_find_targets(int start_x, int start_y, AStarPathTile *targets, int target_count, int max_step_count, Arena *arena)
{
	AStarPaths result = {};
	result.paths      = arena_push_array(arena, target_count, AStarPath);

	Arena *conflicts[] = {arena};
	TmpArena scratch   = arena_begin_scratch(conflicts, array_count(conflicts));

	AStarNode *a_star_node_map = arena_push_array(scratch.arena, MAP_W * MAP_H, AStarNode);
	AStarNode *start_node      = &a_star_node_map[start_y * MAP_W + start_x];

	AStarBinaryHeap open_list     = a_star_heap_make(MAP_W * MAP_H, scratch.arena);
	AStarBinaryHeap tmp_open_list = a_star_heap_make(MAP_W * MAP_H, scratch.arena);

	for(int target_idx = 0; target_idx < target_count; ++target_idx)
	{
		AStarPathTile *target = &targets[target_idx];
		int target_x = target->x;
		int target_y = target->y;

		if(target_idx == 0)
		{
			a_star_node_init(start_node, start_x, start_y, target_x, target_y, 0, NULL);
			a_star_heap_insert(&open_list, start_node);
		}else
		{
			AStarNode *target_node = &a_star_node_map[target_y * MAP_W + target_x];
			if(target_node->closed)
			{
				a_star_push_path(&result, target_node, arena);
				continue;
			}else
			{
				mem_zero_array(tmp_open_list.nodes, tmp_open_list.node_count);
				tmp_open_list.node_count = 0;

				for(int node_idx = 0; node_idx < open_list.node_count; ++node_idx)
				{
					AStarNode *node = open_list.nodes[node_idx];
					a_star_node_init(node, node->x, node->y, target_x, target_y, node->g, node->parent);
					a_star_heap_insert(&tmp_open_list, node);
				}

				swap(open_list, tmp_open_list);
			}
		}

		int step_count = 0;
		while(open_list.node_count > 0)
		{
			AStarNode *curr = open_list.nodes[0];
			a_star_heap_remove_min(&open_list);
			curr->closed = true;

			if((step_count++ >= max_step_count) || (curr->x == target_x && curr->y == target_y))
			{
				a_star_push_path(&result, curr, arena);
				break;
			}

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
							neighbor->g      = g;
							neighbor->parent = curr;

							a_star_heap_heapify_up(&open_list, neighbor->heap_idx);
						}
					}
				}
			}
		}
	}

	arena_end_scratch(scratch);
	
	return result;
}
