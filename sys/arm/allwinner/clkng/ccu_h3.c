/*-
 * Copyright (c) 2017 Emmanuel Vadot <manu@freebsd.org>
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm/allwinner/clkng/aw_ccung.h>
#include <arm/allwinner/clkng/aw_clk.h>
#include <arm/allwinner/clkng/aw_clk_nm.h>
#include <arm/allwinner/clkng/aw_clk_nkmp.h>
#include <arm/allwinner/clkng/aw_clk_prediv_mux.h>

#include "ccu_h3.h"

static struct aw_ccung_reset h3_ccu_resets[] = {
	CCU_RESET(H3_RST_USB_PHY0, 0xcc, 0)
	CCU_RESET(H3_RST_USB_PHY1, 0xcc, 1)
	CCU_RESET(H3_RST_USB_PHY2, 0xcc, 2)
	CCU_RESET(H3_RST_USB_PHY3, 0xcc, 3)

	CCU_RESET(H3_RST_MBUS, 0xfc, 31)

	CCU_RESET(H3_RST_BUS_CE, 0x2c0, 5)
	CCU_RESET(H3_RST_BUS_DMA, 0x2c0, 6)
	CCU_RESET(H3_RST_BUS_MMC0, 0x2c0, 8)
	CCU_RESET(H3_RST_BUS_MMC1, 0x2c0, 9)
	CCU_RESET(H3_RST_BUS_MMC2, 0x2c0, 10)
	CCU_RESET(H3_RST_BUS_NAND, 0x2c0, 13)
	CCU_RESET(H3_RST_BUS_DRAM, 0x2c0, 14)
	CCU_RESET(H3_RST_BUS_EMAC, 0x2c0, 17)
	CCU_RESET(H3_RST_BUS_TS, 0x2c0, 18)
	CCU_RESET(H3_RST_BUS_HSTIMER, 0x2c0, 19)
	CCU_RESET(H3_RST_BUS_SPI0, 0x2c0, 20)
	CCU_RESET(H3_RST_BUS_SPI1, 0x2c0, 21)
	CCU_RESET(H3_RST_BUS_OTG, 0x2c0, 23)
	CCU_RESET(H3_RST_BUS_EHCI0, 0x2c0, 24)
	CCU_RESET(H3_RST_BUS_EHCI1, 0x2c0, 25)
	CCU_RESET(H3_RST_BUS_EHCI2, 0x2c0, 26)
	CCU_RESET(H3_RST_BUS_EHCI3, 0x2c0, 27)
	CCU_RESET(H3_RST_BUS_OHCI0, 0x2c0, 28)
	CCU_RESET(H3_RST_BUS_OHCI1, 0x2c0, 29)
	CCU_RESET(H3_RST_BUS_OHCI2, 0x2c0, 30)
	CCU_RESET(H3_RST_BUS_OHCI3, 0x2c0, 31)

	CCU_RESET(H3_RST_BUS_VE, 0x2c4, 0)
	CCU_RESET(H3_RST_BUS_TCON0, 0x2c4, 3)
	CCU_RESET(H3_RST_BUS_TCON1, 0x2c4, 4)
	CCU_RESET(H3_RST_BUS_DEINTERLACE, 0x2c4, 5)
	CCU_RESET(H3_RST_BUS_CSI, 0x2c4, 8)
	CCU_RESET(H3_RST_BUS_TVE, 0x2c4, 9)
	CCU_RESET(H3_RST_BUS_HDMI0, 0x2c4, 10)
	CCU_RESET(H3_RST_BUS_HDMI1, 0x2c4, 11)
	CCU_RESET(H3_RST_BUS_DE, 0x2c4, 12)
	CCU_RESET(H3_RST_BUS_GPU, 0x2c4, 20)
	CCU_RESET(H3_RST_BUS_MSGBOX, 0x2c4, 21)
	CCU_RESET(H3_RST_BUS_SPINLOCK, 0x2c4, 22)
	CCU_RESET(H3_RST_BUS_DBG, 0x2c4, 31)

	CCU_RESET(H3_RST_BUS_EPHY, 0x2c8, 2)

	CCU_RESET(H3_RST_BUS_CODEC, 0x2d0, 0)
	CCU_RESET(H3_RST_BUS_SPDIF, 0x2d0, 1)
	CCU_RESET(H3_RST_BUS_THS, 0x2d0, 8)
	CCU_RESET(H3_RST_BUS_I2S0, 0x2d0, 12)
	CCU_RESET(H3_RST_BUS_I2S1, 0x2d0, 13)
	CCU_RESET(H3_RST_BUS_I2S2, 0x2d0, 14)

	CCU_RESET(H3_RST_BUS_I2C0, 0x2d8, 0)
	CCU_RESET(H3_RST_BUS_I2C1, 0x2d8, 1)
	CCU_RESET(H3_RST_BUS_I2C2, 0x2d8, 2)
	CCU_RESET(H3_RST_BUS_UART0, 0x2d8, 16)
	CCU_RESET(H3_RST_BUS_UART1, 0x2d8, 17)
	CCU_RESET(H3_RST_BUS_UART2, 0x2d8, 18)
	CCU_RESET(H3_RST_BUS_UART3, 0x2d8, 19)
	CCU_RESET(H3_RST_BUS_SCR, 0x2d8, 20)
};

static struct aw_ccung_gate h3_ccu_gates[] = {
	CCU_GATE(H3_CLK_BUS_CE, "bus-ce", "ahb1", 0x60, 5)
	CCU_GATE(H3_CLK_BUS_DMA, "bus-dma", "ahb1", 0x60, 6)
	CCU_GATE(H3_CLK_BUS_MMC0, "bus-mmc0", "ahb1", 0x60, 8)
	CCU_GATE(H3_CLK_BUS_MMC1, "bus-mmc1", "ahb1", 0x60, 9)
	CCU_GATE(H3_CLK_BUS_MMC2, "bus-mmc2", "ahb1", 0x60, 10)
	CCU_GATE(H3_CLK_BUS_NAND, "bus-nand", "ahb1", 0x60, 13)
	CCU_GATE(H3_CLK_BUS_DRAM, "bus-dram", "ahb1", 0x60, 14)
	CCU_GATE(H3_CLK_BUS_EMAC, "bus-emac", "ahb2", 0x60, 17)
	CCU_GATE(H3_CLK_BUS_TS, "bus-ts", "ahb1", 0x60, 18)
	CCU_GATE(H3_CLK_BUS_HSTIMER, "bus-hstimer", "ahb1", 0x60, 19)
	CCU_GATE(H3_CLK_BUS_SPI0, "bus-spi0", "ahb1", 0x60, 20)
	CCU_GATE(H3_CLK_BUS_SPI1, "bus-spi1", "ahb1", 0x60, 21)
	CCU_GATE(H3_CLK_BUS_OTG, "bus-otg", "ahb1", 0x60, 23)
	CCU_GATE(H3_CLK_BUS_EHCI0, "bus-ehci0", "ahb1", 0x60, 24)
	CCU_GATE(H3_CLK_BUS_EHCI1, "bus-ehci1", "ahb2", 0x60, 25)
	CCU_GATE(H3_CLK_BUS_EHCI2, "bus-ehci2", "ahb2", 0x60, 26)
	CCU_GATE(H3_CLK_BUS_EHCI3, "bus-ehci3", "ahb2", 0x60, 27)
	CCU_GATE(H3_CLK_BUS_OHCI0, "bus-ohci0", "ahb1", 0x60, 28)
	CCU_GATE(H3_CLK_BUS_OHCI1, "bus-ohci1", "ahb2", 0x60, 29)
	CCU_GATE(H3_CLK_BUS_OHCI2, "bus-ohci2", "ahb2", 0x60, 30)
	CCU_GATE(H3_CLK_BUS_OHCI3, "bus-ohci3", "ahb2", 0x60, 31)

	CCU_GATE(H3_CLK_BUS_VE, "bus-ve", "ahb1", 0x64, 0)
	CCU_GATE(H3_CLK_BUS_TCON0, "bus-tcon0", "ahb1", 0x64, 3)
	CCU_GATE(H3_CLK_BUS_TCON1, "bus-tcon1", "ahb1", 0x64, 4)
	CCU_GATE(H3_CLK_BUS_DEINTERLACE, "bus-deinterlace", "ahb1", 0x64, 5)
	CCU_GATE(H3_CLK_BUS_CSI, "bus-csi", "ahb1", 0x64, 8)
	CCU_GATE(H3_CLK_BUS_TVE, "bus-tve", "ahb1", 0x64, 9)
	CCU_GATE(H3_CLK_BUS_HDMI, "bus-hdmi", "ahb1", 0x64, 11)
	CCU_GATE(H3_CLK_BUS_DE, "bus-de", "ahb1", 0x64, 12)
	CCU_GATE(H3_CLK_BUS_GPU, "bus-gpu", "ahb1", 0x64, 20)
	CCU_GATE(H3_CLK_BUS_MSGBOX, "bus-msgbox", "ahb1", 0x64, 21)
	CCU_GATE(H3_CLK_BUS_SPINLOCK, "bus-spinlock", "ahb1", 0x64, 22)

	CCU_GATE(H3_CLK_BUS_CODEC, "bus-codec", "apb1", 0x68, 0)
	CCU_GATE(H3_CLK_BUS_SPDIF, "bus-spdif", "apb1", 0x68, 1)
	CCU_GATE(H3_CLK_BUS_PIO, "bus-pio", "apb1", 0x68, 5)
	CCU_GATE(H3_CLK_BUS_THS, "bus-ths", "apb1", 0x68, 8)
	CCU_GATE(H3_CLK_BUS_I2S0, "bus-i2c0", "apb1", 0x68, 12)
	CCU_GATE(H3_CLK_BUS_I2S1, "bus-i2c1", "apb1", 0x68, 13)
	CCU_GATE(H3_CLK_BUS_I2S2, "bus-i2c2", "apb1", 0x68, 14)

	CCU_GATE(H3_CLK_BUS_I2C0, "bus-i2c0", "apb2", 0x6c, 0)
	CCU_GATE(H3_CLK_BUS_I2C1, "bus-i2c1", "apb2", 0x6c, 1)
	CCU_GATE(H3_CLK_BUS_I2C2, "bus-i2c2", "apb2", 0x6c, 2)
	CCU_GATE(H3_CLK_BUS_UART0, "bus-uart0", "apb2", 0x6c, 16)
	CCU_GATE(H3_CLK_BUS_UART1, "bus-uart1", "apb2", 0x6c, 17)
	CCU_GATE(H3_CLK_BUS_UART2, "bus-uart2", "apb2", 0x6c, 18)
	CCU_GATE(H3_CLK_BUS_UART3, "bus-uart3", "apb2", 0x6c, 19)
	CCU_GATE(H3_CLK_BUS_SCR, "bus-scr", "apb2", 0x6c, 20)

	CCU_GATE(H3_CLK_BUS_EPHY, "bus-ephy", "ahb1", 0x70, 0)
	CCU_GATE(H3_CLK_BUS_DBG, "bus-dbg", "ahb1", 0x70, 7)

	CCU_GATE(H3_CLK_USBPHY0, "usb-phy0", "osc24M", 0xcc, 8)
	CCU_GATE(H3_CLK_USBPHY1, "usb-phy1", "osc24M", 0xcc, 9)
	CCU_GATE(H3_CLK_USBPHY2, "usb-phy2", "osc24M", 0xcc, 10)
	CCU_GATE(H3_CLK_USBPHY3, "usb-phy3", "osc24M", 0xcc, 11)
	CCU_GATE(H3_CLK_USBOHCI0, "usb-ohci0", "osc24M", 0xcc, 16)
	CCU_GATE(H3_CLK_USBOHCI1, "usb-ohci1", "osc24M", 0xcc, 17)
	CCU_GATE(H3_CLK_USBOHCI2, "usb-ohci2", "osc24M", 0xcc, 18)
	CCU_GATE(H3_CLK_USBOHCI3, "usb-ohci3", "osc24M", 0xcc, 19)

	CCU_GATE(H3_CLK_THS, "ths", "thsdiv", 0x74, 31)
	CCU_GATE(H3_CLK_I2S0, "i2s0", "i2s0mux", 0xB0, 31)
	CCU_GATE(H3_CLK_I2S1, "i2s1", "i2s1mux", 0xB4, 31)
	CCU_GATE(H3_CLK_I2S2, "i2s2", "i2s2mux", 0xB8, 31)
};

static const char *pll_cpux_parents[] = {"osc24M"};
static const char *pll_audio_parents[] = {"osc24M"};
static const char *pll_audio_mult_parents[] = {"pll_audio"};
/*
 * Need fractional mode on nkmp or a NM fract
static const char *pll_video_parents[] = {"osc24M"};
 */
