/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005
 *	Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * PCI front-end for the Ralink RT2500 driver.
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ral/if_ralrate.h>
#include <dev/ral/if_ralreg.h>
#include <dev/ral/if_ralvar.h>

MODULE_DEPEND(ral, pci, 1, 1, 1);
MODULE_DEPEND(ral, wlan, 1, 1, 1);

struct ral_pci_ident {
	uint16_t	vendor;
	uint16_t	device;
	const char	*name;
};

static const struct ral_pci_ident ral_pci_ids[] = {
	{ 0x1814, 0x0201, "Ralink Technology RT2500" },

	{ 0, 0, NULL }
};

static int ral_pci_probe(device_t);
static int ral_pci_attach(device_t);
static int ral_pci_suspend(device_t);
static int ral_pci_resume(device_t);

static device_method_t ral_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ral_pci_probe),
	DEVMETHOD(device_attach,	ral_pci_attach),
	DEVMETHOD(device_detach,	ral_detach),
	DEVMETHOD(device_suspend,	ral_pci_suspend),
	DEVMETHOD(device_resume,	ral_pci_resume),

	{ 0, 0 }
};

static driver_t ral_pci_driver = {
	"ral",
	ral_pci_methods,
	sizeof (struct ral_softc)
};

DRIVER_MODULE(ral, pci, ral_pci_driver, ral_devclass, 0, 0);
DRIVER_MODULE(ral, cardbus, ral_pci_driver, ral_devclass, 0, 0);

static int
ral_pci_probe(device_t dev)
{
	const struct ral_pci_ident *ident;

	for (ident = ral_pci_ids; ident->name != NULL; ident++) {
		if (pci_get_vendor(dev) == ident->vendor &&
		    pci_get_device(dev) == ident->device) {
			device_set_desc(dev, ident->name);
			return 0;
		}
	}
	return ENXIO;
}

/* Base Address Register */
#define RAL_PCI_BAR0	0x10

static int
ral_pci_attach(device_t dev)
{
	int error;

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	}

	/* enable bus-mastering */
	pci_enable_busmaster(dev);

	error = ral_alloc(dev, RAL_PCI_BAR0);
	if (error != 0)
		return error;

	error = ral_attach(dev);
	if (error != 0)
		ral_free(dev);

	return error;
}

static int
ral_pci_suspend(device_t dev)
{
	struct ral_softc *sc = device_get_softc(dev);

	ral_stop(sc);

	return 0;
}

static int
ral_pci_resume(device_t dev)
{
	struct ral_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ic.ic_ifp;

	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp->if_softc);
		if (ifp->if_flags & IFF_RUNNING)
			ifp->if_start(ifp);
	}

	return 0;
}
