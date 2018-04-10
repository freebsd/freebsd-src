/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * Copyright (c) 2014 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: if_gre.c,v 1.49 2003/12/11 00:22:29 itojun Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/vnet.h>
#include <net/route.h>

#include <netinet/in.h>
#ifdef INET
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif

#include <netinet/ip_encap.h>
#include <net/bpf.h>
#include <net/if_gre.h>

#include <machine/in_cksum.h>
#include <security/mac/mac_framework.h>

#define	GREMTU			1476
static const char grename[] = "gre";
static MALLOC_DEFINE(M_GRE, grename, "Generic Routing Encapsulation");
static VNET_DEFINE(struct mtx, gre_mtx);
#define	V_gre_mtx	VNET(gre_mtx)
#define	GRE_LIST_LOCK_INIT(x)		mtx_init(&V_gre_mtx, "gre_mtx", NULL, \
					    MTX_DEF)
#define	GRE_LIST_LOCK_DESTROY(x)	mtx_destroy(&V_gre_mtx)
#define	GRE_LIST_LOCK(x)		mtx_lock(&V_gre_mtx)
#define	GRE_LIST_UNLOCK(x)		mtx_unlock(&V_gre_mtx)

static VNET_DEFINE(LIST_HEAD(, gre_softc), gre_softc_list);
#define	V_gre_softc_list	VNET(gre_softc_list)
static struct sx gre_ioctl_sx;
SX_SYSINIT(gre_ioctl_sx, &gre_ioctl_sx, "gre_ioctl");

static int	gre_clone_create(struct if_clone *, int, caddr_t);
static void	gre_clone_destroy(struct ifnet *);
static VNET_DEFINE(struct if_clone *, gre_cloner);
#define	V_gre_cloner	VNET(gre_cloner)

static void	gre_qflush(struct ifnet *);
static int	gre_transmit(struct ifnet *, struct mbuf *);
static int	gre_ioctl(struct ifnet *, u_long, caddr_t);
static int	gre_output(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);

static void	gre_updatehdr(struct gre_softc *);
static int	gre_set_tunnel(struct ifnet *, struct sockaddr *,
    struct sockaddr *);
static void	gre_delete_tunnel(struct ifnet *);

SYSCTL_DECL(_net_link);
static SYSCTL_NODE(_net_link, IFT_TUNNEL, gre, CTLFLAG_RW, 0,
    "Generic Routing Encapsulation");
#ifndef MAX_GRE_NEST
/*
 * This macro controls the default upper limitation on nesting of gre tunnels.
 * Since, setting a large value to this macro with a careless configuration
 * may introduce system crash, we don't allow any nestings by default.
 * If you need to configure nested gre tunnels, you can define this macro
 * in your kernel configuration file.  However, if you do so, please be
 * careful to configure the tunnels so that it won't make a loop.
 */
#define MAX_GRE_NEST 1
#endif

static VNET_DEFINE(int, max_gre_nesting) = MAX_GRE_NEST;
#define	V_max_gre_nesting	VNET(max_gre_nesting)
SYSCTL_INT(_net_link_gre, OID_AUTO, max_nesting, CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(max_gre_nesting), 0, "Max nested tunnels");

static void
vnet_gre_init(const void *unused __unused)
{
	LIST_INIT(&V_gre_softc_list);
	GRE_LIST_LOCK_INIT();
	V_gre_cloner = if_clone_simple(grename, gre_clone_create,
	    gre_clone_destroy, 0);
}
VNET_SYSINIT(vnet_gre_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_gre_init, NULL);

static void
vnet_gre_uninit(const void *unused __unused)
{

	if_clone_detach(V_gre_cloner);
	GRE_LIST_LOCK_DESTROY();
}
VNET_SYSUNINIT(vnet_gre_uninit, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_gre_uninit, NULL);

