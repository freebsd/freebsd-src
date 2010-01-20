/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * modified for PC9801 by M.Ishii 
 *			Kyoto University Microcomputer Club (KMC)
 *
 * modified for 8251(FIFO) by Seigo TANIMURA <tanimura@FreeBSD.org>
 */

/* i8251 mode register */
#define	MOD8251_5BITS	0x00
#define	MOD8251_6BITS	0x04
#define	MOD8251_7BITS	0x08
#define	MOD8251_8BITS	0x0c
#define	MOD8251_PENAB	0x10	/* parity enable */
#define	MOD8251_PEVEN	0x20	/* parity even */
#define	MOD8251_STOP1	0x40	/* 1 stop bit */
#define	MOD8251_STOP15	0x80	/* 1.5 stop bit */
#define	MOD8251_STOP2	0xc0	/* 2 stop bit */
#define	MOD8251_CLKx1	0x01	/* x1 */
#define	MOD8251_CLKx16	0x02	/* x16 */
#define	MOD8251_CLKx64	0x03	/* x64 */

/* i8251 command register */
#define	CMD8251_TxEN	0x01	/* transmit enable */
#define	CMD8251_DTR	0x02	/* assert DTR */
#define	CMD8251_RxEN	0x04	/* receive enable */
#define	CMD8251_SBRK	0x08	/* send break */
#define	CMD8251_ER	0x10	/* error reset */
#define	CMD8251_RTS	0x20	/* assert RTS */
#define	CMD8251_RESET	0x40	/* internal reset */
#define	CMD8251_EH	0x80	/* enter hunt mode */

/* i8251 status register */
#define	STS8251_TxRDY	0x01	/* transmit READY */
#define	STS8251_RxRDY	0x02	/* data exists in receive buffer */
#define	STS8251_TxEMP	0x04	/* transmit buffer EMPTY */
#define	STS8251_PE	0x08	/* perity error */
#define	STS8251_OE	0x10	/* overrun error */
#define	STS8251_FE	0x20	/* framing error */
#define	STS8251_BI	0x40	/* break detect */
#define	STS8251_DSR	0x80	/* DSR is asserted */

/* i8251F line status register */
#define	FLSR_TxEMP	0x01	/* transmit buffer EMPTY */
#define	FLSR_TxRDY	0x02	/* transmit READY */
#define	FLSR_RxRDY	0x04	/* data exists in receive buffer */
#define	FLSR_OE		0x10	/* overrun error */
#define	FLSR_PE		0x20	/* perity error */
#define	FLSR_BI		0x80	/* break detect */

/* i8251F modem status register */
#define	MSR_DCD		0x80	/* Current Data Carrier Detect */
#define	MSR_RI		0x40	/* Current Ring Indicator */
#define	MSR_DSR		0x20	/* Current Data Set Ready */
#define	MSR_CTS		0x10	/* Current Clear to Send */
#define	MSR_DDCD	0x08	/* DCD has changed state */
#define	MSR_TERI	0x04	/* RI has toggled low to high */
#define	MSR_DDSR	0x02	/* DSR has changed state */
#define	MSR_DCTS	0x01	/* CTS has changed state */

/* i8251F interrupt identification register */
#define	IIR_FIFO_CK1	0x40
#define	IIR_FIFO_CK2	0x20
#define	IIR_IMASK	0x0f
#define	IIR_RXTOUT	0x0c	/* Receiver timeout */
#define	IIR_RLS		0x06	/* Line status change */
#define	IIR_RXRDY	0x04	/* Receiver ready */
#define	IIR_TXRDY	0x02	/* Transmitter ready */
#define	IIR_NOPEND	0x01	/* Transmitter ready */
#define	IIR_MLSC	0x00	/* Modem status */

/* i8251F fifo control register */
#define	FIFO_ENABLE	0x01	/* Turn the FIFO on */
#define	FIFO_RCV_RST	0x02	/* Reset RX FIFO */
#define	FIFO_XMT_RST	0x04	/* Reset TX FIFO */
#define	FIFO_LSR_EN	0x08
#define	FIFO_MSR_EN	0x10
#define	FIFO_TRIGGER_1	0x00	/* Trigger RXRDY intr on 1 character */
#define	FIFO_TRIGGER_4	0x40	/* ibid 4 */
#define	FIFO_TRIGGER_8	0x80	/* ibid 8 */
#define	FIFO_TRIGGER_14	0xc0	/* ibid 14 */
