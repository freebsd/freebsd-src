/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MediaTek MT7986 TOPCKGEN: fixed dividers of the PLL outputs and the top
 * level clock muxes.
 *
 * The muxes live in CLK_CFG_n registers (0x10 apart), four 8-bit fields per
 * register.  Each field holds the selection in its low bits and a gate bit
 * (set means gated) in bit 7.  The registers are changed by
 * read-modify-write, and a new selection takes effect after writing the
 * field's bit in one of the two update registers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/mediatek/mtk_clk.h>

#include <dt-bindings/clock/mt7986-clk.h>

#define	TOP_CLK_UPD0		0x1c0
#define	TOP_CLK_UPD1		0x1c4

#define	TOP_MUX(_id, _name, _parents, _reg, _shift, _width, _upd_reg,	\
    _upd_bit, _flags)							\
	{ .type = MTK_CLK_MUX, .mux = {					\
	    .clkdef = { .name = (_name), .id = (_id),			\
		.parent_names = (_parents),				\
		.parent_cnt = nitems(_parents), .flags = (_flags) },	\
	    .reg = (_reg), .shift = (_shift), .width = (_width),	\
	    .gate_bit = (_shift) + 7,					\
	    .upd_reg = (_upd_reg), .upd_bit = (_upd_bit) } }

static const char *nfi1x_parents[] = {
	"top_xtal", "top_mmpll_d8", "top_net1pll_d8_d2", "top_net2pll_d3_d2",
	"top_mpll_d4", "top_mmpll_d8_d2", "top_wedmcupll_d5_d2", "top_mpll_d8"
};
static const char *spinfi_parents[] = {
	"top_xtal_d2", "top_xtal", "top_net1pll_d5_d4", "top_mpll_d4",
	"top_mmpll_d8_d2", "top_wedmcupll_d5_d2", "top_mmpll_d3_d8",
	"top_mpll_d8"
};
static const char *spi_parents[] = {
	"top_xtal", "top_mpll_d2", "top_mmpll_d8", "top_net1pll_d8_d2",
	"top_net2pll_d3_d2", "top_net1pll_d5_d4", "top_mpll_d4",
	"top_wedmcupll_d5_d2"
};
static const char *uart_parents[] = {
	"top_xtal", "top_mpll_d8", "top_mpll_d8_d2"
};
static const char *pwm_parents[] = {
	"top_xtal", "top_net1pll_d8_d2", "top_net1pll_d5_d4", "top_mpll_d4"
};
static const char *i2c_parents[] = {
	"top_xtal", "top_net1pll_d5_d4", "top_mpll_d4", "top_net1pll_d8_d4"
};
static const char *pextp_tl_ck_parents[] = {
	"top_xtal", "top_net1pll_d5_d4", "top_net2pll_d4_d2", "top_rtc_32k"
};
static const char *emmc_250m_parents[] = { "top_xtal", "top_net1pll_d5_d2" };
static const char *emmc_416m_parents[] = { "top_xtal", "mpll" };
static const char *f_26m_adc_parents[] = { "top_xtal", "top_mpll_d8_d2" };
static const char *dramc_md32_parents[] = { "top_xtal", "top_mpll_d2" };
static const char *sysaxi_parents[] = {
	"top_xtal", "top_net1pll_d8_d2", "top_net2pll_d4"
};
static const char *sysapb_parents[] = {
	"top_xtal", "top_mpll_d3_d2", "top_net2pll_d4_d2"
};
static const char *arm_db_main_parents[] = { "top_xtal", "top_net2pll_d3_d2" };
static const char *arm_db_jtsel_parents[] = { "top_jtag", "top_xtal" };
static const char *netsys_parents[] = { "top_xtal", "top_mmpll_d4" };
static const char *netsys_500m_parents[] = { "top_xtal", "top_net1pll_d5" };
static const char *netsys_mcu_parents[] = {
	"top_xtal", "wedmcupll", "top_mmpll_d2", "top_net1pll_d4",
	"top_net1pll_d5"
};
static const char *netsys_2x_parents[] = {
	"top_xtal", "net2pll", "wedmcupll", "top_mmpll_d2"
};
static const char *sgm_325m_parents[] = { "top_xtal", "sgmpll" };
static const char *sgm_reg_parents[] = { "top_xtal", "top_net1pll_d8_d4" };
static const char *a1sys_parents[] = { "top_xtal", "top_apll2_d4" };
static const char *conn_mcusys_parents[] = { "top_xtal", "top_mmpll_d2" };
static const char *eip_b_parents[] = { "top_xtal", "net2pll" };
static const char *aud_l_parents[] = { "top_xtal", "apll2", "top_mpll_d8_d2" };
static const char *a_tuner_parents[] = {
	"top_xtal", "top_apll2_d4", "top_mpll_d8_d2"
};
static const char *u2u3_sys_parents[] = { "top_xtal", "top_net1pll_d5_d4" };
static const char *da_u2_refsel_parents[] = { "top_xtal", "top_mmpll_u2phy" };

