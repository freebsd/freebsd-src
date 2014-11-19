/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LINUX_GFP_H_
#define	_LINUX_GFP_H_

#include <sys/systm.h>
#include <sys/malloc.h>

#include <linux/page.h>

#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#define	__GFP_NOWARN	0
#define	__GFP_HIGHMEM	0
#define	__GFP_ZERO	M_ZERO

#define	GFP_NOWAIT	M_NOWAIT
#define	GFP_ATOMIC	(M_NOWAIT | M_USE_RESERVE)
#define	GFP_KERNEL	M_WAITOK
#define	GFP_USER	M_WAITOK
#define	GFP_HIGHUSER	M_WAITOK
#define	GFP_HIGHUSER_MOVABLE	M_WAITOK
#define	GFP_IOFS	M_NOWAIT

static inline void *
page_address(struct page *page)
{

	if (page->object != kmem_object && page->object != kernel_object)
		return (NULL);
	return ((void *)(uintptr_t)(VM_MIN_KERNEL_ADDRESS +
	    IDX_TO_OFF(page->pindex)));
}

static inline unsigned long
_get_page(gfp_t mask)
{

	return kmem_malloc(kmem_arena, PAGE_SIZE, mask);
}

#define	get_zeroed_page(mask)	_get_page((mask) | M_ZERO)
#define	alloc_page(mask)	virt_to_page(_get_page((mask)))
#define	__get_free_page(mask)	_get_page((mask))

static inline void
free_page(unsigned long page)
{

	if (page == 0)
		return;
	kmem_free(kmem_arena, page, PAGE_SIZE);
}

static inline void
__free_page(struct page *m)
{

	if (m->object != kmem_object)
		panic("__free_page:  Freed page %p not allocated via wrappers.",
		    m);
	kmem_free(kmem_arena, (vm_offset_t)page_address(m), PAGE_SIZE);
}

static inline void
__free_pages(struct page *m, unsigned int order)
{
	size_t size;

	if (m == NULL)
		return;
	size = PAGE_SIZE << order;
	kmem_free(kmem_arena, (vm_offset_t)page_address(m), size);
}

/*
 * Alloc pages allocates directly from the buddy allocator on linux so
 * order specifies a power of two bucket of pages and the results
 * are expected to be aligned on the size as well.
 */
static inline struct page *
alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	unsigned long page;
	size_t size;

	size = PAGE_SIZE << order;
	page = kmem_alloc_contig(kmem_arena, size, gfp_mask, 0, -1,
	    size, 0, VM_MEMATTR_DEFAULT);
	if (page == 0)
		return (NULL);
        return (virt_to_page(page));
}

#define alloc_pages_node(node, mask, order)     alloc_pages(mask, order)

#define kmalloc_node(chunk, mask, node)         kmalloc(chunk, mask)

#endif	/* _LINUX_GFP_H_ */
