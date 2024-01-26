/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 */

#include <sys/cdefs.h>
#include "opt_bootp.h"
#include "opt_inet.h"
#include "opt_ipstealth.h"
#include "opt_ipsec.h"
#include "opt_route.h"
#include "opt_rss.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/hhook.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/sdt.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_private.h>
#include <net/pfil.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/netisr.h>
#include <net/rss_config.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_fib.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_options.h>
#include <machine/in_cksum.h>
#include <netinet/ip_carp.h>
#include <netinet/in_rss.h>
#ifdef SCTP
#include <netinet/sctp_var.h>
#endif

#include <netipsec/ipsec_support.h>

#include <sys/socketvar.h>

#include <security/mac/mac_framework.h>

#ifdef CTASSERT
CTASSERT(sizeof(struct ip) == 20);
#endif

/* IP reassembly functions are defined in ip_reass.c. */
extern void ipreass_init(void);
extern void ipreass_vnet_init(void);
#ifdef VIMAGE
extern void ipreass_destroy(void);
#endif

VNET_DEFINE(int, rsvp_on);

VNET_DEFINE(int, ipforwarding);
SYSCTL_INT(_net_inet_ip, IPCTL_FORWARDING, forwarding, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(ipforwarding), 0,
    "Enable IP forwarding between interfaces");

/*
 * Respond with an ICMP host redirect when we forward a packet out of
 * the same interface on which it was received.  See RFC 792.
 */
VNET_DEFINE(int, ipsendredirects) = 1;
SYSCTL_INT(_net_inet_ip, IPCTL_SENDREDIRECTS, redirect, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(ipsendredirects), 0,
    "Enable sending IP redirects");

VNET_DEFINE_STATIC(bool, ip_strong_es) = false;
#define	V_ip_strong_es	VNET(ip_strong_es)
SYSCTL_BOOL(_net_inet_ip, OID_AUTO, rfc1122_strong_es,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip_strong_es), false,
    "Packet's IP destination address must match address on arrival interface");

VNET_DEFINE_STATIC(bool, ip_sav) = true;
#define	V_ip_sav	VNET(ip_sav)
SYSCTL_BOOL(_net_inet_ip, OID_AUTO, source_address_validation,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip_sav), true,
    "Drop incoming packets with source address that is a local address");

/* Packet filter hooks */
VNET_DEFINE(pfil_head_t, inet_pfil_head);
VNET_DEFINE(pfil_head_t, inet_local_pfil_head);

static struct netisr_handler ip_nh = {
	.nh_name = "ip",
	.nh_handler = ip_input,
	.nh_proto = NETISR_IP,
#ifdef	RSS
	.nh_m2cpuid = rss_soft_m2cpuid_v4,
	.nh_policy = NETISR_POLICY_CPU,
	.nh_dispatch = NETISR_DISPATCH_HYBRID,
#else
	.nh_policy = NETISR_POLICY_FLOW,
#endif
};

#ifdef	RSS
/*
 * Directly dispatched frames are currently assumed
 * to have a flowid already calculated.
 *
 * It should likely have something that assert it
 * actually has valid flow details.
 */
static struct netisr_handler ip_direct_nh = {
	.nh_name = "ip_direct",
	.nh_handler = ip_direct_input,
	.nh_proto = NETISR_IP_DIRECT,
	.nh_m2cpuid = rss_soft_m2cpuid_v4,
	.nh_policy = NETISR_POLICY_CPU,
	.nh_dispatch = NETISR_DISPATCH_HYBRID,
};
#endif

ipproto_input_t		*ip_protox[IPPROTO_MAX] = {
			    [0 ... IPPROTO_MAX - 1] = rip_input };
ipproto_ctlinput_t	*ip_ctlprotox[IPPROTO_MAX] = {
			    [0 ... IPPROTO_MAX - 1] = rip_ctlinput };

VNET_DEFINE(struct in_ifaddrhead, in_ifaddrhead);  /* first inet address */
VNET_DEFINE(struct in_ifaddrhashhead *, in_ifaddrhashtbl); /* inet addr hash table  */
VNET_DEFINE(u_long, in_ifaddrhmask);		/* mask for hash table */

/* Make sure it is safe to use hashinit(9) on CK_LIST. */
CTASSERT(sizeof(struct in_ifaddrhashhead) == sizeof(LIST_HEAD(, in_addr)));

#ifdef IPCTL_DEFMTU
SYSCTL_INT(_net_inet_ip, IPCTL_DEFMTU, mtu, CTLFLAG_RW,
    &ip_mtu, 0, "Default MTU");
#endif

#ifdef IPSTEALTH
VNET_DEFINE(int, ipstealth);
SYSCTL_INT(_net_inet_ip, OID_AUTO, stealth, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(ipstealth), 0,
    "IP stealth mode, no TTL decrementation on forwarding");
#endif

/*
 * IP statistics are stored in the "array" of counter(9)s.
 */
VNET_PCPUSTAT_DEFINE(struct ipstat, ipstat);
VNET_PCPUSTAT_SYSINIT(ipstat);
SYSCTL_VNET_PCPUSTAT(_net_inet_ip, IPCTL_STATS, stats, struct ipstat, ipstat,
    "IP statistics (struct ipstat, netinet/ip_var.h)");

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(ipstat);
#endif /* VIMAGE */

