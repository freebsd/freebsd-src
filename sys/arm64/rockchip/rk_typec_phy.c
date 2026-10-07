/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.Org>
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
 */

/*
 * Rockchip PHY TYPEC
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/gpio.h>
#include <sys/sysctl.h>
#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/clk/clk.h>
#include <dev/iicbus/usb/fusb302_var.h>
#include <dev/phy/phy_usb.h>
#include <dev/syscon/syscon.h>
#include <dev/hwreset/hwreset.h>

#include "syscon_if.h"

#define	GRF_WRITE_MASK(mask)		((mask) << 16)
#define	GRF_USB3OTG_BASE(x)	(0x2430 + (0x10 * x))
#define	GRF_USB3OTG_CON0(x)	(GRF_USB3OTG_BASE(x) + 0x0)
#define	GRF_USB3OTG_CON1(x)	(GRF_USB3OTG_BASE(x) + 0x4)
#define	 USB3OTG_CON1_U3_DIS	(1 << 0)

#define	GRF_USB3PHY_BASE(x)	(0x0e580 + (0xc * (x)))
#define	GRF_USB3PHY_CON0(x)	(GRF_USB3PHY_BASE(x) + 0x0)
#define	 USB3PHY_CON0_USB2_ONLY	(1 << 3)
#define	GRF_USB3PHY_CON1(x)	(GRF_USB3PHY_BASE(x) + 0x4)
#define	GRF_USB3PHY_CON2(x)	(GRF_USB3PHY_BASE(x) + 0x8)
#define	GRF_USB3PHY_STATUS0	0x0e5c0
#define	GRF_USB3PHY_STATUS1	0x0e5c4

#define	CMN_PLL0_VCOCAL_INIT		(0x84 << 2)
#define	CMN_PLL0_VCOCAL_ITER		(0x85 << 2)
#define	CMN_PLL0_INTDIV			(0x94 << 2)
#define	CMN_PLL0_FRACDIV		(0x95 << 2)
#define	CMN_PLL0_HIGH_THR		(0x96 << 2)
#define	CMN_PLL0_DSM_DIAG		(0x97 << 2)
#define	CMN_PLL0_SS_CTRL1		(0x98 << 2)
#define	CMN_PLL0_SS_CTRL2		(0x99 << 2)
#define	CMN_DIAG_PLL0_FBH_OVRD		(0x1c0 << 2)
#define	CMN_DIAG_PLL0_FBL_OVRD		(0x1c1 << 2)
#define	CMN_DIAG_PLL0_OVRD		(0x1c2 << 2)
#define	CMN_DIAG_PLL0_V2I_TUNE		(0x1c5 << 2)
#define	CMN_DIAG_PLL0_CP_TUNE		(0x1c6 << 2)
#define	CMN_DIAG_PLL0_LF_PROG		(0x1c7 << 2)
/*
 * CMN PLL1 register addresses must match reference vendor BSP exactly — the historical
 * port had these at +0x60 offsets which silently broke every HBR PLL
 * configuration write (writes landed in unrelated registers, leaving PLL1's
 * VCO uncalibrated). Matched against drivers/phy/rockchip/phy-rockchip-typec.c
 * lines 92-104. Verified 2026-05-09 as the EQ-at-HBR root cause.
 */
#define	CMN_PLL1_VCOCAL_START		(0xa1 << 2)
#define	CMN_PLL1_VCOCAL_OVRD		(0xa3 << 2)
#define	CMN_PLL1_VCOCAL_INIT		(0xa4 << 2)
#define	CMN_PLL1_VCOCAL_ITER		(0xa5 << 2)
#define	CMN_PLL1_LOCK_REFCNT_START	(0xb0 << 2)
#define	CMN_PLL1_LOCK_PLLCNT_START	(0xb2 << 2)
#define	CMN_PLL1_LOCK_PLLCNT_THR	(0xb3 << 2)
#define	CMN_PLL1_INTDIV			(0xb4 << 2)
#define	CMN_PLL1_FRACDIV		(0xb5 << 2)
#define	CMN_PLL1_HIGH_THR		(0xb6 << 2)
#define	CMN_PLL1_DSM_DIAG		(0xb7 << 2)
#define	CMN_PLL1_SS_CTRL1		(0xb8 << 2)
#define	CMN_PLL1_SS_CTRL2		(0xb9 << 2)
#define	CMN_PLLSM1_USER_DEF_CTRL	(0x37 << 2)
#define	CMN_PLLSM1_PLLLOCK		(0x34 << 2)	/* PLL1 lock status */
#define	CMN_DIAG_PLL1_FBH_OVRD		(0x1d0 << 2)
#define	CMN_DIAG_PLL1_FBL_OVRD		(0x1d1 << 2)
#define	CMN_DIAG_PLL1_OVRD		(0x1d2 << 2)
#define	CMN_DIAG_PLL1_V2I_TUNE		(0x1d5 << 2)
#define	CMN_DIAG_PLL1_CP_TUNE		(0x1d6 << 2)
#define	CMN_DIAG_PLL1_LF_PROG		(0x1d7 << 2)
#define	CMN_DIAG_PLL1_PTATIS_TUNE1	(0x1d8 << 2)
#define	CMN_DIAG_PLL1_PTATIS_TUNE2	(0x1d9 << 2)
#define	CMN_DIAG_PLL1_INCLK_CTRL	(0x1da << 2)
#define	CMN_DIAG_HSCLK_SEL		(0x1e0 << 2)
#define	 CMN_DIAG_HSCLK_SEL_PLL_CONFIG	0x30
#define	 CMN_DIAG_HSCLK_SEL_PLL_MASK	0x33

#define	XCVR_PSM_RCTRL(lane)		((0x4001 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_000(lane)	((0x4050 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_001(lane)	((0x4051 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_010(lane)	((0x4052 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_011(lane)	((0x4053 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_100(lane)	((0x4054 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_101(lane)	((0x4055 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_110(lane)	((0x4056 | ((lane) << 9)) << 2)
#define	TX_TXCC_MGNFS_MULT_111(lane)	((0x4057 | ((lane) << 9)) << 2)
/*
 * CPOST_MULT_XX register addresses must match reference vendor BSP exactly. The
 * historical port had CPOST at offset 0x405c-0x405f (+0x10 byte offset wrong)
 * AND swapped _00 ↔ _10 labels — every set_signal_levels CPOST_MULT_00 write
 * landed on the wrong register, leaving actual TX pre-emphasis at chip-default
 * regardless of partner adjust_request. Smoking-gun: 2026-05-10 register-poke
 * bitbang trace showed we wrote 0x15 to "CPOST_00" successfully (readback OK
 * at the WRONG address) but link partner never saw the pre-emphasis change,
 * EQ stalled at max-swing/pre_emp request indefinitely.
 *
 * the reference driver phy-rockchip-typec.c:161 / rk3399_tcphy_helper.c:103 use 0x404c.
 */
#define	TX_TXCC_CPOST_MULT_00(lane)	((0x404c | ((lane) << 9)) << 2)
#define	TX_TXCC_CPOST_MULT_01(lane)	((0x404d | ((lane) << 9)) << 2)
#define	TX_TXCC_CPOST_MULT_10(lane)	((0x404e | ((lane) << 9)) << 2)
#define	TX_TXCC_CPOST_MULT_11(lane)	((0x404f | ((lane) << 9)) << 2)
#define	TX_TXCC_CAL_SCLR_MULT(lane)	((0x4047 | ((lane) << 9)) << 2)
#define	XCVR_DIAG_PLLDRC_CTRL(lane)	((0x40e0 | ((lane) << 9)) << 2)
#define	XCVR_DIAG_BIDI_CTRL(lane)	((0x40e8 | ((lane) << 9)) << 2)
#define	XCVR_DIAG_LANE_FCM_EN_MGN(lane)	((0x40f2 | ((lane) << 9)) << 2)
#define	TX_PSC_A0(lane)			((0x4100 | ((lane) << 9)) << 2)
#define	TX_PSC_A1(lane)			((0x4101 | ((lane) << 9)) << 2)
#define	TX_PSC_A2(lane)			((0x4102 | ((lane) << 9)) << 2)
#define	TX_PSC_A3(lane)			((0x4103 | ((lane) << 9)) << 2)
#define	TX_RCVDET_EN_TMR(lane)		((0x4122 | ((lane) << 9)) << 2)
#define	TX_RCVDET_ST_TMR(lane)		((0x4123 | ((lane) << 9)) << 2)
#define	TX_DIAG_TX_DRV(lane)		((0x41e1 | ((lane) << 9)) << 2)

#define	RX_PSC_A0(lane)			((0x8000 | ((lane) << 9)) << 2)
#define	RX_PSC_A1(lane)			((0x8001 | ((lane) << 9)) << 2)
#define	RX_PSC_A2(lane)			((0x8002 | ((lane) << 9)) << 2)
#define	RX_PSC_A3(lane)			((0x8003 | ((lane) << 9)) << 2)
#define	RX_PSC_CAL(lane)		((0x8006 | ((lane) << 9)) << 2)
#define	RX_PSC_RDY(lane)		((0x8007 | ((lane) << 9)) << 2)
#define	RX_SIGDET_HL_FILT_TMR(lane)	((0x8090 | ((lane) << 9)) << 2)
#define	RX_REE_CTRL_DATA_MASK(lane)	((0x81bb | ((lane) << 9)) << 2)
#define	RX_DIAG_SIGDET_TUNE(lane)	((0x81dc | ((lane) << 9)) << 2)

#define	PMA_LANE_CFG			(0xc000 << 2)
#define	PIN_ASSIGN_C_E			0x51d9
#define	PIN_ASSIGN_D_F			0x5100
#define	PIPE_CMN_CTRL1			(0xc001 << 2)
#define	DP_MODE_CTL			(0xc008 << 2)
/*
 * DP_MODE_CTL layout:
 *   bits [3:0]   = mode select (1=A0, 4=A2, 8=A3)
 *   bits [7:4]   = mode-ready ACKs (1=A0_ready, 4=A2_ready)
 *   bit  [8]     = DP_LINK_RESET_DEASSERTED
 *   bits [15:12] = PHY_DP_LANE_x_DISABLE (set bit = power down that DP lane)
 *
 * For 4-lane DP we MUST clear bits [15:12] so all four lanes are powered.
 * Previous values 0xc104/0xc101 had bits 14+15 set, which disabled lanes 2+3
 * — the PHY came up as 2-lane while we told the firmware lanes=4. The
 * firmware then tried to drive a 4-lane link against a 2-lane-powered PHY,
 * couldn't complete AUX, and crashed inside its own AUX engine. Symptom was
 * "READ_DPCD mailbox_send accepted, then firmware never replied / KEEP_ALIVE
 * frozen".  Read-modify-write only the mode bits [3:0] and leave 12-15
 * cleared.
 */
