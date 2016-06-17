/* $Id: shub_intr.c,v 1.1 2002/02/28 17:31:25 marcelo Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/sn/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/driver.h>
#include <asm/sn/iograph.h>
#include <asm/param.h>
#include <asm/sn/pio.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/addrs.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/intr.h>
#include <asm/sn/xtalk/xtalkaddrs.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn2/shub_mmr.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/pci/bridge.h>

/* ARGSUSED */
void
hub_intr_init(vertex_hdl_t hubv)
{
}

xwidgetnum_t
hub_widget_id(nasid_t nasid)
{

	if (!(nasid & 1)) {
        	hubii_wcr_t     ii_wcr; /* the control status register */
        	ii_wcr.wcr_reg_value = REMOTE_HUB_L(nasid,IIO_WCR);
        	return ii_wcr.wcr_fields_s.wcr_widget_id;
	} else {
		/* ICE does not have widget id. */
		return(-1);
	}
}

static hub_intr_t
do_hub_intr_alloc(vertex_hdl_t dev,
		device_desc_t dev_desc,
		vertex_hdl_t owner_dev,
		int uncond_nothread)
{
	cpuid_t		cpu = 0;
	int		vector;
	hub_intr_t	intr_hdl;
	cnodeid_t	cnode;
	int		cpuphys, slice;
	int		nasid;
	iopaddr_t	xtalk_addr;
	struct xtalk_intr_s	*xtalk_info;
	xwidget_info_t	xwidget_info;
	ilvl_t		intr_swlevel = 0;

	cpu = intr_heuristic(dev, dev_desc, -1, 0, owner_dev, NULL, &vector);

	if (cpu == CPU_NONE) {
		printk("Unable to allocate interrupt for 0x%p\n", (void *)owner_dev);
		return(0);
	}

	cpuphys = cpu_physical_id(cpu);
	slice = cpu_physical_id_to_slice(cpuphys);
	nasid = cpu_physical_id_to_nasid(cpuphys);
	cnode = cpuid_to_cnodeid(cpu);

	if (slice) {
		xtalk_addr = SH_II_INT1 | ((unsigned long)nasid << 36) | (1UL << 47);
	} else {
		xtalk_addr = SH_II_INT0 | ((unsigned long)nasid << 36) | (1UL << 47);
	}

	intr_hdl = snia_kmem_alloc_node(sizeof(struct hub_intr_s), cnode);
	ASSERT_ALWAYS(intr_hdl);

	xtalk_info = &intr_hdl->i_xtalk_info;
	xtalk_info->xi_dev = dev;
	xtalk_info->xi_vector = vector;
	xtalk_info->xi_addr = xtalk_addr;

	xwidget_info = xwidget_info_get(dev);
	if (xwidget_info) {
		xtalk_info->xi_target = xwidget_info_masterid_get(xwidget_info);
	}

	intr_hdl->i_swlevel = intr_swlevel;
	intr_hdl->i_cpuid = cpu;
	intr_hdl->i_bit = vector;
	intr_hdl->i_flags |= HUB_INTR_IS_ALLOCED;

	return(intr_hdl);
}

hub_intr_t
hub_intr_alloc(vertex_hdl_t dev,
		device_desc_t dev_desc,
		vertex_hdl_t owner_dev)
{
	return(do_hub_intr_alloc(dev, dev_desc, owner_dev, 0));
}

hub_intr_t
hub_intr_alloc_nothd(vertex_hdl_t dev,
		device_desc_t dev_desc,
		vertex_hdl_t owner_dev)
{
	return(do_hub_intr_alloc(dev, dev_desc, owner_dev, 1));
}

void
hub_intr_free(hub_intr_t intr_hdl)
{
	cpuid_t		cpu = intr_hdl->i_cpuid;
	int		vector = intr_hdl->i_bit;
	xtalk_intr_t	xtalk_info;

	if (intr_hdl->i_flags & HUB_INTR_IS_CONNECTED) {
		xtalk_info = &intr_hdl->i_xtalk_info;
		xtalk_info->xi_dev = NODEV;
		xtalk_info->xi_vector = 0;
		xtalk_info->xi_addr = 0;
		hub_intr_disconnect(intr_hdl);
	}

	if (intr_hdl->i_flags & HUB_INTR_IS_ALLOCED) {
		kfree(intr_hdl);
	}
	intr_unreserve_level(cpu, vector);
}

