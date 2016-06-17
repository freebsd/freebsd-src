/* $Id: io.c,v 1.2 2001/06/26 14:02:43 pfg Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <linux/config.h>
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
#include <asm/sn/sn_cpuid.h>

extern xtalk_provider_t hub_provider;
extern void hub_intr_init(vertex_hdl_t hubv);

static int force_fire_and_forget = 1;
static int ignore_conveyor_override;


/* 
 * Implementation of hub iobus operations.
 *
 * Hub provides a crosstalk "iobus" on IP27 systems.  These routines
 * provide a platform-specific implementation of xtalk used by all xtalk 
 * cards on IP27 systems.
 *
 * Called from corresponding xtalk_* routines.
 */


/* PIO MANAGEMENT */
/* For mapping system virtual address space to xtalk space on a specified widget */

/*
 * Setup pio structures needed for a particular hub.
 */
static void
hub_pio_init(vertex_hdl_t hubv)
{
	xwidgetnum_t widget;
	hubinfo_t hubinfo;
	nasid_t nasid;
	int bigwin;
	hub_piomap_t hub_piomap;

	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;

	/* Initialize small window piomaps for this hub */
	for (widget=0; widget <= HUB_WIDGET_ID_MAX; widget++) {
		hub_piomap = hubinfo_swin_piomap_get(hubinfo, (int)widget);
		hub_piomap->hpio_xtalk_info.xp_target = widget;
		hub_piomap->hpio_xtalk_info.xp_xtalk_addr = 0;
		hub_piomap->hpio_xtalk_info.xp_mapsz = SWIN_SIZE;
		hub_piomap->hpio_xtalk_info.xp_kvaddr = (caddr_t)NODE_SWIN_BASE(nasid, widget);
		hub_piomap->hpio_hub = hubv;
		hub_piomap->hpio_flags = HUB_PIOMAP_IS_VALID;
	}

	/* Initialize big window piomaps for this hub */
	for (bigwin=0; bigwin < HUB_NUM_BIG_WINDOW; bigwin++) {
		hub_piomap = hubinfo_bwin_piomap_get(hubinfo, bigwin);
		hub_piomap->hpio_xtalk_info.xp_mapsz = BWIN_SIZE;
		hub_piomap->hpio_hub = hubv;
		hub_piomap->hpio_holdcnt = 0;
		hub_piomap->hpio_flags = HUB_PIOMAP_IS_BIGWINDOW;
		IIO_ITTE_DISABLE(nasid, bigwin);
	}
	hub_set_piomode(nasid, HUB_PIO_CONVEYOR);

	spin_lock_init(&hubinfo->h_bwlock);
/*
 * If this lock can be acquired from interrupts or bh's, add SV_INTS or SV_BHS,
 * respectively, to the flags here.
 */
	sv_init(&hubinfo->h_bwwait, &hubinfo->h_bwlock, SV_ORDER_FIFO | SV_MON_SPIN); 
}

/* 
 * Create a caddr_t-to-xtalk_addr mapping.
 *
 * Use a small window if possible (that's the usual case), but
 * manage big windows if needed.  Big window mappings can be
 * either FIXED or UNFIXED -- we keep at least 1 big window available
 * for UNFIXED mappings.
 *
 * Returns an opaque pointer-sized type which can be passed to
 * other hub_pio_* routines on success, or NULL if the request
 * cannot be satisfied.
 */