static int
gre_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct gre_softc *sc;

	sc = malloc(sizeof(struct gre_softc), M_GRE, M_WAITOK | M_ZERO);
	sc->gre_fibnum = curthread->td_proc->p_fibnum;
	GRE2IFP(sc) = if_alloc(IFT_TUNNEL);
	GRE_LOCK_INIT(sc);
	GRE2IFP(sc)->if_softc = sc;
	if_initname(GRE2IFP(sc), grename, unit);

	GRE2IFP(sc)->if_mtu = GREMTU;
	GRE2IFP(sc)->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	GRE2IFP(sc)->if_output = gre_output;
	GRE2IFP(sc)->if_ioctl = gre_ioctl;
	GRE2IFP(sc)->if_transmit = gre_transmit;
	GRE2IFP(sc)->if_qflush = gre_qflush;
	GRE2IFP(sc)->if_capabilities |= IFCAP_LINKSTATE;
	GRE2IFP(sc)->if_capenable |= IFCAP_LINKSTATE;
	if_attach(GRE2IFP(sc));
	bpfattach(GRE2IFP(sc), DLT_NULL, sizeof(u_int32_t));
	GRE_LIST_LOCK();
	LIST_INSERT_HEAD(&V_gre_softc_list, sc, gre_list);
	GRE_LIST_UNLOCK();
	return (0);
}

static void
gre_clone_destroy(struct ifnet *ifp)
{
	struct gre_softc *sc;

	sx_xlock(&gre_ioctl_sx);
	sc = ifp->if_softc;
	gre_delete_tunnel(ifp);
	GRE_LIST_LOCK();
	LIST_REMOVE(sc, gre_list);
	GRE_LIST_UNLOCK();
	bpfdetach(ifp);
	if_detach(ifp);
	ifp->if_softc = NULL;
	sx_xunlock(&gre_ioctl_sx);

	if_free(ifp);
	GRE_LOCK_DESTROY(sc);
	free(sc, M_GRE);
}

static int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	GRE_RLOCK_TRACKER;
	struct ifreq *ifr = (struct ifreq *)data;
	struct sockaddr *src, *dst;
	struct gre_softc *sc;
#ifdef INET
	struct sockaddr_in *sin = NULL;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6 = NULL;
