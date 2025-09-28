#include "app.h"

Font load_font(const char *filename)
{
	Font result = {};

	FILE *file = fopen(filename, "rb");
	if(file)
	{
		TmpArena scratch = arena_begin_scratch(NULL, 0);

		unsigned char *tmp_bitmap = arena_push_array(scratch.arena, GLYPH_BITMAP_W * GLYPH_BITMAP_H, unsigned char);
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
			stbtt_PackBegin(&pack_ctx, tmp_bitmap, GLYPH_BITMAP_W, GLYPH_BITMAP_H, 0, 1, NULL);
			stbtt_PackSetOversampling(&pack_ctx, 2, 2);
			stbtt_PackFontRange(&pack_ctx, ttf_buf, 0, font_pixel_size, ' ', array_count(result.char_data), result.char_data);
			stbtt_PackEnd(&pack_ctx);

			glGenTextures(1, &result.texture);
			glBindTexture(GL_TEXTURE_2D, result.texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, GLYPH_BITMAP_W, GLYPH_BITMAP_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tmp_bitmap);
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
		stbtt_GetPackedQuad(font->char_data, GLYPH_BITMAP_W, GLYPH_BITMAP_H, *text++ - ' ', &x, &y, &quad, false);

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

Factory generate_factory(AppState *app, Arena *arena)
{
	Factory result = {};

	int map_w = MAP_W;
	int map_h = MAP_H;

	result.map.w     = map_w;
	result.map.h     = map_h;
	result.map.tiles = arena_push_array(arena, map_w * map_h, int);

	result.stations = arena_push_array(arena, DESIRED_STATION_COUNT, Station);

	for(int station_idx = 0; station_idx < DESIRED_STATION_COUNT; ++station_idx)
	{
		int         type_idx = station_idx % STATION_TYPE_COUNT;
		StationType type     = app->station_types[type_idx];

		int w = type.w;
		int h = type.h;

		int bound_x = map_w - w - 2;
		int bound_y = map_h - h - 2;

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
				x = (random(&app->rng_seed) % bound_x) + 1;
				y = (random(&app->rng_seed) % bound_y) + 1;

				// Check for overlap (accounting for door) and regenerate the position if overlap exists
				for(int map_x = x - 1; map_x < x + w + 1; ++map_x)
				{
					for(int map_y = y - 1; map_y < y + h + 1; ++map_y)
					{
						if(result.map.tiles[map_y * map_w + map_x] == 1)
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
						result.map.tiles[map_y * map_w + map_x] = 1;
					}
				}

				Station *station  = &result.stations[result.station_count++];
				station->type_idx = type_idx;
				station->x        = x;
				station->y        = y;
			}
		}
	}

	result.chunks = arena_push_array(arena, CHUNK_X_COUNT * CHUNK_Y_COUNT, GridChunk);

	Arena *conflicts[] = {arena};
	TmpArena scratch   = arena_begin_scratch(conflicts, 1);

	for(int chunk_y = 0; chunk_y < CHUNK_Y_COUNT; ++chunk_y)
	{
		for(int chunk_x = 0; chunk_x < CHUNK_X_COUNT; ++chunk_x)
		{
			GridChunk *chunk = &result.chunks[chunk_y * CHUNK_Y_COUNT + chunk_x];

			int perimeter_tile_count = 0;
			PathTile *perimeter_tiles = arena_push_array(arena, CHUNK_PERIMETER_COUNT, PathTile);

			int rows = CHUNK_W;
			int cols = CHUNK_H;

			for(int y = 0; y < rows; ++y)
			{
				PathTile *tile = &perimeter_tiles[perimeter_tile_count++];
				tile->x = 0;
				tile->y = y;
			}

			for(int x = 1; x < cols; ++x)
			{
				PathTile *tile = &perimeter_tiles[perimeter_tile_count++];
				tile->x = x;
				tile->y = rows - 1;
			}

			for(int y = rows - 2; y >= 0; --y)
			{
				PathTile *tile = &perimeter_tiles[perimeter_tile_count++];
				tile->x = cols - 1;
				tile->y = y;
			}

			for(int x = cols - 2; x > 1; --x)
			{
				PathTile *tile = &perimeter_tiles[perimeter_tile_count++];
				tile->x = x;
				tile->y = 0;
			}

			Map chunk_map   = {};

			chunk_map.w     = CHUNK_W;
			chunk_map.h     = CHUNK_H;
			chunk_map.tiles = arena_push_array(scratch.arena, CHUNK_W * CHUNK_H, MapTile);

			mem_copy_array(chunk_map.tiles, chunk_map.w * chunk_map.h, result.map.tiles, chunk_map.w * chunk_map.h);

			for(int perimeter_tile_idx = 0; perimeter_tile_idx < perimeter_tile_count; ++perimeter_tile_idx)
			{
				PathTile *tile = &perimeter_tiles[perimeter_tile_idx];

				int target_offset = perimeter_tile_idx + 1;
				FoundPaths paths  = path_find_targets(&chunk_map, tile->x, tile->y, perimeter_tiles + target_offset, perimeter_tile_count - target_offset, 2048, scratch.arena);
			}
		}
	}

	arena_end_scratch(scratch);

	return result;
}

