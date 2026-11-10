/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@freebsd.org>
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

#include <dev/clk/clk_div.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_mux.h>

#include <dev/clk/allwinner/aw_ccung.h>

#include <dt-bindings/clock/sun50i-h6-ccu.h>
#include <dt-bindings/reset/sun50i-h6-ccu.h>

/* Non-exported clocks */
#define	CLK_OSC_12M		0
#define	CLK_PLL_CPUX		1
#define	CLK_PLL_DDR0		2
#define	CLK_PLL_PERIPH0_2X	4
#define	CLK_PLL_PERIPH0_4X	5
#define	CLK_PLL_PERIPH1		6
#define	CLK_PLL_PERIPH1_2X	7
#define	CLK_PLL_PERIPH1_4X	8
#define	CLK_PLL_GPU		9
#define	CLK_PLL_VIDEO0		10
#define	CLK_PLL_VIDEO0_4X	11
#define	CLK_PLL_VIDEO1		12
#define	CLK_PLL_VIDEO1_4X	13
#define	CLK_PLL_VE		14
#define	CLK_PLL_DE		14
#define	CLK_PLL_HSIC		16

#define	CLK_AXI			22
#define	CLK_CPUX_APB		23
#define	CLK_PSI_AHB1_AHB2	24
#define	CLK_AHB3		25
#define	CLK_APB2		27
#define	CLK_MBUS		28
#define	CLK_DRAM		52
#define	CLK_BUS_DRAM		60

static struct aw_ccung_reset h6_ccu_resets[] = {
	/* MBUS_CFG_REG */
	CCU_RESET(RST_MBUS, 0x540, 30)

	/* DE_BGR_REG */
	CCU_RESET(RST_BUS_DE, 0x60c, 16)

	/* DI_BGR_REG */
	CCU_RESET(RST_BUS_DEINTERLACE, 0x62c, 16)

	/* GPU_BGR_REG */
	CCU_RESET(RST_BUS_GPU, 0x67c, 16)

	/* CE_BGR_REG */
	CCU_RESET(RST_BUS_CE, 0x68c, 16)

	/* VE_BGR_REG */
	CCU_RESET(RST_BUS_VE, 0x69c, 16)

	/* EMCE_BGR_REG */
	CCU_RESET(RST_BUS_EMCE, 0x6bc, 16)

	/* VP9_BGR_REG */
	CCU_RESET(RST_BUS_VP9, 0x6cc, 16)

	/* DMA_BGR_REG */
	CCU_RESET(RST_BUS_DMA, 0x70c, 16)

	/* MSGBOX_BGR_REG */
	CCU_RESET(RST_BUS_MSGBOX, 0x71c, 16)

	/* SPINLOCK_BGR_REG */
	CCU_RESET(RST_BUS_SPINLOCK, 0x72c, 16)

	/* HSTIMER_BGR_REG */
	CCU_RESET(RST_BUS_HSTIMER, 0x73c, 16)

	/* DBGSYS_BGR_REG */
	CCU_RESET(RST_BUS_DBG, 0x78c, 16)

	/* PSI_BGR_REG */
	CCU_RESET(RST_BUS_PSI, 0x79c, 16)

	/* PWM_BGR_REG */
	CCU_RESET(RST_BUS_PWM, 0x7ac, 16)

	/*
	 * IOMMU_BGR_REG, the manual has no reset in this register but the
	 * binding has RST_BUS_IOMMU and Linux uses the bit 16 for it.  This
	 * bit cannot be written.
	 */
	CCU_RESET(RST_BUS_IOMMU, 0x7bc, 16)

	/* DRAM_BGR_REG */
	CCU_RESET(RST_BUS_DRAM, 0x80c, 16)

	/* NAND_BGR_REG */
	CCU_RESET(RST_BUS_NAND, 0x82c, 16)

	/* SMHC_BGR_REG */
	CCU_RESET(RST_BUS_MMC0, 0x84c, 16)
	CCU_RESET(RST_BUS_MMC1, 0x84c, 17)
	CCU_RESET(RST_BUS_MMC2, 0x84c, 18)

	/* UART_BGR_REG */
	CCU_RESET(RST_BUS_UART0, 0x90c, 16)
	CCU_RESET(RST_BUS_UART1, 0x90c, 17)
	CCU_RESET(RST_BUS_UART2, 0x90c, 18)
	CCU_RESET(RST_BUS_UART3, 0x90c, 19)

