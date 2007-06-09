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
 *	@(#)tcp_timer.c	8.2 (Berkeley) 5/24/95
 * $FreeBSD$
 */

#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/limits.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif

int	tcp_keepinit;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINIT, keepinit, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepinit, 0, sysctl_msec_to_ticks, "I", "");

int	tcp_keepidle;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPIDLE, keepidle, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepidle, 0, sysctl_msec_to_ticks, "I", "");

int	tcp_keepintvl;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINTVL, keepintvl, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepintvl, 0, sysctl_msec_to_ticks, "I", "");

int	tcp_delacktime;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_DELACKTIME, delacktime, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_delacktime, 0, sysctl_msec_to_ticks, "I",
    "Time before a delayed ACK is sent");

int	tcp_msl;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, msl, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_msl, 0, sysctl_msec_to_ticks, "I", "Maximum segment lifetime");

int	tcp_rexmit_min;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_min, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_rexmit_min, 0, sysctl_msec_to_ticks, "I",
    "Minimum Retransmission Timeout");

int	tcp_rexmit_slop;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_slop, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_rexmit_slop, 0, sysctl_msec_to_ticks, "I",
    "Retransmission Timer Slop");

static int	always_keepalive = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, always_keepalive, CTLFLAG_RW,
    &always_keepalive , 0, "Assume SO_KEEPALIVE on all TCP connections");

int    tcp_fast_finwait2_recycle = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, fast_finwait2_recycle, CTLFLAG_RW, 
    &tcp_fast_finwait2_recycle, 0,
    "Recycle closed FIN_WAIT_2 connections faster");

int    tcp_finwait2_timeout;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, finwait2_timeout, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_finwait2_timeout, 0, sysctl_msec_to_ticks, "I", "FIN-WAIT2 timeout");


static int	tcp_keepcnt = TCPTV_KEEPCNT;
	/* max idle probes */
int	tcp_maxpersistidle;
	/* max idle time in persist */
int	tcp_maxidle;

static void	tcp_timer(void *);
static int	tcp_timer_delack(struct tcpcb *, struct inpcb *);
static int	tcp_timer_2msl(struct tcpcb *, struct inpcb *);
static int	tcp_timer_keep(struct tcpcb *, struct inpcb *);
static int	tcp_timer_persist(struct tcpcb *, struct inpcb *);
static int	tcp_timer_rexmt(struct tcpcb *, struct inpcb *);

/*
 * Tcp protocol timeout routine called every 500 ms.
 * Updates timestamps used for TCP
 * causes finite state machine actions if timers expire.
 */
void
tcp_slowtimo(void)
{

	tcp_maxidle = tcp_keepcnt * tcp_keepintvl;
	INP_INFO_WLOCK(&tcbinfo);
	(void) tcp_tw_2msl_scan(0);
	INP_INFO_WUNLOCK(&tcbinfo);
}

int	tcp_syn_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 1, 1, 1, 1, 2, 4, 8, 16, 32, 64, 64, 64 };

int	tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 512, 512, 512 };

static int tcp_totbackoff = 2559;	/* sum of tcp_backoff[] */

static int tcp_timer_race;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, timer_race, CTLFLAG_RD, &tcp_timer_race,
    0, "Count of t_inpcb races on tcp_discardcb");

