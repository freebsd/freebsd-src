/*-
 * Copyright (C) 2007-2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _MVREG_H_
#define _MVREG_H_

#include <machine/intr.h>

/*
 * Physical addresses of integrated SoC peripherals
 */
#define MV_PHYS_BASE		0xF1000000
#define MV_SIZE			0x100000

/*
 * Decode windows addresses (physical)
 */
#define MV_PCIE_IO_PHYS_BASE	(MV_PHYS_BASE + MV_SIZE)
#define MV_PCIE_IO_BASE		MV_PCIE_IO_PHYS_BASE
#define MV_PCIE_IO_SIZE		(1024 * 1024)
#define MV_PCI_IO_PHYS_BASE	(MV_PCIE_IO_PHYS_BASE + MV_PCIE_IO_SIZE) 
#define MV_PCI_IO_BASE		MV_PCI_IO_PHYS_BASE
#define MV_PCI_IO_SIZE		(1024 * 1024)

#define MV_PCIE_MEM_PHYS_BASE	(MV_PCI_IO_PHYS_BASE + MV_PCI_IO_SIZE)
#define MV_PCIE_MEM_BASE	MV_PCIE_MEM_PHYS_BASE
#define MV_PCIE_MEM_SIZE	(64 * 1024 * 1024)
#define MV_PCI_MEM_PHYS_BASE	(MV_PCIE_MEM_PHYS_BASE + MV_PCIE_MEM_SIZE)
#define MV_PCI_MEM_BASE		MV_PCI_MEM_PHYS_BASE
#define MV_PCI_MEM_SIZE		(64 * 1024 * 1024)

/* XXX DEV_BOOT, CSx are board specific, should be defined per platform */

/* 512KB NOR FLASH */
#define MV_DEV_BOOT_PHYS_BASE	(MV_PCI_MEM_PHYS_BASE + MV_PCI_MEM_SIZE)
#define MV_DEV_BOOT_SIZE		(512 * 1024)
/* CS0: 7-seg LED */
#define MV_DEV_CS0_PHYS_BASE	0xFA000000
#define MV_DEV_CS0_SIZE	(1024 * 1024) /* XXX u-boot has 2MB */
/* CS1: 32MB NOR FLASH */
#define MV_DEV_CS1_PHYS_BASE	(MV_DEV_CS0_PHYS_BASE + MV_DEV_CS0_SIZE)
#define MV_DEV_CS1_SIZE	(32 * 1024 * 1024)
/* CS2: 32MB NAND FLASH */
#define MV_DEV_CS2_PHYS_BASE	(MV_DEV_CS1_PHYS_BASE + MV_DEV_CS1_SIZE)
#define MV_DEV_CS2_SIZE	1024	/* XXX u-boot has 1MB */

/* XXX this is probably not robust against wraparounds... */
#if ((MV_DEV_CS2_PHYS_BASE + MV_DEV_CS2_SIZE) > 0xFFFEFFFF)
#error Devices memory layout overlaps reset vectors range!
#endif

/*
 * Integrated SoC peripherals addresses
 */
