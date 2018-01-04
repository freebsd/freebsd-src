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
 *
 * $FreeBSD$
 */

#ifndef _TEGRA210_CAR_
#define	_TEGRA210_CAR_

#include "clkdev_if.h"

#define	RD4(sc, reg, val)	CLKDEV_READ_4((sc)->clkdev, reg, val)
#define	WR4(sc, reg, val)	CLKDEV_WRITE_4((sc)->clkdev, reg, val)
#define	MD4(sc, reg, mask, set)	CLKDEV_MODIFY_4((sc)->clkdev, reg, mask, set)
#define	DEVICE_LOCK(sc)		CLKDEV_DEVICE_LOCK((sc)->clkdev)
#define	DEVICE_UNLOCK(sc)	CLKDEV_DEVICE_UNLOCK((sc)->clkdev)

#define	RST_SOURCE			0x000
#define	RST_DEVICES_L			0x004
#define	RST_DEVICES_H			0x008
#define	RST_DEVICES_U			0x00C
#define	CLK_OUT_ENB_L			0x010
#define	CLK_OUT_ENB_H			0x014
#define	CLK_OUT_ENB_U			0x018
#define	SUPER_CCLK_DIVIDER		0x024
#define	SCLK_BURST_POLICY		0x028
#define	SUPER_SCLK_DIVIDER		0x02c
#define	CLK_SYSTEM_RATE			0x030
#define	CLK_MASK_ARM			0x044
#define MISC_CLK_ENB			0x048

#define	OSC_CTRL			0x050
 #define	OSC_CTRL_OSC_FREQ_GET(x)	(((x) >> 28) & 0x0F)
 #define	OSC_CTRL_PLL_REF_DIV_GET(x)	(((x) >> 26) & 0x03)

#define	OSC_FREQ_DET_STATUS		0x05c
#define	PLLE_SS_CNTL 			0x068
#define	 PLLE_SS_CNTL_INTEGOFFSET(x)		(((x) & 0x03) << 30)
#define	 PLLE_SS_CNTL_SSCINCINTRV(x)		(((x) & 0x3f) << 24)
#define	 PLLE_SS_CNTL_SSCINC(x)			(((x) & 0xff) << 16)
#define	 PLLE_SS_CNTL_SSCINVERT 		(1 << 15)
#define	 PLLE_SS_CNTL_SSCCENTER 		(1 << 14)
#define	 PLLE_SS_CNTL_SSCPDMBYP			(1 << 13)
#define	 PLLE_SS_CNTL_SSCBYP 			(1 << 12)
#define	 PLLE_SS_CNTL_INTERP_RESET 		(1 << 11)
#define	 PLLE_SS_CNTL_BYPASS_SS 		(1 << 10)
#define	 PLLE_SS_CNTL_SSCMAX(x)			(((x) & 0x1ff) <<  0)

#define	 PLLE_SS_CNTL_SSCINCINTRV_MASK		(0x3f << 24)
#define	 PLLE_SS_CNTL_SSCINCINTRV_VAL 		(0x20 << 24)
#define	 PLLE_SS_CNTL_SSCINC_MASK 		(0xff << 16)
#define	 PLLE_SS_CNTL_SSCINC_VAL 		(0x1 << 16)
#define	 PLLE_SS_CNTL_SSCMAX_MASK		0x1ff
#define	 PLLE_SS_CNTL_SSCMAX_VAL 		0x25
#define	 PLLE_SS_CNTL_DISABLE 			(PLLE_SS_CNTL_BYPASS_SS |    \
						 PLLE_SS_CNTL_INTERP_RESET | \
						 PLLE_SS_CNTL_SSCBYP)
#define	 PLLE_SS_CNTL_COEFFICIENTS_MASK 	(PLLE_SS_CNTL_SSCMAX_MASK |  \
						 PLLE_SS_CNTL_SSCINC_MASK |  \
						 PLLE_SS_CNTL_SSCINCINTRV_MASK)