/* ARGSUSED */
hub_piomap_t
hub_piomap_alloc(vertex_hdl_t dev,	/* set up mapping for this device */
		device_desc_t dev_desc,	/* device descriptor */
		iopaddr_t xtalk_addr,	/* map for this xtalk_addr range */
		size_t byte_count,
		size_t byte_count_max, 	/* maximum size of a mapping */
		unsigned flags)		/* defined in sys/pio.h */
{
	xwidget_info_t widget_info = xwidget_info_get(dev);
	xwidgetnum_t widget = xwidget_info_id_get(widget_info);
	vertex_hdl_t hubv = xwidget_info_master_get(widget_info);
	hubinfo_t hubinfo;
	hub_piomap_t bw_piomap;
	int bigwin, free_bw_index;
	nasid_t nasid;
	volatile hubreg_t junk;
	caddr_t kvaddr;
#ifdef PIOMAP_UNC_ACC_SPACE
	uint64_t addr;
#endif

	/* sanity check */
	if (byte_count_max > byte_count)
		return(NULL);

	hubinfo_get(hubv, &hubinfo);

	/* If xtalk_addr range is mapped by a small window, we don't have 
	 * to do much 
	 */
	if (xtalk_addr + byte_count <= SWIN_SIZE) {
		hub_piomap_t piomap;

		piomap = hubinfo_swin_piomap_get(hubinfo, (int)widget);
#ifdef PIOMAP_UNC_ACC_SPACE
		if (flags & PIOMAP_UNC_ACC) {
			addr = (uint64_t)piomap->hpio_xtalk_info.xp_kvaddr;
			addr |= PIOMAP_UNC_ACC_SPACE;
			piomap->hpio_xtalk_info.xp_kvaddr = (caddr_t)addr;
		}
#endif
		return piomap;
	}

	/* We need to use a big window mapping.  */

	/*
	 * TBD: Allow requests that would consume multiple big windows --
	 * split the request up and use multiple mapping entries.
	 * For now, reject requests that span big windows.
	 */
	if ((xtalk_addr % BWIN_SIZE) + byte_count > BWIN_SIZE)
		return(NULL);


	/* Round xtalk address down for big window alignement */
	xtalk_addr = xtalk_addr & ~(BWIN_SIZE-1);

	/*
	 * Check to see if an existing big window mapping will suffice.
	 */
tryagain:
	free_bw_index = -1;
	spin_lock(&hubinfo->h_bwlock);
	for (bigwin=0; bigwin < HUB_NUM_BIG_WINDOW; bigwin++) {
		bw_piomap = hubinfo_bwin_piomap_get(hubinfo, bigwin);

		/* If mapping is not valid, skip it */
		if (!(bw_piomap->hpio_flags & HUB_PIOMAP_IS_VALID)) {
			free_bw_index = bigwin;
			continue;
		}

		/* 
		 * If mapping is UNFIXED, skip it.  We don't allow sharing
		 * of UNFIXED mappings, because this would allow starvation.
		 */
		if (!(bw_piomap->hpio_flags & HUB_PIOMAP_IS_FIXED))
			continue;

		if ( xtalk_addr == bw_piomap->hpio_xtalk_info.xp_xtalk_addr &&
		     widget == bw_piomap->hpio_xtalk_info.xp_target) {
			bw_piomap->hpio_holdcnt++;
			spin_unlock(&hubinfo->h_bwlock);
			return(bw_piomap);
		}
	}

	/*
	 * None of the existing big window mappings will work for us --
	 * we need to establish a new mapping.
	 */

	/* Insure that we don't consume all big windows with FIXED mappings */
	if (flags & PIOMAP_FIXED) {
		if (hubinfo->h_num_big_window_fixed < HUB_NUM_BIG_WINDOW-1) {
			ASSERT(free_bw_index >= 0);
			hubinfo->h_num_big_window_fixed++;
		} else {
			bw_piomap = NULL;
			goto done;
		}
	} else /* PIOMAP_UNFIXED */ {
		if (free_bw_index < 0) {
			if (flags & PIOMAP_NOSLEEP) {
				bw_piomap = NULL;
				goto done;
			}

			sv_wait(&hubinfo->h_bwwait, 0, 0);
			goto tryagain;
		}
	}


	/* OK!  Allocate big window free_bw_index for this mapping. */
 	/* 
	 * The code below does a PIO write to setup an ITTE entry.
	 * We need to prevent other CPUs from seeing our updated memory 
	 * shadow of the ITTE (in the piomap) until the ITTE entry is 
	 * actually set up; otherwise, another CPU might attempt a PIO 
	 * prematurely.  
	 *
	 * Also, the only way we can know that an entry has been received 
	 * by the hub and can be used by future PIO reads/writes is by 
	 * reading back the ITTE entry after writing it.
	 *
	 * For these two reasons, we PIO read back the ITTE entry after
	 * we write it.
	 */

	nasid = hubinfo->h_nasid;
	IIO_ITTE_PUT(nasid, free_bw_index, HUB_PIO_MAP_TO_MEM, widget, xtalk_addr);	
	junk = HUB_L(IIO_ITTE_GET(nasid, free_bw_index));

	bw_piomap = hubinfo_bwin_piomap_get(hubinfo, free_bw_index);
	bw_piomap->hpio_xtalk_info.xp_dev = dev;
	bw_piomap->hpio_xtalk_info.xp_target = widget;
	bw_piomap->hpio_xtalk_info.xp_xtalk_addr = xtalk_addr;
	kvaddr = (caddr_t)NODE_BWIN_BASE(nasid, free_bw_index);
#ifdef PIOMAP_UNC_ACC_SPACE
	if (flags & PIOMAP_UNC_ACC) {
		addr = (uint64_t)kvaddr;
		addr |= PIOMAP_UNC_ACC_SPACE;
		kvaddr = (caddr_t)addr;
	}
#endif
	bw_piomap->hpio_xtalk_info.xp_kvaddr = kvaddr;
	bw_piomap->hpio_holdcnt++;
	bw_piomap->hpio_bigwin_num = free_bw_index;

	if (flags & PIOMAP_FIXED)
		bw_piomap->hpio_flags |= HUB_PIOMAP_IS_VALID | HUB_PIOMAP_IS_FIXED;
	else
		bw_piomap->hpio_flags |= HUB_PIOMAP_IS_VALID;

done:
	spin_unlock(&hubinfo->h_bwlock);
	return(bw_piomap);
}

