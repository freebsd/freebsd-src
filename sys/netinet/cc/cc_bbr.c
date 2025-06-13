/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Google LLC and the University of Cambridge
 * Computer Laboratory under CORIGAP R&D contract no. HR0011-17-C-0020,
 * the University of Cambridge Computer Laboratory under EPSRC INTERNET Project
 * EP/H040536/1 and EP/K019563/1 and the University of Cambridge Computer
 * Laboratory under the EPSRC Impact Acceleration Grant EP/K502760/1.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/cc/cc.h>
#include <errno.h> // Required for ENOTSUP, ENOMEM
#include <string.h> // Required for memset
#include <sys/param.h> // For MIN/MAX if used, or other sys macros

#include <sys/random.h> // For arc4random_uniform

#include <sys/random.h> // For arc4random_uniform

#define BBR_UNIT 1024 // For fixed-point arithmetic with gains
#define BBR_HIGH_GAIN 2959 // Approx 2.89 * 1024 (2/ln(2) * 1024)
#define BBR_BTLBW_FILTER_LEN 10
/* BBR_MIN_PIPE_CWND will be (2 * tp->t_maxseg) used directly in code */

static const int bbr_pacing_gain_cycle[] = {
	BBR_UNIT * 5 / 4,  /* Probe up */
	BBR_UNIT * 3 / 4,  /* Probe down */
	BBR_UNIT, BBR_UNIT, BBR_UNIT, BBR_UNIT, BBR_UNIT, BBR_UNIT /* Cruise */
};
#define BBR_CYCLE_LEN (sizeof(bbr_pacing_gain_cycle) / sizeof(bbr_pacing_gain_cycle[0]))

// BBR internal states
enum bbr_mode {
	BBR_STARTUP,
	BBR_DRAIN,
	BBR_PROBE_BW,
	BBR_PROBE_RTT
};

struct bbr_state {
	uint64_t bbr_BtlBw;
	uint32_t bbr_rtprop;
	uint32_t bbr_pacing_gain;
	uint32_t bbr_cwnd_gain;
	uint8_t bbr_mode; // Current BBR state (Startup, Drain, ProbeBW, ProbeRTT)
	uint32_t bbr_round_cnt; // Count of packet-timed rounds
	tcp_seq bbr_next_round_delivered; // snd_nxt when the current round ends
	uint64_t bbr_pacing_rate; // Current pacing rate in bytes per second
	uint32_t bbr_rtt_cnt; // Counter for ProbeRTT duration
	struct tcpcb *bbr_tp; // Pointer to the tcpcb

	/* Startup phase state */
	bool bbr_filled_pipe; // True if BBR estimates the pipe is filled
	uint32_t bbr_full_bw_cnt; // Number of rounds with no significant BtlBw increase
	uint64_t bbr_full_bw; // Value of BtlBw at the last time it significantly increased

	/* PROBE_BW phase state */
	uint8_t bbr_cycle_idx; // Current index in the PROBE_BW gain cycle array
	uint64_t bbr_cycle_ustamp; // Timestamp of when the current PROBE_BW cycle phase started

	/* PROBE_RTT phase state */
	uint64_t bbr_rtprop_stamp; // Timestamp of the last RTprop update or PROBE_RTT exit
	uint64_t bbr_probe_rtt_done_stamp; // Timestamp when PROBE_RTT mode can exit
	bool bbr_probe_rtt_round_done; // True if a round trip has completed since entering PROBE_RTT

	/* BtlBw filter */
	uint64_t bbr_btlbw_filter[BBR_BTLBW_FILTER_LEN];
	int bbr_btlbw_filter_idx;
};

static void bbr_enter_startup_mode(struct bbr_state *bb);
static bool bbr_check_startup_done(struct cc_var *ccv, struct bbr_state *bb);
static void bbr_enter_drain_mode(struct cc_var *ccv, struct bbr_state *bb);
static uint64_t bbr_bdp(struct bbr_state *bb, struct tcpcb *tp);
static void bbr_enter_probe_bw_mode(struct cc_var *ccv, struct bbr_state *bb);
static void bbr_check_drain(struct cc_var *ccv, struct bbr_state *bb);
static void bbr_advance_probe_bw_cycle_phase(struct cc_var *ccv, struct bbr_state *bb);
static void bbr_update_probe_bw_cycle_phase(struct cc_var *ccv, struct bbr_state *bb);
static void bbr_enter_probe_rtt_mode(struct cc_var *ccv, struct bbr_state *bb);
static void bbr_exit_probe_rtt_mode(struct cc_var *ccv, struct bbr_state *bb);
static void bbr_check_probe_rtt(struct cc_var *ccv, struct bbr_state *bb);
static uint64_t bbr_target_cwnd(struct bbr_state *bb, struct tcpcb *tp);


