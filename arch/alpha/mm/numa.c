/*
 *  linux/arch/alpha/mm/numa.c
 *
 *  DISCONTIGMEM NUMA alpha support.
 *
 *  Copyright (C) 2001 Andrea Arcangeli <andrea@suse.de> SuSE
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/swap.h>
#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/blk.h>
#endif

#include <asm/hwrpb.h>
#include <asm/pgalloc.h>

plat_pg_data_t *plat_node_data[MAX_NUMNODES];
bootmem_data_t plat_node_bdata[MAX_NUMNODES];

#undef DEBUG_DISCONTIG
#ifdef DEBUG_DISCONTIG
#define DBGDCONT(args...) printk(args)
#else
#define DBGDCONT(args...)
#endif

#define PFN_UP(x)       (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)     ((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)     ((x) << PAGE_SHIFT)
#define for_each_mem_cluster(memdesc, cluster, i)		\
	for ((cluster) = (memdesc)->cluster, (i) = 0;		\
	     (i) < (memdesc)->numclusters; (i)++, (cluster)++)

static void __init show_mem_layout(void)
{
	struct memclust_struct * cluster;
	struct memdesc_struct * memdesc;
	int i;

	/* Find free clusters, and init and free the bootmem accordingly.  */
	memdesc = (struct memdesc_struct *)
	  (hwrpb->mddt_offset + (unsigned long) hwrpb);

	printk("Raw memory layout:\n");
	for_each_mem_cluster(memdesc, cluster, i) {
		printk(" memcluster %2d, usage %1lx, start %8lu, end %8lu\n",
		       i, cluster->usage, cluster->start_pfn,
		       cluster->start_pfn + cluster->numpages);
	}
}

