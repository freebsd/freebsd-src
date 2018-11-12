/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 *   This module supports the DEC DEFPA PCI FDDI Controller
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h> 
#include <net/fddi.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pdq/pdq_freebsd.h>
#include <dev/pdq/pdqreg.h>

#define	DEC_VENDORID		0x1011
#define	DEFPA_CHIPID		0x000F

#define	DEFPA_LATENCY	0x88

#define	PCI_CFLT	0x0C	/* Configuration Latency */
#define	PCI_CBMA	0x10	/* Configuration Base Memory Address */
#define	PCI_CBIO	0x14	/* Configuration Base I/O Address */

static int	pdq_pci_probe		(device_t);
static int	pdq_pci_attach		(device_t);
static int	pdq_pci_detach		(device_t);
static int	pdq_pci_shutdown	(device_t);
static void	pdq_pci_ifintr		(void *);

static void
pdq_pci_ifintr(void *arg)
{
    pdq_softc_t *sc;

    sc = arg;

    PDQ_LOCK(sc);
    (void) pdq_interrupt(sc->sc_pdq);
    PDQ_UNLOCK(sc);

    return;
}

/*
 * This is the PCI configuration support.
 */
static int
pdq_pci_probe(device_t dev)
{
    if (pci_get_vendor(dev) == DEC_VENDORID &&
	    pci_get_device(dev) == DEFPA_CHIPID) {
	device_set_desc(dev, "Digital DEFPA PCI FDDI Controller");
	return (BUS_PROBE_DEFAULT);
    }

    return (ENXIO);
}

static int
pdq_pci_attach(device_t dev)
{
    pdq_softc_t *sc;
    u_int32_t command;
    int error;

    sc = device_get_softc(dev);

    sc->dev = dev;

    /*
     * Map control/status registers.
     */
    pci_enable_busmaster(dev);

    command = pci_read_config(dev, PCIR_LATTIMER, 1);
    if (command < DEFPA_LATENCY) {
	command = DEFPA_LATENCY;
	pci_write_config(dev, PCIR_LATTIMER, command, 1);
    }

    sc->mem_rid = PCI_CBMA;
    sc->mem_type = SYS_RES_MEMORY;
    sc->mem = bus_alloc_resource_any(dev, sc->mem_type, &sc->mem_rid,
				     RF_ACTIVE);
    if (!sc->mem) {
	device_printf(dev, "Unable to allocate I/O space resource.\n");
	error = ENXIO;
	goto bad;
    }
    sc->mem_bsh = rman_get_bushandle(sc->mem);
    sc->mem_bst = rman_get_bustag(sc->mem);

    sc->irq_rid = 0;
    sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
				     RF_SHAREABLE | RF_ACTIVE);
    if (!sc->irq) {
	device_printf(dev, "Unable to allocate interrupt resource.\n");
	error = ENXIO;
	goto bad;
    }

    error = pdq_ifattach(sc, sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, PDQ_DEFPA);
    if (error)
	goto bad;
    
    error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE, NULL,
			   pdq_pci_ifintr, sc, &sc->irq_ih);
    if (error) {
	device_printf(dev, "Failed to setup interrupt handler.\n");
	pdq_ifdetach(sc);
	return (error);
    }


    return (0);
bad:
    pdq_free(dev);
    return (error);
}

static int
pdq_pci_detach (dev)
    device_t	dev;
{
    pdq_softc_t *sc;

    sc = device_get_softc(dev);
    pdq_ifdetach(sc);

    return (0);
}

static int
pdq_pci_shutdown(device_t dev)
{
    pdq_softc_t *sc;

    sc = device_get_softc(dev);
    PDQ_LOCK(sc);
    pdq_hwreset(sc->sc_pdq);
    PDQ_UNLOCK(sc);

    return (0);
}

static device_method_t pdq_pci_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	pdq_pci_probe),
    DEVMETHOD(device_attach,	pdq_pci_attach),
    DEVMETHOD(device_detach,	pdq_pci_detach),
    DEVMETHOD(device_shutdown,	pdq_pci_shutdown),

    { 0, 0 }
};

static driver_t pdq_pci_driver = {
    "fpa",
    pdq_pci_methods,
    sizeof(pdq_softc_t),
};

DRIVER_MODULE(fpa, pci, pdq_pci_driver, pdq_devclass, 0, 0);
MODULE_DEPEND(fpa, pci, 1, 1, 1);
MODULE_DEPEND(fpa, fddi, 1, 1, 1);
