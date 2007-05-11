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
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif
#include <netinet6/ip6protosw.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#include <netkey/key.h>
#endif /*IPSEC*/

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/key.h>
#define	IPSEC
#endif /*FAST_IPSEC*/

#include <machine/in_cksum.h>
#include <sys/md5.h>

#include <security/mac/mac_framework.h>

static uma_zone_t tcptw_zone;
static int	maxtcptw;

static int
tcptw_auto_size(void)
{
	int halfrange;

	/*
	 * Max out at half the ephemeral port range so that TIME_WAIT
	 * sockets don't tie up too many ephemeral ports.
	 */
	if (ipport_lastauto > ipport_firstauto)
		halfrange = (ipport_lastauto - ipport_firstauto) / 2;
	else
		halfrange = (ipport_firstauto - ipport_lastauto) / 2;
	/* Protect against goofy port ranges smaller than 32. */
	return (imin(imax(halfrange, 32), maxsockets / 5));
}

static int
sysctl_maxtcptw(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	if (maxtcptw == 0)
		new = tcptw_auto_size();
	else
		new = maxtcptw;
	error = sysctl_handle_int(oidp, &new, sizeof(int), req);
	if (error == 0 && req->newptr)
		if (new >= 32) {
			maxtcptw = new;
			uma_zone_set_max(tcptw_zone, maxtcptw);
		}
	return (error);
}
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, maxtcptw, CTLTYPE_INT|CTLFLAG_RW,
    &maxtcptw, 0, sysctl_maxtcptw, "IU",
    "Maximum number of compressed TCP TIME_WAIT entries");

static int	nolocaltimewait = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, nolocaltimewait, CTLFLAG_RW,
    &nolocaltimewait, 0,
    "Do not create compressed TCP TIME_WAIT entries for local connections");

/*
 * TCP initialization.
 */
static void
tcp_zone_change(void *tag)
{

	if (maxtcptw == 0)
		uma_zone_set_max(tcptw_zone, tcptw_auto_size());
}

void
tcp_init(void)
{

	tcptw_zone = uma_zcreate("tcptw", sizeof(struct tcptw),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	TUNABLE_INT_FETCH("net.inet.tcp.maxtcptw", &maxtcptw);
	if (maxtcptw == 0)
		uma_zone_set_max(tcptw_zone, tcptw_auto_size());
	else
		uma_zone_set_max(tcptw_zone, maxtcptw);
}

/*
 * Move a TCP connection into TIME_WAIT state.
 *    tcbinfo is locked.
 *    inp is locked, and is unlocked before returning.
 */
void
tcp_twstart(struct tcpcb *tp)
{
	struct tcptw *tw;
	struct inpcb *inp = tp->t_inpcb;
	int acknow;
	struct socket *so;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);	/* tcp_timer_2msl_reset(). */
	INP_LOCK_ASSERT(inp);

	if (nolocaltimewait && in_localip(inp->inp_faddr)) {
		tp = tcp_close(tp);
		if (tp != NULL)
			INP_UNLOCK(inp);
		return;
	}

	tw = uma_zalloc(tcptw_zone, M_NOWAIT);
	if (tw == NULL) {
		tw = tcp_timer_2msl_tw(1);
		if (tw == NULL) {
			tp = tcp_close(tp);
			if (tp != NULL)
				INP_UNLOCK(inp);
			return;
		}
	}
	tw->tw_inpcb = inp;

	/*
	 * Recover last window size sent.
	 */
	tw->last_win = (tp->rcv_adv - tp->rcv_nxt) >> tp->rcv_scale;

	/*
	 * Set t_recent if timestamps are used on the connection.
	 */
	if ((tp->t_flags & (TF_REQ_TSTMP|TF_RCVD_TSTMP|TF_NOOPT)) ==
	    (TF_REQ_TSTMP|TF_RCVD_TSTMP)) {
		tw->t_recent = tp->ts_recent;
		tw->ts_offset = tp->ts_offset;
	} else {
		tw->t_recent = 0;
		tw->ts_offset = 0;
	}

	tw->snd_nxt = tp->snd_nxt;
	tw->rcv_nxt = tp->rcv_nxt;
	tw->iss     = tp->iss;
	tw->irs     = tp->irs;
	tw->t_starttime = tp->t_starttime;
	tw->tw_time = 0;

/* XXX
 * If this code will
 * be used for fin-wait-2 state also, then we may need
 * a ts_recent from the last segment.
 */
	acknow = tp->t_flags & TF_ACKNOW;

	/*
	 * First, discard tcpcb state, which includes stopping its timers and
	 * freeing it.  tcp_discardcb() used to also release the inpcb, but
	 * that work is now done in the caller.
	 *
	 * Note: soisdisconnected() call used to be made in tcp_discardcb(),
	 * and might not be needed here any longer.
	 */
	tcp_discardcb(tp);
	so = inp->inp_socket;
	soisdisconnected(so);
	tw->tw_cred = crhold(so->so_cred);
	SOCK_LOCK(so);
	tw->tw_so_options = so->so_options;
	SOCK_UNLOCK(so);
	if (acknow)
		tcp_twrespond(tw, TH_ACK);
	inp->inp_ppcb = tw;
	inp->inp_vflag |= INP_TIMEWAIT;
	tcp_timer_2msl_reset(tw, 0);

	/*
	 * If the inpcb owns the sole reference to the socket, then we can
	 * detach and free the socket as it is not needed in time wait.
	 */
	if (inp->inp_vflag & INP_SOCKREF) {
		KASSERT(so->so_state & SS_PROTOREF,
		    ("tcp_twstart: !SS_PROTOREF"));
		inp->inp_vflag &= ~INP_SOCKREF;
		INP_UNLOCK(inp);
		ACCEPT_LOCK();
		SOCK_LOCK(so);
		so->so_state &= ~SS_PROTOREF;
		sofree(so);
	} else
		INP_UNLOCK(inp);
}

