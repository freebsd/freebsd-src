/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/fdt/fdt_common.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/nvidia/tegra_efuse.h>

#include <dt-bindings/pinctrl/pinctrl-tegra-xusb.h>

#include "phynode_if.h"

/* FUSE calibration data. */
#define	FUSE_SKU_CALIB_0			0x0F0
#define	  FUSE_SKU_CALIB_0_HS_CURR_LEVEL_123(x, i)	(((x) >> (11 + ((i) - 1) * 6)) & 0x3F);
#define	  FUSE_SKU_CALIB_0_HS_TERM_RANGE_ADJ(x)		(((x) >>  7) & 0x0F);
#define	  FUSE_SKU_CALIB_0_HS_CURR_LEVEL_0(x)		(((x) >>  0) & 0x3F);

#define	FUSE_USB_CALIB_EXT_0			0x250
#define	  FUSE_USB_CALIB_EXT_0_RPD_CTRL(x)		(((x) >>  0) & 0x1F);


/* Registers. */
#define	XUSB_PADCTL_USB2_PAD_MUX		0x004

#define	XUSB_PADCTL_USB2_PORT_CAP		0x008
#define	 USB2_PORT_CAP_PORT_REVERSE_ID(p)		(1 << (3 + (p) * 4))
#define	 USB2_PORT_CAP_PORT_INTERNAL(p)			(1 << (2 + (p) * 4))
#define	 USB2_PORT_CAP_PORT_CAP(p, x)			(((x) & 3) << ((p) * 4))
#define	  USB2_PORT_CAP_PORT_CAP_OTG			0x3
#define	  USB2_PORT_CAP_PORT_CAP_DEVICE			0x2
#define	  USB2_PORT_CAP_PORT_CAP_HOST			0x1
#define	  USB2_PORT_CAP_PORT_CAP_DISABLED		0x0

#define	XUSB_PADCTL_SS_PORT_MAP			0x014
#define	 SS_PORT_MAP_PORT_INTERNAL(p)			(1 << (3 + (p) * 4))
#define	 SS_PORT_MAP_PORT_MAP(p, x)			(((x) & 7) << ((p) * 4))

#define	XUSB_PADCTL_ELPG_PROGRAM1		0x024
#define	 ELPG_PROGRAM1_AUX_MUX_LP0_VCORE_DOWN		(1 << 31)
#define	 ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN_EARLY	(1 << 30)
#define	 ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN		(1 << 29)
#define	 ELPG_PROGRAM1_SSP_ELPG_VCORE_DOWN(x) 		(1 << (2 + (x) * 3))
#define	 ELPG_PROGRAM1_SSP_ELPG_CLAMP_EN_EARLY(x) 	(1 << (1 + (x) * 3))
#define	 ELPG_PROGRAM1_SSP_ELPG_CLAMP_EN(x)		(1 << (0 + (x) * 3))

#define	XUSB_PADCTL_USB3_PAD_MUX		0x028
#define	 USB3_PAD_MUX_SATA_IDDQ_DISABLE(x) 		(1 << (8 + (x)))
#define	 USB3_PAD_MUX_PCIE_IDDQ_DISABLE(x) 		(1 << (1 + (x)))

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(x) (0x084 + (x) * 0x40)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBON_RPU_OVRD_VAL (1 << 23)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBON_RPU_OVRD	( 1 << 22)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBON_RPD_OVRD_VAL (1 << 21)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBON_RPD_OVRD	 (1 << 20)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBOP_RPU_OVRD_VAL (1 << 19)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBOP_RPU_OVRD	 (1 << 18)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBOP_RPD_OVRD_VAL (1 << 17)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_USBOP_RPD_OVRD	 (1 << 16)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_DYN_DLY(x)	 (((x) & 0x3) <<  9)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_LEV(x)	 (((x) & 0x3) <<  7)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_FIX18	 (1 <<  6)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_DIV_DET_EN	 (1 <<  4)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VOP_DIV2P7_DET	 (1 <<  3)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VOP_DIV2P0_DET	 (1 <<  2)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VON_DIV2P7_DET	 (1 <<  1)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VON_DIV2P0_DET	 (1 <<  0)

#define	XUSB_PADCTL_USB2_OTG_PAD_CTL0(x) 	(0x088 + (x) * 0x40)
#define	 USB2_OTG_PAD_CTL0_PD_ZI			(1 << 29)
#define	 USB2_OTG_PAD_CTL0_PD2_OVRD_EN			(1 << 28)
#define	 USB2_OTG_PAD_CTL0_PD2				(1 << 27)
#define	 USB2_OTG_PAD_CTL0_PD				(1 << 26)
#define	 USB2_OTG_PAD_CTL0_TERM_EN			(1 << 25)
#define	 USB2_OTG_PAD_CTL0_LS_FSLEW(x)			(((x) & 0x0F) << 21)
#define	 USB2_OTG_PAD_CTL0_LS_RSLEW(x)			(((x) & 0x0F) << 17)
#define	 USB2_OTG_PAD_CTL0_FS_FSLEW(x)			(((x) & 0x0F) << 13)
#define	 USB2_OTG_PAD_CTL0_FS_RSLEW(x)			(((x) & 0x0F) <<  9)
#define	 USB2_OTG_PAD_CTL0_HS_SLEW(x)			(((x) & 0x3F) <<  6)
#define	 USB2_OTG_PAD_CTL0_HS_CURR_LEVEL(x)		(((x) & 0x3F) <<  0)

#define XUSB_PADCTL_USB2_OTG_PAD_CTL1(x) 	(0x08C + (x) * 0x40)
#define	 USB2_OTG_PAD_CTL1_RPD_CTRL(x)			(((x) & 0x1F) <<  26)
#define	 USB2_OTG_PAD_CTL1_RPU_STATUS_HIGH		(1 <<  25)
#define	 USB2_OTG_PAD_CTL1_RPU_SWITCH_LOW		(1 <<  24)
#define	 USB2_OTG_PAD_CTL1_RPU_SWITCH_OVRD		(1 <<  23)
#define	 USB2_OTG_PAD_CTL1_HS_LOOPBACK_OVRD_VAL		(1 <<  22)
#define	 USB2_OTG_PAD_CTL1_HS_LOOPBACK_OVRD_EN		(1 <<  21)
#define	 USB2_OTG_PAD_CTL1_PTERM_RANGE_ADJ(x)		(((x) & 0x0F) << 17)
#define	 USB2_OTG_PAD_CTL1_PD_DISC_OVRD_VAL		(1 << 16)
#define	 USB2_OTG_PAD_CTL1_PD_CHRP_OVRD_VAL		(1 << 15)
#define	 USB2_OTG_PAD_CTL1_RPU_RANGE_ADJ(x)		(((x) & 0x03) << 13)
#define	 USB2_OTG_PAD_CTL1_HS_COUP_EN(x)		(((x) & 0x03) << 11)
#define	 USB2_OTG_PAD_CTL1_SPARE(x)			(((x) & 0x0F) <<  7)
#define	 USB2_OTG_PAD_CTL1_TERM_RANGE_ADJ(x)		(((x) & 0x0F) <<  3)
#define	 USB2_OTG_PAD_CTL1_PD_DR			(1 <<  2)
#define	 USB2_OTG_PAD_CTL1_PD_DISC_OVRD			(1 <<  1)
#define	 USB2_OTG_PAD_CTL1_PD_CHRP_OVRD			(1 <<  0)

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0(x) (0x0C0 + (x) * 0x40)
#define	XUSB_PADCTL_USB2_BIAS_PAD_CTL0		0x0284
#define	 USB2_BIAS_PAD_CTL0_TRK_PWR_ENA			(1 << 29)
#define	 USB2_BIAS_PAD_CTL0_SPARE(x)			(((x) & 0xF) << 25)
#define	 USB2_BIAS_PAD_CTL0_CHG_DIV(x)			(((x) & 0xF) << 21)
#define	 USB2_BIAS_PAD_CTL0_TEMP_COEF(x)		(((x) & 0x7) << 18)
#define	 USB2_BIAS_PAD_CTL0_VREF_CTRL(x)		(((x) & 0x7) << 15)
#define	 USB2_BIAS_PAD_CTL0_ADJRPU(x)			(((x) & 0x7) << 12)
#define	 USB2_BIAS_PAD_CTL0_PD				(1 << 11)
#define	 USB2_BIAS_PAD_CTL0_TERM_OFFSETL(x)		(((x) & 0x7) <<  8)
#define	 USB2_BIAS_PAD_CTL0_HS_CHIRP_LEVEL(x)		(((x) & 0x3) <<  6)
#define	 USB2_BIAS_PAD_CTL0_HS_DISCON_LEVEL(x)		(((x) & 0x7) <<  3)
#define	 USB2_BIAS_PAD_CTL0_HS_SQUELCH_LEVEL(x)		(((x) & 0x7) <<  0)

#define	XUSB_PADCTL_USB2_BIAS_PAD_CTL1		0x0288
#define	 USB2_BIAS_PAD_CTL1_FORCE_TRK_CLK_EN		(1 << 30)
#define	 USB2_BIAS_PAD_CTL1_TRK_SW_OVRD			(1 << 29)
#define	 USB2_BIAS_PAD_CTL1_TRK_DONE			(1 << 28)
#define	 USB2_BIAS_PAD_CTL1_TRK_START			(1 << 27)
#define	 USB2_BIAS_PAD_CTL1_PD_TRK			(1 << 26)
#define	 USB2_BIAS_PAD_CTL1_TRK_DONE_RESET_TIMER(x)	(((x) & 0x7F) << 19)
#define	 USB2_BIAS_PAD_CTL1_TRK_START_TIMER(x)		(((x) & 0x7F) << 12)
#define	 USB2_BIAS_PAD_CTL1_PCTRL(x)			(((x) & 0x3F) <<  6)
#define	 USB2_BIAS_PAD_CTL1_TCTRL(x)			(((x) & 0x3F) <<  0)