int
hub_intr_connect(hub_intr_t intr_hdl,
		intr_func_t intr_func,          /* xtalk intr handler */
		void *intr_arg,                 /* arg to intr handler */
		xtalk_intr_setfunc_t setfunc,
		void *setfunc_arg)
{
	int		rv;
	cpuid_t		cpu = intr_hdl->i_cpuid;
	int 		vector = intr_hdl->i_bit;

	ASSERT(intr_hdl->i_flags & HUB_INTR_IS_ALLOCED);

	rv = intr_connect_level(cpu, vector, intr_hdl->i_swlevel, NULL);
	if (rv < 0) {
		return rv;
	}

	intr_hdl->i_xtalk_info.xi_setfunc = setfunc;
	intr_hdl->i_xtalk_info.xi_sfarg = setfunc_arg;

	if (setfunc) {
		(*setfunc)((xtalk_intr_t)intr_hdl);
	}

	intr_hdl->i_flags |= HUB_INTR_IS_CONNECTED;

	return 0;
}

/*
 * Disassociate handler with the specified interrupt.
 */
void
hub_intr_disconnect(hub_intr_t intr_hdl)
{
	/*REFERENCED*/
	int rv;
	cpuid_t cpu = intr_hdl->i_cpuid;
	int bit = intr_hdl->i_bit;
	xtalk_intr_setfunc_t setfunc;

	setfunc = intr_hdl->i_xtalk_info.xi_setfunc;

	/* TBD: send disconnected interrupts somewhere harmless */
	if (setfunc) (*setfunc)((xtalk_intr_t)intr_hdl);

	rv = intr_disconnect_level(cpu, bit);
	ASSERT(rv == 0);
	intr_hdl->i_flags &= ~HUB_INTR_IS_CONNECTED;
}

/* 
 * Redirect an interrupt to another cpu.
 */

void
sn_shub_redirect_intr(pcibr_intr_t intr, unsigned long cpu) {
	unsigned long bit;
	int cpuphys, slice;
	nasid_t nasid;
	unsigned long xtalk_addr;
	bridge_t	*bridge = intr->bi_soft->bs_base;
	picreg_t	int_enable;
	picreg_t	host_addr;
	int		irq;

	cpuphys = cpu_physical_id(cpu);
	slice = cpu_physical_id_to_slice(cpuphys);
	nasid = cpu_physical_id_to_nasid(cpuphys);

	if (slice) {    
		xtalk_addr = SH_II_INT1 | ((unsigned long)nasid << 36) | (1UL << 47);
	} else {
		xtalk_addr = SH_II_INT0 | ((unsigned long)nasid << 36) | (1UL << 47);
	}

	for (bit = 0; bit < 8; bit++) {
		if (intr->bi_ibits & (1 << bit) ) {
			/* Disable interrupts. */
			int_enable = bridge->p_int_enable_64;
			int_enable &= ~bit;
			bridge->p_int_enable_64 = int_enable;
			/* Reset Host address (Interrupt destination) */
			host_addr = bridge->p_int_addr_64[bit];
			host_addr &= ~((1UL << 48) - 1);
			host_addr |= xtalk_addr;
			bridge->p_int_addr_64[bit] = host_addr;
			/* Enable interrupt */
			int_enable |= bit;
			bridge->p_int_enable_64 = int_enable;
			/* Force an interrupt, just in case. */
			bridge->b_force_pin[bit].intr = 1;
		}
	}
	irq = intr->bi_irq;
	if (pdacpu(cpu).sn_first_irq == 0 || pdacpu(cpu).sn_first_irq > irq) {
		pdacpu(cpu).sn_first_irq = irq;
	}
	if (pdacpu(cpu).sn_last_irq < irq) {
		pdacpu(cpu).sn_last_irq = irq;
	}
	intr->bi_cpu = (int)cpu;
}

