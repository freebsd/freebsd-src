/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * Driver for the Adaptec 'FSA' family of PCI/SCSI RAID adapters.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <dev/aac/aac_compat.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/file.h>
#include <sys/signalvar.h>

#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>

#include <dev/aac/aacreg.h>
#include <dev/aac/aacvar.h>
#include <dev/aac/aac_tables.h>
#include <dev/aac/aac_ioctl.h>

devclass_t	aac_devclass;

static void	aac_startup(void *arg);

/* Command Processing */
static void	aac_startio(struct aac_softc *sc);
static int	aac_start(struct aac_command *cm);
static void	aac_complete(void *context, int pending);
static int	aac_bio_command(struct aac_softc *sc, struct aac_command **cmp);
static void	aac_bio_complete(struct aac_command *cm);
static int	aac_wait_command(struct aac_command *cm, int timeout);
static void	aac_host_command(struct aac_softc *sc);
static void	aac_host_response(struct aac_softc *sc);

/* Command Buffer Management */
static int	aac_alloc_command(struct aac_softc *sc, struct aac_command **cmp);
static void	aac_release_command(struct aac_command *cm);
static void	aac_map_command_cluster(void *arg, bus_dma_segment_t *segs, int nseg, int error);
static void	aac_alloc_command_cluster(struct aac_softc *sc);
static void	aac_free_command_cluster(struct aac_command_cluster *cmc);
static void	aac_map_command(struct aac_command *cm);
static void	aac_unmap_command(struct aac_command *cm);

/* Hardware Interface */
static void	aac_common_map(void *arg, bus_dma_segment_t *segs, int nseg, int error);
static int	aac_init(struct aac_softc *sc);
static int	aac_sync_command(struct aac_softc *sc, u_int32_t command,
				 u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3,
				 u_int32_t *sp);
static int	aac_sync_fib(struct aac_softc *sc, u_int32_t command, u_int32_t xferstate,
			     void *data, u_int16_t datasize,
			     void *result, u_int16_t *resultsize);
static int	aac_enqueue_fib(struct aac_softc *sc, int queue, u_int32_t fib_size, u_int32_t fib_addr);
static int	aac_dequeue_fib(struct aac_softc *sc, int queue, u_int32_t *fib_size, struct aac_fib **fib_addr);

/* StrongARM interface */
static int	aac_sa_get_fwstatus(struct aac_softc *sc);
static void	aac_sa_qnotify(struct aac_softc *sc, int qbit);
static int	aac_sa_get_istatus(struct aac_softc *sc);
static void	aac_sa_clear_istatus(struct aac_softc *sc, int mask);
static void	aac_sa_set_mailbox(struct aac_softc *sc, u_int32_t command,
				   u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3);
static int	aac_sa_get_mailboxstatus(struct aac_softc *sc);
static void	aac_sa_set_interrupts(struct aac_softc *sc, int enable);

struct aac_interface aac_sa_interface = {
    aac_sa_get_fwstatus,
    aac_sa_qnotify,
    aac_sa_get_istatus,
    aac_sa_clear_istatus,
    aac_sa_set_mailbox,
    aac_sa_get_mailboxstatus,
    aac_sa_set_interrupts
};

/* i960Rx interface */    
static int	aac_rx_get_fwstatus(struct aac_softc *sc);
static void	aac_rx_qnotify(struct aac_softc *sc, int qbit);
static int	aac_rx_get_istatus(struct aac_softc *sc);
static void	aac_rx_clear_istatus(struct aac_softc *sc, int mask);
static void	aac_rx_set_mailbox(struct aac_softc *sc, u_int32_t command,
				   u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3);
static int	aac_rx_get_mailboxstatus(struct aac_softc *sc);
static void	aac_rx_set_interrupts(struct aac_softc *sc, int enable);

struct aac_interface aac_rx_interface = {
    aac_rx_get_fwstatus,
    aac_rx_qnotify,
    aac_rx_get_istatus,
    aac_rx_clear_istatus,
    aac_rx_set_mailbox,
    aac_rx_get_mailboxstatus,
    aac_rx_set_interrupts
};

/* Debugging and Diagnostics */
static void	aac_describe_controller(struct aac_softc *sc);
static char	*aac_describe_code(struct aac_code_lookup *table, u_int32_t code);

/* Management Interface */
static d_open_t		aac_open;
static d_close_t	aac_close;
static d_ioctl_t	aac_ioctl;
static int		aac_ioctl_sendfib(struct aac_softc *sc, caddr_t ufib);
static void		aac_handle_aif(struct aac_softc *sc, struct aac_aif_command *aif);
static int		aac_return_aif(struct aac_softc *sc, caddr_t uptr);
#ifdef AAC_COMPAT_LINUX
static int		aac_linux_rev_check(struct aac_softc *sc, caddr_t udata);
static int		aac_linux_getnext_aif(struct aac_softc *sc, caddr_t arg);
#endif

#define AAC_CDEV_MAJOR	150

static struct cdevsw aac_cdevsw = {
    aac_open,		/* open */
    aac_close,		/* close */
    noread,		/* read */
    nowrite,		/* write */
    aac_ioctl,		/* ioctl */
    nopoll,		/* poll */
    nommap,		/* mmap */
    nostrategy,		/* strategy */
    "aac",		/* name */
    AAC_CDEV_MAJOR,	/* major */
    nodump,		/* dump */
    nopsize,		/* psize */
    0,			/* flags */
    -1,			/* bmaj */
};

/********************************************************************************
 ********************************************************************************
                                                                 Device Interface
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Initialise the controller and softc
 */
int
aac_attach(struct aac_softc *sc)
{
    int		error, unit;

    debug_called(1);

    /*
     * Initialise per-controller queues.
     */
    TAILQ_INIT(&sc->aac_freecmds);
    TAILQ_INIT(&sc->aac_ready);
    TAILQ_INIT(&sc->aac_completed);    
    TAILQ_INIT(&sc->aac_clusters);
    bioq_init(&sc->aac_bioq);

#if __FreeBSD_version >= 500005
    /*
     * Initialise command-completion task.
     */
    TASK_INIT(&sc->aac_task_complete, 0, aac_complete, sc);
#endif

    /* disable interrupts before we enable anything */
    AAC_MASK_INTERRUPTS(sc);

    /* mark controller as suspended until we get ourselves organised */
    sc->aac_state |= AAC_STATE_SUSPEND;

    /*
     * Initialise the adapter.
     */
    if ((error = aac_init(sc)))
	return(error);

    /* 
     * Print a little information about the controller.
     */
    aac_describe_controller(sc);

    /*
     * Register to probe our containers later.
     */
    bzero(&sc->aac_ich, sizeof(struct intr_config_hook));
    sc->aac_ich.ich_func = aac_startup;
    sc->aac_ich.ich_arg = sc;
    if (config_intrhook_establish(&sc->aac_ich) != 0) {
        device_printf(sc->aac_dev, "can't establish configuration hook\n");
        return(ENXIO);
    }

    /*
     * Make the control device.
     */
    unit = device_get_unit(sc->aac_dev);
    sc->aac_dev_t = make_dev(&aac_cdevsw, unit, UID_ROOT, GID_WHEEL, 0644, "aac%d", unit);
    sc->aac_dev_t->si_drv1 = sc;

    return(0);
}

/********************************************************************************
 * Probe for containers, create disks.
 */
