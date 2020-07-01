/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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

#include <arm64/freescale/imx/clk/imx_clk_sscg_pll.h>

#include "clkdev_if.h"

struct imx_clk_sscg_pll_sc {
	uint32_t	offset;
};

#define	WRITE4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	READ4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	CFG0	0x00
#define	 CFG0_PLL_LOCK	(1 << 31)
#define	 CFG0_PD	(1 << 7)
#define	 CFG0_BYPASS2	(1 << 5)
#define	 CFG0_BYPASS1	(1 << 4)
#define	CFG1	0x04
#define	CFG2	0x08
#define	 CFG2_DIVR1_MASK	(7 << 25)
#define	 CFG2_DIVR1_SHIFT	25
#define	 CFG2_DIVR2_MASK	(0x3f << 19)
#define	 CFG2_DIVR2_SHIFT	19
#define	 CFG2_DIVF1_MASK	(0x3f << 13)
#define	 CFG2_DIVF1_SHIFT	13
#define	 CFG2_DIVF2_MASK	(0x3f << 7)
#define	 CFG2_DIVF2_SHIFT	7
#define	 CFG2_DIV_MASK		(0x3f << 1)
#define	 CFG2_DIV_SHIFT		1

#if 0
#define	dprintf(format, arg...)						\
	printf("%s:(%s)" format, __func__, clknode_get_name(clk), arg)
#else
#define	dprintf(format, arg...)
#endif

static int
imx_clk_sscg_pll_init(struct clknode *clk, device_t dev)
{
	struct imx_clk_sscg_pll_sc *sc;

	sc = clknode_get_softc(clk);
	if (clknode_get_parents_num(clk) > 1) {
		device_printf(clknode_get_device(clk),
		   "error: SSCG PLL does not support more than one parent yet\n");
		return (EINVAL);
	}
	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
imx_clk_sscg_pll_set_gate(struct clknode *clk, bool enable)
{
	struct imx_clk_sscg_pll_sc *sc;
	uint32_t cfg0;
	int timeout;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset + CFG0, &cfg0);
	if (enable)
		cfg0 &= ~(CFG0_PD);
	else
		cfg0 |= CFG0_PD;
	WRITE4(clk, sc->offset + CFG0, cfg0);

	/* Reading lock */
	if (enable) {
		for (timeout = 1000; timeout; timeout--) {
			READ4(clk, sc->offset + CFG0, &cfg0);
			if (cfg0 & CFG0_PLL_LOCK)
				break;
			DELAY(1);
		}
	}

	DEVICE_UNLOCK(clk);

	return (0);
}

static int
imx_clk_sscg_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct imx_clk_sscg_pll_sc *sc;
	uint32_t cfg0, cfg2;
	int divr1, divr2, divf1, divf2, div;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset + CFG0, &cfg0);
	READ4(clk, sc->offset + CFG2, &cfg2);
	DEVICE_UNLOCK(clk);

	/* PLL is bypassed */
	if (cfg0 & CFG0_BYPASS2)
		return (0);

	divr1 = (cfg2 & CFG2_DIVR1_MASK) >> CFG2_DIVR1_SHIFT;
	divr2 = (cfg2 & CFG2_DIVR2_MASK) >> CFG2_DIVR2_SHIFT;
	divf1 = (cfg2 & CFG2_DIVF1_MASK) >> CFG2_DIVF1_SHIFT;
	divf2 = (cfg2 & CFG2_DIVF2_MASK) >> CFG2_DIVF2_SHIFT;
	div = (cfg2 & CFG2_DIV_MASK) >> CFG2_DIV_SHIFT;

	if (cfg0 & CFG0_BYPASS1) {
		*freq = *freq / ((divr2 + 1) * (div + 1));
		return (0);
	}

	*freq *= 2 * (divf1 + 1) * (divf2 + 1);
	*freq /= (divr1 + 1) * (divr2 + 1) * (div + 1);

	return (0);
}

static clknode_method_t imx_clk_sscg_pll_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		imx_clk_sscg_pll_init),
	CLKNODEMETHOD(clknode_set_gate,		imx_clk_sscg_pll_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	imx_clk_sscg_pll_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(imx_clk_sscg_pll_clknode, imx_clk_sscg_pll_clknode_class,
    imx_clk_sscg_pll_clknode_methods, sizeof(struct imx_clk_sscg_pll_sc),
    clknode_class);

int
imx_clk_sscg_pll_register(struct clkdom *clkdom,
    struct imx_clk_sscg_pll_def *clkdef)
{
	struct clknode *clk;
	struct imx_clk_sscg_pll_sc *sc;

	clk = clknode_create(clkdom, &imx_clk_sscg_pll_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->offset = clkdef->offset;

	clknode_register(clkdom, clk);

	return (0);
}
