/*-
 * Copyright (c) 2006 Berndt Walter.  All rights reserved.
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 */

/* $FreeBSD$ */

#ifndef ARM_AT91_AT91_MCIREG_H
#define ARM_AT91_AT91_MCIREG_H

#define MMC_MAX		30

#define MCI_CR 		0x00 	/* MCI Control Register */
#define MCI_MR		0x04 	/* MCI Mode Register */
#define MCI_DTOR	0x08 	/* MCI Data Timeout Register */
#define MCI_SDCR	0x0c 	/* MCI SD Card Register */
#define MCI_ARGR	0x10 	/* MCI Argument Register */
#define MCI_CMDR	0x14 	/* MCI Command Register */
#define MCI_RSPR	0x20 	/* MCI Response Registers - 4 of them */
#define MCI_RDR		0x30 	/* MCI Receive Data Register */
#define MCI_TDR		0x34 	/* MCI Transmit Data Register */
#define MCI_SR		0x40 	/* MCI Status Register */
#define MCI_IER		0x44 	/* MCI Interrupt Enable Register */
#define MCI_IDR		0x48 	/* MCI Interrupt Disable Register */
#define MCI_IMR		0x4c 	/* MCI Interrupt Mask Register */

/* -------- MCI_CR : (MCI Offset: 0x0) MCI Control Register --------  */
#define	MCI_CR_MCIEN       (0x1u <<  0) /* (MCI) Multimedia Interface Enable */
#define	MCI_CR_MCIDIS      (0x1u <<  1) /* (MCI) Multimedia Interface Disable */
#define	MCI_CR_PWSEN       (0x1u <<  2) /* (MCI) Power Save Mode Enable */
#define	MCI_CR_PWSDIS      (0x1u <<  3) /* (MCI) Power Save Mode Disable */
#define	MCI_CR_SWRST      (0x1u <<  7) /* (MCI) Software Reset */
/* -------- MCI_MR : (MCI Offset: 0x4) MCI Mode Register --------  */
#define	MCI_MR_CLKDIV      (0xffu <<  0) /* (MCI) Clock Divider */
#define	MCI_MR_PWSDIV      (0x3fu <<  8) /* (MCI) Power Saving Divider */
#define	MCI_MR_RDPROOF	(0x1u << 11)	/* (MCI) Read Proof Enable */
#define	MCI_MR_WRPROOF	(0x1u << 12)	/* (MCI) Write Proof Enable */
#define	MCI_MR_PDCFBYTE	(0x1u << 13)	/* (MCI) PDC Force Byte Transfer */
#define	MCI_MR_PDCPADV     (0x1u << 14) /* (MCI) PDC Padding Value */
#define	MCI_MR_PDCMODE     (0x1u << 15) /* (MCI) PDC Oriented Mode */
#define	MCI_MR_BLKLEN      0x3fff0000ul /* (MCI) Data Block Length */
/* -------- MCI_DTOR : (MCI Offset: 0x8) MCI Data Timeout Register --------  */
#define	MCI_DTOR_DTOCYC      (0xfu <<  0) /* (MCI) Data Timeout Cycle Number */
#define	MCI_DTOR_DTOMUL      (0x7u <<  4) /* (MCI) Data Timeout Multiplier */
#define		MCI_DTOR_DTOMUL_1                    (0x0u <<  4) /* (MCI) DTOCYC x 1 */
#define		MCI_DTOR_DTOMUL_16                   (0x1u <<  4) /* (MCI) DTOCYC x 16 */
#define		MCI_DTOR_DTOMUL_128                  (0x2u <<  4) /* (MCI) DTOCYC x 128 */
#define		MCI_DTOR_DTOMUL_256                  (0x3u <<  4) /* (MCI) DTOCYC x 256 */
#define		MCI_DTOR_DTOMUL_1k                   (0x4u <<  4) /* (MCI) DTOCYC x 1024 */
#define		MCI_DTOR_DTOMUL_4k                   (0x5u <<  4) /* (MCI) DTOCYC x 4096 */
#define		MCI_DTOR_DTOMUL_64k                  (0x6u <<  4) /* (MCI) DTOCYC x 65536 */
#define		MCI_DTOR_DTOMUL_1M                   (0x7u <<  4) /* (MCI) DTOCYC x 1048576 */
/* -------- MCI_SDCR : (MCI Offset: 0xc) MCI SD Card Register --------  */
#define	MCI_SDCR_SDCSEL      (0x1u <<  0) /* (MCI) SD Card Selector */
#define	MCI_SDCR_SDCBUS      (0x1u <<  7) /* (MCI) SD Card Bus Width */
/* -------- MCI_CMDR : (MCI Offset: 0x14) MCI Command Register --------  */
#define	MCI_CMDR_CMDNB       (0x1Fu <<  0) /* (MCI) Command Number */
#define	MCI_CMDR_RSPTYP      (0x3u <<  6) /* (MCI) Response Type */
#define		MCI_CMDR_RSPTYP_NO                   (0x0u <<  6) /* (MCI) No response */
#define		MCI_CMDR_RSPTYP_48                   (0x1u <<  6) /* (MCI) 48-bit response */
#define		MCI_CMDR_RSPTYP_136                  (0x2u <<  6) /* (MCI) 136-bit response */
#define	MCI_CMDR_SPCMD       (0x7u <<  8) /* (MCI) Special CMD */
#define		MCI_CMDR_SPCMD_NONE                 (0x0u <<  8) /* (MCI) Not a special CMD */
#define		MCI_CMDR_SPCMD_INIT                 (0x1u <<  8) /* (MCI) Initialization CMD */
#define		MCI_CMDR_SPCMD_SYNC                 (0x2u <<  8) /* (MCI) Synchronized CMD */
#define		MCI_CMDR_SPCMD_IT_CMD               (0x4u <<  8) /* (MCI) Interrupt command */
#define		MCI_CMDR_SPCMD_IT_REP               (0x5u <<  8) /* (MCI) Interrupt response */
#define	MCI_CMDR_OPDCMD      (0x1u << 11) /* (MCI) Open Drain Command */
#define	MCI_CMDR_MAXLAT      (0x1u << 12) /* (MCI) Maximum Latency for Command to respond */
#define	MCI_CMDR_TRCMD       (0x3u << 16) /* (MCI) Transfer CMD */
#define		MCI_CMDR_TRCMD_NO                   (0x0u << 16) /* (MCI) No transfer */
#define		MCI_CMDR_TRCMD_START                (0x1u << 16) /* (MCI) Start transfer */
#define		MCI_CMDR_TRCMD_STOP                 (0x2u << 16) /* (MCI) Stop transfer */
#define	MCI_CMDR_TRDIR       (0x1u << 18) /* (MCI) Transfer Direction */
#define	MCI_CMDR_TRTYP       (0x3u << 19) /* (MCI) Transfer Type */
#define		MCI_CMDR_TRTYP_BLOCK                (0x0u << 19) /* (MCI) Block Transfer type */
#define		MCI_CMDR_TRTYP_MULTIPLE             (0x1u << 19) /* (MCI) Multiple Block transfer type */
#define		MCI_CMDR_TRTYP_STREAM               (0x2u << 19) /* (MCI) Stream transfer type */
/* -------- MCI_SR : (MCI Offset: 0x40) MCI Status Register --------  */
#define	MCI_SR_CMDRDY   (0x1u <<  0) /* (MCI) Command Ready flag */
#define	MCI_SR_RXRDY    (0x1u <<  1) /* (MCI) RX Ready flag */
#define	MCI_SR_TXRDY    (0x1u <<  2) /* (MCI) TX Ready flag */
#define	MCI_SR_BLKE     (0x1u <<  3) /* (MCI) Data Block Transfer Ended flag */
#define	MCI_SR_DTIP     (0x1u <<  4) /* (MCI) Data Transfer in Progress flag */
#define	MCI_SR_NOTBUSY  (0x1u <<  5) /* (MCI) Data Line Not Busy flag */
#define	MCI_SR_ENDRX    (0x1u <<  6) /* (MCI) End of RX Buffer flag */
#define	MCI_SR_ENDTX    (0x1u <<  7) /* (MCI) End of TX Buffer flag */
#define	MCI_SR_RXBUFF   (0x1u << 14) /* (MCI) RX Buffer Full flag */
#define	MCI_SR_TXBUFE   (0x1u << 15) /* (MCI) TX Buffer Empty flag */
#define	MCI_SR_RINDE    (0x1u << 16) /* (MCI) Response Index Error flag */
#define	MCI_SR_RDIRE    (0x1u << 17) /* (MCI) Response Direction Error flag */
#define	MCI_SR_RCRCE    (0x1u << 18) /* (MCI) Response CRC Error flag */
#define	MCI_SR_RENDE    (0x1u << 19) /* (MCI) Response End Bit Error flag */
#define	MCI_SR_RTOE     (0x1u << 20) /* (MCI) Response Time-out Error flag */
#define	MCI_SR_DCRCE    (0x1u << 21) /* (MCI) data CRC Error flag */
#define	MCI_SR_DTOE     (0x1u << 22) /* (MCI) Data timeout Error flag */
#define	MCI_SR_OVRE     (0x1u << 30) /* (MCI) Overrun flag */
#define	MCI_SR_UNRE     (0x1u << 31) /* (MCI) Underrun flag */