/*
 * Need fractional mode on nkmp or a NM fract
static const char *pll_ve_parents[] = {"osc24M"};
 */
/*
 * Needs a update bit on nkmp or special clk
static const char *pll_ddr_parents[] = {"osc24M"};
 */
static const char *pll_periph0_parents[] = {"osc24M"};
static const char *pll_periph0_2x_parents[] = {"pll_periph0"};
/*
 * Need fractional mode on nkmp or a NM fract
static const char *pll_gpu_parents[] = {"osc24M"};
 */
static const char *pll_periph1_parents[] = {"osc24M"};
/*
 * Need fractional mode on nkmp or a NM fract
static const char *pll_de_parents[] = {"osc24M"};
 */

static struct aw_clk_nkmp_def nkmp_clks[] = {
	NKMP_CLK(H3_CLK_PLL_CPUX,			/* id */
	    "pll_cpux", pll_cpux_parents,		/* name, parents */
	    0x00,					/* offset */
	    8, 5, 0, 0,					/* n factor */
	    4, 2, 0, 0,					/* k factor */
	    0, 2, 0, 0,					/* m factor */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* p factor */
	    31,						/* gate */
	    28, 1000,					/* lock */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK | AW_CLK_SCALE_CHANGE)		/* flags */
	NKMP_CLK(H3_CLK_PLL_AUDIO,			/* id */
	    "pll_audio", pll_audio_parents,		/* name, parents */
	    0x08,					/* offset */
	    8, 7, 0, 0,					/* n factor */
	    0, 0, 1, AW_CLK_FACTOR_FIXED,		/* k factor (fake) */
	    0, 5, 0, 0,					/* m factor */
	    16, 4, 0, 0,				/* p factor */
	    31,						/* gate */
	    28, 1000,					/* lock */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK)		/* flags */
	NKMP_CLK(H3_CLK_PLL_PERIPH0,			/* id */
	    "pll_periph0", pll_periph0_parents,		/* name, parents */
	    0x28,					/* offset */
	    8, 5, 0, 0,					/* n factor */
	    4, 2, 0, 0,					/* k factor */
	    0, 0, 2, AW_CLK_FACTOR_FIXED,		/* m factor (fake) */
	    0, 0, 1, AW_CLK_FACTOR_FIXED,		/* p factor (fake) */
	    31,						/* gate */
	    28, 1000,					/* lock */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK)		/* flags */
	NKMP_CLK(H3_CLK_PLL_PERIPH1,			/* id */
	    "pll_periph1", pll_periph1_parents,		/* name, parents */
	    0x44,					/* offset */
	    8, 5, 0, 0,					/* n factor */
	    4, 2, 0, 0,					/* k factor */
	    0, 0, 2, AW_CLK_FACTOR_FIXED,		/* m factor (fake) */
	    0, 0, 1, AW_CLK_FACTOR_FIXED,		/* p factor (fake) */
	    31,						/* gate */
	    28, 1000,					/* lock */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK)		/* flags */
};