static size_t bbr_cc_data_sz(void);
static int bbr_mod_init(void);
static int bbr_mod_destroy(void);
static int bbr_cb_init(struct cc_var *ccv, void *ptr); // Changed return type
static void bbr_cb_destroy(struct cc_var *ccv);
static void bbr_conn_init(struct cc_var *ccv);
static void bbr_ack_received(struct cc_var *ccv, ccsignal_t type);
static void bbr_cong_signal(struct cc_var *ccv, ccsignal_t type);
static void bbr_post_recovery(struct cc_var *ccv);
static void bbr_after_idle(struct cc_var *ccv);
static void bbr_ecnpkt_handler(struct cc_var *ccv);
static void bbr_newround(struct cc_var *ccv, uint32_t round_cnt);
static void bbr_rttsample(struct cc_var *ccv, uint32_t rtt, uint32_t rxtcnt, uint32_t fas);
static int bbr_ctl_output(struct cc_var *ccv, struct sockopt *sopt, void *buf);

static struct cc_algo bbr_algo = {
	.name = "bbr",
	.cc_data_sz = bbr_cc_data_sz,
	.mod_init = bbr_mod_init,
	.mod_destroy = bbr_mod_destroy,
	.cb_init = (void (*)(struct cc_var *, void *))bbr_cb_init, // Cast to match expected type
	.cb_destroy = bbr_cb_destroy,
	.conn_init = bbr_conn_init,
	.ack_received = bbr_ack_received,
	.cong_signal = bbr_cong_signal,
	.post_recovery = bbr_post_recovery, // Will be implemented
	.after_idle = bbr_after_idle,       // Will be implemented
	.ecnpkt_handler = bbr_ecnpkt_handler,
	.newround = bbr_newround,
	.rttsample = bbr_rttsample, // Will be implemented
	.ctl_output = bbr_ctl_output,
};

static size_t
bbr_cc_data_sz(void)
{
	return (sizeof(struct bbr_state));
}

static int
bbr_mod_init(void)
{
	return (0);
}

static int
bbr_mod_destroy(void)
{
	return (0);
}

static int
bbr_cb_init(struct cc_var *ccv, void *ptr)
{
	struct bbr_state *bbr_s;

	if (ptr != NULL) {
		bbr_s = (struct bbr_state *)ptr;
		memset(bbr_s, 0, sizeof(struct bbr_state));
	} else {
		bbr_s = malloc(sizeof(struct bbr_state), M_CC_MEM, M_NOWAIT | M_ZERO);
		if (bbr_s == NULL) {
			ccv->cc_data = NULL; // Ensure cc_data is NULL on failure
			return (ENOMEM);
		}
	}
	bbr_s->bbr_tp = ccv->tp;
	ccv->cc_data = bbr_s;
	return (0);
}

static void
bbr_cb_destroy(struct cc_var *ccv)
{
	if (ccv->cc_data != NULL) {
		// Assuming M_CC_MEM was used if ccv->cc_data is not from stack/inline
		// This logic might need refinement based on how 'ptr' is truly managed
		// by the framework for non-M_CC_MEM allocations.
		// For now, if cc_data is set, we assume it was heap allocated by us or needs freeing.
		if (ccv->cc_data != CCV(ccv, cc_data)) { // Check if not inline part of cc_var
			free(ccv->cc_data, M_CC_MEM);
		}
		ccv->cc_data = NULL;
	}
}

static void
bbr_conn_init(struct cc_var *ccv)
{
	struct bbr_state *bb = (struct bbr_state *)ccv->cc_data;

	// Ensure bb is not NULL, though cb_init should prevent this if it failed
	if (bb == NULL) {
		// This case should ideally not happen if cb_init returned ENOMEM
		// and the framework handled it.
		// However, as a safeguard:
		if (bbr_cb_init(ccv, NULL) != 0) {
			// If allocation still fails, there's not much we can do here.
			// The connection will operate without valid BBR state.
			// Log an error or panic if appropriate in a real system.
			return;
		}
		bb = (struct bbr_state *)ccv->cc_data;
		// Re-assign tp just in case it was missed, though cb_init should handle it.
		bb->bbr_tp = ccv->tp;
	}


	bb->bbr_rtprop = TCPTV_INFINITE_USECS; // Initialize RTprop to a high value
	bb->bbr_BtlBw = 0;                     // Initialize BtlBw to 0
	bb->bbr_pacing_rate = 0;               // Initialize pacing rate
	bb->bbr_rtt_cnt = 0;                   // Initialize RTT count for ProbeRTT (legacy, may remove)

	/* Initialize PROBE_RTT fields */
	bb->bbr_rtprop_stamp = (uint64_t)ticks * (1000000 / hz);
	bb->bbr_probe_rtt_done_stamp = 0;
	bb->bbr_probe_rtt_round_done = false;

	/* Initialize BtlBw filter */
	memset(bb->bbr_btlbw_filter, 0, sizeof(bb->bbr_btlbw_filter));
	bb->bbr_btlbw_filter_idx = 0;

	// Other fields (gains, mode, startup-specific) set by bbr_enter_startup_mode
	// round_cnt and next_round_delivered also effectively start at 0 or tp->snd_una.
	if (bb->bbr_tp != NULL) {
		bb->bbr_next_round_delivered = bb->bbr_tp->snd_una;
	} else {
		bb->bbr_next_round_delivered = 0; // Should be set based on actual snd_una later
	}
	// Initialize BBR state for starting in STARTUP mode
	bbr_enter_startup_mode(bb);
}

