/*-
 * Copyright (c) 2001 Michael Smith
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
 * Common Interface for SCSI-3 Support driver.
 *
 * CISS claims to provide a common interface between a generic SCSI
 * transport and an intelligent host adapter.
 *
 * This driver supports CISS as defined in the document "CISS Command
 * Interface for SCSI-3 Support Open Specification", Version 1.04,
 * Valence Number 1, dated 20001127, produced by Compaq Computer
 * Corporation.  This document appears to be a hastily and somewhat
 * arbitrarlily cut-down version of a larger (and probably even more
 * chaotic and inconsistent) Compaq internal document.  Various
 * details were also gleaned from Compaq's "cciss" driver for Linux.
 *
 * We provide a shim layer between the CISS interface and CAM,
 * offloading most of the queueing and being-a-disk chores onto CAM.
 * Entry to the driver is via the PCI bus attachment (ciss_probe,
 * ciss_attach, etc) and via the CAM interface (ciss_cam_action,
 * ciss_cam_poll).  The Compaq CISS adapters are, however, poor SCSI
 * citizens and we have to fake up some responses to get reasonable
 * behaviour out of them.  In addition, the CISS command set is by no
 * means adequate to support the functionality of a RAID controller,
 * and thus the supported Compaq adapters utilise portions of the
 * control protocol from earlier Compaq adapter families.
 *
 * Note that we only support the "simple" transport layer over PCI.
 * This interface (ab)uses the I2O register set (specifically the post
 * queues) to exchange commands with the adapter.  Other interfaces
 * are available, but we aren't supposed to know about them, and it is
 * dubious whether they would provide major performance improvements
 * except under extreme load.
 * 
 * Currently the only supported CISS adapters are the Compaq Smart
 * Array 5* series (5300, 5i, 532).  Even with only three adapters,
 * Compaq still manage to have interface variations.
 *
 *
 * Thanks must go to Fred Harris and Darryl DeVinney at Compaq, as
 * well as Paul Saab at Yahoo! for their assistance in making this
 * driver happen.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/stat.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <machine/clock.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/ciss/cissreg.h>
#include <dev/ciss/cissvar.h>
#include <dev/ciss/cissio.h>

MALLOC_DEFINE(CISS_MALLOC_CLASS, "ciss_data", "ciss internal data buffers");

/* pci interface */
static int	ciss_lookup(device_t dev);
static int	ciss_probe(device_t dev);
static int	ciss_attach(device_t dev);
static int	ciss_detach(device_t dev);
static int	ciss_shutdown(device_t dev);

/* (de)initialisation functions, control wrappers */
static int	ciss_init_pci(struct ciss_softc *sc);
static int	ciss_wait_adapter(struct ciss_softc *sc);
static int	ciss_flush_adapter(struct ciss_softc *sc);
static int	ciss_init_requests(struct ciss_softc *sc);
static void	ciss_command_map_helper(void *arg, bus_dma_segment_t *segs,
					int nseg, int error);
static int	ciss_identify_adapter(struct ciss_softc *sc);
static int	ciss_init_logical(struct ciss_softc *sc);
static int	ciss_identify_logical(struct ciss_softc *sc, struct ciss_ldrive *ld);
static int	ciss_get_ldrive_status(struct ciss_softc *sc,  struct ciss_ldrive *ld);
static int	ciss_update_config(struct ciss_softc *sc);
static int	ciss_accept_media(struct ciss_softc *sc, int ldrive, int async);
static void	ciss_accept_media_complete(struct ciss_request *cr);
static void	ciss_free(struct ciss_softc *sc);

/* request submission/completion */
static int	ciss_start(struct ciss_request *cr);
static void	ciss_done(struct ciss_softc *sc);
static void	ciss_intr(void *arg);
static void	ciss_complete(struct ciss_softc *sc);
static int	ciss_report_request(struct ciss_request *cr, int *command_status,
				    int *scsi_status);
static int	ciss_synch_request(struct ciss_request *cr, int timeout);
static int	ciss_poll_request(struct ciss_request *cr, int timeout);
static int	ciss_wait_request(struct ciss_request *cr, int timeout);
#if 0
static int	ciss_abort_request(struct ciss_request *cr);
#endif

/* request queueing */
static int	ciss_get_request(struct ciss_softc *sc, struct ciss_request **crp);
static void	ciss_preen_command(struct ciss_request *cr);
static void 	ciss_release_request(struct ciss_request *cr);

/* request helpers */
static int	ciss_get_bmic_request(struct ciss_softc *sc, struct ciss_request **crp,
				      int opcode, void **bufp, size_t bufsize);
static int	ciss_user_command(struct ciss_softc *sc, IOCTL_Command_struct *ioc);

/* DMA map/unmap */
static int	ciss_map_request(struct ciss_request *cr);
static void	ciss_request_map_helper(void *arg, bus_dma_segment_t *segs,
					int nseg, int error);
static void	ciss_unmap_request(struct ciss_request *cr);

/* CAM interface */
static int	ciss_cam_init(struct ciss_softc *sc);
static void	ciss_cam_rescan_target(struct ciss_softc *sc, int target);
static void	ciss_cam_rescan_all(struct ciss_softc *sc);
static void	ciss_cam_rescan_callback(struct cam_periph *periph, union ccb *ccb);
static void	ciss_cam_action(struct cam_sim *sim, union ccb *ccb);
static int	ciss_cam_action_io(struct cam_sim *sim, struct ccb_scsiio *csio);
static int	ciss_cam_emulate(struct ciss_softc *sc, struct ccb_scsiio *csio);
static void	ciss_cam_poll(struct cam_sim *sim);
static void	ciss_cam_complete(struct ciss_request *cr);
static void	ciss_cam_complete_fixup(struct ciss_softc *sc, struct ccb_scsiio *csio);
static struct cam_periph *ciss_find_periph(struct ciss_softc *sc, int target);
static int	ciss_name_device(struct ciss_softc *sc, int target);

/* periodic status monitoring */
static void	ciss_periodic(void *arg);
static void	ciss_notify_event(struct ciss_softc *sc);
static void	ciss_notify_complete(struct ciss_request *cr);
static int	ciss_notify_abort(struct ciss_softc *sc);
static int	ciss_notify_abort_bmic(struct ciss_softc *sc);
static void	ciss_notify_logical(struct ciss_softc *sc, struct ciss_notify *cn);
static void	ciss_notify_physical(struct ciss_softc *sc, struct ciss_notify *cn);

/* debugging output */
static void	ciss_print_request(struct ciss_request *cr);
static void	ciss_print_ldrive(struct ciss_softc *sc, struct ciss_ldrive *ld);
static const char *ciss_name_ldrive_status(int status);
static int	ciss_decode_ldrive_status(int status);
static const char *ciss_name_ldrive_org(int org);
static const char *ciss_name_command_status(int status);

/*
 * PCI bus interface.
 */
static device_method_t ciss_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	ciss_probe),
    DEVMETHOD(device_attach,	ciss_attach),
    DEVMETHOD(device_detach,	ciss_detach),
    DEVMETHOD(device_shutdown,	ciss_shutdown),
    { 0, 0 }
};

static driver_t ciss_pci_driver = {
    "ciss",
    ciss_methods,
    sizeof(struct ciss_softc)
};

static devclass_t	ciss_devclass;
DRIVER_MODULE(ciss, pci, ciss_pci_driver, ciss_devclass, 0, 0);

/*
 * Control device interface.
 */
static d_open_t		ciss_open;
static d_close_t	ciss_close;
static d_ioctl_t	ciss_ioctl;

#define CISS_CDEV_MAJOR  166

static struct cdevsw ciss_cdevsw = {
	.d_open =	ciss_open,
	.d_close =	ciss_close,
	.d_ioctl =	ciss_ioctl,
	.d_name =	"ciss",
	.d_maj =	CISS_CDEV_MAJOR,
};

/************************************************************************
 * CISS adapters amazingly don't have a defined programming interface
 * value.  (One could say some very despairing things about PCI and
 * people just not getting the general idea.)  So we are forced to
 * stick with matching against subvendor/subdevice, and thus have to
 * be updated for every new CISS adapter that appears.
 */
#define CISS_BOARD_SA5	(1<<0)
#define CISS_BOARD_SA5B	(1<<1)

static struct
{
    u_int16_t	subvendor;
    u_int16_t	subdevice;
    int		flags;
    char	*desc;
} ciss_vendor_data[] = {
    { 0x0e11, 0x4070, CISS_BOARD_SA5,	"Compaq Smart Array 5300" },
    { 0x0e11, 0x4080, CISS_BOARD_SA5B,	"Compaq Smart Array 5i" },
    { 0x0e11, 0x4082, CISS_BOARD_SA5B,	"Compaq Smart Array 532" },
    { 0x0e11, 0x4083, CISS_BOARD_SA5B,	"HP Smart Array 5312" },
    { 0x0e11, 0x409A, CISS_BOARD_SA5,	"HP Smart Array 641" },
    { 0x0e11, 0x409B, CISS_BOARD_SA5,	"HP Smart Array 642" },
    { 0x0e11, 0x409C, CISS_BOARD_SA5,	"HP Smart Array 6400" },
    { 0x0e11, 0x409D, CISS_BOARD_SA5,	"HP Smart Array 6400 EM" },
    { 0, 0, 0, NULL }
};

/************************************************************************
 * Find a match for the device in our list of known adapters.
 */
static int
ciss_lookup(device_t dev)
{
    int 	i;
    
    for (i = 0; ciss_vendor_data[i].desc != NULL; i++)
	if ((pci_get_subvendor(dev) == ciss_vendor_data[i].subvendor) &&
	    (pci_get_subdevice(dev) == ciss_vendor_data[i].subdevice)) {
	    return(i);
	}
    return(-1);
}

/************************************************************************
 * Match a known CISS adapter.
 */
static int
ciss_probe(device_t dev)
{
    int		i;
    
    i = ciss_lookup(dev);
    if (i != -1) {
	device_set_desc(dev, ciss_vendor_data[i].desc);
	return(-10);
    }
    return(ENOENT);
}	

/************************************************************************
 * Attach the driver to this adapter.
 */
static int
ciss_attach(device_t dev)
{
    struct ciss_softc	*sc;
    int			i, error;

    debug_called(1);

#ifdef CISS_DEBUG
    /* print structure/union sizes */
    debug_struct(ciss_command);
    debug_struct(ciss_header);
    debug_union(ciss_device_address);
    debug_struct(ciss_cdb);
    debug_struct(ciss_report_cdb);
    debug_struct(ciss_notify_cdb);
    debug_struct(ciss_notify);
    debug_struct(ciss_message_cdb);
    debug_struct(ciss_error_info_pointer);
    debug_struct(ciss_error_info);
    debug_struct(ciss_sg_entry);
    debug_struct(ciss_config_table);
    debug_struct(ciss_bmic_cdb);
    debug_struct(ciss_bmic_id_ldrive);
    debug_struct(ciss_bmic_id_lstatus);
    debug_struct(ciss_bmic_id_table);
    debug_struct(ciss_bmic_id_pdrive);
    debug_struct(ciss_bmic_blink_pdrive);
    debug_struct(ciss_bmic_flush_cache);
    debug_const(CISS_MAX_REQUESTS);
    debug_const(CISS_MAX_LOGICAL);
    debug_const(CISS_INTERRUPT_COALESCE_DELAY);
    debug_const(CISS_INTERRUPT_COALESCE_COUNT);
    debug_const(CISS_COMMAND_ALLOC_SIZE);
    debug_const(CISS_COMMAND_SG_LENGTH);

    debug_type(cciss_pci_info_struct);
    debug_type(cciss_coalint_struct);
    debug_type(cciss_coalint_struct);
    debug_type(NodeName_type);
    debug_type(NodeName_type);
    debug_type(Heartbeat_type);
    debug_type(BusTypes_type);
    debug_type(FirmwareVer_type);
    debug_type(DriverVer_type);
    debug_type(IOCTL_Command_struct);
#endif

    sc = device_get_softc(dev);
    sc->ciss_dev = dev;

    /*
     * Work out adapter type.
     */
    i = ciss_lookup(dev);
    if (ciss_vendor_data[i].flags & CISS_BOARD_SA5) {
	sc->ciss_interrupt_mask = CISS_TL_SIMPLE_INTR_OPQ_SA5;
    } else if (ciss_vendor_data[i].flags & CISS_BOARD_SA5B) {
	sc->ciss_interrupt_mask = CISS_TL_SIMPLE_INTR_OPQ_SA5B;
    } else {
	/* really an error on our part */
	ciss_printf(sc, "unable to determine hardware type\n");
	error = ENXIO;
	goto out;
    }
	
    /*
     * Do PCI-specific init.
     */
    if ((error = ciss_init_pci(sc)) != 0)
	goto out;

    /*
     * Initialise driver queues.
     */
    ciss_initq_free(sc);
    ciss_initq_busy(sc);
    ciss_initq_complete(sc);

    /*
     * Initialise command/request pool.
     */
    if ((error = ciss_init_requests(sc)) != 0)
	goto out;

    /*
     * Get adapter information.
     */
    if ((error = ciss_identify_adapter(sc)) != 0)
	goto out;
    
    /*
     * Build our private table of logical devices.
     */
    if ((error = ciss_init_logical(sc)) != 0)
	goto out;

    /*
     * Enable interrupts so that the CAM scan can complete.
     */
    CISS_TL_SIMPLE_ENABLE_INTERRUPTS(sc);

    /*
     * Initialise the CAM interface.
     */
    if ((error = ciss_cam_init(sc)) != 0)
	goto out;

    /*
     * Start the heartbeat routine and event chain.
     */
    ciss_periodic(sc);

   /*
     * Create the control device.
     */
    sc->ciss_dev_t = make_dev(&ciss_cdevsw, device_get_unit(sc->ciss_dev),
			      UID_ROOT, GID_OPERATOR, S_IRUSR | S_IWUSR,
			      "ciss%d", device_get_unit(sc->ciss_dev));
    sc->ciss_dev_t->si_drv1 = sc;

    /*
     * The adapter is running; synchronous commands can now sleep
     * waiting for an interrupt to signal completion.
     */
    sc->ciss_flags |= CISS_FLAG_RUNNING;

    error = 0;
 out:
    if (error != 0)
	ciss_free(sc);
    return(error);
}

/************************************************************************
 * Detach the driver from this adapter.
 */
static int
ciss_detach(device_t dev)
{
    struct ciss_softc	*sc = device_get_softc(dev);

    debug_called(1);
    
    /* flush adapter cache */
    ciss_flush_adapter(sc);

    destroy_dev(sc->ciss_dev_t);

    /* release all resources */
    ciss_free(sc);

    return(0);
    
}

/************************************************************************
 * Prepare adapter for system shutdown.
 */
