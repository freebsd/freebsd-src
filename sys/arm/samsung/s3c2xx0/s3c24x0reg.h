/* $NetBSD: s3c24x0reg.h,v 1.7 2004/02/12 03:52:46 bsh Exp $ */

/*-
 * Copyright (c) 2003  Genetec corporation  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */


/*
 * Samsung S3C2410X/2400 processor is ARM920T based integrated CPU
 *
 * Reference:
 *  S3C2410X User's Manual
 *  S3C2400 User's Manual
 */
#ifndef _ARM_S3C2XX0_S3C24X0REG_H_
#define	_ARM_S3C2XX0_S3C24X0REG_H_

/* common definitions for S3C2800, S3C2410 and S3C2440 */
#include <arm/samsung/s3c2xx0/s3c2xx0reg.h>

/*
 * Map the device registers into kernel space.
 *
 * As most devices use less than 1 page of memory reduce
 * the distance between allocations by right shifting
 * S3C24X0_DEV_SHIFT bits. Because the UART takes 3*0x4000
 * bytes the upper limit on S3C24X0_DEV_SHIFT is 4.
 * TODO: Fix the UART code so we can increase this value.
 */
#define	S3C24X0_DEV_START	0x48000000
#define	S3C24X0_DEV_STOP	0x60000000
#define	S3C24X0_DEV_VA_OFFSET	0xD8000000
#define	S3C24X0_DEV_SHIFT	4
#define	S3C24X0_DEV_PA_SIZE	(S3C24X0_DEV_STOP - S3C24X0_DEV_START)
#define	S3C24X0_DEV_VA_SIZE	(S3C24X0_DEV_PA_SIZE >> S3C24X0_DEV_SHIFT)
#define	S3C24X0_DEV_PA_TO_VA(x)	((x >> S3C24X0_DEV_SHIFT) - S3C24X0_DEV_START + S3C24X0_DEV_VA_OFFSET)

/*
 * Physical address of integrated peripherals
 */
#define	S3C24X0_MEMCTL_PA_BASE	0x48000000 /* memory controller */
#define	S3C24X0_MEMCTL_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_MEMCTL_PA_BASE)
#define	S3C24X0_USBHC_PA_BASE 	0x49000000 /* USB Host controller */
#define	S3C24X0_USBHC_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_USBHC_PA_BASE)
#define	S3C24X0_INTCTL_PA_BASE	0x4a000000 /* Interrupt controller */
#define	S3C24X0_INTCTL_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_INTCTL_PA_BASE)
#define	S3C24X0_INTCTL_SIZE	0x20
#define	S3C24X0_DMAC_PA_BASE	0x4b000000
#define	S3C24X0_DMAC_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_DMAC_PA_BASE)
#define	S3C24X0_DMAC_SIZE 	0xe4
#define	S3C24X0_CLKMAN_PA_BASE	0x4c000000 /* clock & power management */
#define	S3C24X0_CLKMAN_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_CLKMAN_PA_BASE)
#define	S3C24X0_LCDC_PA_BASE 	0x4d000000 /* LCD controller */
#define	S3C24X0_LCDC_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_LCDC_PA_BASE)
#define	S3C24X0_LCDC_SIZE	0x64
#define	S3C24X0_NANDFC_PA_BASE	0x4e000000 /* NAND Flash controller */
#define	S3C24X0_NANDFC_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_NANDFC_PA_BASE)
#define	S3C24X0_UART0_PA_BASE	0x50000000
#define	S3C24X0_UART0_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_UART0_PA_BASE)
#define	S3C24X0_UART_PA_BASE(n)	(S3C24X0_UART0_PA_BASE+0x4000*(n))
#define	S3C24X0_UART_BASE(n)	(S3C24X0_UART0_BASE+0x4000*(n))
#define	S3C24X0_TIMER_PA_BASE 	0x51000000
#define	S3C24X0_TIMER_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_TIMER_PA_BASE)
#define	S3C24X0_USBDC_PA_BASE 	0x5200140
#define	S3C24X0_USBDC_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_USBDC_PA_BASE)
#define	S3C24X0_USBDC_SIZE 	0x130
#define	S3C24X0_WDT_PA_BASE 	0x53000000
#define	S3C24X0_WDT_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_WDT_PA_BASE)
#define	S3C24X0_IIC_PA_BASE 	0x54000000
#define	S3C24X0_IIC_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_IIC_PA_BASE)
#define	S3C24X0_IIS_PA_BASE 	0x55000000
#define	S3C24X0_IIS_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_IIS_PA_BASE)
#define	S3C24X0_GPIO_PA_BASE	0x56000000
#define	S3C24X0_GPIO_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_GPIO_PA_BASE)
#define	S3C24X0_RTC_PA_BASE	0x57000000
#define	S3C24X0_RTC_BASE	S3C24X0_DEV_PA_TO_VA(S3C24X0_RTC_PA_BASE)
#define	S3C24X0_RTC_SIZE	0x8C
#define	S3C24X0_ADC_PA_BASE 	0x58000000
#define	S3C24X0_ADC_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_ADC_PA_BASE)
#define	S3C24X0_SPI0_PA_BASE 	0x59000000
#define	S3C24X0_SPI0_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_SPI0_PA_BASE)
#define	S3C24X0_SPI1_PA_BASE 	0x59000020
#define	S3C24X0_SPI1_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_SPI1_PA_BASE)
#define	S3C24X0_SDI_PA_BASE 	0x5a000000 /* SD Interface */
#define	S3C24X0_SDI_BASE 	S3C24X0_DEV_PA_TO_VA(S3C24X0_SDI_PA_BASE)

