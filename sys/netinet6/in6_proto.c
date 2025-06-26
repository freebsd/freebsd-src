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
 *	$KAME: in6_proto.c,v 1.91 2001/05/27 13:28:35 itojun Exp $
 */

/*-
 * Copyright (c) 1982, 1986, 1993
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_ipstealth.h"
#include "opt_sctp.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/raw_ip6.h>

/* netinet6/raw_ip6.c */
extern struct protosw rip6_protosw;
/* netinet6/udp6_usrreq.c */
extern struct protosw udp6_protosw, udplite6_protosw;
/* netinet/tcp_usrreq.c */
extern struct protosw tcp6_protosw;
/* netinet/sctp6_usrreq.c */
extern struct protosw sctp6_seqpacket_protosw, sctp6_stream_protosw;

/*
 * TCP/IP protocol family: IP6, ICMP6, UDP, TCP.
 */
FEATURE(inet6, "Internet Protocol version 6");

struct domain inet6domain = {
	.dom_family =		AF_INET6,
	.dom_name =		"internet6",
	.dom_rtattach =		in6_inithead,
#ifdef VIMAGE
	.dom_rtdetach =		in6_detachhead,
#endif
	.dom_ifattach =		in6_domifattach,
	.dom_ifdetach =		in6_domifdetach,
	.dom_ifmtu    =		in6_domifmtu,
	.dom_nprotosw =		14,
	.dom_protosw = {
		&tcp6_protosw,
		&udp6_protosw,
#ifdef SCTP
		&sctp6_seqpacket_protosw,
		&sctp6_stream_protosw,
#else
		NULL, NULL,
#endif
		&udplite6_protosw,
		&rip6_protosw,
		/* Spacer 8 times for loadable protocols. XXXGL: why 8? */
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
};

DOMAIN_SET(inet6);

/*
 * Internet configuration info
 */
#ifndef	IPV6FORWARDING
#ifdef GATEWAY6
#define	IPV6FORWARDING	1	/* forward IP6 packets not for us */
#else
#define	IPV6FORWARDING	0	/* don't forward IP6 packets not for us */
#endif /* GATEWAY6 */
#endif /* !IPV6FORWARDING */

#ifndef	IPV6_SENDREDIRECTS
#define	IPV6_SENDREDIRECTS	1
#endif

VNET_DEFINE(int, ip6_forwarding) = IPV6FORWARDING;	/* act as router? */
VNET_DEFINE(int, ip6_sendredirects) = IPV6_SENDREDIRECTS;
VNET_DEFINE(int, ip6_defhlim) = IPV6_DEFHLIM;
VNET_DEFINE(int, ip6_defmcasthlim) = IPV6_DEFAULT_MULTICAST_HOPS;
VNET_DEFINE(int, ip6_accept_rtadv) = 0;
VNET_DEFINE(int, ip6_no_radr) = 0;
VNET_DEFINE(int, ip6_norbit_raif) = 0;
VNET_DEFINE(int, ip6_rfc6204w3) = 0;
VNET_DEFINE(int, ip6_hdrnestlimit) = 15;/* How many header options will we
					 * process? */
VNET_DEFINE(int, ip6_dad_count) = 1;	/* DupAddrDetectionTransmits */
VNET_DEFINE(int, ip6_auto_flowlabel) = 1;
VNET_DEFINE(int, ip6_use_deprecated) = 1;/* allow deprecated addr
					 * (RFC2462 5.5.4) */
VNET_DEFINE(int, ip6_rr_prune) = 5;	/* router renumbering prefix
					 * walk list every 5 sec. */
VNET_DEFINE(int, ip6_mcast_pmtu) = 0;	/* enable pMTU discovery for multicast? */
VNET_DEFINE(int, ip6_v6only) = 1;

#ifdef IPSTEALTH
VNET_DEFINE(int, ip6stealth) = 0;
#endif
VNET_DEFINE(bool, ip6_log_cannot_forward) = 1;

/*
 * BSDI4 defines these variables in in_proto.c...
 * XXX: what if we don't define INET? Should we define pmtu6_expire
 * or so? (jinmei@kame.net 19990310)
 */
VNET_DEFINE(int, pmtu_expire) = 60*10;
VNET_DEFINE(int, pmtu_probe) = 60*2;

VNET_DEFINE_STATIC(int, ip6_log_interval) = 5;
VNET_DEFINE_STATIC(int, ip6_log_count) = 0;
VNET_DEFINE_STATIC(struct timeval, ip6_log_last) = { 0 };

#define	V_ip6_log_interval	VNET(ip6_log_interval)
#define	V_ip6_log_count		VNET(ip6_log_count)
#define	V_ip6_log_last		VNET(ip6_log_last)

/*
 * sysctl related items.
 */
SYSCTL_NODE(_net, PF_INET6, inet6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Internet6 Family");

/* net.inet6 */
SYSCTL_NODE(_net_inet6,	IPPROTO_IPV6, ip6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IP6");
SYSCTL_NODE(_net_inet6,	IPPROTO_ICMPV6, icmp6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "ICMP6");
SYSCTL_NODE(_net_inet6,	IPPROTO_UDP, udp6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "UDP6");
SYSCTL_NODE(_net_inet6,	IPPROTO_TCP, tcp6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP6");
#if defined(SCTP) || defined(SCTP_SUPPORT)
SYSCTL_NODE(_net_inet6,	IPPROTO_SCTP, sctp6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "SCTP6");
#endif
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
SYSCTL_NODE(_net_inet6,	IPPROTO_ESP, ipsec6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IPSEC6");
#endif /* IPSEC */

/* net.inet6.ip6 */
static int
sysctl_ip6_temppltime(SYSCTL_HANDLER_ARGS)
{
	int error, val, ndf;

	val = V_ip6_temp_preferred_lifetime;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || !req->newptr)
		return (error);
	ndf =  TEMP_MAX_DESYNC_FACTOR_BASE + (val >> 2) + (val >> 3);
	if (val < ndf + V_ip6_temp_regen_advance ||
	    val > V_ip6_temp_valid_lifetime)
		return (EINVAL);
	V_ip6_temp_preferred_lifetime = val;
	V_ip6_temp_max_desync_factor = ndf;
	V_ip6_desync_factor = arc4random() % ndf;
	return (0);
}

static int
sysctl_ip6_tempvltime(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = V_ip6_temp_valid_lifetime;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || !req->newptr)
		return (error);
	if (val < V_ip6_temp_preferred_lifetime)
		return (EINVAL);
	V_ip6_temp_valid_lifetime = val;
	return (0);
}

int
ip6_log_ratelimit(void)
{

	return (ppsratecheck(&V_ip6_log_last, &V_ip6_log_count,
	    V_ip6_log_interval));
}

SYSCTL_INT(_net_inet6_ip6, IPV6CTL_FORWARDING, forwarding,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_forwarding), 0,
	"Enable forwarding of IPv6 packets between interfaces");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_SENDREDIRECTS, redirect,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_sendredirects), 0,
	"Send ICMPv6 redirects for unforwardable IPv6 packets");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_DEFHLIM, hlim,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_defhlim), 0,
	"Default hop limit to use for outgoing IPv6 packets");
