/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

#include <arm64/freescale/imx/clk/imx_clk_composite.h>

#include "clkdev_if.h"

#define	TARGET_ROOT_ENABLE	(1 << 28)
#define	TARGET_ROOT_MUX(n)	((n) << 24)
#define	TARGET_ROOT_MUX_MASK	(7 << 24)
#define	TARGET_ROOT_MUX_SHIFT	24
#define	TARGET_ROOT_PRE_PODF(n)		((((n) - 1) & 0x7) << 16)
#define	TARGET_ROOT_PRE_PODF_MASK	(0x7 << 16)
#define	TARGET_ROOT_PRE_PODF_SHIFT	16
#define	TARGET_ROOT_PRE_PODF_MAX	7
#define	TARGET_ROOT_POST_PODF(n)	((((n) - 1) & 0x3f) << 0)
#define	TARGET_ROOT_POST_PODF_MASK	(0x3f << 0)
#define	TARGET_ROOT_POST_PODF_SHIFT	0
#define	TARGET_ROOT_POST_PODF_MAX	0x3f

struct imx_clk_composite_sc {
	uint32_t	offset;
	uint32_t	flags;
};

#define	WRITE4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	READ4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	IMX_CLK_COMPOSITE_MASK_SHIFT	16

#if 0
#define	dprintf(format, arg...)						\
	printf("%s:(%s)" format, __func__, clknode_get_name(clk), arg)
#else
#define	dprintf(format, arg...)
#endif

static int
imx_clk_composite_init(struct clknode *clk, device_t dev)
{
	struct imx_clk_composite_sc *sc;
	uint32_t val, idx;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	DEVICE_UNLOCK(clk);
	idx = (val & TARGET_ROOT_MUX_MASK) >> TARGET_ROOT_MUX_SHIFT;

	clknode_init_parent_idx(clk, idx);

	return (0);
}

