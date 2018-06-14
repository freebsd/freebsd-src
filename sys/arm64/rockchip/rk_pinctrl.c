/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_pinctrl.h>

#include <dev/extres/syscon/syscon.h>

#include "syscon_if.h"

#include "opt_soc.h"

struct rk_pinctrl_pin_drive {
	uint32_t	value;
	uint32_t	ma;
};

struct rk_pinctrl_bank {
	uint32_t	bank_num;
	uint32_t	subbank_num;
	uint32_t	offset;
	uint32_t	nbits;
};

struct rk_pinctrl_pin_fixup {
	uint32_t	bank;
	uint32_t	subbank;
	uint32_t	pin;
	uint32_t	reg;
	uint32_t	bit;
	uint32_t	mask;
};

struct rk_pinctrl_conf {
	struct rk_pinctrl_bank		*iomux_conf;
	uint32_t			iomux_nbanks;
	struct rk_pinctrl_pin_fixup	*pin_fixup;
	uint32_t			npin_fixup;
	struct rk_pinctrl_pin_drive	*pin_drive;
	uint32_t			npin_drive;
	uint32_t			pd_offset;
	uint32_t			drive_offset;
};

struct rk_pinctrl_softc {
	struct simplebus_softc	simplebus_sc;
	device_t		dev;
	struct syscon		*grf;
	struct rk_pinctrl_conf	*conf;
};

static struct rk_pinctrl_bank rk3328_iomux_bank[] = {
	{
		.bank_num = 0,
		.subbank_num = 0,
		.offset = 0x00,
		.nbits = 2,
	},
	{
		.bank_num = 0,
		.subbank_num = 1,
		.offset = 0x04,
		.nbits = 2,
	},
	{
		.bank_num = 0,
		.subbank_num = 2,
		.offset = 0x08,
		.nbits = 2,
	},
	{
		.bank_num = 0,
		.subbank_num = 3,
		.offset = 0xc,
		.nbits = 2,
	},
	{
		.bank_num = 1,
		.subbank_num = 0,
		.offset = 0x10,
		.nbits = 2,
	},
	{
		.bank_num = 1,
		.subbank_num = 1,
		.offset = 0x14,
		.nbits = 2,
	},
	{
		.bank_num = 1,
		.subbank_num = 2,
		.offset = 0x18,
		.nbits = 2,
	},
	{
		.bank_num = 1,
		.subbank_num = 3,
		.offset = 0x1C,
		.nbits = 2,
	},
	{
		.bank_num = 2,
		.subbank_num = 0,
		.offset = 0x20,
		.nbits = 2,
	},
	{
		.bank_num = 2,
		.subbank_num = 1,
		.offset = 0x24,
		.nbits = 3,
	},
	{
		.bank_num = 2,
		.subbank_num = 2,
		.offset = 0x2c,
		.nbits = 3,
	},
	{
		.bank_num = 2,
		.subbank_num = 3,
		.offset = 0x34,
		.nbits = 2,
	},
	{
		.bank_num = 3,
		.subbank_num = 0,
		.offset = 0x38,
		.nbits = 3,
	},
	{
		.bank_num = 3,
		.subbank_num = 1,
		.offset = 0x40,
		.nbits = 3,
	},
	{
		.bank_num = 3,
		.subbank_num = 2,
		.offset = 0x48,
		.nbits = 3,
	},
	{
		.bank_num = 3,
		.subbank_num = 3,
		.offset = 0x4c,
		.nbits = 3,
	},
};

static struct rk_pinctrl_pin_fixup rk3328_pin_fixup[] = {
	{
		.bank = 2,
		.pin = 12,
		.reg = 0x24,
		.bit = 8,
		.mask = 0x300,
	},
	{
		.bank = 2,
		.pin = 15,
		.reg = 0x28,
		.bit = 0,
		.mask = 0x7,
	},
	{
		.bank = 2,
		.pin = 23,
		.reg = 0x30,
		.bit = 14,
		.mask = 0x6000,
	},
};

static struct rk_pinctrl_pin_drive rk3328_pin_drive[] = {
	{
		.value = 0,
		.ma = 2,
	},
	{
		.value = 1,
		.ma = 4,
	},
	{
		.value = 2,
		.ma = 8,
	},
	{
		.value = 3,
		.ma = 12,
	},
};

struct rk_pinctrl_conf rk3328_conf = {
	.iomux_conf = rk3328_iomux_bank,
	.iomux_nbanks = nitems(rk3328_iomux_bank),
	.pin_fixup = rk3328_pin_fixup,
	.npin_fixup = nitems(rk3328_pin_fixup),
	.pin_drive = rk3328_pin_drive,
	.npin_drive = nitems(rk3328_pin_drive),
	.pd_offset = 0x100,
	.drive_offset = 0x200,
};

static struct ofw_compat_data compat_data[] = {
#ifdef SOC_ROCKCHIP_RK3328
	{"rockchip,rk3328-pinctrl", (uintptr_t)&rk3328_conf},
#endif
	{NULL,             0}
};

static int
rk_pinctrl_parse_bias(phandle_t node)
{
	if (OF_hasprop(node, "bias-disable"))
		return (0);
	if (OF_hasprop(node, "bias-pull-up"))
		return (1);
	if (OF_hasprop(node, "bias-pull-down"))
		return (2);

	return (-1);
}

static int rk_pinctrl_parse_drive(struct rk_pinctrl_softc *sc, phandle_t node,
    uint32_t *drive)
{
	uint32_t value;
	int i;

	if (OF_getencprop(node, "drive-strength", &value,
	    sizeof(value)) != 0)
		return (-1);

	/* Map to the correct drive value */
	for (i = 0; i < sc->conf->npin_drive; i++)
		if (sc->conf->pin_drive[i].ma == value) {
			*drive = sc->conf->pin_drive[i].value;
			return (0);
		}

	return (-1);
}

