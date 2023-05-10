/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Emmanuel Vadot <manu@freebsd.org>
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

#define	CRU_CLKSEL_CON(x)	(0x100 + (x) * 0x4)

/* Registers */
#define	RK3328_GRF_SOC_CON4	0x410
#define	RK3328_GRF_MAC_CON1	0x904
#define	RK3328_GRF_MAC_CON2	0x908

/* Exported clocks */

#define	PLL_APLL		1
#define	PLL_DPLL		2
#define	PLL_CPLL		3
#define	PLL_GPLL		4
#define	PLL_NPLL		5
#define	ARMCLK			6

/* SCLK */
#define	SCLK_RTC32K		30
#define	SCLK_SDMMC_EXT		31
#define	SCLK_SPI		32
#define	SCLK_SDMMC		33
#define	SCLK_SDIO		34
#define	SCLK_EMMC		35
#define	SCLK_TSADC		36
#define	SCLK_SARADC		37
#define	SCLK_UART0		38
#define	SCLK_UART1		39
#define	SCLK_UART2		40
#define	SCLK_I2S0		41
#define	SCLK_I2S1		42
#define	SCLK_I2S2		43
#define	SCLK_I2S1_OUT		44
#define	SCLK_I2S2_OUT		45
#define	SCLK_SPDIF		46
#define	SCLK_TIMER0		47
#define	SCLK_TIMER1		48
#define	SCLK_TIMER2		49
#define	SCLK_TIMER3		50
#define	SCLK_TIMER4		51
#define	SCLK_TIMER5		52
#define	SCLK_WIFI		53
#define	SCLK_CIF_OUT		54
#define	SCLK_I2C0		55
#define	SCLK_I2C1		56
#define	SCLK_I2C2		57
#define	SCLK_I2C3		58
#define	SCLK_CRYPTO		59
#define	SCLK_PWM		60
#define	SCLK_PDM		61
#define	SCLK_EFUSE		62
#define	SCLK_OTP		63
#define	SCLK_DDRCLK		64
#define	SCLK_VDEC_CABAC		65
#define	SCLK_VDEC_CORE		66
#define	SCLK_VENC_DSP		67
#define	SCLK_VENC_CORE		68
#define	SCLK_RGA		69
#define	SCLK_HDMI_SFC		70
#define	SCLK_HDMI_CEC		71	/* Unused ? */
#define	SCLK_USB3_REF		72
#define	SCLK_USB3_SUSPEND	73
#define	SCLK_SDMMC_DRV		74
#define	SCLK_SDIO_DRV		75
#define	SCLK_EMMC_DRV		76
#define	SCLK_SDMMC_EXT_DRV	77
#define	SCLK_SDMMC_SAMPLE	78
#define	SCLK_SDIO_SAMPLE	79
#define	SCLK_EMMC_SAMPLE	80
#define	SCLK_SDMMC_EXT_SAMPLE	81
#define	SCLK_VOP		82
#define	SCLK_MAC2PHY_RXTX	83
#define	SCLK_MAC2PHY_SRC	84
#define	SCLK_MAC2PHY_REF	85
#define	SCLK_MAC2PHY_OUT	86
#define	SCLK_MAC2IO_RX		87
#define	SCLK_MAC2IO_TX		88
#define	SCLK_MAC2IO_REFOUT	89
#define	SCLK_MAC2IO_REF		90
#define	SCLK_MAC2IO_OUT		91
#define	SCLK_TSP		92
#define	SCLK_HSADC_TSP		93
#define	SCLK_USB3PHY_REF	94
#define	SCLK_REF_USB3OTG	95
#define	SCLK_USB3OTG_REF	96
#define	SCLK_USB3OTG_SUSPEND	97
#define	SCLK_REF_USB3OTG_SRC	98
#define	SCLK_MAC2IO_SRC		99
#define	SCLK_MAC2IO		100
#define	SCLK_MAC2PHY		101
#define	SCLK_MAC2IO_EXT		102

/* DCLK */
#define	DCLK_LCDC		120
#define	DCLK_HDMIPHY		121
#define	HDMIPHY			122
#define	USB480M			123
#define	DCLK_LCDC_SRC		124

/* ACLK */
#define	ACLK_AXISRAM		130	/* Unused */
#define	ACLK_VOP_PRE		131
#define	ACLK_USB3OTG		132
#define	ACLK_RGA_PRE		133
#define	ACLK_DMAC		134	/* Unused */
#define	ACLK_GPU		135
#define	ACLK_BUS_PRE		136
#define	ACLK_PERI_PRE		137
#define	ACLK_RKVDEC_PRE		138
#define	ACLK_RKVDEC		139
#define	ACLK_RKVENC		140
#define	ACLK_VPU_PRE		141
#define	ACLK_VIO_PRE		142
#define	ACLK_VPU		143
#define	ACLK_VIO		144
#define	ACLK_VOP		145
#define	ACLK_GMAC		146
#define	ACLK_H265		147
#define	ACLK_H264		148
#define	ACLK_MAC2PHY		149
#define	ACLK_MAC2IO		150
#define	ACLK_DCF		151
#define	ACLK_TSP		152
#define	ACLK_PERI		153
#define	ACLK_RGA		154
#define	ACLK_IEP		155
#define	ACLK_CIF		156
#define	ACLK_HDCP		157

/* PCLK */
#define	PCLK_GPIO0		200
#define	PCLK_GPIO1		201
#define	PCLK_GPIO2		202
#define	PCLK_GPIO3		203
#define	PCLK_GRF		204
#define	PCLK_I2C0		205
#define	PCLK_I2C1		206
#define	PCLK_I2C2		207
#define	PCLK_I2C3		208
#define	PCLK_SPI		209
#define	PCLK_UART0		210
#define	PCLK_UART1		211
#define	PCLK_UART2		212
#define	PCLK_TSADC		213
#define	PCLK_PWM		214
#define	PCLK_TIMER		215
#define	PCLK_BUS_PRE		216
#define	PCLK_PERI_PRE		217	/* Unused */
#define	PCLK_HDMI_CTRL		218	/* Unused */
#define	PCLK_HDMI_PHY		219	/* Unused */
#define	PCLK_GMAC		220
#define	PCLK_H265		221
#define	PCLK_MAC2PHY		222
#define	PCLK_MAC2IO		223
#define	PCLK_USB3PHY_OTG	224
#define	PCLK_USB3PHY_PIPE	225
#define	PCLK_USB3_GRF		226
#define	PCLK_USB2_GRF		227
#define	PCLK_HDMIPHY		228
#define	PCLK_DDR		229
#define	PCLK_PERI		230
#define	PCLK_HDMI		231
#define	PCLK_HDCP		232
#define	PCLK_DCF		233
#define	PCLK_SARADC		234
#define	PCLK_ACODECPHY		235
#define	PCLK_WDT		236	/* Controlled from the secure GRF */

/* HCLK */
#define	HCLK_PERI		308
#define	HCLK_TSP		309
#define	HCLK_GMAC		310	/* Unused */
#define	HCLK_I2S0_8CH		311
#define	HCLK_I2S1_8CH		312
#define	HCLK_I2S2_2CH		313
#define	HCLK_SPDIF_8CH		314
#define	HCLK_VOP		315
#define	HCLK_NANDC		316	/* Unused */
#define	HCLK_SDMMC		317
#define	HCLK_SDIO		318
#define	HCLK_EMMC		319
#define	HCLK_SDMMC_EXT		320
#define	HCLK_RKVDEC_PRE		321
#define	HCLK_RKVDEC		322
#define	HCLK_RKVENC		323
#define	HCLK_VPU_PRE		324
#define	HCLK_VIO_PRE		325
#define	HCLK_VPU		326
/* 327 doesn't exists */
#define	HCLK_BUS_PRE		328
#define	HCLK_PERI_PRE		329	/* Unused */
#define	HCLK_H264		330
#define	HCLK_CIF		331
#define	HCLK_OTG_PMU		332
#define	HCLK_OTG		333
#define	HCLK_HOST0		334
#define	HCLK_HOST0_ARB		335
#define	HCLK_CRYPTO_MST		336
#define	HCLK_CRYPTO_SLV		337
#define	HCLK_PDM		338
#define	HCLK_IEP		339
#define	HCLK_RGA		340
#define	HCLK_HDCP		341

