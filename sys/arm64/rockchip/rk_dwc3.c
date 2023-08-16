/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.Org>
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
 * Rockchip DWC3 glue
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy_usb.h>
#include <dev/extres/syscon/syscon.h>

enum rk_dwc3_type {
	RK3399 = 1,
};

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3399-dwc3",	RK3399 },
	{ NULL,				0 }
};

struct rk_dwc3_softc {
	struct simplebus_softc	sc;
	device_t		dev;
	clk_t			clk_ref;
	clk_t			clk_suspend;
	clk_t			clk_bus;
	clk_t			clk_axi_perf;
	clk_t			clk_usb3;
	clk_t			clk_grf;
	hwreset_t		rst_usb3;
	enum rk_dwc3_type	type;
};

static int
rk_dwc3_probe(device_t dev)
{
	phandle_t node;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	/* Binding says that we need a child node for the actual dwc3 controller */
	node = ofw_bus_get_node(dev);
	if (OF_child(node) <= 0)
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK3399 DWC3");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_dwc3_attach(device_t dev)
{
	struct rk_dwc3_softc *sc;
	device_t cdev;
	phandle_t node, child;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);
	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	/* Mandatory clocks */
	if (clk_get_by_ofw_name(dev, 0, "ref_clk", &sc->clk_ref) != 0) {
		device_printf(dev, "Cannot get ref_clk clock\n");
		return (ENXIO);
	}
	err = clk_enable(sc->clk_ref);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->clk_ref));
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "suspend_clk", &sc->clk_suspend) != 0) {
		device_printf(dev, "Cannot get suspend_clk clock\n");
		return (ENXIO);
	}
	err = clk_enable(sc->clk_suspend);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->clk_suspend));
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "bus_clk", &sc->clk_bus) != 0) {
		device_printf(dev, "Cannot get bus_clk clock\n");
		return (ENXIO);
	}
	err = clk_enable(sc->clk_bus);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->clk_bus));
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "grf_clk", &sc->clk_grf) == 0) {
		err = clk_enable(sc->clk_grf);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			    clk_get_name(sc->clk_grf));
			return (ENXIO);
		}
	}
	/* Optional clocks */
	if (clk_get_by_ofw_name(dev, 0, "aclk_usb3_rksoc_axi_perf", &sc->clk_axi_perf) == 0) {
		err = clk_enable(sc->clk_axi_perf);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			  clk_get_name(sc->clk_axi_perf));
			return (ENXIO);
		}
	}
	if (clk_get_by_ofw_name(dev, 0, "aclk_usb3", &sc->clk_usb3) == 0) {
		err = clk_enable(sc->clk_usb3);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			  clk_get_name(sc->clk_usb3));
			return (ENXIO);
		}
	}

	/* Put module out of reset */
	if (hwreset_get_by_ofw_name(dev, node, "usb3-otg", &sc->rst_usb3) == 0) {
		if (hwreset_deassert(sc->rst_usb3) != 0) {
			device_printf(dev, "Cannot deassert reset\n");
			return (ENXIO);
		}
	}

	simplebus_init(dev, node);
	if (simplebus_fill_ranges(node, &sc->sc) < 0) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	for (child = OF_child(node); child > 0; child = OF_peer(child)) {
		cdev = simplebus_add_device(dev, child, 0, NULL, -1, NULL);
		if (cdev != NULL)
			device_probe_and_attach(cdev);
	}

	return (bus_generic_attach(dev));
}

static device_method_t rk_dwc3_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_dwc3_probe),
	DEVMETHOD(device_attach,	rk_dwc3_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk_dwc3, rk_dwc3_driver, rk_dwc3_methods,
    sizeof(struct rk_dwc3_softc), simplebus_driver);
DRIVER_MODULE(rk_dwc3, simplebus, rk_dwc3_driver, 0, 0);