#define	XUSB_PADCTL_HSIC_PAD_CTL0(x)		(0x300 + (x) * 0x20)
#define	 HSIC_PAD_CTL0_RPU_STROBE			(1 << 18)
#define	 HSIC_PAD_CTL0_RPU_DATA1			(1 << 17)
#define	 HSIC_PAD_CTL0_RPU_DATA0			(1 << 16)
#define	 HSIC_PAD_CTL0_RPD_STROBE			(1 << 15)
#define	 HSIC_PAD_CTL0_RPD_DATA1			(1 << 14)
#define	 HSIC_PAD_CTL0_RPD_DATA0			(1 << 13)
#define	 HSIC_PAD_CTL0_LPBK_STROBE			(1 << 12)
#define	 HSIC_PAD_CTL0_LPBK_DATA1			(1 << 11)
#define	 HSIC_PAD_CTL0_LPBK_DATA0			(1 << 10)
#define	 HSIC_PAD_CTL0_PD_ZI_STROBE			(1 <<  9)
#define	 HSIC_PAD_CTL0_PD_ZI_DATA1			(1 <<  8)
#define	 HSIC_PAD_CTL0_PD_ZI_DATA0			(1 <<  7)
#define	 HSIC_PAD_CTL0_PD_RX_STROBE			(1 <<  6)
#define	 HSIC_PAD_CTL0_PD_RX_DATA1			(1 <<  5)
#define	 HSIC_PAD_CTL0_PD_RX_DATA0			(1 <<  4)
#define	 HSIC_PAD_CTL0_PD_TX_STROBE			(1 <<  3)
#define	 HSIC_PAD_CTL0_PD_TX_DATA1			(1 <<  2)
#define	 HSIC_PAD_CTL0_PD_TX_DATA0			(1 <<  1)
#define	 HSIC_PAD_CTL0_IDDQ				(1 <<  0)

#define	XUSB_PADCTL_HSIC_PAD_CTL1(x)		(0x304 + (x) * 0x20)
#define	 HSIC_PAD_CTL1_RTERM(x)				(((x) & 0xF) << 12)
#define	 HSIC_PAD_CTL1_HSIC_OPT(x)			(((x) & 0xF) <<  8)
#define	 HSIC_PAD_CTL1_TX_SLEW(x)			(((x) & 0xF) <<  4)
#define	 HSIC_PAD_CTL1_TX_RTUNEP(x)			(((x) & 0xF) <<  0)

#define	XUSB_PADCTL_HSIC_PAD_CTL2(x)		(0x308 + (x) * 0x20)
#define	 HSIC_PAD_CTL2_RX_STROBE_TRIM(x)		(((x) & 0xF) <<  8)
#define	 HSIC_PAD_CTL2_RX_DATA1_TRIM(x)			(((x) & 0xF) <<  4)
#define	 HSIC_PAD_CTL2_RX_DATA0_TRIM(x)			(((x) & 0xF) <<  0)

#define	XUSB_PADCTL_HSIC_PAD_TRK_CTL		0x340
#define	 HSIC_PAD_TRK_CTL_AUTO_RTERM_EN			(1 << 24)
#define	 HSIC_PAD_TRK_CTL_FORCE_TRK_CLK_EN		(1 << 23)
#define	 HSIC_PAD_TRK_CTL_TRK_SW_OVRD			(1 << 22)
#define	 HSIC_PAD_TRK_CTL_TRK_DONE			(1 << 21)
#define	 HSIC_PAD_TRK_CTL_TRK_START			(1 << 20)
#define	 HSIC_PAD_TRK_CTL_PD_TRK			(1 << 19)
#define	 HSIC_PAD_TRK_CTL_TRK_DONE_RESET_TIMER(x)	(((x) & 0x3F) << 12)
#define	 HSIC_PAD_TRK_CTL_TRK_START_TIMER(x)		(((x) & 0x7F) <<  5)
#define	 HSIC_PAD_TRK_CTL_RTERM_OUT(x)			(((x) & 0x1F) <<  0)

#define XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL	0x344

#define XUSB_PADCTL_UPHY_PLL_P0_CTL1		0x360
#define  UPHY_PLL_P0_CTL1_PLL0_FREQ_PSDIV(x)		(((x) & 0x03) << 28)
#define  UPHY_PLL_P0_CTL1_PLL0_FREQ_NDIV(x)		(((x) & 0xFF) << 20)
#define  UPHY_PLL_P0_CTL1_PLL0_FREQ_MDIV(x)		(((x) & 0x03) << 16)
#define  UPHY_PLL_P0_CTL1_PLL0_LOCKDET_STATUS 		(1 << 15)
#define  UPHY_PLL_P0_CTL1_PLL0_MODE_GET(x)		(((x) >> 8) & 0x03)
#define  UPHY_PLL_P0_CTL1_PLL0_BYPASS_EN 		(1 <<  7)
#define  UPHY_PLL_P0_CTL1_PLL0_FREERUN_EN		(1 <<  6)
#define  UPHY_PLL_P0_CTL1_PLL0_PWR_OVRD			(1 <<  4)
#define  UPHY_PLL_P0_CTL1_PLL0_ENABLE 			(1 <<  3)
#define  UPHY_PLL_P0_CTL1_PLL0_SLEEP(x)			(((x) & 0x03) <<  1)
#define  UPHY_PLL_P0_CTL1_PLL0_IDDQ 			(1 << 0)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL2		0x364
#define  UPHY_PLL_P0_CTL2_PLL0_CAL_CTRL(x)		(((x) & 0xFFFFFF) << 4)
#define  UPHY_PLL_P0_CTL2_PLL0_CAL_RESET		(1 << 3)
#define  UPHY_PLL_P0_CTL2_PLL0_CAL_OVRD			(1 << 2)
#define  UPHY_PLL_P0_CTL2_PLL0_CAL_DONE			(1 << 1)
#define  UPHY_PLL_P0_CTL2_PLL0_CAL_EN			(1 << 0)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL4		0x36c
#define  UPHY_PLL_P0_CTL4_PLL0_TCLKOUT_EN		(1 << 28)
#define  UPHY_PLL_P0_CTL4_PLL0_CLKDIST_CTRL(x)		(((x) & 0xF) << 20)
#define  UPHY_PLL_P0_CTL4_PLL0_XDIGCLK_EN		(1 << 19)
#define  UPHY_PLL_P0_CTL4_PLL0_XDIGCLK_SEL(x)		(((x) & 0x7) << 16)
#define  UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_EN		(1 << 15)
#define  UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_SEL(x)		(((x) & 0x3) << 12)
#define  UPHY_PLL_P0_CTL4_PLL0_FBCLKBUF_EN		(1 <<  9)
#define  UPHY_PLL_P0_CTL4_PLL0_REFCLKBUF_EN		(1 <<  8)
#define  UPHY_PLL_P0_CTL4_PLL0_REFCLK_SEL(x)		(((x) & 0xF) <<  4)
#define  UPHY_PLL_P0_CTL4_PLL0_REFCLK_TERM100		(1 <<  0)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL5		0x370
#define  UPHY_PLL_P0_CTL5_PLL0_DCO_CTRL(x)		(((x) & 0xFF) << 16)
#define  UPHY_PLL_P0_CTL5_PLL0_LPF_CTRL(x)		(((x) & 0xFF) <<  8)
#define  UPHY_PLL_P0_CTL5_PLL0_CP_CTRL(x)		(((x) & 0x0F) <<  4)
#define  UPHY_PLL_P0_CTL5_PLL0_PFD_CTRL(x)		(((x) & 0x03) <<  0)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL8		0x37c
#define  UPHY_PLL_P0_CTL8_PLL0_RCAL_DONE		(1U << 31)
#define  UPHY_PLL_P0_CTL8_PLL0_RCAL_VAL(x)		(((x) & 0x1F) << 24)
#define  UPHY_PLL_P0_CTL8_PLL0_RCAL_BYP_EN		(1 << 23)
#define  UPHY_PLL_P0_CTL8_PLL0_RCAL_BYP_CODE(x)		(((x) & 0x1F) << 16)
#define  UPHY_PLL_P0_CTL8_PLL0_RCAL_OVRD		(1 << 15)
#define  UPHY_PLL_P0_CTL8_PLL0_RCAL_CLK_EN		(1 << 13)
#define  UPHY_PLL_P0_CTL8_PLL0_RCAL_EN			(1 << 12)
#define  UPHY_PLL_P0_CTL8_PLL0_BGAP_CTRL(x)		(((x) & 0xFFF) <<  0)