static void
aac_startup(void *arg)
{
    struct aac_softc		*sc = (struct aac_softc *)arg;
    struct aac_mntinfo		mi;
    struct aac_mntinforesponse	mir;
    device_t			child;
    u_int16_t			rsize;	
    int				i;

    debug_called(1);

    /* disconnect ourselves from the intrhook chain */
    config_intrhook_disestablish(&sc->aac_ich);

    /* loop over possible containers */
    mi.Command = VM_NameServe;
    mi.MntType = FT_FILESYS;
    for (i = 0; i < AAC_MAX_CONTAINERS; i++) {
	/* request information on this container */
	mi.MntCount = i;
	if (aac_sync_fib(sc, ContainerCommand, 0, &mi, sizeof(struct aac_mntinfo), &mir, &rsize)) {
	    debug(2, "error probing container %d", i);
	    continue;
	}
	/* check response size */
	if (rsize != sizeof(mir)) {
	    debug(2, "container info response wrong size (%d should be %d)", rsize, sizeof(*mir));
	    continue;
	}
	/* 
	 * Check container volume type for validity.  Note that many of the possible types
	 * may never show up.
	 */
	if ((mir.Status == ST_OK) && (mir.MntTable[0].VolType != CT_NONE)) {
	    debug(1, "%d: id %x  name '%.16s'  size %u  type %d", 
		  i, mir.MntTable[0].ObjectId,
		  mir.MntTable[0].FileSystemName, mir.MntTable[0].Capacity,
		  mir.MntTable[0].VolType);

	    if ((child = device_add_child(sc->aac_dev, NULL, -1)) == NULL) {
		device_printf(sc->aac_dev, "device_add_child failed\n");
	    } else {
		device_set_ivars(child, &sc->aac_container[i]);
	    }
	    device_set_desc(child, aac_describe_code(aac_container_types, mir.MntTable[0].VolType));
	    sc->aac_container[i].co_disk = child;
	    sc->aac_container[i].co_mntobj = mir.MntTable[0];
	}
    }

    /* poke the bus to actually attach the child devices */
    if (bus_generic_attach(sc->aac_dev))
	device_printf(sc->aac_dev, "bus_generic_attach failed\n");

    /* mark the controller up */
    sc->aac_state &= ~AAC_STATE_SUSPEND;

    /* enable interrupts now */
    AAC_UNMASK_INTERRUPTS(sc);
}

/********************************************************************************
 * Free all of the resources associated with (sc)
 *
 * Should not be called if the controller is active.
 *
 * XXX verify that we are freeing all our resources here...
 */
void
aac_free(struct aac_softc *sc)
{
    struct aac_command_cluster	*cmc;

    debug_called(1);

    /* remove the control device */
    if (sc->aac_dev_t != NULL)
	destroy_dev(sc->aac_dev_t);

    /* throw away any command buffers */
    while ((cmc = aac_dequeue_cluster(sc)) != NULL)
	aac_free_command_cluster(cmc);

    /* destroy the common area */
    if (sc->aac_common) {
	bus_dmamap_unload(sc->aac_common_dmat, sc->aac_common_dmamap);
	bus_dmamem_free(sc->aac_common_dmat, sc->aac_common, sc->aac_common_dmamap);
	bus_dma_tag_destroy(sc->aac_common_dmat);
    }

    /* disconnect the interrupt handler */
    if (sc->aac_intr)
	bus_teardown_intr(sc->aac_dev, sc->aac_irq, sc->aac_intr);
    if (sc->aac_irq != NULL)
	bus_release_resource(sc->aac_dev, SYS_RES_IRQ, sc->aac_irq_rid, sc->aac_irq);

    /* destroy data-transfer DMA tag */
    if (sc->aac_buffer_dmat)
	bus_dma_tag_destroy(sc->aac_buffer_dmat);

    /* destroy FIB DMA tag */
    if (sc->aac_buffer_dmat)
	bus_dma_tag_destroy(sc->aac_fib_dmat);

    /* destroy the parent DMA tag */
    if (sc->aac_parent_dmat)
	bus_dma_tag_destroy(sc->aac_parent_dmat);

    /* release the register window mapping */
    if (sc->aac_regs_resource != NULL)
	bus_release_resource(sc->aac_dev, SYS_RES_MEMORY, sc->aac_regs_rid, sc->aac_regs_resource);
}

/********************************************************************************
 * Disconnect from the controller completely, in preparation for unload.
 */
int
aac_detach(device_t dev)
{
    struct aac_softc	*sc = device_get_softc(dev);
    int			error;

    debug_called(1);

    if (sc->aac_state & AAC_STATE_OPEN)
	return(EBUSY);

    if ((error = aac_shutdown(dev)))
	return(error);

    aac_free(sc);

    return(0);
}

/********************************************************************************
 * Bring the controller down to a dormant state and detach all child devices.
 *
 * This function is called before detach or system shutdown.
 *
 * Note that we can assume that the camq on the controller is empty, as we won't
 * allow shutdown if any device is open.
 */
int
aac_shutdown(device_t dev)
{
    struct aac_softc		*sc = device_get_softc(dev);
    struct aac_close_command	cc;
    int				s, i;

    debug_called(1);

    s = splbio();

    sc->aac_state |= AAC_STATE_SUSPEND;

    /* 
     * Send a Container shutdown followed by a HostShutdown FIB to the
     * controller to convince it that we don't want to talk to it anymore.
     * We've been closed and all I/O completed already
     */
    device_printf(sc->aac_dev, "shutting down controller...");

    cc.Command = VM_CloseAll;
    cc.ContainerId = 0xffffffff;
    if (aac_sync_fib(sc, ContainerCommand, 0, &cc, sizeof(cc), NULL, NULL)) {
	printf("FAILED.\n");
    } else {
	i = 0;
	if (aac_sync_fib(sc, FsaHostShutdown, AAC_FIBSTATE_SHUTDOWN, &i, sizeof(i), NULL, NULL)) {
	    printf("FAILED.\n");
	} else {
	    printf("done.\n");
	}
    }

    AAC_MASK_INTERRUPTS(sc);

    splx(s);
    return(0);
}

/********************************************************************************
 * Bring the controller to a quiescent state, ready for system suspend.
 */
int
aac_suspend(device_t dev)
{
    struct aac_softc	*sc = device_get_softc(dev);
    int			s;

    debug_called(1);
    s = splbio();

    sc->aac_state |= AAC_STATE_SUSPEND;
    
    AAC_MASK_INTERRUPTS(sc);
    splx(s);
    return(0);
}

/********************************************************************************
 * Bring the controller back to a state ready for operation.
 */
int
aac_resume(device_t dev)
{
    struct aac_softc	*sc = device_get_softc(dev);

    debug_called(1);
    sc->aac_state &= ~AAC_STATE_SUSPEND;
    AAC_UNMASK_INTERRUPTS(sc);
    return(0);
}

/*******************************************************************************
 * Take an interrupt.
 */
void
aac_intr(void *arg)
{
    struct aac_softc	*sc = (struct aac_softc *)arg;
    u_int16_t		reason;

    debug_called(2);

    reason = AAC_GET_ISTATUS(sc);

    /* controller wants to talk to the log?  XXX should we defer this? */
    if (reason & AAC_DB_PRINTF) {
	if (sc->aac_common->ac_printf[0]) {
	    device_printf(sc->aac_dev, "** %.*s", AAC_PRINTF_BUFSIZE, sc->aac_common->ac_printf);
	    sc->aac_common->ac_printf[0] = 0;
	}
	AAC_CLEAR_ISTATUS(sc, AAC_DB_PRINTF);
	AAC_QNOTIFY(sc, AAC_DB_PRINTF);
    }

    /* controller has a message for us? */
    if (reason & AAC_DB_COMMAND_READY) {
	aac_host_command(sc);
	AAC_CLEAR_ISTATUS(sc, AAC_DB_COMMAND_READY);
    }
    
    /* controller has a response for us? */
    if (reason & AAC_DB_RESPONSE_READY) {
	aac_host_response(sc);
	AAC_CLEAR_ISTATUS(sc, AAC_DB_RESPONSE_READY);
    }

    /* spurious interrupts that we don't use - reset the mask and clear the interrupts */
    if (reason & (AAC_DB_COMMAND_NOT_FULL | AAC_DB_RESPONSE_NOT_FULL)) {
	AAC_UNMASK_INTERRUPTS(sc);
	AAC_CLEAR_ISTATUS(sc, AAC_DB_COMMAND_NOT_FULL | AAC_DB_RESPONSE_NOT_FULL);
    }
};

