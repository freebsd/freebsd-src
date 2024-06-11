/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
 * Copyright (c) 2024 Jari Sihvola <jsihv@gmx.com>
 */

/* Clocks for JH7110 AON group. PLL driver must be attached before this. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/hwreset/hwreset.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include <dev/clk/starfive/jh7110_clk.h>

#include <dt-bindings/clock/starfive,jh7110-crg.h>

#include "clkdev_if.h"
#include "hwreset_if.h"

static struct ofw_compat_data compat_data[] = {
	{ "starfive,jh7110-aoncrg",	1 },
	{ NULL,				0 }
};

static struct resource_spec res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE | RF_SHAREABLE },
	RESOURCE_SPEC_END
};

/* parents */
static const char *gmac0_axi_p[] = { "stg_axiahb" };
static const char *gmac0_ahb_p[] = { "stg_axiahb" };
static const char *gmac0_tx_inv_p[] = { "gmac0_tx" };
static const char *gmac0_tx_p[] = { "gmac0_gtxclk", "gmac0_rmii_rtx" };
static const char *gmac0_rmii_rtx_p[] = { "gmac0_rmii_refin" };

/* AON clocks */
static const struct jh7110_clk_def aon_clks[] = {
	JH7110_GATE(JH7110_AONCLK_GMAC0_AXI, "gmac0_axi", gmac0_axi_p),
	JH7110_GATE(JH7110_AONCLK_GMAC0_AHB, "gmac0_ahb", gmac0_ahb_p),
	JH7110_GATEMUX(JH7110_AONCLK_GMAC0_TX, "gmac0_tx", gmac0_tx_p),
	JH7110_INV(JH7110_AONCLK_GMAC0_TX_INV, "gmac0_tx_inv", gmac0_tx_inv_p),
	JH7110_DIV(JH7110_AONCLK_GMAC0_RMII_RTX, "gmac0_rmii_rtx",
	    gmac0_rmii_rtx_p, 30),
};

static int
jh7110_clk_aon_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "StarFive JH7110 AON clock generator");

	return (BUS_PROBE_DEFAULT);
}

static int
jh7110_clk_aon_attach(device_t dev)
{
	struct jh7110_clkgen_softc *sc;
	int err;

	sc = device_get_softc(dev);

	sc->reset_status_offset = AONCRG_RESET_STATUS;
	sc->reset_selector_offset = AONCRG_RESET_SELECTOR;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	err = bus_alloc_resources(dev, res_spec, &sc->mem_res);
	if (err != 0) {
		device_printf(dev, "Couldn't allocate resources, error %d\n",
		    err);
		return (ENXIO);
	}

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Couldn't create clkdom, error %d\n", err);
		return (ENXIO);
	}

	for (int i = 0; i < nitems(aon_clks); i++) {
		err = jh7110_clk_register(sc->clkdom, &aon_clks[i]);
		if (err != 0) {
			device_printf(dev,
			    "Couldn't register clk %s, error %d\n",
				aon_clks[i].clkdef.name, err);
			return (ENXIO);
		}
	}

	if (clkdom_finit(sc->clkdom) != 0)
		panic("Cannot finalize clkdom initialization\n");

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	hwreset_register_ofw_provider(dev);

	return (0);
}

static void
jh7110_clk_aon_device_lock(device_t dev)
{
	struct jh7110_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
jh7110_clk_aon_device_unlock(device_t dev)
{
	struct jh7110_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static int
jh7110_clk_aon_detach(device_t dev)
{
	/* Detach not supported */
	return (EBUSY);
}

static device_method_t jh7110_clk_aon_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         jh7110_clk_aon_probe),
	DEVMETHOD(device_attach,	jh7110_clk_aon_attach),
	DEVMETHOD(device_detach,	jh7110_clk_aon_detach),

	/* clkdev interface */
	DEVMETHOD(clkdev_device_lock,	jh7110_clk_aon_device_lock),
	DEVMETHOD(clkdev_device_unlock, jh7110_clk_aon_device_unlock),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,       jh7110_reset_assert),
	DEVMETHOD(hwreset_is_asserted,  jh7110_reset_is_asserted),

	DEVMETHOD_END
};

DEFINE_CLASS_0(jh7110_aon, jh7110_aon_driver, jh7110_clk_aon_methods,
    sizeof(struct jh7110_clkgen_softc));
EARLY_DRIVER_MODULE(jh7110_aon, simplebus, jh7110_aon_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE);
MODULE_VERSION(jh7110_aon, 1);
