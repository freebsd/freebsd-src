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
 * Driver for the Mylex DAC960 family of RAID controllers.
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

#include <dev/mlx/mlxio.h>
#include <dev/mlx/mlxvar.h>
#include <dev/mlx/mlxreg.h>

#if 0
#define debug(fmt, args...)	printf("%s: " fmt "\n", __FUNCTION__ , ##args)
#else
#define debug(fmt, args...)
#endif

#define MLX_CDEV_MAJOR	130

static struct cdevsw mlx_cdevsw = {
		/* open */	mlx_open,
		/* close */	mlx_close,
		/* read */	noread,
		/* write */	nowrite,
		/* ioctl */	mlx_ioctl,
		/* poll */	nopoll,
		/* mmap */	nommap,
		/* strategy */	nostrategy,
		/* name */ 	"mlx",
		/* maj */	MLX_CDEV_MAJOR,
		/* dump */	nodump,
		/* psize */ 	nopsize,
		/* flags */	0,
		/* bmaj */	-1
};

static int	cdev_registered = 0;
devclass_t	mlx_devclass;

/*
 * Per-interface accessor methods
 */
static int			mlx_v3_tryqueue(struct mlx_softc *sc, struct mlx_command *mc);
static int			mlx_v3_findcomplete(struct mlx_softc *sc, u_int8_t *slot, u_int16_t *status);
static void			mlx_v3_intaction(struct mlx_softc *sc, int action);

static int			mlx_v4_tryqueue(struct mlx_softc *sc, struct mlx_command *mc);
static int			mlx_v4_findcomplete(struct mlx_softc *sc, u_int8_t *slot, u_int16_t *status);
static void			mlx_v4_intaction(struct mlx_softc *sc, int action);

/*
 * Status monitoring
 */
static void			mlx_periodic(void *data);
static void			mlx_periodic_enquiry(struct mlx_command *mc);
static void			mlx_periodic_eventlog_poll(struct mlx_softc *sc);
static void			mlx_periodic_eventlog_respond(struct mlx_command *mc);
static void			mlx_periodic_rebuild(struct mlx_command *mc);

/*
 * Channel Pause
 */
static void			mlx_pause_action(struct mlx_softc *sc);
static void			mlx_pause_done(struct mlx_command *mc);

/*
 * Command submission.
 */
static void			*mlx_enquire(struct mlx_softc *sc, int command, size_t bufsize, 
					     void (*complete)(struct mlx_command *mc));
static int			mlx_flush(struct mlx_softc *sc);
static int			mlx_rebuild(struct mlx_softc *sc, int channel, int target);
static int			mlx_wait_command(struct mlx_command *mc);
static int			mlx_poll_command(struct mlx_command *mc);
static void			mlx_startio(struct mlx_softc *sc);
static void			mlx_completeio(struct mlx_command *mc);
static int			mlx_user_command(struct mlx_softc *sc, struct mlx_usercommand *mu);

/*
 * Command buffer allocation.
 */
static struct mlx_command	*mlx_alloccmd(struct mlx_softc *sc);
static void			mlx_releasecmd(struct mlx_command *mc);
static void			mlx_freecmd(struct mlx_command *mc);

/*
 * Command management.
 */
static int			mlx_getslot(struct mlx_command *mc);
static void			mlx_mapcmd(struct mlx_command *mc);
static void			mlx_unmapcmd(struct mlx_command *mc);
static int			mlx_start(struct mlx_command *mc);
static int			mlx_done(struct mlx_softc *sc);
static void			mlx_complete(struct mlx_softc *sc);

/*
 * Debugging.
 */
static char			*mlx_diagnose_command(struct mlx_command *mc);
static char			*mlx_name_controller(u_int32_t hwid);


/*
 * Utility functions.
 */
static struct mlx_sysdrive	*mlx_findunit(struct mlx_softc *sc, int unit);

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
mlx_free(struct mlx_softc *sc)
{
    struct mlx_command	*mc;

    debug("called");

    /* cancel status timeout */
    untimeout(mlx_periodic, sc, sc->mlx_timeout);

    /* throw away any command buffers */
    while ((mc = TAILQ_FIRST(&sc->mlx_freecmds)) != NULL) {
	TAILQ_REMOVE(&sc->mlx_freecmds, mc, mc_link);
	mlx_freecmd(mc);
    }

    /* destroy data-transfer DMA tag */
    if (sc->mlx_buffer_dmat)
	bus_dma_tag_destroy(sc->mlx_buffer_dmat);

    /* free and destroy DMA memory and tag for s/g lists */
    if (sc->mlx_sgtable)
	bus_dmamem_free(sc->mlx_sg_dmat, sc->mlx_sgtable, sc->mlx_sg_dmamap);
    if (sc->mlx_sg_dmat)
	bus_dma_tag_destroy(sc->mlx_sg_dmat);

    /* disconnect the interrupt handler */
    if (sc->mlx_intr)
	bus_teardown_intr(sc->mlx_dev, sc->mlx_irq, sc->mlx_intr);
    if (sc->mlx_irq != NULL)
	bus_release_resource(sc->mlx_dev, SYS_RES_IRQ, 0, sc->mlx_irq);

    /* destroy the parent DMA tag */
    if (sc->mlx_parent_dmat)
	bus_dma_tag_destroy(sc->mlx_parent_dmat);

    /* release the register window mapping */
    if (sc->mlx_mem != NULL)
	bus_release_resource(sc->mlx_dev, SYS_RES_MEMORY, 
			     (sc->mlx_iftype == MLX_IFTYPE_3) ? MLX_CFG_BASE1 : MLX_CFG_BASE0, sc->mlx_mem);
}

/********************************************************************************
 * Map the scatter/gather table into bus space
 */
static void
mlx_dma_map_sg(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct mlx_softc	*sc = (struct mlx_softc *)arg;

    debug("called");

    /* save base of s/g table's address in bus space */
    sc->mlx_sgbusaddr = segs->ds_addr;
}

static int
mlx_sglist_map(struct mlx_softc *sc)
{
    size_t	segsize;
    int		error;

    debug("called");

    /* destroy any existing mappings */
    if (sc->mlx_sgtable)
	bus_dmamem_free(sc->mlx_sg_dmat, sc->mlx_sgtable, sc->mlx_sg_dmamap);
    if (sc->mlx_sg_dmat)
	bus_dma_tag_destroy(sc->mlx_sg_dmat);

    /*
     * Create a single tag describing a region large enough to hold all of
     * the s/g lists we will need.
     */
    segsize = sizeof(struct mlx_sgentry) * MLX_NSEG * sc->mlx_maxiop;
    error = bus_dma_tag_create(sc->mlx_parent_dmat, 	/* parent */
			       1, 0, 			/* alignment, boundary */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR, 	/* highaddr */
			       NULL, NULL, 		/* filter, filterarg */
			       segsize, 1,		/* maxsize, nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			       0,			/* flags */
			       &sc->mlx_sg_dmat);
    if (error != 0) {
	device_printf(sc->mlx_dev, "can't allocate scatter/gather DMA tag\n");
	return(ENOMEM);
    }

    /*
     * Allocate enough s/g maps for all commands and permanently map them into
     * controller-visible space.
     *	
     * XXX this assumes we can get enough space for all the s/g maps in one 
     * contiguous slab.  We may need to switch to a more complex arrangement where
     * we allocate in smaller chunks and keep a lookup table from slot to bus address.
     */
    error = bus_dmamem_alloc(sc->mlx_sg_dmat, (void **)&sc->mlx_sgtable, BUS_DMA_NOWAIT, &sc->mlx_sg_dmamap);
    if (error) {
	device_printf(sc->mlx_dev, "can't allocate s/g table\n");
	return(ENOMEM);
    }
    bus_dmamap_load(sc->mlx_sg_dmat, sc->mlx_sg_dmamap, sc->mlx_sgtable, segsize, mlx_dma_map_sg, sc, 0);
    return(0);
}

/********************************************************************************
 * Initialise the controller and softc
 */
