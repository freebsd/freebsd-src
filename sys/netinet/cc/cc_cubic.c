/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Lawrence Stewart while studying at the Centre
 * for Advanced Internet Architectures, Swinburne University of Technology, made
 * possible in part by a grant from the Cisco University Research Program Fund
 * at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by David Hayes under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * An implementation of the CUBIC congestion control algorithm for FreeBSD,
 * based on the Internet Draft "draft-rhee-tcpm-cubic-02" by Rhee, Xu and Ha.
 * Originally released as part of the NewTCP research project at Swinburne
 * University of Technology's Centre for Advanced Internet Architectures,
 * Melbourne, Australia, which was made possible in part by a grant from the
 * Cisco University Research Program Fund at Community Foundation Silicon
 * Valley. More details are available at:
 *   http://caia.swin.edu.au/urp/newtcp/
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/vnet.h>

#include <net/route.h>
#include <net/route/nhop.h>

#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_hpts.h>
#include <netinet/cc/cc.h>
#include <netinet/cc/cc_cubic.h>
#include <netinet/cc/cc_module.h>

static void	cubic_ack_received(struct cc_var *ccv, uint16_t type);
static void	cubic_cb_destroy(struct cc_var *ccv);
static int	cubic_cb_init(struct cc_var *ccv, void *ptr);
static void	cubic_cong_signal(struct cc_var *ccv, uint32_t type);
static void	cubic_conn_init(struct cc_var *ccv);
static int	cubic_mod_init(void);
static void	cubic_post_recovery(struct cc_var *ccv);
static void	cubic_record_rtt(struct cc_var *ccv);
static void	cubic_ssthresh_update(struct cc_var *ccv, uint32_t maxseg);
static void	cubic_after_idle(struct cc_var *ccv);
static size_t	cubic_data_sz(void);
static void	cubic_newround(struct cc_var *ccv, uint32_t round_cnt);
static void	cubic_rttsample(struct cc_var *ccv, uint32_t usec_rtt,
       uint32_t rxtcnt, uint32_t fas);

struct cc_algo cubic_cc_algo = {
	.name = "cubic",
	.ack_received = cubic_ack_received,
	.cb_destroy = cubic_cb_destroy,
	.cb_init = cubic_cb_init,
	.cong_signal = cubic_cong_signal,
	.conn_init = cubic_conn_init,
	.mod_init = cubic_mod_init,
	.post_recovery = cubic_post_recovery,
	.after_idle = cubic_after_idle,
	.cc_data_sz = cubic_data_sz,
	.rttsample = cubic_rttsample,
	.newround = cubic_newround
};