SYSCTL_VNET_PCPUSTAT(_net_inet6_ip6, IPV6CTL_STATS, stats, struct ip6stat,
	ip6stat,
	"IP6 statistics (struct ip6stat, netinet6/ip6_var.h)");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_ACCEPT_RTADV, accept_rtadv,
	CTLFLAG_VNET | CTLFLAG_RWTUN, &VNET_NAME(ip6_accept_rtadv), 0,
	"Default value of per-interface flag for accepting ICMPv6 RA messages");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_NO_RADR, no_radr,
	CTLFLAG_VNET | CTLFLAG_RWTUN, &VNET_NAME(ip6_no_radr), 0,
	"Default value of per-interface flag to control whether routers "
	"sending ICMPv6 RA messages on that interface are added into the "
	"default router list");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_NORBIT_RAIF, norbit_raif,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_norbit_raif), 0,
	"Always set clear the R flag in ICMPv6 NA messages when accepting RA "
	"on the interface");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_RFC6204W3, rfc6204w3,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_rfc6204w3), 0,
	"Accept the default router list from ICMPv6 RA messages even "
	"when packet forwarding is enabled");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_LOG_INTERVAL, log_interval,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_log_interval), 0,
	"Frequency in seconds at which to log IPv6 forwarding errors");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_HDRNESTLIMIT, hdrnestlimit,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_hdrnestlimit), 0,
	"Default maximum number of IPv6 extension headers permitted on "
	"incoming IPv6 packets, 0 for no artificial limit");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_DAD_COUNT, dad_count,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_dad_count), 0,
	"Number of ICMPv6 NS messages sent during duplicate address detection");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_AUTO_FLOWLABEL, auto_flowlabel,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_auto_flowlabel), 0,
	"Provide an IPv6 flowlabel in outbound packets");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_DEFMCASTHLIM, defmcasthlim,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_defmcasthlim), 0,
	"Default hop limit for IPv6 multicast packets originating from this "
	"node");
