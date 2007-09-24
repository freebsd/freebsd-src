/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 * $FreeBSD$
 */

#include "opt_compat.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mac.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#ifdef INET6
#include <sys/domain.h>
#endif
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/random.h>

#include <vm/uma.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>
#endif
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_syncache.h>
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif
#include <netinet6/ip6protosw.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/key.h>
#endif /*IPSEC*/

#include <machine/in_cksum.h>
#include <sys/md5.h>

#include <security/mac/mac_framework.h>

int	tcp_mssdflt = TCP_MSS;
SYSCTL_INT(_net_inet_tcp, TCPCTL_MSSDFLT, mssdflt, CTLFLAG_RW,
    &tcp_mssdflt, 0, "Default TCP Maximum Segment Size");

#ifdef INET6
int	tcp_v6mssdflt = TCP6_MSS;
SYSCTL_INT(_net_inet_tcp, TCPCTL_V6MSSDFLT, v6mssdflt,
    CTLFLAG_RW, &tcp_v6mssdflt , 0,
    "Default TCP Maximum Segment Size for IPv6");
#endif

/*
 * Minimum MSS we accept and use. This prevents DoS attacks where
 * we are forced to a ridiculous low MSS like 20 and send hundreds
 * of packets instead of one. The effect scales with the available
 * bandwidth and quickly saturates the CPU and network interface
 * with packet generation and sending. Set to zero to disable MINMSS
 * checking. This setting prevents us from sending too small packets.
 */
int	tcp_minmss = TCP_MINMSS;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, minmss, CTLFLAG_RW,
    &tcp_minmss , 0, "Minmum TCP Maximum Segment Size");

int	tcp_do_rfc1323 = 1;
SYSCTL_INT(_net_inet_tcp, TCPCTL_DO_RFC1323, rfc1323, CTLFLAG_RW,
    &tcp_do_rfc1323, 0, "Enable rfc1323 (high performance TCP) extensions");

static int	tcp_log_debug = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, log_debug, CTLFLAG_RW,
    &tcp_log_debug, 0, "Log errors caused by incoming TCP segments");

static int	tcp_tcbhashsize = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, tcbhashsize, CTLFLAG_RDTUN,
    &tcp_tcbhashsize, 0, "Size of TCP control-block hashtable");

static int	do_tcpdrain = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, do_tcpdrain, CTLFLAG_RW,
    &do_tcpdrain, 0,
    "Enable tcp_drain routine for extra help when low on mbufs");

SYSCTL_INT(_net_inet_tcp, OID_AUTO, pcbcount, CTLFLAG_RD,
    &tcbinfo.ipi_count, 0, "Number of active PCBs");

static int	icmp_may_rst = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, icmp_may_rst, CTLFLAG_RW,
    &icmp_may_rst, 0,
    "Certain ICMP unreachable messages may abort connections in SYN_SENT");

static int	tcp_isn_reseed_interval = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, isn_reseed_interval, CTLFLAG_RW,
    &tcp_isn_reseed_interval, 0, "Seconds between reseeding of ISN secret");

/*
 * TCP bandwidth limiting sysctls.  Note that the default lower bound of
 * 1024 exists only for debugging.  A good production default would be
 * something like 6100.
 */
SYSCTL_NODE(_net_inet_tcp, OID_AUTO, inflight, CTLFLAG_RW, 0,
    "TCP inflight data limiting");

static int	tcp_inflight_enable = 1;
SYSCTL_INT(_net_inet_tcp_inflight, OID_AUTO, enable, CTLFLAG_RW,
    &tcp_inflight_enable, 0, "Enable automatic TCP inflight data limiting");

static int	tcp_inflight_debug = 0;
SYSCTL_INT(_net_inet_tcp_inflight, OID_AUTO, debug, CTLFLAG_RW,
    &tcp_inflight_debug, 0, "Debug TCP inflight calculations");

static int	tcp_inflight_rttthresh;
SYSCTL_PROC(_net_inet_tcp_inflight, OID_AUTO, rttthresh, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_inflight_rttthresh, 0, sysctl_msec_to_ticks, "I",
    "RTT threshold below which inflight will deactivate itself");

static int	tcp_inflight_min = 6144;
SYSCTL_INT(_net_inet_tcp_inflight, OID_AUTO, min, CTLFLAG_RW,
    &tcp_inflight_min, 0, "Lower-bound for TCP inflight window");

static int	tcp_inflight_max = TCP_MAXWIN << TCP_MAX_WINSHIFT;
SYSCTL_INT(_net_inet_tcp_inflight, OID_AUTO, max, CTLFLAG_RW,
    &tcp_inflight_max, 0, "Upper-bound for TCP inflight window");

static int	tcp_inflight_stab = 20;
SYSCTL_INT(_net_inet_tcp_inflight, OID_AUTO, stab, CTLFLAG_RW,
    &tcp_inflight_stab, 0, "Inflight Algorithm Stabilization 20 = 2 packets");

uma_zone_t sack_hole_zone;

static struct inpcb *tcp_notify(struct inpcb *, int);
static void	tcp_isn_tick(void *);

/*
 * Target size of TCP PCB hash tables. Must be a power of two.
 *
 * Note that this can be overridden by the kernel environment
 * variable net.inet.tcp.tcbhashsize
 */
#ifndef TCBHASHSIZE
#define TCBHASHSIZE	512
#endif

/*
 * XXX
 * Callouts should be moved into struct tcp directly.  They are currently
 * separate because the tcpcb structure is exported to userland for sysctl
 * parsing purposes, which do not know about callouts.
 */
struct tcpcb_mem {
	struct	tcpcb		tcb;
	struct	tcp_timer	tt;
};

static uma_zone_t tcpcb_zone;
MALLOC_DEFINE(M_TCPLOG, "tcplog", "TCP address and flags print buffers");
struct callout isn_callout;
static struct mtx isn_mtx;

#define	ISN_LOCK_INIT()	mtx_init(&isn_mtx, "isn_mtx", NULL, MTX_DEF)
#define	ISN_LOCK()	mtx_lock(&isn_mtx)
#define	ISN_UNLOCK()	mtx_unlock(&isn_mtx)

/*
 * TCP initialization.
 */
static void
tcp_zone_change(void *tag)
{

	uma_zone_set_max(tcbinfo.ipi_zone, maxsockets);
	uma_zone_set_max(tcpcb_zone, maxsockets);
	tcp_tw_zone_change();
}

static int
tcp_inpcb_init(void *mem, int size, int flags)
{
	struct inpcb *inp = mem;

	INP_LOCK_INIT(inp, "inp", "tcpinp");
	return (0);
}

