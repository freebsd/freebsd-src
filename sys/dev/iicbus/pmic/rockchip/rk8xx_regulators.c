/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018-2021 Emmanuel Vadot <manu@FreeBSD.org>
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
#include <sys/systm.h>
#include <sys/clock.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/regulator/regulator.h>

#include <dev/iicbus/pmic/rockchip/rk8xx.h>

#include "regdev_if.h"

static int rk8xx_regnode_status(struct regnode *regnode, int *status);
static int rk8xx_regnode_set_voltage(struct regnode *regnode, int min_uvolt,
    int max_uvolt, int *udelay);
static int rk8xx_regnode_get_voltage(struct regnode *regnode, int *uvolt);

/* #define	dprintf(sc, format, arg...)	device_printf(sc->base_dev, "%s: " format, __func__, arg) */
#define	dprintf(sc, format, arg...) (sc = sc)

static int
rk8xx_regnode_init(struct regnode *regnode)
{
	struct rk8xx_reg_sc *sc;
	struct regnode_std_param *param;
	int rv, udelay, uvolt, status;

	sc = regnode_get_softc(regnode);
	dprintf(sc, "Regulator %s init called\n", sc->def->name);
	param = regnode_get_stdparam(regnode);
	if (param->min_uvolt == 0)
		return (0);

	/* Check that the regulator is preset to the correct voltage */
	rv  = rk8xx_regnode_get_voltage(regnode, &uvolt);
	if (rv != 0)
		return(rv);

	if (uvolt >= param->min_uvolt && uvolt <= param->max_uvolt)
		return(0);
	/* 
	 * Set the regulator at the correct voltage if it is not enabled.
	 * Do not enable it, this is will be done either by a
	 * consumer or by regnode_set_constraint if boot_on is true
	 */
	rv = rk8xx_regnode_status(regnode, &status);
	if (rv != 0 || status == REGULATOR_STATUS_ENABLED)
		return (rv);

	rv = rk8xx_regnode_set_voltage(regnode, param->min_uvolt,
	    param->max_uvolt, &udelay);
	if (udelay != 0)
		DELAY(udelay);

	return (rv);
}

static int
rk8xx_regnode_enable(struct regnode *regnode, bool enable, int *udelay)
{
	struct rk8xx_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	dprintf(sc, "%sabling regulator %s\n",
	    enable ? "En" : "Dis",
	    sc->def->name);
	rk8xx_read(sc->base_dev, sc->def->enable_reg, &val, 1);
	if (enable)
		val |= sc->def->enable_mask;
	else
		val &= ~sc->def->enable_mask;
	rk8xx_write(sc->base_dev, sc->def->enable_reg, &val, 1);

	*udelay = 0;

	return (0);
}

static void
rk8xx_regnode_reg_to_voltage(struct rk8xx_reg_sc *sc, uint8_t val, int *uv)
{
	struct rk8xx_softc *sc1;

	sc1 = device_get_softc(sc->base_dev);
	if (sc1->type == RK809 || sc1->type == RK817) {
		if (sc->def->voltage_step2) {
			int change;

			change =
			    ((sc->def->voltage_min2 - sc->def->voltage_min) /
			    sc->def->voltage_step);
			if (val > change) {
				if (val < sc->def->voltage_nstep) {
					*uv = sc->def->voltage_min2 +
					    (val - change) *
					    sc->def->voltage_step2;
				} else
					*uv = sc->def->voltage_max2;
				return;
			}
		}
		if (val < sc->def->voltage_nstep)
			*uv = sc->def->voltage_min + val * sc->def->voltage_step;
		else
			*uv = sc->def->voltage_max;

	} else {
		if (val < sc->def->voltage_nstep)
			*uv = sc->def->voltage_min + val * sc->def->voltage_step;
		else
			*uv = sc->def->voltage_min +
			    (sc->def->voltage_nstep * sc->def->voltage_step);
	}
}

static int
rk8xx_regnode_voltage_to_reg(struct rk8xx_reg_sc *sc, int min_uvolt,
    int max_uvolt, uint8_t *val)
{
	uint8_t nval;
	int nstep, uvolt;
	struct rk8xx_softc *sc1;

	sc1 = device_get_softc(sc->base_dev);
	nval = 0;
	uvolt = sc->def->voltage_min;

	for (nstep = 0; nstep < sc->def->voltage_nstep && uvolt < min_uvolt;
	     nstep++) {
		++nval;
		if (sc1->type == RK809 || sc1->type == RK817) {
			if (sc->def->voltage_step2) {
				if (uvolt < sc->def->voltage_min2)
					uvolt += sc->def->voltage_step;
				else
					uvolt += sc->def->voltage_step2;
			} else
				uvolt += sc->def->voltage_step;
		} else
			uvolt += sc->def->voltage_step;
	}
	if (uvolt > max_uvolt)
		return (EINVAL);

	*val = nval;
	return (0);
}

static int
rk8xx_regnode_status(struct regnode *regnode, int *status)
{
	struct rk8xx_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	*status = 0;
	rk8xx_read(sc->base_dev, sc->def->enable_reg, &val, 1);
	if (val & sc->def->enable_mask)
		*status = REGULATOR_STATUS_ENABLED;

	return (0);
}

