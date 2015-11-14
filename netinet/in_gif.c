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
 *	$KAME: in_gif.c,v 1.54 2001/05/14 14:02:16 itojun Exp $
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
#include <sys/sysctl.h>
#include <sys/protosw.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_ecn.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <net/if_gif.h>

static int in_gif_input(struct mbuf **, int *, int);

extern  struct domain inetdomain;
static struct protosw in_gif_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		0/* IPPROTO_IPV[46] */,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		in_gif_input,
	.pr_output =		rip_output,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};

#define GIF_TTL		30
static VNET_DEFINE(int, ip_gif_ttl) = GIF_TTL;
#define	V_ip_gif_ttl		VNET(ip_gif_ttl)
SYSCTL_INT(_net_inet_ip, IPCTL_GIF_TTL, gifttl, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(ip_gif_ttl), 0, "");

int
in_gif_output(struct ifnet *ifp, struct mbuf *m, int proto, uint8_t ecn)
{
	GIF_RLOCK_TRACKER;
	struct gif_softc *sc = ifp->if_softc;
	struct ip *ip;
	int len;

	/* prepend new IP header */
	len = sizeof(struct ip);
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
		    ("in_gif_output: unexpected misalignment"));
		m->m_data += len;
		m->m_len -= ETHERIP_ALIGN;
	}
#endif
	ip = mtod(m, struct ip *);
	GIF_RLOCK(sc);
	if (sc->gif_family != AF_INET) {
		m_freem(m);
		GIF_RUNLOCK(sc);
		return (ENETDOWN);
	}
	bcopy(sc->gif_iphdr, ip, sizeof(struct ip));
	GIF_RUNLOCK(sc);

	ip->ip_p = proto;
	/* version will be set in ip_output() */
	ip->ip_ttl = V_ip_gif_ttl;
	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_tos = ecn;

	return (ip_output(m, NULL, NULL, 0, NULL, NULL));
}

static int
in_gif_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct gif_softc *sc;
	struct ifnet *gifp;
	struct ip *ip;
	uint8_t ecn;

	sc = encap_getarg(m);
	if (sc == NULL) {
		m_freem(m);
		KMOD_IPSTAT_INC(ips_nogif);
		return (IPPROTO_DONE);
	}
	gifp = GIF2IFP(sc);
	if ((gifp->if_flags & IFF_UP) != 0) {
		ip = mtod(m, struct ip *);
		ecn = ip->ip_tos;
		m_adj(m, *offp);
		gif_input(m, gifp, proto, ecn);
	} else {
		m_freem(m);
		KMOD_IPSTAT_INC(ips_nogif);
	}
	return (IPPROTO_DONE);
}

/*
 * we know that we are in IFF_UP, outer address available, and outer family
 * matched the physical addr family.  see gif_encapcheck().
 */
int
in_gif_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	const struct ip *ip;
	struct gif_softc *sc;
	int ret;

	/* sanity check done in caller */
	sc = (struct gif_softc *)arg;
	GIF_RLOCK_ASSERT(sc);

	/* check for address match */
	ip = mtod(m, const struct ip *);
	if (sc->gif_iphdr->ip_src.s_addr != ip->ip_dst.s_addr)
		return (0);
	ret = 32;
	if (sc->gif_iphdr->ip_dst.s_addr != ip->ip_src.s_addr) {
		if ((sc->gif_options & GIF_IGNORE_SOURCE) == 0)
			return (0);
	} else
		ret += 32;

	/* ingress filters on outer source */
	if ((GIF2IFP(sc)->if_flags & IFF_LINK2) == 0) {
		struct sockaddr_in sin;
		struct rtentry *rt;

		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = ip->ip_src;
		/* XXX MRT  check for the interface we would use on output */
		rt = in_rtalloc1((struct sockaddr *)&sin, 0,
		    0UL, sc->gif_fibnum);
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
in_gif_attach(struct gif_softc *sc)
{

	KASSERT(sc->gif_ecookie == NULL, ("gif_ecookie isn't NULL"));
	sc->gif_ecookie = encap_attach_func(AF_INET, -1, gif_encapcheck,
	    &in_gif_protosw, sc);
	if (sc->gif_ecookie == NULL)
		return (EEXIST);
	return (0);
}