static struct rk_cru_gate rk3328_gates[] = {
	/* CRU_CLKGATE_CON0 */
	CRU_GATE(0, "core_apll_clk", "apll", 0x200, 0)
	CRU_GATE(0, "core_dpll_clk", "dpll", 0x200, 1)
	CRU_GATE(0, "core_gpll_clk", "gpll", 0x200, 2)
	/* Bit 3 bus_src_clk_en */
	/* Bit 4 clk_ddrphy_src_en */
	/* Bit 5 clk_ddrpd_src_en */
	/* Bit 6 clk_ddrmon_en */
	/* Bit 7-8 unused */
	/* Bit 9 testclk_en */
	CRU_GATE(SCLK_WIFI, "sclk_wifi", "sclk_wifi_c", 0x200, 10)
	CRU_GATE(SCLK_RTC32K, "clk_rtc32k", "clk_rtc32k_c", 0x200, 11)
	CRU_GATE(0, "core_npll_clk", "npll", 0x200, 12)
	/* Bit 13-15 unused */

	/* CRU_CLKGATE_CON1 */
	/* Bit 0 unused */
	CRU_GATE(0, "clk_i2s0_div", "clk_i2s0_div_c", 0x204, 1)
	CRU_GATE(0, "clk_i2s0_frac", "clk_i2s0_frac_f", 0x204, 2)
	CRU_GATE(SCLK_I2S0, "clk_i2s0", "clk_i2s0_mux", 0x204, 3)
	CRU_GATE(0, "clk_i2s1_div", "clk_i2s1_div_c", 0x204, 4)
	CRU_GATE(0, "clk_i2s1_frac", "clk_i2s1_frac_f", 0x204, 5)
	CRU_GATE(SCLK_I2S1, "clk_i2s1", "clk_i2s1_mux", 0x204, 6)
	CRU_GATE(0, "clk_i2s1_out", "clk_i2s1_mux", 0x204, 7)
	CRU_GATE(0, "clk_i2s2_div", "clk_i2s2_div_c", 0x204, 8)
	CRU_GATE(0, "clk_i2s2_frac", "clk_i2s2_frac_f", 0x204, 9)
	CRU_GATE(SCLK_I2S2, "clk_i2s2", "clk_i2s2_mux", 0x204, 10)
	CRU_GATE(0, "clk_i2s2_out", "clk_i2s2_mux", 0x204, 11)
	CRU_GATE(0, "clk_spdif_div", "clk_spdif_div_c", 0x204, 12)
	CRU_GATE(0, "clk_spdif_frac", "clk_spdif_frac_f", 0x204, 13)
	CRU_GATE(0, "clk_uart0_div", "clk_uart0_div_c", 0x204, 14)
	CRU_GATE(0, "clk_uart0_frac", "clk_uart0_frac_f", 0x204, 15)

	/* CRU_CLKGATE_CON2 */
	CRU_GATE(0, "clk_uart1_div", "clk_uart1_div_c", 0x208, 0)
	CRU_GATE(0, "clk_uart1_frac", "clk_uart1_frac_f", 0x208, 1)
	CRU_GATE(0, "clk_uart2_div", "clk_uart2_div_c", 0x208, 2)
	CRU_GATE(0, "clk_uart2_frac", "clk_uart2_frac_f", 0x208, 3)
	CRU_GATE(SCLK_CRYPTO, "clk_crypto", "clk_crypto_c", 0x208, 4)
	CRU_GATE(SCLK_TSP, "clk_tsp", "clk_tsp_c", 0x208, 5)
	CRU_GATE(SCLK_TSADC, "clk_tsadc_src", "clk_tsadc_c", 0x208, 6)
	CRU_GATE(SCLK_SPI, "clk_spi", "clk_spi_c", 0x208, 7)
	CRU_GATE(SCLK_PWM, "clk_pwm", "clk_pwm_c", 0x208, 8)
	CRU_GATE(SCLK_I2C0, "clk_i2c0_src", "clk_i2c0_c", 0x208, 9)
	CRU_GATE(SCLK_I2C1, "clk_i2c1_src", "clk_i2c1_c", 0x208, 10)
	CRU_GATE(SCLK_I2C2, "clk_i2c2_src", "clk_i2c2_c", 0x208, 11)
	CRU_GATE(SCLK_I2C3, "clk_i2c3_src", "clk_i2c3_c", 0x208, 12)
	CRU_GATE(SCLK_EFUSE, "clk_efuse", "clk_efuse_c", 0x208, 13)
	CRU_GATE(SCLK_SARADC, "clk_saradc", "clk_saradc_c", 0x208, 14)
	CRU_GATE(SCLK_PDM, "clk_pdm", "clk_pdm_c", 0x208, 15)

	/* CRU_CLKGATE_CON3 */
	CRU_GATE(SCLK_MAC2PHY_SRC, "clk_mac2phy_src", "clk_mac2phy_src_c", 0x20c, 0)
	CRU_GATE(SCLK_MAC2IO_SRC, "clk_mac2io_src", "clk_mac2io_src_c", 0x20c, 1)
	CRU_GATE(ACLK_GMAC, "aclk_gmac", "aclk_gmac_c", 0x20c, 2)
	/* Bit 3 gmac_gpll_src_en Unused ? */
	/* Bit 4 gmac_vpll_src_en Unused ? */
	CRU_GATE(SCLK_MAC2IO_OUT, "clk_mac2io_out", "clk_mac2io_out_c", 0x20c, 5)
	/* Bit 6-7 unused */
	CRU_GATE(SCLK_OTP, "clk_otp", "clk_otp_c", 0x20c, 8)
	/* Bit 9-15 unused */

	/* CRU_CLKGATE_CON4 */
	CRU_GATE(0, "periph_gclk_src", "gpll", 0x210, 0)
	CRU_GATE(0, "periph_cclk_src", "cpll", 0x210, 1)
	CRU_GATE(0, "hdmiphy_peri", "hdmiphy", 0x210, 2)
	CRU_GATE(SCLK_SDMMC, "clk_mmc0_src", "clk_sdmmc_c", 0x210, 3)
	CRU_GATE(SCLK_SDIO, "clk_sdio_src", "clk_sdio_c", 0x210, 4)
	CRU_GATE(SCLK_EMMC, "clk_emmc_src", "clk_emmc_c", 0x210, 5)
	CRU_GATE(SCLK_REF_USB3OTG_SRC, "clk_ref_usb3otg_src", "clk_ref_usb3otg_src_c", 0x210, 6)
	CRU_GATE(SCLK_USB3OTG_REF, "clk_usb3_otg0_ref", "xin24m", 0x210, 7)
	CRU_GATE(SCLK_USB3OTG_SUSPEND, "clk_usb3otg_suspend", "clk_usb3otg_suspend_c", 0x210, 8)
	/* Bit 9 clk_usb3phy_ref_25m_en */
	CRU_GATE(SCLK_SDMMC_EXT, "clk_sdmmc_ext", "clk_sdmmc_ext_c", 0x210, 10)
	/* Bit 11-15 unused */