void
tcp_timer_activate(struct tcpcb *tp, int timer_type, u_int delta)
{
	struct inpcb *inp = tp->t_inpcb;
	struct tcp_timer *tt = tp->t_timers;
	int tick = ticks;			/* Stable time base. */
	int next = delta ? tick + delta : 0;

	INP_LOCK_ASSERT(inp);

	CTR6(KTR_NET, "%p %s inp %p active %x delta %i nextc %i",
	    tp, __func__, inp, tt->tt_active, delta, tt->tt_nextc);

	/* Set new value for timer. */
	switch(timer_type) {
	case TT_DELACK:
		CTR4(KTR_NET, "%p %s TT_DELACK old %i new %i",
		    tp, __func__, tt->tt_delack, next);
		tt->tt_delack = next;
		break;
	case TT_REXMT:
		CTR4(KTR_NET, "%p %s TT_REXMT old %i new %i",
		    tp, __func__, tt->tt_rexmt, next);
		tt->tt_rexmt = next;
		break;
	case TT_PERSIST:
		CTR4(KTR_NET, "%p %s TT_PERSIST old %i new %i",
		    tp, __func__, tt->tt_persist, next);
		tt->tt_persist = next;
		break;
	case TT_KEEP:
		CTR4(KTR_NET, "%p %s TT_KEEP old %i new %i",
		    tp, __func__, tt->tt_keep, next);
		tt->tt_keep = next;
		break;
	case TT_2MSL:
		CTR4(KTR_NET, "%p %s TT_2MSL old %i new %i",
		    tp, __func__, tt->tt_2msl, next);
		tt->tt_2msl = next;
		break;
	case 0:					/* Dummy for timer rescan. */
		CTR3(KTR_NET, "%p %s timer rescan new %i", tp, __func__, next);
		break;
	}

	/* If some other timer is active and is schedules sooner just return. */
	if (tt->tt_active != timer_type && tt->tt_nextc < next &&
	    callout_active(&tt->tt_timer))
		return;

	/* Select next timer to schedule. */
	tt->tt_nextc = INT_MAX;
	tt->tt_active = 0;
	if (tt->tt_delack && tt->tt_delack < tt->tt_nextc) {
		tt->tt_nextc = tt->tt_delack;
		tt->tt_active = TT_DELACK;
	}
	if (tt->tt_rexmt && tt->tt_rexmt < tt->tt_nextc) {
		tt->tt_nextc = tt->tt_rexmt;
		tt->tt_active = TT_REXMT;
	}
	if (tt->tt_persist && tt->tt_persist < tt->tt_nextc) {
		tt->tt_nextc = tt->tt_persist;
		tt->tt_active = TT_PERSIST;
	}
	if (tt->tt_keep && tt->tt_keep < tt->tt_nextc) {
		tt->tt_nextc = tt->tt_keep;
		tt->tt_active = TT_KEEP;
	}
	if (tt->tt_2msl && tt->tt_2msl < tt->tt_nextc) {
		tt->tt_nextc = tt->tt_2msl;
		tt->tt_active = TT_2MSL;
	}

	/* Rearm callout with new timer if we found one. */
	if (tt->tt_active) {
		CTR4(KTR_NET, "%p %s callout_reset active %x nextc in %i",
		    tp, __func__, tt->tt_active, tt->tt_nextc - tick);
		callout_reset(&tt->tt_timer,
		    tt->tt_nextc - tick, tcp_timer, (void *)inp);
	} else {
		CTR2(KTR_NET, "%p %s callout_stop", tp, __func__);
		callout_stop(&tt->tt_timer);
		tt->tt_nextc = 0;
	}

	return;
}

int
tcp_timer_active(struct tcpcb *tp, int timer_type)
{

	switch (timer_type) {
	case TT_DELACK:
		CTR3(KTR_NET, "%p %s TT_DELACK %i",
		    tp, __func__, tp->t_timers->tt_delack);
		return (tp->t_timers->tt_delack ? 1 : 0);
		break;
	case TT_REXMT:
		CTR3(KTR_NET, "%p %s TT_REXMT %i",
		    tp, __func__, tp->t_timers->tt_rexmt);
		return (tp->t_timers->tt_rexmt ? 1 : 0);
		break;
	case TT_PERSIST:
		CTR3(KTR_NET, "%p %s TT_PERSIST %i",
		    tp, __func__, tp->t_timers->tt_persist);
		return (tp->t_timers->tt_persist ? 1 : 0);
		break;
	case TT_KEEP:
		CTR3(KTR_NET, "%p %s TT_KEEP %i",
		    tp, __func__, tp->t_timers->tt_keep);
		return (tp->t_timers->tt_keep ? 1 : 0);
		break;
	case TT_2MSL:
		CTR3(KTR_NET, "%p %s TT_2MSL %i",
		    tp, __func__, tp->t_timers->tt_2msl);
		return (tp->t_timers->tt_2msl ? 1 : 0);
		break;
	}
	return (0);
}

