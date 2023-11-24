/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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
 */

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

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm64/rockchip/clk/rk_cru.h>

#include <dt-bindings/clock/rk3288-cru.h>

#define	CRU_SOFTRST_SIZE	12

#define	CRU_APLL_CON(x)		(0x000 + (x) * 0x4)
#define	CRU_DPLL_CON(x)		(0x010 + (x) * 0x4)
#define	CRU_CPLL_CON(x)		(0x020 + (x) * 0x4)
#define	CRU_GPLL_CON(x)		(0x030 + (x) * 0x4)
#define	CRU_NPLL_CON(x)		(0x040 + (x) * 0x4)
#define	CRU_MODE_CON		0x050
#define	CRU_CLKSEL_CON(x)	(0x060 + (x) * 0x4)
#define	CRU_CLKGATE_CON(x)	(0x160 + (x) * 0x4)
#define	CRU_GLB_SRST_FST_VALUE	0x1b0
#define	CRU_GLB_SRST_SND_VALUE	0x1b4
#define	CRU_SOFTRST_CON(x)	(0x1b8 + (x) * 0x4)
#define	CRU_MISC_CON		0x1e8
#define	CRU_GLB_CNT_TH		0x1ec
#define	CRU_GLB_RST_CON		0x1f0
#define	CRU_GLB_RST_ST		0x1f8
#define	CRU_SDMMC_CON0		0x200
#define	CRU_SDMMC_CON1		0x204
#define	CRU_SDIO0_CON0		0x208
#define	CRU_SDIO0_CON1		0x20c
#define	CRU_SDIO1_CON0		0x210
#define	CRU_SDIO1_CON1		0x214
#define	CRU_EMMC_CON0		0x218
#define	CRU_EMMC_CON1		0x21c

/* GATES */
#define	GATE(_idx, _clkname, _pname, _o, _s)				\
{									\
	.id = _idx,							\
	.name = _clkname,						\
	.parent_name = _pname,						\
	.offset = CRU_CLKGATE_CON(_o),					\
	.shift = _s,							\
}

static struct rk_cru_gate rk3288_gates[] = {
	/* CRU_CLKGATE_CON0 */
	GATE(0, "sclk_acc_efuse", "xin24m",			0, 12),
	GATE(0, "cpll_aclk_cpu", "cpll",			0, 11),
	GATE(0, "gpll_aclk_cpu", "gpll",			0, 10),
	GATE(0, "gpll_ddr", "gpll",				0, 9),
	GATE(0, "dpll_ddr", "dpll",				0, 8),
	GATE(0, "aclk_bus_2pmu", "aclk_cpu_pre",		0, 7),
	GATE(PCLK_CPU, "pclk_cpu", "pclk_cpu_s",		0, 5),
	GATE(HCLK_CPU, "hclk_cpu", "hclk_cpu_s",		0, 4),
	GATE(ACLK_CPU, "aclk_cpu", "aclk_cpu_pre",		0, 3),
	GATE(0, "gpll_core", "gpll",				0, 2),
	GATE(0, "apll_core", "apll",				0, 1),


	/* CRU_CLKGATE_CON1 */
	GATE(0, "uart3_frac", "uart3_frac_s",			1, 15),
	GATE(0, "uart3_src", "uart3_src_s",			1, 14),
	GATE(0, "uart2_frac", "uart2_frac_s",			1, 13),
	GATE(0, "uart2_src", "uart2_src_s",			1, 12),
	GATE(0, "uart1_frac", "uart1_frac_s",			1, 11),
	GATE(0, "uart1_src", "uart1_src_s",			1, 10),
	GATE(0, "uart0_frac", "uart0_frac_s",			1, 9),
	GATE(0, "uart0_src", "uart0_src_s",			1, 8),
	GATE(SCLK_TIMER5, "sclk_timer5", "xin24m",		1, 5),
	GATE(SCLK_TIMER4, "sclk_timer4", "xin24m",		1, 4),
	GATE(SCLK_TIMER3, "sclk_timer3", "xin24m",		1, 3),
	GATE(SCLK_TIMER2, "sclk_timer2", "xin24m",		1, 2),
	GATE(SCLK_TIMER1, "sclk_timer1", "xin24m",		1, 1),
	GATE(SCLK_TIMER0, "sclk_timer0", "xin24m",		1, 0),

	/* CRU_CLKGATE_CON2 */
	GATE(0, "uart4_frac", "uart4_frac_s",			2, 13),
	GATE(0, "uart4_src", "uart4_src_s",			2, 12),
	GATE(SCLK_SPI2, "sclk_spi2", "sclk_spi2_s",		2, 11),
	GATE(SCLK_SPI1, "sclk_spi1", "sclk_spi1_s",		2, 10),
	GATE(SCLK_SPI0, "sclk_spi0", "sclk_spi0_s",		2, 9),
	GATE(SCLK_SARADC, "sclk_saradc", "sclk_saradc_s",	2, 8),
	GATE(SCLK_TSADC, "sclk_tsadc", "sclk_tsadc_s",		2, 7),
	GATE(0, "hsadc_src", "hsadc_src_s",			2, 6),
	GATE(0, "mac_pll_src", "mac_pll_src_s",			2, 5),
	GATE(PCLK_PERI, "pclk_peri", "pclk_peri_s",		2, 3),
	GATE(HCLK_PERI, "hclk_peri", "hclk_peri_s",		2, 2),
	GATE(ACLK_PERI, "aclk_peri", "aclk_peri_src",		2, 1),
	GATE(0, "aclk_peri_src", "aclk_peri_src_s",		2, 0),

	/* CRU_CLKGATE_CON3 */
	GATE(SCLK_ISP_JPE, "sclk_isp_jpe", "sclk_isp_jpe_s",	3, 15),
	GATE(SCLK_ISP, "sclk_isp", "sclk_isp_s",		3, 14),
	GATE(SCLK_EDP, "sclk_edp", "sclk_edp_s",		3, 13),
	GATE(SCLK_EDP_24M, "sclk_edp_24m", "sclk_edp_24m_s",	3, 12),
	GATE(0, "aclk_vdpu", "aclk_vdpu_s",			3, 11),
	GATE(0, "hclk_vcodec_pre", "hclk_vcodec_pre_s",		3, 10),
	GATE(0, "aclk_vepu", "aclk_vepu_s",			3, 9),
	GATE(0, "vip_src", "vip_src_s",				3, 7),
/* 6 - Not in TRM, sclk_hsicphy480m in Linux */
	GATE(0, "aclk_rga_pre", "aclk_rga_pre_s",		3, 5),
	GATE(SCLK_RGA, "sclk_rga", "sclk_rga_s",		3, 4),
	GATE(DCLK_VOP1, "dclk_vop1", "dclk_vop1_s",		3, 3),
	GATE(0, "aclk_vio1", "aclk_vio1_s",			3, 2),
	GATE(DCLK_VOP0, "dclk_vop0", "dclk_vop0_s",		3, 1),
	GATE(0, "aclk_vio0", "aclk_vio0_s",			3, 0),

