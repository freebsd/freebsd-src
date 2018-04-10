/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 *	$KAME: if_gif.c,v 1.87 2001/10/19 08:50:27 itojun Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/conf.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_ecn.h>
#ifdef	INET
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#endif	/* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_ecn.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/ip6protosw.h>
#endif /* INET6 */

#include <netinet/ip_encap.h>
#include <net/ethernet.h>
#include <net/if_bridgevar.h>
#include <net/if_gif.h>

#include <security/mac/mac_framework.h>

static const char gifname[] = "gif";

/*
 * gif_mtx protects a per-vnet gif_softc_list.
 */
static VNET_DEFINE(struct mtx, gif_mtx);
#define	V_gif_mtx		VNET(gif_mtx)
static MALLOC_DEFINE(M_GIF, "gif", "Generic Tunnel Interface");
static VNET_DEFINE(LIST_HEAD(, gif_softc), gif_softc_list);
#define	V_gif_softc_list	VNET(gif_softc_list)
static struct sx gif_ioctl_sx;
SX_SYSINIT(gif_ioctl_sx, &gif_ioctl_sx, "gif_ioctl");

#define	GIF_LIST_LOCK_INIT(x)		mtx_init(&V_gif_mtx, "gif_mtx", \
					    NULL, MTX_DEF)
#define	GIF_LIST_LOCK_DESTROY(x)	mtx_destroy(&V_gif_mtx)
#define	GIF_LIST_LOCK(x)		mtx_lock(&V_gif_mtx)
#define	GIF_LIST_UNLOCK(x)		mtx_unlock(&V_gif_mtx)

void	(*ng_gif_input_p)(struct ifnet *ifp, struct mbuf **mp, int af);
void	(*ng_gif_input_orphan_p)(struct ifnet *ifp, struct mbuf *m, int af);
void	(*ng_gif_attach_p)(struct ifnet *ifp);
void	(*ng_gif_detach_p)(struct ifnet *ifp);

static int	gif_check_nesting(struct ifnet *, struct mbuf *);
static int	gif_set_tunnel(struct ifnet *, struct sockaddr *,
    struct sockaddr *);
static void	gif_delete_tunnel(struct ifnet *);
static int	gif_ioctl(struct ifnet *, u_long, caddr_t);
static int	gif_transmit(struct ifnet *, struct mbuf *);
static void	gif_qflush(struct ifnet *);
static int	gif_clone_create(struct if_clone *, int, caddr_t);
static void	gif_clone_destroy(struct ifnet *);
static VNET_DEFINE(struct if_clone *, gif_cloner);
#define	V_gif_cloner	VNET(gif_cloner)

static int gifmodevent(module_t, int, void *);

SYSCTL_DECL(_net_link);
static SYSCTL_NODE(_net_link, IFT_GIF, gif, CTLFLAG_RW, 0,
    "Generic Tunnel Interface");
#ifndef MAX_GIF_NEST
/*
 * This macro controls the default upper limitation on nesting of gif tunnels.
 * Since, setting a large value to this macro with a careless configuration
 * may introduce system crash, we don't allow any nestings by default.
 * If you need to configure nested gif tunnels, you can define this macro
 * in your kernel configuration file.  However, if you do so, please be
 * careful to configure the tunnels so that it won't make a loop.
 */
#define MAX_GIF_NEST 1
#endif
static VNET_DEFINE(int, max_gif_nesting) = MAX_GIF_NEST;
#define	V_max_gif_nesting	VNET(max_gif_nesting)
SYSCTL_INT(_net_link_gif, OID_AUTO, max_nesting, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(max_gif_nesting), 0, "Max nested tunnels");

/*
 * By default, we disallow creation of multiple tunnels between the same
 * pair of addresses.  Some applications require this functionality so
 * we allow control over this check here.
 */
