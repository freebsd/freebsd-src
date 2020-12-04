/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
#include <sys/systm.h>
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

#include <dev/iicbus/pmic/act8846.h>

#include "regdev_if.h"

MALLOC_DEFINE(M_ACT8846_REG, "ACT8846 regulator", "ACT8846 power regulator");

#if 0
#define	dprintf(sc, format, arg...)
	device_printf(sc->base_sc->dev, "%s: " format, __func__, arg) */
#else
#define	dprintf(sc, format, arg...)
#endif

enum act8846_reg_id {
	ACT8846_REG_ID_REG1,
	ACT8846_REG_ID_REG2,
	ACT8846_REG_ID_REG3,
	ACT8846_REG_ID_REG4,
	ACT8846_REG_ID_REG5,
	ACT8846_REG_ID_REG6,
	ACT8846_REG_ID_REG7,
	ACT8846_REG_ID_REG8,
	ACT8846_REG_ID_REG9,
	ACT8846_REG_ID_REG10,
	ACT8846_REG_ID_REG11,
	ACT8846_REG_ID_REG12,
	ACT8846_REG_ID_REG13,
};

struct act8846_regdef {
	intptr_t		id;		/* ID */
	char			*name;		/* Regulator name */
	char			*supply_name;	/* Source property name */
	uint8_t			enable_reg;
	uint8_t			enable_mask;
	uint8_t			voltage_reg;
	uint8_t			voltage_mask;
	struct regulator_range	*ranges;
	int			nranges;
};
struct act8846_softc;

struct act8846_reg_sc {
	struct regnode		*regnode;
	struct act8846_softc	*base_sc;
	struct act8846_regdef	*def;
	phandle_t		xref;
	struct regnode_std_param *param;
};


static struct regulator_range act8846_ranges[] = {
	REG_RANGE_INIT(  0, 23,  600000, 25000),
	REG_RANGE_INIT( 24, 47, 1200000, 50000),
	REG_RANGE_INIT( 48, 64, 2400000, 100000),
};

static struct act8846_regdef act8846_regdefs[] = {
	{
		.id = ACT8846_REG_ID_REG1,
		.name = "REG1",
		.supply_name = "vp1",
		.enable_reg = ACT8846_REG1_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
	},
	{
		.id = ACT8846_REG_ID_REG2,
		.name = "REG2",
		.supply_name = "vp2",
		.enable_reg = ACT8846_REG2_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG2_VSET0,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG3,
		.name = "REG3",
		.supply_name = "vp3",
		.enable_reg = ACT8846_REG3_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG3_VSET0,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG4,
		.name = "REG4",
		.supply_name = "vp4",
		.enable_reg = ACT8846_REG4_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG4_VSET0,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG5,
		.name = "REG5",
		.supply_name = "inl1",
		.enable_reg = ACT8846_REG5_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG5_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG6,
		.name = "REG6",
		.supply_name = "inl1",
		.enable_reg = ACT8846_REG6_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG6_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG7,
		.name = "REG7",
		.supply_name = "inl1",
		.enable_reg = ACT8846_REG7_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG7_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG8,
		.name = "REG8",
		.supply_name = "inl2",
		.enable_reg = ACT8846_REG8_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG8_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG9,
		.name = "REG9",
		.supply_name = "inl2",
		.enable_reg = ACT8846_REG9_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG9_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG10,
		.name = "REG10",
		.supply_name = "inl3",
		.enable_reg = ACT8846_REG10_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG10_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG11,
		.name = "REG11",
		.supply_name = "inl3",
		.enable_reg = ACT8846_REG11_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG11_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG12,
		.name = "REG12",
		.supply_name = "inl3",
		.enable_reg = ACT8846_REG12_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
		.voltage_reg = ACT8846_REG12_VSET,
		.voltage_mask = ACT8846_VSEL_MASK,
		.ranges = act8846_ranges,
		.nranges = nitems(act8846_ranges),
	},
	{
		.id = ACT8846_REG_ID_REG13,
		.name = "REG13",
		.supply_name = "inl1",
		.enable_reg = ACT8846_REG13_CTRL,
		.enable_mask = ACT8846_CTRL_ENA,
	},
};

