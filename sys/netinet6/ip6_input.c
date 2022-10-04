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
 *	$KAME: ip6_input.c,v 1.259 2002/01/21 04:58:09 jinmei Exp $
 */

/*-
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_route.h"
#include "opt_rss.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/hhook.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/sdt.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/eventhandler.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/rss_config.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/ip_var.h>
#include <netinet/in_systm.h>
#include <net/if_llatbl.h>
#ifdef INET
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#endif /* INET */
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/ip_encap.h>
#include <netinet/in_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/mld6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_rss.h>
#ifdef SCTP
#include <netinet/sctp_pcb.h>
#include <netinet6/sctp6_var.h>
#endif

#include <netipsec/ipsec_support.h>

ip6proto_input_t	*ip6_protox[IPPROTO_MAX] = {
			    [0 ... IPPROTO_MAX - 1] = rip6_input };
ip6proto_ctlinput_t	*ip6_ctlprotox[IPPROTO_MAX] = {
			    [0 ... IPPROTO_MAX - 1] = rip6_ctlinput };

VNET_DEFINE(struct in6_ifaddrhead, in6_ifaddrhead);
VNET_DEFINE(struct in6_ifaddrlisthead *, in6_ifaddrhashtbl);
VNET_DEFINE(u_long, in6_ifaddrhmask);

static struct netisr_handler ip6_nh = {
	.nh_name = "ip6",
	.nh_handler = ip6_input,
	.nh_proto = NETISR_IPV6,
#ifdef RSS
	.nh_m2cpuid = rss_soft_m2cpuid_v6,
	.nh_policy = NETISR_POLICY_CPU,
	.nh_dispatch = NETISR_DISPATCH_HYBRID,
#else
	.nh_policy = NETISR_POLICY_FLOW,
#endif
};

static int
sysctl_netinet6_intr_queue_maxlen(SYSCTL_HANDLER_ARGS)
{
	int error, qlimit;

	netisr_getqlimit(&ip6_nh, &qlimit);
	error = sysctl_handle_int(oidp, &qlimit, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qlimit < 1)
		return (EINVAL);
	return (netisr_setqlimit(&ip6_nh, qlimit));
}
SYSCTL_DECL(_net_inet6_ip6);
SYSCTL_PROC(_net_inet6_ip6, IPV6CTL_INTRQMAXLEN, intr_queue_maxlen,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_netinet6_intr_queue_maxlen, "I",
    "Maximum size of the IPv6 input queue");

VNET_DEFINE_STATIC(bool, ip6_sav) = true;
#define	V_ip6_sav	VNET(ip6_sav)
SYSCTL_BOOL(_net_inet6_ip6, OID_AUTO, source_address_validation,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_sav), true,
    "Drop incoming packets with source address that is a local address");

#ifdef RSS
static struct netisr_handler ip6_direct_nh = {
	.nh_name = "ip6_direct",
	.nh_handler = ip6_direct_input,
	.nh_proto = NETISR_IPV6_DIRECT,
	.nh_m2cpuid = rss_soft_m2cpuid_v6,
	.nh_policy = NETISR_POLICY_CPU,
	.nh_dispatch = NETISR_DISPATCH_HYBRID,
};

static int
sysctl_netinet6_intr_direct_queue_maxlen(SYSCTL_HANDLER_ARGS)
{
	int error, qlimit;

	netisr_getqlimit(&ip6_direct_nh, &qlimit);
	error = sysctl_handle_int(oidp, &qlimit, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qlimit < 1)
		return (EINVAL);
	return (netisr_setqlimit(&ip6_direct_nh, qlimit));
}
SYSCTL_PROC(_net_inet6_ip6, IPV6CTL_INTRDQMAXLEN, intr_direct_queue_maxlen,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_netinet6_intr_direct_queue_maxlen, "I",
    "Maximum size of the IPv6 direct input queue");

#endif

VNET_DEFINE(pfil_head_t, inet6_pfil_head);

VNET_PCPUSTAT_DEFINE(struct ip6stat, ip6stat);
VNET_PCPUSTAT_SYSINIT(ip6stat);
#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(ip6stat);
#endif /* VIMAGE */

struct rmlock in6_ifaddr_lock;
RM_SYSINIT(in6_ifaddr_lock, &in6_ifaddr_lock, "in6_ifaddr_lock");

static int ip6_hopopts_input(u_int32_t *, u_int32_t *, struct mbuf **, int *);

/*
 * IP6 initialization: fill in IP6 protocol switch table.
 * All protocols not implemented in kernel go to raw IP6 protocol handler.
 */