#endif
	uint32_t opt;
	int error;

	switch (cmd) {
	case SIOCSIFMTU:
		 /* XXX: */
		if (ifr->ifr_mtu < 576)
			return (EINVAL);
		ifp->if_mtu = ifr->ifr_mtu;
		return (0);
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
	case SIOCSIFFLAGS:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		return (0);
	case GRESADDRS:
	case GRESADDRD:
	case GREGADDRS:
	case GREGADDRD:
	case GRESPROTO:
	case GREGPROTO:
		return (EOPNOTSUPP);
	}
	src = dst = NULL;
	sx_xlock(&gre_ioctl_sx);
	sc = ifp->if_softc;
	if (sc == NULL) {
		error = ENXIO;
		goto end;
	}
	error = 0;
	switch (cmd) {
	case SIOCSIFPHYADDR:
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
#endif
		error = EINVAL;
		switch (cmd) {
#ifdef INET
		case SIOCSIFPHYADDR:
			src = (struct sockaddr *)
				&(((struct in_aliasreq *)data)->ifra_addr);
			dst = (struct sockaddr *)
				&(((struct in_aliasreq *)data)->ifra_dstaddr);
			break;
#endif
#ifdef INET6
		case SIOCSIFPHYADDR_IN6:
			src = (struct sockaddr *)
				&(((struct in6_aliasreq *)data)->ifra_addr);
			dst = (struct sockaddr *)
				&(((struct in6_aliasreq *)data)->ifra_dstaddr);
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			goto end;
		}
		/* sa_family must be equal */
		if (src->sa_family != dst->sa_family ||
		    src->sa_len != dst->sa_len)
			goto end;

		/* validate sa_len */
		switch (src->sa_family) {
#ifdef INET
		case AF_INET:
			if (src->sa_len != sizeof(struct sockaddr_in))
				goto end;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (src->sa_len != sizeof(struct sockaddr_in6))
				goto end;
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			goto end;
		}
		/* check sa_family looks sane for the cmd */
		error = EAFNOSUPPORT;
		switch (cmd) {
#ifdef INET
		case SIOCSIFPHYADDR:
			if (src->sa_family == AF_INET)
				break;
			goto end;
#endif
#ifdef INET6
		case SIOCSIFPHYADDR_IN6:
			if (src->sa_family == AF_INET6)
				break;
			goto end;
#endif
		}
		error = EADDRNOTAVAIL;
		switch (src->sa_family) {
#ifdef INET
		case AF_INET:
			if (satosin(src)->sin_addr.s_addr == INADDR_ANY ||
			    satosin(dst)->sin_addr.s_addr == INADDR_ANY)
				goto end;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (IN6_IS_ADDR_UNSPECIFIED(&satosin6(src)->sin6_addr)
			    ||
			    IN6_IS_ADDR_UNSPECIFIED(&satosin6(dst)->sin6_addr))
				goto end;
			/*
			 * Check validity of the scope zone ID of the
			 * addresses, and convert it into the kernel
			 * internal form if necessary.
			 */
			error = sa6_embedscope(satosin6(src), 0);
			if (error != 0)
				goto end;
			error = sa6_embedscope(satosin6(dst), 0);
			if (error != 0)
				goto end;
#endif
		}
		error = gre_set_tunnel(ifp, src, dst);
		break;
	case SIOCDIFPHYADDR:
		gre_delete_tunnel(ifp);
		break;
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
#ifdef INET6
	case SIOCGIFPSRCADDR_IN6:
	case SIOCGIFPDSTADDR_IN6:
#endif
		if (sc->gre_family == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		GRE_RLOCK(sc);
		switch (cmd) {
#ifdef INET
		case SIOCGIFPSRCADDR:
		case SIOCGIFPDSTADDR:
			if (sc->gre_family != AF_INET) {
				error = EADDRNOTAVAIL;
				break;
			}
			sin = (struct sockaddr_in *)&ifr->ifr_addr;
			memset(sin, 0, sizeof(*sin));
			sin->sin_family = AF_INET;
			sin->sin_len = sizeof(*sin);
			break;
#endif
#ifdef INET6
		case SIOCGIFPSRCADDR_IN6:
		case SIOCGIFPDSTADDR_IN6:
			if (sc->gre_family != AF_INET6) {
				error = EADDRNOTAVAIL;
				break;
			}
			sin6 = (struct sockaddr_in6 *)
				&(((struct in6_ifreq *)data)->ifr_addr);
			memset(sin6, 0, sizeof(*sin6));
			sin6->sin6_family = AF_INET6;
			sin6->sin6_len = sizeof(*sin6);
			break;
#endif
		}
		if (error == 0) {
			switch (cmd) {
#ifdef INET
			case SIOCGIFPSRCADDR:
				sin->sin_addr = sc->gre_oip.ip_src;
				break;
			case SIOCGIFPDSTADDR:
				sin->sin_addr = sc->gre_oip.ip_dst;
				break;
#endif
#ifdef INET6
			case SIOCGIFPSRCADDR_IN6:
				sin6->sin6_addr = sc->gre_oip6.ip6_src;
				break;
			case SIOCGIFPDSTADDR_IN6:
				sin6->sin6_addr = sc->gre_oip6.ip6_dst;
				break;
#endif
			}
		}
		GRE_RUNLOCK(sc);
		if (error != 0)
			break;
		switch (cmd) {
#ifdef INET
		case SIOCGIFPSRCADDR:
		case SIOCGIFPDSTADDR:
			error = prison_if(curthread->td_ucred,
			    (struct sockaddr *)sin);
			if (error != 0)
				memset(sin, 0, sizeof(*sin));
			break;
#endif
#ifdef INET6
		case SIOCGIFPSRCADDR_IN6:
		case SIOCGIFPDSTADDR_IN6:
			error = prison_if(curthread->td_ucred,
			    (struct sockaddr *)sin6);
			if (error == 0)
				error = sa6_recoverscope(sin6);
			if (error != 0)
				memset(sin6, 0, sizeof(*sin6));
#endif
		}
		break;
	case SIOCGTUNFIB:
		ifr->ifr_fib = sc->gre_fibnum;
		break;
	case SIOCSTUNFIB:
		if ((error = priv_check(curthread, PRIV_NET_GRE)) != 0)
			break;
		if (ifr->ifr_fib >= rt_numfibs)
			error = EINVAL;
		else
			sc->gre_fibnum = ifr->ifr_fib;
		break;
	case GRESKEY:
		if ((error = priv_check(curthread, PRIV_NET_GRE)) != 0)
			break;
		if ((error = copyin(ifr_data_get_ptr(ifr), &opt,
		    sizeof(opt))) != 0)
			break;
		if (sc->gre_key != opt) {
			GRE_WLOCK(sc);
			sc->gre_key = opt;
			gre_updatehdr(sc);
			GRE_WUNLOCK(sc);
		}
		break;
	case GREGKEY:
		error = copyout(&sc->gre_key, ifr_data_get_ptr(ifr),
		    sizeof(sc->gre_key));
		break;
	case GRESOPTS:
		if ((error = priv_check(curthread, PRIV_NET_GRE)) != 0)
			break;
		if ((error = copyin(ifr_data_get_ptr(ifr), &opt,
		    sizeof(opt))) != 0)
			break;
		if (opt & ~GRE_OPTMASK)
			error = EINVAL;
		else {
			if (sc->gre_options != opt) {
				GRE_WLOCK(sc);
				sc->gre_options = opt;
				gre_updatehdr(sc);
				GRE_WUNLOCK(sc);
			}
		}
		break;

	case GREGOPTS:
		error = copyout(&sc->gre_options, ifr_data_get_ptr(ifr),
		    sizeof(sc->gre_options));
		break;
	default:
		error = EINVAL;
		break;
	}
end:
	sx_xunlock(&gre_ioctl_sx);
	return (error);
}

