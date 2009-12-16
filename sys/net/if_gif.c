/*	$FreeBSD$	*/
/*	$KAME: if_gif.c,v 1.87 2001/10/19 08:50:27 itojun Exp $	*/

/*-
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
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
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
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef	INET
#include <netinet/in_var.h>
#include <netinet/in_gif.h>
#include <netinet/ip_var.h>
#endif	/* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_gif.h>
#include <netinet6/ip6protosw.h>
#endif /* INET6 */

#include <netinet/ip_encap.h>
#include <net/ethernet.h>
#include <net/if_bridgevar.h>
#include <net/if_gif.h>

#include <security/mac/mac_framework.h>

#define GIFNAME		"gif"

/*
 * gif_mtx protects the global gif_softc_list.
 */
static struct mtx gif_mtx;
static MALLOC_DEFINE(M_GIF, "gif", "Generic Tunnel Interface");

static VNET_DEFINE(LIST_HEAD(, gif_softc), gif_softc_list);

#define	V_gif_softc_list	VNET(gif_softc_list)

#ifdef INET
VNET_DEFINE(int, ip_gif_ttl) = GIF_TTL;
#define	V_ip_gif_ttl		VNET(ip_gif_ttl)
#endif
#ifdef INET6
VNET_DEFINE(int, ip6_gif_hlim) = GIF_HLIM;
#define	V_ip6_gif_hlim		VNET(ip6_gif_hlim)
#endif

void	(*ng_gif_input_p)(struct ifnet *ifp, struct mbuf **mp, int af);
void	(*ng_gif_input_orphan_p)(struct ifnet *ifp, struct mbuf *m, int af);
void	(*ng_gif_attach_p)(struct ifnet *ifp);
void	(*ng_gif_detach_p)(struct ifnet *ifp);

static void	gif_start(struct ifnet *);
static int	gif_clone_create(struct if_clone *, int, caddr_t);
static void	gif_clone_destroy(struct ifnet *);

IFC_SIMPLE_DECLARE(gif, 0);

static int gifmodevent(module_t, int, void *);

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_GIF, gif, CTLFLAG_RW, 0,
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

SYSCTL_VNET_INT(_net_link_gif, OID_AUTO, max_nesting, CTLFLAG_RW,
    &VNET_NAME(max_gif_nesting), 0, "Max nested tunnels");

#ifdef INET6
SYSCTL_DECL(_net_inet6_ip6);
SYSCTL_VNET_INT(_net_inet6_ip6, IPV6CTL_GIF_HLIM, gifhlim, CTLFLAG_RW,
    &VNET_NAME(ip6_gif_hlim), 0, "");
#endif

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

SYSCTL_VNET_INT(_net_link_gif, OID_AUTO, parallel_tunnels, CTLFLAG_RW,
    &VNET_NAME(parallel_tunnels), 0, "Allow parallel tunnels?");

