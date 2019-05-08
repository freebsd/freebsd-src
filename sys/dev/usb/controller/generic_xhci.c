/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2015 Semihalf.
 * Copyright (c) 2015 Stormshield.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/priv.h>
#include <sys/rman.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/xhci.h>
#include <dev/usb/controller/xhcireg.h>

#include "generic_xhci.h"

#define	IS_DMA_32B	1

int
generic_xhci_attach(device_t dev)
{
	struct xhci_softc *sc = device_get_softc(dev);
	int err = 0, rid = 0;

	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = XHCI_MAX_DEVICES;

	sc->sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_io_res == NULL) {
		device_printf(dev, "Failed to map memory\n");
		generic_xhci_detach(dev);
		return (ENXIO);
	}

	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "Failed to allocate IRQ\n");
		generic_xhci_detach(dev);
		return (ENXIO);
	}

	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (sc->sc_bus.bdev == NULL) {
		device_printf(dev, "Failed to add USB device\n");
		generic_xhci_detach(dev);
		return (ENXIO);
	}

	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	sprintf(sc->sc_vendor, XHCI_HC_VENDOR);
	device_set_desc(sc->sc_bus.bdev, XHCI_HC_DEVSTR);

	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)xhci_interrupt, sc, &sc->sc_intr_hdl);
	if (err != 0) {
		device_printf(dev, "Failed to setup error IRQ, %d\n", err);
		sc->sc_intr_hdl = NULL;
		generic_xhci_detach(dev);
		return (err);
	}

	err = xhci_init(sc, dev, IS_DMA_32B);
	if (err != 0) {
		device_printf(dev, "Failed to init XHCI, with error %d\n", err);
		generic_xhci_detach(dev);
		return (ENXIO);
	}

	err = xhci_start_controller(sc);
	if (err != 0) {
		device_printf(dev, "Failed to start XHCI controller, with error %d\n", err);
		generic_xhci_detach(dev);
		return (ENXIO);
	}

	err = device_probe_and_attach(sc->sc_bus.bdev);
	if (err != 0) {
		device_printf(dev, "Failed to initialize USB, with error %d\n", err);
		generic_xhci_detach(dev);
		return (ENXIO);
	}

	return (0);
}

int
generic_xhci_detach(device_t dev)
{
	struct xhci_softc *sc = device_get_softc(dev);
	int err;

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);

	if (sc->sc_irq_res != NULL && sc->sc_intr_hdl != NULL) {
		err = bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intr_hdl);
		if (err != 0)
			device_printf(dev, "Could not tear down irq, %d\n",
			    err);
		sc->sc_intr_hdl = NULL;
	}

	if (sc->sc_irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->sc_irq_res), sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}

	if (sc->sc_io_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->sc_io_res), sc->sc_io_res);
		sc->sc_io_res = NULL;
	}

	xhci_uninit(sc);

	return (0);
}

static device_method_t xhci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach, generic_xhci_attach),
	DEVMETHOD(device_detach, generic_xhci_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

driver_t generic_xhci_driver = {
	"xhci",
	xhci_methods,
	sizeof(struct xhci_softc),
};