int
mlx_attach(struct mlx_softc *sc)
{
    struct mlx_enquiry	*me;
    struct mlx_enquiry2	*me2;
    int			rid, error;

    debug("called");

    /*
     * Initialise per-controller queues.
     */
    TAILQ_INIT(&sc->mlx_donecmd);
    TAILQ_INIT(&sc->mlx_freecmds);
    bufq_init(&sc->mlx_bufq);

    /* 
     * Select accessor methods based on controller interface type.
     */
    switch(sc->mlx_iftype) {
    case MLX_IFTYPE_3:
	sc->mlx_tryqueue	= mlx_v3_tryqueue;
	sc->mlx_findcomplete	= mlx_v3_findcomplete;
	sc->mlx_intaction	= mlx_v3_intaction;
	break;
    case MLX_IFTYPE_4:
	sc->mlx_tryqueue	= mlx_v4_tryqueue;
	sc->mlx_findcomplete	= mlx_v4_findcomplete;
	sc->mlx_intaction	= mlx_v4_intaction;
	break;
    default:
	device_printf(sc->mlx_dev, "attaching unsupported interface version %d\n", sc->mlx_iftype);
	return(ENXIO);		/* should never happen */
    }

    /* disable interrupts before we start talking to the controller */
    sc->mlx_intaction(sc, MLX_INTACTION_DISABLE);

    /* 
     * Allocate and connect our interrupt.
     */
    rid = 0;
    sc->mlx_irq = bus_alloc_resource(sc->mlx_dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
    if (sc->mlx_irq == NULL) {
	device_printf(sc->mlx_dev, "couldn't allocate interrupt\n");
	mlx_free(sc);
	return(ENXIO);
    }
    error = bus_setup_intr(sc->mlx_dev, sc->mlx_irq, INTR_TYPE_BIO,  mlx_intr, sc, &sc->mlx_intr);
    if (error) {
	device_printf(sc->mlx_dev, "couldn't set up interrupt\n");
	mlx_free(sc);
	return(ENXIO);
    }

    /*
     * Create DMA tag for mapping buffers into controller-addressable space.
     */
    error = bus_dma_tag_create(sc->mlx_parent_dmat, 	/* parent */
			       1, 0, 			/* alignment, boundary */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR, 	/* highaddr */
			       NULL, NULL, 		/* filter, filterarg */
			       MAXBSIZE, MLX_NSEG,	/* maxsize, nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			       0,			/* flags */
			       &sc->mlx_buffer_dmat);
    if (error != 0) {
	device_printf(sc->mlx_dev, "can't allocate buffer DMA tag\n");
	return(ENOMEM);
    }

    /*
     * Create an initial set of s/g mappings.
     */
    sc->mlx_maxiop = 2;
    error = mlx_sglist_map(sc);
    if (error != 0) {
	device_printf(sc->mlx_dev, "couldn't make initial s/g list mapping\n");
	return(error);
    }

    /*
     * Probe the controller for more information.
     */
    /* send an ENQUIRY to the controller */
    if ((me = mlx_enquire(sc, MLX_CMD_ENQUIRY, sizeof(*me), NULL)) == NULL) {
	device_printf(sc->mlx_dev, "ENQUIRY failed\n");
	return(ENXIO);
    }

    /* pull information out of the ENQUIRY result */
    sc->mlx_fwminor = me->me_fwminor;
    sc->mlx_fwmajor = me->me_fwmajor;
    sc->mlx_maxiop = me->me_max_commands;
    sc->mlx_lastevent = sc->mlx_currevent = me->me_event_log_seq_num;
    free(me, M_DEVBUF);

    /* send an ENQUIRY2 to the controller */
    if ((me2 = mlx_enquire(sc, MLX_CMD_ENQUIRY2, sizeof(*me2), NULL)) == NULL) {
	device_printf(sc->mlx_dev, "ENQUIRY2 failed\n");
	return(ENXIO);
    }

    /* pull information out of the ENQUIRY2 result */
    sc->mlx_nchan = me2->me_configured_channels;
    sc->mlx_maxiosize = me2->me_maxblk;
    sc->mlx_maxtarg = me2->me_max_targets;
    sc->mlx_maxtags = me2->me_max_tags;
    sc->mlx_scsicap = me2->me_scsi_cap;
    sc->mlx_hwid = me2->me_hardware_id;

    /* print a little information about the controller and ourselves */
    device_printf(sc->mlx_dev, "Mylex %s, firmware %d.%d, %dMB RAM\n", 
	   mlx_name_controller(sc->mlx_hwid), sc->mlx_fwmajor, sc->mlx_fwminor,
	me2->me_mem_size / (1024 * 1024));
    free(me2, M_DEVBUF);

    /*
     * Do quirk/feature related things.
     */
    switch(sc->mlx_iftype) {
    case MLX_IFTYPE_3:
	/* XXX certify 3.52? */
	if (sc->mlx_fwminor != 51) {
	    device_printf(sc->mlx_dev, " *** WARNING *** This firmware revision is NOT SUPPORTED\n");
	    device_printf(sc->mlx_dev, " *** WARNING *** Use revision 3.51 only\n");
	}
	break;
    case MLX_IFTYPE_4:
	/* XXX certify firmware versions? */
	if (sc->mlx_fwminor != 6) {
	    device_printf(sc->mlx_dev, " *** WARNING *** This firmware revision is NOT SUPPORTED\n");
	    device_printf(sc->mlx_dev, " *** WARNING *** Use revision 4.6 only\n");
	}
	break;
    default:
	device_printf(sc->mlx_dev, "interface version corrupted to %d\n", sc->mlx_iftype);
	return(ENXIO);		/* should never happen */
    }

    /*
     * Create the final set of s/g mappings now that we know how many commands
     * the controller actually supports.
     */
    error = mlx_sglist_map(sc);
    if (error != 0) {
	device_printf(sc->mlx_dev, "couldn't make initial s/g list mapping\n");
	return(error);
    }

    /*
     * No rebuild or check is in progress.
     */
    sc->mlx_rebuild = -1;
    sc->mlx_check = -1;

    /*
     * Register the control device on first attach.
     */
    if (cdev_registered++ == 0)
	cdevsw_add(&mlx_cdevsw);

    /*
     * Start the timeout routine.
     */
    sc->mlx_timeout = timeout(mlx_periodic, sc, hz);

    return(0);
}

/********************************************************************************
 * Locate disk resources and attach children to them.
 */
void
mlx_startup(struct mlx_softc *sc)
{
    struct mlx_enq_sys_drive	*mes;
    struct mlx_sysdrive		*dr;
    int				i, error;

    debug("called");
    
    /*
     * Scan all the system drives and attach children for those that
     * don't currently have them.
     */
    mes = mlx_enquire(sc, MLX_CMD_ENQSYSDRIVE, sizeof(*mes) * MLX_MAXDRIVES, NULL);
    if (mes == NULL) {
	device_printf(sc->mlx_dev, "error fetching drive status");
	return;
    }
    
    /* iterate over drives returned */
    for (i = 0, dr = &sc->mlx_sysdrive[0];
	 (i < MLX_MAXDRIVES) && (mes[i].sd_size != 0xffffffff);
	 i++, dr++) {
	/* are we already attached to this drive? */
    	if (dr->ms_disk == 0) {
	    /* pick up drive information */
	    dr->ms_size = mes[i].sd_size;
	    dr->ms_raidlevel = mes[i].sd_raidlevel & 0xf;
	    dr->ms_state = mes[i].sd_state;

	    /* generate geometry information */
	    if (sc->mlx_geom == MLX_GEOM_128_32) {
		dr->ms_heads = 128;
		dr->ms_sectors = 32;
		dr->ms_cylinders = dr->ms_size / (128 * 32);
	    } else {        /* MLX_GEOM_255/63 */
		dr->ms_heads = 255;
		dr->ms_sectors = 63;
		dr->ms_cylinders = dr->ms_size / (255 * 63);
	    }
	    dr->ms_disk =  device_add_child(sc->mlx_dev, /*"mlxd"*/NULL, -1, dr);
	    if (dr->ms_disk == 0)
		device_printf(sc->mlx_dev, "device_add_child failed\n");
	}
    }
    free(mes, M_DEVBUF);
    if ((error = bus_generic_attach(sc->mlx_dev)) != 0)
	device_printf(sc->mlx_dev, "bus_generic_attach returned %d", error);

    /* mark controller back up */
    sc->mlx_state &= ~MLX_STATE_SHUTDOWN;

    /* enable interrupts */
    sc->mlx_intaction(sc, MLX_INTACTION_ENABLE);
}

/********************************************************************************
 * Disconnect from the controller completely, in preparation for unload.
 */
int
mlx_detach(device_t dev)
{
    struct mlx_softc	*sc = device_get_softc(dev);
    int			error;

    debug("called");

    if (sc->mlx_state & MLX_STATE_OPEN)
	return(EBUSY);

    if ((error = mlx_shutdown(dev)))
	return(error);

    mlx_free(sc);

    /*
     * Deregister the control device on last detach.
     */
    if (--cdev_registered == 0)
	cdevsw_remove(&mlx_cdevsw);

    return(0);
}

/********************************************************************************
 * Bring the controller down to a dormant state and detach all child devices.
 *
 * This function is called before detach, system shutdown, or before performing
 * an operation which may add or delete system disks.  (Call mlx_startup to
 * resume normal operation.)
 *
 * Note that we can assume that the bufq on the controller is empty, as we won't
 * allow shutdown if any device is open.
 */
int
mlx_shutdown(device_t dev)
{
    struct mlx_softc	*sc = device_get_softc(dev);
    struct mlxd_softc	*mlxd;
    int			i, s, error;

    debug("called");

    s = splbio();
    error = 0;

    /* assume we're going to shut down */
    sc->mlx_state |= MLX_STATE_SHUTDOWN;
    for (i = 0; i < MLX_MAXDRIVES; i++) {
	if (sc->mlx_sysdrive[i].ms_disk != 0) {
	    mlxd = device_get_softc(sc->mlx_sysdrive[i].ms_disk);
	    if (mlxd->mlxd_flags & MLXD_OPEN) {		/* drive is mounted, abort shutdown */
		sc->mlx_state &= ~MLX_STATE_SHUTDOWN;
		device_printf(sc->mlx_sysdrive[i].ms_disk, "still open, can't shutdown\n");
		error = EBUSY;
		goto out;
	    }
	}
    }

    /* flush controller */
    device_printf(sc->mlx_dev, "flushing cache...");
    if (mlx_flush(sc)) {
	printf("failed\n");
    } else {
	printf("done\n");
    }
    
    /* delete all our child devices */
    for (i = 0; i < MLX_MAXDRIVES; i++) {
	if (sc->mlx_sysdrive[i].ms_disk != 0) {
	    if ((error = device_delete_child(sc->mlx_dev, sc->mlx_sysdrive[i].ms_disk)) != 0)
		goto out;
	    sc->mlx_sysdrive[i].ms_disk = 0;
	}
    }

    bus_generic_detach(sc->mlx_dev);

 out:
    splx(s);
    return(error);
}

/********************************************************************************
 * Bring the controller to a quiescent state, ready for system suspend.
 */
int
mlx_suspend(device_t dev)
{
    struct mlx_softc	*sc = device_get_softc(dev);
    int			s;

    debug("called");

    s = splbio();
    sc->mlx_state |= MLX_STATE_SUSPEND;
    
    /* flush controller */
    device_printf(sc->mlx_dev, "flushing cache...");
    printf("%s\n", mlx_flush(sc) ? "failed" : "done");

    sc->mlx_intaction(sc, MLX_INTACTION_DISABLE);
    splx(s);

    return(0);
}

/********************************************************************************
 * Bring the controller back to a state ready for operation.
 */
int
mlx_resume(device_t dev)
{
    struct mlx_softc	*sc = device_get_softc(dev);

    debug("called");

    sc->mlx_state &= ~MLX_STATE_SUSPEND;
    sc->mlx_intaction(sc, MLX_INTACTION_ENABLE);

    return(0);
}

/*******************************************************************************
 * Take an interrupt, or be poked by other code to look for interrupt-worthy
 * status.
 */
void
mlx_intr(void *arg)
{
    struct mlx_softc	*sc = (struct mlx_softc *)arg;
    int			worked;

    debug("called");

    /* spin collecting finished commands */
    worked = 0;
    while (mlx_done(sc))
	worked = 1;

    /* did we do anything? */
    if (worked)
	mlx_complete(sc);
};

/*******************************************************************************
 * Receive a buf structure from a child device and queue it on a particular
 * disk resource, then poke the disk resource to start as much work as it can.
 */
int
mlx_submit_buf(struct mlx_softc *sc, struct buf *bp)
{
    debug("called");

    bufq_insert_tail(&sc->mlx_bufq, bp);
    sc->mlx_waitbufs++;
    mlx_startio(sc);
    return(0);
}

/********************************************************************************
 * Accept an open operation on the control device.
 */
int
mlx_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct mlx_softc	*sc = devclass_get_softc(mlx_devclass, unit);

    sc->mlx_state |= MLX_STATE_OPEN;
    return(0);
}

