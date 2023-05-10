/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Axiado Corporation
 * All rights reserved.
 * Copyright (c) 2021 Jessica Clarke <jrtc27@FreeBSD.org>
 *
 * This software was developed in part by Kristof Provost under contract for
 * Axiado Corporation.
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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_gate.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "clkdev_if.h"
#include "hwreset_if.h"

static struct resource_spec prci_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	RESOURCE_SPEC_END
};

struct prci_softc {
	device_t		dev;

	struct mtx		mtx;

	struct clkdom		*clkdom;
	struct resource		*res;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;

	int			nresets;
};

struct prci_clk_pll_sc {
	struct prci_softc	*parent_sc;
	uint32_t		reg;
};

struct prci_clk_div_sc {
	struct prci_softc	*parent_sc;
	uint32_t		reg;
	uint32_t		bias;
};

#define	PRCI_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	PRCI_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	PRCI_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED);
#define	PRCI_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED);

#define	PRCI_PLL_DIVR_MASK		0x3f
#define	PRCI_PLL_DIVR_SHIFT		0
#define	PRCI_PLL_DIVF_MASK		0x7fc0
#define	PRCI_PLL_DIVF_SHIFT		6
#define	PRCI_PLL_DIVQ_MASK		0x38000
#define	PRCI_PLL_DIVQ_SHIFT		15

/* Called devicesresetreg on the FU540 */
#define	PRCI_DEVICES_RESET_N		0x28

#define	PRCI_READ(_sc, _reg)		\
    bus_space_read_4((_sc)->bst, (_sc)->bsh, (_reg))
#define	PRCI_WRITE(_sc, _reg, _val)	\
    bus_space_write_4((_sc)->bst, (_sc)->bsh, (_reg), (_val))

struct prci_pll_def {
	uint32_t	id;
	const char	*name;
	uint32_t	reg;
};

#define PLL(_id, _name, _base)					\
{								\
	.id = (_id),						\
	.name = (_name),					\
	.reg = (_base),						\
}

#define PLL_END	PLL(0, NULL, 0)

struct prci_div_def {
	uint32_t	id;
	const char	*name;
	const char	*parent_name;
	uint32_t	reg;
	uint32_t	bias;
};

#define DIV(_id, _name, _parent_name, _base, _bias)		\
{								\
	.id = (_id),						\
	.name = (_name),					\
	.parent_name = (_parent_name),				\
	.reg = (_base),						\
	.bias = (_bias),					\
}

#define DIV_END	DIV(0, NULL, NULL, 0, 0)

struct prci_gate_def {
	uint32_t	id;
	const char	*name;
	const char	*parent_name;
	uint32_t	reg;
};

#define GATE(_id, _name, _parent_name, _base)			\
{								\
	.id = (_id),						\
	.name = (_name),					\
	.parent_name = (_parent_name),				\
	.reg = (_base),						\
}

#define GATE_END	GATE(0, NULL, NULL, 0)

struct prci_config {
	struct prci_pll_def	*pll_clks;
	struct prci_div_def	*div_clks;
	struct prci_gate_def	*gate_clks;
	struct clk_fixed_def	*tlclk_def;
	int			nresets;
};

/* FU540 clock numbers */
#define	FU540_PRCI_CORECLK		0
#define	FU540_PRCI_DDRCLK		1
#define	FU540_PRCI_GEMGXLCLK		2
#define	FU540_PRCI_TLCLK		3

/* FU540 registers */
#define	FU540_PRCI_COREPLL_CFG0		0x4
#define	FU540_PRCI_DDRPLL_CFG0		0xC
#define	FU540_PRCI_GEMGXLPLL_CFG0	0x1C

/* FU540 PLL clocks */
static struct prci_pll_def fu540_pll_clks[] = {
	PLL(FU540_PRCI_CORECLK, "coreclk", FU540_PRCI_COREPLL_CFG0),
	PLL(FU540_PRCI_DDRCLK, "ddrclk", FU540_PRCI_DDRPLL_CFG0),
	PLL(FU540_PRCI_GEMGXLCLK, "gemgxlclk", FU540_PRCI_GEMGXLPLL_CFG0),
	PLL_END
};

