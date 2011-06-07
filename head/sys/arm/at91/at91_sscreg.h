/*-
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

#ifndef ARM_AT91_AT91_SSCREG_H
#define ARM_AT91_AT91_SSCREG_H

/* Registers */
#define	SSC_CR		0x00		/* Control Register */
#define	SSC_CMR		0x04		/* Clock Mode Register */
		/*	0x08		Reserved */
		/*	0x0c		Reserved */
#define	SSC_RCMR	0x10		/* Receive Clock Mode Register */
#define	SSC_RFMR	0x14		/* Receive Frame Mode Register */
#define	SSC_TCMR	0x18		/* Transmit Clock Mode Register */
#define	SSC_TFMR	0x1c		/* Transmit Frame Mode register */
#define	SSC_RHR		0x20		/* Receive Holding Register */
#define	SSC_THR		0x24		/* Transmit Holding Register */
		/*	0x28		Reserved */
		/*	0x2c		Reserved */
#define	SSC_RSHR	0x30		/* Receive Sync Holding Register */
#define	SSC_TSHR	0x34		/* Transmit Sync Holding Register */
		/*	0x38		Reserved */
		/*	0x3c		Reserved */
#define	SSC_SR		0x40		/* Status Register */
#define	SSC_IER		0x44		/* Interrupt Enable Register */
#define	SSC_IDR		0x48		/* Interrupt Disable Register */
#define	SSC_IMR		0x4c		/* Interrupt Mask Register */
/* And PDC registers */

/* SSC_CR */
#define	SSC_CR_RXEN	(1u << 0)	/* RXEN: Receive Enable */
#define	SSC_CR_RXDIS	(1u << 1)	/* RXDIS: Receive Disable */
#define	SSC_CR_TXEN	(1u << 8)	/* TXEN: Transmit Enable */
#define	SSC_CR_TXDIS	(1u << 9)	/* TXDIS: Transmit Disable */
#define	SSC_CR_SWRST	(1u << 15)	/* SWRST: Software Reset */

/* SSC_CMR */
#define	SSC_CMR_DIV	0xfffu		/* DIV: Clock Divider mask */

/* SSC_RCMR */
#define	SSC_RCMR_PERIOD	(0xffu << 24)	/* PERIOD: Receive Period Divider sel*/
#define	SSC_RCMR_STTDLY	(0xffu << 16)	/* STTDLY: Receive Start Delay */
#define	SSC_RCMR_START	(0xfu << 8)	/* START: Receive Start Sel */
#define		SSC_RCMR_START_CONT		(0u << 8)
#define		SSC_RCMR_START_TX_START		(1u << 8)
#define		SSC_RCMR_START_LOW_RF		(2u << 8)
#define		SSC_RCMR_START_HIGH_RF		(3u << 8)
#define		SSC_RCMR_START_FALL_EDGE_RF	(4u << 8)
#define		SSC_RCMR_START_RISE_EDGE_RF	(5u << 8)
#define		SSC_RCMR_START_LEVEL_CHANGE_RF	(6u << 8)
#define		SSC_RCMR_START_ANY_EDGE_RF	(7u << 8)
#define	SSC_RCMR_CKI	(1u << 5)	/* CKI: Receive Clock Inversion */
#define	SSC_RCMR_CKO	(7u << 2)	/* CKO: Receive Clock Output Mode Sel*/
#define		SSC_RCMR_CKO_NONE		(0u << 2)
#define		SSC_RCMR_CKO_CONTINUOUS		(1u << 2)
#define	SSC_RCMR_CKS	(3u)	       	/* CKS: Receive Clock Selection */
#define		SSC_RCMR_CKS_DIVIDED		(0)
#define		SSC_RCMR_CKS_TK_CLOCK		(1)
#define		SSC_RCMR_CKS_RK			(2)

