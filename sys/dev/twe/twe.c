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
 *	$FreeBSD: src/sys/dev/twe/twe.c,v 1.1.2.1 2000/05/25 01:50:48 msmith Exp $
 */

/*
 * Driver for the 3ware Escalade family of IDE RAID controllers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#if __FreeBSD_version < 500000
# include <dev/twe/twe_compat.h>
#else
# include <sys/bio.h>
#endif
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/stat.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/twe/twereg.h>
#include <dev/twe/twevar.h>

/*
 * Initialisation, bus interface.
 */
static int			twe_probe(device_t dev);
static int			twe_attach(device_t dev);
static void			twe_free(struct twe_softc *sc);
static void			twe_startup(void *arg);
static int			twe_detach(device_t dev);
static int			twe_shutdown(device_t dev);
static int			twe_suspend(device_t dev);
static int			twe_resume(device_t dev);
static void			twe_intr(void *arg);

/*
 * Control device.
 */
static	d_open_t		twe_open;
static	d_close_t		twe_close;
static	d_ioctl_t		twe_ioctl;

/*
 * Command submission.
 */
static void			*twe_get_param(struct twe_softc *sc, int table_id, int parameter_id, size_t size, 
					       void (* func)(struct twe_request *tr));
static int			twe_init_connection(struct twe_softc *sc);
/*static int			twe_wait_request(struct twe_request *tr);*/
static int			twe_immediate_request(struct twe_request *tr);
static void			twe_startio(struct twe_softc *sc);
static void			twe_completeio(struct twe_request *tr);

/*
 * Command I/O to controller.
 */
static int			twe_start(struct twe_request *tr);
static void			twe_done(struct twe_softc *sc);
static void			twe_complete(struct twe_softc *sc);
static int			twe_wait_status(struct twe_softc *sc, u_int32_t status, int timeout);
static int			twe_drain_response_queue(struct twe_softc *sc);
static int			twe_check_bits(struct twe_softc *sc, u_int32_t status_reg);

/*
 * Interrupt handling.
 */
static void			twe_host_intr(struct twe_softc *sc);
static void			twe_attention_intr(struct twe_softc *sc);
static void			twe_command_intr(struct twe_softc *sc);
static void			twe_enable_interrupts(struct twe_softc *sc);
static void			twe_disable_interrupts(struct twe_softc *sc);

/*
 * Asynchronous event handling.
 */
static int			twe_fetch_aen(struct twe_softc *sc);
static void			twe_handle_aen(struct twe_request *tr);
static void			twe_enqueue_aen(struct twe_softc *sc, u_int16_t aen);
/*static int			twe_dequeue_aen(struct twe_softc *sc);*/
static int			twe_drain_aen_queue(struct twe_softc *sc);
static int			twe_find_aen(struct twe_softc *sc, u_int16_t aen);

/*
 * Command buffer management.
 */
static struct twe_request	*twe_get_request(struct twe_softc *sc);
static void			twe_release_request(struct twe_request *tr);
static void			twe_free_request(struct twe_request *tr);
static int			twe_get_requestid(struct twe_request *tr);
static void			twe_release_requestid(struct twe_request *tr);
static void			twe_setup_data_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error);
static void			twe_setup_request_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error);
static void			twe_map_request(struct twe_request *tr);
static void			twe_unmap_request(struct twe_request *tr);

/*
 * Debugging.
 */
static char 			*twe_name_aen(u_int16_t aen);
#if 0
static void			twe_print_request(struct twe_request *tr);
void				twe_report(void);
#endif

/********************************************************************************
 ********************************************************************************
                                                                Public Interfaces
 ********************************************************************************
 ********************************************************************************/

devclass_t	twe_devclass;

static device_method_t twe_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	twe_probe),
    DEVMETHOD(device_attach,	twe_attach),
    DEVMETHOD(device_detach,	twe_detach),
    DEVMETHOD(device_shutdown,	twe_shutdown),
    DEVMETHOD(device_suspend,	twe_suspend),
    DEVMETHOD(device_resume,	twe_resume),

    DEVMETHOD(bus_print_child,	bus_generic_print_child),
    DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
    { 0, 0 }
};

static driver_t twe_pci_driver = {
	"twe",
	twe_methods,
	sizeof(struct twe_softc)
};

DRIVER_MODULE(tw, pci, twe_pci_driver, twe_devclass, 0, 0);

#define TWE_CDEV_MAJOR  146

static struct cdevsw twe_cdevsw = {
                /* open */      twe_open,
                /* close */     twe_close,
                /* read */      noread,
                /* write */     nowrite,
                /* ioctl */     twe_ioctl,
                /* poll */      nopoll,
                /* mmap */      nommap,
                /* strategy */  nostrategy,
                /* name */      "twe",
                /* maj */       TWE_CDEV_MAJOR,
                /* dump */      nodump,
                /* psize */     nopsize,
                /* flags */     0,
                /* bmaj */      -1
};

/********************************************************************************
 * Match a 3ware Escalade ATA RAID controller.
 */
static int
twe_probe(device_t dev)
{

    debug_called(4);

    if ((pci_get_vendor(dev) == TWE_VENDOR_ID) &&
	(pci_get_device(dev) == TWE_DEVICE_ID)) {
	device_set_desc(dev, TWE_DEVICE_NAME);
	return(0);
    }
    return(ENXIO);
}

/********************************************************************************
 * Free all of the resources associated with (sc).
 *
 * Should not be called if the controller is active.
 */
static void
twe_free(struct twe_softc *sc)
{
    struct twe_request	*tr;

    debug_called(4);

    /* throw away any command buffers */
    while ((tr = TAILQ_FIRST(&sc->twe_freecmds)) != NULL) {
	TAILQ_REMOVE(&sc->twe_freecmds, tr, tr_link);
	twe_free_request(tr);
    }

    /* destroy the data-transfer DMA tag */
    if (sc->twe_buffer_dmat)
	bus_dma_tag_destroy(sc->twe_buffer_dmat);

    /* disconnect the interrupt handler */
    if (sc->twe_intr)
	bus_teardown_intr(sc->twe_dev, sc->twe_irq, sc->twe_intr);
    if (sc->twe_irq != NULL)
	bus_release_resource(sc->twe_dev, SYS_RES_IRQ, 0, sc->twe_irq);

    /* destroy the parent DMA tag */
    if (sc->twe_parent_dmat)
	bus_dma_tag_destroy(sc->twe_parent_dmat);

    /* release the register window mapping */
    if (sc->twe_io != NULL)
	bus_release_resource(sc->twe_dev, SYS_RES_IOPORT, TWE_IO_CONFIG_REG, sc->twe_io);

    /* destroy control device */
    if (sc->twe_dev_t != (dev_t)NULL)
	destroy_dev(sc->twe_dev_t);
}

/********************************************************************************
 * Allocate resources, initialise the controller.
 */