#define	 PLLE_SS_CNTL_COEFFICIENTS_VAL 		(PLLE_SS_CNTL_SSCMAX_VAL |   \
						 PLLE_SS_CNTL_SSCINC_VAL |   \
						 PLLE_SS_CNTL_SSCINCINTRV_VAL)

#define	PLLE_MISC1 			0x06C
#define	PLLC_BASE			0x080
#define	PLLC_OUT			0x084
#define	PLLC_MISC_0			0x088
#define	PLLC_MISC_1			0x08c
#define	PLLM_BASE			0x090
#define	PLLM_MISC1			0x099
#define	PLLM_MISC2			0x09c
#define	PLLP_BASE			0x0a0
#define	PLLP_OUTA			0x0a4
#define	PLLP_OUTB			0x0a8
#define	PLLP_MISC			0x0ac
#define	PLLA_BASE			0x0b0
#define	PLLA_OUT			0x0b4
#define	PLLA_MISC1			0x0b8
#define	PLLA_MISC			0x0bc
#define	PLLU_BASE			0x0c0
#define	PLLU_OUTA			0x0c4
#define	PLLU_MISC1			0x0c8
#define	PLLU_MISC			0x0cc
#define	PLLD_BASE			0x0d0
#define	PLLD_MISC1			0x0d8
#define	PLLD_MISC			0x0dc
#define	PLLX_BASE			0x0e0
#define	PLLX_MISC			0x0e4
#define	 PLLX_MISC_LOCK_ENABLE			(1 << 18)

#define	PLLE_BASE			0x0e8
#define	 PLLE_BASE_ENABLE			(1U << 31)
#define	 PLLE_BASE_LOCK_OVERRIDE		(1 << 30)

#define	PLLE_MISC			0x0ec
#define	 PLLE_MISC_SETUP_BASE(x)		(((x) & 0xFFFF) << 16)
#define	 PLLE_MISC_CLKENABLE 			(1 << 15)
#define	 PLLE_MISC_IDDQ_SWCTL			(1 << 14)
#define	 PLLE_MISC_IDDQ_OVERRIDE_VALUE		(1 << 13)
#define	 PLLE_MISC_IDDQ_FREQLOCK		(1 << 12)
#define	 PLLE_MISC_LOCK 			(1 << 11)
#define	 PLLE_MISC_REF_DIS 			(1 << 10)
#define	 PLLE_MISC_LOCK_ENABLE 			(1 <<  9)
#define	 PLLE_MISC_PTS 				(1 <<  8)
#define	 PLLE_MISC_KCP(x)			(((x) & 0x03) << 6)
#define	 PLLE_MISC_VREG_BG_CTRL(x)		(((x) & 0x03) << 4)
#define	 PLLE_MISC_VREG_CTRL(x)			(((x) & 0x03) << 2)
#define	 PLLE_MISC_KVCO				(1 <<  0)

#define	 PLLE_MISC_VREG_BG_CTRL_SHIFT		4
#define	 PLLE_MISC_VREG_BG_CTRL_MASK		(3 << PLLE_MISC_VREG_BG_CTRL_SHIFT)
#define	 PLLE_MISC_VREG_CTRL_SHIFT		2
#define	 PLLE_MISC_VREG_CTRL_MASK		(2 << PLLE_MISC_VREG_CTRL_SHIFT)
#define	 PLLE_MISC_SETUP_BASE_SHIFT 		16
#define	 PLLE_MISC_SETUP_BASE_MASK 		(0xffff << PLLE_MISC_SETUP_BASE_SHIFT)

#define	PLLE_SS_CNTL1			0x0f0
#define	PLLE_SS_CNTL2			0x0f4
#define	LVL2_CLK_GATE_OVRA		0x0f8
#define	LVL2_CLK_GATE_OVRB		0x0fc
#define	LVL2_CLK_GATE_OVRC		0x3a0 	/* Misordered in TRM */
#define	LVL2_CLK_GATE_OVRD		0x3a4
#define	LVL2_CLK_GATE_OVRE		0x554

