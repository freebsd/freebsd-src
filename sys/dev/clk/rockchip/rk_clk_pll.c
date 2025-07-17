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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/clk/clk.h>

#include <dev/clk/rockchip/rk_clk_pll.h>

#include "clkdev_if.h"

struct rk_clk_pll_sc {
	uint32_t	base_offset;

	uint32_t	gate_offset;
	uint32_t	gate_shift;

	uint32_t	mode_reg;
	uint32_t	mode_shift;

	uint32_t	flags;

	struct rk_clk_pll_rate	*rates;
	struct rk_clk_pll_rate	*frac_rates;
};

#define	WRITE4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	READ4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	RK_CLK_PLL_MASK_SHIFT	16

#if 0
#define	dprintf(format, arg...)						\
	printf("%s:(%s)" format, __func__, clknode_get_name(clk), arg)
#else
#define	dprintf(format, arg...)
#endif

static int
rk_clk_pll_set_gate(struct clknode *clk, bool enable)
{
	struct rk_clk_pll_sc *sc;
	uint32_t val = 0;

	sc = clknode_get_softc(clk);

	if ((sc->flags & RK_CLK_PLL_HAVE_GATE) == 0)
		return (0);

	dprintf("%sabling gate\n", enable ? "En" : "Dis");
	if (!enable)
		val |= 1 << sc->gate_shift;
	dprintf("sc->gate_shift: %x\n", sc->gate_shift);
	val |= (1 << sc->gate_shift) << RK_CLK_PLL_MASK_SHIFT;
	dprintf("Write: gate_offset=%x, val=%x\n", sc->gate_offset, val);
	DEVICE_LOCK(clk);
	WRITE4(clk, sc->gate_offset, val);
	DEVICE_UNLOCK(clk);

	return (0);
}

/* CON0 */
#define	RK3066_CLK_PLL_REFDIV_SHIFT	8
#define	RK3066_CLK_PLL_REFDIV_MASK	0x3F00
#define	RK3066_CLK_PLL_POSTDIV_SHIFT	0
#define	RK3066_CLK_PLL_POSTDIV_MASK	0x000F
/* CON1 */
#define	RK3066_CLK_PLL_LOCK_MASK	(1U << 31)
#define	RK3066_CLK_PLL_FBDIV_SHIFT	0
#define	RK3066_CLK_PLL_FBDIV_MASK	0x0FFF
/* CON2 */

/* CON3 */
#define	RK3066_CLK_PLL_RESET		(1 << 5)
#define	RK3066_CLK_PLL_TEST		(1 << 4)
#define	RK3066_CLK_PLL_ENSAT		(1 << 3)
#define	RK3066_CLK_PLL_FASTEN		(1 << 2)
#define	RK3066_CLK_PLL_POWER_DOWN	(1 << 1)
#define	RK3066_CLK_PLL_BYPASS		(1 << 0)

#define	RK3066_CLK_PLL_MODE_SLOW	0
#define	RK3066_CLK_PLL_MODE_NORMAL	1
#define	RK3066_CLK_PLL_MODE_DEEP_SLOW	2
#define	RK3066_CLK_PLL_MODE_MASK	0x3

static int
rk3066_clk_pll_init(struct clknode *clk, device_t dev)
{
	struct rk_clk_pll_sc *sc;
	uint32_t reg;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->mode_reg, &reg);
	DEVICE_UNLOCK(clk);

	reg = (reg >> sc->mode_shift) & RK3066_CLK_PLL_MODE_MASK;
	clknode_init_parent_idx(clk, reg);

	return (0);
}

static int
rk3066_clk_pll_set_mux(struct clknode *clk, int idx)
{
	uint32_t reg;
	struct rk_clk_pll_sc *sc;

	sc = clknode_get_softc(clk);

	reg = (idx & RK3066_CLK_PLL_MODE_MASK) << sc->mode_shift;
	reg |= (RK3066_CLK_PLL_MODE_MASK << sc->mode_shift) <<
		RK_CLK_PLL_MASK_SHIFT;

	DEVICE_LOCK(clk);
	WRITE4(clk, sc->mode_reg, reg);
	DEVICE_UNLOCK(clk);
	return(0);
}

