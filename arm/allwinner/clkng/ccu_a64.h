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

#ifndef __CCU_A64_H__
#define __CCU_A64_H__

#define	A64_RST_USB_PHY0		0
#define	A64_RST_USB_PHY1		1
#define	A64_RST_USB_HSIC		2
#define	A64_RST_DRAM			3
#define	A64_RST_MBUS			4
#define	A64_RST_BUS_MIPI_DSI		5
#define	A64_RST_BUS_CE			6
#define	A64_RST_BUS_DMA			7
#define	A64_RST_BUS_MMC0		8
#define	A64_RST_BUS_MMC1		9
#define	A64_RST_BUS_MMC2		10
#define	A64_RST_BUS_NAND		11
#define	A64_RST_BUS_DRAM		12
#define	A64_RST_BUS_EMAC		13
#define	A64_RST_BUS_TS			14
#define	A64_RST_BUS_HSTIMER		15
#define	A64_RST_BUS_SPI0		16
#define	A64_RST_BUS_SPI1		17
#define	A64_RST_BUS_OTG			18
#define	A64_RST_BUS_EHCI0		19
#define	A64_RST_BUS_EHCI1		20
#define	A64_RST_BUS_OHCI0		21
#define	A64_RST_BUS_OHCI1		22
#define	A64_RST_BUS_VE			23
#define	A64_RST_BUS_TCON0		24
#define	A64_RST_BUS_TCON1		25
#define	A64_RST_BUS_DEINTERLACE		26
#define	A64_RST_BUS_CSI			27
#define	A64_RST_BUS_HDMI0		28
#define	A64_RST_BUS_HDMI1		29
#define	A64_RST_BUS_DE			30
#define	A64_RST_BUS_GPU			31
#define	A64_RST_BUS_MSGBOX		32
#define	A64_RST_BUS_SPINLOCK		33
#define	A64_RST_BUS_DBG			34
#define	A64_RST_BUS_LVDS		35
#define	A64_RST_BUS_CODEC		36
#define	A64_RST_BUS_SPDIF		37
#define	A64_RST_BUS_THS			38
#define	A64_RST_BUS_I2S0		39
#define	A64_RST_BUS_I2S1		40
#define	A64_RST_BUS_I2S2		41
#define	A64_RST_BUS_I2C0		42
#define	A64_RST_BUS_I2C1		43
#define	A64_RST_BUS_I2C2		44
#define	A64_RST_BUS_SCR			45
#define	A64_RST_BUS_UART0		46
#define	A64_RST_BUS_UART1		47
#define	A64_RST_BUS_UART2		48
#define	A64_RST_BUS_UART3		49
#define	A64_RST_BUS_UART4		50