	/* CRU_CLKGATE_CON4 */
/* 15 - Test clock generator */
	GATE(0, "jtag", "ext_jtag",				4, 14),
	GATE(0, "sclk_ddrphy1", "ddrphy",			4, 13),
	GATE(0, "sclk_ddrphy0", "ddrphy",			4, 12),
	GATE(0, "sclk_tspout", "sclk_tspout_s",			4, 11),
	GATE(0, "sclk_tsp", "sclk_tsp_s",			4, 10),
	GATE(SCLK_SPDIF8CH, "sclk_spdif_8ch", "spdif_8ch_mux",	4, 9),
	GATE(0, "spdif_8ch_frac", "spdif_8ch_frac_s",		4, 8),
	GATE(0, "spdif_8ch_pre", "spdif_8ch_pre_s",		4, 7),
	GATE(SCLK_SPDIF, "sclk_spdif", "spdif_mux",		4, 6),
	GATE(0, "spdif_frac", "spdif_frac_s",			4, 5),
	GATE(0, "spdif_pre", "spdif_pre_s",			4, 4),
	GATE(SCLK_I2S0, "sclk_i2s0", "i2s_pre",			4, 3),
	GATE(0, "i2s_frac", "i2s_frac_s",			4, 2),
	GATE(0, "i2s_src", "i2s_src_s",				4, 1),
	GATE(SCLK_I2S0_OUT, "i2s0_clkout", "i2s0_clkout_s",	4, 1),

	/* CRU_CLKGATE_CON5 */
	GATE(SCLK_MIPIDSI_24M, "sclk_mipidsi_24m", "xin24m",	5, 15),
	GATE(SCLK_USBPHY480M_SRC, "usbphy480m_src", "usbphy480m_src_s", 5, 14),
	GATE(SCLK_PS2C, "sclk_ps2c", "xin24m",			5, 13),
	GATE(SCLK_HDMI_HDCP, "sclk_hdmi_hdcp", "xin24m",	5, 12),
	GATE(SCLK_HDMI_CEC, "sclk_hdmi_cec", "xin32k",		5, 11),
	GATE(SCLK_PVTM_GPU, "sclk_pvtm_gpu", "xin24m",		5, 10),
	GATE(SCLK_PVTM_CORE, "sclk_pvtm_core", "xin24m",	5, 9),
	GATE(0, "pclk_pd_pmu", "pclk_pd_pmu_s",			5, 8),
	GATE(SCLK_GPU, "sclk_gpu", "sclk_gpu_s",		5, 7),
	GATE(SCLK_NANDC1, "sclk_nandc1", "sclk_nandc1_s",	5, 6),
	GATE(SCLK_NANDC0, "sclk_nandc0", "sclk_nandc0_s",	5, 5),
	GATE(SCLK_CRYPTO, "crypto", "crypto_s",			5, 4),
	GATE(SCLK_MACREF_OUT, "sclk_macref_out", "mac_clk",	5, 3),
	GATE(SCLK_MACREF, "sclk_macref", "mac_clk",		5, 2),
	GATE(SCLK_MAC_TX, "sclk_mac_tx", "mac_clk",		5, 1),
	GATE(SCLK_MAC_RX, "sclk_mac_rx", "mac_clk",		5, 0),


	/* CRU_CLKGATE_CON6 */
	GATE(PCLK_I2C4, "pclk_i2c4", "pclk_peri",		6, 15),
	GATE(PCLK_I2C3, "pclk_i2c3", "pclk_peri",		6, 14),
	GATE(PCLK_I2C1, "pclk_i2c1", "pclk_peri",		6, 13),
	GATE(PCLK_UART4, "pclk_uart4", "pclk_peri",		6, 12),
	GATE(PCLK_UART3, "pclk_uart3", "pclk_peri",		6, 11),
	GATE(PCLK_UART1, "pclk_uart1", "pclk_peri",		6, 9),
	GATE(PCLK_UART0, "pclk_uart0", "pclk_peri",		6, 8),
	GATE(PCLK_PS2C, "pclk_ps2c", "pclk_peri",		6, 7),
	GATE(PCLK_SPI2, "pclk_spi2", "pclk_peri",		6, 6),
	GATE(PCLK_SPI1, "pclk_spi1", "pclk_peri",		6, 5),
	GATE(PCLK_SPI0, "pclk_spi0", "pclk_peri",		6, 4),
	GATE(ACLK_DMAC2, "aclk_dmac2", "aclk_peri",		6, 3),
	GATE(0, "aclk_peri_axi_matrix", "aclk_peri",		6, 2),
	GATE(0, "pclk_peri_matrix", "pclk_peri",		6, 1),
	GATE(0, "hclk_peri_matrix", "hclk_peri",		6, 0),


	/* CRU_CLKGATE_CON7 */
	GATE(HCLK_NANDC1, "hclk_nandc1", "hclk_peri",		7, 15),
	GATE(HCLK_NANDC0, "hclk_nandc0", "hclk_peri",		7, 14),
	GATE(0, "hclk_mem", "hclk_peri",			7, 13),
	GATE(0, "hclk_emem", "hclk_peri",			7, 12),
	GATE(0, "aclk_peri_niu", "aclk_peri",			7, 11),
	GATE(0, "hclk_peri_ahb_arbi", "hclk_peri",		7, 10),
	GATE(0, "hclk_usb_peri", "hclk_peri",			7, 9),
/* 8 - Not in TRM  - hclk_hsic in Linux	 */
	GATE(HCLK_USBHOST1, "hclk_host1", "hclk_peri",		7, 7),
	GATE(HCLK_USBHOST0, "hclk_host0", "hclk_peri",		7, 6),
	GATE(0, "pmu_hclk_otg0", "hclk_peri",			7, 5),
	GATE(HCLK_OTG0, "hclk_otg0", "hclk_peri",		7, 4),
	GATE(PCLK_SIM, "pclk_sim", "pclk_peri",			7, 3),
	GATE(PCLK_TSADC, "pclk_tsadc", "pclk_peri",		7, 2),
	GATE(PCLK_SARADC, "pclk_saradc", "pclk_peri",		7, 1),
	GATE(PCLK_I2C5, "pclk_i2c5", "pclk_peri",		7, 0),

