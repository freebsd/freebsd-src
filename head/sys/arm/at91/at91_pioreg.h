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

#define AT91C_PIO_PA0        ((unsigned int) 1 <<  0) // Pin Controlled by PA0
#define AT91C_PIO_PA1        ((unsigned int) 1 <<  1) // Pin Controlled by PA1
#define AT91C_PIO_PA2        ((unsigned int) 1 <<  2) // Pin Controlled by PA2
#define AT91C_PIO_PA3        ((unsigned int) 1 <<  3) // Pin Controlled by PA3
#define AT91C_PIO_PA4        ((unsigned int) 1 <<  4) // Pin Controlled by PA4
#define AT91C_PIO_PA5        ((unsigned int) 1 <<  5) // Pin Controlled by PA5
#define AT91C_PIO_PA6        ((unsigned int) 1 <<  6) // Pin Controlled by PA6
#define AT91C_PIO_PA7        ((unsigned int) 1 <<  7) // Pin Controlled by PA7
#define AT91C_PIO_PA8        ((unsigned int) 1 <<  8) // Pin Controlled by PA8
#define AT91C_PIO_PA9        ((unsigned int) 1 <<  9) // Pin Controlled by PA9
#define AT91C_PIO_PA10       ((unsigned int) 1 << 10) // Pin Controlled by PA10
#define AT91C_PIO_PA11       ((unsigned int) 1 << 11) // Pin Controlled by PA11
#define AT91C_PIO_PA12       ((unsigned int) 1 << 12) // Pin Controlled by PA12
#define AT91C_PIO_PA13       ((unsigned int) 1 << 13) // Pin Controlled by PA13
#define AT91C_PIO_PA14       ((unsigned int) 1 << 14) // Pin Controlled by PA14
#define AT91C_PIO_PA15       ((unsigned int) 1 << 15) // Pin Controlled by PA15
#define AT91C_PIO_PA16       ((unsigned int) 1 << 16) // Pin Controlled by PA16
#define AT91C_PIO_PA17       ((unsigned int) 1 << 17) // Pin Controlled by PA17
#define AT91C_PIO_PA18       ((unsigned int) 1 << 18) // Pin Controlled by PA18
#define AT91C_PIO_PA19       ((unsigned int) 1 << 19) // Pin Controlled by PA19
#define AT91C_PIO_PA20       ((unsigned int) 1 << 20) // Pin Controlled by PA20
#define AT91C_PIO_PA21       ((unsigned int) 1 << 21) // Pin Controlled by PA21
#define AT91C_PIO_PA22       ((unsigned int) 1 << 22) // Pin Controlled by PA22
#define AT91C_PIO_PA23       ((unsigned int) 1 << 23) // Pin Controlled by PA23
#define AT91C_PIO_PA24       ((unsigned int) 1 << 24) // Pin Controlled by PA24
#define AT91C_PIO_PA25       ((unsigned int) 1 << 25) // Pin Controlled by PA25
#define AT91C_PIO_PA26       ((unsigned int) 1 << 26) // Pin Controlled by PA26
#define AT91C_PIO_PA27       ((unsigned int) 1 << 27) // Pin Controlled by PA27
#define AT91C_PIO_PA28       ((unsigned int) 1 << 28) // Pin Controlled by PA28
#define AT91C_PIO_PA29       ((unsigned int) 1 << 29) // Pin Controlled by PA29
#define AT91C_PIO_PA30       ((unsigned int) 1 << 30) // Pin Controlled by PA30
#define AT91C_PIO_PA31       ((unsigned int) 1 << 31) // Pin Controlled by PA31
#define AT91C_PIO_PB0        ((unsigned int) 1 <<  0) // Pin Controlled by PB0
#define AT91C_PIO_PB1        ((unsigned int) 1 <<  1) // Pin Controlled by PB1
#define AT91C_PIO_PB2        ((unsigned int) 1 <<  2) // Pin Controlled by PB2
#define AT91C_PIO_PB3        ((unsigned int) 1 <<  3) // Pin Controlled by PB3
#define AT91C_PIO_PB4        ((unsigned int) 1 <<  4) // Pin Controlled by PB4
#define AT91C_PIO_PB5        ((unsigned int) 1 <<  5) // Pin Controlled by PB5
#define AT91C_PIO_PB6        ((unsigned int) 1 <<  6) // Pin Controlled by PB6
#define AT91C_PIO_PB7        ((unsigned int) 1 <<  7) // Pin Controlled by PB7
#define AT91C_PIO_PB8        ((unsigned int) 1 <<  8) // Pin Controlled by PB8
#define AT91C_PIO_PB9        ((unsigned int) 1 <<  9) // Pin Controlled by PB9
#define AT91C_PIO_PB10       ((unsigned int) 1 << 10) // Pin Controlled by PB10
#define AT91C_PIO_PB11       ((unsigned int) 1 << 11) // Pin Controlled by PB11
#define AT91C_PIO_PB12       ((unsigned int) 1 << 12) // Pin Controlled by PB12
#define AT91C_PIO_PB13       ((unsigned int) 1 << 13) // Pin Controlled by PB13
#define AT91C_PIO_PB14       ((unsigned int) 1 << 14) // Pin Controlled by PB14
#define AT91C_PIO_PB15       ((unsigned int) 1 << 15) // Pin Controlled by PB15
#define AT91C_PIO_PB16       ((unsigned int) 1 << 16) // Pin Controlled by PB16
#define AT91C_PIO_PB17       ((unsigned int) 1 << 17) // Pin Controlled by PB17
#define AT91C_PIO_PB18       ((unsigned int) 1 << 18) // Pin Controlled by PB18
#define AT91C_PIO_PB19       ((unsigned int) 1 << 19) // Pin Controlled by PB19
#define AT91C_PIO_PB20       ((unsigned int) 1 << 20) // Pin Controlled by PB20
#define AT91C_PIO_PB21       ((unsigned int) 1 << 21) // Pin Controlled by PB21
#define AT91C_PIO_PB22       ((unsigned int) 1 << 22) // Pin Controlled by PB22
#define AT91C_PIO_PB23       ((unsigned int) 1 << 23) // Pin Controlled by PB23
#define AT91C_PIO_PB24       ((unsigned int) 1 << 24) // Pin Controlled by PB24
#define AT91C_PIO_PB25       ((unsigned int) 1 << 25) // Pin Controlled by PB25
#define AT91C_PIO_PB26       ((unsigned int) 1 << 26) // Pin Controlled by PB26
#define AT91C_PIO_PB27       ((unsigned int) 1 << 27) // Pin Controlled by PB27
#define AT91C_PIO_PB28       ((unsigned int) 1 << 28) // Pin Controlled by PB28
#define AT91C_PIO_PB29       ((unsigned int) 1 << 29) // Pin Controlled by PB29
#define AT91C_PIO_PB30       ((unsigned int) 1 << 30) // Pin Controlled by PB30
#define AT91C_PIO_PB31       ((unsigned int) 1 << 31) // Pin Controlled by PB31
#define AT91C_PIO_PC0        ((unsigned int) 1 <<  0) // Pin Controlled by PC0
#define AT91C_PIO_PC1        ((unsigned int) 1 <<  1) // Pin Controlled by PC1
#define AT91C_PIO_PC2        ((unsigned int) 1 <<  2) // Pin Controlled by PC2
#define AT91C_PIO_PC3        ((unsigned int) 1 <<  3) // Pin Controlled by PC3
#define AT91C_PIO_PC4        ((unsigned int) 1 <<  4) // Pin Controlled by PC4
#define AT91C_PIO_PC5        ((unsigned int) 1 <<  5) // Pin Controlled by PC5
#define AT91C_PIO_PC6        ((unsigned int) 1 <<  6) // Pin Controlled by PC6
#define AT91C_PIO_PC7        ((unsigned int) 1 <<  7) // Pin Controlled by PC7
#define AT91C_PIO_PC8        ((unsigned int) 1 <<  8) // Pin Controlled by PC8
#define AT91C_PIO_PC9        ((unsigned int) 1 <<  9) // Pin Controlled by PC9
#define AT91C_PIO_PC10       ((unsigned int) 1 << 10) // Pin Controlled by PC10
#define AT91C_PIO_PC11       ((unsigned int) 1 << 11) // Pin Controlled by PC11
#define AT91C_PIO_PC12       ((unsigned int) 1 << 12) // Pin Controlled by PC12
#define AT91C_PIO_PC13       ((unsigned int) 1 << 13) // Pin Controlled by PC13
#define AT91C_PIO_PC14       ((unsigned int) 1 << 14) // Pin Controlled by PC14
#define AT91C_PIO_PC15       ((unsigned int) 1 << 15) // Pin Controlled by PC15
#define AT91C_PIO_PC16       ((unsigned int) 1 << 16) // Pin Controlled by PC16
#define AT91C_PIO_PC17       ((unsigned int) 1 << 17) // Pin Controlled by PC17
#define AT91C_PIO_PC18       ((unsigned int) 1 << 18) // Pin Controlled by PC18
#define AT91C_PIO_PC19       ((unsigned int) 1 << 19) // Pin Controlled by PC19
#define AT91C_PIO_PC20       ((unsigned int) 1 << 20) // Pin Controlled by PC20
#define AT91C_PIO_PC21       ((unsigned int) 1 << 21) // Pin Controlled by PC21
#define AT91C_PIO_PC22       ((unsigned int) 1 << 22) // Pin Controlled by PC22
#define AT91C_PIO_PC23       ((unsigned int) 1 << 23) // Pin Controlled by PC23
#define AT91C_PIO_PC24       ((unsigned int) 1 << 24) // Pin Controlled by PC24
#define AT91C_PIO_PC25       ((unsigned int) 1 << 25) // Pin Controlled by PC25
#define AT91C_PIO_PC26       ((unsigned int) 1 << 26) // Pin Controlled by PC26
#define AT91C_PIO_PC27       ((unsigned int) 1 << 27) // Pin Controlled by PC27
#define AT91C_PIO_PC28       ((unsigned int) 1 << 28) // Pin Controlled by PC28
#define AT91C_PIO_PC29       ((unsigned int) 1 << 29) // Pin Controlled by PC29
#define AT91C_PIO_PC30       ((unsigned int) 1 << 30) // Pin Controlled by PC30
#define AT91C_PIO_PC31       ((unsigned int) 1 << 31) // Pin Controlled by PC31

#endif /* ARM_AT91_AT91_PIOREG_H */
