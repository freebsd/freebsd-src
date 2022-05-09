/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_gate.h>
#include <dev/extres/clk/clk_mux.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/clock/tegra210-car.h>

#include "clkdev_if.h"
#include "hwreset_if.h"
#include "tegra210_car.h"

static struct ofw_compat_data compat_data[] = {
	{"nvidia,tegra210-car",	1},
	{NULL,		 	0},
};

#define	PLIST(x) static const char *x[]

/* Pure multiplexer. */
#define	MUX(_id, cname, plists, o, s, w)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = plists,					\
	.clkdef.parent_cnt = nitems(plists),				\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.shift  = s,							\
	.width = w,							\
}

/* Fractional divider (7.1). */
#define	DIV7_1(_id, cname, plist, o, s)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags =  CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.i_shift = (s) + 1,						\
	.i_width = 7,							\
	.f_shift = s,							\
	.f_width = 1,							\
}

/* Integer divider. */
#define	DIV(_id, cname, plist, o, s, w, f)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags =  CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.i_shift = s,							\
	.i_width = w,							\
	.div_flags = f,							\
}

/* Gate in PLL block. */
#define	GATE_PLL(_id, cname, plist, o, s)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.shift = s,							\
	.mask = 3,							\
	.on_value = 3,							\
	.off_value = 0,							\
}

/* Standard gate. */
#define	GATE(_id, cname, plist, o, s)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.shift = s,							\
	.mask = 1,							\
	.on_value = 1,							\
	.off_value = 0,							\
}

/* Inverted gate. */
#define	GATE_INV(_id, cname, plist, o, s)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.shift = s,							\
	.mask = 1,							\
	.on_value = 0,							\
	.off_value = 1,							\
}

/* Fixed rate clock. */
#define	FRATE(_id, cname, _freq)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = NULL,					\
	.clkdef.parent_cnt = 0,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.freq = _freq,							\
}

/* Fixed rate multipier/divider. */
#define	FACT(_id, cname, pname, _mult, _div)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){pname},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.mult = _mult,							\
	.div = _div,							\
}

static uint32_t osc_freqs[16] = {
	 [0] =  13000000,
	 [1] =  16800000,
	 [4] =  19200000,
	 [5] =  38400000,
	 [8] =  12000000,
	 [9] =  48000000,
};


/* Parent lists. */
PLIST(mux_xusb_hs) = {"xusb_ss_div2", "pllU_60", "pc_xusb_ss" };
PLIST(mux_xusb_ssp) = {"xusb_ss", "osc_div_clk"};


/* Clocks adjusted online. */
static struct clk_fixed_def fixed_osc =
	FRATE(TEGRA210_CLK_CLK_M, "osc", 38400000);
static struct clk_fixed_def fixed_clk_m =
	FACT(0, "clk_m", "osc", 1, 1);
static struct clk_fixed_def fixed_osc_div =
	FACT(0, "osc_div_clk", "osc", 1, 1);

static struct clk_fixed_def tegra210_fixed_clks[] = {
	/* Core clocks. */
	FRATE(0, "bogus", 1),
	FRATE(0, "clk_s", 32768),

	/* Audio clocks. */
	FRATE(0, "vimclk_sync", 1),
	FRATE(0, "i2s1_sync", 1),
	FRATE(0, "i2s2_sync", 1),
	FRATE(0, "i2s3_sync", 1),
	FRATE(0, "i2s4_sync", 1),
	FRATE(0, "i2s5_sync", 1),
	FRATE(0, "spdif_in_sync", 1),

	/* XUSB */
	FACT(TEGRA210_CLK_XUSB_SS_DIV2, "xusb_ss_div2", "xusb_ss", 1, 2),

	/* SOR */
	FACT(0, "sor_safe_div", "pllP_out0", 1, 17),
	FACT(0, "dpaux_div", "sor_safe", 1, 17),
	FACT(0, "dpaux1_div", "sor_safe", 1, 17),

