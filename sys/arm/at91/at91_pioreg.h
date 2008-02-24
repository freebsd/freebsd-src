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

/* $FreeBSD: src/sys/arm/at91/at91_pioreg.h,v 1.2 2007/01/01 00:46:54 imp Exp $ */

#ifndef ARM_AT91_AT91_PIOREG_H
#define ARM_AT91_AT91_PIOREG_H

/* Registers */
#define PIO_PER		0x00		/* PIO Enable Register */
#define PIO_PDR		0x04		/* PIO Disable Register */
#define PIO_PSR		0x08		/* PIO Status Register */
		/*	0x0c		   reserved */
#define PIO_OER		0x10		/* PIO Output Enable Register */
#define PIO_ODR		0x14		/* PIO Output Disable Register */
#define PIO_OSR		0x18		/* PIO Output Status Register */
		/*	0x1c		   reserved */
#define PIO_IFER	0x20		/* PIO Glitch Input Enable Register */
#define PIO_IFDR	0x24		/* PIO Glitch Input Disable Register */
#define PIO_IFSR	0x28		/* PIO Glitch Input Status Register */
		/*	0x2c		   reserved */
#define PIO_SODR	0x30		/* PIO Set Output Data Register */
#define PIO_CODR	0x34		/* PIO Clear Output Data Register */
#define PIO_ODSR	0x38		/* PIO Output Data Status Register */
#define PIO_PDSR	0x3c		/* PIO Pin Data Status Register */
#define PIO_IER		0x40		/* PIO Interrupt Enable Register */
#define PIO_IDR		0x44		/* PIO Interrupt Disable Register */
#define PIO_IMR		0x48		/* PIO Interrupt Mask Register */
#define PIO_ISR		0x4c		/* PIO Interrupt Status Register */
#define PIO_MDER	0x50		/* PIO Multi-Driver Enable Register */
#define PIO_MDDR	0x54		/* PIO Multi-Driver Disable Register */
#define PIO_MDSR	0x58		/* PIO Multi-Driver Status Register */
		/*	0x5c		   reserved */
#define PIO_PUDR	0x60		/* PIO Pull-up Disable Register */
#define PIO_PUER	0x64		/* PIO Pull-up Enable Register */
#define PIO_PUSR	0x68		/* PIO Pull-up Status Register */
		/*	0x6c		   reserved */
#define PIO_ASR		0x70		/* PIO Peripheral A Select Register */
#define PIO_BSR		0x74		/* PIO Peripheral B Select Register */
#define PIO_ABSR	0x78		/* PIO AB Status Register */
		/*	0x7c-0x9c	   reserved */
#define PIO_OWER	0xa0		/* PIO Output Write Enable Register */
#define PIO_OWDR	0xa4		/* PIO Output Write Disable Register */
#define PIO_OWSR	0xa8		/* PIO Output Write Status Register */
		/*	0xac		   reserved */

#endif /* ARM_AT91_AT91_PIOREG_H */
