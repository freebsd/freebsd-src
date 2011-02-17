/*-
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2004-2009 Robert N. M. Watson
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
 * Copyright (c) 1995, Mike Mitchell
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)spx_usrreq.h
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/systm.h>

#include <net/route.h>
#include <netinet/tcp_fsm.h>

#include <netipx/ipx.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>
#include <netipx/spx.h>
#include <netipx/spx_debug.h>
#include <netipx/spx_timer.h>
#include <netipx/spx_var.h>

static int	spx_use_delack = 0;
static int	spxrexmtthresh = 3;

MALLOC_DEFINE(M_SPXREASSQ, "spxreassq", "SPX reassembly queue entry");

/*
 * Flesh pending queued segments on SPX close.
 */
void
spx_reass_flush(struct spxpcb *cb)
{
	struct spx_q *q;

	while ((q = LIST_FIRST(&cb->s_q)) != NULL) {
		LIST_REMOVE(q, sq_entry);
		m_freem(q->sq_msi);
		free(q, M_SPXREASSQ);
	}
}

/*
 * Initialize SPX segment reassembly queue on SPX socket open.
 */
void
spx_reass_init(struct spxpcb *cb)
{

	LIST_INIT(&cb->s_q);
}

/*
 * This is structurally similar to the tcp reassembly routine but its
 * function is somewhat different: it merely queues packets up, and
 * suppresses duplicates.
 */