#ifdef XBONEHACK
static VNET_DEFINE(int, parallel_tunnels) = 1;
#else
static VNET_DEFINE(int, parallel_tunnels) = 0;
#endif
#define	V_parallel_tunnels	VNET(parallel_tunnels)
SYSCTL_INT(_net_link_gif, OID_AUTO, parallel_tunnels,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(parallel_tunnels), 0,
    "Allow parallel tunnels?");

static int
gif_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct gif_softc *sc;

	sc = malloc(sizeof(struct gif_softc), M_GIF, M_WAITOK | M_ZERO);
	sc->gif_fibnum = curthread->td_proc->p_fibnum;
	GIF2IFP(sc) = if_alloc(IFT_GIF);
	GIF_LOCK_INIT(sc);
	GIF2IFP(sc)->if_softc = sc;
	if_initname(GIF2IFP(sc), gifname, unit);

	GIF2IFP(sc)->if_addrlen = 0;
	GIF2IFP(sc)->if_mtu    = GIF_MTU;
	GIF2IFP(sc)->if_flags  = IFF_POINTOPOINT | IFF_MULTICAST;
#if 0
	/* turn off ingress filter */
	GIF2IFP(sc)->if_flags  |= IFF_LINK2;
#endif
	GIF2IFP(sc)->if_ioctl  = gif_ioctl;
	GIF2IFP(sc)->if_transmit  = gif_transmit;
	GIF2IFP(sc)->if_qflush  = gif_qflush;
	GIF2IFP(sc)->if_output = gif_output;
	GIF2IFP(sc)->if_capabilities |= IFCAP_LINKSTATE;
	GIF2IFP(sc)->if_capenable |= IFCAP_LINKSTATE;
	if_attach(GIF2IFP(sc));
	bpfattach(GIF2IFP(sc), DLT_NULL, sizeof(u_int32_t));
	if (ng_gif_attach_p != NULL)
		(*ng_gif_attach_p)(GIF2IFP(sc));

	GIF_LIST_LOCK();
	LIST_INSERT_HEAD(&V_gif_softc_list, sc, gif_list);
	GIF_LIST_UNLOCK();
	return (0);
}

static void
gif_clone_destroy(struct ifnet *ifp)
{
	struct gif_softc *sc;

	sx_xlock(&gif_ioctl_sx);
	sc = ifp->if_softc;
	gif_delete_tunnel(ifp);
	GIF_LIST_LOCK();
	LIST_REMOVE(sc, gif_list);
	GIF_LIST_UNLOCK();
	if (ng_gif_detach_p != NULL)
		(*ng_gif_detach_p)(ifp);
	bpfdetach(ifp);
	if_detach(ifp);
	ifp->if_softc = NULL;
	sx_xunlock(&gif_ioctl_sx);

	if_free(ifp);
	GIF_LOCK_DESTROY(sc);
	free(sc, M_GIF);
}

static void
vnet_gif_init(const void *unused __unused)
{

	LIST_INIT(&V_gif_softc_list);
	GIF_LIST_LOCK_INIT();
	V_gif_cloner = if_clone_simple(gifname, gif_clone_create,
	    gif_clone_destroy, 0);
}
VNET_SYSINIT(vnet_gif_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_gif_init, NULL);

static void
vnet_gif_uninit(const void *unused __unused)
{

	if_clone_detach(V_gif_cloner);
	GIF_LIST_LOCK_DESTROY();
}
VNET_SYSUNINIT(vnet_gif_uninit, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_gif_uninit, NULL);

static int
gifmodevent(module_t mod, int type, void *data)
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

static moduledata_t gif_mod = {
	"if_gif",
	gifmodevent,
	0
};

DECLARE_MODULE(if_gif, gif_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_gif, 1);

int
gif_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	GIF_RLOCK_TRACKER;
	const struct ip *ip;
	struct gif_softc *sc;
	int ret;

	sc = (struct gif_softc *)arg;
	if (sc == NULL || (GIF2IFP(sc)->if_flags & IFF_UP) == 0)
		return (0);

	ret = 0;
	GIF_RLOCK(sc);

	/* no physical address */
	if (sc->gif_family == 0)
		goto done;

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
#endif
#ifdef INET6
	case IPPROTO_IPV6:
