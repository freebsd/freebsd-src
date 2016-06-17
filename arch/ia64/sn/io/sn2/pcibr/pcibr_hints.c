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

pcibr_hints_t           pcibr_hints_get(vertex_hdl_t, int);
void                    pcibr_hints_fix_rrbs(vertex_hdl_t);
void                    pcibr_hints_dualslot(vertex_hdl_t, pciio_slot_t, pciio_slot_t);
void			pcibr_hints_intr_bits(vertex_hdl_t, pcibr_intr_bits_f *);
void                    pcibr_set_rrb_callback(vertex_hdl_t, rrb_alloc_funct_t);
void                    pcibr_hints_handsoff(vertex_hdl_t);
void                    pcibr_hints_subdevs(vertex_hdl_t, pciio_slot_t, uint64_t);

pcibr_hints_t
pcibr_hints_get(vertex_hdl_t xconn_vhdl, int alloc)
{
    arbitrary_info_t        ainfo = 0;
    graph_error_t	    rv;
    pcibr_hints_t           hint;

    rv = hwgraph_info_get_LBL(xconn_vhdl, INFO_LBL_PCIBR_HINTS, &ainfo);

    if (alloc && (rv != GRAPH_SUCCESS)) {

	NEW(hint);
	hint->rrb_alloc_funct = NULL;
	hint->ph_intr_bits = NULL;
	rv = hwgraph_info_add_LBL(xconn_vhdl, 
				  INFO_LBL_PCIBR_HINTS, 	
				  (arbitrary_info_t) hint);
	if (rv != GRAPH_SUCCESS)
	    goto abnormal_exit;

	rv = hwgraph_info_get_LBL(xconn_vhdl, INFO_LBL_PCIBR_HINTS, &ainfo);
	
	if (rv != GRAPH_SUCCESS)
	    goto abnormal_exit;

	if (ainfo != (arbitrary_info_t) hint)
	    goto abnormal_exit;
    }
    return (pcibr_hints_t) ainfo;

abnormal_exit:
#ifdef LATER
    printf("SHOULD NOT BE HERE\n");
#endif
    DEL(hint);
    return(NULL);

}

void
pcibr_hints_fix_some_rrbs(vertex_hdl_t xconn_vhdl, unsigned mask)
{
    pcibr_hints_t           hint = pcibr_hints_get(xconn_vhdl, 1);

    if (hint)
	hint->ph_rrb_fixed = mask;
    else
        PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
		    "pcibr_hints_fix_rrbs: pcibr_hints_get failed\n"));
}

void
pcibr_hints_fix_rrbs(vertex_hdl_t xconn_vhdl)
{
    pcibr_hints_fix_some_rrbs(xconn_vhdl, 0xFF);
}

void
pcibr_hints_dualslot(vertex_hdl_t xconn_vhdl,
		     pciio_slot_t host,
		     pciio_slot_t guest)
{
    pcibr_hints_t           hint = pcibr_hints_get(xconn_vhdl, 1);

    if (hint)
	hint->ph_host_slot[guest] = host + 1;
    else
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
		    "pcibr_hints_dualslot: pcibr_hints_get failed\n"));
}

void
pcibr_hints_intr_bits(vertex_hdl_t xconn_vhdl,
		      pcibr_intr_bits_f *xxx_intr_bits)
{
    pcibr_hints_t           hint = pcibr_hints_get(xconn_vhdl, 1);

    if (hint)
	hint->ph_intr_bits = xxx_intr_bits;
    else
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
		    "pcibr_hints_intr_bits: pcibr_hints_get failed\n"));
}

void
pcibr_set_rrb_callback(vertex_hdl_t xconn_vhdl, rrb_alloc_funct_t rrb_alloc_funct)
{
    pcibr_hints_t           hint = pcibr_hints_get(xconn_vhdl, 1);

    if (hint)
	hint->rrb_alloc_funct = rrb_alloc_funct;
}

void
pcibr_hints_handsoff(vertex_hdl_t xconn_vhdl)
{
    pcibr_hints_t           hint = pcibr_hints_get(xconn_vhdl, 1);

    if (hint)
	hint->ph_hands_off = 1;
    else
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
		    "pcibr_hints_handsoff: pcibr_hints_get failed\n"));
}

void
pcibr_hints_subdevs(vertex_hdl_t xconn_vhdl,
		    pciio_slot_t slot,
		    uint64_t subdevs)
{
    arbitrary_info_t        ainfo = 0;
    char                    sdname[16];
    vertex_hdl_t            pconn_vhdl = GRAPH_VERTEX_NONE;

    sprintf(sdname, "%s/%d", EDGE_LBL_PCI, slot);
    (void) hwgraph_path_add(xconn_vhdl, sdname, &pconn_vhdl);
    if (pconn_vhdl == GRAPH_VERTEX_NONE) {
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
		    "pcibr_hints_subdevs: hwgraph_path_create failed\n"));
	return;
    }
    hwgraph_info_get_LBL(pconn_vhdl, INFO_LBL_SUBDEVS, &ainfo);
    if (ainfo == 0) {
	uint64_t                *subdevp;

	NEW(subdevp);
	if (!subdevp) {
	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
			"pcibr_hints_subdevs: subdev ptr alloc failed\n"));
	    return;
	}
	*subdevp = subdevs;
	hwgraph_info_add_LBL(pconn_vhdl, INFO_LBL_SUBDEVS, (arbitrary_info_t) subdevp);
	hwgraph_info_get_LBL(pconn_vhdl, INFO_LBL_SUBDEVS, &ainfo);
	if (ainfo == (arbitrary_info_t) subdevp)
	    return;
	DEL(subdevp);
	if (ainfo == (arbitrary_info_t) NULL) {
	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
			"pcibr_hints_subdevs: null subdevs ptr\n"));
	    return;
	}
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, xconn_vhdl,
		    "pcibr_subdevs_get: dup subdev add_LBL\n"));
    }
    *(uint64_t *) ainfo = subdevs;
}