static int
rk3066_clk_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct rk_clk_pll_sc *sc;
	uint64_t rate;
	uint32_t refdiv, fbdiv, postdiv;
	uint32_t raw0, raw1, raw2, reg;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);

	READ4(clk, sc->base_offset, &raw0);
	READ4(clk, sc->base_offset + 4, &raw1);
	READ4(clk, sc->base_offset + 8, &raw2);
	READ4(clk, sc->mode_reg, &reg);

	DEVICE_UNLOCK(clk);

	reg = (reg >> sc->mode_shift) & RK3066_CLK_PLL_MODE_MASK;

	if (reg != RK3066_CLK_PLL_MODE_NORMAL)
		return (0);

	if (!(raw1 & RK3066_CLK_PLL_LOCK_MASK)) {
		*freq = 0;
		return (0);
	}

	/* TODO MUX */
	refdiv = (raw0 & RK3066_CLK_PLL_REFDIV_MASK) >>
	    RK3066_CLK_PLL_REFDIV_SHIFT;
	refdiv += 1;
	postdiv = (raw0 & RK3066_CLK_PLL_POSTDIV_MASK) >>
	    RK3066_CLK_PLL_POSTDIV_SHIFT;
	postdiv += 1;
	fbdiv = (raw1 & RK3066_CLK_PLL_FBDIV_MASK) >>
	    RK3066_CLK_PLL_FBDIV_SHIFT;
	fbdiv += 1;

	rate = *freq * fbdiv;
	rate /= refdiv;
	*freq = rate / postdiv;

	return (0);
}

static int
rk3066_clk_pll_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct rk_clk_pll_rate *rates;
	struct rk_clk_pll_sc *sc;
	uint32_t reg;
	int rv, timeout;

	sc = clknode_get_softc(clk);

	if (sc->rates == NULL)
		return (EINVAL);

	for (rates = sc->rates; rates->freq; rates++) {
		if (rates->freq == *fout)
			break;
	}
	if (rates->freq == 0) {
		*stop = 1;
		return (EINVAL);
	}

	DEVICE_LOCK(clk);

	/* Setting to slow mode during frequency change */
	reg = (RK3066_CLK_PLL_MODE_MASK << sc->mode_shift) <<
		RK_CLK_PLL_MASK_SHIFT;
	dprintf("Set PLL_MODEREG to %x\n", reg);
	WRITE4(clk, sc->mode_reg, reg);

	/* Reset PLL */
	WRITE4(clk, sc->base_offset + 12, RK3066_CLK_PLL_RESET |
	    RK3066_CLK_PLL_RESET << RK_CLK_PLL_MASK_SHIFT);

	/* Setting postdiv and refdiv */
	reg = 0;
	reg |= RK3066_CLK_PLL_POSTDIV_MASK << 16;
	reg |= (rates->postdiv1 - 1) << RK3066_CLK_PLL_POSTDIV_SHIFT;

	reg |= RK3066_CLK_PLL_REFDIV_MASK << 16;
	reg |= (rates->refdiv - 1)<< RK3066_CLK_PLL_REFDIV_SHIFT;

	dprintf("Set PLL_CON0 to %x\n", reg);
	WRITE4(clk, sc->base_offset, reg);


	/* Setting  fbdiv (no write mask)*/
	READ4(clk, sc->base_offset + 4, &reg);
	reg &= ~RK3066_CLK_PLL_FBDIV_MASK;
	reg |= RK3066_CLK_PLL_FBDIV_MASK << 16;
	reg = (rates->fbdiv - 1) << RK3066_CLK_PLL_FBDIV_SHIFT;

	dprintf("Set PLL_CON1 to %x\n", reg);
	WRITE4(clk, sc->base_offset + 0x4, reg);

	/* PLL loop bandwidth adjust */
	reg =  rates->bwadj - 1;
	dprintf("Set PLL_CON2 to %x (%x)\n", reg, rates->bwadj);
	WRITE4(clk, sc->base_offset + 0x8, reg);

	/* Clear reset */
	WRITE4(clk, sc->base_offset + 12,
	    RK3066_CLK_PLL_RESET << RK_CLK_PLL_MASK_SHIFT);
	DELAY(100000);

	/* Reading lock */
	for (timeout = 1000; timeout >= 0; timeout--) {
		READ4(clk, sc->base_offset + 0x4, &reg);
		if ((reg & RK3066_CLK_PLL_LOCK_MASK) != 0)
			break;
		DELAY(1);
	}

	rv = 0;
	if (timeout < 0) {
		device_printf(clknode_get_device(clk),
		    "%s - Timedout while waiting for lock.\n",
		    clknode_get_name(clk));
		dprintf("PLL_CON1: %x\n", reg);
		rv = ETIMEDOUT;
	}

	/* Set back to normal mode */
	reg = (RK3066_CLK_PLL_MODE_NORMAL << sc->mode_shift);
	reg |= (RK3066_CLK_PLL_MODE_MASK << sc->mode_shift) <<
		RK_CLK_PLL_MASK_SHIFT;
	dprintf("Set PLL_MODEREG to %x\n", reg);
	WRITE4(clk, sc->mode_reg, reg);

	DEVICE_UNLOCK(clk);
	*stop = 1;
	rv = clknode_set_parent_by_idx(clk, 1);
	return (rv);
}

