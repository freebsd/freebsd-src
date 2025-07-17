/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2022 Soren Schmidt <sos@deepcore.dk>
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/pmic/rockchip/rk817reg.h>
#include <dev/iicbus/pmic/rockchip/rk8xx.h>


static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk809",	RK809},
	{"rockchip,rk817",	RK817},
	{NULL,			0}
};

static struct rk8xx_regdef rk809_regdefs[] = {
	{
		.id = RK809_DCDC1,
		.name = "DCDC_REG1",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x11,
		.voltage_reg = RK817_DCDC1_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 2400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 177,
	},
	{
		.id = RK809_DCDC2,
		.name = "DCDC_REG2",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x22,
		.voltage_reg = RK817_DCDC2_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 2400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 177,
	},
	{
		.id = RK809_DCDC3,
		.name = "DCDC_REG3",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x44,
		.voltage_reg = RK817_DCDC3_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 2400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 177,
	},
	{
		.id = RK809_DCDC4,
		.name = "DCDC_REG4",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x88,
		.voltage_reg = RK817_DCDC4_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 3400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 195,
	},
	{
		.id = RK809_DCDC5,
		.name = "DCDC_REG5",
		.enable_reg = RK817_LDO_EN3,
		.enable_mask = 0x22,
		.voltage_reg = RK817_BOOST_ON_VSEL,
		.voltage_mask = 0x07,
		.voltage_min = 1600000,	/* cheat is 1.5V */
		.voltage_max = 3400000,
		.voltage_min2 = 3500000,
		.voltage_max2 = 3600000,
		.voltage_step = 200000,
		.voltage_step2 = 300000,
		.voltage_nstep = 8,
	},
	{
		.id = RK809_LDO1,
		.name = "LDO_REG1",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x11,
		.voltage_reg = RK817_LDO1_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO2,
		.name = "LDO_REG2",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x22,
		.voltage_reg = RK817_LDO2_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO3,
		.name = "LDO_REG3",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x44,
		.voltage_reg = RK817_LDO3_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO4,
		.name = "LDO_REG4",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x88,
		.voltage_reg = RK817_LDO4_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO5,
		.name = "LDO_REG5",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x11,
		.voltage_reg = RK817_LDO5_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO6,
		.name = "LDO_REG6",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x22,
		.voltage_reg = RK817_LDO6_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO7,
		.name = "LDO_REG7",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x44,
		.voltage_reg = RK817_LDO7_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO8,
		.name = "LDO_REG8",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x88,
		.voltage_reg = RK817_LDO8_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_LDO9,
		.name = "LDO_REG9",
		.enable_reg = RK817_LDO_EN3,
		.enable_mask = 0x11,
		.voltage_reg = RK817_LDO9_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK809_SWITCH1,
		.name = "SWITCH_REG1",
		.enable_reg = RK817_LDO_EN3,
		.enable_mask = 0x44,
		.voltage_min = 3300000,
		.voltage_max = 3300000,
		.voltage_nstep = 0,
	},
	{
		.id = RK809_SWITCH2,
		.name = "SWITCH_REG2",
		.enable_reg = RK817_LDO_EN3,
		.enable_mask = 0x88,
		.voltage_min = 3300000,
		.voltage_max = 3300000,
		.voltage_nstep = 0,
	},
};

