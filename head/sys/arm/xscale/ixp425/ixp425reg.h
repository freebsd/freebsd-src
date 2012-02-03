/*	$NetBSD: ixp425reg.h,v 1.19 2005/12/11 12:16:51 christos Exp $ */
/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _IXP425REG_H_
#define _IXP425REG_H_

/*
 * Physical memory map for the Intel IXP425
 */
/*
 * CC00 00FF ---------------------------
 *           SDRAM Configuration Registers
 * CC00 0000 ---------------------------
 *
 * C800 BFFF ---------------------------
 *           System and Peripheral Registers
 * C800 0000 ---------------------------
 *           Expansion Bus Configuration Registers
 * C400 0000 ---------------------------
 *           PCI Configuration and Status Registers
 * C000 0000 ---------------------------
 *
 * 6400 0000 ---------------------------
 *           Queue manager
 * 6000 0000 ---------------------------
 *           Expansion Bus Data
 * 5000 0000 ---------------------------
 *           PCI Data
 * 4800 0000 ---------------------------
 *
 * 4000 0000 ---------------------------
 *           SDRAM
 * 0000 0000 ---------------------------
 */           

/*
 * Virtual memory map for the Intel IXP425/IXP435 integrated devices
 */
/*
 * FFFF FFFF ---------------------------
 *
 *           Global cache clean area
 * FF00 0000 ---------------------------
 *
 * FE00 0000 ---------------------------
 *           16M CFI Flash (on ext bus)
 * FD00 0000 ---------------------------
 *
 * FC00 0000 ---------------------------
 *           PCI Data (memory space)
 * F800 0000 --------------------------- IXP425_PCI_MEM_VBASE
 *
 * F020 1000 ---------------------------
 *           SDRAM/DDR Memory Controller
 * F020 0000 --------------------------- IXP425_MCU_VBASE
 *
 * F001 F000 RS485 (Cambria)		 CAMBRIA_RS485_VBASE
 * F001 E000 GPS (Cambria)		 CAMBRIA_GPS_VBASE
 * F001 D000 EHCI USB 2 (IXP435)	 IXP435_USB2_VBASE
 * F001 C000 EHCI USB 1 (IXP435)	 IXP435_USB1_VBASE
 *           Queue manager
 * F001 8000 --------------------------- IXP425_QMGR_VBASE
 *           PCI Configuration and Status
 * F001 7000 --------------------------- IXP425_PCI_VBASE
 *
 *	     (NB: gap for future addition of EXP CS5-7)
 * F001 4000 Expansion Bus Chip Select 4
 * F001 3000 Expansion Bus Chip Select 3
 * F001 2000 Expansion Bus Chip Select 2
 * F001 1000 Expansion Bus Chip Select 1
 *           Expansion Bus Configuration
 * F001 0000 --------------------------- IXP425_EXP_VBASE
 *
 * F000 C000 MAC-A (IXP435)
 * F000 B000 USB (option on IXP425)
 * F000 A000 MAC-B (IXP425) | MAC-C (IXP435)
 * F000 9000 MAC-A (IXP425)
 * F000 8000 NPE-C
 * F000 7000 NPE-B (IXP425)
 * F000 6000 NPE-A
 * F000 5000 Timers
 * F000 4000 GPIO Controller
 * F000 3000 Interrupt Controller
 * F000 2000 Performance Monitor Controller (PMC)
 * F000 1000 UART 1 (IXP425)
 * F000 0000 UART 0
 * F000 0000 --------------------------- IXP425_IO_VBASE
 *
 * 0000 0000 ---------------------------
 *
 */

/* Physical/Virtual address for I/O space */

#define	IXP425_IO_VBASE		0xf0000000UL
#define	IXP425_IO_HWBASE	0xc8000000UL
#define	IXP425_IO_SIZE		0x00010000UL

/* Physical/Virtual addresss offsets */
#define	IXP425_UART0_OFFSET	0x00000000UL
#define	IXP425_UART1_OFFSET	0x00001000UL
#define	IXP425_PMC_OFFSET	0x00002000UL
#define	IXP425_INTR_OFFSET	0x00003000UL
#define	IXP425_GPIO_OFFSET	0x00004000UL
#define	IXP425_TIMER_OFFSET	0x00005000UL
#define	IXP425_NPE_A_OFFSET	0x00006000UL	/* Not User Programmable */
#define	IXP425_NPE_B_OFFSET	0x00007000UL	/* Not User Programmable */
#define	IXP425_NPE_C_OFFSET	0x00008000UL	/* Not User Programmable */
#define	IXP425_MAC_B_OFFSET	0x00009000UL	/* Ethernet MAC on NPE-B */
#define	IXP425_MAC_C_OFFSET	0x0000a000UL	/* Ethernet MAC on NPE-C */
#define	IXP425_USB_OFFSET	0x0000b000UL