#define MV_BASE			MV_PHYS_BASE	/* VA == PA mapping */
#define MV_DDR_CADR_BASE	(MV_BASE + 0x1500)
#define MV_MPP_BASE		(MV_BASE + 0x10000)
#define MV_GPIO_BASE		(MV_BASE + 0x10100)
#define MV_GPIO_SIZE		0x20
#define MV_RTC_BASE		(MV_BASE + 0x10300)
#define MV_RTC_SIZE		0x08
#define MV_TWSI_BASE		(MV_BASE + 0x11000)
#define MV_TWSI_SIZE		0x20
#define MV_UART0_BASE		(MV_BASE + 0x12000)
#define MV_UART1_BASE		(MV_BASE + 0x12100)
#define MV_UART_SIZE		0x20
#define MV_MBUS_BRIDGE_BASE	(MV_BASE + 0x20000)
#define MV_INTREGS_BASE		(MV_MBUS_BRIDGE_BASE + 0x80)
#define MV_CPU_CONTROL_BASE	(MV_MBUS_BRIDGE_BASE + 0x100)
#define MV_IC_BASE		(MV_MBUS_BRIDGE_BASE + 0x200)
#define MV_IC_SIZE		0x3C
#define MV_TIMERS_BASE		(MV_MBUS_BRIDGE_BASE + 0x300)
#define MV_TIMERS_SIZE		0x30
#define MV_PCI_BASE		(MV_BASE + 0x30000)
#define MV_PCI_SIZE		0x2000
#define MV_PCIE_BASE		(MV_BASE + 0x40000)
#define MV_PCIE_SIZE		0x2000
#define MV_USB0_BASE		(MV_BASE + 0x50000)
#define MV_USB1_BASE		(MV_USB0_BASE + 0x1000)
#define MV_USB2_BASE		(MV_USB0_BASE + 0x2000)
#define MV_USB_SIZE		0x1000
#define MV_USB_HOST_OFST	0x0100		/* EHCI HC regs start at this offset within USB range */
#define MV_USB_AWR_BASE		(MV_USB0_BASE + 0x320)
#define MV_IDMA_BASE		(MV_BASE + 0x60000)
#define MV_IDMA_SIZE		0x1000
#define MV_XOR_BASE		(MV_BASE + 0x60000)
#define MV_XOR_SIZE		0x1000
#define MV_ETH0_BASE		(MV_BASE + 0x72000)
#define MV_ETH1_BASE		(MV_BASE + 0x76000)
#define MV_ETH_SIZE		0x2000

#define MV_DEV_CS0_BASE	MV_DEV_CS0_PHYS_BASE

/*
 * Interrupt sources
 */
#if defined(SOC_MV_ORION)

#define MV_INT_BRIDGE		0	/* AHB-MBus Bridge Interrupt */
#define MV_INT_UART0		3	/* UART0 Interrupt */
#define MV_INT_UART1		4
#define MV_INT_GPIO7_0		6	/* GPIO[7:0] Interrupt */
#define MV_INT_GPIO15_8		7	/* GPIO[15:8] Interrupt */
#define MV_INT_GPIO23_16	8	/* GPIO[23:16] Interrupt */
#define MV_INT_GPIO31_24	9	/* GPIO[31:24] Interrupt */
#define MV_INT_PEX0_ERR		10	/* PCI Express Error */
#define MV_INT_PEX0		11	/* PCI Express INTA,B,C,D Message */
#define MV_INT_PCI_ERR		15	/* PCI Error */
#define MV_INT_USB_BERR		16	/* USB Bridge Error */
#define MV_INT_USB_CI		17	/* USB Controller interrupt */
#define MV_INT_GBERX		18	/* GbE receive interrupt */
#define MV_INT_GBETX		19	/* GbE transmit interrupt */
#define MV_INT_GBEMISC		20	/* GbE misc. interrupt */
#define MV_INT_GBESUM		21	/* GbE summary interrupt */
#define MV_INT_GBEERR		22	/* GbE error interrupt */
#define MV_INT_IDMA_ERR		23	/* DMA error interrupt */
#define MV_INT_IDMA0		24	/* IDMA chan. 0 completion interrupt */
#define MV_INT_IDMA1		25	/* IDMA chan. 1 completion interrupt */
#define MV_INT_IDMA2		26	/* IDMA chan. 2 completion interrupt */
#define MV_INT_IDMA3		27	/* IDMA chan. 3 completion interrupt */
#define MV_INT_SATA		29	/* Serial-ATA Interrupt */

#elif defined(SOC_MV_KIRKWOOD)