static void __init
setup_memory_node(int nid, void *kernel_end)
{
	extern unsigned long mem_size_limit;
	struct memclust_struct * cluster;
	struct memdesc_struct * memdesc;
	unsigned long start_kernel_pfn, end_kernel_pfn;
	unsigned long bootmap_size, bootmap_pages, bootmap_start;
	unsigned long start, end;
	unsigned long node_pfn_start, node_pfn_end;
	unsigned long node_min_pfn, node_max_pfn;
	int i;
	unsigned long node_datasz = PFN_UP(sizeof(plat_pg_data_t));
	int show_init = 0;

	/* Find the bounds of current node */
	node_pfn_start = (NODE_MEM_START(nid)) >> PAGE_SHIFT;
	node_pfn_end = node_pfn_start + (NODE_MEM_SIZE(nid) >> PAGE_SHIFT);
	
	/* Find free clusters, and init and free the bootmem accordingly.  */
	memdesc = (struct memdesc_struct *)
	  (hwrpb->mddt_offset + (unsigned long) hwrpb);

	/* find the bounds of this node (node_min_pfn/node_max_pfn) */
	node_min_pfn = ~0UL;
	node_max_pfn = 0UL;
	for_each_mem_cluster(memdesc, cluster, i) {
		/* Bit 0 is console/PALcode reserved.  Bit 1 is
		   non-volatile memory -- we might want to mark
		   this for later.  */
		if (cluster->usage & 3)
			continue;

		start = cluster->start_pfn;
		end = start + cluster->numpages;

		if (start >= node_pfn_end || end <= node_pfn_start)
			continue;

		if (!show_init) {
			show_init = 1;
			printk("Initialing bootmem allocator on Node ID %d\n", nid);
		}
		printk(" memcluster %2d, usage %1lx, start %8lu, end %8lu\n",
		       i, cluster->usage, cluster->start_pfn,
		       cluster->start_pfn + cluster->numpages);

		if (start < node_pfn_start)
			start = node_pfn_start;
		if (end > node_pfn_end)
			end = node_pfn_end;

		if (start < node_min_pfn)
			node_min_pfn = start;
		if (end > node_max_pfn)
			node_max_pfn = end;
	}

	if (mem_size_limit && node_max_pfn > mem_size_limit) {
		static int msg_shown = 0;
		if (!msg_shown) {
			msg_shown = 1;
			printk("setup: forcing memory size to %ldK (from %ldK).\n",
			       mem_size_limit << (PAGE_SHIFT - 10),
			       node_max_pfn   << (PAGE_SHIFT - 10));
		}
		node_max_pfn = mem_size_limit;
	}

	if (node_min_pfn >= node_max_pfn)
		return;

	/* Update global {min,max}_low_pfn from node information. */
	if (node_min_pfn < min_low_pfn)
		min_low_pfn = node_min_pfn;
	if (node_max_pfn > max_low_pfn)
		max_low_pfn = node_max_pfn;

	num_physpages += node_max_pfn - node_min_pfn;

	/* Cute trick to make sure our local node data is on local memory */
	PLAT_NODE_DATA(nid) = (plat_pg_data_t *)(__va(node_min_pfn << PAGE_SHIFT));
	/* Quasi-mark the plat_pg_data_t as in-use */
	node_min_pfn += node_datasz;
	if (node_min_pfn >= node_max_pfn) {
		printk(" not enough mem to reserve PLAT_NODE_DATA");
		return;
	}
	NODE_DATA(nid)->bdata = &plat_node_bdata[nid];

	printk(" Detected node memory:   start %8lu, end %8lu\n",
	       node_min_pfn, node_max_pfn);

	DBGDCONT(" DISCONTIG: plat_node_data[%d]   is at 0x%p\n", nid, PLAT_NODE_DATA(nid));
	DBGDCONT(" DISCONTIG: NODE_DATA(%d)->bdata is at 0x%p\n", nid, NODE_DATA(nid)->bdata);

	/* Find the bounds of kernel memory.  */
	start_kernel_pfn = PFN_DOWN(KERNEL_START_PHYS);
	end_kernel_pfn = PFN_UP(virt_to_phys(kernel_end));
	bootmap_start = -1;

	if (!nid && (node_max_pfn < end_kernel_pfn || node_min_pfn > start_kernel_pfn))
		panic("kernel loaded out of ram");

	/* Zone start phys-addr must be 2^(MAX_ORDER-1) aligned */
	node_min_pfn = (node_min_pfn + ((1UL << (MAX_ORDER-1))-1)) & ~((1UL << (MAX_ORDER-1))-1);

	/* We need to know how many physically contiguous pages
	   we'll need for the bootmap.  */
	bootmap_pages = bootmem_bootmap_pages(node_max_pfn-node_min_pfn);

	/* Now find a good region where to allocate the bootmap.  */
	for_each_mem_cluster(memdesc, cluster, i) {
		if (cluster->usage & 3)
			continue;

		start = cluster->start_pfn;
		end = start + cluster->numpages;

		if (start >= node_max_pfn || end <= node_min_pfn)
			continue;

		if (end > node_max_pfn)
			end = node_max_pfn;
		if (start < node_min_pfn)
			start = node_min_pfn;

		if (start < start_kernel_pfn) {
			if (end > end_kernel_pfn
			    && end - end_kernel_pfn >= bootmap_pages) {
				bootmap_start = end_kernel_pfn;
				break;
			} else if (end > start_kernel_pfn)
				end = start_kernel_pfn;
		} else if (start < end_kernel_pfn)
			start = end_kernel_pfn;
		if (end - start >= bootmap_pages) {
			bootmap_start = start;
			break;
		}
	}

	if (bootmap_start == -1)
		panic("couldn't find a contigous place for the bootmap");

	/* Allocate the bootmap and mark the whole MM as reserved.  */
	bootmap_size = init_bootmem_node(NODE_DATA(nid), bootmap_start,
					 node_min_pfn, node_max_pfn);
	DBGDCONT(" bootmap_start %lu, bootmap_size %lu, bootmap_pages %lu\n",
		 bootmap_start, bootmap_size, bootmap_pages);

	/* Mark the free regions.  */
	for_each_mem_cluster(memdesc, cluster, i) {
		if (cluster->usage & 3)
			continue;

		start = cluster->start_pfn;
		end = cluster->start_pfn + cluster->numpages;

		if (start >= node_max_pfn || end <= node_min_pfn)
			continue;

		if (end > node_max_pfn)
			end = node_max_pfn;
		if (start < node_min_pfn)
			start = node_min_pfn;

		if (start < start_kernel_pfn) {
			if (end > end_kernel_pfn) {
				free_bootmem_node(NODE_DATA(nid), PFN_PHYS(start),
					     (PFN_PHYS(start_kernel_pfn)
					      - PFN_PHYS(start)));
				printk(" freeing pages %ld:%ld\n",
				       start, start_kernel_pfn);
				start = end_kernel_pfn;
			} else if (end > start_kernel_pfn)
				end = start_kernel_pfn;
		} else if (start < end_kernel_pfn)
			start = end_kernel_pfn;
		if (start >= end)
			continue;

		free_bootmem_node(NODE_DATA(nid), PFN_PHYS(start), PFN_PHYS(end) - PFN_PHYS(start));
		printk(" freeing pages %ld:%ld\n", start, end);
	}

	/* Reserve the bootmap memory.  */
	reserve_bootmem_node(NODE_DATA(nid), PFN_PHYS(bootmap_start), bootmap_size);
	printk(" reserving pages %ld:%ld\n", bootmap_start, bootmap_start+PFN_UP(bootmap_size));

	numnodes++;
}

