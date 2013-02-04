/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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

#ifndef	_ARM_LPC_LPCREG_H
#define	_ARM_LPC_LPCREG_H

#define	LPC_DEV_PHYS_BASE		0x40000000
#define	LPC_DEV_P5_PHYS_BASE		0x20000000
#define	LPC_DEV_P6_PHYS_BASE		0x30000000
#define	LPC_DEV_BASE			0xd0000000
#define	LPC_DEV_SIZE			0x10000000

/*
 * Interrupt controller (from UM10326: LPC32x0 User manual, page 87)

 */
#define	LPC_INTC_MIC_ER			0x0000
#define	LPC_INTC_MIC_RSR		0x0004
#define	LPC_INTC_MIC_SR			0x0008
#define	LPC_INTC_MIC_APR		0x000c
#define	LPC_INTC_MIC_ATR		0x0010
#define	LPC_INTC_MIC_ITR		0x0014
#define	LPC_INTC_SIC1_ER		0x4000
#define	LPC_INTC_SIC1_RSR		0x4004
#define	LPC_INTC_SIC1_SR		0x4008
#define	LPC_INTC_SIC1_APR		0x400c
#define	LPC_INTC_SIC1_ATR		0x4010
#define	LPC_INTC_SIC1_ITR		0x4014
#define	LPC_INTC_SIC2_ER		0x8000
#define	LPC_INTC_SIC2_RSR		0x8004
#define	LPC_INTC_SIC2_SR		0x8008
#define	LPC_INTC_SIC2_APR		0x800c
#define	LPC_INTC_SIC2_ATR		0x8010
#define	LPC_INTC_SIC2_ITR		0x8014


/*
 * Timer 0|1|2|3|4|5. (from UM10326: LPC32x0 User manual, page 540)
 */
#define	LPC_TIMER_IR			0x00
#define	LPC_TIMER_TCR			0x04
#define	LPC_TIMER_TCR_ENABLE		(1 << 0)
#define	LPC_TIMER_TCR_RESET		(1 << 1)
#define	LPC_TIMER_TC			0x08
#define	LPC_TIMER_PR			0x0c
#define	LPC_TIMER_PC			0x10
#define	LPC_TIMER_MCR			0x14
#define	LPC_TIMER_MCR_MR0I		(1 << 0)
#define	LPC_TIMER_MCR_MR0R		(1 << 1)
#define	LPC_TIMER_MCR_MR0S		(1 << 2)
#define	LPC_TIMER_MCR_MR1I		(1 << 3)
#define	LPC_TIMER_MCR_MR1R		(1 << 4)
#define	LPC_TIMER_MCR_MR1S		(1 << 5)
#define	LPC_TIMER_MCR_MR2I		(1 << 6)
#define	LPC_TIMER_MCR_MR2R		(1 << 7)
#define	LPC_TIMER_MCR_MR2S		(1 << 8)
#define	LPC_TIMER_MCR_MR3I		(1 << 9)
#define	LPC_TIMER_MCR_MR3R		(1 << 10)
#define	LPC_TIMER_MCR_MR3S		(1 << 11)
#define	LPC_TIMER_MR0			0x18
#define	LPC_TIMER_CTCR			0x70

/*
 * Watchdog timer. (from UM10326: LPC32x0 User manual, page 572)
 */
#define	LPC_WDTIM_BASE			(LPC_DEV_BASE + 0x3c000)
#define	LPC_WDTIM_INT			0x00
#define	LPC_WDTIM_CTRL			0x04
#define	LPC_WDTIM_COUNTER		0x08
#define	LPC_WDTIM_MCTRL			0x0c
#define	LPC_WDTIM_MATCH0		0x10
#define	LPC_WDTIM_EMR			0x14
#define	LPC_WDTIM_PULSE			0x18
#define	LPC_WDTIM_RES			0x1c

/*
 * Clocking and power control. (from UM10326: LPC32x0 User manual, page 58)
 */
