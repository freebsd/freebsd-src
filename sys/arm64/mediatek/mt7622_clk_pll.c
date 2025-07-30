/*-
 * Copyright (c) 2025 Martin Filla
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include "clkdev_if.h"
#include "mt7622_clk_pll.h"

#define	RD4(sc, reg, val)	CLKDEV_READ_4((sc)->clkdev, reg, val)
#define	WR4(sc, reg, val)	CLKDEV_WRITE_4((sc)->clkdev, reg, val)
#define MT_MASK(h, l)		(((~(uint32_t)0) >> (31 - ((h) - (l)))) << (l))
#define MT_POSTDIV_MASK    0x7

#define MT_PLL_REG_CON0      0x0u
#define MT_PLL_REG_CON1      0x4u
#define MT_PLL_CON0_BASE_EN  (1u << 0)
#define MT_PLL_PWR_ON        (1u << 0)
#define MT_PLL_ISO_EN        (1u << 1)
#define MT_PLL_PCW_CHG_BIT   31
#define MT_AUDPLL_TUNER_EN  (1u << 31)

struct mt7622_pll_sc {
    device_t clkdev;
    uint32_t pll_base_reg;
    uint32_t pll_pwr_reg;
    uint32_t pll_en_mask;
    uint32_t pll_pd_reg;
    uint32_t pll_tuner_reg;
    uint32_t pll_tuner_en_reg;
    uint8_t pll_tuner_en_bit;
    bus_size_t pll_tuner_en_offset;
    int pll_pd_shift;
    unsigned int pll_flags;
    uint64_t pll_fmin;
    uint64_t pll_fmax;
    int pll_pcwbits;
    int pll_pcwibits;
    uint32_t pll_pcw_reg;
    int pll_pcw_shift;
    uint32_t pll_pcw_chg_reg;
    struct div_table *pll_div_table;
    uint32_t pll_en_reg;
};

static int
mt7622_clk_pll_init(struct clknode *clk, device_t dev)
{
	struct mt7622_pll_sc *sc = NULL;
	sc = clknode_get_softc(clk);
	uint32_t reg = 0;

	RD4(sc, sc->pll_pwr_reg, &reg);
	reg = reg | MT_PLL_PWR_ON;
	WR4(sc, sc->pll_pwr_reg, reg);

	RD4(sc, sc->pll_pwr_reg, &reg);
	reg = reg & ~MT_PLL_ISO_EN;
	WR4(sc, sc->pll_pwr_reg, reg);

	if (sc->pll_en_mask) {
		RD4(sc, sc->pll_base_reg + MT_PLL_REG_CON0, &reg);
		reg = reg | sc->pll_en_mask;
		WR4(sc, sc->pll_base_reg + MT_PLL_REG_CON0, reg);
	}

	if (sc->pll_tuner_en_reg) {
		RD4(sc, sc->pll_tuner_en_reg, &reg);
		reg = reg | (reg << sc->pll_tuner_en_bit);
	}
	else if (sc->pll_tuner_reg) {
		RD4(sc, sc->pll_tuner_reg, &reg);
		reg = reg |  MT_AUDPLL_TUNER_EN;
		WR4(sc, sc->pll_tuner_reg, reg);
	}

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static uint64_t
mt7622_pll_recalc_freq(struct mt7622_pll_sc *sc, uint64_t pfreq, uint32_t pcw, uint32_t postdiv)
{
	int pcwbits = sc->pll_pcwbits;
	int pcwfbits = 0, ibits = 0;
	uint64_t vco = 0;
	uint8_t c = 0;

	ibits = sc->pll_pcwibits ? sc->pll_pcwbits: 7;
	if (pcwbits > ibits) {
		pcwfbits = pcwbits - ibits;
	}

	vco = (uint64_t)pfreq * pcw;
	if (pcwfbits && (vco & MT_MASK(pcwfbits - 1, 0))) {
		c = 1;
	}

	vco >>= pcwfbits;
	if (c) {
		vco++;
	}

	return (vco + postdiv - 1) / postdiv;
}

static int
mt7622_clk_pll_recalc_freq(struct clknode *clk, uint64_t *freq)
{
	struct mt7622_pll_sc *sc = NULL;
	sc = clknode_get_softc(clk);
	uint32_t reg, postdiv_sel, postdiv, pcw;

	RD4(sc, sc->pll_pd_reg, &reg);
	postdiv_sel = (reg >> sc->pll_pd_shift) & MT_POSTDIV_MASK;
	postdiv = 1u << postdiv_sel;

	RD4(sc, sc->pll_pcw_reg, &reg);
	pcw = (reg >> sc->pll_pcw_shift);
	pcw &= MT_MASK(sc->pll_pcwbits - 1, 0);

    if (bootverbose) {
        device_printf(sc->clkdev, "%s: %s requested freq: %lu, input freq: %lu\n", __func__, clknode_get_name(clk), *freq, mt7622_pll_recalc_freq(sc, *freq, pcw, postdiv));
    }

    *freq = mt7622_pll_recalc_freq(sc, *freq, pcw, postdiv);

	return (0);
}

static int
mt7622_clk_pll_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
                        int flags, int *stop)
{
    struct mt7622_pll_sc *sc = NULL;
    struct div_table *table = NULL;
    uint64_t freq_out, pcw;
    uint32_t postdiv, val;
    int ibits;

    sc =  clknode_get_softc(clk);;
    device_printf(sc->clkdev, "Set pll frequency %s - %d \n", __func__, __LINE__ );
    if(sc == NULL) {
        return (ENXIO);
    }

    table = sc->pll_div_table;
    if(table == NULL) {
        return (ENXIO);
    }

    device_printf(sc->clkdev, "%s: %s requested freq: %lu, input freq: %lu\n", __func__,
            clknode_get_name(clk), *fout, fin);

    freq_out = *fout;

    /*if (sc->pll_fmin) {
        min = sc->pll_fmin;
    } else {
        min = 1000 * (1000 * 1000);
    }*/

    if (freq_out > sc->pll_fmax) {
        freq_out = sc->pll_fmax;
    }

    if (table) {
        if (freq_out > table[0].freq)
            freq_out = table[0].freq;

        for (val = 0; table[val + 1].freq != 0; val++) {
            if (freq_out > table[val + 1].freq)
                break;
        }
        postdiv = 1 << val;
    } else {
        for (val = 0; val < 5; val++) {
            postdiv = 1 << val;
            if ((freq_out * postdiv) >= sc->pll_fmin)
                break;
        }
    }

    if (sc->pll_pcwibits) {
        ibits = sc->pll_pcwibits;
    } else {
        ibits = 7;
    }

    pcw = (freq_out << val) << (sc->pll_pcwbits - ibits);
    pcw /= fin;

    device_printf(sc->clkdev, "Set pll frequency %s - %d - pcw: %lu\n", __func__, __LINE__, pcw);
    return(0);
}


