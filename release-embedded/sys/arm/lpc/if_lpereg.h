/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef	_ARM_LPC_IF_LPEREG_H
#define	_ARM_LPC_IF_LPEREG_H

#define	LPE_MAC1		0x000
#define	LPE_MAC1_RXENABLE	(1 << 0)
#define	LPE_MAC1_PASSALL	(1 << 1)
#define	LPE_MAC1_RXFLOWCTRL	(1 << 2)
#define	LPE_MAC1_TXFLOWCTRL	(1 << 3)
#define	LPE_MAC1_LOOPBACK	(1 << 4)
#define	LPE_MAC1_RESETTX	(1 << 8)
#define	LPE_MAC1_RESETMCSTX	(1 << 9)
#define	LPE_MAC1_RESETRX	(1 << 10)
#define	LPE_MAC1_RESETMCSRX	(1 << 11)
#define	LPE_MAC1_SIMRESET	(1 << 14)
#define	LPE_MAC1_SOFTRESET	(1 << 15)
#define	LPE_MAC2		0x004
#define	LPE_MAC2_FULLDUPLEX	(1 << 0)
#define	LPE_MAC2_FRAMELENCHECK	(1 << 1)
#define	LPE_MAC2_HUGEFRAME	(1 << 2)
#define	LPE_MAC2_DELAYEDCRC	(1 << 3)
#define	LPE_MAC2_CRCENABLE	(1 << 4)
#define	LPE_MAC2_PADCRCENABLE	(1 << 5)
#define	LPE_MAC2_VLANPADENABLE	(1 << 6)
#define	LPE_MAC2_AUTOPADENABLE	(1 << 7)
#define	LPE_MAC2_PUREPREAMBLE	(1 << 8)
#define	LPE_MAC2_LONGPREAMBLE	(1 << 9)
#define	LPE_MAC2_NOBACKOFF	(1 << 12)
#define	LPE_MAC2_BACKPRESSURE	(1 << 13)
#define	LPE_MAC2_EXCESSDEFER	(1 << 14)
#define	LPE_IPGT		0x008
#define	LPE_IPGR		0x00c
#define	LPE_CLRT		0x010
#define	LPE_MAXF		0x014
#define	LPE_SUPP		0x018
#define	LPE_SUPP_SPEED		(1 << 8)
#define	LPE_TEST		0x01c
#define	LPE_MCFG		0x020
#define	LPE_MCFG_SCANINCR	(1 << 0)
#define	LPE_MCFG_SUPPREAMBLE	(1 << 1)
#define	LPE_MCFG_CLKSEL(_n)	((_n & 0x7) << 2)
#define	LPC_MCFG_RESETMII	(1 << 15)
#define	LPE_MCMD		0x024
#define	LPE_MCMD_READ		(1 << 0)
#define	LPE_MCMD_WRITE		(0 << 0)
#define	LPE_MCMD_SCAN		(1 << 1)
#define	LPE_MADR		0x028
#define	LPE_MADR_REGMASK	0x1f
#define	LPE_MADR_REGSHIFT	0
#define	LPE_MADR_PHYMASK	0x1f
#define	LPE_MADR_PHYSHIFT	8
#define	LPE_MWTD		0x02c
#define	LPE_MWTD_DATAMASK	0xffff
#define	LPE_MRDD		0x030
#define	LPE_MRDD_DATAMASK	0xffff
#define	LPE_MIND		0x034
#define	LPE_MIND_BUSY		(1 << 0)
#define	LPE_MIND_SCANNING	(1 << 1)
#define	LPE_MIND_INVALID	(1 << 2)
#define	LPE_MIND_MIIFAIL	(1 << 3)
#define	LPE_SA0			0x040
#define	LPE_SA1			0x044
#define	LPE_SA2			0x048
#define	LPE_COMMAND		0x100
#define	LPE_COMMAND_RXENABLE	(1 << 0)
#define	LPE_COMMAND_TXENABLE	(1 << 1)
#define	LPE_COMMAND_REGRESET	(1 << 3)
#define	LPE_COMMAND_TXRESET	(1 << 4)
#define	LPE_COMMAND_RXRESET	(1 << 5)
#define	LPE_COMMAND_PASSRUNTFRAME	(1 << 6)
#define	LPE_COMMAND_PASSRXFILTER	(1 << 7)
#define	LPE_COMMAND_TXFLOWCTL		(1 << 8)
#define	LPE_COMMAND_RMII		(1 << 9)
#define	LPE_COMMAND_FULLDUPLEX		(1 << 10)
#define	LPE_STATUS		0x104
#define	LPE_STATUS_RXACTIVE		(1 << 0)
#define	LPE_STATUS_TXACTIVE		(1 << 1)
#define	LPE_RXDESC		0x108
#define	LPE_RXSTATUS		0x10c
#define	LPE_RXDESC_NUMBER	0x110
#define	LPE_RXDESC_PROD		0x114
#define	LPE_RXDESC_CONS		0x118
#define	LPE_TXDESC		0x11c
#define	LPE_TXSTATUS		0x120
#define	LPE_TXDESC_NUMBER	0x124
#define	LPE_TXDESC_PROD		0x128
#define	LPE_TXDESC_CONS		0x12c
#define	LPE_TSV0		0x158
#define	LPE_TSV1		0x15c
#define	LPE_RSV			0x160
#define	LPE_FLOWCONTROL_COUNTER	0x170
#define	LPE_FLOWCONTROL_STATUS	0x174
#define	LPE_RXFILTER_CTRL	0x200
#define	LPE_RXFILTER_UNICAST	(1 << 0)
#define	LPE_RXFILTER_BROADCAST	(1 << 1)
#define LPE_RXFILTER_MULTICAST	(1 << 2)
#define	LPE_RXFILTER_UNIHASH	(1 << 3)
#define	LPE_RXFILTER_MULTIHASH	(1 << 4)
#define	LPE_RXFILTER_PERFECT	(1 << 5)
#define	LPE_RXFILTER_WOL	(1 << 12)
#define	LPE_RXFILTER_FILTWOL	(1 << 13)
#define	LPE_RXFILTER_WOL_STATUS	0x204
#define	LPE_RXFILTER_WOL_CLEAR	0x208
#define	LPE_HASHFILTER_L	0x210
#define	LPE_HASHFILTER_H	0x214
#define	LPE_INTSTATUS		0xfe0
#define	LPE_INTENABLE		0xfe4
#define	LPE_INTCLEAR		0xfe8
#define	LPE_INTSET		0xfec
#define	LPE_INT_RXOVERRUN	(1 << 0)
#define	LPE_INT_RXERROR		(1 << 1)
#define	LPE_INT_RXFINISH	(1 << 2)
#define	LPE_INT_RXDONE		(1 << 3)
#define	LPE_INT_TXUNDERRUN	(1 << 4)
#define	LPE_INT_TXERROR		(1 << 5)
#define	LPE_INT_TXFINISH	(1 << 6)
#define	LPE_INT_TXDONE		(1 << 7)
#define	LPE_INT_SOFTINT		(1 << 12)
#define	LPE_INTWAKEUPINT	(1 << 13)
#define	LPE_POWERDOWN		0xff4