static const char *ahb1_parents[] = {"osc32k", "osc24M", "axi", "pll_periph0"};
static const char *ahb2_parents[] = {"ahb1", "pll_periph0"};

static struct aw_clk_prediv_mux_def prediv_mux_clks[] = {
	PREDIV_CLK(H3_CLK_AHB1,						/* id */
	    "ahb1", ahb1_parents,					/* name, parents */
	    0x54,							/* offset */
	    12, 2,							/* mux */
	    4, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,			/* div */
	    6, 2, 0, AW_CLK_FACTOR_HAS_COND,				/* prediv */
	    12, 2, 3)							/* prediv condition */
	PREDIV_CLK(H3_CLK_AHB2,						/* id */
	    "ahb2", ahb2_parents,					/* name, parents */
	    0x5c,							/* offset */
	    0, 2,							/* mux */
	    0, 0, 1, AW_CLK_FACTOR_FIXED,				/* div */
	    0, 0, 2, AW_CLK_FACTOR_HAS_COND | AW_CLK_FACTOR_FIXED,	/* prediv */
	    0, 2, 1)							/* prediv condition */
};

static const char *apb2_parents[] = {"osc32k", "osc24M", "pll_periph0", "pll_periph0"};
static const char *mod_parents[] = {"osc24M", "pll_periph0", "pll_periph1"};
static const char *ts_parents[] = {"osc24M", "pll_periph0"};
static const char *spdif_parents[] = {"pll_audio"};
static const char *i2s_parents[] = {"pll_audio-8x", "pll_audio-4x", "pll_audio-2x", "pll_audio"};

