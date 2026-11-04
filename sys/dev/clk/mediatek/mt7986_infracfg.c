/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MediaTek MT7986 INFRACFG: peripheral clock selection muxes and the clock
 * gates of the infrastructure peripherals.
 *
 * The muxes are single fields in the MODULE_CLK_SEL registers, written
 * through set/clear registers and without gate or update bits.  The gates
 * come in three banks, each with set (gate), clear (ungate) and status
 * registers.
 *
 * INFRACFG is also the reset controller of the infrastructure peripherals,
 * with three banks of software resets.
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

/*
 * MODULE_CLK_SEL_n: set register at the base, clear at +4, status at +8.
 * Clock gate banks: set (gate) at the base, clear at +4, status at +8.
 */
#define	INFRA_SEL0		0x10
#define	INFRA_SEL1		0x20
#define	INFRA_CG0		0x40
#define	INFRA_CG1		0x50
#define	INFRA_CG2		0x60

#define	INFRA_MUX(_id, _name, _parents, _base, _shift, _width)		\
	{ .type = MTK_CLK_MUX, .mux = {					\
	    .clkdef = { .name = (_name), .id = (_id),			\
		.parent_names = (_parents),				\
		.parent_cnt = nitems(_parents) },			\
	    .reg = (_base) + 8, .set_reg = (_base), .clr_reg = (_base) + 4,\
	    .shift = (_shift), .width = (_width),			\
	    .gate_bit = -1, .upd_bit = -1, .flags = MTK_MUX_SETCLR } }

#define	INFRA_GATE_F(_id, _name, _parent, _base, _bit, _flags)		\
	{ .type = MTK_CLK_GATE, .gate = {				\
	    .clkdef = { .name = (_name), .id = (_id),			\
		.parent_names = (const char *[]){ _parent },		\
		.parent_cnt = 1, .flags = (_flags) },			\
	    .reg = (_base) + 8, .set_reg = (_base), .clr_reg = (_base) + 4,\
	    .bit = (_bit), .flags = MTK_GATE_SETCLR } }
#define	INFRA_GATE(_id, _name, _parent, _base, _bit)			\
	INFRA_GATE_F(_id, _name, _parent, _base, _bit, 0)

static const char *infra_uart_parents[] = { "csw_f26m_sel", "uart_sel" };
static const char *infra_spi0_parents[] = { "i2c_sel", "spi_sel" };
/* SPI1 uses spim_mst_sel, not spi_sel (MediaTek SDK, DTS, MT7981). */
static const char *infra_spi1_parents[] = { "i2c_sel", "spim_mst_sel" };
static const char *infra_pwm_bsel_parents[] = {
	"top_rtc_32p7k", "csw_f26m_sel", "infra_sysaxi_d2", "pwm_sel"
};
static const char *infra_pcie_parents[] = {
	"top_rtc_32p7k", "csw_f26m_sel", "top_xtal", "pextp_tl_ck_sel"
};

static struct mtk_clk_def mt7986_infracfg_clks[] = {
	/* Inputs from TOPCKGEN */
	MTK_LINK("top_xtal"),
	MTK_LINK("top_rtc_32k"),
	MTK_LINK("top_rtc_32p7k"),
	MTK_LINK("sysaxi_sel"),
	MTK_LINK("csw_f26m_sel"),
	MTK_LINK("uart_sel"),
	MTK_LINK("i2c_sel"),
	MTK_LINK("spi_sel"),
	MTK_LINK("spim_mst_sel"),
	MTK_LINK("pwm_sel"),
	MTK_LINK("pextp_tl_ck_sel"),
	MTK_LINK("eip_b_sel"),
	MTK_LINK("aud_l_sel"),
	MTK_LINK("a1sys_sel"),
	MTK_LINK("a_tuner_sel"),
	MTK_LINK("nfi1x_sel"),
	MTK_LINK("spinfi_sel"),
	MTK_LINK("emmc_416m_sel"),
	MTK_LINK("emmc_250m_sel"),
	MTK_LINK("u2u3_sel"),
	MTK_LINK("u2u3_sys_sel"),

	MTK_FACTOR(CLK_INFRA_SYSAXI_D2, "infra_sysaxi_d2", "sysaxi_sel", 1, 2),

	INFRA_MUX(CLK_INFRA_UART0_SEL, "infra_uart0_sel", infra_uart_parents,
	    INFRA_SEL0, 0, 1),
	INFRA_MUX(CLK_INFRA_UART1_SEL, "infra_uart1_sel", infra_uart_parents,
	    INFRA_SEL0, 1, 1),
	INFRA_MUX(CLK_INFRA_UART2_SEL, "infra_uart2_sel", infra_uart_parents,
	    INFRA_SEL0, 2, 1),
	INFRA_MUX(CLK_INFRA_SPI0_SEL, "infra_spi0_sel", infra_spi0_parents,
	    INFRA_SEL0, 4, 1),
	INFRA_MUX(CLK_INFRA_SPI1_SEL, "infra_spi1_sel", infra_spi1_parents,
	    INFRA_SEL0, 5, 1),
	INFRA_MUX(CLK_INFRA_PWM1_SEL, "infra_pwm1_sel", infra_pwm_bsel_parents,
	    INFRA_SEL0, 9, 2),
	INFRA_MUX(CLK_INFRA_PWM2_SEL, "infra_pwm2_sel", infra_pwm_bsel_parents,
	    INFRA_SEL0, 11, 2),
	INFRA_MUX(CLK_INFRA_PWM_BSEL, "infra_pwm_bsel", infra_pwm_bsel_parents,
	    INFRA_SEL0, 13, 2),
	INFRA_MUX(CLK_INFRA_PCIE_SEL, "infra_pcie_sel", infra_pcie_parents,
	    INFRA_SEL1, 0, 2),

