/*
 * Copyright (c) 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: if_hme_pci.c,v 1.4 2001/08/27 22:18:49 augustss Exp
 *
 * $FreeBSD$
 */

/*
 * PCI front-end device driver for the HME ethernet device.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <machine/ofw_machdep.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <mii/mii.h>
#include <mii/miivar.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <hme/if_hmereg.h>
#include <hme/if_hmevar.h>

#include "miibus_if.h"

struct hme_pci_softc {
	struct	hme_softc	hsc_hme;	/* HME device */
	struct	resource	*hsc_sres;
	int			hsc_srid;
	struct	resource	*hsc_ires;
	int			hsc_irid;
	bus_space_tag_t		hsc_memt;
	bus_space_handle_t	hsc_memh;
	void			*hsc_ih;
};

static int hme_pci_probe(device_t);
static int hme_pci_attach(device_t);

static device_method_t hme_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		hme_pci_probe),
	DEVMETHOD(device_attach,	hme_pci_attach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	hme_mii_readreg),
	DEVMETHOD(miibus_writereg,	hme_mii_writereg),
	DEVMETHOD(miibus_statchg,	hme_mii_statchg),

	{ 0, 0 }
};

static driver_t hme_pci_driver = {
	"hme",
	hme_pci_methods,
	sizeof(struct hme_pci_softc)
};

DRIVER_MODULE(if_hme, pci, hme_pci_driver, hme_devclass, 0, 0);

int
hme_pci_probe(device_t dev)
{

	if (pci_get_vendor(dev) == 0x108e &&
	    pci_get_device(dev) ==  0x1001) {
		device_set_desc(dev, "Sun HME 10/100 Ethernet");
		return (0);
	}
	return (ENXIO);
}

int
hme_pci_attach(device_t dev)
{
	struct hme_pci_softc *hsc = device_get_softc(dev);
	struct hme_softc *sc = &hsc->hsc_hme;
	int error;

	/*
	 * Enable memory-space and bus master accesses.  This is kinda of
	 * gross; but the hme comes up with neither enabled.
	 */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, SYS_RES_MEMORY);

	sc->sc_pci = 1; /* XXXXX should all be done in bus_dma. */
	sc->sc_dev = dev;

	/*
	 * Map five register banks:
	 *
	 *	bank 0: HME SEB registers:	+0x0000
	 *	bank 1: HME ETX registers:	+0x2000
	 *	bank 2: HME ERX registers:	+0x4000
	 *	bank 3: HME MAC registers:	+0x6000
	 *	bank 4: HME MIF registers:	+0x7000
	 *
	 */
	hsc->hsc_srid = PCI_HME_BASEADDR;
	hsc->hsc_sres = bus_alloc_resource(dev, SYS_RES_MEMORY, &hsc->hsc_srid,
	    0, ~0, 1, RF_ACTIVE);
	if (hsc->hsc_sres == NULL) {
		device_printf(dev, "could not map device registers\n");
		return (ENXIO);
	}
	hsc->hsc_irid = 0;
	hsc->hsc_ires = bus_alloc_resource(dev, SYS_RES_IRQ, &hsc->hsc_irid, 0,
	    ~0, 1, RF_ACTIVE);
	if (hsc->hsc_ires == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		error = ENXIO;
		goto fail_sres;
	}
	sc->sc_sebt = sc->sc_etxt = sc->sc_erxt = sc->sc_mact = sc->sc_mift =
	    rman_get_bustag(hsc->hsc_sres);
	sc->sc_sebh = sc->sc_etxh = sc->sc_erxh = sc->sc_mach = sc->sc_mifh =
	    rman_get_bushandle(hsc->hsc_sres);
	sc->sc_sebo = 0;
	sc->sc_etxo = 0x2000;
	sc->sc_erxo = 0x4000;
	sc->sc_maco = 0x6000;
	sc->sc_mifo = 0x7000;

	OF_getetheraddr(dev, sc->sc_arpcom.ac_enaddr);

	sc->sc_burst = 64;	/* XXX */

	/*
	 * call the main configure
	 */
	if ((error = hme_config(sc)) != 0) {
		device_printf(dev, "could not be configured\n");
		goto fail_ires;
	}

	if ((error = bus_setup_intr(dev, hsc->hsc_ires, INTR_TYPE_NET, hme_intr,
	     sc, &hsc->hsc_ih)) != 0) {
		device_printf(dev, "couldn't establish interrupt\n");
		goto fail_ires;
	}
	return (0);

fail_ires:
	bus_release_resource(dev, SYS_RES_IRQ, hsc->hsc_irid, hsc->hsc_ires);
fail_sres:
	bus_release_resource(dev, SYS_RES_MEMORY, hsc->hsc_srid, hsc->hsc_sres);
	return (ENXIO);
}
