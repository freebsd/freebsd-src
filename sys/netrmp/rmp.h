/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: rmp.h 1.3 89/06/07$
 *
 *	From: @(#)rmp.h	7.1 (Berkeley) 5/8/90
 *	$Id: rmp.h,v 1.1 1993/11/07 22:55:10 wollman Exp $
 */

#ifndef _NETRMP_RMP_H_
#define _NETRMP_RMP_H_
/*
 *  Define MIN/MAX sizes of RMP (ethernet) packet.  For ease of computation,
 *  the 4 octet CRC field is not included.
 */

#define	RMP_MAX_PACKET	1514
#define	RMP_MIN_PACKET	60


/*
 *  Define IEEE802.2 (Logical Link Control) information.
 */

#define	ETHERTYPE_IEEE	0	/* hack hack hack */

#define	IEEE802LEN_MIN	40
#define IEEE802LEN_MAX	1500

#define	IEEE_DSAP_HP	0xF8	/* Destination Service Access Point */
#define	IEEE_SSAP_HP	0xF8	/* Source Service Access Point */
#define	IEEE_CNTL_HP	0x0300	/* Type 1 / I format control information */

#define	HPEXT_DXSAP	0x608	/* HP Destination Service Access Point */
#define	HPEXT_SXSAP	0x609	/* HP Source Service Access Point */

/*
 * HP uses 802.2 LLC with their own local extensions.  This struct makes
 * sence out of this data (encapsulated in the 802.3 packet).
 */

struct hp_llc {
	u_char	dsap;		/* 802.2 DSAP */
	u_char	ssap;		/* 802.2 SSAP */
	u_short	cntrl;		/* 802.2 control field */
	u_short	filler;		/* HP filler (must be zero) */
	u_short	dxsap;		/* HP extended DSAP */
	u_short	sxsap;		/* HP extended SSAP */
};


/*
 * Protocol(s)
 */

#define RMPPROTO_BOOT	1		/* RMP boot protocol */

#if	defined(KERNEL) & defined(RMP)
extern	struct	domain rmpdomain;
extern	struct	protosw rmpsw[];
#endif

#endif /* _NETRMP_RMP_H_ */
