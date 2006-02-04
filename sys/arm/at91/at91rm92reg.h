/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef AT91RM92REG_H_
#define AT91RM92REG_H_
/* 
 * Memory map, from datasheet :
 * 0x00000000 - 0x0ffffffff : Internal Memories
 * 0x10000000 - 0x1ffffffff : Chip Select 0
 * 0x20000000 - 0x2ffffffff : Chip Select 1
 * 0x30000000 - 0x3ffffffff : Chip Select 2
 * 0x40000000 - 0x4ffffffff : Chip Select 3
 * 0x50000000 - 0x5ffffffff : Chip Select 4
 * 0x60000000 - 0x6ffffffff : Chip Select 5
 * 0x70000000 - 0x7ffffffff : Chip Select 6
 * 0x80000000 - 0x8ffffffff : Chip Select 7
 * 0x90000000 - 0xeffffffff : Undefined (Abort)
 * 0xf0000000 - 0xfffffffff : Peripherals
 */

#define AT91RM92_BASE		0xf0000000
/* Usart */

#define AT91RM92_USART0_BASE	0xffc0000
#define AT91RM92_USART0_PDC	0xffc0100
#define AT91RM92_USART1_BASE	0xffc4000
#define AT91RM92_USART1_PDC	0xffc4100
#define AT91RM92_USART2_BASE	0xffc8000
#define AT91RM92_USART2_PDC	0xffc8100
#define AT91RM92_USART3_BASE	0xffcc000
#define AT91RM92_USART3_PDC	0xffcc100
#define AT91RM92_USART_SIZE	0x4000

/* System Registers */

#define AT91RM92_SYS_BASE	0xffff000
#define AT91RM92_SYS_SIZE	0x1000
/* Interrupt Controller */
#define IC_SMR			(0) /* Source mode register */
#define IC_SVR			(128) /* Source vector register */
#define IC_IVR			(256) /* IRQ vector register */
#define IC_FVR			(260) /* FIQ vector register */
#define IC_ISR			(264) /* Interrupt status register */
#define IC_IPR			(268) /* Interrupt pending register */
#define IC_IMR			(272) /* Interrupt status register */
#define IC_CISR			(276) /* Core interrupt status register */
#define IC_IECR			(288) /* Interrupt enable command register */
#define IC_IDCR			(292) /* Interrupt disable command register */
#define IC_ICCR			(296) /* Interrupt clear command register */
#define IC_ISCR			(300) /* Interrupt set command register */
#define IC_EOICR		(304) /* End of interrupt command register */
#define IC_SPU			(308) /* Spurious vector register */
#define IC_DCR			(312) /* Debug control register */
#define IC_FFER			(320) /* Fast forcing enable register */
#define IC_FFDR			(324) /* Fast forcing disable register */
#define IC_FFSR			(328) /* Fast forcing status register */

/* DBGU */

#define DBGU			0x200
#define DBGU_SIZE		0x200
#define DBGU_C1R		(0x200 + 64) /* Chip ID1 Register */
#define DBGU_C2R		(0x200 + 68) /* Chip ID2 Register */
#define DBGU_FNTR		(0x200 + 72) /* Force NTRST Register */