#define	S3C24X0_REG_BASE	0x48000000
#define	S3C24X0_REG_SIZE	0x13000000

/* Memory controller */
#define	MEMCTL_BWSCON   	0x00	/* Bus width and wait status */
#define	 BWSCON_DW0_SHIFT	1 	/* bank0 is odd */
#define	 BWSCON_BANK_SHIFT(n)	(4*(n))	/* for bank 1..7 */
#define	 BWSCON_DW_MASK 	0x03
#define	 BWSCON_DW_8 		0
#define	 BWSCON_DW_16 		1
#define	 BWSCON_DW_32 		2
#define	 BWSCON_WS		0x04	/* WAIT enable for the bank */
#define	 BWSCON_ST		0x08	/* SRAM use UB/LB for the bank */

#define	MEMCTL_BANKCON0 	0x04	/* Boot ROM control */
#define	MEMCTL_BANKCON(n)	(0x04+4*(n)) /* BANKn control */
#define	 BANKCON_MT_SHIFT 	15
#define	 BANKCON_MT_ROM 	(0<<BANKCON_MT_SHIFT)
#define	 BANKCON_MT_DRAM 	(3<<BANKCON_MT_SHIFT)
#define	 BANKCON_TACS_SHIFT 	13	/* address set-up time to nGCS */
#define	 BANKCON_TCOS_SHIFT 	11	/* CS set-up to nOE */
#define	 BANKCON_TACC_SHIFT 	8	/* CS set-up to nOE */
#define	 BANKCON_TOCH_SHIFT 	6	/* CS hold time from OE */
#define	 BANKCON_TCAH_SHIFT 	4	/* address hold time from OE */
#define	 BANKCON_TACP_SHIFT 	2	/* page mode access cycle */
#define	 BANKCON_TACP_2 	(0<<BANKCON_TACP_SHIFT)
#define	 BANKCON_TACP_3  	(1<<BANKCON_TACP_SHIFT)
#define	 BANKCON_TACP_4  	(2<<BANKCON_TACP_SHIFT)
#define	 BANKCON_TACP_6  	(3<<BANKCON_TACP_SHIFT)
#define	 BANKCON_PMC_4   	(1<<0)
#define	 BANKCON_PMC_8   	(2<<0)
#define	 BANKCON_PMC_16   	(3<<0)
#define	 BANKCON_TRCD_SHIFT 	2	/* RAS to CAS delay */
#define	 BANKCON_TRCD_2  	(0<<2)
#define	 BANKCON_TRCD_3  	(1<<2)
#define	 BANKCON_TRCD_4  	(2<<2)
#define	 BANKCON_SCAN_8 	(0<<0)	/* Column address number */
#define	 BANKCON_SCAN_9 	(1<<0)
#define	 BANKCON_SCAN_10 	(2<<0)
#define	MEMCTL_REFRESH   	0x24	/* DRAM?SDRAM Refresh */
#define	 REFRESH_REFEN 		(1<<23)
#define	 REFRESH_TREFMD  	(1<<22)	/* 1=self refresh */
#define	 REFRESH_TRP_2 		(0<<20)
#define	 REFRESH_TRP_3 		(1<<20)
#define	 REFRESH_TRP_4 		(2<<20)
#define	 REFRESH_TRC_4 		(0<<18)
#define	 REFRESH_TRC_5 		(1<<18)
#define	 REFRESH_TRC_6 		(2<<18)
#define	 REFRESH_TRC_7 		(3<<18)
#define	 REFRESH_COUNTER_MASK	0x3ff
#define	MEMCTL_BANKSIZE 	0x28 	/* Flexible Bank size */
#define	MEMCTL_MRSRB6    	0x2c	/* SDRAM Mode register */
#define	MEMCTL_MRSRB7    	0x30
#define	 MRSR_CL_SHIFT		4	/* CAS Latency */

#define	S3C24X0_MEMCTL_SIZE	0x34

/* USB Host controller */
#define	S3C24X0_USBHC_SIZE	0x5c

/* Interrupt controller */
#define	INTCTL_PRIORITY 	0x0c	/* IRQ Priority control */
#define	INTCTL_INTPND   	0x10	/* Interrupt request status */
#define	INTCTL_INTOFFSET	0x14	/* Interrupt request source */
#define	INTCTL_SUBSRCPND 	0x18	/* sub source pending */
#define	INTCTL_INTSUBMSK  	0x1c	/* sub mask */

