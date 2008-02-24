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

/* $FreeBSD: src/sys/arm/at91/at91_pdcreg.h,v 1.1 2006/05/11 14:30:28 cognet Exp $ */

#ifndef ARM_AT91_AT91_PDCREG_H
#define ARM_AT91_AT91_PDCREG_H

#define PDC_RPR		0x100		/* PDC Receive Pointer Register */
#define PDC_RCR		0x104		/* PDC Receive Counter Register */
#define PDC_TPR		0x108		/* PDC Transmit Pointer Register */
#define PDC_TCR		0x10c		/* PDC Transmit Counter Register */
#define PDC_RNPR	0x110		/* PDC Receive Next Pointer Register */
#define PDC_RNCR	0x114		/* PDC Receive Next Counter Register */
#define PDC_TNPR	0x118		/* PDC Transmit Next Pointer Reg */
#define PDC_TNCR	0x11c		/* PDC Transmit Next Counter Reg */
#define PDC_PTCR	0x120		/* PDC Transfer Control Register */
#define PDC_PTSR	0x124		/* PDC Transfer Status Register */

/* PTCR/PTSR */
#define PDC_PTCR_RXTEN	(1UL << 0)	/* RXTEN: Receiver Transfer Enable */
#define PDC_PTCR_RXTDIS	(1UL << 1)	/* RXTDIS: Receiver Transfer Disable */
#define PDC_PTCR_TXTEN	(1UL << 8)	/* TXTEN: Transmitter Transfer En */
#define PDC_PTCR_TXTDIS	(1UL << 9)	/* TXTDIS: Transmitter Transmit Dis */

#endif /* ARM_AT91_AT91_PDCREG_H */