static int
rk8xx_regnode_set_voltage(struct regnode *regnode, int min_uvolt,
    int max_uvolt, int *udelay)
{
	struct rk8xx_reg_sc *sc;
	uint8_t val, old;
	int uvolt;
	struct rk8xx_softc *sc1;

	sc = regnode_get_softc(regnode);
	sc1 = device_get_softc(sc->base_dev);

	if (!sc->def->voltage_step)
		return (ENXIO);

	dprintf(sc, "Setting %s to %d<->%d uvolts\n",
	    sc->def->name,
	    min_uvolt,
	    max_uvolt);
	rk8xx_read(sc->base_dev, sc->def->voltage_reg, &val, 1);
	old = val;
	if (rk8xx_regnode_voltage_to_reg(sc, min_uvolt, max_uvolt, &val) != 0)
		return (ERANGE);

	if (sc1->type == RK809 || sc1->type == RK817)
		val |= (old &= ~sc->def->voltage_mask);

	rk8xx_write(sc->base_dev, sc->def->voltage_reg, &val, 1);

	rk8xx_read(sc->base_dev, sc->def->voltage_reg, &val, 1);

	*udelay = 0;

	rk8xx_regnode_reg_to_voltage(sc, val, &uvolt);
	dprintf(sc, "Regulator %s set to %d uvolt\n",
	  sc->def->name,
	  uvolt);

	return (0);
}

static int
rk8xx_regnode_get_voltage(struct regnode *regnode, int *uvolt)
{
	struct rk8xx_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	if (sc->def->voltage_min ==  sc->def->voltage_max) {
		*uvolt = sc->def->voltage_min;
		return (0);
	}

	if (!sc->def->voltage_step)
		return (ENXIO);

	rk8xx_read(sc->base_dev, sc->def->voltage_reg, &val, 1);
	rk8xx_regnode_reg_to_voltage(sc, val & sc->def->voltage_mask, uvolt);

	dprintf(sc, "Regulator %s is at %d uvolt\n",
	  sc->def->name,
	  *uvolt);

	return (0);
}

static regnode_method_t rk8xx_regnode_methods[] = {
	/* Regulator interface */
	REGNODEMETHOD(regnode_init,		rk8xx_regnode_init),
	REGNODEMETHOD(regnode_enable,		rk8xx_regnode_enable),
	REGNODEMETHOD(regnode_status,		rk8xx_regnode_status),
	REGNODEMETHOD(regnode_set_voltage,	rk8xx_regnode_set_voltage),
	REGNODEMETHOD(regnode_get_voltage,	rk8xx_regnode_get_voltage),
	REGNODEMETHOD(regnode_check_voltage,	regnode_method_check_voltage),
	REGNODEMETHOD_END
};
DEFINE_CLASS_1(rk8xx_regnode, rk8xx_regnode_class, rk8xx_regnode_methods,
    sizeof(struct rk8xx_reg_sc), regnode_class);

static struct rk8xx_reg_sc *
rk8xx_reg_attach(device_t dev, phandle_t node,
    struct rk8xx_regdef *def)
{
	struct rk8xx_reg_sc *reg_sc;
	struct regnode_init_def initdef;
	struct regnode *regnode;

	memset(&initdef, 0, sizeof(initdef));
	if (regulator_parse_ofw_stdparam(dev, node, &initdef) != 0) {
		device_printf(dev, "cannot create regulator\n");
		return (NULL);
	}
	if (initdef.std_param.min_uvolt == 0)
		initdef.std_param.min_uvolt = def->voltage_min;
	if (initdef.std_param.max_uvolt == 0)
		initdef.std_param.max_uvolt = def->voltage_max;
	initdef.id = def->id;
	initdef.ofw_node = node;

	regnode = regnode_create(dev, &rk8xx_regnode_class, &initdef);
	if (regnode == NULL) {
		device_printf(dev, "cannot create regulator\n");
		return (NULL);
	}

	reg_sc = regnode_get_softc(regnode);
	reg_sc->regnode = regnode;
	reg_sc->base_dev = dev;
	reg_sc->def = def;
	reg_sc->xref = OF_xref_from_node(node);
	reg_sc->param = regnode_get_stdparam(regnode);

	regnode_register(regnode);

	return (reg_sc);
}

void
rk8xx_attach_regulators(struct rk8xx_softc *sc)
{
	struct rk8xx_reg_sc *reg;
	struct reg_list *regp;
	phandle_t rnode, child;
	int i;

	TAILQ_INIT(&sc->regs);

	rnode = ofw_bus_find_child(ofw_bus_get_node(sc->dev), "regulators");
	if (rnode > 0) {
		for (i = 0; i < sc->nregs; i++) {
			child = ofw_bus_find_child(rnode,
			    sc->regdefs[i].name);
			if (child == 0)
				continue;
			if (OF_hasprop(child, "regulator-name") != 1)
				continue;
			reg = rk8xx_reg_attach(sc->dev, child, &sc->regdefs[i]);
			if (reg == NULL) {
				device_printf(sc->dev,
				    "cannot attach regulator %s\n",
				    sc->regdefs[i].name);
				continue;
			}
			regp = malloc(sizeof(*regp), M_DEVBUF, M_WAITOK | M_ZERO);
			regp->reg = reg;
			TAILQ_INSERT_TAIL(&sc->regs, regp, next);
			if (bootverbose)
				device_printf(sc->dev, "Regulator %s attached\n",
				    sc->regdefs[i].name);
		}
	}
}

int
rk8xx_map(device_t dev, phandle_t xref, int ncells,
    pcell_t *cells, intptr_t *id)
{
	struct rk8xx_softc *sc;
	struct reg_list *regp;

	sc = device_get_softc(dev);

	TAILQ_FOREACH(regp, &sc->regs, next) {
		if (regp->reg->xref == xref) {
			*id = regp->reg->def->id;
			return (0);
		}
	}

	return (ERANGE);
}