#define	IXP435_MAC_A_OFFSET	0x0000c000UL	/* Ethernet MAC on NPE-A */

#define	IXP425_REG_SIZE		0x1000

/*
 * UART
 * 	UART0 0xc8000000
 * 	UART1 0xc8001000
 *
 */
/* I/O space */
#define	IXP425_UART0_HWBASE	(IXP425_IO_HWBASE + IXP425_UART0_OFFSET)
#define	IXP425_UART1_HWBASE	(IXP425_IO_HWBASE + IXP425_UART1_OFFSET)

#define	IXP425_UART0_VBASE	(IXP425_IO_VBASE + IXP425_UART0_OFFSET)
						/* 0xf0000000 */
#define	IXP425_UART1_VBASE	(IXP425_IO_VBASE + IXP425_UART1_OFFSET)
						/* 0xf0001000 */

#define	IXP425_UART_FREQ	14745600

#define	IXP425_UART_IER		0x01	/* interrupt enable register */
#define	IXP425_UART_IER_RTOIE	0x10	/* receiver timeout interrupt enable */
#define	IXP425_UART_IER_UUE	0x40	/* UART Unit enable */

/*#define	IXP4XX_COM_NPORTS	8*/

/*
 * Timers
 */
#define	IXP425_TIMER_HWBASE	(IXP425_IO_HWBASE + IXP425_TIMER_OFFSET)
#define	IXP425_TIMER_VBASE	(IXP425_IO_VBASE + IXP425_TIMER_OFFSET)

#define	IXP425_OST_TS		0x0000
#define	IXP425_OST_TIM0		0x0004
#define	IXP425_OST_TIM1		0x000C

#define	IXP425_OST_TIM0_RELOAD	0x0008
#define	IXP425_OST_TIM1_RELOAD	0x0010
#define	TIMERRELOAD_MASK	0xFFFFFFFC
#define	OST_ONESHOT_EN		(1U << 1)
#define	OST_TIMER_EN		(1U << 0)

#define	IXP425_OST_STATUS	0x0020
#define	OST_WARM_RESET		(1U << 4)
#define	OST_WDOG_INT		(1U << 3)
#define	OST_TS_INT		(1U << 2)
#define	OST_TIM1_INT		(1U << 1)
#define	OST_TIM0_INT		(1U << 0)

#define	IXP425_OST_WDOG		0x0014
#define	IXP425_OST_WDOG_ENAB	0x0018
#define	IXP425_OST_WDOG_KEY	0x001c
#define	OST_WDOG_KEY_MAJICK	0x482e
#define	OST_WDOG_ENAB_RST_ENA	(1u << 0)
#define	OST_WDOG_ENAB_INT_ENA	(1u << 1)
#define	OST_WDOG_ENAB_CNT_ENA	(1u << 2)

/*
 * Interrupt Controller Unit.
 *  PA 0xc8003000
 */

#define	IXP425_IRQ_HWBASE	IXP425_IO_HWBASE + IXP425_INTR_OFFSET
#define	IXP425_IRQ_VBASE	IXP425_IO_VBASE  + IXP425_INTR_OFFSET
						/* 0xf0003000 */
#define	IXP425_IRQ_SIZE		0x00000020UL

#define	IXP425_INT_STATUS	(IXP425_IRQ_VBASE + 0x00)
#define	IXP425_INT_ENABLE	(IXP425_IRQ_VBASE + 0x04)
#define	IXP425_INT_SELECT	(IXP425_IRQ_VBASE + 0x08)
#define	IXP425_IRQ_STATUS	(IXP425_IRQ_VBASE + 0x0C)
#define	IXP425_FIQ_STATUS	(IXP425_IRQ_VBASE + 0x10)
#define	IXP425_INT_PRTY		(IXP425_IRQ_VBASE + 0x14)
#define	IXP425_IRQ_ENC		(IXP425_IRQ_VBASE + 0x18)
#define	IXP425_FIQ_ENC		(IXP425_IRQ_VBASE + 0x1C)

