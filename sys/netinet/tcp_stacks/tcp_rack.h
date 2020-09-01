/*-
 * Copyright (c) 2016-2020 Netflix, Inc.
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
 * $FreeBSD$
 */

#ifndef _NETINET_TCP_RACK_H_
#define _NETINET_TCP_RACK_H_

#define RACK_ACKED	    0x0001/* The remote endpoint acked this */
#define RACK_TO_MIXED	    0x0002/* A timeout occured that mixed the send order - not used */
#define RACK_DEFERRED	    0x0004/* We can't use this for RTT calc - not used */
#define RACK_OVERMAX	    0x0008/* We have more retran's then we can fit */
#define RACK_SACK_PASSED    0x0010/* A sack was done above this block */
#define RACK_WAS_SACKPASS   0x0020/* We retransmitted due to SACK pass */
#define RACK_HAS_FIN	    0x0040/* segment is sent with fin */
#define RACK_TLP	    0x0080/* segment sent as tail-loss-probe */
#define RACK_RWND_COLLAPSED 0x0100/* The peer collapsed the rwnd on the segment */
#define RACK_APP_LIMITED    0x0200/* We went app limited after this send */
#define RACK_WAS_ACKED	    0x0400/* a RTO undid the ack, but it already had a rtt calc done */
#define RACK_HAS_SIN	    0x0800/* SIN is on this guy */
#define RACK_NUM_OF_RETRANS 3

#define RACK_INITIAL_RTO 1000 /* 1 second in milli seconds */

#define RACK_REQ_AVG 4 	/* Must be less than 256 */

struct rack_sendmap {
	uint32_t r_start;	/* Sequence number of the segment */
	uint32_t r_end;		/* End seq, this is 1 beyond actually */
	TAILQ_ENTRY(rack_sendmap) r_tnext;	/* Time of transmit based next */
	RB_ENTRY(rack_sendmap) r_next;		/* RB Tree next */
	uint32_t r_rtr_bytes;	/* How many bytes have been retransmitted */
	uint16_t r_rtr_cnt;	/* Retran count, index this -1 to get time
				 * sent */
	uint16_t r_flags;	/* Flags as defined above */
	uint32_t r_tim_lastsent[RACK_NUM_OF_RETRANS];
	uint32_t usec_orig_send;	/* time of orginal send in useconds */
	uint32_t r_nseq_appl;	/* If this one is app limited, this is the nxt seq limited */
	uint32_t r_ack_arrival;	/* This is the time of ack-arrival (if SACK'd) */
	uint8_t r_dupack;	/* Dup ack count */
	uint8_t r_in_tmap;	/* Flag to see if its in the r_tnext array */
	uint8_t r_limit_type;	/* is this entry counted against a limit? */
	uint8_t r_just_ret : 1, /* After sending, the next pkt was just returned, i.e. limited  */
		r_one_out_nr : 1,	/* Special case 1 outstanding and not in recovery */
		r_avail : 6;
	uint8_t r_resv[36];
};

RB_HEAD(rack_rb_tree_head, rack_sendmap);
TAILQ_HEAD(rack_head, rack_sendmap);

#define RACK_LIMIT_TYPE_SPLIT	1

/*
 * We use the rate sample structure to
 * assist in single sack/ack rate and rtt
 * calculation. In the future we will expand
 * this in BBR to do forward rate sample
 * b/w estimation.
 */
#define RACK_RTT_EMPTY 0x00000001	/* Nothing yet stored in RTT's */
#define RACK_RTT_VALID 0x00000002	/* We have at least one valid RTT */
struct rack_rtt_sample {
	uint32_t rs_flags;
	uint32_t rs_rtt_lowest;
	uint32_t rs_rtt_highest;
	uint32_t rs_rtt_cnt;
	uint32_t rs_us_rtt;
	int32_t  confidence;
	uint64_t rs_rtt_tot;
	uint16_t rs_us_rtrcnt;
};

#define RACK_LOG_TYPE_ACK	0x01
#define RACK_LOG_TYPE_OUT	0x02
#define RACK_LOG_TYPE_TO	0x03
#define RACK_LOG_TYPE_ALLOC     0x04
#define RACK_LOG_TYPE_FREE      0x05

struct rack_log {
	union {
		struct rack_sendmap *rsm;	/* For alloc/free */
		uint64_t sb_acc;/* For out/ack or t-o */
	};
	uint32_t th_seq;
	uint32_t th_ack;
	uint32_t snd_una;
	uint32_t snd_nxt;	/* th_win for TYPE_ACK */
	uint32_t snd_max;
	uint32_t blk_start[4];
	uint32_t blk_end[4];
	uint8_t type;
	uint8_t n_sackblks;
	uint16_t len;		/* Timeout T3=1, TLP=2, RACK=3 */
};