static void
ip6_vnet_init(void *arg __unused)
{
	struct pfil_head_args args;

	TUNABLE_INT_FETCH("net.inet6.ip6.auto_linklocal",
	    &V_ip6_auto_linklocal);
	TUNABLE_INT_FETCH("net.inet6.ip6.accept_rtadv", &V_ip6_accept_rtadv);
	TUNABLE_INT_FETCH("net.inet6.ip6.no_radr", &V_ip6_no_radr);

	CK_STAILQ_INIT(&V_in6_ifaddrhead);
	V_in6_ifaddrhashtbl = hashinit(IN6ADDR_NHASH, M_IFADDR,
	    &V_in6_ifaddrhmask);

	/* Initialize packet filter hooks. */
	args.pa_version = PFIL_VERSION;
	args.pa_flags = PFIL_IN | PFIL_OUT;
	args.pa_type = PFIL_TYPE_IP6;
	args.pa_headname = PFIL_INET6_NAME;
	V_inet6_pfil_head = pfil_head_register(&args);

	if (hhook_head_register(HHOOK_TYPE_IPSEC_IN, AF_INET6,
	    &V_ipsec_hhh_in[HHOOK_IPSEC_INET6],
	    HHOOK_WAITOK | HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register input helper hook\n",
		    __func__);
	if (hhook_head_register(HHOOK_TYPE_IPSEC_OUT, AF_INET6,
	    &V_ipsec_hhh_out[HHOOK_IPSEC_INET6],
	    HHOOK_WAITOK | HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register output helper hook\n",
		    __func__);

	scope6_init();
	addrsel_policy_init();
	nd6_init();
	frag6_init();

	V_ip6_desync_factor = arc4random() % MAX_TEMP_DESYNC_FACTOR;

	/* Skip global initialization stuff for non-default instances. */
#ifdef VIMAGE
	netisr_register_vnet(&ip6_nh);
#ifdef RSS
	netisr_register_vnet(&ip6_direct_nh);
#endif
#endif
}
VNET_SYSINIT(ip6_vnet_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH,
    ip6_vnet_init, NULL);

static void
ip6_init(void *arg __unused)
{

	/*
	 * Register statically those protocols that are unlikely to ever go
	 * dynamic.
	 */
	IP6PROTO_REGISTER(IPPROTO_ICMPV6, icmp6_input, rip6_ctlinput);
	IP6PROTO_REGISTER(IPPROTO_DSTOPTS, dest6_input, NULL);
	IP6PROTO_REGISTER(IPPROTO_ROUTING, route6_input, NULL);
	IP6PROTO_REGISTER(IPPROTO_FRAGMENT, frag6_input, NULL);
	IP6PROTO_REGISTER(IPPROTO_IPV4, encap6_input, NULL);
	IP6PROTO_REGISTER(IPPROTO_IPV6, encap6_input, NULL);
	IP6PROTO_REGISTER(IPPROTO_ETHERIP, encap6_input, NULL);
	IP6PROTO_REGISTER(IPPROTO_GRE, encap6_input, NULL);
	IP6PROTO_REGISTER(IPPROTO_PIM, encap6_input, NULL);
#ifdef SCTP	/* XXX: has a loadable & static version */
	IP6PROTO_REGISTER(IPPROTO_SCTP, sctp6_input, sctp6_ctlinput);
#endif

	EVENTHANDLER_REGISTER(vm_lowmem, frag6_drain, NULL, LOWMEM_PRI_DEFAULT);
	EVENTHANDLER_REGISTER(mbuf_lowmem, frag6_drain, NULL,
	    LOWMEM_PRI_DEFAULT);

	netisr_register(&ip6_nh);
#ifdef RSS
	netisr_register(&ip6_direct_nh);
#endif
}
SYSINIT(ip6_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, ip6_init, NULL);

int
ip6proto_register(uint8_t proto, ip6proto_input_t input,
    ip6proto_ctlinput_t ctl)
{

	MPASS(proto > 0);

	if (ip6_protox[proto] == rip6_input) {
		ip6_protox[proto] = input;
		ip6_ctlprotox[proto] = ctl;
		return (0);
	} else
		return (EEXIST);
}

int
ip6proto_unregister(uint8_t proto)
{

	MPASS(proto > 0);

	if (ip6_protox[proto] != rip6_input) {
		ip6_protox[proto] = rip6_input;
		ip6_ctlprotox[proto] = rip6_ctlinput;
		return (0);
	} else
		return (ENOENT);
}

#ifdef VIMAGE
static void
ip6_destroy(void *unused __unused)
{
	struct ifaddr *ifa, *nifa;
	struct ifnet *ifp;
	int error;

#ifdef RSS
	netisr_unregister_vnet(&ip6_direct_nh);
#endif
	netisr_unregister_vnet(&ip6_nh);

	pfil_head_unregister(V_inet6_pfil_head);
	error = hhook_head_deregister(V_ipsec_hhh_in[HHOOK_IPSEC_INET6]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister input helper hook "
		    "type HHOOK_TYPE_IPSEC_IN, id HHOOK_IPSEC_INET6: "
		    "error %d returned\n", __func__, error);
	}
	error = hhook_head_deregister(V_ipsec_hhh_out[HHOOK_IPSEC_INET6]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister output helper hook "
		    "type HHOOK_TYPE_IPSEC_OUT, id HHOOK_IPSEC_INET6: "
		    "error %d returned\n", __func__, error);
	}

	/* Cleanup addresses. */
	IFNET_RLOCK();
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		/* Cannot lock here - lock recursion. */
		/* IF_ADDR_LOCK(ifp); */
		CK_STAILQ_FOREACH_SAFE(ifa, &ifp->if_addrhead, ifa_link, nifa) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			in6_purgeaddr(ifa);
		}
		/* IF_ADDR_UNLOCK(ifp); */
		in6_ifdetach_destroy(ifp);
		mld_domifdetach(ifp);
	}
	IFNET_RUNLOCK();

	/* Make sure any routes are gone as well. */
	rib_flush_routes_family(AF_INET6);

	frag6_destroy();
	nd6_destroy();
	in6_ifattach_destroy();

	hashdestroy(V_in6_ifaddrhashtbl, M_IFADDR, V_in6_ifaddrhmask);
}

VNET_SYSUNINIT(inet6, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, ip6_destroy, NULL);
#endif

static int
ip6_input_hbh(struct mbuf **mp, uint32_t *plen, uint32_t *rtalert, int *off,
    int *nxt, int *ours)
{
	struct mbuf *m;
	struct ip6_hdr *ip6;
	struct ip6_hbh *hbh;

	if (ip6_hopopts_input(plen, rtalert, mp, off)) {
#if 0	/*touches NULL pointer*/
		in6_ifstat_inc((*mp)->m_pkthdr.rcvif, ifs6_in_discard);
#endif
		goto out;	/* m have already been freed */
	}

	/* adjust pointer */
	m = *mp;
	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * if the payload length field is 0 and the next header field
	 * indicates Hop-by-Hop Options header, then a Jumbo Payload
	 * option MUST be included.
	 */
	if (ip6->ip6_plen == 0 && *plen == 0) {
		/*
		 * Note that if a valid jumbo payload option is
		 * contained, ip6_hopopts_input() must set a valid
		 * (non-zero) payload length to the variable plen.
		 */
		IP6STAT_INC(ip6s_badoptions);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
		icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_HEADER,
			    (caddr_t)&ip6->ip6_plen - (caddr_t)ip6);
		goto out;
	}
	/* ip6_hopopts_input() ensures that mbuf is contiguous */
	hbh = (struct ip6_hbh *)(ip6 + 1);
	*nxt = hbh->ip6h_nxt;

	/*
	 * If we are acting as a router and the packet contains a
	 * router alert option, see if we know the option value.
	 * Currently, we only support the option value for MLD, in which
	 * case we should pass the packet to the multicast routing
	 * daemon.
	 */
	if (*rtalert != ~0) {
		switch (*rtalert) {
		case IP6OPT_RTALERT_MLD:
			if (V_ip6_forwarding)
				*ours = 1;
			break;
		default:
			/*
			 * RFC2711 requires unrecognized values must be
			 * silently ignored.
			 */
			break;
		}
	}

	return (0);

out:
	return (1);
}