#define	LPC_CLKPWR_BASE			(LPC_DEV_BASE + 0x4000)
#define	LPC_CLKPWR_PWR_CTRL		0x44
#define	LPC_CLKPWR_OSC_CTRL		0x4c
#define	LPC_CLKPWR_SYSCLK_CTRL		0x50
#define	LPC_CLKPWR_PLL397_CTRL		0x48
#define	LPC_CLKPWR_HCLKPLL_CTRL		0x58
#define	LPC_CLKPWR_HCLKDIV_CTRL		0x40
#define	LPC_CLKPWR_TEST_CTRL		0xa4
#define	LPC_CLKPWR_AUTOCLK_CTRL		0xec
#define	LPC_CLKPWR_START_ER_PIN		0x30
#define	LPC_CLKPWR_START_ER_INT		0x20
#define	LPC_CLKPWR_P0_INTR_ER		0x18
#define	LPC_CLKPWR_START_SR_PIN		0x38
#define	LPC_CLKPWR_START_SR_INT		0x28
#define	LPC_CLKPWR_START_RSR_PIN	0x34
#define	LPC_CLKPWR_START_RSR_INT	0x24
#define	LPC_CLKPWR_START_APR_PIN	0x3c
#define	LPC_CLKPWR_START_APR_INT	0x2c
#define	LPC_CLKPWR_USB_CTRL		0x64
#define	LPC_CLKPWR_USB_CTRL_SLAVE_HCLK	(1 << 24)
#define	LPC_CLKPWR_USB_CTRL_I2C_EN	(1 << 23)
#define	LPC_CLKPWR_USB_CTRL_DEV_NEED_CLK_EN	(1 << 22)
#define	LPC_CLKPWR_USB_CTRL_HOST_NEED_CLK_EN	(1 << 21)
#define	LPC_CLKPWR_USB_CTRL_BUSKEEPER	(1 << 19)
#define	LPC_CLKPWR_USB_CTRL_CLK_EN2	(1 << 18)
#define	LPC_CLKPWR_USB_CTRL_CLK_EN1	(1 << 17)
#define	LPC_CLKPWR_USB_CTRL_PLL_PDOWN	(1 << 16)
#define	LPC_CLKPWR_USB_CTRL_BYPASS	(1 << 15)
#define	LPC_CLKPWR_USB_CTRL_DIRECT_OUT	(1 << 14)
#define	LPC_CLKPWR_USB_CTRL_FEEDBACK	(1 << 13)
#define	LPC_CLKPWR_USB_CTRL_POSTDIV(_x)	((_x & 0x3) << 11)
#define	LPC_CLKPWR_USB_CTRL_PREDIV(_x)	((_x & 0x3) << 9)
#define	LPC_CLKPWR_USB_CTRL_FDBKDIV(_x)	(((_x-1) & 0xff) << 1)
#define	LPC_CLKPWR_USB_CTRL_PLL_LOCK	(1 << 0)
#define	LPC_CLKPWR_USBDIV_CTRL		0x1c
#define	LPC_CLKPWR_MS_CTRL		0x80
#define	LPC_CLKPWR_MS_CTRL_DISABLE_SD	(1 << 10)
#define	LPC_CLKPWR_MS_CTRL_CLOCK_EN	(1 << 9)
#define	LPC_CLKPWR_MS_CTRL_MSSDIO23_PAD	(1 << 8)
#define	LPC_CLKPWR_MS_CTRL_MSSDIO1_PAD	(1 << 7)
#define	LPC_CLKPWR_MS_CTRL_MSSDIO0_PAD	(1 << 6)
#define	LPC_CLKPWR_MS_CTRL_SD_CLOCK	(1 << 5)
#define	LPC_CLKPWR_MS_CTRL_CLKDIV_MASK	0xf
#define	LPC_CLKPWR_DMACLK_CTRL		0xe8
#define	LPC_CLKPWR_DMACLK_CTRL_EN	(1 << 0)
#define	LPC_CLKPWR_FLASHCLK_CTRL	0xc8
#define	LPC_CLKPWR_MACCLK_CTRL		0x90
#define	LPC_CLKPWR_MACCLK_CTRL_REG	(1 << 0)
#define	LPC_CLKPWR_MACCLK_CTRL_SLAVE	(1 << 1)
#define	LPC_CLKPWR_MACCLK_CTRL_MASTER	(1 << 2)
#define	LPC_CLKPWR_MACCLK_CTRL_HDWINF(_n) ((_n & 0x3) << 3)
#define	LPC_CLKPWR_LCDCLK_CTRL		0x54
#define	LPC_CLKPWR_LCDCLK_CTRL_DISPTYPE	(1 << 8)
#define	LPC_CLKPWR_LCDCLK_CTRL_MODE(_n)	((_n & 0x3) << 6)
#define	LPC_CLKPWR_LCDCLK_CTRL_MODE_12	0x0
#define	LPC_CLKPWR_LCDCLK_CTRL_MODE_15	0x1
#define	LPC_CLKPWR_LCDCLK_CTRL_MODE_16	0x2
#define	LPC_CLKPWR_LCDCLK_CTRL_MODE_24	0x3
#define	LPC_CLKPWR_LCDCLK_CTRL_HCLKEN	(1 << 5)
#define	LPC_CLKPWR_LCDCLK_CTRL_CLKDIV(_n) ((_n) & 0x1f)
#define	LPC_CLKPWR_I2S_CTRL		0x7c
#define	LPC_CLKPWR_SSP_CTRL		0x78
#define	LPC_CLKPWR_SSP_CTRL_SSP1RXDMA	(1 << 5)
#define	LPC_CLKPWR_SSP_CTRL_SSP1TXDMA	(1 << 4)
#define	LPC_CLKPWR_SSP_CTRL_SSP0RXDMA	(1 << 3)
#define	LPC_CLKPWR_SSP_CTRL_SSP0TXDMA	(1 << 2)
#define	LPC_CLKPWR_SSP_CTRL_SSP1EN	(1 << 1)
#define	LPC_CLKPWR_SSP_CTRL_SSP0EN	(1 << 0)
#define	LPC_CLKPWR_SPI_CTRL		0xc4
#define	LPC_CLKPWR_I2CCLK_CTRL		0xac
#define	LPC_CLKPWR_TIMCLK_CTRL1		0xc0
#define	LPC_CLKPWR_TIMCLK_CTRL1_TIMER4	(1 << 0)
#define	LPC_CLKPWR_TIMCLK_CTRL1_TIMER5	(1 << 1)
#define	LPC_CLKPWR_TIMCLK_CTRL1_TIMER0	(1 << 2)
#define	LPC_CLKPWR_TIMCLK_CTRL1_TIMER1	(1 << 3)
#define	LPC_CLKPWR_TIMCLK_CTRL1_TIMER2	(1 << 4)
#define	LPC_CLKPWR_TIMCLK_CTRL1_TIMER3	(1 << 5)
#define	LPC_CLKPWR_TIMCLK_CTRL1_MOTORCTL	(1 << 6)
#define	LPC_CLKPWR_TIMCLK_CTRL		0xbc
#define	LPC_CLKPWR_TIMCLK_CTRL_WATCHDOG	(1 << 0)
#define	LPC_CLKPWR_TIMCLK_CTRL_HSTIMER	(1 << 1)
#define	LPC_CLKPWR_ADCLK_CTRL		0xb4
#define	LPC_CLKPWR_ADCLK_CTRL1		0x60
#define	LPC_CLKPWR_KEYCLK_CTRL		0xb0
#define	LPC_CLKPWR_PWMCLK_CTRL		0xb8
#define	LPC_CLKPWR_UARTCLK_CTRL		0xe4
#define	LPC_CLKPWR_POS0_IRAM_CTRL	0x110
#define	LPC_CLKPWR_POS1_IRAM_CTRL	0x114

