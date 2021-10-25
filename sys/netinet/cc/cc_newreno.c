/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2007-2008,2010,2014
 *	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2009-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Lawrence Stewart, James
 * Healy and David Hayes, made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
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
 * This software was first released in 2007 by James Healy and Lawrence Stewart
 * whilst working on the NewTCP research project at Swinburne University of
 * Technology's Centre for Advanced Internet Architectures, Melbourne,
 * Australia, which was made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
 * More details are available at:
 *   http://caia.swin.edu.au/urp/newtcp/
 *
 * Dec 2014 garmitage@swin.edu.au
 * Borrowed code fragments from cc_cdg.c to add modifiable beta
 * via sysctls.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_hpts.h>
#include <netinet/cc/cc.h>
#include <netinet/cc/cc_module.h>
#include <netinet/cc/cc_newreno.h>

static MALLOC_DEFINE(M_NEWRENO, "newreno data",
	"newreno beta values");

static void	newreno_cb_destroy(struct cc_var *ccv);
static void	newreno_ack_received(struct cc_var *ccv, uint16_t type);
static void	newreno_after_idle(struct cc_var *ccv);
static void	newreno_cong_signal(struct cc_var *ccv, uint32_t type);
static void	newreno_post_recovery(struct cc_var *ccv);
static int newreno_ctl_output(struct cc_var *ccv, struct sockopt *sopt, void *buf);
static void	newreno_newround(struct cc_var *ccv, uint32_t round_cnt);
static void	newreno_rttsample(struct cc_var *ccv, uint32_t usec_rtt, uint32_t rxtcnt, uint32_t fas);
static 	int	newreno_cb_init(struct cc_var *ccv);

VNET_DEFINE(uint32_t, newreno_beta) = 50;
VNET_DEFINE(uint32_t, newreno_beta_ecn) = 80;
#define V_newreno_beta VNET(newreno_beta)
#define V_newreno_beta_ecn VNET(newreno_beta_ecn)

struct cc_algo newreno_cc_algo = {
	.name = "newreno",
	.cb_destroy = newreno_cb_destroy,
	.ack_received = newreno_ack_received,
	.after_idle = newreno_after_idle,
	.cong_signal = newreno_cong_signal,
	.post_recovery = newreno_post_recovery,
	.ctl_output = newreno_ctl_output,
	.newround = newreno_newround,
	.rttsample = newreno_rttsample,
	.cb_init = newreno_cb_init,
};

static uint32_t hystart_lowcwnd = 16;
static uint32_t hystart_minrtt_thresh = 4000;
static uint32_t hystart_maxrtt_thresh = 16000;
static uint32_t hystart_n_rttsamples = 8;
static uint32_t hystart_css_growth_div = 4;
static uint32_t hystart_css_rounds = 5;
static uint32_t hystart_bblogs = 0;

static void
newreno_log_hystart_event(struct cc_var *ccv, struct newreno *nreno, uint8_t mod, uint32_t flex1)
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
	 */
	struct tcpcb *tp;

	if (hystart_bblogs == 0)
		return;
	tp = ccv->ccvc.tcp;
	if (tp->t_logstate != TCP_LOG_STATE_OFF) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = flex1;
		log.u_bbr.flex2 = nreno->css_current_round_minrtt;
		log.u_bbr.flex3 = nreno->css_lastround_minrtt;
		log.u_bbr.flex4 = nreno->css_rttsample_count;
		log.u_bbr.flex5 = nreno->css_entered_at_round;
		log.u_bbr.flex6 = nreno->css_baseline_minrtt;
		/* We only need bottom 16 bits of flags */
		log.u_bbr.flex7 = nreno->newreno_flags & 0x0000ffff;
		log.u_bbr.flex8 = mod;
		log.u_bbr.epoch = nreno->css_current_round;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.lt_epoch = nreno->css_fas_at_css_entry;
		log.u_bbr.pkts_out = nreno->css_last_fas;
		log.u_bbr.delivered = nreno->css_lowrtt_fas;
		TCP_LOG_EVENTP(tp, NULL,
		    &tp->t_inpcb->inp_socket->so_rcv,
		    &tp->t_inpcb->inp_socket->so_snd,
		    TCP_HYSTART, 0,
		    0, &log, false, &tv);
	}
}

