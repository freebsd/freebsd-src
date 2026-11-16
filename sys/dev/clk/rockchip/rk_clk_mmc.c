/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 Emmanuel Vadot <manu@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/clk/clk.h>

#include <dev/clk/rockchip/rk_clk_mmc.h>

#include "clkdev_if.h"

#define	WR4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	RD4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

/*
 * MMC drive/sample phase clock.  The controller output clock is the parent
 * rate divided by two (RK_MMC_CLKGEN_DIV).  The phase is programmed as a
 * coarse 90 degree step plus an optional fine delay of up to 255 elements.
 */
#define	RK_MMC_CLKGEN_DIV	2
#define	RK_MMC_DELAY_SEL	(1 << 10)
#define	RK_MMC_DEGREE_MASK	0x3
#define	RK_MMC_DELAYNUM_OFFSET	2
#define	RK_MMC_DELAYNUM_MASK	(0xff << RK_MMC_DELAYNUM_OFFSET)
#define	RK_MMC_PHASE_MASK	0x7ff

/*
 * Each fine delay element is between 44 and 77 ps; assume 60 ps.  So an
 * angle can be off by up to about 25 degrees, which is acceptable because
 * the tuning only needs to find a point far from the bad range.
 */
#define	RK_MMC_DELAY_ELEMENT_PSEC	60

#define	DIV_ROUND_CLOSEST_U(n, d)	(((n) + ((d) / 2)) / (d))

struct rk_clk_mmc_sc {
	uint32_t	offset;
	uint32_t	shift;
};

static int
rk_clk_mmc_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static int
rk_clk_mmc_recalc_freq(struct clknode *clk, uint64_t *freq)
{

	*freq /= RK_MMC_CLKGEN_DIV;
	return (0);
}

static int
rk_clk_mmc_get_phase(struct clknode *clk, int *degrees)
{
	struct rk_clk_mmc_sc *sc;
	struct clknode *parent;
	uint64_t rate;
	uint32_t raw;
	int rv;

	sc = clknode_get_softc(clk);

	/*
	 * Read the parent rate: the core holds our node lock while calling
	 * this method, so clknode_get_freq() on ourselves would relock it.
	 */
	parent = clknode_get_parent(clk);
	if (parent == NULL) {
		*degrees = 0;
		return (0);
	}
	rv = clknode_get_freq(parent, &rate);
	if (rv != 0)
		return (rv);
	rate /= RK_MMC_CLKGEN_DIV;
	/* Constant signal, no measurable phase shift. */
	if (rate == 0) {
		*degrees = 0;
		return (0);
	}

	DEVICE_LOCK(clk);
	rv = RD4(clk, sc->offset, &raw);
	DEVICE_UNLOCK(clk);
	if (rv != 0)
		return (rv);

	raw >>= sc->shift;
	*degrees = (raw & RK_MMC_DEGREE_MASK) * 90;
	if ((raw & RK_MMC_DELAY_SEL) != 0) {
		uint64_t factor, delay_num;

		/* degrees per delay element * 1000000 */
		factor = (RK_MMC_DELAY_ELEMENT_PSEC / 10) * 36 *
		    (rate / 10000);
		delay_num = (raw & RK_MMC_DELAYNUM_MASK) >>
		    RK_MMC_DELAYNUM_OFFSET;
		*degrees += DIV_ROUND_CLOSEST_U(delay_num * factor, 1000000);
	}
	*degrees %= 360;
	return (0);
}

static int
rk_clk_mmc_set_phase(struct clknode *clk, int degrees)
{
	struct rk_clk_mmc_sc *sc;
	struct clknode *parent;
	uint64_t rate, delay;
	uint32_t raw, nineties, remainder;
	int rv;

	sc = clknode_get_softc(clk);

	if (degrees < 0 || degrees >= 360)
		return (EINVAL);
	/*
	 * Read the parent rate: the core holds our node lock while calling
	 * this method, so clknode_get_freq() on ourselves would relock it.
	 */
	parent = clknode_get_parent(clk);
	if (parent == NULL)
		return (EINVAL);
	rv = clknode_get_freq(parent, &rate);
	if (rv != 0)
		return (rv);
	rate /= RK_MMC_CLKGEN_DIV;
	/* The phase can't be calculated for a stopped clock. */
	if (rate == 0)
		return (EINVAL);

	nineties = degrees / 90;
	remainder = degrees % 90;

	/* Convert the remainder to a number of fine delay elements. */
	delay = 10000000ULL * remainder;
	delay = DIV_ROUND_CLOSEST_U(delay, (rate / 1000) * 36 *
	    (RK_MMC_DELAY_ELEMENT_PSEC / 10));
	if (delay > 255)
		delay = 255;

	raw = delay != 0 ? RK_MMC_DELAY_SEL : 0;
	raw |= delay << RK_MMC_DELAYNUM_OFFSET;
	raw |= nineties;

	/* Hiword update: the upper 16 bits are the write enable mask. */
	DEVICE_LOCK(clk);
	rv = WR4(clk, sc->offset, (raw << sc->shift) |
	    ((RK_MMC_PHASE_MASK << sc->shift) << 16));
	DEVICE_UNLOCK(clk);
	return (rv);
}

static clknode_method_t rk_clk_mmc_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		rk_clk_mmc_init),
	CLKNODEMETHOD(clknode_recalc_freq,	rk_clk_mmc_recalc_freq),
	CLKNODEMETHOD(clknode_set_phase,	rk_clk_mmc_set_phase),
	CLKNODEMETHOD(clknode_get_phase,	rk_clk_mmc_get_phase),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(rk_clk_mmc, rk_clk_mmc_class, rk_clk_mmc_methods,
   sizeof(struct rk_clk_mmc_sc), clknode_class);

int
rk_clk_mmc_register(struct clkdom *clkdom, struct rk_clk_mmc_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_mmc_sc *sc;

	clk = clknode_create(clkdom, &rk_clk_mmc_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->offset = clkdef->offset;
	sc->shift = clkdef->shift;

	clknode_register(clkdom, clk);
	return (0);
}