	/* TWI_BGR_REG */
	CCU_RESET(RST_BUS_I2C0, 0x91c, 16)
	CCU_RESET(RST_BUS_I2C1, 0x91c, 17)
	CCU_RESET(RST_BUS_I2C2, 0x91c, 18)
	CCU_RESET(RST_BUS_I2C3, 0x91c, 19)

	/* SCR_BGR_REG */
	CCU_RESET(RST_BUS_SCR0, 0x93c, 16)
	CCU_RESET(RST_BUS_SCR1, 0x93c, 17)

	/* SPI_BGR_REG */
	CCU_RESET(RST_BUS_SPI0, 0x96c, 16)
	CCU_RESET(RST_BUS_SPI1, 0x96c, 17)

	/* EMAC_BGR_REG */
	CCU_RESET(RST_BUS_EMAC, 0x97c, 16)

	/* TS_BGR_REG */
	CCU_RESET(RST_BUS_TS, 0x9bc, 16)

	/* CIRTX_BGR_REG */
	CCU_RESET(RST_BUS_IR_TX, 0x9cc, 16)

	/* THS_BGR_REG */
	CCU_RESET(RST_BUS_THS, 0x9fc, 16)

	/* USB0_CLK_REG */
	CCU_RESET(RST_USB_PHY0, 0xa70, 30)

	/* USB1_CLK_REG */
	CCU_RESET(RST_USB_PHY1, 0xa74, 30)

	/* USB3_CLK_REG */
	CCU_RESET(RST_USB_HSIC, 0xa7c, 28)
	CCU_RESET(RST_USB_PHY3, 0xa7c, 30)

	/* USB_BGR_REG */
	CCU_RESET(RST_BUS_OHCI0, 0xa8c, 16)
	CCU_RESET(RST_BUS_OHCI3, 0xa8c, 19)
	CCU_RESET(RST_BUS_EHCI0, 0xa8c, 20)
	CCU_RESET(RST_BUS_XHCI, 0xa8c, 21)
	CCU_RESET(RST_BUS_EHCI3, 0xa8c, 23)
	CCU_RESET(RST_BUS_OTG, 0xa8c, 24)

	/* HDMI_BGR_REG */
	CCU_RESET(RST_BUS_HDMI, 0xb1c, 16)
	CCU_RESET(RST_BUS_HDMI_SUB, 0xb1c, 17)

	/* DISPLAY_IF_TOP_BGR_REG */
	CCU_RESET(RST_BUS_TCON_TOP, 0xb5c, 16)

	/* TCON_LCD_BGR_REG */
	CCU_RESET(RST_BUS_TCON_LCD0, 0xb7c, 16)

	/* TCON_TV_BGR_REG */
	CCU_RESET(RST_BUS_TCON_TV0, 0xb9c, 16)

	/* HDMI_HDCP_BGR_REG */
	CCU_RESET(RST_BUS_HDCP, 0xc4c, 16)
};

static struct aw_ccung_gate h6_ccu_gates[] = {
	/* DE_BGR_REG */
	CCU_GATE(CLK_BUS_DE, "bus-de", "psi_ahb1_ahb2", 0x60c, 0)

	/* DI_BGR_REG */
	CCU_GATE(CLK_BUS_DEINTERLACE, "bus-deinterlace", "psi_ahb1_ahb2", 0x62c, 0)

	/* GPU_BGR_REG */
	CCU_GATE(CLK_BUS_GPU, "bus-gpu", "psi_ahb1_ahb2", 0x67c, 0)

	/* CE_BGR_REG */
	CCU_GATE(CLK_BUS_CE, "bus-ce", "psi_ahb1_ahb2", 0x68c, 0)

	/* VE_BGR_REG */
	CCU_GATE(CLK_BUS_VE, "bus-ve", "psi_ahb1_ahb2", 0x69c, 0)

	/* EMCE_BGR_REG */
	CCU_GATE(CLK_BUS_EMCE, "bus-emce", "psi_ahb1_ahb2", 0x6bc, 0)

	/* VP9_BGR_REG */
	CCU_GATE(CLK_BUS_VP9, "bus-vp9", "psi_ahb1_ahb2", 0x6cc, 0)