/*
 * Kernel module interface for updating ipstat.  The argument is an index
 * into ipstat treated as an array.
 */
void
kmod_ipstat_inc(int statnum)
{

	counter_u64_add(VNET(ipstat)[statnum], 1);
}

void
kmod_ipstat_dec(int statnum)
{

	counter_u64_add(VNET(ipstat)[statnum], -1);
}

static int
sysctl_netinet_intr_queue_maxlen(SYSCTL_HANDLER_ARGS)
{
	int error, qlimit;

	netisr_getqlimit(&ip_nh, &qlimit);
	error = sysctl_handle_int(oidp, &qlimit, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qlimit < 1)
		return (EINVAL);
	return (netisr_setqlimit(&ip_nh, qlimit));
}
SYSCTL_PROC(_net_inet_ip, IPCTL_INTRQMAXLEN, intr_queue_maxlen,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, 0, 0,
    sysctl_netinet_intr_queue_maxlen, "I",
    "Maximum size of the IP input queue");

static int
sysctl_netinet_intr_queue_drops(SYSCTL_HANDLER_ARGS)
{
	u_int64_t qdrops_long;
	int error, qdrops;

	netisr_getqdrops(&ip_nh, &qdrops_long);
	qdrops = qdrops_long;
	error = sysctl_handle_int(oidp, &qdrops, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qdrops != 0)
		return (EINVAL);
	netisr_clearqdrops(&ip_nh);
	return (0);
}

SYSCTL_PROC(_net_inet_ip, IPCTL_INTRQDROPS, intr_queue_drops,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
    0, 0, sysctl_netinet_intr_queue_drops, "I",
    "Number of packets dropped from the IP input queue");

#ifdef	RSS
static int
sysctl_netinet_intr_direct_queue_maxlen(SYSCTL_HANDLER_ARGS)
{
	int error, qlimit;

	netisr_getqlimit(&ip_direct_nh, &qlimit);
	error = sysctl_handle_int(oidp, &qlimit, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qlimit < 1)
		return (EINVAL);
	return (netisr_setqlimit(&ip_direct_nh, qlimit));
}
SYSCTL_PROC(_net_inet_ip, IPCTL_INTRDQMAXLEN, intr_direct_queue_maxlen,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_netinet_intr_direct_queue_maxlen,
    "I", "Maximum size of the IP direct input queue");

static int
sysctl_netinet_intr_direct_queue_drops(SYSCTL_HANDLER_ARGS)
{
	u_int64_t qdrops_long;
	int error, qdrops;

	netisr_getqdrops(&ip_direct_nh, &qdrops_long);
	qdrops = qdrops_long;
	error = sysctl_handle_int(oidp, &qdrops, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qdrops != 0)
		return (EINVAL);
	netisr_clearqdrops(&ip_direct_nh);
	return (0);
}

SYSCTL_PROC(_net_inet_ip, IPCTL_INTRDQDROPS, intr_direct_queue_drops,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_netinet_intr_direct_queue_drops, "I",
    "Number of packets dropped from the IP direct input queue");
#endif	/* RSS */

/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
static void
ip_vnet_init(void *arg __unused)
{
	struct pfil_head_args args;

	CK_STAILQ_INIT(&V_in_ifaddrhead);
	V_in_ifaddrhashtbl = hashinit(INADDR_NHASH, M_IFADDR, &V_in_ifaddrhmask);

	/* Initialize IP reassembly queue. */
	ipreass_vnet_init();

	/* Initialize packet filter hooks. */
	args.pa_version = PFIL_VERSION;
	args.pa_flags = PFIL_IN | PFIL_OUT;
	args.pa_type = PFIL_TYPE_IP4;
	args.pa_headname = PFIL_INET_NAME;
	V_inet_pfil_head = pfil_head_register(&args);

	args.pa_flags = PFIL_OUT;
	args.pa_headname = PFIL_INET_LOCAL_NAME;
	V_inet_local_pfil_head = pfil_head_register(&args);

	if (hhook_head_register(HHOOK_TYPE_IPSEC_IN, AF_INET,
	    &V_ipsec_hhh_in[HHOOK_IPSEC_INET],
	    HHOOK_WAITOK | HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register input helper hook\n",
		    __func__);
	if (hhook_head_register(HHOOK_TYPE_IPSEC_OUT, AF_INET,
	    &V_ipsec_hhh_out[HHOOK_IPSEC_INET],
	    HHOOK_WAITOK | HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register output helper hook\n",
		    __func__);

#ifdef VIMAGE
	netisr_register_vnet(&ip_nh);
#ifdef	RSS
	netisr_register_vnet(&ip_direct_nh);
#endif
#endif
}
VNET_SYSINIT(ip_vnet_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH,
    ip_vnet_init, NULL);

static void
ip_init(const void *unused __unused)
{

	ipreass_init();

	/*
	 * Register statically compiled protocols, that are unlikely to
	 * ever become dynamic.
	 */
	IPPROTO_REGISTER(IPPROTO_ICMP, icmp_input, NULL);
	IPPROTO_REGISTER(IPPROTO_IGMP, igmp_input, NULL);
	IPPROTO_REGISTER(IPPROTO_RSVP, rsvp_input, NULL);
	IPPROTO_REGISTER(IPPROTO_IPV4, encap4_input, NULL);
	IPPROTO_REGISTER(IPPROTO_MOBILE, encap4_input, NULL);
	IPPROTO_REGISTER(IPPROTO_ETHERIP, encap4_input, NULL);
	IPPROTO_REGISTER(IPPROTO_GRE, encap4_input, NULL);
	IPPROTO_REGISTER(IPPROTO_IPV6, encap4_input, NULL);
	IPPROTO_REGISTER(IPPROTO_PIM, encap4_input, NULL);
#ifdef SCTP	/* XXX: has a loadable & static version */
	IPPROTO_REGISTER(IPPROTO_SCTP, sctp_input, sctp_ctlinput);
#endif

	netisr_register(&ip_nh);
#ifdef	RSS
	netisr_register(&ip_direct_nh);
#endif
}
SYSINIT(ip_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, ip_init, NULL);

#ifdef VIMAGE
static void
ip_destroy(void *unused __unused)
{
	int error;

#ifdef	RSS
	netisr_unregister_vnet(&ip_direct_nh);
#endif
	netisr_unregister_vnet(&ip_nh);

	pfil_head_unregister(V_inet_pfil_head);
	error = hhook_head_deregister(V_ipsec_hhh_in[HHOOK_IPSEC_INET]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister input helper hook "
		    "type HHOOK_TYPE_IPSEC_IN, id HHOOK_IPSEC_INET: "
		    "error %d returned\n", __func__, error);
	}
	error = hhook_head_deregister(V_ipsec_hhh_out[HHOOK_IPSEC_INET]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister output helper hook "
		    "type HHOOK_TYPE_IPSEC_OUT, id HHOOK_IPSEC_INET: "
		    "error %d returned\n", __func__, error);
	}

	/* Remove the IPv4 addresses from all interfaces. */
	in_ifscrub_all();

	/* Make sure the IPv4 routes are gone as well. */
	rib_flush_routes_family(AF_INET);

	/* Destroy IP reassembly queue. */
	ipreass_destroy();

	/* Cleanup in_ifaddr hash table; should be empty. */
	hashdestroy(V_in_ifaddrhashtbl, M_IFADDR, V_in_ifaddrhmask);
}

