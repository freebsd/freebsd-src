/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005, 2006
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
 * PCI/Cardbus front-end for the Ralink RT2560/RT2561/RT2561S/RT2661 driver.
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
#include <net80211/ieee80211_amrr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ral/rt2560var.h>
#include <dev/ral/rt2661var.h>

MODULE_DEPEND(ral, pci, 1, 1, 1);
MODULE_DEPEND(ral, firmware, 1, 1, 1);
MODULE_DEPEND(ral, wlan, 1, 1, 1);
MODULE_DEPEND(ral, wlan_amrr, 1, 1, 1);

struct ral_pci_ident {
	uint16_t	vendor;
	uint16_t	device;
	const char	*name;
};

static const struct ral_pci_ident ral_pci_ids[] = {
	{ 0x1814, 0x0201, "Ralink Technology RT2560" },
	{ 0x1814, 0x0301, "Ralink Technology RT2561S" },
	{ 0x1814, 0x0302, "Ralink Technology RT2561" },
	{ 0x1814, 0x0401, "Ralink Technology RT2661" },

	{ 0, 0, NULL }
};

static struct ral_opns {
	int	(*attach)(device_t, int);
	int	(*detach)(void *);
	void	(*shutdown)(void *);
	void	(*suspend)(void *);
	void	(*resume)(void *);
	void	(*intr)(void *);

}  ral_rt2560_opns = {
	rt2560_attach,
	rt2560_detach,
	rt2560_stop,
	rt2560_stop,
	rt2560_resume,
	rt2560_intr

}, ral_rt2661_opns = {
	rt2661_attach,
	rt2661_detach,
	rt2661_shutdown,
	rt2661_suspend,
	rt2661_resume,
	rt2661_intr
};

struct ral_pci_softc {
	union {
		struct rt2560_softc sc_rt2560;
		struct rt2661_softc sc_rt2661;
	} u;

	struct ral_opns		*sc_opns;
	int			irq_rid;
	int			mem_rid;
	struct resource		*irq;
	struct resource		*mem;
	void			*sc_ih;
};

static int ral_pci_probe(device_t);
static int ral_pci_attach(device_t);
static int ral_pci_detach(device_t);
static int ral_pci_shutdown(device_t);
static int ral_pci_suspend(device_t);
static int ral_pci_resume(device_t);

static device_method_t ral_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ral_pci_probe),
	DEVMETHOD(device_attach,	ral_pci_attach),
	DEVMETHOD(device_detach,	ral_pci_detach),
	DEVMETHOD(device_shutdown,	ral_pci_shutdown),
	DEVMETHOD(device_suspend,	ral_pci_suspend),
	DEVMETHOD(device_resume,	ral_pci_resume),

	{ 0, 0 }
};

static driver_t ral_pci_driver = {
	"ral",
	ral_pci_methods,
	sizeof (struct ral_pci_softc)
};

static devclass_t ral_devclass;

DRIVER_MODULE(ral, pci, ral_pci_driver, ral_devclass, 0, 0);

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
	struct ral_pci_softc *psc = device_get_softc(dev);
	struct rt2560_softc *sc = &psc->u.sc_rt2560;
	int error;

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	}

	/* enable bus-mastering */
	pci_enable_busmaster(dev);

	psc->sc_opns = (pci_get_device(dev) == 0x0201) ? &ral_rt2560_opns :
	    &ral_rt2661_opns;

	psc->mem_rid = RAL_PCI_BAR0;
	psc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &psc->mem_rid,
	    RF_ACTIVE);
	if (psc->mem == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		return ENXIO;
	}

	sc->sc_st = rman_get_bustag(psc->mem);
	sc->sc_sh = rman_get_bushandle(psc->mem);
	sc->sc_invalid = 1;
	
	psc->irq_rid = 0;
	psc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &psc->irq_rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (psc->irq == NULL) {
		device_printf(dev, "could not allocate interrupt resource\n");
		return ENXIO;
	}

	error = (*psc->sc_opns->attach)(dev, pci_get_device(dev));
	if (error != 0)
		return error;

	/*
	 * Hook our interrupt after all initialization is complete.
	 */
	error = bus_setup_intr(dev, psc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, psc->sc_opns->intr, psc, &psc->sc_ih);
	if (error != 0) {
		device_printf(dev, "could not set up interrupt\n");
		return error;
	}
	sc->sc_invalid = 0;
	
	return 0;
}

static int
ral_pci_detach(device_t dev)
{
	struct ral_pci_softc *psc = device_get_softc(dev);
	struct rt2560_softc *sc = &psc->u.sc_rt2560;
	
	/* check if device was removed */
	sc->sc_invalid = !bus_child_present(dev);
	
	(*psc->sc_opns->detach)(psc);

	bus_generic_detach(dev);
	bus_teardown_intr(dev, psc->irq, psc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, psc->irq_rid, psc->irq);

	bus_release_resource(dev, SYS_RES_MEMORY, psc->mem_rid, psc->mem);

	return 0;
}

static int
ral_pci_shutdown(device_t dev)
{
	struct ral_pci_softc *psc = device_get_softc(dev);

	(*psc->sc_opns->shutdown)(psc);

	return 0;
}

static int
ral_pci_suspend(device_t dev)
{
	struct ral_pci_softc *psc = device_get_softc(dev);

	(*psc->sc_opns->suspend)(psc);

	return 0;
}

static int
ral_pci_resume(device_t dev)
{
	struct ral_pci_softc *psc = device_get_softc(dev);

	(*psc->sc_opns->resume)(psc);

	return 0;
}
