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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
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

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#endif

int loioctl __P((struct ifnet *, u_long, caddr_t));
static void lortrequest __P((int, struct rtentry *, struct sockaddr *));

int looutput __P((struct ifnet *ifp,
		struct mbuf *m, struct sockaddr *dst, struct rtentry *rt));

#ifdef TINY_LOMTU
#define	LOMTU	(1024+512)
#elif defined(LARGE_LOMTU)
#define LOMTU	131072
#else
#define LOMTU	16384
#endif

static int nloop = 1;

struct ifnet *loif;			/* Used externally */

static MALLOC_DEFINE(M_LO, "lo", "Loopback Interface");

struct lo_softc {
	struct	ifnet sc_if;		/* network-visible interface */
        LIST_ENTRY(lo_softc) sc_next;
};
static LIST_HEAD(lo_list, lo_softc) lo_list;

static void
locreate(int unit)
{
	struct lo_softc *sc;

	MALLOC(sc, struct lo_softc *, sizeof(*sc), M_LO, M_WAITOK | M_ZERO);

	sc->sc_if.if_name = "lo";
	sc->sc_if.if_unit = unit;
	sc->sc_if.if_mtu = LOMTU;
	sc->sc_if.if_flags = IFF_LOOPBACK | IFF_MULTICAST;
	sc->sc_if.if_ioctl = loioctl;
	sc->sc_if.if_output = looutput;
	sc->sc_if.if_type = IFT_LOOP;
	sc->sc_if.if_snd.ifq_maxlen = ifqmaxlen;
	if_attach(&sc->sc_if);
	bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int));
	LIST_INSERT_HEAD(&lo_list, sc, sc_next);
	if (loif == NULL)
		loif = &sc->sc_if;
}

static void
lodestroy(struct lo_softc *sc)
{
	bpfdetach(&sc->sc_if);
	if_detach(&sc->sc_if);
	LIST_REMOVE(sc, sc_next);
	FREE(sc, M_LO);
}


static int
sysctl_net_nloop(SYSCTL_HANDLER_ARGS)
{
	int newnloop;
	int error;

	newnloop = nloop;

	error = sysctl_handle_opaque(oidp, &newnloop, sizeof newnloop, req);
	if (error || !req->newptr)
		return (error);

	if (newnloop < 1)
		return (EINVAL);
	while (newnloop > nloop) {
		locreate(nloop);
		nloop++;
	}
	while (newnloop < nloop) {
		lodestroy(LIST_FIRST(&lo_list));
		nloop--;
	}
	return (0);
}
SYSCTL_PROC(_net, OID_AUTO, nloop, CTLTYPE_INT | CTLFLAG_RW,
	    0, 0, sysctl_net_nloop, "I", "");

static int
loop_modevent(module_t mod, int type, void *data) 
{ 
	int i;

	switch (type) { 
	case MOD_LOAD: 
		TUNABLE_INT_FETCH("net.nloop", &nloop);
		if (nloop < 1)			/* sanity check */
			nloop = 1;
		for (i = 0; i < nloop; i++)
			locreate(i);
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
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("looutput no HDR");

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}
	/*
	 * KAME requires that the packet to be contiguous on the
	 * mbuf.  We need to make that sure.
	 * this kind of code should be avoided.
	 * XXX: fails to join if interface MTU > MCLBYTES.  jumbogram?
	 */
	if (m && m->m_next != NULL && m->m_pkthdr.len < MCLBYTES) {
		struct mbuf *n;

		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (!n)
			goto contiguousfail;
		MCLGET(n, M_DONTWAIT);
		if (! (n->m_flags & M_EXT)) {
			m_freem(n);
			goto contiguousfail;
		}

		m_copydata(m, 0, m->m_pkthdr.len, mtod(n, caddr_t));
		n->m_pkthdr = m->m_pkthdr;
		n->m_len = m->m_pkthdr.len;
		n->m_pkthdr.aux = m->m_pkthdr.aux;
		m->m_pkthdr.aux = (struct mbuf *)NULL;
		m_freem(m);
		m = n;
	}
	if (0) {
contiguousfail:
		printf("looutput: mbuf allocation failed\n");
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
#if 1	/* XXX */
	switch (dst->sa_family) {
	case AF_INET:
	case AF_INET6:
	case AF_IPX:
	case AF_NS:
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
	struct ifqueue *inq = 0;

	KASSERT((m->m_flags & M_PKTHDR) != 0, ("if_simloop: no HDR"));
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
		bpf_mtap(ifp, n);
	}

	/* Strip away media header */
	if (hlen > 0) {
		m_adj(m, hlen);
#if defined(__alpha__) || defined(__ia64__)
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
		inq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		m->m_flags |= M_LOOP;
		inq = &ip6intrq;
		isr = NETISR_IPV6;
		break;
#endif
#ifdef IPX
	case AF_IPX:
		inq = &ipxintrq;
		isr = NETISR_IPX;
		break;
#endif
#ifdef NS
	case AF_NS:
		inq = &nsintrq;
		isr = NETISR_NS;
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
	        inq = &atintrq2;
		isr = NETISR_ATALK;
		break;
#endif
	default:
		printf("if_simloop: can't handle af=%d\n", af);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	(void) IF_HANDOFF(inq, m, NULL);
	schednetisr(isr);
	return (0);
}

/* ARGSUSED */
static void
lortrequest(cmd, rt, sa)
	int cmd;
	struct rtentry *rt;
	struct sockaddr *sa;
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
