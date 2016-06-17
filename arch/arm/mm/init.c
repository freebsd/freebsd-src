/*
 *  linux/arch/arm/mm/init.c
 *
 *  Copyright (C) 1995-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
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
#include <linux/swapctl.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/blk.h>

#include <asm/segment.h>
#include <asm/mach-types.h>
#include <asm/pgalloc.h>
#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#ifndef CONFIG_DISCONTIGMEM
#define NR_NODES	1
#else
#define NR_NODES	4
#endif

#ifdef CONFIG_CPU_32
#define TABLE_OFFSET	(PTRS_PER_PTE)
#else
#define TABLE_OFFSET	0
#endif

#define TABLE_SIZE	((TABLE_OFFSET + PTRS_PER_PTE) * sizeof(pte_t))

static unsigned long totalram_pages;
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern char _stext, _text, _etext, _end, __init_begin, __init_end;
extern unsigned long phys_initrd_start;
extern unsigned long phys_initrd_size;

/*
 * The sole use of this is to pass memory configuration
 * data from paging_init to mem_init.
 */
static struct meminfo meminfo __initdata = { 0, };

/*
 * empty_zero_page is a special page that is used for
 * zero-initialized data and COW.
 */
struct page *empty_zero_page;

#ifndef CONFIG_NO_PGT_CACHE
struct pgtable_cache_struct quicklists;

int do_check_pgt_cache(int low, int high)
{
	int freed = 0;

	if(pgtable_cache_size > high) {
		do {
			if(pgd_quicklist) {
				free_pgd_slow(get_pgd_fast());
				freed++;
			}
			if(pmd_quicklist) {
				pmd_free_slow(pmd_alloc_one_fast(NULL, 0));
				freed++;
			}
			if(pte_quicklist) {
				pte_free_slow(pte_alloc_one_fast(NULL, 0));
				freed++;
			}
		} while(pgtable_cache_size > low);
	}
	return freed;
}
#else
int do_check_pgt_cache(int low, int high)
{
	return 0;
}
#endif

/* This is currently broken
 * PG_skip is used on sparc/sparc64 architectures to "skip" certain
 * parts of the address space.
 *
 * #define PG_skip	10
 * #define PageSkip(page) (machine_is_riscpc() && test_bit(PG_skip, &(page)->flags))
 *			if (PageSkip(page)) {
 *				page = page->next_hash;
 *				if (page == NULL)
 *					break;
 *			}
 */
void show_mem(void)
{
	int free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0, slab = 0, node;

	printk("Mem-info:\n");
	show_free_areas();
	printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));

	for (node = 0; node < numnodes; node++) {
		struct page *page, *end;

		page = NODE_MEM_MAP(node);
		end  = page + NODE_DATA(node)->node_size;

		do {
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (PageSlab(page))
				slab++;
			else if (!page_count(page))
				free++;
			else
				shared += atomic_read(&page->count) - 1;
			page++;
		} while (page < end);
	}

	printk("%d pages of RAM\n", total);
	printk("%d free pages\n", free);
	printk("%d reserved pages\n", reserved);
	printk("%d slab pages\n", slab);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
#ifndef CONFIG_NO_PGT_CACHE
	printk("%ld page tables cached\n", pgtable_cache_size);
#endif
	show_buffers();
}

struct node_info {
	unsigned int start;
	unsigned int end;
	int bootmap_pages;
};

#define O_PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define V_PFN_DOWN(x)	O_PFN_DOWN(__pa(x))

#define O_PFN_UP(x)	(PAGE_ALIGN(x) >> PAGE_SHIFT)
#define V_PFN_UP(x)	O_PFN_UP(__pa(x))

#define PFN_SIZE(x)	((x) >> PAGE_SHIFT)
#define PFN_RANGE(s,e)	PFN_SIZE(PAGE_ALIGN((unsigned long)(e)) - \
				(((unsigned long)(s)) & PAGE_MASK))