/********************************************************************************
 * Accept the last close on the control device.
 */
int
mlx_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct mlx_softc	*sc = devclass_get_softc(mlx_devclass, unit);

    sc->mlx_state &= ~MLX_STATE_OPEN;
    return (0);
}

/********************************************************************************
 * Handle controller-specific control operations.
 */
int
mlx_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    int			unit = minor(dev);
    struct mlx_softc	*sc = devclass_get_softc(mlx_devclass, unit);
    int			*arg = (int *)addr;
    struct mlx_pause	*mp;
    struct mlx_sysdrive	*dr;
    struct mlxd_softc	*mlxd;
    int			i, error;
    
    switch(cmd) {
	/*
	 * Enumerate connected system drives; returns the first system drive's
	 * unit number if *arg is -1, or the next unit after *arg if it's
	 * a valid unit on this controller.
	 */
    case MLX_NEXT_CHILD:
	/* search system drives */
	for (i = 0; i < MLX_MAXDRIVES; i++) {
	    /* is this one attached? */
	    if (sc->mlx_sysdrive[i].ms_disk != 0) {
		/* looking for the next one we come across? */
		if (*arg == -1) {
		    *arg = device_get_unit(sc->mlx_sysdrive[i].ms_disk);
		    return(0);
		}
		/* we want the one after this one */
		if (*arg == device_get_unit(sc->mlx_sysdrive[i].ms_disk))
		    *arg = -1;
	    }
	}
	return(ENOENT);

	/*
	 * Scan the controller to see whether new drives have appeared.
	 */
    case MLX_RESCAN_DRIVES:
	mlx_startup(sc);
	return(0);

	/*
	 * Disconnect from the specified drive; it may be about to go 
	 * away.
	 */
    case MLX_DETACH_DRIVE:			/* detach one drive */
	
	if (((dr = mlx_findunit(sc, *arg)) == NULL) || 
	    ((mlxd = device_get_softc(dr->ms_disk)) == NULL))
	    return(ENOENT);

	device_printf(dr->ms_disk, "detaching...");
	error = 0;
	if (mlxd->mlxd_flags & MLXD_OPEN) {
	    error = EBUSY;
	    goto detach_out;
	}
	
	/* flush controller */
	if (mlx_flush(sc)) {
	    error = EBUSY;
	    goto detach_out;
	}

	/* nuke drive */
	if ((error = device_delete_child(sc->mlx_dev, dr->ms_disk)) != 0)
	    goto detach_out;
	dr->ms_disk = 0;
	bus_generic_detach(sc->mlx_dev);

    detach_out:
	if (error) {
	    printf("failed\n");
	} else {
	    printf("done\n");
	}
	return(error);

	/*
	 * Pause one or more SCSI channels for a period of time, to assist
	 * in the process of hot-swapping devices.
	 *
	 * Note that at least the 3.51 firmware on the DAC960PL doesn't seem
	 * to do this right.
	 */
    case MLX_PAUSE_CHANNEL:			/* schedule a channel pause */
	/* Does this command work on this firmware? */
	if (!(sc->mlx_feature & MLX_FEAT_PAUSEWORKS))
	    return(EOPNOTSUPP);

	mp = (struct mlx_pause *)addr;
	if ((mp->mp_which == MLX_PAUSE_CANCEL) && (sc->mlx_pause.mp_when != 0)) {
	    /* cancel a pending pause operation */
	    sc->mlx_pause.mp_which = 0;
	} else {
	    /* fix for legal channels */
	    mp->mp_which &= ((1 << sc->mlx_nchan) -1);
	    /* check time values */
	    if ((mp->mp_when < 0) || (mp->mp_when > 3600))
		return(EINVAL);
	    if ((mp->mp_howlong < 1) || (mp->mp_howlong > (0xf * 30)))
		return(EINVAL);
	    
	    /* check for a pause currently running */
	    if ((sc->mlx_pause.mp_which != 0) && (sc->mlx_pause.mp_when == 0))
		return(EBUSY);

	    /* looks ok, go with it */
	    sc->mlx_pause.mp_which = mp->mp_which;
	    sc->mlx_pause.mp_when = time_second + mp->mp_when;
	    sc->mlx_pause.mp_howlong = sc->mlx_pause.mp_when + mp->mp_howlong;
	}
	return(0);

	/*
	 * Accept a command passthrough-style.
	 */
    case MLX_COMMAND:
	return(mlx_user_command(sc, (struct mlx_usercommand *)addr));

    default:	
	return(ENOTTY);
    }
}

/********************************************************************************
 * Handle operations requested by a System Drive connected to this controller.
 */
int
mlx_submit_ioctl(struct mlx_softc *sc, struct mlx_sysdrive *drive, u_long cmd, 
		caddr_t addr, int32_t flag, struct proc *p)
{
    struct mlxd_rebuild		*mr = (struct mlxd_rebuild *)addr;
    struct mlxd_rebuild_status	*mp = (struct mlxd_rebuild_status *)addr;
    int				*arg = (int *)addr;
    int				error;

    switch(cmd) {
	/*
	 * Return the current status of this drive.
	 */
    case MLXD_STATUS:
	*arg = drive->ms_state;
	return(0);
	
	/*
	 * Start a background rebuild on this drive.
	 */
    case MLXD_REBUILDASYNC:
	/* XXX lock? */
	if (sc->mlx_rebuild >= 0)
	    return(EBUSY);
	sc->mlx_rebuild = drive - &sc->mlx_sysdrive[0];
	    
	switch (mlx_rebuild(sc, mr->rb_channel, mr->rb_target)) {
	case 0:
	    drive->ms_state = MLX_SYSD_REBUILD;
	    error = 0;
	    break;
	case 0x10000:
	    error = ENOMEM;		/* couldn't set up the command */
	    break;
	case 0x0002:
	case 0x0106:
	    error = EBUSY;
	    break;
	case 0x0004:
	    error = EIO;
	    break;
	case 0x0105:
	    error = ERANGE;
	    break;
	default:
	    error = EINVAL;
	    break;
	}
	if (error != 0)
	    sc->mlx_rebuild = -1;
	return(error);

	/*
	 * Start a background consistency check on this drive.
	 */
    case MLXD_CHECKASYNC:		/* start a background consistency check */
	/* XXX implement */
	break;

	/*
	 * Get the status of the current rebuild or consistency check.
	 */
    case MLXD_REBUILDSTAT:

	if (sc->mlx_rebuild >= 0) {	/* may be a second or so out of date */
	    mp->rs_drive = sc->mlx_rebuild;
	    mp->rs_size = sc->mlx_sysdrive[sc->mlx_rebuild].ms_size;
	    mp->rs_remaining = sc->mlx_rebuildstat;
	    return(0);
	} else if (sc->mlx_check >= 0) {
	    /* XXX implement */
	} else {
	    /* XXX should return status of last completed operation? */
	    return(EINVAL);
	}

    }
    return(ENOIOCTL);
}


/********************************************************************************
 ********************************************************************************
                                                                Status Monitoring
 ********************************************************************************
 ********************************************************************************/

