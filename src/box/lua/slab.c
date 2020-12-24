/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
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

static int
lbox_runtime_info(struct lua_State *L)
{
	lua_newtable(L);

	lua_pushstring(L, "used");
	luaL_pushuint64(L, runtime.used);
	lua_settable(L, -3);

	lua_pushstring(L, "maxalloc");
	luaL_pushuint64(L, quota_total(runtime.quota));
	lua_settable(L, -3);

	/*
	 * Lua GC heap size
	 */
	lua_pushstring(L, "lua");
	lua_pushinteger(L, G(L)->gc.total);
	lua_settable(L, -3);

	return 1;
}

/** Initialize box.slab package. */
void
box_lua_slab_runtime_init(struct lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "box");

	lua_pushstring(L, "runtime");
	lua_newtable(L);

	lua_pushstring(L, "info");
	lua_pushcfunction(L, lbox_runtime_info);
	lua_settable(L, -3);

	lua_settable(L, -3); /* box.runtime */

	lua_pop(L, 1); /* box. */
}
