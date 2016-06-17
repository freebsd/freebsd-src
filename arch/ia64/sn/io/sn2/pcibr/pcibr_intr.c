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

#ifdef __ia64
inline int
compare_and_swap_ptr(void **location, void *old_ptr, void *new_ptr)
{
	/* FIXME - compare_and_swap_ptr NOT ATOMIC */
	if (*location == old_ptr) {
		*location = new_ptr;
		return(1);
	}
	else
		return(0);
}
#endif

unsigned		pcibr_intr_bits(pciio_info_t info, pciio_intr_line_t lines, int nslots);
pcibr_intr_t            pcibr_intr_alloc(vertex_hdl_t, device_desc_t, pciio_intr_line_t, vertex_hdl_t);
void                    pcibr_intr_free(pcibr_intr_t);
void              pcibr_setpciint(xtalk_intr_t);
int                     pcibr_intr_connect(pcibr_intr_t, intr_func_t, intr_arg_t);
void                    pcibr_intr_disconnect(pcibr_intr_t);

vertex_hdl_t            pcibr_intr_cpu_get(pcibr_intr_t);
void                    pcibr_xintr_preset(void *, int, xwidgetnum_t, iopaddr_t, xtalk_intr_vector_t);
void                    pcibr_intr_func(intr_arg_t);

extern pcibr_info_t      pcibr_info_get(vertex_hdl_t);

/* =====================================================================
 *    INTERRUPT MANAGEMENT
 */

unsigned
pcibr_intr_bits(pciio_info_t info,
		pciio_intr_line_t lines, int nslots)
{
    pciio_slot_t            slot = PCIBR_INFO_SLOT_GET_INT(info);
    unsigned		    bbits = 0;

    /*
     * Currently favored mapping from PCI
     * slot number and INTA/B/C/D to Bridge
     * PCI Interrupt Bit Number:
     *
     *     SLOT     A B C D
     *      0       0 4 0 4
     *      1       1 5 1 5
     *      2       2 6 2 6
     *      3       3 7 3 7
     *      4       4 0 4 0
     *      5       5 1 5 1
     *      6       6 2 6 2
     *      7       7 3 7 3
     */

    if (slot < nslots) {
	if (lines & (PCIIO_INTR_LINE_A| PCIIO_INTR_LINE_C))
	    bbits |= 1 << slot;
	if (lines & (PCIIO_INTR_LINE_B| PCIIO_INTR_LINE_D))
	    bbits |= 1 << (slot ^ 4);
    }
    return bbits;
}


/*
 *	Get the next wrapper pointer queued in the interrupt circular buffer.
 */
pcibr_intr_wrap_t
pcibr_wrap_get(pcibr_intr_cbuf_t cbuf)
{
    pcibr_intr_wrap_t	wrap;

	if (cbuf->ib_in == cbuf->ib_out)
	    panic( "pcibr intr circular buffer empty, cbuf=0x%p, ib_in=ib_out=%d\n",
		(void *)cbuf, cbuf->ib_out);

	wrap = cbuf->ib_cbuf[cbuf->ib_out++];
	cbuf->ib_out = cbuf->ib_out % IBUFSIZE;
	return(wrap);
}

/* 
 *	Queue a wrapper pointer in the interrupt circular buffer.
 */
void
pcibr_wrap_put(pcibr_intr_wrap_t wrap, pcibr_intr_cbuf_t cbuf)
{
	int	in;

	/*
	 * Multiple CPUs could be executing this code simultaneously
	 * if a handler has registered multiple interrupt lines and
	 * the interrupts are directed to different CPUs.
	 */
	spin_lock(&cbuf->ib_lock);
	in = (cbuf->ib_in + 1) % IBUFSIZE;
	if (in == cbuf->ib_out) 
	    panic( "pcibr intr circular buffer full, cbuf=0x%p, ib_in=%d\n",
		(void *)cbuf, cbuf->ib_in);

	cbuf->ib_cbuf[cbuf->ib_in] = wrap;
	cbuf->ib_in = in;
	spin_unlock(&cbuf->ib_lock);
	return;
}

/*
 *	On SN systems there is a race condition between a PIO read response
 *	and DMA's.  In rare cases, the read response may beat the DMA, causing
 *	the driver to think that data in memory is complete and meaningful.
 *	This code eliminates that race.
 *	This routine is called by the PIO read routines after doing the read.
 *	This routine then forces a fake interrupt on another line, which
 *	is logically associated with the slot that the PIO is addressed to.
 *	(see sn_dma_flush_init() )
 *	It then spins while watching the memory location that the interrupt
 *	is targetted to.  When the interrupt response arrives, we are sure
 *	that the DMA has landed in memory and it is safe for the driver
 *	to proceed.
 */

extern struct sn_flush_nasid_entry flush_nasid_list[MAX_NASIDS];