static void
bbr_enter_startup_mode(struct bbr_state *bb)
{
	bb->bbr_mode = BBR_STARTUP;
	bb->bbr_pacing_gain = BBR_HIGH_GAIN;
	bb->bbr_cwnd_gain = BBR_HIGH_GAIN;
	bb->bbr_filled_pipe = false;
	bb->bbr_full_bw_cnt = 0;
	bb->bbr_full_bw = 0;
}

static bool
bbr_check_startup_done(struct cc_var *ccv, struct bbr_state *bb)
{
	/* Startup phase is considered done if the pipe is estimated to be filled. */
	if (bb->bbr_filled_pipe) {
		return (true);
	}

	/*
	 * Original BBR checks if BtlBw has been stable for a few rounds.
	 * This simplified version checks if BtlBw has not increased by much
	 * (e.g., < 25%) for 3 consecutive rounds.
	 * The round check itself (SEQ_GT(ccv->curack, bb->bbr_next_round_delivered))
	 * is handled in bbr_ack_received before this function might be called
	 * in the context of a new round.
	 */
	if (bb->bbr_BtlBw > bb->bbr_full_bw * 5 / 4) { /* Increased by >25% */
		bb->bbr_full_bw = bb->bbr_BtlBw;
		bb->bbr_full_bw_cnt = 0;
	} else {
		bb->bbr_full_bw_cnt++;
	}

	if (bb->bbr_full_bw_cnt >= 3 && bb->bbr_full_bw > 0) { /* Stable for 3 rounds with valid BtlBw */
		bb->bbr_filled_pipe = true;
	}

	return (bb->bbr_filled_pipe);
}

static void
bbr_enter_drain_mode(struct cc_var *ccv, struct bbr_state *bb)
{
	bb->bbr_mode = BBR_DRAIN;
	/* Set pacing gain to drain queue: 1/high_gain, e.g., 1/2.89 = ~0.346 */
	/* (BBR_UNIT / BBR_HIGH_GAIN) = (1024 / 2959) approx 0.346 * 1024 = 354 */
	struct tcpcb *tp = bb->bbr_tp;

	bb->bbr_mode = BBR_DRAIN;
	/* Set pacing gain to drain queue: 1/high_gain */
	bb->bbr_pacing_gain = BBR_UNIT * BBR_UNIT / BBR_HIGH_GAIN;
	/* In DRAIN, cwnd_gain is kept high to allow inflight to approach BDP quickly */
	/* then it's capped at BDP. Some BBR versions use 1.0 for cwnd_gain in DRAIN. */
	/* For now, stick to high gain and then cap. */
	bb->bbr_cwnd_gain = BBR_HIGH_GAIN;

	if (tp) {
		/* Target inflight is BDP. Actual cwnd will be managed in ack_received. */
	}
}

static uint64_t
bbr_bdp(struct bbr_state *bb, struct tcpcb *tp)
{
	uint64_t bdp;

	if (bb->bbr_rtprop == TCPTV_INFINITE_USECS || bb->bbr_rtprop == 0 || bb->bbr_BtlBw == 0) {
		/* Not enough info for BDP, return a fallback based on initial window or min */
		return (tp ? (uint64_t)2 * tp->t_maxseg : (uint64_t)2 * 1460); /* Fallback if tp is NULL */
	}

	bdp = bb->bbr_BtlBw * bb->bbr_rtprop; /* BtlBw (Bps) * RTprop (us) */
	bdp /= 1000000; /* Convert to bytes */

	return (max(bdp, (tp ? (uint64_t)2 * tp->t_maxseg : (uint64_t)2 * 1460)));
}

static void
bbr_enter_probe_bw_mode(struct cc_var *ccv, struct bbr_state *bb)
{
	struct tcpcb *tp = bb->bbr_tp;

	bb->bbr_mode = BBR_PROBE_BW;
	bb->bbr_cwnd_gain = 2 * BBR_UNIT; /* Recommended cwnd gain for PROBE_BW is 2.0 */

	/* Initialize cycle phase. Start at the end of the cycle array so that
	 * the first call to bbr_advance_probe_bw_cycle_phase will wrap around
	 * to the first element (index 0). */
	bb->bbr_cycle_idx = BBR_CYCLE_LEN - 1;
	bbr_advance_probe_bw_cycle_phase(ccv, bb); /* Sets initial gain and timestamp */
}

static void
bbr_advance_probe_bw_cycle_phase(struct cc_var *ccv, struct bbr_state *bb)
{
	/* struct tcpcb *tp = bb->bbr_tp; // For more precise timestamp if needed */
	bb->bbr_cycle_idx = (bb->bbr_cycle_idx + 1) % BBR_CYCLE_LEN;
	bb->bbr_pacing_gain = bbr_pacing_gain_cycle[bb->bbr_cycle_idx];
	bb->bbr_cycle_ustamp = (uint64_t)ticks * (1000000 / hz); /* Placeholder timestamp */
}