/********************************************************************************
 ********************************************************************************
                                                               Command Processing
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Start as much queued I/O as possible on the controller
 */
static void
aac_startio(struct aac_softc *sc)
{
    struct aac_command	*cm;

    debug_called(2);

    for(;;) {
	/* try to get a command that's been put off for lack of resources */
	cm = aac_dequeue_ready(sc);

	/* try to build a command off the bio queue (ignore error return) */
	aac_bio_command(sc, &cm);

	/* nothing to do? */
	if (cm == NULL)
	    break;

	/* try to give the command to the controller */
	if (aac_start(cm) == EBUSY) {
	    /* put it on the ready queue for later */
	    aac_requeue_ready(cm);
	    break;
	}
    }
}

/********************************************************************************
 * Deliver a command to the controller; allocate controller resources at the
 * last moment when possible.
 */
static int
aac_start(struct aac_command *cm)
{
    struct aac_softc	*sc = cm->cm_sc;

    debug_called(2);

    /* get the command mapped */
    aac_map_command(cm);

    /* fix up the address values */
    cm->cm_fib->Header.SenderFibAddress = (u_int32_t)cm->cm_fib;
    cm->cm_fib->Header.ReceiverFibAddress = cm->cm_fibphys;

    /* save a pointer to the command for speedy reverse-lookup */
    cm->cm_fib->Header.SenderData = (u_int32_t)cm;		/* XXX ack, sizing */

    /* put the FIB on the outbound queue */
    if (aac_enqueue_fib(sc, AAC_ADAP_NORM_CMD_QUEUE, cm->cm_fib->Header.Size, 
			cm->cm_fib->Header.ReceiverFibAddress))
	return(EBUSY);

    return(0);
}

/********************************************************************************
 * Handle notification of one or more FIBs coming from the controller.
 */
static void
aac_host_command(struct aac_softc *sc)
{
    struct aac_fib	*fib;
    u_int32_t		fib_size;

    debug_called(1);

    for (;;) {
	if (aac_dequeue_fib(sc, AAC_HOST_NORM_CMD_QUEUE, &fib_size, &fib))
	    break;	/* nothing to do */

	switch(fib->Header.Command) {
	case AifRequest:
	    aac_handle_aif(sc, (struct aac_aif_command *)&fib->data[0]);
	    break;
	default:
	    device_printf(sc->aac_dev, "unknown command from controller\n");
	    AAC_PRINT_FIB(sc, fib);
	    break;
	}

	/* XXX reply to FIBs requesting responses ?? */
	/* XXX how do we return these FIBs to the controller? */
    }
}

/********************************************************************************
 * Handle notification of one or more FIBs completed by the controller
 */
static void
aac_host_response(struct aac_softc *sc)
{
    struct aac_command	*cm;
    struct aac_fib	*fib;
    u_int32_t		fib_size;

    debug_called(2);

    for (;;) {
	/* look for completed FIBs on our queue */
	if (aac_dequeue_fib(sc, AAC_HOST_NORM_RESP_QUEUE, &fib_size, &fib))
	    break;	/* nothing to do */
	
	/* get the command, unmap and queue for later processing */
	cm = (struct aac_command *)fib->Header.SenderData;
	if (cm == NULL) {
	    AAC_PRINT_FIB(sc, fib);
	} else {
	    aac_unmap_command(cm);		/* XXX defer? */
	    aac_enqueue_completed(cm);
	}
    }

    /* handle completion processing */
#if __FreeBSD_version >= 500005
    taskqueue_enqueue(taskqueue_swi, &sc->aac_task_complete);
#else
    aac_complete(sc, 0);
#endif
}

/********************************************************************************
 * Process completed commands.
 */
static void
aac_complete(void *context, int pending)
{
    struct aac_softc	*sc = (struct aac_softc *)context;
    struct aac_command	*cm;
    
    debug_called(2);

    /* pull completed commands off the queue */
    for (;;) {
	cm = aac_dequeue_completed(sc);
	if (cm == NULL)
	    return;
	cm->cm_flags |= AAC_CMD_COMPLETED;

	/* is there a completion handler? */
	if (cm->cm_complete != NULL) {
	    cm->cm_complete(cm);
	} else {
	    /* assume that someone is sleeping on this command */
	    wakeup(cm);
	}
    }
}

/********************************************************************************
 * Handle a bio submitted from a disk device.
 */
void
aac_submit_bio(struct bio *bp)
{
    struct aac_disk	*ad = (struct aac_disk *)bp->bio_dev->si_drv1;
    struct aac_softc	*sc = ad->ad_controller;

    debug_called(2);

    /* queue the BIO and try to get some work done */
    bioq_insert_tail(&sc->aac_bioq, bp);
    aac_startio(sc);
}

/********************************************************************************
 * Get a bio and build a command to go with it.
 */
static int
aac_bio_command(struct aac_softc *sc, struct aac_command **cmp)
{
    struct aac_command		*cm;
    struct aac_fib		*fib;
    struct aac_blockread	*br;
    struct aac_blockwrite	*bw;
    struct aac_disk		*ad;
    struct bio			*bp;
    int				s;

    debug_called(2);

    /* get the resources we will need */
    cm = NULL;
    s = splbio();
    if ((bp = bioq_first(&sc->aac_bioq)))
	bioq_remove(&sc->aac_bioq, bp);
    splx(s);
    if (bp == NULL)			/* no work? */
	goto fail;
    if (aac_alloc_command(sc, &cm))	/* get a command */
	goto fail;

    /* fill out the command */
    cm->cm_private = bp;

    /* build the FIB */
    fib = cm->cm_fib;
    fib->Header.XferState =  
	AAC_FIBSTATE_HOSTOWNED   | 
	AAC_FIBSTATE_INITIALISED | 
	AAC_FIBSTATE_FROMHOST    |
	AAC_FIBSTATE_REXPECTED   |
	AAC_FIBSTATE_NORM;
    fib->Header.Command = ContainerCommand;
    fib->Header.Size = sizeof(struct aac_fib_header);

    /* build the read/write request */
    ad = (struct aac_disk *)bp->bio_dev->si_drv1;
    cm->cm_data = (void *)bp->bio_data;
    cm->cm_datalen = bp->bio_bcount;
    cm->cm_complete = aac_bio_complete;
    if (BIO_IS_READ(bp)) {
	br = (struct aac_blockread *)&fib->data[0];
	br->Command = VM_CtBlockRead;
	br->ContainerId = ad->ad_container->co_mntobj.ObjectId;
	br->BlockNumber = bp->bio_pblkno;
	br->ByteCount = bp->bio_bcount;
	fib->Header.Size += sizeof(struct aac_blockread);
	cm->cm_sgtable = &br->SgMap;
	cm->cm_flags |= AAC_CMD_DATAIN;
    } else {
	bw = (struct aac_blockwrite *)&fib->data[0];
	bw->Command = VM_CtBlockWrite;
	bw->ContainerId = ad->ad_container->co_mntobj.ObjectId;
	bw->BlockNumber = bp->bio_pblkno;
	bw->ByteCount = bp->bio_bcount;
	bw->Stable = CUNSTABLE;		/* XXX what's appropriate here? */
	fib->Header.Size += sizeof(struct aac_blockwrite);
	cm->cm_flags |= AAC_CMD_DATAOUT;
	cm->cm_sgtable = &bw->SgMap;
    }

    *cmp = cm;
    return(0);

fail:
    if (bp != NULL)
	bioq_insert_tail(&sc->aac_bioq, bp);
    if (cm != NULL)
	aac_release_command(cm);
    return(ENOMEM);
}

