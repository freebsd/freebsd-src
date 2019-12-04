/*-
 * Copyright (c) 2016 Emmanuel Vadot <manu@freebsd.org> All rights reserved.
 * Copyright (c) 2006 M. Warner Losh <imp@FreeBSD.org>
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

/*
 * Generic OHCI driver based on AT91 OHCI
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>
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
#include <dev/usb/controller/ohci.h>
#include <dev/usb/controller/ohcireg.h>

#ifdef EXT_RESOURCES
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy.h>
#include <dev/extres/phy/phy_usb.h>
#endif

#include "generic_usb_if.h"

#ifdef EXT_RESOURCES
struct clk_list {
	TAILQ_ENTRY(clk_list)	next;
	clk_t			clk;
};
struct phy_list {
	TAILQ_ENTRY(phy_list)	next;
	phy_t			phy;
};
struct hwrst_list {
	TAILQ_ENTRY(hwrst_list)	next;
	hwreset_t		rst;
};
#endif

struct generic_ohci_softc {
	ohci_softc_t	ohci_sc;

#ifdef EXT_RESOURCES
	TAILQ_HEAD(, clk_list)		clk_list;
	TAILQ_HEAD(, phy_list)		phy_list;
	TAILQ_HEAD(, hwrst_list)	rst_list;
#endif
};

static int generic_ohci_detach(device_t);

static int
generic_ohci_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "generic-ohci"))
		return (ENXIO);

	device_set_desc(dev, "Generic OHCI Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
generic_ohci_attach(device_t dev)
{
	struct generic_ohci_softc *sc = device_get_softc(dev);
	int err, rid;
#ifdef EXT_RESOURCES
	int off;
	struct clk_list *clkp;
	struct phy_list *phyp;
	struct hwrst_list *rstp;
	clk_t clk;
	phy_t phy;
	hwreset_t rst;
#endif

	sc->ohci_sc.sc_bus.parent = dev;
	sc->ohci_sc.sc_bus.devices = sc->ohci_sc.sc_devices;
	sc->ohci_sc.sc_bus.devices_max = OHCI_MAX_DEVICES;
	sc->ohci_sc.sc_bus.dma_bits = 32;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->ohci_sc.sc_bus,
	    USB_GET_DMA_TAG(dev), &ohci_iterate_hw_softc)) {
		return (ENOMEM);
	}

	rid = 0;
	sc->ohci_sc.sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->ohci_sc.sc_io_res == 0) {
		err = ENOMEM;
		goto error;
	}

	sc->ohci_sc.sc_io_tag = rman_get_bustag(sc->ohci_sc.sc_io_res);
	sc->ohci_sc.sc_io_hdl = rman_get_bushandle(sc->ohci_sc.sc_io_res);
	sc->ohci_sc.sc_io_size = rman_get_size(sc->ohci_sc.sc_io_res);

	rid = 0;
	sc->ohci_sc.sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->ohci_sc.sc_irq_res == 0) {
		err = ENXIO;
		goto error;
	}
	sc->ohci_sc.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (sc->ohci_sc.sc_bus.bdev == 0) {
		err = ENXIO;
		goto error;
	}
	device_set_ivars(sc->ohci_sc.sc_bus.bdev, &sc->ohci_sc.sc_bus);

	strlcpy(sc->ohci_sc.sc_vendor, "Generic",
	    sizeof(sc->ohci_sc.sc_vendor));

	err = bus_setup_intr(dev, sc->ohci_sc.sc_irq_res,
	    INTR_TYPE_BIO | INTR_MPSAFE, NULL,
	    (driver_intr_t *)ohci_interrupt, sc, &sc->ohci_sc.sc_intr_hdl);
	if (err) {
		sc->ohci_sc.sc_intr_hdl = NULL;
		goto error;
	}

#ifdef EXT_RESOURCES
	TAILQ_INIT(&sc->clk_list);
	/* Enable clock */
	for (off = 0; clk_get_by_ofw_index(dev, 0, off, &clk) == 0; off++) {
		err = clk_enable(clk);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			    clk_get_name(clk));
			goto error;
		}
		clkp = malloc(sizeof(*clkp), M_DEVBUF, M_WAITOK | M_ZERO);
		clkp->clk = clk;
		TAILQ_INSERT_TAIL(&sc->clk_list, clkp, next);
	}

	/* De-assert reset */
	TAILQ_INIT(&sc->rst_list);
	for (off = 0; hwreset_get_by_ofw_idx(dev, 0, off, &rst) == 0; off++) {
		err = hwreset_deassert(rst);
		if (err != 0) {
			device_printf(dev, "Could not de-assert reset\n");
			goto error;
		}
		rstp = malloc(sizeof(*rstp), M_DEVBUF, M_WAITOK | M_ZERO);
		rstp->rst = rst;
		TAILQ_INSERT_TAIL(&sc->rst_list, rstp, next);
	}

	/* Enable phy */
	TAILQ_INIT(&sc->phy_list);
	for (off = 0; phy_get_by_ofw_idx(dev, 0, off, &phy) == 0; off++) {
		err = phy_usb_set_mode(phy, PHY_USB_MODE_HOST);
		if (err != 0) {
			device_printf(dev, "Could not set phy to host mode\n");
			goto error;
		}
		err = phy_enable(phy);
		if (err != 0) {
			device_printf(dev, "Could not enable phy\n");
			goto error;
		}
		phyp = malloc(sizeof(*phyp), M_DEVBUF, M_WAITOK | M_ZERO);
		phyp->phy = phy;
		TAILQ_INSERT_TAIL(&sc->phy_list, phyp, next);
	}
