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
 * PCI bus interface and resource allocation.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <dev/aac/aac_compat.h>
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

#include <dev/aac/aacreg.h>
#include <dev/aac/aacvar.h>

static int		aac_pci_probe(device_t dev);
static int		aac_pci_attach(device_t dev);

static device_method_t aac_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	aac_pci_probe),
    DEVMETHOD(device_attach,	aac_pci_attach),
    DEVMETHOD(device_detach,	aac_detach),
    DEVMETHOD(device_shutdown,	aac_shutdown),
    DEVMETHOD(device_suspend,	aac_suspend),
    DEVMETHOD(device_resume,	aac_resume),

    DEVMETHOD(bus_print_child,	bus_generic_print_child),
    DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
    { 0, 0 }
};

static driver_t aac_pci_driver = {
	"aac",
	aac_methods,
	sizeof(struct aac_softc)
};

DRIVER_MODULE(aac, pci, aac_pci_driver, aac_devclass, 0, 0);

struct aac_ident
{
    u_int16_t		vendor;
    u_int16_t		device;
    u_int16_t		subvendor;
    u_int16_t		subdevice;
    int			hwif;
    char		*desc;
} aac_identifiers[] = {
    {0x1028, 0x0001, 0x1028, 0x0001, AAC_HWIF_I960RX,    "Dell PERC 2/Si"},
    {0x1028, 0x0002, 0x1028, 0x0002, AAC_HWIF_I960RX,    "Dell PERC 3/Di"},
    {0x1028, 0x0003, 0x1028, 0x0003, AAC_HWIF_I960RX,    "Dell PERC 3/Si"},
    {0x9005, 0x0282, 0x9005, 0x0282, AAC_HWIF_I960RX,    "Adaptec AAC-2622"},
    {0x1011, 0x0046, 0x9005, 0x0364, AAC_HWIF_STRONGARM, "Adaptec AAC-364"},
    {0x1011, 0x0046, 0x9005, 0x0365, AAC_HWIF_STRONGARM, "Adaptec AAC-3642"},
    {0x1011, 0x0046, 0x9005, 0x1364, AAC_HWIF_STRONGARM, "Dell PERC 2/QC"},
    {0x1011, 0x0046, 0x9005, 0x1365, AAC_HWIF_STRONGARM, "Dell PERC 3/QC"},	/* XXX guess */
    {0x1011, 0x0046, 0x103c, 0x10c2, AAC_HWIF_STRONGARM, "HP NetRaid-4M"},
    {0, 0, 0, 0, 0, 0}
};

/********************************************************************************
 * Determine whether this is one of our supported adapters.
 */
static int
aac_pci_probe(device_t dev)
{
    struct aac_ident	*m;

    debug_called(1);

    for (m = aac_identifiers; m->vendor != 0; m++) {
	if ((m->vendor == pci_get_vendor(dev)) &&
	    (m->device == pci_get_device(dev)) &&
	    ((m->subvendor == 0) || (m->subvendor == pci_get_subvendor(dev))) &&
	    ((m->subdevice == 0) || ((m->subdevice == pci_get_subdevice(dev))))) {
	    
	    device_set_desc(dev, m->desc);
	    return(-10);	/* allow room to be overridden */
	}
    }
    return(ENXIO);
}

/********************************************************************************
 * Allocate resources for our device, set up the bus interface.
 */