void
sn_dma_flush(unsigned long addr) {
	nasid_t nasid;
	int wid_num;
	volatile struct sn_flush_device_list *p;
	int i,j;
	int bwin;
	unsigned long flags;

	nasid = NASID_GET(addr);
	wid_num = SWIN_WIDGETNUM(addr);
	bwin = BWIN_WINDOWNUM(addr);

	if (flush_nasid_list[nasid].widget_p == NULL) return;
	if (bwin > 0) {
		bwin--;
		switch (bwin) {
			case 0:
				wid_num = ((flush_nasid_list[nasid].iio_itte1) >> 8) & 0xf;
				break;
			case 1:
				wid_num = ((flush_nasid_list[nasid].iio_itte2) >> 8) & 0xf;
				break;
			case 2: 
				wid_num = ((flush_nasid_list[nasid].iio_itte3) >> 8) & 0xf;
				break;
			case 3: 
				wid_num = ((flush_nasid_list[nasid].iio_itte4) >> 8) & 0xf;
				break;
			case 4: 
				wid_num = ((flush_nasid_list[nasid].iio_itte5) >> 8) & 0xf;
				break;
			case 5: 
				wid_num = ((flush_nasid_list[nasid].iio_itte6) >> 8) & 0xf;
				break;
			case 6: 
				wid_num = ((flush_nasid_list[nasid].iio_itte7) >> 8) & 0xf;
				break;
		}
	}
	if (flush_nasid_list[nasid].widget_p == NULL) return;
	if (flush_nasid_list[nasid].widget_p[wid_num] == NULL) return;
	p = &flush_nasid_list[nasid].widget_p[wid_num][0];

	// find a matching BAR

	for (i=0; i<DEV_PER_WIDGET;i++) {
		for (j=0; j<PCI_ROM_RESOURCE;j++) {
			if (p->bar_list[j].start == 0) break;
			if (addr >= p->bar_list[j].start && addr <= p->bar_list[j].end) break;
		}
		if (j < PCI_ROM_RESOURCE && p->bar_list[j].start != 0) break;
		p++;
	}

	// if no matching BAR, return without doing anything.

	if (i == DEV_PER_WIDGET) return;

	spin_lock_irqsave(&p->flush_lock, flags);

	p->flush_addr = 0;

	// force an interrupt.

	*(bridgereg_t *)(p->force_int_addr) = 1;

	// wait for the interrupt to come back.

	while (p->flush_addr != 0x10f);

	// okay, everything is synched up.
	spin_unlock_irqrestore(&p->flush_lock, flags);

	return;
}

EXPORT_SYMBOL(sn_dma_flush);

/*
 *	There are end cases where a deadlock can occur if interrupt 
 *	processing completes and the Bridge b_int_status bit is still set.
 *
 *	One scenerio is if a second PCI interrupt occurs within 60ns of
 *	the previous interrupt being cleared. In this case the Bridge
 *	does not detect the transition, the Bridge b_int_status bit
 *	remains set, and because no transition was detected no interrupt
 *	packet is sent to the Hub/Heart.
 *
 *	A second scenerio is possible when a b_int_status bit is being
 *	shared by multiple devices:
 *						Device #1 generates interrupt
 *						Bridge b_int_status bit set
 *						Device #2 generates interrupt
 *		interrupt processing begins
 *		  ISR for device #1 runs and
 *			clears interrupt
 *						Device #1 generates interrupt
 *		  ISR for device #2 runs and
 *			clears interrupt
 *						(b_int_status bit still set)
 *		interrupt processing completes
 *		  
 *	Interrupt processing is now complete, but an interrupt is still
 *	outstanding for Device #1. But because there was no transition of
 *	the b_int_status bit, no interrupt packet will be generated and
 *	a deadlock will occur.
 *
 *	To avoid these deadlock situations, this function is used
 *	to check if a specific Bridge b_int_status bit is set, and if so,
 *	cause the setting of the corresponding interrupt bit.
 *
 *	On a XBridge (SN1) and PIC (SN2), we do this by writing the appropriate Bridge Force 
 *	Interrupt register.
 */
void
pcibr_force_interrupt(pcibr_intr_t intr)
{
	unsigned	bit;
	unsigned	bits;
	pcibr_soft_t    pcibr_soft = intr->bi_soft;
	bridge_t       *bridge = pcibr_soft->bs_base;

	bits = intr->bi_ibits;
	for (bit = 0; bit < 8; bit++) {
		if (bits & (1 << bit)) {

			PCIBR_DEBUG((PCIBR_DEBUG_INTR, pcibr_soft->bs_vhdl,
		    		"pcibr_force_interrupt: bit=0x%x\n", bit));

			if (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) {
	    			bridge->b_force_pin[bit].intr = 1;
			}
		}
	}
}

