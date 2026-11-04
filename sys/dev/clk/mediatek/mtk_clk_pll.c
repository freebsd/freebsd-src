/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MediaTek PLL clock node, with the MT7986 register layout.  CON0 holds the
 * enable bit, the post divider (bits 6:4, a power of two up to 16) and an
 * optional reset bar bit.  CON1, the register after it, is the 32-bit
 * frequency control word (PCW) with 7 integer bits.
 *
 *	rate = parent * PCW / 2^25 / 2^postdiv
 *
 * A new PCW only takes effect when the PLL is enabled, so a running PLL is
 * stopped while its rate is changed.  CON1 bit 31, a change strobe on older
 * SoCs, is part of the PCW here.
 *
 * The PLLs have no lock bit, so a PLL that was never programmed cannot be
 * told from a stopped one.  Firmware starts the PLLs and the node has no
 * gate: a PLL that is off reports 0 Hz and its rate cannot be set.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/clk/clk.h>
#include <dev/clk/mediatek/mtk_clk.h>

#include "clkdev_if.h"

#define	PLL_CON0_EN		0x00000001
#define	PLL_POSTDIV_SHIFT	4
#define	PLL_POSTDIV_MASK	0x7
#define	PLL_POSTDIV_MAX		4	/* divide by 16 */
#define	PLL_CON1		0x4	/* PCW, from CON0 */
#define	PLL_PCW_FBITS		25	/* fractional bits of the PCW */

/* The firmware runs the VCOs between 1520 and 2880 MHz. */
#define	PLL_VCO_MIN		1500000000ULL
#define	PLL_VCO_MAX		3000000000ULL

#define	PLL_LOCK_DELAY		20	/* us, after enable */

struct mtk_clk_pll_sc {
	uint32_t	con0;
	uint32_t	rst_bar;
};

#define	RD4(_clk, _off, _val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), _off, _val)
#define	WR4(_clk, _off, _val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), _off, _val)
#define	MD4(_clk, _off, _clr, _set)					\
	CLKDEV_MODIFY_4(clknode_get_device(_clk), _off, _clr, _set)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

static int
mtk_clk_pll_init(struct clknode *clk, device_t dev)
{
	clknode_init_parent_idx(clk, 0);
	return (0);
}

/* Rate for a PCW and post divider, rounded to the nearest Hz. */
static uint64_t
mtk_clk_pll_rate(uint64_t fin, uint32_t pcw, uint32_t postdiv)
{
	uint32_t shift = PLL_PCW_FBITS + postdiv;

	return ((fin * pcw + (1ULL << (shift - 1))) >> shift);
}

static int
mtk_clk_pll_recalc_freq(struct clknode *clk, uint64_t *freq)
{
	struct mtk_clk_pll_sc *sc = clknode_get_softc(clk);
	uint32_t con0, pcw;

	DEVICE_LOCK(clk);
	RD4(clk, sc->con0, &con0);
	RD4(clk, sc->con0 + PLL_CON1, &pcw);
	DEVICE_UNLOCK(clk);

	if ((con0 & PLL_CON0_EN) == 0)
		*freq = 0;
	else
		*freq = mtk_clk_pll_rate(*freq, pcw,
		    (con0 >> PLL_POSTDIV_SHIFT) & PLL_POSTDIV_MASK);
	return (0);
}

/* Enable the PLL and release its reset once it has locked. */
static void
mtk_clk_pll_start(struct clknode *clk, struct mtk_clk_pll_sc *sc)
{
	MD4(clk, sc->con0, 0, PLL_CON0_EN);
	DELAY(PLL_LOCK_DELAY);
	if (sc->rst_bar != 0)
		MD4(clk, sc->con0, 0, sc->rst_bar);
}

static void
mtk_clk_pll_stop(struct clknode *clk, struct mtk_clk_pll_sc *sc)
{
	if (sc->rst_bar != 0)
		MD4(clk, sc->con0, sc->rst_bar, 0);
	MD4(clk, sc->con0, PLL_CON0_EN, 0);
}