#define PIOA_PER		(0x400) /* PIO Enable Register */
#define PIOA_PDR		(0x400 + 4) /* PIO Disable Register */
#define PIOA_PSR		(0x400 + 8) /* PIO status register */
#define PIOA_OER		(0x400 + 16) /* Output enable register */
#define PIOA_ODR		(0x400 + 20) /* Output disable register */
#define PIOA_OSR		(0x400 + 24) /* Output status register */
#define PIOA_IFER		(0x400 + 32) /* Input filter enable register */
#define PIOA_IFDR		(0x400 + 36) /* Input filter disable register */
#define PIOA_IFSR		(0x400 + 40) /* Input filter status register */
#define PIOA_SODR		(0x400 + 48) /* Set output data register */
#define PIOA_CODR		(0x400 + 52) /* Clear output data register */
#define PIOA_ODSR		(0x400 + 56) /* Output data status register */
#define PIOA_PDSR		(0x400 + 60) /* Pin data status register */
#define PIOA_IER		(0x400 + 64) /* Interrupt enable register */
#define PIOA_IDR		(0x400 + 68) /* Interrupt disable register */
#define PIOA_IMR		(0x400 + 72) /* Interrupt mask register */
#define PIOA_ISR		(0x400 + 76) /* Interrupt status register */
#define PIOA_MDER		(0x400 + 80) /* Multi driver enable register */
#define PIOA_MDDR		(0x400 + 84) /* Multi driver disable register */
#define PIOA_MDSR		(0x400 + 88) /* Multi driver status register */
#define PIOA_PPUDR		(0x400 + 96) /* Pull-up disable register */
#define PIOA_PPUER		(0x400 + 100) /* Pull-up enable register */
#define PIOA_PPUSR		(0x400 + 104) /* Pad pull-up status register */
#define PIOA_ASR		(0x400 + 112) /* Select A register */
#define PIOA_BSR		(0x400 + 116) /* Select B register */
#define PIOA_ABSR		(0x400 + 120) /* AB Select status register */
#define PIOA_OWER		(0x400 + 160) /* Output Write enable register */
#define PIOA_OWDR		(0x400 + 164) /* Output write disable register */
#define PIOA_OWSR		(0x400 + 168) /* Output write status register */
#define PIOB_PER		(0x400) /* PIO Enable Register */
#define PIOB_PDR		(0x600 + 4) /* PIO Disable Register */
#define PIOB_PSR		(0x600 + 8) /* PIO status register */
#define PIOB_OER		(0x600 + 16) /* Output enable register */
#define PIOB_ODR		(0x600 + 20) /* Output disable register */
#define PIOB_OSR		(0x600 + 24) /* Output status register */
#define PIOB_IFER		(0x600 + 32) /* Input filter enable register */
#define PIOB_IFDR		(0x600 + 36) /* Input filter disable register */
#define PIOB_IFSR		(0x600 + 40) /* Input filter status register */
#define PIOB_SODR		(0x600 + 48) /* Set output data register */
#define PIOB_CODR		(0x600 + 52) /* Clear output data register */
#define PIOB_ODSR		(0x600 + 56) /* Output data status register */
#define PIOB_PDSR		(0x600 + 60) /* Pin data status register */
#define PIOB_IER		(0x600 + 64) /* Interrupt enable register */
#define PIOB_IDR		(0x600 + 68) /* Interrupt disable register */
#define PIOB_IMR		(0x600 + 72) /* Interrupt mask register */
#define PIOB_ISR		(0x600 + 76) /* Interrupt status register */
#define PIOB_MDER		(0x600 + 80) /* Multi driver enable register */
#define PIOB_MDDR		(0x600 + 84) /* Multi driver disable register */
#define PIOB_MDSR		(0x600 + 88) /* Multi driver status register */
#define PIOB_PPUDR		(0x600 + 96) /* Pull-up disable register */
#define PIOB_PPUER		(0x600 + 100) /* Pull-up enable register */
#define PIOB_PPUSR		(0x600 + 104) /* Pad pull-up status register */
#define PIOB_ASR		(0x600 + 112) /* Select A register */
#define PIOB_BSR		(0x600 + 116) /* Select B register */
#define PIOB_ABSR		(0x600 + 120) /* AB Select status register */
#define PIOB_OWER		(0x600 + 160) /* Output Write enable register */
#define PIOB_OWDR		(0x600 + 164) /* Output write disable register */
#define PIOB_OWSR		(0x600 + 168) /* Output write status register */
#define PIOC_PER		(0x800) /* PIO Enable Register */
#define PIOC_PDR		(0x800 + 4) /* PIO Disable Register */
#define PIOC_PSR		(0x800 + 8) /* PIO status register */
#define PIOC_OER		(0x800 + 16) /* Output enable register */
#define PIOC_ODR		(0x800 + 20) /* Output disable register */
#define PIOC_OSR		(0x800 + 24) /* Output status register */
#define PIOC_IFER		(0x800 + 32) /* Input filter enable register */
#define PIOC_IFDR		(0x800 + 36) /* Input filter disable register */
#define PIOC_IFSR		(0x800 + 40) /* Input filter status register */
#define PIOC_SODR		(0x800 + 48) /* Set output data register */
#define PIOC_CODR		(0x800 + 52) /* Clear output data register */
#define PIOC_ODSR		(0x800 + 56) /* Output data status register */
#define PIOC_PDSR		(0x800 + 60) /* Pin data status register */
#define PIOC_IER		(0x800 + 64) /* Interrupt enable register */
#define PIOC_IDR		(0x800 + 68) /* Interrupt disable register */
#define PIOC_IMR		(0x800 + 72) /* Interrupt mask register */
#define PIOC_ISR		(0x800 + 76) /* Interrupt status register */
#define PIOC_MDER		(0x800 + 80) /* Multi driver enable register */
#define PIOC_MDDR		(0x800 + 84) /* Multi driver disable register */
#define PIOC_MDSR		(0x800 + 88) /* Multi driver status register */
#define PIOC_PPUDR		(0x800 + 96) /* Pull-up disable register */
#define PIOC_PPUER		(0x800 + 100) /* Pull-up enable register */
#define PIOC_PPUSR		(0x800 + 104) /* Pad pull-up status register */
#define PIOC_ASR		(0x800 + 112) /* Select A register */
#define PIOC_BSR		(0x800 + 116) /* Select B register */
#define PIOC_ABSR		(0x800 + 120) /* AB Select status register */
#define PIOC_OWER		(0x800 + 160) /* Output Write enable register */
#define PIOC_OWDR		(0x800 + 164) /* Output write disable register */
#define PIOC_OWSR		(0x800 + 168) /* Output write status register */
#define PIOD_PER		(0xa00) /* PIO Enable Register */
#define PIOD_PDR		(0xa00 + 4) /* PIO Disable Register */
#define PIOD_PSR		(0xa00 + 8) /* PIO status register */
#define PIOD_OER		(0xa00 + 16) /* Output enable register */
#define PIOD_ODR		(0xa00 + 20) /* Output disable register */
#define PIOD_OSR		(0xa00 + 24) /* Output status register */
#define PIOD_IFER		(0xa00 + 32) /* Input filter enable register */
#define PIOD_IFDR		(0xa00 + 36) /* Input filter disable register */
#define PIOD_IFSR		(0xa00 + 40) /* Input filter status register */
#define PIOD_SODR		(0xa00 + 48) /* Set output data register */
#define PIOD_CODR		(0xa00 + 52) /* Clear output data register */
#define PIOD_ODSR		(0xa00 + 56) /* Output data status register */
#define PIOD_PDSR		(0xa00 + 60) /* Pin data status register */
#define PIOD_IER		(0xa00 + 64) /* Interrupt enable register */
#define PIOD_IDR		(0xa00 + 68) /* Interrupt disable register */
#define PIOD_IMR		(0xa00 + 72) /* Interrupt mask register */
#define PIOD_ISR		(0xa00 + 76) /* Interrupt status register */
#define PIOD_MDER		(0xa00 + 80) /* Multi driver enable register */
#define PIOD_MDDR		(0xa00 + 84) /* Multi driver disable register */
#define PIOD_MDSR		(0xa00 + 88) /* Multi driver status register */
#define PIOD_PPUDR		(0xa00 + 96) /* Pull-up disable register */
#define PIOD_PPUER		(0xa00 + 100) /* Pull-up enable register */
#define PIOD_PPUSR		(0xa00 + 104) /* Pad pull-up status register */
#define PIOD_ASR		(0xa00 + 112) /* Select A register */
#define PIOD_BSR		(0xa00 + 116) /* Select B register */
#define PIOD_ABSR		(0xa00 + 120) /* AB Select status register */
#define PIOD_OWER		(0xa00 + 160) /* Output Write enable register */
#define PIOD_OWDR		(0xa00 + 164) /* Output write disable register */
#define PIOD_OWSR		(0xa00 + 168) /* Output write status register */

