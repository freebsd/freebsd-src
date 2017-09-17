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

#ifndef __CCU_H3_H__
#define __CCU_H3_H__

#define	H3_RST_USB_PHY0		0
#define	H3_RST_USB_PHY1		1
#define	H3_RST_USB_PHY2		2
#define	H3_RST_USB_PHY3		3
#define	H3_RST_MBUS		4
#define	H3_RST_BUS_CE		5
#define	H3_RST_BUS_DMA		6
#define	H3_RST_BUS_MMC0		7
#define	H3_RST_BUS_MMC1		8
#define	H3_RST_BUS_MMC2		9
#define	H3_RST_BUS_NAND		10
#define	H3_RST_BUS_DRAM		11
#define	H3_RST_BUS_EMAC		12
#define	H3_RST_BUS_TS		13
#define	H3_RST_BUS_HSTIMER	14
#define	H3_RST_BUS_SPI0		15
#define	H3_RST_BUS_SPI1		16
#define	H3_RST_BUS_OTG		17
#define	H3_RST_BUS_EHCI0	18
#define	H3_RST_BUS_EHCI1	19
#define	H3_RST_BUS_EHCI2	20
#define	H3_RST_BUS_EHCI3	21
#define	H3_RST_BUS_OHCI0	22
#define	H3_RST_BUS_OHCI1	23
#define	H3_RST_BUS_OHCI2	24
#define	H3_RST_BUS_OHCI3	25
#define	H3_RST_BUS_VE		26
#define	H3_RST_BUS_TCON0	27
#define	H3_RST_BUS_TCON1	28
#define	H3_RST_BUS_DEINTERLACE	29
#define	H3_RST_BUS_CSI		30
#define	H3_RST_BUS_TVE		31
#define	H3_RST_BUS_HDMI0	32
#define	H3_RST_BUS_HDMI1	33
#define	H3_RST_BUS_DE		34
#define	H3_RST_BUS_GPU		35
#define	H3_RST_BUS_MSGBOX	36
#define	H3_RST_BUS_SPINLOCK	37
#define	H3_RST_BUS_DBG		38
#define	H3_RST_BUS_EPHY		39
#define	H3_RST_BUS_CODEC	40
#define	H3_RST_BUS_SPDIF	41
#define	H3_RST_BUS_THS		42
#define	H3_RST_BUS_I2S0		43
#define	H3_RST_BUS_I2S1		44
#define	H3_RST_BUS_I2S2		45
#define	H3_RST_BUS_I2C0		46
#define	H3_RST_BUS_I2C1		47
#define	H3_RST_BUS_I2C2		48
#define	H3_RST_BUS_UART0	49
#define	H3_RST_BUS_UART1	50
#define	H3_RST_BUS_UART2	51
#define	H3_RST_BUS_UART3	52
#define	H3_RST_BUS_SCR		53