static void
bbr_update_probe_bw_cycle_phase(struct cc_var *ccv, struct bbr_state *bb)
{
	/* struct tcpcb *tp = bb->bbr_tp; // For more precise timestamp if needed */
	uint64_t now = (uint64_t)ticks * (1000000 / hz); /* Placeholder timestamp */

	/* Cannot determine phase duration if RTprop is unknown or BtlBw is zero. */
	if (bb->bbr_rtprop == TCPTV_INFINITE_USECS || bb->bbr_rtprop == 0) {
		return;
	}
	/* BtlBw check is mostly for sanity, PROBE_BW should have a valid BtlBw.
	 * If BtlBw is 0 here, it might indicate a problem, potentially needing a reset to STARTUP.
	 */
	if (bb->bbr_BtlBw == 0) {
		/* Consider what to do here. For now, don't advance phase if BtlBw is unknown. */
		return;
	}

	/*
	 * Advance phase if current phase has lasted longer than one RTprop.
	 * More sophisticated BBR versions might also check if a BDP worth of data
	 * has been sent in this phase, especially for the up-probing phase (5/4 gain).
	 * This simplified version only checks RTprop duration for all phases.
	 */
	if ((now - bb->bbr_cycle_ustamp) > bb->bbr_rtprop) {
		bbr_advance_probe_bw_cycle_phase(ccv, bb);
	}
}


static void
bbr_check_drain(struct cc_var *ccv, struct bbr_state *bb)
{
	struct tcpcb *tp = bb->bbr_tp;
	uint64_t bdp_val;
	uint32_t inflight;

	if (tp == NULL)
		return;

	bdp_val = bbr_bdp(bb, tp);
	inflight = tp->snd_max - tp->snd_una; /* Approximate bytes in flight */

	/* If inflight is at or below BDP, DRAIN phase is complete. */
	if (inflight <= bdp_val) {
		bbr_enter_probe_bw_mode(ccv, bb);
	}
}