	/* CRU_CLKGATE_CON8 */
	GATE(ACLK_MMU, "aclk_mmu", "aclk_peri",			8, 12),
/* 11 - 9	27m_tsp, hsadc_1_tsp, hsadc_1_tsp			*/
	GATE(HCLK_TSP, "hclk_tsp", "hclk_peri",			8, 8),
	GATE(HCLK_HSADC, "hclk_hsadc", "hclk_peri",		8, 7),
	GATE(HCLK_EMMC, "hclk_emmc", "hclk_peri",		8, 6),
	GATE(HCLK_SDIO1, "hclk_sdio1", "hclk_peri",		8, 5),
	GATE(HCLK_SDIO0, "hclk_sdio0", "hclk_peri",		8, 4),
	GATE(HCLK_SDMMC, "hclk_sdmmc", "hclk_peri",		8, 3),
	GATE(HCLK_GPS, "hclk_gps", "aclk_peri",			8, 2),
	GATE(PCLK_GMAC, "pclk_gmac", "pclk_peri",		8, 1),
	GATE(ACLK_GMAC, "aclk_gmac", "aclk_peri",		8, 0),

	/* CRU_CLKGATE_CON9 */
	GATE(HCLK_VCODEC, "hclk_vcodec", "hclk_vcodec_pre",	9, 1),
	GATE(ACLK_VCODEC, "aclk_vcodec", "aclk_vcodec_pre",	9, 0),

	/* CRU_CLKGATE_CON10 */
	GATE(PCLK_PUBL0, "pclk_publ0", "pclk_cpu",		10, 15),
	GATE(PCLK_DDRUPCTL0, "pclk_ddrupctl0", "pclk_cpu",	10, 14),
	GATE(0, "aclk_strc_sys", "aclk_cpu",			10, 13),
	GATE(ACLK_DMAC1, "aclk_dmac1", "aclk_cpu",		10, 12),
	GATE(HCLK_SPDIF8CH, "hclk_spdif_8ch", "hclk_cpu",	10, 11),
	GATE(HCLK_SPDIF, "hclk_spdif", "hclk_cpu",		10, 10),
	GATE(HCLK_ROM, "hclk_rom", "hclk_cpu",			10, 9),
	GATE(HCLK_I2S0, "hclk_i2s0", "hclk_cpu",		10, 8),
	GATE(0, "sclk_intmem2", "aclk_cpu",			10, 7),
	GATE(0, "sclk_intmem1", "aclk_cpu",			10, 6),
	GATE(0, "sclk_intmem0", "aclk_cpu",			10, 5),
	GATE(0, "aclk_intmem", "aclk_cpu",			10, 4),
	GATE(PCLK_I2C2, "pclk_i2c2", "pclk_cpu",		10, 3),
	GATE(PCLK_I2C0, "pclk_i2c0", "pclk_cpu",		10, 2),
	GATE(PCLK_TIMER, "pclk_timer", "pclk_cpu",		10, 1),
	GATE(PCLK_PWM, "pclk_pwm", "pclk_cpu",			10, 0),

	/* CRU_CLKGATE_CON11 */
	GATE(PCLK_RKPWM, "pclk_rkpwm", "pclk_cpu",		11, 11),
	GATE(PCLK_EFUSE256, "pclk_efuse_256", "pclk_cpu",	11, 10),
	GATE(PCLK_UART2, "pclk_uart2", "pclk_cpu",		11, 9),
	GATE(0, "aclk_ccp", "aclk_cpu",				11, 8),
	GATE(HCLK_CRYPTO, "hclk_crypto", "hclk_cpu",		11, 7),
	GATE(ACLK_CRYPTO, "aclk_crypto", "aclk_cpu",		11, 6),
	GATE(0, "nclk_ddrupctl1", "ddrphy",			11, 5),
	GATE(0, "nclk_ddrupctl0", "ddrphy",			11, 4),
	GATE(PCLK_TZPC, "pclk_tzpc", "pclk_cpu",		11, 3),
	GATE(PCLK_EFUSE1024, "pclk_efuse_1024", "pclk_cpu",	11, 2),
	GATE(PCLK_PUBL1, "pclk_publ1", "pclk_cpu",		11, 1),
	GATE(PCLK_DDRUPCTL1, "pclk_ddrupctl1", "pclk_cpu",	11, 0),

	/* CRU_CLKGATE_CON12 */
	GATE(0, "pclk_core_niu", "pclk_dbg_pre",		12, 11),
	GATE(0, "cs_dbg", "pclk_dbg_pre",			12, 10),
	GATE(0, "pclk_dbg", "pclk_dbg_pre",			12, 9),
	GATE(0, "armcore0", "armcore0_s",			12, 8),
	GATE(0, "armcore1", "armcore1_s",			12, 7),
	GATE(0, "armcore2", "armcore2_s",			12, 6),
	GATE(0, "armcore3", "armcore3_s",			12, 5),
	GATE(0, "l2ram", "l2ram_s",				12, 4),
	GATE(0, "aclk_core_m0", "aclk_core_m0_s",		12, 3),
	GATE(0, "aclk_core_mp", "aclk_core_mp_s",		12, 2),
	GATE(0, "atclk", "atclk_s",				12, 1),
	GATE(0, "pclk_dbg_pre", "pclk_dbg_pre_s",		12, 0),

