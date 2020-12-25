/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "trivia/util.h"

#include "box/lua/slab.h"
#include "lua/utils.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lj_obj.h> /* internals: lua in box.runtime.info() */

#include "small/small.h"
#include "small/quota.h"
#include "memory.h"
#include "box/engine.h"
#include "box/memtx_engine.h"
#include "box/small_allocator.h"
#include "box/system_allocator.h"

#define STATS_NOOP_CB(allocator, cb_stats)						\
static int 										\
allocator##_stats_noop_cb(const struct cb_stats *stats, void *cb_ctx)			\
{											\
	(void) stats;									\
	(void) cb_ctx;									\
	return 0;									\
}
STATS_NOOP_CB(small, mempool_stats)
STATS_NOOP_CB(system, system_stats)

static int
small_stats_lua_cb(const struct mempool_stats *stats, void *cb_ctx)
{
	/** Don't publish information about empty slabs. */
	if (stats->slabcount == 0)
		return 0;

	struct lua_State *L = (struct lua_State *) cb_ctx;

	/*
	 * Create a Lua table for every slab class. A class is
	 * defined by its item size.
	 */
	/** Assign next slab size to the next member of an array. */
	lua_pushnumber(L, lua_objlen(L, -1) + 1);
	lua_newtable(L);
	/**
	 * This is in fact only to force YaML flow "compact" for this
	 * table.
	 */
	luaL_setmaphint(L, -1);

	lua_pushstring(L, "mem_used");
	luaL_pushuint64(L, stats->totals.used);
	lua_settable(L, -3);

	lua_pushstring(L, "slab_size");
	luaL_pushuint64(L, stats->slabsize);
	lua_settable(L, -3);

	lua_pushstring(L, "mem_free");
	luaL_pushuint64(L, stats->totals.total - stats->totals.used);
	lua_settable(L, -3);

	lua_pushstring(L, "item_size");
	luaL_pushuint64(L, stats->objsize);
	lua_settable(L, -3);

	lua_pushstring(L, "slab_count");
	luaL_pushuint64(L, stats->slabcount);
	lua_settable(L, -3);

	lua_pushstring(L, "item_count");
	luaL_pushuint64(L, stats->objcount);
	lua_settable(L, -3);

	lua_settable(L, -3);
	return 0;
}

static int
system_stats_lua_cb(const struct system_stats *stats, void *cb_ctx)
{
	struct lua_State *L = (struct lua_State *) cb_ctx;
	lua_pushnumber(L, lua_objlen(L, -1) + 1);
	lua_newtable(L);
	luaL_setmaphint(L, -1);
	lua_pushstring(L, "mem_used");
	luaL_pushuint64(L, stats->used);
	lua_settable(L, -3);

	lua_pushstring(L, "mem_free");
	luaL_pushuint64(L, stats->total - stats->used);
	lua_settable(L, -3);
	lua_settable(L, -3);
	return 0;
}


template <class allocator_stats, class cb_stats, class Allocator,
	  int (*stats_cb)(const cb_stats *stats, void *cb_ctx)>
static int
lbox_slab_stats(struct lua_State *L)
{
	struct memtx_engine *memtx;
	memtx = (struct memtx_engine *)engine_by_name("memtx");

	allocator_stats totals;
	lua_newtable(L);
	/*
	 * List all slabs used for tuples and slabs used for
	 * indexes, with their stats.
	 */
	Allocator::stats(&totals, stats_cb, L);
	struct mempool_stats index_stats;
	mempool_stats(&memtx->index_extent_pool, &index_stats);
	small_stats_lua_cb(&index_stats, L);

	return 1;
}

template <class allocator_stats, class cb_stats, class Allocator,
	  int (*stats_cb)(const cb_stats *stats, void *cb_ctx)>
