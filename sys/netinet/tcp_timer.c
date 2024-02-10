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
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>
#include <net/rss_config.h>
#include <net/vnet.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/in_rss.h>
#include <netinet/in_systm.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_seq.h>
#include <netinet/cc/cc.h>
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcpip.h>

int    tcp_persmin;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, persmin,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_persmin, 0, sysctl_msec_to_ticks, "I",
    "minimum persistence interval");

int    tcp_persmax;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, persmax,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_persmax, 0, sysctl_msec_to_ticks, "I",
    "maximum persistence interval");

int	tcp_keepinit;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINIT, keepinit,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_keepinit, 0, sysctl_msec_to_ticks, "I",
    "time to establish connection");

int	tcp_keepidle;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPIDLE, keepidle,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_keepidle, 0, sysctl_msec_to_ticks, "I",
    "time before keepalive probes begin");

int	tcp_keepintvl;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINTVL, keepintvl,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_keepintvl, 0, sysctl_msec_to_ticks, "I",
    "time between keepalive probes");

int	tcp_delacktime;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_DELACKTIME, delacktime,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_delacktime, 0, sysctl_msec_to_ticks, "I",
    "Time before a delayed ACK is sent");

VNET_DEFINE(int, tcp_msl);
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, msl,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(tcp_msl), 0, sysctl_msec_to_ticks, "I",
    "Maximum segment lifetime");

int	tcp_rexmit_initial;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_initial,
   CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_rexmit_initial, 0, sysctl_msec_to_ticks, "I",
    "Initial Retransmission Timeout");

int	tcp_rexmit_min;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_min,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_rexmit_min, 0, sysctl_msec_to_ticks, "I",
    "Minimum Retransmission Timeout");

int	tcp_rexmit_slop;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_slop,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_rexmit_slop, 0, sysctl_msec_to_ticks, "I",
    "Retransmission Timer Slop");

VNET_DEFINE(int, tcp_always_keepalive) = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, always_keepalive, CTLFLAG_VNET|CTLFLAG_RW,
    &VNET_NAME(tcp_always_keepalive) , 0,
    "Assume SO_KEEPALIVE on all TCP connections");

int    tcp_fast_finwait2_recycle = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, fast_finwait2_recycle, CTLFLAG_RW,
    &tcp_fast_finwait2_recycle, 0,
    "Recycle closed FIN_WAIT_2 connections faster");

int    tcp_finwait2_timeout;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, finwait2_timeout,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_finwait2_timeout, 0, sysctl_msec_to_ticks, "I",
    "FIN-WAIT2 timeout");

int	tcp_keepcnt = TCPTV_KEEPCNT;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, keepcnt, CTLFLAG_RW, &tcp_keepcnt, 0,
    "Number of keepalive probes to send");

	/* max idle probes */
int	tcp_maxpersistidle;

int	tcp_rexmit_drop_options = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, rexmit_drop_options, CTLFLAG_RW,
    &tcp_rexmit_drop_options, 0,
    "Drop TCP options from 3rd and later retransmitted SYN");

int	tcp_maxunacktime = TCPTV_MAXUNACKTIME;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, maxunacktime,
    CTLTYPE_INT|CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &tcp_maxunacktime, 0, sysctl_msec_to_ticks, "I",
    "Maximum time (in ms) that a session can linger without making progress");

VNET_DEFINE(int, tcp_pmtud_blackhole_detect);
SYSCTL_INT(_net_inet_tcp, OID_AUTO, pmtud_blackhole_detection,
    CTLFLAG_RW|CTLFLAG_VNET,
    &VNET_NAME(tcp_pmtud_blackhole_detect), 0,
    "Path MTU Discovery Black Hole Detection Enabled");

#ifdef INET
VNET_DEFINE(int, tcp_pmtud_blackhole_mss) = 1200;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, pmtud_blackhole_mss,
    CTLFLAG_RW|CTLFLAG_VNET,
    &VNET_NAME(tcp_pmtud_blackhole_mss), 0,
    "Path MTU Discovery Black Hole Detection lowered MSS");