#define MV_INT_BRIDGE		1	/* AHB-MBus Bridge Interrupt */
#define MV_INT_XOR0_CHAN0	5	/* XOR engine 0 channel 0 Interrupt */
#define MV_INT_XOR0_CHAN1	6	/* XOR engine 0 channel 1 Interrupt */
#define MV_INT_XOR1_CHAN0	7	/* XOR engine 1 channel 0 Interrupt */
#define MV_INT_XOR1_CHAN1	8	/* XOR engine 1 channel 1 Interrupt */
#define MV_INT_PEX0		9	/* PCI Express INTA,B,C,D Message */
#define MV_INT_GBESUM		11	/* GbE0 summary interrupt */
#define MV_INT_GBERX		12	/* GbE0 receive interrupt */
#define MV_INT_GBETX		13	/* GbE0 transmit interrupt */
#define MV_INT_GBEMISC		14	/* GbE0 misc. interrupt */
#define MV_INT_GBE1SUM		15	/* GbE1 summary interrupt */
#define MV_INT_GBE1RX		16	/* GbE1 receive interrupt */
#define MV_INT_GBE1TX		17	/* GbE1 transmit interrupt */
#define MV_INT_GBE1MISC		18	/* GbE1 misc. interrupt */
#define MV_INT_USB_CI		19	/* USB Controller interrupt */
#define MV_INT_SATA		21	/* Serial-ATA Interrupt */
#define MV_INT_IDMA_ERR		23	/* DMA error interrupt */
#define MV_INT_UART0		33	/* UART0 Interrupt */
#define MV_INT_UART1		34
#define MV_INT_GPIO7_0		35	/* GPIO[7:0] Interrupt */
#define MV_INT_GPIO15_8		36	/* GPIO[15:8] Interrupt */
#define MV_INT_GPIO23_16	37	/* GPIO[23:16] Interrupt */
#define MV_INT_GPIO31_24	38	/* GPIO[31:24] Interrupt */
#define MV_INT_GPIOHI7_0	39	/* GPIOHI[7:0] Interrupt */
#define MV_INT_GPIOHI15_8	40	/* GPIOHI[15:8] Interrupt */
#define MV_INT_GPIOHI23_16	41	/* GPIOHI[23:16] Interrupt */
#define MV_INT_XOR0_ERR		42	/* XOR engine 0 error Interrupt */
#define MV_INT_XOR1_ERR		43	/* XOR engine 1 error Interrupt */
#define MV_INT_PEX0_ERR		44	/* PCI Express Error */
#define MV_INT_GBEERR		46	/* GbE0 error interrupt */
#define MV_INT_GBE1ERR		47	/* GbE1 error interrupt */
#define MV_INT_USB_BERR		48	/* USB Bridge Error */

#elif defined(SOC_MV_DISCOVERY)