/* copy from src/sys/net/if_ethersubr.c */
static const u_char etherbroadcastaddr[ETHER_ADDR_LEN] =
			{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#ifndef ETHER_IS_BROADCAST
#define ETHER_IS_BROADCAST(addr) \
	(bcmp(etherbroadcastaddr, (addr), ETHER_ADDR_LEN) == 0)
#endif

static int
gif_clone_create(ifc, unit, params)
	struct if_clone *ifc;
	int unit;
	caddr_t params;
{
	struct gif_softc *sc;

	sc = malloc(sizeof(struct gif_softc), M_GIF, M_WAITOK | M_ZERO);
	sc->gif_fibnum = curthread->td_proc->p_fibnum;
	GIF2IFP(sc) = if_alloc(IFT_GIF);
	if (GIF2IFP(sc) == NULL) {
		free(sc, M_GIF);
		return (ENOSPC);
	}

	GIF_LOCK_INIT(sc);

	GIF2IFP(sc)->if_softc = sc;
	if_initname(GIF2IFP(sc), ifc->ifc_name, unit);

	sc->encap_cookie4 = sc->encap_cookie6 = NULL;
	sc->gif_options = GIF_ACCEPT_REVETHIP;

	GIF2IFP(sc)->if_addrlen = 0;
	GIF2IFP(sc)->if_mtu    = GIF_MTU;
	GIF2IFP(sc)->if_flags  = IFF_POINTOPOINT | IFF_MULTICAST;
#if 0
	/* turn off ingress filter */
	GIF2IFP(sc)->if_flags  |= IFF_LINK2;
#endif
	GIF2IFP(sc)->if_ioctl  = gif_ioctl;
	GIF2IFP(sc)->if_start  = gif_start;
	GIF2IFP(sc)->if_output = gif_output;
	GIF2IFP(sc)->if_snd.ifq_maxlen = IFQ_MAXLEN;
	if_attach(GIF2IFP(sc));
	bpfattach(GIF2IFP(sc), DLT_NULL, sizeof(u_int32_t));
	if (ng_gif_attach_p != NULL)
		(*ng_gif_attach_p)(GIF2IFP(sc));

	mtx_lock(&gif_mtx);
	LIST_INSERT_HEAD(&V_gif_softc_list, sc, gif_list);
	mtx_unlock(&gif_mtx);

	return (0);
}

static void
gif_clone_destroy(ifp)
	struct ifnet *ifp;
{
#if defined(INET) || defined(INET6)
	int err;
#endif
	struct gif_softc *sc = ifp->if_softc;

	mtx_lock(&gif_mtx);
	LIST_REMOVE(sc, gif_list);
	mtx_unlock(&gif_mtx);

	gif_delete_tunnel(ifp);
#ifdef INET6
	if (sc->encap_cookie6 != NULL) {
		err = encap_detach(sc->encap_cookie6);
		KASSERT(err == 0, ("Unexpected error detaching encap_cookie6"));
	}
#endif
#ifdef INET
	if (sc->encap_cookie4 != NULL) {
		err = encap_detach(sc->encap_cookie4);
		KASSERT(err == 0, ("Unexpected error detaching encap_cookie4"));
	}
#endif

	if (ng_gif_detach_p != NULL)
		(*ng_gif_detach_p)(ifp);
	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);

	GIF_LOCK_DESTROY(sc);

	free(sc, M_GIF);
}

static void
vnet_gif_init(const void *unused __unused)
{

	LIST_INIT(&V_gif_softc_list);
}
VNET_SYSINIT(vnet_gif_init, SI_SUB_PSEUDO, SI_ORDER_MIDDLE, vnet_gif_init,
    NULL);

static int
gifmodevent(mod, type, data)
	module_t mod;
	int type;
	void *data;
{

	switch (type) {
	case MOD_LOAD:
		mtx_init(&gif_mtx, "gif_mtx", NULL, MTX_DEF);
		if_clone_attach(&gif_cloner);
		break;

	case MOD_UNLOAD:
		if_clone_detach(&gif_cloner);
		mtx_destroy(&gif_mtx);
		break;
	default:
		return EOPNOTSUPP;
	}
	return 0;
}

static moduledata_t gif_mod = {
	"if_gif",
	gifmodevent,
	0
};

DECLARE_MODULE(if_gif, gif_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_gif, 1);

int
gif_encapcheck(m, off, proto, arg)
	const struct mbuf *m;
	int off;
	int proto;
	void *arg;
{
	struct ip ip;
	struct gif_softc *sc;

	sc = (struct gif_softc *)arg;
	if (sc == NULL)
		return 0;

	if ((GIF2IFP(sc)->if_flags & IFF_UP) == 0)
		return 0;

	/* no physical address */
	if (!sc->gif_psrc || !sc->gif_pdst)
		return 0;

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
		break;
#endif
#ifdef INET6
	case IPPROTO_IPV6:
		break;
#endif
	case IPPROTO_ETHERIP:
		break;

	default:
		return 0;
	}

	/* Bail on short packets */
	if (m->m_pkthdr.len < sizeof(ip))
		return 0;

	m_copydata(m, 0, sizeof(ip), (caddr_t)&ip);

	switch (ip.ip_v) {
#ifdef INET
	case 4:
		if (sc->gif_psrc->sa_family != AF_INET ||
		    sc->gif_pdst->sa_family != AF_INET)
			return 0;
		return gif_encapcheck4(m, off, proto, arg);
#endif
#ifdef INET6
	case 6:
		if (m->m_pkthdr.len < sizeof(struct ip6_hdr))
			return 0;
		if (sc->gif_psrc->sa_family != AF_INET6 ||
		    sc->gif_pdst->sa_family != AF_INET6)
			return 0;
		return gif_encapcheck6(m, off, proto, arg);
#endif
	default:
		return 0;
	}
}