#define	IXP425_INT_SW1		31	/* SW Interrupt 1 */
#define	IXP425_INT_SW0		30	/* SW Interrupt 0 */
#define	IXP425_INT_GPIO_12	29	/* GPIO 12 */
#define	IXP425_INT_GPIO_11	28	/* GPIO 11 */
#define	IXP425_INT_GPIO_10	27	/* GPIO 11 */
#define	IXP425_INT_GPIO_9	26	/* GPIO 9 */
#define	IXP425_INT_GPIO_8	25	/* GPIO 8 */
#define	IXP425_INT_GPIO_7	24	/* GPIO 7 */
#define	IXP425_INT_GPIO_6	23	/* GPIO 6 */
#define	IXP425_INT_GPIO_5	22	/* GPIO 5 */
#define	IXP425_INT_GPIO_4	21	/* GPIO 4 */
#define	IXP425_INT_GPIO_3	20	/* GPIO 3 */
#define	IXP425_INT_GPIO_2	19	/* GPIO 2 */
#define	IXP425_INT_XSCALE_PMU	18	/* XScale PMU */
#define	IXP425_INT_AHB_PMU	17	/* AHB PMU */
#define	IXP425_INT_WDOG		16	/* Watchdog Timer */
#define	IXP425_INT_UART0	15	/* HighSpeed UART */
#define	IXP425_INT_STAMP	14	/* Timestamp Timer */
#define	IXP425_INT_UART1	13	/* Console UART */
#define	IXP425_INT_USB		12	/* USB */
#define	IXP425_INT_TMR1		11	/* General-Purpose Timer1 */
#define	IXP425_INT_PCIDMA2	10	/* PCI DMA Channel 2 */
#define	IXP425_INT_PCIDMA1	 9	/* PCI DMA Channel 1 */
#define	IXP425_INT_PCIINT	 8	/* PCI Interrupt */
#define	IXP425_INT_GPIO_1	 7	/* GPIO 1 */
#define	IXP425_INT_GPIO_0	 6	/* GPIO 0 */
#define	IXP425_INT_TMR0		 5	/* General-Purpose Timer0 */
#define	IXP425_INT_QUE33_64	 4	/* Queue Manager 33-64 */
#define	IXP425_INT_QUE1_32	 3	/* Queue Manager  1-32 */
#define	IXP425_INT_NPE_C	 2	/* NPE C */
#define	IXP425_INT_NPE_B	 1	/* NPE B */
#define	IXP425_INT_NPE_A	 0	/* NPE A */

/* NB: IXP435 has an additional 32 IRQ's */
#define	IXP435_INT_STATUS2	(IXP425_IRQ_VBASE + 0x20)
#define	IXP435_INT_ENABLE2	(IXP425_IRQ_VBASE + 0x24)
#define	IXP435_INT_SELECT2	(IXP425_IRQ_VBASE + 0x28)
#define	IXP435_IRQ_STATUS2	(IXP425_IRQ_VBASE + 0x2C)
#define	IXP435_FIQ_STATUS2	(IXP425_IRQ_VBASE + 0x30)

#define	IXP435_INT_USB0		32	/* USB Host 2.0 Host 0 */
#define	IXP435_INT_USB1		33	/* USB Host 2.0 Host 1 */
#define	IXP435_INT_QMGR_PER	60	/* Queue manager parity error */
#define	IXP435_INT_ECC		61	/* Single or multi-bit ECC error */

/*
 * software interrupt
 */
#define	IXP425_INT_bit31	31
#define	IXP425_INT_bit30	30
#define	IXP425_INT_bit14	14
#define	IXP425_INT_bit11	11

#define	IXP425_INT_HWMASK	(0xffffffff & \
					~((1 << IXP425_INT_bit31) | \
					  (1 << IXP425_INT_bit30) | \
					  (1 << IXP425_INT_bit14) | \
					  (1 << IXP425_INT_bit11)))
#define	IXP425_INT_GPIOMASK	(0x3ff800c0u)

#define	IXP435_INT_HWMASK	((1 << (IXP435_INT_USB0 - 32)) | \
				 (1 << (IXP435_INT_USB1 - 32)) | \
				 (1 << (IXP435_INT_QMGR_PER - 32)) | \
				 (1 << (IXP435_INT_ECC - 32)))

/*
 * GPIO
 */
#define	IXP425_GPIO_HWBASE	IXP425_IO_HWBASE + IXP425_GPIO_OFFSET
#define IXP425_GPIO_VBASE	IXP425_IO_VBASE  + IXP425_GPIO_OFFSET
					/* 0xf0004000 */
#define IXP425_GPIO_SIZE	0x00000020UL

#define	IXP425_GPIO_GPOUTR	0x00
#define	IXP425_GPIO_GPOER	0x04
#define	IXP425_GPIO_GPINR	0x08
#define	IXP425_GPIO_GPISR	0x0c
#define	IXP425_GPIO_GPIT1R	0x10
#define	IXP425_GPIO_GPIT2R	0x14
#define	IXP425_GPIO_GPCLKR	0x18
# define GPCLKR_MUX14	(1U << 8)
# define GPCLKR_CLK0TC_SHIFT	4
# define GPCLKR_CLK0DC_SHIFT	0

/* GPIO Output */
#define	GPOUT_ON		0x1
#define	GPOUT_OFF		0x0

/* GPIO direction */
#define	GPOER_INPUT		0x1
#define	GPOER_OUTPUT		0x0

/* GPIO Type bits */
#define	GPIO_TYPE_ACT_HIGH	0x0
#define	GPIO_TYPE_ACT_LOW	0x1
#define	GPIO_TYPE_EDG_RISING	0x2
#define	GPIO_TYPE_EDG_FALLING	0x3
#define	GPIO_TYPE_TRANSITIONAL	0x4
#define	GPIO_TYPE_MASK		0x7
#define	GPIO_TYPE(b,v)		((v) << (((b) & 0x7) * 3))
#define	GPIO_TYPE_REG(b)	(((b)&8)?IXP425_GPIO_GPIT2R:IXP425_GPIO_GPIT1R)