#define MLX_PERIODIC_ISBUSY(sc)	(sc->mlx_polling <= 0)
#define MLX_PERIODIC_BUSY(sc)	atomic_add_int(&sc->mlx_polling, 1);
#define MLX_PERIODIC_UNBUSY(sc) atomic_subtract_int(&sc->mlx_polling, 1);

/********************************************************************************
 * Fire off commands to periodically check the status of connected drives.
 */
static void
mlx_periodic(void *data)
{
    struct mlx_softc *sc = (struct mlx_softc *)data;

    debug("called");

    /*
     * Run a bus pause? 
     */
    if ((sc->mlx_pause.mp_which != 0) &&
	(sc->mlx_pause.mp_when > 0) &&
	(time_second >= sc->mlx_pause.mp_when)){

	mlx_pause_action(sc);		/* pause is running */
	sc->mlx_pause.mp_when = 0;
	sysbeep(500, hz);

	/* 
	 * Bus pause still running?
	 */
    } else if ((sc->mlx_pause.mp_which != 0) &&
	       (sc->mlx_pause.mp_when == 0)) {

	/* time to stop bus pause? */
	if (time_second >= sc->mlx_pause.mp_howlong) {
	    mlx_pause_action(sc);
	    sc->mlx_pause.mp_which = 0;	/* pause is complete */
	    sysbeep(500, hz);
	} else {
	    sysbeep((time_second % 5) * 100 + 500, hz/8);
	}

	/* 
	 * Run normal periodic activities? 
	 */
    } else if (MLX_PERIODIC_ISBUSY(sc)) {

	/* time to perform a periodic status poll? XXX tuneable interval? */
	if (time_second > (sc->mlx_lastpoll + 10)) {
	    sc->mlx_lastpoll = time_second;

	    /* for caution's sake */
	    if (sc->mlx_polling < 0) {
		device_printf(sc->mlx_dev, "mlx_polling < 0\n");
		atomic_set_int(&sc->mlx_polling, 0);
	    }

	    /* 
	     * Check controller status.
	     */
	    MLX_PERIODIC_BUSY(sc);
	    mlx_enquire(sc, MLX_CMD_ENQUIRY, sizeof(struct mlx_enquiry), mlx_periodic_enquiry);

	    /*
	     * Check system drive status.
	     *
	     * XXX This might be better left to event-driven detection, eg. I/O to an offline
	     *     drive will detect it's offline, rebuilds etc. should detect the drive is back
	     *     online.
	     */
	    MLX_PERIODIC_BUSY(sc);
	    mlx_enquire(sc, MLX_CMD_ENQSYSDRIVE, sizeof(struct mlx_enq_sys_drive) * MLX_MAXDRIVES, 
			mlx_periodic_enquiry);
	}

	/* 
	 * Get drive rebuild/check status
	 */
	if (sc->mlx_rebuild >= 0) {
	    MLX_PERIODIC_BUSY(sc);
	    mlx_enquire(sc, MLX_CMD_REBUILDSTAT, sizeof(struct mlx_rebuild_stat), mlx_periodic_rebuild);
	}
    } else {
	/* 
	 * If things are still running from the last poll, complain about it.
	 *
	 * XXX If this becomes an issue, we should have some way of telling what
	 *     has become stuck.
	 */
	device_printf(sc->mlx_dev, "poll still busy (%d)\n", sc->mlx_polling);
    }

    /* XXX check for wedged/timed out commands? */

    /* reschedule another poll next second or so */
    sc->mlx_timeout = timeout(mlx_periodic, sc, hz);
}

/********************************************************************************
 * Handle the result of an ENQUIRY command instigated by periodic status polling.
 */
static void
mlx_periodic_enquiry(struct mlx_command *mc)
{
    struct mlx_softc		*sc = mc->mc_sc;

    debug("called");

    /* Command completed OK? */
    if (mc->mc_status != 0) {
	device_printf(sc->mlx_dev, "periodic enquiry failed\n");
	goto out;
    }

    /* respond to command */
    switch(mc->mc_mailbox[0]) {
	/*
	 * Generic controller status update.  We could do more with this than just
	 * checking the event log.
	 */
    case MLX_CMD_ENQUIRY:
    {
	struct mlx_enquiry		*me = (struct mlx_enquiry *)mc->mc_data;
	
	/* New stuff in the event log? */
	if (me->me_event_log_seq_num != sc->mlx_lastevent) {
	    /* record where current events are up to */
	    sc->mlx_currevent = me->me_event_log_seq_num;
	    device_printf(sc->mlx_dev, "event log pointer was %d, now %d\n", 
			  sc->mlx_lastevent, sc->mlx_currevent);

	    /* start poll of event log */
	    mlx_periodic_eventlog_poll(sc);
	}
	break;
    }
    case MLX_CMD_ENQSYSDRIVE:
    {
	struct mlx_enq_sys_drive	*mes = (struct mlx_enq_sys_drive *)mc->mc_data;
	struct mlx_sysdrive		*dr;
	int				i;
	
	for (i = 0, dr = &sc->mlx_sysdrive[0]; 
	     (i < MLX_MAXDRIVES) && (mes[i].sd_size != 0xffffffff); 
	     i++) {

	    /* if disk is being rebuilt, we should not check it */
	    if (dr->ms_state == MLX_SYSD_REBUILD) {
		/* has state been changed by controller? */
		if (dr->ms_state != mes[i].sd_state) {
		    switch(mes[i].sd_state) {
		    case MLX_SYSD_OFFLINE:
			device_printf(dr->ms_disk, "drive offline\n");
			break;
		    case MLX_SYSD_ONLINE:
			device_printf(dr->ms_disk, "drive online\n");
			break;
		    case MLX_SYSD_CRITICAL:
			device_printf(dr->ms_disk, "drive critical\n");
			break;
		    }
		    /* save new state */
		    dr->ms_state = mes[i].sd_state;
		}
	    }
	}
	break;
    }
    default:
	break;
    }

 out:
    free(mc->mc_data, M_DEVBUF);
    mlx_releasecmd(mc);
    /* this event is done */
    MLX_PERIODIC_UNBUSY(sc);
}

/********************************************************************************
 * Instigate a poll for one event log message on (sc).
 * We only poll for one message at a time, to keep our command usage down.
 */
static void
mlx_periodic_eventlog_poll(struct mlx_softc *sc)
{
    struct mlx_command	*mc;
    void		*result = NULL;
    int			error;

    debug("called");

    /* presume we are going to create another event */
    MLX_PERIODIC_BUSY(sc);

    /* get ourselves a command buffer */
    error = 1;
    if ((mc = mlx_alloccmd(sc)) == NULL)
	goto out;
    /* allocate the response structure */
    if ((result = malloc(/*sizeof(struct mlx_eventlog_entry)*/1024, M_DEVBUF, M_NOWAIT)) == NULL)
	goto out;
    /* get a command slot */
    if (mlx_getslot(mc))
	goto out;

    /* map the command so the controller can see it */
    mc->mc_data = result;
    mc->mc_length = /*sizeof(struct mlx_eventlog_entry)*/1024;
    mlx_mapcmd(mc);

    /* build the command to get one entry */
    mlx_make_type3(mc, MLX_CMD_LOGOP, MLX_LOGOP_GET, 1, sc->mlx_lastevent, 0, 0, mc->mc_dataphys, 0);
    mc->mc_complete = mlx_periodic_eventlog_respond;
    mc->mc_private = mc;

    /* start the command */
    if ((error = mlx_start(mc)) != 0)
	goto out;
    
    error = 0;			/* success */
 out:
    if (error != 0) {
	if (mc != NULL)
	    mlx_releasecmd(mc);
	if (result != NULL)
	    free(result, M_DEVBUF);
	/* abort this event */
	MLX_PERIODIC_UNBUSY(sc);
    }
}

/********************************************************************************
 * Handle the result of polling for a log message, generate diagnostic output.
 * If this wasn't the last message waiting for us, we'll go collect another.
 */
static char *mlx_sense_messages[] = {
    "because write recovery failed",
    "because of SCSI bus reset failure",
    "because of double check condition",
    "because it was removed",
    "because of gross error on SCSI chip",
    "because of bad tag returned from drive",
    "because of timeout on SCSI command",
    "because of reset SCSI command issued from system",
    "because busy or parity error count exceeded limit",
    "because of 'kill drive' command from system",
    "because of selection timeout",
    "due to SCSI phase sequence error",
    "due to unknown status"
};

