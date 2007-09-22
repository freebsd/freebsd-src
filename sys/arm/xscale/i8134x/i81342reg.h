/*-
 * Copyright (c) 2006 Olivier Houchard
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef I83142_REG_H_
#define I83142_REG_H_
/* Physical Memory Map */
/* 
 * 0x000000000 - 0x07FFFFFFF SDRAM
 * 0x090100000 - 0x0901FFFFF ATUe Outbound IO Window
 * 0x0F0000000 - 0x0F1FFFFFF Flash
 * 0x0F2000000 - 0x0F20FFFFF PCE1
 * 0x0F3000000 - 0x0FFCFFFFF Compact Flash
 * 0x0FFD00000 - 0x0FFDFFFFF MMR
 * 0x0FFFB0000 - 0x0FFFBFFFF ATU-X Outbound I/O Window
 * 0x0FFFD0000 - 0x0FFFDFFFF ATUe Outbound I/O Window
 * 0x100000000 - 0x1FFFFFFFF ATU-X outbound Memory Translation Window
 * 0x2FF000000 - 0x2FFFFFFFF ATUe Outbound Memory Translation Window
 */

#define IOP34X_VADDR		0xf0000000
#define IOP34X_HWADDR		0xffd00000
#define IOP34X_SIZE		0x100000

#define IOP34X_ADMA0_OFFSET	0x00080000
#define IOP34X_ADMA1_OFFSET	0x00080200
#define IOP34X_ADMA2_OFFSET	0x00080400
#define IOP34X_ADMA_SIZE	0x200


/* ADMA Registers */
#define IOP34X_ADMA_CCR		0x0000 /* Channel Control Register */
#define IOP34X_ADMA_CSR		0x0004 /* Channel Status Register */
#define IOP34X_ADMA_DAR		0x0008 /* Descriptor Address Register */
#define IOP34X_ADMA_IPCR	0x0018 /* Internal Interface Parity Ctrl Reg */
#define IOP34X_ADMA_NDAR	0x0024 /* Next Descriptor Register */
#define IOP34X_ADMA_DCR		0x0028 /* Descriptor Control Register */

#define	IOP34X_ADMA_IE		(1 << 0) /* Interrupt enable */
#define IOP34X_ADMA_TR		(1 << 1) /* Transfert Direction */
/* 
 *               Source                   Destination
 *  00         Host I/O Interface	Local Memory
 *  01         Local Memory             Host I/O Interface
 *  10         Internal Bus             Local Memory
 *  11         Local Memory             Internal Bus
 */
#define IOP34X_ADMA_SS		(1 << 3) /* Source selection */
/* 0000: Data Transfer / CRC / Memory Block Fill */
#define IOP34X_ADMA_ZRBCE	(1 << 7) /* Zero Result Buffer Check Enable */
#define IOP34X_ADMA_MBFE	(1 << 8) /* Memory Block Fill Enable */
#define IOP34X_ADMA_CGE		(1 << 9) /* CRC Generation enable */
#define IOP34X_ADMA_CTD		(1 << 10) /* CRC Transfer disable */
#define IOP34X_ADMA_CSFD	(1 << 11) /* CRC Seed fetch disable */
#define IOP34X_ADMA_SWBE	(1 << 12) /* Status write back enable */
#define IOP34X_ADMA_ESE		(1 << 13) /* Endian swap enable */
#define IOP34X_ADMA_PQUTE	(1 << 16) /* P+Q Update Transfer Enable */
#define IOP34X_ADMA_DXE		(1 << 17) /* Dual XOR Enable */
#define IOP34X_ADMA_PQTE	(1 << 18) /* P+Q Transfer Enable */
#define IOP34X_ADMA_PTD		(1 << 19) /* P Transfer Disable */
#define IOP34X_ADMA_ROE		(1 << 30) /* Relaxed Ordering Enable */
#define IOP34X_ADMA_NSE		(1 << 31) /* No Snoop Enable */

#define IOP34X_PBBAR0		0x81588 /* PBI Base Address Register 0 */
#define IOP34X_PBBAR0_ADDRMASK	0xfffff000
#define IOP34X_PBBAR1		0x81590
#define IOP34X_PCE1		0xF2000000
#define IOP34X_PCE1_SIZE	0x00100000
#define IOP34X_PCE1_VADDR	0xF1000000
#define IOP34X_ESSTSR0		0x82188
#define IOP34X_CONTROLLER_ONLY	(1 << 14)
#define IOP34X_INT_SEL_PCIX	(1 << 15)
#define IOP34X_PFR		0x82180 /* Processor Frequency Register */
#define IOP34X_FREQ_MASK	((1 << 16) | (1 << 17) | (1 << 18))
#define IOP34X_FREQ_600		(0)
#define IOP34X_FREQ_667		(1 << 16)
#define IOP34X_FREQ_800		(1 << 17)
#define IOP34X_FREQ_833		((1 << 17) | (1 << 16))
#define IOP34X_FREQ_1000	(1 << 18)
#define IOP34X_FREQ_1200	((1 << 16) | (1 << 18))