#define MV_INT_ERRSUM		0	/* Summary of error interrupts */
#define MV_INT_SPI		1	/* SPI interrupt */
#define MV_INT_TWSI0		2	/* TWSI0 interrupt */
#define MV_INT_TWSI1		3	/* TWSI1 interrupt */
#define MV_INT_IDMA0		4	/* IDMA Channel0 completion */
#define MV_INT_IDMA1		5	/* IDMA Channel0 completion */
#define MV_INT_IDMA2		6	/* IDMA Channel0 completion */
#define MV_INT_IDMA3		7	/* IDMA Channel0 completion */
#define MV_INT_TIMER0		8	/* Timer0 interrupt */
#define MV_INT_TIMER1		9	/* Timer1 interrupt */
#define MV_INT_TIMER2		10	/* Timer2 interrupt */
#define MV_INT_TIMER3		11	/* Timer3 interrupt */
#define MV_INT_UART0		12	/* UART0 interrupt */
#define MV_INT_UART1		13	/* UART1 interrupt */
#define MV_INT_UART2		14	/* UART2 interrupt */
#define MV_INT_UART3		15	/* UART3 interrupt */
#define MV_INT_USB0		16	/* USB0 interrupt */
#define MV_INT_USB1		17	/* USB1 interrupt */
#define MV_INT_USB2		18	/* USB2 interrupt */
#define MV_INT_CRYPTO		19	/* Crypto engine completion interrupt */
#define MV_INT_XOR0		22	/* XOR engine 0 completion interrupt */
#define MV_INT_XOR1		23	/* XOR engine 1 completion interrupt */
#define MV_INT_SATA		26	/* SATA interrupt */
#define MV_INT_PEX00		32	/* PCI Express port 0.0 INTA/B/C/D */
#define MV_INT_PEX01		33	/* PCI Express port 0.1 INTA/B/C/D */
#define MV_INT_PEX02		34	/* PCI Express port 0.2 INTA/B/C/D */
#define MV_INT_PEX03		35	/* PCI Express port 0.3 INTA/B/C/D */
#define MV_INT_PEX10		36	/* PCI Express port 1.0 INTA/B/C/D */
#define MV_INT_PEX11		37	/* PCI Express port 1.1 INTA/B/C/D */
#define MV_INT_PEX12		38	/* PCI Express port 1.2 INTA/B/C/D */
#define MV_INT_PEX13		39	/* PCI Express port 1.3 INTA/B/C/D */
#define MV_INT_GBESUM		40	/* Gigabit Ethernet Port 0 summary */
#define MV_INT_GBERX		41	/* Gigabit Ethernet Port 0 Rx summary */
#define MV_INT_GBETX		42	/* Gigabit Ethernet Port 0 Tx summary */
#define MV_INT_GBEMISC		43	/* Gigabit Ethernet Port 0 Misc summ. */
#define MV_INT_GBE1SUM		44	/* Gigabit Ethernet Port 1 summary */
#define MV_INT_GBE1RX		45	/* Gigabit Ethernet Port 1 Rx summary */
#define MV_INT_GBE1TX		46	/* Gigabit Ethernet Port 1 Tx summary */
#define MV_INT_GBE1MISC		47	/* Gigabit Ethernet Port 1 Misc summ. */
#define MV_INT_GPIO7_0		56	/* GPIO[7:0] Interrupt */
#define MV_INT_GPIO15_8		57	/* GPIO[15:8] Interrupt */
#define MV_INT_GPIO23_16	58	/* GPIO[23:16] Interrupt */
#define MV_INT_GPIO31_24	59	/* GPIO[31:24] Interrupt */
#define MV_INT_DB_IN		60	/* Inbound Doorbell Cause reg Summary */
#define MV_INT_DB_OUT		61	/* Outbound Doorbell Cause reg Summ. */
#define MV_INT_CRYPT_ERR	64	/* Crypto engine error */
#define MV_INT_DEV_ERR		65	/* Device bus error */
#define MV_INT_IDMA_ERR		66	/* DMA error */
#define MV_INT_CPU_ERR		67	/* CPU error */
#define MV_INT_PEX0_ERR		68	/* PCI-Express port0 error */
#define MV_INT_PEX1_ERR		69	/* PCI-Express port1 error */
#define MV_INT_GBE_ERR		70	/* Gigabit Ethernet error */
#define MV_INT_USB_ERR		72	/* USB error */
#define MV_INT_DRAM_ERR		73	/* DRAM ECC error */
#define MV_INT_XOR_ERR		74	/* XOR engine error */
#define MV_INT_WD		79	/* WD Timer interrupt */

#endif /* SOC_MV_ORION */

#define BRIDGE_IRQ_CAUSE	0x10
#define BRIGDE_IRQ_MASK		0x14

#if defined(SOC_MV_DISCOVERY)
#define IRQ_CAUSE_ERROR		0x0
#define IRQ_CAUSE		0x4
#define IRQ_CAUSE_HI		0x8
#define IRQ_MASK_ERROR		0xC
#define IRQ_MASK		0x10
#define IRQ_MASK_HI		0x14
#define IRQ_CAUSE_SELECT	0x18
#define FIQ_MASK_ERROR		0x1C
#define FIQ_MASK		0x20
#define FIQ_MASK_HI		0x24
#define FIQ_CAUSE_SELECT	0x28
#define ENDPOINT_IRQ_MASK_ERROR	0x2C
#define ENDPOINT_IRQ_MASK	0x30
#define ENDPOINT_IRQ_MASK_HI	0x34
#define ENDPOINT_IRQ_CAUSE_SELECT 0x38
#else /* !SOC_MV_DISCOVERY */
#define IRQ_CAUSE		0x0
#define IRQ_MASK		0x4
#define FIQ_MASK		0x8
#define ENDPOINT_IRQ_MASK	0xC
#define IRQ_CAUSE_HI		0x10
#define IRQ_MASK_HI		0x14
#define FIQ_MASK_HI		0x18
#define ENDPOINT_IRQ_MASK_HI	0x1C
#define IRQ_CAUSE_ERROR		(-1)		/* Fake defines for unified */
#define IRQ_MASK_ERROR		(-1)		/* interrupt controller code */
#endif

