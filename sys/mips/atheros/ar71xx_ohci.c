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

#include <dev/usb/usb_mfunc.h>
#include <dev/usb/usb.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/ohci.h>

#include <sys/rman.h>

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
	sc->sc_ohci.sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_ohci.sc_io_res == NULL) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_ohci.sc_io_tag = rman_get_bustag(sc->sc_ohci.sc_io_res);
	sc->sc_ohci.sc_io_hdl = rman_get_bushandle(sc->sc_ohci.sc_io_res);
	sc->sc_ohci.sc_io_size = rman_get_size(sc->sc_ohci.sc_io_res);

	rid = 0;
	sc->sc_ohci.sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->sc_ohci.sc_irq_res == NULL) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_ohci.sc_bus.bdev = device_add_child(dev, "usb", -1);
	if (sc->sc_ohci.sc_bus.bdev == NULL) {
		err = ENOMEM;
		goto error;
	}
	device_set_ivars(sc->sc_ohci.sc_bus.bdev, &sc->sc_ohci.sc_bus);

	err = bus_setup_intr(dev, sc->sc_ohci.sc_irq_res, 
	    INTR_TYPE_BIO | INTR_MPSAFE, NULL, 
	    (driver_intr_t *)ohci_interrupt, sc, &sc->sc_ohci.sc_intr_hdl);
	if (err) {
		err = ENXIO;
		goto error;
	}

	strlcpy(sc->sc_ohci.sc_vendor, "Atheros", sizeof(sc->sc_ohci.sc_vendor));

	bus_space_write_4(sc->sc_ohci.sc_io_tag, sc->sc_ohci.sc_io_hdl, OHCI_CONTROL, 0);

	err = ohci_init(&sc->sc_ohci);
	if (!err) {
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

	if (sc->sc_ohci.sc_intr_hdl) {
		bus_teardown_intr(dev, sc->sc_ohci.sc_irq_res, sc->sc_ohci.sc_intr_hdl);
		sc->sc_ohci.sc_intr_hdl = NULL;
	}
	if (sc->sc_ohci.sc_bus.bdev) {
		device_delete_child(dev, sc->sc_ohci.sc_bus.bdev);
		sc->sc_ohci.sc_bus.bdev = NULL;
	}
	if (sc->sc_ohci.sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_ohci.sc_irq_res);
		sc->sc_ohci.sc_irq_res = NULL;
	}
	if (sc->sc_ohci.sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_ohci.sc_io_res);
		sc->sc_ohci.sc_io_res = NULL;
		sc->sc_ohci.sc_io_tag = 0;
		sc->sc_ohci.sc_io_hdl = 0;
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
