/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * USB Enhanced Host Controller Driver, a.k.a. USB 2.0 controller.
 *
 * The EHCI 1.0 spec can be found at
 * http://developer.intel.com/technology/usb/download/ehci-r10.pdf
 * and the USB 2.0 spec at
 * http://www.usb.org/developers/docs/usb_20.zip
 */

/* The low level controller code for EHCI has been split into
 * PCI probes and EHCI specific code. This was done to facilitate the
 * sharing of code between *BSD's
 */

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
/*#include <dev/usb/usbdivar.h>  */
/*#include <dev/usb/usb_mem.h>   */

#include <mips/rmi/ehcireg.h>
#include <mips/rmi/ehcivar.h>

#ifdef USB_DEBUG
#define EHCI_DEBUG USB_DEBUG
#define DPRINTF(x)	do { if (ehcidebug) logprintf x; } while (0)
extern int ehcidebug;

#else
#define DPRINTF(x)
#endif

static int ehci_xls_attach(device_t self);
static int ehci_xls_detach(device_t self);
static int ehci_xls_shutdown(device_t self);
static int ehci_xls_suspend(device_t self);
static int ehci_xls_resume(device_t self);
static void ehci_xls_givecontroller(device_t self);
static void ehci_xls_takecontroller(device_t self);

static int
ehci_xls_suspend(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);
	int err;

	err = bus_generic_suspend(self);
	if (err)
		return (err);
	ehci_power(PWR_SUSPEND, sc);

	return 0;
}

static int
ehci_xls_resume(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);

	ehci_xls_takecontroller(self);
	ehci_power(PWR_RESUME, sc);
	bus_generic_resume(self);

	return 0;
}

static int
ehci_xls_shutdown(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);
	int err;

	err = bus_generic_shutdown(self);
	if (err)
		return (err);
	ehci_shutdown(sc);
	ehci_xls_givecontroller(self);

	return 0;
}


static const char *xlr_usb_dev_desc = "RMI XLR USB 2.0 controller";
static const char *xlr_vendor_desc = "RMI Corp";
static int
ehci_xls_probe(device_t self)
{

	/* TODO see if usb is enabled on the board */
	device_set_desc(self, xlr_usb_dev_desc);
	return BUS_PROBE_DEFAULT;
}

static int
ehci_xls_attach(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);
	device_t parent;
	device_t *neighbors;
	int err;
	int rid;
	int count;
	int res;


	sc->sc_bus.usbrev = USBREV_2_0;

	rid = 0;
	sc->io_res = bus_alloc_resource(self, SYS_RES_MEMORY, &rid,
	    0ul, ~0ul, 0x400, RF_ACTIVE);
	if (!sc->io_res) {
		device_printf(self, "Could not map memory\n");
		return ENXIO;
	}
	sc->iot = rman_get_bustag(sc->io_res);
	sc->ioh = rman_get_bushandle(sc->io_res);

	rid = 0;
	sc->irq_res = bus_alloc_resource(self, SYS_RES_IRQ, &rid,
	    39, 39, 1, RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(self, "Could not allocate irq\n");
		ehci_xls_detach(self);
		return ENXIO;
	}
	sc->sc_bus.bdev = device_add_child(self, "usb", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(self, "Could not add USB device\n");
		ehci_xls_detach(self);
		return ENOMEM;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	/* ehci_pci_match will never return NULL if ehci_pci_probe succeeded */
	device_set_desc(sc->sc_bus.bdev, xlr_usb_dev_desc);
	sprintf(sc->sc_vendor, xlr_vendor_desc);

	err = bus_setup_intr(self, sc->irq_res, INTR_TYPE_BIO,
	    (driver_intr_t *) ehci_intr, sc, &sc->ih);
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		sc->ih = NULL;
		ehci_xls_detach(self);
		return ENXIO;
	}
	/*
	 * Find companion controllers.  According to the spec they always
	 * have lower function numbers so they should be enumerated already.
	 */
	parent = device_get_parent(self);
	res = device_get_children(parent, &neighbors, &count);
	if (res != 0) {
		device_printf(self, "Error finding companion busses\n");
		ehci_xls_detach(self);
		return ENXIO;
	}
	sc->sc_ncomp = 0;

	ehci_xls_takecontroller(self);
	err = ehci_init(sc);
	if (!err) {
		sc->sc_flags |= EHCI_SCFLG_DONEINIT;
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}
	if (err) {
		device_printf(self, "USB init failed err=%d\n", err);
		ehci_xls_detach(self);
		return EIO;
	}
	return 0;
}

static int
ehci_xls_detach(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);

	if (sc->sc_flags & EHCI_SCFLG_DONEINIT) {
		ehci_detach(sc, 0);
		sc->sc_flags &= ~EHCI_SCFLG_DONEINIT;
	}
	/*
	 * disable interrupts that might have been switched on in ehci_init
	 */
	if (sc->iot && sc->ioh)
		bus_space_write_4(sc->iot, sc->ioh, EHCI_USBINTR, 0);

	if (sc->irq_res && sc->ih) {
		int err = bus_teardown_intr(self, sc->irq_res, sc->ih);

		if (err)
			/* XXX or should we panic? */
			device_printf(self, "Could not tear down irq, %d\n",
			    err);
		sc->ih = NULL;
	}
	if (sc->sc_bus.bdev) {
		device_delete_child(self, sc->sc_bus.bdev);
		sc->sc_bus.bdev = NULL;
	}
	if (sc->irq_res) {
		bus_release_resource(self, SYS_RES_IRQ, 0, sc->irq_res);
		sc->irq_res = NULL;
	}
	if (sc->io_res) {
		bus_release_resource(self, SYS_RES_MEMORY, PCI_CBMEM, sc->io_res);
		sc->io_res = NULL;
		sc->iot = 0;
		sc->ioh = 0;
	}
	return 0;
}

static void
ehci_xls_takecontroller(device_t self)
{
	//device_printf(self, "In func %s\n", __func__);
}

static void
ehci_xls_givecontroller(device_t self)
{
	//device_printf(self, "In func %s\n", __func__);
}

static device_method_t ehci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ehci_xls_probe),
	DEVMETHOD(device_attach, ehci_xls_attach),
	DEVMETHOD(device_detach, ehci_xls_detach),
	DEVMETHOD(device_suspend, ehci_xls_suspend),
	DEVMETHOD(device_resume, ehci_xls_resume),
	DEVMETHOD(device_shutdown, ehci_xls_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t ehci_driver = {
	"ehci",
	ehci_methods,
	sizeof(ehci_softc_t),
};

static devclass_t ehci_devclass;

DRIVER_MODULE(ehci, iodi, ehci_driver, ehci_devclass, 0, 0);
/* DRIVER_MODULE(ehci, cardbus, ehci_driver, ehci_devclass, 0, 0); */