/* IRQs : */
/*
 * 0: AIC 
 * 1: System peripheral (System timer, RTC, DBGU)
 * 2: PIO Controller A
 * 3: PIO Controller B
 * 4: PIO Controller C
 * 5: PIO Controller D
 * 6: USART 0
 * 7: USART 1
 * 8: USART 2
 * 9: USART 3
 * 10: MMC Interface
 * 11: USB device port
 * 12: Two-wirte interface
 * 13: SPI
 * 14: SSC
 * 15: SSC
 * 16: SSC
 * 17: Timer Counter 0
 * 18: Timer Counter 1
 * 19: Timer Counter 2
 * 20: Timer Counter 3
 * 21: Timer Counter 4
 * 22: Timer Counter 6
 * 23: USB Host port
 * 24: Ethernet
 * 25: AIC
 * 26: AIC
 * 27: AIC
 * 28: AIC
 * 29: AIC
 * 30: AIC
 * 31: AIC
 */

#define AT91RM92_IRQ_SYSTEM	1
#define AT91RM92_IRQ_PIOA	2
#define AT91RM92_IRQ_PIOB	3
#define AT91RM92_IRQ_PIOC	4
#define AT91RM92_IRQ_PIOD	5
#define AT91RM92_IRQ_USART0	6
#define AT91RM92_IRQ_USART1	7
#define AT91RM92_IRQ_USART2	8
#define AT91RM92_IRQ_USART3	9
#define AT91RM92_IRQ_MCI	10
#define AT91RM92_IRQ_UDP	11
#define AT91RM92_IRQ_TWI	12
#define AT91RM92_IRQ_SPI	13
#define AT91RM92_IRQ_SSC0	14
#define AT91RM92_IRQ_SSC1	15
#define AT91RM92_IRQ_SSC2	16
#define AT91RM92_IRQ_TC0	17
#define AT91RM92_IRQ_TC1	18
#define AT91RM92_IRQ_TC2	19
#define AT91RM92_IRQ_TC3	20
#define AT91RM92_IRQ_TC4	21
#define AT91RM92_IRQ_TC5	22
#define AT91RM92_IRQ_UHP	23
#define AT91RM92_IRQ_EMAC	24
#define AT91RM92_IRQ_AIC_BASE	25

/* Timer */

#define AT91RM92_ST_BASE	0xffffd00
#define AT91RM92_ST_SIZE	0x100

#define AT91RM92_SPI_BASE	0xffe0000
#define AT91RM92_SPI_SIZE	0x4000
#define AT91RM92_SPI_PDC	0xffe0100

#define AT91RM92_SSC0_BASE	0xffd0000
#define AT91RM92_SSC0_PDC	0xffd0100

#define AT91RM92_SSC1_BASE	0xffd4000
#define AT91RM92_SSC1_PDC	0xffd4100

#define AT91RM92_SSC2_BASE	0xffd8000
#define AT91RM92_SSC2_PDC	0xffd8100

#define AT91RM92_SSC_SIZE	0x4000

#define AT91RM92_EMAC_BASE	0xffbc000
#define AT91RM92_EMAC_SIZE	0x4000

#define AT91RM92_TWI_BASE	0xffb8000
#define AT91RM92_TWI_SIZE	0x4000

#define AT91RM92_MCI_BASE	0xffb4000
#define AT91RM92_MCI_PDC	0xffb4100
#define AT91RM92_MCI_SIZE	0x4000

#define AT91RM92_UDP_BASE	0xffb0000
#define AT91RM92_UDP_SIZE	0x4000

#define AT91RM92_TC0_BASE	0xffa0000
#define AT91RM92_TC0_SIZE	0x4000
#define AT91RM92_TC0C0_BASE	0xffa0000
#define AT91RM92_TC0C1_BASE	0xffa0040
#define AT91RM92_TC0C2_BASE	0xffa0080

#define AT91RM92_TC1_BASE	0xffa4000
#define AT91RM92_TC1_SIZE	0x4000
#define AT91RM92_TC1C0_BASE	0xffa4000
#define AT91RM92_TC1C1_BASE	0xffa4040
#define AT91RM92_TC1C2_BASE	0xffa4080

#define AT91RM92_OHCI_BASE	0x00300000
#define AT91RM92_OHCI_SIZE	0x00100000

