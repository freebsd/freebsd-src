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

#ifndef ARM_AT91_AT91_SPIREG_H
#define ARM_AT91_AT91_SPIREG_H

#define SPI_CR		0x00		/* CR: Control Register */
#define	  SPI_CR_SPIEN		0x1
#define	  SPI_CR_SPIDIS		0x2
#define	  SPI_CR_SWRST		0x8
#define SPI_MR		0x04		/* MR: Mode Register */
#define	  SPI_MR_MSTR		0x01
#define	  SPI_MR_PS		0x02
#define   SPI_MR_PCSDEC		0x04
#define   SPI_MR_DIV32		0x08
#define	  SPI_MR_MODFDIS	0x10
#define   SPI_MR_LLB		0x80
#define   SPI_MR_PSC_CS0	0xe0000
#define   SPI_MR_PSC_CS1	0xd0000
#define	  SPI_MR_PSC_CS2	0xb0000
#define	  SPI_MR_PSC_CS3	0x70000
#define SPI_RDR		0x08		/* RDR: Receive Data Register */
#define SPI_TDR		0x0c		/* TDR: Transmit Data Register */
#define SPI_SR		0x10		/* SR: Status Register */
#define	  SPI_SR_RDRF		0x00001
#define	  SPI_SR_TDRE		0x00002
#define	  SPI_SR_MODF		0x00004
#define	  SPI_SR_OVRES		0x00008
#define	  SPI_SR_ENDRX		0x00010
#define	  SPI_SR_ENDTX		0x00020
#define	  SPI_SR_RXBUFF		0x00040
#define	  SPI_SR_TXBUFE		0x00080
#define	  SPI_SR_SPIENS		0x10000
#define	SPI_IER		0x14		/* IER: Interrupt Enable Regsiter */
#define	SPI_IDR		0x18		/* IDR: Interrupt Disable Regsiter */
#define	SPI_IMR		0x1c		/* IMR: Interrupt Mask Regsiter */
#define SPI_CSR0	0x30		/* CS0: Chip Select 0 */
#define   SPI_CSR_CPOL		0x01
#define SPI_CSR1	0x34		/* CS1: Chip Select 1 */
#define SPI_CSR2	0x38		/* CS2: Chip Select 2 */
#define SPI_CSR3	0x3c		/* CS3: Chip Select 3 */

#endif /* ARM_AT91_AT91_SPIREG_H */
