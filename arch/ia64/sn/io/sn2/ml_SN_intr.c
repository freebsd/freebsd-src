/* $Id: ml_SN_intr.c,v 1.1 2002/02/28 17:31:25 marcelo Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * intr.c-
 *	This file contains all of the routines necessary to set up and
 *	handle interrupts on an IPXX board.
 */

#ident  "$Revision: 1.1 $"

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <asm/smp.h>
#include <asm/irq.h>
#include <asm/hw_irq.h>
#include <asm/sn/sgi.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/intr.h>
#include <asm/sn/sn2/shub_mmr_t.h>
#include <asm/sn/sn2/shubio.h>
#include <asm/sal.h>
#include <asm/sn/sn_sal.h>

extern irqpda_t	*irqpdaindr;
extern cnodeid_t master_node_get(vertex_hdl_t vhdl);
extern nasid_t master_nasid;

//  Initialize some shub registers for interrupts, both IO and error.
//  



void
intr_init_vecblk( nodepda_t *npda,
		cnodeid_t node,
		int sn)
{
	int 			nasid = cnodeid_to_nasid(node);
	sh_ii_int0_config_u_t	ii_int_config;
	cpuid_t			cpu;
	cpuid_t			cpu0, cpu1;
	nodepda_t		*lnodepda;
	sh_ii_int0_enable_u_t	ii_int_enable;
	sh_int_node_id_config_u_t	node_id_config;
	sh_local_int5_config_u_t	local5_config;
	sh_local_int5_enable_u_t	local5_enable;
	extern void sn_init_cpei_timer(void);
	static int timer_added = 0;


	if (is_headless_node(node) ) {
		int cnode;
		struct ia64_sal_retval ret_stuff;

		// retarget all interrupts on this node to the master node.
		node_id_config.sh_int_node_id_config_regval = 0;
		node_id_config.sh_int_node_id_config_s.node_id = master_nasid;
		node_id_config.sh_int_node_id_config_s.id_sel = 1;
		HUB_S( (unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_INT_NODE_ID_CONFIG),
			node_id_config.sh_int_node_id_config_regval);
		cnode = nasid_to_cnodeid(master_nasid);
		lnodepda = NODEPDA(cnode);
		cpu = lnodepda->node_first_cpu;
		cpu = cpu_physical_id(cpu);
		SAL_CALL(ret_stuff, SN_SAL_REGISTER_CE, nasid, cpu, master_nasid,0,0,0,0);
		if (ret_stuff.status < 0) {
			printk("%s: SN_SAL_REGISTER_CE SAL_CALL failed\n",__FUNCTION__);
		}
	} else {
		lnodepda = NODEPDA(node);
		cpu = lnodepda->node_first_cpu;
		cpu = cpu_physical_id(cpu);
	}

	// Get the physical id's of the cpu's on this node.
	cpu0 = nasid_slice_to_cpu_physical_id(nasid, 0);
	cpu1 = nasid_slice_to_cpu_physical_id(nasid, 2);

	HUB_S( (unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_PI_ERROR_MASK), 0);
	HUB_S( (unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_PI_CRBP_ERROR_MASK), 0);

	// Config and enable UART interrupt, all nodes.

	local5_config.sh_local_int5_config_regval = 0;
	local5_config.sh_local_int5_config_s.idx = SGI_UART_VECTOR;
	local5_config.sh_local_int5_config_s.pid = cpu;
	HUB_S( (unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_LOCAL_INT5_CONFIG),
		local5_config.sh_local_int5_config_regval);

	local5_enable.sh_local_int5_enable_regval = 0;
	local5_enable.sh_local_int5_enable_s.uart_int = 1;
	HUB_S( (unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_LOCAL_INT5_ENABLE),
		local5_enable.sh_local_int5_enable_regval);


	// The II_INT_CONFIG register for cpu 0.
	ii_int_config.sh_ii_int0_config_regval = 0;
	ii_int_config.sh_ii_int0_config_s.type = 0;
	ii_int_config.sh_ii_int0_config_s.agt = 0;
	ii_int_config.sh_ii_int0_config_s.pid = cpu0;
	ii_int_config.sh_ii_int0_config_s.base = 0;

	HUB_S((unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_II_INT0_CONFIG),
		ii_int_config.sh_ii_int0_config_regval);


	// The II_INT_CONFIG register for cpu 1.
	ii_int_config.sh_ii_int0_config_regval = 0;
	ii_int_config.sh_ii_int0_config_s.type = 0;
	ii_int_config.sh_ii_int0_config_s.agt = 0;
	ii_int_config.sh_ii_int0_config_s.pid = cpu1;
	ii_int_config.sh_ii_int0_config_s.base = 0;

	HUB_S((unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_II_INT1_CONFIG),
		ii_int_config.sh_ii_int0_config_regval);


	// Enable interrupts for II_INT0 and 1.
	ii_int_enable.sh_ii_int0_enable_regval = 0;
	ii_int_enable.sh_ii_int0_enable_s.ii_enable = 1;

	HUB_S((unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_II_INT0_ENABLE),
		ii_int_enable.sh_ii_int0_enable_regval);
	HUB_S((unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_II_INT1_ENABLE),
		ii_int_enable.sh_ii_int0_enable_regval);


	if (!timer_added) { // can only init the timer once.
		timer_added = 1;
		sn_init_cpei_timer();
	}
}