/*
 * FIXME: We really want to avoid allocating the bootmap bitmap
 * over the top of the initrd.  Hopefully, this is located towards
 * the start of a bank, so if we allocate the bootmap bitmap at
 * the end, we won't clash.
 */
static unsigned int __init
find_bootmap_pfn(int node, struct meminfo *mi, unsigned int bootmap_pages)
{
	unsigned int start_pfn, bank, bootmap_pfn;

	start_pfn   = V_PFN_UP(&_end);
	bootmap_pfn = 0;

	for (bank = 0; bank < mi->nr_banks; bank ++) {
		unsigned int start, end;

		if (mi->bank[bank].node != node)
			continue;

		start = O_PFN_UP(mi->bank[bank].start);
		end   = O_PFN_DOWN(mi->bank[bank].size +
				   mi->bank[bank].start);

		if (end < start_pfn)
			continue;

		if (start < start_pfn)
			start = start_pfn;

		if (end <= start)
			continue;

		if (end - start >= bootmap_pages) {
			bootmap_pfn = start;
			break;
		}
	}

	if (bootmap_pfn == 0)
		BUG();

	return bootmap_pfn;
}

/*
 * Scan the memory info structure and pull out:
 *  - the end of memory
 *  - the number of nodes
 *  - the pfn range of each node
 *  - the number of bootmem bitmap pages
 */
static unsigned int __init
find_memend_and_nodes(struct meminfo *mi, struct node_info *np)
{
	unsigned int i, bootmem_pages = 0, memend_pfn = 0;

	for (i = 0; i < NR_NODES; i++) {
		np[i].start = -1U;
		np[i].end = 0;
		np[i].bootmap_pages = 0;
	}

	for (i = 0; i < mi->nr_banks; i++) {
		unsigned long start, end;
		int node;

		if (mi->bank[i].size == 0) {
			/*
			 * Mark this bank with an invalid node number
			 */
			mi->bank[i].node = -1;
			continue;
		}

		node = mi->bank[i].node;

		if (node >= numnodes) {
			numnodes = node + 1;

			/*
			 * Make sure we haven't exceeded the maximum number
			 * of nodes that we have in this configuration.  If
			 * we have, we're in trouble.  (maybe we ought to
			 * limit, instead of bugging?)
			 */
			if (numnodes > NR_NODES)
				BUG();
		}

		/*
		 * Get the start and end pfns for this bank
		 */
		start = O_PFN_UP(mi->bank[i].start);
		end   = O_PFN_DOWN(mi->bank[i].start + mi->bank[i].size);

		if (np[node].start > start)
			np[node].start = start;

		if (np[node].end < end)
			np[node].end = end;

		if (memend_pfn < end)
			memend_pfn = end;
	}

	/*
	 * Calculate the number of pages we require to
	 * store the bootmem bitmaps.
	 */
	for (i = 0; i < numnodes; i++) {
		if (np[i].end == 0)
			continue;

		np[i].bootmap_pages = bootmem_bootmap_pages(np[i].end -
							    np[i].start);
		bootmem_pages += np[i].bootmap_pages;
	}

	/*
	 * This doesn't seem to be used by the Linux memory
	 * manager any more.  If we can get rid of it, we
	 * also get rid of some of the stuff above as well.
	 */
	max_low_pfn = memend_pfn - O_PFN_DOWN(PHYS_OFFSET);
//	max_pfn = memend_pfn - O_PFN_DOWN(PHYS_OFFSET);
	mi->end = memend_pfn << PAGE_SHIFT;

	return bootmem_pages;
}

