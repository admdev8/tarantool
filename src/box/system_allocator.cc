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
#include "system_allocator.h"

void
SystemAllocator::create(struct slab_arena *arena)
{
	system_alloc_create(&system_alloc, arena);
}

void
SystemAllocator::destroy(void)
{
	system_alloc_destroy(&system_alloc);
}

void
SystemAllocator::enter_delayed_free_mode(void)
{
	system_alloc_setopt(&system_alloc, SYSTEM_DELAYED_FREE_MODE, true);
}

void
SystemAllocator::leave_delayed_free_mode(void)
{
	system_alloc_setopt(&system_alloc, SYSTEM_DELAYED_FREE_MODE, false);
}

void
SystemAllocator::stats(struct system_stats *stats, system_stats_cb cb, void *cb_ctx)
{
	system_stats(&system_alloc, stats, cb, cb_ctx);
}

void
SystemAllocator::memory_check(void)
{
}

struct system_alloc SystemAllocator::system_alloc;
