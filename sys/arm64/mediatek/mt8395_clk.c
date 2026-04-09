/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 *
 * MediaTek MT8395 (Genio 1200) / MT8195 Clock Driver
 *
 * This driver handles the TOPCKGEN (top clock generator) and stubs for
 * INFRACFG_AO / PERICFG_AO clock gates on the MT8395 SoC.
 *
 * Clock tree summary:
 *   XTAL 26MHz -> APMIXEDSYS (PLLs) -> TOPCKGEN (muxes) -> peripheral gates
 *
 * The driver registers enough clocks for the kernel to bring up:
 *   - UART (early console, 921600n8 on UART0)
 *   - I2C (PMIC, touchscreen)
 *   - eMMC/SD (MSDC0/1)
 *   - USB3 (xHCI)
 *   - Ethernet (RGMII)
 *   - PCIe
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_link.h>
#include <dev/clk/clk_mux.h>
#include <dev/clk/clk_gate.h>
#include <dev/clk/clk_div.h>

#include <dev/hwreset/hwreset.h>

#include "mdtk_clk.h"
#include "mt8395_clk.h"

/* ------------------------------------------------------------------ */
/* Fixed-rate PLLs (APMIXEDSYS outputs, approximated for boot)         */
/* These are the output frequencies of the PLLs at their typical       */
/* operating points. A full PLL driver would allow dynamic adjustment. */
/* ------------------------------------------------------------------ */
static struct clk_fixed_def mt8395_fixed_clks[] = {
	FRATE(0,  "clk26m",          26000000),	/* Crystal oscillator */
	FRATE(0,  "clk32k",             32768),	/* RTC oscillator */
	FRATE(0,  "clk13m",          13000000),	/* 26M / 2 */
	FRATE(CLK_APMIXED_MAINPLL, "mainpll",  2184000000U), /* 2184 MHz */
	FRATE(CLK_APMIXED_UNIVPLL, "univpll",  4896000000U), /* 4896 MHz */
	FRATE(CLK_APMIXED_MSDCPLL, "msdcpll",  384000000),  /* 384 MHz  */
	FRATE(CLK_APMIXED_MMPLL,   "mmpll",    2750000000U), /* 2750 MHz */
	FRATE(CLK_APMIXED_TVDPLL1, "tvdpll1",  594000000),  /* 594 MHz  */
	FRATE(CLK_APMIXED_TVDPLL2, "tvdpll2",  594000000),  /* 594 MHz  */
	FRATE(CLK_APMIXED_APLL1,   "apll1",    180633600),  /* Audio    */
	FRATE(CLK_APMIXED_APLL2,   "apll2",    196608000),  /* Audio    */
	FRATE(CLK_APMIXED_MPLL,    "mpll",     1500000000U), /* 1500 MHz */
	FRATE(CLK_APMIXED_IMGPLL,  "imgpll",   3666000000U), /* 3666 MHz */
};

/* ------------------------------------------------------------------ */
/* Fixed-factor clocks derived from PLLs                               */
/* ------------------------------------------------------------------ */
static struct clk_fixed_def mt8395_ffact_clks[] = {
	FRATE(0, "mainpll_d3",   728000000),	/* MAINPLL / 3  */
	FRATE(0, "mainpll_d4",   546000000),	/* MAINPLL / 4  */
	FRATE(0, "mainpll_d5",   436800000),	/* MAINPLL / 5  */
	FRATE(0, "mainpll_d7",   312000000),	/* MAINPLL / 7  */
	FRATE(0, "univpll_d2",  2448000000U),	/* UNIVPLL / 2  */
	FRATE(0, "univpll_d3",  1632000000U),	/* UNIVPLL / 3  */
	FRATE(0, "univpll_d4",  1224000000U),	/* UNIVPLL / 4  */
	FRATE(0, "univpll_d5",   979200000),	/* UNIVPLL / 5  */
	FRATE(0, "univpll_d7",   699428571),	/* UNIVPLL / 7  */
	FRATE(0, "univpll_192m", 192000000),	/* UNIVPLL / 25.5 */
	FRATE(0, "univpll_d3_d4", 408000000),	/* UNIVPLL/3/4  */
	FRATE(0, "msdcpll_d2",  192000000),	/* MSDCPLL / 2  */
};

