/*-
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
 * $FreeBSD: src/sys/net/if_loop.c,v 1.112 2007/02/09 00:09:35 cognet Exp $
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
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_clone.h>
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
	struct	ifnet *sc_ifp;
	LIST_ENTRY(lo_softc) sc_next;
};

int		loioctl(struct ifnet *, u_long, caddr_t);
static void	lortrequest(int, struct rtentry *, struct rt_addrinfo *);
int		looutput(struct ifnet *ifp, struct mbuf *m,
		    struct sockaddr *dst, struct rtentry *rt);
static int	lo_clone_create(struct if_clone *, int, caddr_t);
static void	lo_clone_destroy(struct ifnet *);

struct ifnet *loif = NULL;			/* Used externally */

static MALLOC_DEFINE(M_LO, LONAME, "Loopback Interface");

static struct mtx lo_mtx;
static LIST_HEAD(lo_list, lo_softc) lo_list;

IFC_SIMPLE_DECLARE(lo, 1);

static void
lo_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct lo_softc *sc;
	
	sc = ifp->if_softc;

	/* XXX: destroying lo0 will lead to panics. */
	KASSERT(loif != ifp, ("%s: destroying lo0", __func__));

	mtx_lock(&lo_mtx);
	LIST_REMOVE(sc, sc_next);
	mtx_unlock(&lo_mtx);
	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);
	free(sc, M_LO);
}

static int
lo_clone_create(ifc, unit, params)
	struct if_clone *ifc;
	int unit;
	caddr_t params;
{
	struct ifnet *ifp;
	struct lo_softc *sc;

	MALLOC(sc, struct lo_softc *, sizeof(*sc), M_LO, M_WAITOK | M_ZERO);
	ifp = sc->sc_ifp = if_alloc(IFT_LOOP);
	if (ifp == NULL) {
		free(sc, M_LO);
		return (ENOSPC);
	}

	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_mtu = LOMTU;
	ifp->if_flags = IFF_LOOPBACK | IFF_MULTICAST;
	ifp->if_ioctl = loioctl;
	ifp->if_output = looutput;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_softc = sc;
	if_attach(ifp);
	bpfattach(ifp, DLT_NULL, sizeof(u_int32_t));
	mtx_lock(&lo_mtx);
	LIST_INSERT_HEAD(&lo_list, sc, sc_next);
	mtx_unlock(&lo_mtx);
	if (loif == NULL)
		loif = ifp;

	return (0);
}

static int
loop_modevent(module_t mod, int type, void *data) 
{ 
	switch (type) { 
	case MOD_LOAD: 
		mtx_init(&lo_mtx, "lo_mtx", NULL, MTX_DEF);
		LIST_INIT(&lo_list);
		if_clone_attach(&lo_cloner);
		break; 
	case MOD_UNLOAD: 
		printf("loop module unload - not possible for this module type\n"); 
		return EINVAL; 
	default:
		return EOPNOTSUPP;
	} 
	return 0; 
} 

static moduledata_t loop_mod = { 
	"loop", 
	loop_modevent, 
	0
}; 

DECLARE_MODULE(loop, loop_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);

int
looutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	register struct mbuf *m;
	struct sockaddr *dst;
	register struct rtentry *rt;
{
	u_int32_t af;

	M_ASSERTPKTHDR(m); /* check if we have the packet header */

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC) {
		bcopy(dst->sa_data, &af, sizeof(af));
		dst->sa_family = af;
	}

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
	m_tag_delete_nonpersistent(m);
	m->m_pkthdr.rcvif = ifp;

	/*
	 * Let BPF see incoming packet in the following manner:
	 *  - Emulated packet loopback for a simplex interface 
	 *    (net/if_ethersubr.c)
	 *	-> passes it to ifp's BPF
	 *  - IPv4/v6 multicast packet loopback (netinet(6)/ip(6)_output.c)
	 *	-> not passes it to any BPF
	 *  - Normal packet loopback from myself to myself (net/if_loop.c)
	 *	-> passes to lo0's BPF (even in case of IPv6, where ifp!=lo0)
	 */
	if (hlen > 0) {
		if (bpf_peers_present(ifp->if_bpf)) {
			bpf_mtap(ifp->if_bpf, m);
		}
	} else {
		if (bpf_peers_present(loif->if_bpf)) {
			if ((m->m_flags & M_MCAST) == 0 || loif == ifp) {
				/* XXX beware sizeof(af) != 4 */
				u_int32_t af1 = af;	

				/*
				 * We need to prepend the address family.
				 */
				bpf_mtap2(loif->if_bpf, &af1, sizeof(af1), m);
			}
		}
	}

	/* Strip away media header */
	if (hlen > 0) {
		m_adj(m, hlen);
#ifndef __NO_STRICT_ALIGNMENT
		/*
		 * Some archs do not like unaligned data, so
		 * we move data down in the first mbuf.
		 */
		if (mtod(m, vm_offset_t) & 3) {
			KASSERT(hlen >= 3, ("if_simloop: hlen too small"));
			bcopy(m->m_data, 
			    (char *)(mtod(m, vm_offset_t) 
				- (mtod(m, vm_offset_t) & 3)),
			    m->m_len);
			m->m_data -= (mtod(m,vm_offset_t) & 3);
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
	netisr_queue(isr, m);	/* mbuf is free'd on failure. */
	return (0);
}

/* ARGSUSED */
static void
lortrequest(cmd, rt, info)
	int cmd;
	struct rtentry *rt;
	struct rt_addrinfo *info;
{
	RT_LOCK_ASSERT(rt);
	rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
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
		ifp->if_flags |= IFF_UP;
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
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