/* Additional UART registers in CLKPWR address space. */
#define	LPC_CLKPWR_UART_U3CLK		0xd0
#define	LPC_CLKPWR_UART_U4CLK		0xd4
#define	LPC_CLKPWR_UART_U5CLK		0xd8
#define	LPC_CLKPWR_UART_U6CLK		0xdc
#define	LPC_CLKPWR_UART_UCLK_HCLK	(1 << 16)
#define	LPC_CLKPWR_UART_UCLK_X(_n)	(((_n) & 0xff) << 8)
#define	LPC_CLKPWR_UART_UCLK_Y(_n)	((_n) & 0xff)
#define	LPC_CLKPWR_UART_IRDACLK		0xe0

/* Additional UART registers */
#define	LPC_UART_BASE			(LPC_DEV_BASE + 0x80000)
#define	LPC_UART_CONTROL_BASE		(LPC_DEV_BASE + 0x54000)
#define	LPC_UART5_BASE			(LPC_DEV_BASE + 0x90000)
#define	LPC_UART_CTRL			0x00
#define	LPC_UART_CLKMODE		0x04
#define	LPC_UART_CLKMODE_UART3(_n)	(((_n) & 0x3) << 4)
#define	LPC_UART_CLKMODE_UART4(_n)	(((_n) & 0x3) << 6)
#define	LPC_UART_CLKMODE_UART5(_n)	(((_n) & 0x3) << 8)
#define	LPC_UART_CLKMODE_UART6(_n)	(((_n) & 0x3) << 10)
#define	LPC_UART_LOOP			0x08
#define	LPC_UART_FIFOSIZE		64

/*
 * Real time clock. (from UM10326: LPC32x0 User manual, page 566)
 */
#define	LPC_RTC_UCOUNT			0x00
#define	LPC_RTC_DCOUNT			0x04
#define	LPC_RTC_MATCH0			0x08
#define	LPC_RTC_MATCH1			0x0c
#define	LPC_RTC_CTRL			0x10
#define	LPC_RTC_CTRL_ONSW		(1 << 7)
#define	LPC_RTC_CTRL_DISABLE		(1 << 6)
#define	LPC_RTC_CTRL_RTCRESET		(1 << 4)
#define	LPC_RTC_CTRL_MATCH0ONSW		(1 << 3)
#define	LPC_RTC_CTRL_MATCH1ONSW		(1 << 2)
#define	LPC_RTC_CTRL_MATCH1INTR		(1 << 1)
#define	LPC_RTC_CTRL_MATCH0INTR		(1 << 0)
#define	LPC_RTC_INTSTAT			0x14
#define	LPC_RTC_KEY			0x18
#define	LPC_RTC_SRAM_BEGIN		0x80
#define LPC_RTC_SRAM_END		0xff

/*
 * MMC/SD controller. (from UM10326: LPC32x0 User manual, page 436)
 */
