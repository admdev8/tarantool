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
#include <pthread.h>
#include <trivia/util.h>
#include <trivia/config.h>
#include <small/slab_arena.h>
#include <small/quota.h>
#include <small/lifo.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

enum system_opt {
	SYSTEM_DELAYED_FREE_MODE
};

/**
 * Free mode
 */
enum system_free_mode {
	/** Free objects immediately. */
	SYSTEM_FREE,
	/** Collect garbage after delayed free. */
	SYSTEM_COLLECT_GARBAGE,
	/** Postpone deletion of objects. */
	SYSTEM_DELAYED_FREE,
};

struct system_alloc {
	/**
	 * Bytes allocated by system allocator
	 */
	uint64_t used_bytes;
	/**
	 * Arena free bytes
	 */
	uint64_t arena_bytes;
	/**
	 * Allocator arena
	 */
	struct slab_arena *arena;
	/**
	 * Allocator quota
	 */
	struct quota *quota;
	/**
	 * Free mode.
	 */
	enum system_free_mode free_mode;
	/**
	  * List of pointers for delayed free.
	  */
	struct lifo delayed;
	/**
	  * Allocator thread
	  */
	pthread_t allocator_thread;
};

struct system_stats {
	size_t used;
	size_t total;
};

typedef int (*system_stats_cb)(const struct system_stats *stats,
				void *cb_ctx);

/** Initialize a system memory allocator. */
void
system_alloc_create(struct system_alloc *alloc, struct slab_arena *arena);

/**
 * Enter or leave delayed mode - in delayed mode sysfree_delayed()
 * doesn't free memory but puts them into a list, for futher deletion.
 */
 void
system_alloc_setopt(struct system_alloc *alloc, enum system_opt opt, bool val);

/**
 * Destroy the allocator, the destruction of
 * all allocated memory is on the user's conscience.
 */
void
system_alloc_destroy(struct system_alloc *alloc);

/**
 * Allocate memory in the system allocator, using malloc.
 */
void *
sysalloc(struct system_alloc *alloc, size_t bytes);

/**
 * Free memory in the system allocator, using feee.
 */
void
sysfree(struct system_alloc *alloc, void *ptr, MAYBE_UNUSED size_t bytes);

/**
 * Free memory allocated by the system allocator
 * if not in snapshot mode, otherwise put to the delayed
 * free list.
 */
void
sysfree_delayed(struct system_alloc *alloc, void *ptr, size_t bytes);

/**
 * Get system allocator statistic
 */
void
system_stats(struct system_alloc *alloc, struct system_stats *totals,
	     system_stats_cb cb, void *cb_ctx);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
