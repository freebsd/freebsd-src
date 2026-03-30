/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$KAME: ip6_mroute.c,v 1.58 2001/12/18 02:36:31 itojun Exp $
 */

/*-
 * Copyright (c) 1989 Stephen Deering
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	BSDI ip_mroute.c,v 2.10 1996/11/14 00:29:52 jch Exp
 */

/*
 * IP multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1994
 *
 * MROUTING Revision: 3.5.1.2 + PIM-SMv2 (pimd) Support
 */

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/errno.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/domain.h>
#include <sys/priv.h>
#include <sys/protosw.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/icmp6.h>
#include <netinet/ip_encap.h>

#include <netinet/ip6.h>
#include <netinet/in_kdtrace.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/pim6.h>
#include <netinet6/pim6_var.h>

static MALLOC_DEFINE(M_MRTABLE6, "mf6c", "multicast forwarding cache entry");

struct mf6ctable;

static int	ip6_mdq(struct mf6ctable *, struct mbuf *, struct ifnet *,
		    struct mf6c *);
static void	phyint_send(struct ip6_hdr *, struct mif6 *, struct mbuf *);
static int	register_send(struct mf6ctable *, struct ip6_hdr *, mifi_t,
		    struct mbuf *);
static int	set_pim6(int *);
static int	socket_send(struct socket *, struct mbuf *,
		    struct sockaddr_in6 *);

extern int in6_mcast_loop;
extern struct domain inet6domain;

static const struct encaptab *pim6_encap_cookie;
static int pim6_encapcheck(const struct mbuf *, int, int, void *);
static int pim6_input(struct mbuf *, int, int, void *);

static const struct encap_config ipv6_encap_cfg = {
	.proto = IPPROTO_PIM,
	.min_length = sizeof(struct ip6_hdr) + PIM_MINLEN,
	.exact_match = 8,
	.check = pim6_encapcheck,
	.input = pim6_input
};

SYSCTL_DECL(_net_inet6);
SYSCTL_DECL(_net_inet6_ip6);
static SYSCTL_NODE(_net_inet6, IPPROTO_PIM, pim,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "PIM");

static struct mrt6stat mrt6stat;
SYSCTL_STRUCT(_net_inet6_ip6, OID_AUTO, mrt6stat, CTLFLAG_RW,
    &mrt6stat, mrt6stat,
    "Multicast Routing Statistics (struct mrt6stat, netinet6/ip6_mroute.h)");

#define	MRT6STAT_INC(name)	mrt6stat.name += 1
#define NO_RTE_FOUND	0x1
#define RTE_FOUND	0x2

static struct sx mrouter6_mtx;
#define	MROUTER6_LOCKPTR()	(&mrouter6_mtx)
#define	MROUTER6_LOCK()		sx_xlock(MROUTER6_LOCKPTR())
#define	MROUTER6_UNLOCK()	sx_xunlock(MROUTER6_LOCKPTR())
#define	MROUTER6_LOCK_ASSERT()	sx_assert(MROUTER6_LOCKPTR(), SA_XLOCKED
#define	MROUTER6_LOCK_INIT()	sx_init(MROUTER6_LOCKPTR(), "mrouter6")
#define	MROUTER6_LOCK_DESTROY()	sx_destroy(MROUTER6_LOCKPTR())

static struct mtx mfc6_mtx;
#define	MFC6_LOCKPTR()		(&mfc6_mtx)
#define	MFC6_LOCK()		mtx_lock(MFC6_LOCKPTR())
#define	MFC6_UNLOCK()		mtx_unlock(MFC6_LOCKPTR())
#define	MFC6_LOCK_ASSERT()	mtx_assert(MFC6_LOCKPTR(), MA_OWNED)
#define	MFC6_LOCK_INIT()	mtx_init(MFC6_LOCKPTR(),		\
				    "IPv6 multicast forwarding cache",	\
				    NULL, MTX_DEF)
#define	MFC6_LOCK_DESTROY()	mtx_destroy(MFC6_LOCKPTR())

struct mf6ctable {
	struct socket	*router;
	int		router_ver;
	struct mf6c	*mfchashtbl[MF6CTBLSIZ];
	u_char		nexpire[MF6CTBLSIZ];
	int		nummifs;
	struct mif6	miftable[MAXMIFS];

	/*
	 * 'Interfaces' associated with decapsulator (so we can tell packets
	 * that went through it from ones that get reflected by a broken
	 * gateway).  Different from IPv4 register_if, these interfaces are
	 * linked into the system ifnet list, because per-interface IPv6
	 * statistics are maintained in ifp->if_afdata.  But it does not have
	 * any routes point to them.  I.e., packets can't be sent this way.
	 * They only exist as a placeholder for multicast source verification.
	 */
	struct ifnet	*register_if;
	mifi_t		register_mif;
};

VNET_DEFINE_STATIC(struct mf6ctable *, mfctables);
#define	V_mfctables		VNET(mfctables)
VNET_DEFINE_STATIC(uint32_t, nmfctables);
#define	V_nmfctables		VNET(nmfctables)

static eventhandler_tag rtnumfibs_change_tag;

static int
sysctl_mfctable(SYSCTL_HANDLER_ARGS)
{
	int fibnum;

	fibnum = curthread->td_proc->p_fibnum;
	return (SYSCTL_OUT(req, &V_mfctables[fibnum].mfchashtbl,
	    sizeof(struct mfc6c *) * MF6CTBLSIZ));
}
SYSCTL_PROC(_net_inet6_ip6, OID_AUTO, mf6ctable,
    CTLTYPE_OPAQUE | CTLFLAG_RD,
    NULL, 0, sysctl_mfctable, "S,*mf6c[MF6CTBLSIZ]",
    "IPv6 Multicast Forwarding Table (struct mf6c *[MF6CTBLSIZ], "
    "netinet6/ip6_mroute.h)");

static int
sysctl_mif6table(SYSCTL_HANDLER_ARGS)
{
	struct mif6_sctl *out;
	struct mf6ctable *mfct;
	int error;

	mfct = &V_mfctables[curthread->td_proc->p_fibnum];
	out = malloc(sizeof(struct mif6_sctl) * MAXMIFS, M_TEMP,
	    M_WAITOK | M_ZERO);
	for (int i = 0; i < MAXMIFS; i++) {
		struct mif6_sctl *outp = &out[i];
		struct mif6 *mifp = &mfct->miftable[i];

		outp->m6_flags = mifp->m6_flags;
		outp->m6_rate_limit = mifp->m6_rate_limit;
		outp->m6_lcl_addr = mifp->m6_lcl_addr;
		if (mifp->m6_ifp != NULL)
			outp->m6_ifp = mifp->m6_ifp->if_index;
		else
			outp->m6_ifp = 0;
		outp->m6_pkt_in	= mifp->m6_pkt_in;
		outp->m6_pkt_out = mifp->m6_pkt_out;
		outp->m6_bytes_in = mifp->m6_bytes_in;
		outp->m6_bytes_out = mifp->m6_bytes_out;
	}
	error = SYSCTL_OUT(req, out, sizeof(struct mif6_sctl) * MAXMIFS);
	free(out, M_TEMP);
	return (error);
}
SYSCTL_PROC(_net_inet6_ip6, OID_AUTO, mif6table,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
    NULL, 0, sysctl_mif6table, "S,mif6_sctl[MAXMIFS]",
    "IPv6 Multicast Interfaces (struct mif6_sctl[MAXMIFS], "
    "netinet6/ip6_mroute.h)");

static struct mtx mif6_mtx;
#define	MIF6_LOCKPTR()		(&mif6_mtx)
#define	MIF6_LOCK()		mtx_lock(MIF6_LOCKPTR())
#define	MIF6_UNLOCK()		mtx_unlock(MIF6_LOCKPTR())
#define	MIF6_LOCK_ASSERT()	mtx_assert(MIF6_LOCKPTR(), MA_OWNED)
#define	MIF6_LOCK_INIT()	\
	mtx_init(MIF6_LOCKPTR(), "IPv6 multicast interfaces", NULL, MTX_DEF)
#define	MIF6_LOCK_DESTROY()	mtx_destroy(MIF6_LOCKPTR())

