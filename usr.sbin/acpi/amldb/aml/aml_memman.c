/*-
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: aml_memman.c,v 1.10 2000/08/09 14:47:43 iwasaki Exp $
 *	$FreeBSD$
 */

/*
 * Generic Memory Management
 */

#include <sys/param.h>

#include <aml/aml_memman.h>

#ifndef _KERNEL
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#else /* _KERNEL */
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
MALLOC_DEFINE(M_MEMMAN, "memman", "Generic and Simple Memory Management");
#endif /* !_KERNEL */

unsigned int	memid_unkown = 255;

static int		 manage_block(struct memman *memman, unsigned int id,
				      void *block, unsigned static_mem,
				      unsigned entries);
static int		 blockman_init(struct memman *memman, unsigned int id);
static void		 memman_flexsize_add_histogram(struct memman *memman,
						       size_t size,
						       int tolerance);
static int		 memman_comp_histogram_size(const void *a,
						    const void *b);
static void		 memman_sort_histogram_by_size(struct memman *memman);
static unsigned int	 memman_guess_memid(struct memman *memman, void *chunk);
static void		 memman_statistics_fixedsize(struct memman *memman);
static void		 memman_statistics_flexsize(struct memman *memman);

static int
manage_block(struct memman *memman, unsigned int id, void *block,
    unsigned static_mem, unsigned entries)
{
	unsigned int	i;
	size_t	alloc_size;
	void	*tmp, *realblock;
	struct	memman_blockman	*bmp;
	struct	memman_block *memblock;
	struct	memman_node *memnodes;

	bmp = &memman->blockman[id];
	alloc_size = MEMMAN_BLOCKNODE_SIZE(entries);

	if (static_mem) {
		tmp = (void *)block;
		realblock = (char *)block + alloc_size;
	} else {
		tmp = MEMMAN_SYSMALLOC(alloc_size);
		if (!tmp) {
			return (-1);
		}
		realblock = block;

		memman->allocated_mem += alloc_size;
		memman->salloc_called++;
	}

	memblock = (struct memman_block *)tmp;
	memnodes = (struct memman_node *)((char *)tmp + sizeof(struct memman_block));

	memblock->block = realblock;
	memblock->static_mem = static_mem;
	memblock->allocated = entries;
	memblock->available = entries;
	if (!static_mem) {
		alloc_size += roundup(bmp->size * entries, ROUNDUP_UNIT);
	}
	memblock->allocated_mem = alloc_size;
	LIST_INSERT_HEAD(&bmp->block_list, memblock, links);

	for (i = 0; i < entries; ++i) {
		memnodes[i].node = ((char *)realblock + (i * (bmp->size)));
		memnodes[i].memblock = memblock;
		LIST_INSERT_HEAD(&bmp->free_node_list, &memnodes[i], links);
	}
	bmp->available = entries;

	return (0);
}

static int
blockman_init(struct memman *memman, unsigned int id)
{
	int	status;
	struct	memman_blockman *bmp;

	bmp = &memman->blockman[id];
	bmp->initialized = 1;
	LIST_INIT(&bmp->block_list);
	LIST_INIT(&bmp->free_node_list);
	LIST_INIT(&bmp->occupied_node_list);
	status = manage_block(memman, id, bmp->initial_block,
	    1, MEMMAN_INITIAL_SIZE);
	return (status);
}

void *
memman_alloc(struct memman *memman, unsigned int id)
{
	size_t	alloc_size;
	void	*chunk, *block;
	struct	memman_blockman *bmp;
	struct	memman_node *memnode;

	if (memman->max_memid <= id) {
		printf("memman_alloc: invalid memory type id\n");
		return (NULL);
	}
	bmp = &memman->blockman[id];
	if (!bmp->initialized) {
		if (blockman_init(memman, id)) {
			goto malloc_fail;
		}
	}
	memman->alloc_called++;

	if (bmp->available == 0) {
		alloc_size = roundup(bmp->size * MEMMAN_INCR_SIZE,
		    ROUNDUP_UNIT);
		block = MEMMAN_SYSMALLOC(alloc_size);
		if (!block) {
			goto malloc_fail;
		}
		memman->required_mem += bmp->size * MEMMAN_INCR_SIZE;
		memman->allocated_mem += alloc_size;
		memman->salloc_called++;

		if (manage_block(memman, id, block, 0, MEMMAN_INCR_SIZE)) {
			goto malloc_fail;
		}
	}
	memnode = LIST_FIRST(&bmp->free_node_list);
	LIST_REMOVE(memnode, links);
	chunk = memnode->node;
	LIST_INSERT_HEAD(&bmp->occupied_node_list, memnode, links);
	memnode->memblock->available--;
	bmp->available--;

	return (chunk);

malloc_fail:
	printf("memman_alloc: could not allocate memory\n");
	return (NULL);
}

