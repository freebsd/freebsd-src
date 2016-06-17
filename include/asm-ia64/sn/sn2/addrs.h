/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001-2003 Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef _ASM_IA64_SN_SN2_ADDRS_H
#define _ASM_IA64_SN_SN2_ADDRS_H

/* McKinley Address Format:
 *
 *   4 4       3 3  3 3
 *   9 8       8 7  6 5             0
 *  +-+---------+----+--------------+
 *  |0| Node ID | AS | Node Offset  |
 *  +-+---------+----+--------------+
 *
 *   Node ID: If bit 38 = 1, is ICE, else is SHUB
 *   AS: Address Space Identifier. Used only if bit 38 = 0.
 *     b'00: Local Resources and MMR space
 *           bit 35
 *               0: Local resources space
 *                  node id:
 *                        0: IA64/NT compatibility space
 *                        2: Local MMR Space
 *                        4: Local memory, regardless of local node id
 *               1: Global MMR space
 *     b'01: GET space.
 *     b'10: AMO space.
 *     b'11: Cacheable memory space.
 *
 *   NodeOffset: byte offset
 */

#ifndef __ASSEMBLY__
typedef union ia64_sn2_pa {
	struct {
		unsigned long off  : 36;
		unsigned long as   : 2;
		unsigned long nasid: 11;
		unsigned long fill : 15;
	} f;
	unsigned long l;
	void *p;
} ia64_sn2_pa_t;
#endif

#define TO_PHYS_MASK		0x0001ffcfffffffff	/* Note - clear AS bits */


/* Regions determined by AS */
#define LOCAL_MMR_SPACE		0xc000008000000000	/* Local MMR space */
#define LOCAL_PHYS_MMR_SPACE	0x8000008000000000	/* Local PhysicalMMR space */
#define LOCAL_MEM_SPACE		0xc000010000000000	/* Local Memory space */
#define GLOBAL_MMR_SPACE	0xc000000800000000	/* Global MMR space */
#define GLOBAL_PHYS_MMR_SPACE	0x0000000800000000	/* Global Physical MMR space */
#define GET_SPACE		0xe000001000000000	/* GET space */
#define AMO_SPACE		0xc000002000000000	/* AMO space */
#define CACHEABLE_MEM_SPACE	0xe000003000000000	/* Cacheable memory space */
#define UNCACHED                0xc000000000000000      /* UnCacheable memory space */
#define UNCACHED_PHYS           0x8000000000000000      /* UnCacheable physical memory space */

#define PHYS_MEM_SPACE		0x0000003000000000	/* physical memory space */

/* SN2 address macros */
#define NID_SHFT		38
#define LOCAL_MMR_ADDR(a)	(UNCACHED | LOCAL_MMR_SPACE | (a))
#define LOCAL_MMR_PHYS_ADDR(a)	(UNCACHED_PHYS | LOCAL_PHYS_MMR_SPACE | (a))
#define LOCAL_MEM_ADDR(a)	(LOCAL_MEM_SPACE | (a))
#define REMOTE_ADDR(n,a)	((((unsigned long)(n))<<NID_SHFT) | (a))
#define GLOBAL_MMR_ADDR(n,a)	(UNCACHED | GLOBAL_MMR_SPACE | REMOTE_ADDR(n,a))
#define GLOBAL_MMR_PHYS_ADDR(n,a) (UNCACHED_PHYS | GLOBAL_PHYS_MMR_SPACE | REMOTE_ADDR(n,a))
#define GET_ADDR(n,a)		(GET_SPACE | REMOTE_ADDR(n,a))
#define AMO_ADDR(n,a)		(UNCACHED | AMO_SPACE | REMOTE_ADDR(n,a))
#define GLOBAL_MEM_ADDR(n,a)	(CACHEABLE_MEM_SPACE | REMOTE_ADDR(n,a))

/* non-II mmr's start at top of big window space (4G) */
#define BWIN_TOP		0x0000000100000000

/*
 * general address defines - for code common to SN0/SN1/SN2
 */
#define CAC_BASE		CACHEABLE_MEM_SPACE			/* cacheable memory space */
#define IO_BASE			(UNCACHED | GLOBAL_MMR_SPACE)		/* lower 4G maps II's XIO space */
#define AMO_BASE		(UNCACHED | AMO_SPACE)			/* fetch & op space */
#define MSPEC_BASE		AMO_BASE				/* fetch & op space */
#define UNCAC_BASE		(UNCACHED | CACHEABLE_MEM_SPACE)	/* uncached global memory */
#define GET_BASE		GET_SPACE				/* momentarily coherent remote mem. */
#define CALIAS_BASE             LOCAL_CACHEABLE_BASE			/* cached node-local memory */
#define UALIAS_BASE             (UNCACHED | LOCAL_CACHEABLE_BASE)	/* uncached node-local memory */