/*ARGSUSED */
pcibr_intr_t
pcibr_intr_alloc(vertex_hdl_t pconn_vhdl,
		 device_desc_t dev_desc,
		 pciio_intr_line_t lines,
		 vertex_hdl_t owner_dev)
{
    pcibr_info_t            pcibr_info = pcibr_info_get(pconn_vhdl);
    pciio_slot_t            pciio_slot = PCIBR_INFO_SLOT_GET_INT(pcibr_info);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pcibr_info->f_mfast;
    vertex_hdl_t            xconn_vhdl = pcibr_soft->bs_conn;
    bridge_t               *bridge = pcibr_soft->bs_base;
    int                     is_threaded = 0;

    xtalk_intr_t           *xtalk_intr_p;
    pcibr_intr_t           *pcibr_intr_p;
    pcibr_intr_list_t      *intr_list_p;

    unsigned                pcibr_int_bits;
    unsigned                pcibr_int_bit;
    xtalk_intr_t            xtalk_intr = (xtalk_intr_t)0;
    hub_intr_t		    hub_intr;
    pcibr_intr_t            pcibr_intr;
    pcibr_intr_list_t       intr_entry;
    pcibr_intr_list_t       intr_list;
    bridgereg_t             int_dev;


    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
    		"pcibr_intr_alloc: %s%s%s%s%s\n",
		!(lines & 15) ? " No INTs?" : "",
		lines & 1 ? " INTA" : "",
		lines & 2 ? " INTB" : "",
		lines & 4 ? " INTC" : "",
		lines & 8 ? " INTD" : ""));

    NEW(pcibr_intr);
    if (!pcibr_intr)
	return NULL;

    pcibr_intr->bi_dev = pconn_vhdl;
    pcibr_intr->bi_lines = lines;
    pcibr_intr->bi_soft = pcibr_soft;
    pcibr_intr->bi_ibits = 0;		/* bits will be added below */
    pcibr_intr->bi_func = 0;            /* unset until connect */
    pcibr_intr->bi_arg = 0;             /* unset until connect */
    pcibr_intr->bi_flags = is_threaded ? 0 : PCIIO_INTR_NOTHREAD;
    pcibr_intr->bi_mustruncpu = CPU_NONE;
    pcibr_intr->bi_ibuf.ib_in = 0;
    pcibr_intr->bi_ibuf.ib_out = 0;
    spin_lock_init(&pcibr_intr->bi_ibuf.ib_lock);
    pcibr_int_bits = pcibr_soft->bs_intr_bits((pciio_info_t)pcibr_info, lines, 
		PCIBR_NUM_SLOTS(pcibr_soft));


    /*
     * For each PCI interrupt line requested, figure
     * out which Bridge PCI Interrupt Line it maps
     * to, and make sure there are xtalk resources
     * allocated for it.
     */
    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
		"pcibr_intr_alloc: pcibr_int_bits: 0x%x\n", pcibr_int_bits));
    for (pcibr_int_bit = 0; pcibr_int_bit < 8; pcibr_int_bit ++) {
	if (pcibr_int_bits & (1 << pcibr_int_bit)) {
	    xtalk_intr_p = &pcibr_soft->bs_intr[pcibr_int_bit].bsi_xtalk_intr;

	    xtalk_intr = *xtalk_intr_p;

	    if (xtalk_intr == NULL) {
		/*
		 * This xtalk_intr_alloc is constrained for two reasons:
		 * 1) Normal interrupts and error interrupts need to be delivered
		 *    through a single xtalk target widget so that there aren't any
		 *    ordering problems with DMA, completion interrupts, and error
		 *    interrupts. (Use of xconn_vhdl forces this.)
		 *
		 * 2) On SN1, addressing constraints on SN1 and Bridge force
		 *    us to use a single PI number for all interrupts from a
		 *    single Bridge. (SN1-specific code forces this).
		 */

		/*
		 * All code dealing with threaded PCI interrupt handlers
		 * is located at the pcibr level. Because of this,
		 * we always want the lower layers (hub/heart_intr_alloc, 
		 * intr_level_connect) to treat us as non-threaded so we
		 * don't set up a duplicate threaded environment. We make
		 * this happen by calling a special xtalk interface.
		 */
		xtalk_intr = xtalk_intr_alloc_nothd(xconn_vhdl, dev_desc, 
			owner_dev);

		PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
			    "pcibr_intr_alloc: xtalk_intr=0x%x\n", xtalk_intr));

		/* both an assert and a runtime check on this:
		 * we need to check in non-DEBUG kernels, and
		 * the ASSERT gets us more information when
		 * we use DEBUG kernels.
		 */
		ASSERT(xtalk_intr != NULL);
		if (xtalk_intr == NULL) {
		    /* it is quite possible that our
		     * xtalk_intr_alloc failed because
		     * someone else got there first,
		     * and we can find their results
		     * in xtalk_intr_p.
		     */
		    if (!*xtalk_intr_p) {
#ifdef SUPPORT_PRINTING_V_FORMAT
			printk(KERN_ALERT  
				"pcibr_intr_alloc %v: unable to get xtalk interrupt resources",
				xconn_vhdl);
#else
			printk(KERN_ALERT  
				"pcibr_intr_alloc 0x%p: unable to get xtalk interrupt resources",
				(void *)xconn_vhdl);
#endif
			/* yes, we leak resources here. */
			return 0;
		    }
		} else if (compare_and_swap_ptr((void **) xtalk_intr_p, NULL, xtalk_intr)) {
		    /*
		     * now tell the bridge which slot is
		     * using this interrupt line.
		     */
		    int_dev = bridge->b_int_device;
		    int_dev &= ~BRIDGE_INT_DEV_MASK(pcibr_int_bit);
		    int_dev |= pciio_slot << BRIDGE_INT_DEV_SHFT(pcibr_int_bit);
		    bridge->b_int_device = int_dev;	/* XXXMP */

		    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
		    		"bridge intr bit %d clears my wrb\n",
				pcibr_int_bit));
		} else {
		    /* someone else got one allocated first;
		     * free the one we just created, and
		     * retrieve the one they allocated.
		     */
		    xtalk_intr_free(xtalk_intr);
		    xtalk_intr = *xtalk_intr_p;
#if PARANOID
		    /* once xtalk_intr is set, we never clear it,
		     * so if the CAS fails above, this condition
		     * can "never happen" ...
		     */
		    if (!xtalk_intr) {
			printk(KERN_ALERT  
				"pcibr_intr_alloc %v: unable to set xtalk interrupt resources",
				xconn_vhdl);
			/* yes, we leak resources here. */
			return 0;
		    }
#endif
		}
	    }

	    pcibr_intr->bi_ibits |= 1 << pcibr_int_bit;

	    NEW(intr_entry);
	    intr_entry->il_next = NULL;
	    intr_entry->il_intr = pcibr_intr;
	    intr_entry->il_wrbf = &(bridge->b_wr_req_buf[pciio_slot].reg);
	    intr_list_p = 
		&pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_list;

	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
			"Bridge bit 0x%x wrap=0x%x\n", pcibr_int_bit,
			pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap));

	    if (compare_and_swap_ptr((void **) intr_list_p, NULL, intr_entry)) {
		/* we are the first interrupt on this bridge bit.
		 */
		PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
			    "INT 0x%x (bridge bit %d) allocated [FIRST]\n",
			    pcibr_int_bits, pcibr_int_bit));
		continue;
	    }
	    intr_list = *intr_list_p;
	    pcibr_intr_p = &intr_list->il_intr;
	    if (compare_and_swap_ptr((void **) pcibr_intr_p, NULL, pcibr_intr)) {
		/* first entry on list was erased,
		 * and we replaced it, so we
		 * don't need our intr_entry.
		 */
		DEL(intr_entry);
		PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
			    "INT 0x%x (bridge bit %d) replaces erased first\n",
			    pcibr_int_bits, pcibr_int_bit));
		continue;
	    }
	    intr_list_p = &intr_list->il_next;
	    if (compare_and_swap_ptr((void **) intr_list_p, NULL, intr_entry)) {
		/* we are the new second interrupt on this bit.
		 */
		pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_shared = 1;
		PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
			    "INT 0x%x (bridge bit %d) is new SECOND\n",
			    pcibr_int_bits, pcibr_int_bit));
		continue;
	    }
	    while (1) {
		pcibr_intr_p = &intr_list->il_intr;
		if (compare_and_swap_ptr((void **) pcibr_intr_p, NULL, pcibr_intr)) {
		    /* an entry on list was erased,
		     * and we replaced it, so we
		     * don't need our intr_entry.
		     */
		    DEL(intr_entry);

		    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
				"INT 0x%x (bridge bit %d) replaces erase Nth\n",
				pcibr_int_bits, pcibr_int_bit));
		    break;
		}
		intr_list_p = &intr_list->il_next;
		if (compare_and_swap_ptr((void **) intr_list_p, NULL, intr_entry)) {
		    /* entry appended to share list
		     */
		    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pconn_vhdl,
				"INT 0x%x (bridge bit %d) is new Nth\n",
				pcibr_int_bits, pcibr_int_bit));
		    break;
		}
		/* step to next record in chain
		 */
		intr_list = *intr_list_p;
	    }
	}
    }

