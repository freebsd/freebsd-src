/*	$KAME: if_faith.c,v 1.23 2001/12/17 13:55:29 sumikawa Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * $FreeBSD$
 */
/*
 * derived from
 *	@(#)if_loop.c	8.1 (Berkeley) 6/10/93
 * Id: if_loop.c,v 1.22 1996/06/19 16:24:10 wollman Exp
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include <net/net_osdep.h>

#define FAITHNAME	"faith"

struct faith_softc {
	struct ifnet sc_if;	/* must be first */
	LIST_ENTRY(faith_softc) sc_list;
};

static int faithioctl(struct ifnet *, u_long, caddr_t);
int faithoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);
static void faithrtrequest(int, struct rtentry *, struct rt_addrinfo *);
#ifdef INET6
static int faithprefix(struct in6_addr *);
#endif

static int faithmodevent(module_t, int, void *);

static MALLOC_DEFINE(M_FAITH, FAITHNAME, "Firewall Assisted Tunnel Interface");
static LIST_HEAD(, faith_softc) faith_softc_list;

int	faith_clone_create(struct if_clone *, int);
void	faith_clone_destroy(struct ifnet *);

struct if_clone faith_cloner = IF_CLONE_INITIALIZER(FAITHNAME,
    faith_clone_create, faith_clone_destroy, 0, IF_MAXUNIT);

#define	FAITHMTU	1500

static int
faithmodevent(mod, type, data)
	module_t mod;
	int type;
	void *data;
{

	switch (type) {
	case MOD_LOAD:
		LIST_INIT(&faith_softc_list);
		if_clone_attach(&faith_cloner);

#ifdef INET6
		faithprefix_p = faithprefix;
#endif

		break;
	case MOD_UNLOAD:
#ifdef INET6
		faithprefix_p = NULL;
#endif

		if_clone_detach(&faith_cloner);

		while (!LIST_EMPTY(&faith_softc_list))
			faith_clone_destroy(
			    &LIST_FIRST(&faith_softc_list)->sc_if);

		break;
	}
	return 0;
}

static moduledata_t faith_mod = {
	"if_faith",
	faithmodevent,
	0
};

DECLARE_MODULE(if_faith, faith_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_faith, 1);

int
faith_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct faith_softc *sc;

	sc = malloc(sizeof(struct faith_softc), M_FAITH, M_WAITOK);
	bzero(sc, sizeof(struct faith_softc));

	sc->sc_if.if_softc = sc;
	if_initname(&sc->sc_if, ifc->ifc_name, unit);

	sc->sc_if.if_mtu = FAITHMTU;
	/* Change to BROADCAST experimentaly to announce its prefix. */
	sc->sc_if.if_flags = /* IFF_LOOPBACK */ IFF_BROADCAST | IFF_MULTICAST;
	sc->sc_if.if_ioctl = faithioctl;
	sc->sc_if.if_output = faithoutput;
	sc->sc_if.if_type = IFT_FAITH;
	sc->sc_if.if_hdrlen = 0;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_snd.ifq_maxlen = ifqmaxlen;
	if_attach(&sc->sc_if);
	bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int));
	LIST_INSERT_HEAD(&faith_softc_list, sc, sc_list);
	return (0);
}

void
faith_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct faith_softc *sc = (void *) ifp;

	LIST_REMOVE(sc, sc_list);
	bpfdetach(ifp);
	if_detach(ifp);

	free(sc, M_FAITH);
}

int
faithoutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	int isr;

	M_ASSERTPKTHDR(m);

	/* BPF write needs to be handled specially */
	if (dst->sa_family == AF_UNSPEC) {
		dst->sa_family = *(mtod(m, int *));
		m->m_len -= sizeof(int);
		m->m_pkthdr.len -= sizeof(int);
		m->m_data += sizeof(int);
	}

	if (ifp->if_bpf) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a faith header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer a to it).
		 */
		struct mbuf m0;
		u_int32_t af = dst->sa_family;

		m0.m_next = m;
		m0.m_len = 4;
		m0.m_data = (char *)&af;

		BPF_MTAP(ifp, &m0);
	}

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}
	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		isr = NETISR_IPV6;
		break;
#endif
	default:
		m_freem(m);
		return EAFNOSUPPORT;
	}

	/* XXX do we need more sanity checks? */

	m->m_pkthdr.rcvif = ifp;
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	netisr_dispatch(isr, m);
	return (0);
}

/* ARGSUSED */
static void
faithrtrequest(cmd, rt, info)
	int cmd;
	struct rtentry *rt;
	struct rt_addrinfo *info;
{
	RT_LOCK_ASSERT(rt);

	if (rt) {
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu; /* for ISO */
		/*
		 * For optimal performance, the send and receive buffers
		 * should be at least twice the MTU plus a little more for
		 * overhead.
		 */
		rt->rt_rmx.rmx_recvpipe =
			rt->rt_rmx.rmx_sendpipe = 3 * FAITHMTU;
	}
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
static int
faithioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ifaddr *ifa;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP | IFF_RUNNING;
		ifa = (struct ifaddr *)data;
		ifa->ifa_rtrequest = faithrtrequest;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

#ifdef SIOCSIFMTU
	case SIOCSIFMTU:
		ifp->if_mtu = ifr->ifr_mtu;
		break;
#endif

	case SIOCSIFFLAGS:
		break;

	default:
		error = EINVAL;
	}
	return (error);
}

#ifdef INET6
/*
 * XXX could be slow
 * XXX could be layer violation to call sys/net from sys/netinet6
 */
static int
faithprefix(in6)
	struct in6_addr *in6;
{
	struct rtentry *rt;
	struct sockaddr_in6 sin6;
	int ret;

	if (ip6_keepfaith == 0)
		return 0;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *in6;
	rt = rtalloc1((struct sockaddr *)&sin6, 0, 0UL);
	if (rt && rt->rt_ifp && rt->rt_ifp->if_type == IFT_FAITH &&
	    (rt->rt_ifp->if_flags & IFF_UP) != 0)
		ret = 1;
	else
		ret = 0;
	if (rt)
		RTFREE_LOCKED(rt);
	return ret;
}
#endif