/*
 * hub_piomap_free destroys a caddr_t-to-xtalk pio mapping and frees
 * any associated mapping resources.  
 *
 * If this * piomap was handled with a small window, or if it was handled
 * in a big window that's still in use by someone else, then there's 
 * nothing to do.  On the other hand, if this mapping was handled 
 * with a big window, AND if we were the final user of that mapping, 
 * then destroy the mapping.
 */
void
hub_piomap_free(hub_piomap_t hub_piomap)
{
	vertex_hdl_t hubv;
	hubinfo_t hubinfo;
	nasid_t nasid;

	/* 
	 * Small windows are permanently mapped to corresponding widgets,
	 * so there're no resources to free.
	 */
	if (!(hub_piomap->hpio_flags & HUB_PIOMAP_IS_BIGWINDOW))
		return;

	ASSERT(hub_piomap->hpio_flags & HUB_PIOMAP_IS_VALID);
	ASSERT(hub_piomap->hpio_holdcnt > 0);

	hubv = hub_piomap->hpio_hub;
	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;

	spin_lock(&hubinfo->h_bwlock);

	/*
	 * If this is the last hold on this mapping, free it.
	 */
	if (--hub_piomap->hpio_holdcnt == 0) {
		IIO_ITTE_DISABLE(nasid, hub_piomap->hpio_bigwin_num );

		if (hub_piomap->hpio_flags & HUB_PIOMAP_IS_FIXED) {
			hub_piomap->hpio_flags &= ~(HUB_PIOMAP_IS_VALID | HUB_PIOMAP_IS_FIXED);
			hubinfo->h_num_big_window_fixed--;
			ASSERT(hubinfo->h_num_big_window_fixed >= 0);
		} else
			hub_piomap->hpio_flags &= ~HUB_PIOMAP_IS_VALID;

		(void)sv_signal(&hubinfo->h_bwwait);
	}

	spin_unlock(&hubinfo->h_bwlock);
}

/*
 * Establish a mapping to a given xtalk address range using the resources
 * allocated earlier.
 */