	/* DMA_BGR_REG */
	CCU_GATE(CLK_BUS_DMA, "bus-dma", "psi_ahb1_ahb2", 0x70c, 0)

	/* MSGBOX_BGR_REG */
	CCU_GATE(CLK_BUS_MSGBOX, "bus-msgbox", "psi_ahb1_ahb2", 0x71c, 0)

	/* SPINLOCK_BGR_REG */
	CCU_GATE(CLK_BUS_SPINLOCK, "bus-spinlock", "psi_ahb1_ahb2", 0x72c, 0)

	/* HSTIMER_BGR_REG */
	CCU_GATE(CLK_BUS_HSTIMER, "bus-hstimer", "psi_ahb1_ahb2", 0x73c, 0)

	/* AVS_CLK_REG */
	CCU_GATE(CLK_AVS, "avs", "osc24M", 0x740, 31)

	/* DBGSYS_BGR_REG */
	CCU_GATE(CLK_BUS_DBG, "bus-dbg", "psi_ahb1_ahb2", 0x78c, 0)

	/* PSI_BGR_REG */
	CCU_GATE(CLK_BUS_PSI, "bus-psi", "psi_ahb1_ahb2", 0x79c, 0)

	/* PWM_BGR_REG */
	CCU_GATE(CLK_BUS_PWM, "bus-pwm", "apb1", 0x7ac, 0)

	/* IOMMU_BGR_REG */
	CCU_GATE(CLK_BUS_IOMMU, "bus-iommu", "apb1", 0x7bc, 0)

	/* MBUS_MAT_CLK_GATING_REG */
	CCU_GATE(CLK_MBUS_DMA, "mbus-dma", "mbus", 0x804, 0)
	CCU_GATE(CLK_MBUS_VE, "mbus-ve", "mbus", 0x804, 1)
	CCU_GATE(CLK_MBUS_CE, "mbus-ce", "mbus", 0x804, 2)
	CCU_GATE(CLK_MBUS_TS, "mbus-ts", "mbus", 0x804, 3)
	CCU_GATE(CLK_MBUS_NAND, "mbus-nand", "mbus", 0x804, 5)
	CCU_GATE(CLK_MBUS_CSI, "mbus-csi", "mbus", 0x804, 8)
	CCU_GATE(CLK_MBUS_DEINTERLACE, "mbus-deinterlace", "mbus", 0x804, 11)

	/* DRAM_BGR_REG */
	CCU_GATE(CLK_BUS_DRAM, "bus-dram", "psi_ahb1_ahb2", 0x80c, 0)

	/* NAND_BGR_REG */
	CCU_GATE(CLK_BUS_NAND, "bus-nand", "ahb3", 0x82c, 0)

	/* SMHC_BGR_REG */
	CCU_GATE(CLK_BUS_MMC0, "bus-mmc0", "ahb3", 0x84c, 0)
	CCU_GATE(CLK_BUS_MMC1, "bus-mmc1", "ahb3", 0x84c, 1)
	CCU_GATE(CLK_BUS_MMC2, "bus-mmc2", "ahb3", 0x84c, 2)

	/* UART_BGR_REG Enabling the gate enable weir behavior ... */
	/* CCU_GATE(CLK_BUS_UART0, "bus-uart0", "apb2", 0x90c, 0) */
	/* CCU_GATE(CLK_BUS_UART1, "bus-uart1", "apb2", 0x90c, 1) */
	/* CCU_GATE(CLK_BUS_UART2, "bus-uart2", "apb2", 0x90c, 2) */
	/* CCU_GATE(CLK_BUS_UART3, "bus-uart3", "apb2", 0x90c, 3) */

	/* TWI_BGR_REG */
	CCU_GATE(CLK_BUS_I2C0, "bus-i2c0", "apb2", 0x91c, 0)
	CCU_GATE(CLK_BUS_I2C1, "bus-i2c1", "apb2", 0x91c, 1)
	CCU_GATE(CLK_BUS_I2C2, "bus-i2c2", "apb2", 0x91c, 2)
	CCU_GATE(CLK_BUS_I2C3, "bus-i2c3", "apb2", 0x91c, 3)

	/* SCR_BGR_REG */
	CCU_GATE(CLK_BUS_SCR0, "bus-scr0", "apb2", 0x93c, 0)
	CCU_GATE(CLK_BUS_SCR1, "bus-scr1", "apb2", 0x93c, 1)