static clknode_method_t rk3066_clk_pll_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		rk3066_clk_pll_init),
	CLKNODEMETHOD(clknode_set_gate,		rk_clk_pll_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	rk3066_clk_pll_recalc),
	CLKNODEMETHOD(clknode_set_freq,		rk3066_clk_pll_set_freq),
	CLKNODEMETHOD(clknode_set_mux,		rk3066_clk_pll_set_mux),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk3066_clk_pll_clknode, rk3066_clk_pll_clknode_class,
    rk3066_clk_pll_clknode_methods, sizeof(struct rk_clk_pll_sc), clknode_class);

int
rk3066_clk_pll_register(struct clkdom *clkdom, struct rk_clk_pll_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_pll_sc *sc;

	clk = clknode_create(clkdom, &rk3066_clk_pll_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->base_offset = clkdef->base_offset;
	sc->gate_offset = clkdef->gate_offset;
	sc->gate_shift = clkdef->gate_shift;
	sc->mode_reg = clkdef->mode_reg;
	sc->mode_shift = clkdef->mode_shift;
	sc->flags = clkdef->flags;
	sc->rates = clkdef->rates;
	sc->frac_rates = clkdef->frac_rates;

	clknode_register(clkdom, clk);

	return (0);
}

#define	RK3328_CLK_PLL_FBDIV_OFFSET	0
#define	RK3328_CLK_PLL_FBDIV_SHIFT	0
#define	RK3328_CLK_PLL_FBDIV_MASK	0xFFF

#define	RK3328_CLK_PLL_POSTDIV1_OFFSET	0
#define	RK3328_CLK_PLL_POSTDIV1_SHIFT	12
#define	RK3328_CLK_PLL_POSTDIV1_MASK	0x7000

#define	RK3328_CLK_PLL_DSMPD_OFFSET	4
#define	RK3328_CLK_PLL_DSMPD_SHIFT	12
#define	RK3328_CLK_PLL_DSMPD_MASK	0x1000

#define	RK3328_CLK_PLL_REFDIV_OFFSET	4
#define	RK3328_CLK_PLL_REFDIV_SHIFT	0
#define	RK3328_CLK_PLL_REFDIV_MASK	0x3F

#define	RK3328_CLK_PLL_POSTDIV2_OFFSET	4
#define	RK3328_CLK_PLL_POSTDIV2_SHIFT	6
#define	RK3328_CLK_PLL_POSTDIV2_MASK	0x1C0

#define	RK3328_CLK_PLL_FRAC_OFFSET	8
#define	RK3328_CLK_PLL_FRAC_SHIFT	0
#define	RK3328_CLK_PLL_FRAC_MASK	0xFFFFFF

#define	RK3328_CLK_PLL_LOCK_MASK	0x400

#define	RK3328_CLK_PLL_MODE_SLOW	0
#define	RK3328_CLK_PLL_MODE_NORMAL	1
#define	RK3328_CLK_PLL_MODE_MASK	0x1

static int
rk3328_clk_pll_init(struct clknode *clk, device_t dev)
{
	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
rk3328_clk_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct rk_clk_pll_sc *sc;
	uint64_t rate;
	uint32_t dsmpd, refdiv, fbdiv;
	uint32_t postdiv1, postdiv2, frac;
	uint32_t raw1, raw2, raw3;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);

	READ4(clk, sc->base_offset, &raw1);
	READ4(clk, sc->base_offset + 4, &raw2);
	READ4(clk, sc->base_offset + 8, &raw3);

	fbdiv = (raw1 & RK3328_CLK_PLL_FBDIV_MASK) >> RK3328_CLK_PLL_FBDIV_SHIFT;
	postdiv1 = (raw1 & RK3328_CLK_PLL_POSTDIV1_MASK) >> RK3328_CLK_PLL_POSTDIV1_SHIFT;

	dsmpd = (raw2 & RK3328_CLK_PLL_DSMPD_MASK) >> RK3328_CLK_PLL_DSMPD_SHIFT;
	refdiv = (raw2 & RK3328_CLK_PLL_REFDIV_MASK) >> RK3328_CLK_PLL_REFDIV_SHIFT;
	postdiv2 = (raw2 & RK3328_CLK_PLL_POSTDIV2_MASK) >> RK3328_CLK_PLL_POSTDIV2_SHIFT;

	frac = (raw3 & RK3328_CLK_PLL_FRAC_MASK) >> RK3328_CLK_PLL_FRAC_SHIFT;

	DEVICE_UNLOCK(clk);

	rate = *freq * fbdiv / refdiv;
	if (dsmpd == 0) {
		/* Fractional mode */
		uint64_t frac_rate;

		frac_rate = *freq * frac / refdiv;
		rate += frac_rate >> 24;
	}

	*freq = rate / postdiv1 / postdiv2;

	if (*freq % 2)
		*freq = *freq + 1;

	return (0);
}