/* Interrupt source */
#define	S3C24X0_INT_ADCTC 	31	/* ADC (and TC for 2410) */
#define	S3C24X0_INT_RTC  	30	/* RTC alarm */
#define	S3C24X0_INT_SPI1	29	/* SPI 1 */
#define	S3C24X0_INT_UART0	28	/* UART0 */
#define	S3C24X0_INT_IIC  	27
#define	S3C24X0_INT_USBH	26	/* USB Host */
#define	S3C24X0_INT_USBD	25	/* USB Device */
#define	S3C24X0_INT_UART1	23	/* UART0  (2410 only) */
#define	S3C24X0_INT_SPI0  	22	/* SPI 0 */
#define	S3C24X0_INT_SDI 	21
#define	S3C24X0_INT_DMA3	20
#define	S3C24X0_INT_DMA2	19
#define	S3C24X0_INT_DMA1	18
#define	S3C24X0_INT_DMA0	17
#define	S3C24X0_INT_LCD 	16

#define	S3C24X0_INT_UART2 	15	/* UART2 int (2410) */
#define	S3C24X0_INT_TIMER4	14
#define	S3C24X0_INT_TIMER3	13
#define	S3C24X0_INT_TIMER2	12
#define	S3C24X0_INT_TIMER1	11
#define	S3C24X0_INT_TIMER0	10
#define	S3C24X0_INT_TIMER(n)	(10+(n)) /* timer interrupt [4:0] */
#define	S3C24X0_INT_WDT 	9	/* Watch dog timer */
#define	S3C24X0_INT_TICK 	8
#define	S3C24X0_INT_BFLT 	7	/* Battery fault */
#define	S3C24X0_INT_8_23	5	/* Ext int 8..23 */
#define	S3C24X0_INT_4_7 	4	/* Ext int 4..7 */
#define	S3C24X0_INT_3		3
#define	S3C24X0_INT_2		2
#define	S3C24X0_INT_1		1
#define	S3C24X0_INT_0		0

/* 24{1,4}0 has more than 32 interrupt sources.  These are sub-sources
 * that are OR-ed into main interrupt sources, and controlled via
 * SUBSRCPND and  SUBSRCMSK registers */
#define	S3C24X0_SUBIRQ_MIN	32

/* cascaded to INT_ADCTC */
#define	S3C24X0_INT_ADC		(S3C24X0_SUBIRQ_MIN+10)	/* AD converter */
#define	S3C24X0_INT_TC 		(S3C24X0_SUBIRQ_MIN+9)	/* Touch screen */
/* cascaded to INT_UART2 */
#define	S3C24X0_INT_ERR2	(S3C24X0_SUBIRQ_MIN+8)	/* UART2 Error */
#define	S3C24X0_INT_TXD2	(S3C24X0_SUBIRQ_MIN+7)	/* UART2 Tx */
#define	S3C24X0_INT_RXD2	(S3C24X0_SUBIRQ_MIN+6)	/* UART2 Rx */
/* cascaded to INT_UART1 */
#define	S3C24X0_INT_ERR1	(S3C24X0_SUBIRQ_MIN+5)	/* UART1 Error */
#define	S3C24X0_INT_TXD1	(S3C24X0_SUBIRQ_MIN+4)	/* UART1 Tx */
#define	S3C24X0_INT_RXD1	(S3C24X0_SUBIRQ_MIN+3)	/* UART1 Rx */
/* cascaded to INT_UART0 */
#define	S3C24X0_INT_ERR0	(S3C24X0_SUBIRQ_MIN+2)	/* UART0 Error */
#define	S3C24X0_INT_TXD0	(S3C24X0_SUBIRQ_MIN+1)	/* UART0 Tx */
#define	S3C24X0_INT_RXD0	(S3C24X0_SUBIRQ_MIN+0)	/* UART0 Rx */

/*
 * Support for external interrupts. We use values from 48
 * to allow new CPU's to allocate new subirq's.
 */
#define	S3C24X0_EXTIRQ_MIN	48
#define	S3C24X0_EXTIRQ_COUNT	24
#define	S3C24X0_EXTIRQ_MAX	(S3C24X0_EXTIRQ_MIN + S3C24X0_EXTIRQ_COUNT - 1)
#define	S3C24X0_INT_EXT(n)	(S3C24X0_EXTIRQ_MIN + (n))

/* DMA controller */
/* XXX */

/* Clock & power manager */
#define	CLKMAN_LOCKTIME 0x00	/* PLL lock time */
#define	CLKMAN_MPLLCON 	0x04	/* MPLL control */
#define	CLKMAN_UPLLCON 	0x08	/* UPLL control */
#define	 PLLCON_MDIV_SHIFT	12
#define	 PLLCON_MDIV_MASK	(0xff<<PLLCON_MDIV_SHIFT)
#define	 PLLCON_PDIV_SHIFT	4
#define	 PLLCON_PDIV_MASK	(0x3f<<PLLCON_PDIV_SHIFT)
#define	 PLLCON_SDIV_SHIFT	0
#define	 PLLCON_SDIV_MASK	(0x03<<PLLCON_SDIV_SHIFT)
#define	CLKMAN_CLKCON	0x0c
#define	 CLKCON_SPI 	(1<<18)
#define	 CLKCON_IIS 	(1<<17)
#define	 CLKCON_IIC 	(1<<16)
#define	 CLKCON_ADC 	(1<<15)
#define	 CLKCON_RTC 	(1<<14)
#define	 CLKCON_GPIO 	(1<<13)
#define	 CLKCON_UART2 	(1<<12)
#define	 CLKCON_UART1 	(1<<11)
#define	 CLKCON_UART0	(1<<10)	/* PCLK to UART0 */
#define	 CLKCON_SDI	(1<<9)
#define	 CLKCON_TIMER	(1<<8)	/* PCLK to TIMER */
#define	 CLKCON_USBD	(1<<7)	/* PCLK to USB device controller */
#define	 CLKCON_USBH	(1<<6)	/* PCLK to USB host controller */
#define	 CLKCON_LCDC	(1<<5)	/* PCLK to LCD controller */
#define	 CLKCON_NANDFC	(1<<4)	/* PCLK to NAND Flash controller */
#define	 CLKCON_IDLE	(1<<2)	/* 1=transition to IDLE mode */
#define	CLKMAN_CLKSLOW	0x10
#define	CLKMAN_CLKDIVN	0x14
#define	 CLKDIVN_PDIVN	(1<<0)	/* pclk=hclk/2 */

