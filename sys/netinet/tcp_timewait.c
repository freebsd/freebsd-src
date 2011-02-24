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

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/random.h>

#include <vm/uma.h>

#include <net/route.h>
#include <net/if.h>
#include <net/vnet.h>

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

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

static VNET_DEFINE(uma_zone_t, tcptw_zone);
#define	V_tcptw_zone			VNET(tcptw_zone)
static int	maxtcptw;

/*
 * The timed wait queue contains references to each of the TCP sessions
 * currently in the TIME_WAIT state.  The queue pointers, including the
 * queue pointers in each tcptw structure, are protected using the global
 * tcbinfo lock, which must be held over queue iteration and modification.
 */
static VNET_DEFINE(TAILQ_HEAD(, tcptw), twq_2msl);
#define	V_twq_2msl			VNET(twq_2msl)

static void	tcp_tw_2msl_reset(struct tcptw *, int);
static void	tcp_tw_2msl_stop(struct tcptw *);

static int
tcptw_auto_size(void)
{
	int halfrange;

	/*
	 * Max out at half the ephemeral port range so that TIME_WAIT
	 * sockets don't tie up too many ephemeral ports.
	 */
	if (V_ipport_lastauto > V_ipport_firstauto)
		halfrange = (V_ipport_lastauto - V_ipport_firstauto) / 2;
	else
		halfrange = (V_ipport_firstauto - V_ipport_lastauto) / 2;
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
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr)
		if (new >= 32) {
			maxtcptw = new;
			uma_zone_set_max(V_tcptw_zone, maxtcptw);
		}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, maxtcptw, CTLTYPE_INT|CTLFLAG_RW,
    &maxtcptw, 0, sysctl_maxtcptw, "IU",
    "Maximum number of compressed TCP TIME_WAIT entries");

VNET_DEFINE(int, nolocaltimewait) = 0;
#define	V_nolocaltimewait	VNET(nolocaltimewait)
SYSCTL_VNET_INT(_net_inet_tcp, OID_AUTO, nolocaltimewait, CTLFLAG_RW,
    &VNET_NAME(nolocaltimewait), 0,
    "Do not create compressed TCP TIME_WAIT entries for local connections");

void
tcp_tw_zone_change(void)
{

	if (maxtcptw == 0)
		uma_zone_set_max(V_tcptw_zone, tcptw_auto_size());
}

