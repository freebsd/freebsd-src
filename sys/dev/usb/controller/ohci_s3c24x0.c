/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 * Copyright (c) 2009 Andrew Turner.  All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include <sys/rman.h>

#include <arm/s3c2xx0/s3c24x0reg.h>

static device_probe_t ohci_s3c24x0_probe;
static device_attach_t ohci_s3c24x0_attach;
static device_detach_t ohci_s3c24x0_detach;

static int
ohci_s3c24x0_probe(device_t dev)
{
	device_set_desc(dev, "S3C24x0 integrated OHCI controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ohci_s3c24x0_attach(device_t dev)
{
	struct ohci_softc *sc = device_get_softc(dev);
	int err;
	int rid;

	/* initialise some bus fields */
	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = OHCI_MAX_DEVICES;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_bus, USB_GET_DMA_TAG(dev),
	    &ohci_iterate_hw_softc)) {
		return (ENOMEM);
	}

	sc->sc_dev = dev;

	rid = 0;
	sc->sc_io_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
	    &rid, RF_ACTIVE);

	if (!(sc->sc_io_res)) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!(sc->sc_irq_res)) {
		goto error;
	}
	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_bus.bdev)) {
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	strlcpy(sc->sc_vendor, "Samsung", sizeof(sc->sc_vendor));

	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (void *)ohci_interrupt, sc, &sc->sc_intr_hdl);
	if (err) {
		sc->sc_intr_hdl = NULL;
		goto error;
	}

	bus_space_write_4(sc->sc_io_tag, sc->sc_io_hdl,
	    OHCI_CONTROL, 0);

	err = ohci_init(sc);
	if (!err) {
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}
	if (err) {
		goto error;
	}
	return (0);

error:
	ohci_s3c24x0_detach(dev);
	return (ENXIO);
}

static int
ohci_s3c24x0_detach(device_t dev)
{
	struct ohci_softc *sc = device_get_softc(dev);
	device_t bdev;
	int err;

	if (sc->sc_bus.bdev) {
		bdev = sc->sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_all_children(dev);

	/*
	 * Put the controller into reset, then disable clocks and do
	 * the MI tear down.  We have to disable the clocks/hardware
	 * after we do the rest of the teardown.  We also disable the
	 * clocks in the opposite order we acquire them, but that
	 * doesn't seem to be absolutely necessary.  We free up the
	 * clocks after we disable them, so the system could, in
	 * theory, reuse them.
	 */
	bus_space_write_4(sc->sc_io_tag, sc->sc_io_hdl,
	    OHCI_CONTROL, 0);

	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		/*
		 * only call ohci_detach() after ohci_init()
		 */
		ohci_detach(sc);

		err = bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intr_hdl);
		sc->sc_intr_hdl = NULL;
	}
	if (sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	if (sc->sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_bus, &ohci_iterate_hw_softc);

	return (0);
}

static device_method_t ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ohci_s3c24x0_probe),
	DEVMETHOD(device_attach, ohci_s3c24x0_attach),
	DEVMETHOD(device_detach, ohci_s3c24x0_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t ohci_driver = {
	"ohci",
	ohci_methods,
	sizeof(struct ohci_softc),
};

static devclass_t ohci_devclass;

DRIVER_MODULE(ohci, s3c24x0, ohci_driver, ohci_devclass, 0, 0);
MODULE_DEPEND(ohci, usb, 1, 1, 1);