	/* Not Yet Implemented */
	FRATE(0, "audio", 10000000),
	FRATE(0, "audio0", 10000000),
	FRATE(0, "audio1", 10000000),
	FRATE(0, "audio2", 10000000),
	FRATE(0, "audio3", 10000000),
	FRATE(0, "audio4", 10000000),
	FRATE(0, "ext_vimclk", 10000000),
	FRATE(0, "audiod1", 10000000),
	FRATE(0, "audiod2", 10000000),
	FRATE(0, "audiod3", 10000000),
	FRATE(0, "dfllCPU_out", 10000000),

};


static struct clk_mux_def tegra210_mux_clks[] = {
	/* USB. */
	MUX(TEGRA210_CLK_XUSB_HS_SRC, "xusb_hs", mux_xusb_hs, CLK_SOURCE_XUSB_SS, 25, 2),
	MUX(0, "xusb_ssp", mux_xusb_ssp, CLK_SOURCE_XUSB_SS, 24, 1),

};


static struct clk_gate_def tegra210_gate_clks[] = {
	/* Base peripheral clocks. */
	GATE_INV(TEGRA210_CLK_HCLK, "hclk", "hclk_div", CLK_SYSTEM_RATE, 7),
	GATE_INV(TEGRA210_CLK_PCLK, "pclk", "pclk_div", CLK_SYSTEM_RATE, 3),
	GATE(TEGRA210_CLK_CML0, "cml0", "pllE_out0", PLLE_AUX, 0),
	GATE(TEGRA210_CLK_CML1, "cml1", "pllE_out0", PLLE_AUX, 1),
	GATE(0, "pllD_dsi_csi", "pllD_out0", PLLD_MISC, 21),
	GATE(0, "pllP_hsio", "pllP_out0", PLLP_MISC1, 29),
	GATE(0, "pllP_xusb", "pllP_hsio", PLLP_MISC1, 28),
};

static struct clk_div_def tegra210_div_clks[] = {
	/* Base peripheral clocks. */
	DIV(0, "hclk_div", "sclk", CLK_SYSTEM_RATE, 4, 2, 0),
	DIV(0, "pclk_div", "hclk", CLK_SYSTEM_RATE, 0, 2, 0),
};

/* Initial setup table. */
static struct  tegra210_init_item clk_init_table[] = {
	/* clock, partent, frequency, enable */
	{"uarta", "pllP_out0", 408000000, 0},
	{"uartb", "pllP_out0", 408000000, 0},
	{"uartc", "pllP_out0", 408000000, 0},
	{"uartd", "pllP_out0", 408000000, 0},
	{"pllA", NULL, 564480000, 1},
	{"pllA_out0", NULL, 11289600, 1},
	{"extperiph1", "pllA_out0", 0, 1},
	{"i2s1", "pllA_out0", 11289600, 0},
	{"i2s2", "pllA_out0", 11289600, 0},
	{"i2s3", "pllA_out0", 11289600, 0},
	{"i2s4", "pllA_out0", 11289600, 0},
	{"i2s5", "pllA_out0", 11289600, 0},
	{"host1x", "pllP_out0", 136000000, 1},
	{"sclk", "pllP_out2", 102000000, 1},
	{"dvfs_soc", "pllP_out0", 51000000, 1},
	{"dvfs_ref", "pllP_out0", 51000000, 1},
	{"spi4", "pllP_out0", 12000000, 1},
	{"pllREFE", NULL, 672000000, 0},

	{"xusb", NULL, 0, 1},
	{"xusb_ss", "pllU_480", 120000000, 0},
	{"pc_xusb_fs", "pllU_48", 48000000, 0},
	{"xusb_hs", "pc_xusb_ss", 120000000, 0},
	{"xusb_ssp", "xusb_ss", 120000000, 0},
	{"pc_xusb_falcon", "pllP_xusb", 204000000, 0},
	{"pc_xusb_core_host", "pllP_xusb", 102000000, 0},
	{"pc_xusb_core_dev", "pllP_xusb", 102000000, 0},

	{"sata", "pllP_out0", 104000000, 0},
	{"sata_oob", "pllP_out0", 204000000, 0},
	{"emc", NULL, 0, 1},
	{"mselect", NULL, 0, 1},
	{"csite", NULL, 0, 1},