static void
mlx_periodic_eventlog_respond(struct mlx_command *mc)
{
    struct mlx_softc		*sc = mc->mc_sc;
    struct mlx_eventlog_entry	*el = (struct mlx_eventlog_entry *)mc->mc_data;
    char			*reason;

    debug("called");

    if (mc->mc_status == 0) {
	sc->mlx_lastevent++;		/* got the message OK */

	/* handle event log message */
	switch(el->el_type) {
	    /*
	     * This is the only sort of message we understand at the moment.
	     * The tests here are probably incomplete.
	     */
	case MLX_LOGMSG_SENSE:	/* sense data */
	    /* Mylex vendor-specific message indicating a drive was killed? */
	    if ((el->el_sensekey == 9) &&
		(el->el_asc == 0x80)) {
		if (el->el_asq < (sizeof(mlx_sense_messages) / sizeof(mlx_sense_messages[0]))) {
		    reason = mlx_sense_messages[el->el_asq];
		} else {
		    reason = "for unknown reason";
		}
		device_printf(sc->mlx_dev, "physical drive %d:%d killed %s\n",
			      el->el_channel, el->el_target, reason);
	    }
	    /* SCSI drive was reset? */
	    if ((el->el_sensekey == 6) && (el->el_asc == 0x29)) {
		device_printf(sc->mlx_dev, "physical drive %d:%d reset\n", 
			      el->el_channel, el->el_target);
	    }
	    /* SCSI drive error? */
	    if (!((el->el_sensekey == 0) ||
		  ((el->el_sensekey == 2) &&
		   (el->el_asc == 0x04) &&
		   ((el->el_asq == 0x01) ||
		    (el->el_asq == 0x02))))) {
		device_printf(sc->mlx_dev, "physical drive %d:%d error log: sense = %d asc = %x asq = %x\n",
			      el->el_channel, el->el_target, el->el_sensekey, el->el_asc, el->el_asq);
		device_printf(sc->mlx_dev, "  info %4D csi %4D\n", el->el_information, ":", el->el_csi, ":");
	    }
	    break;
	    
	default:
	    device_printf(sc->mlx_dev, "unknown log message type 0x%x\n", el->el_type);
	    break;
	}
    } else {
	device_printf(sc->mlx_dev, "error reading message log - %s\n", mlx_diagnose_command(mc));
    }
	
    /* dispose of command and data */
    free(mc->mc_data, M_DEVBUF);
    mlx_releasecmd(mc);

    /* is there another message to obtain? */
    if (sc->mlx_lastevent != sc->mlx_currevent)
	mlx_periodic_eventlog_poll(sc);

    /* this event is done */
    MLX_PERIODIC_UNBUSY(sc);
}

/********************************************************************************
 * Handle the completion of a rebuild operation.
 */
static void
mlx_periodic_rebuild(struct mlx_command *mc)
{
    struct mlx_softc		*sc = mc->mc_sc;
    struct mlx_rebuild_stat	*mr = (struct mlx_rebuild_stat *)mc->mc_private;

    switch(mc->mc_status) {
    case 0:				/* all OK, rebuild still running */
	sc->mlx_rebuildstat = mr->rb_remaining;
	break;

    case 0x0105:			/* rebuild/check finished */
	if (sc->mlx_rebuild >= 0) {
	    device_printf(sc->mlx_sysdrive[sc->mlx_rebuild].ms_disk, "rebuild completed\n");
	    sc->mlx_rebuild = -1;
	} else if (sc->mlx_check >= 0) {
	    device_printf(sc->mlx_sysdrive[sc->mlx_check].ms_disk, "consistency check completed\n");
	    sc->mlx_check = -1;
	} else {
	    device_printf(sc->mlx_dev, "consistency check completed\n");
	}
	break;
    }
    free(mc->mc_data, M_DEVBUF);
    mlx_releasecmd(mc);
    /* this event is done */
    MLX_PERIODIC_UNBUSY(sc);
}

/********************************************************************************
 ********************************************************************************
                                                                    Channel Pause
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * It's time to perform a channel pause action for (sc), either start or stop
 * the pause.
 */
static void
mlx_pause_action(struct mlx_softc *sc)
{
    struct mlx_command	*mc;
    int			failsafe, i, command;

    /* What are we doing here? */
    if (sc->mlx_pause.mp_when == 0) {
	command = MLX_CMD_STARTCHANNEL;
	failsafe = 0;

    } else {
	command = MLX_CMD_STOPCHANNEL;

	/* 
	 * Channels will always start again after the failsafe period, 
	 * which is specified in multiples of 30 seconds.
	 * This constrains us to a maximum pause of 450 seconds.
	 */
	failsafe = ((sc->mlx_pause.mp_howlong - time_second) + 5) / 30;
	if (failsafe > 0xf) {
	    failsafe = 0xf;
	    sc->mlx_pause.mp_howlong = time_second + (0xf * 30) - 5;
	}
    }

    /* build commands for every channel requested */
    for (i = 0; i < sc->mlx_nchan; i++) {
	if ((1 << i) & sc->mlx_pause.mp_which) {

	    /* get ourselves a command buffer */
	    if ((mc = mlx_alloccmd(sc)) == NULL)
		goto fail;
	    /* get a command slot */
	    mc->mc_flags |= MLX_CMD_PRIORITY;
	    if (mlx_getslot(mc))
		goto fail;

	    /* build the command */
	    mlx_make_type2(mc, command, (failsafe << 4) | i, 0, 0, 0, 0, 0, 0, 0);
	    mc->mc_complete = mlx_pause_done;
	    mc->mc_private = sc;		/* XXX not needed */
	    if (mlx_start(mc))
		goto fail;
	    /* command submitted OK */
	    return;
    
	fail:
	    device_printf(sc->mlx_dev, "%s failed for channel %d\n", 
			  command == MLX_CMD_STOPCHANNEL ? "pause" : "resume", i);
	    if (mc != NULL)
		mlx_releasecmd(mc);
	}
    }
}

static void
mlx_pause_done(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;
    int			command = mc->mc_mailbox[0];
    int			channel = mc->mc_mailbox[2] & 0xf;
    
    if (mc->mc_status != 0) {
	device_printf(sc->mlx_dev, "%s command failed - %s\n", 
		      command == MLX_CMD_STOPCHANNEL ? "pause" : "resume", mlx_diagnose_command(mc));
    } else if (command == MLX_CMD_STOPCHANNEL) {
	device_printf(sc->mlx_dev, "channel %d pausing for %ld seconds\n", 
		      channel, sc->mlx_pause.mp_howlong - time_second);
    } else {
	device_printf(sc->mlx_dev, "channel %d resuming\n", channel);
    }
    mlx_releasecmd(mc);
}

/********************************************************************************
 ********************************************************************************
                                                               Command Submission
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Perform an Enquiry command using a type-3 command buffer and a return a single
 * linear result buffer.  If the completion function is specified, it will
 * be called with the completed command (and the result response will not be
 * valid until that point).  Otherwise, the command will either be busy-waited
 * for (interrupts not enabled), or slept for.
 */
static void *
mlx_enquire(struct mlx_softc *sc, int command, size_t bufsize, void (* complete)(struct mlx_command *mc))
{
    struct mlx_command	*mc;
    void		*result;
    int			error;

    debug("called");

    /* get ourselves a command buffer */
    error = 1;
    result = NULL;
    if ((mc = mlx_alloccmd(sc)) == NULL)
	goto out;
    /* allocate the response structure */
    if ((result = malloc(bufsize, M_DEVBUF, M_NOWAIT)) == NULL)
	goto out;
    /* get a command slot */
    mc->mc_flags |= MLX_CMD_PRIORITY | MLX_CMD_DATAOUT;
    if (mlx_getslot(mc))
	goto out;

    /* map the command so the controller can see it */
    mc->mc_data = result;
    mc->mc_length = bufsize;
    mlx_mapcmd(mc);

    /* build an enquiry command */
    mlx_make_type2(mc, command, 0, 0, 0, 0, 0, 0, mc->mc_dataphys, 0);

    /* do we want a completion callback? */
    if (complete != NULL) {
	mc->mc_complete = complete;
	mc->mc_private = mc;
	if ((error = mlx_start(mc)) != 0)
	    goto out;
    } else {
	/* run the command in either polled or wait mode */
	if ((sc->mlx_state & MLX_STATE_INTEN) ? mlx_wait_command(mc) : mlx_poll_command(mc))
	    goto out;
    
	/* command completed OK? */
	if (mc->mc_status != 0) {
	    device_printf(sc->mlx_dev, "ENQUIRY failed - %s\n", mlx_diagnose_command(mc));
	    goto out;
	}
    }
    error = 0;			/* success */
 out:
    /* we got a command, but nobody else will free it */
    if ((complete == NULL) && (mc != NULL))
	mlx_releasecmd(mc);
    /* we got an error, and we allocated a result */
    if ((error != 0) && (result != NULL)) {
	free(result, M_DEVBUF);
	result = NULL;
    }
    return(result);
}


/********************************************************************************
 * Perform a Flush command on the nominated controller.
 *
 * May be called with interrupts enabled or disabled; will not return until
 * the flush operation completes or fails.
 */
static int
mlx_flush(struct mlx_softc *sc)
{
    struct mlx_command	*mc;
    int			error;

    debug("called");

    /* get ourselves a command buffer */
    error = 1;
    if ((mc = mlx_alloccmd(sc)) == NULL)
	goto out;
    /* get a command slot */
    if (mlx_getslot(mc))
	goto out;

    /* build a flush command */
    mlx_make_type2(mc, MLX_CMD_FLUSH, 0, 0, 0, 0, 0, 0, 0, 0);

    /* run the command in either polled or wait mode */
    if ((sc->mlx_state & MLX_STATE_INTEN) ? mlx_wait_command(mc) : mlx_poll_command(mc))
	goto out;
    
    /* command completed OK? */
    if (mc->mc_status != 0) {
	device_printf(sc->mlx_dev, "FLUSH failed - %s\n", mlx_diagnose_command(mc));
	goto out;
    }
    
    error = 0;			/* success */
 out:
    if (mc != NULL)
	mlx_releasecmd(mc);
    return(error);
}

/********************************************************************************
 * Start a background rebuild on the nominated controller/channel/target.
 *
 * May be called with interrupts enabled or disabled; will return as soon as the
 * operation has started or been refused.
 */
