/*	$KAME: in_gif.c,v 1.54 2001/05/14 14:02:16 itojun Exp $	*/

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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mrouting.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/protosw.h>
#include <sys/malloc.h>
#include <sys/vimage.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_gif.h>
#include <netinet/in_var.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_ecn.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif /* MROUTING */

#include <net/if_gif.h>	

static int gif_validate4(const struct ip *, struct gif_softc *,
	struct ifnet *);

extern  struct domain inetdomain;
struct protosw in_gif_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		0/* IPPROTO_IPV[46] */,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		in_gif_input,
	.pr_output =		(pr_output_t*)rip_output,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};

#ifdef VIMAGE_GLOBALS
extern int ip_gif_ttl;
#endif
SYSCTL_V_INT(V_NET, vnet_gif, _net_inet_ip, IPCTL_GIF_TTL, gifttl,
	CTLFLAG_RW, ip_gif_ttl,	0, "");

int
in_gif_output(struct ifnet *ifp, int family, struct mbuf *m)
{
	INIT_VNET_GIF(ifp->if_vnet);
	struct gif_softc *sc = ifp->if_softc;
	struct sockaddr_in *dst = (struct sockaddr_in *)&sc->gif_ro.ro_dst;
	struct sockaddr_in *sin_src = (struct sockaddr_in *)sc->gif_psrc;
	struct sockaddr_in *sin_dst = (struct sockaddr_in *)sc->gif_pdst;
	struct ip iphdr;	/* capsule IP header, host byte ordered */
	struct etherip_header eiphdr;
	int proto, error;
	u_int8_t tos;

	GIF_LOCK_ASSERT(sc);

	if (sin_src == NULL || sin_dst == NULL ||
	    sin_src->sin_family != AF_INET ||
	    sin_dst->sin_family != AF_INET) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

	switch (family) {
#ifdef INET
	case AF_INET:
	    {
		struct ip *ip;

		proto = IPPROTO_IPV4;
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m)
				return ENOBUFS;
		}
		ip = mtod(m, struct ip *);
		tos = ip->ip_tos;
		break;
	    }
#endif /* INET */
#ifdef INET6
	case AF_INET6:
	    {
		struct ip6_hdr *ip6;
		proto = IPPROTO_IPV6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return ENOBUFS;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		tos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		break;
	    }
#endif /* INET6 */
	case AF_LINK:
 		proto = IPPROTO_ETHERIP;
 		eiphdr.eip_ver = ETHERIP_VERSION & ETHERIP_VER_VERS_MASK;
 		eiphdr.eip_pad = 0;
 		/* prepend Ethernet-in-IP header */
 		M_PREPEND(m, sizeof(struct etherip_header), M_DONTWAIT);
 		if (m && m->m_len < sizeof(struct etherip_header))
 			m = m_pullup(m, sizeof(struct etherip_header));
 		if (m == NULL)
 			return ENOBUFS;
 		bcopy(&eiphdr, mtod(m, struct etherip_header *),
		    sizeof(struct etherip_header));
		break;

	default:
#ifdef DEBUG
		printf("in_gif_output: warning: unknown family %d passed\n",
			family);
#endif
		m_freem(m);
		return EAFNOSUPPORT;
	}

	bzero(&iphdr, sizeof(iphdr));
	iphdr.ip_src = sin_src->sin_addr;
	/* bidirectional configured tunnel mode */
	if (sin_dst->sin_addr.s_addr != INADDR_ANY)
		iphdr.ip_dst = sin_dst->sin_addr;
	else {
		m_freem(m);
		return ENETUNREACH;
	}
	iphdr.ip_p = proto;
	/* version will be set in ip_output() */
	iphdr.ip_ttl = V_ip_gif_ttl;
	iphdr.ip_len = m->m_pkthdr.len + sizeof(struct ip);
	ip_ecn_ingress((ifp->if_flags & IFF_LINK1) ? ECN_ALLOWED : ECN_NOCARE,
		       &iphdr.ip_tos, &tos);

	/* prepend new IP header */
	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL) {
		printf("ENOBUFS in in_gif_output %d\n", __LINE__);
		return ENOBUFS;
	}
	bcopy(&iphdr, mtod(m, struct ip *), sizeof(struct ip));

	M_SETFIB(m, sc->gif_fibnum);

	if (dst->sin_family != sin_dst->sin_family ||
	    dst->sin_addr.s_addr != sin_dst->sin_addr.s_addr) {
		/* cache route doesn't match */
		bzero(dst, sizeof(*dst));
		dst->sin_family = sin_dst->sin_family;
		dst->sin_len = sizeof(struct sockaddr_in);
		dst->sin_addr = sin_dst->sin_addr;
		if (sc->gif_ro.ro_rt) {
			RTFREE(sc->gif_ro.ro_rt);
			sc->gif_ro.ro_rt = NULL;
		}
#if 0
		GIF2IFP(sc)->if_mtu = GIF_MTU;
#endif
	}

	if (sc->gif_ro.ro_rt == NULL) {
		in_rtalloc_ign(&sc->gif_ro, 0, sc->gif_fibnum);
		if (sc->gif_ro.ro_rt == NULL) {
			m_freem(m);
			return ENETUNREACH;
		}

		/* if it constitutes infinite encapsulation, punt. */
		if (sc->gif_ro.ro_rt->rt_ifp == ifp) {
			m_freem(m);
			return ENETUNREACH;	/* XXX */
		}
#if 0
		ifp->if_mtu = sc->gif_ro.ro_rt->rt_ifp->if_mtu
			- sizeof(struct ip);
#endif
	}

	error = ip_output(m, NULL, &sc->gif_ro, 0, NULL, NULL);

	if (!(GIF2IFP(sc)->if_flags & IFF_LINK0) &&
	    sc->gif_ro.ro_rt != NULL) {
		RTFREE(sc->gif_ro.ro_rt);
		sc->gif_ro.ro_rt = NULL;
	}

	return (error);
}