#endif
	case IPPROTO_ETHERIP:
		break;
	default:
		goto done;
	}

	/* Bail on short packets */
	M_ASSERTPKTHDR(m);
	if (m->m_pkthdr.len < sizeof(struct ip))
		goto done;

	ip = mtod(m, const struct ip *);
	switch (ip->ip_v) {
#ifdef INET
	case 4:
		if (sc->gif_family != AF_INET)
			goto done;
		ret = in_gif_encapcheck(m, off, proto, arg);
		break;
#endif
#ifdef INET6
	case 6:
		if (m->m_pkthdr.len < sizeof(struct ip6_hdr))
			goto done;
		if (sc->gif_family != AF_INET6)
			goto done;
		ret = in6_gif_encapcheck(m, off, proto, arg);
		break;
#endif
	}
done:
	GIF_RUNLOCK(sc);
	return (ret);
}

static int
gif_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct gif_softc *sc;
	struct etherip_header *eth;
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
	uint32_t t;
#endif
	uint32_t af;
	uint8_t proto, ecn;
	int error;

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error) {
		m_freem(m);
		goto err;
	}
#endif
	error = ENETDOWN;
	sc = ifp->if_softc;
	if ((ifp->if_flags & IFF_MONITOR) != 0 ||
	    (ifp->if_flags & IFF_UP) == 0 ||
	    sc->gif_family == 0 ||
	    (error = gif_check_nesting(ifp, m)) != 0) {
		m_freem(m);
		goto err;
	}
	/* Now pull back the af that we stashed in the csum_data. */
	if (ifp->if_bridge)
		af = AF_LINK;
	else
		af = m->m_pkthdr.csum_data;
	m->m_flags &= ~(M_BCAST|M_MCAST);
	M_SETFIB(m, sc->gif_fibnum);
	BPF_MTAP2(ifp, &af, sizeof(af), m);
	if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_OBYTES, m->m_pkthdr.len);
	/* inner AF-specific encapsulation */
	ecn = 0;
	switch (af) {
#ifdef INET
	case AF_INET:
		proto = IPPROTO_IPV4;
		if (m->m_len < sizeof(struct ip))
			m = m_pullup(m, sizeof(struct ip));
		if (m == NULL) {
			error = ENOBUFS;
			goto err;
		}
		ip = mtod(m, struct ip *);
		ip_ecn_ingress((ifp->if_flags & IFF_LINK1) ? ECN_ALLOWED:
		    ECN_NOCARE, &ecn, &ip->ip_tos);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		proto = IPPROTO_IPV6;
		if (m->m_len < sizeof(struct ip6_hdr))
			m = m_pullup(m, sizeof(struct ip6_hdr));
		if (m == NULL) {
			error = ENOBUFS;
			goto err;
		}
		t = 0;
		ip6 = mtod(m, struct ip6_hdr *);
		ip6_ecn_ingress((ifp->if_flags & IFF_LINK1) ? ECN_ALLOWED:
		    ECN_NOCARE, &t, &ip6->ip6_flow);
		ecn = (ntohl(t) >> 20) & 0xff;
		break;
#endif
	case AF_LINK:
		proto = IPPROTO_ETHERIP;
		M_PREPEND(m, sizeof(struct etherip_header), M_NOWAIT);
		if (m == NULL) {
			error = ENOBUFS;
			goto err;
		}
		eth = mtod(m, struct etherip_header *);
		eth->eip_resvh = 0;
		eth->eip_ver = ETHERIP_VERSION;
		eth->eip_resvl = 0;
		break;
	default:
		error = EAFNOSUPPORT;
		m_freem(m);
		goto err;
	}
	/* XXX should we check if our outer source is legal? */
	/* dispatch to output logic based on outer AF */
	switch (sc->gif_family) {
#ifdef INET
	case AF_INET:
		error = in_gif_output(ifp, m, proto, ecn);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = in6_gif_output(ifp, m, proto, ecn);
		break;
#endif
	default:
		m_freem(m);
	}