/* FU540 fixed divisor clock TLCLK. */
static struct clk_fixed_def fu540_tlclk_def = {
	.clkdef.id = FU540_PRCI_TLCLK,
	.clkdef.name = "tlclk",
	.clkdef.parent_names = (const char *[]){"coreclk"},
	.clkdef.parent_cnt = 1,
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,
	.mult = 1,
	.div = 2,
};

/* FU540 config */
struct prci_config fu540_prci_config = {
	.pll_clks = fu540_pll_clks,
	.tlclk_def = &fu540_tlclk_def,
	.nresets = 6,
};

/* FU740 clock numbers */
#define	FU740_PRCI_CORECLK		0
#define	FU740_PRCI_DDRCLK		1
#define	FU740_PRCI_GEMGXLCLK		2
#define	FU740_PRCI_DVFSCORECLK		3
#define	FU740_PRCI_HFPCLK		4
#define	FU740_PRCI_CLTXCLK		5
#define	FU740_PRCI_TLCLK		6
#define	FU740_PRCI_PCLK			7
#define	FU740_PRCI_PCIEAUXCLK		8

/* FU740 registers */
#define	FU740_PRCI_COREPLL_CFG0		0x4
#define	FU740_PRCI_DDRPLL_CFG0		0xC
#define	FU740_PRCI_PCIEAUX_GATE		0x14
#define	FU740_PRCI_GEMGXLPLL_CFG0	0x1C
#define	FU740_PRCI_DVFSCOREPLL_CFG0	0x38
#define	FU740_PRCI_HFPCLKPLL_CFG0	0x50
#define	FU740_PRCI_CLTXPLL_CFG0		0x30
#define	FU740_PRCI_HFPCLK_DIV		0x5C

/* FU740 PLL clocks */
static struct prci_pll_def fu740_pll_clks[] = {
	PLL(FU740_PRCI_CORECLK, "coreclk", FU740_PRCI_COREPLL_CFG0),
	PLL(FU740_PRCI_DDRCLK, "ddrclk", FU740_PRCI_DDRPLL_CFG0),
	PLL(FU740_PRCI_GEMGXLCLK, "gemgxlclk", FU740_PRCI_GEMGXLPLL_CFG0),
	PLL(FU740_PRCI_DVFSCORECLK, "dvfscoreclk", FU740_PRCI_DVFSCOREPLL_CFG0),
	PLL(FU740_PRCI_HFPCLK, "hfpclk", FU740_PRCI_HFPCLKPLL_CFG0),
	PLL(FU740_PRCI_CLTXCLK, "cltxclk", FU740_PRCI_CLTXPLL_CFG0),
	PLL_END
};

/* FU740 divisor clocks */
static struct prci_div_def fu740_div_clks[] = {
	DIV(FU740_PRCI_PCLK, "pclk", "hfpclk", FU740_PRCI_HFPCLK_DIV, 2),
	DIV_END
};

/* FU740 gated clocks */
static struct prci_gate_def fu740_gate_clks[] = {
	GATE(FU740_PRCI_PCIEAUXCLK, "pcieauxclk", "hfclk", FU740_PRCI_PCIEAUX_GATE),
	GATE_END
};

/* FU740 fixed divisor clock TLCLK. */
static struct clk_fixed_def fu740_tlclk_def = {
	.clkdef.id = FU740_PRCI_TLCLK,
	.clkdef.name = "tlclk",
	.clkdef.parent_names = (const char *[]){"coreclk"},
	.clkdef.parent_cnt = 1,
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,
	.mult = 1,
	.div = 2,
};

/* FU740 config */
struct prci_config fu740_prci_config = {
	.pll_clks = fu740_pll_clks,
	.div_clks = fu740_div_clks,
	.gate_clks = fu740_gate_clks,
	.tlclk_def = &fu740_tlclk_def,
	.nresets = 7,
};