caddr_t
hub_piomap_addr(hub_piomap_t hub_piomap,	/* mapping resources */
		iopaddr_t xtalk_addr,		/* map for this xtalk address */
		size_t byte_count)		/* map this many bytes */
{
	/* Verify that range can be mapped using the specified piomap */
	if (xtalk_addr < hub_piomap->hpio_xtalk_info.xp_xtalk_addr)
		return(0);

	if (xtalk_addr + byte_count > 
		( hub_piomap->hpio_xtalk_info.xp_xtalk_addr + 
			hub_piomap->hpio_xtalk_info.xp_mapsz))
		return(0);

	if (hub_piomap->hpio_flags & HUB_PIOMAP_IS_VALID)
		return(hub_piomap->hpio_xtalk_info.xp_kvaddr + 
			(xtalk_addr % hub_piomap->hpio_xtalk_info.xp_mapsz));
	else
		return(0);
}


/*
 * Driver indicates that it's done with PIO's from an earlier piomap_addr.
 */
/* ARGSUSED */
void
hub_piomap_done(hub_piomap_t hub_piomap)	/* done with these mapping resources */
{
	/* Nothing to do */
}


/*
 * For translations that require no mapping resources, supply a kernel virtual
 * address that maps to the specified xtalk address range.
 */
/* ARGSUSED */
caddr_t
hub_piotrans_addr(	vertex_hdl_t dev,	/* translate to this device */
			device_desc_t dev_desc,	/* device descriptor */
			iopaddr_t xtalk_addr,	/* Crosstalk address */
			size_t byte_count,	/* map this many bytes */
			unsigned flags)		/* (currently unused) */
{
	xwidget_info_t widget_info = xwidget_info_get(dev);
	xwidgetnum_t widget = xwidget_info_id_get(widget_info);
	vertex_hdl_t hubv = xwidget_info_master_get(widget_info);
	hub_piomap_t hub_piomap;
	hubinfo_t hubinfo;
	caddr_t addr;

	hubinfo_get(hubv, &hubinfo);

	if (xtalk_addr + byte_count <= SWIN_SIZE) {
		hub_piomap = hubinfo_swin_piomap_get(hubinfo, (int)widget);
		addr = hub_piomap_addr(hub_piomap, xtalk_addr, byte_count);
#ifdef PIOMAP_UNC_ACC_SPACE
		if (flags & PIOMAP_UNC_ACC) {
			uint64_t iaddr;
			iaddr = (uint64_t)addr;
			iaddr |= PIOMAP_UNC_ACC_SPACE;
			addr = (caddr_t)iaddr;
		}
#endif
		return(addr);
	} else
		return(0);
}


/* DMA MANAGEMENT */
/* Mapping from crosstalk space to system physical space */


/*
 * Allocate resources needed to set up DMA mappings up to a specified size
 * on a specified adapter.
 * 
 * We don't actually use the adapter ID for anything.  It's just the adapter
 * that the lower level driver plans to use for DMA.
 */
/* ARGSUSED */
hub_dmamap_t
hub_dmamap_alloc(	vertex_hdl_t dev,	/* set up mappings for this device */
			device_desc_t dev_desc,	/* device descriptor */
			size_t byte_count_max, 	/* max size of a mapping */
			unsigned flags)		/* defined in dma.h */
{
	hub_dmamap_t dmamap;
	xwidget_info_t widget_info = xwidget_info_get(dev);
	xwidgetnum_t widget = xwidget_info_id_get(widget_info);
	vertex_hdl_t hubv = xwidget_info_master_get(widget_info);

	dmamap = kmalloc(sizeof(struct hub_dmamap_s), GFP_ATOMIC);
	dmamap->hdma_xtalk_info.xd_dev = dev;
	dmamap->hdma_xtalk_info.xd_target = widget;
	dmamap->hdma_hub = hubv;
	dmamap->hdma_flags = HUB_DMAMAP_IS_VALID;
 	if (flags & XTALK_FIXED)
		dmamap->hdma_flags |= HUB_DMAMAP_IS_FIXED;

	return(dmamap);
}

/*
 * Destroy a DMA mapping from crosstalk space to system address space.
 * There is no actual mapping hardware to destroy, but we at least mark
 * the dmamap INVALID and free the space that it took.
 */
