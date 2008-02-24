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

/* $FreeBSD: src/sys/arm/at91/at91rm92reg.h,v 1.5.2.1 2007/12/02 14:19:37 cognet Exp $ */

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

#define AT91RM92_BASE		0xd0000000
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

/*
 * PIO
 */
#define AT91RM92_PIOA_BASE	0xffff400
#define AT91RM92_PIO_SIZE	0x200
#define AT91RM92_PIOB_BASE	0xffff600
#define AT91RM92_PIOC_BASE	0xffff800
#define AT91RM92_PIOD_BASE	0xffffa00

/*
 * PMC
 */
#define AT91RM92_PMC_BASE	0xffffc00
#define AT91RM92_PMC_SIZE	0x100

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

#define AT91RM92_AIC_BASE	0xffff000
#define AT91RM92_AIC_SIZE	0x200

#define AT91RM92_DBGU_BASE	0xffff200
#define AT91RM92_DBGU_SIZE	0x200

#define AT91RM92_RTC_BASE	0xffffe00
#define AT91RM92_RTC_SIZE	0x100

#define AT91RM92_MC_BASE	0xfffff00
#define AT91RM92_MC_SIZE	0x100

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
#define AT91RM92_TC_SIZE	0x4000
#define AT91RM92_TC0C0_BASE	0xffa0000
#define AT91RM92_TC0C1_BASE	0xffa0040
#define AT91RM92_TC0C2_BASE	0xffa0080

#define AT91RM92_TC1_BASE	0xffa4000
#define AT91RM92_TC1C0_BASE	0xffa4000
#define AT91RM92_TC1C1_BASE	0xffa4040
#define AT91RM92_TC1C2_BASE	0xffa4080

#define AT91RM92_OHCI_BASE	0xdfe00000
#define AT91RM92_OHCI_PA_BASE	0x00300000
#define AT91RM92_OHCI_SIZE	0x00100000

#ifndef AT91C_MASTER_CLOCK
#define AT91C_MASTER_CLOCK	60000000
#endif

/* SDRAMC */

#define AT91RM92_SDRAMC_BASE	0xfffff90
#define AT91RM92_SDRAMC_MR	0x00
#define AT91RM92_SDRAMC_MR_MODE_NORMAL	0
#define AT91RM92_SDRAMC_MR_MODE_NOP	1
#define AT91RM92_SDRAMC_MR_MODE_PRECHARGE 2
#define AT91RM92_SDRAMC_MR_MODE_LOAD_MODE_REGISTER 3
#define AT91RM92_SDRAMC_MR_MODE_REFRESH	4
#define AT91RM92_SDRAMC_MR_DBW_16	0x10
#define AT91RM92_SDRAMC_TR	0x04
#define AT91RM92_SDRAMC_CR	0x08
#define AT91RM92_SDRAMC_CR_NC_8		0x0
#define AT91RM92_SDRAMC_CR_NC_9		0x1
#define AT91RM92_SDRAMC_CR_NC_10	0x2
#define AT91RM92_SDRAMC_CR_NC_11	0x3
#define AT91RM92_SDRAMC_CR_NC_MASK	0x00000003
#define AT91RM92_SDRAMC_CR_NR_11	0x0
#define AT91RM92_SDRAMC_CR_NR_12	0x4
#define AT91RM92_SDRAMC_CR_NR_13	0x8
#define AT91RM92_SDRAMC_CR_NR_RES	0xc
#define AT91RM92_SDRAMC_CR_NR_MASK	0x0000000c
#define AT91RM92_SDRAMC_CR_NB_2		0x00
#define AT91RM92_SDRAMC_CR_NB_4		0x10
#define AT91RM92_SDRAMC_CR_NB_MASK	0x00000010
#define AT91RM92_SDRAMC_CR_NCAS_MASK	0x00000060
#define AT91RM92_SDRAMC_CR_TWR_MASK	0x00000780
#define AT91RM92_SDRAMC_CR_TRC_MASK	0x00007800
#define AT91RM92_SDRAMC_CR_TRP_MASK	0x00078000
#define AT91RM92_SDRAMC_CR_TRCD_MASK	0x00780000
#define AT91RM92_SDRAMC_CR_TRAS_MASK	0x07800000
#define AT91RM92_SDRAMC_CR_TXSR_MASK	0x78000000
#define AT91RM92_SDRAMC_SRR	0x0c
#define AT91RM92_SDRAMC_LPR	0x10
#define AT91RM92_SDRAMC_IER	0x14
#define AT91RM92_SDRAMC_IDR	0x18
#define AT91RM92_SDRAMC_IMR	0x1c
#define AT91RM92_SDRAMC_ISR	0x20
#define AT91RM92_SDRAMC_IER_RES	0x1

#endif /* AT91RM92REG_H_ */