static void
bbr_ack_received(struct cc_var *ccv, ccsignal_t type)
{
	struct bbr_state *bb = (struct bbr_state *)ccv->cc_data;
	struct tcpcb *tp;
	uint32_t rtt_us = 0; /* Calculated RTT for delivery rate, not for RTprop here */

	if (bb == NULL)
		return;

	tp = bb->bbr_tp;
	if (tp == NULL)
		return;

	/* Ignore DUPACKs for now. */
	if (type == CC_DUPACK)
		return;

	/* Data Collection & Model Update */

	/*
	 * Get RTT of the segment that advanced RTT measurement if available,
	 * otherwise fall back to smoothed RTT.
	 * t_rtttime is the RTT measurement from the most recent ACK
	 * that updated SRTT. It is already in microseconds.
	 */
	if (tp->t_rtttime > 0 && ccv->curack > tp->rtt_seq) {
		rtt_us = tp->t_rtttime; // Already in microseconds
	} else {
		/*
		 * tp->t_srtt is in tick units shifted by TCP_RTT_SHIFT (usually 5, so srtt/32 ticks).
		 * Convert ticks to microseconds. (1 tick = (1/hz) seconds = (1000000/hz) microseconds)
		 */
		rtt_us = (tp->t_srtt >> TCP_RTT_SHIFT) * (1000000 / hz);
	}

	/* RTprop update is now handled by bbr_rttsample callback. */
	/* The rtt_us calculated here is still used for delivery rate estimation. */

	/* Update Bottleneck Bandwidth (BtlBw) with windowed filter */
	if (rtt_us > 0 && ccv->bytes_this_ack > 0) {
		uint64_t current_delivery_rate = ((uint64_t)ccv->bytes_this_ack * 1000000) / rtt_us; // bytes per second

		if (current_delivery_rate > 0) { // Only update filter with valid, positive samples
			bb->bbr_btlbw_filter[bb->bbr_btlbw_filter_idx] = current_delivery_rate;
			bb->bbr_btlbw_filter_idx = (bb->bbr_btlbw_filter_idx + 1) % BBR_BTLBW_FILTER_LEN;

			// Recalculate BtlBw as the max of the window
			uint64_t max_filter_val = 0;
			int i;
			for (i = 0; i < BBR_BTLBW_FILTER_LEN; i++) {
				if (bb->bbr_btlbw_filter[i] > max_filter_val) {
					max_filter_val = bb->bbr_btlbw_filter[i];
				}
			}
			// Only update BtlBw if the new max_filter_val is non-zero,
			// or if BtlBw itself is zero (initial condition).
			if (max_filter_val > 0 || bb->bbr_BtlBw == 0) {
				 bb->bbr_BtlBw = max_filter_val;
			}
		}
	}

	/* Round Counting - This must happen before bbr_check_startup_done if it relies on round progression */
	bool new_round = false;
	if (SEQ_GT(ccv->curack, bb->bbr_next_round_delivered)) {
		bb->bbr_next_round_delivered = tp->snd_max;
		bb->bbr_round_cnt++;
		new_round = true;
		/* Placeholder: bbr_handle_new_round_transition(); */
	}

	/* Basic State Machine */
	switch (bb->bbr_mode) {
	case BBR_STARTUP:
		/* Check if STARTUP phase is done */
		if (new_round && bbr_check_startup_done(ccv, bb)) { // Check only on new rounds
			bbr_enter_drain_mode(ccv, bb);
			/* Fall through to DRAIN or handle next ACK in DRAIN?
			 * For now, next ACK will handle DRAIN logic.
			 * Pacing rate and CWND for DRAIN will be set on next ACK.
			 */
		} else {
			/* Pacing rate calculation for STARTUP */
			if (bb->bbr_BtlBw > 0) {
				bb->bbr_pacing_rate = (bb->bbr_BtlBw * bb->bbr_pacing_gain) / BBR_UNIT;
			} else {
				/* Initial pacing rate if BtlBw not yet known.
				 * Can use initial cwnd / estimated initial RTT, or fixed safe rate.
				 * Let TCP stack handle if rate is 0 for now.
				 */
				 if (tp->snd_cwnd > 0 && bb->bbr_rtprop != TCPTV_INFINITE_USECS && bb->bbr_rtprop > 0) {
					bb->bbr_pacing_rate = (uint64_t)tp->snd_cwnd * 1000000 / bb->bbr_rtprop;
					bb->bbr_pacing_rate = (bb->bbr_pacing_rate * bb->bbr_pacing_gain) / BBR_UNIT;
				 } else {
					/* Fallback if rtprop is not available yet, could be a fixed small rate */
				 }
			}

			/* CWND calculation for STARTUP */
			if (tp) {
				if (!bb->bbr_filled_pipe && tp->snd_cwnd < tp->snd_ssthresh) {
					if (ccv->bytes_this_ack > 0)
						tp->snd_cwnd += ccv->bytes_this_ack;
				} else {
					uint64_t target_c = bbr_target_cwnd(bb, tp);
					if (tp->snd_cwnd < target_c && ccv->bytes_this_ack > 0) {
						tp->snd_cwnd += ccv->bytes_this_ack;
					}
					if (tp->snd_cwnd > target_c) {
						tp->snd_cwnd = target_c;
					}
				}
				if (tp->snd_cwnd < (2 * tp->t_maxseg)) {
					tp->snd_cwnd = (2 * tp->t_maxseg);
				}
			}
		}
		break;
	case BBR_DRAIN:
		/* Pacing rate calculation (uses gain set in bbr_enter_drain_mode) */
		if (bb->bbr_BtlBw > 0) {
			bb->bbr_pacing_rate = (bb->bbr_BtlBw * bb->bbr_pacing_gain) / BBR_UNIT;
		} else { /* Should ideally not happen if BtlBw was found in STARTUP */
			if (bb->bbr_rtprop != TCPTV_INFINITE_USECS && bb->bbr_rtprop != 0 && tp) {
				bb->bbr_pacing_rate = ((uint64_t)tp->snd_cwnd * 1000000 / bb->bbr_rtprop);
				bb->bbr_pacing_rate = (bb->bbr_pacing_rate * bb->bbr_pacing_gain) / BBR_UNIT;
			} else if (tp) { /* Further fallback to a minimal rate */
				bb->bbr_pacing_rate = ((uint64_t)tp->t_maxseg * 1000000 / 100000); /* MSS / 100ms estimate */
				bb->bbr_pacing_rate = (bb->bbr_pacing_rate * bb->bbr_pacing_gain) / BBR_UNIT;
			} else {
				bb->bbr_pacing_rate = 0; // Or some minimal default if tp is NULL
			}
		}

		/* CWND calculation for DRAIN */
		if (tp) {
			uint64_t target_c_drain = bbr_bdp(bb, tp);

			if (tp->snd_cwnd > target_c_drain) {
				tp->snd_cwnd = max(target_c_drain, tp->snd_cwnd - ccv->bytes_this_ack);
			} else if (tp->snd_cwnd < target_c_drain && ccv->bytes_this_ack > 0) {
				tp->snd_cwnd += ccv->bytes_this_ack;
			}
			if (tp->snd_cwnd > target_c_drain) {
				tp->snd_cwnd = target_c_drain;
			}

			if (tp->snd_cwnd < (2 * tp->t_maxseg)) { /* Ensure min */
				tp->snd_cwnd = (2 * tp->t_maxseg);
			}
			/* Check if drain phase is complete */
			bbr_check_drain(ccv, bb);
		}
		break;
	case BBR_PROBE_BW:
		bbr_update_probe_bw_cycle_phase(ccv, bb); /* Manage gain cycling */

		/* Pacing rate calculation (uses gain from current cycle phase) */
		if (bb->bbr_BtlBw > 0) {
			bb->bbr_pacing_rate = (bb->bbr_BtlBw * bb->bbr_pacing_gain) / BBR_UNIT;
		} else {
			/* Fallback: Should not happen often. If BtlBw is lost, might need to re-enter STARTUP. */
			if (tp) {
				bb->bbr_pacing_rate = ((uint64_t)tp->t_maxseg * 1000000 / 100000);
				bb->bbr_pacing_rate = (bb->bbr_pacing_rate * bb->bbr_pacing_gain) / BBR_UNIT;
			} else {
				bb->bbr_pacing_rate = 0;
			}
		}

		/* CWND calculation for PROBE_BW */
		if (tp) {
			uint64_t target_c_pbw = bbr_target_cwnd(bb, tp);

			if (tp->snd_cwnd < target_c_pbw && ccv->bytes_this_ack > 0) {
				tp->snd_cwnd += ccv->bytes_this_ack;
			}
			if (tp->snd_cwnd > target_c_pbw) {
				tp->snd_cwnd = target_c_pbw;
			}

			if (tp->snd_cwnd < (2 * tp->t_maxseg)) { /* Ensure min */
				tp->snd_cwnd = (2 * tp->t_maxseg);
			}
		}
		bbr_check_probe_rtt(ccv, bb); // Check if it's time to enter PROBE_RTT
		break;
	case BBR_PROBE_RTT:
		/* In PROBE_RTT, cwnd is clamped to a minimum (e.g., 2*MSS).
		 * Pacing rate is set to BtlBw * 0.75 to help drain the queue.
		 */
		bb->bbr_pacing_gain = BBR_UNIT * 3 / 4; /* 0.75 */
		if (bb->bbr_BtlBw > 0) {
			bb->bbr_pacing_rate = (bb->bbr_BtlBw * bb->bbr_pacing_gain) / BBR_UNIT;
		} else if (tp) { /* Fallback if BtlBw is somehow lost */
			bb->bbr_pacing_rate = ((uint64_t)tp->t_maxseg * 1000000 / 100000); /* MSS / 100ms */
			bb->bbr_pacing_rate = (bb->bbr_pacing_rate * bb->bbr_pacing_gain) / BBR_UNIT;
		} else {
			bb->bbr_pacing_rate = 0; /* Should not happen with valid tp */
		}

		if (tp) {
			/* Clamp cwnd to minimum */
			if (tp->snd_cwnd > (2 * tp->t_maxseg)) {
				tp->snd_cwnd = (2 * tp->t_maxseg);
			}
		}

		uint64_t now_ts_prtt = (uint64_t)ticks * (1000000 / hz);
		if (bb->bbr_probe_rtt_done_stamp == 0) { /* First ACK in this PROBE_RTT phase */
			bb->bbr_probe_rtt_done_stamp = now_ts_prtt + (200 * 1000); /* Stay for at least 200ms */
			bb->bbr_probe_rtt_round_done = false;
			/* Reset round detection for PROBE_RTT: ensure we see a full round trip of ACKs at low inflight.
			 * This means an ACK for data sent *after* entering PROBE_RTT. */
			if (tp) {
				bb->bbr_next_round_delivered = tp->snd_max;
			}
		} else {
			/* Check if a round has completed *within* PROBE_RTT */
			if (tp && SEQ_GT(ccv->curack, bb->bbr_next_round_delivered)) {
				bb->bbr_probe_rtt_round_done = true;
			}

			if (now_ts_prtt >= bb->bbr_probe_rtt_done_stamp && bb->bbr_probe_rtt_round_done) {
				bb->bbr_rtprop_stamp = now_ts_prtt; /* Update stamp: we've probed RTT */
				bbr_exit_probe_rtt_mode(ccv, bb);
			}
		}
		break;
	}

	/* CWND and Pacing Placeholder */
	/* Placeholder: bbr_calculate_pacing_rate(ccv, bb); */
	/* Placeholder: bbr_set_cwnd(ccv, bb); */
}