SYSCTL_STRING(_net_inet6_ip6, IPV6CTL_KAME_VERSION, kame_version,
	CTLFLAG_RD, __KAME_VERSION, 0,
	"KAME version string");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_USE_DEPRECATED, use_deprecated,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_use_deprecated), 0,
	"Allow the use of addresses whose preferred lifetimes have expired");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_RR_PRUNE, rr_prune,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_rr_prune), 0,
	""); /* XXX unused */
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_USETEMPADDR, use_tempaddr,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_use_tempaddr), 0,
	"Create RFC3041 temporary addresses for autoconfigured addresses");
SYSCTL_PROC(_net_inet6_ip6, IPV6CTL_TEMPPLTIME, temppltime,
	CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	NULL, 0, sysctl_ip6_temppltime, "I",
	"Maximum preferred lifetime for temporary addresses");
SYSCTL_PROC(_net_inet6_ip6, IPV6CTL_TEMPVLTIME, tempvltime,
	CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	NULL, 0, sysctl_ip6_tempvltime, "I",
	"Maximum valid lifetime for temporary addresses");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_V6ONLY, v6only,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_v6only), 0,
	"Restrict AF_INET6 sockets to IPv6 addresses only");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_AUTO_LINKLOCAL, auto_linklocal,
	CTLFLAG_VNET | CTLFLAG_RWTUN, &VNET_NAME(ip6_auto_linklocal), 0,
	"Default value of per-interface flag for automatically adding an IPv6 "
	"link-local address to interfaces when attached");
SYSCTL_VNET_PCPUSTAT(_net_inet6_ip6, IPV6CTL_RIP6STATS, rip6stats,
	struct rip6stat, rip6stat,
	"Raw IP6 statistics (struct rip6stat, netinet6/raw_ip6.h)");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_PREFER_TEMPADDR, prefer_tempaddr,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_prefer_tempaddr), 0,
	"Prefer RFC3041 temporary addresses in source address selection");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_USE_DEFAULTZONE, use_defaultzone,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_use_defzone), 0,
	"Use the default scope zone when none is specified");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_MCAST_PMTU, mcast_pmtu,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_mcast_pmtu), 0,
	"Enable path MTU discovery for multicast packets");
#ifdef IPSTEALTH
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_STEALTH, stealth, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(ip6stealth), 0,
	"Forward IPv6 packets without decrementing their TTL");
#endif
SYSCTL_BOOL(_net_inet6_ip6, OID_AUTO,
	log_cannot_forward, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(ip6_log_cannot_forward), 1,
	"Log packets that cannot be forwarded");
