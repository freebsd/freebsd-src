/*	$NetBSD: if_gre.c,v 1.42 2002/08/14 00:23:27 itojun Exp $ */
/*	 $FreeBSD$ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * See RFC 1701 and 1702 for more details.
 * If_gre is compatible with Cisco GRE tunnels, so you can
 * have a NetBSD box as the other end of a tunnel interface of a Cisco
 * router. See gre(4) for more details.
 * Also supported:  IP in IP encaps (proto 55) as of RFC 2004
 */

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_ns.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

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

#include <net/net_osdep.h>
#include <net/if_gre.h>

/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU	1476

#define GRENAME	"gre"

static MALLOC_DEFINE(M_GRE, GRENAME, "Generic Routing Encapsulation");

struct gre_softc_head gre_softc_list;

static int	gre_clone_create __P((struct if_clone *, int));
static void	gre_clone_destroy __P((struct ifnet *));
static int	gre_ioctl(struct ifnet *, u_long, caddr_t);
static int	gre_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *rt);

static struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy, 0, IF_MAXUNIT);

static int gre_compute_route(struct gre_softc *sc);

static void	greattach __P((void));

#ifdef INET
extern struct domain inetdomain;
static const struct protosw in_gre_protosw =
{ SOCK_RAW,     &inetdomain,    IPPROTO_GRE,    PR_ATOMIC|PR_ADDR,
  (pr_input_t*)gre_input, (pr_output_t*)rip_output, rip_ctlinput, rip_ctloutput,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
};
static const struct protosw in_mobile_protosw =
{ SOCK_RAW,     &inetdomain,    IPPROTO_MOBILE, PR_ATOMIC|PR_ADDR,
  (pr_input_t*)gre_mobile_input, (pr_output_t*)rip_output, rip_ctlinput, rip_ctloutput,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
};
#endif

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_OTHER, gre, CTLFLAG_RW, 0,
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

	LIST_INIT(&gre_softc_list);
	if_clone_attach(&gre_cloner);
}

static int
gre_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct gre_softc *sc;

	sc = malloc(sizeof(struct gre_softc), M_GRE, M_WAITOK);
	memset(sc, 0, sizeof(struct gre_softc));

	sc->sc_if.if_name = GRENAME;
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_unit = unit;
	sc->sc_if.if_snd.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_if.if_type = IFT_OTHER;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_hdrlen = 24; /* IP + GRE */
	sc->sc_if.if_mtu = GREMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	sc->sc_if.if_output = gre_output;
	sc->sc_if.if_ioctl = gre_ioctl;
	sc->g_dst.s_addr = sc->g_src.s_addr = INADDR_ANY;
	sc->g_proto = IPPROTO_GRE;
	sc->sc_if.if_flags |= IFF_LINK0;
	sc->encap = NULL;
	sc->called = 0;
	if_attach(&sc->sc_if);
	bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int32_t));
	LIST_INSERT_HEAD(&gre_softc_list, sc, sc_list);
	return (0);
}

static void
gre_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct gre_softc *sc = ifp->if_softc;

#ifdef INET
	if (sc->encap != NULL)
		encap_detach(sc->encap);
#endif
	LIST_REMOVE(sc, sc_list);
	bpfdetach(ifp);
	if_detach(ifp);
	free(sc, M_GRE);
}

/*
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->g_proto. See also RFC 1701 and RFC 2004
 */