err:
	if (error)
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	return (error);
}

static void
gif_qflush(struct ifnet *ifp __unused)
{

}

#define	MTAG_GIF	1080679712
static int
gif_check_nesting(struct ifnet *ifp, struct mbuf *m)
{
	struct m_tag *mtag;
	int count;

	/*
	 * gif may cause infinite recursion calls when misconfigured.
	 * We'll prevent this by detecting loops.
	 *
	 * High nesting level may cause stack exhaustion.
	 * We'll prevent this by introducing upper limit.
	 */
	count = 1;
	mtag = NULL;
	while ((mtag = m_tag_locate(m, MTAG_GIF, 0, mtag)) != NULL) {
		if (*(struct ifnet **)(mtag + 1) == ifp) {
			log(LOG_NOTICE, "%s: loop detected\n", if_name(ifp));
			return (EIO);
		}
		count++;
	}
	if (count > V_max_gif_nesting) {
		log(LOG_NOTICE,
		    "%s: if_output recursively called too many times(%d)\n",
		    if_name(ifp), count);
		return (EIO);
	}
	mtag = m_tag_alloc(MTAG_GIF, 0, sizeof(struct ifnet *), M_NOWAIT);
	if (mtag == NULL)
		return (ENOMEM);
	*(struct ifnet **)(mtag + 1) = ifp;
	m_tag_prepend(m, mtag);
	return (0);
}

int
gif_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	struct route *ro)
{
	uint32_t af;

	if (dst->sa_family == AF_UNSPEC)
		bcopy(dst->sa_data, &af, sizeof(af));
	else
		af = dst->sa_family;
	/*
	 * Now save the af in the inbound pkt csum data, this is a cheat since
	 * we are using the inbound csum_data field to carry the af over to
	 * the gif_transmit() routine, avoiding using yet another mtag.
	 */
	m->m_pkthdr.csum_data = af;
	return (ifp->if_transmit(ifp, m));
}

void
gif_input(struct mbuf *m, struct ifnet *ifp, int proto, uint8_t ecn)
{
	struct etherip_header *eip;
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
	uint32_t t;
#endif
	struct ether_header *eh;
	struct ifnet *oldifp;
	int isr, n, af;

	if (ifp == NULL) {
		/* just in case */
		m_freem(m);
		return;
	}
	m->m_pkthdr.rcvif = ifp;
	m_clrprotoflags(m);
	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
		af = AF_INET;
		if (m->m_len < sizeof(struct ip))
			m = m_pullup(m, sizeof(struct ip));
		if (m == NULL)
			goto drop;
		ip = mtod(m, struct ip *);
		if (ip_ecn_egress((ifp->if_flags & IFF_LINK1) ? ECN_ALLOWED:
		    ECN_NOCARE, &ecn, &ip->ip_tos) == 0) {
			m_freem(m);
			goto drop;
		}
		break;
#endif
#ifdef INET6
	case IPPROTO_IPV6:
		af = AF_INET6;
		if (m->m_len < sizeof(struct ip6_hdr))
			m = m_pullup(m, sizeof(struct ip6_hdr));
		if (m == NULL)
			goto drop;
		t = htonl((uint32_t)ecn << 20);
		ip6 = mtod(m, struct ip6_hdr *);
		if (ip6_ecn_egress((ifp->if_flags & IFF_LINK1) ? ECN_ALLOWED:
		    ECN_NOCARE, &t, &ip6->ip6_flow) == 0) {
			m_freem(m);
			goto drop;
		}
		break;
#endif
	case IPPROTO_ETHERIP:
		af = AF_LINK;
		break;
	default:
		m_freem(m);
		goto drop;
	}

