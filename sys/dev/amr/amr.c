/*-
 * Copyright (c) 1999 Michael Smith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*
 * Driver for the AMI MegaRaid family of controllers
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/resource.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/amr/amrio.h>
#include <dev/amr/amrreg.h>
#include <dev/amr/amrvar.h>

#if 0
#define debug(fmt, args...)	printf("%s: " fmt "\n", __FUNCTION__ , ##args)
#else
#define debug(fmt, args...)
#endif

#define AMR_CDEV_MAJOR	132

static struct cdevsw amr_cdevsw = {
		/* open */	amr_open,
		/* close */	amr_close,
		/* read */	noread,
		/* write */	nowrite,
		/* ioctl */	amr_ioctl,
		/* poll */	nopoll,
		/* mmap */	nommap,
		/* strategy */	nostrategy,
		/* name */ 	"amr",
		/* maj */	AMR_CDEV_MAJOR,
		/* dump */	nodump,
		/* psize */ 	nopsize,
		/* flags */	0,
		/* bmaj */	254	/* XXX magic no-bdev */
};

static int	cdev_registered = 0;
devclass_t	amr_devclass;

/*
 * Command wrappers
 */
static int			amr_query_controller(struct amr_softc *sc);
static void			*amr_enquiry(struct amr_softc *sc, size_t bufsize, 
					     u_int8_t cmd, u_int8_t cmdsub, u_int8_t cmdqual);
static int			amr_flush(struct amr_softc *sc);
static void			amr_startio(struct amr_softc *sc);
static void			amr_completeio(struct amr_command *ac);

/*
 * Command processing.
 */
static int			amr_wait_command(struct amr_command *ac);
static int			amr_poll_command(struct amr_command *ac);
static int			amr_getslot(struct amr_command *ac);
static void			amr_mapcmd(struct amr_command *ac);
static void			amr_unmapcmd(struct amr_command *ac);
static int			amr_start(struct amr_command *ac);
static int			amr_done(struct amr_softc *sc);
static void			amr_complete(struct amr_softc *sc);

/*
 * Command buffer allocation.
 */
static struct amr_command	*amr_alloccmd(struct amr_softc *sc);
static void			amr_releasecmd(struct amr_command *ac);
static void			amr_freecmd(struct amr_command *ac);

/*
 * Status monitoring
 */
static void			amr_periodic(void *data);

/*
 * Interface-specific shims
 */
static void			amr_quartz_submit_command(struct amr_softc *sc);
static int			amr_quartz_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave);
static void			amr_quartz_attach_mailbox(struct amr_softc *sc);

static void			amr_std_submit_command(struct amr_softc *sc);
static int			amr_std_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave);
static void			amr_std_attach_mailbox(struct amr_softc *sc);

/*
 * Debugging
 */
static void			amr_printcommand(struct amr_command *ac);

/********************************************************************************
 ********************************************************************************
                                                                Public Interfaces
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Free all of the resources associated with (sc)
 *
 * Should not be called if the controller is active.
 */
void
amr_free(struct amr_softc *sc)
{
    struct amr_command	*ac;
    u_int8_t		*p;
    
    debug("called");

    /* cancel status timeout */
    untimeout(amr_periodic, sc, sc->amr_timeout);
    
    /* throw away any command buffers */
    while ((ac = TAILQ_FIRST(&sc->amr_freecmds)) != NULL) {
	TAILQ_REMOVE(&sc->amr_freecmds, ac, ac_link);
	amr_freecmd(ac);
    }

    /* destroy data-transfer DMA tag */
    if (sc->amr_buffer_dmat)
	bus_dma_tag_destroy(sc->amr_buffer_dmat);

    /* free and destroy DMA memory and tag for s/g lists */
    if (sc->amr_sgtable)
	bus_dmamem_free(sc->amr_sg_dmat, sc->amr_sgtable, sc->amr_sg_dmamap);
    if (sc->amr_sg_dmat)
	bus_dma_tag_destroy(sc->amr_sg_dmat);

    /* free and destroy DMA memory and tag for mailbox */
    if (sc->amr_mailbox) {
	p = (u_int8_t *)sc->amr_mailbox;
	bus_dmamem_free(sc->amr_sg_dmat, p - 16, sc->amr_sg_dmamap);
    }
    if (sc->amr_sg_dmat)
	bus_dma_tag_destroy(sc->amr_sg_dmat);

    /* disconnect the interrupt handler */
    if (sc->amr_intr)
	bus_teardown_intr(sc->amr_dev, sc->amr_irq, sc->amr_intr);
    if (sc->amr_irq != NULL)
	bus_release_resource(sc->amr_dev, SYS_RES_IRQ, 0, sc->amr_irq);

    /* destroy the parent DMA tag */
    if (sc->amr_parent_dmat)
	bus_dma_tag_destroy(sc->amr_parent_dmat);

    /* release the register window mapping */
    if (sc->amr_reg != NULL)
	bus_release_resource(sc->amr_dev,
			     (sc->amr_type == AMR_TYPE_QUARTZ) ? SYS_RES_MEMORY : SYS_RES_IOPORT,
			     AMR_CFG_BASE, sc->amr_reg);
}

/********************************************************************************
 * Allocate and map the scatter/gather table in bus space.
 */
static void
amr_dma_map_sg(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct amr_softc	*sc = (struct amr_softc *)arg;

    debug("called");

    /* save base of s/g table's address in bus space */
    sc->amr_sgbusaddr = segs->ds_addr;
}