#define	LPC_SD_BASE			(LPC_DEV_P5_PHYS_BASE + 0x98000)
#define	LPC_SD_CLK			(13 * 1000 * 1000)	// 13Mhz
#define	LPC_SD_POWER			0x00
#define	LPC_SD_POWER_OPENDRAIN		(1 << 6)
#define	LPC_SD_POWER_CTRL_OFF		0x00
#define	LPC_SD_POWER_CTRL_UP		0x02
#define	LPC_SD_POWER_CTRL_ON		0x03
#define	LPC_SD_CLOCK			0x04
#define	LPC_SD_CLOCK_WIDEBUS		(1 << 11)
#define	LPC_SD_CLOCK_BYPASS		(1 << 10)
#define	LPC_SD_CLOCK_PWRSAVE		(1 << 9)
#define	LPC_SD_CLOCK_ENABLE		(1 << 8)
#define	LPC_SD_CLOCK_CLKDIVMASK		0xff
#define	LPC_SD_ARGUMENT			0x08
#define	LPC_SD_COMMAND			0x0c
#define	LPC_SD_COMMAND_ENABLE		(1 << 10)
#define	LPC_SD_COMMAND_PENDING		(1 << 9)
#define	LPC_SD_COMMAND_INTERRUPT	(1 << 8)
#define	LPC_SD_COMMAND_LONGRSP		(1 << 7)
#define	LPC_SD_COMMAND_RESPONSE		(1 << 6)
#define	LPC_SD_COMMAND_CMDINDEXMASK	0x3f
#define	LPC_SD_RESPCMD			0x10
#define	LPC_SD_RESP0			0x14
#define	LPC_SD_RESP1			0x18
#define	LPC_SD_RESP2			0x1c
#define	LPC_SD_RESP3			0x20
#define	LPC_SD_DATATIMER		0x24
#define	LPC_SD_DATALENGTH		0x28
#define	LPC_SD_DATACTRL			0x2c
#define	LPC_SD_DATACTRL_BLOCKSIZESHIFT	4
#define	LPC_SD_DATACTRL_BLOCKSIZEMASK	0xf
#define	LPC_SD_DATACTRL_DMAENABLE	(1 << 3)
#define	LPC_SD_DATACTRL_MODE		(1 << 2)
#define	LPC_SD_DATACTRL_WRITE		(0 << 1)
#define	LPC_SD_DATACTRL_READ		(1 << 1)
#define	LPC_SD_DATACTRL_ENABLE		(1 << 0)
#define	LPC_SD_DATACNT			0x30
#define	LPC_SD_STATUS			0x34
#define	LPC_SD_STATUS_RXDATAAVLBL	(1 << 21)
#define	LPC_SD_STATUS_TXDATAAVLBL	(1 << 20)
#define	LPC_SD_STATUS_RXFIFOEMPTY	(1 << 19)
#define	LPC_SD_STATUS_TXFIFOEMPTY	(1 << 18)
#define	LPC_SD_STATUS_RXFIFOFULL	(1 << 17)
#define	LPC_SD_STATUS_TXFIFOFULL	(1 << 16)
#define	LPC_SD_STATUS_RXFIFOHALFFULL	(1 << 15)
#define	LPC_SD_STATUS_TXFIFOHALFEMPTY	(1 << 14)
#define	LPC_SD_STATUS_RXACTIVE		(1 << 13)
#define	LPC_SD_STATUS_TXACTIVE		(1 << 12)
#define	LPC_SD_STATUS_CMDACTIVE		(1 << 11)
#define	LPC_SD_STATUS_DATABLOCKEND	(1 << 10)
#define	LPC_SD_STATUS_STARTBITERR	(1 << 9)
#define	LPC_SD_STATUS_DATAEND		(1 << 8)
#define	LPC_SD_STATUS_CMDSENT		(1 << 7)
#define	LPC_SD_STATUS_CMDRESPEND	(1 << 6)
#define	LPC_SD_STATUS_RXOVERRUN		(1 << 5)
#define	LPC_SD_STATUS_TXUNDERRUN	(1 << 4)
#define	LPC_SD_STATUS_DATATIMEOUT	(1 << 3)
#define	LPC_SD_STATUS_CMDTIMEOUT	(1 << 2)
#define	LPC_SD_STATUS_DATACRCFAIL	(1 << 1)
#define	LPC_SD_STATUS_CMDCRCFAIL	(1 << 0)
#define	LPC_SD_CLEAR			0x38
#define	LPC_SD_MASK0			0x03c
#define	LPC_SD_MASK1			0x40
#define	LPC_SD_FIFOCNT			0x48
#define	LPC_SD_FIFO			0x80

/*
 * USB OTG controller (from UM10326: LPC32x0 User manual, page 410)
 */
#define	LPC_OTG_INT_STATUS		0x100
#define	LPC_OTG_INT_ENABLE		0x104
#define	LPC_OTG_INT_SET			0x108
#define	LPC_OTG_INT_CLEAR		0x10c
#define	LPC_OTG_STATUS			0x110
#define	LPC_OTG_STATUS_ATOB_HNP_TRACK	(1 << 9)
#define	LPC_OTG_STATUS_BTOA_HNP_TACK	(1 << 8)
#define	LPC_OTG_STATUS_TRANSP_I2C_EN	(1 << 7)
#define	LPC_OTG_STATUS_TIMER_RESET	(1 << 6)
#define	LPC_OTG_STATUS_TIMER_EN		(1 << 5)
#define	LPC_OTG_STATUS_TIMER_MODE	(1 << 4)
#define	LPC_OTG_STATUS_TIMER_SCALE	(1 << 2)
#define	LPC_OTG_STATUS_HOST_EN		(1 << 0)
#define	LPC_OTG_TIMER			0x114
#define	LPC_OTG_I2C_TXRX		0x300
#define	LPC_OTG_I2C_STATUS		0x304
#define	LPC_OTG_I2C_STATUS_TFE		(1 << 11)
#define	LPC_OTG_I2C_STATUS_TFF		(1 << 10)
#define	LPC_OTG_I2C_STATUS_RFE		(1 << 9)
#define	LPC_OTG_I2C_STATUS_RFF		(1 << 8)
#define	LPC_OTG_I2C_STATUS_SDA		(1 << 7)
#define	LPC_OTG_I2C_STATUS_SCL		(1 << 6)
#define	LPC_OTG_I2C_STATUS_ACTIVE	(1 << 5)
#define	LPC_OTG_I2C_STATUS_DRSI		(1 << 4)
#define	LPC_OTG_I2C_STATUS_DRMI		(1 << 3)
#define	LPC_OTG_I2C_STATUS_NAI		(1 << 2)
#define	LPC_OTG_I2C_STATUS_AFI		(1 << 1)
#define	LPC_OTG_I2C_STATUS_TDI		(1 << 0)
#define	LPC_OTG_I2C_CTRL		0x308
#define	LPC_OTG_I2C_CTRL_SRST		(1 << 8)
#define	LPC_OTG_I2C_CTRL_TFFIE		(1 << 7)
#define	LPC_OTG_I2C_CTRL_RFDAIE		(1 << 6)
#define	LPC_OTG_I2C_CTRL_RFFIE		(1 << 5)
#define	LPC_OTG_I2C_CTRL_DRSIE		(1 << 4)
#define	LPC_OTG_I2C_CTRL_DRMIE		(1 << 3)
#define	LPC_OTG_I2C_CTRL_NAIE		(1 << 2)
#define	LPC_OTG_I2C_CTRL_AFIE		(1 << 1)
#define	LPC_OTG_I2C_CTRL_TDIE		(1 << 0)
#define	LPC_OTG_I2C_CLKHI		0x30c
#define	LPC_OTG_I2C_CLKLO		0x310
#define	LPC_OTG_CLOCK_CTRL		0xff4
#define	LPC_OTG_CLOCK_CTRL_AHB_EN	(1 << 4)
#define	LPC_OTG_CLOCK_CTRL_OTG_EN	(1 << 3)
#define	LPC_OTG_CLOCK_CTRL_I2C_EN	(1 << 2)
#define	LPC_OTG_CLOCK_CTRL_DEV_EN	(1 << 1)
#define	LPC_OTG_CLOCK_CTRL_HOST_EN	(1 << 0)
#define	LPC_OTG_CLOCK_STATUS		0xff8