#define	IXP4XX_GPIO_PINS	16

/*
 * Expansion Bus Configuration Space.
 */
#define	IXP425_EXP_HWBASE	0xc4000000UL
#define	IXP425_EXP_VBASE	0xf0010000UL
#define	IXP425_EXP_SIZE		0x1000

/* offset */
#define	EXP_TIMING_CS0_OFFSET		0x0000
#define	EXP_TIMING_CS1_OFFSET		0x0004
#define	EXP_TIMING_CS2_OFFSET		0x0008
#define	EXP_TIMING_CS3_OFFSET		0x000c
#define	EXP_TIMING_CS4_OFFSET		0x0010
#define	EXP_TIMING_CS5_OFFSET		0x0014
#define	EXP_TIMING_CS6_OFFSET		0x0018
#define	EXP_TIMING_CS7_OFFSET		0x001c
#define EXP_CNFG0_OFFSET		0x0020
#define EXP_CNFG1_OFFSET		0x0024
#define	EXP_FCTRL_OFFSET		0x0028

#define IXP425_EXP_RECOVERY_SHIFT	16
#define IXP425_EXP_HOLD_SHIFT		20
#define IXP425_EXP_STROBE_SHIFT		22
#define IXP425_EXP_SETUP_SHIFT		26
#define IXP425_EXP_ADDR_SHIFT		28
#define IXP425_EXP_CS_EN		(1U << 31)

#define IXP425_EXP_RECOVERY_T(x)	(((x) & 15) << IXP425_EXP_RECOVERY_SHIFT)
#define IXP425_EXP_HOLD_T(x)		(((x) & 3)  << IXP425_EXP_HOLD_SHIFT)
#define IXP425_EXP_STROBE_T(x)		(((x) & 15) << IXP425_EXP_STROBE_SHIFT)
#define IXP425_EXP_SETUP_T(x)		(((x) & 3)  << IXP425_EXP_SETUP_SHIFT)
#define IXP425_EXP_ADDR_T(x)		(((x) & 3)  << IXP425_EXP_ADDR_SHIFT)

/* EXP_CSn bits */
#define EXP_BYTE_EN		0x00000001	/* bus uses only 8-bit data */
#define EXP_WR_EN		0x00000002	/* ena writes to CS region */
/* bit 2 is reserved */
#define EXP_SPLT_EN		0x00000008	/* ena AHB split transfers */
#define EXP_MUX_EN		0x00000010	/* multiplexed address/data */
#define EXP_HRDY_POL		0x00000020	/* HPI|HRDY polarity */
#define EXP_BYTE_RD16		0x00000040	/* byte rd access to word dev */
#define	EXP_CNFG		0x00003c00	/* device config size */
#define EXP_SZ_512		(0 << 10)
#define EXP_SZ_1K		(1 << 10)
#define EXP_SZ_2K		(2 << 10)
#define EXP_SZ_4K		(3 << 10)
#define EXP_SZ_8K		(4 << 10)
#define EXP_SZ_16K		(5 << 10)
#define EXP_SZ_32K		(6 << 10)
#define EXP_SZ_64K		(7 << 10)
#define EXP_SZ_128K		(8 << 10)
#define EXP_SZ_256K		(9 << 10)
#define EXP_SZ_512K		(10 << 10)
#define EXP_SZ_1M		(11 << 10)
#define EXP_SZ_2M		(12 << 10)
#define EXP_SZ_4M		(13 << 10)
#define EXP_SZ_8M		(14 << 10)
#define EXP_SZ_16M		(15 << 10)
#define	EXP_CYC_TYPE		0x0000c000	/* bus cycle "type" */
#define EXP_CYC_INTEL		(0 << 14)
#define EXP_CYC_MOTO		(1 << 14)
#define EXP_CYC_HPI		(2 << 14)
#define	EXP_T5			0x000f0000	/* recovery timing */
#define	EXP_T4			0x00300000	/* hold timing */
#define	EXP_T3			0x03c00000	/* strobe timing */
#define	EXP_T2			0x0c000000	/* setup/chip select timing */
#define	EXP_T1			0x30000000	/* address timing */
/* bit 30 is reserved */
#define	EXP_CS_EN		0x80000000	/* chip select enabled */

/* EXP_CNFG0 bits */
#define EXP_CNFG0_8BIT             (1 << 0)
#define EXP_CNFG0_PCI_HOST         (1 << 1)
#define EXP_CNFG0_PCI_ARB          (1 << 2)
#define EXP_CNFG0_PCI_66MHZ        (1 << 4)
#define EXP_CNFG0_MEM_MAP          (1 << 31)

