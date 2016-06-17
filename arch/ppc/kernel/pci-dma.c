/*
 * Copyright (C) 2000   Ani Joshi <ajoshi@unixbox.com>
 *
 *
 * Dynamic DMA mapping support.
 *
 * swiped from i386
 *
 */

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <asm/io.h>

void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	void *ret;
	int gfp = GFP_ATOMIC;

	if (hwdev == NULL || hwdev->dma_mask < 0xffffffff)
		gfp |= GFP_DMA;

#ifdef CONFIG_NOT_COHERENT_CACHE
	ret = consistent_alloc(gfp, size, dma_handle);
#else
	ret = (void *)__get_free_pages(gfp, get_order(size));
#endif

	if (ret != NULL) {
		memset(ret, 0, size);
#ifndef CONFIG_NOT_COHERENT_CACHE
		*dma_handle = virt_to_bus(ret);
#endif
	}
	return ret;
}

void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
#ifdef CONFIG_NOT_COHERENT_CACHE
	consistent_free(vaddr);
#else
	free_pages((unsigned long)vaddr, get_order(size));
#endif
}
