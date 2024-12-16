/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
 * Copyright (c) 2022 Mitchell Horne <mhorne@FreeBSD.org>
 * Copyright (c) 2024 Jari Sihvola <jsihv@gmx.com>
 */

/* Clocks for JH7110 SYS group. PLL driver must be attached before this. */

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
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include <dev/clk/starfive/jh7110_clk.h>
#include <dev/hwreset/hwreset.h>

#include <dt-bindings/clock/starfive,jh7110-crg.h>

#include "clkdev_if.h"
#include "hwreset_if.h"

static struct ofw_compat_data compat_data[] = {
	{ "starfive,jh7110-syscrg",	1 },
	{ NULL,				0 }
};

static struct resource_spec res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE | RF_SHAREABLE },
	RESOURCE_SPEC_END
};

/* parents for non-pll SYS clocks */
static const char *cpu_root_p[] = { "osc", "pll0_out" };
static const char *cpu_core_p[] = { "cpu_root" };
static const char *cpu_bus_p[] = { "cpu_core" };
static const char *perh_root_p[] = { "pll0_out", "pll2_out" };
static const char *bus_root_p[] = { "osc", "pll2_out" };

static const char *apb_bus_p[] = { "stg_axiahb" };
static const char *apb0_p[] = { "apb_bus" };
static const char *u0_sys_iomux_apb_p[] = { "apb_bus" };
static const char *stg_axiahb_p[] = { "axi_cfg0" };
static const char *ahb0_p[] = { "stg_axiahb" };
static const char *axi_cfg0_p[] = { "bus_root" };
static const char *nocstg_bus_p[] = { "bus_root" };
static const char *noc_bus_stg_axi_p[] = { "nocstg_bus" };

static const char *u0_dw_uart_clk_apb_p[] = { "apb0" };
static const char *u0_dw_uart_clk_core_p[] = { "osc" };
static const char *u0_dw_sdio_clk_ahb_p[] = { "ahb0" };
static const char *u0_dw_sdio_clk_sdcard_p[] = { "axi_cfg0" };
static const char *u1_dw_uart_clk_apb_p[] = { "apb0" };
static const char *u1_dw_uart_clk_core_p[] = { "osc" };
static const char *u1_dw_sdio_clk_ahb_p[] = { "ahb0" };
static const char *u1_dw_sdio_clk_sdcard_p[] = { "axi_cfg0" };
static const char *usb_125m_p[] = { "pll0_out" };
static const char *u2_dw_uart_clk_apb_p[] = { "apb0" };
static const char *u2_dw_uart_clk_core_p[] = { "osc" };
static const char *u3_dw_uart_clk_apb_p[] = { "apb0" };
static const char *u3_dw_uart_clk_core_p[] = { "perh_root" };

static const char *gmac_src_p[] = { "pll0_out" };
static const char *gmac_phy_p[] = { "gmac_src" };
static const char *gmac0_gtxclk_p[] = { "pll0_out" };
static const char *gmac0_ptp_p[] = { "gmac_src" };
static const char *gmac0_gtxc_p[] = { "gmac0_gtxclk" };
static const char *gmac1_gtxclk_p[] = { "pll0_out" };
static const char *gmac1_gtxc_p[] = { "gmac1_gtxclk" };
static const char *gmac1_rmii_rtx_p[] = { "gmac1_rmii_refin" };
static const char *gmac1_axi_p[] = { "stg_axiahb" };
static const char *gmac1_ahb_p[] = { "ahb0" };
static const char *gmac1_ptp_p[] = { "gmac_src" };
static const char *gmac1_tx_inv_p[] = { "gmac1_tx" };
static const char *gmac1_tx_p[] = { "gmac1_gtxclk", "gmac1_rmii_rtx" };
static const char *gmac1_rx_p[] = { "gmac1_rgmii_rxin", "gmac1_rmii_rtx" };
static const char *gmac1_rx_inv_p[] = { "gmac1_rx" };