#define	CLKMAN_CLKSLOW	0x10	/* slow clock controll */
#define	 CLKSLOW_UCLK 	(1<<7)	/* 1=UPLL off */
#define	 CLKSLOW_MPLL 	(1<<5)	/* 1=PLL off */
#define	 CLKSLOW_SLOW	(1<<4)	/* 1: Enable SLOW mode */
#define	 CLKSLOW_VAL_MASK  0x0f	/* divider value for slow clock */

#define	CLKMAN_CLKDIVN	0x14	/* Software reset control */
#define	 CLKDIVN_PDIVN	(1<<0)

#define	S3C24X0_CLKMAN_SIZE	0x18

/* LCD controller */
#define	LCDC_LCDCON1	0x00	/* control 1 */
#define	 LCDCON1_ENVID   	(1<<0)	/* enable video */
#define	 LCDCON1_BPPMODE_SHIFT 	1
#define	 LCDCON1_BPPMODE_MASK	(0x0f<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN1	(0x0<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN2	(0x1<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN4	(0x2<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN8	(0x3<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN12	(0x4<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT1	(0x8<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT2	(0x9<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT4	(0xa<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT8	(0xb<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT16	(0xc<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT24	(0xd<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFTX	(0x8<<LCDCON1_BPPMODE_SHIFT)

#define	 LCDCON1_PNRMODE_SHIFT	5
#define	 LCDCON1_PNRMODE_MASK	(0x3<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_DUALSTN4    (0x0<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_SINGLESTN4  (0x1<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_SINGLESTN8  (0x2<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_TFT         (0x3<<LCDCON1_PNRMODE_SHIFT)

#define	 LCDCON1_MMODE  	(1<<7) /* VM toggle rate */
#define	 LCDCON1_CLKVAL_SHIFT 	8
#define	 LCDCON1_CLKVAL_MASK	(0x3ff<<LCDCON1_CLKVAL_SHIFT)
#define	 LCDCON1_LINCNT_SHIFT 	18
#define	 LCDCON1_LINCNT_MASK	(0x3ff<<LCDCON1_LINCNT_SHIFT)

#define	LCDC_LCDCON2	0x04	/* control 2 */
#define	 LCDCON2_VPSW_SHIFT	0 	/* TFT Vsync pulse width */
#define	 LCDCON2_VPSW_MASK	(0x3f<<LCDCON2_VPSW_SHIFT)
#define	 LCDCON2_VFPD_SHIFT	6 	/* TFT V front porch */
#define	 LCDCON2_VFPD_MASK	(0xff<<LCDCON2_VFPD_SHIFT)
#define	 LCDCON2_LINEVAL_SHIFT	14 	/* Vertical size */
#define	 LCDCON2_LINEVAL_MASK	(0x3ff<<LCDCON2_LINEVAL_SHIFT)
#define	 LCDCON2_VBPD_SHIFT	24 	/* TFT V back porch */
#define	 LCDCON2_VBPD_MASK	(0xff<<LCDCON2_VBPD_SHIFT)

#define	LCDC_LCDCON3	0x08	/* control 2 */
#define	 LCDCON3_HFPD_SHIFT	0 	/* TFT H front porch */
#define	 LCDCON3_HFPD_MASK	(0xff<<LCDCON3_VPFD_SHIFT)
#define	 LCDCON3_LINEBLANK_SHIFT  0 	/* STN H blank time */
#define	 LCDCON3_LINEBLANK_MASK	  (0xff<<LCDCON3_LINEBLANK_SHIFT)
#define	 LCDCON3_HOZVAL_SHIFT	8 	/* Horizontal size */
#define	 LCDCON3_HOZVAL_MASK	(0x7ff<<LCDCON3_HOZVAL_SHIFT)
#define	 LCDCON3_HBPD_SHIFT	19 	/* TFT H back porch */
#define	 LCDCON3_HBPD_MASK	(0x7f<<LCDCON3_HPBD_SHIFT)
#define	 LCDCON3_WDLY_SHIFT	19	/* STN vline delay */
#define	 LCDCON3_WDLY_MASK	(0x03<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_16	(0x00<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_32	(0x01<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_64	(0x02<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_128	(0x03<<LCDCON3_WDLY_SHIFT)