#endif

#ifdef INET6
VNET_DEFINE(int, tcp_v6pmtud_blackhole_mss) = 1220;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, v6pmtud_blackhole_mss,
    CTLFLAG_RW|CTLFLAG_VNET,
    &VNET_NAME(tcp_v6pmtud_blackhole_mss), 0,
    "Path MTU Discovery IPv6 Black Hole Detection lowered MSS");
#endif

#ifdef	RSS
static int	per_cpu_timers = 1;
#else
static int	per_cpu_timers = 0;
#endif
SYSCTL_INT(_net_inet_tcp, OID_AUTO, per_cpu_timers, CTLFLAG_RW,
    &per_cpu_timers , 0, "run tcp timers on all cpus");

static int
sysctl_net_inet_tcp_retries(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	new = V_tcp_retries;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if ((new < 1) || (new > TCP_MAXRXTSHIFT))
			error = EINVAL;
		else
			V_tcp_retries = new;
	}
	return (error);
}

VNET_DEFINE(int, tcp_retries) = TCP_MAXRXTSHIFT;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, retries,
    CTLTYPE_INT | CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_retries), 0, sysctl_net_inet_tcp_retries, "I",
    "maximum number of consecutive timer based retransmissions");

/*
 * Map the given inp to a CPU id.
 *
 * This queries RSS if it's compiled in, else it defaults to the current
 * CPU ID.
 */
inline int
inp_to_cpuid(struct inpcb *inp)
{
	u_int cpuid;

	if (per_cpu_timers) {
#ifdef	RSS
		cpuid = rss_hash2cpuid(inp->inp_flowid, inp->inp_flowtype);
		if (cpuid == NETISR_CPUID_NONE)
			return (curcpu);	/* XXX */
		else
			return (cpuid);
#endif
		/*
		 * We don't have a flowid -> cpuid mapping, so cheat and
		 * just map unknown cpuids to curcpu.  Not the best, but
		 * apparently better than defaulting to swi 0.
		 */
		cpuid = inp->inp_flowid % (mp_maxid + 1);
		if (! CPU_ABSENT(cpuid))
			return (cpuid);
		return (curcpu);
	} else {
		return (0);
	}
}

int	tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 512, 512, 512 };

int tcp_totbackoff = 2559;	/* sum of tcp_backoff[] */

/*
 * TCP timer processing.
 *
 * Each connection has 5 timers associated with it, which can be scheduled
 * simultaneously.  They all are serviced by one callout tcp_timer_enter().
 * This function executes the next timer via tcp_timersw[] vector.  Each
 * timer is supposed to return 'true' unless the connection was destroyed.
 * In the former case tcp_timer_enter() will schedule callout for next timer.
 */

typedef bool tcp_timer_t(struct tcpcb *);
static tcp_timer_t tcp_timer_delack;
static tcp_timer_t tcp_timer_2msl;
static tcp_timer_t tcp_timer_keep;
static tcp_timer_t tcp_timer_persist;
static tcp_timer_t tcp_timer_rexmt;

static tcp_timer_t * const tcp_timersw[TT_N] = {
	[TT_DELACK] = tcp_timer_delack,
	[TT_REXMT] = tcp_timer_rexmt,
	[TT_PERSIST] = tcp_timer_persist,
	[TT_KEEP] = tcp_timer_keep,
	[TT_2MSL] = tcp_timer_2msl,
};

/*
 * tcp_output_locked() s a timer specific variation of call to tcp_output(),
 * see tcp_var.h for the rest.  It handles drop request from advanced stacks,
 * but keeps tcpcb locked unless tcp_drop() destroyed it.
 * Returns true if tcpcb is valid and locked.
 */
