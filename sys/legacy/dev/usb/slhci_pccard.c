/*-
 * Copyright (C) 2005 Takanori Watanabe. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */



#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pccard/pccard_cis.h>
#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_port.h>
#include <dev/usb/sl811hsvar.h>
#include "pccarddevs.h"

__FBSDID("$FreeBSD$");

static void	slhci_pccard_intr(void *arg);

static const struct pccard_product slhci_pccard_products[] = {
	PCMCIA_CARD(RATOC, REX_CFU1),
	{NULL}
};

static int
slhci_pccard_probe(device_t dev)
{
	const struct pccard_product *pp;
	u_int32_t	fcn = PCCARD_FUNCTION_UNSPEC;
	int		error = 0;

	if ((error = pccard_get_function(dev, &fcn)))
		return error;

	/* if it says its a disk we should register it */
	if (fcn == PCCARD_FUNCTION_DISK)
		return 0;

	/* match other devices here, primarily cdrom/dvd rom */
	if ((pp = pccard_product_lookup(dev, slhci_pccard_products,
				 sizeof(slhci_pccard_products[0]), NULL))) {
		if (pp->pp_name)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return ENXIO;
}

static int
slhci_pccard_detach(device_t dev)
{
	struct slhci_softc *sc = device_get_softc(dev);
	bus_generic_detach(dev);

	if (sc->ih)
		bus_teardown_intr(dev, sc->irq_res, sc->ih);
	if (sc->io_res)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->io_res);
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->sc_bus.bdev)
		device_delete_child(dev, sc->sc_bus.bdev);
	return 0;
}

static int
slhci_pccard_attach(device_t dev)
{
	struct slhci_softc *sc = device_get_softc(dev);
	int		error = ENXIO;
	int		rid = 0;
	if ((sc->io_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE)) == NULL) {
		return ENOMEM;
	}
	sc->sc_iot = rman_get_bustag(sc->io_res);
	sc->sc_ioh = rman_get_bushandle(sc->io_res);

	if (sl811hs_find(sc) == -1) {
		error = ENXIO;
		goto out;
	}

	rid = 0;
	if ((sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		printf("CANNOT ALLOC IRQ\n");
		error = ENOMEM;
		goto out;
	}
	sc->sc_iot = rman_get_bustag(sc->io_res);
	sc->sc_ioh = rman_get_bushandle(sc->io_res);
	sc->sc_bus.bdev = device_add_child(dev, "usb", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(dev, "Could not add USB device\n");
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_BIO, NULL, slhci_pccard_intr, sc, &sc->ih);
	if (error)
		goto out;
#if 1

	if (slhci_attach(sc) == -1) {
		printf("MI attach NO\n");
		goto out;
	}

	error = device_probe_and_attach(sc->sc_bus.bdev);

	if (error) {
		printf("Probing USB bus %x\n", error);
		goto out;
	}
#endif
	printf("ATTACHED\n");
	return 0;
out:
	slhci_pccard_detach(dev);
	return error;
}

#if 0
static void
slhci_pccard_enable_power(void *arg, int mode)
{
#if 0
	struct slhci_softc *sc = arg;
	u_int8_t	r;
#endif
}

static void
slhci_pccard_enable_intr(void *arg, int mode)
{
#if 0
	struct slhci_softc *sc = arg;
	u_int8_t	r;
#endif
}

#endif
static void
slhci_pccard_intr(void *arg)
{
#if 1
	struct slhci_softc *sc = arg;
	sc = sc;
	slhci_intr(sc);
#endif
}

static device_method_t slhci_pccard_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe, slhci_pccard_probe),
	DEVMETHOD(device_attach, slhci_pccard_attach),
	DEVMETHOD(device_detach, slhci_pccard_detach),

	{0, 0}
};

static driver_t	slhci_pccard_driver = {
	"slhci",
	slhci_pccard_methods,
	sizeof(struct slhci_softc),
};

devclass_t	slhci_devclass;

DRIVER_MODULE(slhci, pccard, slhci_pccard_driver, slhci_devclass, 0, 0);
MODULE_DEPEND(slhci, usb, 1, 1, 1);
