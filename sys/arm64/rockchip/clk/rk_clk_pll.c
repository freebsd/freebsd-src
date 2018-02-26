/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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

#include <dev/extres/clk/clk.h>

#include <arm64/rockchip/clk/rk_clk_pll.h>

#include "clkdev_if.h"

struct rk_clk_pll_sc {
	uint32_t	base_offset;

	uint32_t	gate_offset;
	uint32_t	gate_shift;

	uint32_t	flags;
};

#define	WRITE4(_clk, off, val)					\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	READ4(_clk, off, val)					\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)					\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)					\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	RK_CLK_PLL_DSMPD_OFFSET		4
#define	RK_CLK_PLL_DSMPD_SHIFT		12
#define	RK_CLK_PLL_DSMPD_MASK		0x1000

#define	RK_CLK_PLL_REFDIV_OFFSET	4
#define	RK_CLK_PLL_REFDIV_SHIFT		0
#define	RK_CLK_PLL_REFDIV_MASK		0x3F

#define	RK_CLK_PLL_FBDIV_OFFSET		0
#define	RK_CLK_PLL_FBDIV_SHIFT		0
#define	RK_CLK_PLL_FBDIV_MASK		0xFFF

#define	RK_CLK_PLL_POSTDIV1_OFFSET	0
#define	RK_CLK_PLL_POSTDIV1_SHIFT	12
#define	RK_CLK_PLL_POSTDIV1_MASK	0x7000

#define	RK_CLK_PLL_POSTDIV2_OFFSET	4
#define	RK_CLK_PLL_POSTDIV2_SHIFT	6
#define	RK_CLK_PLL_POSTDIV2_MASK	0x1C0

#define	RK_CLK_PLL_FRAC_OFFSET		8
#define	RK_CLK_PLL_FRAC_SHIFT		0
#define	RK_CLK_PLL_FRAC_MASK		0xFFFFFF

static int
rk_clk_pll_init(struct clknode *clk, device_t dev)
{
	struct rk_clk_pll_sc *sc;

	sc = clknode_get_softc(clk);

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
rk_clk_pll_set_gate(struct clknode *clk, bool enable)
{
	struct rk_clk_pll_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if ((sc->flags & RK_CLK_PLL_HAVE_GATE) == 0)
		return (0);

	DEVICE_LOCK(clk);
	READ4(clk, sc->gate_offset, &val);
	if (enable)
		val &= ~(1 << sc->gate_shift);
	else
		val |= 1 << sc->gate_shift;
	WRITE4(clk, sc->gate_offset, val | RK_CLK_PLL_MASK);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
rk_clk_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct rk_clk_pll_sc *sc;
	uint64_t foutvco;
	uint32_t dsmpd, refdiv, fbdiv;
	uint32_t postdiv1, postdiv2, frac;
	uint32_t raw1, raw2;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);

	READ4(clk, sc->base_offset, &raw1);
	READ4(clk, sc->base_offset + 4, &raw2);

	READ4(clk, sc->base_offset + RK_CLK_PLL_DSMPD_OFFSET, &dsmpd);
	dsmpd = (dsmpd & RK_CLK_PLL_DSMPD_MASK) >> RK_CLK_PLL_DSMPD_SHIFT;

	READ4(clk, sc->base_offset + RK_CLK_PLL_REFDIV_OFFSET, &refdiv);
	refdiv = (refdiv & RK_CLK_PLL_REFDIV_MASK) >> RK_CLK_PLL_REFDIV_SHIFT;

	READ4(clk, sc->base_offset + RK_CLK_PLL_FBDIV_OFFSET, &fbdiv);
	fbdiv = (fbdiv & RK_CLK_PLL_FBDIV_MASK) >> RK_CLK_PLL_FBDIV_SHIFT;

	READ4(clk, sc->base_offset + RK_CLK_PLL_POSTDIV1_OFFSET, &postdiv1);
	postdiv1 = (postdiv1 & RK_CLK_PLL_POSTDIV1_MASK) >> RK_CLK_PLL_POSTDIV1_SHIFT;

	READ4(clk, sc->base_offset + RK_CLK_PLL_POSTDIV2_OFFSET, &postdiv2);
	postdiv2 = (postdiv2 & RK_CLK_PLL_POSTDIV2_MASK) >> RK_CLK_PLL_POSTDIV2_SHIFT;

	READ4(clk, sc->base_offset + RK_CLK_PLL_FRAC_OFFSET, &frac);
	frac = (frac & RK_CLK_PLL_FRAC_MASK) >> RK_CLK_PLL_FRAC_SHIFT;

	DEVICE_UNLOCK(clk);

	if (dsmpd == 0) {
		/* Fractional mode */
		foutvco = *freq / refdiv * (fbdiv + frac / 224);
	} else {
		/* Integer mode */

		foutvco = *freq / refdiv * fbdiv;
	}

	*freq = foutvco / postdiv1 / postdiv2;

	return (0);
}

static clknode_method_t rk_clk_pll_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		rk_clk_pll_init),
	CLKNODEMETHOD(clknode_set_gate,		rk_clk_pll_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	rk_clk_pll_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk_clk_pll_clknode, rk_clk_pll_clknode_class,
    rk_clk_pll_clknode_methods, sizeof(struct rk_clk_pll_sc), clknode_class);

int
rk_clk_pll_register(struct clkdom *clkdom, struct rk_clk_pll_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_pll_sc *sc;

	clk = clknode_create(clkdom, &rk_clk_pll_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->base_offset = clkdef->base_offset;
	sc->gate_offset = clkdef->gate_offset;
	sc->gate_shift = clkdef->gate_shift;
	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