	/* CRU_CLKGATE_CON5 */
	CRU_GATE(ACLK_RGA_PRE, "aclk_rga_pre", "aclk_rga_pre_c", 0x214, 0)
	CRU_GATE(SCLK_RGA, "sclk_rga", "sclk_rga_c", 0x214, 0)
	CRU_GATE(ACLK_VIO_PRE, "aclk_vio_pre", "aclk_vio_pre_c", 0x214, 2)
	CRU_GATE(SCLK_CIF_OUT, "clk_cif_src", "clk_cif_src_c", 0x214, 3)
	CRU_GATE(SCLK_HDMI_SFC, "clk_hdmi_sfc", "xin24m", 0x214, 4)
	CRU_GATE(ACLK_VOP_PRE, "aclk_vop_pre", "aclk_vop_pre_c", 0x214, 5)
	CRU_GATE(DCLK_LCDC_SRC, "vop_dclk_src", "vop_dclk_src_c", 0x214, 6)
	/* Bit 7-15 unused */

	/* CRU_CLKGATE_CON6 */
	CRU_GATE(ACLK_RKVDEC_PRE, "aclk_rkvdec_pre", "aclk_rkvdec_c", 0x218, 0)
	CRU_GATE(SCLK_VDEC_CABAC, "sclk_cabac", "sclk_cabac_c", 0x218, 1)
	CRU_GATE(SCLK_VDEC_CORE, "sclk_vdec_core", "sclk_vdec_core_c", 0x218, 2)
	CRU_GATE(ACLK_RKVENC, "aclk_rkvenc", "aclk_rkvenc_c", 0x218, 3)
	CRU_GATE(SCLK_VENC_CORE, "sclk_venc", "sclk_venc_c", 0x218, 4)
	CRU_GATE(ACLK_VPU_PRE, "aclk_vpu_pre", "aclk_vpu_pre_c", 0x218, 5)
	CRU_GATE(0, "aclk_gpu_pre", "aclk_gpu_pre_c", 0x218, 6)
	CRU_GATE(SCLK_VENC_DSP, "sclk_venc_dsp", "sclk_venc_dsp_c", 0x218, 7)
	/* Bit 8-15 unused */

	/* CRU_CLKGATE_CON7 */
	/* Bit 0 aclk_core_en */
	/* Bit 1 clk_core_periph_en */
	/* Bit 2 clk_jtag_en */
	/* Bit 3 unused */
	/* Bit 4 pclk_ddr_en */
	/* Bit 5-15 unused */

	/* CRU_CLKGATE_CON8 */
	CRU_GATE(ACLK_BUS_PRE, "aclk_bus_pre", "aclk_bus_pre_c", 0x220, 0)
	CRU_GATE(HCLK_BUS_PRE, "hclk_bus_pre", "hclk_bus_pre_c", 0x220, 1)
	CRU_GATE(PCLK_BUS_PRE, "pclk_bus_pre", "pclk_bus_pre_c", 0x220, 2)
	CRU_GATE(0, "pclk_bus", "pclk_bus_pre", 0x220, 3)
	CRU_GATE(0, "pclk_phy", "pclk_bus_pre", 0x220, 4)
	CRU_GATE(SCLK_TIMER0, "sclk_timer0", "xin24m", 0x220, 5)
	CRU_GATE(SCLK_TIMER1, "sclk_timer1", "xin24m", 0x220, 6)
	CRU_GATE(SCLK_TIMER2, "sclk_timer2", "xin24m", 0x220, 7)
	CRU_GATE(SCLK_TIMER3, "sclk_timer3", "xin24m", 0x220, 8)
	CRU_GATE(SCLK_TIMER4, "sclk_timer4", "xin24m", 0x220, 9)
	CRU_GATE(SCLK_TIMER5, "sclk_timer5", "xin24m", 0x220, 10)
	/* Bit 11-15 unused */

	/* CRU_CLKGATE_CON9 */
	CRU_GATE(PCLK_GMAC, "pclk_gmac", "aclk_gmac", 0x224, 0)
	CRU_GATE(SCLK_MAC2PHY_RXTX, "clk_gmac2phy_rx", "clk_mac2phy", 0x224, 1)
	CRU_GATE(SCLK_MAC2PHY_OUT, "clk_mac2phy_out", "clk_mac2phy_out_c", 0x224, 2)
	CRU_GATE(SCLK_MAC2PHY_REF, "clk_gmac2phy_ref", "clk_mac2phy", 0x224, 3)
	CRU_GATE(SCLK_MAC2IO_RX, "clk_gmac2io_rx", "clk_mac2io", 0x224, 4)
	CRU_GATE(SCLK_MAC2IO_TX, "clk_gmac2io_tx", "clk_mac2io", 0x224, 5)
	CRU_GATE(SCLK_MAC2IO_REFOUT, "clk_gmac2io_refout", "clk_mac2io", 0x224, 6)
	CRU_GATE(SCLK_MAC2IO_REF, "clk_gmac2io_ref", "clk_mac2io", 0x224, 7)
	/* Bit 8-15 unused */

	/* CRU_CLKGATE_CON10 */
	CRU_GATE(ACLK_PERI, "aclk_peri", "aclk_peri_pre", 0x228, 0)
	CRU_GATE(HCLK_PERI, "hclk_peri", "hclk_peri_c", 0x228, 1)
	CRU_GATE(PCLK_PERI, "pclk_peri", "pclk_peri_c", 0x228, 2)
	/* Bit 3-15 unused */

	/* CRU_CLKGATE_CON11 */
	CRU_GATE(HCLK_RKVDEC_PRE, "hclk_rkvdec_pre", "aclk_rkvdec_pre", 0x22C, 0)
	/* Bit 1-3 unused */
	CRU_GATE(HCLK_RKVENC, "hclk_rkvenc", "aclk_rkvenc", 0x22C, 4)
	/* Bit 5-7 unused */
	CRU_GATE(HCLK_VPU_PRE, "hclk_vpu_pre", "aclk_vpu_pre", 0x22C, 8)
	/* Bit 9-15 unused */

	/* CRU_CLKGATE_CON12 */
	/* unused */

	/* CRU_CLKGATE_CON13 */
	/* Bit 0 aclk_core_niu_en */
	/* Bit 1 aclk_gic400_en */
	/* Bit 2-15 unused */

	/* CRU_CLKGATE_CON14 */
	CRU_GATE(ACLK_GPU, "aclk_gpu", "aclk_gpu_pre", 0x238, 0)
	CRU_GATE(0, "aclk_gpu_niu", "aclk_gpu_pre", 0x238, 1)
	/* Bit 2-15 unused */

	/* CRU_CLKGATE_CON15*/
	/* Bit 0 aclk_intmem_en Unused */
	/* Bit 1 aclk_dmac_bus_en Unused */
	/* Bit 2 hclk_rom_en Unused */
	CRU_GATE(HCLK_I2S0_8CH, "hclk_i2s0_8ch", "hclk_bus_pre", 0x23C, 3)
	CRU_GATE(HCLK_I2S1_8CH, "hclk_i2s1_8ch", "hclk_bus_pre", 0x23C, 4)
	CRU_GATE(HCLK_I2S2_2CH, "hclk_i2s2_2ch", "hclk_bus_pre", 0x23C, 5)
	CRU_GATE(HCLK_SPDIF_8CH, "hclk_spdif_8ch", "hclk_bus_pre", 0x23C, 6)
	CRU_GATE(HCLK_CRYPTO_MST, "hclk_crypto_mst", "hclk_bus_pre", 0x23C, 7)
	CRU_GATE(HCLK_CRYPTO_SLV, "hclk_crypto_slv", "hclk_bus_pre", 0x23C, 8)
	CRU_GATE(0, "pclk_efuse", "pclk_bus", 0x23C, 9)
	CRU_GATE(PCLK_I2C0, "pclk_i2c0", "pclk_bus", 0x23C, 10)
	CRU_GATE(ACLK_DCF, "aclk_dcf", "aclk_bus_pre", 0x23C, 11)
	CRU_GATE(0, "aclk_bus_niu", "aclk_bus_pre", 0x23C, 12)
	CRU_GATE(0, "hclk_bus_niu", "hclk_bus_pre", 0x23C, 13)
	CRU_GATE(0, "pclk_bus_niu", "pclk_bus_pre", 0x23C, 14)
	CRU_GATE(0, "pclk_phy_niu", "pclk_phy", 0x23C, 14)
	/* Bit 15 pclk_phy_niu_en */

