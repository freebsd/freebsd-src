/*-
 * Copyright (c) 2014 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET
#include <net/ethernet.h>
#include <netinet/ip.h>
#endif
#include <netinet/ip_encap.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <net/if_gre.h>

extern  struct domain inet6domain;
struct protosw in6_gre_protosw = {
	.pr_type =	SOCK_RAW,
	.pr_domain =	&inet6domain,
	.pr_protocol =	IPPROTO_GRE,
	.pr_flags =	PR_ATOMIC|PR_ADDR,
	.pr_input =	gre_input,
	.pr_output =	rip6_output,
	.pr_ctloutput =	rip6_ctloutput,
	.pr_usrreqs =	&rip6_usrreqs
};

VNET_DEFINE(int, ip6_gre_hlim) = IPV6_DEFHLIM;
#define	V_ip6_gre_hlim		VNET(ip6_gre_hlim)

SYSCTL_DECL(_net_inet6_ip6);
SYSCTL_INT(_net_inet6_ip6, OID_AUTO, grehlim, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(ip6_gre_hlim), 0, "Default hop limit for encapsulated packets");

static int
in6_gre_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	GRE_RLOCK_TRACKER;
	struct gre_softc *sc;
	struct ip6_hdr *ip6;

	sc = (struct gre_softc *)arg;
	if ((GRE2IFP(sc)->if_flags & IFF_UP) == 0)
		return (0);

	M_ASSERTPKTHDR(m);
	/*
	 * We expect that payload contains at least IPv4
	 * or IPv6 packet.
	 */
	if (m->m_pkthdr.len < sizeof(struct greip6) +
#ifdef INET
	    sizeof(struct ip))
#else
	    sizeof(struct ip6_hdr))
#endif
		return (0);

	GRE_RLOCK(sc);
	if (sc->gre_family == 0)
		goto bad;

	KASSERT(sc->gre_family == AF_INET6,
	    ("wrong gre_family: %d", sc->gre_family));

	ip6 = mtod(m, struct ip6_hdr *);
	if (!IN6_ARE_ADDR_EQUAL(&sc->gre_oip6.ip6_src, &ip6->ip6_dst) ||
	    !IN6_ARE_ADDR_EQUAL(&sc->gre_oip6.ip6_dst, &ip6->ip6_src))
		goto bad;

	GRE_RUNLOCK(sc);
	return (128 * 2);
bad:
	GRE_RUNLOCK(sc);
	return (0);
}

int
in6_gre_output(struct mbuf *m, int af, int hlen)
{
	struct greip6 *gi6;

	gi6 = mtod(m, struct greip6 *);
	gi6->gi6_ip6.ip6_hlim = V_ip6_gre_hlim;
	return (ip6_output(m, NULL, NULL, IPV6_MINMTU, NULL, NULL, NULL));
}

int
in6_gre_attach(struct gre_softc *sc)
{

	KASSERT(sc->gre_ecookie == NULL, ("gre_ecookie isn't NULL"));
	sc->gre_ecookie = encap_attach_func(AF_INET6, IPPROTO_GRE,
	    in6_gre_encapcheck, &in6_gre_protosw, sc);
	if (sc->gre_ecookie == NULL)
		return (EEXIST);
	return (0);
}