	{"dbgapb", NULL, 0, 1 },
	{"tsensor", "clk_m", 400000, 0},
	{"i2c1", "pllP_out0", 0, 0},
	{"i2c2", "pllP_out0", 0, 0},
	{"i2c3", "pllP_out0", 0, 0},
	{"i2c4", "pllP_out0", 0, 0},
	{"i2c5", "pllP_out0", 0, 0},
	{"i2c6", "pllP_out0", 0, 0},

	{"pllDP_out0", NULL, 270000000, 0},
	{"soc_therm", "pllP_out0", 51000000, 0},
	{"cclk_g", NULL, 0, 1},
	{"pllU_out1", NULL, 48000000, 1},
	{"pllU_out2", NULL, 60000000, 1},
	{"pllC4",  NULL, 1000000000, 1},
	{"pllC4_out0", NULL, 1000000000, 1},
};

static void
init_divs(struct tegra210_car_softc *sc, struct clk_div_def *clks, int nclks)
{
	int i, rv;

	for (i = 0; i < nclks; i++) {
		rv = clknode_div_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clk_div_register failed");
	}
}

static void
init_gates(struct tegra210_car_softc *sc, struct clk_gate_def *clks, int nclks)
{
	int i, rv;


	for (i = 0; i < nclks; i++) {
		rv = clknode_gate_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clk_gate_register failed");
	}
}

static void
init_muxes(struct tegra210_car_softc *sc, struct clk_mux_def *clks, int nclks)
{
	int i, rv;


	for (i = 0; i < nclks; i++) {
		rv = clknode_mux_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clk_mux_register failed");
	}
}

static void
init_fixeds(struct tegra210_car_softc *sc, struct clk_fixed_def *clks,
    int nclks)
{
	int i, rv;
	uint32_t val;
	int osc_idx;

	CLKDEV_READ_4(sc->dev, OSC_CTRL, &val);
	osc_idx = OSC_CTRL_OSC_FREQ_GET(val);
	fixed_osc.freq = osc_freqs[osc_idx];
	if (fixed_osc.freq == 0)
		panic("Undefined input frequency");
	rv = clknode_fixed_register(sc->clkdom, &fixed_osc);
	if (rv != 0)
	    panic("clk_fixed_register failed");

	fixed_osc_div.div = 1 << OSC_CTRL_PLL_REF_DIV_GET(val);
	rv = clknode_fixed_register(sc->clkdom, &fixed_osc_div);
	if (rv != 0)
	    panic("clk_fixed_register failed");

	CLKDEV_READ_4(sc->dev, SPARE_REG0, &val);
	fixed_clk_m.div = SPARE_REG0_MDIV_GET(val) + 1;
	rv = clknode_fixed_register(sc->clkdom, &fixed_clk_m);
	if (rv != 0)
	    panic("clk_fixed_register failed");

	for (i = 0; i < nclks; i++) {
		rv = clknode_fixed_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clk_fixed_register failed");
	}
}

static void
postinit_clock(struct tegra210_car_softc *sc)
{
	int i;
	struct tegra210_init_item *tbl;
	struct clknode *clknode;
	int rv;

	for (i = 0; i < nitems(clk_init_table); i++) {
		tbl = &clk_init_table[i];

		clknode =  clknode_find_by_name(tbl->name);
		if (clknode == NULL) {
			device_printf(sc->dev, "Cannot find clock %s\n",
			    tbl->name);
			continue;
		}
		if (tbl->parent != NULL) {
			rv = clknode_set_parent_by_name(clknode, tbl->parent);
			if (rv != 0) {
				device_printf(sc->dev,
				    "Cannot set parent for %s (to %s): %d\n",
				    tbl->name, tbl->parent, rv);
				continue;
			}
		}
		if (tbl->frequency != 0) {
			rv = clknode_set_freq(clknode, tbl->frequency, 0 , 9999);
			if (rv != 0) {
				device_printf(sc->dev,
				    "Cannot set frequency for %s: %d\n",
				    tbl->name, rv);
				continue;
			}
		}
		if (tbl->enable!= 0) {
			rv = clknode_enable(clknode);
			if (rv != 0) {
				device_printf(sc->dev,
				    "Cannot enable %s: %d\n", tbl->name, rv);
				continue;
			}
		}
	}
}

