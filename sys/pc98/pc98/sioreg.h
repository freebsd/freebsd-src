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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)comreg.h	7.2 (Berkeley) 5/9/91
 *	$Id: sioreg.h,v 1.5 1997/04/05 15:05:16 kato Exp $
 */


/* 16 bit baud rate divisor (lower byte in dca_data, upper in dca_ier) */
#if defined(PC98)
#define	COMBRD(x)	(7372800 / (16*(x)))
#else
#define	COMBRD(x)	(1843200 / (16*(x)))
#endif

/* interrupt enable register */
#define	IER_ERXRDY	0x1
#define	IER_ETXRDY	0x2
#define	IER_ERLS	0x4
#define	IER_EMSC	0x8

/* interrupt identification register */
#define	IIR_IMASK	0xf
#define	IIR_RXTOUT	0xc
#define	IIR_RLS		0x6
#define	IIR_RXRDY	0x4
#define	IIR_TXRDY	0x2
#define	IIR_NOPEND	0x1
#define	IIR_MLSC	0x0
#define	IIR_FIFO_MASK	0xc0	/* set if FIFOs are enabled */

/* fifo control register */
#define	FIFO_ENABLE	0x01
#define	FIFO_RCV_RST	0x02
#define	FIFO_XMT_RST	0x04
#define	FIFO_DMA_MODE	0x08
#define	FIFO_RX_LOW	0x00
#define	FIFO_RX_MEDL	0x40
#define	FIFO_RX_MEDH	0x80
#define	FIFO_RX_HIGH	0xc0

/* character format control register */
#define	CFCR_DLAB	0x80
#define	CFCR_SBREAK	0x40
#define	CFCR_PZERO	0x30
#define	CFCR_PONE	0x20
#define	CFCR_PEVEN	0x10
#define	CFCR_PODD	0x00
#define	CFCR_PENAB	0x08
#define	CFCR_STOPB	0x04
#define	CFCR_8BITS	0x03
#define	CFCR_7BITS	0x02
#define	CFCR_6BITS	0x01
#define	CFCR_5BITS	0x00

/* modem control register */
#define	MCR_LOOPBACK	0x10
#define	MCR_IENABLE	0x08
#define	MCR_DRS		0x04
#define	MCR_RTS		0x02
#define	MCR_DTR		0x01

/* line status register */
#define	LSR_RCV_FIFO	0x80
#define	LSR_TSRE	0x40
#define	LSR_TXRDY	0x20
#define	LSR_BI		0x10
#define	LSR_FE		0x08
#define	LSR_PE		0x04
#define	LSR_OE		0x02
#define	LSR_RXRDY	0x01
#define	LSR_RCV_MASK	0x1f

/* modem status register */
#define	MSR_DCD		0x80
#define	MSR_RI		0x40
#define	MSR_DSR		0x20
#define	MSR_CTS		0x10
#define	MSR_DDCD	0x08
#define	MSR_TERI	0x04
#define	MSR_DDSR	0x02
#define	MSR_DCTS	0x01

/* speed to initialize to during chip tests */
#define	SIO_TEST_SPEED	9600
