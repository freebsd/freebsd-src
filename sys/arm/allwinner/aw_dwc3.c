/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");

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

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun50i-h6-dwc3",	1 },
	{ NULL,				0 }
};

struct aw_dwc3_softc {
	struct simplebus_softc	sc;
	device_t		dev;
	clk_t			clk_bus;
	hwreset_t		rst_bus;
};

static int
aw_dwc3_probe(device_t dev)
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

	device_set_desc(dev, "Allwinner H6 DWC3");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_dwc3_attach(device_t dev)
{
	struct aw_dwc3_softc *sc;
	device_t cdev;
	phandle_t node, child;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	/* Enable the clocks */
	if (clk_get_by_ofw_name(dev, 0, "bus", &sc->clk_bus) != 0) {
		device_printf(dev, "Cannot get bus clock\n");
		return (ENXIO);
	}
	err = clk_enable(sc->clk_bus);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->clk_bus));
		return (ENXIO);
	}

	/* Put module out of reset */
	if (hwreset_get_by_ofw_name(dev, node, "bus", &sc->rst_bus) == 0) {
		if (hwreset_deassert(sc->rst_bus) != 0) {
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

static device_method_t aw_dwc3_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_dwc3_probe),
	DEVMETHOD(device_attach,	aw_dwc3_attach),

	DEVMETHOD_END
};

static devclass_t aw_dwc3_devclass;

DEFINE_CLASS_1(aw_dwc3, aw_dwc3_driver, aw_dwc3_methods,
    sizeof(struct aw_dwc3_softc), simplebus_driver);
DRIVER_MODULE(aw_dwc3, simplebus, aw_dwc3_driver, aw_dwc3_devclass, 0, 0);