#if DEBUG && INTR_DEBUG
    printk("%v pcibr_intr_alloc complete\n", pconn_vhdl);
#endif
    hub_intr = (hub_intr_t)xtalk_intr;
    pcibr_intr->bi_irq = hub_intr->i_bit;
    pcibr_intr->bi_cpu = hub_intr->i_cpuid;
    return pcibr_intr;
}

/*ARGSUSED */
void
pcibr_intr_free(pcibr_intr_t pcibr_intr)
{
    unsigned                pcibr_int_bits = pcibr_intr->bi_ibits;
    pcibr_soft_t            pcibr_soft = pcibr_intr->bi_soft;
    unsigned                pcibr_int_bit;
    pcibr_intr_list_t       intr_list;
    int			    intr_shared;
    xtalk_intr_t	    *xtalk_intrp;

    for (pcibr_int_bit = 0; pcibr_int_bit < 8; pcibr_int_bit++) {
	if (pcibr_int_bits & (1 << pcibr_int_bit)) {
	    for (intr_list = 
		     pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_list;
		 intr_list != NULL;
		 intr_list = intr_list->il_next)
		if (compare_and_swap_ptr((void **) &intr_list->il_intr, 
					 pcibr_intr, 
					 NULL)) {

		    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, 
				pcibr_intr->bi_dev,
		    		"pcibr_intr_free: cleared hdlr from bit 0x%x\n",
				pcibr_int_bit));
		}
	    /* If this interrupt line is not being shared between multiple
	     * devices release the xtalk interrupt resources.
	     */
	    intr_shared = 
		pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_shared;
	    xtalk_intrp = &pcibr_soft->bs_intr[pcibr_int_bit].bsi_xtalk_intr;

	    if ((!intr_shared) && (*xtalk_intrp)) {

		bridge_t 	*bridge = pcibr_soft->bs_base;
		bridgereg_t	int_dev;

		xtalk_intr_free(*xtalk_intrp);
		*xtalk_intrp = 0;

		/* Clear the PCI device interrupt to bridge interrupt pin
		 * mapping.
		 */
		int_dev = bridge->b_int_device;
		int_dev &= ~BRIDGE_INT_DEV_MASK(pcibr_int_bit);
		bridge->b_int_device = int_dev;

	    }
	}
    }
    DEL(pcibr_intr);
}

