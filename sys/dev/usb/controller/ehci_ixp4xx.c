/*-
 * Copyright (c) 2008 Sam Leffler.  All rights reserved.
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

/*
 * IXP435 attachment driver for the USB Enhanced Host Controller.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

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
#include <dev/usb/controller/ehci.h>
#include <dev/usb/controller/ehcireg.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#define EHCI_VENDORID_IXP4XX	0x42fa05
#define EHCI_HC_DEVSTR		"IXP4XX Integrated USB 2.0 controller"

struct ixp_ehci_softc {
	ehci_softc_t		base;	/* storage for EHCI code */
	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;
	struct bus_space	tag;	/* tag for private bus space ops */
};

static device_attach_t ehci_ixp_attach;
static device_detach_t ehci_ixp_detach;
static device_shutdown_t ehci_ixp_shutdown;
static device_suspend_t ehci_ixp_suspend;
static device_resume_t ehci_ixp_resume;

static uint8_t ehci_bs_r_1(void *, bus_space_handle_t, bus_size_t);
static void ehci_bs_w_1(void *, bus_space_handle_t, bus_size_t, u_int8_t);
static uint16_t ehci_bs_r_2(void *, bus_space_handle_t, bus_size_t);
static void ehci_bs_w_2(void *, bus_space_handle_t, bus_size_t, uint16_t);
static uint32_t ehci_bs_r_4(void *, bus_space_handle_t, bus_size_t);
static void ehci_bs_w_4(void *, bus_space_handle_t, bus_size_t, uint32_t);

static int
ehci_ixp_suspend(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);
	int err;

	err = bus_generic_suspend(self);
	if (err)
		return (err);
	ehci_suspend(sc);
	return (0);
}

static int
ehci_ixp_resume(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);

	ehci_resume(sc);

	bus_generic_resume(self);

	return (0);
}

static int
ehci_ixp_shutdown(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);
	int err;

	err = bus_generic_shutdown(self);
	if (err)
		return (err);
	ehci_shutdown(sc);

	return (0);
}

static int
ehci_ixp_probe(device_t self)
{

	device_set_desc(self, EHCI_HC_DEVSTR);

	return (BUS_PROBE_DEFAULT);
}

static int
ehci_ixp_attach(device_t self)
{
	struct ixp_ehci_softc *isc = device_get_softc(self);
	ehci_softc_t *sc = &isc->base;
	int err;
	int rid;

	/* initialise some bus fields */
	sc->sc_bus.parent = self;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = EHCI_MAX_DEVICES;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_bus,
	    USB_GET_DMA_TAG(self), &ehci_iterate_hw_softc)) {
		return (ENOMEM);
	}

	/* NB: hints fix the memory location and irq */

	rid = 0;
	sc->sc_io_res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->sc_io_res) {
		device_printf(self, "Could not map memory\n");
		goto error;
	}

	/*
	 * Craft special resource for bus space ops that handle
	 * byte-alignment of non-word addresses.  Also, since
	 * we're already intercepting bus space ops we handle
	 * the register window offset that could otherwise be
	 * done with bus_space_subregion.
	 */
	isc->iot = rman_get_bustag(sc->sc_io_res);
	isc->tag.bs_cookie = isc->iot;
	/* read single */
	isc->tag.bs_r_1	= ehci_bs_r_1,
	isc->tag.bs_r_2	= ehci_bs_r_2,
	isc->tag.bs_r_4	= ehci_bs_r_4,
	/* write (single) */
	isc->tag.bs_w_1	= ehci_bs_w_1,
	isc->tag.bs_w_2	= ehci_bs_w_2,
	isc->tag.bs_w_4	= ehci_bs_w_4,

	sc->sc_io_tag = &isc->tag;
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = IXP435_USB1_SIZE - 0x100;

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(self, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(self, "Could not allocate irq\n");
		goto error;
	}
	sc->sc_bus.bdev = device_add_child(self, "usbus", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(self, "Could not add USB device\n");
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);
	device_set_desc(sc->sc_bus.bdev, EHCI_HC_DEVSTR);

	sprintf(sc->sc_vendor, "Intel");


	err = bus_setup_intr(self, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)ehci_interrupt, sc, &sc->sc_intr_hdl);
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		sc->sc_intr_hdl = NULL;
		goto error;
	}

	/*
	 * Arrange to force Host mode, select big-endian byte alignment,
	 * and arrange to not terminate reset operations (the adapter
	 * will ignore it if we do but might as well save a reg write).
	 * Also, the controller has an embedded Transaction Translator
	 * which means port speed must be read from the Port Status
	 * register following a port enable.
	 */
	sc->sc_flags |= EHCI_SCFLG_TT
		     | EHCI_SCFLG_SETMODE
		     | EHCI_SCFLG_BIGEDESC
		     | EHCI_SCFLG_BIGEMMIO
		     | EHCI_SCFLG_NORESTERM
		     ;

	err = ehci_init(sc);
	if (!err) {
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}
	if (err) {
		device_printf(self, "USB init failed err=%d\n", err);
		goto error;
	}
	return (0);