	/* CRU_CLKGATE_CON13 */
	GATE(SCLK_HEVC_CORE, "sclk_hevc_core", "sclk_hevc_core_s", 13, 15),
	GATE(SCLK_HEVC_CABAC, "sclk_hevc_cabac", "sclk_hevc_cabac_s", 13, 14),
	GATE(ACLK_HEVC, "aclk_hevc", "aclk_hevc_s",		13, 13),
	GATE(0, "wii", "wifi_frac_s",				13, 12),
	GATE(SCLK_LCDC_PWM1, "sclk_lcdc_pwm1", "xin24m",	13, 11),
	GATE(SCLK_LCDC_PWM0, "sclk_lcdc_pwm0", "xin24m",	13, 10),
/* 9 - Not in TRM  - hsicphy12m_xin12m in Linux	 */
	GATE(0, "c2c_host", "aclk_cpu_src",			13, 8),
	GATE(SCLK_OTG_ADP, "sclk_otg_adp", "xin32k",		13, 7),
	GATE(SCLK_OTGPHY2, "sclk_otgphy2", "xin24m",		13, 6),
	GATE(SCLK_OTGPHY1, "sclk_otgphy1", "xin24m",		13, 5),
	GATE(SCLK_OTGPHY0, "sclk_otgphy0", "xin24m",		13, 4),
	GATE(SCLK_EMMC, "sclk_emmc", "sclk_emmc_s",		13, 3),
	GATE(SCLK_SDIO1, "sclk_sdio1", "sclk_sdio1_s",		13, 2),
	GATE(SCLK_SDIO0, "sclk_sdio0", "sclk_sdio0_s",		13, 1),
	GATE(SCLK_SDMMC, "sclk_sdmmc", "sclk_sdmmc_s",		13, 0),

	/* CRU_CLKGATE_CON14 */
	GATE(0, "pclk_alive_niu", "pclk_pd_alive",		14, 12),
	GATE(PCLK_GRF, "pclk_grf", "pclk_pd_alive",		14, 11),
	GATE(PCLK_GPIO8, "pclk_gpio8", "pclk_pd_alive",		14, 8),
	GATE(PCLK_GPIO7, "pclk_gpio7", "pclk_pd_alive",		14, 7),
	GATE(PCLK_GPIO6, "pclk_gpio6", "pclk_pd_alive",		14, 6),
	GATE(PCLK_GPIO5, "pclk_gpio5", "pclk_pd_alive",		14, 5),
	GATE(PCLK_GPIO4, "pclk_gpio4", "pclk_pd_alive",		14, 4),
	GATE(PCLK_GPIO3, "pclk_gpio3", "pclk_pd_alive",		14, 3),
	GATE(PCLK_GPIO2, "pclk_gpio2", "pclk_pd_alive",		14, 2),
	GATE(PCLK_GPIO1, "pclk_gpio1", "pclk_pd_alive",		14, 1),

	/* CRU_CLKGATE_CON15*/
	GATE(HCLK_VIP, "hclk_vip", "hclk_vio",			15, 15),
	GATE(ACLK_VIP, "aclk_vip", "aclk_vio0",			15, 14),
	GATE(ACLK_RGA_NIU, "aclk_rga_niu", "aclk_rga_pre",	15, 13),
	GATE(ACLK_VIO1_NIU, "aclk_vio1_niu", "aclk_vio1",	15, 12),
	GATE(ACLK_VIO0_NIU, "aclk_vio0_niu", "aclk_vio0",	15, 11),
	GATE(HCLK_VIO_NIU, "hclk_vio_niu", "hclk_vio",		15, 10),
	GATE(HCLK_VIO_AHB_ARBI, "hclk_vio_ahb_arbi", "hclk_vio",15, 9),
	GATE(HCLK_VOP1, "hclk_vop1", "hclk_vio",		15, 8),
	GATE(ACLK_VOP1, "aclk_vop1", "aclk_vio1",		15, 7),
	GATE(HCLK_VOP0, "hclk_vop0", "hclk_vio",		15, 6),
	GATE(ACLK_VOP0, "aclk_vop0", "aclk_vio0",		15, 5),
/* 4 -  aclk_lcdc_iep */
	GATE(HCLK_IEP, "hclk_iep", "hclk_vio",			15, 3),
	GATE(ACLK_IEP, "aclk_iep", "aclk_vio0",			15, 2),
	GATE(HCLK_RGA, "hclk_rga", "hclk_vio",			15, 1),
	GATE(ACLK_RGA, "aclk_rga", "aclk_rga_pre",		15, 0),

	/* CRU_CLKGATE_CON16 */
	GATE(PCLK_VIO2_H2P, "pclk_vio2_h2p", "hclk_vio",	16, 11),
	GATE(HCLK_VIO2_H2P, "hclk_vio2_h2p", "hclk_vio",	16, 10),
	GATE(PCLK_HDMI_CTRL, "pclk_hdmi_ctrl", "hclk_vio",	16, 9),
	GATE(PCLK_EDP_CTRL, "pclk_edp_ctrl", "hclk_vio",	16, 8),
	GATE(PCLK_LVDS_PHY, "pclk_lvds_phy", "hclk_vio",	16, 7),
	GATE(PCLK_MIPI_CSI, "pclk_mipi_csi", "hclk_vio",	16, 6),
	GATE(PCLK_MIPI_DSI1, "pclk_mipi_dsi1", "hclk_vio",	16, 5),
	GATE(PCLK_MIPI_DSI0, "pclk_mipi_dsi0", "hclk_vio",	16, 4),
	GATE(PCLK_ISP_IN, "pclk_isp_in", "ext_isp",		16, 3),
	GATE(ACLK_ISP, "aclk_isp", "aclk_vio1",			16, 2),
	GATE(HCLK_ISP, "hclk_isp", "hclk_vio",			16, 1),
	GATE(0, "pclk_vip_in", "ext_vip",			16, 0),

	/* CRU_CLKGATE_CON17 */
	GATE(PCLK_GPIO0, "pclk_gpio0", "pclk_pd_pmu",		17, 4),
	GATE(PCLK_SGRF, "pclk_sgrf", "pclk_pd_pmu",		17, 3),
	GATE(0, "pclk_pmu_niu", "pclk_pd_pmu",			17, 2),
	GATE(0, "pclk_intmem1", "pclk_pd_pmu",			17, 1),
	GATE(PCLK_PMU, "pclk_pmu", "pclk_pd_pmu",		17, 0),

	/* CRU_CLKGATE_CON18 */
	GATE(ACLK_GPU, "aclk_gpu", "sclk_gpu",			18, 0),
};

/*
 * PLLs
 */
#define PLL_RATE_BA(_hz, _ref, _fb, _post, _ba)				\
{									\
	.freq = _hz,							\
	.refdiv = _ref,							\
	.fbdiv = _fb,							\
	.postdiv1 = _post,						\
	.bwadj = _ba,							\
}

#define PLL_RATE(_mhz, _ref, _fb, _post)				\
	 PLL_RATE_BA(_mhz, _ref, _fb, _post, ((_fb < 2) ? 1 : _fb >> 1))