/* EXP_CNFG1 bits */
#define EXP_CNFG1_SW_INT0          (1 << 0)
#define EXP_CNFG1_SW_INT1          (1 << 1)

#define	EXP_FCTRL_RCOMP		(1<<0)
#define	EXP_FCTRL_USB_DEVICE	(1<<1)
#define	EXP_FCTRL_HASH		(1<<2)
#define	EXP_FCTRL_AES		(1<<3)
#define	EXP_FCTRL_DES		(1<<4)
#define	EXP_FCTRL_HDLC		(1<<5)
#define	EXP_FCTRL_AAL		(1<<6)
#define	EXP_FCTRL_HSS		(1<<7)
#define	EXP_FCTRL_UTOPIA	(1<<8)
#define	EXP_FCTRL_ETH0		(1<<9)
#define	EXP_FCTRL_ETH1		(1<<10)
#define	EXP_FCTRL_NPEA		(1<<11)		/* reset */
#define	EXP_FCTRL_NPEB		(1<<12)		/* reset */
#define	EXP_FCTRL_NPEC		(1<<13)		/* reset */
#define	EXP_FCTRL_PCI		(1<<14)
#define	EXP_FCTRL_ECC_TIMESYNC	(1<<15)
#define	EXP_FCTRL_UTOPIA_PHY	(3<<16)		/* PHY limit */
#define	EXP_FCTRL_USB_HOST	(1<<18)
#define	EXP_FCTRL_NPEA_ETH	(1<<19)
#define	EXP_FCTRL_NPEB_ETH	(1<<20)
#define	EXP_FCTRL_RSA		(1<<21)
#define	EXP_FCTRL_MAXFREQ	(3<<22)		/* XScale frequency */
#define	EXP_FCTRL_RESVD		(0xff<<24)

#define EXP_FCTRL_IXP46X_ONLY \
	(EXP_FCTRL_ECC_TIMESYNC | EXP_FCTRL_USB_HOST | EXP_FCTRL_NPEA_ETH | \
	 EXP_FCTRL_NPEB_ETH | EXP_FCTRL_RSA | EXP_FCTRL_MAXFREQ)

#define	EXP_FCTRL_BITS \
	"\20\1RCOMP\2USB\3HASH\4AES\5DES\6HDLC\7AAL\10HSS\11UTOPIA\12ETH0" \
	"\13ETH1\17PCI\20ECC\23USB_HOST\24NPEA_ETH\25NPEB_ETH\26RSA"

/*
 * PCI
 */
#define IXP425_PCI_HWBASE	0xc0000000
#define	IXP425_PCI_VBASE	0xf0017000UL
#define	IXP425_PCI_SIZE		0x1000

#define	IXP425_AHB_OFFSET	0x00000000UL	/* AHB bus */

/*
 * Mapping registers of IXP425 PCI Configuration
 */
/* PCI_ID_REG			0x00 */
/* PCI_COMMAND_STATUS_REG	0x04 */
/* PCI_CLASS_REG		0x08 */
/* PCI_BHLC_REG			0x0c */
#define	PCI_MAPREG_BAR0		0x10	/* Base Address 0 */
#define	PCI_MAPREG_BAR1		0x14	/* Base Address 1 */
#define	PCI_MAPREG_BAR2		0x18	/* Base Address 2 */
#define	PCI_MAPREG_BAR3		0x1c	/* Base Address 3 */
#define	PCI_MAPREG_BAR4		0x20	/* Base Address 4 */
#define	PCI_MAPREG_BAR5		0x24	/* Base Address 5 */
/* PCI_SUBSYS_ID_REG		0x2c */
/* PCI_INTERRUPT_REG		0x3c */
#define	PCI_RTOTTO		0x40

/* PCI Controller CSR Base Address */
#define	IXP425_PCI_CSR_BASE	IXP425_PCI_VBASE

/* PCI Memory Space */
#define	IXP425_PCI_MEM_HWBASE	0x48000000UL
#define	IXP425_PCI_MEM_VBASE	0xf8000000UL
#define	IXP425_PCI_MEM_SIZE	0x04000000UL	/* 64MB */

/* PCI I/O Space */
#define	IXP425_PCI_IO_HWBASE	0x00000000UL
#define	IXP425_PCI_IO_SIZE	0x00100000UL    /* 1Mbyte */

