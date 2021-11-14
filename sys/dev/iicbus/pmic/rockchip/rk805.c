/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
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
#include <sys/clock.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/reboot.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/pmic/rockchip/rk805reg.h>
#include <dev/iicbus/pmic/rockchip/rk808reg.h>
#include <dev/iicbus/pmic/rockchip/rk8xx.h>

#include "clock_if.h"
#include "regdev_if.h"

MALLOC_DEFINE(M_RK805_REG, "RK805 regulator", "RK805 power regulator");

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk805", RK805},
	{"rockchip,rk808", RK808},
	{NULL,             0}
};

static struct rk8xx_regdef rk805_regdefs[] = {
	{
		.id = RK805_BUCK1,
		.name = "DCDC_REG1",
		.enable_reg = RK805_DCDC_EN,
		.enable_mask = 0x11,
		.voltage_reg = RK805_BUCK1_ON_VSEL,
		.voltage_mask = 0x3F,
		.voltage_min = 712500,
		.voltage_max = 1450000,
		.voltage_step = 12500,
		.voltage_nstep = 64,
	},
	{
		.id = RK805_BUCK2,
		.name = "DCDC_REG2",
		.enable_reg = RK805_DCDC_EN,
		.enable_mask = 0x22,
		.voltage_reg = RK805_BUCK2_ON_VSEL,
		.voltage_mask = 0x3F,
		.voltage_min = 712500,
		.voltage_max = 1450000,
		.voltage_step = 12500,
		.voltage_nstep = 64,
	},
	{
		.id = RK805_BUCK3,
		.name = "DCDC_REG3",
		.enable_reg = RK805_DCDC_EN,
		.enable_mask = 0x44,
	},
	{
		.id = RK805_BUCK4,
		.name = "DCDC_REG4",
		.enable_reg = RK805_DCDC_EN,
		.enable_mask = 0x88,
		.voltage_reg = RK805_BUCK4_ON_VSEL,
		.voltage_mask = 0x3F,
		.voltage_min = 800000,
		.voltage_max = 3500000,
		.voltage_step = 100000,
		.voltage_nstep = 28,
	},
	{
		.id = RK805_LDO1,
		.name = "LDO_REG1",
		.enable_reg = RK805_LDO_EN,
		.enable_mask = 0x11,
		.voltage_reg = RK805_LDO1_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 27,
	},
	{
		.id = RK805_LDO2,
		.name = "LDO_REG2",
		.enable_reg = RK805_LDO_EN,
		.enable_mask = 0x22,
		.voltage_reg = RK805_LDO2_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 27,
	},
	{
		.id = RK805_LDO3,
		.name = "LDO_REG3",
		.enable_reg = RK805_LDO_EN,
		.enable_mask = 0x44,
		.voltage_reg = RK805_LDO3_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 27,
	},
};

static struct rk8xx_regdef rk808_regdefs[] = {
	{
		.id = RK808_BUCK1,
		.name = "DCDC_REG1",
		.enable_reg = RK808_DCDC_EN,
		.enable_mask = 0x1,
		.voltage_reg = RK808_BUCK1_ON_VSEL,
		.voltage_mask = 0x3F,
		.voltage_min = 712500,
		.voltage_max = 1500000,
		.voltage_step = 12500,
		.voltage_nstep = 64,
	},
	{
		.id = RK808_BUCK2,
		.name = "DCDC_REG2",
		.enable_reg = RK808_DCDC_EN,
		.enable_mask = 0x2,
		.voltage_reg = RK808_BUCK2_ON_VSEL,
		.voltage_mask = 0x3F,
		.voltage_min = 712500,
		.voltage_max = 1500000,
		.voltage_step = 12500,
		.voltage_nstep = 64,
	},
	{
		/* BUCK3 voltage is calculated based on external resistor */
		.id = RK808_BUCK3,
		.name = "DCDC_REG3",
		.enable_reg = RK808_DCDC_EN,
		.enable_mask = 0x4,
	},
	{
		.id = RK808_BUCK4,
		.name = "DCDC_REG4",
		.enable_reg = RK808_DCDC_EN,
		.enable_mask = 0x8,
		.voltage_reg = RK808_BUCK4_ON_VSEL,
		.voltage_mask = 0xF,
		.voltage_min = 1800000,
		.voltage_max = 3300000,
		.voltage_step = 100000,
		.voltage_nstep = 16,
	},
	{
		.id = RK808_LDO1,
		.name = "LDO_REG1",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x1,
		.voltage_reg = RK808_LDO1_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 1800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 17,
	},
	{
		.id = RK808_LDO2,
		.name = "LDO_REG2",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x2,
		.voltage_reg = RK808_LDO2_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 1800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 17,
	},
	{
		.id = RK808_LDO3,
		.name = "LDO_REG3",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x4,
		.voltage_reg = RK808_LDO3_ON_VSEL,
		.voltage_mask = 0xF,
		.voltage_min = 800000,
		.voltage_max = 2500000,
		.voltage_step = 100000,
		.voltage_nstep = 18,
	},
	{
		.id = RK808_LDO4,
		.name = "LDO_REG4",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x8,
		.voltage_reg = RK808_LDO4_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 1800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 17,
	},
	{
		.id = RK808_LDO5,
		.name = "LDO_REG5",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x10,
		.voltage_reg = RK808_LDO5_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 1800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 17,
	},
	{
		.id = RK808_LDO6,
		.name = "LDO_REG6",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x20,
		.voltage_reg = RK808_LDO6_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 800000,
		.voltage_max = 2500000,
		.voltage_step = 100000,
		.voltage_nstep = 18,
	},
	{
		.id = RK808_LDO7,
		.name = "LDO_REG7",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x40,
		.voltage_reg = RK808_LDO7_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 800000,
		.voltage_max = 2500000,
		.voltage_step = 100000,
		.voltage_nstep = 18,
	},
	{
		.id = RK808_LDO8,
		.name = "LDO_REG8",
		.enable_reg = RK808_LDO_EN,
		.enable_mask = 0x80,
		.voltage_reg = RK808_LDO8_ON_VSEL,
		.voltage_mask = 0x1F,
		.voltage_min = 1800000,
		.voltage_max = 3400000,
		.voltage_step = 100000,
		.voltage_nstep = 17,
	},
	{
		.id = RK808_SWITCH1,
		.name = "SWITCH_REG1",
		.enable_reg = RK808_DCDC_EN,
		.enable_mask = 0x20,
		.voltage_min = 3000000,
		.voltage_max = 3000000,
	},
	{
		.id = RK808_SWITCH2,
		.name = "SWITCH_REG2",
		.enable_reg = RK808_DCDC_EN,
		.enable_mask = 0x40,
		.voltage_min = 3000000,
		.voltage_max = 3000000,
	},
};