void
hub_dmamap_free(hub_dmamap_t hub_dmamap)
{
	hub_dmamap->hdma_flags &= ~HUB_DMAMAP_IS_VALID;
	kfree(hub_dmamap);
}

/*
 * Establish a DMA mapping using the resources allocated in a previous dmamap_alloc.
 * Return an appropriate crosstalk address range that maps to the specified physical 
 * address range.
 */
/* ARGSUSED */
extern iopaddr_t
hub_dmamap_addr(	hub_dmamap_t dmamap,	/* use these mapping resources */
			paddr_t paddr,		/* map for this address */
			size_t byte_count)	/* map this many bytes */
{
	vertex_hdl_t vhdl;

	ASSERT(dmamap->hdma_flags & HUB_DMAMAP_IS_VALID);

	if (dmamap->hdma_flags & HUB_DMAMAP_USED) {
	    /* If the map is FIXED, re-use is OK. */
	    if (!(dmamap->hdma_flags & HUB_DMAMAP_IS_FIXED)) {
		char name[MAXDEVNAME];
		vhdl = dmamap->hdma_xtalk_info.xd_dev;
		printk(KERN_WARNING  "%s: hub_dmamap_addr re-uses dmamap.\n", vertex_to_name(vhdl, name, MAXDEVNAME));
	    }
	} else {
		dmamap->hdma_flags |= HUB_DMAMAP_USED;
	}

	/* There isn't actually any DMA mapping hardware on the hub. */
        return( (PHYS_TO_DMA(paddr)) );
}

/*
 * Establish a DMA mapping using the resources allocated in a previous dmamap_alloc.
 * Return an appropriate crosstalk address list that maps to the specified physical 
 * address list.
 */
/* ARGSUSED */
alenlist_t
hub_dmamap_list(hub_dmamap_t hub_dmamap,	/* use these mapping resources */
		alenlist_t palenlist,		/* map this area of memory */
		unsigned flags)
{
	vertex_hdl_t vhdl;

	ASSERT(hub_dmamap->hdma_flags & HUB_DMAMAP_IS_VALID);

	if (hub_dmamap->hdma_flags & HUB_DMAMAP_USED) {
	    /* If the map is FIXED, re-use is OK. */
	    if (!(hub_dmamap->hdma_flags & HUB_DMAMAP_IS_FIXED)) {
		char name[MAXDEVNAME];
		vhdl = hub_dmamap->hdma_xtalk_info.xd_dev;
		printk(KERN_WARNING  "%s: hub_dmamap_list re-uses dmamap\n", vertex_to_name(vhdl, name, MAXDEVNAME));
	    }
	} else {
		hub_dmamap->hdma_flags |= HUB_DMAMAP_USED;
	}

	/* There isn't actually any DMA mapping hardware on the hub.  */
	return(palenlist);
}

/*
 * Driver indicates that it has completed whatever DMA it may have started
 * after an earlier dmamap_addr or dmamap_list call.
 */
void
hub_dmamap_done(hub_dmamap_t hub_dmamap)	/* done with these mapping resources */
{
	vertex_hdl_t vhdl;

	if (hub_dmamap->hdma_flags & HUB_DMAMAP_USED) {
		hub_dmamap->hdma_flags &= ~HUB_DMAMAP_USED;
	} else {
	    /* If the map is FIXED, re-done is OK. */
	    if (!(hub_dmamap->hdma_flags & HUB_DMAMAP_IS_FIXED)) {
		char name[MAXDEVNAME];
		vhdl = hub_dmamap->hdma_xtalk_info.xd_dev;
		printk(KERN_WARNING  "%s: hub_dmamap_done already done with dmamap\n", vertex_to_name(vhdl, name, MAXDEVNAME));
	    }
	}
}

/*
 * Translate a single system physical address into a crosstalk address.
 */
/* ARGSUSED */
iopaddr_t
hub_dmatrans_addr(	vertex_hdl_t dev,	/* translate for this device */
			device_desc_t dev_desc,	/* device descriptor */
			paddr_t paddr,		/* system physical address */
			size_t byte_count,	/* length */
			unsigned flags)		/* defined in dma.h */
{
	return( (PHYS_TO_DMA(paddr)) );
}

