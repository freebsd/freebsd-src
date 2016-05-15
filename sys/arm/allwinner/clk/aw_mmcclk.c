/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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

/*
 * Allwinner MMC clocks
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/extres/clk/clk_mux.h>
#include <dev/extres/clk/clk_gate.h>

#include "clkdev_if.h"

#define	SCLK_GATING			(1 << 31)
#define	CLK_SRC_SEL			(0x3 << 24)
#define	CLK_SRC_SEL_SHIFT		24
#define	CLK_SRC_SEL_MAX			0x3
#define	CLK_SRC_SEL_OSC24M		0
#define	CLK_SRC_SEL_PLL6		1
#define	CLK_PHASE_CTR			(0x7 << 20)
#define	CLK_PHASE_CTR_SHIFT		20
#define	CLK_RATIO_N			(0x3 << 16)
#define	CLK_RATIO_N_SHIFT		16
#define	CLK_RATIO_N_MAX			0x3
#define	OUTPUT_CLK_PHASE_CTR		(0x7 << 8)
#define	OUTPUT_CLK_PHASE_CTR_SHIFT	8
#define	CLK_RATIO_M			(0xf << 0)
#define	CLK_RATIO_M_SHIFT		0
#define	CLK_RATIO_M_MAX			0xf

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun4i-a10-mmc-clk",	1 },
	{ NULL, 0 }
};

struct aw_mmcclk_sc {
	device_t	clkdev;
	bus_addr_t	reg;
};

#define	MODCLK_READ(sc, val)	CLKDEV_READ_4((sc)->clkdev, (sc)->reg, (val))
#define	MODCLK_WRITE(sc, val)	CLKDEV_WRITE_4((sc)->clkdev, (sc)->reg, (val))
#define	DEVICE_LOCK(sc)		CLKDEV_DEVICE_LOCK((sc)->clkdev)
#define	DEVICE_UNLOCK(sc)	CLKDEV_DEVICE_UNLOCK((sc)->clkdev)

static int
aw_mmcclk_init(struct clknode *clk, device_t dev)
{
	struct aw_mmcclk_sc *sc;
	uint32_t val, index;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(sc);
	MODCLK_READ(sc, &val);
	DEVICE_UNLOCK(sc);

	index = (val & CLK_SRC_SEL) >> CLK_SRC_SEL_SHIFT;

	clknode_init_parent_idx(clk, index);
	return (0);
}

static int
aw_mmcclk_set_mux(struct clknode *clk, int index)
{
	struct aw_mmcclk_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	if (index < 0 || index > CLK_SRC_SEL_MAX)
		return (ERANGE);

	DEVICE_LOCK(sc);
	MODCLK_READ(sc, &val);
	val &= ~CLK_SRC_SEL;
	val |= (index << CLK_SRC_SEL_SHIFT);
	MODCLK_WRITE(sc, val);
	DEVICE_UNLOCK(sc);

	return (0);
}

static int
aw_mmcclk_set_gate(struct clknode *clk, bool enable)
{
	struct aw_mmcclk_sc *sc;
	uint32_t val;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(sc);
	MODCLK_READ(sc, &val);
	if (enable)
		val |= SCLK_GATING;
	else
		val &= ~SCLK_GATING;
	MODCLK_WRITE(sc, val);
	DEVICE_UNLOCK(sc);

	return (0);
}

static int
aw_mmcclk_recalc_freq(struct clknode *clk, uint64_t *freq)
{
	struct aw_mmcclk_sc *sc;
	uint32_t val, m, n;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(sc);
	MODCLK_READ(sc, &val);
	DEVICE_UNLOCK(sc);

	n = 1 << ((val & CLK_RATIO_N) >> CLK_RATIO_N_SHIFT);
	m = ((val & CLK_RATIO_M) >> CLK_RATIO_M_SHIFT) + 1;

	*freq = *freq / n / m;

	return (0);
}

static int
aw_mmcclk_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	struct aw_mmcclk_sc *sc;
	uint32_t val, m, n, phase, ophase;
	int parent_idx, error;

	sc = clknode_get_softc(clk);

	/* XXX
	 * The ophase/phase values should be set by the MMC driver, but
	 * there is currently no way to do this with the clk API
	 */
	if (*fout <= 400000) {
		parent_idx = CLK_SRC_SEL_OSC24M;
		ophase = 0;
		phase = 0;
		n = 2;
	} else if (*fout <= 25000000) {
		parent_idx = CLK_SRC_SEL_PLL6;
		ophase = 0;
		phase = 5;
		n = 2;
	} else if (*fout <= 50000000) {
		parent_idx = CLK_SRC_SEL_PLL6;
		ophase = 3;
		phase = 5;
		n = 0;
	} else
		return (ERANGE);

	/* Switch parent clock, if necessary */
	if (parent_idx != clknode_get_parent_idx(clk)) {
		error = clknode_set_parent_by_idx(clk, parent_idx);
		if (error != 0)
			return (error);

		/* Fetch new input frequency */
		error = clknode_get_freq(clknode_get_parent(clk), &fin);
		if (error != 0)
			return (error);
	}

	m = ((fin / (1 << n)) / *fout) - 1;

	DEVICE_LOCK(sc);
	MODCLK_READ(sc, &val);
	val &= ~(CLK_RATIO_N | CLK_RATIO_M | CLK_PHASE_CTR |
	    OUTPUT_CLK_PHASE_CTR);
	val |= (n << CLK_RATIO_N_SHIFT);
	val |= (m << CLK_RATIO_M_SHIFT);
	val |= (phase << CLK_PHASE_CTR_SHIFT);
	val |= (ophase << OUTPUT_CLK_PHASE_CTR_SHIFT);
	MODCLK_WRITE(sc, val);
	DEVICE_UNLOCK(sc);

	*fout = fin / (1 << n) / (m + 1);
	*stop = 1;

	return (0);
}