/* Pio definitions */
#define AT91RM92_PIO_PA0	(1 << 0)
#define AT91RM92_PA0_MISO	(AT91RM92_PIO_PA0) /* SPI Master In Slave */
#define AT91RM92_PA0_PCK3	(AT91RM92_PIO_PA0) /* PMC Programmable Clock Output 3 */
#define AT91RM92_PIO_PA1	(1 << 1)
#define AT91RM92_PA1_MOSI	(AT91RM92_PIO_PA1) /* SPI Master Out Slave */
#define AT91RM92_PA1_PCK0	(AT91RM92_PIO_PA1) /* PMC Programmable Clock Output 0 */
#define AT91RM92_PIO_PA2	(1 << 2)
#define AT91RM92_PA2_SPCK	(AT91RM92_PIO_PA2) /* SPI Serial Clock */
#define AT91RM92_PA2_IRQ4	(AT91RM92_PIO_PA2) /* AIC Interrupt Input 4 */
#define AT91RM92_PIO_PA3	(1 << 3)
#define AT91RM92_PA3_NPCS0	(AT91RM92_PIO_PA3) /* SPI Peripheral Chip Select 0 */
#define AT91RM92_PA3_IRQ5	(AT91RM92_PIO_PA3) /* AIC Interrupt Input 5 */
#define AT91RM92_PIO_PA4	(1 << 4)
#define AT91RM92_PA4_NPCS1	(AT91RM92_PIO_PA4) /* SPI Peripheral Chip Select 1 */
#define AT91RM92_PA4_PCK1	(AT91RM92_PIO_PA4) /* PMC Programmable Clock Output 1 */
#define AT91RM92_PIO_PA5	(1 << 5)
#define AT91RM92_PA5_NPCS2	(AT91RM92_PIO_PA5) /* SPI Peripheral Chip Select 2 */
#define AT91RM92_PA5_TXD3	(AT91RM92_PIO_PA5) /* USART 3 Transmit Data */
#define AT91RM92_PIO_PA6	(1 << 6)
#define AT91RM92_PA6_NPCS3	(AT91RM92_PIO_PA6) /* SPI Peripheral Chip Select 3 */
#define AT91RM92_PA6_RXD3	(AT91RM92_PIO_PA6) /* USART 3 Receive Data */
#define AT91RM92_PIO_PA7	(1 << 7)
#define AT91RM92_PA7_ETXCK_EREFC	(AT91RM92_PIO_PA7) /* Ethernet MAC Transmit Clock/Reference Clock */
#define AT91RM92_PA7_PCK2	(AT91RM92_PIO_PA7) /* PMC Programmable Clock 2 */
#define AT91RM92_PIO_PA8	(1 << 8)
#define AT91RM92_PA8_ETXEN	(AT91RM92_PIO_PA8) /* Ethernet MAC Transmit Enable */
#define AT91RM92_PA8_MCCDB	(AT91RM92_PIO_PA8) /* Multimedia Card B Command */
#define AT91RM92_PIO_PA9	(1 << 9)
#define AT91RM92_PA9_ETX0	(AT91RM92_PIO_PA9) /* Ethernet MAC Transmit Data 0 */
#define AT91RM92_PA9_MCDB0	(AT91RM92_PIO_PA9) /* Multimedia Card B Data 0 */
#define AT91RM92_PIO_PA10	(1 << 10)
#define AT91RM92_PA10_ETX1	(AT91RM92_PIO_PA10) /* Ethernet MAC Transmit Data 1 */
#define AT91RM92_PA10_MCDB1	(AT91RM92_PIO_PA10) /* Multimedia Card B Data 1 */
#define AT91RM92_PIO_PA11	(1 << 11)
#define AT91RM92_PA11_ECRS_ECRSDV	(AT91RM92_PIO_PA11) /* Ethernet MAC Carrier Sense/Carrier Sense and Data Valid */
#define AT91RM92_PA11_MCDB2	(AT91RM92_PIO_PA11) /* Multimedia Card B Data 2 */
#define AT91RM92_PIO_PA12	(1 << 12)
#define AT91RM92_PA12_ERX0	(AT91RM92_PIO_PA12) /* Ethernet MAC Receive Data 0 */
#define AT91RM92_PA12_MCDB3	(AT91RM92_PIO_PA12) /* Multimedia Card B Data 3 */
#define AT91RM92_PIO_PA13 	(1 << 13)
#define AT91RM92_PA13_ERX1	(AT91RM92_PIO_PA13) /* Ethernet MAC Receive Data 1 */
#define AT91RM92_PA13_TCLK0	(AT91RM92_PIO_PA13) /* Timer Counter 0 external clock input */
#define AT91RM92_PIO_PA14	(1 << 14)
#define AT91RM92_PA14_ERXER	(AT91RM92_PIO_PA14) /* Ethernet MAC Receive Error */
#define AT91RM92_PA14_TCLK1	(AT91RM92_PIO_PA14) /* Timer Counter 1 external clock input */
#define AT91RM92_PIO_PA15	(1 << 15)
#define AT91RM92_PA15_EMDC	(AT91RM92_PIO_PA15) /* Ethernet MAC Management Data Clock */
#define AT91RM92_PA15_TCLK2	(AT91RM92_PIO_PA15) /* Timer Counter 2 external clock input */
#define AT91RM92_PIO_PA16	(1 << 16)
#define AT91RM92_PA16_EMDIO	(AT91RM92_PIO_PA16) /* Ethernet MAC Management Data Input/Output */
#define AT91RM92_PA16_IRQ6	(AT91RM92_PIO_PA16) /* AIC Interrupt input 6 */
#define AT91RM92_PIO_PA17	(1 << 17)
#define AT91RM92_PA17_TXD0	(AT91RM92_PIO_PA17) /* USART 0 Transmit Data */
#define AT91RM92_PA17_TIOA0	(AT91RM92_PIO_PA17) /* Timer Counter 0 Multipurpose Timer I/O Pin A */
#define AT91RM92_PIO_PA18	(1 << 18)
#define AT91RM92_PA18_RXD0	(AT91RM92_PIO_PA18) /* USART 0 Receive Data */
#define AT91RM92_PA18_TIOB0	(AT91RM92_PIO_PA18) /* Timer Counter 0 Multipurpose Timer I/O Pin B */
#define AT91RM92_PIO_PA19	(1 << 19)
#define AT91RM92_PA19_SCK0	(AT91RM92_PIO_PA19) /* USART 0 Serial Clock */
#define AT91RM92_PA19_TIOA1	(AT91RM92_PIO_PA19) /* Timer Counter 1 Multipurpose Timer I/O Pin A */
#define AT91RM92_PIO_PA20	(1 << 20)
#define AT91RM92_PA20_CTS0	(AT91RM92_PIO_PA20) /* USART 0 Clear To Send */
#define AT91RM92_PA20_TIOB1	(AT91RM92_PIO_PA20) /* Timer Counter 1 Multipurpose Timer I/O Pin B */
#define AT91RM92_PIO_PA21	(1 << 21)
#define AT91RM92_PA21_RTS0	(AT91RM92_PIO_PA21) /* USART 0 Ready To Send */
#define AT91RM92_PA21_TIOA2	(AT91RM92_PIO_PA21) /* Timer Counter 2 Multipurpose Timer I/O Pin A */
#define AT91RM92_PIO_PA22	(1 << 22)
#define AT91RM92_PA22_RXD2	(AT91RM92_PIO_PA22) /* USART 2 Receive Data */
#define AT91RM92_PA22_TIOB2	(AT91RM92_PIO_PA22) /* Timer Counter 2 Multipurpose Timer I/O Pin B */
#define AT91RM92_PIO_PA23	(1 << 23)
#define AT91RM92_PA23_TXD2	(AT91RM92_PIO_PA23) /* USART 2 Transmit Data */
#define AT91RM92_PA23_IRQ3	(AT91RM92_PIO_PA23) /* Interrupt input 3 */
#define AT91RM92_PIO_PA24	(1 << 24)
#define AT91RM92_PA24_SCK2	(AT91RM92_PIO_PA24) /* USART 2 Serial Clock */
#define AT91RM92_PA24_PCK1	(AT91RM92_PIO_PA24) /* PMC Programmable Clock Output 1 */
#define AT91RM92_PIO_PA25	(1 << 25)
#define AT91RM92_PA25_TWD	(AT91RM92_PIO_PA25) /* TWI Two-wire Serial Data */
#define AT91RM92_PA25_IRQ2	(AT91RM92_PIO_PA25) /* Interrupt input 2 */
#define AT91RM92_PIO_PA26	(1 << 26)
#define AT91RM92_PA26_TWCK	(AT91RM92_PIO_PA26) /* TWI Two-wire Serial Clock */
#define AT91RM92_PA26_IRQ1	(AT91RM92_PIO_PA26) /* Interrupt input 1 */
#define AT91RM92_PIO_PA27	(1 << 27)
#define AT91RM92_PA27_MCCK	(AT91RM92_PIO_PA27) /* Multimedia Card Clock */
#define AT91RM92_PA27_TCLK3	(AT91RM92_PIO_PA27) /* Timer Counter 3 External Clock Input */
#define AT91RM92_PIO_PA28	(1 << 28)
#define AT91RM92_PA28_MCCDA	(AT91RM92_PIO_PA28) /* Multimedia Card A Command */
#define AT91RM92_PA28_TCLK4	(AT91RM92_PIO_PA28) /* Timer Counter 4 external Clock Input */
#define AT91RM92_PIO_PA29	(1 << 29)
#define AT91RM92_PA29_MCDA0	(AT91RM92_PIO_PA29) /* Multimedia Card A Data 0 */
#define AT91RM92_PA29_TCLK5	(AT91RM92_PIO_PA29) /* Timer Counter 5 external clock input */
#define AT91RM92_PIO_PA30	(1 << 30)
#define AT91RM92_PA30_DRXD	(AT91RM92_PIO_PA30) /* DBGU Debug Receive Data */
#define AT91RM92_PA30_CTS2	(AT91RM92_PIO_PA30) /* USART 2 Clear To Send */
#define AT91RM92_PIO_PA31	(1 << 31)
#define AT91RM92_PA31_DTXD	(AT91RM92_PIO_PA31) /* DBGU Debug Transmit Data */
#define AT91RM92_PA31_RTS2	(AT91RM92_PIO_PA31) /* USART 2 Ready To Send */