	/* CRU_CLKGATE_CON16 */
	CRU_GATE(PCLK_I2C1, "pclk_i2c1", "pclk_bus", 0x240, 0)
	CRU_GATE(PCLK_I2C2, "pclk_i2c2", "pclk_bus", 0x240, 1)
	CRU_GATE(PCLK_I2C3, "pclk_i2c3", "pclk_bus", 0x240, 2)
	CRU_GATE(PCLK_TIMER, "pclk_timer0", "pclk_bus", 0x240, 3)
	CRU_GATE(0, "pclk_stimer", "pclk_bus", 0x240, 4)
	CRU_GATE(PCLK_SPI, "pclk_spi", "pclk_bus", 0x240, 5)
	CRU_GATE(PCLK_PWM, "pclk_pwm", "pclk_bus", 0x240, 6)
	CRU_GATE(PCLK_GPIO0, "pclk_gpio0", "pclk_bus", 0x240, 7)
	CRU_GATE(PCLK_GPIO1, "pclk_gpio1", "pclk_bus", 0x240, 8)
	CRU_GATE(PCLK_GPIO2, "pclk_gpio2", "pclk_bus", 0x240, 9)
	CRU_GATE(PCLK_GPIO3, "pclk_gpio3", "pclk_bus", 0x240, 10)
	CRU_GATE(PCLK_UART0, "pclk_uart0", "pclk_bus", 0x240, 11)
	CRU_GATE(PCLK_UART1, "pclk_uart1", "pclk_bus", 0x240, 12)
	CRU_GATE(PCLK_UART2, "pclk_uart2", "pclk_bus", 0x240, 13)
	CRU_GATE(PCLK_TSADC, "pclk_tsadc", "pclk_bus", 0x240, 14)
	CRU_GATE(PCLK_DCF, "pclk_dcf", "pclk_bus", 0x240, 15)

	/* CRU_CLKGATE_CON17 */
	CRU_GATE(PCLK_GRF, "pclk_grf", "pclk_bus", 0x244, 0)
	/* Bit 1 unused */
	CRU_GATE(PCLK_USB3_GRF, "pclk_usb3grf", "pclk_phy", 0x244, 2)
	CRU_GATE(0, "pclk_ddrphy", "pclk_phy", 0x244, 3)
	CRU_GATE(0, "pclk_cru", "pclk_bus", 0x244, 4)
	CRU_GATE(PCLK_ACODECPHY, "pclk_acodecphy", "pclk_phy", 0x244, 5)
	CRU_GATE(0, "pclk_sgrf", "pclk_bus", 0x244, 6)
	CRU_GATE(PCLK_HDMIPHY, "pclk_hdmiphy", "pclk_phy", 0x244, 7)
	CRU_GATE(0, "pclk_vdacphy", "pclk_bus", 0x244, 8)
	/* Bit 9 unused */
	CRU_GATE(0, "pclk_sim", "pclk_bus", 0x244, 10)
	CRU_GATE(HCLK_TSP, "hclk_tsp", "hclk_bus_pre", 0x244, 11)
	CRU_GATE(ACLK_TSP, "aclk_tsp", "aclk_bus_pre", 0x244, 12)
	/* Bit 13 clk_hsadc_0_tsp_en Depend on a gpio clock ? */
	CRU_GATE(PCLK_USB2_GRF, "pclk_usb2grf", "pclk_phy", 0x244, 14)
	CRU_GATE(PCLK_SARADC, "pclk_saradc", "pclk_bus", 0x244, 15)

	/* CRU_CLKGATE_CON18 */
	/* Bit 0 unused */
	/* Bit 1 pclk_ddr_upctl_en */
	/* Bit 2 pclk_ddr_msch_en */
	/* Bit 3 pclk_ddr_mon_en */
	/* Bit 4 aclk_ddr_upctl_en */
	/* Bit 5 clk_ddr_upctl_en */
	/* Bit 6 clk_ddr_msch_en */
	/* Bit 7 pclk_ddrstdby_en */
	/* Bit 8-15 unused */

	/* CRU_CLKGATE_CON19 */
	CRU_GATE(HCLK_SDMMC, "hclk_sdmmc", "hclk_peri", 0x24C, 0)
	CRU_GATE(HCLK_SDIO, "hclk_sdio", "hclk_peri", 0x24C, 1)
	CRU_GATE(HCLK_EMMC, "hclk_emmc", "hclk_peri", 0x24C, 2)
	/* Bit 3-5 unused */
	CRU_GATE(HCLK_HOST0, "hclk_host0", "hclk_peri", 0x24C, 6)
	CRU_GATE(HCLK_HOST0_ARB, "hclk_host0_arg", "hclk_peri", 0x24C, 7)
	CRU_GATE(HCLK_OTG, "hclk_otg", "hclk_peri", 0x24C, 8)
	CRU_GATE(HCLK_OTG_PMU, "hclk_otg_pmu", "hclk_peri", 0x24C, 9)
	/* Bit 10 unused */
	CRU_GATE(0, "aclk_peri_niu", "aclk_peri", 0x24C, 11)
	CRU_GATE(0, "hclk_peri_niu", "hclk_peri", 0x24C, 12)
	CRU_GATE(0, "pclk_peri_niu", "hclk_peri", 0x24C, 13)
	CRU_GATE(ACLK_USB3OTG, "aclk_usb3otg", "aclk_peri", 0x24C, 14)
	CRU_GATE(HCLK_SDMMC_EXT, "hclk_sdmmc_ext", "hclk_peri", 0x24C, 15)

	/* CRU_CLKGATE_CON20 */
	/* unused */

	/* CRU_CLKGATE_CON21 */
	/* Bit 0-1 unused */
	CRU_GATE(ACLK_VOP, "aclk_vop", "aclk_vop_pre", 0x254, 2)
	CRU_GATE(HCLK_VOP, "hclk_vop", "hclk_vio_pre", 0x254, 3)
	CRU_GATE(0, "aclk_vop_niu", "aclk_vop_pre", 0x254, 4)
	CRU_GATE(0, "hclk_vop_niu", "hclk_vio_pre", 0x254, 5)
	CRU_GATE(ACLK_IEP, "aclk_iep", "aclk_vio_pre", 0x254, 6)
	CRU_GATE(HCLK_IEP, "hclk_iep", "hclk_vio_pre", 0x254, 7)
	CRU_GATE(ACLK_CIF, "aclk_cif", "aclk_vio_pre", 0x254, 8)
	CRU_GATE(HCLK_CIF, "hclk_cif", "hclk_vio_pre", 0x254, 9)
	CRU_GATE(ACLK_RGA, "aclk_rga", "aclk_rga_pre", 0x254, 10)
	CRU_GATE(HCLK_RGA, "hclk_rga", "hclk_vio_pre", 0x254, 11)
	CRU_GATE(0, "hclk_ahb1tom", "hclk_vio_pre", 0x254, 12)
	CRU_GATE(0, "pclk_vio_h2p", "hclk_vio_pre", 0x254, 13)
	CRU_GATE(0, "hclk_vio_h2p", "hclk_vio_pre", 0x254, 14)
	CRU_GATE(ACLK_HDCP, "aclk_hdcp", "aclk_vio_pre", 0x254, 15)