	/* Bank 0 */
	INFRA_GATE(CLK_INFRA_GPT_STA, "infra_gpt_sta", "infra_sysaxi_d2",
	    INFRA_CG0, 0),
	INFRA_GATE(CLK_INFRA_PWM_HCK, "infra_pwm_hck", "infra_sysaxi_d2",
	    INFRA_CG0, 1),
	INFRA_GATE(CLK_INFRA_PWM_STA, "infra_pwm_sta", "infra_pwm_bsel",
	    INFRA_CG0, 2),
	INFRA_GATE(CLK_INFRA_PWM1_CK, "infra_pwm1", "infra_pwm1_sel",
	    INFRA_CG0, 3),
	INFRA_GATE(CLK_INFRA_PWM2_CK, "infra_pwm2", "infra_pwm2_sel",
	    INFRA_CG0, 4),
	INFRA_GATE(CLK_INFRA_CQ_DMA_CK, "infra_cq_dma", "sysaxi_sel",
	    INFRA_CG0, 6),
	INFRA_GATE(CLK_INFRA_EIP97_CK, "infra_eip97", "eip_b_sel",
	    INFRA_CG0, 7),
	INFRA_GATE(CLK_INFRA_AUD_BUS_CK, "infra_aud_bus", "sysaxi_sel",
	    INFRA_CG0, 8),
	INFRA_GATE(CLK_INFRA_AUD_26M_CK, "infra_aud_26m", "csw_f26m_sel",
	    INFRA_CG0, 9),
	INFRA_GATE(CLK_INFRA_AUD_L_CK, "infra_aud_l", "aud_l_sel",
	    INFRA_CG0, 10),
	INFRA_GATE(CLK_INFRA_AUD_AUD_CK, "infra_aud_aud", "a1sys_sel",
	    INFRA_CG0, 11),
	INFRA_GATE(CLK_INFRA_AUD_EG2_CK, "infra_aud_eg2", "a_tuner_sel",
	    INFRA_CG0, 13),
	INFRA_GATE(CLK_INFRA_DRAMC_26M_CK, "infra_dramc_26m", "csw_f26m_sel",
	    INFRA_CG0, 14),
	INFRA_GATE(CLK_INFRA_DBG_CK, "infra_dbg", "infra_sysaxi_d2",
	    INFRA_CG0, 15),
	INFRA_GATE(CLK_INFRA_AP_DMA_CK, "infra_ap_dma", "infra_sysaxi_d2",
	    INFRA_CG0, 16),
	/* Used by OP-TEE, so MediaTek marks them critical. */
	INFRA_GATE_F(CLK_INFRA_SEJ_CK, "infra_sej", "infra_sysaxi_d2",
	    INFRA_CG0, 24, CLK_NODE_CANNOT_STOP),
	INFRA_GATE_F(CLK_INFRA_SEJ_13M_CK, "infra_sej_13m", "csw_f26m_sel",
	    INFRA_CG0, 25, CLK_NODE_CANNOT_STOP),
	INFRA_GATE(CLK_INFRA_TRNG_CK, "infra_trng", "sysaxi_sel",
	    INFRA_CG0, 26),

