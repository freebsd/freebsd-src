#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2008 Hans Petter Selasky <hselasky@FreeBSD.org>
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
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/uss820dci.h>

#include <sys/rman.h>

static device_probe_t uss820_atmelarm_probe;
static device_attach_t uss820_atmelarm_attach;
static device_detach_t uss820_atmelarm_detach;

static device_method_t uss820dci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uss820_atmelarm_probe),
	DEVMETHOD(device_attach, uss820_atmelarm_attach),
	DEVMETHOD(device_detach, uss820_atmelarm_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

static driver_t uss820dci_driver = {
	.name = "uss820dci",
	.methods = uss820dci_methods,
	.size = sizeof(struct uss820dci_softc),
};

static devclass_t uss820dci_devclass;

DRIVER_MODULE(uss820dci, atmelarm, uss820dci_driver, uss820dci_devclass, 0, 0);
MODULE_DEPEND(uss820dci, usb, 1, 1, 1);

static const char *const uss820_desc = "USS820 USB Device Controller";

static int
uss820_atmelarm_probe(device_t dev)
{
	device_set_desc(dev, uss820_desc);
	return (0);			/* success */
}

static int
uss820_atmelarm_attach(device_t dev)
{
	struct uss820dci_softc *sc = device_get_softc(dev);
	int err;
	int rid;

	/* initialise some bus fields */
	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = USS820_MAX_DEVICES;
	sc->sc_bus.dma_bits = 32;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_bus,
	    USB_GET_DMA_TAG(dev), NULL)) {
		return (ENOMEM);
	}
	rid = 0;
	sc->sc_io_res =
	    bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);

	if (!sc->sc_io_res) {
		goto error;
	}
	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		goto error;
	}
	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_bus.bdev)) {
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)uss820dci_interrupt, sc, &sc->sc_intr_hdl);
#else
	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    (driver_intr_t *)uss820dci_interrupt, sc, &sc->sc_intr_hdl);
#endif
	if (err) {
		sc->sc_intr_hdl = NULL;
		goto error;
	}
	err = uss820dci_init(sc);
	if (err) {
		device_printf(dev, "Init failed\n");
		goto error;
	}
	err = device_probe_and_attach(sc->sc_bus.bdev);
	if (err) {
		device_printf(dev, "USB probe and attach failed\n");
		goto error;
	}
	return (0);

error:
	uss820_atmelarm_detach(dev);
	return (ENXIO);
}

static int
uss820_atmelarm_detach(device_t dev)
{
	struct uss820dci_softc *sc = device_get_softc(dev);
	int err;

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);

	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		/*
		 * only call at91_udp_uninit() after at91_udp_init()
		 */
		uss820dci_uninit(sc);

		err = bus_teardown_intr(dev, sc->sc_irq_res,
		    sc->sc_intr_hdl);
		sc->sc_intr_hdl = NULL;
	}
	if (sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0,
		    sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	if (sc->sc_io_res) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_bus, NULL);

	return (0);
}
