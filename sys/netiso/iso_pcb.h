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
 *	from: @(#)iso_pcb.h	7.5 (Berkeley) 5/6/91
 *	$Id: iso_pcb.h,v 1.2 1993/10/16 21:05:24 rgrimes Exp $
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

#define	MAXX25CRUDLEN	16	/* 16 bytes of call request user data */

/*
 * Common structure pcb for argo protocol implementation.
 */
struct isopcb {
	struct	isopcb			*isop_next,*isop_prev; /* pointers to other pcb's */
	struct	isopcb			*isop_head;	/* pointer back to chain of pcbs for 
								this protocol */
	struct	socket			*isop_socket;	/* back pointer to socket */
	struct	sockaddr_iso	*isop_laddr;
	struct	sockaddr_iso	*isop_faddr;
	struct	route_iso {
		struct	rtentry 	*ro_rt;
		struct	sockaddr_iso ro_dst;
	}						isop_route;			/* CLNP routing entry */
	struct	mbuf			*isop_options;		/* CLNP options */
	struct	mbuf			*isop_optindex;		/* CLNP options index */
	struct	mbuf			*isop_clnpcache;	/* CLNP cached hdr */
	caddr_t					isop_chan;		/* actually struct pklcb * */
	u_short					isop_refcnt;		/* mult TP4 tpcb's -> here */
	u_short					isop_lport;			/* MISLEADLING work var */
	int						isop_x25crud_len;	/* x25 call request ud */
	char					isop_x25crud[MAXX25CRUDLEN];
	struct ifaddr			*isop_ifa;		/* ESIS interface assoc w/sock */
	struct	sockaddr_iso	isop_sladdr,		/* preallocated laddr */
							isop_sfaddr;		/* preallocated faddr */
};

#ifdef sotorawcb
/*
 * Common structure pcb for raw clnp protocol access.
 * Here are clnp specific extensions to the raw control block,
 * and space is allocated to the necessary sockaddrs.
 */
struct rawisopcb {
	struct	rawcb risop_rcb;		/* common control block prefix */
	int		risop_flags;			/* flags, e.g. raw sockopts */
	struct	isopcb risop_isop;		/* space for bound addresses, routes etc.*/
};
#endif

#define	sotoisopcb(so)	((struct isopcb *)(so)->so_pcb)
#define	sotorawisopcb(so)	((struct rawisopcb *)(so)->so_pcb)

#ifdef KERNEL
struct	isopcb *iso_pcblookup();
#endif