#define	DP_LINK_RESET_DEASSERTED	(1U << 8)
#define	DP_LANE_0_DISABLE		(1U << 12)
#define	DP_LANE_1_DISABLE		(1U << 13)
#define	DP_LANE_2_DISABLE		(1U << 14)
#define	DP_LANE_3_DISABLE		(1U << 15)
#define	DP_LANE_DISABLE_MASK		(0xfU << 12)
#define	DP_CLK_CTL			(0xc009 << 2)
#define	 DP_PLL_CLOCK_ENABLE		(1U << 2)
#define	 DP_PLL_CLOCK_DISABLE		(0U << 2)
#define	 DP_PLL_CLOCK_ENABLE_MASK	(1U << 2)
#define	 DP_PLL_CLOCK_ENABLE_ACK	(1U << 3)
#define	 DP_PLL_ENABLE			(1U << 0)
#define	 DP_PLL_DISABLE			(0U << 0)
#define	 DP_PLL_ENABLE_MASK		(1U << 0)
#define	 DP_PLL_READY			(1U << 1)
#define	 DP_PLL_DATA_RATE_RBR		((2U << 12) | (4U << 8))
/*
 * Mode-select bits within DP_MODE_CTL[3:0]. Use these with a
 * read-modify-write that preserves bits 4+ (especially 12-15, which on
 * this RK3399 silicon are NOT host-controllable lane disable bits but
 * functional reset-state bits the PMA needs preserved — clobbering them
 * to zero leaves PMA_CMN_CTRL1.READY stuck at 0 and dp-init times out).
 */
#define	DP_MODE_MASK			0xf
#define	DP_MODE_ENTER_A0_BITS		(1U << 0)
#define	DP_MODE_ENTER_A2_BITS		(1U << 2)
#define	DP_MODE_ENTER_A3_BITS		(1U << 3)
#define	 DP_MODE_A0_READY		(1U << 4)
#define	 DP_MODE_A2_READY		(1U << 6)
#define	PMA_CMN_CTRL1			(0xc800 << 2)
#define	 PMA_CMN_CTRL1_READY		(1 << 0)

/* AUX channel calibration registers (within the same TC-PHY MMIO region). */
#define	CMN_TXPUCAL_CTRL		(0x00e0 << 2)
#define	CMN_TXPDCAL_CTRL		(0x00f0 << 2)
#define	CMN_TXPU_ADJ_CTRL		(0x0108 << 2)
#define	CMN_TXPD_ADJ_CTRL		(0x010c << 2)
#define	PHY_DP_TX_CTL			(0xc408 << 2)
#define	TX_ANA_CTRL_REG_1		(0x5020 << 2)
#define	TX_ANA_CTRL_REG_2		(0x5021 << 2)
#define	TXDA_COEFF_CALC_CTRL		(0x5022 << 2)
#define	TX_DIG_CTRL_REG_2		(0x5024 << 2)
#define	TXDA_CYA_AUXDA_CYA		(0x5025 << 2)
#define	TX_ANA_CTRL_REG_3		(0x5026 << 2)
#define	TX_ANA_CTRL_REG_4		(0x5027 << 2)
#define	TX_ANA_CTRL_REG_5		(0x5029 << 2)
#define	AUX_CH_LANE			8
#define	TXDA_DP_AUX_EN			(1U << 15)
#define	TXDA_CAL_LATCH_EN		(1U << 13)
#define	AUXDA_POLARITY			(1U << 12)
#define	TXDA_BGREF_EN			(1U << 8)
#define	TXDA_DRV_LDO_EN			(1U << 7)
#define	TXDA_DECAP_EN_DEL		(1U << 6)
#define	TXDA_DECAP_EN			(1U << 5)
#define	TXDA_UPHY_SUPPLY_EN_DEL		(1U << 4)
#define	TXDA_UPHY_SUPPLY_EN		(1U << 3)
#define	XCVR_DECAP_EN_DEL		(1U << 9)
#define	XCVR_DECAP_EN			(1U << 8)
#define	TXDA_DRV_PREDRV_EN_DEL		(1U << 1)
#define	TXDA_DRV_PREDRV_EN		(1U << 0)
#define	TX_HIGH_Z_TM_EN			(1U << 15)
#define	TX_RESCAL_CODE_MASK		0x3f

struct rk_typec_phy_reg {
	uint16_t	value;
	uint32_t	addr;
};

struct rk_typec_phy_grf_prop {
	uint32_t	reg;
	uint32_t	lsb;
	uint32_t	msb;
};

static const struct rk_typec_phy_grf_prop rk3399_tcphy0_conn_dir = {
	.reg = 0x0e580,
	.lsb = 0,
	.msb = 0,
};
/* Selects external PSM clock source (from CDN-DP); required for PSM to run A2/A0. */
static const struct rk_typec_phy_grf_prop rk3399_tcphy0_external_psm = {
	.reg = 0x0e588,
	.lsb = 14,
	.msb = 14,
};
static const struct rk_typec_phy_grf_prop rk3399_tcphy0_uphy_dp_sel = {
	.reg = 0x6268,
	.lsb = 19,
	.msb = 19,
};

static const struct rk_typec_phy_reg __unused rk3399_tcphy_dp_pll_cfg[] = {
	{ 0xf0,	CMN_PLL1_VCOCAL_INIT },
	{ 0x18,	CMN_PLL1_VCOCAL_ITER },
	{ 0x30b9,	CMN_PLL1_VCOCAL_START },
	{ 0x21c,	CMN_PLL1_INTDIV },
	{ 0x0,		CMN_PLL1_FRACDIV },
	{ 0x5,		CMN_PLL1_HIGH_THR },
	{ 0x35,	CMN_PLL1_SS_CTRL1 },
	{ 0x7f1e,	CMN_PLL1_SS_CTRL2 },
	{ 0x20,	CMN_PLL1_DSM_DIAG },
	{ 0x0,		CMN_PLLSM1_USER_DEF_CTRL },
	{ 0x0,		CMN_DIAG_PLL1_OVRD },
	{ 0x0,		CMN_DIAG_PLL1_FBH_OVRD },
	{ 0x0,		CMN_DIAG_PLL1_FBL_OVRD },
	{ 0x6,		CMN_DIAG_PLL1_V2I_TUNE },
	{ 0x45,	CMN_DIAG_PLL1_CP_TUNE },
	{ 0x8,		CMN_DIAG_PLL1_LF_PROG },
	{ 0x100,	CMN_DIAG_PLL1_PTATIS_TUNE1 },
	{ 0x7,		CMN_DIAG_PLL1_PTATIS_TUNE2 },
	{ 0x4,		CMN_DIAG_PLL1_INCLK_CTRL },
};

/*
 * Link-rate-specific PLL configs (RBR/HBR/HBR2).
 * rk_typec_phy_dp_set_link_rate writes one of these after gating clocks
 * and disabling the PLL, then re-enables.
 */
static const struct rk_typec_phy_reg rk3399_tcphy_dp_pll_rbr_cfg[] = {
	{ 0x00f0,	CMN_PLL1_VCOCAL_INIT },
	{ 0x0018,	CMN_PLL1_VCOCAL_ITER },
	{ 0x30b9,	CMN_PLL1_VCOCAL_START },
	{ 0x0087,	CMN_PLL1_INTDIV },
	{ 0x0000,	CMN_PLL1_FRACDIV },
	{ 0x0022,	CMN_PLL1_HIGH_THR },
	{ 0x8000,	CMN_PLL1_SS_CTRL1 },
	{ 0x0000,	CMN_PLL1_SS_CTRL2 },
	{ 0x0020,	CMN_PLL1_DSM_DIAG },
	{ 0x0000,	CMN_PLLSM1_USER_DEF_CTRL },
	{ 0x0000,	CMN_DIAG_PLL1_OVRD },
	{ 0x0000,	CMN_DIAG_PLL1_FBH_OVRD },
	{ 0x0000,	CMN_DIAG_PLL1_FBL_OVRD },
	{ 0x0006,	CMN_DIAG_PLL1_V2I_TUNE },
	{ 0x0045,	CMN_DIAG_PLL1_CP_TUNE },
	{ 0x0008,	CMN_DIAG_PLL1_LF_PROG },
	{ 0x0100,	CMN_DIAG_PLL1_PTATIS_TUNE1 },
	{ 0x0007,	CMN_DIAG_PLL1_PTATIS_TUNE2 },
	{ 0x0001,	CMN_DIAG_PLL1_INCLK_CTRL },
};
static const struct rk_typec_phy_reg rk3399_tcphy_dp_pll_hbr_cfg[] = {
	{ 0x00f0,	CMN_PLL1_VCOCAL_INIT },
	{ 0x0018,	CMN_PLL1_VCOCAL_ITER },
	{ 0x30b4,	CMN_PLL1_VCOCAL_START },
	{ 0x00e1,	CMN_PLL1_INTDIV },
	{ 0x0000,	CMN_PLL1_FRACDIV },
	{ 0x0005,	CMN_PLL1_HIGH_THR },
	{ 0x8000,	CMN_PLL1_SS_CTRL1 },
	{ 0x0000,	CMN_PLL1_SS_CTRL2 },
	{ 0x0020,	CMN_PLL1_DSM_DIAG },
	{ 0x1000,	CMN_PLLSM1_USER_DEF_CTRL },
	{ 0x0000,	CMN_DIAG_PLL1_OVRD },
	{ 0x0000,	CMN_DIAG_PLL1_FBH_OVRD },
	{ 0x0000,	CMN_DIAG_PLL1_FBL_OVRD },
	{ 0x0007,	CMN_DIAG_PLL1_V2I_TUNE },
	{ 0x0045,	CMN_DIAG_PLL1_CP_TUNE },
	{ 0x0008,	CMN_DIAG_PLL1_LF_PROG },
	{ 0x0001,	CMN_DIAG_PLL1_PTATIS_TUNE1 },
	{ 0x0001,	CMN_DIAG_PLL1_PTATIS_TUNE2 },
	{ 0x0001,	CMN_DIAG_PLL1_INCLK_CTRL },
};
static const struct rk_typec_phy_reg rk3399_tcphy_dp_pll_hbr2_cfg[] = {
	{ 0x00f0,	CMN_PLL1_VCOCAL_INIT },
	{ 0x0018,	CMN_PLL1_VCOCAL_ITER },
	{ 0x30b4,	CMN_PLL1_VCOCAL_START },
	{ 0x00e1,	CMN_PLL1_INTDIV },
	{ 0x0000,	CMN_PLL1_FRACDIV },
	{ 0x0005,	CMN_PLL1_HIGH_THR },
	{ 0x8000,	CMN_PLL1_SS_CTRL1 },
	{ 0x0000,	CMN_PLL1_SS_CTRL2 },
	{ 0x0020,	CMN_PLL1_DSM_DIAG },
	{ 0x1000,	CMN_PLLSM1_USER_DEF_CTRL },
	{ 0x0000,	CMN_DIAG_PLL1_OVRD },
	{ 0x0000,	CMN_DIAG_PLL1_FBH_OVRD },
	{ 0x0000,	CMN_DIAG_PLL1_FBL_OVRD },
	{ 0x0007,	CMN_DIAG_PLL1_V2I_TUNE },
	{ 0x0045,	CMN_DIAG_PLL1_CP_TUNE },
	{ 0x0008,	CMN_DIAG_PLL1_LF_PROG },
	{ 0x0001,	CMN_DIAG_PLL1_PTATIS_TUNE1 },
	{ 0x0001,	CMN_DIAG_PLL1_PTATIS_TUNE2 },
	{ 0x0001,	CMN_DIAG_PLL1_INCLK_CTRL },
};

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3399-typec-phy",	1 },
	{ NULL,				0 }
};

