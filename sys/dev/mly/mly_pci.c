/*-
 * Copyright (c) 2000, 2001 Michael Smith
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/mly/mlyreg.h>
#include <dev/mly/mlyio.h>
#include <dev/mly/mlyvar.h>

static int	mly_pci_probe(device_t dev);
static int	mly_pci_attach(device_t dev);
static int	mly_pci_detach(device_t dev);
static int	mly_pci_shutdown(device_t dev);
static int	mly_pci_suspend(device_t dev); 
static int	mly_pci_resume(device_t dev);
static void	mly_pci_intr(void *arg);

static int	mly_sg_map(struct mly_softc *sc);
static void	mly_sg_map_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error);
static int	mly_mmbox_map(struct mly_softc *sc);
static void	mly_mmbox_map_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error);

static device_method_t mly_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	mly_pci_probe),
    DEVMETHOD(device_attach,	mly_pci_attach),
    DEVMETHOD(device_detach,	mly_pci_detach),
    DEVMETHOD(device_shutdown,	mly_pci_shutdown),
    DEVMETHOD(device_suspend,	mly_pci_suspend),
    DEVMETHOD(device_resume,	mly_pci_resume),
    { 0, 0 }
};

static driver_t mly_pci_driver = {
	"mly",
	mly_methods,
	sizeof(struct mly_softc)
};

static devclass_t	mly_devclass;
DRIVER_MODULE(mly, pci, mly_pci_driver, mly_devclass, 0, 0);

struct mly_ident
{
    u_int16_t		vendor;
    u_int16_t		device;
    u_int16_t		subvendor;
    u_int16_t		subdevice;
    int			hwif;
    char		*desc;
} mly_identifiers[] = {
    {0x1069, 0xba56, 0x1069, 0x0040, MLY_HWIF_STRONGARM, "Mylex eXtremeRAID 2000"},
    {0x1069, 0xba56, 0x1069, 0x0030, MLY_HWIF_STRONGARM, "Mylex eXtremeRAID 3000"},
    {0x1069, 0x0050, 0x1069, 0x0050, MLY_HWIF_I960RX,    "Mylex AcceleRAID 352"},
    {0x1069, 0x0050, 0x1069, 0x0052, MLY_HWIF_I960RX,    "Mylex AcceleRAID 170"},
    {0x1069, 0x0050, 0x1069, 0x0054, MLY_HWIF_I960RX,    "Mylex AcceleRAID 160"},
    {0, 0, 0, 0, 0, 0}
};

/********************************************************************************
 ********************************************************************************
                                                                    Bus Interface
 ********************************************************************************
 ********************************************************************************/

static int
mly_pci_probe(device_t dev)
{
    struct mly_ident	*m;

    debug_called(1);

    for (m = mly_identifiers; m->vendor != 0; m++) {
	if ((m->vendor == pci_get_vendor(dev)) &&
	    (m->device == pci_get_device(dev)) &&
	    ((m->subvendor == 0) || ((m->subvendor == pci_get_subvendor(dev)) &&
				     (m->subdevice == pci_get_subdevice(dev))))) {
	    
	    device_set_desc(dev, m->desc);
	    return(-10);	/* allow room to be overridden */
	}
    }
    return(ENXIO);
}

static int
mly_pci_attach(device_t dev)
{
    struct mly_softc	*sc;
    int			i, error;
    u_int32_t		command;

    debug_called(1);

    /*
     * Initialise softc.
     */
    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc));
    sc->mly_dev = dev;

#ifdef MLY_DEBUG
    if (device_get_unit(sc->mly_dev) == 0)
	mly_softc0 = sc;