static int
twe_attach(device_t dev)
{
    struct twe_softc	*sc;
    int			rid, error;
    u_int32_t		command;

    debug_called(4);

    /*
     * Make sure we are going to be able to talk to this board.
     */
    command = pci_read_config(dev, PCIR_COMMAND, 2);
    if ((command & PCIM_CMD_PORTEN) == 0) {
	device_printf(dev, "register window not available\n");
	return(ENXIO);
    }
    /*
     * Force the busmaster enable bit on, in case the BIOS forgot.
     */
    command |= PCIM_CMD_BUSMASTEREN;
    pci_write_config(dev, PCIR_COMMAND, command, 2);

    /*
     * Initialise the softc structure.
     */
    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc)); 
    sc->twe_dev = dev;
    TAILQ_INIT(&sc->twe_work);
    TAILQ_INIT(&sc->twe_freecmds);
    bioq_init(&sc->twe_bioq);
    sc->twe_wait_aen = -1;

    /*
     * Allocate the PCI register window.
     */
    rid = TWE_IO_CONFIG_REG;
    sc->twe_io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
    if (sc->twe_io == NULL) {
	device_printf(sc->twe_dev, "can't allocate register window\n");
	twe_free(sc);
	return(ENXIO);
    }
    sc->twe_btag = rman_get_bustag(sc->twe_io);
    sc->twe_bhandle = rman_get_bushandle(sc->twe_io);

    /*
     * Allocate the parent bus DMA tag appropriate for PCI.
     */
    error = bus_dma_tag_create(NULL, 				/* parent */
			       1, 0, 				/* alignment, boundary */
			       BUS_SPACE_MAXADDR_32BIT, 	/* lowaddr */
			       BUS_SPACE_MAXADDR, 		/* highaddr */
			       NULL, NULL, 			/* filter, filterarg */
			       MAXBSIZE, TWE_MAX_SGL_LENGTH,	/* maxsize, nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,		/* maxsegsize */
			       BUS_DMA_ALLOCNOW,		/* flags */
			       &sc->twe_parent_dmat);
    if (error != 0) {
	device_printf(dev, "can't allocate parent DMA tag\n");
	twe_free(sc);
	return(ENOMEM);
    }

    /* 
     * Allocate and connect our interrupt.
     */
    rid = 0;
    sc->twe_irq = bus_alloc_resource(sc->twe_dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
    if (sc->twe_irq == NULL) {
	device_printf(sc->twe_dev, "can't allocate interrupt\n");
	twe_free(sc);
	return(ENXIO);
    }
    error = bus_setup_intr(sc->twe_dev, sc->twe_irq, INTR_TYPE_BIO,  twe_intr, sc, &sc->twe_intr);
    if (error) {
	device_printf(sc->twe_dev, "can't set up interrupt\n");
	twe_free(sc);
	return(ENXIO);
    }

    /*
     * Create DMA tag for mapping objects into controller-addressable space.
     */
    error = bus_dma_tag_create(sc->twe_parent_dmat, 		/* parent */
			       1, 0, 				/* alignment, boundary */
			       BUS_SPACE_MAXADDR,		/* lowaddr */
			       BUS_SPACE_MAXADDR, 		/* highaddr */
			       NULL, NULL, 			/* filter, filterarg */
			       MAXBSIZE, TWE_MAX_SGL_LENGTH,	/* maxsize, nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,		/* maxsegsize */
			       0,				/* flags */
			       &sc->twe_buffer_dmat);
    if (error != 0) {
	device_printf(sc->twe_dev, "can't allocate data buffer DMA tag\n");
	twe_free(sc);
	return(ENOMEM);
    }

    /*
     * Create the control device.
     */
    sc->twe_dev_t = make_dev(&twe_cdevsw, device_get_unit(sc->twe_dev), UID_ROOT, GID_OPERATOR,
			     S_IRUSR | S_IWUSR, "twe%d", device_get_unit(sc->twe_dev));

    /*
     * Schedule ourselves to bring the controller up once interrupts are available.
     * This isn't strictly necessary, since we disable interrupts while probing the
     * controller, but it is more in keeping with common practice for other disk 
     * devices.
     */
    bzero(&sc->twe_ich, sizeof(struct intr_config_hook));
    sc->twe_ich.ich_func = twe_startup;
    sc->twe_ich.ich_arg = sc;
    if (config_intrhook_establish(&sc->twe_ich) != 0) {
	device_printf(sc->twe_dev, "can't establish configuration hook\n");
	twe_free(sc);
	return(ENXIO);
    }

    return(0);
}

/********************************************************************************
 * Initialise the controller, locate disk devices and attach children to them.
 */
static void
twe_startup(void *arg)
{
    struct twe_softc		*sc = (struct twe_softc *)arg;
    struct twe_drive		*dr;
    int				i, error;
    u_int32_t			status_reg;
    TWE_Param			*drives, *capacity;

    debug_called(4);

    /* pull ourselves off the intrhook chain */
    config_intrhook_disestablish(&sc->twe_ich);

    /*
     * Wait for the controller to come ready.
     */
    if (twe_wait_status(sc, TWE_STATUS_MICROCONTROLLER_READY, 60)) {
	device_printf(sc->twe_dev, "microcontroller not ready\n");
	return;
    }

    /*
     * Disable interrupts from the card while we're getting it into a safe state.
     */
    twe_disable_interrupts(sc);

    /*
     * Soft reset the controller, look for the AEN acknowledging the reset,
     * check for errors, drain the response queue.
     */
    for (i = 0; i < TWE_MAX_RESET_TRIES; i++) {

	if (i > 0)
	    device_printf(sc->twe_dev, "reset %d failed, trying again\n", i);

	TWE_SOFT_RESET(sc);

	if (twe_wait_status(sc, TWE_STATUS_ATTENTION_INTERRUPT, 15)) {
	    device_printf(sc->twe_dev, "no attention interrupt");
	    continue;
	}
	if (twe_drain_aen_queue(sc)) {
	    device_printf(sc->twe_dev, "can't drain AEN queue\n");
	    continue;
	}
	if (twe_find_aen(sc, TWE_AEN_SOFT_RESET)) {
	    device_printf(sc->twe_dev, "reset not reported\n");
	    continue;
	}
	status_reg = TWE_STATUS(sc);
	if (TWE_STATUS_ERRORS(status_reg) || twe_check_bits(sc, status_reg)) {
	    device_printf(sc->twe_dev, "controller errors detected\n");
	    continue;
	}
	if (twe_drain_response_queue(sc)) {
	    device_printf(sc->twe_dev, "can't drain response queue\n");
	    continue;
	}
	break;			/* reset process complete */
    }
    /* did we give up? */
    if (i >= TWE_MAX_RESET_TRIES) {
	device_printf(sc->twe_dev, "can't initialise controller, giving up\n");
	return;
    }

    /*
     * The controller is in a safe state, so try to find drives attached to it.
     * XXX ick, magic numbers
     */
    if ((drives = twe_get_param(sc, 3, 3, TWE_MAX_UNITS, NULL)) == NULL) {
	device_printf(sc->twe_dev, "can't detect attached units\n");
	return;
    }
    
    /*
     * For each detected unit, create a child device.
     */
    for (i = 0, dr = &sc->twe_drive[0]; i < TWE_MAX_UNITS; i++, dr++) {
	
	if (drives->data[i] == 0)	/* unit not present */
	    continue;

	if ((capacity = twe_get_param(sc, TWE_UNIT_INFORMATION_TABLE_BASE + i, 4, 4, NULL)) == NULL) {
	    device_printf(sc->twe_dev, "error fetching capacity for unit %d\n", i);
	    continue;
	}
	dr->td_size = *(u_int32_t *)capacity->data;
	free(capacity, M_DEVBUF);

	/* build synthetic geometry as per controller internal rules */
	if (dr->td_size > 0x200000) {
	    dr->td_heads = 255;
	    dr->td_sectors = 63;
	} else {
	    dr->td_heads = 64;
	    dr->td_sectors = 32;
	}
	dr->td_cylinders = dr->td_size / (dr->td_heads * dr->td_sectors);

	dr->td_unit = i;
	dr->td_state = TWE_DRIVE_UNKNOWN;		/* XXX how do we find out what the real state is? */
	dr->td_raidlevel = TWE_DRIVE_UNKNOWN;		/* XXX how do we find out what the real raidlevel is? */

	dr->td_disk =  device_add_child(sc->twe_dev, NULL, -1);
	if (dr->td_disk == 0)
	    device_printf(sc->twe_dev, "device_add_child failed\n");
	device_set_ivars(dr->td_disk, dr);
    }
    free(drives, M_DEVBUF);

    if ((error = bus_generic_attach(sc->twe_dev)) != 0)
	device_printf(sc->twe_dev, "bus_generic_attach returned %d\n", error);

    /*
     * Initialise connection with controller.
     */
    twe_init_connection(sc);

    /* 
     * Mark controller up and ready to run.
     */
    sc->twe_state &= ~TWE_STATE_SHUTDOWN;

    /*
     * Finally enable interrupts .
     */
    twe_enable_interrupts(sc);
}