static int
ciss_shutdown(device_t dev)
{
    struct ciss_softc	*sc = device_get_softc(dev);

    debug_called(1);

    /* flush adapter cache */
    ciss_flush_adapter(sc);

    return(0);
}

/************************************************************************
 * Perform PCI-specific attachment actions.
 */
static int
ciss_init_pci(struct ciss_softc *sc)
{
    uintptr_t		cbase, csize, cofs;
    int			error;

    debug_called(1);

    /*
     * Allocate register window first (we need this to find the config
     * struct).
     */
    error = ENXIO;
    sc->ciss_regs_rid = CISS_TL_SIMPLE_BAR_REGS;
    if ((sc->ciss_regs_resource =
	 bus_alloc_resource(sc->ciss_dev, SYS_RES_MEMORY, &sc->ciss_regs_rid,
			    0, ~0, 1, RF_ACTIVE)) == NULL) {
	ciss_printf(sc, "can't allocate register window\n");
	return(ENXIO);
    }
    sc->ciss_regs_bhandle = rman_get_bushandle(sc->ciss_regs_resource);
    sc->ciss_regs_btag = rman_get_bustag(sc->ciss_regs_resource);
    
    /*
     * Find the BAR holding the config structure.  If it's not the one
     * we already mapped for registers, map it too.
     */
    sc->ciss_cfg_rid = CISS_TL_SIMPLE_READ(sc, CISS_TL_SIMPLE_CFG_BAR) & 0xffff;
    if (sc->ciss_cfg_rid != sc->ciss_regs_rid) {
	if ((sc->ciss_cfg_resource =
	     bus_alloc_resource(sc->ciss_dev, SYS_RES_MEMORY, &sc->ciss_cfg_rid,
				0, ~0, 1, RF_ACTIVE)) == NULL) {
	    ciss_printf(sc, "can't allocate config window\n");
	    return(ENXIO);
	}
	cbase = (uintptr_t)rman_get_virtual(sc->ciss_cfg_resource);
	csize = rman_get_end(sc->ciss_cfg_resource) -
	    rman_get_start(sc->ciss_cfg_resource) + 1;
    } else {
	cbase = (uintptr_t)rman_get_virtual(sc->ciss_regs_resource);
	csize = rman_get_end(sc->ciss_regs_resource) -
	    rman_get_start(sc->ciss_regs_resource) + 1;
    }
    cofs = CISS_TL_SIMPLE_READ(sc, CISS_TL_SIMPLE_CFG_OFF);
    
    /*
     * Use the base/size/offset values we just calculated to
     * sanity-check the config structure.  If it's OK, point to it.
     */
    if ((cofs + sizeof(struct ciss_config_table)) > csize) {
	ciss_printf(sc, "config table outside window\n");
	return(ENXIO);
    }
    sc->ciss_cfg = (struct ciss_config_table *)(cbase + cofs);
    debug(1, "config struct at %p", sc->ciss_cfg);
    
    /*
     * Validate the config structure.  If we supported other transport
     * methods, we could select amongst them at this point in time.
     */
    if (strncmp(sc->ciss_cfg->signature, "CISS", 4)) {
	ciss_printf(sc, "config signature mismatch (got '%c%c%c%c')\n",
		    sc->ciss_cfg->signature[0], sc->ciss_cfg->signature[1],
		    sc->ciss_cfg->signature[2], sc->ciss_cfg->signature[3]);
	return(ENXIO);
    }
    if ((sc->ciss_cfg->valence < CISS_MIN_VALENCE) ||
	(sc->ciss_cfg->valence > CISS_MAX_VALENCE)) {
	ciss_printf(sc, "adapter interface specification (%d) unsupported\n", 
		    sc->ciss_cfg->valence);
	return(ENXIO);
    }

    /*
     * Put the board into simple mode, and tell it we're using the low
     * 4GB of RAM.  Set the default interrupt coalescing options.
     */
    if (!(sc->ciss_cfg->supported_methods & CISS_TRANSPORT_METHOD_SIMPLE)) {
	ciss_printf(sc, "adapter does not support 'simple' transport layer\n");
	return(ENXIO);
    }
    sc->ciss_cfg->requested_method = CISS_TRANSPORT_METHOD_SIMPLE;
    sc->ciss_cfg->command_physlimit = 0;
    sc->ciss_cfg->interrupt_coalesce_delay = CISS_INTERRUPT_COALESCE_DELAY;
    sc->ciss_cfg->interrupt_coalesce_count = CISS_INTERRUPT_COALESCE_COUNT;

    if (ciss_update_config(sc)) {
	ciss_printf(sc, "adapter refuses to accept config update (IDBR 0x%x)\n",
		    CISS_TL_SIMPLE_READ(sc, CISS_TL_SIMPLE_IDBR));
	return(ENXIO);
    }
    if (!(sc->ciss_cfg->active_method != CISS_TRANSPORT_METHOD_SIMPLE)) {
	ciss_printf(sc,
		    "adapter refuses to go into 'simple' transport mode (0x%x, 0x%x)\n",
		    sc->ciss_cfg->supported_methods, sc->ciss_cfg->active_method);
	return(ENXIO);
    }

    /*
     * Wait for the adapter to come ready.
     */
    if ((error = ciss_wait_adapter(sc)) != 0)
	return(error);

    /*
     * Turn off interrupts before we go routing anything.
     */
    CISS_TL_SIMPLE_DISABLE_INTERRUPTS(sc);
    
    /*
     * Allocate and set up our interrupt.
     */
    sc->ciss_irq_rid = 0;
    if ((sc->ciss_irq_resource =
	 bus_alloc_resource(sc->ciss_dev, SYS_RES_IRQ, &sc->ciss_irq_rid, 0, ~0, 1, 
			    RF_ACTIVE | RF_SHAREABLE)) == NULL) {
	ciss_printf(sc, "can't allocate interrupt\n");
	return(ENXIO);
    }
    if (bus_setup_intr(sc->ciss_dev, sc->ciss_irq_resource, INTR_TYPE_CAM, ciss_intr, sc,
		       &sc->ciss_intr)) {
	ciss_printf(sc, "can't set up interrupt\n");
	return(ENXIO);
    }

    /*
     * Allocate the parent bus DMA tag appropriate for our PCI
     * interface.
     * 
     * Note that "simple" adapters can only address within a 32-bit
     * span.
     */
    if (bus_dma_tag_create(NULL, 			/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   MAXBSIZE, CISS_COMMAND_SG_LENGTH,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   BUS_DMA_ALLOCNOW,		/* flags */
			   NULL, NULL,			/* lockfunc, lockarg */
			   &sc->ciss_parent_dmat)) {
	ciss_printf(sc, "can't allocate parent DMA tag\n");
	return(ENOMEM);
    }

    /*
     * Create DMA tag for mapping buffers into adapter-addressable
     * space.
     */
    if (bus_dma_tag_create(sc->ciss_parent_dmat, 	/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   MAXBSIZE, CISS_COMMAND_SG_LENGTH,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   busdma_lock_mutex, &Giant,	/* lockfunc, lockarg */
			   &sc->ciss_buffer_dmat)) {
	ciss_printf(sc, "can't allocate buffer DMA tag\n");
	return(ENOMEM);
    }
    return(0);
}

/************************************************************************
 * Wait for the adapter to come ready.
 */
static int
ciss_wait_adapter(struct ciss_softc *sc)
{
    int		i;

    debug_called(1);
    
    /*
     * Wait for the adapter to come ready.
     */
    if (!(sc->ciss_cfg->active_method & CISS_TRANSPORT_METHOD_READY)) {
	ciss_printf(sc, "waiting for adapter to come ready...\n");
	for (i = 0; !(sc->ciss_cfg->active_method & CISS_TRANSPORT_METHOD_READY); i++) {
	    DELAY(1000000);	/* one second */
	    if (i > 30) {
		ciss_printf(sc, "timed out waiting for adapter to come ready\n");
		return(EIO);
	    }
	}
    }
    return(0);
}

/************************************************************************
 * Flush the adapter cache.
 */
static int
ciss_flush_adapter(struct ciss_softc *sc)
{
    struct ciss_request			*cr;
    struct ciss_bmic_flush_cache	*cbfc;
    int					error, command_status;

    debug_called(1);

    cr = NULL;
    cbfc = NULL;

    /*
     * Build a BMIC request to flush the cache.  We don't disable
     * it, as we may be going to do more I/O (eg. we are emulating
     * the Synchronise Cache command).
     */
    if ((cbfc = malloc(sizeof(*cbfc), CISS_MALLOC_CLASS, M_NOWAIT | M_ZERO)) == NULL) {
	error = ENOMEM;
	goto out;
    }
    if ((error = ciss_get_bmic_request(sc, &cr, CISS_BMIC_FLUSH_CACHE,
				       (void **)&cbfc, sizeof(*cbfc))) != 0)
	goto out;

    /*
     * Submit the request and wait for it to complete.
     */
    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "error sending BMIC FLUSH_CACHE command (%d)\n", error);
	goto out;
    }
    
    /*
     * Check response.
     */
    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:
	break;
    default:
	ciss_printf(sc, "error flushing cache (%s)\n",  
		    ciss_name_command_status(command_status));
	error = EIO;
	goto out;
    }

out:
    if (cbfc != NULL)
	free(cbfc, CISS_MALLOC_CLASS);
    if (cr != NULL)
	ciss_release_request(cr);
    return(error);
}

/************************************************************************
 * Allocate memory for the adapter command structures, initialise
 * the request structures.
 *
 * Note that the entire set of commands are allocated in a single
 * contiguous slab.
 */
static int
ciss_init_requests(struct ciss_softc *sc)
{
    struct ciss_request	*cr;
    int			i;

    debug_called(1);
    
    /*
     * Calculate the number of request structures/commands we are
     * going to provide for this adapter.
     */
    sc->ciss_max_requests = min(CISS_MAX_REQUESTS, sc->ciss_cfg->max_outstanding_commands);
    
    if (bootverbose)
	ciss_printf(sc, "using %d of %d available commands\n",
		    sc->ciss_max_requests, sc->ciss_cfg->max_outstanding_commands);

    /*
     * Create the DMA tag for commands.
     */
    if (bus_dma_tag_create(sc->ciss_parent_dmat,	/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   CISS_COMMAND_ALLOC_SIZE * 
			   sc->ciss_max_requests, 1,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   busdma_lock_mutex, &Giant,	/* lockfunc, lockarg */
			   &sc->ciss_command_dmat)) {
	ciss_printf(sc, "can't allocate command DMA tag\n");
	return(ENOMEM);
    }
    /*
     * Allocate memory and make it available for DMA.
     */
    if (bus_dmamem_alloc(sc->ciss_command_dmat, (void **)&sc->ciss_command, 
			 BUS_DMA_NOWAIT, &sc->ciss_command_map)) {
	ciss_printf(sc, "can't allocate command memory\n");
	return(ENOMEM);
    }
    bus_dmamap_load(sc->ciss_command_dmat, sc->ciss_command_map, sc->ciss_command, 
		    CISS_COMMAND_ALLOC_SIZE * sc->ciss_max_requests,
		    ciss_command_map_helper, sc, 0);
    bzero(sc->ciss_command, CISS_COMMAND_ALLOC_SIZE * sc->ciss_max_requests);

    /*
     * Set up the request and command structures, push requests onto
     * the free queue.
     */
    for (i = 1; i < sc->ciss_max_requests; i++) {
	cr = &sc->ciss_request[i];
	cr->cr_sc = sc;
	cr->cr_tag = i;
	bus_dmamap_create(sc->ciss_buffer_dmat, 0, &cr->cr_datamap);
	ciss_enqueue_free(cr);
    }
    return(0);
}

static void
ciss_command_map_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct ciss_softc	*sc = (struct ciss_softc *)arg;

    sc->ciss_command_phys = segs->ds_addr;
}

/************************************************************************
 * Identify the adapter, print some information about it.
 */
static int
ciss_identify_adapter(struct ciss_softc *sc)
{
    struct ciss_request	*cr;
    int			error, command_status;

    debug_called(1);

    cr = NULL;

    /*
     * Get a request, allocate storage for the adapter data.
     */
    if ((error = ciss_get_bmic_request(sc, &cr, CISS_BMIC_ID_CTLR,
				       (void **)&sc->ciss_id,
				       sizeof(*sc->ciss_id))) != 0)
	goto out;

    /*
     * Submit the request and wait for it to complete.
     */
    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "error sending BMIC ID_CTLR command (%d)\n", error);
	goto out;
    }
    
    /*
     * Check response.
     */
    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:		/* buffer right size */
	break;
    case CISS_CMD_STATUS_DATA_UNDERRUN:
    case CISS_CMD_STATUS_DATA_OVERRUN:
	ciss_printf(sc, "data over/underrun reading adapter information\n");
    default:
	ciss_printf(sc, "error reading adapter information (%s)\n",
		    ciss_name_command_status(command_status));
	error = EIO;
	goto out;
    }

    /* sanity-check reply */
    if (!sc->ciss_id->big_map_supported) {
	ciss_printf(sc, "adapter does not support BIG_MAP\n");
	error = ENXIO;
	goto out;
    }

#if 0
    /* XXX later revisions may not need this */
    sc->ciss_flags |= CISS_FLAG_FAKE_SYNCH;
#endif

    /* XXX only really required for old 5300 adapters? */
    sc->ciss_flags |= CISS_FLAG_BMIC_ABORT;
    
    /* print information */
    if (bootverbose) {
	ciss_printf(sc, "  %d logical drive%s configured\n",
		    sc->ciss_id->configured_logical_drives,
		    (sc->ciss_id->configured_logical_drives == 1) ? "" : "s");
	ciss_printf(sc, "  firmware %4.4s\n", sc->ciss_id->running_firmware_revision);
	ciss_printf(sc, "  %d SCSI channels\n", sc->ciss_id->scsi_bus_count);

	ciss_printf(sc, "  signature '%.4s'\n", sc->ciss_cfg->signature);
	ciss_printf(sc, "  valence %d\n", sc->ciss_cfg->valence);
	ciss_printf(sc, "  supported I/O methods 0x%b\n",
		    sc->ciss_cfg->supported_methods, 
		    "\20\1READY\2simple\3performant\4MEMQ\n");
	ciss_printf(sc, "  active I/O method 0x%b\n",
		    sc->ciss_cfg->active_method, "\20\2simple\3performant\4MEMQ\n");
	ciss_printf(sc, "  4G page base 0x%08x\n",
		    sc->ciss_cfg->command_physlimit);
	ciss_printf(sc, "  interrupt coalesce delay %dus\n",
		    sc->ciss_cfg->interrupt_coalesce_delay);
	ciss_printf(sc, "  interrupt coalesce count %d\n",
		    sc->ciss_cfg->interrupt_coalesce_count);
	ciss_printf(sc, "  max outstanding commands %d\n",
		    sc->ciss_cfg->max_outstanding_commands);
	ciss_printf(sc, "  bus types 0x%b\n", sc->ciss_cfg->bus_types, 
		    "\20\1ultra2\2ultra3\10fibre1\11fibre2\n");
	ciss_printf(sc, "  server name '%.16s'\n", sc->ciss_cfg->server_name);
	ciss_printf(sc, "  heartbeat 0x%x\n", sc->ciss_cfg->heartbeat);
    }