static void
memman_flexsize_add_histogram(struct memman *memman, size_t size,
    int tolerance)
{
	int	i;
	int	gap;

	if (size == 0) {
		return;
	}
	for (i = 0; i < memman->flex_mem_histogram_ptr; i++) {
		gap = memman->flex_mem_histogram[i].mem_size - size;
		if (gap >= (tolerance * -1) && gap <= tolerance) {
			memman->flex_mem_histogram[i].count++;
			if (memman->flex_mem_histogram[i].mem_size < size) {
				memman->flex_mem_histogram[i].mem_size = size;
			}
			return;
		}
	}

	if (memman->flex_mem_histogram_ptr == MEMMAN_HISTOGRAM_SIZE) {
		memman_flexsize_add_histogram(memman, size, tolerance + 1);
		return;
	}
	i = memman->flex_mem_histogram_ptr;
	memman->flex_mem_histogram[i].mem_size = size;
	memman->flex_mem_histogram[i].count = 1;
	memman->flex_mem_histogram_ptr++;
}

static int
memman_comp_histogram_size(const void *a, const void *b)
{
	int	delta;

	delta = ((const struct memman_histogram *)a)->mem_size -
	    ((const struct memman_histogram *)b)->mem_size;
	return (delta);
}

static void
memman_sort_histogram_by_size(struct memman *memman)
{
	qsort(memman->flex_mem_histogram, memman->flex_mem_histogram_ptr,
	    sizeof(struct memman_histogram), memman_comp_histogram_size);
}

void *
memman_alloc_flexsize(struct memman *memman, size_t size)
{
	void	*mem;
	struct	memman_flexmem_info *info;

	if (size == 0) {
		return (NULL);
	}
	if ((mem = MEMMAN_SYSMALLOC(size)) != NULL) {	/* XXX */

		info = MEMMAN_SYSMALLOC(sizeof(struct memman_flexmem_info));
		if (info) {
			if (!memman->flex_mem_initialized) {
				LIST_INIT(&memman->flexmem_info_list);
				bzero(memman->flex_mem_histogram,
				    sizeof(struct memman_histogram));
				memman->flex_mem_initialized = 1;
			}
			info->addr = mem;
			info->mem_size = size;
			LIST_INSERT_HEAD(&memman->flexmem_info_list, info, links);
		}
		memman->flex_alloc_called++;
		memman->flex_salloc_called++;
		memman->flex_required_mem += size;
		memman->flex_allocated_mem += size;
		if (memman->flex_mem_size_min == 0 ||
		    memman->flex_mem_size_min > size) {
			memman->flex_mem_size_min = size;
		}
		if (memman->flex_mem_size_max < size) {
			memman->flex_mem_size_max = size;
		}
		if (memman->flex_peak_mem_usage <
		    (memman->flex_allocated_mem - memman->flex_reclaimed_mem)) {
			memman->flex_peak_mem_usage =
			    (memman->flex_allocated_mem - memman->flex_reclaimed_mem);
		}
		memman_flexsize_add_histogram(memman, size,
		    memman->flex_mem_histogram_initial_tolerance);
	}
	return (mem);
}

static unsigned int
memman_guess_memid(struct memman *memman, void *chunk)
{
	unsigned int	id;
	struct	memman_blockman *bmp;
	struct	memman_node *memnode;

	for (id = 0; id < memman->max_memid; id++) {
		bmp = &memman->blockman[id];
		if (!bmp->initialized) {
			if (blockman_init(memman, id)) {
				printf("memman_free: could not initialized\n");
			}
		}
		LIST_FOREACH(memnode, &bmp->occupied_node_list, links) {
			if (memnode->node == chunk) {
				return (id);	/* got it! */
			}
		}
	}
	return (memid_unkown);	/* gave up */
}

void
memman_free(struct memman *memman, unsigned int memid, void *chunk)
{
	unsigned int	id;
	unsigned	found;
	void	*block;
	struct	memman_blockman *bmp;
	struct	memman_block *memblock;
	struct	memman_node *memnode;

	id = memid;
	if (memid == memid_unkown) {
		id = memman_guess_memid(memman, chunk);
	}
	if (memman->max_memid <= id) {
		printf("memman_free: invalid memory type id\n");
		MEMMAN_SYSABORT();
		return;
	}
	bmp = &memman->blockman[id];
	if (!bmp->initialized) {
		if (blockman_init(memman, id)) {
			printf("memman_free: could not initialized\n");
		}
	}
	found = 0;
	LIST_FOREACH(memnode, &bmp->occupied_node_list, links) {
		if (memnode->node == chunk) {
			found = 1;
			break;
		}
	}
	if (!found) {
		printf("memman_free: invalid address\n");
		return;
	}
	memman->free_called++;

	LIST_REMOVE(memnode, links);
	memblock = memnode->memblock;
	memblock->available++;
	LIST_INSERT_HEAD(&bmp->free_node_list, memnode, links);
	bmp->available++;

	if (!memblock->static_mem &&
	    memblock->available == memblock->allocated) {
		LIST_FOREACH(memnode, &bmp->free_node_list, links) {
			if (memnode->memblock != memblock) {
				continue;
			}
			LIST_REMOVE(memnode, links);
			bmp->available--;
		}
		block = memblock->block;
		MEMMAN_SYSFREE(block);
		memman->sfree_called++;

		LIST_REMOVE(memblock, links);
		memman->sfree_called++;
		memman->reclaimed_mem += memblock->allocated_mem;
		MEMMAN_SYSFREE(memblock);
	}
}

