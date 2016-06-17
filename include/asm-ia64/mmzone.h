/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000,2003 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (c) 2002 NEC Corp.
 * Copyright (c) 2002 Erich Focht <efocht@ess.nec.de>
 * Copyright (c) 2002 Kimio Suganuma <k-suganuma@da.jp.nec.com>
 */
#ifndef _ASM_IA64_MMZONE_H
#define _ASM_IA64_MMZONE_H

#include <linux/config.h>
#include <linux/init.h>


#ifdef CONFIG_NUMA

#ifdef CONFIG_IA64_DIG

/*
 * Platform definitions for DIG platform with contiguous memory.
 */
#define MAX_PHYSNODE_ID	8		/* Maximum node number +1 */
#define NR_NODES	8		/* Maximum number of nodes in SSI */
#define NR_MEMBLKS	(NR_NODES * 32)




#elif CONFIG_IA64_SGI_SN2

/*
 * Platform definitions for DIG platform with contiguous memory.
 */
#define MAX_PHYSNODE_ID	2048		/* Maximum node number +1 */
#define NR_NODES	256		/* Maximum number of compute nodes in SSI */
#define NR_MEMBLKS	(NR_NODES)

#elif CONFIG_IA64_GENERIC


/*
 * Platform definitions for GENERIC platform with contiguous or discontiguous memory.
 */
#define MAX_PHYSNODE_ID 2048		/* Maximum node number +1 */
#define NR_NODES        256		/* Maximum number of nodes in SSI */
#define NR_MEMBLKS      (NR_NODES)


#else
#error unknown platform
#endif

extern void build_cpu_to_node_map(void);

#else /* CONFIG_NUMA */

#define NR_NODES	1

#endif /* CONFIG_NUMA */
#endif /* _ASM_IA64_MMZONE_H */