static inline bool
tcp_output_locked(struct tcpcb *tp)
{
	int rv;

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	if ((rv = tp->t_fb->tfb_tcp_output(tp)) < 0) {
		KASSERT(tp->t_fb->tfb_flags & TCP_FUNC_OUTPUT_CANDROP,
		    ("TCP stack %s requested tcp_drop(%p)",
		    tp->t_fb->tfb_tcp_block_name, tp));
		tp = tcp_drop(tp, -rv);
	}

	return (tp != NULL);
}

static bool
tcp_timer_delack(struct tcpcb *tp)
{
	struct epoch_tracker et;
#if defined(INVARIANTS) || defined(VIMAGE)
	struct inpcb *inp = tptoinpcb(tp);
#endif
	bool rv;

	INP_WLOCK_ASSERT(inp);

	CURVNET_SET(inp->inp_vnet);
	tp->t_flags |= TF_ACKNOW;
	TCPSTAT_INC(tcps_delack);
	NET_EPOCH_ENTER(et);
	rv = tcp_output_locked(tp);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (rv);
}

static bool
tcp_timer_2msl(struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);
	bool close = false;

	INP_WLOCK_ASSERT(inp);

	TCP_PROBE2(debug__user, tp, PRU_SLOWTIMO);
	CURVNET_SET(inp->inp_vnet);
	tcp_log_end_status(tp, TCP_EI_STATUS_2MSL);
	tcp_free_sackholes(tp);
	/*
	 * 2 MSL timeout in shutdown went off.  If we're closed but
	 * still waiting for peer to close and connection has been idle
	 * too long delete connection control block.  Otherwise, check
	 * again in a bit.
	 *
	 * If fastrecycle of FIN_WAIT_2, in FIN_WAIT_2 and receiver has closed,
	 * there's no point in hanging onto FIN_WAIT_2 socket. Just close it.
	 * Ignore fact that there were recent incoming segments.
	 *
	 * XXXGL: check if inp_socket shall always be !NULL here?
	 */
	if (tp->t_state == TCPS_TIME_WAIT) {
		close = true;
	} else if (tp->t_state == TCPS_FIN_WAIT_2 &&
	    tcp_fast_finwait2_recycle && inp->inp_socket &&
	    (inp->inp_socket->so_rcv.sb_state & SBS_CANTRCVMORE)) {
		TCPSTAT_INC(tcps_finwait2_drops);
		close = true;
	} else {
		if (ticks - tp->t_rcvtime <= TP_MAXIDLE(tp))
			tcp_timer_activate(tp, TT_2MSL, TP_KEEPINTVL(tp));
		else
			close = true;
	}
	if (close) {
		struct epoch_tracker et;

		NET_EPOCH_ENTER(et);
		tp = tcp_close(tp);
		NET_EPOCH_EXIT(et);
	}
	CURVNET_RESTORE();

	return (tp != NULL);
}

static bool
tcp_timer_keep(struct tcpcb *tp)
{
	struct epoch_tracker et;
	struct inpcb *inp = tptoinpcb(tp);
	struct tcptemp *t_template;

	INP_WLOCK_ASSERT(inp);

	TCP_PROBE2(debug__user, tp, PRU_SLOWTIMO);
	CURVNET_SET(inp->inp_vnet);
	/*
	 * Because we don't regularly reset the keepalive callout in
	 * the ESTABLISHED state, it may be that we don't actually need
	 * to send a keepalive yet. If that occurs, schedule another
	 * call for the next time the keepalive timer might expire.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state)) {
		u_int idletime;

		idletime = ticks - tp->t_rcvtime;
		if (idletime < TP_KEEPIDLE(tp)) {
			tcp_timer_activate(tp, TT_KEEP,
			    TP_KEEPIDLE(tp) - idletime);
			CURVNET_RESTORE();
			return (true);
		}
	}

	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 */
	TCPSTAT_INC(tcps_keeptimeo);
	if (tp->t_state < TCPS_ESTABLISHED)
		goto dropit;
	if ((V_tcp_always_keepalive ||
	    inp->inp_socket->so_options & SO_KEEPALIVE) &&
	    tp->t_state <= TCPS_CLOSING) {
		if (ticks - tp->t_rcvtime >= TP_KEEPIDLE(tp) + TP_MAXIDLE(tp))
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
		TCPSTAT_INC(tcps_keepprobe);
		t_template = tcpip_maketemplate(inp);
		if (t_template) {
			NET_EPOCH_ENTER(et);
			tcp_respond(tp, t_template->tt_ipgen,
				    &t_template->tt_t, (struct mbuf *)NULL,
				    tp->rcv_nxt, tp->snd_una - 1, 0);
			NET_EPOCH_EXIT(et);
			free(t_template, M_TEMP);
		}
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPINTVL(tp));
	} else
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));

	CURVNET_RESTORE();
	return (true);

