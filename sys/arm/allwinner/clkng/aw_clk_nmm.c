/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include <arm/allwinner/clkng/aw_clk_nmm.h>

#include "clkdev_if.h"

/*
 * clknode for clocks matching the formula :
 *
 * clk = clkin * n / m0 / m1
 *
 */

struct aw_clk_nmm_sc {
	uint32_t	offset;

	struct aw_clk_factor	n;
	struct aw_clk_factor	m0;
	struct aw_clk_factor	m1;

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
aw_clk_nmm_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static int
aw_clk_nmm_set_gate(struct clknode *clk, bool enable)
{
	struct aw_clk_nmm_sc *sc;
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

static uint64_t
aw_clk_nmm_find_best(struct aw_clk_nmm_sc *sc, uint64_t fparent, uint64_t *fout,
  uint32_t *factor_n, uint32_t *factor_m0, uint32_t *factor_m1)
{
	uint64_t cur, best;
	uint32_t n, m0, m1;
	uint32_t max_n, max_m0, max_m1;
	uint32_t min_n, min_m0, min_m1;

	*factor_n = *factor_m0 = *factor_m1 = 0;

	max_n = aw_clk_factor_get_max(&sc->n);
	min_n = aw_clk_factor_get_min(&sc->n);
	max_m0 = aw_clk_factor_get_max(&sc->m0);
	min_m0 = aw_clk_factor_get_min(&sc->m0);
	max_m1 = aw_clk_factor_get_max(&sc->m1);
	min_m1 = aw_clk_factor_get_min(&sc->m1);

	for (m0 = min_m0; m0 <= max_m0; ) {
		for (m1 = min_m1; m1 <= max_m1; ) {
			for (n = min_n; n <= max_n; ) {
				cur = fparent * n / m0 / m1;
				if (abs(*fout - cur) < abs(*fout - best)) {
					best = cur;
					*factor_n = n;
					*factor_m0 = m0;
					*factor_m1 = m1;
				}
				n++;
			}
			m1++;
		}
		m0++;
	}

	return (best);
}

static int
aw_clk_nmm_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct aw_clk_nmm_sc *sc;
	uint64_t cur, best;
	uint32_t val, n, m0, m1, best_n, best_m0, best_m1;
	int retry;

	sc = clknode_get_softc(clk);

	best = cur = 0;

	best = aw_clk_nmm_find_best(sc, fparent, fout,
	    &best_n, &best_m0, &best_m1);

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

	n = aw_clk_factor_get_value(&sc->n, best_n);
	m0 = aw_clk_factor_get_value(&sc->m0, best_m0);
	m1 = aw_clk_factor_get_value(&sc->m1, best_m1);
	val &= ~sc->n.mask;
	val &= ~sc->m0.mask;
	val &= ~sc->m1.mask;
	val |= n << sc->n.shift;
	val |= m0 << sc->m0.shift;
	val |= m1 << sc->m1.shift;

	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	if ((sc->flags & AW_CLK_HAS_LOCK) != 0) {
		for (retry = 0; retry < sc->lock_retries; retry++) {
			READ4(clk, sc->offset, &val);
			if ((val & (1 << sc->lock_shift)) != 0)
				break;
			DELAY(1000);
		}
	}

	*fout = best;
	*stop = 1;

	return (0);
}

static int
aw_clk_nmm_recalc(struct clknode *clk, uint64_t *freq)
{
	struct aw_clk_nmm_sc *sc;
	uint32_t val, n, m0, m1;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	DEVICE_UNLOCK(clk);

	n = aw_clk_get_factor(val, &sc->n);
	m0 = aw_clk_get_factor(val, &sc->m0);
	m1 = aw_clk_get_factor(val, &sc->m1);

	*freq = *freq * n / m0 / m1;

	return (0);
}

static clknode_method_t aw_nmm_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		aw_clk_nmm_init),
	CLKNODEMETHOD(clknode_set_gate,		aw_clk_nmm_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	aw_clk_nmm_recalc),
	CLKNODEMETHOD(clknode_set_freq,		aw_clk_nmm_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(aw_nmm_clknode, aw_nmm_clknode_class, aw_nmm_clknode_methods,
    sizeof(struct aw_clk_nmm_sc), clknode_class);

int
aw_clk_nmm_register(struct clkdom *clkdom, struct aw_clk_nmm_def *clkdef)
{
	struct clknode *clk;
	struct aw_clk_nmm_sc *sc;

	clk = clknode_create(clkdom, &aw_nmm_clknode_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->offset = clkdef->offset;

	sc->n.shift = clkdef->n.shift;
	sc->n.width = clkdef->n.width;
	sc->n.mask = ((1 << sc->n.width) - 1) << sc->n.shift;
	sc->n.value = clkdef->n.value;
	sc->n.flags = clkdef->n.flags;

	sc->m0.shift = clkdef->m0.shift;
	sc->m0.width = clkdef->m0.width;
	sc->m0.mask = ((1 << sc->m0.width) - 1) << sc->m0.shift;
	sc->m0.value = clkdef->m0.value;
	sc->m0.flags = clkdef->m0.flags;

	sc->m1.shift = clkdef->m1.shift;
	sc->m1.width = clkdef->m1.width;
	sc->m1.mask = ((1 << sc->m1.width) - 1) << sc->m1.shift;
	sc->m1.value = clkdef->m1.value;
	sc->m1.flags = clkdef->m1.flags;

	sc->gate_shift = clkdef->gate_shift;

	sc->lock_shift = clkdef->lock_shift;
	sc->lock_retries = clkdef->lock_retries;

	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
