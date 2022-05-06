/*-
 * Copyright (c) 2012 Ganbold Tsagaankhuu <ganbold@freebsd.org>
 * Copyright (c) 2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/ehci.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy.h>
#include <dev/extres/phy/phy_usb.h>

#include "generic_ehci.h"

struct clk_list {
	TAILQ_ENTRY(clk_list)	next;
	clk_t			clk;
};

struct hwrst_list {
	TAILQ_ENTRY(hwrst_list)	next;
	hwreset_t		rst;
};

struct phy_list {
	TAILQ_ENTRY(phy_list)	next;
	phy_t			phy;
};

struct generic_ehci_fdt_softc {
	ehci_softc_t	ehci_sc;

	TAILQ_HEAD(, clk_list)	clk_list;
	TAILQ_HEAD(, hwrst_list)	rst_list;
	TAILQ_HEAD(, phy_list)		phy_list;
};

static device_probe_t generic_ehci_fdt_probe;
static device_attach_t generic_ehci_fdt_attach;
static device_detach_t generic_ehci_fdt_detach;

static int
generic_ehci_fdt_probe(device_t self)
{

	if (!ofw_bus_status_okay(self))
		return (ENXIO);

	if (!ofw_bus_is_compatible(self, "generic-ehci"))
		return (ENXIO);

	device_set_desc(self, "Generic EHCI Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
generic_ehci_fdt_attach(device_t dev)
{
	int err;
	struct generic_ehci_fdt_softc *sc;
	struct clk_list *clkp;
	clk_t clk;
	struct hwrst_list *rstp;
	hwreset_t rst;
	struct phy_list *phyp;
	phy_t phy;
	int off;

	sc = device_get_softc(dev);

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

	/* Enable USB PHY */
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

	err = generic_ehci_attach(dev);
	if (err != 0)
		goto error;

	return (0);

error:
	generic_ehci_fdt_detach(dev);
	return (err);
}

static int
generic_ehci_fdt_detach(device_t dev)
{
	struct generic_ehci_fdt_softc *sc;
	struct clk_list *clk, *clk_tmp;
	struct hwrst_list *rst, *rst_tmp;
	struct phy_list *phy, *phy_tmp;
	int err;

	err = generic_ehci_detach(dev);
	if (err != 0)
		return (err);

	sc = device_get_softc(dev);

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

	/* Assert reset */
	TAILQ_FOREACH_SAFE(rst, &sc->rst_list, next, rst_tmp) {
		hwreset_assert(rst->rst);
		hwreset_release(rst->rst);
		TAILQ_REMOVE(&sc->rst_list, rst, next);
		free(rst, M_DEVBUF);
	}

	/* Disable phys */
	TAILQ_FOREACH_SAFE(phy, &sc->phy_list, next, phy_tmp) {
		err = phy_disable(phy->phy);
		if (err != 0)
			device_printf(dev, "Could not disable phy\n");
		phy_release(phy->phy);
		TAILQ_REMOVE(&sc->phy_list, phy, next);
		free(phy, M_DEVBUF);
	}

	return (0);
}

static device_method_t ehci_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		generic_ehci_fdt_probe),
	DEVMETHOD(device_attach,	generic_ehci_fdt_attach),
	DEVMETHOD(device_detach,	generic_ehci_fdt_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ehci, ehci_fdt_driver, ehci_fdt_methods,
    sizeof(ehci_softc_t), generic_ehci_driver);

DRIVER_MODULE(generic_ehci, simplebus, ehci_fdt_driver, 0, 0);
MODULE_DEPEND(generic_ehci, usb, 1, 1, 1);