/* ------------------------------------------------------------------ */
/* TOPCKGEN mux parent lists                                           */
/* Mux inputs select which PLL derivative feeds each peripheral.       */
/* ------------------------------------------------------------------ */
PLIST(uart_parents)    = { "clk26m", "univpll_192m", "univpll_d3_d4" };
PLIST(i2c_parents)     = { "clk26m", "mainpll_d4",  "univpll_d5"    };
PLIST(spi_parents)     = { "clk26m", "mainpll_d5",  "univpll_d5",
			   "univpll_d7" };
PLIST(msdc50_0_parents) = { "clk26m", "msdcpll",  "univpll_d3",
			    "mainpll_d7", "univpll_d5", "msdcpll_d2" };
PLIST(msdc30_1_parents) = { "clk26m", "univpll_d5", "mainpll_d7",
			    "msdcpll_d2", "msdcpll" };
PLIST(ethernet_parents) = { "clk26m", "univpll_d4", "mainpll_d5", "clk13m" };
PLIST(usb_parents)     = { "clk26m", "univpll_d5", "univpll_d3_d4" };
PLIST(pcie_parents)    = { "clk26m", "univpll_192m", "univpll_d5",
			   "mainpll_d7" };

/* ------------------------------------------------------------------ */
/* TOPCKGEN mux clocks                                                 */
/* Register offsets from TOPCKGEN base (MT8195 TRM Table 4-1)         */
/* ------------------------------------------------------------------ */
#define MT8395_TOPCKGEN_BASE	0x10000000
#define CLK_CFG_0		0x010
#define CLK_CFG_1		0x020
#define CLK_CFG_2		0x030
#define CLK_CFG_3		0x040
#define CLK_CFG_7		0x080	/* UART, SPI */
#define CLK_CFG_8		0x090	/* MSDC50_0, MSDC50_0_HCLK */
#define CLK_CFG_9		0x0A0	/* MSDC30_1 */
#define CLK_CFG_15		0x100	/* I2C */
#define CLK_CFG_20		0x150	/* USB */
#define CLK_CFG_25		0x1A0	/* Ethernet */
#define CLK_CFG_28		0x1D0	/* PCIe */

static struct clk_mux_def mt8395_mux_clks[] = {
	MUX0(CLK_TOP_UART_SEL,    "uart_sel",    uart_parents,
	    CLK_CFG_7,  0, 3),
	MUX0(CLK_TOP_SPI_SEL,     "spi_sel",     spi_parents,
	    CLK_CFG_7,  8, 3),
	MUX0(CLK_TOP_I2C_SEL,     "i2c_sel",     i2c_parents,
	    CLK_CFG_15, 0, 3),
	MUX0(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel", msdc50_0_parents,
	    CLK_CFG_8,  0, 3),
	MUX0(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel", msdc30_1_parents,
	    CLK_CFG_9,  0, 3),
	MUX0(CLK_TOP_ETHERNET_SEL, "ethernet_sel", ethernet_parents,
	    CLK_CFG_25, 0, 3),
	MUX0(CLK_TOP_USBTOP_P0_SEL, "usbtop_p0_sel", usb_parents,
	    CLK_CFG_20, 0, 3),
	MUX0(CLK_TOP_PEXTP_P0_SEL, "pextp_p0_sel", pcie_parents,
	    CLK_CFG_28, 0, 3),
};

/* ------------------------------------------------------------------ */
/* INFRACFG_AO gate clocks                                             */
/* Gate register offsets from INFRACFG_AO base (0x10001000)           */
/* ------------------------------------------------------------------ */
#define MT8395_INFRACFG_BASE	0x10001000
#define INFRA_PDN0		0x080
#define INFRA_PDN1		0x088
#define INFRA_PDN2		0x090
#define INFRA_PDN3		0x098
#define INFRA_PDN4		0x0A0
#define INFRA_PDN5		0x0A8

static struct clk_gate_def mt8395_infra_gates[] = {
	/* INFRA_PDN0 */
	GATE(CLK_INFRA_UART0,   "infra_uart0",   "uart_sel",    INFRA_PDN0, 22),
	GATE(CLK_INFRA_UART1,   "infra_uart1",   "uart_sel",    INFRA_PDN0, 23),
	GATE(CLK_INFRA_UART2,   "infra_uart2",   "uart_sel",    INFRA_PDN0, 24),
	GATE(CLK_INFRA_UART3,   "infra_uart3",   "uart_sel",    INFRA_PDN0, 25),
	GATE(CLK_INFRA_UART4,   "infra_uart4",   "uart_sel",    INFRA_PDN0, 26),