static int __init check_initrd(struct meminfo *mi)
{
	int initrd_node = -2;

#ifdef CONFIG_BLK_DEV_INITRD
	unsigned long end = phys_initrd_start + phys_initrd_size;

	/*
	 * Make sure that the initrd is within a valid area of
	 * memory.
	 */
	if (phys_initrd_size) {
		unsigned int i;

		initrd_node = -1;

		for (i = 0; i < mi->nr_banks; i++) {
			unsigned long bank_end;

			bank_end = mi->bank[i].start + mi->bank[i].size;

			if (mi->bank[i].start <= phys_initrd_start &&
			    end <= bank_end)
				initrd_node = mi->bank[i].node;
		}
	}

	if (initrd_node == -1) {
		printk(KERN_ERR "initrd (0x%08lx - 0x%08lx) extends beyond "
		       "physical memory - disabling initrd\n",
		       phys_initrd_start, end);
		phys_initrd_start = phys_initrd_size = 0;
	}
#endif

	return initrd_node;
}

/*
 * Reserve the various regions of node 0
 */
static __init void reserve_node_zero(unsigned int bootmap_pfn, unsigned int bootmap_pages)
{
	pg_data_t *pgdat = NODE_DATA(0);

	/*
	 * Register the kernel text and data with bootmem.
	 * Note that this can only be in node 0.
	 */
	reserve_bootmem_node(pgdat, __pa(&_stext), &_end - &_stext);

#ifdef CONFIG_CPU_32
	/*
	 * Reserve the page tables.  These are already in use,
	 * and can only be in node 0.
	 */
	reserve_bootmem_node(pgdat, __pa(swapper_pg_dir),
			     PTRS_PER_PGD * sizeof(pgd_t));
#endif
	/*
	 * And don't forget to reserve the allocator bitmap,
	 * which will be freed later.
	 */
	reserve_bootmem_node(pgdat, bootmap_pfn << PAGE_SHIFT,
			     bootmap_pages << PAGE_SHIFT);

	/*
	 * Hmm... This should go elsewhere, but we really really
	 * need to stop things allocating the low memory; we need
	 * a better implementation of GFP_DMA which does not assume
	 * that DMA-able memory starts at zero.
	 */
	if (machine_is_integrator())
		reserve_bootmem_node(pgdat, 0, __pa(swapper_pg_dir));
	/*
	 * These should likewise go elsewhere.  They pre-reserve
	 * the screen memory region at the start of main system
	 * memory.
	 */
	if (machine_is_archimedes() || machine_is_a5k())
		reserve_bootmem_node(pgdat, 0x02000000, 0x00080000);
	if (machine_is_edb7211() || machine_is_fortunet())
		reserve_bootmem_node(pgdat, 0xc0000000, 0x00020000);
	if (machine_is_p720t())
		reserve_bootmem_node(pgdat, PHYS_OFFSET, 0x00014000);
#ifdef CONFIG_SA1111
	/*
	 * Because of the SA1111 DMA bug, we want to preserve
	 * our precious DMA-able memory...
	 */
	reserve_bootmem_node(pgdat, PHYS_OFFSET, __pa(swapper_pg_dir)-PHYS_OFFSET);
#endif
}

/*
 * Register all available RAM in this node with the bootmem allocator.
 */
static inline void free_bootmem_node_bank(int node, struct meminfo *mi)
{
	pg_data_t *pgdat = NODE_DATA(node);
	int bank;

	for (bank = 0; bank < mi->nr_banks; bank++)
		if (mi->bank[bank].node == node)
			free_bootmem_node(pgdat, mi->bank[bank].start,
					  mi->bank[bank].size);
}

/*
 * Initialise the bootmem allocator for all nodes.  This is called
 * early during the architecture specific initialisation.
 */