#define	LPE_DESC_ALIGN		8
#define	LPE_TXDESC_NUM		128
#define	LPE_RXDESC_NUM		128
#define	LPE_TXDESC_SIZE		(LPE_TXDESC_NUM * sizeof(struct lpe_hwdesc))
#define	LPE_RXDESC_SIZE		(LPE_RXDESC_NUM * sizeof(struct lpe_hwdesc))
#define	LPE_TXSTATUS_SIZE	(LPE_TXDESC_NUM * sizeof(struct lpe_hwstatus))
#define	LPE_RXSTATUS_SIZE	(LPE_RXDESC_NUM * sizeof(struct lpe_hwstatus))
#define	LPE_MAXFRAGS		8

struct lpe_hwdesc {
	uint32_t	lhr_data;
	uint32_t	lhr_control;
};

struct lpe_hwstatus {
	uint32_t	lhs_info;
	uint32_t	lhs_crc;
};

#define	LPE_INC(x, y)		(x) = ((x) == ((y)-1)) ? 0 : (x)+1

/* These are valid for both Rx and Tx descriptors */
#define	LPE_HWDESC_SIZE_MASK	(1 << 10)
#define	LPE_HWDESC_INTERRUPT	(1U << 31)

/* These are valid for Tx descriptors */
#define	LPE_HWDESC_LAST		(1 << 30)
#define	LPE_HWDESC_CRC		(1 << 29)
#define	LPE_HWDESC_PAD		(1 << 28)
#define	LPE_HWDESC_HUGE		(1 << 27)
#define	LPE_HWDESC_OVERRIDE	(1 << 26)

/* These are valid for Tx status descriptors */
#define	LPE_HWDESC_COLLISIONS(_n) (((_n) >> 21) & 0x7)
#define	LPE_HWDESC_DEFER	(1 << 25)
#define	LPE_HWDESC_EXCDEFER	(1 << 26)
#define	LPE_HWDESC_EXCCOLL	(1 << 27)
#define	LPE_HWDESC_LATECOLL	(1 << 28)
#define	LPE_HWDESC_UNDERRUN	(1 << 29)
#define	LPE_HWDESC_TXNODESCR	(1 << 30)
#define	LPE_HWDESC_ERROR	(1U << 31)

/* These are valid for Rx status descriptors */
#define	LPE_HWDESC_CONTROL	(1 << 18)
#define	LPE_HWDESC_VLAN		(1 << 19)
#define	LPE_HWDESC_FAILFILTER	(1 << 20)
#define	LPE_HWDESC_MULTICAST	(1 << 21)
#define	LPE_HWDESC_BROADCAST	(1 << 22)
#define	LPE_HWDESC_CRCERROR	(1 << 23)
#define	LPE_HWDESC_SYMBOLERROR	(1 << 24)
#define	LPE_HWDESC_LENGTHERROR	(1 << 25)
#define	LPE_HWDESC_RANGEERROR	(1 << 26)
#define	LPE_HWDESC_ALIGNERROR	(1 << 27)
#define	LPE_HWDESC_OVERRUN	(1 << 28)
#define	LPE_HWDESC_RXNODESCR	(1 << 29)
#define	LPE_HWDESC_LASTFLAG	(1 << 30)
#define	LPE_HWDESC_ERROR	(1U << 31)


#endif	/* _ARM_LPC_IF_LPEREG_H */
