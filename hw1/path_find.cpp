#include "path_find.h"

void grid_node_set_h(GridNode *node, int target_x, int target_y)
{
	int manhattan_x        = abs(target_x - node->x);
	int manhattan_y        = abs(target_y - node->y);
	int manhattan_distance = manhattan_x + manhattan_y;

	node->h = manhattan_distance;
}

int grid_node_cmp(GridNode *a, GridNode *b)
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

GridNodeHeap heap_make(Arena *arena)
{
	GridNodeHeap result = {};
	result.nodes        = arena_push_array(arena, GRID_NODE_HEAP_CAPACITY, GridNode *);

	return result;
}

void heap_swap(GridNodeHeap *heap, int a, int b)
{
	swap(heap->nodes[a], heap->nodes[b]);

	heap->nodes[a]->heap_idx = a;
	heap->nodes[b]->heap_idx = b;
}

void heap_heapify_up(GridNodeHeap *heap, int idx)
{
	while(idx > 0 && grid_node_cmp(heap->nodes[get_parent_idx(idx)], heap->nodes[idx]) > 0)
	{
		heap_swap(heap, get_parent_idx(idx), idx);
		idx = get_parent_idx(idx);
	}
}

void heap_insert(GridNodeHeap *heap, GridNode *node)
{
	if(heap->node_count < GRID_NODE_HEAP_CAPACITY)
	{
		node->heap_idx = heap->node_count++;

		// Insert at bottom of tree
		heap->nodes[node->heap_idx] = node;
		heap_heapify_up(heap, node->heap_idx);
	}
}

void heap_remove_min(GridNodeHeap *heap)
{
	int last_idx = heap->node_count - 1;
	int curr_idx = 0;
	heap_swap(heap, last_idx, curr_idx);
	--heap->node_count;

heapify_down:
	int l_idx = get_l_child_idx(curr_idx);
	int r_idx = get_r_child_idx(curr_idx);

	int smallest_idx = curr_idx;

	if(l_idx < heap->node_count && grid_node_cmp(heap->nodes[l_idx], heap->nodes[smallest_idx]) < 0)
	{
		smallest_idx = l_idx;
	}

	if(r_idx < heap->node_count && grid_node_cmp(heap->nodes[r_idx], heap->nodes[smallest_idx]) < 0)
	{
		smallest_idx = r_idx;
	}

	if(smallest_idx != curr_idx)
	{
		heap_swap(heap, smallest_idx, curr_idx);

		curr_idx = smallest_idx;
		goto heapify_down;
	}
}

void push_path(FoundPaths *paths, GridNode *node, Arena *arena)
{
	FoundPath *path = &paths->paths[paths->count++];

	for(GridNode *n = node; n; n = n->parent)
	{
		path->tile_count++;
	}

	path->tiles = arena_push_array(arena, path->tile_count, PathTile);

	int tile_idx = path->tile_count - 1;
	for(GridNode *n = node; n; n = n->parent)
	{
		PathTile *tile = &path->tiles[tile_idx--];
		tile->x             = n->x;
		tile->y             = n->y;
	}
}

FoundPaths path_find_targets(Map *map, int start_x, int start_y, PathTile *targets, int target_count, int max_step_count, Arena *arena)
{
	FoundPaths result = {};
	result.paths      = arena_push_array(arena, target_count, FoundPath);

	Arena *conflicts[] = {arena};
	TmpArena scratch   = arena_begin_scratch(conflicts, array_count(conflicts));

	GridNode *grid_node_map = arena_push_array(scratch.arena, map->w * map->h, GridNode);
	GridNode *start_node    = &grid_node_map[start_y * map->w + start_x];

	start_node->x      = start_x;
	start_node->y      = start_y;
	start_node->opened = true;

	GridNodeHeap open_list     = heap_make(scratch.arena);
	GridNodeHeap tmp_open_list = heap_make(scratch.arena);

	for(int target_idx = 0; target_idx < target_count; ++target_idx)
	{
		PathTile *target = &targets[target_idx];
		int target_x = target->x;
		int target_y = target->y;

		if(target_idx == 0)
		{
			grid_node_set_h(start_node, target_x, target_y);
			heap_insert(&open_list, start_node);
		}else
		{
			GridNode *target_node = &grid_node_map[target_y * map->w + target_x];
			if(target_node->closed)
			{
				push_path(&result, target_node, arena);
				continue;
			}else
			{
				mem_zero_array(tmp_open_list.nodes, tmp_open_list.node_count);
				tmp_open_list.node_count = 0;

				for(int node_idx = 0; node_idx < open_list.node_count; ++node_idx)
				{
					GridNode *node = open_list.nodes[node_idx];
					grid_node_set_h(node, target_x, target_y);

					heap_insert(&tmp_open_list, node);
				}

				swap(open_list, tmp_open_list);
			}
		}

		int step_count = 0;
		while(open_list.node_count > 0)
		{
			GridNode *curr = open_list.nodes[0];
			heap_remove_min(&open_list);
			curr->closed = true;

			if((step_count++ >= max_step_count) || (curr->x == target_x && curr->y == target_y))
			{
				push_path(&result, curr, arena);
				break;
			}

			int neighbor_offsets_x[] = {1, 0, -1,  0};
			int neighbor_offsets_y[] = {0, 1,  0, -1};

			for(int neighbor_idx = 0; neighbor_idx < 4; ++neighbor_idx)
			{
				int neighbor_x = curr->x + neighbor_offsets_x[neighbor_idx];
				int neighbor_y = curr->y + neighbor_offsets_y[neighbor_idx];

				if(neighbor_x >= 0 && neighbor_x < map->w && neighbor_y >= 0 && neighbor_y < map->h && map->tiles[neighbor_y * map->w + neighbor_x] == 0)
				{
					GridNode *neighbor = &grid_node_map[neighbor_y * map->w + neighbor_x];

					if(!neighbor->closed)
					{
						int g = curr->g + 1;

						if(!neighbor->opened)
						{
							neighbor->x      = neighbor_x;
							neighbor->y      = neighbor_y;
							neighbor->g      = g;
							neighbor->opened = true;
							neighbor->parent = curr;

							grid_node_set_h(neighbor, target_x, target_y);

							heap_insert(&open_list, neighbor);
						}else if(g < neighbor->g)
						{
							neighbor->g      = g;
							neighbor->parent = curr;

							heap_heapify_up(&open_list, neighbor->heap_idx);
						}
					}
				}
			}
		}
	}

	arena_end_scratch(scratch);
	
	return result;
}

FoundPath path_find_target(Map *map, int start_x, int start_y, int target_x, int target_y, int max_step_count, Arena *arena)
{
	PathTile   target = {target_x, target_y};
	FoundPaths paths  = path_find_targets(map, start_x, start_y, &target, 1, max_step_count, arena);

	assert(paths.count == 1);

	FoundPath result = paths.paths[0];
	return result;
}