/********************************************************************************
 * Disconnect from the controller completely, in preparation for unload.
 */
static int
twe_detach(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);
    int			s, error;

    debug_called(4);

    error = EBUSY;
    s = splbio();
    if (sc->twe_state & TWE_STATE_OPEN)
	goto out;

    /*	
     * Shut the controller down.
     */
    if ((error = twe_shutdown(dev)))
	goto out;

    twe_free(sc);

    error = 0;
 out:
    splx(s);
    return(error);
}

/********************************************************************************
 * Bring the controller down to a dormant state and detach all child devices.
 *
 * Note that we can assume that the bioq on the controller is empty, as we won't
 * allow shutdown if any device is open.
 */
static int
twe_shutdown(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);
    int			i, s, error;

    debug_called(4);

    s = splbio();
    error = 0;

    /*
     * Mark the controller as shutting down, and disable any further interrupts.
     */
    sc->twe_state |= TWE_STATE_SHUTDOWN;
    twe_disable_interrupts(sc);

    /* 
     * Delete all our child devices.
     */
    for (i = 0; i < TWE_MAX_UNITS; i++) {
	if (sc->twe_drive[i].td_disk != 0) {
	    if ((error = device_delete_child(sc->twe_dev, sc->twe_drive[i].td_disk)) != 0)
		goto out;
	    sc->twe_drive[i].td_disk = 0;
	}
    }

 out:
    splx(s);
    return(error);
}

/********************************************************************************
 * Bring the controller to a quiescent state, ready for system suspend.
 * 
 * XXX this isn't really very well implemented.
 */
static int
twe_suspend(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);
    int			s;

    debug_called(4);

    s = splbio();
    sc->twe_state |= TWE_STATE_SUSPEND;
    
    twe_disable_interrupts(sc);
    splx(s);

    return(0);
}

/********************************************************************************
 * Bring the controller back to a state ready for operation.
 */
static int
twe_resume(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);

    debug_called(4);

    sc->twe_state &= ~TWE_STATE_SUSPEND;
    twe_enable_interrupts(sc);

    return(0);
}

/*******************************************************************************
 * Take an interrupt, or be poked by other code to look for interrupt-worthy
 * status.
 */
static void
twe_intr(void *arg)
{
    struct twe_softc	*sc = (struct twe_softc *)arg;
    u_int32_t		status_reg;

    debug_called(4);

    /*
     * Collect current interrupt status.
     */
    status_reg = TWE_STATUS(sc);
    twe_check_bits(sc, status_reg);

    /*
     * Dispatch based on interrupt status
     */
    if (status_reg & TWE_STATUS_HOST_INTERRUPT)
	twe_host_intr(sc);
    if (status_reg & TWE_STATUS_ATTENTION_INTERRUPT)
	twe_attention_intr(sc);
    if (status_reg & TWE_STATUS_COMMAND_INTERRUPT)
	twe_command_intr(sc);
    if (status_reg * TWE_STATUS_RESPONSE_INTERRUPT)
	twe_done(sc);
};

/********************************************************************************
 ********************************************************************************
                                                                   Control Device
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Accept an open operation on the control device.
 */
static int
twe_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct twe_softc	*sc = devclass_get_softc(twe_devclass, unit);

    sc->twe_state |= TWE_STATE_OPEN;
    return(0);
}

/********************************************************************************
 * Accept the last close on the control device.
 */
static int
twe_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct twe_softc	*sc = devclass_get_softc(twe_devclass, unit);

    sc->twe_state &= ~TWE_STATE_OPEN;
    return (0);
}

/********************************************************************************
 * Handle controller-specific control operations.
 */
static int
twe_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    
    switch(cmd) {
    default:	
	return(ENOTTY);
    }
}

/********************************************************************************
 ********************************************************************************
                                                               Command Submission
 ********************************************************************************
 ********************************************************************************/

/*******************************************************************************
 * Receive a bio structure from a child device and queue it on a particular
 * controller, then poke the controller to start as much work as it can.
 */
int
twe_submit_buf(struct twe_softc *sc, struct bio *bp)
{
    int		s;
    
    debug_called(4);

    s = splbio();
    bioq_insert_tail(&sc->twe_bioq, bp);
    splx(s);

    twe_startio(sc);
    return(0);
}

/********************************************************************************
 * Perform a TWE_OP_GET_PARAM command.  If a callback function is provided, it
 * will be called with the command when it's completed.  If no callback is 
 * provided, we will wait for the command to complete and then return just the data.
 * The caller is responsible for freeing the data when done with it.
 */
