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
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/clk/clk.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clkdev_if.h"

#include "a37x0_tbg_pll.h"

#define RD4(_clk, offset, val)			\
	CLKDEV_READ_4(clknode_get_device(_clk), offset, val)

struct a37x0_tbg_pll_softc {
	struct a37x0_tbg_pll_reg_def		vcodiv;
	struct a37x0_tbg_pll_reg_def		refdiv;
	struct a37x0_tbg_pll_reg_def		fbdiv;
	struct a37x0_tbg_pll_reg_def		tbg_bypass;
};

static int
a37x0_tbg_pll_recalc_freq(struct clknode *clk, uint64_t *freq)
{
	struct a37x0_tbg_pll_softc *sc;
	uint32_t vcodiv, fbdiv, refdiv;
	unsigned int val;

	sc = clknode_get_softc(clk);

	RD4(clk, sc->tbg_bypass.offset, &val);
	if ((val >> sc->tbg_bypass.shift) & sc->tbg_bypass.mask)
		return 0;

	RD4(clk, sc->vcodiv.offset, &val);
	vcodiv = 1 << ((val >> sc->vcodiv.shift) & sc->vcodiv.mask);

	RD4(clk, sc->refdiv.offset, &val);
	refdiv = (val >> sc->refdiv.shift) & sc->refdiv.mask;

	RD4(clk, sc->fbdiv.offset, &val);
	fbdiv = (val >> sc->fbdiv.shift) & sc->fbdiv.mask;

	if (refdiv == 0)
		refdiv = 1;

	*freq = *freq * (fbdiv / refdiv) * 4;
	*freq /= vcodiv;

	return (0);
}

static int
a37x0_tbg_pll_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static clknode_method_t a37x0_tbg_pll_clknode_methods[] = {
	CLKNODEMETHOD(clknode_recalc_freq,	a37x0_tbg_pll_recalc_freq),
	CLKNODEMETHOD(clknode_init,		a37x0_tbg_pll_init),

	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(a37x0_tbg_pll__clknode, a37x0_tbg_pll_clknode_class,
    a37x0_tbg_pll_clknode_methods, sizeof(struct a37x0_tbg_pll_softc),
    clknode_class);

int
a37x0_tbg_pll_clk_register(struct clkdom *clkdom,
    const struct a37x0_tbg_pll_clk_def *clkdef)
{
	struct a37x0_tbg_pll_softc *sc;
	struct clknode *clk;

	clk = clknode_create(clkdom, &a37x0_tbg_pll_clknode_class,
	    &clkdef->clkdef);

	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->vcodiv = clkdef->vcodiv;
	sc->refdiv = clkdef->refdiv;
	sc->fbdiv = clkdef->fbdiv;
	sc->tbg_bypass = clkdef->tbg_bypass;

	if (clknode_register(clkdom, clk) == NULL)
		return (1);

	return (0);
}