/* non-pll SYS clocks */
static const struct jh7110_clk_def sys_clks[] = {
	JH7110_MUX(JH7110_SYSCLK_CPU_ROOT, "cpu_root", cpu_root_p),
	JH7110_DIV(JH7110_SYSCLK_CPU_CORE, "cpu_core", cpu_core_p, 7),
	JH7110_DIV(JH7110_SYSCLK_CPU_BUS, "cpu_bus", cpu_bus_p, 2),
	JH7110_GATEDIV(JH7110_SYSCLK_PERH_ROOT, "perh_root", perh_root_p, 2),
	JH7110_MUX(JH7110_SYSCLK_BUS_ROOT, "bus_root", bus_root_p),

	JH7110_GATE(JH7110_SYSCLK_APB0, "apb0", apb0_p),
	JH7110_GATE(JH7110_SYSCLK_IOMUX_APB, "u0_sys_iomux_apb",
	    u0_sys_iomux_apb_p),
	JH7110_GATE(JH7110_SYSCLK_UART0_APB, "u0_dw_uart_clk_apb",
	    u0_dw_uart_clk_apb_p),
	JH7110_GATE(JH7110_SYSCLK_UART0_CORE, "u0_dw_uart_clk_core",
	    u0_dw_uart_clk_core_p),
	JH7110_GATE(JH7110_SYSCLK_UART1_APB, "u1_dw_uart_clk_apb",
	    u1_dw_uart_clk_apb_p),
	JH7110_GATE(JH7110_SYSCLK_UART1_CORE, "u1_dw_uart_clk_core",
	    u1_dw_uart_clk_core_p),
	JH7110_GATE(JH7110_SYSCLK_UART2_APB, "u2_dw_uart_clk_apb",
	    u2_dw_uart_clk_apb_p),
	JH7110_GATE(JH7110_SYSCLK_UART2_CORE, "u2_dw_uart_clk_core",
	    u2_dw_uart_clk_core_p),
	JH7110_GATE(JH7110_SYSCLK_UART3_APB, "u3_dw_uart_clk_apb",
	    u3_dw_uart_clk_apb_p),
	JH7110_GATE(JH7110_SYSCLK_UART3_CORE, "u3_dw_uart_clk_core",
	    u3_dw_uart_clk_core_p),

	JH7110_DIV(JH7110_SYSCLK_AXI_CFG0, "axi_cfg0", axi_cfg0_p, 3),
	JH7110_DIV(JH7110_SYSCLK_STG_AXIAHB, "stg_axiahb", stg_axiahb_p, 2),
	JH7110_DIV(JH7110_SYSCLK_NOCSTG_BUS, "nocstg_bus", nocstg_bus_p, 3),
	JH7110_GATE(JH7110_SYSCLK_NOC_BUS_STG_AXI, "noc_bus_stg_axi",
	    noc_bus_stg_axi_p),
	JH7110_GATE(JH7110_SYSCLK_AHB0, "ahb0", ahb0_p),
	JH7110_DIV(JH7110_SYSCLK_APB_BUS, "apb_bus", apb_bus_p, 8),

	JH7110_GATE(JH7110_SYSCLK_SDIO0_AHB, "u0_dw_sdio_clk_ahb",
	    u0_dw_sdio_clk_ahb_p),
	JH7110_GATE(JH7110_SYSCLK_SDIO1_AHB, "u1_dw_sdio_clk_ahb",
	    u1_dw_sdio_clk_ahb_p),
	JH7110_GATEDIV(JH7110_SYSCLK_SDIO0_SDCARD, "u0_dw_sdio_clk_sdcard",
	    u0_dw_sdio_clk_sdcard_p, 15),
	JH7110_GATEDIV(JH7110_SYSCLK_SDIO1_SDCARD, "u1_dw_sdio_clk_sdcard",
	    u1_dw_sdio_clk_sdcard_p, 15),
	JH7110_DIV(JH7110_SYSCLK_USB_125M, "usb_125m", usb_125m_p, 15),

	JH7110_DIV(JH7110_SYSCLK_GMAC_SRC, "gmac_src", gmac_src_p, 7),
	JH7110_GATEDIV(JH7110_SYSCLK_GMAC0_GTXCLK, "gmac0_gtxclk",
	    gmac0_gtxclk_p, 15),
	JH7110_GATEDIV(JH7110_SYSCLK_GMAC0_PTP, "gmac0_ptp", gmac0_ptp_p, 31),
	JH7110_GATEDIV(JH7110_SYSCLK_GMAC_PHY, "gmac_phy", gmac_phy_p, 31),
	JH7110_GATE(JH7110_SYSCLK_GMAC0_GTXC, "gmac0_gtxc", gmac0_gtxc_p),

	JH7110_MUX(JH7110_SYSCLK_GMAC1_RX, "gmac1_rx", gmac1_rx_p),
	JH7110_INV(JH7110_SYSCLK_GMAC1_RX_INV, "gmac1_rx_inv", gmac1_rx_inv_p),
	JH7110_GATE(JH7110_SYSCLK_GMAC1_AHB, "gmac1_ahb", gmac1_ahb_p),
	JH7110_DIV(JH7110_SYSCLK_GMAC1_GTXCLK, "gmac1_gtxclk",
	    gmac1_gtxclk_p, 15),
	JH7110_GATEMUX(JH7110_SYSCLK_GMAC1_TX, "gmac1_tx", gmac1_tx_p),
	JH7110_INV(JH7110_SYSCLK_GMAC1_TX_INV, "gmac1_tx_inv", gmac1_tx_inv_p),
	JH7110_GATEDIV(JH7110_SYSCLK_GMAC1_PTP, "gmac1_ptp", gmac1_ptp_p, 31),
	JH7110_GATE(JH7110_SYSCLK_GMAC1_AXI, "gmac1_axi", gmac1_axi_p),
	JH7110_GATE(JH7110_SYSCLK_GMAC1_GTXC, "gmac1_gtxc", gmac1_gtxc_p),
	JH7110_DIV(JH7110_SYSCLK_GMAC1_RMII_RTX, "gmac1_rmii_rtx",
	    gmac1_rmii_rtx_p, 30),
};