#define	CLK_SOURCE_I2S2			0x100
#define	CLK_SOURCE_I2S3			0x104
#define	CLK_SOURCE_SPDIF_OUT		0x108
#define	CLK_SOURCE_SPDIF_IN		0x10c
#define	CLK_SOURCE_PWM			0x110
#define	CLK_SOURCE_SPI2			0x118
#define	CLK_SOURCE_SPI3			0x11c
#define	CLK_SOURCE_I2C1			0x124
#define	CLK_SOURCE_I2C5			0x128
#define	CLK_SOURCE_SPI1			0x134
#define	CLK_SOURCE_DISP1		0x138
#define	CLK_SOURCE_DISP2		0x13c
#define	CLK_SOURCE_ISP			0x144
#define	CLK_SOURCE_VI			0x148
#define	CLK_SOURCE_SDMMC1		0x150
#define	CLK_SOURCE_SDMMC2		0x154
#define	CLK_SOURCE_SDMMC4		0x164
#define	CLK_SOURCE_UARTA		0x178
#define	CLK_SOURCE_UARTB		0x17c
#define	CLK_SOURCE_HOST1X		0x180
#define	CLK_SOURCE_I2C2			0x198
#define	CLK_SOURCE_EMC			0x19c
#define	CLK_SOURCE_UARTC		0x1a0
#define	CLK_SOURCE_VI_SENSOR		0x1a8
#define	CLK_SOURCE_SPI4			0x1b4
#define	CLK_SOURCE_I2C3			0x1b8
#define	CLK_SOURCE_SDMMC3		0x1bc
#define	CLK_SOURCE_UARTD		0x1c0
#define	CLK_SOURCE_OWR			0x1cc
#define	CLK_SOURCE_CSITE		0x1d4
#define	CLK_SOURCE_I2S1			0x1d8
#define	CLK_SOURCE_DTV			0x1dc
#define	CLK_SOURCE_TSEC			0x1f4
#define	CLK_SOURCE_SPARE2		0x1f8

#define	CLK_OUT_ENB_X			0x280
#define	CLK_ENB_X_SET			0x284
#define	CLK_ENB_X_CLR			0x288
#define	RST_DEVICES_X			0x28C
#define	RST_DEV_X_SET			0x290
#define	RST_DEV_X_CLR			0x294
#define	CLK_OUT_ENB_Y			0x298
#define	CLK_ENB_Y_SET			0x29c
#define	CLK_ENB_Y_CLR			0x2a0
#define	RST_DEVICES_Y			0x2a4
#define	RST_DEV_Y_SET			0x2a8
#define	RST_DEV_Y_CLR			0x2ac
#define	DFLL_BASE			0x2f4
#define	 DFLL_BASE_DVFS_DFLL_RESET		(1 << 0)

