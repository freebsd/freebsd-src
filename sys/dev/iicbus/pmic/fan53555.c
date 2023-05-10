/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/regulator/regulator.h>

#include "regdev_if.h"

/* Registers */
#define	FAN53555_VSEL0		0x00
#define	FAN53555_VSEL1		0x01
#define	 FAN53555_VSEL_ENA		(1 << 7)
#define	 FAN53555_VSEL_MODE		(1 << 6)
#define	 FAN53555_VSEL_MASK		0x3f
#define	FAN53555_CTRL		0x02
#define	FAN53555_ID1		0x03
#define	 FAN53555_ID1_DIE_ID(x)		((x) & 0x0F)
#define	FAN53555_ID2		0x04
#define	 FAN53555_ID2_DIE_REV(x)	((x) & 0x0F)
#define	FAN53555_MON		0x05

#define	TCS4525_VSEL0		0x11
#define	TCS4525_VSEL1		0x10
#define	TCS4525_CHIP_ID_12	12

#if 0
#define	dprintf(sc, format, arg...)					\
	device_printf(sc->base_dev, "%s: " format, __func__, arg)
#else
#define	dprintf(sc, format, arg...)
#endif

enum fan53555_pmic_type {
	FAN53555 = 1,
	SYR827,
	SYR828,
	TCS4525,
};

static struct ofw_compat_data compat_data[] = {
	{"fcs,fan53555", 	FAN53555},
	{"silergy,syr827",	SYR827},
	{"silergy,syr828",	SYR828},
	{"tcs,tcs4525",		TCS4525},
	{NULL,		0}
};

struct fan53555_reg_sc {
	struct regnode		*regnode;
	char			*name;
	device_t		base_dev;
	uint8_t			live_reg;
	uint8_t			sleep_reg;
	struct regulator_range	*range;
	struct regnode_std_param *param;
};

struct fan53555_softc {
	device_t		dev;
	uint8_t			live_reg;
	uint8_t			sleep_reg;
};

static struct regulator_range syr_8_range =
   REG_RANGE_INIT(  0, 0x3F,  712500, 12500);

static struct regulator_range fan_0_0_range =
   REG_RANGE_INIT(  0, 0x3F,  600000, 10000);
static struct regulator_range fan_0_13_range =
   REG_RANGE_INIT(  0, 0x3F,  800000, 10000);
static struct regulator_range fan_1_range =
   REG_RANGE_INIT(  0, 0x3F,  600000, 10000);
static struct regulator_range fan_4_range =
   REG_RANGE_INIT(  0, 0x3F,  603000, 12826);

static struct regulator_range tcs_12_range =
   REG_RANGE_INIT(  0, 0x3F,  800000, 6250);

static int
fan53555_read(device_t dev, uint8_t reg, uint8_t *val)
{
	uint8_t addr;
	int rv;
	struct iic_msg msgs[2] = {
		{0, IIC_M_WR | IIC_M_NOSTOP, 1, &addr},
		{0, IIC_M_RD, 1, val},
	};

	msgs[0].slave = iicbus_get_addr(dev);
	msgs[1].slave = iicbus_get_addr(dev);
	addr = reg;

	rv = iicbus_transfer_excl(dev, msgs, 2, IIC_INTRWAIT);
	if (rv != 0) {
		device_printf(dev, "Error when reading reg 0x%02X, rv: %d\n",
		    reg,  rv);
		return (EIO);
	}

	return (0);
}

static int
fan53555_write(device_t dev, uint8_t reg, uint8_t val)
{
	uint8_t data[2];
	int rv;

	struct iic_msg msgs[1] = {
		{0, IIC_M_WR, 2, data},
	};

	msgs[0].slave = iicbus_get_addr(dev);
	data[0] = reg;
	data[1] = val;

	rv = iicbus_transfer_excl(dev, msgs, 1, IIC_INTRWAIT);
	if (rv != 0) {
		device_printf(dev,
		    "Error when writing reg 0x%02X, rv: %d\n", reg, rv);
		return (EIO);
	}
	return (0);
}

static int
fan53555_read_sel(struct fan53555_reg_sc *sc, uint8_t *sel)
{
	int rv;

	rv = fan53555_read(sc->base_dev, sc->live_reg, sel);
	if (rv != 0)
		return (rv);
	*sel &= FAN53555_VSEL_MASK;
	return (0);
}

static int
fan53555_write_sel(struct fan53555_reg_sc *sc, uint8_t sel)
{
	int rv;
	uint8_t reg;

	rv = fan53555_read(sc->base_dev, sc->live_reg, &reg);
	if (rv != 0)
		return (rv);
	reg &= ~FAN53555_VSEL_MASK;
	reg |= sel;

	rv = fan53555_write(sc->base_dev, sc->live_reg, reg);
	if (rv != 0)
		return (rv);
	return (rv);
}