/*
 * ISP3101 USB transceiver registers
 */
#define	LPC_ISP3101_I2C_ADDR		0x2d
#define	LPC_ISP3101_MODE_CONTROL_1	0x04
#define	LPC_ISP3101_MC1_SPEED_REG	(1 << 0)
#define	LPC_ISP3101_MC1_SUSPEND_REG	(1 << 1)
#define	LPC_ISP3101_MC1_DAT_SE0		(1 << 2)
#define	LPC_ISP3101_MC1_TRANSPARENT	(1 << 3)
#define	LPC_ISP3101_MC1_BDIS_ACON_EN	(1 << 4)
#define	LPC_ISP3101_MC1_OE_INT_EN	(1 << 5)
#define	LPC_ISP3101_MC1_UART_EN		(1 << 6)
#define	LPC_ISP3101_MODE_CONTROL_2	0x12
#define	LPC_ISP3101_MC2_GLOBAL_PWR_DN	(1 << 0)
#define	LPC_ISP3101_MC2_SPD_SUSP_CTRL	(1 << 1)
#define	LPC_ISP3101_MC2_BI_DI		(1 << 2)
#define	LPC_ISP3101_MC2_TRANSP_BDIR0	(1 << 3)
#define	LPC_ISP3101_MC2_TRANSP_BDIR1	(1 << 4)
#define	LPC_ISP3101_MC2_AUDIO_EN	(1 << 5)
#define	LPC_ISP3101_MC2_PSW_EN		(1 << 6)
#define	LPC_ISP3101_MC2_EN2V7		(1 << 7)
#define	LPC_ISP3101_OTG_CONTROL_1	0x06
#define	LPC_ISP3101_OTG1_DP_PULLUP	(1 << 0)
#define	LPC_ISP3101_OTG1_DM_PULLUP	(1 << 1)
#define	LPC_ISP3101_OTG1_DP_PULLDOWN	(1 << 2)
#define	LPC_ISP3101_OTG1_DM_PULLDOWN	(1 << 3)
#define	LPC_ISP3101_OTG1_ID_PULLDOWN	(1 << 4)
#define	LPC_ISP3101_OTG1_VBUS_DRV	(1 << 5)
#define	LPC_ISP3101_OTG1_VBUS_DISCHRG	(1 << 6)
#define	LPC_ISP3101_OTG1_VBUS_CHRG	(1 << 7)
#define	LPC_ISP3101_OTG_CONTROL_2	0x10
#define	LPC_ISP3101_OTG_INTR_LATCH	0x0a
#define	LPC_ISP3101_OTG_INTR_FALLING	0x0c
#define	LPC_ISP3101_OTG_INTR_RISING	0x0e
#define	LPC_ISP3101_REG_CLEAR_ADDR	0x01

/*
 * LCD Controller (from UM10326: LPC32x0 User manual, page 229)
 */