#define	RST_DEV_L_SET			0x300
#define	RST_DEV_L_CLR			0x304
#define	RST_DEV_H_SET			0x308
#define	RST_DEV_H_CLR			0x30c
#define	RST_DEV_U_SET			0x310
#define	RST_DEV_U_CLR			0x314
#define	CLK_ENB_L_SET			0x320
#define	CLK_ENB_L_CLR			0x324
#define	CLK_ENB_H_SET			0x328
#define	CLK_ENB_H_CLR			0x32c
#define	CLK_ENB_U_SET			0x330
#define	CLK_ENB_U_CLR			0x334
#define	CCPLEX_PG_SM_OVRD		0x33c
#define	CPU_CMPLX_SET			0x340
#define	RST_DEVICES_V			0x358
#define	RST_DEVICES_W			0x35c
#define	CLK_OUT_ENB_V			0x360
#define	CLK_OUT_ENB_W			0x364
#define	CCLKG_BURST_POLICY		0x368
#define	SUPER_CCLKG_DIVIDER		0x36C
#define	CCLKLP_BURST_POLICY		0x370
#define	SUPER_CCLKLP_DIVIDER		0x374
#define	CLK_CPUG_CMPLX			0x378
#define	CPU_SOFTRST_CTRL		0x380
#define	CPU_SOFTRST_CTRL1		0x384
#define	CPU_SOFTRST_CTRL2		0x388
#define	CLK_SOURCE_MSELECT		0x3b4
#define	CLK_SOURCE_TSENSOR		0x3b8
#define	CLK_SOURCE_I2S4			0x3bc
#define	CLK_SOURCE_I2S5			0x3c0
#define	CLK_SOURCE_I2C4			0x3c4
#define	CLK_SOURCE_AHUB			0x3d0
#define	CLK_SOURCE_HDA2CODEC_2X		0x3e4
#define	CLK_SOURCE_ACTMON		0x3e8
#define	CLK_SOURCE_EXTPERIPH1		0x3ec
#define	CLK_SOURCE_EXTPERIPH2		0x3f0
#define	CLK_SOURCE_EXTPERIPH3		0x3f4
#define	CLK_SOURCE_I2C_SLOW		0x3fc

#define	CLK_SOURCE_SYS			0x400
#define	CLK_SOURCE_ISPB			0x404
#define	CLK_SOURCE_SOR1			0x410
#define	CLK_SOURCE_SOR0			0x414
#define	CLK_SOURCE_SATA_OOB		0x420
#define	CLK_SOURCE_SATA			0x424
#define	CLK_SOURCE_HDA			0x428
#define	RST_DEV_V_SET			0x430
#define	RST_DEV_V_CLR			0x434
#define	RST_DEV_W_SET			0x438
#define	RST_DEV_W_CLR			0x43c
#define	CLK_ENB_V_SET			0x440
#define	CLK_ENB_V_CLR			0x444
#define	CLK_ENB_W_SET			0x448
#define	CLK_ENB_W_CLR			0x44c
#define	RST_CPUG_CMPLX_SET		0x450
#define	RST_CPUG_CMPLX_CLR		0x454
#define	CLK_CPUG_CMPLX_SET		0x460
#define	CLK_CPUG_CMPLX_CLR		0x464
#define	CPU_CMPLX_STATUS		0x470
#define	INTSTATUS			0x478
#define	INTMASK				0x47c
#define	UTMIP_PLL_CFG0			0x480

#define	UTMIP_PLL_CFG1			0x484
#define	 UTMIP_PLL_CFG1_FORCE_PLLU_POWERUP		(1 << 17)
#define	 UTMIP_PLL_CFG1_FORCE_PLLU_POWERDOWN		(1 << 16)
#define	 UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP	(1 << 15)
#define	 UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN	(1 << 14)
#define	 UTMIP_PLL_CFG1_FORCE_PLL_ACTIVE_POWERDOWN 	(1 << 12)
#define	 UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(x)		(((x) & 0x1f) << 6)
#define	 UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)

#define	UTMIP_PLL_CFG2			0x488
#define	 UTMIP_PLL_CFG2_PHY_XTAL_CLOCKEN		(1 << 30)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERUP		(1 << 25)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERDOWN 	(1 << 24)
#define	 UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(x)		(((x) & 0x3f) << 18)
#define	 UTMIP_PLL_CFG2_STABLE_COUNT(x)			(((x) & 0xffff) << 6)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERUP		(1 << 5)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERDOWN 	(1 << 4)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERUP		(1 << 3)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN 	(1 << 2)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERUP		(1 << 1)
#define	 UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN	(1 << 0)