static struct aw_clk_nm_def nm_clks[] = {
	NM_CLK(H3_CLK_APB2,				/* id */
	    "apb2", apb2_parents,			/* name, parents */
	    0x58,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 5, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    0,						/* gate */
	    AW_CLK_HAS_MUX)
	NM_CLK(H3_CLK_NAND, "nand", mod_parents,	/* id, name, parents */
	    0x80,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX)		/* flags */
	NM_CLK(H3_CLK_MMC0, "mmc0", mod_parents,	/* id, name, parents */
	    0x88,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
	    AW_CLK_REPARENT)				/* flags */
	NM_CLK(H3_CLK_MMC1, "mmc1", mod_parents,	/* id, name, parents */
	    0x8c,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
	    AW_CLK_REPARENT)				/* flags */
	NM_CLK(H3_CLK_MMC2, "mmc2", mod_parents,	/* id, name, parents */
	    0x90,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
	    AW_CLK_REPARENT)				/* flags */
	NM_CLK(H3_CLK_TS, "ts", ts_parents,		/* id, name, parents */
	    0x98,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX)		/* flags */
	NM_CLK(H3_CLK_CE, "ce", mod_parents,		/* id, name, parents */
	    0x9C,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX)		/* flags */
	NM_CLK(H3_CLK_SPI0, "spi0", mod_parents,	/* id, name, parents */
	    0xA0,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
	    AW_CLK_REPARENT)				/* flags */
	NM_CLK(H3_CLK_SPI1, "spi1", mod_parents,	/* id, name, parents */
	    0xA4,					/* offset */
	    16, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
	    0, 4, 0, 0,					/* m factor */
	    24, 2,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
	    AW_CLK_REPARENT)				/* flags */
	NM_CLK(H3_CLK_SPDIF, "spdif", spdif_parents,	/* id, name, parents */
	    0xC0,					/* offset */
	    0, 0, 1, AW_CLK_FACTOR_FIXED,		/* n factor (fake) */
	    0, 4, 0, 0,					/* m factor */
	    0, 0,					/* mux */
	    31,						/* gate */
	    AW_CLK_HAS_GATE)				/* flags */

};

