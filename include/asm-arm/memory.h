/*
 *  linux/include/asm-arm/memory.h
 *
 *  Copyright (C) 2000-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Note: this file should not be included by non-asm/.h files
 */
#ifndef __ASM_ARM_MEMORY_H
#define __ASM_ARM_MEMORY_H

#include <linux/config.h>
#include <asm/arch/memory.h>

/*
 * PFNs are used to describe any physical page; this means
 * PFN 0 == physical address 0.
 *
 * This is the PFN of the first RAM page in the kernel
 * direct-mapped view.  We assume this is the first page
 * of RAM in the mem_map as well.
 */
#define PHYS_PFN_OFFSET	(PHYS_OFFSET >> PAGE_SHIFT)

/*
 * These are *only* valid on the kernel direct mapped RAM memory.
 */
static inline unsigned long virt_to_phys(void *x)
{
	return __virt_to_phys((unsigned long)(x));
}

static inline void *phys_to_virt(unsigned long x)
{
	return (void *)(__phys_to_virt((unsigned long)(x)));
}

#define __pa(x)			__virt_to_phys((unsigned long)(x))
#define __va(x)			((void *)__phys_to_virt((unsigned long)(x)))

/*
 * Virtual <-> DMA view memory address translations
 * Again, these are *only* valid on the kernel direct mapped RAM
 * memory.  Use of these is *depreciated*.
 */
#define virt_to_bus(x)		(__virt_to_bus((unsigned long)(x)))
#define bus_to_virt(x)		((void *)(__bus_to_virt((unsigned long)(x))))

/*
 * Conversion between a struct page and a physical address.
 *
 * Note: when converting an unknown physical address to a
 * struct page, the resulting pointer must be validated
 * using VALID_PAGE().  It must return an invalid struct page
 * for any physical address not corresponding to a system
 * RAM address.
 *
 *  page_to_pfn(page)	convert a struct page * to a PFN number
 *  pfn_to_page(pfn)	convert a _valid_ PFN number to struct page *
 *  pfn_valid(pfn)	indicates whether a PFN number is valid
 *
 *  virt_to_page(k)	convert a _valid_ virtual address to struct page *
 *  virt_addr_valid(k)	indicates whether a virtual address is valid
 */
#ifndef CONFIG_DISCONTIGMEM

#define page_to_pfn(page)	(((page) - mem_map) + PHYS_PFN_OFFSET)
#define pfn_to_page(pfn)	((mem_map + (pfn)) - PHYS_PFN_OFFSET)
#define pfn_valid(pfn)		((pfn) >= PHYS_PFN_OFFSET && (pfn) < (PHYS_PFN_OFFSET + max_mapnr))

#define virt_to_page(kaddr)	(pfn_to_page(__pa(kaddr) >> PAGE_SHIFT))
#define virt_addr_valid(kaddr)	((kaddr) >= PAGE_OFFSET && (kaddr) < (unsigned long)high_memory)

#define PHYS_TO_NID(addr)	(0)

#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)

#else

/*
 * This is more complex.  We have a set of mem_map arrays spread
 * around in memory.
 */
#define page_to_pfn(page)					\
	(((page) - page_zone(page)->zone_mem_map)		\
	  + (page_zone(page)->zone_start_paddr >> PAGE_SHIFT))

#define pfn_to_page(pfn)					\
	(PFN_TO_MAPBASE(pfn) + LOCAL_MAP_NR((pfn) << PAGE_SHIFT))

#define pfn_valid(pfn)						\
  ({								\
	unsigned int node = PFN_TO_NID(pfn);			\
	struct pglist_data *nd = NODE_DATA(node);		\
	((node < NR_NODES) &&					\
	 ((pfn - (nd->node_start_paddr >> PAGE_SHIFT)) < nd->node_size));\
   })

#define virt_to_page(kaddr)					\
	(ADDR_TO_MAPBASE(kaddr) + LOCAL_MAP_NR(kaddr))

#define virt_addr_valid(kaddr)	(KVADDR_TO_NID(kaddr) < NR_NODES)

/*
 * Common discontigmem stuff.
 *  PHYS_TO_NID is used by the ARM kernel/setup.c
 */
#define PHYS_TO_NID(addr)	PFN_TO_NID((addr) >> PAGE_SHIFT)

/*
 * 2.4 compatibility
 *
 * VALID_PAGE returns a non-zero value if given page pointer is valid.
 * This assumes all node's mem_maps are stored within the node they
 * refer to.  This is actually inherently buggy.
 */
#define VALID_PAGE(page) \
({ unsigned int node = KVADDR_TO_NID(page); \
   ((node < NR_NODES) && \
     ((unsigned)((page) - NODE_MEM_MAP(node)) < NODE_DATA(node)->node_size)); \
})

#endif

/*
 * For BIO.  "will die".  Kill me when bio_to_phys() and bvec_to_phys() die.
 */
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)

/*
 * We should really eliminate virt_to_bus() here - it's depreciated.
 */
#define page_to_bus(page)	(virt_to_bus(page_address(page)))

#endif