VNET_SYSUNINIT(ip, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, ip_destroy, NULL);
#endif

#ifdef	RSS
/*
 * IP direct input routine.
 *
 * This is called when reinjecting completed fragments where
 * all of the previous checking and book-keeping has been done.
 */
void
ip_direct_input(struct mbuf *m)
{
	struct ip *ip;
	int hlen;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	if (IPSEC_ENABLED(ipv4)) {
		if (IPSEC_INPUT(ipv4, m, hlen, ip->ip_p) != 0)
			return;
	}
#endif /* IPSEC */
	IPSTAT_INC(ips_delivered);
	ip_protox[ip->ip_p](&m, &hlen, ip->ip_p);
}
#endif

/*
 * Ip input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  Process options.  Pass to next level.
 */
void
ip_input(struct mbuf *m)
{
	struct ip *ip = NULL;
	struct in_ifaddr *ia = NULL;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	int hlen = 0;
	uint16_t sum, ip_len;
	int dchg = 0;				/* dest changed after fw */
	struct in_addr odst;			/* original dst address */
	bool strong_es;

	M_ASSERTPKTHDR(m);
	NET_EPOCH_ASSERT();

	if (m->m_flags & M_FASTFWD_OURS) {
		m->m_flags &= ~M_FASTFWD_OURS;
		/* Set up some basics that will be used later. */
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		ip_len = ntohs(ip->ip_len);
		goto ours;
	}

	IPSTAT_INC(ips_total);

	if (__predict_false(m->m_pkthdr.len < sizeof(struct ip)))
		goto tooshort;

	if (m->m_len < sizeof(struct ip)) {
		m = m_pullup(m, sizeof(struct ip));
		if (__predict_false(m == NULL)) {
			IPSTAT_INC(ips_toosmall);
			return;
		}
	}
	ip = mtod(m, struct ip *);

	if (__predict_false(ip->ip_v != IPVERSION)) {
		IPSTAT_INC(ips_badvers);
		goto bad;
	}

	hlen = ip->ip_hl << 2;
	if (__predict_false(hlen < sizeof(struct ip))) {	/* minimum header length */
		IPSTAT_INC(ips_badhlen);
		goto bad;
	}
	if (hlen > m->m_len) {
		m = m_pullup(m, hlen);
		if (__predict_false(m == NULL)) {
			IPSTAT_INC(ips_badhlen);
			return;
		}
		ip = mtod(m, struct ip *);
	}

	IP_PROBE(receive, NULL, NULL, ip, m->m_pkthdr.rcvif, ip, NULL);

	/* IN_LOOPBACK must not appear on the wire - RFC1122 */
	ifp = m->m_pkthdr.rcvif;
	if (IN_LOOPBACK(ntohl(ip->ip_dst.s_addr)) ||
	    IN_LOOPBACK(ntohl(ip->ip_src.s_addr))) {
		if ((ifp->if_flags & IFF_LOOPBACK) == 0) {
			IPSTAT_INC(ips_badaddr);
			goto bad;
		}
	}
	/* The unspecified address can appear only as a src address - RFC1122 */
	if (__predict_false(ntohl(ip->ip_dst.s_addr) == INADDR_ANY)) {
		IPSTAT_INC(ips_badaddr);
		goto bad;
	}

	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) {
		sum = !(m->m_pkthdr.csum_flags & CSUM_IP_VALID);
	} else {
		if (hlen == sizeof(struct ip)) {
			sum = in_cksum_hdr(ip);
		} else {
			sum = in_cksum(m, hlen);
		}
	}
	if (__predict_false(sum)) {
		IPSTAT_INC(ips_badsum);
		goto bad;
	}

	ip_len = ntohs(ip->ip_len);
	if (__predict_false(ip_len < hlen)) {
		IPSTAT_INC(ips_badlen);
		goto bad;
	}

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (__predict_false(m->m_pkthdr.len < ip_len)) {
tooshort:
		IPSTAT_INC(ips_tooshort);
		goto bad;
	}
	if (m->m_pkthdr.len > ip_len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip_len;
			m->m_pkthdr.len = ip_len;
		} else
			m_adj(m, ip_len - m->m_pkthdr.len);
	}

	/*
	 * Try to forward the packet, but if we fail continue.
	 * ip_tryforward() may generate redirects these days.
	 * XXX the logic below falling through to normal processing
	 * if redirects are required should be revisited as well.
	 * ip_tryforward() does inbound and outbound packet firewall
	 * processing. If firewall has decided that destination becomes
	 * our local address, it sets M_FASTFWD_OURS flag. In this
	 * case skip another inbound firewall processing and update
	 * ip pointer.
	 */
	if (V_ipforwarding != 0
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	    && (!IPSEC_ENABLED(ipv4) ||
	    IPSEC_CAPS(ipv4, m, IPSEC_CAP_OPERABLE) == 0)
#endif
	    ) {
		/*
		 * ip_dooptions() was run so we can ignore the source route (or
		 * any IP options case) case for redirects in ip_tryforward().
		 */
		if ((m = ip_tryforward(m)) == NULL)
			return;
		if (m->m_flags & M_FASTFWD_OURS) {
			m->m_flags &= ~M_FASTFWD_OURS;
			ip = mtod(m, struct ip *);
			goto ours;
		}
	}

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/*
	 * Bypass packet filtering for packets previously handled by IPsec.
	 */
	if (IPSEC_ENABLED(ipv4) &&
	    IPSEC_CAPS(ipv4, m, IPSEC_CAP_BYPASS_FILTER) != 0)
			goto passin;