static const char *cpux_parents[] = {"osc32k", "osc24M", "pll_cpux", "pll_cpux"};

static struct clk_mux_def mux_clks[] = {
	MUX_CLK(H3_CLK_CPUX,		/* id */
	    "cpux", cpux_parents,	/* name, parents */
	    0x50, 16, 2)		/* offset, shift, width */
	MUX_CLK(0,
	    "i2s0mux", i2s_parents,
	    0xb0, 16, 2)
	MUX_CLK(0,
	    "i2s1mux", i2s_parents,
	    0xb4, 16, 2)
	MUX_CLK(0,
	    "i2s2mux", i2s_parents,
	    0xb8, 16, 2)
};

static struct clk_div_table apb1_div_table[] = {
	{ .value = 0, .divider = 2, },
	{ .value = 1, .divider = 2, },
	{ .value = 2, .divider = 4, },
	{ .value = 3, .divider = 8, },
	{ },
};

static struct clk_div_table ths_div_table[] = {
	{ .value = 0, .divider = 1, },
	{ .value = 1, .divider = 2, },
	{ .value = 2, .divider = 4, },
	{ .value = 3, .divider = 6, },
	{ },
};

static const char *ths_parents[] = {"osc24M"};
static const char *axi_parents[] = {"cpux"};
static const char *apb1_parents[] = {"ahb1"};