static struct ofw_compat_data compat_data[] = {
	{ "sifive,aloeprci0",		(uintptr_t)&fu540_prci_config },
	{ "sifive,ux00prci0",		(uintptr_t)&fu540_prci_config },
	{ "sifive,fu540-c000-prci",	(uintptr_t)&fu540_prci_config },
	{ "sifive,fu740-c000-prci",	(uintptr_t)&fu740_prci_config },
	{ NULL,				0 },
};

static int
prci_clk_pll_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
prci_clk_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct prci_clk_pll_sc *sc;
	struct clknode *parent_clk;
	uint32_t val;
	uint64_t refclk, divf, divq, divr;
	int err;

	KASSERT(freq != NULL, ("freq cannot be NULL"));

	sc = clknode_get_softc(clk);

	PRCI_LOCK(sc->parent_sc);

	/* Get refclock frequency. */
	parent_clk = clknode_get_parent(clk);
	err = clknode_get_freq(parent_clk, &refclk);
	if (err) {
		device_printf(sc->parent_sc->dev,
		    "Failed to get refclk frequency\n");
		PRCI_UNLOCK(sc->parent_sc);
		return (err);
	}

	/* Calculate the PLL output */
	val = PRCI_READ(sc->parent_sc, sc->reg);

	divf = (val & PRCI_PLL_DIVF_MASK) >> PRCI_PLL_DIVF_SHIFT;
	divq = (val & PRCI_PLL_DIVQ_MASK) >> PRCI_PLL_DIVQ_SHIFT;
	divr = (val & PRCI_PLL_DIVR_MASK) >> PRCI_PLL_DIVR_SHIFT;

	*freq = refclk / (divr + 1) * (2 * (divf + 1)) / (1 << divq);

	PRCI_UNLOCK(sc->parent_sc);

	return (0);
}

