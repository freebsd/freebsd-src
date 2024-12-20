/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Jari Sihvola <jsihv@gmx.com>
 */

/* Clocks for STG group. PLL_OUT & SYS clocks must be registered first. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>
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
	{ "starfive,jh7110-stgcrg",	1 },
	{ NULL,				0 }
};

static struct resource_spec res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	RESOURCE_SPEC_END
};

/* parents */
static const char *e2_rtc_p[] = { "osc" };
static const char *e2_core_p[] = { "stg_axiahb" };
static const char *e2_dbg_p[] = { "stg_axiahb" };

static const char *pcie_slv_main_p[] = { "stg_axiahb" };
static const char *pcie0_tl_p[] = { "stg_axiahb" };
static const char *pcie1_tl_p[] = { "stg_axiahb" };
static const char *pcie0_axi_mst0_p[] = { "stg_axiahb" };
static const char *pcie1_axi_mst0_p[] = { "stg_axiahb" };
static const char *pcie0_apb_p[] = { "apb_bus" };
static const char *pcie1_apb_p[] = { "apb_bus" };

static const char *usb0_lpm_p[] = { "osc" };
static const char *usb0_stb_p[] = { "osc" };
static const char *usb0_apb_p[] = { "apb_bus" };
static const char *usb0_utmi_apb_p[] = { "apb_bus" };
static const char *usb0_axi_p[] = { "stg_axiahb" };
static const char *usb0_app_125_p[] = { "usb_125m" };
static const char *usb0_refclk_p[] = { "osc" };

static const char *dma1p_axi_p[] = { "stg_axiahb" };
static const char *dma1p_ahb_p[] = { "stg_axiahb" };

/* STG clocks */
static const struct jh7110_clk_def stg_clks[] = {
	JH7110_GATE(JH7110_STGCLK_USB0_APB, "usb0_apb", usb0_apb_p),
	JH7110_GATE(JH7110_STGCLK_USB0_UTMI_APB, "usb0_utmi_apb",
	    usb0_utmi_apb_p),
	JH7110_GATE(JH7110_STGCLK_USB0_AXI, "usb0_axi", usb0_axi_p),
	JH7110_GATEDIV(JH7110_STGCLK_USB0_LPM, "usb0_lpm", usb0_lpm_p, 2),
	JH7110_GATEDIV(JH7110_STGCLK_USB0_STB, "usb0_stb", usb0_stb_p, 4),
	JH7110_GATE(JH7110_STGCLK_USB0_APP_125, "usb0_app_125", usb0_app_125_p),
	JH7110_DIV(JH7110_STGCLK_USB0_REFCLK, "usb0_refclk", usb0_refclk_p, 2),

	JH7110_GATE(JH7110_STGCLK_PCIE0_AXI_MST0, "pcie0_axi_mst0",
	    pcie0_axi_mst0_p),
	JH7110_GATE(JH7110_STGCLK_PCIE0_APB, "pcie0_apb", pcie0_apb_p),
	JH7110_GATE(JH7110_STGCLK_PCIE0_TL, "pcie0_tl", pcie0_tl_p),
	JH7110_GATE(JH7110_STGCLK_PCIE1_AXI_MST0, "pcie1_axi_mst0",
	    pcie1_axi_mst0_p),

	JH7110_GATE(JH7110_STGCLK_PCIE1_APB, "pcie1_apb", pcie1_apb_p),
	JH7110_GATE(JH7110_STGCLK_PCIE1_TL, "pcie1_tl", pcie1_tl_p),
	JH7110_GATE(JH7110_STGCLK_PCIE_SLV_MAIN, "pcie_slv_main",
	    pcie_slv_main_p),

	JH7110_GATEDIV(JH7110_STGCLK_E2_RTC, "e2_rtc", e2_rtc_p, 24),
	JH7110_GATE(JH7110_STGCLK_E2_CORE, "e2_core", e2_core_p),
	JH7110_GATE(JH7110_STGCLK_E2_DBG, "e2_dbg", e2_dbg_p),

	JH7110_GATE(JH7110_STGCLK_DMA1P_AXI, "dma1p_axi", dma1p_axi_p),
	JH7110_GATE(JH7110_STGCLK_DMA1P_AHB, "dma1p_ahb", dma1p_ahb_p),
};

static int
jh7110_clk_stg_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "StarFive JH7110 STG clock generator");

	return (BUS_PROBE_DEFAULT);
}

static int
jh7110_clk_stg_attach(device_t dev)
{
	struct jh7110_clkgen_softc *sc;
	int err;

	sc = device_get_softc(dev);

	sc->reset_status_offset = STGCRG_RESET_STATUS;
	sc->reset_selector_offset = STGCRG_RESET_SELECTOR;

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

	for (int i = 0; i < nitems(stg_clks); i++) {
		err = jh7110_clk_register(sc->clkdom, &stg_clks[i]);
		if (err != 0) {
			device_printf(dev,
			    "Couldn't register clk %s, error %d\n",
				stg_clks[i].clkdef.name, err);
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
jh7110_clk_stg_device_lock(device_t dev)
{
	struct jh7110_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
jh7110_clk_stg_device_unlock(device_t dev)
{
	struct jh7110_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static int
jh7110_clk_stg_detach(device_t dev)
{
	/* Detach not supported */
	return (EBUSY);
}

static device_method_t jh7110_clk_stg_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         jh7110_clk_stg_probe),
	DEVMETHOD(device_attach,	jh7110_clk_stg_attach),
	DEVMETHOD(device_detach,	jh7110_clk_stg_detach),

	/* clkdev interface */
	DEVMETHOD(clkdev_device_lock,	jh7110_clk_stg_device_lock),
	DEVMETHOD(clkdev_device_unlock, jh7110_clk_stg_device_unlock),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,       jh7110_reset_assert),
	DEVMETHOD(hwreset_is_asserted,  jh7110_reset_is_asserted),

	DEVMETHOD_END
};

DEFINE_CLASS_0(jh7110_stg, jh7110_stg_driver, jh7110_clk_stg_methods,
    sizeof(struct jh7110_clkgen_softc));
EARLY_DRIVER_MODULE(jh7110_stg, simplebus, jh7110_stg_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE + 1);
MODULE_VERSION(jh7110_stg, 1);
