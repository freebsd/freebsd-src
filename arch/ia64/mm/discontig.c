/*
 * Copyright (c) 2000, 2003 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 Tony Luck <tony.luck@intel.com>
 * Copyright (c) 2002 NEC Corp.
 * Copyright (c) 2002 Kimio Suganuma <k-suganuma@da.jp.nec.com>
 */

/*
 * Platform initialization for Discontig Memory
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/acpi.h>
#include <linux/efi.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>


/*
 * Round an address upward to the next multiple of GRANULE size.
 */
#define GRANULEROUNDDOWN(n) ((n) & ~(IA64_GRANULE_SIZE-1))
#define GRANULEROUNDUP(n) (((n)+IA64_GRANULE_SIZE-1) & ~(IA64_GRANULE_SIZE-1))

/*
 * Used to locate BOOT_DATA prior to initializing the node data area.
 */
#define BOOT_NODE_DATA(node)	pg_data_ptr[node]

/*
 * To prevent cache aliasing effects, align per-node structures so that they 
 * start at addresses that are strided by node number.
 */
#define NODEDATA_ALIGN(addr, node)	((((addr) + 1024*1024-1) & ~(1024*1024-1)) + (node)*PAGE_SIZE)


static struct ia64_node_data	*boot_node_data[NR_NODES] __initdata;
static pg_data_t		*pg_data_ptr[NR_NODES] __initdata;
static bootmem_data_t		bdata[NR_NODES] __initdata;
static unsigned long		boot_pernode[NR_NODES] __initdata;
static unsigned long		boot_pernodesize[NR_NODES] __initdata;

extern int  filter_rsvd_memory (unsigned long start, unsigned long end, void *arg);
extern struct cpuinfo_ia64 *_cpu_data[NR_CPUS];



/*
 * We allocate one of the bootmem_data_t structs for each piece of memory
 * that we wish to treat as a contiguous block.  Each such block must start
 * on a GRANULE boundary.  Multiple banks per node is not supported.
 *   (Note: on SN2, all memory on a node is trated as a single bank.
 *   Holes within the bank are supported. This works because memory
 *   from different banks is not interleaved. The bootmap bitmap
 *   for the node is somewhat large but not too large).
 */
static int __init
build_maps(unsigned long start, unsigned long end, int node)
{
	bootmem_data_t	*bdp;
	unsigned long cstart, epfn;

	bdp = &bdata[node];
	epfn = GRANULEROUNDUP(__pa(end)) >> PAGE_SHIFT;
	cstart = GRANULEROUNDDOWN(__pa(start));

	if (!bdp->node_low_pfn) {
		bdp->node_boot_start = cstart;
		bdp->node_low_pfn = epfn;
	} else {
		bdp->node_boot_start = min(cstart, bdp->node_boot_start);
		bdp->node_low_pfn = max(epfn, bdp->node_low_pfn);
	}

	min_low_pfn = min(min_low_pfn, bdp->node_boot_start>>PAGE_SHIFT);
	max_low_pfn = max(max_low_pfn, bdp->node_low_pfn);

	return 0;
}


/*
 * Count the number of cpus on the node
 */
static __inline__ int
count_cpus(int node)
{
	int cpu, n=0;

	for (cpu=0; cpu < NR_CPUS; cpu++)
		if (node == node_cpuid[cpu].nid)
			n++;
	return n;
}


/*
 * Find space on each node for the bootmem map & other per-node data structures.
 *
 * Called by efi_memmap_walk to find boot memory on each node. Note that
 * only blocks that are free are passed to this routine (currently filtered by
 * free_available_memory).
 */
