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
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_mem_pool.h#1 $
 * $FreeBSD$
 */
/*
 * CAM Target Layer memory pool code.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#ifndef	_CTL_MEMPOOL_H_
#define	_CTL_MEMPOOL_H_

typedef enum {
	CTL_MEM_POOL_NONE,
	CTL_MEM_POOL_PERM_GROW
} ctl_mem_pool_flags;

struct ctl_mem_pool {
	ctl_mem_pool_flags flags;
	int chunk_size;
	int grow_inc;
	struct mtx lock;
        struct cv wait_mem;
	STAILQ_HEAD(, ctl_mem_element) free_mem_list;
};

typedef enum {
	CTL_MEM_ELEMENT_NONE,
	CTL_MEM_ELEMENT_PREALLOC
} ctl_mem_element_flags;

struct ctl_mem_element {
	ctl_mem_element_flags flags;
	struct ctl_mem_pool *pool;
	uint8_t *bytes;
	STAILQ_ENTRY(ctl_mem_element) links;
};

#ifdef	_KERNEL

MALLOC_DECLARE(M_CTL_POOL);

int ctl_init_mem_pool(struct ctl_mem_pool *pool, int chunk_size,
		      ctl_mem_pool_flags flags, int grow_inc,
		      int initial_pool_size);
struct ctl_mem_element *ctl_alloc_mem_element(struct ctl_mem_pool *pool,
					      int can_wait);
void ctl_free_mem_element(struct ctl_mem_element *mem);
int ctl_grow_mem_pool(struct ctl_mem_pool *pool, int count,
		      int can_wait);
int ctl_shrink_mem_pool(struct ctl_mem_pool *pool);
#endif	/* _KERNEL */

#endif	/* _CTL_MEMPOOL_H_ */
