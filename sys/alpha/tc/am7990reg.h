/* $FreeBSD: src/sys/alpha/tc/am7990reg.h,v 1.2 1999/08/28 00:39:06 peter Exp $ */
/*	$NetBSD: am7990reg.h,v 1.4 1997/03/27 21:01:49 veego Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)if_lereg.h	8.1 (Berkeley) 6/10/93
 */

#define	LEBLEN		1536	/* ETHERMTU + header + CRC */
#define	LEMINSIZE	60	/* should be 64 if mode DTCR is set */

/*
 * Receive message descriptor
 */
struct lermd {
	u_int16_t rmd0;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t  rmd1_bits;
	u_int8_t  rmd1_hadr;
#else
	u_int8_t  rmd1_hadr;
	u_int8_t  rmd1_bits;
#endif
	int16_t	  rmd2;
	u_int16_t rmd3;
};

/*
 * Transmit message descriptor
 */
struct letmd {
	u_int16_t tmd0;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t  tmd1_bits;
	u_int8_t  tmd1_hadr;
#else
	u_int8_t  tmd1_hadr;
	u_int8_t  tmd1_bits;
#endif
	int16_t	  tmd2;
	u_int16_t tmd3;
};

/*
 * Initialization block
 */
struct leinit {
	u_int16_t init_mode;		/* +0x0000 */
	u_int16_t init_padr[3];		/* +0x0002 */
	u_int16_t init_ladrf[4];	/* +0x0008 */
	u_int16_t init_rdra;		/* +0x0010 */
	u_int16_t init_rlen;		/* +0x0012 */
	u_int16_t init_tdra;		/* +0x0014 */
	u_int16_t init_tlen;		/* +0x0016 */
	int16_t	  pad0[4];		/* Pad to 16 shorts */
};

#define	LE_INITADDR(sc)		(sc->sc_initaddr)
#define	LE_RMDADDR(sc, bix)	(sc->sc_rmdaddr + sizeof(struct lermd) * (bix))
#define	LE_TMDADDR(sc, bix)	(sc->sc_tmdaddr + sizeof(struct letmd) * (bix))
#define	LE_RBUFADDR(sc, bix)	(sc->sc_rbufaddr[bix])
#define	LE_TBUFADDR(sc, bix)	(sc->sc_tbufaddr[bix])

/* register addresses */
#define	LE_CSR0		0x0000		/* Control and status register */
#define	LE_CSR1		0x0001		/* low address of init block */
#define	LE_CSR2		0x0002		/* high address of init block */
#define	LE_CSR3		0x0003		/* Bus master and control */

/* Control and status register 0 (csr0) */
#define	LE_C0_ERR	0x8000		/* error summary */
#define	LE_C0_BABL	0x4000		/* transmitter timeout error */
#define	LE_C0_CERR	0x2000		/* collision */
#define	LE_C0_MISS	0x1000		/* missed a packet */
#define	LE_C0_MERR	0x0800		/* memory error */
#define	LE_C0_RINT	0x0400		/* receiver interrupt */
#define	LE_C0_TINT	0x0200		/* transmitter interrupt */
#define	LE_C0_IDON	0x0100		/* initalization done */
#define	LE_C0_INTR	0x0080		/* interrupt condition */
#define	LE_C0_INEA	0x0040		/* interrupt enable */
#define	LE_C0_RXON	0x0020		/* receiver on */
#define	LE_C0_TXON	0x0010		/* transmitter on */
#define	LE_C0_TDMD	0x0008		/* transmit demand */
#define	LE_C0_STOP	0x0004		/* disable all external activity */
#define	LE_C0_STRT	0x0002		/* enable external activity */
#define	LE_C0_INIT	0x0001		/* begin initalization */

#define	LE_C0_BITS \
    "\20\20ERR\17BABL\16CERR\15MISS\14MERR\13RINT\
\12TINT\11IDON\10INTR\07INEA\06RXON\05TXON\04TDMD\03STOP\02STRT\01INIT"

/* Control and status register 3 (csr3) */
#define	LE_C3_BSWP	0x0004		/* byte swap */
#define	LE_C3_ACON	0x0002		/* ALE control, eh? */
#define	LE_C3_BCON	0x0001		/* byte control */

