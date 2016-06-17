/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * This file contains NUMA specific variables and functions which can
 * be split away from DISCONTIGMEM and are used on NUMA machines with
 * contiguous memory.
 * 
 *                         2002/08/07 Erich Focht <efocht@ess.nec.de>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/smp.h>
#include <asm/numa.h>

/*
 * The following structures are usually initialized by ACPI or
 * similar mechanisms and describe the NUMA characteristics of the machine.
 */
int num_memblks = 0;
struct node_memblk_s node_memblk[NR_MEMBLKS];
struct node_cpuid_s node_cpuid[NR_CPUS];
/*
 * This is a matrix with "distances" between nodes, they should be
 * proportional to the memory access latency ratios.
 */
u8 numa_slit[NR_NODES * NR_NODES];

/* Identify which cnode a physical address resides on */
int
paddr_to_nid(unsigned long paddr)
{
	int	i;

	for (i = 0; i < num_memblks; i++)
		if (paddr >= node_memblk[i].start_paddr &&
		    paddr < node_memblk[i].start_paddr + node_memblk[i].size)
			break;

	return (i < num_memblks) ? node_memblk[i].nid : (num_memblks ? -1 : 0);
}

/* return end addr of a memblk */
unsigned long
memblk_endpaddr(unsigned long paddr)
{
	int	i;

	for (i = 0; i < num_memblks; i++)
		if (paddr >= node_memblk[i].start_paddr &&
		    paddr < node_memblk[i].start_paddr + node_memblk[i].size)
			return node_memblk[i].start_paddr + node_memblk[i].size;

	return 0;
}


/* on which node is each logical CPU (one cacheline even for 64 CPUs) */
volatile char cpu_to_node_map[NR_CPUS] __cacheline_aligned;

/* which logical CPUs are on which nodes */
volatile unsigned long node_to_cpu_mask[NR_NODES]  __cacheline_aligned;

/*
 * Build cpu to node mapping and initialize the per node cpu masks.
 */
void __init
build_cpu_to_node_map (void)
{
	int cpu, i, node;

	for(cpu = 0; cpu < NR_CPUS; ++cpu) {
		/*
		 * All Itanium NUMA platforms I know use ACPI, so maybe we
		 * can drop this ifdef completely.                    [EF]
		 */
#ifdef CONFIG_SMP
# ifdef CONFIG_ACPI_NUMA
		node = -1;
		for (i = 0; i < NR_CPUS; ++i) {
			extern volatile int ia64_cpu_to_sapicid[];
			if (ia64_cpu_to_sapicid[cpu] == node_cpuid[i].phys_id) {
				node = node_cpuid[i].nid;
				break;
			}
		}
# else
#		error Fixme: Dunno how to build CPU-to-node map.
# endif
		cpu_to_node_map[cpu] = node;
		if (node >= 0)
			__set_bit(cpu, &node_to_cpu_mask[node]);
#else
			__set_bit(0, &node_to_cpu_mask[0]);
#endif
	}
}