error:
	ehci_ixp_detach(self);
	return (ENXIO);
}

static int
ehci_ixp_detach(device_t self)
{
	struct ixp_ehci_softc *isc = device_get_softc(self);
	ehci_softc_t *sc = &isc->base;
	device_t bdev;
	int err;

 	if (sc->sc_bus.bdev) {
		bdev = sc->sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(self, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_all_children(self);

 	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		/*
		 * only call ehci_detach() after ehci_init()
		 */
		ehci_detach(sc);

		err = bus_teardown_intr(self, sc->sc_irq_res, sc->sc_intr_hdl);

		if (err)
			/* XXX or should we panic? */
			device_printf(self, "Could not tear down irq, %d\n",
			    err);
		sc->sc_intr_hdl = NULL;
	}

 	if (sc->sc_irq_res) {
		bus_release_resource(self, SYS_RES_IRQ, 0, sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	if (sc->sc_io_res) {
		bus_release_resource(self, SYS_RES_MEMORY, 0,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_bus, &ehci_iterate_hw_softc);

	return (0);
}

/*
 * Bus space accessors for PIO operations.
 */

static uint8_t
ehci_bs_r_1(void *t, bus_space_handle_t h, bus_size_t o)
{
	return bus_space_read_1((bus_space_tag_t) t, h,
	    0x100 + (o &~ 3) + (3 - (o & 3)));
}

static void
ehci_bs_w_1(void *t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	panic("%s", __func__);
}

static uint16_t
ehci_bs_r_2(void *t, bus_space_handle_t h, bus_size_t o)
{
	return bus_space_read_2((bus_space_tag_t) t, h,
	    0x100 + (o &~ 3) + (2 - (o & 3)));
}

static void
ehci_bs_w_2(void *t, bus_space_handle_t h, bus_size_t o, uint16_t v)
{
	panic("%s", __func__);
}

static uint32_t
ehci_bs_r_4(void *t, bus_space_handle_t h, bus_size_t o)
{
	return bus_space_read_4((bus_space_tag_t) t, h, 0x100 + o);
}

static void
ehci_bs_w_4(void *t, bus_space_handle_t h, bus_size_t o, uint32_t v)
{
	bus_space_write_4((bus_space_tag_t) t, h, 0x100 + o, v);
}

static device_method_t ehci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ehci_ixp_probe),
	DEVMETHOD(device_attach, ehci_ixp_attach),
	DEVMETHOD(device_detach, ehci_ixp_detach),
	DEVMETHOD(device_suspend, ehci_ixp_suspend),
	DEVMETHOD(device_resume, ehci_ixp_resume),
	DEVMETHOD(device_shutdown, ehci_ixp_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t ehci_driver = {
	"ehci",
	ehci_methods,
	sizeof(struct ixp_ehci_softc),
};

static devclass_t ehci_devclass;

DRIVER_MODULE(ehci, ixp, ehci_driver, ehci_devclass, 0, 0);
MODULE_DEPEND(ehci, usb, 1, 1, 1);