/********************************************************************************
 * Handle a bio-instigated command that has been completed.
 */
static void
aac_bio_complete(struct aac_command *cm)
{
    struct aac_softc			*sc = cm->cm_sc;
    struct aac_blockread_response	*brr;
    struct aac_blockwrite_response	*bwr;
    struct bio				*bp;
    AAC_FSAStatus			status;

    /* fetch relevant status and then release the command */
    bp = (struct bio *)cm->cm_private;
    if (BIO_IS_READ(bp)) {
	brr = (struct aac_blockread_response *)&cm->cm_fib->data[0];
	status = brr->Status;
    } else {
	bwr = (struct aac_blockwrite_response *)&cm->cm_fib->data[0];
	status = bwr->Status;
    }
    aac_release_command(cm);

    /* fix up the bio based on status */
    if (status == ST_OK) {
	bp->bio_resid = 0;
    } else {
	bp->bio_error = EIO;
	bp->bio_flags |= BIO_ERROR;
	
	/* XXX be more verbose? */
	device_printf(sc->aac_dev, "I/O error %d (%s)\n", status, AAC_COMMAND_STATUS(status));
    }
    aac_complete_bio(bp);	/* XXX rename one of these functions! */
}

/********************************************************************************
 * Submit a command to the controller, return when it completes.
 */
static int
aac_wait_command(struct aac_command *cm, int timeout)
{
    int s, error = 0;

    debug_called(2);

    /* Put the command on the ready queue and get things going */
    aac_enqueue_ready(cm);
    aac_startio(cm->cm_sc);
    s = splbio();
    while(!(cm->cm_flags & AAC_CMD_COMPLETED) && (error != EWOULDBLOCK)) {
        error = tsleep(cm, PRIBIO, "aacwait", timeout * hz);
    }
    splx(s);
    return(error);
}

/********************************************************************************
 ********************************************************************************
                                                        Command Buffer Management
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Allocate a command.
 */
static int
aac_alloc_command(struct aac_softc *sc, struct aac_command **cmp)
{
    struct aac_command	*cm;

    debug_called(3);

    cm = aac_dequeue_free(sc);
    if (cm == NULL) {
	aac_alloc_command_cluster(sc);
	cm = aac_dequeue_free(sc);
    }
    if (cm == NULL)
	return(ENOMEM);

    /* initialise the command/FIB */
    cm->cm_sgtable = NULL;
    cm->cm_flags = 0;
    cm->cm_complete = NULL;
    cm->cm_private = NULL;
    cm->cm_fib->Header.XferState = AAC_FIBSTATE_EMPTY;
    cm->cm_fib->Header.StructType = AAC_FIBTYPE_TFIB;
    cm->cm_fib->Header.Flags = 0;
    cm->cm_fib->Header.SenderSize = sizeof(struct aac_fib);

    /* 
     * These are duplicated in aac_start to cover the case where an
     * intermediate stage may have destroyed them.  They're left
     * initialised here for debugging purposes only.
     */
    cm->cm_fib->Header.SenderFibAddress = (u_int32_t)cm->cm_fib;
    cm->cm_fib->Header.ReceiverFibAddress = cm->cm_fibphys;

    *cmp = cm;
    return(0);
}

/********************************************************************************
 * Release a command back to the freelist.
 */
static void
aac_release_command(struct aac_command *cm)
{
    debug_called(3);

    aac_enqueue_free(cm);
}

/********************************************************************************
 * Map helper for command cluster allocation. Tell each of the FIBs what its
 * address in the adapter's space is, fill in a few other fields.
 */
static void
aac_map_command_cluster(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct aac_command_cluster	*cmc = (struct aac_command_cluster *)arg;

    debug_called(3);

    cmc->cmc_fibphys = segs[0].ds_addr;
}

/********************************************************************************
 * Allocate and initialise a cluster of commands.
 */
static void
aac_alloc_command_cluster(struct aac_softc *sc)
{
    struct aac_command_cluster	*cmc;
    struct aac_command		*cm;
    int				i;
 
    debug_called(1);

    cmc = malloc(sizeof(struct aac_command_cluster), M_DEVBUF, M_NOWAIT);
    if (cmc != NULL) {
	bzero(cmc, sizeof(*cmc));

	/* allocate the FIB cluster in DMAable memory and load it */
	if (bus_dmamem_alloc(sc->aac_fib_dmat, (void **)&cmc->cmc_fibs, BUS_DMA_NOWAIT, &cmc->cmc_fibmap)) {
	    free(cmc, M_DEVBUF);
	    return;
	}
	bus_dmamap_load(sc->aac_fib_dmat, cmc->cmc_fibmap, cmc->cmc_fibs, 
			AAC_CLUSTER_COUNT * sizeof(struct aac_fib), aac_map_command_cluster, cmc, 0);

	aac_enqueue_cluster(sc, cmc);
	for (i = 0; i < AAC_CLUSTER_COUNT; i++) {
	    cm = &cmc->cmc_command[i];
	    cm->cm_sc = sc;
	    cm->cm_fib = cmc->cmc_fibs + i;
	    cm->cm_fibphys = cmc->cmc_fibphys + (i * sizeof(struct aac_fib));

	    if (!bus_dmamap_create(sc->aac_buffer_dmat, 0, &cm->cm_datamap))
		aac_release_command(cm);
	}
    } else {
	debug(2, "can't allocate memeory for command cluster");
    }
}

/********************************************************************************
 * Free a command cluster.
 */
static void
aac_free_command_cluster(struct aac_command_cluster *cmc)
{
    struct aac_softc	*sc = cmc->cmc_command[0].cm_sc;
    int			i;

    debug_called(1);

    for (i = 0; i < AAC_CLUSTER_COUNT; i++)
	bus_dmamap_destroy(sc->aac_buffer_dmat, cmc->cmc_command[i].cm_datamap);
    bus_dmamap_unload(sc->aac_fib_dmat, cmc->cmc_fibmap);
    bus_dmamem_free(sc->aac_fib_dmat, cmc->cmc_fibs, cmc->cmc_fibmap);

    free(cmc, M_DEVBUF);
}

/********************************************************************************
 * Command-mapping helper function - populate this command's s/g table.
 */
static void
aac_map_command_sg(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct aac_command		*cm = (struct aac_command *)arg;
    struct aac_fib		*fib = cm->cm_fib;
    struct aac_sg_table		*sg;
    int				i;

    debug_called(3);

    /* find the s/g table */
    sg = cm->cm_sgtable;

    /* copy into the FIB */
    if (sg != NULL) {
	sg->SgCount = nseg;
	for (i = 0; i < nseg; i++) {
	    sg->SgEntry[i].SgAddress = segs[i].ds_addr;
	    sg->SgEntry[i].SgByteCount = segs[i].ds_len;
	}
	/* update the FIB size for the s/g count */
	fib->Header.Size += nseg * sizeof(struct aac_sg_entry);
    }

}

/********************************************************************************
 * Map a command into controller-visible space.
 */