dropit:
	TCPSTAT_INC(tcps_keepdrops);
	NET_EPOCH_ENTER(et);
	tcp_log_end_status(tp, TCP_EI_STATUS_KEEP_MAX);
	tp = tcp_drop(tp, ETIMEDOUT);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (tp != NULL);
}

/*
 * Has this session exceeded the maximum time without seeing a substantive
 * acknowledgement? If so, return true; otherwise false.
 */
static bool
tcp_maxunacktime_check(struct tcpcb *tp)
{

	/* Are we tracking this timer for this session? */
	if (TP_MAXUNACKTIME(tp) == 0)
		return false;

	/* Do we have a current measurement. */
	if (tp->t_acktime == 0)
		return false;

	/* Are we within the acceptable range? */
	if (TSTMP_GT(TP_MAXUNACKTIME(tp) + tp->t_acktime, (u_int)ticks))
		return false;

	/* We exceeded the timer. */
	TCPSTAT_INC(tcps_progdrops);
	return true;
}

static bool
tcp_timer_persist(struct tcpcb *tp)
{
	struct epoch_tracker et;
#if defined(INVARIANTS) || defined(VIMAGE)
	struct inpcb *inp = tptoinpcb(tp);
#endif
	bool progdrop, rv;

	INP_WLOCK_ASSERT(inp);

	TCP_PROBE2(debug__user, tp, PRU_SLOWTIMO);
	CURVNET_SET(inp->inp_vnet);
	/*
	 * Persistence timer into zero window.
	 * Force a byte to be output, if possible.
	 */
	TCPSTAT_INC(tcps_persisttimeo);
	/*
	 * Hack: if the peer is dead/unreachable, we do not
	 * time out if the window is closed.  After a full
	 * backoff, drop the connection if the idle time
	 * (no responses to probes) reaches the maximum
	 * backoff that we would use if retransmitting.
	 * Also, drop the connection if we haven't been making
	 * progress.
	 */
	progdrop = tcp_maxunacktime_check(tp);
	if (progdrop || (tp->t_rxtshift >= V_tcp_retries &&
	    (ticks - tp->t_rcvtime >= tcp_maxpersistidle ||
	     ticks - tp->t_rcvtime >= TCP_REXMTVAL(tp) * tcp_totbackoff))) {
		if (!progdrop)
			TCPSTAT_INC(tcps_persistdrop);
		tcp_log_end_status(tp, TCP_EI_STATUS_PERSIST_MAX);
		goto dropit;
	}
	/*
	 * If the user has closed the socket then drop a persisting
	 * connection after a much reduced timeout.
	 */
	if (tp->t_state > TCPS_CLOSE_WAIT &&
	    (ticks - tp->t_rcvtime) >= TCPTV_PERSMAX) {
		TCPSTAT_INC(tcps_persistdrop);
		tcp_log_end_status(tp, TCP_EI_STATUS_PERSIST_MAX);
		goto dropit;
	}
	tcp_setpersist(tp);
	tp->t_flags |= TF_FORCEDATA;
	NET_EPOCH_ENTER(et);
	if ((rv = tcp_output_locked(tp)))
		tp->t_flags &= ~TF_FORCEDATA;
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (rv);

dropit:
	NET_EPOCH_ENTER(et);
	tp = tcp_drop(tp, ETIMEDOUT);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (tp != NULL);
}

