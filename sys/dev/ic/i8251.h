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
 * $FreeBSD: src/sys/dev/ic/i8251.h,v 1.2.26.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * modified for PC9801 by M.Ishii 
 *			Kyoto University Microcomputer Club (KMC)
 */

/*
 * modified for 8251(FIFO) by Seigo TANIMURA <tanimura@FreeBSD.org>
 */

/* define command and status code */
#define	CMD8251_TxEN	0x01	/* transmit enable */
#define	CMD8251_DTR	0x02	/* assert DTR */
#define	CMD8251_RxEN	0x04	/* receive enable */
#define	CMD8251_SBRK	0x08	/* send break */
#define	CMD8251_ER	0x10	/* error reset */
#define	CMD8251_RTS	0x20	/* assert RTS */
#define	CMD8251_RESET	0x40	/* internal reset */
#define	CMD8251_EH	0x80	/* enter hunt mode (only synchronous mode)*/

#define	STS8251_TxRDY	0x01	/* transmit READY */
#define	STS8251_RxRDY	0x02	/* data exists in receive buffer */
#define	STS8251_TxEMP	0x04	/* transmit buffer EMPTY */
#define	STS8251_PE	0x08	/* perity error */
#define	STS8251_OE	0x10	/* overrun error */
#define	STS8251_FE	0x20	/* framing error */
#define	STS8251_BD_SD	0x40	/* break detect (async) / sync detect (sync) */
#define	STS8251_DSR	0x80	/* DSR is asserted */

#define	STS8251F_TxEMP	0x01	/* transmit buffer EMPTY */
#define	STS8251F_TxRDY	0x02	/* transmit READY */
#define	STS8251F_RxRDY	0x04	/* data exists in receive buffer */
#define	STS8251F_OE	0x10	/* overrun error */
#define	STS8251F_PE	0x20	/* perity error */
#define	STS8251F_BD_SD	0x80	/* break detect (async) / sync detect (sync) */

#define	INTR8251F_DTCT	0x60	/* FIFO detection mask */
#define	INTR8251F_INTRV	0x0e	/* interrupt event */
#define	INTR8251F_TO	0x0c	/* receive timeout */
#define	INTR8251F_LSTS	0x06	/* line status */
#define	INTR8251F_RxRDY	0x04	/* receive READY */
#define	INTR8251F_TxRDY	0x02	/* transmit READY */
#define	INTR8251F_ISEV	0x01	/* event occured */
#define	INTR8251F_MSTS	0x00	/* modem status */

#define	CTRL8251F_ENABLE	0x01	/* enable FIFO */
#define	CTRL8251F_RCV_RST	0x02	/* reset receive FIFO */
#define	CTRL8251F_XMT_RST	0x04	/* reset transmit FIFO */

#define	MOD8251_5BITS	0x00
#define	MOD8251_6BITS	0x04
#define	MOD8251_7BITS	0x08
#define	MOD8251_8BITS	0x0c
#define MOD8251_PDISAB	0x00	/* parity disable */
#define	MOD8251_PODD	0x10	/* parity odd */
#define	MOD8251_PEVEN	0x30	/* parity even */
#define	MOD8251_STOP1	0x40	/* stop bit len = 1bit */
#define	MOD8251_STOP2	0xc0	/* stop bit len = 2bit */
#define	MOD8251_CLKX16	0x02	/* x16 */
#define	MOD8251_CLKX1	0x01	/* x1 */

#define	CICSCD_CD	0x20	/* CD */
#define	CICSCD_CS	0x40	/* CS */
#define	CICSCD_CI	0x80	/* CI */

#define	CICSCDF_CS	0x10	/* CS */
#define	CICSCDF_DR	0x20	/* DR */
#define	CICSCDF_CI	0x40	/* CI */
#define	CICSCDF_CD	0x80	/* CD */

/* interrupt mask control */
#define	IEN_Rx		0x01
#define	IEN_TxEMP	0x02
#define	IEN_Tx		0x04