#define	LCDC_LCDCON4	0x0c	/* control 4 */
#define	 LCDCON4_HPSW_SHIFT	0 	/* TFT Hsync pulse width */
#define	 LCDCON4_HPSW_MASK	(0xff<<LCDCON4_HPSW_SHIFT)
#define	 LCDCON4_WLH_SHIFT	0	/* STN VLINE high width */
#define	 LCDCON4_WLH_MASK	(0x03<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_16 	(0x00<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_32  	(0x01<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_64  	(0x02<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_128	(0x03<<LCDCON4_WLH_SHIFT)

#define	 LCDCON4_MVAL_SHIFT	8	/* STN VM toggle rate */
#define	 LCDCON4_MVAL_MASK	(0xff<<LCDCON4_MVAL_SHIFT)

#define	LCDC_LCDCON5	0x10	/* control 5 */
#define	 LCDCON5_HWSWP		(1<<0)	/* half-word swap */
#define	 LCDCON5_BSWP 		(1<<1)	/* byte swap */
#define	 LCDCON5_ENLEND		(1<<2)	/* TFT: enable LEND signal */
#define	 LCDCON5_PWREN		(1<<3)	/* enable PWREN signale */
#define	 LCDCON5_INVLEND	(1<<4)	/* TFT: LEND signal polarity */
#define	 LCDCON5_INVPWREN	(1<<5)	/* PWREN signal polarity */
#define	 LCDCON5_INVVDEN	(1<<6)	/* VDEN signal polarity */
#define	 LCDCON5_INVVD		(1<<7)	/* video data signal polarity */
#define	 LCDCON5_INVVFRAME	(1<<8)	/* VFRAME/VSYNC signal polarity */
#define	 LCDCON5_INVVLINE	(1<<9)	/* VLINE/HSYNC signal polarity */
#define	 LCDCON5_INVVCLK	(1<<10)	/* VCLK signal polarity */
#define	 LCDCON5_INVVCLK_RISING	LCDCON5_INVVCLK
#define	 LCDCON5_INVVCLK_FALLING  0
#define	 LCDCON5_FRM565  	(1<<11)	/* RGB:565 format*/
#define	 LCDCON5_FRM555I	0	/* RGBI:5551 format */
#define	 LCDCON5_BPP24BL	(1<<12)	/* bit order for bpp24 */

#define	 LCDCON5_HSTATUS_SHIFT	17 /* TFT: horizontal status */
#define	 LCDCON5_HSTATUS_MASK	(0x03<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_HSYNC	(0x00<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_BACKP	(0x01<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_ACTIVE	(0x02<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_FRONTP	(0x03<<LCDCON5_HSTATUS_SHIFT)

#define	 LCDCON5_VSTATUS_SHIFT	19 /* TFT: vertical status */
#define	 LCDCON5_VSTATUS_MASK	(0x03<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_HSYNC	(0x00<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_BACKP	(0x01<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_ACTIVE	(0x02<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_FRONTP	(0x03<<LCDCON5_VSTATUS_SHIFT)

#define	LCDC_LCDSADDR1	0x14	/* frame buffer start address */
#define	LCDC_LCDSADDR2	0x18
#define	LCDC_LCDSADDR3	0x1c
#define	 LCDSADDR3_OFFSIZE_SHIFT     11
#define	 LCDSADDR3_PAGEWIDTH_SHIFT   0

#define	LCDC_REDLUT	0x20	/* STN: red lookup table */
#define	LCDC_GREENLUT	0x24	/* STN: green lookup table */
#define	LCDC_BLUELUT	0x28	/* STN: blue lookup table */
#define	LCDC_DITHMODE	0x4c	/* STN: dithering mode */

#define	LCDC_TPAL	0x50	/* TFT: temporary palette */
#define	 TPAL_TPALEN		(1<<24)
#define	 TPAL_RED_SHIFT  	16
#define	 TPAL_GREEN_SHIFT	8
#define	 TPAL_BLUE_SHIFT 	0

#define	LCDC_LCDINTPND	0x54
#define	LCDC_LCDSRCPND	0x58
#define	LCDC_LCDINTMSK	0x5c
#define	 LCDINT_FICNT	(1<<0)	/* FIFO trigger interrupt pending */
#define	 LCDINT_FRSYN	(1<<1)	/* frame sync interrupt pending */
#define	 LCDINT_FIWSEL	(1<<2)	/* FIFO trigger level: 1=8 words, 0=4 words*/

#define	LCDC_LPCSEL	0x60	/* LPC3600 mode  */
#define	 LPCSEL_LPC_EN		(1<<0)	/* enable LPC3600 mode */
#define	 LPCSEL_RES_SEL		(1<<1)	/* 1=240x320 0=320x240 */
#define	 LPCSEL_MODE_SEL	(1<<2)
#define	 LPCSEL_CPV_SEL		(1<<3)


#define	LCDC_PALETTE		0x0400
#define	LCDC_PALETTE_SIZE	0x0400

/* NAND Flash controller */
#define	NANDFC_NFCONF	0x00	/* Configuration */
/* NANDFC_NFSTAT */
#define	 NFSTAT_READY	(1<<0)	/* NAND flash memory ready/busy status */


