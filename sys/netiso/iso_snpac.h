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
 *	from: @(#)iso_snpac.h	7.8 (Berkeley) 5/6/91
 *	$Id: iso_snpac.h,v 1.4 1993/12/19 00:53:27 wollman Exp $
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

#ifndef _NETISO_ISO_SNPAC_H_
#define _NETISO_ISO_SNPAC_H_ 1

#define	MAX_SNPALEN		8			/* curiously equal to sizeof x.121 (
										plus 1 for nibble len) addr */
struct snpa_req {
	struct iso_addr	sr_isoa;		/* nsap address */
	u_char			sr_len;			/* length of snpa */
	u_char			sr_snpa[MAX_SNPALEN];	/* snpa associated 
												with nsap address */
	u_char			sr_flags;		/* true if entry is valid */
	u_short			sr_ht;			/* holding time */
};

#define	SNPA_VALID		0x01
#define	SNPA_ES			0x02
#define SNPA_IS			0x04
#define	SNPA_PERM		0x10

struct systype_req {
	short	sr_holdt;		/* holding timer */
	short	sr_configt;		/* configuration timer */
	short	sr_esconfigt;	/* suggested ES configuration timer */
	char	sr_type;		/* SNPA_ES or SNPA_IS */
};

struct esis_req {
	short	er_ht;			/* holding time */
	u_char	er_flags;		/* type and validity */
};
/*
 * Space for this structure gets added onto the end of a route
 * going to an ethernet or other 802.[45x] device.
 */

struct llinfo_llc {
	struct	llinfo_llc *lc_next;	/* keep all llc routes linked */
	struct	llinfo_llc *lc_prev;	/* keep all llc routes linked */
	struct	rtentry *lc_rt;			/* backpointer to route */
	struct	esis_req lc_er;			/* holding time, etc */
#define lc_ht		lc_er.er_ht
#define lc_flags	lc_er.er_flags
};


/* ISO arp IOCTL data structures */

#define	SIOCSSTYPE 	_IOW('a', 39, struct systype_req) /* set system type */
#define	SIOCGSTYPE 	_IOR('a', 40, struct systype_req) /* get system type */

#ifdef KERNEL
extern struct llinfo_llc llinfo_llc;	/* head for linked lists */
extern void llc_rtrequest(int, struct rtentry *, struct sockaddr *);
extern int iso_snparesolve(struct ifnet *, struct sockaddr_iso *, caddr_t, 
			   int *);
extern void snpac_free(struct llinfo_llc *);
extern int snpac_add(struct ifnet *, struct iso_addr *, caddr_t, int /*char*/,
		     int /*u_short*/, int);
extern int snpac_ioctl(struct socket *, int, caddr_t);
extern void snpac_age(caddr_t, int);
extern int snpac_ownmulti(caddr_t, u_int);
extern void snpac_flushifp(struct ifnet *);
extern void snpac_rtrequest(int, struct iso_addr *, struct iso_addr *,
			    struct iso_addr *, int /*short*/,
			    struct rtentry **);
extern void snpac_addrt(struct ifnet *, struct iso_addr *, struct iso_addr *,
			struct iso_addr *);

#endif /* KERNEL */
#endif /* _NETISO_ISO_SNPAC_H_ */