static void
tcp_timer(void *xinp)
{
	struct inpcb *inp = (struct inpcb *)xinp;
	struct tcpcb *tp = intotcpcb(inp);
	struct tcp_timer *tt;
	int tick = ticks;
	int down, timer;

	/* INP lock was obtained by callout. */
	INP_LOCK_ASSERT(inp);

	/*
	 * We've got a couple of race conditions here:
	 * - The tcpcb was converted into a compressed TW pcb.  All our
	 *   timers have been stopped while this callout already tried
	 *   to obtain the inpcb lock.  TW pcbs have their own timers
	 *   and we just return.
	 */
	if (inp->inp_vflag & INP_TIMEWAIT)
		return;
	/*
	 * - The tcpcb was discarded.  All our timers have been stopped
	 *   while this callout already tried to obtain the inpcb lock
	 *   and we just return.
	 */
	if (tp == NULL)
		return;

	tt = tp->t_timers;	/* Initialize. */
	CTR6(KTR_NET, "%p %s inp %p active %x tick %i nextc %i",
	    tp, __func__, inp, tt->tt_active, tick, tt->tt_nextc);

	/*
	 * - We may have been waiting on the lock while the tcpcb has
	 *   been scheduled for destruction.  In this case no active
	 *   timers remain and we just return.
	 */
	if (tt->tt_active == 0)
		goto done;

	/*
	 * - The timer was rescheduled while this callout was already
	 *   waiting on the lock.  This may happen when a packet just
	 *   came in.  Rescan and reschedule the the timer in case we
	 *   just turned it off.
	 */
	if (tick < tt->tt_nextc)
		goto rescan;

	/*
	 * Mark as done.  The active bit in struct callout is not
	 * automatically cleared.  See callout(9) for more info.
	 * In tcp_discardcb() we depend on the correctly cleared
	 * active bit for faster processing.
	 */
	callout_deactivate(&tt->tt_timer);

	/* Check which timer has fired and remove this timer activation. */
	timer = tt->tt_active;
	tt->tt_active = 0;
	tt->tt_nextc = 0;

	switch (timer) {
	case TT_DELACK:
		CTR2(KTR_NET, "%p %s running TT_DELACK", tp, __func__);
		tt->tt_delack = 0;
		down = tcp_timer_delack(tp, inp);	/* down == 0 */
		break;
	case TT_REXMT:
		CTR2(KTR_NET, "%p %s running TT_REXMT", tp, __func__);
		tt->tt_rexmt = 0;
		down = tcp_timer_rexmt(tp, inp);
		break;
	case TT_PERSIST:
		CTR2(KTR_NET, "%p %s running TT_PERSIST", tp, __func__);
		tt->tt_persist = 0;
		down = tcp_timer_persist(tp, inp);
		break;
	case TT_KEEP:
		CTR2(KTR_NET, "%p %s running TT_KEEP", tp, __func__);
		tt->tt_keep = 0;
		down = tcp_timer_keep(tp, inp);
		break;
	case TT_2MSL:
		CTR2(KTR_NET, "%p %s running TT_2MSL", tp, __func__);
		tt->tt_2msl = 0;
		down = tcp_timer_2msl(tp, inp);
		break;
	default:
		CTR2(KTR_NET, "%p %s running nothing", tp, __func__);
		down = 0;
	}

	CTR4(KTR_NET, "%p %s down %i active %x",
	    tp, __func__, down, tt->tt_active);
	/* Do we still exist? */
	if (down)
		goto shutdown;

rescan:
	/* Rescan if no timer was reactivated above. */
	if (tt->tt_active == 0)
		tcp_timer_activate(tp, 0, 0);

done:
	INP_UNLOCK(inp);		/* CALLOUT_RETURNUNLOCKED */
	return;

shutdown:
	INP_UNLOCK(inp);		/* Prevent LOR at expense of race. */
	INP_INFO_WLOCK(&tcbinfo);
	INP_LOCK(inp);

	/*
	 * XXX: When our tcpcb went into TIMEWAIT, is gone or no
	 * longer the one we used to work with we've lost the race.
	 * This race is inherent in the current socket/inpcb life
	 * cycle system.
	 */
	if ((inp->inp_vflag & INP_TIMEWAIT) || inp->inp_ppcb == NULL ||
	    inp->inp_ppcb != tp) {
		CTR3(KTR_NET, "%p %s inp %p lost shutdown race",
		    tp, __func__, inp);
		tcp_timer_race++;
		INP_UNLOCK(inp);	/* CALLOUT_RETURNUNLOCKED */
		INP_INFO_WUNLOCK(&tcbinfo);
		return;
	}
	KASSERT(tp == inp->inp_ppcb, ("%s: tp changed", __func__));

	/* Shutdown the connection. */
	switch (down) {
	case 1:
		tp = tcp_close(tp);
		break;
	case 2:
		tp = tcp_drop(tp,
			tp->t_softerror ? tp->t_softerror : ETIMEDOUT);
		break;
	}
	CTR3(KTR_NET, "%p %s inp %p after shutdown", tp, __func__, inp);

	if (tp)
		INP_UNLOCK(inp);	/* CALLOUT_RETURNUNLOCKED */

	INP_INFO_WUNLOCK(&tcbinfo);
	return;
}

/*
 * TCP timer processing.
 */