#define	LPC_LCD_TIMH			0x00
#define	LPC_LCD_TIMH_HBP(_n)		(((_n) & 0xff) << 24)
#define	LPC_LCD_TIMH_HFP(_n)		(((_n) & 0xff) << 16)
#define	LPC_LCD_TIMH_HSW(_n)		(((_n) & 0xff) << 8)
#define	LPC_LCD_TIMH_PPL(_n)		(((_n) / 16 - 1) << 2)
#define	LPC_LCD_TIMV			0x04
#define	LPC_LCD_TIMV_VBP(_n)		(((_n) & 0xff) << 24)
#define	LPC_LCD_TIMV_VFP(_n)		(((_n) & 0xff) << 16)
#define	LPC_LCD_TIMV_VSW(_n)		(((_n) & 0x3f) << 10)
#define	LPC_LCD_TIMV_LPP(_n)		((_n) & 0x1ff)
#define	LPC_LCD_POL			0x08
#define	LPC_LCD_POL_PCD_HI		(((_n) & 0x1f) << 27)
#define	LPC_LCD_POL_BCD			(1 << 26)
#define	LPC_LCD_POL_CPL(_n)		(((_n) & 0x3ff) << 16)
#define	LPC_LCD_POL_IOE			(1 << 14)
#define	LPC_LCD_POL_IPC			(1 << 13)
#define	LPC_LCD_POL_IHS			(1 << 12)
#define	LPC_LCD_POL_IVS			(1 << 11)
#define	LPC_LCD_POL_ACB(_n)		((_n & 0x1f) << 6)
#define	LPC_LCD_POL_CLKSEL		(1 << 5)
#define	LPC_LCD_POL_PCD_LO(_n)		((_n) & 0x1f)
#define	LPC_LCD_LE			0x0c
#define	LPC_LCD_LE_LEE			(1 << 16)
#define	LPC_LCD_LE_LED			((_n) & 0x7f)
#define	LPC_LCD_UPBASE			0x10
#define	LPC_LCD_LPBASE			0x14
#define	LPC_LCD_CTRL			0x18
#define	LPC_LCD_CTRL_WATERMARK		(1 << 16)
#define	LPC_LCD_CTRL_LCDVCOMP(_n)	(((_n) & 0x3) << 12)
#define	LPC_LCD_CTRL_LCDPWR		(1 << 11)
#define	LPC_LCD_CTRL_BEPO		(1 << 10)
#define	LPC_LCD_CTRL_BEBO		(1 << 9)
#define	LPC_LCD_CTRL_BGR		(1 << 8)
#define	LPC_LCD_CTRL_LCDDUAL		(1 << 7)
#define	LPC_LCD_CTRL_LCDMONO8		(1 << 6)
#define	LPC_LCD_CTRL_LCDTFT		(1 << 5)
#define	LPC_LCD_CTRL_LCDBW		(1 << 4)
#define	LPC_LCD_CTRL_LCDBPP(_n)		(((_n) & 0x7) << 1)
#define	LPC_LCD_CTRL_BPP1		0
#define	LPC_LCD_CTRL_BPP2		1
#define	LPC_LCD_CTRL_BPP4		2
#define	LPC_LCD_CTRL_BPP8		3
#define	LPC_LCD_CTRL_BPP16		4
#define	LPC_LCD_CTRL_BPP24		5
#define	LPC_LCD_CTRL_BPP16_565		6
#define	LPC_LCD_CTRL_BPP12_444		7
#define	LPC_LCD_CTRL_LCDEN		(1 << 0)
#define	LPC_LCD_INTMSK			0x1c
#define	LPC_LCD_INTRAW			0x20
#define	LPC_LCD_INTSTAT			0x24
#define	LPC_LCD_INTCLR			0x28
#define	LPC_LCD_UPCURR			0x2c
#define	LPC_LCD_LPCURR			0x30
#define	LPC_LCD_PAL			0x200
#define	LPC_LCD_CRSR_IMG		0x800
#define	LPC_LCD_CRSR_CTRL		0xc00
#define	LPC_LCD_CRSR_CFG		0xc04
#define	LPC_LCD_CRSR_PAL0		0xc08
#define	LPC_LCD_CRSR_PAL1		0xc0c
#define	LPC_LCD_CRSR_XY			0xc10
#define	LPC_LCD_CRSR_CLIP		0xc14
#define	LPC_LCD_CRSR_INTMSK		0xc20
#define	LPC_LCD_CRSR_INTCLR		0xc24
#define	LPC_LCD_CRSR_INTRAW		0xc28
#define	LPC_LCD_CRSR_INTSTAT		0xc2c

/*
 * SPI interface (from UM10326: LPC32x0 User manual, page 483)
 */
#define	LPC_SPI_GLOBAL			0x00
#define	LPC_SPI_GLOBAL_RST		(1 << 1)
#define	LPC_SPI_GLOBAL_ENABLE		(1 << 0)
#define	LPC_SPI_CON			0x04
#define	LPC_SPI_CON_UNIDIR		(1 << 23)
#define	LPC_SPI_CON_BHALT		(1 << 22)
#define	LPC_SPI_CON_BPOL		(1 << 21)
#define	LPC_SPI_CON_MSB			(1 << 19)
#define	LPC_SPI_CON_MODE(_n)		((_n & 0x3) << 16)
#define	LPC_SPI_CON_RXTX		(1 << 15)
#define	LPC_SPI_CON_THR			(1 << 14)
#define	LPC_SPI_CON_SHIFT_OFF		(1 << 13)
#define	LPC_SPI_CON_BITNUM(_n)		((_n & 0xf) << 9)
#define	LPC_SPI_CON_MS			(1 << 7)
#define	LPC_SPI_CON_RATE(_n)		(_n & 0x7f)
#define	LPC_SPI_FRM			0x08
#define	LPC_SPI_IER			0x0c
#define	LPC_SPI_IER_INTEOT		(1 << 1)
#define	LPC_SPI_IER_INTTHR		(1 << 0)
#define	LPC_SPI_STAT			0x10
#define	LPC_SPI_STAT_INTCLR		(1 << 8)
#define	LPC_SPI_STAT_EOT		(1 << 7)
#define	LPC_SPI_STAT_BUSYLEV		(1 << 6)
#define	LPC_SPI_STAT_SHIFTACT		(1 << 3)
#define	LPC_SPI_STAT_BF			(1 << 2)
#define	LPC_SPI_STAT_THR		(1 << 1)
#define	LPC_SPI_STAT_BE			(1 << 0)
#define	LPC_SPI_DAT			0x14
#define	LPC_SPI_TIM_CTRL		0x400
#define	LPC_SPI_TIM_COUNT		0x404
#define	LPC_SPI_TIM_STAT		0x408

