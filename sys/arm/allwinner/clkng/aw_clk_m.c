/*-
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

#include <dev/extres/clk/clk.h>

#include <arm/allwinner/clkng/aw_clk.h>
#include <arm/allwinner/clkng/aw_clk_m.h>

#include "clkdev_if.h"

/*
 * clknode for clocks matching the formula :
 *
 * clk = clkin / m
 * And that needs to potentially :
 * 1) Set the parent freq
 * 2) Support Setting the parent to a multiple
 *
 */

struct aw_clk_m_sc {
	uint32_t	offset;

	struct aw_clk_factor	m;

	uint32_t	mux_shift;
	uint32_t	mux_mask;
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

static int
aw_clk_m_init(struct clknode *clk, device_t dev)
{
	struct aw_clk_m_sc *sc;
	uint32_t val, idx;

	sc = clknode_get_softc(clk);

	idx = 0;
	if ((sc->flags & AW_CLK_HAS_MUX) != 0) {
		DEVICE_LOCK(clk);
		READ4(clk, sc->offset, &val);
		DEVICE_UNLOCK(clk);

		idx = (val & sc->mux_mask) >> sc->mux_shift;
	}

	clknode_init_parent_idx(clk, idx);
	return (0);
}

static int
aw_clk_m_set_gate(struct clknode *clk, bool enable)
{
	struct aw_clk_m_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if ((sc->flags & AW_CLK_HAS_GATE) == 0)
		return (0);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	if (enable)
		val |= (1 << sc->gate_shift);
	else
		val &= ~(1 << sc->gate_shift);
	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
aw_clk_m_set_mux(struct clknode *clk, int index)
{
	struct aw_clk_m_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if ((sc->flags & AW_CLK_HAS_MUX) == 0)
		return (0);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	val &= ~sc->mux_mask;
	val |= index << sc->mux_shift;
	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static uint64_t
aw_clk_m_find_best(struct aw_clk_m_sc *sc, uint64_t fparent, uint64_t *fout,
    uint32_t *factor_m)
{
	uint64_t cur, best;
	uint32_t m, max_m, min_m;

	*factor_m = 0;

	max_m = aw_clk_factor_get_max(&sc->m);
	min_m = aw_clk_factor_get_min(&sc->m);

	for (m = min_m; m <= max_m; ) {
		cur = fparent / m;
		if (abs(*fout - cur) < abs(*fout - best)) {
			best = cur;
			*factor_m = m;
		}
		if ((sc->m.flags & AW_CLK_FACTOR_POWER_OF_TWO) != 0)
			m <<= 1;
		else
			m++;
	}

	return (best);
}

static int
aw_clk_m_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct aw_clk_m_sc *sc;
	struct clknode *p_clk;
	uint64_t cur, best;
	uint32_t val, m, best_m;

	sc = clknode_get_softc(clk);

	best = cur = 0;

	best = aw_clk_m_find_best(sc, fparent, fout,
	    &best_m);
	if ((best != *fout) && ((sc->flags & AW_CLK_SET_PARENT) != 0)) {
		p_clk = clknode_get_parent(clk);
		if (p_clk == NULL) {
			printf("%s: Cannot get parent for clock %s\n",
			    __func__,
			    clknode_get_name(clk));
			return (ENXIO);
		}
		clknode_set_freq(p_clk, *fout, CLK_SET_ROUND_MULTIPLE, 0);
		clknode_get_freq(p_clk, &fparent);
		best = aw_clk_m_find_best(sc, fparent, fout,
		    &best_m);
	}

	if ((flags & CLK_SET_DRYRUN) != 0) {
		*fout = best;
		*stop = 1;
		return (0);
	}

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

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);

	m = aw_clk_factor_get_value(&sc->m, best_m);
	val &= ~sc->m.mask;
	val |= m << sc->m.shift;

	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	*fout = best;
	*stop = 1;

	return (0);
}

static int
aw_clk_m_recalc(struct clknode *clk, uint64_t *freq)
{
	struct aw_clk_m_sc *sc;
	uint32_t val, m;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	DEVICE_UNLOCK(clk);

	m = aw_clk_get_factor(val, &sc->m);

	*freq = *freq / m;

	return (0);
}

static clknode_method_t aw_m_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		aw_clk_m_init),
	CLKNODEMETHOD(clknode_set_gate,		aw_clk_m_set_gate),
	CLKNODEMETHOD(clknode_set_mux,		aw_clk_m_set_mux),
	CLKNODEMETHOD(clknode_recalc_freq,	aw_clk_m_recalc),
	CLKNODEMETHOD(clknode_set_freq,		aw_clk_m_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(aw_m_clknode, aw_m_clknode_class, aw_m_clknode_methods,
    sizeof(struct aw_clk_m_sc), clknode_class);

int
aw_clk_m_register(struct clkdom *clkdom, struct aw_clk_m_def *clkdef)
{
	struct clknode *clk;
	struct aw_clk_m_sc *sc;

	clk = clknode_create(clkdom, &aw_m_clknode_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->offset = clkdef->offset;

	sc->m.shift = clkdef->m.shift;
	sc->m.width = clkdef->m.width;
	sc->m.mask = ((1 << sc->m.width) - 1) << sc->m.shift;
	sc->m.value = clkdef->m.value;
	sc->m.flags = clkdef->m.flags;

	sc->mux_shift = clkdef->mux_shift;
	sc->mux_mask = ((1 << clkdef->mux_width) - 1) << sc->mux_shift;

	sc->gate_shift = clkdef->gate_shift;

	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
