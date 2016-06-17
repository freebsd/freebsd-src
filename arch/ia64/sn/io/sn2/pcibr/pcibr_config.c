/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/byteorder/swab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/pci/pci_defs.h>
#include <asm/sn/prio.h>
#include <asm/sn/xtalk/xbow.h>
#include <asm/sn/ioc3.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>

extern pcibr_info_t      pcibr_info_get(vertex_hdl_t);

uint64_t          pcibr_config_get(vertex_hdl_t, unsigned, unsigned);
uint64_t          do_pcibr_config_get(cfg_p, unsigned, unsigned);
void              pcibr_config_set(vertex_hdl_t, unsigned, unsigned, uint64_t);
void       	  do_pcibr_config_set(cfg_p, unsigned, unsigned, uint64_t);

/*
 * on sn-ia we need to twiddle the the addresses going out
 * the pci bus because we use the unswizzled synergy space
 * (the alternative is to use the swizzled synergy space
 * and byte swap the data)
 */
#define CB(b,r) (((volatile uint8_t *) b)[((r)^4)])
#define CS(b,r) (((volatile uint16_t *) b)[((r^4)/2)])
#define CW(b,r) (((volatile uint32_t *) b)[((r^4)/4)])

#define	CBP(b,r) (((volatile uint8_t *) b)[(r)])
#define	CSP(b,r) (((volatile uint16_t *) b)[((r)/2)])
#define	CWP(b,r) (((volatile uint32_t *) b)[(r)/4])

#define SCB(b,r) (((volatile uint8_t *) b)[((r)^3)])
#define SCS(b,r) (((volatile uint16_t *) b)[((r^2)/2)])
#define SCW(b,r) (((volatile uint32_t *) b)[((r)/4)])

/*
 * Return a config space address for given slot / func / offset.  Note the
 * returned pointer is a 32bit word (ie. cfg_p) aligned pointer pointing to
 * the 32bit word that contains the "offset" byte.
 */
cfg_p
pcibr_func_config_addr(bridge_t *bridge, pciio_bus_t bus, pciio_slot_t slot, 
					pciio_function_t func, int offset)
{
	/*
	 * Type 1 config space
	 */
	if (bus > 0) {
		bridge->b_pci_cfg = ((bus << 16) | (slot << 11));
		return &bridge->b_type1_cfg.f[func].l[(offset)];
	}

	/*
	 * Type 0 config space
	 */
	slot++;
	return &bridge->b_type0_cfg_dev[slot].f[func].l[offset];
}

/*
 * Return config space address for given slot / offset.  Note the returned
 * pointer is a 32bit word (ie. cfg_p) aligned pointer pointing to the
 * 32bit word that contains the "offset" byte.
 */
cfg_p
pcibr_slot_config_addr(bridge_t *bridge, pciio_slot_t slot, int offset)
{
	return pcibr_func_config_addr(bridge, 0, slot, 0, offset);
}

/*
 * Return config space data for given slot / offset
 */
unsigned
pcibr_slot_config_get(bridge_t *bridge, pciio_slot_t slot, int offset)
{
	cfg_p  cfg_base;
	
	cfg_base = pcibr_slot_config_addr(bridge, slot, 0);
	return (do_pcibr_config_get(cfg_base, offset, sizeof(unsigned)));
}

/*
 * Return config space data for given slot / func / offset
 */
unsigned
pcibr_func_config_get(bridge_t *bridge, pciio_slot_t slot, 
					pciio_function_t func, int offset)
{
	cfg_p  cfg_base;

	cfg_base = pcibr_func_config_addr(bridge, 0, slot, func, 0);
	return (do_pcibr_config_get(cfg_base, offset, sizeof(unsigned)));
}

/*
 * Set config space data for given slot / offset
 */
void
pcibr_slot_config_set(bridge_t *bridge, pciio_slot_t slot, 
					int offset, unsigned val)
{
	cfg_p  cfg_base;

	cfg_base = pcibr_slot_config_addr(bridge, slot, 0);
	do_pcibr_config_set(cfg_base, offset, sizeof(unsigned), val);
}

/*
 * Set config space data for given slot / func / offset
 */