#define IOP34X_UART0_VADDR	IOP34X_VADDR + 0x82300
#define IOP34X_UART0_HWADDR	IOP34X_HWADDR + 0x82300
#define IOP34X_UART1_VADDR	IOP34X_VADDR + 0x82340
#define IOP34X_UART1_HWADDR	IOP34X_HWADDR + 0x82340
#define IOP34X_PBI_HWADDR	0xffd81580

/* SDRAM Memory Controller */
#define SMC_SDBR		0x8180c /* Base Register */
#define SMC_SDBR_BASEADDR	(1 << 27)
#define SMC_SDBR_BASEADDR_MASK	((1 << 27) | (1 << 28) | (1 << 29) | (1 << 30) \
    				| (1 << 31))
#define SMC_SDUBR		0x81810 /* Upper Base Register */
#define SMC_SBSR		0x81814 /* SDRAM Bank Size Register */
#define SMC_SBSR_BANK_NB	(1 << 2) /* Number of DDR Banks
					   0 => 2 Banks
					   1 => 1 Bank
					   */
#define SMC_SBSR_BANK_SZ	(1 << 27) /* SDRAM Bank Size :
					   0x00000 Empty
					   0x00001 128MB
					   0x00010 256MB
					   0x00100 512MB
					   0x01000 1GB
					   */
#define SMC_SBSR_BANK_SZ_MASK	((1 << 27) | (1 << 28) | (1 << 29) | (1 << 30) \
    				| (1 << 31))


/* Two possible addresses for ATUe depending on configuration. */
#define IOP34X_ATUE_ADDR(esstrsr) ((((esstrsr) & (IOP34X_CONTROLLER_ONLY | \
    IOP34X_INT_SEL_PCIX)) == (IOP34X_CONTROLLER_ONLY | IOP34X_INT_SEL_PCIX)) ? \
    0xffdc8000 : 0xffdcd000)

/* Three possible addresses for ATU-X depending on configuration. */
#define IOP34X_ATUX_ADDR(esstrsr) (!((esstrsr) & IOP34X_CONTROLLER_ONLY) ? \
    0xffdcc000 : !((esstrsr) & IOP34X_INT_SEL_PCIX) ? 0xffdc8000 : 0xffdcd000)

#define IOP34X_OIOBAR_SIZE		0x10000
#define IOP34X_PCIX_OIOBAR		0xfffb0000
#define IOP34X_PCIX_OIOBAR_VADDR	0xf01b0000
#define IOP34X_PCIX_OMBAR		0x100000000
#define IOP34X_PCIE_OIOBAR		0xfffd0000
#define IOP34X_PCIE_OIOBAR_VADDR	0xf01d0000
#define IOP34X_PCIE_OMBAR		0x200000000