#ifdef RSS
/*
 * IPv6 direct input routine.
 *
 * This is called when reinjecting completed fragments where
 * all of the previous checking and book-keeping has been done.
 */
void
ip6_direct_input(struct mbuf *m)
{
	int off, nxt;
	int nest;
	struct m_tag *mtag;
	struct ip6_direct_ctx *ip6dc;

	mtag = m_tag_locate(m, MTAG_ABI_IPV6, IPV6_TAG_DIRECT, NULL);
	KASSERT(mtag != NULL, ("Reinjected packet w/o direct ctx tag!"));

	ip6dc = (struct ip6_direct_ctx *)(mtag + 1);
	nxt = ip6dc->ip6dc_nxt;
	off = ip6dc->ip6dc_off;

	nest = 0;

	m_tag_delete(m, mtag);

	while (nxt != IPPROTO_DONE) {
		if (V_ip6_hdrnestlimit && (++nest > V_ip6_hdrnestlimit)) {
			IP6STAT_INC(ip6s_toomanyhdr);
			goto bad;
		}

		/*
		 * protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			IP6STAT_INC(ip6s_tooshort);
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
			goto bad;
		}

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
		if (IPSEC_ENABLED(ipv6)) {
			if (IPSEC_INPUT(ipv6, m, off, nxt) != 0)
				return;
		}
#endif /* IPSEC */

		nxt = ip6_protox[nxt](&m, &off, nxt);
	}
	return;
bad:
	m_freem(m);
}
#endif