static struct resource_spec rk_typec_phy_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct rk_typec_phy_softc {
	device_t		dev;
	struct resource		*res;
	struct syscon		*grf;
	device_t		typec_dev;
	clk_t			tcpdcore;
	clk_t			tcpdphy_ref;
	hwreset_t		rst_uphy;
	hwreset_t		rst_pipe;
	hwreset_t		rst_tcphy;
	int			mode;
	phy_mode_t		dp_mode;
	phy_submode_t		dp_submode;
	int			phy_ctrl_id;
	bool			flip;
	int			flip_override; /* -1=auto, 0=CC1, 1=CC2 */
	bool			init_done;
	bool			init_flip;	/* sc->flip value at last full init */
};

#define	RK_TYPEC_PHY_READ(sc, reg)		bus_read_4(sc->res, (reg))
#define	RK_TYPEC_PHY_WRITE(sc, reg, val)	bus_write_4(sc->res, (reg), (val))

/* Phy class and methods. */
static int rk_typec_phy_enable(struct phynode *phynode, bool enable);
static int rk_typec_phy_get_mode(struct phynode *phy, int *mode);
static int rk_typec_phy_set_mode(struct phynode *phy, int mode);
static int rk_typec_phy_set_phy_mode(struct phynode *phy, phy_mode_t mode,
    phy_submode_t submode);
static phynode_method_t rk_typec_phy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,		rk_typec_phy_enable),
	PHYNODEMETHOD(phynode_set_mode,		rk_typec_phy_set_phy_mode),
	PHYNODEMETHOD(phynode_usb_get_mode,	rk_typec_phy_get_mode),
	PHYNODEMETHOD(phynode_usb_set_mode,	rk_typec_phy_set_mode),

	PHYNODEMETHOD_END
};

DEFINE_CLASS_1(rk_typec_phy_phynode, rk_typec_phy_phynode_class,
    rk_typec_phy_phynode_methods,
    sizeof(struct phynode_usb_sc), phynode_usb_class);

enum RK3399_USBPHY {
	RK3399_TYPEC_PHY_DP = 0,
	RK3399_TYPEC_PHY_USB3,
};

static int
rk_typec_phy_lookup_typec_status_cb(linker_file_t lf, void *arg)
{
	caddr_t sym;

	sym = linker_file_lookup_symbol(lf, "fusb302_get_typec_status", 0);
	if (sym == 0)
		return (0);

	*(caddr_t *)arg = sym;
	return (1);
}

static int
(*rk_typec_phy_lookup_typec_status(void))(device_t, struct fusb302_typec_status *)
{
	caddr_t sym;

	sym = 0;
	(void)linker_file_foreach(rk_typec_phy_lookup_typec_status_cb, &sym);
	return ((int (*)(device_t, struct fusb302_typec_status *))sym);
}

static bool
rk_typec_phy_update_flip(struct rk_typec_phy_softc *sc)
{
	int (*get_status)(device_t, struct fusb302_typec_status *);
	struct fusb302_typec_status status;
	devclass_t dc;
	device_t dev;

	if (sc->typec_dev == NULL) {
		dc = devclass_find("fusb302");
		if (dc != NULL)
			sc->typec_dev = devclass_get_device(dc, 0);
	}
	dev = sc->typec_dev;
	if (dev == NULL)
		return (false);

	get_status = rk_typec_phy_lookup_typec_status();
	if (get_status == NULL)
		return (false);
	if (get_status(dev, &status) != 0 || !status.state_valid)
		return (false);

	sc->flip = (status.orientation == FUSB302_TYPEC_ORIENT_CC2);
	return (true);
}

static int
rk_typec_phy_set_field(struct rk_typec_phy_softc *sc,
    const struct rk_typec_phy_grf_prop *prop, uint32_t value)
{
	uint32_t field_mask, regval;

	field_mask = ((1u << (prop->msb - prop->lsb + 1)) - 1u) << prop->lsb;
	regval = ((value << prop->lsb) & field_mask) | GRF_WRITE_MASK(field_mask);
	return (SYSCON_WRITE_4(sc->grf, prop->reg, regval));
}

static int
rk_typec_phy_apply_dp_grf(struct rk_typec_phy_softc *sc)
{
	if (sc->phy_ctrl_id != 0)
		return (0);

	return (rk_typec_phy_set_field(sc, &rk3399_tcphy0_conn_dir,
	    sc->flip ? 1 : 0));
}

static int
rk_typec_phy_enable_dp_sel(struct rk_typec_phy_softc *sc)
{
	if (sc->phy_ctrl_id != 0)
		return (0);

	return (rk_typec_phy_set_field(sc, &rk3399_tcphy0_uphy_dp_sel, 1));
}

static void
rk_typec_phy_dp_aux_set_flip(struct rk_typec_phy_softc *sc)
{
	uint32_t reg;
	bool aux_flip;

	/*
	 * flip_override only controls AUXDA_POLARITY, not conn_dir.
	 * Setting conn_dir=1 (for CC2) via rk_typec_phy_apply_dp_grf breaks
	 * the A2 PSM sequence, so leave sc->flip/conn_dir at its auto-detected
	 * value and only override the AUX signal polarity here.
	 */
	aux_flip = (sc->flip_override >= 0) ? (sc->flip_override != 0) : sc->flip;
	reg = RK_TYPEC_PHY_READ(sc, TX_ANA_CTRL_REG_1);
	/*
	 * Empirically on this hardware: with AUX_SWAP_INVERSION_CONTROL=0x3
	 * (RK_CDN_DP_AUX_HOST_INVERT) programmed by the firmware, the PHY's
	 * AUXDA_POLARITY must be SET on CC2 (flip=1) and CLEAR on CC1.  This
	 * is the OPPOSITE of what an upstream-style implementation would do:
	 * with the firmware-side AUX_SWAP=0x3 also applied for CC2, the two
	 * polarities compensate so the wire net-direction is correct only
	 * with this combination.  Verified 2026-05-02: with this code
	 * DPCD ACKs and EDID reads, with the inverse AUX times out.
	 */
	if (aux_flip)
		reg |= AUXDA_POLARITY;
	else
		reg &= ~AUXDA_POLARITY;
	device_printf(sc->dev, "dp-aux-flip: override=%d aux_flip=%s AUXDA_POLARITY=%s\n",
	    sc->flip_override, aux_flip ? "CC2" : "CC1",
	    aux_flip ? "set" : "cleared");
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, reg);
}

static void
rk_typec_phy_cfg_dp_pll(struct rk_typec_phy_softc *sc)
{
	size_t i;
	uint32_t hsclk;

	/*
	 * the reference driver's tcphy_cfg_dp_pll(tcphy, DP_DEFAULT_RATE=162000) loads the
	 * RBR PLL config at init.  We previously loaded a "base" config with
	 * INTDIV=0x21c that doesn't correspond to any standard DP rate;
	 * switching from there to HBR via tcphy_dp_set_link_rate would fail
	 * because PLL_VCOCAL_START / INTDIV mismatched the rate.  Match
	 * the reference driver: set DATA_RATE_RBR, HSCLK_SEL_PLL1_DIV2, and load the RBR
	 * cfg table — set_link_rate transitions cleanly from RBR.
	 */
	RK_TYPEC_PHY_WRITE(sc, DP_CLK_CTL, DP_PLL_CLOCK_ENABLE |
	    DP_PLL_ENABLE | DP_PLL_DATA_RATE_RBR);

	hsclk = RK_TYPEC_PHY_READ(sc, CMN_DIAG_HSCLK_SEL);
	hsclk &= ~((0x3U << 4) | (0x3U << 0));
	hsclk |= (3U << 4) | (0U << 0); /* CLK_PLL1_DIV2 (matches the reference driver RBR) */
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_HSCLK_SEL, hsclk);

	for (i = 0; i < nitems(rk3399_tcphy_dp_pll_rbr_cfg); i++)
		RK_TYPEC_PHY_WRITE(sc, rk3399_tcphy_dp_pll_rbr_cfg[i].addr,
		    rk3399_tcphy_dp_pll_rbr_cfg[i].value);
}

static void
rk_typec_phy_cfg_dp_lane(struct rk_typec_phy_softc *sc, u_int lane)
{
	uint32_t reg;

	RK_TYPEC_PHY_WRITE(sc, XCVR_PSM_RCTRL(lane), 0xbefc);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A0(lane), 0x6799);	/* DP lane; USB3 uses 0x7799 */
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A1(lane), 0x6798);	/* DP lane; USB3 uses 0x7798 */
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A2(lane), 0x98);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A3(lane), 0x98);

	RK_TYPEC_PHY_WRITE(sc, TX_TXCC_MGNFS_MULT_000(lane), 0x0);
	/*
	 * Do NOT pre-zero MGNFS_MULT_001..111 or CPOST_MULT_00..11 here.
	 * Before the 2026-05-10 CPOST address fix (0x405c -> 0x404c) these
	 * zero writes landed on unmapped addresses (no-op). After the fix
	 * they correctly target the analog drive multiplier registers, and
	 * zeroing them at init clobbers chip defaults the AUX path needs —
	 * hangs the dptx firmware after set_host_cap. set_voltages writes
	 * MGNFS_000 + CPOST_00 per train iteration anyway; leave the rest
	 * at chip default. reference vendor BSP tcphy_dp_cfg_lane writes none of
	 * these.
	 */

	RK_TYPEC_PHY_WRITE(sc, TX_TXCC_CAL_SCLR_MULT(lane), 0x128);
	RK_TYPEC_PHY_WRITE(sc, TX_DIAG_TX_DRV(lane), 0x400);

	reg = RK_TYPEC_PHY_READ(sc, XCVR_DIAG_PLLDRC_CTRL(lane));
	reg = (reg & 0x8fff) | 0x6000;
	RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_PLLDRC_CTRL(lane), reg);
}

/*
 * Per-lane USB3 TX configuration. Matches reference vendor BSP
 * `tcphy_tx_usb3_cfg_lane`. Used for the lanes that carry USB3 traffic in
 * PIN_D/F mode (the other pair carries DP).
 */
static void
rk_typec_phy_cfg_usb3_tx_lane(struct rk_typec_phy_softc *sc, u_int lane)
{
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A0(lane), 0x7799);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A1(lane), 0x7798);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A2(lane), 0x5098);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A3(lane), 0x5098);
	RK_TYPEC_PHY_WRITE(sc, TX_TXCC_MGNFS_MULT_000(lane), 0x0);
	RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_BIDI_CTRL(lane), 0xbf);
}