#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif

	if (bpf_peers_present(ifp->if_bpf)) {
		uint32_t af1 = af;
		bpf_mtap2(ifp->if_bpf, &af1, sizeof(af1), m);
	}

	if ((ifp->if_flags & IFF_MONITOR) != 0) {
		if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
		if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
		m_freem(m);
		return;
	}

	if (ng_gif_input_p != NULL) {
		(*ng_gif_input_p)(ifp, &m, af);
		if (m == NULL)
			goto drop;
	}

	/*
	 * Put the packet to the network layer input queue according to the
	 * specified address family.
	 * Note: older versions of gif_input directly called network layer
	 * input functions, e.g. ip6_input, here.  We changed the policy to
	 * prevent too many recursive calls of such input functions, which
	 * might cause kernel panic.  But the change may introduce another
	 * problem; if the input queue is full, packets are discarded.
	 * The kernel stack overflow really happened, and we believed
	 * queue-full rarely occurs, so we changed the policy.
	 */
	switch (af) {
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
	case AF_LINK:
		n = sizeof(struct etherip_header) + sizeof(struct ether_header);
		if (n > m->m_len)
			m = m_pullup(m, n);
		if (m == NULL)
			goto drop;
		eip = mtod(m, struct etherip_header *);
		if (eip->eip_ver != ETHERIP_VERSION) {
			/* discard unknown versions */
			m_freem(m);
			goto drop;
		}
		m_adj(m, sizeof(struct etherip_header));

		m->m_flags &= ~(M_BCAST|M_MCAST);
		m->m_pkthdr.rcvif = ifp;

		if (ifp->if_bridge) {
			oldifp = ifp;
			eh = mtod(m, struct ether_header *);
			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				if (ETHER_IS_BROADCAST(eh->ether_dhost))
					m->m_flags |= M_BCAST;
				else
					m->m_flags |= M_MCAST;
				if_inc_counter(ifp, IFCOUNTER_IMCASTS, 1);
			}
			BRIDGE_INPUT(ifp, m);

			if (m != NULL && ifp != oldifp) {
				/*
				 * The bridge gave us back itself or one of the
				 * members for which the frame is addressed.
				 */
				ether_demux(ifp, m);
				return;
			}
		}
		if (m != NULL)
			m_freem(m);
		return;

	default:
		if (ng_gif_input_orphan_p != NULL)
			(*ng_gif_input_orphan_p)(ifp, m, af);
		else
			m_freem(m);
		return;
	}

	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	M_SETFIB(m, ifp->if_fib);
	netisr_dispatch(isr, m);
	return;
drop:
	if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
}

/* XXX how should we handle IPv6 scope on SIOC[GS]IFPHYADDR? */
int
gif_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	GIF_RLOCK_TRACKER;
	struct ifreq *ifr = (struct ifreq*)data;
	struct sockaddr *dst, *src;
	struct gif_softc *sc;
#ifdef INET
	struct sockaddr_in *sin = NULL;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6 = NULL;