#define	PLLE_AUX			0x48c
#define	 PLLE_AUX_SS_SEQ_INCLUDE			(1U << 31)
#define	 PLLE_AUX_REF_SEL_PLLREFE			(1 << 28)
#define	 PLLE_AUX_SEQ_STATE_GET(x)			(((x) >> 26) & 0x03)
#define	  PLLE_AUX_SEQ_STATE_OFF	 		0
#define	  PLLE_AUX_SEQ_STATE_ON		 		1
#define	  PLLE_AUX_SEQ_STATE_BUSY	 		2
#define	 PLLE_AUX_SEQ_START_STATE 			(1 << 25)
#define	 PLLE_AUX_SEQ_ENABLE				(1 << 24)
#define	 PLLE_AUX_SS_DLY(x)	 			(((x) & 0xFF) << 16)
#define	 PLLE_AUX_SS_LOCK_DLY(x)			(((x) & 0xFF) <<  8)
#define	 PLLE_AUX_SS_TEST_FAST_PT			(1 <<  7)
#define	 PLLE_AUX_SS_SWCTL				(1 <<  6)
#define	 PLLE_AUX_CONFIG_SWCTL				(1 <<  6)
#define	 PLLE_AUX_ENABLE_SWCTL				(1 <<  4)
#define	 PLLE_AUX_USE_LOCKDET				(1 <<  3)
#define	 PLLE_AUX_REF_SRC				(1 <<  2)
#define	 PLLE_AUX_PLLP_CML1_OEN				(1 <<  1)
#define	 PLLE_AUX_PLLP_CML0_OEN				(1 <<  0)

#define	SATA_PLL_CFG0			0x490
#define	SATA_PLL_CFG0_SEQ_START_STATE			(1 << 25)
#define	SATA_PLL_CFG0_SEQ_ENABLE			(1 << 24)
#define	SATA_PLL_CFG0_PADPLL_SLEEP_IDDQ			(1 << 13)
#define	SATA_PLL_CFG0_SEQ_PADPLL_PD_INPUT_VALUE		(1 <<  7)
#define	SATA_PLL_CFG0_SEQ_LANE_PD_INPUT_VALUE		(1 <<  6)
#define	SATA_PLL_CFG0_SEQ_RESET_INPUT_VALUE		(1 <<  5)
#define	SATA_PLL_CFG0_SEQ_IN_SWCTL			(1 <<  4)
#define	SATA_PLL_CFG0_PADPLL_USE_LOCKDET		(1 <<  2)
#define	SATA_PLL_CFG0_PADPLL_RESET_OVERRIDE_VALUE	(1 <<  1)
#define	SATA_PLL_CFG0_PADPLL_RESET_SWCTL		(1 <<  0)

#define	SATA_PLL_CFG1			0x494
#define	PCIE_PLL_CFG			0x498
#define	PCIE_PLL_CFG_SEQ_START_STATE			(1 << 25)
#define	PCIE_PLL_CFG_SEQ_ENABLE				(1 << 24)

#define	PROG_AUDIO_DLY_CLK		0x49c
#define	AUDIO_SYNC_CLK_I2S1		0x4a0
#define	AUDIO_SYNC_CLK_I2S2		0x4a4
#define	AUDIO_SYNC_CLK_I2S3		0x4a8
#define	AUDIO_SYNC_CLK_I2S4		0x4ac
#define	AUDIO_SYNC_CLK_I2S5		0x4b0
#define	AUDIO_SYNC_CLK_SPDIF		0x4b4
#define	PLLD2_BASE			0x4b8
#define	PLLD2_MISC			0x4bc
#define	UTMIP_PLL_CFG3			0x4c0
#define	PLLREFE_BASE			0x4c4
#define	PLLREFE_MISC			0x4c8
#define	PLLREFE_OUT			0x4cc
#define	CPU_FINETRIM_BYP		0x4d0
#define	CPU_FINETRIM_SELECT		0x4d4
#define	CPU_FINETRIM_DR			0x4d8
#define	CPU_FINETRIM_DF			0x4dc
#define	CPU_FINETRIM_F			0x4e0
#define	CPU_FINETRIM_R			0x4e4
#define	PLLC2_BASE			0x4e8
#define	PLLC2_MISC_0			0x4ec
#define	PLLC2_MISC_1			0x4f0
#define	PLLC2_MISC_2			0x4f4
#define	PLLC2_MISC_3			0x4f8
#define	PLLC3_BASE			0x4fc