static void
aac_map_command(struct aac_command *cm)
{
    struct aac_softc	*sc = cm->cm_sc;

    debug_called(2);

    /* don't map more than once */
    if (cm->cm_flags & AAC_CMD_MAPPED)
	return;

    if (cm->cm_datalen != 0) {
	bus_dmamap_load(sc->aac_buffer_dmat, cm->cm_datamap, cm->cm_data, 
			cm->cm_datalen, aac_map_command_sg, cm, 0);

	if (cm->cm_flags & AAC_CMD_DATAIN)
	    bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap, BUS_DMASYNC_PREREAD);
	if (cm->cm_flags & AAC_CMD_DATAOUT)
	    bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap, BUS_DMASYNC_PREWRITE);
    }
    cm->cm_flags |= AAC_CMD_MAPPED;
}

/********************************************************************************
 * Unmap a command from controller-visible space.
 */
static void
aac_unmap_command(struct aac_command *cm)
{
    struct aac_softc	*sc = cm->cm_sc;

    debug_called(2);

    if (!(cm->cm_flags & AAC_CMD_MAPPED))
	return;

    if (cm->cm_datalen != 0) {
	if (cm->cm_flags & AAC_CMD_DATAIN)
	    bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap, BUS_DMASYNC_POSTREAD);
	if (cm->cm_flags & AAC_CMD_DATAOUT)
	    bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap, BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->aac_buffer_dmat, cm->cm_datamap);
    }
    cm->cm_flags &= ~AAC_CMD_MAPPED;
}

/********************************************************************************
 ********************************************************************************
                                                               Hardware Interface
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Initialise the adapter.
 */
static void
aac_common_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct aac_softc	*sc = (struct aac_softc *)arg;

    debug_called(1);

    sc->aac_common_busaddr = segs[0].ds_addr;
}

static int
aac_init(struct aac_softc *sc)
{
    struct aac_adapter_init	*ip;
    time_t			then;
    u_int32_t			code;
    u_int8_t			*qaddr;

    debug_called(1);

    /*
     * First wait for the adapter to come ready.
     */
    then = time_second;
    do {
	code = AAC_GET_FWSTATUS(sc);
	if (code & AAC_SELF_TEST_FAILED) {
	    device_printf(sc->aac_dev, "FATAL: selftest failed\n");
	    return(ENXIO);
	}
	if (code & AAC_KERNEL_PANIC) {
	    device_printf(sc->aac_dev, "FATAL: controller kernel panic\n");
	    return(ENXIO);
	}
	if (time_second > (then + AAC_BOOT_TIMEOUT)) {
	    device_printf(sc->aac_dev, "FATAL: controller not coming ready, status %x\n", code);
	    return(ENXIO);
	}
    } while (!(code & AAC_UP_AND_RUNNING));

    /*
     * Create DMA tag for the common structure and allocate it.
     */
    if (bus_dma_tag_create(sc->aac_parent_dmat, 	/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   sizeof(struct aac_common), 1,/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   &sc->aac_common_dmat)) {
	device_printf(sc->aac_dev, "can't allocate common structure DMA tag\n");
	return(ENOMEM);
    }
    if (bus_dmamem_alloc(sc->aac_common_dmat, (void **)&sc->aac_common, BUS_DMA_NOWAIT, &sc->aac_common_dmamap)) {
	device_printf(sc->aac_dev, "can't allocate common structure\n");
	return(ENOMEM);
    }
    bus_dmamap_load(sc->aac_common_dmat, sc->aac_common_dmamap, sc->aac_common, sizeof(*sc->aac_common),
		    aac_common_map, sc, 0);
    bzero(sc->aac_common, sizeof(*sc->aac_common));
    
    /*
     * Fill in the init structure.  This tells the adapter about the physical location
     * of various important shared data structures.
     */
    ip = &sc->aac_common->ac_init;
    ip->InitStructRevision = AAC_INIT_STRUCT_REVISION;

    ip->AdapterFibsPhysicalAddress = sc->aac_common_busaddr + fldoff(aac_common, ac_fibs);
    ip->AdapterFibsVirtualAddress = &sc->aac_common->ac_fibs[0];
    ip->AdapterFibsSize = AAC_ADAPTER_FIBS * sizeof(struct aac_fib);
    ip->AdapterFibAlign = sizeof(struct aac_fib);

    ip->PrintfBufferAddress = sc->aac_common_busaddr + fldoff(aac_common, ac_printf);
    ip->PrintfBufferSize = AAC_PRINTF_BUFSIZE;

    ip->HostPhysMemPages = 0;			/* not used? */
    ip->HostElapsedSeconds = time_second;	/* reset later if invalid */

    /*
     * Initialise FIB queues.  Note that it appears that the layout of the indexes
     * and the segmentation of the entries is mandated by the adapter, which is 
     * only told about the base of the queue index fields.
     *
     * The initial values of the indices are assumed to inform the adapter
     * of the sizes of the respective queues.
     *
     * The Linux driver uses a much more complex scheme whereby several header
     * records are kept for each queue.  We use a couple of generic list manipulation
     * functions which 'know' the size of each list by virtue of a table.
     */
    qaddr = &sc->aac_common->ac_qbuf[0] + AAC_QUEUE_ALIGN;
    qaddr -= (u_int32_t)qaddr % AAC_QUEUE_ALIGN;
    sc->aac_queues = (struct aac_queue_table *)qaddr;
    ip->CommHeaderAddress = sc->aac_common_busaddr + ((u_int32_t)sc->aac_queues - (u_int32_t)sc->aac_common);
    bzero(sc->aac_queues, sizeof(struct aac_queue_table));

    sc->aac_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX]  = AAC_HOST_NORM_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX]  = AAC_HOST_NORM_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX]  = AAC_HOST_HIGH_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX]  = AAC_HOST_HIGH_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX]  = AAC_ADAP_NORM_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX]  = AAC_ADAP_NORM_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX]  = AAC_ADAP_HIGH_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX]  = AAC_ADAP_HIGH_CMD_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX] = AAC_HOST_NORM_RESP_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX] = AAC_HOST_NORM_RESP_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX] = AAC_HOST_HIGH_RESP_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX] = AAC_HOST_HIGH_RESP_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX] = AAC_ADAP_NORM_RESP_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX] = AAC_ADAP_NORM_RESP_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX] = AAC_ADAP_HIGH_RESP_ENTRIES;
    sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX] = AAC_ADAP_HIGH_RESP_ENTRIES;
    sc->aac_qentries[AAC_HOST_NORM_CMD_QUEUE] = &sc->aac_queues->qt_HostNormCmdQueue[0];
    sc->aac_qentries[AAC_HOST_HIGH_CMD_QUEUE] = &sc->aac_queues->qt_HostHighCmdQueue[0];
    sc->aac_qentries[AAC_ADAP_NORM_CMD_QUEUE] = &sc->aac_queues->qt_AdapNormCmdQueue[0];
    sc->aac_qentries[AAC_ADAP_HIGH_CMD_QUEUE] = &sc->aac_queues->qt_AdapHighCmdQueue[0];
    sc->aac_qentries[AAC_HOST_NORM_RESP_QUEUE] = &sc->aac_queues->qt_HostNormRespQueue[0];
    sc->aac_qentries[AAC_HOST_HIGH_RESP_QUEUE] = &sc->aac_queues->qt_HostHighRespQueue[0];
    sc->aac_qentries[AAC_ADAP_NORM_RESP_QUEUE] = &sc->aac_queues->qt_AdapNormRespQueue[0];
    sc->aac_qentries[AAC_ADAP_HIGH_RESP_QUEUE] = &sc->aac_queues->qt_AdapHighRespQueue[0];

    /*
     * Do controller-type-specific initialisation
     */
    switch (sc->aac_hwif) {
    case AAC_HWIF_I960RX:
	AAC_SETREG4(sc, AAC_RX_ODBR, ~0);
	break;
    }

    /*
     * Give the init structure to the controller.
     */
    if (aac_sync_command(sc, AAC_MONKER_INITSTRUCT, 
			  sc->aac_common_busaddr + fldoff(aac_common, ac_init),
			  0, 0, 0, NULL)) {
	device_printf(sc->aac_dev, "error establishing init structure\n");
	return(EIO);
    }

    return(0);
}