static bool
tcp_timer_rexmt(struct tcpcb *tp)
{
	struct epoch_tracker et;
	struct inpcb *inp = tptoinpcb(tp);
	int rexmt;
	bool isipv6, rv;

	INP_WLOCK_ASSERT(inp);

	TCP_PROBE2(debug__user, tp, PRU_SLOWTIMO);
	CURVNET_SET(inp->inp_vnet);
	if (tp->t_fb->tfb_tcp_rexmit_tmr) {
		/* The stack has a timer action too. */
		(*tp->t_fb->tfb_tcp_rexmit_tmr)(tp);
	}
	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one segment.
	 *
	 * If we've either exceeded the maximum number of retransmissions,
	 * or we've gone long enough without making progress, then drop
	 * the session.
	 */
	if (++tp->t_rxtshift > V_tcp_retries || tcp_maxunacktime_check(tp)) {
		if (tp->t_rxtshift > V_tcp_retries)
			TCPSTAT_INC(tcps_timeoutdrop);
		tp->t_rxtshift = V_tcp_retries;
		tcp_log_end_status(tp, TCP_EI_STATUS_RETRAN);
		NET_EPOCH_ENTER(et);
		tp = tcp_drop(tp, ETIMEDOUT);
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();

		return (tp != NULL);
	}
	if (tp->t_state == TCPS_SYN_SENT) {
		/*
		 * If the SYN was retransmitted, indicate CWND to be
		 * limited to 1 segment in cc_conn_init().
		 */
		tp->snd_cwnd = 1;
	} else if (tp->t_rxtshift == 1) {
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
		if (IN_FASTRECOVERY(tp->t_flags))
			tp->t_flags |= TF_WASFRECOVERY;
		else
			tp->t_flags &= ~TF_WASFRECOVERY;
		if (IN_CONGRECOVERY(tp->t_flags))
			tp->t_flags |= TF_WASCRECOVERY;
		else
			tp->t_flags &= ~TF_WASCRECOVERY;
		if ((tp->t_flags & TF_RCVD_TSTMP) == 0)
			tp->t_badrxtwin = ticks + (tp->t_srtt >> (TCP_RTT_SHIFT + 1));
		/* In the event that we've negotiated timestamps
		 * badrxtwin will be set to the value that we set
		 * the retransmitted packet's to_tsval to by tcp_output
		 */
		tp->t_flags |= TF_PREVVALID;
		tcp_resend_sackholes(tp);
	} else {
		tp->t_flags &= ~TF_PREVVALID;
		tcp_free_sackholes(tp);
	}
	TCPSTAT_INC(tcps_rexmttimeo);
	if ((tp->t_state == TCPS_SYN_SENT) ||
	    (tp->t_state == TCPS_SYN_RECEIVED))
		rexmt = tcp_rexmit_initial * tcp_backoff[tp->t_rxtshift];
	else
		rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
	TCPT_RANGESET(tp->t_rxtcur, rexmt,
		      tp->t_rttmin, TCPTV_REXMTMAX);

	/*
	 * We enter the path for PLMTUD if connection is established or, if
	 * connection is FIN_WAIT_1 status, reason for the last is that if
	 * amount of data we send is very small, we could send it in couple of
	 * packets and process straight to FIN. In that case we won't catch
	 * ESTABLISHED state.
	 */
#ifdef INET6
	isipv6 = (inp->inp_vflag & INP_IPV6) ? true : false;
#else
	isipv6 = false;
#endif
	if (((V_tcp_pmtud_blackhole_detect == 1) ||
	    (V_tcp_pmtud_blackhole_detect == 2 && !isipv6) ||
	    (V_tcp_pmtud_blackhole_detect == 3 && isipv6)) &&
	    ((tp->t_state == TCPS_ESTABLISHED) ||
	    (tp->t_state == TCPS_FIN_WAIT_1))) {
		if (tp->t_rxtshift == 1) {
			/*
			 * We enter blackhole detection after the first
			 * unsuccessful timer based retransmission.
			 * Then we reduce up to two times the MSS, each
			 * candidate giving two tries of retransmissions.
			 * But we give a candidate only two tries, if it
			 * actually reduces the MSS.
			 */
			tp->t_blackhole_enter = 2;
			tp->t_blackhole_exit = tp->t_blackhole_enter;
			if (isipv6) {
#ifdef INET6
				if (tp->t_maxseg > V_tcp_v6pmtud_blackhole_mss)
					tp->t_blackhole_exit += 2;
				if (tp->t_maxseg > V_tcp_v6mssdflt &&
				    V_tcp_v6pmtud_blackhole_mss > V_tcp_v6mssdflt)
					tp->t_blackhole_exit += 2;
#endif
			} else {
#ifdef INET
				if (tp->t_maxseg > V_tcp_pmtud_blackhole_mss)
					tp->t_blackhole_exit += 2;
				if (tp->t_maxseg > V_tcp_mssdflt &&
				    V_tcp_pmtud_blackhole_mss > V_tcp_mssdflt)
					tp->t_blackhole_exit += 2;
#endif
			}
		}
		if (((tp->t_flags2 & (TF2_PLPMTU_PMTUD|TF2_PLPMTU_MAXSEGSNT)) ==
		    (TF2_PLPMTU_PMTUD|TF2_PLPMTU_MAXSEGSNT)) &&
		    (tp->t_rxtshift >= tp->t_blackhole_enter &&
		    tp->t_rxtshift < tp->t_blackhole_exit &&
		    (tp->t_rxtshift - tp->t_blackhole_enter) % 2 == 0)) {
			/*
			 * Enter Path MTU Black-hole Detection mechanism:
			 * - Disable Path MTU Discovery (IP "DF" bit).
			 * - Reduce MTU to lower value than what we
			 *   negotiated with peer.
			 */
			if ((tp->t_flags2 & TF2_PLPMTU_BLACKHOLE) == 0) {
				/* Record that we may have found a black hole. */
				tp->t_flags2 |= TF2_PLPMTU_BLACKHOLE;
				/* Keep track of previous MSS. */
				tp->t_pmtud_saved_maxseg = tp->t_maxseg;
			}

			/*
			 * Reduce the MSS to blackhole value or to the default
			 * in an attempt to retransmit.
			 */
#ifdef INET6
			if (isipv6 &&
			    tp->t_maxseg > V_tcp_v6pmtud_blackhole_mss &&
			    V_tcp_v6pmtud_blackhole_mss > V_tcp_v6mssdflt) {
				/* Use the sysctl tuneable blackhole MSS. */
				tp->t_maxseg = V_tcp_v6pmtud_blackhole_mss;
				TCPSTAT_INC(tcps_pmtud_blackhole_activated);
			} else if (isipv6) {
				/* Use the default MSS. */
				tp->t_maxseg = V_tcp_v6mssdflt;
				/*
				 * Disable Path MTU Discovery when we switch to
				 * minmss.
				 */
				tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
				TCPSTAT_INC(tcps_pmtud_blackhole_activated_min_mss);
			}
#endif
#if defined(INET6) && defined(INET)
			else
#endif
#ifdef INET
			if (tp->t_maxseg > V_tcp_pmtud_blackhole_mss &&
			    V_tcp_pmtud_blackhole_mss > V_tcp_mssdflt) {
				/* Use the sysctl tuneable blackhole MSS. */
				tp->t_maxseg = V_tcp_pmtud_blackhole_mss;
				TCPSTAT_INC(tcps_pmtud_blackhole_activated);
			} else {
				/* Use the default MSS. */
				tp->t_maxseg = V_tcp_mssdflt;
				/*
				 * Disable Path MTU Discovery when we switch to
				 * minmss.
				 */
				tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
				TCPSTAT_INC(tcps_pmtud_blackhole_activated_min_mss);
			}
#endif
			/*
			 * Reset the slow-start flight size
			 * as it may depend on the new MSS.
			 */
			if (CC_ALGO(tp)->conn_init != NULL)
				CC_ALGO(tp)->conn_init(&tp->t_ccv);
		} else {
			/*
			 * If further retransmissions are still unsuccessful
			 * with a lowered MTU, maybe this isn't a blackhole and
			 * we restore the previous MSS and blackhole detection
			 * flags.
			 */
			if ((tp->t_flags2 & TF2_PLPMTU_BLACKHOLE) &&
			    (tp->t_rxtshift >= tp->t_blackhole_exit)) {
				tp->t_flags2 |= TF2_PLPMTU_PMTUD;
				tp->t_flags2 &= ~TF2_PLPMTU_BLACKHOLE;
				tp->t_maxseg = tp->t_pmtud_saved_maxseg;
				TCPSTAT_INC(tcps_pmtud_blackhole_failed);
				/*
				 * Reset the slow-start flight size as it
				 * may depend on the new MSS.
				 */
				if (CC_ALGO(tp)->conn_init != NULL)
					CC_ALGO(tp)->conn_init(&tp->t_ccv);
			}
		}
	}

	/*
	 * Disable RFC1323 and SACK if we haven't got any response to
	 * our third SYN to work-around some broken terminal servers
	 * (most of which have hopefully been retired) that have bad VJ
	 * header compression code which trashes TCP segments containing
	 * unknown-to-them TCP options.
	 */
	if (tcp_rexmit_drop_options && (tp->t_state == TCPS_SYN_SENT) &&
	    (tp->t_rxtshift == 3))
		tp->t_flags &= ~(TF_REQ_SCALE|TF_REQ_TSTMP|TF_SACK_PERMIT);
	/*
	 * If we backed off this far, notify the L3 protocol that we're having
	 * connection problems.
	 */
	if (tp->t_rxtshift > TCP_RTT_INVALIDATE) {
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV6) != 0)
			in6_losing(inp);
		else
