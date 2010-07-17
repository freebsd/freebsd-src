/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
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

static inline unsigned long
get_zeroed_page(gfp_t mask)
{
	vm_page_t m;
	vm_offset_t p;

	p = kmem_malloc(kernel_map, PAGE_SIZE, mask | M_ZERO);
	if (p) {
		m = virt_to_page(p);
		m->flags |= PG_KVA;
		m->object = (vm_object_t)p;
	}
	return (p);
}

static inline void
free_page(unsigned long page)
{
	vm_page_t m;

	m = virt_to_page(page);
	if (m->flags & PG_KVA) {
		m->flags &= ~PG_KVA;
		m->object = kernel_object;
	}
	kmem_free(kernel_map, page, PAGE_SIZE);
}

static inline void
__free_pages(void *p, unsigned int order)
{
	unsigned long page;
	vm_page_t m;
	size_t size;

	size = order << PAGE_SHIFT;
	for (page = (uintptr_t)p; p < (uintptr_t)p + size; page += PAGE_SIZE) {
		m = virt_to_page(page);
		if (m->flags & PG_KVA) {
			m->flags &= ~PG_KVA;
			m->object = kernel_object;
		}
	}
	kmem_free(kernel_map, p, size);
}

static inline struct page *
alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	unsigned long start;
	unsigned long page;
	vm_page_t m;
	size_t size;

	size = order << PAGE_SHIFT;
	start = kmem_alloc_contig(kernel_map, size, gfp_mask, 0, -1,
	    PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
	if (start == 0)
		return (NULL);
	for (page = start; page < start + size; page += PAGE_SIZE) {
		m = virt_to_page(page);
		m->flags |= PG_KVA;
		m->object = (vm_object_t)page;
	}
        return (virt_to_page(start));
}

#endif	/* _LINUX_GFP_H_ */