static void
gre_updatehdr(struct gre_softc *sc)
{
	struct grehdr *gh = NULL;
	uint32_t *opts;
	uint16_t flags;

	GRE_WLOCK_ASSERT(sc);
	switch (sc->gre_family) {
#ifdef INET
	case AF_INET:
		sc->gre_hlen = sizeof(struct greip);
		sc->gre_oip.ip_v = IPPROTO_IPV4;
		sc->gre_oip.ip_hl = sizeof(struct ip) >> 2;
		sc->gre_oip.ip_p = IPPROTO_GRE;
		gh = &sc->gre_gihdr->gi_gre;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sc->gre_hlen = sizeof(struct greip6);
		sc->gre_oip6.ip6_vfc = IPV6_VERSION;
		sc->gre_oip6.ip6_nxt = IPPROTO_GRE;
		gh = &sc->gre_gi6hdr->gi6_gre;
		break;
#endif
	default:
		return;
	}
	flags = 0;
	opts = gh->gre_opts;
	if (sc->gre_options & GRE_ENABLE_CSUM) {
		flags |= GRE_FLAGS_CP;
		sc->gre_hlen += 2 * sizeof(uint16_t);
		*opts++ = 0;
	}
	if (sc->gre_key != 0) {
		flags |= GRE_FLAGS_KP;
		sc->gre_hlen += sizeof(uint32_t);
		*opts++ = htonl(sc->gre_key);
	}
	if (sc->gre_options & GRE_ENABLE_SEQ) {
		flags |= GRE_FLAGS_SP;
		sc->gre_hlen += sizeof(uint32_t);
		*opts++ = 0;
	} else
		sc->gre_oseq = 0;
	gh->gre_flags = htons(flags);
}

static void
gre_detach(struct gre_softc *sc)
{

	sx_assert(&gre_ioctl_sx, SA_XLOCKED);
	if (sc->gre_ecookie != NULL)
		encap_detach(sc->gre_ecookie);
	sc->gre_ecookie = NULL;
}

