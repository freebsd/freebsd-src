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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/hhook.h>
#include <sys/kernel.h>
#include <sys/khelp.h>
#include <sys/sysctl.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#ifdef INET6
#include <sys/domain.h>
#endif
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/random.h>

#include <vm/uma.h>

#include <net/route.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <netinet/cc.h>
#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>
#endif

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
#ifdef INET6
#include <netinet6/ip6protosw.h>
#endif
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/key.h>
#include <sys/syslog.h>
#endif /*IPSEC*/

#include <machine/in_cksum.h>
#include <sys/md5.h>

#include <security/mac/mac_framework.h>

VNET_DEFINE(int, tcp_mssdflt) = TCP_MSS;
#ifdef INET6
VNET_DEFINE(int, tcp_v6mssdflt) = TCP6_MSS;
#endif

static int
sysctl_net_inet_tcp_mss_check(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	new = V_tcp_mssdflt;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (new < TCP_MINMSS)
			error = EINVAL;
		else
			V_tcp_mssdflt = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_MSSDFLT, mssdflt,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW, &VNET_NAME(tcp_mssdflt), 0,
    &sysctl_net_inet_tcp_mss_check, "I",
    "Default TCP Maximum Segment Size");

#ifdef INET6
static int
sysctl_net_inet_tcp_mss_v6_check(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	new = V_tcp_v6mssdflt;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (new < TCP_MINMSS)
			error = EINVAL;
		else
			V_tcp_v6mssdflt = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_V6MSSDFLT, v6mssdflt,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW, &VNET_NAME(tcp_v6mssdflt), 0,
    &sysctl_net_inet_tcp_mss_v6_check, "I",
   "Default TCP Maximum Segment Size for IPv6");
#endif /* INET6 */

/*
 * Minimum MSS we accept and use. This prevents DoS attacks where
 * we are forced to a ridiculous low MSS like 20 and send hundreds
 * of packets instead of one. The effect scales with the available
 * bandwidth and quickly saturates the CPU and network interface
 * with packet generation and sending. Set to zero to disable MINMSS
 * checking. This setting prevents us from sending too small packets.
 */
VNET_DEFINE(int, tcp_minmss) = TCP_MINMSS;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, minmss, CTLFLAG_VNET | CTLFLAG_RW,
     &VNET_NAME(tcp_minmss), 0,
    "Minimum TCP Maximum Segment Size");

VNET_DEFINE(int, tcp_do_rfc1323) = 1;
SYSCTL_INT(_net_inet_tcp, TCPCTL_DO_RFC1323, rfc1323, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_do_rfc1323), 0,
    "Enable rfc1323 (high performance TCP) extensions");

static int	tcp_log_debug = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, log_debug, CTLFLAG_RW,
    &tcp_log_debug, 0, "Log errors caused by incoming TCP segments");

static int	tcp_tcbhashsize;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, tcbhashsize, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &tcp_tcbhashsize, 0, "Size of TCP control-block hashtable");

static int	do_tcpdrain = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, do_tcpdrain, CTLFLAG_RW, &do_tcpdrain, 0,
    "Enable tcp_drain routine for extra help when low on mbufs");

SYSCTL_UINT(_net_inet_tcp, OID_AUTO, pcbcount, CTLFLAG_VNET | CTLFLAG_RD,
    &VNET_NAME(tcbinfo.ipi_count), 0, "Number of active PCBs");

static VNET_DEFINE(int, icmp_may_rst) = 1;
#define	V_icmp_may_rst			VNET(icmp_may_rst)
SYSCTL_INT(_net_inet_tcp, OID_AUTO, icmp_may_rst, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(icmp_may_rst), 0,
    "Certain ICMP unreachable messages may abort connections in SYN_SENT");

static VNET_DEFINE(int, tcp_isn_reseed_interval) = 0;
#define	V_tcp_isn_reseed_interval	VNET(tcp_isn_reseed_interval)
SYSCTL_INT(_net_inet_tcp, OID_AUTO, isn_reseed_interval, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_isn_reseed_interval), 0,
    "Seconds between reseeding of ISN secret");

static int	tcp_soreceive_stream;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, soreceive_stream, CTLFLAG_RDTUN,
    &tcp_soreceive_stream, 0, "Using soreceive_stream for TCP sockets");

#ifdef TCP_SIGNATURE
static int	tcp_sig_checksigs = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, signature_verify_input, CTLFLAG_RW,
    &tcp_sig_checksigs, 0, "Verify RFC2385 digests on inbound traffic");
#endif

VNET_DEFINE(uma_zone_t, sack_hole_zone);
#define	V_sack_hole_zone		VNET(sack_hole_zone)

VNET_DEFINE(struct hhook_head *, tcp_hhh[HHOOK_TCP_LAST+1]);

static struct inpcb *tcp_notify(struct inpcb *, int);
static struct inpcb *tcp_mtudisc_notify(struct inpcb *, int);
static char *	tcp_log_addr(struct in_conninfo *inc, struct tcphdr *th,
		    void *ip4hdr, const void *ip6hdr);
static void	tcp_timer_discard(struct tcpcb *, uint32_t);

/*
 * Target size of TCP PCB hash tables. Must be a power of two.
 *
 * Note that this can be overridden by the kernel environment
 * variable net.inet.tcp.tcbhashsize
 */
#ifndef TCBHASHSIZE
#define TCBHASHSIZE	0
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
	struct	cc_var		ccv;
	struct	osd		osd;
};

static VNET_DEFINE(uma_zone_t, tcpcb_zone);
#define	V_tcpcb_zone			VNET(tcpcb_zone)

MALLOC_DEFINE(M_TCPLOG, "tcplog", "TCP address and flags print buffers");
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

	uma_zone_set_max(V_tcbinfo.ipi_zone, maxsockets);
	uma_zone_set_max(V_tcpcb_zone, maxsockets);
	tcp_tw_zone_change();
}

static int
tcp_inpcb_init(void *mem, int size, int flags)
{
	struct inpcb *inp = mem;

	INP_LOCK_INIT(inp, "inp", "tcpinp");
	return (0);
}

/*
 * Take a value and get the next power of 2 that doesn't overflow.
 * Used to size the tcp_inpcb hash buckets.
 */
static int
maketcp_hashsize(int size)
{
	int hashsize;

	/*
	 * auto tune.
	 * get the next power of 2 higher than maxsockets.
	 */
	hashsize = 1 << fls(size);
	/* catch overflow, and just go one power of 2 smaller */
	if (hashsize < size) {
		hashsize = 1 << (fls(size) - 1);
	}
	return (hashsize);
}

