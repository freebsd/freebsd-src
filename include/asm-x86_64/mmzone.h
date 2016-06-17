/*
 * Written by Kanoj Sarcar (kanoj@sgi.com) Aug 99
 * Adapted for K8/x86-64 Jul 2002 by Andi Kleen.
 */
#ifndef _ASM_MMZONE_H_
#define _ASM_MMZONE_H_

#include <linux/config.h>

typedef struct plat_pglist_data {
	pg_data_t	gendata;
	unsigned long   start_pfn, end_pfn;
} plat_pg_data_t;

struct bootmem_data_t;

/*
 * Following are macros that are specific to this numa platform.
 * 
 * XXX check what the compiler generates for all this
 */

extern plat_pg_data_t *plat_node_data[];

#define MAXNODE 8 
#define MAX_NUMNODES MAXNODE
#define NODEMAPSIZE 0xff

/* Simple perfect hash to map physical addresses to node numbers */
extern int memnode_shift; 
extern u8  memnodemap[NODEMAPSIZE]; 
extern int maxnode;

#if 0
#define VIRTUAL_BUG_ON(x) do { if (x) out_of_line_bug(); } while(0)
#else
#define VIRTUAL_BUG_ON(x) do {} while (0) 
#endif

/* VALID_PAGE below hardcodes the same algorithm*/
static inline int phys_to_nid(unsigned long addr) 
{ 
	int nid; 
	VIRTUAL_BUG_ON((addr >> memnode_shift) >= NODEMAPSIZE);
	nid = memnodemap[addr >> memnode_shift]; 
	VIRTUAL_BUG_ON(nid > maxnode); 
	return nid; 
} 

#define PLAT_NODE_DATA(n)		(plat_node_data[(n)])
#define PLAT_NODE_DATA_STARTNR(n)	\
	(PLAT_NODE_DATA(n)->gendata.node_start_mapnr)
#define PLAT_NODE_DATA_SIZE(n)		(PLAT_NODE_DATA(n)->gendata.node_size)

#define PLAT_NODE_DATA_LOCALNR(p, n)	\
	(((p) - PLAT_NODE_DATA(n)->gendata.node_start_paddr) >> PAGE_SHIFT)

#ifdef CONFIG_DISCONTIGMEM

/*
 * Following are macros that each numa implmentation must define.
 */

/*
 * Given a kernel address, find the home node of the underlying memory.
 */
#define KVADDR_TO_NID(kaddr)	phys_to_nid(__pa(kaddr))

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
 * and returns the the mem_map of that node.
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

#define BAD_PAGE 0xffffffffffff

/* this really should be optimized a bit */
static inline unsigned long 
paddr_to_local_pfn(unsigned long phys_addr, struct page **mem_map, int check) 
{ 
	unsigned long nid;
	if (check) { /* we rely on gcc optimizing this way for most cases */ 
		unsigned long index = phys_addr >> memnode_shift; 
		if (index >= NODEMAPSIZE || memnodemap[index] == 0xff) { 
			*mem_map = NULL;
			return BAD_PAGE;
		} 
		nid = memnodemap[index];
	} else { 
		nid = phys_to_nid(phys_addr); 
	} 			   
	plat_pg_data_t *plat_pgdat = plat_node_data[nid]; 
	unsigned long pfn = phys_addr >> PAGE_SHIFT; 
	VIRTUAL_BUG_ON(pfn >= plat_pgdat->end_pfn);
	VIRTUAL_BUG_ON(pfn < plat_pgdat->start_pfn);
	*mem_map = plat_pgdat->gendata.node_mem_map; 
	return pfn - plat_pgdat->start_pfn;
} 
#define virt_to_page(kaddr) \
	({ struct page *lmemmap; \
	   unsigned long lpfn = paddr_to_local_pfn(__pa(kaddr),&lmemmap,0); \
	   lmemmap + lpfn;  })

/* needs to handle bad addresses too */
#define pte_page(pte) \
	({ struct page *lmemmap; \
	   unsigned long addr = pte_val(pte) & PHYSICAL_PAGE_MASK; \
	   unsigned long lpfn = paddr_to_local_pfn(addr,&lmemmap,1); \
	   lmemmap + lpfn;  })

#define pfn_to_page(pfn)	virt_to_page(__va((unsigned long)(pfn) << PAGE_SHIFT))
#define page_to_pfn(page)	({ \
	int nodeid = phys_to_nid(__pa(page));  \
	plat_pg_data_t *nd = PLAT_NODE_DATA(nodeid); \
	(page - nd->gendata.node_mem_map) + nd->start_pfn; \
})

#define VALID_PAGE(page_ptr) ({ \
	int ok = 0; 						\
	unsigned long phys = __pa(page_ptr); 			\
        unsigned long index = phys >> memnode_shift; 		\
	if (index <= NODEMAPSIZE) { 					  \
		unsigned nodeid = memnodemap[index]; 			  \
		pg_data_t *nd = NODE_DATA(nodeid); 			  \
		struct page *lmemmap = nd->node_mem_map;		  \
		ok = (nodeid != 0xff) && \
		     (page_ptr >= lmemmap && page_ptr < lmemmap + nd->node_size); \
	} 			\
	ok; 			\
})

#define page_to_phys(page) (page_to_pfn(page) << PAGE_SHIFT)



extern void setup_node_bootmem(int nodeid, unsigned long start_, unsigned long end_);


#ifdef CONFIG_NUMA
extern int fake_node;
#define cputonode(cpu) (fake_node ? 0 : (cpu))
#define numa_node_id()	cputonode(smp_processor_id())
#endif /* CONFIG_NUMA */

#define MAX_NR_NODES 8

#endif /* CONFIG_DISCONTIGMEM */

#endif /* _ASM_MMZONE_H_ */