AppState app_make(const char *font_filename, unsigned int rng_seed, Arena *permanent_arena)
{
	AppState result = {};
	result.font     = load_font(font_filename);
	result.baseline = result.font.baseline_advance;

	result.rng_seed = rng_seed;

	result.station_types[0] = {0, 0, 1, 8, 8,  4, -1};
	result.station_types[1] = {0, 1, 0, 4, 4,  4,  2};
	result.station_types[2] = {0, 1, 1, 2, 2,  1,  2};
	result.station_types[3] = {1, 0, 0, 1, 1, -1,  0};

	result.factories = arena_push_array(permanent_arena, POPULATION_COUNT, Factory);

	//result.factories[0] = generate_factory(&result, permanent_arena);

	return result;
}

void app_update(AppState *app, InputState *input, Arena *transient_arena)
{
	for(int i = 0; i < POPULATION_COUNT; ++i)
	{
		app->factories[i] = generate_factory(app, transient_arena);
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

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

	int max_step_count = 1024;
	if(input->keys[KEY_LEFT].held)
	{
		app->step_count = max(app->step_count - 1, 0);
	}
	if(input->keys[KEY_RIGHT].held)
	{
		app->step_count = min(app->step_count + 1, max_step_count);
	}

	Factory *factory = &app->factories[0];

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, factory->map.w, factory->map.h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	int       target_count = factory->station_count;
	PathTile *targets      = arena_push_array(transient_arena, target_count, PathTile);

	assert(target_count <= factory->station_count);
	for(int i = 0; i < target_count; ++i)
	{
		Station *station = &factory->stations[i];
		StationType type = app->station_types[station->type_idx];

		PathTile *target = &targets[i];

		target->x = station->x + type.door_offset_x;
		target->y = station->y + type.door_offset_y;
	}

	glBegin(GL_QUADS);
	for(int station_idx = 0; station_idx < factory->station_count; ++station_idx)
	{
		Station *station = &factory->stations[station_idx];
		StationType type = app->station_types[station->type_idx];

		int start_x = station->x + type.door_offset_x;
		int start_y = station->y + type.door_offset_y;

		int step_count = max_step_count;
		if(app->step_count > 0)
		{
			step_count = app->step_count;
		}

		int target_offset = station_idx + 1;
		FoundPaths paths  = path_find_targets(&factory->map, start_x, start_y, targets + target_offset, target_count - target_offset, step_count, transient_arena);

		for(int path_idx = 0; path_idx < paths.count; ++path_idx)
		{
			FoundPath *path = &paths.paths[path_idx];

			for(int tile_idx = 0; tile_idx < path->tile_count; ++tile_idx)
			{
				PathTile *tile = &path->tiles[tile_idx];
				draw_rect(tile->x, tile->y, 1, 1, 1, 1, 1);
			}
		}
	}

	for(int station_idx = 0; station_idx < factory->station_count; ++station_idx)
	{
		Station *station = &factory->stations[station_idx];
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

	stbsp_snprintf(text, sizeof(text), "Step Count: %d", app->step_count);
	draw_text(&app->font, 0, app->baseline, 1, 1, 1, text);
	app->baseline += app->font.baseline_advance;
}

