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

#ifndef LOCAL
#define LOCAL           static
#endif

/*
 * functions
 */
int               pcibr_init_ext_ate_ram(bridge_t *);
int               pcibr_ate_alloc(pcibr_soft_t, int);
void              pcibr_ate_free(pcibr_soft_t, int, int);
bridge_ate_t      pcibr_flags_to_ate(unsigned);
bridge_ate_p      pcibr_ate_addr(pcibr_soft_t, int);
unsigned 	  ate_freeze(pcibr_dmamap_t pcibr_dmamap,
#if PCIBR_FREEZE_TIME
	   			unsigned *freeze_time_ptr,
#endif
	   			unsigned *cmd_regs);
void 	  ate_write(pcibr_soft_t pcibr_soft, bridge_ate_p ate_ptr, int ate_count, bridge_ate_t ate);
void ate_thaw(pcibr_dmamap_t pcibr_dmamap,
	 			int ate_index,
#if PCIBR_FREEZE_TIME
	 			bridge_ate_t ate,
	 			int ate_total,
	 			unsigned freeze_time_start,
#endif
	 			unsigned *cmd_regs,
	 			unsigned s);


/* Convert from ssram_bits in control register to number of SSRAM entries */
#define ATE_NUM_ENTRIES(n) _ate_info[n]

/* Possible choices for number of ATE entries in Bridge's SSRAM */
LOCAL int               _ate_info[] =
{
    0,					/* 0 entries */
    8 * 1024,				/* 8K entries */
    16 * 1024,				/* 16K entries */
    64 * 1024				/* 64K entries */
};

#define ATE_NUM_SIZES (sizeof(_ate_info) / sizeof(int))
#define ATE_PROBE_VALUE 0x0123456789abcdefULL

/*
 * Determine the size of this bridge's external mapping SSRAM, and set
 * the control register appropriately to reflect this size, and initialize
 * the external SSRAM.
 */
int
pcibr_init_ext_ate_ram(bridge_t *bridge)
{
    int                     largest_working_size = 0;
    int                     num_entries, entry;
    int                     i, j;
    bridgereg_t             old_enable, new_enable;

    /* Probe SSRAM to determine its size. */
    old_enable = bridge->b_int_enable;
    new_enable = old_enable & ~BRIDGE_IMR_PCI_MST_TIMEOUT;
    bridge->b_int_enable = new_enable;

    for (i = 1; i < ATE_NUM_SIZES; i++) {
	/* Try writing a value */
	bridge->b_ext_ate_ram[ATE_NUM_ENTRIES(i) - 1] = ATE_PROBE_VALUE;

	/* Guard against wrap */
	for (j = 1; j < i; j++)
	    bridge->b_ext_ate_ram[ATE_NUM_ENTRIES(j) - 1] = 0;

	/* See if value was written */
	if (bridge->b_ext_ate_ram[ATE_NUM_ENTRIES(i) - 1] == ATE_PROBE_VALUE)
				largest_working_size = i;
    }
    bridge->b_int_enable = old_enable;
    bridge->b_wid_tflush;		/* wait until Bridge PIO complete */

    /*
     * ensure that we write and read without any interruption.
     * The read following the write is required for the Bridge war
     */

    bridge->b_wid_control = (bridge->b_wid_control
			& ~BRIDGE_CTRL_SSRAM_SIZE_MASK)
			| BRIDGE_CTRL_SSRAM_SIZE(largest_working_size);
    bridge->b_wid_control;		/* inval addr bug war */

    num_entries = ATE_NUM_ENTRIES(largest_working_size);

    if (pcibr_debug_mask & PCIBR_DEBUG_ATE) {
	if (num_entries) {
	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATE, NULL,
			"bridge at 0x%x: clearing %d external ATEs\n",
			bridge, num_entries));
	} else {
	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATE, NULL,
			"bridge at 0x%x: no external ATE RAM found\n",
			bridge));
	}
    }

    /* Initialize external mapping entries */
    for (entry = 0; entry < num_entries; entry++)
	bridge->b_ext_ate_ram[entry] = 0;

    return (num_entries);
}

/*
 * Allocate "count" contiguous Bridge Address Translation Entries
 * on the specified bridge to be used for PCI to XTALK mappings.
 * Indices in rm map range from 1..num_entries.  Indicies returned
 * to caller range from 0..num_entries-1.
 *
 * Return the start index on success, -1 on failure.
 */