void
tcp_init(void)
{
	const char *tcbhash_tuneable;
	int hashsize;

	tcbhash_tuneable = "net.inet.tcp.tcbhashsize";

	if (hhook_head_register(HHOOK_TYPE_TCP, HHOOK_TCP_EST_IN,
	    &V_tcp_hhh[HHOOK_TCP_EST_IN], HHOOK_NOWAIT|HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register helper hook\n", __func__);
	if (hhook_head_register(HHOOK_TYPE_TCP, HHOOK_TCP_EST_OUT,
	    &V_tcp_hhh[HHOOK_TCP_EST_OUT], HHOOK_NOWAIT|HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register helper hook\n", __func__);

	hashsize = TCBHASHSIZE;
	TUNABLE_INT_FETCH(tcbhash_tuneable, &hashsize);
	if (hashsize == 0) {
		/*
		 * Auto tune the hash size based on maxsockets.
		 * A perfect hash would have a 1:1 mapping
		 * (hashsize = maxsockets) however it's been
		 * suggested that O(2) average is better.
		 */
		hashsize = maketcp_hashsize(maxsockets / 4);
		/*
		 * Our historical default is 512,
		 * do not autotune lower than this.
		 */
		if (hashsize < 512)
			hashsize = 512;
		if (bootverbose)
			printf("%s: %s auto tuned to %d\n", __func__,
			    tcbhash_tuneable, hashsize);
	}
	/*
	 * We require a hashsize to be a power of two.
	 * Previously if it was not a power of two we would just reset it
	 * back to 512, which could be a nasty surprise if you did not notice
	 * the error message.
	 * Instead what we do is clip it to the closest power of two lower
	 * than the specified hash value.
	 */
	if (!powerof2(hashsize)) {
		int oldhashsize = hashsize;

		hashsize = maketcp_hashsize(hashsize);
		/* prevent absurdly low value */
		if (hashsize < 16)
			hashsize = 16;
		printf("%s: WARNING: TCB hash size not a power of 2, "
		    "clipped from %d to %d.\n", __func__, oldhashsize,
		    hashsize);
	}
	in_pcbinfo_init(&V_tcbinfo, "tcp", &V_tcb, hashsize, hashsize,
	    "tcp_inpcb", tcp_inpcb_init, NULL, UMA_ZONE_NOFREE,
	    IPI_HASHFIELDS_4TUPLE);

	/*
	 * These have to be type stable for the benefit of the timers.
	 */
	V_tcpcb_zone = uma_zcreate("tcpcb", sizeof(struct tcpcb_mem),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(V_tcpcb_zone, maxsockets);
	uma_zone_set_warning(V_tcpcb_zone, "kern.ipc.maxsockets limit reached");

	tcp_tw_init();
	syncache_init();
	tcp_hc_init();

	TUNABLE_INT_FETCH("net.inet.tcp.sack.enable", &V_tcp_do_sack);
	V_sack_hole_zone = uma_zcreate("sackhole", sizeof(struct sackhole),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);

	/* Skip initialization of globals for non-default instances. */
	if (!IS_DEFAULT_VNET(curvnet))
		return;

	tcp_reass_global_init();

	/* XXX virtualize those bellow? */
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
	tcp_finwait2_timeout = TCPTV_FINWAIT2_TIMEOUT;
	tcp_tcbhashsize = hashsize;

	if (tcp_soreceive_stream) {
#ifdef INET
		tcp_usrreqs.pru_soreceive = soreceive_stream;
#endif
#ifdef INET6
		tcp6_usrreqs.pru_soreceive = soreceive_stream;
#endif /* INET6 */
	}

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

	ISN_LOCK_INIT();
	EVENTHANDLER_REGISTER(shutdown_pre_sync, tcp_fini, NULL,
		SHUTDOWN_PRI_DEFAULT);
	EVENTHANDLER_REGISTER(maxsockets_change, tcp_zone_change, NULL,
		EVENTHANDLER_PRI_ANY);
}

#ifdef VIMAGE
void
tcp_destroy(void)
{
	int error;

	tcp_hc_destroy();
	syncache_destroy();
	tcp_tw_destroy();
	in_pcbinfo_destroy(&V_tcbinfo);
	uma_zdestroy(V_sack_hole_zone);
	uma_zdestroy(V_tcpcb_zone);

	error = hhook_head_deregister(V_tcp_hhh[HHOOK_TCP_EST_IN]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister helper hook "
		    "type=%d, id=%d: error %d returned\n", __func__,
		    HHOOK_TYPE_TCP, HHOOK_TCP_EST_IN, error);
	}
	error = hhook_head_deregister(V_tcp_hhh[HHOOK_TCP_EST_OUT]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister helper hook "
		    "type=%d, id=%d: error %d returned\n", __func__,
		    HHOOK_TYPE_TCP, HHOOK_TCP_EST_OUT, error);
	}
}
#endif

void
tcp_fini(void *xtp)
{

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

	INP_WLOCK_ASSERT(inp);

#ifdef INET6
	if ((inp->inp_vflag & INP_IPV6) != 0) {
		struct ip6_hdr *ip6;

		ip6 = (struct ip6_hdr *)ip_ptr;
		ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
			(inp->inp_flow & IPV6_FLOWINFO_MASK);
		ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
			(IPV6_VERSION & IPV6_VERSION_MASK);
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_src = inp->in6p_laddr;
		ip6->ip6_dst = inp->in6p_faddr;
	}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
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
#endif /* INET */
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
	struct tcptemp *t;

	t = malloc(sizeof(*t), M_TEMP, M_NOWAIT);
	if (t == NULL)
		return (NULL);
	tcpip_fillheaders(inp, (void *)&t->tt_ipgen, (void *)&t->tt_t);
	return (t);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == NULL, then we make a copy
 * of the tcpiphdr at th and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection.  If flags are given then we send
 * a message back to the TCP which originated the segment th,
 * and discard the mbuf containing it and any other attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 *
 * NOTE: If m != NULL, then th must point to *inside* the mbuf.
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
	isipv6 = ((struct ip *)ipgen)->ip_v == (IPV6_VERSION >> 4);
	ip6 = ipgen;
#endif /* INET6 */
	ip = ipgen;

	if (tp != NULL) {
		inp = tp->t_inpcb;
		KASSERT(inp != NULL, ("tcp control block w/o inpcb"));
		INP_WLOCK_ASSERT(inp);
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
		m = m_gethdr(M_NOWAIT, MT_DATA);
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
		/*
		 *  reuse the mbuf. 
		 * XXX MRT We inherrit the FIB, which is lucky.
		 */
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
			xchg(ip->ip_dst.s_addr, ip->ip_src.s_addr, uint32_t);
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
		xchg(nth->th_dport, nth->th_sport, uint16_t);
#undef xchg
	}
#ifdef INET6
	if (isipv6) {
		ip6->ip6_flow = 0;
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_nxt = IPPROTO_TCP;
		tlen += sizeof (struct ip6_hdr) + sizeof (struct tcphdr);
		ip6->ip6_plen = htons(tlen - sizeof(*ip6));
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		tlen += sizeof (struct tcpiphdr);
		ip->ip_len = htons(tlen);
		ip->ip_ttl = V_ip_defttl;
		if (V_path_mtu_discovery)
			ip->ip_off |= htons(IP_DF);
	}
#endif
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;
#ifdef MAC
	if (inp != NULL) {
		/*
		 * Packet is associated with a socket, so allow the
		 * label of the response to reflect the socket label.
		 */
		INP_WLOCK_ASSERT(inp);
		mac_inpcb_create_mbuf(inp, m);
	} else {
		/*
		 * Packet is not associated with a socket, so possibly
		 * update the label in place.
		 */
		mac_netinet_tcp_reply(m);
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

	m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
#ifdef INET6
	if (isipv6) {
		m->m_pkthdr.csum_flags = CSUM_TCP_IPV6;
		nth->th_sum = in6_cksum_pseudo(ip6,
		    tlen - sizeof(struct ip6_hdr), IPPROTO_TCP, 0);
		ip6->ip6_hlim = in6_selecthlim(tp != NULL ? tp->t_inpcb :
		    NULL, NULL);
	}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		m->m_pkthdr.csum_flags = CSUM_TCP;
		nth->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons((u_short)(tlen - sizeof(struct ip) + ip->ip_p)));
	}
#endif /* INET */
#ifdef TCPDEBUG
	if (tp == NULL || (inp->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_OUTPUT, 0, tp, mtod(m, void *), th, 0);
#endif
	if (flags & TH_RST)
		TCP_PROBE5(accept__refused, NULL, NULL, mtod(m, const char *),
		    tp, nth);

	TCP_PROBE5(send, NULL, tp, mtod(m, const char *), tp, nth);
#ifdef INET6
	if (isipv6)
		(void) ip6_output(m, NULL, NULL, ipflags, NULL, NULL, inp);
#endif /* INET6 */
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
		(void) ip_output(m, NULL, NULL, ipflags, NULL, inp);
#endif
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

	tm = uma_zalloc(V_tcpcb_zone, M_NOWAIT | M_ZERO);
	if (tm == NULL)
		return (NULL);
	tp = &tm->tcb;

	/* Initialise cc_var struct for this tcpcb. */
	tp->ccv = &tm->ccv;
	tp->ccv->type = IPPROTO_TCP;
	tp->ccv->ccvc.tcp = tp;

	/*
	 * Use the current system default CC algorithm.
	 */
	CC_LIST_RLOCK();
	KASSERT(!STAILQ_EMPTY(&cc_list), ("cc_list is empty!"));
	CC_ALGO(tp) = CC_DEFAULT();
	CC_LIST_RUNLOCK();

	if (CC_ALGO(tp)->cb_init != NULL)
		if (CC_ALGO(tp)->cb_init(tp->ccv) > 0) {
			uma_zfree(V_tcpcb_zone, tm);
			return (NULL);
		}

	tp->osd = &tm->osd;
	if (khelp_init_osd(HELPER_CLASS_TCP, tp->osd)) {
		uma_zfree(V_tcpcb_zone, tm);
		return (NULL);
	}

#ifdef VIMAGE
	tp->t_vnet = inp->inp_vnet;
#endif
	tp->t_timers = &tm->tt;
	/*	LIST_INIT(&tp->t_segq); */	/* XXX covered by M_ZERO */
	tp->t_maxseg = tp->t_maxopd =
#ifdef INET6
		isipv6 ? V_tcp_v6mssdflt :
#endif /* INET6 */
		V_tcp_mssdflt;

	/* Set up our timeouts. */
	callout_init(&tp->t_timers->tt_rexmt, 1);
	callout_init(&tp->t_timers->tt_persist, 1);
	callout_init(&tp->t_timers->tt_keep, 1);
	callout_init(&tp->t_timers->tt_2msl, 1);
	callout_init(&tp->t_timers->tt_delack, 1);

	if (V_tcp_do_rfc1323)
		tp->t_flags = (TF_REQ_SCALE|TF_REQ_TSTMP);
	if (V_tcp_do_sack)
		tp->t_flags |= TF_SACK_PERMIT;
	TAILQ_INIT(&tp->snd_holes);
	/*
	 * The tcpcb will hold a reference on its inpcb until tcp_discardcb()
	 * is called.
	 */
	in_pcbref(inp);	/* Reference for tcpcb */
	tp->t_inpcb = inp;

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
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->t_rcvtime = ticks;
	/*
	 * IPv4 TTL initialization is necessary for an IPv6 socket as well,
	 * because the socket may be bound to an IPv6 wildcard address,
	 * which may match an IPv4-mapped IPv6 address.
	 */
	inp->inp_ip_ttl = V_ip_defttl;
	inp->inp_ppcb = tp;
	return (tp);		/* XXX */
}

/*
 * Switch the congestion control algorithm back to NewReno for any active
 * control blocks using an algorithm which is about to go away.
 * This ensures the CC framework can allow the unload to proceed without leaving
 * any dangling pointers which would trigger a panic.
 * Returning non-zero would inform the CC framework that something went wrong
 * and it would be unsafe to allow the unload to proceed. However, there is no
 * way for this to occur with this implementation so we always return zero.
 */
int
tcp_ccalgounload(struct cc_algo *unload_algo)
{
	struct cc_algo *tmpalgo;
	struct inpcb *inp;
	struct tcpcb *tp;
	VNET_ITERATOR_DECL(vnet_iter);

	/*
	 * Check all active control blocks across all network stacks and change
	 * any that are using "unload_algo" back to NewReno. If "unload_algo"
	 * requires cleanup code to be run, call it.
	 */
	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		INP_INFO_WLOCK(&V_tcbinfo);
		/*
		 * New connections already part way through being initialised
		 * with the CC algo we're removing will not race with this code
		 * because the INP_INFO_WLOCK is held during initialisation. We
		 * therefore don't enter the loop below until the connection
		 * list has stabilised.
		 */
		LIST_FOREACH(inp, &V_tcb, inp_list) {
			INP_WLOCK(inp);
			/* Important to skip tcptw structs. */
			if (!(inp->inp_flags & INP_TIMEWAIT) &&
			    (tp = intotcpcb(inp)) != NULL) {
				/*
				 * By holding INP_WLOCK here, we are assured
				 * that the connection is not currently
				 * executing inside the CC module's functions
				 * i.e. it is safe to make the switch back to
				 * NewReno.
				 */
				if (CC_ALGO(tp) == unload_algo) {
					tmpalgo = CC_ALGO(tp);
					/* NewReno does not require any init. */
					CC_ALGO(tp) = &newreno_cc_algo;
					if (tmpalgo->cb_destroy != NULL)
						tmpalgo->cb_destroy(tp->ccv);
				}
			}
			INP_WUNLOCK(inp);
		}
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK();

	return (0);
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

	INP_INFO_LOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tcp_state_change(tp, TCPS_CLOSED);
		(void) tcp_output(tp);
		TCPSTAT_INC(tcps_drops);
	} else
		TCPSTAT_INC(tcps_conndrops);
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

void
tcp_discardcb(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
#ifdef INET6
	int isipv6 = (inp->inp_vflag & INP_IPV6) != 0;
#endif /* INET6 */
	int released;

	INP_WLOCK_ASSERT(inp);

	/*
	 * Make sure that all of our timers are stopped before we delete the
	 * PCB.
	 *
	 * If stopping a timer fails, we schedule a discard function in same
	 * callout, and the last discard function called will take care of
	 * deleting the tcpcb.
	 */
	tcp_timer_stop(tp, TT_REXMT);
	tcp_timer_stop(tp, TT_PERSIST);
	tcp_timer_stop(tp, TT_KEEP);
	tcp_timer_stop(tp, TT_2MSL);
	tcp_timer_stop(tp, TT_DELACK);

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
		metrics.rmx_cwnd = tp->snd_cwnd;
		metrics.rmx_sendpipe = 0;
		metrics.rmx_recvpipe = 0;

		tcp_hc_update(&inp->inp_inc, &metrics);
	}

	/* free the reassembly queue, if any */
	tcp_reass_flush(tp);

#ifdef TCP_OFFLOAD
	/* Disconnect offload device, if any. */
	if (tp->t_flags & TF_TOE)
		tcp_offload_detach(tp);
#endif
		
	tcp_free_sackholes(tp);

	/* Allow the CC algorithm to clean up after itself. */
	if (CC_ALGO(tp)->cb_destroy != NULL)
		CC_ALGO(tp)->cb_destroy(tp->ccv);

	khelp_destroy_osd(tp->osd);

	CC_ALGO(tp) = NULL;
	inp->inp_ppcb = NULL;
	if ((tp->t_timers->tt_flags & TT_MASK) == 0) {
		/* We own the last reference on tcpcb, let's free it. */
		tp->t_inpcb = NULL;
		uma_zfree(V_tcpcb_zone, tp);
		released = in_pcbrele_wlocked(inp);
		KASSERT(!released, ("%s: inp %p should not have been released "
			"here", __func__, inp));
	}
}

void
tcp_timer_2msl_discard(void *xtp)
{

	tcp_timer_discard((struct tcpcb *)xtp, TT_2MSL);
}

void
tcp_timer_keep_discard(void *xtp)
{

	tcp_timer_discard((struct tcpcb *)xtp, TT_KEEP);
}

void
tcp_timer_persist_discard(void *xtp)
{

	tcp_timer_discard((struct tcpcb *)xtp, TT_PERSIST);
}

void
tcp_timer_rexmt_discard(void *xtp)
{

	tcp_timer_discard((struct tcpcb *)xtp, TT_REXMT);
}

void
tcp_timer_delack_discard(void *xtp)
{

	tcp_timer_discard((struct tcpcb *)xtp, TT_DELACK);
}

void
tcp_timer_discard(struct tcpcb *tp, uint32_t timer_type)
{
	struct inpcb *inp;

	CURVNET_SET(tp->t_vnet);
	INP_INFO_RLOCK(&V_tcbinfo);
	inp = tp->t_inpcb;
	KASSERT(inp != NULL, ("%s: tp %p tp->t_inpcb == NULL",
		__func__, tp));
	INP_WLOCK(inp);
	KASSERT((tp->t_timers->tt_flags & TT_STOPPED) != 0,
		("%s: tcpcb has to be stopped here", __func__));
	KASSERT((tp->t_timers->tt_flags & timer_type) != 0,
		("%s: discard callout should be running", __func__));
	tp->t_timers->tt_flags &= ~timer_type;
	if ((tp->t_timers->tt_flags & TT_MASK) == 0) {
		/* We own the last reference on this tcpcb, let's free it. */
		tp->t_inpcb = NULL;
		uma_zfree(V_tcpcb_zone, tp);
		if (in_pcbrele_wlocked(inp)) {
			INP_INFO_RUNLOCK(&V_tcbinfo);
			CURVNET_RESTORE();
			return;
		}
	}
	INP_WUNLOCK(inp);
	INP_INFO_RUNLOCK(&V_tcbinfo);
	CURVNET_RESTORE();
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

	INP_INFO_LOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(inp);

#ifdef TCP_OFFLOAD
	if (tp->t_state == TCPS_LISTEN)
		tcp_offload_listen_stop(tp);
#endif
	in_pcbdrop(inp);
	TCPSTAT_INC(tcps_closed);
	KASSERT(inp->inp_socket != NULL, ("tcp_close: inp_socket NULL"));
	so = inp->inp_socket;
	soisdisconnected(so);
	if (inp->inp_flags & INP_SOCKREF) {
		KASSERT(so->so_state & SS_PROTOREF,
		    ("tcp_close: !SS_PROTOREF"));
		inp->inp_flags &= ~INP_SOCKREF;
		INP_WUNLOCK(inp);
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
	VNET_ITERATOR_DECL(vnet_iter);

	if (!do_tcpdrain)
		return;

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		struct inpcb *inpb;
		struct tcpcb *tcpb;

	/*
	 * Walk the tcpbs, if existing, and flush the reassembly queue,
	 * if there is one...
	 * XXX: The "Net/3" implementation doesn't imply that the TCP
	 *      reassembly queue should be flushed, but in a situation
	 *	where we're really low on mbufs, this is potentially
	 *	useful.
	 */
		INP_INFO_WLOCK(&V_tcbinfo);
		LIST_FOREACH(inpb, V_tcbinfo.ipi_listhead, inp_list) {
			if (inpb->inp_flags & INP_TIMEWAIT)
				continue;
			INP_WLOCK(inpb);
			if ((tcpb = intotcpcb(inpb)) != NULL) {
				tcp_reass_flush(tcpb);
				tcp_clean_sackreport(tcpb);
			}
			INP_WUNLOCK(inpb);
		}
		INP_INFO_WUNLOCK(&V_tcbinfo);
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
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

	INP_INFO_LOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(inp);

	if ((inp->inp_flags & INP_TIMEWAIT) ||
	    (inp->inp_flags & INP_DROPPED))
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
		n = V_tcbinfo.ipi_count + syncache_pcbcount();
		n += imax(n / 8, 10);
		req->oldidx = 2 * (sizeof xig) + n * sizeof(struct xtcpcb);
		return (0);
	}

	if (req->newptr != NULL)
		return (EPERM);

	/*
	 * OK, now we're committed to doing something.
	 */
	INP_LIST_RLOCK(&V_tcbinfo);
	gencnt = V_tcbinfo.ipi_gencnt;
	n = V_tcbinfo.ipi_count;
	INP_LIST_RUNLOCK(&V_tcbinfo);

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

	INP_INFO_WLOCK(&V_tcbinfo);
	for (inp = LIST_FIRST(V_tcbinfo.ipi_listhead), i = 0;
	    inp != NULL && i < n; inp = LIST_NEXT(inp, inp_list)) {
		INP_WLOCK(inp);
		if (inp->inp_gencnt <= gencnt) {
			/*
			 * XXX: This use of cr_cansee(), introduced with
			 * TCP state changes, is not quite right, but for
			 * now, better than nothing.
			 */
			if (inp->inp_flags & INP_TIMEWAIT) {
				if (intotw(inp) != NULL)
					error = cr_cansee(req->td->td_ucred,
					    intotw(inp)->tw_cred);
				else
					error = EINVAL;	/* Skip this inp. */
			} else
				error = cr_canseeinpcb(req->td->td_ucred, inp);
			if (error == 0) {
				in_pcbref(inp);
				inp_list[i++] = inp;
			}
		}
		INP_WUNLOCK(inp);
	}
	INP_INFO_WUNLOCK(&V_tcbinfo);
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_RLOCK(inp);
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
			else if (inp->inp_flags & INP_TIMEWAIT) {
				bzero((char *) &xt.xt_tp, sizeof xt.xt_tp);
				xt.xt_tp.t_state = TCPS_TIME_WAIT;
			} else {
				bcopy(inp_ppcb, &xt.xt_tp, sizeof xt.xt_tp);
				if (xt.xt_tp.t_timers)
					tcp_timer_to_xtimer(&xt.xt_tp, xt.xt_tp.t_timers, &xt.xt_timer);
			}
			if (inp->inp_socket != NULL)
				sotoxsocket(inp->inp_socket, &xt.xt_socket);
			else {
				bzero(&xt.xt_socket, sizeof xt.xt_socket);
				xt.xt_socket.xso_protocol = IPPROTO_TCP;
			}
			xt.xt_inp.inp_gencnt = inp->inp_gencnt;
			INP_RUNLOCK(inp);
			error = SYSCTL_OUT(req, &xt, sizeof xt);
		} else
			INP_RUNLOCK(inp);
	}
	INP_INFO_RLOCK(&V_tcbinfo);
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_RLOCK(inp);
		if (!in_pcbrele_rlocked(inp))
			INP_RUNLOCK(inp);
	}
	INP_INFO_RUNLOCK(&V_tcbinfo);

	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		INP_LIST_RLOCK(&V_tcbinfo);
		xig.xig_gen = V_tcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = V_tcbinfo.ipi_count + pcb_count;
		INP_LIST_RUNLOCK(&V_tcbinfo);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}
	free(inp_list, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_PCBLIST, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD, NULL, 0,
    tcp_pcblist, "S,xtcpcb", "List of active TCP connections");