static int
amr_sglist_map(struct amr_softc *sc)
{
    size_t	segsize;
    int		error;

    debug("called");

    /* destroy any existing mappings */
    if (sc->amr_sgtable)
	bus_dmamem_free(sc->amr_sg_dmat, sc->amr_sgtable, sc->amr_sg_dmamap);
    if (sc->amr_sg_dmat)
	bus_dma_tag_destroy(sc->amr_sg_dmat);

    /*
     * Create a single tag describing a region large enough to hold all of
     * the s/g lists we will need.
     */
    segsize = sizeof(struct amr_sgentry) * AMR_NSEG * sc->amr_maxio;
    error = bus_dma_tag_create(sc->amr_parent_dmat, 	/* parent */
			       1, 0, 			/* alignment, boundary */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR, 	/* highaddr */
			       NULL, NULL, 		/* filter, filterarg */
			       segsize, 1,		/* maxsize, nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			       0,			/* flags */
			       &sc->amr_sg_dmat);
    if (error != 0) {
	device_printf(sc->amr_dev, "can't allocate scatter/gather DMA tag\n");
	return(ENOMEM);
    }

    /*
     * Allocate enough s/g maps for all commands and permanently map them into
     * controller-visible space.
     *	
     * XXX this assumes we can get enough space for all the s/g maps in one 
     * contiguous slab.  We may need to switch to a more complex arrangement where
     * we allocate in smaller chunks and keep a lookup table from slot to bus address.
     *
     * XXX HACK ALERT: at least some controllers don't like the s/g memory being
     *                 allocated below 0x2000.  We leak some memory if we get some
     *                 below this mark and allocate again.
     */
retry:
    error = bus_dmamem_alloc(sc->amr_sg_dmat, (void **)&sc->amr_sgtable, BUS_DMA_NOWAIT, &sc->amr_sg_dmamap);
    if (error) {
	device_printf(sc->amr_dev, "can't allocate s/g table\n");
	return(ENOMEM);
    }
    bus_dmamap_load(sc->amr_sg_dmat, sc->amr_sg_dmamap, sc->amr_sgtable, segsize, amr_dma_map_sg, sc, 0);
    if (sc->amr_sgbusaddr < 0x2000) {
	device_printf(sc->amr_dev, "s/g table too low (0x%x), reallocating\n", sc->amr_sgbusaddr);
	goto retry;
    }
    return(0);
}

/********************************************************************************
 * Allocate and set up mailbox areas for the controller (sc)
 *
 * The basic mailbox structure should be 16-byte aligned.  This means that the
 * mailbox64 structure has 4 bytes hanging off the bottom.
 */
static void
amr_map_mailbox(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct amr_softc	*sc = (struct amr_softc *)arg;
    
    debug("called");

    /* save phsyical base of the basic mailbox structure */
    sc->amr_mailboxphys = segs->ds_addr + 16;
}

static int
amr_setup_mbox(struct amr_softc *sc)
{
    int		error;
    u_int8_t	*p;
    
    debug("called");

    /*
     * Create a single tag describing a region large enough to hold the entire
     * mailbox.
     */
    error = bus_dma_tag_create(sc->amr_parent_dmat,	/* parent */
			       16, 0,			/* alignment, boundary */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       sizeof(struct amr_mailbox) + 16, 1, /* maxsize, nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			       0,			/* flags */
			       &sc->amr_mailbox_dmat);
    if (error != 0) {
	device_printf(sc->amr_dev, "can't allocate mailbox tag\n");
	return(ENOMEM);
    }

    /*
     * Allocate the mailbox structure and permanently map it into
     * controller-visible space.
     */
    error = bus_dmamem_alloc(sc->amr_mailbox_dmat, (void **)&p, BUS_DMA_NOWAIT, 
			     &sc->amr_mailbox_dmamap);
    if (error) {
	device_printf(sc->amr_dev, "can't allocate mailbox memory\n");
	return(ENOMEM);
    }
    bus_dmamap_load(sc->amr_mailbox_dmat, sc->amr_mailbox_dmamap, p, 
		    sizeof(struct amr_mailbox64), amr_map_mailbox, sc, 0);
    /*
     * Conventional mailbox is inside the mailbox64 region.
     */
    bzero(p, sizeof(struct amr_mailbox64));
    sc->amr_mailbox64 = (struct amr_mailbox64 *)(p + 12);
    sc->amr_mailbox = (struct amr_mailbox *)(p + 16);

    if (sc->amr_type == AMR_TYPE_STD) {
	/* XXX we have to tell the controller where we put it */
    }
    return(0);
}


/********************************************************************************
 * Initialise the controller and softc.
 */
