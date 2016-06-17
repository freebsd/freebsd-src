/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * This file contains NUMA specific prototypes and definitions.
 * 
 * 2002/08/05 Erich Focht <efocht@ess.nec.de>
 *
 */
#ifndef _ASM_IA64_NUMA_H
#define _ASM_IA64_NUMA_H

#ifdef CONFIG_NUMA

#ifdef CONFIG_DISCONTIGMEM
# include <asm/mmzone.h>
#else
# define NR_NODES     (8)
# define NR_MEMBLKS   (NR_NODES * 8)
#endif

#include <linux/cache.h>
#include <linux/threads.h>
#include <linux/smp.h>

#define NODEMASK_WORDCOUNT       ((NR_NODES+(BITS_PER_LONG-1))/BITS_PER_LONG)

#define NODE_MASK_NONE   { [0 ... ((NR_NODES+BITS_PER_LONG-1)/BITS_PER_LONG)-1] = 0 }

typedef unsigned long   nodemask_t[NODEMASK_WORDCOUNT];
                                                                                                                             
extern volatile char cpu_to_node_map[NR_CPUS] __cacheline_aligned;
extern volatile unsigned long node_to_cpu_mask[NR_NODES] __cacheline_aligned;

/* Stuff below this line could be architecture independent */

extern int num_memblks;		/* total number of memory chunks */

/*
 * List of node memory chunks. Filled when parsing SRAT table to
 * obtain information about memory nodes.
*/

struct node_memblk_s {
	unsigned long start_paddr;
	unsigned long size;
	int nid;		/* which logical node contains this chunk? */
	int bank;		/* which mem bank on this node */
};

struct node_cpuid_s {
	u16	phys_id;	/* id << 8 | eid */
	int	nid;		/* logical node containing this CPU */
};

extern struct node_memblk_s node_memblk[NR_MEMBLKS];
extern struct node_cpuid_s node_cpuid[NR_CPUS];

/*
 * ACPI 2.0 SLIT (System Locality Information Table)
 * http://devresource.hp.com/devresource/Docs/TechPapers/IA64/slit.pdf
 *
 * This is a matrix with "distances" between nodes, they should be
 * proportional to the memory access latency ratios.
 */

extern u8 numa_slit[NR_NODES * NR_NODES];
#define node_distance(from,to) (numa_slit[from * numnodes + to])

extern int paddr_to_nid(unsigned long paddr);
extern unsigned long memblk_endpaddr(unsigned long paddr);

#define local_nodeid (cpu_to_node_map[smp_processor_id()])

#else /* !CONFIG_NUMA */

#define node_distance(from,to) 10
#define paddr_to_nid(x) 0
#define memblk_endpaddr(x) ~0UL
#define local_nodeid 0

#endif /* CONFIG_NUMA */

#endif /* _ASM_IA64_NUMA_H */