#define XUSB_PADCTL_UPHY_MISC_PAD_P_CTL1(x)	(0x460 + (x) * 0x40)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL1		0x860
#define  UPHY_PLL_S0_CTL1_PLL0_FREQ_PSDIV(x)		(((x) & 0x03) << 28)
#define  UPHY_PLL_S0_CTL1_PLL0_FREQ_NDIV(x)		(((x) & 0xFF) << 20)
#define  UPHY_PLL_S0_CTL1_PLL0_FREQ_MDIV(x)		(((x) & 0x03) << 16)
#define  UPHY_PLL_S0_CTL1_PLL0_LOCKDET_STATUS 		(1 << 15)
#define  UPHY_PLL_S0_CTL1_PLL0_MODE_GET(x)		(((x) >> 8) & 0x03)
#define  UPHY_PLL_S0_CTL1_PLL0_BYPASS_EN 		(1 <<  7)
#define  UPHY_PLL_S0_CTL1_PLL0_FREERUN_EN		(1 <<  6)
#define  UPHY_PLL_S0_CTL1_PLL0_PWR_OVRD			(1 <<  4)
#define  UPHY_PLL_S0_CTL1_PLL0_ENABLE 			(1 <<  3)
#define  UPHY_PLL_S0_CTL1_PLL0_SLEEP(x)			(((x) & 0x03) <<  1)
#define  UPHY_PLL_S0_CTL1_PLL0_IDDQ 			(1 << 0)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL2		0x864
#define  UPHY_PLL_S0_CTL2_PLL0_CAL_CTRL(x)		(((x) & 0xFFFFFF) << 4)
#define  UPHY_PLL_S0_CTL2_PLL0_CAL_RESET		(1 << 3)
#define  UPHY_PLL_S0_CTL2_PLL0_CAL_OVRD			(1 << 2)
#define  UPHY_PLL_S0_CTL2_PLL0_CAL_DONE			(1 << 1)
#define  UPHY_PLL_S0_CTL2_PLL0_CAL_EN			(1 << 0)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL4		0x86c
#define  UPHY_PLL_S0_CTL4_PLL0_TCLKOUT_EN		(1 << 28)
#define  UPHY_PLL_S0_CTL4_PLL0_CLKDIST_CTRL(x)		(((x) & 0xF) << 20)
#define  UPHY_PLL_S0_CTL4_PLL0_XDIGCLK_EN		(1 << 19)
#define  UPHY_PLL_S0_CTL4_PLL0_XDIGCLK_SEL(x)		(((x) & 0x7) << 16)
#define  UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_EN		(1 << 15)
#define  UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_SEL(x)		(((x) & 0x3) << 12)
#define  UPHY_PLL_S0_CTL4_PLL0_FBCLKBUF_EN		(1 <<  9)
#define  UPHY_PLL_S0_CTL4_PLL0_REFCLKBUF_EN		(1 <<  8)
#define  UPHY_PLL_S0_CTL4_PLL0_REFCLK_SEL(x)		(((x) & 0xF) <<  4)
#define  UPHY_PLL_S0_CTL4_PLL0_REFCLK_TERM100		(1 <<  0)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL5		0x870
#define  UPHY_PLL_S0_CTL5_PLL0_DCO_CTRL(x)		(((x) & 0xFF) << 16)
#define  UPHY_PLL_S0_CTL5_PLL0_LPF_CTRL(x)		(((x) & 0xFF) <<  8)
#define  UPHY_PLL_S0_CTL5_PLL0_CP_CTRL(x)		(((x) & 0x0F) <<  4)
#define  UPHY_PLL_S0_CTL5_PLL0_PFD_CTRL(x)		(((x) & 0x03) <<  0)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL8		0x87c
#define  UPHY_PLL_S0_CTL8_PLL0_RCAL_DONE		(1U << 31)
#define  UPHY_PLL_S0_CTL8_PLL0_RCAL_VAL(x)		(((x) & 0x1F) << 24)
#define  UPHY_PLL_S0_CTL8_PLL0_RCAL_BYP_EN		(1 << 23)
#define  UPHY_PLL_S0_CTL8_PLL0_RCAL_BYP_CODE(x)		(((x) & 0x1F) << 16)
#define  UPHY_PLL_S0_CTL8_PLL0_RCAL_OVRD		(1 << 15)
#define  UPHY_PLL_S0_CTL8_PLL0_RCAL_CLK_EN		(1 << 13)
#define  UPHY_PLL_S0_CTL8_PLL0_RCAL_EN			(1 << 12)
#define  UPHY_PLL_S0_CTL8_PLL0_BGAP_CTRL(x)		(((x) & 0xFFF) <<  0)

#define XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL1	0x960
#define XUSB_PADCTL_UPHY_USB3_PAD_ECTL1(x)	(0xa60 + (x) * 0x40)
#define	 UPHY_USB3_PAD_ECTL1_TX_TERM_CTRL(x)		(((x) & 0x3) << 16)

#define XUSB_PADCTL_UPHY_USB3_PAD_ECTL2(x)	(0xa64 + (x) * 0x40)
#define	 UPHY_USB3_PAD_ECTL2_RX_IQ_CTRL(x)		(((x) & 0x000F) << 16)
#define	 UPHY_USB3_PAD_ECTL2_RX_CTLE(x)			(((x) & 0xFFFF) <<  0)

#define XUSB_PADCTL_UPHY_USB3_PAD_ECTL3(x)	(0xa68 + (x) * 0x40)
#define XUSB_PADCTL_UPHY_USB3_PAD_ECTL4(x)	(0xa6c + (x) * 0x40)
#define	 UPHY_USB3_PAD_ECTL4_RX_CDR_CTRL(x)		(((x) & 0xFFFF) << 16)
#define	 UPHY_USB3_PAD_ECTL4_RX_PI_CTRL(x)		(((x) & 0x00FF) <<  0)

#define XUSB_PADCTL_UPHY_USB3_PAD_ECTL6(x)	(0xa74 + (x) * 0x40)


#define	WR4(_sc, _r, _v)	bus_write_4((_sc)->mem_res, (_r), (_v))
#define	RD4(_sc, _r)		bus_read_4((_sc)->mem_res, (_r))


struct padctl_softc {
	device_t	dev;
	struct resource	*mem_res;
	hwreset_t	rst;
	int		phy_ena_cnt;
	int		pcie_ena_cnt;
	int		sata_ena_cnt;

	/* Fuses calibration data */
	/* USB2 */
	uint32_t	hs_curr_level[4];
	uint32_t	hs_curr_level_offs;	/* Not inited yet, always 0 */
	uint32_t	hs_term_range_adj;
	uint32_t	rpd_ctrl;

	/* HSIC */
	uint32_t	rx_strobe_trim;		/* Not inited yet, always 0 */
	uint32_t	rx_data0_trim;		/* Not inited yet, always 0 */
	uint32_t	rx_data1_trim;		/* Not inited yet, always 0 */
	uint32_t	tx_rtune_p;		/* Not inited yet, always 0 */
	uint32_t	strobe_trim;		/* Not inited yet, always 0 */
};

static struct ofw_compat_data compat_data[] = {
	{"nvidia,tegra210-xusb-padctl",	1},
	{NULL,				0},
};

/* Ports. */
enum padctl_port_type {
	PADCTL_PORT_USB2,
	PADCTL_PORT_HSIC,
	PADCTL_PORT_USB3,
};

struct padctl_lane;
struct padctl_port {
	enum padctl_port_type	type;
	const char		*name;
	const char		*base_name;
	int			idx;
	int			(*init)(struct padctl_softc *sc,
				    struct padctl_port *port);

	/* Runtime data. */
	phandle_t		xref;
	bool			enabled;
	bool			internal;
	uint32_t		companion;
	regulator_t		supply_vbus;
	struct padctl_lane	*lane;
};

static int usb3_port_init(struct padctl_softc *sc, struct padctl_port *port);

#define	PORT(t, n, p, i) {						\
	.type = t,							\
	.name = n "-" #p,						\
	.base_name = n,							\
	.idx = p,							\
	.init = i,							\
}
static struct padctl_port ports_tbl[] = {
	PORT(PADCTL_PORT_USB2, "usb2", 0, NULL),
	PORT(PADCTL_PORT_USB2, "usb2", 1, NULL),
	PORT(PADCTL_PORT_USB2, "usb2", 2, NULL),
	PORT(PADCTL_PORT_USB2, "usb2", 3, NULL),
	PORT(PADCTL_PORT_HSIC, "hsic", 0, NULL),
	PORT(PADCTL_PORT_HSIC, "hsic", 1, NULL),
	PORT(PADCTL_PORT_USB3, "usb3", 0, usb3_port_init),
	PORT(PADCTL_PORT_USB3, "usb3", 1, usb3_port_init),
};

/* Pads - a group of lannes. */
enum padctl_pad_type {
	PADCTL_PAD_USB2,
	PADCTL_PAD_HSIC,
	PADCTL_PAD_PCIE,
	PADCTL_PAD_SATA,
};

struct padctl_lane;
struct padctl_pad {
	const char		*name;
	enum padctl_pad_type	type;
	const char		*clock_name;
	char			*reset_name; 	/* XXX constify !!!!!! */
	int			(*enable)(struct padctl_softc *sc,
				    struct padctl_lane *lane);
	int			(*disable)(struct padctl_softc *sc,
				    struct padctl_lane *lane);
	/* Runtime data. */
	bool			enabled;
	clk_t			clk;
	hwreset_t		reset;
	int			nlanes;
	struct padctl_lane	*lanes[8]; 	/* Safe maximum value. */
};