/* PCI Controller Configuration Offset */
#define	PCI_NP_AD		0x00
#define	PCI_NP_CBE		0x04
# define NP_CBE_SHIFT		4
#define	PCI_NP_WDATA		0x08
#define	PCI_NP_RDATA		0x0c
#define	PCI_CRP_AD_CBE		0x10
#define	PCI_CRP_AD_WDATA	0x14
#define	PCI_CRP_AD_RDATA	0x18
#define	PCI_CSR			0x1c
# define CSR_PRST		(1U << 16)
# define CSR_IC			(1U << 15)
# define CSR_ABE		(1U << 4)
# define CSR_PDS		(1U << 3)
# define CSR_ADS		(1U << 2)
# define CSR_HOST		(1U << 0)
#define	PCI_ISR			0x20
# define ISR_AHBE		(1U << 3)
# define ISR_PPE		(1U << 2)
# define ISR_PFE		(1U << 1)
# define ISR_PSE		(1U << 0)
#define	PCI_INTEN		0x24
#define	PCI_DMACTRL		0x28
#define	PCI_AHBMEMBASE		0x2c
#define	PCI_AHBIOBASE		0x30
#define	PCI_PCIMEMBASE		0x34
#define	PCI_AHBDOORBELL		0x38
#define	PCI_PCIDOORBELL		0x3c
#define	PCI_ATPDMA0_AHBADDR	0x40
#define	PCI_ATPDMA0_PCIADDR	0x44
#define	PCI_ATPDMA0_LENGTH	0x48
#define	PCI_ATPDMA1_AHBADDR	0x4c
#define	PCI_ATPDMA1_PCIADDR	0x50
#define	PCI_ATPDMA1_LENGTH	0x54
#define	PCI_PTADMA0_AHBADDR	0x58
#define	PCI_PTADMA0_PCIADDR	0x5c
#define	PCI_PTADMA0_LENGTH	0x60
#define	PCI_PTADMA1_AHBADDR	0x64
#define	PCI_PTADMA1_PCIADDR	0x68
#define	PCI_PTADMA1_LENGTH	0x6c

/* PCI target(T)/initiator(I) Interface Commands for PCI_NP_CBE register */
#define	COMMAND_NP_IA		0x0	/* Interrupt Acknowledge   (I)*/
#define	COMMAND_NP_SC		0x1	/* Special Cycle	   (I)*/
#define	COMMAND_NP_IO_READ	0x2	/* I/O Read		(T)(I) */
#define	COMMAND_NP_IO_WRITE	0x3	/* I/O Write		(T)(I) */
#define	COMMAND_NP_MEM_READ	0x6	/* Memory Read		(T)(I) */
#define	COMMAND_NP_MEM_WRITE	0x7	/* Memory Write		(T)(I) */
#define	COMMAND_NP_CONF_READ	0xa	/* Configuration Read	(T)(I) */
#define	COMMAND_NP_CONF_WRITE	0xb	/* Configuration Write	(T)(I) */

/* PCI byte enables */
#define	BE_8BIT(a)		((0x10u << ((a) & 0x03)) ^ 0xf0)
#define	BE_16BIT(a)		((0x30u << ((a) & 0x02)) ^ 0xf0)
#define	BE_32BIT(a)		0x00

/* PCI byte selects */
#define	READ_8BIT(v,a)		((u_int8_t)((v) >> (((a) & 3) * 8)))
#define	READ_16BIT(v,a)		((u_int16_t)((v) >> (((a) & 2) * 8)))
#define	WRITE_8BIT(v,a)		(((u_int32_t)(v)) << (((a) & 3) * 8))
#define	WRITE_16BIT(v,a)	(((u_int32_t)(v)) << (((a) & 2) * 8))

/* PCI Controller Configuration Commands for PCI_CRP_AD_CBE */
#define COMMAND_CRP_READ	0x00
#define COMMAND_CRP_WRITE	(1U << 16)

/*
 * SDRAM Configuration Register
 */
#define	IXP425_MCU_HWBASE	0xcc000000UL
#define IXP425_MCU_VBASE	0xf0200000UL
#define	IXP425_MCU_SIZE		0x1000		/* Actually only 256 bytes */
#define	MCU_SDR_CONFIG		0x00
#define  MCU_SDR_CONFIG_MCONF(x) ((x) & 0x7)
#define  MCU_SDR_CONFIG_64MBIT	(1u << 5)
#define	MCU_SDR_REFRESH		0x04
#define	MCU_SDR_IR		0x08

/*
 * IXP435 DDR MCU Registers
 */
#define	IXP435_MCU_HWBASE	0xcc00e500UL
#define	MCU_DDR_SDIR		0x00		/* DDR SDAM Initialization Reg*/
#define	MCU_DDR_SDCR0		0x04		/* DDR SDRAM Control Reg 0 */
#define	MCU_DDR_SDCR1		0x08		/* DDR SDRAM Control Reg 1 */
#define	MCU_DDR_SDBR		0x0c		/* SDRAM Base Register */
#define	MCU_DDR_SBR0		0x10		/* SDRAM Boundary Register 0 */
#define	MCU_DDR_SBR1		0x14		/* SDRAM Boundary Register 1 */
#define	MCU_DDR_ECCR		0x1c		/* ECC Control Register */
#define	MCU_DDR_ELOG0		0x20		/* ECC Log Register 0 */
#define	MCU_DDR_ELOG1		0x24		/* ECC Log Register 1 */
#define	MCU_DDR_ECAR0		0x28		/* ECC Address Register 0 */
#define	MCU_DDR_ECAR1		0x2c		/* ECC Address Register 1 */
#define	MCU_DDR_ECTST		0x30		/* ECC Test Register */
#define	MCU_DDR_MCISR		0x34		/* MC Interrupt Status Reg */
#define	MCU_DDR_MPTCR		0x3c		/* MC Port Transaction Cnt Reg*/
#define	MCU_DDR_RFR		0x48		/* Refresh Frequency Register */
#define	MCU_DDR_SDPR(n)		(0x50+(n)*4)	/* SDRAM Page Register 0-7 */
/* NB: RCVDLY at 0x1050 and LEGOVERIDE at 0x1074 */