	/* Bank 1 */
	INFRA_GATE(CLK_INFRA_THERM_CK, "infra_therm", "csw_f26m_sel",
	    INFRA_CG1, 0),
	INFRA_GATE(CLK_INFRA_I2C0_CK, "infra_i2c0", "i2c_sel",
	    INFRA_CG1, 1),
	INFRA_GATE(CLK_INFRA_UART0_CK, "infra_uart0", "infra_uart0_sel",
	    INFRA_CG1, 2),
	INFRA_GATE(CLK_INFRA_UART1_CK, "infra_uart1", "infra_uart1_sel",
	    INFRA_CG1, 3),
	INFRA_GATE(CLK_INFRA_UART2_CK, "infra_uart2", "infra_uart2_sel",
	    INFRA_CG1, 4),
	INFRA_GATE(CLK_INFRA_NFI1_CK, "infra_nfi1", "nfi1x_sel",
	    INFRA_CG1, 8),
	INFRA_GATE(CLK_INFRA_SPINFI1_CK, "infra_spinfi1", "spinfi_sel",
	    INFRA_CG1, 9),
	INFRA_GATE(CLK_INFRA_NFI_HCK_CK, "infra_nfi_hck", "infra_sysaxi_d2",
	    INFRA_CG1, 10),
	INFRA_GATE(CLK_INFRA_SPI0_CK, "infra_spi0", "infra_spi0_sel",
	    INFRA_CG1, 11),
	INFRA_GATE(CLK_INFRA_SPI1_CK, "infra_spi1", "infra_spi1_sel",
	    INFRA_CG1, 12),
	INFRA_GATE(CLK_INFRA_SPI0_HCK_CK, "infra_spi0_hck", "infra_sysaxi_d2",
	    INFRA_CG1, 13),
	INFRA_GATE(CLK_INFRA_SPI1_HCK_CK, "infra_spi1_hck", "infra_sysaxi_d2",
	    INFRA_CG1, 14),
	INFRA_GATE(CLK_INFRA_FRTC_CK, "infra_frtc", "top_rtc_32k",
	    INFRA_CG1, 15),
	INFRA_GATE(CLK_INFRA_MSDC_CK, "infra_msdc", "emmc_416m_sel",
	    INFRA_CG1, 16),
	INFRA_GATE(CLK_INFRA_MSDC_HCK_CK, "infra_msdc_hck", "emmc_250m_sel",
	    INFRA_CG1, 17),
	INFRA_GATE(CLK_INFRA_MSDC_133M_CK, "infra_msdc_133m", "sysaxi_sel",
	    INFRA_CG1, 18),
	INFRA_GATE(CLK_INFRA_MSDC_66M_CK, "infra_msdc_66m", "infra_sysaxi_d2",
	    INFRA_CG1, 19),
	INFRA_GATE(CLK_INFRA_ADC_26M_CK, "infra_adc_26m", "infra_adc_frc",
	    INFRA_CG1, 20),
	INFRA_GATE(CLK_INFRA_ADC_FRC_CK, "infra_adc_frc", "csw_f26m_sel",
	    INFRA_CG1, 21),
	INFRA_GATE(CLK_INFRA_FBIST2FPC_CK, "infra_fbist2fpc", "nfi1x_sel",
	    INFRA_CG1, 23),

	/* Bank 2 */
	INFRA_GATE(CLK_INFRA_IUSB_133_CK, "infra_iusb_133", "sysaxi_sel",
	    INFRA_CG2, 0),
	INFRA_GATE(CLK_INFRA_IUSB_66M_CK, "infra_iusb_66m", "infra_sysaxi_d2",
	    INFRA_CG2, 1),
	INFRA_GATE(CLK_INFRA_IUSB_SYS_CK, "infra_iusb_sys", "u2u3_sys_sel",
	    INFRA_CG2, 2),
	INFRA_GATE(CLK_INFRA_IUSB_CK, "infra_iusb", "u2u3_sel",
	    INFRA_CG2, 3),
	INFRA_GATE(CLK_INFRA_IPCIE_CK, "infra_ipcie", "pextp_tl_ck_sel",
	    INFRA_CG2, 12),
	INFRA_GATE(CLK_INFRA_IPCIE_PIPE_CK, "infra_ipcie_pipe", "top_xtal",
	    INFRA_CG2, 13),
	INFRA_GATE(CLK_INFRA_IPCIER_CK, "infra_ipcier", "csw_f26m_sel",
	    INFRA_CG2, 14),
	INFRA_GATE(CLK_INFRA_IPCIEB_CK, "infra_ipcieb", "sysaxi_sel",
	    INFRA_CG2, 15),
};

/* Software reset banks: assert at the base, deassert at +4, status at +8. */
static const uint32_t mt7986_infracfg_reset_banks[] = { 0x70, 0x80, 0x90 };

static const struct mtk_clk_reset_desc mt7986_infracfg_resets = {
	.banks = mt7986_infracfg_reset_banks,
	.nbanks = nitems(mt7986_infracfg_reset_banks),
};

static const struct mtk_clk_desc mt7986_infracfg_desc = {
	.clks = mt7986_infracfg_clks,
	.nclks = nitems(mt7986_infracfg_clks),
	.resets = &mt7986_infracfg_resets,
};

static struct ofw_compat_data compat_data[] = {
	{ "mediatek,mt7986-infracfg",	1 },
	{ NULL,				0 }
};

static int
mt7986_infracfg_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "MediaTek MT7986 infrastructure clock controller");
	return (BUS_PROBE_DEFAULT);
}

static int
mt7986_infracfg_attach(device_t dev)
{
	return (mtk_clk_attach(dev, &mt7986_infracfg_desc));
}

static device_method_t mt7986_infracfg_methods[] = {
	DEVMETHOD(device_probe,		mt7986_infracfg_probe),
	DEVMETHOD(device_attach,	mt7986_infracfg_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(mt7986_infracfg, mt7986_infracfg_driver,
    mt7986_infracfg_methods, sizeof(struct mtk_clk_softc), mtk_clk_driver);

/* After TOPCKGEN, which provides the parents of these clocks. */
EARLY_DRIVER_MODULE(mt7986_infracfg, simplebus, mt7986_infracfg_driver,
    NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 3);