#endif

	/*
	 * Run through list of hooks for input packets.
	 *
	 * NB: Beware of the destination address changing (e.g.
	 *     by NAT rewriting).  When this happens, tell
	 *     ip_forward to do the right thing.
	 */

	/* Jump over all PFIL processing if hooks are not active. */
	if (!PFIL_HOOKED_IN(V_inet_pfil_head))
		goto passin;

	odst = ip->ip_dst;
	if (pfil_mbuf_in(V_inet_pfil_head, &m, ifp, NULL) !=
	    PFIL_PASS)
		return;

	ip = mtod(m, struct ip *);
	dchg = (odst.s_addr != ip->ip_dst.s_addr);

	if (m->m_flags & M_FASTFWD_OURS) {
		m->m_flags &= ~M_FASTFWD_OURS;
		goto ours;
	}
	if (m->m_flags & M_IP_NEXTHOP) {
		if (m_tag_find(m, PACKET_TAG_IPFORWARD, NULL) != NULL) {
			/*
			 * Directly ship the packet on.  This allows
			 * forwarding packets originally destined to us
			 * to some other directly connected host.
			 */
			ip_forward(m, 1);
			return;
		}
	}
passin:

	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	if (hlen > sizeof (struct ip) && ip_dooptions(m, 0))
		return;

        /* greedy RSVP, snatches any PATH packet of the RSVP protocol and no
         * matter if it is destined to another node, or whether it is
         * a multicast one, RSVP wants it! and prevents it from being forwarded
         * anywhere else. Also checks if the rsvp daemon is running before
	 * grabbing the packet.
         */
	if (ip->ip_p == IPPROTO_RSVP && V_rsvp_on)
		goto ours;

	/*
	 * Check our list of addresses, to see if the packet is for us.
	 * If we don't have any addresses, assume any unicast packet
	 * we receive might be for us (and let the upper layers deal
	 * with it).
	 */
	if (CK_STAILQ_EMPTY(&V_in_ifaddrhead) &&
	    (m->m_flags & (M_MCAST|M_BCAST)) == 0)
		goto ours;

	/*
	 * Enable a consistency check between the destination address
	 * and the arrival interface for a unicast packet (the RFC 1122
	 * strong ES model) with a list of additional predicates:
	 * - if IP forwarding is disabled
	 * - the packet is not locally generated
	 * - the packet is not subject to 'ipfw fwd'
	 * - Interface is not running CARP. If the packet got here, we already
	 *   checked it with carp_iamatch() and carp_forus().
	 */
	strong_es = V_ip_strong_es && (V_ipforwarding == 0) &&
	    ((ifp->if_flags & IFF_LOOPBACK) == 0) &&
	    ifp->if_carp == NULL && (dchg == 0);

	/*
	 * Check for exact addresses in the hash bucket.
	 */
	CK_LIST_FOREACH(ia, INADDR_HASH(ip->ip_dst.s_addr), ia_hash) {
		if (IA_SIN(ia)->sin_addr.s_addr != ip->ip_dst.s_addr)
			continue;

		/*
		 * net.inet.ip.rfc1122_strong_es: the address matches, verify
		 * that the packet arrived via the correct interface.
		 */
		if (__predict_false(strong_es && ia->ia_ifp != ifp)) {
			IPSTAT_INC(ips_badaddr);
			goto bad;
		}

		/*
		 * net.inet.ip.source_address_validation: drop incoming
		 * packets that pretend to be ours.
		 */
		if (V_ip_sav && !(ifp->if_flags & IFF_LOOPBACK) &&
		    __predict_false(in_localip_fib(ip->ip_src, ifp->if_fib))) {
			IPSTAT_INC(ips_badaddr);
			goto bad;
		}

		counter_u64_add(ia->ia_ifa.ifa_ipackets, 1);
		counter_u64_add(ia->ia_ifa.ifa_ibytes, m->m_pkthdr.len);
		goto ours;
	}

	/*
	 * Check for broadcast addresses.
	 *
	 * Only accept broadcast packets that arrive via the matching
	 * interface.  Reception of forwarded directed broadcasts would
	 * be handled via ip_forward() and ether_output() with the loopback
	 * into the stack for SIMPLEX interfaces handled by ether_output().
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			ia = ifatoia(ifa);
			if (satosin(&ia->ia_broadaddr)->sin_addr.s_addr ==
			    ip->ip_dst.s_addr) {
				counter_u64_add(ia->ia_ifa.ifa_ipackets, 1);
				counter_u64_add(ia->ia_ifa.ifa_ibytes,
				    m->m_pkthdr.len);
				goto ours;
			}
#ifdef BOOTP_COMPAT
			if (IA_SIN(ia)->sin_addr.s_addr == INADDR_ANY) {
				counter_u64_add(ia->ia_ifa.ifa_ipackets, 1);
				counter_u64_add(ia->ia_ifa.ifa_ibytes,
				    m->m_pkthdr.len);
				goto ours;
			}
#endif
		}
		ia = NULL;
	}
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		/*
		 * RFC 3927 2.7: Do not forward multicast packets from
		 * IN_LINKLOCAL.
		 */
		if (V_ip_mrouter && !IN_LINKLOCAL(ntohl(ip->ip_src.s_addr))) {
			/*
			 * If we are acting as a multicast router, all
			 * incoming multicast packets are passed to the
			 * kernel-level multicast forwarding function.
			 * The packet is returned (relatively) intact; if
			 * ip_mforward() returns a non-zero value, the packet
			 * must be discarded, else it may be accepted below.
			 */
			if (ip_mforward && ip_mforward(ip, ifp, m, 0) != 0) {
				IPSTAT_INC(ips_cantforward);
				m_freem(m);
				return;
			}

			/*
			 * The process-level routing daemon needs to receive
			 * all multicast IGMP packets, whether or not this
			 * host belongs to their destination groups.
			 */
			if (ip->ip_p == IPPROTO_IGMP) {
				goto ours;
			}
			IPSTAT_INC(ips_forward);
		}
		/*
		 * Assume the packet is for us, to avoid prematurely taking
		 * a lock on the in_multi hash. Protocols must perform
		 * their own filtering and update statistics accordingly.
		 */
		goto ours;
	}
	if (ip->ip_dst.s_addr == (u_long)INADDR_BROADCAST)
		goto ours;
	if (ip->ip_dst.s_addr == INADDR_ANY)
		goto ours;
	/* RFC 3927 2.7: Do not forward packets to or from IN_LINKLOCAL. */
	if (IN_LINKLOCAL(ntohl(ip->ip_dst.s_addr)) ||
	    IN_LINKLOCAL(ntohl(ip->ip_src.s_addr))) {
		IPSTAT_INC(ips_cantforward);
		m_freem(m);
		return;
	}

	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (V_ipforwarding == 0) {
		IPSTAT_INC(ips_cantforward);
		m_freem(m);
	} else {
		ip_forward(m, dchg);
	}
	return;

