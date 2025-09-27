#pragma once

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

int get_parent_idx (int idx);
int get_l_child_idx(int idx);
int get_r_child_idx(int idx);

AStarBinaryHeap a_star_heap_make(int capacity, Arena *arena);

void a_star_heap_swap      (AStarBinaryHeap *heap, int a, int b);
void a_star_heap_heapify_up(AStarBinaryHeap *heap, int idx);
void a_star_heap_insert    (AStarBinaryHeap *heap, AStarNode *node);
void a_star_heap_remove_min(AStarBinaryHeap *heap);

void a_star_node_init(AStarNode *node, int x, int y, int target_x, int target_y, int g, AStarNode *parent);
int  a_star_node_cmp (AStarNode *a, AStarNode *b);

AStarPath a_star_path_find(int start_x, int start_y, int target_x, int target_y, int max_step_count, Arena *arena);

