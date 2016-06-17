/* 
 * Generic VM initialization for x86-64 NUMA setups.
 * Copyright 2002 Andi Kleen, SuSE Labs.
 * $Id: numa.c,v 1.6 2003/04/03 12:28:08 ak Exp $
 */ 
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/blk.h>
#include <asm/e820.h>
#include <asm/proto.h>
#include <asm/dma.h>

#undef Dprintk
#define Dprintk(...) 

plat_pg_data_t *plat_node_data[MAXNODE];
bootmem_data_t plat_node_bdata[MAX_NUMNODES];

#define ZONE_ALIGN (1UL << (MAX_ORDER+PAGE_SHIFT))

static int numa_off __initdata; 

unsigned long nodes_present; 
int maxnode;

/* Initialize bootmem allocator for a node */
void __init setup_node_bootmem(int nodeid, unsigned long start, unsigned long end)
{ 
	unsigned long start_pfn, end_pfn, bootmap_pages, bootmap_size, bootmap_start; 
	unsigned long nodedata_phys;
	const int pgdat_size = round_up(sizeof(plat_pg_data_t), PAGE_SIZE);

	start = round_up(start, ZONE_ALIGN); 

	printk("Bootmem setup node %d %016lx-%016lx\n", nodeid, start, end);

	start_pfn = start >> PAGE_SHIFT;
	end_pfn = end >> PAGE_SHIFT;

	nodedata_phys = find_e820_area(start, end, pgdat_size); 
	if (nodedata_phys == -1L) 
		panic("Cannot find memory pgdat in node %d\n", nodeid);

	Dprintk("nodedata_phys %lx\n", nodedata_phys); 

	PLAT_NODE_DATA(nodeid) = phys_to_virt(nodedata_phys);
	memset(PLAT_NODE_DATA(nodeid), 0, sizeof(plat_pg_data_t));
	NODE_DATA(nodeid)->bdata = &plat_node_bdata[nodeid];

	/* Find a place for the bootmem map */
	bootmap_pages = bootmem_bootmap_pages(end_pfn - start_pfn); 
	bootmap_start = round_up(nodedata_phys + pgdat_size, PAGE_SIZE);
	bootmap_start = find_e820_area(bootmap_start, end, bootmap_pages<<PAGE_SHIFT);
	if (bootmap_start == -1L) 
		panic("Not enough continuous space for bootmap on node %d", nodeid); 
	Dprintk("bootmap start %lu pages %lu\n", bootmap_start, bootmap_pages); 
	
	bootmap_size = init_bootmem_node(NODE_DATA(nodeid),
					 bootmap_start >> PAGE_SHIFT, 
					 start_pfn, end_pfn); 

	e820_bootmem_free(NODE_DATA(nodeid), start, end);

	reserve_bootmem_node(NODE_DATA(nodeid), nodedata_phys, pgdat_size); 
	reserve_bootmem_node(NODE_DATA(nodeid), bootmap_start, bootmap_pages<<PAGE_SHIFT);

	PLAT_NODE_DATA(nodeid)->start_pfn = start_pfn;
	PLAT_NODE_DATA(nodeid)->end_pfn = end_pfn;

	if (nodeid > maxnode) 
		maxnode = nodeid;
	nodes_present |= (1UL << nodeid); 
} 

/* Initialize final allocator for a zone */
void __init setup_node_zones(int nodeid)
{ 
	unsigned long start_pfn, end_pfn; 
	unsigned long zones[MAX_NR_ZONES];
	unsigned long dma_end_pfn;
	unsigned long lmax_mapnr;

	memset(zones, 0, sizeof(unsigned long) * MAX_NR_ZONES); 

	start_pfn = PLAT_NODE_DATA(nodeid)->start_pfn; 
	end_pfn = PLAT_NODE_DATA(nodeid)->end_pfn; 

	printk("setting up node %d %lx-%lx\n", nodeid, start_pfn, end_pfn); 
	
	/* All nodes > 0 have a zero length zone DMA */ 
	dma_end_pfn = __pa(MAX_DMA_ADDRESS) >> PAGE_SHIFT; 
	if (start_pfn < dma_end_pfn) { 
		zones[ZONE_DMA] = dma_end_pfn - start_pfn;
		zones[ZONE_NORMAL] = end_pfn - dma_end_pfn; 
	} else { 
		zones[ZONE_NORMAL] = end_pfn - start_pfn; 
	} 
    
	free_area_init_node(nodeid, NODE_DATA(nodeid), NULL, zones, 
			    start_pfn<<PAGE_SHIFT, NULL); 
	lmax_mapnr = PLAT_NODE_DATA_STARTNR(nodeid) + PLAT_NODE_DATA_SIZE(nodeid);
	if (lmax_mapnr > max_mapnr)
		max_mapnr = lmax_mapnr;
} 

int fake_node;

int __init numa_initmem_init(unsigned long start_pfn, unsigned long end_pfn)
{ 
#ifdef CONFIG_K8_NUMA
	if (!numa_off && !k8_scan_nodes(start_pfn<<PAGE_SHIFT, end_pfn<<PAGE_SHIFT))
		return 0; 
#endif
	printk(KERN_INFO "%s\n",
	       numa_off ? "NUMA turned off" : "No NUMA configuration found");
		   
	printk(KERN_INFO "Faking a node at %016lx-%016lx\n", 
	       start_pfn << PAGE_SHIFT,
	       end_pfn << PAGE_SHIFT); 
	/* setup dummy node covering all memory */ 
	fake_node = 1; 	
	memnode_shift = 63; 
	memnodemap[0] = 0;
	setup_node_bootmem(0, start_pfn<<PAGE_SHIFT, end_pfn<<PAGE_SHIFT);
	return -1; 
} 

#define for_all_nodes(x) for ((x) = 0; (x) <= maxnode; (x)++) \
				if ((1UL << (x)) & nodes_present)

unsigned long __init numa_free_all_bootmem(void) 
{ 
	int i;
	unsigned long pages = 0;
	for_all_nodes(i) {
		pages += free_all_bootmem_node(NODE_DATA(i));
	}
	return pages;
} 

void __init paging_init(void)
{ 
	int i;
	for_all_nodes(i) { 
		setup_node_zones(i); 
	}
} 

void show_mem(void)
{
	long i,free = 0,total = 0,reserved = 0;
	long shared = 0, cached = 0;
	int nid;

	printk("\nMem-info:\n");
	show_free_areas();
	printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));
	for_all_nodes (nid) { 
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
	show_buffers();
}

/* [numa=off] */
__init int numa_setup(char *opt) 
{ 
	if (!strncmp(opt,"off",3))
		numa_off = 1;
	return 1;
} 

__setup("numa=", numa_setup); 

