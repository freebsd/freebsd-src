/* $NetBSD: uart.h,v 1.1 2007/03/20 08:52:02 dyoung Exp $ */

/*-
 * Copyright (c) 2007 Ruslan Ermilov and Vsevolod Lobko.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef	_ADMUART_H
#define	_ADMUART_H
/* UART registers */
#define	UART_DR_REG	0x00
#define	UART_RSR_REG	0x04
#define		UART_RSR_FE			0x01
#define		UART_RSR_PE			0x02
#define		UART_RSR_BE			0x04
#define		UART_RSR_OE			0x08
#define	UART_ECR_REG	0x04
#define		UART_ECR_RSR			0x80
#define	UART_LCR_H_REG	0x08
#define		UART_LCR_H_FEN			0x10
#define	UART_LCR_M_REG	0x0c
#define	UART_LCR_L_REG	0x10
#define	UART_CR_REG	0x14
#define 	UART_CR_PORT_EN			0x01
#define 	UART_CR_SIREN			0x02
#define 	UART_CR_SIRLP			0x04
#define 	UART_CR_MODEM_STATUS_INT_EN	0x08
#define 	UART_CR_RX_INT_EN		0x10
#define 	UART_CR_TX_INT_EN		0x20
#define 	UART_CR_RX_TIMEOUT_INT_EN	0x40
#define 	UART_CR_LOOPBACK_EN		0x80
#define	UART_FR_REG	0x18
#define		UART_FR_CTS		0x01
#define		UART_FR_DSR		0x02
#define		UART_FR_DCD		0x04
#define		UART_FR_BUSY		0x08
#define		UART_FR_RX_FIFO_EMPTY	0x10
#define		UART_FR_TX_FIFO_FULL	0x20
#define		UART_FR_RX_FIFO_FULL	0x40
#define		UART_FR_TX_FIFO_EMPTY	0x80
#define	UART_IR_REG	0x1c
#define		UART_IR_MODEM_STATUS_INT	0x01
#define		UART_IR_RX_INT			0x02
#define		UART_IR_TX_INT			0x04
#define		UART_IR_RX_TIMEOUT_INT		0x08
#define		UART_IR_INT_MASK		0x0f
#define		UART_IR_UICR			0x80
#define	UART_ILPR_REG	0x20

/* UART interrupts */

int	uart_cnattach(void);
#endif	/* _ADMUART_H */