/* SSC_RFMR */
#define	SSC_RFMR_FSEDGE	(1u << 24)	/* FSEDGE: Frame Sync Edge Detection */
#define	SSC_RFMR_FSOS	(7u << 20)	/* FSOS: Receive frame Sync Out sel */
#define		SSC_RFMR_FSOS_NONE		(0u << 20)
#define		SSC_RFMR_FSOS_NEG_PULSE		(1u << 20)
#define		SSC_RFMR_FSOS_POS_PULSE		(2u << 20)
#define		SSC_RFMR_FSOS_LOW		(3u << 20)
#define		SSC_RFMR_FSOS_HIGH		(4u << 20)
#define		SSC_RFMR_FSOS_TOGGLE		(5u << 20)
#define	SSC_RFMR_FSLEN	(0xfu << 16)	/* FSLEN: Receive Frame Sync Length */
#define	SSC_RFMR_DATNB	(0xfu << 8)	/* DATNB: Data Number per Frame */
#define	SSC_RFMR_MSFBF	(1u << 7)	/* MSBF: Most Significant Bit First */
#define	SSC_RFMR_LOOP	(1u << 5)	/* LOOP: Loop Mode */
#define	SSC_RFMR_DATLEN	(0x1fu << 0)	/* DATLEN: Data Length */

/* SSC_TCMR */
#define	SSC_TCMR_PERIOD	(0xffu << 24)	/* PERIOD: Receive Period Divider sel*/
#define	SSC_TCMR_STTDLY	(0xffu << 16)	/* STTDLY: Receive Start Delay */
#define	SSC_TCMR_START	(0xfu << 8)	/* START: Receive Start Sel */
#define		SSC_TCMR_START_CONT		(0u << 8)
#define		SSC_TCMR_START_RX_START		(1u << 8)
#define		SSC_TCMR_START_LOW_RF		(2u << 8)
#define		SSC_TCMR_START_HIGH_RF		(3u << 8)
#define		SSC_TCMR_START_FALL_EDGE_RF	(4u << 8)
#define		SSC_TCMR_START_RISE_EDGE_RF	(5u << 8)
#define		SSC_TCMR_START_LEVEL_CHANGE_RF	(6u << 8)
#define		SSC_TCMR_START_ANY_EDGE_RF	(7u << 8)
#define	SSC_TCMR_CKI	(1u << 5)	/* CKI: Receive Clock Inversion */
#define	SSC_TCMR_CKO	(7u << 2)	/* CKO: Receive Clock Output Mode Sel*/
#define		SSC_TCMR_CKO_NONE		(0u << 2)
#define		SSC_TCMR_CKO_CONTINUOUS		(1u << 2)
#define	SSC_TCMR_CKS	(3u)	       	/* CKS: Receive Clock Selection */
#define		SSC_TCMR_CKS_DIVIDED		(0)
#define		SSC_TCMR_CKS_RK_CLOCK		(1)
#define		SSC_TCMR_CKS_TK			(2)

/* SSC_TFMR */
#define	SSC_TFMR_FSEDGE	(1u << 24)	/* FSEDGE: Frame Sync Edge Detection */
#define	SSC_TFMR_FSOS	(7u << 20)	/* FSOS: Receive frame Sync Out sel */
#define		SSC_TFMR_FSOS_NONE		(0u << 20)
#define		SSC_TFMR_FSOS_NEG_PULSE		(1u << 20)
#define		SSC_TFMR_FSOS_POS_PULSE		(2u << 20)
#define		SSC_TFMR_FSOS_LOW		(3u << 20)
#define		SSC_TFMR_FSOS_HIGH		(4u << 20)
#define		SSC_TFMR_FSOS_TOGGLE		(5u << 20)
#define	SSC_TFMR_FSLEN	(0xfu << 16)	/* FSLEN: Receive Frame Sync Length */
#define	SSC_TFMR_DATNB	(0xfu << 8)	/* DATNB: Data Number per Frame */
#define	SSC_TFMR_MSFBF	(1u << 7)	/* MSBF: Most Significant Bit First */
#define	SSC_TFMR_DATDEF	(1u << 5)	/* DATDEF: Data Default Value */
#define	SSC_TFMR_DATLEN	(0x1fu << 0)	/* DATLEN: Data Length */

/* SSC_SR */
#define	SSC_SR_TXRDY	(1u << 0)
#define	SSC_SR_TXEMPTY	(1u << 1)
#define	SSC_SR_ENDTX	(1u << 2)
#define	SSC_SR_TXBUFE	(1u << 3)
#define	SSC_SR_RXRDY	(1u << 4)
#define	SSC_SR_OVRUN	(1u << 5)
#define	SSC_SR_ENDRX	(1u << 6)
#define	SSC_SR_RXBUFF	(1u << 7)
#define	SSC_SR_TXSYN	(1u << 10)
#define	SSC_SR_RSSYN	(1u << 11)
#define	SSC_SR_TXEN	(1u << 16)
#define	SSC_SR_RXEN	(1u << 17)

#endif /* ARM_AT91_AT91_SSCREG_H */