// (Un)Reserve an irq on this cpu.

static int
do_intr_reserve_level(cpuid_t cpu,
			int bit,
			int reserve)
{
	int i;
	irqpda_t	*irqs = irqpdaindr;
	int		min_shared;

	if (reserve) {
		if (bit < 0) {
			for (i = IA64_SN2_FIRST_DEVICE_VECTOR; i <= IA64_SN2_LAST_DEVICE_VECTOR; i++) {
				if (irqs->irq_flags[i] == 0) {
					bit = i;
					break;
				}
			}
		}
		if (bit < 0) {  /* ran out of irqs.  Have to share.  This will be rare. */
			min_shared = 256;
			for (i=IA64_SN2_FIRST_DEVICE_VECTOR; i < IA64_SN2_LAST_DEVICE_VECTOR; i++) {
				/* Share with the same device class */
				if (irqpdaindr->current->vendor == irqpdaindr->device_dev[i]->vendor &&
					irqpdaindr->current->device == irqpdaindr->device_dev[i]->device &&
					irqpdaindr->share_count[i] < min_shared) {
						min_shared = irqpdaindr->share_count[i];
						bit = i;
				}
			}
			min_shared = 256;
			if (bit < 0) {  /* didn't find a matching device, just pick one. This will be */
					/* exceptionally rare. */
				for (i=IA64_SN2_FIRST_DEVICE_VECTOR; i < IA64_SN2_LAST_DEVICE_VECTOR; i++) {
					if (irqpdaindr->share_count[i] < min_shared) {
						min_shared = irqpdaindr->share_count[i];
						bit = i;
					}
				}
			}
			irqpdaindr->share_count[bit]++;
		}
		if (irqs->irq_flags[bit] & SN2_IRQ_SHARED) {
			irqs->irq_flags[bit] |= SN2_IRQ_RESERVED;
			return bit;
		}
		if (irqs->irq_flags[bit] & SN2_IRQ_RESERVED) {
			return -1;
		} else {
			irqs->num_irq_used++;
			irqs->irq_flags[bit] |= SN2_IRQ_RESERVED;
			return bit;
		}
	} else {
		if (irqs->irq_flags[bit] & SN2_IRQ_RESERVED) {
			irqs->num_irq_used--;
			irqs->irq_flags[bit] &= ~SN2_IRQ_RESERVED;
			return bit;
		} else {
			return -1;
		}
	}
}

int
intr_reserve_level(cpuid_t cpu,
		int bit,
		int resflags,
		vertex_hdl_t owner_dev,
		char *name)
{
	return(do_intr_reserve_level(cpu, bit, 1));
}

void
intr_unreserve_level(cpuid_t cpu,
		int bit)
{
	(void)do_intr_reserve_level(cpu, bit, 0);
}

// Mark an irq on this cpu as (dis)connected.

static int
do_intr_connect_level(cpuid_t cpu,
			int bit,
			int connect)
{
	irqpda_t	*irqs = irqpdaindr;

	if (connect) {
		if (irqs->irq_flags[bit] & SN2_IRQ_SHARED) {
			irqs->irq_flags[bit] |= SN2_IRQ_CONNECTED;
			return bit;
		}
		if (irqs->irq_flags[bit] & SN2_IRQ_CONNECTED) {
			return -1;
		} else {
			irqs->irq_flags[bit] |= SN2_IRQ_CONNECTED;
			return bit;
		}
	} else {
		if (irqs->irq_flags[bit] & SN2_IRQ_CONNECTED) {
			irqs->irq_flags[bit] &= ~SN2_IRQ_CONNECTED;
			return bit;
		} else {
			return -1;
		}
	}
	return(bit);
}

int
intr_connect_level(cpuid_t cpu,
		int bit,
		ilvl_t is,
		intr_func_t intr_prefunc)
{
	return(do_intr_connect_level(cpu, bit, 1));
}

int
intr_disconnect_level(cpuid_t cpu,
		int bit)
{
	return(do_intr_connect_level(cpu, bit, 0));
}

// Choose a cpu on this node.
// We choose the one with the least number of int's assigned to it.

