/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/sx.h>

#include <machine/bus.h>

#include <dev/extres/regulator/regulator.h>
#include <dev/gpio/gpiobusvar.h>

#include <dt-bindings/mfd/max77620.h>

#include "max77620.h"

MALLOC_DEFINE(M_MAX77620_REG, "MAX77620 regulator", "MAX77620 power regulator");

#define	DIV_ROUND_UP(n,d) howmany(n, d)

enum max77620_reg_id {
	MAX77620_REG_ID_SD0,
	MAX77620_REG_ID_SD1,
	MAX77620_REG_ID_SD2,
	MAX77620_REG_ID_SD3,
	MAX77620_REG_ID_LDO0,
	MAX77620_REG_ID_LDO1,
	MAX77620_REG_ID_LDO2,
	MAX77620_REG_ID_LDO3,
	MAX77620_REG_ID_LDO4,
	MAX77620_REG_ID_LDO5,
	MAX77620_REG_ID_LDO6,
	MAX77620_REG_ID_LDO7,
	MAX77620_REG_ID_LDO8,
};

/* Initial configuration. */
struct max77620_regnode_init_def {
	struct regnode_init_def	reg_init_def;
	int active_fps_src;
	int active_fps_pu_slot;
	int active_fps_pd_slot;
	int suspend_fps_src;
	int suspend_fps_pu_slot;
	int suspend_fps_pd_slot;
	int ramp_rate_setting;
};

/* Regulator HW definition. */
struct reg_def {
	intptr_t		id;		/* ID */
	char			*name;		/* Regulator name */
	char			*supply_name;	/* Source property name */
	bool 			is_sd_reg; 	/* SD or LDO regulator? */
	uint8_t			volt_reg;
	uint8_t			volt_vsel_mask;
	uint8_t			cfg_reg;
	uint8_t			fps_reg;
	uint8_t			pwr_mode_reg;
	uint8_t			pwr_mode_mask;
	uint8_t			pwr_mode_shift;
	struct regulator_range	*ranges;
	int			nranges;
};

struct max77620_reg_sc {
	struct regnode		*regnode;
	struct max77620_softc	*base_sc;
	struct reg_def		*def;
	phandle_t		xref;

	struct regnode_std_param *param;
	/* Configured values */
	int			active_fps_src;
	int			active_fps_pu_slot;
	int			active_fps_pd_slot;
	int			suspend_fps_src;
	int			suspend_fps_pu_slot;
	int			suspend_fps_pd_slot;
	int			ramp_rate_setting;
	int			enable_usec;
	uint8_t			enable_pwr_mode;

	/* Cached values */
	uint8_t			fps_src;
	uint8_t			pwr_mode;
	int			pwr_ramp_delay;
};

static struct regulator_range max77620_sd0_ranges[] = {
	REG_RANGE_INIT(0, 64, 600000, 12500),  /* 0.6V - 1.4V / 12.5mV */
};

static struct regulator_range max77620_sd1_ranges[] = {
	REG_RANGE_INIT(0, 76, 600000, 12500),  /* 0.6V - 1.55V / 12.5mV */
};

static struct regulator_range max77620_sdx_ranges[] = {
	REG_RANGE_INIT(0, 255, 600000, 12500),  /* 0.6V - 3.7875V / 12.5mV */
};

static struct regulator_range max77620_ldo0_1_ranges[] = {
	REG_RANGE_INIT(0, 63, 800000, 25000),  /* 0.8V - 2.375V / 25mV */
};

static struct regulator_range max77620_ldo4_ranges[] = {
	REG_RANGE_INIT(0, 63, 800000, 12500),  /* 0.8V - 1.5875V / 12.5mV */
};

static struct regulator_range max77620_ldox_ranges[] = {
	REG_RANGE_INIT(0, 63, 800000, 50000),  /* 0.8V - 3.95V / 50mV */
};