/*
 * Per-lane USB3 RX configuration. Matches reference vendor BSP
 * `tcphy_rx_usb3_cfg_lane`. Used for the lanes that carry USB3 traffic in
 * PIN_D/F mode.
 */
static void
rk_typec_phy_cfg_usb3_rx_lane(struct rk_typec_phy_softc *sc, u_int lane)
{
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A0(lane), 0xa6fd);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A1(lane), 0xa6fd);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A2(lane), 0xa410);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A3(lane), 0x2410);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_CAL(lane), 0x23ff);
	RK_TYPEC_PHY_WRITE(sc, RX_SIGDET_HL_FILT_TMR(lane), 0x13);
	RK_TYPEC_PHY_WRITE(sc, RX_REE_CTRL_DATA_MASK(lane), 0x03e7);
	RK_TYPEC_PHY_WRITE(sc, RX_DIAG_SIGDET_TUNE(lane), 0x1004);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_RDY(lane), 0x2010);
	RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_BIDI_CTRL(lane), 0xfb);
}

static void
rk_typec_phy_set_usb2_only(struct rk_typec_phy_softc *sc, bool usb2only)
{
	uint32_t reg;

	/* Disable usb3tousb2 only */
	reg = SYSCON_READ_4(sc->grf, GRF_USB3PHY_CON0(sc->phy_ctrl_id));
	if (usb2only)
		reg |= USB3PHY_CON0_USB2_ONLY;
	else
		reg &= ~USB3PHY_CON0_USB2_ONLY;
	/* Write Mask */
	reg |= (USB3PHY_CON0_USB2_ONLY) << 16;
	SYSCON_WRITE_4(sc->grf, GRF_USB3PHY_CON0(sc->phy_ctrl_id), reg);

	/* Enable the USB3 Super Speed port */
	reg = SYSCON_READ_4(sc->grf, GRF_USB3OTG_CON1(sc->phy_ctrl_id));
	if (usb2only)
		reg |= USB3OTG_CON1_U3_DIS;
	else
		reg &= ~USB3OTG_CON1_U3_DIS;
	/* Write Mask */
	reg |= (USB3OTG_CON1_U3_DIS) << 16;
	SYSCON_WRITE_4(sc->grf, GRF_USB3OTG_CON1(sc->phy_ctrl_id), reg);
}

/*
 * rk_typec_phy_dp_aux_calibration
 *
 * Programs the AUX channel analog section using values derived from the PHY's
 * built-in PU/PD calibration registers.  Must be called after the PMA PLL has
 * locked (PMA_CMN_CTRL1_READY) and before requesting the A0 state transition.
 * Programs CMN_TXPUCAL/PDCAL/PUADJ/PDADJ-derived analog calibration into
 * the AUX TX channel.
 */
static void
rk_typec_phy_dp_aux_calibration(struct rk_typec_phy_softc *sc)
{
	uint32_t tx1, tx2, val;
	int pu_calib, pd_calib, pu_adj, pd_adj, calib;

	pu_calib = (int)(RK_TYPEC_PHY_READ(sc, CMN_TXPUCAL_CTRL) & TX_RESCAL_CODE_MASK);
	pd_calib = (int)(RK_TYPEC_PHY_READ(sc, CMN_TXPDCAL_CTRL) & TX_RESCAL_CODE_MASK);
	pu_adj   = (int)(int8_t)(RK_TYPEC_PHY_READ(sc, CMN_TXPU_ADJ_CTRL) & 0xff);
	pd_adj   = (int)(int8_t)(RK_TYPEC_PHY_READ(sc, CMN_TXPD_ADJ_CTRL) & 0xff);
	calib = (pu_calib + pd_calib) / 2 + pu_adj + pd_adj;
	if (calib < 0)
		calib = 0;
	if (calib > TX_RESCAL_CODE_MASK)
		calib = TX_RESCAL_CODE_MASK;

	tx1 = RK_TYPEC_PHY_READ(sc, TX_ANA_CTRL_REG_1);
	tx1 &= ~TXDA_CAL_LATCH_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);

	val = RK_TYPEC_PHY_READ(sc, TX_DIG_CTRL_REG_2);
	val &= ~TX_RESCAL_CODE_MASK;
	val |= (uint32_t)calib;
	RK_TYPEC_PHY_WRITE(sc, TX_DIG_CTRL_REG_2, val);
	DELAY(10000);

	tx1 |= TXDA_CAL_LATCH_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);
	DELAY(200);

	RK_TYPEC_PHY_WRITE(sc, PHY_DP_TX_CTL, 0);

	tx2 = XCVR_DECAP_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_2, tx2);
	DELAY(1);
	tx2 |= XCVR_DECAP_EN_DEL;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_2, tx2);

	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_3, 0);

	tx1 |= TXDA_UPHY_SUPPLY_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);
	DELAY(1);
	tx1 |= TXDA_UPHY_SUPPLY_EN_DEL;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);

	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_5, 0);
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_4, 0x1001);

	tx1 |= TXDA_DRV_LDO_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);
	DELAY(5);
	tx1 |= TXDA_BGREF_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);

	tx2 |= TXDA_DRV_PREDRV_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_2, tx2);
	DELAY(1);
	tx2 |= TXDA_DRV_PREDRV_EN_DEL;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_2, tx2);

	tx1 |= TXDA_DP_AUX_EN | TXDA_DECAP_EN;
	tx1 &= ~TXDA_DRV_LDO_EN;
	tx1 &= ~TXDA_BGREF_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);
	DELAY(1);
	tx1 |= TXDA_DECAP_EN_DEL;
	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_1, tx1);

	RK_TYPEC_PHY_WRITE(sc, TX_ANA_CTRL_REG_4, 0);
	RK_TYPEC_PHY_WRITE(sc, TXDA_COEFF_CALC_CTRL, 0);
	RK_TYPEC_PHY_WRITE(sc, TXDA_CYA_AUXDA_CYA, 0);
	RK_TYPEC_PHY_WRITE(sc, TX_TXCC_CAL_SCLR_MULT(AUX_CH_LANE), 0x128);

	val = RK_TYPEC_PHY_READ(sc, TX_DIG_CTRL_REG_2);
	val |= TX_HIGH_Z_TM_EN;
	RK_TYPEC_PHY_WRITE(sc, TX_DIG_CTRL_REG_2, val);
}

/*
 * Per-lane TX signal-level table indexed [voltage_swing][pre_emphasis].
 * 3 voltage-swing levels × 4 pre-emphasis levels.
 * Slots with swing=0 pe=0 are unreachable combinations per DP spec.
 */
struct rk_typec_phy_signal_cfg {
	uint16_t swing;
	uint16_t pe;
};
static const struct rk_typec_phy_signal_cfg
rk_typec_phy_signal_table[3][4] = {
	{ { 0x2a, 0x00 }, { 0x1f, 0x15 }, { 0x14, 0x22 }, { 0x02, 0x2b } },
	{ { 0x21, 0x00 }, { 0x12, 0x15 }, { 0x02, 0x22 }, { 0,    0    } },
	{ { 0x15, 0x00 }, { 0x00, 0x15 }, { 0,    0    }, { 0,    0    } },
};

/*
 * rk_typec_phy_dp_set_signal_levels
 *
 * Programs the per-lane TX swing/pre-emphasis registers from the table for the
 * given voltage_swing/pre_emp levels.
 * Called from rk_cdn_dp link training between DPCD writes when ADJUST_REQUEST
 * asks for new levels.
 *
 * `dev`        : rk_typec_phy device (lookup via phynode_get_device).
 * `link_rate`  : DP link rate in kHz (270000 = HBR, 540000 = HBR2).
 * `lane_count` : 1, 2, or 4.
 * `swing`      : voltage swing level, 0..2.
 * `pre_emp`    : pre-emphasis level, 0..3.
 *
 * Exported (non-static) so rk_cdn_dp.ko can resolve via the kernel linker.
 */
int rk_typec_phy_dp_set_signal_levels(device_t dev, int link_rate,
    int lane_count, uint8_t swing, uint8_t pre_emp);
int rk_typec_phy_dp_set_signal_levels_first(int link_rate, int lane_count,
    uint8_t swing, uint8_t pre_emp);
int rk_typec_phy_dp_set_link_rate(device_t dev, int link_rate, bool ssc_on);
int rk_typec_phy_dp_set_link_rate_first(int link_rate, bool ssc_on);
int rk_typec_phy_dp_set_lane_count(device_t dev, int lane_count);
int rk_typec_phy_dp_set_lane_count_first(int lane_count);
int rk_typec_phy_dp_refresh_orientation_first(void);

static struct rk_typec_phy_softc *rk_typec_phy_first_softc(void);

/*
 * tcphy_dp_set_power_state equivalent: drives DP_MODE_CTL to request
 * A0/A2/A3 and waits for the corresponding READY bit to assert.
 *  state encoding (DP_MODE_CTL[3:0]): 1=A0, 2=A1, 4=A2, 8=A3
 *  ready bits  (DP_MODE_CTL[7:4]): 1=A0_ready, 2=A1, 4=A2_ready, 8=A3_ready
 */
static int
rk_typec_phy_dp_set_power_state(struct rk_typec_phy_softc *sc,
    int state)
{
	uint32_t reg, want_state, want_ack;
	int retry;

	switch (state) {
	case 0: want_state = 1U << 0; break;	/* A0 */
	case 2: want_state = 1U << 2; break;	/* A2 */
	case 3: want_state = 1U << 3; break;	/* A3 */
	default: return (EINVAL);
	}
	want_ack = want_state << 4;

	reg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
	reg &= ~DP_MODE_MASK;
	reg |= want_state | DP_LINK_RESET_DEASSERTED;
	RK_TYPEC_PHY_WRITE(sc, DP_MODE_CTL, reg);

	for (retry = 10000; retry > 0; retry--) {
		reg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
		if (reg & want_ack)
			return (0);
		DELAY(10);
	}
	device_printf(sc->dev,
	    "set_power_state A%d timeout: DP_MODE_CTL=0x%x\n", state, reg);
	return (ETIMEDOUT);
}

/*
 * Switch the PHY's PLL to the requested DP link rate.
 * Sequence: A3 → gate clocks → disable PLL → load rate-specific PLL
 * table → enable PLL → enable clocks → A2 → A0.
 *
 * link_rate is in kHz: 162000 (RBR), 270000 (HBR), 540000 (HBR2).
 * ssc_on is honored for table selection but currently we only carry
 * non-SSC tables; pass false to avoid surprises.
 */