	/* CRU_CLKGATE_CON22 */
	CRU_GATE(HCLK_HDCP, "hclk_hdcp", "hclk_vio_pre", 0x258, 0)
	CRU_GATE(0, "hclk_vio_niu", "hclk_vio_pre", 0x258, 1)
	CRU_GATE(0, "aclk_vio_niu", "aclk_vio_pre", 0x258, 2)
	CRU_GATE(0, "aclk_rga_niu", "aclk_rga_pre", 0x258, 3)
	CRU_GATE(PCLK_HDMI, "pclk_hdmi", "hclk_vio_pre", 0x258, 4)
	CRU_GATE(PCLK_HDCP, "pclk_hdcp", "hclk_vio_pre", 0x258, 5)
	/* Bit 6-15 unused */

	/* CRU_CLKGATE_CON23 */
	CRU_GATE(ACLK_VPU, "aclk_vpu", "aclk_vpu_pre", 0x25C, 0)
	CRU_GATE(HCLK_VPU, "hclk_vpu", "hclk_vpu_pre", 0x25C, 1)
	CRU_GATE(0, "aclk_vpu_niu", "aclk_vpu_pre", 0x25C, 2)
	CRU_GATE(0, "hclk_vpu_niu", "hclk_vpu_pre", 0x25C, 3)
	/* Bit 4-15 unused */

	/* CRU_CLKGATE_CON24 */
	CRU_GATE(ACLK_RKVDEC, "aclk_rkvdec", "aclk_rkvdec_pre", 0x260, 0)
	CRU_GATE(HCLK_RKVDEC, "hclk_rkvdec", "hclk_rkvdec_pre", 0x260, 1)
	CRU_GATE(0, "aclk_rkvdec_niu", "aclk_rkvdec_pre", 0x260, 2)
	CRU_GATE(0, "hclk_rkvdec_niu", "hclk_rkvdec_pre", 0x260, 3)
	/* Bit 4-15 unused */

	/* CRU_CLKGATE_CON25 */
	CRU_GATE(0, "aclk_rkvenc_niu", "aclk_rkvenc", 0x264, 0)
	CRU_GATE(0, "hclk_rkvenc_niu", "hclk_rkvenc", 0x264, 1)
	CRU_GATE(ACLK_H265, "aclk_h265", "aclk_rkvenc", 0x264, 2)
	CRU_GATE(PCLK_H265, "pclk_h265", "hclk_rkvenc", 0x264, 3)
	CRU_GATE(ACLK_H264, "aclk_h264", "aclk_rkvenc", 0x264, 4)
	CRU_GATE(HCLK_H264, "hclk_h264", "hclk_rkvenc", 0x264, 5)
	CRU_GATE(0, "aclk_axisram", "hclk_rkvenc", 0x264, 6)
	/* Bit 7-15 unused */

	/* CRU_CLKGATE_CON26 */
	CRU_GATE(ACLK_MAC2PHY, "aclk_gmac2phy", "aclk_gmac", 0x268, 0)
	CRU_GATE(PCLK_MAC2PHY, "pclk_gmac2phy", "pclk_gmac", 0x268, 1)
	CRU_GATE(ACLK_MAC2IO, "aclk_gmac2io", "aclk_gmac", 0x268, 2)
	CRU_GATE(PCLK_MAC2IO, "pclk_gmac2io", "pclk_gmac", 0x268, 3)
	CRU_GATE(0, "aclk_gmac_niu", "aclk_gmac", 0x268, 4)
	CRU_GATE(0, "pclk_gmac_niu", "pclk_gmac", 0x268, 5)
	/* Bit 6-15 unused */

	/* CRU_CLKGATE_CON27 */
	/* Bit 0 clk_ddrphy_en */
	/* Bit 1 clk4x_ddrphy_en */

	/* CRU_CLKGATE_CON28 */
	CRU_GATE(HCLK_PDM, "hclk_pdm", "hclk_bus_pre", 0x270, 0)
	CRU_GATE(PCLK_USB3PHY_OTG, "pclk_usb3phy_otg", "pclk_phy", 0x270, 1)
	CRU_GATE(PCLK_USB3PHY_PIPE, "pclk_usb3phy_pipe", "pclk_phy", 0x270, 2)
	CRU_GATE(0, "pclk_pmu", "pclk_bus", 0x270, 3)
	CRU_GATE(0, "pclk_otp", "pclk_bus", 0x270, 4)
	/* Bit 5-15 unused */
};

/*
 * PLLs
 */

#define PLL_RATE(_hz, _ref, _fb, _post1, _post2, _dspd, _frac)		\
{									\
	.freq = _hz,							\
	.refdiv = _ref,							\
	.fbdiv = _fb,							\
	.postdiv1 = _post1,						\
	.postdiv2 = _post2,						\
	.dsmpd = _dspd,							\
	.frac = _frac,							\
}