/*
 * SSP interface (from UM10326: LPC32x0 User manual, page 500)
 */
#define	LPC_SSP0_BASE			0x4c00
#define	LPC_SSP1_BASE			0xc000
#define	LPC_SSP_CR0			0x00
#define	LPC_SSP_CR0_DSS(_n)		((_n-1) & 0xf)
#define	LPC_SSP_CR0_TI			(1 << 4)
#define	LPC_SSP_CR0_MICROWIRE		(1 << 5)
#define	LPC_SSP_CR0_CPOL		(1 << 6)
#define	LPC_SSP_CR0_CPHA		(1 << 7)
#define	LPC_SSP_CR0_SCR(_n)		((_x & & 0xff) << 8)
#define	LPC_SSP_CR1			0x04
#define	LPC_SSP_CR1_LBM			(1 << 0)
#define	LPC_SSP_CR1_SSE			(1 << 1)
#define	LPC_SSP_CR1_MS			(1 << 2)
#define	LPC_SSP_CR1_SOD			(1 << 3)
#define	LPC_SSP_DR			0x08
#define	LPC_SSP_SR			0x0c
#define	LPC_SSP_SR_TFE			(1 << 0)
#define	LPC_SSP_SR_TNF			(1 << 1)
#define	LPC_SSP_SR_RNE			(1 << 2)
#define	LPC_SSP_SR_RFF			(1 << 3)
#define	LPC_SSP_SR_BSY			(1 << 4)
#define	LPC_SSP_CPSR			0x10
#define	LPC_SSP_IMSC			0x14
#define	LPC_SSP_IMSC_RORIM		(1 << 0)
#define	LPC_SSP_IMSC_RTIM		(1 << 1)
#define	LPC_SSP_IMSC_RXIM		(1 << 2)
#define	LPC_SSP_IMSC_TXIM		(1 << 3)
#define	LPC_SSP_RIS			0x18
#define	LPC_SSP_RIS_RORRIS		(1 << 0)
#define	LPC_SSP_RIS_RTRIS		(1 << 1)
#define	LPC_SSP_RIS_RXRIS		(1 << 2)
#define	LPC_SSP_RIS_TXRIS		(1 << 3)
#define	LPC_SSP_MIS			0x1c
#define	LPC_SSP_ICR			0x20
#define	LPC_SSP_DMACR			0x24

/*
 * GPIO (from UM10326: LPC32x0 User manual, page 606)
 */
#define	LPC_GPIO_BASE			(LPC_DEV_BASE + 0x28000)
#define	LPC_GPIO_P0_COUNT		8
#define	LPC_GPIO_P1_COUNT		24
#define	LPC_GPIO_P2_COUNT		13
#define	LPC_GPIO_P3_COUNT		52
#define	LPC_GPIO_P0_INP_STATE		0x40
#define	LPC_GPIO_P0_OUTP_SET		0x44
#define	LPC_GPIO_P0_OUTP_CLR		0x48
#define	LPC_GPIO_P0_OUTP_STATE		0x4c
#define	LPC_GPIO_P0_DIR_SET		0x50
#define	LPC_GPIO_P0_DIR_CLR		0x54
#define	LPC_GPIO_P0_DIR_STATE		0x58
#define	LPC_GPIO_P1_INP_STATE		0x60
#define	LPC_GPIO_P1_OUTP_SET		0x64
#define	LPC_GPIO_P1_OUTP_CLR		0x68
#define	LPC_GPIO_P1_OUTP_STATE		0x6c
#define	LPC_GPIO_P1_DIR_SET		0x70
#define	LPC_GPIO_P1_DIR_CLR		0x74
#define	LPC_GPIO_P1_DIR_STATE		0x78
#define	LPC_GPIO_P2_INP_STATE		0x1c
#define	LPC_GPIO_P2_OUTP_SET		0x20
#define	LPC_GPIO_P2_OUTP_CLR		0x24
#define	LPC_GPIO_P2_DIR_SET		0x10
#define	LPC_GPIO_P2_DIR_CLR		0x14
#define	LPC_GPIO_P2_DIR_STATE		0x14
#define	LPC_GPIO_P3_INP_STATE		0x00
#define	LPC_GPIO_P3_OUTP_SET		0x04
#define	LPC_GPIO_P3_OUTP_CLR		0x08
#define	LPC_GPIO_P3_OUTP_STATE		0x0c
/* Aliases for logical pin numbers: */
#define	LPC_GPIO_GPI_00(_n)		(0 + _n)
#define	LPC_GPIO_GPI_15(_n)		(10 + _n)
#define	LPC_GPIO_GPI_25			(19)
#define	LPC_GPIO_GPI_27(_n)		(20 + _n)
#define	LPC_GPIO_GPO_00(_n)		(22 + _n)
#define	LPC_GPIO_GPIO_00(_n)		(46 + _n)
/* SPI devices chip selects: */
#define	SSD1289_CS_PIN			LPC_GPIO_GPO_00(4)
#define	SSD1289_DC_PIN			LPC_GPIO_GPO_00(5)
#define	ADS7846_CS_PIN			LPC_GPIO_GPO_00(11)
#define	ADS7846_INTR_PIN		LPC_GPIO_GPIO_00(0)