#define	PLLC3_MISC_0			0x500
#define	PLLC3_MISC_1			0x504
#define	PLLC3_MISC_2			0x508
#define	PLLC3_MISC_3			0x50c
#define	PLLX_MISC_2			0x514
#define	PLLX_MISC_2			0x514
#define	 PLLX_MISC_2_DYNRAMP_STEPB(x)			(((x) & 0xFF) << 24)
#define	 PLLX_MISC_2_DYNRAMP_STEPA(x)			(((x) & 0xFF) << 16)
#define	 PLLX_MISC_2_NDIV_NEW(x)			(((x) & 0xFF) <<  8)
#define	 PLLX_MISC_2_EN_FSTLCK				(1 << 5)
#define	 PLLX_MISC_2_LOCK_OVERRIDE			(1 << 4)
#define	 PLLX_MISC_2_PLL_FREQLOCK			(1 << 3)
#define	 PLLX_MISC_2_DYNRAMP_DONE			(1 << 2)
#define	 PLLX_MISC_2_EN_DYNRAMP				(1 << 0)

#define	PLLX_MISC_3			0x518

#define	XUSBIO_PLL_CFG0			0x51c
#define	 XUSBIO_PLL_CFG0_SEQ_START_STATE		(1 << 25)
#define	 XUSBIO_PLL_CFG0_SEQ_ENABLE			(1 << 24)
#define	 XUSBIO_PLL_CFG0_PADPLL_SLEEP_IDDQ		(1 << 13)
#define	 XUSBIO_PLL_CFG0_PADPLL_USE_LOCKDET		(1 <<  6)
#define	 XUSBIO_PLL_CFG0_CLK_ENABLE_SWCTL		(1 <<  2)
#define	 XUSBIO_PLL_CFG0_PADPLL_RESET_SWCTL		(1 <<  0)

#define	XUSBIO_PLL_CFG1			0x520
#define	PLLE_AUX1			0x524
#define	PLLP_RESHIFT			0x528
#define	UTMIPLL_HW_PWRDN_CFG0		0x52c
#define	 UTMIPLL_HW_PWRDN_CFG0_UTMIPLL_LOCK		(1U << 31)
#define	 UTMIPLL_HW_PWRDN_CFG0_SEQ_START_STATE		(1 << 25)
#define	 UTMIPLL_HW_PWRDN_CFG0_SEQ_ENABLE		(1 << 24)
#define	 UTMIPLL_HW_PWRDN_CFG0_IDDQ_PD_INCLUDE		(1 << 7)
#define	 UTMIPLL_HW_PWRDN_CFG0_USE_LOCKDET		(1 <<  6)
#define	 UTMIPLL_HW_PWRDN_CFG0_SEQ_RESET_INPUT_VALUE	(1 <<  5)
#define	 UTMIPLL_HW_PWRDN_CFG0_SEQ_IN_SWCTL		(1 <<  4)
#define	 UTMIPLL_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL		(1 <<  2)
#define	 UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE		(1 <<  1)
#define	 UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL		(1 <<  0)

#define	PLLU_HW_PWRDN_CFG0		0x530
#define	 PLLU_HW_PWRDN_CFG0_IDDQ_PD_INCLUDE		(1 << 28)
#define	 PLLU_HW_PWRDN_CFG0_SEQ_ENABLE			(1 << 24)
#define	 PLLU_HW_PWRDN_CFG0_USE_SWITCH_DETECT		(1 <<  7)
#define	 PLLU_HW_PWRDN_CFG0_USE_LOCKDET			(1 <<  6)
#define	 PLLU_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL		(1 <<  2)
#define	 PLLU_HW_PWRDN_CFG0_CLK_SWITCH_SWCTL		(1 <<  0)

