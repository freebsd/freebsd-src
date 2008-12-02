/*-
 * Copyright (c) 2006-2008 University of Zagreb
 * Copyright (c) 2006-2008 FreeBSD Foundation
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NETINET6_VINET6_H_
#define _NETINET6_VINET6_H_

#include <sys/callout.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <net/if_var.h>

#include <netinet/icmp6.h>
#include <netinet/in.h>

#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/raw_ip6.h>
#include <netinet6/scope6_var.h>

struct vnet_inet6 {
	struct in6_ifaddr *		_in6_ifaddr;

	u_int				_frag6_nfragpackets;
	u_int				_frag6_nfrags;
	struct ip6q			_ip6q;

	struct route_in6 		_ip6_forward_rt;

	struct in6_addrpolicy 		_defaultaddrpolicy;
	TAILQ_HEAD(, addrsel_policyent) _addrsel_policytab;
	u_int				_in6_maxmtu;
	int				_ip6_auto_linklocal;
	int				_rtq_minreallyold6;
	int				_rtq_reallyold6;
	int				_rtq_toomany6;

	struct ip6stat 			_ip6stat;
	struct rip6stat 		_rip6stat;
	struct icmp6stat 		_icmp6stat;

	int				_rtq_timeout6;  
	struct callout 			_rtq_timer6;
	struct callout 			_rtq_mtutimer;
	struct callout 			_nd6_slowtimo_ch;
	struct callout 			_nd6_timer_ch;
	struct callout 			_in6_tmpaddrtimer_ch;

	int				_nd6_inuse;
	int				_nd6_allocated;
	struct llinfo_nd6		_llinfo_nd6;
	struct nd_drhead		_nd_defrouter;
	struct nd_prhead 		_nd_prefix;
	struct ifnet *			_nd6_defifp;
	int				_nd6_defifindex;

	struct scope6_id 		_sid_default;

	TAILQ_HEAD(, dadq) 		_dadq;
	int				_dad_init;

	int				_icmp6errpps_count;
	int				_icmp6errppslim_last;

	int 				_ip6_forwarding;
	int				_ip6_sendredirects;
	int				_ip6_defhlim;
	int				_ip6_defmcasthlim;
	int				_ip6_accept_rtadv;
	int				_ip6_maxfragpackets;
	int				_ip6_maxfrags;
	int				_ip6_log_interval;
	int				_ip6_hdrnestlimit;
	int				_ip6_dad_count;
	int				_ip6_auto_flowlabel;
	int				_ip6_use_deprecated;
	int				_ip6_rr_prune;
	int				_ip6_mcast_pmtu;
	int				_ip6_v6only;
	int				_ip6_keepfaith;
	int				_ip6stealth;
	time_t				_ip6_log_time;
	int				_nd6_onlink_ns_rfc4861;

	int				_pmtu_expire;
	int				_pmtu_probe;
	u_long				_rip6_sendspace;
	u_long				_rip6_recvspace;
	int				_icmp6_rediraccept;
	int				_icmp6_redirtimeout;
	int				_icmp6errppslim;
	int				_icmp6_nodeinfo;
	int				_udp6_sendspace;
	int				_udp6_recvspace;
	int				_ip6qmaxlen;
	int				_ip6_prefer_tempaddr;
	int				_ip6_forward_srcrt;
	int				_ip6_sourcecheck;
	int				_ip6_sourcecheck_interval;
	int				_ip6_ours_check_algorithm;

	int				_nd6_prune;
	int				_nd6_delay;
	int				_nd6_umaxtries;
	int				_nd6_mmaxtries;
	int				_nd6_useloopback;
	int				_nd6_gctimer;
	int				_nd6_maxndopt;
	int				_nd6_maxnudhint;
	int				_nd6_maxqueuelen;
	int				_nd6_debug;
	int				_nd6_recalc_reachtm_interval;
	int				_dad_ignore_ns;
	int				_dad_maxtry;
	int				_ip6_use_tempaddr;
	int				_ip6_desync_factor;
	u_int32_t			_ip6_temp_preferred_lifetime;
	u_int32_t			_ip6_temp_valid_lifetime;

	int				_ip6_mrouter_ver;
	int				_pim6;
	u_int				_mrt6debug;

	int				_ip6_temp_regen_advance;
	int				_ip6_use_defzone;

	struct ip6_pktopts		_ip6_opts;
};

#define	INIT_VNET_INET6(vnet) \
	INIT_FROM_VNET(vnet, VNET_MOD_INET6, struct vnet_inet6, vnet_inet6)

#define	VNET_INET6(sym)		VSYM(vnet_inet6, sym)

/*
 * Symbol translation macros
 */
