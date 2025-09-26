#pragma once

#define array_count(arr) (sizeof(arr) / sizeof((arr)[0]))
#define abs(x) ((x) > 0 ? (x) : -(x))

#define swap(a, b) do { static_assert(sizeof(a) == sizeof(b)); unsigned char swap_tmp[sizeof(a)]; memcpy(swap_tmp, &a, sizeof(a)); memcpy(&a, &b, sizeof(a)); memcpy(&b, swap_tmp, sizeof(a)); } while(false)

const int STATION_TYPE_COUNT = 4;
const int DESIRED_STATION_COUNT = 24;

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

	bool visited;
	bool search;

	AStarNode *next_in_path;
};

struct AStarBinaryHeapNode
{
	AStarNode *n;

	int                  l_child_count;
	AStarBinaryHeapNode *l_child;

	int                  r_child_count;
	AStarBinaryHeapNode *r_child;
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

	StationType station_types[STATION_TYPE_COUNT];

	int     station_count;
	Station stations[DESIRED_STATION_COUNT];
};

struct InputState
{
	unsigned int rng_seed;

	int client_w;
	int client_h;

	uint64_t elapsed_microsecs;
};

unsigned int random(unsigned int *rng_seed);

Font load_font(const char *filename);

void draw_text(Font *font, float x, float y, float r, float g, float b, char *text);
void draw_rect(float x, float y, float w, float h, float r, float g, float b);

AStarBinaryHeapNode *a_star_node_remove_bottom  (AStarBinaryHeapNode *parent);
int                  a_star_node_compare_f_score(AStarBinaryHeapNode *a, AStarBinaryHeapNode *b);
void                 a_star_node_remove_fix     (AStarBinaryHeapNode *parent);
AStarBinaryHeapNode *a_star_node_remove         (AStarBinaryHeapNode *parent);
AStarBinaryHeapNode *a_star_node_insert         (AStarBinaryHeapNode *parent, AStarNode *to_insert);

AStarNode *a_star_path_find_new(int start_x, int start_y, int target_x, int target_y);
AStarNode *a_star_path_find_old(int start_x, int start_y, int target_x, int target_y);

AppState app_make  (const char *font_filename, InputState *input);
void     app_update(AppState *app, InputState *input);