/********************************************************************************
 * Send a synchronous command to the controller and wait for a result.
 */
static int
aac_sync_command(struct aac_softc *sc, u_int32_t command,
		       u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3,
		       u_int32_t *sp)
{
    time_t	then;
    u_int32_t	status;

    debug_called(3);

    /* populate the mailbox */
    AAC_SET_MAILBOX(sc, command, arg0, arg1, arg2, arg3);

    /* ensure the sync command doorbell flag is cleared */
    AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

    /* then set it to signal the adapter */
    AAC_QNOTIFY(sc, AAC_DB_SYNC_COMMAND);

    /* spin waiting for the command to complete */
    then = time_second;
    do {
	if (time_second > (then + AAC_IMMEDIATE_TIMEOUT)) {
	    debug(2, "timed out");
	    return(EIO);
	}
    } while (!(AAC_GET_ISTATUS(sc) & AAC_DB_SYNC_COMMAND));

    /* clear the completion flag */
    AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

    /* get the command status */
    status = AAC_GET_MAILBOXSTATUS(sc);
    if (sp != NULL)
	*sp = status;
    return(0);	/* check command return status? */
}

/********************************************************************************
 * Send a synchronous FIB to the controller and wait for a result.
 */
static int
aac_sync_fib(struct aac_softc *sc, u_int32_t command, u_int32_t xferstate, 
	     void *data, u_int16_t datasize,
	     void *result, u_int16_t *resultsize)
{
    struct aac_fib	*fib = &sc->aac_common->ac_sync_fib;

    debug_called(3);

    if (datasize > AAC_FIB_DATASIZE)
	return(EINVAL);

    /*
     * Set up the sync FIB
     */
    fib->Header.XferState = AAC_FIBSTATE_HOSTOWNED | AAC_FIBSTATE_INITIALISED | AAC_FIBSTATE_EMPTY;
    fib->Header.XferState |= xferstate;
    fib->Header.Command = command;
    fib->Header.StructType = AAC_FIBTYPE_TFIB;
    fib->Header.Size = sizeof(struct aac_fib) + datasize;
    fib->Header.SenderSize = sizeof(struct aac_fib);
    fib->Header.SenderFibAddress = (u_int32_t)fib;
    fib->Header.ReceiverFibAddress = sc->aac_common_busaddr + fldoff(aac_common, ac_sync_fib);

    /*
     * Copy in data.
     */
    if (data != NULL) {
	bcopy(data, fib->data, datasize);
	fib->Header.XferState |= AAC_FIBSTATE_FROMHOST | AAC_FIBSTATE_NORM;
    }

    /*
     * Give the FIB to the controller, wait for a response.
     */
    if (aac_sync_command(sc, AAC_MONKER_SYNCFIB, fib->Header.ReceiverFibAddress,
			  0, 0, 0, NULL)) {
	debug(2, "IO error");
	return(EIO);
    }

    /* 
     * Copy out the result
     */
    if (result != NULL) {
	*resultsize = fib->Header.Size - sizeof(struct aac_fib_header);
	bcopy(fib->data, result, *resultsize);
    }
    return(0);
}

/********************************************************************************
 * Adapter-space FIB queue manipulation
 *
 * Note that the queue implementation here is a little funky; neither the PI or
 * CI will ever be zero.  This behaviour is a controller feature.
 */
static struct {
    int		size;
    int		notify;
} aac_qinfo[] = {
    {AAC_HOST_NORM_CMD_ENTRIES, AAC_DB_COMMAND_NOT_FULL},
    {AAC_HOST_HIGH_CMD_ENTRIES, 0},
    {AAC_ADAP_NORM_CMD_ENTRIES, AAC_DB_COMMAND_READY},
    {AAC_ADAP_HIGH_CMD_ENTRIES, 0},
    {AAC_HOST_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_NOT_FULL},
    {AAC_HOST_HIGH_RESP_ENTRIES, 0},
    {AAC_ADAP_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_READY},
    {AAC_ADAP_HIGH_RESP_ENTRIES, 0}
};

/*
 * Atomically insert an entry into the nominated queue, returns 0 on success or EBUSY
 * if the queue is full.
 *
 * XXX note that it would be more efficient to defer notifying the controller in
 * the case where we may be inserting several entries in rapid succession, but
 * implementing this usefully is difficult.
 */
static int
aac_enqueue_fib(struct aac_softc *sc, int queue, u_int32_t fib_size, u_int32_t fib_addr)
{
    u_int32_t	pi, ci;
    int		s, error;

    debug_called(3);

    s = splbio();

    /* get the producer/consumer indices */
    pi = sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX];
    ci = sc->aac_queues->qt_qindex[queue][AAC_CONSUMER_INDEX];

    /* wrap the queue? */
    if (pi >= aac_qinfo[queue].size)
	pi = 0;

    /* check for queue full */
    if ((pi + 1) == ci) {
	error = EBUSY;
	goto out;
    }

    /* populate queue entry */
    (sc->aac_qentries[queue] + pi)->aq_fib_size = fib_size;
    (sc->aac_qentries[queue] + pi)->aq_fib_addr = fib_addr;

    /* update producer index */
    sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX] = pi + 1;

    /* notify the adapter if we know how */
    if (aac_qinfo[queue].notify != 0)
	AAC_QNOTIFY(sc, aac_qinfo[queue].notify);

    error = 0;

out:
    splx(s);
    return(error);
}

/*
 * Atomically remove one entry from the nominated queue, returns 0 on success or ENOENT
 * if the queue is empty.
 */
static int
aac_dequeue_fib(struct aac_softc *sc, int queue, u_int32_t *fib_size, struct aac_fib **fib_addr)
{
    u_int32_t	pi, ci;
    int		s, error;

    debug_called(3);

    s = splbio();

    /* get the producer/consumer indices */
    pi = sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX];
    ci = sc->aac_queues->qt_qindex[queue][AAC_CONSUMER_INDEX];

    /* check for queue empty */
    if (ci == pi) {
	error = ENOENT;
	goto out;
    }
    
    /* wrap the queue? */
    if (ci >= aac_qinfo[queue].size)
	ci = 0;

    /* fetch the entry */
    *fib_size = (sc->aac_qentries[queue] + ci)->aq_fib_size;
    *fib_addr = (struct aac_fib *)(sc->aac_qentries[queue] + ci)->aq_fib_addr;

    /* update consumer index */
    sc->aac_queues->qt_qindex[queue][AAC_CONSUMER_INDEX] = ci + 1;

    /* if we have made the queue un-full, notify the adapter */
    if (((pi + 1) == ci) && (aac_qinfo[queue].notify != 0))
	AAC_QNOTIFY(sc, aac_qinfo[queue].notify);
    error = 0;

out:
    splx(s);
    return(error);
}

/********************************************************************************
 ********************************************************************************
                                                       Interface Function Vectors
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Read the current firmware status word.
 */
static int
aac_sa_get_fwstatus(struct aac_softc *sc)
{
    debug_called(3);

    return(AAC_GETREG4(sc, AAC_SA_FWSTATUS));
}

static int
aac_rx_get_fwstatus(struct aac_softc *sc)
{
    debug_called(3);

    return(AAC_GETREG4(sc, AAC_RX_FWSTATUS));
}

