/*
 *  PowerPC version derived from arch/arm/mm/consistent.c
 *    Copyright (C) 2001 Dan Malek (dmalek@jlc.net)
 *
 *  linux/arch/arm/mm/consistent.c
 *
 *  Copyright (C) 2000 Russell King
 *
 * Consistent memory allocators.  Used for DMA devices that want to
 * share uncached memory with the processor core.  The function return
 * is the virtual address and 'dma_handle' is the physical address.
 * Mostly stolen from the ARM port, with some changes for PowerPC.
 *						-- Dan
 * Modified for 36-bit support.  -Matt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/pci.h>

#include <asm/pgalloc.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/uaccess.h>
#include <asm/smp.h>
#include <asm/machdep.h>

#include "mmu_decl.h"

extern int map_page(unsigned long va, phys_addr_t pa, int flags);

/* This function will allocate the requested contiguous pages and
 * map them into the kernel's vmalloc() space.  This is done so we
 * get unique mapping for these pages, outside of the kernel's 1:1
 * virtual:physical mapping.  This is necessary so we can cover large
 * portions of the kernel with single large page TLB entries, and
 * still get unique uncached pages for consistent DMA.
 */
void *consistent_alloc(int gfp, size_t size, dma_addr_t *dma_handle)
{
	int order, err, i;
	unsigned long page, va, flags;
	phys_addr_t pa;
	struct vm_struct *area;
	void	 *ret;

	if (in_interrupt())
		BUG();

	/* Only allocate page size areas.
	*/
	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = __get_free_pages(gfp, order);
	if (!page) {
		BUG();
		return NULL;
	}

	/*
	 * we need to ensure that there are no cachelines in use,
	 * or worse dirty in this area.
	 */
	invalidate_dcache_range(page, page + size);

	/* Allocate some common virtual space to map the new pages.
	*/
	area = get_vm_area(size, VM_ALLOC);
	if (area == 0) {
		free_pages(page, order);
		return NULL;
	}
	va = VMALLOC_VMADDR(area->addr);
	ret = (void *)va;

	/* This gives us the real physical address of the first page.
	*/
	*dma_handle = pa = virt_to_bus((void *)page);

	flags = _PAGE_KERNEL | _PAGE_NO_CACHE;

	/*
	 * Set refcount=1 on all pages in an order>0
	 * allocation so that vfree() will actually
	 * free all pages that were allocated. 
	 */
	if (order > 0)
	{
		struct page *rpage = virt_to_page(page);
		for (i = 1; i < (1 << order); i++)
			set_page_count(rpage+i, 1);
	}

	err = 0;
	for (i = 0; i < size && err == 0; i += PAGE_SIZE)
		err = map_page(va+i, pa+i, flags);
	
	if (err) {
		vfree((void *)va);
		return NULL;
	}

	return ret;
}

/*
 * free page(s) as defined by the above mapping.
 */
void consistent_free(void *vaddr)
{
	if (in_interrupt())
		BUG();
	vfree(vaddr);
}

/*
 * make an area consistent.
 */
void consistent_sync(void *vaddr, size_t size, int direction)
{
	unsigned long start = (unsigned long)vaddr;
	unsigned long end   = start + size;

	switch (direction) {
	case PCI_DMA_NONE:
		BUG();
	case PCI_DMA_FROMDEVICE:	/* invalidate only */
		invalidate_dcache_range(start, end);
		break;
	case PCI_DMA_TODEVICE:		/* writeback only */
		clean_dcache_range(start, end);
		break;
	case PCI_DMA_BIDIRECTIONAL:	/* writeback and invalidate */
		flush_dcache_range(start, end);
		break;
	}
}

/*
 * consistent_sync_page make a page are consistent. identical
 * to consistent_sync, but takes a struct page instead of a virtual address
 */

void consistent_sync_page(struct page *page, unsigned long offset,
size_t size, int direction)
{
	void *start;

	start = page_address(page) + offset; 
	consistent_sync(start, size, direction);
}
