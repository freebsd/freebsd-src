/*-
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
 * $NetBSD: ip_gre.c,v 1.29 2003/09/05 23:02:43 itojun Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <net/if_gre.h>

extern struct domain inetdomain;
static const struct protosw in_gre_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_GRE,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		gre_input,
	.pr_output =		rip_output,
	.pr_ctlinput =		rip_ctlinput,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};

#define	GRE_TTL			30
VNET_DEFINE(int, ip_gre_ttl) = GRE_TTL;
#define	V_ip_gre_ttl		VNET(ip_gre_ttl)
SYSCTL_INT(_net_inet_ip, OID_AUTO, grettl, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(ip_gre_ttl), 0, "");

static int
in_gre_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	GRE_RLOCK_TRACKER;
	struct gre_softc *sc;
	struct ip *ip;

	sc = (struct gre_softc *)arg;
	if ((GRE2IFP(sc)->if_flags & IFF_UP) == 0)
		return (0);

	M_ASSERTPKTHDR(m);
	/*
	 * We expect that payload contains at least IPv4
	 * or IPv6 packet.
	 */
	if (m->m_pkthdr.len < sizeof(struct greip) + sizeof(struct ip))
		return (0);

	GRE_RLOCK(sc);
	if (sc->gre_family == 0)
		goto bad;

	KASSERT(sc->gre_family == AF_INET,
	    ("wrong gre_family: %d", sc->gre_family));

	ip = mtod(m, struct ip *);
	if (sc->gre_oip.ip_src.s_addr != ip->ip_dst.s_addr ||
	    sc->gre_oip.ip_dst.s_addr != ip->ip_src.s_addr)
		goto bad;

	GRE_RUNLOCK(sc);
	return (32 * 2);
bad:
	GRE_RUNLOCK(sc);
	return (0);
}

int
in_gre_output(struct mbuf *m, int af, int hlen)
{
	struct greip *gi;

	gi = mtod(m, struct greip *);
	switch (af) {
	case AF_INET:
		/*
		 * gre_transmit() has used M_PREPEND() that doesn't guarantee
		 * m_data is contiguous more than hlen bytes. Use m_copydata()
		 * here to avoid m_pullup().
		 */
		m_copydata(m, hlen + offsetof(struct ip, ip_tos),
		    sizeof(u_char), &gi->gi_ip.ip_tos);
		m_copydata(m, hlen + offsetof(struct ip, ip_id),
		    sizeof(u_short), (caddr_t)&gi->gi_ip.ip_id);
		break;
#ifdef INET6
	case AF_INET6:
		gi->gi_ip.ip_tos = 0; /* XXX */
		ip_fillid(&gi->gi_ip);
		break;
#endif
	}
	gi->gi_ip.ip_ttl = V_ip_gre_ttl;
	gi->gi_ip.ip_len = htons(m->m_pkthdr.len);
	return (ip_output(m, NULL, NULL, IP_FORWARDING, NULL, NULL));
}

int
in_gre_attach(struct gre_softc *sc)
{

	KASSERT(sc->gre_ecookie == NULL, ("gre_ecookie isn't NULL"));
	sc->gre_ecookie = encap_attach_func(AF_INET, IPPROTO_GRE,
	    in_gre_encapcheck, &in_gre_protosw, sc);
	if (sc->gre_ecookie == NULL)
		return (EEXIST);
	return (0);
}