out:
    if (error) {
	if (sc->ciss_id != NULL) {
	    free(sc->ciss_id, CISS_MALLOC_CLASS);
	    sc->ciss_id = NULL;
	}
    }	
    if (cr != NULL)
	ciss_release_request(cr);
    return(error);
}

/************************************************************************
 * Find logical drives on the adapter.
 */
static int
ciss_init_logical(struct ciss_softc *sc)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_report_cdb	*crc;
    struct ciss_lun_report	*cll;
    int				error, i;
    size_t			report_size;
    int				ndrives;
    int				command_status;

    debug_called(1);

    cr = NULL;
    cll = NULL;

    /*
     * Get a request, allocate storage for the address list.
     */
    if ((error = ciss_get_request(sc, &cr)) != 0)
	goto out;
    report_size = sizeof(*cll) + CISS_MAX_LOGICAL * sizeof(union ciss_device_address);
    if ((cll = malloc(report_size, CISS_MALLOC_CLASS, M_NOWAIT | M_ZERO)) == NULL) {
	ciss_printf(sc, "can't allocate memory for logical drive list\n");
	error = ENOMEM;
	goto out;
    }

    /*
     * Build the Report Logical LUNs command.
     */
    cc = CISS_FIND_COMMAND(cr);
    cr->cr_data = cll;
    cr->cr_length = report_size;
    cr->cr_flags = CISS_REQ_DATAIN;
    
    cc->header.address.physical.mode = CISS_HDR_ADDRESS_MODE_PERIPHERAL;
    cc->header.address.physical.bus = 0;
    cc->header.address.physical.target = 0;
    cc->cdb.cdb_length = sizeof(*crc);
    cc->cdb.type = CISS_CDB_TYPE_COMMAND;
    cc->cdb.attribute = CISS_CDB_ATTRIBUTE_SIMPLE;
    cc->cdb.direction = CISS_CDB_DIRECTION_READ;
    cc->cdb.timeout = 30;	/* XXX better suggestions? */

    crc = (struct ciss_report_cdb *)&(cc->cdb.cdb[0]);
    bzero(crc, sizeof(*crc));
    crc->opcode = CISS_OPCODE_REPORT_LOGICAL_LUNS;
    crc->length = htonl(report_size);			/* big-endian field */
    cll->list_size = htonl(report_size - sizeof(*cll));	/* big-endian field */
    
    /*
     * Submit the request and wait for it to complete.  (timeout
     * here should be much greater than above)
     */
    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "error sending Report Logical LUNs command (%d)\n", error);
	goto out;
    }

    /*
     * Check response.  Note that data over/underrun is OK.
     */
    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:	/* buffer right size */
    case CISS_CMD_STATUS_DATA_UNDERRUN:	/* buffer too large, not bad */
	break;
    case CISS_CMD_STATUS_DATA_OVERRUN:
	ciss_printf(sc, "WARNING: more logical drives than driver limit (%d), adjust CISS_MAX_LOGICAL\n",
		    CISS_MAX_LOGICAL);
	break;
    default:
	ciss_printf(sc, "error detecting logical drive configuration (%s)\n",
		    ciss_name_command_status(command_status));
	error = EIO;
	goto out;
    }
    ciss_release_request(cr);
    cr = NULL;

    /* sanity-check reply */
    ndrives = (ntohl(cll->list_size) / sizeof(union ciss_device_address));
    if ((ndrives < 0) || (ndrives >= CISS_MAX_LOGICAL)) {
	ciss_printf(sc, "adapter claims to report absurd number of logical drives (%d > %d)\n",
		    ndrives, CISS_MAX_LOGICAL);
	return(ENXIO);
    }

    /*
     * Save logical drive information.
     */
    if (bootverbose)
	ciss_printf(sc, "%d logical drive%s\n", ndrives, (ndrives > 1) ? "s" : "");
    if (ndrives != sc->ciss_id->configured_logical_drives)
	ciss_printf(sc, "logical drive map claims %d drives, but adapter claims %d\n",
		    ndrives, sc->ciss_id->configured_logical_drives);
    for (i = 0; i < CISS_MAX_LOGICAL; i++) {
	if (i < ndrives) {
	    sc->ciss_logical[i].cl_address = cll->lun[i];	/* XXX endianness? */
	    if (ciss_identify_logical(sc, &sc->ciss_logical[i]) != 0)
		continue;
	    /*
	     * If the drive has had media exchanged, we should bring it online.
	     */
	    if (sc->ciss_logical[i].cl_lstatus->media_exchanged)
		ciss_accept_media(sc, i, 0);

	} else {
	    sc->ciss_logical[i].cl_status = CISS_LD_NONEXISTENT;
	}
    }
    error = 0;
    
 out:
    /*
     * Note that if the error is a timeout, we are taking a slight
     * risk here and assuming that the adapter will not respond at a
     * later time, scribbling over host memory.
     */
    if (cr != NULL)
	ciss_release_request(cr);
    if (cll != NULL)
	free(cll, CISS_MALLOC_CLASS);
    return(error);
}

static int
ciss_inquiry_logical(struct ciss_softc *sc, struct ciss_ldrive *ld)
{
    struct ciss_request			*cr;
    struct ciss_command			*cc;
    struct scsi_inquiry			*inq;
    int					error;
    int					command_status;
    int					lun;

    cr = NULL;
    lun = ld->cl_address.logical.lun;

    bzero(&ld->cl_geometry, sizeof(ld->cl_geometry));

    if ((error = ciss_get_request(sc, &cr)) != 0)
	goto out;

    cc = CISS_FIND_COMMAND(cr);
    cr->cr_data = &ld->cl_geometry;
    cr->cr_length = sizeof(ld->cl_geometry);
    cr->cr_flags = CISS_REQ_DATAIN;

    cc->header.address.logical.mode = CISS_HDR_ADDRESS_MODE_LOGICAL;
    cc->header.address.logical.lun  = lun;
    cc->cdb.cdb_length = 6;
    cc->cdb.type = CISS_CDB_TYPE_COMMAND;
    cc->cdb.attribute = CISS_CDB_ATTRIBUTE_SIMPLE;
    cc->cdb.direction = CISS_CDB_DIRECTION_READ;
    cc->cdb.timeout = 30;

    inq = (struct scsi_inquiry *)&(cc->cdb.cdb[0]);
    inq->opcode = INQUIRY;
    inq->byte2 = SI_EVPD;
    inq->page_code = CISS_VPD_LOGICAL_DRIVE_GEOMETRY;
    inq->length = sizeof(ld->cl_geometry);

    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "error getting geometry (%d)\n", error);
	goto out;
    }

    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:
    case CISS_CMD_STATUS_DATA_UNDERRUN:
	break;
    case CISS_CMD_STATUS_DATA_OVERRUN:
	ciss_printf(sc, "WARNING: Data overrun\n");
	break;
    default:
	ciss_printf(sc, "Error detecting logical drive geometry (%s)\n",
		    ciss_name_command_status(command_status));
	break;
    }

out:
    if (cr != NULL)
	ciss_release_request(cr);
    return(error);
}
/************************************************************************
 * Identify a logical drive, initialise state related to it.
 */
static int
ciss_identify_logical(struct ciss_softc *sc, struct ciss_ldrive *ld)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_bmic_cdb	*cbc;
    int				error, command_status;

    debug_called(1);

    cr = NULL;

    /*
     * Build a BMIC request to fetch the drive ID.
     */
    if ((error = ciss_get_bmic_request(sc, &cr, CISS_BMIC_ID_LDRIVE,
				       (void **)&ld->cl_ldrive, 
				       sizeof(*ld->cl_ldrive))) != 0)
	goto out;
    cc = CISS_FIND_COMMAND(cr);
    cbc = (struct ciss_bmic_cdb *)&(cc->cdb.cdb[0]);
    cbc->log_drive = ld->cl_address.logical.lun;

    /*
     * Submit the request and wait for it to complete.
     */
    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "error sending BMIC LDRIVE command (%d)\n", error);
	goto out;
    }
    
    /*
     * Check response.
     */
    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:		/* buffer right size */
	break;
    case CISS_CMD_STATUS_DATA_UNDERRUN:
    case CISS_CMD_STATUS_DATA_OVERRUN:
	ciss_printf(sc, "data over/underrun reading logical drive ID\n");
    default:
	ciss_printf(sc, "error reading logical drive ID (%s)\n",
		    ciss_name_command_status(command_status));
	error = EIO;
	goto out;
    }
    ciss_release_request(cr);
    cr = NULL;

    /*
     * Build a CISS BMIC command to get the logical drive status.
     */
    if ((error = ciss_get_ldrive_status(sc, ld)) != 0)
	goto out;

    /*
     * Get the logical drive geometry.
     */
    if ((error = ciss_inquiry_logical(sc, ld)) != 0)
	goto out;

    /*
     * Print the drive's basic characteristics.
     */
    if (bootverbose) {
	ciss_printf(sc, "logical drive %d: %s, %dMB ",
		    cbc->log_drive, ciss_name_ldrive_org(ld->cl_ldrive->fault_tolerance),
		    ((ld->cl_ldrive->blocks_available / (1024 * 1024)) *
		     ld->cl_ldrive->block_size));

	ciss_print_ldrive(sc, ld);
    }
out:
    if (error != 0) {
	/* make the drive not-exist */
	ld->cl_status = CISS_LD_NONEXISTENT;
	if (ld->cl_ldrive != NULL) {
	    free(ld->cl_ldrive, CISS_MALLOC_CLASS);
	    ld->cl_ldrive = NULL;
	}
	if (ld->cl_lstatus != NULL) {
	    free(ld->cl_lstatus, CISS_MALLOC_CLASS);
	    ld->cl_lstatus = NULL;
	}
    }
    if (cr != NULL)
	ciss_release_request(cr);
	
    return(error);
}

/************************************************************************
 * Get status for a logical drive.
 *
 * XXX should we also do this in response to Test Unit Ready?
 */
static int
ciss_get_ldrive_status(struct ciss_softc *sc,  struct ciss_ldrive *ld)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_bmic_cdb	*cbc;
    int				error, command_status;

    /*
     * Build a CISS BMIC command to get the logical drive status.
     */
    if ((error = ciss_get_bmic_request(sc, &cr, CISS_BMIC_ID_LSTATUS,
				       (void **)&ld->cl_lstatus, 
				       sizeof(*ld->cl_lstatus))) != 0)
	goto out;
    cc = CISS_FIND_COMMAND(cr);
    cbc = (struct ciss_bmic_cdb *)&(cc->cdb.cdb[0]);
    cbc->log_drive = ld->cl_address.logical.lun;

    /*
     * Submit the request and wait for it to complete.
     */
    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "error sending BMIC LSTATUS command (%d)\n", error);
	goto out;
    }
    
    /*
     * Check response.
     */
    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:		/* buffer right size */
	break;
    case CISS_CMD_STATUS_DATA_UNDERRUN:
    case CISS_CMD_STATUS_DATA_OVERRUN:
	ciss_printf(sc, "data over/underrun reading logical drive status\n");
    default:
	ciss_printf(sc, "error reading logical drive status (%s)\n",
		    ciss_name_command_status(command_status));
	error = EIO;
	goto out;
    }

    /*
     * Set the drive's summary status based on the returned status.
     *
     * XXX testing shows that a failed JBOD drive comes back at next 
     * boot in "queued for expansion" mode.  WTF?
     */
    ld->cl_status = ciss_decode_ldrive_status(ld->cl_lstatus->status);

out:
    if (cr != NULL)
	ciss_release_request(cr);
    return(error);
}

/************************************************************************
 * Notify the adapter of a config update.
 */
static int
ciss_update_config(struct ciss_softc *sc)
{
    int		i;

    debug_called(1);

    CISS_TL_SIMPLE_WRITE(sc, CISS_TL_SIMPLE_IDBR, CISS_TL_SIMPLE_IDBR_CFG_TABLE);
    for (i = 0; i < 1000; i++) {
	if (!(CISS_TL_SIMPLE_READ(sc, CISS_TL_SIMPLE_IDBR) &
	      CISS_TL_SIMPLE_IDBR_CFG_TABLE)) {
	    return(0);
	}
	DELAY(1000);
    }
    return(1);
}

/************************************************************************
 * Accept new media into a logical drive.
 *
 * XXX The drive has previously been offline; it would be good if we
 *     could make sure it's not open right now.
 */
static int
ciss_accept_media(struct ciss_softc *sc, int ldrive, int async)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_bmic_cdb	*cbc;
    int				error;

    debug(0, "bringing logical drive %d back online %ssynchronously", 
	  ldrive, async ? "a" : "");

    /*
     * Build a CISS BMIC command to bring the drive back online.
     */
    if ((error = ciss_get_bmic_request(sc, &cr, CISS_BMIC_ACCEPT_MEDIA,
				       NULL, 0)) != 0)
	goto out;
    cc = CISS_FIND_COMMAND(cr);
    cbc = (struct ciss_bmic_cdb *)&(cc->cdb.cdb[0]);
    cbc->log_drive = ldrive;

    /*
     * Dispatch the request asynchronously if we can't sleep waiting
     * for it to complete.
     */
    if (async) {
	cr->cr_complete = ciss_accept_media_complete;
	if ((error = ciss_start(cr)) != 0)
	    goto out;
	return(0);
    } else {
	/*
	 * Submit the request and wait for it to complete.
	 */
	if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	    ciss_printf(sc, "error sending BMIC LSTATUS command (%d)\n", error);
	    goto out;
	}
    }

    /*
     * Call the completion callback manually.
     */
    ciss_accept_media_complete(cr);
    return(0);
    
out:
    if (cr != NULL)
	ciss_release_request(cr);
    return(error);
}

static void
ciss_accept_media_complete(struct ciss_request *cr)
{
    int				command_status;
    
    /*
     * Check response.
     */
    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:		/* all OK */
	/* we should get a logical drive status changed event here */
	break;
    default:
	ciss_printf(cr->cr_sc, "error accepting media into failed logical drive (%s)\n",
		    ciss_name_command_status(command_status));
	break;
    }
    ciss_release_request(cr);
}

/************************************************************************
 * Release adapter resources.
 */