#define BRIDGE_IRQ_CAUSE	0x10
#define IRQ_CPU_SELF		0x00000001
#define IRQ_TIMER0		0x00000002
#define IRQ_TIMER1		0x00000004
#define IRQ_TIMER_WD		0x00000008

#define BRIDGE_IRQ_MASK		0x14
#define IRQ_CPU_MASK		0x00000001
#define IRQ_TIMER0_MASK		0x00000002
#define IRQ_TIMER1_MASK		0x00000004
#define IRQ_TIMER_WD_MASK	0x00000008

/*
 * System reset
 */
#define RSTOUTn_MASK		0x8
#define WD_RST_OUT_EN		0x00000002
#define SOFT_RST_OUT_EN		0x00000004
#define SYSTEM_SOFT_RESET	0xc
#define SYS_SOFT_RST		0x00000001

/*
 * Power Control
 */
#define CPU_PM_CTRL		0x1C
#define CPU_PM_CTRL_NONE	0

#if defined(SOC_MV_KIRKWOOD)
#define CPU_PM_CTRL_GE0		(1 << 0)
#define CPU_PM_CTRL_PEX0_PHY	(1 << 1)
#define CPU_PM_CTRL_PEX0	(1 << 2)
#define CPU_PM_CTRL_USB0	(1 << 3)
#define CPU_PM_CTRL_SDIO	(1 << 4)
#define CPU_PM_CTRL_TSU		(1 << 5)
#define CPU_PM_CTRL_DUNIT	(1 << 6)
#define CPU_PM_CTRL_RUNIT	(1 << 7)
#define CPU_PM_CTRL_XOR0	(1 << 8)
#define CPU_PM_CTRL_AUDIO	(1 << 9)
#define CPU_PM_CTRL_SATA0	(1 << 14)
#define CPU_PM_CTRL_SATA1	(1 << 15)
#define CPU_PM_CTRL_XOR1	(1 << 16)
#define CPU_PM_CTRL_CRYPTO	(1 << 17)
#define CPU_PM_CTRL_GE1		(1 << 18)
#define CPU_PM_CTRL_TDM		(1 << 19)
#elif defined(SOC_MV_DISCOVERY)
#define CPU_PM_CTRL_GE0		(1 << 1)
#define CPU_PM_CTRL_GE1		(1 << 2)
#define CPU_PM_CTRL_PEX00	(1 << 5)
#define CPU_PM_CTRL_PEX01	(1 << 6)
#define CPU_PM_CTRL_PEX02	(1 << 7)
#define CPU_PM_CTRL_PEX03	(1 << 8)
#define CPU_PM_CTRL_PEX10	(1 << 9)
#define CPU_PM_CTRL_PEX11	(1 << 10)
#define CPU_PM_CTRL_PEX12	(1 << 11)
#define CPU_PM_CTRL_PEX13	(1 << 12)
#define CPU_PM_CTRL_SATA0_PHY	(1 << 13)
#define CPU_PM_CTRL_SATA0	(1 << 14)
#define CPU_PM_CTRL_SATA1_PHY	(1 << 15)
#define CPU_PM_CTRL_SATA1	(1 << 16)
#define CPU_PM_CTRL_USB0	(1 << 17)
#define CPU_PM_CTRL_USB1	(1 << 18)
#define CPU_PM_CTRL_USB2	(1 << 19)
#define CPU_PM_CTRL_IDMA	(1 << 20)
#define CPU_PM_CTRL_XOR		(1 << 21)
#define CPU_PM_CTRL_CRYPTO	(1 << 22)
#define CPU_PM_CTRL_DEVICE	(1 << 23)
#endif