/* ATU Registers */
/* Common for ATU-X and ATUe */
#define ATU_VID		0x0000 /* ATU Vendor ID */
#define ATU_DID		0x0002 /* ATU Device ID */
#define ATU_CMD		0x0004 /* ATU Command Register */
#define ATU_SR		0x0006 /* ATU Status Register */
#define ATU_RID		0x0008 /* ATU Revision ID */
#define ATU_CCR		0x0009 /* ATU Class Code */
#define ATU_CLSR	0x000c /* ATU Cacheline Size */
#define ATU_LT		0x000d /* ATU Latency Timer */
#define ATU_HTR		0x000e /* ATU Header Type */
#define ATU_BISTR	0x000f /* ATU BIST Register */
#define ATU_IABAR0	0x0010 /* Inbound ATU Base Address register 0 */
#define ATU_IAUBAR0	0x0014 /* Inbound ATU Upper Base Address Register 0 */
#define ATU_IABAR1	0x0018 /* Inbound ATU Base Address Register 1 */
#define ATU_IAUBAR1	0x001c /* Inbound ATU Upper Base Address Register 1 */
#define ATU_IABAR2	0x0020 /* Inbound ATU Base Address Register 2 */
#define ATU_IAUBAR2	0x0024 /* Inbound ATU Upper Base Address Register 2 */
#define ATU_VSIR	0x002c /* ATU Subsystem Vendor ID Register */
#define ATU_SIR		0x002e /* ATU Subsystem ID Register */
#define ATU_ERBAR	0x0030 /* Expansion ROM Base Address Register */
#define ATU_CAPPTR	0x0034 /* ATU Capabilities Pointer Register */
#define ATU_ILR		0x003c /* ATU Interrupt Line Register */
#define ATU_IPR		0x003d /* ATU Interrupt Pin Register */
#define ATU_MGNT	0x003e /* ATU Minimum Grand Register */
#define ATU_MLAT	0x003f /* ATU Maximum Latency Register */
#define ATU_IALR0	0x0040 /* Inbound ATU Limit Register 0 */
#define ATU_IATVR0	0x0044 /* Inbound ATU Translate Value Register 0 */
#define ATU_IAUTVR0	0x0048 /* Inbound ATU Upper Translate Value Register 0*/
#define ATU_IALR1	0x004c /* Inbound ATU Limit Register 1 */
#define ATU_IATVR1	0x0050 /* Inbound ATU Translate Value Register 1 */
#define ATU_IAUTVR1	0x0054 /* Inbound ATU Upper Translate Value Register 1*/
#define ATU_IALR2	0x0058 /* Inbound ATU Limit Register 2 */
#define ATU_IATVR2	0x005c /* Inbound ATU Translate Value Register 2 */
#define ATU_IAUTVR2	0x0060 /* Inbound ATU Upper Translate Value Register 2*/
#define ATU_ERLR	0x0064 /* Expansion ROM Limit Register */
#define ATU_ERTVR	0x0068 /* Expansion ROM Translater Value Register */
#define ATU_ERUTVR	0x006c /* Expansion ROM Upper Translate Value Register*/
#define ATU_CR		0x0070 /* ATU Configuration Register */
#define ATU_CR_OUT_EN	(1 << 1)
#define ATU_PCSR	0x0074 /* PCI Configuration and Status Register */
#define PCIE_BUSNO(x)	((x & 0xff000000) >> 24)
#define ATUX_CORE_RST	((1 << 30) | (1 << 31)) /* Core Processor Reset */
#define ATUX_P_RSTOUT	(1 << 21) /* Central Resource PCI Bus Reset */
#define ATUE_CORE_RST	((1 << 9) | (1 << 8)) /* Core Processor Reset */
#define ATU_ISR		0x0078 /* ATU Interrupt Status Register */
#define ATUX_ISR_PIE	(1 << 18) /* PCI Interface error */
#define ATUX_ISR_IBPR	(1 << 16) /* Internal Bus Parity Error */
#define ATUX_ISR_DCE	(1 << 14) /* Detected Correctable error */
#define ATUX_ISR_ISCE	(1 << 13) /* Initiated Split Completion Error Msg */
#define ATUX_ISR_RSCE	(1 << 12) /* Received Split Completion Error Msg */
#define ATUX_ISR_DPE	(1 << 9)  /* Detected Parity Error */
#define ATUX_ISR_IBMA	(1 << 7)  /* Internal Bus Master Abort */
#define ATUX_ISR_PMA	(1 << 3)  /* PCI Master Abort */
#define ATUX_ISR_PTAM	(1 << 2)  /* PCI Target Abort (Master) */
#define ATUX_ISR_PTAT	(1 << 1)  /* PCI Target Abort (Target) */
#define ATUX_ISR_PMPE	(1 << 0)  /* PCI Master Parity Error */
#define ATUX_ISR_ERRMSK	(ATUX_ISR_PIE | ATUX_ISR_IBPR | ATUX_ISR_DCE | \
    ATUX_ISR_ISCE | ATUX_ISR_RSCE | ATUX_ISR_DPE | ATUX_ISR_IBMA | ATUX_ISR_PMA\
    | ATUX_ISR_PTAM | ATUX_ISR_PTAT | ATUX_ISR_PMPE)
