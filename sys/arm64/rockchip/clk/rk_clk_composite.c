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

#include <arm64/rockchip/clk/rk_clk_composite.h>

#include "clkdev_if.h"

struct rk_clk_composite_sc {
	uint32_t	muxdiv_offset;
	uint32_t	mux_shift;
	uint32_t	mux_width;
	uint32_t	mux_mask;

	uint32_t	div_shift;
	uint32_t	div_width;
	uint32_t	div_mask;

	uint32_t	gate_offset;
	uint32_t	gate_shift;

	uint32_t	flags;
};

#define	WRITE4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	READ4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)							\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	RK_COMPOSITE_WRITE_MASK	0xFFFF0000

static int
rk_clk_composite_init(struct clknode *clk, device_t dev)
{
	struct rk_clk_composite_sc *sc;
	uint32_t val, idx;

	sc = clknode_get_softc(clk);

	idx = 0;
	if ((sc->flags & RK_CLK_COMPOSITE_HAVE_MUX) != 0) {
		DEVICE_LOCK(clk);
		READ4(clk, sc->muxdiv_offset, &val);
		DEVICE_UNLOCK(clk);

		idx = (val & sc->mux_mask) >> sc->mux_shift;
	}

	clknode_init_parent_idx(clk, idx);

	return (0);
}

static int
rk_clk_composite_set_gate(struct clknode *clk, bool enable)
{
	struct rk_clk_composite_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if ((sc->flags & RK_CLK_COMPOSITE_HAVE_GATE) == 0)
		return (0);

	DEVICE_LOCK(clk);
	READ4(clk, sc->gate_offset, &val);
	if (enable)
		val &= ~(1 << sc->gate_shift);
	else
		val |= 1 << sc->gate_shift;
	WRITE4(clk, sc->gate_offset, val | RK_CLK_COMPOSITE_MASK);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
rk_clk_composite_set_mux(struct clknode *clk, int index)
{
	struct rk_clk_composite_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if ((sc->flags & RK_CLK_COMPOSITE_HAVE_MUX) == 0)
		return (0);

	DEVICE_LOCK(clk);
	READ4(clk, sc->muxdiv_offset, &val);
	val &= ~(sc->mux_mask >> sc->mux_shift);
	val |= index << sc->mux_shift;
	WRITE4(clk, sc->muxdiv_offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
rk_clk_composite_recalc(struct clknode *clk, uint64_t *freq)
{
	struct rk_clk_composite_sc *sc;
	uint32_t reg, div;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);

	READ4(clk, sc->muxdiv_offset, &reg);

	DEVICE_UNLOCK(clk);

	div = ((reg & sc->div_mask) >> sc->div_shift) + 1;

	*freq = *freq / div;

	return (0);
}

static uint32_t
rk_clk_composite_find_best(struct rk_clk_composite_sc *sc, uint64_t fparent,
    uint64_t freq)
{
	uint64_t best, cur;
	uint32_t best_div, div;

	for (best = 0, best_div = 0, div = 0;
	     div <= ((sc->div_mask >> sc->div_shift) + 1); div++) {
		cur = fparent / div;
		if ((freq - cur) < (freq - best)) {
			best = cur;
			best_div = div;
			break;
		}
	}

	return (best_div);
}

static int
rk_clk_composite_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct rk_clk_composite_sc *sc;
	struct clknode *p_clk;
	const char **p_names;
	uint64_t best, cur;
	uint32_t div, best_div, val;
	int p_idx, best_parent;

	sc = clknode_get_softc(clk);

	p_names = clknode_get_parent_names(clk);
	for (best_div = 0, best = 0, p_idx = 0;
	     p_idx != clknode_get_parents_num(clk); p_idx++) {
		p_clk = clknode_find_by_name(p_names[p_idx]);
		clknode_get_freq(p_clk, &fparent);
		div = rk_clk_composite_find_best(sc, fparent, *fout);
		cur = fparent / div;
		if ((*fout - cur) < (*fout - best)) {
			best = cur;
			best_div = div;
			best_parent = p_idx;
		}
	}

	if (best_div == 0)
		return (0);

	if ((best < *fout) &&
	  ((flags & CLK_SET_ROUND_DOWN) == 0)) {
		*stop = 1;
		return (ERANGE);
	}
	if ((best > *fout) &&
	  ((flags & CLK_SET_ROUND_UP) == 0)) {
		*stop = 1;
		return (ERANGE);
	}

	if ((flags & CLK_SET_DRYRUN) != 0) {
		*fout = best;
		*stop = 1;
		return (0);
	}

	if (p_idx != best_parent)
		clknode_set_parent_by_idx(clk, best_parent);

	DEVICE_LOCK(clk);
	READ4(clk, sc->muxdiv_offset, &val);
	val &= ~sc->div_mask;
	val |= (best_div - 1) << sc->div_shift;
	WRITE4(clk, sc->muxdiv_offset, val | RK_CLK_COMPOSITE_MASK);
	DEVICE_UNLOCK(clk);

	*fout = best;
	*stop = 1;

	return (0);
}

static clknode_method_t rk_clk_composite_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		rk_clk_composite_init),
	CLKNODEMETHOD(clknode_set_gate,		rk_clk_composite_set_gate),
	CLKNODEMETHOD(clknode_set_mux,		rk_clk_composite_set_mux),
	CLKNODEMETHOD(clknode_recalc_freq,	rk_clk_composite_recalc),
	CLKNODEMETHOD(clknode_set_freq,		rk_clk_composite_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk_clk_composite_clknode, rk_clk_composite_clknode_class,
    rk_clk_composite_clknode_methods, sizeof(struct rk_clk_composite_sc),
    clknode_class);

int
rk_clk_composite_register(struct clkdom *clkdom, struct rk_clk_composite_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_composite_sc *sc;

	clk = clknode_create(clkdom, &rk_clk_composite_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->muxdiv_offset = clkdef->muxdiv_offset;

	sc->mux_shift = clkdef->mux_shift;
	sc->mux_width = clkdef->mux_width;
	sc->mux_mask = ((1 << clkdef->mux_width) - 1) << sc->mux_shift;

	sc->div_shift = clkdef->div_shift;
	sc->div_width = clkdef->div_width;
	sc->div_mask = ((1 << clkdef->div_width) - 1) << sc->div_shift;

	sc->gate_offset = clkdef->gate_offset;
	sc->gate_shift = clkdef->gate_shift;

	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