static struct rk_clk_pll_rate rk3288_pll_rates[] = {
	PLL_RATE(   2208000000, 1,  92,  1),
	PLL_RATE(   2184000000, 1,  91,  1),
	PLL_RATE(   2160000000, 1,  90,  1),
	PLL_RATE(   2136000000, 1,  89,  1),
	PLL_RATE(   2112000000, 1,  88,  1),
	PLL_RATE(   2088000000, 1,  87,  1),
	PLL_RATE(   2064000000, 1,  86,  1),
	PLL_RATE(   2040000000, 1,  85,  1),
	PLL_RATE(   2016000000, 1,  84,  1),
	PLL_RATE(   1992000000, 1,  83,  1),
	PLL_RATE(   1968000000, 1,  82,  1),
	PLL_RATE(   1944000000, 1,  81,  1),
	PLL_RATE(   1920000000, 1,  80,  1),
	PLL_RATE(   1896000000, 1,  79,  1),
	PLL_RATE(   1872000000, 1,  78,  1),
	PLL_RATE(   1848000000, 1,  77,  1),
	PLL_RATE(   1824000000, 1,  76,  1),
	PLL_RATE(   1800000000, 1,  75,  1),
	PLL_RATE(   1776000000, 1,  74,  1),
	PLL_RATE(   1752000000, 1,  73,  1),
	PLL_RATE(   1728000000, 1,  72,  1),
	PLL_RATE(   1704000000, 1,  71,  1),
	PLL_RATE(   1680000000, 1,  70,  1),
	PLL_RATE(   1656000000, 1,  69,  1),
	PLL_RATE(   1632000000, 1,  68,  1),
	PLL_RATE(   1608000000, 1,  67,  1),
	PLL_RATE(   1560000000, 1,  65,  1),
	PLL_RATE(   1512000000, 1,  63,  1),
	PLL_RATE(   1488000000, 1,  62,  1),
	PLL_RATE(   1464000000, 1,  61,  1),
	PLL_RATE(   1440000000, 1,  60,  1),
	PLL_RATE(   1416000000, 1,  59,  1),
	PLL_RATE(   1392000000, 1,  58,  1),
	PLL_RATE(   1368000000, 1,  57,  1),
	PLL_RATE(   1344000000, 1,  56,  1),
	PLL_RATE(   1320000000, 1,  55,  1),
	PLL_RATE(   1296000000, 1,  54,  1),
	PLL_RATE(   1272000000, 1,  53,  1),
	PLL_RATE(   1248000000, 1,  52,  1),
	PLL_RATE(   1224000000, 1,  51,  1),
	PLL_RATE(   1200000000, 1,  50,  1),
	PLL_RATE(   1188000000, 2,  99,  1),
	PLL_RATE(   1176000000, 1,  49,  1),
	PLL_RATE(   1128000000, 1,  47,  1),
	PLL_RATE(   1104000000, 1,  46,  1),
	PLL_RATE(   1008000000, 1,  84,  2),
	PLL_RATE(    912000000, 1,  76,  2),
	PLL_RATE(    891000000, 8, 594,  2),
	PLL_RATE(    888000000, 1,  74,  2),
	PLL_RATE(    816000000, 1,  68,  2),
	PLL_RATE(    798000000, 2, 133,  2),
	PLL_RATE(    792000000, 1,  66,  2),
	PLL_RATE(    768000000, 1,  64,  2),
	PLL_RATE(    742500000, 8, 495,  2),
	PLL_RATE(    696000000, 1,  58,  2),
	PLL_RATE_BA( 621000000, 1, 207,  8, 1),
	PLL_RATE(    600000000, 1,  50,  2),
	PLL_RATE_BA( 594000000, 1, 198,  8, 1),
	PLL_RATE(    552000000, 1,  46,  2),
	PLL_RATE(    504000000, 1,  84,  4),
	PLL_RATE(    500000000, 3, 125,  2),
	PLL_RATE(    456000000, 1,  76,  4),
	PLL_RATE(    428000000, 1, 107,  6),
	PLL_RATE(    408000000, 1,  68,  4),
	PLL_RATE(    400000000, 3, 100,  2),
	PLL_RATE_BA( 394000000, 1, 197, 12, 1),
	PLL_RATE(    384000000, 2, 128,  4),
	PLL_RATE(    360000000, 1,  60,  4),
	PLL_RATE_BA( 356000000, 1, 178, 12, 1),
	PLL_RATE_BA( 324000000, 1, 189, 14, 1),
	PLL_RATE(    312000000, 1,  52,  4),
	PLL_RATE_BA( 308000000, 1, 154, 12, 1),
	PLL_RATE_BA( 303000000, 1, 202, 16, 1),
	PLL_RATE(    300000000, 1,  75,  6),
	PLL_RATE_BA( 297750000, 2, 397, 16, 1),
	PLL_RATE_BA( 293250000, 2, 391, 16, 1),
	PLL_RATE_BA( 292500000, 1, 195, 16, 1),
	PLL_RATE(    273600000, 1, 114, 10),
	PLL_RATE_BA( 273000000, 1, 182, 16, 1),
	PLL_RATE_BA( 270000000, 1, 180, 16, 1),
	PLL_RATE_BA( 266250000, 2, 355, 16, 1),
	PLL_RATE_BA( 256500000, 1, 171, 16, 1),
	PLL_RATE(    252000000, 1,  84,  8),
	PLL_RATE_BA( 250500000, 1, 167, 16, 1),
	PLL_RATE_BA( 243428571, 1, 142, 14, 1),
	PLL_RATE(    238000000, 1, 119, 12),
	PLL_RATE_BA( 219750000, 2, 293, 16, 1),
	PLL_RATE_BA( 216000000, 1, 144, 16, 1),
	PLL_RATE_BA( 213000000, 1, 142, 16, 1),
	PLL_RATE(    195428571, 1, 114, 14),
	PLL_RATE(    160000000, 1,  80, 12),
	PLL_RATE(    157500000, 1, 105, 16),
	PLL_RATE(    126000000, 1,  84, 16),
	PLL_RATE(     48000000, 1,  64, 32),
	{},
};

static struct rk_clk_armclk_rates rk3288_armclk_rates[] = {
	{ 1800000000, 1},
	{ 1704000000, 1},
	{ 1608000000, 1},
	{ 1512000000, 1},
	{ 1416000000, 1},
	{ 1200000000, 1},
	{ 1008000000, 1},
	{  816000000, 1},
	{  696000000, 1},
	{  600000000, 1},
	{  408000000, 1},
	{  312000000, 1},
	{  216000000, 1},
	{  126000000, 1},
};