/*
 * Timers
 */
#define CPU_TIMER_CONTROL	0x0
#define CPU_TIMER0_EN		0x00000001
#define CPU_TIMER0_AUTO		0x00000002
#define CPU_TIMER1_EN		0x00000004
#define CPU_TIMER1_AUTO		0x00000008
#define CPU_TIMER_WD_EN		0x00000010
#define CPU_TIMER_WD_AUTO	0x00000020
#define CPU_TIMER0_REL		0x10
#define CPU_TIMER0		0x14

/*
 * GPIO
 */
#define GPIO_DATA_OUT		0x00
#define GPIO_DATA_OUT_EN_CTRL	0x04
#define GPIO_BLINK_EN		0x08
#define GPIO_DATA_IN_POLAR	0x0c
#define GPIO_DATA_IN		0x10
#define GPIO_INT_CAUSE		0x14
#define GPIO_INT_EDGE_MASK	0x18
#define GPIO_INT_LEV_MASK	0x1c

#define GPIO_HI_DATA_OUT		0x40
#define GPIO_HI_DATA_OUT_EN_CTRL	0x44
#define GPIO_HI_BLINK_EN		0x48
#define GPIO_HI_DATA_IN_POLAR		0x4c
#define GPIO_HI_DATA_IN			0x50
#define GPIO_HI_INT_CAUSE		0x54
#define GPIO_HI_INT_EDGE_MASK		0x58
#define GPIO_HI_INT_LEV_MASK		0x5c

#define GPIO(n)			(1 << (n))
#define MV_GPIO_MAX_NPINS	64

#define MV_GPIO_BLINK		0x1
#define MV_GPIO_POLARITY	0x2
#define MV_GPIO_EDGE		0x4
#define MV_GPIO_LEVEL		0x8

#define IS_GPIO_IRQ(irq)	((irq) >= NIRQ && (irq) < NIRQ + MV_GPIO_MAX_NPINS)
#define GPIO2IRQ(gpio)		((gpio) + NIRQ)
#define IRQ2GPIO(irq)		((irq) - NIRQ)

/*
 * MPP
 */
#define MPP_CONTROL0		0x00
#define MPP_CONTROL1		0x04
#define MPP_CONTROL2		0x50
#define DEVICE_MULTIPLEX	0x08

#if defined(SOC_MV_ORION)
#define SAMPLE_AT_RESET		0x10
#elif defined(SOC_MV_KIRKWOOD) || defined(SOC_MV_DISCOVERY)
#define SAMPLE_AT_RESET		0x30
#else
#error SOC_MV_XX not defined
#endif

/*
 * Clocks
 */
#ifdef SOC_MV_ORION
#define TCLK_MASK		0x300
#define TCLK_SHIFT		0x8
#elif defined(SOC_MV_KIRKWOOD) || defined(SOC_MV_DISCOVERY)
#define TCLK_MASK		0x30000
#define TCLK_SHIFT		0x10
#else
#error SOC_MV_XX not defined
#endif

#define TCLK_100MHZ		100000000
#define TCLK_125MHZ		125000000
#define TCLK_133MHZ		133333333
#define TCLK_150MHZ		150000000
#define TCLK_166MHZ		166666667
#define TCLK_200MHZ		200000000

/*
 * Chip ID
 */
#define MV_DEV_88F5181	0x5181
#define MV_DEV_88F5182	0x5182
#define MV_DEV_88F5281	0x5281
#define MV_DEV_88F6281	0x6281
#define MV_DEV_MV78100	0x6381

/*
 * Decode windows definitions and macros
 */
#define MV_WIN_CPU_CTRL(n)		(0x10 * (n) + (((n) < 8) ? 0x000 : 0x880))
#define MV_WIN_CPU_BASE(n)		(0x10 * (n) + (((n) < 8) ? 0x004 : 0x884))
#define MV_WIN_CPU_REMAP_LO(n)		(0x10 * (n) + (((n) < 8) ? 0x008 : 0x888))
#define MV_WIN_CPU_REMAP_HI(n)		(0x10 * (n) + (((n) < 8) ? 0x00C : 0x88C))
#if defined(SOC_MV_DISCOVERY)
#define MV_WIN_CPU_MAX			14
#else
#define MV_WIN_CPU_MAX			8
#endif

