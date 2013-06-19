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

#ifndef _NETATALK_AT_EXTERN_H_
#define	_NETATALK_AT_EXTERN_H_

extern struct mtx	aarptab_mtx;

#define	AARPTAB_LOCK()		mtx_lock(&aarptab_mtx)
#define	AARPTAB_UNLOCK()	mtx_unlock(&aarptab_mtx)
#define	AARPTAB_LOCK_ASSERT()	mtx_assert(&aarptab_mtx, MA_OWNED)
#define	AARPTAB_UNLOCK_ASSERT()	mtx_assert(&aarptab_mtx, MA_NOTOWNED)

struct at_ifaddr;
struct ifnet;
struct mbuf;
struct route;
struct thread;
struct sockaddr_at;
struct socket;
void		 aarpintr(struct mbuf *);
void		 aarpprobe(void *arg);
int		 aarpresolve(struct ifnet *, struct mbuf *,
		    const struct sockaddr_at *, u_char *);
void		 aarp_clean(void);
void		 at1intr(struct mbuf *);
void		 at2intr(struct mbuf *);
int		 at_broadcast(const struct sockaddr_at  *);
u_short		 at_cksum(struct mbuf *m, int skip);
int		 at_control(struct socket *so, u_long cmd, caddr_t data,
		    struct ifnet *ifp, struct thread *td);
struct at_ifaddr	*at_ifawithnet(const struct sockaddr_at *);
struct at_ifaddr	*at_ifawithnet_locked(const struct sockaddr_at  *sat);

int		 at_inithead(void**, int);
void		 ddp_init(void);
int		 ddp_output(struct mbuf *m, struct socket *so); 
int		 ddp_route(struct mbuf *m, struct route *ro);
struct ddpcb	*ddp_search(struct sockaddr_at *, struct sockaddr_at *,
		    struct at_ifaddr *);

#endif /* !_NETATALK_AT_EXTERN_H_ */
