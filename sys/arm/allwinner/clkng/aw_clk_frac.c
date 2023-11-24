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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/extres/clk/clk.h>

#include <arm/allwinner/clkng/aw_clk.h>
#include <arm/allwinner/clkng/aw_clk_frac.h>

#include "clkdev_if.h"

/* #define	dprintf(format, arg...)	printf("%s:(%s)" format, __func__, clknode_get_name(clk), arg) */
#define	dprintf(format, arg...)

/*
 * clknode for clocks matching the formula :
 *
 * clk = (24Mhz * n) / m in integer mode
 * clk = frac_out1 or frac_out2 in fractional mode
 *
 */

struct aw_clk_frac_sc {
	uint32_t	offset;

	struct aw_clk_factor	m;
	struct aw_clk_factor	n;
	struct aw_clk_frac	frac;

	uint64_t		min_freq;
	uint64_t		max_freq;

	uint32_t	mux_shift;
	uint32_t	mux_mask;
	uint32_t	gate_shift;
	uint32_t	lock_shift;
	uint32_t	lock_retries;

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
aw_clk_frac_init(struct clknode *clk, device_t dev)
{
	struct aw_clk_frac_sc *sc;
	uint32_t val, idx;

	sc = clknode_get_softc(clk);

	idx = 0;
	if ((sc->flags & AW_CLK_HAS_MUX) != 0) {
		DEVICE_LOCK(clk);
		READ4(clk, sc->offset, &val);
		DEVICE_UNLOCK(clk);

		idx = (val & sc->mux_mask) >> sc->mux_shift;
	}

	dprintf("init parent idx %d\n", idx);
	clknode_init_parent_idx(clk, idx);
	return (0);
}

static int
aw_clk_frac_set_gate(struct clknode *clk, bool enable)
{
	struct aw_clk_frac_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if ((sc->flags & AW_CLK_HAS_GATE) == 0)
		return (0);

	dprintf("%sabling gate\n", enable ? "En" : "Dis");
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
aw_clk_frac_set_mux(struct clknode *clk, int index)
{
	struct aw_clk_frac_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if ((sc->flags & AW_CLK_HAS_MUX) == 0)
		return (0);

	dprintf("Set mux to %d\n", index);
	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	val &= ~sc->mux_mask;
	val |= index << sc->mux_shift;
	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static uint64_t
aw_clk_frac_find_best(struct aw_clk_frac_sc *sc, uint64_t fparent, uint64_t fout,
    uint32_t *factor_n, uint32_t *factor_m)
{
	uint64_t cur, best;
	uint32_t m, n, max_m, max_n, min_m, min_n;

	*factor_n = *factor_m = 0;
	best = cur = 0;

	max_m = aw_clk_factor_get_max(&sc->m);
	max_n = aw_clk_factor_get_max(&sc->n);
	min_m = aw_clk_factor_get_min(&sc->m);
	min_n = sc->min_freq / fparent;

	for (n = min_n; n <= max_n; n++) {
		for (m = min_m; m <= max_m; m++) {
			cur = fparent * n / m;
			if (cur < sc->min_freq) {
				continue;
			}
			if (cur > sc->max_freq) {
				continue;
			}
			if (cur == fout) {
				*factor_n = n;
				*factor_m = m;
				return (cur);
			}
			if (abs((fout - cur)) < abs((fout - best))) {
				best = cur;
				*factor_n = n;
				*factor_m = m;
			}
		}
	}

	return (best);
}

static int
aw_clk_frac_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct aw_clk_frac_sc *sc;
	uint64_t cur, best, best_frac;
	uint32_t val, m, n, best_m, best_n;
	int retry, multiple, max_mult, best_mult;

	sc = clknode_get_softc(clk);

	best = best_frac = cur = 0;
	best_mult = 0;
	max_mult = 1;

	dprintf("Trying to find freq %ju with parent %ju\n", *fout, fparent);
	if ((flags & CLK_SET_ROUND_MULTIPLE) != 0)
		max_mult = 10;
	for (multiple = 1; multiple <= max_mult; multiple++) {
		/* First test the fractional frequencies */
		dprintf("Testing with multiple %d\n", multiple);
		if (*fout * multiple == sc->frac.freq0) {
			best = best_frac = sc->frac.freq0;
			best_mult = multiple;
			dprintf("Found with using frac.freq0 and multiple %d\n", multiple);
			break;
		}
		else if (*fout * multiple == sc->frac.freq1) {
			best = best_frac = sc->frac.freq1;
			best_mult = multiple;
			dprintf("Found with using frac.freq1 and multiple %d\n", multiple);
			break;
		}
		else {
			cur = aw_clk_frac_find_best(sc, fparent, *fout * multiple,
			  &n, &m);
			dprintf("Got %ju with n=%d, m=%d\n", cur, n, m);
			if (cur == (*fout * multiple)) {
				best = cur;
				best_mult = multiple;
				best_n = n;
				best_m = m;
				dprintf("This is the one: n=%d m=%d mult=%d\n", best_n, best_m, best_mult);
				break;
			}
			if (abs(((*fout * multiple) - cur)) < abs(((*fout * multiple) - best))) {
				best = cur;
				best_mult = multiple;
				best_n = n;
				best_m = m;
				dprintf("This is the best for now: n=%d m=%d mult=%d\n", best_n, best_m, best_mult);
			}
		}
	}

	if (best < sc->min_freq ||
	    best > sc->max_freq) {
		printf("%s: Cannot set %ju for %s (min=%ju max=%ju)\n",
		    __func__, best, clknode_get_name(clk),
		    sc->min_freq, sc->max_freq);
		return (ERANGE);
	}
	if ((flags & CLK_SET_DRYRUN) != 0) {
		*fout = best;
		*stop = 1;
		return (0);
	}

	if ((best < (*fout * best_mult)) &&
	  ((flags & CLK_SET_ROUND_DOWN) == 0)) {
		*stop = 1;
		return (ERANGE);
	}
	if ((best > *fout * best_mult) &&
	  ((flags & CLK_SET_ROUND_UP) == 0)) {
		*stop = 1;
		return (ERANGE);
	}

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	/* Disable clock during freq changes */
	val &= ~(1 << sc->gate_shift);
	WRITE4(clk, sc->offset, val);

	if (best_frac != 0) {
		val &= ~sc->frac.mode_sel;
		/* M should be 0 per the manual */
		val &= ~sc->m.mask;
		if (best_frac == sc->frac.freq0)
			val &= ~sc->frac.freq_sel;
		else
			val |= sc->frac.freq_sel;
	} else {
		val |= sc->frac.mode_sel; /* Select integer mode */
		n = aw_clk_factor_get_value(&sc->n, best_n);
		m = aw_clk_factor_get_value(&sc->m, best_m);
		val &= ~sc->n.mask;
		val &= ~sc->m.mask;
		val |= n << sc->n.shift;
		val |= m << sc->m.shift;
	}

	/* Write the clock changes */
	WRITE4(clk, sc->offset, val);

	/* Enable clock now that we've change it */
	val |= 1 << sc->gate_shift;
	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	for (retry = 0; retry < sc->lock_retries; retry++) {
		READ4(clk, sc->offset, &val);
		if ((val & (1 << sc->lock_shift)) != 0)
			break;
		DELAY(1000);
	}

	*fout = best;
	*stop = 1;

	return (0);
}

static int
aw_clk_frac_recalc(struct clknode *clk, uint64_t *freq)
{
	struct aw_clk_frac_sc *sc;
	uint32_t val, m, n;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	DEVICE_UNLOCK(clk);

	if ((val & sc->frac.mode_sel) == 0) {
		if (val & sc->frac.freq_sel)
			*freq = sc->frac.freq1;
		else
			*freq = sc->frac.freq0;
	} else {
		m = aw_clk_get_factor(val, &sc->m);
		n = aw_clk_get_factor(val, &sc->n);
		*freq = *freq * n / m;
	}

	return (0);
}

static clknode_method_t aw_frac_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		aw_clk_frac_init),
	CLKNODEMETHOD(clknode_set_gate,		aw_clk_frac_set_gate),
	CLKNODEMETHOD(clknode_set_mux,		aw_clk_frac_set_mux),
	CLKNODEMETHOD(clknode_recalc_freq,	aw_clk_frac_recalc),
	CLKNODEMETHOD(clknode_set_freq,		aw_clk_frac_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(aw_frac_clknode, aw_frac_clknode_class, aw_frac_clknode_methods,
    sizeof(struct aw_clk_frac_sc), clknode_class);

int
aw_clk_frac_register(struct clkdom *clkdom, struct aw_clk_frac_def *clkdef)
{
	struct clknode *clk;
	struct aw_clk_frac_sc *sc;

	clk = clknode_create(clkdom, &aw_frac_clknode_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->offset = clkdef->offset;

	sc->m.shift = clkdef->m.shift;
	sc->m.width = clkdef->m.width;
	sc->m.mask = ((1 << sc->m.width) - 1) << sc->m.shift;
	sc->m.value = clkdef->m.value;
	sc->m.flags = clkdef->m.flags;

	sc->n.shift = clkdef->n.shift;
	sc->n.width = clkdef->n.width;
	sc->n.mask = ((1 << sc->n.width) - 1) << sc->n.shift;
	sc->n.value = clkdef->n.value;
	sc->n.flags = clkdef->n.flags;

	sc->frac.freq0 = clkdef->frac.freq0;
	sc->frac.freq1 = clkdef->frac.freq1;
	sc->frac.mode_sel = 1 << clkdef->frac.mode_sel;
	sc->frac.freq_sel = 1 << clkdef->frac.freq_sel;

	sc->min_freq = clkdef->min_freq;
	sc->max_freq = clkdef->max_freq;

	sc->mux_shift = clkdef->mux_shift;
	sc->mux_mask = ((1 << clkdef->mux_width) - 1) << sc->mux_shift;

	sc->gate_shift = clkdef->gate_shift;

	sc->lock_shift = clkdef->lock_shift;
	sc->lock_retries = clkdef->lock_retries;

	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
