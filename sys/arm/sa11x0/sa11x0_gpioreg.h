/*	$NetBSD: sa11x0_gpioreg.h,v 1.2 2001/07/30 15:58:56 rjs Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org).
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/sa11x0/sa11x0_gpioreg.h,v 1.1 2004/05/14 11:46:45 cognet Exp $
 *
 */

/*
 * SA-11x0 GPIO Register 
 */

#define SAGPIO_NPORTS	8

/* GPIO pin-level register */
#define SAGPIO_PLR	0x00

/* GPIO pin direction register */
#define SAGPIO_PDR	0x04

/* GPIO pin output set register */
#define SAGPIO_PSR	0x08

/* GPIO pin output clear register */
#define SAGPIO_PCR	0x0C

/* GPIO rising-edge detect register */
#define SAGPIO_RER	0x10

/* GPIO falling-edge detect register */
#define SAGPIO_FER	0x14

/* GPIO edge-detect status register */
#define SAGPIO_EDR	0x18

/* GPIO alternate function register */
#define SAGPIO_AFR	0x1C

/* XXX */
#define GPIO(x)		(0x00000001 << (x))

/*
 * SA-11x0 GPIOs parameter
 */
/*
port	name 		desc
0	Reserved
1	Reserved
2...9	LDD{8..15}	LCD DATA(8-15)
10	SSP_TXD		SSP transmit
11	SSP_RXD		SSP receive
12	SSP_SCLK	SSP serial clock
13	SSP_SFRM	SSP frameclock
14	UART_TXD	UART transmit
15	UART_RXD	UART receive
16	GPCLK_OUT	General-purpose clock out
17	Reserved
18	UART_SCLK	Sample clock input
19	SSP_CLK		Sample clock input
20	UART_SCLK3	Sample clock input
21	MCP_CLK		MCP dock in
22	TREQA		Either TIC request A
23	TREQB		Either TIC request B
24	Reserved
25	RTC		Real Time Clock
26	RCLK_OUT	internal clock /2
27	32KHZ_OUT	Raw 32.768kHz osc output
 */