ours:
#ifdef IPSTEALTH
	/*
	 * IPSTEALTH: Process non-routing options only
	 * if the packet is destined for us.
	 */
	if (V_ipstealth && hlen > sizeof (struct ip) && ip_dooptions(m, 1))
		return;
#endif /* IPSTEALTH */

	/*
	 * We are going to ship the packet to the local protocol stack. Call the
	 * filter again for this 'output' action, allowing redirect-like rules
	 * to adjust the source address.
	 */
	if (PFIL_HOOKED_OUT(V_inet_local_pfil_head)) {
		if (pfil_mbuf_out(V_inet_local_pfil_head, &m, V_loif, NULL) !=
		    PFIL_PASS)
			return;
		ip = mtod(m, struct ip *);
	}

	/*
	 * Attempt reassembly; if it succeeds, proceed.
	 * ip_reass() will return a different mbuf.
	 */
	if (ip->ip_off & htons(IP_MF | IP_OFFMASK)) {
		/* XXXGL: shouldn't we save & set m_flags? */
		m = ip_reass(m);
		if (m == NULL)
			return;
		ip = mtod(m, struct ip *);
		/* Get the header length of the reassembled packet */
		hlen = ip->ip_hl << 2;
	}

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	if (IPSEC_ENABLED(ipv4)) {
		if (IPSEC_INPUT(ipv4, m, hlen, ip->ip_p) != 0)
			return;
	}
