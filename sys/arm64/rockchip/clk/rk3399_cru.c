/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
 * Copyright (c) 2018 Greg V <greg@unrelenting.technology>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm64/rockchip/clk/rk_cru.h>

#include <arm64/rockchip/clk/rk3399_cru_dt.h>

#define	CRU_CLKSEL_CON(x)	(0x100 + (x) * 0x4)
#define	CRU_CLKGATE_CON(x)	(0x300 + (x) * 0x4)

/* GATES */

static struct rk_cru_gate rk3399_gates[] = {
	/* CRU_CLKGATE_CON0 */
	/* 15-8 unused */
	GATE(SCLK_PVTM_CORE_L, "clk_pvtm_core_l", "xin24m",		0, 7),
	GATE(0, "pclk_dbg_core_l", "pclk_dbg_core_l_c",			0, 6),
	GATE(0, "atclk_core_l", "atclk_core_l_c",			0, 5),
	GATE(0, "aclkm_core_l", "aclkm_core_l_c",			0, 4),
	GATE(0, "clk_core_l_gpll_src", "gpll",				0, 3),
	GATE(0, "clk_core_l_dpll_src", "dpll",				0, 2),
	GATE(0, "clk_core_l_bpll_src", "bpll",				0, 1),
	GATE(0, "clk_core_l_lpll_src", "lpll",				0, 0),

	/* CRU_CLKGATE_CON1 */
	/* 15 - 8 unused */
	GATE(SCLK_PVTM_CORE_B, "clk_pvtm_core_b", "xin24m",		1, 7),
	GATE(0, "pclk_dbg_core_b","pclk_dbg_core_b_c",			1, 6),
	GATE(0, "atclk_core_b", "atclk_core_b_c", 			1, 5),
	GATE(0, "aclkm_core_b", "aclkm_core_b_c",			1, 4),
	GATE(0, "clk_core_b_gpll_src", "gpll",				1, 3),
	GATE(0, "clk_core_b_dpll_src", "dpll",				1, 2),
	GATE(0, "clk_core_b_bpll_src", "bpll",				1, 1),
	GATE(0, "clk_core_b_lpll_src", "lpll",				1, 0),

	/* CRU_CLKGATE_CON2 */
	/* 15 - 11 unused */
	GATE(0, "npll_cs", "npll",					2, 10),
	GATE(0, "gpll_cs", "gpll",					2, 9),
	GATE(0, "cpll_cs", "cpll",					2, 8),
	GATE(SCLK_CCI_TRACE, "clk_cci_trace", "clk_cci_trace_c",	2, 7),
	GATE(0, "gpll_cci_trace", "gpll",				2, 6),
	GATE(0, "cpll_cci_trace", "cpll",				2, 5),
	GATE(0, "aclk_cci_pre", "aclk_cci_pre_c",			2, 4),
	GATE(0, "vpll_aclk_cci_src", "vpll",				2, 3),
	GATE(0, "npll_aclk_cci_src", "npll",				2, 2),
	GATE(0, "gpll_aclk_cci_src", "gpll",				2, 1),
	GATE(0, "cpll_aclk_cci_src", "cpll",				2, 0),

	/* CRU_CLKGATE_CON3 */
	/* 15 - 8 unused */
	GATE(0, "aclk_center", "aclk_center_c",				3, 7),
	/* 6 unused */
	/* 5 unused */
	GATE(PCLK_DDR, "pclk_ddr", "pclk_ddr_c",			3, 4),
	GATE(0, "clk_ddrc_gpll_src", "gpll",				3, 3),
	GATE(0, "clk_ddrc_dpll_src", "dpll",				3, 2),
	GATE(0, "clk_ddrc_bpll_src", "bpll",				3, 1),
	GATE(0, "clk_ddrc_lpll_src", "lpll",				3, 0),

	/* CRU_CLKGATE_CON4 */
	/* 15 - 12 unused */
	GATE(SCLK_PVTM_DDR, "clk_pvtm_ddr", "xin24m",			4, 11),
	GATE(0, "clk_rga_core", "clk_rga_core_c",			4, 10),
	GATE(0, "hclk_rga_pre", "hclk_rga_pre_c",			4, 9),
	GATE(0, "aclk_rga_pre", "aclk_rga_pre_c",			4, 8),
	GATE(0, "hclk_iep_pre", "hclk_iep_pre_c",			4, 7),
	GATE(0, "aclk_iep_pre", "aclk_iep_pre_c",			4, 6),
	GATE(SCLK_VDU_CA, "clk_vdu_ca", "clk_vdu_ca_c",			4, 5),
	GATE(SCLK_VDU_CORE, "clk_vdu_core", "clk_vdu_core_c",		4, 4),
	GATE(0, "hclk_vdu_pre", "hclk_vdu_pre_c",			4, 3),
	GATE(0, "aclk_vdu_pre", "aclk_vdu_pre_c",			4, 2),
	GATE(0, "hclk_vcodec_pre", "hclk_vcodec_pre_c",			4, 1),
	GATE(0, "aclk_vcodec_pre", "aclk_vcodec_pre_c",			4, 0),

	/* CRU_CLKGATE_CON5 */
	/* 15 - 10 unused */
	GATE(SCLK_MAC_TX, "clk_rmii_tx", "clk_rmii_src",		5, 9),
	GATE(SCLK_MAC_RX, "clk_rmii_rx", "clk_rmii_src",		5, 8),
	GATE(SCLK_MACREF, "clk_mac_ref", "clk_rmii_src",		5, 7),
	GATE(SCLK_MACREF_OUT, "clk_mac_refout", "clk_rmii_src",		5, 6),
	GATE(SCLK_MAC, "clk_gmac", "clk_gmac_c",			5, 5),
	GATE(PCLK_PERIHP, "pclk_perihp", "pclk_perihp_c",		5, 4),
	GATE(HCLK_PERIHP, "hclk_perihp", "hclk_perihp_c",		5, 3),
	GATE(ACLK_PERIHP, "aclk_perihp", "aclk_perihp_c",		5, 2),
	GATE(0, "cpll_aclk_perihp_src", "cpll",				5, 1),
	GATE(0, "gpll_aclk_perihp_src", "gpll",				5, 0),

	/* CRU_CLKGATE_CON6 */
	/* 15 unused */
	GATE(SCLK_EMMC, "clk_emmc", "clk_emmc_c",			6, 14),
	GATE(0, "cpll_aclk_emmc_src", "cpll",				6, 13),
	GATE(0, "gpll_aclk_emmc_src", "gpll",				6, 12),
	GATE(0, "pclk_gmac_pre", "pclk_gmac_pre_c",			6, 11),
	GATE(0, "aclk_gmac_pre", "aclk_gmac_pre_c",			6, 10),
	GATE(0, "cpll_aclk_gmac_src", "cpll",				6, 9),
	GATE(0, "gpll_aclk_gmac_src", "gpll",				6, 8),
	/* 7 unused */
	GATE(SCLK_USB2PHY1_REF, "clk_usb2phy1_ref", "xin24m",		6, 6),
	GATE(SCLK_USB2PHY0_REF, "clk_usb2phy0_ref", "xin24m", 		6, 5),
	GATE(SCLK_HSICPHY, "clk_hsicphy", "clk_hsicphy_c",		6, 4),
	GATE(0, "clk_pcie_core_cru", "clk_pcie_core_cru_c",		6, 3),
	GATE(SCLK_PCIE_PM, "clk_pcie_pm", "clk_pcie_pm_c",		6, 2),
	GATE(SCLK_SDMMC, "clk_sdmmc", "clk_sdmmc_c",			6, 1),
	GATE(SCLK_SDIO, "clk_sdio", "clk_sdio_c",			6, 0),

	/* CRU_CLKGATE_CON7 */
	/* 15 - 10 unused */
	GATE(FCLK_CM0S, "fclk_cm0s", "fclk_cm0s_c",			7, 9),
	GATE(SCLK_CRYPTO1, "clk_crypto1", "clk_crypto1_c",		7, 8),
	GATE(SCLK_CRYPTO0, "clk_crypto0", "clk_crypto0_c",		7, 7),
	GATE(0, "cpll_fclk_cm0s_src", "cpll",				7, 6),
	GATE(0, "gpll_fclk_cm0s_src", "gpll",				7, 5),
	GATE(PCLK_PERILP0, "pclk_perilp0", "pclk_perilp0_c",		7, 4),
	GATE(HCLK_PERILP0, "hclk_perilp0", "hclk_perilp0_c",		7, 3),
	GATE(ACLK_PERILP0, "aclk_perilp0", "aclk_perilp0_c",		7, 2),
	GATE(0, "cpll_aclk_perilp0_src", "cpll",			7, 1),
	GATE(0, "gpll_aclk_perilp0_src", "gpll",			7, 0),

	/* CRU_CLKGATE_CON8 */
	GATE(SCLK_SPDIF_8CH, "clk_spdif", "clk_spdif_mux",		8, 15),
	GATE(0, "clk_spdif_frac", "clk_spdif_frac_c",			8, 14),
	GATE(0, "clk_spdif_div", "clk_spdif_div_c",			8, 13),
	GATE(SCLK_I2S_8CH_OUT, "clk_i2sout", "clk_i2sout_c",		8, 12),
	GATE(SCLK_I2S2_8CH, "clk_i2s2", "clk_i2s2_mux",			8, 11),
	GATE(0, "clk_i2s2_frac", "clk_i2s2_frac_c",			8, 10),
	GATE(0, "clk_i2s2_div", "clk_i2s2_div_c",			8, 9),
	GATE(SCLK_I2S1_8CH, "clk_i2s1", "clk_i2s1_mux",			8, 8),
	GATE(0, "clk_i2s1_frac", "clk_i2s1_frac_c",			8, 7),
	GATE(0, "clk_i2s1_div", "clk_i2s1_div_c",			8, 6),
	GATE(SCLK_I2S0_8CH, "clk_i2s0", "clk_i2s0_mux",			8, 5),
	GATE(0, "clk_i2s0_frac","clk_i2s0_frac_c",			8, 4),
	GATE(0, "clk_i2s0_div","clk_i2s0_div_c",			8, 3),
	GATE(PCLK_PERILP1, "pclk_perilp1", "pclk_perilp1_c",		8, 2),
	GATE(HCLK_PERILP1, "cpll_hclk_perilp1_src", "cpll",		8, 1),
	GATE(0, "gpll_hclk_perilp1_src", "gpll",			8, 0),