void
pcibr_func_config_set(bridge_t *bridge, pciio_slot_t slot, 
			pciio_function_t func, int offset, unsigned val)
{
	cfg_p  cfg_base;

	cfg_base = pcibr_func_config_addr(bridge, 0, slot, func, 0);
	do_pcibr_config_set(cfg_base, offset, sizeof(unsigned), val);
}

int pcibr_config_debug = 0;

cfg_p
pcibr_config_addr(vertex_hdl_t conn,
		  unsigned reg)
{
    pcibr_info_t            pcibr_info;
    pciio_bus_t		    pciio_bus;
    pciio_slot_t            pciio_slot;
    pciio_function_t        pciio_func;
    pcibr_soft_t            pcibr_soft;
    bridge_t               *bridge;
    cfg_p                   cfgbase = (cfg_p)0;
    pciio_info_t	    pciio_info;

    pciio_info = pciio_info_get(conn);
    pcibr_info = pcibr_info_get(conn);

    /*
     * Determine the PCI bus/slot/func to generate a config address for.
     */

    if (pciio_info_type1_get(pciio_info)) {
	/*
	 * Conn is a vhdl which uses TYPE 1 addressing explicitly passed 
	 * in reg.
	 */
	pciio_bus = PCI_TYPE1_BUS(reg);
	pciio_slot = PCI_TYPE1_SLOT(reg);
	pciio_func = PCI_TYPE1_FUNC(reg);

	ASSERT(pciio_bus != 0);
    } else {
	/*
	 * Conn is directly connected to the host bus.  PCI bus number is
	 * hardcoded to 0 (even though it may have a logical bus number != 0)
	 * and slot/function are derived from the pcibr_info_t associated
	 * with the device.
	 */
	pciio_bus = 0;

    pciio_slot = PCIBR_INFO_SLOT_GET_INT(pcibr_info);
    if (pciio_slot == PCIIO_SLOT_NONE)
	pciio_slot = PCI_TYPE1_SLOT(reg);

    pciio_func = pcibr_info->f_func;
    if (pciio_func == PCIIO_FUNC_NONE)
	pciio_func = PCI_TYPE1_FUNC(reg);
    }

    pcibr_soft = (pcibr_soft_t) pcibr_info->f_mfast;

    bridge = pcibr_soft->bs_base;

    cfgbase = pcibr_func_config_addr(bridge,
			pciio_bus, pciio_slot, pciio_func, 0);

    return cfgbase;
}

uint64_t
pcibr_config_get(vertex_hdl_t conn,
		 unsigned reg,
		 unsigned size)
{
	return do_pcibr_config_get(pcibr_config_addr(conn, reg),
				PCI_TYPE1_REG(reg), size);
}

uint64_t
do_pcibr_config_get(cfg_p cfgbase,
		       unsigned reg,
		       unsigned size)
{
    unsigned                value;

    value = CWP(cfgbase, reg);
    if (reg & 3)
	value >>= 8 * (reg & 3);
    if (size < 4)
	value &= (1 << (8 * size)) - 1;
    return value;
}

void
pcibr_config_set(vertex_hdl_t conn,
		 unsigned reg,
		 unsigned size,
		 uint64_t value)
{
	do_pcibr_config_set(pcibr_config_addr(conn, reg),
			PCI_TYPE1_REG(reg), size, value);
}

void
do_pcibr_config_set(cfg_p cfgbase,
		    unsigned reg,
		    unsigned size,
		    uint64_t value)
{
	switch (size) {
	case 1:
		CBP(cfgbase, reg) = value;
		break;
	case 2:
		if (reg & 1) {
			CBP(cfgbase, reg) = value;
			CBP(cfgbase, reg + 1) = value >> 8;
		} else
			CSP(cfgbase, reg) = value;
		break;
	case 3:
		if (reg & 1) {
			CBP(cfgbase, reg) = value;
			CSP(cfgbase, (reg + 1)) = value >> 8;
		} else {
			CSP(cfgbase, reg) = value;
			CBP(cfgbase, reg + 2) = value >> 16;
		}
		break;
	case 4:
		CWP(cfgbase, reg) = value;
		break;
 	}
}