int
pcibr_ate_alloc(pcibr_soft_t pcibr_soft, int count)
{
    int			    status = 0;
    struct resource	    *new_res;
    struct resource         **allocated_res;

    new_res = (struct resource *) kmalloc( sizeof(struct resource), GFP_ATOMIC);
    memset(new_res, 0, sizeof(*new_res));
    status = allocate_resource( &pcibr_soft->bs_int_ate_resource, new_res,
				count, pcibr_soft->bs_int_ate_resource.start, 
				pcibr_soft->bs_int_ate_resource.end, 1,
				NULL, NULL);

    if ( status && (pcibr_soft->bs_ext_ate_resource.end != 0) ) {
	status = allocate_resource( &pcibr_soft->bs_ext_ate_resource, new_res,
				count, pcibr_soft->bs_ext_ate_resource.start,
				pcibr_soft->bs_ext_ate_resource.end, 1,
				NULL, NULL);
	if (status) {
		new_res->start = -1;
	}
    }

    if (status) {
	/* Failed to allocate */
	kfree(new_res);
	return -1;
    }

    /* Save the resource for freeing */
    allocated_res = (struct resource **)(((unsigned long)pcibr_soft->bs_allocated_ate_res) + new_res->start * sizeof( unsigned long));
    *allocated_res = new_res;

    return new_res->start;
}

void
pcibr_ate_free(pcibr_soft_t pcibr_soft, int index, int count)
/* Who says there's no such thing as a free meal? :-) */
{

    struct resource **allocated_res;
    int status = 0;

    allocated_res = (struct resource **)(((unsigned long)pcibr_soft->bs_allocated_ate_res) + index * sizeof(unsigned long));

    status = release_resource(*allocated_res);
    if (status)
	BUG(); /* Ouch .. */
    kfree(*allocated_res);

}

/*
 * Convert PCI-generic software flags and Bridge-specific software flags
 * into Bridge-specific Address Translation Entry attribute bits.
 */
bridge_ate_t
pcibr_flags_to_ate(unsigned flags)
{
    bridge_ate_t            attributes;

    /* default if nothing specified:
     * NOBARRIER
     * NOPREFETCH
     * NOPRECISE
     * COHERENT
     * Plus the valid bit
     */
    attributes = ATE_CO | ATE_V;

    /* Generic macro flags
     */
    if (flags & PCIIO_DMA_DATA) {	/* standard data channel */
	attributes &= ~ATE_BAR;		/* no barrier */
	attributes |= ATE_PREF;		/* prefetch on */
    }
    if (flags & PCIIO_DMA_CMD) {	/* standard command channel */
	attributes |= ATE_BAR;		/* barrier bit on */
	attributes &= ~ATE_PREF;	/* disable prefetch */
    }
    /* Generic detail flags
     */
    if (flags & PCIIO_PREFETCH)
	attributes |= ATE_PREF;
    if (flags & PCIIO_NOPREFETCH)
	attributes &= ~ATE_PREF;

    /* Provider-specific flags
     */
    if (flags & PCIBR_BARRIER)
	attributes |= ATE_BAR;
    if (flags & PCIBR_NOBARRIER)
	attributes &= ~ATE_BAR;

    if (flags & PCIBR_PREFETCH)
	attributes |= ATE_PREF;
    if (flags & PCIBR_NOPREFETCH)
	attributes &= ~ATE_PREF;

    if (flags & PCIBR_PRECISE)
	attributes |= ATE_PREC;
    if (flags & PCIBR_NOPRECISE)
	attributes &= ~ATE_PREC;

    return (attributes);
}

/*
 * Setup an Address Translation Entry as specified.  Use either the Bridge
 * internal maps or the external map RAM, as appropriate.
 */
bridge_ate_p
pcibr_ate_addr(pcibr_soft_t pcibr_soft,
	       int ate_index)
{
    bridge_t *bridge = pcibr_soft->bs_base;

    return (ate_index < pcibr_soft->bs_int_ate_size)
	? &(bridge->b_int_ate_ram[ate_index].wr)
	: &(bridge->b_ext_ate_ram[ate_index]);
}

/* We are starting to get more complexity
 * surrounding writing ATEs, so pull
 * the writing code into this new function.
 */

#if PCIBR_FREEZE_TIME
#define	ATE_FREEZE()	s = ate_freeze(pcibr_dmamap, &freeze_time, cmd_regs)
#else
#define	ATE_FREEZE()	s = ate_freeze(pcibr_dmamap, cmd_regs)
#endif