	/* CRU_CLKGATE_CON9 */
	GATE(SCLK_SPI4, "clk_spi4", "clk_spi4_c", 			9, 15),
	GATE(SCLK_SPI2, "clk_spi2", "clk_spi2_c",			9, 14),
	GATE(SCLK_SPI1, "clk_spi1", "clk_spi1_c",			9, 13),
	GATE(SCLK_SPI0, "clk_spi0", "clk_spi0_c",			9, 12),
	GATE(SCLK_SARADC, "clk_saradc", "clk_saradc_c",			9, 11),
	GATE(SCLK_TSADC, "clk_tsadc", "clk_tsadc_c",			9, 10),
	/* 9 - 8 unused */
	GATE(0, "clk_uart3_frac", "clk_uart3_frac_c",			9, 7),
	GATE(0, "clk_uart3_div", "clk_uart3_div_c",			9, 6),
	GATE(0, "clk_uart2_frac", "clk_uart2_frac_c",			9, 5),
	GATE(0, "clk_uart2_div", "clk_uart2_div_c",			9, 4),
	GATE(0, "clk_uart1_frac", "clk_uart1_frac_c",			9, 3),
	GATE(0, "clk_uart1_div", "clk_uart1_div_c",			9, 2),
	GATE(0, "clk_uart0_frac", "clk_uart0_frac_c", 			9, 1),
	GATE(0, "clk_uart0_div", "clk_uart0_div_c",			9, 0),

	/* CRU_CLKGATE_CON10 */
	GATE(SCLK_VOP1_PWM, "clk_vop1_pwm", "clk_vop1_pwm_c",		10, 15),
	GATE(SCLK_VOP0_PWM, "clk_vop0_pwm", "clk_vop0_pwm_c",		10, 14),
	GATE(DCLK_VOP0_DIV, "dclk_vop0_div", "dclk_vop0_div_c",		10, 12),
	GATE(DCLK_VOP1_DIV, "dclk_vop1_div", "dclk_vop1_div_c",		10, 13),
	GATE(0, "hclk_vop1_pre", "hclk_vop1_pre_c",			10, 11),
	GATE(ACLK_VOP1_PRE, "aclk_vop1_pre", "aclk_vop1_pre_c",		10, 10),
	GATE(0, "hclk_vop0_pre", "hclk_vop0_pre_c",			10, 9),
	GATE(ACLK_VOP0_PRE, "aclk_vop0_pre", "aclk_vop0_pre_c",		10, 8),
	GATE(0, "clk_cifout_src", "clk_cifout_src_c",			10, 7),
	GATE(SCLK_SPDIF_REC_DPTX, "clk_spdif_rec_dptx", "clk_spdif_rec_dptx_c", 10, 6),
	GATE(SCLK_I2C7, "clk_i2c7", "clk_i2c7_c",			10, 5),
	GATE(SCLK_I2C3, "clk_i2c3", "clk_i2c3_c",			10, 4),
	GATE(SCLK_I2C6, "clk_i2c6", "clk_i2c6_c",			10, 3),
	GATE(SCLK_I2C2, "clk_i2c2", "clk_i2c2_c",			10, 2),
	GATE(SCLK_I2C5, "clk_i2c5", "clk_i2c5_c",			10, 1),
	GATE(SCLK_I2C1, "clk_i2c1", "clk_i2c1_c",			10, 0),

	/* CRU_CLKGATE_CON11 */
	GATE(SCLK_MIPIDPHY_CFG, "clk_mipidphy_cfg", "xin24m",		11, 15),
	GATE(SCLK_MIPIDPHY_REF, "clk_mipidphy_ref", "xin24m",		11, 14),
	/* 13-12 unused */
	GATE(PCLK_EDP, "pclk_edp", "pclk_edp_c",			11, 11),
	GATE(PCLK_HDCP, "pclk_hdcp", "pclk_hdcp_c",			11, 10),
	/* 9 unuwsed */
	GATE(SCLK_DP_CORE, "clk_dp_core", "clk_dp_core_c",		11, 8),
	GATE(SCLK_HDMI_CEC, "clk_hdmi_cec", "clk_hdmi_cec_c",		11, 7),
	GATE(SCLK_HDMI_SFR, "clk_hdmi_sfr", "xin24m",			11, 6),
	GATE(SCLK_ISP1, "clk_isp1", "clk_isp1_c",			11, 5),
	GATE(SCLK_ISP0, "clk_isp0",  "clk_isp0_c",			11, 4),
	GATE(HCLK_HDCP, "hclk_hdcp",  "hclk_hdcp_c",			11, 3),
	GATE(ACLK_HDCP, "aclk_hdcp", "aclk_hdcp_c",			11, 2),
	GATE(PCLK_VIO, "pclk_vio", "pclk_vio_c",			11, 1),
	GATE(ACLK_VIO, "aclk_vio", "aclk_vio_c",			11, 0),

	/* CRU_CLKGATE_CON12 */
	/* 15 - 14 unused */
	GATE(HCLK_SD, "hclk_sd", "hclk_sd_c",				12, 13),
	GATE(ACLK_GIC_PRE, "aclk_gic_pre",  "aclk_gic_pre_c",		12, 12),
	GATE(HCLK_ISP1, "hclk_isp1", "hclk_isp1_c",			12, 11),
	GATE(ACLK_ISP1, "aclk_isp1", "aclk_isp1_c",			12, 10),
	GATE(HCLK_ISP0, "hclk_isp0", "hclk_isp0_c",			12, 9),
	GATE(ACLK_ISP0, "aclk_isp0",  "aclk_isp0_c",			12, 8),
	/* 7 unused */
	GATE(SCLK_PCIEPHY_REF100M, "clk_pciephy_ref100m", "clk_pciephy_ref100m_c", 12, 6),
	/* 5 unused */
	GATE(SCLK_USB3OTG1_SUSPEND, "clk_usb3otg1_suspend", "clk_usb3otg1_suspend_c", 12, 4),
	GATE(SCLK_USB3OTG0_SUSPEND, "clk_usb3otg0_suspend", "clk_usb3otg0_suspend_c", 12, 3),
	GATE(SCLK_USB3OTG1_REF, "clk_usb3otg1_ref", "xin24m",		12, 2),
	GATE(SCLK_USB3OTG0_REF, "clk_usb3otg0_ref", "xin24m",		12, 1),
	GATE(ACLK_USB3, "aclk_usb3",  "aclk_usb3_c", 			12, 0),

	/* CRU_CLKGATE_CON13 */
	GATE(SCLK_TESTCLKOUT2, "clk_testout2", "clk_testout2_c",	13, 15),
	GATE(SCLK_TESTCLKOUT1, "clk_testout1",  "clk_testout1_c",	13, 14),
	GATE(SCLK_SPI5, "clk_spi5",  "clk_spi5_c",			13, 13),
	GATE(0, "clk_usbphy0_480m_src", "clk_usbphy0_480m",		13, 12),
	GATE(0, "clk_usbphy1_480m_src", "clk_usbphy1_480m",		13, 12),
	GATE(0, "clk_test", "clk_test_c",				13, 11),
	/* 10 unused */
	GATE(0, "clk_test_frac", "clk_test_frac_c",			13, 9),
	/* 8 unused */
	GATE(SCLK_UPHY1_TCPDCORE, "clk_uphy1_tcpdcore", "clk_uphy1_tcpdcore_c", 13, 7),
	GATE(SCLK_UPHY1_TCPDPHY_REF, "clk_uphy1_tcpdphy_ref", "clk_uphy1_tcpdphy_ref_c", 13, 6),
	GATE(SCLK_UPHY0_TCPDCORE, "clk_uphy0_tcpdcore", "clk_uphy0_tcpdcore_c", 13, 5),
	GATE(SCLK_UPHY0_TCPDPHY_REF, "clk_uphy0_tcpdphy_ref", "clk_uphy0_tcpdphy_ref_c", 13, 4),
	/* 3 - 2 unused */
	GATE(SCLK_PVTM_GPU, "aclk_pvtm_gpu", "xin24m", 			13, 1),
	GATE(0, "aclk_gpu_pre", "aclk_gpu_pre_c",			13, 0),

	/* CRU_CLKGATE_CON14 */
	/* 15 - 14 unused */
	GATE(ACLK_PERF_CORE_L, "aclk_perf_core_l", "aclkm_core_l",	14, 13),
	GATE(ACLK_CORE_ADB400_CORE_L_2_CCI500, "aclk_core_adb400_core_l_2_cci500", "aclkm_core_l", 14, 12),
	GATE(ACLK_GIC_ADB400_CORE_L_2_GIC, "aclk_core_adb400_core_l_2_gic", "armclkl", 14, 11),
	GATE(ACLK_GIC_ADB400_GIC_2_CORE_L, "aclk_core_adb400_gic_2_core_l", "armclkl", 14, 10),
	GATE(0, "clk_dbg_pd_core_l", "armclkl",				14, 9),
	/* 8 - 7 unused */
	GATE(ACLK_PERF_CORE_B, "aclk_perf_core_b", "aclkm_core_b", 	14, 6),
	GATE(ACLK_CORE_ADB400_CORE_B_2_CCI500, "aclk_core_adb400_core_b_2_cci500", "aclkm_core_b", 14, 5),
	GATE(ACLK_GIC_ADB400_CORE_B_2_GIC, "aclk_core_adb400_core_b_2_gic", "armclkb", 14, 4),
	GATE(ACLK_GIC_ADB400_GIC_2_CORE_B, "aclk_core_adb400_gic_2_core_b", "armclkb", 14, 3),
	GATE(0, "pclk_dbg_cxcs_pd_core_b", "pclk_dbg_core_b",		14, 2),
	GATE(0, "clk_dbg_pd_core_b", "armclkb", 			14, 1),
	/* 0 unused */

