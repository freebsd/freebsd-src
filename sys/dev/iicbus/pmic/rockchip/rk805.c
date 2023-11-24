/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <dev/iicbus/pmic/rockchip/rk8xx.h>

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk805", RK805},
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

static int
rk805_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip RK805 PMIC");
	return (BUS_PROBE_DEFAULT);
}

static int
rk805_attach(device_t dev)
{
	struct rk8xx_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	sc->regdefs = rk805_regdefs;
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
	sc->dev_ctrl.dev_ctrl_reg = RK805_DEV_CTRL;
	sc->dev_ctrl.pwr_off_mask = RK805_DEV_CTRL_OFF;

	return (rk8xx_attach(sc));
}

static device_method_t rk805_methods[] = {
	DEVMETHOD(device_probe,		rk805_probe),
	DEVMETHOD(device_attach,	rk805_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk805_pmu, rk805_driver, rk805_methods,
    sizeof(struct rk8xx_softc), rk8xx_driver);

EARLY_DRIVER_MODULE(rk805_pmu, iicbus, rk805_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
EARLY_DRIVER_MODULE(iicbus, rk805_pmu, iicbus_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
MODULE_DEPEND(rk805_pmu, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(rk805_pmu, 1);