static int
fan53555_regnode_init(struct regnode *regnode)
{
	return (0);
}

static int
fan53555_regnode_enable(struct regnode *regnode, bool enable, int *udelay)
{
	struct fan53555_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	dprintf(sc, "%sabling regulator %s\n", enable ? "En" : "Dis",
	    sc->name);
	fan53555_read(sc->base_dev, sc->live_reg, &val);
	if (enable)
		val |=FAN53555_VSEL_ENA;
	else
		val &= ~FAN53555_VSEL_ENA;
	fan53555_write(sc->base_dev, sc->live_reg, val);

	*udelay = sc->param->enable_delay;
	return (0);
}


static int
fan53555_regnode_set_voltage(struct regnode *regnode, int min_uvolt,
    int max_uvolt, int *udelay)
{
	struct fan53555_reg_sc *sc;
	uint8_t sel;
	int uvolt, rv;

	sc = regnode_get_softc(regnode);

	dprintf(sc, "Setting %s to %d<->%d uvolts\n", sc->name, min_uvolt,
	    max_uvolt);
	rv = regulator_range_volt_to_sel8(sc->range, 1, min_uvolt, max_uvolt,
	    &sel);
	if (rv != 0)
		return (rv);
	*udelay = sc->param->ramp_delay;
	rv = fan53555_write_sel(sc, sel);
	dprintf(sc, "Regulator %s writing sel: 0x%02X\n", sc->name, sel);

	fan53555_read_sel(sc, &sel);
	regulator_range_sel8_to_volt(sc->range, 1, sel, &uvolt);
	dprintf(sc, "Regulator %s set to %d uvolt (sel: 0x%02X)\n", sc->name,
	    uvolt, sel);

	return (rv);
}

static int
fan53555_regnode_get_voltage(struct regnode *regnode, int *uvolt)
{
	struct fan53555_reg_sc *sc;
	uint8_t sel;
	int rv;

	sc = regnode_get_softc(regnode);

	rv = fan53555_read_sel(sc, &sel);
	if (rv != 0)
		return (rv);
	rv = regulator_range_sel8_to_volt(sc->range, 1, sel, uvolt);
	dprintf(sc, "Regulator %s is at %d uvolt ((sel: 0x%02X)\n", sc->name,
	    *uvolt, sel);

	return (rv);
}

static regnode_method_t fan53555_regnode_methods[] = {
	/* Regulator interface */
	REGNODEMETHOD(regnode_init,		fan53555_regnode_init),
	REGNODEMETHOD(regnode_enable,		fan53555_regnode_enable),
	REGNODEMETHOD(regnode_set_voltage,	fan53555_regnode_set_voltage),
	REGNODEMETHOD(regnode_get_voltage,	fan53555_regnode_get_voltage),
	REGNODEMETHOD_END
};
DEFINE_CLASS_1(fan53555_regnode, fan53555_regnode_class,
    fan53555_regnode_methods, sizeof(struct fan53555_reg_sc), regnode_class);

static struct regulator_range *
fan53555_get_range(struct fan53555_softc *sc, int type, uint8_t id,
    uint8_t rev)
{
	if (type == SYR827 || type == SYR828) {
		switch (id) {
		case 8:
			return (&syr_8_range);
		default:
			return (NULL);
		}
	}

	if (type == FAN53555) {
		switch (id) {
		case 0:
			if (rev == 0)
				return (&fan_0_0_range);
			else if (rev == 13)
				return (&fan_0_13_range);
			else
				return (NULL);
		case 1:
		case 3:
		case 5:
		case 8:
			return (&fan_1_range);
		case 4:
			return (&fan_4_range);
		default:
			return (NULL);
		}
	}

	if (type == TCS4525) {
		switch (id) {
		case TCS4525_CHIP_ID_12:
			return (&tcs_12_range);
		default:
			return (NULL);
		}
	}

	return (NULL);
}