static void
bbr_cong_signal(struct cc_var *ccv, ccsignal_t type)
{
	struct bbr_state *bb = (struct bbr_state *)ccv->cc_data;
	struct tcpcb *tp;

	if (bb == NULL)
		return;

	tp = bb->bbr_tp;
	if (tp == NULL)
		return;

	switch (type) {
	case CC_RTO:
		/*
		 * BBR reduces cwnd to 1 MSS on RTO and may re-enter startup.
		 * Actual BBR might have more specific actions, e.g., resetting BtlBw,
		 * RTprop, and other model parameters if the RTO is seen as a sign
		 * of persistent congestion invalidating the current model.
		 */
		tp->snd_cwnd = tp->t_maxseg;
		bbr_enter_startup_mode(bb); // Re-enter startup on RTO
		/* Placeholder: Clear BtlBw sample? Or other RTO-specific recovery actions. */
		break;
/* TODO: Consider PROBE_RTT for CC_NDUPACK/CC_ECN if losses persist? */
	case CC_NDUPACK:
	case CC_ECN:
		/*
		 * On loss or ECN, BBR reacts by reducing the congestion window
		 * and potentially adjusting its state based on the current mode.
		 */

		// If we were in STARTUP and experienced loss, the pipe is likely full.
		// Mark it as filled so that on the next ACK, we transition to DRAIN.
		if (bb->bbr_mode == BBR_STARTUP) {
			bb->bbr_filled_pipe = true;
		}

		// If we were probing bandwidth upwards (gain > 1.0),
		// the loss/ECN indicates we've likely found the path capacity for this cycle.
		// Revert to a gain of 1.0 for the remainder of this probing sub-cycle.
		// The main gain cycle (bbr_update_probe_bw_cycle_phase) will continue as scheduled,
		// eventually moving to the cruise (1.0) or probe-down (0.75) phases.
		if (bb->bbr_mode == BBR_PROBE_BW && bb->bbr_pacing_gain > BBR_UNIT) {
			bb->bbr_pacing_gain = BBR_UNIT;
			// Note: cwnd_gain in PROBE_BW is typically fixed at 2.0.
			// BBR's primary mechanism to handle this is by setting cwnd to BDP,
			// ensuring inflight does not persistently exceed target_cwnd (gain * BDP).
		}

		// Set target congestion window to BDP (or minimum of 2 MSS).
		// This aims to clear any queue that might have formed due to overestimation
		// or transient congestion.
		if (tp) { // Ensure tp is not NULL
			uint64_t bdp_val = bbr_bdp(bb, tp);
			tp->snd_cwnd = max((uint32_t)(2 * tp->t_maxseg), (uint32_t)bdp_val);
		}

		/*
		 * Placeholder: More advanced BBR loss recovery might involve:
		 * - Explicitly tracking lost packets to adjust inflight estimate (e.g., using ccv->tx_data_retrans).
		 * - If losses are persistent, potentially reducing BtlBw estimate after a timeout
		 *   or if a certain number of loss events occur in a round.
		 * - Considering entering PROBE_RTT if losses make RTprop uncertain or if they occur
		 *   when inflight is already low.
		 */
		break;
	default:
		/* Nothing to do for other signal types in this initial version. */
		break;
	}
	/*
	 * The main TCP stack usually handles recording that a congestion event occurred
	 * (e.g., for standard TCP congestion window backoff procedures if applicable,
	 * or for statistics). BBR's direct manipulation of snd_cwnd here might
	 * interact with or override some of those default behaviors.
	 */
}