/*
 * Performance Monitoring Unit          (CP14)
 *
 *      CP14.0.1	Performance Monitor Control Register(PMNC)
 *      CP14.1.1	Clock Counter(CCNT)
 *      CP14.4.1	Interrupt Enable Register(INTEN)
 *      CP14.5.1	Overflow Flag Register(FLAG)
 *      CP14.8.1	Event Selection Register(EVTSEL)
 *      CP14.0.2	Performance Counter Register 0(PMN0)
 *      CP14.1.2	Performance Counter Register 0(PMN1)
 *      CP14.2.2	Performance Counter Register 0(PMN2)
 *      CP14.3.2	Performance Counter Register 0(PMN3)
 */

#define	PMNC_E		0x00000001	/* enable all counters */
#define	PMNC_P		0x00000002	/* reset all PMNs to 0 */
#define	PMNC_C		0x00000004	/* clock counter reset */
#define	PMNC_D		0x00000008	/* clock counter / 64 */

#define INTEN_CC_IE	0x00000001	/* enable clock counter interrupt */
#define	INTEN_PMN0_IE	0x00000002	/* enable PMN0 interrupt */
#define	INTEN_PMN1_IE	0x00000004	/* enable PMN1 interrupt */
#define	INTEN_PMN2_IE	0x00000008	/* enable PMN2 interrupt */
#define	INTEN_PMN3_IE	0x00000010	/* enable PMN3 interrupt */

#define	FLAG_CC_IF	0x00000001	/* clock counter overflow */
#define	FLAG_PMN0_IF	0x00000002	/* PMN0 overflow */
#define	FLAG_PMN1_IF	0x00000004	/* PMN1 overflow */
#define	FLAG_PMN2_IF	0x00000008	/* PMN2 overflow */
#define	FLAG_PMN3_IF	0x00000010	/* PMN3 overflow */

#define EVTSEL_EVCNT_MASK 0x0000000ff	/* event to count for PMNs */
#define PMNC_EVCNT0_SHIFT 0
#define PMNC_EVCNT1_SHIFT 8
#define PMNC_EVCNT2_SHIFT 16
#define PMNC_EVCNT3_SHIFT 24

/* 
 * Queue Manager
 */
#define	IXP425_QMGR_HWBASE	0x60000000UL
#define IXP425_QMGR_VBASE	0xf0018000UL
#define IXP425_QMGR_SIZE	0x4000

/*
 * Network Processing Engines (NPE's) and associated Ethernet MAC's.
 */
#define IXP425_NPE_A_HWBASE	(IXP425_IO_HWBASE + IXP425_NPE_A_OFFSET)
#define IXP425_NPE_A_VBASE	(IXP425_IO_VBASE + IXP425_NPE_A_OFFSET)
#define IXP425_NPE_A_SIZE	0x1000		/* Actually only 256 bytes */

#define IXP425_NPE_B_HWBASE	(IXP425_IO_HWBASE + IXP425_NPE_B_OFFSET)
#define IXP425_NPE_B_VBASE	(IXP425_IO_VBASE + IXP425_NPE_B_OFFSET)
#define IXP425_NPE_B_SIZE	0x1000		/* Actually only 256 bytes */

#define IXP425_NPE_C_HWBASE	(IXP425_IO_HWBASE + IXP425_NPE_C_OFFSET)
#define IXP425_NPE_C_VBASE	(IXP425_IO_VBASE + IXP425_NPE_C_OFFSET)
#define IXP425_NPE_C_SIZE	0x1000		/* Actually only 256 bytes */

#define IXP425_MAC_B_HWBASE	(IXP425_IO_HWBASE + IXP425_MAC_B_OFFSET)
#define IXP425_MAC_B_VBASE	(IXP425_IO_VBASE + IXP425_MAC_B_OFFSET)
#define IXP425_MAC_B_SIZE	0x1000 		/* Actually only 256 bytes */

#define IXP425_MAC_C_HWBASE	(IXP425_IO_HWBASE + IXP425_MAC_C_OFFSET)
#define IXP425_MAC_C_VBASE	(IXP425_IO_VBASE + IXP425_MAC_C_OFFSET)
#define IXP425_MAC_C_SIZE	0x1000		/* Actually only 256 bytes */