#endif
			in_losing(inp);
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

	cc_cong_signal(tp, NULL, CC_RTO);
	NET_EPOCH_ENTER(et);
	rv = tcp_output_locked(tp);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (rv);
}

static void
tcp_bblog_timer(struct tcpcb *tp, tt_which which, tt_what what, uint32_t ticks)
{
	struct tcp_log_buffer *lgb;
	uint64_t ms;

	INP_WLOCK_ASSERT(tptoinpcb(tp));
	if (tcp_bblogging_on(tp))
		lgb = tcp_log_event(tp, NULL, NULL, NULL, TCP_LOG_RTO, 0, 0,
		    NULL, false, NULL, NULL, 0, NULL);
	else
		lgb = NULL;
	if (lgb != NULL) {
		lgb->tlb_flex1 = (what << 8) | which;
		if (what == TT_STARTING) {
			/* Convert ticks to ms and store it in tlb_flex2. */
			if (hz == 1000)
				lgb->tlb_flex2 = ticks;
			else {
				ms = (((uint64_t)ticks * 1000) + (hz - 1)) / hz;
				if (ms > UINT32_MAX)
					lgb->tlb_flex2 = UINT32_MAX;
				else
					lgb->tlb_flex2 = (uint32_t)ms;
			}
		}
	}
}