int
rk8xx_read(device_t dev, uint8_t reg, uint8_t *data, uint8_t size)
{
	int err;

	err = iicdev_readfrom(dev, reg, data, size, IIC_INTRWAIT);
	return (err);
}

int
rk8xx_write(device_t dev, uint8_t reg, uint8_t *data, uint8_t size)
{

	return (iicdev_writeto(dev, reg, data, size, IIC_INTRWAIT));
}

static int
rk8xx_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip RK8XX PMIC");
	return (BUS_PROBE_DEFAULT);
}

static void
rk8xx_start(void *pdev)
{
	struct rk8xx_softc *sc;
	device_t dev;
	uint8_t data[2];
	int err;

	dev = pdev;
	sc = device_get_softc(dev);
	sc->dev = dev;

	/* No version register in RK808 */
	if (bootverbose && sc->type == RK805) {
		err = rk8xx_read(dev, RK805_CHIP_NAME, data, 1);
		if (err != 0) {
			device_printf(dev, "Cannot read chip name reg\n");
			return;
		}
		err = rk8xx_read(dev, RK805_CHIP_VER, data + 1, 1);
		if (err != 0) {
			device_printf(dev, "Cannot read chip version reg\n");
			return;
		}
		device_printf(dev, "Chip Name: %x\n",
		    data[0] << 4 | ((data[1] >> 4) & 0xf));
		device_printf(dev, "Chip Version: %x\n", data[1] & 0xf);
	}

	/* Register this as a 1Hz clock */
	clock_register(dev, 1000000);

	config_intrhook_disestablish(&sc->intr_hook);
}

