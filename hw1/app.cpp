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
			stbtt_PackBegin          (&pack_ctx, tmp_bitmap, GLYPH_BITMAP_W, GLYPH_BITMAP_H, 0, 1, NULL);
			stbtt_PackSetOversampling(&pack_ctx, 2, 2);
			stbtt_PackFontRange      (&pack_ctx, ttf_buf, 0, font_pixel_size, ' ', array_count(result.char_data), result.char_data);
			stbtt_PackEnd            (&pack_ctx);

			glGenTextures  (1, &result.texture);
			glBindTexture  (GL_TEXTURE_2D, result.texture);
			glTexImage2D   (GL_TEXTURE_2D, 0, GL_ALPHA, GLYPH_BITMAP_W, GLYPH_BITMAP_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tmp_bitmap);
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

bool test_bounds(int x0, int y0, int x1, int y1)
{
	bool point0_in_bounds = x0 >= 0 && x0 < MAP_W && y0 >= 0 && y0 < MAP_H;
	bool point1_in_bounds = x1 >= 0 && x1 < MAP_W && y1 >= 0 && y1 < MAP_H;

	bool result = point0_in_bounds && point1_in_bounds;
	return result;
}

// Check for overlap (accounting for door) and return true if overlap exists
bool test_overlap(Factory *factory, Station *s)
{
	int x0 = s->x0 - 1;
	int y0 = s->y0 - 1;
	int x1 = s->x1 + 1;
	int y1 = s->y1 + 1;

	bool result = false;
	if(x0 < x1 && y0 < y1 && test_bounds(x0, y0, x1, y1))
	{
		for(int map_x = x0; map_x < x1; ++map_x)
		{
			for(int map_y = y0; map_y < y1; ++map_y)
			{
				if(factory->map[map_y * MAP_W + map_x] == 1)
				{
					result = true;
					break;
				}
			}
		}
	}else
	{
		result = true;
	}
	return result;
}

bool test_overlap(Factory *factory, int x, int y, int w, int h)
{
	Station s = {};
	s.x0 = x;
	s.y0 = y;
	s.x1 = x + w;
	s.y1 = y + h;

	bool result = test_overlap(factory, &s);
	return result;
}

void write_to_map(Factory *factory, Station *s, int val)
{
	for(int map_x = s->x0; map_x < s->x1; ++map_x)
	{
		for(int map_y = s->y0; map_y < s->y1; ++map_y)
		{
			factory->map[map_y * MAP_W + map_x] = val;
		}
	}
}

void write_to_map(Factory *factory, int x, int y, int w, int h, int val)
{
	Station s = {};
	s.x0 = x;
	s.y0 = y;
	s.x1 = x + w;
	s.y1 = y + h;

	write_to_map(factory, &s, val);
}

Factory generate_factory(AppState *app, Arena *arena)
{
	Factory result  = {};
	result.map      = arena_push_array(arena, MAP_W * MAP_H, int);
	result.stations = arena_push_array(arena, DESIRED_STATION_COUNT, Station);

	for(int station_idx = 0; station_idx < DESIRED_STATION_COUNT; ++station_idx)
	{
		int         type_idx = station_idx % STATION_TYPE_COUNT;
		StationType type     = app->station_types[type_idx];

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
				x = (random(&app->rng_seed) % bound_x) + 1;
				y = (random(&app->rng_seed) % bound_y) + 1;

				if(test_overlap(&result, x, y, w, h))
				{
					goto regenerate_facility;
				}
			}

			if(try_count < max_tries)
			{
				write_to_map(&result, x, y, w, h, 1);

				Station *station = &result.stations[result.station_count++];
				StationType type = app->station_types[type_idx];

				station->type = type_idx;

				station->r = type.r;
				station->g = type.g;
				station->b = type.b;

				station->x0 = x;
				station->y0 = y;

				station->x1 = x + w;
				station->y1 = y + h;

				station->door_offset_x = type.door_offset_x;
				station->door_offset_y = type.door_offset_y;
			}
		}
	}

	return result;
}