	/* CRU_CLKGATE_CON15 */
	/* 15 - 8 unused */
	GATE(ACLK_CCI_GRF, "aclk_cci_grf", "aclk_cci_pre",		15, 7),
	GATE(0, "clk_dbg_noc", "clk_cs",				15, 6),
	GATE(0, "clk_dbg_cxcs", "clk_cs",				15, 5),
	GATE(ACLK_CCI_NOC1, "aclk_cci_noc1", "aclk_cci_pre",		15, 4),
	GATE(ACLK_CCI_NOC0, "aclk_cci_noc0", "aclk_cci_pre",		15, 3),
	GATE(ACLK_CCI, "aclk_cci", "aclk_cci_pre",			15, 2),
	GATE(ACLK_ADB400M_PD_CORE_B, "aclk_adb400m_pd_core_b", "aclk_cci_pre", 15, 1),
	GATE(ACLK_ADB400M_PD_CORE_L, "aclk_adb400m_pd_core_l", "aclk_cci_pre", 15, 0),

	/* CRU_CLKGATE_CON16 */
	/* 15 - 12 unused */
	GATE(HCLK_RGA_NOC, "hclk_rga_noc", "hclk_rga_pre",		16, 11),
	GATE(HCLK_RGA, "hclk_rga", "hclk_rga_pre", 			16, 10),
	GATE(ACLK_RGA_NOC, "aclk_rga_noc", "aclk_rga_pre",		16, 9),
	GATE(ACLK_RGA, "aclk_rga", "aclk_rga_pre", 			16, 8),
	/* 7 - 4 unused */
	GATE(HCLK_IEP_NOC, "hclk_iep_noc", "hclk_iep_pre",		16, 3),
	GATE(HCLK_IEP, "hclk_iep", "hclk_iep_pre", 			16, 2),
	GATE(ACLK_IEP_NOC, "aclk_iep_noc", "aclk_iep_pre",		16, 1),
	GATE(ACLK_IEP, "aclk_iep", "aclk_iep_pre", 			16, 0),

	/* CRU_CLKGATE_CON17 */
	/* 15 - 12 unused */
	GATE(HCLK_VDU_NOC, "hclk_vdu_noc", "hclk_vdu_pre",		17, 11),
	GATE(HCLK_VDU, "hclk_vdu", "hclk_vdu_pre",			17, 10),
	GATE(ACLK_VDU_NOC, "aclk_vdu_noc", "aclk_vdu_pre",		17, 9),
	GATE(ACLK_VDU, "aclk_vdu", "aclk_vdu_pre", 			17, 8),
	GATE(0, "hclk_vcodec_noc", "hclk_vcodec_pre",			17, 3),
	GATE(HCLK_VCODEC, "hclk_vcodec", "hclk_vcodec_pre",		17, 2),
	GATE(0, "aclk_vcodec_noc", "aclk_vcodec_pre",			17, 1),
	GATE(ACLK_VCODEC, "aclk_vcodec", "aclk_vcodec_pre",		17, 0),

	/* CRU_CLKGATE_CON18 */
	GATE(PCLK_CIC, "pclk_cic", "pclk_ddr",				18, 15),
	GATE(0, "clk_ddr_mon_timer", "xin24m",				18, 14),
	GATE(0, "clk_ddr_mon", "clk_ddrc_div2",				18, 13),
	GATE(PCLK_DDR_MON, "pclk_ddr_mon", "pclk_ddr", 			18, 12),
	GATE(0, "clk_ddr_cic", "clk_ddrc_div2",				18, 11),
	GATE(PCLK_CENTER_MAIN_NOC, "pclk_center_main_noc", "pclk_ddr",	18, 10),
	GATE(0, "clk_ddrcfg_msch1", "clk_ddrc_div2",			18,  9),
	GATE(0, "clk_ddrphy1", "clk_ddrc_div2",				18,  8),
	GATE(0, "clk_ddrphy_ctrl1", "clk_ddrc_div2",			18,  7),
	GATE(0, "clk_ddrc1", "clk_ddrc_div2",				18,  6),
	GATE(0, "clk_ddr1_msch", "clk_ddrc_div2",			18,  5),
	GATE(0, "clk_ddrcfg_msch0", "clk_ddrc_div2",			18,  4),
	GATE(0, "clk_ddrphy0", "clk_ddrc_div2",				18,  3),
	GATE(0, "clk_ddrphy_ctrl0", "clk_ddrc_div2",			18,  2),
	GATE(0, "clk_ddrc0", "clk_ddrc_div2",				18,  1),

	/* CRU_CLKGATE_CON19 */
	/* 15 - 3 unused */
	GATE(PCLK_DDR_SGRF, "pclk_ddr_sgrf", "pclk_ddr",		19, 2),
	GATE(ACLK_CENTER_PERI_NOC, "aclk_center_peri_noc", "aclk_center", 19, 1),
	GATE(ACLK_CENTER_MAIN_NOC, "aclk_center_main_noc", "aclk_center", 19, 0),

	/* CRU_CLKGATE_CON20 */
	GATE(0, "hclk_ahb1tom", "hclk_perihp",				20, 15),
	GATE(0, "pclk_perihp_noc", "pclk_perihp",			20, 14),
	GATE(0, "hclk_perihp_noc", "hclk_perihp",			20, 13),
	GATE(0, "aclk_perihp_noc", "aclk_perihp",			20, 12),
	GATE(PCLK_PCIE, "pclk_pcie", "pclk_perihp",			20, 11),
	GATE(ACLK_PCIE, "aclk_pcie", "aclk_perihp",			20, 10),
	GATE(HCLK_HSIC, "hclk_hsic", "hclk_perihp",			20, 9),
	GATE(HCLK_HOST1_ARB, "hclk_host1_arb", "hclk_perihp",		20, 8),
	GATE(HCLK_HOST1, "hclk_host1", "hclk_perihp",			20, 7),
	GATE(HCLK_HOST0_ARB, "hclk_host0_arb", "hclk_perihp",		20, 6),
	GATE(HCLK_HOST0, "hclk_host0", "hclk_perihp",			20, 5),
	GATE(PCLK_PERIHP_GRF, "pclk_perihp_grf", "pclk_perihp",		20, 4),
	GATE(ACLK_PERF_PCIE, "aclk_perf_pcie", "aclk_perihp",		20, 2),
	/* 1 - 0 unused */

	/* CRU_CLKGATE_CON21 */
	/* 15 - 10 unused */
	GATE(PCLK_UPHY1_TCPD_G, "pclk_uphy1_tcpd_g", "pclk_alive",	21, 9),
	GATE(PCLK_UPHY1_TCPHY_G, "pclk_uphy1_tcphy_g", "pclk_alive",	21, 8),
	/* 7 unused */
	GATE(PCLK_UPHY0_TCPD_G, "pclk_uphy0_tcpd_g", "pclk_alive",	21, 6),
	GATE(PCLK_UPHY0_TCPHY_G, "pclk_uphy0_tcphy_g", "pclk_alive",	21, 5),
	GATE(PCLK_USBPHY_MUX_G, "pclk_usbphy_mux_g", "pclk_alive",	21, 4),
	GATE(SCLK_DPHY_RX0_CFG, "clk_dphy_rx0_cfg", "clk_mipidphy_cfg",	21, 3),
	GATE(SCLK_DPHY_TX1RX1_CFG, "clk_dphy_tx1rx1_cfg", "clk_mipidphy_cfg", 21, 2),
	GATE(SCLK_DPHY_TX0_CFG, "clk_dphy_tx0_cfg", "clk_mipidphy_cfg",	21, 1),
	GATE(SCLK_DPHY_PLL, "clk_dphy_pll", "clk_mipidphy_ref",		21, 0),

	/* CRU_CLKGATE_CON22 */
	GATE(PCLK_EFUSE1024S, "pclk_efuse1024s", "pclk_perilp1",	22, 15),
	GATE(PCLK_EFUSE1024NS, "pclk_efuse1024ns", "pclk_perilp1",	22, 14),
	GATE(PCLK_TSADC, "pclk_tsadc", "pclk_perilp1",			22, 13),
	GATE(PCLK_SARADC, "pclk_saradc", "pclk_perilp1",		22, 12),
	GATE(PCLK_MAILBOX0, "pclk_mailbox0", "pclk_perilp1",		22, 11),
	GATE(PCLK_I2C3, "pclk_i2c3", "pclk_perilp1",			22, 10),
	GATE(PCLK_I2C2, "pclk_i2c2", "pclk_perilp1",			22, 9),
	GATE(PCLK_I2C6, "pclk_i2c6", "pclk_perilp1",			22, 8),
	GATE(PCLK_I2C5, "pclk_i2c5", "pclk_perilp1",			22, 7),
	GATE(PCLK_I2C1, "pclk_i2c1", "pclk_perilp1",			22, 6),
	GATE(PCLK_I2C7, "pclk_i2c7", "pclk_perilp1",			22, 5),
	GATE(PCLK_UART3, "pclk_uart3", "pclk_perilp1",			22, 3),
	GATE(PCLK_UART2, "pclk_uart2", "pclk_perilp1",			22, 2),
	GATE(PCLK_UART1, "pclk_uart1", "pclk_perilp1",			22, 1),
	GATE(PCLK_UART0, "pclk_uart0", "pclk_perilp1",			22, 0),

