/*-
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
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
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 *
 * $FreeBSD$
 */
struct mbuf;
struct sockaddr_at;

#ifdef _NET_IF_ARP_H_
extern timeout_t	aarpprobe;
extern int	aarpresolve	(struct ifnet *,
					struct mbuf *,
					struct sockaddr_at *,
					u_char *);
extern int	at_broadcast	(struct sockaddr_at  *);
#endif

#ifdef _NETATALK_AARP_H_
extern void	aarptfree	(struct aarptab *);
#endif

struct ifnet;
struct thread;
struct socket;

extern void	aarpintr	(struct mbuf *);
extern void	at1intr		(struct mbuf *);
extern void	at2intr		(struct mbuf *);
extern void	aarp_clean	(void);
extern int	at_control	(struct socket *so,
					u_long cmd,
					caddr_t data,
					struct ifnet *ifp,
					struct thread *td);
extern u_short	at_cksum	(struct mbuf *m, int skip);
extern void	ddp_init	(void);
extern struct at_ifaddr *at_ifawithnet	(struct sockaddr_at *);
#ifdef	_NETATALK_DDP_VAR_H_
extern int	ddp_output	(struct mbuf *m, struct socket *so); 

#endif
#if	defined (_NETATALK_DDP_VAR_H_) && defined(_NETATALK_AT_VAR_H_)
extern struct ddpcb  *ddp_search(struct sockaddr_at *,
                                		struct sockaddr_at *,
						struct at_ifaddr *);
#endif
#ifdef _NET_ROUTE_H_
int     ddp_route(struct mbuf *m, struct route *ro);
#endif