static uint64_t
bbr_target_cwnd(struct bbr_state *bb, struct tcpcb *tp)
{
	uint64_t bdp;
	uint64_t target;

	if (tp == NULL) { /* Should not happen if called from ack_received with valid tp */
		return (2 * 1460); /* Fallback to a generic 2*MSS */
	}

	bdp = bbr_bdp(bb, tp);
	target = (bdp * bb->bbr_cwnd_gain) / BBR_UNIT;

	/* Add modest ACK aggregation headroom (e.g., 2 MSS). */
	target += 2 * (uint64_t)tp->t_maxseg;

	/* Ensure target_cwnd is at least the minimum pipe cwnd. */
	return (max(target, (uint64_t)(2 * tp->t_maxseg)));
}


static void
bbr_enter_probe_rtt_mode(struct cc_var *ccv, struct bbr_state *bb)
{
	/* struct tcpcb *tp = bb->bbr_tp; // Not strictly needed for these settings */
	bb->bbr_mode = BBR_PROBE_RTT;
	bb->bbr_pacing_gain = BBR_UNIT; /* Pacing gain in ProbeRTT is 1.0 */
	bb->bbr_cwnd_gain = BBR_UNIT;   /* cwnd gain is also 1.0, but cwnd is clamped low */
	/* Actual cwnd clamping and exit conditions are handled in ack_received for PROBE_RTT */
}

static void
bbr_exit_probe_rtt_mode(struct cc_var *ccv, struct bbr_state *bb)
{
	if (bb->bbr_filled_pipe) {
		bbr_enter_probe_bw_mode(ccv, bb);
	} else {
		bbr_enter_startup_mode(bb);
	}
}

static void
bbr_check_probe_rtt(struct cc_var *ccv, struct bbr_state *bb)
{
	struct tcpcb *tp = bb->bbr_tp;
	uint64_t now_ts;
	uint32_t inflight;
	uint64_t bdp_val;

	if (tp == NULL || bb->bbr_mode == BBR_PROBE_RTT) { /* Already in PROBE_RTT or no tp */
		return;
	}

	now_ts = (uint64_t)ticks * (1000000 / hz);

	/*
	 * Enter PROBE_RTT if RTprop hasn't been updated for ~10s
	 * AND inflight is low enough (e.g., <= BDP or a minimum).
	 * BBR aims to keep inflight at BDP * ProbeRTT_cwnd_gain (typically 0.5)
	 * or ProbeRTT_min_bytes (typically 4*MSS).
	 * We simplify by checking if inflight is <= BDP or 4*MSS.
	 */
	if (bb->bbr_rtprop != TCPTV_INFINITE_USECS &&
	    (now_ts - bb->bbr_rtprop_stamp) > (10 * 1000000) /* 10 seconds */ ) {

		bdp_val = bbr_bdp(bb, tp); /* This already has a min of 2*MSS */
		inflight = tp->snd_max - tp->snd_una;

		/* Check if inflight is low enough. BBR draft suggests target of BDP or ProbeRTT_min_bytes */
		/* For simplicity, if inflight is already low (e.g. near min_cwnd), or if BDP is small. */
		/* Here, we use max(BDP, 4*MSS) as the threshold for inflight to be low enough. */
		/* Original BBR might trigger ProbeRTT more aggressively if inflight is already low. */
		/* Let's use a threshold that allows ProbeRTT if inflight is not excessive. */
		/* A common check is if inflight <= BDP_TARGET_FOR_PROBERTT or MIN_CWND_FOR_PROBERTT */
		/* For now, let's use a simplified condition: if inflight is not much larger than BDP */
		/* or if it's already very low (like min_pipe_cwnd which is 2*MSS). */
		/* The BBR paper mentions "if inflight is less than or equal to BDP + ack aggregation headroom" */
		/* For this implementation, we'll use max(bdp_val, 4 * tp->t_maxseg) as a threshold. */
		/* This means if current inflight is below this, we can enter probe_rtt. */
		/* This condition might need tuning. */
		if (inflight <= max(bdp_val, (uint64_t)(4 * tp->t_maxseg))) {
			bbr_enter_probe_rtt_mode(ccv, bb);
			bb->bbr_probe_rtt_done_stamp = 0; /* Mark that we need to set the exit time */
			bb->bbr_probe_rtt_round_done = false;
			/* rtprop_stamp is updated on exiting PROBE_RTT or when a new min_rtt is found */
		}
	}
}


