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
 * $FreeBSD: src/sys/netatalk/at_var.h,v 1.15.6.1 2008/11/25 02:59:29 kensmith Exp $
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
	struct at_ifaddr	*aa_next;
};
#define	aa_ifp		aa_ifa.ifa_ifp
#define	aa_dstaddr	aa_broadaddr;

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
extern struct at_ifaddr	*at_ifaddr_list;
#endif

#endif /* _NETATALK_AT_VAR_H_ */
