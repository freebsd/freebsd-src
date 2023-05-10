/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Semihalf.
 * Copyright (c) 2015 Stormshield.
 * All rights reserved.
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
#include <dev/usb/controller/xhci.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/phy/phy.h>

#include "generic_xhci.h"

static struct ofw_compat_data compat_data[] = {
	{"marvell,armada-380-xhci",	true},
	{"marvell,armada3700-xhci",	true},
	{"marvell,armada-8k-xhci",	true},
	{"generic-xhci",		true},
	{NULL,				false}
};

static int
generic_xhci_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, XHCI_HC_DEVSTR);

	return (BUS_PROBE_DEFAULT);
}

static int
generic_xhci_fdt_attach(device_t dev)
{
	phandle_t node;
	phy_t phy;

	node = ofw_bus_get_node(dev);
	if (phy_get_by_ofw_property(dev, node, "usb-phy", &phy) == 0)
		if (phy_enable(phy) != 0)
			device_printf(dev, "Cannot enable phy\n");

	return (generic_xhci_attach(dev));
}

static int
generic_xhci_fdt_detach(device_t dev)
{
	phandle_t node;
	phy_t phy;
	int err;

	err = generic_xhci_detach(dev);
	if (err != 0)
		return (err);

	node = ofw_bus_get_node(dev);
	if (phy_get_by_ofw_property(dev, node, "usb-phy", &phy) == 0)
		phy_release(phy);

	return (0);
}

static device_method_t xhci_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, generic_xhci_fdt_probe),
	DEVMETHOD(device_attach, generic_xhci_fdt_attach),
	DEVMETHOD(device_detach, generic_xhci_fdt_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(xhci, xhci_fdt_driver, xhci_fdt_methods,
    sizeof(struct xhci_softc), generic_xhci_driver);

DRIVER_MODULE(xhci, simplebus, xhci_fdt_driver, 0, 0);
MODULE_DEPEND(xhci, usb, 1, 1, 1);
