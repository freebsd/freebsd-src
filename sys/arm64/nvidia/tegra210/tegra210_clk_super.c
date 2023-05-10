/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>

#include <dt-bindings/clock/tegra210-car.h>
#include "tegra210_car.h"

struct super_mux_def {
	struct clknode_init_def	clkdef;
	uint32_t		base_reg;
	uint32_t		flags;
};

#define	PLIST(x) static const char *x[]
#define	SM(_id, cn, pl, r)						\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cn,						\
	.clkdef.parent_names = pl,					\
	.clkdef.parent_cnt = nitems(pl),				\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.base_reg = r,							\
}


PLIST(cclk_g_parents) = {
	"clk_m", NULL, "clk_s", NULL,
	"pllP_out0", "pllP_out4", NULL, NULL,
	"pllX_out0", "dfllCPU_out_alias", NULL, NULL,
	NULL, NULL, "pllX_out0_alias", "dfllCPU_out",
};

PLIST(cclk_lp_parents) = {
	"clk_m", NULL, "clk_s", NULL,
	"pllP_out0", "pllP_out4", NULL, NULL,
	"pllX_out0", "dfllCPU_out_alias", NULL, NULL,
	NULL, NULL, "pllX_out0_alias", "dfllCPU_out",
};

PLIST(sclk_parents) = {
	"clk_m", "pllC_out1", "pllC4_out3", "pllP_out0",
	"pllP_out2", "pllC4_out1", "clk_s", "pllC4_out1",
};

static struct super_mux_def super_mux_def[] = {
 SM(TEGRA210_CLK_CCLK_G, "cclk_g", cclk_g_parents, CCLKG_BURST_POLICY),
 SM(TEGRA210_CLK_CCLK_LP, "cclk_lp", cclk_lp_parents, CCLKLP_BURST_POLICY),
 SM(TEGRA210_CLK_SCLK, "sclk", sclk_parents, SCLK_BURST_POLICY),
};

static int super_mux_init(struct clknode *clk, device_t dev);
static int super_mux_set_mux(struct clknode *clk, int idx);

struct super_mux_sc {
	device_t		clkdev;
	uint32_t		base_reg;
	uint32_t		flags;

	int 			mux;
};

static clknode_method_t super_mux_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		super_mux_init),
	CLKNODEMETHOD(clknode_set_mux, 		super_mux_set_mux),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(tegra210_super_mux, tegra210_super_mux_class, super_mux_methods,
   sizeof(struct super_mux_sc), clknode_class);

/* Mux status. */
#define	SUPER_MUX_STATE_STDBY		0
#define	SUPER_MUX_STATE_IDLE		1
#define	SUPER_MUX_STATE_RUN		2
#define	SUPER_MUX_STATE_IRQ		3
#define	SUPER_MUX_STATE_FIQ		4

/* Mux register bits. */
#define	SUPER_MUX_STATE_BIT_SHIFT	28
#define	SUPER_MUX_STATE_BIT_MASK	0xF
/* State is Priority encoded */
#define	SUPER_MUX_STATE_BIT_STDBY	0x00
#define	SUPER_MUX_STATE_BIT_IDLE	0x01
#define	SUPER_MUX_STATE_BIT_RUN		0x02
#define	SUPER_MUX_STATE_BIT_IRQ		0x04
#define	SUPER_MUX_STATE_BIT_FIQ		0x08

#define	SUPER_MUX_MUX_WIDTH		4

static uint32_t
super_mux_get_state(uint32_t reg)
{
	reg = (reg >> SUPER_MUX_STATE_BIT_SHIFT) & SUPER_MUX_STATE_BIT_MASK;
	if (reg & SUPER_MUX_STATE_BIT_FIQ)
		 return (SUPER_MUX_STATE_FIQ);
	if (reg & SUPER_MUX_STATE_BIT_IRQ)
		 return (SUPER_MUX_STATE_IRQ);
	if (reg & SUPER_MUX_STATE_BIT_RUN)
		 return (SUPER_MUX_STATE_RUN);
	if (reg & SUPER_MUX_STATE_BIT_IDLE)
		 return (SUPER_MUX_STATE_IDLE);
	return (SUPER_MUX_STATE_STDBY);
}

static int
super_mux_init(struct clknode *clk, device_t dev)
{
	struct super_mux_sc *sc;
	uint32_t reg;
	int shift, state;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(sc);
	RD4(sc, sc->base_reg, &reg);
	DEVICE_UNLOCK(sc);
	state = super_mux_get_state(reg);

	if ((state != SUPER_MUX_STATE_RUN) &&
	    (state != SUPER_MUX_STATE_IDLE)) {
		panic("Unexpected super mux state: %u", state);
	}

	shift = state * SUPER_MUX_MUX_WIDTH;
	sc->mux = (reg >> shift) & ((1 << SUPER_MUX_MUX_WIDTH) - 1);

	clknode_init_parent_idx(clk, sc->mux);

	return(0);
}

static int
super_mux_set_mux(struct clknode *clk, int idx)
{

	struct super_mux_sc *sc;
	int shift, state;
	uint32_t reg, dummy;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(sc);
	RD4(sc, sc->base_reg, &reg);
	state = super_mux_get_state(reg);

	if ((state != SUPER_MUX_STATE_RUN) &&
	    (state != SUPER_MUX_STATE_IDLE)) {
		panic("Unexpected super mux state: %u", state);
	}

	shift = (state - 1) * SUPER_MUX_MUX_WIDTH;
	sc->mux = idx;
	reg &= ~(((1 << SUPER_MUX_MUX_WIDTH) - 1) << shift);
	reg |= idx << shift;

	WR4(sc, sc->base_reg, reg);
	RD4(sc, sc->base_reg, &dummy);
	DEVICE_UNLOCK(sc);

	return(0);
}

static int
super_mux_register(struct clkdom *clkdom, struct super_mux_def *clkdef)
{
	struct clknode *clk;
	struct super_mux_sc *sc;

	clk = clknode_create(clkdom, &tegra210_super_mux_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->clkdev = clknode_get_device(clk);
	sc->base_reg = clkdef->base_reg;
	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);
	return (0);
}

void
tegra210_super_mux_clock(struct tegra210_car_softc *sc)
{
	int i, rv;

	for (i = 0; i <  nitems(super_mux_def); i++) {
		rv = super_mux_register(sc->clkdom, &super_mux_def[i]);
		if (rv != 0)
			panic("super_mux_register failed");
	}

}