static int
imx_clk_composite_set_gate(struct clknode *clk, bool enable)
{
	struct imx_clk_composite_sc *sc;
	uint32_t val = 0;

	sc = clknode_get_softc(clk);

	dprintf("%sabling gate\n", enable ? "En" : "Dis");
	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	if (enable)
		val |= TARGET_ROOT_ENABLE;
	else
		val &= ~(TARGET_ROOT_ENABLE);
	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
imx_clk_composite_set_mux(struct clknode *clk, int index)
{
	struct imx_clk_composite_sc *sc;
	uint32_t val = 0;

	sc = clknode_get_softc(clk);

	dprintf("Set mux to %d\n", index);
	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	val &= ~(TARGET_ROOT_MUX_MASK);
	val |= TARGET_ROOT_MUX(index);
	WRITE4(clk, sc->offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
imx_clk_composite_recalc(struct clknode *clk, uint64_t *freq)
{
	struct imx_clk_composite_sc *sc;
	uint32_t reg, pre_div, post_div;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &reg);
	DEVICE_UNLOCK(clk);

	pre_div = ((reg & TARGET_ROOT_PRE_PODF_MASK)
	    >> TARGET_ROOT_PRE_PODF_SHIFT) + 1;
	post_div = ((reg & TARGET_ROOT_POST_PODF_MASK)
	    >> TARGET_ROOT_POST_PODF_SHIFT) + 1;

	dprintf("parent_freq=%ju, div=%u\n", *freq, div);
	*freq = *freq / pre_div / post_div;
	dprintf("Final freq=%ju\n", *freq);
	return (0);
}

static int
imx_clk_composite_find_best(uint64_t fparent, uint64_t ftarget,
	uint32_t *pre_div, uint32_t *post_div, int flags)
{
	uint32_t prediv, postdiv, best_prediv, best_postdiv;
	int64_t diff, best_diff;
	uint64_t cur;

	best_diff = INT64_MAX;
	for (prediv = 1; prediv <= TARGET_ROOT_PRE_PODF_MAX + 1; prediv++) {
		for (postdiv = 1; postdiv <= TARGET_ROOT_POST_PODF_MAX + 1; postdiv++) {
			cur= fparent / prediv / postdiv;
			diff = (int64_t)ftarget - (int64_t)cur;
			if (flags & CLK_SET_ROUND_DOWN) {
				if (diff >= 0 && diff < best_diff) {
					best_diff = diff;
					best_prediv = prediv;
					best_postdiv = postdiv;
				}
			}
			else if (flags & CLK_SET_ROUND_UP) {
				if (diff <= 0 && abs(diff) < best_diff) {
					best_diff = diff;
					best_prediv = prediv;
					best_postdiv = postdiv;
				}
			}
			else {
				if (abs(diff) < best_diff) {
					best_diff = abs(diff);
					best_prediv = prediv;
					best_postdiv = postdiv;
				}
			}
		}
	}

	if (best_diff == INT64_MAX)
		return (ERANGE);

	*pre_div = best_prediv;
	*post_div = best_postdiv;

	return (0);
}

static int
imx_clk_composite_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct imx_clk_composite_sc *sc;
	struct clknode *p_clk;
	const char **p_names;
	int p_idx, best_parent;
	int64_t best_diff, diff;
	int32_t best_pre_div __unused, best_post_div __unused;
	int32_t pre_div, post_div;
	uint64_t cur, best;
	uint32_t val;

	sc = clknode_get_softc(clk);
	dprintf("Finding best parent/div for target freq of %ju\n", *fout);
	p_names = clknode_get_parent_names(clk);

	best_diff = 0;

	for (p_idx = 0; p_idx != clknode_get_parents_num(clk); p_idx++) {
		p_clk = clknode_find_by_name(p_names[p_idx]);
		clknode_get_freq(p_clk, &fparent);
		dprintf("Testing with parent %s (%d) at freq %ju\n",
		    clknode_get_name(p_clk), p_idx, fparent);

		if (!imx_clk_composite_find_best(fparent, *fout, &pre_div, &post_div, sc->flags))
			continue;
		cur = fparent / pre_div / post_div;
		diff = abs((int64_t)*fout - (int64_t)cur);
		if (diff < best_diff) {
			best = cur;
			best_diff = diff;
			best_pre_div = pre_div;
			best_post_div = post_div;
			best_parent = p_idx;
			dprintf("Best parent so far %s (%d) with best freq at "
			    "%ju\n", clknode_get_name(p_clk), p_idx, best);
		}
	}

	*stop = 1;
	if (best_diff == INT64_MAX)
		return (ERANGE);

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

	dprintf("Setting dividers to pre=%d, post=%d\n", best_pre_div, best_post_div);

	DEVICE_LOCK(clk);
	READ4(clk, sc->offset, &val);
	val &= ~(TARGET_ROOT_PRE_PODF_MASK | TARGET_ROOT_POST_PODF_MASK);
	val |= TARGET_ROOT_PRE_PODF(pre_div);
	val |= TARGET_ROOT_POST_PODF(post_div);
	DEVICE_UNLOCK(clk);

	*fout = best;
	return (0);
}

static clknode_method_t imx_clk_composite_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		imx_clk_composite_init),
	CLKNODEMETHOD(clknode_set_gate,		imx_clk_composite_set_gate),
	CLKNODEMETHOD(clknode_set_mux,		imx_clk_composite_set_mux),
	CLKNODEMETHOD(clknode_recalc_freq,	imx_clk_composite_recalc),
	CLKNODEMETHOD(clknode_set_freq,		imx_clk_composite_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(imx_clk_composite_clknode, imx_clk_composite_clknode_class,
    imx_clk_composite_clknode_methods, sizeof(struct imx_clk_composite_sc),
    clknode_class);

int
imx_clk_composite_register(struct clkdom *clkdom,
    struct imx_clk_composite_def *clkdef)
{
	struct clknode *clk;
	struct imx_clk_composite_sc *sc;

	clk = clknode_create(clkdom, &imx_clk_composite_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->offset = clkdef->offset;
	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