/*
 * Magic numbers for logging timeout events if the
 * logging is enabled.
 */
#define RACK_TO_FRM_TMR  1
#define RACK_TO_FRM_TLP  2
#define RACK_TO_FRM_RACK 3
#define RACK_TO_FRM_KEEP 4
#define RACK_TO_FRM_PERSIST 5
#define RACK_TO_FRM_DELACK 6

struct rack_opts_stats {
	uint64_t tcp_rack_prop_rate;
 	uint64_t tcp_rack_prop;
	uint64_t tcp_rack_tlp_reduce;
	uint64_t tcp_rack_early_recov;
	uint64_t tcp_rack_pace_always;
	uint64_t tcp_rack_pace_reduce;
	uint64_t tcp_rack_max_seg;
	uint64_t tcp_rack_prr_sendalot;
	uint64_t tcp_rack_min_to;
	uint64_t tcp_rack_early_seg;
	uint64_t tcp_rack_reord_thresh;
	uint64_t tcp_rack_reord_fade;
	uint64_t tcp_rack_tlp_thresh;
	uint64_t tcp_rack_pkt_delay;
	uint64_t tcp_rack_tlp_inc_var;
	uint64_t tcp_tlp_use;
	uint64_t tcp_rack_idle_reduce;
	uint64_t tcp_rack_idle_reduce_high;
	uint64_t rack_no_timer_in_hpts;
	uint64_t tcp_rack_min_pace_seg;
	uint64_t tcp_rack_pace_rate_ca;
	uint64_t tcp_rack_rr;
	uint64_t tcp_rack_do_detection;
	uint64_t tcp_rack_rrr_no_conf_rate;
	uint64_t tcp_initial_rate;
	uint64_t tcp_initial_win;
	uint64_t tcp_hdwr_pacing;
	uint64_t tcp_gp_inc_ss;
	uint64_t tcp_gp_inc_ca;
	uint64_t tcp_gp_inc_rec;
	uint64_t tcp_rack_force_max_seg;
	uint64_t tcp_rack_pace_rate_ss;
	uint64_t tcp_rack_pace_rate_rec;
	/* Temp counters for dsack */
	uint64_t tcp_sack_path_1;
	uint64_t tcp_sack_path_2a;
	uint64_t tcp_sack_path_2b;
	uint64_t tcp_sack_path_3;
	uint64_t tcp_sack_path_4;
	/* non temp counters */
	uint64_t tcp_rack_scwnd;
	uint64_t tcp_rack_noprr;
	uint64_t tcp_rack_cfg_rate;
	uint64_t tcp_timely_dyn;
	uint64_t tcp_rack_mbufq;
	uint64_t tcp_fillcw;
	uint64_t tcp_npush;
	uint64_t tcp_lscwnd;
	uint64_t tcp_profile;
};

/* RTT shrink reasons */
#define RACK_RTTS_INIT     0
#define RACK_RTTS_NEWRTT   1
#define RACK_RTTS_EXITPROBE 2
#define RACK_RTTS_ENTERPROBE 3
#define RACK_RTTS_REACHTARGET 4
#define RACK_RTTS_SEEHBP 5
#define RACK_RTTS_NOBACKOFF 6
#define RACK_RTTS_SAFETY 7

#define RACK_USE_BEG 1
#define RACK_USE_END 2
#define RACK_USE_END_OR_THACK 3

#define TLP_USE_ID	1	/* Internet draft behavior */
#define TLP_USE_TWO_ONE 2	/* Use 2.1 behavior */
#define TLP_USE_TWO_TWO 3	/* Use 2.2 behavior */
#define RACK_MIN_BW 8000	/* 64kbps in Bps */

