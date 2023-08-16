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
#include "opt_acpi.h"

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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

#include <dev/usb/controller/dwc_otg.h>

static device_probe_t dwc_otg_probe;
static device_attach_t dwc_otg_attach;
static device_attach_t dwc_otg_detach;

static char *dwc_otg_ids[] = {
	"BCM2848",
	NULL
};

static int
dwc_otg_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("dwc_otg"))
		return (ENXIO);

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, dwc_otg_ids, NULL);
	if (rv > 0)
		return (rv);

	device_set_desc(dev, "DWC OTG 2.0 integrated USB controller");

	return (BUS_PROBE_DEFAULT);
}

static int
dwc_otg_attach(device_t dev)
{
	struct dwc_otg_softc *sc = device_get_softc(dev);
	int err;
	int rid;

	sc->sc_bus.parent = dev;

	/* assume device mode (this is only used for the Raspberry Pi 4's
	 * USB-C port, which only works in device mode) */
	sc->sc_mode = DWC_MODE_DEVICE;

	rid = 0;
	sc->sc_io_res =
	    bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);

	if (sc->sc_io_res == NULL)
		goto error;

	rid = 0;
	sc->sc_irq_res =
	    bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->sc_irq_res == NULL)
		goto error;

	err = dwc_otg_init(sc);
	if (err == 0) {
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}
	if (err)
		goto error;

	return (0);

error:
	dwc_otg_detach(dev);
	return (ENXIO);
}

static int
dwc_otg_detach(device_t dev)
{
	struct dwc_otg_softc *sc = device_get_softc(dev);

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);

	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		/*
		 * only call dwc_otg_uninit() after dwc_otg_init()
		 */
		dwc_otg_uninit(sc);

		bus_teardown_intr(dev, sc->sc_irq_res,
		    sc->sc_intr_hdl);
		sc->sc_intr_hdl = NULL;
	}
	/* free IRQ channel, if any */
	if (sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0,
		    sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	/* free memory resource, if any */
	if (sc->sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_bus, NULL);

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

static driver_t dwc_otg_driver = {
	.name = "dwcotg",
	.methods = dwc_otg_methods,
	.size = sizeof(struct dwc_otg_softc),
};

DRIVER_MODULE(dwcotg, acpi, dwc_otg_driver, 0, 0);
MODULE_DEPEND(dwcotg, usb, 1, 1, 1);