static struct mtk_clk_def mt7986_topckgen_clks[] = {
	/* Inputs from the crystal and from APMIXEDSYS */
	MTK_LINK("clkxtal"),
	MTK_LINK("mpll"),
	MTK_LINK("mmpll"),
	MTK_LINK("apll2"),
	MTK_LINK("net1pll"),
	MTK_LINK("net2pll"),
	MTK_LINK("wedmcupll"),
	MTK_LINK("sgmpll"),

	MTK_FACTOR(CLK_TOP_XTAL,	"top_xtal",	"clkxtal", 1, 1),
	/*
	 * The external JTAG clock (TCK).  The rate is nominal, as in the
	 * Linux driver; the clock only feeds the debug clock mux.
	 */
	MTK_FIXED(CLK_TOP_JTAG,		"top_jtag",	50000000),

	MTK_FACTOR(CLK_TOP_XTAL_D2,	"top_xtal_d2",	"top_xtal", 1, 2),
	MTK_FACTOR(CLK_TOP_RTC_32K,	"top_rtc_32k",	"top_xtal", 1, 1250),
	MTK_FACTOR(CLK_TOP_RTC_32P7K,	"top_rtc_32p7k", "top_xtal", 1, 1220),
	MTK_FACTOR(CLK_TOP_MPLL_D2,	"top_mpll_d2",	"mpll", 1, 2),
	MTK_FACTOR(CLK_TOP_MPLL_D4,	"top_mpll_d4",	"mpll", 1, 4),
	MTK_FACTOR(CLK_TOP_MPLL_D8,	"top_mpll_d8",	"mpll", 1, 8),
	MTK_FACTOR(CLK_TOP_MPLL_D8_D2,	"top_mpll_d8_d2", "mpll", 1, 16),
	MTK_FACTOR(CLK_TOP_MPLL_D3_D2,	"top_mpll_d3_d2", "mpll", 1, 6),
	MTK_FACTOR(CLK_TOP_MMPLL_D2,	"top_mmpll_d2",	"mmpll", 1, 2),
	MTK_FACTOR(CLK_TOP_MMPLL_D4,	"top_mmpll_d4",	"mmpll", 1, 4),
	MTK_FACTOR(CLK_TOP_MMPLL_D8,	"top_mmpll_d8",	"mmpll", 1, 8),
	MTK_FACTOR(CLK_TOP_MMPLL_D8_D2,	"top_mmpll_d8_d2", "mmpll", 1, 16),
	MTK_FACTOR(CLK_TOP_MMPLL_D3_D8,	"top_mmpll_d3_d8", "mmpll", 1, 24),
	MTK_FACTOR(CLK_TOP_MMPLL_U2PHY,	"top_mmpll_u2phy", "mmpll", 1, 30),
	MTK_FACTOR(CLK_TOP_APLL2_D4,	"top_apll2_d4",	"apll2", 1, 4),
	MTK_FACTOR(CLK_TOP_NET1PLL_D4,	"top_net1pll_d4", "net1pll", 1, 4),
	MTK_FACTOR(CLK_TOP_NET1PLL_D5,	"top_net1pll_d5", "net1pll", 1, 5),
	MTK_FACTOR(CLK_TOP_NET1PLL_D5_D2, "top_net1pll_d5_d2", "net1pll",
	    1, 10),
	MTK_FACTOR(CLK_TOP_NET1PLL_D5_D4, "top_net1pll_d5_d4", "net1pll",
	    1, 20),
	MTK_FACTOR(CLK_TOP_NET1PLL_D8_D2, "top_net1pll_d8_d2", "net1pll",
	    1, 16),
	MTK_FACTOR(CLK_TOP_NET1PLL_D8_D4, "top_net1pll_d8_d4", "net1pll",
	    1, 32),
	MTK_FACTOR(CLK_TOP_NET2PLL_D4,	"top_net2pll_d4", "net2pll", 1, 4),
	MTK_FACTOR(CLK_TOP_NET2PLL_D4_D2, "top_net2pll_d4_d2", "net2pll", 1, 8),
	/* Divides by 6 (measured), not by 2 as in Linux and U-Boot. */
	MTK_FACTOR(CLK_TOP_NET2PLL_D3_D2, "top_net2pll_d3_d2", "net2pll", 1, 6),
	MTK_FACTOR(CLK_TOP_WEDMCUPLL_D5_D2, "top_wedmcupll_d5_d2", "wedmcupll",
	    1, 10),