#define ATUE_ISR_HON	(1 << 13) /* Halt on Error Interrupt */
#define ATUE_ISR_RSE	(1 << 12) /* Root System Error Message */
#define ATUE_ISR_REM	(1 << 11) /* Root Error Message */
#define ATUE_ISR_PIE	(1 << 10) /* PCI Interface error */
#define ATUE_ISR_CEM	(1 << 9)  /* Correctable Error Message */
#define ATUE_ISR_UEM	(1 << 8)  /* Uncorrectable error message */
#define ATUE_ISR_CRS	(1 << 7)  /* Received Configuration Retry Status */
#define ATUE_ISR_IBMA	(1 << 5)  /* Internal Bus Master Abort */
#define ATUE_ISR_DPE	(1 << 4)  /* Detected Parity Error Interrupt */
#define ATUE_ISR_MAI	(1 << 3)  /* Received Master Abort Interrupt */
#define ATUE_ISR_STAI	(1 << 2)  /* Signaled Target Abort Interrupt */
#define ATUE_ISR_TAI	(1 << 1)  /* Received Target Abort Interrupt */
#define ATUE_ISR_MDPE	(1 << 0)  /* Master Data Parity Error Interrupt */
#define ATUE_ISR_ERRMSK	(ATUE_ISR_HON | ATUE_ISR_RSE | ATUE_ISR_REM | \
    ATUE_ISR_PIE | ATUE_ISR_CEM | ATUE_ISR_UEM | ATUE_ISR_CRS | ATUE_ISR_IBMA |\
    ATUE_ISR_DPE | ATUE_ISR_MAI | ATUE_ISR_STAI | ATUE_ISR_TAI | ATUE_ISR_MDPE)
#define ATU_IMR		0x007c /* ATU Interrupt Mask Register */
/* 0x0080 - 0x008f reserved */
#define ATU_VPDCID	0x0090 /* VPD Capability Identifier Register */
#define ATU_VPDNIP	0x0091 /* VPD Next Item Pointer Register */
#define ATU_VPDAR	0x0092 /* VPD Address Register */
#define ATU_VPDDR	0x0094 /* VPD Data Register */
#define ATU_PMCID	0x0098 /* PM Capability Identifier Register */
#define ATU_PMNIPR	0x0099 /* PM Next Item Pointer Register */
#define ATU_PMCR	0x009a /* ATU Power Management Capabilities Register */
#define ATU_PMCSR	0x009c /* ATU Power Management Control/Status Register*/
#define ATU_MSICIR	0x00a0 /* MSI Capability Identifier Register */
#define ATU_MSINIPR	0x00a1 /* MSI Next Item Pointer Register */
#define ATU_MCR		0x00a2 /* Message Control Register */
#define ATU_MAR		0x00a4 /* Message Address Register */
#define ATU_MUAR	0x00a8 /* Message Upper Address Register */
#define ATU_MDR		0x00ac /* Message Data Register */
#define ATU_PCIXSR	0x00d4 /* PCI-X Status Register */
#define PCIXSR_BUSNO(x)         (((x) & 0xff00) >> 8)
#define ATU_IABAR3	0x0200 /* Inbound ATU Base Address Register 3 */
#define ATU_IAUBAR3	0x0204 /* Inbound ATU Upper Base Address Register 3 */
#define ATU_IALR3	0x0208 /* Inbound ATU Limit Register 3 */
#define ATU_ITVR3	0x020c /* Inbound ATU Upper Translate Value Reg 3 */
#define ATU_OIOBAR	0x0300 /* Outbound I/O Base Address Register */
#define ATU_OIOWTVR	0x0304 /* Outbound I/O Window Translate Value Reg */
#define ATU_OUMBAR0	0x0308 /* Outbound Upper Memory Window base addr reg 0*/
#define ATU_OUMBAR_FUNC	(28)
#define ATU_OUMBAR_EN	(1 << 31)
#define ATU_OUMWTVR0	0x030c /* Outbound Upper 32bit Memory Window Translate Value Register 0 */
#define ATU_OUMBAR1	0x0310 /* Outbound Upper Memory Window base addr reg1*/
#define ATU_OUMWTVR1	0x0314 /* Outbound Upper 32bit Memory Window Translate Value Register 1 */
#define ATU_OUMBAR2	0x0318 /* Outbound Upper Memory Window base addr reg2*/
#define ATU_OUMWTVR2	0x031c /* Outbount Upper 32bit Memory Window Translate Value Register 2 */
#define ATU_OUMBAR3	0x0320 /* Outbound Upper Memory Window base addr reg3*/
#define ATU_OUMWTVR3	0x0324 /* Outbound Upper 32bit Memory Window Translate Value Register 3 */

/* ATU-X specific */
#define ATUX_OCCAR	0x0330 /* Outbound Configuration Cycle Address Reg */
#define ATUX_OCCDR	0x0334 /* Outbound Configuration Cycle Data Reg */
#define ATUX_OCCFN	0x0338 /* Outbound Configuration Cycle Function Number*/
/* ATUe specific */
#define ATUE_OCCAR	0x032c /* Outbound Configuration Cycle Address Reg */
#define ATUE_OCCDR	0x0330 /* Outbound Configuration Cycle Data Reg */
#define ATUE_OCCFN	0x0334 /* Outbound Configuration Cycle Function Number*/
/* Interrupts */

