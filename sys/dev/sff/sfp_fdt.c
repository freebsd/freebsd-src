/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2023 Dmitry Salychev
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
 * Small Form Factor (SFF) Committee Pluggable (SFP) Transceiver (FDT-based).
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/fdt/simplebus.h>

#include "sff_if.h"

struct sfp_fdt_softc {
	phandle_t	ofw_node;
	phandle_t	i2c_bus;

	phandle_t	mod_def;
	phandle_t	los;
	phandle_t	tx_fault;
	phandle_t	tx_disable;
	phandle_t	rx_rate;
	phandle_t	tx_rate;
	uint32_t	max_power; /* in mW */
};

static int
sfp_fdt_probe(device_t dev)
{
	phandle_t node;
	ssize_t s;

	node = ofw_bus_get_node(dev);
	if (!ofw_bus_node_is_compatible(node, "sff,sfp"))
		return (ENXIO);

	s = device_get_property(dev, "i2c-bus", &node, sizeof(node),
	    DEVICE_PROP_HANDLE);
	if (s == -1) {
		device_printf(dev, "%s: '%s' has no 'i2c-bus' property, s %zd\n",
		    __func__, ofw_bus_get_name(dev), s);
		return (ENXIO);
	}

	device_set_desc(dev, "Small Form-factor Pluggable Transceiver");
	return (BUS_PROBE_DEFAULT);
}

static int
sfp_fdt_attach(device_t dev)
{
	struct sfp_fdt_softc *sc;
	ssize_t s;
	int error;

	sc = device_get_softc(dev);
	sc->ofw_node = ofw_bus_get_node(dev);

	s = device_get_property(dev, "i2c-bus", &sc->i2c_bus,
	    sizeof(sc->i2c_bus), DEVICE_PROP_HANDLE);
	if (s == -1) {
		device_printf(dev, "%s: cannot find 'i2c-bus' property: %zd\n",
		    __func__, s);
		return (ENXIO);
	}

	/* Optional properties */
	(void)device_get_property(dev, "mod-def0-gpios", &sc->mod_def,
	    sizeof(sc->mod_def), DEVICE_PROP_HANDLE);
	(void)device_get_property(dev, "los-gpios", &sc->los, sizeof(sc->los),
	    DEVICE_PROP_HANDLE);
	(void)device_get_property(dev, "tx-fault-gpios", &sc->tx_fault,
	    sizeof(sc->tx_fault), DEVICE_PROP_HANDLE);
	(void)device_get_property(dev, "tx-disable-gpios", &sc->tx_disable,
	    sizeof(sc->tx_disable), DEVICE_PROP_HANDLE);
	(void)device_get_property(dev, "rate-select0-gpios", &sc->rx_rate,
	    sizeof(sc->rx_rate), DEVICE_PROP_HANDLE);
	(void)device_get_property(dev, "rate-select1-gpios", &sc->tx_rate,
	    sizeof(sc->tx_rate), DEVICE_PROP_HANDLE);
	(void)device_get_property(dev, "maximum-power-milliwatt", &sc->max_power,
	    sizeof(sc->max_power), DEVICE_PROP_UINT32);

	error = OF_device_register_xref(OF_xref_from_node(sc->ofw_node), dev);
	if (error != 0)
		device_printf(dev, "%s: failed to register xref %#x\n",
		    __func__, OF_xref_from_node(sc->ofw_node));

	return (error);
}

static int
sfp_fdt_get_i2c_bus(device_t dev, device_t *i2c_bus)
{
	struct sfp_fdt_softc *sc;
	device_t xdev;

	KASSERT(i2c_bus != NULL, ("%s: i2c_bus is NULL", __func__));

	sc = device_get_softc(dev);
	xdev = OF_device_from_xref(OF_xref_from_node(sc->i2c_bus));
	if (xdev == NULL)
		return (ENXIO);

	*i2c_bus = xdev;
	return (0);
}

static device_method_t sfp_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sfp_fdt_probe),
	DEVMETHOD(device_attach,	sfp_fdt_attach),

	/* SFF */
	DEVMETHOD(sff_get_i2c_bus,	sfp_fdt_get_i2c_bus),

	DEVMETHOD_END
};

DEFINE_CLASS_0(sfp_fdt, sfp_fdt_driver, sfp_fdt_methods,
    sizeof(struct sfp_fdt_softc));

EARLY_DRIVER_MODULE(sfp_fdt, simplebus, sfp_fdt_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);
EARLY_DRIVER_MODULE(sfp_fdt, ofwbus, sfp_fdt_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);