void
pcibr_setpciint(xtalk_intr_t xtalk_intr)
{
    iopaddr_t		 addr;
    xtalk_intr_vector_t	 vect;
    vertex_hdl_t	 vhdl;
    bridge_t		*bridge;
    picreg_t	*int_addr;

    addr = xtalk_intr_addr_get(xtalk_intr);
    vect = xtalk_intr_vector_get(xtalk_intr);
    vhdl = xtalk_intr_dev_get(xtalk_intr);
    bridge = (bridge_t *)xtalk_piotrans_addr(vhdl, 0, 0, sizeof(bridge_t), 0);

    int_addr = (picreg_t *)xtalk_intr_sfarg_get(xtalk_intr);
    *int_addr = ((PIC_INT_ADDR_FLD & ((uint64_t)vect << 48)) |
		     (PIC_INT_ADDR_HOST & addr));
}

/*ARGSUSED */
int
pcibr_intr_connect(pcibr_intr_t pcibr_intr, intr_func_t intr_func, intr_arg_t intr_arg)
{
    pcibr_soft_t            pcibr_soft = pcibr_intr->bi_soft;
    bridge_t               *bridge = pcibr_soft->bs_base;
    unsigned                pcibr_int_bits = pcibr_intr->bi_ibits;
    unsigned                pcibr_int_bit;
    uint64_t		    int_enable;
    unsigned long           s;

    if (pcibr_intr == NULL)
	return -1;

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pcibr_intr->bi_dev,
		"pcibr_intr_connect: intr_func=0x%x\n",
		pcibr_intr));

    pcibr_intr->bi_func = intr_func;
    pcibr_intr->bi_arg = intr_arg;
    *((volatile unsigned *)&pcibr_intr->bi_flags) |= PCIIO_INTR_CONNECTED;

    /*
     * For each PCI interrupt line requested, figure
     * out which Bridge PCI Interrupt Line it maps
     * to, and make sure there are xtalk resources
     * allocated for it.
     */
    for (pcibr_int_bit = 0; pcibr_int_bit < 8; pcibr_int_bit++)
	if (pcibr_int_bits & (1 << pcibr_int_bit)) {
            pcibr_intr_wrap_t       intr_wrap;
	    xtalk_intr_t            xtalk_intr;
            void                   *int_addr;

	    xtalk_intr = pcibr_soft->bs_intr[pcibr_int_bit].bsi_xtalk_intr;
	    intr_wrap = &pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap;

	    /*
	     * If this interrupt line is being shared and the connect has
	     * already been done, no need to do it again.
	     */
	    if (pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_connected)
		continue;


	    /*
	     * Use the pcibr wrapper function to handle all Bridge interrupts
	     * regardless of whether the interrupt line is shared or not.
	     */
	    if (IS_PIC_SOFT(pcibr_soft)) 
		int_addr = (void *)&(bridge->p_int_addr_64[pcibr_int_bit]);
	    else
		int_addr = (void *)&(bridge->b_int_addr[pcibr_int_bit].addr);

	    xtalk_intr_connect(xtalk_intr, pcibr_intr_func, (intr_arg_t) intr_wrap,
					(xtalk_intr_setfunc_t) pcibr_setpciint,
			       			(void *)int_addr);

	    pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_connected = 1;

	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pcibr_intr->bi_dev,
			"pcibr_setpciint: int_addr=0x%x, *int_addr=0x%x, "
			"pcibr_int_bit=0x%x\n", int_addr,
			 *(picreg_t *)int_addr,
			pcibr_int_bit));
	}

	/* PIC WAR. PV# 854697
	 * On PIC we must write 64-bit MMRs with 64-bit stores
	 */
	s = pcibr_lock(pcibr_soft);
	if (IS_PIC_SOFT(pcibr_soft) &&
			PCIBR_WAR_ENABLED(PV854697, pcibr_soft)) {
	    int_enable = bridge->p_int_enable_64;
	    int_enable |= pcibr_int_bits;
	    bridge->p_int_enable_64 = int_enable;
	} else {
	    bridgereg_t int_enable;

	    int_enable = bridge->b_int_enable;
	    int_enable |= pcibr_int_bits;
	    bridge->b_int_enable = int_enable;
	}
	bridge->b_wid_tflush;	/* wait until Bridge PIO complete */
	pcibr_unlock(pcibr_soft, s);

    return 0;
}

