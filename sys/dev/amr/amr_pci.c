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
 *	$FreeBSD: src/sys/dev/amr/amr_pci.c,v 1.1.2.1 2000/04/11 00:12:46 msmith Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/amr/amrio.h>
#include <dev/amr/amrreg.h>
#include <dev/amr/amrvar.h>

#if 0
#define debug(fmt, args...)	printf("%s: " fmt "\n", __FUNCTION__ , ##args)
#else
#define debug(fmt, args...)
#endif

static int			amr_pci_probe(device_t dev);
static int			amr_pci_attach(device_t dev);

static device_method_t amr_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	amr_pci_probe),
    DEVMETHOD(device_attach,	amr_pci_attach),
    DEVMETHOD(device_detach,	amr_detach),
    DEVMETHOD(device_shutdown,	amr_shutdown),
    DEVMETHOD(device_suspend,	amr_suspend),
    DEVMETHOD(device_resume,	amr_resume),

    DEVMETHOD(bus_print_child,	bus_generic_print_child),
    DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
    { 0, 0 }
};

static driver_t amr_pci_driver = {
	"amr",
	amr_methods,
	sizeof(struct amr_softc)
};

DRIVER_MODULE(amr, pci, amr_pci_driver, amr_devclass, 0, 0);

static struct 
{
    int		vendor;
    int		device;
    int		flag;
#define PROBE_SIGNATURE	(1<<0)
} amr_device_ids[] = {
    {0x101e, 0x9010, 0},
    {0x101e, 0x9060, 0},
    {0x8086, 0x1960, PROBE_SIGNATURE},	/* generic i960RD, check signature */
    {0, 0, 0}
};
    
static int
amr_pci_probe(device_t dev)
{
    int		i;

    debug("called");

    for (i = 0; amr_device_ids[i].vendor != 0; i++) {
	if ((pci_get_vendor(dev) == amr_device_ids[i].vendor) &&
	    (pci_get_device(dev) == amr_device_ids[i].device)) {

	    /* do we need to test for a signature? */
	    if ((amr_device_ids[i].flag & PROBE_SIGNATURE) &&
		(pci_read_config(dev, AMR_CFG_SIG, 2) != AMR_SIGNATURE))
		continue;
	    device_set_desc(dev, "AMI MegaRAID");
	    return(0);
	}
    }
    return(ENXIO);
}

static int
amr_pci_attach(device_t dev)
{
    struct amr_softc	*sc;
    int			rid, rtype, error;
    u_int32_t		command;

    debug("called");

    /*
     * Initialise softc.
     */
    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc));
    sc->amr_dev = dev;

    /*
     * Determine board type..
     */
    command = pci_read_config(dev, PCIR_COMMAND, 1);
    if ((pci_get_vendor(dev) == 0x8086) && (pci_get_device(dev) == 0x1960)) {
	sc->amr_type = AMR_TYPE_QUARTZ;

	/*
	 * Make sure we are going to be able to talk to this board.
	 */
	if ((command & PCIM_CMD_MEMEN) == 0) {
	    device_printf(dev, "memory window not available\n");
	    return(ENXIO);
	}

    } else {
	sc->amr_type = AMR_TYPE_STD;

	/*
	 * Make sure we are going to be able to talk to this board.
	 */
	if ((command & PCIM_CMD_PORTEN) == 0) {
	    device_printf(dev, "I/O window not available\n");
	    return(ENXIO);
	}
    }

    /* force the busmaster enable bit on */
    if (!(command & PCIM_CMD_BUSMASTEREN)) {
	device_printf(dev, "busmaster bit not set, enabling\n");
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 2);
    }

    /*
     * Allocate the PCI register window.
     */
    rid = AMR_CFG_BASE;
    rtype = (sc->amr_type == AMR_TYPE_QUARTZ) ? SYS_RES_MEMORY : SYS_RES_IOPORT;
    sc->amr_reg = bus_alloc_resource(dev, rtype, &rid, 0, ~0, 1, RF_ACTIVE);
    if (sc->amr_reg == NULL) {
	device_printf(sc->amr_dev, "couldn't allocate register window\n");
	amr_free(sc);
	return(ENXIO);
    }
    sc->amr_btag = rman_get_bustag(sc->amr_reg);
    sc->amr_bhandle = rman_get_bushandle(sc->amr_reg);

    /*
     * Allocate the parent bus DMA tag appropriate for PCI.
     */
    error = bus_dma_tag_create(NULL, 			/* parent */
			       1, 0, 			/* alignment, boundary */
			       BUS_SPACE_MAXADDR_32BIT, /* lowaddr */
			       BUS_SPACE_MAXADDR, 	/* highaddr */
			       NULL, NULL, 		/* filter, filterarg */
			       MAXBSIZE, AMR_NSEG,	/* maxsize, nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       &sc->amr_parent_dmat);
    if (error != 0) {
	device_printf(dev, "can't allocate parent DMA tag\n");
	amr_free(sc);
	return(ENOMEM);
    }

    /*
     * Do bus-independant initialisation.
     */
    error = amr_attach(sc);
    if (error != 0) {
	amr_free(sc);
	return(error);
    }
    
    /*
     * Start the controller.
     */
    amr_startup(sc);
    return(0);
}
