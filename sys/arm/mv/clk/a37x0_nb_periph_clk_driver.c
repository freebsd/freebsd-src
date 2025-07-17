/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Semihalf.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clkdev_if.h"
#include "periph.h"

#define NB_DEV_COUNT 17

static struct clk_div_table a37x0_periph_clk_table_6 [] = {
	{ .value = 1, .divider = 1 },
	{ .value = 2, .divider = 2 },
	{ .value = 3, .divider = 3 },
	{ .value = 4, .divider = 4 },
	{ .value = 5, .divider = 5 },
	{ .value = 6, .divider = 6 },
	{ .value = 0, .divider = 0 }
};

static struct clk_div_table a37x0_periph_clk_table_2 [] = {
	{ .value = 0, .divider = 1 },
	{ .value = 1, .divider = 2 },
	{ .value = 2, .divider = 4 },
	{ .value = 3, .divider = 1 }
};

static struct a37x0_periph_clknode_def a37x0_nb_devices [] = {
	CLK_FULL_DD("mmc", 0, 2, 0, 0, DIV_SEL2, DIV_SEL2, 16, 13,
	    "tbg_mux_mmc_50", "div1_mmc_51", "div2_mmc_52", "clk_mux_mmc_53"),
	CLK_FULL_DD("sata_host", 1, 3, 2, 1, DIV_SEL2, DIV_SEL2, 10, 7,
	    "tbg_sata_host_mmc_55", "div1_sata_host_56", "div2_sata_host_57",
	    "clk_sata_host_mmc_58"),
	CLK_FULL_DD("sec_at", 2, 6, 4, 2, DIV_SEL1, DIV_SEL1, 3, 0,
	    "tbg_mux_sec_at_60", "div1_sec_at_61", "div2_sec_at_62",
	    "clk_mux_sec_at_63"),
	CLK_FULL_DD("sec_dap", 3, 7, 6, 3, DIV_SEL1, DIV_SEL1, 9, 6,
	    "tbg_mux_sec_dap_65", "div1_sec_dap_67", "div2_sec_dap_68",
	    "clk_mux_sec_dap_69"),
	CLK_FULL_DD("tsecm", 4, 8, 8, 4, DIV_SEL1, DIV_SEL1, 15, 12,
	    "tbg_mux_tsecm_71", "div1_tsecm_72", "div2_tsecm_73",
	    "clk_mux_tsecm_74"),
	CLK_FULL("setm_tmx", 5, 10, 10, 5, DIV_SEL1, 18,
	    a37x0_periph_clk_table_6, "tbg_mux_setm_tmx_76",
	    "div1_setm_tmx_77", "clk_mux_setm_tmx_78"),
	CLK_FIXED("avs", 6, 11, 6, "mux_avs_80", "fixed1_avs_82"),
	CLK_FULL_DD("pwm", 7, 13, 14, 8, DIV_SEL0, DIV_SEL0, 3, 0,
	    "tbg_mux_pwm_83", "div1_pwm_84", "div2_pwm_85", "clk_mux_pwm_86"),
	CLK_FULL_DD("sqf", 8, 12, 12, 7, DIV_SEL1, DIV_SEL1, 27, 14,
	    "tbg_mux_sqf_88", "div1_sqf_89", "div2_sqf_90", "clk_mux_sqf_91"),
	CLK_GATE("i2c_2", 9, 16, NULL),
	CLK_GATE("i2c_1", 10, 17, NULL),
	CLK_MUX_GATE_FIXED("ddr_phy", 11, 19, 10, "mux_ddr_phy_95",
	    "gate_ddr_phy_96", "fixed1_ddr_phy_97"),
	CLK_FULL_DD("ddr_fclk", 12, 21, 16, 11, DIV_SEL0, DIV_SEL0, 15, 12,
	    "tbg_mux_ddr_fclk_99", "div1_ddr_fclk_100", "div2_ddr_fclk_101",
	    "clk_mux_ddr_fclk_102"),
	CLK_FULL("trace", 13, 22, 18, 12, DIV_SEL0, 20,
	    a37x0_periph_clk_table_6, "tbg_mux_trace_104", "div1_trace_105",
	    "clk_mux_trace_106"),
	CLK_FULL("counter", 14, 23, 20, 13, DIV_SEL0, 23,
	    a37x0_periph_clk_table_6, "tbg_mux_counter_108",
	    "div1_counter_109", "clk_mux_counter_110"),
	CLK_FULL_DD("eip97", 15, 26, 24, 9, DIV_SEL2, DIV_SEL2, 22, 19,
	    "tbg_mux_eip97_112", "div1_eip97_113", "div2_eip97_114",
	    "clk_mux_eip97_115"),
	CLK_CPU("cpu", 16, 22, 15, DIV_SEL0, 28, a37x0_periph_clk_table_2,
	    "tbg_mux_cpu_117", "div1_cpu_118"),
};

static struct ofw_compat_data a37x0_periph_compat_data [] = {
	{ "marvell,armada-3700-periph-clock-nb",	1 },
	{ NULL,						0 }
};

static int a37x0_nb_periph_clk_attach(device_t);
static int a37x0_nb_periph_clk_probe(device_t);

static device_method_t a37x0_nb_periph_clk_methods[] = {
	DEVMETHOD(clkdev_device_unlock,		a37x0_periph_clk_device_unlock),
	DEVMETHOD(clkdev_device_lock,		a37x0_periph_clk_device_lock),
	DEVMETHOD(clkdev_read_4,		a37x0_periph_clk_read_4),

	DEVMETHOD(device_attach,		a37x0_nb_periph_clk_attach),
	DEVMETHOD(device_detach,		a37x0_periph_clk_detach),
	DEVMETHOD(device_probe,			a37x0_nb_periph_clk_probe),

	DEVMETHOD_END
};

static driver_t a37x0_nb_periph_driver = {
	"a37x0_nb_periph_driver",
	a37x0_nb_periph_clk_methods,
	sizeof(struct a37x0_periph_clk_softc)
};

EARLY_DRIVER_MODULE(a37x0_nb_periph, simplebus, a37x0_nb_periph_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_LATE);

static int
a37x0_nb_periph_clk_attach(device_t dev)
{
	struct a37x0_periph_clk_softc *sc;

	sc = device_get_softc(dev);
	sc->devices = a37x0_nb_devices;
	sc->device_count = NB_DEV_COUNT;

	return (a37x0_periph_clk_attach(dev));
}

static int
a37x0_nb_periph_clk_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev,
	    a37x0_periph_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "marvell,armada-3700-nb-periph-clock");

	return (BUS_PROBE_DEFAULT);
}