static int __init
find_pernode_space(unsigned long start, unsigned long end, int node)
{
	unsigned long	mapsize, pages, epfn, map=0, cpu, cpus;
	unsigned long	pernodesize=0, pernode;
	unsigned long	cpu_data, mmu_gathers;
	unsigned long	pstart, length;
	bootmem_data_t	*bdp;

	pstart = __pa(start);
	length = end - start;
	epfn = (pstart + length) >> PAGE_SHIFT;
	bdp = &bdata[node];

	if (pstart < bdp->node_boot_start || epfn > bdp->node_low_pfn)
		return 0;

	if (!boot_pernode[node]) {
		cpus = count_cpus(node);
		pernodesize += PAGE_ALIGN(sizeof(struct cpuinfo_ia64)) * cpus;
		pernodesize += L1_CACHE_ALIGN(sizeof(mmu_gather_t)) * cpus;
		pernodesize += L1_CACHE_ALIGN(sizeof(pg_data_t));
		pernodesize += L1_CACHE_ALIGN(sizeof(struct ia64_node_data));
		pernodesize = PAGE_ALIGN(pernodesize);
		pernode = NODEDATA_ALIGN(pstart, node);
	
		if (pstart + length > (pernode + pernodesize)) {
			boot_pernode[node] = pernode;
			boot_pernodesize[node] = pernodesize;
			memset(__va(pernode), 0, pernodesize);

			cpu_data = pernode;
			pernode += PAGE_ALIGN(sizeof(struct cpuinfo_ia64)) * cpus;

			mmu_gathers = pernode;
			pernode += L1_CACHE_ALIGN(sizeof(mmu_gather_t)) * cpus;

			pg_data_ptr[node] = __va(pernode);
			pernode += L1_CACHE_ALIGN(sizeof(pg_data_t));

			boot_node_data[node] = __va(pernode);
			pernode += L1_CACHE_ALIGN(sizeof(struct ia64_node_data));

			pg_data_ptr[node]->bdata = &bdata[node];
			pernode += L1_CACHE_ALIGN(sizeof(pg_data_t));

			for (cpu=0; cpu < NR_CPUS; cpu++) {
				if (node == node_cpuid[cpu].nid) {
					_cpu_data[cpu] = __va(cpu_data);
					_cpu_data[cpu]->node_data = boot_node_data[node];
					_cpu_data[cpu]->nodeid = node;
					_cpu_data[cpu]->mmu_gathers = __va(mmu_gathers);
					cpu_data +=  PAGE_ALIGN(sizeof(struct cpuinfo_ia64));
					mmu_gathers += L1_CACHE_ALIGN(sizeof(mmu_gather_t));
				}
			}

		}
	}

	pernode = boot_pernode[node];
	pernodesize = boot_pernodesize[node];
	if (pernode && !bdp->node_bootmem_map) {
		pages = bdp->node_low_pfn - (bdp->node_boot_start>>PAGE_SHIFT);
		mapsize = bootmem_bootmap_pages(pages) << PAGE_SHIFT;

		if (pernode - pstart > mapsize)
			map = pstart;
		else if (pstart + length - pernode - pernodesize > mapsize)
			map = pernode + pernodesize;

		if (map) {
			init_bootmem_node(
				BOOT_NODE_DATA(node),
				map>>PAGE_SHIFT, 
				bdp->node_boot_start>>PAGE_SHIFT,
				bdp->node_low_pfn);
		}

	}

	return 0;
}


/*
 * Free available memory to the bootmem allocator.
 *
 * Note that only blocks that are free are passed to this routine (currently 
 * filtered by free_available_memory).
 *
 */
static int __init
discontig_free_bootmem_node(unsigned long start, unsigned long end, int node)
{
	free_bootmem_node(BOOT_NODE_DATA(node), __pa(start), end - start);

	return 0;
}


/*
 * Reserve the space used by the bootmem maps.
 */
static void __init
discontig_reserve_bootmem(void)
{
	int		node;
	unsigned long	base, size, pages;
	bootmem_data_t	*bdp;

	for (node = 0; node < numnodes; node++) {
		bdp = BOOT_NODE_DATA(node)->bdata;

		pages = bdp->node_low_pfn - (bdp->node_boot_start>>PAGE_SHIFT);
		size = bootmem_bootmap_pages(pages) << PAGE_SHIFT;
		base = __pa(bdp->node_bootmem_map);
		reserve_bootmem_node(BOOT_NODE_DATA(node), base, size);

		size = boot_pernodesize[node];
		base = __pa(boot_pernode[node]);
		reserve_bootmem_node(BOOT_NODE_DATA(node), base, size);
	}
}

/*
 * Initialize per-node data
 *
 * Finish setting up the node data for this node, then copy it to the other nodes.
 *
 */
static void __init
initialize_pernode_data(void)
{
	int	cpu, node;

	memcpy(boot_node_data[0]->pg_data_ptrs, pg_data_ptr, sizeof(pg_data_ptr));
	memcpy(boot_node_data[0]->node_data_ptrs, boot_node_data, sizeof(boot_node_data));

	for (node=1; node < numnodes; node++) {
		memcpy(boot_node_data[node], boot_node_data[0], sizeof(struct ia64_node_data));
		boot_node_data[node]->node = node;
	}

	for (cpu=0; cpu < NR_CPUS; cpu++) {
		node = node_cpuid[cpu].nid;
		_cpu_data[cpu]->node_data = boot_node_data[node];
		_cpu_data[cpu]->nodeid = node;
	}
}


/*
 * Called early in boot to setup the boot memory allocator, and to
 * allocate the node-local pg_data & node-directory data structures..
 */
void __init
discontig_mem_init(void)
{
	if (numnodes == 0) {
		printk("node info missing!\n");
		numnodes = 1;
	}

	min_low_pfn = -1;
	max_low_pfn = 0;

        efi_memmap_walk(filter_rsvd_memory, build_maps);
        efi_memmap_walk(filter_rsvd_memory, find_pernode_space);
        efi_memmap_walk(filter_rsvd_memory, discontig_free_bootmem_node);

	discontig_reserve_bootmem();
	initialize_pernode_data();
}