#define	H3_CLK_PLL_CPUX		0
#define	H3_CLK_PLL_AUDIO_BASE	1
#define	H3_CLK_PLL_AUDIO	2
#define	H3_CLK_PLL_AUDIO_2X	3
#define	H3_CLK_PLL_AUDIO_4X	4
#define	H3_CLK_PLL_AUDIO_8X	5
#define	H3_CLK_PLL_VIDEO	6
#define	H3_CLK_PLL_VE		7
#define	H3_CLK_PLL_DDR		8
#define	H3_CLK_PLL_PERIPH0	9
#define	H3_CLK_PLL_PERIPH0_2X	10
#define	H3_CLK_PLL_GPU		11
#define	H3_CLK_PLL_PERIPH1	12
#define	H3_CLK_PLL_DE		13
#define	H3_CLK_CPUX		14
#define	H3_CLK_AXI		15
#define	H3_CLK_AHB1		16
#define	H3_CLK_APB1		17
#define	H3_CLK_APB2		18
#define	H3_CLK_AHB2		19
#define	H3_CLK_BUS_CE		20
#define	H3_CLK_BUS_DMA		21
#define	H3_CLK_BUS_MMC0		22
#define	H3_CLK_BUS_MMC1		23
#define	H3_CLK_BUS_MMC2		24
#define	H3_CLK_BUS_NAND		25
#define	H3_CLK_BUS_DRAM		26
#define	H3_CLK_BUS_EMAC		27
#define	H3_CLK_BUS_TS		28
#define	H3_CLK_BUS_HSTIMER	29
#define	H3_CLK_BUS_SPI0		30
#define	H3_CLK_BUS_SPI1		31
#define	H3_CLK_BUS_OTG		32
#define	H3_CLK_BUS_EHCI0	33
#define	H3_CLK_BUS_EHCI1	34
#define	H3_CLK_BUS_EHCI2	35
#define	H3_CLK_BUS_EHCI3	36
#define	H3_CLK_BUS_OHCI0	37
#define	H3_CLK_BUS_OHCI1	38
#define	H3_CLK_BUS_OHCI2	39
#define	H3_CLK_BUS_OHCI3	40
#define	H3_CLK_BUS_VE		41
#define	H3_CLK_BUS_TCON0	42
#define	H3_CLK_BUS_TCON1	43
#define	H3_CLK_BUS_DEINTERLACE	44
#define	H3_CLK_BUS_CSI		45
#define	H3_CLK_BUS_TVE		46
#define	H3_CLK_BUS_HDMI		47
#define	H3_CLK_BUS_DE		48
#define	H3_CLK_BUS_GPU		49
#define	H3_CLK_BUS_MSGBOX	50
#define	H3_CLK_BUS_SPINLOCK	51
#define	H3_CLK_BUS_CODEC	52
#define	H3_CLK_BUS_SPDIF	53
#define	H3_CLK_BUS_PIO		54
#define	H3_CLK_BUS_THS		55
#define	H3_CLK_BUS_I2S0		56
#define	H3_CLK_BUS_I2S1		57
#define	H3_CLK_BUS_I2S2		58
#define	H3_CLK_BUS_I2C0		59
#define	H3_CLK_BUS_I2C1		60
#define	H3_CLK_BUS_I2C2		61
#define	H3_CLK_BUS_UART0	62
#define	H3_CLK_BUS_UART1	63
#define	H3_CLK_BUS_UART2	64
#define	H3_CLK_BUS_UART3	65
#define	H3_CLK_BUS_SCR		66
#define	H3_CLK_BUS_EPHY		67
#define	H3_CLK_BUS_DBG		68
#define	H3_CLK_THS		69
#define	H3_CLK_NAND		70
#define	H3_CLK_MMC0		71
#define	H3_CLK_MMC0_SAMPLE	72
#define	H3_CLK_MMC0_OUTPUT	73
#define	H3_CLK_MMC1		74
#define	H3_CLK_MMC1_SAMPLE	75
#define	H3_CLK_MMC1_OUTPUT	76
#define	H3_CLK_MMC2		77
#define	H3_CLK_MMC2_SAMPLE	78
#define	H3_CLK_MMC2_OUTPUT	79
#define	H3_CLK_TS		80
#define	H3_CLK_CE		81
#define	H3_CLK_SPI0		82
#define	H3_CLK_SPI1		83
#define	H3_CLK_I2S0		84
#define	H3_CLK_I2S1		85
#define	H3_CLK_I2S2		86
#define	H3_CLK_SPDIF		87
#define	H3_CLK_USBPHY0		88
#define	H3_CLK_USBPHY1		89
#define	H3_CLK_USBPHY2		90
#define	H3_CLK_USBPHY3		91
#define	H3_CLK_USBOHCI0		92
#define	H3_CLK_USBOHCI1		93
#define	H3_CLK_USBOHCI2		94
#define	H3_CLK_USBOHCI3		95
#define	H3_CLK_DRAM		96
#define	H3_CLK_DRAM_VE		97
#define	H3_CLK_DRAM_CSI		98
#define	H3_CLK_DRAM_DEINTERLACE	99
#define	H3_CLK_DRAM_TS		100
#define	H3_CLK_DE		101
#define	H3_CLK_TCON0		102
#define	H3_CLK_TVE		103
#define	H3_CLK_DEINTERLACE	104
#define	H3_CLK_CSI_MISC		105
#define	H3_CLK_CSI_SCLK		106
#define	H3_CLK_CSI_MCLK		107
#define	H3_CLK_VE		108
#define	H3_CLK_AC_DIG		109
#define	H3_CLK_AVS		110
#define	H3_CLK_HDMI		111
#define	H3_CLK_HDMI_DDC		112
#define	H3_CLK_MBUS		113
#define	H3_CLK_GPU		114

void ccu_h3_register_clocks(struct aw_ccung_softc *sc);

#endif /* __CCU_H3_H__ */
