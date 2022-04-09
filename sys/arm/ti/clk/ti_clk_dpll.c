/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@freebsd.org>
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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
 * based on sys/arm/allwinner/clkng/aw_clk_np.c
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/extres/clk/clk.h>

#include <arm/ti/clk/ti_clk_dpll.h>

#include "clkdev_if.h"

/*
 * clknode for clocks matching the formula :
 *
 * clk = clkin * n / p
 *
 */

struct ti_dpll_clknode_sc {
	uint32_t		ti_clkmode_offset; /* control */
	uint8_t			ti_clkmode_flags;

	uint32_t		ti_idlest_offset;

	uint32_t		ti_clksel_offset; /* mult-div1 */
	struct ti_clk_factor	n; /* ti_clksel_mult */
	struct ti_clk_factor	p; /* ti_clksel_div */

	uint32_t		ti_autoidle_offset;
};

#define	WRITE4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	READ4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

static int
ti_dpll_clk_init(struct clknode *clk, device_t dev)
{
	clknode_init_parent_idx(clk, 0);
	return (0);
}

/* helper to keep aw_clk_np_find_best "intact" */
static inline uint32_t
ti_clk_factor_get_max(struct ti_clk_factor *factor)
{
	uint32_t max;

	if (factor->flags & TI_CLK_FACTOR_FIXED)
		max = factor->value;
	else {
		max = (1 << factor->width);
	}

	return (max);
}

static inline uint32_t
ti_clk_factor_get_min(struct ti_clk_factor *factor)
{
	uint32_t min;

	if (factor->flags & TI_CLK_FACTOR_FIXED)
		min = factor->value;
	else if (factor->flags & TI_CLK_FACTOR_ZERO_BASED)
		min = 0;
	else if (factor->flags & TI_CLK_FACTOR_MIN_VALUE)
		min = factor->min_value;
	else
		min = 1;

	return (min);
}

static uint64_t
ti_dpll_clk_find_best(struct ti_dpll_clknode_sc *sc, uint64_t fparent,
	uint64_t *fout, uint32_t *factor_n, uint32_t *factor_p)
{
	uint64_t cur, best;
	uint32_t n, p, max_n, max_p, min_n, min_p;

	*factor_n = *factor_p = 0;

	max_n = ti_clk_factor_get_max(&sc->n);
	max_p = ti_clk_factor_get_max(&sc->p);
	min_n = ti_clk_factor_get_min(&sc->n);
	min_p = ti_clk_factor_get_min(&sc->p);

	for (p = min_p; p <= max_p; ) {
		for (n = min_n; n <= max_n; ) {
			cur = fparent * n / p;
			if (abs(*fout - cur) < abs(*fout - best)) {
				best = cur;
				*factor_n = n;
				*factor_p = p;
			}

			n++;
		}
		p++;
	}

	return (best);
}

static inline uint32_t
ti_clk_get_factor(uint32_t val, struct ti_clk_factor *factor)
{
	uint32_t factor_val;

	if (factor->flags & TI_CLK_FACTOR_FIXED)
		return (factor->value);

	factor_val = (val & factor->mask) >> factor->shift;
	if (!(factor->flags & TI_CLK_FACTOR_ZERO_BASED))
		factor_val += 1;

	return (factor_val);
}

static inline uint32_t
ti_clk_factor_get_value(struct ti_clk_factor *factor, uint32_t raw)
{
	uint32_t val;

	if (factor->flags & TI_CLK_FACTOR_FIXED)
		return (factor->value);

	if (factor->flags & TI_CLK_FACTOR_ZERO_BASED)
		val = raw;
	else if (factor->flags & TI_CLK_FACTOR_MAX_VALUE &&
	    raw > factor->max_value)
		val = factor->max_value;
	else
		val = raw - 1;

	return (val);
}

