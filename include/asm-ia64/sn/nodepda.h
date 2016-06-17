/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_NODEPDA_H
#define _ASM_IA64_SN_NODEPDA_H


#include <linux/config.h>
#include <asm/sn/sgi.h>
#include <asm/irq.h>
#include <asm/sn/intr.h>
#include <asm/sn/router.h>
#include <asm/sn/pda.h>
#include <asm/sn/module.h>
#include <asm/sn/bte.h>

/*
 * NUMA Node-Specific Data structures are defined in this file.
 * In particular, this is the location of the node PDA.
 * A pointer to the right node PDA is saved in each CPU PDA.
 */

/*
 * Node-specific data structure.
 *
 * One of these structures is allocated on each node of a NUMA system.
 *
 * This structure provides a convenient way of keeping together 
 * all per-node data structures. 
 */



struct nodepda_s {


	cpuid_t         node_first_cpu; /* Starting cpu number for node */
					/* WARNING: no guarantee that   */
					/*  the second cpu on a node is */
					/*  node_first_cpu+1.           */

	vertex_hdl_t 	xbow_vhdl;
	nasid_t		xbow_peer;	/* NASID of our peer hub on xbow */
	struct semaphore xbow_sema;	/* Sema for xbow synchronization */
	slotid_t	slotdesc;
	geoid_t		geoid;
	module_t	*module;	/* Pointer to containing module */
	xwidgetnum_t 	basew_id;
	vertex_hdl_t 	basew_xc;
	int		hubticks;
	int		num_routers;	/* XXX not setup! Total routers in the system */

	
	char		*hwg_node_name;	/* hwgraph node name */
	vertex_hdl_t	node_vertex;	/* Hwgraph vertex for this node */

	void 		*pdinfo;	/* Platform-dependent per-node info */


	nodepda_router_info_t	*npda_rip_first;
	nodepda_router_info_t	**npda_rip_last;


	spinlock_t		bist_lock;

	/*
	 * The BTEs on this node are shared by the local cpus
	 */
	struct bteinfo_s	bte_if[BTES_PER_NODE];	/* Virtual Interface */
	struct timer_list	bte_recovery_timer;
	spinlock_t		bte_recovery_lock;

	/* 
	 * Array of pointers to the nodepdas for each node.
	 */
	struct nodepda_s	*pernode_pdaindr[MAX_COMPACT_NODES]; 

};

typedef struct nodepda_s nodepda_t;

struct irqpda_s {
	int num_irq_used;
	char irq_flags[NR_IRQS];
	struct pci_dev *device_dev[NR_IRQS];
	char share_count[NR_IRQS];
	struct pci_dev *current;
};

typedef struct irqpda_s irqpda_t;


/*
 * Access Functions for node PDA.
 * Since there is one nodepda for each node, we need a convenient mechanism
 * to access these nodepdas without cluttering code with #ifdefs.
 * The next set of definitions provides this.
 * Routines are expected to use 
 *
 *	nodepda			-> to access node PDA for the node on which code is running
 *	subnodepda		-> to access subnode PDA for the subnode on which code is running
 *
 *	NODEPDA(cnode)		-> to access node PDA for cnodeid 
 *	SUBNODEPDA(cnode,sn)	-> to access subnode PDA for cnodeid/subnode
 */

#define	nodepda		pda.p_nodepda		/* Ptr to this node's PDA */
#define	NODEPDA(cnode)		(nodepda->pernode_pdaindr[cnode])


/*
 * Macros to access data structures inside nodepda 
 */
#define NODE_MODULEID(cnode)    geo_module((NODEPDA(cnode)->geoid))
#define NODE_SLOTID(cnode)	(NODEPDA(cnode)->slotdesc)


/*
 * Quickly convert a compact node ID into a hwgraph vertex
 */
#define cnodeid_to_vertex(cnodeid) (NODEPDA(cnodeid)->node_vertex)


/*
 * Check if given a compact node id the corresponding node has all the
 * cpus disabled. 
 */
#define is_headless_node(cnode)	(!test_bit(cnode, &node_has_active_cpus))

/*
 * Check if given a node vertex handle the corresponding node has all the
 * cpus disabled. 
 */
#define is_headless_node_vertex(_nodevhdl) \
			is_headless_node(nodevertex_to_cnodeid(_nodevhdl))


#endif /* _ASM_IA64_SN_NODEPDA_H */