static struct reg_def max77620s_def[] = {
	{
		.id = MAX77620_REG_ID_SD0,
		.name = "sd0",
		.supply_name = "in-sd0",
		.is_sd_reg = true,
		.volt_reg = MAX77620_REG_SD0,
		.volt_vsel_mask = MAX77620_SD0_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG_SD0,
		.fps_reg = MAX77620_REG_FPS_SD0,
		.pwr_mode_reg = MAX77620_REG_CFG_SD0,
		.pwr_mode_mask = MAX77620_SD_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_SD_POWER_MODE_SHIFT,
		.ranges = max77620_sd0_ranges,
		.nranges = nitems(max77620_sd0_ranges),
	},
	{
		.id = MAX77620_REG_ID_SD1,
		.name = "sd1",
		.supply_name = "in-sd1",
		.is_sd_reg = true,
		.volt_reg = MAX77620_REG_SD1,
		.volt_vsel_mask = MAX77620_SD1_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG_SD1,
		.fps_reg = MAX77620_REG_FPS_SD1,
		.pwr_mode_reg = MAX77620_REG_CFG_SD1,
		.pwr_mode_mask = MAX77620_SD_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_SD_POWER_MODE_SHIFT,
		.ranges = max77620_sd1_ranges,
		.nranges = nitems(max77620_sd1_ranges),
	},
	{
		.id = MAX77620_REG_ID_SD2,
		.name = "sd2",
		.supply_name = "in-sd2",
		.is_sd_reg = true,
		.volt_reg = MAX77620_REG_SD2,
		.volt_vsel_mask = MAX77620_SDX_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG_SD2,
		.fps_reg = MAX77620_REG_FPS_SD2,
		.pwr_mode_reg = MAX77620_REG_CFG_SD2,
		.pwr_mode_mask = MAX77620_SD_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_SD_POWER_MODE_SHIFT,
		.ranges = max77620_sdx_ranges,
		.nranges = nitems(max77620_sdx_ranges),
	},
	{
		.id = MAX77620_REG_ID_SD3,
		.name = "sd3",
		.supply_name = "in-sd3",
		.is_sd_reg = true,
		.volt_reg = MAX77620_REG_SD3,
		.volt_vsel_mask = MAX77620_SDX_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG_SD3,
		.fps_reg = MAX77620_REG_FPS_SD3,
		.pwr_mode_reg = MAX77620_REG_CFG_SD3,
		.pwr_mode_mask = MAX77620_SD_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_SD_POWER_MODE_SHIFT,
		.ranges = max77620_sdx_ranges,
		.nranges = nitems(max77620_sdx_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO0,
		.name = "ldo0",
		.supply_name = "vin-ldo0-1",
		.volt_reg = MAX77620_REG_CFG_LDO0,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.is_sd_reg = false,
		.cfg_reg = MAX77620_REG_CFG2_LDO0,
		.fps_reg = MAX77620_REG_FPS_LDO0,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO0,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldo0_1_ranges,
		.nranges = nitems(max77620_ldo0_1_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO1,
		.name = "ldo1",
		.supply_name = "in-ldo0-1",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO1,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO1,
		.fps_reg = MAX77620_REG_FPS_LDO1,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO1,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldo0_1_ranges,
		.nranges = nitems(max77620_ldo0_1_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO2,
		.name = "ldo2",
		.supply_name = "in-ldo2",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO2,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO2,
		.fps_reg = MAX77620_REG_FPS_LDO2,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO2,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldox_ranges,
		.nranges = nitems(max77620_ldox_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO3,
		.name = "ldo3",
		.supply_name = "in-ldo3-5",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO3,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO3,
		.fps_reg = MAX77620_REG_FPS_LDO3,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO3,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldox_ranges,
		.nranges = nitems(max77620_ldox_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO4,
		.name = "ldo4",
		.supply_name = "in-ldo4-6",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO4,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO4,
		.fps_reg = MAX77620_REG_FPS_LDO4,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO4,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldo4_ranges,
		.nranges = nitems(max77620_ldo4_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO5,
		.name = "ldo5",
		.supply_name = "in-ldo3-5",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO5,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO5,
		.fps_reg = MAX77620_REG_FPS_LDO5,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO5,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldox_ranges,
		.nranges = nitems(max77620_ldox_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO6,
		.name = "ldo6",
		.supply_name = "in-ldo4-6",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO6,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO6,
		.fps_reg = MAX77620_REG_FPS_LDO6,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO6,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldox_ranges,
		.nranges = nitems(max77620_ldox_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO7,
		.name = "ldo7",
		.supply_name = "in-ldo7-8",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO7,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO7,
		.fps_reg = MAX77620_REG_FPS_LDO7,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO7,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldox_ranges,
		.nranges = nitems(max77620_ldox_ranges),
	},
	{
		.id = MAX77620_REG_ID_LDO8,
		.name = "ldo8",
		.supply_name = "in-ldo7-8",
		.is_sd_reg = false,
		.volt_reg = MAX77620_REG_CFG_LDO8,
		.volt_vsel_mask = MAX77620_LDO_VSEL_MASK,
		.cfg_reg = MAX77620_REG_CFG2_LDO8,
		.fps_reg = MAX77620_REG_FPS_LDO8,
		.pwr_mode_reg = MAX77620_REG_CFG_LDO8,
		.pwr_mode_mask = MAX77620_LDO_POWER_MODE_MASK,
		.pwr_mode_shift = MAX77620_LDO_POWER_MODE_SHIFT,
		.ranges = max77620_ldox_ranges,
		.nranges = nitems(max77620_ldox_ranges),
	},
};


static int max77620_regnode_init(struct regnode *regnode);
static int max77620_regnode_enable(struct regnode *regnode, bool enable,
    int *udelay);
static int max77620_regnode_set_volt(struct regnode *regnode, int min_uvolt,
    int max_uvolt, int *udelay);
static int max77620_regnode_get_volt(struct regnode *regnode, int *uvolt);
static regnode_method_t max77620_regnode_methods[] = {
	/* Regulator interface */
	REGNODEMETHOD(regnode_init,		max77620_regnode_init),
	REGNODEMETHOD(regnode_enable,		max77620_regnode_enable),
	REGNODEMETHOD(regnode_set_voltage,	max77620_regnode_set_volt),
	REGNODEMETHOD(regnode_get_voltage,	max77620_regnode_get_volt),
	REGNODEMETHOD_END
};
DEFINE_CLASS_1(max77620_regnode, max77620_regnode_class, max77620_regnode_methods,
   sizeof(struct max77620_reg_sc), regnode_class);

static int
max77620_get_sel(struct max77620_reg_sc *sc, uint8_t *sel)
{
	int rv;

	rv = RD1(sc->base_sc, sc->def->volt_reg, sel);
	if (rv != 0) {
		printf("%s: cannot read volatge selector: %d\n",
		    regnode_get_name(sc->regnode), rv);
		return (rv);
	}
	*sel &= sc->def->volt_vsel_mask;
	*sel >>= ffs(sc->def->volt_vsel_mask) - 1;
	return (0);
}

static int
max77620_set_sel(struct max77620_reg_sc *sc, uint8_t sel)
{
	int rv;

	sel <<= ffs(sc->def->volt_vsel_mask) - 1;
	sel &= sc->def->volt_vsel_mask;

	rv = RM1(sc->base_sc, sc->def->volt_reg,
	    sc->def->volt_vsel_mask, sel);
	if (rv != 0) {
		printf("%s: cannot set volatge selector: %d\n",
		    regnode_get_name(sc->regnode), rv);
		return (rv);
	}
	return (rv);
}

static int
max77620_get_fps_src(struct max77620_reg_sc *sc, uint8_t *fps_src)
{
	uint8_t val;
	int rv;

	rv = RD1(sc->base_sc, sc->def->fps_reg, &val);
	if (rv != 0)
		return (rv);

	*fps_src  = (val & MAX77620_FPS_SRC_MASK) >> MAX77620_FPS_SRC_SHIFT;
	return (0);
}

static int
max77620_set_fps_src(struct max77620_reg_sc *sc, uint8_t fps_src)
{
	int rv;

	rv = RM1(sc->base_sc, sc->def->fps_reg, MAX77620_FPS_SRC_MASK,
	    fps_src << MAX77620_FPS_SRC_SHIFT);
	if (rv != 0)
		return (rv);
	sc->fps_src = fps_src;
	return (0);
}

static int
max77620_set_fps_slots(struct max77620_reg_sc *sc, bool suspend)
{
	uint8_t mask, val;
	int pu_slot, pd_slot, rv;

	if (suspend) {
		pu_slot = sc->suspend_fps_pu_slot;
		pd_slot = sc->suspend_fps_pd_slot;
	} else {
		pu_slot = sc->active_fps_pu_slot;
		pd_slot = sc->active_fps_pd_slot;
	}

	mask = 0;
	val = 0;
	if (pu_slot >= 0) {
		mask |= MAX77620_FPS_PU_PERIOD_MASK;
		val |= ((uint8_t)pu_slot << MAX77620_FPS_PU_PERIOD_SHIFT) &
		    MAX77620_FPS_PU_PERIOD_MASK;
	}
	if (pd_slot >= 0) {
		mask |= MAX77620_FPS_PD_PERIOD_MASK;
		val |= ((uint8_t)pd_slot << MAX77620_FPS_PD_PERIOD_SHIFT) &
		    MAX77620_FPS_PD_PERIOD_MASK;
	}

	rv = RM1(sc->base_sc, sc->def->fps_reg, mask, val);
	if (rv != 0)
		return (rv);
	return (0);
}

static int
max77620_get_pwr_mode(struct max77620_reg_sc *sc, uint8_t *pwr_mode)
{
	uint8_t val;
	int rv;

	rv = RD1(sc->base_sc, sc->def->pwr_mode_reg, &val);
	if (rv != 0)
		return (rv);

	*pwr_mode  = (val & sc->def->pwr_mode_mask) >> sc->def->pwr_mode_shift;
	return (0);
}

static int
max77620_set_pwr_mode(struct max77620_reg_sc *sc, uint8_t pwr_mode)
{
	int rv;

	rv = RM1(sc->base_sc, sc->def->pwr_mode_reg, sc->def->pwr_mode_shift,
	    pwr_mode << sc->def->pwr_mode_shift);
	if (rv != 0)
		return (rv);
	sc->pwr_mode = pwr_mode;
	return (0);
}

static int
max77620_get_pwr_ramp_delay(struct max77620_reg_sc *sc, int *rate)
{
	uint8_t val;
	int rv;

	rv = RD1(sc->base_sc, sc->def->cfg_reg, &val);
	if (rv != 0)
		return (rv);

	if (sc->def->is_sd_reg) {
		val = (val & MAX77620_SD_SR_MASK) >> MAX77620_SD_SR_SHIFT;
		if (val == 0)
			*rate = 13750;
		else if (val == 1)
			*rate = 27500;
		else if (val == 2)
			*rate = 55000;
		else
			*rate = 100000;
	} else {
		val = (val & MAX77620_LDO_SLEW_RATE_MASK) >>
		    MAX77620_LDO_SLEW_RATE_SHIFT;
		if (val == 0)
			*rate = 100000;
		else
			*rate = 5000;
	}
	sc->pwr_ramp_delay = *rate;
	return (0);
}

static int
max77620_set_pwr_ramp_delay(struct max77620_reg_sc *sc, int rate)
{
	uint8_t val, mask;
	int rv;

	if (sc->def->is_sd_reg) {
		if (rate <= 13750)
			val = 0;
		else if (rate <= 27500)
			val = 1;
		else if (rate <= 55000)
			val = 2;
		else
			val = 3;
		val <<= MAX77620_SD_SR_SHIFT;
		mask = MAX77620_SD_SR_MASK;
	} else {
		if (rate <= 5000)
			val = 1;
		else
			val = 0;
		val <<= MAX77620_LDO_SLEW_RATE_SHIFT;
		mask = MAX77620_LDO_SLEW_RATE_MASK;
	}
	rv = RM1(sc->base_sc, sc->def->cfg_reg, mask, val);
	if (rv != 0)
		return (rv);
	return (0);
}

static int
max77620_regnode_init(struct regnode *regnode)
{
	struct max77620_reg_sc *sc;
	uint8_t val;
	int intval, rv;

	sc = regnode_get_softc(regnode);
	sc->enable_usec = 500;
	sc->enable_pwr_mode = MAX77620_POWER_MODE_NORMAL;
#if 0
{
uint8_t val1, val2, val3;
RD1(sc->base_sc, sc->def->volt_reg, &val1);
RD1(sc->base_sc, sc->def->cfg_reg, &val2);
RD1(sc->base_sc, sc->def->fps_reg, &val3);
printf("%s: Volt: 0x%02X, CFG: 0x%02X, FPS: 0x%02X\n", regnode_get_name(sc->regnode), val1, val2, val3);
}
#endif
	/* Get current power mode */
	rv = max77620_get_pwr_mode(sc, &val);
	if (rv != 0) {
		printf("%s: cannot read current power mode: %d\n",
		    regnode_get_name(sc->regnode), rv);
		return (rv);
	}
	sc->pwr_mode = val;

	/* Get current power ramp delay */
	rv = max77620_get_pwr_ramp_delay(sc, &intval);
	if (rv != 0) {
		printf("%s: cannot read current power mode: %d\n",
		    regnode_get_name(sc->regnode), rv);
		return (rv);
	}
	sc->pwr_ramp_delay = intval;

	/* Get FPS source if is not specified. */
	if (sc->active_fps_src == -1) {
		rv = max77620_get_fps_src(sc, &val);
		if (rv != 0) {
			printf("%s: cannot read current FPS source: %d\n",
			    regnode_get_name(sc->regnode), rv);
			return (rv);
		}
		sc->active_fps_src = val;
	}

	/* Configure power mode non-FPS controlled regulators. */
	if (sc->active_fps_src != MAX77620_FPS_SRC_NONE ||
	    (sc->pwr_mode != MAX77620_POWER_MODE_DISABLE &&
	    sc->pwr_mode != sc->enable_pwr_mode)) {
		rv = max77620_set_pwr_mode(sc, (uint8_t)sc->enable_pwr_mode);
		if (rv != 0) {
			printf("%s: cannot set power mode: %d\n",
			    regnode_get_name(sc->regnode), rv);
			return (rv);
		}
	}

	/* Set FPS source. */
	rv = max77620_set_fps_src(sc, sc->active_fps_src);
	if (rv != 0) {
		printf("%s: cannot setup FPS source: %d\n",
		    regnode_get_name(sc->regnode), rv);
		return (rv);
	}
	/* Set FPS slots. */
	rv = max77620_set_fps_slots(sc, false);
	if (rv != 0) {
		printf("%s: cannot setup power slots: %d\n",
		    regnode_get_name(sc->regnode), rv);
		return (rv);
	}
	/* Setup power ramp . */
	if (sc->ramp_rate_setting != -1) {
		rv = max77620_set_pwr_ramp_delay(sc, sc->pwr_ramp_delay);
		if (rv != 0) {
			printf("%s: cannot set power ramp delay: %d\n",
			    regnode_get_name(sc->regnode), rv);
			return (rv);
		}
	}

	return (0);
}

static void
max77620_fdt_parse(struct max77620_softc *sc, phandle_t node, struct reg_def *def,
struct max77620_regnode_init_def *init_def)
{
	int rv;
	phandle_t parent, supply_node;
	char prop_name[64]; /* Maximum OFW property name length. */

	rv = regulator_parse_ofw_stdparam(sc->dev, node,
	    &init_def->reg_init_def);

	rv = OF_getencprop(node, "maxim,active-fps-source",
	    &init_def->active_fps_src, sizeof(init_def->active_fps_src));
	if (rv <= 0)
		init_def->active_fps_src = MAX77620_FPS_SRC_DEF;

	rv = OF_getencprop(node, "maxim,active-fps-power-up-slot",
	    &init_def->active_fps_pu_slot, sizeof(init_def->active_fps_pu_slot));
	if (rv <= 0)
		init_def->active_fps_pu_slot = -1;

	rv = OF_getencprop(node, "maxim,active-fps-power-down-slot",
	    &init_def->active_fps_pd_slot, sizeof(init_def->active_fps_pd_slot));
	if (rv <= 0)
		init_def->active_fps_pd_slot = -1;

	rv = OF_getencprop(node, "maxim,suspend-fps-source",
	    &init_def->suspend_fps_src, sizeof(init_def->suspend_fps_src));
	if (rv <= 0)
		init_def->suspend_fps_src = -1;

	rv = OF_getencprop(node, "maxim,suspend-fps-power-up-slot",
	    &init_def->suspend_fps_pu_slot, sizeof(init_def->suspend_fps_pu_slot));
	if (rv <= 0)
		init_def->suspend_fps_pu_slot = -1;

	rv = OF_getencprop(node, "maxim,suspend-fps-power-down-slot",
	    &init_def->suspend_fps_pd_slot, sizeof(init_def->suspend_fps_pd_slot));
	if (rv <= 0)
		init_def->suspend_fps_pd_slot = -1;

	rv = OF_getencprop(node, "maxim,ramp-rate-setting",
	    &init_def->ramp_rate_setting, sizeof(init_def->ramp_rate_setting));
	if (rv <= 0)
		init_def->ramp_rate_setting = -1;

	/* Get parent supply. */
	if (def->supply_name == NULL)
		 return;

	parent = OF_parent(node);
	snprintf(prop_name, sizeof(prop_name), "%s-supply",
	    def->supply_name);
	rv = OF_getencprop(parent, prop_name, &supply_node,
	    sizeof(supply_node));
	if (rv <= 0)
		return;
	supply_node = OF_node_from_xref(supply_node);
	rv = OF_getprop_alloc(supply_node, "regulator-name",
	    (void **)&init_def->reg_init_def.parent_name);
	if (rv <= 0)
		init_def->reg_init_def.parent_name = NULL;
}

static struct max77620_reg_sc *
max77620_attach(struct max77620_softc *sc, phandle_t node, struct reg_def *def)
{
	struct max77620_reg_sc *reg_sc;
	struct max77620_regnode_init_def init_def;
	struct regnode *regnode;

	bzero(&init_def, sizeof(init_def));

	max77620_fdt_parse(sc, node, def, &init_def);
	init_def.reg_init_def.id = def->id;
	init_def.reg_init_def.ofw_node = node;
	regnode = regnode_create(sc->dev, &max77620_regnode_class,
	    &init_def.reg_init_def);
	if (regnode == NULL) {
		device_printf(sc->dev, "Cannot create regulator.\n");
		return (NULL);
	}
	reg_sc = regnode_get_softc(regnode);

	/* Init regulator softc. */
	reg_sc->regnode = regnode;
	reg_sc->base_sc = sc;
	reg_sc->def = def;
	reg_sc->xref = OF_xref_from_node(node);
	reg_sc->param = regnode_get_stdparam(regnode);
	reg_sc->active_fps_src = init_def.active_fps_src;
	reg_sc->active_fps_pu_slot = init_def.active_fps_pu_slot;
	reg_sc->active_fps_pd_slot = init_def.active_fps_pd_slot;
	reg_sc->suspend_fps_src = init_def.suspend_fps_src;
	reg_sc->suspend_fps_pu_slot = init_def.suspend_fps_pu_slot;
	reg_sc->suspend_fps_pd_slot = init_def.suspend_fps_pd_slot;
	reg_sc->ramp_rate_setting = init_def.ramp_rate_setting;

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
			device_printf(sc->dev,
			    "  FPS source: %d, mode: %d, ramp delay: %d\n",
			    reg_sc->fps_src, reg_sc->pwr_mode,
			    reg_sc->pwr_ramp_delay);
		}
		regnode_topo_unlock();
	}

	return (reg_sc);
}

int
max77620_regulator_attach(struct max77620_softc *sc, phandle_t node)
{
	struct max77620_reg_sc *reg;
	phandle_t child, rnode;
	int i;

	rnode = ofw_bus_find_child(node, "regulators");
	if (rnode <= 0) {
		device_printf(sc->dev, " Cannot find regulators subnode\n");
		return (ENXIO);
	}

	sc->nregs = nitems(max77620s_def);
	sc->regs = malloc(sizeof(struct max77620_reg_sc *) * sc->nregs,
	    M_MAX77620_REG, M_WAITOK | M_ZERO);


	/* Attach all known regulators if exist in DT. */
	for (i = 0; i < sc->nregs; i++) {
		child = ofw_bus_find_child(rnode, max77620s_def[i].name);
		if (child == 0) {
			if (bootverbose)
				device_printf(sc->dev,
				    "Regulator %s missing in DT\n",
				    max77620s_def[i].name);
			continue;
		}
		if (ofw_bus_node_status_okay(child) == 0)
			continue;
		reg = max77620_attach(sc, child, max77620s_def + i);
		if (reg == NULL) {
			device_printf(sc->dev, "Cannot attach regulator: %s\n",
			    max77620s_def[i].name);
			return (ENXIO);
		}
		sc->regs[i] = reg;
	}
	return (0);
}

int
max77620_regulator_map(device_t dev, phandle_t xref, int ncells,
    pcell_t *cells, intptr_t *num)
{
	struct max77620_softc *sc;
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

static int
max77620_regnode_enable(struct regnode *regnode, bool val, int *udelay)
{

	struct max77620_reg_sc *sc;
	uint8_t mode;
	int rv;

	sc = regnode_get_softc(regnode);

	if (sc->active_fps_src != MAX77620_FPS_SRC_NONE) {
		*udelay = 0;
		return (0);
	}

	if (val)
		mode = sc->enable_pwr_mode;
	else
		mode = MAX77620_POWER_MODE_DISABLE;

	rv = max77620_set_pwr_mode(sc, mode);
	if (rv != 0) {
		printf("%s: cannot set power mode: %d\n",
		    regnode_get_name(sc->regnode), rv);
		return (rv);
	}

	*udelay = sc->enable_usec;
	return (0);
}

static int
max77620_regnode_set_volt(struct regnode *regnode, int min_uvolt, int max_uvolt,
    int *udelay)
{
	struct max77620_reg_sc *sc;
	uint8_t sel;
	int rv;

	sc = regnode_get_softc(regnode);

	*udelay = 0;
	rv = regulator_range_volt_to_sel8(sc->def->ranges, sc->def->nranges,
	    min_uvolt, max_uvolt, &sel);
	if (rv != 0)
		return (rv);
	rv = max77620_set_sel(sc, sel);
	return (rv);
}

static int
max77620_regnode_get_volt(struct regnode *regnode, int *uvolt)
{

	struct max77620_reg_sc *sc;
	uint8_t sel;
	int rv;

	sc = regnode_get_softc(regnode);
	rv = max77620_get_sel(sc, &sel);
	if (rv != 0)
		return (rv);

	rv = regulator_range_sel8_to_volt(sc->def->ranges, sc->def->nranges,
	    sel, uvolt);
	return (rv);
	return(0);
}
