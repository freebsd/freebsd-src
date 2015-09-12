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
 *
 *	$KAME: in6_gif.c,v 1.49 2001/05/14 14:02:17 itojun Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/protosw.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET
#include <netinet/ip.h>
#endif
#include <netinet/ip_encap.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#endif
#include <netinet/ip_ecn.h>
#ifdef INET6
#include <netinet6/ip6_ecn.h>
#endif

#include <net/if_gif.h>

#define GIF_HLIM	30
static VNET_DEFINE(int, ip6_gif_hlim) = GIF_HLIM;
#define	V_ip6_gif_hlim			VNET(ip6_gif_hlim)

SYSCTL_DECL(_net_inet6_ip6);
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_GIF_HLIM, gifhlim, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(ip6_gif_hlim), 0, "");

static int in6_gif_input(struct mbuf **, int *, int);

extern  struct domain inet6domain;
static struct protosw in6_gif_protosw = {
	.pr_type =	SOCK_RAW,
	.pr_domain =	&inet6domain,
	.pr_protocol =	0,			/* IPPROTO_IPV[46] */
	.pr_flags =	PR_ATOMIC|PR_ADDR,
	.pr_input =	in6_gif_input,
	.pr_output =	rip6_output,
	.pr_ctloutput =	rip6_ctloutput,
	.pr_usrreqs =	&rip6_usrreqs
};

int
in6_gif_output(struct ifnet *ifp, struct mbuf *m, int proto, uint8_t ecn)
{
	GIF_RLOCK_TRACKER;
	struct gif_softc *sc = ifp->if_softc;
	struct ip6_hdr *ip6;
	int len;

	/* prepend new IP header */
	len = sizeof(struct ip6_hdr);
#ifndef __NO_STRICT_ALIGNMENT
	if (proto == IPPROTO_ETHERIP)
		len += ETHERIP_ALIGN;
#endif
	M_PREPEND(m, len, M_NOWAIT);
	if (m == NULL)
		return (ENOBUFS);
#ifndef __NO_STRICT_ALIGNMENT
	if (proto == IPPROTO_ETHERIP) {
		len = mtod(m, vm_offset_t) & 3;
		KASSERT(len == 0 || len == ETHERIP_ALIGN,
		    ("in6_gif_output: unexpected misalignment"));
		m->m_data += len;
		m->m_len -= ETHERIP_ALIGN;
	}
#endif

	ip6 = mtod(m, struct ip6_hdr *);
	GIF_RLOCK(sc);
	if (sc->gif_family != AF_INET6) {
		m_freem(m);
		GIF_RUNLOCK(sc);
		return (ENETDOWN);
	}
	bcopy(sc->gif_ip6hdr, ip6, sizeof(struct ip6_hdr));
	GIF_RUNLOCK(sc);

	ip6->ip6_flow  |= htonl((uint32_t)ecn << 20);
	ip6->ip6_nxt	= proto;
	ip6->ip6_hlim	= V_ip6_gif_hlim;
	/*
	 * force fragmentation to minimum MTU, to avoid path MTU discovery.
	 * it is too painful to ask for resend of inner packet, to achieve
	 * path MTU discovery for encapsulated packets.
	 */
	return (ip6_output(m, 0, NULL, IPV6_MINMTU, 0, NULL, NULL));
}

static int
in6_gif_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct ifnet *gifp;
	struct gif_softc *sc;
	struct ip6_hdr *ip6;
	uint8_t ecn;

	sc = encap_getarg(m);
	if (sc == NULL) {
		m_freem(m);
		IP6STAT_INC(ip6s_nogif);
		return (IPPROTO_DONE);
	}
	gifp = GIF2IFP(sc);
	if ((gifp->if_flags & IFF_UP) != 0) {
		ip6 = mtod(m, struct ip6_hdr *);
		ecn = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		m_adj(m, *offp);
		gif_input(m, gifp, proto, ecn);
	} else {
		m_freem(m);
		IP6STAT_INC(ip6s_nogif);
	}
	return (IPPROTO_DONE);
}

/*
 * we know that we are in IFF_UP, outer address available, and outer family
 * matched the physical addr family.  see gif_encapcheck().
 */
int
in6_gif_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	const struct ip6_hdr *ip6;
	struct gif_softc *sc;
	int ret;

	/* sanity check done in caller */
	sc = (struct gif_softc *)arg;
	GIF_RLOCK_ASSERT(sc);

	/*
	 * Check for address match.  Note that the check is for an incoming
	 * packet.  We should compare the *source* address in our configuration
	 * and the *destination* address of the packet, and vice versa.
	 */
	ip6 = mtod(m, const struct ip6_hdr *);
	if (!IN6_ARE_ADDR_EQUAL(&sc->gif_ip6hdr->ip6_src, &ip6->ip6_dst))
		return (0);
	ret = 128;
	if (!IN6_ARE_ADDR_EQUAL(&sc->gif_ip6hdr->ip6_dst, &ip6->ip6_src)) {
		if ((sc->gif_options & GIF_IGNORE_SOURCE) == 0)
			return (0);
	} else
		ret += 128;

	/* ingress filters on outer source */
	if ((GIF2IFP(sc)->if_flags & IFF_LINK2) == 0) {
		struct sockaddr_in6 sin6;
		struct rtentry *rt;

		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr = ip6->ip6_src;
		sin6.sin6_scope_id = 0; /* XXX */

		rt = in6_rtalloc1((struct sockaddr *)&sin6, 0, 0UL,
		    sc->gif_fibnum);
		if (rt == NULL || rt->rt_ifp != m->m_pkthdr.rcvif) {
			if (rt != NULL)
				RTFREE_LOCKED(rt);
			return (0);
		}
		RTFREE_LOCKED(rt);
	}
	return (ret);
}

int
in6_gif_attach(struct gif_softc *sc)
{

	KASSERT(sc->gif_ecookie == NULL, ("gif_ecookie isn't NULL"));
	sc->gif_ecookie = encap_attach_func(AF_INET6, -1, gif_encapcheck,
	    (void *)&in6_gif_protosw, sc);
	if (sc->gif_ecookie == NULL)
		return (EEXIST);
	return (0);
}