void
tcp_init(void)
{

	int hashsize = TCBHASHSIZE;
	tcp_delacktime = TCPTV_DELACK;
	tcp_keepinit = TCPTV_KEEP_INIT;
	tcp_keepidle = TCPTV_KEEP_IDLE;
	tcp_keepintvl = TCPTV_KEEPINTVL;
	tcp_maxpersistidle = TCPTV_KEEP_IDLE;
	tcp_msl = TCPTV_MSL;
	tcp_rexmit_min = TCPTV_MIN;
	if (tcp_rexmit_min < 1)
		tcp_rexmit_min = 1;
	tcp_rexmit_slop = TCPTV_CPU_VAR;
	tcp_inflight_rttthresh = TCPTV_INFLIGHT_RTTTHRESH;
	tcp_finwait2_timeout = TCPTV_FINWAIT2_TIMEOUT;

	INP_INFO_LOCK_INIT(&tcbinfo, "tcp");
	LIST_INIT(&tcb);
	tcbinfo.ipi_listhead = &tcb;
	TUNABLE_INT_FETCH("net.inet.tcp.tcbhashsize", &hashsize);
	if (!powerof2(hashsize)) {
		printf("WARNING: TCB hash size not a power of 2\n");
		hashsize = 512; /* safe default */
	}
	tcp_tcbhashsize = hashsize;
	tcbinfo.ipi_hashbase = hashinit(hashsize, M_PCB,
	    &tcbinfo.ipi_hashmask);
	tcbinfo.ipi_porthashbase = hashinit(hashsize, M_PCB,
	    &tcbinfo.ipi_porthashmask);
	tcbinfo.ipi_zone = uma_zcreate("inpcb", sizeof(struct inpcb),
	    NULL, NULL, tcp_inpcb_init, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(tcbinfo.ipi_zone, maxsockets);
#ifdef INET6
#define TCP_MINPROTOHDR (sizeof(struct ip6_hdr) + sizeof(struct tcphdr))
#else /* INET6 */
#define TCP_MINPROTOHDR (sizeof(struct tcpiphdr))
#endif /* INET6 */
	if (max_protohdr < TCP_MINPROTOHDR)
		max_protohdr = TCP_MINPROTOHDR;
	if (max_linkhdr + TCP_MINPROTOHDR > MHLEN)
		panic("tcp_init");
#undef TCP_MINPROTOHDR
	/*
	 * These have to be type stable for the benefit of the timers.
	 */
	tcpcb_zone = uma_zcreate("tcpcb", sizeof(struct tcpcb_mem),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(tcpcb_zone, maxsockets);
	tcp_tw_init();
	syncache_init();
	tcp_hc_init();
	tcp_reass_init();
	ISN_LOCK_INIT();
	callout_init(&isn_callout, CALLOUT_MPSAFE);
	tcp_isn_tick(NULL);
	EVENTHANDLER_REGISTER(shutdown_pre_sync, tcp_fini, NULL,
		SHUTDOWN_PRI_DEFAULT);
	sack_hole_zone = uma_zcreate("sackhole", sizeof(struct sackhole),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	EVENTHANDLER_REGISTER(maxsockets_change, tcp_zone_change, NULL,
		EVENTHANDLER_PRI_ANY);
}

void
tcp_fini(void *xtp)
{

	callout_stop(&isn_callout);
}

/*
 * Fill in the IP and TCP headers for an outgoing packet, given the tcpcb.
 * tcp_template used to store this data in mbufs, but we now recopy it out
 * of the tcpcb each time to conserve mbufs.
 */
void
tcpip_fillheaders(struct inpcb *inp, void *ip_ptr, void *tcp_ptr)
{
	struct tcphdr *th = (struct tcphdr *)tcp_ptr;

	INP_LOCK_ASSERT(inp);

#ifdef INET6
	if ((inp->inp_vflag & INP_IPV6) != 0) {
		struct ip6_hdr *ip6;

		ip6 = (struct ip6_hdr *)ip_ptr;
		ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
			(inp->in6p_flowinfo & IPV6_FLOWINFO_MASK);
		ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
			(IPV6_VERSION & IPV6_VERSION_MASK);
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = sizeof(struct tcphdr);
		ip6->ip6_src = inp->in6p_laddr;
		ip6->ip6_dst = inp->in6p_faddr;
	} else
#endif
	{
		struct ip *ip;

		ip = (struct ip *)ip_ptr;
		ip->ip_v = IPVERSION;
		ip->ip_hl = 5;
		ip->ip_tos = inp->inp_ip_tos;
		ip->ip_len = 0;
		ip->ip_id = 0;
		ip->ip_off = 0;
		ip->ip_ttl = inp->inp_ip_ttl;
		ip->ip_sum = 0;
		ip->ip_p = IPPROTO_TCP;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst = inp->inp_faddr;
	}
	th->th_sport = inp->inp_lport;
	th->th_dport = inp->inp_fport;
	th->th_seq = 0;
	th->th_ack = 0;
	th->th_x2 = 0;
	th->th_off = 5;
	th->th_flags = 0;
	th->th_win = 0;
	th->th_urp = 0;
	th->th_sum = 0;		/* in_pseudo() is called later for ipv4 */
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Allocates an mbuf and fills in a skeletal tcp/ip header.  The only
 * use for this function is in keepalives, which use tcp_respond.
 */
struct tcptemp *
tcpip_maketemplate(struct inpcb *inp)
{
	struct mbuf *m;
	struct tcptemp *n;

	m = m_get(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (0);
	m->m_len = sizeof(struct tcptemp);
	n = mtod(m, struct tcptemp *);

	tcpip_fillheaders(inp, (void *)&n->tt_ipgen, (void *)&n->tt_t);
	return (n);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == NULL, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection.  If flags are given then we send
 * a message back to the TCP which originated the * segment ti,
 * and discard the mbuf containing it and any other attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 *
 * NOTE: If m != NULL, then ti must point to *inside* the mbuf.
 */
void
tcp_respond(struct tcpcb *tp, void *ipgen, struct tcphdr *th, struct mbuf *m,
    tcp_seq ack, tcp_seq seq, int flags)
{
	int tlen;
	int win = 0;
	struct ip *ip;
	struct tcphdr *nth;
#ifdef INET6
	struct ip6_hdr *ip6;
	int isipv6;
#endif /* INET6 */
	int ipflags = 0;
	struct inpcb *inp;

	KASSERT(tp != NULL || m != NULL, ("tcp_respond: tp and m both NULL"));

#ifdef INET6
	isipv6 = ((struct ip *)ipgen)->ip_v == 6;
	ip6 = ipgen;
#endif /* INET6 */
	ip = ipgen;

	if (tp != NULL) {
		inp = tp->t_inpcb;
		KASSERT(inp != NULL, ("tcp control block w/o inpcb"));
		INP_LOCK_ASSERT(inp);
	} else
		inp = NULL;

	if (tp != NULL) {
		if (!(flags & TH_RST)) {
			win = sbspace(&inp->inp_socket->so_rcv);
			if (win > (long)TCP_MAXWIN << tp->rcv_scale)
				win = (long)TCP_MAXWIN << tp->rcv_scale;
		}
	}
	if (m == NULL) {
		m = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return;
		tlen = 0;
		m->m_data += max_linkhdr;
#ifdef INET6
		if (isipv6) {
			bcopy((caddr_t)ip6, mtod(m, caddr_t),
			      sizeof(struct ip6_hdr));
			ip6 = mtod(m, struct ip6_hdr *);
			nth = (struct tcphdr *)(ip6 + 1);
		} else
#endif /* INET6 */
	      {
		bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
		ip = mtod(m, struct ip *);
		nth = (struct tcphdr *)(ip + 1);
	      }
		bcopy((caddr_t)th, (caddr_t)nth, sizeof(struct tcphdr));
		flags = TH_ACK;
	} else {
		m_freem(m->m_next);
		m->m_next = NULL;
		m->m_data = (caddr_t)ipgen;
		/* m_len is set later */
		tlen = 0;
#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
#ifdef INET6
		if (isipv6) {
			xchg(ip6->ip6_dst, ip6->ip6_src, struct in6_addr);
			nth = (struct tcphdr *)(ip6 + 1);
		} else
#endif /* INET6 */
	      {
		xchg(ip->ip_dst.s_addr, ip->ip_src.s_addr, n_long);
		nth = (struct tcphdr *)(ip + 1);
	      }
		if (th != nth) {
			/*
			 * this is usually a case when an extension header
			 * exists between the IPv6 header and the
			 * TCP header.
			 */
			nth->th_sport = th->th_sport;
			nth->th_dport = th->th_dport;
		}
		xchg(nth->th_dport, nth->th_sport, n_short);
#undef xchg
	}
#ifdef INET6
	if (isipv6) {
		ip6->ip6_flow = 0;
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = htons((u_short)(sizeof (struct tcphdr) +
						tlen));
		tlen += sizeof (struct ip6_hdr) + sizeof (struct tcphdr);
	} else
#endif
	{
		tlen += sizeof (struct tcpiphdr);
		ip->ip_len = tlen;
		ip->ip_ttl = ip_defttl;
		if (path_mtu_discovery)
			ip->ip_off |= IP_DF;
	}
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;
#ifdef MAC
	if (inp != NULL) {
		/*
		 * Packet is associated with a socket, so allow the
		 * label of the response to reflect the socket label.
		 */
		INP_LOCK_ASSERT(inp);
		mac_create_mbuf_from_inpcb(inp, m);
	} else {
		/*
		 * Packet is not associated with a socket, so possibly
		 * update the label in place.
		 */
		mac_reflect_mbuf_tcp(m);
	}
#endif
	nth->th_seq = htonl(seq);
	nth->th_ack = htonl(ack);
	nth->th_x2 = 0;
	nth->th_off = sizeof (struct tcphdr) >> 2;
	nth->th_flags = flags;
	if (tp != NULL)
		nth->th_win = htons((u_short) (win >> tp->rcv_scale));
	else
		nth->th_win = htons((u_short)win);
	nth->th_urp = 0;
#ifdef INET6
	if (isipv6) {
		nth->th_sum = 0;
		nth->th_sum = in6_cksum(m, IPPROTO_TCP,
					sizeof(struct ip6_hdr),
					tlen - sizeof(struct ip6_hdr));
		ip6->ip6_hlim = in6_selecthlim(tp != NULL ? tp->t_inpcb :
		    NULL, NULL);
	} else
#endif /* INET6 */
	{
		nth->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons((u_short)(tlen - sizeof(struct ip) + ip->ip_p)));
		m->m_pkthdr.csum_flags = CSUM_TCP;
		m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
	}
#ifdef TCPDEBUG
	if (tp == NULL || (inp->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_OUTPUT, 0, tp, mtod(m, void *), th, 0);
#endif
#ifdef INET6
	if (isipv6)
		(void) ip6_output(m, NULL, NULL, ipflags, NULL, NULL, inp);
	else
#endif /* INET6 */
	(void) ip_output(m, NULL, NULL, ipflags, NULL, inp);
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.  The `inp' parameter must have
 * come from the zone allocator set up in tcp_init().
 */
struct tcpcb *
tcp_newtcpcb(struct inpcb *inp)
{
	struct tcpcb_mem *tm;
	struct tcpcb *tp;
#ifdef INET6
	int isipv6 = (inp->inp_vflag & INP_IPV6) != 0;
#endif /* INET6 */

	tm = uma_zalloc(tcpcb_zone, M_NOWAIT | M_ZERO);
	if (tm == NULL)
		return (NULL);
	tp = &tm->tcb;
	tp->t_timers = &tm->tt;
	/*	LIST_INIT(&tp->t_segq); */	/* XXX covered by M_ZERO */
	tp->t_maxseg = tp->t_maxopd =
#ifdef INET6
		isipv6 ? tcp_v6mssdflt :
#endif /* INET6 */
		tcp_mssdflt;

	/* Set up our timeouts. */
	callout_init(&tp->t_timers->tt_rexmt, CALLOUT_MPSAFE);
	callout_init(&tp->t_timers->tt_persist, CALLOUT_MPSAFE);
	callout_init(&tp->t_timers->tt_keep, CALLOUT_MPSAFE);
	callout_init(&tp->t_timers->tt_2msl, CALLOUT_MPSAFE);
	callout_init(&tp->t_timers->tt_delack, CALLOUT_MPSAFE);

	if (tcp_do_rfc1323)
		tp->t_flags = (TF_REQ_SCALE|TF_REQ_TSTMP);
	if (tcp_do_sack)
		tp->t_flags |= TF_SACK_PERMIT;
	TAILQ_INIT(&tp->snd_holes);
	tp->t_inpcb = inp;	/* XXX */
	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 4 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = ((TCPTV_RTOBASE - TCPTV_SRTTBASE) << TCP_RTTVAR_SHIFT) / 4;
	tp->t_rttmin = tcp_rexmit_min;
	tp->t_rxtcur = TCPTV_RTOBASE;
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_bwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->t_rcvtime = ticks;
	tp->t_bw_rtttime = ticks;
	/*
	 * IPv4 TTL initialization is necessary for an IPv6 socket as well,
	 * because the socket may be bound to an IPv6 wildcard address,
	 * which may match an IPv4-mapped IPv6 address.
	 */
	inp->inp_ip_ttl = ip_defttl;
	inp->inp_ppcb = tp;
	return (tp);		/* XXX */
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop(struct tcpcb *tp, int errno)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(tp->t_inpcb);

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		(void) tcp_output(tp);
		tcpstat.tcps_drops++;
	} else
		tcpstat.tcps_conndrops++;
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

void
tcp_discardcb(struct tcpcb *tp)
{
	struct tseg_qent *q;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
#ifdef INET6
	int isipv6 = (inp->inp_vflag & INP_IPV6) != 0;
#endif /* INET6 */

	INP_LOCK_ASSERT(inp);

	/*
	 * Make sure that all of our timers are stopped before we
	 * delete the PCB.
	 */
	callout_stop(&tp->t_timers->tt_rexmt);
	callout_stop(&tp->t_timers->tt_persist);
	callout_stop(&tp->t_timers->tt_keep);
	callout_stop(&tp->t_timers->tt_2msl);
	callout_stop(&tp->t_timers->tt_delack);

	/*
	 * If we got enough samples through the srtt filter,
	 * save the rtt and rttvar in the routing entry.
	 * 'Enough' is arbitrarily defined as 4 rtt samples.
	 * 4 samples is enough for the srtt filter to converge
	 * to within enough % of the correct value; fewer samples
	 * and we could save a bogus rtt. The danger is not high
	 * as tcp quickly recovers from everything.
	 * XXX: Works very well but needs some more statistics!
	 */
	if (tp->t_rttupdated >= 4) {
		struct hc_metrics_lite metrics;
		u_long ssthresh;

		bzero(&metrics, sizeof(metrics));
		/*
		 * Update the ssthresh always when the conditions below
		 * are satisfied. This gives us better new start value
		 * for the congestion avoidance for new connections.
		 * ssthresh is only set if packet loss occured on a session.
		 *
		 * XXXRW: 'so' may be NULL here, and/or socket buffer may be
		 * being torn down.  Ideally this code would not use 'so'.
		 */
		ssthresh = tp->snd_ssthresh;
		if (ssthresh != 0 && ssthresh < so->so_snd.sb_hiwat / 2) {
			/*
			 * convert the limit from user data bytes to
			 * packets then to packet data bytes.
			 */
			ssthresh = (ssthresh + tp->t_maxseg / 2) / tp->t_maxseg;
			if (ssthresh < 2)
				ssthresh = 2;
			ssthresh *= (u_long)(tp->t_maxseg +
#ifdef INET6
				      (isipv6 ? sizeof (struct ip6_hdr) +
					       sizeof (struct tcphdr) :
#endif
				       sizeof (struct tcpiphdr)
#ifdef INET6
				       )
#endif
				      );
		} else
			ssthresh = 0;
		metrics.rmx_ssthresh = ssthresh;

		metrics.rmx_rtt = tp->t_srtt;
		metrics.rmx_rttvar = tp->t_rttvar;
		/* XXX: This wraps if the pipe is more than 4 Gbit per second */
		metrics.rmx_bandwidth = tp->snd_bandwidth;
		metrics.rmx_cwnd = tp->snd_cwnd;
		metrics.rmx_sendpipe = 0;
		metrics.rmx_recvpipe = 0;

		tcp_hc_update(&inp->inp_inc, &metrics);
	}

	/* free the reassembly queue, if any */
	while ((q = LIST_FIRST(&tp->t_segq)) != NULL) {
		LIST_REMOVE(q, tqe_q);
		m_freem(q->tqe_m);
		uma_zfree(tcp_reass_zone, q);
		tp->t_segqlen--;
		tcp_reass_qsize--;
	}
	tcp_free_sackholes(tp);
	inp->inp_ppcb = NULL;
	tp->t_inpcb = NULL;
	uma_zfree(tcpcb_zone, tp);
}

/*
 * Attempt to close a TCP control block, marking it as dropped, and freeing
 * the socket if we hold the only reference.
 */
struct tcpcb *
tcp_close(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(inp);

	in_pcbdrop(inp);
	tcpstat.tcps_closed++;
	KASSERT(inp->inp_socket != NULL, ("tcp_close: inp_socket NULL"));
	so = inp->inp_socket;
	soisdisconnected(so);
	if (inp->inp_vflag & INP_SOCKREF) {
		KASSERT(so->so_state & SS_PROTOREF,
		    ("tcp_close: !SS_PROTOREF"));
		inp->inp_vflag &= ~INP_SOCKREF;
		INP_UNLOCK(inp);
		ACCEPT_LOCK();
		SOCK_LOCK(so);
		so->so_state &= ~SS_PROTOREF;
		sofree(so);
		return (NULL);
	}
	return (tp);
}

void
tcp_drain(void)
{

	if (do_tcpdrain) {
		struct inpcb *inpb;
		struct tcpcb *tcpb;
		struct tseg_qent *te;

	/*
	 * Walk the tcpbs, if existing, and flush the reassembly queue,
	 * if there is one...
	 * XXX: The "Net/3" implementation doesn't imply that the TCP
	 *      reassembly queue should be flushed, but in a situation
	 *	where we're really low on mbufs, this is potentially
	 *	usefull.
	 */
		INP_INFO_RLOCK(&tcbinfo);
		LIST_FOREACH(inpb, tcbinfo.ipi_listhead, inp_list) {
			if (inpb->inp_vflag & INP_TIMEWAIT)
				continue;
			INP_LOCK(inpb);
			if ((tcpb = intotcpcb(inpb)) != NULL) {
				while ((te = LIST_FIRST(&tcpb->t_segq))
			            != NULL) {
					LIST_REMOVE(te, tqe_q);
					m_freem(te->tqe_m);
					uma_zfree(tcp_reass_zone, te);
					tcpb->t_segqlen--;
					tcp_reass_qsize--;
				}
				tcp_clean_sackreport(tcpb);
			}
			INP_UNLOCK(inpb);
		}
		INP_INFO_RUNLOCK(&tcbinfo);
	}
}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 *
 * Do not wake up user since there currently is no mechanism for
 * reporting soft errors (yet - a kqueue filter may be added).
 */
static struct inpcb *
tcp_notify(struct inpcb *inp, int error)
{
	struct tcpcb *tp;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(inp);

	if ((inp->inp_vflag & INP_TIMEWAIT) ||
	    (inp->inp_vflag & INP_DROPPED))
		return (inp);

	tp = intotcpcb(inp);
	KASSERT(tp != NULL, ("tcp_notify: tp == NULL"));

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (error == EHOSTUNREACH || error == ENETUNREACH ||
	     error == EHOSTDOWN)) {
		return (inp);
	} else if (tp->t_state < TCPS_ESTABLISHED && tp->t_rxtshift > 3 &&
	    tp->t_softerror) {
		tp = tcp_drop(tp, error);
		if (tp != NULL)
			return (inp);
		else
			return (NULL);
	} else {
		tp->t_softerror = error;
		return (inp);
	}
#if 0
	wakeup( &so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
#endif
}

static int
tcp_pcblist(SYSCTL_HANDLER_ARGS)
{
	int error, i, m, n, pcb_count;
	struct inpcb *inp, **inp_list;
	inp_gen_t gencnt;
	struct xinpgen xig;

	/*
	 * The process of preparing the TCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == NULL) {
		m = syncache_pcbcount();
		n = tcbinfo.ipi_count;
		req->oldidx = 2 * (sizeof xig)
			+ ((m + n) + n/8) * sizeof(struct xtcpcb);
		return (0);
	}

	if (req->newptr != NULL)
		return (EPERM);

	/*
	 * OK, now we're committed to doing something.
	 */
	INP_INFO_RLOCK(&tcbinfo);
	gencnt = tcbinfo.ipi_gencnt;
	n = tcbinfo.ipi_count;
	INP_INFO_RUNLOCK(&tcbinfo);

	m = syncache_pcbcount();

	error = sysctl_wire_old_buffer(req, 2 * (sizeof xig)
		+ (n + m) * sizeof(struct xtcpcb));
	if (error != 0)
		return (error);

	xig.xig_len = sizeof xig;
	xig.xig_count = n + m;
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return (error);

	error = syncache_pcblist(req, m, &pcb_count);
	if (error)
		return (error);

	inp_list = malloc(n * sizeof *inp_list, M_TEMP, M_WAITOK);
	if (inp_list == NULL)
		return (ENOMEM);

	INP_INFO_RLOCK(&tcbinfo);
	for (inp = LIST_FIRST(tcbinfo.ipi_listhead), i = 0; inp != NULL && i
	    < n; inp = LIST_NEXT(inp, inp_list)) {
		INP_LOCK(inp);
		if (inp->inp_gencnt <= gencnt) {
			/*
			 * XXX: This use of cr_cansee(), introduced with
			 * TCP state changes, is not quite right, but for
			 * now, better than nothing.
			 */
			if (inp->inp_vflag & INP_TIMEWAIT) {
				if (intotw(inp) != NULL)
					error = cr_cansee(req->td->td_ucred,
					    intotw(inp)->tw_cred);
				else
					error = EINVAL;	/* Skip this inp. */
			} else
				error = cr_canseesocket(req->td->td_ucred,
				    inp->inp_socket);
			if (error == 0)
				inp_list[i++] = inp;
		}
		INP_UNLOCK(inp);
	}
	INP_INFO_RUNLOCK(&tcbinfo);
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_LOCK(inp);
		if (inp->inp_gencnt <= gencnt) {
			struct xtcpcb xt;
			void *inp_ppcb;

			bzero(&xt, sizeof(xt));
			xt.xt_len = sizeof xt;
			/* XXX should avoid extra copy */
			bcopy(inp, &xt.xt_inp, sizeof *inp);
			inp_ppcb = inp->inp_ppcb;
			if (inp_ppcb == NULL)
				bzero((char *) &xt.xt_tp, sizeof xt.xt_tp);
			else if (inp->inp_vflag & INP_TIMEWAIT) {
				bzero((char *) &xt.xt_tp, sizeof xt.xt_tp);
				xt.xt_tp.t_state = TCPS_TIME_WAIT;
			} else
				bcopy(inp_ppcb, &xt.xt_tp, sizeof xt.xt_tp);
			if (inp->inp_socket != NULL)
				sotoxsocket(inp->inp_socket, &xt.xt_socket);
			else {
				bzero(&xt.xt_socket, sizeof xt.xt_socket);
				xt.xt_socket.xso_protocol = IPPROTO_TCP;
			}
			xt.xt_inp.inp_gencnt = inp->inp_gencnt;
			INP_UNLOCK(inp);
			error = SYSCTL_OUT(req, &xt, sizeof xt);
		} else
			INP_UNLOCK(inp);
	
	}
	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		INP_INFO_RLOCK(&tcbinfo);
		xig.xig_gen = tcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = tcbinfo.ipi_count + pcb_count;
		INP_INFO_RUNLOCK(&tcbinfo);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}
	free(inp_list, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_PCBLIST, pcblist, CTLFLAG_RD, 0, 0,
    tcp_pcblist, "S,xtcpcb", "List of active TCP connections");

static int
tcp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in addrs[2];
	struct inpcb *inp;
	int error;

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	INP_INFO_RLOCK(&tcbinfo);
	inp = in_pcblookup_hash(&tcbinfo, addrs[1].sin_addr, addrs[1].sin_port,
	    addrs[0].sin_addr, addrs[0].sin_port, 0, NULL);
	if (inp == NULL) {
		error = ENOENT;
		goto outunlocked;
	}
	INP_LOCK(inp);
	if (inp->inp_socket == NULL) {
		error = ENOENT;
		goto out;
	}
	error = cr_canseesocket(req->td->td_ucred, inp->inp_socket);
	if (error)
		goto out;
	cru2x(inp->inp_socket->so_cred, &xuc);
out:
	INP_UNLOCK(inp);
outunlocked:
	INP_INFO_RUNLOCK(&tcbinfo);
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, getcred,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0,
    tcp_getcred, "S,xucred", "Get the xucred of a TCP connection");

#ifdef INET6
static int
tcp6_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in6 addrs[2];
	struct inpcb *inp;
	int error, mapped = 0;

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	if ((error = sa6_embedscope(&addrs[0], ip6_use_defzone)) != 0 ||
	    (error = sa6_embedscope(&addrs[1], ip6_use_defzone)) != 0) {
		return (error);
	}
	if (IN6_IS_ADDR_V4MAPPED(&addrs[0].sin6_addr)) {
		if (IN6_IS_ADDR_V4MAPPED(&addrs[1].sin6_addr))
			mapped = 1;
		else
			return (EINVAL);
	}

	INP_INFO_RLOCK(&tcbinfo);
	if (mapped == 1)
		inp = in_pcblookup_hash(&tcbinfo,
			*(struct in_addr *)&addrs[1].sin6_addr.s6_addr[12],
			addrs[1].sin6_port,
			*(struct in_addr *)&addrs[0].sin6_addr.s6_addr[12],
			addrs[0].sin6_port,
			0, NULL);
	else
		inp = in6_pcblookup_hash(&tcbinfo,
			&addrs[1].sin6_addr, addrs[1].sin6_port,
			&addrs[0].sin6_addr, addrs[0].sin6_port, 0, NULL);
	if (inp == NULL) {
		error = ENOENT;
		goto outunlocked;
	}
	INP_LOCK(inp);
	if (inp->inp_socket == NULL) {
		error = ENOENT;
		goto out;
	}
	error = cr_canseesocket(req->td->td_ucred, inp->inp_socket);
	if (error)
		goto out;
	cru2x(inp->inp_socket->so_cred, &xuc);
out:
	INP_UNLOCK(inp);
outunlocked:
	INP_INFO_RUNLOCK(&tcbinfo);
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet6_tcp6, OID_AUTO, getcred,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0,
    tcp6_getcred, "S,xucred", "Get the xucred of a TCP6 connection");
#endif


void
tcp_ctlinput(int cmd, struct sockaddr *sa, void *vip)
{
	struct ip *ip = vip;
	struct tcphdr *th;
	struct in_addr faddr;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct inpcb *(*notify)(struct inpcb *, int) = tcp_notify;
	struct icmp *icp;
	struct in_conninfo inc;
	tcp_seq icmp_tcp_seq;
	int mtu;

	faddr = ((struct sockaddr_in *)sa)->sin_addr;
	if (sa->sa_family != AF_INET || faddr.s_addr == INADDR_ANY)
		return;

	if (cmd == PRC_MSGSIZE)
		notify = tcp_mtudisc;
	else if (icmp_may_rst && (cmd == PRC_UNREACH_ADMIN_PROHIB ||
		cmd == PRC_UNREACH_PORT || cmd == PRC_TIMXCEED_INTRANS) && ip)
		notify = tcp_drop_syn_sent;
	/*
	 * Redirects don't need to be handled up here.
	 */
	else if (PRC_IS_REDIRECT(cmd))
		return;
	/*
	 * Source quench is depreciated.
	 */
	else if (cmd == PRC_QUENCH)
		return;
	/*
	 * Hostdead is ugly because it goes linearly through all PCBs.
	 * XXX: We never get this from ICMP, otherwise it makes an
	 * excellent DoS attack on machines with many connections.
	 */
	else if (cmd == PRC_HOSTDEAD)
		ip = NULL;
	else if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0)
		return;
	if (ip != NULL) {
		icp = (struct icmp *)((caddr_t)ip
				      - offsetof(struct icmp, icmp_ip));
		th = (struct tcphdr *)((caddr_t)ip
				       + (ip->ip_hl << 2));
		INP_INFO_WLOCK(&tcbinfo);
		inp = in_pcblookup_hash(&tcbinfo, faddr, th->th_dport,
		    ip->ip_src, th->th_sport, 0, NULL);
		if (inp != NULL)  {
			INP_LOCK(inp);
			if (!(inp->inp_vflag & INP_TIMEWAIT) &&
			    !(inp->inp_vflag & INP_DROPPED) &&
			    !(inp->inp_socket == NULL)) {
				icmp_tcp_seq = htonl(th->th_seq);
				tp = intotcpcb(inp);
				if (SEQ_GEQ(icmp_tcp_seq, tp->snd_una) &&
				    SEQ_LT(icmp_tcp_seq, tp->snd_max)) {
					if (cmd == PRC_MSGSIZE) {
					    /*
					     * MTU discovery:
					     * If we got a needfrag set the MTU
					     * in the route to the suggested new
					     * value (if given) and then notify.
					     */
					    bzero(&inc, sizeof(inc));
					    inc.inc_flags = 0;	/* IPv4 */
					    inc.inc_faddr = faddr;

					    mtu = ntohs(icp->icmp_nextmtu);
					    /*
					     * If no alternative MTU was
					     * proposed, try the next smaller
					     * one.  ip->ip_len has already
					     * been swapped in icmp_input().
					     */
					    if (!mtu)
						mtu = ip_next_mtu(ip->ip_len,
						 1);
					    if (mtu < max(296, (tcp_minmss)
						 + sizeof(struct tcpiphdr)))
						mtu = 0;
					    if (!mtu)
						mtu = tcp_mssdflt
						 + sizeof(struct tcpiphdr);
					    /*
					     * Only cache the the MTU if it
					     * is smaller than the interface
					     * or route MTU.  tcp_mtudisc()
					     * will do right thing by itself.
					     */
					    if (mtu <= tcp_maxmtu(&inc, NULL))
						tcp_hc_updatemtu(&inc, mtu);
					}

					inp = (*notify)(inp, inetctlerrmap[cmd]);
				}
			}
			if (inp != NULL)
				INP_UNLOCK(inp);
		} else {
			inc.inc_fport = th->th_dport;
			inc.inc_lport = th->th_sport;
			inc.inc_faddr = faddr;
			inc.inc_laddr = ip->ip_src;
#ifdef INET6
			inc.inc_isipv6 = 0;
#endif
			syncache_unreach(&inc, th);
		}
		INP_INFO_WUNLOCK(&tcbinfo);
	} else
		in_pcbnotifyall(&tcbinfo, faddr, inetctlerrmap[cmd], notify);
}

#ifdef INET6
void
tcp6_ctlinput(int cmd, struct sockaddr *sa, void *d)
{
	struct tcphdr th;
	struct inpcb *(*notify)(struct inpcb *, int) = tcp_notify;
	struct ip6_hdr *ip6;
	struct mbuf *m;
	struct ip6ctlparam *ip6cp = NULL;
	const struct sockaddr_in6 *sa6_src = NULL;
	int off;
	struct tcp_portonly {
		u_int16_t th_sport;
		u_int16_t th_dport;
	} *thp;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;

	if (cmd == PRC_MSGSIZE)
		notify = tcp_mtudisc;
	else if (!PRC_IS_REDIRECT(cmd) &&
		 ((unsigned)cmd >= PRC_NCMDS || inet6ctlerrmap[cmd] == 0))
		return;
	/* Source quench is depreciated. */
	else if (cmd == PRC_QUENCH)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		sa6_src = ip6cp->ip6c_src;
	} else {
		m = NULL;
		ip6 = NULL;
		off = 0;	/* fool gcc */
		sa6_src = &sa6_any;
	}

	if (ip6 != NULL) {
		struct in_conninfo inc;
		/*
		 * XXX: We assume that when IPV6 is non NULL,
		 * M and OFF are valid.
		 */

		/* check if we can safely examine src and dst ports */
		if (m->m_pkthdr.len < off + sizeof(*thp))
			return;

		bzero(&th, sizeof(th));
		m_copydata(m, off, sizeof(*thp), (caddr_t)&th);

		in6_pcbnotify(&tcbinfo, sa, th.th_dport,
		    (struct sockaddr *)ip6cp->ip6c_src,
		    th.th_sport, cmd, NULL, notify);

		inc.inc_fport = th.th_dport;
		inc.inc_lport = th.th_sport;
		inc.inc6_faddr = ((struct sockaddr_in6 *)sa)->sin6_addr;
		inc.inc6_laddr = ip6cp->ip6c_src->sin6_addr;
		inc.inc_isipv6 = 1;
		INP_INFO_WLOCK(&tcbinfo);
		syncache_unreach(&inc, &th);
		INP_INFO_WUNLOCK(&tcbinfo);
	} else
		in6_pcbnotify(&tcbinfo, sa, 0, (const struct sockaddr *)sa6_src,
			      0, cmd, NULL, notify);
}
#endif /* INET6 */


/*
 * Following is where TCP initial sequence number generation occurs.
 *
 * There are two places where we must use initial sequence numbers:
 * 1.  In SYN-ACK packets.
 * 2.  In SYN packets.
 *
 * All ISNs for SYN-ACK packets are generated by the syncache.  See
 * tcp_syncache.c for details.
 *
 * The ISNs in SYN packets must be monotonic; TIME_WAIT recycling
 * depends on this property.  In addition, these ISNs should be
 * unguessable so as to prevent connection hijacking.  To satisfy
 * the requirements of this situation, the algorithm outlined in
 * RFC 1948 is used, with only small modifications.
 *
 * Implementation details:
 *
 * Time is based off the system timer, and is corrected so that it
 * increases by one megabyte per second.  This allows for proper
 * recycling on high speed LANs while still leaving over an hour
 * before rollover.
 *
 * As reading the *exact* system time is too expensive to be done
 * whenever setting up a TCP connection, we increment the time
 * offset in two ways.  First, a small random positive increment
 * is added to isn_offset for each connection that is set up.
 * Second, the function tcp_isn_tick fires once per clock tick
 * and increments isn_offset as necessary so that sequence numbers
 * are incremented at approximately ISN_BYTES_PER_SECOND.  The
 * random positive increments serve only to ensure that the same
 * exact sequence number is never sent out twice (as could otherwise
 * happen when a port is recycled in less than the system tick
 * interval.)
 *
 * net.inet.tcp.isn_reseed_interval controls the number of seconds
 * between seeding of isn_secret.  This is normally set to zero,
 * as reseeding should not be necessary.
 *
 * Locking of the global variables isn_secret, isn_last_reseed, isn_offset,
 * isn_offset_old, and isn_ctx is performed using the TCP pcbinfo lock.  In
 * general, this means holding an exclusive (write) lock.
 */

#define ISN_BYTES_PER_SECOND 1048576
#define ISN_STATIC_INCREMENT 4096
#define ISN_RANDOM_INCREMENT (4096 - 1)

static u_char isn_secret[32];
static int isn_last_reseed;
static u_int32_t isn_offset, isn_offset_old;
static MD5_CTX isn_ctx;

tcp_seq
tcp_new_isn(struct tcpcb *tp)
{
	u_int32_t md5_buffer[4];
	tcp_seq new_isn;

	INP_LOCK_ASSERT(tp->t_inpcb);

	ISN_LOCK();
	/* Seed if this is the first use, reseed if requested. */
	if ((isn_last_reseed == 0) || ((tcp_isn_reseed_interval > 0) &&
	     (((u_int)isn_last_reseed + (u_int)tcp_isn_reseed_interval*hz)
		< (u_int)ticks))) {
		read_random(&isn_secret, sizeof(isn_secret));
		isn_last_reseed = ticks;
	}

	/* Compute the md5 hash and return the ISN. */
	MD5Init(&isn_ctx);
	MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->inp_fport, sizeof(u_short));
	MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->inp_lport, sizeof(u_short));
#ifdef INET6
	if ((tp->t_inpcb->inp_vflag & INP_IPV6) != 0) {
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->in6p_faddr,
			  sizeof(struct in6_addr));
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->in6p_laddr,
			  sizeof(struct in6_addr));
	} else
#endif
	{
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->inp_faddr,
			  sizeof(struct in_addr));
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->inp_laddr,
			  sizeof(struct in_addr));
	}
	MD5Update(&isn_ctx, (u_char *) &isn_secret, sizeof(isn_secret));
	MD5Final((u_char *) &md5_buffer, &isn_ctx);
	new_isn = (tcp_seq) md5_buffer[0];
	isn_offset += ISN_STATIC_INCREMENT +
		(arc4random() & ISN_RANDOM_INCREMENT);
	new_isn += isn_offset;
	ISN_UNLOCK();
	return (new_isn);
}

/*
 * Increment the offset to the next ISN_BYTES_PER_SECOND / 100 boundary
 * to keep time flowing at a relatively constant rate.  If the random
 * increments have already pushed us past the projected offset, do nothing.
 */
static void
tcp_isn_tick(void *xtp)
{
	u_int32_t projected_offset;

	ISN_LOCK();
	projected_offset = isn_offset_old + ISN_BYTES_PER_SECOND / 100;

	if (SEQ_GT(projected_offset, isn_offset))
		isn_offset = projected_offset;

	isn_offset_old = isn_offset;
	callout_reset(&isn_callout, hz/100, tcp_isn_tick, NULL);
	ISN_UNLOCK();
}

/*
 * When a specific ICMP unreachable message is received and the
 * connection state is SYN-SENT, drop the connection.  This behavior
 * is controlled by the icmp_may_rst sysctl.
 */
struct inpcb *
tcp_drop_syn_sent(struct inpcb *inp, int errno)
{
	struct tcpcb *tp;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(inp);

	if ((inp->inp_vflag & INP_TIMEWAIT) ||
	    (inp->inp_vflag & INP_DROPPED))
		return (inp);

	tp = intotcpcb(inp);
	if (tp->t_state != TCPS_SYN_SENT)
		return (inp);

	tp = tcp_drop(tp, errno);
	if (tp != NULL)
		return (inp);
	else
		return (NULL);
}

/*
 * When `need fragmentation' ICMP is received, update our idea of the MSS
 * based on the new value in the route.  Also nudge TCP to send something,
 * since we know the packet we just sent was dropped.
 * This duplicates some code in the tcp_mss() function in tcp_input.c.
 */
struct inpcb *
tcp_mtudisc(struct inpcb *inp, int errno)
{
	struct tcpcb *tp;
	struct socket *so = inp->inp_socket;
	u_int maxmtu;
	u_int romtu;
	int mss;
#ifdef INET6
	int isipv6;
#endif /* INET6 */

	INP_LOCK_ASSERT(inp);
	if ((inp->inp_vflag & INP_TIMEWAIT) ||
	    (inp->inp_vflag & INP_DROPPED))
		return (inp);

	tp = intotcpcb(inp);
	KASSERT(tp != NULL, ("tcp_mtudisc: tp == NULL"));

#ifdef INET6
	isipv6 = (tp->t_inpcb->inp_vflag & INP_IPV6) != 0;
#endif
	maxmtu = tcp_hc_getmtu(&inp->inp_inc); /* IPv4 and IPv6 */
	romtu =
#ifdef INET6
	    isipv6 ? tcp_maxmtu6(&inp->inp_inc, NULL) :
#endif /* INET6 */
	    tcp_maxmtu(&inp->inp_inc, NULL);
	if (!maxmtu)
		maxmtu = romtu;
	else
		maxmtu = min(maxmtu, romtu);
	if (!maxmtu) {
		tp->t_maxopd = tp->t_maxseg =
#ifdef INET6
			isipv6 ? tcp_v6mssdflt :
#endif /* INET6 */
			tcp_mssdflt;
		return (inp);
	}
	mss = maxmtu -
#ifdef INET6
		(isipv6 ? sizeof(struct ip6_hdr) + sizeof(struct tcphdr) :
#endif /* INET6 */
		 sizeof(struct tcpiphdr)
#ifdef INET6
		 )
#endif /* INET6 */
		;

	/*
	 * XXX - The above conditional probably violates the TCP
	 * spec.  The problem is that, since we don't know the
	 * other end's MSS, we are supposed to use a conservative
	 * default.  But, if we do that, then MTU discovery will
	 * never actually take place, because the conservative
	 * default is much less than the MTUs typically seen
	 * on the Internet today.  For the moment, we'll sweep
	 * this under the carpet.
	 *
	 * The conservative default might not actually be a problem
	 * if the only case this occurs is when sending an initial
	 * SYN with options and data to a host we've never talked
	 * to before.  Then, they will reply with an MSS value which
	 * will get recorded and the new parameters should get
	 * recomputed.  For Further Study.
	 */
	if (tp->t_maxopd <= mss)
		return (inp);
	tp->t_maxopd = mss;

	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	    (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP)
		mss -= TCPOLEN_TSTAMP_APPA;
#if	(MCLBYTES & (MCLBYTES - 1)) == 0
	if (mss > MCLBYTES)
		mss &= ~(MCLBYTES-1);
#else
	if (mss > MCLBYTES)
		mss = mss / MCLBYTES * MCLBYTES;
#endif
	if (so->so_snd.sb_hiwat < mss)
		mss = so->so_snd.sb_hiwat;

	tp->t_maxseg = mss;

	tcpstat.tcps_mturesent++;
	tp->t_rtttime = 0;
	tp->snd_nxt = tp->snd_una;
	tcp_free_sackholes(tp);
	tp->snd_recover = tp->snd_max;
	if (tp->t_flags & TF_SACK_PERMIT)
		EXIT_FASTRECOVERY(tp);
	tcp_output(tp);
	return (inp);
}

/*
 * Look-up the routing entry to the peer of this inpcb.  If no route
 * is found and it cannot be allocated, then return NULL.  This routine
 * is called by TCP routines that access the rmx structure and by tcp_mss
 * to get the interface MTU.
 */
u_long
tcp_maxmtu(struct in_conninfo *inc, int *flags)
{
	struct route sro;
	struct sockaddr_in *dst;
	struct ifnet *ifp;
	u_long maxmtu = 0;

	KASSERT(inc != NULL, ("tcp_maxmtu with NULL in_conninfo pointer"));

	bzero(&sro, sizeof(sro));
	if (inc->inc_faddr.s_addr != INADDR_ANY) {
	        dst = (struct sockaddr_in *)&sro.ro_dst;
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = inc->inc_faddr;
		rtalloc_ign(&sro, RTF_CLONING);
	}
	if (sro.ro_rt != NULL) {
		ifp = sro.ro_rt->rt_ifp;
		if (sro.ro_rt->rt_rmx.rmx_mtu == 0)
			maxmtu = ifp->if_mtu;
		else
			maxmtu = min(sro.ro_rt->rt_rmx.rmx_mtu, ifp->if_mtu);

		/* Report additional interface capabilities. */
		if (flags != NULL) {
			if (ifp->if_capenable & IFCAP_TSO4 &&
			    ifp->if_hwassist & CSUM_TSO)
				*flags |= CSUM_TSO;
		}
		RTFREE(sro.ro_rt);
	}
	return (maxmtu);
}

#ifdef INET6
u_long
tcp_maxmtu6(struct in_conninfo *inc, int *flags)
{
	struct route_in6 sro6;
	struct ifnet *ifp;
	u_long maxmtu = 0;

	KASSERT(inc != NULL, ("tcp_maxmtu6 with NULL in_conninfo pointer"));

	bzero(&sro6, sizeof(sro6));
	if (!IN6_IS_ADDR_UNSPECIFIED(&inc->inc6_faddr)) {
		sro6.ro_dst.sin6_family = AF_INET6;
		sro6.ro_dst.sin6_len = sizeof(struct sockaddr_in6);
		sro6.ro_dst.sin6_addr = inc->inc6_faddr;
		rtalloc_ign((struct route *)&sro6, RTF_CLONING);
	}
	if (sro6.ro_rt != NULL) {
		ifp = sro6.ro_rt->rt_ifp;
		if (sro6.ro_rt->rt_rmx.rmx_mtu == 0)
			maxmtu = IN6_LINKMTU(sro6.ro_rt->rt_ifp);
		else
			maxmtu = min(sro6.ro_rt->rt_rmx.rmx_mtu,
				     IN6_LINKMTU(sro6.ro_rt->rt_ifp));

		/* Report additional interface capabilities. */
		if (flags != NULL) {
			if (ifp->if_capenable & IFCAP_TSO6 &&
			    ifp->if_hwassist & CSUM_TSO)
				*flags |= CSUM_TSO;
		}
		RTFREE(sro6.ro_rt);
	}

	return (maxmtu);
}
#endif /* INET6 */

#ifdef IPSEC
/* compute ESP/AH header size for TCP, including outer IP header. */
size_t
ipsec_hdrsiz_tcp(struct tcpcb *tp)
{
	struct inpcb *inp;
	struct mbuf *m;
	size_t hdrsiz;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct tcphdr *th;

	if ((tp == NULL) || ((inp = tp->t_inpcb) == NULL))
		return (0);
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (!m)
		return (0);

#ifdef INET6
	if ((inp->inp_vflag & INP_IPV6) != 0) {
		ip6 = mtod(m, struct ip6_hdr *);
		th = (struct tcphdr *)(ip6 + 1);
		m->m_pkthdr.len = m->m_len =
			sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
		tcpip_fillheaders(inp, ip6, th);
		hdrsiz = ipsec6_hdrsiz(m, IPSEC_DIR_OUTBOUND, inp);
	} else
#endif /* INET6 */
	{
		ip = mtod(m, struct ip *);
		th = (struct tcphdr *)(ip + 1);
		m->m_pkthdr.len = m->m_len = sizeof(struct tcpiphdr);
		tcpip_fillheaders(inp, ip, th);
		hdrsiz = ipsec4_hdrsiz(m, IPSEC_DIR_OUTBOUND, inp);
	}

	m_free(m);
	return (hdrsiz);
}
#endif /* IPSEC */

/*
 * TCP BANDWIDTH DELAY PRODUCT WINDOW LIMITING
 *
 * This code attempts to calculate the bandwidth-delay product as a
 * means of determining the optimal window size to maximize bandwidth,
 * minimize RTT, and avoid the over-allocation of buffers on interfaces and
 * routers.  This code also does a fairly good job keeping RTTs in check
 * across slow links like modems.  We implement an algorithm which is very
 * similar (but not meant to be) TCP/Vegas.  The code operates on the
 * transmitter side of a TCP connection and so only effects the transmit
 * side of the connection.
 *
 * BACKGROUND:  TCP makes no provision for the management of buffer space
 * at the end points or at the intermediate routers and switches.  A TCP
 * stream, whether using NewReno or not, will eventually buffer as
 * many packets as it is able and the only reason this typically works is
 * due to the fairly small default buffers made available for a connection
 * (typicaly 16K or 32K).  As machines use larger windows and/or window
 * scaling it is now fairly easy for even a single TCP connection to blow-out
 * all available buffer space not only on the local interface, but on
 * intermediate routers and switches as well.  NewReno makes a misguided
 * attempt to 'solve' this problem by waiting for an actual failure to occur,
 * then backing off, then steadily increasing the window again until another
 * failure occurs, ad-infinitum.  This results in terrible oscillation that
 * is only made worse as network loads increase and the idea of intentionally
 * blowing out network buffers is, frankly, a terrible way to manage network
 * resources.
 *
 * It is far better to limit the transmit window prior to the failure
 * condition being achieved.  There are two general ways to do this:  First
 * you can 'scan' through different transmit window sizes and locate the
 * point where the RTT stops increasing, indicating that you have filled the
 * pipe, then scan backwards until you note that RTT stops decreasing, then
 * repeat ad-infinitum.  This method works in principle but has severe
 * implementation issues due to RTT variances, timer granularity, and
 * instability in the algorithm which can lead to many false positives and
 * create oscillations as well as interact badly with other TCP streams
 * implementing the same algorithm.
 *
 * The second method is to limit the window to the bandwidth delay product
 * of the link.  This is the method we implement.  RTT variances and our
 * own manipulation of the congestion window, bwnd, can potentially
 * destabilize the algorithm.  For this reason we have to stabilize the
 * elements used to calculate the window.  We do this by using the minimum
 * observed RTT, the long term average of the observed bandwidth, and
 * by adding two segments worth of slop.  It isn't perfect but it is able
 * to react to changing conditions and gives us a very stable basis on
 * which to extend the algorithm.
 */
void
tcp_xmit_bandwidth_limit(struct tcpcb *tp, tcp_seq ack_seq)
{
	u_long bw;
	u_long bwnd;
	int save_ticks;

	INP_LOCK_ASSERT(tp->t_inpcb);

	/*
	 * If inflight_enable is disabled in the middle of a tcp connection,
	 * make sure snd_bwnd is effectively disabled.
	 */
	if (tcp_inflight_enable == 0 || tp->t_rttlow < tcp_inflight_rttthresh) {
		tp->snd_bwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
		tp->snd_bandwidth = 0;
		return;
	}

	/*
	 * Figure out the bandwidth.  Due to the tick granularity this
	 * is a very rough number and it MUST be averaged over a fairly
	 * long period of time.  XXX we need to take into account a link
	 * that is not using all available bandwidth, but for now our
	 * slop will ramp us up if this case occurs and the bandwidth later
	 * increases.
	 *
	 * Note: if ticks rollover 'bw' may wind up negative.  We must
	 * effectively reset t_bw_rtttime for this case.
	 */
	save_ticks = ticks;
	if ((u_int)(save_ticks - tp->t_bw_rtttime) < 1)
		return;

	bw = (int64_t)(ack_seq - tp->t_bw_rtseq) * hz /
	    (save_ticks - tp->t_bw_rtttime);
	tp->t_bw_rtttime = save_ticks;
	tp->t_bw_rtseq = ack_seq;
	if (tp->t_bw_rtttime == 0 || (int)bw < 0)
		return;
	bw = ((int64_t)tp->snd_bandwidth * 15 + bw) >> 4;

	tp->snd_bandwidth = bw;

	/*
	 * Calculate the semi-static bandwidth delay product, plus two maximal
	 * segments.  The additional slop puts us squarely in the sweet
	 * spot and also handles the bandwidth run-up case and stabilization.
	 * Without the slop we could be locking ourselves into a lower
	 * bandwidth.
	 *
	 * Situations Handled:
	 *	(1) Prevents over-queueing of packets on LANs, especially on
	 *	    high speed LANs, allowing larger TCP buffers to be
	 *	    specified, and also does a good job preventing
	 *	    over-queueing of packets over choke points like modems
	 *	    (at least for the transmit side).
	 *
	 *	(2) Is able to handle changing network loads (bandwidth
	 *	    drops so bwnd drops, bandwidth increases so bwnd
	 *	    increases).
	 *
	 *	(3) Theoretically should stabilize in the face of multiple
	 *	    connections implementing the same algorithm (this may need
	 *	    a little work).
	 *
	 *	(4) Stability value (defaults to 20 = 2 maximal packets) can
	 *	    be adjusted with a sysctl but typically only needs to be
	 *	    on very slow connections.  A value no smaller then 5
	 *	    should be used, but only reduce this default if you have
	 *	    no other choice.
	 */
#define USERTT	((tp->t_srtt + tp->t_rttbest) / 2)
	bwnd = (int64_t)bw * USERTT / (hz << TCP_RTT_SHIFT) + tcp_inflight_stab * tp->t_maxseg / 10;
#undef USERTT

	if (tcp_inflight_debug > 0) {
		static int ltime;
		if ((u_int)(ticks - ltime) >= hz / tcp_inflight_debug) {
			ltime = ticks;
			printf("%p bw %ld rttbest %d srtt %d bwnd %ld\n",
			    tp,
			    bw,
			    tp->t_rttbest,
			    tp->t_srtt,
			    bwnd
			);
		}
	}
	if ((long)bwnd < tcp_inflight_min)
		bwnd = tcp_inflight_min;
	if (bwnd > tcp_inflight_max)
		bwnd = tcp_inflight_max;
	if ((long)bwnd < tp->t_maxseg * 2)
		bwnd = tp->t_maxseg * 2;
	tp->snd_bwnd = bwnd;
}

#ifdef TCP_SIGNATURE
/*
 * Callback function invoked by m_apply() to digest TCP segment data
 * contained within an mbuf chain.
 */
static int
tcp_signature_apply(void *fstate, void *data, u_int len)
{

	MD5Update(fstate, (u_char *)data, len);
	return (0);
}

/*
 * Compute TCP-MD5 hash of a TCPv4 segment. (RFC2385)
 *
 * Parameters:
 * m		pointer to head of mbuf chain
 * off0		offset to TCP header within the mbuf chain
 * len		length of TCP segment data, excluding options
 * optlen	length of TCP segment options
 * buf		pointer to storage for computed MD5 digest
 * direction	direction of flow (IPSEC_DIR_INBOUND or OUTBOUND)
 *
 * We do this over ip, tcphdr, segment data, and the key in the SADB.
 * When called from tcp_input(), we can be sure that th_sum has been
 * zeroed out and verified already.
 *
 * This function is for IPv4 use only. Calling this function with an
 * IPv6 packet in the mbuf chain will yield undefined results.
 *
 * Return 0 if successful, otherwise return -1.
 *
 * XXX The key is retrieved from the system's PF_KEY SADB, by keying a
 * search with the destination IP address, and a 'magic SPI' to be
 * determined by the application. This is hardcoded elsewhere to 1179
 * right now. Another branch of this code exists which uses the SPD to
 * specify per-application flows but it is unstable.
 */
int
tcp_signature_compute(struct mbuf *m, int off0, int len, int optlen,
    u_char *buf, u_int direction)
{
	union sockaddr_union dst;
	struct ippseudo ippseudo;
	MD5_CTX ctx;
	int doff;
	struct ip *ip;
	struct ipovly *ipovly;
	struct secasvar *sav;
	struct tcphdr *th;
	u_short savecsum;

	KASSERT(m != NULL, ("NULL mbuf chain"));
	KASSERT(buf != NULL, ("NULL signature pointer"));

	/* Extract the destination from the IP header in the mbuf. */
	ip = mtod(m, struct ip *);
	bzero(&dst, sizeof(union sockaddr_union));
	dst.sa.sa_len = sizeof(struct sockaddr_in);
	dst.sa.sa_family = AF_INET;
	dst.sin.sin_addr = (direction == IPSEC_DIR_INBOUND) ?
	    ip->ip_src : ip->ip_dst;

	/* Look up an SADB entry which matches the address of the peer. */
	sav = KEY_ALLOCSA(&dst, IPPROTO_TCP, htonl(TCP_SIG_SPI));
	if (sav == NULL) {
		printf("%s: SADB lookup failed for %s\n", __func__,
		    inet_ntoa(dst.sin.sin_addr));
		return (EINVAL);
	}

	MD5Init(&ctx);
	ipovly = (struct ipovly *)ip;
	th = (struct tcphdr *)((u_char *)ip + off0);
	doff = off0 + sizeof(struct tcphdr) + optlen;

	/*
	 * Step 1: Update MD5 hash with IP pseudo-header.
	 *
	 * XXX The ippseudo header MUST be digested in network byte order,
	 * or else we'll fail the regression test. Assume all fields we've
	 * been doing arithmetic on have been in host byte order.
	 * XXX One cannot depend on ipovly->ih_len here. When called from
	 * tcp_output(), the underlying ip_len member has not yet been set.
	 */
	ippseudo.ippseudo_src = ipovly->ih_src;
	ippseudo.ippseudo_dst = ipovly->ih_dst;
	ippseudo.ippseudo_pad = 0;
	ippseudo.ippseudo_p = IPPROTO_TCP;
	ippseudo.ippseudo_len = htons(len + sizeof(struct tcphdr) + optlen);
	MD5Update(&ctx, (char *)&ippseudo, sizeof(struct ippseudo));

	/*
	 * Step 2: Update MD5 hash with TCP header, excluding options.
	 * The TCP checksum must be set to zero.
	 */
	savecsum = th->th_sum;
	th->th_sum = 0;
	MD5Update(&ctx, (char *)th, sizeof(struct tcphdr));
	th->th_sum = savecsum;

	/*
	 * Step 3: Update MD5 hash with TCP segment data.
	 *         Use m_apply() to avoid an early m_pullup().
	 */
	if (len > 0)
		m_apply(m, doff, len, tcp_signature_apply, &ctx);

	/*
	 * Step 4: Update MD5 hash with shared secret.
	 */
	MD5Update(&ctx, _KEYBUF(sav->key_auth), _KEYLEN(sav->key_auth));
	MD5Final(buf, &ctx);

	key_sa_recordxfer(sav, m);
	KEY_FREESAV(&sav);
	return (0);
}
#endif /* TCP_SIGNATURE */

static int
sysctl_drop(SYSCTL_HANDLER_ARGS)
{
	/* addrs[0] is a foreign socket, addrs[1] is a local one. */
	struct sockaddr_storage addrs[2];
	struct inpcb *inp;
	struct tcpcb *tp;
	struct tcptw *tw;
	struct sockaddr_in *fin, *lin;
#ifdef INET6
	struct sockaddr_in6 *fin6, *lin6;
	struct in6_addr f6, l6;
#endif
	int error;

	inp = NULL;
	fin = lin = NULL;
#ifdef INET6
	fin6 = lin6 = NULL;
#endif
	error = 0;

	if (req->oldptr != NULL || req->oldlen != 0)
		return (EINVAL);
	if (req->newptr == NULL)
		return (EPERM);
	if (req->newlen < sizeof(addrs))
		return (ENOMEM);
	error = SYSCTL_IN(req, &addrs, sizeof(addrs));
	if (error)
		return (error);

	switch (addrs[0].ss_family) {
#ifdef INET6
	case AF_INET6:
		fin6 = (struct sockaddr_in6 *)&addrs[0];
		lin6 = (struct sockaddr_in6 *)&addrs[1];
		if (fin6->sin6_len != sizeof(struct sockaddr_in6) ||
		    lin6->sin6_len != sizeof(struct sockaddr_in6))
			return (EINVAL);
		if (IN6_IS_ADDR_V4MAPPED(&fin6->sin6_addr)) {
			if (!IN6_IS_ADDR_V4MAPPED(&lin6->sin6_addr))
				return (EINVAL);
			in6_sin6_2_sin_in_sock((struct sockaddr *)&addrs[0]);
			in6_sin6_2_sin_in_sock((struct sockaddr *)&addrs[1]);
			fin = (struct sockaddr_in *)&addrs[0];
			lin = (struct sockaddr_in *)&addrs[1];
			break;
		}
		error = sa6_embedscope(fin6, ip6_use_defzone);
		if (error)
			return (error);
		error = sa6_embedscope(lin6, ip6_use_defzone);
		if (error)
			return (error);
		break;
#endif
	case AF_INET:
		fin = (struct sockaddr_in *)&addrs[0];
		lin = (struct sockaddr_in *)&addrs[1];
		if (fin->sin_len != sizeof(struct sockaddr_in) ||
		    lin->sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}
	INP_INFO_WLOCK(&tcbinfo);
	switch (addrs[0].ss_family) {
#ifdef INET6
	case AF_INET6:
		inp = in6_pcblookup_hash(&tcbinfo, &f6, fin6->sin6_port,
		    &l6, lin6->sin6_port, 0, NULL);
		break;
#endif
	case AF_INET:
		inp = in_pcblookup_hash(&tcbinfo, fin->sin_addr, fin->sin_port,
		    lin->sin_addr, lin->sin_port, 0, NULL);
		break;
	}
	if (inp != NULL) {
		INP_LOCK(inp);
		if (inp->inp_vflag & INP_TIMEWAIT) {
			/*
			 * XXXRW: There currently exists a state where an
			 * inpcb is present, but its timewait state has been
			 * discarded.  For now, don't allow dropping of this
			 * type of inpcb.
			 */
			tw = intotw(inp);
			if (tw != NULL)
				tcp_twclose(tw, 0);
		} else if (!(inp->inp_vflag & INP_DROPPED) &&
			   !(inp->inp_socket->so_options & SO_ACCEPTCONN)) {
			tp = intotcpcb(inp);
			tcp_drop(tp, ECONNABORTED);
		}
		INP_UNLOCK(inp);
	} else
		error = ESRCH;
	INP_INFO_WUNLOCK(&tcbinfo);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_DROP, drop,
    CTLTYPE_STRUCT|CTLFLAG_WR|CTLFLAG_SKIP, NULL,
    0, sysctl_drop, "", "Drop TCP connection");

/*
 * Generate a standardized TCP log line for use throughout the
 * tcp subsystem.  Memory allocation is done with M_NOWAIT to
 * allow use in the interrupt context.
 *
 * NB: The caller MUST free(s, M_TCPLOG) the returned string.
 * NB: The function may return NULL if memory allocation failed.
 *
 * Due to header inclusion and ordering limitations the struct ip
 * and ip6_hdr pointers have to be passed as void pointers.
 */
char *
tcp_log_addrs(struct in_conninfo *inc, struct tcphdr *th, void *ip4hdr,
    const void *ip6hdr)
{
	char *s, *sp;
	size_t size;
	struct ip *ip;
#ifdef INET6
	const struct ip6_hdr *ip6;

	ip6 = (const struct ip6_hdr *)ip6hdr;
#endif /* INET6 */
	ip = (struct ip *)ip4hdr;

	/*
	 * The log line looks like this:
	 * "TCP: [1.2.3.4]:50332 to [1.2.3.4]:80 tcpflags 0x2<SYN>"
	 */
	size = sizeof("TCP: []:12345 to []:12345 tcpflags 0x2<>") +
	    sizeof(PRINT_TH_FLAGS) + 1 +
#ifdef INET6
	    2 * INET6_ADDRSTRLEN;
#else
	    2 * INET_ADDRSTRLEN;
#endif /* INET6 */

	/* Is logging enabled? */
	if (tcp_log_debug == 0 && tcp_log_in_vain == 0)
		return (NULL);

	s = malloc(size, M_TCPLOG, M_ZERO|M_NOWAIT);
	if (s == NULL)
		return (NULL);

	strcat(s, "TCP: [");
	sp = s + strlen(s);

	if (inc && inc->inc_isipv6 == 0) {
		inet_ntoa_r(inc->inc_faddr, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(inc->inc_fport));
		sp = s + strlen(s);
		inet_ntoa_r(inc->inc_laddr, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(inc->inc_lport));
#ifdef INET6
	} else if (inc) {
		ip6_sprintf(sp, &inc->inc6_faddr);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(inc->inc_fport));
		sp = s + strlen(s);
		ip6_sprintf(sp, &inc->inc6_laddr);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(inc->inc_lport));
	} else if (ip6 && th) {
		ip6_sprintf(sp, &ip6->ip6_src);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(th->th_sport));
		sp = s + strlen(s);
		ip6_sprintf(sp, &ip6->ip6_dst);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(th->th_dport));
#endif /* INET6 */
	} else if (ip && th) {
		inet_ntoa_r(ip->ip_src, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(th->th_sport));
		sp = s + strlen(s);
		inet_ntoa_r(ip->ip_dst, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(th->th_dport));
	} else {
		free(s, M_TCPLOG);
		return (NULL);
	}
	sp = s + strlen(s);
	if (th)
		sprintf(sp, " tcpflags 0x%b", th->th_flags, PRINT_TH_FLAGS);
	if (*(s + size - 1) != '\0')
		panic("%s: string too long", __func__);
	return (s);
}