int
spx_reass(struct spxpcb *cb, struct mbuf *msi, struct spx *si)
{
	struct spx_q *q, *q_new, *q_temp;
	struct mbuf *m;
	struct socket *so = cb->s_ipxpcb->ipxp_socket;
	char packetp = cb->s_flags & SF_HI;
	int incr;
	char wakeup = 0;

	IPX_LOCK_ASSERT(cb->s_ipxpcb);

	if (si == SI(0))
		goto present;

	/*
	 * Update our news from them.
	 */
	if (si->si_cc & SPX_SA)
		cb->s_flags |= (spx_use_delack ? SF_DELACK : SF_ACKNOW);
	if (SSEQ_GT(si->si_alo, cb->s_ralo))
		cb->s_flags |= SF_WIN;
	if (SSEQ_LEQ(si->si_ack, cb->s_rack)) {
		if ((si->si_cc & SPX_SP) && cb->s_rack != (cb->s_smax + 1)) {
			spxstat.spxs_rcvdupack++;

			/*
			 * If this is a completely duplicate ack and other
			 * conditions hold, we assume a packet has been
			 * dropped and retransmit it exactly as in
			 * tcp_input().
			 */
			if (si->si_ack != cb->s_rack ||
			    si->si_alo != cb->s_ralo)
				cb->s_dupacks = 0;
			else if (++cb->s_dupacks == spxrexmtthresh) {
				u_short onxt = cb->s_snxt;
				int cwnd = cb->s_cwnd;

				cb->s_snxt = si->si_ack;
				cb->s_cwnd = CUNIT;
				cb->s_force = 1 + SPXT_REXMT;
				spx_output(cb, NULL);
				cb->s_timer[SPXT_REXMT] = cb->s_rxtcur;
				cb->s_rtt = 0;
				if (cwnd >= 4 * CUNIT)
					cb->s_cwnd = cwnd / 2;
				if (SSEQ_GT(onxt, cb->s_snxt))
					cb->s_snxt = onxt;
				return (1);
			}
		} else
			cb->s_dupacks = 0;
		goto update_window;
	}
	cb->s_dupacks = 0;

	/*
	 * If our correspondent acknowledges data we haven't sent TCP would
	 * drop the packet after acking.  We'll be a little more permissive.
	 */
	if (SSEQ_GT(si->si_ack, (cb->s_smax + 1))) {
		spxstat.spxs_rcvacktoomuch++;
		si->si_ack = cb->s_smax + 1;
	}
	spxstat.spxs_rcvackpack++;

	/*
	 * If transmit timer is running and timed sequence number was acked,
	 * update smoothed round trip time.  See discussion of algorithm in
	 * tcp_input.c
	 */
	if (cb->s_rtt && SSEQ_GT(si->si_ack, cb->s_rtseq)) {
		spxstat.spxs_rttupdated++;
		if (cb->s_srtt != 0) {
			short delta;
			delta = cb->s_rtt - (cb->s_srtt >> 3);
			if ((cb->s_srtt += delta) <= 0)
				cb->s_srtt = 1;
			if (delta < 0)
				delta = -delta;
			delta -= (cb->s_rttvar >> 2);
			if ((cb->s_rttvar += delta) <= 0)
				cb->s_rttvar = 1;
		} else {
			/*
			 * No rtt measurement yet.
			 */
			cb->s_srtt = cb->s_rtt << 3;
			cb->s_rttvar = cb->s_rtt << 1;
		}
		cb->s_rtt = 0;
		cb->s_rxtshift = 0;
		SPXT_RANGESET(cb->s_rxtcur,
			((cb->s_srtt >> 2) + cb->s_rttvar) >> 1,
			SPXTV_MIN, SPXTV_REXMTMAX);
	}

	/*
	 * If all outstanding data is acked, stop retransmit timer and
	 * remember to restart (more output or persist).  If there is more
	 * data to be acked, restart retransmit timer, using current
	 * (possibly backed-off) value;
	 */
	if (si->si_ack == cb->s_smax + 1) {
		cb->s_timer[SPXT_REXMT] = 0;
		cb->s_flags |= SF_RXT;
	} else if (cb->s_timer[SPXT_PERSIST] == 0)
		cb->s_timer[SPXT_REXMT] = cb->s_rxtcur;

	/*
	 * When new data is acked, open the congestion window.  If the window
	 * gives us less than ssthresh packets in flight, open exponentially
	 * (maxseg at a time).  Otherwise open linearly (maxseg^2 / cwnd at a
	 * time).
	 */
	incr = CUNIT;
	if (cb->s_cwnd > cb->s_ssthresh)
		incr = max(incr * incr / cb->s_cwnd, 1);
	cb->s_cwnd = min(cb->s_cwnd + incr, cb->s_cwmx);

	/*
	 * Trim Acked data from output queue.
	 */
	SOCKBUF_LOCK(&so->so_snd);
	while ((m = so->so_snd.sb_mb) != NULL) {
		if (SSEQ_LT((mtod(m, struct spx *))->si_seq, si->si_ack))
			sbdroprecord_locked(&so->so_snd);
		else
			break;
	}
	sowwakeup_locked(so);
	cb->s_rack = si->si_ack;
update_window:
	if (SSEQ_LT(cb->s_snxt, cb->s_rack))
		cb->s_snxt = cb->s_rack;
	if (SSEQ_LT(cb->s_swl1, si->si_seq) || ((cb->s_swl1 == si->si_seq &&
	    (SSEQ_LT(cb->s_swl2, si->si_ack))) ||
	     (cb->s_swl2 == si->si_ack && SSEQ_LT(cb->s_ralo, si->si_alo)))) {
		/* keep track of pure window updates */
		if ((si->si_cc & SPX_SP) && cb->s_swl2 == si->si_ack
		    && SSEQ_LT(cb->s_ralo, si->si_alo)) {
			spxstat.spxs_rcvwinupd++;
			spxstat.spxs_rcvdupack--;
		}
		cb->s_ralo = si->si_alo;
		cb->s_swl1 = si->si_seq;
		cb->s_swl2 = si->si_ack;
		cb->s_swnd = (1 + si->si_alo - si->si_ack);
		if (cb->s_swnd > cb->s_smxw)
			cb->s_smxw = cb->s_swnd;
		cb->s_flags |= SF_WIN;
	}

	/*
	 * If this packet number is higher than that which we have allocated
	 * refuse it, unless urgent.
	 */
	if (SSEQ_GT(si->si_seq, cb->s_alo)) {
		if (si->si_cc & SPX_SP) {
			spxstat.spxs_rcvwinprobe++;
			return (1);
		} else
			spxstat.spxs_rcvpackafterwin++;
		if (si->si_cc & SPX_OB) {
			if (SSEQ_GT(si->si_seq, cb->s_alo + 60))
				return (1); /* else queue this packet; */
		} else {
#ifdef BROKEN
			/*
			 * XXXRW: This is broken on at least one count:
			 * spx_close() will free the ipxp and related parts,
			 * which are then touched by spx_input() after the
			 * return from spx_reass().
			 */
			/*struct socket *so = cb->s_ipxpcb->ipxp_socket;
			if (so->so_state && SS_NOFDREF) {
				spx_close(cb);
			} else
				       would crash system*/
#endif
			spx_istat.notyet++;
			return (1);
		}
	}

	/*
	 * If this is a system packet, we don't need to queue it up, and
	 * won't update acknowledge #.
	 */
	if (si->si_cc & SPX_SP)
		return (1);

	/*
	 * We have already seen this packet, so drop.
	 */
	if (SSEQ_LT(si->si_seq, cb->s_ack)) {
		spx_istat.bdreas++;
		spxstat.spxs_rcvduppack++;
		if (si->si_seq == cb->s_ack - 1)
			spx_istat.lstdup++;
		return (1);
	}

	/*
	 * Loop through all packets queued up to insert in appropriate
	 * sequence.
	 */
	q_new = malloc(sizeof(*q_new), M_SPXREASSQ, M_NOWAIT | M_ZERO);
	if (q_new == NULL)
		return (1);
	q_new->sq_si = si;
	q_new->sq_msi = msi;
	LIST_FOREACH(q, &cb->s_q, sq_entry) {
		if (si->si_seq == q->sq_si->si_seq) {
			free(q_new, M_SPXREASSQ);
			spxstat.spxs_rcvduppack++;
			return (1);
		}
		if (SSEQ_LT(si->si_seq, q->sq_si->si_seq)) {
			spxstat.spxs_rcvoopack++;
			break;
		}
	}
	if (q != NULL)
		LIST_INSERT_BEFORE(q, q_new, sq_entry);
	else
		LIST_INSERT_HEAD(&cb->s_q, q_new, sq_entry);

	/*
	 * If this packet is urgent, inform process
	 */
	if (si->si_cc & SPX_OB) {
		cb->s_iobc = ((char *)si)[1 + sizeof(*si)];
		sohasoutofband(so);
		cb->s_oobflags |= SF_IOOB;
	}
present:
#define SPINC sizeof(struct spxhdr)
	SOCKBUF_LOCK(&so->so_rcv);

	/*
	 * Loop through all packets queued up to update acknowledge number,
	 * and present all acknowledged data to user; if in packet interface
	 * mode, show packet headers.
	 */
	LIST_FOREACH_SAFE(q, &cb->s_q, sq_entry, q_temp) {
		struct spx *qsi;
		struct mbuf *mqsi;

		qsi = q->sq_si;
		mqsi = q->sq_msi;
		if (qsi->si_seq == cb->s_ack) {
			cb->s_ack++;
			if (qsi->si_cc & SPX_OB) {
				cb->s_oobflags &= ~SF_IOOB;
				if (so->so_rcv.sb_cc)
					so->so_oobmark = so->so_rcv.sb_cc;
				else
					so->so_rcv.sb_state |= SBS_RCVATMARK;
			}
			LIST_REMOVE(q, sq_entry);
			free(q, M_SPXREASSQ);
			wakeup = 1;
			spxstat.spxs_rcvpack++;
#ifdef SF_NEWCALL
			if (cb->s_flags2 & SF_NEWCALL) {
				struct spxhdr *sp =
				    mtod(mqsi, struct spxhdr *);
				u_char dt = sp->spx_dt;

				spx_newchecks[4]++;
				if (dt != cb->s_rhdr.spx_dt) {
					struct mbuf *mm =
					   m_getclr(M_DONTWAIT, MT_CONTROL);
					spx_newchecks[0]++;
					if (mm != NULL) {
						u_short *s =
							mtod(mm, u_short *);
						cb->s_rhdr.spx_dt = dt;
						mm->m_len = 5; /*XXX*/
						s[0] = 5;
						s[1] = 1;
						*(u_char *)(&s[2]) = dt;
						sbappend_locked(&so->so_rcv, mm);
					}
				}
				if (sp->spx_cc & SPX_OB) {
					MCHTYPE(mqsi, MT_OOBDATA);
					spx_newchecks[1]++;
					so->so_oobmark = 0;
					so->so_rcv.sb_state &= ~SBS_RCVATMARK;
				}
				if (packetp == 0) {
					mqsi->m_data += SPINC;
					mqsi->m_len -= SPINC;
					mqsi->m_pkthdr.len -= SPINC;
				}
				if ((sp->spx_cc & SPX_EM) || packetp) {
					sbappendrecord_locked(&so->so_rcv,
					    mqsi);
					spx_newchecks[9]++;
				} else
					sbappend_locked(&so->so_rcv, mqsi);
			} else
#endif
			if (packetp)
				sbappendrecord_locked(&so->so_rcv, mqsi);
			else {
				cb->s_rhdr = *mtod(mqsi, struct spxhdr *);
				mqsi->m_data += SPINC;
				mqsi->m_len -= SPINC;
				mqsi->m_pkthdr.len -= SPINC;
				sbappend_locked(&so->so_rcv, mqsi);
			}
		  } else
			break;
	}
	if (wakeup)
		sorwakeup_locked(so);
	else
		SOCKBUF_UNLOCK(&so->so_rcv);
	return (0);
}