static int
newreno_cb_init(struct cc_var *ccv)
{
	struct newreno *nreno;

	ccv->cc_data = malloc(sizeof(struct newreno), M_NEWRENO, M_NOWAIT);
	if (ccv->cc_data == NULL)
		return (ENOMEM);
	nreno = (struct newreno *)ccv->cc_data;
	/* NB: nreno is not zeroed, so initialise all fields. */
	nreno->beta = V_newreno_beta;
	nreno->beta_ecn = V_newreno_beta_ecn;
	/*
	 * We set the enabled flag so that if
	 * the socket option gets strobed and
	 * we have not hit a loss
	 */
	nreno->newreno_flags = CC_NEWRENO_HYSTART_ENABLED;
	/* At init set both to infinity */
	nreno->css_lastround_minrtt = 0xffffffff;
	nreno->css_current_round_minrtt = 0xffffffff;
	nreno->css_current_round = 0;
	nreno->css_baseline_minrtt = 0xffffffff;
	nreno->css_rttsample_count = 0;
	nreno->css_entered_at_round = 0;
	nreno->css_fas_at_css_entry = 0;
	nreno->css_lowrtt_fas = 0;
	nreno->css_last_fas = 0;
	return (0);
}

static void
newreno_cb_destroy(struct cc_var *ccv)
{
	free(ccv->cc_data, M_NEWRENO);
}

