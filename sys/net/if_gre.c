/*	$NetBSD: if_gre.c,v 1.49 2003/12/11 00:22:29 itojun Exp $ */
/*	 $FreeBSD$ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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
 */

/*
 * Encapsulate L3 protocols into IP
 * See RFC 2784 (successor of RFC 1701 and 1702) for more details.
 * If_gre is compatible with Cisco GRE tunnels, so you can
 * have a NetBSD box as the other end of a tunnel interface of a Cisco
 * router. See gre(4) for more details.
 * Also supported:  IP in IP encaps (proto 55) as of RFC 2004
 */

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/vnet.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_gre.h>
#include <netinet/ip_var.h>
#include <netinet/ip_encap.h>
#else
#error "Huh? if_gre without inet?"
#endif

#include <net/bpf.h>

#include <net/if_gre.h>

/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU	1476

#define	MTAG_COOKIE_GRE		1307983903
#define	MTAG_GRE_NESTING	1
struct mtag_gre_nesting {
	uint16_t	count;
	uint16_t	max;
	struct ifnet	*ifp[];
};

/*
 * gre_mtx protects all global variables in if_gre.c.
 * XXX: gre_softc data not protected yet.
 */
struct mtx gre_mtx;
static const char grename[] = "gre";
static MALLOC_DEFINE(M_GRE, grename, "Generic Routing Encapsulation");

struct gre_softc_head gre_softc_list;

static int	gre_clone_create(struct if_clone *, int, caddr_t);
static void	gre_clone_destroy(struct ifnet *);
static struct if_clone *gre_cloner;

static int	gre_ioctl(struct ifnet *, u_long, caddr_t);
static int	gre_output(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);

static int gre_compute_route(struct gre_softc *sc);

static void	greattach(void);

#ifdef INET
extern struct domain inetdomain;
static const struct protosw in_gre_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_GRE,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		gre_input,
	.pr_output =		(pr_output_t *)rip_output,
	.pr_ctlinput =		rip_ctlinput,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};
static const struct protosw in_mobile_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_MOBILE,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		gre_mobile_input,
	.pr_output =		(pr_output_t *)rip_output,
	.pr_ctlinput =		rip_ctlinput,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};
#endif

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
static int max_gre_nesting = MAX_GRE_NEST;
SYSCTL_INT(_net_link_gre, OID_AUTO, max_nesting, CTLFLAG_RW,
    &max_gre_nesting, 0, "Max nested tunnels");

/* ARGSUSED */
static void
greattach(void)
{

	mtx_init(&gre_mtx, "gre_mtx", NULL, MTX_DEF);
	LIST_INIT(&gre_softc_list);
	gre_cloner = if_clone_simple(grename, gre_clone_create,
	    gre_clone_destroy, 0);
}

static int
gre_clone_create(ifc, unit, params)
	struct if_clone *ifc;
	int unit;
	caddr_t params;
{
	struct gre_softc *sc;

	sc = malloc(sizeof(struct gre_softc), M_GRE, M_WAITOK | M_ZERO);

	GRE2IFP(sc) = if_alloc(IFT_TUNNEL);
	if (GRE2IFP(sc) == NULL) {
		free(sc, M_GRE);
		return (ENOSPC);
	}

	GRE2IFP(sc)->if_softc = sc;
	if_initname(GRE2IFP(sc), grename, unit);

	GRE2IFP(sc)->if_snd.ifq_maxlen = ifqmaxlen;
	GRE2IFP(sc)->if_addrlen = 0;
	GRE2IFP(sc)->if_hdrlen = 24; /* IP + GRE */
	GRE2IFP(sc)->if_mtu = GREMTU;
	GRE2IFP(sc)->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	GRE2IFP(sc)->if_output = gre_output;
	GRE2IFP(sc)->if_ioctl = gre_ioctl;
	sc->g_dst.s_addr = sc->g_src.s_addr = INADDR_ANY;
	sc->g_proto = IPPROTO_GRE;
	GRE2IFP(sc)->if_flags |= IFF_LINK0;
	sc->encap = NULL;
	sc->gre_fibnum = curthread->td_proc->p_fibnum;
	sc->wccp_ver = WCCP_V1;
	sc->key = 0;
	if_attach(GRE2IFP(sc));
	bpfattach(GRE2IFP(sc), DLT_NULL, sizeof(u_int32_t));
	mtx_lock(&gre_mtx);
	LIST_INSERT_HEAD(&gre_softc_list, sc, sc_list);
	mtx_unlock(&gre_mtx);
	return (0);
}