/* Set a new PCW and post divider, stopping the running PLL for it. */
static void
mtk_clk_pll_program(struct clknode *clk, uint32_t pcw, uint32_t postdiv)
{
	struct mtk_clk_pll_sc *sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	mtk_clk_pll_stop(clk, sc);
	WR4(clk, sc->con0 + PLL_CON1, pcw);
	MD4(clk, sc->con0, PLL_POSTDIV_MASK << PLL_POSTDIV_SHIFT,
	    postdiv << PLL_POSTDIV_SHIFT);
	mtk_clk_pll_start(clk, sc);
	DEVICE_UNLOCK(clk);
}

/*
 * Use the smallest post divider that keeps the VCO in its range, as the
 * firmware does.  The PCW has 25 fractional bits, so the rate is then set
 * to within about 1 Hz, and the rounding only chooses between two
 * neighbouring PCW values.
 */
static int
mtk_clk_pll_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	uint64_t cur, freq, vco;
	uint32_t pcw, postdiv;
	int round;

	*stop = 1;
	round = CLK_SET_ROUND(flags);

	/* A new rate would only take effect when the PLL is started. */
	cur = fin;
	mtk_clk_pll_recalc_freq(clk, &cur);
	if (cur == 0)
		return (ENXIO);

	/* A request beyond the range can only be rounded to its limit. */
	vco = *fout;
	if (vco > PLL_VCO_MAX && (round & CLK_SET_ROUND_DOWN) != 0)
		vco = PLL_VCO_MAX;
	else if (vco < (PLL_VCO_MIN >> PLL_POSTDIV_MAX) &&
	    (round & CLK_SET_ROUND_UP) != 0)
		vco = PLL_VCO_MIN >> PLL_POSTDIV_MAX;
	for (postdiv = 0; postdiv < PLL_POSTDIV_MAX && vco < PLL_VCO_MIN;
	    postdiv++)
		vco <<= 1;
	if (vco < PLL_VCO_MIN || vco > PLL_VCO_MAX)
		return (ERANGE);

	/* Cannot overflow: the VCO rate is below 2^32. */
	pcw = ((vco << PLL_PCW_FBITS) + fin / 2) / fin;
	freq = mtk_clk_pll_rate(fin, pcw, postdiv);
	if (round == CLK_SET_ROUND_UP && freq < *fout)
		freq = mtk_clk_pll_rate(fin, ++pcw, postdiv);
	else if (round == CLK_SET_ROUND_DOWN && freq > *fout)
		freq = mtk_clk_pll_rate(fin, --pcw, postdiv);
	else if (round == CLK_SET_ROUND_EXACT && freq != *fout)
		return (ERANGE);

	if (cur != freq) {
		/* The PLL would have to stop. */
		if ((clknode_get_flags(clk) & CLK_NODE_CANNOT_STOP) != 0)
			return (EBUSY);
		if ((flags & CLK_SET_DRYRUN) == 0)
			mtk_clk_pll_program(clk, pcw, postdiv);
	}
	*fout = freq;
	return (0);
}

static clknode_method_t mtk_clk_pll_methods[] = {
	CLKNODEMETHOD(clknode_init,		mtk_clk_pll_init),
	CLKNODEMETHOD(clknode_recalc_freq,	mtk_clk_pll_recalc_freq),
	CLKNODEMETHOD(clknode_set_freq,		mtk_clk_pll_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(mtk_clk_pll_clknode, mtk_clk_pll_clknode_class,
    mtk_clk_pll_methods, sizeof(struct mtk_clk_pll_sc), clknode_class);

int
mtk_clk_pll_register(struct clkdom *clkdom, const struct mtk_clk_pll_def *def)
{
	struct clknode *clk;
	struct mtk_clk_pll_sc *sc;

	clk = clknode_create(clkdom, &mtk_clk_pll_clknode_class, &def->clkdef);
	if (clk == NULL)
		return (ENXIO);

	sc = clknode_get_softc(clk);
	sc->con0 = def->con0;
	sc->rst_bar = def->rst_bar;

	if (clknode_register(clkdom, clk) == NULL)
		return (ENXIO);
	return (0);
}