void
tcp_tw_init(void)
{

	V_tcptw_zone = uma_zcreate("tcptw", sizeof(struct tcptw),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	TUNABLE_INT_FETCH("net.inet.tcp.maxtcptw", &maxtcptw);
	if (maxtcptw == 0)
		uma_zone_set_max(V_tcptw_zone, tcptw_auto_size());
	else
		uma_zone_set_max(V_tcptw_zone, maxtcptw);
	TAILQ_INIT(&V_twq_2msl);
}

#ifdef VIMAGE
void
tcp_tw_destroy(void)
{
	struct tcptw *tw;

	INP_INFO_WLOCK(&V_tcbinfo);
	while((tw = TAILQ_FIRST(&V_twq_2msl)) != NULL)
		tcp_twclose(tw, 0);
	INP_INFO_WUNLOCK(&V_tcbinfo);

	uma_zdestroy(V_tcptw_zone);
}
#endif

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

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);	/* tcp_tw_2msl_reset(). */
	INP_WLOCK_ASSERT(inp);

	if (V_nolocaltimewait && in_localip(inp->inp_faddr)) {
		tp = tcp_close(tp);
		if (tp != NULL)
			INP_WUNLOCK(inp);
		return;
	}

	tw = uma_zalloc(V_tcptw_zone, M_NOWAIT);
	if (tw == NULL) {
		tw = tcp_tw_2msl_scan(1);
		if (tw == NULL) {
			tp = tcp_close(tp);
			if (tp != NULL)
				INP_WUNLOCK(inp);
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
	inp->inp_flags |= INP_TIMEWAIT;
	tcp_tw_2msl_reset(tw, 0);

	/*
	 * If the inpcb owns the sole reference to the socket, then we can
	 * detach and free the socket as it is not needed in time wait.
	 */
	if (inp->inp_flags & INP_SOCKREF) {
		KASSERT(so->so_state & SS_PROTOREF,
		    ("tcp_twstart: !SS_PROTOREF"));
		inp->inp_flags &= ~INP_SOCKREF;
		INP_WUNLOCK(inp);
		ACCEPT_LOCK();
		SOCK_LOCK(so);
		so->so_state &= ~SS_PROTOREF;
		sofree(so);
	} else
		INP_WUNLOCK(inp);
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

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	new_iss += (ticks - tw->t_starttime) * (ISN_BYTES_PER_SECOND / hz);
	new_irs += (ticks - tw->t_starttime) * (MS_ISN_BYTES_PER_SECOND / hz);

	if (SEQ_GT(new_iss, tw->snd_nxt) && SEQ_GT(new_irs, tw->rcv_nxt))
		return (1);
	else
		return (0);
}
#endif

/*
 * Returns 1 if the TIME_WAIT state was killed and we should start over,
 * looking for a pcb in the listen state.  Returns 0 otherwise.
 */
int
tcp_twcheck(struct inpcb *inp, struct tcpopt *to, struct tcphdr *th,
    struct mbuf *m, int tlen)
{
	struct tcptw *tw;
	int thflags;
	tcp_seq seq;

	/* tcbinfo lock required for tcp_twclose(), tcp_tw_2msl_reset(). */
	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(inp);

	/*
	 * XXXRW: Time wait state for inpcb has been recycled, but inpcb is
	 * still present.  This is undesirable, but temporarily necessary
	 * until we work out how to handle inpcb's who's timewait state has
	 * been removed.
	 */
	tw = intotw(inp);
	if (tw == NULL)
		goto drop;

	thflags = th->th_flags;

	/*
	 * NOTE: for FIN_WAIT_2 (to be added later),
	 * must validate sequence number before accepting RST
	 */

	/*
	 * If the segment contains RST:
	 *	Drop the segment - see Stevens, vol. 2, p. 964 and
	 *      RFC 1337.
	 */
	if (thflags & TH_RST)
		goto drop;

#if 0
/* PAWS not needed at the moment */
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if ((to.to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to.to_tsval, tp->ts_recent)) {
		if ((thflags & TH_ACK) == 0)
			goto drop;
		goto ack;
	}
	/*
	 * ts_recent is never updated because we never accept new segments.
	 */
#endif

	/*
	 * If a new connection request is received
	 * while in TIME_WAIT, drop the old connection
	 * and start over if the sequence numbers
	 * are above the previous ones.
	 */
	if ((thflags & TH_SYN) && SEQ_GT(th->th_seq, tw->rcv_nxt)) {
		tcp_twclose(tw, 0);
		return (1);
	}

	/*
	 * Drop the segment if it does not contain an ACK.
	 */
	if ((thflags & TH_ACK) == 0)
		goto drop;

	/*
	 * Reset the 2MSL timer if this is a duplicate FIN.
	 */
	if (thflags & TH_FIN) {
		seq = th->th_seq + tlen + (thflags & TH_SYN ? 1 : 0);
		if (seq + 1 == tw->rcv_nxt)
			tcp_tw_2msl_reset(tw, 1);
	}

	/*
	 * Acknowledge the segment if it has data or is not a duplicate ACK.
	 */
	if (thflags != TH_ACK || tlen != 0 ||
	    th->th_seq != tw->rcv_nxt || th->th_ack != tw->snd_nxt)
		tcp_twrespond(tw, TH_ACK);
drop:
	INP_WUNLOCK(inp);
	m_freem(m);
	return (0);
}

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
	KASSERT((inp->inp_flags & INP_TIMEWAIT), ("tcp_twclose: !timewait"));
	KASSERT(intotw(inp) == tw, ("tcp_twclose: inp_ppcb != tw"));
	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);	/* tcp_tw_2msl_stop(). */
	INP_WLOCK_ASSERT(inp);

	tw->tw_inpcb = NULL;
	tcp_tw_2msl_stop(tw);
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
		if (inp->inp_flags & INP_SOCKREF) {
			inp->inp_flags &= ~INP_SOCKREF;
			INP_WUNLOCK(inp);
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
			INP_WUNLOCK(inp);
		}
	} else
		in_pcbfree(inp);
	TCPSTAT_INC(tcps_closed);
	crfree(tw->tw_cred);
	tw->tw_cred = NULL;
	if (reuse)
		return;
	uma_zfree(V_tcptw_zone, tw);
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
	int isipv6 = inp->inp_inc.inc_flags & INC_ISIPV6;
#endif

	INP_WLOCK_ASSERT(inp);

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	m->m_data += max_linkhdr;

#ifdef MAC
	mac_inpcb_create_mbuf(inp, m);
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
		if (V_path_mtu_discovery)
			ip->ip_off |= IP_DF;
		error = ip_output(m, inp->inp_options, NULL,
		    ((tw->tw_so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0),
		    NULL, inp);
	}
	if (flags & TH_ACK)
		TCPSTAT_INC(tcps_sndacks);
	else
		TCPSTAT_INC(tcps_sndctrl);
	TCPSTAT_INC(tcps_sndtotal);
	return (error);
}

static void
tcp_tw_2msl_reset(struct tcptw *tw, int rearm)
{

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tw->tw_inpcb);
	if (rearm)
		TAILQ_REMOVE(&V_twq_2msl, tw, tw_2msl);
	tw->tw_time = ticks + 2 * tcp_msl;
	TAILQ_INSERT_TAIL(&V_twq_2msl, tw, tw_2msl);
}

static void
tcp_tw_2msl_stop(struct tcptw *tw)
{

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	TAILQ_REMOVE(&V_twq_2msl, tw, tw_2msl);
}

struct tcptw *
tcp_tw_2msl_scan(int reuse)
{
	struct tcptw *tw;

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	for (;;) {
		tw = TAILQ_FIRST(&V_twq_2msl);
		if (tw == NULL || (!reuse && (tw->tw_time - ticks) > 0))
			break;
		INP_WLOCK(tw->tw_inpcb);
		tcp_twclose(tw, reuse);
		if (reuse)
			return (tw);
	}
	return (NULL);
}