static int usb2_enable(struct padctl_softc *sc, struct padctl_lane *lane);
static int usb2_disable(struct padctl_softc *sc, struct padctl_lane *lane);
static int hsic_enable(struct padctl_softc *sc, struct padctl_lane *lane);
static int hsic_disable(struct padctl_softc *sc, struct padctl_lane *lane);
static int pcie_enable(struct padctl_softc *sc, struct padctl_lane *lane);
static int pcie_disable(struct padctl_softc *sc, struct padctl_lane *lane);
static int sata_enable(struct padctl_softc *sc, struct padctl_lane *lane);
static int sata_disable(struct padctl_softc *sc, struct padctl_lane *lane);

#define	PAD(n, t, cn, rn, e, d) {						\
	.name = n,							\
	.type = t,							\
	.clock_name = cn,						\
	.reset_name = rn,						\
	.enable = e,							\
	.disable = d,							\
}
static struct padctl_pad pads_tbl[] = {
	PAD("usb2", PADCTL_PAD_USB2, "trk",  NULL, usb2_enable, usb2_disable),
	PAD("hsic", PADCTL_PAD_HSIC, "trk",  NULL, hsic_enable, hsic_disable),
	PAD("pcie", PADCTL_PAD_PCIE, "pll", "phy", pcie_enable, pcie_disable),
	PAD("sata", PADCTL_PAD_SATA, "pll", "phy", sata_enable, sata_disable),
};

/* Lanes. */
static char *usb_mux[] = {"snps", "xusb", "uart", "rsvd"};
static char *hsic_mux[] = {"snps", "xusb"};
static char *pci_mux[] = {"pcie-x1", "usb3-ss", "sata", "pcie-x4"};

struct padctl_lane {
	const char		*name;
	int			idx;
	bus_size_t		reg;
	uint32_t		shift;
	uint32_t		mask;
	char			**mux;
	int			nmux;
	/* Runtime data. */
	bool			enabled;
	phandle_t		xref;
	struct padctl_pad	*pad;
	struct padctl_port	*port;
	int			mux_idx;

};

#define	LANE(n, p, r, s, m, mx) {					\
	.name = n "-" #p,						\
	.idx = p,							\
	.reg = r,							\
	.shift = s,							\
	.mask = m,							\
	.mux = mx,							\
	.nmux = nitems(mx),						\
}
static struct padctl_lane lanes_tbl[] = {
	LANE("usb2", 0, XUSB_PADCTL_USB2_PAD_MUX,  0, 0x3, usb_mux),
	LANE("usb2", 1, XUSB_PADCTL_USB2_PAD_MUX,  2, 0x3, usb_mux),
	LANE("usb2", 2, XUSB_PADCTL_USB2_PAD_MUX,  4, 0x3, usb_mux),
	LANE("usb2", 3, XUSB_PADCTL_USB2_PAD_MUX,  6, 0x3, usb_mux),
	LANE("hsic", 0, XUSB_PADCTL_USB2_PAD_MUX, 14, 0x1, hsic_mux),
	LANE("hsic", 1, XUSB_PADCTL_USB2_PAD_MUX, 15, 0x1, hsic_mux),
	LANE("pcie", 0, XUSB_PADCTL_USB3_PAD_MUX, 12, 0x3, pci_mux),
	LANE("pcie", 1, XUSB_PADCTL_USB3_PAD_MUX, 14, 0x3, pci_mux),
	LANE("pcie", 2, XUSB_PADCTL_USB3_PAD_MUX, 16, 0x3, pci_mux),
	LANE("pcie", 3, XUSB_PADCTL_USB3_PAD_MUX, 18, 0x3, pci_mux),
	LANE("pcie", 4, XUSB_PADCTL_USB3_PAD_MUX, 20, 0x3, pci_mux),
	LANE("pcie", 5, XUSB_PADCTL_USB3_PAD_MUX, 22, 0x3, pci_mux),
	LANE("pcie", 6, XUSB_PADCTL_USB3_PAD_MUX, 24, 0x3, pci_mux),
	LANE("sata", 0, XUSB_PADCTL_USB3_PAD_MUX, 30, 0x3, pci_mux),
};

/* Define all possible mappings for USB3 port lanes */
struct padctl_lane_map {
	int			port_idx;
	enum padctl_pad_type	pad_type;
	int			lane_idx;
};

#define	LANE_MAP(pi, pt, li) {						\
	.port_idx = pi,							\
	.pad_type = pt,							\
	.lane_idx = li,							\
}
static struct padctl_lane_map lane_map_tbl[] = {
	LANE_MAP(0, PADCTL_PAD_PCIE, 6), 	/* port USB3-0 -> lane PCIE-0 */
	LANE_MAP(1, PADCTL_PAD_PCIE, 5), 	/* port USB3-1 -> lane PCIE-1 */
	LANE_MAP(2, PADCTL_PAD_PCIE, 0), 	/* port USB3-2 -> lane PCIE-0 */
	LANE_MAP(2, PADCTL_PAD_PCIE, 2), 	/* port USB3-2 -> lane PCIE-2 */
	LANE_MAP(3, PADCTL_PAD_PCIE, 4), 	/* port USB3-3 -> lane PCIE-4 */
};

/* Phy class and methods. */
static int xusbpadctl_phy_enable(struct phynode *phy, bool enable);
static phynode_method_t xusbpadctl_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,	xusbpadctl_phy_enable),
	PHYNODEMETHOD_END

};
DEFINE_CLASS_1(xusbpadctl_phynode, xusbpadctl_phynode_class,
    xusbpadctl_phynode_methods, 0, phynode_class);

static struct padctl_port *search_lane_port(struct padctl_softc *sc,
    struct padctl_lane *lane);


static void tegra210_xusb_pll_hw_control_enable(void) {}
static void tegra210_xusb_pll_hw_sequence_start(void) {}
static void tegra210_sata_pll_hw_control_enable(void) {}
static void tegra210_sata_pll_hw_sequence_start(void) {}

/* -------------------------------------------------------------------------
 *
 *   PEX functions
 */
static int
uphy_pex_enable(struct padctl_softc *sc, struct padctl_pad *pad)
{
	uint32_t reg;
	int rv, i;

	if (sc->pcie_ena_cnt > 0) {
		sc->pcie_ena_cnt++;
		return (0);
	}

	/* 22.8.4 UPHY PLLs, Step 4, page 1346 */
	/* 1. Deassert PLL/Lane resets. */
	rv = clk_enable(pad->clk);
	if (rv < 0) {
		device_printf(sc->dev, "Cannot enable clock for pad '%s': %d\n",
		    pad->name, rv);
		return (rv);
	}

	rv = hwreset_deassert(pad->reset);
	if (rv < 0) {
		device_printf(sc->dev, "Cannot unreset pad '%s': %d\n",
		    pad->name, rv);
		clk_disable(pad->clk);
		return (rv);
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
	reg &= ~UPHY_PLL_P0_CTL2_PLL0_CAL_CTRL(~0);
	reg |= UPHY_PLL_P0_CTL2_PLL0_CAL_CTRL(0x136);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL5);
	reg &= ~UPHY_PLL_P0_CTL5_PLL0_DCO_CTRL(~0);
	reg |= UPHY_PLL_P0_CTL5_PLL0_DCO_CTRL(0x2a);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL5, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1);
	reg |= UPHY_PLL_P0_CTL1_PLL0_PWR_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
	reg |= UPHY_PLL_P0_CTL2_PLL0_CAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8);
	reg |= UPHY_PLL_P0_CTL8_PLL0_RCAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8, reg);

	/*
	 * 2. For the following registers, default values
	 *    take care of the desired frequency.
	 */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL4);
	reg &= ~UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_SEL(~0);
	reg &= ~UPHY_PLL_P0_CTL4_PLL0_REFCLK_SEL(~0);
	reg |= UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_SEL(0x2);
	reg |= UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL4, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1);
	reg &= ~UPHY_PLL_P0_CTL1_PLL0_FREQ_MDIV(~0);
	reg &= ~UPHY_PLL_P0_CTL1_PLL0_FREQ_NDIV(~0);
	reg |= UPHY_PLL_P0_CTL1_PLL0_FREQ_NDIV(0x19);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1);
	reg &= ~UPHY_PLL_P0_CTL1_PLL0_IDDQ;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1);
	reg &= ~UPHY_PLL_P0_CTL1_PLL0_SLEEP(~0);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1, reg);

	/* 3. Wait 100 ns. */
	DELAY(10);

	/* XXX This in not in TRM */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL4);
	reg |= UPHY_PLL_P0_CTL4_PLL0_REFCLKBUF_EN;
	WR4(sc,  XUSB_PADCTL_UPHY_PLL_P0_CTL4, reg);

	/* 4. Calibration. */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
	reg |= UPHY_PLL_P0_CTL2_PLL0_CAL_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2, reg);
	for (i = 30; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
		if (reg & UPHY_PLL_P0_CTL2_PLL0_CAL_DONE)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in calibration step 1 "
		    "for pad '%s' (0x%08X).\n", pad->name, reg);
		rv = ETIMEDOUT;
		goto err;
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
	reg &= ~UPHY_PLL_P0_CTL2_PLL0_CAL_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2, reg);
	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
		if ((reg & UPHY_PLL_P0_CTL2_PLL0_CAL_DONE) == 0)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in calibration step 2 "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	/* 5. Enable the PLL (20 µs Lock time) */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1);
	reg |= UPHY_PLL_P0_CTL1_PLL0_ENABLE;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1, reg);
	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1);
		if (reg & UPHY_PLL_P0_CTL1_PLL0_LOCKDET_STATUS)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout while enabling PLL "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	/* 6. RCAL. */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8);
	reg |= UPHY_PLL_P0_CTL8_PLL0_RCAL_EN;
	reg |= UPHY_PLL_P0_CTL8_PLL0_RCAL_CLK_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8, reg);

	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8);
		if (reg & UPHY_PLL_P0_CTL8_PLL0_RCAL_DONE)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in RX calibration step 1 "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8);
	reg &= ~UPHY_PLL_P0_CTL8_PLL0_RCAL_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8, reg);

	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8);
		if (!(reg & UPHY_PLL_P0_CTL8_PLL0_RCAL_DONE))
			break;

		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in RX calibration step 2 "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8);
	reg &= ~UPHY_PLL_P0_CTL8_PLL0_RCAL_CLK_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8, reg);

	/* Enable Hardware Power Sequencer. */
	tegra210_xusb_pll_hw_control_enable();

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1);
	reg &= ~UPHY_PLL_P0_CTL1_PLL0_PWR_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
	reg &= ~UPHY_PLL_P0_CTL2_PLL0_CAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8);
	reg &= ~UPHY_PLL_P0_CTL8_PLL0_RCAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL8, reg);

	DELAY(50);

	tegra210_xusb_pll_hw_sequence_start();

	sc->pcie_ena_cnt++;

	return (0);