#endif    

    /* assume failure is 'not configured' */
    error = ENXIO;

    /* 
     * Verify that the adapter is correctly set up in PCI space.
     */
    command = pci_read_config(sc->mly_dev, PCIR_COMMAND, 2);
    command |= PCIM_CMD_BUSMASTEREN;
    pci_write_config(dev, PCIR_COMMAND, command, 2);
    command = pci_read_config(sc->mly_dev, PCIR_COMMAND, 2);
    if (!(command & PCIM_CMD_BUSMASTEREN)) {
	mly_printf(sc, "can't enable busmaster feature\n");
	goto fail;
    }
    if ((command & PCIM_CMD_MEMEN) == 0) {
	mly_printf(sc, "memory window not available\n");
	goto fail;
    }

    /*
     * Allocate the PCI register window.
     */
    sc->mly_regs_rid = PCIR_MAPS;	/* first base address register */
    if ((sc->mly_regs_resource = bus_alloc_resource(sc->mly_dev, SYS_RES_MEMORY, &sc->mly_regs_rid, 
						    0, ~0, 1, RF_ACTIVE)) == NULL) {
	mly_printf(sc, "can't allocate register window\n");
	goto fail;
    }
    sc->mly_btag = rman_get_bustag(sc->mly_regs_resource);
    sc->mly_bhandle = rman_get_bushandle(sc->mly_regs_resource);

    /* 
     * Allocate and connect our interrupt.
     */
    sc->mly_irq_rid = 0;
    if ((sc->mly_irq = bus_alloc_resource(sc->mly_dev, SYS_RES_IRQ, &sc->mly_irq_rid, 
					  0, ~0, 1, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
	mly_printf(sc, "can't allocate interrupt\n");
	goto fail;
    }
    if (bus_setup_intr(sc->mly_dev, sc->mly_irq, INTR_TYPE_CAM,  mly_pci_intr, sc, &sc->mly_intr)) {
	mly_printf(sc, "can't set up interrupt\n");
	goto fail;
    }

    /* assume failure is 'out of memory' */
    error = ENOMEM;

    /*
     * Allocate the parent bus DMA tag appropriate for our PCI interface.
     * 
     * Note that all of these controllers are 64-bit capable.
     */
    if (bus_dma_tag_create(NULL, 			/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   MAXBSIZE, MLY_MAXSGENTRIES,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   BUS_DMA_ALLOCNOW,		/* flags */
			   &sc->mly_parent_dmat)) {
	mly_printf(sc, "can't allocate parent DMA tag\n");
	goto fail;
    }

    /*
     * Create DMA tag for mapping buffers into controller-addressable space.
     */
    if (bus_dma_tag_create(sc->mly_parent_dmat, 	/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   MAXBSIZE, MLY_MAXSGENTRIES,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   &sc->mly_buffer_dmat)) {
	mly_printf(sc, "can't allocate buffer DMA tag\n");
	goto fail;
    }

    /*
     * Initialise the DMA tag for command packets.
     */
    if (bus_dma_tag_create(sc->mly_parent_dmat,		/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   sizeof(union mly_command_packet) * MLY_MAXCOMMANDS, 1,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   &sc->mly_packet_dmat)) {
	mly_printf(sc, "can't allocate command packet DMA tag\n");
	goto fail;
    }

    /* 
     * Detect the hardware interface version 
     */
    for (i = 0; mly_identifiers[i].vendor != 0; i++) {
	if ((mly_identifiers[i].vendor == pci_get_vendor(dev)) &&
	    (mly_identifiers[i].device == pci_get_device(dev))) {
	    sc->mly_hwif = mly_identifiers[i].hwif;
	    switch(sc->mly_hwif) {
	    case MLY_HWIF_I960RX:
		debug(2, "set hardware up for i960RX");
		sc->mly_doorbell_true = 0x00;
		sc->mly_command_mailbox =  MLY_I960RX_COMMAND_MAILBOX;
		sc->mly_status_mailbox =   MLY_I960RX_STATUS_MAILBOX;
		sc->mly_idbr =             MLY_I960RX_IDBR;
		sc->mly_odbr =             MLY_I960RX_ODBR;
		sc->mly_error_status =     MLY_I960RX_ERROR_STATUS;
		sc->mly_interrupt_status = MLY_I960RX_INTERRUPT_STATUS;
		sc->mly_interrupt_mask =   MLY_I960RX_INTERRUPT_MASK;
		break;
	    case MLY_HWIF_STRONGARM:
		debug(2, "set hardware up for StrongARM");
		sc->mly_doorbell_true = 0xff;		/* doorbell 'true' is 0 */
		sc->mly_command_mailbox =  MLY_STRONGARM_COMMAND_MAILBOX;
		sc->mly_status_mailbox =   MLY_STRONGARM_STATUS_MAILBOX;
		sc->mly_idbr =             MLY_STRONGARM_IDBR;
		sc->mly_odbr =             MLY_STRONGARM_ODBR;
		sc->mly_error_status =     MLY_STRONGARM_ERROR_STATUS;
		sc->mly_interrupt_status = MLY_STRONGARM_INTERRUPT_STATUS;
		sc->mly_interrupt_mask =   MLY_STRONGARM_INTERRUPT_MASK;
		break;
	    }
	    break;
	}
    }

    /*
     * Create the scatter/gather mappings.
     */
    if ((error = mly_sg_map(sc)))
	goto fail;

    /*
     * Allocate and map the memory mailbox
     */
    if ((error = mly_mmbox_map(sc)))
	goto fail;

    /*
     * Do bus-independent initialisation.
     */
    if ((error = mly_attach(sc)))
	goto fail;
    
    return(0);
	    
fail:
    mly_free(sc);
    return(error);
}

/********************************************************************************
 * Disconnect from the controller completely, in preparation for unload.
 */
static int
mly_pci_detach(device_t dev)
{
    struct mly_softc	*sc = device_get_softc(dev);
    int			error;

    debug_called(1);

    if (sc->mly_state & MLY_STATE_OPEN)
	return(EBUSY);

    if ((error = mly_pci_shutdown(dev)))
	return(error);

    mly_free(sc);

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
static int
mly_pci_shutdown(device_t dev)
{
    struct mly_softc	*sc = device_get_softc(dev);

    debug_called(1);

    mly_detach(sc);
    return(0);
}

/********************************************************************************
 * Bring the controller to a quiescent state, ready for system suspend.
 *
 * We can't assume that the controller is not active at this point, so we need
 * to mask interrupts.
 */
static int
mly_pci_suspend(device_t dev)
{
    struct mly_softc	*sc = device_get_softc(dev);
    int			s;

    debug_called(1);
    s = splcam();
    mly_detach(sc);
    splx(s);
    return(0);
}

/********************************************************************************
 * Bring the controller back to a state ready for operation.
 */
static int
mly_pci_resume(device_t dev)
{
    struct mly_softc	*sc = device_get_softc(dev);

    debug_called(1);
    sc->mly_state &= ~MLY_STATE_SUSPEND;
    MLY_UNMASK_INTERRUPTS(sc);
    return(0);
}

/*******************************************************************************
 * Take an interrupt, or be poked by other code to look for interrupt-worthy
 * status.
 */
static void
mly_pci_intr(void *arg)
{
    struct mly_softc	*sc = (struct mly_softc *)arg;

    debug_called(3);

    /* collect finished commands, queue anything waiting */
    mly_done(sc);
};

/********************************************************************************
 ********************************************************************************
                                                Bus-dependant Resource Management
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Allocate memory for the scatter/gather tables
 */
static int
mly_sg_map(struct mly_softc *sc)
{
    size_t	segsize;

    debug_called(1);

    /*
     * Create a single tag describing a region large enough to hold all of
     * the s/g lists we will need.
     */
    segsize = sizeof(struct mly_sg_entry) * MLY_MAXCOMMANDS * MLY_MAXSGENTRIES;
    if (bus_dma_tag_create(sc->mly_parent_dmat,		/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   segsize, 1,			/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   &sc->mly_sg_dmat)) {
	mly_printf(sc, "can't allocate scatter/gather DMA tag\n");
	return(ENOMEM);
    }

    /*
     * Allocate enough s/g maps for all commands and permanently map them into
     * controller-visible space.
     *	
     * XXX this assumes we can get enough space for all the s/g maps in one 
     * contiguous slab.
     */
    if (bus_dmamem_alloc(sc->mly_sg_dmat, (void **)&sc->mly_sg_table, BUS_DMA_NOWAIT, &sc->mly_sg_dmamap)) {
	mly_printf(sc, "can't allocate s/g table\n");
	return(ENOMEM);
    }
    bus_dmamap_load(sc->mly_sg_dmat, sc->mly_sg_dmamap, sc->mly_sg_table, segsize, mly_sg_map_helper, sc, 0);
    return(0);
}

/********************************************************************************
 * Save the physical address of the base of the s/g table.
 */
static void
mly_sg_map_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct mly_softc	*sc = (struct mly_softc *)arg;

    debug_called(2);

    /* save base of s/g table's address in bus space */
    sc->mly_sg_busaddr = segs->ds_addr;
}

/********************************************************************************
 * Allocate memory for the memory-mailbox interface
 */
static int
mly_mmbox_map(struct mly_softc *sc)
{

    /*
     * Create a DMA tag for a single contiguous region large enough for the
     * memory mailbox structure.
     */
    if (bus_dma_tag_create(sc->mly_parent_dmat,		/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   sizeof(struct mly_mmbox), 1,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   &sc->mly_mmbox_dmat)) {
	mly_printf(sc, "can't allocate memory mailbox DMA tag\n");
	return(ENOMEM);
    }

    /*
     * Allocate the buffer
     */
    if (bus_dmamem_alloc(sc->mly_mmbox_dmat, (void **)&sc->mly_mmbox, BUS_DMA_NOWAIT, &sc->mly_mmbox_dmamap)) {
	mly_printf(sc, "can't allocate memory mailbox\n");
	return(ENOMEM);
    }
    bus_dmamap_load(sc->mly_mmbox_dmat, sc->mly_mmbox_dmamap, sc->mly_mmbox, sizeof(struct mly_mmbox), 
		    mly_mmbox_map_helper, sc, 0);
    bzero(sc->mly_mmbox, sizeof(*sc->mly_mmbox));
    return(0);

}

/********************************************************************************
 * Save the physical address of the memory mailbox 
 */
static void
mly_mmbox_map_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct mly_softc	*sc = (struct mly_softc *)arg;

    debug_called(2);

    sc->mly_mmbox_busaddr = segs->ds_addr;
}

