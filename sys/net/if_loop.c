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
 *	@(#)if_loop.c	8.2 (Berkeley) 1/9/95
 * $FreeBSD$
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipx.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#endif

#ifdef TINY_LOMTU
#define	LOMTU	(1024+512)
#elif defined(LARGE_LOMTU)
#define LOMTU	131072
#else
#define LOMTU	16384
#endif

#define LONAME	"lo"

struct lo_softc {
	struct	ifnet sc_if;		/* network-visible interface */
	LIST_ENTRY(lo_softc) sc_next;
};

int		loioctl(struct ifnet *, u_long, caddr_t);
static void	lortrequest(int, struct rtentry *, struct rt_addrinfo *);
int		looutput(struct ifnet *ifp, struct mbuf *m,
		    struct sockaddr *dst, struct rtentry *rt);
int		lo_clone_create(struct if_clone *, int);
void		lo_clone_destroy(struct ifnet *);

struct ifnet *loif = NULL;			/* Used externally */

static MALLOC_DEFINE(M_LO, LONAME, "Loopback Interface");

static LIST_HEAD(lo_list, lo_softc) lo_list;

struct if_clone lo_cloner = IF_CLONE_INITIALIZER(LONAME,
    lo_clone_create, lo_clone_destroy, 1, IF_MAXUNIT);

void
lo_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct lo_softc *sc;
	
	sc = ifp->if_softc;

	/* XXX: destroying lo0 will lead to panics. */
	KASSERT(loif != ifp, ("%s: destroying lo0", __func__));

	bpfdetach(ifp);
	if_detach(ifp);
	LIST_REMOVE(sc, sc_next);
	free(sc, M_LO);
}

int
lo_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct lo_softc *sc;

	MALLOC(sc, struct lo_softc *, sizeof(*sc), M_LO, M_WAITOK | M_ZERO);

	sc->sc_if.if_name = LONAME;
	sc->sc_if.if_unit = unit;
	sc->sc_if.if_mtu = LOMTU;
	sc->sc_if.if_flags = IFF_LOOPBACK | IFF_MULTICAST;
	sc->sc_if.if_ioctl = loioctl;
	sc->sc_if.if_output = looutput;
	sc->sc_if.if_type = IFT_LOOP;
	sc->sc_if.if_snd.ifq_maxlen = ifqmaxlen;
	sc->sc_if.if_softc = sc;
	if_attach(&sc->sc_if);
	bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int));
	LIST_INSERT_HEAD(&lo_list, sc, sc_next);
	if (loif == NULL)
		loif = &sc->sc_if;

	return (0);
}

static int
loop_modevent(module_t mod, int type, void *data) 
{ 
	switch (type) { 
	case MOD_LOAD: 
		LIST_INIT(&lo_list);
		if_clone_attach(&lo_cloner);
		break; 
	case MOD_UNLOAD: 
		printf("loop module unload - not possible for this module type\n"); 
		return EINVAL; 
	} 
	return 0; 
} 

static moduledata_t loop_mod = { 
	"loop", 
	loop_modevent, 
	0
}; 