/* Standard PLL. */
#define PLL(_id, _name, _base, _shift)					\
{									\
	.type = RK3066_CLK_PLL,						\
	.clk.pll = &(struct rk_clk_pll_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = pll_src_p,			\
		.clkdef.parent_cnt = nitems(pll_src_p),		\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.base_offset = _base,					\
		.mode_reg = CRU_MODE_CON,				\
		.mode_shift = _shift,					\
		.rates = rk3288_pll_rates,				\
	},								\
}

#define ARMDIV(_id, _name, _pn, _r, _o, _ds, _dw, _ms, _mw, _mp, _ap)	\
{									\
	.type = RK_CLK_ARMCLK,						\
	.clk.armclk = &(struct rk_clk_armclk_def) {			\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pn,				\
		.clkdef.parent_cnt = nitems(_pn),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = CRU_CLKSEL_CON(_o),			\
		.mux_shift = _ms,					\
		.mux_width = _mw,					\
		.div_shift = _ds,					\
		.div_width = _dw,					\
		.main_parent = _mp,					\
		.alt_parent = _ap,					\
		.rates = _r,						\
		.nrates = nitems(_r),					\
	},								\
}

PLIST(pll_src_p) = {"xin24m", "xin24m", "xin32k"};
PLIST(armclk_p)= {"apll_core", "gpll_core"};
PLIST(ddrphy_p) = {"dpll_ddr", "gpll_ddr"};
PLIST(aclk_cpu_p) = {"cpll_aclk_cpu", "gpll_aclk_cpu"};

PLIST(cpll_gpll_p) = {"cpll", "gpll"};
PLIST(npll_cpll_gpll_p) = {"npll", "cpll", "gpll"};
PLIST(cpll_gpll_npll_p) = {"cpll", "gpll", "npll"};
PLIST(cpll_gpll_usb480m_p)= {"cpll", "gpll", "usbphy480m_src"};
PLIST(cpll_gpll_usb480m_npll_p) = {"cpll", "gpll", "usbphy480m_src", "npll"};

PLIST(mmc_p) = {"cpll", "gpll", "xin24m", "xin24m"};
PLIST(i2s_pre_p) = {"i2s_src", "i2s_frac", "ext_i2s", "xin12m"};
PLIST(i2s_clkout_p) = {"i2s_pre", "xin12m"};
PLIST(spdif_p) = {"spdif_pre", "spdif_frac", "xin12m"};
PLIST(spdif_8ch_p) = {"spdif_8ch_pre", "spdif_8ch_frac", "xin12m"};
PLIST(uart0_p) = {"uart0_src", "uart0_frac", "xin24m"};
PLIST(uart1_p) = {"uart1_src", "uart1_frac", "xin24m"};
PLIST(uart2_p) = {"uart2_src", "uart2_frac", "xin24m"};
PLIST(uart3_p) = {"uart3_src", "uart3_frac", "xin24m"};
PLIST(uart4_p) = {"uart4_src", "uart4_frac", "xin24m"};
PLIST(vip_out_p) = {"vip_src", "xin24m"};
PLIST(mac_p) = {"mac_pll_src", "ext_gmac"};
PLIST(hsadcout_p) = {"hsadc_src", "ext_hsadc"};
PLIST(edp_24m_p) = {"ext_edp_24m", "xin24m"};
PLIST(tspout_p) = {"cpll", "gpll", "npll", "xin27m"};
PLIST(wifi_p) = {"cpll", "gpll"};
PLIST(usbphy480m_p) = {"sclk_otgphy1_480m", "sclk_otgphy2_480m", "sclk_otgphy0_480m"};

/* PLIST(aclk_vcodec_pre_p) = {"aclk_vepu", "aclk_vdpu"}; */


static struct rk_clk rk3288_clks[] = {
	/* External clocks */
	LINK("xin24m"),
	FRATE(0, "xin32k", 32000),
	FRATE(0, "xin27m", 27000000),
	FRATE(0, "ext_hsadc", 0),
	FRATE(0, "ext_jtag", 0),
	FRATE(0, "ext_isp", 0),
	FRATE(0, "ext_vip", 0),
	FRATE(0, "ext_i2s", 0),
	FRATE(0, "ext_edp_24m", 0),

	FRATE(0, "sclk_otgphy0_480m", 0),
	FRATE(0, "sclk_otgphy1_480m", 0),
	FRATE(0, "sclk_otgphy2_480m", 0),

	FRATE(0, "aclk_vcodec_pre", 0),

	/* Fixed dividers */
	FFACT(0, "xin12m", "xin24m", 1, 2),
	FFACT(0, "hclk_vcodec_pre_s", "aclk_vcodec_pre", 1, 4),

	PLL(PLL_APLL, "apll", CRU_APLL_CON(0), 0),
	PLL(PLL_DPLL, "dpll", CRU_DPLL_CON(0), 4),
	PLL(PLL_CPLL, "cpll", CRU_CPLL_CON(0), 8),
	PLL(PLL_GPLL, "gpll", CRU_GPLL_CON(0), 12),
	PLL(PLL_NPLL, "npll", CRU_NPLL_CON(0), 14),

	/* CRU_CLKSEL0_CON */
	ARMDIV(ARMCLK, "armclk", armclk_p, rk3288_armclk_rates,
	    0, 8, 5,	15, 1, 0, 1),
	CDIV(0, "aclk_core_mp_s", "armclk", 0,
	    0, 4, 4),
	CDIV(0, "aclk_core_m0_s", "armclk", 0,
	    0, 0, 4),

	/* CRU_CLKSEL1_CON */
	CDIV(0, "pclk_cpu_s", "aclk_cpu_pre", 0,
	    1, 12, 3),
	CDIV(0, "hclk_cpu_s", "aclk_cpu_pre", RK_CLK_COMPOSITE_DIV_EXP,
	    1, 8, 2),
	COMP(0, "aclk_cpu_src", aclk_cpu_p, 0,
	    1, 3, 5,	15, 1),
	CDIV(0, "aclk_cpu_pre", "aclk_cpu_src", 0,
	    1, 0, 3),

	/* CRU_CLKSEL2_CON */
/* 12:8 testout_div */
	CDIV(0, "sclk_tsadc_s", "xin32k", 0,
	    2, 0, 6),

	/* CRU_CLKSEL3_CON */
	MUX(SCLK_UART4, "sclk_uart4", uart4_p, 0,
	    3,		8, 2),
	CDIV(0, "uart4_src_s", "uart_src", 0,
	    3, 0, 7),