static void
bbr_post_recovery(struct cc_var *ccv)
{
	struct bbr_state *bb = (struct bbr_state *)ccv->cc_data;
	struct tcpcb *tp;

	if (bb == NULL)
		return;
	tp = bb->bbr_tp;
	if (tp == NULL)
		return;

	/* After fast recovery, BBR might re-assess its state. */
	/* If we were in STARTUP and hit recovery, it implies pipe might be full. */
	if (bb->bbr_mode == BBR_STARTUP) {
		bb->bbr_filled_pipe = true; /* Mark pipe as filled */
		bbr_enter_drain_mode(ccv, bb);
	} else if (bb->bbr_mode == BBR_PROBE_BW) {
		/* If in PROBE_BW, ensure gains are conservative (1.0) after recovery. */
		bb->bbr_pacing_gain = BBR_UNIT;
		bb->bbr_cwnd_gain = 2 * BBR_UNIT; /* Standard PROBE_BW cwnd gain */
	}

	/* Reset round counting for a fresh perspective after recovery. */
	bb->bbr_next_round_delivered = tp->snd_max;
	/* It's debatable whether to reset bbr_round_cnt. Let's not reset it for now,
	 * as it tracks overall progress. Startup exit condition uses its own counter. */
}

static void
bbr_after_idle(struct cc_var *ccv)
{
	struct bbr_state *bb = (struct bbr_state *)ccv->cc_data;
	/* struct tcpcb *tp = bb->bbr_tp; // tp might not be strictly needed here */

	if (bb == NULL)
		return;

	/* After an idle period, BBR's estimates (BtlBw, RTprop) can be stale.
	 * It's generally safe to re-enter STARTUP to quickly find the new path characteristics. */
	bbr_enter_startup_mode(bb);

	/* Reset RTprop estimate and its timestamp. */
	bb->bbr_rtprop = TCPTV_INFINITE_USECS;
	/* Set to current time to avoid immediate PROBE_RTT unless it's naturally due. */
	bb->bbr_rtprop_stamp = (uint64_t)ticks * (1000000 / hz);

	/* Reset BtlBw estimate and its filter as they are likely stale. */
	bb->bbr_BtlBw = 0;
	memset(bb->bbr_btlbw_filter, 0, sizeof(bb->bbr_btlbw_filter));
	bb->bbr_btlbw_filter_idx = 0;
	bb->bbr_full_bw = 0; /* Reset for startup bandwidth growth check */
	bb->bbr_full_bw_cnt = 0; /* Reset for startup bandwidth growth check */

	/*
	 * CWND after idle:
	 * The main TCP stack usually handles initial cwnd (e.g. IW based on RFC3390/5681).
	 * BBR's bbr_enter_startup_mode sets high gains, and the first few ACKs in
	 * bbr_ack_received (STARTUP mode) will rapidly grow cwnd if allowed by ssthresh.
	 * So, no explicit cwnd set here needed, rely on STARTUP mode's logic.
	 */
}

static void
bbr_ecnpkt_handler(struct cc_var *ccv)
{
}

static void
bbr_newround(struct cc_var *ccv, uint32_t round_cnt)
{
}

static void
bbr_rttsample(struct cc_var *ccv, uint32_t rtt_sample_us, uint32_t rxtcnt, uint32_t fas)
{
	struct bbr_state *bb = (struct bbr_state *)ccv->cc_data;

	if (bb == NULL)
		return;

	/* The 'rtt_sample_us' parameter is the RTT sample in microseconds provided by the stack. */
	if (rtt_sample_us > 0 && rtt_sample_us < bb->bbr_rtprop) {
		bb->bbr_rtprop = rtt_sample_us;
		bb->bbr_rtprop_stamp = (uint64_t)ticks * (1000000 / hz); /* Update timestamp */
	}

	/*
	 * 'fas' (flight at send) could be used by BBR for more advanced BtlBw estimation
	 * if needed, but the current delivery rate calculation in bbr_ack_received is common.
	 * 'rxtcnt' (retransmit count for this RTT sample) might also be useful for BBR's model,
	 * e.g., to invalidate BtlBw samples if associated with retransmissions.
	 */
}

static int
bbr_ctl_output(struct cc_var *ccv, struct sockopt *sopt, void *buf)
{
	return (ENOTSUP);
}

CC_ALGO_DECLARE(bbr, &bbr_algo);