DECLARE_MODULE(loop, loop_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

int
looutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	register struct mbuf *m;
	struct sockaddr *dst;
	register struct rtentry *rt;
{
#ifdef INET6
	struct mbuf *n;
#endif

	M_ASSERTPKTHDR(m); /* check if we have the packet header */

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}
#ifdef INET6
	/*
	 * KAME requires that the packet to be contiguous on the
	 * mbuf.  We need to make that sure.
	 * this kind of code should be avoided.
	 *
	 * XXX: KAME may no longer need contiguous packets.  Once
	 * that has been verified, the following code _should_ be
	 * removed.
	 */

	if (m && m->m_next != NULL) {

		n = m_defrag(m, M_DONTWAIT);

		if (n == NULL) {
			m_freem(m);
			return (ENOBUFS);
		} else {
			m = n;
		}
	}
#endif

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
#if 1	/* XXX */
	switch (dst->sa_family) {
	case AF_INET:
	case AF_INET6:
	case AF_IPX:
	case AF_APPLETALK:
		break;
	default:
		printf("looutput: af=%d unexpected\n", dst->sa_family);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
#endif
	return(if_simloop(ifp, m, dst->sa_family, 0));
}

/*
 * if_simloop()
 *
 * This function is to support software emulation of hardware loopback,
 * i.e., for interfaces with the IFF_SIMPLEX attribute. Since they can't
 * hear their own broadcasts, we create a copy of the packet that we
 * would normally receive via a hardware loopback.
 *
 * This function expects the packet to include the media header of length hlen.
 */

int
if_simloop(ifp, m, af, hlen)
	struct ifnet *ifp;
	struct mbuf *m;
	int af;
	int hlen;
{
	int isr;

	M_ASSERTPKTHDR(m);
	m->m_pkthdr.rcvif = ifp;

	/* BPF write needs to be handled specially */
	if (af == AF_UNSPEC) {
		KASSERT(m->m_len >= sizeof(int), ("if_simloop: m_len"));
		af = *(mtod(m, int *));
		m->m_len -= sizeof(int);
		m->m_pkthdr.len -= sizeof(int);
		m->m_data += sizeof(int);
	}

	/* Let BPF see incoming packet */
	if (ifp->if_bpf) {
		struct mbuf m0, *n = m;

		if (ifp->if_bpf->bif_dlt == DLT_NULL) {
			/*
			 * We need to prepend the address family as
			 * a four byte field.  Cons up a dummy header
			 * to pacify bpf.  This is safe because bpf
			 * will only read from the mbuf (i.e., it won't
			 * try to free it or keep a pointer a to it).
			 */
			m0.m_next = m;
			m0.m_len = 4;
			m0.m_data = (char *)&af;
			n = &m0;
		}
		BPF_MTAP(ifp, n);
	}

	/* Strip away media header */
	if (hlen > 0) {
		m_adj(m, hlen);
#if defined(__alpha__) || defined(__ia64__) || defined(__sparc64__)
		/* The alpha doesn't like unaligned data.
		 * We move data down in the first mbuf */
		if (mtod(m, vm_offset_t) & 3) {
			KASSERT(hlen >= 3, ("if_simloop: hlen too small"));
			bcopy(m->m_data, 
			    (char *)(mtod(m, vm_offset_t) 
				- (mtod(m, vm_offset_t) & 3)),
			    m->m_len);
			mtod(m,vm_offset_t) -= (mtod(m, vm_offset_t) & 3);
		}
#endif
	}

	/* Deliver to upper layer protocol */
	switch (af) {
#ifdef INET
	case AF_INET:
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		m->m_flags |= M_LOOP;
		isr = NETISR_IPV6;
		break;
#endif
#ifdef IPX
	case AF_IPX:
		isr = NETISR_IPX;
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
		isr = NETISR_ATALK2;
		break;
#endif
	default:
		printf("if_simloop: can't handle af=%d\n", af);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	netisr_dispatch(isr, m);
	return (0);
}

/* ARGSUSED */
static void
lortrequest(cmd, rt, info)
	int cmd;
	struct rtentry *rt;
	struct rt_addrinfo *info;
{
	if (rt) {
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu; /* for ISO */
		/*
		 * For optimal performance, the send and receive buffers
		 * should be at least twice the MTU plus a little more for
		 * overhead.
		 */
		rt->rt_rmx.rmx_recvpipe =
			rt->rt_rmx.rmx_sendpipe = 3 * LOMTU;
	}
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
loioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	register struct ifaddr *ifa;
	register struct ifreq *ifr = (struct ifreq *)data;
	register int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP | IFF_RUNNING;
		ifa = (struct ifaddr *)data;
		ifa->ifa_rtrequest = lortrequest;
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

	case SIOCSIFMTU:
		ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		break;

	default:
		error = EINVAL;
	}
	return (error);
}
