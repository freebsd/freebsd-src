/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

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
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/random.h>

#include <vm/uma.h>

#include <net/route.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

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
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>

#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netipsec/ipsec_support.h>

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

VNET_DEFINE_STATIC(bool, nolocaltimewait) = true;
#define	V_nolocaltimewait	VNET(nolocaltimewait)
SYSCTL_BOOL(_net_inet_tcp, OID_AUTO, nolocaltimewait,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(nolocaltimewait), true,
    "Do not create TCP TIME_WAIT state for local connections");

/*
 * Move a TCP connection into TIME_WAIT state.
 *    inp is locked, and is unlocked before returning.
 *
 * This function used to free tcpcb and allocate a compressed TCP time-wait
 * structure tcptw.  This served well for 20 years but is no longer relevant
 * on modern machines in the modern internet.  However, the function remains
 * so that TCP stacks require less modification and we don't burn the bridge
 * to go back to using compressed time-wait.
 */
void
tcp_twstart(struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);
#ifdef INET6
	bool isipv6 = inp->inp_inc.inc_flags & INC_ISIPV6;
#endif

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);

	/* A dropped inp should never transition to TIME_WAIT state. */
	KASSERT((inp->inp_flags & INP_DROPPED) == 0, ("tcp_twstart: "
	    "(inp->inp_flags & INP_DROPPED) != 0"));

	tcp_state_change(tp, TCPS_TIME_WAIT);
	soisdisconnected(inp->inp_socket);

	if (tp->t_flags & TF_ACKNOW)
		tcp_output(tp);

	if (V_nolocaltimewait && (
#ifdef INET6
	    isipv6 ? in6_localaddr(&inp->in6p_faddr) :
#endif
#ifdef INET
	    in_localip(inp->inp_faddr)
#else
	    false
#endif
	    )) {
		if ((tp = tcp_close(tp)) != NULL)
			INP_WUNLOCK(inp);
		return;
	}

	tcp_timer_activate(tp, TT_2MSL, 2 * V_tcp_msl);
	INP_WUNLOCK(inp);
}

/*
 * Returns true if the TIME_WAIT state was killed and we should start over,
 * looking for a pcb in the listen state.  Otherwise returns false and frees
 * the mbuf.
 *
 * For pure SYN-segments the PCB shall be read-locked and the tcpopt pointer
 * may be NULL.  For the rest write-lock and valid tcpopt.
 */
bool
tcp_twcheck(struct inpcb *inp, struct tcpopt *to, struct tcphdr *th,
    struct mbuf *m, int tlen)
{
	struct tcpcb *tp = intotcpcb(inp);
	char *s;
	int thflags;
	tcp_seq seq;

	NET_EPOCH_ASSERT();
	INP_LOCK_ASSERT(inp);

	thflags = tcp_get_flags(th);
#ifdef INVARIANTS
	if ((thflags & (TH_SYN | TH_ACK)) == TH_SYN)
		INP_RLOCK_ASSERT(inp);
	else {
		INP_WLOCK_ASSERT(inp);
		KASSERT(to != NULL,
		    ("%s: called without options on a non-SYN segment",
		    __func__));
	}
#endif

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

	/* Honor the drop_synfin sysctl variable. */
	if ((thflags & TH_SYN) && (thflags & TH_FIN) && V_drop_synfin) {
		if ((s = tcp_log_addrs(&inp->inp_inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: "
			    "SYN|FIN segment ignored (based on "
			    "sysctl setting)\n", s, __func__);
			free(s, M_TCPLOG);
		}
		goto drop;
	}

	/*
	 * If a new connection request is received
	 * while in TIME_WAIT, drop the old connection
	 * and start over if the sequence numbers
	 * are above the previous ones.
	 * Allow UDP port number changes in this case.
	 */
	if (((thflags & (TH_SYN | TH_ACK)) == TH_SYN) &&
	    SEQ_GT(th->th_seq, tp->rcv_nxt)) {
		/*
		 * In case we can't upgrade our lock just pretend we have
		 * lost this packet.
		 */
		if (INP_TRY_UPGRADE(inp) == 0)
			goto drop;
		if ((tp = tcp_close(tp)) != NULL)
			INP_WUNLOCK(inp);
		TCPSTAT_INC(tcps_tw_recycles);
		return (true);
	}

	/*
	 * Send RST if UDP port numbers don't match
	 */
	if (tp->t_port != m->m_pkthdr.tcp_tun_port) {
		if (tcp_get_flags(th) & TH_ACK) {
			tcp_respond(tp, mtod(m, void *), th, m,
			    (tcp_seq)0, th->th_ack, TH_RST);
		} else {
			if (tcp_get_flags(th) & TH_SYN)
				tlen++;
			if (tcp_get_flags(th) & TH_FIN)
				tlen++;
			tcp_respond(tp, mtod(m, void *), th, m,
			    th->th_seq+tlen, (tcp_seq)0, TH_RST|TH_ACK);
		}
		INP_UNLOCK(inp);
		TCPSTAT_INC(tcps_tw_resets);
		return (false);
	}

	/*
	 * Drop the segment if it does not contain an ACK.
	 */
	if ((thflags & TH_ACK) == 0)
		goto drop;

	INP_WLOCK_ASSERT(inp);

	/*
	 * If timestamps were negotiated during SYN/ACK and a
	 * segment without a timestamp is received, silently drop
	 * the segment, unless the missing timestamps are tolerated.
	 * See section 3.2 of RFC 7323.
	 */
	if (((to->to_flags & TOF_TS) == 0) && (tp->ts_recent != 0) &&
	    (V_tcp_tolerate_missing_ts == 0)) {
		goto drop;
	}

	/*
	 * Reset the 2MSL timer if this is a duplicate FIN.
	 */
	if (thflags & TH_FIN) {
		seq = th->th_seq + tlen + (thflags & TH_SYN ? 1 : 0);
		if (seq + 1 == tp->rcv_nxt)
			tcp_timer_activate(tp, TT_2MSL, 2 * V_tcp_msl);
	}

	/*
	 * Acknowledge the segment if it has data or is not a duplicate ACK.
	 */
	if (thflags != TH_ACK || tlen != 0 ||
	    th->th_seq != tp->rcv_nxt || th->th_ack != tp->snd_nxt) {
		TCP_PROBE5(receive, NULL, NULL, m, NULL, th);
		tcp_respond(tp, mtod(m, void *), th, m, tp->rcv_nxt,
		    tp->snd_nxt, TH_ACK);
		INP_UNLOCK(inp);
		TCPSTAT_INC(tcps_tw_responds);
		return (false);
	}
drop:
	TCP_PROBE5(receive, NULL, NULL, m, NULL, th);
	INP_UNLOCK(inp);
	m_freem(m);
	return (false);
}
