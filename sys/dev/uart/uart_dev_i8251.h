/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 *
 * $FreeBSD$
 */

#ifndef _DEV_UART_DEV_I8251_H_
#define _DEV_UART_DEV_I8251_H_

/* Enhanced Feature Register. */
#define	EFR_CTS		0x80
#define	EFR_RTS		0x40
#define	EFR_SCD		0x20		/* Special Character Detect. */
#define	EFR_EFC		0x10		/* Enhanced Function Control. */
#define	EFR_SFC_MASK	0x0f		/* Software Flow Control. */
#define	EFR_SFC_TX12	0x0c		/* BIT: Transmit XON1+2/XOFF1+2. */
#define	EFR_SFC_TX1	0x08		/* BIT: Transmit XON1/XOFF1. */
#define	EFR_SFC_TX2	0x04		/* BIT: Transmit XON2/XOFF2. */
#define	EFR_SFC_RX1	0x02		/* BIT: Receive XON1/XOFF1. */
#define	EFR_SFC_RX2	0x01		/* BIT: Receive XON2/XOFF2. */
#define	EFR_SFC_T12R12	0x0f		/* VAL: TX 1+2, RX 1+2. */
#define	EFR_SFC_T1R12	0x0b		/* VAL: TX 1, RX 1+2. */
#define	EFR_SFC_T2R12	0x07		/* VAL: TX 2, RX 1+2. */

/* FIFO Control Register. */
#define	FCR_RX_HIGH	0xc0
#define	FCR_RX_MEDH	0x80
#define	FCR_RX_MEDL	0x40
#define	FCR_RX_LOW	0x00
#define	FCR_TX_HIGH	0x30
#define	FCR_TX_MEDH	0x20
#define	FCR_TX_LOW	0x10
#define	FCR_TX_MEDL	0x00
#define	FCR_DMA		0x08
#define	FCR_XMT_RST	0x04
#define	FCR_RCV_RST	0x02
#define	FCR_ENABLE	0x01

/* Interrupt Enable Register. */
#define	IER_CTS		0x80
#define	IER_RTS		0x40
#define	IER_XOFF	0x20
#define	IER_SLEEP	0x10
#define	IER_EMSC	0x08
#define	IER_ERLS	0x04
#define	IER_ETXRDY	0x02
#define	IER_ERXRDY	0x01

/* Interrupt Identification Register. */
#define	IIR_FIFO_MASK	0xc0
#define	IIR_RTSCTS	0x20
#define	IIR_XOFF	0x10
#define	IIR_IMASK	0x0f
#define	IIR_RXTOUT	0x0c
#define	IIR_RLS		0x06
#define	IIR_RXRDY	0x04
#define	IIR_TXRDY	0x02
#define	IIR_MLSC	0x00
#define	IIR_NOPEND	0x01

/* Line Control Register. */
#define	LCR_DLAB	0x80
#define	LCR_SBREAK	0x40
#define	LCR_PZERO	0x30
#define	LCR_PONE	0x20
#define	LCR_PEVEN	0x10
#define	LCR_PODD	0x00
#define	LCR_PENAB	0x08
#define	LCR_STOPB	0x04
#define	LCR_8BITS	0x03
#define	LCR_7BITS	0x02
#define	LCR_6BITS	0x01
#define	LCR_5BITS	0x00

/* Line Status Register. */
#define	LSR_DERR	0x80
#define	LSR_TEMT	0x40	/* Transmitter Empty. */
#define	LSR_THRE	0x20	/* Transmitter Holding Register Empty. */
#define	LSR_BI		0x10
#define	LSR_FE		0x08
#define	LSR_PE		0x04
#define	LSR_OE		0x02
#define	LSR_RXRDY	0x01

/* Modem Control Register. */
#define	MCR_CS		0x80
#define	MCR_IRE		0x40
#define	MCR_ISEL	0x20
#define	MCR_LOOPBACK	0x10
#define	MCR_IE		0x08
#define	MCR_LBDCD	MCR_IE
#define	MCR_LBRI	0x04
#define	MCR_RTS		0x02
#define	MCR_DTR		0x01

/* Modem Status Register. */
#define	MSR_DCD		0x80
#define	MSR_RI		0x40
#define	MSR_DSR		0x20
#define	MSR_CTS		0x10
#define	MSR_DDCD	0x08
#define	MSR_TERI	0x04
#define	MSR_DDSR	0x02
#define	MSR_DCTS	0x01

/* General registers. */
#define	REG_DATA	0		/* Data Register. */
#define	REG_RBR		REG_DATA	/* Receiver Buffer Register (R). */
#define	REG_THR		REG_DATA	/* Transmitter Holding Register (W). */
#define	REG_IER		1		/* Interrupt Enable Register */
#define	REG_IIR		2		/* Interrupt Ident. Register (R). */
#define	REG_FCR		2		/* FIFO Control Register (W). */
#define	REG_LCR		3		/* Line Control Register. */
#define	REG_MCR		4		/* Modem Control Register. */
#define	REG_LSR		5		/* Line Status Register. */
#define	REG_MSR		6		/* Modem Status Register. */
#define	REG_SPR		7		/* Scratch Pad Register. */

/* Baudrate registers (LCR[7] = 1). */
#define	REG_DLBL	0		/* Divisor Latch (LSB). */
#define	REG_DLBH	1		/* Divisor Latch (MSB). */
#define	REG_DL		REG_DLBL	/* Divisor Latch (16-bit I/O). */

/* Enhanced registers (LCR = 0xBF). */
#define	REG_EFR		2		/* Enhanced Feature Register. */
#define	REG_XON1	4		/* XON character 1. */
#define	REG_XON2	5		/* XON character 2. */
#define	REG_XOFF1	6		/* XOFF character 1. */
#define	REG_XOFF2	7		/* XOFF character 2. */

#endif /* _DEV_UART_DEV_I8251_H_ */