static int
rk3328_clk_pll_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct rk_clk_pll_rate *rates;
	struct rk_clk_pll_sc *sc;
	uint32_t reg;
	int timeout;

	sc = clknode_get_softc(clk);

	if (sc->rates)
		rates = sc->rates;
	else if (sc->frac_rates)
		rates = sc->frac_rates;
	else
		return (EINVAL);

	for (; rates->freq; rates++) {
		if (rates->freq == *fout)
			break;
	}
	if (rates->freq == 0) {
		*stop = 1;
		return (EINVAL);
	}

	DEVICE_LOCK(clk);

	/* Setting to slow mode during frequency change */
	reg = (RK3328_CLK_PLL_MODE_MASK << sc->mode_shift) <<
		RK_CLK_PLL_MASK_SHIFT;
	dprintf("Set PLL_MODEREG to %x\n", reg);
	WRITE4(clk, sc->mode_reg, reg);

	/* Setting postdiv1 and fbdiv */
	reg = (rates->postdiv1 << RK3328_CLK_PLL_POSTDIV1_SHIFT) |
		(rates->fbdiv << RK3328_CLK_PLL_FBDIV_SHIFT);
	reg |= (RK3328_CLK_PLL_POSTDIV1_MASK | RK3328_CLK_PLL_FBDIV_MASK) << 16;
	dprintf("Set PLL_CON0 to %x\n", reg);
	WRITE4(clk, sc->base_offset, reg);

	/* Setting dsmpd, postdiv2 and refdiv */
	reg = (rates->dsmpd << RK3328_CLK_PLL_DSMPD_SHIFT) |
		(rates->postdiv2 << RK3328_CLK_PLL_POSTDIV2_SHIFT) |
		(rates->refdiv << RK3328_CLK_PLL_REFDIV_SHIFT);
	reg |= (RK3328_CLK_PLL_DSMPD_MASK |
	    RK3328_CLK_PLL_POSTDIV2_MASK |
	    RK3328_CLK_PLL_REFDIV_MASK) << RK_CLK_PLL_MASK_SHIFT;
	dprintf("Set PLL_CON1 to %x\n", reg);
	WRITE4(clk, sc->base_offset + 0x4, reg);

	/* Setting frac */
	READ4(clk, sc->base_offset + 0x8, &reg);
	reg &= ~RK3328_CLK_PLL_FRAC_MASK;
	reg |= rates->frac << RK3328_CLK_PLL_FRAC_SHIFT;
	dprintf("Set PLL_CON2 to %x\n", reg);
	WRITE4(clk, sc->base_offset + 0x8, reg);

	/* Reading lock */
	for (timeout = 1000; timeout; timeout--) {
		READ4(clk, sc->base_offset + 0x4, &reg);
		if ((reg & RK3328_CLK_PLL_LOCK_MASK) == 0)
			break;
		DELAY(1);
	}

	/* Set back to normal mode */
	reg = (RK3328_CLK_PLL_MODE_NORMAL << sc->mode_shift);
	reg |= (RK3328_CLK_PLL_MODE_MASK << sc->mode_shift) <<
		RK_CLK_PLL_MASK_SHIFT;
	dprintf("Set PLL_MODEREG to %x\n", reg);
	WRITE4(clk, sc->mode_reg, reg);

	DEVICE_UNLOCK(clk);

	*stop = 1;
	return (0);
}