static clknode_method_t aw_mmcclk_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		aw_mmcclk_init),
	CLKNODEMETHOD(clknode_set_gate,		aw_mmcclk_set_gate),
	CLKNODEMETHOD(clknode_set_mux,		aw_mmcclk_set_mux),
	CLKNODEMETHOD(clknode_recalc_freq,	aw_mmcclk_recalc_freq),
	CLKNODEMETHOD(clknode_set_freq,		aw_mmcclk_set_freq),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(aw_mmcclk_clknode, aw_mmcclk_clknode_class,
    aw_mmcclk_clknode_methods, sizeof(struct aw_mmcclk_sc), clknode_class);

static int
aw_mmcclk_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner MMC Clock");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_mmcclk_attach(device_t dev)
{
	struct clknode_init_def def;
	struct aw_mmcclk_sc *sc;
	struct clkdom *clkdom;
	struct clknode *clk;
	const char **names;
	uint32_t *indices;
	clk_t clk_parent;
	bus_addr_t paddr;
	bus_size_t psize;
	phandle_t node;
	int error, nout, ncells, i;

	node = ofw_bus_get_node(dev);

	if (ofw_reg_to_paddr(node, 0, &paddr, &psize, NULL) != 0) {
		device_printf(dev, "cannot parse 'reg' property\n");
		return (ENXIO);
	}

	error = ofw_bus_parse_xref_list_get_length(node, "clocks",
	    "#clock-cells", &ncells);
	if (error != 0 || ncells == 0) {
		device_printf(dev, "couldn't find parent clocks\n");
		return (ENXIO);
	}

	clkdom = clkdom_create(dev);

	nout = clk_parse_ofw_out_names(dev, node, &names, &indices);
	if (nout == 0) {
		device_printf(dev, "no output clocks found\n");
		error = ENXIO;
		goto fail;
	}

	memset(&def, 0, sizeof(def));
	def.name = names[0];
	def.id = 0;
	def.parent_names = malloc(sizeof(char *) * ncells, M_OFWPROP, M_WAITOK);
	for (i = 0; i < ncells; i++) {
		error = clk_get_by_ofw_index(dev, i, &clk_parent);
		if (error != 0) {
			device_printf(dev, "cannot get clock %d\n", i);
			goto fail;
		}
		def.parent_names[i] = clk_get_name(clk_parent);
		clk_release(clk_parent);
	}
	def.parent_cnt = ncells;
	def.flags = CLK_NODE_GLITCH_FREE;

	clk = clknode_create(clkdom, &aw_mmcclk_clknode_class, &def);
	if (clk == NULL) {
		device_printf(dev, "cannot create clknode\n");
		error = ENXIO;
		goto fail;
	}

	sc = clknode_get_softc(clk);
	sc->reg = paddr;
	sc->clkdev = device_get_parent(dev);

	clknode_register(clkdom, clk);

	if (clkdom_finit(clkdom) != 0) {
		device_printf(dev, "cannot finalize clkdom initialization\n");
		error = ENXIO;
		goto fail;
	}

	if (bootverbose)
		clkdom_dump(clkdom);

	return (0);

fail:
	return (error);
}

static device_method_t aw_mmcclk_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_mmcclk_probe),
	DEVMETHOD(device_attach,	aw_mmcclk_attach),

	DEVMETHOD_END
};

static driver_t aw_mmcclk_driver = {
	"aw_mmcclk",
	aw_mmcclk_methods,
	0
};

static devclass_t aw_mmcclk_devclass;

EARLY_DRIVER_MODULE(aw_mmcclk, simplebus, aw_mmcclk_driver,
    aw_mmcclk_devclass, 0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