err:
	hwreset_deassert(pad->reset);
	clk_disable(pad->clk);
	return (rv);
}

static void
uphy_pex_disable(struct padctl_softc *sc, struct padctl_pad *pad)
{
	int rv;

	sc->pcie_ena_cnt--;
	if (sc->pcie_ena_cnt <= 0) {
		rv = hwreset_assert(pad->reset);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot reset pad '%s': %d\n",
			    pad->name, rv);
		}
		rv = clk_disable(pad->clk);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot dicable clock for pad '%s': %d\n",
			    pad->name, rv);
		}
	}
}

static int
uphy_sata_enable(struct padctl_softc *sc, struct padctl_pad *pad, bool usb)
{
	uint32_t reg;
	int rv, i;

	/* 22.8.4 UPHY PLLs, Step 4, page 1346 */
	/* 1. Deassert PLL/Lane resets. */
	if (sc->sata_ena_cnt > 0) {
		sc->sata_ena_cnt++;
		return (0);
	}

	rv = clk_enable(pad->clk);
	if (rv < 0) {
		device_printf(sc->dev, "Cannot enable clock for pad '%s': %d\n",
		    pad->name, rv);
		return (rv);
	}

	rv = hwreset_deassert(pad->reset);
	if (rv < 0) {
		device_printf(sc->dev, "Cannot unreset pad '%s': %d\n",
		    pad->name, rv);
		clk_disable(pad->clk);
		return (rv);
	}
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2);
	reg &= ~UPHY_PLL_P0_CTL2_PLL0_CAL_CTRL(~0);
	reg |= UPHY_PLL_P0_CTL2_PLL0_CAL_CTRL(0x136);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL2, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL5);
	reg &= ~UPHY_PLL_P0_CTL5_PLL0_DCO_CTRL(~0);
	reg |= UPHY_PLL_P0_CTL5_PLL0_DCO_CTRL(0x2a);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_P0_CTL5, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1);
	reg |= UPHY_PLL_S0_CTL1_PLL0_PWR_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2);
	reg |= UPHY_PLL_S0_CTL2_PLL0_CAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8);
	reg |= UPHY_PLL_S0_CTL8_PLL0_RCAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8, reg);

	/*
	 * 2. For the following registers, default values
	 *    take care of the desired frequency.
	 */
	 reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL4);
	reg &= ~UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_SEL(~0);
	reg &= ~UPHY_PLL_S0_CTL4_PLL0_REFCLK_SEL(~0);
	reg |= UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_EN;

	if (usb)
		reg |= UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_SEL(0x2);
	else
		reg |= UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_SEL(0x0);

	/* XXX PLL0_XDIGCLK_EN */
	/*
	value &= ~(1 << 19);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL4, reg);
	*/

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1);
	reg &= ~UPHY_PLL_S0_CTL1_PLL0_FREQ_MDIV(~0);
	reg &= ~UPHY_PLL_S0_CTL1_PLL0_FREQ_NDIV(~0);
	if (usb)
		reg |= UPHY_PLL_S0_CTL1_PLL0_FREQ_NDIV(0x19);
	else
		reg |= UPHY_PLL_S0_CTL1_PLL0_FREQ_NDIV(0x1e);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1);
	reg &= ~UPHY_PLL_S0_CTL1_PLL0_IDDQ;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1);
	reg &= ~UPHY_PLL_S0_CTL1_PLL0_SLEEP(~0);
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1, reg);

	/* 3. Wait 100 ns. */
	DELAY(1);

	/* XXX This in not in TRM */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL4);
	reg |= UPHY_PLL_S0_CTL4_PLL0_REFCLKBUF_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL4, reg);

	/* 4. Calibration. */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2);
	reg |= UPHY_PLL_S0_CTL2_PLL0_CAL_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2, reg);
	for (i = 30; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2);
		if (reg & UPHY_PLL_S0_CTL2_PLL0_CAL_DONE)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in calibration step 1 "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2);
	reg &= ~UPHY_PLL_S0_CTL2_PLL0_CAL_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2, reg);
	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2);
		if ((reg & UPHY_PLL_S0_CTL2_PLL0_CAL_DONE) == 0)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in calibration step 2 "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	/* 5. Enable the PLL (20 µs Lock time) */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1);
	reg |= UPHY_PLL_S0_CTL1_PLL0_ENABLE;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1, reg);
	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1);
		if (reg & UPHY_PLL_S0_CTL1_PLL0_LOCKDET_STATUS)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout while enabling PLL "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	/* 6. RCAL. */
	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8);
	reg |= UPHY_PLL_S0_CTL8_PLL0_RCAL_EN;
	reg |= UPHY_PLL_S0_CTL8_PLL0_RCAL_CLK_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8, reg);
	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8);
		if (reg & UPHY_PLL_S0_CTL8_PLL0_RCAL_DONE)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in RX calibration step 1 "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8);
	reg &= ~UPHY_PLL_S0_CTL8_PLL0_RCAL_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8, reg);
	for (i = 10; i > 0; i--) {
		reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8);
		if (!(reg & UPHY_PLL_S0_CTL8_PLL0_RCAL_DONE))
			break;
		DELAY(10);
	}
	if (i <= 0) {
		device_printf(sc->dev, "Timedout in RX calibration step 2 "
		    "for pad '%s'.\n", pad->name);
		rv = ETIMEDOUT;
		goto err;
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8);
	reg &= ~UPHY_PLL_S0_CTL8_PLL0_RCAL_CLK_EN;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8, reg);

	/* Enable Hardware Power Sequencer. */
	tegra210_sata_pll_hw_control_enable();

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1);
	reg &= ~UPHY_PLL_S0_CTL1_PLL0_PWR_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2);
	reg &= ~UPHY_PLL_S0_CTL2_PLL0_CAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL2, reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8);
	reg &= ~UPHY_PLL_S0_CTL8_PLL0_RCAL_OVRD;
	WR4(sc, XUSB_PADCTL_UPHY_PLL_S0_CTL8, reg);

	DELAY(50);

	tegra210_sata_pll_hw_sequence_start();

	sc->sata_ena_cnt++;

	return (0);

err:
	hwreset_deassert(pad->reset);
	clk_disable(pad->clk);
	return (rv);
}

static void
uphy_sata_disable(struct padctl_softc *sc, struct padctl_pad *pad)
{
	int rv;

	sc->sata_ena_cnt--;
	if (sc->sata_ena_cnt <= 0) {
		rv = hwreset_assert(pad->reset);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot reset pad '%s': %d\n",
			    pad->name, rv);
		}
		rv = clk_disable(pad->clk);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot dicable clock for pad '%s': %d\n",
			    pad->name, rv);
		}
	}
}


static int
usb3_port_init(struct padctl_softc *sc, struct padctl_port *port)
{
	uint32_t reg;
	struct padctl_pad *pad;
	int rv;

	pad = port->lane->pad;
	reg = RD4(sc, XUSB_PADCTL_SS_PORT_MAP);
	if (port->internal)
		reg &= ~SS_PORT_MAP_PORT_INTERNAL(port->idx);
	else
		reg |= SS_PORT_MAP_PORT_INTERNAL(port->idx);
	reg &= ~SS_PORT_MAP_PORT_MAP(port->idx, ~0);
	reg |= SS_PORT_MAP_PORT_MAP(port->idx, port->companion);
	WR4(sc, XUSB_PADCTL_SS_PORT_MAP, reg);

	if (port->supply_vbus != NULL) {
		rv = regulator_enable(port->supply_vbus);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot enable vbus regulator\n");
			return (rv);
		}
	}

	reg = RD4(sc, XUSB_PADCTL_UPHY_USB3_PAD_ECTL1(port->idx));
	reg &= ~UPHY_USB3_PAD_ECTL1_TX_TERM_CTRL(~0);
	reg |= UPHY_USB3_PAD_ECTL1_TX_TERM_CTRL(2);
	WR4(sc, XUSB_PADCTL_UPHY_USB3_PAD_ECTL1(port->idx), reg);

	reg = RD4(sc, XUSB_PADCTL_UPHY_USB3_PAD_ECTL2(port->idx));
	reg &= ~UPHY_USB3_PAD_ECTL2_RX_CTLE(~0);
	reg |= UPHY_USB3_PAD_ECTL2_RX_CTLE(0x00fc);
	WR4(sc, XUSB_PADCTL_UPHY_USB3_PAD_ECTL2(port->idx), reg);

	WR4(sc, XUSB_PADCTL_UPHY_USB3_PAD_ECTL3(port->idx), 0xc0077f1f);

	reg = RD4(sc,  XUSB_PADCTL_UPHY_USB3_PAD_ECTL4(port->idx));
	reg &= ~UPHY_USB3_PAD_ECTL4_RX_CDR_CTRL(~0);
	reg |= UPHY_USB3_PAD_ECTL4_RX_CDR_CTRL(0x01c7);
	WR4(sc, XUSB_PADCTL_UPHY_USB3_PAD_ECTL4(port->idx), reg);

	WR4(sc, XUSB_PADCTL_UPHY_USB3_PAD_ECTL6(port->idx), 0xfcf01368);

	if (pad->type == PADCTL_PAD_SATA)
		rv = uphy_sata_enable(sc, pad, true);
	else
		rv = uphy_pex_enable(sc, pad);
	if (rv != 0)
		return (rv);

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_SSP_ELPG_VCORE_DOWN(port->idx);
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_SSP_ELPG_CLAMP_EN_EARLY(port->idx);
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_SSP_ELPG_CLAMP_EN(port->idx);
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	return (0);
}