static void *
twe_get_param(struct twe_softc *sc, int table_id, int parameter_id, size_t size, void (* func)(struct twe_request *tr))
{
    struct twe_request	*tr;
    TWE_Command		*cmd;
    TWE_Param		*param;
    int			error;

    debug_called(4);

    tr = NULL;
    param = NULL;

    /* get a command */
    if ((tr = twe_get_request(sc)) == NULL)
	goto err;

    /* get a buffer */
    if ((param = (TWE_Param *)malloc(TWE_SECTOR_SIZE, M_DEVBUF, M_NOWAIT)) == NULL)
	goto err;
    tr->tr_data = param;
    tr->tr_length = TWE_SECTOR_SIZE;
    tr->tr_flags = TWE_CMD_DATAIN | TWE_CMD_DATAOUT;
    tr->tr_complete = NULL;
    tr->tr_private = NULL;

    /* build the command for the controller */
    cmd = &tr->tr_command;
    cmd->opcode = TWE_OP_GET_PARAM;
    cmd->sgl_offset = 2;
    cmd->size = 2;
    cmd->unit = 0;
    cmd->count = 1;

    /* map the command/data into controller-visible space */
    twe_map_request(tr);

    /* fill in the outbound parameter data */
    param->table_id = table_id;
    param->parameter_id = parameter_id;
    param->parameter_size_bytes = size;

    /* submit the command and either wait or let the callback handle it */
    if (func == NULL) {
	/* XXX could use twe_wait_request here if interrupts were enabled? */
	error = twe_immediate_request(tr);
	if (error == 0) {
	    if (tr->tr_command.status != 0) {
		debug(2, "command failed - 0x%x", tr->tr_command.status);
		goto err;
	    }
	    twe_release_request(tr);
	    return(param);
	}
    } else {
	tr->tr_complete = func;
	error = twe_start(tr);
	if (error == 0)
	    return(func);
    }

    /* something failed */
err:
    debug(1, "failed");
    if (tr != NULL)
	twe_release_request(tr);
    if (param != NULL)
	free(param, M_DEVBUF);
    return(NULL);
}

/********************************************************************************
 * Perform a TWE_OP_INIT_CONNECTION command, returns nonzero on error.
 *
 * Typically called with interrupts disabled.
 */
static int
twe_init_connection(struct twe_softc *sc)
{
    struct twe_request	*tr;
    TWE_Command		*cmd;
    int			error;
    
    debug_called(4);

    /* get a command */
    if ((tr = twe_get_request(sc)) == NULL)
	return(NULL);

    /* build the command */
    cmd = &tr->tr_command;
    cmd->opcode = TWE_OP_INIT_CONNECTION;
    cmd->sgl_offset = 0;
    cmd->size = 3;
    cmd->unit = 0;
    cmd->count = TWE_INIT_MESSAGE_CREDITS;
    cmd->args.init_connection.response_queue_pointer = 0;

    /* map the command into controller-visible space */
    twe_map_request(tr);

    /* submit the command */
    error = twe_immediate_request(tr);
    /* XXX check command result? */
    twe_unmap_request(tr);
    twe_release_request(tr);

    return(error);
}

#if 0
/********************************************************************************
 * Start the command (tr) and sleep waiting for it to complete.
 *
 * Successfully completed commands are dequeued.
 */
static int
twe_wait_request(struct twe_request *tr)
{
    int			error, s;

    debug_called(4);

    error = 0;
    tr->tr_private = tr;				/* our wait channel */
    s = splbio();
    if ((error = twe_start(tr)) != 0)
	goto out;
    tsleep(tr->tr_private, PUSER, "twwcmd", hz);	/* XXX more sensible timeout than 1s? */
    splx(s);

out:
    return(error);
}
#endif

/********************************************************************************
 * Start the command (tr) and busy-wait for it to complete.
 * This should only be used when interrupts are actually disabled (although it
 * will work if they are not).
 */
static int
twe_immediate_request(struct twe_request *tr)
{
    int		error;

    debug_called(4);

    error = 0;

    if ((error = twe_start(tr)) != 0)
	return(error);
    while (tr->tr_status == TWE_CMD_BUSY){
	twe_done(tr->tr_sc);
    }
    return(tr->tr_status != TWE_CMD_COMPLETE);
}

/********************************************************************************
 * Pull as much work off the softc's work queue as possible and give it to the
 * controller.
 */
static void
twe_startio(struct twe_softc *sc)
{
    struct twe_request	*tr;
    TWE_Command		*cmd;
    struct bio		*bp;
    int			s, error;

    debug_called(4);

    /* spin until something prevents us from doing any work */
    s = splbio();
    for (;;) {

	if (sc->twe_deferred == NULL) {
	    /* see if there's work to be done */
	    if ((bp = bioq_first(&sc->twe_bioq)) == NULL)
		break;
	    /* get a command */
	    if ((tr = twe_get_request(sc)) == NULL)
		break;

	    /* get the bio containing our work */
	    bioq_remove(&sc->twe_bioq, bp);
	    splx(s);
	
	    /* connect the bio to the command */
	    tr->tr_complete = twe_completeio;
	    tr->tr_private = bp;
	    tr->tr_data = bp->bio_data;
	    tr->tr_length = bp->bio_bcount;
	    cmd = &tr->tr_command;
#ifdef FREEBSD_4
	    if (bp->bio_flags & B_READ)
#else
	    if (bp->bio_cmd == BIO_READ)
#endif
	    {
		tr->tr_flags |= TWE_CMD_DATAIN;
		cmd->opcode = TWE_OP_READ;
	    } else {
		tr->tr_flags |= TWE_CMD_DATAOUT;
		cmd->opcode = TWE_OP_WRITE;
	    }
	
	    /* build a suitable I/O command (assumes 512-byte rounded transfers) */
	    cmd->sgl_offset = 3;
	    cmd->size = 3;
	    cmd->unit = ((struct twed_softc *)bp->bio_dev->si_drv1)->twed_drive->td_unit;
	    cmd->args.io.lba = bp->bio_pblkno;
	    cmd->count = (bp->bio_bcount + TWE_BLOCK_SIZE - 1) / TWE_BLOCK_SIZE;

	    /* map the command so the controller can work with it */
	    twe_map_request(tr);
	} else {

	    /* we previously deferred a command, try to submit it again */
	    tr = sc->twe_deferred;
	    sc->twe_deferred = NULL;
	}
	
	/* try to give command to controller */
	error = twe_start(tr);

	if (error != 0) {
	    if (error == EBUSY) {
		sc->twe_deferred = tr;		/* try it again later */
		break;				/* don't try anything more for now */
	    }
	    /* otherwise, fail the command */
	    tr->tr_status = TWE_CMD_FAILED;
	    twe_completeio(tr);
	}
	s = splbio();
    }
    splx(s);
}

/********************************************************************************
 * Handle completion of an I/O command.
 */
