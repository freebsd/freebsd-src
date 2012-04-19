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

#ifndef ARM_AT91_AT91_TWIREG_H
#define	ARM_AT91_AT91_TWIREG_H

#define	TWI_CR		0x00		/* TWI Control Register */
#define	TWI_MMR		0x04		/* TWI Master Mode Register */
#define	TWI_SMR		0x08		/* TWI Master Mode Register */
#define	TWI_IADR	0x0c		/* TWI Internal Address Register */
#define	TWI_CWGR	0x10		/* TWI Clock Waveform Generator Reg */
		/*	0x14		   reserved */
		/*	0x18		   reserved */
		/*	0x1c		   reserved */
#define	TWI_SR		0x20		/* TWI Status Register */
#define	TWI_IER		0x24		/* TWI Interrupt Enable Register */
#define	TWI_IDR		0x28		/* TWI Interrupt Disable Register */
#define	TWI_IMR		0x2c		/* TWI Interrupt Mask Register */
#define	TWI_RHR		0x30		/* TWI Receiver Holding Register */
#define	TWI_THR		0x34		/* TWI Transmit Holding Register */

/* TWI_CR */
#define	TWI_CR_START	(1U << 0)	/* Send a start */
#define	TWI_CR_STOP	(1U << 1)	/* Send a stop */
#define	TWI_CR_MSEN	(1U << 2)	/* Master Transfer Enable */
#define	TWI_CR_MSDIS	(1U << 3)	/* Master Transfer Disable */
#define	TWI_CR_SVEN	(1U << 4)	/* Slave Transfer Enable */
#define	TWI_CR_SVDIS	(1U << 5)	/* Slave Transfer Disable */
#define	TWI_CR_SWRST	(1U << 7)	/* Software Reset */

/* TWI_MMR */
/* TWI_SMR */
#define	TWI_MMR_IADRSZ(n) ((n) << 8)	/* Set size of transfer */
#define	TWI_MMR_MWRITE	0U		/* Master Read Direction */
#define	TWI_MMR_MREAD	(1U << 12)	/* Master Read Direction */
#define	TWI_MMR_DADR(n)	((n) << 15)	/* Device Address */

/* TWI_CWGR */
#define	TWI_CWGR_CKDIV(x) ((x) << 16)	/* Clock Divider */
#define	TWI_CWGR_CHDIV(x) ((x) << 8)	/* Clock High Divider */
#define	TWI_CWGR_CLDIV(x) ((x) << 0)	/* Clock Low Divider */
#define	TWI_CWGR_DIV(rate) 		 		\
	(at91_is_sam9() || at91_is_sam9xe() ?		\
	    ((at91_master_clock / (4 * (rate))) - 3) :	\
	    ((at91_master_clock / (4 * (rate))) - 2))

/* TWI_SR */
/* TWI_IER */
/* TWI_IDR */
/* TWI_IMR */
#define	TWI_SR_TXCOMP	(1U << 0)	/* Transmission Completed */
#define	TWI_SR_RXRDY	(1U << 1)	/* Receive Holding Register Ready */
#define	TWI_SR_TXRDY	(1U << 2)	/* Transmit Holding Register Ready */
#define	TWI_SR_SVREAD	(1U << 3)	/* Slave Read */
#define	TWI_SR_SVACC	(1U << 4)	/* Slave Access */
#define	TWI_SR_GCACC	(1U << 5)	/* General Call Access */
#define	TWI_SR_OVRE	(1U << 6)	/* Overrun error */
#define	TWI_SR_UNRE	(1U << 7)	/* Underrun Error */
#define	TWI_SR_NACK	(1U << 8)	/* Not Acknowledged */
#define	TWI_SR_ARBLST	(1U << 9)	/* Arbitration Lost */

#endif /* ARM_AT91_AT91_TWIREG_H */