#if 0
/*
 * The appromixate rate of ISN increase of Microsoft TCP stacks;
 * the actual rate is slightly higher due to the addition of
 * random positive increments.
 *
 * Most other new OSes use semi-randomized ISN values, so we
 * do not need to worry about them.
 */
#define MS_ISN_BYTES_PER_SECOND		250000

/*
 * Determine if the ISN we will generate has advanced beyond the last
 * sequence number used by the previous connection.  If so, indicate
 * that it is safe to recycle this tw socket by returning 1.
 */
int
tcp_twrecycleable(struct tcptw *tw)
{
	tcp_seq new_iss = tw->iss;
	tcp_seq new_irs = tw->irs;

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	new_iss += (ticks - tw->t_starttime) * (ISN_BYTES_PER_SECOND / hz);
	new_irs += (ticks - tw->t_starttime) * (MS_ISN_BYTES_PER_SECOND / hz);

	if (SEQ_GT(new_iss, tw->snd_nxt) && SEQ_GT(new_irs, tw->rcv_nxt))
		return (1);
	else
		return (0);
}
#endif

void
tcp_twclose(struct tcptw *tw, int reuse)
{
	struct socket *so;
	struct inpcb *inp;

	/*
	 * At this point, we are in one of two situations:
	 *
	 * (1) We have no socket, just an inpcb<->twtcp pair.  We can free
	 *     all state.
	 *
	 * (2) We have a socket -- if we own a reference, release it and
	 *     notify the socket layer.
	 */
	inp = tw->tw_inpcb;
	KASSERT((inp->inp_vflag & INP_TIMEWAIT), ("tcp_twclose: !timewait"));
	KASSERT(intotw(inp) == tw, ("tcp_twclose: inp_ppcb != tw"));
	INP_INFO_WLOCK_ASSERT(&tcbinfo);	/* tcp_timer_2msl_stop(). */
	INP_LOCK_ASSERT(inp);

	tw->tw_inpcb = NULL;
	tcp_timer_2msl_stop(tw);
	inp->inp_ppcb = NULL;
	in_pcbdrop(inp);

	so = inp->inp_socket;
	if (so != NULL) {
		/*
		 * If there's a socket, handle two cases: first, we own a
		 * strong reference, which we will now release, or we don't
		 * in which case another reference exists (XXXRW: think
		 * about this more), and we don't need to take action.
		 */
		if (inp->inp_vflag & INP_SOCKREF) {
			inp->inp_vflag &= ~INP_SOCKREF;
			INP_UNLOCK(inp);
			ACCEPT_LOCK();
			SOCK_LOCK(so);
			KASSERT(so->so_state & SS_PROTOREF,
			    ("tcp_twclose: INP_SOCKREF && !SS_PROTOREF"));
			so->so_state &= ~SS_PROTOREF;
			sofree(so);
		} else {
			/*
			 * If we don't own the only reference, the socket and
			 * inpcb need to be left around to be handled by
			 * tcp_usr_detach() later.
			 */
			INP_UNLOCK(inp);
		}
	} else {
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6PROTO)
			in6_pcbfree(inp);
		else
#endif
			in_pcbfree(inp);
	}
	tcpstat.tcps_closed++;
	crfree(tw->tw_cred);
	tw->tw_cred = NULL;
	if (reuse)
		return;
	uma_zfree(tcptw_zone, tw);
}