static void
ciss_free(struct ciss_softc *sc)
{
    struct ciss_request *cr;

    debug_called(1);

    /* we're going away */
    sc->ciss_flags |= CISS_FLAG_ABORTING;

    /* terminate the periodic heartbeat routine */
    untimeout(ciss_periodic, sc, sc->ciss_periodic);

    /* cancel the Event Notify chain */
    ciss_notify_abort(sc);
    
    /* free the controller data */
    if (sc->ciss_id != NULL)
	free(sc->ciss_id, CISS_MALLOC_CLASS);

    /* release I/O resources */
    if (sc->ciss_regs_resource != NULL)
	bus_release_resource(sc->ciss_dev, SYS_RES_MEMORY,
			     sc->ciss_regs_rid, sc->ciss_regs_resource);
    if (sc->ciss_cfg_resource != NULL)
	bus_release_resource(sc->ciss_dev, SYS_RES_MEMORY,
			     sc->ciss_cfg_rid, sc->ciss_cfg_resource);
    if (sc->ciss_intr != NULL)
	bus_teardown_intr(sc->ciss_dev, sc->ciss_irq_resource, sc->ciss_intr);
    if (sc->ciss_irq_resource != NULL)
	bus_release_resource(sc->ciss_dev, SYS_RES_IRQ,
			     sc->ciss_irq_rid, sc->ciss_irq_resource);

    /* destroy DMA tags */
    if (sc->ciss_parent_dmat)
	bus_dma_tag_destroy(sc->ciss_parent_dmat);

    while ((cr = ciss_dequeue_free(sc)) != NULL)
	bus_dmamap_destroy(sc->ciss_buffer_dmat, cr->cr_datamap);
    if (sc->ciss_buffer_dmat)
	bus_dma_tag_destroy(sc->ciss_buffer_dmat);

    /* destroy command memory and DMA tag */
    if (sc->ciss_command != NULL) {
	bus_dmamap_unload(sc->ciss_command_dmat, sc->ciss_command_map);
	bus_dmamem_free(sc->ciss_command_dmat, sc->ciss_command, sc->ciss_command_map);
    }
    if (sc->ciss_command_dmat)
	bus_dma_tag_destroy(sc->ciss_command_dmat);

    /* disconnect from CAM */
    if (sc->ciss_cam_sim) {
	xpt_bus_deregister(cam_sim_path(sc->ciss_cam_sim));
	cam_sim_free(sc->ciss_cam_sim, 0);
    }
    if (sc->ciss_cam_devq)
	cam_simq_free(sc->ciss_cam_devq);
    /* XXX what about ciss_cam_path? */
}

/************************************************************************
 * Give a command to the adapter.
 *
 * Note that this uses the simple transport layer directly.  If we
 * want to add support for other layers, we'll need a switch of some
 * sort.
 *
 * Note that the simple transport layer has no way of refusing a
 * command; we only have as many request structures as the adapter
 * supports commands, so we don't have to check (this presumes that
 * the adapter can handle commands as fast as we throw them at it).
 */
static int
ciss_start(struct ciss_request *cr)
{
    struct ciss_command	*cc;	/* XXX debugging only */
    int			error;

    cc = CISS_FIND_COMMAND(cr);
    debug(2, "post command %d tag %d ", cr->cr_tag, cc->header.host_tag);

    /*
     * Map the request's data.
     */
    if ((error = ciss_map_request(cr)))
	return(error);

#if 0
    ciss_print_request(cr);
#endif

    /*
     * Post the command to the adapter.
     */
    ciss_enqueue_busy(cr);
    CISS_TL_SIMPLE_POST_CMD(cr->cr_sc, CISS_FIND_COMMANDPHYS(cr));

    return(0);
}

/************************************************************************
 * Fetch completed request(s) from the adapter, queue them for
 * completion handling.
 *
 * Note that this uses the simple transport layer directly.  If we
 * want to add support for other layers, we'll need a switch of some
 * sort.
 *
 * Note that the simple transport mechanism does not require any
 * reentrancy protection; the OPQ read is atomic.  If there is a
 * chance of a race with something else that might move the request
 * off the busy list, then we will have to lock against that
 * (eg. timeouts, etc.)
 */
static void
ciss_done(struct ciss_softc *sc)
{
    struct ciss_request	*cr;
    struct ciss_command	*cc;
    u_int32_t		tag, index;
    int			complete;
    
    debug_called(3);

    /*
     * Loop quickly taking requests from the adapter and moving them
     * from the busy queue to the completed queue.
     */
    complete = 0;
    for (;;) {

	/* see if the OPQ contains anything */
	if (!CISS_TL_SIMPLE_OPQ_INTERRUPT(sc))
	    break;

	tag = CISS_TL_SIMPLE_FETCH_CMD(sc);
	if (tag == CISS_TL_SIMPLE_OPQ_EMPTY)
	    break;
	index = tag >> 2;
	debug(2, "completed command %d%s", index, 
	      (tag & CISS_HDR_HOST_TAG_ERROR) ? " with error" : "");
	if (index >= sc->ciss_max_requests) {
	    ciss_printf(sc, "completed invalid request %d (0x%x)\n", index, tag);
	    continue;
	}
	cr = &(sc->ciss_request[index]);
	cc = CISS_FIND_COMMAND(cr);
	cc->header.host_tag = tag;	/* not updated by adapter */
	if (ciss_remove_busy(cr)) {
	    /* assume this is garbage out of the adapter */
	    ciss_printf(sc, "completed nonbusy request %d\n", index);
	} else {
	    ciss_enqueue_complete(cr);
	}
	complete = 1;
    }
    
    /*
     * Invoke completion processing.  If we can defer this out of
     * interrupt context, that'd be good.
     */
    if (complete)
	ciss_complete(sc);
}

/************************************************************************
 * Take an interrupt from the adapter.
 */
static void
ciss_intr(void *arg)
{
    struct ciss_softc	*sc = (struct ciss_softc *)arg;

    /*
     * The only interrupt we recognise indicates that there are
     * entries in the outbound post queue.
     */
    ciss_done(sc);
}

/************************************************************************
 * Process completed requests.
 *
 * Requests can be completed in three fashions:
 *
 * - by invoking a callback function (cr_complete is non-null)
 * - by waking up a sleeper (cr_flags has CISS_REQ_SLEEP set)
 * - by clearing the CISS_REQ_POLL flag in interrupt/timeout context
 */
static void
ciss_complete(struct ciss_softc *sc)
{
    struct ciss_request	*cr;

    debug_called(2);

    /*
     * Loop taking requests off the completed queue and performing
     * completion processing on them.
     */
    for (;;) {
	if ((cr = ciss_dequeue_complete(sc)) == NULL)
	    break;
	ciss_unmap_request(cr);
	
	/*
	 * If the request has a callback, invoke it.
	 */
	if (cr->cr_complete != NULL) {
	    cr->cr_complete(cr);
	    continue;
	}
	
	/*
	 * If someone is sleeping on this request, wake them up.
	 */
	if (cr->cr_flags & CISS_REQ_SLEEP) {
	    cr->cr_flags &= ~CISS_REQ_SLEEP;
	    wakeup(cr);
	    continue;
	}

	/*
	 * If someone is polling this request for completion, signal.
	 */
	if (cr->cr_flags & CISS_REQ_POLL) {
	    cr->cr_flags &= ~CISS_REQ_POLL;
	    continue;
	}
	
	/*
	 * Give up and throw the request back on the free queue.  This
	 * should never happen; resources will probably be lost.
	 */
	ciss_printf(sc, "WARNING: completed command with no submitter\n");
	ciss_enqueue_free(cr);
    }
}

/************************************************************************
 * Report on the completion status of a request, and pass back SCSI
 * and command status values.
 */
static int
ciss_report_request(struct ciss_request *cr, int *command_status, int *scsi_status)
{
    struct ciss_command		*cc;
    struct ciss_error_info	*ce;

    debug_called(2);

    cc = CISS_FIND_COMMAND(cr);
    ce = (struct ciss_error_info *)&(cc->sg[0]);

    /*
     * We don't consider data under/overrun an error for the Report
     * Logical/Physical LUNs commands.
     */
    if ((cc->header.host_tag & CISS_HDR_HOST_TAG_ERROR) &&
	((cc->cdb.cdb[0] == CISS_OPCODE_REPORT_LOGICAL_LUNS) ||
	 (cc->cdb.cdb[0] == CISS_OPCODE_REPORT_PHYSICAL_LUNS))) {
	cc->header.host_tag &= ~CISS_HDR_HOST_TAG_ERROR;
	debug(2, "ignoring irrelevant under/overrun error");
    }
    
    /*
     * Check the command's error bit, if clear, there's no status and
     * everything is OK.
     */
    if (!(cc->header.host_tag & CISS_HDR_HOST_TAG_ERROR)) {
	if (scsi_status != NULL)
	    *scsi_status = SCSI_STATUS_OK;
	if (command_status != NULL)
	    *command_status = CISS_CMD_STATUS_SUCCESS;
	return(0);
    } else {
	if (command_status != NULL)
	    *command_status = ce->command_status;
	if (scsi_status != NULL) {
	    if (ce->command_status == CISS_CMD_STATUS_TARGET_STATUS) {
		*scsi_status = ce->scsi_status;
	    } else {
		*scsi_status = -1;
	    }
	}
	if (bootverbose)
	    ciss_printf(cr->cr_sc, "command status 0x%x (%s) scsi status 0x%x\n",
			ce->command_status, ciss_name_command_status(ce->command_status),
			ce->scsi_status);
	if (ce->command_status == CISS_CMD_STATUS_INVALID_COMMAND) {
	    ciss_printf(cr->cr_sc, "invalid command, offense size %d at %d, value 0x%x\n",
			ce->additional_error_info.invalid_command.offense_size,
			ce->additional_error_info.invalid_command.offense_offset,
			ce->additional_error_info.invalid_command.offense_value);
	}
    }
    return(1);
}

/************************************************************************
 * Issue a request and don't return until it's completed.
 *
 * Depending on adapter status, we may poll or sleep waiting for
 * completion.
 */
static int
ciss_synch_request(struct ciss_request *cr, int timeout)
{
    if (cr->cr_sc->ciss_flags & CISS_FLAG_RUNNING) {
	return(ciss_wait_request(cr, timeout));
    } else {
	return(ciss_poll_request(cr, timeout));
    }
}

/************************************************************************
 * Issue a request and poll for completion.
 *
 * Timeout in milliseconds.
 */
static int
ciss_poll_request(struct ciss_request *cr, int timeout)
{
    int		error;
    
    debug_called(2);

    cr->cr_flags |= CISS_REQ_POLL;
    if ((error = ciss_start(cr)) != 0)
	return(error);

    do {
	ciss_done(cr->cr_sc);
	if (!(cr->cr_flags & CISS_REQ_POLL))
	    return(0);
	DELAY(1000);
    } while (timeout-- >= 0);
    return(EWOULDBLOCK);
}

/************************************************************************
 * Issue a request and sleep waiting for completion.
 *
 * Timeout in milliseconds.  Note that a spurious wakeup will reset
 * the timeout.
 */
static int
ciss_wait_request(struct ciss_request *cr, int timeout)
{
    int		s, error;

    debug_called(2);

    cr->cr_flags |= CISS_REQ_SLEEP;
    if ((error = ciss_start(cr)) != 0)
	return(error);

    s = splcam();
    while (cr->cr_flags & CISS_REQ_SLEEP) {
	error = tsleep(cr, PCATCH, "cissREQ", (timeout * hz) / 1000);
	/* 
	 * On wakeup or interruption due to restartable activity, go
	 * back and check to see if we're done.
	 */
	if ((error == 0) || (error == ERESTART)) {
	    error = 0;
	    continue;
	}
	/*
	 * Timeout, interrupted system call, etc.
	 */
	break;
    }
    splx(s);
    return(error);
}

#if 0
/************************************************************************
 * Abort a request.  Note that a potential exists here to race the
 * request being completed; the caller must deal with this.
 */
static int
ciss_abort_request(struct ciss_request *ar)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_message_cdb	*cmc;
    int				error;

    debug_called(1);

    /* get a request */
    if ((error = ciss_get_request(ar->cr_sc, &cr)) != 0)
	return(error);

    /* build the abort command */	
    cc = CISS_FIND_COMMAND(cr);
    cc->header.address.mode.mode = CISS_HDR_ADDRESS_MODE_PERIPHERAL;	/* addressing? */
    cc->header.address.physical.target = 0;
    cc->header.address.physical.bus = 0;
    cc->cdb.cdb_length = sizeof(*cmc);
    cc->cdb.type = CISS_CDB_TYPE_MESSAGE;
    cc->cdb.attribute = CISS_CDB_ATTRIBUTE_SIMPLE;
    cc->cdb.direction = CISS_CDB_DIRECTION_NONE;
    cc->cdb.timeout = 30;

    cmc = (struct ciss_message_cdb *)&(cc->cdb.cdb[0]);
    cmc->opcode = CISS_OPCODE_MESSAGE_ABORT;
    cmc->type = CISS_MESSAGE_ABORT_TASK;
    cmc->abort_tag = ar->cr_tag;	/* endianness?? */

    /*
     * Send the request and wait for a response.  If we believe we
     * aborted the request OK, clear the flag that indicates it's
     * running.
     */
    error = ciss_synch_request(cr, 35 * 1000);
    if (!error)
	error = ciss_report_request(cr, NULL, NULL);
    ciss_release_request(cr);

    return(error);
}
#endif


/************************************************************************
 * Fetch and initialise a request
 */
static int
ciss_get_request(struct ciss_softc *sc, struct ciss_request **crp)
{
    struct ciss_request *cr;

    debug_called(2);

    /*
     * Get a request and clean it up.
     */
    if ((cr = ciss_dequeue_free(sc)) == NULL)
	return(ENOMEM);

    cr->cr_data = NULL;
    cr->cr_flags = 0;
    cr->cr_complete = NULL;
    
    ciss_preen_command(cr);
    *crp = cr;
    return(0);
}

static void
ciss_preen_command(struct ciss_request *cr)
{
    struct ciss_command	*cc;
    u_int32_t		cmdphys;

    /*
     * Clean up the command structure.
     *
     * Note that we set up the error_info structure here, since the
     * length can be overwritten by any command.
     */
    cc = CISS_FIND_COMMAND(cr);
    cc->header.sg_in_list = 0;		/* kinda inefficient this way */
    cc->header.sg_total = 0;
    cc->header.host_tag = cr->cr_tag << 2;
    cc->header.host_tag_zeroes = 0;
    cmdphys = CISS_FIND_COMMANDPHYS(cr);
    cc->error_info.error_info_address = cmdphys + sizeof(struct ciss_command);
    cc->error_info.error_info_length = CISS_COMMAND_ALLOC_SIZE - sizeof(struct ciss_command);
    
}

/************************************************************************
 * Release a request to the free list.
 */
static void
ciss_release_request(struct ciss_request *cr)
{
    struct ciss_softc	*sc;

    debug_called(2);

    sc = cr->cr_sc;
    
    /* release the request to the free queue */
    ciss_requeue_free(cr);
}