static inline tt_which
tcp_timer_next(struct tcpcb *tp, sbintime_t *precision)
{
	tt_which i, rv;
	sbintime_t after, before;

	for (i = 0, rv = TT_N, after = before = SBT_MAX; i < TT_N; i++) {
		if (tp->t_timers[i] < after) {
			after = tp->t_timers[i];
			rv = i;
		}
		before = MIN(before, tp->t_timers[i] + tp->t_precisions[i]);
	}
	if (precision != NULL)
		*precision = before - after;

	return (rv);
}

static void
tcp_timer_enter(void *xtp)
{
	struct tcpcb *tp = xtp;
	struct inpcb *inp = tptoinpcb(tp);
	sbintime_t precision;
	tt_which which;
	bool tp_valid;

	INP_WLOCK_ASSERT(inp);
	MPASS((curthread->td_pflags & TDP_INTCPCALLOUT) == 0);

	curthread->td_pflags |= TDP_INTCPCALLOUT;

	which = tcp_timer_next(tp, NULL);
	MPASS(which < TT_N);
	tp->t_timers[which] = SBT_MAX;
	tp->t_precisions[which] = 0;

	tcp_bblog_timer(tp, which, TT_PROCESSING, 0);
	tp_valid = tcp_timersw[which](tp);
	if (tp_valid) {
		tcp_bblog_timer(tp, which, TT_PROCESSED, 0);
		if ((which = tcp_timer_next(tp, &precision)) != TT_N) {
			callout_reset_sbt_on(&tp->t_callout,
			    tp->t_timers[which], precision, tcp_timer_enter,
			    tp, inp_to_cpuid(inp), C_ABSOLUTE);
		}
		INP_WUNLOCK(inp);
	}

	curthread->td_pflags &= ~TDP_INTCPCALLOUT;
}

