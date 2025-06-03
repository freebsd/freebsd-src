/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore <ian@freebsd.org>
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
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "iicbus_if.h"
#include "iicmux_if.h"


static struct ofw_compat_data compat_data[] = {
	{"nxp,pca9547",		1},
	{NULL,			0}
};
IICBUS_FDT_PNP_INFO(compat_data);

#include <dev/iicbus/mux/iicmux.h>

struct pca9547_softc {
	struct iicmux_softc mux;
	device_t	dev;
	bool idle_disconnect;
};

static int
pca9547_bus_select(device_t dev, int busidx, struct iic_reqbus_data *rd)
{
	struct pca9547_softc *sc = device_get_softc(dev);
	uint8_t busbits;

	/*
	 * The iicmux caller ensures busidx is between 0 and the number of buses
	 * we passed to iicmux_init_softc(), no need for validation here.  If
	 * the fdt data has the idle_disconnect property we idle the bus by
	 * selecting no downstream buses, otherwise we just leave the current
	 * bus active.  The upper bits of control register 3 activate the
	 * downstream buses; bit 7 is the first bus, bit 6 the second, etc.
	 */
	if (busidx == IICMUX_SELECT_IDLE) {
		if (sc->idle_disconnect)
			busbits = 0;
		else
			return (0);
	} else {
		busbits = 0x8 | (busidx & 0x7);
	}

	/*
	 * We have to add the IIC_RECURSIVE flag because the iicmux core has
	 * already reserved the bus for us, and iicdev_writeto() is going to try
	 * to reserve it again, which is allowed with the recursive flag.
	 */

	return (iicdev_writeto(dev, busbits, NULL, 0,
	    rd->flags | IIC_RECURSIVE));
}

static int
pca9547_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "PCA9547 IIC bus multiplexor");
	return (BUS_PROBE_DEFAULT);
}

static int
pca9547_attach(device_t dev)
{
	struct pca9547_softc *sc;
	phandle_t node;
	int rv;

	sc = device_get_softc(dev);
	sc ->dev = dev;

	node = ofw_bus_get_node(dev);
	sc->idle_disconnect = OF_hasprop(node, "i2c-mux-idle-disconnect");

	rv = iicmux_attach(sc->dev, device_get_parent(dev), 8);
	if (rv != 0)
		return (rv);
	bus_attach_children(dev);

	return (rv);
}

static int
pca9547_detach(device_t dev)
{
	int rv;

	rv = iicmux_detach(dev);
	if (rv != 0)
		return (rv);

	return (0);
}

static device_method_t pca9547_methods[] = {
	/* device methods */
	DEVMETHOD(device_probe,			pca9547_probe),
	DEVMETHOD(device_attach,		pca9547_attach),
	DEVMETHOD(device_detach,		pca9547_detach),

	/* iicmux methods */
	DEVMETHOD(iicmux_bus_select,		pca9547_bus_select),

	DEVMETHOD_END
};

DEFINE_CLASS_1(iicmux, pca9547_driver, pca9547_methods,
    sizeof(struct pca9547_softc), iicmux_driver);
DRIVER_MODULE(pca_iicmux, iicbus, pca9547_driver, 0, 0);
DRIVER_MODULE(iicbus, iicmux, iicbus_driver, 0, 0);
DRIVER_MODULE(ofw_iicbus, iicmux, ofw_iicbus_driver, 0, 0);
MODULE_DEPEND(pca9547, iicmux, 1, 1, 1);
MODULE_DEPEND(pca9547, iicbus, 1, 1, 1);