int
amr_attach(struct amr_softc *sc)
{
    int			rid, error;

    /*
     * Initialise per-controller queues.
     */
    TAILQ_INIT(&sc->amr_work);
    TAILQ_INIT(&sc->amr_freecmds);
    bufq_init(&sc->amr_bufq);

    /*
     * Configure for this controller type.
     */
    if (sc->amr_type == AMR_TYPE_QUARTZ) {
	sc->amr_submit_command = amr_quartz_submit_command;
	sc->amr_get_work       = amr_quartz_get_work;
	sc->amr_attach_mailbox = amr_quartz_attach_mailbox;
    } else {
	sc->amr_submit_command = amr_std_submit_command;
	sc->amr_get_work       = amr_std_get_work;
	sc->amr_attach_mailbox = amr_std_attach_mailbox;
    }

    /*
     * Allocate and connect our interrupt.
     */
    rid = 0;
    sc->amr_irq = bus_alloc_resource(sc->amr_dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
    if (sc->amr_irq == NULL) {
        device_printf(sc->amr_dev, "couldn't allocate interrupt\n");
        amr_free(sc);
        return(ENXIO);
    }
    error = bus_setup_intr(sc->amr_dev, sc->amr_irq, INTR_TYPE_BIO,  amr_intr, sc, &sc->amr_intr);
    if (error) {
        device_printf(sc->amr_dev, "couldn't set up interrupt\n");
        amr_free(sc);
        return(ENXIO);
    }

    /*
     * Create DMA tag for mapping buffers into controller-addressable space.
     */
    error = bus_dma_tag_create(sc->amr_parent_dmat,     /* parent */
                               1, 0,                    /* alignment, boundary */
                               BUS_SPACE_MAXADDR,       /* lowaddr */
                               BUS_SPACE_MAXADDR,       /* highaddr */
                               NULL, NULL,              /* filter, filterarg */
                               MAXBSIZE, AMR_NSEG,      /* maxsize, nsegments */
                               BUS_SPACE_MAXSIZE_32BIT, /* maxsegsize */
                               0,                       /* flags */
                               &sc->amr_buffer_dmat);
    if (error != 0) {
        device_printf(sc->amr_dev, "can't allocate buffer DMA tag\n");
        return(ENOMEM);
    }

    /*
     * Allocate and set up mailbox in a bus-visible fashion, attach to controller.
     */
    if ((error = amr_setup_mbox(sc)) != 0)
	return(error);
    sc->amr_attach_mailbox(sc);

    /*
     * Build a temporary set of scatter/gather buffers.
     */
    sc->amr_maxio = 2;
    if (amr_sglist_map(sc))
	return(ENXIO);

    /*
     * Quiz controller for features and limits.
     */
    if (amr_query_controller(sc))
	return(ENXIO);

    /*
     * Rebuild the scatter/gather buffers now we know how many we need.
     */
    if (amr_sglist_map(sc))
	return(ENXIO);

    /*
     * Start the timeout routine.
     */
    sc->amr_timeout = timeout(amr_periodic, sc, hz);

    return(0);
}

/********************************************************************************
 * Locate disk resources and attach children to them.
 */
void
amr_startup(struct amr_softc *sc)
{
    struct amr_logdrive	*dr;
    int			i, error;
    
    debug("called");

    /* get up-to-date drive information */
    if (amr_query_controller(sc)) {
	device_printf(sc->amr_dev, "couldn't scan controller for drives\n");
	return;
    }

    /* iterate over available drives */
    for (i = 0, dr = &sc->amr_drive[0]; (i < AMR_MAXLD) && (dr->al_size != 0xffffffff); i++, dr++) {
	/* are we already attached to this drive? */
	if (dr->al_disk == 0) {
	    /* generate geometry information */
	    if (dr->al_size > 0x200000) {	/* extended translation? */
		dr->al_heads = 255;
		dr->al_sectors = 63;
	    } else {
		dr->al_heads = 64;
		dr->al_sectors = 32;
	    }
	    dr->al_cylinders = dr->al_size / (dr->al_heads * dr->al_sectors);
	    
	    dr->al_disk = device_add_child(sc->amr_dev, NULL, -1);
	    if (dr->al_disk == 0)
		device_printf(sc->amr_dev, "device_add_child failed\n");
	    device_set_ivars(dr->al_disk, dr);
	}
    }
    
    if ((error = bus_generic_attach(sc->amr_dev)) != 0)
	device_printf(sc->amr_dev, "bus_generic_attach returned %d\n", error);
    
    /* mark controller back up */
    sc->amr_state &= ~AMR_STATE_SHUTDOWN;

    /* interrupts will be enabled before we do anything more */
    sc->amr_state |= AMR_STATE_INTEN;
}

/********************************************************************************
 * Disconnect from the controller completely, in preparation for unload.
 */
int
amr_detach(device_t dev)
{
    struct amr_softc	*sc = device_get_softc(dev);
    struct amrd_softc	*ad;
    int			i, s, error;

    debug("called");

    error = EBUSY;
    s = splbio();
    if (sc->amr_state & AMR_STATE_OPEN)
	goto out;

    for (i = 0; i < AMR_MAXLD; i++) {
	if (sc->amr_drive[i].al_disk != 0) {
	    ad = device_get_softc(sc->amr_drive[i].al_disk);
	    if (ad->amrd_flags & AMRD_OPEN) {		/* drive is mounted, abort detach */
		device_printf(sc->amr_drive[i].al_disk, "still open, can't detach\n");
		goto out;
	    }
	}
    }
    if ((error = amr_shutdown(dev)))
	return(error);

    amr_free(sc);

    /*
     * Deregister the control device on last detach.
     */
    if (--cdev_registered == 0)
	cdevsw_remove(&amr_cdevsw);

    error = 0;
 out:
    splx(s);
    return(error);
}

/********************************************************************************
 * Bring the controller down to a dormant state and detach all child devices.
 *
 * This function is called before detach, system shutdown, or before performing
 * an operation which may add or delete system disks.  (Call amr_startup to
 * resume normal operation.)
 *
 * Note that we can assume that the bufq on the controller is empty, as we won't
 * allow shutdown if any device is open.
 */
int
amr_shutdown(device_t dev)
{
    struct amr_softc	*sc = device_get_softc(dev);
    int			i, s, error;

    debug("called");

    s = splbio();
    error = 0;

    /* assume we're going to shut down */
    sc->amr_state |= AMR_STATE_SHUTDOWN;

    /* flush controller */
    device_printf(sc->amr_dev, "flushing cache...");
    if (amr_flush(sc)) {
	printf("failed\n");
    } else {
	printf("done\n");
    }
    
    /* delete all our child devices */
    for (i = 0; i < AMR_MAXLD; i++) {
	if (sc->amr_drive[i].al_disk != 0) {
	    if ((error = device_delete_child(sc->amr_dev, sc->amr_drive[i].al_disk)) != 0)
		goto out;
	    sc->amr_drive[i].al_disk = 0;
	}
    }

 out:
    splx(s);
    return(error);
}

/********************************************************************************
 * Bring the controller to a quiescent state, ready for system suspend.
 */
int
amr_suspend(device_t dev)
{
    struct amr_softc	*sc = device_get_softc(dev);

    debug("called");

    sc->amr_state |= AMR_STATE_SUSPEND;

    /* flush controller */
    device_printf(sc->amr_dev, "flushing cache...");
    printf("%s\n", amr_flush(sc) ? "failed" : "done");
    
    return(0);
}

/********************************************************************************
 * Bring the controller back to a state ready for operation.
 */
int
amr_resume(device_t dev)
{
    struct amr_softc	*sc = device_get_softc(dev);

    debug("called");

    sc->amr_state &= ~AMR_STATE_SUSPEND;

    return(0);
}

/*******************************************************************************
 * Take an interrupt, or be poked by other code to look for interrupt-worthy
 * status.
 */
void
amr_intr(void *arg)
{
    struct amr_softc	*sc = (struct amr_softc *)arg;

    debug("called");

    /* collect finished commands, queue anything waiting */
    amr_done(sc);
};

/*******************************************************************************
 * Receive a buf structure from a child device and queue it on a particular
 * disk resource, then poke the disk resource to start as much work as it can.
 */
int
amr_submit_buf(struct amr_softc *sc, struct buf *bp)
{
    int		s;

    debug("called");

    s = splbio();
    bufq_insert_tail(&sc->amr_bufq, bp);
    splx(s);
    sc->amr_waitbufs++;
    amr_startio(sc);
    return(0);
}

/********************************************************************************
 * Accept an open operation on the control device.
 */
int
amr_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct amr_softc	*sc = devclass_get_softc(amr_devclass, unit);

    sc->amr_state |= AMR_STATE_OPEN;
    return(0);
}