#define	XUSB_PLL_CFG0			0x534
#define	 XUSB_PLL_CFG0_UTMIPLL_LOCK_DLY			0x3ff
#define	 XUSB_PLL_CFG0_PLLU_LOCK_DLY_MASK		(0x3ff << 14)

#define	CLK_CPU_MISC			0x53c
#define	CLK_CPUG_MISC			0x540
#define	PLLX_HW_CTRL_CFG		0x548
#define	PLLX_SW_RAMP_CFG		0x54c
#define	PLLX_HW_CTRL_STATUS		0x550
#define	SPARE_REG0			0x55c
#define	 SPARE_REG0_MDIV_GET(x)				(((x) >> 2) & 0x03)

#define	AUDIO_SYNC_CLK_DMIC1		0x560
#define	AUDIO_SYNC_CLK_DMIC2		0x564
#define	PLLD2_SS_CFG			0x570
#define	PLLD2_SS_CTRL1			0x574
#define	PLLD2_SS_CTRL2			0x578
#define	PLLDP_BASE			0x590
#define	PLLDP_MISC			0x594
#define	PLLDP_SS_CFG			0x594
#define	PLLDP_SS_CTRL1			0x598
#define	PLLDP_SS_CTRL2			0x5a0
#define	PLLC4_BASE			0x5a4
#define	PLLC4_MISC			0x5a8
#define	SPARE0				0x5c4
#define	SPARE1				0x5c8
#define	GPU_ISOB_CTRL			0x5cc
#define	PLLC_MISC_2			0x5d0
#define	PLLC_MISC_3			0x5d4
#define	PLLA_MISC2			0x5d8
#define	PLLC4_OUT			0x5e4
#define	PLLMB_BASE			0x5e8
#define	PLLMB_MISC1			0x5ec
#define	PLLX_MISC_4			0x5f0
#define	PLLX_MISC_5			0x5f4

