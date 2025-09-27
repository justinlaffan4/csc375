#pragma once

#include "common.h"
#include "platform.h"

const int MAP_W = 128;
const int MAP_H = 128;

const int STATION_TYPE_COUNT = 4;
const int DESIRED_STATION_COUNT = 48;
const int POPULATION_COUNT = 100;

const int GLYPH_BITMAP_W = 512;
const int GLYPH_BITMAP_H = 512;

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
	int type_idx;

	int x;
	int y;
};

typedef int MapTile;

struct Factory
{
	MapTile *map;

	int      station_count;
	Station *stations;
};

struct AppState
{
	Font  font;
	float baseline;

	unsigned int rng_seed;

	StationType station_types[STATION_TYPE_COUNT];
	
	int      factory_count;
	Factory *factories;

	int step_count;
};

Font load_font(const char *filename);

void draw_text(Font *font, float x, float y, float r, float g, float b, char *text);
void draw_rect(float x, float y, float w, float h, float r, float g, float b);

Factory generate_factory(AppState *app, Arena *arena);

AppState app_make  (const char *font_filename, unsigned int rng_seed, Arena *permanent_arena);
void     app_update(AppState *app, InputState *input, Arena *transient_arena);