/************************************************************************
 * Allocate a request that will be used to send a BMIC command.  Do some
 * of the common setup here to avoid duplicating it everywhere else.
 */
static int
ciss_get_bmic_request(struct ciss_softc *sc, struct ciss_request **crp,
		      int opcode, void **bufp, size_t bufsize)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_bmic_cdb	*cbc;
    void			*buf;
    int				error;
    int				dataout;

    debug_called(2);

    cr = NULL;
    buf = NULL;	

    /*
     * Get a request.
     */
    if ((error = ciss_get_request(sc, &cr)) != 0)
	goto out;

    /*
     * Allocate data storage if requested, determine the data direction.
     */
    dataout = 0;
    if ((bufsize > 0) && (bufp != NULL)) {
	if (*bufp == NULL) {
	    if ((buf = malloc(bufsize, CISS_MALLOC_CLASS, M_NOWAIT | M_ZERO)) == NULL) {
		error = ENOMEM;
		goto out;
	    }
	} else {
	    buf = *bufp;
	    dataout = 1;	/* we are given a buffer, so we are writing */
	}
    }

    /*
     * Build a CISS BMIC command to get the logical drive ID.
     */
    cr->cr_data = buf;
    cr->cr_length = bufsize;
    if (!dataout)
	cr->cr_flags = CISS_REQ_DATAIN;
    
    cc = CISS_FIND_COMMAND(cr);
    cc->header.address.physical.mode = CISS_HDR_ADDRESS_MODE_PERIPHERAL;
    cc->header.address.physical.bus = 0;
    cc->header.address.physical.target = 0;
    cc->cdb.cdb_length = sizeof(*cbc);
    cc->cdb.type = CISS_CDB_TYPE_COMMAND;
    cc->cdb.attribute = CISS_CDB_ATTRIBUTE_SIMPLE;
    cc->cdb.direction = dataout ? CISS_CDB_DIRECTION_WRITE : CISS_CDB_DIRECTION_READ;
    cc->cdb.timeout = 0;

    cbc = (struct ciss_bmic_cdb *)&(cc->cdb.cdb[0]);
    bzero(cbc, sizeof(*cbc));
    cbc->opcode = dataout ? CISS_ARRAY_CONTROLLER_WRITE : CISS_ARRAY_CONTROLLER_READ;
    cbc->bmic_opcode = opcode;
    cbc->size = htons((u_int16_t)bufsize);

out:
    if (error) {
	if (cr != NULL)
	    ciss_release_request(cr);
	if ((bufp != NULL) && (*bufp == NULL) && (buf != NULL))
	    free(buf, CISS_MALLOC_CLASS);
    } else {
	*crp = cr;
	if ((bufp != NULL) && (*bufp == NULL) && (buf != NULL))
	    *bufp = buf;
    }
    return(error);
}

/************************************************************************
 * Handle a command passed in from userspace.
 */
static int
ciss_user_command(struct ciss_softc *sc, IOCTL_Command_struct *ioc)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_error_info	*ce;
    int				error = 0;

    debug_called(1);

    cr = NULL;

    /*
     * Get a request.
     */
    if ((error = ciss_get_request(sc, &cr)) != 0)
	goto out;
    cc = CISS_FIND_COMMAND(cr);

    /*
     * Allocate an in-kernel databuffer if required, copy in user data.
     */
    cr->cr_length = ioc->buf_size;
    if (ioc->buf_size > 0) {
	if ((cr->cr_data = malloc(ioc->buf_size, CISS_MALLOC_CLASS, M_WAITOK)) == NULL) {
	    error = ENOMEM;
	    goto out;
	}
	if ((error = copyin(ioc->buf, cr->cr_data, ioc->buf_size))) {
	    debug(0, "copyin: bad data buffer %p/%d", ioc->buf, ioc->buf_size);
	    goto out;
	}
    }

    /*
     * Build the request based on the user command.
     */
    bcopy(&ioc->LUN_info, &cc->header.address, sizeof(cc->header.address));
    bcopy(&ioc->Request, &cc->cdb, sizeof(cc->cdb));

    /* XXX anything else to populate here? */

    /*
     * Run the command.
     */
    if ((error = ciss_synch_request(cr, 60 * 1000))) {
	debug(0, "request failed - %d", error);
	goto out;
    }

    /*
     * Check to see if the command succeeded.
     */
    ce = (struct ciss_error_info *)&(cc->sg[0]);
    if (ciss_report_request(cr, NULL, NULL) == 0)
	bzero(ce, sizeof(*ce));
    else
	error = EIO;

    /*
     * Copy the results back to the user.
     */
    bcopy(ce, &ioc->error_info, sizeof(*ce));
    if ((ioc->buf_size > 0) &&
	(error = copyout(cr->cr_data, ioc->buf, ioc->buf_size))) {
	debug(0, "copyout: bad data buffer %p/%d", ioc->buf, ioc->buf_size);
	goto out;
    }

    /* done OK */
    error = 0;

out:
    if ((cr != NULL) && (cr->cr_data != NULL))
	free(cr->cr_data, CISS_MALLOC_CLASS);
    if (cr != NULL)
	ciss_release_request(cr);
    return(error);
}

/************************************************************************
 * Map a request into bus-visible space, initialise the scatter/gather
 * list.
 */
static int
ciss_map_request(struct ciss_request *cr)
{
    struct ciss_softc	*sc;

    debug_called(2);
    
    sc = cr->cr_sc;

    /* check that mapping is necessary */
    if ((cr->cr_flags & CISS_REQ_MAPPED) || (cr->cr_data == NULL))
	return(0);
    
    bus_dmamap_load(sc->ciss_buffer_dmat, cr->cr_datamap, cr->cr_data, cr->cr_length,
		    ciss_request_map_helper, CISS_FIND_COMMAND(cr), 0);
	
    if (cr->cr_flags & CISS_REQ_DATAIN)
	bus_dmamap_sync(sc->ciss_buffer_dmat, cr->cr_datamap, BUS_DMASYNC_PREREAD);
    if (cr->cr_flags & CISS_REQ_DATAOUT)
	bus_dmamap_sync(sc->ciss_buffer_dmat, cr->cr_datamap, BUS_DMASYNC_PREWRITE);

    cr->cr_flags |= CISS_REQ_MAPPED;
    return(0);
}

static void
ciss_request_map_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct ciss_command	*cc;
    int			i;

    debug_called(2);
    
    cc = (struct ciss_command *)arg;
    for (i = 0; i < nseg; i++) {
	cc->sg[i].address = segs[i].ds_addr;
	cc->sg[i].length = segs[i].ds_len;
	cc->sg[i].extension = 0;
    }
    /* we leave the s/g table entirely within the command */
    cc->header.sg_in_list = nseg;
    cc->header.sg_total = nseg;
}

/************************************************************************
 * Unmap a request from bus-visible space.
 */
static void
ciss_unmap_request(struct ciss_request *cr)
{
    struct ciss_softc	*sc;

    debug_called(2);
    
    sc = cr->cr_sc;

    /* check that unmapping is necessary */
    if (!(cr->cr_flags & CISS_REQ_MAPPED) || (cr->cr_data == NULL))
	return;

    if (cr->cr_flags & CISS_REQ_DATAIN)
	bus_dmamap_sync(sc->ciss_buffer_dmat, cr->cr_datamap, BUS_DMASYNC_POSTREAD);
    if (cr->cr_flags & CISS_REQ_DATAOUT)
	bus_dmamap_sync(sc->ciss_buffer_dmat, cr->cr_datamap, BUS_DMASYNC_POSTWRITE);

    bus_dmamap_unload(sc->ciss_buffer_dmat, cr->cr_datamap);
    cr->cr_flags &= ~CISS_REQ_MAPPED;
}

/************************************************************************
 * Attach the driver to CAM.
 *
 * We put all the logical drives on a single SCSI bus.
 */
static int
ciss_cam_init(struct ciss_softc *sc)
{

    debug_called(1);

    /*
     * Allocate a devq.  We can reuse this for the masked physical
     * devices if we decide to export these as well.
     */
    if ((sc->ciss_cam_devq = cam_simq_alloc(sc->ciss_max_requests)) == NULL) {
	ciss_printf(sc, "can't allocate CAM SIM queue\n");
	return(ENOMEM);
    }

    /*
     * Create a SIM.
     */
    if ((sc->ciss_cam_sim = cam_sim_alloc(ciss_cam_action, ciss_cam_poll, "ciss", sc,
					  device_get_unit(sc->ciss_dev),
					  sc->ciss_max_requests - 2,
					  1,
					  sc->ciss_cam_devq)) == NULL) {
	ciss_printf(sc, "can't allocate CAM SIM\n");
	return(ENOMEM);
    }

    /*
     * Register bus 0 (the 'logical drives' bus) with this SIM.
     */
    if (xpt_bus_register(sc->ciss_cam_sim, 0) != 0) {
	ciss_printf(sc, "can't register SCSI bus 0\n");
	return(ENXIO);
    }

    /*
     * Initiate a rescan of the bus.
     */
    ciss_cam_rescan_all(sc);
    
    return(0);
}

/************************************************************************
 * Initiate a rescan of the 'logical devices' SIM
 */ 
static void
ciss_cam_rescan_target(struct ciss_softc *sc, int target)
{
    union ccb	*ccb;

    debug_called(1);

    if ((ccb = malloc(sizeof(union ccb), M_TEMP, M_WAITOK | M_ZERO)) == NULL) {
	ciss_printf(sc, "rescan failed (can't allocate CCB)\n");
	return;
    }
    
    if (xpt_create_path(&sc->ciss_cam_path, xpt_periph, cam_sim_path(sc->ciss_cam_sim), target, 0)
	!= CAM_REQ_CMP) {
	ciss_printf(sc, "rescan failed (can't create path)\n");
	return;
    }
    
    xpt_setup_ccb(&ccb->ccb_h, sc->ciss_cam_path, 5/*priority (low)*/);
    ccb->ccb_h.func_code = XPT_SCAN_BUS;
    ccb->ccb_h.cbfcnp = ciss_cam_rescan_callback;
    ccb->crcn.flags = CAM_FLAG_NONE;
    xpt_action(ccb);
 
    /* scan is now in progress */
}

static void
ciss_cam_rescan_all(struct ciss_softc *sc)
{
    ciss_cam_rescan_target(sc, 0);
}

static void
ciss_cam_rescan_callback(struct cam_periph *periph, union ccb *ccb)
{
    xpt_free_path(ccb->ccb_h.path);
    free(ccb, M_TEMP);
}

/************************************************************************
 * Handle requests coming from CAM
 */
static void
ciss_cam_action(struct cam_sim *sim, union ccb *ccb)
{
    struct ciss_softc	*sc;
    struct ccb_scsiio	*csio;
    int			target;

    sc = cam_sim_softc(sim);
    csio = (struct ccb_scsiio *)&ccb->csio;
    target = csio->ccb_h.target_id;

    switch (ccb->ccb_h.func_code) {

	/* perform SCSI I/O */
    case XPT_SCSI_IO:
	if (!ciss_cam_action_io(sim, csio))
	    return;
	break;

	/* perform geometry calculations */
    case XPT_CALC_GEOMETRY:
    {
	struct ccb_calc_geometry	*ccg = &ccb->ccg;
	struct ciss_ldrive		*ld = &sc->ciss_logical[target];

	debug(1, "XPT_CALC_GEOMETRY %d:%d:%d", cam_sim_bus(sim), ccb->ccb_h.target_id, ccb->ccb_h.target_lun);

	/*
	 * Use the cached geometry settings unless the fault tolerance
	 * is invalid.
	 */
	if (ld->cl_geometry.fault_tolerance == 0xFF) {
	    u_int32_t			secs_per_cylinder;

	    ccg->heads = 255;
	    ccg->secs_per_track = 32;
	    secs_per_cylinder = ccg->heads * ccg->secs_per_track;
	    ccg->cylinders = ccg->volume_size / secs_per_cylinder;
	} else {
	    ccg->heads = ld->cl_geometry.heads;
	    ccg->secs_per_track = ld->cl_geometry.sectors;
	    ccg->cylinders = ntohs(ld->cl_geometry.cylinders);
	}
	ccb->ccb_h.status = CAM_REQ_CMP;
        break;
    }

	/* handle path attribute inquiry */
    case XPT_PATH_INQ:
    {
	struct ccb_pathinq	*cpi = &ccb->cpi;

	debug(1, "XPT_PATH_INQ %d:%d:%d", cam_sim_bus(sim), ccb->ccb_h.target_id, ccb->ccb_h.target_lun);

	cpi->version_num = 1;
	cpi->hba_inquiry = PI_TAG_ABLE;	/* XXX is this correct? */
	cpi->target_sprt = 0;
	cpi->hba_misc = 0;
	cpi->max_target = CISS_MAX_LOGICAL;
	cpi->max_lun = 0;		/* 'logical drive' channel only */
	cpi->initiator_id = CISS_MAX_LOGICAL;
	strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
        strncpy(cpi->hba_vid, "msmith@freebsd.org", HBA_IDLEN);
        strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
        cpi->unit_number = cam_sim_unit(sim);
        cpi->bus_id = cam_sim_bus(sim);
	cpi->base_transfer_speed = 132 * 1024;	/* XXX what to set this to? */
	ccb->ccb_h.status = CAM_REQ_CMP;
	break;
    }

    case XPT_GET_TRAN_SETTINGS:
    {
	struct ccb_trans_settings	*cts = &ccb->cts;
	int				bus, target;

	bus = cam_sim_bus(sim);
	target = cts->ccb_h.target_id;

	debug(1, "XPT_GET_TRAN_SETTINGS %d:%d", bus, target);
	cts->valid = 0;

	/* disconnect always OK */
	cts->flags |= CCB_TRANS_DISC_ENB;
	cts->valid |= CCB_TRANS_DISC_VALID;

	cts->ccb_h.status = CAM_REQ_CMP;
	break;
    }

    default:		/* we can't do this */
	debug(1, "unspported func_code = 0x%x", ccb->ccb_h.func_code);
	ccb->ccb_h.status = CAM_REQ_INVALID;
	break;
    }

    xpt_done(ccb);
}

/************************************************************************
 * Handle a CAM SCSI I/O request.
 */