/********************************************************************************
 * Free all of the resources associated with (sc)
 *
 * Should not be called if the controller is active.
 */
void
mly_free(struct mly_softc *sc)
{
    struct mly_command	*mc;
    
    debug_called(1);

    /* detach from CAM */
    mly_cam_detach(sc);

    /* throw away command buffer DMA maps */
    while (mly_alloc_command(sc, &mc) == 0)
	bus_dmamap_destroy(sc->mly_buffer_dmat, mc->mc_datamap);

    /* release the packet storage */
    if (sc->mly_packet != NULL) {
	bus_dmamap_unload(sc->mly_packet_dmat, sc->mly_packetmap);
	bus_dmamem_free(sc->mly_packet_dmat, sc->mly_packet, sc->mly_packetmap);
    }

    /* throw away the controllerinfo structure */
    if (sc->mly_controllerinfo != NULL)
	free(sc->mly_controllerinfo, M_DEVBUF);

    /* throw away the controllerparam structure */
    if (sc->mly_controllerparam != NULL)
	free(sc->mly_controllerparam, M_DEVBUF);

    /* destroy data-transfer DMA tag */
    if (sc->mly_buffer_dmat)
	bus_dma_tag_destroy(sc->mly_buffer_dmat);

    /* free and destroy DMA memory and tag for s/g lists */
    if (sc->mly_sg_table) {
	bus_dmamap_unload(sc->mly_sg_dmat, sc->mly_sg_dmamap);
	bus_dmamem_free(sc->mly_sg_dmat, sc->mly_sg_table, sc->mly_sg_dmamap);
    }
    if (sc->mly_sg_dmat)
	bus_dma_tag_destroy(sc->mly_sg_dmat);

    /* free and destroy DMA memory and tag for memory mailbox */
    if (sc->mly_mmbox) {
	bus_dmamap_unload(sc->mly_mmbox_dmat, sc->mly_mmbox_dmamap);
	bus_dmamem_free(sc->mly_mmbox_dmat, sc->mly_mmbox, sc->mly_mmbox_dmamap);
    }
    if (sc->mly_mmbox_dmat)
	bus_dma_tag_destroy(sc->mly_mmbox_dmat);

    /* disconnect the interrupt handler */
    if (sc->mly_intr)
	bus_teardown_intr(sc->mly_dev, sc->mly_irq, sc->mly_intr);
    if (sc->mly_irq != NULL)
	bus_release_resource(sc->mly_dev, SYS_RES_IRQ, sc->mly_irq_rid, sc->mly_irq);

    /* destroy the parent DMA tag */
    if (sc->mly_parent_dmat)
	bus_dma_tag_destroy(sc->mly_parent_dmat);

    /* release the register window mapping */
    if (sc->mly_regs_resource != NULL)
	bus_release_resource(sc->mly_dev, SYS_RES_MEMORY, sc->mly_regs_rid, sc->mly_regs_resource);
}