#ifdef INET
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
	inp = in_pcblookup(&V_tcbinfo, addrs[1].sin_addr, addrs[1].sin_port,
	    addrs[0].sin_addr, addrs[0].sin_port, INPLOOKUP_RLOCKPCB, NULL);
	if (inp != NULL) {
		if (inp->inp_socket == NULL)
			error = ENOENT;
		if (error == 0)
			error = cr_canseeinpcb(req->td->td_ucred, inp);
		if (error == 0)
			cru2x(inp->inp_cred, &xuc);
		INP_RUNLOCK(inp);
	} else
		error = ENOENT;
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, getcred,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0,
    tcp_getcred, "S,xucred", "Get the xucred of a TCP connection");
#endif /* INET */

#ifdef INET6
static int
tcp6_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in6 addrs[2];
	struct inpcb *inp;
	int error;
#ifdef INET
	int mapped = 0;
#endif

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	if ((error = sa6_embedscope(&addrs[0], V_ip6_use_defzone)) != 0 ||
	    (error = sa6_embedscope(&addrs[1], V_ip6_use_defzone)) != 0) {
		return (error);
	}
	if (IN6_IS_ADDR_V4MAPPED(&addrs[0].sin6_addr)) {
#ifdef INET
		if (IN6_IS_ADDR_V4MAPPED(&addrs[1].sin6_addr))
			mapped = 1;
		else
#endif
			return (EINVAL);
	}