int
rk_typec_phy_dp_set_link_rate(device_t dev, int link_rate, bool ssc_on)
{
	struct rk_typec_phy_softc *sc;
	const struct rk_typec_phy_reg *cfg;
	uint32_t cmn_diag_hsclk_sel, phy_dp_clk_ctl, reg;
	size_t cfg_size, i;
	int err, retry;

	if (dev == NULL)
		return (EINVAL);
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);

	err = rk_typec_phy_dp_set_power_state(sc, 3);
	if (err != 0)
		return (err);

	/* Gate the PLL clocks from PMA. */
	reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
	reg &= ~DP_PLL_CLOCK_ENABLE_MASK;
	reg |= DP_PLL_CLOCK_DISABLE;
	RK_TYPEC_PHY_WRITE(sc, DP_CLK_CTL, reg);
	for (retry = 10000; retry > 0; retry--) {
		reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
		if (!(reg & DP_PLL_CLOCK_ENABLE_ACK))
			break;
		DELAY(10);
	}
	if (retry == 0) {
		device_printf(sc->dev, "set_link_rate: gate clocks timeout\n");
		return (ETIMEDOUT);
	}

	/* Disable the PLL. */
	reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
	reg &= ~DP_PLL_ENABLE_MASK;
	reg |= DP_PLL_DISABLE;
	RK_TYPEC_PHY_WRITE(sc, DP_CLK_CTL, reg);
	for (retry = 10000; retry > 0; retry--) {
		reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
		if (!(reg & DP_PLL_READY))
			break;
		DELAY(10);
	}
	if (retry == 0) {
		device_printf(sc->dev,
		    "set_link_rate: PLL disable timeout\n");
		return (ETIMEDOUT);
	}

	cmn_diag_hsclk_sel = RK_TYPEC_PHY_READ(sc, CMN_DIAG_HSCLK_SEL);
	cmn_diag_hsclk_sel &= ~((0x3U << 4) | (0x3U << 0));

	phy_dp_clk_ctl = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
	phy_dp_clk_ctl &= ~((0xfU << 12) | (0xfU << 8));

	switch (link_rate) {
	case 162000:
		cmn_diag_hsclk_sel |= (3U << 4) | (0U << 0);
		phy_dp_clk_ctl |= (2U << 12) | (4U << 8);
		cfg = rk3399_tcphy_dp_pll_rbr_cfg;
		cfg_size = nitems(rk3399_tcphy_dp_pll_rbr_cfg);
		break;
	case 270000:
		cmn_diag_hsclk_sel |= (3U << 4) | (0U << 0);
		phy_dp_clk_ctl |= (2U << 12) | (4U << 8);
		cfg = rk3399_tcphy_dp_pll_hbr_cfg;
		cfg_size = nitems(rk3399_tcphy_dp_pll_hbr_cfg);
		break;
	case 540000:
		cmn_diag_hsclk_sel |= (2U << 4) | (0U << 0);
		phy_dp_clk_ctl |= (1U << 12) | (2U << 8);
		cfg = rk3399_tcphy_dp_pll_hbr2_cfg;
		cfg_size = nitems(rk3399_tcphy_dp_pll_hbr2_cfg);
		break;
	default:
		return (EINVAL);
	}

	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_HSCLK_SEL, cmn_diag_hsclk_sel);
	RK_TYPEC_PHY_WRITE(sc, DP_CLK_CTL, phy_dp_clk_ctl);

	for (i = 0; i < cfg_size; i++)
		RK_TYPEC_PHY_WRITE(sc, cfg[i].addr, cfg[i].value);

	/* Enable the PLL. */
	reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
	reg &= ~DP_PLL_ENABLE_MASK;
	reg |= DP_PLL_ENABLE;
	RK_TYPEC_PHY_WRITE(sc, DP_CLK_CTL, reg);
	for (retry = 10000; retry > 0; retry--) {
		reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
		if (reg & DP_PLL_READY)
			break;
		DELAY(10);
	}
	if (retry == 0) {
		device_printf(sc->dev,
		    "set_link_rate: PLL enable timeout\n");
		return (ETIMEDOUT);
	}

	/* Enable the PMA PLL clocks. */
	reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
	reg &= ~DP_PLL_CLOCK_ENABLE_MASK;
	reg |= DP_PLL_CLOCK_ENABLE;
	RK_TYPEC_PHY_WRITE(sc, DP_CLK_CTL, reg);
	for (retry = 10000; retry > 0; retry--) {
		reg = RK_TYPEC_PHY_READ(sc, DP_CLK_CTL);
		if (reg & DP_PLL_CLOCK_ENABLE_ACK)
			break;
		DELAY(10);
	}
	if (retry == 0) {
		device_printf(sc->dev,
		    "set_link_rate: clock enable timeout\n");
		return (ETIMEDOUT);
	}

	/* PMA must traverse A2 on a data-rate change. */
	err = rk_typec_phy_dp_set_power_state(sc, 2);
	if (err != 0)
		return (err);
	err = rk_typec_phy_dp_set_power_state(sc, 0);
	if (err != 0)
		return (err);

	device_printf(sc->dev,
	    "set_link_rate: applied %d kHz (ssc=%d)\n", link_rate,
	    ssc_on ? 1 : 0);
	return (0);
}

/*
 * Empirically: live read of the reference build (reference vendor BSP, working DP at 1400x1050) shows
 * DP_MODE_CTL = 0x110 / 0x140 across boot and desktop modes — bits 15:12 are
 * always 0.  Even though the reference driver's tcphy_dp_set_lane_count() writes those bits,
 * the final settled state has all lanes enabled.  Some other code path (PHY
 * re-init or a write we haven't traced) clears them after.  Treat lane count
 * as sink-side training state only and leave DP_MODE_CTL untouched.
 */
int
rk_typec_phy_dp_set_lane_count(device_t dev, int lane_count)
{
	struct rk_typec_phy_softc *sc;

	if (dev == NULL)
		return (EINVAL);
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);

	switch (lane_count) {
	case 4:
	case 2:
	case 1:
		break;
	default:
		return (EINVAL);
	}
	device_printf(sc->dev,
	    "set_lane_count: lanes=%d leaving DP_MODE_CTL=0x%x unchanged\n",
	    lane_count, RK_TYPEC_PHY_READ(sc, DP_MODE_CTL));
	return (0);
}

int
rk_typec_phy_dp_set_link_rate_first(int link_rate, bool ssc_on)
{
	struct rk_typec_phy_softc *sc;

	sc = rk_typec_phy_first_softc();
	if (sc == NULL)
		return (ENXIO);
	return (rk_typec_phy_dp_set_link_rate(sc->dev, link_rate, ssc_on));
}

int
rk_typec_phy_dp_set_lane_count_first(int lane_count)
{
	struct rk_typec_phy_softc *sc;

	sc = rk_typec_phy_first_softc();
	if (sc == NULL)
		return (ENXIO);
	return (rk_typec_phy_dp_set_lane_count(sc->dev, lane_count));
}

/*
 * rk_typec_phy_dp_refresh_orientation_first
 *
 * Re-latch fusb302's current orientation (CC1/CC2) into sc->flip and force a
 * full PHY re-init on the next phy_enable so conn_dir GRF and AUX polarity
 * pick up the new orientation.  The caller (rk_cdn_dp at link-train start)
 * should phy_disable + phy_enable after this to actually trigger the rebuild.
 *
 * Reason this exists: at first phy_enable (stage 5) fusb302 may not yet have
 * negotiated the CC orientation, so apply_dp_grf locks conn_dir at sc->flip's
 * default value (false).  Subsequent VDM updates change sc->flip but the GRF
 * stays stale because no later phy_enable runs.  The flip-change-reinit logic
 * in rk_typec_phy_enable triggers on init_flip != flip, so simply forcing the
 * flip-aware path requires (a) refreshing sc->flip from fusb302 and (b)
 * confirming init_flip captured a different value.
 */
int
rk_typec_phy_dp_refresh_orientation_first(void)
{
	struct rk_typec_phy_softc *sc;
	bool prev_flip;

	sc = rk_typec_phy_first_softc();
	if (sc == NULL)
		return (ENXIO);

	prev_flip = sc->flip;
	(void)rk_typec_phy_update_flip(sc);
	if (sc->flip == prev_flip && sc->flip == sc->init_flip) {
		device_printf(sc->dev,
		    "refresh_orientation: flip=%u unchanged, no reinit needed\n",
		    sc->flip ? 1 : 0);
		return (0);
	}
	device_printf(sc->dev,
	    "refresh_orientation: flip %u->%u (init_flip=%u), tearing down PHY for reinit\n",
	    prev_flip ? 1 : 0, sc->flip ? 1 : 0,
	    sc->init_flip ? 1 : 0);
	/*
	 * Tear down the PMA so the next rk_typec_phy_enable observes
	 * PMA_CMN_CTRL1.READY=0 and runs the full init path including
	 * apply_dp_grf (which writes conn_dir from the current sc->flip).
	 * Clearing init_done is also required so the skip-init guard
	 * doesn't fire even after resets deassert.
	 */
	hwreset_assert(sc->rst_pipe);
	hwreset_assert(sc->rst_uphy);
	hwreset_assert(sc->rst_tcphy);
	DELAY(10000);
	sc->init_done = false;
	return (0);
}

static struct rk_typec_phy_softc *
rk_typec_phy_first_softc(void)
{
	devclass_t dc;
	device_t dev;

	dc = devclass_find("rk_typec_phy");
	if (dc == NULL)
		return (NULL);
	dev = devclass_get_device(dc, 0);
	if (dev == NULL)
		return (NULL);
	return (device_get_softc(dev));
}

int
rk_typec_phy_dp_set_signal_levels(device_t dev, int link_rate, int lane_count,
    uint8_t swing, uint8_t pre_emp)
{
	struct rk_typec_phy_softc *sc;
	uint32_t val;
	int i, j, lane;

	if (dev == NULL)
		return (EINVAL);
	if (lane_count != 1 && lane_count != 2 && lane_count != 4)
		return (EINVAL);
	if (swing > 2 || pre_emp > 3)
		return (EINVAL);
	if (rk_typec_phy_signal_table[swing][pre_emp].swing == 0 &&
	    rk_typec_phy_signal_table[swing][pre_emp].pe == 0 &&
	    !(swing == 0 && pre_emp == 0))
		return (EINVAL);

	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);

	if (lane_count == 4) {
		i = 0;
		j = 3;
	} else if (sc->flip) {
		i = 0;
		j = lane_count - 1;
	} else {
		i = 4 - lane_count;
		j = 3;
	}

	for (lane = i; lane <= j; lane++) {
		RK_TYPEC_PHY_WRITE(sc, TX_TXCC_MGNFS_MULT_000(lane),
		    rk_typec_phy_signal_table[swing][pre_emp].swing);
		RK_TYPEC_PHY_WRITE(sc, TX_TXCC_CPOST_MULT_00(lane),
		    rk_typec_phy_signal_table[swing][pre_emp].pe);

		if (swing == 2 && pre_emp == 0 && link_rate != 540000) {
			RK_TYPEC_PHY_WRITE(sc, TX_DIAG_TX_DRV(lane), 0x700);
			RK_TYPEC_PHY_WRITE(sc, TX_TXCC_CAL_SCLR_MULT(lane),
			    0x13c);
		} else {
			RK_TYPEC_PHY_WRITE(sc, TX_TXCC_CAL_SCLR_MULT(lane),
			    0x128);
			RK_TYPEC_PHY_WRITE(sc, TX_DIAG_TX_DRV(lane), 0x0400);
		}

		val = RK_TYPEC_PHY_READ(sc, XCVR_DIAG_PLLDRC_CTRL(lane));
		val &= ~(0x7U << 12);
		val |= ((link_rate == 540000) ? 0x5U : 0x6U) << 12;
		RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_PLLDRC_CTRL(lane), val);
	}

	return (0);
}