/* IINTRSRC0 */
#define ICU_INT_ADMA0_EOT	(0) /* ADMA 0 End of transfer */
#define ICU_INT_ADMA0_EOC	(1) /* ADMA 0 End of Chain */
#define ICU_INT_ADMA1_EOT	(2) /* ADMA 1 End of transfer */
#define ICU_INT_ADMA1_EOC	(3) /* ADMA 1 End of chain */
#define ICU_INT_ADMA2_EOT	(4) /* ADMA 2 End of transfer */
#define ICU_INT_ADMA2_EOC	(5) /* ADMA 2 end of chain */
#define ICU_INT_WDOG		(6) /* Watchdog timer */
/* 7 Reserved */
#define ICU_INT_TIMER0		(8) /* Timer 0 */
#define ICU_INT_TIMER1		(9) /* Timer 1 */
#define ICU_INT_I2C0		(10) /* I2C bus interface 0 */
#define ICU_INT_I2C1		(11) /* I2C bus interface 1 */
#define ICU_INT_MU		(12) /* Message Unit */
#define ICU_INT_MU_IPQ		(13) /* Message unit inbound post queue */
#define ICU_INT_ATUE_IM		(14) /* ATU-E inbound message */
#define ICU_INT_ATU_BIST	(15) /* ATU/Start BIST */
#define ICU_INT_PMC		(16) /* PMC */
#define ICU_INT_PMU		(17) /* PMU */
#define ICU_INT_PC		(18) /* Processor cache */
/* 19-23 Reserved */
#define ICU_INT_XINT0		(24)
#define ICU_INT_XINT1		(25)
#define ICU_INT_XINT2		(26)
#define ICU_INT_XINT3		(27)
#define ICU_INT_XINT4		(28)
#define ICU_INT_XINT5		(29)
#define ICU_INT_XINT6		(30)
#define ICU_INT_XINT7		(31)
/* IINTSRC1 */
#define ICU_INT_XINT8		(32)
#define ICU_INT_XINT9		(33)
#define ICU_INT_XINT10		(34)
#define ICU_INT_XINT11		(35)
#define ICU_INT_XINT12		(36)
#define ICU_INT_XINT13		(37)
#define ICU_INT_XINT14		(38)
#define ICU_INT_XINT15		(39)
/* 40-50 reserved */
#define ICU_INT_UART0		(51) /* UART 0 */
#define ICU_INT_UART1		(52) /* UART 1 */
#define ICU_INT_PBIUE		(53) /* Peripheral bus interface unit error */
#define ICU_INT_ATUCRW		(54) /* ATU Configuration register write */
#define ICU_INT_ATUE		(55) /* ATU error */
#define ICU_INT_MCUE		(56) /* Memory controller unit error */
#define ICU_INT_ADMA0E		(57) /* ADMA Channel 0 error */
#define ICU_INT_ADMA1E		(58) /* ADMA Channel 1 error */
#define ICU_INT_ADMA2E		(59) /* ADMA Channel 2 error */
/* 60-61 reserved */
#define ICU_INT_MUE		(62) /* Messaging Unit Error */
/* 63 reserved */

/* IINTSRC2 */
#define ICU_INT_IP		(64) /* Inter-processor */
/* 65-93 reserved */
#define ICU_INT_SIBBE		(94) /* South internal bus bridge error */
/* 95 reserved */

/* IINTSRC3 */
#define ICU_INT_I2C2		(96) /* I2C bus interface 2 */
#define ICU_INT_ATUE_BIST	(97) /* ATU-E/Start BIST */
#define ICU_INT_ATUE_CRW	(98) /* ATU-E Configuration register write */
#define ICU_INT_ATUEE		(99) /* ATU-E Error */
#define ICU_INT_IMU		(100) /* IMU */
/* 101-106 reserved */
#define ICU_INT_ATUE_MA		(107) /* ATUE Interrupt message A */
#define ICU_INT_ATUE_MB		(108) /* ATUE Interrupt message B */
#define ICU_INT_ATUE_MC		(109) /* ATUE Interrupt message C */
#define ICU_INT_ATUE_MD		(110) /* ATUE Interrupt message D */
#define ICU_INT_MU_MSIX_TW	(111) /* MU MSI-X Table write */
/* 112 reserved */
#define ICU_INT_IMSI		(113) /* Inbound MSI */
/* 114-126 reserved */
#define ICU_INT_HPI		(127) /* HPI */


#endif /* I81342_REG_H_ */
