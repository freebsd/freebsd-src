/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

static int ar71xx_ohci_attach(device_t dev);
static int ar71xx_ohci_detach(device_t dev);
static int ar71xx_ohci_probe(device_t dev);

struct ar71xx_ohci_softc
{
	struct ohci_softc sc_ohci;
};

static int
ar71xx_ohci_probe(device_t dev)
{
	device_set_desc(dev, "AR71XX integrated OHCI controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ar71xx_ohci_attach(device_t dev)
{
	struct ar71xx_ohci_softc *sc = device_get_softc(dev);
	int err;
	int rid;

	rid = 0;
	sc->sc_ohci.io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_ohci.io_res == NULL) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_ohci.iot = rman_get_bustag(sc->sc_ohci.io_res);
	sc->sc_ohci.ioh = rman_get_bushandle(sc->sc_ohci.io_res);

	rid = 0;
	sc->sc_ohci.irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->sc_ohci.irq_res == NULL) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_ohci.sc_bus.bdev = device_add_child(dev, "usb", -1);
	if (sc->sc_ohci.sc_bus.bdev == NULL) {
		err = ENOMEM;
		goto error;
	}
	device_set_ivars(sc->sc_ohci.sc_bus.bdev, &sc->sc_ohci.sc_bus);

	/* Allocate a parent dma tag for DMA maps */
	err = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, USB_DMA_NSEG, BUS_SPACE_MAXSIZE_32BIT, 0,
	    NULL, NULL, &sc->sc_ohci.sc_bus.parent_dmatag);
	if (err) {
		device_printf(dev, "Could not allocate parent DMA tag (%d)\n",
		    err);
		err = ENXIO;
		goto error;
	}

	/* Allocate a dma tag for transfer buffers */
	err = bus_dma_tag_create(sc->sc_ohci.sc_bus.parent_dmatag, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, USB_DMA_NSEG, BUS_SPACE_MAXSIZE_32BIT, 0,
	    busdma_lock_mutex, &Giant, &sc->sc_ohci.sc_bus.buffer_dmatag);
	if (err) {
		device_printf(dev, "Could not allocate transfer tag (%d)\n",
		    err);
		err = ENXIO;
		goto error;
	}

	err = bus_setup_intr(dev, sc->sc_ohci.irq_res, INTR_TYPE_BIO, NULL, 
	    ohci_intr, sc, &sc->sc_ohci.ih);
	if (err) {
		err = ENXIO;
		goto error;
	}
	strlcpy(sc->sc_ohci.sc_vendor, "Atheros", 
	    sizeof(sc->sc_ohci.sc_vendor));

	bus_space_write_4(sc->sc_ohci.iot, sc->sc_ohci.ioh, OHCI_CONTROL, 0);

	err = ohci_init(&sc->sc_ohci);
	if (!err) {
		sc->sc_ohci.sc_flags |= OHCI_SCFLG_DONEINIT;
		err = device_probe_and_attach(sc->sc_ohci.sc_bus.bdev);
	}

error:
	if (err) {
		ar71xx_ohci_detach(dev);
		return (err);
	}
	return (err);
}

static int
ar71xx_ohci_detach(device_t dev)
{
	struct ar71xx_ohci_softc *sc = device_get_softc(dev);

	if (sc->sc_ohci.sc_flags & OHCI_SCFLG_DONEINIT) {
		ohci_detach(&sc->sc_ohci, 0);
		sc->sc_ohci.sc_flags &= ~OHCI_SCFLG_DONEINIT;
	}

	if (sc->sc_ohci.ih) {
		bus_teardown_intr(dev, sc->sc_ohci.irq_res, sc->sc_ohci.ih);
		sc->sc_ohci.ih = NULL;
	}

	if (sc->sc_ohci.sc_bus.parent_dmatag != NULL)
		bus_dma_tag_destroy(sc->sc_ohci.sc_bus.parent_dmatag);
	if (sc->sc_ohci.sc_bus.buffer_dmatag != NULL)
		bus_dma_tag_destroy(sc->sc_ohci.sc_bus.buffer_dmatag);

	if (sc->sc_ohci.sc_bus.bdev) {
		device_delete_child(dev, sc->sc_ohci.sc_bus.bdev);
		sc->sc_ohci.sc_bus.bdev = NULL;
	}
	if (sc->sc_ohci.irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_ohci.irq_res);
		sc->sc_ohci.irq_res = NULL;
	}
	if (sc->sc_ohci.io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_ohci.io_res);
		sc->sc_ohci.io_res = NULL;
		sc->sc_ohci.iot = 0;
		sc->sc_ohci.ioh = 0;
	}
	return (0);
}

static device_method_t ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ar71xx_ohci_probe),
	DEVMETHOD(device_attach, ar71xx_ohci_attach),
	DEVMETHOD(device_detach, ar71xx_ohci_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t ohci_driver = {
	"ohci",
	ohci_methods,
	sizeof(struct ar71xx_ohci_softc),
};

static devclass_t ohci_devclass;

DRIVER_MODULE(ohci, apb, ohci_driver, ohci_devclass, 0, 0);