static void
gif_start(struct ifnet *ifp)
{
	struct gif_softc *sc;
	struct mbuf *m;

	sc = ifp->if_softc;

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

		gif_output(ifp, m, sc->gif_pdst, NULL);

	}
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return;
}

int
gif_output(ifp, m, dst, ro)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct route *ro;
{
	struct gif_softc *sc = ifp->if_softc;
	struct m_tag *mtag;
	int error = 0;
	int gif_called;
	u_int32_t af;

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error) {
		m_freem(m);
		goto end;
	}
#endif

	/*
	 * gif may cause infinite recursion calls when misconfigured.
	 * We'll prevent this by detecting loops.
	 *
	 * High nesting level may cause stack exhaustion.
	 * We'll prevent this by introducing upper limit.
	 */
	gif_called = 1;
	mtag = m_tag_locate(m, MTAG_GIF, MTAG_GIF_CALLED, NULL);
	while (mtag != NULL) {
		if (*(struct ifnet **)(mtag + 1) == ifp) {
			log(LOG_NOTICE,
			    "gif_output: loop detected on %s\n",
			    (*(struct ifnet **)(mtag + 1))->if_xname);
			m_freem(m);
			error = EIO;	/* is there better errno? */
			goto end;
		}
		mtag = m_tag_locate(m, MTAG_GIF, MTAG_GIF_CALLED, mtag);
		gif_called++;
	}
	if (gif_called > V_max_gif_nesting) {
		log(LOG_NOTICE,
		    "gif_output: recursively called too many times(%d)\n",
		    gif_called);
		m_freem(m);
		error = EIO;	/* is there better errno? */
		goto end;
	}
	mtag = m_tag_alloc(MTAG_GIF, MTAG_GIF_CALLED, sizeof(struct ifnet *),
	    M_NOWAIT);
	if (mtag == NULL) {
		m_freem(m);
		error = ENOMEM;
		goto end;
	}
	*(struct ifnet **)(mtag + 1) = ifp;
	m_tag_prepend(m, mtag);

	m->m_flags &= ~(M_BCAST|M_MCAST);

	GIF_LOCK(sc);

	if (!(ifp->if_flags & IFF_UP) ||
	    sc->gif_psrc == NULL || sc->gif_pdst == NULL) {
		GIF_UNLOCK(sc);
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC) {
		bcopy(dst->sa_data, &af, sizeof(af));
		dst->sa_family = af;
	}

	af = dst->sa_family;
	BPF_MTAP2(ifp, &af, sizeof(af), m);
	ifp->if_opackets++;	
	ifp->if_obytes += m->m_pkthdr.len;

	/* override to IPPROTO_ETHERIP for bridged traffic */
	if (ifp->if_bridge)
		af = AF_LINK;

	M_SETFIB(m, sc->gif_fibnum);
	/* inner AF-specific encapsulation */

	/* XXX should we check if our outer source is legal? */

	/* dispatch to output logic based on outer AF */
	switch (sc->gif_psrc->sa_family) {
#ifdef INET
	case AF_INET:
		error = in_gif_output(ifp, af, m);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = in6_gif_output(ifp, af, m);
		break;
#endif
	default:
		m_freem(m);		
		error = ENETDOWN;
	}

	GIF_UNLOCK(sc);
  end:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

void
gif_input(m, af, ifp)
	struct mbuf *m;
	int af;
	struct ifnet *ifp;
{
	int isr, n;
	struct gif_softc *sc;
	struct etherip_header *eip;
	struct ether_header *eh;
	struct ifnet *oldifp;