#ifdef INET
	if (mapped == 1)
		inp = in_pcblookup(&V_tcbinfo,
			*(struct in_addr *)&addrs[1].sin6_addr.s6_addr[12],
			addrs[1].sin6_port,
			*(struct in_addr *)&addrs[0].sin6_addr.s6_addr[12],
			addrs[0].sin6_port, INPLOOKUP_RLOCKPCB, NULL);
	else
#endif
		inp = in6_pcblookup(&V_tcbinfo,
			&addrs[1].sin6_addr, addrs[1].sin6_port,
			&addrs[0].sin6_addr, addrs[0].sin6_port,
			INPLOOKUP_RLOCKPCB, NULL);
	if (inp != NULL) {
		if (inp->inp_socket == NULL)
			error = ENOENT;
		if (error == 0)
			error = cr_canseeinpcb(req->td->td_ucred, inp);
		if (error == 0)
			cru2x(inp->inp_cred, &xuc);
		INP_RUNLOCK(inp);
	} else
		error = ENOENT;
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet6_tcp6, OID_AUTO, getcred,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0,
    tcp6_getcred, "S,xucred", "Get the xucred of a TCP6 connection");
#endif /* INET6 */


#ifdef INET
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
		notify = tcp_mtudisc_notify;
	else if (V_icmp_may_rst && (cmd == PRC_UNREACH_ADMIN_PROHIB ||
		cmd == PRC_UNREACH_PORT || cmd == PRC_TIMXCEED_INTRANS) && ip)
		notify = tcp_drop_syn_sent;
	/*
	 * Redirects don't need to be handled up here.
	 */
	else if (PRC_IS_REDIRECT(cmd))
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
		INP_INFO_RLOCK(&V_tcbinfo);
		inp = in_pcblookup(&V_tcbinfo, faddr, th->th_dport,
		    ip->ip_src, th->th_sport, INPLOOKUP_WLOCKPCB, NULL);
		if (inp != NULL)  {
			if (!(inp->inp_flags & INP_TIMEWAIT) &&
			    !(inp->inp_flags & INP_DROPPED) &&
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
					    inc.inc_faddr = faddr;
					    inc.inc_fibnum =
						inp->inp_inc.inc_fibnum;

					    mtu = ntohs(icp->icmp_nextmtu);
					    /*
					     * If no alternative MTU was
					     * proposed, try the next smaller
					     * one.
					     */
					    if (!mtu)
						mtu = ip_next_mtu(
						 ntohs(ip->ip_len), 1);
					    if (mtu < V_tcp_minmss
						 + sizeof(struct tcpiphdr))
						mtu = V_tcp_minmss
						 + sizeof(struct tcpiphdr);
					    /*
					     * Only cache the MTU if it
					     * is smaller than the interface
					     * or route MTU.  tcp_mtudisc()
					     * will do right thing by itself.
					     */
					    if (mtu <= tcp_maxmtu(&inc, NULL))
						tcp_hc_updatemtu(&inc, mtu);
					    tcp_mtudisc(inp, mtu);
					} else
						inp = (*notify)(inp,
						    inetctlerrmap[cmd]);
				}
			}
			if (inp != NULL)
				INP_WUNLOCK(inp);
		} else {
			bzero(&inc, sizeof(inc));
			inc.inc_fport = th->th_dport;
			inc.inc_lport = th->th_sport;
			inc.inc_faddr = faddr;
			inc.inc_laddr = ip->ip_src;
			syncache_unreach(&inc, th);
		}
		INP_INFO_RUNLOCK(&V_tcbinfo);
	} else
		in_pcbnotifyall(&V_tcbinfo, faddr, inetctlerrmap[cmd], notify);
}
#endif /* INET */

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
		notify = tcp_mtudisc_notify;
	else if (!PRC_IS_REDIRECT(cmd) &&
		 ((unsigned)cmd >= PRC_NCMDS || inet6ctlerrmap[cmd] == 0))
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

		in6_pcbnotify(&V_tcbinfo, sa, th.th_dport,
		    (struct sockaddr *)ip6cp->ip6c_src,
		    th.th_sport, cmd, NULL, notify);

		bzero(&inc, sizeof(inc));
		inc.inc_fport = th.th_dport;
		inc.inc_lport = th.th_sport;
		inc.inc6_faddr = ((struct sockaddr_in6 *)sa)->sin6_addr;
		inc.inc6_laddr = ip6cp->ip6c_src->sin6_addr;
		inc.inc_flags |= INC_ISIPV6;
		INP_INFO_RLOCK(&V_tcbinfo);
		syncache_unreach(&inc, &th);
		INP_INFO_RUNLOCK(&V_tcbinfo);
	} else
		in6_pcbnotify(&V_tcbinfo, sa, 0, (const struct sockaddr *)sa6_src,
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

static VNET_DEFINE(u_char, isn_secret[32]);
static VNET_DEFINE(int, isn_last);
static VNET_DEFINE(int, isn_last_reseed);
static VNET_DEFINE(u_int32_t, isn_offset);
static VNET_DEFINE(u_int32_t, isn_offset_old);

#define	V_isn_secret			VNET(isn_secret)
#define	V_isn_last			VNET(isn_last)
#define	V_isn_last_reseed		VNET(isn_last_reseed)
#define	V_isn_offset			VNET(isn_offset)
#define	V_isn_offset_old		VNET(isn_offset_old)

tcp_seq
tcp_new_isn(struct tcpcb *tp)
{
	MD5_CTX isn_ctx;
	u_int32_t md5_buffer[4];
	tcp_seq new_isn;
	u_int32_t projected_offset;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	ISN_LOCK();
	/* Seed if this is the first use, reseed if requested. */
	if ((V_isn_last_reseed == 0) || ((V_tcp_isn_reseed_interval > 0) &&
	     (((u_int)V_isn_last_reseed + (u_int)V_tcp_isn_reseed_interval*hz)
		< (u_int)ticks))) {
		read_random(&V_isn_secret, sizeof(V_isn_secret));
		V_isn_last_reseed = ticks;
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
	MD5Update(&isn_ctx, (u_char *) &V_isn_secret, sizeof(V_isn_secret));
	MD5Final((u_char *) &md5_buffer, &isn_ctx);
	new_isn = (tcp_seq) md5_buffer[0];
	V_isn_offset += ISN_STATIC_INCREMENT +
		(arc4random() & ISN_RANDOM_INCREMENT);
	if (ticks != V_isn_last) {
		projected_offset = V_isn_offset_old +
		    ISN_BYTES_PER_SECOND / hz * (ticks - V_isn_last);
		if (SEQ_GT(projected_offset, V_isn_offset))
			V_isn_offset = projected_offset;
		V_isn_offset_old = V_isn_offset;
		V_isn_last = ticks;
	}
	new_isn += V_isn_offset;
	ISN_UNLOCK();
	return (new_isn);
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

	INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(inp);

	if ((inp->inp_flags & INP_TIMEWAIT) ||
	    (inp->inp_flags & INP_DROPPED))
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
 * based on the new value. Also nudge TCP to send something, since we
 * know the packet we just sent was dropped.
 * This duplicates some code in the tcp_mss() function in tcp_input.c.
 */
static struct inpcb *
tcp_mtudisc_notify(struct inpcb *inp, int error)
{

	return (tcp_mtudisc(inp, -1));
}

struct inpcb *
tcp_mtudisc(struct inpcb *inp, int mtuoffer)
{
	struct tcpcb *tp;
	struct socket *so;

	INP_WLOCK_ASSERT(inp);
	if ((inp->inp_flags & INP_TIMEWAIT) ||
	    (inp->inp_flags & INP_DROPPED))
		return (inp);

	tp = intotcpcb(inp);
	KASSERT(tp != NULL, ("tcp_mtudisc: tp == NULL"));

	tcp_mss_update(tp, -1, mtuoffer, NULL, NULL);
  
	so = inp->inp_socket;
	SOCKBUF_LOCK(&so->so_snd);
	/* If the mss is larger than the socket buffer, decrease the mss. */
	if (so->so_snd.sb_hiwat < tp->t_maxseg)
		tp->t_maxseg = so->so_snd.sb_hiwat;
	SOCKBUF_UNLOCK(&so->so_snd);

	TCPSTAT_INC(tcps_mturesent);
	tp->t_rtttime = 0;
	tp->snd_nxt = tp->snd_una;
	tcp_free_sackholes(tp);
	tp->snd_recover = tp->snd_max;
	if (tp->t_flags & TF_SACK_PERMIT)
		EXIT_FASTRECOVERY(tp->t_flags);
	tcp_output(tp);
	return (inp);
}

#ifdef INET
/*
 * Look-up the routing entry to the peer of this inpcb.  If no route
 * is found and it cannot be allocated, then return 0.  This routine
 * is called by TCP routines that access the rmx structure and by
 * tcp_mss_update to get the peer/interface MTU.
 */
u_long
tcp_maxmtu(struct in_conninfo *inc, struct tcp_ifcap *cap)
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
		in_rtalloc_ign(&sro, 0, inc->inc_fibnum);
	}
	if (sro.ro_rt != NULL) {
		ifp = sro.ro_rt->rt_ifp;
		if (sro.ro_rt->rt_mtu == 0)
			maxmtu = ifp->if_mtu;
		else
			maxmtu = min(sro.ro_rt->rt_mtu, ifp->if_mtu);

		/* Report additional interface capabilities. */
		if (cap != NULL) {
			if (ifp->if_capenable & IFCAP_TSO4 &&
			    ifp->if_hwassist & CSUM_TSO) {
				cap->ifcap |= CSUM_TSO;
				cap->tsomax = ifp->if_hw_tsomax;
				cap->tsomaxsegcount = ifp->if_hw_tsomaxsegcount;
				cap->tsomaxsegsize = ifp->if_hw_tsomaxsegsize;
			}
		}
		RTFREE(sro.ro_rt);
	}
	return (maxmtu);
}
#endif /* INET */

#ifdef INET6
u_long
tcp_maxmtu6(struct in_conninfo *inc, struct tcp_ifcap *cap)
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
		in6_rtalloc_ign(&sro6, 0, inc->inc_fibnum);
	}
	if (sro6.ro_rt != NULL) {
		ifp = sro6.ro_rt->rt_ifp;
		if (sro6.ro_rt->rt_mtu == 0)
			maxmtu = IN6_LINKMTU(sro6.ro_rt->rt_ifp);
		else
			maxmtu = min(sro6.ro_rt->rt_mtu,
				     IN6_LINKMTU(sro6.ro_rt->rt_ifp));

		/* Report additional interface capabilities. */
		if (cap != NULL) {
			if (ifp->if_capenable & IFCAP_TSO6 &&
			    ifp->if_hwassist & CSUM_TSO) {
				cap->ifcap |= CSUM_TSO;
				cap->tsomax = ifp->if_hw_tsomax;
				cap->tsomaxsegcount = ifp->if_hw_tsomaxsegcount;
				cap->tsomaxsegsize = ifp->if_hw_tsomaxsegsize;
			}
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
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (!m)
		return (0);

#ifdef INET6
	if ((inp->inp_vflag & INP_IPV6) != 0) {
		ip6 = mtod(m, struct ip6_hdr *);
		th = (struct tcphdr *)(ip6 + 1);
		m->m_pkthdr.len = m->m_len =
			sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
		tcpip_fillheaders(inp, ip6, th);
		hdrsiz = ipsec_hdrsiz(m, IPSEC_DIR_OUTBOUND, inp);
	} else
#endif /* INET6 */
	{
		ip = mtod(m, struct ip *);
		th = (struct tcphdr *)(ip + 1);
		m->m_pkthdr.len = m->m_len = sizeof(struct tcpiphdr);
		tcpip_fillheaders(inp, ip, th);
		hdrsiz = ipsec_hdrsiz(m, IPSEC_DIR_OUTBOUND, inp);
	}

	m_free(m);
	return (hdrsiz);
}
#endif /* IPSEC */

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
 * XXX The key is retrieved from the system's PF_KEY SADB, by keying a
 * search with the destination IP address, and a 'magic SPI' to be
 * determined by the application. This is hardcoded elsewhere to 1179
*/
struct secasvar *
tcp_get_sav(struct mbuf *m, u_int direction)
{
	union sockaddr_union dst;
	struct secasvar *sav;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
	char ip6buf[INET6_ADDRSTRLEN];
#endif

	/* Extract the destination from the IP header in the mbuf. */
	bzero(&dst, sizeof(union sockaddr_union));
	ip = mtod(m, struct ip *);
#ifdef INET6
	ip6 = NULL;	/* Make the compiler happy. */
#endif
	switch (ip->ip_v) {
#ifdef INET
	case IPVERSION:
		dst.sa.sa_len = sizeof(struct sockaddr_in);
		dst.sa.sa_family = AF_INET;
		dst.sin.sin_addr = (direction == IPSEC_DIR_INBOUND) ?
		    ip->ip_src : ip->ip_dst;
		break;
#endif
#ifdef INET6
	case (IPV6_VERSION >> 4):
		ip6 = mtod(m, struct ip6_hdr *);
		dst.sa.sa_len = sizeof(struct sockaddr_in6);
		dst.sa.sa_family = AF_INET6;
		dst.sin6.sin6_addr = (direction == IPSEC_DIR_INBOUND) ?
		    ip6->ip6_src : ip6->ip6_dst;
		break;
#endif
	default:
		return (NULL);
		/* NOTREACHED */
		break;
	}

	/* Look up an SADB entry which matches the address of the peer. */
	sav = KEY_ALLOCSA(&dst, IPPROTO_TCP, htonl(TCP_SIG_SPI));
	if (sav == NULL) {
		ipseclog((LOG_ERR, "%s: SADB lookup failed for %s\n", __func__,
		    (ip->ip_v == IPVERSION) ? inet_ntoa(dst.sin.sin_addr) :
#ifdef INET6
			(ip->ip_v == (IPV6_VERSION >> 4)) ?
			    ip6_sprintf(ip6buf, &dst.sin6.sin6_addr) :
#endif
			"(unsupported)"));
	}

	return (sav);
}

/*
 * Compute TCP-MD5 hash of a TCP segment. (RFC2385)
 *
 * Parameters:
 * m		pointer to head of mbuf chain
 * len		length of TCP segment data, excluding options
 * optlen	length of TCP segment options
 * buf		pointer to storage for computed MD5 digest
 * sav		pointer to security assosiation
 *
 * We do this over ip, tcphdr, segment data, and the key in the SADB.
 * When called from tcp_input(), we can be sure that th_sum has been
 * zeroed out and verified already.
 *
 * Releases reference to SADB key before return. 
 *
 * Return 0 if successful, otherwise return -1.
 *
 */
int
tcp_signature_do_compute(struct mbuf *m, int len, int optlen,
    u_char *buf, struct secasvar *sav)
{
#ifdef INET
	struct ippseudo ippseudo;
#endif
	MD5_CTX ctx;
	int doff;
	struct ip *ip;
#ifdef INET
	struct ipovly *ipovly;
#endif
	struct tcphdr *th;
#ifdef INET6
	struct ip6_hdr *ip6;
	struct in6_addr in6;
	uint32_t plen;
	uint16_t nhdr;
#endif
	u_short savecsum;

	KASSERT(m != NULL, ("NULL mbuf chain"));
	KASSERT(buf != NULL, ("NULL signature pointer"));

	/* Extract the destination from the IP header in the mbuf. */
	ip = mtod(m, struct ip *);
#ifdef INET6
	ip6 = NULL;	/* Make the compiler happy. */
#endif

	MD5Init(&ctx);
	/*
	 * Step 1: Update MD5 hash with IP(v6) pseudo-header.
	 *
	 * XXX The ippseudo header MUST be digested in network byte order,
	 * or else we'll fail the regression test. Assume all fields we've
	 * been doing arithmetic on have been in host byte order.
	 * XXX One cannot depend on ipovly->ih_len here. When called from
	 * tcp_output(), the underlying ip_len member has not yet been set.
	 */
	switch (ip->ip_v) {
#ifdef INET
	case IPVERSION:
		ipovly = (struct ipovly *)ip;
		ippseudo.ippseudo_src = ipovly->ih_src;
		ippseudo.ippseudo_dst = ipovly->ih_dst;
		ippseudo.ippseudo_pad = 0;
		ippseudo.ippseudo_p = IPPROTO_TCP;
		ippseudo.ippseudo_len = htons(len + sizeof(struct tcphdr) +
		    optlen);
		MD5Update(&ctx, (char *)&ippseudo, sizeof(struct ippseudo));

		th = (struct tcphdr *)((u_char *)ip + sizeof(struct ip));
		doff = sizeof(struct ip) + sizeof(struct tcphdr) + optlen;
		break;
#endif
#ifdef INET6
	/*
	 * RFC 2385, 2.0  Proposal
	 * For IPv6, the pseudo-header is as described in RFC 2460, namely the
	 * 128-bit source IPv6 address, 128-bit destination IPv6 address, zero-
	 * extended next header value (to form 32 bits), and 32-bit segment
	 * length.
	 * Note: Upper-Layer Packet Length comes before Next Header.
	 */
	case (IPV6_VERSION >> 4):
		in6 = ip6->ip6_src;
		in6_clearscope(&in6);
		MD5Update(&ctx, (char *)&in6, sizeof(struct in6_addr));
		in6 = ip6->ip6_dst;
		in6_clearscope(&in6);
		MD5Update(&ctx, (char *)&in6, sizeof(struct in6_addr));
		plen = htonl(len + sizeof(struct tcphdr) + optlen);
		MD5Update(&ctx, (char *)&plen, sizeof(uint32_t));
		nhdr = 0;
		MD5Update(&ctx, (char *)&nhdr, sizeof(uint8_t));
		MD5Update(&ctx, (char *)&nhdr, sizeof(uint8_t));
		MD5Update(&ctx, (char *)&nhdr, sizeof(uint8_t));
		nhdr = IPPROTO_TCP;
		MD5Update(&ctx, (char *)&nhdr, sizeof(uint8_t));

		th = (struct tcphdr *)((u_char *)ip6 + sizeof(struct ip6_hdr));
		doff = sizeof(struct ip6_hdr) + sizeof(struct tcphdr) + optlen;
		break;
#endif
	default:
		KEY_FREESAV(&sav);
		return (-1);
		/* NOTREACHED */
		break;
	}


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
	MD5Update(&ctx, sav->key_auth->key_data, _KEYLEN(sav->key_auth));
	MD5Final(buf, &ctx);

	key_sa_recordxfer(sav, m);
	KEY_FREESAV(&sav);
	return (0);
}

/*
 * Compute TCP-MD5 hash of a TCP segment. (RFC2385)
 *
 * Return 0 if successful, otherwise return -1.
 */
int
tcp_signature_compute(struct mbuf *m, int _unused, int len, int optlen,
    u_char *buf, u_int direction)
{
	struct secasvar *sav;

	if ((sav = tcp_get_sav(m, direction)) == NULL)
		return (-1);

	return (tcp_signature_do_compute(m, len, optlen, buf, sav));
}

/*
 * Verify the TCP-MD5 hash of a TCP segment. (RFC2385)
 *
 * Parameters:
 * m		pointer to head of mbuf chain
 * len		length of TCP segment data, excluding options
 * optlen	length of TCP segment options
 * buf		pointer to storage for computed MD5 digest
 * direction	direction of flow (IPSEC_DIR_INBOUND or OUTBOUND)
 *
 * Return 1 if successful, otherwise return 0.
 */
int
tcp_signature_verify(struct mbuf *m, int off0, int tlen, int optlen,
    struct tcpopt *to, struct tcphdr *th, u_int tcpbflag)
{
	char tmpdigest[TCP_SIGLEN];

	if (tcp_sig_checksigs == 0)
		return (1);
	if ((tcpbflag & TF_SIGNATURE) == 0) {
		if ((to->to_flags & TOF_SIGNATURE) != 0) {

			/*
			 * If this socket is not expecting signature but
			 * the segment contains signature just fail.
			 */
			TCPSTAT_INC(tcps_sig_err_sigopt);
			TCPSTAT_INC(tcps_sig_rcvbadsig);
			return (0);
		}

		/* Signature is not expected, and not present in segment. */
		return (1);
	}

	/*
	 * If this socket is expecting signature but the segment does not
	 * contain any just fail.
	 */
	if ((to->to_flags & TOF_SIGNATURE) == 0) {
		TCPSTAT_INC(tcps_sig_err_nosigopt);
		TCPSTAT_INC(tcps_sig_rcvbadsig);
		return (0);
	}
	if (tcp_signature_compute(m, off0, tlen, optlen, &tmpdigest[0],
	    IPSEC_DIR_INBOUND) == -1) {
		TCPSTAT_INC(tcps_sig_err_buildsig);
		TCPSTAT_INC(tcps_sig_rcvbadsig);
		return (0);
	}
	
	if (bcmp(to->to_signature, &tmpdigest[0], TCP_SIGLEN) != 0) {
		TCPSTAT_INC(tcps_sig_rcvbadsig);
		return (0);
	}
	TCPSTAT_INC(tcps_sig_rcvgoodsig);
	return (1);
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
		error = sa6_embedscope(fin6, V_ip6_use_defzone);
		if (error)
			return (error);
		error = sa6_embedscope(lin6, V_ip6_use_defzone);
		if (error)
			return (error);
		break;
#endif
#ifdef INET
	case AF_INET:
		fin = (struct sockaddr_in *)&addrs[0];
		lin = (struct sockaddr_in *)&addrs[1];
		if (fin->sin_len != sizeof(struct sockaddr_in) ||
		    lin->sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);
		break;
#endif
	default:
		return (EINVAL);
	}
	INP_INFO_RLOCK(&V_tcbinfo);
	switch (addrs[0].ss_family) {
#ifdef INET6
	case AF_INET6:
		inp = in6_pcblookup(&V_tcbinfo, &fin6->sin6_addr,
		    fin6->sin6_port, &lin6->sin6_addr, lin6->sin6_port,
		    INPLOOKUP_WLOCKPCB, NULL);
		break;
#endif
#ifdef INET
	case AF_INET:
		inp = in_pcblookup(&V_tcbinfo, fin->sin_addr, fin->sin_port,
		    lin->sin_addr, lin->sin_port, INPLOOKUP_WLOCKPCB, NULL);
		break;
#endif
	}
	if (inp != NULL) {
		if (inp->inp_flags & INP_TIMEWAIT) {
			/*
			 * XXXRW: There currently exists a state where an
			 * inpcb is present, but its timewait state has been
			 * discarded.  For now, don't allow dropping of this
			 * type of inpcb.
			 */
			tw = intotw(inp);
			if (tw != NULL)
				tcp_twclose(tw, 0);
			else
				INP_WUNLOCK(inp);
		} else if (!(inp->inp_flags & INP_DROPPED) &&
			   !(inp->inp_socket->so_options & SO_ACCEPTCONN)) {
			tp = intotcpcb(inp);
			tp = tcp_drop(tp, ECONNABORTED);
			if (tp != NULL)
				INP_WUNLOCK(inp);
		} else
			INP_WUNLOCK(inp);
	} else
		error = ESRCH;
	INP_INFO_RUNLOCK(&V_tcbinfo);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_DROP, drop,
    CTLFLAG_VNET | CTLTYPE_STRUCT | CTLFLAG_WR | CTLFLAG_SKIP, NULL,
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
tcp_log_vain(struct in_conninfo *inc, struct tcphdr *th, void *ip4hdr,
    const void *ip6hdr)
{

	/* Is logging enabled? */
	if (tcp_log_in_vain == 0)
		return (NULL);

	return (tcp_log_addr(inc, th, ip4hdr, ip6hdr));
}

char *
tcp_log_addrs(struct in_conninfo *inc, struct tcphdr *th, void *ip4hdr,
    const void *ip6hdr)
{

	/* Is logging enabled? */
	if (tcp_log_debug == 0)
		return (NULL);

	return (tcp_log_addr(inc, th, ip4hdr, ip6hdr));
}

static char *
tcp_log_addr(struct in_conninfo *inc, struct tcphdr *th, void *ip4hdr,
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

	s = malloc(size, M_TCPLOG, M_ZERO|M_NOWAIT);
	if (s == NULL)
		return (NULL);

	strcat(s, "TCP: [");
	sp = s + strlen(s);

	if (inc && ((inc->inc_flags & INC_ISIPV6) == 0)) {
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
#ifdef INET
	} else if (ip && th) {
		inet_ntoa_r(ip->ip_src, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(th->th_sport));
		sp = s + strlen(s);
		inet_ntoa_r(ip->ip_dst, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(th->th_dport));
#endif /* INET */
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

/*
 * A subroutine which makes it easy to track TCP state changes with DTrace.
 * This function shouldn't be called for t_state initializations that don't
 * correspond to actual TCP state transitions.
 */
void
tcp_state_change(struct tcpcb *tp, int newstate)
{
#if defined(KDTRACE_HOOKS)
	int pstate = tp->t_state;
#endif

	tp->t_state = newstate;
	TCP_PROBE6(state__change, NULL, tp, NULL, tp, NULL, pstate);
}