#endif
	u_int options;
	int error;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCGIFMTU:
	case SIOCSIFFLAGS:
		return (0);
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < GIF_MTU_MIN ||
		    ifr->ifr_mtu > GIF_MTU_MAX)
			return (EINVAL);
		else
			ifp->if_mtu = ifr->ifr_mtu;
		return (0);
	}
	sx_xlock(&gif_ioctl_sx);
	sc = ifp->if_softc;
	if (sc == NULL) {
		error = ENXIO;
		goto bad;
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
			goto bad;
		}
		/* sa_family must be equal */
		if (src->sa_family != dst->sa_family ||
		    src->sa_len != dst->sa_len)
			goto bad;

		/* validate sa_len */
		/* check sa_family looks sane for the cmd */
		switch (src->sa_family) {
#ifdef INET
		case AF_INET:
			if (src->sa_len != sizeof(struct sockaddr_in))
				goto bad;
			if (cmd != SIOCSIFPHYADDR) {
				error = EAFNOSUPPORT;
				goto bad;
			}
			if (satosin(src)->sin_addr.s_addr == INADDR_ANY ||
			    satosin(dst)->sin_addr.s_addr == INADDR_ANY) {
				error = EADDRNOTAVAIL;
				goto bad;
			}
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (src->sa_len != sizeof(struct sockaddr_in6))
				goto bad;
			if (cmd != SIOCSIFPHYADDR_IN6) {
				error = EAFNOSUPPORT;
				goto bad;
			}
			error = EADDRNOTAVAIL;
			if (IN6_IS_ADDR_UNSPECIFIED(&satosin6(src)->sin6_addr)
			    ||
			    IN6_IS_ADDR_UNSPECIFIED(&satosin6(dst)->sin6_addr))
				goto bad;
			/*
			 * Check validity of the scope zone ID of the
			 * addresses, and convert it into the kernel
			 * internal form if necessary.
			 */
			error = sa6_embedscope(satosin6(src), 0);
			if (error != 0)
				goto bad;
			error = sa6_embedscope(satosin6(dst), 0);
			if (error != 0)
				goto bad;
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			goto bad;
		}
		error = gif_set_tunnel(ifp, src, dst);
		break;
	case SIOCDIFPHYADDR:
		gif_delete_tunnel(ifp);
		break;
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
#ifdef INET6
	case SIOCGIFPSRCADDR_IN6:
	case SIOCGIFPDSTADDR_IN6:
