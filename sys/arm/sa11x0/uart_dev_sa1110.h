/*-
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
 * $FreeBSD: src/sys/arm/sa11x0/uart_dev_sa1110.h,v 1.2 2005/01/05 21:58:48 imp Exp $
 */

#ifndef _DEV_UART_DEV_SA1110_H_
#define _DEV_UART_DEV_SA1110_H_

#define SACOM_FREQ	(3686400 / 16)
#define SACOMSPEED(b)	(SACOM_FREQ / (b) - 1)

/* UART control register 0 */
#define SACOM_CR0	0x00
#define CR0_PE		0x01	/* Parity enable */
#define CR0_OES		0x02	/* Odd/even parity select */
#define CR0_SBS		0x04	/* Stop bit select */
#define CR0_DSS		0x08	/* Data size select */
#define CR0_SCE		0x10	/* Sample clock enable */
#define CR0_RCE		0x20	/* Receive clock edge enable */
#define CR0_TCE		0x40	/* Transmit clock edge enable */

/* UART control register 1 and 2 - baud rate divisor */
#define SACOM_CR1	0x04
#define SACOM_CR2	0x08

/* UART control register 3 */
#define SACOM_CR3	0x0C
#define CR3_RXE		0x01	/* Receiver enable */
#define CR3_TXE		0x02	/* Transmitter enable */
#define CR3_BRK		0x04	/* Break */
#define CR3_RIE		0x08	/* Receive FIFO interrupt enable */
#define CR3_TIE		0x10	/* Transmit FIFO interrupt enable */
#define CR3_LBM		0x20	/* Loopback mode */

/* UART data register */
#define SACOM_DR	0x14
#define DR_PRE		0x100	/* Parity error */
#define DR_FRE		0x200	/* Framing error */
#define DR_ROR		0x400	/* Receiver overrun */

/* UART status register 0 */
#define SACOM_SR0	0x1C
#define SR0_TFS		0x01	/* Transmit FIFO service request */
#define SR0_RFS		0x02	/* Receive FIFO service request */
#define SR0_RID		0x04	/* Receiver idle */
#define SR0_RBB		0x08	/* Receiver begin of break */
#define SR0_REB		0x10	/* Receiver end of break */
#define SR0_EIF		0x20	/* Error in FIFO */

/* UART status register 1 */
#define SACOM_SR1	0x20
#define SR1_TBY		0x01	/* Transmitter busy */
#define SR1_RNE		0x02	/* Receive FIFO not empty */
#define SR1_TNF		0x04	/* Transmit FIFO not full */
#define SR1_PRE		0x08	/* Parity error */
#define SR1_FRE		0x10	/* Framing error */
#define SR1_ROR		0x20	/* Receive FIFO overrun */

#define ISSET(a, b) ((a) & (b))
#endif
