/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm/allwinner/clkng/aw_ccung.h>

#include <gnu/dts/include/dt-bindings/clock/sun50i-h6-r-ccu.h>
#include <gnu/dts/include/dt-bindings/reset/sun50i-h6-r-ccu.h>

/* Non-exported clocks */
#define	CLK_R_AHB	1
#define	CLK_R_APB2	3

static struct aw_ccung_reset ccu_sun50i_h6_r_resets[] = {
	CCU_RESET(RST_R_APB1_TIMER, 0x11c, 16)
	CCU_RESET(RST_R_APB1_TWD, 0x12c, 16)
	CCU_RESET(RST_R_APB1_PWM, 0x13c, 16)
	CCU_RESET(RST_R_APB2_UART, 0x18c, 16)
	CCU_RESET(RST_R_APB2_I2C, 0x19c, 16)
	CCU_RESET(RST_R_APB1_IR, 0x1cc, 16)
	CCU_RESET(RST_R_APB1_W1, 0x1ec, 16)
};

static struct aw_ccung_gate ccu_sun50i_h6_r_gates[] = {
	CCU_GATE(CLK_R_APB1_TIMER, "r_apb1-timer", "r_apb1", 0x11c, 0)
	CCU_GATE(CLK_R_APB1_TWD, "r_apb1-twd", "r_apb1", 0x12c, 0)
	CCU_GATE(CLK_R_APB1_PWM, "r_apb1-pwm", "r_apb1", 0x13c, 0)
	CCU_GATE(CLK_R_APB2_UART, "r_apb1-uart", "r_apb2", 0x18c, 0)
	CCU_GATE(CLK_R_APB2_I2C, "r_apb1-i2c", "r_apb2", 0x19c, 0)
	CCU_GATE(CLK_R_APB1_IR, "r_apb1-ir", "r_apb1", 0x1cc, 0)
	CCU_GATE(CLK_R_APB1_W1, "r_apb1-w1", "r_apb1", 0x1ec, 0)
};

static const char *ar100_parents[] = {"osc24M", "osc32k", "pll_periph0", "iosc"};
PREDIV_CLK(ar100_clk, CLK_AR100,				/* id */
    "ar100", ar100_parents,					/* name, parents */
    0x00,							/* offset */
    16, 2,							/* mux */
    4, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,			/* div */
    8, 5, 0, AW_CLK_FACTOR_HAS_COND,				/* prediv */
    16, 2, 2);							/* prediv condition */

static const char *r_ahb_parents[] = {"ar100"};
FIXED_CLK(r_ahb_clk,
    CLK_R_AHB,			/* id */
    "r_ahb",			/* name */
    r_ahb_parents,		/* parent */
    0,				/* freq */
    1,				/* mult */
    1,				/* div */
    0);				/* flags */

static const char *r_apb1_parents[] = {"r_ahb"};
DIV_CLK(r_apb1_clk,
    CLK_R_APB1,			/* id */
    "r_apb1", r_apb1_parents,	/* name, parents */
    0x0c,			/* offset */
    0, 2,			/* shift, width */
    0, NULL);			/* flags, div table */

static const char *r_apb2_parents[] = {"osc24M", "osc32k", "pll_periph0", "iosc"};
PREDIV_CLK(r_apb2_clk, CLK_R_APB2,				/* id */
    "r_apb2", r_apb2_parents,					/* name, parents */
    0x10,							/* offset */
    16, 2,							/* mux */
    4, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,			/* div */
    8, 5, 0, AW_CLK_FACTOR_HAS_COND,				/* prediv */
    16, 2, 2);							/* prediv condition */

static struct aw_ccung_clk clks[] = {
	{ .type = AW_CLK_PREDIV_MUX, .clk.prediv_mux = &ar100_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &r_ahb_clk},
	{ .type = AW_CLK_DIV, .clk.div = &r_apb1_clk},
	{ .type = AW_CLK_PREDIV_MUX, .clk.prediv_mux = &r_apb2_clk},
};

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun50i-h6-r-ccu", 1 },
	{ NULL, 0},
};

static int
ccu_sun50i_h6_r_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner SUN50I_H6_R Clock Control Unit NG");
	return (BUS_PROBE_DEFAULT);
}

static int
ccu_sun50i_h6_r_attach(device_t dev)
{
	struct aw_ccung_softc *sc;

	sc = device_get_softc(dev);

	sc->resets = ccu_sun50i_h6_r_resets;
	sc->nresets = nitems(ccu_sun50i_h6_r_resets);
	sc->gates = ccu_sun50i_h6_r_gates;
	sc->ngates = nitems(ccu_sun50i_h6_r_gates);
	sc->clks = clks;
	sc->nclks = nitems(clks);

	return (aw_ccung_attach(dev));
}

static device_method_t ccu_sun50i_h6_r_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ccu_sun50i_h6_r_probe),
	DEVMETHOD(device_attach,	ccu_sun50i_h6_r_attach),

	DEVMETHOD_END
};

static devclass_t ccu_sun50i_h6_r_devclass;

DEFINE_CLASS_1(ccu_sun50i_h6_r, ccu_sun50i_h6_r_driver, ccu_sun50i_h6_r_methods,
  sizeof(struct aw_ccung_softc), aw_ccung_driver);

EARLY_DRIVER_MODULE(ccu_sun50i_h6_r, simplebus, ccu_sun50i_h6_r_driver,
    ccu_sun50i_h6_r_devclass, 0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