void __init
setup_memory(void *kernel_end)
{
	int nid;

	show_mem_layout();

	numnodes = 0;

	min_low_pfn = ~0UL;
	max_low_pfn = 0UL;
	for (nid = 0; nid < MAX_NUMNODES; nid++)
		setup_memory_node(nid, kernel_end);

#ifdef CONFIG_BLK_DEV_INITRD
	initrd_start = INITRD_START;
	if (initrd_start) {
		extern void *move_initrd(unsigned long);

		initrd_end = initrd_start+INITRD_SIZE;
		printk("Initial ramdisk at: 0x%p (%lu bytes)\n",
		       (void *) initrd_start, INITRD_SIZE);

		if ((void *)initrd_end > phys_to_virt(PFN_PHYS(max_low_pfn))) {
			if (!move_initrd(PFN_PHYS(max_low_pfn)))
				printk("initrd extends beyond end of memory "
				       "(0x%08lx > 0x%p)\ndisabling initrd\n",
				       initrd_end,
				       phys_to_virt(PFN_PHYS(max_low_pfn)));
		} else {
			reserve_bootmem_node(NODE_DATA(KVADDR_TO_NID(initrd_start)),
					     virt_to_phys((void *)initrd_start),
					     INITRD_SIZE);
		}
	}
#endif /* CONFIG_BLK_DEV_INITRD */
}

void __init paging_init(void)
{
	unsigned int    nid;
	unsigned long   zones_size[MAX_NR_ZONES] = {0, };
	unsigned long	dma_local_pfn;

	/*
	 * The old global MAX_DMA_ADDRESS per-arch API doesn't fit
	 * in the NUMA model, for now we convert it to a pfn and
	 * we interpret this pfn as a local per-node information.
	 * This issue isn't very important since none of these machines
	 * have legacy ISA slots anyways.
	 */
	dma_local_pfn = virt_to_phys((char *)MAX_DMA_ADDRESS) >> PAGE_SHIFT;

	for (nid = 0; nid < numnodes; nid++) {
		unsigned long start_pfn = plat_node_bdata[nid].node_boot_start >> PAGE_SHIFT;
		unsigned long end_pfn = plat_node_bdata[nid].node_low_pfn;
		unsigned long lmax_mapnr;

		if (dma_local_pfn >= end_pfn - start_pfn)
			zones_size[ZONE_DMA] = end_pfn - start_pfn;
		else {
			zones_size[ZONE_DMA] = dma_local_pfn;
			zones_size[ZONE_NORMAL] = (end_pfn - start_pfn) - dma_local_pfn;
		}
		free_area_init_node(nid, NODE_DATA(nid), NULL, zones_size, start_pfn<<PAGE_SHIFT, NULL);
		lmax_mapnr = PLAT_NODE_DATA_STARTNR(nid) + PLAT_NODE_DATA_SIZE(nid);
		if (lmax_mapnr > max_mapnr) {
			max_mapnr = lmax_mapnr;
			DBGDCONT("Grow max_mapnr to %ld\n", max_mapnr);
		}
	}

	/* Initialize the kernel's ZERO_PGE. */
	memset((void *)ZERO_PGE, 0, PAGE_SIZE);
}