void __init bootmem_init(struct meminfo *mi)
{
	struct node_info node_info[NR_NODES], *np = node_info;
	unsigned int bootmap_pages, bootmap_pfn, map_pg;
	int node, initrd_node;

	bootmap_pages = find_memend_and_nodes(mi, np);
	bootmap_pfn   = find_bootmap_pfn(0, mi, bootmap_pages);
	initrd_node   = check_initrd(mi);

	map_pg = bootmap_pfn;

	/*
	 * Initialise the bootmem nodes.
	 *
	 * What we really want to do is:
	 *
	 *   unmap_all_regions_except_kernel();
	 *   for_each_node_in_reverse_order(node) {
	 *     map_node(node);
	 *     allocate_bootmem_map(node);
	 *     init_bootmem_node(node);
	 *     free_bootmem_node(node);
	 *   }
	 *
	 * but this is a 2.5-type change.  For now, we just set
	 * the nodes up in reverse order.
	 *
	 * (we could also do with rolling bootmem_init and paging_init
	 * into one generic "memory_init" type function).
	 */
	np += numnodes - 1;
	for (node = numnodes - 1; node >= 0; node--, np--) {
		/*
		 * If there are no pages in this node, ignore it.
		 * Note that node 0 must always have some pages.
		 */
		if (np->end == 0) {
			if (node == 0)
				BUG();
			continue;
		}

		/*
		 * Initialise the bootmem allocator.
		 */
		init_bootmem_node(NODE_DATA(node), map_pg, np->start, np->end);
		free_bootmem_node_bank(node, mi);
		map_pg += np->bootmap_pages;

		/*
		 * If this is node 0, we need to reserve some areas ASAP -
		 * we may use bootmem on node 0 to setup the other nodes.
		 */
		if (node == 0)
			reserve_node_zero(bootmap_pfn, bootmap_pages);
	}


#ifdef CONFIG_BLK_DEV_INITRD
	if (phys_initrd_size && initrd_node >= 0) {
		reserve_bootmem_node(NODE_DATA(initrd_node), phys_initrd_start,
				     phys_initrd_size);
		initrd_start = __phys_to_virt(phys_initrd_start);
		initrd_end = initrd_start + phys_initrd_size;
	}
#endif

	if (map_pg != bootmap_pfn + bootmap_pages)
		BUG();

}

/*
 * paging_init() sets up the page tables, initialises the zone memory
 * maps, and sets up the zero page, bad page and bad page tables.
 */
void __init paging_init(struct meminfo *mi, struct machine_desc *mdesc)
{
	void *zero_page;
	int node;

	memcpy(&meminfo, mi, sizeof(meminfo));

	/*
	 * allocate the zero page.  Note that we count on this going ok.
	 */
	zero_page = alloc_bootmem_low_pages(PAGE_SIZE);

	/*
	 * initialise the page tables.
	 */
	memtable_init(mi);
	if (mdesc->map_io)
		mdesc->map_io();
	flush_cache_all();
	flush_tlb_all();

	/*
	 * initialise the zones within each node
	 */
	for (node = 0; node < numnodes; node++) {
		unsigned long zone_size[MAX_NR_ZONES];
		unsigned long zhole_size[MAX_NR_ZONES];
		struct bootmem_data *bdata;
		pg_data_t *pgdat;
		int i;

		/*
		 * Initialise the zone size information.
		 */
		for (i = 0; i < MAX_NR_ZONES; i++) {
			zone_size[i]  = 0;
			zhole_size[i] = 0;
		}

		pgdat = NODE_DATA(node);
		bdata = pgdat->bdata;

		/*
		 * The size of this node has already been determined.
		 * If we need to do anything fancy with the allocation
		 * of this memory to the zones, now is the time to do
		 * it.
		 */
		zone_size[0] = bdata->node_low_pfn -
				(bdata->node_boot_start >> PAGE_SHIFT);

		/*
		 * If this zone has zero size, skip it.
		 */
		if (!zone_size[0])
			continue;

		/*
		 * For each bank in this node, calculate the size of the
		 * holes.  holes = node_size - sum(bank_sizes_in_node)
		 */
		zhole_size[0] = zone_size[0];
		for (i = 0; i < mi->nr_banks; i++) {
			if (mi->bank[i].node != node)
				continue;

			zhole_size[0] -= mi->bank[i].size >> PAGE_SHIFT;
		}

		/*
		 * Adjust the sizes according to any special
		 * requirements for this machine type.
		 */
		arch_adjust_zones(node, zone_size, zhole_size);

		free_area_init_node(node, pgdat, 0, zone_size,
				bdata->node_boot_start, zhole_size);
	}

	/*
	 * finish off the bad pages once
	 * the mem_map is initialised
	 */
	memzero(zero_page, PAGE_SIZE);
	empty_zero_page = virt_to_page(zero_page);
	flush_dcache_page(empty_zero_page);
}

