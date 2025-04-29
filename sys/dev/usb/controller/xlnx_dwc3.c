/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
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
 * Xilinx DWC3 glue
 */

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

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/phy/phy_usb.h>
#include <dev/syscon/syscon.h>

static struct ofw_compat_data compat_data[] = {
	{ "xlnx,zynqmp-dwc3",	1 },
	{ NULL,			0 }
};

struct xlnx_dwc3_softc {
	struct simplebus_softc	sc;
	device_t		dev;
	hwreset_t		rst_crst;
	hwreset_t		rst_hibrst;
	hwreset_t		rst_apbrst;
};

static int
xlnx_dwc3_probe(device_t dev)
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

	device_set_desc(dev, "Xilinx ZYNQMP DWC3");
	return (BUS_PROBE_DEFAULT);
}

static int
xlnx_dwc3_attach(device_t dev)
{
	struct xlnx_dwc3_softc *sc;
	device_t cdev;
	phandle_t node, child;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	/*
	 * Put module out of reset
	 * Based on the bindings this should be mandatory to have
	 * but reality shows that they aren't always there.
	 * This is the case on the DTB in the AVnet Ultra96
	 */
	if (hwreset_get_by_ofw_name(dev, node, "usb_crst", &sc->rst_crst) == 0) {
		if (hwreset_deassert(sc->rst_crst) != 0) {
			device_printf(dev, "Cannot deassert reset\n");
			return (ENXIO);
		}
	}
	if (hwreset_get_by_ofw_name(dev, node, "usb_hibrst", &sc->rst_hibrst) == 0) {
		if (hwreset_deassert(sc->rst_hibrst) != 0) {
			device_printf(dev, "Cannot deassert reset\n");
			return (ENXIO);
		}
	}
	if (hwreset_get_by_ofw_name(dev, node, "usb_apbrst", &sc->rst_apbrst) == 0) {
		if (hwreset_deassert(sc->rst_apbrst) != 0) {
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

	bus_attach_children(dev);
	return (0);
}

static device_method_t xlnx_dwc3_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xlnx_dwc3_probe),
	DEVMETHOD(device_attach,	xlnx_dwc3_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(xlnx_dwc3, xlnx_dwc3_driver, xlnx_dwc3_methods,
    sizeof(struct xlnx_dwc3_softc), simplebus_driver);
DRIVER_MODULE(xlnx_dwc3, simplebus, xlnx_dwc3_driver, 0, 0);