/*
 * Translate a list of IP27 addresses and lengths into a list of crosstalk 
 * addresses and lengths.  No actual hardware mapping takes place; the hub 
 * has no DMA mapping registers -- crosstalk addresses map directly.
 */
/* ARGSUSED */
alenlist_t
hub_dmatrans_list(	vertex_hdl_t dev,	/* translate for this device */
			device_desc_t dev_desc,	/* device descriptor */
			alenlist_t palenlist,	/* system address/length list */
			unsigned flags)		/* defined in dma.h */
{
	BUG();
	/* no translation needed */
	return(palenlist);
}

/*ARGSUSED*/
void
hub_dmamap_drain(	hub_dmamap_t map)
{
    /* XXX- flush caches, if cache coherency WAR is needed */
}

/*ARGSUSED*/
void
hub_dmaaddr_drain(	vertex_hdl_t vhdl,
			paddr_t addr,
			size_t bytes)
{
    /* XXX- flush caches, if cache coherency WAR is needed */
}

/*ARGSUSED*/
void
hub_dmalist_drain(	vertex_hdl_t vhdl,
			alenlist_t list)
{
    /* XXX- flush caches, if cache coherency WAR is needed */
}


int
hub_dma_enabled(vertex_hdl_t xconn_vhdl)
{
	return(0);
}

int
hub_error_devenable(vertex_hdl_t xconn_vhdl, int devnum, int error_code)
{
	return(0);
}


/* CONFIGURATION MANAGEMENT */

/*
 * Perform initializations that allow this hub to start crosstalk support.
 */
void
hub_provider_startup(vertex_hdl_t hubv)
{
	hub_pio_init(hubv);
	hub_intr_init(hubv);
}

/*
 * Shutdown crosstalk support from a hub.
 */
void
hub_provider_shutdown(vertex_hdl_t hub)
{
	/* TBD */
	xtalk_provider_unregister(hub);
}

/*
 * Check that an address is in the real small window widget 0 space
 * or else in the big window we're using to emulate small window 0
 * in the kernel.
 */
int
hub_check_is_widget0(void *addr)
{
	nasid_t nasid = NASID_GET(addr);

	if (((__psunsigned_t)addr >= RAW_NODE_SWIN_BASE(nasid, 0)) &&
	    ((__psunsigned_t)addr < RAW_NODE_SWIN_BASE(nasid, 1)))
		return 1;
	return 0;
}


/*
 * Check that two addresses use the same widget
 */
int
hub_check_window_equiv(void *addra, void *addrb)
{
	if (hub_check_is_widget0(addra) && hub_check_is_widget0(addrb))
		return 1;

	/* XXX - Assume this is really a small window address */
	if (WIDGETID_GET((__psunsigned_t)addra) ==
	    WIDGETID_GET((__psunsigned_t)addrb))
		return 1;

	return 0;
}


/*
 * hub_setup_prb(nasid, prbnum, credits, conveyor)
 *
 * 	Put a PRB into fire-and-forget mode if conveyor isn't set.  Otherwise,
 * 	put it into conveyor belt mode with the specified number of credits.
 */
void
hub_setup_prb(nasid_t nasid, int prbnum, int credits, int conveyor)
{
	iprb_t prb;
	int prb_offset;

	if (force_fire_and_forget && !ignore_conveyor_override)
	    if (conveyor == HUB_PIO_CONVEYOR)
		conveyor = HUB_PIO_FIRE_N_FORGET;

	/*
	 * Get the current register value.
	 */
	prb_offset = IIO_IOPRB(prbnum);
	prb.iprb_regval = REMOTE_HUB_L(nasid, prb_offset);

	/*
	 * Clear out some fields.
	 */
	prb.iprb_ovflow = 1;
	prb.iprb_bnakctr = 0;
	prb.iprb_anakctr = 0;

	/*
	 * Enable or disable fire-and-forget mode.
	 */
	prb.iprb_ff = ((conveyor == HUB_PIO_CONVEYOR) ? 0 : 1);

	/*
	 * Set the appropriate number of PIO cresits for the widget.
	 */
	prb.iprb_xtalkctr = credits;

	/*
	 * Store the new value to the register.
	 */
	REMOTE_HUB_S(nasid, prb_offset, prb.iprb_regval);
}

