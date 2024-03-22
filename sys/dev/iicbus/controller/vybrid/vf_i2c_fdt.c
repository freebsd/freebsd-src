/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2024 Pierre-Luc Drouin <pldrouin@pldrouin.net>
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
 * Vybrid Family Inter-Integrated Circuit (I2C)
 * Chapter 48, Vybrid Reference Manual, Rev. 5, 07/2013
 *
 * The current implementation is based on the original driver by Ruslan Bukin,
 * later modified by Dawid Górecki, and split into FDT and ACPI drivers by Val
 * Packett.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/iicbus.h>

#include <dev/iicbus/controller/vybrid/vf_i2c.h>

static const struct ofw_compat_data vf_i2c_compat_data[] = {
	{"fsl,mvf600-i2c",    HW_MVF600},
	{"fsl,vf610-i2c",     HW_VF610},
	{NULL,                0}
};

static int
vf_i2c_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, vf_i2c_compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, VF_I2C_DEVSTR);
	return (BUS_PROBE_DEFAULT);
}

static int
vf_i2c_fdt_attach(device_t dev)
{
	struct vf_i2c_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->hwtype = ofw_bus_search_compatible(dev, vf_i2c_compat_data)->ocd_data;
	node = ofw_bus_get_node(dev);

	error = clk_get_by_ofw_index(dev, node, 0, &sc->clock);
	if (error != 0) {
		sc->freq = 0;
		device_printf(dev, "Parent clock not found.\n");
	} else {
		if (OF_hasprop(node, "clock-frequency"))
			OF_getencprop(node, "clock-frequency", &sc->freq,
					sizeof(sc->freq));
		else
			sc->freq = VF_I2C_DEFAULT_BUS_SPEED;
	}
	return (vf_i2c_attach_common(dev));
}

static phandle_t
vf_i2c_get_node(device_t bus, device_t dev)
{
	return (ofw_bus_get_node(bus));
}

static device_method_t vf_i2c_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,                 vf_i2c_fdt_probe),
	DEVMETHOD(device_attach,                vf_i2c_fdt_attach),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,             vf_i2c_get_node),
	DEVMETHOD_END
};

DEFINE_CLASS_1(vf_i2c_fdt, vf_i2c_fdt_driver, vf_i2c_fdt_methods,
		sizeof(struct vf_i2c_softc), vf_i2c_driver);

DRIVER_MODULE(vf_i2c_fdt, simplebus, vf_i2c_fdt_driver, 0, 0);
DRIVER_MODULE(iicbus, vf_i2c_fdt, iicbus_driver, 0, 0);
DRIVER_MODULE(ofw_iicbus, vf_i2c_fdt, ofw_iicbus_driver, 0, 0);