static int
tcp_timer_delack(struct tcpcb *tp, struct inpcb *inp)
{

	tp->t_flags |= TF_ACKNOW;
	tcpstat.tcps_delack++;
	(void) tcp_output(tp);
	return (0);
}

static int
tcp_timer_2msl(struct tcpcb *tp, struct inpcb *inp)
{
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	/*
	 * 2 MSL timeout in shutdown went off.  If we're closed but
	 * still waiting for peer to close and connection has been idle
	 * too long, or if 2MSL time is up from TIME_WAIT, delete connection
	 * control block.  Otherwise, check again in a bit.
	 *
	 * If fastrecycle of FIN_WAIT_2, in FIN_WAIT_2 and receiver has closed, 
	 * there's no point in hanging onto FIN_WAIT_2 socket. Just close it. 
	 * Ignore fact that there were recent incoming segments.
	 */
	if (tcp_fast_finwait2_recycle && tp->t_state == TCPS_FIN_WAIT_2 &&
	    tp->t_inpcb->inp_socket && 
	    (tp->t_inpcb->inp_socket->so_rcv.sb_state & SBS_CANTRCVMORE)) {
		tcpstat.tcps_finwait2_drops++;
		return (1);		/* tcp_close */
	} else {
		if (tp->t_state != TCPS_TIME_WAIT &&
		   (ticks - tp->t_rcvtime) <= tcp_maxidle)
			tcp_timer_activate(tp, TT_2MSL, tcp_keepintvl);
		else
			return (1);	/* tcp_close */
	}

#ifdef TCPDEBUG
	if (tp->t_inpcb->inp_socket->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0,
			  PRU_SLOWTIMO);
#endif
	return (0);
}

static int
tcp_timer_keep(struct tcpcb *tp, struct inpcb *inp)
{
	struct tcptemp *t_template;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 */
	tcpstat.tcps_keeptimeo++;
	if (tp->t_state < TCPS_ESTABLISHED)
		goto dropit;
	if ((always_keepalive || inp->inp_socket->so_options & SO_KEEPALIVE) &&
	    tp->t_state <= TCPS_CLOSING) {
		if ((ticks - tp->t_rcvtime) >= tcp_keepidle + tcp_maxidle)
			goto dropit;
		/*
		 * Send a packet designed to force a response
		 * if the peer is up and reachable:
		 * either an ACK if the connection is still alive,
		 * or an RST if the peer has closed the connection
		 * due to timeout or reboot.
		 * Using sequence number tp->snd_una-1
		 * causes the transmitted zero-length segment
		 * to lie outside the receive window;
		 * by the protocol spec, this requires the
		 * correspondent TCP to respond.
		 */
		tcpstat.tcps_keepprobe++;
		t_template = tcpip_maketemplate(inp);
		if (t_template) {
			tcp_respond(tp, t_template->tt_ipgen,
				    &t_template->tt_t, (struct mbuf *)NULL,
				    tp->rcv_nxt, tp->snd_una - 1, 0);
			(void) m_free(dtom(t_template));
		}
		tcp_timer_activate(tp, TT_KEEP, tcp_keepintvl);
	} else
		tcp_timer_activate(tp, TT_KEEP, tcp_keepidle);

#ifdef TCPDEBUG
	if (inp->inp_socket->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0,
			  PRU_SLOWTIMO);
#endif
	return (0);

dropit:
	tcpstat.tcps_keepdrops++;
	return (2);			/* tcp_drop() */
}

static int
tcp_timer_persist(struct tcpcb *tp, struct inpcb *inp)
{
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	/*
	 * Persistance timer into zero window.
	 * Force a byte to be output, if possible.
	 */
	tcpstat.tcps_persisttimeo++;
	/*
	 * Hack: if the peer is dead/unreachable, we do not
	 * time out if the window is closed.  After a full
	 * backoff, drop the connection if the idle time
	 * (no responses to probes) reaches the maximum
	 * backoff that we would use if retransmitting.
	 */
	if (tp->t_rxtshift == TCP_MAXRXTSHIFT &&
	    ((ticks - tp->t_rcvtime) >= tcp_maxpersistidle ||
	     (ticks - tp->t_rcvtime) >= TCP_REXMTVAL(tp) * tcp_totbackoff)) {
		tcpstat.tcps_persistdrop++;
		return (2);		/* tcp_drop() */
	}
	tcp_setpersist(tp);
	tp->t_flags |= TF_FORCEDATA;
	(void) tcp_output(tp);
	tp->t_flags &= ~TF_FORCEDATA;

#ifdef TCPDEBUG
	if (tp != NULL && tp->t_inpcb->inp_socket->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO);
#endif
	return (0);
}