static int
ti_dpll_clk_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct ti_dpll_clknode_sc *sc;
	uint64_t cur, best;
	uint32_t val, n, p, best_n, best_p, timeout;

	sc = clknode_get_softc(clk);

	best = cur = 0;

	best = ti_dpll_clk_find_best(sc, fparent, fout,
	    &best_n, &best_p);

	if ((flags & CLK_SET_DRYRUN) != 0) {
		*fout = best;
		*stop = 1;
		return (0);
	}

	if ((best < *fout) &&
	  (flags == CLK_SET_ROUND_DOWN)) {
		*stop = 1;
		return (ERANGE);
	}
	if ((best > *fout) &&
	  (flags == CLK_SET_ROUND_UP)) {
		*stop = 1;
		return (ERANGE);
	}

	DEVICE_LOCK(clk);
	/* 1 switch PLL to bypass mode */
	WRITE4(clk, sc->ti_clkmode_offset, DPLL_EN_MN_BYPASS_MODE);

	/* 2 Ensure PLL is in bypass */
	timeout = 10000;
	do {
		DELAY(10);
		READ4(clk, sc->ti_idlest_offset, &val);
	} while (!(val & ST_MN_BYPASS_MASK) && timeout--);

	if (timeout == 0) {
		DEVICE_UNLOCK(clk);
		return (ERANGE); // FIXME: Better return value?
	}

	/* 3 Set DPLL_MULT & DPLL_DIV bits */
	READ4(clk, sc->ti_clksel_offset, &val);

	n = ti_clk_factor_get_value(&sc->n, best_n);
	p = ti_clk_factor_get_value(&sc->p, best_p);
	val &= ~sc->n.mask;
	val &= ~sc->p.mask;
	val |= n << sc->n.shift;
	val |= p << sc->p.shift;

	WRITE4(clk, sc->ti_clksel_offset, val);

	/* 4. configure M2, M4, M5 and M6 */
	/*
	 * FIXME: According to documentation M2/M4/M5/M6 can be set "later"
	 * See note in TRM 8.1.6.7.1
	 */

	/* 5 Switch over to lock mode */
	WRITE4(clk, sc->ti_clkmode_offset, DPLL_EN_LOCK_MODE);

	/* 6 Ensure PLL is locked */
	timeout = 10000;
	do {
		DELAY(10);
		READ4(clk, sc->ti_idlest_offset, &val);
	} while (!(val & ST_DPLL_CLK_MASK) && timeout--);

	DEVICE_UNLOCK(clk);
	if (timeout == 0) {
		return (ERANGE); // FIXME: Better return value?
	}

	*fout = best;
	*stop = 1;

	return (0);
}

static int
ti_dpll_clk_recalc(struct clknode *clk, uint64_t *freq)
{
	struct ti_dpll_clknode_sc *sc;
	uint32_t val, n, p;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->ti_clksel_offset, &val);
	DEVICE_UNLOCK(clk);

	n = ti_clk_get_factor(val, &sc->n);
	p = ti_clk_get_factor(val, &sc->p);

	*freq = *freq * n / p;

	return (0);
}

static clknode_method_t ti_dpll_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		ti_dpll_clk_init),
	CLKNODEMETHOD(clknode_recalc_freq,	ti_dpll_clk_recalc),
	CLKNODEMETHOD(clknode_set_freq,		ti_dpll_clk_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(ti_dpll_clknode, ti_dpll_clknode_class, ti_dpll_clknode_methods,
	sizeof(struct ti_dpll_clknode_sc), clknode_class);

int
ti_clknode_dpll_register(struct clkdom *clkdom, struct ti_clk_dpll_def *clkdef)
{
	struct clknode *clk;
	struct ti_dpll_clknode_sc *sc;

	clk = clknode_create(clkdom, &ti_dpll_clknode_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->ti_clkmode_offset = clkdef->ti_clkmode_offset;
	sc->ti_clkmode_flags = clkdef->ti_clkmode_flags;
	sc->ti_idlest_offset = clkdef->ti_idlest_offset;
	sc->ti_clksel_offset = clkdef->ti_clksel_offset;

	sc->n.shift = clkdef->ti_clksel_mult.shift;
	sc->n.mask = clkdef->ti_clksel_mult.mask;
	sc->n.width = clkdef->ti_clksel_mult.width;
	sc->n.value = clkdef->ti_clksel_mult.value;
	sc->n.min_value = clkdef->ti_clksel_mult.min_value;
	sc->n.max_value = clkdef->ti_clksel_mult.max_value;
	sc->n.flags = clkdef->ti_clksel_mult.flags;

	sc->p.shift = clkdef->ti_clksel_div.shift;
	sc->p.mask = clkdef->ti_clksel_div.mask;
	sc->p.width = clkdef->ti_clksel_div.width;
	sc->p.value = clkdef->ti_clksel_div.value;
	sc->p.min_value = clkdef->ti_clksel_div.min_value;
	sc->p.max_value = clkdef->ti_clksel_div.max_value;
	sc->p.flags = clkdef->ti_clksel_div.flags;

	sc->ti_autoidle_offset = clkdef->ti_autoidle_offset;

	clknode_register(clkdom, clk);

	return (0);
}