/********************************************************************************
 * Accept the last close on the control device.
 */
int
amr_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct amr_softc	*sc = devclass_get_softc(amr_devclass, unit);

    sc->amr_state &= ~AMR_STATE_OPEN;
    return (0);
}

/********************************************************************************
 * Handle controller-specific control operations.
 */
int
amr_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    
    switch(cmd) {
    default:	
	return(ENOTTY);
    }
}

/********************************************************************************
 * Handle operations requested by a drive connected to this controller.
 */
int
amr_submit_ioctl(struct amr_softc *sc, struct amr_logdrive *drive, u_long cmd, 
		 caddr_t addr, int32_t flag, struct proc *p)
{
    return(ENOTTY);
}

/********************************************************************************
 ********************************************************************************
                                                                Status Monitoring
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Perform a periodic check of the controller status
 */
static void
amr_periodic(void *data)
{
    struct amr_softc	*sc = (struct amr_softc *)data;
    int			s, i;

    debug("called");

#if 0	
    /* 
     * XXX this is basically wrong - returning a command that's wedged
     * leaves us vulnerable to the controller later completing the command
     * and overwriting memory that may have subsequently been reused.
     */
       
    /*
     * Check for commands that are massively late.  This will need to be 
     * revisited if/when we deal with eg. device format commands.
     * The 30 second value is entirely arbitrary.
     */
    s = splbio();
    if (sc->amr_busycmdcount > 0) {
	for (i = 0; i < AMR_MAXCMD; i++) {
	    /*
	     * If the command has been busy for more than 30 seconds, declare it
	     * wedged and retire it with an error.
	     */
	    if ((sc->amr_busycmd[i] != NULL) &&
		(sc->amr_busycmd[i]->ac_status == AMR_STATUS_BUSY) && 
		((sc->amr_busycmd[i]->ac_stamp + 30) < time_second)) {
		device_printf(sc->amr_dev, "command %d wedged after 30 seconds\n", i);
		sc->amr_busycmd[i]->ac_status = AMR_STATUS_WEDGED;
		amr_completeio(sc->amr_busycmd[i]);
	    }
	}
    }
    splx(s);
#endif

    /* reschedule */
    sc->amr_timeout = timeout(amr_periodic, sc, hz);
}


/********************************************************************************
 ********************************************************************************
                                                                 Command Wrappers
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Interrogate the controller for the operational parameters we require.
 */
static int
amr_query_controller(struct amr_softc *sc)
{
    void	*buf;
    int		i;

    /* try to issue an ENQUIRY3 command */
    if ((buf = amr_enquiry(sc, 2048, AMR_CMD_CONFIG, AMR_CONFIG_ENQ3, 
			   AMR_CONFIG_ENQ3_SOLICITED_FULL)) == NULL) {

	struct amr_enquiry	*ae;

	/* failed, try the old ENQUIRY command */
	if ((ae = (struct amr_enquiry *)amr_enquiry(sc, 2048, AMR_CMD_ENQUIRY, 0, 0)) == NULL) {
	    device_printf(sc->amr_dev, "could not obtain configuration data from controller\n");
	    return(1);
	}
	/* first-time enquiry? */
	if (sc->amr_maxdrives == 0) {
	    device_printf(sc->amr_dev, "firmware %.4s bios %.4s  %dMB memory\n", 
			  ae->ae_adapter.aa_firmware, ae->ae_adapter.aa_bios,
			  ae->ae_adapter.aa_memorysize);
	}
	sc->amr_maxdrives = 8;

	/* 
	 * Cap the maximum number of outstanding I/Os.  AMI's Linux driver doesn't trust
	 * the controller's reported value, and lockups have been seen when we do.
	 */
	sc->amr_maxio = imin(ae->ae_adapter.aa_maxio, AMR_LIMITCMD);

	for (i = 0; i < ae->ae_ldrv.al_numdrives; i++) {
	    sc->amr_drive[i].al_size = ae->ae_ldrv.al_size[i];
	    sc->amr_drive[i].al_state = ae->ae_ldrv.al_state[i];
	    sc->amr_drive[i].al_properties = ae->ae_ldrv.al_properties[i];
	    debug("  drive %d: %d state %x properties %x\n", i, sc->amr_drive[i].al_size,
		  sc->amr_drive[i].al_state, sc->amr_drive[i].al_properties);
	}
	for (; i < AMR_MAXLD; i++)
	    sc->amr_drive[i].al_size = 0xffffffff;
	free(ae, M_DEVBUF);
    } else {
	/*
	 * The "40LD" (40 logical drive support) firmware is mentioned in the Linux
	 * driver, but no adapters from AMI appear to support it.
	 */
	free(buf, M_DEVBUF);
	sc->amr_maxdrives = 40;
	
	/* get static product info */
	if ((buf = amr_enquiry(sc, 2048, AMR_CMD_CONFIG, AMR_CONFIG_PRODINFO, 0)) == NULL) {
	    device_printf(sc->amr_dev, "controller supports 40ld but CONFIG_PRODINFO failed\n");
	    return(1);
	}
	free(buf, M_DEVBUF);
	device_printf(sc->amr_dev, "40LD firmware unsupported; send controller to msmith@freebsd.org\n");
	return(1);
    }
    return(0);
}

/********************************************************************************
 * Run a generic enquiry-style command.
 */