#endif /* IPSEC */

	/*
	 * Switch out to protocol's input routine.
	 */
	IPSTAT_INC(ips_delivered);

	ip_protox[ip->ip_p](&m, &hlen, ip->ip_p);
	return;
bad:
	m_freem(m);
}

int
ipproto_register(uint8_t proto, ipproto_input_t input, ipproto_ctlinput_t ctl)
{

	MPASS(proto > 0);

	/*
	 * The protocol slot must not be occupied by another protocol
	 * already.  An index pointing to rip_input() is unused.
	 */
	if (ip_protox[proto] == rip_input) {
		ip_protox[proto] = input;
		ip_ctlprotox[proto] = ctl;
		return (0);
	} else
		return (EEXIST);
}

int
ipproto_unregister(uint8_t proto)
{

	MPASS(proto > 0);

	if (ip_protox[proto] != rip_input) {
		ip_protox[proto] = rip_input;
		ip_ctlprotox[proto] = rip_ctlinput;
		return (0);
	} else
		return (ENOENT);
}

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
void
ip_forward(struct mbuf *m, int srcrt)
{
	struct ip *ip = mtod(m, struct ip *);
	struct in_ifaddr *ia;
	struct mbuf *mcopy;
	struct sockaddr_in *sin;
	struct in_addr dest;
	struct route ro;
	uint32_t flowid;
	int error, type = 0, code = 0, mtu = 0;

	NET_EPOCH_ASSERT();

	if (m->m_flags & (M_BCAST|M_MCAST) || in_canforward(ip->ip_dst) == 0) {
		IPSTAT_INC(ips_cantforward);
		m_freem(m);
		return;
	}
	if (
#ifdef IPSTEALTH
	    V_ipstealth == 0 &&
#endif
	    ip->ip_ttl <= IPTTLDEC) {
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, 0, 0);
		return;
	}

	bzero(&ro, sizeof(ro));
	sin = (struct sockaddr_in *)&ro.ro_dst;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = ip->ip_dst;
	flowid = m->m_pkthdr.flowid;
	ro.ro_nh = fib4_lookup(M_GETFIB(m), ip->ip_dst, 0, NHR_REF, flowid);
	if (ro.ro_nh != NULL) {
		ia = ifatoia(ro.ro_nh->nh_ifa);
	} else
		ia = NULL;
	/*
	 * Save the IP header and at most 8 bytes of the payload,
	 * in case we need to generate an ICMP message to the src.
	 *
	 * XXX this can be optimized a lot by saving the data in a local
	 * buffer on the stack (72 bytes at most), and only allocating the
	 * mbuf if really necessary. The vast majority of the packets
	 * are forwarded without having to send an ICMP back (either
	 * because unnecessary, or because rate limited), so we are
	 * really we are wasting a lot of work here.
	 *
	 * We don't use m_copym() because it might return a reference
	 * to a shared cluster. Both this function and ip_output()
	 * assume exclusive access to the IP header in `m', so any
	 * data in a cluster may change before we reach icmp_error().
	 */
	mcopy = m_gethdr(M_NOWAIT, m->m_type);
	if (mcopy != NULL && !m_dup_pkthdr(mcopy, m, M_NOWAIT)) {
		/*
		 * It's probably ok if the pkthdr dup fails (because
		 * the deep copy of the tag chain failed), but for now
		 * be conservative and just discard the copy since
		 * code below may some day want the tags.
		 */
		m_free(mcopy);
		mcopy = NULL;
	}
	if (mcopy != NULL) {
		mcopy->m_len = min(ntohs(ip->ip_len), M_TRAILINGSPACE(mcopy));
		mcopy->m_pkthdr.len = mcopy->m_len;
		m_copydata(m, 0, mcopy->m_len, mtod(mcopy, caddr_t));
	}
#ifdef IPSTEALTH
	if (V_ipstealth == 0)
#endif
		ip->ip_ttl -= IPTTLDEC;
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	if (IPSEC_ENABLED(ipv4)) {
		if ((error = IPSEC_FORWARD(ipv4, m)) != 0) {
			/* mbuf consumed by IPsec */
			RO_NHFREE(&ro);
			m_freem(mcopy);
			if (error != EINPROGRESS)
				IPSTAT_INC(ips_cantforward);
			return;
		}
		/* No IPsec processing required */
	}
