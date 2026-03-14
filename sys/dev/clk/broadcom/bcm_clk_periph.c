/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Perdixky <3293789706@qq.com>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/clk/broadcom/bcm_clk_periph.h>
#include <dev/clk/clk.h>

#include "clkdev_if.h"

struct bcm_clk_periph_sc {
	uint32_t ctl_offset;
	uint32_t div_offset;

	uint32_t passwd_shift;
	uint32_t passwd_mask;
	uint32_t passwd_val;

	uint32_t mash_shift;
	uint32_t mash_mask;

	uint32_t busy_shift;

	uint32_t enable_shift;

	uint32_t src_shift;
	uint32_t src_mask;

	uint32_t div_int_shift;
	uint32_t div_int_mask;
	uint32_t div_frac_shift;
	uint32_t div_frac_mask;
};

#define WRITE4(_clk, _sc, _off, _val)                    \
	CLKDEV_WRITE_4(clknode_get_device(_clk), (_off), \
	    ((_val) & ~(_sc)->passwd_mask) | (_sc)->passwd_val)
#define READ4(_clk, _off, _val) \
	CLKDEV_READ_4(clknode_get_device(_clk), (_off), (_val))

#define LOCK(_clk)   CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define UNLOCK(_clk) CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

static int
bcm_clk_periph_init(struct clknode *clk, device_t dev)
{
	struct bcm_clk_periph_sc *sc;
	uint32_t val, idx;

	sc = clknode_get_softc(clk);

	LOCK(clk);
	READ4(clk, sc->ctl_offset, &val);
	UNLOCK(clk);

	idx = (val & sc->src_mask) >> sc->src_shift;
	clknode_init_parent_idx(clk, idx);

	return (0);
}

static int
bcm_clk_ctl_periph_gate(struct clknode *clk, bool enable)
{
	struct bcm_clk_periph_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	LOCK(clk);
	READ4(clk, sc->ctl_offset, &val);
	if (enable)
		val |= (1u << sc->enable_shift);
	else
		val &= ~(1u << sc->enable_shift);
	WRITE4(clk, sc, sc->ctl_offset, val);
	UNLOCK(clk);

	return (0);
}

static uint32_t
bcm_clk_choose_div(struct bcm_clk_periph_sc *sc, uint64_t fin, uint64_t *fout)
{
	uint64_t div, best;
	uint32_t min_div, max_div;

	div = ((fin << 12) + (*fout / 2)) / *fout;
	/* MASH typically requires a minimum divider of 2. */
	min_div = (sc->mash_mask != 0) ? (2u << 12) : (1u << 12);
	max_div = (sc->div_int_mask | sc->div_frac_mask);
	if (div < min_div)
		div = min_div;
	if (div > max_div)
		div = max_div;
	best = (fin << 12) / div;
	*fout = best;
	return ((uint32_t)div);
}

static int
bcm_clk_wait_while_busy(struct clknode *clk, struct bcm_clk_periph_sc *sc)
{
	uint32_t val;
	int retry;

	for (retry = 0; retry < 50; retry++) {
		READ4(clk, sc->ctl_offset, &val);
		if ((val & (1u << sc->busy_shift)) == 0)
			return (0);
		DELAY(1);
	}
	return (ETIMEDOUT);
}

static int
bcm_clk_periph_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct bcm_clk_periph_sc *sc;
	uint64_t req;
	uint32_t ctl, div;
	bool was_enabled;
	int rv;

	sc = clknode_get_softc(clk);
	req = *fout;
	div = bcm_clk_choose_div(sc, fparent, fout);
	if ((flags & CLK_SET_DRYRUN) != 0) {
		*stop = 1;
		return (0);
	}
	if ((*fout < req) && ((flags & CLK_SET_ROUND_DOWN) == 0))
		return (ERANGE);
	if ((*fout > req) && ((flags & CLK_SET_ROUND_UP) == 0))
		return (ERANGE);

	/* Disable the clock and wait for BUSY to clear to avoid glitches. */
	LOCK(clk);
	READ4(clk, sc->ctl_offset, &ctl);
	was_enabled = ((ctl & (1u << sc->enable_shift)) != 0);
	if (was_enabled) {
		ctl &= ~(1u << sc->enable_shift);
		WRITE4(clk, sc, sc->ctl_offset, ctl);
	}

	rv = bcm_clk_wait_while_busy(clk, sc);
	if (rv != 0) {
		UNLOCK(clk);
		return (rv);
	}

	READ4(clk, sc->ctl_offset, &ctl);
	WRITE4(clk, sc, sc->div_offset, div);

	if (was_enabled)
		ctl |= (1u << sc->enable_shift);

	ctl &= ~sc->mash_mask;
	if (div & sc->div_frac_mask)
		ctl |= (2u << sc->mash_shift);

	WRITE4(clk, sc, sc->ctl_offset, ctl);
	UNLOCK(clk);

	*stop = 1;
	return (0);
}

static int
bcm_clk_periph_recalc(struct clknode *clk, uint64_t *freq)
{
	struct bcm_clk_periph_sc *sc;
	uint32_t div;

	sc = clknode_get_softc(clk);

	LOCK(clk);
	READ4(clk, sc->div_offset, &div);
	UNLOCK(clk);

	*freq = (*freq << 12) / div;
	return (0);
}

static clknode_method_t bcm_clk_periph_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init, bcm_clk_periph_init),
	CLKNODEMETHOD(clknode_set_gate, bcm_clk_ctl_periph_gate),
	CLKNODEMETHOD(clknode_recalc_freq, bcm_clk_periph_recalc),
	CLKNODEMETHOD(clknode_set_freq, bcm_clk_periph_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(bcm_clk_periph_clknode, bcm_clk_periph_clknode_class,
    bcm_clk_periph_clknode_methods, sizeof(struct bcm_clk_periph_sc),
    clknode_class);

int
bcm_clk_periph_register(struct clkdom *clkdom,
    struct bcm_clk_periph_def *clkdef)
{
	struct clknode *clk;
	struct bcm_clk_periph_sc *sc;

	clk = clknode_create(clkdom, &bcm_clk_periph_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->ctl_offset = clkdef->ctl_offset;
	sc->div_offset = clkdef->div_offset;

	sc->passwd_shift = clkdef->passwd_shift;
	sc->passwd_mask = ((1u << clkdef->passwd_width) - 1)
	    << sc->passwd_shift;
	sc->passwd_val = clkdef->passwd << sc->passwd_shift;

	sc->mash_shift = clkdef->mash_shift;
	sc->mash_mask = ((1u << clkdef->mash_width) - 1) << sc->mash_shift;

	sc->busy_shift = clkdef->busy_shift;

	sc->enable_shift = clkdef->enable_shift;

	sc->src_shift = clkdef->src_shift;
	sc->src_mask = ((1u << clkdef->src_width) - 1) << sc->src_shift;

	sc->div_int_shift = clkdef->div_int_shift;
	sc->div_int_mask = ((1u << clkdef->div_int_width) - 1)
	    << sc->div_int_shift;
	sc->div_frac_shift = clkdef->div_frac_shift;
	sc->div_frac_mask = ((1u << clkdef->div_frac_width) - 1)
	    << sc->div_frac_shift;

	clknode_register(clkdom, clk);

	return (0);
}