static void *
amr_enquiry(struct amr_softc *sc, size_t bufsize, u_int8_t cmd, u_int8_t cmdsub, u_int8_t cmdqual)
{
    struct amr_command	*ac;
    void		*result;
    u_int8_t		*mbox;
    int			error;

    debug("called");

    error = 1;
    result = NULL;
    
    /* get ourselves a command buffer */
    if ((ac = amr_alloccmd(sc)) == NULL)
	goto out;
    /* allocate the response structure */
    if ((result = malloc(bufsize, M_DEVBUF, M_NOWAIT)) == NULL)
	goto out;
    /* get a command slot */
    ac->ac_flags |= AMR_CMD_PRIORITY | AMR_CMD_DATAOUT;
    if (amr_getslot(ac))
	goto out;
    
    /* map the command so the controller can see it */
    ac->ac_data = result;
    ac->ac_length = bufsize;
    amr_mapcmd(ac);
    
    /* build the command proper */
    mbox = (u_int8_t *)&ac->ac_mailbox;		/* XXX want a real structure for this? */
    mbox[0] = cmd;
    mbox[2] = cmdsub;
    mbox[3] = cmdqual;
    ac->ac_mailbox.mb_physaddr = ac->ac_dataphys;

    /* can't assume that interrupts are going to work here, so play it safe */
    if (amr_poll_command(ac))
	goto out;
    error = ac->ac_status;
    
 out:
    if (ac != NULL)
	amr_releasecmd(ac);
    if ((error != 0) && (result != NULL)) {
	free(result, M_DEVBUF);
	result = NULL;
    }
    return(result);
}

/********************************************************************************
 * Flush the controller's internal cache, return status.
 */
static int
amr_flush(struct amr_softc *sc)
{
    struct amr_command	*ac;
    int			error;

    /* get ourselves a command buffer */
    error = 1;
    if ((ac = amr_alloccmd(sc)) == NULL)
	goto out;
    /* get a command slot */
    ac->ac_flags |= AMR_CMD_PRIORITY | AMR_CMD_DATAOUT;
    if (amr_getslot(ac))
	goto out;
    
    /* build the command proper */
    ac->ac_mailbox.mb_command = AMR_CMD_FLUSH;

    /* we have to poll, as the system may be going down or otherwise damaged */
    if (amr_poll_command(ac))
	goto out;
    error = ac->ac_status;
    
 out:
    if (ac != NULL)
	amr_releasecmd(ac);
    return(error);
}

/********************************************************************************
 * Pull as much work off the softc's work queue as possible and give it to the
 * controller.  Leave a couple of slots free for emergencies.
 *
 * We avoid running at splbio() whenever possible.
 */
static void
amr_startio(struct amr_softc *sc)
{
    struct amr_command	*ac;
    struct amrd_softc	*amrd;
    struct buf		*bp;
    int			blkcount;
    int			driveno;
    int			cmd;
    int			s;

    /* avoid reentrancy */
    if (amr_lock_tas(sc, AMR_LOCK_STARTING))
	return;

    /* spin until something prevents us from doing any work */
    s = splbio();
    for (;;) {

	/* see if there's work to be done */
	if ((bp = bufq_first(&sc->amr_bufq)) == NULL)
	    break;
	/* get a command */
	if ((ac = amr_alloccmd(sc)) == NULL)
	    break;
	/* get a slot for the command */
	if (amr_getslot(ac) != 0) {
	    amr_releasecmd(ac);
	    break;
	}
	/* get the buf containing our work */
	bufq_remove(&sc->amr_bufq, bp);
	sc->amr_waitbufs--;
	splx(s);
	
	/* connect the buf to the command */
	ac->ac_complete = amr_completeio;
	ac->ac_private = bp;
	ac->ac_data = bp->b_data;
	ac->ac_length = bp->b_bcount;
	if (bp->b_flags & B_READ) {
	    ac->ac_flags |= AMR_CMD_DATAIN;
	    cmd = AMR_CMD_LREAD;
	} else {
	    ac->ac_flags |= AMR_CMD_DATAOUT;
	    cmd = AMR_CMD_LWRITE;
	}
	
	/* map the command so the controller can work with it */
	amr_mapcmd(ac);
	
	/* build a suitable I/O command (assumes 512-byte rounded transfers) */
	amrd = (struct amrd_softc *)bp->b_dev->si_drv1;
	driveno = amrd->amrd_drive - sc->amr_drive;
	blkcount = (bp->b_bcount + AMR_BLKSIZE - 1) / AMR_BLKSIZE;

	if ((bp->b_pblkno + blkcount) > sc->amr_drive[driveno].al_size)
	    device_printf(sc->amr_dev, "I/O beyond end of unit (%u,%d > %u)\n", 
			  bp->b_pblkno, blkcount, sc->amr_drive[driveno].al_size);

	/*
	 * Build the I/O command.
	 */
	ac->ac_mailbox.mb_command = cmd;
	ac->ac_mailbox.mb_blkcount = blkcount;
	ac->ac_mailbox.mb_lba = bp->b_pblkno;
	ac->ac_mailbox.mb_physaddr = ac->ac_sgphys;
	ac->ac_mailbox.mb_drive = driveno;
	ac->ac_mailbox.mb_nsgelem = ac->ac_nsgent;

	/* try to give command to controller */
	if (amr_start(ac) != 0) {
	    /* fail the command */
	    ac->ac_status = AMR_STATUS_WEDGED;
	    amr_completeio(ac);
	}
	s = splbio();
    }
    splx(s);
    amr_lock_clr(sc, AMR_LOCK_STARTING);
}

/********************************************************************************
 * Handle completion of an I/O command.
 */
static void
amr_completeio(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    struct buf		*bp = (struct buf *)ac->ac_private;
    int			notify, release;

    notify = 1;
    release = 1;
    
    if (ac->ac_status != AMR_STATUS_SUCCESS) {	/* could be more verbose here? */
	bp->b_error = EIO;
	bp->b_flags |= B_ERROR;

	switch(ac->ac_status) {
	    /* XXX need more information on I/O error reasons */
	case AMR_STATUS_LATE:
	    notify = 0;				/* we've already notified the parent */
	    break;

	case AMR_STATUS_WEDGED:
	    release = 0;			/* the command is still outstanding, we can't release */
	    break;
	    
	default:
	    device_printf(sc->amr_dev, "I/O error - %x\n", ac->ac_status);
	    amr_printcommand(ac);
	    break;
	}
    }
    if (release)
	amr_releasecmd(ac);
    if (notify)
	amrd_intr(bp);
}