#endif /* IPSEC */
	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modified by a redirect.
	 */
	dest.s_addr = 0;
	if (!srcrt && V_ipsendredirects &&
	    ia != NULL && ia->ia_ifp == m->m_pkthdr.rcvif) {
		struct nhop_object *nh;

		nh = ro.ro_nh;

		if (nh != NULL && ((nh->nh_flags & (NHF_REDIRECT|NHF_DEFAULT)) == 0)) {
			struct in_ifaddr *nh_ia = (struct in_ifaddr *)(nh->nh_ifa);
			u_long src = ntohl(ip->ip_src.s_addr);

			if (nh_ia != NULL &&
			    (src & nh_ia->ia_subnetmask) == nh_ia->ia_subnet) {
				/* Router requirements says to only send host redirects */
				type = ICMP_REDIRECT;
				code = ICMP_REDIRECT_HOST;
				if (nh->nh_flags & NHF_GATEWAY) {
				    if (nh->gw_sa.sa_family == AF_INET)
					dest.s_addr = nh->gw4_sa.sin_addr.s_addr;
				    else /* Do not redirect in case gw is AF_INET6 */
					type = 0;
				} else
					dest.s_addr = ip->ip_dst.s_addr;
			}
		}
	}

	error = ip_output(m, NULL, &ro, IP_FORWARDING, NULL, NULL);

	if (error == EMSGSIZE && ro.ro_nh)
		mtu = ro.ro_nh->nh_mtu;
	RO_NHFREE(&ro);

	if (error)
		IPSTAT_INC(ips_cantforward);
	else {
		IPSTAT_INC(ips_forward);
		if (type)
			IPSTAT_INC(ips_redirectsent);
		else {
			if (mcopy)
				m_freem(mcopy);
			return;
		}
	}
	if (mcopy == NULL)
		return;

	switch (error) {
	case 0:				/* forwarded, but need redirect */
		/* type, code set above */
		break;

	case ENETUNREACH:
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;

	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;
		/*
		 * If the MTU was set before make sure we are below the
		 * interface MTU.
		 * If the MTU wasn't set before use the interface mtu or
		 * fall back to the next smaller mtu step compared to the
		 * current packet size.
		 */
		if (mtu != 0) {
			if (ia != NULL)
				mtu = min(mtu, ia->ia_ifp->if_mtu);
		} else {
			if (ia != NULL)
				mtu = ia->ia_ifp->if_mtu;
			else
				mtu = ip_next_mtu(ntohs(ip->ip_len), 0);
		}
		IPSTAT_INC(ips_cantfrag);
		break;

	case ENOBUFS:
	case EACCES:			/* ipfw denied packet */
		m_freem(mcopy);
		return;
	}
	icmp_error(mcopy, type, code, dest.s_addr, mtu);
}

#define	CHECK_SO_CT(sp, ct) \
    (((sp->so_options & SO_TIMESTAMP) && (sp->so_ts_clock == ct)) ? 1 : 0)

void
ip_savecontrol(struct inpcb *inp, struct mbuf **mp, struct ip *ip,
    struct mbuf *m)
{
	bool stamped;