void
in_gif_input(struct mbuf *m, int off)
{
	INIT_VNET_INET(curvnet);
	struct ifnet *gifp = NULL;
	struct gif_softc *sc;
	struct ip *ip;
	int af;
	u_int8_t otos;
	int proto;

	ip = mtod(m, struct ip *);
	proto = ip->ip_p;

	sc = (struct gif_softc *)encap_getarg(m);
	if (sc == NULL) {
		m_freem(m);
		V_ipstat.ips_nogif++;
		return;
	}

	gifp = GIF2IFP(sc);
	if (gifp == NULL || (gifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		V_ipstat.ips_nogif++;
		return;
	}

	otos = ip->ip_tos;
	m_adj(m, off);

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
	    {
		struct ip *ip;
		af = AF_INET;
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m)
				return;
		}
		ip = mtod(m, struct ip *);
		if (ip_ecn_egress((gifp->if_flags & IFF_LINK1) ?
				  ECN_ALLOWED : ECN_NOCARE,
				  &otos, &ip->ip_tos) == 0) {
			m_freem(m);
			return;
		}
		break;
	    }
#endif
#ifdef INET6
	case IPPROTO_IPV6:
	    {
		struct ip6_hdr *ip6;
		u_int8_t itos, oitos;

		af = AF_INET6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		itos = oitos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		if (ip_ecn_egress((gifp->if_flags & IFF_LINK1) ?
				  ECN_ALLOWED : ECN_NOCARE,
				  &otos, &itos) == 0) {
			m_freem(m);
			return;
		}
		if (itos != oitos) {
			ip6->ip6_flow &= ~htonl(0xff << 20);
			ip6->ip6_flow |= htonl((u_int32_t)itos << 20);
		}
		break;
	    }
#endif /* INET6 */
 	case IPPROTO_ETHERIP:
 		af = AF_LINK;
 		break;	

	default:
		V_ipstat.ips_nogif++;
		m_freem(m);
		return;
	}
	gif_input(m, af, gifp);
	return;
}

/*
 * validate outer address.
 */
static int
gif_validate4(const struct ip *ip, struct gif_softc *sc, struct ifnet *ifp)
{
	INIT_VNET_INET(curvnet);
	struct sockaddr_in *src, *dst;
	struct in_ifaddr *ia4;

	src = (struct sockaddr_in *)sc->gif_psrc;
	dst = (struct sockaddr_in *)sc->gif_pdst;

	/* check for address match */
	if (src->sin_addr.s_addr != ip->ip_dst.s_addr ||
	    dst->sin_addr.s_addr != ip->ip_src.s_addr)
		return 0;

	/* martian filters on outer source - NOT done in ip_input! */
	if (IN_MULTICAST(ntohl(ip->ip_src.s_addr)))
		return 0;
	switch ((ntohl(ip->ip_src.s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return 0;
	}
	/* reject packets with broadcast on source */
	TAILQ_FOREACH(ia4, &V_in_ifaddrhead, ia_link) {
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0)
			continue;
		if (ip->ip_src.s_addr == ia4->ia_broadaddr.sin_addr.s_addr)
			return 0;
	}

	/* ingress filters on outer source */
	if ((GIF2IFP(sc)->if_flags & IFF_LINK2) == 0 && ifp) {
		struct sockaddr_in sin;
		struct rtentry *rt;

		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = ip->ip_src;
		/* XXX MRT  check for the interface we would use on output */
		rt = in_rtalloc1((struct sockaddr *)&sin, 0,
		    0UL, sc->gif_fibnum);
		if (!rt || rt->rt_ifp != ifp) {
#if 0
			log(LOG_WARNING, "%s: packet from 0x%x dropped "
			    "due to ingress filter\n", if_name(GIF2IFP(sc)),
			    (u_int32_t)ntohl(sin.sin_addr.s_addr));
#endif
			if (rt)
				RTFREE_LOCKED(rt);
			return 0;
		}
		RTFREE_LOCKED(rt);
	}

	return 32 * 2;
}

/*
 * we know that we are in IFF_UP, outer address available, and outer family
 * matched the physical addr family.  see gif_encapcheck().
 */
int
gif_encapcheck4(const struct mbuf *m, int off, int proto, void *arg)
{
	struct ip ip;
	struct gif_softc *sc;
	struct ifnet *ifp;

	/* sanity check done in caller */
	sc = (struct gif_softc *)arg;

	/* LINTED const cast */
	m_copydata(m, 0, sizeof(ip), (caddr_t)&ip);
	ifp = ((m->m_flags & M_PKTHDR) != 0) ? m->m_pkthdr.rcvif : NULL;

	return gif_validate4(&ip, sc, ifp);
}

int
in_gif_attach(struct gif_softc *sc)
{
	sc->encap_cookie4 = encap_attach_func(AF_INET, -1, gif_encapcheck,
	    &in_gif_protosw, sc);
	if (sc->encap_cookie4 == NULL)
		return EEXIST;
	return 0;
}

int
in_gif_detach(struct gif_softc *sc)
{
	int error;

	error = encap_detach(sc->encap_cookie4);
	if (error == 0)
		sc->encap_cookie4 = NULL;
	return error;
}