/*
 * hub_set_piomode()
 *
 * 	Put the hub into either "PIO conveyor belt" mode or "fire-and-forget"
 * 	mode.  To do this, we have to make absolutely sure that no PIOs
 *	are in progress so we turn off access to all widgets for the duration
 *	of the function.
 * 
 * XXX - This code should really check what kind of widget we're talking
 * to.  Bridges can only handle three requests, but XG will do more.
 * How many can crossbow handle to widget 0?  We're assuming 1.
 *
 * XXX - There is a bug in the crossbow that link reset PIOs do not
 * return write responses.  The easiest solution to this problem is to
 * leave widget 0 (xbow) in fire-and-forget mode at all times.  This
 * only affects pio's to xbow registers, which should be rare.
 */
void
hub_set_piomode(nasid_t nasid, int conveyor)
{
	hubreg_t ii_iowa;
	int direct_connect;
	hubii_wcr_t ii_wcr;
	int prbnum;

	ASSERT(NASID_TO_COMPACT_NODEID(nasid) != INVALID_CNODEID);

	ii_iowa = REMOTE_HUB_L(nasid, IIO_OUTWIDGET_ACCESS);
	REMOTE_HUB_S(nasid, IIO_OUTWIDGET_ACCESS, 0);

	ii_wcr.wcr_reg_value = REMOTE_HUB_L(nasid, IIO_WCR);
	direct_connect = ii_wcr.iwcr_dir_con;

	if (direct_connect) {
		/* 
		 * Assume a bridge here.
		 */
		hub_setup_prb(nasid, 0, 3, conveyor);
	} else {
		/* 
		 * Assume a crossbow here.
		 */
		hub_setup_prb(nasid, 0, 1, conveyor);
	}

	for (prbnum = HUB_WIDGET_ID_MIN; prbnum <= HUB_WIDGET_ID_MAX; prbnum++) {
		/*
		 * XXX - Here's where we should take the widget type into
		 * when account assigning credits.
		 */
		/* Always set the PRBs in fire-and-forget mode */
		hub_setup_prb(nasid, prbnum, 3, conveyor);
	}

	REMOTE_HUB_S(nasid, IIO_OUTWIDGET_ACCESS, ii_iowa);
}
/* Interface to allow special drivers to set hub specific
 * device flags.
 * Return 0 on failure , 1 on success
 */
int
hub_widget_flags_set(nasid_t		nasid,
		     xwidgetnum_t	widget_num,
		     hub_widget_flags_t	flags)
{

	ASSERT((flags & HUB_WIDGET_FLAGS) == flags);

	if (flags & HUB_PIO_CONVEYOR) {
		hub_setup_prb(nasid,widget_num,
			      3,HUB_PIO_CONVEYOR); /* set the PRB in conveyor 
						    * belt mode with 3 credits
						    */
	} else if (flags & HUB_PIO_FIRE_N_FORGET) {
		hub_setup_prb(nasid,widget_num,
			      3,HUB_PIO_FIRE_N_FORGET); /* set the PRB in fire
							 *  and forget mode 
							 */
	}

	return 1;
}

/*
 * A pointer to this structure hangs off of every hub hwgraph vertex.
 * The generic xtalk layer may indirect through it to get to this specific
 * crosstalk bus provider.
 */
