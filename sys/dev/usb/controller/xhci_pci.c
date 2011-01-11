/*-
 * Copyright (c) 2010 Hans Petter Selasky. All rights reserved.
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
#include <dev/usb/usb_pci.h>
#include <dev/usb/controller/xhci.h>
#include <dev/usb/controller/xhcireg.h>

static device_probe_t xhci_pci_probe;
static device_attach_t xhci_pci_attach;
static device_detach_t xhci_pci_detach;
static device_suspend_t xhci_pci_suspend;
static device_resume_t xhci_pci_resume;
static device_shutdown_t xhci_pci_shutdown;
static void xhci_pci_takecontroller(device_t);

static device_method_t xhci_device_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe, xhci_pci_probe),
	DEVMETHOD(device_attach, xhci_pci_attach),
	DEVMETHOD(device_detach, xhci_pci_detach),
	DEVMETHOD(device_suspend, xhci_pci_suspend),
	DEVMETHOD(device_resume, xhci_pci_resume),
	DEVMETHOD(device_shutdown, xhci_pci_shutdown),
	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t xhci_driver = {
	.name = "xhci",
	.methods = xhci_device_methods,
	.size = sizeof(struct xhci_softc),
};

static devclass_t xhci_devclass;

DRIVER_MODULE(xhci, pci, xhci_driver, xhci_devclass, 0, 0);
MODULE_DEPEND(xhci, usb, 1, 1, 1);

static int
xhci_pci_suspend(device_t self)
{
	struct xhci_softc *sc = device_get_softc(self);
	int err;

	err = bus_generic_suspend(self);
	if (err)
		return (err);
	xhci_suspend(sc);
	return (0);
}

static int
xhci_pci_resume(device_t self)
{
	struct xhci_softc *sc = device_get_softc(self);

	xhci_pci_takecontroller(self);
	xhci_resume(sc);

	bus_generic_resume(self);

	return (0);
}

static int
xhci_pci_shutdown(device_t self)
{
	struct xhci_softc *sc = device_get_softc(self);
	int err;

	err = bus_generic_shutdown(self);
	if (err)
		return (err);
	xhci_shutdown(sc);

	return (0);
}

static const char *
xhci_pci_match(device_t self)
{
	if ((pci_get_class(self) == PCIC_SERIALBUS)
	    && (pci_get_subclass(self) == PCIS_SERIALBUS_USB)
	    && (pci_get_progif(self) == PCI_INTERFACE_XHCI)) {
		return ("XHCI (generic) USB 3.0 controller");
	}
	return (NULL);			/* dunno */
}

static int
xhci_pci_probe(device_t self)
{
	const char *desc = xhci_pci_match(self);

	if (desc) {
		device_set_desc(self, desc);
		return (0);
	} else {
		return (ENXIO);
	}
}

static int
xhci_pci_attach(device_t self)
{
	struct xhci_softc *sc = device_get_softc(self);
	int err;
	int rid;

	/* XXX check for 64-bit capability */

	if (xhci_init(sc, self)) {
		device_printf(self, "Could not initialize softc\n");
		goto error;
	}

	pci_enable_busmaster(self);

	rid = PCI_XHCI_CBMEM;
	sc->sc_io_res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_io_res) {
		device_printf(self, "Could not map memory\n");
		goto error;
	}
	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(self, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(self, "Could not allocate IRQ\n");
		goto error;
	}
	sc->sc_bus.bdev = device_add_child(self, "usbus", -1);
	if (sc->sc_bus.bdev == NULL) {
		device_printf(self, "Could not add USB device\n");
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	sprintf(sc->sc_vendor, "0x%04x", pci_get_vendor(self));

#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(self, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)xhci_interrupt, sc, &sc->sc_intr_hdl);
#else
	err = bus_setup_intr(self, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    (driver_intr_t *)xhci_interrupt, sc, &sc->sc_intr_hdl);
#endif
	if (err) {
		device_printf(self, "Could not setup IRQ, err=%d\n", err);
		sc->sc_intr_hdl = NULL;
		goto error;
	}
	xhci_pci_takecontroller(self);

	err = xhci_halt_controller(sc);

	if (err == 0)
		err = xhci_start_controller(sc);

	if (err == 0)
		err = device_probe_and_attach(sc->sc_bus.bdev);

	if (err) {
		device_printf(self, "XHCI halt/start/probe failed err=%d\n", err);
		goto error;
	}
	return (0);

error:
	xhci_pci_detach(self);
	return (ENXIO);
}

static int
xhci_pci_detach(device_t self)
{
	struct xhci_softc *sc = device_get_softc(self);
	device_t bdev;

	if (sc->sc_bus.bdev != NULL) {
		bdev = sc->sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(self, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_all_children(self);

	pci_disable_busmaster(self);

	if (sc->sc_irq_res && sc->sc_intr_hdl) {

		xhci_halt_controller(sc);

		bus_teardown_intr(self, sc->sc_irq_res, sc->sc_intr_hdl);
		sc->sc_intr_hdl = NULL;
	}
	if (sc->sc_irq_res) {
		bus_release_resource(self, SYS_RES_IRQ, 0, sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	if (sc->sc_io_res) {
		bus_release_resource(self, SYS_RES_MEMORY, PCI_XHCI_CBMEM,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}

	xhci_uninit(sc);

	return (0);
}

static void
xhci_pci_takecontroller(device_t self)
{
	struct xhci_softc *sc = device_get_softc(self);
	uint32_t cparams;
	uint32_t eecp;
	uint32_t eec;
	uint16_t to;
	uint8_t bios_sem;

	cparams = XREAD4(sc, capa, XHCI_HCSPARAMS0);

	eec = -1;

	/* Synchronise with the BIOS if it owns the controller. */
	for (eecp = XHCI_HCS0_XECP(cparams) << 2; eecp != 0 && XHCI_XECP_NEXT(eec);
	    eecp += XHCI_XECP_NEXT(eec) << 2) {
		eec = XREAD4(sc, capa, eecp);

		if (XHCI_XECP_ID(eec) != XHCI_ID_USB_LEGACY)
			continue;
		bios_sem = XREAD1(sc, capa, eecp +
		    XHCI_XECP_BIOS_SEM);
		if (bios_sem == 0)
			continue;
		device_printf(sc->sc_bus.bdev, "waiting for BIOS "
		    "to give up control\n");
		XWRITE1(sc, capa, eecp +
		    XHCI_XECP_OS_SEM, 1);
		to = 500;
		while (1) {
			bios_sem = XREAD1(sc, capa, eecp +
			    XHCI_XECP_BIOS_SEM);
			if (bios_sem == 0)
				break;

			if (--to == 0) {
				device_printf(sc->sc_bus.bdev,
				    "timed out waiting for BIOS\n");
				break;
			}
			usb_pause_mtx(NULL, hz / 100);	/* wait 10ms */
		}
	}
}
