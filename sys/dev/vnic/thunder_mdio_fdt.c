/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "thunder_mdio_var.h"

static int thunder_mdio_fdt_probe(device_t);
static int thunder_mdio_fdt_attach(device_t);

static device_method_t thunder_mdio_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		thunder_mdio_fdt_probe),
	DEVMETHOD(device_attach,	thunder_mdio_fdt_attach),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(thunder_mdio, thunder_mdio_fdt_driver, thunder_mdio_fdt_methods,
    sizeof(struct thunder_mdio_softc), thunder_mdio_driver);

static devclass_t thunder_mdio_fdt_devclass;

DRIVER_MODULE(thunder_mdio, ofwbus, thunder_mdio_fdt_driver,
    thunder_mdio_fdt_devclass, 0, 0);

static int
thunder_mdio_fdt_probe(device_t dev)
{

	if (ofw_bus_is_compatible(dev, "cavium,octeon-3860-mdio")) {
		device_set_desc(dev, THUNDER_MDIO_DEVSTR);
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
thunder_mdio_fdt_attach(device_t dev)
{
	phandle_t node;
	int ret;

	/* Call core attach */
	ret = thunder_mdio_attach(dev);
	if (ret != 0)
		return (ret);
	/*
	 * Register device to this node/xref.
	 * Thanks to that we will be able to retrieve device_t structure
	 * while holding only node reference acquired from FDT.
	 */
	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}
