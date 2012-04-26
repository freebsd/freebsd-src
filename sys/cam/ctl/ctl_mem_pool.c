/*-
 * Copyright (c) 2003, 2004 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_mem_pool.c#1 $
 */
/*
 * CAM Target Layer memory pool code.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/queue.h>

#include <cam/ctl/ctl_mem_pool.h>

MALLOC_DEFINE(M_CTL_POOL, "ctlpool", "CTL memory pool");

int
ctl_init_mem_pool(struct ctl_mem_pool *pool, int chunk_size,
		  ctl_mem_pool_flags flags, int grow_inc,
		  int initial_pool_size)
{
	pool->flags = flags;
	pool->chunk_size = chunk_size;
	pool->grow_inc = grow_inc;
	mtx_init(&pool->lock, "Pool mutex", NULL, MTX_DEF);
	STAILQ_INIT(&pool->free_mem_list);
	cv_init(&pool->wait_mem, "CTL mem pool");

	if (ctl_grow_mem_pool(pool, initial_pool_size, /*can_wait*/ 1) !=
	    initial_pool_size)
		return (1);
	else
		return (0);
}

struct ctl_mem_element *
ctl_alloc_mem_element(struct ctl_mem_pool *pool, int can_wait)
{
	struct ctl_mem_element *mem;

	for (;;) {
		mtx_lock(&pool->lock);

		mem = STAILQ_FIRST(&pool->free_mem_list);
		if (mem != NULL) {
			STAILQ_REMOVE(&pool->free_mem_list, mem,
				      ctl_mem_element, links);
			mem->flags = CTL_MEM_ELEMENT_PREALLOC;
		}
		mtx_unlock(&pool->lock);

		if (mem != NULL)
			return (mem);

		/*
		 * Grow the pool permanantly by the requested increment
		 * instead of temporarily.  This has the effect that
		 * whatever the high water mark of transactions is for
		 * this pool, we'll keep that much memory around.
		 */
		if (pool->flags & CTL_MEM_POOL_PERM_GROW) {
			if (ctl_grow_mem_pool(pool, pool->grow_inc,
					      can_wait) != 0)
				continue;
		}
		mem = (struct ctl_mem_element *)malloc(sizeof(*mem),
			M_CTL_POOL, can_wait ? M_WAITOK : M_NOWAIT);

		if (mem != NULL) {
			mem->flags = CTL_MEM_ELEMENT_NONE;
			mem->pool = pool;

			mem->bytes = malloc(pool->chunk_size, M_CTL_POOL,
				can_wait ?  M_WAITOK : M_NOWAIT);
			if (mem->bytes == NULL) {
				free(mem, M_CTL_POOL);
				mem = NULL;
			} else {
				return (mem);
			}
		}

		if (can_wait == 0)
			return (NULL);

		cv_wait_unlock(&pool->wait_mem, &pool->lock);
	}
}

void
ctl_free_mem_element(struct ctl_mem_element *mem)
{
	struct ctl_mem_pool *pool;

	pool = mem->pool;

	if (mem->flags & CTL_MEM_ELEMENT_PREALLOC) {
		mtx_lock(&pool->lock);
		STAILQ_INSERT_TAIL(&pool->free_mem_list, mem, links);
		mtx_unlock(&pool->lock);
		cv_broadcast(&pool->wait_mem);
	} else
		free(mem, M_CTL_POOL);
}

int
ctl_grow_mem_pool(struct ctl_mem_pool *pool, int count, int can_wait)
{
	int i;

	for (i = 0; i < count; i++) {
		struct ctl_mem_element *mem;

		mem = (struct ctl_mem_element *)malloc(sizeof(*mem),
			M_CTL_POOL, can_wait ? M_WAITOK : M_NOWAIT);

		if (mem == NULL)
			break;

		mem->bytes = malloc(pool->chunk_size, M_CTL_POOL, can_wait ?
				     M_WAITOK : M_NOWAIT);
		if (mem->bytes == NULL) {
			free(mem, M_CTL_POOL);
			break;
		}
		mem->flags = CTL_MEM_ELEMENT_PREALLOC;
		mem->pool = pool;
		mtx_lock(&pool->lock);
		STAILQ_INSERT_TAIL(&pool->free_mem_list, mem, links);
		mtx_unlock(&pool->lock);
	}

	return (i);
}

int
ctl_shrink_mem_pool(struct ctl_mem_pool *pool)
{
	struct ctl_mem_element *mem, *mem_next;

	mtx_lock(&pool->lock);
	for (mem = STAILQ_FIRST(&pool->free_mem_list); mem != NULL;
	     mem = mem_next) {
		mem_next = STAILQ_NEXT(mem, links);

		STAILQ_REMOVE(&pool->free_mem_list, mem, ctl_mem_element,
			      links);
		free(mem->bytes, M_CTL_POOL);
		free(mem, M_CTL_POOL);
	}
	mtx_unlock(&pool->lock);

	return (0);
}