static void
twe_completeio(struct twe_request *tr)
{
    TWE_Command		*cmd;
    struct twe_softc	*sc = tr->tr_sc;
    struct bio		*bp = (struct bio *)tr->tr_private;
    struct twed_softc	*twed = (struct twed_softc *)bp->bio_dev->si_drv1;

    debug_called(4);

    if (tr->tr_status == TWE_CMD_COMPLETE) {
	cmd = &tr->tr_command;
	if (cmd->status != 0) {
	    bp->bio_error = EIO;
	    bp->bio_flags |= BIO_ERROR;
	    device_printf(twed->twed_dev, "command failed - 0x%x\n", cmd->status);
	}
    } else if (tr->tr_status == TWE_CMD_FAILED) {	/* could be more verbose here? */
	bp->bio_error = EIO;
	bp->bio_flags |= BIO_ERROR;
	device_printf(sc->twe_dev, "command failed submission - controller wedged\n");
    }
    twe_release_request(tr);
    twed_intr(bp);
}

/********************************************************************************
 ********************************************************************************
                                                        Command I/O to Controller
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Try to deliver (tr) to the controller.
 *
 * Can be called at any interrupt level, with or without interrupts enabled.
 */
static int
twe_start(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;
    int			i, s, done;
    u_int32_t		status_reg;

    debug_called(4);

    /* give the command a request ID */
    if (twe_get_requestid(tr))
	return(EBUSY);				/* can't handle it now, try later */

    /* mark the command as currently being processed */
    tr->tr_status = TWE_CMD_BUSY;

    /* spin briefly waiting for the controller to come ready */
    for (i = 100000, done = 0; (i > 0) && !done; i--) {
	s = splbio();
	
	/* check to see if we can post a command */
	status_reg = TWE_STATUS(sc);
	twe_check_bits(sc, status_reg);

	if (!(status_reg & TWE_STATUS_COMMAND_QUEUE_FULL)) {
	    TWE_COMMAND_QUEUE(sc, tr->tr_cmdphys);
	    done = 1;
	    /* move command to work queue */
	    TAILQ_INSERT_TAIL(&sc->twe_work, tr, tr_link);
	    if (tr->tr_complete != NULL) {
		debug(3, "queued request %d with callback %p", tr->tr_command.request_id, tr->tr_complete);
	    } else if (tr->tr_private != NULL) {
		debug(3, "queued request %d with wait channel %p", tr->tr_command.request_id, tr->tr_private);
	    } else {
		debug(3, "queued request %d for polling caller", tr->tr_command.request_id);
	    }
	}
	splx(s);	/* drop spl to allow completion interrupts */
    }

    /* command is enqueued */
    if (done)
	return(0);

    /* 
     * We couldn't get the controller to take the command; try submitting it again later.
     * This should only happen if something is wrong with the controller, or if we have
     * overestimated the number of commands it can accept.  (Should we actually reject
     * the command at this point?)
     */
    twe_release_requestid(tr);
    return(EBUSY);
}

/********************************************************************************
 * Poll the controller (sc) for completed commands.
 *
 * Can be called at any interrupt level, with or without interrupts enabled.
 */
static void
twe_done(struct twe_softc *sc)
{
    TWE_Response_Queue	rq;
    struct twe_request	*tr;
    int			s, found;
    u_int32_t		status_reg;
    
    debug_called(5);

    /* loop collecting completed commands */
    found = 0;
    s = splbio();
    for (;;) {
	status_reg = TWE_STATUS(sc);
	twe_check_bits(sc, status_reg);		/* XXX should this fail? */

	if (!(status_reg & TWE_STATUS_RESPONSE_QUEUE_EMPTY)) {
	    found = 1;
	    rq = TWE_RESPONSE_QUEUE(sc);
	    tr = sc->twe_cmdlookup[rq.u.response_id];	/* find command */
	    if (tr != NULL) {				/* paranoia */
		tr->tr_status = TWE_CMD_COMPLETE;
		debug(3, "completed request id %d with status %d", tr->tr_command.request_id, tr->tr_command.status);
		twe_release_requestid(tr);
	    } else {
		debug(2, "done event for nonbusy id %d\n", rq.u.response_id);
	    }
	} else {
	    break;					/* no response ready */
	}
    }
    splx(s);

    /* if we've completed any commands, try posting some more */
    if (found)
	twe_startio(sc);

    /* handle completion and timeouts */
    twe_complete(sc);
}

/********************************************************************************
 * Perform post-completion processing for commands on (sc).
 *
 * This is split from twe_done as it can be safely deferred and run at a lower
 * priority level should facilities for such a thing become available.
 */
static void
twe_complete(struct twe_softc *sc) 
{
    struct twe_request	*tr, *nr;
    int			s;
    
    debug_called(5);

    s = splbio();

    /*
     * Scan the list of busy/done commands, dispatch them appropriately.
     */
    tr = TAILQ_FIRST(&sc->twe_work);
    while (tr != NULL) {
	nr = TAILQ_NEXT(tr, tr_link);

	/* command has been completed in some fashion */
	if (tr->tr_status > TWE_CMD_BUSY) {
	
	    /* unmap the command's data buffer */
	    twe_unmap_request(tr);

	    /* remove from work list */
	    TAILQ_REMOVE(&sc->twe_work, tr, tr_link);

	    /* dispatch to suit command originator */
	    if (tr->tr_complete != NULL) {		/* completion callback */
		debug(2, "call completion handler %p", tr->tr_complete);
		tr->tr_complete(tr);

	    } else if (tr->tr_private != NULL) {	/* caller is asleep waiting */
		debug(2, "wake up command owner on %p", tr->tr_private);
		wakeup_one(tr->tr_private);

	    } else {					/* caller is polling command */
		debug(2, "command left for owner");
	    }
	}
	tr = nr;
    }
    splx(s);
}

/********************************************************************************
 * Wait for (status) to be set in the controller status register for up to
 * (timeout) seconds.  Returns 0 if found, nonzero if we time out.
 *
 * Note: this busy-waits, rather than sleeping, since we may be called with
 * eg. clock interrupts masked.
 */
static int
twe_wait_status(struct twe_softc *sc, u_int32_t status, int timeout)
{
    time_t	expiry;
    u_int32_t	status_reg;

    debug_called(4);

    expiry = time_second + timeout;

    do {
	status_reg = TWE_STATUS(sc);
	if (status_reg & status)	/* got the required bit(s)? */
	    return(0);
	DELAY(100000);
    } while (time_second <= expiry);

    return(1);
}

/********************************************************************************
 * Drain the response queue, which may contain responses to commands we know
 * nothing about.
 */
static int
twe_drain_response_queue(struct twe_softc *sc)
{
    TWE_Response_Queue	rq;
    u_int32_t		status_reg;

    debug_called(4);

    for (;;) {				/* XXX give up eventually? */
	status_reg = TWE_STATUS(sc);
	if (twe_check_bits(sc, status_reg))
	    return(1);
	if (status_reg & TWE_STATUS_RESPONSE_QUEUE_EMPTY)
	    return(0);
	rq = TWE_RESPONSE_QUEUE(sc);
    }
}

