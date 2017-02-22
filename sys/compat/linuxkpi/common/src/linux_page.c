/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2016 Matt Macy (mmacy@nextbsd.org)
 * Copyright (c) 2017 Mellanox Technologies, Ltd.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/sched.h>

#include <machine/bus.h>

#include <linux/gfp.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

void *
linux_page_address(struct page *page)
{
#ifdef __amd64__
	return ((void *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(page)));
#else
	if (page->object != kmem_object && page->object != kernel_object)
		return (NULL);
	return ((void *)(uintptr_t)(VM_MIN_KERNEL_ADDRESS +
	    IDX_TO_OFF(page->pindex)));
#endif
}

vm_page_t
linux_alloc_pages(gfp_t flags, unsigned int order)
{
#ifdef __amd64__
	unsigned long npages = 1UL << order;
	int req = (flags & M_ZERO) ? (VM_ALLOC_ZERO | VM_ALLOC_NOOBJ |
	    VM_ALLOC_NORMAL) : (VM_ALLOC_NOOBJ | VM_ALLOC_NORMAL);
	vm_page_t page;

	if (order == 0 && (flags & GFP_DMA32) == 0) {
		page = vm_page_alloc(NULL, 0, req);
		if (page == NULL)
			return (NULL);
	} else {
		vm_paddr_t pmax = (flags & GFP_DMA32) ?
		    BUS_SPACE_MAXADDR_32BIT : BUS_SPACE_MAXADDR;
retry:
		page = vm_page_alloc_contig(NULL, 0, req,
		    npages, 0, pmax, PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);

		if (page == NULL) {
			if (flags & M_WAITOK) {
				if (!vm_page_reclaim_contig(req,
				    npages, 0, pmax, PAGE_SIZE, 0)) {
					VM_WAIT;
				}
				flags &= ~M_WAITOK;
				goto retry;
			}
			return (NULL);
		}
	}
	if (flags & M_ZERO) {
		unsigned long x;

		for (x = 0; x != npages; x++) {
			vm_page_t pgo = page + x;

			if ((pgo->flags & PG_ZERO) == 0)
				pmap_zero_page(pgo);
		}
	}
#else
	vm_offset_t vaddr;
	vm_page_t page;

	vaddr = linux_alloc_kmem(flags, order);
	if (vaddr == 0)
		return (NULL);

	page = PHYS_TO_VM_PAGE(vtophys((void *)vaddr));

	KASSERT(vaddr == (vm_offset_t)page_address(page),
	    ("Page address mismatch"));
#endif
	return (page);
}

void
linux_free_pages(vm_page_t page, unsigned int order)
{
#ifdef __amd64__
	unsigned long npages = 1UL << order;
	unsigned long x;

	for (x = 0; x != npages; x++) {
		vm_page_t pgo = page + x;

		vm_page_lock(pgo);
		vm_page_free(pgo);
		vm_page_unlock(pgo);
	}
#else
	vm_offset_t vaddr;

	vaddr = (vm_offset_t)page_address(page);

	linux_free_kmem(vaddr, order);
#endif
}

vm_offset_t
linux_alloc_kmem(gfp_t flags, unsigned int order)
{
	size_t size = ((size_t)PAGE_SIZE) << order;
	vm_offset_t addr;

	if ((flags & GFP_DMA32) == 0) {
		addr = kmem_malloc(kmem_arena, size, flags & GFP_NATIVE_MASK);
	} else {
		addr = kmem_alloc_contig(kmem_arena, size,
		    flags & GFP_NATIVE_MASK, 0, BUS_SPACE_MAXADDR_32BIT,
		    PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
	}
	return (addr);
}

void
linux_free_kmem(vm_offset_t addr, unsigned int order)
{
	size_t size = ((size_t)PAGE_SIZE) << order;

	kmem_free(kmem_arena, addr, size);
}