static struct rk_clk_pll_rate rk3328_pll_rates[] = {
	/* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
	PLL_RATE(1608000000, 1, 67, 1, 1, 1, 0),
	PLL_RATE(1584000000, 1, 66, 1, 1, 1, 0),
	PLL_RATE(1560000000, 1, 65, 1, 1, 1, 0),
	PLL_RATE(1536000000, 1, 64, 1, 1, 1, 0),
	PLL_RATE(1512000000, 1, 63, 1, 1, 1, 0),
	PLL_RATE(1488000000, 1, 62, 1, 1, 1, 0),
	PLL_RATE(1464000000, 1, 61, 1, 1, 1, 0),
	PLL_RATE(1440000000, 1, 60, 1, 1, 1, 0),
	PLL_RATE(1416000000, 1, 59, 1, 1, 1, 0),
	PLL_RATE(1392000000, 1, 58, 1, 1, 1, 0),
	PLL_RATE(1368000000, 1, 57, 1, 1, 1, 0),
	PLL_RATE(1344000000, 1, 56, 1, 1, 1, 0),
	PLL_RATE(1320000000, 1, 55, 1, 1, 1, 0),
	PLL_RATE(1296000000, 1, 54, 1, 1, 1, 0),
	PLL_RATE(1272000000, 1, 53, 1, 1, 1, 0),
	PLL_RATE(1248000000, 1, 52, 1, 1, 1, 0),
	PLL_RATE(1200000000, 1, 50, 1, 1, 1, 0),
	PLL_RATE(1188000000, 2, 99, 1, 1, 1, 0),
	PLL_RATE(1104000000, 1, 46, 1, 1, 1, 0),
	PLL_RATE(1100000000, 12, 550, 1, 1, 1, 0),
	PLL_RATE(1008000000, 1, 84, 2, 1, 1, 0),
	PLL_RATE(1000000000, 6, 500, 2, 1, 1, 0),
	PLL_RATE(984000000, 1, 82, 2, 1, 1, 0),
	PLL_RATE(960000000, 1, 80, 2, 1, 1, 0),
	PLL_RATE(936000000, 1, 78, 2, 1, 1, 0),
	PLL_RATE(912000000, 1, 76, 2, 1, 1, 0),
	PLL_RATE(900000000, 4, 300, 2, 1, 1, 0),
	PLL_RATE(888000000, 1, 74, 2, 1, 1, 0),
	PLL_RATE(864000000, 1, 72, 2, 1, 1, 0),
	PLL_RATE(840000000, 1, 70, 2, 1, 1, 0),
	PLL_RATE(816000000, 1, 68, 2, 1, 1, 0),
	PLL_RATE(800000000, 6, 400, 2, 1, 1, 0),
	PLL_RATE(700000000, 6, 350, 2, 1, 1, 0),
	PLL_RATE(696000000, 1, 58, 2, 1, 1, 0),
	PLL_RATE(600000000, 1, 75, 3, 1, 1, 0),
	PLL_RATE(594000000, 2, 99, 2, 1, 1, 0),
	PLL_RATE(504000000, 1, 63, 3, 1, 1, 0),
	PLL_RATE(500000000, 6, 250, 2, 1, 1, 0),
	PLL_RATE(408000000, 1, 68, 2, 2, 1, 0),
	PLL_RATE(312000000, 1, 52, 2, 2, 1, 0),
	PLL_RATE(216000000, 1, 72, 4, 2, 1, 0),
	PLL_RATE(96000000, 1, 64, 4, 4, 1, 0),
	{},
};

static struct rk_clk_pll_rate rk3328_pll_frac_rates[] = {
	PLL_RATE(1016064000, 3, 127, 1, 1, 0, 134217),
	PLL_RATE(983040000, 24, 983, 1, 1, 0, 671088),
	PLL_RATE(491520000, 24, 983, 2, 1, 0, 671088),
	PLL_RATE(61440000, 6, 215, 7, 2, 0, 671088),
	PLL_RATE(56448000, 12, 451, 4, 4, 0, 9797894),
	PLL_RATE(40960000, 12, 409, 4, 5, 0, 10066329),
	{},
};

/* Clock parents */
#define PLIST(_name) static const char *_name[]

PLIST(pll_src_p) = {"xin24m"};
PLIST(xin24m_rtc32k_p) = {"xin24m", "clk_rtc32k"};

PLIST(pll_src_cpll_gpll_p) = {"cpll", "gpll"};
PLIST(pll_src_cpll_gpll_apll_p) = {"cpll", "gpll", "apll"};
PLIST(pll_src_cpll_gpll_xin24m_p) = {"cpll", "gpll", "xin24m", "xin24m" /* Dummy */};
PLIST(pll_src_cpll_gpll_usb480m_p) = {"cpll", "gpll", "usb480m"};
PLIST(pll_src_cpll_gpll_hdmiphy_p) = {"cpll", "gpll", "hdmi_phy"};
PLIST(pll_src_cpll_gpll_hdmiphy_usb480m_p) = {"cpll", "gpll", "hdmi_phy", "usb480m"};
PLIST(pll_src_apll_gpll_dpll_npll_p) = {"apll", "gpll", "dpll", "npll"};
PLIST(pll_src_cpll_gpll_xin24m_usb480m_p) = {"cpll", "gpll", "xin24m", "usb480m"};
PLIST(mux_ref_usb3otg_p) = { "xin24m", "clk_usb3_otg0_ref" };
PLIST(mux_mac2io_p) = { "clk_mac2io_src", "gmac_clkin" };
PLIST(mux_mac2io_ext_p) = { "clk_mac2io", "gmac_clkin" };
PLIST(mux_mac2phy_p) = { "clk_mac2phy_src", "phy_50m_out" };
PLIST(mux_i2s0_p) = { "clk_i2s0_div", "clk_i2s0_frac", "xin12m", "xin12m" };
PLIST(mux_i2s1_p) = { "clk_i2s1_div", "clk_i2s1_frac", "clkin_i2s1", "xin12m" };
PLIST(mux_i2s2_p) = { "clk_i2s2_div", "clk_i2s2_frac", "clkin_i2s2", "xin12m" };
PLIST(mux_dclk_lcdc_p) = {"hdmiphy", "vop_dclk_src"};
PLIST(mux_hdmiphy_p) = {"hdmi_phy", "xin24m"};
PLIST(mux_usb480m_p) = {"usb480m_phy", "xin24m"};
PLIST(mux_uart0_p) = {"clk_uart0_div", "clk_uart0_frac", "xin24m", "xin24m"};
PLIST(mux_uart1_p) = {"clk_uart1_div", "clk_uart1_frac", "xin24m", "xin24m"};
PLIST(mux_uart2_p) = {"clk_uart2_div", "clk_uart2_frac", "xin24m", "xin24m"};
PLIST(mux_spdif_p) = {"clk_spdif_div", "clk_spdif_frac", "xin12m", "xin12m"};
PLIST(mux_cif_p) = {"clk_cif_pll", "xin24m"};

static struct rk_clk_pll_def apll = {
	.clkdef = {
		.id = PLL_APLL,
		.name = "apll",
		.parent_names = pll_src_p,
		.parent_cnt = nitems(pll_src_p),
	},
	.base_offset = 0x00,
	.gate_offset = 0x200,
	.gate_shift = 0,
	.mode_reg = 0x80,
	.mode_shift = 1,
	.flags = RK_CLK_PLL_HAVE_GATE,
	.frac_rates = rk3328_pll_frac_rates,
};

static struct rk_clk_pll_def dpll = {
	.clkdef = {
		.id = PLL_DPLL,
		.name = "dpll",
		.parent_names = pll_src_p,
		.parent_cnt = nitems(pll_src_p),
	},
	.base_offset = 0x20,
	.gate_offset = 0x200,
	.gate_shift = 1,
	.mode_reg = 0x80,
	.mode_shift = 4,
	.flags = RK_CLK_PLL_HAVE_GATE,
};

static struct rk_clk_pll_def cpll = {
	.clkdef = {
		.id = PLL_CPLL,
		.name = "cpll",
		.parent_names = pll_src_p,
		.parent_cnt = nitems(pll_src_p),
	},
	.base_offset = 0x40,
	.mode_reg = 0x80,
	.mode_shift = 8,
	.rates = rk3328_pll_rates,
};

static struct rk_clk_pll_def gpll = {
	.clkdef = {
		.id = PLL_GPLL,
		.name = "gpll",
		.parent_names = pll_src_p,
		.parent_cnt = nitems(pll_src_p),
	},
	.base_offset = 0x60,
	.gate_offset = 0x200,
	.gate_shift = 2,
	.mode_reg = 0x80,
	.mode_shift = 12,
	.flags = RK_CLK_PLL_HAVE_GATE,
	.frac_rates = rk3328_pll_frac_rates,
};

static struct rk_clk_pll_def npll = {
	.clkdef = {
		.id = PLL_NPLL,
		.name = "npll",
		.parent_names = pll_src_p,
		.parent_cnt = nitems(pll_src_p),
	},
	.base_offset = 0xa0,
	.gate_offset = 0x200,
	.gate_shift = 12,
	.mode_reg = 0x80,
	.mode_shift = 1,
	.flags = RK_CLK_PLL_HAVE_GATE,
	.rates = rk3328_pll_rates,
};

static struct rk_clk_armclk_rates rk3328_armclk_rates[] = {
	{
		.freq = 1296000000,
		.div = 1,
	},
	{
		.freq = 1200000000,
		.div = 1,
	},
	{
		.freq = 1104000000,
		.div = 1,
	},
	{
		.freq = 1008000000,
		.div = 1,
	},
	{
		.freq = 912000000,
		.div = 1,
	},
	{
		.freq = 816000000,
		.div = 1,
	},
	{
		.freq = 696000000,
		.div = 1,
	},
	{
		.freq = 600000000,
		.div = 1,
	},
	{
		.freq = 408000000,
		.div = 1,
	},
	{
		.freq = 312000000,
		.div = 1,
	},
	{
		.freq = 216000000,
		.div = 1,
	},
	{
		.freq = 96000000,
		.div = 1,
	},
};

static struct rk_clk_armclk_def armclk = {
	.clkdef = {
		.id = ARMCLK,
		.name = "armclk",
		.parent_names = pll_src_apll_gpll_dpll_npll_p,
		.parent_cnt = nitems(pll_src_apll_gpll_dpll_npll_p),
	},
	.muxdiv_offset = 0x100,
	.mux_shift = 6,
	.mux_width = 2,