/********************************************************************************
 ********************************************************************************
                                                               Interrupt Handling
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Host interrupt.
 *
 * XXX what does this mean?
 */
static void
twe_host_intr(struct twe_softc *sc)
{
    debug_called(4);

    device_printf(sc->twe_dev, "host interrupt\n");
    TWE_CONTROL(sc, TWE_CONTROL_CLEAR_HOST_INTERRUPT);
}

/********************************************************************************
 * Attention interrupt.
 *
 * Signalled when the controller has one or more AENs for us.
 */
static void
twe_attention_intr(struct twe_softc *sc)
{
    debug_called(4);

    /* instigate a poll for AENs */
    if (twe_fetch_aen(sc)) {
	device_printf(sc->twe_dev, "error polling for signalled AEN\n");
    } else {
	TWE_CONTROL(sc, TWE_CONTROL_CLEAR_ATTENTION_INTERRUPT);
    }
}

/********************************************************************************
 * Command interrupt.
 *
 * Signalled when the controller can handle more commands.
 */
static void
twe_command_intr(struct twe_softc *sc)
{
    debug_called(4);

    /*
     * We don't use this, rather we try to submit commands when we receive
     * them, and when other commands have completed.  Mask it so we don't get
     * another one.
     */
    device_printf(sc->twe_dev, "command interrupt\n");
    TWE_CONTROL(sc, TWE_CONTROL_MASK_COMMAND_INTERRUPT);
}

/********************************************************************************
 * Enable the useful interrupts from the controller.
 */
static void
twe_enable_interrupts(struct twe_softc *sc)
{
    sc->twe_state |= TWE_STATE_INTEN;
    TWE_CONTROL(sc, 
	       TWE_CONTROL_CLEAR_ATTENTION_INTERRUPT |
	       TWE_CONTROL_UNMASK_RESPONSE_INTERRUPT |
	       TWE_CONTROL_ENABLE_INTERRUPTS);
}

/********************************************************************************
 * Disable interrupts from the controller.
 */
static void
twe_disable_interrupts(struct twe_softc *sc)
{
    TWE_CONTROL(sc, TWE_CONTROL_DISABLE_INTERRUPTS);
    sc->twe_state &= ~TWE_STATE_INTEN;
}

/********************************************************************************
 ********************************************************************************
                                                      Asynchronous Event Handling
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Request an AEN from the controller.
 */
static int
twe_fetch_aen(struct twe_softc *sc)
{

    debug_called(4);

    /* XXX ick, magic numbers */
    if ((twe_get_param(sc, 0x401, 2, 2, twe_handle_aen)) == NULL)
	return(EIO);
    return(0);
}

/********************************************************************************
 * Handle an AEN returned by the controller.
 */
static void
twe_handle_aen(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;
    TWE_Param		*param;
    u_int16_t		aen;

    debug_called(4);

    /* XXX check for command success somehow? */

    param = (TWE_Param *)tr->tr_data;
    aen = *(u_int16_t *)(param->data);

    free(tr->tr_data, M_DEVBUF);
    twe_release_request(tr);
    twe_enqueue_aen(sc, aen);

    /* XXX poll for more AENs? */
}

/********************************************************************************
 * Pull AENs out of the controller and park them in the queue, in a context where
 * interrupts aren't active.  Return nonzero if we encounter any errors in the
 * process of obtaining all the available AENs.
 */
static int
twe_drain_aen_queue(struct twe_softc *sc)
{
    TWE_Param	*param;
    u_int16_t	aen;

    for (;;) {
	/* XXX ick, magic numbers */
	param = twe_get_param(sc, 0x401, 2, 2, NULL);
	if (param == NULL)
	    return(1);
	aen = *(u_int16_t *)(param->data);
	if (aen == TWE_AEN_QUEUE_EMPTY)
	    return(0);
	twe_enqueue_aen(sc, aen);
    }
}

/********************************************************************************
 * Push an AEN that we've received onto the queue.
 *
 * Note that we have to lock this against reentrance, since it may be called
 * from both interrupt and non-interrupt context.
 *
 * If someone is waiting for the AEN we have, wake them up.
 */
static void
twe_enqueue_aen(struct twe_softc *sc, u_int16_t aen)
{
    int		s, next;

    debug_called(4);

    debug(1, "queueing AEN <%s>", twe_name_aen(aen));

    s = splbio();
    /* enqueue the AEN */
    next = ((sc->twe_aen_head + 1) % TWE_Q_LENGTH);
    if (next != sc->twe_aen_tail) {
	sc->twe_aen_queue[sc->twe_aen_head] = aen;
	sc->twe_aen_head = next;
    } else {
	device_printf(sc->twe_dev, "AEN queue overflow, lost AEN <%s>\n", twe_name_aen(aen));
    }

    /* anyone looking for this AEN? */
    if (sc->twe_wait_aen == aen) {
	sc->twe_wait_aen = -1;
	wakeup(&sc->twe_wait_aen);
    }
    splx(s);
}

#if 0
/********************************************************************************
 * Pop an AEN off the queue, or return -1 if there are none left.
 *
 * We are more or less interrupt-safe, so don't block interrupts.
 */
static int
twe_dequeue_aen(struct twe_softc *sc)
{
    int		result;
    
    debug_called(4);

    if (sc->twe_aen_tail == sc->twe_aen_head) {
	result = -1;
    } else {
	result = sc->twe_aen_queue[sc->twe_aen_tail];
	sc->twe_aen_tail = ((sc->twe_aen_tail + 1) % TWE_Q_LENGTH);
    }
    return(result);
}
#endif

/********************************************************************************
 * Check to see if the requested AEN is in the queue.
 *
 * XXX we could probably avoid masking interrupts here
 */
static int
twe_find_aen(struct twe_softc *sc, u_int16_t aen)
{
    int		i, s, missing;

    missing = 1;
    s = splbio();
    for (i = sc->twe_aen_tail; (i != sc->twe_aen_head) && missing; i = (i + 1) % TWE_Q_LENGTH) {
	if (sc->twe_aen_queue[i] == aen)
	    missing = 0;
    }
    return(missing);
}


#if 0	/* currently unused */
/********************************************************************************
 * Sleep waiting for at least (timeout) seconds until we see (aen) as 
 * requested.  Returns nonzero on timeout or failure.
 *
 * XXX: this should not be used in cases where there may be more than one sleeper
 *      without a mechanism for registering multiple sleepers.
 */