void
ip6_input(struct mbuf *m)
{
	struct in6_addr odst;
	struct ip6_hdr *ip6;
	struct in6_ifaddr *ia;
	struct ifnet *rcvif;
	u_int32_t plen;
	u_int32_t rtalert = ~0;
	int off = sizeof(struct ip6_hdr), nest;
	int nxt, ours = 0;
	int srcrt = 0;

	/*
	 * Drop the packet if IPv6 operation is disabled on the interface.
	 */
	rcvif = m->m_pkthdr.rcvif;
	if ((ND_IFINFO(rcvif)->flags & ND6_IFF_IFDISABLED))
		goto bad;

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/*
	 * should the inner packet be considered authentic?
	 * see comment in ah4_input().
	 * NB: m cannot be NULL when passed to the input routine
	 */

	m->m_flags &= ~M_AUTHIPHDR;
	m->m_flags &= ~M_AUTHIPDGM;

#endif /* IPSEC */

	if (m->m_flags & M_FASTFWD_OURS) {
		/*
		 * Firewall changed destination to local.
		 */
		ip6 = mtod(m, struct ip6_hdr *);
		goto passin;
	}

	/*
	 * mbuf statistics
	 */
	if (m->m_flags & M_EXT) {
		if (m->m_next)
			IP6STAT_INC(ip6s_mext2m);
		else
			IP6STAT_INC(ip6s_mext1);
	} else {
		if (m->m_next) {
			struct ifnet *ifp = (m->m_flags & M_LOOP) ? V_loif : rcvif;
			int ifindex = ifp->if_index;
			if (ifindex >= IP6S_M2MMAX)
				ifindex = 0;
			IP6STAT_INC(ip6s_m2m[ifindex]);
		} else
			IP6STAT_INC(ip6s_m1);
	}

	in6_ifstat_inc(rcvif, ifs6_in_receive);
	IP6STAT_INC(ip6s_total);

	/*
	 * L2 bridge code and some other code can return mbuf chain
	 * that does not conform to KAME requirement.  too bad.
	 * XXX: fails to join if interface MTU > MCLBYTES.  jumbogram?
	 */
	if (m && m->m_next != NULL && m->m_pkthdr.len < MCLBYTES) {
		struct mbuf *n;

		if (m->m_pkthdr.len > MHLEN)
			n = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		else
			n = m_gethdr(M_NOWAIT, MT_DATA);
		if (n == NULL)
			goto bad;

		m_move_pkthdr(n, m);
		m_copydata(m, 0, n->m_pkthdr.len, mtod(n, caddr_t));
		n->m_len = n->m_pkthdr.len;
		m_freem(m);
		m = n;
	}
	if (m->m_len < sizeof(struct ip6_hdr)) {
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			IP6STAT_INC(ip6s_toosmall);
			in6_ifstat_inc(rcvif, ifs6_in_hdrerr);
			goto bad;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		IP6STAT_INC(ip6s_badvers);
		in6_ifstat_inc(rcvif, ifs6_in_hdrerr);
		goto bad;
	}

	IP6STAT_INC(ip6s_nxthist[ip6->ip6_nxt]);
	IP_PROBE(receive, NULL, NULL, ip6, rcvif, NULL, ip6);

	/*
	 * Check against address spoofing/corruption.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst)) {
		/*
		 * XXX: "badscope" is not very suitable for a multicast source.
		 */
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(rcvif, ifs6_in_addrerr);
		goto bad;
	}
	if (IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) &&
	    !(m->m_flags & M_LOOP)) {
		/*
		 * In this case, the packet should come from the loopback
		 * interface.  However, we cannot just check the if_flags,
		 * because ip6_mloopback() passes the "actual" interface
		 * as the outgoing/incoming interface.
		 */
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(rcvif, ifs6_in_addrerr);
		goto bad;
	}
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) &&
	    IPV6_ADDR_MC_SCOPE(&ip6->ip6_dst) == 0) {
		/*
		 * RFC4291 2.7:
		 * Nodes must not originate a packet to a multicast address
		 * whose scop field contains the reserved value 0; if such
		 * a packet is received, it must be silently dropped.
		 */
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(rcvif, ifs6_in_addrerr);
		goto bad;
	}
	/*
	 * The following check is not documented in specs.  A malicious
	 * party may be able to use IPv4 mapped addr to confuse tcp/udp stack
	 * and bypass security checks (act as if it was from 127.0.0.1 by using
	 * IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * We have supported IPv6-only kernels for a few years and this issue
	 * has not come up.  The world seems to move mostly towards not using
	 * v4mapped on the wire, so it makes sense for us to keep rejecting
	 * any such packets.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(rcvif, ifs6_in_addrerr);
		goto bad;
	}
#if 0
	/*
	 * Reject packets with IPv4 compatible addresses (auto tunnel).
	 *
	 * The code forbids auto tunnel relay case in RFC1933 (the check is
	 * stronger than RFC1933).  We may want to re-enable it if mech-xx
	 * is revised to forbid relaying case.
	 */
	if (IN6_IS_ADDR_V4COMPAT(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4COMPAT(&ip6->ip6_dst)) {
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
#endif
	/*
	 * Try to forward the packet, but if we fail continue.
	 * ip6_tryforward() does not generate redirects, so fall
	 * through to normal processing if redirects are required.
	 * ip6_tryforward() does inbound and outbound packet firewall
	 * processing. If firewall has decided that destination becomes
	 * our local address, it sets M_FASTFWD_OURS flag. In this
	 * case skip another inbound firewall processing and update
	 * ip6 pointer.
	 */
	if (V_ip6_forwarding != 0 && V_ip6_sendredirects == 0
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	    && (!IPSEC_ENABLED(ipv6) ||
	    IPSEC_CAPS(ipv6, m, IPSEC_CAP_OPERABLE) == 0)
#endif
	    ) {
		if ((m = ip6_tryforward(m)) == NULL)
			return;
		if (m->m_flags & M_FASTFWD_OURS) {
			ip6 = mtod(m, struct ip6_hdr *);
			goto passin;
		}
	}
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/*
	 * Bypass packet filtering for packets previously handled by IPsec.
	 */
	if (IPSEC_ENABLED(ipv6) &&
	    IPSEC_CAPS(ipv6, m, IPSEC_CAP_BYPASS_FILTER) != 0)
			goto passin;
#endif
	/*
	 * Run through list of hooks for input packets.
	 *
	 * NB: Beware of the destination address changing
	 *     (e.g. by NAT rewriting).  When this happens,
	 *     tell ip6_forward to do the right thing.
	 */

	/* Jump over all PFIL processing if hooks are not active. */
	if (!PFIL_HOOKED_IN(V_inet6_pfil_head))
		goto passin;

	odst = ip6->ip6_dst;
	if (pfil_mbuf_in(V_inet6_pfil_head, &m, m->m_pkthdr.rcvif,
	    NULL) != PFIL_PASS)
		return;
	ip6 = mtod(m, struct ip6_hdr *);
	srcrt = !IN6_ARE_ADDR_EQUAL(&odst, &ip6->ip6_dst);
	if ((m->m_flags & (M_IP6_NEXTHOP | M_FASTFWD_OURS)) == M_IP6_NEXTHOP &&
	    m_tag_find(m, PACKET_TAG_IPFORWARD, NULL) != NULL) {
		/*
		 * Directly ship the packet on.  This allows forwarding
		 * packets originally destined to us to some other directly
		 * connected host.
		 */
		ip6_forward(m, 1);
		return;
	}

passin:
	/*
	 * Disambiguate address scope zones (if there is ambiguity).
	 * We first make sure that the original source or destination address
	 * is not in our internal form for scoped addresses.  Such addresses
	 * are not necessarily invalid spec-wise, but we cannot accept them due
	 * to the usage conflict.
	 * in6_setscope() then also checks and rejects the cases where src or
	 * dst are the loopback address and the receiving interface
	 * is not loopback.
	 */
	if (in6_clearscope(&ip6->ip6_src) || in6_clearscope(&ip6->ip6_dst)) {
		IP6STAT_INC(ip6s_badscope); /* XXX */
		goto bad;
	}
	if (in6_setscope(&ip6->ip6_src, rcvif, NULL) ||
	    in6_setscope(&ip6->ip6_dst, rcvif, NULL)) {
		IP6STAT_INC(ip6s_badscope);
		goto bad;
	}
	if (m->m_flags & M_FASTFWD_OURS) {
		m->m_flags &= ~M_FASTFWD_OURS;
		ours = 1;
		goto hbhcheck;
	}
	/*
	 * Multicast check. Assume packet is for us to avoid
	 * prematurely taking locks.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		ours = 1;
		in6_ifstat_inc(rcvif, ifs6_in_mcast);
		goto hbhcheck;
	}
	/*
	 * Unicast check
	 * XXX: For now we keep link-local IPv6 addresses with embedded
	 *      scope zone id, therefore we use zero zoneid here.
	 */
	ia = in6ifa_ifwithaddr(&ip6->ip6_dst, 0 /* XXX */, false);
	if (ia != NULL) {
		if (ia->ia6_flags & IN6_IFF_NOTREADY) {
			char ip6bufs[INET6_ADDRSTRLEN];
			char ip6bufd[INET6_ADDRSTRLEN];
			/* address is not ready, so discard the packet. */
			nd6log((LOG_INFO,
			    "ip6_input: packet to an unready address %s->%s\n",
			    ip6_sprintf(ip6bufs, &ip6->ip6_src),
			    ip6_sprintf(ip6bufd, &ip6->ip6_dst)));
			goto bad;
		}
		if (V_ip6_sav && !(m->m_flags & M_LOOP) &&
		    __predict_false(in6_localip_fib(&ip6->ip6_src,
			    rcvif->if_fib))) {
			IP6STAT_INC(ip6s_badscope); /* XXX */
			goto bad;
		}
		/* Count the packet in the ip address stats */
		counter_u64_add(ia->ia_ifa.ifa_ipackets, 1);
		counter_u64_add(ia->ia_ifa.ifa_ibytes, m->m_pkthdr.len);
		ours = 1;
		goto hbhcheck;
	}

	/*
	 * Now there is no reason to process the packet if it's not our own
	 * and we're not a router.
	 */
	if (!V_ip6_forwarding) {
		IP6STAT_INC(ip6s_cantforward);
		goto bad;
	}

  hbhcheck:
	/*
	 * Process Hop-by-Hop options header if it's contained.
	 * m may be modified in ip6_hopopts_input().
	 * If a JumboPayload option is included, plen will also be modified.
	 */
	plen = (u_int32_t)ntohs(ip6->ip6_plen);
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
		if (ip6_input_hbh(&m, &plen, &rtalert, &off, &nxt, &ours) != 0)
			return;
	} else
		nxt = ip6->ip6_nxt;

	/*
	 * Use mbuf flags to propagate Router Alert option to
	 * ICMPv6 layer, as hop-by-hop options have been stripped.
	 */
	if (rtalert != ~0)
		m->m_flags |= M_RTALERT_MLD;

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPv6 header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) < plen) {
		IP6STAT_INC(ip6s_tooshort);
		in6_ifstat_inc(rcvif, ifs6_in_truncated);
		goto bad;
	}
	if (m->m_pkthdr.len > sizeof(struct ip6_hdr) + plen) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = sizeof(struct ip6_hdr) + plen;
			m->m_pkthdr.len = sizeof(struct ip6_hdr) + plen;
		} else
			m_adj(m, sizeof(struct ip6_hdr) + plen - m->m_pkthdr.len);
	}

	/*
	 * Forward if desirable.
	 */
	if (V_ip6_mrouter &&
	    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		/*
		 * If we are acting as a multicast router, all
		 * incoming multicast packets are passed to the
		 * kernel-level multicast forwarding function.
		 * The packet is returned (relatively) intact; if
		 * ip6_mforward() returns a non-zero value, the packet
		 * must be discarded, else it may be accepted below.
		 *
		 * XXX TODO: Check hlim and multicast scope here to avoid
		 * unnecessarily calling into ip6_mforward().
		 */
		if (ip6_mforward && ip6_mforward(ip6, rcvif, m)) {
			IP6STAT_INC(ip6s_cantforward);
			goto bad;
		}
	} else if (!ours) {
		ip6_forward(m, srcrt);
		return;
	}

	/*
	 * Tell launch routine the next header
	 */
	IP6STAT_INC(ip6s_delivered);
	in6_ifstat_inc(rcvif, ifs6_in_deliver);
	nest = 0;

	while (nxt != IPPROTO_DONE) {
		if (V_ip6_hdrnestlimit && (++nest > V_ip6_hdrnestlimit)) {
			IP6STAT_INC(ip6s_toomanyhdr);
			goto bad;
		}

		/*
		 * protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			IP6STAT_INC(ip6s_tooshort);
			in6_ifstat_inc(rcvif, ifs6_in_truncated);
			goto bad;
		}

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
		if (IPSEC_ENABLED(ipv6)) {
			if (IPSEC_INPUT(ipv6, m, off, nxt) != 0)
				return;
		}
#endif /* IPSEC */

		nxt = ip6_protox[nxt](&m, &off, nxt);
	}
	return;
