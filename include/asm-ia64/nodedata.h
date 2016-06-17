/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (c) 2002 NEC Corp.
 * Copyright (c) 2002 Erich Focht <efocht@ess.nec.de>
 * Copyright (c) 2002 Kimio Suganuma <k-suganuma@da.jp.nec.com>
 */


#ifndef _ASM_IA64_NODEDATA_H
#define _ASM_IA64_NODEDATA_H


#include <asm/mmzone.h>

/*
 * Node Data. One of these structures is located on each node of a NUMA system.
 */

struct pglist_data;
struct ia64_node_data {
	short			node;
	short			active_cpu_count;
	/*
	 * The fields are read-only (after boot). They contain pointers to various structures
	 * located on other nodes. Ths data is replicated on each node in order to reduce
	 * off-node references.
	 */
        struct pglist_data	*pg_data_ptrs[NR_NODES];
	struct ia64_node_data	*node_data_ptrs[NR_NODES];
};


/*
 * Return a pointer to the node_data structure for the executing cpu.
 */
#define local_node_data		(local_cpu_data->node_data)


/*
 * Return a pointer to the node_data structure for the specified node.
 */
#define node_data(node)	(local_node_data->node_data_ptrs[node])


/*
 * Given a node id, return a pointer to the pg_data_t for the node.
 * The following 2 macros are similar. 
 *
 * NODE_DATA 	- should be used in all code not related to system
 *		  initialization. It uses pernode data structures to minimize
 *		  offnode memory references. However, these structure are not 
 *		  present during boot. This macro can be used once cpu_init
 *		  completes.
 *
 * NOTE:   The names of these macros are misleading but are difficult to change
 *	   since they are used in generic linux & on other architecures.
 */
#define NODE_DATA(nid)		(local_node_data->pg_data_ptrs[nid])

extern struct pglist_data * __init boot_get_pg_data_ptr(long);

#endif /* _ASM_IA64_NODEDATA_H */
