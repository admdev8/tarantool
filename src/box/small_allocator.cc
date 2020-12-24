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
#include "small_allocator.h"

void
SmallAllocator::create(struct slab_arena *arena,
		uint32_t objsize_min, float alloc_factor, float *actual_alloc_factor)
{
	slab_cache_create(&slab_cache, arena);
	small_alloc_create(&small_alloc, &slab_cache,
			   objsize_min, alloc_factor, actual_alloc_factor);
}

void
SmallAllocator::destroy(void)
{
	small_alloc_destroy(&small_alloc);
	slab_cache_destroy(&slab_cache);
}

void
SmallAllocator::enter_delayed_free_mode(void)
{
	small_alloc_setopt(&small_alloc, SMALL_DELAYED_FREE_MODE, true);
}

void
SmallAllocator::leave_delayed_free_mode(void)
{
	small_alloc_setopt(&small_alloc, SMALL_DELAYED_FREE_MODE, false);
}

void
SmallAllocator::stats(struct small_stats *stats, mempool_stats_cb cb, void *cb_ctx)
{
	small_stats(&small_alloc, stats, cb, cb_ctx);
}

void
SmallAllocator::memory_check(void)
{
	slab_cache_check(&slab_cache);
}

struct small_alloc SmallAllocator::small_alloc;
struct slab_cache SmallAllocator::slab_cache;
