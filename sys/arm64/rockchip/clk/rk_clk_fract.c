/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2019 Michal Meloun <mmel@FreeBSD.org>
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

#include <dev/extres/clk/clk.h>

#include <arm64/rockchip/clk/rk_clk_fract.h>

#include "clkdev_if.h"

#define	WR4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	RD4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	MD4(_clk, off, clr, set )					\
	CLKDEV_MODIFY_4(clknode_get_device(_clk), off, clr, set)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	RK_CLK_FRACT_MASK_SHIFT	16

static int rk_clk_fract_init(struct clknode *clk, device_t dev);
static int rk_clk_fract_recalc(struct clknode *clk, uint64_t *req);
static int rk_clk_fract_set_freq(struct clknode *clknode, uint64_t fin,
    uint64_t *fout, int flag, int *stop);
static int rk_clk_fract_set_gate(struct clknode *clk, bool enable);

struct rk_clk_fract_sc {
	uint32_t	flags;
	uint32_t	offset;
	uint32_t	numerator;
	uint32_t	denominator;
	uint32_t	gate_offset;
	uint32_t	gate_shift;
};

static clknode_method_t rk_clk_fract_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		rk_clk_fract_init),
	CLKNODEMETHOD(clknode_set_gate,		rk_clk_fract_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	rk_clk_fract_recalc),
	CLKNODEMETHOD(clknode_set_freq,		rk_clk_fract_set_freq),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(rk_clk_fract, rk_clk_fract_class, rk_clk_fract_methods,
   sizeof(struct rk_clk_fract_sc), clknode_class);

/*
 * Compute best rational approximation of input fraction
 * for fixed sized fractional divider registers.
 * http://en.wikipedia.org/wiki/Continued_fraction
 *
 * - n_input, d_input	Given input fraction
 * - n_max, d_max	Maximum vaues of divider registers
 * - n_out, d_out	Computed approximation
 */

static void
clk_compute_fract_div(
	uint64_t n_input, uint64_t d_input,
	uint64_t n_max, uint64_t d_max,
	uint64_t *n_out, uint64_t *d_out)
{
	uint64_t n_prev, d_prev;	/* previous convergents */
	uint64_t n_cur, d_cur;		/* current  convergents */
	uint64_t n_rem, d_rem;		/* fraction remainder */
	uint64_t tmp, fact;

	/* Initialize fraction reminder */
	n_rem = n_input;
	d_rem = d_input;

	/* Init convergents to 0/1 and 1/0 */
	n_prev = 0;
	d_prev = 1;
	n_cur = 1;
	d_cur = 0;

	while (d_rem != 0 && n_cur < n_max && d_cur < d_max) {
		/* Factor for this step. */
		fact = n_rem / d_rem;

		/* Adjust fraction reminder */
		tmp = d_rem;
		d_rem = n_rem % d_rem;
		n_rem = tmp;

		/* Compute new nominator and save last one */
		tmp = n_prev + fact * n_cur;
		n_prev = n_cur;
		n_cur = tmp;

		/* Compute new denominator and save last one */
		tmp = d_prev + fact * d_cur;
		d_prev = d_cur;
		d_cur = tmp;
	}

	if (n_cur > n_max || d_cur > d_max) {
		*n_out = n_prev;
		*d_out = d_prev;
	} else {
		*n_out = n_cur;
		*d_out = d_cur;
	}
}

static int
rk_clk_fract_init(struct clknode *clk, device_t dev)
{
	uint32_t reg;
	struct rk_clk_fract_sc *sc;

	sc = clknode_get_softc(clk);
	DEVICE_LOCK(clk);
	RD4(clk, sc->offset, &reg);
	DEVICE_UNLOCK(clk);

	sc->numerator  = (reg >> 16) & 0xFFFF;
	sc->denominator  = reg & 0xFFFF;
	clknode_init_parent_idx(clk, 0);

	return(0);
}

static int
rk_clk_fract_set_gate(struct clknode *clk, bool enable)
{
	struct rk_clk_fract_sc *sc;
	uint32_t val = 0;

	sc = clknode_get_softc(clk);

	if ((sc->flags & RK_CLK_FRACT_HAVE_GATE) == 0)
		return (0);

	RD4(clk, sc->gate_offset, &val);

	val = 0;
	if (!enable)
		val |= 1 << sc->gate_shift;
	val |= (1 << sc->gate_shift) << RK_CLK_FRACT_MASK_SHIFT;
	DEVICE_LOCK(clk);
	WR4(clk, sc->gate_offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
rk_clk_fract_recalc(struct clknode *clk, uint64_t *freq)
{
	struct rk_clk_fract_sc *sc;

	sc = clknode_get_softc(clk);
	if (sc->denominator == 0) {
		printf("%s: %s denominator is zero!\n", clknode_get_name(clk),
		__func__);
		*freq = 0;
		return(EINVAL);
	}

	*freq *= sc->numerator;
	*freq /= sc->denominator;

	return (0);
}

static int
rk_clk_fract_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	struct rk_clk_fract_sc *sc;
	uint64_t div_n, div_d, _fout;

	sc = clknode_get_softc(clk);

	clk_compute_fract_div(*fout, fin, 0xFFFF, 0xFFFF, &div_n, &div_d);
	_fout = fin * div_n;
	_fout /= div_d;

	/* Rounding. */
	if ((flags & CLK_SET_ROUND_UP) && (_fout < *fout)) {
		if (div_n > div_d && div_d > 1)
			div_n++;
		else
			div_d--;
	} else if ((flags & CLK_SET_ROUND_DOWN) && (_fout > *fout)) {
		if (div_n > div_d && div_n > 1)
			div_n--;
		else
			div_d++;
	}

	/* Check range after rounding */
	if (div_n > 0xFFFF || div_d > 0xFFFF)
		return (ERANGE);

	if (div_d == 0) {
		printf("%s: %s divider is zero!\n",
		     clknode_get_name(clk), __func__);
		return(EINVAL);
	}
	/* Recompute final output frequency */
	_fout = fin * div_n;
	_fout /= div_d;

	*stop = 1;

	if ((flags & CLK_SET_DRYRUN) == 0) {
		if (*stop != 0 &&
		    (flags & (CLK_SET_ROUND_UP | CLK_SET_ROUND_DOWN)) == 0 &&
		    *fout != _fout)
			return (ERANGE);

		sc->numerator  = (uint32_t)div_n;
		sc->denominator = (uint32_t)div_d;

		DEVICE_LOCK(clk);
		WR4(clk, sc->offset, sc->numerator << 16 | sc->denominator);
		DEVICE_UNLOCK(clk);
	}

	*fout = _fout;
	return (0);
}

int
rk_clk_fract_register(struct clkdom *clkdom, struct rk_clk_fract_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_fract_sc *sc;

	clk = clknode_create(clkdom, &rk_clk_fract_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->flags = clkdef->flags;
	sc->offset = clkdef->offset;
	sc->gate_offset = clkdef->gate_offset;
	sc->gate_shift = clkdef->gate_shift;

	clknode_register(clkdom, clk);
	return (0);
}
