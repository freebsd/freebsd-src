/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>

void
tcp_reass_flush(struct tcpcb *tp)
{
	struct mbuf *m;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	while ((m = tp->t_segq) != NULL) {
		tp->t_segq = m->m_nextpkt;
		tp->t_segqlen -= m->m_pkthdr.len;
		m_freem(m);
	}

	KASSERT((tp->t_segqlen == 0),
	    ("TCP reass queue %p length is %d instead of 0 after flush.",
	    tp, tp->t_segqlen));
}

#define	M_TCPHDR(m)	((struct tcphdr *)((m)->m_pkthdr.pkt_tcphdr))

int
tcp_reass(struct tcpcb *tp, struct tcphdr *th, int *tlenp, struct mbuf *m)
{
	struct socket *so = tp->t_inpcb->inp_socket;
	struct mbuf *mq, *mp;
	int flags, wakeup;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * XXX: tcp_reass() is rather inefficient with its data structures
	 * and should be rewritten (see NetBSD for optimizations).
	 */

	/*
	 * Call with th==NULL after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (th == NULL)
		goto present;

	M_ASSERTPKTHDR(m);
	KASSERT(*tlenp == m->m_pkthdr.len, ("%s: tlenp %u len %u", __func__,
	    *tlenp, m->m_pkthdr.len));

	/*
	 * Limit the number of segments that can be queued to reduce the
	 * potential for mbuf exhaustion. For best performance, we want to be
	 * able to queue a full window's worth of segments. The size of the
	 * socket receive buffer determines our advertised window and grows
	 * automatically when socket buffer autotuning is enabled. Use it as the
	 * basis for our queue limit.
	 * Always let the missing segment through which caused this queue.
	 * NB: Access to the socket buffer is left intentionally unlocked as we
	 * can tolerate stale information here.
	 */
	if ((th->th_seq != tp->rcv_nxt || !TCPS_HAVEESTABLISHED(tp->t_state)) &&
	    tp->t_segqlen + m->m_pkthdr.len >= sbspace(&so->so_rcv)) {
		char *s;

		TCPSTAT_INC(tcps_rcvreassfull);
		*tlenp = 0;
		if ((s = tcp_log_addrs(&tp->t_inpcb->inp_inc, th, NULL,
		    NULL))) {
			log(LOG_DEBUG, "%s; %s: queue limit reached, "
			    "segment dropped\n", s, __func__);
			free(s, M_TCPLOG);
		}
		m_freem(m);
		return (0);
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	mp = NULL;
	for (mq = tp->t_segq; mq != NULL; mq = mq->m_nextpkt) {
		if (SEQ_GT(M_TCPHDR(mq)->th_seq, th->th_seq))
			break;
		mp = mq;
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (mp != NULL) {
		int i;

		/* conversion to int (in i) handles seq wraparound */
		i = M_TCPHDR(mp)->th_seq + mp->m_pkthdr.len - th->th_seq;
		if (i > 0) {
			if (i >= *tlenp) {
				TCPSTAT_INC(tcps_rcvduppack);
				TCPSTAT_ADD(tcps_rcvdupbyte, *tlenp);
				m_freem(m);
				/*
				 * Try to present any queued data
				 * at the left window edge to the user.
				 * This is needed after the 3-WHS
				 * completes.
				 */
				goto present;	/* ??? */
			}
			m_adj(m, i);
			*tlenp -= i;
			th->th_seq += i;
		}
	}
	tp->t_rcvoopack++;
	TCPSTAT_INC(tcps_rcvoopack);
	TCPSTAT_ADD(tcps_rcvoobyte, *tlenp);

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (mq) {
		struct mbuf *nq;
		int i;

		i = (th->th_seq + *tlenp) - M_TCPHDR(mq)->th_seq;
		if (i <= 0)
			break;
		if (i < mq->m_pkthdr.len) {
			M_TCPHDR(mq)->th_seq += i;
			m_adj(mq, i);
			tp->t_segqlen -= i;
			break;
		}

		nq = mq->m_nextpkt;
		tp->t_segqlen -= mq->m_pkthdr.len;
		m_freem(mq);
		if (mp)
			mp->m_nextpkt = nq;
		else
			tp->t_segq = nq;
		mq = nq;
	}

	/*
	 * Insert the new segment queue entry into place.  Try to collapse
	 * mbuf chains if segments are adjacent.
	 */
	if (mp) {
		if (M_TCPHDR(mp)->th_seq + mp->m_pkthdr.len == th->th_seq)
			m_catpkt(mp, m);
		else {
			m->m_nextpkt = mp->m_nextpkt;
			mp->m_nextpkt = m;
			m->m_pkthdr.pkt_tcphdr = th;
		}
	} else {
		mq = tp->t_segq;
		tp->t_segq = m;
		if (mq && th->th_seq + *tlenp == M_TCPHDR(mq)->th_seq) {
			m->m_nextpkt = mq->m_nextpkt;
			mq->m_nextpkt = NULL;
			m_catpkt(m, mq);
		} else
			m->m_nextpkt = mq;
		m->m_pkthdr.pkt_tcphdr = th;
	}
	tp->t_segqlen += *tlenp;

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (!TCPS_HAVEESTABLISHED(tp->t_state))
		return (0);

	flags = 0;
	wakeup = 0;
	SOCKBUF_LOCK(&so->so_rcv);
	while ((mq = tp->t_segq) != NULL &&
	    M_TCPHDR(mq)->th_seq == tp->rcv_nxt) {
		tp->t_segq = mq->m_nextpkt;

		tp->rcv_nxt += mq->m_pkthdr.len;
		tp->t_segqlen -= mq->m_pkthdr.len;
		flags = M_TCPHDR(mq)->th_flags & TH_FIN;

		if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
			m_freem(mq);
		else {
			mq->m_nextpkt = NULL;
			sbappendstream_locked(&so->so_rcv, mq);
			wakeup = 1;
		}
	}
	ND6_HINT(tp);
	if (wakeup)
		sorwakeup_locked(so);
	else
		SOCKBUF_UNLOCK(&so->so_rcv);
	return (flags);
}