	/* SPI_BGR_REG */
	CCU_GATE(CLK_BUS_SPI0, "bus-spi0", "ahb3", 0x96c, 0)
	CCU_GATE(CLK_BUS_SPI1, "bus-spi1", "ahb3", 0x96c, 1)

	/* EMAC_BGR_REG */
	CCU_GATE(CLK_BUS_EMAC, "bus-emac", "ahb3", 0x97c, 0)

	/* TS_BGR_REG */
	CCU_GATE(CLK_BUS_TS, "bus-ts", "ahb3", 0x9bc, 0)

	/* CIRTX_BGR_REG */
	CCU_GATE(CLK_BUS_IR_TX, "bus-ir-tx", "apb1", 0x9cc, 0)

	/* THS_BGR_REG */
	CCU_GATE(CLK_BUS_THS, "bus-ths", "apb1", 0x9fc, 0)

	/* USB0_CLK_REG */
	CCU_GATE(CLK_USB_PHY0, "usb-phy0", "ahb3", 0xa70, 29)
	CCU_GATE(CLK_USB_OHCI0, "usb-ohci0", "ahb3", 0xa70, 31)

	/* USB1_CLK_REG */
	CCU_GATE(CLK_USB_PHY1, "usb-phy1", "ahb3", 0xa74, 29)

	/* USB3_CLK_REG */
	CCU_GATE(CLK_USB_HSIC, "usb-hsic", "ahb3", 0xa7c, 26)
	CCU_GATE(CLK_USB_HSIC_12M, "usb-hsic-12M", "ahb3", 0xa7c, 27)
	CCU_GATE(CLK_USB_PHY3, "usb-phy3", "ahb3", 0xa7c, 29)
	CCU_GATE(CLK_USB_OHCI3, "usb-ohci3", "ahb3", 0xa7c, 31)

	/* USB_BGR_REG */
	CCU_GATE(CLK_BUS_OHCI0, "bus-ohci0", "ahb3", 0xa8c, 0)
	CCU_GATE(CLK_BUS_OHCI3, "bus-ohci3", "ahb3", 0xa8c, 3)
	CCU_GATE(CLK_BUS_EHCI0, "bus-ehci0", "ahb3", 0xa8c, 4)
	CCU_GATE(CLK_BUS_XHCI, "bus-xhci", "ahb3", 0xa8c, 5)
	CCU_GATE(CLK_BUS_EHCI3, "bus-ehci3", "ahb3", 0xa8c, 7)
	CCU_GATE(CLK_BUS_OTG, "bus-otg", "ahb3", 0xa8c, 8)

	/* HDMI_SLOW_CLK_REG */
	CCU_GATE(CLK_HDMI_SLOW, "hdmi-slow", "osc24M", 0xb04, 31)

	/* HDMI_BGR_REG */
	CCU_GATE(CLK_BUS_HDMI, "bus-hdmi", "ahb3", 0xb1c, 0)

	/* DISPLAY_IF_TOP_BGR_REG */
	CCU_GATE(CLK_BUS_TCON_TOP, "bus-tcon-top", "ahb3", 0xb5c, 0)

	/* TCON_LCD_BGR_REG */
	CCU_GATE(CLK_BUS_TCON_LCD0, "bus-tcon-lcd0", "ahb3", 0xb7c, 0)

	/* TCON_TV_BGR_REG */
	CCU_GATE(CLK_BUS_TCON_TV0, "bus-tcon-tv0", "ahb3", 0xb9c, 0)

	/* HDMI_HDCP_BGR_REG */
	CCU_GATE(CLK_BUS_HDCP, "bus-hdcp", "ahb3", 0xc4c, 0)
};

static const char *osc12m_parents[] = {"osc24M"};
FIXED_CLK(osc12m_clk,
    CLK_OSC_12M,			/* id */
    "osc12M",				/* name */
    osc12m_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    2,					/* div */
    0);					/* flags */

/*
 * The P factor, bits 17:16, is /1, /2 or /4 and should only be used for
 * frequencies lower than 288MHz, which the minimum of N does not allow.  It
 * is read but the rate is always searched with it at 1.  The bits 1:0 are
 * the M factor which is only used for testing, it is not used.
 */
