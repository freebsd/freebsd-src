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

#ifndef _NETINET_VINET_H_
#define _NETINET_VINET_H_

#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/md5.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_hostcache.h>
#include <netinet/tcp_syncache.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

struct vnet_inet {
	struct	in_ifaddrhashhead *_in_ifaddrhashtbl;
	struct	in_ifaddrhead _in_ifaddrhead;
	u_long	_in_ifaddrhmask;
	struct	in_multihead _in_multihead;

	int	_arpt_keep;
	int	_arp_maxtries;
	int	_useloopback;
	int	_arp_proxyall;
	int	_subnetsarelocal;
	int	_sameprefixcarponly;

	int	_ipforwarding;
	int	_ipstealth;
	int	_ipfastforward_active;
	int	_ipsendredirects;
	int	_ip_defttl;
	int	_ip_keepfaith;
	int	_ip_sendsourcequench;
	int	_ip_do_randomid;
	int	_ip_checkinterface;
	u_short	_ip_id;

	uma_zone_t _ipq_zone;
	int	_nipq;			/* Total # of reass queues */
	int	_maxnipq;		/* Admin. limit on # reass queues. */
	int	_maxfragsperpacket;
	TAILQ_HEAD(ipqhead, ipq) _ipq[IPREASS_NHASH];

	struct	inpcbhead _tcb;		/* head of queue of active tcpcb's */
	struct	inpcbinfo _tcbinfo;
	struct	tcpstat _tcpstat;	/* tcp statistics */
	struct	tcp_hostcache _tcp_hostcache;
	struct  callout _tcp_hc_callout;

	struct	tcp_syncache _tcp_syncache;
	int	_tcp_syncookies;
	int	_tcp_syncookiesonly;
	int	_tcp_sc_rst_sock_fail;

	struct	inpcbhead _divcb;
	struct	inpcbinfo _divcbinfo;
	TAILQ_HEAD(, tcptw) _twq_2msl;

	int	_tcp_mssdflt;
	int	_tcp_v6mssdflt;
	int	_tcp_minmss;
	int	_tcp_do_rfc1323;
	int	_icmp_may_rst;
	int	_tcp_isn_reseed_interval;
	int	_tcp_inflight_enable;
	int	_tcp_inflight_rttthresh;
	int	_tcp_inflight_min;
	int	_tcp_inflight_max;
	int	_tcp_inflight_stab;
	int	_nolocaltimewait;
	int	_path_mtu_discovery;
	int	_ss_fltsz;
	int	_ss_fltsz_local;
	int	_tcp_do_newreno;
	int	_tcp_do_tso;
	int	_tcp_do_autosndbuf;
	int	_tcp_autosndbuf_inc;
	int	_tcp_autosndbuf_max;
	int	_tcp_do_sack;
	int	_tcp_sack_maxholes;
	int	_tcp_sack_globalmaxholes;
	int	_tcp_sack_globalholes;
	int	_blackhole;
	int	_tcp_delack_enabled;
	int	_drop_synfin;
	int	_tcp_do_rfc3042;
	int	_tcp_do_rfc3390;
	int	_tcp_do_ecn;
	int	_tcp_ecn_maxretries;
	int	_tcp_insecure_rst;
	int	_tcp_do_autorcvbuf;
	int	_tcp_autorcvbuf_inc;
	int	_tcp_autorcvbuf_max;
	int	_tcp_reass_maxseg;
	int	_tcp_reass_qsize;
	int	_tcp_reass_maxqlen;
	int	_tcp_reass_overflows;

	u_char	_isn_secret[32];
	int	_isn_last_reseed;
	u_int32_t _isn_offset;
	u_int32_t _isn_offset_old;
	MD5_CTX	_isn_ctx;

	struct	inpcbhead _udb;
	struct	inpcbinfo _udbinfo;
	struct	udpstat	_udpstat;
	int	_udp_blackhole;