static clknode_method_t prci_clk_pll_clknode_methods[] = {
	CLKNODEMETHOD(clknode_init,		prci_clk_pll_init),
	CLKNODEMETHOD(clknode_recalc_freq,	prci_clk_pll_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(prci_clk_pll_clknode, prci_clk_pll_clknode_class,
    prci_clk_pll_clknode_methods, sizeof(struct prci_clk_pll_sc),
    clknode_class);

static int
prci_clk_div_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
prci_clk_div_recalc(struct clknode *clk, uint64_t *freq)
{
	struct prci_clk_div_sc *sc;
	struct clknode *parent_clk;
	uint32_t div;
	uint64_t refclk;
	int err;

	KASSERT(freq != NULL, ("freq cannot be NULL"));

	sc = clknode_get_softc(clk);

	PRCI_LOCK(sc->parent_sc);

	/* Get refclock frequency. */
	parent_clk = clknode_get_parent(clk);
	err = clknode_get_freq(parent_clk, &refclk);
	if (err) {
		device_printf(sc->parent_sc->dev,
		    "Failed to get refclk frequency\n");
		PRCI_UNLOCK(sc->parent_sc);
		return (err);
	}

	/* Calculate the divisor output */
	div = PRCI_READ(sc->parent_sc, sc->reg);

	*freq = refclk / (div + sc->bias);

	PRCI_UNLOCK(sc->parent_sc);

	return (0);
}

static clknode_method_t prci_clk_div_clknode_methods[] = {
	CLKNODEMETHOD(clknode_init,		prci_clk_div_init),
	CLKNODEMETHOD(clknode_recalc_freq,	prci_clk_div_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(prci_clk_div_clknode, prci_clk_div_clknode_class,
    prci_clk_div_clknode_methods, sizeof(struct prci_clk_div_sc),
    clknode_class);

static int
prci_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "SiFive Power Reset Clocking Interrupt");

	return (BUS_PROBE_DEFAULT);
}

static void
prci_pll_register(struct prci_softc *parent_sc, struct clknode_init_def *clkdef,
	uint32_t reg)
{
	struct clknode *clk;
	struct prci_clk_pll_sc *sc;

	clk = clknode_create(parent_sc->clkdom, &prci_clk_pll_clknode_class,
	    clkdef);
	if (clk == NULL)
		panic("Failed to create clknode");

	sc = clknode_get_softc(clk);
	sc->parent_sc = parent_sc;
	sc->reg = reg;

	clknode_register(parent_sc->clkdom, clk);
}

static void
prci_div_register(struct prci_softc *parent_sc, struct clknode_init_def *clkdef,
	uint32_t reg, uint32_t bias)
{
	struct clknode *clk;
	struct prci_clk_div_sc *sc;

	clk = clknode_create(parent_sc->clkdom, &prci_clk_div_clknode_class,
	    clkdef);
	if (clk == NULL)
		panic("Failed to create clknode");

	sc = clknode_get_softc(clk);
	sc->parent_sc = parent_sc;
	sc->reg = reg;
	sc->bias = bias;

	clknode_register(parent_sc->clkdom, clk);
}

static int
prci_attach(device_t dev)
{
	struct clknode_init_def clkdef, clkdef_div;
	struct clk_gate_def clkdef_gate;
	struct prci_softc *sc;
	clk_t clk_parent;
	phandle_t node;
	int i, ncells, error;
	struct prci_config *cfg;
	struct prci_pll_def *pll_clk;
	struct prci_div_def *div_clk;
	struct prci_gate_def *gate_clk;

	sc = device_get_softc(dev);
	sc->dev = dev;

	cfg = (struct prci_config *)ofw_bus_search_compatible(dev,
	    compat_data)->ocd_data;

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	error = bus_alloc_resources(dev, prci_spec, &sc->res);
	if (error) {
		device_printf(dev, "Couldn't allocate resources\n");
		goto fail;
	}
	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	node = ofw_bus_get_node(dev);
	error = ofw_bus_parse_xref_list_get_length(node, "clocks",
	    "#clock-cells", &ncells);
	if (error != 0 || ncells < 1) {
		device_printf(dev, "couldn't find parent clock\n");
		goto fail;
	}

	bzero(&clkdef, sizeof(clkdef));
	clkdef.parent_names = mallocarray(ncells, sizeof(char *), M_OFWPROP,
	    M_WAITOK);
	for (i = 0; i < ncells; i++) {
		error = clk_get_by_ofw_index(dev, 0, i, &clk_parent);
		if (error != 0) {
			device_printf(dev, "cannot get clock %d\n", error);
			goto fail1;
		}
		clkdef.parent_names[i] = clk_get_name(clk_parent);
		if (bootverbose)
			device_printf(dev, "clk parent: %s\n",
			    clkdef.parent_names[i]);
		clk_release(clk_parent);
	}
	clkdef.parent_cnt = ncells;

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Couldn't create clock domain\n");
		goto fail;
	}

	/* We can't free a clkdom, so from now on we cannot fail. */
	for (pll_clk = cfg->pll_clks; pll_clk->name; pll_clk++) {
		clkdef.id = pll_clk->id;
		clkdef.name = pll_clk->name;
		prci_pll_register(sc, &clkdef, pll_clk->reg);
	}

	if (cfg->div_clks != NULL) {
		bzero(&clkdef_div, sizeof(clkdef_div));
		for (div_clk = cfg->div_clks; div_clk->name; div_clk++) {
			clkdef_div.id = div_clk->id;
			clkdef_div.name = div_clk->name;
			clkdef_div.parent_names = &div_clk->parent_name;
			clkdef_div.parent_cnt = 1;
			prci_div_register(sc, &clkdef_div, div_clk->reg,
			    div_clk->bias);
		}
	}

	if (cfg->gate_clks != NULL) {
		bzero(&clkdef_gate, sizeof(clkdef_gate));
		for (gate_clk = cfg->gate_clks; gate_clk->name; gate_clk++) {
			clkdef_gate.clkdef.id = gate_clk->id;
			clkdef_gate.clkdef.name = gate_clk->name;
			clkdef_gate.clkdef.parent_names = &gate_clk->parent_name;
			clkdef_gate.clkdef.parent_cnt = 1;
			clkdef_gate.offset = gate_clk->reg;
			clkdef_gate.shift = 0;
			clkdef_gate.mask = 1;
			clkdef_gate.on_value = 1;
			clkdef_gate.off_value = 0;
			error = clknode_gate_register(sc->clkdom,
			    &clkdef_gate);
			if (error != 0) {
				device_printf(dev,
				    "Couldn't create gated clock %s: %d\n",
				    gate_clk->name, error);
				goto fail;
			}
		}
	}

	/*
	 * Register the fixed clock "tlclk".
	 *
	 * If an older device tree is being used, tlclk may appear as its own
	 * entity in the device tree, under soc/tlclk. If this is the case it
	 * will be registered automatically by the fixed_clk driver, and the
	 * version we register here will be an unreferenced duplicate.
	 */
	clknode_fixed_register(sc->clkdom, cfg->tlclk_def);

	error = clkdom_finit(sc->clkdom);
	if (error)
		panic("Couldn't finalise clock domain");

	sc->nresets = cfg->nresets;

	return (0);

fail1:
	free(clkdef.parent_names, M_OFWPROP);

fail:
	bus_release_resources(dev, prci_spec, &sc->res);
	mtx_destroy(&sc->mtx);
	return (error);
}

static int
prci_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct prci_softc *sc;

	sc = device_get_softc(dev);

	PRCI_WRITE(sc, addr, val);

	return (0);
}