static const char *pll_cpux_parents[] = {"osc24M"};
NP_CLK(pll_cpux_clk,
    CLK_PLL_CPUX,				/* id */
    "pll_cpux", pll_cpux_parents,		/* name, parents */
    0x00,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    16, 2, 1, AW_CLK_FACTOR_POWER_OF_TWO |
    AW_CLK_FACTOR_SEARCH_VALUE,			/* p factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_ddr0_parents[] = {"osc24M"};
NMM_CLK(pll_ddr0_clk,
    CLK_PLL_DDR0,				/* id */
    "pll_ddr0", pll_ddr0_parents,		/* name, parents */
    0x10,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_peri0_4x_parents[] = {"osc24M"};
NMM_CLK(pll_peri0_4x_clk,
    CLK_PLL_PERIPH0_4X,				/* id */
    "pll_periph0_4x", pll_peri0_4x_parents,	/* name, parents */
    0x20,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_peri0_2x_parents[] = {"pll_periph0_4x"};
FIXED_CLK(pll_peri0_2x_clk,
    CLK_PLL_PERIPH0_2X,			/* id */
    "pll_periph0_2x",			/* name */
    pll_peri0_2x_parents,		/* parent */
    0,					/* freq */
    1,					/* mult */
    2,					/* div */
    0);					/* flags */
static const char *pll_peri0_parents[] = {"pll_periph0_4x"};
FIXED_CLK(pll_peri0_clk,
    CLK_PLL_PERIPH0,			/* id */
    "pll_periph0",			/* name */
    pll_peri0_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

static const char *pll_peri1_4x_parents[] = {"osc24M"};
NMM_CLK(pll_peri1_4x_clk,
    CLK_PLL_PERIPH1_4X,				/* id */
    "pll_periph1_4x", pll_peri1_4x_parents,	/* name, parents */
    0x28,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_peri1_2x_parents[] = {"pll_periph1_4x"};
FIXED_CLK(pll_peri1_2x_clk,
    CLK_PLL_PERIPH1_2X,			/* id */
    "pll_periph1_2x",			/* name */
    pll_peri1_2x_parents,		/* parent */
    0,					/* freq */
    1,					/* mult */
    2,					/* div */
    0);					/* flags */
static const char *pll_peri1_parents[] = {"pll_periph1_4x"};
FIXED_CLK(pll_peri1_clk,
    CLK_PLL_PERIPH1,			/* id */
    "pll_periph1",			/* name */
    pll_peri1_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

/*
 * Changing the output divider (bit 0) of the GPU PLL to scale its frequency
 * makes the system fail.  It is set at reset so it is read but the rate is
 * always searched with it at 1, like Linux which also clears it.
 */
static const char *pll_gpu_parents[] = {"osc24M"};
NMM_CLK(pll_gpu_clk,
    CLK_PLL_GPU,				/* id */
    "pll_gpu", pll_gpu_parents,			/* name, parents */
    0x30,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 1, 1, AW_CLK_FACTOR_SEARCH_VALUE,	/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

/*
 * The output divider of the video PLLs, bit 0, is only used for testing and
 * is not a part of their rate, which is 1188MHz at reset when it is set.  It
 * is not used.
 */
static const char *pll_video0_4x_parents[] = {"osc24M"};
NMM_CLK(pll_video0_4x_clk,
    CLK_PLL_VIDEO0_4X,				/* id */
    "pll_video0_4x", pll_video0_4x_parents,	/* name, parents */
    0x40,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 0, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_video0_parents[] = {"pll_video0_4x"};
FIXED_CLK(pll_video0_clk,
    CLK_PLL_VIDEO0,			/* id */
    "pll_video0",			/* name */
    pll_video0_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

static const char *pll_video1_4x_parents[] = {"osc24M"};
NMM_CLK(pll_video1_4x_clk,
    CLK_PLL_VIDEO1_4X,				/* id */
    "pll_video1_4x", pll_video1_4x_parents,	/* name, parents */
    0x48,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 0, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_video1_parents[] = {"pll_video1_4x"};
FIXED_CLK(pll_video1_clk,
    CLK_PLL_VIDEO1,			/* id */
    "pll_video1",			/* name */
    pll_video1_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

static const char *pll_ve_parents[] = {"osc24M"};
NMM_CLK(pll_ve_clk,
    CLK_PLL_VE,					/* id */
    "pll_ve", pll_ve_parents,			/* name, parents */
    0x58,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_de_parents[] = {"osc24M"};
NMM_CLK(pll_de_clk,
    CLK_PLL_DE,					/* id */
    "pll_de", pll_de_parents,			/* name, parents */
    0x60,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_hsic_parents[] = {"osc24M"};
NMM_CLK(pll_hsic_clk,
    CLK_PLL_HSIC,				/* id */
    "pll_hsic", pll_hsic_parents,		/* name, parents */
    0x70,					/* offset */
    8, 8, 0, AW_CLK_FACTOR_MIN_VALUE, 12,	/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

/* PLL_AUDIO missing */

static const char *cpux_parents[] = {"osc24M", "osc32k", "iosc", "pll_cpux"};
MUX_CLK(cpux_clk,
    CLK_CPUX,			/* id */
    "cpux", cpux_parents,	/* name, parents */
    0x500, 24, 2);		/* offset, shift, width */

static const char *axi_parents[] = {"cpux"};
DIV_CLK(axi_clk,
    CLK_AXI,			/* id */
    "axi", axi_parents,		/* name, parents */
    0x500,			/* offset */
    0, 2,			/* shift, width */
    0, NULL);			/* flags, div table */

static const char *cpux_apb_parents[] = {"cpux"};
DIV_CLK(cpux_apb_clk,
    CLK_CPUX_APB,		/* id */
    "cpux_apb", cpux_apb_parents,	/* name, parents */
    0x500,			/* offset */
    8, 2,			/* shift, width */
    0, NULL);			/* flags, div table */

static const char *psi_ahb1_ahb2_parents[] = {"osc24M", "osc32k", "iosc", "pll_periph0"};
NM_CLK(psi_ahb1_ahb2_clk,
    CLK_PSI_AHB1_AHB2, "psi_ahb1_ahb2", psi_ahb1_ahb2_parents,		/* id, name, parents */
    0x510,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

static const char *ahb3_parents[] = {"osc24M", "osc32k", "psi_ahb1_ahb2", "pll_periph0"};
NM_CLK(ahb3_clk,
    CLK_AHB3, "ahb3", ahb3_parents,		/* id, name, parents */
    0x51C,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

static const char *apb1_parents[] = {"osc24M", "osc32k", "psi_ahb1_ahb2", "pll_periph0"};
NM_CLK(apb1_clk,
    CLK_APB1, "apb1", apb1_parents,		/* id, name, parents */
    0x520,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

static const char *apb2_parents[] = {"osc24M", "osc32k", "psi_ahb1_ahb2", "pll_periph0"};
NM_CLK(apb2_clk,
    CLK_APB2, "apb2", apb2_parents,		/* id, name, parents */
    0x524,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

/* Missing MBUS clock */

static const char *mod_parents[] = {"osc24M", "pll_periph0_2x", "pll_periph1_2x"};
NM_CLK(mmc0_clk,
    CLK_MMC0, "mmc0", mod_parents,		/* id, name, parents */
    0x830,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

NM_CLK(mmc1_clk,
    CLK_MMC1, "mmc1", mod_parents,		/* id, name, parents */
    0x834,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

NM_CLK(mmc2_clk,
    CLK_MMC2, "mmc2", mod_parents,		/* id, name, parents */
    0x838,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

static const char *mbus_parents[] = {"osc24M", "pll_periph0_2x", "pll_ddr0",
    "pll_periph0_4x"};
M_CLK(mbus_clk,
    CLK_MBUS, "mbus", mbus_parents,		/* id, name, parents */
    0x540,					/* offset */
    0, 3, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_HAS_GATE);		/* flags */

/* The DRAM is set up by the firmware, its rate should not be changed */
static const char *dram_parents[] = {"pll_ddr0"};
M_CLK(dram_clk,
    CLK_DRAM, "dram", dram_parents,		/* id, name, parents */
    0x800,					/* offset */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX);				/* flags */

static const char *nand_spi_parents[] = {"osc24M", "pll_periph0", "pll_periph1",
    "pll_periph0_2x", "pll_periph1_2x"};
NM_CLK(nand0_clk,
    CLK_NAND0, "nand0", nand_spi_parents,		/* id, name, parents */
    0x810,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 3,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

NM_CLK(nand1_clk,
    CLK_NAND1, "nand1", nand_spi_parents,		/* id, name, parents */
    0x814,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 3,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

NM_CLK(spi0_clk,
    CLK_SPI0, "spi0", nand_spi_parents,		/* id, name, parents */
    0x940,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 3,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

NM_CLK(spi1_clk,
    CLK_SPI1, "spi1", nand_spi_parents,		/* id, name, parents */
    0x944,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 3,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

static const char *ts_parents[] = {"osc24M", "pll_periph0"};
NM_CLK(ts_clk,
    CLK_TS, "ts", ts_parents,		/* id, name, parents */
    0x9b0,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

static const char *ir_tx_parents[] = {"osc32k", "osc24M"};
NM_CLK(ir_tx_clk,
    CLK_IR_TX, "ir-tx", ir_tx_parents,		/* id, name, parents */
    0x9c0,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

static const char *de_parents[] = {"pll_de", "pll_periph0_2x"};
M_CLK(de_clk,
    CLK_DE, "de", de_parents,		/* id, name, parents */
    0x600,					/* offset */
    0, 4, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *deinterlace_parents[] = {"pll_periph0", "pll_periph1"};
M_CLK(deinterlace_clk,
    CLK_DEINTERLACE, "deinterlace", deinterlace_parents,		/* id, name, parents */
    0x620,					/* offset */
    0, 4, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *gpu_parents[] = {"pll_gpu"};
M_CLK(gpu_clk,
    CLK_GPU, "gpu", gpu_parents,		/* id, name, parents */
    0x670,					/* offset */
    0, 3, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *ce_parents[] = {"osc24M", "pll_periph0_2x"};
NM_CLK(ce_clk,
    CLK_CE, "ce", ce_parents,		/* id, name, parents */
    0x680,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *ve_parents[] = {"pll_ve"};
M_CLK(ve_clk,
    CLK_VE, "ve", ve_parents,		/* id, name, parents */
    0x690,					/* offset */
    0, 3, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

NM_CLK(emce_clk,
    CLK_EMCE, "emce", ce_parents,		/* id, name, parents */
    0x6b0,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *vp9_parents[] = {"pll_ve", "pll_periph0_2x"};
M_CLK(vp9_clk,
    CLK_VP9, "vp9", vp9_parents,		/* id, name, parents */
    0x6c0,					/* offset */
    0, 3, 0, 0,					/* m factor */
    24, 1,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *hdmi_parents[] = {"pll_video0", "pll_video1",
    "pll_video1_4x"};
M_CLK(hdmi_clk,
    CLK_HDMI, "hdmi", hdmi_parents,		/* id, name, parents */
    0xb00,					/* offset */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

/* The TCON LCD clock has no divider */
static const char *tcon_lcd0_parents[] = {"pll_video0", "pll_video0_4x",
    "pll_video1"};
M_CLK(tcon_lcd0_clk,
    CLK_TCON_LCD0, "tcon-lcd0", tcon_lcd0_parents, /* id, name, parents */
    0xb60,					/* offset */
    0, 0, 0, 0,					/* m factor */
    24, 3,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *tcon_tv0_parents[] = {"pll_video0", "pll_video0_4x",
    "pll_video1", "pll_video1_4x"};
NM_CLK(tcon_tv0_clk,
    CLK_TCON_TV0, "tcon-tv0", tcon_tv0_parents,	/* id, name, parents */
    0xb80,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 3,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static const char *hdcp_parents[] = {"pll_periph0", "pll_periph1"};
M_CLK(hdcp_clk,
    CLK_HDCP, "hdcp", hdcp_parents,		/* id, name, parents */
    0xc40,					/* offset */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX);		/* flags */

static struct aw_ccung_clk h6_ccu_clks[] = {
	{ .type = AW_CLK_NP, .clk.np = &pll_cpux_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_ddr0_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_peri0_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_peri1_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_gpu_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_video0_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_video1_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_ve_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_de_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_hsic_clk},

	{ .type = AW_CLK_MUX, .clk.mux = &cpux_clk},
	{ .type = AW_CLK_DIV, .clk.div = &axi_clk},
	{ .type = AW_CLK_DIV, .clk.div = &cpux_apb_clk},

	{ .type = AW_CLK_NM, .clk.nm = &psi_ahb1_ahb2_clk},
	{ .type = AW_CLK_NM, .clk.nm = &ahb3_clk},
	{ .type = AW_CLK_NM, .clk.nm = &apb1_clk},
	{ .type = AW_CLK_NM, .clk.nm = &apb2_clk},
	{ .type = AW_CLK_M, .clk.m = &mbus_clk},
	{ .type = AW_CLK_M, .clk.m = &dram_clk},

	{ .type = AW_CLK_NM, .clk.nm = &nand0_clk},
	{ .type = AW_CLK_NM, .clk.nm = &nand1_clk},
	{ .type = AW_CLK_NM, .clk.nm = &spi0_clk},
	{ .type = AW_CLK_NM, .clk.nm = &spi1_clk},
	{ .type = AW_CLK_NM, .clk.nm = &ts_clk},
	{ .type = AW_CLK_NM, .clk.nm = &ir_tx_clk},

	{ .type = AW_CLK_M, .clk.m = &de_clk},
	{ .type = AW_CLK_M, .clk.m = &deinterlace_clk},
	{ .type = AW_CLK_M, .clk.m = &gpu_clk},
	{ .type = AW_CLK_NM, .clk.nm = &ce_clk},
	{ .type = AW_CLK_M, .clk.m = &ve_clk},
	{ .type = AW_CLK_NM, .clk.nm = &emce_clk},
	{ .type = AW_CLK_M, .clk.m = &vp9_clk},

	{ .type = AW_CLK_M, .clk.m = &hdmi_clk},
	{ .type = AW_CLK_M, .clk.m = &tcon_lcd0_clk},
	{ .type = AW_CLK_NM, .clk.nm = &tcon_tv0_clk},
	{ .type = AW_CLK_M, .clk.m = &hdcp_clk},

	{ .type = AW_CLK_NM, .clk.nm = &mmc0_clk},
	{ .type = AW_CLK_NM, .clk.nm = &mmc1_clk},
	{ .type = AW_CLK_NM, .clk.nm = &mmc2_clk},

	{ .type = AW_CLK_FIXED, .clk.fixed = &osc12m_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri0_2x_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri0_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri1_2x_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri1_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_video0_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_video1_clk},
};

/*
 * Registers of the PLLs, the lock status bit of a PLL is only updated when
 * its lock enable bit is set.
 */
#define	PLL_LOCK_ENABLE		(1 << 29)

static const bus_size_t h6_pll_regs[] = {
	0x00,	/* PLL_CPUX */
	0x10,	/* PLL_DDR0 */
	0x20,	/* PLL_PERIPH0 */
	0x28,	/* PLL_PERIPH1 */
	0x30,	/* PLL_GPU */
	0x40,	/* PLL_VIDEO0 */
	0x48,	/* PLL_VIDEO1 */
	0x58,	/* PLL_VE */
	0x60,	/* PLL_DE */
	0x70,	/* PLL_HSIC */
};

static int
ccu_h6_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "allwinner,sun50i-h6-ccu"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner H6 Clock Control Unit NG");
	return (BUS_PROBE_DEFAULT);
}

static int
ccu_h6_attach(device_t dev)
{
	struct aw_ccung_softc *sc;
	uint32_t val;
	int error, i;

	sc = device_get_softc(dev);

	sc->resets = h6_ccu_resets;
	sc->nresets = nitems(h6_ccu_resets);
	sc->gates = h6_ccu_gates;
	sc->ngates = nitems(h6_ccu_gates);
	sc->clks = h6_ccu_clks;
	sc->nclks = nitems(h6_ccu_clks);

	error = aw_ccung_attach(dev);
	if (error != 0)
		return (error);

	/*
	 * Only the PLLs used by the firmware have the lock enable bit set,
	 * set it for all of them so that we can wait for them to lock.
	 */
	for (i = 0; i < nitems(h6_pll_regs); i++) {
		val = bus_read_4(sc->res, h6_pll_regs[i]);
		bus_write_4(sc->res, h6_pll_regs[i], val | PLL_LOCK_ENABLE);
	}

	return (0);
}

static device_method_t ccu_h6ng_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ccu_h6_probe),
	DEVMETHOD(device_attach,	ccu_h6_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ccu_h6ng, ccu_h6ng_driver, ccu_h6ng_methods,
  sizeof(struct aw_ccung_softc), aw_ccung_driver);

EARLY_DRIVER_MODULE(ccu_h6ng, simplebus, ccu_h6ng_driver, 0, 0,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