static int
act8846_read_sel(struct act8846_reg_sc *sc, uint8_t *sel)
{
	int rv;

	rv = RD1(sc->base_sc, sc->def->voltage_reg, sel);
	if (rv != 0)
		return (rv);
	*sel &= sc->def->voltage_mask;
	*sel >>= ffs(sc->def->voltage_mask) - 1;
	return (0);
}

static int
act8846_write_sel(struct act8846_reg_sc *sc, uint8_t sel)
{
	int rv;

	sel <<= ffs(sc->def->voltage_mask) - 1;
	sel &= sc->def->voltage_mask;

	rv = RM1(sc->base_sc, sc->def->voltage_reg,
	    sc->def->voltage_mask, sel);
	if (rv != 0)
		return (rv);
	return (rv);
}

static int
act8846_regnode_init(struct regnode *regnode)
{
	return (0);
}

static int
act8846_regnode_enable(struct regnode *regnode, bool enable, int *udelay)
{
	struct act8846_reg_sc *sc;
	int rv;

	sc = regnode_get_softc(regnode);

	dprintf(sc, "%sabling regulator %s\n",
	    enable ? "En" : "Dis",
	    sc->def->name);
	rv = RM1(sc->base_sc, sc->def->enable_reg,
	    sc->def->enable_mask, enable ? sc->def->enable_mask: 0);
	*udelay = sc->param->enable_delay;

	return (rv);
}

static int
act8846_regnode_set_voltage(struct regnode *regnode, int min_uvolt,
    int max_uvolt, int *udelay)
{
	struct act8846_reg_sc *sc;
	uint8_t sel;
	int uvolt, rv;

	sc = regnode_get_softc(regnode);

	if (sc->def->ranges == NULL)
		return (ENXIO);

	dprintf(sc, "Setting %s to %d<->%d uvolts\n",
	    sc->def->name,
	    min_uvolt,
	    max_uvolt);
	rv = regulator_range_volt_to_sel8(sc->def->ranges, sc->def->nranges,
	    min_uvolt, max_uvolt, &sel);
	if (rv != 0)
		return (rv);
	*udelay = sc->param->ramp_delay;
	rv = act8846_write_sel(sc, sel);

	act8846_read_sel(sc, &sel);
	regulator_range_sel8_to_volt(sc->def->ranges, sc->def->nranges,
	    sel, &uvolt);
	dprintf(sc, "Regulator %s set to %d uvolt\n", sc->def->name,
	    uvolt);

	return (rv);
}

static int
act8846_regnode_get_voltage(struct regnode *regnode, int *uvolt)
{
	struct act8846_reg_sc *sc;
	uint8_t sel;
	int rv;

	sc = regnode_get_softc(regnode);

	if (sc->def->ranges == NULL) {
		if (sc->def->id == ACT8846_REG_ID_REG13) {
			*uvolt = 1800000;
			return (0);
		}
		return (ENXIO);
	}

	rv = act8846_read_sel(sc, &sel);
	if (rv != 0)
		return (rv);
	rv = regulator_range_sel8_to_volt(sc->def->ranges, sc->def->nranges,
	    sel, uvolt);
	dprintf(sc, "Regulator %s is at %d uvolt\n", sc->def->name,
	    *uvolt);

	return (rv);
}

static regnode_method_t act8846_regnode_methods[] = {
	/* Regulator interface */
	REGNODEMETHOD(regnode_init,		act8846_regnode_init),
	REGNODEMETHOD(regnode_enable,		act8846_regnode_enable),
	REGNODEMETHOD(regnode_set_voltage,	act8846_regnode_set_voltage),
	REGNODEMETHOD(regnode_get_voltage,	act8846_regnode_get_voltage),
	REGNODEMETHOD_END
};
DEFINE_CLASS_1(act8846_regnode, act8846_regnode_class, act8846_regnode_methods,
    sizeof(struct act8846_reg_sc), regnode_class);