static inline void free_area(unsigned long addr, unsigned long end, char *s)
{
	unsigned int size = (end - addr) >> 10;

	for (; addr < end; addr += PAGE_SIZE) {
		struct page *page = virt_to_page(addr);
		ClearPageReserved(page);
		set_page_count(page, 1);
		free_page(addr);
		totalram_pages++;
	}

	if (size && s)
		printk(KERN_INFO "Freeing %s memory: %dK\n", s, size);
}

/*
 * mem_init() marks the free areas in the mem_map and tells us how much
 * memory is free.  This is done after various parts of the system have
 * claimed their memory after the kernel image.
 */
void __init mem_init(void)
{
	unsigned int codepages, datapages, initpages;
	int i, node;

	codepages = &_etext - &_text;
	datapages = &_end - &_etext;
	initpages = &__init_end - &__init_begin;

	high_memory = (void *)__va(meminfo.end);
	max_mapnr   = virt_to_page(high_memory) - mem_map;

	/*
	 * We may have non-contiguous memory.
	 */
	if (meminfo.nr_banks != 1)
		create_memmap_holes(&meminfo);

	/* this will put all unused low memory onto the freelists */
	for (node = 0; node < numnodes; node++) {
		pg_data_t *pgdat = NODE_DATA(node);

		if (pgdat->node_size != 0)
			totalram_pages += free_all_bootmem_node(pgdat);
	}

#ifdef CONFIG_SA1111
	/* now that our DMA memory is actually so designated, we can free it */
	free_area(PAGE_OFFSET, (unsigned long)swapper_pg_dir, NULL);
#endif

	/*
	 * Since our memory may not be contiguous, calculate the
	 * real number of pages we have in this system
	 */
	printk(KERN_INFO "Memory:");

	num_physpages = 0;
	for (i = 0; i < meminfo.nr_banks; i++) {
		num_physpages += meminfo.bank[i].size >> PAGE_SHIFT;
		printk(" %ldMB", meminfo.bank[i].size >> 20);
	}

	printk(" = %luMB total\n", num_physpages >> (20 - PAGE_SHIFT));
	printk(KERN_NOTICE "Memory: %luKB available (%dK code, "
		"%dK data, %dK init)\n",
		(unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
		codepages >> 10, datapages >> 10, initpages >> 10);

	if (PAGE_SIZE >= 16384 && num_physpages <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get
		 * anywhere without overcommit, so turn
		 * it on by default.
		 */
		sysctl_overcommit_memory = 1;
	}
}

void free_initmem(void)
{
	if (!machine_is_integrator()) {
		free_area((unsigned long)(&__init_begin),
			  (unsigned long)(&__init_end),
			  "init");
	}
}

#ifdef CONFIG_BLK_DEV_INITRD

static int keep_initrd;

void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (!keep_initrd)
		free_area(start, end, "initrd");
}

static int __init keepinitrd_setup(char *__unused)
{
	keep_initrd = 1;
	return 1;
}

__setup("keepinitrd", keepinitrd_setup);
#endif

void si_meminfo(struct sysinfo *val)
{
	val->totalram  = totalram_pages;
	val->sharedram = 0;
	val->freeram   = nr_free_pages();
	val->bufferram = atomic_read(&buffermem_pages);
	val->totalhigh = 0;
	val->freehigh  = 0;
	val->mem_unit  = PAGE_SIZE;
}
