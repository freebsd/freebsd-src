/* $Id: arch.h,v 1.1 2002/02/28 17:31:25 marcelo Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_SN2_ARCH_H
#define _ASM_IA64_SN_SN2_ARCH_H

#include <asm/types.h>


#define CPUS_PER_NODE           4       /* CPUs on a single hub */
#define CPUS_PER_SUBNODE        4       /* CPUs on a single hub PI */


/*
 * This is the maximum number of NASIDS that can be present in a system.
 * (Highest NASID plus one.)
 */
#define MAX_NASIDS              2048


/*
 * This is the maximum number of nodes that can be part of a kernel.
 * Effectively, it's the maximum number of compact node ids (cnodeid_t).
 * This is not necessarily the same as MAX_NASIDS.
 */
#define MAX_COMPACT_NODES       2048

/*
 * MAX_REGIONS refers to the maximum number of hardware partitioned regions.
 */
#define	MAX_REGIONS		64
#define MAX_NONPREMIUM_REGIONS  16
#define MAX_PREMIUM_REGIONS     MAX_REGIONS


/*
 * MAX_PARITIONS refers to the maximum number of logically defined 
 * partitions the system can support.
 */
#define MAX_PARTITIONS		MAX_REGIONS


#define NASID_MASK_BYTES	((MAX_NASIDS + 7) / 8)
#define CNASID_MASK_BYTES	(NASID_MASK_BYTES / 2)


/*
 * 1 FSB per SHUB, with up to 4 cpus per FSB.
 */
#define NUM_SUBNODES	1
#define SUBNODE_SHFT	0
#define SUBNODE_MASK	(0x0 << SUBNODE_SHFT)
#define LOCALCPU_SHFT	0
#define LOCALCPU_MASK	(0x3 << LOCALCPU_SHFT)
#define SUBNODE(slice)	(((slice) & SUBNODE_MASK) >> SUBNODE_SHFT)
#define LOCALCPU(slice)	(((slice) & LOCALCPU_MASK) >> LOCALCPU_SHFT)
#define TO_SLICE(subn, local)	(((subn) << SUBNODE_SHFT) | \
				 ((local) << LOCALCPU_SHFT))

#endif /* _ASM_IA64_SN_SN2_ARCH_H */