bad:
	in6_ifstat_inc(rcvif, ifs6_in_discard);
	if (m != NULL)
		m_freem(m);
}

/*
 * Hop-by-Hop options header processing. If a valid jumbo payload option is
 * included, the real payload length will be stored in plenp.
 *
 * rtalertp - XXX: should be stored more smart way
 */
static int
ip6_hopopts_input(u_int32_t *plenp, u_int32_t *rtalertp,
    struct mbuf **mp, int *offp)
{
	struct mbuf *m = *mp;
	int off = *offp, hbhlen;
	struct ip6_hbh *hbh;

	/* validation of the length of the header */
	if (m->m_len < off + sizeof(*hbh)) {
		m = m_pullup(m, off + sizeof(*hbh));
		if (m == NULL) {
			IP6STAT_INC(ip6s_exthdrtoolong);
			*mp = NULL;
			return (-1);
		}
	}
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	hbhlen = (hbh->ip6h_len + 1) << 3;

	if (m->m_len < off + hbhlen) {
		m = m_pullup(m, off + hbhlen);
		if (m == NULL) {
			IP6STAT_INC(ip6s_exthdrtoolong);
			*mp = NULL;
			return (-1);
		}
	}
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	off += hbhlen;
	hbhlen -= sizeof(struct ip6_hbh);
	if (ip6_process_hopopts(m, (u_int8_t *)hbh + sizeof(struct ip6_hbh),
				hbhlen, rtalertp, plenp) < 0) {
		*mp = NULL;
		return (-1);
	}

	*offp = off;
	*mp = m;
	return (0);
}

/*
 * Search header for all Hop-by-hop options and process each option.
 * This function is separate from ip6_hopopts_input() in order to
 * handle a case where the sending node itself process its hop-by-hop
 * options header. In such a case, the function is called from ip6_output().
 *
 * The function assumes that hbh header is located right after the IPv6 header
 * (RFC2460 p7), opthead is pointer into data content in m, and opthead to
 * opthead + hbhlen is located in contiguous memory region.
 */
int
ip6_process_hopopts(struct mbuf *m, u_int8_t *opthead, int hbhlen,
    u_int32_t *rtalertp, u_int32_t *plenp)
{
	struct ip6_hdr *ip6;
	int optlen = 0;
	u_int8_t *opt = opthead;
	u_int16_t rtalert_val;
	u_int32_t jumboplen;
	const int erroff = sizeof(struct ip6_hdr) + sizeof(struct ip6_hbh);

	for (; hbhlen > 0; hbhlen -= optlen, opt += optlen) {
		switch (*opt) {
		case IP6OPT_PAD1:
			optlen = 1;
			break;
		case IP6OPT_PADN:
			if (hbhlen < IP6OPT_MINLEN) {
				IP6STAT_INC(ip6s_toosmall);
				goto bad;
			}
			optlen = *(opt + 1) + 2;
			break;
		case IP6OPT_ROUTER_ALERT:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_RTALERT_LEN) {
				IP6STAT_INC(ip6s_toosmall);
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_RTALERT_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 1 - opthead);
				return (-1);
			}
			optlen = IP6OPT_RTALERT_LEN;
			bcopy((caddr_t)(opt + 2), (caddr_t)&rtalert_val, 2);
			*rtalertp = ntohs(rtalert_val);
			break;
		case IP6OPT_JUMBO:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_JUMBO_LEN) {
				IP6STAT_INC(ip6s_toosmall);
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_JUMBO_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 1 - opthead);
				return (-1);
			}
			optlen = IP6OPT_JUMBO_LEN;

			/*
			 * IPv6 packets that have non 0 payload length
			 * must not contain a jumbo payload option.
			 */
			ip6 = mtod(m, struct ip6_hdr *);
			if (ip6->ip6_plen) {
				IP6STAT_INC(ip6s_badoptions);
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt - opthead);
				return (-1);
			}

			/*
			 * We may see jumbolen in unaligned location, so
			 * we'd need to perform bcopy().
			 */
			bcopy(opt + 2, &jumboplen, sizeof(jumboplen));
			jumboplen = (u_int32_t)htonl(jumboplen);

#if 1
			/*
			 * if there are multiple jumbo payload options,
			 * *plenp will be non-zero and the packet will be
			 * rejected.
			 * the behavior may need some debate in ipngwg -
			 * multiple options does not make sense, however,
			 * there's no explicit mention in specification.
			 */
			if (*plenp != 0) {
				IP6STAT_INC(ip6s_badoptions);
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 2 - opthead);
				return (-1);
			}