static struct clk_div_def div_clks[] = {
	DIV_CLK(H3_CLK_AXI,		/* id */
	    "axi", axi_parents,		/* name, parents */
	    0x50,			/* offset */
	    0, 2,			/* shift, width */
	    0, NULL)			/* flags, div table */
	DIV_CLK(H3_CLK_APB1,		/* id */
	    "apb1", apb1_parents,	/* name, parents */
	    0x54,			/* offset */
	    8, 2,			/* shift, width */
	    CLK_DIV_WITH_TABLE,		/* flags */
	    apb1_div_table)		/* div table */
	DIV_CLK(0,			/* id */
	    "thsdiv", ths_parents,	/* name, parents */
	    0x74,			/* offset */
	    0, 2,			/* shift, width */
	    CLK_DIV_WITH_TABLE,		/* flags */
	    ths_div_table)		/* div table */
};

static struct clk_fixed_def fixed_factor_clks[] = {
	FIXED_CLK(H3_CLK_PLL_PERIPH0_2X,	/* id */
	    "pll_periph0-2x",			/* name */
	    pll_periph0_2x_parents,		/* parent */
	    0,					/* freq */
	    2,					/* mult */
	    1,					/* div */
	    0)					/* flags */
	FIXED_CLK(H3_CLK_PLL_AUDIO_2X,		/* id */
	    "pll_audio-2x",			/* name */
	    pll_audio_mult_parents,		/* parent */
	    0,					/* freq */
	    2,					/* mult */
	    1,					/* div */
	    0)					/* flags */
	FIXED_CLK(H3_CLK_PLL_AUDIO_4X,		/* id */
	    "pll_audio-4x",			/* name */
	    pll_audio_mult_parents,		/* parent */
	    0,					/* freq */
	    4,					/* mult */
	    1,					/* div */
	    0)					/* flags */
	FIXED_CLK(H3_CLK_PLL_AUDIO_8X,		/* id */
	    "pll_audio-8x",			/* name */
	    pll_audio_mult_parents,		/* parent */
	    0,					/* freq */
	    8,					/* mult */
	    1,					/* div */
	    0)					/* flags */
};

static struct aw_clk_init init_clks[] = {
	{"ahb1", "pll_periph0", 0, false},
	{"ahb2", "pll_periph0", 0, false},
};

void
ccu_h3_register_clocks(struct aw_ccung_softc *sc)
{
	int i;

	sc->resets = h3_ccu_resets;
	sc->nresets = nitems(h3_ccu_resets);
	sc->gates = h3_ccu_gates;
	sc->ngates = nitems(h3_ccu_gates);
	sc->clk_init = init_clks;
	sc->n_clk_init = nitems(init_clks);

	for (i = 0; i < nitems(nkmp_clks); i++)
		aw_clk_nkmp_register(sc->clkdom, &nkmp_clks[i]);
	for (i = 0; i < nitems(nm_clks); i++)
		aw_clk_nm_register(sc->clkdom, &nm_clks[i]);
	for (i = 0; i < nitems(prediv_mux_clks); i++)
		aw_clk_prediv_mux_register(sc->clkdom, &prediv_mux_clks[i]);

	for (i = 0; i < nitems(mux_clks); i++)
		clknode_mux_register(sc->clkdom, &mux_clks[i]);
	for (i = 0; i < nitems(div_clks); i++)
		clknode_div_register(sc->clkdom, &div_clks[i]);
	for (i = 0; i < nitems(fixed_factor_clks); i++)
		clknode_fixed_register(sc->clkdom, &fixed_factor_clks[i]);
}
