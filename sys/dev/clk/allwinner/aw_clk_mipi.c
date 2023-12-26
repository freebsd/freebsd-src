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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/clk/clk.h>

#include <dev/clk/allwinner/aw_clk.h>
#include <dev/clk/allwinner/aw_clk_mipi.h>

#include "clkdev_if.h"

/* #define	dprintf(format, arg...)	printf("%s:(%s)" format, __func__, clknode_get_name(clk), arg) */
#define	dprintf(format, arg...)

/*
 * clknode for PLL_MIPI :
 *
 * clk = (pll_video0 * n * k) / m when vfb_sel=0
 * clk depend on sint_frac, sdiv2, s6p25_7p5, pll_feedback_div when vfb_sel=1
 *
 */

struct aw_clk_mipi_sc {
	uint32_t	offset;

	struct aw_clk_factor	k;
	struct aw_clk_factor	m;
	struct aw_clk_factor	n;

	uint64_t		min_freq;
	uint64_t		max_freq;

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

#define	LDO1_EN_SHIFT	23
#define	LDO2_EN_SHIFT	22
#define	VFB_SEL_SHIFT	16

static int
aw_clk_mipi_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static int
aw_clk_mipi_set_gate(struct clknode *clk, bool enable)
{
	struct aw_clk_mipi_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	dprintf("%sabling gate\n", enable ? "En" : "Dis");
	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	if (enable) {
		val |= (1 << sc->gate_shift);
		val |= (1 << LDO1_EN_SHIFT);
		val |= (1 << LDO2_EN_SHIFT);
	} else {
		val &= ~(1 << sc->gate_shift);
		val &= ~(1 << LDO1_EN_SHIFT);
		val &= ~(1 << LDO2_EN_SHIFT);
	}
	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static uint64_t
aw_clk_mipi_find_best(struct aw_clk_mipi_sc *sc, uint64_t fparent, uint64_t *fout,
    uint32_t *factor_k, uint32_t *factor_m, uint32_t *factor_n)
{
	uint64_t cur, best;
	uint32_t n, k, m;

	best = 0;
	*factor_n = 0;
	*factor_k = 0;
	*factor_m = 0;

	for (n = aw_clk_factor_get_min(&sc->n); n <= aw_clk_factor_get_max(&sc->n); n++) {
		for (k = aw_clk_factor_get_min(&sc->k); k <= aw_clk_factor_get_max(&sc->k); k++) {
			for (m = aw_clk_factor_get_min(&sc->m); m <= aw_clk_factor_get_max(&sc->m); m++) {
				cur = (fparent * n * k) / m;
				if ((*fout - cur) < (*fout - best)) {
					best = cur;
					*factor_n = n;
					*factor_k = k;
					*factor_m = m;
				}
				if (best == *fout)
					return (best);
			}
		}
	}

	return best;
}

static int
aw_clk_mipi_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct aw_clk_mipi_sc *sc;
	uint64_t best = 0;
	uint32_t best_k, best_m, best_n;
	uint32_t k, m, n;
	uint32_t val;
	uint32_t retry;

	sc = clknode_get_softc(clk);

	best = aw_clk_mipi_find_best(sc, fparent, fout, &best_k, &best_m, &best_n);

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

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	/* Disable clock during freq changes */
	val &= ~(1 << sc->gate_shift);
	WRITE4(clk, sc->offset, val);

	k = aw_clk_factor_get_value(&sc->k, best_k);
	n = aw_clk_factor_get_value(&sc->n, best_n);
	m = aw_clk_factor_get_value(&sc->m, best_m);
	val &= ~sc->k.mask;
	val &= ~sc->m.mask;
	val &= ~sc->n.mask;
	val |= k << sc->k.shift;
	val |= m << sc->m.shift;
	val |= n << sc->n.shift;

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
aw_clk_mipi_recalc(struct clknode *clk, uint64_t *freq)
{
	struct aw_clk_mipi_sc *sc;
	uint32_t val, m, n, k;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	DEVICE_UNLOCK(clk);

	k = aw_clk_get_factor(val, &sc->k);
	m = aw_clk_get_factor(val, &sc->m);
	n = aw_clk_get_factor(val, &sc->n);

	*freq = (*freq * n * k) / m;

	return (0);
}

static clknode_method_t aw_mipi_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		aw_clk_mipi_init),
	CLKNODEMETHOD(clknode_set_gate,		aw_clk_mipi_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	aw_clk_mipi_recalc),
	CLKNODEMETHOD(clknode_set_freq,		aw_clk_mipi_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(aw_mipi_clknode, aw_mipi_clknode_class, aw_mipi_clknode_methods,
    sizeof(struct aw_clk_mipi_sc), clknode_class);

int
aw_clk_mipi_register(struct clkdom *clkdom, struct aw_clk_mipi_def *clkdef)
{
	struct clknode *clk;
	struct aw_clk_mipi_sc *sc;

	clk = clknode_create(clkdom, &aw_mipi_clknode_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->offset = clkdef->offset;

	sc->k.shift = clkdef->k.shift;
	sc->k.width = clkdef->k.width;
	sc->k.mask = ((1 << sc->k.width) - 1) << sc->k.shift;
	sc->k.value = clkdef->k.value;
	sc->k.flags = clkdef->k.flags;
	sc->k.min_value = clkdef->k.min_value;

	sc->m.shift = clkdef->m.shift;
	sc->m.width = clkdef->m.width;
	sc->m.mask = ((1 << sc->m.width) - 1) << sc->m.shift;
	sc->m.value = clkdef->m.value;
	sc->m.flags = clkdef->m.flags;
	sc->m.min_value = clkdef->m.min_value;

	sc->n.shift = clkdef->n.shift;
	sc->n.width = clkdef->n.width;
	sc->n.mask = ((1 << sc->n.width) - 1) << sc->n.shift;
	sc->n.value = clkdef->n.value;
	sc->n.flags = clkdef->n.flags;
	sc->n.min_value = clkdef->n.min_value;

	sc->min_freq = clkdef->min_freq;
	sc->max_freq = clkdef->max_freq;

	sc->gate_shift = clkdef->gate_shift;

	sc->lock_shift = clkdef->lock_shift;
	sc->lock_retries = clkdef->lock_retries;

	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