/*
 * Activate or stop (delta == 0) a TCP timer.
 */
void
tcp_timer_activate(struct tcpcb *tp, tt_which which, u_int delta)
{
	struct inpcb *inp = tptoinpcb(tp);
	sbintime_t precision;
	tt_what what;

#ifdef TCP_OFFLOAD
	if (tp->t_flags & TF_TOE)
		return;
#endif

	INP_WLOCK_ASSERT(inp);
	MPASS(tp->t_state > TCPS_CLOSED);

	if (delta > 0) {
		what = TT_STARTING;
		callout_when(tick_sbt * delta, 0, C_HARDCLOCK,
		    &tp->t_timers[which], &tp->t_precisions[which]);
	} else {
		what = TT_STOPPING;
		tp->t_timers[which] = SBT_MAX;
	}
	tcp_bblog_timer(tp, which, what, delta);

	if ((which = tcp_timer_next(tp, &precision)) != TT_N)
		callout_reset_sbt_on(&tp->t_callout, tp->t_timers[which],
		    precision, tcp_timer_enter, tp, inp_to_cpuid(inp),
		    C_ABSOLUTE);
	else
		callout_stop(&tp->t_callout);
}

bool
tcp_timer_active(struct tcpcb *tp, tt_which which)
{

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	return (tp->t_timers[which] != SBT_MAX);
}

/*
 * Stop all timers associated with tcpcb.
 *
 * Called only on tcpcb destruction.  The tcpcb shall already be dropped from
 * the pcb lookup database and socket is not losing the last reference.
 *
 * XXXGL: unfortunately our callout(9) is not able to fully stop a locked
 * callout even when only two threads are involved: the callout itself and the
 * thread that does callout_stop().  See where softclock_call_cc() swaps the
 * callwheel lock to callout lock and then checks cc_exec_cancel().  This is
 * the race window.  If it happens, the tcp_timer_enter() won't be executed,
 * however pcb lock will be locked and released, hence we can't free memory.
 * Until callout(9) is improved, just keep retrying.  In my profiling I've seen
 * such event happening less than 1 time per hour with 20-30 Gbit/s of traffic.
 */
void
tcp_timer_stop(struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);

	INP_WLOCK_ASSERT(inp);

	if (curthread->td_pflags & TDP_INTCPCALLOUT) {
		int stopped __diagused;

		stopped = callout_stop(&tp->t_callout);
		MPASS(stopped == 0);
	} else while(__predict_false(callout_stop(&tp->t_callout) == 0)) {
		INP_WUNLOCK(inp);
		kern_yield(PRI_UNCHANGED);
		INP_WLOCK(inp);
	}
}
