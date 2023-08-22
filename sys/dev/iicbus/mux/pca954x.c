/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore <ian@freebsd.org>
 * Copyright (c) 2020-2021 Andriy Gapon
 * Copyright (c) 2022 Bjoern A. Zeeb
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

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>
#endif

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include "iicbus_if.h"
#include "iicmux_if.h"
#include <dev/iicbus/mux/iicmux.h>

enum pca954x_type {
	PCA954X_MUX,
	PCA954X_SW,
};

struct pca954x_descr {
	const char 		*partname;
	const char		*description;
	enum pca954x_type	type;
	uint8_t			numchannels;
	uint8_t			enable;
};

static struct pca954x_descr pca9540_descr = {
	.partname = "pca9540",
	.description = "PCA9540B I2C Mux",
	.type = PCA954X_MUX,
	.numchannels = 2,
	.enable = 0x04,
};

static struct pca954x_descr pca9547_descr = {
	.partname = "pca9547",
	.description = "PCA9547 I2C Mux",
	.type = PCA954X_MUX,
	.numchannels = 8,
	.enable = 0x08,
};

static struct pca954x_descr pca9548_descr = {
	.partname = "pca9548",
	.description = "PCA9548A I2C Switch",
	.type = PCA954X_SW,
	.numchannels = 8,
};

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{ "nxp,pca9540", (uintptr_t)&pca9540_descr },
	{ "nxp,pca9547", (uintptr_t)&pca9547_descr },
	{ "nxp,pca9548", (uintptr_t)&pca9548_descr },
	{ NULL, 0 },
};
#else
static struct pca954x_descr *part_descrs[] = {
	&pca9540_descr,
	&pca9547_descr,
	&pca9548_descr,
};
#endif

struct pca954x_softc {
	struct iicmux_softc mux;
	const struct pca954x_descr *descr;
	uint8_t addr;
	bool idle_disconnect;
};

static int
pca954x_bus_select(device_t dev, int busidx, struct iic_reqbus_data *rd)
{
	struct pca954x_softc *sc;
	struct iic_msg msg;
	int error;
	uint8_t busbits;

	sc = device_get_softc(dev);

	/*
	 * The iicmux caller ensures busidx is between 0 and the number of buses
	 * we passed to iicmux_init_softc(), no need for validation here.  If
	 * the fdt data has the idle_disconnect property we idle the bus by
	 * selecting no downstream buses, otherwise we just leave the current
	 * bus active.
	 */
	if (busidx == IICMUX_SELECT_IDLE) {
		if (sc->idle_disconnect)
			busbits = 0;
		else
			return (0);
	} else if (sc->descr->type == PCA954X_MUX) {
		uint8_t en;

		en = sc->descr->enable;
		KASSERT(en > 0 && powerof2(en), ("%s: %s enable %#x "
		    "invalid\n", __func__, sc->descr->partname, en));
		busbits = en | (busidx & (en - 1));
	} else if (sc->descr->type == PCA954X_SW) {
		busbits = 1u << busidx;
	} else {
		panic("%s: %s: unsupported type %d\n",
		    __func__, sc->descr->partname, sc->descr->type);
	}

	msg.slave = sc->addr;
	msg.flags = IIC_M_WR;
	msg.len = 1;
	msg.buf = &busbits;
	error = iicbus_transfer(dev, &msg, 1);
	return (error);
}

static const struct pca954x_descr *
pca954x_find_chip(device_t dev)
{
#ifdef FDT
	const struct ofw_compat_data *compat;

	if (!ofw_bus_status_okay(dev))
		return (NULL);

	compat = ofw_bus_search_compatible(dev, compat_data);
	if (compat == NULL)
		return (NULL);
	return ((const struct pca954x_descr *)compat->ocd_data);
#else
	const char *type;
	int i;

	if (resource_string_value(device_get_name(dev), device_get_unit(dev),
	    "chip_type", &type) == 0) {
		for (i = 0; i < nitems(part_descrs) - 1; ++i) {
			if (strcasecmp(type, part_descrs[i]->partname) == 0)
				return (part_descrs[i]);
		}
	}
	return (NULL);
#endif
}

static int
pca954x_probe(device_t dev)
{
	const struct pca954x_descr *descr;

	descr = pca954x_find_chip(dev);
	if (descr == NULL)
		return (ENXIO);

	device_set_desc(dev, descr->description);
	return (BUS_PROBE_DEFAULT);
}

static int
pca954x_attach(device_t dev)
{
	struct pca954x_softc *sc;
	const struct pca954x_descr *descr;
	int error;

	sc = device_get_softc(dev);
	sc->addr = iicbus_get_addr(dev);
	sc->idle_disconnect = device_has_property(dev, "i2c-mux-idle-disconnect");

	sc->descr = descr = pca954x_find_chip(dev);
	error = iicmux_attach(dev, device_get_parent(dev), descr->numchannels);
	if (error == 0)
                bus_generic_attach(dev);

	return (error);
}

static int
pca954x_detach(device_t dev)
{
	int error;

	error = iicmux_detach(dev);
	return (error);
}

static device_method_t pca954x_methods[] = {
	/* device methods */
	DEVMETHOD(device_probe,			pca954x_probe),
	DEVMETHOD(device_attach,		pca954x_attach),
	DEVMETHOD(device_detach,		pca954x_detach),

	/* iicmux methods */
	DEVMETHOD(iicmux_bus_select,		pca954x_bus_select),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pca954x, pca954x_driver, pca954x_methods,
    sizeof(struct pca954x_softc), iicmux_driver);
DRIVER_MODULE(pca954x, iicbus, pca954x_driver, 0, 0);

#ifdef FDT
DRIVER_MODULE(ofw_iicbus, pca954x, ofw_iicbus_driver, 0, 0);
#else
DRIVER_MODULE(iicbus, pca954x, iicbus_driver, 0, 0);
#endif

MODULE_DEPEND(pca954x, iicmux, 1, 1, 1);
MODULE_DEPEND(pca954x, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(pca954x, 1);

#ifdef FDT
IICBUS_FDT_PNP_INFO(compat_data);
#endif