/*
 * rk_typec_phy_dp_set_signal_levels_first
 *
 * Convenience wrapper for rk_cdn_dp.ko: finds the first (and only on RockPro64)
 * rk_typec_phy device and applies signal levels to it.  Avoids having to
 * resolve a phy_t -> phynode -> device chain across module boundaries.
 */
int
rk_typec_phy_dp_set_signal_levels_first(int link_rate, int lane_count,
    uint8_t swing, uint8_t pre_emp)
{
	struct rk_typec_phy_softc *sc;

	sc = rk_typec_phy_first_softc();
	if (sc == NULL)
		return (ENXIO);
	return (rk_typec_phy_dp_set_signal_levels(sc->dev, link_rate,
	    lane_count, swing, pre_emp));
}

static int
rk_typec_phy_enable(struct phynode *phynode, bool enable)
{
	struct rk_typec_phy_softc *sc;
	device_t dev;
	intptr_t phy;
	uint32_t reg;
	int err, retry;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_TYPEC_PHY_DP && phy != RK3399_TYPEC_PHY_USB3)
		return (ERANGE);

	/*
	 * USB3-vs-USB2 routing for the SS lanes:
	 *   USB3 mode: SS lanes carry USB3 SuperSpeed → enable USB3 host.
	 *   DP mode (PIN_C/E, 4-lane DP):  SS lanes are DP → force USB3 host
	 *     into USB2-only so it can't drive interfering signals on the
	 *     same physical wires as our DP TX.  the reference driver's `tcphy_phy_init` for
	 *     `MODE_DFP_DP` calls `tcphy_cfg_usb3_to_usb2_only(tcphy, true)`
	 *     for exactly this reason (line 1179 in 4.4 BSP phy-rockchip-typec.c).
	 *     Without this, the USB3 host's idle signaling on SS lanes leaks
	 *     into the DP main link → CR survives but EQ never symbol-locks.
	 */
	if (phy == RK3399_TYPEC_PHY_USB3)
		rk_typec_phy_set_usb2_only(sc, false);
	else if (phy == RK3399_TYPEC_PHY_DP)
		rk_typec_phy_set_usb2_only(sc, true);

	err = clk_enable(sc->tcpdcore);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->tcpdcore));
		return (ENXIO);
	}
	err = clk_enable(sc->tcpdphy_ref);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->tcpdphy_ref));
		clk_disable(sc->tcpdcore);
		return (ENXIO);
	}

	(void)rk_typec_phy_update_flip(sc);

	/*
	 * Tear down the PHY if it was left active from a previous enable call.
	 * Writing DP_MODE_ENTER_A2 to a PHY already in A0 never asserts A2_READY
	 * because the lane PSMs won't regress without a full reset.  A clean
	 * cycle would assert all three resets before re-enabling.
	 * For our staged bring-up path, assert the resets here if the PMA is
	 * still running so the sequence starts from a clean slate.
	 */
	if (phy == RK3399_TYPEC_PHY_DP) {
		uint32_t pma_rd = RK_TYPEC_PHY_READ(sc, PMA_CMN_CTRL1);
		uint32_t dp_rd  = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);

		if ((pma_rd & PMA_CMN_CTRL1_READY) &&
		    !(dp_rd & DP_MODE_A2_READY)) {
			/*
			 * PMA is active but PHY is NOT already in A2.  This happens
			 * when the PHY was left in A0 (or USB3) by a prior run.
			 * Assert all three resets without touching clocks so the
			 * lane PSMs reset cleanly.  external_psm is re-latched by
			 * the toggle below after rst_tcphy deasserts.
			 */
			device_printf(dev,
			    "dp-init: PHY active non-A2 (dp=0x%x pma=0x%x), tearing down\n",
			    dp_rd, pma_rd);
			hwreset_assert(sc->rst_pipe);
			hwreset_assert(sc->rst_uphy);
			hwreset_assert(sc->rst_tcphy);
			DELAY(10000);
		} else if ((pma_rd & PMA_CMN_CTRL1_READY) &&
		    sc->init_done && sc->flip == sc->init_flip) {
			/*
			 * PMA active and PHY already in A2_READY: skip init only
			 * if this driver already brought the PHY up in the same
			 * connector orientation.  Re-running common 24M setup
			 * knocks PMA out of A2, and asserting rst_uphy while the
			 * PLL runs leaves PMA stuck.  conn_dir GRF can NOT be
			 * safely written mid-flight either (breaks A2 PSM).
			 *
			 * Do not trust a bootloader- or firmware-inherited A2
			 * state: if init_done is false, or the live orientation
			 * differs from init_flip, we must tear the PHY down and
			 * reinitialize it so PMA_LANE_CFG and conn_dir match the
			 * current Type-C orientation.
			 */
			device_printf(dev,
			    "dp-init: PHY already in A2 (dp=0x%x pma=0x%x), skip init (flip=%u init_flip=%u init_done=%u)\n",
			    dp_rd, pma_rd, sc->flip ? 1 : 0,
			    sc->init_flip ? 1 : 0, sc->init_done ? 1 : 0);
			rk_typec_phy_dp_aux_set_flip(sc);
			rk_typec_phy_dp_aux_calibration(sc);
			return (0);
		} else if (pma_rd & PMA_CMN_CTRL1_READY) {
			device_printf(dev,
			    "dp-init: inherited/stale A2 state (dp=0x%x pma=0x%x flip=%u init_flip=%u init_done=%u), forcing reinit\n",
			    dp_rd, pma_rd, sc->flip ? 1 : 0,
			    sc->init_flip ? 1 : 0, sc->init_done ? 1 : 0);
			hwreset_assert(sc->rst_pipe);
			hwreset_assert(sc->rst_uphy);
			hwreset_assert(sc->rst_tcphy);
			DELAY(10000);
		}
	}

	/*
	 * Set external PSM clock and uphy_dp_sel before releasing tcphy reset.
	 * Both must be set at probe with all resets held so the PHY comes
	 * out of reset already knowing its PSM clock source and its routing
	 * target.  uphy_dp_sel written after rst_tcphy deasserts means the
	 * PHY PSM clock mux pointed at the wrong source when the PLL
	 * started, which is why A2→A0 could stall.
	 */
	if (phy == RK3399_TYPEC_PHY_DP && sc->phy_ctrl_id == 0) {
		err = rk_typec_phy_set_field(sc, &rk3399_tcphy0_external_psm, 1);
		if (err != 0) {
			device_printf(dev, "cannot set external PSM clock\n");
			return (err);
		}
		err = rk_typec_phy_enable_dp_sel(sc);
		if (err != 0) {
			device_printf(dev, "cannot enable DP sel GRF\n");
			return (err);
		}
	}

	hwreset_deassert(sc->rst_tcphy);

	if (phy == RK3399_TYPEC_PHY_DP && sc->phy_ctrl_id == 0) {
		/*
		 * Force re-latch of external PSM clock source in the PHY after
		 * tcphy_rst deasserts.  The PHY has an internal latch that captures
		 * the GRF external_psm bit on the tcphy_rst deassert edge.
		 * On re-enable (after teardown), tcphy_rst was briefly asserted which
		 * resets the latch; writing the GRF before deassert is necessary but
		 * not sufficient — the PHY samples the transition, not the level.
		 * Toggle 0→1 here to guarantee the PHY re-latches the correct value.
		 */
		DELAY(10);
		(void)rk_typec_phy_set_field(sc, &rk3399_tcphy0_external_psm, 0);
		DELAY(10);
		(void)rk_typec_phy_set_field(sc, &rk3399_tcphy0_external_psm, 1);
		DELAY(10);
	}

	if (phy == RK3399_TYPEC_PHY_DP) {
		err = rk_typec_phy_apply_dp_grf(sc);
		if (err != 0) {
			device_printf(dev, "cannot apply DP GRF routing\n");
			return (err);
		}
		rk_typec_phy_dp_aux_set_flip(sc);

		/* Common 24 MHz setup. */
		RK_TYPEC_PHY_WRITE(sc, PMA_CMN_CTRL1, 0x830);
		for (int i = 0; i < 4; i++) {
			RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_LANE_FCM_EN_MGN(i), 0x90);
			RK_TYPEC_PHY_WRITE(sc, TX_RCVDET_EN_TMR(i), 0x960);
			RK_TYPEC_PHY_WRITE(sc, TX_RCVDET_ST_TMR(i), 0x30);
		}
		reg = RK_TYPEC_PHY_READ(sc, CMN_DIAG_HSCLK_SEL);
		reg &= ~CMN_DIAG_HSCLK_SEL_PLL_MASK;
		reg |= CMN_DIAG_HSCLK_SEL_PLL_CONFIG;
		RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_HSCLK_SEL, reg);

		rk_typec_phy_cfg_dp_pll(sc);
		/*
		 * Per-lane configuration matching reference vendor BSP's PIN_D/F path
		 * (`tcphy_phy_init` else-branch when MODE includes MODE_DFP_USB):
		 *   flip=1 (CC2): USB3 TX on lane 3, USB3 RX on lane 2, DP on 0,1
		 *   flip=0 (CC1): USB3 TX on lane 0, USB3 RX on lane 1, DP on 2,3
		 * This matches `set_signal_levels`'s flip-aware lane selection.
		 * Configuring all 4 lanes as DP (the previous `for i 0..3
		 * cfg_dp_lane`) conflicted with `PMA_LANE_CFG=PIN_ASSIGN_D_F`
		 * (which says lanes 2,3 are USB3) — analog cross-coupling between
		 * misconfigured DP-driven lanes and the actual DP lanes broke EQ.
		 * Live ftrace capture from working the reference build 4.4 BSP confirmed the reference driver
		 * uses this 2-USB3-+-2-DP per-lane split for our exact cable.
		 */
		if (sc->flip) {
			rk_typec_phy_cfg_usb3_tx_lane(sc, 3);
			rk_typec_phy_cfg_usb3_rx_lane(sc, 2);
			rk_typec_phy_cfg_dp_lane(sc, 0);
			rk_typec_phy_cfg_dp_lane(sc, 1);
		} else {
			rk_typec_phy_cfg_usb3_tx_lane(sc, 0);
			rk_typec_phy_cfg_usb3_rx_lane(sc, 1);
			rk_typec_phy_cfg_dp_lane(sc, 2);
			rk_typec_phy_cfg_dp_lane(sc, 3);
		}
		/*
		 * Pin Assignment selection.  Live read of the reference build shows
		 * PMA_LANE_CFG = 0x51d9 (PIN_C_E), but the reference build's per-lane DP
		 * electrical config is keyed to the C_E lane map (PMA0..3 all DP,
		 * with PMA0,1 internally = DP_LANE_2,3).  Our cfg_dp_lane() calls
		 * still drive flip-aware on PMA lanes 0,1 (CC2) / 2,3 (CC1), which
		 * matches the D_F map (PMA 0,1 = USB3, PMA 2,3 = DP).  Switching to
		 * C_E without also reworking cfg_dp_lane causes the trained link
		 * to collapse at modeset (PMA-vs-internal-lane skew).  Stay on D_F
		 * until per-lane cfg is reworked too.
		 */
		RK_TYPEC_PHY_WRITE(sc, PMA_LANE_CFG, PIN_ASSIGN_D_F);

		device_printf(sc->dev,
		    "dp-init: enter A2 dp_mode=0x%x pma_cmn=0x%x pma_lane=0x%x\n",
		    RK_TYPEC_PHY_READ(sc, DP_MODE_CTL),
		    RK_TYPEC_PHY_READ(sc, PMA_CMN_CTRL1),
		    RK_TYPEC_PHY_READ(sc, PMA_LANE_CFG));
		/*
		 * Read-modify-write: clear the mode-select bits [3:0] and OR
		 * in ENTER_A2 + LINK_RESET_DEASSERTED. Preserves bits 12-15
		 * (functional reset-state, NOT lane disable on this silicon).
		 * Read-modify-write only the mode bits.
		 */
		reg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
		reg &= ~DP_MODE_MASK;
		reg |= DP_MODE_ENTER_A2_BITS | DP_LINK_RESET_DEASSERTED;
		RK_TYPEC_PHY_WRITE(sc, DP_MODE_CTL, reg);

		device_printf(sc->dev,
		    "dp-init: pre-uphy-deassert ext_psm=0x%x dp_sel=0x%x pma=0x%x dp_mode=0x%x\n",
		    SYSCON_READ_4(sc->grf, rk3399_tcphy0_external_psm.reg),
		    SYSCON_READ_4(sc->grf, rk3399_tcphy0_uphy_dp_sel.reg),
		    RK_TYPEC_PHY_READ(sc, PMA_CMN_CTRL1),
		    RK_TYPEC_PHY_READ(sc, DP_MODE_CTL));
		hwreset_deassert(sc->rst_uphy);
		for (retry = 10000; retry > 0; retry--) {
			reg = RK_TYPEC_PHY_READ(sc, PMA_CMN_CTRL1);
			if (reg & PMA_CMN_CTRL1_READY)
				break;
			DELAY(10);
		}
		if (retry == 0) {
			device_printf(sc->dev, "Timeout waiting for PMA\n");
			return (ENXIO);
		}

		/*
		 * Stop here, deassert pipe_rst.  The next phase enables
		 * dp_sel, runs AUX calibration, enters A0, requires A0_READY.
		 */
		hwreset_deassert(sc->rst_pipe);

		device_printf(sc->dev,
		    "pre-A2-wait: dp_mode=0x%x ext_psm_grf=0x%x dp_sel_grf=0x%x\n",
		    RK_TYPEC_PHY_READ(sc, DP_MODE_CTL),
		    SYSCON_READ_4(sc->grf, rk3399_tcphy0_external_psm.reg),
		    SYSCON_READ_4(sc->grf, rk3399_tcphy0_uphy_dp_sel.reg));

		for (retry = 10000; retry > 0; retry--) {
			reg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
			if (reg & DP_MODE_A2_READY)
				break;
			DELAY(10);
		}
		device_printf(sc->dev,
		    "post-rst_pipe: dp_mode=0x%x A2_ready=%d ext_psm=0x%x dp_sel=0x%x\n",
		    RK_TYPEC_PHY_READ(sc, DP_MODE_CTL),
		    retry != 0 ? 1 : 0,
		    SYSCON_READ_4(sc->grf, rk3399_tcphy0_external_psm.reg),
		    SYSCON_READ_4(sc->grf, rk3399_tcphy0_uphy_dp_sel.reg));
		if (retry == 0)
			device_printf(sc->dev,
			    "Timeout waiting for DP A2: dp_mode=0x%x\n",
			    RK_TYPEC_PHY_READ(sc, DP_MODE_CTL));

		rk_typec_phy_dp_aux_calibration(sc);

		/* Read-modify-write A0 enter, preserving bits 12-15. */
		reg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
		reg &= ~DP_MODE_MASK;
		reg |= DP_MODE_ENTER_A0_BITS | DP_LINK_RESET_DEASSERTED;
		RK_TYPEC_PHY_WRITE(sc, DP_MODE_CTL, reg);
		for (retry = 10000; retry > 0; retry--) {
			reg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
			if (reg & DP_MODE_A0_READY)
				break;
			DELAY(10);
		}
		device_printf(sc->dev,
		    "post-A0: dp_mode=0x%x A0_ready=%d\n",
		    reg, retry != 0 ? 1 : 0);
		if (retry == 0) {
			/* Roll back to A2 on failure, preserving bits 12-15. */
			reg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
			reg &= ~DP_MODE_MASK;
			reg |= DP_MODE_ENTER_A2_BITS | DP_LINK_RESET_DEASSERTED;
			RK_TYPEC_PHY_WRITE(sc, DP_MODE_CTL, reg);
			device_printf(sc->dev,
			    "Timeout waiting for DP A0: dp_mode=0x%x\n",
			    RK_TYPEC_PHY_READ(sc, DP_MODE_CTL));
			return (ENXIO);
		}

		sc->init_done = true;
		sc->init_flip = sc->flip;
		return (0);
	}

	/* 24M configuration, magic values from rockchip */
	RK_TYPEC_PHY_WRITE(sc, PMA_CMN_CTRL1, 0x830);
	for (int i = 0; i < 4; i++) {
		RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_LANE_FCM_EN_MGN(i), 0x90);
		RK_TYPEC_PHY_WRITE(sc, TX_RCVDET_EN_TMR(i), 0x960);
		RK_TYPEC_PHY_WRITE(sc, TX_RCVDET_ST_TMR(i), 0x30);
	}
	reg = RK_TYPEC_PHY_READ(sc, CMN_DIAG_HSCLK_SEL);
	reg &= ~CMN_DIAG_HSCLK_SEL_PLL_MASK;
	reg |= CMN_DIAG_HSCLK_SEL_PLL_CONFIG;
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_HSCLK_SEL, reg);

	/* PLL configuration, magic values from rockchip */
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_VCOCAL_INIT, 0xf0);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_VCOCAL_ITER, 0x18);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_INTDIV, 0xd0);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_FRACDIV, 0x4a4a);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_HIGH_THR, 0x34);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_SS_CTRL1, 0x1ee);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_SS_CTRL2, 0x7f03);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_DSM_DIAG, 0x20);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_OVRD, 0);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_FBH_OVRD, 0);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_FBL_OVRD, 0);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_V2I_TUNE, 0x7);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_CP_TUNE, 0x45);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_LF_PROG, 0x8);

	/* Configure the TX and RX line, magic values from rockchip */
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A0(0), 0x7799);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A1(0), 0x7798);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A2(0), 0x5098);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A3(0), 0x5098);
	RK_TYPEC_PHY_WRITE(sc, TX_TXCC_MGNFS_MULT_000(0), 0x0);
	RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_BIDI_CTRL(0), 0xbf);

	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A0(1), 0xa6fd);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A1(1), 0xa6fd);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A2(1), 0xa410);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A3(1), 0x2410);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_CAL(1), 0x23ff);
	RK_TYPEC_PHY_WRITE(sc, RX_SIGDET_HL_FILT_TMR(1), 0x13);
	RK_TYPEC_PHY_WRITE(sc, RX_REE_CTRL_DATA_MASK(1), 0x03e7);
	RK_TYPEC_PHY_WRITE(sc, RX_DIAG_SIGDET_TUNE(1), 0x1004);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_RDY(1), 0x2010);
	RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_BIDI_CTRL(1), 0xfb);

	RK_TYPEC_PHY_WRITE(sc, PMA_LANE_CFG, PIN_ASSIGN_D_F);

	{
		uint32_t mreg = RK_TYPEC_PHY_READ(sc, DP_MODE_CTL);
		mreg &= ~DP_MODE_MASK;
		mreg |= DP_MODE_ENTER_A2_BITS | DP_LINK_RESET_DEASSERTED;
		RK_TYPEC_PHY_WRITE(sc, DP_MODE_CTL, mreg);
	}

	hwreset_deassert(sc->rst_uphy);

	for (retry = 10000; retry > 0; retry--) {
		reg = RK_TYPEC_PHY_READ(sc, PMA_CMN_CTRL1);
		if (reg & PMA_CMN_CTRL1_READY)
			break;
		DELAY(10);
	}
	if (retry == 0) {
		device_printf(sc->dev, "Timeout waiting for PMA\n");
		return (ENXIO);
	}

	hwreset_deassert(sc->rst_pipe);

	return (0);
}