	if (ifp == NULL) {
		/* just in case */
		m_freem(m);
		return;
	}
	sc = ifp->if_softc;
	m->m_pkthdr.rcvif = ifp;

#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif

	if (bpf_peers_present(ifp->if_bpf)) {
		u_int32_t af1 = af;
		bpf_mtap2(ifp->if_bpf, &af1, sizeof(af1), m);
	}

	if (ng_gif_input_p != NULL) {
		(*ng_gif_input_p)(ifp, &m, af);
		if (m == NULL)
			return;
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
		if (n > m->m_len) {
			m = m_pullup(m, n);
			if (m == NULL) {
				ifp->if_ierrors++;
				return;
			}
		}

		eip = mtod(m, struct etherip_header *);
		/* 
		 * GIF_ACCEPT_REVETHIP (enabled by default) intentionally
		 * accepts an EtherIP packet with revered version field in
		 * the header.  This is a knob for backward compatibility
		 * with FreeBSD 7.2R or prior.
		 */
		if (sc->gif_options & GIF_ACCEPT_REVETHIP) {
			if (eip->eip_resvl != ETHERIP_VERSION
			    && eip->eip_ver != ETHERIP_VERSION) {
				/* discard unknown versions */
				m_freem(m);
				return;
			}
		} else {
			if (eip->eip_ver != ETHERIP_VERSION) {
				/* discard unknown versions */
				m_freem(m);
				return;
			}
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
				ifp->if_imcasts++;
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

	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	netisr_dispatch(isr, m);
}

/* XXX how should we handle IPv6 scope on SIOC[GS]IFPHYADDR? */
int
gif_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct gif_softc *sc  = ifp->if_softc;
	struct ifreq     *ifr = (struct ifreq*)data;
	int error = 0, size;
	u_int	options;
	struct sockaddr *dst, *src;
#ifdef	SIOCSIFMTU /* xxx */
	u_long mtu;
#endif

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		break;
		
	case SIOCSIFDSTADDR:
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

#ifdef	SIOCSIFMTU /* xxx */
	case SIOCGIFMTU:
		break;

	case SIOCSIFMTU:
		mtu = ifr->ifr_mtu;
		if (mtu < GIF_MTU_MIN || mtu > GIF_MTU_MAX)
			return (EINVAL);
		ifp->if_mtu = mtu;
		break;
#endif /* SIOCSIFMTU */

#ifdef INET
	case SIOCSIFPHYADDR:
#endif
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
#endif /* INET6 */
	case SIOCSLIFPHYADDR:
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
		case SIOCSLIFPHYADDR:
			src = (struct sockaddr *)
				&(((struct if_laddrreq *)data)->addr);
			dst = (struct sockaddr *)
				&(((struct if_laddrreq *)data)->dstaddr);
			break;
		default:
			return EINVAL;
		}

		/* sa_family must be equal */
		if (src->sa_family != dst->sa_family)
			return EINVAL;

		/* validate sa_len */
		switch (src->sa_family) {
#ifdef INET
		case AF_INET:
			if (src->sa_len != sizeof(struct sockaddr_in))
				return EINVAL;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (src->sa_len != sizeof(struct sockaddr_in6))
				return EINVAL;
			break;
#endif
		default:
			return EAFNOSUPPORT;
		}
		switch (dst->sa_family) {
#ifdef INET
		case AF_INET:
			if (dst->sa_len != sizeof(struct sockaddr_in))
				return EINVAL;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (dst->sa_len != sizeof(struct sockaddr_in6))
				return EINVAL;
			break;
#endif
		default:
			return EAFNOSUPPORT;
		}

		/* check sa_family looks sane for the cmd */
		switch (cmd) {
		case SIOCSIFPHYADDR:
			if (src->sa_family == AF_INET)
				break;
			return EAFNOSUPPORT;
#ifdef INET6
		case SIOCSIFPHYADDR_IN6:
			if (src->sa_family == AF_INET6)
				break;
			return EAFNOSUPPORT;
#endif /* INET6 */
		case SIOCSLIFPHYADDR:
			/* checks done in the above */
			break;
		}