static int
twe_wait_aen(struct twe_softc *sc, int aen, int timeout)
{
    time_t	expiry;
    int		found, s;

    debug_called(4);

    expiry = time_second + timeout;
    found = 0;

    s = splbio();
    sc->twe_wait_aen = aen;
    do {
	twe_fetch_aen(sc);
	tsleep(&sc->twe_wait_aen, PZERO, "twewaen", hz);
	if (sc->twe_wait_aen == -1)
	    found = 1;
    } while ((time_second <= expiry) && !found);
    splx(s);
    return(!found);
}
#endif

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
 * On the other hand, using malloc ensures that the command structure at the top
 * of the request is aligned within the controller's constraints (64 bytes).
 *
 * If possible, we recycle a command buffer that's been used before.
 *
 * XXX currently, commands are mapped into controller space just before being
 * handed to the controller.  It may be more efficient to do that here.
 */
static struct twe_request *
twe_get_request(struct twe_softc *sc)
{
    struct twe_request	*tr;
    int			s, error;

    debug_called(4);

    /* try to reuse an old buffer */
    s = splbio();
    if ((tr = TAILQ_FIRST(&sc->twe_freecmds)) != NULL)
	TAILQ_REMOVE(&sc->twe_freecmds, tr, tr_link);
    splx(s);

    /* allocate a new command buffer? */
    if (tr == NULL) {
	tr = (struct twe_request *)malloc(sizeof(*tr), M_DEVBUF, M_NOWAIT);
	if (tr != NULL) {
	    bzero(tr, sizeof(*tr));
	    tr->tr_sc = sc;
	    error = bus_dmamap_create(sc->twe_buffer_dmat, 0, &tr->tr_cmdmap);
	    if (error) {
		free(tr, M_DEVBUF);
		return(NULL);
	    }
	    error = bus_dmamap_create(sc->twe_buffer_dmat, 0, &tr->tr_dmamap);
	    if (error) {
		bus_dmamap_destroy(sc->twe_buffer_dmat, tr->tr_cmdmap);
		free(tr, M_DEVBUF);
		return(NULL);
	    }
	}
    }

    /* initialise some fields to their defaults */
    tr->tr_status = TWE_CMD_SETUP;		/* command is in setup phase */
    tr->tr_flags = 0;
    tr->tr_command.host_id = 0;			/* not used */
    tr->tr_command.status = 0;			/* before submission to controller */
    tr->tr_command.flags = 0;			/* not used */
    return(tr);
}

/********************************************************************************
 * Release a command buffer for recycling.
 *
 * XXX It might be a good idea to limit the number of commands we save for reuse
 *     if it's shown that this list bloats out massively.
 */
static void
twe_release_request(struct twe_request *tr)
{
    int		s;
    
    debug_called(4);

    s = splbio();
    TAILQ_INSERT_HEAD(&tr->tr_sc->twe_freecmds, tr, tr_link);
    splx(s);
}

/********************************************************************************
 * Permanently discard a command buffer.
 */
static void
twe_free_request(struct twe_request *tr) 
{
    struct twe_softc	*sc = tr->tr_sc;
    
    debug_called(4);

    bus_dmamap_destroy(sc->twe_buffer_dmat, tr->tr_cmdmap);
    bus_dmamap_destroy(sc->twe_buffer_dmat, tr->tr_dmamap);
    free(tr, M_DEVBUF);
}

/********************************************************************************
 * Allocate a request ID for a command about to be submitted.
 */
static int
twe_get_requestid(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;
    int			i, s, result;

    debug_called(4);

    s = splbio();
    result = 1;

    /* XXX linear search is slow */
    for (i = 0; i < TWE_Q_LENGTH; i++) {
	if (sc->twe_cmdlookup[i] == NULL) {
	    tr->tr_command.request_id = i;
	    sc->twe_cmdlookup[i] = tr;
	    result = 0;
	    break;
	}
    }
    splx(s);

    return(result);
}

/********************************************************************************
 * Free a command's request ID for reuse.
 */
static void
twe_release_requestid(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;

    debug_called(4);

    sc->twe_cmdlookup[tr->tr_command.request_id] = 0;	/* XXX atomic? */
}

/********************************************************************************
 * Map/unmap (tr)'s command and data in the controller's addressable space.
 *
 * These routines ensure that the data which the controller is going to try to
 * access is actually visible to the controller, in a machine-independant 
 * fasion.  Due to a hardware limitation, I/O buffers must be 512-byte aligned
 * and we take care of that here as well.
 */
static void
twe_setup_data_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct twe_request	*tr = (struct twe_request *)arg;
    TWE_Command		*cmd = &tr->tr_command;
    int			i;

    debug_called(4);

    /* save base of first segment in command (applicable if there only one segment) */
    tr->tr_dataphys = segs[0].ds_addr;

    /* correct command size for s/g list size */
    tr->tr_command.size += 2 * nsegments;

    /*
     * Due to the fact that parameter and I/O commands have the scatter/gather list in
     * different places, we need to determine which sort of command this actually is
     * before we can populate it correctly.
     */
    switch(cmd->sgl_offset) {
    case 2:
	for (i = 0; i < nsegments; i++) {
	    cmd->args.param.sgl[i].address = segs[i].ds_addr;
	    cmd->args.param.sgl[i].length = segs[i].ds_len;
	}
	for (; i < TWE_MAX_SGL_LENGTH; i++) {		/* XXX necessary? */
	    cmd->args.param.sgl[i].address = 0;
	    cmd->args.param.sgl[i].length = 0;
	}
	break;
    case 3:
	for (i = 0; i < nsegments; i++) {
	    cmd->args.io.sgl[i].address = segs[i].ds_addr;
	    cmd->args.io.sgl[i].length = segs[i].ds_len;
	}
	for (; i < TWE_MAX_SGL_LENGTH; i++) {		/* XXX necessary? */
	    cmd->args.io.sgl[i].address = 0;
	    cmd->args.io.sgl[i].length = 0;
	}
	break;
    default:
	/* no s/g list, nothing to do */
    }
}

static void
twe_setup_request_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct twe_request	*tr = (struct twe_request *)arg;

    debug_called(4);

    /* command can't cross a page boundary */
    tr->tr_cmdphys = segs[0].ds_addr;
}

static void
twe_map_request(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;

    debug_called(4);


    /*
     * Map the command into bus space.
     */
    bus_dmamap_load(sc->twe_buffer_dmat, tr->tr_cmdmap, &tr->tr_command, sizeof(tr->tr_command), 
		    twe_setup_request_dmamap, tr, 0);
    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_cmdmap, BUS_DMASYNC_PREWRITE);

    /*
     * If the command involves data, map that too.
     */
    if (tr->tr_data != NULL) {

	/* 
	 * Data must be 64-byte aligned; allocate a fixup buffer if it's not.
	 */
	if (((vm_offset_t)tr->tr_data % TWE_ALIGNMENT) != 0) {
	    tr->tr_realdata = tr->tr_data;				/* save pointer to 'real' data */
	    tr->tr_flags |= TWE_CMD_ALIGNBUF;
	    tr->tr_data = malloc(tr->tr_length, M_DEVBUF, M_NOWAIT);	/* XXX check result here */
	}
	
	/*
	 * Map the data buffer into bus space and build the s/g list.
	 */
	bus_dmamap_load(sc->twe_buffer_dmat, tr->tr_dmamap, tr->tr_data, tr->tr_length, 
			twe_setup_data_dmamap, tr, 0);
	if (tr->tr_flags & TWE_CMD_DATAIN)
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_PREREAD);
	if (tr->tr_flags & TWE_CMD_DATAOUT) {
	    /* if we're using an alignment buffer, and we're writing data, copy the real data out */
	    if (tr->tr_flags & TWE_CMD_ALIGNBUF)
		bcopy(tr->tr_realdata, tr->tr_data, tr->tr_length);
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_PREWRITE);
	}
    }
}