static int
act8846_fdt_parse(struct act8846_softc *sc, phandle_t pnode, phandle_t node,
    struct act8846_regdef *def, struct regnode_init_def *init_def)
{
	int rv;
	phandle_t supply_node;
	char prop_name[64]; /* Maximum OFW property name length. */

	rv = regulator_parse_ofw_stdparam(sc->dev, node, init_def);

	/* Get parent supply. */
	if (def->supply_name == NULL)
		 return (0);

	snprintf(prop_name, sizeof(prop_name), "%s-supply",
	    def->supply_name);
	rv = OF_getencprop(pnode, prop_name, &supply_node,
	    sizeof(supply_node));
	if (rv <= 0)
		return (rv);
	supply_node = OF_node_from_xref(supply_node);
	rv = OF_getprop_alloc(supply_node, "regulator-name",
	    (void **)&init_def->parent_name);
	if (rv <= 0)
		init_def->parent_name = NULL;
	return (0);
}

static struct act8846_reg_sc *
act8846_attach(struct act8846_softc *sc, phandle_t pnode, phandle_t node,
    struct act8846_regdef *def)
{
	struct act8846_reg_sc *reg_sc;
	struct regnode_init_def initdef;
	struct regnode *regnode;

	memset(&initdef, 0, sizeof(initdef));
	if (act8846_fdt_parse(sc, pnode, node, def, &initdef) != 0) {
		device_printf(sc->dev, "cannot parse FDT data for regulator\n");
		return (NULL);
	}
	initdef.id = def->id;
	initdef.ofw_node = node;

	regnode = regnode_create(sc->dev, &act8846_regnode_class, &initdef);
	if (regnode == NULL) {
		device_printf(sc->dev, "cannot create regulator\n");
		return (NULL);
	}

	reg_sc = regnode_get_softc(regnode);
	reg_sc->base_sc = sc;
	reg_sc->def = def;
	reg_sc->xref = OF_xref_from_node(node);
	reg_sc->param = regnode_get_stdparam(regnode);

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


int
act8846_regulator_attach(struct act8846_softc *sc, phandle_t node)
{
	struct act8846_reg_sc *reg;
	phandle_t child, rnode;
	int i;

	rnode = ofw_bus_find_child(node, "regulators");
	if (rnode <= 0) {
		device_printf(sc->dev, " Cannot find regulators subnode\n");
		return (ENXIO);
	}

	/* ACT8846 specific definitio. */
	sc->nregs = nitems(act8846_regdefs);
	sc->regs = malloc(sizeof(struct act8846_reg_sc *) * sc->nregs,
	    M_ACT8846_REG, M_WAITOK | M_ZERO);


	/* Attach all known regulators if exist in DT. */
	for (i = 0; i < sc->nregs; i++) {
		child = ofw_bus_find_child(rnode, act8846_regdefs[i].name);
		if (child == 0) {
			if (bootverbose)
				device_printf(sc->dev,
				    "Regulator %s missing in DT\n",
				    act8846_regdefs[i].name);
			continue;
		}
		reg = act8846_attach(sc, node, child, act8846_regdefs + i);
		if (reg == NULL) {
			device_printf(sc->dev, "Cannot attach regulator: %s\n",
			    act8846_regdefs[i].name);
			return (ENXIO);
		}
		sc->regs[i] = reg;
	}
	return (0);
}

int
act8846_regulator_map(device_t dev, phandle_t xref, int ncells,
    pcell_t *cells, int *num)
{
	struct act8846_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->nregs; i++) {
		if (sc->regs[i] == NULL)
			continue;
		if (sc->regs[i]->xref == xref) {
			*num = sc->regs[i]->def->id;
			return (0);
		}
	}
	return (ENXIO);
}
