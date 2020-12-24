#pragma once
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
#include <small/small.h>

struct SmallAllocator
{
	static void create(struct slab_arena *arena,
		uint32_t objsize_min, float alloc_factor,
		float *actual_alloc_factor);
	static void destroy(void);
	static void enter_delayed_free_mode(void);
	static void leave_delayed_free_mode(void);
	static void stats(struct small_stats *stats, mempool_stats_cb cb, void *cb_ctx);
	static void memory_check(void);
	static inline void *alloc(size_t size) {
		return smalloc(&small_alloc, size);
	};
	static inline void free(void *ptr, size_t size) {
		smfree(&small_alloc, ptr, size);
	}
	static inline void free_delayed(void *ptr, size_t size) {
		smfree_delayed(&small_alloc, ptr, size);
	}

	/** Tuple allocator. */
	static struct small_alloc small_alloc;
	/** Slab cache for allocating tuples. */
	static struct slab_cache slab_cache;
};