/* MMC/SD */
#define	SDI_CON		0x00
#define	 CON_BYTEORDER		(1<<4)
#define	 CON_SDIO_INTR		(1<<3)
#define	 CON_READWAIT_EN	(1<<2)
#define	 CON_CLOCK_EN		(1<<0)
#define	SDI_PRE		0x04
#define	SDI_CARG	0x08
#define	SDI_CCON	0x0c
#define	 CCON_ABORDCMD		(1<<12) /* Abort SDIO CMD12/52 */
#define	 CCON_WITHDATA  	(1<<11) /* CMD with data */
#define	 CCON_LONGRSP		(1<<10) /* 136 bit response */
#define	 CCON_WAITRSP		(1<<9)  /* Host waits for response */
#define	 CCON_CMD_START		(1<<8)
#define	 CCON_CMDINDEX_MASK	(0x7F) /* Command number index */
#define	SDI_CSTA	0x10
#define	 CSTA_RSPCRCFAIL	(1<<12)
#define	 CSTA_CMDSENT		(1<<11)
#define	 CSTA_CMDTOUT		(1<<10)
#define	 CSTA_RSPFIN		(1<<9)
/* All the bits to be cleared */
#define	 CSTA_ALL_CLEAR		(CSTA_RSPCRCFAIL | CSTA_CMDSENT | \
				 CSTA_CMDTOUT | CSTA_RSPFIN)
#define	 CSTA_ERROR		(CSTA_RSPCRCFAIL | CSTA_CMDTOUT)
#define	 CSTA_CMDON		(1<<8)
#define	SDI_RSP0	0x14
#define	SDI_RSP1	0x18
#define	SDI_RSP2	0x1c
#define	SDI_RSP3	0x20
#define	SDI_DTIMER	0x24
#define	SDI_BSIZE	0x28
#define	SDI_DCON	0x2c
#define	 DCON_PRDTYPE		(1<<21)
#define	 DCON_TARSP		(1<<20) /* Transmit after response */
#define	 DCON_RACMD		(1<<19) /* Receive after command */
#define	 DCON_BACMD		(1<<18) /* Busy after command */
#define	 DCON_BLKMODE		(1<<17) /* Stream/Block mode */
#define	 DCON_WIDEBUS		(1<<16) /* Standard/Wide bus */
#define	 DCON_ENDMA		(1<<15) /* DMA Enable */
/* Determine the direction of the data transfer */
#define	 DCON_DATA_READY	(0<<12) /* No transfer */
#define	 DCON_ONLYBUST		(1<<12) /* Check if busy */
#define	 DCON_DATA_RECEIVE	(2<<12) /* Receive data from SD */
#define	 DCON_DATA_TRANSMIT	(3<<12) /* Send data to SD */
#define	 DCON_BLKNUM_MASK	(0x7FF) /* Block number */
#define	SDI_DCNT	0x30
#define	SDI_DSTA	0x34
#define	SDI_FSTA	0x38
#define	 FSTA_TX_AVAIL		(1<<13)
#define	 FSTA_RX_AVAIL		(1<<12)
#define	 FSTA_TX_FIFO_HALF_FULL	(1<<11)
#define	 FSTA_TX_FIFO_EMPTY	(1<<10)
#define	 FSTA_RX_FIFO_LAST_DATA	(1<<9)
#define	 FSTA_RX_FIFO_FULL	(1<<8)
#define	 FSTA_RX_FIFO_HALF_FULL	(1<<7)
#define	 FSTA_FIFO_COUNT_MSK	(0x7F)

/* Timer */
#define	TIMER_TCFG0 	0x00	/* Timer configuration */
#define	TIMER_TCFG1	0x04
#define	 TCFG1_MUX_SHIFT(n)	(4*(n))
#define	 TCFG1_MUX_MASK(n)	(0x0f << TCFG1_MUX_SHIFT(n))
#define	 TCFG1_MUX_DIV2		0
#define	 TCFG1_MUX_DIV4		1
#define	 TCFG1_MUX_DIV8		2
#define	 TCFG1_MUX_DIV16	3
#define	 TCFG1_MUX_EXT 		4
#define	TIMER_TCON 	0x08	/* control */
#define	 TCON_SHIFT(n)		(4 * ((n)==0 ? 0 : (n)+1))
#define	 TCON_START(n)		(1 << TCON_SHIFT(n))
#define	 TCON_MANUALUPDATE(n)	(1 << (TCON_SHIFT(n) + 1))
#define	 TCON_INVERTER(n)	(1 << (TCON_SHIFT(n) + 2))
#define	 __TCON_AUTORELOAD(n)	(1 << (TCON_SHIFT(n) + 3)) /* n=0..3 */
#define	 TCON_AUTORELOAD4 	(1<<22)	       /* stupid hardware design */
#define	 TCON_AUTORELOAD(n)	\
	((n)==4 ? TCON_AUTORELOAD4 : __TCON_AUTORELOAD(n))
#define	 TCON_MASK(n)		(0x0f << TCON_SHIFT(n))
#define	TIMER_TCNTB(n) 	 (0x0c+0x0c*(n))	/* count buffer */
#define	TIMER_TCMPB(n)	 (0x10+0x0c*(n))	/* compare buffer */
#define	__TIMER_TCNTO(n) (0x14+0x0c*(n))	/* count observation */
#define	TIMER_TCNTO4	0x40
#define	TIMER_TCNTO(n)	((n)==4 ? TIMER_TCNTO4 : __TIMER_TCNTO(n))