	/* CRU_CLKGATE_CON23 */
	/* 15 - 14 unused */
	GATE(PCLK_SPI4, "pclk_spi4", "pclk_perilp1",			23, 13),
	GATE(PCLK_SPI2, "pclk_spi2", "pclk_perilp1",			23, 12),
	GATE(PCLK_SPI1, "pclk_spi1", "pclk_perilp1",			23, 11),
	GATE(PCLK_SPI0, "pclk_spi0", "pclk_perilp1",			23, 10),
	GATE(PCLK_DCF, "pclk_dcf", "pclk_perilp0",			23, 9),
	GATE(ACLK_DCF, "aclk_dcf", "aclk_perilp0",			23, 8),
	GATE(SCLK_INTMEM5, "clk_intmem5", "aclk_perilp0",		23, 7),
	GATE(SCLK_INTMEM4, "clk_intmem4", "aclk_perilp0",		23, 6),
	GATE(SCLK_INTMEM3, "clk_intmem3", "aclk_perilp0",		23, 5),
	GATE(SCLK_INTMEM2, "clk_intmem2", "aclk_perilp0",		23, 4),
	GATE(SCLK_INTMEM1, "clk_intmem1", "aclk_perilp0",		23, 3),
	GATE(SCLK_INTMEM0, "clk_intmem0", "aclk_perilp0",		23, 2),
	GATE(ACLK_TZMA, "aclk_tzma", "aclk_perilp0",			23, 1),
	GATE(ACLK_INTMEM, "aclk_intmem", "aclk_perilp0",		23, 0),

	/* CRU_CLKGATE_CON24 */
	GATE(HCLK_S_CRYPTO1, "hclk_s_crypto1", "hclk_perilp0",		24, 15),
	GATE(HCLK_M_CRYPTO1, "hclk_m_crypto1", "hclk_perilp0",		24, 14),
	GATE(PCLK_PERIHP_GRF, "pclk_perilp_sgrf", "pclk_perilp1",	24, 13),
	GATE(SCLK_M0_PERILP_DEC, "clk_m0_perilp_dec", "fclk_cm0s",	24, 11),
	GATE(DCLK_M0_PERILP, "dclk_m0_perilp", "fclk_cm0s", 		24, 10),
	GATE(HCLK_M0_PERILP, "hclk_m0_perilp", "fclk_cm0s", 		24, 9),
	GATE(SCLK_M0_PERILP, "sclk_m0_perilp", "fclk_cm0s",		24, 8),
	/* 7 - unused */
	GATE(HCLK_S_CRYPTO0, "hclk_s_crypto0", "hclk_perilp0",		24, 6),
	GATE(HCLK_M_CRYPTO0, "hclk_m_crypto0", "hclk_perilp0",		24, 5),
	GATE(HCLK_ROM, "hclk_rom", "hclk_perilp0",			24, 4),
	/* 3 - 0 unused */

	/* CRU_CLKGATE_CON25 */
	/* 15 - 13 unused */
	GATE(0, "hclk_sdio_noc", "hclk_perilp1",			25, 12),
	GATE(HCLK_M0_PERILP_NOC, "hclk_m0_perilp_noc", "fclk_cm0s",	25, 11),
	GATE(0, "pclk_perilp1_noc", "pclk_perilp1",			25, 10),
	GATE(0, "hclk_perilp1_noc", "hclk_perilp1",			25, 9),
	GATE(HCLK_PERILP0_NOC, "hclk_perilp0_noc", "hclk_perilp0",	25, 8),
	GATE(ACLK_PERILP0_NOC, "aclk_perilp0_noc", "aclk_perilp0",	25, 7),
	GATE(ACLK_DMAC1_PERILP, "aclk_dmac1_perilp", "aclk_perilp0",	25, 6),
	GATE(ACLK_DMAC0_PERILP, "aclk_dmac0_perilp", "aclk_perilp0",	25, 5),
	/* 4 - 0 unused */

	/* CRU_CLKGATE_CON26 */
	/* 15 - 12 unused */
	GATE(SCLK_TIMER11, "clk_timer11", "xin24m",			26, 11),
	GATE(SCLK_TIMER10, "clk_timer10", "xin24m",			26, 10),
	GATE(SCLK_TIMER09, "clk_timer09", "xin24m",			26, 9),
	GATE(SCLK_TIMER08, "clk_timer08", "xin24m",			26, 8),
	GATE(SCLK_TIMER07, "clk_timer07", "xin24m",			26, 7),
	GATE(SCLK_TIMER06, "clk_timer06", "xin24m",			26, 6),
	GATE(SCLK_TIMER05, "clk_timer05", "xin24m",			26, 5),
	GATE(SCLK_TIMER04, "clk_timer04", "xin24m",			26, 4),
	GATE(SCLK_TIMER03, "clk_timer03", "xin24m",			26, 3),
	GATE(SCLK_TIMER02, "clk_timer02", "xin24m",			26, 2),
	GATE(SCLK_TIMER01, "clk_timer01", "xin24m",			26, 1),
	GATE(SCLK_TIMER00, "clk_timer00", "xin24m",			26, 0),

	/* CRU_CLKGATE_CON27 */
	/* 15 - 9 unused */
	GATE(ACLK_ISP1_WRAPPER, "aclk_isp1_wrapper", "hclk_isp1", 	27, 8),
	GATE(HCLK_ISP1_WRAPPER, "hclk_isp1_wrapper", "aclk_isp0", 	27, 7),
	GATE(PCLK_ISP1_WRAPPER, "pclkin_isp1_wrapper", "pclkin_cif",	27, 6),
	GATE(ACLK_ISP0_WRAPPER, "aclk_isp0_wrapper", "aclk_isp0", 	27, 5),
	GATE(HCLK_ISP0_WRAPPER, "hclk_isp0_wrapper", "hclk_isp0", 	27, 4),
	GATE(ACLK_ISP1_NOC, "aclk_isp1_noc", "aclk_isp1",		27, 3),
	GATE(HCLK_ISP1_NOC, "hclk_isp1_noc", "hclk_isp1",		27, 2),
	GATE(ACLK_ISP0_NOC, "aclk_isp0_noc", "aclk_isp0",		27, 1),
	GATE(HCLK_ISP0_NOC, "hclk_isp0_noc", "hclk_isp0",		27, 0),

	/* CRU_CLKGATE_CON28 */
	/* 15 - 8 unused */
	GATE(ACLK_VOP1, "aclk_vop1", "aclk_vop1_pre",			28, 7),
	GATE(HCLK_VOP1, "hclk_vop1", "hclk_vop1_pre",			28, 6),
	GATE(ACLK_VOP1_NOC, "aclk_vop1_noc", "aclk_vop1_pre",		28, 5),
	GATE(HCLK_VOP1_NOC, "hclk_vop1_noc", "hclk_vop1_pre",		28, 4),
	GATE(ACLK_VOP0, "aclk_vop0", "aclk_vop0_pre",			28, 3),
	GATE(HCLK_VOP0, "hclk_vop0", "hclk_vop0_pre",			28, 2),
	GATE(ACLK_VOP0_NOC, "aclk_vop0_noc", "aclk_vop0_pre",		28, 1),
	GATE(HCLK_VOP0_NOC, "hclk_vop0_noc", "hclk_vop0_pre",		28, 0),

	/* CRU_CLKGATE_CON29 */
	/* 15 - 13 unused */
	GATE(PCLK_VIO_GRF, "pclk_vio_grf", "pclk_vio",			29, 12),
	GATE(PCLK_GASKET, "pclk_gasket", "pclk_hdcp",			29, 11),
	GATE(ACLK_HDCP22, "aclk_hdcp22", "aclk_hdcp",			29, 10),
	GATE(HCLK_HDCP22, "hclk_hdcp22", "hclk_hdcp",			29, 9),
	GATE(PCLK_HDCP22, "pclk_hdcp22", "pclk_hdcp",			29, 8),
	GATE(PCLK_DP_CTRL, "pclk_dp_ctrl", "pclk_hdcp",			29, 7),
	GATE(PCLK_HDMI_CTRL, "pclk_hdmi_ctrl", "pclk_hdcp",		29, 6),
	GATE(HCLK_HDCP_NOC, "hclk_hdcp_noc", "hclk_hdcp",		29, 5),
	GATE(ACLK_HDCP_NOC, "aclk_hdcp_noc", "aclk_hdcp",		29, 4),
	GATE(PCLK_HDCP_NOC, "pclk_hdcp_noc", "pclk_hdcp",		29, 3),
	GATE(PCLK_MIPI_DSI1, "pclk_mipi_dsi1", "pclk_vio", 		29, 2),
	GATE(PCLK_MIPI_DSI0, "pclk_mipi_dsi0", "pclk_vio", 		29, 1),
	GATE(ACLK_VIO_NOC, "aclk_vio_noc", "aclk_vio",			29, 0),

	/* CRU_CLKGATE_CON30 */
	/* 15 - 12 unused */
	GATE(ACLK_GPU_GRF, "aclk_gpu_grf", "aclk_gpu_pre", 		30, 11),
	GATE(ACLK_PERF_GPU, "aclk_perf_gpu", "aclk_gpu_pre",		30, 10),
	/* 9 unused */
	GATE(ACLK_GPU, "aclk_gpu", "aclk_gpu_pre",			30, 8),
	/* 7 - 5 unused */
	GATE(ACLK_USB3_GRF, "aclk_usb3_grf", "aclk_usb3",		30, 4),
	GATE(ACLK_USB3_RKSOC_AXI_PERF, "aclk_usb3_rksoc_axi_perf", "aclk_usb3", 30, 3),
	GATE(ACLK_USB3OTG1, "aclk_usb3otg1", "aclk_usb3",		30, 2),
	GATE(ACLK_USB3OTG0, "aclk_usb3otg0", "aclk_usb3",		30, 1),
	GATE(ACLK_USB3_NOC, "aclk_usb3_noc", "aclk_usb3",		30, 0),