static int
prci_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct prci_softc *sc;

	sc = device_get_softc(dev);

	*val = PRCI_READ(sc, addr);

	return (0);
}

static int
prci_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	struct prci_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	reg = PRCI_READ(sc, addr);
	reg &= ~clr;
	reg |= set;
	PRCI_WRITE(sc, addr, reg);

	return (0);
}

static void
prci_device_lock(device_t dev)
{
	struct prci_softc *sc;

	sc = device_get_softc(dev);
	PRCI_LOCK(sc);
}

static void
prci_device_unlock(device_t dev)
{
	struct prci_softc *sc;

	sc = device_get_softc(dev);
	PRCI_UNLOCK(sc);
}

static int
prci_reset_assert(device_t dev, intptr_t id, bool reset)
{
	struct prci_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (id >= sc->nresets)
		return (ENXIO);

	PRCI_LOCK(sc);
	reg = PRCI_READ(sc, PRCI_DEVICES_RESET_N);
	if (reset)
		reg &= ~(1u << id);
	else
		reg |= (1u << id);
	PRCI_WRITE(sc, PRCI_DEVICES_RESET_N, reg);
	PRCI_UNLOCK(sc);

	return (0);
}

static int
prci_reset_is_asserted(device_t dev, intptr_t id, bool *reset)
{
	struct prci_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (id >= sc->nresets)
		return (ENXIO);

	PRCI_LOCK(sc);
	reg = PRCI_READ(sc, PRCI_DEVICES_RESET_N);
	*reset = (reg & (1u << id)) == 0;
	PRCI_UNLOCK(sc);

	return (0);
}

static device_method_t prci_methods[] = {
	DEVMETHOD(device_probe,		prci_probe),
	DEVMETHOD(device_attach,	prci_attach),

	/* clkdev interface */
	DEVMETHOD(clkdev_write_4,	prci_write_4),
	DEVMETHOD(clkdev_read_4,	prci_read_4),
	DEVMETHOD(clkdev_modify_4,	prci_modify_4),
	DEVMETHOD(clkdev_device_lock,	prci_device_lock),
	DEVMETHOD(clkdev_device_unlock,	prci_device_unlock),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,	prci_reset_assert),
	DEVMETHOD(hwreset_is_asserted,	prci_reset_is_asserted),

	DEVMETHOD_END
};

static driver_t prci_driver = {
	"sifive_prci",
	prci_methods,
	sizeof(struct prci_softc)
};

/*
 * hfclk and rtcclk appear later in the device tree than prci, so we must
 * attach late.
 */
EARLY_DRIVER_MODULE(sifive_prci, simplebus, prci_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE);