#define MIN_GP_WIN 6	/* We need at least 6 MSS in a GP measurement */
#ifdef _KERNEL
#define RACK_OPTS_SIZE (sizeof(struct rack_opts_stats)/sizeof(uint64_t))
extern counter_u64_t rack_opts_arry[RACK_OPTS_SIZE];
#define RACK_OPTS_ADD(name, amm) counter_u64_add(rack_opts_arry[(offsetof(struct rack_opts_stats, name)/sizeof(uint64_t))], (amm))
#define RACK_OPTS_INC(name) RACK_OPTS_ADD(name, 1)
#endif
/*
 * As we get each SACK we wade through the
 * rc_map and mark off what is acked.
 * We also increment rc_sacked as well.
 *
 * We also pay attention to missing entries
 * based on the time and possibly mark them
 * for retransmit. If we do and we are not already
 * in recovery we enter recovery. In doing
 * so we claer prr_delivered/holes_rxt and prr_sent_dur_rec.
 * We also setup rc_next/rc_snd_nxt/rc_send_end so
 * we will know where to send from. When not in
 * recovery rc_next will be NULL and rc_snd_nxt should
 * equal snd_max.
 *
 * Whenever we retransmit from recovery we increment
 * rc_holes_rxt as we retran a block and mark it as retransmitted
 * with the time it was sent. During non-recovery sending we
 * add to our map and note the time down of any send expanding
 * the rc_map at the tail and moving rc_snd_nxt up with snd_max.
 *
 * In recovery during SACK/ACK processing if a chunk has
 * been retransmitted and it is now acked, we decrement rc_holes_rxt.
 * When we retransmit from the scoreboard we use
 * rc_next and rc_snd_nxt/rc_send_end to help us
 * find what needs to be retran.
 *
 * To calculate pipe we simply take (snd_max - snd_una) + rc_holes_rxt
 * This gets us the effect of RFC6675 pipe, counting twice for
 * bytes retransmitted.
 */

#define TT_RACK_FR_TMR	0x2000

/*
 * Locking for the rack control block.
 * a) Locked by INP_WLOCK
 * b) Locked by the hpts-mutex
 *
 */
#define RACK_GP_HIST 4	/* How much goodput history do we maintain? */

struct rack_control {
	/* Second cache line 0x40 from tcp_rack */
	struct rack_rb_tree_head rc_mtree; /* Tree of all segments Lock(a) */
	struct rack_head rc_tmap;	/* List in transmit order Lock(a) */
	struct rack_sendmap *rc_tlpsend;	/* Remembered place for
						 * tlp_sending Lock(a) */
	struct rack_sendmap *rc_resend;	/* something we have been asked to
					 * resend */
	uint32_t input_pkt;
	uint32_t saved_input_pkt;
	uint32_t rc_hpts_flags;
	uint32_t rc_fixed_pacing_rate_ca;
	uint32_t rc_fixed_pacing_rate_rec;
	uint32_t rc_fixed_pacing_rate_ss;
	uint32_t cwnd_to_use;	/* The cwnd in use */
	uint32_t rc_timer_exp;	/* If a timer ticks of expiry */
	uint32_t rc_rack_min_rtt;	/* lowest RTT seen Lock(a) */
	uint32_t rc_rack_largest_cwnd;	/* Largest CWND we have seen Lock(a) */

	/* Third Cache line 0x80 */
	struct rack_head rc_free;	/* Allocation array */
	uint32_t rc_time_last_sent;	/* Time we last sent some data and
					 * logged it Lock(a). */
	uint32_t rc_reorder_ts;	/* Last time we saw reordering Lock(a) */

	uint32_t rc_tlp_new_data;	/* we need to send new-data on a TLP
					 * Lock(a) */
	uint32_t rc_prr_out;	/* bytes sent during recovery Lock(a) */

	uint32_t rc_prr_recovery_fs;	/* recovery fs point Lock(a) */

	uint32_t rc_prr_sndcnt;	/* Prr sndcnt Lock(a) */

	uint32_t rc_sacked;	/* Tot sacked on scoreboard Lock(a) */
	uint32_t xxx_rc_last_tlp_seq;	/* Last tlp sequence Lock(a) */

	uint32_t rc_prr_delivered;	/* during recovery prr var Lock(a) */
	uint16_t rc_tlp_cnt_out;	/* count of times we have sent a TLP without new data */
	uint16_t xxx_rc_tlp_seg_send_cnt;	/* Number of times we have TLP sent
					 * rc_last_tlp_seq Lock(a) */

	uint32_t rc_loss_count;	/* How many bytes have been retransmitted
				 * Lock(a) */
	uint32_t rc_reorder_fade;	/* Socket option value Lock(a) */

	/* Forth cache line 0xc0  */
	/* Times */

	uint32_t rc_rack_tmit_time;	/* Rack transmit time Lock(a) */
	uint32_t rc_holes_rxt;	/* Tot retraned from scoreboard Lock(a) */

	/* Variables to track bad retransmits and recover */
	uint32_t rc_rsm_start;	/* RSM seq number we retransmitted Lock(a) */
	uint32_t rc_cwnd_at;	/* cwnd at the retransmit Lock(a) */

	uint32_t rc_ssthresh_at;/* ssthresh at the retransmit Lock(a) */
	uint32_t rc_num_maps_alloced;	/* Number of map blocks (sacks) we
					 * have allocated */
	uint32_t rc_rcvtime;	/* When we last received data */
	uint32_t rc_num_split_allocs;	/* num split map entries allocated */