static int
aac_pci_attach(device_t dev)
{
    struct aac_softc	*sc;
    int			i, error;
    u_int32_t		command;

    debug_called(1);

    /*
     * Initialise softc.
     */
    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc));
    sc->aac_dev = dev;

    /* assume failure is 'not configured' */
    error = ENXIO;

    /* 
     * Verify that the adapter is correctly set up in PCI space.
     */
    command = pci_read_config(sc->aac_dev, PCIR_COMMAND, 2);
    command |= PCIM_CMD_BUSMASTEREN;
    pci_write_config(dev, PCIR_COMMAND, command, 2);
    command = pci_read_config(sc->aac_dev, PCIR_COMMAND, 2);
    if (!(command & PCIM_CMD_BUSMASTEREN)) {
	device_printf(sc->aac_dev, "can't enable bus-master feature\n");
	goto out;
    }
    if ((command & PCIM_CMD_MEMEN) == 0) {
	device_printf(sc->aac_dev, "memory window not available\n");
	goto out;
    }

    /*
     * Allocate the PCI register window.
     */
    sc->aac_regs_rid = 0x10;	/* first base address register */
    if ((sc->aac_regs_resource = bus_alloc_resource(sc->aac_dev, SYS_RES_MEMORY, &sc->aac_regs_rid, 
						    0, ~0, 1, RF_ACTIVE)) == NULL) {
	device_printf(sc->aac_dev, "couldn't allocate register window\n");
	goto out;
    }
    sc->aac_btag = rman_get_bustag(sc->aac_regs_resource);
    sc->aac_bhandle = rman_get_bushandle(sc->aac_regs_resource);

    /* 
     * Allocate and connect our interrupt.
     */
    sc->aac_irq_rid = 0;
    if ((sc->aac_irq = bus_alloc_resource(sc->aac_dev, SYS_RES_IRQ, &sc->aac_irq_rid, 
					  0, ~0, 1, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
	device_printf(sc->aac_dev, "can't allocate interrupt\n");
	goto out;
    }
    if (bus_setup_intr(sc->aac_dev, sc->aac_irq, INTR_TYPE_BIO,  aac_intr, sc, &sc->aac_intr)) {
	device_printf(sc->aac_dev, "can't set up interrupt\n");
	goto out;
    }

    /* assume failure is 'out of memory' */
    error = ENOMEM;

    /*
     * Allocate the parent bus DMA tag appropriate for our PCI interface.
     * 
     * Note that some of these controllers are 64-bit capable.
     */
    if (bus_dma_tag_create(NULL, 			/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   MAXBSIZE, AAC_MAXSGENTRIES,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   BUS_DMA_ALLOCNOW,		/* flags */
			   &sc->aac_parent_dmat)) {
	device_printf(sc->aac_dev, "can't allocate parent DMA tag\n");
	goto out;
    }

    /*
     * Create DMA tag for mapping buffers into controller-addressable space.
     */
    if (bus_dma_tag_create(sc->aac_parent_dmat, 	/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   MAXBSIZE, AAC_MAXSGENTRIES,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   &sc->aac_buffer_dmat)) {
	device_printf(sc->aac_dev, "can't allocate buffer DMA tag\n");
	goto out;
    }

    /*
     * Create DMA tag for mapping FIBs into controller-addressable space..
     */
    if (bus_dma_tag_create(sc->aac_parent_dmat, 			/* parent */
			   1, 0, 					/* alignment, boundary */
			   BUS_SPACE_MAXADDR,				/* lowaddr */
			   BUS_SPACE_MAXADDR, 				/* highaddr */
			   NULL, NULL, 					/* filter, filterarg */
			   AAC_CLUSTER_COUNT * sizeof(struct aac_fib), 1,/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,			/* maxsegsize */
			   0,						/* flags */
			   &sc->aac_fib_dmat)) {
	device_printf(sc->aac_dev, "can't allocate FIB DMA tag\n");
	goto out;
    }

    /* 
     * Detect the hardware interface version, set up the bus interface indirection.
     */
    for (i = 0; aac_identifiers[i].vendor != 0; i++) {
	if ((aac_identifiers[i].vendor == pci_get_vendor(dev)) &&
	    (aac_identifiers[i].device == pci_get_device(dev))) {
	    sc->aac_hwif = aac_identifiers[i].hwif;
	    switch(sc->aac_hwif) {
	    case AAC_HWIF_I960RX:
		debug(2, "set hardware up for i960Rx");
		sc->aac_if = aac_rx_interface;
		break;

	    case AAC_HWIF_STRONGARM:
		debug(2, "set hardware up for StrongARM");
		sc->aac_if = aac_sa_interface;
		break;
	    }
	    break;
	}
    }

    /*
     * Do bus-independent initialisation.
     */
    error = aac_attach(sc);
    
out:
    if (error)
	aac_free(sc);
    return(error);
}