#define TO_PHYS(x)              (              ((x) & TO_PHYS_MASK))
#define TO_CAC(x)               (CAC_BASE    | ((x) & TO_PHYS_MASK))
#define TO_UNCAC(x)             (UNCAC_BASE  | ((x) & TO_PHYS_MASK))
#define TO_MSPEC(x)             (MSPEC_BASE  | ((x) & TO_PHYS_MASK))
#define TO_GET(x)		(GET_BASE    | ((x) & TO_PHYS_MASK))
#define TO_CALIAS(x)            (CALIAS_BASE | TO_NODE_ADDRSPACE(x))
#define TO_UALIAS(x)            (UALIAS_BASE | TO_NODE_ADDRSPACE(x))
#define NODE_SIZE_BITS		36	/* node offset : bits <35:0> */
#define BWIN_SIZE_BITS		29	/* big window size: 512M */
#define NASID_BITS		11	/* bits <48:38> */
#define NASID_BITMASK		(0x7ffULL)
#define NASID_SHFT		NID_SHFT
#define NASID_META_BITS		0	/* ???? */
#define NASID_LOCAL_BITS	7	/* same router as SN1 */

#define NODE_ADDRSPACE_SIZE     (UINT64_CAST 1 << NODE_SIZE_BITS)
#define NASID_MASK              (UINT64_CAST NASID_BITMASK << NASID_SHFT)
#define NASID_GET(_pa)          (int) ((UINT64_CAST (_pa) >>            \
                                        NASID_SHFT) & NASID_BITMASK)
#define PHYS_TO_DMA(x)          ( ((x & NASID_MASK) >> 2) |             \
                                  (x & (NODE_ADDRSPACE_SIZE - 1)) )

#define CHANGE_NASID(n,x)	({ia64_sn2_pa_t _v; _v.l = (long) (x); _v.f.nasid = n; _v.p;})

/*
 * Determine if a physical address should be referenced as cached or uncached. 
 * For now, assume all memory is cached and everything else is noncached.
 * (Later, we may need to special case areas of memory to be reference uncached).
 */
#define IS_CACHED_ADDRESS(x)	(((x) & PHYS_MEM_SPACE) == PHYS_MEM_SPACE)


#ifndef __ASSEMBLY__
#define NODE_SWIN_BASE(nasid, widget)                                   \
        ((widget == 0) ? NODE_BWIN_BASE((nasid), SWIN0_BIGWIN)          \
        : RAW_NODE_SWIN_BASE(nasid, widget))
#else
#define NODE_SWIN_BASE(nasid, widget) \
     (NODE_IO_BASE(nasid) + (UINT64_CAST (widget) << SWIN_SIZE_BITS))
#define LOCAL_SWIN_BASE(widget) \
	(UNCACHED | LOCAL_MMR_SPACE | ((UINT64_CAST (widget) << SWIN_SIZE_BITS)))
#endif /* __ASSEMBLY__ */

/*
 * The following definitions pertain to the IO special address
 * space.  They define the location of the big and little windows
 * of any given node.
 */

#define BWIN_INDEX_BITS         3
#define BWIN_SIZE               (UINT64_CAST 1 << BWIN_SIZE_BITS)
#define BWIN_SIZEMASK           (BWIN_SIZE - 1)
#define BWIN_WIDGET_MASK        0x7
#define NODE_BWIN_BASE0(nasid)  (NODE_IO_BASE(nasid) + BWIN_SIZE)
#define NODE_BWIN_BASE(nasid, bigwin)   (NODE_BWIN_BASE0(nasid) +       \
                        (UINT64_CAST (bigwin) << BWIN_SIZE_BITS))

#define BWIN_WIDGETADDR(addr)   ((addr) & BWIN_SIZEMASK)
#define BWIN_WINDOWNUM(addr)    (((addr) >> BWIN_SIZE_BITS) & BWIN_WIDGET_MASK)

/*
 * Verify if addr belongs to large window address of node with "nasid"
 *
 *
 * NOTE: "addr" is expected to be XKPHYS address, and NOT physical
 * address
 *
 *
 */

#define NODE_BWIN_ADDR(nasid, addr)     \
                (((addr) >= NODE_BWIN_BASE0(nasid)) && \
                 ((addr) < (NODE_BWIN_BASE(nasid, HUB_NUM_BIG_WINDOW) + \
                                BWIN_SIZE)))

#endif	/* _ASM_IA64_SN_SN2_ADDRS_H */