static int
mlx_rebuild(struct mlx_softc *sc, int channel, int target)
{
    struct mlx_command	*mc;
    int			error;

    debug("called");

    /* get ourselves a command buffer */
    error = 0x10000;
    if ((mc = mlx_alloccmd(sc)) == NULL)
	goto out;
    /* get a command slot */
    if (mlx_getslot(mc))
	goto out;

    /* build a rebuild command */
    mlx_make_type2(mc, MLX_CMD_REBUILDASYNC, channel, target, 0, 0, 0, 0, 0, 0);

    /* run the command in either polled or wait mode */
    if ((sc->mlx_state & MLX_STATE_INTEN) ? mlx_wait_command(mc) : mlx_poll_command(mc))
	goto out;
    
    /* command completed OK? */
    if (mc->mc_status != 0) {	
	device_printf(sc->mlx_dev, "REBUILD ASYNC failed - %s\n", mlx_diagnose_command(mc));
    } else {
	device_printf(sc->mlx_sysdrive[sc->mlx_rebuild].ms_disk, "rebuild started");
    }
    error = mc->mc_status;

 out:
    if (mc != NULL)
	mlx_releasecmd(mc);
    return(error);
}

/********************************************************************************
 * Run the command (mc) and return when it completes.
 *
 * Interrupts need to be enabled; returns nonzero on error.
 */
static int
mlx_wait_command(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;
    int			error, count;

    debug("called");

    mc->mc_complete = NULL;
    mc->mc_private = mc;		/* wake us when you're done */
    if ((error = mlx_start(mc)) != 0)
	return(error);

    count = 0;
    /* XXX better timeout? */
    while ((mc->mc_status == MLX_STATUS_BUSY) && (count < 30)) {
	tsleep(mc->mc_private, PRIBIO | PCATCH, "mlxwcmd", hz);
    }

    if (mc->mc_status != 0) {
	device_printf(sc->mlx_dev, "I/O error 0x%x\n", mc->mc_status);
	return(EIO);
    }
    return(0);
}


/********************************************************************************
 * Start the command (mc) and busy-wait for it to complete.
 *
 * Should only be used when interrupts are not available. Returns 0 on 
 * success, nonzero on error.
 * Successfully completed commands are dequeued.
 */
static int
mlx_poll_command(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;
    int			error, count, s;

    debug("called");

    mc->mc_complete = NULL;
    mc->mc_private = NULL;	/* we will poll for it */
    if ((error = mlx_start(mc)) != 0)
	return(error);
    
    count = 0;
    do {
	/* poll for completion */
	mlx_done(mc->mc_sc);
    } while ((mc->mc_status == MLX_STATUS_BUSY) && (count < 10000));
    if (mc->mc_status != MLX_STATUS_BUSY) {
	s = splbio();
	TAILQ_REMOVE(&sc->mlx_donecmd, mc, mc_link);
	splx(s);
	return(0);
    }
    device_printf(sc->mlx_dev, "I/O error 0x%x\n", mc->mc_status);
    return(EIO);
}

/********************************************************************************
 * Pull as much work off the softc's work queue as possible and give it to the
 * controller.  Leave a couple of slots free for emergencies.
 *
 * Must be called at splbio or in an equivalent fashion that prevents 
 * reentry or activity on the bufq..
 */
static void
mlx_startio(struct mlx_softc *sc)
{
    struct mlx_command	*mc;
    struct mlxd_softc	*mlxd;
    struct buf		*bp;
    int			blkcount;
    int			driveno;
    int			cmd;

    /* spin until something prevents us from doing any work */
    for (;;) {

	/* see if there's work to be done */
	if ((bp = bufq_first(&sc->mlx_bufq)) == NULL)
	    break;
	/* get a command */
	if ((mc = mlx_alloccmd(sc)) == NULL)
	    break;
	/* get a slot for the command */
	if (mlx_getslot(mc) != 0) {
	    mlx_releasecmd(mc);
	    break;
	}
	/* get the buf containing our work */
	bufq_remove(&sc->mlx_bufq, bp);
	sc->mlx_waitbufs--;
	
	/* connect the buf to the command */
	mc->mc_complete = mlx_completeio;
	mc->mc_private = bp;
	mc->mc_data = bp->b_data;
	mc->mc_length = bp->b_bcount;
	if (bp->b_flags & B_READ) {
	    mc->mc_flags |= MLX_CMD_DATAIN;
	    cmd = MLX_CMD_READOLDSG;
	} else {
	    mc->mc_flags |= MLX_CMD_DATAOUT;
	    cmd = MLX_CMD_WRITEOLDSG;
	}
	
	/* map the command so the controller can work with it */
	mlx_mapcmd(mc);
	
	/* build a suitable I/O command (assumes 512-byte rounded transfers) */
	mlxd = (struct mlxd_softc *)bp->b_dev->si_drv1;
	driveno = mlxd->mlxd_drive - &sc->mlx_sysdrive[0];
	blkcount = bp->b_bcount / MLX_BLKSIZE;

	if ((bp->b_pblkno + blkcount) > sc->mlx_sysdrive[driveno].ms_size)
	    device_printf(sc->mlx_dev, "I/O beyond end of unit (%u,%d > %u)\n", 
			  bp->b_pblkno, blkcount, sc->mlx_sysdrive[driveno].ms_size);

	/*
	 * Build the I/O command.  Note that the SG list type bits are set to zero,
	 * denoting the format of SG list that we are using.
	 */
	mlx_make_type5(mc, cmd, 
		       blkcount & 0xff, 				/* xfer length low byte */
		       (driveno << 3) | ((blkcount >> 8) & 0x07),	/* target and length high 3 bits */
		       bp->b_pblkno,					/* physical block number */
		       mc->mc_sgphys,					/* location of SG list */
		       mc->mc_nsgent & 0x3f);				/* size of SG list (top 2 bits clear) */
	

	/* try to give command to controller */
	if (mlx_start(mc) != 0) {
	    /* fail the command */
	    mc->mc_status = MLX_STATUS_WEDGED;
	    mlx_completeio(mc);
	}
    }
}

/********************************************************************************
 * Handle completion of an I/O command.
 */
static void
mlx_completeio(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;
    struct buf		*bp = (struct buf *)mc->mc_private;
    struct mlxd_softc	*mlxd = (struct mlxd_softc *)bp->b_dev->si_drv1;
    
    if (mc->mc_status != MLX_STATUS_OK) {	/* could be more verbose here? */
	bp->b_error = EIO;
	bp->b_flags |= B_ERROR;

	switch(mc->mc_status) {
	case MLX_STATUS_RDWROFFLINE:		/* system drive has gone offline */
	    device_printf(mlxd->mlxd_dev, "drive offline\n");
	    /* should signal this with a return code */
	    mlxd->mlxd_drive->ms_state = MLX_SYSD_OFFLINE;
	    break;

	default:				/* other I/O error */
	    device_printf(sc->mlx_dev, "I/O error - %s\n", mlx_diagnose_command(mc));
#if 0
	    device_printf(sc->mlx_dev, "  b_bcount %ld  blkcount %ld  b_pblkno %d\n", 
			  bp->b_bcount, bp->b_bcount / MLX_BLKSIZE, bp->b_pblkno);
	    device_printf(sc->mlx_dev, "  %13D\n", mc->mc_mailbox, " ");
#endif
	    break;
	}
    }
    mlx_releasecmd(mc);
    mlxd_intr(bp);
}

/********************************************************************************
 * Take a command from user-space and try to run it.
 */
static int
mlx_user_command(struct mlx_softc *sc, struct mlx_usercommand *mu)
{
    struct mlx_command	*mc;
    void		*kbuf;
    int			error;
    
    kbuf = NULL;
    mc = NULL;
    error = ENOMEM;
    /* get a kernel buffer for the transfer */
    if (mu->mu_datasize > 0) {
	if ((kbuf = malloc(mu->mu_datasize, M_DEVBUF, M_WAITOK)) == NULL)
	    goto out;
	if ((mu->mu_bufptr < 0) || (mu->mu_bufptr > (sizeof(mu->mu_command) < sizeof(u_int32_t)))) {
	    error = EINVAL;
	    goto out;
	}
    }
    /* get ourselves a command buffer */
    if ((mc = mlx_alloccmd(sc)) == NULL)
	goto out;

    /* copy the command and data */
    bcopy(mu->mu_command, mc->mc_mailbox, sizeof(mc->mc_mailbox));
    if ((mu->mu_datasize > 0) && ((error = copyin(mu->mu_buf, kbuf, mu->mu_datasize))))
	goto out;

    /* get a command slot */
    if (mlx_getslot(mc))
	goto out;

    /* map the command so the controller can see it */
    mc->mc_data = kbuf;
    mc->mc_length = mu->mu_datasize;
    mlx_mapcmd(mc);

    /* if there's a data buffer, fix up the command */
    if (mu->mu_datasize > 0) {
	mc->mc_mailbox[mu->mu_bufptr    ] =  mc->mc_length        & 0xff;
	mc->mc_mailbox[mu->mu_bufptr + 1] = (mc->mc_length >> 8)  & 0xff;
	mc->mc_mailbox[mu->mu_bufptr + 2] = (mc->mc_length >> 16) & 0xff;
	mc->mc_mailbox[mu->mu_bufptr + 3] = (mc->mc_length >> 24) & 0xff;
    }

    /* submit the command and wait */
    if ((error = mlx_wait_command(mc)) != 0)
	goto out;

    /* copy out status and data */
    mu->mu_status = mc->mc_status;
    if ((mu->mu_datasize > 0) && ((error = copyout(kbuf, mu->mu_buf, mu->mu_datasize))))
	goto out;
    error = 0;
    
 out:
    mlx_releasecmd(mc);
    if (kbuf != NULL)
	free(kbuf, M_DEVBUF);
    return(error);
}

