#pragma once

#include "common.h"

const int MAP_W = 64;
const int MAP_H = 64;

const int GRID_NODE_HEAP_CAPACITY = MAP_W * MAP_H;

typedef int MapTile;

struct GridNode
{
	int x;
	int y;

	int g; // Distance from start to node
	int h; // Best distance (ignoring occlusions) from node to target

	bool opened;
	bool closed;

	int heap_idx;

	GridNode *parent;
};

// Binary minimum heap
struct GridNodeHeap
{
	int        node_count;
	GridNode **nodes;
};

struct PathTile
{
	int x;
	int y;
};

struct FoundPath
{
	int       tile_count;
	PathTile *tiles;
};

struct FoundPaths
{
	int        count;
	FoundPath *paths;
};

void grid_node_set_h(GridNode *node, int target_x, int target_y);
int  grid_node_cmp  (GridNode *a, GridNode *b);

int get_parent_idx (int idx);
int get_l_child_idx(int idx);
int get_r_child_idx(int idx);

GridNodeHeap heap_make(Arena *arena);

void heap_swap      (GridNodeHeap *heap, int a, int b);
void heap_heapify_up(GridNodeHeap *heap, int idx);
void heap_insert    (GridNodeHeap *heap, GridNode *node);
void heap_remove_min(GridNodeHeap *heap);

FoundPaths path_find_targets(MapTile *map, int start_x, int start_y, PathTile *targets, int target_count, int max_step_count, Arena *arena);
FoundPath  path_find_target (MapTile *map, int start_x, int start_y, int target_x, int target_y, int max_step_count, Arena *arena);