		error = gif_set_tunnel(GIF2IFP(sc), src, dst);
		break;

#ifdef SIOCDIFPHYADDR
	case SIOCDIFPHYADDR:
		gif_delete_tunnel(GIF2IFP(sc));
		break;
#endif
			
	case SIOCGIFPSRCADDR:
#ifdef INET6
	case SIOCGIFPSRCADDR_IN6:
#endif /* INET6 */
		if (sc->gif_psrc == NULL) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		src = sc->gif_psrc;
		switch (cmd) {
#ifdef INET
		case SIOCGIFPSRCADDR:
			dst = &ifr->ifr_addr;
			size = sizeof(ifr->ifr_addr);
			break;
#endif /* INET */
#ifdef INET6
		case SIOCGIFPSRCADDR_IN6:
			dst = (struct sockaddr *)
				&(((struct in6_ifreq *)data)->ifr_addr);
			size = sizeof(((struct in6_ifreq *)data)->ifr_addr);
			break;
#endif /* INET6 */
		default:
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if (src->sa_len > size)
			return EINVAL;
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);
#ifdef INET6
		if (dst->sa_family == AF_INET6) {
			error = sa6_recoverscope((struct sockaddr_in6 *)dst);
			if (error != 0)
				return (error);
		}
#endif
		break;
			
	case SIOCGIFPDSTADDR:
#ifdef INET6
	case SIOCGIFPDSTADDR_IN6:
#endif /* INET6 */
		if (sc->gif_pdst == NULL) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		src = sc->gif_pdst;
		switch (cmd) {
#ifdef INET
		case SIOCGIFPDSTADDR:
			dst = &ifr->ifr_addr;
			size = sizeof(ifr->ifr_addr);
			break;
#endif /* INET */
#ifdef INET6
		case SIOCGIFPDSTADDR_IN6:
			dst = (struct sockaddr *)
				&(((struct in6_ifreq *)data)->ifr_addr);
			size = sizeof(((struct in6_ifreq *)data)->ifr_addr);
			break;
#endif /* INET6 */
		default:
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if (src->sa_len > size)
			return EINVAL;
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);
#ifdef INET6
		if (dst->sa_family == AF_INET6) {
			error = sa6_recoverscope((struct sockaddr_in6 *)dst);
			if (error != 0)
				return (error);
		}
#endif
		break;

	case SIOCGLIFPHYADDR:
		if (sc->gif_psrc == NULL || sc->gif_pdst == NULL) {
			error = EADDRNOTAVAIL;
			goto bad;
		}

		/* copy src */
		src = sc->gif_psrc;
		dst = (struct sockaddr *)
			&(((struct if_laddrreq *)data)->addr);
		size = sizeof(((struct if_laddrreq *)data)->addr);
		if (src->sa_len > size)
			return EINVAL;
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);

		/* copy dst */
		src = sc->gif_pdst;
		dst = (struct sockaddr *)
			&(((struct if_laddrreq *)data)->dstaddr);
		size = sizeof(((struct if_laddrreq *)data)->dstaddr);
		if (src->sa_len > size)
			return EINVAL;
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);
		break;

	case SIOCSIFFLAGS:
		/* if_ioctl() takes care of it */
		break;

	case GIFGOPTS:
		options = sc->gif_options;
		error = copyout(&options, ifr->ifr_data,
				sizeof(options));
		break;

	case GIFSOPTS:
		if ((error = priv_check(curthread, PRIV_NET_GIF)) != 0)
			break;
		error = copyin(ifr->ifr_data, &options, sizeof(options));
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
	return error;
}

/*
 * XXXRW: There's a general event-ordering issue here: the code to check
 * if a given tunnel is already present happens before we perform a
 * potentially blocking setup of the tunnel.  This code needs to be
 * re-ordered so that the check and replacement can be atomic using
 * a mutex.
 */
