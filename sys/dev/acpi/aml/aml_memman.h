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
 *	$Id: aml_memman.h,v 1.9 2000/08/09 14:47:43 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _MEMMAN_H_
#define _MEMMAN_H_

/*
 * Generic Memory Management
 */

#include <sys/param.h>
#include <sys/queue.h>

/* memory block */
struct	memman_block {
	LIST_ENTRY(memman_block) links;
	void		*block;
	unsigned	static_mem;	/* static memory or not */
	unsigned int	allocated;	/* number of allocated chunks */
	unsigned int	available;	/* number of available chunks */
	unsigned int	allocated_mem;	/* block + misc (in bytes) */

}__attribute__((packed));

LIST_HEAD(memman_block_list, memman_block);

/* memory node in block */
struct	memman_node {
	LIST_ENTRY(memman_node)	links;
	void	*node;
	struct	memman_block *memblock;
}__attribute__((packed));

LIST_HEAD(memman_node_list, memman_node);

/* memory type id */
extern unsigned int	memid_unkown;

/* memory block manager */
struct	memman_blockman {
	unsigned int	size;		/* size of chunk */
	unsigned	int available;	/* total # of available chunks */
	void		*initial_block;	/* initial memory storage */
	unsigned	initialized;	/* initialized or not */

	struct	memman_block_list block_list;
	struct	memman_node_list free_node_list;
	struct	memman_node_list occupied_node_list;
};

/* memory size histogram */
#define MEMMAN_HISTOGRAM_SIZE	20
struct	memman_histogram {
	int	mem_size;
	int	count;
};

/* flex size memory allocation info */
struct	memman_flexmem_info {
	LIST_ENTRY(memman_flexmem_info) links;
	void	*addr;
	size_t	mem_size;
}__attribute__((packed));

LIST_HEAD(memman_flexmem_info_list, memman_flexmem_info);

/* memory manager */
struct	memman {
	struct	memman_blockman	*blockman;
	unsigned int	max_memid;	/* max number of valid memid */

	/* fixed size memory blocks */
	unsigned int	alloc_called;	/* memman_alloc() calling */
	unsigned int	free_called;	/* memman_free() calling */
	unsigned int	salloc_called;	/* malloc() calling */
	unsigned int	sfree_called;	/* free() calling */
	size_t		required_mem;	/* total required memory (in bytes) */
	size_t		allocated_mem;	/* total malloc()ed memory */
	size_t		reclaimed_mem;	/* total free()ed memory */
	/* flex size memory blocks */
	unsigned int	flex_alloc_called; /* memman_alloc_flexsize() calling */
	unsigned int	flex_free_called;  /* memman_free_flexsize() calling */
	unsigned int	flex_salloc_called;/* malloc() calling */
	unsigned int	flex_sfree_called; /* free() calling */
	size_t		flex_required_mem; /* total required memory (in bytes) */
	size_t		flex_allocated_mem;/* total malloc()ed memory */
	size_t		flex_reclaimed_mem;/* total free()ed memory */
	size_t		flex_mem_size_min; /* min size of allocated memory */
	size_t		flex_mem_size_max; /* max size of allocated memory */
	size_t		flex_peak_mem_usage;/* memory usage at a peak period */

	/* stuff for more detailed statistical information */
	struct	memman_histogram *flex_mem_histogram;
	unsigned int	flex_mem_histogram_ptr;
	int		flex_mem_histogram_initial_tolerance;
	unsigned	flex_mem_initialized;
	struct	memman_flexmem_info_list flexmem_info_list;
};

#define MEMMAN_BLOCKNODE_SIZE(entries) sizeof(struct memman_block) + \
    sizeof(struct memman_node) * entries

#ifndef ROUNDUP_UNIT
#define ROUNDUP_UNIT	4
#endif

#if !defined(MEMMAN_INITIAL_SIZE) || MEMMAN_INITIAL_SIZE < 2048
#define MEMMAN_INITIAL_SIZE	2048
#endif

#if !defined(MEMMAN_INCR_SIZE) || MEMMAN_INCR_SIZE < 512
#define MEMMAN_INCR_SIZE	512
#endif

#define MEMMAN_INITIALSTORAGE_DESC(type, name) \
static struct { \
	char	blocknodes[MEMMAN_BLOCKNODE_SIZE(MEMMAN_INITIAL_SIZE)]; \
	type	realblock[MEMMAN_INITIAL_SIZE]; \
} name

#define MEMMAN_MEMBLOCK_DESC(size, initial_storage) \
	{ size, MEMMAN_INITIAL_SIZE, &initial_storage, 0 }

#define MEMMAN_MEMMANAGER_DESC(blockman, max_memid, histogram, tolerance) \
	{ blockman, max_memid, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	  0, 0, 0, histogram, 0, tolerance, 0}

void	*memman_alloc(struct memman *, unsigned int);
void	*memman_alloc_flexsize(struct memman *, size_t);
void	 memman_free(struct memman *, unsigned int, void *);
void	 memman_free_flexsize(struct memman *, void *);
void	 memman_freeall(struct memman *);
void	 memman_statistics(struct memman *);
size_t	 memman_memid2size(struct memman *, unsigned int);

#ifdef _KERNEL
#define MEMMAN_SYSMALLOC(size)	malloc(size, M_MEMMAN, M_WAITOK)
#define MEMMAN_SYSFREE(ptr)	free(ptr, M_MEMMAN)
#define MEMMAN_SYSABORT()	/* no abort in kernel */
#else /* !_KERNEL */
#define MEMMAN_SYSMALLOC(size)	malloc(size)
#define MEMMAN_SYSFREE(ptr)	free(ptr)
#define MEMMAN_SYSABORT()	abort()
#endif /* _KERNEL */

#endif /* !_MEMMAN_H_ */
