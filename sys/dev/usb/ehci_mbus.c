/*-
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * MBus attachment driver for the USB Enhanced Host Controller.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/lockmgr.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#define EHCI_VENDORID_MRVL	0x1286
#define EHCI_HC_DEVSTR		"Marvell Integrated USB 2.0 controller"

static device_attach_t ehci_mbus_attach;
static device_detach_t ehci_mbus_detach;
static device_shutdown_t ehci_mbus_shutdown;
static device_suspend_t ehci_mbus_suspend;
static device_resume_t ehci_mbus_resume;

static int
ehci_mbus_suspend(device_t self)
{
	ehci_softc_t *sc;
	int err;

	err = bus_generic_suspend(self);
	if (err)
		return (err);

	sc = device_get_softc(self);
	ehci_power(PWR_SUSPEND, sc);

	return (0);
}

static int
ehci_mbus_resume(device_t self)
{
	ehci_softc_t *sc;

	sc = device_get_softc(self);

	ehci_power(PWR_RESUME, sc);
	bus_generic_resume(self);

	return (0);
}

static int
ehci_mbus_shutdown(device_t self)
{
	ehci_softc_t *sc;
	int err;

	err = bus_generic_shutdown(self);
	if (err)
		return (err);

	sc = device_get_softc(self);
	ehci_shutdown(sc);

	return (0);
}

static int
ehci_mbus_probe(device_t self)
{

	device_set_desc(self, EHCI_HC_DEVSTR);

	return (BUS_PROBE_DEFAULT);
}

static int
ehci_mbus_attach(device_t self)
{
	ehci_softc_t *sc;
	bus_space_handle_t bsh;
	int err, rid;

	sc = device_get_softc(self);
	sc->sc_bus.usbrev = USBREV_2_0;

	rid = 0;
	sc->io_res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->io_res) {
		device_printf(self, "Could not map memory\n");
		return (ENXIO);
	}
	sc->iot = rman_get_bustag(sc->io_res);
	bsh = rman_get_bushandle(sc->io_res);

	/*
	 * Marvell EHCI host controller registers start at certain offset within
	 * the whole USB registers range, so create a subregion for the host
	 * mode configuration purposes.
	 */
	if (bus_space_subregion(sc->iot, bsh, MV_USB_HOST_OFST,
	    MV_USB_SIZE - MV_USB_HOST_OFST, &sc->ioh) != 0)
		panic("%s: unable to subregion USB host registers",
		    device_get_name(self));
	sc->sc_size = MV_USB_SIZE - MV_USB_HOST_OFST;

	/*
	 * Notice: Marvell EHCI controller has TWO interrupt lines, so make sure to
	 * use the correct rid for the main one (controller interrupt) --
	 * refer to obio_devices[] for the right resource number to use here.
	 */
	rid = 1;
	sc->irq_res = bus_alloc_resource_any(self, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(self, "Could not allocate irq\n");
		ehci_mbus_detach(self);
		return (ENXIO);
	}
	sc->sc_bus.bdev = device_add_child(self, "usb", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(self, "Could not add USB device\n");
		ehci_mbus_detach(self);
		return (ENOMEM);
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	sprintf(sc->sc_vendor, "Marvell");
	sc->sc_id_vendor = EHCI_VENDORID_MRVL;

	err = bus_setup_intr(self, sc->irq_res, INTR_TYPE_BIO,
	    NULL, (driver_intr_t*)ehci_intr, sc, &sc->ih);
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		sc->ih = NULL;
		ehci_mbus_detach(self);
		return (ENXIO);
	}

	/* There are no companion USB controllers */
	sc->sc_ncomp = 0;

	/* Allocate a parent dma tag for DMA maps */
	err = bus_dma_tag_create(bus_get_dma_tag(self), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, USB_DMA_NSEG, BUS_SPACE_MAXSIZE_32BIT, 0,
	    NULL, NULL, &sc->sc_bus.parent_dmatag);
	if (err) {
		device_printf(self, "Could not allocate parent DMA tag (%d)\n",
		    err);
		ehci_mbus_detach(self);
		return (ENXIO);
	}

	/* Allocate a dma tag for transfer buffers */
	err = bus_dma_tag_create(sc->sc_bus.parent_dmatag, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, USB_DMA_NSEG, BUS_SPACE_MAXSIZE_32BIT, 0,
	    busdma_lock_mutex, &Giant, &sc->sc_bus.buffer_dmatag);
	if (err) {
		device_printf(self, "Could not allocate buffer DMA tag (%d)\n",
		    err);
		ehci_mbus_detach(self);
		return (ENXIO);
	}

	/*
	 * Workaround for Marvell integrated EHCI controller: reset of
	 * the EHCI core clears the USBMODE register, which sets the core in
	 * an undefined state (neither host nor agent), so it needs to be set
	 * again for proper operation.
	 *
	 * Refer to errata document MV-S500832-00D.pdf (p. 5.24 GL USB-2) for
	 * details.
	 */
	sc->sc_flags |= EHCI_SCFLG_SETMODE;
	if (bootverbose)
		device_printf(self, "5.24 GL USB-2 workaround enabled\n");

	/* XXX all MV chips need it? */
	sc->sc_flags |= EHCI_SCFLG_FORCESPEED | EHCI_SCFLG_NORESTERM;

	err = ehci_init(sc);
	if (!err) {
		sc->sc_flags |= EHCI_SCFLG_DONEINIT;
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}

	if (err) {
		device_printf(self, "USB init failed err=%d\n", err);
		ehci_mbus_detach(self);
		return (EIO);
	}
	return (0);
}

static int
ehci_mbus_detach(device_t self)
{
	ehci_softc_t *sc;
	int err;

	sc = device_get_softc(self);
	if (sc->sc_flags & EHCI_SCFLG_DONEINIT) {
		ehci_detach(sc, 0);
		sc->sc_flags &= ~EHCI_SCFLG_DONEINIT;
	}

	/*
	 * Disable interrupts that might have been switched on in ehci_init()
	 */
	if (sc->iot && sc->ioh)
		bus_space_write_4(sc->iot, sc->ioh, EHCI_USBINTR, 0);
	if (sc->sc_bus.parent_dmatag != NULL)
		bus_dma_tag_destroy(sc->sc_bus.parent_dmatag);
	if (sc->sc_bus.buffer_dmatag != NULL)
		bus_dma_tag_destroy(sc->sc_bus.buffer_dmatag);

	if (sc->irq_res && sc->ih) {
		err = bus_teardown_intr(self, sc->irq_res, sc->ih);

		if (err)
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
		bus_release_resource(self, SYS_RES_MEMORY, 0, sc->io_res);
		sc->io_res = NULL;
		sc->iot = 0;
		sc->ioh = 0;
	}
	return (0);
}

static device_method_t ehci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ehci_mbus_probe),
	DEVMETHOD(device_attach, ehci_mbus_attach),
	DEVMETHOD(device_detach, ehci_mbus_detach),
	DEVMETHOD(device_suspend, ehci_mbus_suspend),
	DEVMETHOD(device_resume, ehci_mbus_resume),
	DEVMETHOD(device_shutdown, ehci_mbus_shutdown),

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

DRIVER_MODULE(ehci, mbus, ehci_driver, ehci_devclass, 0, 0);