	/* INFRA_PDN1 */
	GATE(CLK_INFRA_SPI0,    "infra_spi0",    "spi_sel",     INFRA_PDN1,  1),
	GATE(CLK_INFRA_SPI1,    "infra_spi1",    "spi_sel",     INFRA_PDN1,  2),
	GATE(CLK_INFRA_SPI2,    "infra_spi2",    "spi_sel",     INFRA_PDN1,  3),
	GATE(CLK_INFRA_SPI3,    "infra_spi3",    "spi_sel",     INFRA_PDN1,  4),
	GATE(CLK_INFRA_I2C0,    "infra_i2c0",    "i2c_sel",     INFRA_PDN1, 11),
	GATE(CLK_INFRA_I2C1,    "infra_i2c1",    "i2c_sel",     INFRA_PDN1, 12),
	GATE(CLK_INFRA_I2C2,    "infra_i2c2",    "i2c_sel",     INFRA_PDN1, 13),
	GATE(CLK_INFRA_I2C3,    "infra_i2c3",    "i2c_sel",     INFRA_PDN1, 14),
	GATE(CLK_INFRA_I2C4,    "infra_i2c4",    "i2c_sel",     INFRA_PDN1, 15),
	GATE(CLK_INFRA_I2C5,    "infra_i2c5",    "i2c_sel",     INFRA_PDN1, 16),
	GATE(CLK_INFRA_I2C6,    "infra_i2c6",    "i2c_sel",     INFRA_PDN1, 17),

	/* INFRA_PDN2 */
	GATE(CLK_INFRA_MSDC0_CK, "infra_msdc0",  "msdc50_0_sel", INFRA_PDN2,  0),
	GATE(CLK_INFRA_MSDC1_CK, "infra_msdc1",  "msdc30_1_sel", INFRA_PDN2,  1),
	GATE(CLK_INFRA_USB_CK,  "infra_usb",    "usbtop_p0_sel", INFRA_PDN2, 26),
};

/* ------------------------------------------------------------------ */
/* mdtk_clk_def aggregates all tables for mdtk_register_clocks()      */
/* ------------------------------------------------------------------ */
static struct mdtk_clk_def mt8395_topckgen_def = {
	.fixed_def  = mt8395_fixed_clks,
	.num_fixed  = nitems(mt8395_fixed_clks),
	.muxes_def  = mt8395_mux_clks,
	.num_muxes  = nitems(mt8395_mux_clks),
	.gates_def  = mt8395_infra_gates,
	.num_gates  = nitems(mt8395_infra_gates),
	.linked_def = NULL,
	.num_linked = 0,
	.dived_def  = NULL,
	.num_dived  = 0,
};

/* ------------------------------------------------------------------ */
/* Device driver                                                        */
/* ------------------------------------------------------------------ */
static struct ofw_compat_data mt8395_clk_compat[] = {
	{ "mediatek,mt8195-topckgen",   1 },
	{ "mediatek,mt8395-topckgen",   1 },
	{ NULL,                         0 },
};

static int
mt8395_clk_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, mt8395_clk_compat)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "MediaTek MT8395 Clock Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
mt8395_clk_attach(device_t dev)
{
	struct mdtk_clk_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resource\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	mdtk_register_clocks(dev, &mt8395_topckgen_def);

	return (0);
}

static int
mt8395_clk_detach(device_t dev)
{
	struct mdtk_clk_softc *sc;

	sc = device_get_softc(dev);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t mt8395_clk_methods[] = {
	DEVMETHOD(device_probe,  mt8395_clk_probe),
	DEVMETHOD(device_attach, mt8395_clk_attach),
	DEVMETHOD(device_detach, mt8395_clk_detach),
	DEVMETHOD_END
};

static driver_t mt8395_clk_driver = {
	"mt8395_clk",
	mt8395_clk_methods,
	sizeof(struct mdtk_clk_softc),
};

EARLY_DRIVER_MODULE(mt8395_clk, simplebus, mt8395_clk_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(mt8395_clk, 1);