static int
rk8xx_attach(device_t dev)
{
	struct rk8xx_softc *sc;
	struct rk8xx_reg_sc *reg;
	struct rk8xx_regdef *regdefs;
	struct reg_list *regp;
	phandle_t rnode, child;
	int error, i;

	sc = device_get_softc(dev);

	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	error = rk8xx_export_clocks(dev);
	if (error != 0)
		return (error);

	sc->intr_hook.ich_func = rk8xx_start;
	sc->intr_hook.ich_arg = dev;
	if (config_intrhook_establish(&sc->intr_hook) != 0)
		return (ENOMEM);

	switch (sc->type) {
	case RK805:
		regdefs = rk805_regdefs;
		sc->nregs = nitems(rk805_regdefs);
		sc->rtc_regs.secs = RK805_RTC_SECS;
		sc->rtc_regs.secs_mask = RK805_RTC_SECS_MASK;
		sc->rtc_regs.minutes = RK805_RTC_MINUTES;
		sc->rtc_regs.minutes_mask = RK805_RTC_MINUTES_MASK;
		sc->rtc_regs.hours = RK805_RTC_HOURS;
		sc->rtc_regs.hours_mask = RK805_RTC_HOURS_MASK;
		sc->rtc_regs.days = RK805_RTC_DAYS;
		sc->rtc_regs.days_mask = RK805_RTC_DAYS_MASK;
		sc->rtc_regs.months = RK805_RTC_MONTHS;
		sc->rtc_regs.months_mask = RK805_RTC_MONTHS_MASK;
		sc->rtc_regs.years = RK805_RTC_YEARS;
		sc->rtc_regs.weeks = RK805_RTC_WEEKS_MASK;
		sc->rtc_regs.ctrl = RK805_RTC_CTRL;
		sc->rtc_regs.ctrl_stop_mask = RK805_RTC_CTRL_STOP;
		sc->rtc_regs.ctrl_ampm_mask = RK805_RTC_AMPM_MODE;
		sc->rtc_regs.ctrl_gettime_mask = RK805_RTC_GET_TIME;
		sc->rtc_regs.ctrl_readsel_mask = RK805_RTC_READSEL;
		break;
	case RK808:
		regdefs = rk808_regdefs;
		sc->nregs = nitems(rk808_regdefs);
		sc->rtc_regs.secs = RK808_RTC_SECS;
		sc->rtc_regs.secs_mask = RK808_RTC_SECS_MASK;
		sc->rtc_regs.minutes = RK808_RTC_MINUTES;
		sc->rtc_regs.minutes_mask = RK808_RTC_MINUTES_MASK;
		sc->rtc_regs.hours = RK808_RTC_HOURS;
		sc->rtc_regs.hours_mask = RK808_RTC_HOURS_MASK;
		sc->rtc_regs.days = RK808_RTC_DAYS;
		sc->rtc_regs.days_mask = RK808_RTC_DAYS_MASK;
		sc->rtc_regs.months = RK808_RTC_MONTHS;
		sc->rtc_regs.months_mask = RK808_RTC_MONTHS_MASK;
		sc->rtc_regs.years = RK808_RTC_YEARS;
		sc->rtc_regs.weeks = RK808_RTC_WEEKS_MASK;
		sc->rtc_regs.ctrl = RK808_RTC_CTRL;
		sc->rtc_regs.ctrl_stop_mask = RK808_RTC_CTRL_STOP;
		sc->rtc_regs.ctrl_ampm_mask = RK808_RTC_AMPM_MODE;
		sc->rtc_regs.ctrl_gettime_mask = RK808_RTC_GET_TIME;
		sc->rtc_regs.ctrl_readsel_mask = RK808_RTC_READSEL;
		break;
	default:
		device_printf(dev, "Unknown type %d\n", sc->type);
		return (ENXIO);
	}

	TAILQ_INIT(&sc->regs);

	rnode = ofw_bus_find_child(ofw_bus_get_node(dev), "regulators");
	if (rnode > 0) {
		for (i = 0; i < sc->nregs; i++) {
			child = ofw_bus_find_child(rnode,
			    regdefs[i].name);
			if (child == 0)
				continue;
			if (OF_hasprop(child, "regulator-name") != 1)
				continue;
			reg = rk8xx_reg_attach(dev, child, &regdefs[i]);
			if (reg == NULL) {
				device_printf(dev,
				    "cannot attach regulator %s\n",
				    regdefs[i].name);
				continue;
			}
			regp = malloc(sizeof(*regp), M_DEVBUF, M_WAITOK | M_ZERO);
			regp->reg = reg;
			TAILQ_INSERT_TAIL(&sc->regs, regp, next);
			if (bootverbose)
				device_printf(dev, "Regulator %s attached\n",
				    regdefs[i].name);
		}
	}

	if (OF_hasprop(ofw_bus_get_node(dev),
	    "rockchip,system-power-controller")) {
		/*
		 * The priority is chosen to override PSCI and EFI shutdown
		 * methods as those two just hang without powering off on Rock64
		 * at least.
		 */
		EVENTHANDLER_REGISTER(shutdown_final, rk805_poweroff, dev,
		    SHUTDOWN_PRI_LAST - 2);
	}

	return (0);
}

static int
rk8xx_detach(device_t dev)
{

	/* We cannot detach regulators */
	return (EBUSY);
}

static device_method_t rk8xx_methods[] = {
	DEVMETHOD(device_probe,		rk8xx_probe),
	DEVMETHOD(device_attach,	rk8xx_attach),
	DEVMETHOD(device_detach,	rk8xx_detach),

	/* regdev interface */
	DEVMETHOD(regdev_map,		rk8xx_map),

	/* Clock interface */
	DEVMETHOD(clock_gettime,	rk8xx_gettime),
	DEVMETHOD(clock_settime,	rk8xx_settime),

	DEVMETHOD_END
};

static driver_t rk8xx_driver = {
	"rk8xx_pmu",
	rk8xx_methods,
	sizeof(struct rk8xx_softc),
};

static devclass_t rk8xx_devclass;

EARLY_DRIVER_MODULE(rk8xx, iicbus, rk8xx_driver, rk8xx_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
MODULE_DEPEND(rk8xx, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(rk8xx, 1);