/********************************************************************************
 ********************************************************************************
                                                               Command Processing
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Take a command, submit it to the controller and sleep until it completes
 * or fails.  Interrupts must be enabled, returns nonzero on error.
 */
static int
amr_wait_command(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			error, count;
    
    debug("called");

    ac->ac_complete = NULL;
    ac->ac_private = ac;
    if ((error = amr_start(ac)) != 0)
	return(error);
    
    count = 0;
    /* XXX better timeout? */
    while ((ac->ac_status == AMR_STATUS_BUSY) && (count < 30)) {
	tsleep(ac->ac_private, PRIBIO | PCATCH, "amrwcmd", hz);
    }
    
    if (ac->ac_status != 0) {
	device_printf(sc->amr_dev, "I/O error 0x%x\n", ac->ac_status);
	return(EIO);
    }
    return(0);
}

/********************************************************************************
 * Take a command, submit it to the controller and busy-wait for it to return.
 * Returns nonzero on error.  Can be safely called with interrupts enabled.
 */
static int
amr_poll_command(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			error, count, s;

    debug("called");

    ac->ac_complete = NULL;
    ac->ac_private = NULL;
    if ((error = amr_start(ac)) != 0)
	return(error);

    count = 0;
    do {
	/* 
	 * Poll for completion, although the interrupt handler may beat us to it. 
	 * Note that the timeout here is somewhat arbitrary.
	 */
	amr_done(sc);
    } while ((ac->ac_status == AMR_STATUS_BUSY) && (count++ < 100000));
    s = splbio();
    if (ac->ac_status != AMR_STATUS_BUSY) {
	TAILQ_REMOVE(&sc->amr_work, ac, ac_link);
	sc->amr_workcount--;
	error = 0;
    } else {
	/* take the command out of the busy list, mark slot as bogus */
	sc->amr_busycmd[ac->ac_slot] = (struct amr_command *)sc;
	error = EIO;
	device_printf(sc->amr_dev, "I/O error 0x%x\n", ac->ac_status);
    }
    splx(s);
    return(error);
}

/********************************************************************************
 * Get a free command slot.
 */
static int
amr_getslot(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			s, slot, limit;

    debug("called");
    
    /* enforce slot usage limit */
    limit = (ac->ac_flags & AMR_CMD_PRIORITY) ? sc->amr_maxio : sc->amr_maxio - 4;
    if (sc->amr_busycmdcount > limit)
	return(EBUSY);
    
    /*
     * Allocate a slot
     */
    s = splbio();
    for (slot = 0; slot < sc->amr_maxio; slot++) {
	if (sc->amr_busycmd[slot] == NULL)
	    break;
    }
    if (slot < sc->amr_maxio) {
	sc->amr_busycmdcount++;
	sc->amr_busycmd[slot] = ac;
    }
    splx(s);

    /* out of slots? */
    if (slot >= sc->amr_maxio)
	return(EBUSY);
    
    ac->ac_slot = slot;
    return(0);
}

/********************************************************************************
 * Map/unmap (ac)'s data in the controller's addressable space.
 */
static void
amr_setup_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct amr_command	*ac = (struct amr_command *)arg;
    struct amr_softc	*sc = ac->ac_sc;
    struct amr_sgentry	*sg;
    int			i;

    debug("called");

    /* get base address of s/g table */
    sg = sc->amr_sgtable + (ac->ac_slot * AMR_NSEG);

    /* save s/g table information in command */
    ac->ac_nsgent = nsegments;
    ac->ac_sgphys = sc->amr_sgbusaddr + (ac->ac_slot * AMR_NSEG * sizeof(struct amr_sgentry));
    ac->ac_dataphys = segs[0].ds_addr;

    /* populate s/g table */
    for (i = 0; i < nsegments; i++, sg++) {
	sg->sg_addr = segs[i].ds_addr;
	sg->sg_count = segs[i].ds_len;
    }
}

static void
amr_mapcmd(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;

    debug("called");

    /* if the command involves data at all */
    if (ac->ac_data != NULL) {
	
	/* map the data buffer into bus space and build the s/g list */
	bus_dmamap_load(sc->amr_buffer_dmat, ac->ac_dmamap, ac->ac_data, ac->ac_length, 
			amr_setup_dmamap, ac, 0);
	if (ac->ac_flags & AMR_CMD_DATAIN)
	    bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_PREREAD);
	if (ac->ac_flags & AMR_CMD_DATAOUT)
	    bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_PREWRITE);
    }
}

static void
amr_unmapcmd(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;

    debug("called");

    /* if the command involved data at all */
    if (ac->ac_data != NULL) {
	
	if (ac->ac_flags & AMR_CMD_DATAIN)
	    bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_POSTREAD);
	if (ac->ac_flags & AMR_CMD_DATAOUT)
	    bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->amr_buffer_dmat, ac->ac_dmamap); 
    }
}

/********************************************************************************
 * Take a command and give it to the controller. 
 */
