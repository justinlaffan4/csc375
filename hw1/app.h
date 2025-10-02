#pragma once

#include "common.h"
#include "path_find.h"
#include "platform.h"

const int STATION_TYPE_COUNT = 4;
const int DESIRED_STATION_COUNT = 48;
const int DESIRED_POPULATION_COUNT = 100;

const int GLYPH_BITMAP_W = 512;
const int GLYPH_BITMAP_H = 512;

const int MAX_STEP_COUNT = 1024;

const int THREAD_COUNT = 4;

struct Font
{
	float  baseline_advance;
	GLuint texture;

	stbtt_packedchar char_data['~' - ' '];
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
	int type;

	float r;
	float g;
	float b;

	int x0;
	int y0;

	int x1;
	int y1;

	int door_offset_x;
	int door_offset_y;
};

struct Factory
{
	MapTile *map;

	int      station_count;
	Station *stations;

	int fitness_score;
};

struct AppState
{
	Font  font;
	float baseline;

	unsigned int rng_seed;

	StationType station_types[STATION_TYPE_COUNT];
	int         station_weight_lut[STATION_TYPE_COUNT][STATION_TYPE_COUNT];
	
	int      population_count;
	Factory *population;

	int generation_count;

	int step_count;
};

Font load_font(const char *filename);

void draw_text(Font *font, float x, float y, float r, float g, float b, char *text);
void draw_rect(float x, float y, float w, float h, float r, float g, float b);

Factory generate_factory(AppState *app, Arena *arena);

AppState app_make  (const char *font_filename, unsigned int rng_seed, Arena *permanent_arena);
void     app_update(AppState *app, InputState *input, WorkQueue *work_queue, Arena *transient_arena);