/********************************************************************************
 ********************************************************************************
                                                        Command I/O to Controller
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Find a free command slot for (mc).
 *
 * Don't hand out a slot to a normal-priority command unless there are at least
 * 4 slots free for priority commands.
 */
static int
mlx_getslot(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;
    int			s, slot, limit;

    debug("called  mc %p  sc %p", mc, sc);

    /* enforce slot-usage limit */
    limit = (mc->mc_flags & MLX_CMD_PRIORITY) ? sc->mlx_maxiop : sc->mlx_maxiop - 4;
    if (sc->mlx_busycmds > limit)
	return(EBUSY);

    /* 
     * Allocate an outstanding command slot 
     *
     * XXX linear search is slow
     */
    s = splbio();
    for (slot = 0; slot < sc->mlx_maxiop; slot++) {
	debug("try slot %d", slot);
	if (sc->mlx_busycmd[slot] == NULL)
	    break;
    }
    if (slot < sc->mlx_maxiop) {
	sc->mlx_busycmd[slot] = mc;
	sc->mlx_busycmds++;
    }
    splx(s);

    /* out of slots? */
    if (slot >= sc->mlx_maxiop)
	return(EBUSY);

    debug("got slot %d", slot);
    mc->mc_slot = slot;
    return(0);
}

/********************************************************************************
 * Map/unmap (mc)'s data in the controller's addressable space.
 */
static void
mlx_setup_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct mlx_command	*mc = (struct mlx_command *)arg;
    struct mlx_softc	*sc = mc->mc_sc;
    struct mlx_sgentry	*sg;
    int			i;

    debug("called");

    /* get base address of s/g table */
    sg = sc->mlx_sgtable + (mc->mc_slot * MLX_NSEG);

    /* save s/g table information in command */
    mc->mc_nsgent = nsegments;
    mc->mc_sgphys = sc->mlx_sgbusaddr + (mc->mc_slot * MLX_NSEG * sizeof(struct mlx_sgentry));
    mc->mc_dataphys = segs[0].ds_addr;

    /* populate s/g table */
    for (i = 0; i < nsegments; i++, sg++) {
	sg->sg_addr = segs[i].ds_addr;
	sg->sg_count = segs[i].ds_len;
    }
}

static void
mlx_mapcmd(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;

    debug("called");

    /* if the command involves data at all */
    if (mc->mc_data != NULL) {
	
	/* map the data buffer into bus space and build the s/g list */
	bus_dmamap_load(sc->mlx_buffer_dmat, mc->mc_dmamap, mc->mc_data, mc->mc_length, 
			mlx_setup_dmamap, mc, 0);
	if (mc->mc_flags & MLX_CMD_DATAIN)
	    bus_dmamap_sync(sc->mlx_buffer_dmat, mc->mc_dmamap, BUS_DMASYNC_PREREAD);
	if (mc->mc_flags & MLX_CMD_DATAOUT)
	    bus_dmamap_sync(sc->mlx_buffer_dmat, mc->mc_dmamap, BUS_DMASYNC_PREWRITE);
    }
}

static void
mlx_unmapcmd(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;

    debug("called");

    /* if the command involved data at all */
    if (mc->mc_data != NULL) {
	
	if (mc->mc_flags & MLX_CMD_DATAIN)
	    bus_dmamap_sync(sc->mlx_buffer_dmat, mc->mc_dmamap, BUS_DMASYNC_POSTREAD);
	if (mc->mc_flags & MLX_CMD_DATAOUT)
	    bus_dmamap_sync(sc->mlx_buffer_dmat, mc->mc_dmamap, BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->mlx_buffer_dmat, mc->mc_dmamap); 
    }
}

/********************************************************************************
 * Try to deliver (mc) to the controller.  Take care of any completed commands
 * that we encounter while doing so.
 *
 * Can be called at any interrupt level, with or without interrupts enabled.
 */
static int
mlx_start(struct mlx_command *mc)
{
    struct mlx_softc	*sc = mc->mc_sc;
    int			i, s, done, worked;

    debug("called");

    /* save the slot number as ident so we can handle this command when complete */
    mc->mc_mailbox[0x1] = mc->mc_slot;

    /* set impossible status so that a woken sleeper can tell the command is in progress */
    mc->mc_status = MLX_STATUS_BUSY;
    
    /* assume we don't collect any completed commands */
    worked = 0;

    /* spin waiting for the mailbox */
    for (i = 100000, done = 0; (i > 0) && !done; i--) {
	s = splbio();
	done = sc->mlx_tryqueue(sc, mc);
	splx(s);
	/* check for command completion while we're at it */
	if (mlx_done(sc))
	    worked = 1;
    }
    /* check to see if we picked up any completed commands */
    if (worked)
	mlx_complete(sc);

    /* command is enqueued */
    if (done)
	return(0);

    /* 
     * We couldn't get the controller to take the command.  Revoke the slot
     * that the command was given and return it with a bad status.
     */
    sc->mlx_busycmd[mc->mc_slot] = NULL;
    device_printf(sc->mlx_dev, "controller wedged (not taking commands)\n");
    mc->mc_status = MLX_STATUS_WEDGED;
    return(EIO);
}

/********************************************************************************
 * Look at the controller (sc) and see if a command has been completed.
 * If so, move the command buffer to the done queue for later collection
 * and free the slot for immediate reuse.
 *
 * Returns nonzero if anything was added to the done queue.
 */
static int
mlx_done(struct mlx_softc *sc)
{
    struct mlx_command	*mc;
    int			s;
    u_int8_t		slot;
    u_int16_t		status;
    
    debug("called");

    s = splbio();
    mc = NULL;
    slot = 0;

    /* poll for a completed command's identifier and status */
    if (sc->mlx_findcomplete(sc, &slot, &status)) {
	mc = sc->mlx_busycmd[slot];		/* find command */
	if (mc != NULL) {			/* paranoia */
	    if (mc->mc_status == MLX_STATUS_BUSY) {
		mc->mc_status = status;			/* save status */

		/* move completed command to 'done' queue */
		TAILQ_INSERT_TAIL(&sc->mlx_donecmd, mc, mc_link);
	
		/* free slot for reuse */
		sc->mlx_busycmd[slot] = NULL;
		sc->mlx_busycmds--;
	    } else {
		device_printf(sc->mlx_dev, "duplicate done event for slot %d\n", slot);
		mc = NULL;
	    }
	} else {
	    device_printf(sc->mlx_dev, "done event for nonbusy slot %d\n", slot);
	}
    }
    splx(s);

    if (mc != NULL) {
	/* unmap the command's data buffer */
	mlx_unmapcmd(mc);
	return(1);
    }
    return(0);
}

/********************************************************************************
 * Handle completion for all commands on (sc)'s done queue.
 */
static void
mlx_complete(struct mlx_softc *sc) 
{
    struct mlx_command	*mc, *nc;
    int			s, count;
    
    debug("called");

    s = splbio();
    count = 0;
    
    /* scan the list of done commands */
    mc = TAILQ_FIRST(&sc->mlx_donecmd);
    while (mc != NULL) {
	nc = TAILQ_NEXT(mc, mc_link);

	/* XXX this is slightly bogus */
	if (count++ > (sc->mlx_maxiop * 2))
	    panic("mlx_donecmd list corrupt!");

	/*
	 * Does the command have a completion handler?
	 */
	if (mc->mc_complete != NULL) {
	    /* remove from list and give to handler */
	    TAILQ_REMOVE(&sc->mlx_donecmd, mc, mc_link);
	    mc->mc_complete(mc);

	    /* 
	     * Is there a sleeper waiting on this command?
	     */
	} else if (mc->mc_private != NULL) {	/* sleeping caller wants to know about it */

	    /* remove from list and wake up sleeper */
	    TAILQ_REMOVE(&sc->mlx_donecmd, mc, mc_link);
	    wakeup_one(mc->mc_private);

	    /*
	     * Leave the command for a caller that's polling for it.
	     */
	} else {
	}
	mc = nc;
    }
    splx(s);

    /* queue some more work if there is any */
    mlx_startio(sc);
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
static struct mlx_command *
mlx_alloccmd(struct mlx_softc *sc)
{
    struct mlx_command	*mc;
    int			error;
    int			s;

    debug("called");

    s = splbio();
    if ((mc = TAILQ_FIRST(&sc->mlx_freecmds)) != NULL)
	TAILQ_REMOVE(&sc->mlx_freecmds, mc, mc_link);
    splx(s);

    /* allocate a new command buffer? */
    if (mc == NULL) {
	mc = (struct mlx_command *)malloc(sizeof(*mc), M_DEVBUF, M_NOWAIT);
	if (mc != NULL) {
	    bzero(mc, sizeof(*mc));
	    mc->mc_sc = sc;
	    error = bus_dmamap_create(sc->mlx_buffer_dmat, 0, &mc->mc_dmamap);
	    if (error) {
		free(mc, M_DEVBUF);
		return(NULL);
	    }
	}
    }
    return(mc);
}

/********************************************************************************
 * Release a command buffer for recycling.
 *
 * XXX It might be a good idea to limit the number of commands we save for reuse
 *     if it's shown that this list bloats out massively.
 */
static void
mlx_releasecmd(struct mlx_command *mc)
{
    int		s;
    
    debug("called");

    s = splbio();
    TAILQ_INSERT_HEAD(&mc->mc_sc->mlx_freecmds, mc, mc_link);
    splx(s);
}

/********************************************************************************
 * Permanently discard a command buffer.
 */
static void
mlx_freecmd(struct mlx_command *mc) 
{
    struct mlx_softc	*sc = mc->mc_sc;
    
    debug("called");

    bus_dmamap_destroy(sc->mlx_buffer_dmat, mc->mc_dmamap);
    free(mc, M_DEVBUF);
}


/********************************************************************************
 ********************************************************************************
                                                Type 3 interface accessor methods
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Try to give (mc) to the controller.  Returns 1 if successful, 0 on failure
 * (the controller is not ready to take a command).
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v3_tryqueue(struct mlx_softc *sc, struct mlx_command *mc)
{
    int		i;
    
    debug("called");

    /* ready for our command? */
    if (!(MLX_V3_GET_IDBR(sc) & MLX_V3_IDB_FULL)) {
	/* copy mailbox data to window */
	for (i = 0; i < 13; i++)
	    MLX_V3_PUT_MAILBOX(sc, i, mc->mc_mailbox[i]);
	
	/* post command */
	MLX_V3_PUT_IDBR(sc, MLX_V3_IDB_FULL);
	return(1);
    }
    return(0);
}