#endif
		if (sc->gif_family == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		GIF_RLOCK(sc);
		switch (cmd) {
#ifdef INET
		case SIOCGIFPSRCADDR:
		case SIOCGIFPDSTADDR:
			if (sc->gif_family != AF_INET) {
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
			if (sc->gif_family != AF_INET6) {
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
		default:
			error = EAFNOSUPPORT;
		}
		if (error == 0) {
			switch (cmd) {
#ifdef INET
			case SIOCGIFPSRCADDR:
				sin->sin_addr = sc->gif_iphdr->ip_src;
				break;
			case SIOCGIFPDSTADDR:
				sin->sin_addr = sc->gif_iphdr->ip_dst;
				break;
#endif
#ifdef INET6
			case SIOCGIFPSRCADDR_IN6:
				sin6->sin6_addr = sc->gif_ip6hdr->ip6_src;
				break;
			case SIOCGIFPDSTADDR_IN6:
				sin6->sin6_addr = sc->gif_ip6hdr->ip6_dst;
				break;
#endif
			}
		}
		GIF_RUNLOCK(sc);
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
		ifr->ifr_fib = sc->gif_fibnum;
		break;
	case SIOCSTUNFIB:
		if ((error = priv_check(curthread, PRIV_NET_GIF)) != 0)
			break;
		if (ifr->ifr_fib >= rt_numfibs)
			error = EINVAL;
		else
			sc->gif_fibnum = ifr->ifr_fib;
		break;
	case GIFGOPTS:
		options = sc->gif_options;
		error = copyout(&options, ifr_data_get_ptr(ifr),
		    sizeof(options));
		break;
	case GIFSOPTS:
		if ((error = priv_check(curthread, PRIV_NET_GIF)) != 0)
			break;
		error = copyin(ifr_data_get_ptr(ifr), &options,
		    sizeof(options));
		if (error)
			break;
		if (options & ~GIF_OPTMASK)
			error = EINVAL;
		else
			sc->gif_options = options;
		break;
	default:
		error = EINVAL;
		break;
	}
bad:
	sx_xunlock(&gif_ioctl_sx);
	return (error);
}

static void
gif_detach(struct gif_softc *sc)
{

	sx_assert(&gif_ioctl_sx, SA_XLOCKED);
	if (sc->gif_ecookie != NULL)
		encap_detach(sc->gif_ecookie);
	sc->gif_ecookie = NULL;
}

static int
gif_attach(struct gif_softc *sc, int af)
{

	sx_assert(&gif_ioctl_sx, SA_XLOCKED);
	switch (af) {
#ifdef INET
	case AF_INET:
		return (in_gif_attach(sc));
#endif
#ifdef INET6
	case AF_INET6:
		return (in6_gif_attach(sc));
#endif
	}
	return (EAFNOSUPPORT);
}

static int
gif_set_tunnel(struct ifnet *ifp, struct sockaddr *src, struct sockaddr *dst)
{
	struct gif_softc *sc = ifp->if_softc;
	struct gif_softc *tsc;
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	void *hdr;
	int error = 0;

	if (sc == NULL)
		return (ENXIO);
	/* Disallow parallel tunnels unless instructed otherwise. */
	if (V_parallel_tunnels == 0) {
		GIF_LIST_LOCK();
		LIST_FOREACH(tsc, &V_gif_softc_list, gif_list) {
			if (tsc == sc || tsc->gif_family != src->sa_family)
				continue;
#ifdef INET
			if (tsc->gif_family == AF_INET &&
			    tsc->gif_iphdr->ip_src.s_addr ==
			    satosin(src)->sin_addr.s_addr &&
			    tsc->gif_iphdr->ip_dst.s_addr ==
			    satosin(dst)->sin_addr.s_addr) {
				error = EADDRNOTAVAIL;
				GIF_LIST_UNLOCK();
				goto bad;
			}
#endif
#ifdef INET6
			if (tsc->gif_family == AF_INET6 &&
			    IN6_ARE_ADDR_EQUAL(&tsc->gif_ip6hdr->ip6_src,
			    &satosin6(src)->sin6_addr) &&
			    IN6_ARE_ADDR_EQUAL(&tsc->gif_ip6hdr->ip6_dst,
			    &satosin6(dst)->sin6_addr)) {
				error = EADDRNOTAVAIL;
				GIF_LIST_UNLOCK();
				goto bad;
			}
#endif
		}
		GIF_LIST_UNLOCK();
	}
	switch (src->sa_family) {
#ifdef INET
	case AF_INET:
		hdr = ip = malloc(sizeof(struct ip), M_GIF,
		    M_WAITOK | M_ZERO);
		ip->ip_src.s_addr = satosin(src)->sin_addr.s_addr;
		ip->ip_dst.s_addr = satosin(dst)->sin_addr.s_addr;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		hdr = ip6 = malloc(sizeof(struct ip6_hdr), M_GIF,
		    M_WAITOK | M_ZERO);
		ip6->ip6_src = satosin6(src)->sin6_addr;
		ip6->ip6_dst = satosin6(dst)->sin6_addr;
		ip6->ip6_vfc = IPV6_VERSION;
		break;
#endif
	default:
		return (EAFNOSUPPORT);
	}

	if (sc->gif_family != src->sa_family)
		gif_detach(sc);
	if (sc->gif_family == 0 ||
	    sc->gif_family != src->sa_family)
		error = gif_attach(sc, src->sa_family);

	GIF_WLOCK(sc);
	if (sc->gif_family != 0)
		free(sc->gif_hdr, M_GIF);
	sc->gif_family = src->sa_family;
	sc->gif_hdr = hdr;
	GIF_WUNLOCK(sc);
#if defined(INET) || defined(INET6)
bad:
#endif
	if (error == 0 && sc->gif_family != 0) {
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
		if_link_state_change(ifp, LINK_STATE_UP);
	} else {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		if_link_state_change(ifp, LINK_STATE_DOWN);
	}
	return (error);
}

static void
gif_delete_tunnel(struct ifnet *ifp)
{
	struct gif_softc *sc = ifp->if_softc;
	int family;

	if (sc == NULL)
		return;

	GIF_WLOCK(sc);
	family = sc->gif_family;
	sc->gif_family = 0;
	GIF_WUNLOCK(sc);
	if (family != 0) {
		gif_detach(sc);
		free(sc->gif_hdr, M_GIF);
	}
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	if_link_state_change(ifp, LINK_STATE_DOWN);
}
