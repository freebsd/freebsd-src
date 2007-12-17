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
#include <netinet/tcp_offload.h>
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

#include <dev/cxgb/ulp/tom/cxgb_tcp.h>


SYSCTL_NODE(_net_inet_tcp, 0,	cxgb,	CTLFLAG_RW, 0,	"chelsio TOE");

static int	tcp_log_debug = 0;
SYSCTL_INT(_net_inet_tcp_cxgb, OID_AUTO, log_debug, CTLFLAG_RW,
    &tcp_log_debug, 0, "Log errors caused by incoming TCP segments");

static int	tcp_tcbhashsize = 0;
SYSCTL_INT(_net_inet_tcp_cxgb, OID_AUTO, tcbhashsize, CTLFLAG_RDTUN,
    &tcp_tcbhashsize, 0, "Size of TCP control-block hashtable");

static int	do_tcpdrain = 1;
SYSCTL_INT(_net_inet_tcp_cxgb, OID_AUTO, do_tcpdrain, CTLFLAG_RW,
    &do_tcpdrain, 0,
    "Enable tcp_drain routine for extra help when low on mbufs");

SYSCTL_INT(_net_inet_tcp_cxgb, OID_AUTO, pcbcount, CTLFLAG_RD,
    &tcbinfo.ipi_count, 0, "Number of active PCBs");

static int	icmp_may_rst = 1;
SYSCTL_INT(_net_inet_tcp_cxgb, OID_AUTO, icmp_may_rst, CTLFLAG_RW,
    &icmp_may_rst, 0,
    "Certain ICMP unreachable messages may abort connections in SYN_SENT");

static int	tcp_isn_reseed_interval = 0;
SYSCTL_INT(_net_inet_tcp_cxgb, OID_AUTO, isn_reseed_interval, CTLFLAG_RW,
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
static struct inpcb *cxgb_tcp_drop_syn_sent(struct inpcb *inp, int errno);

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

MALLOC_DEFINE(M_TCPLOG, "tcplog", "TCP address and flags print buffers");

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
cxgb_tcp_drop(struct tcpcb *tp, int errno)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(tp->t_inpcb);

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		(void) tcp_gen_reset(tp);
		tcpstat.tcps_drops++;
	} else
		tcpstat.tcps_conndrops++;
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (cxgb_tcp_close(tp));
}

/*
 * Attempt to close a TCP control block, marking it as dropped, and freeing
 * the socket if we hold the only reference.
 */
struct tcpcb *
cxgb_tcp_close(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(inp);

	if (tp->t_state == TCPS_LISTEN)
		tcp_gen_listen_close(tp);
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
		tp = cxgb_tcp_drop(tp, error);
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

void
cxgb_tcp_ctlinput(int cmd, struct sockaddr *sa, void *vip)
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
		notify = cxgb_tcp_drop_syn_sent;
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


/*
 * When a specific ICMP unreachable message is received and the
 * connection state is SYN-SENT, drop the connection.  This behavior
 * is controlled by the icmp_may_rst sysctl.
 */
static struct inpcb *
cxgb_tcp_drop_syn_sent(struct inpcb *inp, int errno)
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

	tp = cxgb_tcp_drop(tp, errno);
	if (tp != NULL)
		return (inp);
	else
		return (NULL);
}

static int
cxgb_sysctl_drop(SYSCTL_HANDLER_ARGS)
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
			else
				INP_UNLOCK(inp);
		} else if (!(inp->inp_vflag & INP_DROPPED) &&
			   !(inp->inp_socket->so_options & SO_ACCEPTCONN)) {
			tp = intotcpcb(inp);
			tp = cxgb_tcp_drop(tp, ECONNABORTED);
			if (tp != NULL)
				INP_UNLOCK(inp);
		} else
			INP_UNLOCK(inp);
	} else
		error = ESRCH;
	INP_INFO_WUNLOCK(&tcbinfo);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp_cxgb, TCPCTL_DROP, drop,
    CTLTYPE_STRUCT|CTLFLAG_WR|CTLFLAG_SKIP, NULL,
    0, cxgb_sysctl_drop, "", "Drop TCP connection");