static cpuid_t
do_intr_cpu_choose(cnodeid_t cnode) {
	cpuid_t		cpu, best_cpu = CPU_NONE;
	int		slice, min_count = 1000;
	irqpda_t	*irqs;

	for (slice = CPUS_PER_NODE - 1; slice >= 0; slice--) {
		int intrs;

		cpu = cnode_slice_to_cpuid(cnode, slice);
		if (cpu == smp_num_cpus) {
			continue;
		}

		if (!cpu_online(cpu)) {
			continue;
		}

		irqs = irqpdaindr;
		intrs = irqs->num_irq_used;

		if (min_count > intrs) {
			min_count = intrs;
			best_cpu = cpu;
			if ( enable_shub_wars_1_1() ) {
				/* Rather than finding the best cpu, always return the first cpu*/
				/* This forces all interrupts to the same cpu */
				break;
			}
		}
	}
	return best_cpu;
}

static cpuid_t
intr_cpu_choose_from_node(cnodeid_t cnode)
{
	return(do_intr_cpu_choose(cnode));
}

// See if we can use this cpu/vect.

static cpuid_t
intr_bit_reserve_test(cpuid_t cpu,
			int favor_subnode,
			cnodeid_t cnode,
			int req_bit,
			int resflags,
			vertex_hdl_t owner_dev,
			char *name,
			int *resp_bit)
{
	ASSERT( (cpu == CPU_NONE) || (cnode == CNODEID_NONE) );

	if (cnode != CNODEID_NONE) {
		cpu = intr_cpu_choose_from_node(cnode);
	}

	if (cpu != CPU_NONE) {
		*resp_bit = do_intr_reserve_level(cpu, req_bit, 1);
		if (*resp_bit >= 0) {
			return(cpu);
		}
	}
	return CPU_NONE;
}

// Find the node to assign for this interrupt.

cpuid_t
intr_heuristic(vertex_hdl_t dev,
		device_desc_t dev_desc,
		int	req_bit,
		int resflags,
		vertex_hdl_t owner_dev,
		char *name,
		int *resp_bit)
{
	cpuid_t		cpuid;
	cpuid_t		candidate = CPU_NONE;
	cnodeid_t	candidate_node;
	vertex_hdl_t	pconn_vhdl;
	pcibr_soft_t	pcibr_soft;
	int 		bit;
	static cnodeid_t last_node = 0;

/* SN2 + pcibr addressing limitation */
/* Due to this limitation, all interrupts from a given bridge must go to the name node.*/
/* The interrupt must also be targetted for the same processor. */
/* This limitation does not exist on PIC. */
/* But, the processor limitation will stay.  The limitation will be similar to */
/* the bedrock/xbridge limit regarding PI's */

	if ( (hwgraph_edge_get(dev, EDGE_LBL_PCI, &pconn_vhdl) == GRAPH_SUCCESS) &&
		( (pcibr_soft = pcibr_soft_get(pconn_vhdl) ) != NULL) ) {
			if (pcibr_soft->bsi_err_intr) {
				candidate =  ((hub_intr_t)pcibr_soft->bsi_err_intr)->i_cpuid;
			}
	}


	if (candidate != CPU_NONE) {
		// The cpu was chosen already when we assigned the error interrupt.
		bit = intr_reserve_level(candidate, 
					req_bit, 
					resflags, 
					owner_dev, 
					name);
		if (bit < 0) {
			cpuid = CPU_NONE;
		} else {
			cpuid = candidate;
			*resp_bit = bit;
		}
	} else {
		// Need to choose one.  Try the controlling c-brick first.
		cpuid = intr_bit_reserve_test(CPU_NONE,
						0,
						master_node_get(dev),
						req_bit,
						0,
						owner_dev,
						name,
						resp_bit);
	}

	if (cpuid != CPU_NONE) {
		return cpuid;
	}

	if (candidate  != CPU_NONE) {
		printk("Cannot target interrupt to target node (%ld).\n",candidate);
		return CPU_NONE; 
	} else {
		printk("Cannot target interrupt to closest node (0x%x) 0x%p\n",
			master_node_get(dev), (void *)owner_dev);
	}

	// We couldn't put it on the closest node.  Try to find another one.
	// Do a stupid round-robin assignment of the node.

	{
		int i;

		if (last_node >= numnodes) last_node = 0;
		for (i = 0, candidate_node = last_node; i < numnodes; candidate_node++,i++) {
			if (candidate_node == numnodes) candidate_node = 0;
			cpuid = intr_bit_reserve_test(CPU_NONE,
							0,
							candidate_node,
							req_bit,
							0,
							owner_dev,
							name,
							resp_bit);
			if (cpuid != CPU_NONE) {
				last_node = candidate_node + 1;
				return cpuid;
			}
		}
	}

	printk("cannot target interrupt: 0x%p\n",(void *)owner_dev);
	return CPU_NONE;
}