	/* CRU_CLKGATE_CON31 */
	/* 15 - 11 unused */
	GATE(PCLK_SGRF, "pclk_sgrf", "pclk_alive",			31, 10),
	GATE(PCLK_PMU_INTR_ARB, "pclk_pmu_intr_arb", "pclk_alive",	31, 9),
	GATE(PCLK_HSICPHY, "pclk_hsicphy", "pclk_perihp",		31, 8),
	GATE(PCLK_TIMER1, "pclk_timer1", "pclk_alive",			31, 7),
	GATE(PCLK_TIMER0, "pclk_timer0", "pclk_alive",			31, 6),
	GATE(PCLK_GPIO4, "pclk_gpio4", "pclk_alive",			31, 5),
	GATE(PCLK_GPIO3, "pclk_gpio3", "pclk_alive",			31, 4),
	GATE(PCLK_GPIO2, "pclk_gpio2", "pclk_alive",			31, 3),
	GATE(PCLK_INTR_ARB, "pclk_intr_arb", "pclk_alive",		31, 2),
	GATE(PCLK_GRF, "pclk_grf", "pclk_alive",			31, 1),
	/* 0 unused */

	/* CRU_CLKGATE_CON32 */
	/* 15 - 14 unused */
	GATE(PCLK_EDP_CTRL, "pclk_edp_ctrl", "pclk_edp",		32, 13),
	GATE(PCLK_EDP_NOC, "pclk_edp_noc", "pclk_edp",			32, 12),
	/* 11 unused */
	GATE(ACLK_EMMC_GRF, "aclk_emmcgrf", "aclk_emmc",		32, 10),
	GATE(ACLK_EMMC_NOC, "aclk_emmc_noc", "aclk_emmc",		32, 9),
	GATE(ACLK_EMMC_CORE, "aclk_emmccore", "aclk_emmc",		32, 8),
	/* 7 - 5 unused */
	GATE(ACLK_PERF_GMAC, "aclk_perf_gmac", "aclk_gmac_pre",		32, 4),
	GATE(PCLK_GMAC_NOC, "pclk_gmac_noc", "pclk_gmac_pre", 		32, 3),
	GATE(PCLK_GMAC, "pclk_gmac", "pclk_gmac_pre",			32, 2),
	GATE(ACLK_GMAC_NOC, "aclk_gmac_noc", "aclk_gmac_pre",		32, 1),
	GATE(ACLK_GMAC, "aclk_gmac", "aclk_gmac_pre",			32, 0),

	/* CRU_CLKGATE_CON33 */
	/* 15 - 10 unused */
	GATE(0, "hclk_sdmmc_noc", "hclk_sd",				33, 9),
	GATE(HCLK_SDMMC, "hclk_sdmmc", "hclk_sd", 			33, 8),
	GATE(ACLK_GIC_ADB400_GIC_2_CORE_B, "aclk_gic_adb400_gic_2_core_b", "aclk_gic_pre", 33, 5),
	GATE(ACLK_GIC_ADB400_GIC_2_CORE_L, "aclk_gic_adb400_gic_2_core_l", "aclk_gic_pre", 33, 4),
	GATE(ACLK_GIC_ADB400_CORE_B_2_GIC, "aclk_gic_adb400_core_b_2_gic", "aclk_gic_pre", 33, 3),
	GATE(ACLK_GIC_ADB400_CORE_L_2_GIC, "aclk_gic_adb400_core_l_2_gic", "aclk_gic_pre", 33, 2),
	GATE(ACLK_GIC_NOC, "aclk_gic_noc", "aclk_gic_pre",		33, 1),
	GATE(ACLK_GIC, "aclk_gic", "aclk_gic_pre",			33, 0),

	/* CRU_CLKGATE_CON34 */
	/* 15 - 7 unused */
	GATE(0, "hclk_sdioaudio_noc", "hclk_perilp1",			34, 6),
	GATE(PCLK_SPI5, "pclk_spi5", "hclk_perilp1",			34, 5),
	GATE(HCLK_SDIO, "hclk_sdio", "hclk_perilp1",			34, 4),
	GATE(HCLK_SPDIF, "hclk_spdif", "hclk_perilp1",			34, 3),
	GATE(HCLK_I2S2_8CH, "hclk_i2s2", "hclk_perilp1",		34, 2),
	GATE(HCLK_I2S1_8CH, "hclk_i2s1", "hclk_perilp1",		34, 1),
	GATE(HCLK_I2S0_8CH, "hclk_i2s0", "hclk_perilp1",		34, 0),
};

#define PLL_RATE(_hz, _ref, _fb, _post1, _post2, _dspd)			\
{									\
	.freq = _hz,							\
	.refdiv = _ref,							\
	.fbdiv = _fb,							\
	.postdiv1 = _post1,						\
	.postdiv2 = _post2,						\
	.dsmpd = _dspd,							\
}