	/* CRU_CLKSEL4_CON */
	MUX(0, "i2s_pre", i2s_pre_p,  0,
	    4,		8, 2),
	MUX(0, "i2s0_clkout_s", i2s_clkout_p, 0,
	    4,		12, 1),
	COMP(0, "i2s_src_s", cpll_gpll_p, 0,
	    4, 0, 7,	15, 1),

	/* CRU_CLKSEL5_CON */
	MUX(0, "spdif_src", cpll_gpll_p, 0,
	    5,		15, 1),
	MUX(0, "spdif_mux", spdif_p, 0,
	    5,		8, 2),
	CDIV(0, "spdif_pre_s", "spdif_src", 0,
	    5, 0, 7),

	/* CRU_CLKSEL6_CON */
	COMP(0, "sclk_isp_jpe_s", cpll_gpll_npll_p, 0,
	    6, 8, 6,	14, 2),
	COMP(0, "sclk_isp_s", cpll_gpll_npll_p, 0,
	    6, 0, 6,	6, 2),

	/* CRU_CLKSEL7_CON */
	FRACT(0, "uart4_frac_s", "uart4_src", 0,
	    7),

	/* CRU_CLKSEL8_CON */
	FRACT(0, "i2s_frac_s", "i2s_src", 0,
	    8),

	/* CRU_CLKSEL9_CON */
	FRACT(0, "spdif_frac_s", "spdif_src", 0,
	    9),

	/* CRU_CLKSEL10_CON */
	CDIV(0, "pclk_peri_s", "aclk_peri_src", RK_CLK_COMPOSITE_DIV_EXP,
	    10, 12, 2),
	CDIV(0, "hclk_peri_s", "aclk_peri_src", RK_CLK_COMPOSITE_DIV_EXP,
	    10, 8, 2),
	COMP(0, "aclk_peri_src_s", cpll_gpll_p, 0,
	    10, 0, 5,	15, 1),

	/* CRU_CLKSEL11_CON */
	COMP(0, "sclk_sdmmc_s", mmc_p, 0,
	    11, 0, 6,	6, 2),

	/* CRU_CLKSEL12_CON */
	COMP(0, "sclk_emmc_s", mmc_p, 0,
	    12, 8, 6,	14, 2),
	COMP(0, "sclk_sdio0_s", mmc_p, 0,
	    12, 0, 6,	6, 2),

	/* CRU_CLKSEL13_CON */
	MUX(0, "uart_src", cpll_gpll_p, 0,
	    13,		15, 1),
	MUX(0, "usbphy480m_src_s", usbphy480m_p, 0,
	    13,		11, 2),
	MUX(SCLK_UART0, "sclk_uart0", uart0_p, 0,
	    13,		8, 2),
	COMP(0, "uart0_src_s", cpll_gpll_usb480m_npll_p, 0,
	    13, 0, 7, 	13, 2),

	/* CRU_CLKSEL14_CON */
	MUX(SCLK_UART1, "sclk_uart1", uart1_p, 0,
	    14,		8, 2),
	CDIV(0, "uart1_src_s", "uart_src", 0,
	    14, 0, 7),


	/* CRU_CLKSEL15_CON */
	MUX(SCLK_UART2, "sclk_uart2", uart2_p, 0,
	    15, 	8, 2),
	CDIV(0, "uart2_src_s", "uart_src", 0,
	    15, 0, 7),

	/* CRU_CLKSEL16_CON */
	MUX(SCLK_UART3, "sclk_uart3", uart3_p, 0,
	    16,		8, 2),
	CDIV(0, "uart3_src_s", "uart_src", 0,
	    16, 0, 7),

	/* CRU_CLKSEL17_CON */
	FRACT(0, "uart0_frac_s", "uart0_src", 0,
	    17),

	/* CRU_CLKSEL18_CON */
	FRACT(0, "uart1_frac_s", "uart1_src", 0,
	    18),

	/* CRU_CLKSEL19_CON */
	FRACT(0, "uart2_frac_s", "uart2_src", 0,
	    19),

	/* CRU_CLKSEL20_CON */
	FRACT(0, "uart3_frac_s", "uart3_src", 0,
	    20),

	/* CRU_CLKSEL21_CON */
	COMP(0, "mac_pll_src_s", npll_cpll_gpll_p, 0,
	    21, 8, 5,	0, 2),
	MUX(SCLK_MAC, "mac_clk", mac_p, 0,
	    21, 4, 1),

	/* CRU_CLKSEL22_CON */
	MUX(0, "sclk_hsadc_out", hsadcout_p, 0,
	    22,		4, 1),
	COMP(0, "hsadc_src_s", cpll_gpll_p, 0,
	    22, 8, 8,	0, 1),
	MUX(0, "wifi_src", wifi_p, 0,
	    22,		1, 1),
/* 7 - inverter "sclk_hsadc", "sclk_hsadc_out" */

	/* CRU_CLKSEL23_CON */
	FRACT(0, "wifi_frac_s", "wifi_src", 0,
	    23),

	/* CRU_CLKSEL24_CON */
	CDIV(0, "sclk_saradc_s", "xin24m", 0,
	    24, 8, 8),

	/* CRU_CLKSEL25_CON */
	COMP(0, "sclk_spi1_s", cpll_gpll_p, 0,
	    25, 8, 7, 	15, 1),
	COMP(0, "sclk_spi0_s", cpll_gpll_p, 0,
	    25, 0, 7, 	7, 1),

	/* CRU_CLKSEL26_CON */
	COMP(SCLK_VIP_OUT, "sclk_vip_out", vip_out_p, 0,
	    26, 9, 5,	15, 1),
	MUX(0, "vip_src_s", cpll_gpll_p, 0,
	    26, 8, 1),
	CDIV(0, "crypto_s", "aclk_cpu_pre", 0,
	    26, 6, 2),
	COMP(0, "ddrphy", ddrphy_p, RK_CLK_COMPOSITE_DIV_EXP,
	    26, 0, 2,	2, 1),

	/* CRU_CLKSEL27_CON */
	COMP(0, "dclk_vop0_s", cpll_gpll_npll_p, 0,
	    27, 8, 8,	0, 2),

	MUX(0, "sclk_edp_24m_s", edp_24m_p, 0,
	    28, 15, 1),
	CDIV(0, "hclk_vio", "aclk_vio0", 0,
	    28, 8, 5),
	COMP(0, "sclk_edp_s", cpll_gpll_npll_p, 0,
	    28, 0, 6,	6, 2),

