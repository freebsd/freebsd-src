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
 *    derived from this software withough specific prior written permission
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
 * $FreeBSD$
 *
 */

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 *   This module supports the DEC DEFPA PCI FDDI Controller
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>

#include <net/ethernet.h>
#include <net/if_arp.h>
#include <dev/pdq/pdqvar.h>
#include <dev/pdq/pdqreg.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>


#define	DEC_VENDORID		0x1011
#define	DEFPA_CHIPID		0x000F

#define	DEFPA_LATENCY	0x88

#define	PCI_CFLT	0x0C	/* Configuration Latency */
#define	PCI_CBMA	0x10	/* Configuration Base Memory Address */
#define	PCI_CBIO	0x14	/* Configuration Base I/O Address */

static void
pdq_pci_ifintr(void *arg)
{
    pdq_softc_t *sc;

    sc = device_get_softc(arg);
    (void) pdq_interrupt(sc->sc_pdq);
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
	return 0;
    }
    return ENXIO;
}

static int
pdq_pci_attach(device_t dev)
{
    pdq_softc_t *sc;
    int data;
    struct resource *memres, *irqres;
    int rid;
    void *ih;

    memres = NULL;
    irqres = NULL;
    sc = device_get_softc(dev);

    data = pci_read_config(dev, PCIR_LATTIMER, 1);
    if (data < DEFPA_LATENCY) {
	data = DEFPA_LATENCY;
	pci_write_config(dev, PCIR_LATTIMER, data, 1);
    }

    rid = PCI_CBMA;
    memres = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1, RF_ACTIVE);
    if (!memres)
	goto bad;

    sc->sc_if.if_name = "fpa";
    sc->sc_if.if_unit = device_get_unit(dev);
    sc->sc_membase = (pdq_bus_memaddr_t) rman_get_virtual(memres);
    sc->sc_pdq = pdq_initialize(PDQ_BUS_PCI, sc->sc_membase,
				sc->sc_if.if_name, sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL)
	goto bad;
    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
    pdq_ifattach(sc, NULL);
    rid = 0;
    irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				       RF_SHAREABLE | RF_ACTIVE);
    if (!irqres)
	goto bad;
    if (bus_setup_intr(dev, irqres, INTR_TYPE_NET, pdq_pci_ifintr,
		       (void *)dev, &ih))
	goto bad;
    return 0;

bad:
    if (memres)
	bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, memres);
    if (irqres)
	bus_release_resource(dev, SYS_RES_IRQ, 0, irqres);
    return ENXIO;
}

static void
pdq_pci_shutdown(device_t dev)
{
    pdq_softc_t *sc;

    sc = device_get_softc(dev);
    pdq_hwreset(sc->sc_pdq);
}

static device_method_t pdq_pci_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	pdq_pci_probe),
    DEVMETHOD(device_attach,	pdq_pci_attach),
    DEVMETHOD(device_shutdown,	pdq_pci_shutdown),
    { 0, 0 }
};
static driver_t pdq_pci_driver = {
    "fpa",
    pdq_pci_methods,
    sizeof(pdq_softc_t),
};
static devclass_t pdq_devclass;
DRIVER_MODULE(if_fpa, pci, pdq_pci_driver, pdq_devclass, 0, 0);