	/* CLK_CFG_0 */
	TOP_MUX(CLK_TOP_NFI1X_SEL, "nfi1x_sel", nfi1x_parents,
	    0x000, 0, 3, TOP_CLK_UPD0, 0, 0),
	TOP_MUX(CLK_TOP_SPINFI_SEL, "spinfi_sel", spinfi_parents,
	    0x000, 8, 3, TOP_CLK_UPD0, 1, 0),
	TOP_MUX(CLK_TOP_SPI_SEL, "spi_sel", spi_parents,
	    0x000, 16, 3, TOP_CLK_UPD0, 2, 0),
	TOP_MUX(CLK_TOP_SPIM_MST_SEL, "spim_mst_sel", spi_parents,
	    0x000, 24, 3, TOP_CLK_UPD0, 3, 0),
	/* CLK_CFG_1 */
	TOP_MUX(CLK_TOP_UART_SEL, "uart_sel", uart_parents,
	    0x010, 0, 2, TOP_CLK_UPD0, 4, 0),
	TOP_MUX(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents,
	    0x010, 8, 2, TOP_CLK_UPD0, 5, 0),
	TOP_MUX(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents,
	    0x010, 16, 2, TOP_CLK_UPD0, 6, 0),
	TOP_MUX(CLK_TOP_PEXTP_TL_SEL, "pextp_tl_ck_sel", pextp_tl_ck_parents,
	    0x010, 24, 2, TOP_CLK_UPD0, 7, 0),
	/* CLK_CFG_2 */
	TOP_MUX(CLK_TOP_EMMC_250M_SEL, "emmc_250m_sel", emmc_250m_parents,
	    0x020, 0, 1, TOP_CLK_UPD0, 8, 0),
	TOP_MUX(CLK_TOP_EMMC_416M_SEL, "emmc_416m_sel", emmc_416m_parents,
	    0x020, 8, 1, TOP_CLK_UPD0, 9, 0),
	TOP_MUX(CLK_TOP_F_26M_ADC_SEL, "f_26m_adc_sel", f_26m_adc_parents,
	    0x020, 16, 1, TOP_CLK_UPD0, 10, 0),
	TOP_MUX(CLK_TOP_DRAMC_SEL, "dramc_sel", f_26m_adc_parents,
	    0x020, 24, 1, TOP_CLK_UPD0, 11, CLK_NODE_CANNOT_STOP),
	/* CLK_CFG_3 */
	TOP_MUX(CLK_TOP_DRAMC_MD32_SEL, "dramc_md32_sel", dramc_md32_parents,
	    0x030, 0, 1, TOP_CLK_UPD0, 12, CLK_NODE_CANNOT_STOP),
	TOP_MUX(CLK_TOP_SYSAXI_SEL, "sysaxi_sel", sysaxi_parents,
	    0x030, 8, 2, TOP_CLK_UPD0, 13, CLK_NODE_CANNOT_STOP),
	TOP_MUX(CLK_TOP_SYSAPB_SEL, "sysapb_sel", sysapb_parents,
	    0x030, 16, 2, TOP_CLK_UPD0, 14, CLK_NODE_CANNOT_STOP),
	TOP_MUX(CLK_TOP_ARM_DB_MAIN_SEL, "arm_db_main_sel", arm_db_main_parents,
	    0x030, 24, 1, TOP_CLK_UPD0, 15, 0),
	/* CLK_CFG_4 */
	TOP_MUX(CLK_TOP_ARM_DB_JTSEL, "arm_db_jtsel", arm_db_jtsel_parents,
	    0x040, 0, 1, TOP_CLK_UPD0, 16, 0),
	TOP_MUX(CLK_TOP_NETSYS_SEL, "netsys_sel", netsys_parents,
	    0x040, 8, 1, TOP_CLK_UPD0, 17, 0),
	TOP_MUX(CLK_TOP_NETSYS_500M_SEL, "netsys_500m_sel", netsys_500m_parents,
	    0x040, 16, 1, TOP_CLK_UPD0, 18, 0),
	TOP_MUX(CLK_TOP_NETSYS_MCU_SEL, "netsys_mcu_sel", netsys_mcu_parents,
	    0x040, 24, 3, TOP_CLK_UPD0, 19, 0),
	/* CLK_CFG_5 */
	TOP_MUX(CLK_TOP_NETSYS_2X_SEL, "netsys_2x_sel", netsys_2x_parents,
	    0x050, 0, 2, TOP_CLK_UPD0, 20, 0),
	TOP_MUX(CLK_TOP_SGM_325M_SEL, "sgm_325m_sel", sgm_325m_parents,
	    0x050, 8, 1, TOP_CLK_UPD0, 21, 0),
	TOP_MUX(CLK_TOP_SGM_REG_SEL, "sgm_reg_sel", sgm_reg_parents,
	    0x050, 16, 1, TOP_CLK_UPD0, 22, CLK_NODE_CANNOT_STOP),
	TOP_MUX(CLK_TOP_A1SYS_SEL, "a1sys_sel", a1sys_parents,
	    0x050, 24, 1, TOP_CLK_UPD0, 23, 0),
	/* CLK_CFG_6 */
	TOP_MUX(CLK_TOP_CONN_MCUSYS_SEL, "conn_mcusys_sel", conn_mcusys_parents,
	    0x060, 0, 1, TOP_CLK_UPD0, 24, 0),
	TOP_MUX(CLK_TOP_EIP_B_SEL, "eip_b_sel", eip_b_parents,
	    0x060, 8, 1, TOP_CLK_UPD0, 25, 0),
	TOP_MUX(CLK_TOP_PCIE_PHY_SEL, "pcie_phy_sel", f_26m_adc_parents,
	    0x060, 16, 1, TOP_CLK_UPD0, 26, 0),
	TOP_MUX(CLK_TOP_USB3_PHY_SEL, "usb3_phy_sel", f_26m_adc_parents,
	    0x060, 24, 1, TOP_CLK_UPD0, 27, 0),
	/* CLK_CFG_7 */
	TOP_MUX(CLK_TOP_F26M_SEL, "csw_f26m_sel", f_26m_adc_parents,
	    0x070, 0, 1, TOP_CLK_UPD0, 28, CLK_NODE_CANNOT_STOP),
	TOP_MUX(CLK_TOP_AUD_L_SEL, "aud_l_sel", aud_l_parents,
	    0x070, 8, 2, TOP_CLK_UPD0, 29, 0),
	TOP_MUX(CLK_TOP_A_TUNER_SEL, "a_tuner_sel", a_tuner_parents,
	    0x070, 16, 2, TOP_CLK_UPD0, 30, 0),
	TOP_MUX(CLK_TOP_U2U3_SEL, "u2u3_sel", f_26m_adc_parents,
	    0x070, 24, 1, TOP_CLK_UPD1, 0, 0),
	/* CLK_CFG_8 */
	TOP_MUX(CLK_TOP_U2U3_SYS_SEL, "u2u3_sys_sel", u2u3_sys_parents,
	    0x080, 0, 1, TOP_CLK_UPD1, 1, 0),
	TOP_MUX(CLK_TOP_U2U3_XHCI_SEL, "u2u3_xhci_sel", u2u3_sys_parents,
	    0x080, 8, 1, TOP_CLK_UPD1, 2, 0),
	TOP_MUX(CLK_TOP_DA_U2_REFSEL, "da_u2_refsel", da_u2_refsel_parents,
	    0x080, 16, 1, TOP_CLK_UPD1, 3, 0),
	TOP_MUX(CLK_TOP_DA_U2_CK_1P_SEL, "da_u2_ck_1p_sel",
	    da_u2_refsel_parents, 0x080, 24, 1, TOP_CLK_UPD1, 4, 0),
	/* CLK_CFG_9 */
	TOP_MUX(CLK_TOP_AP2CNN_HOST_SEL, "ap2cnn_host_sel", sgm_reg_parents,
	    0x090, 0, 1, TOP_CLK_UPD1, 5, 0),
};

static const struct mtk_clk_desc mt7986_topckgen_desc = {
	.clks = mt7986_topckgen_clks,
	.nclks = nitems(mt7986_topckgen_clks),
};

static struct ofw_compat_data compat_data[] = {
	{ "mediatek,mt7986-topckgen",	1 },
	{ NULL,				0 }
};

static int
mt7986_topckgen_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "MediaTek MT7986 top clock generator");
	return (BUS_PROBE_DEFAULT);
}

static int
mt7986_topckgen_attach(device_t dev)
{
	return (mtk_clk_attach(dev, &mt7986_topckgen_desc));
}

static device_method_t mt7986_topckgen_methods[] = {
	DEVMETHOD(device_probe,		mt7986_topckgen_probe),
	DEVMETHOD(device_attach,	mt7986_topckgen_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(mt7986_topckgen, mt7986_topckgen_driver,
    mt7986_topckgen_methods, sizeof(struct mtk_clk_softc), mtk_clk_driver);

/* After APMIXEDSYS: the critical muxes hold a reference on PLLs. */
EARLY_DRIVER_MODULE(mt7986_topckgen, simplebus, mt7986_topckgen_driver,
    NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 2);