#define	S3C24X0_TIMER_SIZE	0x44

/* UART */
/* diffs to s3c2800 */
/* SSCOM_UMCON */
#define	 UMCON_AFC	(1<<4)	/* auto flow control */
/* SSCOM_UMSTAT */
#define	 UMSTAT_DCTS	(1<<2)	/* CTS change */
/* SSCOM_UMSTAT */
#define	 ULCON_IR  	(1<<6)
#define	 ULCON_PARITY_SHIFT  3

#define	S3C24X0_UART_SIZE 	0x2c

/* USB device */
/* XXX */

/* Watch dog timer */
#define	WDT_WTCON 	0x00	/* WDT mode */
#define	 WTCON_PRESCALE_SHIFT	8
#define	 WTCON_PRESCALE	(0xff<<WTCON_PRESCALE_SHIFT)
#define	 WTCON_ENABLE   (1<<5)
#define	 WTCON_CLKSEL	(3<<3)
#define	 WTCON_CLKSEL_16  (0<<3)
#define	 WTCON_CLKSEL_32  (1<<3)
#define	 WTCON_CLKSEL_64  (2<<3)
#define	 WTCON_CLKSEL_128 (3<<3)
#define	 WTCON_ENINT    (1<<2)
#define	 WTCON_ENRST	(1<<0)

#define	 WTCON_WDTSTOP	0
	
#define	WDT_WTDAT 	0x04	/* timer data */
#define	WDT_WTCNT 	0x08	/* timer count */

#define	S3C24X0_WDT_SIZE 	0x0c

/* IIC */
#define	S3C24X0_IIC_SIZE 	0x0c


/* IIS */
#define	S3C24X0_IIS_SIZE 	0x14

/* GPIO */
#define	GPIO_PACON	0x00	/* port A configuration */
#define	GPIO_PADAT	0x04	/* port A data */

#define	GPIO_PBCON	0x10
/* These are only used on port B-H on 2410 & B-H,J on 2440 */
#define	 PCON_INPUT	0	/* Input port */
#define	 PCON_OUTPUT	1	/* Output port */
#define	 PCON_ALTFUN	2	/* Alternate function */
#define	 PCON_ALTFUN2	3	/* Alternate function */
#define	GPIO_PBDAT	0x14
/* This is different between 2440 and 2442 (pull up vs pull down): */
#define	GPIO_PBUP 	0x18	/* 2410 & 2440 */
#define	GPIO_PBDOWN	0x18	/* 2442 */

#define	GPIO_PCCON	0x20
#define	GPIO_PCDAT	0x24
#define	GPIO_PCUP	0x28	/* 2410 & 2440 */
#define	GPIO_PCDOWN	0x28	/* 2442 */

#define	GPIO_PDCON	0x30
#define	GPIO_PDDAT	0x34
#define	GPIO_PDUP	0x38	/* 2410 & 2440 */
#define	GPIO_PDDOWN	0x38	/* 2442 */

#define	GPIO_PECON	0x40
#define	 PECON_INPUT(x)		(0<<((x)*2)) /* Pin is used for input */
#define	 PECON_OUTPUT(x)	(1<<((x)*2)) /* Pin is used for output */
#define	 PECON_FUNC_A(x)	(2<<((x)*2)) /* Pin is used for function 'A' */
#define	 PECON_FUNC_B(x)	(3<<((x)*2)) /* Pin is used for function 'B' */
#define	 PECON_MASK(x)		(3<<((x)*2))
#define	GPIO_PEDAT	0x44
#define	GPIO_PEUP	0x48	/* 2410 & 2440 */
#define	GPIO_PEDOWN	0x48	/* 2442 */
#define	 PEUD_ENABLE(x)		(~(1<<(x))) /* Enable the pull Up/Down */
#define	 PEUD_DISABLE(x)	(1<<(x)) /* Disable the pull Up/Down */

#define	GPIO_PFCON	0x50
#define	GPIO_PFDAT	0x54
#define	GPIO_PFUP	0x58	/* 2410 & 2440 */
#define	GPIO_PFDOWN	0x58	/* 2442 */

#define	GPIO_PGCON	0x60
#define	GPIO_PGDAT	0x64
#define	GPIO_PGUP	0x68	/* 2410 & 2440 */
#define	GPIO_PGDOWN	0x68	/* 2442 */

#define	GPIO_PHCON	0x70
#define	GPIO_PHDAT	0x74
#define	GPIO_PHUP	0x78	/* 2410 & 2440 */
#define	GPIO_PHDOWN	0x78	/* 2442 */

