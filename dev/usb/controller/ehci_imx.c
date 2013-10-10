/*-
 * Copyright (c) 2010-2012 Semihalf
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
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

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/ehci.h>
#include <dev/usb/controller/ehcireg.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include "opt_platform.h"

#define	FSL_EHCI_COUNT		4
#define FSL_EHCI_REG_OFF	0x100
#define FSL_EHCI_REG_SIZE	0x100
#define	FSL_EHCI_REG_STEP	0x200

struct imx_ehci_softc {
	ehci_softc_t		ehci[FSL_EHCI_COUNT];
	/* MEM + 4 interrupts */
	struct resource		*sc_res[1 + FSL_EHCI_COUNT];
};

/* i.MX515 have 4 EHCI inside USB core */
/* TODO: we can get number of EHCIs by IRQ allocation */
static struct resource_spec imx_ehci_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	/* RF_OPTIONAL will allow to use driver for systems with 3 EHCIs */
	{ SYS_RES_IRQ,		3,	RF_ACTIVE | RF_OPTIONAL },
	{ -1, 0 }
};

/* Forward declarations */
static int	fsl_ehci_attach(device_t self);
static int	fsl_ehci_detach(device_t self);
static int	fsl_ehci_probe(device_t self);

static device_method_t ehci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, fsl_ehci_probe),
	DEVMETHOD(device_attach, fsl_ehci_attach),
	DEVMETHOD(device_detach, fsl_ehci_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{ 0, 0 }
};

/* kobj_class definition */
static driver_t ehci_driver = {
	"ehci",
	ehci_methods,
	sizeof(struct imx_ehci_softc)
};

static devclass_t ehci_devclass;

DRIVER_MODULE(ehci, simplebus, ehci_driver, ehci_devclass, 0, 0);
MODULE_DEPEND(ehci, usb, 1, 1, 1);

/*
 * Public methods
 */
static int
fsl_ehci_probe(device_t dev)
{

	if (ofw_bus_is_compatible(dev, "fsl,usb-4core") == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale integrated USB controller");

	return (BUS_PROBE_DEFAULT);
}

static int
fsl_ehci_attach(device_t self)
{
	struct imx_ehci_softc *sc;
	bus_space_tag_t iot;
	ehci_softc_t *esc;
	int err, i, rid;

	sc = device_get_softc(self);
	rid = 0;

	/* Allocate io resource for EHCI */
	if (bus_alloc_resources(self, imx_ehci_spec, sc->sc_res)) {
		device_printf(self, "could not allocate resources\n");
		return (ENXIO);
	}
	iot = rman_get_bustag(sc->sc_res[0]);

	/* TODO: Power/clock enable */
	/* TODO: basic init */

	for (i = 0; i < FSL_EHCI_COUNT; i ++) {
		/* No interrupt - no driver */
		if (sc->sc_res[1 + i] == NULL)
			continue;

		esc = &sc->ehci[i];
		esc->sc_io_tag = iot;
		esc->sc_bus.parent = self;
		esc->sc_bus.devices = esc->sc_devices;
		esc->sc_bus.devices_max = EHCI_MAX_DEVICES;

		if (usb_bus_mem_alloc_all(&esc->sc_bus, USB_GET_DMA_TAG(self),
		    &ehci_iterate_hw_softc))
			continue;

		/*
		 * Set handle to USB related registers subregion used by
		 * generic EHCI driver.
		 */
		err = bus_space_subregion(iot,
		    rman_get_bushandle(sc->sc_res[0]),
		    FSL_EHCI_REG_OFF + (i * FSL_EHCI_REG_STEP),
		    FSL_EHCI_REG_SIZE, &esc->sc_io_hdl);
		if (err != 0)
			continue;

		/* Setup interrupt handler */
		err = bus_setup_intr(self, sc->sc_res[1 + i], INTR_TYPE_BIO,
		    NULL, (driver_intr_t *)ehci_interrupt, esc,
		    &esc->sc_intr_hdl);
		if (err) {
			device_printf(self, "Could not setup irq, "
			    "for EHCI%d %d\n", i, err);
			continue;
		}

		/* Add USB device */
		esc->sc_bus.bdev = device_add_child(self, "usbus", -1);
		if (!esc->sc_bus.bdev) {
			device_printf(self, "Could not add USB device\n");
			err = bus_teardown_intr(self, esc->sc_irq_res,
			    esc->sc_intr_hdl);
			if (err)
				device_printf(self, "Could not tear down irq,"
				    " %d\n", err);
			continue;
		}
		device_set_ivars(esc->sc_bus.bdev, &esc->sc_bus);

		esc->sc_id_vendor = 0x1234;
		strlcpy(esc->sc_vendor, "Freescale", sizeof(esc->sc_vendor));

		/* Set flags */
		esc->sc_flags |= EHCI_SCFLG_DONTRESET | EHCI_SCFLG_NORESTERM;

		err = ehci_init(esc);
		if (!err) {
			esc->sc_flags |= EHCI_SCFLG_DONEINIT;
			err = device_probe_and_attach(esc->sc_bus.bdev);
		} else {
			device_printf(self, "USB init failed err=%d\n", err);

			device_delete_child(self, esc->sc_bus.bdev);
			esc->sc_bus.bdev = NULL;

			err = bus_teardown_intr(self, esc->sc_irq_res,
			    esc->sc_intr_hdl);
			if (err)
				device_printf(self, "Could not tear down irq,"
				    " %d\n", err);

			continue;
		}
	}
	return (0);
}

static int
fsl_ehci_detach(device_t self)
{
	struct imx_ehci_softc *sc;
	ehci_softc_t *esc;
	int err, i;

	sc = device_get_softc(self);

	for (i = 0; i < FSL_EHCI_COUNT; i ++) {
		esc = &sc->ehci[i];
		if (esc->sc_flags & EHCI_SCFLG_DONEINIT)
			continue;
		/*
		 * only call ehci_detach() after ehci_init()
		 */
		if (esc->sc_flags & EHCI_SCFLG_DONEINIT) {
			ehci_detach(esc);
			esc->sc_flags &= ~EHCI_SCFLG_DONEINIT;
		}

		/*
		 * Disable interrupts that might have been switched on in
		 * ehci_init.
		 */
		if (esc->sc_io_tag && esc->sc_io_hdl)
			bus_space_write_4(esc->sc_io_tag, esc->sc_io_hdl,
			    EHCI_USBINTR, 0);

		if (esc->sc_irq_res && esc->sc_intr_hdl) {
			err = bus_teardown_intr(self, esc->sc_irq_res,
			    esc->sc_intr_hdl);
			if (err) {
				device_printf(self, "Could not tear down irq,"
				    " %d\n", err);
				return (err);
			}
			esc->sc_intr_hdl = NULL;
		}

		if (esc->sc_bus.bdev) {
			device_delete_child(self, esc->sc_bus.bdev);
			esc->sc_bus.bdev = NULL;
		}
	}

	/* During module unload there are lots of children leftover */
	device_delete_children(self);

	if (sc->sc_res[0])
		bus_release_resources(self, imx_ehci_spec, sc->sc_res);

	return (0);
}