static int
ciss_cam_action_io(struct cam_sim *sim, struct ccb_scsiio *csio)
{
    struct ciss_softc	*sc;
    int			bus, target;
    struct ciss_request	*cr;
    struct ciss_command	*cc;
    int			error;

    sc = cam_sim_softc(sim);
    bus = cam_sim_bus(sim);
    target = csio->ccb_h.target_id;

    debug(2, "XPT_SCSI_IO %d:%d:%d", bus, target, csio->ccb_h.target_lun);

    /* check for I/O attempt to nonexistent device */
    if ((bus != 0) ||
	(target >= CISS_MAX_LOGICAL) ||
	(sc->ciss_logical[target].cl_status == CISS_LD_NONEXISTENT)) {
	debug(3, "  device does not exist");
	csio->ccb_h.status = CAM_REQ_CMP_ERR;
    }

    /* firmware does not support commands > 10 bytes */
    if (csio->cdb_len > 12/*CISS_CDB_BUFFER_SIZE*/) {
	debug(3, "  command too large (%d > %d)", csio->cdb_len, CISS_CDB_BUFFER_SIZE);
	csio->ccb_h.status = CAM_REQ_CMP_ERR;
    }

    /* check that the CDB pointer is not to a physical address */
    if ((csio->ccb_h.flags & CAM_CDB_POINTER) && (csio->ccb_h.flags & CAM_CDB_PHYS)) {
	debug(3, "  CDB pointer is to physical address");
	csio->ccb_h.status = CAM_REQ_CMP_ERR;
    }

    /* if there is data transfer, it must be to/from a virtual address */
    if ((csio->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
	if (csio->ccb_h.flags & CAM_DATA_PHYS) {		/* we can't map it */
	    debug(3, "  data pointer is to physical address");
	    csio->ccb_h.status = CAM_REQ_CMP_ERR;
	}
	if (csio->ccb_h.flags & CAM_SCATTER_VALID) {	/* we want to do the s/g setup */
	    debug(3, "  data has premature s/g setup");
	    csio->ccb_h.status = CAM_REQ_CMP_ERR;
	}
    }

    /* abandon aborted ccbs or those that have failed validation */
    if ((csio->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
	debug(3, "abandoning CCB due to abort/validation failure");
	return(EINVAL);
    }

    /* handle emulation of some SCSI commands ourself */
    if (ciss_cam_emulate(sc, csio))
	return(0);

    /*
     * Get a request to manage this command.  If we can't, return the
     * ccb, freeze the queue and flag so that we unfreeze it when a
     * request completes.
     */
    if ((error = ciss_get_request(sc, &cr)) != 0) {
	xpt_freeze_simq(sc->ciss_cam_sim, 1);
	csio->ccb_h.status |= CAM_REQUEUE_REQ;
	return(error);
    }

    /*
     * Build the command.
     */
    cc = CISS_FIND_COMMAND(cr);
    cr->cr_data = csio->data_ptr;
    cr->cr_length = csio->dxfer_len;
    cr->cr_complete = ciss_cam_complete;
    cr->cr_private = csio;
	
    cc->header.address.logical.mode = CISS_HDR_ADDRESS_MODE_LOGICAL;
    cc->header.address.logical.lun = target;
    cc->cdb.cdb_length = csio->cdb_len;
    cc->cdb.type = CISS_CDB_TYPE_COMMAND;
    cc->cdb.attribute = CISS_CDB_ATTRIBUTE_SIMPLE;	/* XXX ordered tags? */
    if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT) {
	cr->cr_flags = CISS_REQ_DATAOUT;
	cc->cdb.direction = CISS_CDB_DIRECTION_WRITE;
    } else if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
	cr->cr_flags = CISS_REQ_DATAIN;
	cc->cdb.direction = CISS_CDB_DIRECTION_READ;
    } else {
	cr->cr_flags = 0;
	cc->cdb.direction = CISS_CDB_DIRECTION_NONE;
    }
    cc->cdb.timeout = (csio->ccb_h.timeout / 1000) + 1;
    if (csio->ccb_h.flags & CAM_CDB_POINTER) {
	bcopy(csio->cdb_io.cdb_ptr, &cc->cdb.cdb[0], csio->cdb_len);
    } else {
	bcopy(csio->cdb_io.cdb_bytes, &cc->cdb.cdb[0], csio->cdb_len);
    }

    /*
     * Submit the request to the adapter.
     *
     * Note that this may fail if we're unable to map the request (and
     * if we ever learn a transport layer other than simple, may fail
     * if the adapter rejects the command).
     */
    if ((error = ciss_start(cr)) != 0) {
	xpt_freeze_simq(sc->ciss_cam_sim, 1);
	csio->ccb_h.status |= CAM_REQUEUE_REQ;
	ciss_release_request(cr);
	return(error);
    }
	
    return(0);
}

/************************************************************************
 * Emulate SCSI commands the adapter doesn't handle as we might like.
 */
static int
ciss_cam_emulate(struct ciss_softc *sc, struct ccb_scsiio *csio)
{
    int		target;
    u_int8_t	opcode;
    
    
    target = csio->ccb_h.target_id;
    opcode = (csio->ccb_h.flags & CAM_CDB_POINTER) ? 
	*(u_int8_t *)csio->cdb_io.cdb_ptr : csio->cdb_io.cdb_bytes[0];

    /*
     * Handle requests for volumes that don't exist.  A selection timeout
     * is slightly better than an illegal request.  Other errors might be 
     * better.
     */
    if (sc->ciss_logical[target].cl_status == CISS_LD_NONEXISTENT) {
	csio->ccb_h.status = CAM_SEL_TIMEOUT;
	xpt_done((union ccb *)csio);
	return(1);
    }

    /*
     * Handle requests for volumes that exist but are offline.
     *
     * I/O operations should fail, everything else should work.
     */
    if (sc->ciss_logical[target].cl_status == CISS_LD_OFFLINE) {
	switch(opcode) {
	case READ_6:
	case READ_10:
	case READ_12:
	case WRITE_6:
	case WRITE_10:
	case WRITE_12:
	    csio->ccb_h.status = CAM_SEL_TIMEOUT;
	    xpt_done((union ccb *)csio);
	    return(1);
	}
    }
	    

    /* if we have to fake Synchronise Cache */
    if (sc->ciss_flags & CISS_FLAG_FAKE_SYNCH) {
	
	/*
	 * If this is a Synchronise Cache command, typically issued when
	 * a device is closed, flush the adapter and complete now.
	 */
	if (((csio->ccb_h.flags & CAM_CDB_POINTER) ? 
	     *(u_int8_t *)csio->cdb_io.cdb_ptr : csio->cdb_io.cdb_bytes[0]) == SYNCHRONIZE_CACHE) {
	    ciss_flush_adapter(sc);
	    csio->ccb_h.status = CAM_REQ_CMP;
	    xpt_done((union ccb *)csio);
	    return(1);
	}
    }

    return(0);
}

/************************************************************************
 * Check for possibly-completed commands.
 */
static void
ciss_cam_poll(struct cam_sim *sim)
{
    struct ciss_softc	*sc = cam_sim_softc(sim);

    debug_called(2);

    ciss_done(sc);
}

/************************************************************************
 * Handle completion of a command - pass results back through the CCB
 */
static void
ciss_cam_complete(struct ciss_request *cr)
{
    struct ciss_softc		*sc;
    struct ciss_command		*cc;
    struct ciss_error_info	*ce;
    struct ccb_scsiio		*csio;
    int				scsi_status;
    int				command_status;

    debug_called(2);

    sc = cr->cr_sc;
    cc = CISS_FIND_COMMAND(cr);
    ce = (struct ciss_error_info *)&(cc->sg[0]);
    csio = (struct ccb_scsiio *)cr->cr_private;

    /*
     * Extract status values from request.
     */
    ciss_report_request(cr, &command_status, &scsi_status);
    csio->scsi_status = scsi_status;
    
    /*
     * Handle specific SCSI status values.
     */
    switch(scsi_status) {
	/* no status due to adapter error */
    case -1:				
	debug(0, "adapter error");
	csio->ccb_h.status = CAM_REQ_CMP_ERR;
	break;
	
	/* no status due to command completed OK */
    case SCSI_STATUS_OK:		/* CISS_SCSI_STATUS_GOOD */
	debug(2, "SCSI_STATUS_OK");
	csio->ccb_h.status = CAM_REQ_CMP;
	break;

	/* check condition, sense data included */
    case SCSI_STATUS_CHECK_COND:	/* CISS_SCSI_STATUS_CHECK_CONDITION */
	debug(0, "SCSI_STATUS_CHECK_COND  sense size %d  resid %d",
	      ce->sense_length, ce->residual_count);
	bzero(&csio->sense_data, SSD_FULL_SIZE);
	bcopy(&ce->sense_info[0], &csio->sense_data, ce->sense_length);
	csio->sense_len = ce->sense_length;
	csio->resid = ce->residual_count;	
	csio->ccb_h.status = CAM_SCSI_STATUS_ERROR | CAM_AUTOSNS_VALID;
#ifdef CISS_DEBUG
	{
	    struct scsi_sense_data	*sns = (struct scsi_sense_data *)&ce->sense_info[0];
	    debug(0, "sense key %x", sns->flags & SSD_KEY);
	}
#endif	    
	break;

    case SCSI_STATUS_BUSY:		/* CISS_SCSI_STATUS_BUSY */
	debug(0, "SCSI_STATUS_BUSY");
	csio->ccb_h.status = CAM_SCSI_BUSY;
	break;

    default:
	debug(0, "unknown status 0x%x", csio->scsi_status);
	csio->ccb_h.status = CAM_REQ_CMP_ERR;
	break;
    }

    /* handle post-command fixup */
    ciss_cam_complete_fixup(sc, csio);

    /* tell CAM we're ready for more commands */
    csio->ccb_h.status |= CAM_RELEASE_SIMQ;

    xpt_done((union ccb *)csio);
    ciss_release_request(cr);
}

/********************************************************************************
 * Fix up the result of some commands here.
 */
static void
ciss_cam_complete_fixup(struct ciss_softc *sc, struct ccb_scsiio *csio)
{
    struct scsi_inquiry_data	*inq;
    struct ciss_ldrive		*cl;
    int				target;

    if (((csio->ccb_h.flags & CAM_CDB_POINTER) ? 
	 *(u_int8_t *)csio->cdb_io.cdb_ptr : csio->cdb_io.cdb_bytes[0]) == INQUIRY) {

	inq = (struct scsi_inquiry_data *)csio->data_ptr;
	target = csio->ccb_h.target_id;
	cl = &sc->ciss_logical[target];
	
	padstr(inq->vendor, "COMPAQ", 8);
	padstr(inq->product, ciss_name_ldrive_org(cl->cl_ldrive->fault_tolerance), 8);
	padstr(inq->revision, ciss_name_ldrive_status(cl->cl_lstatus->status), 16);
    }
}


/********************************************************************************
 * Find a peripheral attached at (target)
 */
static struct cam_periph *
ciss_find_periph(struct ciss_softc *sc, int target)
{
    struct cam_periph	*periph;
    struct cam_path	*path;
    int			status;

    status = xpt_create_path(&path, NULL, cam_sim_path(sc->ciss_cam_sim), target, 0);
    if (status == CAM_REQ_CMP) {
	periph = cam_periph_find(path, NULL);
	xpt_free_path(path);
    } else {
	periph = NULL;
    }
    return(periph);
}

/********************************************************************************
 * Name the device at (target)
 *
 * XXX is this strictly correct?
 */
static int
ciss_name_device(struct ciss_softc *sc, int target)
{
    struct cam_periph	*periph;

    if ((periph = ciss_find_periph(sc, target)) != NULL) {
	sprintf(sc->ciss_logical[target].cl_name, "%s%d", periph->periph_name, periph->unit_number);
	return(0);
    }
    sc->ciss_logical[target].cl_name[0] = 0;
    return(ENOENT);
}

/************************************************************************
 * Periodic status monitoring.
 */
static void
ciss_periodic(void *arg)
{
    struct ciss_softc	*sc;

    debug_called(1);
    
    sc = (struct ciss_softc *)arg;

    /*
     * Check the adapter heartbeat.
     */
    if (sc->ciss_cfg->heartbeat == sc->ciss_heartbeat) {
	sc->ciss_heart_attack++;
	debug(0, "adapter heart attack in progress 0x%x/%d", 
	      sc->ciss_heartbeat, sc->ciss_heart_attack);
	if (sc->ciss_heart_attack == 3) {
	    ciss_printf(sc, "ADAPTER HEARTBEAT FAILED\n");
	    /* XXX should reset adapter here */
	}
    } else {
	sc->ciss_heartbeat = sc->ciss_cfg->heartbeat;
	sc->ciss_heart_attack = 0;
	debug(3, "new heartbeat 0x%x", sc->ciss_heartbeat);
    }
    
    /*
     * If the notify event request has died for some reason, or has
     * not started yet, restart it.
     */
    if (!(sc->ciss_flags & CISS_FLAG_NOTIFY_OK)) {
	debug(0, "(re)starting Event Notify chain");
	ciss_notify_event(sc);
    }

    /*
     * Reschedule.
     */
    if (!(sc->ciss_flags & CISS_FLAG_ABORTING))
	sc->ciss_periodic = timeout(ciss_periodic, sc, CISS_HEARTBEAT_RATE * hz);
}

/************************************************************************
 * Request a notification response from the adapter.
 *
 * If (cr) is NULL, this is the first request of the adapter, so
 * reset the adapter's message pointer and start with the oldest
 * message available.
 */
static void
ciss_notify_event(struct ciss_softc *sc)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_notify_cdb	*cnc;
    int				error;

    debug_called(1);

    cr = sc->ciss_periodic_notify;
    
    /* get a request if we don't already have one */
    if (cr == NULL) {
	if ((error = ciss_get_request(sc, &cr)) != 0) {
	    debug(0, "can't get notify event request");
	    goto out;
	}
	sc->ciss_periodic_notify = cr;
	cr->cr_complete = ciss_notify_complete;
	debug(1, "acquired request %d", cr->cr_tag);
    }
    
    /* 
     * Get a databuffer if we don't already have one, note that the
     * adapter command wants a larger buffer than the actual
     * structure.
     */
    if (cr->cr_data == NULL) {
	if ((cr->cr_data = malloc(CISS_NOTIFY_DATA_SIZE, CISS_MALLOC_CLASS, M_NOWAIT)) == NULL) {
	    debug(0, "can't get notify event request buffer");
	    error = ENOMEM;
	    goto out;
	}
	cr->cr_length = CISS_NOTIFY_DATA_SIZE;
    }

    /* re-setup the request's command (since we never release it) XXX overkill*/
    ciss_preen_command(cr);

    /* (re)build the notify event command */
    cc = CISS_FIND_COMMAND(cr);
    cc->header.address.physical.mode = CISS_HDR_ADDRESS_MODE_PERIPHERAL;
    cc->header.address.physical.bus = 0;
    cc->header.address.physical.target = 0;

    cc->cdb.cdb_length = sizeof(*cnc);
    cc->cdb.type = CISS_CDB_TYPE_COMMAND;
    cc->cdb.attribute = CISS_CDB_ATTRIBUTE_SIMPLE;
    cc->cdb.direction = CISS_CDB_DIRECTION_READ;
    cc->cdb.timeout = 0;	/* no timeout, we hope */
    
    cnc = (struct ciss_notify_cdb *)&(cc->cdb.cdb[0]);
    bzero(cr->cr_data, CISS_NOTIFY_DATA_SIZE);
    cnc->opcode = CISS_OPCODE_READ;
    cnc->command = CISS_COMMAND_NOTIFY_ON_EVENT;
    cnc->timeout = 0;		/* no timeout, we hope */
    cnc->synchronous = 0;
    cnc->ordered = 0;
    cnc->seek_to_oldest = 0;
    cnc->new_only = 0;
    cnc->length = htonl(CISS_NOTIFY_DATA_SIZE);

    /* submit the request */
    error = ciss_start(cr);

 out:
    if (error) {
	if (cr != NULL) {
	    if (cr->cr_data != NULL)
		free(cr->cr_data, CISS_MALLOC_CLASS);
	    ciss_release_request(cr);
	}
	sc->ciss_periodic_notify = NULL;
	debug(0, "can't submit notify event request");
	sc->ciss_flags &= ~CISS_FLAG_NOTIFY_OK;
    } else {
	debug(1, "notify event submitted");
	sc->ciss_flags |= CISS_FLAG_NOTIFY_OK;
    }
}