/*	TXRDY,DTIP,ENDTX,TXBUFE,RTOE */

#define MCI_SR_BITSTRING \
	"\020" \
	"\001CMDRDY" \
	"\002RXRDY" \
	"\003TXRDY" \
	"\004BLKE" \
	"\005DTIP" \
	"\006NOTBUSY" \
	"\007ENDRX" \
	"\010ENDTX" \
	"\017RXBUFF" \
	"\020TXBUFE" \
	"\021RINDE" \
	"\022RDIRE" \
	"\023RCRCE" \
	"\024RENDE" \
	"\025RTOE" \
	"\026DCRCE" \
	"\027DTOE" \
	"\037OVRE" \
	"\040UNRE"

/* -------- MCI_IER : (MCI Offset: 0x44) MCI Interrupt Enable Register --------  */
/* -------- MCI_IDR : (MCI Offset: 0x48) MCI Interrupt Disable Register --------  */
/* -------- MCI_IMR : (MCI Offset: 0x4c) MCI Interrupt Mask Register --------  */

#define MCI_SR_ERROR	(MCI_SR_UNRE | MCI_SR_OVRE | MCI_SR_DTOE | \
			MCI_SR_DCRCE | MCI_SR_RTOE | MCI_SR_RENDE | \
			MCI_SR_RCRCE | MCI_SR_RDIRE | MCI_SR_RINDE)

#define AT91C_BUS_WIDTH_1BIT		0x00
#define AT91C_BUS_WIDTH_4BITS		0x02

#endif /* ARM_AT91_AT91_MCIREG_H */