int
gif_set_tunnel(ifp, src, dst)
	struct ifnet *ifp;
	struct sockaddr *src;
	struct sockaddr *dst;
{
	struct gif_softc *sc = ifp->if_softc;
	struct gif_softc *sc2;
	struct sockaddr *osrc, *odst, *sa;
	int error = 0; 

	mtx_lock(&gif_mtx);
	LIST_FOREACH(sc2, &V_gif_softc_list, gif_list) {
		if (sc2 == sc)
			continue;
		if (!sc2->gif_pdst || !sc2->gif_psrc)
			continue;
		if (sc2->gif_pdst->sa_family != dst->sa_family ||
		    sc2->gif_pdst->sa_len != dst->sa_len ||
		    sc2->gif_psrc->sa_family != src->sa_family ||
		    sc2->gif_psrc->sa_len != src->sa_len)
			continue;

		/*
		 * Disallow parallel tunnels unless instructed
		 * otherwise.
		 */
		if (!V_parallel_tunnels &&
		    bcmp(sc2->gif_pdst, dst, dst->sa_len) == 0 &&
		    bcmp(sc2->gif_psrc, src, src->sa_len) == 0) {
			error = EADDRNOTAVAIL;
			mtx_unlock(&gif_mtx);
			goto bad;
		}

		/* XXX both end must be valid? (I mean, not 0.0.0.0) */
	}
	mtx_unlock(&gif_mtx);

	/* XXX we can detach from both, but be polite just in case */
	if (sc->gif_psrc)
		switch (sc->gif_psrc->sa_family) {
#ifdef INET
		case AF_INET:
			(void)in_gif_detach(sc);
			break;
#endif
#ifdef INET6
		case AF_INET6:
			(void)in6_gif_detach(sc);
			break;
#endif
		}

	osrc = sc->gif_psrc;
	sa = (struct sockaddr *)malloc(src->sa_len, M_IFADDR, M_WAITOK);
	bcopy((caddr_t)src, (caddr_t)sa, src->sa_len);
	sc->gif_psrc = sa;

	odst = sc->gif_pdst;
	sa = (struct sockaddr *)malloc(dst->sa_len, M_IFADDR, M_WAITOK);
	bcopy((caddr_t)dst, (caddr_t)sa, dst->sa_len);
	sc->gif_pdst = sa;

	switch (sc->gif_psrc->sa_family) {
#ifdef INET
	case AF_INET:
		error = in_gif_attach(sc);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		/*
		 * Check validity of the scope zone ID of the addresses, and
		 * convert it into the kernel internal form if necessary.
		 */
		error = sa6_embedscope((struct sockaddr_in6 *)sc->gif_psrc, 0);
		if (error != 0)
			break;
		error = sa6_embedscope((struct sockaddr_in6 *)sc->gif_pdst, 0);
		if (error != 0)
			break;
		error = in6_gif_attach(sc);
		break;
#endif
	}
	if (error) {
		/* rollback */
		free((caddr_t)sc->gif_psrc, M_IFADDR);
		free((caddr_t)sc->gif_pdst, M_IFADDR);
		sc->gif_psrc = osrc;
		sc->gif_pdst = odst;
		goto bad;
	}

	if (osrc)
		free((caddr_t)osrc, M_IFADDR);
	if (odst)
		free((caddr_t)odst, M_IFADDR);

 bad:
	if (sc->gif_psrc && sc->gif_pdst)
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
	else
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	return error;
}

void
gif_delete_tunnel(ifp)
	struct ifnet *ifp;
{
	struct gif_softc *sc = ifp->if_softc;

	if (sc->gif_psrc) {
		free((caddr_t)sc->gif_psrc, M_IFADDR);
		sc->gif_psrc = NULL;
	}
	if (sc->gif_pdst) {
		free((caddr_t)sc->gif_pdst, M_IFADDR);
		sc->gif_pdst = NULL;
	}
	/* it is safe to detach from both */
#ifdef INET
	(void)in_gif_detach(sc);
#endif
#ifdef INET6
	(void)in6_gif_detach(sc);
#endif
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
}