#define AT91RM92_PIO_PB0	(1 << 0)
#define AT91RM92_PB0_TF0	(AT91RM92_PIO_PB0) /* SSC Transmit Frame Sync 0 */
#define AT91RM92_PB0_RTS3	(AT91RM92_PIO_PB0) /* USART 3 Ready To Send */
#define AT91RM92_PIO_PB1	(1 << 1)
#define AT91RM92_PB1_TK0	(AT91RM92_PIO_PB1) /* SSC Transmit Clock 0 */
#define AT91RM92_PB1_CTS3	(AT91RM92_PIO_PB1) /* USART 3 Clear To Send */
#define AT91RM92_PIO_PB2	(1 << 2)
#define AT91RM92_PB2_TD0	(AT91RM92_PIO_PB2) /* SSC Transmit data */
#define AT91RM92_PB2_SCK3	(AT91RM92_PIO_PB2) /* USART 3 Serial Clock */
#define AT91RM92_PIO_PB3	(1 << 3)
#define AT91RM92_PB3_RD0	(AT91RM92_PIO_PB3) /* SSC Receive Data */
#define AT91RM92_PB3_MCDA1	(AT91RM92_PIO_PB3) /* Multimedia Card A Data 1 */
#define AT91RM92_PIO_PB4	(1 << 4)
#define AT91RM92_PB4_RK0	(AT91RM92_PIO_PB4) /* SSC Receive Clock */
#define AT91RM92_PB4_MCDA2	(AT91RM92_PIO_PB4) /* Multimedia Card A Data 2 */
#define AT91RM92_PIO_PB5	(1 << 5)
#define AT91RM92_PB5_RF0	(AT91RM92_PIO_PB5) /* SSC Receive Frame Sync 0 */
#define AT91RM92_PB5_MCDA3	(AT91RM92_PIO_PB5) /* Multimedia Card A Data 3 */
#define AT91RM92_PIO_PB6	(1 << 6)
#define AT91RM92_PB6_TF1	(AT91RM92_PIO_PB6) /* SSC Transmit Frame Sync 1 */
#define AT91RM92_PB6_TIOA3	(AT91RM92_PIO_PB6) /* Timer Counter 4 Multipurpose Timer I/O Pin A */
#define AT91RM92_PIO_PB7	(1 << 7)
#define AT91RM92_PB7_TK1	(AT91RM92_PIO_PB7) /* SSC Transmit Clock 1 */
#define AT91RM92_PB7_TIOB3	(AT91RM92_PIO_PB7) /* Timer Counter 3 Multipurpose Timer I/O Pin B */
#define AT91RM92_PIO_PB8	(1 << 8)
#define AT91RM92_PB8_TD1	(AT91RM92_PIO_PB8) /* SSC Transmit Data 1 */
#define AT91RM92_PB8_TIOA4	(AT91RM92_PIO_PB8) /* Timer Counter 4 Multipurpose Timer I/O Pin A */
#define AT91RM92_PIO_PB9	(1 << 9)
#define AT91RM92_PB9_RD1	(AT91RM92_PIO_PB9) /* SSC Receive Data 1 */
#define AT91RM92_PB9_TIOB4	(AT91RM92_PIO_PB9) /* Timer Counter 4 Multipurpose Timer I/O Pin B */
#define AT91RM92_PIO_PB10	(1 << 10)
#define AT91RM92_PB10_RK1	(AT91RM92_PIO_PB10) /* SSC Receive Clock 1 */
#define AT91RM92_PB10_TIOA5	(AT91RM92_PIO_PB10) /* Timer Counter 5 Multipurpose Timer I/O Pin A */
#define AT91RM92_PIO_PB11	(1 << 11)
#define AT91RM92_PB11_RF1	(AT91RM92_PIO_PB11) /* SSC Receive Frame Sync 1 */
#define AT91RM92_PB11_TIOB5	(AT91RM92_PIO_PB11) /* Timer Counter 5 Multipurpose Timer I/O Pin B */
#define AT91RM92_PIO_PB12	(1 << 12)
#define AT91RM92_PB12_TF2	(AT91RM92_PIO_PB12) /* SSC Transmit Frame Sync 2 */
#define AT91RM92_PB12_ETX2	(AT91RM92_PIO_PB12) /* Ethernet MAC Transmit Data 2 */
#define AT91RM92_PIO_PB13	(1 << 13)
#define AT91RM92_PB13_TK2	(AT91RM92_PIO_PB13) /* SSC Transmit Clock 2 */
#define AT91RM92_PB13_ETX3	(AT91RM92_PIO_PB13) /* Ethernet MAC Transmit Data 3 */
#define AT91RM92_PIO_PB14	(1 << 14)
#define AT91RM92_PB14_TD2	(AT91RM92_PIO_PB14) /* SSC Transmit Data 2 */
#define AT91RM92_PB14_ETXER	(AT91RM92_PIO_PB14) /* Ethernet MAC Transmikt Coding Error */
#define AT91RM92_PIO_PB15	(1 << 15)
#define AT91RM92_PB15_RD2	(AT91RM92_PIO_PB15) /* SSC Receive Data 2 */
#define AT91RM92_PB15_ERX2	(AT91RM92_PIO_PB15) /* Ethernet MAC Receive Data 2 */
#define AT91RM92_PIO_PB16	(1 << 16)
#define AT91RM92_PB16_RK2	(AT91RM92_PIO_PB16) /* SSC Receive Clock 2 */
#define AT91RM92_PB16_ERX3	(AT91RM92_PIO_PB16) /* Ethernet MAC Receive Data 3 */
#define AT91RM92_PIO_PB17	(1 << 17)
#define AT91RM92_PB17_RF2	(AT91RM92_PIO_PB17) /* SSC Receive Frame Sync 2 */
#define AT91RM92_PB17_ERXDV	(AT91RM92_PIO_PB17) /* Ethernet MAC Receive Data Valid */
#define AT91RM92_PIO_PB18	(1 << 18)
#define AT91RM92_PB18_RI1	(AT91RM92_PIO_PB18) /* USART 1 Ring Indicator */
#define AT91RM92_PB18_ECOL	(AT91RM92_PIO_PB18) /* Ethernet MAC Collision Detected */
#define AT91RM92_PIO_PB19	(1 << 19)
#define AT91RM92_PB19_DTR1	(AT91RM92_PIO_PB19) /* USART 1 Data Terminal ready */
#define AT91RM92_PB19_ERXCK	(AT91RM92_PIO_PB19) /* Ethernet MAC Receive Clock */
#define AT91RM92_PIO_PB20	(1 << 20)
#define AT91RM92_PB20_TXD1	(AT91RM92_PIO_PB20) /* USART 1 Transmit Data */
#define AT91RM92_PIO_PB21	(1 << 21)
#define AT91RM92_PB21_RXD1	(AT91RM92_PIO_PB21) /* USART 1 Receive Data */
#define AT91RM92_PIO_PB22	(1 << 22)
#define AT91RM92_PB22_SCK1	(AT91RM92_PIO_PB22) /* USART 1 Serial Clock */
#define AT91RM92_PIO_PB23	(1 << 23)
#define AT91RM92_PB23_DCD1	(AT91RM92_PIO_PB23) /* USART 1 Data Carrier Detect */
#define AT91RM92_PIO_PB24	(1 << 24)
#define AT91RM92_PB24_CTS1	(AT91RM92_PIO_PB24) /* USART 1 Clear To Send */
#define AT91RM92_PIO_PB25	(1 << 25)
#define AT91RM92_PB25_DSR1	(AT91RM92_PIO_PB25) /* USART 1 Data Set ready */
#define AT91RM92_PB25_EF100	(AT91RM92_PIO_PB25) /* Ethernet MAC Force 100 Mbits/sec */
#define AT91RM92_PIO_PB26	(1 << 26)
#define AT91RM92_PB26_RTS1	(AT91RM92_PIO_PB26) /* USART 1 Ready To Send */
#define AT91RM92_PIO_PB27	(1 << 27)
#define AT91RM92_PB27_PCK0	(AT91RM92_PIO_PB27) /* PMC Programmable Clock Output 0 */
#define AT91RM92_PIO_PB28	(1 << 28)
#define AT91RM92_PB28_FIQ	(AT91RM92_PIO_PB28) /* AIC Fast Interrupt Input */
#define AT91RM92_PIO_PB29	(1 << 29)
#define AT91RM92_PB29_IRQ0	(AT91RM92_PIO_PB29) /* Interrupt input 0 */