static struct rk8xx_regdef rk817_regdefs[] = {
	{
		.id = RK817_DCDC1,
		.name = "DCDC_REG1",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x11,
		.voltage_reg = RK817_DCDC1_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 2400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 177,
	},
	{
		.id = RK817_DCDC2,
		.name = "DCDC_REG2",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x22,
		.voltage_reg = RK817_DCDC2_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 2400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 177,
	},
	{
		.id = RK817_DCDC3,
		.name = "DCDC_REG3",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x44,
		.voltage_reg = RK817_DCDC3_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 2400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 177,
	},
	{
		.id = RK817_DCDC4,
		.name = "DCDC_REG4",
		.enable_reg = RK817_DCDC_EN,
		.enable_mask = 0x88,
		.voltage_reg = RK817_DCDC4_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 500000,
		.voltage_max = 1487500,
		.voltage_min2 = 1500000,
		.voltage_max2 = 3400000,
		.voltage_step = 12500,
		.voltage_step2 = 100000,
		.voltage_nstep = 195,
	},
	{
		.id = RK817_LDO1,
		.name = "LDO_REG1",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x11,
		.voltage_reg = RK817_LDO1_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO2,
		.name = "LDO_REG2",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x22,
		.voltage_reg = RK817_LDO2_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO3,
		.name = "LDO_REG3",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x44,
		.voltage_reg = RK817_LDO3_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO4,
		.name = "LDO_REG4",
		.enable_reg = RK817_LDO_EN1,
		.enable_mask = 0x88,
		.voltage_reg = RK817_LDO4_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO5,
		.name = "LDO_REG5",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x11,
		.voltage_reg = RK817_LDO5_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO6,
		.name = "LDO_REG6",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x22,
		.voltage_reg = RK817_LDO6_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO7,
		.name = "LDO_REG7",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x44,
		.voltage_reg = RK817_LDO7_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO8,
		.name = "LDO_REG8",
		.enable_reg = RK817_LDO_EN2,
		.enable_mask = 0x88,
		.voltage_reg = RK817_LDO8_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_LDO9,
		.name = "LDO_REG9",
		.enable_reg = RK817_LDO_EN3,
		.enable_mask = 0x11,
		.voltage_reg = RK817_LDO9_ON_VSEL,
		.voltage_mask = 0x7f,
		.voltage_min = 600000,
		.voltage_max = 3400000,
		.voltage_step = 25000,
		.voltage_nstep = 112,
	},
	{
		.id = RK817_BOOST,
		.name = "BOOST",
		.enable_reg = RK817_LDO_EN3,
		.enable_mask = 0x22,
		.voltage_reg = RK817_BOOST_ON_VSEL,
		.voltage_mask = 0x07,
		.voltage_min = 4700000,
		.voltage_max = 5400000,
		.voltage_step = 100000,
		.voltage_nstep = 8,
	},
	{
		.id = RK817_OTG_SWITCH,
		.name = "OTG_SWITCH",
		.enable_reg = RK817_LDO_EN3,
		.enable_mask = 0x44,
		.voltage_nstep = 0,
	},
};

static int
rk817_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	switch (ofw_bus_search_compatible(dev, compat_data)->ocd_data) {
	case RK809:
		device_set_desc(dev, "RockChip RK809 PMIC");
		break;
	case RK817:
		device_set_desc(dev, "RockChip RK817 PMIC");
		break;
	default:
		return (ENXIO);
	}

	return (BUS_PROBE_DEFAULT);
}

static int
rk817_attach(device_t dev)
{
	struct rk8xx_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	switch (sc->type) {
	case RK809:
		sc->regdefs = rk809_regdefs;
		sc->nregs = nitems(rk809_regdefs);
		break;
	case RK817:
		sc->regdefs = rk817_regdefs;
		sc->nregs = nitems(rk817_regdefs);
		break;
	default:
		device_printf(dev, "Unknown type %d\n", sc->type);
		return (ENXIO);
	}
	sc->rtc_regs.secs = RK817_RTC_SECONDS;
	sc->rtc_regs.secs_mask = RK817_RTC_SECONDS_MASK;
	sc->rtc_regs.minutes = RK817_RTC_MINUTES;
	sc->rtc_regs.minutes_mask = RK817_RTC_MINUTES_MASK;
	sc->rtc_regs.hours = RK817_RTC_HOURS;
	sc->rtc_regs.hours_mask = RK817_RTC_HOURS_MASK;
	sc->rtc_regs.days = RK817_RTC_DAYS;
	sc->rtc_regs.days_mask = RK817_RTC_DAYS_MASK;
	sc->rtc_regs.months = RK817_RTC_MONTHS;
	sc->rtc_regs.months_mask = RK817_RTC_MONTHS_MASK;
	sc->rtc_regs.years = RK817_RTC_YEARS;
	sc->rtc_regs.weeks = RK817_RTC_WEEKS_MASK;
	sc->rtc_regs.ctrl = RK817_RTC_CTRL;
	sc->rtc_regs.ctrl_stop_mask = RK817_RTC_CTRL_STOP;
	sc->rtc_regs.ctrl_ampm_mask = RK817_RTC_AMPM_MODE;
	sc->rtc_regs.ctrl_gettime_mask = RK817_RTC_GET_TIME;
	sc->rtc_regs.ctrl_readsel_mask = RK817_RTC_READSEL;
	sc->dev_ctrl.dev_ctrl_reg = RK817_SYS_CFG3;
	sc->dev_ctrl.pwr_off_mask = RK817_SYS_CFG3_OFF;
	sc->dev_ctrl.pwr_rst_mask = RK817_SYS_CFG3_RST;

	return (rk8xx_attach(sc));
}

static device_method_t rk817_methods[] = {
	DEVMETHOD(device_probe,		rk817_probe),
	DEVMETHOD(device_attach,	rk817_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk817_pmu, rk817_driver, rk817_methods,
    sizeof(struct rk8xx_softc), rk8xx_driver);

EARLY_DRIVER_MODULE(rk817_pmu, iicbus, rk817_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
EARLY_DRIVER_MODULE(iicbus, rk817_pmu, iicbus_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
MODULE_DEPEND(rk817_pmu, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(rk817_pmu, 1);
