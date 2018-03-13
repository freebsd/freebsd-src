/*-
 * Copyright (c) 2017 Emmanuel Vadot <manu@freebsd.org>
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

#if defined(__aarch64__)
#include "opt_soc.h"
#endif

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm/allwinner/clkng/aw_ccung.h>
#include <arm/allwinner/clkng/aw_clk.h>
#include <arm/allwinner/clkng/aw_clk_nm.h>
#include <arm/allwinner/clkng/aw_clk_nkmp.h>
#include <arm/allwinner/clkng/aw_clk_prediv_mux.h>

#include <arm/allwinner/clkng/ccu_sun8i_r.h>

#include <gnu/dts/include/dt-bindings/clock/sun8i-r-ccu.h>
#include <gnu/dts/include/dt-bindings/reset/sun8i-r-ccu.h>

/* Non-exported clocks */
#define	CLK_AHB0	1
#define	CLK_APB0	2

static struct aw_ccung_reset ccu_sun8i_r_resets[] = {
	CCU_RESET(RST_APB0_IR, 0xb0, 1)
	CCU_RESET(RST_APB0_TIMER, 0xb0, 2)
	CCU_RESET(RST_APB0_RSB, 0xb0, 4)
	CCU_RESET(RST_APB0_UART, 0xb0, 6)
};

static struct aw_ccung_gate ccu_sun8i_r_gates[] = {
	CCU_GATE(CLK_APB0_PIO, "apb0-pio", "apb0", 0x28, 0)
	CCU_GATE(CLK_APB0_IR, "apb0-ir", "apb0", 0x28, 1)
	CCU_GATE(CLK_APB0_TIMER, "apb0-timer", "apb0", 0x28, 2)
	CCU_GATE(CLK_APB0_RSB, "apb0-rsb", "apb0", 0x28, 3)
	CCU_GATE(CLK_APB0_UART, "apb0-uart", "apb0", 0x28, 4)
	CCU_GATE(CLK_APB0_I2C, "apb0-i2c", "apb0", 0x28, 6)
	CCU_GATE(CLK_APB0_TWD, "apb0-twd", "apb0", 0x28, 7)
};

static const char *ar100_parents[] = {"osc32k", "osc24M", "pll_periph0", "iosc"};
static const char *a83t_ar100_parents[] = {"osc16M-d512", "osc24M", "pll_periph", "osc16M"};
PREDIV_CLK(ar100_clk, CLK_AR100,				/* id */
    "ar100", ar100_parents,					/* name, parents */
    0x00,							/* offset */
    16, 2,							/* mux */
    4, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,			/* div */
    8, 5, 0, AW_CLK_FACTOR_HAS_COND,				/* prediv */
    16, 2, 2);							/* prediv condition */
PREDIV_CLK(a83t_ar100_clk, CLK_AR100,				/* id */
    "ar100", a83t_ar100_parents,				/* name, parents */
    0x00,							/* offset */
    16, 2,							/* mux */
    4, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,			/* div */
    8, 5, 0, AW_CLK_FACTOR_HAS_COND,				/* prediv */
    16, 2, 2);							/* prediv condition */

static const char *ahb0_parents[] = {"ar100"};
FIXED_CLK(ahb0_clk,
    CLK_AHB0,			/* id */
    "ahb0",			/* name */
    ahb0_parents,		/* parent */
    0,				/* freq */
    1,				/* mult */
    1,				/* div */
    0);				/* flags */

static const char *apb0_parents[] = {"ahb0"};
DIV_CLK(apb0_clk,
    CLK_APB0,			/* id */
    "apb0", apb0_parents,	/* name, parents */
    0x0c,			/* offset */
    0, 2,			/* shift, width */
    0, NULL);			/* flags, div table */

static const char *ir_parents[] = {"osc32k", "osc24M"};
NM_CLK(ir_clk,
    CLK_IR,				/* id */
    "ir", ir_parents,			/* names, parents */
    0x54,				/* offset */
    0, 4, 0, 0,				/* N factor */
    16, 2, 0, 0,			/* M flags */
    24, 2,				/* mux */
    31,					/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);	/* flags */

static struct aw_clk_prediv_mux_def *r_ccu_prediv_mux_clks[] = {
	&ar100_clk,
};

static struct aw_clk_prediv_mux_def *a83t_r_ccu_prediv_mux_clks[] = {
	&a83t_ar100_clk,
};

static struct clk_div_def *div_clks[] = {
	&apb0_clk,
};

static struct clk_fixed_def *fixed_factor_clks[] = {
	&ahb0_clk,
};

static struct aw_clk_nm_def *nm_clks[] = {
	&ir_clk,
};

void
ccu_sun8i_r_register_clocks(struct aw_ccung_softc *sc)
{
	int i;
	struct aw_clk_prediv_mux_def **prediv_mux_clks;

	sc->resets = ccu_sun8i_r_resets;
	sc->nresets = nitems(ccu_sun8i_r_resets);
	sc->gates = ccu_sun8i_r_gates;
	sc->ngates = nitems(ccu_sun8i_r_gates);

	/* a83t names the parents differently than the others */
	if (sc->type == A83T_R_CCU)
		prediv_mux_clks = a83t_r_ccu_prediv_mux_clks;
	else
		prediv_mux_clks = r_ccu_prediv_mux_clks;

	for (i = 0; i < nitems(prediv_mux_clks); i++)
		aw_clk_prediv_mux_register(sc->clkdom, prediv_mux_clks[i]);
	for (i = 0; i < nitems(div_clks); i++)
		clknode_div_register(sc->clkdom, div_clks[i]);
	for (i = 0; i < nitems(fixed_factor_clks); i++)
		clknode_fixed_register(sc->clkdom, fixed_factor_clks[i]);
	for (i = 0; i < nitems(nm_clks); i++)
		aw_clk_nm_register(sc->clkdom, nm_clks[i]);
}