static int
jh7110_clk_sys_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "StarFive JH7110 SYS clock generator");

	return (BUS_PROBE_DEFAULT);
}

static int
jh7110_clk_sys_attach(device_t dev)
{
	struct jh7110_clkgen_softc *sc;
	int i, error;

	sc = device_get_softc(dev);

	sc->reset_status_offset = SYSCRG_RESET_STATUS;
	sc->reset_selector_offset = SYSCRG_RESET_SELECTOR;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/* Allocate memory groups */
	error = bus_alloc_resources(dev, res_spec, &sc->mem_res);
	if (error != 0) {
		device_printf(dev, "Couldn't allocate resources, error %d\n",
		    error);
		return (ENXIO);
	}

	/* Create clock domain */
	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Couldn't create clkdom\n");
		return (ENXIO);
	}

	/* Register clocks */
	for (i = 0; i < nitems(sys_clks); i++) {
		error = jh7110_clk_register(sc->clkdom, &sys_clks[i]);
		if (error != 0) {
			device_printf(dev, "Couldn't register clock %s: %d\n",
			    sys_clks[i].clkdef.name, error);
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

static int
jh7110_clk_sys_detach(device_t dev)
{
	/* Detach not supported */
	return (EBUSY);
}

static void
jh7110_clk_sys_device_lock(device_t dev)
{
	struct jh7110_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
jh7110_clk_sys_device_unlock(device_t dev)
{
	struct jh7110_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static device_method_t jh7110_clk_sys_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		jh7110_clk_sys_probe),
	DEVMETHOD(device_attach,	jh7110_clk_sys_attach),
	DEVMETHOD(device_detach,	jh7110_clk_sys_detach),

	/* clkdev interface */
	DEVMETHOD(clkdev_device_lock,	jh7110_clk_sys_device_lock),
	DEVMETHOD(clkdev_device_unlock,	jh7110_clk_sys_device_unlock),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,	jh7110_reset_assert),
	DEVMETHOD(hwreset_is_asserted,	jh7110_reset_is_asserted),

	DEVMETHOD_END
};

DEFINE_CLASS_0(jh7110_clk_sys, jh7110_clk_sys_driver, jh7110_clk_sys_methods,
    sizeof(struct jh7110_clkgen_softc));
EARLY_DRIVER_MODULE(jh7110_clk_sys, simplebus, jh7110_clk_sys_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE);
MODULE_VERSION(jh7110_clk_sys, 1);