static int
lbox_slab_info(struct lua_State *L)
{
	struct memtx_engine *memtx;
	memtx = (struct memtx_engine *)engine_by_name("memtx");

	allocator_stats totals;

	/*
	 * List all slabs used for tuples and slabs used for
	 * indexes, with their stats.
	 */
	lua_newtable(L);
	Allocator::stats(&totals, stats_cb, L);
	struct mempool_stats index_stats;
	mempool_stats(&memtx->index_extent_pool, &index_stats);

	double ratio;
	char ratio_buf[32];

	ratio = 100 * ((double) totals.used
		/ ((double) totals.total + 0.0001));
	snprintf(ratio_buf, sizeof(ratio_buf), "%0.2lf%%", ratio);

	/** How much address space has been already touched */
	lua_pushstring(L, "items_size");
	luaL_pushuint64(L, totals.total);
	lua_settable(L, -3);
	/**
	 * How much of this formatted address space is used for
	 * actual data.
	 */
	lua_pushstring(L, "items_used");
	luaL_pushuint64(L, totals.used);
	lua_settable(L, -3);

	/*
	 * Fragmentation factor for tuples. Don't account indexes,
	 * even if they are fragmented, there is nothing people
	 * can do about it.
	 */
	lua_pushstring(L, "items_used_ratio");
	lua_pushstring(L, ratio_buf);
	lua_settable(L, -3);

	/** How much address space has been already touched
	 * (tuples and indexes) */
	lua_pushstring(L, "arena_size");
	/*
	 * We could use totals.total + index_stats.total here,
	 * but this would not account for slabs which are sitting
	 * in slab cache or in the arena, available for reuse.
	 * Make sure a simple formula:
	 * items_used_ratio > 0.9 && arena_used_ratio > 0.9 &&
	 * quota_used_ratio > 0.9 work as an indicator
	 * for reaching Tarantool memory limit.
	 */
	size_t arena_size = memtx->arena.used;
	luaL_pushuint64(L, arena_size);
	lua_settable(L, -3);
	/**
	 * How much of this formatted address space is used for
	 * data (tuples and indexes).
	 */
	lua_pushstring(L, "arena_used");
	luaL_pushuint64(L, totals.used + index_stats.totals.used);
	lua_settable(L, -3);

	ratio = 100 * ((double) (totals.used + index_stats.totals.used)
		       / (double) arena_size);
	snprintf(ratio_buf, sizeof(ratio_buf), "%0.1lf%%", ratio);

	lua_pushstring(L, "arena_used_ratio");
	lua_pushstring(L, ratio_buf);
	lua_settable(L, -3);

	/*
	 * This is pretty much the same as
	 * box.cfg.slab_alloc_arena, but in bytes
	 */
	lua_pushstring(L, "quota_size");
	luaL_pushuint64(L, quota_total(&memtx->quota));
	lua_settable(L, -3);

	/*
	 * How much quota has been booked - reflects the total
	 * size of slabs in various slab caches.
	 */
	lua_pushstring(L, "quota_used");
	luaL_pushuint64(L, quota_used(&memtx->quota));
	lua_settable(L, -3);

	/**
	 * This should be the same as arena_size/arena_used, however,
	 * don't trust totals in the most important monitoring
	 * factor, it's the quota that give you OOM error in the
	 * end of the day.
	 */
	ratio = 100 * ((double) quota_used(&memtx->quota) /
		 ((double) quota_total(&memtx->quota) + 0.0001));
	snprintf(ratio_buf, sizeof(ratio_buf), "%0.2lf%%", ratio);

	lua_pushstring(L, "quota_used_ratio");
	lua_pushstring(L, ratio_buf);
	lua_settable(L, -3);

	return 1;
}

template <class Allocator>
static int
lbox_slab_check(MAYBE_UNUSED struct lua_State *L)
{
	Allocator::memory_check();
	return 0;
}

template <class allocator_stats, class cb_stats, class Allocator,
	  int (*stats_cb_info)(const cb_stats *stats, void *cb_ctx),
	  int (*stats_cb_stats)(const cb_stats *stats, void *cb_ctx)>
static void
box_lua_slab_init(struct lua_State *L)
{
	lua_pushstring(L, "info");
	lua_pushcfunction(L, (lbox_slab_info<allocator_stats,
		cb_stats, Allocator, stats_cb_info>));
	lua_settable(L, -3);

	lua_pushstring(L, "stats");
	lua_pushcfunction(L, (lbox_slab_stats<allocator_stats,
		cb_stats, Allocator, stats_cb_stats>));
	lua_settable(L, -3);

	lua_pushstring(L, "check");
	lua_pushcfunction(L, lbox_slab_check<Allocator>);
	lua_settable(L, -3);
}

/** Initialize box.slab package. */
void
box_lua_slab_init(struct lua_State *L)
{
	struct memtx_engine *memtx;
	memtx = (struct memtx_engine *)engine_by_name("memtx");

	lua_getfield(L, LUA_GLOBALSINDEX, "box");
	lua_pushstring(L, "slab");
	lua_newtable(L);

	switch(memtx->allocator_type) {
	case MEMTX_SMALL_ALLOCATOR:
		box_lua_slab_init<struct small_stats,
			struct mempool_stats, SmallAllocator,
			small_stats_noop_cb, small_stats_lua_cb>(L);
		break;
	case MEMTX_SYSTEM_ALLOCATOR:
		box_lua_slab_init<struct system_stats,
			struct system_stats, SystemAllocator,
			system_stats_noop_cb, system_stats_lua_cb>(L);
		break;
	default:
		;
	}

	lua_settable(L, -3); /* box.slab */

	lua_pop(L, 1); /* box. */
}
