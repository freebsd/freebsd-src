/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Hans Petter Selasky.
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

#include <dev/usb/controller/dwc_otg.h>
#include <dev/usb/controller/dwc_otg_fdt.h>

static device_probe_t dwc_otg_probe;

static struct ofw_compat_data compat_data[] = {
	{ "synopsys,designware-hs-otg2",	1 },
	{ "snps,dwc2",				1 },
	{ NULL,					0 }
};

static int
dwc_otg_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "DWC OTG 2.0 integrated USB controller");

	return (BUS_PROBE_DEFAULT);
}

static int
dwc_otg_irq_index(device_t dev, int *rid)
{
	int idx, rv;
	phandle_t node;

	node = ofw_bus_get_node(dev);
	rv = ofw_bus_find_string_index(node, "interrupt-names", "usb", &idx);
	if (rv != 0)
		return (rv);
	*rid = idx;
	return (0);
}

int
dwc_otg_attach(device_t dev)
{
	struct dwc_otg_fdt_softc *sc = device_get_softc(dev);
	char usb_mode[24];
	int err;
	int rid;

	sc->sc_otg.sc_bus.parent = dev;

	/* get USB mode, if any */
	if (OF_getprop(ofw_bus_get_node(dev), "dr_mode",
	    &usb_mode, sizeof(usb_mode)) > 0) {
		/* ensure proper zero termination */
		usb_mode[sizeof(usb_mode) - 1] = 0;

		if (strcasecmp(usb_mode, "host") == 0)
			sc->sc_otg.sc_mode = DWC_MODE_HOST;
		else if (strcasecmp(usb_mode, "peripheral") == 0)
			sc->sc_otg.sc_mode = DWC_MODE_DEVICE;
		else if (strcasecmp(usb_mode, "otg") != 0) {
			device_printf(dev, "Invalid FDT dr_mode: %s\n",
			    usb_mode);
		}
	}

	rid = 0;
	sc->sc_otg.sc_io_res =
	    bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);

	if (!(sc->sc_otg.sc_io_res))
		goto error;

	/*
	 * brcm,bcm2708-usb FDT provides two interrupts, we need only the USB
	 * interrupt (VC_USB).  The latest FDT for it provides an
	 * interrupt-names property and swapped them around, while older ones
	 * did not have interrupt-names and put the usb interrupt in the second
	 * position.  We'll attempt to use interrupt-names first with a fallback
	 * to the old method of assuming the index based on the compatible
	 * string.
	 */
	if (dwc_otg_irq_index(dev, &rid) != 0)
		rid = ofw_bus_is_compatible(dev, "brcm,bcm2708-usb") ? 1 : 0;
	sc->sc_otg.sc_irq_res =
	    bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->sc_otg.sc_irq_res == NULL)
		goto error;

	sc->sc_otg.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (sc->sc_otg.sc_bus.bdev == NULL)
		goto error;

	err = dwc_otg_init(&sc->sc_otg);
	if (err == 0) {
		err = device_probe_and_attach(sc->sc_otg.sc_bus.bdev);
	}
	if (err)
		goto error;

	return (0);

error:
	dwc_otg_detach(dev);
	return (ENXIO);
}

int
dwc_otg_detach(device_t dev)
{
	struct dwc_otg_fdt_softc *sc = device_get_softc(dev);

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);

	if (sc->sc_otg.sc_irq_res && sc->sc_otg.sc_intr_hdl) {
		/*
		 * only call dwc_otg_uninit() after dwc_otg_init()
		 */
		dwc_otg_uninit(&sc->sc_otg);

		bus_teardown_intr(dev, sc->sc_otg.sc_irq_res,
		    sc->sc_otg.sc_intr_hdl);
		sc->sc_otg.sc_intr_hdl = NULL;
	}
	/* free IRQ channel, if any */
	if (sc->sc_otg.sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0,
		    sc->sc_otg.sc_irq_res);
		sc->sc_otg.sc_irq_res = NULL;
	}
	/* free memory resource, if any */
	if (sc->sc_otg.sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0,
		    sc->sc_otg.sc_io_res);
		sc->sc_otg.sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_otg.sc_bus, NULL);

	return (0);
}

static device_method_t dwc_otg_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, dwc_otg_probe),
	DEVMETHOD(device_attach, dwc_otg_attach),
	DEVMETHOD(device_detach, dwc_otg_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

driver_t dwc_otg_driver = {
	.name = "dwcotg",
	.methods = dwc_otg_methods,
	.size = sizeof(struct dwc_otg_fdt_softc),
};

DRIVER_MODULE(dwcotg, simplebus, dwc_otg_driver, 0, 0);
MODULE_DEPEND(dwcotg, usb, 1, 1, 1);