static int
pcie_enable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;
	int rv;

	rv = uphy_pex_enable(sc, lane->pad);
	if (rv != 0)
		return (rv);

	reg = RD4(sc, XUSB_PADCTL_USB3_PAD_MUX);
	reg |= USB3_PAD_MUX_PCIE_IDDQ_DISABLE(lane->idx);
	WR4(sc, XUSB_PADCTL_USB3_PAD_MUX, reg);

	return (0);
}

static int
pcie_disable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;

	reg = RD4(sc, XUSB_PADCTL_USB3_PAD_MUX);
	reg &= ~USB3_PAD_MUX_PCIE_IDDQ_DISABLE(lane->idx);
	WR4(sc, XUSB_PADCTL_USB3_PAD_MUX, reg);

	uphy_pex_disable(sc, lane->pad);

	return (0);

}

static int
sata_enable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;
	int rv;

	rv = uphy_sata_enable(sc, lane->pad, false);
	if (rv != 0)
		return (rv);

	reg = RD4(sc, XUSB_PADCTL_USB3_PAD_MUX);
	reg |= USB3_PAD_MUX_SATA_IDDQ_DISABLE(lane->idx);
	WR4(sc, XUSB_PADCTL_USB3_PAD_MUX, reg);

	return (0);
}

static int
sata_disable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;

	reg = RD4(sc, XUSB_PADCTL_USB3_PAD_MUX);
	reg &= ~USB3_PAD_MUX_SATA_IDDQ_DISABLE(lane->idx);
	WR4(sc, XUSB_PADCTL_USB3_PAD_MUX, reg);

	uphy_sata_disable(sc, lane->pad);

	return (0);
}

static int
hsic_enable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;
	struct padctl_pad *pad;
	struct padctl_port *port;
	int rv;

	port = search_lane_port(sc, lane);
	if (port == NULL) {
		device_printf(sc->dev, "Cannot find port for lane: %s\n",
		    lane->name);
	}
	pad = lane->pad;

	if (port->supply_vbus != NULL) {
		rv = regulator_enable(port->supply_vbus);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot enable vbus regulator\n");
			return (rv);
		}
	}

	WR4(sc, XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL, sc->strobe_trim);

	reg = RD4(sc, XUSB_PADCTL_HSIC_PAD_CTL1(lane->idx));
	reg &= ~HSIC_PAD_CTL1_TX_RTUNEP(~0);
	reg |= HSIC_PAD_CTL1_TX_RTUNEP(sc->tx_rtune_p);
	WR4(sc, XUSB_PADCTL_HSIC_PAD_CTL1(lane->idx), reg);

	reg = RD4(sc, XUSB_PADCTL_HSIC_PAD_CTL2(lane->idx));
	reg &= ~HSIC_PAD_CTL2_RX_STROBE_TRIM(~0);
	reg &= ~HSIC_PAD_CTL2_RX_DATA1_TRIM(~0);
	reg &= ~HSIC_PAD_CTL2_RX_DATA0_TRIM(~0);
	reg |= HSIC_PAD_CTL2_RX_STROBE_TRIM(sc->rx_strobe_trim);
	reg |= HSIC_PAD_CTL2_RX_DATA1_TRIM(sc->rx_data1_trim);
	reg |= HSIC_PAD_CTL2_RX_DATA0_TRIM(sc->rx_data0_trim);
	WR4(sc, XUSB_PADCTL_HSIC_PAD_CTL2(lane->idx), reg);

	reg = RD4(sc, XUSB_PADCTL_HSIC_PAD_CTL0(lane->idx));
	reg &= ~HSIC_PAD_CTL0_RPU_DATA0;
	reg &= ~HSIC_PAD_CTL0_RPU_DATA1;
	reg &= ~HSIC_PAD_CTL0_RPU_STROBE;
	reg &= ~HSIC_PAD_CTL0_PD_RX_DATA0;
	reg &= ~HSIC_PAD_CTL0_PD_RX_DATA1;
	reg &= ~HSIC_PAD_CTL0_PD_RX_STROBE;
	reg &= ~HSIC_PAD_CTL0_PD_ZI_DATA0;
	reg &= ~HSIC_PAD_CTL0_PD_ZI_DATA1;
	reg &= ~HSIC_PAD_CTL0_PD_ZI_STROBE;
	reg &= ~HSIC_PAD_CTL0_PD_TX_DATA0;
	reg &= ~HSIC_PAD_CTL0_PD_TX_DATA1;
	reg &= ~HSIC_PAD_CTL0_PD_TX_STROBE;
	reg |= HSIC_PAD_CTL0_RPD_DATA0;
	reg |= HSIC_PAD_CTL0_RPD_DATA1;
	reg |= HSIC_PAD_CTL0_RPD_STROBE;
	WR4(sc, XUSB_PADCTL_HSIC_PAD_CTL0(lane->idx), reg);

	rv = clk_enable(pad->clk);
	if (rv < 0) {
		device_printf(sc->dev, "Cannot enable clock for pad '%s': %d\n",
		    pad->name, rv);
		if (port->supply_vbus != NULL)
			regulator_disable(port->supply_vbus);
		return (rv);
	}

	reg = RD4(sc, XUSB_PADCTL_HSIC_PAD_TRK_CTL);
	reg &= ~HSIC_PAD_TRK_CTL_TRK_START_TIMER(~0);
	reg &= ~HSIC_PAD_TRK_CTL_TRK_DONE_RESET_TIMER(~0);
	reg |= HSIC_PAD_TRK_CTL_TRK_START_TIMER(0x1e);
	reg |= HSIC_PAD_TRK_CTL_TRK_DONE_RESET_TIMER(0x0a);
	WR4(sc, XUSB_PADCTL_HSIC_PAD_TRK_CTL, reg);

	DELAY(10);

	reg = RD4(sc, XUSB_PADCTL_HSIC_PAD_TRK_CTL);
	reg &= ~HSIC_PAD_TRK_CTL_PD_TRK;
	WR4(sc, XUSB_PADCTL_HSIC_PAD_TRK_CTL, reg);

	DELAY(50);
	clk_disable(pad->clk);
	return (0);
}

static int
hsic_disable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;
	struct padctl_port *port;
	int rv;

	port = search_lane_port(sc, lane);
	if (port == NULL) {
		device_printf(sc->dev, "Cannot find port for lane: %s\n",
		    lane->name);
	}

	reg = RD4(sc, XUSB_PADCTL_HSIC_PAD_CTL0(lane->idx));
	reg |= HSIC_PAD_CTL0_PD_RX_DATA0;
	reg |= HSIC_PAD_CTL0_PD_RX_DATA1;
	reg |= HSIC_PAD_CTL0_PD_RX_STROBE;
	reg |= HSIC_PAD_CTL0_PD_ZI_DATA0;
	reg |= HSIC_PAD_CTL0_PD_ZI_DATA1;
	reg |= HSIC_PAD_CTL0_PD_ZI_STROBE;
	reg |= HSIC_PAD_CTL0_PD_TX_DATA0;
	reg |= HSIC_PAD_CTL0_PD_TX_DATA1;
	reg |= HSIC_PAD_CTL0_PD_TX_STROBE;
	WR4(sc, XUSB_PADCTL_HSIC_PAD_CTL1(lane->idx), reg);

	if (port->supply_vbus != NULL) {
		rv = regulator_disable(port->supply_vbus);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot disable vbus regulator\n");
			return (rv);
		}
	}

	return (0);
}