static void
cubic_log_hystart_event(struct cc_var *ccv, struct cubic *cubicd, uint8_t mod, uint32_t flex1)
{
	/*
	 * Types of logs (mod value)
	 * 1 - rtt_thresh in flex1, checking to see if RTT is to great.
	 * 2 - rtt is too great, rtt_thresh in flex1.
	 * 3 - CSS is active incr in flex1
	 * 4 - A new round is beginning flex1 is round count
	 * 5 - A new RTT measurement flex1 is the new measurement.
	 * 6 - We enter CA ssthresh is also in flex1.
	 * 7 - Socket option to change hystart executed opt.val in flex1.
	 * 8 - Back out of CSS into SS, flex1 is the css_baseline_minrtt
	 * 9 - We enter CA, via an ECN mark.
	 * 10 - We enter CA, via a loss.
	 * 11 - We have slipped out of SS into CA via cwnd growth.
	 * 12 - After idle has re-enabled hystart++
	 */
	struct tcpcb *tp;

	if (hystart_bblogs == 0)
		return;
	tp = ccv->ccvc.tcp;
	if (tcp_bblogging_on(tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = flex1;
		log.u_bbr.flex2 = cubicd->css_current_round_minrtt;
		log.u_bbr.flex3 = cubicd->css_lastround_minrtt;
		log.u_bbr.flex4 = cubicd->css_rttsample_count;
		log.u_bbr.flex5 = cubicd->css_entered_at_round;
		log.u_bbr.flex6 = cubicd->css_baseline_minrtt;
		/* We only need bottom 16 bits of flags */
		log.u_bbr.flex7 = cubicd->flags & 0x0000ffff;
		log.u_bbr.flex8 = mod;
		log.u_bbr.epoch = cubicd->css_current_round;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.lt_epoch = cubicd->css_fas_at_css_entry;
		log.u_bbr.pkts_out = cubicd->css_last_fas;
		log.u_bbr.delivered = cubicd->css_lowrtt_fas;
		log.u_bbr.pkt_epoch = ccv->flags;
		TCP_LOG_EVENTP(tp, NULL,
		    &tptosocket(tp)->so_rcv,
		    &tptosocket(tp)->so_snd,
		    TCP_HYSTART, 0,
		    0, &log, false, &tv);
	}
}

static void
cubic_does_slow_start(struct cc_var *ccv, struct cubic *cubicd)
{
	/*
	 * In slow-start with ABC enabled and no RTO in sight?
	 * (Must not use abc_l_var > 1 if slow starting after
	 * an RTO. On RTO, snd_nxt = snd_una, so the
	 * snd_nxt == snd_max check is sufficient to
	 * handle this).
	 *
	 * XXXLAS: Find a way to signal SS after RTO that
	 * doesn't rely on tcpcb vars.
	 */
	u_int cw = CCV(ccv, snd_cwnd);
	u_int incr = CCV(ccv, t_maxseg);
	uint16_t abc_val;

	cubicd->flags |= CUBICFLAG_IN_SLOWSTART;
	if (ccv->flags & CCF_USE_LOCAL_ABC)
		abc_val = ccv->labc;
	else
		abc_val = V_tcp_abc_l_var;
	if ((ccv->flags & CCF_HYSTART_ALLOWED) &&
	    (cubicd->flags & CUBICFLAG_HYSTART_ENABLED) &&
	    ((cubicd->flags & CUBICFLAG_HYSTART_IN_CSS) == 0)) {
		/*
		 * Hystart is allowed and still enabled and we are not yet
		 * in CSS. Lets check to see if we can make a decision on
		 * if we need to go into CSS.
		 */
		if ((cubicd->css_rttsample_count >= hystart_n_rttsamples) &&
		    (cubicd->css_current_round_minrtt != 0xffffffff) &&
		    (cubicd->css_lastround_minrtt != 0xffffffff)) {
			uint32_t rtt_thresh;

			/* Clamp (minrtt_thresh, lastround/8, maxrtt_thresh) */
			rtt_thresh = (cubicd->css_lastround_minrtt >> 3);
			if (rtt_thresh < hystart_minrtt_thresh)
				rtt_thresh = hystart_minrtt_thresh;
			if (rtt_thresh > hystart_maxrtt_thresh)
				rtt_thresh = hystart_maxrtt_thresh;
			cubic_log_hystart_event(ccv, cubicd, 1, rtt_thresh);

			if (cubicd->css_current_round_minrtt >= (cubicd->css_lastround_minrtt + rtt_thresh)) {
				/* Enter CSS */
				cubicd->flags |= CUBICFLAG_HYSTART_IN_CSS;
				cubicd->css_fas_at_css_entry = cubicd->css_lowrtt_fas;
				/* 
				 * The draft (v4) calls for us to set baseline to css_current_round_min
				 * but that can cause an oscillation. We probably shoudl be using
				 * css_lastround_minrtt, but the authors insist that will cause
				 * issues on exiting early. We will leave the draft version for now
				 * but I suspect this is incorrect.
				 */
				cubicd->css_baseline_minrtt = cubicd->css_current_round_minrtt;
				cubicd->css_entered_at_round = cubicd->css_current_round;
				cubic_log_hystart_event(ccv, cubicd, 2, rtt_thresh);
			}
		}
	}
	if (CCV(ccv, snd_nxt) == CCV(ccv, snd_max))
		incr = min(ccv->bytes_this_ack,
			   ccv->nsegs * abc_val *
			   CCV(ccv, t_maxseg));
	else
		incr = min(ccv->bytes_this_ack, CCV(ccv, t_maxseg));

	/* Only if Hystart is enabled will the flag get set */
	if (cubicd->flags & CUBICFLAG_HYSTART_IN_CSS) {
		incr /= hystart_css_growth_div;
		cubic_log_hystart_event(ccv, cubicd, 3, incr);
	}
	/* ABC is on by default, so incr equals 0 frequently. */
	if (incr > 0)
		CCV(ccv, snd_cwnd) = min((cw + incr),
					 TCP_MAXWIN << CCV(ccv, snd_scale));
}

static void
cubic_ack_received(struct cc_var *ccv, uint16_t type)
{
	struct cubic *cubic_data;
	unsigned long W_est, W_cubic;
	int usecs_since_epoch;

	cubic_data = ccv->cc_data;
	cubic_record_rtt(ccv);

	/*
	 * For a regular ACK and we're not in cong/fast recovery and
	 * we're cwnd limited, always recalculate cwnd.
	 */
	if (type == CC_ACK && !IN_RECOVERY(CCV(ccv, t_flags)) &&
	    (ccv->flags & CCF_CWND_LIMITED)) {
		 /* Use the logic in NewReno ack_received() for slow start. */
		if (CCV(ccv, snd_cwnd) <= CCV(ccv, snd_ssthresh) ||
		    cubic_data->min_rtt_usecs == TCPTV_SRTTBASE) {
			cubic_does_slow_start(ccv, cubic_data);
		} else {
			if (cubic_data->flags & CUBICFLAG_HYSTART_IN_CSS) {
				/*
				 * We have slipped into CA with
				 * CSS active. Deactivate all.
				 */
				/* Turn off the CSS flag */
				cubic_data->flags &= ~CUBICFLAG_HYSTART_IN_CSS;
				/* Disable use of CSS in the future except long idle  */
				cubic_data->flags &= ~CUBICFLAG_HYSTART_ENABLED;
				cubic_log_hystart_event(ccv, cubic_data, 11, CCV(ccv, snd_ssthresh));
			}
			if ((cubic_data->flags & CUBICFLAG_RTO_EVENT) &&
			    (cubic_data->flags & CUBICFLAG_IN_SLOWSTART)) {
				/* RFC8312 Section 4.7 */
				cubic_data->flags &= ~(CUBICFLAG_RTO_EVENT |
						       CUBICFLAG_IN_SLOWSTART);
				cubic_data->W_max = CCV(ccv, snd_cwnd);
				cubic_data->K = 0;
			} else if (cubic_data->flags & (CUBICFLAG_IN_SLOWSTART |
						 CUBICFLAG_IN_APPLIMIT)) {
				cubic_data->flags &= ~(CUBICFLAG_IN_SLOWSTART |
						       CUBICFLAG_IN_APPLIMIT);
				cubic_data->t_epoch = ticks;
				cubic_data->K = cubic_k(cubic_data->W_max /
							CCV(ccv, t_maxseg));
			}
			usecs_since_epoch = (ticks - cubic_data->t_epoch) * tick;
			if (usecs_since_epoch < 0) {
				/*
				 * dragging t_epoch along
				 */
				usecs_since_epoch = INT_MAX;
				cubic_data->t_epoch = ticks - INT_MAX;
			}
			/*
			 * The mean RTT is used to best reflect the equations in
			 * the I-D. Using min_rtt in the tf_cwnd calculation
			 * causes W_est to grow much faster than it should if the
			 * RTT is dominated by network buffering rather than
			 * propagation delay.
			 */
			W_est = tf_cwnd(usecs_since_epoch, cubic_data->mean_rtt_usecs,
				       cubic_data->W_max, CCV(ccv, t_maxseg));

			W_cubic = cubic_cwnd(usecs_since_epoch +
					     cubic_data->mean_rtt_usecs,
					     cubic_data->W_max,
					     CCV(ccv, t_maxseg),
					     cubic_data->K);

			ccv->flags &= ~CCF_ABC_SENTAWND;

			if (W_cubic < W_est) {
				/*
				 * TCP-friendly region, follow tf
				 * cwnd growth.
				 */
				if (CCV(ccv, snd_cwnd) < W_est)
					CCV(ccv, snd_cwnd) = ulmin(W_est, INT_MAX);
			} else if (CCV(ccv, snd_cwnd) < W_cubic) {
				/*
				 * Concave or convex region, follow CUBIC
				 * cwnd growth.
				 * Only update snd_cwnd, if it doesn't shrink.
				 */
				CCV(ccv, snd_cwnd) = ulmin(W_cubic, INT_MAX);
			}

			/*
			 * If we're not in slow start and we're probing for a
			 * new cwnd limit at the start of a connection
			 * (happens when hostcache has a relevant entry),
			 * keep updating our current estimate of the
			 * W_max.
			 */
			if (((cubic_data->flags & CUBICFLAG_CONG_EVENT) == 0) &&
			    cubic_data->W_max < CCV(ccv, snd_cwnd)) {
				cubic_data->W_max = CCV(ccv, snd_cwnd);
				cubic_data->K = cubic_k(cubic_data->W_max /
				    CCV(ccv, t_maxseg));
			}
		}
	} else if (type == CC_ACK && !IN_RECOVERY(CCV(ccv, t_flags)) &&
	    !(ccv->flags & CCF_CWND_LIMITED)) {
		cubic_data->flags |= CUBICFLAG_IN_APPLIMIT;
	}
}

/*
 * This is a CUBIC specific implementation of after_idle.
 *   - Reset cwnd by calling New Reno implementation of after_idle.
 *   - Reset t_epoch.
 */
static void
cubic_after_idle(struct cc_var *ccv)
{
	struct cubic *cubic_data;

	cubic_data = ccv->cc_data;

	cubic_data->W_max = ulmax(cubic_data->W_max, CCV(ccv, snd_cwnd));
	cubic_data->K = cubic_k(cubic_data->W_max / CCV(ccv, t_maxseg));
	if ((cubic_data->flags & CUBICFLAG_HYSTART_ENABLED) == 0) {
		/*
		 * Re-enable hystart if we have been idle.
		 */
		cubic_data->flags &= ~CUBICFLAG_HYSTART_IN_CSS;
		cubic_data->flags |= CUBICFLAG_HYSTART_ENABLED;
		cubic_log_hystart_event(ccv, cubic_data, 12, CCV(ccv, snd_ssthresh));
	}
	newreno_cc_after_idle(ccv);
	cubic_data->t_epoch = ticks;
}

static void
cubic_cb_destroy(struct cc_var *ccv)
{
	free(ccv->cc_data, M_CC_MEM);
}

static size_t
cubic_data_sz(void)
{
	return (sizeof(struct cubic));
}

static int
cubic_cb_init(struct cc_var *ccv, void *ptr)
{
	struct cubic *cubic_data;

	INP_WLOCK_ASSERT(tptoinpcb(ccv->ccvc.tcp));
	if (ptr == NULL) {
		cubic_data = malloc(sizeof(struct cubic), M_CC_MEM, M_NOWAIT|M_ZERO);
		if (cubic_data == NULL)
			return (ENOMEM);
	} else
		cubic_data = ptr;

	/* Init some key variables with sensible defaults. */
	cubic_data->t_epoch = ticks;
	cubic_data->min_rtt_usecs = TCPTV_SRTTBASE;
	cubic_data->mean_rtt_usecs = 1;

	ccv->cc_data = cubic_data;
	cubic_data->flags = CUBICFLAG_HYSTART_ENABLED;
	/* At init set both to infinity */
	cubic_data->css_lastround_minrtt = 0xffffffff;
	cubic_data->css_current_round_minrtt = 0xffffffff;
	cubic_data->css_current_round = 0;
	cubic_data->css_baseline_minrtt = 0xffffffff;
	cubic_data->css_rttsample_count = 0;
	cubic_data->css_entered_at_round = 0;
	cubic_data->css_fas_at_css_entry = 0;
	cubic_data->css_lowrtt_fas = 0;
	cubic_data->css_last_fas = 0;

	return (0);
}

/*
 * Perform any necessary tasks before we enter congestion recovery.
 */
static void
cubic_cong_signal(struct cc_var *ccv, uint32_t type)
{
	struct cubic *cubic_data;
	uint32_t mss, pipe;

	cubic_data = ccv->cc_data;
	mss = tcp_fixed_maxseg(ccv->ccvc.tcp);

	switch (type) {
	case CC_NDUPACK:
		if (cubic_data->flags & CUBICFLAG_HYSTART_ENABLED) {
			/* Make sure the flags are all off we had a loss */
			cubic_data->flags &= ~CUBICFLAG_HYSTART_ENABLED;
			cubic_data->flags &= ~CUBICFLAG_HYSTART_IN_CSS;
			cubic_log_hystart_event(ccv, cubic_data, 10, CCV(ccv, snd_ssthresh));
		}
		if (!IN_FASTRECOVERY(CCV(ccv, t_flags))) {
			if (!IN_CONGRECOVERY(CCV(ccv, t_flags))) {
				cubic_ssthresh_update(ccv, mss);
				cubic_data->flags |= CUBICFLAG_CONG_EVENT;
				cubic_data->t_epoch = ticks;
				cubic_data->K = cubic_k(cubic_data->W_max / mss);
			}
			ENTER_RECOVERY(CCV(ccv, t_flags));
		}
		break;

	case CC_ECN:
		if (cubic_data->flags & CUBICFLAG_HYSTART_ENABLED) {
			/* Make sure the flags are all off we had a loss */
			cubic_data->flags &= ~CUBICFLAG_HYSTART_ENABLED;
			cubic_data->flags &= ~CUBICFLAG_HYSTART_IN_CSS;
			cubic_log_hystart_event(ccv, cubic_data, 9, CCV(ccv, snd_ssthresh));
		}
		if (!IN_CONGRECOVERY(CCV(ccv, t_flags))) {
			cubic_ssthresh_update(ccv, mss);
			cubic_data->flags |= CUBICFLAG_CONG_EVENT;
			cubic_data->t_epoch = ticks;
			cubic_data->K = cubic_k(cubic_data->W_max / mss);
			CCV(ccv, snd_cwnd) = CCV(ccv, snd_ssthresh);
			ENTER_CONGRECOVERY(CCV(ccv, t_flags));
		}
		break;

	case CC_RTO:
		/* RFC8312 Section 4.7 */
		if (CCV(ccv, t_rxtshift) == 1) {
			/*
			 * Remember the state only for the first RTO event. This
			 * will help us restore the state to the values seen
			 * at the most recent congestion avoidance stage before
			 * the current RTO event.
			 */
			cubic_data->undo_t_epoch = cubic_data->t_epoch;
			cubic_data->undo_cwnd_epoch = cubic_data->cwnd_epoch;
			cubic_data->undo_W_est = cubic_data->W_est;
			cubic_data->undo_cwnd_prior = cubic_data->cwnd_prior;
			cubic_data->undo_W_max = cubic_data->W_max;
			cubic_data->undo_K = cubic_data->K;
			if (V_tcp_do_newsack) {
				pipe = tcp_compute_pipe(ccv->ccvc.tcp);
			} else {
				pipe = CCV(ccv, snd_max) -
					CCV(ccv, snd_fack) +
					CCV(ccv, sackhint.sack_bytes_rexmit);
			}
			CCV(ccv, snd_ssthresh) = max(2,
				(((uint64_t)min(CCV(ccv, snd_wnd), pipe) *
				CUBIC_BETA) >> CUBIC_SHIFT) / mss) * mss;
		}
		cubic_data->flags |= CUBICFLAG_CONG_EVENT | CUBICFLAG_RTO_EVENT;
		cubic_data->undo_W_max = cubic_data->W_max;
		cubic_data->num_cong_events++;
		CCV(ccv, snd_cwnd) = mss;
		break;

	case CC_RTO_ERR:
		cubic_data->flags &= ~(CUBICFLAG_CONG_EVENT | CUBICFLAG_RTO_EVENT);
		cubic_data->num_cong_events--;
		cubic_data->K = cubic_data->undo_K;
		cubic_data->cwnd_prior = cubic_data->undo_cwnd_prior;
		cubic_data->W_max = cubic_data->undo_W_max;
		cubic_data->W_est = cubic_data->undo_W_est;
		cubic_data->cwnd_epoch = cubic_data->undo_cwnd_epoch;
		cubic_data->t_epoch = cubic_data->undo_t_epoch;
		break;
	}
}

static void
cubic_conn_init(struct cc_var *ccv)
{
	struct cubic *cubic_data;

	cubic_data = ccv->cc_data;

	/*
	 * Ensure we have a sane initial value for W_max recorded. Without
	 * this here bad things happen when entries from the TCP hostcache
	 * get used.
	 */
	cubic_data->W_max = CCV(ccv, snd_cwnd);
}

static int
cubic_mod_init(void)
{
	return (0);
}

/*
 * Perform any necessary tasks before we exit congestion recovery.
 */
static void
cubic_post_recovery(struct cc_var *ccv)
{
	struct cubic *cubic_data;
	int pipe;

	cubic_data = ccv->cc_data;
	pipe = 0;

	if (IN_FASTRECOVERY(CCV(ccv, t_flags))) {
		/*
		 * If inflight data is less than ssthresh, set cwnd
		 * conservatively to avoid a burst of data, as suggested in
		 * the NewReno RFC. Otherwise, use the CUBIC method.
		 *
		 * XXXLAS: Find a way to do this without needing curack
		 */
		if (V_tcp_do_newsack)
			pipe = tcp_compute_pipe(ccv->ccvc.tcp);
		else
			pipe = CCV(ccv, snd_max) - ccv->curack;

		if (pipe < CCV(ccv, snd_ssthresh))
			/*
			 * Ensure that cwnd does not collapse to 1 MSS under
			 * adverse conditions. Implements RFC6582
			 */
			CCV(ccv, snd_cwnd) = max(pipe, CCV(ccv, t_maxseg)) +
			    CCV(ccv, t_maxseg);
		else
			/* Update cwnd based on beta and adjusted W_max. */
			CCV(ccv, snd_cwnd) = max(((uint64_t)cubic_data->W_max *
			    CUBIC_BETA) >> CUBIC_SHIFT,
			    2 * CCV(ccv, t_maxseg));
	}

	/* Calculate the average RTT between congestion epochs. */
	if (cubic_data->epoch_ack_count > 0 &&
	    cubic_data->sum_rtt_usecs >= cubic_data->epoch_ack_count) {
		cubic_data->mean_rtt_usecs = (int)(cubic_data->sum_rtt_usecs /
		    cubic_data->epoch_ack_count);
	}

	cubic_data->epoch_ack_count = 0;
	cubic_data->sum_rtt_usecs = 0;
}

/*
 * Record the min RTT and sum samples for the epoch average RTT calculation.
 */
static void
cubic_record_rtt(struct cc_var *ccv)
{
	struct cubic *cubic_data;
	uint32_t t_srtt_usecs;

	/* Ignore srtt until a min number of samples have been taken. */
	if (CCV(ccv, t_rttupdated) >= CUBIC_MIN_RTT_SAMPLES) {
		cubic_data = ccv->cc_data;
		t_srtt_usecs = tcp_get_srtt(ccv->ccvc.tcp,
					    TCP_TMR_GRANULARITY_USEC);
		/*
		 * Record the current SRTT as our minrtt if it's the smallest
		 * we've seen or minrtt is currently equal to its initialised
		 * value.
		 *
		 * XXXLAS: Should there be some hysteresis for minrtt?
		 */
		if ((t_srtt_usecs < cubic_data->min_rtt_usecs ||
		    cubic_data->min_rtt_usecs == TCPTV_SRTTBASE)) {
			/* A minimal rtt is a single unshifted tick of a ticks
			 * timer. */
			cubic_data->min_rtt_usecs = max(tick >> TCP_RTT_SHIFT,
							t_srtt_usecs);

			/*
			 * If the connection is within its first congestion
			 * epoch, ensure we prime mean_rtt_usecs with a
			 * reasonable value until the epoch average RTT is
			 * calculated in cubic_post_recovery().
			 */
			if (cubic_data->min_rtt_usecs >
			    cubic_data->mean_rtt_usecs)
				cubic_data->mean_rtt_usecs =
				    cubic_data->min_rtt_usecs;
		}

		/* Sum samples for epoch average RTT calculation. */
		cubic_data->sum_rtt_usecs += t_srtt_usecs;
		cubic_data->epoch_ack_count++;
	}
}

/*
 * Update the ssthresh in the event of congestion.
 */
static void
cubic_ssthresh_update(struct cc_var *ccv, uint32_t maxseg)
{
	struct cubic *cubic_data;
	uint32_t ssthresh;
	uint32_t cwnd;

	cubic_data = ccv->cc_data;
	cwnd = CCV(ccv, snd_cwnd);

	/* Fast convergence heuristic. */
	if (cwnd < cubic_data->W_max) {
		cwnd = ((uint64_t)cwnd * CUBIC_FC_FACTOR) >> CUBIC_SHIFT;
	}
	cubic_data->undo_W_max = cubic_data->W_max;
	cubic_data->W_max = cwnd;

	/*
	 * On the first congestion event, set ssthresh to cwnd * 0.5
	 * and reduce W_max to cwnd * beta. This aligns the cubic concave
	 * region appropriately. On subsequent congestion events, set
	 * ssthresh to cwnd * beta.
	 */
	if ((cubic_data->flags & CUBICFLAG_CONG_EVENT) == 0) {
		ssthresh = cwnd >> 1;
		cubic_data->W_max = ((uint64_t)cwnd *
		    CUBIC_BETA) >> CUBIC_SHIFT;
	} else {
		ssthresh = ((uint64_t)cwnd *
		    CUBIC_BETA) >> CUBIC_SHIFT;
	}
	CCV(ccv, snd_ssthresh) = max(ssthresh, 2 * maxseg);
}

static void
cubic_rttsample(struct cc_var *ccv, uint32_t usec_rtt, uint32_t rxtcnt, uint32_t fas)
{
	struct cubic *cubicd;

	cubicd = ccv->cc_data;
	if (rxtcnt > 1) {
		/*
		 * Only look at RTT's that are non-ambiguous.
		 */
		return;
	}
	cubicd->css_rttsample_count++;
	cubicd->css_last_fas = fas;
	if (cubicd->css_current_round_minrtt > usec_rtt) {
		cubicd->css_current_round_minrtt = usec_rtt;
		cubicd->css_lowrtt_fas = cubicd->css_last_fas;
	}
	if ((cubicd->css_rttsample_count >= hystart_n_rttsamples) &&
	    (cubicd->css_current_round_minrtt != 0xffffffff) &&
	    (cubicd->css_current_round_minrtt < cubicd->css_baseline_minrtt) &&
	    (cubicd->css_lastround_minrtt != 0xffffffff)) {
		/*
		 * We were in CSS and the RTT is now less, we
		 * entered CSS erroneously.
		 */
		cubicd->flags &= ~CUBICFLAG_HYSTART_IN_CSS;
		cubic_log_hystart_event(ccv, cubicd, 8, cubicd->css_baseline_minrtt);
		cubicd->css_baseline_minrtt = 0xffffffff;
	}
	if (cubicd->flags & CUBICFLAG_HYSTART_ENABLED)
		cubic_log_hystart_event(ccv, cubicd, 5, usec_rtt);
}

static void
cubic_newround(struct cc_var *ccv, uint32_t round_cnt)
{
	struct cubic *cubicd;

	cubicd = ccv->cc_data;
	/* We have entered a new round */
	cubicd->css_lastround_minrtt = cubicd->css_current_round_minrtt;
	cubicd->css_current_round_minrtt = 0xffffffff;
	cubicd->css_rttsample_count = 0;
	cubicd->css_current_round = round_cnt;
	if ((cubicd->flags & CUBICFLAG_HYSTART_IN_CSS) &&
	    ((round_cnt - cubicd->css_entered_at_round) >= hystart_css_rounds)) {
		/* Enter CA */
		if (ccv->flags & CCF_HYSTART_CAN_SH_CWND) {
			/*
			 * We engage more than snd_ssthresh, engage
			 * the brakes!! Though we will stay in SS to
			 * creep back up again, so lets leave CSS active
			 * and give us hystart_css_rounds more rounds.
			 */
			if (ccv->flags & CCF_HYSTART_CONS_SSTH) {
				CCV(ccv, snd_ssthresh) = ((cubicd->css_lowrtt_fas + cubicd->css_fas_at_css_entry) / 2);
			} else {
				CCV(ccv, snd_ssthresh) = cubicd->css_lowrtt_fas;
			}
			CCV(ccv, snd_cwnd) = cubicd->css_fas_at_css_entry;
			cubicd->css_entered_at_round = round_cnt;
		} else {
			CCV(ccv, snd_ssthresh) = CCV(ccv, snd_cwnd);
			/* Turn off the CSS flag */
			cubicd->flags &= ~CUBICFLAG_HYSTART_IN_CSS;
			/* Disable use of CSS in the future except long idle  */
			cubicd->flags &= ~CUBICFLAG_HYSTART_ENABLED;
		}
		cubic_log_hystart_event(ccv, cubicd, 6, CCV(ccv, snd_ssthresh));
	}
	if (cubicd->flags & CUBICFLAG_HYSTART_ENABLED)
		cubic_log_hystart_event(ccv, cubicd, 4, round_cnt);
}

DECLARE_CC_MODULE(cubic, &cubic_cc_algo);
MODULE_VERSION(cubic, 2);