/********************************************************************************
 * Notify the controller of a change in a given queue
 */

static void
aac_sa_qnotify(struct aac_softc *sc, int qbit)
{
    debug_called(3);

    AAC_SETREG2(sc, AAC_SA_DOORBELL1_SET, qbit);
}

static void
aac_rx_qnotify(struct aac_softc *sc, int qbit)
{
    debug_called(3);

    AAC_SETREG4(sc, AAC_RX_IDBR, qbit);
}

/********************************************************************************
 * Get the interrupt reason bits
 */
static int
aac_sa_get_istatus(struct aac_softc *sc)
{
    debug_called(3);

    return(AAC_GETREG2(sc, AAC_SA_DOORBELL0));
}

static int
aac_rx_get_istatus(struct aac_softc *sc)
{
    debug_called(3);

    return(AAC_GETREG4(sc, AAC_RX_ODBR));
}

/********************************************************************************
 * Clear some interrupt reason bits
 */
static void
aac_sa_clear_istatus(struct aac_softc *sc, int mask)
{
    debug_called(3);

    AAC_SETREG2(sc, AAC_SA_DOORBELL0_CLEAR, mask);
}

static void
aac_rx_clear_istatus(struct aac_softc *sc, int mask)
{
    debug_called(3);

    AAC_SETREG4(sc, AAC_RX_ODBR, mask);
}

/********************************************************************************
 * Populate the mailbox and set the command word
 */
static void
aac_sa_set_mailbox(struct aac_softc *sc, u_int32_t command,
		u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3)
{
    debug_called(4);

    AAC_SETREG4(sc, AAC_SA_MAILBOX, command);
    AAC_SETREG4(sc, AAC_SA_MAILBOX + 4, arg0);
    AAC_SETREG4(sc, AAC_SA_MAILBOX + 8, arg1);
    AAC_SETREG4(sc, AAC_SA_MAILBOX + 12, arg2);
    AAC_SETREG4(sc, AAC_SA_MAILBOX + 16, arg3);
}

static void
aac_rx_set_mailbox(struct aac_softc *sc, u_int32_t command,
		u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3)
{
    debug_called(4);

    AAC_SETREG4(sc, AAC_RX_MAILBOX, command);
    AAC_SETREG4(sc, AAC_RX_MAILBOX + 4, arg0);
    AAC_SETREG4(sc, AAC_RX_MAILBOX + 8, arg1);
    AAC_SETREG4(sc, AAC_RX_MAILBOX + 12, arg2);
    AAC_SETREG4(sc, AAC_RX_MAILBOX + 16, arg3);
}

/********************************************************************************
 * Fetch the immediate command status word
 */
static int
aac_sa_get_mailboxstatus(struct aac_softc *sc)
{
    debug_called(4);

    return(AAC_GETREG4(sc, AAC_SA_MAILBOX));
}

static int
aac_rx_get_mailboxstatus(struct aac_softc *sc)
{
    debug_called(4);

    return(AAC_GETREG4(sc, AAC_RX_MAILBOX));
}

/********************************************************************************
 * Set/clear interrupt masks
 */
static void
aac_sa_set_interrupts(struct aac_softc *sc, int enable)
{
    debug(2, "%sable interrupts", enable ? "en" : "dis");

    if (enable) {
	AAC_SETREG2((sc), AAC_SA_MASK0_CLEAR, AAC_DB_INTERRUPTS);
    } else {
	AAC_SETREG2((sc), AAC_SA_MASK0_SET, ~0);
    }
}

static void
aac_rx_set_interrupts(struct aac_softc *sc, int enable)
{
    debug(2, "%sable interrupts", enable ? "en" : "dis");

    if (enable) {
	AAC_SETREG4(sc, AAC_RX_OIMR, ~AAC_DB_INTERRUPTS);
    } else {
	AAC_SETREG4(sc, AAC_RX_OIMR, ~0);
    }
}

/********************************************************************************
 ********************************************************************************
                                                        Debugging and Diagnostics
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Print some information about the controller.
 */
static void
aac_describe_controller(struct aac_softc *sc)
{
    u_int8_t			buf[AAC_FIB_DATASIZE];	/* XXX really a bit big for the stack */
    u_int16_t			bufsize;
    struct aac_adapter_info	*info;
    u_int8_t			arg;

    debug_called(2);

    arg = 0;
    if (aac_sync_fib(sc, RequestAdapterInfo, 0, &arg, sizeof(arg), &buf, &bufsize)) {
	device_printf(sc->aac_dev, "RequestAdapterInfo failed\n");
	return;
    }
    if (bufsize != sizeof(*info)) {
	device_printf(sc->aac_dev, "RequestAdapterInfo returned wrong data size (%d != %d)\n",
		      bufsize, sizeof(*info));
	return;
    }
    info = (struct aac_adapter_info *)&buf[0];

    device_printf(sc->aac_dev, "%s %dMHz, %dMB total memory, %s (%d)\n", 
		  aac_describe_code(aac_cpu_variant, info->CpuVariant), info->ClockSpeed, 
		  info->TotalMem / (1024 * 1024), 
		  aac_describe_code(aac_battery_platform, info->batteryPlatform), info->batteryPlatform);

    /* save the kernel revision structure for later use */
    sc->aac_revision = info->KernelRevision;
    device_printf(sc->aac_dev, "Kernel %d.%d-%d, S/N %llx\n",
		  info->KernelRevision.external.comp.major,
		  info->KernelRevision.external.comp.minor,
		  info->KernelRevision.external.comp.dash,
		  info->SerialNumber);	/* XXX how is this meant to be formatted? */
}

/********************************************************************************
 * Look up a text description of a numeric error code and return a pointer to
 * same.
 */
static char *
aac_describe_code(struct aac_code_lookup *table, u_int32_t code)
{
    int		i;

    for (i = 0; table[i].string != NULL; i++)
	if (table[i].code == code)
	    return(table[i].string);
    return(table[i+1].string);
}

/*****************************************************************************
 *****************************************************************************
                                                    Management Interface
 *****************************************************************************
 *****************************************************************************/

static int
aac_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct aac_softc	*sc = dev->si_drv1;

    debug_called(2);

    /* Check to make sure the device isn't already open */
    if (sc->aac_state & AAC_STATE_OPEN) {
        return EBUSY;
    }
    sc->aac_state |= AAC_STATE_OPEN;

    return 0;
}

static int
aac_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct aac_softc	*sc = dev->si_drv1;

    debug_called(2);

    /* Mark this unit as no longer open  */
    sc->aac_state &= ~AAC_STATE_OPEN;

    return 0;
}

static int
aac_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct proc *p)
{
    struct aac_softc	*sc = dev->si_drv1;
    int			error = 0, i;

    debug_called(2);

    switch (cmd) {
#ifdef AAC_COMPAT_LINUX
    case FSACTL_SENDFIB:
	debug(0, "FSACTL_SENDFIB");
	error = aac_ioctl_sendfib(sc, arg);
	break;
    case FSACTL_AIF_THREAD:
	debug(0, "FSACTL_AIF_THREAD");
	error = EINVAL;
	break;
    case FSACTL_OPEN_GET_ADAPTER_FIB:
	debug(0, "FSACTL_OPEN_GET_ADAPTER_FIB");
	/*
	 * Pass the caller out an AdapterFibContext.
	 *
	 * Note that because we only support one opener, we
	 * basically ignore this.  Set the caller's context to a magic
	 * number just in case.
	 */
	i = AAC_AIF_SILLYMAGIC;
	error = copyout(&i, arg, sizeof(i));
	break;
    case FSACTL_GET_NEXT_ADAPTER_FIB:
	debug(0, "FSACTL_GET_NEXT_ADAPTER_FIB");
	error = aac_linux_getnext_aif(sc, arg);
	break;
    case FSACTL_CLOSE_GET_ADAPTER_FIB:
	debug(0, "FSACTL_CLOSE_GET_ADAPTER_FIB");
	/* don't do anything here */
	break;
    case FSACTL_MINIPORT_REV_CHECK:
	debug(0, "FSACTL_MINIPORT_REV_CHECK");
	error = aac_linux_rev_check(sc, arg);
	break;
#endif
    default:
	device_printf(sc->aac_dev, "unsupported cmd 0x%lx\n", cmd);
	error = EINVAL;
	break;
    }
    return(error);
}