	/* CRU_CLKSEL29_CON */
	COMP(0, "dclk_vop1_s", cpll_gpll_npll_p, 0,
	   29, 8, 8,	6, 2),
/* 4 - inverter "pclk_vip"  "pclk_vip_in" */
/* 3 - inverter "pclk_isp", "pclk_isp_in" */

	/* CRU_CLKSEL30_CON */
	COMP(0, "sclk_rga_s", cpll_gpll_usb480m_p, 0,
	    30, 8, 5,	14, 2),
	COMP(0, "aclk_rga_pre_s", cpll_gpll_usb480m_p, 0,
	    30, 0, 5,	6, 2),

	/* CRU_CLKSEL31_CON */
	COMP(0, "aclk_vio1_s", cpll_gpll_usb480m_p, 0,
	    31, 8, 5,	14, 2),
	COMP(0, "aclk_vio0_s", cpll_gpll_usb480m_p, 0,
	    31, 0, 5, 	6, 2),

	/* CRU_CLKSEL32_CON */
	COMP(0, "aclk_vdpu_s", cpll_gpll_usb480m_p, 0,
	    32, 8, 5,	14, 2),
	COMP(0, "aclk_vepu_s", cpll_gpll_usb480m_p, 0,
	    32, 0, 5,	6, 2),

	/* CRU_CLKSEL33_CON */
	CDIV(0, "pclk_pd_alive", "gpll", 0,
	    33, 8, 5),
	CDIV(0, "pclk_pd_pmu_s", "gpll", 0,
	    33, 0, 5),

	/* CRU_CLKSEL34_CON */
	COMP(0, "sclk_sdio1_s", mmc_p, 0,
	    34, 8, 6,	14, 2),
	COMP(0, "sclk_gpu_s", cpll_gpll_usb480m_npll_p, 0,
	    34, 0, 5,	6, 2),

	/* CRU_CLKSEL35_CON */
	COMP(0, "sclk_tspout_s", tspout_p, 0,
	    35, 8, 5,	14, 2),
	COMP(0, "sclk_tsp_s", cpll_gpll_npll_p, 0,
	    35, 0, 5,	6, 2),

	/* CRU_CLKSEL36_CON */
	CDIV(0, "armcore3_s", "armclk", 0,
	    36, 12, 3),
	CDIV(0, "armcore2_s", "armclk", 0,
	    36, 8, 3),
	CDIV(0, "armcore1_s", "armclk", 0,
	    36, 4, 3),
	CDIV(0, "armcore0_s", "armclk", 0,
	    36, 0, 3),

	/* CRU_CLKSEL37_CON */
	CDIV(0, "pclk_dbg_pre_s", "armclk", 0,
	    37, 9, 5),
	CDIV(0, "atclk_s", "armclk", 0,
	    37, 4, 5),
	CDIV(0, "l2ram_s", "armclk", 0,
	    37, 0, 3),

	/* CRU_CLKSEL38_CON */
	COMP(0, "sclk_nandc1_s", cpll_gpll_p, 0,
	    38, 8, 5,	15, 1),
	COMP(0, "sclk_nandc0_s", cpll_gpll_p, 0,
	    38, 0, 5,	7, 1),

	/* CRU_CLKSEL39_CON */
	COMP(0, "aclk_hevc_s", cpll_gpll_npll_p, 0,
	    39, 8, 5,	14, 2),
	COMP(0, "sclk_spi2_s", cpll_gpll_p, 0,
	    39, 0, 7,	7, 1),

	/* CRU_CLKSEL40_CON */
	CDIV(HCLK_HEVC, "hclk_hevc", "aclk_hevc", 0,
	    40, 12, 2),
	MUX(0, "spdif_8ch_mux", spdif_8ch_p, 0,
	    40,		8, 2),
	CDIV(0, "spdif_8ch_pre_s", "spdif_src", 0,
	    40, 0, 7),

	/* CRU_CLKSEL41_CON */
	FRACT(0, "spdif_8ch_frac_s", "spdif_8ch_pre", 0,
	    41),

	/* CRU_CLKSEL42_CON */
	COMP(0, "sclk_hevc_core_s", cpll_gpll_npll_p, 0,
	    42, 8, 5,	14, 2),
	COMP(0, "sclk_hevc_cabac_s", cpll_gpll_npll_p, 0,
	    42, 0, 5,	6, 2),
/*
 *  not yet implemented MMC clocks
 *		id		name		src		reg
 *	SCLK_SDMMC_DRV,    "sdmmc_drv",    "sclk_sdmmc", RK3288_SDMMC_CON0
 *	SCLK_SDMMC_SAMPLE, "sdmmc_sample", "sclk_sdmmc", RK3288_SDMMC_CON1,

 *	SCLK_SDIO0_DRV,    "sdio0_drv",    "sclk_sdio0", RK3288_SDIO0_CON0, 1),
 *	SCLK_SDIO0_SAMPLE, "sdio0_sample", "sclk_sdio0", RK3288_SDIO0_CON1, 0),

 *	SCLK_SDIO1_DRV,    "sdio1_drv",    "sclk_sdio1", RK3288_SDIO1_CON0, 1),
 *	SCLK_SDIO1_SAMPLE, "sdio1_sample", "sclk_sdio1", RK3288_SDIO1_CON1, 0),

 *	SCLK_EMMC_DRV,     "emmc_drv",     "sclk_emmc",  RK3288_EMMC_CON0,  1),
 *	SCLK_EMMC_SAMPLE,  "emmc_sample",  "sclk_emmc",  RK3288_EMMC_CON1,  0),
 *
 * and GFR based mux for "aclk_vcodec_pre"
 */

};

static int
rk3288_cru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3288-cru")) {
		device_set_desc(dev, "Rockchip RK3288 Clock and Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
rk3288_cru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->gates = rk3288_gates;
	sc->ngates = nitems(rk3288_gates);

	sc->clks = rk3288_clks;
	sc->nclks = nitems(rk3288_clks);

	sc->reset_num = CRU_SOFTRST_SIZE * 16;
	sc->reset_offset = CRU_SOFTRST_CON(0);

	return (rk_cru_attach(dev));
}

static device_method_t rk3288_cru_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3288_cru_probe),
	DEVMETHOD(device_attach,	rk3288_cru_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3288_cru, rk3288_cru_driver, rk3288_cru_methods,
  sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3288_cru, simplebus, rk3288_cru_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 1);