void
memman_free_flexsize(struct memman *memman, void *chunk)
{
	struct	memman_flexmem_info *info;

	LIST_FOREACH(info, &memman->flexmem_info_list, links) {
		if (info->addr == chunk) {
			memman->flex_reclaimed_mem += info->mem_size;
			LIST_REMOVE(info, links);
			MEMMAN_SYSFREE(info);
			break;
		}
	}
	/* XXX */
	memman->flex_free_called++;
	memman->flex_sfree_called++;
	MEMMAN_SYSFREE(chunk);
}

void
memman_freeall(struct memman *memman)
{
	int	id;
	void	*chunk;
	struct	memman_blockman *bmp;
	struct	memman_node *memnode;
	struct	memman_block *memblock;
	struct	memman_flexmem_info *info;

	for (id = 0; id < memman->max_memid; id++) {
		bmp = &memman->blockman[id];

		while ((memnode = LIST_FIRST(&bmp->occupied_node_list))) {
			chunk = memnode->node;
			printf("memman_freeall: fixed size (id = %d)\n", id);
			memman_free(memman, id, chunk);
		}
		while ((memblock = LIST_FIRST(&bmp->block_list))) {
			LIST_REMOVE(memblock, links);
			if (!memblock->static_mem) {
				memman->sfree_called++;
				memman->reclaimed_mem += memblock->allocated_mem;
				MEMMAN_SYSFREE(memblock);
			}
		}
		bmp->initialized = 0;
	}

	LIST_FOREACH(info, &memman->flexmem_info_list, links) {
		printf("memman_freeall: flex size (size = %d, addr = %p)\n",
		    info->mem_size, info->addr);
		memman_free_flexsize(memman, info->addr);
	}
}

static void
memman_statistics_fixedsize(struct memman *memman)
{
	printf("  fixed size memory blocks\n");
	printf("    alloc():		%d times\n", memman->alloc_called);
	printf("    system malloc():	%d times\n", memman->salloc_called);
	printf("    free():		%d times\n", memman->free_called);
	printf("    system free():	%d times\n", memman->sfree_called);
	printf("    required memory:	%d bytes\n", memman->required_mem);
	printf("    allocated memory:	%d bytes\n", memman->allocated_mem);
	printf("    reclaimed memory:	%d bytes\n", memman->reclaimed_mem);
}

static void
memman_statistics_flexsize(struct memman *memman)
{
	int	i;

	printf("  flexible size memory blocks\n");
	printf("    alloc():		%d times\n", memman->flex_alloc_called);
	printf("    system malloc():	%d times\n", memman->flex_salloc_called);
	printf("    free():		%d times\n", memman->flex_free_called);
	printf("    system free():	%d times\n", memman->flex_sfree_called);
	printf("    required memory:	%d bytes\n", memman->flex_required_mem);
	printf("    allocated memory:	%d bytes\n", memman->flex_allocated_mem);
	printf("    reclaimed memory:	%d bytes\n", memman->flex_reclaimed_mem);
	printf("    peak memory usage:	%d bytes\n", memman->flex_peak_mem_usage);
	printf("    min memory size:	%d bytes\n", memman->flex_mem_size_min);
	printf("    max memory size:	%d bytes\n", memman->flex_mem_size_max);
	printf("    avg memory size:	%d bytes\n",
	    (memman->flex_alloc_called) ?
	    memman->flex_allocated_mem / memman->flex_alloc_called : 0);

	printf("    memory size histogram (%d entries):\n",
	    memman->flex_mem_histogram_ptr);
	printf("	size	count\n");
	memman_sort_histogram_by_size(memman);
	for (i = 0; i < memman->flex_mem_histogram_ptr; i++) {
		printf("	%d	%d\n",
		    memman->flex_mem_histogram[i].mem_size,
		    memman->flex_mem_histogram[i].count);
	}
}

void
memman_statistics(struct memman *memman)
{
	printf("memman: reporting statistics\n");
	memman_statistics_fixedsize(memman);
	memman_statistics_flexsize(memman);
}

size_t
memman_memid2size(struct memman *memman, unsigned int id)
{
	if (memman->max_memid <= id) {
		printf("memman_alloc: invalid memory type id\n");
		return (0);
	}
	return (memman->blockman[id].size);
}