static int
gre_set_tunnel(struct ifnet *ifp, struct sockaddr *src,
    struct sockaddr *dst)
{
	struct gre_softc *sc, *tsc;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
#ifdef INET
	struct ip *ip;
#endif
	void *hdr;
	int error;

	sx_assert(&gre_ioctl_sx, SA_XLOCKED);
	GRE_LIST_LOCK();
	sc = ifp->if_softc;
	LIST_FOREACH(tsc, &V_gre_softc_list, gre_list) {
		if (tsc == sc || tsc->gre_family != src->sa_family)
			continue;
#ifdef INET
		if (tsc->gre_family == AF_INET &&
		    tsc->gre_oip.ip_src.s_addr ==
		    satosin(src)->sin_addr.s_addr &&
		    tsc->gre_oip.ip_dst.s_addr ==
		    satosin(dst)->sin_addr.s_addr) {
			GRE_LIST_UNLOCK();
			return (EADDRNOTAVAIL);
		}
#endif
#ifdef INET6
		if (tsc->gre_family == AF_INET6 &&
		    IN6_ARE_ADDR_EQUAL(&tsc->gre_oip6.ip6_src,
		    &satosin6(src)->sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&tsc->gre_oip6.ip6_dst,
			&satosin6(dst)->sin6_addr)) {
			GRE_LIST_UNLOCK();
			return (EADDRNOTAVAIL);
		}
#endif
	}
	GRE_LIST_UNLOCK();

	switch (src->sa_family) {
#ifdef INET
	case AF_INET:
		hdr = ip = malloc(sizeof(struct greip) +
		    3 * sizeof(uint32_t), M_GRE, M_WAITOK | M_ZERO);
		ip->ip_src = satosin(src)->sin_addr;
		ip->ip_dst = satosin(dst)->sin_addr;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		hdr = ip6 = malloc(sizeof(struct greip6) +
		    3 * sizeof(uint32_t), M_GRE, M_WAITOK | M_ZERO);
		ip6->ip6_src = satosin6(src)->sin6_addr;
		ip6->ip6_dst = satosin6(dst)->sin6_addr;
		break;
#endif
	default:
		return (EAFNOSUPPORT);
	}
	if (sc->gre_family != 0)
		gre_detach(sc);
	GRE_WLOCK(sc);
	if (sc->gre_family != 0)
		free(sc->gre_hdr, M_GRE);
	sc->gre_family = src->sa_family;
	sc->gre_hdr = hdr;
	sc->gre_oseq = 0;
	sc->gre_iseq = UINT32_MAX;
	gre_updatehdr(sc);
	GRE_WUNLOCK(sc);

	error = 0;
	switch (src->sa_family) {
#ifdef INET
	case AF_INET:
		error = in_gre_attach(sc);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = in6_gre_attach(sc);
		break;
#endif
	}
	if (error == 0) {
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
		if_link_state_change(ifp, LINK_STATE_UP);
	}
	return (error);
}

static void
gre_delete_tunnel(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;
	int family;

	GRE_WLOCK(sc);
	family = sc->gre_family;
	sc->gre_family = 0;
	GRE_WUNLOCK(sc);
	if (family != 0) {
		gre_detach(sc);
		free(sc->gre_hdr, M_GRE);
	}
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	if_link_state_change(ifp, LINK_STATE_DOWN);
}

