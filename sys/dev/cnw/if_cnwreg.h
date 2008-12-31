/*	$NetBSD: if_cnwreg.h,v 1.3 2000/07/05 18:42:19 itojun Exp $	*/
/* $FreeBSD: src/sys/dev/cnw/if_cnwreg.h,v 1.1.32.1 2008/11/25 02:59:29 kensmith Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Eriksson.
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
 */


/* I/O area */
#define CNW_IO_SIZE		0x10
/* I/O area can be accessed via mapped memory too */
#define CNW_IOM_ADDR		0x28000
#define CNW_IOM_SIZE		CNW_IO_SIZE
#define CNW_IOM_OFF		(CNW_IOM_ADDR - CNW_MEM_ADDR)

/* I/O registers */
#define CNW_REG_COR		0x0
#    define CNW_COR_IENA		0x01	/* Interrupt enable */
#    define CNW_COR_LVLREQ		0x40	/* Keep high */
#define CNW_REG_CCSR		0x2
#define CNW_REG_ASR		0x4
#    define CNW_ASR_TXBA		0x01	/* Trasmit buffer available */
#    define CNW_ASR_WOC			0x08	/* Write Operation Complete */
#    define CNW_ASR_TXDN		0x20	/* Transmit done */
#    define CNW_ASR_RXERR		0x40	/* Receive error */
#    define CNW_ASR_RXRDY		0x80	/* Packet received */ 
#define CNW_REG_IOLOW		0x6
#define CNW_REG_IOHI		0x7
#define CNW_REG_IOCONTROL	0x8
#define CNW_REG_IMR		0xa
#    define CNW_IMR_IENA		0x02	/* Interrupt enable */
#    define CNW_IMR_RFU1		0x10	/* RFU intr mask, keep high */
#define CNW_REG_PMR		0xc
#    define CNW_PMR_RESET		0x80
#define CNW_REG_DATA		0xf


/* Mapped memory */
#define CNW_MEM_ADDR		0x20000
#define CNW_MEM_SIZE		0x8000

/* Extended I/O registers (memory mapped) */
#define CNW_EREG_CB		0x100
#define CNW_EREG_ASCC		0x114
#define CNW_EREG_RSER		0x120
#    define CNW_RSER_RXBIG		0x02
#    define CNW_RSER_RXCRC		0x04
#    define CNW_RSER_RXOVERRUN		0x08
#    define CNW_RSER_RXOVERFLOW		0x10
#    define CNW_RSER_RXERR		0x40
#    define CNW_RSER_RXAVAIL		0x80
#define CNW_EREG_RSERW		0x124
#define CNW_EREG_TSER		0x130
#    define CNW_TSER_RTRY		0x0f
#    define CNW_TSER_TXERR		0x10
#    define CNW_TSER_TXOK		0x20
#    define CNW_TSER_TXNOAP		0x40
#    define CNW_TSER_TXGU		0x80
#    define CNW_TSER_ERROR		(CNW_TSER_TXERR | CNW_TSER_TXNOAP | \
					 CNW_TSER_TXGU)
#define CNW_EREG_TSERW		0x134
#define CNW_EREG_TDP		0x140
#define CNW_EREG_LIF		0x14e
#define CNW_EREG_RDP		0x150
#define CNW_EREG_SPCQ		0x154
#define CNW_EREG_SPU		0x155
#define CNW_EREG_ISPLQ		0x156
#define CNW_EREG_HHC		0x158
#define CNW_EREG_PA		0x160
#define CNW_EREG_ARW		0x166
#define CNW_EREG_MHS		0x16b
#define CNW_EREG_NI		0x16e
#define CNW_EREG_CRBP		0x17a
#define CNW_EREG_EC		0x180
#define CNW_EREG_STAT_RXERR	0x184
#define CNW_EREG_STAT_FRAME	0x186
#define CNW_EREG_STAT_IBEAT	0x188
#define CNW_EREG_STAT_RXBUF	0x18e
#define CNW_EREG_STAT_RXMULTI	0x190
#define CNW_EREG_STAT_TXRETRY	0x192
#define CNW_EREG_STAT_TXABORT	0x194
#define CNW_EREG_STAT_OBEAT	0x198
#define CNW_EREG_STAT_TXOK	0x19a
#define CNW_EREG_STAT_TXSENT	0x19c

/*
 * Commands used in the extended command buffer
 * CNW_EREG_CB (0x100-0x10f) 
 */
#define CNW_CMD_NOP		0x00
#define CNW_CMD_SRC		0x01
#    define CNW_RXCONF_RXENA		0x80	/* Receive Enable */
#    define CNW_RXCONF_MAC		0x20	/* MAC host receive mode */
#    define CNW_RXCONF_PRO		0x10	/* Promiscuous */
#    define CNW_RXCONF_AMP		0x08	/* Accept Multicast Packets */
#    define CNW_RXCONF_BCAST		0x04	/* Accept Broadcast Packets */
#define CNW_CMD_STC		0x02
#    define CNW_TXCONF_TXENA		0x80	/* Transmit Enable */
#    define CNW_TXCONF_MAC		0x20	/* Host sends MAC mode */
#    define CNW_TXCONF_EUD		0x10	/* Enable Uni-Data packets */
#    define CNW_TXCONF_KEY		0x02	/* Scramble data packets */
#    define CNW_TXCONF_LOOP		0x01	/* Loopback mode */
#define CNW_CMD_AMA		0x03
#define CNW_CMD_DMA		0x04
#define CNW_CMD_SAMA		0x05
#define CNW_CMD_ER		0x06
#define CNW_CMD_DR		0x07
#define CNW_CMD_TL		0x08
#define CNW_CMD_SRP		0x09
#define CNW_CMD_SSK		0x0a
#define CNW_CMD_SMD		0x0b
#define CNW_CMD_SAPD		0x0c
#define CNW_CMD_SSS		0x11
#define CNW_CMD_EOC		0x00		/* End-of-command marker */