#define	A64_CLK_OSC_12M			0
#define	A64_CLK_PLL_CPUX		1
#define	A64_CLK_PLL_AUDIO_BASE		2
#define	A64_CLK_PLL_AUDIO		3
#define	A64_CLK_PLL_AUDIO_2X		4
#define	A64_CLK_PLL_AUDIO_4X		5
#define	A64_CLK_PLL_AUDIO_8X		6
#define	A64_CLK_PLL_VIDEO0		7
#define	A64_CLK_PLL_VIDEO0_2X		8
#define	A64_CLK_PLL_VE			9
#define	A64_CLK_PLL_DDR0		10
#define	A64_CLK_PLL_PERIPH0		11
#define	A64_CLK_PLL_PERIPH0_2X		12
#define	A64_CLK_PLL_PERIPH1		13
#define	A64_CLK_PLL_PERIPH1_2X		14
#define	A64_CLK_PLL_VIDEO1		15
#define	A64_CLK_PLL_GPU			16
#define	A64_CLK_PLL_MIPI		17
#define	A64_CLK_PLL_HSIC		18
#define	A64_CLK_PLL_DE			19
#define	A64_CLK_PLL_DDR1		20
#define	A64_CLK_CPUX			21
#define	A64_CLK_AXI			22
#define	A64_CLK_APB			23
#define	A64_CLK_AHB1			24
#define	A64_CLK_APB1			25
#define	A64_CLK_APB2			26
#define	A64_CLK_AHB2			27
#define	A64_CLK_BUS_MIPI_DSI		28
#define	A64_CLK_BUS_CE			29
#define	A64_CLK_BUS_DMA			30
#define	A64_CLK_BUS_MMC0		31
#define	A64_CLK_BUS_MMC1		32
#define	A64_CLK_BUS_MMC2		33
#define	A64_CLK_BUS_NAND		34
#define	A64_CLK_BUS_DRAM		35
#define	A64_CLK_BUS_EMAC		36
#define	A64_CLK_BUS_TS			37
#define	A64_CLK_BUS_HSTIMER		38
#define	A64_CLK_BUS_SPI0		39
#define	A64_CLK_BUS_SPI1		40
#define	A64_CLK_BUS_OTG			41
#define	A64_CLK_BUS_EHCI0		42
#define	A64_CLK_BUS_EHCI1		43
#define	A64_CLK_BUS_OHCI0		44
#define	A64_CLK_BUS_OHCI1		45
#define	A64_CLK_BUS_VE			46
#define	A64_CLK_BUS_TCON0		47
#define	A64_CLK_BUS_TCON1		48
#define	A64_CLK_BUS_DEINTERLACE		49
#define	A64_CLK_BUS_CSI			50
#define	A64_CLK_BUS_HDMI		51
#define	A64_CLK_BUS_DE			52
#define	A64_CLK_BUS_GPU			53
#define	A64_CLK_BUS_MSGBOX		54
#define	A64_CLK_BUS_SPINLOCK		55
#define	A64_CLK_BUS_CODEC		56
#define	A64_CLK_BUS_SPDIF		57
#define	A64_CLK_BUS_PIO			58
#define	A64_CLK_BUS_THS			59
#define	A64_CLK_BUS_I2S0		60
#define	A64_CLK_BUS_I2S1		61
#define	A64_CLK_BUS_I2S2		62
#define	A64_CLK_BUS_I2C0		63
#define	A64_CLK_BUS_I2C1		64
#define	A64_CLK_BUS_I2C2		65
#define	A64_CLK_BUS_SCR			66
#define	A64_CLK_BUS_UART0		67
#define	A64_CLK_BUS_UART1		68
#define	A64_CLK_BUS_UART2		69
#define	A64_CLK_BUS_UART3		70
#define	A64_CLK_BUS_UART4		71
#define	A64_CLK_BUS_DBG			72
#define	A64_CLK_THS			73
#define	A64_CLK_NAND			74
#define	A64_CLK_MMC0			75
#define	A64_CLK_MMC1			76
#define	A64_CLK_MMC2			77
#define	A64_CLK_TS			78
#define	A64_CLK_CE			79
#define	A64_CLK_SPI0			80
#define	A64_CLK_SPI1			81
#define	A64_CLK_I2S0			82
#define	A64_CLK_I2S1			83
#define	A64_CLK_I2S2			84
#define	A64_CLK_SPDIF			85
#define	A64_CLK_USB_PHY0		86
#define	A64_CLK_USB_PHY1		87
#define	A64_CLK_USB_HSIC		88
#define	A64_CLK_USB_HSIC_12M		89
#define	A64_CLK_USB_OHCI0_12M		90
#define	A64_CLK_USB_OHCI0		91
#define	A64_CLK_USB_OHCI1_12M		92
#define	A64_CLK_USB_OHCI1		93
#define	A64_CLK_DRAM			94
#define	A64_CLK_DRAM_VE			95
#define	A64_CLK_DRAM_CSI		96
#define	A64_CLK_DRAM_DEINTERLACE	97
#define	A64_CLK_DRAM_TS			98
#define	A64_CLK_DE			99
#define	A64_CLK_TCON0			100
#define	A64_CLK_TCON1			101
#define	A64_CLK_DEINTERLACE		102
#define	A64_CLK_CSI_MISC		103
#define	A64_CLK_CSI_SCLK		104
#define	A64_CLK_CSI_MCLK		105
#define	A64_CLK_VE			106
#define	A64_CLK_AC_DIG			107
#define	A64_CLK_AC_DIG_4X		108
#define	A64_CLK_AVS			109
#define	A64_CLK_HDMI			110
#define	A64_CLK_HDMI_DDC		111

#define	A64_CLK_MBUS			112

#define	A64_CLK_DSI_DPHY		113
#define	A64_CLK_GPU			114

void ccu_a64_register_clocks(struct aw_ccung_softc *sc);

#endif /* __CCU_A64_H__ */