#ifdef MRT6DEBUG
VNET_DEFINE_STATIC(u_int, mrt6debug) = 0;	/* debug level */
#define	V_mrt6debug		VNET(mrt6debug)
#define DEBUG_MFC	0x02
#define DEBUG_FORWARD	0x04
#define DEBUG_EXPIRE	0x08
#define DEBUG_XMIT	0x10
#define DEBUG_REG	0x20
#define DEBUG_PIM	0x40
#define	DEBUG_ERR	0x80
#define	DEBUG_ANY	0x7f
#define	MRT6_DLOG(m, fmt, ...)	\
	if (V_mrt6debug & (m))	\
		log(((m) & DEBUG_ERR) ? LOG_ERR: LOG_DEBUG, \
		    "%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define	MRT6_DLOG(m, fmt, ...)
#endif

static void	expire_upcalls(struct mf6ctable *);
static void	expire_upcalls_all(void *);
#define	EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second */
#define	UPCALL_EXPIRE	6		/* number of timeouts */

static struct pim6stat pim6stat;
SYSCTL_STRUCT(_net_inet6_pim, PIM6CTL_STATS, stats, CTLFLAG_RW,
    &pim6stat, pim6stat,
    "PIM Statistics (struct pim6stat, netinet6/pim6_var.h)");

#define	PIM6STAT_INC(name)	pim6stat.name += 1
VNET_DEFINE_STATIC(int, pim6);
#define	V_pim6		VNET(pim6)

/*
 * Hash function for a source, group entry
 */
#define MF6CHASH(a, g) MF6CHASHMOD((a).s6_addr32[0] ^ (a).s6_addr32[1] ^ \
				   (a).s6_addr32[2] ^ (a).s6_addr32[3] ^ \
				   (g).s6_addr32[0] ^ (g).s6_addr32[1] ^ \
				   (g).s6_addr32[2] ^ (g).s6_addr32[3])

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 * XXX: replace with timersub() ?
 */
#define TV_DELTA(a, b, delta) do { \
	    int xxs; \
		\
	    delta = (a).tv_usec - (b).tv_usec; \
	    if ((xxs = (a).tv_sec - (b).tv_sec)) { \
	       switch (xxs) { \
		      case 2: \
			  delta += 1000000; \
			      /* FALLTHROUGH */ \
		      case 1: \
			  delta += 1000000; \
			  break; \
		      default: \
			  delta += (1000000 * xxs); \
	       } \
	    } \
} while (/*CONSTCOND*/ 0)