unsigned
ate_freeze(pcibr_dmamap_t pcibr_dmamap,
#if PCIBR_FREEZE_TIME
	   unsigned *freeze_time_ptr,
#endif
	   unsigned *cmd_regs)
{
    pcibr_soft_t            pcibr_soft = pcibr_dmamap->bd_soft;
#ifdef LATER
    int                     dma_slot = pcibr_dmamap->bd_slot;
#endif
    int                     ext_ates = pcibr_dmamap->bd_flags & PCIBR_DMAMAP_SSRAM;
    int                     slot;

    unsigned long           s;
    unsigned                cmd_reg;
    volatile unsigned      *cmd_lwa;
    unsigned                cmd_lwd;

    if (!ext_ates)
	return 0;

    /* Bridge Hardware Bug WAR #484930:
     * Bridge can't handle updating External ATEs
     * while DMA is occurring that uses External ATEs,
     * even if the particular ATEs involved are disjoint.
     */

    /* need to prevent anyone else from
     * unfreezing the grant while we
     * are working; also need to prevent
     * this thread from being interrupted
     * to keep PCI grant freeze time
     * at an absolute minimum.
     */
    s = pcibr_lock(pcibr_soft);

#ifdef LATER
    /* just in case pcibr_dmamap_done was not called */
    if (pcibr_dmamap->bd_flags & PCIBR_DMAMAP_BUSY) {
	pcibr_dmamap->bd_flags &= ~PCIBR_DMAMAP_BUSY;
	if (pcibr_dmamap->bd_flags & PCIBR_DMAMAP_SSRAM)
	    atomic_dec(&(pcibr_soft->bs_slot[dma_slot]. bss_ext_ates_active));
	xtalk_dmamap_done(pcibr_dmamap->bd_xtalk);
    }
#endif	/* LATER */
#if PCIBR_FREEZE_TIME
    *freeze_time_ptr = get_timestamp();
#endif

    cmd_lwa = 0;
    for (slot = pcibr_soft->bs_min_slot; 
		slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot)
	if (atomic_read(&pcibr_soft->bs_slot[slot].bss_ext_ates_active)) {
	    cmd_reg = pcibr_soft->
		bs_slot[slot].
		bss_cmd_shadow;
	    if (cmd_reg & PCI_CMD_BUS_MASTER) {
		cmd_lwa = pcibr_soft->
		    bs_slot[slot].
		    bss_cmd_pointer;
		cmd_lwd = cmd_reg ^ PCI_CMD_BUS_MASTER;
		cmd_lwa[0] = cmd_lwd;
	    }
	    cmd_regs[slot] = cmd_reg;
	} else
	    cmd_regs[slot] = 0;

    if (cmd_lwa) {
	    bridge_t	*bridge = pcibr_soft->bs_base;

	    /* Read the last master bit that has been cleared. This PIO read
	     * on the PCI bus is to ensure the completion of any DMAs that
	     * are due to bus requests issued by PCI devices before the
	     * clearing of master bits.
	     */
	    cmd_lwa[0];

	    /* Flush all the write buffers in the bridge */
	    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot) {
		    if (atomic_read(&pcibr_soft->bs_slot[slot].bss_ext_ates_active)) {
			    /* Flush the write buffer associated with this
			     * PCI device which might be using dma map RAM.
			     */
			bridge->b_wr_req_buf[slot].reg;
		    }
	    }
    }
    return s;
}

void
ate_write(pcibr_soft_t pcibr_soft,
	  bridge_ate_p ate_ptr,
	  int ate_count,
	  bridge_ate_t ate)
{
    	while (ate_count-- > 0) {
		*ate_ptr++ = ate;
		ate += IOPGSIZE;
	}
}

#if PCIBR_FREEZE_TIME
#define	ATE_THAW()	ate_thaw(pcibr_dmamap, ate_index, ate, ate_total, freeze_time, cmd_regs, s)
#else
#define	ATE_THAW()	ate_thaw(pcibr_dmamap, ate_index, cmd_regs, s)
#endif

void
ate_thaw(pcibr_dmamap_t pcibr_dmamap,
	 int ate_index,
#if PCIBR_FREEZE_TIME
	 bridge_ate_t ate,
	 int ate_total,
	 unsigned freeze_time_start,
#endif
	 unsigned *cmd_regs,
	 unsigned s)
{
    pcibr_soft_t            pcibr_soft = pcibr_dmamap->bd_soft;
    int                     dma_slot = pcibr_dmamap->bd_slot;
    int                     slot;
    bridge_t               *bridge = pcibr_soft->bs_base;
    int                     ext_ates = pcibr_dmamap->bd_flags & PCIBR_DMAMAP_SSRAM;

    unsigned                cmd_reg;

#if PCIBR_FREEZE_TIME
    unsigned                freeze_time;
    static unsigned         max_freeze_time = 0;
    static unsigned         max_ate_total;
#endif

    if (!ext_ates)
	return;

    /* restore cmd regs */
    for (slot = pcibr_soft->bs_min_slot; 
		slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot) {
	if ((cmd_reg = cmd_regs[slot]) & PCI_CMD_BUS_MASTER) {
		pcibr_slot_config_set(bridge, slot, PCI_CFG_COMMAND/4, cmd_reg);
	}
    }
    pcibr_dmamap->bd_flags |= PCIBR_DMAMAP_BUSY;
    atomic_inc(&(pcibr_soft->bs_slot[dma_slot]. bss_ext_ates_active));

#if PCIBR_FREEZE_TIME
    freeze_time = get_timestamp() - freeze_time_start;

    if ((max_freeze_time < freeze_time) ||
	(max_ate_total < ate_total)) {
	if (max_freeze_time < freeze_time)
	    max_freeze_time = freeze_time;
	if (max_ate_total < ate_total)
	    max_ate_total = ate_total;
	pcibr_unlock(pcibr_soft, s);
	printk( "%s: pci freeze time %d usec for %d ATEs\n"
		"\tfirst ate: %R\n",
		pcibr_soft->bs_name,
		freeze_time * 1000 / 1250,
		ate_total,
		ate, ate_bits);
    } else
#endif
	pcibr_unlock(pcibr_soft, s);
}