static clknode_method_t rk3328_clk_pll_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		rk3328_clk_pll_init),
	CLKNODEMETHOD(clknode_set_gate,		rk_clk_pll_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	rk3328_clk_pll_recalc),
	CLKNODEMETHOD(clknode_set_freq,		rk3328_clk_pll_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk3328_clk_pll_clknode, rk3328_clk_pll_clknode_class,
    rk3328_clk_pll_clknode_methods, sizeof(struct rk_clk_pll_sc), clknode_class);

int
rk3328_clk_pll_register(struct clkdom *clkdom, struct rk_clk_pll_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_pll_sc *sc;

	clk = clknode_create(clkdom, &rk3328_clk_pll_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->base_offset = clkdef->base_offset;
	sc->gate_offset = clkdef->gate_offset;
	sc->gate_shift = clkdef->gate_shift;
	sc->mode_reg = clkdef->mode_reg;
	sc->mode_shift = clkdef->mode_shift;
	sc->flags = clkdef->flags;
	sc->rates = clkdef->rates;
	sc->frac_rates = clkdef->frac_rates;

	clknode_register(clkdom, clk);

	return (0);
}

#define	RK3399_CLK_PLL_FBDIV_OFFSET		0
#define	RK3399_CLK_PLL_FBDIV_SHIFT		0
#define	RK3399_CLK_PLL_FBDIV_MASK		0xFFF

#define	RK3399_CLK_PLL_POSTDIV2_OFFSET	4
#define	RK3399_CLK_PLL_POSTDIV2_SHIFT	12
#define	RK3399_CLK_PLL_POSTDIV2_MASK	0x7000

#define	RK3399_CLK_PLL_POSTDIV1_OFFSET	4
#define	RK3399_CLK_PLL_POSTDIV1_SHIFT	8
#define	RK3399_CLK_PLL_POSTDIV1_MASK	0x700

#define	RK3399_CLK_PLL_REFDIV_OFFSET	4
#define	RK3399_CLK_PLL_REFDIV_SHIFT	0
#define	RK3399_CLK_PLL_REFDIV_MASK	0x3F

#define	RK3399_CLK_PLL_FRAC_OFFSET	8
#define	RK3399_CLK_PLL_FRAC_SHIFT	0
#define	RK3399_CLK_PLL_FRAC_MASK	0xFFFFFF

#define	RK3399_CLK_PLL_DSMPD_OFFSET	0xC
#define	RK3399_CLK_PLL_DSMPD_SHIFT	3
#define	RK3399_CLK_PLL_DSMPD_MASK	0x8

#define	RK3399_CLK_PLL_LOCK_OFFSET	8
#define	RK3399_CLK_PLL_LOCK_MASK	0x400

#define	RK3399_CLK_PLL_MODE_OFFSET	0xC
#define	RK3399_CLK_PLL_MODE_MASK	0x300
#define	RK3399_CLK_PLL_MODE_SLOW	0
#define	RK3399_CLK_PLL_MODE_NORMAL	1
#define	RK3399_CLK_PLL_MODE_DEEPSLOW	2
#define	RK3399_CLK_PLL_MODE_SHIFT	8

#define	RK3399_CLK_PLL_WRITE_MASK	0xFFFF0000

static int
rk3399_clk_pll_init(struct clknode *clk, device_t dev)
{
	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
rk3399_clk_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct rk_clk_pll_sc *sc;
	uint32_t dsmpd, refdiv, fbdiv;
	uint32_t postdiv1, postdiv2, fracdiv;
	uint32_t con1, con2, con3, con4;
	uint64_t foutvco;
	uint32_t mode;
	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	READ4(clk, sc->base_offset, &con1);
	READ4(clk, sc->base_offset + 4, &con2);
	READ4(clk, sc->base_offset + 8, &con3);
	READ4(clk, sc->base_offset + 0xC, &con4);
	DEVICE_UNLOCK(clk);

	/*
	 * if we are in slow mode the output freq
	 * is the parent one, the 24Mhz external oscillator
	 * if we are in deep mode the output freq is 32.768khz
	 */
	mode = (con4 & RK3399_CLK_PLL_MODE_MASK) >> RK3399_CLK_PLL_MODE_SHIFT;
	if (mode == RK3399_CLK_PLL_MODE_SLOW) {
		dprintf("pll in slow mode, con4=%x\n", con4);
		return (0);
	} else if (mode == RK3399_CLK_PLL_MODE_DEEPSLOW) {
		dprintf("pll in deep slow, con4=%x\n", con4);
		*freq = 32768;
		return (0);
	}

	dprintf("con0: %x\n", con1);
	dprintf("con1: %x\n", con2);
	dprintf("con2: %x\n", con3);
	dprintf("con3: %x\n", con4);

	fbdiv = (con1 & RK3399_CLK_PLL_FBDIV_MASK)
	    >> RK3399_CLK_PLL_FBDIV_SHIFT;

	postdiv1 = (con2 & RK3399_CLK_PLL_POSTDIV1_MASK)
	    >> RK3399_CLK_PLL_POSTDIV1_SHIFT;
	postdiv2 = (con2 & RK3399_CLK_PLL_POSTDIV2_MASK)
	    >> RK3399_CLK_PLL_POSTDIV2_SHIFT;
	refdiv = (con2 & RK3399_CLK_PLL_REFDIV_MASK)
	    >> RK3399_CLK_PLL_REFDIV_SHIFT;

	fracdiv = (con3 & RK3399_CLK_PLL_FRAC_MASK)
	    >> RK3399_CLK_PLL_FRAC_SHIFT;
	fracdiv >>= 24;

	dsmpd = (con4 & RK3399_CLK_PLL_DSMPD_MASK) >> RK3399_CLK_PLL_DSMPD_SHIFT;

	dprintf("fbdiv: %d\n", fbdiv);
	dprintf("postdiv1: %d\n", postdiv1);
	dprintf("postdiv2: %d\n", postdiv2);
	dprintf("refdiv: %d\n", refdiv);
	dprintf("fracdiv: %d\n", fracdiv);
	dprintf("dsmpd: %d\n", dsmpd);

	dprintf("parent freq=%ju\n", *freq);

	if (dsmpd == 0) {
		/* Fractional mode */
		foutvco = *freq / refdiv * (fbdiv + fracdiv);
	} else {
		/* Integer mode */
		foutvco = *freq / refdiv * fbdiv;
	}
	dprintf("foutvco: %ju\n", foutvco);

	*freq = foutvco / postdiv1 / postdiv2;
	dprintf("freq: %ju\n", *freq);

	return (0);
}

static int
rk3399_clk_pll_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct rk_clk_pll_rate *rates;
	struct rk_clk_pll_sc *sc;
	uint32_t reg;
	int timeout;

	sc = clknode_get_softc(clk);

	if (sc->rates)
		rates = sc->rates;
	else if (sc->frac_rates)
		rates = sc->frac_rates;
	else
		return (EINVAL);

	for (; rates->freq; rates++) {
		if (rates->freq == *fout)
			break;
	}
	if (rates->freq == 0) {
		*stop = 1;
		return (EINVAL);
	}

	DEVICE_LOCK(clk);

	/* Set to slow mode during frequency change */
	reg = RK3399_CLK_PLL_MODE_SLOW << RK3399_CLK_PLL_MODE_SHIFT;
	reg |= RK3399_CLK_PLL_MODE_MASK << RK_CLK_PLL_MASK_SHIFT;
	WRITE4(clk, sc->base_offset + 0xC, reg);

	/* Setting fbdiv */
	reg = rates->fbdiv << RK3399_CLK_PLL_FBDIV_SHIFT;
	reg |= RK3399_CLK_PLL_FBDIV_MASK << RK_CLK_PLL_MASK_SHIFT;
	WRITE4(clk, sc->base_offset, reg);

	/* Setting postdiv1, postdiv2 and refdiv */
	reg = rates->postdiv1 << RK3399_CLK_PLL_POSTDIV1_SHIFT;
	reg |= rates->postdiv2 << RK3399_CLK_PLL_POSTDIV2_SHIFT;
	reg |= rates->refdiv << RK3399_CLK_PLL_REFDIV_SHIFT;
	reg |= (RK3399_CLK_PLL_POSTDIV1_MASK | RK3399_CLK_PLL_POSTDIV2_MASK |
	    RK3399_CLK_PLL_REFDIV_MASK) << RK_CLK_PLL_MASK_SHIFT;
	WRITE4(clk, sc->base_offset + 0x4, reg);

	/* Setting frac */
	READ4(clk, sc->base_offset + 0x8, &reg);
	reg &= ~RK3399_CLK_PLL_FRAC_MASK;
	reg |= rates->frac << RK3399_CLK_PLL_FRAC_SHIFT;
	WRITE4(clk, sc->base_offset + 0x8, reg | RK3399_CLK_PLL_WRITE_MASK);

	/* Set dsmpd */
	reg = rates->dsmpd << RK3399_CLK_PLL_DSMPD_SHIFT;
	reg |= RK3399_CLK_PLL_DSMPD_MASK << RK_CLK_PLL_MASK_SHIFT;
	WRITE4(clk, sc->base_offset + 0xC, reg);

	/* Reading lock */
	for (timeout = 1000; timeout; timeout--) {
		READ4(clk, sc->base_offset + RK3399_CLK_PLL_LOCK_OFFSET, &reg);
		if ((reg & RK3399_CLK_PLL_LOCK_MASK) == 0)
			break;
		DELAY(1);
	}

	/* Set back to normal mode */
	reg = RK3399_CLK_PLL_MODE_NORMAL << RK3399_CLK_PLL_MODE_SHIFT;
	reg |= RK3399_CLK_PLL_MODE_MASK << RK_CLK_PLL_MASK_SHIFT;
	WRITE4(clk, sc->base_offset + 0xC, reg);

	DEVICE_UNLOCK(clk);

	*stop = 1;
	return (0);
}

static clknode_method_t rk3399_clk_pll_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		rk3399_clk_pll_init),
	CLKNODEMETHOD(clknode_set_gate,		rk_clk_pll_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	rk3399_clk_pll_recalc),
	CLKNODEMETHOD(clknode_set_freq,		rk3399_clk_pll_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk3399_clk_pll_clknode, rk3399_clk_pll_clknode_class,
    rk3399_clk_pll_clknode_methods, sizeof(struct rk_clk_pll_sc), clknode_class);

int
rk3399_clk_pll_register(struct clkdom *clkdom, struct rk_clk_pll_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_pll_sc *sc;

	clk = clknode_create(clkdom, &rk3399_clk_pll_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);

	sc->base_offset = clkdef->base_offset;
	sc->gate_offset = clkdef->gate_offset;
	sc->gate_shift = clkdef->gate_shift;
	sc->flags = clkdef->flags;
	sc->rates = clkdef->rates;
	sc->frac_rates = clkdef->frac_rates;

	clknode_register(clkdom, clk);

	return (0);
}