#define printkdot()					\
do {							\
	if (!(i++ % ((100UL*1024*1024)>>PAGE_SHIFT)))	\
		printk(".");				\
} while(0)

#define clobber(p, size) memset(page_address(p), 0xaa, (size))

void __init mem_stress(void)
{
	LIST_HEAD(x);
	LIST_HEAD(xx);
	struct page * p;
	unsigned long i = 0;

	printk("starting memstress");
	while ((p = alloc_pages(GFP_ATOMIC, 1))) {
		clobber(p, PAGE_SIZE*2);
		list_add(&p->list, &x);
		printkdot();
	}
	while ((p = alloc_page(GFP_ATOMIC))) {
		clobber(p, PAGE_SIZE);
		list_add(&p->list, &xx);
		printkdot();
	}
	while (!list_empty(&x)) {
		p = list_entry(x.next, struct page, list);
		clobber(p, PAGE_SIZE*2);
		list_del(x.next);
		__free_pages(p, 1);
		printkdot();
	}
	while (!list_empty(&xx)) {
		p = list_entry(xx.next, struct page, list);
		clobber(p, PAGE_SIZE);
		list_del(xx.next);
		__free_pages(p, 0);
		printkdot();
	}
	printk("I'm still alive duh!\n");
}

#undef printkdot
#undef clobber

void __init mem_init(void)
{
	unsigned long codesize, reservedpages, datasize, initsize, pfn;
	extern int page_is_ram(unsigned long) __init;
	extern char _text, _etext, _data, _edata;
	extern char __init_begin, __init_end;
	extern unsigned long totalram_pages;
	unsigned long nid, i;
	mem_map_t * lmem_map;

	high_memory = (void *) __va(max_mapnr <<PAGE_SHIFT);

	reservedpages = 0;
	for (nid = 0; nid < numnodes; nid++) {
		/*
		 * This will free up the bootmem, ie, slot 0 memory
		 */
		totalram_pages += free_all_bootmem_node(NODE_DATA(nid));

		lmem_map = NODE_MEM_MAP(nid);
		pfn = NODE_DATA(nid)->node_start_paddr >> PAGE_SHIFT;
		for (i = 0; i < PLAT_NODE_DATA_SIZE(nid); i++, pfn++)
			if (page_is_ram(pfn) && PageReserved(lmem_map+i))
				reservedpages++;
	}

	codesize =  (unsigned long) &_etext - (unsigned long) &_text;
	datasize =  (unsigned long) &_edata - (unsigned long) &_data;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	printk("Memory: %luk/%luk available (%luk kernel code, %luk reserved, "
	       "%luk data, %luk init)\n",
	       (unsigned long)nr_free_pages() << (PAGE_SHIFT-10),
	       num_physpages << (PAGE_SHIFT-10),
	       codesize >> 10,
	       reservedpages << (PAGE_SHIFT-10),
	       datasize >> 10,
	       initsize >> 10);
#if 0
	mem_stress();
#endif
}

void
show_mem(void)
{
	long i,free = 0,total = 0,reserved = 0;
	long shared = 0, cached = 0;
	int nid;

	printk("\nMem-info:\n");
	show_free_areas();
	printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));
	for (nid = 0; nid < numnodes; nid++) {
		mem_map_t * lmem_map = NODE_MEM_MAP(nid);
		i = PLAT_NODE_DATA_SIZE(nid);
		while (i-- > 0) {
			total++;
			if (PageReserved(lmem_map+i))
				reserved++;
			else if (PageSwapCache(lmem_map+i))
				cached++;
			else if (!page_count(lmem_map+i))
				free++;
			else
				shared += atomic_read(&lmem_map[i].count) - 1;
		}
	}
	printk("%ld pages of RAM\n",total);
	printk("%ld free pages\n",free);
	printk("%ld reserved pages\n",reserved);
	printk("%ld pages shared\n",shared);
	printk("%ld pages swap cached\n",cached);
	printk("%ld pages in page table cache\n",pgtable_cache_size);
	show_buffers();
}