static int
amr_start(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			done, s, i;
    
    debug("called");

    /* 
     * Save the slot number so that we can locate this command when complete.
     * Note that ident = 0 seems to be special, so we don't use it.
     */
    ac->ac_mailbox.mb_ident = ac->ac_slot + 1;

    /* set the busy flag when we copy the mailbox in */
    ac->ac_mailbox.mb_busy = 1;

    /* set impossible status so that a woken sleeper can tell the command is busy */
    ac->ac_status = AMR_STATUS_BUSY;

    /* 
     * Spin waiting for the mailbox, give up after ~1 second.
     */
    debug("wait for mailbox");
    for (i = 10000, done = 0; (i > 0) && !done; i--) {
	s = splbio();
	
	/* is the mailbox free? */
	if (sc->amr_mailbox->mb_busy == 0) {
	    debug("got mailbox");
	    sc->amr_mailbox64->mb64_segment = 0;
	    bcopy(&ac->ac_mailbox, sc->amr_mailbox, AMR_MBOX_CMDSIZE);
	    sc->amr_submit_command(sc);
	    done = 1;
	    sc->amr_workcount++;
	    TAILQ_INSERT_TAIL(&sc->amr_work, ac, ac_link);

	    /* not free, try to clean up while we wait */
	} else {
	    debug("busy flag %x\n", sc->amr_mailbox->mb_busy);
	    /* this is somewhat ugly */
	    DELAY(100);
	}
	splx(s);	/* drop spl to allow completion interrupts */
    }
    
    /* command is enqueued? */
    if (done) {
	ac->ac_stamp = time_second;
	debug("posted command");
	return(0);
    }
    
    /*
     * The controller wouldn't take the command.  Revoke the slot
     * that the command was given and return with a bad status.
     */
    sc->amr_busycmd[ac->ac_slot] = NULL;
    device_printf(sc->amr_dev, "controller wedged (not taking commands)\n");
    ac->ac_status = AMR_STATUS_WEDGED;
    amr_complete(sc);
    return(EIO);
}

/********************************************************************************
 * Extract one or more completed commands from the controller (sc)
 *
 * Returns nonzero if any commands on the work queue were marked as completed.
 */
static int
amr_done(struct amr_softc *sc)
{
    struct amr_command	*ac;
    struct amr_mailbox	mbox;
    int			i, idx, s, result;
    
    debug("called");

    /* See if there's anything for us to do */
    result = 0;

    /* loop collecting completed commands */
    s = splbio();
    for (;;) {
	/* poll for a completed command's identifier and status */
	if (sc->amr_get_work(sc, &mbox)) {
	    result = 1;
	    
	    /* iterate over completed commands in this result */
	    for (i = 0; i < mbox.mb_nstatus; i++) {
		/* get pointer to busy command */
		idx = mbox.mb_completed[i] - 1;
		ac = sc->amr_busycmd[idx];

		/* really a busy command? */
		if (ac != NULL) {

		    /* pull the command from the busy index */
		    sc->amr_busycmd[idx] = NULL;
		    sc->amr_busycmdcount--;
		
		    /* aborted command? */
		    if (ac == (struct amr_command *)sc) {
			device_printf(sc->amr_dev, "aborted command completed (%d)\n", idx);
			sc->amr_busycmd[idx] = NULL;	/* free the slot again */
			ac = NULL;

			/* wedged command? */
		    } else if (ac->ac_status == AMR_STATUS_WEDGED) {
			device_printf(sc->amr_dev, "wedged command completed (%d)\n", idx);
			ac->ac_status = AMR_STATUS_LATE;

			/* completed normally, save status */
		    } else {
			ac->ac_status = mbox.mb_status;
			debug("completed command with status %x", mbox.mb_status);
		    }
		}
	    }
	} else {
	    break;
	}
    }
    
    /* if we've completed any commands, try posting some more */
    if (result)
	amr_startio(sc);
    
    /* handle completion and timeouts */
    amr_complete(sc);
    
    return(result);
}

/********************************************************************************
 * Do completion processing on done commands on (sc)
 */
static void
amr_complete(struct amr_softc *sc)
{
    struct amr_command	*ac, *nc;
    int			s, count;

    debug("called");

    if (amr_lock_tas(sc, AMR_LOCK_COMPLETING))
	return;

    s = splbio();
    count = 0;

    /* scan the list of busy/done commands */
    ac = TAILQ_FIRST(&sc->amr_work);
    while (ac != NULL) {
	nc = TAILQ_NEXT(ac, ac_link);

	/* Command has been completed in some fashion */
	if (ac->ac_status != AMR_STATUS_BUSY) {

	    /* unmap the command's data buffer */
	    amr_unmapcmd(ac);
	    
	    /* 
	     * Is there a completion handler? 
	     */
	    if (ac->ac_complete != NULL) {

		/* remove and give to completion handler */
		TAILQ_REMOVE(&sc->amr_work, ac, ac_link);
		sc->amr_workcount--;
		ac->ac_complete(ac);
	    
		/* 
		 * Is someone sleeping on this one?
		 */
	    } else if (ac->ac_private != NULL) {

		/* remove and wake up */
		TAILQ_REMOVE(&sc->amr_work, ac, ac_link);
		sc->amr_workcount--;
		wakeup_one(ac->ac_private);

		/*
		 * Leave it for a polling caller.
		 */
	    } else {
	    }
	}
	ac = nc;
    }
    splx(s);

    amr_lock_clr(sc, AMR_LOCK_COMPLETING);
}

/********************************************************************************
 ********************************************************************************
                                                        Command Buffer Management
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Get a new command buffer.
 *
 * This may return NULL in low-memory cases.
 *
 * Note that using malloc() is expensive (the command buffer is << 1 page) but
 * necessary if we are to be a loadable module before the zone allocator is fixed.
 *
 * If possible, we recycle a command buffer that's been used before.
 *
 * XXX Note that command buffers are not cleaned out - it is the caller's 
 *     responsibility to ensure that all required fields are filled in before
 *     using a buffer.
 */
static struct amr_command *
amr_alloccmd(struct amr_softc *sc)
{
    struct amr_command	*ac;
    int			error;
    int			s;

    debug("called");

    s = splbio();
    if ((ac = TAILQ_FIRST(&sc->amr_freecmds)) != NULL)
	TAILQ_REMOVE(&sc->amr_freecmds, ac, ac_link);
    splx(s);

    /* allocate a new command buffer? */
    if (ac == NULL) {
	ac = (struct amr_command *)malloc(sizeof(*ac), M_DEVBUF, M_NOWAIT);
	if (ac != NULL) {
	    bzero(ac, sizeof(*ac));
	    ac->ac_sc = sc;
	    error = bus_dmamap_create(sc->amr_buffer_dmat, 0, &ac->ac_dmamap);
	    if (error) {
		free(ac, M_DEVBUF);
		return(NULL);
	    }
	}
    }
    bzero(&ac->ac_mailbox, sizeof(struct amr_mailbox));
    return(ac);
}