static int
usb2_enable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;
	struct padctl_pad *pad;
	struct padctl_port *port;
	int rv;

	port = search_lane_port(sc, lane);
	if (port == NULL) {
		device_printf(sc->dev, "Cannot find port for lane: %s\n",
		    lane->name);
	}
	pad = lane->pad;

	reg = RD4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
	reg &= ~USB2_BIAS_PAD_CTL0_HS_SQUELCH_LEVEL(~0);
	reg &= ~USB2_BIAS_PAD_CTL0_HS_DISCON_LEVEL(~0);
	reg |= USB2_BIAS_PAD_CTL0_HS_DISCON_LEVEL(0x7);
	WR4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, reg);

	reg = RD4(sc, XUSB_PADCTL_USB2_PORT_CAP);
	reg &= ~USB2_PORT_CAP_PORT_CAP(lane->idx, ~0);
	reg |= USB2_PORT_CAP_PORT_CAP(lane->idx, USB2_PORT_CAP_PORT_CAP_HOST);
	WR4(sc, XUSB_PADCTL_USB2_PORT_CAP, reg);

	reg = RD4(sc, XUSB_PADCTL_USB2_OTG_PAD_CTL0(lane->idx));
	reg &= ~USB2_OTG_PAD_CTL0_HS_CURR_LEVEL(~0);
	reg &= ~USB2_OTG_PAD_CTL0_HS_SLEW(~0);
	reg &= ~USB2_OTG_PAD_CTL0_PD;
	reg &= ~USB2_OTG_PAD_CTL0_PD2;
	reg &= ~USB2_OTG_PAD_CTL0_PD_ZI;
	reg |= USB2_OTG_PAD_CTL0_HS_SLEW(14);
	reg |= USB2_OTG_PAD_CTL0_HS_CURR_LEVEL(sc->hs_curr_level[lane->idx] +
	    sc->hs_curr_level_offs);
	WR4(sc, XUSB_PADCTL_USB2_OTG_PAD_CTL0(lane->idx), reg);

	reg = RD4(sc, XUSB_PADCTL_USB2_OTG_PAD_CTL1(lane->idx));
	reg &= ~USB2_OTG_PAD_CTL1_TERM_RANGE_ADJ(~0);
	reg &= ~USB2_OTG_PAD_CTL1_RPD_CTRL(~0);
	reg &= ~USB2_OTG_PAD_CTL1_PD_DR;
	reg &= ~USB2_OTG_PAD_CTL1_PD_CHRP_OVRD;
	reg &= ~USB2_OTG_PAD_CTL1_PD_DISC_OVRD;
	reg |= USB2_OTG_PAD_CTL1_TERM_RANGE_ADJ(sc->hs_term_range_adj);
	reg |= USB2_OTG_PAD_CTL1_RPD_CTRL(sc->rpd_ctrl);
	WR4(sc, XUSB_PADCTL_USB2_OTG_PAD_CTL1(lane->idx), reg);

	reg = RD4(sc, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(lane->idx));
	reg &= ~USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_LEV(~0);
	reg |= USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_FIX18;
	WR4(sc, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(lane->idx), reg);

	if (port->supply_vbus != NULL) {
		rv = regulator_enable(port->supply_vbus);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot enable vbus regulator\n");
			return (rv);
		}
	}
	rv = clk_enable(pad->clk);
	if (rv < 0) {
		device_printf(sc->dev, "Cannot enable clock for pad '%s': %d\n",
		    pad->name, rv);
		if (port->supply_vbus != NULL)
			regulator_disable(port->supply_vbus);
		return (rv);
	}
	reg = RD4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
	reg &= ~USB2_BIAS_PAD_CTL1_TRK_START_TIMER(~0);
	reg &= ~USB2_BIAS_PAD_CTL1_TRK_DONE_RESET_TIMER(~0);
	reg |= USB2_BIAS_PAD_CTL1_TRK_START_TIMER(0x1e);
	reg |= USB2_BIAS_PAD_CTL1_TRK_DONE_RESET_TIMER(0x0a);
	WR4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, reg);

	reg = RD4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
	reg &= ~USB2_BIAS_PAD_CTL0_PD;
	WR4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, reg);
	return (0);
}

static int
usb2_disable(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;
	struct padctl_pad *pad;
	struct padctl_port *port;
	int rv;

	port = search_lane_port(sc, lane);
	if (port == NULL) {
		device_printf(sc->dev, "Cannot find port for lane: %s\n",
		    lane->name);
	}
	pad = lane->pad;

	reg = RD4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
	reg |= USB2_BIAS_PAD_CTL0_PD;
	WR4(sc, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, reg);

	if (port->supply_vbus != NULL) {
		rv = regulator_disable(port->supply_vbus);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot disable vbus regulator\n");
			return (rv);
		}
	}

	rv = clk_disable(pad->clk);
	if (rv < 0) {
		device_printf(sc->dev, "Cannot disable clock for pad '%s': %d\n",
		    pad->name, rv);
		return (rv);
	}

	return (0);
}


static int
pad_common_enable(struct padctl_softc *sc)
{
	uint32_t reg;

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN;
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN_EARLY;
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_AUX_MUX_LP0_VCORE_DOWN;
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	return (0);
}

static int
pad_common_disable(struct padctl_softc *sc)
{
	uint32_t reg;

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg |= ELPG_PROGRAM1_AUX_MUX_LP0_VCORE_DOWN;
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg |= ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN_EARLY;
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = RD4(sc, XUSB_PADCTL_ELPG_PROGRAM1);
	reg |= ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN;
	WR4(sc, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	return (0);
}

static int
xusbpadctl_phy_enable(struct phynode *phy, bool enable)
{
	device_t dev;
	intptr_t id;
	struct padctl_softc *sc;
	struct padctl_lane *lane;
	struct padctl_pad *pad;
	int rv;

	dev = phynode_get_device(phy);
	id = phynode_get_id(phy);
	sc = device_get_softc(dev);

	if (id < 0 || id >= nitems(lanes_tbl)) {
		device_printf(dev, "Unknown phy: %d\n", (int)id);
		return (ENXIO);
	}

	lane = lanes_tbl + id;
	if (!lane->enabled) {
		device_printf(dev, "Lane is not enabled/configured: %s\n",
		    lane->name);
		return (ENXIO);
	}

	pad = lane->pad;
	if (enable) {
		if (sc->phy_ena_cnt == 0) {
			rv = pad_common_enable(sc);
			if (rv != 0)
				return (rv);
		}
		sc->phy_ena_cnt++;
	}

	if (enable)
		rv = pad->enable(sc, lane);
	else
		rv = pad->disable(sc, lane);
	if (rv != 0)
		return (rv);

	if (!enable) {
		 if (sc->phy_ena_cnt == 1) {
			rv = pad_common_disable(sc);
			if (rv != 0)
				return (rv);
		}
		sc->phy_ena_cnt--;
	}

	return (0);
}

/* -------------------------------------------------------------------------
 *
 *   FDT processing
 */
static struct padctl_port *
search_port(struct padctl_softc *sc, char *port_name)
{
	int i;

	for (i = 0; i < nitems(ports_tbl); i++) {
		if (strcmp(port_name, ports_tbl[i].name) == 0)
			return (&ports_tbl[i]);
	}
	return (NULL);
}

static struct padctl_port *
search_lane_port(struct padctl_softc *sc, struct padctl_lane *lane)
{
	int i;

	for (i = 0; i < nitems(ports_tbl); i++) {
		if (!ports_tbl[i].enabled)
			continue;
		if (ports_tbl[i].lane == lane)
			return (ports_tbl + i);
	}
	return (NULL);
}

static struct padctl_lane *
search_lane(struct padctl_softc *sc, char *lane_name)
{
	int i;

	for (i = 0; i < nitems(lanes_tbl); i++) {
		if (strcmp(lane_name, lanes_tbl[i].name) == 0)
			return 	(lanes_tbl + i);
	}
	return (NULL);
}

static struct padctl_lane *
search_pad_lane(struct padctl_softc *sc, enum padctl_pad_type type, int idx)
{
	int i;

	for (i = 0; i < nitems(lanes_tbl); i++) {
		if (!lanes_tbl[i].enabled)
			continue;
		if (type == lanes_tbl[i].pad->type && idx == lanes_tbl[i].idx)
			return 	(lanes_tbl + i);
	}
	return (NULL);
}

static struct padctl_lane *
search_usb3_pad_lane(struct padctl_softc *sc, int idx)
{
	int i;
	struct padctl_lane *lane, *tmp;

	lane = NULL;
	for (i = 0; i < nitems(lane_map_tbl); i++) {
		if (idx != lane_map_tbl[i].port_idx)
			continue;
		tmp = search_pad_lane(sc, lane_map_tbl[i].pad_type,
		    lane_map_tbl[i].lane_idx);
		if (tmp == NULL)
			continue;
		if (strcmp(tmp->mux[tmp->mux_idx], "usb3-ss") != 0)
			continue;
		if (lane != NULL) {
			device_printf(sc->dev, "Duplicated mappings found for"
			 " lanes: %s and %s\n", lane->name, tmp->name);
			return (NULL);
		}
		lane = tmp;
	}
	return (lane);
}

static struct padctl_pad *
search_pad(struct padctl_softc *sc, char *pad_name)
{
	int i;

	for (i = 0; i < nitems(pads_tbl); i++) {
		if (strcmp(pad_name, pads_tbl[i].name) == 0)
			return 	(pads_tbl + i);
	}
	return (NULL);
}

static int
search_mux(struct padctl_softc *sc, struct padctl_lane *lane, char *fnc_name)
{
	int i;

	for (i = 0; i < lane->nmux; i++) {
		if (strcmp(fnc_name, lane->mux[i]) == 0)
			return 	(i);
	}
	return (-1);
}

static int
config_lane(struct padctl_softc *sc, struct padctl_lane *lane)
{
	uint32_t reg;

	reg = RD4(sc, lane->reg);
	reg &= ~(lane->mask << lane->shift);
	reg |=  (lane->mux_idx & lane->mask) << lane->shift;
	WR4(sc, lane->reg, reg);
	return (0);
}

static int
process_lane(struct padctl_softc *sc, phandle_t node, struct padctl_pad *pad)
{
	struct padctl_lane *lane;
	struct phynode *phynode;
	struct phynode_init_def phy_init;
	char *name;
	char *function;
	int rv;

	name = NULL;
	function = NULL;
	rv = OF_getprop_alloc(node, "name", (void **)&name);
	if (rv <= 0) {
		device_printf(sc->dev, "Cannot read lane name.\n");
		return (ENXIO);
	}

	lane = search_lane(sc, name);
	if (lane == NULL) {
		device_printf(sc->dev, "Unknown lane: %s\n", name);
		rv = ENXIO;
		goto end;
	}

	/* Read function (mux) settings. */
	rv = OF_getprop_alloc(node, "nvidia,function", (void **)&function);
	if (rv <= 0) {
		device_printf(sc->dev, "Cannot read lane function.\n");
		rv = ENXIO;
		goto end;
	}

	lane->mux_idx = search_mux(sc, lane, function);
	if (lane->mux_idx == ~0) {
		device_printf(sc->dev, "Unknown function %s for lane %s\n",
		    function, name);
		rv = ENXIO;
		goto end;
	}

	rv = config_lane(sc, lane);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot configure lane: %s: %d\n",
		    name, rv);
		rv = ENXIO;
		goto end;
	}
	lane->xref = OF_xref_from_node(node);
	lane->pad = pad;
	lane->enabled = true;
	pad->lanes[pad->nlanes++] = lane;

	/* Create and register phy. */
	bzero(&phy_init, sizeof(phy_init));
	phy_init.id = lane - lanes_tbl;
	phy_init.ofw_node = node;
	phynode = phynode_create(sc->dev, &xusbpadctl_phynode_class, &phy_init);
	if (phynode == NULL) {
		device_printf(sc->dev, "Cannot create phy\n");
		rv = ENXIO;
		goto end;
	}
	if (phynode_register(phynode) == NULL) {
		device_printf(sc->dev, "Cannot create phy\n");
		return (ENXIO);
	}

	rv = 0;