static void
newreno_ack_received(struct cc_var *ccv, uint16_t type)
{
	struct newreno *nreno;

	/*
	 * Other TCP congestion controls use newreno_ack_received(), but
	 * with their own private cc_data. Make sure the cc_data is used
	 * correctly.
	 */
	nreno = (CC_ALGO(ccv->ccvc.tcp) == &newreno_cc_algo) ? ccv->cc_data : NULL;

	if (type == CC_ACK && !IN_RECOVERY(CCV(ccv, t_flags)) &&
	    (ccv->flags & CCF_CWND_LIMITED)) {
		u_int cw = CCV(ccv, snd_cwnd);
		u_int incr = CCV(ccv, t_maxseg);

		/*
		 * Regular in-order ACK, open the congestion window.
		 * Method depends on which congestion control state we're
		 * in (slow start or cong avoid) and if ABC (RFC 3465) is
		 * enabled.
		 *
		 * slow start: cwnd <= ssthresh
		 * cong avoid: cwnd > ssthresh
		 *
		 * slow start and ABC (RFC 3465):
		 *   Grow cwnd exponentially by the amount of data
		 *   ACKed capping the max increment per ACK to
		 *   (abc_l_var * maxseg) bytes.
		 *
		 * slow start without ABC (RFC 5681):
		 *   Grow cwnd exponentially by maxseg per ACK.
		 *
		 * cong avoid and ABC (RFC 3465):
		 *   Grow cwnd linearly by maxseg per RTT for each
		 *   cwnd worth of ACKed data.
		 *
		 * cong avoid without ABC (RFC 5681):
		 *   Grow cwnd linearly by approximately maxseg per RTT using
		 *   maxseg^2 / cwnd per ACK as the increment.
		 *   If cwnd > maxseg^2, fix the cwnd increment at 1 byte to
		 *   avoid capping cwnd.
		 */
		if (cw > CCV(ccv, snd_ssthresh)) {
			if ((nreno != NULL) &&
			    (nreno->newreno_flags & CC_NEWRENO_HYSTART_IN_CSS)) {
				/*
				 * We have slipped into CA with
				 * CSS active. Deactivate all.
				 */
				/* Turn off the CSS flag */
				nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_IN_CSS;
				/* Disable use of CSS in the future except long idle  */
				nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_ENABLED;
			}
			if (V_tcp_do_rfc3465) {
				if (ccv->flags & CCF_ABC_SENTAWND)
					ccv->flags &= ~CCF_ABC_SENTAWND;
				else
					incr = 0;
			} else
				incr = max((incr * incr / cw), 1);
		} else if (V_tcp_do_rfc3465) {
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
			uint16_t abc_val;

			if (ccv->flags & CCF_USE_LOCAL_ABC)
				abc_val = ccv->labc;
			else
				abc_val = V_tcp_abc_l_var;
			if ((nreno != NULL) &&
			    (nreno->newreno_flags & CC_NEWRENO_HYSTART_ALLOWED) &&
			    (nreno->newreno_flags & CC_NEWRENO_HYSTART_ENABLED) &&
			    ((nreno->newreno_flags & CC_NEWRENO_HYSTART_IN_CSS) == 0)) {
				/*
				 * Hystart is allowed and still enabled and we are not yet
				 * in CSS. Lets check to see if we can make a decision on
				 * if we need to go into CSS.
				 */
				if ((nreno->css_rttsample_count >= hystart_n_rttsamples) &&
				    (CCV(ccv, snd_cwnd) >
				     (hystart_lowcwnd * tcp_fixed_maxseg(ccv->ccvc.tcp)))) {
					uint32_t rtt_thresh;

					/* Clamp (minrtt_thresh, lastround/8, maxrtt_thresh) */
					rtt_thresh = (nreno->css_lastround_minrtt >> 3);
					if (rtt_thresh < hystart_minrtt_thresh)
						rtt_thresh = hystart_minrtt_thresh;
					if (rtt_thresh > hystart_maxrtt_thresh)
						rtt_thresh = hystart_maxrtt_thresh;
					newreno_log_hystart_event(ccv, nreno, 1, rtt_thresh);
					if (nreno->css_current_round_minrtt >= (nreno->css_lastround_minrtt + rtt_thresh)) {
						/* Enter CSS */
						nreno->newreno_flags |= CC_NEWRENO_HYSTART_IN_CSS;
						nreno->css_fas_at_css_entry = nreno->css_lowrtt_fas;
						nreno->css_baseline_minrtt = nreno->css_current_round_minrtt;
						nreno->css_entered_at_round = nreno->css_current_round;
						newreno_log_hystart_event(ccv, nreno, 2, rtt_thresh);
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
			if ((nreno != NULL) &&
			    (nreno->newreno_flags & CC_NEWRENO_HYSTART_IN_CSS)) {
				incr /= hystart_css_growth_div;
				newreno_log_hystart_event(ccv, nreno, 3, incr);
			}
		}
		/* ABC is on by default, so incr equals 0 frequently. */
		if (incr > 0)
			CCV(ccv, snd_cwnd) = min(cw + incr,
			    TCP_MAXWIN << CCV(ccv, snd_scale));
	}
}

static void
newreno_after_idle(struct cc_var *ccv)
{
	struct newreno *nreno;
	uint32_t rw;

	/*
	 * Other TCP congestion controls use newreno_after_idle(), but
	 * with their own private cc_data. Make sure the cc_data is used
	 * correctly.
	 */
	nreno = (CC_ALGO(ccv->ccvc.tcp) == &newreno_cc_algo) ? ccv->cc_data : NULL;
	/*
	 * If we've been idle for more than one retransmit timeout the old
	 * congestion window is no longer current and we have to reduce it to
	 * the restart window before we can transmit again.
	 *
	 * The restart window is the initial window or the last CWND, whichever
	 * is smaller.
	 *
	 * This is done to prevent us from flooding the path with a full CWND at
	 * wirespeed, overloading router and switch buffers along the way.
	 *
	 * See RFC5681 Section 4.1. "Restarting Idle Connections".
	 *
	 * In addition, per RFC2861 Section 2, the ssthresh is set to the
	 * maximum of the former ssthresh or 3/4 of the old cwnd, to
	 * not exit slow-start prematurely.
	 */
	rw = tcp_compute_initwnd(tcp_maxseg(ccv->ccvc.tcp));

	CCV(ccv, snd_ssthresh) = max(CCV(ccv, snd_ssthresh),
	    CCV(ccv, snd_cwnd)-(CCV(ccv, snd_cwnd)>>2));

	CCV(ccv, snd_cwnd) = min(rw, CCV(ccv, snd_cwnd));
	if ((nreno != NULL) &&
	    (nreno->newreno_flags & CC_NEWRENO_HYSTART_ENABLED) == 0) {
		if (CCV(ccv, snd_cwnd) <= (hystart_lowcwnd * tcp_fixed_maxseg(ccv->ccvc.tcp))) {
			/*
			 * Re-enable hystart if our cwnd has fallen below
			 * the hystart lowcwnd point.
			 */
			nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_IN_CSS;
			nreno->newreno_flags |= CC_NEWRENO_HYSTART_ENABLED;
		}
	}
}

/*
 * Perform any necessary tasks before we enter congestion recovery.
 */
static void
newreno_cong_signal(struct cc_var *ccv, uint32_t type)
{
	struct newreno *nreno;
	uint32_t beta, beta_ecn, cwin, factor;
	u_int mss;

	cwin = CCV(ccv, snd_cwnd);
	mss = tcp_fixed_maxseg(ccv->ccvc.tcp);
	/*
	 * Other TCP congestion controls use newreno_cong_signal(), but
	 * with their own private cc_data. Make sure the cc_data is used
	 * correctly.
	 */
	nreno = (CC_ALGO(ccv->ccvc.tcp) == &newreno_cc_algo) ? ccv->cc_data : NULL;
	beta = (nreno == NULL) ? V_newreno_beta : nreno->beta;;
	beta_ecn = (nreno == NULL) ? V_newreno_beta_ecn : nreno->beta_ecn;
	/*
	 * Note that we only change the backoff for ECN if the
	 * global sysctl V_cc_do_abe is set <or> the stack itself
	 * has set a flag in our newreno_flags (due to pacing) telling
	 * us to use the lower valued back-off.
	 */
	if ((type == CC_ECN) &&
	    (V_cc_do_abe ||
	    ((nreno != NULL) && (nreno->newreno_flags & CC_NEWRENO_BETA_ECN_ENABLED))))
		factor = beta_ecn;
	else
		factor = beta;

	/* Catch algos which mistakenly leak private signal types. */
	KASSERT((type & CC_SIGPRIVMASK) == 0,
	    ("%s: congestion signal type 0x%08x is private\n", __func__, type));

	cwin = max(((uint64_t)cwin * (uint64_t)factor) / (100ULL * (uint64_t)mss),
	    2) * mss;

	switch (type) {
	case CC_NDUPACK:
		if ((nreno != NULL) &&
		    (nreno->newreno_flags & CC_NEWRENO_HYSTART_ENABLED)) {
			/* Make sure the flags are all off we had a loss */
			nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_ENABLED;
			nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_IN_CSS;
		}
		if (!IN_FASTRECOVERY(CCV(ccv, t_flags))) {
			if (IN_CONGRECOVERY(CCV(ccv, t_flags) &&
			    V_cc_do_abe && V_cc_abe_frlossreduce)) {
				CCV(ccv, snd_ssthresh) =
				    ((uint64_t)CCV(ccv, snd_ssthresh) *
				     (uint64_t)beta) / (uint64_t)beta_ecn;
			}
			if (!IN_CONGRECOVERY(CCV(ccv, t_flags)))
				CCV(ccv, snd_ssthresh) = cwin;
			ENTER_RECOVERY(CCV(ccv, t_flags));
		}
		break;
	case CC_ECN:
		if ((nreno != NULL) &&
		    (nreno->newreno_flags & CC_NEWRENO_HYSTART_ENABLED)) {
			/* Make sure the flags are all off we had a loss */
			nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_ENABLED;
			nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_IN_CSS;
		}
		if (!IN_CONGRECOVERY(CCV(ccv, t_flags))) {
			CCV(ccv, snd_ssthresh) = cwin;
			CCV(ccv, snd_cwnd) = cwin;
			ENTER_CONGRECOVERY(CCV(ccv, t_flags));
		}
		break;
	case CC_RTO:
		CCV(ccv, snd_ssthresh) = max(min(CCV(ccv, snd_wnd),
						 CCV(ccv, snd_cwnd)) / 2 / mss,
					     2) * mss;
		CCV(ccv, snd_cwnd) = mss;
		break;
	}
}

/*
 * Perform any necessary tasks before we exit congestion recovery.
 */
static void
newreno_post_recovery(struct cc_var *ccv)
{
	int pipe;

	if (IN_FASTRECOVERY(CCV(ccv, t_flags))) {
		/*
		 * Fast recovery will conclude after returning from this
		 * function. Window inflation should have left us with
		 * approximately snd_ssthresh outstanding data. But in case we
		 * would be inclined to send a burst, better to do it via the
		 * slow start mechanism.
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
			 * adverse conditons. Implements RFC6582
			 */
			CCV(ccv, snd_cwnd) = max(pipe, CCV(ccv, t_maxseg)) +
			    CCV(ccv, t_maxseg);
		else
			CCV(ccv, snd_cwnd) = CCV(ccv, snd_ssthresh);
	}
}

static int
newreno_ctl_output(struct cc_var *ccv, struct sockopt *sopt, void *buf)
{
	struct newreno *nreno;
	struct cc_newreno_opts *opt;

	if (sopt->sopt_valsize != sizeof(struct cc_newreno_opts))
		return (EMSGSIZE);

	if (CC_ALGO(ccv->ccvc.tcp) != &newreno_cc_algo)
		return (ENOPROTOOPT);

	nreno = (struct newreno *)ccv->cc_data;
	opt = buf;
	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (opt->name) {
		case CC_NEWRENO_BETA:
			nreno->beta = opt->val;
			break;
		case CC_NEWRENO_BETA_ECN:
			if ((!V_cc_do_abe) && ((nreno->newreno_flags & CC_NEWRENO_BETA_ECN) == 0))
				return (EACCES);
			nreno->beta_ecn = opt->val;
			nreno->newreno_flags |= CC_NEWRENO_BETA_ECN_ENABLED;
			break;
		case CC_NEWRENO_ENABLE_HYSTART:
			/* Allow hystart on this connection */
			if (opt->val != 0) {
				nreno->newreno_flags |= CC_NEWRENO_HYSTART_ALLOWED;
				if (opt->val > 1)
					nreno->newreno_flags |= CC_NEWRENO_HYSTART_CAN_SH_CWND;
				if (opt->val > 2)
					nreno->newreno_flags |= CC_NEWRENO_HYSTART_CONS_SSTH;
			} else
				nreno->newreno_flags &= ~(CC_NEWRENO_HYSTART_ALLOWED|CC_NEWRENO_HYSTART_CAN_SH_CWND|CC_NEWRENO_HYSTART_CONS_SSTH);
			newreno_log_hystart_event(ccv, nreno, 7, opt->val);
			break;
		default:
			return (ENOPROTOOPT);
		}
		break;
	case SOPT_GET:
		switch (opt->name) {
		case CC_NEWRENO_BETA:
			opt->val = (nreno == NULL) ?
			    V_newreno_beta : nreno->beta;
			break;
		case CC_NEWRENO_BETA_ECN:
			opt->val = (nreno == NULL) ?
			    V_newreno_beta_ecn : nreno->beta_ecn;
			break;
		case CC_NEWRENO_ENABLE_HYSTART:
			if (nreno->newreno_flags & CC_NEWRENO_HYSTART_ALLOWED) {
				if (nreno->newreno_flags & CC_NEWRENO_HYSTART_CONS_SSTH)
					opt->val = 3;
				else if (nreno->newreno_flags & CC_NEWRENO_HYSTART_CAN_SH_CWND)
					opt->val = 2;
				else
					opt->val = 1;
			} else
				opt->val = 0;
			break;
		default:
			return (ENOPROTOOPT);
		}
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
newreno_beta_handler(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t new;

	new = *(uint32_t *)arg1;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr != NULL ) {
		if (arg1 == &VNET_NAME(newreno_beta_ecn) && !V_cc_do_abe)
			error = EACCES;
		else if (new == 0 || new > 100)
			error = EINVAL;
		else
			*(uint32_t *)arg1 = new;
	}

	return (error);
}

static void
newreno_newround(struct cc_var *ccv, uint32_t round_cnt)
{
	struct newreno *nreno;

	nreno = (struct newreno *)ccv->cc_data;
	/* We have entered a new round */
	nreno->css_lastround_minrtt = nreno->css_current_round_minrtt;
	nreno->css_current_round_minrtt = 0xffffffff;
	nreno->css_rttsample_count = 0;
	nreno->css_current_round = round_cnt;
	if ((nreno->newreno_flags & CC_NEWRENO_HYSTART_IN_CSS) &&
	    ((round_cnt - nreno->css_entered_at_round) >= hystart_css_rounds)) {
		/* Enter CA */
		if (nreno->newreno_flags & CC_NEWRENO_HYSTART_CAN_SH_CWND) {
			/*
			 * We engage more than snd_ssthresh, engage
			 * the brakes!! Though we will stay in SS to
			 * creep back up again, so lets leave CSS active
			 * and give us hystart_css_rounds more rounds.
			 */
			if (nreno->newreno_flags & CC_NEWRENO_HYSTART_CONS_SSTH) {
				CCV(ccv, snd_ssthresh) = ((nreno->css_lowrtt_fas + nreno->css_fas_at_css_entry) / 2);
			} else {
				CCV(ccv, snd_ssthresh) = nreno->css_lowrtt_fas;
			}
			CCV(ccv, snd_cwnd) = nreno->css_fas_at_css_entry;
			nreno->css_entered_at_round = round_cnt;
		} else {
			CCV(ccv, snd_ssthresh) = CCV(ccv, snd_cwnd);
			/* Turn off the CSS flag */
			nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_IN_CSS;
			/* Disable use of CSS in the future except long idle  */
			nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_ENABLED;
		}
		newreno_log_hystart_event(ccv, nreno, 6, CCV(ccv, snd_ssthresh));
	}
	newreno_log_hystart_event(ccv, nreno, 4, round_cnt);
}

static void
newreno_rttsample(struct cc_var *ccv, uint32_t usec_rtt, uint32_t rxtcnt, uint32_t fas)
{
	struct newreno *nreno;

	nreno = (struct newreno *)ccv->cc_data;
	if (rxtcnt > 1) {
		/*
		 * Only look at RTT's that are non-ambiguous.
		 */
		return;
	}
	nreno->css_rttsample_count++;
	nreno->css_last_fas = fas;
	if (nreno->css_current_round_minrtt > usec_rtt) {
		nreno->css_current_round_minrtt = usec_rtt;
		nreno->css_lowrtt_fas = nreno->css_last_fas;
	}
	if ((nreno->newreno_flags & CC_NEWRENO_HYSTART_IN_CSS) &&
	    (nreno->css_rttsample_count >= hystart_n_rttsamples) &&
	    (nreno->css_baseline_minrtt > nreno->css_current_round_minrtt)) {
		/*
		 * We were in CSS and the RTT is now less, we
		 * entered CSS erroneously.
		 */
		nreno->newreno_flags &= ~CC_NEWRENO_HYSTART_IN_CSS;
		newreno_log_hystart_event(ccv, nreno, 8, nreno->css_baseline_minrtt);
		nreno->css_baseline_minrtt = 0xffffffff;
	}
	newreno_log_hystart_event(ccv, nreno, 5, usec_rtt);
}

SYSCTL_DECL(_net_inet_tcp_cc_newreno);
SYSCTL_NODE(_net_inet_tcp_cc, OID_AUTO, newreno,
    CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "New Reno related settings");

SYSCTL_PROC(_net_inet_tcp_cc_newreno, OID_AUTO, beta,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(newreno_beta), 3, &newreno_beta_handler, "IU",
    "New Reno beta, specified as number between 1 and 100");

SYSCTL_PROC(_net_inet_tcp_cc_newreno, OID_AUTO, beta_ecn,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(newreno_beta_ecn), 3, &newreno_beta_handler, "IU",
    "New Reno beta ecn, specified as number between 1 and 100");

SYSCTL_NODE(_net_inet_tcp_cc_newreno, OID_AUTO, hystartplusplus,
    CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "New Reno related HyStart++ settings");

SYSCTL_UINT(_net_inet_tcp_cc_newreno_hystartplusplus, OID_AUTO, lowcwnd,
    CTLFLAG_RW,
    &hystart_lowcwnd, 16,
   "The number of MSS in the CWND before HyStart++ is active");

SYSCTL_UINT(_net_inet_tcp_cc_newreno_hystartplusplus, OID_AUTO, minrtt_thresh,
    CTLFLAG_RW,
    &hystart_minrtt_thresh, 4000,
   "HyStarts++ minimum RTT thresh used in clamp (in microseconds)");

SYSCTL_UINT(_net_inet_tcp_cc_newreno_hystartplusplus, OID_AUTO, maxrtt_thresh,
    CTLFLAG_RW,
    &hystart_maxrtt_thresh, 16000,
   "HyStarts++ maximum RTT thresh used in clamp (in microseconds)");

SYSCTL_UINT(_net_inet_tcp_cc_newreno_hystartplusplus, OID_AUTO, n_rttsamples,
    CTLFLAG_RW,
    &hystart_n_rttsamples, 8,
   "The number of RTT samples that must be seen to consider HyStart++");

SYSCTL_UINT(_net_inet_tcp_cc_newreno_hystartplusplus, OID_AUTO, css_growth_div,
    CTLFLAG_RW,
    &hystart_css_growth_div, 4,
   "The divisor to the growth when in Hystart++ CSS");

SYSCTL_UINT(_net_inet_tcp_cc_newreno_hystartplusplus, OID_AUTO, css_rounds,
    CTLFLAG_RW,
    &hystart_css_rounds, 5,
   "The number of rounds HyStart++ lasts in CSS before falling to CA");

SYSCTL_UINT(_net_inet_tcp_cc_newreno_hystartplusplus, OID_AUTO, bblogs,
    CTLFLAG_RW,
    &hystart_bblogs, 0,
   "Do we enable HyStart++ Black Box logs to be generated if BB logging is on");


DECLARE_CC_MODULE(newreno, &newreno_cc_algo);
MODULE_VERSION(newreno, 1);
