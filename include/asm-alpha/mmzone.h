/*
 * Written by Kanoj Sarcar (kanoj@sgi.com) Aug 99
 * Adapted for the alpha wildfire architecture Jan 2001.
 */
#ifndef _ASM_MMZONE_H_
#define _ASM_MMZONE_H_

#include <linux/config.h>
#ifdef CONFIG_NUMA_SCHED
#include <linux/numa_sched.h>
#endif
#ifdef NOTYET
#include <asm/sn/types.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/klkernvars.h>
#endif /* NOTYET */

typedef struct plat_pglist_data {
	pg_data_t	gendata;
#ifdef NOTYET
	kern_vars_t	kern_vars;
#endif
#if defined(CONFIG_NUMA) && defined(CONFIG_NUMA_SCHED)
	struct numa_schedule_data schedule_data;
#endif
} plat_pg_data_t;

struct bootmem_data_t; /* stupid forward decl. */

/*
 * Following are macros that are specific to this numa platform.
 */

extern plat_pg_data_t *plat_node_data[];

#define ALPHA_PA_TO_NID(pa)		\
        (alpha_mv.pa_to_nid 		\
	 ? alpha_mv.pa_to_nid(pa)	\
	 : (0))
#define NODE_MEM_START(nid)		\
        (alpha_mv.node_mem_start 	\
	 ? alpha_mv.node_mem_start(nid) \
	 : (0UL))
#define NODE_MEM_SIZE(nid)		\
        (alpha_mv.node_mem_size 	\
	 ? alpha_mv.node_mem_size(nid) 	\
	 : ((nid) ? (0UL) : (~0UL)))
#define MAX_NUMNODES		128		/* marvel */

#define PHYSADDR_TO_NID(pa)		ALPHA_PA_TO_NID(pa)
#define PLAT_NODE_DATA(n)		(plat_node_data[(n)])
#define PLAT_NODE_DATA_STARTNR(n)	\
	(PLAT_NODE_DATA(n)->gendata.node_start_mapnr)
#define PLAT_NODE_DATA_SIZE(n)		(PLAT_NODE_DATA(n)->gendata.node_size)

#if 1
#define PLAT_NODE_DATA_LOCALNR(p, n)	\
	(((p) - PLAT_NODE_DATA(n)->gendata.node_start_paddr) >> PAGE_SHIFT)
#else
static inline unsigned long
PLAT_NODE_DATA_LOCALNR(unsigned long p, int n)
{
	unsigned long temp;
	temp = p - PLAT_NODE_DATA(n)->gendata.node_start_paddr;
	return (temp >> PAGE_SHIFT);
}
#endif

#ifdef CONFIG_DISCONTIGMEM

/*
 * Following are macros that each numa implmentation must define.
 */

/*
 * Given a kernel address, find the home node of the underlying memory.
 */
#define KVADDR_TO_NID(kaddr)	PHYSADDR_TO_NID(__pa(kaddr))

/*
 * Return a pointer to the node data for node n.
 */
#define NODE_DATA(n)	(&((PLAT_NODE_DATA(n))->gendata))

/*
 * NODE_MEM_MAP gives the kaddr for the mem_map of the node.
 */
#define NODE_MEM_MAP(nid)	(NODE_DATA(nid)->node_mem_map)

/*
 * Given a kaddr, ADDR_TO_MAPBASE finds the owning node of the memory
 * and returns the mem_map of that node.
 */
#define ADDR_TO_MAPBASE(kaddr) \
			NODE_MEM_MAP(KVADDR_TO_NID((unsigned long)(kaddr)))

/*
 * Given a kaddr, LOCAL_BASE_ADDR finds the owning node of the memory
 * and returns the kaddr corresponding to first physical page in the
 * node's mem_map.
 */
#define LOCAL_BASE_ADDR(kaddr)	((unsigned long)__va(NODE_DATA(KVADDR_TO_NID(kaddr))->node_start_paddr))

#define LOCAL_MAP_NR(kvaddr) \
	(((unsigned long)(kvaddr)-LOCAL_BASE_ADDR(kvaddr)) >> PAGE_SHIFT)

#define kern_addr_valid(kaddr)	test_bit(LOCAL_MAP_NR(kaddr), \
					 NODE_DATA(KVADDR_TO_NID(kaddr))->valid_addr_bitmap)

#define virt_to_page(kaddr)	(ADDR_TO_MAPBASE(kaddr) + LOCAL_MAP_NR(kaddr))
#define VALID_PAGE(page)	(((page) - mem_map) < max_mapnr)

#ifdef CONFIG_NUMA
#ifdef CONFIG_NUMA_SCHED
#define NODE_SCHEDULE_DATA(nid)	(&((PLAT_NODE_DATA(nid))->schedule_data))
#endif

#define cputonode(cpu) 	\
        (alpha_mv.cpuid_to_nid ? alpha_mv.cpuid_to_nid(cpu) : 0)

#define numa_node_id()	cputonode(smp_processor_id())
#endif /* CONFIG_NUMA */

#endif /* CONFIG_DISCONTIGMEM */

#endif /* _ASM_MMZONE_H_ */
