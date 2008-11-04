/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_standard.h>

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_sw_transfer.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>
#include <dev/usb2/controller/musb2_otg.h>

#include <sys/rman.h>

static device_probe_t musbotg_probe;
static device_attach_t musbotg_attach;
static device_detach_t musbotg_detach;
static device_shutdown_t musbotg_shutdown;

struct musbotg_super_softc {
	struct musbotg_softc sc_otg;	/* must be first */
};

static void
musbotg_vbus_interrupt(struct musbotg_super_softc *sc)
{
	uint8_t vbus_val = 1;		/* fake VBUS on - TODO */

	/* just forward it */

	(sc->sc_otg.sc_bus.methods->vbus_interrupt)
	    (&sc->sc_otg.sc_bus, vbus_val);
	return;
}

static void
musbotg_clocks_on(void *arg)
{
#if 0
	struct musbotg_super_softc *sc = arg;

#endif

	return;
}

static void
musbotg_clocks_off(void *arg)
{
#if 0
	struct musbotg_super_softc *sc = arg;

#endif

	return;
}

static int
musbotg_probe(device_t dev)
{
	device_set_desc(dev, "MUSB OTG integrated USB controller");
	return (0);
}

static int
musbotg_attach(device_t dev)
{
	struct musbotg_super_softc *sc = device_get_softc(dev);
	int err;
	int rid;

	if (sc == NULL) {
		return (ENXIO);
	}
	/* setup MUSB OTG USB controller interface softc */

	sc->sc_otg.sc_clocks_on = &musbotg_clocks_on;
	sc->sc_otg.sc_clocks_off = &musbotg_clocks_off;
	sc->sc_otg.sc_clocks_arg = sc;

	/* get all DMA memory */

	if (usb2_bus_mem_alloc_all(&sc->sc_otg.sc_bus,
	    USB_GET_DMA_TAG(dev), NULL)) {
		return (ENOMEM);
	}
	rid = 0;
	sc->sc_otg.sc_io_res =
	    bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);

	if (!(sc->sc_otg.sc_io_res)) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_otg.sc_io_tag = rman_get_bustag(sc->sc_otg.sc_io_res);
	sc->sc_otg.sc_io_hdl = rman_get_bushandle(sc->sc_otg.sc_io_res);
	sc->sc_otg.sc_io_size = rman_get_size(sc->sc_otg.sc_io_res);

	rid = 0;
	sc->sc_otg.sc_irq_res =
	    bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (!(sc->sc_otg.sc_irq_res)) {
		goto error;
	}
	sc->sc_otg.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_otg.sc_bus.bdev)) {
		goto error;
	}
	device_set_ivars(sc->sc_otg.sc_bus.bdev, &sc->sc_otg.sc_bus);

	err = usb2_config_td_setup(&sc->sc_otg.sc_config_td, sc,
	    &sc->sc_otg.sc_bus.mtx, NULL, 0, 4);
	if (err) {
		device_printf(dev, "could not setup config thread!\n");
		goto error;
	}
#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(dev, sc->sc_otg.sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (void *)musbotg_interrupt, sc, &sc->sc_otg.sc_intr_hdl);
#else
	err = bus_setup_intr(dev, sc->sc_otg.sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    (void *)musbotg_interrupt, sc, &sc->sc_otg.sc_intr_hdl);
#endif
	if (err) {
		sc->sc_otg.sc_intr_hdl = NULL;
		goto error;
	}
	err = musbotg_init(&sc->sc_otg);
	if (!err) {
		err = device_probe_and_attach(sc->sc_otg.sc_bus.bdev);
	}
	if (err) {
		goto error;
	} else {
		/* poll VBUS one time */
		musbotg_vbus_interrupt(sc);
	}
	return (0);

error:
	musbotg_detach(dev);
	return (ENXIO);
}

static int
musbotg_detach(device_t dev)
{
	struct musbotg_super_softc *sc = device_get_softc(dev);
	device_t bdev;
	int err;

	if (sc->sc_otg.sc_bus.bdev) {
		bdev = sc->sc_otg.sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_all_children(dev);

	if (sc->sc_otg.sc_irq_res && sc->sc_otg.sc_intr_hdl) {
		/*
		 * only call musbotg_uninit() after musbotg_init()
		 */
		musbotg_uninit(&sc->sc_otg);

		err = bus_teardown_intr(dev, sc->sc_otg.sc_irq_res,
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
	usb2_config_td_unsetup(&sc->sc_otg.sc_config_td);

	usb2_bus_mem_free_all(&sc->sc_otg.sc_bus, NULL);

	return (0);
}

static int
musbotg_shutdown(device_t dev)
{
	struct musbotg_super_softc *sc = device_get_softc(dev);
	int err;

	err = bus_generic_shutdown(dev);
	if (err)
		return (err);

	musbotg_uninit(&sc->sc_otg);

	return (0);
}

static device_method_t musbotg_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, musbotg_probe),
	DEVMETHOD(device_attach, musbotg_attach),
	DEVMETHOD(device_detach, musbotg_detach),
	DEVMETHOD(device_shutdown, musbotg_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t musbotg_driver = {
	"musbotg",
	musbotg_methods,
	sizeof(struct musbotg_super_softc),
};

static devclass_t musbotg_devclass;

DRIVER_MODULE(musbotg, atmelarm, musbotg_driver, musbotg_devclass, 0, 0);
MODULE_DEPEND(musbotg, usb2_controller, 1, 1, 1);
MODULE_DEPEND(musbotg, usb2_core, 1, 1, 1);
