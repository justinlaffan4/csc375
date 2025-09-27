#include "common.h"

Arena arena_make(uint64_t reserve_size)
{
	Arena result = {};

	result.capacity = reserve_size;
	result.base     = vmem_reserve(result.capacity);
	assert(result.base);

	return result;
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
		result.arenas[i] = arena_make();
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