static void
ciss_notify_complete(struct ciss_request *cr)
{
    struct ciss_command	*cc;
    struct ciss_notify	*cn;
    struct ciss_softc	*sc;
    int			scsi_status;
    int			command_status;

    debug_called(1);
    
    cc = CISS_FIND_COMMAND(cr);
    cn = (struct ciss_notify *)cr->cr_data;
    sc = cr->cr_sc;
    
    /*
     * Report request results, decode status.
     */
    ciss_report_request(cr, &command_status, &scsi_status);

    /*
     * Abort the chain on a fatal error.
     *
     * XXX which of these are actually errors?
     */
    if ((command_status != CISS_CMD_STATUS_SUCCESS) &&
	(command_status != CISS_CMD_STATUS_TARGET_STATUS) &&
	(command_status != CISS_CMD_STATUS_TIMEOUT)) {	/* XXX timeout? */
	ciss_printf(sc, "fatal error in Notify Event request (%s)\n",
		    ciss_name_command_status(command_status));
	ciss_release_request(cr);
	sc->ciss_flags &= ~CISS_FLAG_NOTIFY_OK;
	return;
    }

    /* 
     * If the adapter gave us a text message, print it.
     */
    if (cn->message[0] != 0)
	ciss_printf(sc, "*** %.80s\n", cn->message);

    debug(0, "notify event class %d subclass %d detail %d",
		cn->class, cn->subclass, cn->detail);

    /*
     * If there's room, save the event for a user-level tool.
     */
    if (((sc->ciss_notify_head + 1) % CISS_MAX_EVENTS) != sc->ciss_notify_tail) {
	sc->ciss_notify[sc->ciss_notify_head] = *cn;
	sc->ciss_notify_head = (sc->ciss_notify_head + 1) % CISS_MAX_EVENTS;
    }

    /*
     * Some events are directly of interest to us.
     */
    switch (cn->class) {
    case CISS_NOTIFY_LOGICAL:
	ciss_notify_logical(sc, cn);
	break;
    case CISS_NOTIFY_PHYSICAL:
	ciss_notify_physical(sc, cn);
	break;
    }

    /*
     * If the response indicates that the notifier has been aborted,
     * release the notifier command.
     */
    if ((cn->class == CISS_NOTIFY_NOTIFIER) &&
	(cn->subclass == CISS_NOTIFY_NOTIFIER_STATUS) &&
	(cn->detail == 1)) {
	debug(0, "notifier exiting");
	sc->ciss_flags &= ~CISS_FLAG_NOTIFY_OK;
	ciss_release_request(cr);
	sc->ciss_periodic_notify = NULL;
	wakeup(&sc->ciss_periodic_notify);
    }
	
    /*
     * Send a new notify event command, if we're not aborting.
     */
    if (!(sc->ciss_flags & CISS_FLAG_ABORTING)) {
	ciss_notify_event(sc);
    }
}

/************************************************************************
 * Abort the Notify Event chain.
 *
 * Note that we can't just abort the command in progress; we have to
 * explicitly issue an Abort Notify Event command in order for the
 * adapter to clean up correctly.
 *
 * If we are called with CISS_FLAG_ABORTING set in the adapter softc,
 * the chain will not restart itself.
 */
static int
ciss_notify_abort(struct ciss_softc *sc)
{
    struct ciss_request		*cr;
    struct ciss_command		*cc;
    struct ciss_notify_cdb	*cnc;
    int				error, s, command_status, scsi_status;

    debug_called(1);

    cr = NULL;
    error = 0;
    
    /* verify that there's an outstanding command */
    if (!(sc->ciss_flags & CISS_FLAG_NOTIFY_OK))
	goto out;
    
    /* get a command to issue the abort with */
    if ((error = ciss_get_request(sc, &cr)))
	goto out;

    /* get a buffer for the result */
    if ((cr->cr_data = malloc(CISS_NOTIFY_DATA_SIZE, CISS_MALLOC_CLASS, M_NOWAIT)) == NULL) {
	debug(0, "can't get notify event request buffer");
	error = ENOMEM;
	goto out;
    }
    cr->cr_length = CISS_NOTIFY_DATA_SIZE;
    
    /* build the CDB */
    cc = CISS_FIND_COMMAND(cr);
    cc->header.address.physical.mode = CISS_HDR_ADDRESS_MODE_PERIPHERAL;
    cc->header.address.physical.bus = 0;
    cc->header.address.physical.target = 0;
    cc->cdb.cdb_length = sizeof(*cnc);
    cc->cdb.type = CISS_CDB_TYPE_COMMAND;
    cc->cdb.attribute = CISS_CDB_ATTRIBUTE_SIMPLE;
    cc->cdb.direction = CISS_CDB_DIRECTION_READ;
    cc->cdb.timeout = 0;	/* no timeout, we hope */
    
    cnc = (struct ciss_notify_cdb *)&(cc->cdb.cdb[0]);
    bzero(cnc, sizeof(*cnc));
    cnc->opcode = CISS_OPCODE_WRITE;
    cnc->command = CISS_COMMAND_ABORT_NOTIFY;
    cnc->length = htonl(CISS_NOTIFY_DATA_SIZE);

    ciss_print_request(cr);
    
    /*
     * Submit the request and wait for it to complete.
     */
    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "Abort Notify Event command failed (%d)\n", error);
	goto out;
    }

    /*
     * Check response.
     */
    ciss_report_request(cr, &command_status, &scsi_status);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:
	break;
    case CISS_CMD_STATUS_INVALID_COMMAND:
	/*
	 * Some older adapters don't support the CISS version of this
	 * command.  Fall back to using the BMIC version.
	 */
	error = ciss_notify_abort_bmic(sc);
	if (error != 0)
	    goto out;
	break;
	
    case CISS_CMD_STATUS_TARGET_STATUS:
	/*
	 * This can happen if the adapter thinks there wasn't an outstanding
	 * Notify Event command but we did.  We clean up here.
	 */
	if (scsi_status == CISS_SCSI_STATUS_CHECK_CONDITION) {
	    if (sc->ciss_periodic_notify != NULL)
		ciss_release_request(sc->ciss_periodic_notify);
	    error = 0;
	    goto out;
	}
	/* FALLTHROUGH */
	    
    default:
	ciss_printf(sc, "Abort Notify Event command failed (%s)\n",
		    ciss_name_command_status(command_status));
	error = EIO;
	goto out;
    }
    
    /*
     * Sleep waiting for the notifier command to complete.  Note
     * that if it doesn't, we may end up in a bad situation, since
     * the adapter may deliver it later.  Also note that the adapter
     * requires the Notify Event command to be cancelled in order to
     * maintain internal bookkeeping.
     */
    s = splcam();
    while (sc->ciss_periodic_notify != NULL) {
	error = tsleep(&sc->ciss_periodic_notify, 0, "cissNEA", hz * 5);
	if (error == EWOULDBLOCK) {
	    ciss_printf(sc, "Notify Event command failed to abort, adapter may wedge.\n");
	    break;
	}
    }
    splx(s);

 out:
    /* release the cancel request */
    if (cr != NULL) {
	if (cr->cr_data != NULL)
	    free(cr->cr_data, CISS_MALLOC_CLASS);
	ciss_release_request(cr);
    }
    if (error == 0)
	sc->ciss_flags &= ~CISS_FLAG_NOTIFY_OK;
    return(error);
}

/************************************************************************
 * Abort the Notify Event chain using a BMIC command.
 */
static int
ciss_notify_abort_bmic(struct ciss_softc *sc)
{
    struct ciss_request			*cr;
    int					error, command_status;

    debug_called(1);

    cr = NULL;
    error = 0;

    /* verify that there's an outstanding command */
    if (!(sc->ciss_flags & CISS_FLAG_NOTIFY_OK))
	goto out;
    
    /*
     * Build a BMIC command to cancel the Notify on Event command.
     *
     * Note that we are sending a CISS opcode here.  Odd.
     */
    if ((error = ciss_get_bmic_request(sc, &cr, CISS_COMMAND_ABORT_NOTIFY,
				       NULL, 0)) != 0)
	goto out;

    /*
     * Submit the request and wait for it to complete.
     */
    if ((error = ciss_synch_request(cr, 60 * 1000)) != 0) {
	ciss_printf(sc, "error sending BMIC Cancel Notify on Event command (%d)\n", error);
	goto out;
    }
    
    /*
     * Check response.
     */
    ciss_report_request(cr, &command_status, NULL);
    switch(command_status) {
    case CISS_CMD_STATUS_SUCCESS:
	break;
    default:
	ciss_printf(sc, "error cancelling Notify on Event (%s)\n",  
		    ciss_name_command_status(command_status));
	error = EIO;
	goto out;
    }

out:
    if (cr != NULL)
	ciss_release_request(cr);
    return(error);
}

/************************************************************************
 * Handle a notify event relating to the status of a logical drive.
 *
 * XXX need to be able to defer some of these to properly handle
 *     calling the "ID Physical drive" command, unless the 'extended'
 *     drive IDs are always in BIG_MAP format.
 */
static void
ciss_notify_logical(struct ciss_softc *sc, struct ciss_notify *cn)
{
    struct ciss_ldrive	*ld;
    int			ostatus;

    debug_called(2);

    ld = &sc->ciss_logical[cn->data.logical_status.logical_drive];

    switch (cn->subclass) {
    case CISS_NOTIFY_LOGICAL_STATUS:
	switch (cn->detail) {
	case 0:
	    ciss_name_device(sc, cn->data.logical_status.logical_drive);
	    ciss_printf(sc, "logical drive %d (%s) changed status %s->%s, spare status 0x%b\n",
			cn->data.logical_status.logical_drive, ld->cl_name,
			ciss_name_ldrive_status(cn->data.logical_status.previous_state),
			ciss_name_ldrive_status(cn->data.logical_status.new_state),
			cn->data.logical_status.spare_state,
			"\20\1configured\2rebuilding\3failed\4in use\5available\n");

	    /*
	     * Update our idea of the drive's status.
	     */
	    ostatus = ciss_decode_ldrive_status(cn->data.logical_status.previous_state);
	    ld->cl_status = ciss_decode_ldrive_status(cn->data.logical_status.new_state);
	    if (ld->cl_lstatus != NULL)
		ld->cl_lstatus->status = cn->data.logical_status.new_state;

#if 0
	    /*
	     * Have CAM rescan the drive if its status has changed.
	     */
	    if (ostatus != ld->cl_status)
		ciss_cam_rescan_target(sc, cn->data.logical_status.logical_drive);
#endif

	    break;

	case 1:	/* logical drive has recognised new media, needs Accept Media Exchange */
	    ciss_name_device(sc, cn->data.logical_status.logical_drive);
	    ciss_printf(sc, "logical drive %d (%s) media exchanged, ready to go online\n",
			cn->data.logical_status.logical_drive, ld->cl_name);
	    ciss_accept_media(sc, cn->data.logical_status.logical_drive, 1);
	    break;

	case 2:
	case 3:
	    ciss_printf(sc, "rebuild of logical drive %d (%s) failed due to %s error\n",
			cn->data.rebuild_aborted.logical_drive,
			sc->ciss_logical[cn->data.rebuild_aborted.logical_drive].cl_name,
			(cn->detail == 2) ? "read" : "write");
	    break;
	}
	break;

    case CISS_NOTIFY_LOGICAL_ERROR:
	if (cn->detail == 0) {
	    ciss_printf(sc, "FATAL I/O ERROR on logical drive %d (%s), SCSI port %d ID %d\n",
			cn->data.io_error.logical_drive,
			sc->ciss_logical[cn->data.io_error.logical_drive].cl_name,
			cn->data.io_error.failure_bus,
			cn->data.io_error.failure_drive);
	    /* XXX should we take the drive down at this point, or will we be told? */
	}
	break;

    case CISS_NOTIFY_LOGICAL_SURFACE:
	if (cn->detail == 0)
	    ciss_printf(sc, "logical drive %d (%s) completed consistency initialisation\n",
			cn->data.consistency_completed.logical_drive,
			sc->ciss_logical[cn->data.consistency_completed.logical_drive].cl_name);
	break;
    }
}

/************************************************************************
 * Handle a notify event relating to the status of a physical drive.
 */
static void
ciss_notify_physical(struct ciss_softc *sc, struct ciss_notify *cn)
{
    
}

/************************************************************************
 * Print a request.
 */