#define MV_WIN_DDR_BASE(n)		(0x8 * (n) + 0x0)
#define MV_WIN_DDR_SIZE(n)		(0x8 * (n) + 0x4)
#define MV_WIN_DDR_MAX			4

#define MV_WIN_USB_CTRL(n)		(0x10 * (n) + 0x0)
#define MV_WIN_USB_BASE(n)		(0x10 * (n) + 0x4)
#define MV_WIN_USB_MAX			4

#define MV_WIN_ETH_BASE(n)		(0x8 * (n) + 0x200)
#define MV_WIN_ETH_SIZE(n)		(0x8 * (n) + 0x204)
#define MV_WIN_ETH_REMAP(n)		(0x4 * (n) + 0x280)
#define MV_WIN_ETH_MAX			6

#define MV_WIN_IDMA_BASE(n)		(0x8 * (n) + 0xa00)
#define MV_WIN_IDMA_SIZE(n)		(0x8 * (n) + 0xa04)
#define MV_WIN_IDMA_REMAP(n)		(0x4 * (n) + 0xa60)
#define MV_WIN_IDMA_CAP(n)		(0x4 * (n) + 0xa70)
#define MV_WIN_IDMA_MAX			8
#define MV_IDMA_CHAN_MAX		4

#define MV_WIN_PCIE_CTRL(n)		(0x10 * (((n) < 5) ? (n) : \
					    (n) + 1) + 0x1820)
#define MV_WIN_PCIE_BASE(n)		(0x10 * (((n) < 5) ? (n) : \
					    (n) + 1) + 0x1824)
#define MV_WIN_PCIE_REMAP(n)		(0x10 * (((n) < 5) ? (n) : \
					    (n) + 1) + 0x182C)
#define MV_WIN_PCIE_MAX			6

#define MV_PCIE_BAR(n)			(0x04 * (n) + 0x1804)
#define MV_PCIE_BAR_MAX			3

#define WIN_REG_IDX_RD(pre,reg,off,base)					\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(int i)						\
	{									\
		return (bus_space_read_4(obio_tag, base, off(i)));		\
	}

#define WIN_REG_BASE_IDX_RD(pre,reg,off)					\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(uint32_t base, int i)				\
	{									\
		return (bus_space_read_4(obio_tag, base, off(i)));		\
	}

#define WIN_REG_IDX_WR(pre,reg,off,base)					\
	static __inline void							\
	pre ## _ ## reg ## _write(int i, uint32_t val)				\
	{									\
		bus_space_write_4(obio_tag, base, off(i), val);			\
	}

#define WIN_REG_BASE_IDX_WR(pre,reg,off)					\
	static __inline void							\
	pre ## _ ## reg ## _write(uint32_t base, int i, uint32_t val)		\
	{									\
		bus_space_write_4(obio_tag, base, off(i), val);			\
	}

#define WIN_REG_RD(pre,reg,off,base)						\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(void)						\
	{									\
		return (bus_space_read_4(obio_tag, base, off));			\
	}

#define WIN_REG_BASE_RD(pre,reg,off)						\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(uint32_t base)					\
	{									\
		return (bus_space_read_4(obio_tag, base, off));			\
	}

#define WIN_REG_WR(pre,reg,off,base)						\
	static __inline void							\
	pre ## _ ## reg ## _write(uint32_t val)					\
	{									\
		bus_space_write_4(obio_tag, base, off, val);			\
	}

#define WIN_REG_BASE_WR(pre,reg,off)						\
	static __inline void							\
	pre ## _ ## reg ## _write(uint32_t base, uint32_t val)			\
	{									\
		bus_space_write_4(obio_tag, base, off, val);			\
	}

#endif /* _MVREG_H_ */