#define AT91RM92_PIO_PC0	(1 << 0)
#define AT91RM92_PC0_BFCK	(AT91RM92_PIO_PC0) /* Burst Flash Clock */
#define AT91RM92_PIO_PC1	(1 << 1)
#define AT91RM92_PC1_BFRDY_SMOE	(AT91RM92_PIO_PC1) /* Burst Flash Ready */
#define AT91RM92_PIO_PC2	(1 << 2)
#define AT91RM92_PC2_BFAVD	(AT91RM92_PIO_PC2) /* Burst Flash Address Valid */
#define AT91RM92_PIO_PC3	(1 << 3)
#define AT91RM92_PC3_BFBAA_SMWE	(AT91RM92_PIO_PC3) /* Burst Flash Address Advance / SmartMedia Write Enable */
#define AT91RM92_PIO_PC4	(1 << 4)
#define AT91RM92_PC4_BFOE	(AT91RM92_PIO_PC4) /* Burst Flash Output Enable */
#define AT91RM92_PIO_PC5	(1 << 5)
#define AT91RM92_PC5_BFWE	(AT91RM92_PIO_PC5) /* Burst Flash Write Enable */
#define AT91RM92_PIO_PC6	(1 << 6)
#define AT91RM92_PC6_NWAIT	(AT91RM92_PIO_PC6) /* NWAIT */
#define AT91RM92_PIO_PC7	(1 << 7)
#define AT91RM92_PC7_A23	(AT91RM92_PIO_PC7) /* Address Bus[23] */
#define AT91RM92_PIO_PC8	(1 << 8)
#define AT91RM92_PC8_A24	(AT91RM92_PIO_PC8) /* Address Bus[24] */
#define AT91RM92_PIO_PC9	(1 << 9)
#define AT91RM92_PC9_A25_CFRNW	(AT91RM92_PIO_PC9) /* Address Bus[25] /  Compact Flash Read Not Write */
#define AT91RM92_PIO_PC10	(1 << 10)
#define AT91RM92_PC10_NCS4_CFCS	(AT91RM92_PIO_PC10) /* Compact Flash Chip Select */
#define AT91RM92_PIO_PC11	(1 << 11)
#define AT91RM92_PC11_NCS5_CFCE1	(AT91RM92_PIO_PC11) /* Chip Select 5 / Compact Flash Chip Enable 1 */
#define AT91RM92_PIO_PC12	(1 << 12)
#define AT91RM92_PC12_NCS6_CFCE2(AT91RM92_PIO_PC12) /* Chip Select 6 / Compact Flash Chip Enable 2 */
#define AT91RM92_PIO_PC13	(1 << 13)
#define AT91RM92_PC13_NCS7	(AT91RM92_PIO_PC13) /* Chip Select 7 */
#define AT91RM92_PIO_PC14	(1 << 14)
#define AT91RM92_PIO_PC15	(1 << 15)
#define AT91RM92_PIO_PC16	(1 << 16)
#define AT91RM92_PC16_D16	(AT91RM92_PIO_PC16) /* Data Bus [16] */
#define AT91RM92_PIO_PC17	(1 << 17)
#define AT91RM92_PC17_D17	(AT91RM92_PIO_PC17) /* Data Bus [17] */
#define AT91RM92_PIO_PC18	(1 << 18)
#define AT91RM92_PC18_D18	(AT91RM92_PIO_PC18) /* Data Bus [18] */
#define AT91RM92_PIO_PC19	(1 << 19)
#define AT91RM92_PC19_D19	(AT91RM92_PIO_PC19) /* Data Bus [19] */
#define AT91RM92_PIO_PC20	(1 << 20)
#define AT91RM92_PC20_D20	(AT91RM92_PIO_PC20) /* Data Bus [20] */
#define AT91RM92_PIO_PC21	(1 << 21)
#define AT91RM92_PC21_D21	(AT91RM92_PIO_PC21) /* Data Bus [21] */
#define AT91RM92_PIO_PC22	(1 << 22)
#define AT91RM92_PC22_D22	(AT91RM92_PIO_PC22) /* Data Bus [22] */
#define AT91RM92_PIO_PC23	(1 << 23)
#define AT91RM92_PC23_D23	(AT91RM92_PIO_PC23) /* Data Bus [23] */
#define AT91RM92_PIO_PC24	(1 << 24)
#define AT91RM92_PC24_D24	(AT91RM92_PIO_PC24) /* Data Bus [24] */
#define AT91RM92_PIO_PC25	(1 << 25)
#define AT91RM92_PC25_D25	(AT91RM92_PIO_PC25) /* Data Bus [25] */
#define AT91RM92_PIO_PC26	(1 << 26)
#define AT91RM92_PC26_D26	(AT91RM92_PIO_PC26) /* Data Bus [26] */
#define AT91RM92_PIO_PC27	(1 << 27)
#define AT91RM92_PC27_D27	(AT91RM92_PIO_PC27) /* Data Bus [27] */
#define AT91RM92_PIO_PC28	(1 << 28)
#define AT91RM92_PC28_D28	(AT91RM92_PIO_PC28) /* Data Bus [28] */
#define AT91RM92_PIO_PC29	(1 << 29)
#define AT91RM92_PC29_D29	(AT91RM92_PIO_PC29) /* Data Bus [29] */
#define AT91RM92_PIO_PC30	(1 << 30)
#define AT91RM92_PC30_D30	(AT91RM92_PIO_PC30) /* Data Bus [30] */
#define AT91RM92_PIO_PC31	(1 << 31)
#define AT91RM92_PC31_D31	(AT91RM92_PIO_PC31) /* Data Bus [31] */

