/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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
#include <dev/extres/syscon/syscon.h>

#include <arm64/rockchip/clk/rk_clk_composite.h>

#include "clkdev_if.h"
#include "syscon_if.h"

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

	struct syscon	*grf;
};

#define	WRITE4(_clk, off, val)						\
	rk_clk_composite_write_4(_clk, off, val)
#define	READ4(_clk, off, val)						\
	rk_clk_composite_read_4(_clk, off, val)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	RK_CLK_COMPOSITE_MASK_SHIFT	16

#if 0
#define	dprintf(format, arg...)						\
	printf("%s:(%s)" format, __func__, clknode_get_name(clk), arg)
#else
#define	dprintf(format, arg...)
#endif

static void
rk_clk_composite_read_4(struct clknode *clk, bus_addr_t addr, uint32_t *val)
{
	struct rk_clk_composite_sc *sc;

	sc = clknode_get_softc(clk);
	if (sc->grf)
		*val = SYSCON_READ_4(sc->grf, addr);
	else
		CLKDEV_READ_4(clknode_get_device(clk), addr, val);
}

static void
rk_clk_composite_write_4(struct clknode *clk, bus_addr_t addr, uint32_t val)
{
	struct rk_clk_composite_sc *sc;

	sc = clknode_get_softc(clk);
	if (sc->grf)
		SYSCON_WRITE_4(sc->grf, addr, val | (0xffff << 16));
	else
		CLKDEV_WRITE_4(clknode_get_device(clk), addr, val);
}

static struct syscon *
rk_clk_composite_get_grf(struct clknode *clk)
{
	device_t dev;
	phandle_t node;
	struct syscon *grf;

	grf = NULL;
	dev = clknode_get_device(clk);
	node = ofw_bus_get_node(dev);
	if (OF_hasprop(node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &grf) != 0) {
		return (NULL);
        }

	return (grf);
}

static int
rk_clk_composite_init(struct clknode *clk, device_t dev)
{
	struct rk_clk_composite_sc *sc;
	uint32_t val, idx;

	sc = clknode_get_softc(clk);
	if ((sc->flags & RK_CLK_COMPOSITE_GRF) != 0) {
		sc->grf = rk_clk_composite_get_grf(clk);
		if (sc->grf == NULL)
			panic("clock %s has GRF flag set but no syscon is available",
			    clknode_get_name(clk));
	}

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
	uint32_t val = 0;

	sc = clknode_get_softc(clk);

	if ((sc->flags & RK_CLK_COMPOSITE_HAVE_GATE) == 0)
		return (0);

	dprintf("%sabling gate\n", enable ? "En" : "Dis");
	if (!enable)
		val |= 1 << sc->gate_shift;
	dprintf("sc->gate_shift: %x\n", sc->gate_shift);
	val |= (1 << sc->gate_shift) << RK_CLK_COMPOSITE_MASK_SHIFT;
	dprintf("Write: gate_offset=%x, val=%x\n", sc->gate_offset, val);
	DEVICE_LOCK(clk);
	WRITE4(clk, sc->gate_offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
rk_clk_composite_set_mux(struct clknode *clk, int index)
{
	struct rk_clk_composite_sc *sc;
	uint32_t val = 0;

	sc = clknode_get_softc(clk);

	if ((sc->flags & RK_CLK_COMPOSITE_HAVE_MUX) == 0)
		return (0);

	dprintf("Set mux to %d\n", index);
	DEVICE_LOCK(clk);
	val |= (index << sc->mux_shift);
	val |= sc->mux_mask << RK_CLK_COMPOSITE_MASK_SHIFT;
	dprintf("Write: muxdiv_offset=%x, val=%x\n", sc->muxdiv_offset, val);
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
	dprintf("Read: muxdiv_offset=%x, val=%x\n", sc->muxdiv_offset, reg);

	DEVICE_UNLOCK(clk);

	div = ((reg & sc->div_mask) >> sc->div_shift);
	if (sc->flags & RK_CLK_COMPOSITE_DIV_EXP)
		div = 1 << div;
	else
		div += 1;
	dprintf("parent_freq=%ju, div=%u\n", *freq, div);
	*freq = *freq / div;
	dprintf("Final freq=%ju\n", *freq);
	return (0);
}

static uint32_t
rk_clk_composite_find_best(struct rk_clk_composite_sc *sc, uint64_t fparent,
    uint64_t freq, uint32_t *reg)
{
	uint64_t best, cur;
	uint32_t best_div, best_div_reg;
	uint32_t div, div_reg;

	best = 0;
	best_div = 0;
	best_div_reg = 0;

	for (div_reg = 0;  div_reg <= ((sc->div_mask >> sc->div_shift) + 1);
	    div_reg++) {
		if (sc->flags == RK_CLK_COMPOSITE_DIV_EXP)
			div = 1 << div_reg;
		else
			div = div_reg + 1;
		cur = fparent / div;
		if ((freq - cur) < (freq - best)) {
			best = cur;
			best_div = div;
			best_div_reg = div_reg;
			break;
		}
	}
	*reg = div_reg;
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
	uint32_t div, div_reg, best_div, best_div_reg, val;
	int p_idx, best_parent;

	sc = clknode_get_softc(clk);
	dprintf("Finding best parent/div for target freq of %ju\n", *fout);
	p_names = clknode_get_parent_names(clk);
	for (best_div = 0, best = 0, p_idx = 0;
	     p_idx != clknode_get_parents_num(clk); p_idx++) {
		p_clk = clknode_find_by_name(p_names[p_idx]);
		clknode_get_freq(p_clk, &fparent);
		dprintf("Testing with parent %s (%d) at freq %ju\n",
		    clknode_get_name(p_clk), p_idx, fparent);
		div = rk_clk_composite_find_best(sc, fparent, *fout, &div_reg);
		cur = fparent / div;
		if ((*fout - cur) < (*fout - best)) {
			best = cur;
			best_div = div;
			best_div_reg = div_reg;
			best_parent = p_idx;
			dprintf("Best parent so far %s (%d) with best freq at "
			    "%ju\n", clknode_get_name(p_clk), p_idx, best);
		}
	}

	*stop = 1;
	if (best_div == 0)
		return (ERANGE);

	if ((best < *fout) && ((flags & CLK_SET_ROUND_DOWN) == 0))
		return (ERANGE);

	if ((best > *fout) && ((flags & CLK_SET_ROUND_UP) == 0)) {
		return (ERANGE);
	}

	if ((flags & CLK_SET_DRYRUN) != 0) {
		*fout = best;
		return (0);
	}

	p_idx = clknode_get_parent_idx(clk);
	if (p_idx != best_parent) {
		dprintf("Switching parent index from %d to %d\n", p_idx,
		    best_parent);
		clknode_set_parent_by_idx(clk, best_parent);
	}

	dprintf("Setting divider to %d (reg: %d)\n", best_div, best_div_reg);
	dprintf(" div_mask: 0x%X, div_shift: %d\n", sc->div_mask,
	    sc->div_shift);

	DEVICE_LOCK(clk);
	val = best_div_reg << sc->div_shift;
	val |= sc->div_mask << RK_CLK_COMPOSITE_MASK_SHIFT;
	dprintf("Write: muxdiv_offset=%x, val=%x\n", sc->muxdiv_offset, val);
	WRITE4(clk, sc->muxdiv_offset, val);
	DEVICE_UNLOCK(clk);

	*fout = best;
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
rk_clk_composite_register(struct clkdom *clkdom,
    struct rk_clk_composite_def *clkdef)
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