static clknode_method_t mt7622_pllnode_methods[] = {
        /* Device interface */
        CLKNODEMETHOD(clknode_init,		mt7622_clk_pll_init),
        CLKNODEMETHOD(clknode_recalc_freq,	mt7622_clk_pll_recalc_freq),
        CLKNODEMETHOD(clknode_set_freq,		mt7622_clk_pll_set_freq),

        CLKNODEMETHOD_END
};

DEFINE_CLASS_1(mt7622_pllnode, mt7622_pllnode_class, mt7622_pllnode_methods,
sizeof(struct mt7622_pll_sc), clknode_class);

int
mt7622_clk_pll_register(struct clkdom *clkdom, struct clk_pll_def *clkdef)
{
    struct clknode *clk = NULL;
    struct mt7622_pll_sc *sc;

    clk = clknode_create(clkdom, &mt7622_pllnode_class, &clkdef->clkdef);
    if (clk == NULL) {
        return (ENXIO);
    }

    sc = clknode_get_softc(clk);
    sc->clkdev = clknode_get_device(clk);
    sc->pll_base_reg = clkdef->pll_base_reg;
    sc->pll_pwr_reg = clkdef->pll_pwr_reg;
    sc->pll_en_mask = clkdef->pll_en_mask;
    sc->pll_pd_reg = clkdef->pll_pd_reg;
    sc->pll_tuner_reg = clkdef->pll_tuner_reg;
    sc->pll_tuner_en_reg = clkdef->pll_tuner_en_reg;
    sc->pll_tuner_en_bit = clkdef->pll_tuner_en_bit;
    sc->pll_tuner_en_offset = clkdef->pll_tuner_en_offset;
    sc->pll_pd_shift = clkdef->pll_pd_shift;
    sc->pll_flags = clkdef->pll_flags;
    sc->pll_fmin = clkdef->pll_fmin;
    sc->pll_fmax = clkdef->pll_fmax;
    sc->pll_pcwbits = clkdef->pll_pcwbits;
    sc->pll_pcwibits = clkdef->pll_pcwibits;
    sc->pll_pcw_reg = clkdef->pll_pcw_reg;
    sc->pll_pcw_shift = clkdef->pll_pcw_shift;
    sc->pll_pcw_chg_reg = clkdef->pll_pcw_chg_reg;
    sc->pll_div_table = clkdef->pll_div_table;
    sc->pll_en_reg = clkdef->pll_en_reg;
    if (clknode_register(clkdom, clk) == NULL) {
	    return (ENXIO);
    }

    return (0);
}