static struct rk_clk_pll_rate rk3399_pll_rates[] = {
	/* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
	PLL_RATE(2208000000,  1,  92, 1, 1, 1),
	PLL_RATE(2184000000,  1,  91, 1, 1, 1),
	PLL_RATE(2160000000,  1,  90, 1, 1, 1),
	PLL_RATE(2136000000,  1,  89, 1, 1, 1),
	PLL_RATE(2112000000,  1,  88, 1, 1, 1),
	PLL_RATE(2088000000,  1,  87, 1, 1, 1),
	PLL_RATE(2064000000,  1,  86, 1, 1, 1),
	PLL_RATE(2040000000,  1,  85, 1, 1, 1),
	PLL_RATE(2016000000,  1,  84, 1, 1, 1),
	PLL_RATE(1992000000,  1,  83, 1, 1, 1),
	PLL_RATE(1968000000,  1,  82, 1, 1, 1),
	PLL_RATE(1944000000,  1,  81, 1, 1, 1),
	PLL_RATE(1920000000,  1,  80, 1, 1, 1),
	PLL_RATE(1896000000,  1,  79, 1, 1, 1),
	PLL_RATE(1872000000,  1,  78, 1, 1, 1),
	PLL_RATE(1848000000,  1,  77, 1, 1, 1),
	PLL_RATE(1824000000,  1,  76, 1, 1, 1),
	PLL_RATE(1800000000,  1,  75, 1, 1, 1),
	PLL_RATE(1776000000,  1,  74, 1, 1, 1),
	PLL_RATE(1752000000,  1,  73, 1, 1, 1),
	PLL_RATE(1728000000,  1,  72, 1, 1, 1),
	PLL_RATE(1704000000,  1,  71, 1, 1, 1),
	PLL_RATE(1680000000,  1,  70, 1, 1, 1),
	PLL_RATE(1656000000,  1,  69, 1, 1, 1),
	PLL_RATE(1632000000,  1,  68, 1, 1, 1),
	PLL_RATE(1608000000,  1,  67, 1, 1, 1),
	PLL_RATE(1600000000,  3, 200, 1, 1, 1),
	PLL_RATE(1584000000,  1,  66, 1, 1, 1),
	PLL_RATE(1560000000,  1,  65, 1, 1, 1),
	PLL_RATE(1536000000,  1,  64, 1, 1, 1),
	PLL_RATE(1512000000,  1,  63, 1, 1, 1),
	PLL_RATE(1488000000,  1,  62, 1, 1, 1),
	PLL_RATE(1464000000,  1,  61, 1, 1, 1),
	PLL_RATE(1440000000,  1,  60, 1, 1, 1),
	PLL_RATE(1416000000,  1,  59, 1, 1, 1),
	PLL_RATE(1392000000,  1,  58, 1, 1, 1),
	PLL_RATE(1368000000,  1,  57, 1, 1, 1),
	PLL_RATE(1344000000,  1,  56, 1, 1, 1),
	PLL_RATE(1320000000,  1,  55, 1, 1, 1),
	PLL_RATE(1296000000,  1,  54, 1, 1, 1),
	PLL_RATE(1272000000,  1,  53, 1, 1, 1),
	PLL_RATE(1248000000,  1,  52, 1, 1, 1),
	PLL_RATE(1200000000,  1,  50, 1, 1, 1),
	PLL_RATE(1188000000,  2,  99, 1, 1, 1),
	PLL_RATE(1104000000,  1,  46, 1, 1, 1),
	PLL_RATE(1100000000, 12, 550, 1, 1, 1),
	PLL_RATE(1008000000,  1,  84, 2, 1, 1),
	PLL_RATE(1000000000,  1, 125, 3, 1, 1),
	PLL_RATE( 984000000,  1,  82, 2, 1, 1),
	PLL_RATE( 960000000,  1,  80, 2, 1, 1),
	PLL_RATE( 936000000,  1,  78, 2, 1, 1),
	PLL_RATE( 912000000,  1,  76, 2, 1, 1),
	PLL_RATE( 900000000,  4, 300, 2, 1, 1),
	PLL_RATE( 888000000,  1,  74, 2, 1, 1),
	PLL_RATE( 864000000,  1,  72, 2, 1, 1),
	PLL_RATE( 840000000,  1,  70, 2, 1, 1),
	PLL_RATE( 816000000,  1,  68, 2, 1, 1),
	PLL_RATE( 800000000,  1, 100, 3, 1, 1),
	PLL_RATE( 700000000,  6, 350, 2, 1, 1),
	PLL_RATE( 696000000,  1,  58, 2, 1, 1),
	PLL_RATE( 676000000,  3, 169, 2, 1, 1),
	PLL_RATE( 600000000,  1,  75, 3, 1, 1),
	PLL_RATE( 594000000,  1,  99, 4, 1, 1),
	PLL_RATE( 533250000,  8, 711, 4, 1, 1),
	PLL_RATE( 504000000,  1,  63, 3, 1, 1),
	PLL_RATE( 500000000,  6, 250, 2, 1, 1),
	PLL_RATE( 408000000,  1,  68, 2, 2, 1),
	PLL_RATE( 312000000,  1,  52, 2, 2, 1),
	PLL_RATE( 297000000,  1,  99, 4, 2, 1),
	PLL_RATE( 216000000,  1,  72, 4, 2, 1),
	PLL_RATE( 148500000,  1,  99, 4, 4, 1),
	PLL_RATE( 106500000,  1,  71, 4, 4, 1),
	PLL_RATE(  96000000,  1,  64, 4, 4, 1),
	PLL_RATE(  74250000,  2,  99, 4, 4, 1),
	PLL_RATE(  65000000,  1,  65, 6, 4, 1),
	PLL_RATE(  54000000,  1,  54, 6, 4, 1),
	PLL_RATE(  27000000,  1,  27, 6, 4, 1),
	{},
};

static struct rk_clk_armclk_rates rk3399_cpu_l_rates[]  = {
	{1800000000, 1},
	{1704000000, 1},
	{1608000000, 1},
	{1512000000, 1},
	{1488000000, 1},
	{1416000000, 1},
	{1200000000, 1},
	{1008000000, 1},
	{ 816000000, 1},
	{ 696000000, 1},
	{ 600000000, 1},
	{ 408000000, 1},
	{ 312000000, 1},
	{ 216000000, 1},
	{  96000000, 1},
};

static struct rk_clk_armclk_rates rk3399_cpu_b_rates[] = {
	{2208000000, 1},
	{2184000000, 1},
	{2088000000, 1},
	{2040000000, 1},
	{2016000000, 1},
	{1992000000, 1},
	{1896000000, 1},
	{1800000000, 1},
	{1704000000, 1},
	{1608000000, 1},
	{1512000000, 1},
	{1488000000, 1},
	{1416000000, 1},
	{1200000000, 1},
	{1008000000, 1},
	{ 816000000, 1},
	{ 696000000, 1},
	{ 600000000, 1},
	{ 408000000, 1},
	{ 312000000, 1},
	{ 216000000, 1},
	{  96000000, 1},
};

/* Standard PLL. */
#define PLL(_id, _name, _base)						\
{									\
	.type = RK3399_CLK_PLL,						\
	.clk.pll = &(struct rk_clk_pll_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = pll_src_p,			\
		.clkdef.parent_cnt = nitems(pll_src_p),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.base_offset = _base,					\
		.rates = rk3399_pll_rates,				\
	},								\
}

#define PLIST(_name) static const char *_name[]
PLIST(pll_src_p) = {"xin24m", "xin32k"};

PLIST(armclkl_p) = {"clk_core_l_lpll_src", "clk_core_l_bpll_src",
		    "clk_core_l_dpll_src", "clk_core_l_gpll_src"};
PLIST(armclkb_p) = {"clk_core_b_lpll_src", "clk_core_b_bpll_src",
		    "clk_core_b_dpll_src", "clk_core_b_gpll_src"};
PLIST(ddrclk_p) = {"clk_ddrc_lpll_src", "clk_ddrc_bpll_src",
		   "clk_ddrc_dpll_src", "clk_ddrc_gpll_src"};
PLIST(pll_src_cpll_gpll_p) =		{"cpll", "gpll"};
PLIST(pll_src_cpll_gpll_ppll_p) =	{"cpll", "gpll", "ppll"};
PLIST(pll_src_cpll_gpll_upll_p) =	{"cpll", "gpll", "upll"};
PLIST(pll_src_npll_cpll_gpll_p) =	{"npll", "cpll", "gpll"};
PLIST(pll_src_cpll_gpll_npll_npll_p) =	{"cpll", "gpll", "npll", "npll"};
PLIST(pll_src_cpll_gpll_npll_ppll_p) =	{"cpll", "gpll", "npll", "ppll" };
PLIST(pll_src_cpll_gpll_npll_24m_p) =	{"cpll", "gpll", "npll",  "xin24m" };
PLIST(pll_src_cpll_gpll_npll_usbphy480m_p)= {"cpll", "gpll", "npll", "clk_usbphy_480m" };
PLIST(pll_src_ppll_cpll_gpll_npll_upll_p) = { "ppll", "cpll", "gpll", "npll", "upll" };
PLIST(pll_src_cpll_gpll_npll_upll_24m_p)= { "cpll", "gpll", "npll", "upll", "xin24m" };
PLIST(pll_src_cpll_gpll_npll_ppll_upll_24m_p) = { "cpll", "gpll", "npll",  "ppll", "upll", "xin24m" };
PLIST(pll_src_vpll_cpll_gpll_gpll_p) = 	{"vpll", "cpll", "gpll", "gpll"};
PLIST(pll_src_vpll_cpll_gpll_npll_p) = 	{"vpll", "cpll", "gpll", "npll"};

PLIST(aclk_cci_p) = {"cpll_aclk_cci_src", "gpll_aclk_cci_src",
		     "npll_aclk_cci_src", "vpll_aclk_cci_src"};
PLIST(cci_trace_p) = {"cpll_cci_trace","gpll_cci_trace"};
PLIST(cs_p)= {"cpll_cs", "gpll_cs", "npll_cs","npll_cs"};
PLIST(aclk_perihp_p)= {"cpll_aclk_perihp_src", "gpll_aclk_perihp_src" };
PLIST(dclk_vop0_p) =	{"dclk_vop0_div", "dclk_vop0_frac"};
PLIST(dclk_vop1_p)= 	{"dclk_vop1_div", "dclk_vop1_frac"};

PLIST(clk_cif_p) = 	{"clk_cifout_src", "xin24m"};

PLIST(pll_src_24m_usbphy480m_p) = { "xin24m", "clk_usbphy_480m"};
PLIST(pll_src_24m_pciephy_p) = { "xin24m", "clk_pciephy_ref100m"};
PLIST(pll_src_24m_32k_cpll_gpll_p)= {"xin24m", "xin32k", "cpll", "gpll"};
PLIST(pciecore_cru_phy_p) = {"clk_pcie_core_cru", "clk_pcie_core_phy"};

PLIST(aclk_emmc_p)		= { "cpll_aclk_emmc_src", "gpll_aclk_emmc_src"};

PLIST(aclk_perilp0_p)		= { "cpll_aclk_perilp0_src",
					    "gpll_aclk_perilp0_src" };

PLIST(fclk_cm0s_p)			= { "cpll_fclk_cm0s_src",
					    "gpll_fclk_cm0s_src" };

PLIST(hclk_perilp1_p)		= { "cpll_hclk_perilp1_src",
					    "gpll_hclk_perilp1_src" };

PLIST(clk_testout1_p)		= { "clk_testout1_pll_src", "xin24m" };
PLIST(clk_testout2_p)		= { "clk_testout2_pll_src", "xin24m" };

PLIST(usbphy_480m_p)		= { "clk_usbphy0_480m_src",
					    "clk_usbphy1_480m_src" };
PLIST(aclk_gmac_p)		= { "cpll_aclk_gmac_src",
				    "gpll_aclk_gmac_src" };
PLIST(rmii_p)			= { "clk_gmac", "clkin_gmac" };
PLIST(spdif_p)			= { "clk_spdif_div", "clk_spdif_frac",
				    "clkin_i2s", "xin12m" };
PLIST(i2s0_p)			= { "clk_i2s0_div", "clk_i2s0_frac",
				    "clkin_i2s", "xin12m" };
PLIST(i2s1_p)			= { "clk_i2s1_div", "clk_i2s1_frac",
				    "clkin_i2s", "xin12m" };
PLIST(i2s2_p)			= { "clk_i2s2_div", "clk_i2s2_frac",
				    "clkin_i2s", "xin12m" };
PLIST(i2sch_p)			= {"clk_i2s0", "clk_i2s1", "clk_i2s2"};
PLIST(i2sout_p)			= {"clk_i2sout_src", "xin12m"};

PLIST(uart0_p)= {"clk_uart0_div", "clk_uart0_frac", "xin24m"};
PLIST(uart1_p)= {"clk_uart1_div", "clk_uart1_frac", "xin24m"};
PLIST(uart2_p)= {"clk_uart2_div", "clk_uart2_frac", "xin24m"};
PLIST(uart3_p)= {"clk_uart3_div", "clk_uart3_frac", "xin24m"};

static struct rk_clk rk3399_clks[] = {
	/* External clocks */
	LINK("xin24m"),
	FRATE(0, "xin32k", 32768),
	FFACT(0, "xin12m", "xin24m", 1, 2),
	FRATE(0, "clkin_i2s", 0),
	FRATE(0, "pclkin_cif", 0),
	LINK("clk_usbphy0_480m"),
	LINK("clk_usbphy1_480m"),
	LINK("clkin_gmac"),
	FRATE(0, "clk_pcie_core_phy", 0),
	FFACT(0, "clk_ddrc_div2", "clk_ddrc", 1, 2),

	/* PLLs */
	PLL(PLL_APLLL, "lpll", 0x00),
	PLL(PLL_APLLB, "bpll", 0x20),
	PLL(PLL_DPLL,  "dpll", 0x40),
	PLL(PLL_CPLL,  "cpll", 0x60),
	PLL(PLL_GPLL,  "gpll", 0x80),
	PLL(PLL_NPLL,  "npll", 0xA0),
	PLL(PLL_VPLL,  "vpll", 0xC0),

	/*  CRU_CLKSEL_CON0 */
	CDIV(0, "aclkm_core_l_c", "armclkl", 0,
	    0, 8, 5),
	ARMDIV(ARMCLKL, "armclkl", armclkl_p, rk3399_cpu_l_rates,
	    0, 0, 5,	6, 2, 0, 3),
	/* CRU_CLKSEL_CON1 */
	CDIV(0, "pclk_dbg_core_l_c", "armclkl", 0,
	    1, 8, 5),
	CDIV(0, "atclk_core_l_c", "armclkl", 0,
	    1, 0, 5),

	/* CRU_CLKSEL_CON2 */
	CDIV(0, "aclkm_core_b_c", "armclkb", 0,
	    2, 8, 5),
	ARMDIV(ARMCLKB, "armclkb", armclkb_p, rk3399_cpu_b_rates,
	    2, 0, 5,	6, 2, 1, 3),