/* XXX: replace with timercmp(a, b, <) ? */
#define TV_LT(a, b) (((a).tv_usec < (b).tv_usec && \
	      (a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

#ifdef UPCALL_TIMING
#define UPCALL_MAX	50
static u_long upcall_data[UPCALL_MAX + 1];
static void collate(struct timeval *);
#endif /* UPCALL_TIMING */

static int ip6_mrouter_init(struct socket *, int, int);
static int add_m6fc(struct mf6ctable *, struct mf6cctl *);
static int add_m6if(struct mf6ctable *, int, struct mif6ctl *);
static int del_m6fc(struct mf6ctable *, struct mf6cctl *);
static int del_m6if(struct mf6ctable *, mifi_t);
static int del_m6if_locked(struct mf6ctable *, mifi_t);
static int get_mif6_cnt(struct mf6ctable *, struct sioc_mif_req6 *);
static int get_sg_cnt(struct mf6ctable *, struct sioc_sg_req6 *);

VNET_DEFINE_STATIC(struct callout, expire_upcalls_ch);
#define	V_expire_upcalls_ch	VNET(expire_upcalls_ch)

static int X_ip6_mforward(struct ip6_hdr *, struct ifnet *, struct mbuf *);
static void X_ip6_mrouter_done(struct socket *);
static int X_ip6_mrouter_set(struct socket *, struct sockopt *);
static int X_ip6_mrouter_get(struct socket *, struct sockopt *);
static int X_mrt6_ioctl(u_long, caddr_t, int);

static struct mf6c *
mf6c_find(const struct mf6ctable *mfct, const struct in6_addr *origin,
    const struct in6_addr *group)
{
	MFC6_LOCK_ASSERT();

	for (struct mf6c *rt = mfct->mfchashtbl[MF6CHASH(*origin, *group)];
	    rt != NULL; rt = rt->mf6c_next) {
		if (IN6_ARE_ADDR_EQUAL(&rt->mf6c_origin.sin6_addr, origin) &&
		    IN6_ARE_ADDR_EQUAL(&rt->mf6c_mcastgrp.sin6_addr, group) &&
		    rt->mf6c_stall == NULL)
			return (rt);
	}
	MRT6STAT_INC(mrt6s_mfc_misses);
	return (NULL);
}

static struct mf6ctable *
somfctable(struct socket *so)
{
	int fib;

	fib = atomic_load_int(&so->so_fibnum);
	KASSERT(fib >= 0 && fib < V_nmfctables,
	    ("%s: so_fibnum %d out of range", __func__, fib));
	return (&V_mfctables[fib]);
}

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
static int
X_ip6_mrouter_set(struct socket *so, struct sockopt *sopt)
{
	struct mf6ctable *mfct;
	int error = 0;
	int optval;
	struct mif6ctl mifc;
	struct mf6cctl mfcc;
	mifi_t mifi;

	mfct = somfctable(so);
	if (so != mfct->router && sopt->sopt_name != MRT6_INIT)
		return (EPERM);

	switch (sopt->sopt_name) {
	case MRT6_INIT:
#ifdef MRT6_OINIT
	case MRT6_OINIT:
#endif
		error = sooptcopyin(sopt, &optval, sizeof(optval),
		    sizeof(optval));
		if (error)
			break;
		error = ip6_mrouter_init(so, optval, sopt->sopt_name);
		break;
	case MRT6_DONE:
		X_ip6_mrouter_done(so);
		break;
	case MRT6_ADD_MIF:
		error = sooptcopyin(sopt, &mifc, sizeof(mifc), sizeof(mifc));
		if (error)
			break;
		error = add_m6if(mfct, so->so_fibnum, &mifc);
		break;
	case MRT6_ADD_MFC:
		error = sooptcopyin(sopt, &mfcc, sizeof(mfcc), sizeof(mfcc));
		if (error)
			break;
		error = add_m6fc(mfct, &mfcc);
		break;
	case MRT6_DEL_MFC:
		error = sooptcopyin(sopt, &mfcc, sizeof(mfcc), sizeof(mfcc));
		if (error)
			break;
		error = del_m6fc(mfct, &mfcc);
		break;
	case MRT6_DEL_MIF:
		error = sooptcopyin(sopt, &mifi, sizeof(mifi), sizeof(mifi));
		if (error)
			break;
		error = del_m6if(mfct, mifi);
		break;
	case MRT6_PIM:
		error = sooptcopyin(sopt, &optval, sizeof(optval),
		    sizeof(optval));
		if (error)
			break;
		error = set_pim6(&optval);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

/*
 * Handle MRT getsockopt commands
 */
static int
X_ip6_mrouter_get(struct socket *so, struct sockopt *sopt)
{
	struct mf6ctable *mfct;
	int error = 0;

	mfct = somfctable(so);
	if (so != mfct->router)
		return (EACCES);

	switch (sopt->sopt_name) {
		case MRT6_PIM:
			error = sooptcopyout(sopt, &V_pim6, sizeof(V_pim6));
			break;
	}
	return (error);
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
static int
X_mrt6_ioctl(u_long cmd, caddr_t data, int fibnum)
{
	struct mf6ctable *mfct;
	int error;

	error = priv_check(curthread, PRIV_NETINET_MROUTE);
	if (error)
		return (error);

	mfct = &V_mfctables[fibnum];
	switch (cmd) {
	case SIOCGETSGCNT_IN6:
		error = get_sg_cnt(mfct, (struct sioc_sg_req6 *)data);
		break;

	case SIOCGETMIFCNT_IN6:
		error = get_mif6_cnt(mfct, (struct sioc_mif_req6 *)data);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
static int
get_sg_cnt(struct mf6ctable *mfct, struct sioc_sg_req6 *req)
{
	struct mf6c *rt;
	int ret;

	ret = 0;

	MFC6_LOCK();
	rt = mf6c_find(mfct, &req->src.sin6_addr, &req->grp.sin6_addr);
	if (rt == NULL) {
		ret = ESRCH;
	} else {
		req->pktcnt = rt->mf6c_pkt_cnt;
		req->bytecnt = rt->mf6c_byte_cnt;
		req->wrong_if = rt->mf6c_wrong_if;
	}
	MFC6_UNLOCK();

	return (ret);
}

/*
 * returns the input and output packet and byte counts on the mif provided
 */
static int
get_mif6_cnt(struct mf6ctable *mfct, struct sioc_mif_req6 *req)
{
	mifi_t mifi;
	int ret;

	ret = 0;
	mifi = req->mifi;

	MIF6_LOCK();

	if (mifi >= mfct->nummifs) {
		ret = EINVAL;
	} else {
		struct mif6 *mif = &mfct->miftable[mifi];

		req->icount = mif->m6_pkt_in;
		req->ocount = mif->m6_pkt_out;
		req->ibytes = mif->m6_bytes_in;
		req->obytes = mif->m6_bytes_out;
	}

	MIF6_UNLOCK();

	return (ret);
}

static int
set_pim6(int *i)
{
	if ((*i != 1) && (*i != 0))
		return (EINVAL);

	/* XXX-MJ */
	V_pim6 = *i;

	return (0);
}

/*
 * Enable multicast routing
 */
static int
ip6_mrouter_init(struct socket *so, int v, int cmd)
{
	struct mf6ctable *mfct;

	MRT6_DLOG(DEBUG_ANY, "%s: socket %p", __func__, so);

	if (v != 1)
		return (ENOPROTOOPT);

	mfct = somfctable(so);
	MROUTER6_LOCK();

	if (mfct->router != NULL) {
		MROUTER6_UNLOCK();
		return (EADDRINUSE);
	}

	MFC6_LOCK();
	V_ip6_mrouting_enabled = true;
	mfct->router = so;
	mfct->router_ver = cmd;

	bzero(&mfct->mfchashtbl, sizeof(mfct->mfchashtbl));
	bzero(&mfct->nexpire, sizeof(mfct->nexpire));

	V_pim6 = 0;/* used for stubbing out/in pim stuff */

	callout_reset(&V_expire_upcalls_ch, EXPIRE_TIMEOUT, expire_upcalls_all,
	    curvnet);

	MFC6_UNLOCK();
	MROUTER6_UNLOCK();

	MRT6_DLOG(DEBUG_ANY, "finished");

	return (0);
}

/*
 * Disable IPv6 multicast forwarding.
 */
static void
X_ip6_mrouter_done(struct socket *so)
{
	struct mf6ctable *mfct;
	mifi_t mifi;
	u_long i;
	struct mf6c *rt;
	struct rtdetq *rte;

	mfct = somfctable(so);
	MROUTER6_LOCK();

	if (mfct->router != so) {
		MROUTER6_UNLOCK();
		return;
	}

	/*
	 * For each phyint in use, disable promiscuous reception of all IPv6
	 * multicasts.
	 */
	for (mifi = 0; mifi < mfct->nummifs; mifi++) {
		struct mif6 *mif = &mfct->miftable[mifi];

		if (mif->m6_ifp && !(mif->m6_flags & MIFF_REGISTER)) {
			if_allmulti(mif->m6_ifp, 0);
		}
	}
	MFC6_LOCK();
	bzero(mfct->miftable, sizeof(mfct->miftable));
	mfct->nummifs = 0;

	V_pim6 = 0; /* used to stub out/in pim specific code */

	/*
	 * Free all multicast forwarding cache entries.
	 */
	for (i = 0; i < MF6CTBLSIZ; i++) {
		rt = mfct->mfchashtbl[i];
		while (rt) {
			struct mf6c *frt;

			for (rte = rt->mf6c_stall; rte != NULL; ) {
				struct rtdetq *n = rte->next;

				m_freem(rte->m);
				free(rte, M_MRTABLE6);
				rte = n;
			}
			frt = rt;
			rt = rt->mf6c_next;
			free(frt, M_MRTABLE6);
		}
	}
	mfct->router = NULL;
	mfct->router_ver = 0;
	V_ip6_mrouting_enabled = false;

	bzero(mfct->mfchashtbl, sizeof(mfct->mfchashtbl));
	MFC6_UNLOCK();

	/*
	 * Reset register interface
	 */
	if (mfct->register_mif != (mifi_t)-1 && mfct->register_if != NULL) {
		if_detach(mfct->register_if);
		if_free(mfct->register_if);
		mfct->register_mif = (mifi_t)-1;
		mfct->register_if = NULL;
	}

	MROUTER6_UNLOCK();
	MRT6_DLOG(DEBUG_ANY, "finished");
}

static struct sockaddr_in6 sin6 = { sizeof(sin6), AF_INET6 };

/*
 * Add a mif to the mif table
 */
static int
add_m6if(struct mf6ctable *mfct, int fibnum, struct mif6ctl *mifcp)
{
	struct epoch_tracker et;
	struct mif6 *mifp;
	struct ifnet *ifp;
	int error;

	MIF6_LOCK();

	if (mifcp->mif6c_mifi >= MAXMIFS) {
		MIF6_UNLOCK();
		return (EINVAL);
	}
	mifp = &mfct->miftable[mifcp->mif6c_mifi];
	if (mifp->m6_ifp != NULL) {
		MIF6_UNLOCK();
		return (EADDRINUSE); /* XXX: is it appropriate? */
	}

	NET_EPOCH_ENTER(et);
	if ((ifp = ifnet_byindex(mifcp->mif6c_pifi)) == NULL) {
		NET_EPOCH_EXIT(et);
		MIF6_UNLOCK();
		return (ENXIO);
	}
	NET_EPOCH_EXIT(et);	/* XXXGL: unsafe ifp */

	if (mifcp->mif6c_flags & MIFF_REGISTER) {
		if (mfct->register_mif == (mifi_t)-1) {
			ifp = if_alloc(IFT_OTHER);

			if_initname(ifp, "register_mif", 0);
			ifp->if_flags |= IFF_LOOPBACK;
			if_attach(ifp);
			mfct->register_if = ifp;
			mfct->register_mif = mifcp->mif6c_mifi;
			/*
			 * it is impossible to guess the ifindex of the
			 * register interface.  So mif6c_pifi is automatically
			 * calculated.
			 */
			mifcp->mif6c_pifi = ifp->if_index;
		} else {
			ifp = mfct->register_if;
		}
	} else {
		/* Make sure the interface supports multicast */
		if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			MIF6_UNLOCK();
			return (EOPNOTSUPP);
		}
		if (ifp->if_fib != fibnum) {
			MIF6_UNLOCK();
			return (EADDRNOTAVAIL);
		}

		error = if_allmulti(ifp, 1);
		if (error) {
			MIF6_UNLOCK();
			return (error);
		}
	}

	mifp->m6_flags     = mifcp->mif6c_flags;
	mifp->m6_ifp       = ifp;

	/* initialize per mif pkt counters */
	mifp->m6_pkt_in    = 0;
	mifp->m6_pkt_out   = 0;
	mifp->m6_bytes_in  = 0;
	mifp->m6_bytes_out = 0;

	/* Adjust nummifs up if the mifi is higher than nummifs */
	if (mfct->nummifs <= mifcp->mif6c_mifi)
		mfct->nummifs = mifcp->mif6c_mifi + 1;

	MIF6_UNLOCK();
	MRT6_DLOG(DEBUG_ANY, "mif #%d, phyint %s", mifcp->mif6c_mifi,
	    if_name(ifp));

	return (0);
}

/*
 * Delete a mif from the mif table
 */
static int
del_m6if_locked(struct mf6ctable *mfct, mifi_t mifi)
{
	struct mif6 *mifp;
	mifi_t tmp;
	struct ifnet *ifp;

	MIF6_LOCK_ASSERT();

	if (mifi >= mfct->nummifs)
		return (EINVAL);
	mifp = &mfct->miftable[mifi];
	if (mifp->m6_ifp == NULL)
		return (EINVAL);

	if (!(mifp->m6_flags & MIFF_REGISTER)) {
		/* XXX: TODO: Maintain an ALLMULTI refcount in struct ifnet. */
		ifp = mifp->m6_ifp;
		if_allmulti(ifp, 0);
	} else {
		if (mfct->register_mif != (mifi_t)-1 &&
		    mfct->register_if != NULL) {
			if_detach(mfct->register_if);
			if_free(mfct->register_if);
			mfct->register_mif = (mifi_t)-1;
			mfct->register_if = NULL;
		}
	}

	bzero(mifp, sizeof(*mifp));

	/* Adjust nummifs down */
	for (tmp = mfct->nummifs; tmp > 0; tmp--)
		if (mfct->miftable[tmp - 1].m6_ifp != NULL)
			break;
	mfct->nummifs = tmp;
	MRT6_DLOG(DEBUG_ANY, "mif %d, nummifs %d", mifi, mfct->nummifs);

	return (0);
}

static int
del_m6if(struct mf6ctable *mfct, mifi_t mifi)
{
	int cc;

	MIF6_LOCK();
	cc = del_m6if_locked(mfct, mifi);
	MIF6_UNLOCK();

	return (cc);
}

/*
 * Add an mfc entry
 */
static int
add_m6fc(struct mf6ctable *mfct, struct mf6cctl *mfccp)
{
	struct mf6c *rt;
	u_long hash;
	struct rtdetq *rte;
	u_short nstl;
	char ip6bufo[INET6_ADDRSTRLEN], ip6bufg[INET6_ADDRSTRLEN];

	MFC6_LOCK();
	rt = mf6c_find(mfct, &mfccp->mf6cc_origin.sin6_addr,
	    &mfccp->mf6cc_mcastgrp.sin6_addr);
	/* If an entry already exists, just update the fields */
	if (rt) {
		MRT6_DLOG(DEBUG_MFC, "no upcall o %s g %s p %x",
		    ip6_sprintf(ip6bufo, &mfccp->mf6cc_origin.sin6_addr),
		    ip6_sprintf(ip6bufg, &mfccp->mf6cc_mcastgrp.sin6_addr),
		    mfccp->mf6cc_parent);

		rt->mf6c_parent = mfccp->mf6cc_parent;
		rt->mf6c_ifset = mfccp->mf6cc_ifset;

		MFC6_UNLOCK();
		return (0);
	}

	/*
	 * Find the entry for which the upcall was made and update
	 */
	hash = MF6CHASH(mfccp->mf6cc_origin.sin6_addr,
			mfccp->mf6cc_mcastgrp.sin6_addr);
	for (rt = mfct->mfchashtbl[hash], nstl = 0; rt; rt = rt->mf6c_next) {
		if (IN6_ARE_ADDR_EQUAL(&rt->mf6c_origin.sin6_addr,
				       &mfccp->mf6cc_origin.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&rt->mf6c_mcastgrp.sin6_addr,
				       &mfccp->mf6cc_mcastgrp.sin6_addr) &&
		    (rt->mf6c_stall != NULL)) {
			if (nstl++)
				log(LOG_ERR,
				    "add_m6fc: %s o %s g %s p %x dbx %p\n",
				    "multiple kernel entries",
				    ip6_sprintf(ip6bufo,
					    &mfccp->mf6cc_origin.sin6_addr),
				    ip6_sprintf(ip6bufg,
					    &mfccp->mf6cc_mcastgrp.sin6_addr),
				    mfccp->mf6cc_parent, rt->mf6c_stall);

			MRT6_DLOG(DEBUG_MFC, "o %s g %s p %x dbg %p",
			    ip6_sprintf(ip6bufo,
			    &mfccp->mf6cc_origin.sin6_addr),
			    ip6_sprintf(ip6bufg,
				&mfccp->mf6cc_mcastgrp.sin6_addr),
			    mfccp->mf6cc_parent, rt->mf6c_stall);

			rt->mf6c_origin     = mfccp->mf6cc_origin;
			rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
			rt->mf6c_parent     = mfccp->mf6cc_parent;
			rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
			/* initialize pkt counters per src-grp */
			rt->mf6c_pkt_cnt    = 0;
			rt->mf6c_byte_cnt   = 0;
			rt->mf6c_wrong_if   = 0;

			rt->mf6c_expire = 0;	/* Don't clean this guy up */
			mfct->nexpire[hash]--;

			/* free packets Qed at the end of this entry */
			for (rte = rt->mf6c_stall; rte != NULL; ) {
				struct rtdetq *n = rte->next;
				ip6_mdq(mfct, rte->m, rte->ifp, rt);
				m_freem(rte->m);
#ifdef UPCALL_TIMING
				collate(&(rte->t));
#endif /* UPCALL_TIMING */
				free(rte, M_MRTABLE6);
				rte = n;
			}
			rt->mf6c_stall = NULL;
		}
	}

	/*
	 * It is possible that an entry is being inserted without an upcall
	 */
	if (nstl == 0) {
		MRT6_DLOG(DEBUG_MFC, "no upcall h %lu o %s g %s p %x", hash,
		    ip6_sprintf(ip6bufo, &mfccp->mf6cc_origin.sin6_addr),
		    ip6_sprintf(ip6bufg, &mfccp->mf6cc_mcastgrp.sin6_addr),
		    mfccp->mf6cc_parent);

		for (rt = mfct->mfchashtbl[hash]; rt; rt = rt->mf6c_next) {
			if (IN6_ARE_ADDR_EQUAL(&rt->mf6c_origin.sin6_addr,
					       &mfccp->mf6cc_origin.sin6_addr)&&
			    IN6_ARE_ADDR_EQUAL(&rt->mf6c_mcastgrp.sin6_addr,
					       &mfccp->mf6cc_mcastgrp.sin6_addr)) {
				rt->mf6c_origin     = mfccp->mf6cc_origin;
				rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
				rt->mf6c_parent     = mfccp->mf6cc_parent;
				rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
				/* initialize pkt counters per src-grp */
				rt->mf6c_pkt_cnt    = 0;
				rt->mf6c_byte_cnt   = 0;
				rt->mf6c_wrong_if   = 0;

				if (rt->mf6c_expire)
					mfct->nexpire[hash]--;
				rt->mf6c_expire	   = 0;
			}
		}
		if (rt == NULL) {
			/* no upcall, so make a new entry */
			rt = malloc(sizeof(*rt), M_MRTABLE6, M_NOWAIT);
			if (rt == NULL) {
				MFC6_UNLOCK();
				return (ENOBUFS);
			}

			/* insert new entry at head of hash chain */
			rt->mf6c_origin     = mfccp->mf6cc_origin;
			rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
			rt->mf6c_parent     = mfccp->mf6cc_parent;
			rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
			/* initialize pkt counters per src-grp */
			rt->mf6c_pkt_cnt    = 0;
			rt->mf6c_byte_cnt   = 0;
			rt->mf6c_wrong_if   = 0;
			rt->mf6c_expire     = 0;
			rt->mf6c_stall = NULL;

			/* link into table */
			rt->mf6c_next  = mfct->mfchashtbl[hash];
			mfct->mfchashtbl[hash] = rt;
		}
	}

	MFC6_UNLOCK();
	return (0);
}

#ifdef UPCALL_TIMING
/*
 * collect delay statistics on the upcalls
 */
static void
collate(struct timeval *t)
{
	u_long d;
	struct timeval tp;
	u_long delta;

	GET_TIME(tp);

	if (TV_LT(*t, tp))
	{
		TV_DELTA(tp, *t, delta);

		d = delta >> 10;
		if (d > UPCALL_MAX)
			d = UPCALL_MAX;

		++upcall_data[d];
	}
}
#endif /* UPCALL_TIMING */

/*
 * Delete an mfc entry
 */
static int
del_m6fc(struct mf6ctable *mfct, struct mf6cctl *mfccp)
{
#ifdef MRT6DEBUG
	char ip6bufo[INET6_ADDRSTRLEN], ip6bufg[INET6_ADDRSTRLEN];
#endif
	struct sockaddr_in6	origin;
	struct sockaddr_in6	mcastgrp;
	struct mf6c		*rt;
	struct mf6c		**nptr;
	u_long		hash;

	origin = mfccp->mf6cc_origin;
	mcastgrp = mfccp->mf6cc_mcastgrp;
	hash = MF6CHASH(origin.sin6_addr, mcastgrp.sin6_addr);

	MRT6_DLOG(DEBUG_MFC, "orig %s mcastgrp %s",
	    ip6_sprintf(ip6bufo, &origin.sin6_addr),
	    ip6_sprintf(ip6bufg, &mcastgrp.sin6_addr));

	MFC6_LOCK();

	nptr = &mfct->mfchashtbl[hash];
	while ((rt = *nptr) != NULL) {
		if (IN6_ARE_ADDR_EQUAL(&origin.sin6_addr,
				       &rt->mf6c_origin.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&mcastgrp.sin6_addr,
				       &rt->mf6c_mcastgrp.sin6_addr) &&
		    rt->mf6c_stall == NULL)
			break;

		nptr = &rt->mf6c_next;
	}
	if (rt == NULL) {
		MFC6_UNLOCK();
		return (EADDRNOTAVAIL);
	}

	*nptr = rt->mf6c_next;
	free(rt, M_MRTABLE6);

	MFC6_UNLOCK();

	return (0);
}

static int
socket_send(struct socket *s, struct mbuf *mm, struct sockaddr_in6 *src)
{

	if (s) {
		if (sbappendaddr(&s->so_rcv,
				 (struct sockaddr *)src,
				 mm, (struct mbuf *)0) != 0) {
			sorwakeup(s);
			return (0);
		} else
			soroverflow(s);
	}
	m_freem(mm);
	return (-1);
}

/*
 * IPv6 multicast forwarding function. This function assumes that the packet
 * pointed to by "ip6" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IPv6 multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 *
 * NOTE: this implementation assumes that m->m_pkthdr.rcvif is NULL iff
 * this function is called in the originating context (i.e., not when
 * forwarding a packet from other node).  ip6_output(), which is currently the
 * only function that calls this function is called in the originating context,
 * explicitly ensures this condition.  It is caller's responsibility to ensure
 * that if this function is called from somewhere else in the originating
 * context in the future.
 */
static int
X_ip6_mforward(struct ip6_hdr *ip6, struct ifnet *ifp, struct mbuf *m)
{
	struct mf6ctable *mfct;
	struct rtdetq *rte;
	struct mbuf *mb0;
	struct mf6c *rt;
	struct mif6 *mifp;
	struct mbuf *mm;
	u_long hash;
	mifi_t mifi;
	char ip6bufs[INET6_ADDRSTRLEN], ip6bufd[INET6_ADDRSTRLEN];
#ifdef UPCALL_TIMING
	struct timeval tp;

	GET_TIME(tp);
#endif /* UPCALL_TIMING */

	M_ASSERTMAPPED(m);
	MRT6_DLOG(DEBUG_FORWARD, "src %s, dst %s, ifindex %d",
	    ip6_sprintf(ip6bufs, &ip6->ip6_src),
	    ip6_sprintf(ip6bufd, &ip6->ip6_dst), ifp->if_index);

	/*
	 * Don't forward a packet with Hop limit of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip6->ip6_hlim <= 1 || IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&ip6->ip6_dst))
		return (0);
	ip6->ip6_hlim--;

	/*
	 * Source address check: do not forward packets with unspecified
	 * source. It was discussed in July 2000, on ipngwg mailing list.
	 * This is rather more serious than unicast cases, because some
	 * MLD packets can be sent with the unspecified source address
	 * (although such packets must normally set 1 to the hop limit field).
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		IP6STAT_INC(ip6s_cantforward);
		if (V_ip6_log_cannot_forward && ip6_log_ratelimit()) {
			log(LOG_DEBUG,
			    "cannot forward "
			    "from %s to %s nxt %d received on %s\n",
			    ip6_sprintf(ip6bufs, &ip6->ip6_src),
			    ip6_sprintf(ip6bufd, &ip6->ip6_dst),
			    ip6->ip6_nxt,
			    if_name(m->m_pkthdr.rcvif));
		}
		return (0);
	}

	mfct = &V_mfctables[M_GETFIB(m)];
	MFC6_LOCK();

	/*
	 * Determine forwarding mifs from the forwarding cache table
	 */
	rt = mf6c_find(mfct, &ip6->ip6_src, &ip6->ip6_dst);
	MRT6STAT_INC(mrt6s_mfc_lookups);

	/* Entry exists, so forward if necessary */
	if (rt) {
		MFC6_UNLOCK();
		return (ip6_mdq(mfct, m, ifp, rt));
	}

	/*
	 * If we don't have a route for packet's origin,
	 * Make a copy of the packet & send message to routing daemon.
	 */
	MRT6STAT_INC(mrt6s_no_route);
	MRT6_DLOG(DEBUG_FORWARD | DEBUG_MFC, "no rte s %s g %s",
	    ip6_sprintf(ip6bufs, &ip6->ip6_src),
	    ip6_sprintf(ip6bufd, &ip6->ip6_dst));

	/*
	 * Allocate mbufs early so that we don't do extra work if we
	 * are just going to fail anyway.
	 */
	rte = malloc(sizeof(*rte), M_MRTABLE6, M_NOWAIT);
	if (rte == NULL) {
		MFC6_UNLOCK();
		return (ENOBUFS);
	}
	mb0 = m_copym(m, 0, M_COPYALL, M_NOWAIT);
	/*
	 * Pullup packet header if needed before storing it,
	 * as other references may modify it in the meantime.
	 */
	if (mb0 && (!M_WRITABLE(mb0) || mb0->m_len < sizeof(struct ip6_hdr)))
		mb0 = m_pullup(mb0, sizeof(struct ip6_hdr));
	if (mb0 == NULL) {
		free(rte, M_MRTABLE6);
		MFC6_UNLOCK();
		return (ENOBUFS);
	}

	/* is there an upcall waiting for this packet? */
	hash = MF6CHASH(ip6->ip6_src, ip6->ip6_dst);
	for (rt = mfct->mfchashtbl[hash]; rt; rt = rt->mf6c_next) {
		if (IN6_ARE_ADDR_EQUAL(&ip6->ip6_src,
		    &rt->mf6c_origin.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
		    &rt->mf6c_mcastgrp.sin6_addr) && (rt->mf6c_stall != NULL))
			break;
	}

	if (rt == NULL) {
		struct mrt6msg *im;
#ifdef MRT6_OINIT
		struct omrt6msg *oim;
#endif
		/* no upcall, so make a new entry */
		rt = malloc(sizeof(*rt), M_MRTABLE6, M_NOWAIT);
		if (rt == NULL) {
			free(rte, M_MRTABLE6);
			m_freem(mb0);
			MFC6_UNLOCK();
			return (ENOBUFS);
		}
		/*
		 * Make a copy of the header to send to the user
		 * level process
		 */
		mm = m_copym(mb0, 0, sizeof(struct ip6_hdr), M_NOWAIT);
		if (mm == NULL) {
			free(rte, M_MRTABLE6);
			m_freem(mb0);
			free(rt, M_MRTABLE6);
			MFC6_UNLOCK();
			return (ENOBUFS);
		}

		/*
		 * Send message to routing daemon
		 */
		sin6.sin6_addr = ip6->ip6_src;
		im = NULL;
#ifdef MRT6_OINIT
		oim = NULL;
#endif
		switch (mfct->router_ver) {
#ifdef MRT6_OINIT
		case MRT6_OINIT:
			oim = mtod(mm, struct omrt6msg *);
			oim->im6_msgtype = MRT6MSG_NOCACHE;
			oim->im6_mbz = 0;
			break;
#endif
		case MRT6_INIT:
			im = mtod(mm, struct mrt6msg *);
			im->im6_msgtype = MRT6MSG_NOCACHE;
			im->im6_mbz = 0;
			break;
		default:
			free(rte, M_MRTABLE6);
			m_freem(mb0);
			free(rt, M_MRTABLE6);
			MFC6_UNLOCK();
			return (EINVAL);
		}

		MRT6_DLOG(DEBUG_FORWARD, "getting the iif info in the kernel");
		for (mifp = mfct->miftable, mifi = 0;
		    mifi < mfct->nummifs && mifp->m6_ifp != ifp; mifp++, mifi++)
			;

		switch (mfct->router_ver) {
#ifdef MRT6_OINIT
		case MRT6_OINIT:
			oim->im6_mif = mifi;
			break;
#endif
		case MRT6_INIT:
			im->im6_mif = mifi;
			break;
		}

		if (socket_send(mfct->router, mm, &sin6) < 0) {
			log(LOG_WARNING, "ip6_mforward: ip6_mrouter "
			    "socket queue full\n");
			MRT6STAT_INC(mrt6s_upq_sockfull);
			free(rte, M_MRTABLE6);
			m_freem(mb0);
			free(rt, M_MRTABLE6);
			MFC6_UNLOCK();
			return (ENOBUFS);
		}

		MRT6STAT_INC(mrt6s_upcalls);

		/* insert new entry at head of hash chain */
		bzero(rt, sizeof(*rt));
		rt->mf6c_origin.sin6_family = AF_INET6;
		rt->mf6c_origin.sin6_len = sizeof(struct sockaddr_in6);
		rt->mf6c_origin.sin6_addr = ip6->ip6_src;
		rt->mf6c_mcastgrp.sin6_family = AF_INET6;
		rt->mf6c_mcastgrp.sin6_len = sizeof(struct sockaddr_in6);
		rt->mf6c_mcastgrp.sin6_addr = ip6->ip6_dst;
		rt->mf6c_expire = UPCALL_EXPIRE;
		mfct->nexpire[hash]++;
		rt->mf6c_parent = MF6C_INCOMPLETE_PARENT;

		/* link into table */
		rt->mf6c_next = mfct->mfchashtbl[hash];
		mfct->mfchashtbl[hash] = rt;
		/* Add this entry to the end of the queue */
		rt->mf6c_stall = rte;
	} else {
		/* determine if q has overflowed */
		struct rtdetq **p;
		int npkts = 0;

		for (p = &rt->mf6c_stall; *p != NULL; p = &(*p)->next)
			if (++npkts > MAX_UPQ6) {
				MRT6STAT_INC(mrt6s_upq_ovflw);
				free(rte, M_MRTABLE6);
				m_freem(mb0);
				MFC6_UNLOCK();
				return (0);
			}

		/* Add this entry to the end of the queue */
		*p = rte;
	}

	rte->next = NULL;
	rte->m = mb0;
	rte->ifp = ifp;
#ifdef UPCALL_TIMING
	rte->t = tp;
#endif /* UPCALL_TIMING */

	MFC6_UNLOCK();

	return (0);
}

/*
 * Clean up cache entries if upcalls are not serviced
 * Call from the Slow Timeout mechanism, every half second.
 */
static void
expire_upcalls(struct mf6ctable *mfct)
{
#ifdef MRT6DEBUG
	char ip6bufo[INET6_ADDRSTRLEN], ip6bufg[INET6_ADDRSTRLEN];
#endif
	struct rtdetq *rte;
	struct mf6c *mfc, **nptr;
	u_long i;

	MFC6_LOCK_ASSERT();

	for (i = 0; i < MF6CTBLSIZ; i++) {
		if (mfct->nexpire[i] == 0)
			continue;
		nptr = &mfct->mfchashtbl[i];
		while ((mfc = *nptr) != NULL) {
			rte = mfc->mf6c_stall;
			/*
			 * Skip real cache entries
			 * Make sure it wasn't marked to not expire (shouldn't happen)
			 * If it expires now
			 */
			if (rte != NULL &&
			    mfc->mf6c_expire != 0 &&
			    --mfc->mf6c_expire == 0) {
				MRT6_DLOG(DEBUG_EXPIRE, "expiring (%s %s)",
				    ip6_sprintf(ip6bufo, &mfc->mf6c_origin.sin6_addr),
				    ip6_sprintf(ip6bufg, &mfc->mf6c_mcastgrp.sin6_addr));
				/*
				 * drop all the packets
				 * free the mbuf with the pkt, if, timing info
				 */
				do {
					struct rtdetq *n = rte->next;
					m_freem(rte->m);
					free(rte, M_MRTABLE6);
					rte = n;
				} while (rte != NULL);
				MRT6STAT_INC(mrt6s_cache_cleanups);
				mfct->nexpire[i]--;

				*nptr = mfc->mf6c_next;
				free(mfc, M_MRTABLE6);
			} else {
				nptr = &mfc->mf6c_next;
			}
		}
	}
}

/*
 * Clean up the cache entry if upcall is not serviced
 */
static void
expire_upcalls_all(void *arg)
{
	CURVNET_SET((struct vnet *)arg);

	for (int i = 0; i < V_nmfctables; i++)
		expire_upcalls(&V_mfctables[i]);

	callout_reset(&V_expire_upcalls_ch, EXPIRE_TIMEOUT, expire_upcalls_all,
	    curvnet);

	CURVNET_RESTORE();
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip6_mdq(struct mf6ctable *mfct, struct mbuf *m, struct ifnet *ifp,
    struct mf6c *rt)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	mifi_t mifi, iif;
	struct mif6 *mifp;
	int plen = m->m_pkthdr.len;
	struct in6_addr src0, dst0; /* copies for local work */
	u_int32_t iszone, idzone, oszone, odzone;
	int error = 0;

	M_ASSERTMAPPED(m);

	/*
	 * Don't forward if it didn't arrive from the parent mif
	 * for its origin.
	 */
	mifi = rt->mf6c_parent;
	if (mifi >= mfct->nummifs || mfct->miftable[mifi].m6_ifp != ifp) {
		MRT6STAT_INC(mrt6s_wrong_if);
		rt->mf6c_wrong_if++;
		if (mifi >= mfct->nummifs)
			return (0);

		mifp = &mfct->miftable[mifi];
		MRT6_DLOG(DEBUG_FORWARD,
		    "wrong if: ifid %d mifi %d mififid %x", ifp->if_index,
		    mifi, mifp->m6_ifp->if_index);

		/*
		 * If we are doing PIM processing, and we are forwarding
		 * packets on this interface, send a message to the
		 * routing daemon.
		 */
		/* have to make sure this is a valid mif */
		if (mifp->m6_ifp && V_pim6 && (m->m_flags & M_LOOP) == 0) {
			/*
			 * Check the M_LOOP flag to avoid an
			 * unnecessary PIM assert.
			 * XXX: M_LOOP is an ad-hoc hack...
			 */
			static struct sockaddr_in6 sin6 =
			{ sizeof(sin6), AF_INET6 };

			struct mbuf *mm;
			struct mrt6msg *im;
#ifdef MRT6_OINIT
			struct omrt6msg *oim;
#endif

			mm = m_copym(m, 0, sizeof(struct ip6_hdr),
			    M_NOWAIT);
			if (mm &&
			    (!M_WRITABLE(mm) ||
			     mm->m_len < sizeof(struct ip6_hdr)))
				mm = m_pullup(mm, sizeof(struct ip6_hdr));
			if (mm == NULL)
				return (ENOBUFS);

#ifdef MRT6_OINIT
			oim = NULL;
#endif
			im = NULL;
			switch (mfct->router_ver) {
#ifdef MRT6_OINIT
			case MRT6_OINIT:
				oim = mtod(mm, struct omrt6msg *);
				oim->im6_msgtype = MRT6MSG_WRONGMIF;
				oim->im6_mbz = 0;
				break;
#endif
			case MRT6_INIT:
				im = mtod(mm, struct mrt6msg *);
				im->im6_msgtype = MRT6MSG_WRONGMIF;
				im->im6_mbz = 0;
				break;
			default:
				m_freem(mm);
				return (EINVAL);
			}

			for (mifp = mfct->miftable, iif = 0;
			     iif < mfct->nummifs && mifp->m6_ifp != ifp;
			     mifp++, iif++)
				;

			switch (mfct->router_ver) {
#ifdef MRT6_OINIT
			case MRT6_OINIT:
				oim->im6_mif = iif;
				sin6.sin6_addr = oim->im6_src;
				break;
#endif
			case MRT6_INIT:
				im->im6_mif = iif;
				sin6.sin6_addr = im->im6_src;
				break;
			}

			MRT6STAT_INC(mrt6s_upcalls);

			if (socket_send(mfct->router, mm, &sin6) < 0) {
				MRT6_DLOG(DEBUG_ANY,
				    "ip6_mrouter socket queue full");
				MRT6STAT_INC(mrt6s_upq_sockfull);
				return (ENOBUFS);
			}
		}
		return (0);
	}

	mifp = &mfct->miftable[mifi];

	/* If I sourced this packet, it counts as output, else it was input. */
	if (m->m_pkthdr.rcvif == NULL) {
		/* XXX: is rcvif really NULL when output?? */
		mifp->m6_pkt_out++;
		mifp->m6_bytes_out += plen;
	} else {
		mifp->m6_pkt_in++;
		mifp->m6_bytes_in += plen;
	}
	rt->mf6c_pkt_cnt++;
	rt->mf6c_byte_cnt += plen;

	/*
	 * For each mif, forward a copy of the packet if there are group
	 * members downstream on the interface.
	 */
	src0 = ip6->ip6_src;
	dst0 = ip6->ip6_dst;
	if ((error = in6_setscope(&src0, ifp, &iszone)) != 0 ||
	    (error = in6_setscope(&dst0, ifp, &idzone)) != 0) {
		IP6STAT_INC(ip6s_badscope);
		return (error);
	}
	for (mifp = mfct->miftable, mifi = 0; mifi < mfct->nummifs;
	    mifp++, mifi++) {
		if (IF_ISSET(mifi, &rt->mf6c_ifset)) {
			/*
			 * check if the outgoing packet is going to break
			 * a scope boundary.
			 * XXX For packets through PIM register tunnel
			 * interface, we believe a routing daemon.
			 */
			if (!(mfct->miftable[rt->mf6c_parent].m6_flags &
			      MIFF_REGISTER) &&
			    !(mifp->m6_flags & MIFF_REGISTER)) {
				if (in6_setscope(&src0, mifp->m6_ifp,
				    &oszone) ||
				    in6_setscope(&dst0, mifp->m6_ifp,
				    &odzone) ||
				    iszone != oszone ||
				    idzone != odzone) {
					IP6STAT_INC(ip6s_badscope);
					continue;
				}
			}

			mifp->m6_pkt_out++;
			mifp->m6_bytes_out += plen;
			if (mifp->m6_flags & MIFF_REGISTER)
				register_send(mfct, ip6, mifi, m);
			else
				phyint_send(ip6, mifp, m);
		}
	}
	return (0);
}

static void
phyint_send(struct ip6_hdr *ip6, struct mif6 *mifp, struct mbuf *m)
{
#ifdef MRT6DEBUG
	char ip6bufs[INET6_ADDRSTRLEN], ip6bufd[INET6_ADDRSTRLEN];
#endif
	struct mbuf *mb_copy;
	struct ifnet *ifp = mifp->m6_ifp;
	int error __unused = 0;
	u_long linkmtu;

	M_ASSERTMAPPED(m);

	/*
	 * Make a new reference to the packet; make sure that
	 * the IPv6 header is actually copied, not just referenced,
	 * so that ip6_output() only scribbles on the copy.
	 */
	mb_copy = m_copym(m, 0, M_COPYALL, M_NOWAIT);
	if (mb_copy &&
	    (!M_WRITABLE(mb_copy) || mb_copy->m_len < sizeof(struct ip6_hdr)))
		mb_copy = m_pullup(mb_copy, sizeof(struct ip6_hdr));
	if (mb_copy == NULL) {
		return;
	}
	/* set MCAST flag to the outgoing packet */
	mb_copy->m_flags |= M_MCAST;

	/*
	 * If we sourced the packet, call ip6_output since we may devide
	 * the packet into fragments when the packet is too big for the
	 * outgoing interface.
	 * Otherwise, we can simply send the packet to the interface
	 * sending queue.
	 */
	if (m->m_pkthdr.rcvif == NULL) {
		struct ip6_moptions im6o;
		struct epoch_tracker et;

		im6o.im6o_multicast_ifp = ifp;
		/* XXX: ip6_output will override ip6->ip6_hlim */
		im6o.im6o_multicast_hlim = ip6->ip6_hlim;
		im6o.im6o_multicast_loop = 1;
		NET_EPOCH_ENTER(et);
		error = ip6_output(mb_copy, NULL, NULL, IPV6_FORWARDING, &im6o,
		    NULL, NULL);
		NET_EPOCH_EXIT(et);

		MRT6_DLOG(DEBUG_XMIT, "mif %u err %d",
		    (uint16_t)(mifp - mif6table), error);
		return;
	}

	/*
	 * If configured to loop back multicasts by default,
	 * loop back a copy now.
	 */
	if (in6_mcast_loop)
		ip6_mloopback(ifp, m);

	/*
	 * Put the packet into the sending queue of the outgoing interface
	 * if it would fit in the MTU of the interface.
	 */
	linkmtu = in6_ifmtu(ifp);
	if (mb_copy->m_pkthdr.len <= linkmtu || linkmtu < IPV6_MMTU) {
		struct sockaddr_in6 dst6;

		bzero(&dst6, sizeof(dst6));
		dst6.sin6_len = sizeof(struct sockaddr_in6);
		dst6.sin6_family = AF_INET6;
		dst6.sin6_addr = ip6->ip6_dst;

		IP_PROBE(send, NULL, NULL, ip6, ifp, NULL, ip6);
		/*
		 * We just call if_output instead of nd6_output here, since
		 * we need no ND for a multicast forwarded packet...right?
		 */
		m_clrprotoflags(m);	/* Avoid confusing lower layers. */
		error = (*ifp->if_output)(ifp, mb_copy,
		    (struct sockaddr *)&dst6, NULL);
		MRT6_DLOG(DEBUG_XMIT, "mif %u err %d",
		    (uint16_t)(mifp - mif6table), error);
	} else {
		/*
		 * pMTU discovery is intentionally disabled by default, since
		 * various router may notify pMTU in multicast, which can be
		 * a DDoS to a router
		 */
		if (V_ip6_mcast_pmtu)
			icmp6_error(mb_copy, ICMP6_PACKET_TOO_BIG, 0, linkmtu);
		else {
			MRT6_DLOG(DEBUG_XMIT, " packet too big on %s o %s "
			    "g %s size %d (discarded)", if_name(ifp),
			    ip6_sprintf(ip6bufs, &ip6->ip6_src),
			    ip6_sprintf(ip6bufd, &ip6->ip6_dst),
			    mb_copy->m_pkthdr.len);
			m_freem(mb_copy); /* simply discard the packet */
		}
	}
}

static int
register_send(struct mf6ctable *mfct, struct ip6_hdr *ip6, mifi_t mifi,
    struct mbuf *m)
{
#ifdef MRT6DEBUG
	char ip6bufs[INET6_ADDRSTRLEN], ip6bufd[INET6_ADDRSTRLEN];
#endif
	struct mbuf *mm;
	int i, len = m->m_pkthdr.len;
	static struct sockaddr_in6 sin6 = { sizeof(sin6), AF_INET6 };
	struct mrt6msg *im6;

	MRT6_DLOG(DEBUG_ANY, "src %s dst %s",
	    ip6_sprintf(ip6bufs, &ip6->ip6_src),
	    ip6_sprintf(ip6bufd, &ip6->ip6_dst));
	PIM6STAT_INC(pim6s_snd_registers);

	/* Make a copy of the packet to send to the user level process. */
	mm = m_gethdr(M_NOWAIT, MT_DATA);
	if (mm == NULL)
		return (ENOBUFS);
	mm->m_data += max_linkhdr;
	mm->m_len = sizeof(struct ip6_hdr);

	if ((mm->m_next = m_copym(m, 0, M_COPYALL, M_NOWAIT)) == NULL) {
		m_freem(mm);
		return (ENOBUFS);
	}
	i = MHLEN - M_LEADINGSPACE(mm);
	if (i > len)
		i = len;
	mm = m_pullup(mm, i);
	if (mm == NULL)
		return (ENOBUFS);
/* TODO: check it! */
	mm->m_pkthdr.len = len + sizeof(struct ip6_hdr);

	/*
	 * Send message to routing daemon
	 */
	sin6.sin6_addr = ip6->ip6_src;

	im6 = mtod(mm, struct mrt6msg *);
	im6->im6_msgtype      = MRT6MSG_WHOLEPKT;
	im6->im6_mbz          = 0;

	im6->im6_mif = mifi;

	/* iif info is not given for reg. encap.n */
	MRT6STAT_INC(mrt6s_upcalls);

	if (socket_send(mfct->router, mm, &sin6) < 0) {
		MRT6_DLOG(DEBUG_ANY, "ip6_mrouter socket queue full");
		MRT6STAT_INC(mrt6s_upq_sockfull);
		return (ENOBUFS);
	}
	return (0);
}

/*
 * pim6_encapcheck() is called by the encap6_input() path at runtime to
 * determine if a packet is for PIM; allowing PIM to be dynamically loaded
 * into the kernel.
 */
static int
pim6_encapcheck(const struct mbuf *m __unused, int off __unused,
    int proto __unused, void *arg __unused)
{

    KASSERT(proto == IPPROTO_PIM, ("not for IPPROTO_PIM"));
    return (8);		/* claim the datagram. */
}

/*
 * PIM sparse mode hook
 * Receives the pim control messages, and passes them up to the listening
 * socket, using rip6_input.
 * The only message processed is the REGISTER pim message; the pim header
 * is stripped off, and the inner packet is passed to register_mforward.
 */
static int
pim6_input(struct mbuf *m, int off, int proto, void *arg __unused)
{
	struct mf6ctable *mfct;
	struct pim *pim;
	struct ip6_hdr *ip6;
	int pimlen;
	int minlen;

	mfct = &V_mfctables[M_GETFIB(m)];

	PIM6STAT_INC(pim6s_rcv_total);

	/*
	 * Validate lengths
	 */
	pimlen = m->m_pkthdr.len - off;
	if (pimlen < PIM_MINLEN) {
		PIM6STAT_INC(pim6s_rcv_tooshort);
		MRT6_DLOG(DEBUG_PIM, "PIM packet too short");
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/*
	 * if the packet is at least as big as a REGISTER, go ahead
	 * and grab the PIM REGISTER header size, to avoid another
	 * possible m_pullup() later.
	 *
	 * PIM_MINLEN       == pimhdr + u_int32 == 8
	 * PIM6_REG_MINLEN   == pimhdr + reghdr + eip6hdr == 4 + 4 + 40
	 */
	minlen = (pimlen >= PIM6_REG_MINLEN) ? PIM6_REG_MINLEN : PIM_MINLEN;

	/*
	 * Make sure that the IP6 and PIM headers in contiguous memory, and
	 * possibly the PIM REGISTER header
	 */
	if (m->m_len < off + minlen) {
		m = m_pullup(m, off + minlen);
		if (m == NULL) {
			IP6STAT_INC(ip6s_exthdrtoolong);
			return (IPPROTO_DONE);
		}
	}
	ip6 = mtod(m, struct ip6_hdr *);
	pim = (struct pim *)((caddr_t)ip6 + off);

#define PIM6_CHECKSUM
#ifdef PIM6_CHECKSUM
	{
		int cksumlen;

		/*
		 * Validate checksum.
		 * If PIM REGISTER, exclude the data packet
		 */
		if (pim->pim_type == PIM_REGISTER)
			cksumlen = PIM_MINLEN;
		else
			cksumlen = pimlen;

		if (in6_cksum(m, IPPROTO_PIM, off, cksumlen)) {
			PIM6STAT_INC(pim6s_rcv_badsum);
			MRT6_DLOG(DEBUG_PIM, "invalid checksum");
			m_freem(m);
			return (IPPROTO_DONE);
		}
	}
#endif /* PIM_CHECKSUM */

	/* PIM version check */
	if (pim->pim_ver != PIM_VERSION) {
		PIM6STAT_INC(pim6s_rcv_badversion);
		MRT6_DLOG(DEBUG_ANY | DEBUG_ERR,
		    "incorrect version %d, expecting %d",
		    pim->pim_ver, PIM_VERSION);
		m_freem(m);
		return (IPPROTO_DONE);
	}

	if (pim->pim_type == PIM_REGISTER) {
		/*
		 * since this is a REGISTER, we'll make a copy of the register
		 * headers ip6+pim+u_int32_t+encap_ip6, to be passed up to the
		 * routing daemon.
		 */
		static struct sockaddr_in6 dst = { sizeof(dst), AF_INET6 };

		struct mbuf *mcp;
		struct ip6_hdr *eip6;
		u_int32_t *reghdr;
#ifdef MRT6DEBUG
		char ip6bufs[INET6_ADDRSTRLEN], ip6bufd[INET6_ADDRSTRLEN];
#endif

		PIM6STAT_INC(pim6s_rcv_registers);

		if (mfct->register_mif >= mfct->nummifs ||
		    mfct->register_mif == (mifi_t)-1) {
			MRT6_DLOG(DEBUG_PIM, "register mif not set: %d",
			    mfct->register_mif);
			m_freem(m);
			return (IPPROTO_DONE);
		}

		reghdr = (u_int32_t *)(pim + 1);

		if ((ntohl(*reghdr) & PIM_NULL_REGISTER))
			goto pim6_input_to_daemon;

		/*
		 * Validate length
		 */
		if (pimlen < PIM6_REG_MINLEN) {
			PIM6STAT_INC(pim6s_rcv_tooshort);
			PIM6STAT_INC(pim6s_rcv_badregisters);
			MRT6_DLOG(DEBUG_ANY | DEBUG_ERR, "register packet "
			    "size too small %d from %s",
			    pimlen, ip6_sprintf(ip6bufs, &ip6->ip6_src));
			m_freem(m);
			return (IPPROTO_DONE);
		}

		eip6 = (struct ip6_hdr *) (reghdr + 1);
		MRT6_DLOG(DEBUG_PIM, "eip6: %s -> %s, eip6 plen %d",
		    ip6_sprintf(ip6bufs, &eip6->ip6_src),
		    ip6_sprintf(ip6bufd, &eip6->ip6_dst),
		    ntohs(eip6->ip6_plen));

		/* verify the version number of the inner packet */
		if ((eip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
			PIM6STAT_INC(pim6s_rcv_badregisters);
			MRT6_DLOG(DEBUG_ANY, "invalid IP version (%d) "
			    "of the inner packet",
			    (eip6->ip6_vfc & IPV6_VERSION));
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/* verify the inner packet is destined to a mcast group */
		if (!IN6_IS_ADDR_MULTICAST(&eip6->ip6_dst)) {
			PIM6STAT_INC(pim6s_rcv_badregisters);
			MRT6_DLOG(DEBUG_PIM, "inner packet of register "
			    "is not multicast %s",
			    ip6_sprintf(ip6bufd, &eip6->ip6_dst));
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/*
		 * make a copy of the whole header to pass to the daemon later.
		 */
		mcp = m_copym(m, 0, off + PIM6_REG_MINLEN, M_NOWAIT);
		if (mcp == NULL) {
			MRT6_DLOG(DEBUG_ANY | DEBUG_ERR, "pim register: "
			    "could not copy register head");
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/*
		 * forward the inner ip6 packet; point m_data at the inner ip6.
		 */
		m_adj(m, off + PIM_MINLEN);
		MRT6_DLOG(DEBUG_PIM, "forwarding decapsulated register: "
		    "src %s, dst %s, mif %d",
		    ip6_sprintf(ip6bufs, &eip6->ip6_src),
		    ip6_sprintf(ip6bufd, &eip6->ip6_dst), mfct->register_mif);

		if_simloop(mfct->miftable[mfct->register_mif].m6_ifp, m,
		    dst.sin6_family, 0);

		/* prepare the register head to send to the mrouting daemon */
		m = mcp;
	}

	/*
	 * Pass the PIM message up to the daemon; if it is a register message
	 * pass the 'head' only up to the daemon. This includes the
	 * encapsulator ip6 header, pim header, register header and the
	 * encapsulated ip6 header.
	 */
  pim6_input_to_daemon:
	return (rip6_input(&m, &off, proto));
}

static void
ip6_mroute_rtnumfibs_change(void *arg __unused, uint32_t ntables)
{
	struct mf6ctable *mfctables, *omfctables;

	KASSERT(ntables >= V_nmfctables,
	    ("%s: ntables %u nmfctables %u", __func__, ntables, V_nmfctables));

	mfctables = mallocarray(ntables, sizeof(*mfctables), M_MRTABLE6,
	    M_WAITOK | M_ZERO);
	omfctables = V_mfctables;

	MROUTER6_LOCK();
	MFC6_LOCK();
	for (int i = 0; i < V_nmfctables; i++)
		memcpy(&mfctables[i], &omfctables[i], sizeof(*mfctables));
	atomic_store_rel_ptr((uintptr_t *)&V_mfctables, (uintptr_t)mfctables);
	MFC6_UNLOCK();
	MROUTER6_UNLOCK();

	NET_EPOCH_WAIT();

	V_nmfctables = ntables;
	free(omfctables, M_MRTABLE6);
}

static void
vnet_mroute_init(const void *unused __unused)
{
	ip6_mroute_rtnumfibs_change(NULL, V_rt_numfibs);

	callout_init_mtx(&V_expire_upcalls_ch, MFC6_LOCKPTR(), 0);
}
VNET_SYSINIT(vnet_mroute6_init, SI_SUB_PROTO_MC, SI_ORDER_ANY, vnet_mroute_init,
    NULL);

static void
vnet_mroute_uninit(const void *unused __unused)
{
	callout_drain(&V_expire_upcalls_ch);
	free(V_mfctables, M_MRTABLE6);
	V_mfctables = NULL;
}
VNET_SYSUNINIT(vnet_mroute6_uninit, SI_SUB_PROTO_MC, SI_ORDER_ANY,
    vnet_mroute_uninit, NULL);

static int
ip6_mroute_modevent(module_t mod, int type, void *unused)
{

	switch (type) {
	case MOD_LOAD:
		MROUTER6_LOCK_INIT();
		MFC6_LOCK_INIT();
		MIF6_LOCK_INIT();

		rtnumfibs_change_tag = EVENTHANDLER_REGISTER(
		    rtnumfibs_change, ip6_mroute_rtnumfibs_change,
		    NULL, EVENTHANDLER_PRI_ANY);

		pim6_encap_cookie = ip6_encap_attach(&ipv6_encap_cfg,
		    NULL, M_WAITOK);
		if (pim6_encap_cookie == NULL) {
			printf("ip6_mroute: unable to attach pim6 encap\n");
			MIF6_LOCK_DESTROY();
			MFC6_LOCK_DESTROY();
			MROUTER6_LOCK_DESTROY();
			return (EINVAL);
		}

		ip6_mforward = X_ip6_mforward;
		ip6_mrouter_done = X_ip6_mrouter_done;
		ip6_mrouter_get = X_ip6_mrouter_get;
		ip6_mrouter_set = X_ip6_mrouter_set;
		mrt6_ioctl = X_mrt6_ioctl;
		break;

	case MOD_UNLOAD:
		if (V_ip6_mrouting_enabled)
			return (EBUSY);

		EVENTHANDLER_DEREGISTER(rtnumfibs_change,
		    rtnumfibs_change_tag);

		if (pim6_encap_cookie) {
			ip6_encap_detach(pim6_encap_cookie);
			pim6_encap_cookie = NULL;
		}

		ip6_mforward = NULL;
		ip6_mrouter_done = NULL;
		ip6_mrouter_get = NULL;
		ip6_mrouter_set = NULL;
		mrt6_ioctl = NULL;

		MIF6_LOCK_DESTROY();
		MFC6_LOCK_DESTROY();
		MROUTER6_LOCK_DESTROY();
		break;

	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t ip6_mroutemod = {
	"ip6_mroute",
	ip6_mroute_modevent,
	0
};

DECLARE_MODULE(ip6_mroute, ip6_mroutemod, SI_SUB_PROTO_MC, SI_ORDER_MIDDLE);
