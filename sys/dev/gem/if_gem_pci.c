/*
 * Copyright (C) 2001 Eduardo Horvath.
 * All rights reserved.
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: if_gem_pci.c,v 1.7 2001/10/18 15:09:15 thorpej Exp
 *
 * $FreeBSD$
 */

/*
 * PCI bindings for Sun GEM ethernet controllers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <machine/endian.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/ofw_machdep.h>

#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/gem/if_gemreg.h>
#include <dev/gem/if_gemvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "miibus_if.h"

struct gem_pci_softc {
	struct	gem_softc	gsc_gem;	/* GEM device */
	struct	resource	*gsc_sres;
	int			gsc_srid;
	struct	resource	*gsc_ires;
	int			gsc_irid;
	void			*gsc_ih;
};

static int	gem_pci_probe(device_t);
static int	gem_pci_attach(device_t);
static int	gem_pci_detach(device_t);
static int	gem_pci_suspend(device_t);
static int	gem_pci_resume(device_t);

static device_method_t gem_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gem_pci_probe),
	DEVMETHOD(device_attach,	gem_pci_attach),
	DEVMETHOD(device_detach,	gem_pci_detach),
	DEVMETHOD(device_suspend,	gem_pci_suspend),
	DEVMETHOD(device_resume,	gem_pci_resume),
	/* Use the suspend handler here, it is all that is required. */
	DEVMETHOD(device_shutdown,	gem_pci_suspend),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	gem_mii_readreg),
	DEVMETHOD(miibus_writereg,	gem_mii_writereg),
	DEVMETHOD(miibus_statchg,	gem_mii_statchg),

	{ 0, 0 }
};

static driver_t gem_pci_driver = {
	"gem",
	gem_pci_methods,
	sizeof(struct gem_pci_softc)
};


DRIVER_MODULE(gem, pci, gem_pci_driver, gem_devclass, 0, 0);
MODULE_DEPEND(gem, pci, 1, 1, 1);
MODULE_DEPEND(gem, ether, 1, 1, 1);

struct gem_pci_dev {
	u_int32_t	gpd_devid;
	int	gpd_variant;
	char	*gpd_desc;
} gem_pci_devlist[] = {
	{ 0x1101108e, GEM_SUN_GEM,	"Sun ERI 10/100 Ethernet Adaptor" },
	{ 0x2bad108e, GEM_SUN_GEM,	"Sun GEM Gigabit Ethernet Adaptor" },
	{ 0x0021106b, GEM_APPLE_GMAC,	"Apple GMAC Ethernet Adaptor" },
	{ 0x0024106b, GEM_APPLE_GMAC,	"Apple GMAC2 Ethernet Adaptor" },
	{ 0, NULL }
};

/*
 * Attach routines need to be split out to different bus-specific files.
 */
static int
gem_pci_probe(dev)
	device_t dev;
{
	int i;
	u_int32_t devid;
	struct gem_pci_softc *gsc;

	devid = pci_get_devid(dev);
	for (i = 0; gem_pci_devlist[i].gpd_desc != NULL; i++) {
		if (devid == gem_pci_devlist[i].gpd_devid) {
			device_set_desc(dev, gem_pci_devlist[i].gpd_desc);
			gsc = device_get_softc(dev);
			gsc->gsc_gem.sc_variant =
			    gem_pci_devlist[i].gpd_variant;
			return (0);
		}
	}

	return (ENXIO);
}

static int
gem_pci_attach(dev)
	device_t dev;
{
	struct gem_pci_softc *gsc = device_get_softc(dev);
	struct gem_softc *sc = &gsc->gsc_gem;

	pci_enable_busmaster(dev);

	/*
	 * Some Sun GEMs/ERIs do have their intpin register bogusly set to 0,
	 * although it should be 1. correct that.
	 */
	if (pci_get_intpin(dev) == 0)
		pci_set_intpin(dev, 1);

	sc->sc_dev = dev;
	sc->sc_pci = 1;		/* XXX */

	gsc->gsc_srid = PCI_GEM_BASEADDR;
	gsc->gsc_sres = bus_alloc_resource(dev, SYS_RES_MEMORY, &gsc->gsc_srid,
	    0, ~0, 1, RF_ACTIVE);
	if (gsc->gsc_sres == NULL) {
		device_printf(dev, "failed to allocate bus space resource\n");
		return (ENXIO);
	}

	gsc->gsc_irid = 0;
	gsc->gsc_ires = bus_alloc_resource(dev, SYS_RES_IRQ, &gsc->gsc_irid, 0,
	    ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (gsc->gsc_ires == NULL) {
		device_printf(dev, "failed to allocate interrupt resource\n");
		goto fail_sres;
	}

	sc->sc_bustag = rman_get_bustag(gsc->gsc_sres);
	sc->sc_h = rman_get_bushandle(gsc->gsc_sres);

	/* All platform that this driver is used on must provide this. */
	OF_getetheraddr(dev, sc->sc_arpcom.ac_enaddr);

	/*
	 * call the main configure
	 */
	if (gem_attach(sc) != 0) {
		device_printf(dev, "could not be configured\n");
		goto fail_ires;
	}

	if (bus_setup_intr(dev, gsc->gsc_ires, INTR_TYPE_NET, gem_intr, sc,
	    &gsc->gsc_ih) != 0) {
		device_printf(dev, "failed to set up interrupt\n");
		gem_detach(sc);
		goto fail_ires;
	}
	return (0);

fail_ires:
	bus_release_resource(dev, SYS_RES_IRQ, gsc->gsc_irid, gsc->gsc_ires);
fail_sres:
	bus_release_resource(dev, SYS_RES_MEMORY, gsc->gsc_srid, gsc->gsc_sres);
	return (ENXIO);
}

static int
gem_pci_detach(dev)
	device_t dev;
{
	struct gem_pci_softc *gsc = device_get_softc(dev);
	struct gem_softc *sc = &gsc->gsc_gem;

	gem_detach(sc);

	bus_teardown_intr(dev, gsc->gsc_ires, gsc->gsc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, gsc->gsc_irid, gsc->gsc_ires);
	bus_release_resource(dev, SYS_RES_MEMORY, gsc->gsc_srid, gsc->gsc_sres);
	return (0);
}

static int
gem_pci_suspend(dev)
	device_t dev;
{
	struct gem_pci_softc *gsc = device_get_softc(dev);
	struct gem_softc *sc = &gsc->gsc_gem;

	gem_suspend(sc);
	return (0);
}

static int
gem_pci_resume(dev)
	device_t dev;
{
	struct gem_pci_softc *gsc = device_get_softc(dev);
	struct gem_softc *sc = &gsc->gsc_gem;

	gem_resume(sc);
	return (0);
}