	stamped = false;
	if ((inp->inp_socket->so_options & SO_BINTIME) ||
	    CHECK_SO_CT(inp->inp_socket, SO_TS_BINTIME)) {
		struct bintime boottimebin, bt;
		struct timespec ts1;

		if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
		    M_TSTMP)) {
			mbuf_tstmp2timespec(m, &ts1);
			timespec2bintime(&ts1, &bt);
			getboottimebin(&boottimebin);
			bintime_add(&bt, &boottimebin);
		} else {
			bintime(&bt);
		}
		*mp = sbcreatecontrol(&bt, sizeof(bt), SCM_BINTIME,
		    SOL_SOCKET, M_NOWAIT);
		if (*mp != NULL) {
			mp = &(*mp)->m_next;
			stamped = true;
		}
	}
	if (CHECK_SO_CT(inp->inp_socket, SO_TS_REALTIME_MICRO)) {
		struct bintime boottimebin, bt1;
		struct timespec ts1;
		struct timeval tv;

		if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
		    M_TSTMP)) {
			mbuf_tstmp2timespec(m, &ts1);
			timespec2bintime(&ts1, &bt1);
			getboottimebin(&boottimebin);
			bintime_add(&bt1, &boottimebin);
			bintime2timeval(&bt1, &tv);
		} else {
			microtime(&tv);
		}
		*mp = sbcreatecontrol((caddr_t)&tv, sizeof(tv), SCM_TIMESTAMP,
		    SOL_SOCKET, M_NOWAIT);
		if (*mp != NULL) {
			mp = &(*mp)->m_next;
			stamped = true;
		}
	} else if (CHECK_SO_CT(inp->inp_socket, SO_TS_REALTIME)) {
		struct bintime boottimebin;
		struct timespec ts, ts1;

		if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
		    M_TSTMP)) {
			mbuf_tstmp2timespec(m, &ts);
			getboottimebin(&boottimebin);
			bintime2timespec(&boottimebin, &ts1);
			timespecadd(&ts, &ts1, &ts);
		} else {
			nanotime(&ts);
		}
		*mp = sbcreatecontrol(&ts, sizeof(ts), SCM_REALTIME,
		    SOL_SOCKET, M_NOWAIT);
		if (*mp != NULL) {
			mp = &(*mp)->m_next;
			stamped = true;
		}
	} else if (CHECK_SO_CT(inp->inp_socket, SO_TS_MONOTONIC)) {
		struct timespec ts;

		if ((m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
		    M_TSTMP))
			mbuf_tstmp2timespec(m, &ts);
		else
			nanouptime(&ts);
		*mp = sbcreatecontrol(&ts, sizeof(ts), SCM_MONOTONIC,
		    SOL_SOCKET, M_NOWAIT);
		if (*mp != NULL) {
			mp = &(*mp)->m_next;
			stamped = true;
		}
	}
	if (stamped && (m->m_flags & (M_PKTHDR | M_TSTMP)) == (M_PKTHDR |
	    M_TSTMP)) {
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
	if (inp->inp_flags & INP_RECVDSTADDR) {
		*mp = sbcreatecontrol(&ip->ip_dst, sizeof(struct in_addr),
		    IP_RECVDSTADDR, IPPROTO_IP, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inp->inp_flags & INP_RECVTTL) {
		*mp = sbcreatecontrol(&ip->ip_ttl, sizeof(u_char), IP_RECVTTL,
		    IPPROTO_IP, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#ifdef notyet
	/* XXX
	 * Moving these out of udp_input() made them even more broken
	 * than they already were.
	 */
	/* options were tossed already */
	if (inp->inp_flags & INP_RECVOPTS) {
		*mp = sbcreatecontrol(opts_deleted_above,
		    sizeof(struct in_addr), IP_RECVOPTS, IPPROTO_IP, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	/* ip_srcroute doesn't do what we want here, need to fix */
	if (inp->inp_flags & INP_RECVRETOPTS) {
		*mp = sbcreatecontrol(ip_srcroute(m), sizeof(struct in_addr),
		    IP_RECVRETOPTS, IPPROTO_IP, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#endif
	if (inp->inp_flags & INP_RECVIF) {
		struct ifnet *ifp;
		struct sdlbuf {
			struct sockaddr_dl sdl;
			u_char	pad[32];
		} sdlbuf;
		struct sockaddr_dl *sdp;
		struct sockaddr_dl *sdl2 = &sdlbuf.sdl;

		if ((ifp = m->m_pkthdr.rcvif)) {
			sdp = (struct sockaddr_dl *)ifp->if_addr->ifa_addr;
			/*
			 * Change our mind and don't try copy.
			 */
			if (sdp->sdl_family != AF_LINK ||
			    sdp->sdl_len > sizeof(sdlbuf)) {
				goto makedummy;
			}
			bcopy(sdp, sdl2, sdp->sdl_len);
		} else {
makedummy:
			sdl2->sdl_len =
			    offsetof(struct sockaddr_dl, sdl_data[0]);
			sdl2->sdl_family = AF_LINK;
			sdl2->sdl_index = 0;
			sdl2->sdl_nlen = sdl2->sdl_alen = sdl2->sdl_slen = 0;
		}
		*mp = sbcreatecontrol(sdl2, sdl2->sdl_len, IP_RECVIF,
		    IPPROTO_IP, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inp->inp_flags & INP_RECVTOS) {
		*mp = sbcreatecontrol(&ip->ip_tos, sizeof(u_char), IP_RECVTOS,
		    IPPROTO_IP, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if (inp->inp_flags2 & INP_RECVFLOWID) {
		uint32_t flowid, flow_type;

		flowid = m->m_pkthdr.flowid;
		flow_type = M_HASHTYPE_GET(m);

		/*
		 * XXX should handle the failure of one or the
		 * other - don't populate both?
		 */
		*mp = sbcreatecontrol(&flowid, sizeof(uint32_t), IP_FLOWID,
		    IPPROTO_IP, M_NOWAIT);
		if (*mp)
			mp = &(*mp)->m_next;
		*mp = sbcreatecontrol(&flow_type, sizeof(uint32_t),
		    IP_FLOWTYPE, IPPROTO_IP, M_NOWAIT);
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
			    IP_RSSBUCKETID, IPPROTO_IP, M_NOWAIT);
			if (*mp)
				mp = &(*mp)->m_next;
		}
	}
#endif
}

/*
 * XXXRW: Multicast routing code in ip_mroute.c is generally MPSAFE, but the
 * ip_rsvp and ip_rsvp_on variables need to be interlocked with rsvp_on
 * locking.  This code remains in ip_input.c as ip_mroute.c is optionally
 * compiled.
 */
VNET_DEFINE_STATIC(int, ip_rsvp_on);
VNET_DEFINE(struct socket *, ip_rsvpd);

#define	V_ip_rsvp_on		VNET(ip_rsvp_on)

int
ip_rsvp_init(struct socket *so)
{

	if (V_ip_rsvpd != NULL)
		return EADDRINUSE;

	V_ip_rsvpd = so;
	/*
	 * This may seem silly, but we need to be sure we don't over-increment
	 * the RSVP counter, in case something slips up.
	 */
	if (!V_ip_rsvp_on) {
		V_ip_rsvp_on = 1;
		V_rsvp_on++;
	}

	return 0;
}

int
ip_rsvp_done(void)
{

	V_ip_rsvpd = NULL;
	/*
	 * This may seem silly, but we need to be sure we don't over-decrement
	 * the RSVP counter, in case something slips up.
	 */
	if (V_ip_rsvp_on) {
		V_ip_rsvp_on = 0;
		V_rsvp_on--;
	}
	return 0;
}

int
rsvp_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m;

	m = *mp;
	*mp = NULL;

	if (rsvp_input_p) { /* call the real one if loaded */
		*mp = m;
		rsvp_input_p(mp, offp, proto);
		return (IPPROTO_DONE);
	}

	/* Can still get packets with rsvp_on = 0 if there is a local member
	 * of the group to which the RSVP packet is addressed.  But in this
	 * case we want to throw the packet away.
	 */

	if (!V_rsvp_on) {
		m_freem(m);
		return (IPPROTO_DONE);
	}

	if (V_ip_rsvpd != NULL) {
		*mp = m;
		rip_input(mp, offp, proto);
		return (IPPROTO_DONE);
	}
	/* Drop the packet */
	m_freem(m);
	return (IPPROTO_DONE);
}