static void
register_clocks(device_t dev)
{
	struct tegra210_car_softc *sc;

	sc = device_get_softc(dev);
	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL)
		panic("clkdom == NULL");

	init_fixeds(sc, tegra210_fixed_clks, nitems(tegra210_fixed_clks));
	tegra210_init_plls(sc);
	init_muxes(sc, tegra210_mux_clks, nitems(tegra210_mux_clks));
	init_divs(sc, tegra210_div_clks, nitems(tegra210_div_clks));
	init_gates(sc, tegra210_gate_clks, nitems(tegra210_gate_clks));
	tegra210_periph_clock(sc);
	tegra210_super_mux_clock(sc);
	clkdom_finit(sc->clkdom);
	clkdom_xlock(sc->clkdom);
	postinit_clock(sc);
	clkdom_unlock(sc->clkdom);
	if (bootverbose)
		clkdom_dump(sc->clkdom);
}

static int
tegra210_car_clkdev_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct tegra210_car_softc *sc;

	sc = device_get_softc(dev);
	*val = bus_read_4(sc->mem_res, addr);
	return (0);
}

static int
tegra210_car_clkdev_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct tegra210_car_softc *sc;

	sc = device_get_softc(dev);
	bus_write_4(sc->mem_res, addr, val);
	return (0);
}

static int
tegra210_car_clkdev_modify_4(device_t dev, bus_addr_t addr, uint32_t clear_mask,
    uint32_t set_mask)
{
	struct tegra210_car_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	reg = bus_read_4(sc->mem_res, addr);
	reg &= ~clear_mask;
	reg |= set_mask;
	bus_write_4(sc->mem_res, addr, reg);
	return (0);
}

static void
tegra210_car_clkdev_device_lock(device_t dev)
{
	struct tegra210_car_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
tegra210_car_clkdev_device_unlock(device_t dev)
{
	struct tegra210_car_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static int
tegra210_car_detach(device_t dev)
{

	device_printf(dev, "Error: Clock driver cannot be detached\n");
	return (EBUSY);
}

static int
tegra210_car_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "Tegra Clock Driver");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
tegra210_car_attach(device_t dev)
{
	struct tegra210_car_softc *sc = device_get_softc(dev);
	int rid, rv;

	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	/* Resource setup. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->mem_res) {
		device_printf(dev, "cannot allocate memory resource\n");
		rv = ENXIO;
		goto fail;
	}

	register_clocks(dev);
	hwreset_register_ofw_provider(dev);
	return (0);

fail:
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (rv);
}

static int
tegra210_car_hwreset_assert(device_t dev, intptr_t id, bool value)
{
	struct tegra210_car_softc *sc = device_get_softc(dev);

	return (tegra210_hwreset_by_idx(sc, id, value));
}

static device_method_t tegra210_car_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tegra210_car_probe),
	DEVMETHOD(device_attach,	tegra210_car_attach),
	DEVMETHOD(device_detach,	tegra210_car_detach),

	/* Clkdev interface*/
	DEVMETHOD(clkdev_read_4,	tegra210_car_clkdev_read_4),
	DEVMETHOD(clkdev_write_4,	tegra210_car_clkdev_write_4),
	DEVMETHOD(clkdev_modify_4,	tegra210_car_clkdev_modify_4),
	DEVMETHOD(clkdev_device_lock,	tegra210_car_clkdev_device_lock),
	DEVMETHOD(clkdev_device_unlock,	tegra210_car_clkdev_device_unlock),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,	tegra210_car_hwreset_assert),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(car, tegra210_car_driver, tegra210_car_methods,
    sizeof(struct tegra210_car_softc));
EARLY_DRIVER_MODULE(tegra210_car, simplebus, tegra210_car_driver, NULL, NULL,
    BUS_PASS_TIMER);