static struct fan53555_reg_sc *
fan53555_reg_attach(struct fan53555_softc *sc, phandle_t node, int  type)
{
	struct fan53555_reg_sc *reg_sc;
	struct regnode_init_def initdef;
	struct regnode *regnode;
	static struct regulator_range *range;
	uint8_t id1, id2;

	memset(&initdef, 0, sizeof(initdef));
	if (regulator_parse_ofw_stdparam(sc->dev, node, &initdef) != 0) {
		device_printf(sc->dev, "cannot parse regulator FDT data\n");
		return (NULL);
	}

	if (fan53555_read(sc->dev, FAN53555_ID1, &id1) != 0) {
		device_printf(sc->dev, "cannot read ID1\n");
		return (NULL);
	}

	if (fan53555_read(sc->dev, FAN53555_ID2, &id2) != 0) {
		device_printf(sc->dev, "cannot read ID2\n");
		return (NULL);
	}
	dprintf(sc, "Device ID1: 0x%02X, ID2: 0x%02X\n", id1, id2);

	range = fan53555_get_range(sc, type, FAN53555_ID1_DIE_ID(id1),
	     FAN53555_ID2_DIE_REV(id2));
	if (range == NULL) {
		device_printf(sc->dev,
		    "cannot determine chip type (ID1: 0x%02X, ID2: 0x%02X)\n",
		    id1, id2);
		return (NULL);
	}

	initdef.id = 1;
	initdef.ofw_node = node;

	regnode = regnode_create(sc->dev, &fan53555_regnode_class, &initdef);
	if (regnode == NULL) {
		device_printf(sc->dev, "cannot create regulator\n");
		return (NULL);
	}

	reg_sc = regnode_get_softc(regnode);
	reg_sc->name = "fan53555";
	reg_sc->regnode = regnode;
	reg_sc->base_dev = sc->dev;
	reg_sc->param = regnode_get_stdparam(regnode);
	reg_sc->range = range;
	reg_sc->live_reg = sc->live_reg;
	reg_sc->sleep_reg = sc->sleep_reg;

	dprintf(sc->dev, "live_reg: %d, sleep_reg: %d\n", reg_sc->live_reg,
	    reg_sc->sleep_reg);

	regnode_register(regnode);

	if (bootverbose) {
		int volt, rv;
		regnode_topo_slock();
		rv = regnode_get_voltage(regnode, &volt);
		if (rv == ENODEV) {
			device_printf(sc->dev,
			   " Regulator %s: parent doesn't exist yet.\n",
			   regnode_get_name(regnode));
		} else if (rv != 0) {
			device_printf(sc->dev,
			   " Regulator %s: voltage: INVALID!!!\n",
			   regnode_get_name(regnode));
		} else {
			device_printf(sc->dev,
			    " Regulator %s: voltage: %d uV\n",
			    regnode_get_name(regnode), volt);
		}
		regnode_topo_unlock();
	}

	return (reg_sc);
}

static int
fan53555_probe(device_t dev)
{
	int type;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	switch (type) {
	case FAN53555:
		device_set_desc(dev, "FAN53555 PMIC");
		break;
	case SYR827:
		device_set_desc(dev, "SYR827 PMIC");
		break;
	case SYR828:
		device_set_desc(dev, "SYR828 PMIC");
		break;
	case TCS4525:
		device_set_desc(dev, "TCS4525 PMIC");
		break;
	default:
		return (ENXIO);
	}

	return (BUS_PROBE_DEFAULT);
}

static int
fan53555_attach(device_t dev)
{
	struct fan53555_softc *sc;
	phandle_t node;
	int type, susp_sel, rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);
	type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	rv = OF_getencprop(node, "fcs,suspend-voltage-selector", &susp_sel,
		sizeof(susp_sel));
	if (rv <= 0)
		susp_sel = 1;

	switch (type) {
	case FAN53555:
	case SYR827:
	case SYR828:
		if (susp_sel == 1) {
			sc->live_reg = FAN53555_VSEL0;
			sc->sleep_reg = FAN53555_VSEL1;
		} else {
			sc->live_reg = FAN53555_VSEL1;
			sc->sleep_reg = FAN53555_VSEL0;
		}
		break;
	case TCS4525:
		if (susp_sel == 1) {
			sc->live_reg = TCS4525_VSEL0;
			sc->sleep_reg = TCS4525_VSEL1;
		} else {
			sc->live_reg = TCS4525_VSEL1;
			sc->sleep_reg = TCS4525_VSEL0;
		}
		break;
	default:
		return (ENXIO);
	}
	if (fan53555_reg_attach(sc, node, type) == NULL)
		device_printf(dev, "cannot attach regulator.\n");

	return (0);
}

static int
fan53555_detach(device_t dev)
{

	/* We cannot detach regulators */
	return (EBUSY);
}

static device_method_t fan53555_methods[] = {
	DEVMETHOD(device_probe,		fan53555_probe),
	DEVMETHOD(device_attach,	fan53555_attach),
	DEVMETHOD(device_detach,	fan53555_detach),

	/* Regdev interface */
	DEVMETHOD(regdev_map,		regdev_default_ofw_map),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(fan53555_pmic, fan53555_driver, fan53555_methods,
    sizeof(struct fan53555_softc));

EARLY_DRIVER_MODULE(fan53555, iicbus, fan53555_driver, 0, 0, BUS_PASS_RESOURCE);
MODULE_VERSION(fan53555, 1);
MODULE_DEPEND(fan53555, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
IICBUS_FDT_PNP_INFO(compat_data);