	.div_shift = 0,
	.div_width = 5,

	.flags = RK_CLK_COMPOSITE_HAVE_MUX,
	.main_parent = 3, /* npll */
	.alt_parent = 0, /* apll */

	.rates = rk3328_armclk_rates,
	.nrates = nitems(rk3328_armclk_rates),
};

static struct rk_clk rk3328_clks[] = {
	/* External clocks */
	LINK("xin24m"),
	LINK("gmac_clkin"),
	LINK("hdmi_phy"),
	LINK("usb480m_phy"),
	FRATE(0, "xin12m", 12000000),
	FRATE(0, "phy_50m_out", 50000000),
	FRATE(0, "clkin_i2s1", 0),
	FRATE(0, "clkin_i2s2", 0),

	/* PLLs */
	{
		.type = RK3328_CLK_PLL,
		.clk.pll = &apll
	},
	{
		.type = RK3328_CLK_PLL,
		.clk.pll = &dpll
	},
	{
		.type = RK3328_CLK_PLL,
		.clk.pll = &cpll
	},
	{
		.type = RK3328_CLK_PLL,
		.clk.pll = &gpll
	},
	{
		.type = RK3328_CLK_PLL,
		.clk.pll = &npll
	},

	{
		.type = RK_CLK_ARMCLK,
		.clk.armclk = &armclk,
	},

	/* CRU_CRU_MISC */
	MUXRAW(HDMIPHY, "hdmiphy", mux_hdmiphy_p, 0, 0x84, 13, 1),
	MUXRAW(USB480M, "usb480m", mux_usb480m_p, 0, 0x84, 15, 1),

	/* CRU_CLKSEL_CON0 */
	/* COMP clk_core_div_con core_clk_pll_sel */
	COMP(0, "aclk_bus_pre_c", pll_src_cpll_gpll_hdmiphy_p, 0, 0, 8, 5, 13, 2),

	/* CRU_CLKSEL_CON1 */
	/* CDIV clk_core_dbg_div_con */
	/* CDIV aclk_core_div_con */
	CDIV(0, "hclk_bus_pre_c", "aclk_bus_pre", 0, 1, 8, 2),
	CDIV(0, "pclk_bus_pre_c", "aclk_bus_pre", 0, 1, 12, 2),

	/* CRU_CLKSEL_CON2 */
	/* CDIV test_div_con */
	/* CDIV func_24m_div_con */

	/* CRU_CLKSEL_CON3 */
	/* COMP ddr_div_cnt ddr_clk_pll_sel */

	/* CRU_CLKSEL_CON4 */
	COMP(0, "clk_otp_c", pll_src_cpll_gpll_xin24m_p, 0, 4, 0, 6, 6, 2),
	/* COMP pd_ddr_div_con ddrpdclk_clk_pll_sel */

	/* CRU_CLKSEL_CON5 */
	COMP(0, "clk_efuse_c", pll_src_cpll_gpll_xin24m_p, 0, 5, 8, 5, 14, 2),

	/* CRU_CLKSEL_CON6 */
	MUX(0, "clk_i2s0_mux", mux_i2s0_p, RK_CLK_MUX_REPARENT, 6, 8, 2),
	COMP(0, "clk_i2s0_div_c", pll_src_cpll_gpll_p, 0, 6, 0, 7, 15, 1),

	/* CRU_CLKSEL_CON7 */
	FRACT(0, "clk_i2s0_frac_f", "clk_i2s0_div", 0, 7),

	/* CRU_CLKSEL_CON8 */
	MUX(0, "clk_i2s1_mux", mux_i2s1_p, RK_CLK_MUX_REPARENT, 8, 8, 2),
	COMP(0, "clk_i2s1_div_c", pll_src_cpll_gpll_p, 0, 8, 0, 7, 15, 1),
	/* MUX i2s1_out_sel */

	/* CRU_CLKSEL_CON9 */
	FRACT(0, "clk_i2s1_frac_f", "clk_i2s1_div", 0, 9),

	/* CRU_CLKSEL_CON10 */
	MUX(0, "clk_i2s2_mux", mux_i2s2_p, RK_CLK_MUX_REPARENT, 10, 8, 2),
	COMP(0, "clk_i2s2_div_c", pll_src_cpll_gpll_p, 0, 10, 0, 7, 15, 1),
	/* MUX i2s2_out_sel */

	/* CRU_CLKSEL_CON11 */
	FRACT(0, "clk_i2s2_frac_f", "clk_i2s2_div", 0, 11),

	/* CRU_CLKSEL_CON12 */
	MUX(0, "clk_spdif_pll", pll_src_cpll_gpll_p, 0, 12, 15, 1),
	MUX(SCLK_SPDIF, "clk_spdif", mux_spdif_p, 0, 12, 8, 2),
	CDIV(0, "clk_spdif_div_c", "clk_spdif_pll", 0, 12, 0, 7),

	/* CRU_CLKSEL_CON13 */
	FRACT(0, "clk_spdif_frac_f", "clk_spdif", 0, 13),

	/* CRU_CLKSEL_CON14 */
	MUX(0, "clk_uart0_pll", pll_src_cpll_gpll_usb480m_p, 0, 14, 12, 2),
	MUX(SCLK_UART0, "clk_uart0", mux_uart0_p, 0, 14, 8, 2),
	CDIV(0, "clk_uart0_div_c", "clk_uart0_pll", 0, 14, 0, 7),

	/* CRU_CLKSEL_CON15 */
	FRACT(0, "clk_uart0_frac_f", "clk_uart0_pll", 0, 15),

	/* CRU_CLKSEL_CON16 */
	MUX(0, "clk_uart1_pll", pll_src_cpll_gpll_usb480m_p, 0, 16, 12, 2),
	MUX(SCLK_UART1, "clk_uart1", mux_uart1_p, 0, 16, 8, 2),
	CDIV(0, "clk_uart1_div_c", "clk_uart1_pll", 0, 16, 0, 7),

	/* CRU_CLKSEL_CON17 */
	FRACT(0, "clk_uart1_frac_f", "clk_uart1_pll", 0, 17),

	/* CRU_CLKSEL_CON18 */
	MUX(0, "clk_uart2_pll", pll_src_cpll_gpll_usb480m_p, 0, 18, 12, 2),
	MUX(SCLK_UART2, "clk_uart2", mux_uart2_p, 0, 18, 8, 2),
	CDIV(0, "clk_uart2_div_c", "clk_uart2_pll", 0, 18, 0, 7),

	/* CRU_CLKSEL_CON19 */
	FRACT(0, "clk_uart2_frac_f", "clk_uart2_pll", 0, 19),

	/* CRU_CLKSEL_CON20 */
	COMP(0, "clk_pdm_c", pll_src_cpll_gpll_apll_p, 0, 20, 8, 5, 14, 2),
	COMP(0, "clk_crypto_c", pll_src_cpll_gpll_p, 0, 20, 0, 5, 7, 1),

	/* CRU_CLKSEL_CON21 */
	COMP(0, "clk_tsp_c", pll_src_cpll_gpll_p, 0, 21, 8, 5, 15, 1),

	/* CRU_CLKSEL_CON22 */
	CDIV(0, "clk_tsadc_c", "xin24m", 0, 22, 0, 10),

	/* CRU_CLKSEL_CON23 */
	CDIV(0, "clk_saradc_c", "xin24m", 0, 23, 0, 10),