#endif

			/*
			 * jumbo payload length must be larger than 65535.
			 */
			if (jumboplen <= IPV6_MAXPACKET) {
				IP6STAT_INC(ip6s_badoptions);
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 2 - opthead);
				return (-1);
			}
			*plenp = jumboplen;

			break;
		default:		/* unknown option */
			if (hbhlen < IP6OPT_MINLEN) {
				IP6STAT_INC(ip6s_toosmall);
				goto bad;
			}
			optlen = ip6_unknown_opt(opt, m,
			    erroff + opt - opthead);
			if (optlen == -1)
				return (-1);
			optlen += 2;
			break;
		}
	}

	return (0);

  bad:
	m_freem(m);
	return (-1);
}

/*
 * Unknown option processing.
 * The third argument `off' is the offset from the IPv6 header to the option,
 * which is necessary if the IPv6 header the and option header and IPv6 header
 * is not contiguous in order to return an ICMPv6 error.
 */
int
ip6_unknown_opt(u_int8_t *optp, struct mbuf *m, int off)
{
	struct ip6_hdr *ip6;

	switch (IP6OPT_TYPE(*optp)) {
	case IP6OPT_TYPE_SKIP: /* ignore the option */
		return ((int)*(optp + 1));
	case IP6OPT_TYPE_DISCARD:	/* silently discard */
		m_freem(m);
		return (-1);
	case IP6OPT_TYPE_FORCEICMP: /* send ICMP even if multicasted */
		IP6STAT_INC(ip6s_badoptions);
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_OPTION, off);
		return (-1);
	case IP6OPT_TYPE_ICMP: /* send ICMP if not multicasted */
		IP6STAT_INC(ip6s_badoptions);
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    (m->m_flags & (M_BCAST|M_MCAST)))
			m_freem(m);
		else
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_OPTION, off);
		return (-1);
	}

	m_freem(m);		/* XXX: NOTREACHED */
	return (-1);
}

/*
 * Create the "control" list for this pcb.
 * These functions will not modify mbuf chain at all.
 *
 * The routine will be called from upper layer handlers like tcp6_input().
 * Thus the routine assumes that the caller (tcp6_input) have already
 * called m_pullup() and all the extension headers are located in the
 * very first mbuf on the mbuf chain.
 *
 * ip6_savecontrol_v4 will handle those options that are possible to be
 * set on a v4-mapped socket.
 * ip6_savecontrol will directly call ip6_savecontrol_v4 to handle those
 * options and handle the v6-only ones itself.
 */
struct mbuf **
ip6_savecontrol_v4(struct inpcb *inp, struct mbuf *m, struct mbuf **mp,
    int *v4only)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

#ifdef SO_TIMESTAMP
	if ((inp->inp_socket->so_options & SO_TIMESTAMP) != 0) {
		union {
			struct timeval tv;
			struct bintime bt;
			struct timespec ts;
		} t;
		struct bintime boottimebin, bt1;
		struct timespec ts1;
		bool stamped;

		stamped = false;
		switch (inp->inp_socket->so_ts_clock) {
		case SO_TS_REALTIME_MICRO:
			if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
			    M_TSTMP)) {
				mbuf_tstmp2timespec(m, &ts1);
				timespec2bintime(&ts1, &bt1);
				getboottimebin(&boottimebin);
				bintime_add(&bt1, &boottimebin);
				bintime2timeval(&bt1, &t.tv);
			} else {
				microtime(&t.tv);
			}
			*mp = sbcreatecontrol(&t.tv, sizeof(t.tv),
			    SCM_TIMESTAMP, SOL_SOCKET, M_NOWAIT);
			if (*mp != NULL) {
				mp = &(*mp)->m_next;
				stamped = true;
			}
			break;

		case SO_TS_BINTIME:
			if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
			    M_TSTMP)) {
				mbuf_tstmp2timespec(m, &ts1);
				timespec2bintime(&ts1, &t.bt);
				getboottimebin(&boottimebin);
				bintime_add(&t.bt, &boottimebin);
			} else {
				bintime(&t.bt);
			}
			*mp = sbcreatecontrol(&t.bt, sizeof(t.bt), SCM_BINTIME,
			    SOL_SOCKET, M_NOWAIT);
			if (*mp != NULL) {
				mp = &(*mp)->m_next;
				stamped = true;
			}
			break;

		case SO_TS_REALTIME:
			if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
			    M_TSTMP)) {
				mbuf_tstmp2timespec(m, &t.ts);
				getboottimebin(&boottimebin);
				bintime2timespec(&boottimebin, &ts1);
				timespecadd(&t.ts, &ts1, &t.ts);
			} else {
				nanotime(&t.ts);
			}
			*mp = sbcreatecontrol(&t.ts, sizeof(t.ts),
			    SCM_REALTIME, SOL_SOCKET, M_NOWAIT);
			if (*mp != NULL) {
				mp = &(*mp)->m_next;
				stamped = true;
			}
			break;

		case SO_TS_MONOTONIC:
			if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
			    M_TSTMP))
				mbuf_tstmp2timespec(m, &t.ts);
			else
				nanouptime(&t.ts);
			*mp = sbcreatecontrol(&t.ts, sizeof(t.ts),
			    SCM_MONOTONIC, SOL_SOCKET, M_NOWAIT);
			if (*mp != NULL) {
				mp = &(*mp)->m_next;
				stamped = true;
			}
			break;

		default:
			panic("unknown (corrupted) so_ts_clock");
		}
		if (stamped && (m->m_flags & (M_PKTHDR | M_TSTMP)) ==
		    (M_PKTHDR | M_TSTMP)) {
			struct sock_timestamp_info sti;

			bzero(&sti, sizeof(sti));
			sti.st_info_flags = ST_INFO_HW;
			if ((m->m_flags & M_TSTMP_HPREC) != 0)
				sti.st_info_flags |= ST_INFO_HW_HPREC;
			*mp = sbcreatecontrol(&sti, sizeof(sti), SCM_TIME_INFO,
			    SOL_SOCKET, M_NOWAIT);
			if (*mp != NULL)
				mp = &(*mp)->m_next;
		}
	}
#endif