xtalk_provider_t hub_provider = {
	(xtalk_piomap_alloc_f *)	hub_piomap_alloc,
	(xtalk_piomap_free_f *)		hub_piomap_free,
	(xtalk_piomap_addr_f *)		hub_piomap_addr,
	(xtalk_piomap_done_f *)		hub_piomap_done,
	(xtalk_piotrans_addr_f *)	hub_piotrans_addr,

	(xtalk_dmamap_alloc_f *)	hub_dmamap_alloc,
	(xtalk_dmamap_free_f *)		hub_dmamap_free,
	(xtalk_dmamap_addr_f *)		hub_dmamap_addr,
	(xtalk_dmamap_list_f *)		hub_dmamap_list,
	(xtalk_dmamap_done_f *)		hub_dmamap_done,
	(xtalk_dmatrans_addr_f *)	hub_dmatrans_addr,
	(xtalk_dmatrans_list_f *)	hub_dmatrans_list,
	(xtalk_dmamap_drain_f *)	hub_dmamap_drain,
	(xtalk_dmaaddr_drain_f *)	hub_dmaaddr_drain,
	(xtalk_dmalist_drain_f *)	hub_dmalist_drain,

	(xtalk_intr_alloc_f *)		hub_intr_alloc,
	(xtalk_intr_alloc_f *)		hub_intr_alloc_nothd,
	(xtalk_intr_free_f *)		hub_intr_free,
	(xtalk_intr_connect_f *)	hub_intr_connect,
	(xtalk_intr_disconnect_f *)	hub_intr_disconnect,
	(xtalk_provider_startup_f *)	hub_provider_startup,
	(xtalk_provider_shutdown_f *)	hub_provider_shutdown,
};

/*
 * per_ice_init
 *
 *      This code is executed once for each Ice chip.
 */
void
per_ice_init(cnodeid_t cnode)
{

        /* Initialize error interrupts for this ice. */
	printk("per_ice_init: We need to init ice here ....!\n");
        /* ice_error_init(cnode); */

}
/*
 * per_hub_init
 *
 *	This code is executed once for each Hub chip.
 */
void
per_hub_init(cnodeid_t cnode)
{
	nasid_t		nasid;
	nodepda_t	*npdap;
	ii_icmr_u_t	ii_icmr;
	ii_ibcr_u_t	ii_ibcr;
	ii_ilcsr_u_t	ii_ilcsr;

	nasid = COMPACT_TO_NASID_NODEID(cnode);

	ASSERT(nasid != INVALID_NASID);
	ASSERT(NASID_TO_COMPACT_NODEID(nasid) == cnode);

	npdap = NODEPDA(cnode);

	/* Disable the request and reply errors. */
	REMOTE_HUB_S(nasid, IIO_IWEIM, 0xC000);

	/*
	 * Set the total number of CRBs that can be used.
	 */
	ii_icmr.ii_icmr_regval= 0x0;
	ii_icmr.ii_icmr_fld_s.i_c_cnt = 0xf;
	if (enable_shub_wars_1_1() ) {
		// Set bit one of ICMR to prevent II from sending interrupt for II bug.
		ii_icmr.ii_icmr_regval |= 0x1;
	}
	REMOTE_HUB_S(nasid, IIO_ICMR, ii_icmr.ii_icmr_regval);

	/*
	 * Set the number of CRBs that both of the BTEs combined
	 * can use minus 1.
	 */
	ii_ibcr.ii_ibcr_regval= 0x0;
	ii_ilcsr.ii_ilcsr_regval = REMOTE_HUB_L(nasid, IIO_LLP_CSR);
	if (ii_ilcsr.ii_ilcsr_fld_s.i_llp_stat & LNK_STAT_WORKING) {
	    ii_ibcr.ii_ibcr_fld_s.i_count = 0x8;
	} else {
	    /*
	     * if the LLP is down, there is no attached I/O, so
	    * give BTE all the CRBs.
	    */
	    ii_ibcr.ii_ibcr_fld_s.i_count = 0x14;
	}
	REMOTE_HUB_S(nasid, IIO_IBCR, ii_ibcr.ii_ibcr_regval);

	/*
	 * Set CRB timeout to be 10ms.
	 */
	REMOTE_HUB_S(nasid, IIO_ICTP, 0xffffff );
	REMOTE_HUB_S(nasid, IIO_ICTO, 0xff);

	/* Initialize error interrupts for this hub. */
	hub_error_init(cnode);
}