int
gre_input(struct mbuf **mp, int *offp, int proto)
{
	struct gre_softc *sc;
	struct grehdr *gh;
	struct ifnet *ifp;
	struct mbuf *m;
	uint32_t *opts;
#ifdef notyet
	uint32_t key;
#endif
	uint16_t flags;
	int hlen, isr, af;

	m = *mp;
	sc = encap_getarg(m);
	KASSERT(sc != NULL, ("encap_getarg returned NULL"));

	ifp = GRE2IFP(sc);
	hlen = *offp + sizeof(struct grehdr) + 4 * sizeof(uint32_t);
	if (m->m_pkthdr.len < hlen)
		goto drop;
	if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (m == NULL)
			goto drop;
	}
	gh = (struct grehdr *)mtodo(m, *offp);
	flags = ntohs(gh->gre_flags);
	if (flags & ~GRE_FLAGS_MASK)
		goto drop;
	opts = gh->gre_opts;
	hlen = 2 * sizeof(uint16_t);
	if (flags & GRE_FLAGS_CP) {
		/* reserved1 field must be zero */
		if (((uint16_t *)opts)[1] != 0)
			goto drop;
		if (in_cksum_skip(m, m->m_pkthdr.len, *offp) != 0)
			goto drop;
		hlen += 2 * sizeof(uint16_t);
		opts++;
	}
	if (flags & GRE_FLAGS_KP) {
#ifdef notyet
        /* 
         * XXX: The current implementation uses the key only for outgoing
         * packets. But we can check the key value here, or even in the
         * encapcheck function.
         */
		key = ntohl(*opts);
#endif
		hlen += sizeof(uint32_t);
		opts++;
    }
#ifdef notyet
	} else
		key = 0;

	if (sc->gre_key != 0 && (key != sc->gre_key || key != 0))
		goto drop;
#endif
	if (flags & GRE_FLAGS_SP) {
#ifdef notyet
		seq = ntohl(*opts);
#endif
		hlen += sizeof(uint32_t);
	}
	switch (ntohs(gh->gre_proto)) {
	case ETHERTYPE_WCCP:
		/*
		 * For WCCP skip an additional 4 bytes if after GRE header
		 * doesn't follow an IP header.
		 */
		if (flags == 0 && (*(uint8_t *)gh->gre_opts & 0xF0) != 0x40)
			hlen += sizeof(uint32_t);
		/* FALLTHROUGH */
	case ETHERTYPE_IP:
		isr = NETISR_IP;
		af = AF_INET;
		break;
	case ETHERTYPE_IPV6:
		isr = NETISR_IPV6;
		af = AF_INET6;
		break;
	default:
		goto drop;
	}
	m_adj(m, *offp + hlen);
	m_clrprotoflags(m);
	m->m_pkthdr.rcvif = ifp;
	M_SETFIB(m, ifp->if_fib);
#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif
	BPF_MTAP2(ifp, &af, sizeof(af), m);
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	if ((ifp->if_flags & IFF_MONITOR) != 0)
		m_freem(m);
	else
		netisr_dispatch(isr, m);
	return (IPPROTO_DONE);
drop:
	if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
	m_freem(m);
	return (IPPROTO_DONE);
}

#define	MTAG_GRE	1307983903
static int
gre_check_nesting(struct ifnet *ifp, struct mbuf *m)
{
	struct m_tag *mtag;
	int count;

	count = 1;
	mtag = NULL;
	while ((mtag = m_tag_locate(m, MTAG_GRE, 0, mtag)) != NULL) {
		if (*(struct ifnet **)(mtag + 1) == ifp) {
			log(LOG_NOTICE, "%s: loop detected\n", ifp->if_xname);
			return (EIO);
		}
		count++;
	}
	if (count > V_max_gre_nesting) {
		log(LOG_NOTICE,
		    "%s: if_output recursively called too many times(%d)\n",
		    ifp->if_xname, count);
		return (EIO);
	}
	mtag = m_tag_alloc(MTAG_GRE, 0, sizeof(struct ifnet *), M_NOWAIT);
	if (mtag == NULL)
		return (ENOMEM);
	*(struct ifnet **)(mtag + 1) = ifp;
	m_tag_prepend(m, mtag);
	return (0);
}

static int
gre_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
   struct route *ro)
{
	uint32_t af;
	int error;

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error != 0)
		goto drop;