	struct	inpcbhead _ripcb;
	struct	inpcbinfo _ripcbinfo;
	struct	socket *_ip_mrouter;

	struct	socket *_ip_rsvpd;
	int	_ip_rsvp_on;
	int	_rsvp_on;

	struct	icmpstat _icmpstat;
	struct	ipstat _ipstat;
	struct	igmpstat _igmpstat;

	SLIST_HEAD(, router_info) _router_info_head;

	int	_rtq_timeout;
	int	_rtq_reallyold;
	int	_rtq_minreallyold;
	int	_rtq_toomany;
	struct	callout _rtq_timer;

	int	_ipport_lowfirstauto;
	int	_ipport_lowlastauto;
	int	_ipport_firstauto;
	int	_ipport_lastauto;
	int	_ipport_hifirstauto;
	int	_ipport_hilastauto;
	int	_ipport_reservedhigh;
	int	_ipport_reservedlow;
	int	_ipport_randomized;
	int	_ipport_randomcps;
	int	_ipport_randomtime;
	int	_ipport_stoprandom;
	int	_ipport_tcpallocs;
	int	_ipport_tcplastcount;

	int	_icmpmaskrepl;
	u_int	_icmpmaskfake;
	int	_drop_redirect;
	int	_log_redirect;
	int	_icmplim;
	int	_icmplim_output;
	char	_reply_src[IFNAMSIZ];
	int	_icmp_rfi;
	int	_icmp_quotelen;
	int	_icmpbmcastecho;
};

/*
 * Symbol translation macros
 */
#define	INIT_VNET_INET(vnet) \
	INIT_FROM_VNET(vnet, VNET_MOD_INET, struct vnet_inet, vnet_inet)

#define	VNET_INET(sym)	VSYM(vnet_inet, sym)

