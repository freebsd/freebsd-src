/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2010 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
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
#include <dev/usb/controller/ohci.h>
#include <dev/usb/controller/ohcireg.h>

#include "ps3bus.h"

static int
ohci_ps3_probe(device_t dev)
{
	if (ps3bus_get_bustype(dev) != PS3_BUSTYPE_SYSBUS ||
	    ps3bus_get_devtype(dev) != PS3_DEVTYPE_USB)
		return (ENXIO);

	device_set_desc(dev, "Playstation 3 USB 2.0 controller");
	return (BUS_PROBE_SPECIFIC);
}

static int
ohci_ps3_attach(device_t dev)
{
	ohci_softc_t *sc = device_get_softc(dev);
	int rid, err;

	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = OHCI_MAX_DEVICES;
	sc->sc_bus.dma_bits = 32;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_bus,
	    USB_GET_DMA_TAG(dev), &ohci_iterate_hw_softc))
		return (ENOMEM);

	rid = 0;
	sc->sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!sc->sc_io_res) {
		device_printf(dev, "Could not map memory\n");
		goto error;
	}

	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "Could not allocate irq\n");
		return (ENXIO);
	}

	sc->sc_bus.bdev = device_add_child(dev, "usbus", DEVICE_UNIT_ANY);
	if (!sc->sc_bus.bdev) {
		device_printf(dev, "Could not add USB device\n");
		return (ENXIO);
	}

	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	sprintf(sc->sc_vendor, "Sony");

	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)ohci_interrupt, sc, &sc->sc_intr_hdl);
	if (err) {
		device_printf(dev, "Could not setup error irq, %d\n", err);
		goto error;
	}

	//sc->sc_flags |= EHCI_SCFLG_BIGEMMIO;
	bus_space_write_4(sc->sc_io_tag, sc->sc_io_hdl,
	    OHCI_CONTROL, 0);
	err = ohci_init(sc);
	if (err) {
		device_printf(dev, "USB init failed err=%d\n", err);
		goto error;
	}

	err = device_probe_and_attach(sc->sc_bus.bdev);
	if (err == 0)
		return (0);

error:
	return (ENXIO);
}

static device_method_t ohci_ps3_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ohci_ps3_probe),
	DEVMETHOD(device_attach, ohci_ps3_attach),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

static driver_t ohci_ps3_driver = {
	.name = "ohci",
	.methods = ohci_ps3_methods,
	.size = sizeof(ohci_softc_t),
};

DRIVER_MODULE(ohci_ps3, ps3bus, ohci_ps3_driver, 0, 0);
MODULE_DEPEND(ohci_ps3, usb, 1, 1, 1);