/*ARGSUSED */
void
pcibr_intr_disconnect(pcibr_intr_t pcibr_intr)
{
    pcibr_soft_t            pcibr_soft = pcibr_intr->bi_soft;
    bridge_t               *bridge = pcibr_soft->bs_base;
    unsigned                pcibr_int_bits = pcibr_intr->bi_ibits;
    unsigned                pcibr_int_bit;
    pcibr_intr_wrap_t       intr_wrap;
    uint64_t                int_enable;
    unsigned long           s;

    /* Stop calling the function. Now.
     */
    *((volatile unsigned *)&pcibr_intr->bi_flags) &= ~PCIIO_INTR_CONNECTED;
    pcibr_intr->bi_func = 0;
    pcibr_intr->bi_arg = 0;
    /*
     * For each PCI interrupt line requested, figure
     * out which Bridge PCI Interrupt Line it maps
     * to, and disconnect the interrupt.
     */

    /* don't disable interrupts for lines that
     * are shared between devices.
     */
    for (pcibr_int_bit = 0; pcibr_int_bit < 8; pcibr_int_bit++)
	if ((pcibr_int_bits & (1 << pcibr_int_bit)) &&
	    (pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_shared))
	    pcibr_int_bits &= ~(1 << pcibr_int_bit);
    if (!pcibr_int_bits)
	return;

    /* PIC WAR. PV# 854697
     * On PIC we must write 64-bit MMRs with 64-bit stores
     */
    s = pcibr_lock(pcibr_soft);
    if (IS_PIC_SOFT(pcibr_soft) && PCIBR_WAR_ENABLED(PV854697, pcibr_soft)) {
	int_enable = bridge->p_int_enable_64;
	int_enable &= ~pcibr_int_bits;
	bridge->p_int_enable_64 = int_enable;
    } else {
	int_enable = (uint64_t)bridge->b_int_enable;
	int_enable &= ~pcibr_int_bits;
	bridge->b_int_enable = (bridgereg_t)int_enable;
    }
    bridge->b_wid_tflush;		/* wait until Bridge PIO complete */
    pcibr_unlock(pcibr_soft, s);

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pcibr_intr->bi_dev,
		"pcibr_intr_disconnect: disabled int_bits=0x%x\n", 
		pcibr_int_bits));

    for (pcibr_int_bit = 0; pcibr_int_bit < 8; pcibr_int_bit++)
	if (pcibr_int_bits & (1 << pcibr_int_bit)) {
            void                   *int_addr;

	    /* if the interrupt line is now shared,
	     * do not disconnect it.
	     */
	    if (pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_shared)
		continue;

	    xtalk_intr_disconnect(pcibr_soft->bs_intr[pcibr_int_bit].bsi_xtalk_intr);
	    pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_connected = 0;

	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pcibr_intr->bi_dev,
			"pcibr_intr_disconnect: disconnect int_bits=0x%x\n",
			pcibr_int_bits));

	    /* if we are sharing the interrupt line,
	     * connect us up; this closes the hole
	     * where the another pcibr_intr_alloc()
	     * was in progress as we disconnected.
	     */
	    if (!pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_shared)
		continue;

	    intr_wrap = &pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap;
            if (!pcibr_soft->bs_intr[pcibr_int_bit].bsi_pcibr_intr_wrap.iw_shared)
                continue;

            if (IS_PIC_SOFT(pcibr_soft))
                int_addr = (void *)&(bridge->p_int_addr_64[pcibr_int_bit]);
            else
                int_addr = (void *)&(bridge->b_int_addr[pcibr_int_bit].addr);

	    xtalk_intr_connect(pcibr_soft->bs_intr[pcibr_int_bit].bsi_xtalk_intr,
				pcibr_intr_func, (intr_arg_t) intr_wrap,
			       (xtalk_intr_setfunc_t)pcibr_setpciint,
			       (void *)(long)pcibr_int_bit);
	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pcibr_intr->bi_dev,
			"pcibr_intr_disconnect: now-sharing int_bits=0x%x\n",
			pcibr_int_bit));
	}
}

/*ARGSUSED */
vertex_hdl_t
pcibr_intr_cpu_get(pcibr_intr_t pcibr_intr)
{
    pcibr_soft_t            pcibr_soft = pcibr_intr->bi_soft;
    unsigned                pcibr_int_bits = pcibr_intr->bi_ibits;
    unsigned                pcibr_int_bit;

    for (pcibr_int_bit = 0; pcibr_int_bit < 8; pcibr_int_bit++)
	if (pcibr_int_bits & (1 << pcibr_int_bit))
	    return xtalk_intr_cpu_get(pcibr_soft->bs_intr[pcibr_int_bit].bsi_xtalk_intr);
    return 0;
}

/* =====================================================================
 *    INTERRUPT HANDLING
 */
void
pcibr_clearwidint(bridge_t *bridge)
{
    bridge->b_wid_int_upper = 0;
    bridge->b_wid_int_lower = 0;
}