	/* CRU_CLKSEL_CON3 */
	CDIV(0, "pclken_dbg_core_b", "pclk_dbg_core_b", 0,
	    3, 13, 2),
	CDIV(0, "pclk_dbg_core_b_c", "armclkb", 0,
	    3, 8, 5),
	CDIV(0, "atclk_core_b_c", "armclkb", 0,
	    3, 0, 5),

	/* CRU_CLKSEL_CON4 */
	COMP(0, "clk_cs", cs_p, 0,
	    4, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON5 */
	COMP(0, "clk_cci_trace_c", cci_trace_p, 0,
	    5, 8, 5,	15, 1),
	COMP(0, "aclk_cci_pre_c", aclk_cci_p, 0,
	    5, 0, 5, 	6, 2),

	/*  CRU_CLKSEL_CON6 */
	COMP(0, "pclk_ddr_c", pll_src_cpll_gpll_p, 0,
	    6, 8, 5,	15, 1),
	COMP(SCLK_DDRC, "clk_ddrc", ddrclk_p, 0,
	    6, 0, 3, 	4, 2),

	/* CRU_CLKSEL_CON7 */
	CDIV(0, "hclk_vcodec_pre_c", "aclk_vcodec_pre", 0,
	    7, 8, 5),
	COMP(0, "aclk_vcodec_pre_c", pll_src_cpll_gpll_npll_ppll_p, 0,
	    7, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON8 */
	CDIV(0, "hclk_vdu_pre_c", "aclk_vdu_pre", 0,
	    8, 8, 5),
	COMP(0, "aclk_vdu_pre_c", pll_src_cpll_gpll_npll_ppll_p, 0,
	    8, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON9 */
	COMP(0, "clk_vdu_ca_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    9, 8, 5,	14, 2),
	COMP(0, "clk_vdu_core_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    9, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON10 */
	CDIV(0, "hclk_iep_pre_c", "aclk_iep_pre", 0,
	    10, 8, 5),
	COMP(0, "aclk_iep_pre_c", pll_src_cpll_gpll_npll_ppll_p, 0,
	    10, 0, 5, 	6, 2),

	/* CRU_CLKSEL_CON11 */
	CDIV(0, "hclk_rga_pre_c", "aclk_rga_pre", 0,
	    11, 8, 5),
	COMP(0, "aclk_rga_pre_c", pll_src_cpll_gpll_npll_ppll_p, 0,
	    11, 0, 5,	 6, 2),

	/* CRU_CLKSEL_CON12 */
	COMP(0, "aclk_center_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    12, 8, 5, 	14, 2),
	COMP(SCLK_RGA_CORE, "clk_rga_core_c", pll_src_cpll_gpll_npll_ppll_p, 0,
	    12, 0, 5, 	6, 2),

	/* CRU_CLKSEL_CON13 */
	COMP(0, "hclk_sd_c", pll_src_cpll_gpll_p, 0,
	    13, 8, 5,	15, 1),
	COMP(0, "aclk_gpu_pre_c", pll_src_ppll_cpll_gpll_npll_upll_p, 0,
	    13, 0, 5,	5, 3),

	/* CRU_CLKSEL_CON14 */
	MUX(0, "upll", pll_src_24m_usbphy480m_p, 0,
	    14, 	15, 1),
	CDIV(0, "pclk_perihp_c", "aclk_perihp", 0,
	    14, 12, 2),
	CDIV(0, "hclk_perihp_c", "aclk_perihp", 0,
	    14, 8, 2),
	MUX(0, "clk_usbphy_480m", usbphy_480m_p, 0,
	    14,		 6, 1),
	COMP(0, "aclk_perihp_c", aclk_perihp_p, 0,
	    14, 0, 5,	7, 1),

	/* CRU_CLKSEL_CON15 */
	COMP(0, "clk_sdio_c", pll_src_cpll_gpll_npll_ppll_upll_24m_p, 0,
	    15, 0, 7,	8, 3),

	/* CRU_CLKSEL_CON16 */
	COMP(0, "clk_sdmmc_c", pll_src_cpll_gpll_npll_ppll_upll_24m_p, 0,
	    16, 0, 7,	8, 3),

	/* CRU_CLKSEL_CON17 */
	COMP(0, "clk_pcie_pm_c", pll_src_cpll_gpll_npll_24m_p, 0,
	    17, 0, 7,	8, 3),

	/* CRU_CLKSEL_CON18 */
	CDIV(0, "clk_pciephy_ref100m_c", "npll", 0,
	    18, 11, 5),
	MUX(SCLK_PCIEPHY_REF, "clk_pciephy_ref", pll_src_24m_pciephy_p, 0,
	    18,		10, 1),
	MUX(SCLK_PCIE_CORE, "clk_pcie_core", pciecore_cru_phy_p, 0,
	    18,		7, 1),
	COMP(0, "clk_pcie_core_cru_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    18, 0, 7,	8, 2),

	/* CRU_CLKSEL_CON19 */
	CDIV(0, "pclk_gmac_pre_c", "aclk_gmac_pre", 0,
	    19, 8, 3),
	MUX(SCLK_RMII_SRC, "clk_rmii_src",rmii_p, 0,
	    19,		4, 1),
	MUX(SCLK_HSICPHY, "clk_hsicphy_c", pll_src_cpll_gpll_npll_usbphy480m_p, 0,
	    19,		0, 2),

	/* CRU_CLKSEL_CON20 */
	COMP(0, "clk_gmac_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    20, 8, 5,	14, 2),
	COMP(0, "aclk_gmac_pre_c", aclk_gmac_p, 0,
	    20, 0, 5,	7, 1),

	/* CRU_CLKSEL_CON21 */
	COMP(ACLK_EMMC, "aclk_emmc", aclk_emmc_p, 0,
	    21, 0, 5,	7, 1),

	/* CRU_CLKSEL_CON22 */
	COMP(0, "clk_emmc_c", pll_src_cpll_gpll_npll_upll_24m_p, 0,
	    22, 0, 7,	8, 3),

	/* CRU_CLKSEL_CON23 */
	CDIV(0, "pclk_perilp0_c", "aclk_perilp0", 0,
	    23, 12, 3),
	CDIV(0, "hclk_perilp0_c", "aclk_perilp0", 0,
	    23, 8, 2),
	COMP(0, "aclk_perilp0_c", aclk_perilp0_p, 0,
	    23, 0, 5,	7, 1),

	/* CRU_CLKSEL_CON24 */
	COMP(0, "fclk_cm0s_c", fclk_cm0s_p, 0,
	    24, 8, 5,	15, 1),
	COMP(0, "clk_crypto0_c", pll_src_cpll_gpll_ppll_p, 0,
	    24, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON25 */
	CDIV(0, "pclk_perilp1_c", "hclk_perilp1", 0,
	    25, 8, 3),
	COMP(HCLK_PERILP1, "hclk_perilp1", hclk_perilp1_p, 0,
	    25, 0, 5,	7, 1),

	/* CRU_CLKSEL_CON26 */
	CDIV(0, "clk_saradc_c", "xin24m", 0,
	    26, 8, 8),
	COMP(0, "clk_crypto1_c", pll_src_cpll_gpll_ppll_p, 0,
	    26, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON27 */
	COMP(0, "clk_tsadc_c", pll_src_p, 0,
	    27, 0, 10,	15, 1),

	/* CRU_CLKSEL_CON28 */
	MUX(0, "clk_i2s0_mux", i2s0_p, 0,
	    28, 8, 2),
	COMP(0, "clk_i2s0_div_c", pll_src_cpll_gpll_p, 0,
	    28, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON29 */
	MUX(0, "clk_i2s1_mux", i2s1_p, 0,
	    29,		8, 2),
	COMP(0, "clk_i2s1_div_c", pll_src_cpll_gpll_p, 0,
	    29, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON30 */
	MUX(0, "clk_i2s2_mux", i2s2_p, 0,
	    30,		8, 2),
	COMP(0, "clk_i2s2_div_c", pll_src_cpll_gpll_p, 0,
	    30, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON31 */
	MUX(0, "clk_i2sout_c", i2sout_p, 0,
	    31,		2, 1),
	MUX(0, "clk_i2sout_src", i2sch_p, 0,
	    31,		0, 2),

	/* CRU_CLKSEL_CON32 */
	COMP(0, "clk_spdif_rec_dptx_c", pll_src_cpll_gpll_p, 0,
	    32, 8, 5,	15, 1),
	MUX(0, "clk_spdif_mux", spdif_p, 0,
	    32,		13, 2),
	COMP(0, "clk_spdif_div_c", pll_src_cpll_gpll_p, 0,
	    32, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON33 */
	MUX(0, "clk_uart_src", pll_src_cpll_gpll_p, 0,
	    33,		15, 1),
	MUX(0, "clk_uart0_src", pll_src_cpll_gpll_upll_p, 0,
	    33,		12, 2),
	MUX(SCLK_UART0, "clk_uart0", uart0_p, 0,
	    33,		8, 2),
	CDIV(0, "clk_uart0_div_c", "clk_uart0_src", 0,
	    33, 0, 7),

	/* CRU_CLKSEL_CON34 */
	MUX(SCLK_UART1, "clk_uart1", uart1_p, 0,
	    34,		8, 2),
	CDIV(0, "clk_uart1_div_c", "clk_uart_src", 0,
	    34, 0, 7),

	/* CRU_CLKSEL_CON35 */
	MUX(SCLK_UART2, "clk_uart2", uart2_p, 0,
	    35,		8, 2),
	CDIV(0, "clk_uart2_div_c", "clk_uart_src", 0,
	    35, 0, 7),

	/* CRU_CLKSEL_CON36 */
	MUX(SCLK_UART3, "clk_uart3", uart3_p, 0,
	    36,		8, 2),
	CDIV(0, "clk_uart3_div_c", "clk_uart_src", 0,
	    36, 0, 7),

	/* CRU_CLKSEL_CON37 */
	/* unused */

	/* CRU_CLKSEL_CON38 */
	MUX(0, "clk_testout2_pll_src", pll_src_cpll_gpll_npll_npll_p, 0,
	    38,		14, 2),
	COMP(0, "clk_testout2_c", clk_testout2_p, 0,
	    38, 8, 5,	13, 1),
	MUX(0, "clk_testout1_pll_src", pll_src_cpll_gpll_npll_npll_p, 0,
	    38,		6, 2),
	COMP(0, "clk_testout1_c", clk_testout1_p, 0,
	    38, 0, 5,	5, 1),

	/* CRU_CLKSEL_CON39 */
	COMP(0, "aclk_usb3_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    39, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON40 */
	COMP(0, "clk_usb3otg0_suspend_c", pll_src_p, 0,
	    40, 0, 10,	15, 1),

	/* CRU_CLKSEL_CON41 */
	COMP(0, "clk_usb3otg1_suspend_c", pll_src_p, 0,
	    41, 0, 10,	15, 1),

	/* CRU_CLKSEL_CON42 */
	COMP(0, "aclk_hdcp_c", pll_src_cpll_gpll_ppll_p, 0,
	    42, 8, 5,	14, 2),
	COMP(0, "aclk_vio_c", pll_src_cpll_gpll_ppll_p, 0,
	    42, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON43 */
	CDIV(0, "pclk_hdcp_c", "aclk_hdcp", 0,
	    43, 10, 5),
	CDIV(0, "hclk_hdcp_c", "aclk_hdcp", 0,
	    43, 5, 5),
	CDIV(0, "pclk_vio_c", "aclk_vio", 0,
	    43, 0, 5),

	/* CRU_CLKSEL_CON44 */
	COMP(0, "pclk_edp_c", pll_src_cpll_gpll_p, 0,
	    44, 8, 6,	15, 1),

	/* CRU_CLKSEL_CON45  - XXX clocks in mux are reversed in TRM !!!*/
	COMP(0, "clk_hdmi_cec_c", pll_src_p, 0,
	    45, 0, 10,	15, 1),

	/* CRU_CLKSEL_CON46 */
	COMP(0, "clk_dp_core_c", pll_src_npll_cpll_gpll_p, 0,
	    46, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON47 */
	CDIV(0, "hclk_vop0_pre_c", "aclk_vop0_pre_c", 0,
	    47, 8, 5),
	COMP(0, "aclk_vop0_pre_c", pll_src_vpll_cpll_gpll_npll_p, 0,
	    47, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON48 */
	CDIV(0, "hclk_vop1_pre_c", "aclk_vop1_pre", 0,
	    48, 8, 5),
	COMP(0, "aclk_vop1_pre_c", pll_src_vpll_cpll_gpll_npll_p, 0,
	    48, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON49 */
	MUX(DCLK_VOP0, "dclk_vop0", dclk_vop0_p, 0,
	    49,		11, 1),
	COMP(0, "dclk_vop0_div_c", pll_src_vpll_cpll_gpll_gpll_p, 0,
	    49, 0, 8,	8, 2),

	/* CRU_CLKSEL_CON50 */
	MUX(DCLK_VOP1, "dclk_vop1", dclk_vop1_p, 0,
	    50,		11, 1),
	COMP(0, "dclk_vop1_div_c", pll_src_vpll_cpll_gpll_gpll_p, 0,
	    50, 0, 8,	8, 2),

	/* CRU_CLKSEL_CON51 */
	COMP(0, "clk_vop0_pwm_c", pll_src_vpll_cpll_gpll_gpll_p, 0,
	    51, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON52 */
	COMP(0, "clk_vop1_pwm_c", pll_src_vpll_cpll_gpll_gpll_p, 0,
	    52, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON53 */
	CDIV(0, "hclk_isp0_c", "aclk_isp0", 0,
	    53, 8, 5),
	COMP(0, "aclk_isp0_c", pll_src_cpll_gpll_ppll_p, 0,
	    53, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON54 */
	CDIV(0, "hclk_isp1_c", "aclk_isp1", 0,
	    54, 8, 5),
	COMP(0, "aclk_isp1_c", pll_src_cpll_gpll_ppll_p, 0,
	    54, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON55 */
	COMP(0, "clk_isp1_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    55, 8, 5,	14, 2),
	COMP(0, "clk_isp0_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    55, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON56 */
	COMP(0, "aclk_gic_pre_c", pll_src_cpll_gpll_p, 0,
	    56, 8, 5,	15, 1),
	MUX(0, "clk_cifout_src_c", pll_src_cpll_gpll_npll_npll_p, 0,
	    56,		6, 2),
	COMP(SCLK_CIF_OUT, "clk_cifout", clk_cif_p, 0,
	    56, 0, 5,	5, 1),

	/* CRU_CLKSEL_CON57 */
	CDIV(0, "clk_test_24m", "xin24m", 0,
	    57, 6, 10),
	CDIV(PCLK_ALIVE, "pclk_alive", "gpll", 0,
	    57, 0, 5),

	/* CRU_CLKSEL_CON58 */
	COMP(0, "clk_spi5_c", pll_src_cpll_gpll_p, 0,
	    58, 8, 7,	15, 1),
	MUX(0, "clk_test_pre", pll_src_cpll_gpll_p, 0,
	    58,		7, 1),
	CDIV(0, "clk_test_c", "clk_test_pre", 0,
	    58, 0, 5),

	/* CRU_CLKSEL_CON59 */
	COMP(0, "clk_spi1_c", pll_src_cpll_gpll_p, 0,
	    59, 8, 7,	15, 1),
	COMP(0, "clk_spi0_c", pll_src_cpll_gpll_p, 0,
	    59, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON60 */
	COMP(0, "clk_spi4_c", pll_src_cpll_gpll_p, 0,
	    60, 8, 7,	15, 1),
	COMP(0, "clk_spi2_c", pll_src_cpll_gpll_p, 0,
	    60, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON61 */
	COMP(0, "clk_i2c5_c", pll_src_cpll_gpll_p, 0,
	    61, 8, 7,	15, 1),
	COMP(0, "clk_i2c1_c", pll_src_cpll_gpll_p, 0,
	    61, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON62 */
	COMP(0, "clk_i2c6_c", pll_src_cpll_gpll_p, 0,
	    62, 8, 7,	15, 1),
	COMP(0, "clk_i2c2_c", pll_src_cpll_gpll_p, 0,
	    62, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON63 */
	COMP(0, "clk_i2c7_c", pll_src_cpll_gpll_p, 0,
	    63, 8, 7,	15, 1),
	COMP(0, "clk_i2c3_c", pll_src_cpll_gpll_p, 0,
	    63, 0, 7,	7, 1),

	/* CRU_CLKSEL_CON64 */
	COMP(0, "clk_uphy0_tcpdphy_ref_c", pll_src_p, 0,
	    64, 8, 5,	15, 1),
	COMP(0, "clk_uphy0_tcpdcore_c", pll_src_24m_32k_cpll_gpll_p, 0,
	    64, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON65 */
	COMP(0, "clk_uphy1_tcpdphy_ref_c", pll_src_p, 0,
	    65, 8, 5,	15, 1),
	COMP(0, "clk_uphy1_tcpdcore_c", pll_src_24m_32k_cpll_gpll_p, 0,
	    65, 0, 5,	6, 2),

	/* CRU_CLKSEL_CON99  - 107 */
	FRACT(0, "clk_spdif_frac_c", "clk_spdif_div", 0,
	    99),
	FRACT(0, "clk_i2s0_frac_c", "clk_i2s0_div", 0,
	    96),
	FRACT(0, "clk_i2s1_frac_c", "clk_i2s1_div", 0,
	    97),
	FRACT(0, "clk_i2s2_frac_c", "clk_i2s2_div", 0,
	    98),
	FRACT(0, "clk_uart0_frac_c", "clk_uart0_div", 0,
	    100),
	FRACT(0, "clk_uart1_frac_c", "clk_uart1_div", 0,
	    101),
	FRACT(0, "clk_uart2_frac_c", "clk_uart2_div", 0,
	    102),
	FRACT(0, "clk_uart3_frac_c", "clk_uart3_div", 0,
	    103),
	FRACT(0, "clk_test_frac_c", "clk_test_pre", 0,
	    105),
	FRACT(DCLK_VOP0_FRAC, "dclk_vop0_frac", "dclk_vop0_div", 0,
	    106),
	FRACT(DCLK_VOP1_FRAC, "dclk_vop1_frac", "dclk_vop1_div", 0,
	    107),

/* Not yet implemented yet
 *	MMC(SCLK_SDMMC_DRV,     "sdmmc_drv",    "clk_sdmmc", RK3399_SDMMC_CON0, 1),
 *	MMC(SCLK_SDMMC_SAMPLE,  "sdmmc_sample", "clk_sdmmc", RK3399_SDMMC_CON1, 1),
 *	MMC(SCLK_SDIO_DRV,      "sdio_drv",     "clk_sdio",  RK3399_SDIO_CON0,  1),
 *	MMC(SCLK_SDIO_SAMPLE,   "sdio_sample",  "clk_sdio",  RK3399_SDIO_CON1,  1),
 */

};

static int
rk3399_cru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3399-cru")) {
		device_set_desc(dev, "Rockchip RK3399 Clock and Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
rk3399_cru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->gates = rk3399_gates;
	sc->ngates = nitems(rk3399_gates);

	sc->clks = rk3399_clks;
	sc->nclks = nitems(rk3399_clks);

	sc->reset_offset = 0x400;
	sc->reset_num = 335;

	return (rk_cru_attach(dev));
}

static device_method_t rk3399_cru_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3399_cru_probe),
	DEVMETHOD(device_attach,	rk3399_cru_attach),

	DEVMETHOD_END
};

static devclass_t rk3399_cru_devclass;

DEFINE_CLASS_1(rk3399_cru, rk3399_cru_driver, rk3399_cru_methods,
  sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3399_cru, simplebus, rk3399_cru_driver,
    rk3399_cru_devclass, 0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