static void
ciss_print_request(struct ciss_request *cr)
{
    struct ciss_softc	*sc;
    struct ciss_command	*cc;
    int			i;

    sc = cr->cr_sc;
    cc = CISS_FIND_COMMAND(cr);
    
    ciss_printf(sc, "REQUEST @ %p\n", cr);
    ciss_printf(sc, "  data %p/%d  tag %d  flags %b\n",
	      cr->cr_data, cr->cr_length, cr->cr_tag, cr->cr_flags,
	      "\20\1mapped\2sleep\3poll\4dataout\5datain\n");
    ciss_printf(sc, "  sg list/total %d/%d  host tag 0x%x\n",
		cc->header.sg_in_list, cc->header.sg_total, cc->header.host_tag);
    switch(cc->header.address.mode.mode) {
    case CISS_HDR_ADDRESS_MODE_PERIPHERAL:
    case CISS_HDR_ADDRESS_MODE_MASK_PERIPHERAL:
	ciss_printf(sc, "  physical bus %d target %d\n",
		    cc->header.address.physical.bus, cc->header.address.physical.target);
	break;
    case CISS_HDR_ADDRESS_MODE_LOGICAL:
	ciss_printf(sc, "  logical unit %d\n", cc->header.address.logical.lun);
	break;
    }
    ciss_printf(sc, "  %s cdb length %d type %s attribute %s\n", 
		(cc->cdb.direction == CISS_CDB_DIRECTION_NONE) ? "no-I/O" :
		(cc->cdb.direction == CISS_CDB_DIRECTION_READ) ? "READ" :
		(cc->cdb.direction == CISS_CDB_DIRECTION_WRITE) ? "WRITE" : "??",
		cc->cdb.cdb_length,
		(cc->cdb.type == CISS_CDB_TYPE_COMMAND) ? "command" :
		(cc->cdb.type == CISS_CDB_TYPE_MESSAGE) ? "message" : "??",
		(cc->cdb.attribute == CISS_CDB_ATTRIBUTE_UNTAGGED) ? "untagged" :
		(cc->cdb.attribute == CISS_CDB_ATTRIBUTE_SIMPLE) ? "simple" :
		(cc->cdb.attribute == CISS_CDB_ATTRIBUTE_HEAD_OF_QUEUE) ? "head-of-queue" :
		(cc->cdb.attribute == CISS_CDB_ATTRIBUTE_ORDERED) ? "ordered" :
		(cc->cdb.attribute == CISS_CDB_ATTRIBUTE_AUTO_CONTINGENT) ? "auto-contingent" : "??");
    ciss_printf(sc, "  %*D\n", cc->cdb.cdb_length, &cc->cdb.cdb[0], " ");

    if (cc->header.host_tag & CISS_HDR_HOST_TAG_ERROR) {
	/* XXX print error info */
    } else {
	/* since we don't use chained s/g, don't support it here */
	for (i = 0; i < cc->header.sg_in_list; i++) {
	    if ((i % 4) == 0)
		ciss_printf(sc, "   ");
	    printf("0x%08x/%d ", (u_int32_t)cc->sg[i].address, cc->sg[i].length);
	    if ((((i + 1) % 4) == 0) || (i == (cc->header.sg_in_list - 1)))
		printf("\n");
	}
    }
}

/************************************************************************
 * Print information about the status of a logical drive.
 */
static void
ciss_print_ldrive(struct ciss_softc *sc, struct ciss_ldrive *ld)
{
    int		bus, target, i;

    if (ld->cl_lstatus == NULL) {
	printf("does not exist\n");
	return;
    }

    /* print drive status */
    switch(ld->cl_lstatus->status) {
    case CISS_LSTATUS_OK:
	printf("online\n");
	break;
    case CISS_LSTATUS_INTERIM_RECOVERY:
	printf("in interim recovery mode\n");
	break;
    case CISS_LSTATUS_READY_RECOVERY:
	printf("ready to begin recovery\n");
	break;
    case CISS_LSTATUS_RECOVERING:
	bus = CISS_BIG_MAP_BUS(sc, ld->cl_lstatus->drive_rebuilding);
	target = CISS_BIG_MAP_BUS(sc, ld->cl_lstatus->drive_rebuilding);
	printf("being recovered, working on physical drive %d.%d, %u blocks remaining\n",
	       bus, target, ld->cl_lstatus->blocks_to_recover);
	break;
    case CISS_LSTATUS_EXPANDING:
	printf("being expanded, %u blocks remaining\n",
	       ld->cl_lstatus->blocks_to_recover);
	break;
    case CISS_LSTATUS_QUEUED_FOR_EXPANSION:
	printf("queued for expansion\n");
	break;
    case CISS_LSTATUS_FAILED:
	printf("queued for expansion\n");
	break;
    case CISS_LSTATUS_WRONG_PDRIVE:
	printf("wrong physical drive inserted\n");
	break;
    case CISS_LSTATUS_MISSING_PDRIVE:
	printf("missing a needed physical drive\n");
	break;
    case CISS_LSTATUS_BECOMING_READY:
	printf("becoming ready\n");
	break;
    }

    /* print failed physical drives */
    for (i = 0; i < CISS_BIG_MAP_ENTRIES / 8; i++) {
	bus = CISS_BIG_MAP_BUS(sc, ld->cl_lstatus->drive_failure_map[i]);
	target = CISS_BIG_MAP_TARGET(sc, ld->cl_lstatus->drive_failure_map[i]);
	if (bus == -1)
	    continue;
	ciss_printf(sc, "physical drive %d:%d (%x) failed\n", bus, target, 
		    ld->cl_lstatus->drive_failure_map[i]);
    }
}

#ifdef CISS_DEBUG
/************************************************************************
 * Print information about the controller/driver.
 */
static void
ciss_print_adapter(struct ciss_softc *sc)
{
    int		i;

    ciss_printf(sc, "ADAPTER:\n");
    for (i = 0; i < CISSQ_COUNT; i++) {
	ciss_printf(sc, "%s     %d/%d\n",
	    i == 0 ? "free" :
	    i == 1 ? "busy" : "complete",
	    sc->ciss_qstat[i].q_length,
	    sc->ciss_qstat[i].q_max);
    }
    ciss_printf(sc, "max_requests %d\n", sc->ciss_max_requests);
    ciss_printf(sc, "notify_head/tail %d/%d\n",
	sc->ciss_notify_head, sc->ciss_notify_tail);
    ciss_printf(sc, "flags %b\n", sc->ciss_flags,
	"\20\1notify_ok\2control_open\3aborting\4running\21fake_synch\22bmic_abort\n");

    for (i = 0; i < CISS_MAX_LOGICAL; i++) {
	ciss_printf(sc, "LOGICAL DRIVE %d:  ", i);
	ciss_print_ldrive(sc, sc->ciss_logical + i);
    }

    for (i = 1; i < sc->ciss_max_requests; i++)
	ciss_print_request(sc->ciss_request + i);

}

/* DDB hook */
static void
ciss_print0(void)
{
    struct ciss_softc	*sc;
    
    sc = devclass_get_softc(devclass_find("ciss"), 0);
    if (sc == NULL) {
	printf("no ciss controllers\n");
    } else {
	ciss_print_adapter(sc);
    }
}
#endif

/************************************************************************
 * Return a name for a logical drive status value.
 */
static const char *
ciss_name_ldrive_status(int status)
{
    switch (status) {
    case CISS_LSTATUS_OK:
	return("OK");
    case CISS_LSTATUS_FAILED:
	return("failed");
    case CISS_LSTATUS_NOT_CONFIGURED:
	return("not configured");
    case CISS_LSTATUS_INTERIM_RECOVERY:
	return("interim recovery");
    case CISS_LSTATUS_READY_RECOVERY:
	return("ready for recovery");
    case CISS_LSTATUS_RECOVERING:
	return("recovering");
    case CISS_LSTATUS_WRONG_PDRIVE:
	return("wrong physical drive inserted");
    case CISS_LSTATUS_MISSING_PDRIVE:
	return("missing physical drive");
    case CISS_LSTATUS_EXPANDING:
	return("expanding");
    case CISS_LSTATUS_BECOMING_READY:
	return("becoming ready");
    case CISS_LSTATUS_QUEUED_FOR_EXPANSION:
	return("queued for expansion");
    }
    return("unknown status");
}

/************************************************************************
 * Return an online/offline/nonexistent value for a logical drive
 * status value.
 */
static int
ciss_decode_ldrive_status(int status)
{
    switch(status) {
    case CISS_LSTATUS_NOT_CONFIGURED:
	return(CISS_LD_NONEXISTENT);

    case CISS_LSTATUS_OK:
    case CISS_LSTATUS_INTERIM_RECOVERY:
    case CISS_LSTATUS_READY_RECOVERY:
    case CISS_LSTATUS_RECOVERING:
    case CISS_LSTATUS_EXPANDING:
    case CISS_LSTATUS_QUEUED_FOR_EXPANSION:
	return(CISS_LD_ONLINE);

    case CISS_LSTATUS_FAILED:
    case CISS_LSTATUS_WRONG_PDRIVE:
    case CISS_LSTATUS_MISSING_PDRIVE:
    case CISS_LSTATUS_BECOMING_READY:
    default:
	return(CISS_LD_OFFLINE);
    }
}


/************************************************************************
 * Return a name for a logical drive's organisation.
 */
static const char *
ciss_name_ldrive_org(int org)
{
    switch(org) {
    case CISS_LDRIVE_RAID0:
	return("RAID 0");
    case CISS_LDRIVE_RAID1:
	return("RAID 1");
    case CISS_LDRIVE_RAID4:
	return("RAID 4");
    case CISS_LDRIVE_RAID5:
	return("RAID 5");
    }
    return("unkown");
}

/************************************************************************
 * Return a name for a command status value.
 */
static const char *
ciss_name_command_status(int status)
{
    switch(status) {
    case CISS_CMD_STATUS_SUCCESS:
	return("success");
    case CISS_CMD_STATUS_TARGET_STATUS:
	return("target status");
    case CISS_CMD_STATUS_DATA_UNDERRUN:
	return("data underrun");
    case CISS_CMD_STATUS_DATA_OVERRUN:
	return("data overrun");
    case CISS_CMD_STATUS_INVALID_COMMAND:
	return("invalid command");
    case CISS_CMD_STATUS_PROTOCOL_ERROR:
	return("protocol error");
    case CISS_CMD_STATUS_HARDWARE_ERROR:
	return("hardware error");
    case CISS_CMD_STATUS_CONNECTION_LOST:
	return("connection lost");
    case CISS_CMD_STATUS_ABORTED:
	return("aborted");
    case CISS_CMD_STATUS_ABORT_FAILED:
	return("abort failed");
    case CISS_CMD_STATUS_UNSOLICITED_ABORT:
	return("unsolicited abort");
    case CISS_CMD_STATUS_TIMEOUT:
	return("timeout");
    case CISS_CMD_STATUS_UNABORTABLE:
	return("unabortable");
    }
    return("unknown status");
}

/************************************************************************
 * Handle an open on the control device.
 */
static int
ciss_open(dev_t dev, int flags, int fmt, d_thread_t *p)
{
    struct ciss_softc	*sc;

    debug_called(1);
    
    sc = (struct ciss_softc *)dev->si_drv1;

    /* we might want to veto if someone already has us open */
	
    sc->ciss_flags |= CISS_FLAG_CONTROL_OPEN;
    return(0);
}

/************************************************************************
 * Handle the last close on the control device.
 */
static int
ciss_close(dev_t dev, int flags, int fmt, d_thread_t *p)
{
    struct ciss_softc	*sc;

    debug_called(1);
    
    sc = (struct ciss_softc *)dev->si_drv1;
    
    sc->ciss_flags &= ~CISS_FLAG_CONTROL_OPEN;
    return (0);
}

/********************************************************************************
 * Handle adapter-specific control operations.
 *
 * Note that the API here is compatible with the Linux driver, in order to
 * simplify the porting of Compaq's userland tools.
 */
static int
ciss_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, d_thread_t *p)
{
    struct ciss_softc		*sc;
    int				error;

    debug_called(1);

    sc = (struct ciss_softc *)dev->si_drv1;
    error = 0;

    switch(cmd) {
    case CCISS_GETPCIINFO:
    {
	cciss_pci_info_struct	*pis = (cciss_pci_info_struct *)addr;

	pis->bus = pci_get_bus(sc->ciss_dev);
	pis->dev_fn = pci_get_slot(sc->ciss_dev);
	pis->board_id = pci_get_devid(sc->ciss_dev);

	break;
    }
    
    case CCISS_GETINTINFO:
    {
	cciss_coalint_struct	*cis = (cciss_coalint_struct *)addr;

	cis->delay = sc->ciss_cfg->interrupt_coalesce_delay;
	cis->count = sc->ciss_cfg->interrupt_coalesce_count;

	break;
    }

    case CCISS_SETINTINFO:
    {
	cciss_coalint_struct	*cis = (cciss_coalint_struct *)addr;

	if ((cis->delay == 0) && (cis->count == 0)) {
	    error = EINVAL;
	    break;
	}

	/*
	 * XXX apparently this is only safe if the controller is idle,
	 *     we should suspend it before doing this.
	 */
	sc->ciss_cfg->interrupt_coalesce_delay = cis->delay;
	sc->ciss_cfg->interrupt_coalesce_count = cis->count;

	if (ciss_update_config(sc))
	    error = EIO;

	/* XXX resume the controller here */
	break;
    }

    case CCISS_GETNODENAME:
	bcopy(sc->ciss_cfg->server_name, (NodeName_type *)addr,
	      sizeof(NodeName_type));
	break;

    case CCISS_SETNODENAME:
	bcopy((NodeName_type *)addr, sc->ciss_cfg->server_name,
	      sizeof(NodeName_type));
	if (ciss_update_config(sc))
	    error = EIO;
	break;
	
    case CCISS_GETHEARTBEAT:
	*(Heartbeat_type *)addr = sc->ciss_cfg->heartbeat;
	break;

    case CCISS_GETBUSTYPES:
	*(BusTypes_type *)addr = sc->ciss_cfg->bus_types;
	break;

    case CCISS_GETFIRMVER:
	bcopy(sc->ciss_id->running_firmware_revision, (FirmwareVer_type *)addr,
	      sizeof(FirmwareVer_type));
	break;

    case CCISS_GETDRIVERVER:
	*(DriverVer_type *)addr = CISS_DRIVER_VERSION;
	break;

    case CCISS_REVALIDVOLS:
	/*
	 * This is a bit ugly; to do it "right" we really need
	 * to find any disks that have changed, kick CAM off them,
	 * then rescan only these disks.  It'd be nice if they
	 * a) told us which disk(s) they were going to play with,
	 * and b) which ones had arrived. 8(
	 */
	break;

    case CCISS_PASSTHRU:
	error = ciss_user_command(sc, (IOCTL_Command_struct *)addr);
	break;

    default:
	debug(0, "unknown ioctl 0x%lx", cmd);

	debug(1, "CCISS_GETPCIINFO:   0x%lx", CCISS_GETPCIINFO);
	debug(1, "CCISS_GETINTINFO:   0x%lx", CCISS_GETINTINFO);
	debug(1, "CCISS_SETINTINFO:   0x%lx", CCISS_SETINTINFO);
	debug(1, "CCISS_GETNODENAME:  0x%lx", CCISS_GETNODENAME);
	debug(1, "CCISS_SETNODENAME:  0x%lx", CCISS_SETNODENAME);
	debug(1, "CCISS_GETHEARTBEAT: 0x%lx", CCISS_GETHEARTBEAT);
	debug(1, "CCISS_GETBUSTYPES:  0x%lx", CCISS_GETBUSTYPES);
	debug(1, "CCISS_GETFIRMVER:   0x%lx", CCISS_GETFIRMVER);
	debug(1, "CCISS_GETDRIVERVER: 0x%lx", CCISS_GETDRIVERVER);
	debug(1, "CCISS_REVALIDVOLS:  0x%lx", CCISS_REVALIDVOLS);
	debug(1, "CCISS_PASSTHRU:     0x%lx", CCISS_PASSTHRU);

	error = ENOIOCTL;
	break;
    }

    return(error);
}