int
tcp_twrespond(struct tcptw *tw, int flags)
{
	struct inpcb *inp = tw->tw_inpcb;
	struct tcphdr *th;
	struct mbuf *m;
	struct ip *ip = NULL;
	u_int hdrlen, optlen;
	int error;
	struct tcpopt to;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
	int isipv6 = inp->inp_inc.inc_isipv6;
#endif

	INP_LOCK_ASSERT(inp);

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	m->m_data += max_linkhdr;

#ifdef MAC
	mac_create_mbuf_from_inpcb(inp, m);
#endif

#ifdef INET6
	if (isipv6) {
		hdrlen = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
		ip6 = mtod(m, struct ip6_hdr *);
		th = (struct tcphdr *)(ip6 + 1);
		tcpip_fillheaders(inp, ip6, th);
	} else
#endif
	{
		hdrlen = sizeof(struct tcpiphdr);
		ip = mtod(m, struct ip *);
		th = (struct tcphdr *)(ip + 1);
		tcpip_fillheaders(inp, ip, th);
	}
	to.to_flags = 0;

	/*
	 * Send a timestamp and echo-reply if both our side and our peer
	 * have sent timestamps in our SYN's and this is not a RST.
	 */
	if (tw->t_recent && flags == TH_ACK) {
		to.to_flags |= TOF_TS;
		to.to_tsval = ticks + tw->ts_offset;
		to.to_tsecr = tw->t_recent;
	}
	optlen = tcp_addoptions(&to, (u_char *)(th + 1));

	m->m_len = hdrlen + optlen;
	m->m_pkthdr.len = m->m_len;

	KASSERT(max_linkhdr + m->m_len <= MHLEN, ("tcptw: mbuf too small"));

	th->th_seq = htonl(tw->snd_nxt);
	th->th_ack = htonl(tw->rcv_nxt);
	th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
	th->th_flags = flags;
	th->th_win = htons(tw->last_win);

#ifdef INET6
	if (isipv6) {
		th->th_sum = in6_cksum(m, IPPROTO_TCP, sizeof(struct ip6_hdr),
		    sizeof(struct tcphdr) + optlen);
		ip6->ip6_hlim = in6_selecthlim(inp, NULL);
		error = ip6_output(m, inp->in6p_outputopts, NULL,
		    (tw->tw_so_options & SO_DONTROUTE), NULL, NULL, inp);
	} else
#endif
	{
		th->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons(sizeof(struct tcphdr) + optlen + IPPROTO_TCP));
		m->m_pkthdr.csum_flags = CSUM_TCP;
		m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
		ip->ip_len = m->m_pkthdr.len;
		if (path_mtu_discovery)
			ip->ip_off |= IP_DF;
		error = ip_output(m, inp->inp_options, NULL,
		    ((tw->tw_so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0),
		    NULL, inp);
	}
	if (flags & TH_ACK)
		tcpstat.tcps_sndacks++;
	else
		tcpstat.tcps_sndctrl++;
	tcpstat.tcps_sndtotal++;
	return (error);
}