#define IS2292(inp, x, y)	(((inp)->inp_flags & IN6P_RFC2292) ? (x) : (y))
	/* RFC 2292 sec. 5 */
	if ((inp->inp_flags & IN6P_PKTINFO) != 0) {
		struct in6_pktinfo pi6;

		if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
#ifdef INET
			struct ip *ip;

			ip = mtod(m, struct ip *);
			pi6.ipi6_addr.s6_addr32[0] = 0;
			pi6.ipi6_addr.s6_addr32[1] = 0;
			pi6.ipi6_addr.s6_addr32[2] = IPV6_ADDR_INT32_SMP;
			pi6.ipi6_addr.s6_addr32[3] = ip->ip_dst.s_addr;
#else
			/* We won't hit this code */
			bzero(&pi6.ipi6_addr, sizeof(struct in6_addr));
#endif
		} else {	
			bcopy(&ip6->ip6_dst, &pi6.ipi6_addr, sizeof(struct in6_addr));
			in6_clearscope(&pi6.ipi6_addr);	/* XXX */
		}
		pi6.ipi6_ifindex =
		    (m && m->m_pkthdr.rcvif) ? m->m_pkthdr.rcvif->if_index : 0;

		*mp = sbcreatecontrol(&pi6, sizeof(struct in6_pktinfo),
		    IS2292(inp, IPV6_2292PKTINFO, IPV6_PKTINFO), IPPROTO_IPV6,
		    M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if ((inp->inp_flags & IN6P_HOPLIMIT) != 0) {
		int hlim;

		if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
#ifdef INET
			struct ip *ip;

			ip = mtod(m, struct ip *);
			hlim = ip->ip_ttl;
#else
			/* We won't hit this code */
			hlim = 0;
#endif
		} else {
			hlim = ip6->ip6_hlim & 0xff;
		}
		*mp = sbcreatecontrol(&hlim, sizeof(int),
		    IS2292(inp, IPV6_2292HOPLIMIT, IPV6_HOPLIMIT),
		    IPPROTO_IPV6, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if ((inp->inp_flags & IN6P_TCLASS) != 0) {
		int tclass;

		if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
#ifdef INET
			struct ip *ip;

			ip = mtod(m, struct ip *);
			tclass = ip->ip_tos;
#else
			/* We won't hit this code */
			tclass = 0;
#endif
		} else {
			u_int32_t flowinfo;

			flowinfo = (u_int32_t)ntohl(ip6->ip6_flow & IPV6_FLOWINFO_MASK);
			flowinfo >>= 20;
			tclass = flowinfo & 0xff;
		}
		*mp = sbcreatecontrol(&tclass, sizeof(int), IPV6_TCLASS,
		    IPPROTO_IPV6, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if (v4only != NULL) {
		if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
			*v4only = 1;
		} else {
			*v4only = 0;
		}
	}

	return (mp);
}

void
ip6_savecontrol(struct inpcb *inp, struct mbuf *m, struct mbuf **mp)
{
	struct ip6_hdr *ip6;
	int v4only = 0;

	mp = ip6_savecontrol_v4(inp, m, mp, &v4only);
	if (v4only)
		return;

	ip6 = mtod(m, struct ip6_hdr *);
	/*
	 * IPV6_HOPOPTS socket option.  Recall that we required super-user
	 * privilege for the option (see ip6_ctloutput), but it might be too
	 * strict, since there might be some hop-by-hop options which can be
	 * returned to normal user.
	 * See also RFC 2292 section 6 (or RFC 3542 section 8).
	 */
	if ((inp->inp_flags & IN6P_HOPOPTS) != 0) {
		/*
		 * Check if a hop-by-hop options header is contatined in the
		 * received packet, and if so, store the options as ancillary
		 * data. Note that a hop-by-hop options header must be
		 * just after the IPv6 header, which is assured through the
		 * IPv6 input processing.
		 */
		if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
			struct ip6_hbh *hbh;
			u_int hbhlen;

			hbh = (struct ip6_hbh *)(ip6 + 1);
			hbhlen = (hbh->ip6h_len + 1) << 3;

			/*
			 * XXX: We copy the whole header even if a
			 * jumbo payload option is included, the option which
			 * is to be removed before returning according to
			 * RFC2292.
			 * Note: this constraint is removed in RFC3542
			 */
			*mp = sbcreatecontrol(hbh, hbhlen,
			    IS2292(inp, IPV6_2292HOPOPTS, IPV6_HOPOPTS),
			    IPPROTO_IPV6, M_NOWAIT);
			if (*mp)
				mp = &(*mp)->m_next;
		}
	}

	if ((inp->inp_flags & (IN6P_RTHDR | IN6P_DSTOPTS)) != 0) {
		int nxt = ip6->ip6_nxt, off = sizeof(struct ip6_hdr);

		/*
		 * Search for destination options headers or routing
		 * header(s) through the header chain, and stores each
		 * header as ancillary data.
		 * Note that the order of the headers remains in
		 * the chain of ancillary data.
		 */
		while (1) {	/* is explicit loop prevention necessary? */
			struct ip6_ext *ip6e = NULL;
			u_int elen;

			/*
			 * if it is not an extension header, don't try to
			 * pull it from the chain.
			 */
			switch (nxt) {
			case IPPROTO_DSTOPTS:
			case IPPROTO_ROUTING:
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;
			default:
				goto loopend;
			}

			if (off + sizeof(*ip6e) > m->m_len)
				goto loopend;
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + off);
			if (nxt == IPPROTO_AH)
				elen = (ip6e->ip6e_len + 2) << 2;
			else
				elen = (ip6e->ip6e_len + 1) << 3;
			if (off + elen > m->m_len)
				goto loopend;

			switch (nxt) {
			case IPPROTO_DSTOPTS:
				if (!(inp->inp_flags & IN6P_DSTOPTS))
					break;

				*mp = sbcreatecontrol(ip6e, elen,
				    IS2292(inp, IPV6_2292DSTOPTS, IPV6_DSTOPTS),
				    IPPROTO_IPV6, M_NOWAIT);
				if (*mp)
					mp = &(*mp)->m_next;
				break;
			case IPPROTO_ROUTING:
				if (!(inp->inp_flags & IN6P_RTHDR))
					break;

				*mp = sbcreatecontrol(ip6e, elen,
				    IS2292(inp, IPV6_2292RTHDR, IPV6_RTHDR),
				    IPPROTO_IPV6, M_NOWAIT);
				if (*mp)
					mp = &(*mp)->m_next;
				break;
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;

			default:
				/*
				 * other cases have been filtered in the above.
				 * none will visit this case.  here we supply
				 * the code just in case (nxt overwritten or
				 * other cases).
				 */
				goto loopend;
			}

			/* proceed with the next header. */
			off += elen;
			nxt = ip6e->ip6e_nxt;
			ip6e = NULL;
		}
	  loopend:
		;
	}

	if (inp->inp_flags2 & INP_RECVFLOWID) {
		uint32_t flowid, flow_type;

		flowid = m->m_pkthdr.flowid;
		flow_type = M_HASHTYPE_GET(m);

		/*
		 * XXX should handle the failure of one or the
		 * other - don't populate both?
		 */
		*mp = sbcreatecontrol(&flowid, sizeof(uint32_t), IPV6_FLOWID,
		    IPPROTO_IPV6, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
		*mp = sbcreatecontrol(&flow_type, sizeof(uint32_t),
		    IPV6_FLOWTYPE, IPPROTO_IPV6, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}

#ifdef	RSS
	if (inp->inp_flags2 & INP_RECVRSSBUCKETID) {
		uint32_t flowid, flow_type;
		uint32_t rss_bucketid;

		flowid = m->m_pkthdr.flowid;
		flow_type = M_HASHTYPE_GET(m);

		if (rss_hash2bucket(flowid, flow_type, &rss_bucketid) == 0) {
			*mp = sbcreatecontrol(&rss_bucketid, sizeof(uint32_t),
			    IPV6_RSSBUCKETID, IPPROTO_IPV6, M_NOWAIT);
			if (*mp)
				mp = &(*mp)->m_next;
		}
	}
#endif

}
#undef IS2292

void
ip6_notify_pmtu(struct inpcb *inp, struct sockaddr_in6 *dst, u_int32_t mtu)
{
	struct socket *so;
	struct mbuf *m_mtu;
	struct ip6_mtuinfo mtuctl;

	KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
	/*
	 * Notify the error by sending IPV6_PATHMTU ancillary data if
	 * application wanted to know the MTU value.
	 * NOTE: we notify disconnected sockets, because some udp
	 * applications keep sending sockets disconnected.
	 * NOTE: our implementation doesn't notify connected sockets that has
	 * foreign address that is different than given destination addresses
	 * (this is permitted by RFC 3542).
	 */
	if ((inp->inp_flags & IN6P_MTU) == 0 || (
	    !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
	    !IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr, &dst->sin6_addr)))
		return;

	mtuctl.ip6m_mtu = mtu;
	mtuctl.ip6m_addr = *dst;
	if (sa6_recoverscope(&mtuctl.ip6m_addr))
		return;

	if ((m_mtu = sbcreatecontrol(&mtuctl, sizeof(mtuctl), IPV6_PATHMTU,
	    IPPROTO_IPV6, M_NOWAIT)) == NULL)
		return;

	so =  inp->inp_socket;
	if (sbappendaddr(&so->so_rcv, (struct sockaddr *)dst, NULL, m_mtu)
	    == 0) {
		soroverflow(so);
		m_freem(m_mtu);
		/* XXX: should count statistics */
	} else
		sorwakeup(so);
}