/* Initialzation block (mode) */
#define	LE_MODE_PROM	0x8000		/* promiscuous mode */
/*			0x7f80		   reserved, must be zero */
/* 0x4000 - 0x0080 are not available on LANCE 7990 */
#define	LE_MODE_DRCVBC	0x4000		/* disable receive brodcast */
#define	LE_MODE_DRCVPA	0x2000		/* disable physical address detection */
#define	LE_MODE_DLNKTST	0x1000		/* disable link status */
#define	LE_MODE_DAPC	0x0800		/* disable automatic polarity correction */
#define	LE_MODE_MENDECL	0x0400		/* MENDEC loopback mode */
#define	LE_MODE_LRTTSEL	0x0200		/* lower receice threshold /
					   transmit mode selection */
#define	LE_MODE_PSEL1	0x0100		/* port selection bit1 */
#define	LE_MODE_PSEL0	0x0080		/* port selection bit0 */
#define	LE_MODE_INTL	0x0040		/* internal loopback */
#define	LE_MODE_DRTY	0x0020		/* disable retry */
#define	LE_MODE_COLL	0x0010		/* force a collision */
#define	LE_MODE_DTCR	0x0008		/* disable transmit CRC */
#define	LE_MODE_LOOP	0x0004		/* loopback mode */
#define	LE_MODE_DTX	0x0002		/* disable transmitter */
#define	LE_MODE_DRX	0x0001		/* disable receiver */
#define	LE_MODE_NORMAL	0		/* none of the above */

/* Receive message descriptor 1 (rmd1_bits) */ 
#define	LE_R1_OWN	0x80		/* LANCE owns the packet */
#define	LE_R1_ERR	0x40		/* error summary */
#define	LE_R1_FRAM	0x20		/* framing error */
#define	LE_R1_OFLO	0x10		/* overflow error */
#define	LE_R1_CRC	0x08		/* CRC error */
#define	LE_R1_BUFF	0x04		/* buffer error */
#define	LE_R1_STP	0x02		/* start of packet */
#define	LE_R1_ENP	0x01		/* end of packet */

#define	LE_R1_BITS \
    "\20\10OWN\7ERR\6FRAM\5OFLO\4CRC\3BUFF\2STP\1ENP"

/* Transmit message descriptor 1 (tmd1_bits) */ 
#define	LE_T1_OWN	0x80		/* LANCE owns the packet */
#define	LE_T1_ERR	0x40		/* error summary */
#define	LE_T1_MORE	0x10		/* multiple collisions */
#define	LE_T1_ONE	0x08		/* single collision */
#define	LE_T1_DEF	0x04		/* defferred transmit */
#define	LE_T1_STP	0x02		/* start of packet */
#define	LE_T1_ENP	0x01		/* end of packet */

#define	LE_T1_BITS \
    "\20\10OWN\7ERR\6RES\5MORE\4ONE\3DEF\2STP\1ENP"

/* Transmit message descriptor 3 (tmd3) */ 
#define	LE_T3_BUFF	0x8000		/* buffer error */
#define	LE_T3_UFLO	0x4000		/* underflow error */
#define	LE_T3_LCOL	0x1000		/* late collision */
#define	LE_T3_LCAR	0x0800		/* loss of carrier */
#define	LE_T3_RTRY	0x0400		/* retry error */
#define	LE_T3_TDR_MASK	0x03ff		/* time domain reflectometry counter */

#define	LE_XMD2_ONES	0xf000

#define	LE_T3_BITS \
    "\20\20BUFF\17UFLO\16RES\15LCOL\14LCAR\13RTRY"

/*
 * PCnet-ISA defines which are not available on LANCE 7990
 */

/* (ISA) Bus Configuration Registers */
#define	LE_BCR_MSRDA	0x0000
#define	LE_BCR_MSWRA	0x0001
#define	LE_BCR_MC	0x0002
#define	LE_BCR_LED1	0x0005
#define	LE_BCR_LED2	0x0006
#define	LE_BCR_LED3	0x0007

/* Bus configurations bits (MC) */
#define	LE_MC_EADISEL	0x0008		/* EADI selection */
#define	LE_MC_AWAKE	0x0004		/* auto-wake */
#define	LE_MC_ASEL	0x0002		/* auto selection */
#define	LE_MC_XMAUSEL	0x0001		/* external MAU selection */

/* LED bis (LED[123]) */
#define	LE_LED_LEDOUT	0x8000
#define	LE_LED_PSE	0x0080
#define	LE_LED_XMTE	0x0010
#define	LE_LED_PVPE	0x0008
#define	LE_LED_PCVE	0x0004
#define	LE_LED_JABE	0x0002
#define	LE_LED_COLE	0x0001