/*
 * GPDMA controller (from UM10326: LPC32x0 User manual, page 106)
 */
#define	LPC_DMAC_INTSTAT		0x00
#define	LPC_DMAC_INTTCSTAT		0x04
#define	LPC_DMAC_INTTCCLEAR		0x08
#define	LPC_DMAC_INTERRSTAT		0x0c
#define	LPC_DMAC_INTERRCLEAR		0x10
#define	LPC_DMAC_RAWINTTCSTAT		0x14
#define	LPC_DMAC_RAWINTERRSTAT		0x18
#define	LPC_DMAC_ENABLED_CHANNELS	0x1c
#define	LPC_DMAC_SOFTBREQ		0x20
#define	LPC_DMAC_SOFTSREQ		0x24
#define	LPC_DMAC_SOFTLBREQ		0x28
#define	LPC_DMAC_SOFTLSREQ		0x2c
#define	LPC_DMAC_CONFIG			0x30
#define	LPC_DMAC_CONFIG_M1		(1 << 2)
#define	LPC_DMAC_CONFIG_M0		(1 << 1)
#define	LPC_DMAC_CONFIG_ENABLE		(1 << 0)
#define	LPC_DMAC_CHADDR(_n)		(0x100 + (_n * 0x20))
#define	LPC_DMAC_CHNUM			8
#define	LPC_DMAC_CHSIZE			0x20
#define	LPC_DMAC_CH_SRCADDR		0x00
#define	LPC_DMAC_CH_DSTADDR		0x04
#define	LPC_DMAC_CH_LLI			0x08
#define	LPC_DMAC_CH_LLI_AHB1		(1 << 0)
#define	LPC_DMAC_CH_CONTROL		0x0c
#define	LPC_DMAC_CH_CONTROL_I		(1 << 31)
#define	LPC_DMAC_CH_CONTROL_DI		(1 << 27)
#define	LPC_DMAC_CH_CONTROL_SI		(1 << 26)
#define	LPC_DMAC_CH_CONTROL_D		(1 << 25)
#define	LPC_DMAC_CH_CONTROL_S		(1 << 24)
#define	LPC_DMAC_CH_CONTROL_WIDTH_4	2
#define	LPC_DMAC_CH_CONTROL_DWIDTH(_n)	((_n & 0x7) << 21)
#define	LPC_DMAC_CH_CONTROL_SWIDTH(_n)	((_n & 0x7) << 18)
#define	LPC_DMAC_CH_CONTROL_BURST_8	2
#define	LPC_DMAC_CH_CONTROL_DBSIZE(_n)	((_n & 0x7) << 15)
#define	LPC_DMAC_CH_CONTROL_SBSIZE(_n)	((_n & 0x7) << 12)
#define	LPC_DMAC_CH_CONTROL_XFERLEN(_n)	(_n & 0xfff) 
#define	LPC_DMAC_CH_CONFIG		0x10
#define	LPC_DMAC_CH_CONFIG_H		(1 << 18)
#define	LPC_DMAC_CH_CONFIG_A		(1 << 17)
#define	LPC_DMAC_CH_CONFIG_L		(1 << 16)
#define	LPC_DMAC_CH_CONFIG_ITC		(1 << 15)
#define	LPC_DMAC_CH_CONFIG_IE		(1 << 14)
#define	LPC_DMAC_CH_CONFIG_FLOWCNTL(_n)	((_n & 0x7) << 11)
#define	LPC_DMAC_CH_CONFIG_DESTP(_n)	((_n & 0x1f) << 6)
#define	LPC_DMAC_CH_CONFIG_SRCP(_n)	((_n & 0x1f) << 1)
#define	LPC_DMAC_CH_CONFIG_E		(1 << 0)

/* DMA flow control values */
#define	LPC_DMAC_FLOW_D_M2M		0
#define	LPC_DMAC_FLOW_D_M2P		1
#define	LPC_DMAC_FLOW_D_P2M		2
#define	LPC_DMAC_FLOW_D_P2P		3
#define	LPC_DMAC_FLOW_DP_P2P		4
#define	LPC_DMAC_FLOW_P_M2P		5
#define	LPC_DMAC_FLOW_P_P2M		6
#define	LPC_DMAC_FLOW_SP_P2P		7

/* DMA peripheral ID's */
#define	LPC_DMAC_I2S0_DMA0_ID		0
#define	LPC_DMAC_NAND_ID		1
#define	LPC_DMAC_IS21_DMA0_ID		2
#define	LPC_DMAC_SSP1_ID		3
#define	LPC_DMAC_SPI2_ID		3
#define	LPC_DMAC_SD_ID			4
#define	LPC_DMAC_UART1_TX_ID		5
#define	LPC_DMAC_UART1_RX_ID		6
#define	LPC_DMAC_UART2_TX_ID		7
#define	LPC_DMAC_UART2_RX_ID		8
#define	LPC_DMAC_UART7_TX_ID		9
#define	LPC_DMAC_UART7_RX_ID		10
#define	LPC_DMAC_I2S1_DMA1_ID		10
#define	LPC_DMAC_SPI1_ID		11
#define	LPC_DMAC_SSP1_TX_ID		11
#define	LPC_DMAC_NAND2_ID		12
#define	LPC_DMAC_I2S0_DMA1_ID		13
#define	LPC_DMAC_SSP0_RX		14
#define	LPC_DMAC_SSP0_TX		15

#endif	/* _ARM_LPC_LPCREG_H */