	uint32_t rc_last_output_to;
	uint32_t rc_went_idle_time;

	struct rack_sendmap *rc_sacklast;	/* sack remembered place
						 * Lock(a) */

	struct rack_sendmap *rc_rsm_at_retran;	/* Debug variable kept for
						 * cache line alignment
						 * Lock(a) */
	struct rack_sendmap *rc_first_appl;	/* Pointer to first app limited */
	struct rack_sendmap *rc_end_appl;	/* Pointer to last app limited */
	/* Cache line split 0x100 */
	struct sack_filter rack_sf;
	/* Cache line split 0x140 */
	/* Flags for various things */
	uint32_t last_pacing_time;
	uint32_t rc_pace_max_segs;
	uint32_t rc_pace_min_segs;
	uint32_t rc_app_limited_cnt;
	uint16_t rack_per_of_gp_ss; /* 100 = 100%, so from 65536 = 655 x bw  */
	uint16_t rack_per_of_gp_ca; /* 100 = 100%, so from 65536 = 655 x bw  */
	uint16_t rack_per_of_gp_rec; /* 100 = 100%, so from 65536 = 655 x bw, 0=off */
	uint16_t rack_per_of_gp_probertt; /* 100 = 100%, so from 65536 = 655 x bw, 0=off */
	uint32_t rc_high_rwnd;
	uint32_t ack_count;
	uint32_t sack_count;
	uint32_t sack_noextra_move;
	uint32_t sack_moved_extra;
	struct rack_rtt_sample rack_rs;
	const struct tcp_hwrate_limit_table *crte;
	uint32_t rc_agg_early;
	uint32_t rc_agg_delayed;
	uint32_t rc_tlp_rxt_last_time;
	uint32_t rc_saved_cwnd;
	uint32_t rc_gp_output_ts;
	uint32_t rc_gp_cumack_ts;
	struct timeval act_rcv_time;
	struct timeval rc_last_time_decay;	/* SAD time decay happened here */
	uint64_t gp_bw;
	uint64_t init_rate;
#ifdef NETFLIX_SHARED_CWND
	struct shared_cwnd *rc_scw;
#endif
	uint64_t last_gp_comp_bw;
	uint64_t last_max_bw;	/* Our calculated max b/w last */
	struct time_filter_small rc_gp_min_rtt;
	int32_t rc_rtt_diff;		/* Timely style rtt diff of our gp_srtt */
	uint32_t rc_gp_srtt;		/* Current GP srtt */
	uint32_t rc_prev_gp_srtt;	/* Previous RTT */
	uint32_t rc_entry_gp_rtt;	/* Entry to PRTT gp-rtt */
	uint32_t rc_loss_at_start;	/* At measurement window where was our lost value */

	uint32_t forced_ack_ts;
	uint32_t rc_lower_rtt_us_cts;	/* Time our GP rtt was last lowered */
	uint32_t rc_time_probertt_entered;
	uint32_t rc_time_probertt_starts;
	uint32_t rc_lowest_us_rtt;
	uint32_t rc_highest_us_rtt;
	uint32_t rc_last_us_rtt;
	uint32_t rc_time_of_last_probertt;
	uint32_t rc_target_probertt_flight;
	uint32_t rc_probertt_sndmax_atexit;	/* Highest sent to in probe-rtt */
	uint32_t rc_gp_lowrtt;			/* Lowest rtt seen during GPUT measurement */
	uint32_t rc_gp_high_rwnd;		/* Highest rwnd seen during GPUT measurement */
	int32_t rc_scw_index;
	uint32_t rc_tlp_threshold;	/* Socket option value Lock(a) */
	uint16_t rc_early_recovery_segs;	/* Socket option value Lock(a) */
	uint16_t rc_reorder_shift;	/* Socket option value Lock(a) */
	uint16_t rc_pkt_delay;	/* Socket option value Lock(a) */
	uint8_t rc_no_push_at_mrtt;	/* No push when we exceed max rtt */
	uint8_t num_avg;	/* average count before we go to normal decay */
	uint8_t rc_prop_rate;	/* Socket option value Lock(a) */
	uint8_t rc_prop_reduce;	/* Socket option value Lock(a) */
	uint8_t rc_tlp_cwnd_reduce;	/* Socket option value Lock(a) */
	uint8_t rc_early_recovery;	/* Socket option value Lock(a) */
	uint8_t rc_prr_sendalot;/* Socket option value Lock(a) */
	uint8_t rc_min_to;	/* Socket option value Lock(a) */
	uint8_t rc_rate_sample_method;
	uint8_t rc_gp_hist_idx;
};