static void
rk_pinctrl_get_fixup(struct rk_pinctrl_softc *sc, uint32_t bank, uint32_t pin,
    uint32_t *reg, uint32_t *mask, uint32_t *bit)
{
	int i;

	for (i = 0; i < sc->conf->npin_fixup; i++)
		if (sc->conf->pin_fixup[i].bank == bank &&
		    sc->conf->pin_fixup[i].pin == pin) {
			*reg = sc->conf->pin_fixup[i].reg;
			*mask = sc->conf->pin_fixup[i].mask;
			*bit = sc->conf->pin_fixup[i].bit;

			return;
		}
}

static void
rk_pinctrl_configure_pin(struct rk_pinctrl_softc *sc, uint32_t *pindata)
{
	phandle_t pin_conf;
	uint32_t bank, subbank, pin, function, bias;
	uint32_t bit, mask, reg, drive;
	int i;

	bank = pindata[0];
	pin = pindata[1];
	function = pindata[2];
	pin_conf = OF_node_from_xref(pindata[3]);
	subbank = pin / 8;

	for (i = 0; i < sc->conf->iomux_nbanks; i++)
		if (sc->conf->iomux_conf[i].bank_num == bank &&
		    sc->conf->iomux_conf[i].subbank_num == subbank)
			break;

	if (i == sc->conf->iomux_nbanks) {
		device_printf(sc->dev, "Unknown pin %d in bank %d\n", pin,
		    bank);
		return;
	}

	/* Parse pin function */
	reg = sc->conf->iomux_conf[i].offset;
	switch (sc->conf->iomux_conf[i].nbits) {
	case 3:
		if ((pin % 8) >= 5)
			reg += 4;
		bit = (pin % 8 % 5) * 3;
		mask = (0x7 << bit) << 16;
		break;
	case 2:
	default:
		bit = (pin % 8) * 2;
		mask = (0x3 << bit) << 16;
		break;
	}
	rk_pinctrl_get_fixup(sc, bank, pin, &reg, &mask, &bit);
	SYSCON_WRITE_4(sc->grf, reg, function << bit | mask);

	/* Pull-Up/Down */
	bias = rk_pinctrl_parse_bias(pin_conf);
	if (bias >= 0) {
		reg = sc->conf->pd_offset;

		reg += bank * 0x10 + ((pin / 8) * 0x4);
		bit = (pin % 8) * 2;
		mask = (0x3 << bit) << 16;
		SYSCON_WRITE_4(sc->grf, reg, bias << bit | mask);
	}

	/* Drive Strength */
	if (rk_pinctrl_parse_drive(sc, pin_conf, &drive) == 0) {
		reg = sc->conf->drive_offset;

		reg += bank * 0x10 + ((pin / 8) * 0x4);
		bit = (pin % 8) * 2;
		mask = (0x3 << bit) << 16;
		SYSCON_WRITE_4(sc->grf, reg, bias << bit | mask);
	}
}

static int
rk_pinctrl_configure_pins(device_t dev, phandle_t cfgxref)
{
	struct rk_pinctrl_softc *sc;
	phandle_t node;
	uint32_t *pins;
	int i, npins;

	sc = device_get_softc(dev);
	node = OF_node_from_xref(cfgxref);

	npins = OF_getencprop_alloc_multi(node, "rockchip,pins",  sizeof(*pins),
	    (void **)&pins);
	if (npins <= 0)
		return (ENOENT);

	for (i = 0; i != npins; i += 4)
		rk_pinctrl_configure_pin(sc, pins + i);

	return (0);
}

static int
rk_pinctrl_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip Pinctrl controller");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_pinctrl_attach(device_t dev)
{
	struct rk_pinctrl_softc *sc;
	phandle_t node;
	device_t cdev;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	if (OF_hasprop(node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &sc->grf) != 0) {
		device_printf(dev, "cannot get grf driver handle\n");
		return (ENXIO);
	}

	sc->conf = (struct rk_pinctrl_conf *)ofw_bus_search_compatible(dev,
	    compat_data)->ocd_data;

	fdt_pinctrl_register(dev, "rockchip,pins");
	fdt_pinctrl_configure_tree(dev);

	simplebus_init(dev, node);

	bus_generic_probe(dev);

	/* Attach child devices */
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		if (!ofw_bus_node_is_compatible(node, "rockchip,gpio-bank"))
			continue;
		cdev = simplebus_add_device(dev, node, 0, NULL, -1, NULL);
		if (cdev != NULL)
			device_probe_and_attach(cdev);
	}

	return (bus_generic_attach(dev));
}

static int
rk_pinctrl_detach(device_t dev)
{

	return (EBUSY);
}

static device_method_t rk_pinctrl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_pinctrl_probe),
	DEVMETHOD(device_attach,	rk_pinctrl_attach),
	DEVMETHOD(device_detach,	rk_pinctrl_detach),

        /* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure,rk_pinctrl_configure_pins),

	DEVMETHOD_END
};

static devclass_t rk_pinctrl_devclass;

DEFINE_CLASS_1(rk_pinctrl, rk_pinctrl_driver, rk_pinctrl_methods,
    sizeof(struct rk_pinctrl_softc), simplebus_driver);

EARLY_DRIVER_MODULE(rk_pinctrl, simplebus, rk_pinctrl_driver,
    rk_pinctrl_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(rk_pinctrl, 1);