/********************************************************************************
 * See if a command has been completed, if so acknowledge its completion
 * and recover the slot number and status code.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v3_findcomplete(struct mlx_softc *sc, u_int8_t *slot, u_int16_t *status)
{

    debug("called");

    /* status available? */
    if (MLX_V3_GET_ODBR(sc) & MLX_V3_ODB_SAVAIL) {
	*slot = MLX_V3_GET_STATUS_IDENT(sc);		/* get command identifier */
	*status = MLX_V3_GET_STATUS(sc);		/* get status */

	/* acknowledge completion */
	MLX_V3_PUT_ODBR(sc, MLX_V3_ODB_SAVAIL);
	MLX_V3_PUT_IDBR(sc, MLX_V3_IDB_SACK);
	return(1);
    }
    return(0);
}

/********************************************************************************
 * Enable/disable interrupts as requested. (No acknowledge required)
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static void
mlx_v3_intaction(struct mlx_softc *sc, int action)
{
    debug("called");

    switch(action) {
    case MLX_INTACTION_DISABLE:
	MLX_V3_PUT_IER(sc, 0);
	sc->mlx_state &= ~MLX_STATE_INTEN;
	break;
    case MLX_INTACTION_ENABLE:
	MLX_V3_PUT_IER(sc, 1);
	sc->mlx_state |= MLX_STATE_INTEN;
	break;
    }
}


/********************************************************************************
 ********************************************************************************
                                                Type 4 interface accessor methods
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Try to give (mc) to the controller.  Returns 1 if successful, 0 on failure
 * (the controller is not ready to take a command).
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v4_tryqueue(struct mlx_softc *sc, struct mlx_command *mc)
{
    int		i;
    
    debug("called");

    /* ready for our command? */
    if (!(MLX_V4_GET_IDBR(sc) & MLX_V4_IDB_FULL)) {
	/* copy mailbox data to window */
	for (i = 0; i < 13; i++)
	    MLX_V4_PUT_MAILBOX(sc, i, mc->mc_mailbox[i]);
	
	/* post command */
	MLX_V4_PUT_IDBR(sc, MLX_V4_IDB_HWMBOX_CMD);
	return(1);
    }
    return(0);
}

/********************************************************************************
 * See if a command has been completed, if so acknowledge its completion
 * and recover the slot number and status code.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v4_findcomplete(struct mlx_softc *sc, u_int8_t *slot, u_int16_t *status)
{

    debug("called");

    /* status available? */
    if (MLX_V4_GET_ODBR(sc) & MLX_V4_ODB_HWSAVAIL) {
	*slot = MLX_V4_GET_STATUS_IDENT(sc);		/* get command identifier */
	*status = MLX_V4_GET_STATUS(sc);		/* get status */

	/* acknowledge completion */
	MLX_V4_PUT_ODBR(sc, MLX_V4_ODB_HWMBOX_ACK);
	MLX_V4_PUT_IDBR(sc, MLX_V4_IDB_SACK);
	return(1);
    }
    return(0);
}

/********************************************************************************
 * Enable/disable interrupts as requested.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static void
mlx_v4_intaction(struct mlx_softc *sc, int action)
{
    debug("called");

    switch(action) {
    case MLX_INTACTION_DISABLE:
	MLX_V4_PUT_IER(sc, MLX_V4_IER_MASK | MLX_V4_IER_DISINT);
	sc->mlx_state &= ~MLX_STATE_INTEN;
	break;
    case MLX_INTACTION_ENABLE:
	MLX_V4_PUT_IER(sc, MLX_V4_IER_MASK & ~MLX_V4_IER_DISINT);
	sc->mlx_state |= MLX_STATE_INTEN;
	break;
    }
}


/********************************************************************************
 ********************************************************************************
                                                                        Debugging
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Return a status message describing (mc)
 */
static char *mlx_status_messages[] = {
    "normal completion",			/* 00 */
    "irrecoverable data error",			/* 01 */
    "drive does not exist, or is offline",	/* 02 */
    "attempt to write beyond end of drive",	/* 03 */
    "bad data encountered",			/* 04 */
    "invalid log entry request",		/* 05 */
    "attempt to rebuild online drive",		/* 06 */
    "new disk failed during rebuild",		/* 07 */
    "invalid channel/target",			/* 08 */
    "rebuild/check already in progress",	/* 09 */
    "one or more disks are dead",		/* 10 */
    "invalid or non-redundant drive",		/* 11 */
    "channel is busy",				/* 12 */
    "channel is not stopped",			/* 13 */
};

static struct
{
    int		command;
    u_int16_t	status;
    int		msg;
} mlx_messages[] = {
    {MLX_CMD_READOLDSG,		0x0001,	 1},
    {MLX_CMD_READOLDSG,		0x0002,	 1},
    {MLX_CMD_READOLDSG,		0x0105,	 3},
    {MLX_CMD_READOLDSG,		0x010c,	 4},
    {MLX_CMD_WRITEOLDSG,	0x0001,	 1},
    {MLX_CMD_WRITEOLDSG,	0x0002,	 1},
    {MLX_CMD_WRITEOLDSG,	0x0105,	 3},
    {MLX_CMD_LOGOP,		0x0105,	 5},
    {MLX_CMD_REBUILDASYNC,	0x0002,  6},
    {MLX_CMD_REBUILDASYNC,	0x0004,  7},
    {MLX_CMD_REBUILDASYNC,	0x0105,  8},
    {MLX_CMD_REBUILDASYNC,	0x0106,  9},
    {MLX_CMD_CHECKASYNC,	0x0002, 10},
    {MLX_CMD_CHECKASYNC,	0x0105, 11},
    {MLX_CMD_CHECKASYNC,	0x0106,  9},
    {MLX_CMD_STOPCHANNEL,	0x0106, 12},
    {MLX_CMD_STOPCHANNEL,	0x0105,  8},
    {MLX_CMD_STARTCHANNEL,	0x0005, 13},
    {MLX_CMD_STARTCHANNEL,	0x0105,  8},
    {-1, 0, 0}
};

static char *
mlx_diagnose_command(struct mlx_command *mc)
{
    static char	unkmsg[80];
    int		i;
    
    /* look up message in table */
    for (i = 0; mlx_messages[i].command != -1; i++)
	if ((mc->mc_mailbox[0] == mlx_messages[i].command) &&
	    (mc->mc_status == mlx_messages[i].status))
	    return(mlx_status_messages[mlx_messages[i].msg]);
	
    sprintf(unkmsg, "unknown response 0x%x for command 0x%x", (int)mc->mc_status, (int)mc->mc_mailbox[0]);
    return(unkmsg);
}

/*******************************************************************************
 * Return a string describing the controller (hwid)
 */
static char *
mlx_name_controller(u_int32_t hwid) 
{
    static char		buf[80];
    char		smbuf[16];
    char		*submodel;
    int			nchn;
    
    switch(hwid & 0xff) {
    case 0x01:
	submodel = "P/PD";
	break;
    case 0x02:
	submodel = "PL";
	break;
    case 0x10:
	submodel = "PG";
	break;
    case 0x11:
	submodel = "PJ";
	break;
    default:
	sprintf(smbuf, " model 0x%x", hwid & 0xff);
	submodel = smbuf;
	break;
    }
    nchn = (hwid >> 8) & 0xff;
    sprintf(buf, "DAC960%s, %d channel%s", submodel, nchn, nchn > 1 ? "s" : "");
    return(buf);
}

/********************************************************************************
 ********************************************************************************
                                                                Utility Functions
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Find the disk whose unit number is (unit) on this controller
 */
static struct mlx_sysdrive *
mlx_findunit(struct mlx_softc *sc, int unit)
{
    int		i;
    
    /* search system drives */
    for (i = 0; i < MLX_MAXDRIVES; i++) {
	/* is this one attached? */
	if (sc->mlx_sysdrive[i].ms_disk != 0) {
	    /* is this the one? */
	    if (unit == device_get_unit(sc->mlx_sysdrive[i].ms_disk))
		return(&sc->mlx_sysdrive[i]);
	}
    }
    return(NULL);
}