static int
gre_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	   struct rtentry *rt)
{
	int error = 0;
	struct gre_softc *sc = ifp->if_softc;
	struct greip *gh;
	struct ip *ip;
	u_char osrc;
	u_short etype = 0;
	struct mobile_h mob_h;

	/*
	 * gre may cause infinite recursion calls when misconfigured.
	 * We'll prevent this by introducing upper limit.
	 */
	if (++(sc->called) > max_gre_nesting) {
		printf("%s: gre_output: recursively called too many "
		       "times(%d)\n", if_name(&sc->sc_if), sc->called);
		m_freem(m);
		error = EIO;    /* is there better errno? */
		goto end;
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == 0 ||
	    sc->g_src.s_addr == INADDR_ANY || sc->g_dst.s_addr == INADDR_ANY) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	gh = NULL;
	ip = NULL;
	osrc = 0;

	if (ifp->if_bpf) {
		/* see comment of other if_foo.c files */
		struct mbuf m0;
		u_int32_t af = dst->sa_family;

		m0.m_next = m;
		m0.m_len = 4;
		m0.m_data = (char *)&af;

		bpf_mtap(ifp, &m0);
	}

	m->m_flags &= ~(M_BCAST|M_MCAST);

	if (sc->g_proto == IPPROTO_MOBILE) {
		if (dst->sa_family == AF_INET) {
			struct mbuf *m0;
			int msiz;

			ip = mtod(m, struct ip *);

			/*
			 * RFC2004 specifies that fragmented diagrams shouldn't
			 * be encapsulated.
			 */
			if ((ip->ip_off & IP_MF) != 0) {
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
			mob_h.hcrc = gre_in_cksum((u_short *)&mob_h, msiz);

			if ((m->m_data - msiz) < m->m_pktdat) {
				/* need new mbuf */
				MGETHDR(m0, M_DONTWAIT, MT_HEADER);
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
			ip->ip_len = ntohs(ip->ip_len) + msiz;
		} else {  /* AF_INET */
			_IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EINVAL;
			goto end;
		}
	} else if (sc->g_proto == IPPROTO_GRE) {
		switch (dst->sa_family) {
		case AF_INET:
			ip = mtod(m, struct ip *);
			etype = ETHERTYPE_IP;
			break;
#ifdef NETATALK
		case AF_APPLETALK:
			etype = ETHERTYPE_ATALK;
			break;
#endif
#ifdef NS
		case AF_NS:
			etype = ETHERTYPE_NS;
			break;
#endif
		default:
			_IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EAFNOSUPPORT;
			goto end;
		}
		M_PREPEND(m, sizeof(struct greip), M_DONTWAIT);
	} else {
		_IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = EINVAL;
		goto end;
	}

	if (m == NULL) {	/* impossible */
		_IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto end;
	}

	gh = mtod(m, struct greip *);
	if (sc->g_proto == IPPROTO_GRE) {
		/* we don't have any GRE flags for now */

		memset((void *)&gh->gi_g, 0, sizeof(struct gre_h));
		gh->gi_ptype = htons(etype);
	}

	gh->gi_pr = sc->g_proto;
	if (sc->g_proto != IPPROTO_MOBILE) {
		gh->gi_src = sc->g_src;
		gh->gi_dst = sc->g_dst;
		((struct ip*)gh)->ip_hl = (sizeof(struct ip)) >> 2;
		((struct ip*)gh)->ip_ttl = GRE_TTL;
		((struct ip*)gh)->ip_tos = ip->ip_tos;
		((struct ip*)gh)->ip_id = ip->ip_id;
		gh->gi_len = m->m_pkthdr.len;
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	/* send it off */
	error = ip_output(m, NULL, &sc->route, 0, NULL);
  end:
	sc->called = 0;
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
	int s;
	struct sockaddr_in si;
	struct sockaddr *sa = NULL;
	int error;
	struct sockaddr_in sp, sm, dp, dm;

	error = 0;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		break;
	case SIOCSIFDSTADDR: 
		break;
	case SIOCSIFFLAGS:
		if ((error = suser(curthread)) != 0)
			break;
		if ((ifr->ifr_flags & IFF_LINK0) != 0)
			sc->g_proto = IPPROTO_GRE;
		else
			sc->g_proto = IPPROTO_MOBILE;
		goto recompute;
	case SIOCSIFMTU:
		if ((error = suser(curthread)) != 0)
			break;
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = sc->sc_if.if_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = suser(curthread)) != 0)
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
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case GRESPROTO:
		if ((error = suser(curthread)) != 0)
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
		if ((error = suser(curthread)) != 0)
			break;
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
				    if_name(&sc->sc_if));
#endif
			if (sc->route.ro_rt != 0) /* free old route */
				RTFREE(sc->route.ro_rt);
			if (gre_compute_route(sc) == 0)
				ifp->if_flags |= IFF_RUNNING;
			else
				ifp->if_flags &= ~IFF_RUNNING;
		}
		break;
	case GREGADDRS:
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
		break;
	case GREGADDRD:
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
		break;
	case SIOCSIFPHYADDR:
		if ((error = suser(curthread)) != 0)
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
		if ((error = suser(curthread)) != 0)
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
		sc->g_src = (satosin((struct sockadrr *)&lifr->addr))->sin_addr;
		sc->g_dst =
		    (satosin((struct sockadrr *)&lifr->dstaddr))->sin_addr;
		goto recompute;
	case SIOCDIFPHYADDR:
		if ((error = suser(curthread)) != 0)
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
		memcpy(&lifr->addr, &si, sizeof(si));
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		memcpy(&lifr->dstaddr, &si, sizeof(si));
		break;
	case SIOCGIFPSRCADDR:
		if (sc->g_src.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		bcopy(&si, &ifr->ifr_addr, sizeof(ifr->ifr_addr));
		break;
	case SIOCGIFPDSTADDR:
		if (sc->g_dst.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		bcopy(&si, &ifr->ifr_addr, sizeof(ifr->ifr_addr));
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

/*
 * computes a route to our destination that is not the one
 * which would be taken by ip_output(), as this one will loop back to
 * us. If the interface is p2p as  a--->b, then a routing entry exists
 * If we now send a packet to b (e.g. ping b), this will come down here
 * gets src=a, dst=b tacked on and would from ip_ouput() sent back to
 * if_gre.
 * Goal here is to compute a route to b that is less specific than
 * a-->b. We know that this one exists as in normal operation we have
 * at least a default route which matches.
 */
static int
gre_compute_route(struct gre_softc *sc)
{
	struct route *ro;
	u_int32_t a, b, c;

	ro = &sc->route;

	memset(ro, 0, sizeof(struct route));
	((struct sockaddr_in *)&ro->ro_dst)->sin_addr = sc->g_dst;
	ro->ro_dst.sa_family = AF_INET;
	ro->ro_dst.sa_len = sizeof(ro->ro_dst);

	/*
	 * toggle last bit, so our interface is not found, but a less
	 * specific route. I'd rather like to specify a shorter mask,
	 * but this is not possible. Should work though. XXX
	 * there is a simpler way ...
	 */
	if ((sc->sc_if.if_flags & IFF_LINK1) == 0) {
		a = ntohl(sc->g_dst.s_addr);
		b = a & 0x01;
		c = a & 0xfffffffe;
		b = b ^ 0x01;
		a = b | c;
		((struct sockaddr_in *)&ro->ro_dst)->sin_addr.s_addr
		    = htonl(a);
	}

#ifdef DIAGNOSTIC
	printf("%s: searching a route to %s", if_name(&sc->sc_if),
	    inet_ntoa(((struct sockaddr_in *)&ro->ro_dst)->sin_addr));
#endif

	rtalloc(ro);

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
	if ((sc->sc_if.if_flags & IFF_LINK1) == 0)
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
u_short
gre_in_cksum(u_short *p, u_int len)
{
	u_int sum = 0;
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
		if_clone_detach(&gre_cloner);

		while (!LIST_EMPTY(&gre_softc_list))
			gre_clone_destroy(&LIST_FIRST(&gre_softc_list)->sc_if);
		break;
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