/*
 * Get pointer to the previous header followed by the header
 * currently processed.
 */
int
ip6_get_prevhdr(const struct mbuf *m, int off)
{
	struct ip6_ext ip6e;
	struct ip6_hdr *ip6;
	int len, nlen, nxt;

	if (off == sizeof(struct ip6_hdr))
		return (offsetof(struct ip6_hdr, ip6_nxt));
	if (off < sizeof(struct ip6_hdr))
		panic("%s: off < sizeof(struct ip6_hdr)", __func__);

	ip6 = mtod(m, struct ip6_hdr *);
	nxt = ip6->ip6_nxt;
	len = sizeof(struct ip6_hdr);
	nlen = 0;
	while (len < off) {
		m_copydata(m, len, sizeof(ip6e), (caddr_t)&ip6e);
		switch (nxt) {
		case IPPROTO_FRAGMENT:
			nlen = sizeof(struct ip6_frag);
			break;
		case IPPROTO_AH:
			nlen = (ip6e.ip6e_len + 2) << 2;
			break;
		default:
			nlen = (ip6e.ip6e_len + 1) << 3;
		}
		len += nlen;
		nxt = ip6e.ip6e_nxt;
	}
	return (len - nlen);
}

/*
 * get next header offset.  m will be retained.
 */
int
ip6_nexthdr(const struct mbuf *m, int off, int proto, int *nxtp)
{
	struct ip6_hdr ip6;
	struct ip6_ext ip6e;
	struct ip6_frag fh;

	/* just in case */
	if (m == NULL)
		panic("ip6_nexthdr: m == NULL");
	if ((m->m_flags & M_PKTHDR) == 0 || m->m_pkthdr.len < off)
		return -1;

	switch (proto) {
	case IPPROTO_IPV6:
		if (m->m_pkthdr.len < off + sizeof(ip6))
			return -1;
		m_copydata(m, off, sizeof(ip6), (caddr_t)&ip6);
		if (nxtp)
			*nxtp = ip6.ip6_nxt;
		off += sizeof(ip6);
		return off;

	case IPPROTO_FRAGMENT:
		/*
		 * terminate parsing if it is not the first fragment,
		 * it does not make sense to parse through it.
		 */
		if (m->m_pkthdr.len < off + sizeof(fh))
			return -1;
		m_copydata(m, off, sizeof(fh), (caddr_t)&fh);
		/* IP6F_OFF_MASK = 0xfff8(BigEndian), 0xf8ff(LittleEndian) */
		if (fh.ip6f_offlg & IP6F_OFF_MASK)
			return -1;
		if (nxtp)
			*nxtp = fh.ip6f_nxt;
		off += sizeof(struct ip6_frag);
		return off;

	case IPPROTO_AH:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 2) << 2;
		return off;

	case IPPROTO_HOPOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_DSTOPTS:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 1) << 3;
		return off;

	case IPPROTO_NONE:
	case IPPROTO_ESP:
	case IPPROTO_IPCOMP:
		/* give up */
		return -1;

	default:
		return -1;
	}

	/* NOTREACHED */
}

/*
 * get offset for the last header in the chain.  m will be kept untainted.
 */
int
ip6_lasthdr(const struct mbuf *m, int off, int proto, int *nxtp)
{
	int newoff;
	int nxt;

	if (!nxtp) {
		nxt = -1;
		nxtp = &nxt;
	}
	while (1) {
		newoff = ip6_nexthdr(m, off, proto, nxtp);
		if (newoff < 0)
			return off;
		else if (newoff < off)
			return -1;	/* invalid */
		else if (newoff == off)
			return newoff;

		off = newoff;
		proto = *nxtp;
	}
}