#define AT91RM92_PIO_PD0	(1 << 0)
#define AT91RM92_PD0_ETX0	(AT91RM92_PIO_PD0) /* Ethernet MAC Transmit Data 0 */
#define AT91RM92_PIO_PD1	(1 << 1)
#define AT91RM92_PD1_ETX1	(AT91RM92_PIO_PD1) /* Ethernet MAC Transmit Data 1 */
#define AT91RM92_PIO_PD2	(1 << 2)
#define AT91RM92_PD2_ETX2	(AT91RM92_PIO_PD2) /* Ethernet MAC Transmit Data 2 */
#define AT91RM92_PIO_PD3	(1 << 3)
#define AT91RM92_PD3_ETX3	(AT91RM92_PIO_PD3) /* Ethernet MAC Transmit Data 3 */
#define AT91RM92_PIO_PD4	(1 << 4)
#define AT91RM92_PD4_ETXEN	(AT91RM92_PIO_PD4) /* Ethernet MAC Transmit Enable */
#define AT91RM92_PIO_PD5	(1 << 5)
#define AT91RM92_PD5_ETXER	(AT91RM92_PIO_PD5) /* Ethernet MAC Transmit Coding Error */
#define AT91RM92_PIO_PD6	(1 << 6)
#define AT91RM92_PD6_DTXD	(AT91RM92_PIO_PD6) /* DBGU Debug Transmit Data */
#define AT91RM92_PIO_PD7	(1 << 7)
#define AT91RM92_PD7_PCK0	(AT91RM92_PIO_PD7) /* PMC Programmable Clock Output 0 */
#define AT91RM92_PD7_TSYNC	(AT91RM92_PIO_PD7) /* ETM Synchronization signal */
#define AT91RM92_PIO_PD8	(1 << 8)
#define AT91RM92_PD8_PCK1	(AT91RM92_PIO_PD8) /* PMC Programmable Clock Output 1 */
#define AT91RM92_PD8_TCLK	(AT91RM92_PIO_PD8) /* ETM Trace Clock signal */
#define AT91RM92_PIO_PD9	(1 << 9)
#define AT91RM92_PD9_PCK2	(AT91RM92_PIO_PD9) /* PMC Programmable Clock 2 */
#define AT91RM92_PD9_TPS0	(AT91RM92_PIO_PD9) /* ETM ARM9 pipeline status 0 */
#define AT91RM92_PIO_PD10	(1 << 10)
#define AT91RM92_PD10_PCK3	(AT91RM92_PIO_PD10) /* PMC Programmable Clock Output 3 */
#define AT91RM92_PD10_TPS1	(AT91RM92_PIO_PD10) /* ETM ARM9 pipeline status 1 */
#define AT91RM92_PIO_PD11	(1 << 11)
#define AT91RM92_PD11_TPS2	(AT91RM92_PIO_PD11) /* ETM ARM9 pipeline status 2 */
#define AT91RM92_PIO_PD12	(1 << 12)
#define AT91RM92_PD12_TPK0	(AT91RM92_PIO_PD12) /* ETM Trace Packet 0 */
#define AT91RM92_PIO_PD13	(1 << 13)
#define AT91RM92_PD13_TPK1	(AT91RM92_PIO_PD13) /* ETM Trace Packet 1 */
#define AT91RM92_PIO_PD14	(1 << 14)
#define AT91RM92_PD14_TPK2	(AT91RM92_PIO_PD14) /* ETM Trace Packet 2 */
#define AT91RM92_PIO_PD15	(1 << 15)
#define AT91RM92_PD15_TD0	(AT91RM92_PIO_PD15) /* SSC Transmit data */
#define AT91RM92_PD15_TPK3	(AT91RM92_PIO_PD15) /* ETM Trace Packet 3 */
#define AT91RM92_PIO_PD16	(1 << 16)
#define AT91RM92_PD16_TD1	(AT91RM92_PIO_PD16) /* SSC Transmit Data 1 */
#define AT91RM92_PD16_TPK4	(AT91RM92_PIO_PD16) /* ETM Trace Packet 4 */
#define AT91RM92_PIO_PD17	(1 << 17)
#define AT91RM92_PD17_TD2	(AT91RM92_PIO_PD17) /* SSC Transmit Data 2 */
#define AT91RM92_PD17_TPK5	(AT91RM92_PIO_PD17) /* ETM Trace Packet 5 */
#define AT91RM92_PIO_PD18	(1 << 18)
#define AT91RM92_PD18_NPCS1	(AT91RM92_PIO_PD18) /* SPI Peripheral Chip Select 1 */
#define AT91RM92_PD18_TPK6	(AT91RM92_PIO_PD18) /* ETM Trace Packet 6 */
#define AT91RM92_PIO_PD19	(1 << 19)
#define AT91RM92_PD19_NPCS2	(AT91RM92_PIO_PD19) /* SPI Peripheral Chip Select 2 */
#define AT91RM92_PD19_TPK7	(AT91RM92_PIO_PD19) /* ETM Trace Packet 7 */
#define AT91RM92_PIO_PD20	(1 << 20)
#define AT91RM92_PD20_NPCS3	(AT91RM92_PIO_PD20) /* SPI Peripheral Chip Select 3 */
#define AT91RM92_PD20_TPK8	(AT91RM92_PIO_PD20) /* ETM Trace Packet 8 */
#define AT91RM92_PIO_PD21	(1 << 21)
#define AT91RM92_PD21_RTS0	(AT91RM92_PIO_PD21) /* Usart 0 Ready To Send */
#define AT91RM92_PD21_TPK9	(AT91RM92_PIO_PD21) /* ETM Trace Packet 9 */
#define AT91RM92_PIO_PD22	(1 << 22)
#define AT91RM92_PD22_RTS1	(AT91RM92_PIO_PD22) /* Usart 0 Ready To Send */
#define AT91RM92_PD22_TPK10	(AT91RM92_PIO_PD22) /* ETM Trace Packet 10 */
#define AT91RM92_PIO_PD23	(1 << 23)
#define AT91RM92_PD23_RTS2	(AT91RM92_PIO_PD23) /* USART 2 Ready To Send */
#define AT91RM92_PD23_TPK11	(AT91RM92_PIO_PD23) /* ETM Trace Packet 11 */
#define AT91RM92_PIO_PD24	(1 << 24)
#define AT91RM92_PD24_RTS3	(AT91RM92_PIO_PD24) /* USART 3 Ready To Send */
#define AT91RM92_PD24_TPK12	(AT91RM92_PIO_PD24) /* ETM Trace Packet 12 */
#define AT91RM92_PIO_PD25	(1 << 25)
#define AT91RM92_PD25_DTR1	(AT91RM92_PIO_PD25) /* USART 1 Data Terminal ready */
#define AT91RM92_PD25_TPK13	(AT91RM92_PIO_PD25) /* ETM Trace Packet 13 */
#define AT91RM92_PIO_PD26	(1 << 26)
#define AT91RM92_PD26_TPK14	(AT91RM92_PIO_PD26) /* ETM Trace Packet 14 */
#define AT91RM92_PIO_PD27	(1 << 27)
#define AT91RM92_PD27_TPK15	(AT91RM92_PIO_PD27) /* ETM Trace Packet 15 */

#define AT91C_MASTER_CLOCK	60000000

#endif /* AT91RM92REG_H_ */