void merge_sort_factories(Factory *sorted, Factory *unsorted, int count)
{
	if(count <= 0)
	{
		return;
	}else if(count == 1)
	{
		sorted[0] = unsorted[0];
		return;
	}else if(count == 2)
	{
		Factory *a = &unsorted[0];
		Factory *b = &unsorted[1];
		if (b->fitness_score > a->fitness_score)
		{
			Factory tmp = *a;

			*a = *b;
			*b = tmp;
		}
		return;
	}

	int l = count / 2;
	int r = count - l;

	merge_sort_factories(    sorted,     unsorted, l);
	merge_sort_factories(&sorted[l], &unsorted[l], r);

	Factory *src_l = unsorted;
	Factory *src_r = &unsorted[l];

	Factory *end_l = src_r;
	Factory *end_r = &unsorted[count];

	for(int i = 0; i < count; ++i)
	{
		Factory *record = &sorted[i];

		if(src_l == end_l)
		{
			*record = *src_r++;
		}else if(src_r == end_r)
		{
			*record = *src_l++;
		}else if(src_l->fitness_score > src_r->fitness_score)
		{
			*record = *src_l++;
		}else
		{
			*record = *src_r++;
		}
	}

	memcpy(unsorted, sorted, count * sizeof(Factory));
}

int get_fitness_score(AppState *app, Factory *factory)
{
	int result = 1000000;

	TmpArena scratch = arena_begin_scratch(NULL, 0);

	int       target_count = factory->station_count;
	PathTile *targets      = arena_push_array(scratch.arena, target_count, PathTile);

	assert(target_count <= factory->station_count);
	for(int i = 0; i < target_count; ++i)
	{
		Station *station = &factory->stations[i];

		PathTile *target = &targets[i];

		target->x = station->x0 + station->door_offset_x;
		target->y = station->y0 + station->door_offset_y;
	}

	for(int station_idx = 0; station_idx < factory->station_count; ++station_idx)
	{
		Station *station = &factory->stations[station_idx];

		int start_x = station->x0 + station->door_offset_x;
		int start_y = station->y0 + station->door_offset_y;

		int step_count = MAX_STEP_COUNT;
		if(app->step_count > 0)
		{
			step_count = app->step_count;
		}

		int target_offset = station_idx + 1;
		FoundPaths paths  = path_find_targets(factory->map, start_x, start_y, targets + target_offset, target_count - target_offset, step_count, scratch.arena);

		for(int path_idx = 0; path_idx < paths.count; ++path_idx)
		{
			FoundPath *path = &paths.paths[path_idx];

			Station *target_station = factory->stations + target_offset + path_idx;

			int weight = app->station_weight_lut[station->type][target_station->type];

			result -= path->tile_count * weight;
#if 0
			for(int tile_idx = 0; tile_idx < path->tile_count; ++tile_idx)
			{
				PathTile *tile = &path->tiles[tile_idx];
				draw_rect(tile->x, tile->y, 1, 1, 1, 1, 1);
			}
#endif
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

	int station_weight_lut[STATION_TYPE_COUNT][STATION_TYPE_COUNT] = {
		1, 20, 30, 40,
		20, 1, 20, 30,
		30, 20, 1, 20,
		40, 30, 20, 1,
	};

	memcpy(result.station_weight_lut, station_weight_lut, sizeof(station_weight_lut));

	result.population = arena_push_array(permanent_arena, DESIRED_POPULATION_COUNT, Factory);

	for(int i = 0; i < DESIRED_POPULATION_COUNT; ++i)
	{
		Factory factory = generate_factory(&result, permanent_arena);
		if(factory.station_count == DESIRED_STATION_COUNT)
		{
			result.population[result.population_count++] = factory;
		}
	}

	return result;
}

struct ThreadedFitnessScore
{
	AppState *app;
	int population_start;
	int population_count;
	Factory *selected_population;

	int min_fitness_score;
	int max_fitness_score;
};

work_queue_callback(get_fitness_score_threaded)
{
	ThreadedFitnessScore *work = (ThreadedFitnessScore *)user_params;

	int min_fitness_score = INT_MAX;
	int max_fitness_score = 0;
	int selected_population_count = 0;
	for(int factory_idx = work->population_start; factory_idx < work->population_start + work->population_count; ++factory_idx)
	{
		Factory *factory = &work->app->population[factory_idx];

		factory->fitness_score = get_fitness_score(work->app, factory);

		work->min_fitness_score = min(factory->fitness_score, min_fitness_score);
		work->max_fitness_score = max(factory->fitness_score, max_fitness_score);

		work->selected_population[selected_population_count++] = *factory;
	}
}

void app_update(AppState *app, InputState *input, WorkQueue *work_queue, Arena *transient_arena)
{
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

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, MAP_W, MAP_H, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if(input->keys[KEY_LEFT].held)
	{
		app->step_count = max(app->step_count - 1, 0);
	}
	if(input->keys[KEY_RIGHT].held)
	{
		app->step_count = min(app->step_count + 1, MAX_STEP_COUNT);
	}

	ThreadedFitnessScore threaded_fitness_score[THREAD_COUNT] = {};

	Factory *unsorted_selected_population = arena_push_array(transient_arena, app->population_count, Factory);

	int population_count_per_thread = app->population_count / THREAD_COUNT;
	int population_count_remainder  = app->population_count % THREAD_COUNT;

	for(int thread_idx = 0; thread_idx < THREAD_COUNT; ++thread_idx)
	{
		ThreadedFitnessScore *threaded = &threaded_fitness_score[thread_idx];
		threaded->app = app;
		threaded->population_start = population_count_per_thread * thread_idx;
		threaded->population_count = population_count_per_thread;
		threaded->selected_population = unsorted_selected_population + threaded->population_start;

		if(thread_idx == THREAD_COUNT - 1)
		{
			threaded->population_count += population_count_remainder;
		}

		work_queue_push_work(work_queue, get_fitness_score_threaded, threaded);
	}

	work_queue_work_until_done(work_queue, 0);

	int min_fitness_score = INT_MAX;
	int max_fitness_score = 0;
	for(int thread_idx = 0; thread_idx < THREAD_COUNT; ++thread_idx)
	{
		ThreadedFitnessScore *threaded = &threaded_fitness_score[thread_idx];

		min_fitness_score = min(threaded->min_fitness_score, min_fitness_score);
		max_fitness_score = max(threaded->max_fitness_score, max_fitness_score);
	}

	int selected_population_count = app->population_count;

	// The problem with this approach is handling the case where only 1 factory makes it (which happened to me)
	//int selection_fitness_score = (random(&app->rng_seed) % (max_fitness_score - min_fitness_score)) + min_fitness_score;

	int keep_evens = random(&app->rng_seed) % 2;

	for(int factory_idx = selected_population_count - 1; factory_idx >= 0; --factory_idx)
	{
		Factory *factory = &unsorted_selected_population[factory_idx];
#if 0
		if(factory->fitness_score < selection_fitness_score)
#else
		if((factory_idx % 2) == keep_evens)
#endif
		{
			*factory = unsorted_selected_population[--selected_population_count];
		}
	}

	Factory *selected_population = arena_push_array(transient_arena, selected_population_count, Factory);
	merge_sort_factories(selected_population, unsorted_selected_population, selected_population_count);

	int      next_population_count = 0;
	Factory *next_population       = arena_push_array(transient_arena, DESIRED_POPULATION_COUNT, Factory);

	for(int factory_idx = 0; factory_idx < DESIRED_POPULATION_COUNT; ++factory_idx)
	{
		Factory child_factory = {};

		child_factory.map      = arena_push_array(transient_arena, MAP_W * MAP_H, MapTile);
		child_factory.stations = arena_push_array(transient_arena, DESIRED_STATION_COUNT, Station);

		int parent_factory0_idx = random(&app->rng_seed) % selected_population_count;
		int parent_factory1_idx = random(&app->rng_seed) % selected_population_count;

		Factory *parent_factory0 = &selected_population[parent_factory0_idx];
		Factory *parent_factory1 = &selected_population[parent_factory1_idx];

		if(parent_factory1->fitness_score > parent_factory0->fitness_score)
		{
			swap(parent_factory0, parent_factory1);
		}

		for(int station_idx = 0; station_idx < parent_factory0->station_count; ++station_idx)
		{
			Factory *parent_factory        = NULL;
			Factory *backup_parent_factory = NULL;

			int chance = random(&app->rng_seed) % 100;
			if(chance < 80)
			{
				parent_factory        = parent_factory0;
				backup_parent_factory = parent_factory1;
			}else
			{
				parent_factory        = parent_factory1;
				backup_parent_factory = parent_factory0;
			}

			Station *station        = &parent_factory->stations[station_idx];
			Station *backup_station = &backup_parent_factory->stations[station_idx];

			bool does_station_overlap        = test_overlap(&child_factory, station);
			bool does_backup_station_overlap = test_overlap(&child_factory, backup_station);

			if(!does_station_overlap)
			{
				write_to_map(&child_factory, station, 1);
				child_factory.stations[child_factory.station_count++] = *station;
			}else if(!does_backup_station_overlap)
			{
				write_to_map(&child_factory, backup_station, 1);
				child_factory.stations[child_factory.station_count++] = *backup_station;
			}
		}

		if(child_factory.station_count == DESIRED_STATION_COUNT)
		{
			next_population[next_population_count++] = child_factory;
		}
	}

	for(int factory_idx = 0; factory_idx < next_population_count; ++factory_idx)
	{
		Factory *factory = &next_population[factory_idx];

		for(int station_idx = 0; station_idx < factory->station_count; ++station_idx)
		{
			Station *station = &factory->stations[station_idx];

			int mutation_chance = random(&app->rng_seed) % 100;
			if(mutation_chance < 6)
			{
				Station new_station = *station;

				int shift_x_count = (random(&app->rng_seed) % 4) - 2;
				int shift_y_count = (random(&app->rng_seed) % 4) - 2;

				new_station.x0 = station->x0 + shift_x_count;
				new_station.y0 = station->y0 + shift_y_count;

				new_station.x1 = station->x1 + shift_x_count;
				new_station.y1 = station->y1 + shift_y_count;

#if 1
				write_to_map(factory, station, 0);
				if(!test_overlap(factory, &new_station))
				{
					write_to_map(factory, &new_station, 1);
					*station = new_station;
				}else
				{
					write_to_map(factory, station, 1);
				}
#endif
			}
		}
	}

	for(int factory_idx = 0; factory_idx < next_population_count; ++factory_idx)
	{
		Factory *dst = &app->population[factory_idx];
		Factory *src = &next_population[factory_idx];

		mem_copy_array(dst->map,      MAP_W * MAP_H,      src->map,      MAP_W * MAP_H);
		mem_copy_array(dst->stations, dst->station_count, src->stations, src->station_count);

		dst->station_count = src->station_count;
	}
	app->population_count = next_population_count;

	Factory *factory = &selected_population[0];

	glBegin(GL_QUADS);
	for(int station_idx = 0; station_idx < factory->station_count; ++station_idx)
	{
		Station *station = &factory->stations[station_idx];

		float r = station->r;
		float g = station->g;
		float b = station->b;

		int x = station->x0;
		int y = station->y0;
		int w = station->x1 - station->x0;
		int h = station->y1 - station->y0;

		draw_rect(x, y, w, h, r, g, b);
	}

	int       target_count = factory->station_count;
	PathTile *targets      = arena_push_array(transient_arena, target_count, PathTile);

	assert(target_count <= factory->station_count);
	for(int i = 0; i < target_count; ++i)
	{
		Station *station = &factory->stations[i];

		PathTile *target = &targets[i];

		target->x = station->x0 + station->door_offset_x;
		target->y = station->y0 + station->door_offset_y;
	}

	for(int station_idx = 0; station_idx < factory->station_count; ++station_idx)
	{
		Station *station = &factory->stations[station_idx];

		int start_x = station->x0 + station->door_offset_x;
		int start_y = station->y0 + station->door_offset_y;

		int step_count = MAX_STEP_COUNT;
		if(app->step_count > 0)
		{
			step_count = app->step_count;
		}

		int target_offset = station_idx + 1;
		FoundPaths paths  = path_find_targets(factory->map, start_x, start_y, targets + target_offset, target_count - target_offset, step_count, transient_arena);

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

		int x = station->x0;
		int y = station->y0;

		int door_x = x + station->door_offset_x;
		int door_y = y + station->door_offset_y;
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

	stbsp_snprintf(text, sizeof(text), "Generation Count: %d", app->generation_count++);
	draw_text(&app->font, 0, app->baseline, 1, 1, 1, text);
	app->baseline += app->font.baseline_advance;

	stbsp_snprintf(text, sizeof(text), "Fitness Score: %d", factory->fitness_score);
	draw_text(&app->font, 0, app->baseline, 1, 1, 1, text);
	app->baseline += app->font.baseline_advance;
}