static void
twe_unmap_request(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;

    debug_called(4);

    /*
     * Unmap the command from bus space.
     */
    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_cmdmap, BUS_DMASYNC_POSTWRITE);
    bus_dmamap_unload(sc->twe_buffer_dmat, tr->tr_cmdmap); 

    /*
     * If the command involved data, unmap that too.
     */
    if (tr->tr_data != NULL) {
	
	if (tr->tr_flags & TWE_CMD_DATAIN) {
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_POSTREAD);
	    /* if we're using an alignment buffer, and we're reading data, copy the real data in */
	    if (tr->tr_flags & TWE_CMD_ALIGNBUF)
		bcopy(tr->tr_data, tr->tr_realdata, tr->tr_length);
	}
	if (tr->tr_flags & TWE_CMD_DATAOUT)
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->twe_buffer_dmat, tr->tr_dmamap); 
    }

    /* free alignment buffer if it was used */
    if (tr->tr_flags & TWE_CMD_ALIGNBUF) {
	free(tr->tr_data, M_DEVBUF);
	tr->tr_data = tr->tr_realdata;		/* restore 'real' data pointer */
    }
}

/********************************************************************************
 ********************************************************************************
                                                                        Debugging
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Complain if the status bits aren't what we're expecting.
 */
static int
twe_check_bits(struct twe_softc *sc, u_int32_t status_reg)
{
    int		result;

    result = 0;
    if ((status_reg & TWE_STATUS_EXPECTED_BITS) != TWE_STATUS_EXPECTED_BITS) {
	device_printf(sc->twe_dev, "missing expected status bit(s) %b\n", ~status_reg & TWE_STATUS_EXPECTED_BITS, 
		      TWE_STATUS_BITS_DESCRIPTION);
	result = 1;
    }

    if ((status_reg & TWE_STATUS_UNEXPECTED_BITS) != 0) {
	device_printf(sc->twe_dev, "unexpected status bit(s) %b\n", status_reg & TWE_STATUS_UNEXPECTED_BITS, 
		      TWE_STATUS_BITS_DESCRIPTION);
	result = 1;
    }
    return(result);
}	

/********************************************************************************
 * Return a string naming (aen).
 */
static struct {
    u_int16_t	aen;
    char	*desc;
} twe_aen_names[] = {
    {0x0000,	"queue empty"},
    {0x0001,	"soft reset"},
    {0x0002,	"degraded mirror"},
    {0x0003,	"controller error"},
    {0x0004,	"rebuild fail"},
    {0x0005,	"rebuild done"},
    {0x00ff,	"aen queue full"},
    {0, NULL}
};

static char *
twe_name_aen(u_int16_t aen)
{
    int		i;
    static char	buf[16];

    for (i = 0; twe_aen_names[i].desc != NULL; i++)
	if (twe_aen_names[i].aen == aen)
	    return(twe_aen_names[i].desc);

    sprintf(buf, "0x%x", aen);
    return(buf);
}

#if 0
/********************************************************************************
 * Return a string naming (opcode).
 */
static struct {
    u_int8_t	opcode;
    char	*desc;
} twe_opcode_names[] = {
    {0x00,	"nop"},
    {0x01,	"init connection"},
    {0x02,	"read"},
    {0x03,	"write"},
    {0x04,	"verify"},
    {0x12,	"get param"},
    {0x13,	"set param"},
    {0x1a,	"sector info"},
    {0x1c,	"listen"},
    {0, NULL}
};

static char *
twe_name_opcode(u_int8_t opcode)
{
    int		i;
    static char	buf[16];

    for (i = 0; twe_opcode_names[i].desc != NULL; i++)
	if (twe_opcode_names[i].opcode == opcode)
	    return(twe_opcode_names[i].desc);

    sprintf(buf, "0x%x", opcode);
    return(buf);
}

/********************************************************************************
 * Print a request/command in human-readable format.
 */
static void
twe_print_request(struct twe_request *tr)
{
    device_t	dev = tr->tr_sc->twe_dev;
    TWE_Command	*cmd = &tr->tr_command;
    int		i;

    device_printf(dev, "CMD: request_id %d  opcode <%s>  size %d  unit %d  host_id %d\n", 
		  cmd->request_id, twe_name_opcode(cmd->opcode), cmd->size, cmd->unit, cmd->host_id);
    device_printf(dev, " status %d  flags 0x%x  count %d  sgl_offset %d\n", 
		  cmd->status, cmd->flags, cmd->count, cmd->sgl_offset);
    switch(cmd->sgl_offset) {
    case 3:
	device_printf(dev, " lba %d\n", cmd->args.io.lba);
	for (i = 0; (i < TWE_MAX_SGL_LENGTH) && (cmd->args.io.sgl[i].length != 0); i++)
	    device_printf(dev, "  %d: 0x%x/%d\n", 
			  i, cmd->args.io.sgl[i].address, cmd->args.io.sgl[i].length);
	break;

    case 2:
	for (i = 0; (i < TWE_MAX_SGL_LENGTH) && (cmd->args.param.sgl[i].length != 0); i++)
	    device_printf(dev, "  %d: 0x%x/%d\n", 
			  i, cmd->args.param.sgl[i].address, cmd->args.param.sgl[i].length);
	break;

    default:
	device_printf(dev, " response queue pointer 0x%x\n", 
		      cmd->args.init_connection.response_queue_pointer);
    }
    device_printf(dev, " tr_command %p/0x%x  tr_data %p/0x%x,%d\n", 
		  tr, tr->tr_cmdphys, tr->tr_data, tr->tr_dataphys, tr->tr_length);
    device_printf(dev, " tr_status %d  tr_flags 0x%x  tr_complete %p  tr_private %p\n", 
		  tr->tr_status, tr->tr_flags, tr->tr_complete, tr->tr_private);
}

/********************************************************************************
 * Print current controller status, call from DDB.
 */
void
twe_report(void)
{
    u_int32_t		status_reg;
    struct twe_softc	*sc;
    int			i, s;

    s = splbio();
    for (i = 0; (sc = devclass_get_softc(twe_devclass, i)) != NULL; i++) {
	status_reg = TWE_STATUS(sc);
	device_printf(sc->twe_dev, "status %b\n", status_reg, TWE_STATUS_BITS_DESCRIPTION);
    }
    splx(s);
}
#endif