void
pcibr_setwidint(xtalk_intr_t intr)
{
    xwidgetnum_t            targ = xtalk_intr_target_get(intr);
    iopaddr_t               addr = xtalk_intr_addr_get(intr);
    xtalk_intr_vector_t     vect = xtalk_intr_vector_get(intr);
    widgetreg_t		    NEW_b_wid_int_upper, NEW_b_wid_int_lower;
    widgetreg_t		    OLD_b_wid_int_upper, OLD_b_wid_int_lower;

    bridge_t               *bridge = (bridge_t *)xtalk_intr_sfarg_get(intr);

    NEW_b_wid_int_upper = ( (0x000F0000 & (targ << 16)) |
			       XTALK_ADDR_TO_UPPER(addr));
    NEW_b_wid_int_lower = XTALK_ADDR_TO_LOWER(addr);

    OLD_b_wid_int_upper = bridge->b_wid_int_upper;
    OLD_b_wid_int_lower = bridge->b_wid_int_lower;

    /* Verify that all interrupts from this Bridge are using a single PI */
    if ((OLD_b_wid_int_upper != 0) && (OLD_b_wid_int_lower != 0)) {
	/*
	 * Once set, these registers shouldn't change; they should
	 * be set multiple times with the same values.
	 *
	 * If we're attempting to change these registers, it means
	 * that our heuristics for allocating interrupts in a way
	 * appropriate for IP35 have failed, and the admin needs to
	 * explicitly direct some interrupts (or we need to make the
	 * heuristics more clever).
	 *
	 * In practice, we hope this doesn't happen very often, if
	 * at all.
	 */
	if ((OLD_b_wid_int_upper != NEW_b_wid_int_upper) ||
	    (OLD_b_wid_int_lower != NEW_b_wid_int_lower)) {
		printk(KERN_WARNING  "Interrupt allocation is too complex.\n");
		printk(KERN_WARNING  "Use explicit administrative interrupt targetting.\n");
		printk(KERN_WARNING  "bridge=0x%lx targ=0x%x\n", (unsigned long)bridge, targ);
		printk(KERN_WARNING  "NEW=0x%x/0x%x  OLD=0x%x/0x%x\n",
			NEW_b_wid_int_upper, NEW_b_wid_int_lower,
			OLD_b_wid_int_upper, OLD_b_wid_int_lower);
		panic("PCI Bridge interrupt targetting error\n");
	}
    }

    bridge->b_wid_int_upper = NEW_b_wid_int_upper;
    bridge->b_wid_int_lower = NEW_b_wid_int_lower;
    bridge->b_int_host_err = vect;

}

/*
 * pcibr_intr_preset: called during mlreset time
 * if the platform specific code needs to route
 * one of the Bridge's xtalk interrupts before the
 * xtalk infrastructure is available.
 */
void
pcibr_xintr_preset(void *which_widget,
		   int which_widget_intr,
		   xwidgetnum_t targ,
		   iopaddr_t addr,
		   xtalk_intr_vector_t vect)
{
    bridge_t               *bridge = (bridge_t *) which_widget;

    if (which_widget_intr == -1) {
	/* bridge widget error interrupt */
	bridge->b_wid_int_upper = ( (0x000F0000 & (targ << 16)) |
				   XTALK_ADDR_TO_UPPER(addr));
	bridge->b_wid_int_lower = XTALK_ADDR_TO_LOWER(addr);
	bridge->b_int_host_err = vect;
printk("pcibr_xintr_preset: b_wid_int_upper 0x%lx b_wid_int_lower 0x%lx b_int_host_err 0x%x\n",
	( (0x000F0000 & (targ << 16)) | XTALK_ADDR_TO_UPPER(addr)),
	XTALK_ADDR_TO_LOWER(addr), vect);

	/* turn on all interrupts except
	 * the PCI interrupt requests,
	 * at least at heart.
	 */
	bridge->b_int_enable |= ~BRIDGE_IMR_INT_MSK;

    } else {
	/* routing a PCI device interrupt.
	 * targ and low 38 bits of addr must
	 * be the same as the already set
	 * value for the widget error interrupt.
	 */
	bridge->b_int_addr[which_widget_intr].addr =
	    ((BRIDGE_INT_ADDR_HOST & (addr >> 30)) |
	     (BRIDGE_INT_ADDR_FLD & vect));
	/*
	 * now bridge can let it through;
	 * NB: still should be blocked at
	 * xtalk provider end, until the service
	 * function is set.
	 */
	bridge->b_int_enable |= 1 << vect;
    }
    bridge->b_wid_tflush;		/* wait until Bridge PIO complete */
}


/*
 * pcibr_intr_func()
 *
 * This is the pcibr interrupt "wrapper" function that is called,
 * in interrupt context, to initiate the interrupt handler(s) registered
 * (via pcibr_intr_alloc/connect) for the occurring interrupt. Non-threaded 
 * handlers will be called directly, and threaded handlers will have their 
 * thread woken up.
 */