#define	CLK_SOURCE_XUSB_CORE_HOST	0x600
#define	CLK_SOURCE_XUSB_FALCON		0x604
#define	CLK_SOURCE_XUSB_FS		0x608
#define	CLK_SOURCE_XUSB_CORE_DEV	0x60c
#define	CLK_SOURCE_XUSB_SS		0x610
#define	CLK_SOURCE_CILAB		0x614
#define	CLK_SOURCE_CILCD		0x618
#define	CLK_SOURCE_CILEF		0x61c
#define	CLK_SOURCE_DSIA_LP		0x620
#define	CLK_SOURCE_DSIB_LP		0x624
#define	CLK_SOURCE_ENTROPY		0x628
#define	CLK_SOURCE_DVFS_REF		0x62c
#define	CLK_SOURCE_DVFS_SOC		0x630
#define	CLK_SOURCE_EMC_LATENCY		0x640
#define	CLK_SOURCE_SOC_THERM		0x644
#define	CLK_SOURCE_DMIC1		0x64c
#define	CLK_SOURCE_DMIC2		0x650
#define	CLK_SOURCE_VI_SENSOR2		0x658
#define	CLK_SOURCE_I2C6			0x65c
#define	CLK_SOURCE_MIPIBIF		0x660
#define	CLK_SOURCE_EMC_DLL		0x664
#define	CLK_SOURCE_UART_FST_MIPI_CAL	0x66c
#define	CLK_SOURCE_VIC			0x678
#define	PLLP_OUTC			0x67c
#define	PLLP_MISC1			0x680
#define	EMC_DIV_CLK_SHAPER_CTRL		0x68c
#define	EMC_PLLC_SHAPER_CTRL		0x690
#define	CLK_SOURCE_SDMMC_LEGACY_TM	0x694
#define	CLK_SOURCE_NVDEC		0x698
#define	CLK_SOURCE_NVJPG		0x69c
#define	CLK_SOURCE_NVENC		0x6a0
#define	PLLA1_BASE			0x6a4
#define	PLLA1_MISC_0			0x6a8
#define	PLLA1_MISC_1			0x6ac
#define	PLLA1_MISC_2			0x6b0
#define	PLLA1_MISC_3			0x6b4
#define	AUDIO_SYNC_CLK_DMIC3		0x6b8
#define	CLK_SOURCE_DMIC3		0x6bc
#define	CLK_SOURCE_APE			0x6c0
#define	CLK_SOURCE_QSPI			0x6c4
#define	CLK_SOURCE_VI_I2C		0x6c8
#define	CLK_SOURCE_USB2_HSIC_TRK	0x6cc
#define	CLK_SOURCE_PEX_SATA_USB_RX_BYP	0x6d0
#define	CLK_SOURCE_MAUD			0x6d4
#define	CLK_SOURCE_TSECB		0x6d8
#define	CLK_CPUG_MISC1			0x6d8
#define	ACLK_BURST_POLICY		0x6e0
#define	SUPER_ACLK_DIVIDER		0x6e4
#define	NVENC_SUPER_CLK_DIVIDER		0x6e8
#define	VI_SUPER_CLK_DIVIDER		0x6ec
#define	VIC_SUPER_CLK_DIVIDER		0x6f0
#define	NVDEC_SUPER_CLK_DIVIDER		0x6f4
#define	ISP_SUPER_CLK_DIVIDER		0x6f8
#define	ISPB_SUPER_CLK_DIVIDER		0x6fc

#define	NVJPG_SUPER_CLK_DIVIDER		0x700
#define	SE_SUPER_CLK_DIVIDER		0x704
#define	TSEC_SUPER_CLK_DIVIDER		0x708
#define	TSECB_SUPER_CLK_DIVIDER		0x70c
#define	CLK_SOURCE_UARTAPE		0x710
#define	CLK_CPUG_MISC2			0x714
#define	CLK_SOURCE_DBGAPB		0x718
#define	CLK_CCPLEX_CC4_RET_CLK_ENB	0x71c
#define	ACTMON_CPU_CLK			0x720
#define	CLK_SOURCE_EMC_SAFE		0x724
#define	SDMMC2_PLLC4_OUT0_SHAPER_CTRL	0x728
#define	SDMMC2_PLLC4_OUT1_SHAPER_CTRL	0x72c
#define	SDMMC2_PLLC4_OUT2_SHAPER_CTRL	0x730
#define	SDMMC2_DIV_CLK_SHAPER_CTRL	0x734
#define	SDMMC4_PLLC4_OUT0_SHAPER_CTRL	0x738
#define	SDMMC4_PLLC4_OUT1_SHAPER_CTRL	0x73c
#define	SDMMC4_PLLC4_OUT2_SHAPER_CTRL	0x740
#define	SDMMC4_DIV_CLK_SHAPER_CTRL	0x744

struct tegra210_car_softc {
	device_t		dev;
	struct resource *	mem_res;
	struct mtx		mtx;
	struct clkdom 		*clkdom;
	int			type;
};

struct tegra210_init_item {
	char 		*name;
	char 		*parent;
	uint64_t	frequency;
	int 		enable;
};

void tegra210_init_plls(struct tegra210_car_softc *sc);

void tegra210_periph_clock(struct tegra210_car_softc *sc);
void tegra210_super_mux_clock(struct tegra210_car_softc *sc);

int tegra210_hwreset_by_idx(struct tegra210_car_softc *sc, intptr_t idx,
    bool reset);

#endif /*_TEGRA210_CAR_*/