end:
	if (name != NULL)
		OF_prop_free(name);
	if (function != NULL)
		OF_prop_free(function);
	return (rv);
}

static int
process_pad(struct padctl_softc *sc, phandle_t node)
{
	phandle_t  xref;
	struct padctl_pad *pad;
	char *name;
	int rv;

	name = NULL;
	rv = OF_getprop_alloc(node, "name", (void **)&name);
	if (rv <= 0) {
		device_printf(sc->dev, "Cannot read pad name.\n");
		return (ENXIO);
	}

	pad = search_pad(sc, name);
	if (pad == NULL) {
		device_printf(sc->dev, "Unknown pad: %s\n", name);
		rv = ENXIO;
		goto end;
	}

	if (pad->clock_name != NULL) {
		rv = clk_get_by_ofw_name(sc->dev, node, pad->clock_name,
		    &pad->clk);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot get '%s' clock\n",
			    pad->clock_name);
			return (ENXIO);
		}
	}

	if (pad->reset_name != NULL) {
		rv = hwreset_get_by_ofw_name(sc->dev, node, pad->reset_name,
		    &pad->reset);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot get '%s' reset\n",
			    pad->reset_name);
			return (ENXIO);
		}
	}

	/* Read and process associated lanes. */
	node = ofw_bus_find_child(node, "lanes");
	if (node <= 0) {
		device_printf(sc->dev, "Cannot find 'lanes' subnode\n");
		rv = ENXIO;
		goto end;
	}

	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;

		rv = process_lane(sc, node, pad);
		if (rv != 0)
			goto end;

		xref = OF_xref_from_node(node);
		OF_device_register_xref(xref, sc->dev);
	}
	pad->enabled = true;
	rv = 0;
end:
	if (name != NULL)
		OF_prop_free(name);
	return (rv);
}

static int
process_port(struct padctl_softc *sc, phandle_t node)
{

	struct padctl_port *port;
	char *name;
	int rv;

	name = NULL;
	rv = OF_getprop_alloc(node, "name", (void **)&name);
	if (rv <= 0) {
		device_printf(sc->dev, "Cannot read port name.\n");
		return (ENXIO);
	}

	port = search_port(sc, name);
	if (port == NULL) {
		device_printf(sc->dev, "Unknown port: %s\n", name);
		rv = ENXIO;
		goto end;
	}

	regulator_get_by_ofw_property(sc->dev, node,
	    "vbus-supply", &port->supply_vbus);

	if (OF_hasprop(node, "nvidia,internal"))
		port->internal = true;

	/* Find assigned lane */
	if (port->lane == NULL) {
		switch(port->type) {
		/* Routing is fixed for USB2 AND HSIC. */
		case PADCTL_PORT_USB2:
			port->lane = search_pad_lane(sc, PADCTL_PAD_USB2,
			    port->idx);
			break;
		case PADCTL_PORT_HSIC:
			port->lane = search_pad_lane(sc, PADCTL_PAD_HSIC,
			    port->idx);
			break;
		case PADCTL_PORT_USB3:
			port->lane = search_usb3_pad_lane(sc, port->idx);
			break;
		}
	}
	if (port->lane == NULL) {
		device_printf(sc->dev, "Cannot find lane for port: %s\n", name);
		rv = ENXIO;
		goto end;
	}

	if (port->type == PADCTL_PORT_USB3) {
		rv = OF_getencprop(node,  "nvidia,usb2-companion",
		   &(port->companion), sizeof(port->companion));
		if (rv <= 0) {
			device_printf(sc->dev,
			    "Missing 'nvidia,usb2-companion' property "
			    "for port: %s\n", name);
			rv = ENXIO;
			goto end;
		}
	}

	port->enabled = true;
	rv = 0;
end:
	if (name != NULL)
		OF_prop_free(name);
	return (rv);
}

static int
parse_fdt(struct padctl_softc *sc, phandle_t base_node)
{
	phandle_t node;
	int rv;

	rv = 0;
	node = ofw_bus_find_child(base_node, "pads");

	if (node <= 0) {
		device_printf(sc->dev, "Cannot find pads subnode.\n");
		return (ENXIO);
	}
	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;
		rv = process_pad(sc, node);
		if (rv != 0)
			return (rv);
	}

	node = ofw_bus_find_child(base_node, "ports");
	if (node <= 0) {
		device_printf(sc->dev, "Cannot find ports subnode.\n");
		return (ENXIO);
	}
	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;
		rv = process_port(sc, node);
		if (rv != 0)
			return (rv);
	}

	return (0);
}

static void
load_calibration(struct padctl_softc *sc)
{
	uint32_t reg;
	int i;

	reg = tegra_fuse_read_4(FUSE_SKU_CALIB_0);
	sc->hs_curr_level[0] = FUSE_SKU_CALIB_0_HS_CURR_LEVEL_0(reg);
	for (i = 1; i < nitems(sc->hs_curr_level); i++) {
		sc->hs_curr_level[i] =
		    FUSE_SKU_CALIB_0_HS_CURR_LEVEL_123(reg, i);
	}
	sc->hs_term_range_adj = FUSE_SKU_CALIB_0_HS_TERM_RANGE_ADJ(reg);

	tegra_fuse_read_4(FUSE_USB_CALIB_EXT_0);
	sc->rpd_ctrl = FUSE_USB_CALIB_EXT_0_RPD_CTRL(reg);
}

/* -------------------------------------------------------------------------
 *
 *   BUS functions
 */
static int
xusbpadctl_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Tegra XUSB phy");
	return (BUS_PROBE_DEFAULT);
}

static int
xusbpadctl_detach(device_t dev)
{

	/* This device is always present. */
	return (EBUSY);
}

static int
xusbpadctl_attach(device_t dev)
{
	struct padctl_softc * sc;
	int i, rid, rv;
	struct padctl_port *port;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		return (ENXIO);
	}

	rv = hwreset_get_by_ofw_name(dev, 0, "padctl", &sc->rst);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'padctl' reset: %d\n", rv);
		return (rv);
	}
	rv = hwreset_deassert(sc->rst);
	if (rv != 0) {
		device_printf(dev, "Cannot unreset 'padctl' reset: %d\n", rv);
		return (rv);
	}

	load_calibration(sc);

	rv = parse_fdt(sc, node);
	if (rv != 0) {
		device_printf(dev, "Cannot parse fdt configuration: %d\n", rv);
		return (rv);
	}
	for (i = 0; i < nitems(ports_tbl); i++) {
		port = ports_tbl + i;
		if (!port->enabled)
			continue;
		if (port->init == NULL)
			continue;
		rv = port->init(sc, port);
		if (rv != 0) {
			device_printf(dev, "Cannot init port '%s'\n",
			    port->name);
			return (rv);
		}
	}
	return (0);
}

static device_method_t tegra_xusbpadctl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         xusbpadctl_probe),
	DEVMETHOD(device_attach,        xusbpadctl_attach),
	DEVMETHOD(device_detach,        xusbpadctl_detach),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(xusbpadctl, tegra_xusbpadctl_driver,
    tegra_xusbpadctl_methods, sizeof(struct padctl_softc));
EARLY_DRIVER_MODULE(tegra_xusbpadctl, simplebus, tegra_xusbpadctl_driver,
    NULL, NULL, 73);