#define RACK_TIMELY_CNT_BOOST 5	/* At 5th increase boost */
#define RACK_MINRTT_FILTER_TIM 10 /* Seconds */

#ifdef _KERNEL

struct tcp_rack {
	/* First cache line 0x00 */
	TAILQ_ENTRY(tcp_rack) r_hpts;	/* hptsi queue next Lock(b) */
	int32_t(*r_substate) (struct mbuf *, struct tcphdr *,
	    struct socket *, struct tcpcb *, struct tcpopt *,
	    int32_t, int32_t, uint32_t, int, int, uint8_t);	/* Lock(a) */
	struct tcpcb *rc_tp;	/* The tcpcb Lock(a) */
	struct inpcb *rc_inp;	/* The inpcb Lock(a) */
	uint32_t rc_free_cnt;	/* Number of free entries on the rc_free list
				 * Lock(a) */
	uint32_t rc_rack_rtt;	/* RACK-RTT Lock(a) */
	uint16_t r_mbuf_queue : 1,	/* Do we do mbuf queue for non-paced */
		 rtt_limit_mul : 4,	/* muliply this by low rtt */
		 r_limit_scw : 1,
		 r_avail_bits : 10;	/* Available */

	uint16_t rc_user_set_max_segs;	/* Socket option value Lock(a) */
	uint16_t forced_ack : 1,
		rc_gp_incr : 1,
		rc_gp_bwred : 1,
		rc_gp_timely_inc_cnt : 3,
		rc_gp_timely_dec_cnt : 3,
		rc_not_backing_off: 1,
		rc_highly_buffered: 1,		/* The path is highly buffered */
		rc_dragged_bottom: 1,
		rc_dack_mode : 1,		/* Mac O/S emulation of d-ack */
		rc_dack_toggle : 1,		/* For Mac O/S emulation of d-ack */
		pacing_longer_than_rtt : 1,
		rc_gp_filled : 1;
	uint8_t r_state;	/* Current rack state Lock(a) */
	uint8_t rc_tmr_stopped : 7,
		t_timers_stopped : 1;
	uint8_t rc_enobuf : 7,	/* count of enobufs on connection provides */
		rc_on_min_to : 1;
	uint8_t r_timer_override : 1,	/* hpts override Lock(a) */
		r_is_v6 : 1,	/* V6 pcb Lock(a)  */
		rc_in_persist : 1,
		rc_tlp_in_progress : 1,
		rc_always_pace : 1,	/* Socket option value Lock(a) */
		rc_pace_to_cwnd : 1,
		rc_pace_fill_if_rttin_range : 1,
		xxx_avail_bits : 1;
	uint8_t app_limited_needs_set : 1,
		use_fixed_rate : 1,
		rc_has_collapsed : 1,
		r_rep_attack : 1,
		r_rep_reverse : 1,
		rack_hdrw_pacing : 1,  /* We are doing Hardware pacing */
		rack_hdw_pace_ena : 1, /* Is hardware pacing enabled? */
		rack_attempt_hdwr_pace : 1; /* Did we attempt hdwr pacing (if allowed) */
	uint8_t rack_tlp_threshold_use : 3,	/* only 1, 2 and 3 used so far */
		rack_rec_nonrxt_use_cr : 1,
		rack_enable_scwnd : 1,
		rack_attempted_scwnd : 1,
		rack_no_prr : 1,
		rack_scwnd_is_idle : 1;
	uint8_t rc_allow_data_af_clo: 1,
		delayed_ack : 1,
		set_pacing_done_a_iw : 1,
		use_rack_rr : 1,
		alloc_limit_reported : 1,
		sack_attack_disable : 1,
		do_detection : 1,
		rc_force_max_seg : 1;
	uint8_t rack_cwnd_limited : 1,
		r_early : 1,
		r_late : 1,
		r_running_early : 1,
		r_running_late : 1,
		r_wanted_output: 1,
		r_rr_config : 2;
	uint16_t rc_init_win : 8,
		rc_gp_rtt_set : 1,
		rc_gp_dyn_mul : 1,
		rc_gp_saw_rec : 1,
		rc_gp_saw_ca : 1,
		rc_gp_saw_ss : 1,
		rc_gp_no_rec_chg : 1,
		in_probe_rtt : 1,
		measure_saw_probe_rtt : 1;
	/* Cache line 2 0x40 */
	struct rack_control r_ctl;
}        __aligned(CACHE_LINE_SIZE);

#endif
#endif