#endif

	if (GENERIC_USB_INIT(dev) != 0) {
		err = ENXIO;
		goto error;
	}

	err = ohci_init(&sc->ohci_sc);
	if (err == 0)
		err = device_probe_and_attach(sc->ohci_sc.sc_bus.bdev);
	if (err)
		goto error;

	return (0);
error:
	generic_ohci_detach(dev);
	return (err);
}

static int
generic_ohci_detach(device_t dev)
{
	struct generic_ohci_softc *sc = device_get_softc(dev);
	int err;
#ifdef EXT_RESOURCES
	struct clk_list *clk, *clk_tmp;
	struct phy_list *phy, *phy_tmp;
	struct hwrst_list *rst, *rst_tmp;
#endif

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);

	/*
	 * Put the controller into reset, then disable clocks and do
	 * the MI tear down.  We have to disable the clocks/hardware
	 * after we do the rest of the teardown.  We also disable the
	 * clocks in the opposite order we acquire them, but that
	 * doesn't seem to be absolutely necessary.  We free up the
	 * clocks after we disable them, so the system could, in
	 * theory, reuse them.
	 */
	bus_space_write_4(sc->ohci_sc.sc_io_tag, sc->ohci_sc.sc_io_hdl,
	    OHCI_CONTROL, 0);

	if (sc->ohci_sc.sc_irq_res && sc->ohci_sc.sc_intr_hdl) {
		/*
		 * only call ohci_detach() after ohci_init()
		 */
		ohci_detach(&sc->ohci_sc);

		err = bus_teardown_intr(dev, sc->ohci_sc.sc_irq_res,
		    sc->ohci_sc.sc_intr_hdl);
		sc->ohci_sc.sc_intr_hdl = NULL;
	}
	if (sc->ohci_sc.sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0,
		    sc->ohci_sc.sc_irq_res);
		sc->ohci_sc.sc_irq_res = NULL;
	}
	if (sc->ohci_sc.sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0,
		    sc->ohci_sc.sc_io_res);
		sc->ohci_sc.sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->ohci_sc.sc_bus, &ohci_iterate_hw_softc);

#ifdef EXT_RESOURCES
	/* Disable phy */
	TAILQ_FOREACH_SAFE(phy, &sc->phy_list, next, phy_tmp) {
		err = phy_disable(phy->phy);
		if (err != 0)
			device_printf(dev, "Could not disable phy\n");
		phy_release(phy->phy);
		TAILQ_REMOVE(&sc->phy_list, phy, next);
		free(phy, M_DEVBUF);
	}

	/* Assert reset */
	TAILQ_FOREACH_SAFE(rst, &sc->rst_list, next, rst_tmp) {
		hwreset_assert(rst->rst);
		hwreset_release(rst->rst);
		TAILQ_REMOVE(&sc->rst_list, rst, next);
		free(rst, M_DEVBUF);
	}

	/* Disable clock */
	TAILQ_FOREACH_SAFE(clk, &sc->clk_list, next, clk_tmp) {
		err = clk_disable(clk->clk);
		if (err != 0)
			device_printf(dev, "Could not disable clock %s\n",
			    clk_get_name(clk->clk));
		err = clk_release(clk->clk);
		if (err != 0)
			device_printf(dev, "Could not release clock %s\n",
			    clk_get_name(clk->clk));
		TAILQ_REMOVE(&sc->clk_list, clk, next);
		free(clk, M_DEVBUF);
	}
#endif

	if (GENERIC_USB_DEINIT(dev) != 0)
		return (ENXIO);

	return (0);
}

static device_method_t generic_ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, generic_ohci_probe),
	DEVMETHOD(device_attach, generic_ohci_attach),
	DEVMETHOD(device_detach, generic_ohci_detach),

	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

driver_t generic_ohci_driver = {
	.name = "ohci",
	.methods = generic_ohci_methods,
	.size = sizeof(struct generic_ohci_softc),
};

static devclass_t generic_ohci_devclass;

DRIVER_MODULE(ohci, simplebus, generic_ohci_driver,
    generic_ohci_devclass, 0, 0);
MODULE_DEPEND(ohci, usb, 1, 1, 1);
