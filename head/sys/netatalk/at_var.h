/*-
 * Copyright (c) 1990, 1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 *
 * $FreeBSD$
 */

#ifndef _NETATALK_AT_VAR_H_
#define	_NETATALK_AT_VAR_H_

/*
 * For phase2, we need to keep not only our address on an interface, but also
 * the legal networks on the interface.
 */
struct at_ifaddr {
	struct ifaddr		 aa_ifa;
	struct sockaddr_at	 aa_addr;
	struct sockaddr_at	 aa_broadaddr;
	struct sockaddr_at	 aa_netmask;
	int			 aa_flags;
	u_short			 aa_firstnet;
	u_short			 aa_lastnet;
	int			 aa_probcnt;
	struct callout		 aa_callout;
	TAILQ_ENTRY(at_ifaddr)	 aa_link;
};
#define	aa_ifp		aa_ifa.ifa_ifp
#define	aa_dstaddr	aa_broadaddr;

TAILQ_HEAD(at_ifaddrhead, at_ifaddr);

struct at_aliasreq {
	char			ifra_name[IFNAMSIZ];
	struct sockaddr_at	ifra_addr;
	struct sockaddr_at	ifra_broadaddr;
	struct sockaddr_at	ifra_mask;
};
#define	ifra_dstaddr	ifra_broadaddr

#define	AA_SAT(aa)	(&(aa->aa_addr))
#define	satosat(sa)	((struct sockaddr_at *)(sa))

#define	AFA_ROUTE	0x0001
#define	AFA_PROBING	0x0002
#define	AFA_PHASE2	0x0004

#ifdef _KERNEL
extern struct rwlock		at_ifaddr_rw;
extern struct at_ifaddrhead	at_ifaddrhead;

#define	AT_IFADDR_LOCK_INIT()	rw_init(&at_ifaddr_rw, "at_ifaddr_rw")
#define	AT_IFADDR_LOCK_ASSERT()	rw_assert(&at_ifaddr_rw, RA_LOCKED)
#define	AT_IFADDR_RLOCK()	rw_rlock(&at_ifaddr_rw)
#define	AT_IFADDR_RUNLOCK()	rw_runlock(&at_ifaddr_rw)
#define	AT_IFADDR_WLOCK()	rw_wlock(&at_ifaddr_rw)
#define	AT_IFADDR_WUNLOCK()	rw_wunlock(&at_ifaddr_rw)
#endif

#endif /* _NETATALK_AT_VAR_H_ */