/********************************************************************************
 * Send a FIB supplied from userspace
 */
static int
aac_ioctl_sendfib(struct aac_softc *sc, caddr_t ufib)
{
    struct aac_command 	*cm;
    int			size, error;

    debug_called(2);

    cm = NULL;

    /*
     * Get a command
     */
    if (aac_alloc_command(sc, &cm)) {
	error = EBUSY;
	goto out;
    }

    /*
     * Fetch the FIB header, then re-copy to get data as well.
     */
    if ((error = copyin(ufib, cm->cm_fib, sizeof(struct aac_fib_header))) != 0)
	goto out;
    size = cm->cm_fib->Header.Size + sizeof(struct aac_fib_header);
    if (size > sizeof(struct aac_fib)) {
	device_printf(sc->aac_dev, "incoming FIB oversized (%d > %d)\n", size, sizeof(struct aac_fib));
	size = sizeof(struct aac_fib);
    }
    if ((error = copyin(ufib, cm->cm_fib, size)) != 0)
	goto out;
    cm->cm_fib->Header.Size = size;

    /*
     * Pass the FIB to the controller, wait for it to complete.
     */
    if ((error = aac_wait_command(cm, 30)) != 0)	/* XXX user timeout? */
	goto out;

    /*
     * Copy the FIB and data back out to the caller.
     */
    size = cm->cm_fib->Header.Size;
    if (size > sizeof(struct aac_fib)) {
	device_printf(sc->aac_dev, "outbound FIB oversized (%d > %d)\n", size, sizeof(struct aac_fib));
	size = sizeof(struct aac_fib);
    }
    error = copyout(cm->cm_fib, ufib, size);

out:
    if (cm != NULL)
	aac_release_command(cm);
    return(error);
}

/********************************************************************************
 * Handle an AIF sent to us by the controller; queue it for later reference.
 *
 * XXX what's the right thing to do here when the queue is full?  Drop the older
 * or newer entries?
 */
static void
aac_handle_aif(struct aac_softc *sc, struct aac_aif_command *aif)
{
    int		next, s;

    debug_called(2);

    s = splbio();
    next = (sc->aac_aifq_head + 1) % AAC_AIFQ_LENGTH;
    if (next != sc->aac_aifq_tail) {
	bcopy(aif, &sc->aac_aifq[next], sizeof(struct aac_aif_command));
	sc->aac_aifq_head = next;
	if (sc->aac_state & AAC_STATE_AIF_SLEEPER)
	    wakeup(sc->aac_aifq);
    }
    splx(s);
    aac_print_aif(sc, aif);
}

/********************************************************************************
 * Hand the next AIF off the top of the queue out to userspace.
 */
static int
aac_return_aif(struct aac_softc *sc, caddr_t uptr)
{
    int		error, s;

    debug_called(2);

    s = splbio();
    if (sc->aac_aifq_tail == sc->aac_aifq_head) {
	error = EAGAIN;
    } else {
	error = copyout(&sc->aac_aifq[sc->aac_aifq_tail], uptr, sizeof(struct aac_aif_command));
	if (!error)
	    sc->aac_aifq_tail = (sc->aac_aifq_tail + 1) % AAC_AIFQ_LENGTH;
    }
    splx(s);
    return(error);
}

/********************************************************************************
 ********************************************************************************
                                                       Linux Management Interface
 ********************************************************************************
 ********************************************************************************/

#ifdef AAC_COMPAT_LINUX

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_ioctl.h>

#define AAC_LINUX_IOCTL_MIN  0x2000
#define AAC_LINUX_IOCTL_MAX  0x21ff

static linux_ioctl_function_t aac_linux_ioctl;
static struct linux_ioctl_handler aac_handler = {aac_linux_ioctl, AAC_LINUX_IOCTL_MIN, AAC_LINUX_IOCTL_MAX};

SYSINIT  (aac_register,   SI_SUB_KLD, SI_ORDER_MIDDLE, linux_ioctl_register_handler, &aac_handler);
SYSUNINIT(aac_unregister, SI_SUB_KLD, SI_ORDER_MIDDLE, linux_ioctl_unregister_handler, &aac_handler);

MODULE_DEPEND(aac, linux, 1, 1, 1);

static int
aac_linux_ioctl(struct proc *p, struct linux_ioctl_args *args)
{
    struct file		*fp = p->p_fd->fd_ofiles[args->fd];
    u_long		cmd = args->cmd;

    /*
     * Pass the ioctl off to our standard handler.
     */
    return(fo_ioctl(fp, cmd, (caddr_t)args->arg, p));
}

/********************************************************************************
 * Return the Revision of the driver to the userspace and check to see if the
 * userspace app is possibly compatible.  This is extremely bogus right now
 * because I have no idea how to handle the versioning of this driver.  It is
 * needed, though, to get aaccli working.
 */
static int
aac_linux_rev_check(struct aac_softc *sc, caddr_t udata)
{
    struct aac_rev_check	rev_check;
    struct aac_rev_check_resp	rev_check_resp;
    int				error = 0;

    debug_called(2);

    /*
     * Copyin the revision struct from userspace
     */
    if ((error = copyin(udata, (caddr_t)&rev_check, sizeof(struct aac_rev_check))) != 0) {
	return error;
    }

    debug(2, "Userland revision= %d\n", rev_check.callingRevision.buildNumber);

    /*
     * Doctor up the response struct.
     */
    rev_check_resp.possiblyCompatible = 1;
    rev_check_resp.adapterSWRevision.external.ul = sc->aac_revision.external.ul;
    rev_check_resp.adapterSWRevision.buildNumber = sc->aac_revision.buildNumber;

    return(copyout((caddr_t)&rev_check_resp, udata, sizeof(struct aac_rev_check_resp)));
}

/********************************************************************************
 * Pass the caller the next AIF in their queue
 */
static int
aac_linux_getnext_aif(struct aac_softc *sc, caddr_t arg)
{
    struct get_adapter_fib_ioctl	agf;
    int					error, s;

    debug_called(2);

    if ((error = copyin(arg, &agf, sizeof(agf))) == 0) {

	/*
	 * Check the magic number that we gave the caller.
	 */
	if (agf.AdapterFibContext != AAC_AIF_SILLYMAGIC) {
	    error = EFAULT;
	} else {

	    s = splbio();
	    error = aac_return_aif(sc, agf.AifFib);

	    if ((error == EAGAIN) && (agf.Wait)) {
		sc->aac_state |= AAC_STATE_AIF_SLEEPER;
		while (error == EAGAIN) {
		    error = tsleep(sc->aac_aifq, PRIBIO | PCATCH, "aacaif", 0);
		    if (error == 0)
			error = aac_return_aif(sc, agf.AifFib);
		}
		sc->aac_state &= ~AAC_STATE_AIF_SLEEPER;
	    }
	    splx(s);
	}
    }
    return(error);
}

#endif /* AAC_COMPAT_LINUX */