static int
rk_typec_phy_get_mode(struct phynode *phynode, int *mode)
{
	struct rk_typec_phy_softc *sc;
	intptr_t phy;
	device_t dev;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_TYPEC_PHY_USB3)
		return (ERANGE);

	*mode = sc->mode;

	return (0);
}

static int
rk_typec_phy_set_mode(struct phynode *phynode, int mode)
{
	struct rk_typec_phy_softc *sc;
	intptr_t phy;
	device_t dev;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_TYPEC_PHY_USB3)
		return (ERANGE);

	sc->mode = mode;

	return (0);
}

static int
rk_typec_phy_set_phy_mode(struct phynode *phynode, phy_mode_t mode,
    phy_submode_t submode)
{
	struct rk_typec_phy_softc *sc;
	intptr_t phy;
	device_t dev;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy == RK3399_TYPEC_PHY_DP) {
		if (mode != PHY_MODE_DP)
			return (EINVAL);
		sc->dp_mode = mode;
		sc->dp_submode = submode;
		return (0);
	}

	if (phy != RK3399_TYPEC_PHY_USB3)
		return (ERANGE);

	switch (mode) {
	case PHY_MODE_USB_HOST:
	case PHY_MODE_USB_DEVICE:
	case PHY_MODE_USB_OTG:
		return (0);
	default:
		return (EINVAL);
	}
}

/*
 * "Poke ahead" diagnostic: pre-write the signal-level registers for the
 * EQ-target values (swing=2, pre_emp=1) to the active DP lanes BEFORE the
 * cdn_dp link-train sequence runs.  Then poll dump_regs to see whether
 * those values stay or get overwritten during EQ — points to whether
 * something else (firmware, another driver path) is mutating our writes.
 *
 * Hardcoded for swing=2 pre_emp=1 at RBR/HBR (not HBR2): MGNFS=0x00,
 * CPOST=0x15, TX_DRV=0x400, CAL_SCLR=0x128, PLLDRC bits 14:12 = 0x6.
 */
