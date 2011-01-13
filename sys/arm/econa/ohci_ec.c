/*-
 * Copyright (c) 2009 Yohanes Nugroho <yohanes@gmail.com>
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

#include <arm/econa/econa_reg.h>

#define	MEM_RID	0

static device_probe_t ohci_ec_probe;
static device_attach_t ohci_ec_attach;
static device_detach_t ohci_ec_detach;

struct ec_ohci_softc {
	struct ohci_softc sc_ohci;	/* must be first */
};

static int
ohci_ec_probe(device_t dev)
{
	device_set_desc(dev, "Econa integrated OHCI controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ohci_ec_attach(device_t dev)
{
	struct ec_ohci_softc *sc = device_get_softc(dev);
	bus_space_handle_t bsh;
	int err;
	int rid;

	/* initialise some bus fields */
	sc->sc_ohci.sc_bus.parent = dev;
	sc->sc_ohci.sc_bus.devices = sc->sc_ohci.sc_devices;
	sc->sc_ohci.sc_bus.devices_max = OHCI_MAX_DEVICES;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_ohci.sc_bus,
	    USB_GET_DMA_TAG(dev), &ohci_iterate_hw_softc)) {
		return (ENOMEM);
	}
	sc->sc_ohci.sc_dev = dev;

	rid = MEM_RID;

	sc->sc_ohci.sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(sc->sc_ohci.sc_io_res)) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_ohci.sc_io_tag = rman_get_bustag(sc->sc_ohci.sc_io_res);
	bsh = rman_get_bushandle(sc->sc_ohci.sc_io_res);
	/* Undocumented magic initialization */
	bus_space_write_4((sc)->sc_ohci.sc_io_tag, bsh,0x04, 0x146);

	bus_space_write_4((sc)->sc_ohci.sc_io_tag, bsh,0x44, 0x0200);

	DELAY(1000);

	sc->sc_ohci.sc_io_size = rman_get_size(sc->sc_ohci.sc_io_res);

	if (bus_space_subregion(sc->sc_ohci.sc_io_tag, bsh, 0x4000000,
	    sc->sc_ohci.sc_io_size, &sc->sc_ohci.sc_io_hdl) != 0)
		panic("%s: unable to subregion USB host registers",
		    device_get_name(dev));

	rid = 0;
	sc->sc_ohci.sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!(sc->sc_ohci.sc_irq_res)) {
		goto error;
	}
	sc->sc_ohci.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_ohci.sc_bus.bdev)) {
		goto error;
	}
	device_set_ivars(sc->sc_ohci.sc_bus.bdev, &sc->sc_ohci.sc_bus);

	strlcpy(sc->sc_ohci.sc_vendor, "Cavium",
		sizeof(sc->sc_ohci.sc_vendor));

#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(dev, sc->sc_ohci.sc_irq_res,
	    INTR_TYPE_BIO | INTR_MPSAFE,  NULL,
	    (driver_intr_t *)ohci_interrupt, sc,
	    &sc->sc_ohci.sc_intr_hdl);
#else
	err = bus_setup_intr(dev, sc->sc_ohci.sc_irq_res,
	    INTR_TYPE_BIO | INTR_MPSAFE,
	    (driver_intr_t *)ohci_interrupt, sc,
	    &sc->sc_ohci.sc_intr_hdl);
#endif
	if (err) {
		sc->sc_ohci.sc_intr_hdl = NULL;
		goto error;
	}

	bus_space_write_4(sc->sc_ohci.sc_io_tag, sc->sc_ohci.sc_io_hdl,
	    OHCI_CONTROL, 0);

	err = ohci_init(&sc->sc_ohci);
	if (!err) {
		err = device_probe_and_attach(sc->sc_ohci.sc_bus.bdev);
	}
	if (err) {
		goto error;
	}
	return (0);

error:
	ohci_ec_detach(dev);
	return (ENXIO);
}

static int
ohci_ec_detach(device_t dev)
{
	struct ec_ohci_softc *sc = device_get_softc(dev);
	device_t bdev;
	int err;

	if (sc->sc_ohci.sc_bus.bdev) {
		bdev = sc->sc_ohci.sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_all_children(dev);

	bus_space_write_4(sc->sc_ohci.sc_io_tag, sc->sc_ohci.sc_io_hdl,
	    OHCI_CONTROL, 0);

	if (sc->sc_ohci.sc_irq_res && sc->sc_ohci.sc_intr_hdl) {
		/*
		 * only call ohci_detach() after ohci_init()
		 */
		ohci_detach(&sc->sc_ohci);

		err = bus_teardown_intr(dev, sc->sc_ohci.sc_irq_res,
		    sc->sc_ohci.sc_intr_hdl);
		sc->sc_ohci.sc_intr_hdl = NULL;
	}
	if (sc->sc_ohci.sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0,
		    sc->sc_ohci.sc_irq_res);
		sc->sc_ohci.sc_irq_res = NULL;
	}
	if (sc->sc_ohci.sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, MEM_RID,
		    sc->sc_ohci.sc_io_res);
		sc->sc_ohci.sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_ohci.sc_bus, &ohci_iterate_hw_softc);

	return (0);
}

static device_method_t ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ohci_ec_probe),
	DEVMETHOD(device_attach, ohci_ec_attach),
	DEVMETHOD(device_detach, ohci_ec_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t ohci_driver = {
	"ohci",
	ohci_methods,
	sizeof(struct ec_ohci_softc),
};

static devclass_t ohci_devclass;

DRIVER_MODULE(ohci, econaarm, ohci_driver, ohci_devclass, 0, 0);
MODULE_DEPEND(ohci, usb, 1, 1, 1);