void
pcibr_intr_func(intr_arg_t arg)
{
    pcibr_intr_wrap_t       wrap = (pcibr_intr_wrap_t) arg;
    reg_p                   wrbf;
    intr_func_t             func;
    pcibr_intr_t            intr;
    pcibr_intr_list_t       list;
    int                     clearit;
    int			    do_nonthreaded = 1;
    int			    is_threaded = 0;
    int			    x = 0;
    pcibr_soft_t            pcibr_soft = wrap->iw_soft;
    bridge_t               *bridge = pcibr_soft->bs_base;
    uint64_t		    p_enable = pcibr_soft->bs_int_enable;
    int			    bit = wrap->iw_ibit;

	/*
	 * PIC WAR.  PV#855272
	 * Early attempt at a workaround for the runaway
	 * interrupt problem.   Briefly disable the enable bit for
	 * this device.
	 */
	if (IS_PIC_SOFT(pcibr_soft) &&
			PCIBR_WAR_ENABLED(PV855272, pcibr_soft)) {
		unsigned s;

		/* disable-enable interrupts for this bridge pin */

		p_enable &= ~(1 << bit);
	        s = pcibr_lock(pcibr_soft);
		bridge->p_int_enable_64 = p_enable;
		p_enable |= (1 << bit);
		bridge->p_int_enable_64 = p_enable;
	        pcibr_unlock(pcibr_soft, s);
	}

	/*
	 * If any handler is still running from a previous interrupt
	 * just return. If there's a need to call the handler(s) again,
	 * another interrupt will be generated either by the device or by
	 * pcibr_force_interrupt().
	 */

	if (wrap->iw_hdlrcnt) {
		return;
	}

    /*
     * Call all interrupt handlers registered.
     * First, the pcibr_intrd threads for any threaded handlers will be
     * awoken, then any non-threaded handlers will be called sequentially.
     */
	
	clearit = 1;
	while (do_nonthreaded) {
	    for (list = wrap->iw_list; list != NULL; list = list->il_next) {
		if ((intr = list->il_intr) && (intr->bi_flags & PCIIO_INTR_CONNECTED)) {

		    /*
		     * This device may have initiated write
		     * requests since the bridge last saw
		     * an edge on this interrupt input; flushing
		     * the buffer prior to invoking the handler
		     * should help but may not be sufficient if we 
		     * get more requests after the flush, followed
		     * by the card deciding it wants service, before
		     * the interrupt handler checks to see if things need
		     * to be done.
		     *
		     * There is a similar race condition if
		     * an interrupt handler loops around and
		     * notices further service is required.
		     * Perhaps we need to have an explicit
		     * call that interrupt handlers need to
		     * do between noticing that DMA to memory
		     * has completed, but before observing the
		     * contents of memory?
		     */

		    if ((do_nonthreaded) && (!is_threaded)) {
			/* Non-threaded -  Call the interrupt handler at interrupt level */
			/* Only need to flush write buffers if sharing */

			if ((wrap->iw_shared) && (wrbf = list->il_wrbf)) {
			    if ((x = *wrbf))	/* write request buffer flush */
#ifdef SUPPORT_PRINTING_V_FORMAT
				printk(KERN_ALERT  "pcibr_intr_func %v: \n"
				    "write buffer flush failed, wrbf=0x%x\n", 
				    list->il_intr->bi_dev, wrbf);
#else
				printk(KERN_ALERT  "pcibr_intr_func %p: \n"
				    "write buffer flush failed, wrbf=0x%lx\n", 
				    (void *)list->il_intr->bi_dev, (long) wrbf);
#endif
			}
			func = intr->bi_func;
			if ( func )
				func(intr->bi_arg);
		    }
		    clearit = 0;
		}
	    }
	    do_nonthreaded = 0;

	    /*
	     * If the non-threaded handler was the last to complete,
	     * (i.e., no threaded handlers still running) force an
	     * interrupt to avoid a potential deadlock situation.
	     */
	    if (wrap->iw_hdlrcnt == 0) {
		pcibr_force_interrupt((pcibr_intr_t) wrap);
	    }
	}

	/* If there were no handlers,
	 * disable the interrupt and return.
	 * It will get enabled again after
	 * a handler is connected.
	 * If we don't do this, we would
	 * sit here and spin through the
	 * list forever.
	 */
	if (clearit) {
	    pcibr_soft_t            pcibr_soft = wrap->iw_soft;
	    bridge_t               *bridge = pcibr_soft->bs_base;
	    bridgereg_t             int_enable;
	    bridgereg_t		    mask = 1 << wrap->iw_ibit;
	    unsigned long           s;

	    /* PIC BRINUGP WAR (PV# 854697):
	     * On PIC we must write 64-bit MMRs with 64-bit stores
	     */
	    s = pcibr_lock(pcibr_soft);
	    if (IS_PIC_SOFT(pcibr_soft) &&
				PCIBR_WAR_ENABLED(PV854697, pcibr_soft)) {
		int_enable = bridge->p_int_enable_64;
		int_enable &= ~mask;
		bridge->p_int_enable_64 = int_enable;
	    } else {
		int_enable = (uint64_t)bridge->b_int_enable;
		int_enable &= ~mask;
		bridge->b_int_enable = (bridgereg_t)int_enable;
	    }
	    bridge->b_wid_tflush;	/* wait until Bridge PIO complete */
	    pcibr_unlock(pcibr_soft, s);
	    return;
	}
}