static int
tcp_timer_rexmt(struct tcpcb *tp, struct inpcb *inp)
{
	int rexmt;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	tcp_free_sackholes(tp);
	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one segment.
	 */
	if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
		tp->t_rxtshift = TCP_MAXRXTSHIFT;
		tcpstat.tcps_timeoutdrop++;
		return (2);		/* tcp_drop() */
	}
	if (tp->t_rxtshift == 1) {
		/*
		 * first retransmit; record ssthresh and cwnd so they can
		 * be recovered if this turns out to be a "bad" retransmit.
		 * A retransmit is considered "bad" if an ACK for this
		 * segment is received within RTT/2 interval; the assumption
		 * here is that the ACK was already in flight.  See
		 * "On Estimating End-to-End Network Path Properties" by
		 * Allman and Paxson for more details.
		 */
		tp->snd_cwnd_prev = tp->snd_cwnd;
		tp->snd_ssthresh_prev = tp->snd_ssthresh;
		tp->snd_recover_prev = tp->snd_recover;
		if (IN_FASTRECOVERY(tp))
			tp->t_flags |= TF_WASFRECOVERY;
		else
			tp->t_flags &= ~TF_WASFRECOVERY;
		tp->t_badrxtwin = ticks + (tp->t_srtt >> (TCP_RTT_SHIFT + 1));
	}
	tcpstat.tcps_rexmttimeo++;
	if (tp->t_state == TCPS_SYN_SENT)
		rexmt = TCP_REXMTVAL(tp) * tcp_syn_backoff[tp->t_rxtshift];
	else
		rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
	TCPT_RANGESET(tp->t_rxtcur, rexmt,
		      tp->t_rttmin, TCPTV_REXMTMAX);
	/*
	 * Disable rfc1323 if we havn't got any response to
	 * our third SYN to work-around some broken terminal servers
	 * (most of which have hopefully been retired) that have bad VJ
	 * header compression code which trashes TCP segments containing
	 * unknown-to-them TCP options.
	 */
	if ((tp->t_state == TCPS_SYN_SENT) && (tp->t_rxtshift == 3))
		tp->t_flags &= ~(TF_REQ_SCALE|TF_REQ_TSTMP);
	/*
	 * If we backed off this far, our srtt estimate is probably bogus.
	 * Clobber it so we'll take the next rtt measurement as our srtt;
	 * move the current srtt into rttvar to keep the current
	 * retransmit times until then.
	 */
	if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
#ifdef INET6
		if ((tp->t_inpcb->inp_vflag & INP_IPV6) != 0)
			in6_losing(tp->t_inpcb);
		else
#endif
		tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
		tp->t_srtt = 0;
	}
	tp->snd_nxt = tp->snd_una;
	tp->snd_recover = tp->snd_max;
	/*
	 * Force a segment to be sent.
	 */
	tp->t_flags |= TF_ACKNOW;
	/*
	 * If timing a segment in this window, stop the timer.
	 */
	tp->t_rtttime = 0;
	/*
	 * Close the congestion window down to one segment
	 * (we'll open it by one segment for each ack we get).
	 * Since we probably have a window's worth of unacked
	 * data accumulated, this "slow start" keeps us from
	 * dumping all that data as back-to-back packets (which
	 * might overwhelm an intermediate gateway).
	 *
	 * There are two phases to the opening: Initially we
	 * open by one mss on each ack.  This makes the window
	 * size increase exponentially with time.  If the
	 * window is larger than the path can handle, this
	 * exponential growth results in dropped packet(s)
	 * almost immediately.  To get more time between
	 * drops but still "push" the network to take advantage
	 * of improving conditions, we switch from exponential
	 * to linear window opening at some threshhold size.
	 * For a threshhold, we use half the current window
	 * size, truncated to a multiple of the mss.
	 *
	 * (the minimum cwnd that will give us exponential
	 * growth is 2 mss.  We don't allow the threshhold
	 * to go below this.)
	 */
	{
		u_int win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
		if (win < 2)
			win = 2;
		tp->snd_cwnd = tp->t_maxseg;
		tp->snd_ssthresh = win * tp->t_maxseg;
		tp->t_dupacks = 0;
	}
	EXIT_FASTRECOVERY(tp);
	(void) tcp_output(tp);

#ifdef TCPDEBUG
	if (tp != NULL && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0,
			  PRU_SLOWTIMO);
#endif
	return (0);
}