#define	V_arp_maxtries		VNET_INET(arp_maxtries)
#define	V_arp_proxyall		VNET_INET(arp_proxyall)
#define	V_arpt_keep		VNET_INET(arpt_keep)
#define	V_blackhole		VNET_INET(blackhole)
#define	V_divcb			VNET_INET(divcb)
#define	V_divcbinfo		VNET_INET(divcbinfo)
#define	V_drop_redirect		VNET_INET(drop_redirect)
#define	V_drop_synfin		VNET_INET(drop_synfin)
#define	V_icmp_may_rst		VNET_INET(icmp_may_rst)
#define	V_icmp_quotelen		VNET_INET(icmp_quotelen)
#define	V_icmp_rfi		VNET_INET(icmp_rfi)
#define	V_icmpbmcastecho	VNET_INET(icmpbmcastecho)
#define	V_icmplim		VNET_INET(icmplim)
#define	V_icmplim_output	VNET_INET(icmplim_output)
#define	V_icmpmaskfake		VNET_INET(icmpmaskfake)
#define	V_icmpmaskrepl		VNET_INET(icmpmaskrepl)
#define	V_icmpstat		VNET_INET(icmpstat)
#define	V_igmpstat		VNET_INET(igmpstat)
#define	V_in_ifaddrhashtbl	VNET_INET(in_ifaddrhashtbl)
#define	V_in_ifaddrhead		VNET_INET(in_ifaddrhead)
#define	V_in_ifaddrhmask	VNET_INET(in_ifaddrhmask)
#define	V_in_multihead		VNET_INET(in_multihead)
#define	V_ip_checkinterface	VNET_INET(ip_checkinterface)
#define	V_ip_defttl		VNET_INET(ip_defttl)
#define	V_ip_do_randomid	VNET_INET(ip_do_randomid)
#define	V_ip_id			VNET_INET(ip_id)
#define	V_ip_keepfaith		VNET_INET(ip_keepfaith)
#define	V_ip_mrouter		VNET_INET(ip_mrouter)
#define	V_ip_rsvp_on		VNET_INET(ip_rsvp_on)
#define	V_ip_rsvpd		VNET_INET(ip_rsvpd)
#define	V_ip_sendsourcequench	VNET_INET(ip_sendsourcequench)
#define	V_ipfastforward_active	VNET_INET(ipfastforward_active)
#define	V_ipforwarding		VNET_INET(ipforwarding)
#define	V_ipport_firstauto	VNET_INET(ipport_firstauto)
#define	V_ipport_hifirstauto	VNET_INET(ipport_hifirstauto)
#define	V_ipport_hilastauto	VNET_INET(ipport_hilastauto)
#define	V_ipport_lastauto	VNET_INET(ipport_lastauto)
#define	V_ipport_lowfirstauto	VNET_INET(ipport_lowfirstauto)
#define	V_ipport_lowlastauto	VNET_INET(ipport_lowlastauto)
#define	V_ipport_randomcps	VNET_INET(ipport_randomcps)
#define	V_ipport_randomized	VNET_INET(ipport_randomized)
#define	V_ipport_randomtime	VNET_INET(ipport_randomtime)
#define	V_ipport_reservedhigh	VNET_INET(ipport_reservedhigh)
#define	V_ipport_reservedlow	VNET_INET(ipport_reservedlow)
#define	V_ipport_stoprandom	VNET_INET(ipport_stoprandom)
#define	V_ipport_tcpallocs	VNET_INET(ipport_tcpallocs)
#define	V_ipport_tcplastcount	VNET_INET(ipport_tcplastcount)
#define	V_ipq			VNET_INET(ipq)
#define	V_ipq_zone		VNET_INET(ipq_zone)
#define	V_ipsendredirects	VNET_INET(ipsendredirects)
#define	V_ipstat		VNET_INET(ipstat)
#define	V_ipstealth		VNET_INET(ipstealth)
#define	V_isn_ctx		VNET_INET(isn_ctx)
#define	V_isn_last_reseed	VNET_INET(isn_last_reseed)
#define	V_isn_offset		VNET_INET(isn_offset)
#define	V_isn_offset_old	VNET_INET(isn_offset_old)
#define	V_isn_secret		VNET_INET(isn_secret)
#define	V_llinfo_arp		VNET_INET(llinfo_arp)
#define	V_log_redirect		VNET_INET(log_redirect)
#define	V_maxfragsperpacket	VNET_INET(maxfragsperpacket)
#define	V_maxnipq		VNET_INET(maxnipq)
#define	V_nipq			VNET_INET(nipq)
#define	V_nolocaltimewait	VNET_INET(nolocaltimewait)
#define	V_path_mtu_discovery	VNET_INET(path_mtu_discovery)
#define	V_reply_src		VNET_INET(reply_src)
#define	V_ripcb			VNET_INET(ripcb)
#define	V_ripcbinfo		VNET_INET(ripcbinfo)
#define	V_router_info_head	VNET_INET(router_info_head)
#define	V_rsvp_on		VNET_INET(rsvp_on)
#define	V_rtq_minreallyold	VNET_INET(rtq_minreallyold)
#define	V_rtq_reallyold		VNET_INET(rtq_reallyold)
#define	V_rtq_timeout		VNET_INET(rtq_timeout)
#define	V_rtq_timer		VNET_INET(rtq_timer)
#define	V_rtq_toomany		VNET_INET(rtq_toomany)
#define	V_sameprefixcarponly	VNET_INET(sameprefixcarponly)
#define	V_ss_fltsz		VNET_INET(ss_fltsz)
#define	V_ss_fltsz_local	VNET_INET(ss_fltsz_local)
#define	V_subnetsarelocal	VNET_INET(subnetsarelocal)
#define	V_tcb			VNET_INET(tcb)
#define	V_tcbinfo		VNET_INET(tcbinfo)
#define	V_tcp_autorcvbuf_inc	VNET_INET(tcp_autorcvbuf_inc)
#define	V_tcp_autorcvbuf_max	VNET_INET(tcp_autorcvbuf_max)
#define	V_tcp_autosndbuf_inc	VNET_INET(tcp_autosndbuf_inc)
#define	V_tcp_autosndbuf_max	VNET_INET(tcp_autosndbuf_max)
#define	V_tcp_delack_enabled	VNET_INET(tcp_delack_enabled)
#define	V_tcp_do_autorcvbuf	VNET_INET(tcp_do_autorcvbuf)
#define	V_tcp_do_autosndbuf	VNET_INET(tcp_do_autosndbuf)
#define	V_tcp_do_ecn		VNET_INET(tcp_do_ecn)
#define	V_tcp_do_newreno	VNET_INET(tcp_do_newreno)
#define	V_tcp_do_rfc1323	VNET_INET(tcp_do_rfc1323)
#define	V_tcp_do_rfc3042	VNET_INET(tcp_do_rfc3042)
#define	V_tcp_do_rfc3390	VNET_INET(tcp_do_rfc3390)
#define	V_tcp_do_sack		VNET_INET(tcp_do_sack)
#define	V_tcp_do_tso		VNET_INET(tcp_do_tso)
#define	V_tcp_ecn_maxretries	VNET_INET(tcp_ecn_maxretries)
#define	V_tcp_hc_callout	VNET_INET(tcp_hc_callout)
#define	V_tcp_hostcache		VNET_INET(tcp_hostcache)
#define	V_tcp_inflight_enable	VNET_INET(tcp_inflight_enable)
#define	V_tcp_inflight_max	VNET_INET(tcp_inflight_max)
#define	V_tcp_inflight_min	VNET_INET(tcp_inflight_min)
#define	V_tcp_inflight_rttthresh VNET_INET(tcp_inflight_rttthresh)
#define	V_tcp_inflight_stab	VNET_INET(tcp_inflight_stab)
#define	V_tcp_insecure_rst	VNET_INET(tcp_insecure_rst)
#define	V_tcp_isn_reseed_interval VNET_INET(tcp_isn_reseed_interval)
#define	V_tcp_minmss		VNET_INET(tcp_minmss)
#define	V_tcp_mssdflt		VNET_INET(tcp_mssdflt)
#define	V_tcp_reass_maxqlen	VNET_INET(tcp_reass_maxqlen)
#define	V_tcp_reass_maxseg	VNET_INET(tcp_reass_maxseg)
#define	V_tcp_reass_overflows	VNET_INET(tcp_reass_overflows)
#define	V_tcp_reass_qsize	VNET_INET(tcp_reass_qsize)
#define	V_tcp_sack_globalholes	VNET_INET(tcp_sack_globalholes)
#define	V_tcp_sack_globalmaxholes VNET_INET(tcp_sack_globalmaxholes)
#define	V_tcp_sack_maxholes	VNET_INET(tcp_sack_maxholes)
#define	V_tcp_sc_rst_sock_fail	VNET_INET(tcp_sc_rst_sock_fail)
#define	V_tcp_syncache		VNET_INET(tcp_syncache)
#define	V_tcp_syncookies	VNET_INET(tcp_syncookies)
#define	V_tcp_syncookiesonly	VNET_INET(tcp_syncookiesonly)
#define	V_tcp_v6mssdflt		VNET_INET(tcp_v6mssdflt)
#define	V_tcpstat		VNET_INET(tcpstat)
#define	V_twq_2msl		VNET_INET(twq_2msl)
#define	V_udb			VNET_INET(udb)
#define	V_udbinfo		VNET_INET(udbinfo)
#define	V_udp_blackhole		VNET_INET(udp_blackhole)
#define	V_udpstat		VNET_INET(udpstat)
#define	V_useloopback		VNET_INET(useloopback)

static __inline uint16_t ip_newid(void);
extern int ip_do_randomid;

static __inline uint16_t
ip_newid(void)
{
        if (V_ip_do_randomid)
                return ip_randomid();

        return htons(V_ip_id++);
}

#endif /* !_NETINET_VINET_H_ */
