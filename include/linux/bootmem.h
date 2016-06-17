/*
 * Discontiguous memory support, Kanoj Sarcar, SGI, Nov 1999
 */
#ifndef _LINUX_BOOTMEM_H
#define _LINUX_BOOTMEM_H

#include <asm/pgtable.h>
#include <asm/dma.h>
#include <linux/cache.h>
#include <linux/init.h>
#include <linux/mmzone.h>

/*
 *  simple boot-time physical memory area allocator.
 */

extern unsigned long max_low_pfn;
extern unsigned long min_low_pfn;
extern unsigned long max_pfn;

/*
 * node_bootmem_map is a map pointer - the bits represent all physical 
 * memory pages (including holes) on the node.
 */
typedef struct bootmem_data {
	unsigned long node_boot_start;
	unsigned long node_low_pfn;
	void *node_bootmem_map;
	unsigned long last_offset;
	unsigned long last_pos;
} bootmem_data_t;

extern unsigned long __init bootmem_bootmap_pages (unsigned long);
extern unsigned long __init init_bootmem (unsigned long addr, unsigned long memend);
extern void __init reserve_bootmem (unsigned long addr, unsigned long size);
extern void __init free_bootmem (unsigned long addr, unsigned long size);
extern void * __init __alloc_bootmem (unsigned long size, unsigned long align, unsigned long goal);
#define alloc_bootmem(x) \
	__alloc_bootmem((x), SMP_CACHE_BYTES, __pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_low(x) \
	__alloc_bootmem((x), SMP_CACHE_BYTES, 0)
#define alloc_bootmem_pages(x) \
	__alloc_bootmem((x), PAGE_SIZE, __pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_low_pages(x) \
	__alloc_bootmem((x), PAGE_SIZE, 0)
extern unsigned long __init free_all_bootmem (void);

extern unsigned long __init init_bootmem_node (pg_data_t *pgdat, unsigned long freepfn, unsigned long startpfn, unsigned long endpfn);
extern void __init reserve_bootmem_node (pg_data_t *pgdat, unsigned long physaddr, unsigned long size);
extern void __init free_bootmem_node (pg_data_t *pgdat, unsigned long addr, unsigned long size);
extern unsigned long __init free_all_bootmem_node (pg_data_t *pgdat);
extern void * __init __alloc_bootmem_node (pg_data_t *pgdat, unsigned long size, unsigned long align, unsigned long goal);
#define alloc_bootmem_node(pgdat, x) \
	__alloc_bootmem_node((pgdat), (x), SMP_CACHE_BYTES, __pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_pages_node(pgdat, x) \
	__alloc_bootmem_node((pgdat), (x), PAGE_SIZE, __pa(MAX_DMA_ADDRESS))
#define alloc_bootmem_low_pages_node(pgdat, x) \
	__alloc_bootmem_node((pgdat), (x), PAGE_SIZE, 0)

#endif /* _LINUX_BOOTMEM_H */