/********************************************************************************
 * Release a command buffer for recycling.
 *
 * XXX It might be a good idea to limit the number of commands we save for reuse
 *     if it's shown that this list bloats out massively.
 */
static void
amr_releasecmd(struct amr_command *ac)
{
    int		s;
    
    debug("called");

    s = splbio();
    TAILQ_INSERT_HEAD(&ac->ac_sc->amr_freecmds, ac, ac_link);
    splx(s);
}

/********************************************************************************
 * Permanently discard a command buffer.
 */
static void
amr_freecmd(struct amr_command *ac) 
{
    struct amr_softc	*sc = ac->ac_sc;
    
    debug("called");

    bus_dmamap_destroy(sc->amr_buffer_dmat, ac->ac_dmamap);
    free(ac, M_DEVBUF);
}

/********************************************************************************
 ********************************************************************************
                                                         Interface-specific Shims
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Tell the controller that the mailbox contains a valid command
 */
static void
amr_quartz_submit_command(struct amr_softc *sc)
{
    debug("called");

    sc->amr_mailbox->mb_poll = 0;
    sc->amr_mailbox->mb_ack = 0;
    while(AMR_QGET_IDB(sc) & AMR_QIDB_SUBMIT)
	;				/* XXX aiee! what if it dies? */
    AMR_QPUT_IDB(sc, sc->amr_mailboxphys | AMR_QIDB_SUBMIT);
}

static void
amr_std_submit_command(struct amr_softc *sc)
{
    debug("called");

    /* XXX write barrier? */
    while (AMR_SGET_MBSTAT(sc) & AMR_SMBOX_BUSYFLAG)
	;				/* XXX aiee! what if it dies? */
    AMR_SPOST_COMMAND(sc);
}

/********************************************************************************
 * Claim any work that the controller has completed; acknowledge completion,
 * save details of the completion in (mbsave)
 */
static int
amr_quartz_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave)
{
    int		s, worked;
    u_int32_t	outd;
    
/*    debug("called"); */

    worked = 0;
    s = splbio();

    /* work waiting for us? */
    if ((outd = AMR_QGET_ODB(sc)) == AMR_QODB_READY) {
	AMR_QPUT_ODB(sc, AMR_QODB_READY);

	/* save mailbox, which contains a list of completed commands */
	/* XXX read barrier? */
	bcopy(sc->amr_mailbox, mbsave, sizeof(*mbsave));

	/* acknowledge that we have the commands */
	AMR_QPUT_IDB(sc, sc->amr_mailboxphys | AMR_QIDB_ACK);
	while(AMR_QGET_IDB(sc) & AMR_QIDB_ACK)
	    ;				/* XXX aiee! what if it dies? */
	worked = 1;			/* got some work */
    }

    splx(s);
    return(worked);
}

static int
amr_std_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave)
{
    int		s, worked;
    u_int8_t	istat;

    debug("called");

    worked = 0;
    s = splbio();

    /* check for valid interrupt status */
    istat = AMR_SGET_ISTAT(sc);
    if ((istat & AMR_SINTR_VALID) != 0) {
	AMR_SPUT_ISTAT(sc, istat);	/* ack interrupt status */

	/* save mailbox, which contains a list of completed commands */
	/* XXX read barrier? */
	bcopy(sc->amr_mailbox, mbsave, sizeof(*mbsave));

	AMR_SACK_INTERRUPT(sc);		/* acknowledge we have the mailbox */
	worked = 1;
    }

    splx(s);
    return(worked);
}

/********************************************************************************
 * Notify the controller of the mailbox location.
 */
static void
amr_quartz_attach_mailbox(struct amr_softc *sc)
{
    /* Quartz is given the mailbox location when a command is submitted */
}

static void
amr_std_attach_mailbox(struct amr_softc *sc)
{

    /* program the mailbox physical address */
    AMR_SBYTE_SET(sc, AMR_SMBOX_0, sc->amr_mailboxphys         & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_1, (sc->amr_mailboxphys >>  8) & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_2, (sc->amr_mailboxphys >> 16) & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_3, (sc->amr_mailboxphys >> 24) & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_ENABLE, AMR_SMBOX_ADDR);

    /* clear any outstanding interrupt and enable interrupts proper */
    AMR_SACK_INTERRUPT(sc);
    AMR_SENABLE_INTR(sc);
}

/********************************************************************************
 ********************************************************************************
                                                                        Debugging
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Print the command (ac) in human-readable format
 */
static void
amr_printcommand(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    struct amr_sgentry	*sg;
    int			i;
    
    device_printf(sc->amr_dev, "cmd %x  ident %d  drive %d\n",
		  ac->ac_mailbox.mb_command, ac->ac_mailbox.mb_ident, ac->ac_mailbox.mb_drive);
    device_printf(sc->amr_dev, "blkcount %d  lba %d\n", 
		  ac->ac_mailbox.mb_blkcount, ac->ac_mailbox.mb_lba);
    device_printf(sc->amr_dev, "virtaddr %p  length %lu\n", ac->ac_data, (unsigned long)ac->ac_length);
    device_printf(sc->amr_dev, "sg physaddr %08x  nsg %d\n",
		  ac->ac_mailbox.mb_physaddr, ac->ac_mailbox.mb_nsgelem);

    /* get base address of s/g table */
    sg = sc->amr_sgtable + (ac->ac_slot * AMR_NSEG);
    for (i = 0; i < ac->ac_mailbox.mb_nsgelem; i++, sg++)
	device_printf(sc->amr_dev, "  %x/%d\n", sg->sg_addr, sg->sg_count);
}

/********************************************************************************
 * Print information on all the controllers in the system, useful mostly 
 * for calling from DDB.
 */
void
amr_report(void)
{
    struct amr_softc	*sc;
    int			i, s;
    
    s = splbio();
    for (i = 0; (sc = devclass_get_softc(amr_devclass, i)) != NULL; i++) {
	device_printf(sc->amr_dev, "amr_waitbufs %d  amr_busycmdcount %d  amr_workcount %d\n",
		      sc->amr_waitbufs, sc->amr_busycmdcount, sc->amr_workcount);
    }
}
