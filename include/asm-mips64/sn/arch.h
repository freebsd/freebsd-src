/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI specific setup.
 *
 * Copyright (C) 1995 - 1997, 1999 Silcon Graphics, Inc.
 * Copyright (C) 1999 Ralf Baechle (ralf@gnu.org)
 */
#ifndef _ASM_SN_ARCH_H
#define _ASM_SN_ARCH_H

#include <linux/types.h>
#include <linux/config.h>

#if !defined(CONFIG_SGI_IO)
#include <asm/sn/types.h>
#include <asm/sn/sn0/arch.h>
#endif


#ifndef __ASSEMBLY__
#if !defined(CONFIG_SGI_IO)
typedef u64	hubreg_t;
typedef u64	nic_t;
#endif
#endif

#ifdef CONFIG_SGI_IP27
#define CPUS_PER_NODE		2	/* CPUs on a single hub */
#define CPUS_PER_NODE_SHFT	1	/* Bits to shift in the node number */
#define CPUS_PER_SUBNODE	2	/* CPUs on a single hub PI */
#endif
#define CNODE_NUM_CPUS(_cnode)		(NODEPDA(_cnode)->node_num_cpus)

#define CNODE_TO_CPU_BASE(_cnode)	(NODEPDA(_cnode)->node_first_cpu)
#define cputocnode(cpu)				\
               (cpu_data[(cpu)].p_nodeid)
#define cputonasid(cpu)				\
               (cpu_data[(cpu)].p_nasid)
#define cputoslice(cpu)				\
               (cpu_data[(cpu)].p_slice)
#define makespnum(_nasid, _slice)					\
		(((_nasid) << CPUS_PER_NODE_SHFT) | (_slice))

#ifndef __ASSEMBLY__

#define INVALID_NASID		(nasid_t)-1
#define INVALID_CNODEID		(cnodeid_t)-1
#define INVALID_PNODEID		(pnodeid_t)-1
#define INVALID_MODULE		(moduleid_t)-1
#define	INVALID_PARTID		(partid_t)-1

extern nasid_t get_nasid(void);
extern cnodeid_t get_cpu_cnode(cpuid_t);
extern int get_cpu_slice(cpuid_t);

/*
 * NO ONE should access these arrays directly.  The only reason we refer to
 * them here is to avoid the procedure call that would be required in the
 * macros below.  (Really want private data members here :-)
 */
extern cnodeid_t nasid_to_compact_node[MAX_NASIDS];
extern nasid_t compact_to_nasid_node[MAX_COMPACT_NODES];

/*
 * These macros are used by various parts of the kernel to convert
 * between the three different kinds of node numbering.   At least some
 * of them may change to procedure calls in the future, but the macros
 * will continue to work.  Don't use the arrays above directly.
 */

#define	NASID_TO_REGION(nnode)	      	\
    ((nnode) >> \
     (is_fine_dirmode() ? NASID_TO_FINEREG_SHFT : NASID_TO_COARSEREG_SHFT))

#if !defined(_STANDALONE)
extern cnodeid_t nasid_to_compact_node[MAX_NASIDS];
extern nasid_t compact_to_nasid_node[MAX_COMPACT_NODES];
extern cnodeid_t cpuid_to_compact_node[MAXCPUS];
#endif

#if !defined(DEBUG) && (!defined(SABLE) || defined(_STANDALONE))

#define NASID_TO_COMPACT_NODEID(nnode)	(nasid_to_compact_node[nnode])
#define COMPACT_TO_NASID_NODEID(cnode)	(compact_to_nasid_node[cnode])
#define CPUID_TO_COMPACT_NODEID(cpu)	(cpuid_to_compact_node[(cpu)])
#else

/*
 * These functions can do type checking and fail if they need to return
 * a bad nodeid, but they're not as fast so just use 'em for debug kernels.
 */
cnodeid_t nasid_to_compact_nodeid(nasid_t nasid);
nasid_t compact_to_nasid_nodeid(cnodeid_t cnode);

#define NASID_TO_COMPACT_NODEID(nnode)	nasid_to_compact_nodeid(nnode)
#define COMPACT_TO_NASID_NODEID(cnode)	compact_to_nasid_nodeid(cnode)
#define CPUID_TO_COMPACT_NODEID(cpu)	(cpuid_to_compact_node[(cpu)])
#endif

extern int node_getlastslot(cnodeid_t);

#endif /* !__ASSEMBLY__ */

#define SLOT_BITMASK    	(MAX_MEM_SLOTS - 1)
#define SLOT_SIZE		(1LL<<SLOT_SHIFT)

#define node_getnumslots(node)	(MAX_MEM_SLOTS)
#define NODE_MAX_MEM_SIZE	SLOT_SIZE * MAX_MEM_SLOTS

/*
 * New stuff in here from Irix sys/pfdat.h.
 */
#define	SLOT_PFNSHIFT		(SLOT_SHIFT - PAGE_SHIFT)
#define	PFN_NASIDSHFT		(NASID_SHFT - PAGE_SHIFT)
#define mkpfn(nasid, off)	(((pfn_t)(nasid) << PFN_NASIDSHFT) | (off))
#define slot_getbasepfn(node,slot) \
		(mkpfn(COMPACT_TO_NASID_NODEID(node), slot<<SLOT_PFNSHIFT))
#endif /* _ASM_SN_ARCH_H */
