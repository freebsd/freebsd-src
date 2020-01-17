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
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include "iicbus_if.h"
#include "iicmux_if.h"

static struct chip_info {
	const char 	*partname;
	const char	*description;
	int		numchannels;
} chip_infos[] = {
	{"ltc4305", "LTC4305 I2C Mux", 2},
	{"ltc4306", "LTC4306 I2C Mux", 4},
};
#define CHIP_NONE	(-1)
#define	CHIP_LTC4305	0
#define	CHIP_LTC4306	1

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

static struct ofw_compat_data compat_data[] = {
	{"lltc,ltc4305",  CHIP_LTC4305},
	{"lltc,ltc4306",  CHIP_LTC4306},
	{NULL,            CHIP_NONE}
};
IICBUS_FDT_PNP_INFO(compat_data);
#endif

#include <dev/iicbus/mux/iicmux.h>

struct ltc430x_softc {
	struct iicmux_softc mux;
	bool idle_disconnect;
};

/*
 * The datasheet doesn't name the registers, it calls them control register 0-3.
 */
#define	LTC430X_CTLREG_0	0
#define	LTC430X_CTLREG_1	1
#define	LTC430X_CTLREG_2	2
#define	LTC430X_CTLREG_3	3

static int
ltc430x_bus_select(device_t dev, int busidx, struct iic_reqbus_data *rd)
{
	struct ltc430x_softc *sc = device_get_softc(dev);
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
		busbits = 1u << (7 - busidx);
	}

	/*
	 * We have to add the IIC_RECURSIVE flag because the iicmux core has
	 * already reserved the bus for us, and iicdev_writeto() is going to try
	 * to reserve it again, which is allowed with the recursive flag.
	 */
	return (iicdev_writeto(dev, LTC430X_CTLREG_3, &busbits, sizeof(busbits),
	    rd->flags | IIC_RECURSIVE));
}

static int
ltc430x_find_chiptype(device_t dev)
{
#ifdef FDT
	return (ofw_bus_search_compatible(dev, compat_data)->ocd_data);
#else
	const char *type;
	int i;

	if (resource_string_value(device_get_name(dev), device_get_unit(dev),
	    "chip_type", &type) == 0) {
		for (i = 0; i < nitems(chip_infos); ++i) {
			if (strcasecmp(type, chip_infos[i].partname) == 0) {
				return (i);
			}
		}
	}
	return (CHIP_NONE);
#endif
}

static int
ltc430x_probe(device_t dev)
{
	int type;

#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
#endif

	type = ltc430x_find_chiptype(dev);
	if (type == CHIP_NONE)
		return (ENXIO);

	device_set_desc(dev, chip_infos[type].description);

	return (BUS_PROBE_DEFAULT);
}

static int
ltc430x_attach(device_t dev)
{
	struct ltc430x_softc *sc __unused;
	int chip, err, numchan;

	sc = device_get_softc(dev);

#ifdef FDT
	phandle_t node;

	node = ofw_bus_get_node(dev);
	sc->idle_disconnect = OF_hasprop(node, "i2c-mux-idle-disconnect");
#endif

	/* We found the chip type when probing, so now it "can't fail". */
	if ((chip = ltc430x_find_chiptype(dev)) == CHIP_NONE) {
		device_printf(dev, "impossible: can't identify chip type\n");
		return (ENXIO);
	}
	numchan = chip_infos[chip].numchannels;

	if ((err = iicmux_attach(dev, device_get_parent(dev), numchan)) == 0)
                bus_generic_attach(dev);

	return (err);
}

static int
ltc430x_detach(device_t dev)
{
	int err;

	if ((err = iicmux_detach(dev)) != 0)
		return (err);

	return (0);
}

static device_method_t ltc430x_methods[] = {
	/* device methods */
	DEVMETHOD(device_probe,			ltc430x_probe),
	DEVMETHOD(device_attach,		ltc430x_attach),
	DEVMETHOD(device_detach,		ltc430x_detach),

	/* iicmux methods */
	DEVMETHOD(iicmux_bus_select,		ltc430x_bus_select),

	DEVMETHOD_END
};

static devclass_t ltc430x_devclass;

DEFINE_CLASS_1(ltc430x, ltc430x_driver, ltc430x_methods,
    sizeof(struct ltc430x_softc), iicmux_driver);
DRIVER_MODULE(ltc430x, iicbus, ltc430x_driver, ltc430x_devclass, 0, 0);

#ifdef FDT
DRIVER_MODULE(ofw_iicbus, ltc430x, ofw_iicbus_driver, ofw_iicbus_devclass, 0, 0);
#else
DRIVER_MODULE(iicbus, ltc430x, iicbus_driver, iicbus_devclass, 0, 0);
#endif

MODULE_DEPEND(ltc430x, iicmux, 1, 1, 1);
MODULE_DEPEND(ltc430x, iicbus, 1, 1, 1);