	/* CRU_CLKSEL_CON24 */
	COMP(0, "clk_pwm_c", pll_src_cpll_gpll_p, 0, 24, 8, 7, 15, 1),
	COMP(0, "clk_spi_c", pll_src_cpll_gpll_p, 0, 24, 0, 7, 7, 1),

	/* CRU_CLKSEL_CON25 */
	COMP(0, "aclk_gmac_c", pll_src_cpll_gpll_p, 0, 35, 0, 5, 6, 2),
	CDIV(0, "pclk_gmac_c", "pclk_gmac", 0, 25, 8, 3),

	/* CRU_CLKSEL_CON26 */
	CDIV(0, "clk_mac2phy_out_c", "clk_mac2phy", 0, 26, 8, 2),
	COMP(0, "clk_mac2phy_src_c", pll_src_cpll_gpll_p, 0, 26, 0, 5, 7, 1),

	/* CRU_CLKSEL_CON27 */
	COMP(0, "clk_mac2io_src_c", pll_src_cpll_gpll_p, 0, 27, 0, 5, 7, 1),
	COMP(0, "clk_mac2io_out_c", pll_src_cpll_gpll_p, 0, 27, 8, 5, 15, 1),

	/* CRU_CLKSEL_CON28 */
	COMP(ACLK_PERI_PRE, "aclk_peri_pre", pll_src_cpll_gpll_hdmiphy_p, 0, 28, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON29 */
	CDIV(0, "pclk_peri_c", "aclk_peri_pre", 0, 29, 0, 2),
	CDIV(0, "hclk_peri_c", "aclk_peri_pre", 0, 29, 4, 3),

	/* CRU_CLKSEL_CON30 */
	COMP(0, "clk_sdmmc_c", pll_src_cpll_gpll_xin24m_usb480m_p, 0, 30, 0, 8, 8, 2),

	/* CRU_CLKSEL_CON31 */
	COMP(0, "clk_sdio_c", pll_src_cpll_gpll_xin24m_usb480m_p, 0, 31, 0, 8, 8, 2),

	/* CRU_CLKSEL_CON32 */
	COMP(0, "clk_emmc_c", pll_src_cpll_gpll_xin24m_usb480m_p, 0, 32, 0, 8, 8, 2),

	/* CRU_CLKSEL_CON33 */
	COMP(0, "clk_usb3otg_suspend_c", xin24m_rtc32k_p, 0, 33, 0, 10, 15, 1),

	/* CRU_CLKSEL_CON34 */
	COMP(0, "clk_i2c0_c", pll_src_cpll_gpll_p, 0, 34, 0, 7, 7, 1),
	COMP(0, "clk_i2c1_c", pll_src_cpll_gpll_p, 0, 34, 8, 7, 15, 1),

	/* CRU_CLKSEL_CON35 */
	COMP(0, "clk_i2c2_c", pll_src_cpll_gpll_p, 0, 35, 0, 7, 7, 1),
	COMP(0, "clk_i2c3_c", pll_src_cpll_gpll_p, 0, 35, 8, 7, 15, 1),

	/* CRU_CLKSEL_CON36 */
	COMP(0, "aclk_rga_pre_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 36, 8, 5, 14, 2),
	COMP(0, "sclk_rga_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 36, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON37 */
	COMP(0, "aclk_vio_pre_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 37, 0, 5, 6, 2),
	CDIV(HCLK_VIO_PRE, "hclk_vio_pre", "aclk_vio_pre", 0, 37, 8, 5),

	/* CRU_CLKSEL_CON38 */
	COMP(0, "clk_rtc32k_c", pll_src_cpll_gpll_xin24m_p, 0, 38, 0, 14, 14, 2),

	/* CRU_CLKSEL_CON39 */
	COMP(0, "aclk_vop_pre_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 39, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON40 */
	COMP(0, "vop_dclk_src_c", pll_src_cpll_gpll_p, 0, 40, 8, 8, 0, 1),
	CDIV(DCLK_HDMIPHY, "hdmiphy_div", "vop_dclk_src", 0, 40, 3, 3),
	/* MUX vop_dclk_frac_sel */
	MUX(DCLK_LCDC, "vop_dclk", mux_dclk_lcdc_p, 0, 40, 1, 1),

	/* CRU_CLKSEL_CON41 */
	/* FRACT dclk_vop_frac_div_con */

	/* CRU_CLKSEL_CON42 */
	MUX(0, "clk_cif_pll", pll_src_cpll_gpll_p, 0, 42, 7, 1),
	COMP(0, "clk_cif_src_c", mux_cif_p, 0, 42, 0, 5, 5, 1),

	/* CRU_CLKSEL_CON43 */
	COMP(0, "clk_sdmmc_ext_c", pll_src_cpll_gpll_xin24m_usb480m_p, 0, 43, 0, 8, 8, 2),

	/* CRU_CLKSEL_CON44 */
	COMP(0, "aclk_gpu_pre_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 44, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON45 */
	MUX(SCLK_REF_USB3OTG, "clk_ref_usb3otg", mux_ref_usb3otg_p, 0, 45, 8, 1),
	COMP(0, "clk_ref_usb3otg_src_c", pll_src_cpll_gpll_p, 0, 45, 0, 7, 7, 1),

	/* CRU_CLKSEL_CON46 */
	/* Unused */

	/* CRU_CLKSEL_CON47 */
	/* Unused */

	/* CRU_CLKSEL_CON48 */
	COMP(0, "sclk_cabac_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 48, 8, 5, 14, 2),
	COMP(0, "aclk_rkvdec_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 48, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON49 */
	COMP(0, "sclk_vdec_core_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 49, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON50 */
	COMP(0, "aclk_vpu_pre_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 50, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON51 */
	COMP(0, "sclk_venc_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 51, 8, 5, 14, 2),
	COMP(0, "aclk_rkvenc_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 51, 0, 5, 6, 2),

	/* CRU_CLKSEL_CON52 */
	COMP(0, "sclk_venc_dsp_c", pll_src_cpll_gpll_hdmiphy_usb480m_p, 0, 51, 8, 5, 14, 2),
	COMP(0, "sclk_wifi_c", pll_src_cpll_gpll_usb480m_p, 0, 51, 0, 6, 6, 2),

	/* GRF_SOC_CON4 */
	MUXGRF(SCLK_MAC2IO_EXT, "clk_mac2io_ext", mux_mac2io_ext_p, 0, RK3328_GRF_SOC_CON4, 14, 1),

	/* GRF_MAC_CON1 */
	MUXGRF(SCLK_MAC2IO, "clk_mac2io", mux_mac2io_p, 0, RK3328_GRF_MAC_CON1, 10, 1),

	/* GRF_MAC_CON2 */
	MUXGRF(SCLK_MAC2PHY, "clk_mac2phy", mux_mac2phy_p, 0, RK3328_GRF_MAC_CON2, 10, 1),

	/*
	 * This clock is controlled in the secure world
	 */
	FFACT(PCLK_WDT, "pclk_wdt", "pclk_bus", 1, 1),
};

static int
rk3328_cru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3328-cru")) {
		device_set_desc(dev, "Rockchip RK3328 Clock and Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
rk3328_cru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->gates = rk3328_gates;
	sc->ngates = nitems(rk3328_gates);

	sc->clks = rk3328_clks;
	sc->nclks = nitems(rk3328_clks);

	sc->reset_offset = 0x300;
	sc->reset_num = 184;

	return (rk_cru_attach(dev));
}

static device_method_t rk3328_cru_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3328_cru_probe),
	DEVMETHOD(device_attach,	rk3328_cru_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3328_cru, rk3328_cru_driver, rk3328_cru_methods,
  sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3328_cru, simplebus, rk3328_cru_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