#define IXP435_MAC_A_HWBASE	(IXP425_IO_HWBASE + IXP435_MAC_A_OFFSET)
#define IXP435_MAC_A_VBASE	(IXP425_IO_VBASE + IXP435_MAC_A_OFFSET)
#define IXP435_MAC_A_SIZE	0x1000 		/* Actually only 256 bytes */

/*
 * Expansion Bus Data Space.
 */
#define	IXP425_EXP_BUS_HWBASE	0x50000000UL
#define	IXP425_EXP_BUS_SIZE	0x01000000	/* max, typically smaller */

#define	IXP425_EXP_BUS_CSx_HWBASE(i) \
	(IXP425_EXP_BUS_HWBASE + (i)*IXP425_EXP_BUS_SIZE)
#define	IXP425_EXP_BUS_CSx_SIZE		0x1000
#define	IXP425_EXP_BUS_CSx_VBASE(i) \
	(0xF0011000UL + (((i)-1)*IXP425_EXP_BUS_CSx_SIZE))

/* NB: CS0 is special; it maps flash */
#define	IXP425_EXP_BUS_CS0_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(0)
#define IXP425_EXP_BUS_CS0_VBASE	0xFD000000UL
#ifndef IXP4XX_FLASH_SIZE
#define IXP425_EXP_BUS_CS0_SIZE		0x01000000	/* NB: 16M */
#else
#define IXP425_EXP_BUS_CS0_SIZE		IXP4XX_FLASH_SIZE
#endif
#define	IXP425_EXP_BUS_CS1_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(1)
#define IXP425_EXP_BUS_CS1_VBASE	IXP425_EXP_BUS_CSx_VBASE(1)
#define IXP425_EXP_BUS_CS1_SIZE		IXP425_EXP_BUS_CSx_SIZE
#define	IXP425_EXP_BUS_CS2_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(2)
#define IXP425_EXP_BUS_CS2_VBASE	IXP425_EXP_BUS_CSx_VBASE(2)
#define IXP425_EXP_BUS_CS2_SIZE		IXP425_EXP_BUS_CSx_SIZE
#define	IXP425_EXP_BUS_CS3_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(3)
#define IXP425_EXP_BUS_CS3_VBASE	IXP425_EXP_BUS_CSx_VBASE(3)
#define IXP425_EXP_BUS_CS3_SIZE		IXP425_EXP_BUS_CSx_SIZE
#define	IXP425_EXP_BUS_CS4_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(4)
#define IXP425_EXP_BUS_CS4_VBASE	IXP425_EXP_BUS_CSx_VBASE(4)
#define IXP425_EXP_BUS_CS4_SIZE		IXP425_EXP_BUS_CSx_SIZE

/* NB: not mapped (yet) */
#define	IXP425_EXP_BUS_CS5_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(5)
#define	IXP425_EXP_BUS_CS6_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(6)
#define	IXP425_EXP_BUS_CS7_HWBASE	IXP425_EXP_BUS_CSx_HWBASE(7)

/*
 * IXP435/Gateworks Cambria
 */
#define IXP435_USB1_HWBASE	0xCD000000UL	/* USB host controller 1 */
#define IXP435_USB1_VBASE	0xF001C000UL
#define IXP435_USB1_SIZE	0x1000		/* NB: only uses 0x300 */

#define IXP435_USB2_HWBASE	0xCE000000UL	/* USB host controller 2 */
#define IXP435_USB2_VBASE	0xF001D000UL
#define IXP435_USB2_SIZE	0x1000		/* NB: only uses 0x300 */

#define	CAMBRIA_GPS_HWBASE	0x53FC0000UL	/* optional GPS Serial Port */
#define	CAMBRIA_GPS_VBASE	0xF001E000UL
#define	CAMBRIA_GPS_SIZE	0x1000
#define	CAMBRIA_RS485_HWBASE	0x53F80000UL	/* optional RS485 Serial Port */
#define	CAMBRIA_RS485_VBASE	0xF001F000UL
#define	CAMBRIA_RS485_SIZE	0x1000

/* NB: these are mapped on the fly, so no fixed virtual addresses */
#define	CAMBRIA_OCTAL_LED_HWBASE 0x53F40000UL	/* Octal Status LED Latch */
#define	CAMBRIA_OCTAL_LED_SIZE	0x1000
#define	CAMBRIA_CFSEL1_HWBASE	0x53E40000UL	/* Compact Flash Socket Sel 0 */
#define	CAMBRIA_CFSEL1_SIZE	0x40000
#define	CAMBRIA_CFSEL0_HWBASE	0x53E00000UL	/* Compact Flash Socket Sel 1 */
#define	CAMBRIA_CFSEL0_SIZE	0x40000

#endif /* _IXP425REG_H_ */