static void
gre_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct gre_softc *sc = ifp->if_softc;

	mtx_lock(&gre_mtx);
	LIST_REMOVE(sc, sc_list);
	mtx_unlock(&gre_mtx);

#ifdef INET
	if (sc->encap != NULL)
		encap_detach(sc->encap);
#endif
	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);
	free(sc, M_GRE);
}

/*
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->g_proto. See also RFC 1701 and RFC 2004
 */
static int
gre_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	   struct route *ro)
{
	int error = 0;
	struct gre_softc *sc = ifp->if_softc;
	struct greip *gh;
	struct ip *ip;
	struct m_tag *mtag;
	struct mtag_gre_nesting *gt;
	size_t len;
	u_short gre_ip_id = 0;
	uint8_t gre_ip_tos = 0;
	u_int16_t etype = 0;
	struct mobile_h mob_h;
	u_int32_t af;
	int extra = 0, max;

	/*
	 * gre may cause infinite recursion calls when misconfigured.  High
	 * nesting level may cause stack exhaustion.  We'll prevent this by
	 * detecting loops and by introducing upper limit.
	 */
	mtag = m_tag_locate(m, MTAG_COOKIE_GRE, MTAG_GRE_NESTING, NULL);
	if (mtag != NULL) {
		struct ifnet **ifp2;

		gt = (struct mtag_gre_nesting *)(mtag + 1);
		gt->count++;
		if (gt->count > min(gt->max,max_gre_nesting)) {
			printf("%s: hit maximum recursion limit %u on %s\n",
				__func__, gt->count - 1, ifp->if_xname);
			m_freem(m);
			error = EIO;	/* is there better errno? */
			goto end;
		}

		ifp2 = gt->ifp;
		for (max = gt->count - 1; max > 0; max--) {
			if (*ifp2 == ifp)
				break;
			ifp2++;
		}
		if (*ifp2 == ifp) {
			printf("%s: detected loop with nexting %u on %s\n",
				__func__, gt->count-1, ifp->if_xname);
			m_freem(m);
			error = EIO;	/* is there better errno? */
			goto end;
		}
		*ifp2 = ifp;

	} else {
		/*
		 * Given that people should NOT increase max_gre_nesting beyond
		 * their real needs, we allocate once per packet rather than
		 * allocating an mtag once per passing through gre.
		 *
		 * Note: the sysctl does not actually check for saneness, so we
		 * limit the maximum numbers of possible recursions here.
		 */
		max = imin(max_gre_nesting, 256);
		/* If someone sets the sysctl <= 0, we want at least 1. */
		max = imax(max, 1);
		len = sizeof(struct mtag_gre_nesting) +
		    max * sizeof(struct ifnet *);
		mtag = m_tag_alloc(MTAG_COOKIE_GRE, MTAG_GRE_NESTING, len,
		    M_NOWAIT);
		if (mtag == NULL) {
			m_freem(m);
			error = ENOMEM;
			goto end;
		}
		gt = (struct mtag_gre_nesting *)(mtag + 1);
		bzero(gt, len);
		gt->count = 1;
		gt->max = max;
		*gt->ifp = ifp;
		m_tag_prepend(m, mtag);
	}

	if (!((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)) ||
	    sc->g_src.s_addr == INADDR_ANY || sc->g_dst.s_addr == INADDR_ANY) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	gh = NULL;
	ip = NULL;

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC)
		bcopy(dst->sa_data, &af, sizeof(af));
	else
		af = dst->sa_family;

	if (bpf_peers_present(ifp->if_bpf))
		bpf_mtap2(ifp->if_bpf, &af, sizeof(af), m);

	if ((ifp->if_flags & IFF_MONITOR) != 0) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	m->m_flags &= ~(M_BCAST|M_MCAST);

	if (sc->g_proto == IPPROTO_MOBILE) {
		if (af == AF_INET) {
			struct mbuf *m0;
			int msiz;

			ip = mtod(m, struct ip *);

			/*
			 * RFC2004 specifies that fragmented diagrams shouldn't
			 * be encapsulated.
			 */
			if (ip->ip_off & htons(IP_MF | IP_OFFMASK)) {
				_IF_DROP(&ifp->if_snd);
				m_freem(m);
				error = EINVAL;    /* is there better errno? */
				goto end;
			}
			memset(&mob_h, 0, MOB_H_SIZ_L);
			mob_h.proto = (ip->ip_p) << 8;
			mob_h.odst = ip->ip_dst.s_addr;
			ip->ip_dst.s_addr = sc->g_dst.s_addr;

			/*
			 * If the packet comes from our host, we only change
			 * the destination address in the IP header.
			 * Else we also need to save and change the source
			 */
			if (in_hosteq(ip->ip_src, sc->g_src)) {
				msiz = MOB_H_SIZ_S;
			} else {
				mob_h.proto |= MOB_H_SBIT;
				mob_h.osrc = ip->ip_src.s_addr;
				ip->ip_src.s_addr = sc->g_src.s_addr;
				msiz = MOB_H_SIZ_L;
			}
			mob_h.proto = htons(mob_h.proto);
			mob_h.hcrc = gre_in_cksum((u_int16_t *)&mob_h, msiz);

			if ((m->m_data - msiz) < m->m_pktdat) {
				m0 = m_gethdr(M_NOWAIT, MT_DATA);
				if (m0 == NULL) {
					_IF_DROP(&ifp->if_snd);
					m_freem(m);
					error = ENOBUFS;
					goto end;
				}
				m0->m_next = m;
				m->m_data += sizeof(struct ip);
				m->m_len -= sizeof(struct ip);
				m0->m_pkthdr.len = m->m_pkthdr.len + msiz;
				m0->m_len = msiz + sizeof(struct ip);
				m0->m_data += max_linkhdr;
				memcpy(mtod(m0, caddr_t), (caddr_t)ip,
				       sizeof(struct ip));
				m = m0;
			} else {  /* we have some space left in the old one */
				m->m_data -= msiz;
				m->m_len += msiz;
				m->m_pkthdr.len += msiz;
				bcopy(ip, mtod(m, caddr_t),
					sizeof(struct ip));
			}
			ip = mtod(m, struct ip *);
			memcpy((caddr_t)(ip + 1), &mob_h, (unsigned)msiz);
			ip->ip_len = htons(ntohs(ip->ip_len) + msiz);
		} else {  /* AF_INET */
			_IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EINVAL;
			goto end;
		}
	} else if (sc->g_proto == IPPROTO_GRE) {
		switch (af) {
		case AF_INET:
			ip = mtod(m, struct ip *);
			gre_ip_tos = ip->ip_tos;
			gre_ip_id = ip->ip_id;
			if (sc->wccp_ver == WCCP_V2) {
				extra = sizeof(uint32_t);
				etype =  WCCP_PROTOCOL_TYPE;
			} else {
				etype = ETHERTYPE_IP;
			}
			break;
#ifdef INET6
		case AF_INET6:
			gre_ip_id = ip_newid();
			etype = ETHERTYPE_IPV6;
			break;
#endif
#ifdef NETATALK
		case AF_APPLETALK:
			etype = ETHERTYPE_ATALK;
			break;
#endif
		default:
			_IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EAFNOSUPPORT;
			goto end;
		}
			
		/* Reserve space for GRE header + optional GRE key */
		int hdrlen = sizeof(struct greip) + extra;
		if (sc->key)
			hdrlen += sizeof(uint32_t);
		M_PREPEND(m, hdrlen, M_NOWAIT);
	} else {
		_IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = EINVAL;
		goto end;
	}

	if (m == NULL) {	/* mbuf allocation failed */
		_IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto end;
	}

	M_SETFIB(m, sc->gre_fibnum); /* The envelope may use a different FIB */

	gh = mtod(m, struct greip *);
	if (sc->g_proto == IPPROTO_GRE) {
		uint32_t *options = gh->gi_options;

		memset((void *)gh, 0, sizeof(struct greip) + extra);
		gh->gi_ptype = htons(etype);
		gh->gi_flags = 0;

		/* Add key option */
		if (sc->key)
		{
			gh->gi_flags |= htons(GRE_KP);
			*(options++) = htonl(sc->key);
		}
	}

	gh->gi_pr = sc->g_proto;
	if (sc->g_proto != IPPROTO_MOBILE) {
		gh->gi_src = sc->g_src;
		gh->gi_dst = sc->g_dst;
		((struct ip*)gh)->ip_v = IPPROTO_IPV4;
		((struct ip*)gh)->ip_hl = (sizeof(struct ip)) >> 2;
		((struct ip*)gh)->ip_ttl = GRE_TTL;
		((struct ip*)gh)->ip_tos = gre_ip_tos;
		((struct ip*)gh)->ip_id = gre_ip_id;
		gh->gi_len = htons(m->m_pkthdr.len);
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	/*
	 * Send it off and with IP_FORWARD flag to prevent it from
	 * overwriting the ip_id again.  ip_id is already set to the
	 * ip_id of the encapsulated packet.
	 */
	error = ip_output(m, NULL, &sc->route, IP_FORWARDING,
	    (struct ip_moptions *)NULL, (struct inpcb *)NULL);
  end:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

static int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct if_laddrreq *lifr = (struct if_laddrreq *)data;
	struct in_aliasreq *aifr = (struct in_aliasreq *)data;
	struct gre_softc *sc = ifp->if_softc;
	struct sockaddr_in si;
	struct sockaddr *sa = NULL;
	int error, adj;
	struct sockaddr_in sp, sm, dp, dm;
	uint32_t key;

	error = 0;
	adj = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		break;
	case SIOCSIFDSTADDR:
		break;
	case SIOCSIFFLAGS:
		/*
		 * XXXRW: Isn't this priv_check() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_SETIFFLAGS)) != 0)
			break;
		if ((ifr->ifr_flags & IFF_LINK0) != 0)
			sc->g_proto = IPPROTO_GRE;
		else
			sc->g_proto = IPPROTO_MOBILE;
		if ((ifr->ifr_flags & IFF_LINK2) != 0)
			sc->wccp_ver = WCCP_V2;
		else
			sc->wccp_ver = WCCP_V1;
		goto recompute;
	case SIOCSIFMTU:
		/*
		 * XXXRW: Isn't this priv_check() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_SETIFMTU)) != 0)
			break;
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = GRE2IFP(sc)->if_mtu;
		break;
	case SIOCADDMULTI:
		/*
		 * XXXRW: Isn't this priv_checkr() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_ADDMULTI)) != 0)
			break;
		if (ifr == 0) {
			error = EAFNOSUPPORT;
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
	case SIOCDELMULTI:
		/*
		 * XXXRW: Isn't this priv_check() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_DELIFGROUP)) != 0)
			break;
		if (ifr == 0) {
			error = EAFNOSUPPORT;
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
	case GRESPROTO:
		/*
		 * XXXRW: Isn't this priv_check() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_GRE)) != 0)
			break;
		sc->g_proto = ifr->ifr_flags;
		switch (sc->g_proto) {
		case IPPROTO_GRE:
			ifp->if_flags |= IFF_LINK0;
			break;
		case IPPROTO_MOBILE:
			ifp->if_flags &= ~IFF_LINK0;
			break;
		default:
			error = EPROTONOSUPPORT;
			break;
		}
		goto recompute;
	case GREGPROTO:
		ifr->ifr_flags = sc->g_proto;
		break;
	case GRESADDRS:
	case GRESADDRD:
		error = priv_check(curthread, PRIV_NET_GRE);
		if (error)
			return (error);
		/*
		 * set tunnel endpoints, compute a less specific route
		 * to the remote end and mark if as up
		 */
		sa = &ifr->ifr_addr;
		if (cmd == GRESADDRS)
			sc->g_src = (satosin(sa))->sin_addr;
		if (cmd == GRESADDRD)
			sc->g_dst = (satosin(sa))->sin_addr;
	recompute:
#ifdef INET
		if (sc->encap != NULL) {
			encap_detach(sc->encap);
			sc->encap = NULL;
		}
#endif
		if ((sc->g_src.s_addr != INADDR_ANY) &&
		    (sc->g_dst.s_addr != INADDR_ANY)) {
			bzero(&sp, sizeof(sp));
			bzero(&sm, sizeof(sm));
			bzero(&dp, sizeof(dp));
			bzero(&dm, sizeof(dm));
			sp.sin_len = sm.sin_len = dp.sin_len = dm.sin_len =
			    sizeof(struct sockaddr_in);
			sp.sin_family = sm.sin_family = dp.sin_family =
			    dm.sin_family = AF_INET;
			sp.sin_addr = sc->g_src;
			dp.sin_addr = sc->g_dst;
			sm.sin_addr.s_addr = dm.sin_addr.s_addr =
			    INADDR_BROADCAST;
#ifdef INET
			sc->encap = encap_attach(AF_INET, sc->g_proto,
			    sintosa(&sp), sintosa(&sm), sintosa(&dp),
			    sintosa(&dm), (sc->g_proto == IPPROTO_GRE) ?
				&in_gre_protosw : &in_mobile_protosw, sc);
			if (sc->encap == NULL)
				printf("%s: unable to attach encap\n",
				    if_name(GRE2IFP(sc)));
#endif
			if (sc->route.ro_rt != 0) /* free old route */
				RTFREE(sc->route.ro_rt);
			if (gre_compute_route(sc) == 0)
				ifp->if_drv_flags |= IFF_DRV_RUNNING;
			else
				ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		}
		break;
	case GREGADDRS:
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		sa = sintosa(&si);
		error = prison_if(curthread->td_ucred, sa);
		if (error != 0)
			break;
		ifr->ifr_addr = *sa;
		break;
	case GREGADDRD:
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		sa = sintosa(&si);
		error = prison_if(curthread->td_ucred, sa);
		if (error != 0)
			break;
		ifr->ifr_addr = *sa;
		break;
	case SIOCSIFPHYADDR:
		/*
		 * XXXRW: Isn't this priv_check() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_SETIFPHYS)) != 0)
			break;
		if (aifr->ifra_addr.sin_family != AF_INET ||
		    aifr->ifra_dstaddr.sin_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		if (aifr->ifra_addr.sin_len != sizeof(si) ||
		    aifr->ifra_dstaddr.sin_len != sizeof(si)) {
			error = EINVAL;
			break;
		}
		sc->g_src = aifr->ifra_addr.sin_addr;
		sc->g_dst = aifr->ifra_dstaddr.sin_addr;
		goto recompute;
	case SIOCSLIFPHYADDR:
		/*
		 * XXXRW: Isn't this priv_check() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_SETIFPHYS)) != 0)
			break;
		if (lifr->addr.ss_family != AF_INET ||
		    lifr->dstaddr.ss_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		if (lifr->addr.ss_len != sizeof(si) ||
		    lifr->dstaddr.ss_len != sizeof(si)) {
			error = EINVAL;
			break;
		}
		sc->g_src = (satosin(&lifr->addr))->sin_addr;
		sc->g_dst =
		    (satosin(&lifr->dstaddr))->sin_addr;
		goto recompute;
	case SIOCDIFPHYADDR:
		/*
		 * XXXRW: Isn't this priv_check() redundant to the ifnet
		 * layer check?
		 */
		if ((error = priv_check(curthread, PRIV_NET_SETIFPHYS)) != 0)
			break;
		sc->g_src.s_addr = INADDR_ANY;
		sc->g_dst.s_addr = INADDR_ANY;
		goto recompute;
	case SIOCGLIFPHYADDR:
		if (sc->g_src.s_addr == INADDR_ANY ||
		    sc->g_dst.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		error = prison_if(curthread->td_ucred, (struct sockaddr *)&si);
		if (error != 0)
			break;
		memcpy(&lifr->addr, &si, sizeof(si));
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		error = prison_if(curthread->td_ucred, (struct sockaddr *)&si);
		if (error != 0)
			break;
		memcpy(&lifr->dstaddr, &si, sizeof(si));
		break;
	case SIOCGIFPSRCADDR:
#ifdef INET6
	case SIOCGIFPSRCADDR_IN6:
#endif
		if (sc->g_src.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		error = prison_if(curthread->td_ucred, (struct sockaddr *)&si);
		if (error != 0)
			break;
		bcopy(&si, &ifr->ifr_addr, sizeof(ifr->ifr_addr));
		break;
	case SIOCGIFPDSTADDR:
#ifdef INET6
	case SIOCGIFPDSTADDR_IN6:
#endif
		if (sc->g_dst.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		error = prison_if(curthread->td_ucred, (struct sockaddr *)&si);
		if (error != 0)
			break;
		bcopy(&si, &ifr->ifr_addr, sizeof(ifr->ifr_addr));
		break;
	case GRESKEY:
		error = priv_check(curthread, PRIV_NET_GRE);
		if (error)
			break;
		error = copyin(ifr->ifr_data, &key, sizeof(key));
		if (error)
			break;
		/* adjust MTU for option header */
		if (key == 0 && sc->key != 0)		/* clear */
			adj += sizeof(key);
		else if (key != 0 && sc->key == 0)	/* set */
			adj -= sizeof(key);

		if (ifp->if_mtu + adj < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu += adj;
		sc->key = key;
		break;
	case GREGKEY:
		error = copyout(&sc->key, ifr->ifr_data, sizeof(sc->key));
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * computes a route to our destination that is not the one
 * which would be taken by ip_output(), as this one will loop back to
 * us. If the interface is p2p as  a--->b, then a routing entry exists
 * If we now send a packet to b (e.g. ping b), this will come down here
 * gets src=a, dst=b tacked on and would from ip_output() sent back to
 * if_gre.
 * Goal here is to compute a route to b that is less specific than
 * a-->b. We know that this one exists as in normal operation we have
 * at least a default route which matches.
 */
static int
gre_compute_route(struct gre_softc *sc)
{
	struct route *ro;

	ro = &sc->route;

	memset(ro, 0, sizeof(struct route));
	((struct sockaddr_in *)&ro->ro_dst)->sin_addr = sc->g_dst;
	ro->ro_dst.sa_family = AF_INET;
	ro->ro_dst.sa_len = sizeof(ro->ro_dst);

	/*
	 * toggle last bit, so our interface is not found, but a less
	 * specific route. I'd rather like to specify a shorter mask,
	 * but this is not possible. Should work though. XXX
	 * XXX MRT Use a different FIB for the tunnel to solve this problem.
	 */
	if ((GRE2IFP(sc)->if_flags & IFF_LINK1) == 0) {
		((struct sockaddr_in *)&ro->ro_dst)->sin_addr.s_addr ^=
		    htonl(0x01);
	}

#ifdef DIAGNOSTIC
	printf("%s: searching for a route to %s", if_name(GRE2IFP(sc)),
	    inet_ntoa(((struct sockaddr_in *)&ro->ro_dst)->sin_addr));
#endif

	rtalloc_fib(ro, sc->gre_fibnum);

	/*
	 * check if this returned a route at all and this route is no
	 * recursion to ourself
	 */
	if (ro->ro_rt == NULL || ro->ro_rt->rt_ifp->if_softc == sc) {
#ifdef DIAGNOSTIC
		if (ro->ro_rt == NULL)
			printf(" - no route found!\n");
		else
			printf(" - route loops back to ourself!\n");
#endif
		return EADDRNOTAVAIL;
	}

	/*
	 * now change it back - else ip_output will just drop
	 * the route and search one to this interface ...
	 */
	if ((GRE2IFP(sc)->if_flags & IFF_LINK1) == 0)
		((struct sockaddr_in *)&ro->ro_dst)->sin_addr = sc->g_dst;

#ifdef DIAGNOSTIC
	printf(", choosing %s with gateway %s", if_name(ro->ro_rt->rt_ifp),
	    inet_ntoa(((struct sockaddr_in *)(ro->ro_rt->rt_gateway))->sin_addr));
	printf("\n");
#endif

	return 0;
}

/*
 * do a checksum of a buffer - much like in_cksum, which operates on
 * mbufs.
 */
u_int16_t
gre_in_cksum(u_int16_t *p, u_int len)
{
	u_int32_t sum = 0;
	int nwords = len >> 1;

	while (nwords-- != 0)
		sum += *p++;

	if (len & 1) {
		union {
			u_short w;
			u_char c[2];
		} u;
		u.c[0] = *(u_char *)p;
		u.c[1] = 0;
		sum += u.w;
	}

	/* end-around-carry */
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (~sum);
}

static int
gremodevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		greattach();
		break;
	case MOD_UNLOAD:
		if_clone_detach(gre_cloner);
		mtx_destroy(&gre_mtx);
		break;
	default:
		return EOPNOTSUPP;
	}
	return 0;
}

static moduledata_t gre_mod = {
	"if_gre",
	gremodevent,
	0
};

DECLARE_MODULE(if_gre, gre_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_gre, 1);