static int
rk_typec_phy_sysctl_force_eq_test(SYSCTL_HANDLER_ARGS)
{
	struct rk_typec_phy_softc *sc = arg1;
	int error, val = 0;
	int dp[2];
	int i;
	uint32_t reg;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL || val != 1)
		return (error);

	if (sc->flip) {
		dp[0] = 0; dp[1] = 1;
	} else {
		dp[0] = 2; dp[1] = 3;
	}

	/*
	 * write+readback+verify each target register; only consider the bit
	 * "poked successfully" if the readback matches.  Mismatches indicate
	 * read-only bits, sticky bits, or some other registers ignoring writes
	 * in the current chip state — which itself is diagnostic.
	 */
#define POKE_VERIFY(_reg_macro, _addr_arg, _want, _name) do {		\
	uint32_t _wrote = (_want);					\
	RK_TYPEC_PHY_WRITE(sc, _reg_macro(_addr_arg), _wrote);		\
	uint32_t _read = RK_TYPEC_PHY_READ(sc, _reg_macro(_addr_arg));	\
	if (_read == _wrote)						\
		device_printf(sc->dev,					\
		    "FORCE_EQ: lane%d %s wrote=0x%04x readback=0x%04x ✓\n",\
		    _addr_arg, _name, _wrote, _read);			\
	else								\
		device_printf(sc->dev,					\
		    "FORCE_EQ: lane%d %s wrote=0x%04x readback=0x%04x ✗ "\
		    "(bit not writable in current state)\n",		\
		    _addr_arg, _name, _wrote, _read);			\
} while (0)

	for (i = 0; i < 2; i++) {
		int lane = dp[i];
		POKE_VERIFY(TX_TXCC_MGNFS_MULT_000, lane, 0x00, "MGNFS_000");
		POKE_VERIFY(TX_TXCC_CPOST_MULT_00,  lane, 0x15, "CPOST_00 ");
		POKE_VERIFY(TX_TXCC_CAL_SCLR_MULT,  lane, 0x128, "CAL_SCLR ");
		POKE_VERIFY(TX_DIAG_TX_DRV,         lane, 0x400, "TX_DRV   ");

		reg = RK_TYPEC_PHY_READ(sc, XCVR_DIAG_PLLDRC_CTRL(lane));
		reg = (reg & ~(0x7U << 12)) | (0x6U << 12);
		POKE_VERIFY(XCVR_DIAG_PLLDRC_CTRL,  lane, reg,  "PLLDRC   ");
	}
#undef POKE_VERIFY
	return (0);
}

/*
 * Snapshot a curated set of PHY registers to dmesg.  Intended for diff'ing
 * across time (poll every 500 ms while link-train runs) to find which
 * registers mutate during EQ phase.  Uses %x1, %x2 for x16 register I/O
 * matching the rest of the driver.
 */
static int
rk_typec_phy_sysctl_dump_regs(SYSCTL_HANDLER_ARGS)
{
	struct rk_typec_phy_softc *sc = arg1;
	int error, val = 0;
	int dp[2];
	int i;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL || val != 1)
		return (error);

	/* DP lane indices for current flip orientation. */
	if (sc->flip) {
		dp[0] = 0; dp[1] = 1;
	} else {
		dp[0] = 2; dp[1] = 3;
	}

	device_printf(sc->dev,
	    "REGDUMP: DP_MODE_CTL=0x%08x PMA_CMN_CTRL1=0x%08x DP_CLK_CTL=0x%08x "
	    "PMA_LANE_CFG=0x%08x flip=%d flip_override=%d\n",
	    RK_TYPEC_PHY_READ(sc, DP_MODE_CTL),
	    RK_TYPEC_PHY_READ(sc, PMA_CMN_CTRL1),
	    RK_TYPEC_PHY_READ(sc, DP_CLK_CTL),
	    RK_TYPEC_PHY_READ(sc, PMA_LANE_CFG),
	    sc->flip, sc->flip_override);

	device_printf(sc->dev,
	    "REGDUMP: PLL1 INTDIV=0x%04x VCOCAL_START=0x%04x "
	    "PLLSM1_LOCK=0x%04x HSCLK_SEL=0x%04x\n",
	    RK_TYPEC_PHY_READ(sc, CMN_PLL1_INTDIV),
	    RK_TYPEC_PHY_READ(sc, CMN_PLL1_VCOCAL_START),
	    RK_TYPEC_PHY_READ(sc, CMN_PLLSM1_PLLLOCK),
	    RK_TYPEC_PHY_READ(sc, CMN_DIAG_HSCLK_SEL));

	for (i = 0; i < 2; i++) {
		int lane = dp[i];
		device_printf(sc->dev,
		    "REGDUMP: dp_lane%d MGNFS_000=0x%04x CPOST_00=0x%04x "
		    "TX_DRV=0x%04x CAL_SCLR=0x%04x PLLDRC=0x%04x "
		    "PSC_A0=0x%04x BIDI=0x%04x\n",
		    lane,
		    RK_TYPEC_PHY_READ(sc, TX_TXCC_MGNFS_MULT_000(lane)),
		    RK_TYPEC_PHY_READ(sc, TX_TXCC_CPOST_MULT_00(lane)),
		    RK_TYPEC_PHY_READ(sc, TX_DIAG_TX_DRV(lane)),
		    RK_TYPEC_PHY_READ(sc, TX_TXCC_CAL_SCLR_MULT(lane)),
		    RK_TYPEC_PHY_READ(sc, XCVR_DIAG_PLLDRC_CTRL(lane)),
		    RK_TYPEC_PHY_READ(sc, TX_PSC_A0(lane)),
		    RK_TYPEC_PHY_READ(sc, XCVR_DIAG_BIDI_CTRL(lane)));
	}

	return (0);
}

static int
rk_typec_phy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK3399 PHY TYPEC");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_typec_phy_attach(device_t dev)
{
	struct rk_typec_phy_softc *sc;
	struct phynode_init_def phy_init;
	struct phynode *phynode;
	phandle_t node, dp, usb3;
	phandle_t reg_prop[4];

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	/* 
	 * Find out which phy we are.
	 * There is not property for this so we need to know the
	 * address to use the correct GRF registers.
	 */
	if (OF_getencprop(node, "reg", reg_prop, sizeof(reg_prop)) <= 0) {
		device_printf(dev, "Cannot guess phy controller id\n");
		return (ENXIO);
	}
	switch (reg_prop[1]) {
	case 0xff7c0000:
		sc->phy_ctrl_id = 0;
		break;
	case 0xff800000:
		sc->phy_ctrl_id = 1;
		break;
	default:
		device_printf(dev, "Unknown address %x for typec-phy\n", reg_prop[1]);
		return (ENXIO);
	}
	if (bus_alloc_resources(dev, rk_typec_phy_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		goto fail;
	}

	if (syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &sc->grf) != 0) {
		device_printf(dev, "Cannot get syscon handle\n");
		goto fail;
	}

	if (clk_get_by_ofw_name(dev, 0, "tcpdcore", &sc->tcpdcore) != 0) {
		device_printf(dev, "Cannot get tcpdcore clock\n");
		goto fail;
	}
	if (clk_get_by_ofw_name(dev, 0, "tcpdphy-ref", &sc->tcpdphy_ref) != 0) {
		device_printf(dev, "Cannot get tcpdphy-ref clock\n");
		goto fail;
	}

	if (hwreset_get_by_ofw_name(dev, 0, "uphy", &sc->rst_uphy) != 0) {
		device_printf(dev, "Cannot get uphy reset\n");
		goto fail;
	}
	if (hwreset_get_by_ofw_name(dev, 0, "uphy-pipe", &sc->rst_pipe) != 0) {
		device_printf(dev, "Cannot get uphy-pipe reset\n");
		goto fail;
	}
	if (hwreset_get_by_ofw_name(dev, 0, "uphy-tcphy", &sc->rst_tcphy) != 0) {
		device_printf(dev, "Cannot get uphy-tcphy reset\n");
		goto fail;
	}

	/* 
	 * Make sure that the module is asserted 
	 * We need to deassert in a certain order when we enable the phy
	 */
	hwreset_assert(sc->rst_uphy);
	hwreset_assert(sc->rst_pipe);
	hwreset_assert(sc->rst_tcphy);

	/* Set the assigned clocks parent and freq */
	if (clk_set_assigned(dev, node) != 0) {
		device_printf(dev, "clk_set_assigned failed\n");
		goto fail;
	}

	dp = ofw_bus_find_child(node, "dp-port");
	usb3 = ofw_bus_find_child(node, "usb3-port");
	if (usb3 == 0) {
		device_printf(dev, "Cannot find usb3-port child node\n");
		goto fail;
	}
	if (dp != 0 && ofw_bus_node_status_okay(dp)) {
		phy_init.id = RK3399_TYPEC_PHY_DP;
		phy_init.ofw_node = dp;
		phynode = phynode_create(dev, &rk_typec_phy_phynode_class,
		    &phy_init);
		if (phynode == NULL) {
			device_printf(dev, "failed to create phy dp-port\n");
			goto fail;
		}
		if (phynode_register(phynode) == NULL) {
			device_printf(dev, "failed to register phy dp-port\n");
			goto fail;
		}
		OF_device_register_xref(OF_xref_from_node(dp), dev);
	}
	/* If the child isn't enable attach the driver
	 *  but do not register the PHY. 
	 */
	if (!ofw_bus_node_status_okay(usb3))
		return (0);

	phy_init.id = RK3399_TYPEC_PHY_USB3;
	phy_init.ofw_node = usb3;
	phynode = phynode_create(dev, &rk_typec_phy_phynode_class, &phy_init);
	if (phynode == NULL) {
		device_printf(dev, "failed to create phy usb3-port\n");
		goto fail;
	}
	if (phynode_register(phynode) == NULL) {
		device_printf(dev, "failed to register phy usb3-port\n");
		goto fail;
	}

	OF_device_register_xref(OF_xref_from_node(usb3), dev);

	sc->flip_override = -1;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "flip_override", CTLFLAG_RW, &sc->flip_override, -1,
	    "AUX polarity flip override: -1=auto(FUSB302), 0=CC1, 1=CC2");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "dump_regs", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_typec_phy_sysctl_dump_regs, "I",
	    "Write 1 to snapshot key PHY registers (DP mode, PMA, PLL1 lock, "
	    "per-lane signal levels) into dmesg — for diff'ing across time "
	    "during link train to find mutating registers");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "force_eq_test", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, rk_typec_phy_sysctl_force_eq_test, "I",
	    "Write 1 to pre-poke EQ-target signal levels (swing=2 pre_emp=1) "
	    "to the active DP lanes — for watching whether stage-16 link-train "
	    "preserves or overwrites them");

	return (0);

fail:
	bus_release_resources(dev, rk_typec_phy_spec, &sc->res);

	return (ENXIO);
}

static device_method_t rk_typec_phy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_typec_phy_probe),
	DEVMETHOD(device_attach,	rk_typec_phy_attach),

	DEVMETHOD_END
};

static driver_t rk_typec_phy_driver = {
	"rk_typec_phy",
	rk_typec_phy_methods,
	sizeof(struct rk_typec_phy_softc)
};

EARLY_DRIVER_MODULE(rk_typec_phy, simplebus, rk_typec_phy_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(rk_typec_phy, 1);