#define	GPIO_MISCCR 	0x80	/* miscellaneous control */
#define	GPIO_DCLKCON 	0x84	/* DCLK 0/1 */
#define	GPIO_EXTINT(n)	(0x88+4*(n))	/* external int control 0/1/2 */
#define	GPIO_EINTFLT(n)	(0x94+4*(n))	/* external int filter control 0..3 */
#define	 EXTINTR_LOW	 0x00
#define	 EXTINTR_HIGH	 0x01
#define	 EXTINTR_FALLING 0x02
#define	 EXTINTR_RISING  0x04
#define	 EXTINTR_BOTH    0x06
#define	GPIO_EINTMASK	0xa4
#define	GPIO_EINTPEND	0xa8
#define	GPIO_GSTATUS0	0xac	/* external pin status */
#define	GPIO_GSTATUS1	0xb0	/* Chip ID */
#define	 CHIPID_S3C2410A	0x32410002
#define	 CHIPID_S3C2440A	0x32440001
#define	 CHIPID_S3C2442B	0x32440AAB
#define	GPIO_GSTATUS2	0xb4	/* Reset status */
#define	GPIO_GSTATUS3	0xb8
#define	GPIO_GSTATUS4	0xbc

#define	GPIO_SET_FUNC(v,port,func)	\
		(((v) & ~(3<<(2*(port))))|((func)<<(2*(port))))

/* ADC */
#define	ADC_ADCCON	0x00
#define	 ADCCON_ENABLE_START	(1<<0)
#define	 ADCCON_READ_START	(1<<1)
#define	 ADCCON_STDBM    	(1<<2)
#define	 ADCCON_SEL_MUX_SHIFT	3
#define	 ADCCON_SEL_MUX_MASK	(0x7<<ADCCON_SEL_MUX_SHIFT)
#define	 ADCCON_PRSCVL_SHIFT	6
#define	 ADCCON_PRSCVL_MASK	(0xff<<ADCCON_PRSCVL_SHIFT)
#define	 ADCCON_PRSCEN  	(1<<14)
#define	 ADCCON_ECFLG   	(1<<15)

#define	ADC_ADCTSC 	0x04
#define	 ADCTSC_XY_PST   	0x03
#define	 ADCTSC_AUTO_PST    	(1<<2)
#define	 ADCTSC_PULL_UP		(1<<3)
#define	 ADCTSC_XP_SEN		(1<<4)
#define	 ADCTSC_XM_SEN		(1<<5)
#define	 ADCTSC_YP_SEN		(1<<6)
#define	 ADCTSC_YM_SEN		(1<<7)
#define	ADC_ADCDLY	0x08
#define	ADC_ADCDAT0	0x0c
#define	ADC_ADCDAT1	0x10

#define	ADCDAT_DATAMASK  	0x3ff

/* RTC */
#define	RTC_RTCCON		0x40
#define	 RTCCON_RTCEN		(1<<0)
#define	 RTCCON_CLKSEL		(1<<1)
#define	 RTCCON_CNTSEL		(1<<2)
#define	 RTCCON_CLKRST		(1<<3)
#define	RTC_TICNT0		0x44
/* TICNT1 on 2440 */
#define	RTC_RTCALM		0x50
#define	RTC_ALMSEC		0x54
#define	RTC_ALMMIN		0x58
#define	RTC_ALMHOUR		0x5C
#define	RTC_ALMDATE		0x60
#define	RTC_ALMMON		0x64
#define	RTC_ALMYEAR		0x68
/* RTCRST on 2410 */
#define	RTC_BCDSEC		0x70
#define	RTC_BCDMIN		0x74
#define	RTC_BCDHOUR		0x78
#define	RTC_BCDDATE		0x7C
#define	RTC_BCDDAY		0x80
#define	RTC_BCDMON		0x84
#define	RTC_BCDYEAR		0x88


/* SPI */
#define	S3C24X0_SPI_SIZE 	0x20

#define	SPI_SPCON		0x00
#define	 SPCON_TAGD		(1<<0) /* Tx auto garbage */
#define	 SPCON_CPHA		(1<<1)
#define	 SPCON_CPOL		(1<<2)
#define	 SPCON_IDLELOW_RISING	  (0|0)
#define	 SPCON_IDLELOW_FALLING	  (0|SPCON_CPHA)
#define	 SPCON_IDLEHIGH_FALLING  (SPCON_CPOL|0)
#define	 SPCON_IDLEHIGH_RISING	  (SPCON_CPOL|SPCON_CPHA)
#define	 SPCON_MSTR		(1<<3)
#define	 SPCON_ENSCK		(1<<4)
#define	 SPCON_SMOD_SHIFT	5
#define	 SPCON_SMOD_MASK	(0x03<<SPCON_SMOD_SHIFT)
#define	 SPCON_SMOD_POLL	(0x00<<SPCON_SMOD_SHIFT)
#define	 SPCON_SMOD_INT 	(0x01<<SPCON_SMOD_SHIFT)
#define	 SPCON_SMOD_DMA 	(0x02<<SPCON_SMOD_SHIFT)

#define	SPI_SPSTA		0x04 /* status register */
#define	 SPSTA_REDY		(1<<0) /* ready */
#define	 SPSTA_MULF		(1<<1) /* multi master error */
#define	 SPSTA_DCOL		(1<<2) /* Data collision error */

#define	SPI_SPPIN		0x08
#define	 SPPIN_KEEP		(1<<0)
#define	 SPPIN_ENMUL		(1<<2) /* multi master error detect */

#define	SPI_SPPRE		0x0c /* prescaler */
#define	SPI_SPTDAT		0x10 /* tx data */
#define	SPI_SPRDAT		0x14 /* rx data */


#endif /* _ARM_S3C2XX0_S3C24X0REG_H_ */