#define	V_addrsel_policytab		VNET_INET6(addrsel_policytab)
#define	V_dad_ignore_ns			VNET_INET6(dad_ignore_ns)
#define	V_dad_init			VNET_INET6(dad_init)
#define	V_dad_maxtry			VNET_INET6(dad_maxtry)
#define	V_dadq				VNET_INET6(dadq)
#define	V_defaultaddrpolicy		VNET_INET6(defaultaddrpolicy)
#define	V_frag6_nfragpackets		VNET_INET6(frag6_nfragpackets)
#define	V_frag6_nfrags			VNET_INET6(frag6_nfrags)
#define	V_icmp6_nodeinfo		VNET_INET6(icmp6_nodeinfo)
#define	V_icmp6_rediraccept		VNET_INET6(icmp6_rediraccept)
#define	V_icmp6_redirtimeout		VNET_INET6(icmp6_redirtimeout)
#define	V_icmp6errpps_count		VNET_INET6(icmp6errpps_count)
#define	V_icmp6errppslim		VNET_INET6(icmp6errppslim)
#define	V_icmp6errppslim_last		VNET_INET6(icmp6errppslim_last)
#define	V_icmp6stat			VNET_INET6(icmp6stat)
#define	V_in6_ifaddr			VNET_INET6(in6_ifaddr)
#define	V_in6_maxmtu			VNET_INET6(in6_maxmtu)
#define	V_in6_tmpaddrtimer_ch		VNET_INET6(in6_tmpaddrtimer_ch)
#define	V_ip6_accept_rtadv		VNET_INET6(ip6_accept_rtadv)
#define	V_ip6_auto_flowlabel		VNET_INET6(ip6_auto_flowlabel)
#define	V_ip6_auto_linklocal		VNET_INET6(ip6_auto_linklocal)
#define	V_ip6_dad_count			VNET_INET6(ip6_dad_count)
#define	V_ip6_defhlim			VNET_INET6(ip6_defhlim)
#define	V_ip6_defmcasthlim		VNET_INET6(ip6_defmcasthlim)
#define	V_ip6_desync_factor		VNET_INET6(ip6_desync_factor)
#define	V_ip6_forward_rt		VNET_INET6(ip6_forward_rt)
#define	V_ip6_forward_srcrt		VNET_INET6(ip6_forward_srcrt)
#define	V_ip6_forwarding		VNET_INET6(ip6_forwarding)
#define	V_ip6_hdrnestlimit		VNET_INET6(ip6_hdrnestlimit)
#define	V_ip6_keepfaith			VNET_INET6(ip6_keepfaith)
#define	V_ip6_log_interval		VNET_INET6(ip6_log_interval)
#define	V_ip6_log_time			VNET_INET6(ip6_log_time)
#define	V_ip6_maxfragpackets		VNET_INET6(ip6_maxfragpackets)
#define	V_ip6_maxfrags			VNET_INET6(ip6_maxfrags)
#define	V_ip6_mcast_pmtu		VNET_INET6(ip6_mcast_pmtu)
#define	V_ip6_mrouter_ver		VNET_INET6(ip6_mrouter_ver)
#define	V_ip6_opts			VNET_INET6(ip6_opts)
#define	V_ip6_ours_check_algorithm	VNET_INET6(ip6_ours_check_algorithm)
#define	V_ip6_prefer_tempaddr		VNET_INET6(ip6_prefer_tempaddr)
#define	V_ip6_rr_prune			VNET_INET6(ip6_rr_prune)
#define	V_ip6_sendredirects		VNET_INET6(ip6_sendredirects)
#define	V_ip6_sourcecheck		VNET_INET6(ip6_sourcecheck)
#define	V_ip6_sourcecheck_interval	VNET_INET6(ip6_sourcecheck_interval)
#define	V_ip6_temp_preferred_lifetime	VNET_INET6(ip6_temp_preferred_lifetime)
#define	V_ip6_temp_regen_advance	VNET_INET6(ip6_temp_regen_advance)
#define	V_ip6_temp_valid_lifetime	VNET_INET6(ip6_temp_valid_lifetime)
#define	V_ip6_use_defzone		VNET_INET6(ip6_use_defzone)
#define	V_ip6_use_deprecated		VNET_INET6(ip6_use_deprecated)
#define	V_ip6_use_tempaddr		VNET_INET6(ip6_use_tempaddr)
#define	V_ip6_v6only			VNET_INET6(ip6_v6only)
#define	V_ip6q				VNET_INET6(ip6q)
#define	V_ip6qmaxlen			VNET_INET6(ip6qmaxlen)
#define	V_ip6stat			VNET_INET6(ip6stat)
#define	V_ip6stealth			VNET_INET6(ip6stealth)
#define	V_llinfo_nd6			VNET_INET6(llinfo_nd6)
#define	V_mrt6debug			VNET_INET6(mrt6debug)
#define	V_nd6_allocated			VNET_INET6(nd6_allocated)
#define	V_nd6_debug			VNET_INET6(nd6_debug)
#define	V_nd6_defifindex		VNET_INET6(nd6_defifindex)
#define	V_nd6_defifp			VNET_INET6(nd6_defifp)
#define	V_nd6_delay			VNET_INET6(nd6_delay)
#define	V_nd6_gctimer			VNET_INET6(nd6_gctimer)
#define	V_nd6_inuse			VNET_INET6(nd6_inuse)
#define	V_nd6_maxndopt			VNET_INET6(nd6_maxndopt)
#define	V_nd6_maxnudhint		VNET_INET6(nd6_maxnudhint)
#define	V_nd6_maxqueuelen		VNET_INET6(nd6_maxqueuelen)
#define	V_nd6_mmaxtries			VNET_INET6(nd6_mmaxtries)
#define	V_nd6_onlink_ns_rfc4861		VNET_INET6(nd6_onlink_ns_rfc4861)
#define	V_nd6_prune			VNET_INET6(nd6_prune)
#define	V_nd6_recalc_reachtm_interval	VNET_INET6(nd6_recalc_reachtm_interval)
#define	V_nd6_slowtimo_ch		VNET_INET6(nd6_slowtimo_ch)
#define	V_nd6_timer_ch			VNET_INET6(nd6_timer_ch)
#define	V_nd6_umaxtries			VNET_INET6(nd6_umaxtries)
#define	V_nd6_useloopback		VNET_INET6(nd6_useloopback)
#define	V_nd_defrouter			VNET_INET6(nd_defrouter)
#define	V_nd_prefix			VNET_INET6(nd_prefix)
#define	V_pim6				VNET_INET6(pim6)
#define	V_pmtu_expire			VNET_INET6(pmtu_expire)
#define	V_pmtu_probe			VNET_INET6(pmtu_probe)
#define	V_rip6_recvspace		VNET_INET6(rip6_recvspace)
#define	V_rip6_sendspace		VNET_INET6(rip6_sendspace)
#define	V_rip6stat			VNET_INET6(rip6stat)
#define	V_rtq_minreallyold6 		VNET_INET6(rtq_minreallyold6)
#define	V_rtq_mtutimer			VNET_INET6(rtq_mtutimer)
#define	V_rtq_reallyold6		VNET_INET6(rtq_reallyold6)
#define	V_rtq_timeout6			VNET_INET6(rtq_timeout6)
#define	V_rtq_timer6			VNET_INET6(rtq_timer6)
#define	V_rtq_toomany6			VNET_INET6(rtq_toomany6)
#define	V_sid_default			VNET_INET6(sid_default)
#define	V_udp6_recvspace		VNET_INET6(udp6_recvspace)	
#define	V_udp6_sendspace		VNET_INET6(udp6_sendspace)

#endif /* !_NETINET6_VINET6_H_ */