#endif
	if ((ifp->if_flags & IFF_MONITOR) != 0 ||
	    (ifp->if_flags & IFF_UP) == 0) {
		error = ENETDOWN;
		goto drop;
	}

	error = gre_check_nesting(ifp, m);
	if (error != 0)
		goto drop;

	m->m_flags &= ~(M_BCAST|M_MCAST);
	if (dst->sa_family == AF_UNSPEC)
		bcopy(dst->sa_data, &af, sizeof(af));
	else
		af = dst->sa_family;
	BPF_MTAP2(ifp, &af, sizeof(af), m);
	m->m_pkthdr.csum_data = af;	/* save af for if_transmit */
	return (ifp->if_transmit(ifp, m));
drop:
	m_freem(m);
	if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	return (error);
}

static void
gre_setseqn(struct grehdr *gh, uint32_t seq)
{
	uint32_t *opts;
	uint16_t flags;

	opts = gh->gre_opts;
	flags = ntohs(gh->gre_flags);
	KASSERT((flags & GRE_FLAGS_SP) != 0,
	    ("gre_setseqn called, but GRE_FLAGS_SP isn't set "));
	if (flags & GRE_FLAGS_CP)
		opts++;
	if (flags & GRE_FLAGS_KP)
		opts++;
	*opts = htonl(seq);
}

static int
gre_transmit(struct ifnet *ifp, struct mbuf *m)
{
	GRE_RLOCK_TRACKER;
	struct gre_softc *sc;
	struct grehdr *gh;
	uint32_t iaf, oaf, oseq;
	int error, hlen, olen, plen;
	int want_seq, want_csum;

	plen = 0;
	sc = ifp->if_softc;
	if (sc == NULL) {
		error = ENETDOWN;
		m_freem(m);
		goto drop;
	}
	GRE_RLOCK(sc);
	if (sc->gre_family == 0) {
		GRE_RUNLOCK(sc);
		error = ENETDOWN;
		m_freem(m);
		goto drop;
	}
	iaf = m->m_pkthdr.csum_data;
	oaf = sc->gre_family;
	hlen = sc->gre_hlen;
	want_seq = (sc->gre_options & GRE_ENABLE_SEQ) != 0;
	if (want_seq)
		oseq = sc->gre_oseq++; /* XXX */
	else
		oseq = 0;		/* Make compiler happy. */
	want_csum = (sc->gre_options & GRE_ENABLE_CSUM) != 0;
	M_SETFIB(m, sc->gre_fibnum);
	M_PREPEND(m, hlen, M_NOWAIT);
	if (m == NULL) {
		GRE_RUNLOCK(sc);
		error = ENOBUFS;
		goto drop;
	}
	bcopy(sc->gre_hdr, mtod(m, void *), hlen);
	GRE_RUNLOCK(sc);
	switch (oaf) {
#ifdef INET
	case AF_INET:
		olen = sizeof(struct ip);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		olen = sizeof(struct ip6_hdr);
		break;
#endif
	default:
		error = ENETDOWN;
		goto drop;
	}
	gh = (struct grehdr *)mtodo(m, olen);
	switch (iaf) {
#ifdef INET
	case AF_INET:
		gh->gre_proto = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		gh->gre_proto = htons(ETHERTYPE_IPV6);
		break;
#endif
	default:
		error = ENETDOWN;
		goto drop;
	}
	if (want_seq)
		gre_setseqn(gh, oseq);
	if (want_csum) {
		*(uint16_t *)gh->gre_opts = in_cksum_skip(m,
		    m->m_pkthdr.len, olen);
	}
	plen = m->m_pkthdr.len - hlen;
	switch (oaf) {
#ifdef INET
	case AF_INET:
		error = in_gre_output(m, iaf, hlen);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = in6_gre_output(m, iaf, hlen);
		break;
#endif
	default:
		m_freem(m);
		error = ENETDOWN;
	}
drop:
	if (error)
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	else {
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(ifp, IFCOUNTER_OBYTES, plen);
	}
	return (error);
}

static void
gre_qflush(struct ifnet *ifp __unused)
{

}

static int
gremodevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
	case MOD_UNLOAD:
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t gre_mod = {
	"if_gre",
	gremodevent,
	0
};

DECLARE_MODULE(if_gre, gre_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_gre, 1);
