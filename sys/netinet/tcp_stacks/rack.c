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
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_ratelimit.h"
#include "opt_kern_tls.h"
#if defined(INET) || defined(INET6)
#include <sys/param.h>
#include <sys/arb.h>
#include <sys/module.h>
#include <sys/kernel.h>
#ifdef TCP_HHOOK
#include <sys/hhook.h>
#endif
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#ifdef STATS
#include <sys/qmath.h>
#include <sys/tree.h>
#include <sys/stats.h> /* Must come after qmath.h and tree.h */
#else
#include <sys/tree.h>
#endif
#include <sys/refcount.h>
#include <sys/queue.h>
#include <sys/tim_filter.h>
#include <sys/smp.h>
#include <sys/kthread.h>
#include <sys/kern_prefetch.h>
#include <sys/protosw.h>
#ifdef TCP_ACCOUNTING
#include <sys/sched.h>
#include <machine/cpu.h>
#endif
#include <vm/uma.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/vnet.h>

#define TCPSTATES		/* for logging */

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* required for icmp_var.h */
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM */
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet/tcp.h>
#define	TCPOUTFLAGS
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_syncache.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_ratelimit.h>
#include <netinet/tcp_accounting.h>
#include <netinet/tcpip.h>
#include <netinet/cc/cc.h>
#include <netinet/cc/cc_newreno.h>
#include <netinet/tcp_fastopen.h>
#include <netinet/tcp_lro.h>
#ifdef NETFLIX_SHARED_CWND
#include <netinet/tcp_shared_cwnd.h>
#endif
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcp_ecn.h>

#include <netipsec/ipsec_support.h>

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif				/* IPSEC */

#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <machine/in_cksum.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif
#include "sack_filter.h"
#include "tcp_rack.h"
#include "tailq_hash.h"
#include "rack_bbr_common.h"

uma_zone_t rack_zone;
uma_zone_t rack_pcb_zone;

#ifndef TICKS2SBT
#define	TICKS2SBT(__t)	(tick_sbt * ((sbintime_t)(__t)))
#endif

VNET_DECLARE(uint32_t, newreno_beta);
VNET_DECLARE(uint32_t, newreno_beta_ecn);
#define V_newreno_beta VNET(newreno_beta)
#define V_newreno_beta_ecn VNET(newreno_beta_ecn)

#define	M_TCPFSB	__CONCAT(M_TCPFSB, STACKNAME)
#define	M_TCPDO		__CONCAT(M_TCPDO, STACKNAME)

MALLOC_DEFINE(M_TCPFSB, "tcp_fsb_" __XSTRING(STACKNAME), "TCP fast send block");
MALLOC_DEFINE(M_TCPDO, "tcp_do_" __XSTRING(STACKNAME), "TCP deferred options");
MALLOC_DEFINE(M_TCPPCM, "tcp_pcm_" __XSTRING(STACKNAME), "TCP PCM measurement information");

struct sysctl_ctx_list rack_sysctl_ctx;
struct sysctl_oid *rack_sysctl_root;

#define CUM_ACKED 1
#define SACKED 2

/*
 * The RACK module incorporates a number of
 * TCP ideas that have been put out into the IETF
 * over the last few years:
 * - Matt Mathis's Rate Halving which slowly drops
 *    the congestion window so that the ack clock can
 *    be maintained during a recovery.
 * - Yuchung Cheng's RACK TCP (for which its named) that
 *    will stop us using the number of dup acks and instead
 *    use time as the gage of when we retransmit.
 * - Reorder Detection of RFC4737 and the Tail-Loss probe draft
 *    of Dukkipati et.al.
 * RACK depends on SACK, so if an endpoint arrives that
 * cannot do SACK the state machine below will shuttle the
 * connection back to using the "default" TCP stack that is
 * in FreeBSD.
 *
 * To implement RACK the original TCP stack was first decomposed
 * into a functional state machine with individual states
 * for each of the possible TCP connection states. The do_segment
 * functions role in life is to mandate the connection supports SACK
 * initially and then assure that the RACK state matches the conenction
 * state before calling the states do_segment function. Each
 * state is simplified due to the fact that the original do_segment
 * has been decomposed and we *know* what state we are in (no
 * switches on the state) and all tests for SACK are gone. This
 * greatly simplifies what each state does.
 *
 * TCP output is also over-written with a new version since it
 * must maintain the new rack scoreboard.
 *
 */
static int32_t rack_tlp_thresh = 1;
static int32_t rack_tlp_limit = 2;	/* No more than 2 TLPs w-out new data */
static int32_t rack_tlp_use_greater = 1;
static int32_t rack_reorder_thresh = 2;
static int32_t rack_reorder_fade = 60000000;	/* 0 - never fade, def 60,000,000
						 * - 60 seconds */
static uint16_t rack_policer_rxt_thresh= 0;	/* 499 = 49.9%, 0 is off  */
static uint8_t rack_policer_avg_thresh = 0; /* 3.2 */
static uint8_t rack_policer_med_thresh = 0; /* 1 - 16 */
static uint16_t rack_policer_bucket_reserve = 20; /* How much % is reserved in the bucket */
static uint64_t rack_pol_min_bw = 125000;	/* 1mbps in Bytes per sec */
static uint32_t rack_policer_data_thresh = 64000;	/* 64,000 bytes must be sent before we engage */
static uint32_t rack_policing_do_bw_comp = 1;
static uint32_t rack_pcm_every_n_rounds = 100;
static uint32_t rack_pcm_blast = 0;
static uint32_t rack_pcm_is_enabled = 1;
static uint8_t rack_req_del_mss = 18;	/* How many segments need to be sent in a recovery episode to do policer_detection */
static uint8_t rack_ssthresh_rest_rto_rec = 0; /* Do we restore ssthresh when we have rec -> rto -> rec */

static uint32_t rack_gp_gain_req = 1200;		/* Amount percent wise required to gain to record a round has "gaining" */
static uint32_t rack_rnd_cnt_req = 0x10005;		/* Default number of rounds if we are below rack_gp_gain_req where we exit ss */


static int32_t rack_rxt_scoreboard_clear_thresh = 2;
static int32_t rack_dnd_default = 0;		/* For rr_conf = 3, what is the default for dnd */
static int32_t rack_rxt_controls = 0;
static int32_t rack_fill_cw_state = 0;
static uint8_t rack_req_measurements = 1;
/* Attack threshold detections */
static uint32_t rack_highest_sack_thresh_seen = 0;
static uint32_t rack_highest_move_thresh_seen = 0;
static uint32_t rack_merge_out_sacks_on_attack = 0;
static int32_t rack_enable_hw_pacing = 0; /* Due to CCSP keep it off by default */
static int32_t rack_hw_pace_extra_slots = 0;	/* 2 extra MSS time betweens */
static int32_t rack_hw_rate_caps = 0; /* 1; */
static int32_t rack_hw_rate_cap_per = 0;	/* 0 -- off  */
static int32_t rack_hw_rate_min = 0; /* 1500000;*/
static int32_t rack_hw_rate_to_low = 0; /* 1200000; */
static int32_t rack_hw_up_only = 0;
static int32_t rack_stats_gets_ms_rtt = 1;
static int32_t rack_prr_addbackmax = 2;
static int32_t rack_do_hystart = 0;
static int32_t rack_apply_rtt_with_reduced_conf = 0;
static int32_t rack_hibeta_setting = 0;
static int32_t rack_default_pacing_divisor = 250;
static uint16_t rack_pacing_min_seg = 0;
static int32_t rack_timely_off = 0;

static uint32_t sad_seg_size_per = 800;	/* 80.0 % */
static int32_t rack_pkt_delay = 1000;
static int32_t rack_send_a_lot_in_prr = 1;
static int32_t rack_min_to = 1000;	/* Number of microsecond  min timeout */
static int32_t rack_verbose_logging = 0;
static int32_t rack_ignore_data_after_close = 1;
static int32_t rack_enable_shared_cwnd = 1;
static int32_t rack_use_cmp_acks = 1;
static int32_t rack_use_fsb = 1;
static int32_t rack_use_rfo = 1;
static int32_t rack_use_rsm_rfo = 1;
static int32_t rack_max_abc_post_recovery = 2;
static int32_t rack_client_low_buf = 0;
static int32_t rack_dsack_std_based = 0x3;	/* bit field bit 1 sets rc_rack_tmr_std_based and bit 2 sets rc_rack_use_dsack */
static int32_t rack_bw_multipler = 0;		/* Limit on fill cw's jump up to be this x gp_est */
#ifdef TCP_ACCOUNTING
static int32_t rack_tcp_accounting = 0;
#endif
static int32_t rack_limits_scwnd = 1;
static int32_t rack_enable_mqueue_for_nonpaced = 0;
static int32_t rack_hybrid_allow_set_maxseg = 0;
static int32_t rack_disable_prr = 0;
static int32_t use_rack_rr = 1;
static int32_t rack_non_rxt_use_cr = 0; /* does a non-rxt in recovery use the configured rate (ss/ca)? */
static int32_t rack_persist_min = 250000;	/* 250usec */
static int32_t rack_persist_max = 2000000;	/* 2 Second in usec's */
static int32_t rack_honors_hpts_min_to =  1;	/* Do we honor the hpts minimum time out for pacing timers */
static uint32_t rack_max_reduce = 10;		/* Percent we can reduce slot by */
static int32_t rack_sack_not_required = 1;	/* set to one to allow non-sack to use rack */
static int32_t rack_limit_time_with_srtt = 0;
static int32_t rack_autosndbuf_inc = 20;	/* In percentage form */
static int32_t rack_enobuf_hw_boost_mult = 0;	/* How many times the hw rate we boost slot using time_between */
static int32_t rack_enobuf_hw_max = 12000;	/* 12 ms in usecs */
static int32_t rack_enobuf_hw_min = 10000;	/* 10 ms in usecs */
static int32_t rack_hw_rwnd_factor = 2;		/* How many max_segs the rwnd must be before we hold off sending */
static int32_t rack_hw_check_queue = 0;		/* Do we always pre-check queue depth of a hw queue */
static int32_t rack_full_buffer_discount = 10;
/*
 * Currently regular tcp has a rto_min of 30ms
 * the backoff goes 12 times so that ends up
 * being a total of 122.850 seconds before a
 * connection is killed.
 */
static uint32_t rack_def_data_window = 20;
static uint32_t rack_goal_bdp = 2;
static uint32_t rack_min_srtts = 1;
static uint32_t rack_min_measure_usec = 0;
static int32_t rack_tlp_min = 10000;	/* 10ms */
static int32_t rack_rto_min = 30000;	/* 30,000 usec same as main freebsd */
static int32_t rack_rto_max = 4000000;	/* 4 seconds in usec's */
static const int32_t rack_free_cache = 2;
static int32_t rack_hptsi_segments = 40;
static int32_t rack_rate_sample_method = USE_RTT_LOW;
static int32_t rack_pace_every_seg = 0;
static int32_t rack_delayed_ack_time = 40000;	/* 40ms in usecs */
static int32_t rack_slot_reduction = 4;
static int32_t rack_wma_divisor = 8;		/* For WMA calculation */
static int32_t rack_cwnd_block_ends_measure = 0;
static int32_t rack_rwnd_block_ends_measure = 0;
static int32_t rack_def_profile = 0;

static int32_t rack_lower_cwnd_at_tlp = 0;
static int32_t rack_always_send_oldest = 0;
static int32_t rack_tlp_threshold_use = TLP_USE_TWO_ONE;

static uint16_t rack_per_of_gp_ss = 250;	/* 250 % slow-start */
static uint16_t rack_per_of_gp_ca = 200;	/* 200 % congestion-avoidance */
static uint16_t rack_per_of_gp_rec = 200;	/* 200 % of bw */

/* Probertt */
static uint16_t rack_per_of_gp_probertt = 60;	/* 60% of bw */
static uint16_t rack_per_of_gp_lowthresh = 40;	/* 40% is bottom */
static uint16_t rack_per_of_gp_probertt_reduce = 10; /* 10% reduction */
static uint16_t rack_atexit_prtt_hbp = 130;	/* Clamp to 130% on exit prtt if highly buffered path */
static uint16_t rack_atexit_prtt = 130;	/* Clamp to 100% on exit prtt if non highly buffered path */

static uint32_t rack_max_drain_wait = 2;	/* How man gp srtt's before we give up draining */
static uint32_t rack_must_drain = 1;		/* How many GP srtt's we *must* wait */
static uint32_t rack_probertt_use_min_rtt_entry = 1;	/* Use the min to calculate the goal else gp_srtt */
static uint32_t rack_probertt_use_min_rtt_exit = 0;
static uint32_t rack_probe_rtt_sets_cwnd = 0;
static uint32_t rack_probe_rtt_safety_val = 2000000;	/* No more than 2 sec in probe-rtt */
static uint32_t rack_time_between_probertt = 9600000;	/* 9.6 sec in usecs */
static uint32_t rack_probertt_gpsrtt_cnt_mul = 0;	/* How many srtt periods does probe-rtt last top fraction */
static uint32_t rack_probertt_gpsrtt_cnt_div = 0;	/* How many srtt periods does probe-rtt last bottom fraction */
static uint32_t rack_min_probertt_hold = 40000;		/* Equal to delayed ack time */
static uint32_t rack_probertt_filter_life = 10000000;
static uint32_t rack_probertt_lower_within = 10;
static uint32_t rack_min_rtt_movement = 250000;	/* Must move at least 250ms (in microseconds)  to count as a lowering */
static int32_t rack_pace_one_seg = 0;		/* Shall we pace for less than 1.4Meg 1MSS at a time */
static int32_t rack_probertt_clear_is = 1;
static int32_t rack_max_drain_hbp = 1;		/* Extra drain times gpsrtt for highly buffered paths */
static int32_t rack_hbp_thresh = 3;		/* what is the divisor max_rtt/min_rtt to decided a hbp */

/* Part of pacing */
static int32_t rack_max_per_above = 30;		/* When we go to increment stop if above 100+this% */

/* Timely information:
 *
 * Here we have various control parameters on how
 * timely may change the multiplier. rack_gain_p5_ub
 * is associated with timely but not directly influencing
 * the rate decision like the other variables. It controls
 * the way fill-cw interacts with timely and caps how much
 * timely can boost the fill-cw b/w.
 *
 * The other values are various boost/shrink numbers as well
 * as potential caps when adjustments are made to the timely
 * gain (returned by rack_get_output_gain(). Remember too that
 * the gain returned can be overriden by other factors such as
 * probeRTT as well as fixed-rate-pacing.
 */
static int32_t rack_gain_p5_ub = 250;
static int32_t rack_gp_per_bw_mul_up = 2;	/* 2% */
static int32_t rack_gp_per_bw_mul_down = 4;	/* 4% */
static int32_t rack_gp_rtt_maxmul = 3;		/* 3 x maxmin */
static int32_t rack_gp_rtt_minmul = 1;		/* minrtt + (minrtt/mindiv) is lower rtt */
static int32_t rack_gp_rtt_mindiv = 4;		/* minrtt + (minrtt * minmul/mindiv) is lower rtt */
static int32_t rack_gp_decrease_per = 80;	/* Beta value of timely decrease (.8) = 80 */
static int32_t rack_gp_increase_per = 2;	/* 2% increase in multiplier */
static int32_t rack_per_lower_bound = 50;	/* Don't allow to drop below this multiplier */
static int32_t rack_per_upper_bound_ss = 0;	/* Don't allow SS to grow above this */
static int32_t rack_per_upper_bound_ca = 0;	/* Don't allow CA to grow above this */
static int32_t rack_do_dyn_mul = 0;		/* Are the rack gp multipliers dynamic */
static int32_t rack_gp_no_rec_chg = 1;		/* Prohibit recovery from reducing it's multiplier */
static int32_t rack_timely_dec_clear = 6;	/* Do we clear decrement count at a value (6)? */
static int32_t rack_timely_max_push_rise = 3;	/* One round of pushing */
static int32_t rack_timely_max_push_drop = 3;	/* Three round of pushing */
static int32_t rack_timely_min_segs = 4;	/* 4 segment minimum */
static int32_t rack_use_max_for_nobackoff = 0;
static int32_t rack_timely_int_timely_only = 0;	/* do interim timely's only use the timely algo (no b/w changes)? */
static int32_t rack_timely_no_stopping = 0;
static int32_t rack_down_raise_thresh = 100;
static int32_t rack_req_segs = 1;
static uint64_t rack_bw_rate_cap = 0;
static uint64_t rack_fillcw_bw_cap = 3750000;	/* Cap fillcw at 30Mbps */


/* Rack specific counters */
counter_u64_t rack_saw_enobuf;
counter_u64_t rack_saw_enobuf_hw;
counter_u64_t rack_saw_enetunreach;
counter_u64_t rack_persists_sends;
counter_u64_t rack_persists_acks;
counter_u64_t rack_persists_loss;
counter_u64_t rack_persists_lost_ends;
counter_u64_t rack_total_bytes;
#ifdef INVARIANTS
counter_u64_t rack_adjust_map_bw;
#endif
/* Tail loss probe counters */
counter_u64_t rack_tlp_tot;
counter_u64_t rack_tlp_newdata;
counter_u64_t rack_tlp_retran;
counter_u64_t rack_tlp_retran_bytes;
counter_u64_t rack_to_tot;
counter_u64_t rack_hot_alloc;
counter_u64_t tcp_policer_detected;
counter_u64_t rack_to_alloc;
counter_u64_t rack_to_alloc_hard;
counter_u64_t rack_to_alloc_emerg;
counter_u64_t rack_to_alloc_limited;
counter_u64_t rack_alloc_limited_conns;
counter_u64_t rack_split_limited;
counter_u64_t rack_rxt_clamps_cwnd;
counter_u64_t rack_rxt_clamps_cwnd_uniq;

counter_u64_t rack_multi_single_eq;
counter_u64_t rack_proc_non_comp_ack;

counter_u64_t rack_fto_send;
counter_u64_t rack_fto_rsm_send;
counter_u64_t rack_nfto_resend;
counter_u64_t rack_non_fto_send;
counter_u64_t rack_extended_rfo;

counter_u64_t rack_sack_proc_all;
counter_u64_t rack_sack_proc_short;
counter_u64_t rack_sack_proc_restart;
counter_u64_t rack_sack_attacks_detected;
counter_u64_t rack_sack_attacks_reversed;
counter_u64_t rack_sack_attacks_suspect;
counter_u64_t rack_sack_used_next_merge;
counter_u64_t rack_sack_splits;
counter_u64_t rack_sack_used_prev_merge;
counter_u64_t rack_sack_skipped_acked;
counter_u64_t rack_ack_total;
counter_u64_t rack_express_sack;
counter_u64_t rack_sack_total;
counter_u64_t rack_move_none;
counter_u64_t rack_move_some;

counter_u64_t rack_input_idle_reduces;
counter_u64_t rack_collapsed_win;
counter_u64_t rack_collapsed_win_seen;
counter_u64_t rack_collapsed_win_rxt;
counter_u64_t rack_collapsed_win_rxt_bytes;
counter_u64_t rack_try_scwnd;
counter_u64_t rack_hw_pace_init_fail;
counter_u64_t rack_hw_pace_lost;

counter_u64_t rack_out_size[TCP_MSS_ACCT_SIZE];
counter_u64_t rack_opts_arry[RACK_OPTS_SIZE];


#define	RACK_REXMTVAL(tp) max(rack_rto_min, ((tp)->t_srtt + ((tp)->t_rttvar << 2)))

#define	RACK_TCPT_RANGESET(tv, value, tvmin, tvmax, slop) do {	\
	(tv) = (value) + slop;	 \
	if ((u_long)(tv) < (u_long)(tvmin)) \
		(tv) = (tvmin); \
	if ((u_long)(tv) > (u_long)(tvmax)) \
		(tv) = (tvmax); \
} while (0)

static void
rack_log_progress_event(struct tcp_rack *rack, struct tcpcb *tp, uint32_t tick,  int event, int line);

static int
rack_process_ack(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to,
    uint32_t tiwin, int32_t tlen, int32_t * ofia, int32_t thflags, int32_t * ret_val, int32_t orig_tlen);
static int
rack_process_data(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt);
static void
rack_ack_received(struct tcpcb *tp, struct tcp_rack *rack,
   uint32_t th_ack, uint16_t nsegs, uint16_t type, int32_t recovery);
static struct rack_sendmap *rack_alloc(struct tcp_rack *rack);
static struct rack_sendmap *rack_alloc_limit(struct tcp_rack *rack,
    uint8_t limit_type);
static struct rack_sendmap *
rack_check_recovery_mode(struct tcpcb *tp,
    uint32_t tsused);
static uint32_t
rack_grab_rtt(struct tcpcb *tp, struct tcp_rack *rack);
static void
rack_cong_signal(struct tcpcb *tp,
		 uint32_t type, uint32_t ack, int );
static void rack_counter_destroy(void);
static int
rack_ctloutput(struct tcpcb *tp, struct sockopt *sopt);
static int32_t rack_ctor(void *mem, int32_t size, void *arg, int32_t how);
static void
rack_set_pace_segments(struct tcpcb *tp, struct tcp_rack *rack, uint32_t line, uint64_t *fill_override);
static void
rack_do_segment(struct tcpcb *tp, struct mbuf *m, struct tcphdr *th,
    int32_t drop_hdrlen, int32_t tlen, uint8_t iptos);
static void rack_dtor(void *mem, int32_t size, void *arg);
static void
rack_log_alt_to_to_cancel(struct tcp_rack *rack,
    uint32_t flex1, uint32_t flex2,
    uint32_t flex3, uint32_t flex4,
    uint32_t flex5, uint32_t flex6,
    uint16_t flex7, uint8_t mod);

static void
rack_log_pacing_delay_calc(struct tcp_rack *rack, uint32_t len, uint32_t slot,
   uint64_t bw_est, uint64_t bw, uint64_t len_time, int method, int line,
   struct rack_sendmap *rsm, uint8_t quality);
static struct rack_sendmap *
rack_find_high_nonack(struct tcp_rack *rack,
    struct rack_sendmap *rsm);
static struct rack_sendmap *rack_find_lowest_rsm(struct tcp_rack *rack);
static void rack_free(struct tcp_rack *rack, struct rack_sendmap *rsm);
static void rack_fini(struct tcpcb *tp, int32_t tcb_is_purged);
static int rack_get_sockopt(struct tcpcb *tp, struct sockopt *sopt);
static void
rack_do_goodput_measurement(struct tcpcb *tp, struct tcp_rack *rack,
			    tcp_seq th_ack, int line, uint8_t quality);
static void
rack_log_type_pacing_sizes(struct tcpcb *tp, struct tcp_rack *rack, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint8_t frm);

static uint32_t
rack_get_pacing_len(struct tcp_rack *rack, uint64_t bw, uint32_t mss);
static int32_t rack_handoff_ok(struct tcpcb *tp);
static int32_t rack_init(struct tcpcb *tp, void **ptr);
static void rack_init_sysctls(void);

static void
rack_log_ack(struct tcpcb *tp, struct tcpopt *to,
    struct tcphdr *th, int entered_rec, int dup_ack_struck,
    int *dsack_seen, int *sacks_seen);
static void
rack_log_output(struct tcpcb *tp, struct tcpopt *to, int32_t len,
    uint32_t seq_out, uint16_t th_flags, int32_t err, uint64_t ts,
    struct rack_sendmap *hintrsm, uint32_t add_flags, struct mbuf *s_mb, uint32_t s_moff, int hw_tls, int segsiz);

static uint64_t rack_get_gp_est(struct tcp_rack *rack);


static void
rack_log_sack_passed(struct tcpcb *tp, struct tcp_rack *rack,
    struct rack_sendmap *rsm, uint32_t cts);
static void rack_log_to_event(struct tcp_rack *rack, int32_t to_num, struct rack_sendmap *rsm);
static int32_t rack_output(struct tcpcb *tp);

static uint32_t
rack_proc_sack_blk(struct tcpcb *tp, struct tcp_rack *rack,
    struct sackblk *sack, struct tcpopt *to, struct rack_sendmap **prsm,
    uint32_t cts, int *no_extra, int *moved_two, uint32_t segsiz);
static void rack_post_recovery(struct tcpcb *tp, uint32_t th_seq);
static void rack_remxt_tmr(struct tcpcb *tp);
static int rack_set_sockopt(struct tcpcb *tp, struct sockopt *sopt);
static void rack_set_state(struct tcpcb *tp, struct tcp_rack *rack);
static int32_t rack_stopall(struct tcpcb *tp);
static void rack_timer_cancel(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts, int line);
static uint32_t
rack_update_entry(struct tcpcb *tp, struct tcp_rack *rack,
    struct rack_sendmap *rsm, uint64_t ts, int32_t * lenp, uint32_t add_flag, int segsiz);
static void
rack_update_rsm(struct tcpcb *tp, struct tcp_rack *rack,
    struct rack_sendmap *rsm, uint64_t ts, uint32_t add_flag, int segsiz);
static int
rack_update_rtt(struct tcpcb *tp, struct tcp_rack *rack,
    struct rack_sendmap *rsm, struct tcpopt *to, uint32_t cts, int32_t ack_type, tcp_seq th_ack);
static int32_t tcp_addrack(module_t mod, int32_t type, void *data);
static int
rack_do_close_wait(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);

static void
rack_peg_rxt(struct tcp_rack *rack, struct rack_sendmap *rsm, uint32_t segsiz);

static int
rack_do_closing(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);
static int
rack_do_established(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);
static int
rack_do_fastnewdata(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t nxt_pkt, uint8_t iptos);
static int
rack_do_fin_wait_1(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);
static int
rack_do_fin_wait_2(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);
static int
rack_do_lastack(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);
static int
rack_do_syn_recv(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);
static int
rack_do_syn_sent(struct mbuf *m, struct tcphdr *th,
    struct socket *so, struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen,
    int32_t tlen, uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos);
static void rack_chk_req_and_hybrid_on_out(struct tcp_rack *rack, tcp_seq seq, uint32_t len, uint64_t cts);
struct rack_sendmap *
tcp_rack_output(struct tcpcb *tp, struct tcp_rack *rack,
    uint32_t tsused);
static void tcp_rack_xmit_timer(struct tcp_rack *rack, int32_t rtt,
    uint32_t len, uint32_t us_tim, int confidence, struct rack_sendmap *rsm, uint16_t rtrcnt);
static void
     tcp_rack_partialack(struct tcpcb *tp);
static int
rack_set_profile(struct tcp_rack *rack, int prof);
static void
rack_apply_deferred_options(struct tcp_rack *rack);

int32_t rack_clear_counter=0;

static uint64_t
rack_get_lt_bw(struct tcp_rack *rack)
{
	struct timeval tv;
	uint64_t tim, bytes;

	tim = rack->r_ctl.lt_bw_time;
	bytes = rack->r_ctl.lt_bw_bytes;
	if (rack->lt_bw_up) {
		/* Include all the current bytes too */
		microuptime(&tv);
		bytes += (rack->rc_tp->snd_una - rack->r_ctl.lt_seq);
		tim += (tcp_tv_to_lusectick(&tv) - rack->r_ctl.lt_timemark);
	}
	if ((bytes != 0) && (tim != 0))
		return ((bytes * (uint64_t)1000000) / tim);
	else
		return (0);
}

static void
rack_swap_beta_values(struct tcp_rack *rack, uint8_t flex8)
{
	struct sockopt sopt;
	struct cc_newreno_opts opt;
	struct newreno old;
	struct tcpcb *tp;
	int error, failed = 0;

	tp = rack->rc_tp;
	if (tp->t_cc == NULL) {
		/* Tcb is leaving */
		return;
	}
	rack->rc_pacing_cc_set = 1;
	if (strcmp(tp->t_cc->name, CCALGONAME_NEWRENO) != 0) {
		/* Not new-reno we can't play games with beta! */
		failed = 1;
		goto out;

	}
	if (CC_ALGO(tp)->ctl_output == NULL)  {
		/* Huh, not using new-reno so no swaps.? */
		failed = 2;
		goto out;
	}
	/* Get the current values out */
	sopt.sopt_valsize = sizeof(struct cc_newreno_opts);
	sopt.sopt_dir = SOPT_GET;
	opt.name = CC_NEWRENO_BETA;
	error = CC_ALGO(tp)->ctl_output(&tp->t_ccv, &sopt, &opt);
	if (error)  {
		failed = 3;
		goto out;
	}
	old.beta = opt.val;
	opt.name = CC_NEWRENO_BETA_ECN;
	error = CC_ALGO(tp)->ctl_output(&tp->t_ccv, &sopt, &opt);
	if (error)  {
		failed = 4;
		goto out;
	}
	old.beta_ecn = opt.val;

	/* Now lets set in the values we have stored */
	sopt.sopt_dir = SOPT_SET;
	opt.name = CC_NEWRENO_BETA;
	opt.val = rack->r_ctl.rc_saved_beta.beta;
	error = CC_ALGO(tp)->ctl_output(&tp->t_ccv, &sopt, &opt);
	if (error)  {
		failed = 5;
		goto out;
	}
	opt.name = CC_NEWRENO_BETA_ECN;
	opt.val = rack->r_ctl.rc_saved_beta.beta_ecn;
	error = CC_ALGO(tp)->ctl_output(&tp->t_ccv, &sopt, &opt);
	if (error) {
		failed = 6;
		goto out;
	}
	/* Save off the values for restoral */
	memcpy(&rack->r_ctl.rc_saved_beta, &old, sizeof(struct newreno));
out:
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		struct newreno *ptr;

		ptr = ((struct newreno *)tp->t_ccv.cc_data);
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = ptr->beta;
		log.u_bbr.flex2 = ptr->beta_ecn;
		log.u_bbr.flex3 = ptr->newreno_flags;
		log.u_bbr.flex4 = rack->r_ctl.rc_saved_beta.beta;
		log.u_bbr.flex5 = rack->r_ctl.rc_saved_beta.beta_ecn;
		log.u_bbr.flex6 = failed;
		log.u_bbr.flex7 = rack->gp_ready;
		log.u_bbr.flex7 <<= 1;
		log.u_bbr.flex7 |= rack->use_fixed_rate;
		log.u_bbr.flex7 <<= 1;
		log.u_bbr.flex7 |= rack->rc_pacing_cc_set;
		log.u_bbr.pkts_out = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex8 = flex8;
		tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_CWND, error,
			       0, &log, false, NULL, NULL, 0, &tv);
	}
}

static void
rack_set_cc_pacing(struct tcp_rack *rack)
{
	if (rack->rc_pacing_cc_set)
		return;
	/*
	 * Use the swap utility placing in 3 for flex8 to id a
	 * set of a new set of values.
	 */
	rack->rc_pacing_cc_set = 1;
	rack_swap_beta_values(rack, 3);
}

static void
rack_undo_cc_pacing(struct tcp_rack *rack)
{
	if (rack->rc_pacing_cc_set == 0)
		return;
	/*
	 * Use the swap utility placing in 4 for flex8 to id a
	 * restoral of the old values.
	 */
	rack->rc_pacing_cc_set = 0;
	rack_swap_beta_values(rack, 4);
}

static void
rack_remove_pacing(struct tcp_rack *rack)
{
	if (rack->rc_pacing_cc_set)
		rack_undo_cc_pacing(rack);
	if (rack->r_ctl.pacing_method & RACK_REG_PACING)
		tcp_decrement_paced_conn();
	if (rack->r_ctl.pacing_method & RACK_DGP_PACING)
		tcp_dec_dgp_pacing_cnt();
	rack->rc_always_pace = 0;
	rack->r_ctl.pacing_method = RACK_PACING_NONE;
	rack->dgp_on = 0;
	rack->rc_hybrid_mode = 0;
	rack->use_fixed_rate = 0;
}

static void
rack_log_gpset(struct tcp_rack *rack, uint32_t seq_end, uint32_t ack_end_t,
	       uint32_t send_end_t, int line, uint8_t mode, struct rack_sendmap *rsm)
{
	if (tcp_bblogging_on(rack->rc_tp) && (rack_verbose_logging != 0)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = seq_end;
		log.u_bbr.flex2 = rack->rc_tp->gput_seq;
		log.u_bbr.flex3 = ack_end_t;
		log.u_bbr.flex4 = rack->rc_tp->gput_ts;
		log.u_bbr.flex5 = send_end_t;
		log.u_bbr.flex6 = rack->rc_tp->gput_ack;
		log.u_bbr.flex7 = mode;
		log.u_bbr.flex8 = 69;
		log.u_bbr.rttProp = rack->r_ctl.rc_gp_cumack_ts;
		log.u_bbr.delRate = rack->r_ctl.rc_gp_output_ts;
		log.u_bbr.pkts_out = line;
		log.u_bbr.cwnd_gain = rack->app_limited_needs_set;
		log.u_bbr.pkt_epoch = rack->r_ctl.rc_app_limited_cnt;
		log.u_bbr.epoch = rack->r_ctl.current_round;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_considered_lost;
		if (rsm != NULL) {
			log.u_bbr.applimited = rsm->r_start;
			log.u_bbr.delivered = rsm->r_end;
			log.u_bbr.epoch = rsm->r_flags;
		}
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_HPTSI_CALC, 0,
		    0, &log, false, &tv);
	}
}

static int
sysctl_rack_clear(SYSCTL_HANDLER_ARGS)
{
	uint32_t stat;
	int32_t error;

	error = SYSCTL_OUT(req, &rack_clear_counter, sizeof(uint32_t));
	if (error || req->newptr == NULL)
		return error;

	error = SYSCTL_IN(req, &stat, sizeof(uint32_t));
	if (error)
		return (error);
	if (stat == 1) {
#ifdef INVARIANTS
		printf("Clearing RACK counters\n");
#endif
		counter_u64_zero(rack_tlp_tot);
		counter_u64_zero(rack_tlp_newdata);
		counter_u64_zero(rack_tlp_retran);
		counter_u64_zero(rack_tlp_retran_bytes);
		counter_u64_zero(rack_to_tot);
		counter_u64_zero(rack_saw_enobuf);
		counter_u64_zero(rack_saw_enobuf_hw);
		counter_u64_zero(rack_saw_enetunreach);
		counter_u64_zero(rack_persists_sends);
		counter_u64_zero(rack_total_bytes);
		counter_u64_zero(rack_persists_acks);
		counter_u64_zero(rack_persists_loss);
		counter_u64_zero(rack_persists_lost_ends);
#ifdef INVARIANTS
		counter_u64_zero(rack_adjust_map_bw);
#endif
		counter_u64_zero(rack_to_alloc_hard);
		counter_u64_zero(rack_to_alloc_emerg);
		counter_u64_zero(rack_sack_proc_all);
		counter_u64_zero(rack_fto_send);
		counter_u64_zero(rack_fto_rsm_send);
		counter_u64_zero(rack_extended_rfo);
		counter_u64_zero(rack_hw_pace_init_fail);
		counter_u64_zero(rack_hw_pace_lost);
		counter_u64_zero(rack_non_fto_send);
		counter_u64_zero(rack_nfto_resend);
		counter_u64_zero(rack_sack_proc_short);
		counter_u64_zero(rack_sack_proc_restart);
		counter_u64_zero(rack_to_alloc);
		counter_u64_zero(rack_to_alloc_limited);
		counter_u64_zero(rack_alloc_limited_conns);
		counter_u64_zero(rack_split_limited);
		counter_u64_zero(rack_rxt_clamps_cwnd);
		counter_u64_zero(rack_rxt_clamps_cwnd_uniq);
		counter_u64_zero(rack_multi_single_eq);
		counter_u64_zero(rack_proc_non_comp_ack);
		counter_u64_zero(rack_sack_attacks_detected);
		counter_u64_zero(rack_sack_attacks_reversed);
		counter_u64_zero(rack_sack_attacks_suspect);
		counter_u64_zero(rack_sack_used_next_merge);
		counter_u64_zero(rack_sack_used_prev_merge);
		counter_u64_zero(rack_sack_splits);
		counter_u64_zero(rack_sack_skipped_acked);
		counter_u64_zero(rack_ack_total);
		counter_u64_zero(rack_express_sack);
		counter_u64_zero(rack_sack_total);
		counter_u64_zero(rack_move_none);
		counter_u64_zero(rack_move_some);
		counter_u64_zero(rack_try_scwnd);
		counter_u64_zero(rack_collapsed_win);
		counter_u64_zero(rack_collapsed_win_rxt);
		counter_u64_zero(rack_collapsed_win_seen);
		counter_u64_zero(rack_collapsed_win_rxt_bytes);
	} else if (stat == 2) {
#ifdef INVARIANTS
		printf("Clearing RACK option array\n");
#endif
		COUNTER_ARRAY_ZERO(rack_opts_arry, RACK_OPTS_SIZE);
	} else if (stat == 3) {
		printf("Rack has no stats counters to clear (use 1 to clear all stats in sysctl node)\n");
	} else if (stat == 4) {
#ifdef INVARIANTS
		printf("Clearing RACK out size array\n");
#endif
		COUNTER_ARRAY_ZERO(rack_out_size, TCP_MSS_ACCT_SIZE);
	}
	rack_clear_counter = 0;
	return (0);
}

static void
rack_init_sysctls(void)
{
	struct sysctl_oid *rack_counters;
	struct sysctl_oid *rack_attack;
	struct sysctl_oid *rack_pacing;
	struct sysctl_oid *rack_timely;
	struct sysctl_oid *rack_timers;
	struct sysctl_oid *rack_tlp;
	struct sysctl_oid *rack_misc;
	struct sysctl_oid *rack_features;
	struct sysctl_oid *rack_measure;
	struct sysctl_oid *rack_probertt;
	struct sysctl_oid *rack_hw_pacing;
	struct sysctl_oid *rack_policing;

	rack_attack = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "sack_attack",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Rack Sack Attack Counters and Controls");
	rack_counters = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "stats",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Rack Counters");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO, "rate_sample_method", CTLFLAG_RW,
	    &rack_rate_sample_method , USE_RTT_LOW,
	    "What method should we use for rate sampling 0=high, 1=low ");
	/* Probe rtt related controls */
	rack_probertt = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "probertt",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "ProbeRTT related Controls");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "exit_per_hpb", CTLFLAG_RW,
	    &rack_atexit_prtt_hbp, 130,
	    "What percentage above goodput do we clamp CA/SS to at exit on high-BDP path 110%");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "exit_per_nonhpb", CTLFLAG_RW,
	    &rack_atexit_prtt, 130,
	    "What percentage above goodput do we clamp CA/SS to at exit on a non high-BDP path 100%");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "gp_per_mul", CTLFLAG_RW,
	    &rack_per_of_gp_probertt, 60,
	    "What percentage of goodput do we pace at in probertt");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "gp_per_reduce", CTLFLAG_RW,
	    &rack_per_of_gp_probertt_reduce, 10,
	    "What percentage of goodput do we reduce every gp_srtt");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "gp_per_low", CTLFLAG_RW,
	    &rack_per_of_gp_lowthresh, 40,
	    "What percentage of goodput do we allow the multiplier to fall to");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "time_between", CTLFLAG_RW,
	    & rack_time_between_probertt, 96000000,
	    "How many useconds between the lowest rtt falling must past before we enter probertt");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "safety", CTLFLAG_RW,
	    &rack_probe_rtt_safety_val, 2000000,
	    "If not zero, provides a maximum usecond that you can stay in probertt (2sec = 2000000)");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "sets_cwnd", CTLFLAG_RW,
	    &rack_probe_rtt_sets_cwnd, 0,
	    "Do we set the cwnd too (if always_lower is on)");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "maxdrainsrtts", CTLFLAG_RW,
	    &rack_max_drain_wait, 2,
	    "Maximum number of gp_srtt's to hold in drain waiting for flight to reach goal");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "mustdrainsrtts", CTLFLAG_RW,
	    &rack_must_drain, 1,
	    "We must drain this many gp_srtt's waiting for flight to reach goal");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "goal_use_min_entry", CTLFLAG_RW,
	    &rack_probertt_use_min_rtt_entry, 1,
	    "Should we use the min-rtt to calculate the goal rtt (else gp_srtt) at entry");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "goal_use_min_exit", CTLFLAG_RW,
	    &rack_probertt_use_min_rtt_exit, 0,
	    "How to set cwnd at exit, 0 - dynamic, 1 - use min-rtt, 2 - use curgprtt, 3 - entry gp-rtt");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "length_div", CTLFLAG_RW,
	    &rack_probertt_gpsrtt_cnt_div, 0,
	    "How many recent goodput srtt periods plus hold tim does probertt last (bottom of fraction)");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "length_mul", CTLFLAG_RW,
	    &rack_probertt_gpsrtt_cnt_mul, 0,
	    "How many recent goodput srtt periods plus hold tim does probertt last (top of fraction)");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "holdtim_at_target", CTLFLAG_RW,
	    &rack_min_probertt_hold, 200000,
	    "What is the minimum time we hold probertt at target");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "filter_life", CTLFLAG_RW,
	    &rack_probertt_filter_life, 10000000,
	    "What is the time for the filters life in useconds");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "lower_within", CTLFLAG_RW,
	    &rack_probertt_lower_within, 10,
	    "If the rtt goes lower within this percentage of the time, go into probe-rtt");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "must_move", CTLFLAG_RW,
	    &rack_min_rtt_movement, 250,
	    "How much is the minimum movement in rtt to count as a drop for probertt purposes");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "clear_is_cnts", CTLFLAG_RW,
	    &rack_probertt_clear_is, 1,
	    "Do we clear I/S counts on exiting probe-rtt");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "hbp_extra_drain", CTLFLAG_RW,
	    &rack_max_drain_hbp, 1,
	    "How many extra drain gpsrtt's do we get in highly buffered paths");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_probertt),
	    OID_AUTO, "hbp_threshold", CTLFLAG_RW,
	    &rack_hbp_thresh, 3,
	    "We are highly buffered if min_rtt_seen / max_rtt_seen > this-threshold");
	/* Pacing related sysctls */
	rack_pacing = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "pacing",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Pacing related Controls");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "pcm_enabled", CTLFLAG_RW,
	    &rack_pcm_is_enabled, 1,
	    "Do we by default do PCM measurements?");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "pcm_rnds", CTLFLAG_RW,
	    &rack_pcm_every_n_rounds, 100,
	    "How many rounds before we need to do a PCM measurement");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "pcm_blast", CTLFLAG_RW,
	    &rack_pcm_blast, 0,
	    "Blast out the full cwnd/rwnd when doing a PCM measurement");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "rnd_gp_gain", CTLFLAG_RW,
	    &rack_gp_gain_req, 1200,
	    "How much do we have to increase the GP to record the round 1200 = 120.0");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "dgp_out_of_ss_at", CTLFLAG_RW,
	    &rack_rnd_cnt_req, 0x10005,
	    "How many rounds less than rnd_gp_gain will drop us out of SS");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "no_timely", CTLFLAG_RW,
	    &rack_timely_off, 0,
	    "Do we not use timely in DGP?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "fullbufdisc", CTLFLAG_RW,
	    &rack_full_buffer_discount, 10,
	    "What percentage b/w reduction over the GP estimate for a full buffer (default=0 off)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "fillcw", CTLFLAG_RW,
	    &rack_fill_cw_state, 0,
	    "Enable fillcw on new connections (default=0 off)?");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "min_burst", CTLFLAG_RW,
	    &rack_pacing_min_seg, 0,
	    "What is the min burst size for pacing (0 disables)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "divisor", CTLFLAG_RW,
	    &rack_default_pacing_divisor, 250,
	    "What is the default divisor given to the rl code?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "fillcw_max_mult", CTLFLAG_RW,
	    &rack_bw_multipler, 0,
	    "What is the limit multiplier of the current gp_est that fillcw can increase the b/w too, 200 == 200% (0 = off)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "max_pace_over", CTLFLAG_RW,
	    &rack_max_per_above, 30,
	    "What is the maximum allowable percentage that we can pace above (so 30 = 130% of our goal)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "allow1mss", CTLFLAG_RW,
	    &rack_pace_one_seg, 0,
	    "Do we allow low b/w pacing of 1MSS instead of two (1.2Meg and less)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "limit_wsrtt", CTLFLAG_RW,
	    &rack_limit_time_with_srtt, 0,
	    "Do we limit pacing time based on srtt");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "gp_per_ss", CTLFLAG_RW,
	    &rack_per_of_gp_ss, 250,
	    "If non zero, what percentage of goodput to pace at in slow start");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "gp_per_ca", CTLFLAG_RW,
	    &rack_per_of_gp_ca, 150,
	    "If non zero, what percentage of goodput to pace at in congestion avoidance");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "gp_per_rec", CTLFLAG_RW,
	    &rack_per_of_gp_rec, 200,
	    "If non zero, what percentage of goodput to pace at in recovery");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "pace_max_seg", CTLFLAG_RW,
	    &rack_hptsi_segments, 40,
	    "What size is the max for TSO segments in pacing and burst mitigation");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "burst_reduces", CTLFLAG_RW,
	    &rack_slot_reduction, 4,
	    "When doing only burst mitigation what is the reduce divisor");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO, "use_pacing", CTLFLAG_RW,
	    &rack_pace_every_seg, 0,
	    "If set we use pacing, if clear we use only the original burst mitigation");
	SYSCTL_ADD_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "rate_cap", CTLFLAG_RW,
	    &rack_bw_rate_cap, 0,
	    "If set we apply this value to the absolute rate cap used by pacing");
	SYSCTL_ADD_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_pacing),
	    OID_AUTO, "fillcw_cap", CTLFLAG_RW,
	    &rack_fillcw_bw_cap, 3750000,
	    "Do we have an absolute cap on the amount of b/w fillcw can specify (0 = no)?");
	SYSCTL_ADD_U8(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO, "req_measure_cnt", CTLFLAG_RW,
	    &rack_req_measurements, 1,
	    "If doing dynamic pacing, how many measurements must be in before we start pacing?");
	/* Hardware pacing */
	rack_hw_pacing = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "hdwr_pacing",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Pacing related Controls");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "rwnd_factor", CTLFLAG_RW,
	    &rack_hw_rwnd_factor, 2,
	    "How many times does snd_wnd need to be bigger than pace_max_seg so we will hold off and get more acks?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "precheck", CTLFLAG_RW,
	    &rack_hw_check_queue, 0,
	    "Do we always precheck the hdwr pacing queue to avoid ENOBUF's?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "pace_enobuf_mult", CTLFLAG_RW,
	    &rack_enobuf_hw_boost_mult, 0,
	    "By how many time_betweens should we boost the pacing time if we see a ENOBUFS?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "pace_enobuf_max", CTLFLAG_RW,
	    &rack_enobuf_hw_max, 2,
	    "What is the max boost the pacing time if we see a ENOBUFS?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "pace_enobuf_min", CTLFLAG_RW,
	    &rack_enobuf_hw_min, 2,
	    "What is the min boost the pacing time if we see a ENOBUFS?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "enable", CTLFLAG_RW,
	    &rack_enable_hw_pacing, 0,
	    "Should RACK attempt to use hw pacing?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "rate_cap", CTLFLAG_RW,
	    &rack_hw_rate_caps, 0,
	    "Does the highest hardware pacing rate cap the rate we will send at??");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "uncap_per", CTLFLAG_RW,
	    &rack_hw_rate_cap_per, 0,
	    "If you go over b/w by this amount you will be uncapped (0 = never)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "rate_min", CTLFLAG_RW,
	    &rack_hw_rate_min, 0,
	    "Do we need a minimum estimate of this many bytes per second in order to engage hw pacing?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "rate_to_low", CTLFLAG_RW,
	    &rack_hw_rate_to_low, 0,
	    "If we fall below this rate, dis-engage hw pacing?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "up_only", CTLFLAG_RW,
	    &rack_hw_up_only, 0,
	    "Do we allow hw pacing to lower the rate selected?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_hw_pacing),
	    OID_AUTO, "extra_mss_precise", CTLFLAG_RW,
	    &rack_hw_pace_extra_slots, 0,
	    "If the rates between software and hardware match precisely how many extra time_betweens do we get?");
	rack_timely = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "timely",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Rack Timely RTT Controls");
	/* Timely based GP dynmics */
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "upper", CTLFLAG_RW,
	    &rack_gp_per_bw_mul_up, 2,
	    "Rack timely upper range for equal b/w (in percentage)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "lower", CTLFLAG_RW,
	    &rack_gp_per_bw_mul_down, 4,
	    "Rack timely lower range for equal b/w (in percentage)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "rtt_max_mul", CTLFLAG_RW,
	    &rack_gp_rtt_maxmul, 3,
	    "Rack timely multiplier of lowest rtt for rtt_max");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "rtt_min_div", CTLFLAG_RW,
	    &rack_gp_rtt_mindiv, 4,
	    "Rack timely divisor used for rtt + (rtt * mul/divisor) for check for lower rtt");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "rtt_min_mul", CTLFLAG_RW,
	    &rack_gp_rtt_minmul, 1,
	    "Rack timely multiplier used for rtt + (rtt * mul/divisor) for check for lower rtt");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "decrease", CTLFLAG_RW,
	    &rack_gp_decrease_per, 80,
	    "Rack timely Beta value 80 = .8 (scaled by 100)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "increase", CTLFLAG_RW,
	    &rack_gp_increase_per, 2,
	    "Rack timely increase perentage of our GP multiplication factor");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "lowerbound", CTLFLAG_RW,
	    &rack_per_lower_bound, 50,
	    "Rack timely lowest percentage we allow GP multiplier to fall to");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "p5_upper", CTLFLAG_RW,
	    &rack_gain_p5_ub, 250,
	    "Profile 5 upper bound to timely gain");

	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "upperboundss", CTLFLAG_RW,
	    &rack_per_upper_bound_ss, 0,
	    "Rack timely highest percentage we allow GP multiplier in SS to raise to (0 is no upperbound)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "upperboundca", CTLFLAG_RW,
	    &rack_per_upper_bound_ca, 0,
	    "Rack timely highest percentage we allow GP multiplier to CA raise to (0 is no upperbound)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "dynamicgp", CTLFLAG_RW,
	    &rack_do_dyn_mul, 0,
	    "Rack timely do we enable dynmaic timely goodput by default");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "no_rec_red", CTLFLAG_RW,
	    &rack_gp_no_rec_chg, 1,
	    "Rack timely do we prohibit the recovery multiplier from being lowered");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "red_clear_cnt", CTLFLAG_RW,
	    &rack_timely_dec_clear, 6,
	    "Rack timely what threshold do we count to before another boost during b/w decent");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "max_push_rise", CTLFLAG_RW,
	    &rack_timely_max_push_rise, 3,
	    "Rack timely how many times do we push up with b/w increase");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "max_push_drop", CTLFLAG_RW,
	    &rack_timely_max_push_drop, 3,
	    "Rack timely how many times do we push back on b/w decent");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "min_segs", CTLFLAG_RW,
	    &rack_timely_min_segs, 4,
	    "Rack timely when setting the cwnd what is the min num segments");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "noback_max", CTLFLAG_RW,
	    &rack_use_max_for_nobackoff, 0,
	    "Rack timely when deciding if to backoff on a loss, do we use under max rtt else min");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "interim_timely_only", CTLFLAG_RW,
	    &rack_timely_int_timely_only, 0,
	    "Rack timely when doing interim timely's do we only do timely (no b/w consideration)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "nonstop", CTLFLAG_RW,
	    &rack_timely_no_stopping, 0,
	    "Rack timely don't stop increase");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "dec_raise_thresh", CTLFLAG_RW,
	    &rack_down_raise_thresh, 100,
	    "If the CA or SS is below this threshold raise on the first 3 b/w lowers (0=always)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timely),
	    OID_AUTO, "bottom_drag_segs", CTLFLAG_RW,
	    &rack_req_segs, 1,
	    "Bottom dragging if not these many segments outstanding and room");

	/* TLP and Rack related parameters */
	rack_tlp = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "tlp",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "TLP and Rack related Controls");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "use_rrr", CTLFLAG_RW,
	    &use_rack_rr, 1,
	    "Do we use Rack Rapid Recovery");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "post_rec_labc", CTLFLAG_RW,
	    &rack_max_abc_post_recovery, 2,
	    "Since we do early recovery, do we override the l_abc to a value, if so what?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "nonrxt_use_cr", CTLFLAG_RW,
	    &rack_non_rxt_use_cr, 0,
	    "Do we use ss/ca rate if in recovery we are transmitting a new data chunk");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "tlpmethod", CTLFLAG_RW,
	    &rack_tlp_threshold_use, TLP_USE_TWO_ONE,
	    "What method do we do for TLP time calc 0=no-de-ack-comp, 1=ID, 2=2.1, 3=2.2");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "limit", CTLFLAG_RW,
	    &rack_tlp_limit, 2,
	    "How many TLP's can be sent without sending new data");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "use_greater", CTLFLAG_RW,
	    &rack_tlp_use_greater, 1,
	    "Should we use the rack_rtt time if its greater than srtt");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "tlpminto", CTLFLAG_RW,
	    &rack_tlp_min, 10000,
	    "TLP minimum timeout per the specification (in microseconds)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "send_oldest", CTLFLAG_RW,
	    &rack_always_send_oldest, 0,
	    "Should we always send the oldest TLP and RACK-TLP");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "tlp_cwnd_flag", CTLFLAG_RW,
	    &rack_lower_cwnd_at_tlp, 0,
	    "When a TLP completes a retran should we enter recovery");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "reorder_thresh", CTLFLAG_RW,
	    &rack_reorder_thresh, 2,
	    "What factor for rack will be added when seeing reordering (shift right)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "rtt_tlp_thresh", CTLFLAG_RW,
	    &rack_tlp_thresh, 1,
	    "What divisor for TLP rtt/retran will be added (1=rtt, 2=1/2 rtt etc)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "reorder_fade", CTLFLAG_RW,
	    &rack_reorder_fade, 60000000,
	    "Does reorder detection fade, if so how many microseconds (0 means never)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_tlp),
	    OID_AUTO, "pktdelay", CTLFLAG_RW,
	    &rack_pkt_delay, 1000,
	    "Extra RACK time (in microseconds) besides reordering thresh");

	/* Timer related controls */
	rack_timers = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "timers",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Timer related controls");
	SYSCTL_ADD_U8(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "reset_ssth_rec_rto", CTLFLAG_RW,
	    &rack_ssthresh_rest_rto_rec, 0,
	    "When doing recovery -> rto -> recovery do we reset SSthresh?");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "scoreboard_thresh", CTLFLAG_RW,
	    &rack_rxt_scoreboard_clear_thresh, 2,
	    "How many RTO's are allowed before we clear the scoreboard");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "honor_hpts_min", CTLFLAG_RW,
	    &rack_honors_hpts_min_to, 1,
	    "Do rack pacing timers honor hpts min timeout");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "hpts_max_reduce", CTLFLAG_RW,
	    &rack_max_reduce, 10,
	    "Max percentage we will reduce slot by for pacing when we are behind");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "persmin", CTLFLAG_RW,
	    &rack_persist_min, 250000,
	    "What is the minimum time in microseconds between persists");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "persmax", CTLFLAG_RW,
	    &rack_persist_max, 2000000,
	    "What is the largest delay in microseconds between persists");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "delayed_ack", CTLFLAG_RW,
	    &rack_delayed_ack_time, 40000,
	    "Delayed ack time (40ms in microseconds)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "minrto", CTLFLAG_RW,
	    &rack_rto_min, 30000,
	    "Minimum RTO in microseconds -- set with caution below 1000 due to TLP");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "maxrto", CTLFLAG_RW,
	    &rack_rto_max, 4000000,
	    "Maximum RTO in microseconds -- should be at least as large as min_rto");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_timers),
	    OID_AUTO, "minto", CTLFLAG_RW,
	    &rack_min_to, 1000,
	    "Minimum rack timeout in microseconds");
	/* Measure controls */
	rack_measure = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "measure",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Measure related controls");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_measure),
	    OID_AUTO, "wma_divisor", CTLFLAG_RW,
	    &rack_wma_divisor, 8,
	    "When doing b/w calculation what is the  divisor for the WMA");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_measure),
	    OID_AUTO, "end_cwnd", CTLFLAG_RW,
	    &rack_cwnd_block_ends_measure, 0,
	    "Does a cwnd just-return end the measurement window (app limited)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_measure),
	    OID_AUTO, "end_rwnd", CTLFLAG_RW,
	    &rack_rwnd_block_ends_measure, 0,
	    "Does an rwnd just-return end the measurement window (app limited -- not persists)");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_measure),
	    OID_AUTO, "min_target", CTLFLAG_RW,
	    &rack_def_data_window, 20,
	    "What is the minimum target window (in mss) for a GP measurements");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_measure),
	    OID_AUTO, "goal_bdp", CTLFLAG_RW,
	    &rack_goal_bdp, 2,
	    "What is the goal BDP to measure");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_measure),
	    OID_AUTO, "min_srtts", CTLFLAG_RW,
	    &rack_min_srtts, 1,
	    "What is the goal BDP to measure");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_measure),
	    OID_AUTO, "min_measure_tim", CTLFLAG_RW,
	    &rack_min_measure_usec, 0,
	    "What is the Minimum time time for a measurement if 0, this is off");
	/* Features */
	rack_features = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "features",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Feature controls");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_features),
	    OID_AUTO, "hybrid_set_maxseg", CTLFLAG_RW,
	    &rack_hybrid_allow_set_maxseg, 0,
	    "Should hybrid pacing allow the setmss command");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_features),
	    OID_AUTO, "cmpack", CTLFLAG_RW,
	    &rack_use_cmp_acks, 1,
	    "Should RACK have LRO send compressed acks");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_features),
	    OID_AUTO, "fsb", CTLFLAG_RW,
	    &rack_use_fsb, 1,
	    "Should RACK use the fast send block?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_features),
	    OID_AUTO, "rfo", CTLFLAG_RW,
	    &rack_use_rfo, 1,
	    "Should RACK use rack_fast_output()?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_features),
	    OID_AUTO, "rsmrfo", CTLFLAG_RW,
	    &rack_use_rsm_rfo, 1,
	    "Should RACK use rack_fast_rsm_output()?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_features),
	    OID_AUTO, "non_paced_lro_queue", CTLFLAG_RW,
	    &rack_enable_mqueue_for_nonpaced, 0,
	    "Should RACK use mbuf queuing for non-paced connections");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_features),
	    OID_AUTO, "hystartplusplus", CTLFLAG_RW,
	    &rack_do_hystart, 0,
	    "Should RACK enable HyStart++ on connections?");
	/* Policer detection */
	rack_policing = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "policing",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "policer detection");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "rxt_thresh", CTLFLAG_RW,
	    &rack_policer_rxt_thresh, 0,
	   "Percentage of retransmits we need to be a possible policer (499 = 49.9 percent)");
	SYSCTL_ADD_U8(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "avg_thresh", CTLFLAG_RW,
	    &rack_policer_avg_thresh, 0,
	    "What threshold of average retransmits needed to recover a lost packet (1 - 169 aka 21 = 2.1)?");
	SYSCTL_ADD_U8(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "med_thresh", CTLFLAG_RW,
	    &rack_policer_med_thresh, 0,
	    "What threshold of Median retransmits needed to recover a lost packet (1 - 16)?");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "data_thresh", CTLFLAG_RW,
	    &rack_policer_data_thresh, 64000,
	    "How many bytes must have gotten through before we can start doing policer detection?");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "bwcomp", CTLFLAG_RW,
	    &rack_policing_do_bw_comp, 1,
	    "Do we raise up low b/w so that at least pace_max_seg can be sent in the srtt?");
	SYSCTL_ADD_U8(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "recmss", CTLFLAG_RW,
	    &rack_req_del_mss, 18,
	    "How many MSS must be delivered during recovery to engage policer detection?");
	SYSCTL_ADD_U16(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "res_div", CTLFLAG_RW,
	    &rack_policer_bucket_reserve, 20,
	    "What percentage is reserved in the policer bucket?");
	SYSCTL_ADD_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_policing),
	    OID_AUTO, "min_comp_bw", CTLFLAG_RW,
	    &rack_pol_min_bw, 125000,
	    "Do we have a min b/w for b/w compensation (0 = no)?");
	/* Misc rack controls */
	rack_misc = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO,
	    "misc",
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "Misc related controls");
#ifdef TCP_ACCOUNTING
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "tcp_acct", CTLFLAG_RW,
	    &rack_tcp_accounting, 0,
	    "Should we turn on TCP accounting for all rack sessions?");
#endif
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "dnd", CTLFLAG_RW,
	    &rack_dnd_default, 0,
	    "Do not disturb default for rack_rrr = 3");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "sad_seg_per", CTLFLAG_RW,
	    &sad_seg_size_per, 800,
	    "Percentage of segment size needed in a sack 800 = 80.0?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "rxt_controls", CTLFLAG_RW,
	    &rack_rxt_controls, 0,
	    "Retransmit sending size controls (valid  values 0, 1, 2 default=1)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "rack_hibeta", CTLFLAG_RW,
	    &rack_hibeta_setting, 0,
	    "Do we ue a high beta (80 instead of 50)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "apply_rtt_with_low_conf", CTLFLAG_RW,
	    &rack_apply_rtt_with_reduced_conf, 0,
	    "When a persist or keep-alive probe is not answered do we calculate rtt on subsequent answers?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "rack_dsack_ctl", CTLFLAG_RW,
	    &rack_dsack_std_based, 3,
	    "How do we process dsack with respect to rack timers, bit field, 3 is standards based?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "prr_addback_max", CTLFLAG_RW,
	    &rack_prr_addbackmax, 2,
	    "What is the maximum number of MSS we allow to be added back if prr can't send all its data?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "stats_gets_ms", CTLFLAG_RW,
	    &rack_stats_gets_ms_rtt, 1,
	    "What do we feed the stats framework (1 = ms_rtt, 0 = us_rtt, 2 = ms_rtt from hdwr, > 2 usec rtt from hdwr)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "clientlowbuf", CTLFLAG_RW,
	    &rack_client_low_buf, 0,
	    "Client low buffer level (below this we are more aggressive in DGP exiting recovery (0 = off)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "defprofile", CTLFLAG_RW,
	    &rack_def_profile, 0,
	    "Should RACK use a default profile (0=no, num == profile num)?");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "shared_cwnd", CTLFLAG_RW,
	    &rack_enable_shared_cwnd, 1,
	    "Should RACK try to use the shared cwnd on connections where allowed");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "limits_on_scwnd", CTLFLAG_RW,
	    &rack_limits_scwnd, 1,
	    "Should RACK place low end time limits on the shared cwnd feature");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "no_prr", CTLFLAG_RW,
	    &rack_disable_prr, 0,
	    "Should RACK not use prr and only pace (must have pacing on)");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "bb_verbose", CTLFLAG_RW,
	    &rack_verbose_logging, 0,
	    "Should RACK black box logging be verbose");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "data_after_close", CTLFLAG_RW,
	    &rack_ignore_data_after_close, 1,
	    "Do we hold off sending a RST until all pending data is ack'd");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "no_sack_needed", CTLFLAG_RW,
	    &rack_sack_not_required, 1,
	    "Do we allow rack to run on connections not supporting SACK");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "prr_sendalot", CTLFLAG_RW,
	    &rack_send_a_lot_in_prr, 1,
	    "Send a lot in prr");
	SYSCTL_ADD_S32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_misc),
	    OID_AUTO, "autoscale", CTLFLAG_RW,
	    &rack_autosndbuf_inc, 20,
	    "What percentage should rack scale up its snd buffer by?");


	/* Sack Attacker detection stuff */
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "merge_out", CTLFLAG_RW,
	    &rack_merge_out_sacks_on_attack, 0,
	    "Do we merge the sendmap when we decide we are being attacked?");

	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "detect_highsackratio", CTLFLAG_RW,
	    &rack_highest_sack_thresh_seen, 0,
	    "Highest sack to ack ratio seen");
	SYSCTL_ADD_U32(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "detect_highmoveratio", CTLFLAG_RW,
	    &rack_highest_move_thresh_seen, 0,
	    "Highest move to non-move ratio seen");
	rack_ack_total = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "acktotal", CTLFLAG_RD,
	    &rack_ack_total,
	    "Total number of Ack's");
	rack_express_sack = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "exp_sacktotal", CTLFLAG_RD,
	    &rack_express_sack,
	    "Total expresss number of Sack's");
	rack_sack_total = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "sacktotal", CTLFLAG_RD,
	    &rack_sack_total,
	    "Total number of SACKs");
	rack_move_none = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "move_none", CTLFLAG_RD,
	    &rack_move_none,
	    "Total number of SACK index reuse of positions under threshold");
	rack_move_some = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "move_some", CTLFLAG_RD,
	    &rack_move_some,
	    "Total number of SACK index reuse of positions over threshold");
	rack_sack_attacks_detected = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "attacks", CTLFLAG_RD,
	    &rack_sack_attacks_detected,
	    "Total number of SACK attackers that had sack disabled");
	rack_sack_attacks_reversed = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "reversed", CTLFLAG_RD,
	    &rack_sack_attacks_reversed,
	    "Total number of SACK attackers that were later determined false positive");
	rack_sack_attacks_suspect = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "suspect", CTLFLAG_RD,
	    &rack_sack_attacks_suspect,
	    "Total number of SACKs that triggered early detection");

	rack_sack_used_next_merge = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "nextmerge", CTLFLAG_RD,
	    &rack_sack_used_next_merge,
	    "Total number of times we used the next merge");
	rack_sack_used_prev_merge = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "prevmerge", CTLFLAG_RD,
	    &rack_sack_used_prev_merge,
	    "Total number of times we used the prev merge");
	/* Counters */
	rack_total_bytes = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "totalbytes", CTLFLAG_RD,
	    &rack_total_bytes,
	    "Total number of bytes sent");
	rack_fto_send = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "fto_send", CTLFLAG_RD,
	    &rack_fto_send, "Total number of rack_fast_output sends");
	rack_fto_rsm_send = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "fto_rsm_send", CTLFLAG_RD,
	    &rack_fto_rsm_send, "Total number of rack_fast_rsm_output sends");
	rack_nfto_resend = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "nfto_resend", CTLFLAG_RD,
	    &rack_nfto_resend, "Total number of rack_output retransmissions");
	rack_non_fto_send = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "nfto_send", CTLFLAG_RD,
	    &rack_non_fto_send, "Total number of rack_output first sends");
	rack_extended_rfo = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "rfo_extended", CTLFLAG_RD,
	    &rack_extended_rfo, "Total number of times we extended rfo");

	rack_hw_pace_init_fail = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "hwpace_init_fail", CTLFLAG_RD,
	    &rack_hw_pace_init_fail, "Total number of times we failed to initialize hw pacing");
	rack_hw_pace_lost = counter_u64_alloc(M_WAITOK);

	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "hwpace_lost", CTLFLAG_RD,
	    &rack_hw_pace_lost, "Total number of times we failed to initialize hw pacing");
	rack_tlp_tot = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "tlp_to_total", CTLFLAG_RD,
	    &rack_tlp_tot,
	    "Total number of tail loss probe expirations");
	rack_tlp_newdata = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "tlp_new", CTLFLAG_RD,
	    &rack_tlp_newdata,
	    "Total number of tail loss probe sending new data");
	rack_tlp_retran = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "tlp_retran", CTLFLAG_RD,
	    &rack_tlp_retran,
	    "Total number of tail loss probe sending retransmitted data");
	rack_tlp_retran_bytes = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "tlp_retran_bytes", CTLFLAG_RD,
	    &rack_tlp_retran_bytes,
	    "Total bytes of tail loss probe sending retransmitted data");
	rack_to_tot = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "rack_to_tot", CTLFLAG_RD,
	    &rack_to_tot,
	    "Total number of times the rack to expired");
	rack_saw_enobuf = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "saw_enobufs", CTLFLAG_RD,
	    &rack_saw_enobuf,
	    "Total number of times a sends returned enobuf for non-hdwr paced connections");
	rack_saw_enobuf_hw = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "saw_enobufs_hw", CTLFLAG_RD,
	    &rack_saw_enobuf_hw,
	    "Total number of times a send returned enobuf for hdwr paced connections");
	rack_saw_enetunreach = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "saw_enetunreach", CTLFLAG_RD,
	    &rack_saw_enetunreach,
	    "Total number of times a send received a enetunreachable");
	rack_hot_alloc = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "alloc_hot", CTLFLAG_RD,
	    &rack_hot_alloc,
	    "Total allocations from the top of our list");
	tcp_policer_detected = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "policer_detected", CTLFLAG_RD,
	    &tcp_policer_detected,
	    "Total policer_detections");

	rack_to_alloc = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "allocs", CTLFLAG_RD,
	    &rack_to_alloc,
	    "Total allocations of tracking structures");
	rack_to_alloc_hard = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "allochard", CTLFLAG_RD,
	    &rack_to_alloc_hard,
	    "Total allocations done with sleeping the hard way");
	rack_to_alloc_emerg = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "allocemerg", CTLFLAG_RD,
	    &rack_to_alloc_emerg,
	    "Total allocations done from emergency cache");
	rack_to_alloc_limited = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "alloc_limited", CTLFLAG_RD,
	    &rack_to_alloc_limited,
	    "Total allocations dropped due to limit");
	rack_alloc_limited_conns = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "alloc_limited_conns", CTLFLAG_RD,
	    &rack_alloc_limited_conns,
	    "Connections with allocations dropped due to limit");
	rack_split_limited = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "split_limited", CTLFLAG_RD,
	    &rack_split_limited,
	    "Split allocations dropped due to limit");
	rack_rxt_clamps_cwnd = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "rxt_clamps_cwnd", CTLFLAG_RD,
	    &rack_rxt_clamps_cwnd,
	    "Number of times that excessive rxt clamped the cwnd down");
	rack_rxt_clamps_cwnd_uniq = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "rxt_clamps_cwnd_uniq", CTLFLAG_RD,
	    &rack_rxt_clamps_cwnd_uniq,
	    "Number of connections that have had excessive rxt clamped the cwnd down");
	rack_persists_sends = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "persist_sends", CTLFLAG_RD,
	    &rack_persists_sends,
	    "Number of times we sent a persist probe");
	rack_persists_acks = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "persist_acks", CTLFLAG_RD,
	    &rack_persists_acks,
	    "Number of times a persist probe was acked");
	rack_persists_loss = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "persist_loss", CTLFLAG_RD,
	    &rack_persists_loss,
	    "Number of times we detected a lost persist probe (no ack)");
	rack_persists_lost_ends = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "persist_loss_ends", CTLFLAG_RD,
	    &rack_persists_lost_ends,
	    "Number of lost persist probe (no ack) that the run ended with a PERSIST abort");
#ifdef INVARIANTS
	rack_adjust_map_bw = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "map_adjust_req", CTLFLAG_RD,
	    &rack_adjust_map_bw,
	    "Number of times we hit the case where the sb went up and down on a sendmap entry");
#endif
	rack_multi_single_eq = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "cmp_ack_equiv", CTLFLAG_RD,
	    &rack_multi_single_eq,
	    "Number of compressed acks total represented");
	rack_proc_non_comp_ack = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "cmp_ack_not", CTLFLAG_RD,
	    &rack_proc_non_comp_ack,
	    "Number of non compresseds acks that we processed");


	rack_sack_proc_all = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "sack_long", CTLFLAG_RD,
	    &rack_sack_proc_all,
	    "Total times we had to walk whole list for sack processing");
	rack_sack_proc_restart = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "sack_restart", CTLFLAG_RD,
	    &rack_sack_proc_restart,
	    "Total times we had to walk whole list due to a restart");
	rack_sack_proc_short = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "sack_short", CTLFLAG_RD,
	    &rack_sack_proc_short,
	    "Total times we took shortcut for sack processing");
	rack_sack_skipped_acked = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "skipacked", CTLFLAG_RD,
	    &rack_sack_skipped_acked,
	    "Total number of times we skipped previously sacked");
	rack_sack_splits = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_attack),
	    OID_AUTO, "ofsplit", CTLFLAG_RD,
	    &rack_sack_splits,
	    "Total number of times we did the old fashion tree split");
	rack_input_idle_reduces = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "idle_reduce_oninput", CTLFLAG_RD,
	    &rack_input_idle_reduces,
	    "Total number of idle reductions on input");
	rack_collapsed_win_seen = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "collapsed_win_seen", CTLFLAG_RD,
	    &rack_collapsed_win_seen,
	    "Total number of collapsed window events seen (where our window shrinks)");

	rack_collapsed_win = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "collapsed_win", CTLFLAG_RD,
	    &rack_collapsed_win,
	    "Total number of collapsed window events where we mark packets");
	rack_collapsed_win_rxt = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "collapsed_win_rxt", CTLFLAG_RD,
	    &rack_collapsed_win_rxt,
	    "Total number of packets that were retransmitted");
	rack_collapsed_win_rxt_bytes = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "collapsed_win_bytes", CTLFLAG_RD,
	    &rack_collapsed_win_rxt_bytes,
	    "Total number of bytes that were retransmitted");
	rack_try_scwnd = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_counters),
	    OID_AUTO, "tried_scwnd", CTLFLAG_RD,
	    &rack_try_scwnd,
	    "Total number of scwnd attempts");
	COUNTER_ARRAY_ALLOC(rack_out_size, TCP_MSS_ACCT_SIZE, M_WAITOK);
	SYSCTL_ADD_COUNTER_U64_ARRAY(&rack_sysctl_ctx, SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO, "outsize", CTLFLAG_RD,
	    rack_out_size, TCP_MSS_ACCT_SIZE, "MSS send sizes");
	COUNTER_ARRAY_ALLOC(rack_opts_arry, RACK_OPTS_SIZE, M_WAITOK);
	SYSCTL_ADD_COUNTER_U64_ARRAY(&rack_sysctl_ctx, SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO, "opts", CTLFLAG_RD,
	    rack_opts_arry, RACK_OPTS_SIZE, "RACK Option Stats");
	SYSCTL_ADD_PROC(&rack_sysctl_ctx,
	    SYSCTL_CHILDREN(rack_sysctl_root),
	    OID_AUTO, "clear", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &rack_clear_counter, 0, sysctl_rack_clear, "IU", "Clear counters");
}

static uint32_t
rc_init_window(struct tcp_rack *rack)
{
	return (tcp_compute_initwnd(tcp_maxseg(rack->rc_tp)));

}

static uint64_t
rack_get_fixed_pacing_bw(struct tcp_rack *rack)
{
	if (IN_FASTRECOVERY(rack->rc_tp->t_flags))
		return (rack->r_ctl.rc_fixed_pacing_rate_rec);
	else if (rack->r_ctl.cwnd_to_use < rack->rc_tp->snd_ssthresh)
		return (rack->r_ctl.rc_fixed_pacing_rate_ss);
	else
		return (rack->r_ctl.rc_fixed_pacing_rate_ca);
}

static void
rack_log_hybrid_bw(struct tcp_rack *rack, uint32_t seq, uint64_t cbw, uint64_t tim,
	uint64_t data, uint8_t mod, uint16_t aux,
	struct tcp_sendfile_track *cur, int line)
{
#ifdef TCP_REQUEST_TRK
	int do_log = 0;

	/*
	 * The rate cap one is noisy and only should come out when normal BB logging
	 * is enabled, the other logs (not RATE_CAP and NOT CAP_CALC) only come out
	 * once per chunk and make up the BBpoint that can be turned on by the client.
	 */
	if ((mod == HYBRID_LOG_RATE_CAP) || (mod == HYBRID_LOG_CAP_CALC)) {
		/*
		 * The very noisy two need to only come out when
		 * we have verbose logging on.
		 */
		if (rack_verbose_logging != 0)
			do_log = tcp_bblogging_on(rack->rc_tp);
		else
			do_log = 0;
	} else if (mod != HYBRID_LOG_BW_MEASURE) {
		/*
		 * All other less noisy logs here except the measure which
		 * also needs to come out on the point and the log.
		 */
		do_log = tcp_bblogging_on(rack->rc_tp);
	} else {
		do_log = tcp_bblogging_point_on(rack->rc_tp, TCP_BBPOINT_REQ_LEVEL_LOGGING);
	}

	if (do_log) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		uint64_t lt_bw;

		/* Convert our ms to a microsecond */
		memset(&log, 0, sizeof(log));

		log.u_bbr.cwnd_gain = line;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.rttProp = tim;
		log.u_bbr.bw_inuse = cbw;
		log.u_bbr.delRate = rack_get_gp_est(rack);
		lt_bw = rack_get_lt_bw(rack);
		log.u_bbr.flex1 = seq;
		log.u_bbr.pacing_gain = aux;
		/* lt_bw = < flex3 | flex2 > */
		log.u_bbr.flex2 = (uint32_t)(lt_bw & 0x00000000ffffffff);
		log.u_bbr.flex3 = (uint32_t)((lt_bw >> 32) & 0x00000000ffffffff);
		/* Record the last obtained us rtt in inflight */
		if (cur == NULL) {
			/* Make sure we are looking at the right log if an overide comes in */
			cur = rack->r_ctl.rc_last_sft;
		}
		if (rack->r_ctl.rack_rs.rs_flags != RACK_RTT_EMPTY)
			log.u_bbr.inflight = rack->r_ctl.rack_rs.rs_us_rtt;
		else {
			/* Use the last known rtt i.e. the rack-rtt */
			log.u_bbr.inflight = rack->rc_rack_rtt;
		}
		if (cur != NULL) {
			uint64_t off;

			log.u_bbr.cur_del_rate = cur->deadline;
			if ((mod == HYBRID_LOG_RATE_CAP) || (mod == HYBRID_LOG_CAP_CALC)) {
				/* start = < lost | pkt_epoch > */
				log.u_bbr.pkt_epoch = (uint32_t)(cur->start & 0x00000000ffffffff);
				log.u_bbr.lost = (uint32_t)((cur->start >> 32) & 0x00000000ffffffff);
				log.u_bbr.flex6 = cur->start_seq;
				log.u_bbr.pkts_out = cur->end_seq;
			} else {
				/* start = < lost | pkt_epoch > */
				log.u_bbr.pkt_epoch = (uint32_t)(cur->start & 0x00000000ffffffff);
				log.u_bbr.lost = (uint32_t)((cur->start >> 32) & 0x00000000ffffffff);
				/* end = < pkts_out | flex6 > */
				log.u_bbr.flex6 = (uint32_t)(cur->end & 0x00000000ffffffff);
				log.u_bbr.pkts_out = (uint32_t)((cur->end >> 32) & 0x00000000ffffffff);
			}
			/* first_send = <lt_epoch | epoch> */
			log.u_bbr.epoch = (uint32_t)(cur->first_send & 0x00000000ffffffff);
			log.u_bbr.lt_epoch = (uint32_t)((cur->first_send >> 32) & 0x00000000ffffffff);
			/* localtime = <delivered | applimited>*/
			log.u_bbr.applimited = (uint32_t)(cur->localtime & 0x00000000ffffffff);
			log.u_bbr.delivered = (uint32_t)((cur->localtime >> 32) & 0x00000000ffffffff);
#ifdef TCP_REQUEST_TRK
			off = (uint64_t)(cur) - (uint64_t)(&rack->rc_tp->t_tcpreq_info[0]);
			log.u_bbr.bbr_substate = (uint8_t)(off / sizeof(struct tcp_sendfile_track));
#endif
			log.u_bbr.inhpts = 1;
			log.u_bbr.flex4 = (uint32_t)(rack->rc_tp->t_sndbytes - cur->sent_at_fs);
			log.u_bbr.flex5 = (uint32_t)(rack->rc_tp->t_snd_rxt_bytes - cur->rxt_at_fs);
			log.u_bbr.flex7 = (uint16_t)cur->hybrid_flags;
		} else {
			log.u_bbr.flex7 = 0xffff;
			log.u_bbr.cur_del_rate = 0xffffffffffffffff;
		}
		/*
		 * Compose bbr_state to be a bit wise 0000ADHF
		 * where A is the always_pace flag
		 * where D is the dgp_on flag
		 * where H is the hybrid_mode on flag
		 * where F is the use_fixed_rate flag.
		 */
		log.u_bbr.bbr_state = rack->rc_always_pace;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->dgp_on;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->rc_hybrid_mode;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->use_fixed_rate;
		log.u_bbr.flex8 = mod;
		tcp_log_event(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_HYBRID_PACING_LOG, 0,
		    0, &log, false, NULL, __func__, __LINE__, &tv);

	}
#endif
}

#ifdef TCP_REQUEST_TRK
static void
rack_log_hybrid_sends(struct tcp_rack *rack, struct tcp_sendfile_track *cur, int line)
{
	if (tcp_bblogging_point_on(rack->rc_tp, TCP_BBPOINT_REQ_LEVEL_LOGGING)) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		uint64_t off;

		/* Convert our ms to a microsecond */
		memset(&log, 0, sizeof(log));

		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.delRate = cur->sent_at_fs;

		if ((cur->flags & TCP_TRK_TRACK_FLG_LSND) == 0) {
			/*
			 * We did not get a new Rules Applied to set so
			 * no overlapping send occured, this means the
			 * current byte counts are correct.
			 */
			log.u_bbr.cur_del_rate = rack->rc_tp->t_sndbytes;
			log.u_bbr.rttProp = rack->rc_tp->t_snd_rxt_bytes;
		} else {
			/*
			 * Overlapping send case, we switched to a new
			 * send and did a rules applied.
			 */
			log.u_bbr.cur_del_rate = cur->sent_at_ls;
			log.u_bbr.rttProp = cur->rxt_at_ls;
		}
		log.u_bbr.bw_inuse = cur->rxt_at_fs;
		log.u_bbr.cwnd_gain = line;
		off = (uint64_t)(cur) - (uint64_t)(&rack->rc_tp->t_tcpreq_info[0]);
		log.u_bbr.bbr_substate = (uint8_t)(off / sizeof(struct tcp_sendfile_track));
		/* start = < flex1 | flex2 > */
		log.u_bbr.flex2 = (uint32_t)(cur->start & 0x00000000ffffffff);
		log.u_bbr.flex1 = (uint32_t)((cur->start >> 32) & 0x00000000ffffffff);
		/* end = < flex3 | flex4 > */
		log.u_bbr.flex4 = (uint32_t)(cur->end & 0x00000000ffffffff);
		log.u_bbr.flex3 = (uint32_t)((cur->end >> 32) & 0x00000000ffffffff);

		/* localtime = <delivered | applimited>*/
		log.u_bbr.applimited = (uint32_t)(cur->localtime & 0x00000000ffffffff);
		log.u_bbr.delivered = (uint32_t)((cur->localtime >> 32) & 0x00000000ffffffff);
		/* client timestamp = <lt_epoch | epoch>*/
		log.u_bbr.epoch = (uint32_t)(cur->timestamp & 0x00000000ffffffff);
		log.u_bbr.lt_epoch = (uint32_t)((cur->timestamp >> 32) & 0x00000000ffffffff);
		/* now set all the flags in */
		log.u_bbr.pkts_out = cur->hybrid_flags;
		log.u_bbr.lost = cur->playout_ms;
		log.u_bbr.flex6 = cur->flags;
		/*
		 * Last send time  = <flex5 | pkt_epoch>  note we do not distinguish cases
		 * where a false retransmit occurred so first_send  <-> lastsend may
		 * include longer time then it actually took if we have a false rxt.
		 */
		log.u_bbr.pkt_epoch = (uint32_t)(rack->r_ctl.last_tmit_time_acked & 0x00000000ffffffff);
		log.u_bbr.flex5 = (uint32_t)((rack->r_ctl.last_tmit_time_acked >> 32) & 0x00000000ffffffff);
		/*
		 * Compose bbr_state to be a bit wise 0000ADHF
		 * where A is the always_pace flag
		 * where D is the dgp_on flag
		 * where H is the hybrid_mode on flag
		 * where F is the use_fixed_rate flag.
		 */
		log.u_bbr.bbr_state = rack->rc_always_pace;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->dgp_on;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->rc_hybrid_mode;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->use_fixed_rate;

		log.u_bbr.flex8 = HYBRID_LOG_SENT_LOST;
		tcp_log_event(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_HYBRID_PACING_LOG, 0,
		    0, &log, false, NULL, __func__, __LINE__, &tv);
	}
}
#endif

static inline uint64_t
rack_compensate_for_linerate(struct tcp_rack *rack, uint64_t bw)
{
	uint64_t ret_bw, ether;
	uint64_t u_segsiz;

	ether = rack->rc_tp->t_maxseg + sizeof(struct tcphdr);
	if (rack->r_is_v6){
#ifdef INET6
		ether += sizeof(struct ip6_hdr);
#endif
		ether += 14;	/* eheader size 6+6+2 */
	} else {
#ifdef INET
		ether += sizeof(struct ip);
#endif
		ether += 14;	/* eheader size 6+6+2 */
	}
	u_segsiz = (uint64_t)min(ctf_fixed_maxseg(rack->rc_tp), rack->r_ctl.rc_pace_min_segs);
	ret_bw = bw;
	ret_bw *= ether;
	ret_bw /= u_segsiz;
	return (ret_bw);
}

static void
rack_rate_cap_bw(struct tcp_rack *rack, uint64_t *bw, int *capped)
{
#ifdef TCP_REQUEST_TRK
	struct timeval tv;
	uint64_t timenow, timeleft, lenleft, lengone, calcbw;
#endif

	if (rack->r_ctl.bw_rate_cap == 0)
		return;
#ifdef TCP_REQUEST_TRK
	if (rack->rc_catch_up && rack->rc_hybrid_mode &&
	    (rack->r_ctl.rc_last_sft != NULL)) {
		/*
		 * We have a dynamic cap. The original target
		 * is in bw_rate_cap, but we need to look at
		 * how long it is until we hit the deadline.
		 */
		struct tcp_sendfile_track *ent;

      		ent = rack->r_ctl.rc_last_sft;
		microuptime(&tv);
		timenow = tcp_tv_to_lusectick(&tv);
		if (timenow >= ent->deadline) {
			/* No time left we do DGP only */
			rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
					   0, 0, 0, HYBRID_LOG_OUTOFTIME, 0, ent, __LINE__);
			rack->r_ctl.bw_rate_cap = 0;
			return;
		}
		/* We have the time */
		timeleft = rack->r_ctl.rc_last_sft->deadline - timenow;
		if (timeleft < HPTS_MSEC_IN_SEC) {
			/* If there is less than a ms left just use DGPs rate */
			rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
					   0, timeleft, 0, HYBRID_LOG_OUTOFTIME, 0, ent, __LINE__);
			rack->r_ctl.bw_rate_cap = 0;
			return;
		}
		/*
		 * Now lets find the amount of data left to send.
		 *
		 * Now ideally we want to use the end_seq to figure out how much more
		 * but it might not be possible (only if we have the TRACK_FG_COMP on the entry..
		 */
		if (ent->flags & TCP_TRK_TRACK_FLG_COMP) {
			if (SEQ_GT(ent->end_seq, rack->rc_tp->snd_una))
				lenleft = ent->end_seq - rack->rc_tp->snd_una;
			else {
				/* TSNH, we should catch it at the send */
				rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
						   0, timeleft, 0, HYBRID_LOG_CAPERROR, 0, ent, __LINE__);
				rack->r_ctl.bw_rate_cap = 0;
				return;
			}
		} else {
			/*
			 * The hard way, figure out how much is gone and then
			 * take that away from the total the client asked for
			 * (thats off by tls overhead if this is tls).
			 */
			if (SEQ_GT(rack->rc_tp->snd_una, ent->start_seq))
				lengone = rack->rc_tp->snd_una - ent->start_seq;
			else
				lengone = 0;
			if (lengone < (ent->end - ent->start))
				lenleft = (ent->end - ent->start) - lengone;
			else {
				/* TSNH, we should catch it at the send */
				rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
						   0, timeleft, lengone, HYBRID_LOG_CAPERROR, 0, ent, __LINE__);
				rack->r_ctl.bw_rate_cap = 0;
				return;
			}
		}
		if (lenleft == 0) {
			/* We have it all sent */
			rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
					   0, timeleft, lenleft, HYBRID_LOG_ALLSENT, 0, ent, __LINE__);
			if (rack->r_ctl.bw_rate_cap)
				goto normal_ratecap;
			else
				return;
		}
		calcbw = lenleft * HPTS_USEC_IN_SEC;
		calcbw /= timeleft;
		/* Now we must compensate for IP/TCP overhead */
		calcbw = rack_compensate_for_linerate(rack, calcbw);
		/* Update the bit rate cap */
		rack->r_ctl.bw_rate_cap = calcbw;
		if ((rack->r_ctl.rc_last_sft->hybrid_flags & TCP_HYBRID_PACING_S_MSS) &&
		    (rack_hybrid_allow_set_maxseg == 1) &&
		    ((rack->r_ctl.rc_last_sft->hybrid_flags & TCP_HYBRID_PACING_SETMSS) == 0)) {
			/* Lets set in a smaller mss possibly here to match our rate-cap */
			uint32_t orig_max;

			orig_max = rack->r_ctl.rc_pace_max_segs;
			rack->r_ctl.rc_last_sft->hybrid_flags |= TCP_HYBRID_PACING_SETMSS;
			rack->r_ctl.rc_pace_max_segs = rack_get_pacing_len(rack, calcbw, ctf_fixed_maxseg(rack->rc_tp));
			rack_log_type_pacing_sizes(rack->rc_tp, rack, rack->r_ctl.client_suggested_maxseg, orig_max, __LINE__, 5);
		}
		rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
				   calcbw, timeleft, lenleft, HYBRID_LOG_CAP_CALC, 0, ent, __LINE__);
		if ((calcbw > 0) && (*bw > calcbw)) {
			rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
					   *bw, ent->deadline, lenleft, HYBRID_LOG_RATE_CAP, 0, ent, __LINE__);
			*capped = 1;
			*bw = calcbw;
		}
		return;
	}
normal_ratecap:
#endif
	if ((rack->r_ctl.bw_rate_cap > 0) && (*bw > rack->r_ctl.bw_rate_cap)) {
#ifdef TCP_REQUEST_TRK
		if (rack->rc_hybrid_mode &&
		    rack->rc_catch_up &&
		    (rack->r_ctl.rc_last_sft != NULL) &&
		    (rack->r_ctl.rc_last_sft->hybrid_flags & TCP_HYBRID_PACING_S_MSS) &&
		    (rack_hybrid_allow_set_maxseg == 1) &&
		    ((rack->r_ctl.rc_last_sft->hybrid_flags & TCP_HYBRID_PACING_SETMSS) == 0)) {
			/* Lets set in a smaller mss possibly here to match our rate-cap */
			uint32_t orig_max;

			orig_max = rack->r_ctl.rc_pace_max_segs;
			rack->r_ctl.rc_last_sft->hybrid_flags |= TCP_HYBRID_PACING_SETMSS;
			rack->r_ctl.rc_pace_max_segs = rack_get_pacing_len(rack, rack->r_ctl.bw_rate_cap, ctf_fixed_maxseg(rack->rc_tp));
			rack_log_type_pacing_sizes(rack->rc_tp, rack, rack->r_ctl.client_suggested_maxseg, orig_max, __LINE__, 5);
		}
#endif
		*capped = 1;
		*bw = rack->r_ctl.bw_rate_cap;
		rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
				   *bw, 0, 0,
				   HYBRID_LOG_RATE_CAP, 1, NULL, __LINE__);
	}
}

static uint64_t
rack_get_gp_est(struct tcp_rack *rack)
{
	uint64_t bw, lt_bw, ret_bw;

	if (rack->rc_gp_filled == 0) {
		/*
		 * We have yet no b/w measurement,
		 * if we have a user set initial bw
		 * return it. If we don't have that and
		 * we have an srtt, use the tcp IW (10) to
		 * calculate a fictional b/w over the SRTT
		 * which is more or less a guess. Note
		 * we don't use our IW from rack on purpose
		 * so if we have like IW=30, we are not
		 * calculating a "huge" b/w.
		 */
		uint64_t srtt;

		if (rack->dis_lt_bw == 1)
			lt_bw = 0;
		else
			lt_bw = rack_get_lt_bw(rack);
		if (lt_bw) {
			/*
			 * No goodput bw but a long-term b/w does exist
			 * lets use that.
			 */
			ret_bw = lt_bw;
			goto compensate;
		}
		if (rack->r_ctl.init_rate)
			return (rack->r_ctl.init_rate);

		/* Ok lets come up with the IW guess, if we have a srtt */
		if (rack->rc_tp->t_srtt == 0) {
			/*
			 * Go with old pacing method
			 * i.e. burst mitigation only.
			 */
			return (0);
		}
		/* Ok lets get the initial TCP win (not racks) */
		bw = tcp_compute_initwnd(tcp_maxseg(rack->rc_tp));
		srtt = (uint64_t)rack->rc_tp->t_srtt;
		bw *= (uint64_t)USECS_IN_SECOND;
		bw /= srtt;
		ret_bw = bw;
		goto compensate;

	}
	if (rack->r_ctl.num_measurements >= RACK_REQ_AVG) {
		/* Averaging is done, we can return the value */
		bw = rack->r_ctl.gp_bw;
	} else {
		/* Still doing initial average must calculate */
		bw = rack->r_ctl.gp_bw / max(rack->r_ctl.num_measurements, 1);
	}
	if (rack->dis_lt_bw) {
		/* We are not using lt-bw */
		ret_bw = bw;
		goto compensate;
	}
	lt_bw = rack_get_lt_bw(rack);
	if (lt_bw == 0) {
		/* If we don't have one then equate it to the gp_bw */
		lt_bw = rack->r_ctl.gp_bw;
	}
	if (rack->use_lesser_lt_bw) {
		if (lt_bw < bw)
			ret_bw = lt_bw;
		else
			ret_bw = bw;
	} else {
		if (lt_bw > bw)
			ret_bw = lt_bw;
		else
			ret_bw = bw;
	}
	/*
	 * Now lets compensate based on the TCP/IP overhead. Our
	 * Goodput estimate does not include this so we must pace out
	 * a bit faster since our pacing calculations do. The pacing
	 * calculations use the base ETHERNET_SEGMENT_SIZE and the segsiz
	 * we are using to do this, so we do that here in the opposite
	 * direction as well. This means that if we are tunneled and the
	 * segsiz is say 1200 bytes we will get quite a boost, but its
	 * compensated for in the pacing time the opposite way.
	 */
compensate:
	ret_bw = rack_compensate_for_linerate(rack, ret_bw);
	return(ret_bw);
}


static uint64_t
rack_get_bw(struct tcp_rack *rack)
{
	uint64_t bw;

	if (rack->use_fixed_rate) {
		/* Return the fixed pacing rate */
		return (rack_get_fixed_pacing_bw(rack));
	}
	bw = rack_get_gp_est(rack);
	return (bw);
}

static uint16_t
rack_get_output_gain(struct tcp_rack *rack, struct rack_sendmap *rsm)
{
	if (rack->use_fixed_rate) {
		return (100);
	} else if (rack->in_probe_rtt && (rsm == NULL))
		return (rack->r_ctl.rack_per_of_gp_probertt);
	else if ((IN_FASTRECOVERY(rack->rc_tp->t_flags) &&
		  rack->r_ctl.rack_per_of_gp_rec)) {
		if (rsm) {
			/* a retransmission always use the recovery rate */
			return (rack->r_ctl.rack_per_of_gp_rec);
		} else if (rack->rack_rec_nonrxt_use_cr) {
			/* Directed to use the configured rate */
			goto configured_rate;
		} else if (rack->rack_no_prr &&
			   (rack->r_ctl.rack_per_of_gp_rec > 100)) {
			/* No PRR, lets just use the b/w estimate only */
			return (100);
		} else {
			/*
			 * Here we may have a non-retransmit but we
			 * have no overrides, so just use the recovery
			 * rate (prr is in effect).
			 */
			return (rack->r_ctl.rack_per_of_gp_rec);
		}
	}
configured_rate:
	/* For the configured rate we look at our cwnd vs the ssthresh */
	if (rack->r_ctl.cwnd_to_use < rack->rc_tp->snd_ssthresh)
		return (rack->r_ctl.rack_per_of_gp_ss);
	else
		return (rack->r_ctl.rack_per_of_gp_ca);
}

static void
rack_log_dsack_event(struct tcp_rack *rack, uint8_t mod, uint32_t flex4, uint32_t flex5, uint32_t flex6)
{
	/*
	 * Types of logs (mod value)
	 * 1 = dsack_persists reduced by 1 via T-O or fast recovery exit.
	 * 2 = a dsack round begins, persist is reset to 16.
	 * 3 = a dsack round ends
	 * 4 = Dsack option increases rack rtt flex5 is the srtt input, flex6 is thresh
	 * 5 = Socket option set changing the control flags rc_rack_tmr_std_based, rc_rack_use_dsack
	 * 6 = Final rack rtt, flex4 is srtt and flex6 is final limited thresh.
	 */
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = rack->rc_rack_tmr_std_based;
		log.u_bbr.flex1 <<= 1;
		log.u_bbr.flex1 |= rack->rc_rack_use_dsack;
		log.u_bbr.flex1 <<= 1;
		log.u_bbr.flex1 |= rack->rc_dsack_round_seen;
		log.u_bbr.flex2 = rack->r_ctl.dsack_round_end;
		log.u_bbr.flex3 = rack->r_ctl.num_dsack;
		log.u_bbr.flex4 = flex4;
		log.u_bbr.flex5 = flex5;
		log.u_bbr.flex6 = flex6;
		log.u_bbr.flex7 = rack->r_ctl.dsack_persist;
		log.u_bbr.flex8 = mod;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.epoch = rack->r_ctl.current_round;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    RACK_DSACK_HANDLING, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_hdwr_pacing(struct tcp_rack *rack,
		     uint64_t rate, uint64_t hw_rate, int line,
		     int error, uint16_t mod)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		const struct ifnet *ifp;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = ((hw_rate >> 32) & 0x00000000ffffffff);
		log.u_bbr.flex2 = (hw_rate & 0x00000000ffffffff);
		if (rack->r_ctl.crte) {
			ifp = rack->r_ctl.crte->ptbl->rs_ifp;
		} else if (rack->rc_inp->inp_route.ro_nh &&
			   rack->rc_inp->inp_route.ro_nh->nh_ifp) {
			ifp = rack->rc_inp->inp_route.ro_nh->nh_ifp;
		} else
			ifp = NULL;
		if (ifp) {
			log.u_bbr.flex3 = (((uint64_t)ifp  >> 32) & 0x00000000ffffffff);
			log.u_bbr.flex4 = ((uint64_t)ifp & 0x00000000ffffffff);
		}
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.bw_inuse = rate;
		log.u_bbr.flex5 = line;
		log.u_bbr.flex6 = error;
		log.u_bbr.flex7 = mod;
		log.u_bbr.applimited = rack->r_ctl.rc_pace_max_segs;
		log.u_bbr.flex8 = rack->use_fixed_rate;
		log.u_bbr.flex8 <<= 1;
		log.u_bbr.flex8 |= rack->rack_hdrw_pacing;
		log.u_bbr.pkts_out = rack->rc_tp->t_maxseg;
		log.u_bbr.delRate = rack->r_ctl.crte_prev_rate;
		if (rack->r_ctl.crte)
			log.u_bbr.cur_del_rate = rack->r_ctl.crte->rate;
		else
			log.u_bbr.cur_del_rate = 0;
		log.u_bbr.rttProp = rack->r_ctl.last_hw_bw_req;
		log.u_bbr.epoch = rack->r_ctl.current_round;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_HDWR_PACE, 0,
		    0, &log, false, &tv);
	}
}

static uint64_t
rack_get_output_bw(struct tcp_rack *rack, uint64_t bw, struct rack_sendmap *rsm, int *capped)
{
	/*
	 * We allow rack_per_of_gp_xx to dictate our bw rate we want.
	 */
	uint64_t bw_est, high_rate;
	uint64_t gain;

	gain = (uint64_t)rack_get_output_gain(rack, rsm);
	bw_est = bw * gain;
	bw_est /= (uint64_t)100;
	/* Never fall below the minimum (def 64kbps) */
	if (bw_est < RACK_MIN_BW)
		bw_est = RACK_MIN_BW;
	if (rack->r_rack_hw_rate_caps) {
		/* Rate caps are in place */
		if (rack->r_ctl.crte != NULL) {
			/* We have a hdwr rate already */
			high_rate = tcp_hw_highest_rate(rack->r_ctl.crte);
			if (bw_est >= high_rate) {
				/* We are capping bw at the highest rate table entry */
				if (rack_hw_rate_cap_per &&
				    (((high_rate * (100 + rack_hw_rate_cap_per)) / 100) < bw_est)) {
					rack->r_rack_hw_rate_caps = 0;
					goto done;
				}
				rack_log_hdwr_pacing(rack,
						     bw_est, high_rate, __LINE__,
						     0, 3);
				bw_est = high_rate;
				if (capped)
					*capped = 1;
			}
		} else if ((rack->rack_hdrw_pacing == 0) &&
			   (rack->rack_hdw_pace_ena) &&
			   (rack->rack_attempt_hdwr_pace == 0) &&
			   (rack->rc_inp->inp_route.ro_nh != NULL) &&
			   (rack->rc_inp->inp_route.ro_nh->nh_ifp != NULL)) {
			/*
			 * Special case, we have not yet attempted hardware
			 * pacing, and yet we may, when we do, find out if we are
			 * above the highest rate. We need to know the maxbw for the interface
			 * in question (if it supports ratelimiting). We get back
			 * a 0, if the interface is not found in the RL lists.
			 */
			high_rate = tcp_hw_highest_rate_ifp(rack->rc_inp->inp_route.ro_nh->nh_ifp, rack->rc_inp);
			if (high_rate) {
				/* Yep, we have a rate is it above this rate? */
				if (bw_est > high_rate) {
					bw_est = high_rate;
					if (capped)
						*capped = 1;
				}
			}
		}
	}
done:
	return (bw_est);
}

static void
rack_log_retran_reason(struct tcp_rack *rack, struct rack_sendmap *rsm, uint32_t tsused, uint32_t thresh, int mod)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		if (rack->sack_attack_disable > 0)
			goto log_anyway;
		if ((mod != 1) && (rack_verbose_logging == 0))  {
			/*
			 * We get 3 values currently for mod
			 * 1 - We are retransmitting and this tells the reason.
			 * 2 - We are clearing a dup-ack count.
			 * 3 - We are incrementing a dup-ack count.
			 *
			 * The clear/increment are only logged
			 * if you have BBverbose on.
			 */
			return;
		}
log_anyway:
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = tsused;
		log.u_bbr.flex2 = thresh;
		log.u_bbr.flex3 = rsm->r_flags;
		log.u_bbr.flex4 = rsm->r_dupack;
		log.u_bbr.flex5 = rsm->r_start;
		log.u_bbr.flex6 = rsm->r_end;
		log.u_bbr.flex8 = mod;
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.epoch = rack->r_ctl.current_round;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_SETTINGS_CHG, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_to_start(struct tcp_rack *rack, uint32_t cts, uint32_t to, int32_t slot, uint8_t which)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = rack->rc_tp->t_srtt;
		log.u_bbr.flex2 = to;
		log.u_bbr.flex3 = rack->r_ctl.rc_hpts_flags;
		log.u_bbr.flex4 = slot;
		log.u_bbr.flex5 = rack->rc_tp->t_hpts_slot;
		log.u_bbr.flex6 = rack->rc_tp->t_rxtcur;
		log.u_bbr.flex7 = rack->rc_in_persist;
		log.u_bbr.flex8 = which;
		if (rack->rack_no_prr)
			log.u_bbr.pkts_out = 0;
		else
			log.u_bbr.pkts_out = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.cwnd_gain = rack->rack_deferred_inited;
		log.u_bbr.pkt_epoch = rack->rc_has_collapsed;
		log.u_bbr.lt_epoch = rack->rc_tp->t_rxtshift;
		log.u_bbr.lost = rack_rto_min;
		log.u_bbr.epoch = rack->r_ctl.roundends;
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		log.u_bbr.applimited = rack->rc_tp->t_flags2;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_TIMERSTAR, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_to_event(struct tcp_rack *rack, int32_t to_num, struct rack_sendmap *rsm)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex8 = to_num;
		log.u_bbr.flex1 = rack->r_ctl.rc_rack_min_rtt;
		log.u_bbr.flex2 = rack->rc_rack_rtt;
		if (rsm == NULL)
			log.u_bbr.flex3 = 0;
		else
			log.u_bbr.flex3 = rsm->r_end - rsm->r_start;
		if (rack->rack_no_prr)
			log.u_bbr.flex5 = 0;
		else
			log.u_bbr.flex5 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_RTO, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_map_chg(struct tcpcb *tp, struct tcp_rack *rack,
		 struct rack_sendmap *prev,
		 struct rack_sendmap *rsm,
		 struct rack_sendmap *next,
		 int flag, uint32_t th_ack, int line)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex8 = flag;
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.cur_del_rate = (uint64_t)prev;
		log.u_bbr.delRate = (uint64_t)rsm;
		log.u_bbr.rttProp = (uint64_t)next;
		log.u_bbr.flex7 = 0;
		if (prev) {
			log.u_bbr.flex1 = prev->r_start;
			log.u_bbr.flex2 = prev->r_end;
			log.u_bbr.flex7 |= 0x4;
		}
		if (rsm) {
			log.u_bbr.flex3 = rsm->r_start;
			log.u_bbr.flex4 = rsm->r_end;
			log.u_bbr.flex7 |= 0x2;
		}
		if (next) {
			log.u_bbr.flex5 = next->r_start;
			log.u_bbr.flex6 = next->r_end;
			log.u_bbr.flex7 |= 0x1;
		}
		log.u_bbr.applimited = line;
		log.u_bbr.pkts_out = th_ack;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		if (rack->rack_no_prr)
			log.u_bbr.lost = 0;
		else
			log.u_bbr.lost = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_LOG_MAPCHG, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_rtt_upd(struct tcpcb *tp, struct tcp_rack *rack, uint32_t t, uint32_t len,
		 struct rack_sendmap *rsm, int conf)
{
	if (tcp_bblogging_on(tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex1 = t;
		log.u_bbr.flex2 = len;
		log.u_bbr.flex3 = rack->r_ctl.rc_rack_min_rtt;
		log.u_bbr.flex4 = rack->r_ctl.rack_rs.rs_rtt_lowest;
		log.u_bbr.flex5 = rack->r_ctl.rack_rs.rs_rtt_highest;
		log.u_bbr.flex6 = rack->r_ctl.rack_rs.rs_us_rtrcnt;
		log.u_bbr.flex7 = conf;
		log.u_bbr.rttProp = (uint64_t)rack->r_ctl.rack_rs.rs_rtt_tot;
		log.u_bbr.flex8 = rack->r_ctl.rc_rate_sample_method;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.delivered = rack->r_ctl.rack_rs.rs_us_rtrcnt;
		log.u_bbr.pkts_out = rack->r_ctl.rack_rs.rs_flags;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		if (rsm) {
			log.u_bbr.pkt_epoch = rsm->r_start;
			log.u_bbr.lost = rsm->r_end;
			log.u_bbr.cwnd_gain = rsm->r_rtr_cnt;
			/* We loose any upper of the 24 bits */
			log.u_bbr.pacing_gain = (uint16_t)rsm->r_flags;
		} else {
			/* Its a SYN */
			log.u_bbr.pkt_epoch = rack->rc_tp->iss;
			log.u_bbr.lost = 0;
			log.u_bbr.cwnd_gain = 0;
			log.u_bbr.pacing_gain = 0;
		}
		/* Write out general bits of interest rrs here */
		log.u_bbr.use_lt_bw = rack->rc_highly_buffered;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->forced_ack;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->rc_gp_dyn_mul;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->in_probe_rtt;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->measure_saw_probe_rtt;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->app_limited_needs_set;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->rc_gp_filled;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->rc_dragged_bottom;
		log.u_bbr.applimited = rack->r_ctl.rc_target_probertt_flight;
		log.u_bbr.epoch = rack->r_ctl.rc_time_probertt_starts;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_time_probertt_entered;
		log.u_bbr.cur_del_rate = rack->r_ctl.rc_lower_rtt_us_cts;
		log.u_bbr.delRate = rack->r_ctl.rc_gp_srtt;
		log.u_bbr.bw_inuse = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
		log.u_bbr.bw_inuse <<= 32;
		if (rsm)
			log.u_bbr.bw_inuse |= ((uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)]);
		TCP_LOG_EVENTP(tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_BBRRTT, 0,
		    0, &log, false, &tv);


	}
}

static void
rack_log_rtt_sample(struct tcp_rack *rack, uint32_t rtt)
{
	/*
	 * Log the rtt sample we are
	 * applying to the srtt algorithm in
	 * useconds.
	 */
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		/* Convert our ms to a microsecond */
		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = rtt;
		log.u_bbr.flex2 = rack->r_ctl.ack_count;
		log.u_bbr.flex3 = rack->r_ctl.sack_count;
		log.u_bbr.flex4 = rack->r_ctl.sack_noextra_move;
		log.u_bbr.flex5 = rack->r_ctl.sack_moved_extra;
		log.u_bbr.flex6 = rack->rc_tp->t_rxtcur;
		log.u_bbr.flex7 = 1;
		log.u_bbr.flex8 = rack->sack_attack_disable;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		/*
		 * We capture in delRate the upper 32 bits as
		 * the confidence level we had declared, and the
		 * lower 32 bits as the actual RTT using the arrival
		 * timestamp.
		 */
		log.u_bbr.delRate = rack->r_ctl.rack_rs.confidence;
		log.u_bbr.delRate <<= 32;
		log.u_bbr.delRate |= rack->r_ctl.rack_rs.rs_us_rtt;
		/* Lets capture all the things that make up t_rtxcur */
		log.u_bbr.applimited = rack_rto_min;
		log.u_bbr.epoch = rack_rto_max;
		log.u_bbr.lt_epoch = rack->r_ctl.timer_slop;
		log.u_bbr.lost = rack_rto_min;
		log.u_bbr.pkt_epoch = TICKS_2_USEC(tcp_rexmit_slop);
		log.u_bbr.rttProp = RACK_REXMTVAL(rack->rc_tp);
		log.u_bbr.bw_inuse = rack->r_ctl.act_rcv_time.tv_sec;
		log.u_bbr.bw_inuse *= HPTS_USEC_IN_SEC;
		log.u_bbr.bw_inuse += rack->r_ctl.act_rcv_time.tv_usec;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_LOG_RTT, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_rtt_sample_calc(struct tcp_rack *rack, uint32_t rtt, uint32_t send_time, uint32_t ack_time, int where)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		/* Convert our ms to a microsecond */
		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = rtt;
		log.u_bbr.flex2 = send_time;
		log.u_bbr.flex3 = ack_time;
		log.u_bbr.flex4 = where;
		log.u_bbr.flex7 = 2;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_LOG_RTT, 0,
		    0, &log, false, &tv);
	}
}


static void
rack_log_rtt_sendmap(struct tcp_rack *rack, uint32_t idx, uint64_t tsv, uint32_t tsecho)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		/* Convert our ms to a microsecond */
		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = idx;
		log.u_bbr.flex2 = rack_ts_to_msec(tsv);
		log.u_bbr.flex3 = tsecho;
		log.u_bbr.flex7 = 3;
		log.u_bbr.rttProp = tsv;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_LOG_RTT, 0,
		    0, &log, false, &tv);
	}
}


static inline void
rack_log_progress_event(struct tcp_rack *rack, struct tcpcb *tp, uint32_t tick,  int event, int line)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex1 = line;
		log.u_bbr.flex2 = tick;
		log.u_bbr.flex3 = tp->t_maxunacktime;
		log.u_bbr.flex4 = tp->t_acktime;
		log.u_bbr.flex8 = event;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_PROGRESS, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_type_bbrsnd(struct tcp_rack *rack, uint32_t len, uint32_t slot, uint32_t cts, struct timeval *tv, int line)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex1 = slot;
		if (rack->rack_no_prr)
			log.u_bbr.flex2 = 0;
		else
			log.u_bbr.flex2 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex4 = rack->r_ctl.rc_hpts_flags;
		log.u_bbr.flex5 = rack->r_ctl.ack_during_sd;
		log.u_bbr.flex6 = line;
		log.u_bbr.flex7 = (0x0000ffff & rack->r_ctl.rc_hpts_flags);
		log.u_bbr.flex8 = rack->rc_in_persist;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_BBRSND, 0,
		    0, &log, false, tv);
	}
}

static void
rack_log_doseg_done(struct tcp_rack *rack, uint32_t cts, int32_t nxt_pkt, int32_t did_out, int way_out, int nsegs)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = did_out;
		log.u_bbr.flex2 = nxt_pkt;
		log.u_bbr.flex3 = way_out;
		log.u_bbr.flex4 = rack->r_ctl.rc_hpts_flags;
		if (rack->rack_no_prr)
			log.u_bbr.flex5 = 0;
		else
			log.u_bbr.flex5 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex6 = nsegs;
		log.u_bbr.applimited = rack->r_ctl.rc_pace_min_segs;
		log.u_bbr.flex7 = rack->rc_ack_can_sendout_data;	/* Do we have ack-can-send set */
		log.u_bbr.flex7 <<= 1;
		log.u_bbr.flex7 |= rack->r_fast_output;	/* is fast output primed */
		log.u_bbr.flex7 <<= 1;
		log.u_bbr.flex7 |= rack->r_wanted_output;	/* Do we want output */
		log.u_bbr.flex8 = rack->rc_in_persist;
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.use_lt_bw = rack->r_ent_rec_ns;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->r_might_revert;
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		log.u_bbr.epoch = rack->rc_inp->inp_socket->so_snd.sb_hiwat;
		log.u_bbr.lt_epoch = rack->rc_inp->inp_socket->so_rcv.sb_hiwat;
		log.u_bbr.lost = rack->rc_tp->t_srtt;
		log.u_bbr.pkt_epoch = rack->rc_tp->rfbuf_cnt;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_DOSEG_DONE, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_type_pacing_sizes(struct tcpcb *tp, struct tcp_rack *rack, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint8_t frm)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = rack->r_ctl.rc_pace_min_segs;
		log.u_bbr.flex3 = rack->r_ctl.rc_pace_max_segs;
		log.u_bbr.flex4 = arg1;
		log.u_bbr.flex5 = arg2;
		log.u_bbr.flex7 = rack->r_ctl.rc_user_set_min_segs;
		log.u_bbr.flex6 = arg3;
		log.u_bbr.flex8 = frm;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.applimited = rack->r_ctl.rc_sacked;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		TCP_LOG_EVENTP(tp, NULL, &tptosocket(tp)->so_rcv,
		    &tptosocket(tp)->so_snd,
		    TCP_HDWR_PACE_SIZE, 0, 0, &log, false, &tv);
	}
}

static void
rack_log_type_just_return(struct tcp_rack *rack, uint32_t cts, uint32_t tlen, uint32_t slot,
			  uint8_t hpts_calling, int reason, uint32_t cwnd_to_use)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex1 = slot;
		log.u_bbr.flex2 = rack->r_ctl.rc_hpts_flags;
		log.u_bbr.flex4 = reason;
		if (rack->rack_no_prr)
			log.u_bbr.flex5 = 0;
		else
			log.u_bbr.flex5 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex7 = hpts_calling;
		log.u_bbr.flex8 = rack->rc_in_persist;
		log.u_bbr.lt_epoch = cwnd_to_use;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.cwnd_gain = rack->rc_has_collapsed;
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_JUSTRET, 0,
		    tlen, &log, false, &tv);
	}
}

static void
rack_log_to_cancel(struct tcp_rack *rack, int32_t hpts_removed, int line, uint32_t us_cts,
		   struct timeval *tv, uint32_t flags_on_entry)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex1 = line;
		log.u_bbr.flex2 = rack->r_ctl.rc_last_output_to;
		log.u_bbr.flex3 = flags_on_entry;
		log.u_bbr.flex4 = us_cts;
		if (rack->rack_no_prr)
			log.u_bbr.flex5 = 0;
		else
			log.u_bbr.flex5 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex6 = rack->rc_tp->t_rxtcur;
		log.u_bbr.flex7 = hpts_removed;
		log.u_bbr.flex8 = 1;
		log.u_bbr.applimited = rack->r_ctl.rc_hpts_flags;
		log.u_bbr.timeStamp = us_cts;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.bw_inuse = rack->r_ctl.current_round;
		log.u_bbr.bw_inuse <<= 32;
		log.u_bbr.bw_inuse |= rack->r_ctl.rc_considered_lost;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_TIMERCANC, 0,
		    0, &log, false, tv);
	}
}

static void
rack_log_alt_to_to_cancel(struct tcp_rack *rack,
			  uint32_t flex1, uint32_t flex2,
			  uint32_t flex3, uint32_t flex4,
			  uint32_t flex5, uint32_t flex6,
			  uint16_t flex7, uint8_t mod)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		if (mod == 1) {
			/* No you can't use 1, its for the real to cancel */
			return;
		}
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = flex1;
		log.u_bbr.flex2 = flex2;
		log.u_bbr.flex3 = flex3;
		log.u_bbr.flex4 = flex4;
		log.u_bbr.flex5 = flex5;
		log.u_bbr.flex6 = flex6;
		log.u_bbr.flex7 = flex7;
		log.u_bbr.flex8 = mod;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_TIMERCANC, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_to_processing(struct tcp_rack *rack, uint32_t cts, int32_t ret, int32_t timers)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = timers;
		log.u_bbr.flex2 = ret;
		log.u_bbr.flex3 = rack->r_ctl.rc_timer_exp;
		log.u_bbr.flex4 = rack->r_ctl.rc_hpts_flags;
		log.u_bbr.flex5 = cts;
		if (rack->rack_no_prr)
			log.u_bbr.flex6 = 0;
		else
			log.u_bbr.flex6 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.pkts_out = rack->r_ctl.rc_out_at_rto;
		log.u_bbr.delivered = rack->r_ctl.rc_snd_max_at_rto;
		log.u_bbr.pacing_gain = rack->r_must_retran;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_TO_PROCESS, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_log_to_prr(struct tcp_rack *rack, int frm, int orig_cwnd, int line)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = rack->r_ctl.rc_prr_out;
		log.u_bbr.flex2 = rack->r_ctl.rc_prr_recovery_fs;
		if (rack->rack_no_prr)
			log.u_bbr.flex3 = 0;
		else
			log.u_bbr.flex3 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex4 = rack->r_ctl.rc_prr_delivered;
		log.u_bbr.flex5 = rack->r_ctl.rc_sacked;
		log.u_bbr.flex6 = rack->r_ctl.rc_holes_rxt;
		log.u_bbr.flex7 = line;
		log.u_bbr.flex8 = frm;
		log.u_bbr.pkts_out = orig_cwnd;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.use_lt_bw = rack->r_ent_rec_ns;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->r_might_revert;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_BBRUPD, 0,
		    0, &log, false, &tv);
	}
}

#ifdef TCP_SAD_DETECTION
static void
rack_log_sad(struct tcp_rack *rack, int event)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = rack->r_ctl.sack_count;
		log.u_bbr.flex2 = rack->r_ctl.ack_count;
		log.u_bbr.flex3 = rack->r_ctl.sack_moved_extra;
		log.u_bbr.flex4 = rack->r_ctl.sack_noextra_move;
		log.u_bbr.flex5 = rack->r_ctl.rc_num_maps_alloced;
		log.u_bbr.flex6 = tcp_sack_to_ack_thresh;
		log.u_bbr.pkts_out = tcp_sack_to_move_thresh;
		log.u_bbr.lt_epoch = (tcp_force_detection << 8);
		log.u_bbr.lt_epoch |= rack->do_detection;
		log.u_bbr.applimited = tcp_map_minimum;
		log.u_bbr.flex7 = rack->sack_attack_disable;
		log.u_bbr.flex8 = event;
		log.u_bbr.bbr_state = rack->rc_suspicious;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.delivered = tcp_sad_decay_val;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_SAD_DETECT, 0,
		    0, &log, false, &tv);
	}
}
#endif

static void
rack_counter_destroy(void)
{
	counter_u64_free(rack_total_bytes);
	counter_u64_free(rack_fto_send);
	counter_u64_free(rack_fto_rsm_send);
	counter_u64_free(rack_nfto_resend);
	counter_u64_free(rack_hw_pace_init_fail);
	counter_u64_free(rack_hw_pace_lost);
	counter_u64_free(rack_non_fto_send);
	counter_u64_free(rack_extended_rfo);
	counter_u64_free(rack_ack_total);
	counter_u64_free(rack_express_sack);
	counter_u64_free(rack_sack_total);
	counter_u64_free(rack_move_none);
	counter_u64_free(rack_move_some);
	counter_u64_free(rack_sack_attacks_detected);
	counter_u64_free(rack_sack_attacks_reversed);
	counter_u64_free(rack_sack_attacks_suspect);
	counter_u64_free(rack_sack_used_next_merge);
	counter_u64_free(rack_sack_used_prev_merge);
	counter_u64_free(rack_tlp_tot);
	counter_u64_free(rack_tlp_newdata);
	counter_u64_free(rack_tlp_retran);
	counter_u64_free(rack_tlp_retran_bytes);
	counter_u64_free(rack_to_tot);
	counter_u64_free(rack_saw_enobuf);
	counter_u64_free(rack_saw_enobuf_hw);
	counter_u64_free(rack_saw_enetunreach);
	counter_u64_free(rack_hot_alloc);
	counter_u64_free(tcp_policer_detected);
	counter_u64_free(rack_to_alloc);
	counter_u64_free(rack_to_alloc_hard);
	counter_u64_free(rack_to_alloc_emerg);
	counter_u64_free(rack_to_alloc_limited);
	counter_u64_free(rack_alloc_limited_conns);
	counter_u64_free(rack_split_limited);
	counter_u64_free(rack_multi_single_eq);
	counter_u64_free(rack_rxt_clamps_cwnd);
	counter_u64_free(rack_rxt_clamps_cwnd_uniq);
	counter_u64_free(rack_proc_non_comp_ack);
	counter_u64_free(rack_sack_proc_all);
	counter_u64_free(rack_sack_proc_restart);
	counter_u64_free(rack_sack_proc_short);
	counter_u64_free(rack_sack_skipped_acked);
	counter_u64_free(rack_sack_splits);
	counter_u64_free(rack_input_idle_reduces);
	counter_u64_free(rack_collapsed_win);
	counter_u64_free(rack_collapsed_win_rxt);
	counter_u64_free(rack_collapsed_win_rxt_bytes);
	counter_u64_free(rack_collapsed_win_seen);
	counter_u64_free(rack_try_scwnd);
	counter_u64_free(rack_persists_sends);
	counter_u64_free(rack_persists_acks);
	counter_u64_free(rack_persists_loss);
	counter_u64_free(rack_persists_lost_ends);
#ifdef INVARIANTS
	counter_u64_free(rack_adjust_map_bw);
#endif
	COUNTER_ARRAY_FREE(rack_out_size, TCP_MSS_ACCT_SIZE);
	COUNTER_ARRAY_FREE(rack_opts_arry, RACK_OPTS_SIZE);
}

static struct rack_sendmap *
rack_alloc(struct tcp_rack *rack)
{
	struct rack_sendmap *rsm;

	/*
	 * First get the top of the list it in
	 * theory is the "hottest" rsm we have,
	 * possibly just freed by ack processing.
	 */
	if (rack->rc_free_cnt > rack_free_cache) {
		rsm = TAILQ_FIRST(&rack->r_ctl.rc_free);
		TAILQ_REMOVE(&rack->r_ctl.rc_free, rsm, r_tnext);
		counter_u64_add(rack_hot_alloc, 1);
		rack->rc_free_cnt--;
		return (rsm);
	}
	/*
	 * Once we get under our free cache we probably
	 * no longer have a "hot" one available. Lets
	 * get one from UMA.
	 */
	rsm = uma_zalloc(rack_zone, M_NOWAIT);
	if (rsm) {
		rack->r_ctl.rc_num_maps_alloced++;
		counter_u64_add(rack_to_alloc, 1);
		return (rsm);
	}
	/*
	 * Dig in to our aux rsm's (the last two) since
	 * UMA failed to get us one.
	 */
	if (rack->rc_free_cnt) {
		counter_u64_add(rack_to_alloc_emerg, 1);
		rsm = TAILQ_FIRST(&rack->r_ctl.rc_free);
		TAILQ_REMOVE(&rack->r_ctl.rc_free, rsm, r_tnext);
		rack->rc_free_cnt--;
		return (rsm);
	}
	return (NULL);
}

static struct rack_sendmap *
rack_alloc_full_limit(struct tcp_rack *rack)
{
	if ((V_tcp_map_entries_limit > 0) &&
	    (rack->do_detection == 0) &&
	    (rack->r_ctl.rc_num_maps_alloced >= V_tcp_map_entries_limit)) {
		counter_u64_add(rack_to_alloc_limited, 1);
		if (!rack->alloc_limit_reported) {
			rack->alloc_limit_reported = 1;
			counter_u64_add(rack_alloc_limited_conns, 1);
		}
		return (NULL);
	}
	return (rack_alloc(rack));
}

/* wrapper to allocate a sendmap entry, subject to a specific limit */
static struct rack_sendmap *
rack_alloc_limit(struct tcp_rack *rack, uint8_t limit_type)
{
	struct rack_sendmap *rsm;

	if (limit_type) {
		/* currently there is only one limit type */
		if (rack->r_ctl.rc_split_limit > 0 &&
		    (rack->do_detection == 0) &&
		    rack->r_ctl.rc_num_split_allocs >= rack->r_ctl.rc_split_limit) {
			counter_u64_add(rack_split_limited, 1);
			if (!rack->alloc_limit_reported) {
				rack->alloc_limit_reported = 1;
				counter_u64_add(rack_alloc_limited_conns, 1);
			}
			return (NULL);
#ifdef TCP_SAD_DETECTION
		} else if ((tcp_sad_limit != 0) &&
			   (rack->do_detection == 1) &&
			   (rack->r_ctl.rc_num_split_allocs >= tcp_sad_limit)) {
			counter_u64_add(rack_split_limited, 1);
			if (!rack->alloc_limit_reported) {
				rack->alloc_limit_reported = 1;
				counter_u64_add(rack_alloc_limited_conns, 1);
			}
			return (NULL);
#endif
		}
	}

	/* allocate and mark in the limit type, if set */
	rsm = rack_alloc(rack);
	if (rsm != NULL && limit_type) {
		rsm->r_limit_type = limit_type;
		rack->r_ctl.rc_num_split_allocs++;
	}
	return (rsm);
}

static void
rack_free_trim(struct tcp_rack *rack)
{
	struct rack_sendmap *rsm;

	/*
	 * Free up all the tail entries until
	 * we get our list down to the limit.
	 */
	while (rack->rc_free_cnt > rack_free_cache) {
		rsm = TAILQ_LAST(&rack->r_ctl.rc_free, rack_head);
		TAILQ_REMOVE(&rack->r_ctl.rc_free, rsm, r_tnext);
		rack->rc_free_cnt--;
		rack->r_ctl.rc_num_maps_alloced--;
		uma_zfree(rack_zone, rsm);
	}
}

static void
rack_free(struct tcp_rack *rack, struct rack_sendmap *rsm)
{
	if (rsm->r_flags & RACK_APP_LIMITED) {
		if (rack->r_ctl.rc_app_limited_cnt > 0) {
			rack->r_ctl.rc_app_limited_cnt--;
		}
	}
	if (rsm->r_limit_type) {
		/* currently there is only one limit type */
		rack->r_ctl.rc_num_split_allocs--;
	}
	if (rsm == rack->r_ctl.rc_first_appl) {
		rack->r_ctl.cleared_app_ack_seq = rsm->r_start + (rsm->r_end - rsm->r_start);
		rack->r_ctl.cleared_app_ack = 1;
		if (rack->r_ctl.rc_app_limited_cnt == 0)
			rack->r_ctl.rc_first_appl = NULL;
		else
			rack->r_ctl.rc_first_appl = tqhash_find(rack->r_ctl.tqh, rsm->r_nseq_appl);
	}
	if (rsm == rack->r_ctl.rc_resend)
		rack->r_ctl.rc_resend = NULL;
	if (rsm == rack->r_ctl.rc_end_appl)
		rack->r_ctl.rc_end_appl = NULL;
	if (rack->r_ctl.rc_tlpsend == rsm)
		rack->r_ctl.rc_tlpsend = NULL;
	if (rack->r_ctl.rc_sacklast == rsm)
		rack->r_ctl.rc_sacklast = NULL;
	memset(rsm, 0, sizeof(struct rack_sendmap));
	/* Make sure we are not going to overrun our count limit of 0xff */
	if ((rack->rc_free_cnt + 1) > RACK_FREE_CNT_MAX) {
		rack_free_trim(rack);
	}
	TAILQ_INSERT_HEAD(&rack->r_ctl.rc_free, rsm, r_tnext);
	rack->rc_free_cnt++;
}

static uint32_t
rack_get_measure_window(struct tcpcb *tp, struct tcp_rack *rack)
{
	uint64_t srtt, bw, len, tim;
	uint32_t segsiz, def_len, minl;

	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	def_len = rack_def_data_window * segsiz;
	if (rack->rc_gp_filled == 0) {
		/*
		 * We have no measurement (IW is in flight?) so
		 * we can only guess using our data_window sysctl
		 * value (usually 20MSS).
		 */
		return (def_len);
	}
	/*
	 * Now we have a number of factors to consider.
	 *
	 * 1) We have a desired BDP which is usually
	 *    at least 2.
	 * 2) We have a minimum number of rtt's usually 1 SRTT
	 *    but we allow it too to be more.
	 * 3) We want to make sure a measurement last N useconds (if
	 *    we have set rack_min_measure_usec.
	 *
	 * We handle the first concern here by trying to create a data
	 * window of max(rack_def_data_window, DesiredBDP). The
	 * second concern we handle in not letting the measurement
	 * window end normally until at least the required SRTT's
	 * have gone by which is done further below in
	 * rack_enough_for_measurement(). Finally the third concern
	 * we also handle here by calculating how long that time
	 * would take at the current BW and then return the
	 * max of our first calculation and that length. Note
	 * that if rack_min_measure_usec is 0, we don't deal
	 * with concern 3. Also for both Concern 1 and 3 an
	 * application limited period could end the measurement
	 * earlier.
	 *
	 * So lets calculate the BDP with the "known" b/w using
	 * the SRTT has our rtt and then multiply it by the
	 * goal.
	 */
	bw = rack_get_bw(rack);
	srtt = (uint64_t)tp->t_srtt;
	len = bw * srtt;
	len /= (uint64_t)HPTS_USEC_IN_SEC;
	len *= max(1, rack_goal_bdp);
	/* Now we need to round up to the nearest MSS */
	len = roundup(len, segsiz);
	if (rack_min_measure_usec) {
		/* Now calculate our min length for this b/w */
		tim = rack_min_measure_usec;
		minl = (tim * bw) / (uint64_t)HPTS_USEC_IN_SEC;
		if (minl == 0)
			minl = 1;
		minl = roundup(minl, segsiz);
		if (len < minl)
			len = minl;
	}
	/*
	 * Now if we have a very small window we want
	 * to attempt to get the window that is
	 * as small as possible. This happens on
	 * low b/w connections and we don't want to
	 * span huge numbers of rtt's between measurements.
	 *
	 * We basically include 2 over our "MIN window" so
	 * that the measurement can be shortened (possibly) by
	 * an ack'ed packet.
	 */
	if (len < def_len)
		return (max((uint32_t)len, ((MIN_GP_WIN+2) * segsiz)));
	else
		return (max((uint32_t)len, def_len));

}

static int
rack_enough_for_measurement(struct tcpcb *tp, struct tcp_rack *rack, tcp_seq th_ack, uint8_t *quality)
{
	uint32_t tim, srtts, segsiz;

	/*
	 * Has enough time passed for the GP measurement to be valid?
	 */
	if (SEQ_LT(th_ack, tp->gput_seq)) {
		/* Not enough bytes yet */
		return (0);
	}
	if ((tp->snd_max == tp->snd_una) ||
	    (th_ack == tp->snd_max)){
		/*
		 * All is acked quality of all acked is
		 * usually low or medium, but we in theory could split
		 * all acked into two cases, where you got
		 * a signifigant amount of your window and
		 * where you did not. For now we leave it
		 * but it is something to contemplate in the
		 * future. The danger here is that delayed ack
		 * is effecting the last byte (which is a 50:50 chance).
		 */
		*quality = RACK_QUALITY_ALLACKED;
		return (1);
	}
	if (SEQ_GEQ(th_ack,  tp->gput_ack)) {
		/*
		 * We obtained our entire window of data we wanted
		 * no matter if we are in recovery or not then
		 * its ok since expanding the window does not
		 * make things fuzzy (or at least not as much).
		 */
		*quality = RACK_QUALITY_HIGH;
		return (1);
	}
	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	if (SEQ_LT(th_ack, tp->gput_ack) &&
	    ((th_ack - tp->gput_seq) < max(rc_init_window(rack), (MIN_GP_WIN * segsiz)))) {
		/* Not enough bytes yet */
		return (0);
	}
	if (rack->r_ctl.rc_first_appl &&
	    (SEQ_GEQ(th_ack, rack->r_ctl.rc_first_appl->r_end))) {
		/*
		 * We are up to the app limited send point
		 * we have to measure irrespective of the time..
		 */
		*quality = RACK_QUALITY_APPLIMITED;
		return (1);
	}
	/* Now what about time? */
	srtts = (rack->r_ctl.rc_gp_srtt * rack_min_srtts);
	tim = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time) - tp->gput_ts;
	if ((tim >= srtts) && (IN_RECOVERY(rack->rc_tp->t_flags) == 0)) {
		/*
		 * We do not allow a measurement if we are in recovery
		 * that would shrink the goodput window we wanted.
		 * This is to prevent cloudyness of when the last send
		 * was actually made.
		 */
		*quality = RACK_QUALITY_HIGH;
		return (1);
	}
	/* Nope not even a full SRTT has passed */
	return (0);
}

static void
rack_log_timely(struct tcp_rack *rack,
		uint32_t logged, uint64_t cur_bw, uint64_t low_bnd,
		uint64_t up_bnd, int line, uint8_t method)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = logged;
		log.u_bbr.flex2 = rack->rc_gp_timely_inc_cnt;
		log.u_bbr.flex2 <<= 4;
		log.u_bbr.flex2 |= rack->rc_gp_timely_dec_cnt;
		log.u_bbr.flex2 <<= 4;
		log.u_bbr.flex2 |= rack->rc_gp_incr;
		log.u_bbr.flex2 <<= 4;
		log.u_bbr.flex2 |= rack->rc_gp_bwred;
		log.u_bbr.flex3 = rack->rc_gp_incr;
		log.u_bbr.flex4 = rack->r_ctl.rack_per_of_gp_ss;
		log.u_bbr.flex5 = rack->r_ctl.rack_per_of_gp_ca;
		log.u_bbr.flex6 = rack->r_ctl.rack_per_of_gp_rec;
		log.u_bbr.flex7 = rack->rc_gp_bwred;
		log.u_bbr.flex8 = method;
		log.u_bbr.cur_del_rate = cur_bw;
		log.u_bbr.delRate = low_bnd;
		log.u_bbr.bw_inuse = up_bnd;
		log.u_bbr.rttProp = rack_get_bw(rack);
		log.u_bbr.pkt_epoch = line;
		log.u_bbr.pkts_out = rack->r_ctl.rc_rtt_diff;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.epoch = rack->r_ctl.rc_gp_srtt;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_prev_gp_srtt;
		log.u_bbr.cwnd_gain = rack->rc_dragged_bottom;
		log.u_bbr.cwnd_gain <<= 1;
		log.u_bbr.cwnd_gain |= rack->rc_gp_saw_rec;
		log.u_bbr.cwnd_gain <<= 1;
		log.u_bbr.cwnd_gain |= rack->rc_gp_saw_ss;
		log.u_bbr.cwnd_gain <<= 1;
		log.u_bbr.cwnd_gain |= rack->rc_gp_saw_ca;
		log.u_bbr.lost = rack->r_ctl.rc_loss_count;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_TIMELY_WORK, 0,
		    0, &log, false, &tv);
	}
}

static int
rack_bw_can_be_raised(struct tcp_rack *rack, uint64_t cur_bw, uint64_t last_bw_est, uint16_t mult)
{
	/*
	 * Before we increase we need to know if
	 * the estimate just made was less than
	 * our pacing goal (i.e. (cur_bw * mult) > last_bw_est)
	 *
	 * If we already are pacing at a fast enough
	 * rate to push us faster there is no sense of
	 * increasing.
	 *
	 * We first caculate our actual pacing rate (ss or ca multiplier
	 * times our cur_bw).
	 *
	 * Then we take the last measured rate and multipy by our
	 * maximum pacing overage to give us a max allowable rate.
	 *
	 * If our act_rate is smaller than our max_allowable rate
	 * then we should increase. Else we should hold steady.
	 *
	 */
	uint64_t act_rate, max_allow_rate;

	if (rack_timely_no_stopping)
		return (1);

	if ((cur_bw == 0) || (last_bw_est == 0)) {
		/*
		 * Initial startup case or
		 * everything is acked case.
		 */
		rack_log_timely(rack,  mult, cur_bw, 0, 0,
				__LINE__, 9);
		return (1);
	}
	if (mult <= 100) {
		/*
		 * We can always pace at or slightly above our rate.
		 */
		rack_log_timely(rack,  mult, cur_bw, 0, 0,
				__LINE__, 9);
		return (1);
	}
	act_rate = cur_bw * (uint64_t)mult;
	act_rate /= 100;
	max_allow_rate = last_bw_est * ((uint64_t)rack_max_per_above + (uint64_t)100);
	max_allow_rate /= 100;
	if (act_rate < max_allow_rate) {
		/*
		 * Here the rate we are actually pacing at
		 * is smaller than 10% above our last measurement.
		 * This means we are pacing below what we would
		 * like to try to achieve (plus some wiggle room).
		 */
		rack_log_timely(rack,  mult, cur_bw, act_rate, max_allow_rate,
				__LINE__, 9);
		return (1);
	} else {
		/*
		 * Here we are already pacing at least rack_max_per_above(10%)
		 * what we are getting back. This indicates most likely
		 * that we are being limited (cwnd/rwnd/app) and can't
		 * get any more b/w. There is no sense of trying to
		 * raise up the pacing rate its not speeding us up
		 * and we already are pacing faster than we are getting.
		 */
		rack_log_timely(rack,  mult, cur_bw, act_rate, max_allow_rate,
				__LINE__, 8);
		return (0);
	}
}

static void
rack_validate_multipliers_at_or_above100(struct tcp_rack *rack)
{
	/*
	 * When we drag bottom, we want to assure
	 * that no multiplier is below 1.0, if so
	 * we want to restore it to at least that.
	 */
	if (rack->r_ctl.rack_per_of_gp_rec  < 100) {
		/* This is unlikely we usually do not touch recovery */
		rack->r_ctl.rack_per_of_gp_rec = 100;
	}
	if (rack->r_ctl.rack_per_of_gp_ca < 100) {
		rack->r_ctl.rack_per_of_gp_ca = 100;
	}
	if (rack->r_ctl.rack_per_of_gp_ss < 100) {
		rack->r_ctl.rack_per_of_gp_ss = 100;
	}
}

static void
rack_validate_multipliers_at_or_below_100(struct tcp_rack *rack)
{
	if (rack->r_ctl.rack_per_of_gp_ca > 100) {
		rack->r_ctl.rack_per_of_gp_ca = 100;
	}
	if (rack->r_ctl.rack_per_of_gp_ss > 100) {
		rack->r_ctl.rack_per_of_gp_ss = 100;
	}
}

static void
rack_increase_bw_mul(struct tcp_rack *rack, int timely_says, uint64_t cur_bw, uint64_t last_bw_est, int override)
{
	int32_t  calc, logged, plus;

	logged = 0;

	if (rack->rc_skip_timely)
		return;
	if (override) {
		/*
		 * override is passed when we are
		 * loosing b/w and making one last
		 * gasp at trying to not loose out
		 * to a new-reno flow.
		 */
		goto extra_boost;
	}
	/* In classic timely we boost by 5x if we have 5 increases in a row, lets not */
	if (rack->rc_gp_incr &&
	    ((rack->rc_gp_timely_inc_cnt + 1) >= RACK_TIMELY_CNT_BOOST)) {
		/*
		 * Reset and get 5 strokes more before the boost. Note
		 * that the count is 0 based so we have to add one.
		 */
extra_boost:
		plus = (uint32_t)rack_gp_increase_per * RACK_TIMELY_CNT_BOOST;
		rack->rc_gp_timely_inc_cnt = 0;
	} else
		plus = (uint32_t)rack_gp_increase_per;
	/* Must be at least 1% increase for true timely increases */
	if ((plus < 1) &&
	    ((rack->r_ctl.rc_rtt_diff <= 0) || (timely_says <= 0)))
		plus = 1;
	if (rack->rc_gp_saw_rec &&
	    (rack->rc_gp_no_rec_chg == 0) &&
	    rack_bw_can_be_raised(rack, cur_bw, last_bw_est,
				  rack->r_ctl.rack_per_of_gp_rec)) {
		/* We have been in recovery ding it too */
		calc = rack->r_ctl.rack_per_of_gp_rec + plus;
		if (calc > 0xffff)
			calc = 0xffff;
		logged |= 1;
		rack->r_ctl.rack_per_of_gp_rec = (uint16_t)calc;
		if (rack->r_ctl.rack_per_upper_bound_ca &&
		    (rack->rc_dragged_bottom == 0) &&
		    (rack->r_ctl.rack_per_of_gp_rec > rack->r_ctl.rack_per_upper_bound_ca))
			rack->r_ctl.rack_per_of_gp_rec = rack->r_ctl.rack_per_upper_bound_ca;
	}
	if (rack->rc_gp_saw_ca &&
	    (rack->rc_gp_saw_ss == 0) &&
	    rack_bw_can_be_raised(rack, cur_bw, last_bw_est,
				  rack->r_ctl.rack_per_of_gp_ca)) {
		/* In CA */
		calc = rack->r_ctl.rack_per_of_gp_ca + plus;
		if (calc > 0xffff)
			calc = 0xffff;
		logged |= 2;
		rack->r_ctl.rack_per_of_gp_ca = (uint16_t)calc;
		if (rack->r_ctl.rack_per_upper_bound_ca &&
		    (rack->rc_dragged_bottom == 0) &&
		    (rack->r_ctl.rack_per_of_gp_ca > rack->r_ctl.rack_per_upper_bound_ca))
			rack->r_ctl.rack_per_of_gp_ca = rack->r_ctl.rack_per_upper_bound_ca;
	}
	if (rack->rc_gp_saw_ss &&
	    rack_bw_can_be_raised(rack, cur_bw, last_bw_est,
				  rack->r_ctl.rack_per_of_gp_ss)) {
		/* In SS */
		calc = rack->r_ctl.rack_per_of_gp_ss + plus;
		if (calc > 0xffff)
			calc = 0xffff;
		rack->r_ctl.rack_per_of_gp_ss = (uint16_t)calc;
		if (rack->r_ctl.rack_per_upper_bound_ss &&
		    (rack->rc_dragged_bottom == 0) &&
		    (rack->r_ctl.rack_per_of_gp_ss > rack->r_ctl.rack_per_upper_bound_ss))
			rack->r_ctl.rack_per_of_gp_ss = rack->r_ctl.rack_per_upper_bound_ss;
		logged |= 4;
	}
	if (logged &&
	    (rack->rc_gp_incr == 0)){
		/* Go into increment mode */
		rack->rc_gp_incr = 1;
		rack->rc_gp_timely_inc_cnt = 0;
	}
	if (rack->rc_gp_incr &&
	    logged &&
	    (rack->rc_gp_timely_inc_cnt < RACK_TIMELY_CNT_BOOST)) {
		rack->rc_gp_timely_inc_cnt++;
	}
	rack_log_timely(rack,  logged, plus, 0, 0,
			__LINE__, 1);
}

static uint32_t
rack_get_decrease(struct tcp_rack *rack, uint32_t curper, int32_t rtt_diff)
{
	/*-
	 * norm_grad = rtt_diff / minrtt;
	 * new_per = curper * (1 - B * norm_grad)
	 *
	 * B = rack_gp_decrease_per (default 80%)
	 * rtt_dif = input var current rtt-diff
	 * curper = input var current percentage
	 * minrtt = from rack filter
	 *
	 * In order to do the floating point calculations above we
	 * do an integer conversion. The code looks confusing so let me
	 * translate it into something that use more variables and
	 * is clearer for us humans :)
	 *
	 * uint64_t norm_grad, inverse, reduce_by, final_result;
	 * uint32_t perf;
	 *
	 * norm_grad = (((uint64_t)rtt_diff * 1000000) /
	 *             (uint64_t)get_filter_small(&rack->r_ctl.rc_gp_min_rtt));
	 * inverse = ((uint64_t)rack_gp_decrease * (uint64_t)1000000) * norm_grad;
	 * inverse /= 1000000;
	 * reduce_by = (1000000 - inverse);
	 * final_result = (cur_per * reduce_by) / 1000000;
	 * perf = (uint32_t)final_result;
	 */
	uint64_t perf;

	perf = (((uint64_t)curper * ((uint64_t)1000000 -
		    ((uint64_t)rack_gp_decrease_per * (uint64_t)10000 *
		     (((uint64_t)rtt_diff * (uint64_t)1000000)/
		      (uint64_t)get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt)))/
		     (uint64_t)1000000)) /
		(uint64_t)1000000);
	if (perf > curper) {
		/* TSNH */
		perf = curper - 1;
	}
	return ((uint32_t)perf);
}

static uint32_t
rack_decrease_highrtt(struct tcp_rack *rack, uint32_t curper, uint32_t rtt)
{
	/*
	 *                                   highrttthresh
	 * result = curper * (1 - (B * ( 1 -  ------          ))
	 *                                     gp_srtt
	 *
	 * B = rack_gp_decrease_per (default .8 i.e. 80)
	 * highrttthresh = filter_min * rack_gp_rtt_maxmul
	 */
	uint64_t perf;
	uint32_t highrttthresh;

	highrttthresh = get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) * rack_gp_rtt_maxmul;

	perf = (((uint64_t)curper * ((uint64_t)1000000 -
				     ((uint64_t)rack_gp_decrease_per * ((uint64_t)1000000 -
					((uint64_t)highrttthresh * (uint64_t)1000000) /
						    (uint64_t)rtt)) / 100)) /(uint64_t)1000000);
	if (tcp_bblogging_on(rack->rc_tp)) {
		uint64_t log1;

		log1 = rtt;
		log1 <<= 32;
		log1 |= highrttthresh;
		rack_log_timely(rack,
				rack_gp_decrease_per,
				(uint64_t)curper,
				log1,
				perf,
				__LINE__,
				15);
	}
	return (perf);
}

static void
rack_decrease_bw_mul(struct tcp_rack *rack, int timely_says, uint32_t rtt, int32_t rtt_diff)
{
	uint64_t logvar, logvar2, logvar3;
	uint32_t logged, new_per, ss_red, ca_red, rec_red, alt, val;

	if (rack->rc_skip_timely)
		return;
	if (rack->rc_gp_incr) {
		/* Turn off increment counting */
		rack->rc_gp_incr = 0;
		rack->rc_gp_timely_inc_cnt = 0;
	}
	ss_red = ca_red = rec_red = 0;
	logged = 0;
	/* Calculate the reduction value */
	if (rtt_diff < 0) {
		rtt_diff *= -1;
	}
	/* Must be at least 1% reduction */
	if (rack->rc_gp_saw_rec && (rack->rc_gp_no_rec_chg == 0)) {
		/* We have been in recovery ding it too */
		if (timely_says == 2) {
			new_per = rack_decrease_highrtt(rack, rack->r_ctl.rack_per_of_gp_rec, rtt);
			alt = rack_get_decrease(rack, rack->r_ctl.rack_per_of_gp_rec, rtt_diff);
			if (alt < new_per)
				val = alt;
			else
				val = new_per;
		} else
			 val = new_per = alt = rack_get_decrease(rack, rack->r_ctl.rack_per_of_gp_rec, rtt_diff);
		if (rack->r_ctl.rack_per_of_gp_rec > val) {
			rec_red = (rack->r_ctl.rack_per_of_gp_rec - val);
			rack->r_ctl.rack_per_of_gp_rec = (uint16_t)val;
		} else {
			rack->r_ctl.rack_per_of_gp_rec = rack_per_lower_bound;
			rec_red = 0;
		}
		if (rack_per_lower_bound > rack->r_ctl.rack_per_of_gp_rec)
			rack->r_ctl.rack_per_of_gp_rec = rack_per_lower_bound;
		logged |= 1;
	}
	if (rack->rc_gp_saw_ss) {
		/* Sent in SS */
		if (timely_says == 2) {
			new_per = rack_decrease_highrtt(rack, rack->r_ctl.rack_per_of_gp_ss, rtt);
			alt = rack_get_decrease(rack, rack->r_ctl.rack_per_of_gp_ss, rtt_diff);
			if (alt < new_per)
				val = alt;
			else
				val = new_per;
		} else
			val = new_per = alt = rack_get_decrease(rack, rack->r_ctl.rack_per_of_gp_ss, rtt_diff);
		if (rack->r_ctl.rack_per_of_gp_ss > new_per) {
			ss_red = rack->r_ctl.rack_per_of_gp_ss - val;
			rack->r_ctl.rack_per_of_gp_ss = (uint16_t)val;
		} else {
			ss_red = new_per;
			rack->r_ctl.rack_per_of_gp_ss = rack_per_lower_bound;
			logvar = new_per;
			logvar <<= 32;
			logvar |= alt;
			logvar2 = (uint32_t)rtt;
			logvar2 <<= 32;
			logvar2 |= (uint32_t)rtt_diff;
			logvar3 = rack_gp_rtt_maxmul;
			logvar3 <<= 32;
			logvar3 |= get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt);
			rack_log_timely(rack, timely_says,
					logvar2, logvar3,
					logvar, __LINE__, 10);
		}
		if (rack_per_lower_bound > rack->r_ctl.rack_per_of_gp_ss)
			rack->r_ctl.rack_per_of_gp_ss = rack_per_lower_bound;
		logged |= 4;
	} else if (rack->rc_gp_saw_ca) {
		/* Sent in CA */
		if (timely_says == 2) {
			new_per = rack_decrease_highrtt(rack, rack->r_ctl.rack_per_of_gp_ca, rtt);
			alt = rack_get_decrease(rack, rack->r_ctl.rack_per_of_gp_ca, rtt_diff);
			if (alt < new_per)
				val = alt;
			else
				val = new_per;
		} else
			val = new_per = alt = rack_get_decrease(rack, rack->r_ctl.rack_per_of_gp_ca, rtt_diff);
		if (rack->r_ctl.rack_per_of_gp_ca > val) {
			ca_red = rack->r_ctl.rack_per_of_gp_ca - val;
			rack->r_ctl.rack_per_of_gp_ca = (uint16_t)val;
		} else {
			rack->r_ctl.rack_per_of_gp_ca = rack_per_lower_bound;
			ca_red = 0;
			logvar = new_per;
			logvar <<= 32;
			logvar |= alt;
			logvar2 = (uint32_t)rtt;
			logvar2 <<= 32;
			logvar2 |= (uint32_t)rtt_diff;
			logvar3 = rack_gp_rtt_maxmul;
			logvar3 <<= 32;
			logvar3 |= get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt);
			rack_log_timely(rack, timely_says,
					logvar2, logvar3,
					logvar, __LINE__, 10);
		}
		if (rack_per_lower_bound > rack->r_ctl.rack_per_of_gp_ca)
			rack->r_ctl.rack_per_of_gp_ca = rack_per_lower_bound;
		logged |= 2;
	}
	if (rack->rc_gp_timely_dec_cnt < 0x7) {
		rack->rc_gp_timely_dec_cnt++;
		if (rack_timely_dec_clear &&
		    (rack->rc_gp_timely_dec_cnt == rack_timely_dec_clear))
			rack->rc_gp_timely_dec_cnt = 0;
	}
	logvar = ss_red;
	logvar <<= 32;
	logvar |= ca_red;
	rack_log_timely(rack,  logged, rec_red, rack_per_lower_bound, logvar,
			__LINE__, 2);
}

static void
rack_log_rtt_shrinks(struct tcp_rack *rack, uint32_t us_cts,
		     uint32_t rtt, uint32_t line, uint8_t reas)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = line;
		log.u_bbr.flex2 = rack->r_ctl.rc_time_probertt_starts;
		log.u_bbr.flex3 = rack->r_ctl.rc_lower_rtt_us_cts;
		log.u_bbr.flex4 = rack->r_ctl.rack_per_of_gp_ss;
		log.u_bbr.flex5 = rtt;
		log.u_bbr.flex6 = rack->rc_highly_buffered;
		log.u_bbr.flex6 <<= 1;
		log.u_bbr.flex6 |= rack->forced_ack;
		log.u_bbr.flex6 <<= 1;
		log.u_bbr.flex6 |= rack->rc_gp_dyn_mul;
		log.u_bbr.flex6 <<= 1;
		log.u_bbr.flex6 |= rack->in_probe_rtt;
		log.u_bbr.flex6 <<= 1;
		log.u_bbr.flex6 |= rack->measure_saw_probe_rtt;
		log.u_bbr.flex7 = rack->r_ctl.rack_per_of_gp_probertt;
		log.u_bbr.pacing_gain = rack->r_ctl.rack_per_of_gp_ca;
		log.u_bbr.cwnd_gain = rack->r_ctl.rack_per_of_gp_rec;
		log.u_bbr.flex8 = reas;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.delRate = rack_get_bw(rack);
		log.u_bbr.cur_del_rate = rack->r_ctl.rc_highest_us_rtt;
		log.u_bbr.cur_del_rate <<= 32;
		log.u_bbr.cur_del_rate |= rack->r_ctl.rc_lowest_us_rtt;
		log.u_bbr.applimited = rack->r_ctl.rc_time_probertt_entered;
		log.u_bbr.pkts_out = rack->r_ctl.rc_rtt_diff;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.epoch = rack->r_ctl.rc_gp_srtt;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_prev_gp_srtt;
		log.u_bbr.pkt_epoch = rack->r_ctl.rc_lower_rtt_us_cts;
		log.u_bbr.delivered = rack->r_ctl.rc_target_probertt_flight;
		log.u_bbr.lost = get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt);
		log.u_bbr.rttProp = us_cts;
		log.u_bbr.rttProp <<= 32;
		log.u_bbr.rttProp |= rack->r_ctl.rc_entry_gp_rtt;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_RTT_SHRINKS, 0,
		    0, &log, false, &rack->r_ctl.act_rcv_time);
	}
}

static void
rack_set_prtt_target(struct tcp_rack *rack, uint32_t segsiz, uint32_t rtt)
{
	uint64_t bwdp;

	bwdp = rack_get_bw(rack);
	bwdp *= (uint64_t)rtt;
	bwdp /= (uint64_t)HPTS_USEC_IN_SEC;
	rack->r_ctl.rc_target_probertt_flight = roundup((uint32_t)bwdp, segsiz);
	if (rack->r_ctl.rc_target_probertt_flight < (segsiz * rack_timely_min_segs)) {
		/*
		 * A window protocol must be able to have 4 packets
		 * outstanding as the floor in order to function
		 * (especially considering delayed ack :D).
		 */
		rack->r_ctl.rc_target_probertt_flight = (segsiz * rack_timely_min_segs);
	}
}

static void
rack_enter_probertt(struct tcp_rack *rack, uint32_t us_cts)
{
	/**
	 * ProbeRTT is a bit different in rack_pacing than in
	 * BBR. It is like BBR in that it uses the lowering of
	 * the RTT as a signal that we saw something new and
	 * counts from there for how long between. But it is
	 * different in that its quite simple. It does not
	 * play with the cwnd and wait until we get down
	 * to N segments outstanding and hold that for
	 * 200ms. Instead it just sets the pacing reduction
	 * rate to a set percentage (70 by default) and hold
	 * that for a number of recent GP Srtt's.
	 */
	uint32_t segsiz;

	rack->r_ctl.rc_lower_rtt_us_cts = us_cts;
	if (rack->rc_gp_dyn_mul == 0)
		return;

	if (rack->rc_tp->snd_max == rack->rc_tp->snd_una) {
		/* We are idle */
		return;
	}
	if ((rack->rc_tp->t_flags & TF_GPUTINPROG) &&
	    SEQ_GT(rack->rc_tp->snd_una, rack->rc_tp->gput_seq)) {
		/*
		 * Stop the goodput now, the idea here is
		 * that future measurements with in_probe_rtt
		 * won't register if they are not greater so
		 * we want to get what info (if any) is available
		 * now.
		 */
		rack_do_goodput_measurement(rack->rc_tp, rack,
					    rack->rc_tp->snd_una, __LINE__,
					    RACK_QUALITY_PROBERTT);
	}
	rack->r_ctl.rack_per_of_gp_probertt = rack_per_of_gp_probertt;
	rack->r_ctl.rc_time_probertt_entered = us_cts;
	segsiz = min(ctf_fixed_maxseg(rack->rc_tp),
		     rack->r_ctl.rc_pace_min_segs);
	rack->in_probe_rtt = 1;
	rack->measure_saw_probe_rtt = 1;
	rack->r_ctl.rc_time_probertt_starts = 0;
	rack->r_ctl.rc_entry_gp_rtt = rack->r_ctl.rc_gp_srtt;
	if (rack_probertt_use_min_rtt_entry)
		rack_set_prtt_target(rack, segsiz, get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt));
	else
		rack_set_prtt_target(rack, segsiz, rack->r_ctl.rc_gp_srtt);
	rack_log_rtt_shrinks(rack,  us_cts,  get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt),
			     __LINE__, RACK_RTTS_ENTERPROBE);
}

static void
rack_exit_probertt(struct tcp_rack *rack, uint32_t us_cts)
{
	struct rack_sendmap *rsm;
	uint32_t segsiz;

	segsiz = min(ctf_fixed_maxseg(rack->rc_tp),
		     rack->r_ctl.rc_pace_min_segs);
	rack->in_probe_rtt = 0;
	if ((rack->rc_tp->t_flags & TF_GPUTINPROG) &&
	    SEQ_GT(rack->rc_tp->snd_una, rack->rc_tp->gput_seq)) {
		/*
		 * Stop the goodput now, the idea here is
		 * that future measurements with in_probe_rtt
		 * won't register if they are not greater so
		 * we want to get what info (if any) is available
		 * now.
		 */
		rack_do_goodput_measurement(rack->rc_tp, rack,
					    rack->rc_tp->snd_una, __LINE__,
					    RACK_QUALITY_PROBERTT);
	} else if (rack->rc_tp->t_flags & TF_GPUTINPROG) {
		/*
		 * We don't have enough data to make a measurement.
		 * So lets just stop and start here after exiting
		 * probe-rtt. We probably are not interested in
		 * the results anyway.
		 */
		rack->rc_tp->t_flags &= ~TF_GPUTINPROG;
	}
	/*
	 * Measurements through the current snd_max are going
	 * to be limited by the slower pacing rate.
	 *
	 * We need to mark these as app-limited so we
	 * don't collapse the b/w.
	 */
	rsm = tqhash_max(rack->r_ctl.tqh);
	if (rsm && ((rsm->r_flags & RACK_APP_LIMITED) == 0)) {
		if (rack->r_ctl.rc_app_limited_cnt == 0)
			rack->r_ctl.rc_end_appl = rack->r_ctl.rc_first_appl = rsm;
		else {
			/*
			 * Go out to the end app limited and mark
			 * this new one as next and move the end_appl up
			 * to this guy.
			 */
			if (rack->r_ctl.rc_end_appl)
				rack->r_ctl.rc_end_appl->r_nseq_appl = rsm->r_start;
			rack->r_ctl.rc_end_appl = rsm;
		}
		rsm->r_flags |= RACK_APP_LIMITED;
		rack->r_ctl.rc_app_limited_cnt++;
	}
	/*
	 * Now, we need to examine our pacing rate multipliers.
	 * If its under 100%, we need to kick it back up to
	 * 100%. We also don't let it be over our "max" above
	 * the actual rate i.e. 100% + rack_clamp_atexit_prtt.
	 * Note setting clamp_atexit_prtt to 0 has the effect
	 * of setting CA/SS to 100% always at exit (which is
	 * the default behavior).
	 */
	if (rack_probertt_clear_is) {
		rack->rc_gp_incr = 0;
		rack->rc_gp_bwred = 0;
		rack->rc_gp_timely_inc_cnt = 0;
		rack->rc_gp_timely_dec_cnt = 0;
	}
	/* Do we do any clamping at exit? */
	if (rack->rc_highly_buffered && rack_atexit_prtt_hbp) {
		rack->r_ctl.rack_per_of_gp_ca = rack_atexit_prtt_hbp;
		rack->r_ctl.rack_per_of_gp_ss = rack_atexit_prtt_hbp;
	}
	if ((rack->rc_highly_buffered == 0) && rack_atexit_prtt) {
		rack->r_ctl.rack_per_of_gp_ca = rack_atexit_prtt;
		rack->r_ctl.rack_per_of_gp_ss = rack_atexit_prtt;
	}
	/*
	 * Lets set rtt_diff to 0, so that we will get a "boost"
	 * after exiting.
	 */
	rack->r_ctl.rc_rtt_diff = 0;

	/* Clear all flags so we start fresh */
	rack->rc_tp->t_bytes_acked = 0;
	rack->rc_tp->t_ccv.flags &= ~CCF_ABC_SENTAWND;
	/*
	 * If configured to, set the cwnd and ssthresh to
	 * our targets.
	 */
	if (rack_probe_rtt_sets_cwnd) {
		uint64_t ebdp;
		uint32_t setto;

		/* Set ssthresh so we get into CA once we hit our target */
		if (rack_probertt_use_min_rtt_exit == 1) {
			/* Set to min rtt */
			rack_set_prtt_target(rack, segsiz,
					     get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt));
		} else if (rack_probertt_use_min_rtt_exit == 2) {
			/* Set to current gp rtt */
			rack_set_prtt_target(rack, segsiz,
					     rack->r_ctl.rc_gp_srtt);
		} else if (rack_probertt_use_min_rtt_exit == 3) {
			/* Set to entry gp rtt */
			rack_set_prtt_target(rack, segsiz,
					     rack->r_ctl.rc_entry_gp_rtt);
		} else {
			uint64_t sum;
			uint32_t setval;

			sum = rack->r_ctl.rc_entry_gp_rtt;
			sum *= 10;
			sum /= (uint64_t)(max(1, rack->r_ctl.rc_gp_srtt));
			if (sum >= 20) {
				/*
				 * A highly buffered path needs
				 * cwnd space for timely to work.
				 * Lets set things up as if
				 * we are heading back here again.
				 */
				setval = rack->r_ctl.rc_entry_gp_rtt;
			} else if (sum >= 15) {
				/*
				 * Lets take the smaller of the
				 * two since we are just somewhat
				 * buffered.
				 */
				setval = rack->r_ctl.rc_gp_srtt;
				if (setval > rack->r_ctl.rc_entry_gp_rtt)
					setval = rack->r_ctl.rc_entry_gp_rtt;
			} else {
				/*
				 * Here we are not highly buffered
				 * and should pick the min we can to
				 * keep from causing loss.
				 */
				setval = get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt);
			}
			rack_set_prtt_target(rack, segsiz,
					     setval);
		}
		if (rack_probe_rtt_sets_cwnd > 1) {
			/* There is a percentage here to boost */
			ebdp = rack->r_ctl.rc_target_probertt_flight;
			ebdp *= rack_probe_rtt_sets_cwnd;
			ebdp /= 100;
			setto = rack->r_ctl.rc_target_probertt_flight + ebdp;
		} else
			setto = rack->r_ctl.rc_target_probertt_flight;
		rack->rc_tp->snd_cwnd = roundup(setto, segsiz);
		if (rack->rc_tp->snd_cwnd < (segsiz * rack_timely_min_segs)) {
			/* Enforce a min */
			rack->rc_tp->snd_cwnd = segsiz * rack_timely_min_segs;
		}
		/* If we set in the cwnd also set the ssthresh point so we are in CA */
		rack->rc_tp->snd_ssthresh = (rack->rc_tp->snd_cwnd - 1);
	}
	rack_log_rtt_shrinks(rack,  us_cts,
			     get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt),
			     __LINE__, RACK_RTTS_EXITPROBE);
	/* Clear times last so log has all the info */
	rack->r_ctl.rc_probertt_sndmax_atexit = rack->rc_tp->snd_max;
	rack->r_ctl.rc_time_probertt_entered = us_cts;
	rack->r_ctl.rc_time_probertt_starts = rack->r_ctl.rc_lower_rtt_us_cts = us_cts;
	rack->r_ctl.rc_time_of_last_probertt = us_cts;
}

static void
rack_check_probe_rtt(struct tcp_rack *rack, uint32_t us_cts)
{
	/* Check in on probe-rtt */

	if (rack->rc_gp_filled == 0) {
		/* We do not do p-rtt unless we have gp measurements */
		return;
	}
	if (rack->in_probe_rtt) {
		uint64_t no_overflow;
		uint32_t endtime, must_stay;

		if (rack->r_ctl.rc_went_idle_time &&
		    ((us_cts - rack->r_ctl.rc_went_idle_time) > rack_min_probertt_hold)) {
			/*
			 * We went idle during prtt, just exit now.
			 */
			rack_exit_probertt(rack, us_cts);
		} else if (rack_probe_rtt_safety_val &&
		    TSTMP_GT(us_cts, rack->r_ctl.rc_time_probertt_entered) &&
		    ((us_cts - rack->r_ctl.rc_time_probertt_entered) > rack_probe_rtt_safety_val)) {
			/*
			 * Probe RTT safety value triggered!
			 */
			rack_log_rtt_shrinks(rack,  us_cts,
					     get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt),
					     __LINE__, RACK_RTTS_SAFETY);
			rack_exit_probertt(rack, us_cts);
		}
		/* Calculate the max we will wait */
		endtime = rack->r_ctl.rc_time_probertt_entered + (rack->r_ctl.rc_gp_srtt * rack_max_drain_wait);
		if (rack->rc_highly_buffered)
			endtime += (rack->r_ctl.rc_gp_srtt * rack_max_drain_hbp);
		/* Calculate the min we must wait */
		must_stay = rack->r_ctl.rc_time_probertt_entered + (rack->r_ctl.rc_gp_srtt * rack_must_drain);
		if ((ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked) > rack->r_ctl.rc_target_probertt_flight) &&
		    TSTMP_LT(us_cts, endtime)) {
			uint32_t calc;
			/* Do we lower more? */
no_exit:
			if (TSTMP_GT(us_cts, rack->r_ctl.rc_time_probertt_entered))
				calc = us_cts - rack->r_ctl.rc_time_probertt_entered;
			else
				calc = 0;
			calc /= max(rack->r_ctl.rc_gp_srtt, 1);
			if (calc) {
				/* Maybe */
				calc *= rack_per_of_gp_probertt_reduce;
				if (calc > rack_per_of_gp_probertt)
					rack->r_ctl.rack_per_of_gp_probertt = rack_per_of_gp_lowthresh;
				else
					rack->r_ctl.rack_per_of_gp_probertt = rack_per_of_gp_probertt - calc;
				/* Limit it too */
				if (rack->r_ctl.rack_per_of_gp_probertt < rack_per_of_gp_lowthresh)
					rack->r_ctl.rack_per_of_gp_probertt = rack_per_of_gp_lowthresh;
			}
			/* We must reach target or the time set */
			return;
		}
		if (rack->r_ctl.rc_time_probertt_starts == 0) {
			if ((TSTMP_LT(us_cts, must_stay) &&
			     rack->rc_highly_buffered) ||
			     (ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked) >
			      rack->r_ctl.rc_target_probertt_flight)) {
				/* We are not past the must_stay time */
				goto no_exit;
			}
			rack_log_rtt_shrinks(rack,  us_cts,
					     get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt),
					     __LINE__, RACK_RTTS_REACHTARGET);
			rack->r_ctl.rc_time_probertt_starts = us_cts;
			if (rack->r_ctl.rc_time_probertt_starts == 0)
				rack->r_ctl.rc_time_probertt_starts = 1;
			/* Restore back to our rate we want to pace at in prtt */
			rack->r_ctl.rack_per_of_gp_probertt = rack_per_of_gp_probertt;
		}
		/*
		 * Setup our end time, some number of gp_srtts plus 200ms.
		 */
		no_overflow = ((uint64_t)rack->r_ctl.rc_gp_srtt *
			       (uint64_t)rack_probertt_gpsrtt_cnt_mul);
		if (rack_probertt_gpsrtt_cnt_div)
			endtime = (uint32_t)(no_overflow / (uint64_t)rack_probertt_gpsrtt_cnt_div);
		else
			endtime = 0;
		endtime += rack_min_probertt_hold;
		endtime += rack->r_ctl.rc_time_probertt_starts;
		if (TSTMP_GEQ(us_cts,  endtime)) {
			/* yes, exit probertt */
			rack_exit_probertt(rack, us_cts);
		}

	} else if ((rack->rc_skip_timely == 0) &&
		   (TSTMP_GT(us_cts, rack->r_ctl.rc_lower_rtt_us_cts)) &&
		   ((us_cts - rack->r_ctl.rc_lower_rtt_us_cts) >= rack_time_between_probertt)) {
		/* Go into probertt, its been too long since we went lower */
		rack_enter_probertt(rack, us_cts);
	}
}

static void
rack_update_multiplier(struct tcp_rack *rack, int32_t timely_says, uint64_t last_bw_est,
		       uint32_t rtt, int32_t rtt_diff)
{
	uint64_t cur_bw, up_bnd, low_bnd, subfr;
	uint32_t losses;

	if ((rack->rc_gp_dyn_mul == 0) ||
	    (rack->use_fixed_rate) ||
	    (rack->in_probe_rtt) ||
	    (rack->rc_always_pace == 0)) {
		/* No dynamic GP multiplier in play */
		return;
	}
	losses = rack->r_ctl.rc_loss_count - rack->r_ctl.rc_loss_at_start;
	cur_bw = rack_get_bw(rack);
	/* Calculate our up and down range */
	up_bnd = rack->r_ctl.last_gp_comp_bw * (uint64_t)rack_gp_per_bw_mul_up;
	up_bnd /= 100;
	up_bnd += rack->r_ctl.last_gp_comp_bw;

	subfr = (uint64_t)rack->r_ctl.last_gp_comp_bw * (uint64_t)rack_gp_per_bw_mul_down;
	subfr /= 100;
	low_bnd = rack->r_ctl.last_gp_comp_bw - subfr;
	if ((timely_says == 2) && (rack->r_ctl.rc_no_push_at_mrtt)) {
		/*
		 * This is the case where our RTT is above
		 * the max target and we have been configured
		 * to just do timely no bonus up stuff in that case.
		 *
		 * There are two configurations, set to 1, and we
		 * just do timely if we are over our max. If its
		 * set above 1 then we slam the multipliers down
		 * to 100 and then decrement per timely.
		 */
		rack_log_timely(rack,  timely_says, cur_bw, low_bnd, up_bnd,
				__LINE__, 3);
		if (rack->r_ctl.rc_no_push_at_mrtt > 1)
			rack_validate_multipliers_at_or_below_100(rack);
		rack_decrease_bw_mul(rack, timely_says, rtt, rtt_diff);
	} else if ((timely_says != 0) && (last_bw_est < low_bnd) && !losses) {
		/*
		 * We are decreasing this is a bit complicated this
		 * means we are loosing ground. This could be
		 * because another flow entered and we are competing
		 * for b/w with it. This will push the RTT up which
		 * makes timely unusable unless we want to get shoved
		 * into a corner and just be backed off (the age
		 * old problem with delay based CC).
		 *
		 * On the other hand if it was a route change we
		 * would like to stay somewhat contained and not
		 * blow out the buffers.
		 */
		rack_log_timely(rack,  timely_says, cur_bw, low_bnd, up_bnd,
				__LINE__, 3);
		rack->r_ctl.last_gp_comp_bw = cur_bw;
		if (rack->rc_gp_bwred == 0) {
			/* Go into reduction counting */
			rack->rc_gp_bwred = 1;
			rack->rc_gp_timely_dec_cnt = 0;
		}
		if (rack->rc_gp_timely_dec_cnt < rack_timely_max_push_drop) {
			/*
			 * Push another time with a faster pacing
			 * to try to gain back (we include override to
			 * get a full raise factor).
			 */
			if ((rack->rc_gp_saw_ca && rack->r_ctl.rack_per_of_gp_ca <= rack_down_raise_thresh) ||
			    (rack->rc_gp_saw_ss && rack->r_ctl.rack_per_of_gp_ss <= rack_down_raise_thresh) ||
			    (timely_says == 0) ||
			    (rack_down_raise_thresh == 0)) {
				/*
				 * Do an override up in b/w if we were
				 * below the threshold or if the threshold
				 * is zero we always do the raise.
				 */
				rack_increase_bw_mul(rack, timely_says, cur_bw, last_bw_est, 1);
			} else {
				/* Log it stays the same */
				rack_log_timely(rack,  0, last_bw_est, low_bnd, 0,
						__LINE__, 11);
			}
			rack->rc_gp_timely_dec_cnt++;
			/* We are not incrementing really no-count */
			rack->rc_gp_incr = 0;
			rack->rc_gp_timely_inc_cnt = 0;
		} else {
			/*
			 * Lets just use the RTT
			 * information and give up
			 * pushing.
			 */
			goto use_timely;
		}
	} else if ((timely_says != 2) &&
		    !losses &&
		    (last_bw_est > up_bnd)) {
		/*
		 * We are increasing b/w lets keep going, updating
		 * our b/w and ignoring any timely input, unless
		 * of course we are at our max raise (if there is one).
		 */

		rack_log_timely(rack,  timely_says, cur_bw, low_bnd, up_bnd,
				__LINE__, 3);
		rack->r_ctl.last_gp_comp_bw = cur_bw;
		if (rack->rc_gp_saw_ss &&
		    rack->r_ctl.rack_per_upper_bound_ss &&
		     (rack->r_ctl.rack_per_of_gp_ss == rack->r_ctl.rack_per_upper_bound_ss)) {
			    /*
			     * In cases where we can't go higher
			     * we should just use timely.
			     */
			    goto use_timely;
		}
		if (rack->rc_gp_saw_ca &&
		    rack->r_ctl.rack_per_upper_bound_ca &&
		    (rack->r_ctl.rack_per_of_gp_ca == rack->r_ctl.rack_per_upper_bound_ca)) {
			    /*
			     * In cases where we can't go higher
			     * we should just use timely.
			     */
			    goto use_timely;
		}
		rack->rc_gp_bwred = 0;
		rack->rc_gp_timely_dec_cnt = 0;
		/* You get a set number of pushes if timely is trying to reduce */
		if ((rack->rc_gp_incr < rack_timely_max_push_rise) || (timely_says == 0)) {
			rack_increase_bw_mul(rack, timely_says, cur_bw, last_bw_est, 0);
		} else {
			/* Log it stays the same */
			rack_log_timely(rack,  0, last_bw_est, up_bnd, 0,
			    __LINE__, 12);
		}
		return;
	} else {
		/*
		 * We are staying between the lower and upper range bounds
		 * so use timely to decide.
		 */
		rack_log_timely(rack,  timely_says, cur_bw, low_bnd, up_bnd,
				__LINE__, 3);
use_timely:
		if (timely_says) {
			rack->rc_gp_incr = 0;
			rack->rc_gp_timely_inc_cnt = 0;
			if ((rack->rc_gp_timely_dec_cnt < rack_timely_max_push_drop) &&
			    !losses &&
			    (last_bw_est < low_bnd)) {
				/* We are loosing ground */
				rack_increase_bw_mul(rack, timely_says, cur_bw, last_bw_est, 0);
				rack->rc_gp_timely_dec_cnt++;
				/* We are not incrementing really no-count */
				rack->rc_gp_incr = 0;
				rack->rc_gp_timely_inc_cnt = 0;
			} else
				rack_decrease_bw_mul(rack, timely_says, rtt, rtt_diff);
		} else {
			rack->rc_gp_bwred = 0;
			rack->rc_gp_timely_dec_cnt = 0;
			rack_increase_bw_mul(rack, timely_says, cur_bw, last_bw_est, 0);
		}
	}
}

static int32_t
rack_make_timely_judgement(struct tcp_rack *rack, uint32_t rtt, int32_t rtt_diff, uint32_t prev_rtt)
{
	int32_t timely_says;
	uint64_t log_mult, log_rtt_a_diff;

	log_rtt_a_diff = rtt;
	log_rtt_a_diff <<= 32;
	log_rtt_a_diff |= (uint32_t)rtt_diff;
	if (rtt >= (get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) *
		    rack_gp_rtt_maxmul)) {
		/* Reduce the b/w multiplier */
		timely_says = 2;
		log_mult = get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) * rack_gp_rtt_maxmul;
		log_mult <<= 32;
		log_mult |= prev_rtt;
		rack_log_timely(rack,  timely_says, log_mult,
				get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt),
				log_rtt_a_diff, __LINE__, 4);
	} else if (rtt <= (get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) +
			   ((get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) * rack_gp_rtt_minmul) /
			    max(rack_gp_rtt_mindiv , 1)))) {
		/* Increase the b/w multiplier */
		log_mult = get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) +
			((get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) * rack_gp_rtt_minmul) /
			 max(rack_gp_rtt_mindiv , 1));
		log_mult <<= 32;
		log_mult |= prev_rtt;
		timely_says = 0;
		rack_log_timely(rack,  timely_says, log_mult ,
				get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt),
				log_rtt_a_diff, __LINE__, 5);
	} else {
		/*
		 * Use a gradient to find it the timely gradient
		 * is:
		 * grad = rc_rtt_diff / min_rtt;
		 *
		 * anything below or equal to 0 will be
		 * a increase indication. Anything above
		 * zero is a decrease. Note we take care
		 * of the actual gradient calculation
		 * in the reduction (its not needed for
		 * increase).
		 */
		log_mult = prev_rtt;
		if (rtt_diff <= 0) {
			/*
			 * Rttdiff is less than zero, increase the
			 * b/w multiplier (its 0 or negative)
			 */
			timely_says = 0;
			rack_log_timely(rack,  timely_says, log_mult,
					get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt), log_rtt_a_diff, __LINE__, 6);
		} else {
			/* Reduce the b/w multiplier */
			timely_says = 1;
			rack_log_timely(rack,  timely_says, log_mult,
					get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt), log_rtt_a_diff, __LINE__, 7);
		}
	}
	return (timely_says);
}

static __inline int
rack_in_gp_window(struct tcpcb *tp, struct rack_sendmap *rsm)
{
	if (SEQ_GEQ(rsm->r_start, tp->gput_seq) &&
	    SEQ_LEQ(rsm->r_end, tp->gput_ack)) {
		/**
		 * This covers the case that the
		 * resent is completely inside
		 * the gp range or up to it.
		 *      |----------------|
		 *      |-----| <or>
		 *            |----|
		 *            <or>   |---|
		 */
		return (1);
	} else if (SEQ_LT(rsm->r_start, tp->gput_seq) &&
		   SEQ_GT(rsm->r_end, tp->gput_seq)){
		/**
		 * This covers the case of
		 *      |--------------|
		 *  |-------->|
		 */
		return (1);
	} else if (SEQ_GEQ(rsm->r_start, tp->gput_seq) &&
		   SEQ_LT(rsm->r_start, tp->gput_ack) &&
		   SEQ_GEQ(rsm->r_end, tp->gput_ack)) {

		/**
		 * This covers the case of
		 *      |--------------|
		 *              |-------->|
		 */
		return (1);
	}
	return (0);
}

static __inline void
rack_mark_in_gp_win(struct tcpcb *tp, struct rack_sendmap *rsm)
{

	if ((tp->t_flags & TF_GPUTINPROG) == 0)
		return;
	/*
	 * We have a Goodput measurement in progress. Mark
	 * the send if its within the window. If its not
	 * in the window make sure it does not have the mark.
	 */
	if (rack_in_gp_window(tp, rsm))
		rsm->r_flags |= RACK_IN_GP_WIN;
	else
		rsm->r_flags &= ~RACK_IN_GP_WIN;
}

static __inline void
rack_clear_gp_marks(struct tcpcb *tp, struct tcp_rack *rack)
{
	/* A GP measurement is ending, clear all marks on the send map*/
	struct rack_sendmap *rsm = NULL;

	rsm = tqhash_find(rack->r_ctl.tqh, tp->gput_seq);
	if (rsm == NULL) {
		rsm = tqhash_min(rack->r_ctl.tqh);
	}
	/* Nothing left? */
	while ((rsm != NULL) && (SEQ_GEQ(tp->gput_ack, rsm->r_start))){
		rsm->r_flags &= ~RACK_IN_GP_WIN;
		rsm = tqhash_next(rack->r_ctl.tqh, rsm);
	}
}


static __inline void
rack_tend_gp_marks(struct tcpcb *tp, struct tcp_rack *rack)
{
	struct rack_sendmap *rsm = NULL;

	if (tp->snd_una == tp->snd_max) {
		/* Nothing outstanding yet, nothing to do here */
		return;
	}
	if (SEQ_GT(tp->gput_seq, tp->snd_una)) {
		/*
		 * We are measuring ahead of some outstanding
		 * data. We need to walk through up until we get
		 * to gp_seq marking so that no rsm is set incorrectly
		 * with RACK_IN_GP_WIN.
		 */
		rsm = tqhash_min(rack->r_ctl.tqh);
		while (rsm != NULL) {
			rack_mark_in_gp_win(tp, rsm);
			if (SEQ_GEQ(rsm->r_end, tp->gput_seq))
				break;
			rsm = tqhash_next(rack->r_ctl.tqh, rsm);
		}
	}
	if (rsm == NULL) {
		/*
		 * Need to find the GP seq, if rsm is
		 * set we stopped as we hit it.
		 */
		rsm = tqhash_find(rack->r_ctl.tqh, tp->gput_seq);
		if (rsm == NULL)
			return;
		rack_mark_in_gp_win(tp, rsm);
	}
	/*
	 * Now we may need to mark already sent rsm, ahead of
	 * gput_seq in the window since they may have been sent
	 * *before* we started our measurment. The rsm, if non-null
	 * has been marked (note if rsm would have been NULL we would have
	 * returned in the previous block). So we go to the next, and continue
	 * until we run out of entries or we exceed the gp_ack value.
	 */
	rsm = tqhash_next(rack->r_ctl.tqh, rsm);
	while (rsm) {
		rack_mark_in_gp_win(tp, rsm);
		if (SEQ_GT(rsm->r_end, tp->gput_ack))
			break;
		rsm = tqhash_next(rack->r_ctl.tqh, rsm);
	}
}

static void
rack_log_gp_calc(struct tcp_rack *rack, uint32_t add_part, uint32_t sub_part, uint32_t srtt, uint64_t meas_bw, uint64_t utim, uint8_t meth, uint32_t line)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = add_part;
		log.u_bbr.flex2 = sub_part;
		log.u_bbr.flex3 = rack_wma_divisor;
		log.u_bbr.flex4 = srtt;
		log.u_bbr.flex7 = (uint16_t)line;
		log.u_bbr.flex8 = meth;
		log.u_bbr.delRate = rack->r_ctl.gp_bw;
		log.u_bbr.cur_del_rate = meas_bw;
		log.u_bbr.rttProp = utim;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_THRESH_CALC, 0,
		    0, &log, false, &rack->r_ctl.act_rcv_time);
	}
}

static void
rack_do_goodput_measurement(struct tcpcb *tp, struct tcp_rack *rack,
			    tcp_seq th_ack, int line, uint8_t quality)
{
	uint64_t tim, bytes_ps, stim, utim;
	uint32_t segsiz, bytes, reqbytes, us_cts;
	int32_t gput, new_rtt_diff, timely_says;
	uint64_t  resid_bw, subpart = 0, addpart = 0, srtt;
	int did_add = 0;

	us_cts = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	if (TSTMP_GEQ(us_cts, tp->gput_ts))
		tim = us_cts - tp->gput_ts;
	else
		tim = 0;
	if (rack->r_ctl.rc_gp_cumack_ts > rack->r_ctl.rc_gp_output_ts)
		stim = rack->r_ctl.rc_gp_cumack_ts - rack->r_ctl.rc_gp_output_ts;
	else
		stim = 0;
	/*
	 * Use the larger of the send time or ack time. This prevents us
	 * from being influenced by ack artifacts to come up with too
	 * high of measurement. Note that since we are spanning over many more
	 * bytes in most of our measurements hopefully that is less likely to
	 * occur.
	 */
	if (tim > stim)
		utim = max(tim, 1);
	else
		utim = max(stim, 1);
	reqbytes = min(rc_init_window(rack), (MIN_GP_WIN * segsiz));
	rack_log_gpset(rack, th_ack, us_cts, rack->r_ctl.rc_gp_cumack_ts, __LINE__, 3, NULL);
	if ((tim == 0) && (stim == 0)) {
		/*
		 * Invalid measurement time, maybe
		 * all on one ack/one send?
		 */
		bytes = 0;
		bytes_ps = 0;
		rack_log_pacing_delay_calc(rack, bytes_ps, reqbytes,
					   0, 0, 0, 10, __LINE__, NULL, quality);
		goto skip_measurement;
	}
	if (rack->r_ctl.rc_gp_lowrtt == 0xffffffff) {
		/* We never made a us_rtt measurement? */
		bytes = 0;
		bytes_ps = 0;
		rack_log_pacing_delay_calc(rack, bytes_ps, reqbytes,
					   0, 0, 0, 10, __LINE__, NULL, quality);
		goto skip_measurement;
	}
	/*
	 * Calculate the maximum possible b/w this connection
	 * could have. We base our calculation on the lowest
	 * rtt we have seen during the measurement and the
	 * largest rwnd the client has given us in that time. This
	 * forms a BDP that is the maximum that we could ever
	 * get to the client. Anything larger is not valid.
	 *
	 * I originally had code here that rejected measurements
	 * where the time was less than 1/2 the latest us_rtt.
	 * But after thinking on that I realized its wrong since
	 * say you had a 150Mbps or even 1Gbps link, and you
	 * were a long way away.. example I am in Europe (100ms rtt)
	 * talking to my 1Gbps link in S.C. Now measuring say 150,000
	 * bytes my time would be 1.2ms, and yet my rtt would say
	 * the measurement was invalid the time was < 50ms. The
	 * same thing is true for 150Mb (8ms of time).
	 *
	 * A better way I realized is to look at what the maximum
	 * the connection could possibly do. This is gated on
	 * the lowest RTT we have seen and the highest rwnd.
	 * We should in theory never exceed that, if we are
	 * then something on the path is storing up packets
	 * and then feeding them all at once to our endpoint
	 * messing up our measurement.
	 */
	rack->r_ctl.last_max_bw = rack->r_ctl.rc_gp_high_rwnd;
	rack->r_ctl.last_max_bw *= HPTS_USEC_IN_SEC;
	rack->r_ctl.last_max_bw /= rack->r_ctl.rc_gp_lowrtt;
	if (SEQ_LT(th_ack, tp->gput_seq)) {
		/* No measurement can be made */
		bytes = 0;
		bytes_ps = 0;
		rack_log_pacing_delay_calc(rack, bytes_ps, reqbytes,
					   0, 0, 0, 10, __LINE__, NULL, quality);
		goto skip_measurement;
	} else
		bytes = (th_ack - tp->gput_seq);
	bytes_ps = (uint64_t)bytes;
	/*
	 * Don't measure a b/w for pacing unless we have gotten at least
	 * an initial windows worth of data in this measurement interval.
	 *
	 * Small numbers of bytes get badly influenced by delayed ack and
	 * other artifacts. Note we take the initial window or our
	 * defined minimum GP (defaulting to 10 which hopefully is the
	 * IW).
	 */
	if (rack->rc_gp_filled == 0) {
		/*
		 * The initial estimate is special. We
		 * have blasted out an IW worth of packets
		 * without a real valid ack ts results. We
		 * then setup the app_limited_needs_set flag,
		 * this should get the first ack in (probably 2
		 * MSS worth) to be recorded as the timestamp.
		 * We thus allow a smaller number of bytes i.e.
		 * IW - 2MSS.
		 */
		reqbytes -= (2 * segsiz);
		/* Also lets fill previous for our first measurement to be neutral */
		rack->r_ctl.rc_prev_gp_srtt = rack->r_ctl.rc_gp_srtt;
	}
	if ((bytes_ps < reqbytes) || rack->app_limited_needs_set) {
		rack_log_pacing_delay_calc(rack, bytes_ps, reqbytes,
					   rack->r_ctl.rc_app_limited_cnt,
					   0, 0, 10, __LINE__, NULL, quality);
		goto skip_measurement;
	}
	/*
	 * We now need to calculate the Timely like status so
	 * we can update (possibly) the b/w multipliers.
	 */
	new_rtt_diff = (int32_t)rack->r_ctl.rc_gp_srtt - (int32_t)rack->r_ctl.rc_prev_gp_srtt;
	if (rack->rc_gp_filled == 0) {
		/* No previous reading */
		rack->r_ctl.rc_rtt_diff = new_rtt_diff;
	} else {
		if (rack->measure_saw_probe_rtt == 0) {
			/*
			 * We don't want a probertt to be counted
			 * since it will be negative incorrectly. We
			 * expect to be reducing the RTT when we
			 * pace at a slower rate.
			 */
			rack->r_ctl.rc_rtt_diff -= (rack->r_ctl.rc_rtt_diff / 8);
			rack->r_ctl.rc_rtt_diff += (new_rtt_diff / 8);
		}
	}
	timely_says = rack_make_timely_judgement(rack,
	    rack->r_ctl.rc_gp_srtt,
	    rack->r_ctl.rc_rtt_diff,
	    rack->r_ctl.rc_prev_gp_srtt
	);
	bytes_ps *= HPTS_USEC_IN_SEC;
	bytes_ps /= utim;
	if (bytes_ps > rack->r_ctl.last_max_bw) {
		/*
		 * Something is on path playing
		 * since this b/w is not possible based
		 * on our BDP (highest rwnd and lowest rtt
		 * we saw in the measurement window).
		 *
		 * Another option here would be to
		 * instead skip the measurement.
		 */
		rack_log_pacing_delay_calc(rack, bytes, reqbytes,
					   bytes_ps, rack->r_ctl.last_max_bw, 0,
					   11, __LINE__, NULL, quality);
		bytes_ps = rack->r_ctl.last_max_bw;
	}
	/* We store gp for b/w in bytes per second */
	if (rack->rc_gp_filled == 0) {
		/* Initial measurement */
		if (bytes_ps) {
			rack->r_ctl.gp_bw = bytes_ps;
			rack->rc_gp_filled = 1;
			rack->r_ctl.num_measurements = 1;
			rack_set_pace_segments(rack->rc_tp, rack, __LINE__, NULL);
		} else {
			rack_log_pacing_delay_calc(rack, bytes_ps, reqbytes,
						   rack->r_ctl.rc_app_limited_cnt,
						   0, 0, 10, __LINE__, NULL, quality);
		}
		if (tcp_in_hpts(rack->rc_tp) &&
		    (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT)) {
			/*
			 * Ok we can't trust the pacer in this case
			 * where we transition from un-paced to paced.
			 * Or for that matter when the burst mitigation
			 * was making a wild guess and got it wrong.
			 * Stop the pacer and clear up all the aggregate
			 * delays etc.
			 */
			tcp_hpts_remove(rack->rc_tp);
			rack->r_ctl.rc_hpts_flags = 0;
			rack->r_ctl.rc_last_output_to = 0;
		}
		did_add = 2;
	} else if (rack->r_ctl.num_measurements < RACK_REQ_AVG) {
		/* Still a small number run an average */
		rack->r_ctl.gp_bw += bytes_ps;
		addpart = rack->r_ctl.num_measurements;
		rack->r_ctl.num_measurements++;
		if (rack->r_ctl.num_measurements >= RACK_REQ_AVG) {
			/* We have collected enough to move forward */
			rack->r_ctl.gp_bw /= (uint64_t)rack->r_ctl.num_measurements;
		}
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		did_add = 3;
	} else {
		/*
		 * We want to take 1/wma of the goodput and add in to 7/8th
		 * of the old value weighted by the srtt. So if your measurement
		 * period is say 2 SRTT's long you would get 1/4 as the
		 * value, if it was like 1/2 SRTT then you would get 1/16th.
		 *
		 * But we must be careful not to take too much i.e. if the
		 * srtt is say 20ms and the measurement is taken over
		 * 400ms our weight would be 400/20 i.e. 20. On the
		 * other hand if we get a measurement over 1ms with a
		 * 10ms rtt we only want to take a much smaller portion.
		 */
		uint8_t meth;

		if (rack->r_ctl.num_measurements < 0xff) {
			rack->r_ctl.num_measurements++;
		}
		srtt = (uint64_t)tp->t_srtt;
		if (srtt == 0) {
			/*
			 * Strange why did t_srtt go back to zero?
			 */
			if (rack->r_ctl.rc_rack_min_rtt)
				srtt = rack->r_ctl.rc_rack_min_rtt;
			else
				srtt = HPTS_USEC_IN_MSEC;
		}
		/*
		 * XXXrrs: Note for reviewers, in playing with
		 * dynamic pacing I discovered this GP calculation
		 * as done originally leads to some undesired results.
		 * Basically you can get longer measurements contributing
		 * too much to the WMA. Thus I changed it if you are doing
		 * dynamic adjustments to only do the aportioned adjustment
		 * if we have a very small (time wise) measurement. Longer
		 * measurements just get there weight (defaulting to 1/8)
		 * add to the WMA. We may want to think about changing
		 * this to always do that for both sides i.e. dynamic
		 * and non-dynamic... but considering lots of folks
		 * were playing with this I did not want to change the
		 * calculation per.se. without your thoughts.. Lawerence?
		 * Peter??
		 */
		if (rack->rc_gp_dyn_mul == 0) {
			subpart = rack->r_ctl.gp_bw * utim;
			subpart /= (srtt * 8);
			if (subpart < (rack->r_ctl.gp_bw / 2)) {
				/*
				 * The b/w update takes no more
				 * away then 1/2 our running total
				 * so factor it in.
				 */
				addpart = bytes_ps * utim;
				addpart /= (srtt * 8);
				meth = 1;
			} else {
				/*
				 * Don't allow a single measurement
				 * to account for more than 1/2 of the
				 * WMA. This could happen on a retransmission
				 * where utim becomes huge compared to
				 * srtt (multiple retransmissions when using
				 * the sending rate which factors in all the
				 * transmissions from the first one).
				 */
				subpart = rack->r_ctl.gp_bw / 2;
				addpart = bytes_ps / 2;
				meth = 2;
			}
			rack_log_gp_calc(rack, addpart, subpart, srtt, bytes_ps, utim, meth, __LINE__);
			resid_bw = rack->r_ctl.gp_bw - subpart;
			rack->r_ctl.gp_bw = resid_bw + addpart;
			did_add = 1;
		} else {
			if ((utim / srtt) <= 1) {
				/*
				 * The b/w update was over a small period
				 * of time. The idea here is to prevent a small
				 * measurement time period from counting
				 * too much. So we scale it based on the
				 * time so it attributes less than 1/rack_wma_divisor
				 * of its measurement.
				 */
				subpart = rack->r_ctl.gp_bw * utim;
				subpart /= (srtt * rack_wma_divisor);
				addpart = bytes_ps * utim;
				addpart /= (srtt * rack_wma_divisor);
				meth = 3;
			} else {
				/*
				 * The scaled measurement was long
				 * enough so lets just add in the
				 * portion of the measurement i.e. 1/rack_wma_divisor
				 */
				subpart = rack->r_ctl.gp_bw / rack_wma_divisor;
				addpart = bytes_ps / rack_wma_divisor;
				meth = 4;
			}
			if ((rack->measure_saw_probe_rtt == 0) ||
		            (bytes_ps > rack->r_ctl.gp_bw)) {
				/*
				 * For probe-rtt we only add it in
				 * if its larger, all others we just
				 * add in.
				 */
				did_add = 1;
				rack_log_gp_calc(rack, addpart, subpart, srtt, bytes_ps, utim, meth, __LINE__);
				resid_bw = rack->r_ctl.gp_bw - subpart;
				rack->r_ctl.gp_bw = resid_bw + addpart;
			}
		}
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
	}
	/*
	 * We only watch the growth of the GP during the initial startup
	 * or first-slowstart that ensues. If we ever needed to watch
	 * growth of gp outside of that period all we need to do is
	 * remove the first clause of this if (rc_initial_ss_comp).
	 */
	if ((rack->rc_initial_ss_comp == 0) &&
	    (rack->r_ctl.num_measurements >= RACK_REQ_AVG)) {
		uint64_t gp_est;

		gp_est = bytes_ps;
		if (tcp_bblogging_on(rack->rc_tp)) {
			union tcp_log_stackspecific log;
			struct timeval tv;

			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.timeStamp = tcp_get_usecs(&tv);
			log.u_bbr.flex1 = rack->r_ctl.current_round;
			log.u_bbr.flex2 = rack->r_ctl.last_rnd_of_gp_rise;
			log.u_bbr.delRate = gp_est;
			log.u_bbr.cur_del_rate = rack->r_ctl.last_gpest;
			log.u_bbr.flex8 = 41;
			(void)tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_CWND, 0,
					    0, &log, false, NULL, __func__, __LINE__,&tv);
		}
		if ((rack->r_ctl.num_measurements == RACK_REQ_AVG) ||
		    (rack->r_ctl.last_gpest == 0)) {
			/*
			 * The round we get our measurement averaging going
			 * is the base round so it always is the source point
			 * for when we had our first increment. From there on
			 * we only record the round that had a rise.
			 */
			rack->r_ctl.last_rnd_of_gp_rise = rack->r_ctl.current_round;
			rack->r_ctl.last_gpest = rack->r_ctl.gp_bw;
		} else if (gp_est >= rack->r_ctl.last_gpest) {
			/*
			 * Test to see if its gone up enough
			 * to set the round count up to now. Note
			 * that on the seeding of the 4th measurement we
			 */
			gp_est *= 1000;
			gp_est /= rack->r_ctl.last_gpest;
			if ((uint32_t)gp_est > rack->r_ctl.gp_gain_req) {
				/*
				 * We went up enough to record the round.
				 */
				if (tcp_bblogging_on(rack->rc_tp)) {
					union tcp_log_stackspecific log;
					struct timeval tv;

					memset(&log.u_bbr, 0, sizeof(log.u_bbr));
					log.u_bbr.timeStamp = tcp_get_usecs(&tv);
					log.u_bbr.flex1 = rack->r_ctl.current_round;
					log.u_bbr.flex2 = (uint32_t)gp_est;
					log.u_bbr.flex3 = rack->r_ctl.gp_gain_req;
					log.u_bbr.delRate = gp_est;
					log.u_bbr.cur_del_rate = rack->r_ctl.last_gpest;
					log.u_bbr.flex8 = 42;
					(void)tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_CWND, 0,
							    0, &log, false, NULL, __func__, __LINE__,&tv);
				}
				rack->r_ctl.last_rnd_of_gp_rise = rack->r_ctl.current_round;
				if (rack->r_ctl.use_gp_not_last == 1)
					rack->r_ctl.last_gpest = rack->r_ctl.gp_bw;
				else
					rack->r_ctl.last_gpest = bytes_ps;
			}
		}
	}
	if ((rack->gp_ready == 0) &&
	    (rack->r_ctl.num_measurements >= rack->r_ctl.req_measurements)) {
		/* We have enough measurements now */
		rack->gp_ready = 1;
		if (rack->dgp_on ||
		    rack->rack_hibeta)
			rack_set_cc_pacing(rack);
		if (rack->defer_options)
			rack_apply_deferred_options(rack);
	}
	rack_log_pacing_delay_calc(rack, subpart, addpart, bytes_ps, stim,
				   rack_get_bw(rack), 22, did_add, NULL, quality);
	/* We do not update any multipliers if we are in or have seen a probe-rtt */

	if ((rack->measure_saw_probe_rtt == 0) &&
	    rack->rc_gp_rtt_set) {
		if (rack->rc_skip_timely == 0) {
			rack_update_multiplier(rack, timely_says, bytes_ps,
					       rack->r_ctl.rc_gp_srtt,
					       rack->r_ctl.rc_rtt_diff);
		}
	}
	rack_log_pacing_delay_calc(rack, bytes, tim, bytes_ps, stim,
				   rack_get_bw(rack), 3, line, NULL, quality);
	rack_log_pacing_delay_calc(rack,
				   bytes, /* flex2 */
				   tim, /* flex1 */
				   bytes_ps, /* bw_inuse */
				   rack->r_ctl.gp_bw, /* delRate */
				   rack_get_lt_bw(rack), /* rttProp */
				   20, line, NULL, 0);
	/* reset the gp srtt and setup the new prev */
	rack->r_ctl.rc_prev_gp_srtt = rack->r_ctl.rc_gp_srtt;
	/* Record the lost count for the next measurement */
	rack->r_ctl.rc_loss_at_start = rack->r_ctl.rc_loss_count;
skip_measurement:
	/*
	 * We restart our diffs based on the gpsrtt in the
	 * measurement window.
	 */
	rack->rc_gp_rtt_set = 0;
	rack->rc_gp_saw_rec = 0;
	rack->rc_gp_saw_ca = 0;
	rack->rc_gp_saw_ss = 0;
	rack->rc_dragged_bottom = 0;
	if (quality == RACK_QUALITY_HIGH) {
		/*
		 * Gput in the stats world is in kbps where bytes_ps is
		 * bytes per second so we do ((x * 8)/ 1000).
		 */
		gput = (int32_t)((bytes_ps << 3) / (uint64_t)1000);
#ifdef STATS
		stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_GPUT,
					 gput);
		/*
		 * XXXLAS: This is a temporary hack, and should be
		 * chained off VOI_TCP_GPUT when stats(9) grows an
		 * API to deal with chained VOIs.
		 */
		if (tp->t_stats_gput_prev > 0)
			stats_voi_update_abs_s32(tp->t_stats,
						 VOI_TCP_GPUT_ND,
						 ((gput - tp->t_stats_gput_prev) * 100) /
						 tp->t_stats_gput_prev);
#endif
		tp->t_stats_gput_prev = gput;
	}
	tp->t_flags &= ~TF_GPUTINPROG;
	/*
	 * Now are we app limited now and there is space from where we
	 * were to where we want to go?
	 *
	 * We don't do the other case i.e. non-applimited here since
	 * the next send will trigger us picking up the missing data.
	 */
	if (rack->r_ctl.rc_first_appl &&
	    TCPS_HAVEESTABLISHED(tp->t_state) &&
	    rack->r_ctl.rc_app_limited_cnt &&
	    (SEQ_GT(rack->r_ctl.rc_first_appl->r_start, th_ack)) &&
	    ((rack->r_ctl.rc_first_appl->r_end - th_ack) >
	     max(rc_init_window(rack), (MIN_GP_WIN * segsiz)))) {
		/*
		 * Yep there is enough outstanding to make a measurement here.
		 */
		struct rack_sendmap *rsm;

		rack->r_ctl.rc_gp_lowrtt = 0xffffffff;
		rack->r_ctl.rc_gp_high_rwnd = rack->rc_tp->snd_wnd;
		tp->gput_ts = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
		rack->app_limited_needs_set = 0;
		tp->gput_seq = th_ack;
		if (rack->in_probe_rtt)
			rack->measure_saw_probe_rtt = 1;
		else if ((rack->measure_saw_probe_rtt) &&
			 (SEQ_GEQ(tp->gput_seq, rack->r_ctl.rc_probertt_sndmax_atexit)))
			rack->measure_saw_probe_rtt = 0;
		if ((rack->r_ctl.rc_first_appl->r_end - th_ack) >= rack_get_measure_window(tp, rack)) {
			/* There is a full window to gain info from */
			tp->gput_ack = tp->gput_seq + rack_get_measure_window(tp, rack);
		} else {
			/* We can only measure up to the applimited point */
			tp->gput_ack = tp->gput_seq + (rack->r_ctl.rc_first_appl->r_end - th_ack);
			if ((tp->gput_ack - tp->gput_seq) < (MIN_GP_WIN * segsiz)) {
				/*
				 * We don't have enough to make a measurement.
				 */
				tp->t_flags &= ~TF_GPUTINPROG;
				rack_log_pacing_delay_calc(rack, tp->gput_ack, tp->gput_seq,
							   0, 0, 0, 6, __LINE__, NULL, quality);
				return;
			}
		}
		if (tp->t_state >= TCPS_FIN_WAIT_1) {
			/*
			 * We will get no more data into the SB
			 * this means we need to have the data available
			 * before we start a measurement.
			 */
			if (sbavail(&tptosocket(tp)->so_snd) < (tp->gput_ack - tp->gput_seq)) {
				/* Nope not enough data. */
				return;
			}
		}
		tp->t_flags |= TF_GPUTINPROG;
		/*
		 * Now we need to find the timestamp of the send at tp->gput_seq
		 * for the send based measurement.
		 */
		rack->r_ctl.rc_gp_cumack_ts = 0;
		rsm = tqhash_find(rack->r_ctl.tqh, tp->gput_seq);
		if (rsm) {
			/* Ok send-based limit is set */
			if (SEQ_LT(rsm->r_start, tp->gput_seq)) {
				/*
				 * Move back to include the earlier part
				 * so our ack time lines up right (this may
				 * make an overlapping measurement but thats
				 * ok).
				 */
				tp->gput_seq = rsm->r_start;
			}
			if (rsm->r_flags & RACK_ACKED) {
				struct rack_sendmap *nrsm;

				tp->gput_ts = (uint32_t)rsm->r_ack_arrival;
				tp->gput_seq = rsm->r_end;
				nrsm = tqhash_next(rack->r_ctl.tqh, rsm);
				if (nrsm)
					rsm = nrsm;
				else {
					rack->app_limited_needs_set = 1;
				}
			} else
				rack->app_limited_needs_set = 1;
			/* We always go from the first send */
			rack->r_ctl.rc_gp_output_ts = rsm->r_tim_lastsent[0];
		} else {
			/*
			 * If we don't find the rsm due to some
			 * send-limit set the current time, which
			 * basically disables the send-limit.
			 */
			struct timeval tv;

			microuptime(&tv);
			rack->r_ctl.rc_gp_output_ts = rack_to_usec_ts(&tv);
		}
		rack_tend_gp_marks(tp, rack);
		rack_log_pacing_delay_calc(rack,
					   tp->gput_seq,
					   tp->gput_ack,
					   (uint64_t)rsm,
					   tp->gput_ts,
					   (((uint64_t)rack->r_ctl.rc_app_limited_cnt << 32) | (uint64_t)rack->r_ctl.rc_gp_output_ts),
					   9,
					   __LINE__, rsm, quality);
		rack_log_gpset(rack, tp->gput_ack, 0, 0, __LINE__, 1, NULL);
	} else {
		/*
		 * To make sure proper timestamp merging occurs, we need to clear
		 * all GP marks if we don't start a measurement.
		 */
		rack_clear_gp_marks(tp, rack);
	}
}

/*
 * CC wrapper hook functions
 */
static void
rack_ack_received(struct tcpcb *tp, struct tcp_rack *rack, uint32_t th_ack, uint16_t nsegs,
    uint16_t type, int32_t post_recovery)
{
	uint32_t prior_cwnd, acked;
	struct tcp_log_buffer *lgb = NULL;
	uint8_t labc_to_use, quality;

	INP_WLOCK_ASSERT(tptoinpcb(tp));
	tp->t_ccv.nsegs = nsegs;
	acked = tp->t_ccv.bytes_this_ack = (th_ack - tp->snd_una);
	if ((post_recovery) && (rack->r_ctl.rc_early_recovery_segs)) {
		uint32_t max;

		max = rack->r_ctl.rc_early_recovery_segs * ctf_fixed_maxseg(tp);
		if (tp->t_ccv.bytes_this_ack > max) {
			tp->t_ccv.bytes_this_ack = max;
		}
	}
#ifdef STATS
	stats_voi_update_abs_s32(tp->t_stats, VOI_TCP_CALCFRWINDIFF,
	    ((int32_t)rack->r_ctl.cwnd_to_use) - tp->snd_wnd);
#endif
	if ((th_ack == tp->snd_max) && rack->lt_bw_up) {
		/*
		 * We will ack all the data, time to end any
		 * lt_bw_up we have running until something
		 * new is sent. Note we need to use the actual
		 * ack_rcv_time which with pacing may be different.
		 */
		uint64_t tmark;

		rack->r_ctl.lt_bw_bytes += (tp->snd_max - rack->r_ctl.lt_seq);
		rack->r_ctl.lt_seq = tp->snd_max;
		tmark = tcp_tv_to_lusectick(&rack->r_ctl.act_rcv_time);
		if (tmark >= rack->r_ctl.lt_timemark) {
			rack->r_ctl.lt_bw_time += (tmark - rack->r_ctl.lt_timemark);
		}
		rack->r_ctl.lt_timemark = tmark;
		rack->lt_bw_up = 0;
	}
	quality = RACK_QUALITY_NONE;
	if ((tp->t_flags & TF_GPUTINPROG) &&
	    rack_enough_for_measurement(tp, rack, th_ack, &quality)) {
		/* Measure the Goodput */
		rack_do_goodput_measurement(tp, rack, th_ack, __LINE__, quality);
	}
	/* Which way our we limited, if not cwnd limited no advance in CA */
	if (tp->snd_cwnd <= tp->snd_wnd)
		tp->t_ccv.flags |= CCF_CWND_LIMITED;
	else
		tp->t_ccv.flags &= ~CCF_CWND_LIMITED;
	if (tp->snd_cwnd > tp->snd_ssthresh) {
		tp->t_bytes_acked += min(tp->t_ccv.bytes_this_ack,
			 nsegs * V_tcp_abc_l_var * ctf_fixed_maxseg(tp));
		/* For the setting of a window past use the actual scwnd we are using */
		if (tp->t_bytes_acked >= rack->r_ctl.cwnd_to_use) {
			tp->t_bytes_acked -= rack->r_ctl.cwnd_to_use;
			tp->t_ccv.flags |= CCF_ABC_SENTAWND;
		}
	} else {
		tp->t_ccv.flags &= ~CCF_ABC_SENTAWND;
		tp->t_bytes_acked = 0;
	}
	prior_cwnd = tp->snd_cwnd;
	if ((post_recovery == 0) || (rack_max_abc_post_recovery == 0) || rack->r_use_labc_for_rec ||
	    (rack_client_low_buf && rack->client_bufferlvl &&
	    (rack->client_bufferlvl < rack_client_low_buf)))
		labc_to_use = rack->rc_labc;
	else
		labc_to_use = rack_max_abc_post_recovery;
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = th_ack;
		log.u_bbr.flex2 = tp->t_ccv.flags;
		log.u_bbr.flex3 = tp->t_ccv.bytes_this_ack;
		log.u_bbr.flex4 = tp->t_ccv.nsegs;
		log.u_bbr.flex5 = labc_to_use;
		log.u_bbr.flex6 = prior_cwnd;
		log.u_bbr.flex7 = V_tcp_do_newsack;
		log.u_bbr.flex8 = 1;
		lgb = tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_CWND, 0,
				     0, &log, false, NULL, __func__, __LINE__,&tv);
	}
	if (CC_ALGO(tp)->ack_received != NULL) {
		/* XXXLAS: Find a way to live without this */
		tp->t_ccv.curack = th_ack;
		tp->t_ccv.labc = labc_to_use;
		tp->t_ccv.flags |= CCF_USE_LOCAL_ABC;
		CC_ALGO(tp)->ack_received(&tp->t_ccv, type);
	}
	if (lgb) {
		lgb->tlb_stackinfo.u_bbr.flex6 = tp->snd_cwnd;
	}
	if (rack->r_must_retran) {
		if (SEQ_GEQ(th_ack, rack->r_ctl.rc_snd_max_at_rto)) {
			/*
			 * We now are beyond the rxt point so lets disable
			 * the flag.
			 */
			rack->r_ctl.rc_out_at_rto = 0;
			rack->r_must_retran = 0;
		} else if ((prior_cwnd + ctf_fixed_maxseg(tp)) <= tp->snd_cwnd) {
			/*
			 * Only decrement the rc_out_at_rto if the cwnd advances
			 * at least a whole segment. Otherwise next time the peer
			 * acks, we won't be able to send this generaly happens
			 * when we are in Congestion Avoidance.
			 */
			if (acked <= rack->r_ctl.rc_out_at_rto){
				rack->r_ctl.rc_out_at_rto -= acked;
			} else {
				rack->r_ctl.rc_out_at_rto = 0;
			}
		}
	}
#ifdef STATS
	stats_voi_update_abs_ulong(tp->t_stats, VOI_TCP_LCWIN, rack->r_ctl.cwnd_to_use);
#endif
	if (rack->r_ctl.rc_rack_largest_cwnd < rack->r_ctl.cwnd_to_use) {
		rack->r_ctl.rc_rack_largest_cwnd = rack->r_ctl.cwnd_to_use;
	}
	if ((rack->rc_initial_ss_comp == 0) &&
	    (tp->snd_cwnd >= tp->snd_ssthresh)) {
		/*
		 * The cwnd has grown beyond ssthresh we have
		 * entered ca and completed our first Slowstart.
		 */
		rack->rc_initial_ss_comp = 1;
	}
}

static void
tcp_rack_partialack(struct tcpcb *tp)
{
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	INP_WLOCK_ASSERT(tptoinpcb(tp));
	/*
	 * If we are doing PRR and have enough
	 * room to send <or> we are pacing and prr
	 * is disabled we will want to see if we
	 * can send data (by setting r_wanted_output to
	 * true).
	 */
	if ((rack->r_ctl.rc_prr_sndcnt > 0) ||
	    rack->rack_no_prr)
		rack->r_wanted_output = 1;
}

static inline uint64_t
rack_get_rxt_per(uint64_t snds,  uint64_t rxts)
{
	uint64_t rxt_per;

	if (snds > 0) {
		rxt_per = rxts * 1000;
		rxt_per /= snds;
	} else {
		/* This is an unlikely path */
		if (rxts) {
			/* Its the max it was all re-transmits */
			rxt_per = 0xffffffffffffffff;
		} else {
			rxt_per = 0;
		}
	}
	return (rxt_per);
}

static void
policer_detection_log(struct tcp_rack *rack, uint32_t flex1, uint32_t flex2, uint32_t flex3, uint32_t flex4, uint8_t flex8)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = flex1;
		log.u_bbr.flex2 = flex2;
		log.u_bbr.flex3 = flex3;
		log.u_bbr.flex4 = flex4;
		log.u_bbr.flex5 = rack->r_ctl.current_policer_bucket;
		log.u_bbr.flex6 = rack->r_ctl.policer_bucket_size;
		log.u_bbr.flex7 = 0;
		log.u_bbr.flex8 = flex8;
		log.u_bbr.bw_inuse = rack->r_ctl.policer_bw;
		log.u_bbr.applimited = rack->r_ctl.current_round;
		log.u_bbr.epoch = rack->r_ctl.policer_max_seg;
		log.u_bbr.delivered = (uint32_t)rack->r_ctl.bytes_acked_in_recovery;
		log.u_bbr.cur_del_rate = rack->rc_tp->t_sndbytes;
		log.u_bbr.delRate = rack->rc_tp->t_snd_rxt_bytes;
		log.u_bbr.rttProp = rack->r_ctl.gp_bw;
		log.u_bbr.bbr_state = rack->rc_policer_detected;
		log.u_bbr.bbr_substate = 0;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.use_lt_bw = rack->policer_detect_on;
		log.u_bbr.lt_epoch = 0;
		log.u_bbr.pkts_out = 0;
		tcp_log_event(rack->rc_tp, NULL, NULL, NULL, TCP_POLICER_DET, 0,
			      0, &log, false, NULL, NULL, 0, &tv);
	}

}

static void
policer_detection(struct tcpcb *tp, struct tcp_rack *rack, int post_recovery)
{
	/*
	 * Rack excess rxt accounting is turned on. If we
	 * are above a threshold of rxt's in at least N
	 * rounds, then back off the cwnd and ssthresh
	 * to fit into the long-term b/w.
	 */

	uint32_t pkts, mid, med, alt_med, avg, segsiz, tot_retran_pkt_count = 0;
	uint32_t cnt_of_mape_rxt = 0;
	uint64_t snds, rxts, rxt_per, tim, del, del_bw;
	int i;
	struct timeval tv;


	/*
	 * First is there enough packets delivered during recovery to make
	 * a determiniation of b/w?
	 */
	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	if ((rack->rc_policer_detected == 0) &&
	    (rack->r_ctl.policer_del_mss > 0) &&
	    ((uint32_t)rack->r_ctl.policer_del_mss > ((rack->r_ctl.bytes_acked_in_recovery + segsiz - 1)/segsiz))) {
		/*
		 * Not enough data sent in recovery for initial detection. Once
		 * we have deteced a policer we allow less than the threshold (polcer_del_mss)
		 * amount of data in a recovery to let us fall through and double check
		 * our policer settings and possibly expand or collapse the bucket size and
		 * the polcier b/w.
		 *
		 * Once you are declared to be policed. this block of code cannot be
		 * reached, instead blocks further down will re-check the policer detection
		 * triggers and possibly reset the measurements if somehow we have let the
		 * policer bucket size grow too large.
		 */
		if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
			policer_detection_log(rack, rack->r_ctl.policer_del_mss,
					      ((rack->r_ctl.bytes_acked_in_recovery + segsiz - 1)/segsiz),
					      rack->r_ctl.bytes_acked_in_recovery, segsiz, 18);
		}
		return;
	}
	tcp_get_usecs(&tv);
	tim = tcp_tv_to_lusectick(&tv) - rack->r_ctl.time_entered_recovery;
	del = rack->r_ctl.bytes_acked_in_recovery;
	if (tim > 0)
		del_bw = (del * (uint64_t)1000000) / tim;
	else
		del_bw = 0;
	/* B/W compensation? */

	if (rack->r_ctl.pol_bw_comp && ((rack->r_ctl.policer_bw > 0) ||
					(del_bw > 0))) {
		/*
		 * Sanity check now that the data is in. How long does it
		 * take for us to pace out two of our policer_max_seg's?
		 *
		 * If it is longer than the RTT then we are set
		 * too slow, maybe because of not enough data
		 * sent during recovery.
		 */
		uint64_t lentime, res, srtt, max_delbw, alt_bw;

		srtt = (uint64_t)rack_grab_rtt(tp, rack);
		if ((tp->t_srtt > 0) && (srtt > tp->t_srtt))
			srtt = tp->t_srtt;
		lentime = rack->r_ctl.policer_max_seg * (uint64_t)HPTS_USEC_IN_SEC * 2;
		if (del_bw > rack->r_ctl.policer_bw) {
			max_delbw = del_bw;
		} else {
			max_delbw = rack->r_ctl.policer_bw;
		}
		res = lentime / max_delbw;
		if ((srtt > 0) && (res > srtt)) {
			/*
			 * At this rate we can not get two policer_maxsegs
			 * out before the ack arrives back.
			 *
			 * Lets at least get it raised up so that
			 * we can be a bit faster than that if possible.
			 */
			lentime = (rack->r_ctl.policer_max_seg * 2);
			tim = srtt;
			alt_bw = (lentime * (uint64_t)HPTS_USEC_IN_SEC) / tim;
			if (alt_bw > max_delbw) {
				uint64_t cap_alt_bw;

				cap_alt_bw = (max_delbw + (max_delbw * rack->r_ctl.pol_bw_comp));
				if ((rack_pol_min_bw > 0) && (cap_alt_bw < rack_pol_min_bw)) {
					/* We place a min on the cap which defaults to 1Mbps */
					cap_alt_bw = rack_pol_min_bw;
				}
				if (alt_bw <= cap_alt_bw) {
					/* It should be */
					del_bw = alt_bw;
					policer_detection_log(rack,
							      (uint32_t)tim,
							      rack->r_ctl.policer_max_seg,
							      0,
							      0,
							      16);
				} else {
					/*
					 * This is an odd case where likely the RTT is very very
					 * low. And yet it is still being policed. We don't want
					 * to get more than (rack_policing_do_bw_comp+1) x del-rate
					 * where del-rate is what we got in recovery for either the
					 * first Policer Detection(PD) or this PD we are on now.
					 */
					del_bw = cap_alt_bw;
					policer_detection_log(rack,
							      (uint32_t)tim,
							      rack->r_ctl.policer_max_seg,
							      (uint32_t)max_delbw,
							      (rack->r_ctl.pol_bw_comp + 1),
							      16);
				}
			}
		}
	}
	snds = tp->t_sndbytes - rack->r_ctl.last_policer_sndbytes;
	rxts = tp->t_snd_rxt_bytes - rack->r_ctl.last_policer_snd_rxt_bytes;
	rxt_per = rack_get_rxt_per(snds,  rxts);
	/* Figure up the average  and median */
	for(i = 0; i < RETRAN_CNT_SIZE; i++) {
		if (rack->r_ctl.rc_cnt_of_retran[i] > 0) {
			tot_retran_pkt_count += (i + 1) * rack->r_ctl.rc_cnt_of_retran[i];
			cnt_of_mape_rxt  += rack->r_ctl.rc_cnt_of_retran[i];
		}
	}
	if (cnt_of_mape_rxt)
		avg = (tot_retran_pkt_count * 10)/cnt_of_mape_rxt;
	else
		avg = 0;
	alt_med = med = 0;
	mid = tot_retran_pkt_count/2;
	for(i = 0; i < RETRAN_CNT_SIZE; i++) {
		pkts = (i + 1) * rack->r_ctl.rc_cnt_of_retran[i];
		if (mid > pkts) {
			mid -= pkts;
			continue;
		}
		med = (i + 1);
		break;
	}
	mid = cnt_of_mape_rxt / 2;
	for(i = 0; i < RETRAN_CNT_SIZE; i++) {
		if (mid > rack->r_ctl.rc_cnt_of_retran[i]) {
			mid -= rack->r_ctl.rc_cnt_of_retran[i];
			continue;
		}
		alt_med = (i + 1);
		break;
	}
	if (rack->r_ctl.policer_alt_median) {
		/* Swap the medians */
		uint32_t swap;

		swap = med;
		med = alt_med;
		alt_med = swap;
	}
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = avg;
		log.u_bbr.flex2 = med;
		log.u_bbr.flex3 = (uint32_t)rxt_per;
		log.u_bbr.flex4 = rack->r_ctl.policer_avg_threshold;
		log.u_bbr.flex5 = rack->r_ctl.policer_med_threshold;
		log.u_bbr.flex6 = rack->r_ctl.policer_rxt_threshold;
		log.u_bbr.flex7 = rack->r_ctl.policer_alt_median;
		log.u_bbr.flex8 = 1;
		log.u_bbr.delivered = rack->r_ctl.policer_bucket_size;
		log.u_bbr.applimited = rack->r_ctl.current_round;
		log.u_bbr.epoch = rack->r_ctl.policer_max_seg;
		log.u_bbr.bw_inuse = del_bw;
		log.u_bbr.cur_del_rate = rxts;
		log.u_bbr.delRate = snds;
		log.u_bbr.rttProp = rack->r_ctl.gp_bw;
		log.u_bbr.bbr_state = rack->rc_policer_detected;
		log.u_bbr.bbr_substate = 0;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.use_lt_bw = rack->policer_detect_on;
		log.u_bbr.lt_epoch = (uint32_t)tim;
		log.u_bbr.pkts_out = rack->r_ctl.bytes_acked_in_recovery;
		tcp_log_event(tp, NULL, NULL, NULL, TCP_POLICER_DET, 0,
			      0, &log, false, NULL, NULL, 0, &tv);
	}
	if (med == RETRAN_CNT_SIZE) {
		/*
		 * If the median is the maximum, then what we
		 * likely have here is a network breakage. Either that
		 * or we are so unlucky that all of our traffic is being
		 * dropped and having to be retransmitted the maximum times
		 * and this just is not how a policer works.
		 *
		 * If it is truely a policer eventually we will come
		 * through and it won't be the maximum.
		 */
		return;
	}
	/* Has enough rounds progressed for us to re-measure? */
	if ((rxt_per >= (uint64_t)rack->r_ctl.policer_rxt_threshold) &&
	    (avg >= rack->r_ctl.policer_avg_threshold) &&
	    (med >= rack->r_ctl.policer_med_threshold)) {
		/*
		 * We hit all thresholds that indicate we are
		 * being policed. Now we may be doing this from a rack timeout
		 * which then means the rest of recovery will hopefully go
		 * smoother as we pace. At the end of recovery we will
		 * fall back in here and reset the values using the
		 * results of the entire recovery episode (we could also
		 * hit this as we exit recovery as well which means only
		 * one time in here).
		 *
		 * This is done explicitly that if we hit the thresholds
		 * again in a second recovery we overwrite the values. We do
		 * that because over time, as we pace the policer_bucket_size may
		 * continue to grow. This then provides more and more times when
		 * we are not pacing to the policer rate. This lets us compensate
		 * for when we hit a false positive and those flows continue to
		 * increase. However if its a real policer we will then get over its
		 * limit, over time, again and thus end up back here hitting the
		 * thresholds again.
		 *
		 * The alternative to this is to instead whenever we pace due to
		 * policing in rack_policed_sending we could add the amount len paced to the
		 * idle_snd_una value (which decreases the amount in last_amount_before_rec
		 * since that is always [th_ack - idle_snd_una]). This would then prevent
		 * the polcier_bucket_size from growing in additional recovery episodes
		 * Which would then mean false  postives would be pretty much stuck
		 * after things got back to normal (assuming that what caused the
		 * false positive was a small network outage).
		 *
		 */
		tcp_trace_point(rack->rc_tp, TCP_TP_POLICER_DET);
		if (rack->rc_policer_detected == 0) {
			/*
			 * Increment the stat that tells us we identified
			 * a policer only once. Note that if we ever allow
			 * the flag to be cleared (reverted) then we need
			 * to adjust this to not do multi-counting.
			 */
			counter_u64_add(tcp_policer_detected, 1);
		}
		rack->r_ctl.last_policer_sndbytes = tp->t_sndbytes;
		rack->r_ctl.last_policer_snd_rxt_bytes = tp->t_snd_rxt_bytes;
		rack->r_ctl.policer_bw = del_bw;
		rack->r_ctl.policer_max_seg = tcp_get_pacing_burst_size_w_divisor(rack->rc_tp,
										  rack->r_ctl.policer_bw,
										  min(ctf_fixed_maxseg(rack->rc_tp),
										      rack->r_ctl.rc_pace_min_segs),
										  0, NULL,
										  NULL, rack->r_ctl.pace_len_divisor);
		/* Now what about the policer bucket size */
		rack->r_ctl.policer_bucket_size = rack->r_ctl.last_amount_before_rec;
		if (rack->r_ctl.policer_bucket_size < rack->r_ctl.policer_max_seg) {
			/* We must be able to send our max-seg or else chaos ensues */
			rack->r_ctl.policer_bucket_size = rack->r_ctl.policer_max_seg * 2;
		}
		if (rack->rc_policer_detected == 0)
			rack->r_ctl.current_policer_bucket = 0;
		if (tcp_bblogging_on(rack->rc_tp)) {
			union tcp_log_stackspecific log;
			struct timeval tv;

			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.timeStamp = tcp_get_usecs(&tv);
			log.u_bbr.flex1 = avg;
			log.u_bbr.flex2 = med;
			log.u_bbr.flex3 = rxt_per;
			log.u_bbr.flex4 = rack->r_ctl.policer_avg_threshold;
			log.u_bbr.flex5 = rack->r_ctl.policer_med_threshold;
			log.u_bbr.flex6 = rack->r_ctl.policer_rxt_threshold;
			log.u_bbr.flex7 = rack->r_ctl.policer_alt_median;
			log.u_bbr.flex8 = 2;
			log.u_bbr.applimited = rack->r_ctl.current_round;
			log.u_bbr.bw_inuse = del_bw;
			log.u_bbr.delivered = rack->r_ctl.policer_bucket_size;
			log.u_bbr.cur_del_rate = rxts;
			log.u_bbr.delRate = snds;
			log.u_bbr.rttProp = rack->r_ctl.gp_bw;
			log.u_bbr.bbr_state = rack->rc_policer_detected;
			log.u_bbr.bbr_substate = 0;
			log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
			log.u_bbr.use_lt_bw = rack->policer_detect_on;
			log.u_bbr.epoch = rack->r_ctl.policer_max_seg;
			log.u_bbr.lt_epoch = (uint32_t)tim;
			log.u_bbr.pkts_out = rack->r_ctl.bytes_acked_in_recovery;
			tcp_log_event(tp, NULL, NULL, NULL, TCP_POLICER_DET, 0,
				      0, &log, false, NULL, NULL, 0, &tv);
			/*
			 * Put out an added log, 19, for the sole purpose
			 * of getting the txt/rxt so that we can benchmark
			 * in read-bbrlog the ongoing rxt rate after our
			 * policer invocation in the HYSTART announcments.
			 */
			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.timeStamp = tcp_tv_to_usectick(&tv);
			log.u_bbr.flex1 = alt_med;
			log.u_bbr.flex8 = 19;
			log.u_bbr.cur_del_rate = tp->t_sndbytes;
			log.u_bbr.delRate = tp->t_snd_rxt_bytes;
			tcp_log_event(tp, NULL, NULL, NULL, TCP_POLICER_DET, 0,
				      0, &log, false, NULL, NULL, 0, &tv);
		}
		/* Turn off any fast output, thats ended */
		rack->r_fast_output = 0;
		/* Mark the time for credits */
		rack->r_ctl.last_sendtime = tcp_get_u64_usecs(NULL);
		if (rack->r_rr_config < 2) {
			/*
			 * We need to be stricter on the RR config so
			 * the pacing has priority.
			 */
			rack->r_rr_config = 2;
		}
		policer_detection_log(rack,
				      rack->r_ctl.idle_snd_una,
				      rack->r_ctl.ack_for_idle,
				      0,
				      (uint32_t)tim,
				      14);
		rack->rc_policer_detected = 1;
	} else if ((rack->rc_policer_detected == 1) &&
		   (post_recovery == 1)) {
		/*
		 * If we are exiting recovery and have already detected
		 * we need to possibly update the values.
		 *
		 * First: Update the idle -> recovery sent value.
		 */
		uint32_t srtt;

		if (rack->r_ctl.last_amount_before_rec > rack->r_ctl.policer_bucket_size) {
			rack->r_ctl.policer_bucket_size = rack->r_ctl.last_amount_before_rec;
		}
		srtt = (uint64_t)rack_grab_rtt(tp, rack);
		if ((tp->t_srtt > 0) && (srtt > tp->t_srtt))
			srtt = tp->t_srtt;
		if ((srtt != 0) &&
		    (tim < (uint64_t)srtt)) {
			/*
			 * Not long enough.
			 */
			if (rack_verbose_logging)
				policer_detection_log(rack,
						      (uint32_t)tim,
						      0,
						      0,
						      0,
						      15);
			return;
		}
		/*
		 * Finally update the b/w if its grown.
		 */
		if (del_bw > rack->r_ctl.policer_bw) {
			rack->r_ctl.policer_bw = del_bw;
			rack->r_ctl.policer_max_seg = tcp_get_pacing_burst_size_w_divisor(rack->rc_tp,
											  rack->r_ctl.policer_bw,
											  min(ctf_fixed_maxseg(rack->rc_tp),
											      rack->r_ctl.rc_pace_min_segs),
											  0, NULL,
											  NULL, rack->r_ctl.pace_len_divisor);
			if (rack->r_ctl.policer_bucket_size < rack->r_ctl.policer_max_seg) {
				/* We must be able to send our max-seg or else chaos ensues */
				rack->r_ctl.policer_bucket_size = rack->r_ctl.policer_max_seg * 2;
			}
		}
		policer_detection_log(rack,
				      rack->r_ctl.idle_snd_una,
				      rack->r_ctl.ack_for_idle,
				      0,
				      (uint32_t)tim,
				      3);
	}
}

static void
rack_exit_recovery(struct tcpcb *tp, struct tcp_rack *rack, int how)
{
	/* now check with the policer if on */
	if (rack->policer_detect_on == 1) {
		policer_detection(tp, rack, 1);
	}
	/*
	 * Now exit recovery, note we must do the idle set after the policer_detection
	 * to get the amount acked prior to recovery correct.
	 */
	rack->r_ctl.idle_snd_una = tp->snd_una;
	EXIT_RECOVERY(tp->t_flags);
}

static void
rack_post_recovery(struct tcpcb *tp, uint32_t th_ack)
{
	struct tcp_rack *rack;
	uint32_t orig_cwnd;

	orig_cwnd = tp->snd_cwnd;
	INP_WLOCK_ASSERT(tptoinpcb(tp));
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	/* only alert CC if we alerted when we entered */
	if (CC_ALGO(tp)->post_recovery != NULL) {
		tp->t_ccv.curack = th_ack;
		CC_ALGO(tp)->post_recovery(&tp->t_ccv);
		if (tp->snd_cwnd < tp->snd_ssthresh) {
			/*
			 * Rack has burst control and pacing
			 * so lets not set this any lower than
			 * snd_ssthresh per RFC-6582 (option 2).
			 */
			tp->snd_cwnd = tp->snd_ssthresh;
		}
	}
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = th_ack;
		log.u_bbr.flex2 = tp->t_ccv.flags;
		log.u_bbr.flex3 = tp->t_ccv.bytes_this_ack;
		log.u_bbr.flex4 = tp->t_ccv.nsegs;
		log.u_bbr.flex5 = V_tcp_abc_l_var;
		log.u_bbr.flex6 = orig_cwnd;
		log.u_bbr.flex7 = V_tcp_do_newsack;
		log.u_bbr.pkts_out = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex8 = 2;
		tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_CWND, 0,
			       0, &log, false, NULL, __func__, __LINE__, &tv);
	}
	if ((rack->rack_no_prr == 0) &&
	    (rack->no_prr_addback == 0) &&
	    (rack->r_ctl.rc_prr_sndcnt > 0)) {
		/*
		 * Suck the next prr cnt back into cwnd, but
		 * only do that if we are not application limited.
		 */
		if (ctf_outstanding(tp) <= sbavail(&tptosocket(tp)->so_snd)) {
			/*
			 * We are allowed to add back to the cwnd the amount we did
			 * not get out if:
			 * a) no_prr_addback is off.
			 * b) we are not app limited
			 * c) we are doing prr
			 * <and>
			 * d) it is bounded by rack_prr_addbackmax (if addback is 0, then none).
			 */
			tp->snd_cwnd += min((ctf_fixed_maxseg(tp) * rack_prr_addbackmax),
					    rack->r_ctl.rc_prr_sndcnt);
		}
		rack->r_ctl.rc_prr_sndcnt = 0;
		rack_log_to_prr(rack, 1, 0, __LINE__);
	}
	rack_log_to_prr(rack, 14, orig_cwnd, __LINE__);
	tp->snd_recover = tp->snd_una;
	if (rack->r_ctl.dsack_persist) {
		rack->r_ctl.dsack_persist--;
		if (rack->r_ctl.num_dsack && (rack->r_ctl.dsack_persist == 0)) {
			rack->r_ctl.num_dsack = 0;
		}
		rack_log_dsack_event(rack, 1, __LINE__, 0, 0);
	}
	if (rack->rto_from_rec == 1) {
		rack->rto_from_rec = 0;
		if (rack->r_ctl.rto_ssthresh > tp->snd_ssthresh)
			tp->snd_ssthresh = rack->r_ctl.rto_ssthresh;
	}
	rack_exit_recovery(tp, rack, 1);
}

static void
rack_cong_signal(struct tcpcb *tp, uint32_t type, uint32_t ack, int line)
{
	struct tcp_rack *rack;
	uint32_t ssthresh_enter, cwnd_enter, in_rec_at_entry, orig_cwnd;

	INP_WLOCK_ASSERT(tptoinpcb(tp));
#ifdef STATS
	stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_CSIG, type);
#endif
	if (IN_RECOVERY(tp->t_flags) == 0) {
		in_rec_at_entry = 0;
		ssthresh_enter = tp->snd_ssthresh;
		cwnd_enter = tp->snd_cwnd;
	} else
		in_rec_at_entry = 1;
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	switch (type) {
	case CC_NDUPACK:
		tp->t_flags &= ~TF_WASFRECOVERY;
		tp->t_flags &= ~TF_WASCRECOVERY;
		if (!IN_FASTRECOVERY(tp->t_flags)) {
			struct rack_sendmap *rsm;
			struct timeval tv;
			uint32_t segsiz;

			/* Check if this is the end of the initial Start-up i.e. initial slow-start */
			if (rack->rc_initial_ss_comp == 0) {
				/* Yep it is the end of the initial slowstart */
				rack->rc_initial_ss_comp = 1;
			}
			microuptime(&tv);
			rack->r_ctl.time_entered_recovery = tcp_tv_to_lusectick(&tv);
			if (SEQ_GEQ(ack, tp->snd_una)) {
				/*
				 * The ack is above snd_una. Lets see
				 * if we can establish a postive distance from
				 * our idle mark.
				 */
				rack->r_ctl.ack_for_idle = ack;
				if (SEQ_GT(ack, rack->r_ctl.idle_snd_una)) {
					rack->r_ctl.last_amount_before_rec = ack - rack->r_ctl.idle_snd_una;
				} else {
					/* No data thru yet */
					rack->r_ctl.last_amount_before_rec = 0;
				}
			} else if (SEQ_GT(tp->snd_una, rack->r_ctl.idle_snd_una)) {
				/*
				 * The ack is out of order and behind the snd_una. It may
				 * have contained SACK information which we processed else
				 * we would have rejected it.
				 */
				rack->r_ctl.ack_for_idle = tp->snd_una;
				rack->r_ctl.last_amount_before_rec = tp->snd_una - rack->r_ctl.idle_snd_una;
			} else {
				rack->r_ctl.ack_for_idle = ack;
				rack->r_ctl.last_amount_before_rec = 0;
			}
			if (rack->rc_policer_detected) {
				/*
				 * If we are being policed and we have a loss, it
				 * means our bucket is now empty. This can happen
				 * where some other flow on the same host sends
				 * that this connection is not aware of.
				 */
				rack->r_ctl.current_policer_bucket = 0;
				if (rack_verbose_logging)
					policer_detection_log(rack, rack->r_ctl.last_amount_before_rec, 0, 0, 0, 4);
				if (rack->r_ctl.last_amount_before_rec > rack->r_ctl.policer_bucket_size) {
					rack->r_ctl.policer_bucket_size = rack->r_ctl.last_amount_before_rec;
				}
			}
			memset(rack->r_ctl.rc_cnt_of_retran, 0, sizeof(rack->r_ctl.rc_cnt_of_retran));
			segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
			TAILQ_FOREACH(rsm, &rack->r_ctl.rc_tmap, r_tnext) {
				/*
				 * Go through the outstanding and re-peg
				 * any that should have been left in the
				 * retransmit list (on a double recovery).
				 */
				if (rsm->r_act_rxt_cnt > 0) {
					rack_peg_rxt(rack, rsm, segsiz);
				}
			}
			rack->r_ctl.bytes_acked_in_recovery = 0;
			rack->r_ctl.rc_prr_delivered = 0;
			rack->r_ctl.rc_prr_out = 0;
			rack->r_fast_output = 0;
			if (rack->rack_no_prr == 0) {
				rack->r_ctl.rc_prr_sndcnt = ctf_fixed_maxseg(tp);
				rack_log_to_prr(rack, 2, in_rec_at_entry, line);
			}
			rack->r_ctl.rc_prr_recovery_fs = tp->snd_max - tp->snd_una;
			tp->snd_recover = tp->snd_max;
			if (tp->t_flags2 & TF2_ECN_PERMIT)
				tp->t_flags2 |= TF2_ECN_SND_CWR;
		}
		break;
	case CC_ECN:
		if (!IN_CONGRECOVERY(tp->t_flags) ||
		    /*
		     * Allow ECN reaction on ACK to CWR, if
		     * that data segment was also CE marked.
		     */
		    SEQ_GEQ(ack, tp->snd_recover)) {
			EXIT_CONGRECOVERY(tp->t_flags);
			KMOD_TCPSTAT_INC(tcps_ecn_rcwnd);
			rack->r_fast_output = 0;
			tp->snd_recover = tp->snd_max + 1;
			if (tp->t_flags2 & TF2_ECN_PERMIT)
				tp->t_flags2 |= TF2_ECN_SND_CWR;
		}
		break;
	case CC_RTO:
		tp->t_dupacks = 0;
		tp->t_bytes_acked = 0;
		rack->r_fast_output = 0;
		if (IN_RECOVERY(tp->t_flags))
			rack_exit_recovery(tp, rack, 2);
		rack->r_ctl.bytes_acked_in_recovery = 0;
		rack->r_ctl.time_entered_recovery = 0;
		orig_cwnd = tp->snd_cwnd;
		rack_log_to_prr(rack, 16, orig_cwnd, line);
		if (CC_ALGO(tp)->cong_signal == NULL) {
			/* TSNH */
			tp->snd_ssthresh = max(2,
			    min(tp->snd_wnd, rack->r_ctl.cwnd_to_use) / 2 /
			    ctf_fixed_maxseg(tp)) * ctf_fixed_maxseg(tp);
			tp->snd_cwnd = ctf_fixed_maxseg(tp);
		}
		if (tp->t_flags2 & TF2_ECN_PERMIT)
			tp->t_flags2 |= TF2_ECN_SND_CWR;
		break;
	case CC_RTO_ERR:
		KMOD_TCPSTAT_INC(tcps_sndrexmitbad);
		/* RTO was unnecessary, so reset everything. */
		tp->snd_cwnd = tp->snd_cwnd_prev;
		tp->snd_ssthresh = tp->snd_ssthresh_prev;
		tp->snd_recover = tp->snd_recover_prev;
		if (tp->t_flags & TF_WASFRECOVERY) {
			ENTER_FASTRECOVERY(tp->t_flags);
			tp->t_flags &= ~TF_WASFRECOVERY;
		}
		if (tp->t_flags & TF_WASCRECOVERY) {
			ENTER_CONGRECOVERY(tp->t_flags);
			tp->t_flags &= ~TF_WASCRECOVERY;
		}
		tp->snd_nxt = tp->snd_max;
		tp->t_badrxtwin = 0;
		break;
	}
	if ((CC_ALGO(tp)->cong_signal != NULL)  &&
	    (type != CC_RTO)){
		tp->t_ccv.curack = ack;
		CC_ALGO(tp)->cong_signal(&tp->t_ccv, type);
	}
	if ((in_rec_at_entry == 0) && IN_RECOVERY(tp->t_flags)) {
		rack_log_to_prr(rack, 15, cwnd_enter, line);
		rack->r_ctl.dsack_byte_cnt = 0;
		rack->r_ctl.retran_during_recovery = 0;
		rack->r_ctl.rc_cwnd_at_erec = cwnd_enter;
		rack->r_ctl.rc_ssthresh_at_erec = ssthresh_enter;
		rack->r_ent_rec_ns = 1;
	}
}

static inline void
rack_cc_after_idle(struct tcp_rack *rack, struct tcpcb *tp)
{
	uint32_t i_cwnd;

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	if (CC_ALGO(tp)->after_idle != NULL)
		CC_ALGO(tp)->after_idle(&tp->t_ccv);

	if (tp->snd_cwnd == 1)
		i_cwnd = tp->t_maxseg;		/* SYN(-ACK) lost */
	else
		i_cwnd = rc_init_window(rack);

	/*
	 * Being idle is no different than the initial window. If the cc
	 * clamps it down below the initial window raise it to the initial
	 * window.
	 */
	if (tp->snd_cwnd < i_cwnd) {
		tp->snd_cwnd = i_cwnd;
	}
}

/*
 * Indicate whether this ack should be delayed.  We can delay the ack if
 * following conditions are met:
 *	- There is no delayed ack timer in progress.
 *	- Our last ack wasn't a 0-sized window. We never want to delay
 *	  the ack that opens up a 0-sized window.
 *	- LRO wasn't used for this segment. We make sure by checking that the
 *	  segment size is not larger than the MSS.
 *	- Delayed acks are enabled or this is a half-synchronized T/TCP
 *	  connection.
 */
#define DELAY_ACK(tp, tlen)			 \
	(((tp->t_flags & TF_RXWIN0SENT) == 0) && \
	((tp->t_flags & TF_DELACK) == 0) &&	 \
	(tlen <= tp->t_maxseg) &&		 \
	(tp->t_delayed_ack || (tp->t_flags & TF_NEEDSYN)))

static struct rack_sendmap *
rack_find_lowest_rsm(struct tcp_rack *rack)
{
	struct rack_sendmap *rsm;

	/*
	 * Walk the time-order transmitted list looking for an rsm that is
	 * not acked. This will be the one that was sent the longest time
	 * ago that is still outstanding.
	 */
	TAILQ_FOREACH(rsm, &rack->r_ctl.rc_tmap, r_tnext) {
		if (rsm->r_flags & RACK_ACKED) {
			continue;
		}
		goto finish;
	}
finish:
	return (rsm);
}

static struct rack_sendmap *
rack_find_high_nonack(struct tcp_rack *rack, struct rack_sendmap *rsm)
{
	struct rack_sendmap *prsm;

	/*
	 * Walk the sequence order list backward until we hit and arrive at
	 * the highest seq not acked. In theory when this is called it
	 * should be the last segment (which it was not).
	 */
	prsm = rsm;

	TQHASH_FOREACH_REVERSE_FROM(prsm, rack->r_ctl.tqh) {
		if (prsm->r_flags & (RACK_ACKED | RACK_HAS_FIN)) {
			continue;
		}
		return (prsm);
	}
	return (NULL);
}

static uint32_t
rack_calc_thresh_rack(struct tcp_rack *rack, uint32_t srtt, uint32_t cts, int line, int log_allowed)
{
	int32_t lro;
	uint32_t thresh;

	/*
	 * lro is the flag we use to determine if we have seen reordering.
	 * If it gets set we have seen reordering. The reorder logic either
	 * works in one of two ways:
	 *
	 * If reorder-fade is configured, then we track the last time we saw
	 * re-ordering occur. If we reach the point where enough time as
	 * passed we no longer consider reordering has occuring.
	 *
	 * Or if reorder-face is 0, then once we see reordering we consider
	 * the connection to alway be subject to reordering and just set lro
	 * to 1.
	 *
	 * In the end if lro is non-zero we add the extra time for
	 * reordering in.
	 */
	if (srtt == 0)
		srtt = 1;
	if (rack->r_ctl.rc_reorder_ts) {
		if (rack->r_ctl.rc_reorder_fade) {
			if (SEQ_GEQ(cts, rack->r_ctl.rc_reorder_ts)) {
				lro = cts - rack->r_ctl.rc_reorder_ts;
				if (lro == 0) {
					/*
					 * No time as passed since the last
					 * reorder, mark it as reordering.
					 */
					lro = 1;
				}
			} else {
				/* Negative time? */
				lro = 0;
			}
			if (lro > rack->r_ctl.rc_reorder_fade) {
				/* Turn off reordering seen too */
				rack->r_ctl.rc_reorder_ts = 0;
				lro = 0;
			}
		} else {
			/* Reodering does not fade */
			lro = 1;
		}
	} else {
		lro = 0;
	}
	if (rack->rc_rack_tmr_std_based == 0) {
		thresh = srtt + rack->r_ctl.rc_pkt_delay;
	} else {
		/* Standards based pkt-delay is 1/4 srtt */
		thresh = srtt +  (srtt >> 2);
	}
	if (lro && (rack->rc_rack_tmr_std_based == 0)) {
		/* It must be set, if not you get 1/4 rtt */
		if (rack->r_ctl.rc_reorder_shift)
			thresh += (srtt >> rack->r_ctl.rc_reorder_shift);
		else
			thresh += (srtt >> 2);
	}
	if (rack->rc_rack_use_dsack &&
	    lro &&
	    (rack->r_ctl.num_dsack > 0)) {
		/*
		 * We only increase the reordering window if we
		 * have seen reordering <and> we have a DSACK count.
		 */
		thresh += rack->r_ctl.num_dsack * (srtt >> 2);
		if (log_allowed)
			rack_log_dsack_event(rack, 4, line, srtt, thresh);
	}
	/* SRTT * 2 is the ceiling */
	if (thresh > (srtt * 2)) {
		thresh = srtt * 2;
	}
	/* And we don't want it above the RTO max either */
	if (thresh > rack_rto_max) {
		thresh = rack_rto_max;
	}
	if (log_allowed)
		rack_log_dsack_event(rack, 6, line,  srtt, thresh);
	return (thresh);
}

static uint32_t
rack_calc_thresh_tlp(struct tcpcb *tp, struct tcp_rack *rack,
		     struct rack_sendmap *rsm, uint32_t srtt)
{
	struct rack_sendmap *prsm;
	uint32_t thresh, len;
	int segsiz;

	if (srtt == 0)
		srtt = 1;
	if (rack->r_ctl.rc_tlp_threshold)
		thresh = srtt + (srtt / rack->r_ctl.rc_tlp_threshold);
	else
		thresh = (srtt * 2);

	/* Get the previous sent packet, if any */
	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	len = rsm->r_end - rsm->r_start;
	if (rack->rack_tlp_threshold_use == TLP_USE_ID) {
		/* Exactly like the ID */
		if (((tp->snd_max - tp->snd_una) - rack->r_ctl.rc_sacked + rack->r_ctl.rc_holes_rxt) <= segsiz) {
			uint32_t alt_thresh;
			/*
			 * Compensate for delayed-ack with the d-ack time.
			 */
			alt_thresh = srtt + (srtt / 2) + rack_delayed_ack_time;
			if (alt_thresh > thresh)
				thresh = alt_thresh;
		}
	} else if (rack->rack_tlp_threshold_use == TLP_USE_TWO_ONE) {
		/* 2.1 behavior */
		prsm = TAILQ_PREV(rsm, rack_head, r_tnext);
		if (prsm && (len <= segsiz)) {
			/*
			 * Two packets outstanding, thresh should be (2*srtt) +
			 * possible inter-packet delay (if any).
			 */
			uint32_t inter_gap = 0;
			int idx, nidx;

			idx = rsm->r_rtr_cnt - 1;
			nidx = prsm->r_rtr_cnt - 1;
			if (rsm->r_tim_lastsent[nidx] >= prsm->r_tim_lastsent[idx]) {
				/* Yes it was sent later (or at the same time) */
				inter_gap = rsm->r_tim_lastsent[idx] - prsm->r_tim_lastsent[nidx];
			}
			thresh += inter_gap;
		} else if (len <= segsiz) {
			/*
			 * Possibly compensate for delayed-ack.
			 */
			uint32_t alt_thresh;

			alt_thresh = srtt + (srtt / 2) + rack_delayed_ack_time;
			if (alt_thresh > thresh)
				thresh = alt_thresh;
		}
	} else if (rack->rack_tlp_threshold_use == TLP_USE_TWO_TWO) {
		/* 2.2 behavior */
		if (len <= segsiz) {
			uint32_t alt_thresh;
			/*
			 * Compensate for delayed-ack with the d-ack time.
			 */
			alt_thresh = srtt + (srtt / 2) + rack_delayed_ack_time;
			if (alt_thresh > thresh)
				thresh = alt_thresh;
		}
	}
	/* Not above an RTO */
	if (thresh > tp->t_rxtcur) {
		thresh = tp->t_rxtcur;
	}
	/* Not above a RTO max */
	if (thresh > rack_rto_max) {
		thresh = rack_rto_max;
	}
	/* Apply user supplied min TLP */
	if (thresh < rack_tlp_min) {
		thresh = rack_tlp_min;
	}
	return (thresh);
}

static uint32_t
rack_grab_rtt(struct tcpcb *tp, struct tcp_rack *rack)
{
	/*
	 * We want the rack_rtt which is the
	 * last rtt we measured. However if that
	 * does not exist we fallback to the srtt (which
	 * we probably will never do) and then as a last
	 * resort we use RACK_INITIAL_RTO if no srtt is
	 * yet set.
	 */
	if (rack->rc_rack_rtt)
		return (rack->rc_rack_rtt);
	else if (tp->t_srtt == 0)
		return (RACK_INITIAL_RTO);
	return (tp->t_srtt);
}

static struct rack_sendmap *
rack_check_recovery_mode(struct tcpcb *tp, uint32_t tsused)
{
	/*
	 * Check to see that we don't need to fall into recovery. We will
	 * need to do so if our oldest transmit is past the time we should
	 * have had an ack.
	 */
	struct tcp_rack *rack;
	struct rack_sendmap *rsm;
	int32_t idx;
	uint32_t srtt, thresh;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (tqhash_empty(rack->r_ctl.tqh)) {
		return (NULL);
	}
	rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
	if (rsm == NULL)
		return (NULL);


	if (rsm->r_flags & RACK_ACKED) {
		rsm = rack_find_lowest_rsm(rack);
		if (rsm == NULL)
			return (NULL);
	}
	idx = rsm->r_rtr_cnt - 1;
	srtt = rack_grab_rtt(tp, rack);
	thresh = rack_calc_thresh_rack(rack, srtt, tsused, __LINE__, 1);
	if (TSTMP_LT(tsused, ((uint32_t)rsm->r_tim_lastsent[idx]))) {
		return (NULL);
	}
	if ((tsused - ((uint32_t)rsm->r_tim_lastsent[idx])) < thresh) {
		return (NULL);
	}
	/* Ok if we reach here we are over-due and this guy can be sent */
	rack_cong_signal(tp, CC_NDUPACK, tp->snd_una, __LINE__);
	return (rsm);
}

static uint32_t
rack_get_persists_timer_val(struct tcpcb *tp, struct tcp_rack *rack)
{
	int32_t t;
	int32_t tt;
	uint32_t ret_val;

	t = (tp->t_srtt + (tp->t_rttvar << 2));
	RACK_TCPT_RANGESET(tt, t * tcp_backoff[tp->t_rxtshift],
 	    rack_persist_min, rack_persist_max, rack->r_ctl.timer_slop);
	rack->r_ctl.rc_hpts_flags |= PACE_TMR_PERSIT;
	ret_val = (uint32_t)tt;
	return (ret_val);
}

static uint32_t
rack_timer_start(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts, int sup_rack)
{
	/*
	 * Start the FR timer, we do this based on getting the first one in
	 * the rc_tmap. Note that if its NULL we must stop the timer. in all
	 * events we need to stop the running timer (if its running) before
	 * starting the new one.
	 */
	uint32_t thresh, exp, to, srtt, time_since_sent, tstmp_touse;
	uint32_t srtt_cur;
	int32_t idx;
	int32_t is_tlp_timer = 0;
	struct rack_sendmap *rsm;

	if (rack->t_timers_stopped) {
		/* All timers have been stopped none are to run */
		return (0);
	}
	if (rack->rc_in_persist) {
		/* We can't start any timer in persists */
		return (rack_get_persists_timer_val(tp, rack));
	}
	rack->rc_on_min_to = 0;
	if ((tp->t_state < TCPS_ESTABLISHED) ||
	    (rack->sack_attack_disable > 0) ||
	    ((tp->t_flags & TF_SACK_PERMIT) == 0)) {
		goto activate_rxt;
	}
	rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
	if ((rsm == NULL) || sup_rack) {
		/* Nothing on the send map or no rack */
activate_rxt:
		time_since_sent = 0;
		rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
		if (rsm) {
			/*
			 * Should we discount the RTX timer any?
			 *
			 * We want to discount it the smallest amount.
			 * If a timer (Rack/TLP or RXT) has gone off more
			 * recently thats the discount we want to use (now - timer time).
			 * If the retransmit of the oldest packet was more recent then
			 * we want to use that (now - oldest-packet-last_transmit_time).
			 *
			 */
			idx = rsm->r_rtr_cnt - 1;
			if (TSTMP_GEQ(rack->r_ctl.rc_tlp_rxt_last_time, ((uint32_t)rsm->r_tim_lastsent[idx])))
				tstmp_touse = (uint32_t)rack->r_ctl.rc_tlp_rxt_last_time;
			else
				tstmp_touse = (uint32_t)rsm->r_tim_lastsent[idx];
			if (TSTMP_GT(cts, tstmp_touse))
			    time_since_sent = cts - tstmp_touse;
		}
		if (SEQ_LT(tp->snd_una, tp->snd_max) ||
		    sbavail(&tptosocket(tp)->so_snd)) {
			rack->r_ctl.rc_hpts_flags |= PACE_TMR_RXT;
			to = tp->t_rxtcur;
			if (to > time_since_sent)
				to -= time_since_sent;
			else
				to = rack->r_ctl.rc_min_to;
			if (to == 0)
				to = 1;
			/* Special case for KEEPINIT */
			if ((TCPS_HAVEESTABLISHED(tp->t_state) == 0) &&
			    (TP_KEEPINIT(tp) != 0) &&
			    rsm) {
				/*
				 * We have to put a ceiling on the rxt timer
				 * of the keep-init timeout.
				 */
				uint32_t max_time, red;

				max_time = TICKS_2_USEC(TP_KEEPINIT(tp));
				if (TSTMP_GT(cts, (uint32_t)rsm->r_tim_lastsent[0])) {
					red = (cts - (uint32_t)rsm->r_tim_lastsent[0]);
					if (red < max_time)
						max_time -= red;
					else
						max_time = 1;
				}
				/* Reduce timeout to the keep value if needed */
				if (max_time < to)
					to = max_time;
			}
			return (to);
		}
		return (0);
	}
	if (rsm->r_flags & RACK_ACKED) {
		rsm = rack_find_lowest_rsm(rack);
		if (rsm == NULL) {
			/* No lowest? */
			goto activate_rxt;
		}
	}
	if (rack->sack_attack_disable) {
		/*
		 * We don't want to do
		 * any TLP's if you are an attacker.
		 * Though if you are doing what
		 * is expected you may still have
		 * SACK-PASSED marks.
		 */
		goto activate_rxt;
	}
	/* Convert from ms to usecs */
	if ((rsm->r_flags & RACK_SACK_PASSED) ||
	    (rsm->r_flags & RACK_RWND_COLLAPSED) ||
	    (rsm->r_dupack >= DUP_ACK_THRESHOLD)) {
		if ((tp->t_flags & TF_SENTFIN) &&
		    ((tp->snd_max - tp->snd_una) == 1) &&
		    (rsm->r_flags & RACK_HAS_FIN)) {
			/*
			 * We don't start a rack timer if all we have is a
			 * FIN outstanding.
			 */
			goto activate_rxt;
		}
		if ((rack->use_rack_rr == 0) &&
		    (IN_FASTRECOVERY(tp->t_flags)) &&
		    (rack->rack_no_prr == 0) &&
		     (rack->r_ctl.rc_prr_sndcnt  < ctf_fixed_maxseg(tp))) {
			/*
			 * We are not cheating, in recovery  and
			 * not enough ack's to yet get our next
			 * retransmission out.
			 *
			 * Note that classified attackers do not
			 * get to use the rack-cheat.
			 */
			goto activate_tlp;
		}
		srtt = rack_grab_rtt(tp, rack);
		thresh = rack_calc_thresh_rack(rack, srtt, cts, __LINE__, 1);
		idx = rsm->r_rtr_cnt - 1;
		exp = ((uint32_t)rsm->r_tim_lastsent[idx]) + thresh;
		if (SEQ_GEQ(exp, cts)) {
			to = exp - cts;
			if (to < rack->r_ctl.rc_min_to) {
				to = rack->r_ctl.rc_min_to;
				if (rack->r_rr_config == 3)
					rack->rc_on_min_to = 1;
			}
		} else {
			to = rack->r_ctl.rc_min_to;
			if (rack->r_rr_config == 3)
				rack->rc_on_min_to = 1;
		}
	} else {
		/* Ok we need to do a TLP not RACK */
activate_tlp:
		if ((rack->rc_tlp_in_progress != 0) &&
		    (rack->r_ctl.rc_tlp_cnt_out >= rack_tlp_limit)) {
			/*
			 * The previous send was a TLP and we have sent
			 * N TLP's without sending new data.
			 */
			goto activate_rxt;
		}
		rsm = TAILQ_LAST_FAST(&rack->r_ctl.rc_tmap, rack_sendmap, r_tnext);
		if (rsm == NULL) {
			/* We found no rsm to TLP with. */
			goto activate_rxt;
		}
		if (rsm->r_flags & RACK_HAS_FIN) {
			/* If its a FIN we dont do TLP */
			rsm = NULL;
			goto activate_rxt;
		}
		idx = rsm->r_rtr_cnt - 1;
		time_since_sent = 0;
		if (TSTMP_GEQ(((uint32_t)rsm->r_tim_lastsent[idx]), rack->r_ctl.rc_tlp_rxt_last_time))
			tstmp_touse = (uint32_t)rsm->r_tim_lastsent[idx];
		else
			tstmp_touse = (uint32_t)rack->r_ctl.rc_tlp_rxt_last_time;
		if (TSTMP_GT(cts, tstmp_touse))
		    time_since_sent = cts - tstmp_touse;
		is_tlp_timer = 1;
		if (tp->t_srtt) {
			if ((rack->rc_srtt_measure_made == 0) &&
			    (tp->t_srtt == 1)) {
				/*
				 * If another stack as run and set srtt to 1,
				 * then the srtt was 0, so lets use the initial.
				 */
				srtt = RACK_INITIAL_RTO;
			} else {
				srtt_cur = tp->t_srtt;
				srtt = srtt_cur;
			}
		} else
			srtt = RACK_INITIAL_RTO;
		/*
		 * If the SRTT is not keeping up and the
		 * rack RTT has spiked we want to use
		 * the last RTT not the smoothed one.
		 */
		if (rack_tlp_use_greater &&
		    tp->t_srtt &&
		    (srtt < rack_grab_rtt(tp, rack))) {
			srtt = rack_grab_rtt(tp, rack);
		}
		thresh = rack_calc_thresh_tlp(tp, rack, rsm, srtt);
		if (thresh > time_since_sent) {
			to = thresh - time_since_sent;
		} else {
			to = rack->r_ctl.rc_min_to;
			rack_log_alt_to_to_cancel(rack,
						  thresh,		/* flex1 */
						  time_since_sent,	/* flex2 */
						  tstmp_touse,		/* flex3 */
						  rack->r_ctl.rc_tlp_rxt_last_time, /* flex4 */
						  (uint32_t)rsm->r_tim_lastsent[idx],
						  srtt,
						  idx, 99);
		}
		if (to < rack_tlp_min) {
			to = rack_tlp_min;
		}
		if (to > TICKS_2_USEC(TCPTV_REXMTMAX)) {
			/*
			 * If the TLP time works out to larger than the max
			 * RTO lets not do TLP.. just RTO.
			 */
			goto activate_rxt;
		}
	}
	if (is_tlp_timer == 0) {
		rack->r_ctl.rc_hpts_flags |= PACE_TMR_RACK;
	} else {
		rack->r_ctl.rc_hpts_flags |= PACE_TMR_TLP;
	}
	if (to == 0)
		to = 1;
	return (to);
}

static void
rack_enter_persist(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts, tcp_seq snd_una)
{
	if (rack->rc_in_persist == 0) {
		if (tp->t_flags & TF_GPUTINPROG) {
			/*
			 * Stop the goodput now, the calling of the
			 * measurement function clears the flag.
			 */
			rack_do_goodput_measurement(tp, rack, tp->snd_una, __LINE__,
						    RACK_QUALITY_PERSIST);
		}
#ifdef NETFLIX_SHARED_CWND
		if (rack->r_ctl.rc_scw) {
			tcp_shared_cwnd_idle(rack->r_ctl.rc_scw, rack->r_ctl.rc_scw_index);
			rack->rack_scwnd_is_idle = 1;
		}
#endif
		rack->r_ctl.rc_went_idle_time = cts;
		if (rack->r_ctl.rc_went_idle_time == 0)
			rack->r_ctl.rc_went_idle_time = 1;
		if (rack->lt_bw_up) {
			/* Suspend our LT BW measurement */
			uint64_t tmark;

			rack->r_ctl.lt_bw_bytes += (snd_una - rack->r_ctl.lt_seq);
			rack->r_ctl.lt_seq = snd_una;
			tmark = tcp_tv_to_lusectick(&rack->r_ctl.act_rcv_time);
			if (tmark >= rack->r_ctl.lt_timemark) {
				rack->r_ctl.lt_bw_time += (tmark - rack->r_ctl.lt_timemark);
			}
			rack->r_ctl.lt_timemark = tmark;
			rack->lt_bw_up = 0;
			rack->r_persist_lt_bw_off = 1;
		}
		rack_timer_cancel(tp, rack, cts, __LINE__);
		rack->r_ctl.persist_lost_ends = 0;
		rack->probe_not_answered = 0;
		rack->forced_ack = 0;
		tp->t_rxtshift = 0;
		RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
			      rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
		rack->rc_in_persist = 1;
	}
}

static void
rack_exit_persist(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts)
{
	if (tcp_in_hpts(rack->rc_tp)) {
		tcp_hpts_remove(rack->rc_tp);
		rack->r_ctl.rc_hpts_flags = 0;
	}
#ifdef NETFLIX_SHARED_CWND
	if (rack->r_ctl.rc_scw) {
		tcp_shared_cwnd_active(rack->r_ctl.rc_scw, rack->r_ctl.rc_scw_index);
		rack->rack_scwnd_is_idle = 0;
	}
#endif
	if (rack->rc_gp_dyn_mul &&
	    (rack->use_fixed_rate == 0) &&
	    (rack->rc_always_pace)) {
		/*
		 * Do we count this as if a probe-rtt just
		 * finished?
		 */
		uint32_t time_idle, idle_min;

		time_idle = cts - rack->r_ctl.rc_went_idle_time;
		idle_min = rack_min_probertt_hold;
		if (rack_probertt_gpsrtt_cnt_div) {
			uint64_t extra;
			extra = (uint64_t)rack->r_ctl.rc_gp_srtt *
				(uint64_t)rack_probertt_gpsrtt_cnt_mul;
			extra /= (uint64_t)rack_probertt_gpsrtt_cnt_div;
			idle_min += (uint32_t)extra;
		}
		if (time_idle >= idle_min) {
			/* Yes, we count it as a probe-rtt. */
			uint32_t us_cts;

			us_cts = tcp_get_usecs(NULL);
			if (rack->in_probe_rtt == 0) {
				rack->r_ctl.rc_lower_rtt_us_cts = us_cts;
				rack->r_ctl.rc_time_probertt_entered = rack->r_ctl.rc_lower_rtt_us_cts;
				rack->r_ctl.rc_time_probertt_starts = rack->r_ctl.rc_lower_rtt_us_cts;
				rack->r_ctl.rc_time_of_last_probertt = rack->r_ctl.rc_lower_rtt_us_cts;
			} else {
				rack_exit_probertt(rack, us_cts);
			}
		}
	}
	if (rack->r_persist_lt_bw_off) {
		/* Continue where we left off */
		rack->r_ctl.lt_timemark = tcp_get_u64_usecs(NULL);
		rack->lt_bw_up = 1;
		rack->r_persist_lt_bw_off = 0;
	}
	rack->r_ctl.idle_snd_una = tp->snd_una;
	rack->rc_in_persist = 0;
	rack->r_ctl.rc_went_idle_time = 0;
	tp->t_rxtshift = 0;
	RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
	   rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
	rack->r_ctl.rc_agg_delayed = 0;
	rack->r_early = 0;
	rack->r_late = 0;
	rack->r_ctl.rc_agg_early = 0;
}

static void
rack_log_hpts_diag(struct tcp_rack *rack, uint32_t cts,
		   struct hpts_diag *diag, struct timeval *tv)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = diag->p_nxt_slot;
		log.u_bbr.flex2 = diag->p_cur_slot;
		log.u_bbr.flex3 = diag->slot_req;
		log.u_bbr.flex4 = diag->inp_hptsslot;
		log.u_bbr.flex5 = diag->slot_remaining;
		log.u_bbr.flex6 = diag->need_new_to;
		log.u_bbr.flex7 = diag->p_hpts_active;
		log.u_bbr.flex8 = diag->p_on_min_sleep;
		/* Hijack other fields as needed */
		log.u_bbr.epoch = diag->have_slept;
		log.u_bbr.lt_epoch = diag->yet_to_sleep;
		log.u_bbr.pkts_out = diag->co_ret;
		log.u_bbr.applimited = diag->hpts_sleep_time;
		log.u_bbr.delivered = diag->p_prev_slot;
		log.u_bbr.inflight = diag->p_runningslot;
		log.u_bbr.bw_inuse = diag->wheel_slot;
		log.u_bbr.rttProp = diag->wheel_cts;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.delRate = diag->maxslots;
		log.u_bbr.cur_del_rate = diag->p_curtick;
		log.u_bbr.cur_del_rate <<= 32;
		log.u_bbr.cur_del_rate |= diag->p_lasttick;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_HPTSDIAG, 0,
		    0, &log, false, tv);
	}

}

static void
rack_log_wakeup(struct tcpcb *tp, struct tcp_rack *rack, struct sockbuf *sb, uint32_t len, int type)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.flex1 = sb->sb_flags;
		log.u_bbr.flex2 = len;
		log.u_bbr.flex3 = sb->sb_state;
		log.u_bbr.flex8 = type;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_LOG_SB_WAKE, 0,
		    len, &log, false, &tv);
	}
}

static void
rack_start_hpts_timer (struct tcp_rack *rack, struct tcpcb *tp, uint32_t cts,
      int32_t slot, uint32_t tot_len_this_send, int sup_rack)
{
	struct hpts_diag diag;
	struct inpcb *inp = tptoinpcb(tp);
	struct timeval tv;
	uint32_t delayed_ack = 0;
	uint32_t hpts_timeout;
	uint32_t entry_slot = slot;
	uint8_t stopped;
	uint32_t left = 0;
	uint32_t us_cts;

	if ((tp->t_state == TCPS_CLOSED) ||
	    (tp->t_state == TCPS_LISTEN)) {
		return;
	}
	if (tcp_in_hpts(tp)) {
		/* Already on the pacer */
		return;
	}
	stopped = rack->rc_tmr_stopped;
	if (stopped && TSTMP_GT(rack->r_ctl.rc_timer_exp, cts)) {
		left = rack->r_ctl.rc_timer_exp - cts;
	}
	rack->r_ctl.rc_timer_exp = 0;
	rack->r_ctl.rc_hpts_flags = 0;
	us_cts = tcp_get_usecs(&tv);
	/* Now early/late accounting */
	rack_log_pacing_delay_calc(rack, entry_slot, slot, 0, 0, 0, 26, __LINE__, NULL, 0);
	if (rack->r_early && (rack->rc_ack_can_sendout_data == 0)) {
		/*
		 * We have a early carry over set,
		 * we can always add more time so we
		 * can always make this compensation.
		 *
		 * Note if ack's are allowed to wake us do not
		 * penalize the next timer for being awoke
		 * by an ack aka the rc_agg_early (non-paced mode).
		 */
		slot += rack->r_ctl.rc_agg_early;
		rack->r_early = 0;
		rack->r_ctl.rc_agg_early = 0;
	}
	if ((rack->r_late) &&
	    ((rack->r_use_hpts_min == 0) || (rack->dgp_on == 0))) {
		/*
		 * This is harder, we can
		 * compensate some but it
		 * really depends on what
		 * the current pacing time is.
		 */
		if (rack->r_ctl.rc_agg_delayed >= slot) {
			/*
			 * We can't compensate for it all.
			 * And we have to have some time
			 * on the clock. We always have a min
			 * 10 slots (10 x 10 i.e. 100 usecs).
			 */
			if (slot <= HPTS_TICKS_PER_SLOT) {
				/* We gain delay */
				rack->r_ctl.rc_agg_delayed += (HPTS_TICKS_PER_SLOT - slot);
				slot = HPTS_TICKS_PER_SLOT;
			} else {
				/* We take off some */
				rack->r_ctl.rc_agg_delayed -= (slot - HPTS_TICKS_PER_SLOT);
				slot = HPTS_TICKS_PER_SLOT;
			}
		} else {
			slot -= rack->r_ctl.rc_agg_delayed;
			rack->r_ctl.rc_agg_delayed = 0;
			/* Make sure we have 100 useconds at minimum */
			if (slot < HPTS_TICKS_PER_SLOT) {
				rack->r_ctl.rc_agg_delayed = HPTS_TICKS_PER_SLOT - slot;
				slot = HPTS_TICKS_PER_SLOT;
			}
			if (rack->r_ctl.rc_agg_delayed == 0)
				rack->r_late = 0;
		}
	} else if (rack->r_late) {
		/* r_use_hpts_min is on and so is DGP */
		uint32_t max_red;

		max_red = (slot * rack->r_ctl.max_reduction) / 100;
		if (max_red >= rack->r_ctl.rc_agg_delayed) {
			slot -= rack->r_ctl.rc_agg_delayed;
			rack->r_ctl.rc_agg_delayed = 0;
		} else {
			slot -= max_red;
			rack->r_ctl.rc_agg_delayed -= max_red;
		}
	}
	if ((rack->r_use_hpts_min == 1) &&
	    (slot > 0) &&
	    (rack->dgp_on == 1)) {
		/*
		 * We are enforcing a min pacing timer
		 * based on our hpts min timeout.
		 */
		uint32_t min;

		min = get_hpts_min_sleep_time();
		if (min > slot) {
			slot = min;
		}
	}
	hpts_timeout = rack_timer_start(tp, rack, cts, sup_rack);
#ifdef TCP_SAD_DETECTION
	if (rack->sack_attack_disable &&
	    (rack->r_ctl.ack_during_sd > 0) &&
	    (slot < tcp_sad_pacing_interval)) {
		/*
		 * We have a potential attacker on
		 * the line. We have possibly some
		 * (or now) pacing time set. We want to
		 * slow down the processing of sacks by some
		 * amount (if it is an attacker). Set the default
		 * slot for attackers in place (unless the original
		 * interval is longer). Its stored in
		 * micro-seconds, so lets convert to msecs.
		 */
		slot = tcp_sad_pacing_interval;
		rack_log_type_bbrsnd(rack, tot_len_this_send, slot, us_cts, &tv, __LINE__);
		rack->r_ctl.ack_during_sd = 0;
	}
#endif
	if (tp->t_flags & TF_DELACK) {
		delayed_ack = TICKS_2_USEC(tcp_delacktime);
		rack->r_ctl.rc_hpts_flags |= PACE_TMR_DELACK;
	}
	if (delayed_ack && ((hpts_timeout == 0) ||
			    (delayed_ack < hpts_timeout)))
		hpts_timeout = delayed_ack;
	else
		rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_DELACK;
	/*
	 * If no timers are going to run and we will fall off the hptsi
	 * wheel, we resort to a keep-alive timer if its configured.
	 */
	if ((hpts_timeout == 0) &&
	    (slot == 0)) {
		if ((V_tcp_always_keepalive || inp->inp_socket->so_options & SO_KEEPALIVE) &&
		    (tp->t_state <= TCPS_CLOSING)) {
			/*
			 * Ok we have no timer (persists, rack, tlp, rxt  or
			 * del-ack), we don't have segments being paced. So
			 * all that is left is the keepalive timer.
			 */
			if (TCPS_HAVEESTABLISHED(tp->t_state)) {
				/* Get the established keep-alive time */
				hpts_timeout = TICKS_2_USEC(TP_KEEPIDLE(tp));
			} else {
				/*
				 * Get the initial setup keep-alive time,
				 * note that this is probably not going to
				 * happen, since rack will be running a rxt timer
				 * if a SYN of some sort is outstanding. It is
				 * actually handled in rack_timeout_rxt().
				 */
				hpts_timeout = TICKS_2_USEC(TP_KEEPINIT(tp));
			}
			rack->r_ctl.rc_hpts_flags |= PACE_TMR_KEEP;
			if (rack->in_probe_rtt) {
				/*
				 * We want to instead not wake up a long time from
				 * now but to wake up about the time we would
				 * exit probe-rtt and initiate a keep-alive ack.
				 * This will get us out of probe-rtt and update
				 * our min-rtt.
				 */
				hpts_timeout = rack_min_probertt_hold;
			}
		}
	}
	if (left && (stopped & (PACE_TMR_KEEP | PACE_TMR_DELACK)) ==
	    (rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK)) {
		/*
		 * RACK, TLP, persists and RXT timers all are restartable
		 * based on actions input .. i.e we received a packet (ack
		 * or sack) and that changes things (rw, or snd_una etc).
		 * Thus we can restart them with a new value. For
		 * keep-alive, delayed_ack we keep track of what was left
		 * and restart the timer with a smaller value.
		 */
		if (left < hpts_timeout)
			hpts_timeout = left;
	}
	if (hpts_timeout) {
		/*
		 * Hack alert for now we can't time-out over 2,147,483
		 * seconds (a bit more than 596 hours), which is probably ok
		 * :).
		 */
		if (hpts_timeout > 0x7ffffffe)
			hpts_timeout = 0x7ffffffe;
		rack->r_ctl.rc_timer_exp = cts + hpts_timeout;
	}
	rack_log_pacing_delay_calc(rack, entry_slot, slot, hpts_timeout, 0, 0, 27, __LINE__, NULL, 0);
	if ((rack->gp_ready == 0) &&
	    (rack->use_fixed_rate == 0) &&
	    (hpts_timeout < slot) &&
	    (rack->r_ctl.rc_hpts_flags & (PACE_TMR_TLP|PACE_TMR_RXT))) {
		/*
		 * We have no good estimate yet for the
		 * old clunky burst mitigation or the
		 * real pacing. And the tlp or rxt is smaller
		 * than the pacing calculation. Lets not
		 * pace that long since we know the calculation
		 * so far is not accurate.
		 */
		slot = hpts_timeout;
	}
	/**
	 * Turn off all the flags for queuing by default. The
	 * flags have important meanings to what happens when
	 * LRO interacts with the transport. Most likely (by default now)
	 * mbuf_queueing and ack compression are on. So the transport
	 * has a couple of flags that control what happens (if those
	 * are not on then these flags won't have any effect since it
	 * won't go through the queuing LRO path).
	 *
	 * TF2_MBUF_QUEUE_READY - This flags says that I am busy
	 *                        pacing output, so don't disturb. But
	 *                        it also means LRO can wake me if there
	 *                        is a SACK arrival.
	 *
	 * TF2_DONT_SACK_QUEUE - This flag is used in conjunction
	 *                       with the above flag (QUEUE_READY) and
	 *                       when present it says don't even wake me
	 *                       if a SACK arrives.
	 *
	 * The idea behind these flags is that if we are pacing we
	 * set the MBUF_QUEUE_READY and only get woken up if
	 * a SACK arrives (which could change things) or if
	 * our pacing timer expires. If, however, we have a rack
	 * timer running, then we don't even want a sack to wake
	 * us since the rack timer has to expire before we can send.
	 *
	 * Other cases should usually have none of the flags set
	 * so LRO can call into us.
	 */
	tp->t_flags2 &= ~(TF2_DONT_SACK_QUEUE|TF2_MBUF_QUEUE_READY);
	if (slot) {
		rack->r_ctl.rc_hpts_flags |= PACE_PKT_OUTPUT;
		rack->r_ctl.rc_last_output_to = us_cts + slot;
		/*
		 * A pacing timer (slot) is being set, in
		 * such a case we cannot send (we are blocked by
		 * the timer). So lets tell LRO that it should not
		 * wake us unless there is a SACK. Note this only
		 * will be effective if mbuf queueing is on or
		 * compressed acks are being processed.
		 */
		tp->t_flags2 |= TF2_MBUF_QUEUE_READY;
		/*
		 * But wait if we have a Rack timer running
		 * even a SACK should not disturb us (with
		 * the exception of r_rr_config 3).
		 */
		if ((rack->r_ctl.rc_hpts_flags & PACE_TMR_RACK) ||
		    (IN_RECOVERY(tp->t_flags))) {
			if (rack->r_rr_config != 3)
				tp->t_flags2 |= TF2_DONT_SACK_QUEUE;
			else if (rack->rc_pace_dnd) {
				/*
				 * When DND is on, we only let a sack
				 * interrupt us if we are not in recovery.
				 *
				 * If DND is off, then we never hit here
				 * and let all sacks wake us up.
				 *
				 */
				tp->t_flags2 |= TF2_DONT_SACK_QUEUE;
			}
		}
		/* For sack attackers we want to ignore sack */
		if (rack->sack_attack_disable == 1) {
			tp->t_flags2 |= (TF2_DONT_SACK_QUEUE |
			    TF2_MBUF_QUEUE_READY);
		} else if (rack->rc_ack_can_sendout_data) {
			/*
			 * Ahh but wait, this is that special case
			 * where the pacing timer can be disturbed
			 * backout the changes (used for non-paced
			 * burst limiting).
			 */
			tp->t_flags2 &= ~(TF2_DONT_SACK_QUEUE |
			    TF2_MBUF_QUEUE_READY);
		}
		if ((rack->use_rack_rr) &&
		    (rack->r_rr_config < 2) &&
		    ((hpts_timeout) && (hpts_timeout < slot))) {
			/*
			 * Arrange for the hpts to kick back in after the
			 * t-o if the t-o does not cause a send.
			 */
			(void)tcp_hpts_insert_diag(tp, HPTS_USEC_TO_SLOTS(hpts_timeout),
						   __LINE__, &diag);
			rack_log_hpts_diag(rack, us_cts, &diag, &tv);
			rack_log_to_start(rack, cts, hpts_timeout, slot, 0);
		} else {
			(void)tcp_hpts_insert_diag(tp, HPTS_USEC_TO_SLOTS(slot),
						   __LINE__, &diag);
			rack_log_hpts_diag(rack, us_cts, &diag, &tv);
			rack_log_to_start(rack, cts, hpts_timeout, slot, 1);
		}
	} else if (hpts_timeout) {
		/*
		 * With respect to t_flags2(?) here, lets let any new acks wake
		 * us up here. Since we are not pacing (no pacing timer), output
		 * can happen so we should let it. If its a Rack timer, then any inbound
		 * packet probably won't change the sending (we will be blocked)
		 * but it may change the prr stats so letting it in (the set defaults
		 * at the start of this block) are good enough.
		 */
		rack->r_ctl.rc_hpts_flags &= ~PACE_PKT_OUTPUT;
		(void)tcp_hpts_insert_diag(tp, HPTS_USEC_TO_SLOTS(hpts_timeout),
					   __LINE__, &diag);
		rack_log_hpts_diag(rack, us_cts, &diag, &tv);
		rack_log_to_start(rack, cts, hpts_timeout, slot, 0);
	} else {
		/* No timer starting */
#ifdef INVARIANTS
		if (SEQ_GT(tp->snd_max, tp->snd_una)) {
			panic("tp:%p rack:%p tlts:%d cts:%u slot:%u pto:%u -- no timer started?",
			    tp, rack, tot_len_this_send, cts, slot, hpts_timeout);
		}
#endif
	}
	rack->rc_tmr_stopped = 0;
	if (slot)
		rack_log_type_bbrsnd(rack, tot_len_this_send, slot, us_cts, &tv, __LINE__);
}

static void
rack_mark_lost(struct tcpcb *tp,
    struct tcp_rack *rack, struct rack_sendmap *rsm, uint32_t cts)
{
	struct rack_sendmap *nrsm;
	uint32_t thresh,  exp;

	thresh = rack_calc_thresh_rack(rack, rack_grab_rtt(tp, rack), cts, __LINE__, 0);
	nrsm = rsm;
	TAILQ_FOREACH_FROM(nrsm, &rack->r_ctl.rc_tmap, r_tnext) {
		if ((nrsm->r_flags & RACK_SACK_PASSED) == 0) {
			/* Got up to all that were marked sack-passed */
			break;
		}
		if ((nrsm->r_flags & RACK_WAS_LOST) == 0) {
			exp = ((uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)]) + thresh;
			if (TSTMP_LT(exp, cts) || (exp == cts)) {
				/* We now consider it lost */
				nrsm->r_flags |= RACK_WAS_LOST;
				rack->r_ctl.rc_considered_lost += nrsm->r_end - nrsm->r_start;
			} else {
				/* Past here it won't be lost so stop */
				break;
			}
		}
	}
}

/*
 * RACK Timer, here we simply do logging and house keeping.
 * the normal rack_output() function will call the
 * appropriate thing to check if we need to do a RACK retransmit.
 * We return 1, saying don't proceed with rack_output only
 * when all timers have been stopped (destroyed PCB?).
 */
static int
rack_timeout_rack(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts)
{
	/*
	 * This timer simply provides an internal trigger to send out data.
	 * The check_recovery_mode call will see if there are needed
	 * retransmissions, if so we will enter fast-recovery. The output
	 * call may or may not do the same thing depending on sysctl
	 * settings.
	 */
	struct rack_sendmap *rsm;

	counter_u64_add(rack_to_tot, 1);
	if (rack->r_state && (rack->r_state != tp->t_state))
		rack_set_state(tp, rack);
	rack->rc_on_min_to = 0;
	rsm = rack_check_recovery_mode(tp, cts);
	rack_log_to_event(rack, RACK_TO_FRM_RACK, rsm);
	if (rsm) {
		/* We need to stroke any lost that are now declared as lost */
		rack_mark_lost(tp, rack, rsm, cts);
		rack->r_ctl.rc_resend = rsm;
		rack->r_timer_override = 1;
		if (rack->use_rack_rr) {
			/*
			 * Don't accumulate extra pacing delay
			 * we are allowing the rack timer to
			 * over-ride pacing i.e. rrr takes precedence
			 * if the pacing interval is longer than the rrr
			 * time (in other words we get the min pacing
			 * time versus rrr pacing time).
			 */
			rack->r_ctl.rc_hpts_flags &= ~PACE_PKT_OUTPUT;
		}
	}
	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_RACK;
	if (rsm == NULL) {
		/* restart a timer and return 1 */
		rack_start_hpts_timer(rack, tp, cts,
				      0, 0, 0);
		return (1);
	}
	if ((rack->policer_detect_on == 1) &&
	    (rack->rc_policer_detected == 0)) {
		/*
		 * We do this early if we have not
		 * deteceted to attempt to detect
		 * quicker. Normally we want to do this
		 * as recovery exits (and we will again).
		 */
		policer_detection(tp, rack, 0);
	}
	return (0);
}



static void
rack_adjust_orig_mlen(struct rack_sendmap *rsm)
{

	if ((M_TRAILINGROOM(rsm->m) != rsm->orig_t_space)) {
		/*
		 * The trailing space changed, mbufs can grow
		 * at the tail but they can't shrink from
		 * it, KASSERT that. Adjust the orig_m_len to
		 * compensate for this change.
		 */
		KASSERT((rsm->orig_t_space > M_TRAILINGROOM(rsm->m)),
			("mbuf:%p rsm:%p trailing_space:%jd ots:%u oml:%u mlen:%u\n",
			 rsm->m,
			 rsm,
			 (intmax_t)M_TRAILINGROOM(rsm->m),
			 rsm->orig_t_space,
			 rsm->orig_m_len,
			 rsm->m->m_len));
		rsm->orig_m_len += (rsm->orig_t_space - M_TRAILINGROOM(rsm->m));
		rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
	}
	if (rsm->m->m_len < rsm->orig_m_len) {
		/*
		 * Mbuf shrank, trimmed off the top by an ack, our
		 * offset changes.
		 */
		KASSERT((rsm->soff >= (rsm->orig_m_len - rsm->m->m_len)),
			("mbuf:%p len:%u rsm:%p oml:%u soff:%u\n",
			 rsm->m, rsm->m->m_len,
			 rsm, rsm->orig_m_len,
			 rsm->soff));
		if (rsm->soff >= (rsm->orig_m_len - rsm->m->m_len))
			rsm->soff -= (rsm->orig_m_len - rsm->m->m_len);
		else
			rsm->soff = 0;
		rsm->orig_m_len = rsm->m->m_len;
#ifdef INVARIANTS
	} else if (rsm->m->m_len > rsm->orig_m_len) {
		panic("rsm:%p m:%p m_len grew outside of t_space compensation",
		      rsm, rsm->m);
#endif
	}
}

static void
rack_setup_offset_for_rsm(struct tcp_rack *rack, struct rack_sendmap *src_rsm, struct rack_sendmap *rsm)
{
	struct mbuf *m;
	uint32_t soff;

	if (src_rsm->m &&
	    ((src_rsm->orig_m_len != src_rsm->m->m_len) ||
	     (M_TRAILINGROOM(src_rsm->m) != src_rsm->orig_t_space))) {
		/* Fix up the orig_m_len and possibly the mbuf offset */
		rack_adjust_orig_mlen(src_rsm);
	}
	m = src_rsm->m;
	soff = src_rsm->soff + (src_rsm->r_end - src_rsm->r_start);
	while (soff >= m->m_len) {
		/* Move out past this mbuf */
		soff -= m->m_len;
		m = m->m_next;
		KASSERT((m != NULL),
			("rsm:%p nrsm:%p hit at soff:%u null m",
			 src_rsm, rsm, soff));
		if (m == NULL) {
			/* This should *not* happen which is why there is a kassert */
			src_rsm->m = sbsndmbuf(&rack->rc_inp->inp_socket->so_snd,
					       (src_rsm->r_start - rack->rc_tp->snd_una),
					       &src_rsm->soff);
			src_rsm->orig_m_len = src_rsm->m->m_len;
			src_rsm->orig_t_space = M_TRAILINGROOM(src_rsm->m);
			rsm->m = sbsndmbuf(&rack->rc_inp->inp_socket->so_snd,
					   (rsm->r_start - rack->rc_tp->snd_una),
					   &rsm->soff);
			rsm->orig_m_len = rsm->m->m_len;
			rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
			return;
		}
	}
	rsm->m = m;
	rsm->soff = soff;
	rsm->orig_m_len = m->m_len;
	rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
}

static __inline void
rack_clone_rsm(struct tcp_rack *rack, struct rack_sendmap *nrsm,
	       struct rack_sendmap *rsm, uint32_t start)
{
	int idx;

	nrsm->r_start = start;
	nrsm->r_end = rsm->r_end;
	nrsm->r_rtr_cnt = rsm->r_rtr_cnt;
	nrsm->r_act_rxt_cnt = rsm->r_act_rxt_cnt;
	nrsm->r_flags = rsm->r_flags;
	nrsm->r_dupack = rsm->r_dupack;
	nrsm->r_no_rtt_allowed = rsm->r_no_rtt_allowed;
	nrsm->r_rtr_bytes = 0;
	nrsm->r_fas = rsm->r_fas;
	nrsm->r_bas = rsm->r_bas;
	tqhash_update_end(rack->r_ctl.tqh, rsm, nrsm->r_start);
	nrsm->r_just_ret = rsm->r_just_ret;
	for (idx = 0; idx < nrsm->r_rtr_cnt; idx++) {
		nrsm->r_tim_lastsent[idx] = rsm->r_tim_lastsent[idx];
	}
	/* Now if we have SYN flag we keep it on the left edge */
	if (nrsm->r_flags & RACK_HAS_SYN)
		nrsm->r_flags &= ~RACK_HAS_SYN;
	/* Now if we have a FIN flag we keep it on the right edge */
	if (rsm->r_flags & RACK_HAS_FIN)
		rsm->r_flags &= ~RACK_HAS_FIN;
	/* Push bit must go to the right edge as well */
	if (rsm->r_flags & RACK_HAD_PUSH)
		rsm->r_flags &= ~RACK_HAD_PUSH;
	/* Clone over the state of the hw_tls flag */
	nrsm->r_hw_tls = rsm->r_hw_tls;
	/*
	 * Now we need to find nrsm's new location in the mbuf chain
	 * we basically calculate a new offset, which is soff +
	 * how much is left in original rsm. Then we walk out the mbuf
	 * chain to find the righ position, it may be the same mbuf
	 * or maybe not.
	 */
	KASSERT(((rsm->m != NULL) ||
		 (rsm->r_flags & (RACK_HAS_SYN|RACK_HAS_FIN))),
		("rsm:%p nrsm:%p rack:%p -- rsm->m is NULL?", rsm, nrsm, rack));
	if (rsm->m)
		rack_setup_offset_for_rsm(rack, rsm, nrsm);
}

static struct rack_sendmap *
rack_merge_rsm(struct tcp_rack *rack,
	       struct rack_sendmap *l_rsm,
	       struct rack_sendmap *r_rsm)
{
	/*
	 * We are merging two ack'd RSM's,
	 * the l_rsm is on the left (lower seq
	 * values) and the r_rsm is on the right
	 * (higher seq value). The simplest way
	 * to merge these is to move the right
	 * one into the left. I don't think there
	 * is any reason we need to try to find
	 * the oldest (or last oldest retransmitted).
	 */
	rack_log_map_chg(rack->rc_tp, rack, NULL,
			 l_rsm, r_rsm, MAP_MERGE, r_rsm->r_end, __LINE__);
	tqhash_update_end(rack->r_ctl.tqh, l_rsm, r_rsm->r_end);
	if (l_rsm->r_dupack < r_rsm->r_dupack)
		l_rsm->r_dupack = r_rsm->r_dupack;
	if (r_rsm->r_rtr_bytes)
		l_rsm->r_rtr_bytes += r_rsm->r_rtr_bytes;
	if (r_rsm->r_in_tmap) {
		/* This really should not happen */
		TAILQ_REMOVE(&rack->r_ctl.rc_tmap, r_rsm, r_tnext);
		r_rsm->r_in_tmap = 0;
	}

	/* Now the flags */
	if (r_rsm->r_flags & RACK_HAS_FIN)
		l_rsm->r_flags |= RACK_HAS_FIN;
	if (r_rsm->r_flags & RACK_TLP)
		l_rsm->r_flags |= RACK_TLP;
	if (r_rsm->r_flags & RACK_RWND_COLLAPSED)
		l_rsm->r_flags |= RACK_RWND_COLLAPSED;
	if ((r_rsm->r_flags & RACK_APP_LIMITED)  &&
	    ((l_rsm->r_flags & RACK_APP_LIMITED) == 0)) {
		/*
		 * If both are app-limited then let the
		 * free lower the count. If right is app
		 * limited and left is not, transfer.
		 */
		l_rsm->r_flags |= RACK_APP_LIMITED;
		r_rsm->r_flags &= ~RACK_APP_LIMITED;
		if (r_rsm == rack->r_ctl.rc_first_appl)
			rack->r_ctl.rc_first_appl = l_rsm;
	}
	tqhash_remove(rack->r_ctl.tqh, r_rsm, REMOVE_TYPE_MERGE);
	/*
	 * We keep the largest value, which is the newest
	 * send. We do this in case a segment that is
	 * joined together and not part of a GP estimate
	 * later gets expanded into the GP estimate.
	 *
	 * We prohibit the merging of unlike kinds i.e.
	 * all pieces that are in the GP estimate can be
	 * merged and all pieces that are not in a GP estimate
	 * can be merged, but not disimilar pieces. Combine
	 * this with taking the highest here and we should
	 * be ok unless of course the client reneges. Then
	 * all bets are off.
	 */
	if(l_rsm->r_tim_lastsent[(l_rsm->r_rtr_cnt-1)] <
	   r_rsm->r_tim_lastsent[(r_rsm->r_rtr_cnt-1)]) {
		l_rsm->r_tim_lastsent[(l_rsm->r_rtr_cnt-1)] = r_rsm->r_tim_lastsent[(r_rsm->r_rtr_cnt-1)];
	}
	/*
	 * When merging two RSM's we also need to consider the ack time and keep
	 * newest. If the ack gets merged into a measurement then that is the
	 * one we will want to be using.
	 */
	if(l_rsm->r_ack_arrival	 < r_rsm->r_ack_arrival)
		l_rsm->r_ack_arrival = r_rsm->r_ack_arrival;

	if ((r_rsm->r_limit_type == 0) && (l_rsm->r_limit_type != 0)) {
		/* Transfer the split limit to the map we free */
		r_rsm->r_limit_type = l_rsm->r_limit_type;
		l_rsm->r_limit_type = 0;
	}
	rack_free(rack, r_rsm);
	l_rsm->r_flags |= RACK_MERGED;
	return (l_rsm);
}

/*
 * TLP Timer, here we simply setup what segment we want to
 * have the TLP expire on, the normal rack_output() will then
 * send it out.
 *
 * We return 1, saying don't proceed with rack_output only
 * when all timers have been stopped (destroyed PCB?).
 */
static int
rack_timeout_tlp(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts, uint8_t *doing_tlp)
{
	/*
	 * Tail Loss Probe.
	 */
	struct rack_sendmap *rsm = NULL;
	int insret __diagused;
	struct socket *so = tptosocket(tp);
	uint32_t amm;
	uint32_t out, avail;
	int collapsed_win = 0;

	if (TSTMP_LT(cts, rack->r_ctl.rc_timer_exp)) {
		/* Its not time yet */
		return (0);
	}
	if (ctf_progress_timeout_check(tp, true)) {
		rack_log_progress_event(rack, tp, tick, PROGRESS_DROP, __LINE__);
		return (-ETIMEDOUT);	/* tcp_drop() */
	}
	/*
	 * A TLP timer has expired. We have been idle for 2 rtts. So we now
	 * need to figure out how to force a full MSS segment out.
	 */
	rack_log_to_event(rack, RACK_TO_FRM_TLP, NULL);
	rack->r_ctl.retran_during_recovery = 0;
	rack->r_might_revert = 0;
	rack->r_ctl.dsack_byte_cnt = 0;
	counter_u64_add(rack_tlp_tot, 1);
	if (rack->r_state && (rack->r_state != tp->t_state))
		rack_set_state(tp, rack);
	avail = sbavail(&so->so_snd);
	out = tp->snd_max - tp->snd_una;
	if ((out > tp->snd_wnd) || rack->rc_has_collapsed) {
		/* special case, we need a retransmission */
		collapsed_win = 1;
		goto need_retran;
	}
	if (rack->r_ctl.dsack_persist && (rack->r_ctl.rc_tlp_cnt_out >= 1)) {
		rack->r_ctl.dsack_persist--;
		if (rack->r_ctl.num_dsack && (rack->r_ctl.dsack_persist == 0)) {
			rack->r_ctl.num_dsack = 0;
		}
		rack_log_dsack_event(rack, 1, __LINE__, 0, 0);
	}
	if ((tp->t_flags & TF_GPUTINPROG) &&
	    (rack->r_ctl.rc_tlp_cnt_out == 1)) {
		/*
		 * If this is the second in a row
		 * TLP and we are doing a measurement
		 * its time to abandon the measurement.
		 * Something is likely broken on
		 * the clients network and measuring a
		 * broken network does us no good.
		 */
		tp->t_flags &= ~TF_GPUTINPROG;
		rack_log_pacing_delay_calc(rack, (tp->gput_ack - tp->gput_seq) /*flex2*/,
					   rack->r_ctl.rc_gp_srtt /*flex1*/,
					   tp->gput_seq,
					   0, 0, 18, __LINE__, NULL, 0);
	}
	/*
	 * Check our send oldest always settings, and if
	 * there is an oldest to send jump to the need_retran.
	 */
	if (rack_always_send_oldest && (TAILQ_EMPTY(&rack->r_ctl.rc_tmap) == 0))
		goto need_retran;

	if (avail > out) {
		/* New data is available */
		amm = avail - out;
		if (amm > ctf_fixed_maxseg(tp)) {
			amm = ctf_fixed_maxseg(tp);
			if ((amm + out) > tp->snd_wnd) {
				/* We are rwnd limited */
				goto need_retran;
			}
		} else if (amm < ctf_fixed_maxseg(tp)) {
			/* not enough to fill a MTU */
			goto need_retran;
		}
		if (IN_FASTRECOVERY(tp->t_flags)) {
			/* Unlikely */
			if (rack->rack_no_prr == 0) {
				if (out + amm <= tp->snd_wnd) {
					rack->r_ctl.rc_prr_sndcnt = amm;
					rack->r_ctl.rc_tlp_new_data = amm;
					rack_log_to_prr(rack, 4, 0, __LINE__);
				}
			} else
				goto need_retran;
		} else {
			/* Set the send-new override */
			if (out + amm <= tp->snd_wnd)
				rack->r_ctl.rc_tlp_new_data = amm;
			else
				goto need_retran;
		}
		rack->r_ctl.rc_tlpsend = NULL;
		counter_u64_add(rack_tlp_newdata, 1);
		goto send;
	}
need_retran:
	/*
	 * Ok we need to arrange the last un-acked segment to be re-sent, or
	 * optionally the first un-acked segment.
	 */
	if (collapsed_win == 0) {
		if (rack_always_send_oldest)
			rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
		else {
			rsm = tqhash_max(rack->r_ctl.tqh);
			if (rsm && (rsm->r_flags & (RACK_ACKED | RACK_HAS_FIN))) {
				rsm = rack_find_high_nonack(rack, rsm);
			}
		}
		if (rsm == NULL) {
#ifdef TCP_BLACKBOX
			tcp_log_dump_tp_logbuf(tp, "nada counter trips", M_NOWAIT, true);
#endif
			goto out;
		}
	} else {
		/*
		 * We had a collapsed window, lets find
		 * the point before the collapse.
		 */
		if (SEQ_GT((rack->r_ctl.last_collapse_point - 1), rack->rc_tp->snd_una))
			rsm = tqhash_find(rack->r_ctl.tqh, (rack->r_ctl.last_collapse_point - 1));
		else {
			rsm = tqhash_min(rack->r_ctl.tqh);
		}
		if (rsm == NULL) {
			/* Huh */
			goto out;
		}
	}
	if ((rsm->r_end - rsm->r_start) > ctf_fixed_maxseg(tp)) {
		/*
		 * We need to split this the last segment in two.
		 */
		struct rack_sendmap *nrsm;

		nrsm = rack_alloc_full_limit(rack);
		if (nrsm == NULL) {
			/*
			 * No memory to split, we will just exit and punt
			 * off to the RXT timer.
			 */
			goto out;
		}
		rack_clone_rsm(rack, nrsm, rsm,
			       (rsm->r_end - ctf_fixed_maxseg(tp)));
		rack_log_map_chg(tp, rack, NULL, rsm, nrsm, MAP_SPLIT, 0, __LINE__);
#ifndef INVARIANTS
		(void)tqhash_insert(rack->r_ctl.tqh, nrsm);
#else
		if ((insret = tqhash_insert(rack->r_ctl.tqh, nrsm)) != 0) {
			panic("Insert in tailq_hash of %p fails ret:%d rack:%p rsm:%p",
			      nrsm, insret, rack, rsm);
		}
#endif
		if (rsm->r_in_tmap) {
			TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, rsm, nrsm, r_tnext);
			nrsm->r_in_tmap = 1;
		}
		rsm = nrsm;
	}
	rack->r_ctl.rc_tlpsend = rsm;
send:
	/* Make sure output path knows we are doing a TLP */
	*doing_tlp = 1;
	rack->r_timer_override = 1;
	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_TLP;
	return (0);
out:
	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_TLP;
	return (0);
}

/*
 * Delayed ack Timer, here we simply need to setup the
 * ACK_NOW flag and remove the DELACK flag. From there
 * the output routine will send the ack out.
 *
 * We only return 1, saying don't proceed, if all timers
 * are stopped (destroyed PCB?).
 */
static int
rack_timeout_delack(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts)
{

	rack_log_to_event(rack, RACK_TO_FRM_DELACK, NULL);
	tp->t_flags &= ~TF_DELACK;
	tp->t_flags |= TF_ACKNOW;
	KMOD_TCPSTAT_INC(tcps_delack);
	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_DELACK;
	return (0);
}

static inline int
rack_send_ack_challange(struct tcp_rack *rack)
{
	struct tcptemp *t_template;

	t_template = tcpip_maketemplate(rack->rc_inp);
	if (t_template) {
		if (rack->forced_ack == 0) {
			rack->forced_ack = 1;
			rack->r_ctl.forced_ack_ts = tcp_get_usecs(NULL);
		} else {
			rack->probe_not_answered = 1;
		}
		tcp_respond(rack->rc_tp, t_template->tt_ipgen,
			    &t_template->tt_t, (struct mbuf *)NULL,
			    rack->rc_tp->rcv_nxt, rack->rc_tp->snd_una - 1, 0);
		free(t_template, M_TEMP);
		/* This does send an ack so kill any D-ack timer */
		if (rack->rc_tp->t_flags & TF_DELACK)
			rack->rc_tp->t_flags &= ~TF_DELACK;
		return(1);
	} else
		return (0);

}

/*
 * Persists timer, here we simply send the
 * same thing as a keepalive will.
 * the one byte send.
 *
 * We only return 1, saying don't proceed, if all timers
 * are stopped (destroyed PCB?).
 */
static int
rack_timeout_persist(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts)
{
	int32_t retval = 1;

	if (rack->rc_in_persist == 0)
		return (0);
	if (ctf_progress_timeout_check(tp, false)) {
		tcp_log_end_status(tp, TCP_EI_STATUS_PERSIST_MAX);
		rack_log_progress_event(rack, tp, tick, PROGRESS_DROP, __LINE__);
		counter_u64_add(rack_persists_lost_ends, rack->r_ctl.persist_lost_ends);
		return (-ETIMEDOUT);	/* tcp_drop() */
	}
	/*
	 * Persistence timer into zero window. Force a byte to be output, if
	 * possible.
	 */
	KMOD_TCPSTAT_INC(tcps_persisttimeo);
	/*
	 * Hack: if the peer is dead/unreachable, we do not time out if the
	 * window is closed.  After a full backoff, drop the connection if
	 * the idle time (no responses to probes) reaches the maximum
	 * backoff that we would use if retransmitting.
	 */
	if (tp->t_rxtshift >= V_tcp_retries &&
	    (ticks - tp->t_rcvtime >= tcp_maxpersistidle ||
	     TICKS_2_USEC(ticks - tp->t_rcvtime) >= RACK_REXMTVAL(tp) * tcp_totbackoff)) {
		KMOD_TCPSTAT_INC(tcps_persistdrop);
		tcp_log_end_status(tp, TCP_EI_STATUS_PERSIST_MAX);
		counter_u64_add(rack_persists_lost_ends, rack->r_ctl.persist_lost_ends);
		retval = -ETIMEDOUT;	/* tcp_drop() */
		goto out;
	}
	if ((sbavail(&rack->rc_inp->inp_socket->so_snd) == 0) &&
	    tp->snd_una == tp->snd_max)
		rack_exit_persist(tp, rack, cts);
	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_PERSIT;
	/*
	 * If the user has closed the socket then drop a persisting
	 * connection after a much reduced timeout.
	 */
	if (tp->t_state > TCPS_CLOSE_WAIT &&
	    (ticks - tp->t_rcvtime) >= TCPTV_PERSMAX) {
		KMOD_TCPSTAT_INC(tcps_persistdrop);
		tcp_log_end_status(tp, TCP_EI_STATUS_PERSIST_MAX);
		counter_u64_add(rack_persists_lost_ends, rack->r_ctl.persist_lost_ends);
		retval = -ETIMEDOUT;	/* tcp_drop() */
		goto out;
	}
	if (rack_send_ack_challange(rack)) {
		/* only set it if we were answered */
		if (rack->probe_not_answered) {
			counter_u64_add(rack_persists_loss, 1);
			rack->r_ctl.persist_lost_ends++;
		}
		counter_u64_add(rack_persists_sends, 1);
		counter_u64_add(rack_out_size[TCP_MSS_ACCT_PERSIST], 1);
	}
	if (tp->t_rxtshift < V_tcp_retries)
		tp->t_rxtshift++;
out:
	rack_log_to_event(rack, RACK_TO_FRM_PERSIST, NULL);
	rack_start_hpts_timer(rack, tp, cts,
			      0, 0, 0);
	return (retval);
}

/*
 * If a keepalive goes off, we had no other timers
 * happening. We always return 1 here since this
 * routine either drops the connection or sends
 * out a segment with respond.
 */
static int
rack_timeout_keepalive(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts)
{
	struct inpcb *inp = tptoinpcb(tp);

	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_KEEP;
	rack_log_to_event(rack, RACK_TO_FRM_KEEP, NULL);
	/*
	 * Keep-alive timer went off; send something or drop connection if
	 * idle for too long.
	 */
	KMOD_TCPSTAT_INC(tcps_keeptimeo);
	if (tp->t_state < TCPS_ESTABLISHED)
		goto dropit;
	if ((V_tcp_always_keepalive || inp->inp_socket->so_options & SO_KEEPALIVE) &&
	    tp->t_state <= TCPS_CLOSING) {
		if (ticks - tp->t_rcvtime >= TP_KEEPIDLE(tp) + TP_MAXIDLE(tp))
			goto dropit;
		/*
		 * Send a packet designed to force a response if the peer is
		 * up and reachable: either an ACK if the connection is
		 * still alive, or an RST if the peer has closed the
		 * connection due to timeout or reboot. Using sequence
		 * number tp->snd_una-1 causes the transmitted zero-length
		 * segment to lie outside the receive window; by the
		 * protocol spec, this requires the correspondent TCP to
		 * respond.
		 */
		KMOD_TCPSTAT_INC(tcps_keepprobe);
		rack_send_ack_challange(rack);
	}
	rack_start_hpts_timer(rack, tp, cts, 0, 0, 0);
	return (1);
dropit:
	KMOD_TCPSTAT_INC(tcps_keepdrops);
	tcp_log_end_status(tp, TCP_EI_STATUS_KEEP_MAX);
	return (-ETIMEDOUT);	/* tcp_drop() */
}

/*
 * Retransmit helper function, clear up all the ack
 * flags and take care of important book keeping.
 */
static void
rack_remxt_tmr(struct tcpcb *tp)
{
	/*
	 * The retransmit timer went off, all sack'd blocks must be
	 * un-acked.
	 */
	struct rack_sendmap *rsm, *trsm = NULL;
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	rack_timer_cancel(tp, rack, tcp_get_usecs(NULL), __LINE__);
	rack_log_to_event(rack, RACK_TO_FRM_TMR, NULL);
	rack->r_timer_override = 1;
	rack->r_ctl.rc_snd_max_at_rto = tp->snd_max;
	rack->r_ctl.rc_last_timeout_snduna = tp->snd_una;
	rack->r_late = 0;
	rack->r_early = 0;
	rack->r_ctl.rc_agg_delayed = 0;
	rack->r_ctl.rc_agg_early = 0;
	if (rack->r_state && (rack->r_state != tp->t_state))
		rack_set_state(tp, rack);
	if (tp->t_rxtshift <= rack_rxt_scoreboard_clear_thresh) {
		/*
		 * We do not clear the scoreboard until we have had
		 * more than rack_rxt_scoreboard_clear_thresh time-outs.
		 */
		rack->r_ctl.rc_resend = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
		if (rack->r_ctl.rc_resend != NULL)
			rack->r_ctl.rc_resend->r_flags |= RACK_TO_REXT;

		return;
	}
	/*
	 * Ideally we would like to be able to
	 * mark SACK-PASS on anything not acked here.
	 *
	 * However, if we do that we would burst out
	 * all that data 1ms apart. This would be unwise,
	 * so for now we will just let the normal rxt timer
	 * and tlp timer take care of it.
	 *
	 * Also we really need to stick them back in sequence
	 * order. This way we send in the proper order and any
	 * sacks that come floating in will "re-ack" the data.
	 * To do this we zap the tmap with an INIT and then
	 * walk through and place every rsm in the tail queue
	 * hash table back in its seq ordered place.
	 */
	TAILQ_INIT(&rack->r_ctl.rc_tmap);

	TQHASH_FOREACH(rsm, rack->r_ctl.tqh)  {
		rsm->r_dupack = 0;
		if (rack_verbose_logging)
			rack_log_retran_reason(rack, rsm, __LINE__, 0, 2);
		/* We must re-add it back to the tlist */
		if (trsm == NULL) {
			TAILQ_INSERT_HEAD(&rack->r_ctl.rc_tmap, rsm, r_tnext);
		} else {
			TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, trsm, rsm, r_tnext);
		}
		rsm->r_in_tmap = 1;
		trsm = rsm;
		if (rsm->r_flags & RACK_ACKED)
			rsm->r_flags |= RACK_WAS_ACKED;
		rsm->r_flags &= ~(RACK_ACKED | RACK_SACK_PASSED | RACK_WAS_SACKPASS | RACK_RWND_COLLAPSED | RACK_WAS_LOST);
		rsm->r_flags |= RACK_MUST_RXT;
	}
	/* zero the lost since it's all gone */
	rack->r_ctl.rc_considered_lost = 0;
	/* Clear the count (we just un-acked them) */
	rack->r_ctl.rc_sacked = 0;
	rack->r_ctl.rc_sacklast = NULL;
	/* Clear the tlp rtx mark */
	rack->r_ctl.rc_resend = tqhash_min(rack->r_ctl.tqh);
	if (rack->r_ctl.rc_resend != NULL)
		rack->r_ctl.rc_resend->r_flags |= RACK_TO_REXT;
	rack->r_ctl.rc_prr_sndcnt = 0;
	rack_log_to_prr(rack, 6, 0, __LINE__);
	rack->r_ctl.rc_resend = tqhash_min(rack->r_ctl.tqh);
	if (rack->r_ctl.rc_resend != NULL)
		rack->r_ctl.rc_resend->r_flags |= RACK_TO_REXT;
	if ((((tp->t_flags & TF_SACK_PERMIT) == 0)
#ifdef TCP_SAD_DETECTION
	     || (rack->sack_attack_disable != 0)
#endif
		    ) && ((tp->t_flags & TF_SENTFIN) == 0)) {
		/*
		 * For non-sack customers new data
		 * needs to go out as retransmits until
		 * we retransmit up to snd_max.
		 */
		rack->r_must_retran = 1;
		rack->r_ctl.rc_out_at_rto = ctf_flight_size(rack->rc_tp,
							    rack->r_ctl.rc_sacked);
	}
}

static void
rack_convert_rtts(struct tcpcb *tp)
{
	tcp_change_time_units(tp, TCP_TMR_GRANULARITY_USEC);
	tp->t_rxtcur = RACK_REXMTVAL(tp);
	if (TCPS_HAVEESTABLISHED(tp->t_state)) {
		tp->t_rxtcur += TICKS_2_USEC(tcp_rexmit_slop);
	}
	if (tp->t_rxtcur > rack_rto_max) {
		tp->t_rxtcur = rack_rto_max;
	}
}

static void
rack_cc_conn_init(struct tcpcb *tp)
{
	struct tcp_rack *rack;
	uint32_t srtt;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	srtt = tp->t_srtt;
	cc_conn_init(tp);
	/*
	 * Now convert to rack's internal format,
	 * if required.
	 */
	if ((srtt == 0) && (tp->t_srtt != 0))
		rack_convert_rtts(tp);
	/*
	 * We want a chance to stay in slowstart as
	 * we create a connection. TCP spec says that
	 * initially ssthresh is infinite. For our
	 * purposes that is the snd_wnd.
	 */
	if (tp->snd_ssthresh < tp->snd_wnd) {
		tp->snd_ssthresh = tp->snd_wnd;
	}
	/*
	 * We also want to assure a IW worth of
	 * data can get inflight.
	 */
	if (rc_init_window(rack) < tp->snd_cwnd)
		tp->snd_cwnd = rc_init_window(rack);
}

/*
 * Re-transmit timeout! If we drop the PCB we will return 1, otherwise
 * we will setup to retransmit the lowest seq number outstanding.
 */
static int
rack_timeout_rxt(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts)
{
	struct inpcb *inp = tptoinpcb(tp);
	int32_t rexmt;
	int32_t retval = 0;
	bool isipv6;

	if ((tp->t_flags & TF_GPUTINPROG) &&
	    (tp->t_rxtshift)) {
		/*
		 * We have had a second timeout
		 * measurements on successive rxt's are not profitable.
		 * It is unlikely to be of any use (the network is
		 * broken or the client went away).
		 */
		tp->t_flags &= ~TF_GPUTINPROG;
		rack_log_pacing_delay_calc(rack, (tp->gput_ack - tp->gput_seq) /*flex2*/,
					   rack->r_ctl.rc_gp_srtt /*flex1*/,
					   tp->gput_seq,
					   0, 0, 18, __LINE__, NULL, 0);
	}
	if (ctf_progress_timeout_check(tp, false)) {
		tcp_log_end_status(tp, TCP_EI_STATUS_RETRAN);
		rack_log_progress_event(rack, tp, tick, PROGRESS_DROP, __LINE__);
		return (-ETIMEDOUT);	/* tcp_drop() */
	}
	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_RXT;
	rack->r_ctl.retran_during_recovery = 0;
	rack->rc_ack_required = 1;
	rack->r_ctl.dsack_byte_cnt = 0;
	if (IN_RECOVERY(tp->t_flags) &&
	    (rack->rto_from_rec == 0)) {
		/*
		 * Mark that we had a rto while in recovery
		 * and save the ssthresh so if we go back
		 * into recovery we will have a chance
		 * to slowstart back to the level.
		 */
		rack->rto_from_rec = 1;
		rack->r_ctl.rto_ssthresh = tp->snd_ssthresh;
	}
	if (IN_FASTRECOVERY(tp->t_flags))
		tp->t_flags |= TF_WASFRECOVERY;
	else
		tp->t_flags &= ~TF_WASFRECOVERY;
	if (IN_CONGRECOVERY(tp->t_flags))
		tp->t_flags |= TF_WASCRECOVERY;
	else
		tp->t_flags &= ~TF_WASCRECOVERY;
	if (TCPS_HAVEESTABLISHED(tp->t_state) &&
	    (tp->snd_una == tp->snd_max)) {
		/* Nothing outstanding .. nothing to do */
		return (0);
	}
	if (rack->r_ctl.dsack_persist) {
		rack->r_ctl.dsack_persist--;
		if (rack->r_ctl.num_dsack && (rack->r_ctl.dsack_persist == 0)) {
			rack->r_ctl.num_dsack = 0;
		}
		rack_log_dsack_event(rack, 1, __LINE__, 0, 0);
	}
	/*
	 * Rack can only run one timer  at a time, so we cannot
	 * run a KEEPINIT (gating SYN sending) and a retransmit
	 * timer for the SYN. So if we are in a front state and
	 * have a KEEPINIT timer we need to check the first transmit
	 * against now to see if we have exceeded the KEEPINIT time
	 * (if one is set).
	 */
	if ((TCPS_HAVEESTABLISHED(tp->t_state) == 0) &&
	    (TP_KEEPINIT(tp) != 0)) {
		struct rack_sendmap *rsm;

		rsm = tqhash_min(rack->r_ctl.tqh);
		if (rsm) {
			/* Ok we have something outstanding to test keepinit with */
			if ((TSTMP_GT(cts, (uint32_t)rsm->r_tim_lastsent[0])) &&
			    ((cts - (uint32_t)rsm->r_tim_lastsent[0]) >= TICKS_2_USEC(TP_KEEPINIT(tp)))) {
				/* We have exceeded the KEEPINIT time */
				tcp_log_end_status(tp, TCP_EI_STATUS_KEEP_MAX);
				goto drop_it;
			}
		}
	}
	/*
	 * Retransmission timer went off.  Message has not been acked within
	 * retransmit interval.  Back off to a longer retransmit interval
	 * and retransmit one segment.
	 */
	if ((rack->r_ctl.rc_resend == NULL) ||
	    ((rack->r_ctl.rc_resend->r_flags & RACK_RWND_COLLAPSED) == 0)) {
		/*
		 * If the rwnd collapsed on
		 * the one we are retransmitting
		 * it does not count against the
		 * rxt count.
		 */
		tp->t_rxtshift++;
	}
	rack_remxt_tmr(tp);
	if (tp->t_rxtshift > V_tcp_retries) {
		tcp_log_end_status(tp, TCP_EI_STATUS_RETRAN);
drop_it:
		tp->t_rxtshift = V_tcp_retries;
		KMOD_TCPSTAT_INC(tcps_timeoutdrop);
		/* XXXGL: previously t_softerror was casted to uint16_t */
		MPASS(tp->t_softerror >= 0);
		retval = tp->t_softerror ? -tp->t_softerror : -ETIMEDOUT;
		goto out;	/* tcp_drop() */
	}
	if (tp->t_state == TCPS_SYN_SENT) {
		/*
		 * If the SYN was retransmitted, indicate CWND to be limited
		 * to 1 segment in cc_conn_init().
		 */
		tp->snd_cwnd = 1;
	} else if (tp->t_rxtshift == 1) {
		/*
		 * first retransmit; record ssthresh and cwnd so they can be
		 * recovered if this turns out to be a "bad" retransmit. A
		 * retransmit is considered "bad" if an ACK for this segment
		 * is received within RTT/2 interval; the assumption here is
		 * that the ACK was already in flight.  See "On Estimating
		 * End-to-End Network Path Properties" by Allman and Paxson
		 * for more details.
		 */
		tp->snd_cwnd_prev = tp->snd_cwnd;
		tp->snd_ssthresh_prev = tp->snd_ssthresh;
		tp->snd_recover_prev = tp->snd_recover;
		tp->t_badrxtwin = ticks + (USEC_2_TICKS(tp->t_srtt)/2);
		tp->t_flags |= TF_PREVVALID;
	} else if ((tp->t_flags & TF_RCVD_TSTMP) == 0)
		tp->t_flags &= ~TF_PREVVALID;
	KMOD_TCPSTAT_INC(tcps_rexmttimeo);
	if ((tp->t_state == TCPS_SYN_SENT) ||
	    (tp->t_state == TCPS_SYN_RECEIVED))
		rexmt = RACK_INITIAL_RTO * tcp_backoff[tp->t_rxtshift];
	else
		rexmt = max(rack_rto_min, (tp->t_srtt + (tp->t_rttvar << 2))) * tcp_backoff[tp->t_rxtshift];

	RACK_TCPT_RANGESET(tp->t_rxtcur, rexmt,
	   max(rack_rto_min, rexmt), rack_rto_max, rack->r_ctl.timer_slop);
	/*
	 * We enter the path for PLMTUD if connection is established or, if
	 * connection is FIN_WAIT_1 status, reason for the last is that if
	 * amount of data we send is very small, we could send it in couple
	 * of packets and process straight to FIN. In that case we won't
	 * catch ESTABLISHED state.
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
		/*
		 * Idea here is that at each stage of mtu probe (usually,
		 * 1448 -> 1188 -> 524) should be given 2 chances to recover
		 * before further clamping down. 'tp->t_rxtshift % 2 == 0'
		 * should take care of that.
		 */
		if (((tp->t_flags2 & (TF2_PLPMTU_PMTUD | TF2_PLPMTU_MAXSEGSNT)) ==
		    (TF2_PLPMTU_PMTUD | TF2_PLPMTU_MAXSEGSNT)) &&
		    (tp->t_rxtshift >= 2 && tp->t_rxtshift < 6 &&
		    tp->t_rxtshift % 2 == 0)) {
			/*
			 * Enter Path MTU Black-hole Detection mechanism: -
			 * Disable Path MTU Discovery (IP "DF" bit). -
			 * Reduce MTU to lower value than what we negotiated
			 * with peer.
			 */
			if ((tp->t_flags2 & TF2_PLPMTU_BLACKHOLE) == 0) {
				/* Record that we may have found a black hole. */
				tp->t_flags2 |= TF2_PLPMTU_BLACKHOLE;
				/* Keep track of previous MSS. */
				tp->t_pmtud_saved_maxseg = tp->t_maxseg;
			}

			/*
			 * Reduce the MSS to blackhole value or to the
			 * default in an attempt to retransmit.
			 */
#ifdef INET6
			if (isipv6 &&
			    tp->t_maxseg > V_tcp_v6pmtud_blackhole_mss) {
				/* Use the sysctl tuneable blackhole MSS. */
				tp->t_maxseg = V_tcp_v6pmtud_blackhole_mss;
				KMOD_TCPSTAT_INC(tcps_pmtud_blackhole_activated);
			} else if (isipv6) {
				/* Use the default MSS. */
				tp->t_maxseg = V_tcp_v6mssdflt;
				/*
				 * Disable Path MTU Discovery when we switch
				 * to minmss.
				 */
				tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
				KMOD_TCPSTAT_INC(tcps_pmtud_blackhole_activated_min_mss);
			}
#endif
#if defined(INET6) && defined(INET)
			else
#endif
#ifdef INET
			if (tp->t_maxseg > V_tcp_pmtud_blackhole_mss) {
				/* Use the sysctl tuneable blackhole MSS. */
				tp->t_maxseg = V_tcp_pmtud_blackhole_mss;
				KMOD_TCPSTAT_INC(tcps_pmtud_blackhole_activated);
			} else {
				/* Use the default MSS. */
				tp->t_maxseg = V_tcp_mssdflt;
				/*
				 * Disable Path MTU Discovery when we switch
				 * to minmss.
				 */
				tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
				KMOD_TCPSTAT_INC(tcps_pmtud_blackhole_activated_min_mss);
			}
#endif
		} else {
			/*
			 * If further retransmissions are still unsuccessful
			 * with a lowered MTU, maybe this isn't a blackhole
			 * and we restore the previous MSS and blackhole
			 * detection flags. The limit '6' is determined by
			 * giving each probe stage (1448, 1188, 524) 2
			 * chances to recover.
			 */
			if ((tp->t_flags2 & TF2_PLPMTU_BLACKHOLE) &&
			    (tp->t_rxtshift >= 6)) {
				tp->t_flags2 |= TF2_PLPMTU_PMTUD;
				tp->t_flags2 &= ~TF2_PLPMTU_BLACKHOLE;
				tp->t_maxseg = tp->t_pmtud_saved_maxseg;
				KMOD_TCPSTAT_INC(tcps_pmtud_blackhole_failed);
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
	 * If we backed off this far, our srtt estimate is probably bogus.
	 * Clobber it so we'll take the next rtt measurement as our srtt;
	 * move the current srtt into rttvar to keep the current retransmit
	 * times until then.
	 */
	if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV6) != 0)
			in6_losing(inp);
		else
#endif
			in_losing(inp);
		tp->t_rttvar += tp->t_srtt;
		tp->t_srtt = 0;
	}
	sack_filter_clear(&rack->r_ctl.rack_sf, tp->snd_una);
	tp->snd_recover = tp->snd_max;
	tp->t_flags |= TF_ACKNOW;
	tp->t_rtttime = 0;
	rack_cong_signal(tp, CC_RTO, tp->snd_una, __LINE__);
out:
	return (retval);
}

static int
rack_process_timers(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts, uint8_t hpts_calling, uint8_t *doing_tlp)
{
	int32_t ret = 0;
	int32_t timers = (rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK);

	if ((tp->t_state >= TCPS_FIN_WAIT_1) &&
	    (tp->t_flags & TF_GPUTINPROG)) {
		/*
		 * We have a goodput in progress
		 * and we have entered a late state.
		 * Do we have enough data in the sb
		 * to handle the GPUT request?
		 */
		uint32_t bytes;

		bytes = tp->gput_ack - tp->gput_seq;
		if (SEQ_GT(tp->gput_seq, tp->snd_una))
			bytes += tp->gput_seq - tp->snd_una;
		if (bytes > sbavail(&tptosocket(tp)->so_snd)) {
			/*
			 * There are not enough bytes in the socket
			 * buffer that have been sent to cover this
			 * measurement. Cancel it.
			 */
			rack_log_pacing_delay_calc(rack, (tp->gput_ack - tp->gput_seq) /*flex2*/,
						   rack->r_ctl.rc_gp_srtt /*flex1*/,
						   tp->gput_seq,
						   0, 0, 18, __LINE__, NULL, 0);
			tp->t_flags &= ~TF_GPUTINPROG;
		}
	}
	if (timers == 0) {
		return (0);
	}
	if (tp->t_state == TCPS_LISTEN) {
		/* no timers on listen sockets */
		if (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT)
			return (0);
		return (1);
	}
	if ((timers & PACE_TMR_RACK) &&
	    rack->rc_on_min_to) {
		/*
		 * For the rack timer when we
		 * are on a min-timeout (which means rrr_conf = 3)
		 * we don't want to check the timer. It may
		 * be going off for a pace and thats ok we
		 * want to send the retransmit (if its ready).
		 *
		 * If its on a normal rack timer (non-min) then
		 * we will check if its expired.
		 */
		goto skip_time_check;
	}
	if (TSTMP_LT(cts, rack->r_ctl.rc_timer_exp)) {
		uint32_t left;

		if (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) {
			ret = -1;
			rack_log_to_processing(rack, cts, ret, 0);
			return (0);
		}
		if (hpts_calling == 0) {
			/*
			 * A user send or queued mbuf (sack) has called us? We
			 * return 0 and let the pacing guards
			 * deal with it if they should or
			 * should not cause a send.
			 */
			ret = -2;
			rack_log_to_processing(rack, cts, ret, 0);
			return (0);
		}
		/*
		 * Ok our timer went off early and we are not paced false
		 * alarm, go back to sleep. We make sure we don't have
		 * no-sack wakeup on since we no longer have a PKT_OUTPUT
		 * flag in place.
		 */
		rack->rc_tp->t_flags2 &= ~TF2_DONT_SACK_QUEUE;
		ret = -3;
		left = rack->r_ctl.rc_timer_exp - cts;
		tcp_hpts_insert(tp, HPTS_MS_TO_SLOTS(left));
		rack_log_to_processing(rack, cts, ret, left);
		return (1);
	}
skip_time_check:
	rack->rc_tmr_stopped = 0;
	rack->r_ctl.rc_hpts_flags &= ~PACE_TMR_MASK;
	if (timers & PACE_TMR_DELACK) {
		ret = rack_timeout_delack(tp, rack, cts);
	} else if (timers & PACE_TMR_RACK) {
		rack->r_ctl.rc_tlp_rxt_last_time = cts;
		rack->r_fast_output = 0;
		ret = rack_timeout_rack(tp, rack, cts);
	} else if (timers & PACE_TMR_TLP) {
		rack->r_ctl.rc_tlp_rxt_last_time = cts;
		ret = rack_timeout_tlp(tp, rack, cts, doing_tlp);
	} else if (timers & PACE_TMR_RXT) {
		rack->r_ctl.rc_tlp_rxt_last_time = cts;
		rack->r_fast_output = 0;
		ret = rack_timeout_rxt(tp, rack, cts);
	} else if (timers & PACE_TMR_PERSIT) {
		ret = rack_timeout_persist(tp, rack, cts);
	} else if (timers & PACE_TMR_KEEP) {
		ret = rack_timeout_keepalive(tp, rack, cts);
	}
	rack_log_to_processing(rack, cts, ret, timers);
	return (ret);
}

static void
rack_timer_cancel(struct tcpcb *tp, struct tcp_rack *rack, uint32_t cts, int line)
{
	struct timeval tv;
	uint32_t us_cts, flags_on_entry;
	uint8_t hpts_removed = 0;

	flags_on_entry = rack->r_ctl.rc_hpts_flags;
	us_cts = tcp_get_usecs(&tv);
	if ((rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) &&
	    ((TSTMP_GEQ(us_cts, rack->r_ctl.rc_last_output_to)) ||
	     ((tp->snd_max - tp->snd_una) == 0))) {
		tcp_hpts_remove(rack->rc_tp);
		hpts_removed = 1;
		/* If we were not delayed cancel out the flag. */
		if ((tp->snd_max - tp->snd_una) == 0)
			rack->r_ctl.rc_hpts_flags &= ~PACE_PKT_OUTPUT;
		rack_log_to_cancel(rack, hpts_removed, line, us_cts, &tv, flags_on_entry);
	}
	if (rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK) {
		rack->rc_tmr_stopped = rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK;
		if (tcp_in_hpts(rack->rc_tp) &&
		    ((rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) == 0)) {
			/*
			 * Canceling timer's when we have no output being
			 * paced. We also must remove ourselves from the
			 * hpts.
			 */
			tcp_hpts_remove(rack->rc_tp);
			hpts_removed = 1;
		}
		rack->r_ctl.rc_hpts_flags &= ~(PACE_TMR_MASK);
	}
	if (hpts_removed == 0)
		rack_log_to_cancel(rack, hpts_removed, line, us_cts, &tv, flags_on_entry);
}

static int
rack_stopall(struct tcpcb *tp)
{
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	rack->t_timers_stopped = 1;

	tcp_hpts_remove(tp);

	return (0);
}

static void
rack_stop_all_timers(struct tcpcb *tp, struct tcp_rack *rack)
{
	/*
	 * Assure no timers are running.
	 */
	if (tcp_timer_active(tp, TT_PERSIST)) {
		/* We enter in persists, set the flag appropriately */
		rack->rc_in_persist = 1;
	}
	if (tcp_in_hpts(rack->rc_tp)) {
		tcp_hpts_remove(rack->rc_tp);
	}
}

/*
 * We maintain an array fo 16 (RETRAN_CNT_SIZE) entries. This
 * array is zeroed at the start of recovery. Each time a segment
 * is retransmitted, we translate that into a number of packets
 * (based on segsiz) and based on how many times its been retransmitted
 * increment by the number of packets the counter that represents
 * retansmitted N times. Index 0 is retransmitted 1 time, index 1
 * is retransmitted 2 times etc.
 *
 * So for example when we send a 4344 byte transmission with a 1448
 * byte segsize, and its the third time we have retransmitted this
 * segment, we would add to the rc_cnt_of_retran[2] the value of
 * 3. That represents 3 MSS were retransmitted 3 times (index is
 * the number of times retranmitted minus 1).
 */
static void
rack_peg_rxt(struct tcp_rack *rack, struct rack_sendmap *rsm, uint32_t segsiz)
{
	int idx;
	uint32_t peg;

	peg = ((rsm->r_end - rsm->r_start) + segsiz) - 1;
	peg /= segsiz;
	idx = rsm->r_act_rxt_cnt - 1;
	if (idx >= RETRAN_CNT_SIZE)
		idx = RETRAN_CNT_SIZE - 1;
	/* Max of a uint16_t retransmits in a bucket */
	if ((rack->r_ctl.rc_cnt_of_retran[idx] + peg) < 0xffff)
		rack->r_ctl.rc_cnt_of_retran[idx] += peg;
	else
		rack->r_ctl.rc_cnt_of_retran[idx] = 0xffff;
}

/*
 * We maintain an array fo 16 (RETRAN_CNT_SIZE) entries. This
 * array is zeroed at the start of recovery. Each time a segment
 * is retransmitted, we translate that into a number of packets
 * (based on segsiz) and based on how many times its been retransmitted
 * increment by the number of packets the counter that represents
 * retansmitted N times. Index 0 is retransmitted 1 time, index 1
 * is retransmitted 2 times etc.
 *
 * The rack_unpeg_rxt is used when we go to retransmit a segment
 * again. Basically if the segment had previously been retransmitted
 * say 3 times (as our previous example illustrated in the comment
 * above rack_peg_rxt() prior to calling that and incrementing
 * r_ack_rxt_cnt we would have called rack_unpeg_rxt() that would
 * subtract back the previous add from its last rxt (in this
 * example r_act_cnt would have been 2 for 2 retransmissions. So
 * we would have subtracted 3 from rc_cnt_of_reetran[1] to remove
 * those 3 segments. You will see this in the rack_update_rsm()
 * below where we do:
 *	if (rsm->r_act_rxt_cnt > 0) {
 *		rack_unpeg_rxt(rack, rsm, segsiz);
 *	}
 *	rsm->r_act_rxt_cnt++;
 *	rack_peg_rxt(rack, rsm, segsiz);
 *
 * This effectively moves the count from rc_cnt_of_retran[1] to
 * rc_cnt_of_retran[2].
 */
static void
rack_unpeg_rxt(struct tcp_rack *rack, struct rack_sendmap *rsm, uint32_t segsiz)
{
	int idx;
	uint32_t peg;

	idx = rsm->r_act_rxt_cnt - 1;
	if (idx >= RETRAN_CNT_SIZE)
		idx = RETRAN_CNT_SIZE - 1;
	peg = ((rsm->r_end - rsm->r_start) + segsiz) - 1;
	peg /= segsiz;
	if (peg < rack->r_ctl.rc_cnt_of_retran[idx])
		rack->r_ctl.rc_cnt_of_retran[idx] -= peg;
	else {
		/* TSNH */
		rack->r_ctl.rc_cnt_of_retran[idx] = 0;
	}
}

static void
rack_update_rsm(struct tcpcb *tp, struct tcp_rack *rack,
    struct rack_sendmap *rsm, uint64_t ts, uint32_t add_flag, int segsiz)
{
	int32_t idx;

	rsm->r_rtr_cnt++;
	if (rsm->r_rtr_cnt > RACK_NUM_OF_RETRANS) {
		rsm->r_rtr_cnt = RACK_NUM_OF_RETRANS;
		rsm->r_flags |= RACK_OVERMAX;
	}
	if (rsm->r_act_rxt_cnt > 0) {
		/* Drop the count back for this, its retransmitting again */
		rack_unpeg_rxt(rack, rsm, segsiz);
	}
	rsm->r_act_rxt_cnt++;
	/* Peg the count/index */
	rack_peg_rxt(rack, rsm, segsiz);
	rack_log_retran_reason(rack, rsm, __LINE__, 0, 2);
	rsm->r_dupack = 0;
	if ((rsm->r_rtr_cnt > 1) && ((rsm->r_flags & RACK_TLP) == 0)) {
		rack->r_ctl.rc_holes_rxt += (rsm->r_end - rsm->r_start);
		rsm->r_rtr_bytes += (rsm->r_end - rsm->r_start);
	}
	if (rsm->r_flags & RACK_WAS_LOST) {
		/*
		 * We retransmitted it putting it back in flight
		 * remove the lost desgination and reduce the
		 * bytes considered lost.
		 */
		rsm->r_flags  &= ~RACK_WAS_LOST;
		KASSERT((rack->r_ctl.rc_considered_lost >= (rsm->r_end - rsm->r_start)),
			("rsm:%p rack:%p rc_considered_lost goes negative", rsm,  rack));
		if (rack->r_ctl.rc_considered_lost >= (rsm->r_end - rsm->r_start))
			rack->r_ctl.rc_considered_lost -= rsm->r_end - rsm->r_start;
		else
			rack->r_ctl.rc_considered_lost = 0;
	}
	idx = rsm->r_rtr_cnt - 1;
	rsm->r_tim_lastsent[idx] = ts;
	/*
	 * Here we don't add in the len of send, since its already
	 * in snduna <->snd_max.
	 */
	rsm->r_fas = ctf_flight_size(rack->rc_tp,
				     rack->r_ctl.rc_sacked);
	if (rsm->r_flags & RACK_ACKED) {
		/* Problably MTU discovery messing with us */
		rsm->r_flags &= ~RACK_ACKED;
		rack->r_ctl.rc_sacked -= (rsm->r_end - rsm->r_start);
	}
	if (rsm->r_in_tmap) {
		TAILQ_REMOVE(&rack->r_ctl.rc_tmap, rsm, r_tnext);
		rsm->r_in_tmap = 0;
	}
	/* Lets make sure it really is in or not the GP window */
	rack_mark_in_gp_win(tp, rsm);
	TAILQ_INSERT_TAIL(&rack->r_ctl.rc_tmap, rsm, r_tnext);
	rsm->r_in_tmap = 1;
	rsm->r_bas = (uint8_t)(((rsm->r_end - rsm->r_start) + segsiz - 1) / segsiz);
	/* Take off the must retransmit flag, if its on */
	if (rsm->r_flags & RACK_MUST_RXT) {
		if (rack->r_must_retran)
			rack->r_ctl.rc_out_at_rto -= (rsm->r_end - rsm->r_start);
		if (SEQ_GEQ(rsm->r_end, rack->r_ctl.rc_snd_max_at_rto)) {
			/*
			 * We have retransmitted all we need. Clear
			 * any must retransmit flags.
			 */
			rack->r_must_retran = 0;
			rack->r_ctl.rc_out_at_rto = 0;
		}
		rsm->r_flags &= ~RACK_MUST_RXT;
	}
	/* Remove any collapsed flag */
	rsm->r_flags &= ~RACK_RWND_COLLAPSED;
	if (rsm->r_flags & RACK_SACK_PASSED) {
		/* We have retransmitted due to the SACK pass */
		rsm->r_flags &= ~RACK_SACK_PASSED;
		rsm->r_flags |= RACK_WAS_SACKPASS;
	}
}

static uint32_t
rack_update_entry(struct tcpcb *tp, struct tcp_rack *rack,
    struct rack_sendmap *rsm, uint64_t ts, int32_t *lenp, uint32_t add_flag, int segsiz)
{
	/*
	 * We (re-)transmitted starting at rsm->r_start for some length
	 * (possibly less than r_end.
	 */
	struct rack_sendmap *nrsm;
	int insret __diagused;
	uint32_t c_end;
	int32_t len;

	len = *lenp;
	c_end = rsm->r_start + len;
	if (SEQ_GEQ(c_end, rsm->r_end)) {
		/*
		 * We retransmitted the whole piece or more than the whole
		 * slopping into the next rsm.
		 */
		rack_update_rsm(tp, rack, rsm, ts, add_flag, segsiz);
		if (c_end == rsm->r_end) {
			*lenp = 0;
			return (0);
		} else {
			int32_t act_len;

			/* Hangs over the end return whats left */
			act_len = rsm->r_end - rsm->r_start;
			*lenp = (len - act_len);
			return (rsm->r_end);
		}
		/* We don't get out of this block. */
	}
	/*
	 * Here we retransmitted less than the whole thing which means we
	 * have to split this into what was transmitted and what was not.
	 */
	nrsm = rack_alloc_full_limit(rack);
	if (nrsm == NULL) {
		/*
		 * We can't get memory, so lets not proceed.
		 */
		*lenp = 0;
		return (0);
	}
	/*
	 * So here we are going to take the original rsm and make it what we
	 * retransmitted. nrsm will be the tail portion we did not
	 * retransmit. For example say the chunk was 1, 11 (10 bytes). And
	 * we retransmitted 5 bytes i.e. 1, 5. The original piece shrinks to
	 * 1, 6 and the new piece will be 6, 11.
	 */
	rack_clone_rsm(rack, nrsm, rsm, c_end);
	nrsm->r_dupack = 0;
	rack_log_retran_reason(rack, nrsm, __LINE__, 0, 2);
#ifndef INVARIANTS
	(void)tqhash_insert(rack->r_ctl.tqh, nrsm);
#else
	if ((insret = tqhash_insert(rack->r_ctl.tqh, nrsm)) != 0) {
		panic("Insert in tailq_hash of %p fails ret:%d rack:%p rsm:%p",
		      nrsm, insret, rack, rsm);
	}
#endif
	if (rsm->r_in_tmap) {
		TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, rsm, nrsm, r_tnext);
		nrsm->r_in_tmap = 1;
	}
	rsm->r_flags &= (~RACK_HAS_FIN);
	rack_update_rsm(tp, rack, rsm, ts, add_flag, segsiz);
	/* Log a split of rsm into rsm and nrsm */
	rack_log_map_chg(tp, rack, NULL, rsm, nrsm, MAP_SPLIT, 0, __LINE__);
	*lenp = 0;
	return (0);
}

static void
rack_log_output(struct tcpcb *tp, struct tcpopt *to, int32_t len,
		uint32_t seq_out, uint16_t th_flags, int32_t err, uint64_t cts,
		struct rack_sendmap *hintrsm, uint32_t add_flag, struct mbuf *s_mb,
		uint32_t s_moff, int hw_tls, int segsiz)
{
	struct tcp_rack *rack;
	struct rack_sendmap *rsm, *nrsm;
	int insret __diagused;

	register uint32_t snd_max, snd_una;

	/*
	 * Add to the RACK log of packets in flight or retransmitted. If
	 * there is a TS option we will use the TS echoed, if not we will
	 * grab a TS.
	 *
	 * Retransmissions will increment the count and move the ts to its
	 * proper place. Note that if options do not include TS's then we
	 * won't be able to effectively use the ACK for an RTT on a retran.
	 *
	 * Notes about r_start and r_end. Lets consider a send starting at
	 * sequence 1 for 10 bytes. In such an example the r_start would be
	 * 1 (starting sequence) but the r_end would be r_start+len i.e. 11.
	 * This means that r_end is actually the first sequence for the next
	 * slot (11).
	 *
	 */
	/*
	 * If err is set what do we do XXXrrs? should we not add the thing?
	 * -- i.e. return if err != 0 or should we pretend we sent it? --
	 * i.e. proceed with add ** do this for now.
	 */
	INP_WLOCK_ASSERT(tptoinpcb(tp));
	if (err)
		/*
		 * We don't log errors -- we could but snd_max does not
		 * advance in this case either.
		 */
		return;

	if (th_flags & TH_RST) {
		/*
		 * We don't log resets and we return immediately from
		 * sending
		 */
		return;
	}
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	snd_una = tp->snd_una;
	snd_max = tp->snd_max;
	if (th_flags & (TH_SYN | TH_FIN)) {
		/*
		 * The call to rack_log_output is made before bumping
		 * snd_max. This means we can record one extra byte on a SYN
		 * or FIN if seq_out is adding more on and a FIN is present
		 * (and we are not resending).
		 */
		if ((th_flags & TH_SYN) && (seq_out == tp->iss))
			len++;
		if (th_flags & TH_FIN)
			len++;
	}
	if (SEQ_LEQ((seq_out + len), snd_una)) {
		/* Are sending an old segment to induce an ack (keep-alive)? */
		return;
	}
	if (SEQ_LT(seq_out, snd_una)) {
		/* huh? should we panic? */
		uint32_t end;

		end = seq_out + len;
		seq_out = snd_una;
		if (SEQ_GEQ(end, seq_out))
			len = end - seq_out;
		else
			len = 0;
	}
	if (len == 0) {
		/* We don't log zero window probes */
		return;
	}
	if (IN_FASTRECOVERY(tp->t_flags)) {
		rack->r_ctl.rc_prr_out += len;
	}
	/* First question is it a retransmission or new? */
	if (seq_out == snd_max) {
		/* Its new */
		rack_chk_req_and_hybrid_on_out(rack, seq_out, len, cts);
again:
		rsm = rack_alloc(rack);
		if (rsm == NULL) {
			/*
			 * Hmm out of memory and the tcb got destroyed while
			 * we tried to wait.
			 */
			return;
		}
		if (th_flags & TH_FIN) {
			rsm->r_flags = RACK_HAS_FIN|add_flag;
		} else {
			rsm->r_flags = add_flag;
		}
		if (hw_tls)
			rsm->r_hw_tls = 1;
		rsm->r_tim_lastsent[0] = cts;
		rsm->r_rtr_cnt = 1;
 		rsm->r_act_rxt_cnt = 0;
		rsm->r_rtr_bytes = 0;
		if (th_flags & TH_SYN) {
			/* The data space is one beyond snd_una */
			rsm->r_flags |= RACK_HAS_SYN;
		}
		rsm->r_start = seq_out;
		rsm->r_end = rsm->r_start + len;
		rack_mark_in_gp_win(tp, rsm);
		rsm->r_dupack = 0;
		/*
		 * save off the mbuf location that
		 * sndmbuf_noadv returned (which is
		 * where we started copying from)..
		 */
		rsm->m = s_mb;
		rsm->soff = s_moff;
		/*
		 * Here we do add in the len of send, since its not yet
		 * reflected in in snduna <->snd_max
		 */
		rsm->r_fas = (ctf_flight_size(rack->rc_tp,
					      rack->r_ctl.rc_sacked) +
			      (rsm->r_end - rsm->r_start));
		if ((rack->rc_initial_ss_comp == 0) &&
		    (rack->r_ctl.ss_hi_fs < rsm->r_fas)) {
			   rack->r_ctl.ss_hi_fs = rsm->r_fas;
		}
		/* rsm->m will be NULL if RACK_HAS_SYN or RACK_HAS_FIN is set */
		if (rsm->m) {
			if (rsm->m->m_len <= rsm->soff) {
				/*
				 * XXXrrs Question, will this happen?
				 *
				 * If sbsndptr is set at the correct place
				 * then s_moff should always be somewhere
				 * within rsm->m. But if the sbsndptr was
				 * off then that won't be true. If it occurs
				 * we need to walkout to the correct location.
				 */
				struct mbuf *lm;

				lm = rsm->m;
				while (lm->m_len <= rsm->soff) {
					rsm->soff -= lm->m_len;
					lm = lm->m_next;
					KASSERT(lm != NULL, ("%s rack:%p lm goes null orig_off:%u origmb:%p rsm->soff:%u",
							     __func__, rack, s_moff, s_mb, rsm->soff));
				}
				rsm->m = lm;
			}
			rsm->orig_m_len = rsm->m->m_len;
			rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
		} else {
			rsm->orig_m_len = 0;
			rsm->orig_t_space = 0;
		}
		rsm->r_bas = (uint8_t)((len + segsiz - 1) / segsiz);
		rack_log_retran_reason(rack, rsm, __LINE__, 0, 2);
		/* Log a new rsm */
		rack_log_map_chg(tp, rack, NULL, rsm, NULL, MAP_NEW, 0, __LINE__);
#ifndef INVARIANTS
		(void)tqhash_insert(rack->r_ctl.tqh, rsm);
#else
		if ((insret = tqhash_insert(rack->r_ctl.tqh, rsm)) != 0) {
			panic("Insert in tailq_hash of %p fails ret:%d rack:%p rsm:%p",
			      nrsm, insret, rack, rsm);
		}
#endif
		TAILQ_INSERT_TAIL(&rack->r_ctl.rc_tmap, rsm, r_tnext);
		rsm->r_in_tmap = 1;
		if (rsm->r_flags & RACK_IS_PCM) {
			rack->r_ctl.pcm_i.send_time = cts;
			rack->r_ctl.pcm_i.eseq = rsm->r_end;
			/* First time through we set the start too */
			if (rack->pcm_in_progress == 0)
				rack->r_ctl.pcm_i.sseq = rsm->r_start;
		}
		/*
		 * Special case detection, is there just a single
		 * packet outstanding when we are not in recovery?
		 *
		 * If this is true mark it so.
		 */
		if ((IN_FASTRECOVERY(tp->t_flags) == 0) &&
		    (ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked) == ctf_fixed_maxseg(tp))) {
			struct rack_sendmap *prsm;

			prsm = tqhash_prev(rack->r_ctl.tqh, rsm);
			if (prsm)
				prsm->r_one_out_nr = 1;
		}
		return;
	}
	/*
	 * If we reach here its a retransmission and we need to find it.
	 */
more:
	if (hintrsm && (hintrsm->r_start == seq_out)) {
		rsm = hintrsm;
		hintrsm = NULL;
	} else {
		/* No hints sorry */
		rsm = NULL;
	}
	if ((rsm) && (rsm->r_start == seq_out)) {
		seq_out = rack_update_entry(tp, rack, rsm, cts, &len, add_flag, segsiz);
		if (len == 0) {
			return;
		} else {
			goto more;
		}
	}
	/* Ok it was not the last pointer go through it the hard way. */
refind:
	rsm = tqhash_find(rack->r_ctl.tqh, seq_out);
	if (rsm) {
		if (rsm->r_start == seq_out) {
			seq_out = rack_update_entry(tp, rack, rsm, cts, &len, add_flag, segsiz);
			if (len == 0) {
				return;
			} else {
				goto refind;
			}
		}
		if (SEQ_GEQ(seq_out, rsm->r_start) && SEQ_LT(seq_out, rsm->r_end)) {
			/* Transmitted within this piece */
			/*
			 * Ok we must split off the front and then let the
			 * update do the rest
			 */
			nrsm = rack_alloc_full_limit(rack);
			if (nrsm == NULL) {
				rack_update_rsm(tp, rack, rsm, cts, add_flag, segsiz);
				return;
			}
			/*
			 * copy rsm to nrsm and then trim the front of rsm
			 * to not include this part.
			 */
			rack_clone_rsm(rack, nrsm, rsm, seq_out);
			rack_log_map_chg(tp, rack, NULL, rsm, nrsm, MAP_SPLIT, 0, __LINE__);
#ifndef INVARIANTS
			(void)tqhash_insert(rack->r_ctl.tqh, nrsm);
#else
			if ((insret = tqhash_insert(rack->r_ctl.tqh, nrsm)) != 0) {
				panic("Insert in tailq_hash of %p fails ret:%d rack:%p rsm:%p",
				      nrsm, insret, rack, rsm);
			}
#endif
			if (rsm->r_in_tmap) {
				TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, rsm, nrsm, r_tnext);
				nrsm->r_in_tmap = 1;
			}
			rsm->r_flags &= (~RACK_HAS_FIN);
			seq_out = rack_update_entry(tp, rack, nrsm, cts, &len, add_flag, segsiz);
			if (len == 0) {
				return;
			} else if (len > 0)
				goto refind;
		}
	}
	/*
	 * Hmm not found in map did they retransmit both old and on into the
	 * new?
	 */
	if (seq_out == tp->snd_max) {
		goto again;
	} else if (SEQ_LT(seq_out, tp->snd_max)) {
#ifdef INVARIANTS
		printf("seq_out:%u len:%d snd_una:%u snd_max:%u -- but rsm not found?\n",
		       seq_out, len, tp->snd_una, tp->snd_max);
		printf("Starting Dump of all rack entries\n");
		TQHASH_FOREACH(rsm, rack->r_ctl.tqh)  {
			printf("rsm:%p start:%u end:%u\n",
			       rsm, rsm->r_start, rsm->r_end);
		}
		printf("Dump complete\n");
		panic("seq_out not found rack:%p tp:%p",
		      rack, tp);
#endif
	} else {
#ifdef INVARIANTS
		/*
		 * Hmm beyond sndmax? (only if we are using the new rtt-pack
		 * flag)
		 */
		panic("seq_out:%u(%d) is beyond snd_max:%u tp:%p",
		      seq_out, len, tp->snd_max, tp);
#endif
	}
}

/*
 * Record one of the RTT updates from an ack into
 * our sample structure.
 */

static void
tcp_rack_xmit_timer(struct tcp_rack *rack, int32_t rtt, uint32_t len, uint32_t us_rtt,
		    int confidence, struct rack_sendmap *rsm, uint16_t rtrcnt)
{
	if ((rack->r_ctl.rack_rs.rs_flags & RACK_RTT_EMPTY) ||
	    (rack->r_ctl.rack_rs.rs_rtt_lowest > rtt)) {
		rack->r_ctl.rack_rs.rs_rtt_lowest = rtt;
	}
	if ((rack->r_ctl.rack_rs.rs_flags & RACK_RTT_EMPTY) ||
	    (rack->r_ctl.rack_rs.rs_rtt_highest < rtt)) {
		rack->r_ctl.rack_rs.rs_rtt_highest = rtt;
	}
	if (rack->rc_tp->t_flags & TF_GPUTINPROG) {
	    if (us_rtt < rack->r_ctl.rc_gp_lowrtt)
		rack->r_ctl.rc_gp_lowrtt = us_rtt;
	    if (rack->rc_tp->snd_wnd > rack->r_ctl.rc_gp_high_rwnd)
		    rack->r_ctl.rc_gp_high_rwnd = rack->rc_tp->snd_wnd;
	}
	if ((confidence == 1) &&
	    ((rsm == NULL) ||
	     (rsm->r_just_ret) ||
	     (rsm->r_one_out_nr &&
	      len < (ctf_fixed_maxseg(rack->rc_tp) * 2)))) {
		/*
		 * If the rsm had a just return
		 * hit it then we can't trust the
		 * rtt measurement for buffer deterimination
		 * Note that a confidence of 2, indicates
		 * SACK'd which overrides the r_just_ret or
		 * the r_one_out_nr. If it was a CUM-ACK and
		 * we had only two outstanding, but get an
		 * ack for only 1. Then that also lowers our
		 * confidence.
		 */
		confidence = 0;
	}
	if ((rack->r_ctl.rack_rs.rs_flags & RACK_RTT_EMPTY) ||
	    (rack->r_ctl.rack_rs.rs_us_rtt > us_rtt)) {
		if (rack->r_ctl.rack_rs.confidence == 0) {
			/*
			 * We take anything with no current confidence
			 * saved.
			 */
			rack->r_ctl.rack_rs.rs_us_rtt = us_rtt;
			rack->r_ctl.rack_rs.confidence = confidence;
			rack->r_ctl.rack_rs.rs_us_rtrcnt = rtrcnt;
		} else if (confidence != 0) {
			/*
			 * Once we have a confident number,
			 * we can update it with a smaller
			 * value since this confident number
			 * may include the DSACK time until
			 * the next segment (the second one) arrived.
			 */
			rack->r_ctl.rack_rs.rs_us_rtt = us_rtt;
			rack->r_ctl.rack_rs.confidence = confidence;
			rack->r_ctl.rack_rs.rs_us_rtrcnt = rtrcnt;
		}
	}
	rack_log_rtt_upd(rack->rc_tp, rack, us_rtt, len, rsm, confidence);
	rack->r_ctl.rack_rs.rs_flags = RACK_RTT_VALID;
	rack->r_ctl.rack_rs.rs_rtt_tot += rtt;
	rack->r_ctl.rack_rs.rs_rtt_cnt++;
}

/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
static void
tcp_rack_xmit_timer_commit(struct tcp_rack *rack, struct tcpcb *tp)
{
	int32_t delta;
	int32_t rtt;

	if (rack->r_ctl.rack_rs.rs_flags & RACK_RTT_EMPTY)
		/* No valid sample */
		return;
	if (rack->r_ctl.rc_rate_sample_method == USE_RTT_LOW) {
		/* We are to use the lowest RTT seen in a single ack */
		rtt = rack->r_ctl.rack_rs.rs_rtt_lowest;
	} else if (rack->r_ctl.rc_rate_sample_method == USE_RTT_HIGH) {
		/* We are to use the highest RTT seen in a single ack */
		rtt = rack->r_ctl.rack_rs.rs_rtt_highest;
	} else if (rack->r_ctl.rc_rate_sample_method == USE_RTT_AVG) {
		/* We are to use the average RTT seen in a single ack */
		rtt = (int32_t)(rack->r_ctl.rack_rs.rs_rtt_tot /
				(uint64_t)rack->r_ctl.rack_rs.rs_rtt_cnt);
	} else {
#ifdef INVARIANTS
		panic("Unknown rtt variant %d", rack->r_ctl.rc_rate_sample_method);
#endif
		return;
	}
	if (rtt == 0)
		rtt = 1;
	if (rack->rc_gp_rtt_set == 0) {
		/*
		 * With no RTT we have to accept
		 * even one we are not confident of.
		 */
		rack->r_ctl.rc_gp_srtt = rack->r_ctl.rack_rs.rs_us_rtt;
		rack->rc_gp_rtt_set = 1;
	} else if (rack->r_ctl.rack_rs.confidence) {
		/* update the running gp srtt */
		rack->r_ctl.rc_gp_srtt -= (rack->r_ctl.rc_gp_srtt/8);
		rack->r_ctl.rc_gp_srtt += rack->r_ctl.rack_rs.rs_us_rtt / 8;
	}
	if (rack->r_ctl.rack_rs.confidence) {
		/*
		 * record the low and high for highly buffered path computation,
		 * we only do this if we are confident (not a retransmission).
		 */
		if (rack->r_ctl.rc_highest_us_rtt < rack->r_ctl.rack_rs.rs_us_rtt) {
			rack->r_ctl.rc_highest_us_rtt = rack->r_ctl.rack_rs.rs_us_rtt;
		}
		if (rack->rc_highly_buffered == 0) {
			/*
			 * Currently once we declare a path has
			 * highly buffered there is no going
			 * back, which may be a problem...
			 */
			if ((rack->r_ctl.rc_highest_us_rtt / rack->r_ctl.rc_lowest_us_rtt) > rack_hbp_thresh) {
				rack_log_rtt_shrinks(rack, rack->r_ctl.rack_rs.rs_us_rtt,
						     rack->r_ctl.rc_highest_us_rtt,
						     rack->r_ctl.rc_lowest_us_rtt,
						     RACK_RTTS_SEEHBP);
				rack->rc_highly_buffered = 1;
			}
		}
	}
	if ((rack->r_ctl.rack_rs.confidence) ||
	    (rack->r_ctl.rack_rs.rs_us_rtrcnt == 1)) {
		/*
		 * If we are highly confident of it <or> it was
		 * never retransmitted we accept it as the last us_rtt.
		 */
		rack->r_ctl.rc_last_us_rtt = rack->r_ctl.rack_rs.rs_us_rtt;
		/* The lowest rtt can be set if its was not retransmited */
		if (rack->r_ctl.rc_lowest_us_rtt > rack->r_ctl.rack_rs.rs_us_rtt) {
			rack->r_ctl.rc_lowest_us_rtt = rack->r_ctl.rack_rs.rs_us_rtt;
			if (rack->r_ctl.rc_lowest_us_rtt == 0)
				rack->r_ctl.rc_lowest_us_rtt = 1;
		}
	}
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (tp->t_srtt != 0) {
		/*
		 * We keep a simple srtt in microseconds, like our rtt
		 * measurement. We don't need to do any tricks with shifting
		 * etc. Instead we just add in 1/8th of the new measurement
		 * and subtract out 1/8 of the old srtt. We do the same with
		 * the variance after finding the absolute value of the
		 * difference between this sample and the current srtt.
		 */
		delta = tp->t_srtt - rtt;
		/* Take off 1/8th of the current sRTT */
		tp->t_srtt -= (tp->t_srtt >> 3);
		/* Add in 1/8th of the new RTT just measured */
		tp->t_srtt += (rtt >> 3);
		if (tp->t_srtt <= 0)
			tp->t_srtt = 1;
		/* Now lets make the absolute value of the variance */
		if (delta < 0)
			delta = -delta;
		/* Subtract out 1/8th */
		tp->t_rttvar -= (tp->t_rttvar >> 3);
		/* Add in 1/8th of the new variance we just saw */
		tp->t_rttvar += (delta >> 3);
		if (tp->t_rttvar <= 0)
			tp->t_rttvar = 1;
	} else {
		/*
		 * No rtt measurement yet - use the unsmoothed rtt. Set the
		 * variance to half the rtt (so our first retransmit happens
		 * at 3*rtt).
		 */
		tp->t_srtt = rtt;
		tp->t_rttvar = rtt >> 1;
	}
	rack->rc_srtt_measure_made = 1;
	KMOD_TCPSTAT_INC(tcps_rttupdated);
	if (tp->t_rttupdated < UCHAR_MAX)
		tp->t_rttupdated++;
#ifdef STATS
	if (rack_stats_gets_ms_rtt == 0) {
		/* Send in the microsecond rtt used for rxt timeout purposes */
		stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_RTT, imax(0, rtt));
	} else if (rack_stats_gets_ms_rtt == 1) {
		/* Send in the millisecond rtt used for rxt timeout purposes */
		int32_t ms_rtt;

		/* Round up */
		ms_rtt = (rtt + HPTS_USEC_IN_MSEC - 1) / HPTS_USEC_IN_MSEC;
		stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_RTT, imax(0, ms_rtt));
	} else if (rack_stats_gets_ms_rtt == 2) {
		/* Send in the millisecond rtt has close to the path RTT as we can get  */
		int32_t ms_rtt;

		/* Round up */
		ms_rtt = (rack->r_ctl.rack_rs.rs_us_rtt + HPTS_USEC_IN_MSEC - 1) / HPTS_USEC_IN_MSEC;
		stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_RTT, imax(0, ms_rtt));
	}  else {
		/* Send in the microsecond rtt has close to the path RTT as we can get  */
		stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_RTT, imax(0, rack->r_ctl.rack_rs.rs_us_rtt));
	}
	stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_PATHRTT, imax(0, rack->r_ctl.rack_rs.rs_us_rtt));
#endif
	rack->r_ctl.last_rcv_tstmp_for_rtt = tcp_tv_to_mssectick(&rack->r_ctl.act_rcv_time);
	/*
	 * the retransmit should happen at rtt + 4 * rttvar. Because of the
	 * way we do the smoothing, srtt and rttvar will each average +1/2
	 * tick of bias.  When we compute the retransmit timer, we want 1/2
	 * tick of rounding and 1 extra tick because of +-1/2 tick
	 * uncertainty in the firing of the timer.  The bias will give us
	 * exactly the 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below the minimum
	 * feasible timer (which is 2 ticks).
	 */
	tp->t_rxtshift = 0;
	RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
		      max(rack_rto_min, rtt + 2), rack_rto_max, rack->r_ctl.timer_slop);
	rack_log_rtt_sample(rack, rtt);
	tp->t_softerror = 0;
}


static void
rack_apply_updated_usrtt(struct tcp_rack *rack, uint32_t us_rtt, uint32_t us_cts)
{
	/*
	 * Apply to filter the inbound us-rtt at us_cts.
	 */
	uint32_t old_rtt;

	old_rtt = get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt);
	apply_filter_min_small(&rack->r_ctl.rc_gp_min_rtt,
			       us_rtt, us_cts);
	if (old_rtt > us_rtt) {
		/* We just hit a new lower rtt time */
		rack_log_rtt_shrinks(rack,  us_cts,  old_rtt,
				     __LINE__, RACK_RTTS_NEWRTT);
		/*
		 * Only count it if its lower than what we saw within our
		 * calculated range.
		 */
		if ((old_rtt - us_rtt) > rack_min_rtt_movement) {
			if (rack_probertt_lower_within &&
			    rack->rc_gp_dyn_mul &&
			    (rack->use_fixed_rate == 0) &&
			    (rack->rc_always_pace)) {
				/*
				 * We are seeing a new lower rtt very close
				 * to the time that we would have entered probe-rtt.
				 * This is probably due to the fact that a peer flow
				 * has entered probe-rtt. Lets go in now too.
				 */
				uint32_t val;

				val = rack_probertt_lower_within * rack_time_between_probertt;
				val /= 100;
				if ((rack->in_probe_rtt == 0)  &&
				    (rack->rc_skip_timely == 0) &&
				    ((us_cts - rack->r_ctl.rc_lower_rtt_us_cts) >= (rack_time_between_probertt - val)))	{
					rack_enter_probertt(rack, us_cts);
				}
			}
			rack->r_ctl.rc_lower_rtt_us_cts = us_cts;
		}
	}
}

static int
rack_update_rtt(struct tcpcb *tp, struct tcp_rack *rack,
    struct rack_sendmap *rsm, struct tcpopt *to, uint32_t cts, int32_t ack_type, tcp_seq th_ack)
{
	uint32_t us_rtt;
	int32_t i, all;
	uint32_t t, len_acked;

	if ((rsm->r_flags & RACK_ACKED) ||
	    (rsm->r_flags & RACK_WAS_ACKED))
		/* Already done */
		return (0);
	if (rsm->r_no_rtt_allowed) {
		/* Not allowed */
		return (0);
	}
	if (ack_type == CUM_ACKED) {
		if (SEQ_GT(th_ack, rsm->r_end)) {
			len_acked = rsm->r_end - rsm->r_start;
			all = 1;
		} else {
			len_acked = th_ack - rsm->r_start;
			all = 0;
		}
	} else {
		len_acked = rsm->r_end - rsm->r_start;
		all = 0;
	}
	if (rsm->r_rtr_cnt == 1) {

		t = cts - (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)];
		if ((int)t <= 0)
			t = 1;
		if (!tp->t_rttlow || tp->t_rttlow > t)
			tp->t_rttlow = t;
		if (!rack->r_ctl.rc_rack_min_rtt ||
		    SEQ_LT(t, rack->r_ctl.rc_rack_min_rtt)) {
			rack->r_ctl.rc_rack_min_rtt = t;
			if (rack->r_ctl.rc_rack_min_rtt == 0) {
				rack->r_ctl.rc_rack_min_rtt = 1;
			}
		}
		if (TSTMP_GT(tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time), rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)]))
			us_rtt = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time) - (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)];
		else
			us_rtt = tcp_get_usecs(NULL) - (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)];
		if (us_rtt == 0)
			us_rtt = 1;
		if (CC_ALGO(tp)->rttsample != NULL) {
			/* Kick the RTT to the CC */
			CC_ALGO(tp)->rttsample(&tp->t_ccv, us_rtt, 1, rsm->r_fas);
		}
		rack_apply_updated_usrtt(rack, us_rtt, tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time));
		if (ack_type == SACKED) {
			rack_log_rtt_sample_calc(rack, t, (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)], cts, 1);
			tcp_rack_xmit_timer(rack, t + 1, len_acked, us_rtt, 2 , rsm, rsm->r_rtr_cnt);
		} else {
			/*
			 * We need to setup what our confidence
			 * is in this ack.
			 *
			 * If the rsm was app limited and it is
			 * less than a mss in length (the end
			 * of the send) then we have a gap. If we
			 * were app limited but say we were sending
			 * multiple MSS's then we are more confident
			 * int it.
			 *
			 * When we are not app-limited then we see if
			 * the rsm is being included in the current
			 * measurement, we tell this by the app_limited_needs_set
			 * flag.
			 *
			 * Note that being cwnd blocked is not applimited
			 * as well as the pacing delay between packets which
			 * are sending only 1 or 2 MSS's also will show up
			 * in the RTT. We probably need to examine this algorithm
			 * a bit more and enhance it to account for the delay
			 * between rsm's. We could do that by saving off the
			 * pacing delay of each rsm (in an rsm) and then
			 * factoring that in somehow though for now I am
			 * not sure how :)
			 */
			int calc_conf = 0;

			if (rsm->r_flags & RACK_APP_LIMITED) {
				if (all && (len_acked <= ctf_fixed_maxseg(tp)))
					calc_conf = 0;
				else
					calc_conf = 1;
			} else if (rack->app_limited_needs_set == 0) {
				calc_conf = 1;
			} else {
				calc_conf = 0;
			}
			rack_log_rtt_sample_calc(rack, t, (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)], cts, 2);
			tcp_rack_xmit_timer(rack, t + 1, len_acked, us_rtt,
					    calc_conf, rsm, rsm->r_rtr_cnt);
		}
		if ((rsm->r_flags & RACK_TLP) &&
		    (!IN_FASTRECOVERY(tp->t_flags))) {
			/* Segment was a TLP and our retrans matched */
			if (rack->r_ctl.rc_tlp_cwnd_reduce) {
				rack_cong_signal(tp, CC_NDUPACK, th_ack, __LINE__);
			}
		}
		if ((rack->r_ctl.rc_rack_tmit_time == 0) ||
		    (SEQ_LT(rack->r_ctl.rc_rack_tmit_time,
			    (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)]))) {
			/* New more recent rack_tmit_time */
			rack->r_ctl.rc_rack_tmit_time = (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)];
			if (rack->r_ctl.rc_rack_tmit_time == 0)
				rack->r_ctl.rc_rack_tmit_time = 1;
			rack->rc_rack_rtt = t;
		}
		return (1);
	}
	/*
	 * We clear the soft/rxtshift since we got an ack.
	 * There is no assurance we will call the commit() function
	 * so we need to clear these to avoid incorrect handling.
	 */
	tp->t_rxtshift = 0;
	RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
		      rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
	tp->t_softerror = 0;
	if (to && (to->to_flags & TOF_TS) &&
	    (ack_type == CUM_ACKED) &&
	    (to->to_tsecr) &&
	    ((rsm->r_flags & RACK_OVERMAX) == 0)) {
		/*
		 * Now which timestamp does it match? In this block the ACK
		 * must be coming from a previous transmission.
		 */
		for (i = 0; i < rsm->r_rtr_cnt; i++) {
			if (rack_ts_to_msec(rsm->r_tim_lastsent[i]) == to->to_tsecr) {
				t = cts - (uint32_t)rsm->r_tim_lastsent[i];
				if ((int)t <= 0)
					t = 1;
				if (CC_ALGO(tp)->rttsample != NULL) {
					/*
					 * Kick the RTT to the CC, here
					 * we lie a bit in that we know the
					 * retransmission is correct even though
					 * we retransmitted. This is because
					 * we match the timestamps.
					 */
					if (TSTMP_GT(tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time), rsm->r_tim_lastsent[i]))
						us_rtt = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time) - (uint32_t)rsm->r_tim_lastsent[i];
					else
						us_rtt = tcp_get_usecs(NULL) - (uint32_t)rsm->r_tim_lastsent[i];
					CC_ALGO(tp)->rttsample(&tp->t_ccv, us_rtt, 1, rsm->r_fas);
				}
				if ((i + 1) < rsm->r_rtr_cnt) {
					/*
					 * The peer ack'd from our previous
					 * transmission. We have a spurious
					 * retransmission and thus we dont
					 * want to update our rack_rtt.
					 *
					 * Hmm should there be a CC revert here?
					 *
					 */
					return (0);
				}
				if (!tp->t_rttlow || tp->t_rttlow > t)
					tp->t_rttlow = t;
				if (!rack->r_ctl.rc_rack_min_rtt || SEQ_LT(t, rack->r_ctl.rc_rack_min_rtt)) {
					rack->r_ctl.rc_rack_min_rtt = t;
					if (rack->r_ctl.rc_rack_min_rtt == 0) {
						rack->r_ctl.rc_rack_min_rtt = 1;
					}
				}
				if ((rack->r_ctl.rc_rack_tmit_time == 0) ||
				    (SEQ_LT(rack->r_ctl.rc_rack_tmit_time,
					    (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)]))) {
					/* New more recent rack_tmit_time */
					rack->r_ctl.rc_rack_tmit_time = (uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)];
					if (rack->r_ctl.rc_rack_tmit_time == 0)
						rack->r_ctl.rc_rack_tmit_time = 1;
					rack->rc_rack_rtt = t;
				}
				rack_log_rtt_sample_calc(rack, t, (uint32_t)rsm->r_tim_lastsent[i], cts, 3);
				tcp_rack_xmit_timer(rack, t + 1, len_acked, t, 0, rsm,
						    rsm->r_rtr_cnt);
				return (1);
			}
		}
		/* If we are logging log out the sendmap */
		if (tcp_bblogging_on(rack->rc_tp)) {
			for (i = 0; i < rsm->r_rtr_cnt; i++) {
				rack_log_rtt_sendmap(rack, i, rsm->r_tim_lastsent[i], to->to_tsecr);
			}
		}
		goto ts_not_found;
	} else {
		/*
		 * Ok its a SACK block that we retransmitted. or a windows
		 * machine without timestamps. We can tell nothing from the
		 * time-stamp since its not there or the time the peer last
		 * received a segment that moved forward its cum-ack point.
		 */
ts_not_found:
		i = rsm->r_rtr_cnt - 1;
		t = cts - (uint32_t)rsm->r_tim_lastsent[i];
		if ((int)t <= 0)
			t = 1;
		if (rack->r_ctl.rc_rack_min_rtt && SEQ_LT(t, rack->r_ctl.rc_rack_min_rtt)) {
			/*
			 * We retransmitted and the ack came back in less
			 * than the smallest rtt we have observed. We most
			 * likely did an improper retransmit as outlined in
			 * 6.2 Step 2 point 2 in the rack-draft so we
			 * don't want to update our rack_rtt. We in
			 * theory (in future) might want to think about reverting our
			 * cwnd state but we won't for now.
			 */
			return (0);
		} else if (rack->r_ctl.rc_rack_min_rtt) {
			/*
			 * We retransmitted it and the retransmit did the
			 * job.
			 */
			if (!rack->r_ctl.rc_rack_min_rtt ||
			    SEQ_LT(t, rack->r_ctl.rc_rack_min_rtt)) {
				rack->r_ctl.rc_rack_min_rtt = t;
				if (rack->r_ctl.rc_rack_min_rtt == 0) {
					rack->r_ctl.rc_rack_min_rtt = 1;
				}
			}
			if ((rack->r_ctl.rc_rack_tmit_time == 0) ||
			    (SEQ_LT(rack->r_ctl.rc_rack_tmit_time,
				    (uint32_t)rsm->r_tim_lastsent[i]))) {
				/* New more recent rack_tmit_time */
				rack->r_ctl.rc_rack_tmit_time = (uint32_t)rsm->r_tim_lastsent[i];
				if (rack->r_ctl.rc_rack_tmit_time == 0)
					rack->r_ctl.rc_rack_tmit_time = 1;
				rack->rc_rack_rtt = t;
			}
			return (1);
		}
	}
	return (0);
}

/*
 * Mark the SACK_PASSED flag on all entries prior to rsm send wise.
 */
static void
rack_log_sack_passed(struct tcpcb *tp,
    struct tcp_rack *rack, struct rack_sendmap *rsm, uint32_t cts)
{
	struct rack_sendmap *nrsm;
	uint32_t thresh;

	/* Get our rxt threshold for lost consideration */
	thresh = rack_calc_thresh_rack(rack, rack_grab_rtt(tp, rack), cts, __LINE__, 0);
	/* Now start looking at rsm's */
	nrsm = rsm;
	TAILQ_FOREACH_REVERSE_FROM(nrsm, &rack->r_ctl.rc_tmap,
	    rack_head, r_tnext) {
		if (nrsm == rsm) {
			/* Skip original segment he is acked */
			continue;
		}
		if (nrsm->r_flags & RACK_ACKED) {
			/*
			 * Skip ack'd segments, though we
			 * should not see these, since tmap
			 * should not have ack'd segments.
			 */
			continue;
		}
		if (nrsm->r_flags & RACK_RWND_COLLAPSED) {
			/*
			 * If the peer dropped the rwnd on
			 * these then we don't worry about them.
			 */
			continue;
		}
		/* Check lost state */
		if ((nrsm->r_flags & RACK_WAS_LOST) == 0) {
			uint32_t exp;

			exp = ((uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)]) + thresh;
			if (TSTMP_LT(exp, cts) || (exp == cts)) {
				/* We consider it lost */
				nrsm->r_flags |= RACK_WAS_LOST;
				rack->r_ctl.rc_considered_lost += nrsm->r_end - nrsm->r_start;
			}
		}
		if (nrsm->r_flags & RACK_SACK_PASSED) {
			/*
			 * We found one that is already marked
			 * passed, we have been here before and
			 * so all others below this are marked.
			 */
			break;
		}
		nrsm->r_flags |= RACK_SACK_PASSED;
		nrsm->r_flags &= ~RACK_WAS_SACKPASS;
	}
}

static void
rack_need_set_test(struct tcpcb *tp,
		   struct tcp_rack *rack,
		   struct rack_sendmap *rsm,
		   tcp_seq th_ack,
		   int line,
		   int use_which)
{
	struct rack_sendmap *s_rsm;

	if ((tp->t_flags & TF_GPUTINPROG) &&
	    SEQ_GEQ(rsm->r_end, tp->gput_seq)) {
		/*
		 * We were app limited, and this ack
		 * butts up or goes beyond the point where we want
		 * to start our next measurement. We need
		 * to record the new gput_ts as here and
		 * possibly update the start sequence.
		 */
		uint32_t seq, ts;

		if (rsm->r_rtr_cnt > 1) {
			/*
			 * This is a retransmit, can we
			 * really make any assessment at this
			 * point?  We are not really sure of
			 * the timestamp, is it this or the
			 * previous transmission?
			 *
			 * Lets wait for something better that
			 * is not retransmitted.
			 */
			return;
		}
		seq = tp->gput_seq;
		ts = tp->gput_ts;
		rack->app_limited_needs_set = 0;
		tp->gput_ts = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
		/* Do we start at a new end? */
		if ((use_which == RACK_USE_BEG) &&
		    SEQ_GEQ(rsm->r_start, tp->gput_seq)) {
			/*
			 * When we get an ACK that just eats
			 * up some of the rsm, we set RACK_USE_BEG
			 * since whats at r_start (i.e. th_ack)
			 * is left unacked and thats where the
			 * measurement now starts.
			 */
			tp->gput_seq = rsm->r_start;
		}
		if ((use_which == RACK_USE_END) &&
		    SEQ_GEQ(rsm->r_end, tp->gput_seq)) {
			/*
			 * We use the end when the cumack
			 * is moving forward and completely
			 * deleting the rsm passed so basically
			 * r_end holds th_ack.
			 *
			 * For SACK's we also want to use the end
			 * since this piece just got sacked and
			 * we want to target anything after that
			 * in our measurement.
			 */
			tp->gput_seq = rsm->r_end;
		}
		if (use_which == RACK_USE_END_OR_THACK) {
			/*
			 * special case for ack moving forward,
			 * not a sack, we need to move all the
			 * way up to where this ack cum-ack moves
			 * to.
			 */
			if (SEQ_GT(th_ack, rsm->r_end))
				tp->gput_seq = th_ack;
			else
				tp->gput_seq = rsm->r_end;
		}
		if (SEQ_LT(tp->gput_seq, tp->snd_max))
			s_rsm = tqhash_find(rack->r_ctl.tqh, tp->gput_seq);
		else
			s_rsm = NULL;
		/*
		 * Pick up the correct send time if we can the rsm passed in
		 * may be equal to s_rsm if the RACK_USE_BEG was set. For the other
		 * two cases (RACK_USE_THACK or RACK_USE_END) most likely we will
		 * find a different seq i.e. the next send up.
		 *
		 * If that has not been sent, s_rsm will be NULL and we must
		 * arrange it so this function will get called again by setting
		 * app_limited_needs_set.
		 */
		if (s_rsm)
			rack->r_ctl.rc_gp_output_ts = s_rsm->r_tim_lastsent[0];
		else {
			/* If we hit here we have to have *not* sent tp->gput_seq */
			rack->r_ctl.rc_gp_output_ts = rsm->r_tim_lastsent[0];
			/* Set it up so we will go through here again */
			rack->app_limited_needs_set = 1;
		}
		if (SEQ_GT(tp->gput_seq, tp->gput_ack)) {
			/*
			 * We moved beyond this guy's range, re-calculate
			 * the new end point.
			 */
			if (rack->rc_gp_filled == 0) {
				tp->gput_ack = tp->gput_seq + max(rc_init_window(rack), (MIN_GP_WIN * ctf_fixed_maxseg(tp)));
			} else {
				tp->gput_ack = tp->gput_seq + rack_get_measure_window(tp, rack);
			}
		}
		/*
		 * We are moving the goal post, we may be able to clear the
		 * measure_saw_probe_rtt flag.
		 */
		if ((rack->in_probe_rtt == 0) &&
		    (rack->measure_saw_probe_rtt) &&
		    (SEQ_GEQ(tp->gput_seq, rack->r_ctl.rc_probertt_sndmax_atexit)))
			rack->measure_saw_probe_rtt = 0;
		rack_log_pacing_delay_calc(rack, ts, tp->gput_ts,
					   seq, tp->gput_seq,
					   (((uint64_t)rack->r_ctl.rc_app_limited_cnt << 32) |
					    (uint64_t)rack->r_ctl.rc_gp_output_ts),
					   5, line, NULL, 0);
		if (rack->rc_gp_filled &&
		    ((tp->gput_ack - tp->gput_seq) <
		     max(rc_init_window(rack), (MIN_GP_WIN *
						ctf_fixed_maxseg(tp))))) {
			uint32_t ideal_amount;

			ideal_amount = rack_get_measure_window(tp, rack);
			if (ideal_amount > sbavail(&tptosocket(tp)->so_snd)) {
				/*
				 * There is no sense of continuing this measurement
				 * because its too small to gain us anything we
				 * trust. Skip it and that way we can start a new
				 * measurement quicker.
				 */
				tp->t_flags &= ~TF_GPUTINPROG;
				rack_log_pacing_delay_calc(rack, tp->gput_ack, tp->gput_seq,
							   0, 0,
							   (((uint64_t)rack->r_ctl.rc_app_limited_cnt << 32) |
							    (uint64_t)rack->r_ctl.rc_gp_output_ts),
							   6, __LINE__, NULL, 0);
			} else {
				/*
				 * Reset the window further out.
				 */
				tp->gput_ack = tp->gput_seq + ideal_amount;
			}
		}
		rack_tend_gp_marks(tp, rack);
		rack_log_gpset(rack, tp->gput_ack, 0, 0, line, 2, rsm);
	}
}

static inline int
is_rsm_inside_declared_tlp_block(struct tcp_rack *rack, struct rack_sendmap *rsm)
{
	if (SEQ_LT(rsm->r_end, rack->r_ctl.last_tlp_acked_start)) {
		/* Behind our TLP definition or right at */
		return (0);
	}
	if (SEQ_GT(rsm->r_start, rack->r_ctl.last_tlp_acked_end)) {
		/* The start is beyond or right at our end of TLP definition */
		return (0);
	}
	/* It has to be a sub-part of the original TLP recorded */
	return (1);
}

static uint32_t
rack_proc_sack_blk(struct tcpcb *tp, struct tcp_rack *rack, struct sackblk *sack,
		   struct tcpopt *to, struct rack_sendmap **prsm, uint32_t cts,
		   int *no_extra,
		   int *moved_two, uint32_t segsiz)
{
	uint32_t start, end, changed = 0;
	struct rack_sendmap stack_map;
	struct rack_sendmap *rsm, *nrsm, *prev, *next;
	int insret __diagused;
	int32_t used_ref = 1;
	int moved = 0;
#ifdef TCP_SAD_DETECTION
	int allow_segsiz;
	int first_time_through = 1;
#endif
	int noextra = 0;
	int can_use_hookery = 0;

	start = sack->start;
	end = sack->end;
	rsm = *prsm;

#ifdef TCP_SAD_DETECTION
	/*
	 * There are a strange number of proxys and meddle boxes in the world
	 * that seem to cut up segments on different boundaries. This gets us
	 * smaller sacks that are still ok in terms of it being an attacker.
	 * We use the base segsiz to calculate an allowable smallness but
	 * also enforce a min on the segsiz in case it is an attacker playing
	 * games with MSS. So basically if the sack arrives and it is
	 * larger than a worse case 960 bytes, we don't classify the guy
	 * as supicious.
	 */
	allow_segsiz = max(segsiz, 1200) * sad_seg_size_per;
	allow_segsiz /= 1000;
#endif
do_rest_ofb:
	if ((rsm == NULL) ||
	    (SEQ_LT(end, rsm->r_start)) ||
	    (SEQ_GEQ(start, rsm->r_end)) ||
	    (SEQ_LT(start, rsm->r_start))) {
		/*
		 * We are not in the right spot,
		 * find the correct spot in the tree.
		 */
		used_ref = 0;
		rsm = tqhash_find(rack->r_ctl.tqh, start);
		moved++;
	}
	if (rsm == NULL) {
		/* TSNH */
		goto out;
	}
#ifdef TCP_SAD_DETECTION
	/* Now we must check for suspicous activity */
	if ((first_time_through == 1) &&
	    ((end - start) < min((rsm->r_end - rsm->r_start), allow_segsiz)) &&
	    ((rsm->r_flags & RACK_PMTU_CHG) == 0) &&
	    ((rsm->r_flags & RACK_TLP) == 0)) {
		/*
		 * Its less than a full MSS or the segment being acked
		 * this should only happen if the rsm in question had the
		 * r_just_ret flag set <and> the end matches the end of
		 * the rsm block.
		 *
		 * Note we do not look at segments that have had TLP's on
		 * them since we can get un-reported rwnd collapses that
		 * basically we TLP on and then we get back a sack block
		 * that goes from the start to only a small way.
		 *
		 */
		int loss, ok;

		ok = 0;
		if (SEQ_GEQ(end, rsm->r_end)) {
			if (rsm->r_just_ret == 1) {
				/* This was at the end of a send which is ok */
				ok = 1;
			} else {
				/* A bit harder was it the end of our segment */
				int segs, len;

				len = (rsm->r_end - rsm->r_start);
				segs = len / segsiz;
				segs *= segsiz;
				if ((segs + (rsm->r_end - start)) == len) {
					/*
					 * So this last bit was the
					 * end of our send if we cut it
					 * up into segsiz pieces so its ok.
					 */
					ok = 1;
				}
			}
		}
		if (ok == 0) {
			/*
			 * This guy is doing something suspicious
			 * lets start detection.
			 */
			if (rack->rc_suspicious == 0) {
				tcp_trace_point(rack->rc_tp, TCP_TP_SAD_SUSPECT);
				counter_u64_add(rack_sack_attacks_suspect, 1);
				rack->rc_suspicious = 1;
				rack_log_sad(rack, 4);
				if (tcp_bblogging_on(rack->rc_tp)) {
					union tcp_log_stackspecific log;
					struct timeval tv;

					memset(&log.u_bbr, 0, sizeof(log.u_bbr));
					log.u_bbr.flex1 = end;
					log.u_bbr.flex2 = start;
					log.u_bbr.flex3 = rsm->r_end;
					log.u_bbr.flex4 = rsm->r_start;
					log.u_bbr.flex5 = segsiz;
					log.u_bbr.flex6 = rsm->r_fas;
					log.u_bbr.flex7 = rsm->r_bas;
					log.u_bbr.flex8 = 5;
					log.u_bbr.pkts_out = rsm->r_flags;
					log.u_bbr.bbr_state = rack->rc_suspicious;
					log.u_bbr.bbr_substate = rsm->r_just_ret;
					log.u_bbr.timeStamp = tcp_get_usecs(&tv);
					log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
					TCP_LOG_EVENTP(rack->rc_tp, NULL,
						       &rack->rc_inp->inp_socket->so_rcv,
						       &rack->rc_inp->inp_socket->so_snd,
						       TCP_SAD_DETECTION, 0,
						       0, &log, false, &tv);
				}
			}
			/* You loose some ack count every time you sack
			 * a small bit that is not butting to the end of
			 * what we have sent. This is because we never
			 * send small bits unless its the end of the sb.
			 * Anyone sending a sack that is not at the end
			 * is thus very very suspicious.
			 */
			loss = (segsiz/2) / (end - start);
			if (loss < rack->r_ctl.ack_count)
				rack->r_ctl.ack_count -= loss;
			else
				rack->r_ctl.ack_count = 0;
		}
	}
	first_time_through = 0;
#endif
	/* Ok we have an ACK for some piece of this rsm */
	if (rsm->r_start != start) {
		if ((rsm->r_flags & RACK_ACKED) == 0) {
			/*
			 * Before any splitting or hookery is
			 * done is it a TLP of interest i.e. rxt?
			 */
			if ((rsm->r_flags & RACK_TLP) &&
			    (rsm->r_rtr_cnt > 1)) {
				/*
				 * We are splitting a rxt TLP, check
				 * if we need to save off the start/end
				 */
				if (rack->rc_last_tlp_acked_set &&
				    (is_rsm_inside_declared_tlp_block(rack, rsm))) {
					/*
					 * We already turned this on since we are inside
					 * the previous one was a partially sack now we
					 * are getting another one (maybe all of it).
					 *
					 */
					rack_log_dsack_event(rack, 10, __LINE__, rsm->r_start, rsm->r_end);
					/*
					 * Lets make sure we have all of it though.
					 */
					if (SEQ_LT(rsm->r_start, rack->r_ctl.last_tlp_acked_start)) {
						rack->r_ctl.last_tlp_acked_start = rsm->r_start;
						rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
								     rack->r_ctl.last_tlp_acked_end);
					}
					if (SEQ_GT(rsm->r_end, rack->r_ctl.last_tlp_acked_end)) {
						rack->r_ctl.last_tlp_acked_end = rsm->r_end;
						rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
								     rack->r_ctl.last_tlp_acked_end);
					}
				} else {
					rack->r_ctl.last_tlp_acked_start = rsm->r_start;
					rack->r_ctl.last_tlp_acked_end = rsm->r_end;
					rack->rc_last_tlp_past_cumack = 0;
					rack->rc_last_tlp_acked_set = 1;
					rack_log_dsack_event(rack, 8, __LINE__, rsm->r_start, rsm->r_end);
				}
			}
			/**
			 * Need to split this in two pieces the before and after,
			 * the before remains in the map, the after must be
			 * added. In other words we have:
			 * rsm        |--------------|
			 * sackblk        |------->
			 * rsm will become
			 *     rsm    |---|
			 * and nrsm will be  the sacked piece
			 *     nrsm       |----------|
			 *
			 * But before we start down that path lets
			 * see if the sack spans over on top of
			 * the next guy and it is already sacked.
			 *
			 */
			/*
			 * Hookery can only be used if the two entries
			 * are in the same bucket and neither one of
			 * them staddle the bucket line.
			 */
			next = tqhash_next(rack->r_ctl.tqh, rsm);
			if (next &&
			    (rsm->bindex == next->bindex) &&
			    ((rsm->r_flags & RACK_STRADDLE) == 0) &&
			    ((next->r_flags & RACK_STRADDLE) == 0) &&
			    ((rsm->r_flags & RACK_IS_PCM) == 0) &&
			    ((next->r_flags & RACK_IS_PCM) == 0) &&
			    (rsm->r_flags & RACK_IN_GP_WIN) &&
			    (next->r_flags & RACK_IN_GP_WIN))
				can_use_hookery = 1;
			else
				can_use_hookery = 0;
			if (next && can_use_hookery &&
			    (next->r_flags & RACK_ACKED) &&
			    SEQ_GEQ(end, next->r_start)) {
				/**
				 * So the next one is already acked, and
				 * we can thus by hookery use our stack_map
				 * to reflect the piece being sacked and
				 * then adjust the two tree entries moving
				 * the start and ends around. So we start like:
				 *  rsm     |------------|             (not-acked)
				 *  next                 |-----------| (acked)
				 *  sackblk        |-------->
				 *  We want to end like so:
				 *  rsm     |------|                   (not-acked)
				 *  next           |-----------------| (acked)
				 *  nrsm           |-----|
				 * Where nrsm is a temporary stack piece we
				 * use to update all the gizmos.
				 */
				/* Copy up our fudge block */
				noextra++;
				nrsm = &stack_map;
				memcpy(nrsm, rsm, sizeof(struct rack_sendmap));
				/* Now adjust our tree blocks */
				tqhash_update_end(rack->r_ctl.tqh, rsm, start);
				next->r_start = start;
 				rsm->r_flags |= RACK_SHUFFLED;
				next->r_flags |= RACK_SHUFFLED;
				/* Now we must adjust back where next->m is */
				rack_setup_offset_for_rsm(rack, rsm, next);
				/*
				 * Which timestamp do we keep? It is rather
				 * important in GP measurements to have the
				 * accurate end of the send window.
				 *
				 * We keep the largest value, which is the newest
				 * send. We do this in case a segment that is
				 * joined together and not part of a GP estimate
				 * later gets expanded into the GP estimate.
				 *
				 * We prohibit the merging of unlike kinds i.e.
				 * all pieces that are in the GP estimate can be
				 * merged and all pieces that are not in a GP estimate
				 * can be merged, but not disimilar pieces. Combine
				 * this with taking the highest here and we should
				 * be ok unless of course the client reneges. Then
				 * all bets are off.
				 */
				if (next->r_tim_lastsent[(next->r_rtr_cnt-1)] <
				    nrsm->r_tim_lastsent[(nrsm->r_rtr_cnt-1)])
					next->r_tim_lastsent[(next->r_rtr_cnt-1)] = nrsm->r_tim_lastsent[(nrsm->r_rtr_cnt-1)];
				/*
				 * And we must keep the newest ack arrival time.
				 */
				if (next->r_ack_arrival <
				    rack_to_usec_ts(&rack->r_ctl.act_rcv_time))
					next->r_ack_arrival = rack_to_usec_ts(&rack->r_ctl.act_rcv_time);


				/* We don't need to adjust rsm, it did not change */
				/* Clear out the dup ack count of the remainder */
				rsm->r_dupack = 0;
				rsm->r_just_ret = 0;
				rack_log_retran_reason(rack, rsm, __LINE__, 0, 2);
				/* Now lets make sure our fudge block is right */
				nrsm->r_start = start;
				/* Now lets update all the stats and such */
				rack_update_rtt(tp, rack, nrsm, to, cts, SACKED, 0);
				if (rack->app_limited_needs_set)
					rack_need_set_test(tp, rack, nrsm, tp->snd_una, __LINE__, RACK_USE_END);
				changed += (nrsm->r_end - nrsm->r_start);
				/* You get a count for acking a whole segment or more */
				if ((nrsm->r_end - nrsm->r_start) >= segsiz)
					rack->r_ctl.ack_count += ((nrsm->r_end - nrsm->r_start) / segsiz);
				rack->r_ctl.rc_sacked += (nrsm->r_end - nrsm->r_start);
				if (rsm->r_flags & RACK_WAS_LOST) {
					int my_chg;

					my_chg = (nrsm->r_end - nrsm->r_start);
					KASSERT((rack->r_ctl.rc_considered_lost >= my_chg),
						("rsm:%p rack:%p rc_considered_lost goes negative", rsm,  rack));
					if (my_chg <= rack->r_ctl.rc_considered_lost)
						rack->r_ctl.rc_considered_lost -= my_chg;
					else
						rack->r_ctl.rc_considered_lost = 0;
				}
				if (nrsm->r_flags & RACK_SACK_PASSED) {
					rack->r_ctl.rc_reorder_ts = cts;
					if (rack->r_ctl.rc_reorder_ts == 0)
						rack->r_ctl.rc_reorder_ts = 1;
				}
				/*
				 * Now we want to go up from rsm (the
				 * one left un-acked) to the next one
				 * in the tmap. We do this so when
				 * we walk backwards we include marking
				 * sack-passed on rsm (The one passed in
				 * is skipped since it is generally called
				 * on something sacked before removing it
				 * from the tmap).
				 */
				if (rsm->r_in_tmap) {
					nrsm = TAILQ_NEXT(rsm, r_tnext);
					/*
					 * Now that we have the next
					 * one walk backwards from there.
					 */
					if (nrsm && nrsm->r_in_tmap)
						rack_log_sack_passed(tp, rack, nrsm, cts);
				}
				/* Now are we done? */
				if (SEQ_LT(end, next->r_end) ||
				    (end == next->r_end)) {
					/* Done with block */
					goto out;
				}
				rack_log_map_chg(tp, rack, &stack_map, rsm, next, MAP_SACK_M1, end, __LINE__);
				counter_u64_add(rack_sack_used_next_merge, 1);
				/* Postion for the next block */
				start = next->r_end;
				rsm = tqhash_next(rack->r_ctl.tqh, next);
				if (rsm == NULL)
					goto out;
			} else {
				/**
				 * We can't use any hookery here, so we
				 * need to split the map. We enter like
				 * so:
				 *  rsm      |--------|
				 *  sackblk       |----->
				 * We will add the new block nrsm and
				 * that will be the new portion, and then
				 * fall through after reseting rsm. So we
				 * split and look like this:
				 *  rsm      |----|
				 *  sackblk       |----->
				 *  nrsm          |---|
				 * We then fall through reseting
				 * rsm to nrsm, so the next block
				 * picks it up.
				 */
				nrsm = rack_alloc_limit(rack, RACK_LIMIT_TYPE_SPLIT);
				if (nrsm == NULL) {
					/*
					 * failed XXXrrs what can we do but loose the sack
					 * info?
					 */
					goto out;
				}
				counter_u64_add(rack_sack_splits, 1);
				rack_clone_rsm(rack, nrsm, rsm, start);
				moved++;
				rsm->r_just_ret = 0;
#ifndef INVARIANTS
				(void)tqhash_insert(rack->r_ctl.tqh, nrsm);
#else
				if ((insret = tqhash_insert(rack->r_ctl.tqh, nrsm)) != 0) {
					panic("Insert in tailq_hash of %p fails ret:%d rack:%p rsm:%p",
					      nrsm, insret, rack, rsm);
				}
#endif
				if (rsm->r_in_tmap) {
					TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, rsm, nrsm, r_tnext);
					nrsm->r_in_tmap = 1;
				}
				rack_log_map_chg(tp, rack, NULL, rsm, nrsm, MAP_SACK_M2, end, __LINE__);
				rsm->r_flags &= (~RACK_HAS_FIN);
				/* Position us to point to the new nrsm that starts the sack blk */
				rsm = nrsm;
			}
		} else {
			/* Already sacked this piece */
			counter_u64_add(rack_sack_skipped_acked, 1);
			moved++;
			if (end == rsm->r_end) {
				/* Done with block */
				rsm = tqhash_next(rack->r_ctl.tqh, rsm);
				goto out;
			} else if (SEQ_LT(end, rsm->r_end)) {
				/* A partial sack to a already sacked block */
				moved++;
				rsm = tqhash_next(rack->r_ctl.tqh, rsm);
				goto out;
			} else {
				/*
				 * The end goes beyond this guy
				 * reposition the start to the
				 * next block.
				 */
				start = rsm->r_end;
				rsm = tqhash_next(rack->r_ctl.tqh, rsm);
				if (rsm == NULL)
					goto out;
			}
		}
	}
	if (SEQ_GEQ(end, rsm->r_end)) {
		/**
		 * The end of this block is either beyond this guy or right
		 * at this guy. I.e.:
		 *  rsm ---                 |-----|
		 *  end                     |-----|
		 *  <or>
		 *  end                     |---------|
		 */
		if ((rsm->r_flags & RACK_ACKED) == 0) {
			/*
			 * Is it a TLP of interest?
			 */
			if ((rsm->r_flags & RACK_TLP) &&
			    (rsm->r_rtr_cnt > 1)) {
				/*
				 * We are splitting a rxt TLP, check
				 * if we need to save off the start/end
				 */
				if (rack->rc_last_tlp_acked_set &&
				    (is_rsm_inside_declared_tlp_block(rack, rsm))) {
					/*
					 * We already turned this on since we are inside
					 * the previous one was a partially sack now we
					 * are getting another one (maybe all of it).
					 */
					rack_log_dsack_event(rack, 10, __LINE__, rsm->r_start, rsm->r_end);
					/*
					 * Lets make sure we have all of it though.
					 */
					if (SEQ_LT(rsm->r_start, rack->r_ctl.last_tlp_acked_start)) {
						rack->r_ctl.last_tlp_acked_start = rsm->r_start;
						rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
								     rack->r_ctl.last_tlp_acked_end);
					}
					if (SEQ_GT(rsm->r_end, rack->r_ctl.last_tlp_acked_end)) {
						rack->r_ctl.last_tlp_acked_end = rsm->r_end;
						rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
								     rack->r_ctl.last_tlp_acked_end);
					}
				} else {
					rack->r_ctl.last_tlp_acked_start = rsm->r_start;
					rack->r_ctl.last_tlp_acked_end = rsm->r_end;
					rack->rc_last_tlp_past_cumack = 0;
					rack->rc_last_tlp_acked_set = 1;
					rack_log_dsack_event(rack, 8, __LINE__, rsm->r_start, rsm->r_end);
				}
			}
			rack_update_rtt(tp, rack, rsm, to, cts, SACKED, 0);
			changed += (rsm->r_end - rsm->r_start);
			/* You get a count for acking a whole segment or more */
			if ((rsm->r_end - rsm->r_start) >= segsiz)
				rack->r_ctl.ack_count += ((rsm->r_end - rsm->r_start) / segsiz);
			if (rsm->r_flags & RACK_WAS_LOST) {
				int my_chg;

				my_chg = (rsm->r_end - rsm->r_start);
				rsm->r_flags &= ~RACK_WAS_LOST;
				KASSERT((rack->r_ctl.rc_considered_lost >= my_chg),
					("rsm:%p rack:%p rc_considered_lost goes negative", rsm,  rack));
				if (my_chg <= rack->r_ctl.rc_considered_lost)
					rack->r_ctl.rc_considered_lost -= my_chg;
				else
					rack->r_ctl.rc_considered_lost = 0;
			}
			rack->r_ctl.rc_sacked += (rsm->r_end - rsm->r_start);
			if (rsm->r_in_tmap) /* should be true */
				rack_log_sack_passed(tp, rack, rsm, cts);
			/* Is Reordering occuring? */
			if (rsm->r_flags & RACK_SACK_PASSED) {
				rsm->r_flags &= ~RACK_SACK_PASSED;
				rack->r_ctl.rc_reorder_ts = cts;
				if (rack->r_ctl.rc_reorder_ts == 0)
					rack->r_ctl.rc_reorder_ts = 1;
			}
			if (rack->app_limited_needs_set)
				rack_need_set_test(tp, rack, rsm, tp->snd_una, __LINE__, RACK_USE_END);
			rsm->r_ack_arrival = rack_to_usec_ts(&rack->r_ctl.act_rcv_time);
			rsm->r_flags |= RACK_ACKED;
			rack_update_pcm_ack(rack, 0, rsm->r_start, rsm->r_end);
			if (rsm->r_in_tmap) {
				TAILQ_REMOVE(&rack->r_ctl.rc_tmap, rsm, r_tnext);
				rsm->r_in_tmap = 0;
			}
			rack_log_map_chg(tp, rack, NULL, rsm, NULL, MAP_SACK_M3, end, __LINE__);
		} else {
			counter_u64_add(rack_sack_skipped_acked, 1);
			moved++;
		}
		if (end == rsm->r_end) {
			/* This block only - done, setup for next */
			goto out;
		}
		/*
		 * There is more not coverend by this rsm move on
		 * to the next block in the tail queue hash table.
		 */
		nrsm = tqhash_next(rack->r_ctl.tqh, rsm);
		start = rsm->r_end;
		rsm = nrsm;
		if (rsm == NULL)
			goto out;
		goto do_rest_ofb;
	}
	/**
	 * The end of this sack block is smaller than
	 * our rsm i.e.:
	 *  rsm ---                 |-----|
	 *  end                     |--|
	 */
	if ((rsm->r_flags & RACK_ACKED) == 0) {
		/*
		 * Is it a TLP of interest?
		 */
		if ((rsm->r_flags & RACK_TLP) &&
		    (rsm->r_rtr_cnt > 1)) {
			/*
			 * We are splitting a rxt TLP, check
			 * if we need to save off the start/end
			 */
			if (rack->rc_last_tlp_acked_set &&
			    (is_rsm_inside_declared_tlp_block(rack, rsm))) {
				/*
				 * We already turned this on since we are inside
				 * the previous one was a partially sack now we
				 * are getting another one (maybe all of it).
				 */
				rack_log_dsack_event(rack, 10, __LINE__, rsm->r_start, rsm->r_end);
				/*
				 * Lets make sure we have all of it though.
				 */
				if (SEQ_LT(rsm->r_start, rack->r_ctl.last_tlp_acked_start)) {
					rack->r_ctl.last_tlp_acked_start = rsm->r_start;
					rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
							     rack->r_ctl.last_tlp_acked_end);
				}
				if (SEQ_GT(rsm->r_end, rack->r_ctl.last_tlp_acked_end)) {
					rack->r_ctl.last_tlp_acked_end = rsm->r_end;
					rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
							     rack->r_ctl.last_tlp_acked_end);
				}
			} else {
				rack->r_ctl.last_tlp_acked_start = rsm->r_start;
				rack->r_ctl.last_tlp_acked_end = rsm->r_end;
				rack->rc_last_tlp_past_cumack = 0;
				rack->rc_last_tlp_acked_set = 1;
				rack_log_dsack_event(rack, 8, __LINE__, rsm->r_start, rsm->r_end);
			}
		}
		/*
		 * Hookery can only be used if the two entries
		 * are in the same bucket and neither one of
		 * them staddle the bucket line.
		 */
		prev = tqhash_prev(rack->r_ctl.tqh, rsm);
		if (prev &&
		    (rsm->bindex == prev->bindex) &&
		    ((rsm->r_flags & RACK_STRADDLE) == 0) &&
		    ((prev->r_flags & RACK_STRADDLE) == 0) &&
		    ((rsm->r_flags & RACK_IS_PCM) == 0) &&
		    ((prev->r_flags & RACK_IS_PCM) == 0) &&
		    (rsm->r_flags & RACK_IN_GP_WIN) &&
		    (prev->r_flags & RACK_IN_GP_WIN))
			can_use_hookery = 1;
		else
			can_use_hookery = 0;
		if (prev && can_use_hookery &&
		    (prev->r_flags & RACK_ACKED)) {
			/**
			 * Goal, we want the right remainder of rsm to shrink
			 * in place and span from (rsm->r_start = end) to rsm->r_end.
			 * We want to expand prev to go all the way
			 * to prev->r_end <- end.
			 * so in the tree we have before:
			 *   prev     |--------|         (acked)
			 *   rsm               |-------| (non-acked)
			 *   sackblk           |-|
			 * We churn it so we end up with
			 *   prev     |----------|       (acked)
			 *   rsm                 |-----| (non-acked)
			 *   nrsm              |-| (temporary)
			 *
			 * Note if either prev/rsm is a TLP we don't
			 * do this.
			 */
			noextra++;
			nrsm = &stack_map;
			memcpy(nrsm, rsm, sizeof(struct rack_sendmap));
			tqhash_update_end(rack->r_ctl.tqh, prev, end);
			rsm->r_start = end;
			rsm->r_flags |= RACK_SHUFFLED;
			prev->r_flags |= RACK_SHUFFLED;
			/* Now adjust nrsm (stack copy) to be
			 * the one that is the small
			 * piece that was "sacked".
			 */
			nrsm->r_end = end;
			rsm->r_dupack = 0;
			/*
			 * Which timestamp do we keep? It is rather
			 * important in GP measurements to have the
			 * accurate end of the send window.
			 *
			 * We keep the largest value, which is the newest
			 * send. We do this in case a segment that is
			 * joined together and not part of a GP estimate
			 * later gets expanded into the GP estimate.
			 *
			 * We prohibit the merging of unlike kinds i.e.
			 * all pieces that are in the GP estimate can be
			 * merged and all pieces that are not in a GP estimate
			 * can be merged, but not disimilar pieces. Combine
			 * this with taking the highest here and we should
			 * be ok unless of course the client reneges. Then
			 * all bets are off.
			 */
			if(prev->r_tim_lastsent[(prev->r_rtr_cnt-1)] <
			   nrsm->r_tim_lastsent[(nrsm->r_rtr_cnt-1)]) {
				prev->r_tim_lastsent[(prev->r_rtr_cnt-1)] = nrsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)];
			}
			/*
			 * And we must keep the newest ack arrival time.
			 */

			if(prev->r_ack_arrival <
			   rack_to_usec_ts(&rack->r_ctl.act_rcv_time))
				prev->r_ack_arrival = rack_to_usec_ts(&rack->r_ctl.act_rcv_time);

			rack_log_retran_reason(rack, rsm, __LINE__, 0, 2);
			/*
			 * Now that the rsm has had its start moved forward
			 * lets go ahead and get its new place in the world.
			 */
			rack_setup_offset_for_rsm(rack, prev, rsm);
			/*
			 * Now nrsm is our new little piece
			 * that is acked (which was merged
			 * to prev). Update the rtt and changed
			 * based on that. Also check for reordering.
			 */
			rack_update_rtt(tp, rack, nrsm, to, cts, SACKED, 0);
			if (rack->app_limited_needs_set)
				rack_need_set_test(tp, rack, nrsm, tp->snd_una, __LINE__, RACK_USE_END);
			changed += (nrsm->r_end - nrsm->r_start);
			/* You get a count for acking a whole segment or more */
			if ((nrsm->r_end - nrsm->r_start) >= segsiz)
				rack->r_ctl.ack_count += ((nrsm->r_end - nrsm->r_start) / segsiz);

			rack->r_ctl.rc_sacked += (nrsm->r_end - nrsm->r_start);
			if (rsm->r_flags & RACK_WAS_LOST) {
				int my_chg;

				my_chg = (nrsm->r_end - nrsm->r_start);
				KASSERT((rack->r_ctl.rc_considered_lost >= my_chg),
					("rsm:%p rack:%p rc_considered_lost goes negative", rsm,  rack));
				if (my_chg <= rack->r_ctl.rc_considered_lost)
					rack->r_ctl.rc_considered_lost -= my_chg;
				else
					rack->r_ctl.rc_considered_lost = 0;
			}
			if (nrsm->r_flags & RACK_SACK_PASSED) {
				rack->r_ctl.rc_reorder_ts = cts;
				if (rack->r_ctl.rc_reorder_ts == 0)
					rack->r_ctl.rc_reorder_ts = 1;
			}
			rack_log_map_chg(tp, rack, prev, &stack_map, rsm, MAP_SACK_M4, end, __LINE__);
			rsm = prev;
			counter_u64_add(rack_sack_used_prev_merge, 1);
		} else {
			/**
			 * This is the case where our previous
			 * block is not acked either, so we must
			 * split the block in two.
			 */
			nrsm = rack_alloc_limit(rack, RACK_LIMIT_TYPE_SPLIT);
			if (nrsm == NULL) {
				/* failed rrs what can we do but loose the sack info? */
				goto out;
			}
			if ((rsm->r_flags & RACK_TLP) &&
			    (rsm->r_rtr_cnt > 1)) {
				/*
				 * We are splitting a rxt TLP, check
				 * if we need to save off the start/end
				 */
				if (rack->rc_last_tlp_acked_set &&
				    (is_rsm_inside_declared_tlp_block(rack, rsm))) {
					/*
					 * We already turned this on since this block is inside
					 * the previous one was a partially sack now we
					 * are getting another one (maybe all of it).
					 */
					rack_log_dsack_event(rack, 10, __LINE__, rsm->r_start, rsm->r_end);
					/*
					 * Lets make sure we have all of it though.
					 */
					if (SEQ_LT(rsm->r_start, rack->r_ctl.last_tlp_acked_start)) {
						rack->r_ctl.last_tlp_acked_start = rsm->r_start;
						rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
								     rack->r_ctl.last_tlp_acked_end);
					}
					if (SEQ_GT(rsm->r_end, rack->r_ctl.last_tlp_acked_end)) {
						rack->r_ctl.last_tlp_acked_end = rsm->r_end;
						rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
								     rack->r_ctl.last_tlp_acked_end);
					}
				} else {
					rack->r_ctl.last_tlp_acked_start = rsm->r_start;
					rack->r_ctl.last_tlp_acked_end = rsm->r_end;
					rack->rc_last_tlp_acked_set = 1;
					rack->rc_last_tlp_past_cumack = 0;
					rack_log_dsack_event(rack, 8, __LINE__, rsm->r_start, rsm->r_end);
				}
			}
			/**
			 * In this case nrsm becomes
			 * nrsm->r_start = end;
			 * nrsm->r_end = rsm->r_end;
			 * which is un-acked.
			 * <and>
			 * rsm->r_end = nrsm->r_start;
			 * i.e. the remaining un-acked
			 * piece is left on the left
			 * hand side.
			 *
			 * So we start like this
			 * rsm      |----------| (not acked)
			 * sackblk  |---|
			 * build it so we have
			 * rsm      |---|         (acked)
			 * nrsm         |------|  (not acked)
			 */
			counter_u64_add(rack_sack_splits, 1);
			rack_clone_rsm(rack, nrsm, rsm, end);
			moved++;
			rsm->r_flags &= (~RACK_HAS_FIN);
			rsm->r_just_ret = 0;
#ifndef INVARIANTS
			(void)tqhash_insert(rack->r_ctl.tqh, nrsm);
#else
			if ((insret = tqhash_insert(rack->r_ctl.tqh, nrsm)) != 0) {
				panic("Insert in tailq_hash of %p fails ret:% rack:%p rsm:%p",
				      nrsm, insret, rack, rsm);
			}
#endif
			if (rsm->r_in_tmap) {
				TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, rsm, nrsm, r_tnext);
				nrsm->r_in_tmap = 1;
			}
			nrsm->r_dupack = 0;
			rack_log_retran_reason(rack, nrsm, __LINE__, 0, 2);
			rack_update_rtt(tp, rack, rsm, to, cts, SACKED, 0);
			changed += (rsm->r_end - rsm->r_start);
			/* You get a count for acking a whole segment or more */
			if ((rsm->r_end - rsm->r_start) >= segsiz)
				rack->r_ctl.ack_count += ((rsm->r_end - rsm->r_start) / segsiz);
			if (rsm->r_flags & RACK_WAS_LOST) {
				int my_chg;

				my_chg = (rsm->r_end - rsm->r_start);
				rsm->r_flags &= ~RACK_WAS_LOST;
				KASSERT((rack->r_ctl.rc_considered_lost >= my_chg),
					("rsm:%p rack:%p rc_considered_lost goes negative", rsm,  rack));
				if (my_chg <= rack->r_ctl.rc_considered_lost)
					rack->r_ctl.rc_considered_lost -= my_chg;
				else
					rack->r_ctl.rc_considered_lost = 0;
			}
			rack->r_ctl.rc_sacked += (rsm->r_end - rsm->r_start);

			if (rsm->r_in_tmap) /* should be true */
				rack_log_sack_passed(tp, rack, rsm, cts);
			/* Is Reordering occuring? */
			if (rsm->r_flags & RACK_SACK_PASSED) {
				rsm->r_flags &= ~RACK_SACK_PASSED;
				rack->r_ctl.rc_reorder_ts = cts;
				if (rack->r_ctl.rc_reorder_ts == 0)
					rack->r_ctl.rc_reorder_ts = 1;
			}
			if (rack->app_limited_needs_set)
				rack_need_set_test(tp, rack, rsm, tp->snd_una, __LINE__, RACK_USE_END);
			rsm->r_ack_arrival = rack_to_usec_ts(&rack->r_ctl.act_rcv_time);
			rsm->r_flags |= RACK_ACKED;
			rack_update_pcm_ack(rack, 0, rsm->r_start, rsm->r_end);
			rack_log_map_chg(tp, rack, NULL, rsm, nrsm, MAP_SACK_M5, end, __LINE__);
			if (rsm->r_in_tmap) {
				TAILQ_REMOVE(&rack->r_ctl.rc_tmap, rsm, r_tnext);
				rsm->r_in_tmap = 0;
			}
		}
	} else if (start != end){
		/*
		 * The block was already acked.
		 */
		counter_u64_add(rack_sack_skipped_acked, 1);
		moved++;
	}
out:
	if (rsm &&
	    ((rsm->r_flags & RACK_TLP) == 0) &&
	    (rsm->r_flags & RACK_ACKED)) {
		/*
		 * Now can we merge where we worked
		 * with either the previous or
		 * next block?
		 */
		next = tqhash_next(rack->r_ctl.tqh, rsm);
		while (next) {
			if (next->r_flags & RACK_TLP)
				break;
			/* Only allow merges between ones in or out of GP window */
			if ((next->r_flags & RACK_IN_GP_WIN) &&
			    ((rsm->r_flags & RACK_IN_GP_WIN) == 0)) {
				break;
			}
			if ((rsm->r_flags & RACK_IN_GP_WIN) &&
			    ((next->r_flags & RACK_IN_GP_WIN) == 0)) {
				break;
			}
			if (rsm->bindex != next->bindex)
				break;
			if (rsm->r_flags & RACK_STRADDLE)
				break;
			if (rsm->r_flags & RACK_IS_PCM)
				break;
			if (next->r_flags & RACK_STRADDLE)
				break;
			if (next->r_flags & RACK_IS_PCM)
				break;
			if (next->r_flags & RACK_ACKED) {
				/* yep this and next can be merged */
				rsm = rack_merge_rsm(rack, rsm, next);
				noextra++;
				next = tqhash_next(rack->r_ctl.tqh, rsm);
			} else
				break;
		}
		/* Now what about the previous? */
		prev = tqhash_prev(rack->r_ctl.tqh, rsm);
		while (prev) {
			if (prev->r_flags & RACK_TLP)
				break;
			/* Only allow merges between ones in or out of GP window */
			if ((prev->r_flags & RACK_IN_GP_WIN) &&
			    ((rsm->r_flags & RACK_IN_GP_WIN) == 0)) {
				break;
			}
			if ((rsm->r_flags & RACK_IN_GP_WIN) &&
			    ((prev->r_flags & RACK_IN_GP_WIN) == 0)) {
				break;
			}
			if (rsm->bindex != prev->bindex)
				break;
			if (rsm->r_flags & RACK_STRADDLE)
				break;
			if (rsm->r_flags & RACK_IS_PCM)
				break;
			if (prev->r_flags & RACK_STRADDLE)
				break;
			if (prev->r_flags & RACK_IS_PCM)
				break;
			if (prev->r_flags & RACK_ACKED) {
				/* yep the previous and this can be merged */
				rsm = rack_merge_rsm(rack, prev, rsm);
				noextra++;
				prev = tqhash_prev(rack->r_ctl.tqh, rsm);
			} else
				break;
		}
	}
	if (used_ref == 0) {
		counter_u64_add(rack_sack_proc_all, 1);
	} else {
		counter_u64_add(rack_sack_proc_short, 1);
	}
	/* Save off the next one for quick reference. */
	nrsm = tqhash_find(rack->r_ctl.tqh, end);
	*prsm = rack->r_ctl.rc_sacklast = nrsm;
	/* Pass back the moved. */
	*moved_two = moved;
	*no_extra = noextra;
	if (IN_RECOVERY(tp->t_flags)) {
		rack->r_ctl.bytes_acked_in_recovery += changed;
	}
	return (changed);
}

static void inline
rack_peer_reneges(struct tcp_rack *rack, struct rack_sendmap *rsm, tcp_seq th_ack)
{
	struct rack_sendmap *tmap;

	tmap = NULL;
	while (rsm && (rsm->r_flags & RACK_ACKED)) {
		/* Its no longer sacked, mark it so */
		rack->r_ctl.rc_sacked -= (rsm->r_end - rsm->r_start);
#ifdef INVARIANTS
		if (rsm->r_in_tmap) {
			panic("rack:%p rsm:%p flags:0x%x in tmap?",
			      rack, rsm, rsm->r_flags);
		}
#endif
		rsm->r_flags &= ~(RACK_ACKED|RACK_SACK_PASSED|RACK_WAS_SACKPASS);
		/* Rebuild it into our tmap */
		if (tmap == NULL) {
			TAILQ_INSERT_HEAD(&rack->r_ctl.rc_tmap, rsm, r_tnext);
			tmap = rsm;
		} else {
			TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, tmap, rsm, r_tnext);
			tmap = rsm;
		}
		tmap->r_in_tmap = 1;
		rsm = tqhash_next(rack->r_ctl.tqh, rsm);
	}
	/*
	 * Now lets possibly clear the sack filter so we start
	 * recognizing sacks that cover this area.
	 */
	sack_filter_clear(&rack->r_ctl.rack_sf, th_ack);

}

static void
rack_do_decay(struct tcp_rack *rack)
{
	struct timeval res;

#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)

	timersub(&rack->r_ctl.act_rcv_time, &rack->r_ctl.rc_last_time_decay, &res);
#undef timersub

	rack->r_ctl.input_pkt++;
	if ((rack->rc_in_persist) ||
	    (res.tv_sec >= 1) ||
	    (rack->rc_tp->snd_max == rack->rc_tp->snd_una)) {
		/*
		 * Check for decay of non-SAD,
		 * we want all SAD detection metrics to
		 * decay 1/4 per second (or more) passed.
		 * Current default is 800 so it decays
		 * 80% every second.
		 */
#ifdef TCP_SAD_DETECTION
		uint32_t pkt_delta;

		pkt_delta = rack->r_ctl.input_pkt - rack->r_ctl.saved_input_pkt;
#endif
		/* Update our saved tracking values */
		rack->r_ctl.saved_input_pkt = rack->r_ctl.input_pkt;
		rack->r_ctl.rc_last_time_decay = rack->r_ctl.act_rcv_time;
		/* Now do we escape without decay? */
#ifdef TCP_SAD_DETECTION
		if (rack->rc_in_persist ||
		    (rack->rc_tp->snd_max == rack->rc_tp->snd_una) ||
		    (pkt_delta < tcp_sad_low_pps)){
			/*
			 * We don't decay idle connections
			 * or ones that have a low input pps.
			 */
			return;
		}
		/* Decay the counters */
		rack->r_ctl.ack_count = ctf_decay_count(rack->r_ctl.ack_count,
							tcp_sad_decay_val);
		rack->r_ctl.sack_count = ctf_decay_count(rack->r_ctl.sack_count,
							 tcp_sad_decay_val);
		rack->r_ctl.sack_moved_extra = ctf_decay_count(rack->r_ctl.sack_moved_extra,
							       tcp_sad_decay_val);
		rack->r_ctl.sack_noextra_move = ctf_decay_count(rack->r_ctl.sack_noextra_move,
								tcp_sad_decay_val);
#endif
	}
}

static void inline
rack_rsm_sender_update(struct tcp_rack *rack, struct tcpcb *tp, struct rack_sendmap *rsm, uint8_t from)
{
	/*
	 * We look at advancing the end send time for our GP
	 * measurement tracking only as the cumulative acknowledgment
	 * moves forward. You might wonder about this, why not
	 * at every transmission or retransmission within the
	 * GP window update the rc_gp_cumack_ts? Well its rather
	 * nuanced but basically the GP window *may* expand (as
	 * it does below) or worse and harder to track it may shrink.
	 *
	 * This last makes it impossible to track at the time of
	 * the send, since you may set forward your rc_gp_cumack_ts
	 * when you send, because that send *is* in your currently
	 * "guessed" window, but then it shrinks. Now which was
	 * the send time of the last bytes in the window, by the
	 * time you ask that question that part of the sendmap
	 * is freed. So you don't know and you will have too
	 * long of send window. Instead by updating the time
	 * marker only when the cumack advances this assures us
	 * that we will have only the sends in the window of our
	 * GP measurement.
	 *
	 * Another complication from this is the
	 * merging of sendmap entries. During SACK processing this
	 * can happen to conserve the sendmap size. That breaks
	 * everything down in tracking the send window of the GP
	 * estimate. So to prevent that and keep it working with
	 * a tiny bit more limited merging, we only allow like
	 * types to be merged. I.e. if two sends are in the GP window
	 * then its ok to merge them together. If two sends are not
	 * in the GP window its ok to merge them together too. Though
	 * one send in and one send out cannot be merged. We combine
	 * this with never allowing the shrinking of the GP window when
	 * we are in recovery so that we can properly calculate the
	 * sending times.
	 *
	 * This all of course seems complicated, because it is.. :)
	 *
	 * The cum-ack is being advanced upon the sendmap.
	 * If we are not doing a GP estimate don't
	 * proceed.
	 */
	uint64_t ts;

	if ((tp->t_flags & TF_GPUTINPROG) == 0)
		return;
	/*
	 * If this sendmap entry is going
	 * beyond the measurement window we had picked,
	 * expand the measurement window by that much.
	 */
	if (SEQ_GT(rsm->r_end, tp->gput_ack)) {
		tp->gput_ack = rsm->r_end;
	}
	/*
	 * If we have not setup a ack, then we
	 * have no idea if the newly acked pieces
	 * will be "in our seq measurement range". If
	 * it is when we clear the app_limited_needs_set
	 * flag the timestamp will be updated.
	 */
	if (rack->app_limited_needs_set)
		return;
	/*
	 * Finally, we grab out the latest timestamp
	 * that this packet was sent and then see
	 * if:
	 *  a) The packet touches are newly defined GP range.
	 *  b) The time is greater than (newer) than the
	 *     one we currently have. If so we update
	 *     our sending end time window.
	 *
	 * Note we *do not* do this at send time. The reason
	 * is that if you do you *may* pick up a newer timestamp
	 * for a range you are not going to measure. We project
	 * out how far and then sometimes modify that to be
	 * smaller. If that occurs then you will have a send
	 * that does not belong to the range included.
	 */
	if ((ts = rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)]) <=
	    rack->r_ctl.rc_gp_cumack_ts)
		return;
	if (rack_in_gp_window(tp, rsm)) {
		rack->r_ctl.rc_gp_cumack_ts = ts;
		rack_log_gpset(rack, tp->gput_ack, (uint32_t)ts, rsm->r_end,
			       __LINE__, from, rsm);
	}
}

static void
rack_process_to_cumack(struct tcpcb *tp, struct tcp_rack *rack, register uint32_t th_ack, uint32_t cts, struct tcpopt *to, uint64_t acktime)
{
	struct rack_sendmap *rsm;
	/*
	 * The ACK point is advancing to th_ack, we must drop off
	 * the packets in the rack log and calculate any eligble
	 * RTT's.
	 */

	if (sack_filter_blks_used(&rack->r_ctl.rack_sf)) {
		/*
		 * If we have some sack blocks in the filter
		 * lets prune them out by calling sfb with no blocks.
		 */
		sack_filter_blks(&rack->r_ctl.rack_sf, NULL, 0, th_ack);
	}
	if (SEQ_GT(th_ack, tp->snd_una)) {
		/* Clear any app ack remembered settings */
		rack->r_ctl.cleared_app_ack = 0;
	}
	rack->r_wanted_output = 1;
	if (SEQ_GT(th_ack, tp->snd_una))
		rack->r_ctl.last_cumack_advance = acktime;

	/* Tend any TLP that has been marked for 1/2 the seq space (its old)  */
	if ((rack->rc_last_tlp_acked_set == 1)&&
	    (rack->rc_last_tlp_past_cumack == 1) &&
	    (SEQ_GT(rack->r_ctl.last_tlp_acked_start, th_ack))) {
		/*
		 * We have reached the point where our last rack
		 * tlp retransmit sequence is ahead of the cum-ack.
		 * This can only happen when the cum-ack moves all
		 * the way around (its been a full 2^^31+1 bytes
		 * or more since we sent a retransmitted TLP). Lets
		 * turn off the valid flag since its not really valid.
		 *
		 * Note since sack's also turn on this event we have
		 * a complication, we have to wait to age it out until
		 * the cum-ack is by the TLP before checking which is
		 * what the next else clause does.
		 */
		rack_log_dsack_event(rack, 9, __LINE__,
				     rack->r_ctl.last_tlp_acked_start,
				     rack->r_ctl.last_tlp_acked_end);
		rack->rc_last_tlp_acked_set = 0;
		rack->rc_last_tlp_past_cumack = 0;
	} else if ((rack->rc_last_tlp_acked_set == 1) &&
		   (rack->rc_last_tlp_past_cumack == 0) &&
		   (SEQ_GEQ(th_ack, rack->r_ctl.last_tlp_acked_end))) {
		/*
		 * It is safe to start aging TLP's out.
		 */
		rack->rc_last_tlp_past_cumack = 1;
	}
	/* We do the same for the tlp send seq as well */
	if ((rack->rc_last_sent_tlp_seq_valid == 1) &&
	    (rack->rc_last_sent_tlp_past_cumack == 1) &&
	    (SEQ_GT(rack->r_ctl.last_sent_tlp_seq,  th_ack))) {
		rack_log_dsack_event(rack, 9, __LINE__,
				     rack->r_ctl.last_sent_tlp_seq,
				     (rack->r_ctl.last_sent_tlp_seq +
				      rack->r_ctl.last_sent_tlp_len));
		rack->rc_last_sent_tlp_seq_valid = 0;
		rack->rc_last_sent_tlp_past_cumack = 0;
	} else if ((rack->rc_last_sent_tlp_seq_valid == 1) &&
		   (rack->rc_last_sent_tlp_past_cumack == 0) &&
		   (SEQ_GEQ(th_ack, rack->r_ctl.last_sent_tlp_seq))) {
		/*
		 * It is safe to start aging TLP's send.
		 */
		rack->rc_last_sent_tlp_past_cumack = 1;
	}
more:
	rsm = tqhash_min(rack->r_ctl.tqh);
	if (rsm == NULL) {
		if ((th_ack - 1) == tp->iss) {
			/*
			 * For the SYN incoming case we will not
			 * have called tcp_output for the sending of
			 * the SYN, so there will be no map. All
			 * other cases should probably be a panic.
			 */
			return;
		}
		if (tp->t_flags & TF_SENTFIN) {
			/* if we sent a FIN we often will not have map */
			return;
		}
#ifdef INVARIANTS
		panic("No rack map tp:%p for state:%d ack:%u rack:%p snd_una:%u snd_max:%u\n",
		      tp,
		      tp->t_state, th_ack, rack,
		      tp->snd_una, tp->snd_max);
#endif
		return;
	}
	if (SEQ_LT(th_ack, rsm->r_start)) {
		/* Huh map is missing this */
#ifdef INVARIANTS
		printf("Rack map starts at r_start:%u for th_ack:%u huh? ts:%d rs:%d\n",
		       rsm->r_start,
		       th_ack, tp->t_state, rack->r_state);
#endif
		return;
	}
	rack_update_rtt(tp, rack, rsm, to, cts, CUM_ACKED, th_ack);

	/* Now was it a retransmitted TLP? */
	if ((rsm->r_flags & RACK_TLP) &&
	    (rsm->r_rtr_cnt > 1)) {
		/*
		 * Yes, this rsm was a TLP and retransmitted, remember that
		 * since if a DSACK comes back on this we don't want
		 * to think of it as a reordered segment. This may
		 * get updated again with possibly even other TLPs
		 * in flight, but thats ok. Only when we don't send
		 * a retransmitted TLP for 1/2 the sequences space
		 * will it get turned off (above).
		 */
		if (rack->rc_last_tlp_acked_set &&
		    (is_rsm_inside_declared_tlp_block(rack, rsm))) {
			/*
			 * We already turned this on since the end matches,
			 * the previous one was a partially ack now we
			 * are getting another one (maybe all of it).
			 */
			rack_log_dsack_event(rack, 10, __LINE__, rsm->r_start, rsm->r_end);
			/*
			 * Lets make sure we have all of it though.
			 */
			if (SEQ_LT(rsm->r_start, rack->r_ctl.last_tlp_acked_start)) {
				rack->r_ctl.last_tlp_acked_start = rsm->r_start;
				rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
						     rack->r_ctl.last_tlp_acked_end);
			}
			if (SEQ_GT(rsm->r_end, rack->r_ctl.last_tlp_acked_end)) {
				rack->r_ctl.last_tlp_acked_end = rsm->r_end;
				rack_log_dsack_event(rack, 11, __LINE__, rack->r_ctl.last_tlp_acked_start,
						     rack->r_ctl.last_tlp_acked_end);
			}
		} else {
			rack->rc_last_tlp_past_cumack = 1;
			rack->r_ctl.last_tlp_acked_start = rsm->r_start;
			rack->r_ctl.last_tlp_acked_end = rsm->r_end;
			rack->rc_last_tlp_acked_set = 1;
			rack_log_dsack_event(rack, 8, __LINE__, rsm->r_start, rsm->r_end);
		}
	}
	/* Now do we consume the whole thing? */
	rack->r_ctl.last_tmit_time_acked = rsm->r_tim_lastsent[(rsm->r_rtr_cnt - 1)];
	if (SEQ_GEQ(th_ack, rsm->r_end)) {
		/* Its all consumed. */
		uint32_t left;
		uint8_t newly_acked;

		if (rsm->r_flags & RACK_WAS_LOST) {
			/*
			 * This can happen when we marked it as lost
			 * and yet before retransmitting we get an ack
			 * which can happen due to reordering.
			 */
			rsm->r_flags  &= ~RACK_WAS_LOST;
			KASSERT((rack->r_ctl.rc_considered_lost >= (rsm->r_end - rsm->r_start)),
				("rsm:%p rack:%p rc_considered_lost goes negative", rsm,  rack));
			if (rack->r_ctl.rc_considered_lost >= (rsm->r_end - rsm->r_start))
				rack->r_ctl.rc_considered_lost -= rsm->r_end - rsm->r_start;
			else
				rack->r_ctl.rc_considered_lost = 0;
		}
		rack_log_map_chg(tp, rack, NULL, rsm, NULL, MAP_FREE, rsm->r_end, __LINE__);
		rack->r_ctl.rc_holes_rxt -= rsm->r_rtr_bytes;
		rsm->r_rtr_bytes = 0;
		/*
		 * Record the time of highest cumack sent if its in our measurement
		 * window and possibly bump out the end.
		 */
		rack_rsm_sender_update(rack, tp, rsm, 4);
		tqhash_remove(rack->r_ctl.tqh, rsm, REMOVE_TYPE_CUMACK);
		if (rsm->r_in_tmap) {
			TAILQ_REMOVE(&rack->r_ctl.rc_tmap, rsm, r_tnext);
			rsm->r_in_tmap = 0;
		}
		newly_acked = 1;
		if (((rsm->r_flags & RACK_ACKED) == 0) &&
		    (IN_RECOVERY(tp->t_flags))) {
			rack->r_ctl.bytes_acked_in_recovery += (rsm->r_end - rsm->r_start);
		}
		if (rsm->r_flags & RACK_ACKED) {
			/*
			 * It was acked on the scoreboard -- remove
			 * it from total
			 */
			rack->r_ctl.rc_sacked -= (rsm->r_end - rsm->r_start);
			newly_acked = 0;
		} else if (rsm->r_flags & RACK_SACK_PASSED) {
			/*
			 * There are segments ACKED on the
			 * scoreboard further up. We are seeing
			 * reordering.
			 */
			rsm->r_flags &= ~RACK_SACK_PASSED;
			rsm->r_ack_arrival = rack_to_usec_ts(&rack->r_ctl.act_rcv_time);
			rsm->r_flags |= RACK_ACKED;
			rack->r_ctl.rc_reorder_ts = cts;
			if (rack->r_ctl.rc_reorder_ts == 0)
				rack->r_ctl.rc_reorder_ts = 1;
			if (rack->r_ent_rec_ns) {
				/*
				 * We have sent no more, and we saw an sack
				 * then ack arrive.
				 */
				rack->r_might_revert = 1;
			}
			rack_update_pcm_ack(rack, 1, rsm->r_start, rsm->r_end);
		} else {
			rack_update_pcm_ack(rack, 1, rsm->r_start, rsm->r_end);
		}
		if ((rsm->r_flags & RACK_TO_REXT) &&
		    (tp->t_flags & TF_RCVD_TSTMP) &&
		    (to->to_flags & TOF_TS) &&
		    (to->to_tsecr != 0) &&
		    (tp->t_flags & TF_PREVVALID)) {
			/*
			 * We can use the timestamp to see
			 * if this retransmission was from the
			 * first transmit. If so we made a mistake.
			 */
			tp->t_flags &= ~TF_PREVVALID;
			if (to->to_tsecr == rack_ts_to_msec(rsm->r_tim_lastsent[0])) {
				/* The first transmit is what this ack is for */
				rack_cong_signal(tp, CC_RTO_ERR, th_ack, __LINE__);
			}
		}
		left = th_ack - rsm->r_end;
		if (rack->app_limited_needs_set && newly_acked)
			rack_need_set_test(tp, rack, rsm, th_ack, __LINE__, RACK_USE_END_OR_THACK);
		/* Free back to zone */
		rack_free(rack, rsm);
		if (left) {
			goto more;
		}
		/* Check for reneging */
		rsm = tqhash_min(rack->r_ctl.tqh);
		if (rsm && (rsm->r_flags & RACK_ACKED) && (th_ack == rsm->r_start)) {
			/*
			 * The peer has moved snd_una up to
			 * the edge of this send, i.e. one
			 * that it had previously acked. The only
			 * way that can be true if the peer threw
			 * away data (space issues) that it had
			 * previously sacked (else it would have
			 * given us snd_una up to (rsm->r_end).
			 * We need to undo the acked markings here.
			 *
			 * Note we have to look to make sure th_ack is
			 * our rsm->r_start in case we get an old ack
			 * where th_ack is behind snd_una.
			 */
			rack_peer_reneges(rack, rsm, th_ack);
		}
		return;
	}
	if (rsm->r_flags & RACK_ACKED) {
		/*
		 * It was acked on the scoreboard -- remove it from
		 * total for the part being cum-acked.
		 */
		rack->r_ctl.rc_sacked -= (th_ack - rsm->r_start);
	} else {
		if (((rsm->r_flags & RACK_ACKED) == 0) &&
		    (IN_RECOVERY(tp->t_flags))) {
			rack->r_ctl.bytes_acked_in_recovery += (th_ack - rsm->r_start);
		}
		rack_update_pcm_ack(rack, 1, rsm->r_start, th_ack);
	}
	/* And what about the lost flag? */
	if (rsm->r_flags & RACK_WAS_LOST) {
		/*
		 * This can happen when we marked it as lost
		 * and yet before retransmitting we get an ack
		 * which can happen due to reordering. In this
		 * case its only a partial ack of the send.
		 */
		KASSERT((rack->r_ctl.rc_considered_lost >= (th_ack - rsm->r_start)),
			("rsm:%p rack:%p rc_considered_lost goes negative th_ack:%u", rsm,  rack, th_ack));
		if (rack->r_ctl.rc_considered_lost >= (th_ack - rsm->r_start))
			rack->r_ctl.rc_considered_lost -= th_ack - rsm->r_start;
		else
			rack->r_ctl.rc_considered_lost = 0;
	}
	/*
	 * Clear the dup ack count for
	 * the piece that remains.
	 */
	rsm->r_dupack = 0;
	rack_log_retran_reason(rack, rsm, __LINE__, 0, 2);
	if (rsm->r_rtr_bytes) {
		/*
		 * It was retransmitted adjust the
		 * sack holes for what was acked.
		 */
		int ack_am;

		ack_am = (th_ack - rsm->r_start);
		if (ack_am >= rsm->r_rtr_bytes) {
			rack->r_ctl.rc_holes_rxt -= ack_am;
			rsm->r_rtr_bytes -= ack_am;
		}
	}
	/*
	 * Update where the piece starts and record
	 * the time of send of highest cumack sent if
	 * its in our GP range.
	 */
	rack_log_map_chg(tp, rack, NULL, rsm, NULL, MAP_TRIM_HEAD, th_ack, __LINE__);
	/* Now we need to move our offset forward too */
	if (rsm->m &&
	    ((rsm->orig_m_len != rsm->m->m_len) ||
	     (M_TRAILINGROOM(rsm->m) != rsm->orig_t_space))) {
		/* Fix up the orig_m_len and possibly the mbuf offset */
		rack_adjust_orig_mlen(rsm);
	}
	rsm->soff += (th_ack - rsm->r_start);
	rack_rsm_sender_update(rack, tp, rsm, 5);
	/* The trim will move th_ack into r_start for us */
	tqhash_trim(rack->r_ctl.tqh, th_ack);
	/* Now do we need to move the mbuf fwd too? */
	{
		struct mbuf *m;
		uint32_t soff;

		m = rsm->m;
		soff = rsm->soff;
		if (m) {
			while (soff >= m->m_len) {
				soff -= m->m_len;
				KASSERT((m->m_next != NULL),
					(" rsm:%p  off:%u soff:%u m:%p",
					 rsm, rsm->soff, soff, m));
				m = m->m_next;
				if (m == NULL) {
					/*
					 * This is a fall-back that prevents a panic. In reality
					 * we should be able to walk the mbuf's and find our place.
					 * At this point snd_una has not been updated with the sbcut() yet
					 * but tqhash_trim did update rsm->r_start so the offset calcuation
					 * should work fine. This is undesirable since we will take cache
					 * hits to access the socket buffer. And even more puzzling is that
					 * it happens occasionally. It should not :(
					 */
					m = sbsndmbuf(&rack->rc_inp->inp_socket->so_snd,
						      (rsm->r_start - tp->snd_una),
						      &soff);
					break;
				}
			}
			/*
			 * Now save in our updated values.
			 */
			rsm->m = m;
			rsm->soff = soff;
			rsm->orig_m_len = rsm->m->m_len;
			rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
		}
	}
	if (rack->app_limited_needs_set &&
	    SEQ_GEQ(th_ack, tp->gput_seq))
		rack_need_set_test(tp, rack, rsm, tp->snd_una, __LINE__, RACK_USE_BEG);
}

static void
rack_handle_might_revert(struct tcpcb *tp, struct tcp_rack *rack)
{
	struct rack_sendmap *rsm;
	int sack_pass_fnd = 0;

	if (rack->r_might_revert) {
		/*
		 * Ok we have reordering, have not sent anything, we
		 * might want to revert the congestion state if nothing
		 * further has SACK_PASSED on it. Lets check.
		 *
		 * We also get here when we have DSACKs come in for
		 * all the data that we FR'd. Note that a rxt or tlp
		 * timer clears this from happening.
		 */

		TAILQ_FOREACH(rsm, &rack->r_ctl.rc_tmap, r_tnext) {
			if (rsm->r_flags & RACK_SACK_PASSED) {
				sack_pass_fnd = 1;
				break;
			}
		}
		if (sack_pass_fnd == 0) {
			/*
			 * We went into recovery
			 * incorrectly due to reordering!
			 */
			int orig_cwnd;

			rack->r_ent_rec_ns = 0;
			orig_cwnd = tp->snd_cwnd;
			tp->snd_ssthresh = rack->r_ctl.rc_ssthresh_at_erec;
			tp->snd_recover = tp->snd_una;
			rack_log_to_prr(rack, 14, orig_cwnd, __LINE__);
			if (IN_RECOVERY(tp->t_flags)) {
				rack_exit_recovery(tp, rack, 3);
				if ((rack->rto_from_rec == 1) && (rack_ssthresh_rest_rto_rec != 0) ){
					/*
					 * We were in recovery, had an RTO
					 * and then re-entered recovery (more sack's arrived)
					 * and we have properly recorded the old ssthresh from
					 * the first recovery. We want to be able to slow-start
					 * back to this level. The ssthresh from the timeout
					 * and then back into recovery will end up most likely
					 * to be min(cwnd=1mss, 2mss). Which makes it basically
					 * so we get no slow-start after our RTO.
					 */
					rack->rto_from_rec = 0;
					if (rack->r_ctl.rto_ssthresh > tp->snd_ssthresh)
						tp->snd_ssthresh = rack->r_ctl.rto_ssthresh;
				}
			}
			rack->r_ctl.bytes_acked_in_recovery = 0;
			rack->r_ctl.time_entered_recovery = 0;
		}
		rack->r_might_revert = 0;
	}
}

#ifdef TCP_SAD_DETECTION

static void
rack_merge_out_sacks(struct tcp_rack *rack)
{
	struct rack_sendmap *cur, *next, *rsm, *trsm = NULL;

	cur = tqhash_min(rack->r_ctl.tqh);
	while(cur) {
		next = tqhash_next(rack->r_ctl.tqh, cur);
		/*
		 * The idea is to go through all and merge back
		 * together the pieces sent together,
		 */
		if ((next != NULL) &&
		    (cur->r_tim_lastsent[0] == next->r_tim_lastsent[0])) {
			rack_merge_rsm(rack, cur, next);
		} else {
			cur = next;
		}
	}
	/*
	 * now treat it like a rxt event, everything is outstanding
	 * and sent nothing acvked and dupacks are all zero. If this
	 * is not an attacker it will have to dupack its way through
	 * it all.
	 */
	TAILQ_INIT(&rack->r_ctl.rc_tmap);
	TQHASH_FOREACH(rsm, rack->r_ctl.tqh)  {
		rsm->r_dupack = 0;
		/* We must re-add it back to the tlist */
		if (trsm == NULL) {
			TAILQ_INSERT_HEAD(&rack->r_ctl.rc_tmap, rsm, r_tnext);
		} else {
			TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, trsm, rsm, r_tnext);
		}
		rsm->r_in_tmap = 1;
		trsm = rsm;
		rsm->r_flags &= ~(RACK_ACKED | RACK_SACK_PASSED | RACK_WAS_SACKPASS | RACK_RWND_COLLAPSED);
	}
	sack_filter_clear(&rack->r_ctl.rack_sf, rack->rc_tp->snd_una);
}

static void
rack_do_detection(struct tcpcb *tp, struct tcp_rack *rack,  uint32_t bytes_this_ack, uint32_t segsiz)
{
	int do_detection = 0;

	if (rack->sack_attack_disable || rack->rc_suspicious) {
		/*
		 * If we have been disabled we must detect
		 * to possibly reverse it. Or if the guy has
		 * sent in suspicious sacks we want to do detection too.
		 */
		do_detection = 1;

	} else if  ((rack->do_detection || tcp_force_detection) &&
		    (tcp_sack_to_ack_thresh > 0) &&
		    (tcp_sack_to_move_thresh > 0) &&
		    (rack->r_ctl.rc_num_maps_alloced > tcp_map_minimum)) {
		/*
		 * We only detect here if:
		 * 1) System wide forcing is on <or> do_detection is on
		 *   <and>
		 * 2) We have thresholds for move and ack (set one to 0 and we are off)
		 *   <and>
		 * 3) We have maps allocated larger than our min (500).
		 */
		do_detection = 1;
	}
	if (do_detection > 0) {
		/*
		 * We have thresholds set to find
		 * possible attackers and disable sack.
		 * Check them.
		 */
		uint64_t ackratio, moveratio, movetotal;

		/* Log detecting */
		rack_log_sad(rack, 1);
		/* Do we establish a ack ratio */
		if ((rack->r_ctl.sack_count > tcp_map_minimum)  ||
		    (rack->rc_suspicious == 1) ||
		    (rack->sack_attack_disable > 0)) {
			ackratio = (uint64_t)(rack->r_ctl.sack_count);
			ackratio *= (uint64_t)(1000);
			if (rack->r_ctl.ack_count)
				ackratio /= (uint64_t)(rack->r_ctl.ack_count);
			else {
				/* We can hit this due to ack totals degregation (via small sacks) */
				ackratio = 1000;
			}
		} else {
			/*
			 * No ack ratio needed if we have not
			 * seen more sacks then the number of map entries.
			 * The exception to that is if we have disabled sack then
			 * we need to find a ratio.
			 */
			ackratio = 0;
		}

		if ((rack->sack_attack_disable == 0) &&
		    (ackratio > rack_highest_sack_thresh_seen))
			rack_highest_sack_thresh_seen = (uint32_t)ackratio;
		/* Do we establish a move ratio? */
		if ((rack->r_ctl.sack_moved_extra > tcp_map_minimum) ||
		    (rack->rc_suspicious == 1) ||
		    (rack->sack_attack_disable > 0)) {
			/*
			 * We need to have more sack moves than maps
			 * allocated to have a move ratio considered.
			 */
			movetotal = rack->r_ctl.sack_moved_extra;
			movetotal += rack->r_ctl.sack_noextra_move;
			moveratio = rack->r_ctl.sack_moved_extra;
			moveratio *= (uint64_t)1000;
			if (movetotal)
				moveratio /= movetotal;
			else {
				/* No moves, thats pretty good */
				moveratio = 0;
			}
		} else {
			/*
			 * Not enough moves have occured to consider
			 * if we are out of whack in that ratio.
			 * The exception to that is if we have disabled sack then
			 * we need to find a ratio.
			 */
			moveratio = 0;
		}
		if ((rack->sack_attack_disable == 0) &&
		    (moveratio > rack_highest_move_thresh_seen))
			rack_highest_move_thresh_seen = (uint32_t)moveratio;
		/* Now the tests */
		if (rack->sack_attack_disable == 0) {
			/* Not disabled, do we need to disable? */
			if ((ackratio > tcp_sack_to_ack_thresh) &&
			    (moveratio > tcp_sack_to_move_thresh)) {
				/* Disable sack processing */
				tcp_trace_point(rack->rc_tp, TCP_TP_SAD_TRIGGERED);
				rack->sack_attack_disable = 1;
				/* set it so we have the built in delay */
				rack->r_ctl.ack_during_sd = 1;
				if (rack_merge_out_sacks_on_attack)
					rack_merge_out_sacks(rack);
				counter_u64_add(rack_sack_attacks_detected, 1);
				tcp_trace_point(rack->rc_tp, TCP_TP_SAD_TRIGGERED);
				/* Clamp the cwnd at flight size */
				rack->r_ctl.rc_saved_cwnd = rack->rc_tp->snd_cwnd;
				rack->rc_tp->snd_cwnd = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
				rack_log_sad(rack, 2);
			}
		} else {
			/* We are sack-disabled check for false positives */
			if ((ackratio <= tcp_restoral_thresh) ||
			    ((rack_merge_out_sacks_on_attack == 0) &&
			     (rack->rc_suspicious == 0) &&
			     (rack->r_ctl.rc_num_maps_alloced <= (tcp_map_minimum/2)))) {
				rack->sack_attack_disable = 0;
				rack_log_sad(rack, 3);
				/* Restart counting */
				rack->r_ctl.sack_count = 0;
				rack->r_ctl.sack_moved_extra = 0;
				rack->r_ctl.sack_noextra_move = 1;
				rack->rc_suspicious = 0;
				rack->r_ctl.ack_count = max(1,
							    (bytes_this_ack / segsiz));

				counter_u64_add(rack_sack_attacks_reversed, 1);
				/* Restore the cwnd */
				if (rack->r_ctl.rc_saved_cwnd > rack->rc_tp->snd_cwnd)
					rack->rc_tp->snd_cwnd = rack->r_ctl.rc_saved_cwnd;
			}
		}
	}
}
#endif

static int
rack_note_dsack(struct tcp_rack *rack, tcp_seq start, tcp_seq end)
{

	uint32_t am, l_end;
	int was_tlp = 0;

	if (SEQ_GT(end, start))
		am = end - start;
	else
		am = 0;
	if ((rack->rc_last_tlp_acked_set ) &&
	    (SEQ_GEQ(start, rack->r_ctl.last_tlp_acked_start)) &&
	    (SEQ_LEQ(end, rack->r_ctl.last_tlp_acked_end))) {
		/*
		 * The DSACK is because of a TLP which we don't
		 * do anything with the reordering window over since
		 * it was not reordering that caused the DSACK but
		 * our previous retransmit TLP.
		 */
		rack_log_dsack_event(rack, 7, __LINE__, start, end);
		was_tlp = 1;
		goto skip_dsack_round;
	}
	if (rack->rc_last_sent_tlp_seq_valid) {
		l_end = rack->r_ctl.last_sent_tlp_seq + rack->r_ctl.last_sent_tlp_len;
		if (SEQ_GEQ(start, rack->r_ctl.last_sent_tlp_seq) &&
		    (SEQ_LEQ(end, l_end))) {
			/*
			 * This dsack is from the last sent TLP, ignore it
			 * for reordering purposes.
			 */
			rack_log_dsack_event(rack, 7, __LINE__, start, end);
			was_tlp = 1;
			goto skip_dsack_round;
		}
	}
	if (rack->rc_dsack_round_seen == 0) {
		rack->rc_dsack_round_seen = 1;
		rack->r_ctl.dsack_round_end = rack->rc_tp->snd_max;
		rack->r_ctl.num_dsack++;
		rack->r_ctl.dsack_persist = 16;	/* 16 is from the standard */
		rack_log_dsack_event(rack, 2, __LINE__, 0, 0);
	}
skip_dsack_round:
	/*
	 * We keep track of how many DSACK blocks we get
	 * after a recovery incident.
	 */
	rack->r_ctl.dsack_byte_cnt += am;
	if (!IN_FASTRECOVERY(rack->rc_tp->t_flags) &&
	    rack->r_ctl.retran_during_recovery &&
	    (rack->r_ctl.dsack_byte_cnt >= rack->r_ctl.retran_during_recovery)) {
		/*
		 * False recovery most likely culprit is reordering. If
		 * nothing else is missing we need to revert.
		 */
		rack->r_might_revert = 1;
		rack_handle_might_revert(rack->rc_tp, rack);
		rack->r_might_revert = 0;
		rack->r_ctl.retran_during_recovery = 0;
		rack->r_ctl.dsack_byte_cnt = 0;
	}
	return (was_tlp);
}

static uint32_t
do_rack_compute_pipe(struct tcpcb *tp, struct tcp_rack *rack, uint32_t snd_una)
{
	return (((tp->snd_max - snd_una) -
		 (rack->r_ctl.rc_sacked + rack->r_ctl.rc_considered_lost)) + rack->r_ctl.rc_holes_rxt);
}

static int32_t
rack_compute_pipe(struct tcpcb *tp)
{
	return ((int32_t)do_rack_compute_pipe(tp,
					      (struct tcp_rack *)tp->t_fb_ptr,
					      tp->snd_una));
}

static void
rack_update_prr(struct tcpcb *tp, struct tcp_rack *rack, uint32_t changed, tcp_seq th_ack)
{
	/* Deal with changed and PRR here (in recovery only) */
	uint32_t pipe, snd_una;

	rack->r_ctl.rc_prr_delivered += changed;

	if (sbavail(&rack->rc_inp->inp_socket->so_snd) <= (tp->snd_max - tp->snd_una)) {
		/*
		 * It is all outstanding, we are application limited
		 * and thus we don't need more room to send anything.
		 * Note we use tp->snd_una here and not th_ack because
		 * the data as yet not been cut from the sb.
		 */
		rack->r_ctl.rc_prr_sndcnt = 0;
		return;
	}
	/* Compute prr_sndcnt */
	if (SEQ_GT(tp->snd_una, th_ack)) {
		snd_una = tp->snd_una;
	} else {
		snd_una = th_ack;
	}
	pipe = do_rack_compute_pipe(tp, rack, snd_una);
	if (pipe > tp->snd_ssthresh) {
		long sndcnt;

		sndcnt = rack->r_ctl.rc_prr_delivered * tp->snd_ssthresh;
		if (rack->r_ctl.rc_prr_recovery_fs > 0)
			sndcnt /= (long)rack->r_ctl.rc_prr_recovery_fs;
		else {
			rack->r_ctl.rc_prr_sndcnt = 0;
			rack_log_to_prr(rack, 9, 0, __LINE__);
			sndcnt = 0;
		}
		sndcnt++;
		if (sndcnt > (long)rack->r_ctl.rc_prr_out)
			sndcnt -= rack->r_ctl.rc_prr_out;
		else
			sndcnt = 0;
		rack->r_ctl.rc_prr_sndcnt = sndcnt;
		rack_log_to_prr(rack, 10, 0, __LINE__);
	} else {
		uint32_t limit;

		if (rack->r_ctl.rc_prr_delivered > rack->r_ctl.rc_prr_out)
			limit = (rack->r_ctl.rc_prr_delivered - rack->r_ctl.rc_prr_out);
		else
			limit = 0;
		if (changed > limit)
			limit = changed;
		limit += ctf_fixed_maxseg(tp);
		if (tp->snd_ssthresh > pipe) {
			rack->r_ctl.rc_prr_sndcnt = min((tp->snd_ssthresh - pipe), limit);
			rack_log_to_prr(rack, 11, 0, __LINE__);
		} else {
			rack->r_ctl.rc_prr_sndcnt = min(0, limit);
			rack_log_to_prr(rack, 12, 0, __LINE__);
		}
	}
}

static void
rack_log_ack(struct tcpcb *tp, struct tcpopt *to, struct tcphdr *th, int entered_recovery, int dup_ack_struck,
	     int *dsack_seen, int *sacks_seen)
{
	uint32_t changed;
	struct tcp_rack *rack;
	struct rack_sendmap *rsm;
	struct sackblk sack, sack_blocks[TCP_MAX_SACK + 1];
	register uint32_t th_ack;
	int32_t i, j, k, num_sack_blks = 0;
	uint32_t cts, acked, ack_point;
	int loop_start = 0, moved_two = 0, no_extra = 0;
	uint32_t tsused;
	uint32_t segsiz, o_cnt;


	INP_WLOCK_ASSERT(tptoinpcb(tp));
	if (tcp_get_flags(th) & TH_RST) {
		/* We don't log resets */
		return;
	}
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	cts = tcp_get_usecs(NULL);
	rsm = tqhash_min(rack->r_ctl.tqh);
	changed = 0;
	th_ack = th->th_ack;
	if (rack->sack_attack_disable == 0)
		rack_do_decay(rack);
	segsiz = ctf_fixed_maxseg(rack->rc_tp);
	if (BYTES_THIS_ACK(tp, th) >=  segsiz) {
		/*
		 * You only get credit for
		 * MSS and greater (and you get extra
		 * credit for larger cum-ack moves).
		 */
		int ac;

		ac = BYTES_THIS_ACK(tp, th) / ctf_fixed_maxseg(rack->rc_tp);
		rack->r_ctl.ack_count += ac;
		counter_u64_add(rack_ack_total, ac);
	}
	if (rack->r_ctl.ack_count > 0xfff00000) {
		/*
		 * reduce the number to keep us under
		 * a uint32_t.
		 */
		rack->r_ctl.ack_count /= 2;
		rack->r_ctl.sack_count /= 2;
	}
	if (SEQ_GT(th_ack, tp->snd_una)) {
		rack_log_progress_event(rack, tp, ticks, PROGRESS_UPDATE, __LINE__);
		tp->t_acktime = ticks;
	}
	if (rsm && SEQ_GT(th_ack, rsm->r_start))
		changed = th_ack - rsm->r_start;
	if (changed) {
		rack_process_to_cumack(tp, rack, th_ack, cts, to,
				       tcp_tv_to_lusectick(&rack->r_ctl.act_rcv_time));
	}
	if ((to->to_flags & TOF_SACK) == 0) {
		/* We are done nothing left and no sack. */
		rack_handle_might_revert(tp, rack);
		/*
		 * For cases where we struck a dup-ack
		 * with no SACK, add to the changes so
		 * PRR will work right.
		 */
		if (dup_ack_struck && (changed == 0)) {
			changed += ctf_fixed_maxseg(rack->rc_tp);
		}
		goto out;
	}
	/* Sack block processing */
	if (SEQ_GT(th_ack, tp->snd_una))
		ack_point = th_ack;
	else
		ack_point = tp->snd_una;
	for (i = 0; i < to->to_nsacks; i++) {
		bcopy((to->to_sacks + i * TCPOLEN_SACK),
		      &sack, sizeof(sack));
		sack.start = ntohl(sack.start);
		sack.end = ntohl(sack.end);
		if (SEQ_GT(sack.end, sack.start) &&
		    SEQ_GT(sack.start, ack_point) &&
		    SEQ_LT(sack.start, tp->snd_max) &&
		    SEQ_GT(sack.end, ack_point) &&
		    SEQ_LEQ(sack.end, tp->snd_max)) {
			sack_blocks[num_sack_blks] = sack;
			num_sack_blks++;
		} else if (SEQ_LEQ(sack.start, th_ack) &&
			   SEQ_LEQ(sack.end, th_ack)) {
			int was_tlp;

			if (dsack_seen != NULL)
				*dsack_seen = 1;
			was_tlp = rack_note_dsack(rack, sack.start, sack.end);
			/*
			 * Its a D-SACK block.
			 */
			tcp_record_dsack(tp, sack.start, sack.end, was_tlp);
		}
	}
	if (rack->rc_dsack_round_seen) {
		/* Is the dsack roound over? */
		if (SEQ_GEQ(th_ack, rack->r_ctl.dsack_round_end)) {
			/* Yes it is */
			rack->rc_dsack_round_seen = 0;
			rack_log_dsack_event(rack, 3, __LINE__, 0, 0);
		}
	}
	/*
	 * Sort the SACK blocks so we can update the rack scoreboard with
	 * just one pass.
	 */
	o_cnt = num_sack_blks;
	num_sack_blks = sack_filter_blks(&rack->r_ctl.rack_sf, sack_blocks,
					 num_sack_blks, th->th_ack);
	ctf_log_sack_filter(rack->rc_tp, num_sack_blks, sack_blocks);
	if (sacks_seen != NULL)
		*sacks_seen = num_sack_blks;
	if (num_sack_blks == 0) {
		/* Nothing to sack, but we need to update counts */
		if ((o_cnt == 1) &&
		    (*dsack_seen != 1))
			rack->r_ctl.sack_count++;
		else if (o_cnt > 1)
			rack->r_ctl.sack_count++;
		goto out_with_totals;
	}
	if (rack->sack_attack_disable) {
		/*
		 * An attacker disablement is in place, for
		 * every sack block that is not at least a full MSS
		 * count up sack_count.
		 */
		for (i = 0; i < num_sack_blks; i++) {
			if ((sack_blocks[i].end - sack_blocks[i].start) < segsiz) {
				rack->r_ctl.sack_count++;
			}
			if (rack->r_ctl.sack_count > 0xfff00000) {
				/*
				 * reduce the number to keep us under
				 * a uint32_t.
				 */
				rack->r_ctl.ack_count /= 2;
				rack->r_ctl.sack_count /= 2;
			}
		}
		goto out;
	}
	/* Its a sack of some sort */
	rack->r_ctl.sack_count += num_sack_blks;
	if (rack->r_ctl.sack_count > 0xfff00000) {
		/*
		 * reduce the number to keep us under
		 * a uint32_t.
		 */
		rack->r_ctl.ack_count /= 2;
		rack->r_ctl.sack_count /= 2;
	}
	if (num_sack_blks < 2) {
		/* Only one, we don't need to sort */
		goto do_sack_work;
	}
	/* Sort the sacks */
	for (i = 0; i < num_sack_blks; i++) {
		for (j = i + 1; j < num_sack_blks; j++) {
			if (SEQ_GT(sack_blocks[i].end, sack_blocks[j].end)) {
				sack = sack_blocks[i];
				sack_blocks[i] = sack_blocks[j];
				sack_blocks[j] = sack;
			}
		}
	}
	/*
	 * Now are any of the sack block ends the same (yes some
	 * implementations send these)?
	 */
again:
	if (num_sack_blks == 0)
		goto out_with_totals;
	if (num_sack_blks > 1) {
		for (i = 0; i < num_sack_blks; i++) {
			for (j = i + 1; j < num_sack_blks; j++) {
				if (sack_blocks[i].end == sack_blocks[j].end) {
					/*
					 * Ok these two have the same end we
					 * want the smallest end and then
					 * throw away the larger and start
					 * again.
					 */
					if (SEQ_LT(sack_blocks[j].start, sack_blocks[i].start)) {
						/*
						 * The second block covers
						 * more area use that
						 */
						sack_blocks[i].start = sack_blocks[j].start;
					}
					/*
					 * Now collapse out the dup-sack and
					 * lower the count
					 */
					for (k = (j + 1); k < num_sack_blks; k++) {
						sack_blocks[j].start = sack_blocks[k].start;
						sack_blocks[j].end = sack_blocks[k].end;
						j++;
					}
					num_sack_blks--;
					goto again;
				}
			}
		}
	}
do_sack_work:
	/*
	 * First lets look to see if
	 * we have retransmitted and
	 * can use the transmit next?
	 */
	rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
	if (rsm &&
	    SEQ_GT(sack_blocks[0].end, rsm->r_start) &&
	    SEQ_LT(sack_blocks[0].start, rsm->r_end)) {
		/*
		 * We probably did the FR and the next
		 * SACK in continues as we would expect.
		 */
		acked = rack_proc_sack_blk(tp, rack, &sack_blocks[0], to, &rsm, cts, &no_extra, &moved_two, segsiz);
		if (acked) {
			rack->r_wanted_output = 1;
			changed += acked;
		}
		if (num_sack_blks == 1) {
			/*
			 * This is what we would expect from
			 * a normal implementation to happen
			 * after we have retransmitted the FR,
			 * i.e the sack-filter pushes down
			 * to 1 block and the next to be retransmitted
			 * is the sequence in the sack block (has more
			 * are acked). Count this as ACK'd data to boost
			 * up the chances of recovering any false positives.
			 */
			rack->r_ctl.ack_count += (acked / ctf_fixed_maxseg(rack->rc_tp));
			counter_u64_add(rack_ack_total, (acked / ctf_fixed_maxseg(rack->rc_tp)));
			counter_u64_add(rack_express_sack, 1);
			if (rack->r_ctl.ack_count > 0xfff00000) {
				/*
				 * reduce the number to keep us under
				 * a uint32_t.
				 */
				rack->r_ctl.ack_count /= 2;
				rack->r_ctl.sack_count /= 2;
			}
			if (moved_two) {
				/*
				 * If we did not get a SACK for at least a MSS and
				 * had to move at all, or if we moved more than our
				 * threshold, it counts against the "extra" move.
				 */
				rack->r_ctl.sack_moved_extra += moved_two;
				rack->r_ctl.sack_noextra_move += no_extra;
				counter_u64_add(rack_move_some, 1);
			} else {
				/*
				 * else we did not have to move
				 * any more than we would expect.
				 */
				rack->r_ctl.sack_noextra_move += no_extra;
				rack->r_ctl.sack_noextra_move++;
				counter_u64_add(rack_move_none, 1);
			}
			if ((rack->r_ctl.sack_moved_extra > 0xfff00000) ||
			    (rack->r_ctl.sack_noextra_move > 0xfff00000)) {
				rack->r_ctl.sack_moved_extra /= 2;
				rack->r_ctl.sack_noextra_move /= 2;
			}
			goto out_with_totals;
		} else {
			/*
			 * Start the loop through the
			 * rest of blocks, past the first block.
			 */
			loop_start = 1;
		}
	}
	counter_u64_add(rack_sack_total, 1);
	rsm = rack->r_ctl.rc_sacklast;
	for (i = loop_start; i < num_sack_blks; i++) {
		acked = rack_proc_sack_blk(tp, rack, &sack_blocks[i], to, &rsm, cts, &no_extra, &moved_two, segsiz);
		if (acked) {
			rack->r_wanted_output = 1;
			changed += acked;
		}
		if (moved_two) {
			/*
			 * If we did not get a SACK for at least a MSS and
			 * had to move at all, or if we moved more than our
			 * threshold, it counts against the "extra" move.
			 */
			rack->r_ctl.sack_moved_extra += moved_two;
			rack->r_ctl.sack_noextra_move += no_extra;
			counter_u64_add(rack_move_some, 1);
		} else {
			/*
			 * else we did not have to move
			 * any more than we would expect.
			 */
			rack->r_ctl.sack_noextra_move += no_extra;
			rack->r_ctl.sack_noextra_move++;
			counter_u64_add(rack_move_none, 1);
		}
		if ((rack->r_ctl.sack_moved_extra > 0xfff00000) ||
		    (rack->r_ctl.sack_noextra_move > 0xfff00000)) {
			rack->r_ctl.sack_moved_extra /= 2;
			rack->r_ctl.sack_noextra_move /= 2;
		}
		if (moved_two && (acked < ctf_fixed_maxseg(rack->rc_tp))) {
			/*
			 * If the SACK was not a full MSS then
			 * we add to sack_count the number of
			 * MSS's (or possibly more than
			 * a MSS if its a TSO send) we had to skip by.
			 */
			rack->r_ctl.sack_count += moved_two;
			if (rack->r_ctl.sack_count > 0xfff00000) {
				rack->r_ctl.ack_count /= 2;
				rack->r_ctl.sack_count /= 2;
			}
			counter_u64_add(rack_sack_total, moved_two);
		}
		/*
		 * Now we need to setup for the next
		 * round. First we make sure we won't
		 * exceed the size of our uint32_t on
		 * the various counts, and then clear out
		 * moved_two.
		 */
		moved_two = 0;
		no_extra = 0;
	}
out_with_totals:
	if (num_sack_blks > 1) {
		/*
		 * You get an extra stroke if
		 * you have more than one sack-blk, this
		 * could be where we are skipping forward
		 * and the sack-filter is still working, or
		 * it could be an attacker constantly
		 * moving us.
		 */
		rack->r_ctl.sack_moved_extra++;
		counter_u64_add(rack_move_some, 1);
	}
out:
#ifdef TCP_SAD_DETECTION
	rack_do_detection(tp, rack, BYTES_THIS_ACK(tp, th), ctf_fixed_maxseg(rack->rc_tp));
#endif
	if (changed) {
		/* Something changed cancel the rack timer */
		rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
	}
	tsused = tcp_get_usecs(NULL);
	rsm = tcp_rack_output(tp, rack, tsused);
	if ((!IN_FASTRECOVERY(tp->t_flags)) &&
	    rsm &&
	    ((rsm->r_flags & RACK_MUST_RXT) == 0)) {
		/* Enter recovery */
		entered_recovery = 1;
		rack_cong_signal(tp, CC_NDUPACK, th_ack, __LINE__);
		/*
		 * When we enter recovery we need to assure we send
		 * one packet.
		 */
		if (rack->rack_no_prr == 0) {
			rack->r_ctl.rc_prr_sndcnt = ctf_fixed_maxseg(tp);
			rack_log_to_prr(rack, 8, 0, __LINE__);
		}
		rack->r_timer_override = 1;
		rack->r_early = 0;
		rack->r_ctl.rc_agg_early = 0;
	} else if (IN_FASTRECOVERY(tp->t_flags) &&
		   rsm &&
		   (rack->r_rr_config == 3)) {
		/*
		 * Assure we can output and we get no
		 * remembered pace time except the retransmit.
		 */
		rack->r_timer_override = 1;
		rack->r_ctl.rc_hpts_flags &= ~PACE_PKT_OUTPUT;
		rack->r_ctl.rc_resend = rsm;
	}
	if (IN_FASTRECOVERY(tp->t_flags) &&
	    (rack->rack_no_prr == 0) &&
	    (entered_recovery == 0)) {
		rack_update_prr(tp, rack, changed, th_ack);
		if ((rsm && (rack->r_ctl.rc_prr_sndcnt >= ctf_fixed_maxseg(tp)) &&
		     ((tcp_in_hpts(rack->rc_tp) == 0) &&
		      ((rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) == 0)))) {
			/*
			 * If you are pacing output you don't want
			 * to override.
			 */
			rack->r_early = 0;
			rack->r_ctl.rc_agg_early = 0;
			rack->r_timer_override = 1;
		}
	}
}

static void
rack_strike_dupack(struct tcp_rack *rack, tcp_seq th_ack)
{
	struct rack_sendmap *rsm;

	rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
	while (rsm) {
		/*
		 * We need to skip anything already set
		 * to be retransmitted.
		 */
		if ((rsm->r_dupack >= DUP_ACK_THRESHOLD)  ||
		    (rsm->r_flags & RACK_MUST_RXT)) {
			rsm = TAILQ_NEXT(rsm, r_tnext);
			continue;
		}
		break;
	}
	if (rsm && (rsm->r_dupack < 0xff)) {
		rsm->r_dupack++;
		if (rsm->r_dupack >= DUP_ACK_THRESHOLD) {
			struct timeval tv;
			uint32_t cts;
			/*
			 * Here we see if we need to retransmit. For
			 * a SACK type connection if enough time has passed
			 * we will get a return of the rsm. For a non-sack
			 * connection we will get the rsm returned if the
			 * dupack value is 3 or more.
			 */
			cts = tcp_get_usecs(&tv);
			rack->r_ctl.rc_resend = tcp_rack_output(rack->rc_tp, rack, cts);
			if (rack->r_ctl.rc_resend != NULL) {
				if (!IN_FASTRECOVERY(rack->rc_tp->t_flags)) {
					rack_cong_signal(rack->rc_tp, CC_NDUPACK,
							 th_ack,  __LINE__);
				}
				rack->r_wanted_output = 1;
				rack->r_timer_override = 1;
				rack_log_retran_reason(rack, rsm, __LINE__, 1, 3);
			}
		} else {
			rack_log_retran_reason(rack, rsm, __LINE__, 0, 3);
		}
	}
}

static void
rack_check_bottom_drag(struct tcpcb *tp,
		       struct tcp_rack *rack,
		       struct socket *so)
{
	/*
	 * So what is dragging bottom?
	 *
	 * Dragging bottom means you were under pacing and had a
	 * delay in processing inbound acks waiting on our pacing
	 * timer to expire. While you were waiting all of the acknowledgments
	 * for the packets you sent have arrived. This means we are pacing
	 * way underneath the bottleneck to the point where our Goodput
	 * measurements stop working, since they require more than one
	 * ack (usually at least 8 packets worth with multiple acks so we can
	 * gauge the inter-ack times). If that occurs we have a real problem
	 * since we are stuck in a hole that we can't get out of without
	 * something speeding us up.
	 *
	 * We also check to see if we are widdling down to just one segment
	 * outstanding. If this occurs and we have room to send in our cwnd/rwnd
	 * then we are adding the delayed ack interval into our measurments and
	 * we need to speed up slightly.
	 */
	uint32_t segsiz, minseg;

	segsiz = ctf_fixed_maxseg(tp);
	minseg = segsiz;
	if (tp->snd_max == tp->snd_una) {
		/*
		 * We are doing dynamic pacing and we are way
		 * under. Basically everything got acked while
		 * we were still waiting on the pacer to expire.
		 *
		 * This means we need to boost the b/w in
		 * addition to any earlier boosting of
		 * the multiplier.
		 */
		uint64_t lt_bw;

		tcp_trace_point(rack->rc_tp, TCP_TP_PACED_BOTTOM);
		lt_bw = rack_get_lt_bw(rack);
		rack->rc_dragged_bottom = 1;
		rack_validate_multipliers_at_or_above100(rack);
		if ((rack->r_ctl.rack_rs.rs_flags & RACK_RTT_VALID) &&
		    (rack->dis_lt_bw == 0) &&
		    (rack->use_lesser_lt_bw == 0) &&
		    (lt_bw > 0)) {
			/*
			 * Lets use the long-term b/w we have
			 * been getting as a base.
			 */
			if (rack->rc_gp_filled == 0) {
				if (lt_bw > ONE_POINT_TWO_MEG) {
					/*
					 * If we have no measurement
					 * don't let us set in more than
					 * 1.2Mbps. If we are still too
					 * low after pacing with this we
					 * will hopefully have a max b/w
					 * available to sanity check things.
					 */
					lt_bw = ONE_POINT_TWO_MEG;
				}
				rack->r_ctl.rc_rtt_diff = 0;
				rack->r_ctl.gp_bw = lt_bw;
				rack->rc_gp_filled = 1;
				if (rack->r_ctl.num_measurements < RACK_REQ_AVG)
					rack->r_ctl.num_measurements = RACK_REQ_AVG;
				rack_set_pace_segments(rack->rc_tp, rack, __LINE__, NULL);
			} else if (lt_bw > rack->r_ctl.gp_bw) {
				rack->r_ctl.rc_rtt_diff = 0;
				if (rack->r_ctl.num_measurements < RACK_REQ_AVG)
					rack->r_ctl.num_measurements = RACK_REQ_AVG;
				rack->r_ctl.gp_bw = lt_bw;
				rack_set_pace_segments(rack->rc_tp, rack, __LINE__, NULL);
			} else
				rack_increase_bw_mul(rack, -1, 0, 0, 1);
			if ((rack->gp_ready == 0) &&
			    (rack->r_ctl.num_measurements >= rack->r_ctl.req_measurements)) {
				/* We have enough measurements now */
				rack->gp_ready = 1;
				if (rack->dgp_on ||
				    rack->rack_hibeta)
					rack_set_cc_pacing(rack);
				if (rack->defer_options)
					rack_apply_deferred_options(rack);
			}
		} else {
			/*
			 * zero rtt possibly?, settle for just an old increase.
			 */
			rack_increase_bw_mul(rack, -1, 0, 0, 1);
		}
	} else if ((IN_FASTRECOVERY(tp->t_flags) == 0) &&
		   (sbavail(&so->so_snd) > max((segsiz * (4 + rack_req_segs)),
					       minseg)) &&
		   (rack->r_ctl.cwnd_to_use > max((segsiz * (rack_req_segs + 2)), minseg)) &&
		   (tp->snd_wnd > max((segsiz * (rack_req_segs + 2)), minseg)) &&
		   (ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked) <=
		    (segsiz * rack_req_segs))) {
		/*
		 * We are doing dynamic GP pacing and
		 * we have everything except 1MSS or less
		 * bytes left out. We are still pacing away.
		 * And there is data that could be sent, This
		 * means we are inserting delayed ack time in
		 * our measurements because we are pacing too slow.
		 */
		rack_validate_multipliers_at_or_above100(rack);
		rack->rc_dragged_bottom = 1;
		rack_increase_bw_mul(rack, -1, 0, 0, 1);
	}
}

#ifdef TCP_REQUEST_TRK
static void
rack_log_hybrid(struct tcp_rack *rack, uint32_t seq,
		struct tcp_sendfile_track *cur, uint8_t mod, int line, int err)
{
	int do_log;

	do_log = tcp_bblogging_on(rack->rc_tp);
	if (do_log == 0) {
		if ((do_log = tcp_bblogging_point_on(rack->rc_tp, TCP_BBPOINT_REQ_LEVEL_LOGGING) )== 0)
			return;
		/* We only allow the three below with point logging on */
		if ((mod != HYBRID_LOG_RULES_APP) &&
		    (mod != HYBRID_LOG_RULES_SET) &&
		    (mod != HYBRID_LOG_REQ_COMP))
			return;

	}
	if (do_log) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		/* Convert our ms to a microsecond */
		memset(&log, 0, sizeof(log));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex1 = seq;
		log.u_bbr.cwnd_gain = line;
		if (cur != NULL) {
			uint64_t off;

			log.u_bbr.flex2 = cur->start_seq;
			log.u_bbr.flex3 = cur->end_seq;
			log.u_bbr.flex4 = (uint32_t)((cur->localtime >> 32) & 0x00000000ffffffff);
			log.u_bbr.flex5 = (uint32_t)(cur->localtime & 0x00000000ffffffff);
			log.u_bbr.flex6 = cur->flags;
			log.u_bbr.pkts_out = cur->hybrid_flags;
			log.u_bbr.rttProp = cur->timestamp;
			log.u_bbr.cur_del_rate = cur->cspr;
			log.u_bbr.bw_inuse = cur->start;
			log.u_bbr.applimited = (uint32_t)(cur->end & 0x00000000ffffffff);
			log.u_bbr.delivered = (uint32_t)((cur->end >> 32) & 0x00000000ffffffff) ;
			log.u_bbr.epoch = (uint32_t)(cur->deadline & 0x00000000ffffffff);
			log.u_bbr.lt_epoch = (uint32_t)((cur->deadline >> 32) & 0x00000000ffffffff) ;
			log.u_bbr.inhpts = 1;
#ifdef TCP_REQUEST_TRK
			off = (uint64_t)(cur) - (uint64_t)(&rack->rc_tp->t_tcpreq_info[0]);
			log.u_bbr.use_lt_bw = (uint8_t)(off / sizeof(struct tcp_sendfile_track));
#endif
		} else {
			log.u_bbr.flex2 = err;
		}
		/*
		 * Fill in flex7 to be CHD (catchup|hybrid|DGP)
		 */
		log.u_bbr.flex7 = rack->rc_catch_up;
		log.u_bbr.flex7 <<= 1;
		log.u_bbr.flex7 |= rack->rc_hybrid_mode;
		log.u_bbr.flex7 <<= 1;
		log.u_bbr.flex7 |= rack->dgp_on;
		/*
		 * Compose bbr_state to be a bit wise 0000ADHF
		 * where A is the always_pace flag
		 * where D is the dgp_on flag
		 * where H is the hybrid_mode on flag
		 * where F is the use_fixed_rate flag.
		 */
		log.u_bbr.bbr_state = rack->rc_always_pace;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->dgp_on;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->rc_hybrid_mode;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->use_fixed_rate;
		log.u_bbr.flex8 = mod;
		log.u_bbr.delRate = rack->r_ctl.bw_rate_cap;
		log.u_bbr.bbr_substate = rack->r_ctl.client_suggested_maxseg;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkt_epoch = rack->rc_tp->tcp_hybrid_start;
		log.u_bbr.lost = rack->rc_tp->tcp_hybrid_error;
		log.u_bbr.pacing_gain = (uint16_t)rack->rc_tp->tcp_hybrid_stop;
		tcp_log_event(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_HYBRID_PACING_LOG, 0,
	            0, &log, false, NULL, __func__, __LINE__, &tv);
	}
}
#endif

#ifdef TCP_REQUEST_TRK
static void
rack_set_dgp_hybrid_mode(struct tcp_rack *rack, tcp_seq seq, uint32_t len, uint64_t cts)
{
	struct tcp_sendfile_track *rc_cur, *orig_ent;
	struct tcpcb *tp;
	int err = 0;

	orig_ent = rack->r_ctl.rc_last_sft;
	rc_cur = tcp_req_find_req_for_seq(rack->rc_tp, seq);
	if (rc_cur == NULL) {
		/* If not in the beginning what about the end piece */
		if (rack->rc_hybrid_mode)
			rack_log_hybrid(rack, seq, NULL, HYBRID_LOG_NO_RANGE, __LINE__, err);
		rc_cur = tcp_req_find_req_for_seq(rack->rc_tp, (seq + len - 1));
	} else {
		err = 12345;
	}
	/* If we find no parameters we are in straight DGP mode */
	if(rc_cur == NULL) {
		/* None found for this seq, just DGP for now */
		if (rack->rc_hybrid_mode) {
			rack->r_ctl.client_suggested_maxseg = 0;
			rack->rc_catch_up = 0;
			if (rack->cspr_is_fcc == 0)
				rack->r_ctl.bw_rate_cap = 0;
			else
				rack->r_ctl.fillcw_cap = rack_fillcw_bw_cap;
		}
		if (rack->rc_hybrid_mode) {
			rack_log_hybrid(rack, (seq + len - 1), NULL, HYBRID_LOG_NO_RANGE, __LINE__, err);
		}
		if (rack->r_ctl.rc_last_sft) {
			rack->r_ctl.rc_last_sft = NULL;
		}
		return;
	}
	if ((rc_cur->hybrid_flags & TCP_HYBRID_PACING_WASSET) == 0) {
		/* This entry was never setup for hybrid pacing on/off etc */
		if (rack->rc_hybrid_mode) {
			rack->r_ctl.client_suggested_maxseg = 0;
			rack->rc_catch_up = 0;
			rack->r_ctl.bw_rate_cap = 0;
		}
		if (rack->r_ctl.rc_last_sft) {
			rack->r_ctl.rc_last_sft = NULL;
		}
		if ((rc_cur->flags & TCP_TRK_TRACK_FLG_FSND) == 0) {
			rc_cur->flags |= TCP_TRK_TRACK_FLG_FSND;
			rc_cur->first_send = cts;
			rc_cur->sent_at_fs = rack->rc_tp->t_sndbytes;
			rc_cur->rxt_at_fs = rack->rc_tp->t_snd_rxt_bytes;
		}
		return;
	}
	/*
	 * Ok if we have a new entry *or* have never
	 * set up an entry we need to proceed. If
	 * we have already set it up this entry we
	 * just continue along with what we already
	 * setup.
	 */
	tp = rack->rc_tp;
	if ((rack->r_ctl.rc_last_sft != NULL) &&
	    (rack->r_ctl.rc_last_sft == rc_cur)) {
		/* Its already in place */
		if (rack->rc_hybrid_mode)
			rack_log_hybrid(rack, seq, rc_cur, HYBRID_LOG_ISSAME, __LINE__, 0);
		return;
	}
	if (rack->rc_hybrid_mode == 0) {
		rack->r_ctl.rc_last_sft = rc_cur;
		if (orig_ent) {
			orig_ent->sent_at_ls = rack->rc_tp->t_sndbytes;
			orig_ent->rxt_at_ls = rack->rc_tp->t_snd_rxt_bytes;
			orig_ent->flags |= TCP_TRK_TRACK_FLG_LSND;
		}
		rack_log_hybrid(rack, seq, rc_cur, HYBRID_LOG_RULES_APP, __LINE__, 0);
		return;
	}
	if ((rc_cur->hybrid_flags & TCP_HYBRID_PACING_CSPR) && rc_cur->cspr){
		/* Compensate for all the header overhead's */
		if (rack->cspr_is_fcc == 0)
			rack->r_ctl.bw_rate_cap	= rack_compensate_for_linerate(rack, rc_cur->cspr);
		else
			rack->r_ctl.fillcw_cap =  rack_compensate_for_linerate(rack, rc_cur->cspr);
	} else {
		if (rack->rc_hybrid_mode) {
			if (rack->cspr_is_fcc == 0)
				rack->r_ctl.bw_rate_cap = 0;
			else
				rack->r_ctl.fillcw_cap = rack_fillcw_bw_cap;
		}
	}
	if (rc_cur->hybrid_flags & TCP_HYBRID_PACING_H_MS)
		rack->r_ctl.client_suggested_maxseg = rc_cur->hint_maxseg;
	else
		rack->r_ctl.client_suggested_maxseg = 0;
	if (rc_cur->timestamp == rack->r_ctl.last_tm_mark) {
		/*
		 * It is the same timestamp as the previous one
		 * add the hybrid flag that will indicate we use
		 * sendtime not arrival time for catch-up mode.
		 */
		rc_cur->hybrid_flags |= TCP_HYBRID_PACING_SENDTIME;
	}
	if ((rc_cur->hybrid_flags & TCP_HYBRID_PACING_CU) &&
	    (rc_cur->cspr > 0)) {
		uint64_t len;

		rack->rc_catch_up = 1;
		/*
		 * Calculate the deadline time, first set the
		 * time to when the request arrived.
		 */
		if (rc_cur->hybrid_flags & TCP_HYBRID_PACING_SENDTIME) {
			/*
			 * For cases where its a duplicate tm (we received more
			 * than one request for a tm) we want to use now, the point
			 * where we are just sending the first bit of the request.
			 */
			rc_cur->deadline = cts;
		} else {
			/*
			 * Here we have a different tm from the last request
			 * so we want to use arrival time as our base.
			 */
			rc_cur->deadline = rc_cur->localtime;
		}
		/*
		 * Next calculate the length and compensate for
		 * TLS if need be.
		 */
		len = rc_cur->end - rc_cur->start;
		if (tp->t_inpcb.inp_socket->so_snd.sb_tls_info) {
			/*
			 * This session is doing TLS. Take a swag guess
			 * at the overhead.
			 */
			len += tcp_estimate_tls_overhead(tp->t_inpcb.inp_socket, len);
		}
		/*
		 * Now considering the size, and the cspr, what is the time that
		 * would be required at the cspr rate. Here we use the raw
		 * cspr value since the client only looks at the raw data. We
		 * do use len which includes TLS overhead, but not the TCP/IP etc.
		 * That will get made up for in the CU pacing rate set.
		 */
		len *= HPTS_USEC_IN_SEC;
		len /= rc_cur->cspr;
		rc_cur->deadline += len;
	} else {
		rack->rc_catch_up = 0;
		rc_cur->deadline = 0;
	}
	if (rack->r_ctl.client_suggested_maxseg != 0) {
		/*
		 * We need to reset the max pace segs if we have a
		 * client_suggested_maxseg.
		 */
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
	}
	if (orig_ent) {
		orig_ent->sent_at_ls = rack->rc_tp->t_sndbytes;
		orig_ent->rxt_at_ls = rack->rc_tp->t_snd_rxt_bytes;
		orig_ent->flags |= TCP_TRK_TRACK_FLG_LSND;
	}
	rack_log_hybrid(rack, seq, rc_cur, HYBRID_LOG_RULES_APP, __LINE__, 0);
	/* Remember it for next time and for CU mode */
	rack->r_ctl.rc_last_sft = rc_cur;
	rack->r_ctl.last_tm_mark = rc_cur->timestamp;
}
#endif

static void
rack_chk_req_and_hybrid_on_out(struct tcp_rack *rack, tcp_seq seq, uint32_t len, uint64_t cts)
{
#ifdef TCP_REQUEST_TRK
	struct tcp_sendfile_track *ent;

	ent = rack->r_ctl.rc_last_sft;
	if ((ent == NULL) ||
	    (ent->flags == TCP_TRK_TRACK_FLG_EMPTY) ||
	    (SEQ_GEQ(seq, ent->end_seq))) {
		/* Time to update the track. */
		rack_set_dgp_hybrid_mode(rack, seq, len, cts);
		ent = rack->r_ctl.rc_last_sft;
	}
	/* Out of all */
	if (ent == NULL) {
		return;
	}
	if (SEQ_LT(ent->end_seq, (seq + len))) {
		/*
		 * This is the case where our end_seq guess
		 * was wrong. This is usually due to TLS having
		 * more bytes then our guess. It could also be the
		 * case that the client sent in two requests closely
		 * and the SB is full of both so we are sending part
		 * of each (end|beg). In such a case lets move this
		 * guys end to match the end of this send. That
		 * way it will complete when all of it is acked.
		 */
		ent->end_seq = (seq + len);
		if (rack->rc_hybrid_mode)
			rack_log_hybrid_bw(rack, seq, len, 0, 0, HYBRID_LOG_EXTEND, 0, ent, __LINE__);
	}
	/* Now validate we have set the send time of this one */
	if ((ent->flags & TCP_TRK_TRACK_FLG_FSND) == 0) {
		ent->flags |= TCP_TRK_TRACK_FLG_FSND;
		ent->first_send = cts;
		ent->sent_at_fs = rack->rc_tp->t_sndbytes;
		ent->rxt_at_fs = rack->rc_tp->t_snd_rxt_bytes;
	}
#endif
}

static void
rack_gain_for_fastoutput(struct tcp_rack *rack, struct tcpcb *tp, struct socket *so, uint32_t acked_amount)
{
	/*
	 * The fast output path is enabled and we
	 * have moved the cumack forward. Lets see if
	 * we can expand forward the fast path length by
	 * that amount. What we would ideally like to
	 * do is increase the number of bytes in the
	 * fast path block (left_to_send) by the
	 * acked amount. However we have to gate that
	 * by two factors:
	 * 1) The amount outstanding and the rwnd of the peer
	 *    (i.e. we don't want to exceed the rwnd of the peer).
	 *    <and>
	 * 2) The amount of data left in the socket buffer (i.e.
	 *    we can't send beyond what is in the buffer).
	 *
	 * Note that this does not take into account any increase
	 * in the cwnd. We will only extend the fast path by
	 * what was acked.
	 */
	uint32_t new_total, gating_val;

	new_total = acked_amount + rack->r_ctl.fsb.left_to_send;
	gating_val = min((sbavail(&so->so_snd) - (tp->snd_max - tp->snd_una)),
			 (tp->snd_wnd - (tp->snd_max - tp->snd_una)));
	if (new_total <= gating_val) {
		/* We can increase left_to_send by the acked amount */
		counter_u64_add(rack_extended_rfo, 1);
		rack->r_ctl.fsb.left_to_send = new_total;
		KASSERT((rack->r_ctl.fsb.left_to_send <= (sbavail(&rack->rc_inp->inp_socket->so_snd) - (tp->snd_max - tp->snd_una))),
			("rack:%p left_to_send:%u sbavail:%u out:%u",
			 rack, rack->r_ctl.fsb.left_to_send,
			 sbavail(&rack->rc_inp->inp_socket->so_snd),
			 (tp->snd_max - tp->snd_una)));

	}
}

static void
rack_adjust_sendmap_head(struct tcp_rack *rack, struct sockbuf *sb)
{
	/*
	 * Here any sendmap entry that points to the
	 * beginning mbuf must be adjusted to the correct
	 * offset. This must be called with:
	 * 1) The socket buffer locked
	 * 2) snd_una adjusted to its new position.
	 *
	 * Note that (2) implies rack_ack_received has also
	 * been called and all the sbcut's have been done.
	 *
	 * We grab the first mbuf in the socket buffer and
	 * then go through the front of the sendmap, recalculating
	 * the stored offset for any sendmap entry that has
	 * that mbuf. We must use the sb functions to do this
	 * since its possible an add was done has well as
	 * the subtraction we may have just completed. This should
	 * not be a penalty though, since we just referenced the sb
	 * to go in and trim off the mbufs that we freed (of course
	 * there will be a penalty for the sendmap references though).
	 *
	 * Note also with INVARIANT on, we validate with a KASSERT
	 * that the first sendmap entry has a soff of 0.
	 *
	 */
	struct mbuf *m;
	struct rack_sendmap *rsm;
	tcp_seq snd_una;
#ifdef INVARIANTS
	int first_processed = 0;
#endif

	snd_una = rack->rc_tp->snd_una;
	SOCKBUF_LOCK_ASSERT(sb);
	m = sb->sb_mb;
	rsm = tqhash_min(rack->r_ctl.tqh);
	if ((rsm == NULL) || (m == NULL)) {
		/* Nothing outstanding */
		return;
	}
	/* The very first RSM's mbuf must point to the head mbuf in the sb */
	KASSERT((rsm->m == m),
		("Rack:%p sb:%p rsm:%p -- first rsm mbuf not aligned to sb",
		 rack, sb, rsm));
	while (rsm->m && (rsm->m == m)) {
		/* one to adjust */
#ifdef INVARIANTS
		struct mbuf *tm;
		uint32_t soff;

		tm = sbsndmbuf(sb, (rsm->r_start - snd_una), &soff);
		if ((rsm->orig_m_len != m->m_len) ||
		    (rsm->orig_t_space != M_TRAILINGROOM(m))){
			rack_adjust_orig_mlen(rsm);
		}
		if (first_processed == 0) {
			KASSERT((rsm->soff == 0),
				("Rack:%p rsm:%p -- rsm at head but soff not zero",
				 rack, rsm));
			first_processed = 1;
		}
		if ((rsm->soff != soff) || (rsm->m != tm)) {
			/*
			 * This is not a fatal error, we anticipate it
			 * might happen (the else code), so we count it here
			 * so that under invariant we can see that it really
			 * does happen.
			 */
			counter_u64_add(rack_adjust_map_bw, 1);
		}
		rsm->m = tm;
		rsm->soff = soff;
		if (tm) {
			rsm->orig_m_len = rsm->m->m_len;
			rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
		} else {
			rsm->orig_m_len = 0;
			rsm->orig_t_space = 0;
		}
#else
		rsm->m = sbsndmbuf(sb, (rsm->r_start - snd_una), &rsm->soff);
		if (rsm->m) {
			rsm->orig_m_len = rsm->m->m_len;
			rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
		} else {
			rsm->orig_m_len = 0;
			rsm->orig_t_space = 0;
		}
#endif
		rsm = tqhash_next(rack->r_ctl.tqh, rsm);
		if (rsm == NULL)
			break;
	}
}

#ifdef TCP_REQUEST_TRK
static inline void
rack_req_check_for_comp(struct tcp_rack *rack, tcp_seq th_ack)
{
	struct tcp_sendfile_track *ent;
	int i;

	if ((rack->rc_hybrid_mode == 0) &&
	    (tcp_bblogging_point_on(rack->rc_tp, TCP_BBPOINT_REQ_LEVEL_LOGGING) == 0)) {
		/*
		 * Just do normal completions hybrid pacing is not on
		 * and CLDL is off as well.
		 */
		tcp_req_check_for_comp(rack->rc_tp, th_ack);
		return;
	}
	/*
	 * Originally I was just going to find the th_ack associated
	 * with an entry. But then I realized a large strech ack could
	 * in theory ack two or more requests at once. So instead we
	 * need to find all entries that are completed by th_ack not
	 * just a single entry and do our logging.
	 */
	ent = tcp_req_find_a_req_that_is_completed_by(rack->rc_tp, th_ack, &i);
	while (ent != NULL) {
		/*
		 * We may be doing hybrid pacing or CLDL and need more details possibly
		 * so we do it manually instead of calling
		 * tcp_req_check_for_comp()
		 */
		uint64_t laa, tim, data, cbw, ftim;

		/* Ok this ack frees it */
		rack_log_hybrid(rack, th_ack,
				ent, HYBRID_LOG_REQ_COMP, __LINE__, 0);
		rack_log_hybrid_sends(rack, ent, __LINE__);
		/* calculate the time based on the ack arrival */
		data = ent->end - ent->start;
		laa = tcp_tv_to_lusectick(&rack->r_ctl.act_rcv_time);
		if (ent->flags & TCP_TRK_TRACK_FLG_FSND) {
			if (ent->first_send > ent->localtime)
				ftim = ent->first_send;
			else
				ftim = ent->localtime;
		} else {
			/* TSNH */
			ftim = ent->localtime;
		}
		if (laa > ent->localtime)
			tim = laa - ftim;
		else
			tim = 0;
		cbw = data * HPTS_USEC_IN_SEC;
		if (tim > 0)
			cbw /= tim;
		else
			cbw = 0;
		rack_log_hybrid_bw(rack, th_ack, cbw, tim, data, HYBRID_LOG_BW_MEASURE, 0, ent, __LINE__);
		/*
		 * Check to see if we are freeing what we are pointing to send wise
		 * if so be sure to NULL the pointer so we know we are no longer
		 * set to anything.
		 */
		if (ent == rack->r_ctl.rc_last_sft) {
			rack->r_ctl.rc_last_sft = NULL;
			if (rack->rc_hybrid_mode) {
				rack->rc_catch_up = 0;
				if (rack->cspr_is_fcc == 0)
					rack->r_ctl.bw_rate_cap = 0;
				else
					rack->r_ctl.fillcw_cap = rack_fillcw_bw_cap;
				rack->r_ctl.client_suggested_maxseg = 0;
			}
		}
		/* Generate the log that the tcp_netflix call would have */
		tcp_req_log_req_info(rack->rc_tp, ent,
				      i, TCP_TRK_REQ_LOG_FREED, 0, 0);
		/* Free it and see if there is another one */
		tcp_req_free_a_slot(rack->rc_tp, ent);
		ent = tcp_req_find_a_req_that_is_completed_by(rack->rc_tp, th_ack, &i);
	}
}
#endif


/*
 * Return value of 1, we do not need to call rack_process_data().
 * return value of 0, rack_process_data can be called.
 * For ret_val if its 0 the TCP is locked, if its non-zero
 * its unlocked and probably unsafe to touch the TCB.
 */
static int
rack_process_ack(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to,
    uint32_t tiwin, int32_t tlen,
    int32_t * ofia, int32_t thflags, int32_t *ret_val, int32_t orig_tlen)
{
	int32_t ourfinisacked = 0;
	int32_t nsegs, acked_amount;
	int32_t acked;
	struct mbuf *mfree;
	struct tcp_rack *rack;
	int32_t under_pacing = 0;
	int32_t post_recovery = 0;
	uint32_t p_cwnd;

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (SEQ_GT(th->th_ack, tp->snd_max)) {
		__ctf_do_dropafterack(m, tp, th, thflags, tlen, ret_val,
				      &rack->r_ctl.challenge_ack_ts,
				      &rack->r_ctl.challenge_ack_cnt);
		rack->r_wanted_output = 1;
		return (1);
	}
	if (rack->gp_ready &&
	    (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT)) {
		under_pacing = 1;
	}
	if (SEQ_GEQ(th->th_ack, tp->snd_una) || to->to_nsacks) {
		int in_rec, dup_ack_struck = 0;
		int dsack_seen = 0, sacks_seen = 0;

		in_rec = IN_FASTRECOVERY(tp->t_flags);
		if (rack->rc_in_persist) {
			tp->t_rxtshift = 0;
			RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
				      rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
		}

		if ((th->th_ack == tp->snd_una) &&
		    (tiwin == tp->snd_wnd) &&
		    (orig_tlen == 0) &&
		    ((to->to_flags & TOF_SACK) == 0)) {
			rack_strike_dupack(rack, th->th_ack);
			dup_ack_struck = 1;
		}
		rack_log_ack(tp, to, th, ((in_rec == 0) && IN_FASTRECOVERY(tp->t_flags)),
			     dup_ack_struck, &dsack_seen, &sacks_seen);
		if ((rack->sack_attack_disable > 0) &&
		    (th->th_ack == tp->snd_una) &&
		    (tiwin == tp->snd_wnd) &&
		    (orig_tlen == 0) &&
		    (dsack_seen == 0) &&
		    (sacks_seen > 0)) {
			/*
			 * If sacks have been disabled we may
			 * want to strike a dup-ack "ignoring" the
			 * sack as long as the sack was not a "dsack". Note
			 * that if no sack is sent (TOF_SACK is off) then the
			 * normal dsack code above rack_log_ack() would have
			 * already struck. So this is just to catch the case
			 * were we are ignoring sacks from this guy due to
			 * it being a suspected attacker.
			 */
			rack_strike_dupack(rack, th->th_ack);
		}

	}
	if (__predict_false(SEQ_LEQ(th->th_ack, tp->snd_una))) {
		/*
		 * Old ack, behind (or duplicate to) the last one rcv'd
		 * Note: We mark reordering is occuring if its
		 * less than and we have not closed our window.
		 */
		if (SEQ_LT(th->th_ack, tp->snd_una) && (sbspace(&so->so_rcv) > ctf_fixed_maxseg(tp))) {
			rack->r_ctl.rc_reorder_ts = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
			if (rack->r_ctl.rc_reorder_ts == 0)
				rack->r_ctl.rc_reorder_ts = 1;
		}
		return (0);
	}
	/*
	 * If we reach this point, ACK is not a duplicate, i.e., it ACKs
	 * something we sent.
	 */
	if (tp->t_flags & TF_NEEDSYN) {
		/*
		 * T/TCP: Connection was half-synchronized, and our SYN has
		 * been ACK'd (so connection is now fully synchronized).  Go
		 * to non-starred state, increment snd_una for ACK of SYN,
		 * and check if we can do window scaling.
		 */
		tp->t_flags &= ~TF_NEEDSYN;
		tp->snd_una++;
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
		    (TF_RCVD_SCALE | TF_REQ_SCALE)) {
			tp->rcv_scale = tp->request_r_scale;
			/* Send window already scaled. */
		}
	}
	nsegs = max(1, m->m_pkthdr.lro_nsegs);

	acked = BYTES_THIS_ACK(tp, th);
	if (acked) {
		/*
		 * Any time we move the cum-ack forward clear
		 * keep-alive tied probe-not-answered. The
		 * persists clears its own on entry.
		 */
		rack->probe_not_answered = 0;
	}
	KMOD_TCPSTAT_ADD(tcps_rcvackpack, nsegs);
	KMOD_TCPSTAT_ADD(tcps_rcvackbyte, acked);
	/*
	 * If we just performed our first retransmit, and the ACK arrives
	 * within our recovery window, then it was a mistake to do the
	 * retransmit in the first place.  Recover our original cwnd and
	 * ssthresh, and proceed to transmit where we left off.
	 */
	if ((tp->t_flags & TF_PREVVALID) &&
	    ((tp->t_flags & TF_RCVD_TSTMP) == 0)) {
		tp->t_flags &= ~TF_PREVVALID;
		if (tp->t_rxtshift == 1 &&
		    (int)(ticks - tp->t_badrxtwin) < 0)
			rack_cong_signal(tp, CC_RTO_ERR, th->th_ack, __LINE__);
	}
	if (acked) {
		/* assure we are not backed off */
		tp->t_rxtshift = 0;
		RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
			      rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
		rack->rc_tlp_in_progress = 0;
		rack->r_ctl.rc_tlp_cnt_out = 0;
		/*
		 * If it is the RXT timer we want to
		 * stop it, so we can restart a TLP.
		 */
		if (rack->r_ctl.rc_hpts_flags & PACE_TMR_RXT)
			rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
#ifdef TCP_REQUEST_TRK
		rack_req_check_for_comp(rack, th->th_ack);
#endif
	}
	/*
	 * If we have a timestamp reply, update smoothed round trip time. If
	 * no timestamp is present but transmit timer is running and timed
	 * sequence number was acked, update smoothed round trip time. Since
	 * we now have an rtt measurement, cancel the timer backoff (cf.,
	 * Phil Karn's retransmit alg.). Recompute the initial retransmit
	 * timer.
	 *
	 * Some boxes send broken timestamp replies during the SYN+ACK
	 * phase, ignore timestamps of 0 or we could calculate a huge RTT
	 * and blow up the retransmit timer.
	 */
	/*
	 * If all outstanding data is acked, stop retransmit timer and
	 * remember to restart (more output or persist). If there is more
	 * data to be acked, restart retransmit timer, using current
	 * (possibly backed-off) value.
	 */
	if (acked == 0) {
		if (ofia)
			*ofia = ourfinisacked;
		return (0);
	}
	if (IN_RECOVERY(tp->t_flags)) {
		if (SEQ_LT(th->th_ack, tp->snd_recover) &&
		    (SEQ_LT(th->th_ack, tp->snd_max))) {
			tcp_rack_partialack(tp);
		} else {
			rack_post_recovery(tp, th->th_ack);
			post_recovery = 1;
			/*
			 * Grab the segsiz, multiply by 2 and add the snd_cwnd
			 * that is the max the CC should add if we are exiting
			 * recovery and doing a late add.
			 */
			p_cwnd = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
			p_cwnd <<= 1;
			p_cwnd += tp->snd_cwnd;
		}
	} else if ((rack->rto_from_rec == 1) &&
		   SEQ_GEQ(th->th_ack, tp->snd_recover)) {
		/*
		 * We were in recovery, hit a rxt timeout
		 * and never re-entered recovery. The timeout(s)
		 * made up all the lost data. In such a case
		 * we need to clear the rto_from_rec flag.
		 */
		rack->rto_from_rec = 0;
	}
	/*
	 * Let the congestion control algorithm update congestion control
	 * related information. This typically means increasing the
	 * congestion window.
	 */
	rack_ack_received(tp, rack, th->th_ack, nsegs, CC_ACK, post_recovery);
	if (post_recovery &&
	    (tp->snd_cwnd > p_cwnd)) {
		/* Must be non-newreno (cubic) getting too ahead of itself */
		tp->snd_cwnd = p_cwnd;
	}
	SOCKBUF_LOCK(&so->so_snd);
	acked_amount = min(acked, (int)sbavail(&so->so_snd));
	tp->snd_wnd -= acked_amount;
	mfree = sbcut_locked(&so->so_snd, acked_amount);
	if ((sbused(&so->so_snd) == 0) &&
	    (acked > acked_amount) &&
	    (tp->t_state >= TCPS_FIN_WAIT_1) &&
	    (tp->t_flags & TF_SENTFIN)) {
		/*
		 * We must be sure our fin
		 * was sent and acked (we can be
		 * in FIN_WAIT_1 without having
		 * sent the fin).
		 */
		ourfinisacked = 1;
	}
	tp->snd_una = th->th_ack;
	/* wakeups? */
	if (acked_amount && sbavail(&so->so_snd))
		rack_adjust_sendmap_head(rack, &so->so_snd);
	rack_log_wakeup(tp,rack, &so->so_snd, acked, 2);
	/* NB: sowwakeup_locked() does an implicit unlock. */
	sowwakeup_locked(so);
	m_freem(mfree);
	if (SEQ_GT(tp->snd_una, tp->snd_recover))
		tp->snd_recover = tp->snd_una;

	if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
		tp->snd_nxt = tp->snd_max;
	}
	if (under_pacing &&
	    (rack->use_fixed_rate == 0) &&
	    (rack->in_probe_rtt == 0) &&
	    rack->rc_gp_dyn_mul &&
	    rack->rc_always_pace) {
		/* Check if we are dragging bottom */
		rack_check_bottom_drag(tp, rack, so);
	}
	if (tp->snd_una == tp->snd_max) {
		/* Nothing left outstanding */
		tp->t_flags &= ~TF_PREVVALID;
		rack->r_ctl.idle_snd_una = tp->snd_una;
		rack->r_ctl.rc_went_idle_time = tcp_get_usecs(NULL);
		if (rack->r_ctl.rc_went_idle_time == 0)
			rack->r_ctl.rc_went_idle_time = 1;
		rack->r_ctl.retran_during_recovery = 0;
		rack->r_ctl.dsack_byte_cnt = 0;
		rack_log_progress_event(rack, tp, 0, PROGRESS_CLEAR, __LINE__);
		if (sbavail(&tptosocket(tp)->so_snd) == 0)
			tp->t_acktime = 0;
		rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
		rack->rc_suspicious = 0;
		/* Set need output so persist might get set */
		rack->r_wanted_output = 1;
		sack_filter_clear(&rack->r_ctl.rack_sf, tp->snd_una);
		if ((tp->t_state >= TCPS_FIN_WAIT_1) &&
		    (sbavail(&so->so_snd) == 0) &&
		    (tp->t_flags2 & TF2_DROP_AF_DATA)) {
			/*
			 * The socket was gone and the
			 * peer sent data (now or in the past), time to
			 * reset him.
			 */
			*ret_val = 1;
			/* tcp_close will kill the inp pre-log the Reset */
			tcp_log_end_status(tp, TCP_EI_STATUS_SERVER_RST);
			tp = tcp_close(tp);
			ctf_do_dropwithreset(m, tp, th, BANDLIM_UNLIMITED, tlen);
			return (1);
		}
	}
	if (ofia)
		*ofia = ourfinisacked;
	return (0);
}


static void
rack_log_collapse(struct tcp_rack *rack, uint32_t cnt, uint32_t split, uint32_t out, int line,
		  int dir, uint32_t flags, struct rack_sendmap *rsm)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = cnt;
		log.u_bbr.flex2 = split;
		log.u_bbr.flex3 = out;
		log.u_bbr.flex4 = line;
		log.u_bbr.flex5 = rack->r_must_retran;
		log.u_bbr.flex6 = flags;
		log.u_bbr.flex7 = rack->rc_has_collapsed;
		log.u_bbr.flex8 = dir;	/*
					 * 1 is collapsed, 0 is uncollapsed,
					 * 2 is log of a rsm being marked, 3 is a split.
					 */
		if (rsm == NULL)
			log.u_bbr.rttProp = 0;
		else
			log.u_bbr.rttProp = (uint64_t)rsm;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    TCP_RACK_LOG_COLLAPSE, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_collapsed_window(struct tcp_rack *rack, uint32_t out, tcp_seq th_ack, int line)
{
	/*
	 * Here all we do is mark the collapsed point and set the flag.
	 * This may happen again and again, but there is no
	 * sense splitting our map until we know where the
	 * peer finally lands in the collapse.
	 */
	tcp_trace_point(rack->rc_tp, TCP_TP_COLLAPSED_WND);
	if ((rack->rc_has_collapsed == 0) ||
	    (rack->r_ctl.last_collapse_point != (th_ack + rack->rc_tp->snd_wnd)))
		counter_u64_add(rack_collapsed_win_seen, 1);
	rack->r_ctl.last_collapse_point = th_ack + rack->rc_tp->snd_wnd;
	rack->r_ctl.high_collapse_point = rack->rc_tp->snd_max;
	rack->rc_has_collapsed = 1;
	rack->r_collapse_point_valid = 1;
	rack_log_collapse(rack, 0, th_ack, rack->r_ctl.last_collapse_point, line, 1, 0, NULL);
}

static void
rack_un_collapse_window(struct tcp_rack *rack, int line)
{
	struct rack_sendmap *nrsm, *rsm;
	int cnt = 0, split = 0;
	int insret __diagused;


	tcp_trace_point(rack->rc_tp, TCP_TP_COLLAPSED_WND);
	rack->rc_has_collapsed = 0;
	rsm = tqhash_find(rack->r_ctl.tqh, rack->r_ctl.last_collapse_point);
	if (rsm == NULL) {
		/* Nothing to do maybe the peer ack'ed it all */
		rack_log_collapse(rack, 0, 0, ctf_outstanding(rack->rc_tp), line, 0, 0, NULL);
		return;
	}
	/* Now do we need to split this one? */
	if (SEQ_GT(rack->r_ctl.last_collapse_point, rsm->r_start)) {
		rack_log_collapse(rack, rsm->r_start, rsm->r_end,
				  rack->r_ctl.last_collapse_point, line, 3, rsm->r_flags, rsm);
		nrsm = rack_alloc_limit(rack, RACK_LIMIT_TYPE_SPLIT);
		if (nrsm == NULL) {
			/* We can't get a rsm, mark all? */
			nrsm = rsm;
			goto no_split;
		}
		/* Clone it */
		split = 1;
		rack_clone_rsm(rack, nrsm, rsm, rack->r_ctl.last_collapse_point);
#ifndef INVARIANTS
		(void)tqhash_insert(rack->r_ctl.tqh, nrsm);
#else
		if ((insret = tqhash_insert(rack->r_ctl.tqh, nrsm)) != 0) {
			panic("Insert in tailq_hash of %p fails ret:%d rack:%p rsm:%p",
			      nrsm, insret, rack, rsm);
		}
#endif
		rack_log_map_chg(rack->rc_tp, rack, NULL, rsm, nrsm, MAP_SPLIT,
				 rack->r_ctl.last_collapse_point, __LINE__);
		if (rsm->r_in_tmap) {
			TAILQ_INSERT_AFTER(&rack->r_ctl.rc_tmap, rsm, nrsm, r_tnext);
			nrsm->r_in_tmap = 1;
		}
		/*
		 * Set in the new RSM as the
		 * collapsed starting point
		 */
		rsm = nrsm;
	}

no_split:
	TQHASH_FOREACH_FROM(nrsm, rack->r_ctl.tqh, rsm)  {
		cnt++;
		nrsm->r_flags |= RACK_RWND_COLLAPSED;
		rack_log_collapse(rack, nrsm->r_start, nrsm->r_end, 0, line, 4, nrsm->r_flags, nrsm);
		cnt++;
	}
	if (cnt) {
		counter_u64_add(rack_collapsed_win, 1);
	}
	rack_log_collapse(rack, cnt, split, ctf_outstanding(rack->rc_tp), line, 0, 0, NULL);
}

static void
rack_handle_delayed_ack(struct tcpcb *tp, struct tcp_rack *rack,
			int32_t tlen, int32_t tfo_syn)
{
	if (DELAY_ACK(tp, tlen) || tfo_syn) {
		rack_timer_cancel(tp, rack,
				  rack->r_ctl.rc_rcvtime, __LINE__);
		tp->t_flags |= TF_DELACK;
	} else {
		rack->r_wanted_output = 1;
		tp->t_flags |= TF_ACKNOW;
	}
}

static void
rack_validate_fo_sendwin_up(struct tcpcb *tp, struct tcp_rack *rack)
{
	/*
	 * If fast output is in progress, lets validate that
	 * the new window did not shrink on us and make it
	 * so fast output should end.
	 */
	if (rack->r_fast_output) {
		uint32_t out;

		/*
		 * Calculate what we will send if left as is
		 * and compare that to our send window.
		 */
		out = ctf_outstanding(tp);
		if ((out + rack->r_ctl.fsb.left_to_send) > tp->snd_wnd) {
			/* ok we have an issue */
			if (out >= tp->snd_wnd) {
				/* Turn off fast output the window is met or collapsed */
				rack->r_fast_output = 0;
			} else {
				/* we have some room left */
				rack->r_ctl.fsb.left_to_send = tp->snd_wnd - out;
				if (rack->r_ctl.fsb.left_to_send < ctf_fixed_maxseg(tp)) {
					/* If not at least 1 full segment never mind */
					rack->r_fast_output = 0;
				}
			}
		}
	}
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_process_data(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt)
{
	/*
	 * Update window information. Don't look at window if no ACK: TAC's
	 * send garbage on first SYN.
	 */
	int32_t nsegs;
	int32_t tfo_syn;
	struct tcp_rack *rack;

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	nsegs = max(1, m->m_pkthdr.lro_nsegs);
	if ((thflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
	    (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tlen == 0 &&
		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
			KMOD_TCPSTAT_INC(tcps_rcvwinupd);
		tp->snd_wnd = tiwin;
		rack_validate_fo_sendwin_up(tp, rack);
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		rack->r_wanted_output = 1;
	} else if (thflags & TH_ACK) {
		if ((tp->snd_wl2 == th->th_ack) && (tiwin < tp->snd_wnd)) {
			tp->snd_wnd = tiwin;
			rack_validate_fo_sendwin_up(tp, rack);
			tp->snd_wl1 = th->th_seq;
			tp->snd_wl2 = th->th_ack;
		}
	}
	if (tp->snd_wnd < ctf_outstanding(tp))
		/* The peer collapsed the window */
		rack_collapsed_window(rack, ctf_outstanding(tp), th->th_ack, __LINE__);
	else if (rack->rc_has_collapsed)
		rack_un_collapse_window(rack, __LINE__);
	if ((rack->r_collapse_point_valid) &&
	    (SEQ_GT(th->th_ack, rack->r_ctl.high_collapse_point)))
		rack->r_collapse_point_valid = 0;
	/* Was persist timer active and now we have window space? */
	if ((rack->rc_in_persist != 0) &&
	    (tp->snd_wnd >= min((rack->r_ctl.rc_high_rwnd/2),
				rack->r_ctl.rc_pace_min_segs))) {
		rack_exit_persist(tp, rack, rack->r_ctl.rc_rcvtime);
		tp->snd_nxt = tp->snd_max;
		/* Make sure we output to start the timer */
		rack->r_wanted_output = 1;
	}
	/* Do we enter persists? */
	if ((rack->rc_in_persist == 0) &&
	    (tp->snd_wnd < min((rack->r_ctl.rc_high_rwnd/2), rack->r_ctl.rc_pace_min_segs)) &&
	    TCPS_HAVEESTABLISHED(tp->t_state) &&
	    ((tp->snd_max == tp->snd_una) || rack->rc_has_collapsed) &&
	    sbavail(&tptosocket(tp)->so_snd) &&
	    (sbavail(&tptosocket(tp)->so_snd) > tp->snd_wnd)) {
		/*
		 * Here the rwnd is less than
		 * the pacing size, we are established,
		 * nothing is outstanding, and there is
		 * data to send. Enter persists.
		 */
		rack_enter_persist(tp, rack, rack->r_ctl.rc_rcvtime, tp->snd_una);
	}
	if (tp->t_flags2 & TF2_DROP_AF_DATA) {
		m_freem(m);
		return (0);
	}
	/*
	 * don't process the URG bit, ignore them drag
	 * along the up.
	 */
	tp->rcv_up = tp->rcv_nxt;

	/*
	 * Process the segment text, merging it into the TCP sequencing
	 * queue, and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data is
	 * presented to the user (this happens in tcp_usrreq.c, case
	 * PRU_RCVD).  If a FIN has already been received on this connection
	 * then we just ignore the text.
	 */
	tfo_syn = ((tp->t_state == TCPS_SYN_RECEIVED) &&
	    (tp->t_flags & TF_FASTOPEN));
	if ((tlen || (thflags & TH_FIN) || (tfo_syn && tlen > 0)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		tcp_seq save_start = th->th_seq;
		tcp_seq save_rnxt  = tp->rcv_nxt;
		int     save_tlen  = tlen;

		m_adj(m, drop_hdrlen);	/* delayed header drop */
		/*
		 * Insert segment which includes th into TCP reassembly
		 * queue with control block tp.  Set thflags to whether
		 * reassembly now includes a segment with FIN.  This handles
		 * the common case inline (segment is the next to be
		 * received on an established connection, and the queue is
		 * empty), avoiding linkage into and removal from the queue
		 * and repetition of various conversions. Set DELACK for
		 * segments received in order, but ack immediately when
		 * segments are out of order (so fast retransmit can work).
		 */
		if (th->th_seq == tp->rcv_nxt &&
		    SEGQ_EMPTY(tp) &&
		    (TCPS_HAVEESTABLISHED(tp->t_state) ||
		    tfo_syn)) {
#ifdef NETFLIX_SB_LIMITS
			u_int mcnt, appended;

			if (so->so_rcv.sb_shlim) {
				mcnt = m_memcnt(m);
				appended = 0;
				if (counter_fo_get(so->so_rcv.sb_shlim, mcnt,
				    CFO_NOSLEEP, NULL) == false) {
					counter_u64_add(tcp_sb_shlim_fails, 1);
					m_freem(m);
					return (0);
				}
			}
#endif
			rack_handle_delayed_ack(tp, rack, tlen, tfo_syn);
			tp->rcv_nxt += tlen;
			if (tlen &&
			    ((tp->t_flags2 & TF2_FBYTES_COMPLETE) == 0) &&
			    (tp->t_fbyte_in == 0)) {
				tp->t_fbyte_in = ticks;
				if (tp->t_fbyte_in == 0)
					tp->t_fbyte_in = 1;
				if (tp->t_fbyte_out && tp->t_fbyte_in)
					tp->t_flags2 |= TF2_FBYTES_COMPLETE;
			}
			thflags = tcp_get_flags(th) & TH_FIN;
			KMOD_TCPSTAT_ADD(tcps_rcvpack, nsegs);
			KMOD_TCPSTAT_ADD(tcps_rcvbyte, tlen);
			SOCKBUF_LOCK(&so->so_rcv);
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
				m_freem(m);
			} else {
				int32_t newsize;

				if (tlen > 0) {
					newsize = tcp_autorcvbuf(m, th, so, tp, tlen);
					if (newsize)
						if (!sbreserve_locked(so, SO_RCV, newsize, NULL))
							so->so_rcv.sb_flags &= ~SB_AUTOSIZE;
				}
#ifdef NETFLIX_SB_LIMITS
				appended =
#endif
					sbappendstream_locked(&so->so_rcv, m, 0);
			}
			rack_log_wakeup(tp,rack, &so->so_rcv, tlen, 1);
			/* NB: sorwakeup_locked() does an implicit unlock. */
			sorwakeup_locked(so);
#ifdef NETFLIX_SB_LIMITS
			if (so->so_rcv.sb_shlim && appended != mcnt)
				counter_fo_release(so->so_rcv.sb_shlim,
				    mcnt - appended);
#endif
		} else {
			/*
			 * XXX: Due to the header drop above "th" is
			 * theoretically invalid by now.  Fortunately
			 * m_adj() doesn't actually frees any mbufs when
			 * trimming from the head.
			 */
			tcp_seq temp = save_start;

			thflags = tcp_reass(tp, th, &temp, &tlen, m);
			tp->t_flags |= TF_ACKNOW;
			if (tp->t_flags & TF_WAKESOR) {
				tp->t_flags &= ~TF_WAKESOR;
				/* NB: sorwakeup_locked() does an implicit unlock. */
				sorwakeup_locked(so);
			}
		}
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    (save_tlen > 0) &&
		    TCPS_HAVEESTABLISHED(tp->t_state)) {
			if ((tlen == 0) && (SEQ_LT(save_start, save_rnxt))) {
				/*
				 * DSACK actually handled in the fastpath
				 * above.
				 */
				tcp_update_sack_list(tp, save_start,
				    save_start + save_tlen);
			} else if ((tlen > 0) && SEQ_GT(tp->rcv_nxt, save_rnxt)) {
				if ((tp->rcv_numsacks >= 1) &&
				    (tp->sackblks[0].end == save_start)) {
					/*
					 * Partial overlap, recorded at todrop
					 * above.
					 */
					tcp_update_sack_list(tp,
					    tp->sackblks[0].start,
					    tp->sackblks[0].end);
				} else {
					tcp_update_dsack_list(tp, save_start,
					    save_start + save_tlen);
				}
			} else if (tlen >= save_tlen) {
				/* Update of sackblks. */
				tcp_update_dsack_list(tp, save_start,
				    save_start + save_tlen);
			} else if (tlen > 0) {
				tcp_update_dsack_list(tp, save_start,
				    save_start + tlen);
			}
		}
	} else {
		m_freem(m);
		thflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know that the
	 * connection is closing.
	 */
	if (thflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			/* The socket upcall is handled by socantrcvmore. */
			socantrcvmore(so);
			/*
			 * If connection is half-synchronized (ie NEEDSYN
			 * flag on) then delay ACK, so it may be piggybacked
			 * when SYN is sent. Otherwise, since we received a
			 * FIN then no more input can be expected, send ACK
			 * now.
			 */
			if (tp->t_flags & TF_NEEDSYN) {
				rack_timer_cancel(tp, rack,
				    rack->r_ctl.rc_rcvtime, __LINE__);
				tp->t_flags |= TF_DELACK;
			} else {
				tp->t_flags |= TF_ACKNOW;
			}
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {
			/*
			 * In SYN_RECEIVED and ESTABLISHED STATES enter the
			 * CLOSE_WAIT state.
			 */
		case TCPS_SYN_RECEIVED:
			tp->t_starttime = ticks;
			/* FALLTHROUGH */
		case TCPS_ESTABLISHED:
			rack_timer_cancel(tp, rack,
			    rack->r_ctl.rc_rcvtime, __LINE__);
			tcp_state_change(tp, TCPS_CLOSE_WAIT);
			break;

			/*
			 * If still in FIN_WAIT_1 STATE FIN has not been
			 * acked so enter the CLOSING state.
			 */
		case TCPS_FIN_WAIT_1:
			rack_timer_cancel(tp, rack,
			    rack->r_ctl.rc_rcvtime, __LINE__);
			tcp_state_change(tp, TCPS_CLOSING);
			break;

			/*
			 * In FIN_WAIT_2 state enter the TIME_WAIT state,
			 * starting the time-wait timer, turning off the
			 * other standard timers.
			 */
		case TCPS_FIN_WAIT_2:
			rack_timer_cancel(tp, rack,
			    rack->r_ctl.rc_rcvtime, __LINE__);
			tcp_twstart(tp);
			return (1);
		}
	}
	/*
	 * Return any desired output.
	 */
	if ((tp->t_flags & TF_ACKNOW) ||
	    (sbavail(&so->so_snd) > (tp->snd_max - tp->snd_una))) {
		rack->r_wanted_output = 1;
	}
	return (0);
}

/*
 * Here nothing is really faster, its just that we
 * have broken out the fast-data path also just like
 * the fast-ack.
 */
static int
rack_do_fastnewdata(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t nsegs;
	int32_t newsize = 0;	/* automatic sockbuf scaling */
	struct tcp_rack *rack;
#ifdef NETFLIX_SB_LIMITS
	u_int mcnt, appended;
#endif

	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * the timestamp. NOTE that the test is modified according to the
	 * latest proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 */
	if (__predict_false(th->th_seq != tp->rcv_nxt)) {
		return (0);
	}
	if (tiwin && tiwin != tp->snd_wnd) {
		return (0);
	}
	if (__predict_false((tp->t_flags & (TF_NEEDSYN | TF_NEEDFIN)))) {
		return (0);
	}
	if (__predict_false((to->to_flags & TOF_TS) &&
	    (TSTMP_LT(to->to_tsval, tp->ts_recent)))) {
		return (0);
	}
	if (__predict_false((th->th_ack != tp->snd_una))) {
		return (0);
	}
	if (__predict_false(tlen > sbspace(&so->so_rcv))) {
		return (0);
	}
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	/*
	 * This is a pure, in-sequence data packet with nothing on the
	 * reassembly queue and we have enough buffer space to take it.
	 */
	nsegs = max(1, m->m_pkthdr.lro_nsegs);

#ifdef NETFLIX_SB_LIMITS
	if (so->so_rcv.sb_shlim) {
		mcnt = m_memcnt(m);
		appended = 0;
		if (counter_fo_get(so->so_rcv.sb_shlim, mcnt,
		    CFO_NOSLEEP, NULL) == false) {
			counter_u64_add(tcp_sb_shlim_fails, 1);
			m_freem(m);
			return (1);
		}
	}
#endif
	/* Clean receiver SACK report if present */
	if (tp->rcv_numsacks)
		tcp_clean_sackreport(tp);
	KMOD_TCPSTAT_INC(tcps_preddat);
	tp->rcv_nxt += tlen;
	if (tlen &&
	    ((tp->t_flags2 & TF2_FBYTES_COMPLETE) == 0) &&
	    (tp->t_fbyte_in == 0)) {
		tp->t_fbyte_in = ticks;
		if (tp->t_fbyte_in == 0)
			tp->t_fbyte_in = 1;
		if (tp->t_fbyte_out && tp->t_fbyte_in)
			tp->t_flags2 |= TF2_FBYTES_COMPLETE;
	}
	/*
	 * Pull snd_wl1 up to prevent seq wrap relative to th_seq.
	 */
	tp->snd_wl1 = th->th_seq;
	/*
	 * Pull rcv_up up to prevent seq wrap relative to rcv_nxt.
	 */
	tp->rcv_up = tp->rcv_nxt;
	KMOD_TCPSTAT_ADD(tcps_rcvpack, nsegs);
	KMOD_TCPSTAT_ADD(tcps_rcvbyte, tlen);
	newsize = tcp_autorcvbuf(m, th, so, tp, tlen);

	/* Add data to socket buffer. */
	SOCKBUF_LOCK(&so->so_rcv);
	if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
		m_freem(m);
	} else {
		/*
		 * Set new socket buffer size. Give up when limit is
		 * reached.
		 */
		if (newsize)
			if (!sbreserve_locked(so, SO_RCV, newsize, NULL))
				so->so_rcv.sb_flags &= ~SB_AUTOSIZE;
		m_adj(m, drop_hdrlen);	/* delayed header drop */
#ifdef NETFLIX_SB_LIMITS
		appended =
#endif
			sbappendstream_locked(&so->so_rcv, m, 0);
		ctf_calc_rwin(so, tp);
	}
	rack_log_wakeup(tp,rack, &so->so_rcv, tlen, 1);
	/* NB: sorwakeup_locked() does an implicit unlock. */
	sorwakeup_locked(so);
#ifdef NETFLIX_SB_LIMITS
	if (so->so_rcv.sb_shlim && mcnt != appended)
		counter_fo_release(so->so_rcv.sb_shlim, mcnt - appended);
#endif
	rack_handle_delayed_ack(tp, rack, tlen, 0);
	if (tp->snd_una == tp->snd_max)
		sack_filter_clear(&rack->r_ctl.rack_sf, tp->snd_una);
	return (1);
}

/*
 * This subfunction is used to try to highly optimize the
 * fast path. We again allow window updates that are
 * in sequence to remain in the fast-path. We also add
 * in the __predict's to attempt to help the compiler.
 * Note that if we return a 0, then we can *not* process
 * it and the caller should push the packet into the
 * slow-path.
 */
static int
rack_fastack(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t nxt_pkt, uint32_t cts)
{
	int32_t acked;
	int32_t nsegs;
	int32_t under_pacing = 0;
	struct tcp_rack *rack;

	if (__predict_false(SEQ_LEQ(th->th_ack, tp->snd_una))) {
		/* Old ack, behind (or duplicate to) the last one rcv'd */
		return (0);
	}
	if (__predict_false(SEQ_GT(th->th_ack, tp->snd_max))) {
		/* Above what we have sent? */
		return (0);
	}
	if (__predict_false(tiwin == 0)) {
		/* zero window */
		return (0);
	}
	if (__predict_false(tp->t_flags & (TF_NEEDSYN | TF_NEEDFIN))) {
		/* We need a SYN or a FIN, unlikely.. */
		return (0);
	}
	if ((to->to_flags & TOF_TS) && __predict_false(TSTMP_LT(to->to_tsval, tp->ts_recent))) {
		/* Timestamp is behind .. old ack with seq wrap? */
		return (0);
	}
	if (__predict_false(IN_RECOVERY(tp->t_flags))) {
		/* Still recovering */
		return (0);
	}
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack->r_ctl.rc_sacked) {
		/* We have sack holes on our scoreboard */
		return (0);
	}
	/* Ok if we reach here, we can process a fast-ack */
	if (rack->gp_ready &&
	    (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT)) {
		under_pacing = 1;
	}
	nsegs = max(1, m->m_pkthdr.lro_nsegs);
	rack_log_ack(tp, to, th, 0, 0, NULL, NULL);
	/* Did the window get updated? */
	if (tiwin != tp->snd_wnd) {
		tp->snd_wnd = tiwin;
		rack_validate_fo_sendwin_up(tp, rack);
		tp->snd_wl1 = th->th_seq;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
	}
	/* Do we exit persists? */
	if ((rack->rc_in_persist != 0) &&
	    (tp->snd_wnd >= min((rack->r_ctl.rc_high_rwnd/2),
			       rack->r_ctl.rc_pace_min_segs))) {
		rack_exit_persist(tp, rack, cts);
	}
	/* Do we enter persists? */
	if ((rack->rc_in_persist == 0) &&
	    (tp->snd_wnd < min((rack->r_ctl.rc_high_rwnd/2), rack->r_ctl.rc_pace_min_segs)) &&
	    TCPS_HAVEESTABLISHED(tp->t_state) &&
	    ((tp->snd_max == tp->snd_una) || rack->rc_has_collapsed) &&
	    sbavail(&tptosocket(tp)->so_snd) &&
	    (sbavail(&tptosocket(tp)->so_snd) > tp->snd_wnd)) {
		/*
		 * Here the rwnd is less than
		 * the pacing size, we are established,
		 * nothing is outstanding, and there is
		 * data to send. Enter persists.
		 */
		rack_enter_persist(tp, rack, rack->r_ctl.rc_rcvtime, th->th_ack);
	}
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * the timestamp. NOTE that the test is modified according to the
	 * latest proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * This is a pure ack for outstanding data.
	 */
	KMOD_TCPSTAT_INC(tcps_predack);

	/*
	 * "bad retransmit" recovery.
	 */
	if ((tp->t_flags & TF_PREVVALID) &&
	    ((tp->t_flags & TF_RCVD_TSTMP) == 0)) {
		tp->t_flags &= ~TF_PREVVALID;
		if (tp->t_rxtshift == 1 &&
		    (int)(ticks - tp->t_badrxtwin) < 0)
			rack_cong_signal(tp, CC_RTO_ERR, th->th_ack, __LINE__);
	}
	/*
	 * Recalculate the transmit timer / rtt.
	 *
	 * Some boxes send broken timestamp replies during the SYN+ACK
	 * phase, ignore timestamps of 0 or we could calculate a huge RTT
	 * and blow up the retransmit timer.
	 */
	acked = BYTES_THIS_ACK(tp, th);

#ifdef TCP_HHOOK
	/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
	hhook_run_tcp_est_in(tp, th, to);
#endif
	KMOD_TCPSTAT_ADD(tcps_rcvackpack, nsegs);
	KMOD_TCPSTAT_ADD(tcps_rcvackbyte, acked);
	if (acked) {
		struct mbuf *mfree;

		rack_ack_received(tp, rack, th->th_ack, nsegs, CC_ACK, 0);
		SOCKBUF_LOCK(&so->so_snd);
		mfree = sbcut_locked(&so->so_snd, acked);
		tp->snd_una = th->th_ack;
		/* Note we want to hold the sb lock through the sendmap adjust */
		rack_adjust_sendmap_head(rack, &so->so_snd);
		/* Wake up the socket if we have room to write more */
		rack_log_wakeup(tp,rack, &so->so_snd, acked, 2);
		sowwakeup_locked(so);
		m_freem(mfree);
		tp->t_rxtshift = 0;
		RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
			      rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
		rack->rc_tlp_in_progress = 0;
		rack->r_ctl.rc_tlp_cnt_out = 0;
		/*
		 * If it is the RXT timer we want to
		 * stop it, so we can restart a TLP.
		 */
		if (rack->r_ctl.rc_hpts_flags & PACE_TMR_RXT)
			rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);

#ifdef TCP_REQUEST_TRK
		rack_req_check_for_comp(rack, th->th_ack);
#endif
	}
	/*
	 * Let the congestion control algorithm update congestion control
	 * related information. This typically means increasing the
	 * congestion window.
	 */
	if (tp->snd_wnd < ctf_outstanding(tp)) {
		/* The peer collapsed the window */
		rack_collapsed_window(rack, ctf_outstanding(tp), th->th_ack, __LINE__);
	} else if (rack->rc_has_collapsed)
		rack_un_collapse_window(rack, __LINE__);
	if ((rack->r_collapse_point_valid) &&
	    (SEQ_GT(tp->snd_una, rack->r_ctl.high_collapse_point)))
		rack->r_collapse_point_valid = 0;
	/*
	 * Pull snd_wl2 up to prevent seq wrap relative to th_ack.
	 */
	tp->snd_wl2 = th->th_ack;
	tp->t_dupacks = 0;
	m_freem(m);
	/* ND6_HINT(tp);	 *//* Some progress has been made. */

	/*
	 * If all outstanding data are acked, stop retransmit timer,
	 * otherwise restart timer using current (possibly backed-off)
	 * value. If process is waiting for space, wakeup/selwakeup/signal.
	 * If data are ready to send, let tcp_output decide between more
	 * output or persist.
	 */
	if (under_pacing &&
	    (rack->use_fixed_rate == 0) &&
	    (rack->in_probe_rtt == 0) &&
	    rack->rc_gp_dyn_mul &&
	    rack->rc_always_pace) {
		/* Check if we are dragging bottom */
		rack_check_bottom_drag(tp, rack, so);
	}
	if (tp->snd_una == tp->snd_max) {
		tp->t_flags &= ~TF_PREVVALID;
		rack->r_ctl.retran_during_recovery = 0;
		rack->rc_suspicious = 0;
		rack->r_ctl.dsack_byte_cnt = 0;
		rack->r_ctl.idle_snd_una = tp->snd_una;
		rack->r_ctl.rc_went_idle_time = tcp_get_usecs(NULL);
		if (rack->r_ctl.rc_went_idle_time == 0)
			rack->r_ctl.rc_went_idle_time = 1;
		rack_log_progress_event(rack, tp, 0, PROGRESS_CLEAR, __LINE__);
		if (sbavail(&tptosocket(tp)->so_snd) == 0)
			tp->t_acktime = 0;
		rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
	}
	if (acked && rack->r_fast_output)
		rack_gain_for_fastoutput(rack, tp, so, (uint32_t)acked);
	if (sbavail(&so->so_snd)) {
		rack->r_wanted_output = 1;
	}
	return (1);
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_syn_sent(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t ret_val = 0;
	int32_t orig_tlen = tlen;
	int32_t todrop;
	int32_t ourfinisacked = 0;
	struct tcp_rack *rack;

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	ctf_calc_rwin(so, tp);
	/*
	 * If the state is SYN_SENT: if seg contains an ACK, but not for our
	 * SYN, drop the input. if seg contains a RST, then drop the
	 * connection. if seg does not contain SYN, then drop it. Otherwise
	 * this is an acceptable SYN segment initialize tp->rcv_nxt and
	 * tp->irs if seg contains ack then advance tp->snd_una if seg
	 * contains an ECE and ECN support is enabled, the stream is ECN
	 * capable. if SYN has been acked change to ESTABLISHED else
	 * SYN_RCVD state arrange for segment to be acked (eventually)
	 * continue processing rest of data/controls.
	 */
	if ((thflags & TH_ACK) &&
	    (SEQ_LEQ(th->th_ack, tp->iss) ||
	    SEQ_GT(th->th_ack, tp->snd_max))) {
		tcp_log_end_status(tp, TCP_EI_STATUS_RST_IN_FRONT);
		ctf_do_dropwithreset(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
		return (1);
	}
	if ((thflags & (TH_ACK | TH_RST)) == (TH_ACK | TH_RST)) {
		TCP_PROBE5(connect__refused, NULL, tp,
		    mtod(m, const char *), tp, th);
		tp = tcp_drop(tp, ECONNREFUSED);
		ctf_do_drop(m, tp);
		return (1);
	}
	if (thflags & TH_RST) {
		ctf_do_drop(m, tp);
		return (1);
	}
	if (!(thflags & TH_SYN)) {
		ctf_do_drop(m, tp);
		return (1);
	}
	tp->irs = th->th_seq;
	tcp_rcvseqinit(tp);
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (thflags & TH_ACK) {
		int tfo_partial = 0;

		KMOD_TCPSTAT_INC(tcps_connects);
		soisconnected(so);
#ifdef MAC
		mac_socketpeer_set_from_mbuf(m, so);
#endif
		/* Do window scaling on this connection? */
		if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
		    (TF_RCVD_SCALE | TF_REQ_SCALE)) {
			tp->rcv_scale = tp->request_r_scale;
		}
		tp->rcv_adv += min(tp->rcv_wnd,
		    TCP_MAXWIN << tp->rcv_scale);
		/*
		 * If not all the data that was sent in the TFO SYN
		 * has been acked, resend the remainder right away.
		 */
		if ((tp->t_flags & TF_FASTOPEN) &&
		    (tp->snd_una != tp->snd_max)) {
			/* Was it a partial ack? */
			if (SEQ_LT(th->th_ack, tp->snd_max))
				tfo_partial = 1;
		}
		/*
		 * If there's data, delay ACK; if there's also a FIN ACKNOW
		 * will be turned on later.
		 */
		if (DELAY_ACK(tp, tlen) && tlen != 0 && !tfo_partial) {
			rack_timer_cancel(tp, rack,
					  rack->r_ctl.rc_rcvtime, __LINE__);
			tp->t_flags |= TF_DELACK;
		} else {
			rack->r_wanted_output = 1;
			tp->t_flags |= TF_ACKNOW;
		}

		tcp_ecn_input_syn_sent(tp, thflags, iptos);

		if (SEQ_GT(th->th_ack, tp->snd_una)) {
			/*
			 * We advance snd_una for the
			 * fast open case. If th_ack is
			 * acknowledging data beyond
			 * snd_una we can't just call
			 * ack-processing since the
			 * data stream in our send-map
			 * will start at snd_una + 1 (one
			 * beyond the SYN). If its just
			 * equal we don't need to do that
			 * and there is no send_map.
			 */
			tp->snd_una++;
			if (tfo_partial && (SEQ_GT(tp->snd_max, tp->snd_una))) {
				/*
				 * We sent a SYN with data, and thus have a
				 * sendmap entry with a SYN set. Lets find it
				 * and take off the send bit and the byte and
				 * set it up to be what we send (send it next).
				 */
				struct rack_sendmap *rsm;

				rsm = tqhash_min(rack->r_ctl.tqh);
				if (rsm) {
					if (rsm->r_flags & RACK_HAS_SYN) {
						rsm->r_flags &= ~RACK_HAS_SYN;
						rsm->r_start++;
					}
					rack->r_ctl.rc_resend = rsm;
				}
			}
		}
		/*
		 * Received <SYN,ACK> in SYN_SENT[*] state. Transitions:
		 * SYN_SENT  --> ESTABLISHED SYN_SENT* --> FIN_WAIT_1
		 */
		tp->t_starttime = ticks;
		if (tp->t_flags & TF_NEEDFIN) {
			tcp_state_change(tp, TCPS_FIN_WAIT_1);
			tp->t_flags &= ~TF_NEEDFIN;
			thflags &= ~TH_SYN;
		} else {
			tcp_state_change(tp, TCPS_ESTABLISHED);
			TCP_PROBE5(connect__established, NULL, tp,
			    mtod(m, const char *), tp, th);
			rack_cc_conn_init(tp);
		}
	} else {
		/*
		 * Received initial SYN in SYN-SENT[*] state => simultaneous
		 * open.  If segment contains CC option and there is a
		 * cached CC, apply TAO test. If it succeeds, connection is *
		 * half-synchronized. Otherwise, do 3-way handshake:
		 * SYN-SENT -> SYN-RECEIVED SYN-SENT* -> SYN-RECEIVED* If
		 * there was no CC option, clear cached CC value.
		 */
		tp->t_flags |= (TF_ACKNOW | TF_NEEDSYN | TF_SONOTCONN);
		tcp_state_change(tp, TCPS_SYN_RECEIVED);
	}
	/*
	 * Advance th->th_seq to correspond to first data byte. If data,
	 * trim to stay within window, dropping FIN if necessary.
	 */
	th->th_seq++;
	if (tlen > tp->rcv_wnd) {
		todrop = tlen - tp->rcv_wnd;
		m_adj(m, -todrop);
		tlen = tp->rcv_wnd;
		thflags &= ~TH_FIN;
		KMOD_TCPSTAT_INC(tcps_rcvpackafterwin);
		KMOD_TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
	}
	tp->snd_wl1 = th->th_seq - 1;
	tp->rcv_up = th->th_seq;
	/*
	 * Client side of transaction: already sent SYN and data. If the
	 * remote host used T/TCP to validate the SYN, our data will be
	 * ACK'd; if so, enter normal data segment processing in the middle
	 * of step 5, ack processing. Otherwise, goto step 6.
	 */
	if (thflags & TH_ACK) {
		/* For syn-sent we need to possibly update the rtt */
		if ((to->to_flags & TOF_TS) != 0 && to->to_tsecr) {
			uint32_t t, mcts;

			mcts = tcp_ts_getticks();
			t = (mcts - to->to_tsecr) * HPTS_USEC_IN_MSEC;
			if (!tp->t_rttlow || tp->t_rttlow > t)
				tp->t_rttlow = t;
			rack_log_rtt_sample_calc(rack, t, (to->to_tsecr * 1000), (mcts * 1000), 4);
			tcp_rack_xmit_timer(rack, t + 1, 1, t, 0, NULL, 2);
			tcp_rack_xmit_timer_commit(rack, tp);
		}
		if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, &ourfinisacked, thflags, &ret_val, orig_tlen))
			return (ret_val);
		/* We may have changed to FIN_WAIT_1 above */
		if (tp->t_state == TCPS_FIN_WAIT_1) {
			/*
			 * In FIN_WAIT_1 STATE in addition to the processing
			 * for the ESTABLISHED state if our FIN is now
			 * acknowledged then enter FIN_WAIT_2.
			 */
			if (ourfinisacked) {
				/*
				 * If we can't receive any more data, then
				 * closing user can proceed. Starting the
				 * timer is contrary to the specification,
				 * but if we don't get a FIN we'll hang
				 * forever.
				 *
				 * XXXjl: we should release the tp also, and
				 * use a compressed state.
				 */
				if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
					soisdisconnected(so);
					tcp_timer_activate(tp, TT_2MSL,
					    (tcp_fast_finwait2_recycle ?
					    tcp_finwait2_timeout :
					    TP_MAXIDLE(tp)));
				}
				tcp_state_change(tp, TCPS_FIN_WAIT_2);
			}
		}
	}
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	   tiwin, thflags, nxt_pkt));
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_syn_recv(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	struct tcp_rack *rack;
	int32_t orig_tlen = tlen;
	int32_t ret_val = 0;
	int32_t ourfinisacked = 0;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	ctf_calc_rwin(so, tp);
	if ((thflags & TH_RST) ||
	    (tp->t_fin_is_rst && (thflags & TH_FIN)))
		return (__ctf_process_rst(m, th, so, tp,
					  &rack->r_ctl.challenge_ack_ts,
					  &rack->r_ctl.challenge_ack_cnt));
	if ((thflags & TH_ACK) &&
	    (SEQ_LEQ(th->th_ack, tp->snd_una) ||
	    SEQ_GT(th->th_ack, tp->snd_max))) {
		tcp_log_end_status(tp, TCP_EI_STATUS_RST_IN_FRONT);
		ctf_do_dropwithreset(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
		return (1);
	}
	if (tp->t_flags & TF_FASTOPEN) {
		/*
		 * When a TFO connection is in SYN_RECEIVED, the
		 * only valid packets are the initial SYN, a
		 * retransmit/copy of the initial SYN (possibly with
		 * a subset of the original data), a valid ACK, a
		 * FIN, or a RST.
		 */
		if ((thflags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK)) {
			tcp_log_end_status(tp, TCP_EI_STATUS_RST_IN_FRONT);
			ctf_do_dropwithreset(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
			return (1);
		} else if (thflags & TH_SYN) {
			/* non-initial SYN is ignored */
			if ((rack->r_ctl.rc_hpts_flags & PACE_TMR_RXT) ||
			    (rack->r_ctl.rc_hpts_flags & PACE_TMR_TLP) ||
			    (rack->r_ctl.rc_hpts_flags & PACE_TMR_RACK)) {
				ctf_do_drop(m, NULL);
				return (0);
			}
		} else if (!(thflags & (TH_ACK | TH_FIN | TH_RST))) {
			ctf_do_drop(m, NULL);
			return (0);
		}
	}

	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment and
	 * it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {
		if (ctf_ts_check(m, th, tp, tlen, thflags, &ret_val))
			return (ret_val);
	}
	/*
	 * In the SYN-RECEIVED state, validate that the packet belongs to
	 * this connection before trimming the data to fit the receive
	 * window.  Check the sequence number versus IRS since we know the
	 * sequence numbers haven't wrapped.  This is a partial fix for the
	 * "LAND" DoS attack.
	 */
	if (SEQ_LT(th->th_seq, tp->irs)) {
		tcp_log_end_status(tp, TCP_EI_STATUS_RST_IN_FRONT);
		ctf_do_dropwithreset(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
		return (1);
	}
	if (_ctf_drop_checks(to, m, th, tp, &tlen, &thflags, &drop_hdrlen, &ret_val,
			      &rack->r_ctl.challenge_ack_ts,
			      &rack->r_ctl.challenge_ack_cnt)) {
		return (ret_val);
	}
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * its timestamp. NOTE: 1) That the test incorporates suggestions
	 * from the latest proposal of the tcplw@cray.com list (Braden
	 * 1993/04/26). 2) That updating only on newer timestamps interferes
	 * with our earlier PAWS tests, so this check should be solely
	 * predicated on the sequence space of this segment. 3) That we
	 * modify the segment boundary check to be Last.ACK.Sent <= SEG.SEQ
	 * + SEG.Len  instead of RFC1323's Last.ACK.Sent < SEG.SEQ +
	 * SEG.Len, This modified check allows us to overcome RFC1323's
	 * limitations as described in Stevens TCP/IP Illustrated Vol. 2
	 * p.869. In such cases, we can still calculate the RTT correctly
	 * when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	tp->snd_wnd = tiwin;
	rack_validate_fo_sendwin_up(tp, rack);
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN flag
	 * is on (half-synchronized state), then queue data for later
	 * processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_FASTOPEN) {
			rack_cc_conn_init(tp);
		}
		return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
		    tiwin, thflags, nxt_pkt));
	}
	KMOD_TCPSTAT_INC(tcps_connects);
	if (tp->t_flags & TF_SONOTCONN) {
		tp->t_flags &= ~TF_SONOTCONN;
		soisconnected(so);
	}
	/* Do window scaling? */
	if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
	    (TF_RCVD_SCALE | TF_REQ_SCALE)) {
		tp->rcv_scale = tp->request_r_scale;
	}
	/*
	 * Make transitions: SYN-RECEIVED  -> ESTABLISHED SYN-RECEIVED* ->
	 * FIN-WAIT-1
	 */
	tp->t_starttime = ticks;
	if ((tp->t_flags & TF_FASTOPEN) && tp->t_tfo_pending) {
		tcp_fastopen_decrement_counter(tp->t_tfo_pending);
		tp->t_tfo_pending = NULL;
	}
	if (tp->t_flags & TF_NEEDFIN) {
		tcp_state_change(tp, TCPS_FIN_WAIT_1);
		tp->t_flags &= ~TF_NEEDFIN;
	} else {
		tcp_state_change(tp, TCPS_ESTABLISHED);
		TCP_PROBE5(accept__established, NULL, tp,
		    mtod(m, const char *), tp, th);
		/*
		 * TFO connections call cc_conn_init() during SYN
		 * processing.  Calling it again here for such connections
		 * is not harmless as it would undo the snd_cwnd reduction
		 * that occurs when a TFO SYN|ACK is retransmitted.
		 */
		if (!(tp->t_flags & TF_FASTOPEN))
			rack_cc_conn_init(tp);
	}
	/*
	 * Account for the ACK of our SYN prior to
	 * regular ACK processing below, except for
	 * simultaneous SYN, which is handled later.
	 */
	if (SEQ_GT(th->th_ack, tp->snd_una) && !(tp->t_flags & TF_NEEDSYN))
		tp->snd_una++;
	/*
	 * If segment contains data or ACK, will call tcp_reass() later; if
	 * not, do so now to pass queued data to user.
	 */
	if (tlen == 0 && (thflags & TH_FIN) == 0) {
		(void) tcp_reass(tp, (struct tcphdr *)0, NULL, 0,
		    (struct mbuf *)0);
		if (tp->t_flags & TF_WAKESOR) {
			tp->t_flags &= ~TF_WAKESOR;
			/* NB: sorwakeup_locked() does an implicit unlock. */
			sorwakeup_locked(so);
		}
	}
	tp->snd_wl1 = th->th_seq - 1;
	/* For syn-recv we need to possibly update the rtt */
	if ((to->to_flags & TOF_TS) != 0 && to->to_tsecr) {
		uint32_t t, mcts;

		mcts = tcp_ts_getticks();
		t = (mcts - to->to_tsecr) * HPTS_USEC_IN_MSEC;
		if (!tp->t_rttlow || tp->t_rttlow > t)
			tp->t_rttlow = t;
		rack_log_rtt_sample_calc(rack, t, (to->to_tsecr * 1000), (mcts * 1000), 5);
		tcp_rack_xmit_timer(rack, t + 1, 1, t, 0, NULL, 2);
		tcp_rack_xmit_timer_commit(rack, tp);
	}
	if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, &ourfinisacked, thflags, &ret_val, orig_tlen)) {
		return (ret_val);
	}
	if (tp->t_state == TCPS_FIN_WAIT_1) {
		/* We could have went to FIN_WAIT_1 (or EST) above */
		/*
		 * In FIN_WAIT_1 STATE in addition to the processing for the
		 * ESTABLISHED state if our FIN is now acknowledged then
		 * enter FIN_WAIT_2.
		 */
		if (ourfinisacked) {
			/*
			 * If we can't receive any more data, then closing
			 * user can proceed. Starting the timer is contrary
			 * to the specification, but if we don't get a FIN
			 * we'll hang forever.
			 *
			 * XXXjl: we should release the tp also, and use a
			 * compressed state.
			 */
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
				soisdisconnected(so);
				tcp_timer_activate(tp, TT_2MSL,
				    (tcp_fast_finwait2_recycle ?
				    tcp_finwait2_timeout :
				    TP_MAXIDLE(tp)));
			}
			tcp_state_change(tp, TCPS_FIN_WAIT_2);
		}
	}
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	    tiwin, thflags, nxt_pkt));
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_established(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t ret_val = 0;
	int32_t orig_tlen = tlen;
	struct tcp_rack *rack;

	/*
	 * Header prediction: check for the two common cases of a
	 * uni-directional data xfer.  If the packet has no control flags,
	 * is in-sequence, the window didn't change and we're not
	 * retransmitting, it's a candidate.  If the length is zero and the
	 * ack moved forward, we're the sender side of the xfer.  Just free
	 * the data acked & wake any higher level process that was blocked
	 * waiting for space.  If the length is non-zero and the ack didn't
	 * move, we're the receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data toc The socket
	 * buffer and note that we need a delayed ack. Make sure that the
	 * hidden state-flags are also off. Since we check for
	 * TCPS_ESTABLISHED first, it can only be TH_NEEDSYN.
	 */
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (__predict_true(((to->to_flags & TOF_SACK) == 0)) &&
	    __predict_true((thflags & (TH_SYN | TH_FIN | TH_RST | TH_ACK)) == TH_ACK) &&
	    __predict_true(SEGQ_EMPTY(tp)) &&
	    __predict_true(th->th_seq == tp->rcv_nxt)) {
		if (tlen == 0) {
			if (rack_fastack(m, th, so, tp, to, drop_hdrlen, tlen,
			    tiwin, nxt_pkt, rack->r_ctl.rc_rcvtime)) {
				return (0);
			}
		} else {
			if (rack_do_fastnewdata(m, th, so, tp, to, drop_hdrlen, tlen,
			    tiwin, nxt_pkt, iptos)) {
				return (0);
			}
		}
	}
	ctf_calc_rwin(so, tp);

	if ((thflags & TH_RST) ||
	    (tp->t_fin_is_rst && (thflags & TH_FIN)))
		return (__ctf_process_rst(m, th, so, tp,
					  &rack->r_ctl.challenge_ack_ts,
					  &rack->r_ctl.challenge_ack_cnt));

	/*
	 * RFC5961 Section 4.2 Send challenge ACK for any SYN in
	 * synchronized state.
	 */
	if (thflags & TH_SYN) {
		ctf_challenge_ack(m, th, tp, iptos, &ret_val);
		return (ret_val);
	}
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment and
	 * it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {
		if (ctf_ts_check(m, th, tp, tlen, thflags, &ret_val))
			return (ret_val);
	}
	if (_ctf_drop_checks(to, m, th, tp, &tlen, &thflags, &drop_hdrlen, &ret_val,
			      &rack->r_ctl.challenge_ack_ts,
			      &rack->r_ctl.challenge_ack_cnt)) {
		return (ret_val);
	}
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * its timestamp. NOTE: 1) That the test incorporates suggestions
	 * from the latest proposal of the tcplw@cray.com list (Braden
	 * 1993/04/26). 2) That updating only on newer timestamps interferes
	 * with our earlier PAWS tests, so this check should be solely
	 * predicated on the sequence space of this segment. 3) That we
	 * modify the segment boundary check to be Last.ACK.Sent <= SEG.SEQ
	 * + SEG.Len  instead of RFC1323's Last.ACK.Sent < SEG.SEQ +
	 * SEG.Len, This modified check allows us to overcome RFC1323's
	 * limitations as described in Stevens TCP/IP Illustrated Vol. 2
	 * p.869. In such cases, we can still calculate the RTT correctly
	 * when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN flag
	 * is on (half-synchronized state), then queue data for later
	 * processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_NEEDSYN) {
			return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
			    tiwin, thflags, nxt_pkt));

		} else if (tp->t_flags & TF_ACKNOW) {
			ctf_do_dropafterack(m, tp, th, thflags, tlen, &ret_val);
			((struct tcp_rack *)tp->t_fb_ptr)->r_wanted_output = 1;
			return (ret_val);
		} else {
			ctf_do_drop(m, NULL);
			return (0);
		}
	}
	/*
	 * Ack processing.
	 */
	if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, NULL, thflags, &ret_val, orig_tlen)) {
		return (ret_val);
	}
	if (sbavail(&so->so_snd)) {
		if (ctf_progress_timeout_check(tp, true)) {
			rack_log_progress_event(rack, tp, tick, PROGRESS_DROP, __LINE__);
			ctf_do_dropwithreset_conn(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
			return (1);
		}
	}
	/* State changes only happen in rack_process_data() */
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	    tiwin, thflags, nxt_pkt));
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_close_wait(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t ret_val = 0;
	int32_t orig_tlen = tlen;
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	ctf_calc_rwin(so, tp);
	if ((thflags & TH_RST) ||
	    (tp->t_fin_is_rst && (thflags & TH_FIN)))
		return (__ctf_process_rst(m, th, so, tp,
					  &rack->r_ctl.challenge_ack_ts,
					  &rack->r_ctl.challenge_ack_cnt));
	/*
	 * RFC5961 Section 4.2 Send challenge ACK for any SYN in
	 * synchronized state.
	 */
	if (thflags & TH_SYN) {
		ctf_challenge_ack(m, th, tp, iptos, &ret_val);
		return (ret_val);
	}
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment and
	 * it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {
		if (ctf_ts_check(m, th, tp, tlen, thflags, &ret_val))
			return (ret_val);
	}
	if (_ctf_drop_checks(to, m, th, tp, &tlen, &thflags, &drop_hdrlen, &ret_val,
			      &rack->r_ctl.challenge_ack_ts,
			      &rack->r_ctl.challenge_ack_cnt)) {
		return (ret_val);
	}
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * its timestamp. NOTE: 1) That the test incorporates suggestions
	 * from the latest proposal of the tcplw@cray.com list (Braden
	 * 1993/04/26). 2) That updating only on newer timestamps interferes
	 * with our earlier PAWS tests, so this check should be solely
	 * predicated on the sequence space of this segment. 3) That we
	 * modify the segment boundary check to be Last.ACK.Sent <= SEG.SEQ
	 * + SEG.Len  instead of RFC1323's Last.ACK.Sent < SEG.SEQ +
	 * SEG.Len, This modified check allows us to overcome RFC1323's
	 * limitations as described in Stevens TCP/IP Illustrated Vol. 2
	 * p.869. In such cases, we can still calculate the RTT correctly
	 * when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN flag
	 * is on (half-synchronized state), then queue data for later
	 * processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_NEEDSYN) {
			return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
			    tiwin, thflags, nxt_pkt));

		} else if (tp->t_flags & TF_ACKNOW) {
			ctf_do_dropafterack(m, tp, th, thflags, tlen, &ret_val);
			((struct tcp_rack *)tp->t_fb_ptr)->r_wanted_output = 1;
			return (ret_val);
		} else {
			ctf_do_drop(m, NULL);
			return (0);
		}
	}
	/*
	 * Ack processing.
	 */
	if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, NULL, thflags, &ret_val, orig_tlen)) {
		return (ret_val);
	}
	if (sbavail(&so->so_snd)) {
		if (ctf_progress_timeout_check(tp, true)) {
			rack_log_progress_event((struct tcp_rack *)tp->t_fb_ptr,
						tp, tick, PROGRESS_DROP, __LINE__);
			ctf_do_dropwithreset_conn(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
			return (1);
		}
	}
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	    tiwin, thflags, nxt_pkt));
}

static int
rack_check_data_after_close(struct mbuf *m,
    struct tcpcb *tp, int32_t *tlen, struct tcphdr *th, struct socket *so)
{
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack->rc_allow_data_af_clo == 0) {
	close_now:
		tcp_log_end_status(tp, TCP_EI_STATUS_DATA_A_CLOSE);
		/* tcp_close will kill the inp pre-log the Reset */
		tcp_log_end_status(tp, TCP_EI_STATUS_SERVER_RST);
		tp = tcp_close(tp);
		KMOD_TCPSTAT_INC(tcps_rcvafterclose);
		ctf_do_dropwithreset(m, tp, th, BANDLIM_UNLIMITED, (*tlen));
		return (1);
	}
	if (sbavail(&so->so_snd) == 0)
		goto close_now;
	/* Ok we allow data that is ignored and a followup reset */
	tcp_log_end_status(tp, TCP_EI_STATUS_DATA_A_CLOSE);
	tp->rcv_nxt = th->th_seq + *tlen;
	tp->t_flags2 |= TF2_DROP_AF_DATA;
	rack->r_wanted_output = 1;
	*tlen = 0;
	return (0);
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_fin_wait_1(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t ret_val = 0;
	int32_t orig_tlen = tlen;
	int32_t ourfinisacked = 0;
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	ctf_calc_rwin(so, tp);

	if ((thflags & TH_RST) ||
	    (tp->t_fin_is_rst && (thflags & TH_FIN)))
		return (__ctf_process_rst(m, th, so, tp,
					  &rack->r_ctl.challenge_ack_ts,
					  &rack->r_ctl.challenge_ack_cnt));
	/*
	 * RFC5961 Section 4.2 Send challenge ACK for any SYN in
	 * synchronized state.
	 */
	if (thflags & TH_SYN) {
		ctf_challenge_ack(m, th, tp, iptos, &ret_val);
		return (ret_val);
	}
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment and
	 * it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {
		if (ctf_ts_check(m, th, tp, tlen, thflags, &ret_val))
			return (ret_val);
	}
	if (_ctf_drop_checks(to, m, th, tp, &tlen, &thflags, &drop_hdrlen, &ret_val,
			      &rack->r_ctl.challenge_ack_ts,
			      &rack->r_ctl.challenge_ack_cnt)) {
		return (ret_val);
	}
	/*
	 * If new data are received on a connection after the user processes
	 * are gone, then RST the other end.
	 */
	if ((tp->t_flags & TF_CLOSED) && tlen &&
	    rack_check_data_after_close(m, tp, &tlen, th, so))
		return (1);
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * its timestamp. NOTE: 1) That the test incorporates suggestions
	 * from the latest proposal of the tcplw@cray.com list (Braden
	 * 1993/04/26). 2) That updating only on newer timestamps interferes
	 * with our earlier PAWS tests, so this check should be solely
	 * predicated on the sequence space of this segment. 3) That we
	 * modify the segment boundary check to be Last.ACK.Sent <= SEG.SEQ
	 * + SEG.Len  instead of RFC1323's Last.ACK.Sent < SEG.SEQ +
	 * SEG.Len, This modified check allows us to overcome RFC1323's
	 * limitations as described in Stevens TCP/IP Illustrated Vol. 2
	 * p.869. In such cases, we can still calculate the RTT correctly
	 * when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN flag
	 * is on (half-synchronized state), then queue data for later
	 * processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_NEEDSYN) {
			return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
			    tiwin, thflags, nxt_pkt));
		} else if (tp->t_flags & TF_ACKNOW) {
			ctf_do_dropafterack(m, tp, th, thflags, tlen, &ret_val);
			((struct tcp_rack *)tp->t_fb_ptr)->r_wanted_output = 1;
			return (ret_val);
		} else {
			ctf_do_drop(m, NULL);
			return (0);
		}
	}
	/*
	 * Ack processing.
	 */
	if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, &ourfinisacked, thflags, &ret_val, orig_tlen)) {
		return (ret_val);
	}
	if (ourfinisacked) {
		/*
		 * If we can't receive any more data, then closing user can
		 * proceed. Starting the timer is contrary to the
		 * specification, but if we don't get a FIN we'll hang
		 * forever.
		 *
		 * XXXjl: we should release the tp also, and use a
		 * compressed state.
		 */
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
			soisdisconnected(so);
			tcp_timer_activate(tp, TT_2MSL,
			    (tcp_fast_finwait2_recycle ?
			    tcp_finwait2_timeout :
			    TP_MAXIDLE(tp)));
		}
		tcp_state_change(tp, TCPS_FIN_WAIT_2);
	}
	if (sbavail(&so->so_snd)) {
		if (ctf_progress_timeout_check(tp, true)) {
			rack_log_progress_event((struct tcp_rack *)tp->t_fb_ptr,
						tp, tick, PROGRESS_DROP, __LINE__);
			ctf_do_dropwithreset_conn(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
			return (1);
		}
	}
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	    tiwin, thflags, nxt_pkt));
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_closing(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t ret_val = 0;
	int32_t orig_tlen = tlen;
	int32_t ourfinisacked = 0;
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	ctf_calc_rwin(so, tp);

	if ((thflags & TH_RST) ||
	    (tp->t_fin_is_rst && (thflags & TH_FIN)))
		return (__ctf_process_rst(m, th, so, tp,
					  &rack->r_ctl.challenge_ack_ts,
					  &rack->r_ctl.challenge_ack_cnt));
	/*
	 * RFC5961 Section 4.2 Send challenge ACK for any SYN in
	 * synchronized state.
	 */
	if (thflags & TH_SYN) {
		ctf_challenge_ack(m, th, tp, iptos, &ret_val);
		return (ret_val);
	}
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment and
	 * it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {
		if (ctf_ts_check(m, th, tp, tlen, thflags, &ret_val))
			return (ret_val);
	}
	if (_ctf_drop_checks(to, m, th, tp, &tlen, &thflags, &drop_hdrlen, &ret_val,
			      &rack->r_ctl.challenge_ack_ts,
			      &rack->r_ctl.challenge_ack_cnt)) {
		return (ret_val);
	}
	/*
	 * If new data are received on a connection after the user processes
	 * are gone, then RST the other end.
	 */
	if ((tp->t_flags & TF_CLOSED) && tlen &&
	    rack_check_data_after_close(m, tp, &tlen, th, so))
		return (1);
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * its timestamp. NOTE: 1) That the test incorporates suggestions
	 * from the latest proposal of the tcplw@cray.com list (Braden
	 * 1993/04/26). 2) That updating only on newer timestamps interferes
	 * with our earlier PAWS tests, so this check should be solely
	 * predicated on the sequence space of this segment. 3) That we
	 * modify the segment boundary check to be Last.ACK.Sent <= SEG.SEQ
	 * + SEG.Len  instead of RFC1323's Last.ACK.Sent < SEG.SEQ +
	 * SEG.Len, This modified check allows us to overcome RFC1323's
	 * limitations as described in Stevens TCP/IP Illustrated Vol. 2
	 * p.869. In such cases, we can still calculate the RTT correctly
	 * when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN flag
	 * is on (half-synchronized state), then queue data for later
	 * processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_NEEDSYN) {
			return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
			    tiwin, thflags, nxt_pkt));
		} else if (tp->t_flags & TF_ACKNOW) {
			ctf_do_dropafterack(m, tp, th, thflags, tlen, &ret_val);
			((struct tcp_rack *)tp->t_fb_ptr)->r_wanted_output = 1;
			return (ret_val);
		} else {
			ctf_do_drop(m, NULL);
			return (0);
		}
	}
	/*
	 * Ack processing.
	 */
	if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, &ourfinisacked, thflags, &ret_val, orig_tlen)) {
		return (ret_val);
	}
	if (ourfinisacked) {
		tcp_twstart(tp);
		m_freem(m);
		return (1);
	}
	if (sbavail(&so->so_snd)) {
		if (ctf_progress_timeout_check(tp, true)) {
			rack_log_progress_event((struct tcp_rack *)tp->t_fb_ptr,
						tp, tick, PROGRESS_DROP, __LINE__);
			ctf_do_dropwithreset_conn(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
			return (1);
		}
	}
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	    tiwin, thflags, nxt_pkt));
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_lastack(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t ret_val = 0;
	int32_t orig_tlen;
	int32_t ourfinisacked = 0;
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	ctf_calc_rwin(so, tp);

	if ((thflags & TH_RST) ||
	    (tp->t_fin_is_rst && (thflags & TH_FIN)))
		return (__ctf_process_rst(m, th, so, tp,
					  &rack->r_ctl.challenge_ack_ts,
					  &rack->r_ctl.challenge_ack_cnt));
	/*
	 * RFC5961 Section 4.2 Send challenge ACK for any SYN in
	 * synchronized state.
	 */
	if (thflags & TH_SYN) {
		ctf_challenge_ack(m, th, tp, iptos, &ret_val);
		return (ret_val);
	}
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment and
	 * it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {
		if (ctf_ts_check(m, th, tp, tlen, thflags, &ret_val))
			return (ret_val);
	}
	orig_tlen = tlen;
	if (_ctf_drop_checks(to, m, th, tp, &tlen, &thflags, &drop_hdrlen, &ret_val,
			      &rack->r_ctl.challenge_ack_ts,
			      &rack->r_ctl.challenge_ack_cnt)) {
		return (ret_val);
	}
	/*
	 * If new data are received on a connection after the user processes
	 * are gone, then RST the other end.
	 */
	if ((tp->t_flags & TF_CLOSED) && tlen &&
	    rack_check_data_after_close(m, tp, &tlen, th, so))
		return (1);
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * its timestamp. NOTE: 1) That the test incorporates suggestions
	 * from the latest proposal of the tcplw@cray.com list (Braden
	 * 1993/04/26). 2) That updating only on newer timestamps interferes
	 * with our earlier PAWS tests, so this check should be solely
	 * predicated on the sequence space of this segment. 3) That we
	 * modify the segment boundary check to be Last.ACK.Sent <= SEG.SEQ
	 * + SEG.Len  instead of RFC1323's Last.ACK.Sent < SEG.SEQ +
	 * SEG.Len, This modified check allows us to overcome RFC1323's
	 * limitations as described in Stevens TCP/IP Illustrated Vol. 2
	 * p.869. In such cases, we can still calculate the RTT correctly
	 * when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN flag
	 * is on (half-synchronized state), then queue data for later
	 * processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_NEEDSYN) {
			return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
			    tiwin, thflags, nxt_pkt));
		} else if (tp->t_flags & TF_ACKNOW) {
			ctf_do_dropafterack(m, tp, th, thflags, tlen, &ret_val);
			((struct tcp_rack *)tp->t_fb_ptr)->r_wanted_output = 1;
			return (ret_val);
		} else {
			ctf_do_drop(m, NULL);
			return (0);
		}
	}
	/*
	 * case TCPS_LAST_ACK: Ack processing.
	 */
	if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, &ourfinisacked, thflags, &ret_val, orig_tlen)) {
		return (ret_val);
	}
	if (ourfinisacked) {
		tp = tcp_close(tp);
		ctf_do_drop(m, tp);
		return (1);
	}
	if (sbavail(&so->so_snd)) {
		if (ctf_progress_timeout_check(tp, true)) {
			rack_log_progress_event((struct tcp_rack *)tp->t_fb_ptr,
						tp, tick, PROGRESS_DROP, __LINE__);
			ctf_do_dropwithreset_conn(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
			return (1);
		}
	}
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	    tiwin, thflags, nxt_pkt));
}

/*
 * Return value of 1, the TCB is unlocked and most
 * likely gone, return value of 0, the TCP is still
 * locked.
 */
static int
rack_do_fin_wait_2(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, struct tcpopt *to, int32_t drop_hdrlen, int32_t tlen,
    uint32_t tiwin, int32_t thflags, int32_t nxt_pkt, uint8_t iptos)
{
	int32_t ret_val = 0;
	int32_t orig_tlen = tlen;
	int32_t ourfinisacked = 0;
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	ctf_calc_rwin(so, tp);

	/* Reset receive buffer auto scaling when not in bulk receive mode. */
	if ((thflags & TH_RST) ||
	    (tp->t_fin_is_rst && (thflags & TH_FIN)))
		return (__ctf_process_rst(m, th, so, tp,
					  &rack->r_ctl.challenge_ack_ts,
					  &rack->r_ctl.challenge_ack_cnt));
	/*
	 * RFC5961 Section 4.2 Send challenge ACK for any SYN in
	 * synchronized state.
	 */
	if (thflags & TH_SYN) {
		ctf_challenge_ack(m, th, tp, iptos, &ret_val);
		return (ret_val);
	}
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment and
	 * it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {
		if (ctf_ts_check(m, th, tp, tlen, thflags, &ret_val))
			return (ret_val);
	}
	if (_ctf_drop_checks(to, m, th, tp, &tlen, &thflags, &drop_hdrlen, &ret_val,
			      &rack->r_ctl.challenge_ack_ts,
			      &rack->r_ctl.challenge_ack_cnt)) {
		return (ret_val);
	}
	/*
	 * If new data are received on a connection after the user processes
	 * are gone, then RST the other end.
	 */
	if ((tp->t_flags & TF_CLOSED) && tlen &&
	    rack_check_data_after_close(m, tp, &tlen, th, so))
		return (1);
	/*
	 * If last ACK falls within this segment's sequence numbers, record
	 * its timestamp. NOTE: 1) That the test incorporates suggestions
	 * from the latest proposal of the tcplw@cray.com list (Braden
	 * 1993/04/26). 2) That updating only on newer timestamps interferes
	 * with our earlier PAWS tests, so this check should be solely
	 * predicated on the sequence space of this segment. 3) That we
	 * modify the segment boundary check to be Last.ACK.Sent <= SEG.SEQ
	 * + SEG.Len  instead of RFC1323's Last.ACK.Sent < SEG.SEQ +
	 * SEG.Len, This modified check allows us to overcome RFC1323's
	 * limitations as described in Stevens TCP/IP Illustrated Vol. 2
	 * p.869. In such cases, we can still calculate the RTT correctly
	 * when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN flag
	 * is on (half-synchronized state), then queue data for later
	 * processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_NEEDSYN) {
			return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
			    tiwin, thflags, nxt_pkt));
		} else if (tp->t_flags & TF_ACKNOW) {
			ctf_do_dropafterack(m, tp, th, thflags, tlen, &ret_val);
			((struct tcp_rack *)tp->t_fb_ptr)->r_wanted_output = 1;
			return (ret_val);
		} else {
			ctf_do_drop(m, NULL);
			return (0);
		}
	}
	/*
	 * Ack processing.
	 */
	if (rack_process_ack(m, th, so, tp, to, tiwin, tlen, &ourfinisacked, thflags, &ret_val, orig_tlen)) {
		return (ret_val);
	}
	if (sbavail(&so->so_snd)) {
		if (ctf_progress_timeout_check(tp, true)) {
			rack_log_progress_event((struct tcp_rack *)tp->t_fb_ptr,
						tp, tick, PROGRESS_DROP, __LINE__);
			ctf_do_dropwithreset_conn(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
			return (1);
		}
	}
	return (rack_process_data(m, th, so, tp, drop_hdrlen, tlen,
	    tiwin, thflags, nxt_pkt));
}

static void inline
rack_clear_rate_sample(struct tcp_rack *rack)
{
	rack->r_ctl.rack_rs.rs_flags = RACK_RTT_EMPTY;
	rack->r_ctl.rack_rs.rs_rtt_cnt = 0;
	rack->r_ctl.rack_rs.rs_rtt_tot = 0;
}

static void
rack_set_pace_segments(struct tcpcb *tp, struct tcp_rack *rack, uint32_t line, uint64_t *fill_override)
{
	uint64_t bw_est, rate_wanted;
	int chged = 0;
	uint32_t user_max, orig_min, orig_max;

#ifdef TCP_REQUEST_TRK
	if (rack->rc_hybrid_mode &&
	    (rack->r_ctl.rc_pace_max_segs != 0) &&
	    (rack_hybrid_allow_set_maxseg == 1) &&
	    (rack->r_ctl.rc_last_sft != NULL)) {
		rack->r_ctl.rc_last_sft->hybrid_flags &= ~TCP_HYBRID_PACING_SETMSS;
		return;
	}
#endif
	orig_min = rack->r_ctl.rc_pace_min_segs;
	orig_max = rack->r_ctl.rc_pace_max_segs;
	user_max = ctf_fixed_maxseg(tp) * rack->rc_user_set_max_segs;
	if (ctf_fixed_maxseg(tp) != rack->r_ctl.rc_pace_min_segs)
		chged = 1;
	rack->r_ctl.rc_pace_min_segs = ctf_fixed_maxseg(tp);
	if (rack->use_fixed_rate || rack->rc_force_max_seg) {
		if (user_max != rack->r_ctl.rc_pace_max_segs)
			chged = 1;
	}
	if (rack->rc_force_max_seg) {
		rack->r_ctl.rc_pace_max_segs = user_max;
	} else if (rack->use_fixed_rate) {
		bw_est = rack_get_bw(rack);
		if ((rack->r_ctl.crte == NULL) ||
		    (bw_est != rack->r_ctl.crte->rate)) {
			rack->r_ctl.rc_pace_max_segs = user_max;
		} else {
			/* We are pacing right at the hardware rate */
			uint32_t segsiz, pace_one;

			if (rack_pace_one_seg ||
			    (rack->r_ctl.rc_user_set_min_segs == 1))
				pace_one = 1;
			else
				pace_one = 0;
			segsiz = min(ctf_fixed_maxseg(tp),
				     rack->r_ctl.rc_pace_min_segs);
			rack->r_ctl.rc_pace_max_segs = tcp_get_pacing_burst_size_w_divisor(
				tp, bw_est, segsiz, pace_one,
				rack->r_ctl.crte, NULL, rack->r_ctl.pace_len_divisor);
		}
	} else if (rack->rc_always_pace) {
		if (rack->r_ctl.gp_bw ||
		    rack->r_ctl.init_rate) {
			/* We have a rate of some sort set */
			uint32_t  orig;

			bw_est = rack_get_bw(rack);
			orig = rack->r_ctl.rc_pace_max_segs;
			if (fill_override)
				rate_wanted = *fill_override;
			else
				rate_wanted = rack_get_gp_est(rack);
			if (rate_wanted) {
				/* We have something */
				rack->r_ctl.rc_pace_max_segs = rack_get_pacing_len(rack,
										   rate_wanted,
										   ctf_fixed_maxseg(rack->rc_tp));
			} else
				rack->r_ctl.rc_pace_max_segs = rack->r_ctl.rc_pace_min_segs;
			if (orig != rack->r_ctl.rc_pace_max_segs)
				chged = 1;
		} else if ((rack->r_ctl.gp_bw == 0) &&
			   (rack->r_ctl.rc_pace_max_segs == 0)) {
			/*
			 * If we have nothing limit us to bursting
			 * out IW sized pieces.
			 */
			chged = 1;
			rack->r_ctl.rc_pace_max_segs = rc_init_window(rack);
		}
	}
	if (rack->r_ctl.rc_pace_max_segs > PACE_MAX_IP_BYTES) {
		chged = 1;
		rack->r_ctl.rc_pace_max_segs = PACE_MAX_IP_BYTES;
	}
	if (chged)
		rack_log_type_pacing_sizes(tp, rack, orig_min, orig_max, line, 2);
}


static void
rack_init_fsb_block(struct tcpcb *tp, struct tcp_rack *rack, int32_t flags)
{
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
#ifdef INET
	struct ip *ip = NULL;
#endif
	struct udphdr *udp = NULL;

	/* Ok lets fill in the fast block, it can only be used with no IP options! */
#ifdef INET6
	if (rack->r_is_v6) {
		rack->r_ctl.fsb.tcp_ip_hdr_len = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
		ip6 = (struct ip6_hdr *)rack->r_ctl.fsb.tcp_ip_hdr;
		if (tp->t_port) {
			rack->r_ctl.fsb.tcp_ip_hdr_len += sizeof(struct udphdr);
			udp = (struct udphdr *)((caddr_t)ip6 + sizeof(struct ip6_hdr));
			udp->uh_sport = htons(V_tcp_udp_tunneling_port);
			udp->uh_dport = tp->t_port;
			rack->r_ctl.fsb.udp = udp;
			rack->r_ctl.fsb.th = (struct tcphdr *)(udp + 1);
		} else
		{
			rack->r_ctl.fsb.th = (struct tcphdr *)(ip6 + 1);
			rack->r_ctl.fsb.udp = NULL;
		}
		tcpip_fillheaders(rack->rc_inp,
				  tp->t_port,
				  ip6, rack->r_ctl.fsb.th);
		rack->r_ctl.fsb.hoplimit = in6_selecthlim(rack->rc_inp, NULL);
	} else
#endif				/* INET6 */
#ifdef INET
	{
		rack->r_ctl.fsb.tcp_ip_hdr_len = sizeof(struct tcpiphdr);
		ip = (struct ip *)rack->r_ctl.fsb.tcp_ip_hdr;
		if (tp->t_port) {
			rack->r_ctl.fsb.tcp_ip_hdr_len += sizeof(struct udphdr);
			udp = (struct udphdr *)((caddr_t)ip + sizeof(struct ip));
			udp->uh_sport = htons(V_tcp_udp_tunneling_port);
			udp->uh_dport = tp->t_port;
			rack->r_ctl.fsb.udp = udp;
			rack->r_ctl.fsb.th = (struct tcphdr *)(udp + 1);
		} else
		{
			rack->r_ctl.fsb.udp = NULL;
			rack->r_ctl.fsb.th = (struct tcphdr *)(ip + 1);
		}
		tcpip_fillheaders(rack->rc_inp,
				  tp->t_port,
				  ip, rack->r_ctl.fsb.th);
		rack->r_ctl.fsb.hoplimit = tptoinpcb(tp)->inp_ip_ttl;
	}
#endif
	rack->r_ctl.fsb.recwin = lmin(lmax(sbspace(&tptosocket(tp)->so_rcv), 0),
	    (long)TCP_MAXWIN << tp->rcv_scale);
	rack->r_fsb_inited = 1;
}

static int
rack_init_fsb(struct tcpcb *tp, struct tcp_rack *rack)
{
	/*
	 * Allocate the larger of spaces V6 if available else just
	 * V4 and include udphdr (overbook)
	 */
#ifdef INET6
	rack->r_ctl.fsb.tcp_ip_hdr_len = sizeof(struct ip6_hdr) + sizeof(struct tcphdr) + sizeof(struct udphdr);
#else
	rack->r_ctl.fsb.tcp_ip_hdr_len = sizeof(struct tcpiphdr) + sizeof(struct udphdr);
#endif
	rack->r_ctl.fsb.tcp_ip_hdr = malloc(rack->r_ctl.fsb.tcp_ip_hdr_len,
					    M_TCPFSB, M_NOWAIT|M_ZERO);
	if (rack->r_ctl.fsb.tcp_ip_hdr == NULL) {
		return (ENOMEM);
	}
	rack->r_fsb_inited = 0;
	return (0);
}

static void
rack_log_hystart_event(struct tcp_rack *rack, uint32_t high_seq, uint8_t mod)
{
	/*
	 * Types of logs (mod value)
	 * 20 - Initial round setup
	 * 21 - Rack declares a new round.
	 */
	struct tcpcb *tp;

	tp = rack->rc_tp;
	if (tcp_bblogging_on(tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = rack->r_ctl.current_round;
		log.u_bbr.flex2 = rack->r_ctl.roundends;
		log.u_bbr.flex3 = high_seq;
		log.u_bbr.flex4 = tp->snd_max;
		log.u_bbr.flex8 = mod;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.cur_del_rate = rack->rc_tp->t_sndbytes;
		log.u_bbr.delRate = rack->rc_tp->t_snd_rxt_bytes;
		TCP_LOG_EVENTP(tp, NULL,
		    &tptosocket(tp)->so_rcv,
		    &tptosocket(tp)->so_snd,
		    TCP_HYSTART, 0,
		    0, &log, false, &tv);
	}
}

static void
rack_deferred_init(struct tcpcb *tp, struct tcp_rack *rack)
{
	rack->rack_deferred_inited = 1;
	rack->r_ctl.roundends = tp->snd_max;
	rack->r_ctl.rc_high_rwnd = tp->snd_wnd;
	rack->r_ctl.cwnd_to_use = tp->snd_cwnd;
}

static void
rack_init_retransmit_value(struct tcp_rack *rack, int ctl)
{
	/* Retransmit bit controls.
	 *
	 * The setting of these values control one of
	 * three settings you can have and dictate
	 * how rack does retransmissions. Note this
	 * is in *any* mode i.e. pacing on or off DGP
	 * fixed rate pacing, or just bursting rack.
	 *
	 * 1 - Use full sized retransmits i.e. limit
	 *     the size to whatever the pace_max_segments
	 *     size is.
	 *
	 * 2 - Use pacer min granularity as a guide to
	 *     the size combined with the current calculated
	 *     goodput b/w measurement. So for example if
	 *     the goodput is measured at 20Mbps we would
	 *     calculate 8125 (pacer minimum 250usec in
	 *     that b/w) and then round it up to the next
	 *     MSS i.e. for 1448 mss 6 MSS or 8688 bytes.
	 *
	 * 0 - The rack default 1 MSS (anything not 0/1/2
	 *     fall here too if we are setting via rack_init()).
	 *
	 */
	if (ctl == 1) {
		rack->full_size_rxt = 1;
		rack->shape_rxt_to_pacing_min  = 0;
	} else if (ctl == 2) {
		rack->full_size_rxt = 0;
		rack->shape_rxt_to_pacing_min  = 1;
	} else {
		rack->full_size_rxt = 0;
		rack->shape_rxt_to_pacing_min  = 0;
	}
}

static void
rack_log_chg_info(struct tcpcb *tp, struct tcp_rack *rack, uint8_t mod,
		  uint32_t flex1,
		  uint32_t flex2,
		  uint32_t flex3)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex8 = mod;
		log.u_bbr.flex1 = flex1;
		log.u_bbr.flex2 = flex2;
		log.u_bbr.flex3 = flex3;
		tcp_log_event(tp, NULL, NULL, NULL, TCP_CHG_QUERY, 0,
			       0, &log, false, NULL, __func__, __LINE__, &tv);
	}
}

static int
rack_chg_query(struct tcpcb *tp, struct tcp_query_resp *reqr)
{
	struct tcp_rack *rack;
	struct rack_sendmap *rsm;
	int i;


	rack = (struct tcp_rack *)tp->t_fb_ptr;
	switch (reqr->req) {
	case TCP_QUERY_SENDMAP:
		if ((reqr->req_param == tp->snd_max) ||
		    (tp->snd_max == tp->snd_una)){
			/* Unlikely */
			return (0);
		}
		rsm = tqhash_find(rack->r_ctl.tqh, reqr->req_param);
		if (rsm == NULL) {
			/* Can't find that seq -- unlikely */
			return (0);
		}
		reqr->sendmap_start = rsm->r_start;
		reqr->sendmap_end = rsm->r_end;
		reqr->sendmap_send_cnt = rsm->r_rtr_cnt;
		reqr->sendmap_fas = rsm->r_fas;
		if (reqr->sendmap_send_cnt > SNDMAP_NRTX)
			reqr->sendmap_send_cnt = SNDMAP_NRTX;
		for(i=0; i<reqr->sendmap_send_cnt; i++)
			reqr->sendmap_time[i] = rsm->r_tim_lastsent[i];
		reqr->sendmap_ack_arrival = rsm->r_ack_arrival;
		reqr->sendmap_flags = rsm->r_flags & SNDMAP_MASK;
		reqr->sendmap_r_rtr_bytes = rsm->r_rtr_bytes;
		reqr->sendmap_dupacks = rsm->r_dupack;
		rack_log_chg_info(tp, rack, 1,
				  rsm->r_start,
				  rsm->r_end,
				  rsm->r_flags);
		return(1);
		break;
	case TCP_QUERY_TIMERS_UP:
		if (rack->r_ctl.rc_hpts_flags == 0) {
			/* no timers up */
			return (0);
		}
		reqr->timer_hpts_flags = rack->r_ctl.rc_hpts_flags;
		if (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) {
			reqr->timer_pacing_to = rack->r_ctl.rc_last_output_to;
		}
		if (rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK) {
			reqr->timer_timer_exp = rack->r_ctl.rc_timer_exp;
		}
		rack_log_chg_info(tp, rack, 2,
				  rack->r_ctl.rc_hpts_flags,
				  rack->r_ctl.rc_last_output_to,
				  rack->r_ctl.rc_timer_exp);
		return (1);
		break;
	case TCP_QUERY_RACK_TIMES:
		/* Reordering items */
		reqr->rack_num_dsacks = rack->r_ctl.num_dsack;
		reqr->rack_reorder_ts = rack->r_ctl.rc_reorder_ts;
		/* Timerstamps and timers */
		reqr->rack_rxt_last_time = rack->r_ctl.rc_tlp_rxt_last_time;
		reqr->rack_min_rtt = rack->r_ctl.rc_rack_min_rtt;
		reqr->rack_rtt = rack->rc_rack_rtt;
		reqr->rack_tmit_time = rack->r_ctl.rc_rack_tmit_time;
		reqr->rack_srtt_measured = rack->rc_srtt_measure_made;
		/* PRR data */
		reqr->rack_sacked = rack->r_ctl.rc_sacked;
		reqr->rack_holes_rxt = rack->r_ctl.rc_holes_rxt;
		reqr->rack_prr_delivered = rack->r_ctl.rc_prr_delivered;
		reqr->rack_prr_recovery_fs = rack->r_ctl.rc_prr_recovery_fs;
		reqr->rack_prr_sndcnt = rack->r_ctl.rc_prr_sndcnt;
		reqr->rack_prr_out = rack->r_ctl.rc_prr_out;
		/* TLP and persists info */
		reqr->rack_tlp_out = rack->rc_tlp_in_progress;
		reqr->rack_tlp_cnt_out = rack->r_ctl.rc_tlp_cnt_out;
		if (rack->rc_in_persist) {
			reqr->rack_time_went_idle = rack->r_ctl.rc_went_idle_time;
			reqr->rack_in_persist = 1;
		} else {
			reqr->rack_time_went_idle = 0;
			reqr->rack_in_persist = 0;
		}
		if (rack->r_wanted_output)
			reqr->rack_wanted_output = 1;
		else
			reqr->rack_wanted_output = 0;
		return (1);
		break;
	default:
		return (-EINVAL);
	}
}

static void
rack_switch_failed(struct tcpcb *tp)
{
	/*
	 * This method gets called if a stack switch was
	 * attempted and it failed. We are left
	 * but our hpts timers were stopped and we
	 * need to validate time units and t_flags2.
	 */
	struct tcp_rack *rack;
	struct timeval tv;
	uint32_t cts;
	uint32_t toval;
	struct hpts_diag diag;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	tcp_change_time_units(tp, TCP_TMR_GRANULARITY_USEC);
	if  (rack->r_mbuf_queue || rack->rc_always_pace || rack->r_use_cmp_ack)
		tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
	else
		tp->t_flags2 &= ~TF2_SUPPORTS_MBUFQ;
	if (rack->r_use_cmp_ack && TCPS_HAVEESTABLISHED(tp->t_state))
		tp->t_flags2 |= TF2_MBUF_ACKCMP;
	if (tp->t_in_hpts > IHPTS_NONE) {
		/* Strange */
		return;
	}
	cts = tcp_get_usecs(&tv);
	if (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) {
		if (TSTMP_GT(rack->r_ctl.rc_last_output_to, cts)) {
			toval = rack->r_ctl.rc_last_output_to - cts;
		} else {
			/* one slot please */
			toval = HPTS_TICKS_PER_SLOT;
		}
	} else if (rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK) {
		if (TSTMP_GT(rack->r_ctl.rc_timer_exp, cts)) {
			toval = rack->r_ctl.rc_timer_exp - cts;
		} else {
			/* one slot please */
			toval = HPTS_TICKS_PER_SLOT;
		}
	} else
		toval = HPTS_TICKS_PER_SLOT;
	(void)tcp_hpts_insert_diag(tp, HPTS_USEC_TO_SLOTS(toval),
				   __LINE__, &diag);
	rack_log_hpts_diag(rack, cts, &diag, &tv);
}

static int
rack_init_outstanding(struct tcpcb *tp, struct tcp_rack *rack, uint32_t us_cts, void *ptr)
{
	struct rack_sendmap *rsm, *ersm;
	int insret __diagused;
	/*
	 * When initing outstanding, we must be quite careful
	 * to not refer to tp->t_fb_ptr. This has the old rack
	 * pointer in it, not the "new" one (when we are doing
	 * a stack switch).
	 */


	if (tp->t_fb->tfb_chg_query == NULL) {
		/* Create a send map for the current outstanding data */

		rsm = rack_alloc(rack);
		if (rsm == NULL) {
			uma_zfree(rack_pcb_zone, ptr);
			return (ENOMEM);
		}
		rsm->r_no_rtt_allowed = 1;
		rsm->r_tim_lastsent[0] = rack_to_usec_ts(&rack->r_ctl.act_rcv_time);
		rsm->r_rtr_cnt = 1;
		rsm->r_rtr_bytes = 0;
		if (tp->t_flags & TF_SENTFIN)
			rsm->r_flags |= RACK_HAS_FIN;
		rsm->r_end = tp->snd_max;
		if (tp->snd_una == tp->iss) {
			/* The data space is one beyond snd_una */
			rsm->r_flags |= RACK_HAS_SYN;
			rsm->r_start = tp->iss;
			rsm->r_end = rsm->r_start + (tp->snd_max - tp->snd_una);
		} else
			rsm->r_start = tp->snd_una;
		rsm->r_dupack = 0;
		if (rack->rc_inp->inp_socket->so_snd.sb_mb != NULL) {
			rsm->m = sbsndmbuf(&rack->rc_inp->inp_socket->so_snd, 0, &rsm->soff);
			if (rsm->m) {
				rsm->orig_m_len = rsm->m->m_len;
				rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
			} else {
				rsm->orig_m_len = 0;
				rsm->orig_t_space = 0;
			}
		} else {
			/*
			 * This can happen if we have a stand-alone FIN or
			 *  SYN.
			 */
			rsm->m = NULL;
			rsm->orig_m_len = 0;
			rsm->orig_t_space = 0;
			rsm->soff = 0;
		}
#ifdef INVARIANTS
		if ((insret = tqhash_insert(rack->r_ctl.tqh, rsm)) != 0) {
			panic("Insert in tailq_hash fails ret:%d rack:%p rsm:%p",
			      insret, rack, rsm);
		}
#else
		(void)tqhash_insert(rack->r_ctl.tqh, rsm);
#endif
		TAILQ_INSERT_TAIL(&rack->r_ctl.rc_tmap, rsm, r_tnext);
		rsm->r_in_tmap = 1;
	} else {
		/* We have a query mechanism, lets use it */
		struct tcp_query_resp qr;
		int i;
		tcp_seq at;

		at = tp->snd_una;
		while (at != tp->snd_max) {
			memset(&qr, 0, sizeof(qr));
			qr.req = TCP_QUERY_SENDMAP;
			qr.req_param = at;
			if ((*tp->t_fb->tfb_chg_query)(tp, &qr) == 0)
				break;
			/* Move forward */
			at = qr.sendmap_end;
			/* Now lets build the entry for this one */
			rsm = rack_alloc(rack);
			if (rsm == NULL) {
				uma_zfree(rack_pcb_zone, ptr);
				return (ENOMEM);
			}
			memset(rsm, 0, sizeof(struct rack_sendmap));
			/* Now configure the rsm and insert it */
			rsm->r_dupack = qr.sendmap_dupacks;
			rsm->r_start = qr.sendmap_start;
			rsm->r_end = qr.sendmap_end;
			if (qr.sendmap_fas)
				rsm->r_fas = qr.sendmap_end;
			else
				rsm->r_fas = rsm->r_start - tp->snd_una;
			/*
			 * We have carefully aligned the bits
			 * so that all we have to do is copy over
			 * the bits with the mask.
			 */
			rsm->r_flags = qr.sendmap_flags & SNDMAP_MASK;
			rsm->r_rtr_bytes = qr.sendmap_r_rtr_bytes;
			rsm->r_rtr_cnt = qr.sendmap_send_cnt;
			rsm->r_ack_arrival = qr.sendmap_ack_arrival;
			for (i=0 ; i<rsm->r_rtr_cnt; i++)
				rsm->r_tim_lastsent[i]	= qr.sendmap_time[i];
			rsm->m = sbsndmbuf(&rack->rc_inp->inp_socket->so_snd,
					   (rsm->r_start - tp->snd_una), &rsm->soff);
			if (rsm->m) {
				rsm->orig_m_len = rsm->m->m_len;
				rsm->orig_t_space = M_TRAILINGROOM(rsm->m);
			} else {
				rsm->orig_m_len = 0;
				rsm->orig_t_space = 0;
			}
#ifdef INVARIANTS
			if ((insret = tqhash_insert(rack->r_ctl.tqh, rsm)) != 0) {
				panic("Insert in tailq_hash fails ret:%d rack:%p rsm:%p",
				      insret, rack, rsm);
			}
#else
			(void)tqhash_insert(rack->r_ctl.tqh, rsm);
#endif
			if ((rsm->r_flags & RACK_ACKED) == 0)  {
				TAILQ_FOREACH(ersm, &rack->r_ctl.rc_tmap, r_tnext) {
					if (ersm->r_tim_lastsent[(ersm->r_rtr_cnt-1)] >
					    rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)]) {
						/*
						 * If the existing ersm was sent at
						 * a later time than the new one, then
						 * the new one should appear ahead of this
						 * ersm.
						 */
						rsm->r_in_tmap = 1;
						TAILQ_INSERT_BEFORE(ersm, rsm, r_tnext);
						break;
					}
				}
				if (rsm->r_in_tmap == 0) {
					/*
					 * Not found so shove it on the tail.
					 */
					TAILQ_INSERT_TAIL(&rack->r_ctl.rc_tmap, rsm, r_tnext);
					rsm->r_in_tmap = 1;
				}
 			} else {
				if ((rack->r_ctl.rc_sacklast == NULL) ||
				    (SEQ_GT(rsm->r_end, rack->r_ctl.rc_sacklast->r_end))) {
					rack->r_ctl.rc_sacklast = rsm;
				}
			}
			rack_log_chg_info(tp, rack, 3,
					  rsm->r_start,
					  rsm->r_end,
					  rsm->r_flags);
		}
	}
	return (0);
}

static void
rack_translate_policer_detect(struct tcp_rack *rack, uint32_t optval)
{
	/*
	 * P = Percent of retransmits 499 = 49.9%
	 * A = Average number 1 (.1%) -> 169 (16.9%)
	 * M = Median number of retrans 1 - 16
	 * MMMM MMMM AAAA AAAA PPPP PPPP PPPP PPPP
	 *
	 */
	uint16_t per, upp;

	per = optval & 0x0000ffff;
	rack->r_ctl.policer_rxt_threshold = (uint32_t)(per & 0xffff);
	upp = ((optval & 0xffff0000) >> 16);
	rack->r_ctl.policer_avg_threshold = (0x00ff & upp);
	rack->r_ctl.policer_med_threshold = ((upp >> 8) & 0x00ff);
	if ((rack->r_ctl.policer_rxt_threshold > 0) &&
	    (rack->r_ctl.policer_avg_threshold > 0) &&
	    (rack->r_ctl.policer_med_threshold > 0)) {
		rack->policer_detect_on = 1;
	} else {
		rack->policer_detect_on = 0;
	}
	rack->r_ctl.saved_policer_val = optval;
	policer_detection_log(rack, optval,
			      rack->r_ctl.policer_avg_threshold,
			      rack->r_ctl.policer_med_threshold,
			      rack->r_ctl.policer_rxt_threshold, 11);
}

static int32_t
rack_init(struct tcpcb *tp, void **ptr)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct tcp_rack *rack = NULL;
	uint32_t iwin, snt, us_cts;
	size_t sz;
	int err, no_query;

	tcp_hpts_init(tp);

	/*
	 * First are we the initial or are we a switched stack?
	 * If we are initing via tcp_newtcppcb the ptr passed
	 * will be tp->t_fb_ptr. If its a stack switch that
	 * has a previous stack we can query it will be a local
	 * var that will in the end be set into t_fb_ptr.
	 */
	if (ptr == &tp->t_fb_ptr)
		no_query = 1;
	else
		no_query = 0;
	*ptr = uma_zalloc(rack_pcb_zone, M_NOWAIT);
	if (*ptr == NULL) {
		/*
		 * We need to allocate memory but cant. The INP and INP_INFO
		 * locks and they are recursive (happens during setup. So a
		 * scheme to drop the locks fails :(
		 *
		 */
		return(ENOMEM);
	}
	memset(*ptr, 0, sizeof(struct tcp_rack));
	rack = (struct tcp_rack *)*ptr;
	rack->r_ctl.tqh = malloc(sizeof(struct tailq_hash), M_TCPFSB, M_NOWAIT);
	if (rack->r_ctl.tqh == NULL) {
		uma_zfree(rack_pcb_zone, rack);
		return(ENOMEM);
	}
	tqhash_init(rack->r_ctl.tqh);
	TAILQ_INIT(&rack->r_ctl.rc_free);
	TAILQ_INIT(&rack->r_ctl.rc_tmap);
	rack->rc_tp = tp;
	rack->rc_inp = inp;
	/* Set the flag */
	rack->r_is_v6 = (inp->inp_vflag & INP_IPV6) != 0;
	/* Probably not needed but lets be sure */
	rack_clear_rate_sample(rack);
	/*
	 * Save off the default values, socket options will poke
	 * at these if pacing is not on or we have not yet
	 * reached where pacing is on (gp_ready/fixed enabled).
	 * When they get set into the CC module (when gp_ready
	 * is enabled or we enable fixed) then we will set these
	 * values into the CC and place in here the old values
	 * so we have a restoral. Then we will set the flag
	 * rc_pacing_cc_set. That way whenever we turn off pacing
	 * or switch off this stack, we will know to go restore
	 * the saved values.
	 *
	 * We specifically put into the beta the ecn value for pacing.
	 */
	rack->rc_new_rnd_needed = 1;
	rack->r_ctl.rc_split_limit = V_tcp_map_split_limit;
	/* We want abe like behavior as well */

	rack->r_ctl.rc_saved_beta.newreno_flags |= CC_NEWRENO_BETA_ECN_ENABLED;
	rack->r_ctl.rc_reorder_fade = rack_reorder_fade;
	rack->rc_allow_data_af_clo = rack_ignore_data_after_close;
	rack->r_ctl.rc_tlp_threshold = rack_tlp_thresh;
	rack->r_ctl.policer_del_mss = rack_req_del_mss;
	if ((rack_policer_rxt_thresh > 0) &&
	    (rack_policer_avg_thresh > 0) &&
	    (rack_policer_med_thresh > 0)) {
		rack->r_ctl.policer_rxt_threshold = rack_policer_rxt_thresh;
		rack->r_ctl.policer_avg_threshold = rack_policer_avg_thresh;
		rack->r_ctl.policer_med_threshold = rack_policer_med_thresh;
		rack->policer_detect_on = 1;
	} else {
		rack->policer_detect_on = 0;
	}
	if (rack_fill_cw_state)
		rack->rc_pace_to_cwnd = 1;
	if (rack_pacing_min_seg)
		rack->r_ctl.rc_user_set_min_segs = rack_pacing_min_seg;
	if (use_rack_rr)
		rack->use_rack_rr = 1;
	if (rack_dnd_default) {
		rack->rc_pace_dnd = 1;
	}
	if (V_tcp_delack_enabled)
		tp->t_delayed_ack = 1;
	else
		tp->t_delayed_ack = 0;
#ifdef TCP_ACCOUNTING
	if (rack_tcp_accounting) {
		tp->t_flags2 |= TF2_TCP_ACCOUNTING;
	}
#endif
	rack->r_ctl.pcm_i.cnt_alloc = RACK_DEFAULT_PCM_ARRAY;
	sz = (sizeof(struct rack_pcm_stats) * rack->r_ctl.pcm_i.cnt_alloc);
	rack->r_ctl.pcm_s = malloc(sz,M_TCPPCM, M_NOWAIT);
	if (rack->r_ctl.pcm_s == NULL) {
		rack->r_ctl.pcm_i.cnt_alloc = 0;
	}
#ifdef NETFLIX_STATS
	rack->r_ctl.side_chan_dis_mask = tcp_sidechannel_disable_mask;
#endif
	rack->r_ctl.rack_per_upper_bound_ss = (uint8_t)rack_per_upper_bound_ss;
	rack->r_ctl.rack_per_upper_bound_ca = (uint8_t)rack_per_upper_bound_ca;
	if (rack_enable_shared_cwnd)
		rack->rack_enable_scwnd = 1;
	rack->r_ctl.pace_len_divisor = rack_default_pacing_divisor;
	rack->rc_user_set_max_segs = rack_hptsi_segments;
	rack->r_ctl.max_reduction = rack_max_reduce;
	rack->rc_force_max_seg = 0;
	TAILQ_INIT(&rack->r_ctl.opt_list);
	rack->r_ctl.rc_saved_beta.beta = V_newreno_beta_ecn;
	rack->r_ctl.rc_saved_beta.beta_ecn = V_newreno_beta_ecn;
	if (rack_hibeta_setting) {
		rack->rack_hibeta = 1;
		if ((rack_hibeta_setting >= 50) &&
		    (rack_hibeta_setting <= 100)) {
			rack->r_ctl.rc_saved_beta.beta = rack_hibeta_setting;
			rack->r_ctl.saved_hibeta = rack_hibeta_setting;
		}
	} else {
		rack->r_ctl.saved_hibeta = 50;
	}
	/*
	 * We initialize to all ones so we never match 0
	 * just in case the client sends in 0, it hopefully
	 * will never have all 1's in ms :-)
	 */
	rack->r_ctl.last_tm_mark = 0xffffffffffffffff;
	rack->r_ctl.rc_reorder_shift = rack_reorder_thresh;
	rack->r_ctl.rc_pkt_delay = rack_pkt_delay;
	rack->r_ctl.pol_bw_comp = rack_policing_do_bw_comp;
	rack->r_ctl.rc_tlp_cwnd_reduce = rack_lower_cwnd_at_tlp;
	rack->r_ctl.rc_lowest_us_rtt = 0xffffffff;
	rack->r_ctl.rc_highest_us_rtt = 0;
	rack->r_ctl.bw_rate_cap = rack_bw_rate_cap;
	rack->pcm_enabled = rack_pcm_is_enabled;
	if (rack_fillcw_bw_cap)
		rack->r_ctl.fillcw_cap = rack_fillcw_bw_cap;
	rack->r_ctl.timer_slop = TICKS_2_USEC(tcp_rexmit_slop);
	if (rack_use_cmp_acks)
		rack->r_use_cmp_ack = 1;
	if (rack_disable_prr)
		rack->rack_no_prr = 1;
	if (rack_gp_no_rec_chg)
		rack->rc_gp_no_rec_chg = 1;
	if (rack_pace_every_seg && tcp_can_enable_pacing()) {
		rack->r_ctl.pacing_method |= RACK_REG_PACING;
		rack->rc_always_pace = 1;
		if (rack->rack_hibeta)
			rack_set_cc_pacing(rack);
	} else
		rack->rc_always_pace = 0;
	if (rack_enable_mqueue_for_nonpaced || rack->r_use_cmp_ack)
		rack->r_mbuf_queue = 1;
	else
		rack->r_mbuf_queue = 0;
	rack_set_pace_segments(tp, rack, __LINE__, NULL);
	if (rack_limits_scwnd)
		rack->r_limit_scw = 1;
	else
		rack->r_limit_scw = 0;
	rack_init_retransmit_value(rack, rack_rxt_controls);
	rack->rc_labc = V_tcp_abc_l_var;
	if (rack_honors_hpts_min_to)
		rack->r_use_hpts_min = 1;
	if (tp->snd_una != 0) {
		rack->r_ctl.idle_snd_una = tp->snd_una;
		rack->rc_sendvars_notset = 0;
		/*
		 * Make sure any TCP timers are not running.
		 */
		tcp_timer_stop(tp);
	} else {
		/*
		 * Server side, we are called from the
		 * syn-cache. This means none of the
		 * snd_una/max are set yet so we have
		 * to defer this until the first send.
		 */
		rack->rc_sendvars_notset = 1;
	}

	rack->r_ctl.rc_rate_sample_method = rack_rate_sample_method;
	rack->rack_tlp_threshold_use = rack_tlp_threshold_use;
	rack->r_ctl.rc_prr_sendalot = rack_send_a_lot_in_prr;
	rack->r_ctl.rc_min_to = rack_min_to;
	microuptime(&rack->r_ctl.act_rcv_time);
	rack->r_ctl.rc_last_time_decay = rack->r_ctl.act_rcv_time;
	rack->r_ctl.rack_per_of_gp_ss = rack_per_of_gp_ss;
	if (rack_hw_up_only)
		rack->r_up_only = 1;
	if (rack_do_dyn_mul) {
		/* When dynamic adjustment is on CA needs to start at 100% */
		rack->rc_gp_dyn_mul = 1;
		if (rack_do_dyn_mul >= 100)
			rack->r_ctl.rack_per_of_gp_ca = rack_do_dyn_mul;
	} else
		rack->r_ctl.rack_per_of_gp_ca = rack_per_of_gp_ca;
	rack->r_ctl.rack_per_of_gp_rec = rack_per_of_gp_rec;
	if (rack_timely_off) {
		rack->rc_skip_timely = 1;
	}
	if (rack->rc_skip_timely) {
		rack->r_ctl.rack_per_of_gp_rec = 90;
		rack->r_ctl.rack_per_of_gp_ca = 100;
		rack->r_ctl.rack_per_of_gp_ss = 250;
	}
	rack->r_ctl.rack_per_of_gp_probertt = rack_per_of_gp_probertt;
	rack->r_ctl.rc_tlp_rxt_last_time = tcp_tv_to_mssectick(&rack->r_ctl.act_rcv_time);
	rack->r_ctl.last_rcv_tstmp_for_rtt = tcp_tv_to_mssectick(&rack->r_ctl.act_rcv_time);

	setup_time_filter_small(&rack->r_ctl.rc_gp_min_rtt, FILTER_TYPE_MIN,
				rack_probertt_filter_life);
	us_cts = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
	rack->r_ctl.rc_lower_rtt_us_cts = us_cts;
	rack->r_ctl.rc_time_of_last_probertt = us_cts;
	rack->r_ctl.rc_went_idle_time = us_cts;
	rack->r_ctl.challenge_ack_ts = tcp_ts_getticks() - (tcp_ack_war_time_window + 1);
	rack->r_ctl.rc_time_probertt_starts = 0;

	rack->r_ctl.gp_rnd_thresh = rack_rnd_cnt_req & 0xff;
	if (rack_rnd_cnt_req  & 0x10000)
		rack->r_ctl.gate_to_fs = 1;
	rack->r_ctl.gp_gain_req = rack_gp_gain_req;
	if ((rack_rnd_cnt_req & 0x100) > 0) {

	}
	if (rack_dsack_std_based & 0x1) {
		/* Basically this means all rack timers are at least (srtt + 1/4 srtt) */
		rack->rc_rack_tmr_std_based = 1;
	}
	if (rack_dsack_std_based & 0x2) {
		/* Basically this means  rack timers are extended based on dsack by up to (2 * srtt) */
		rack->rc_rack_use_dsack = 1;
	}
	/* We require at least one measurement, even if the sysctl is 0 */
	if (rack_req_measurements)
		rack->r_ctl.req_measurements = rack_req_measurements;
	else
		rack->r_ctl.req_measurements = 1;
	if (rack_enable_hw_pacing)
		rack->rack_hdw_pace_ena = 1;
	if (rack_hw_rate_caps)
		rack->r_rack_hw_rate_caps = 1;
#ifdef TCP_SAD_DETECTION
	rack->do_detection = 1;
#else
	rack->do_detection = 0;
#endif
	if (rack_non_rxt_use_cr)
		rack->rack_rec_nonrxt_use_cr = 1;
	/* Lets setup the fsb block */
	err = rack_init_fsb(tp, rack);
	if (err) {
		uma_zfree(rack_pcb_zone, *ptr);
		*ptr = NULL;
		return (err);
	}
	if (rack_do_hystart) {
		tp->t_ccv.flags |= CCF_HYSTART_ALLOWED;
		if (rack_do_hystart > 1)
			tp->t_ccv.flags |= CCF_HYSTART_CAN_SH_CWND;
		if (rack_do_hystart > 2)
			tp->t_ccv.flags |= CCF_HYSTART_CONS_SSTH;
	}
	/* Log what we will do with queries */
	rack_log_chg_info(tp, rack, 7,
			  no_query, 0, 0);
	if (rack_def_profile)
		rack_set_profile(rack, rack_def_profile);
	/* Cancel the GP measurement in progress */
	tp->t_flags &= ~TF_GPUTINPROG;
	if ((tp->t_state != TCPS_CLOSED) &&
	    (tp->t_state != TCPS_TIME_WAIT)) {
		/*
		 * We are already open, we may
		 * need to adjust a few things.
		 */
		if (SEQ_GT(tp->snd_max, tp->iss))
			snt = tp->snd_max - tp->iss;
		else
			snt = 0;
		iwin = rc_init_window(rack);
		if ((snt < iwin) &&
		    (no_query == 1)) {
			/* We are not past the initial window
			 * on the first init (i.e. a stack switch
			 * has not yet occured) so we need to make
			 * sure cwnd and ssthresh is correct.
			 */
			if (tp->snd_cwnd < iwin)
				tp->snd_cwnd = iwin;
			/*
			 * If we are within the initial window
			 * we want ssthresh to be unlimited. Setting
			 * it to the rwnd (which the default stack does
			 * and older racks) is not really a good idea
			 * since we want to be in SS and grow both the
			 * cwnd and the rwnd (via dynamic rwnd growth). If
			 * we set it to the rwnd then as the peer grows its
			 * rwnd we will be stuck in CA and never hit SS.
			 *
			 * Its far better to raise it up high (this takes the
			 * risk that there as been a loss already, probably
			 * we should have an indicator in all stacks of loss
			 * but we don't), but considering the normal use this
			 * is a risk worth taking. The consequences of not
			 * hitting SS are far worse than going one more time
			 * into it early on (before we have sent even a IW).
			 * It is highly unlikely that we will have had a loss
			 * before getting the IW out.
			 */
			tp->snd_ssthresh = 0xffffffff;
		}
		/*
		 * Any init based on sequence numbers
		 * should be done in the deferred init path
		 * since we can be CLOSED and not have them
		 * inited when rack_init() is called. We
		 * are not closed so lets call it.
		 */
		rack_deferred_init(tp, rack);
	}
	if ((tp->t_state != TCPS_CLOSED) &&
	    (tp->t_state != TCPS_TIME_WAIT) &&
	    (no_query == 0) &&
	    (tp->snd_una != tp->snd_max))  {
		err = rack_init_outstanding(tp, rack, us_cts, *ptr);
		if (err) {
			*ptr = NULL;
			return(err);
		}
	}
	rack_stop_all_timers(tp, rack);
	/* Setup all the t_flags2 */
	if  (rack->r_mbuf_queue || rack->rc_always_pace || rack->r_use_cmp_ack)
		tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
	else
		tp->t_flags2 &= ~TF2_SUPPORTS_MBUFQ;
	if (rack->r_use_cmp_ack && TCPS_HAVEESTABLISHED(tp->t_state))
		tp->t_flags2 |= TF2_MBUF_ACKCMP;
	/*
	 * Timers in Rack are kept in microseconds so lets
	 * convert any initial incoming variables
	 * from ticks into usecs. Note that we
	 * also change the values of t_srtt and t_rttvar, if
	 * they are non-zero. They are kept with a 5
	 * bit decimal so we have to carefully convert
	 * these to get the full precision.
	 */
	rack_convert_rtts(tp);
	rack_log_hystart_event(rack, rack->r_ctl.roundends, 20);
	if ((tptoinpcb(tp)->inp_flags & INP_DROPPED) == 0) {
		/* We do not start any timers on DROPPED connections */
		if (tp->t_fb->tfb_chg_query == NULL) {
			rack_start_hpts_timer(rack, tp, tcp_get_usecs(NULL), 0, 0, 0);
		} else {
			struct tcp_query_resp qr;
			int ret;

			memset(&qr, 0, sizeof(qr));

			/* Get the misc time stamps and such for rack */
			qr.req = TCP_QUERY_RACK_TIMES;
			ret = (*tp->t_fb->tfb_chg_query)(tp, &qr);
			if (ret == 1) {
				rack->r_ctl.rc_reorder_ts = qr.rack_reorder_ts;
				rack->r_ctl.num_dsack  = qr.rack_num_dsacks;
				rack->r_ctl.rc_tlp_rxt_last_time = qr.rack_rxt_last_time;
				rack->r_ctl.rc_rack_min_rtt = qr.rack_min_rtt;
				rack->rc_rack_rtt = qr.rack_rtt;
				rack->r_ctl.rc_rack_tmit_time = qr.rack_tmit_time;
				rack->r_ctl.rc_sacked = qr.rack_sacked;
				rack->r_ctl.rc_holes_rxt = qr.rack_holes_rxt;
				rack->r_ctl.rc_prr_delivered = qr.rack_prr_delivered;
				rack->r_ctl.rc_prr_recovery_fs = qr.rack_prr_recovery_fs;
				rack->r_ctl.rc_prr_sndcnt = qr.rack_prr_sndcnt;
				rack->r_ctl.rc_prr_out = qr.rack_prr_out;
				if (qr.rack_tlp_out) {
					rack->rc_tlp_in_progress = 1;
					rack->r_ctl.rc_tlp_cnt_out = qr.rack_tlp_cnt_out;
				} else {
					rack->rc_tlp_in_progress = 0;
					rack->r_ctl.rc_tlp_cnt_out = 0;
				}
				if (qr.rack_srtt_measured)
					rack->rc_srtt_measure_made = 1;
				if (qr.rack_in_persist == 1) {
					rack->r_ctl.rc_went_idle_time = qr.rack_time_went_idle;
#ifdef NETFLIX_SHARED_CWND
					if (rack->r_ctl.rc_scw) {
						tcp_shared_cwnd_idle(rack->r_ctl.rc_scw, rack->r_ctl.rc_scw_index);
						rack->rack_scwnd_is_idle = 1;
					}
#endif
					rack->r_ctl.persist_lost_ends = 0;
					rack->probe_not_answered = 0;
					rack->forced_ack = 0;
					tp->t_rxtshift = 0;
					rack->rc_in_persist = 1;
					RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
							   rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
				}
				if (qr.rack_wanted_output)
					rack->r_wanted_output = 1;
				rack_log_chg_info(tp, rack, 6,
						  qr.rack_min_rtt,
						  qr.rack_rtt,
						  qr.rack_reorder_ts);
			}
			/* Get the old stack timers */
			qr.req_param = 0;
			qr.req = TCP_QUERY_TIMERS_UP;
			ret = (*tp->t_fb->tfb_chg_query)(tp, &qr);
			if (ret) {
				/*
				 * non-zero return means we have a timer('s)
				 * to start. Zero means no timer (no keepalive
				 * I suppose).
				 */
				uint32_t tov = 0;

				rack->r_ctl.rc_hpts_flags = qr.timer_hpts_flags;
				if (qr.timer_hpts_flags & PACE_PKT_OUTPUT) {
					rack->r_ctl.rc_last_output_to = qr.timer_pacing_to;
					if (TSTMP_GT(qr.timer_pacing_to, us_cts))
						tov = qr.timer_pacing_to - us_cts;
					else
						tov = HPTS_TICKS_PER_SLOT;
				}
				if (qr.timer_hpts_flags & PACE_TMR_MASK) {
					rack->r_ctl.rc_timer_exp = qr.timer_timer_exp;
					if (tov == 0) {
						if (TSTMP_GT(qr.timer_timer_exp, us_cts))
							tov = qr.timer_timer_exp - us_cts;
						else
							tov = HPTS_TICKS_PER_SLOT;
					}
				}
				rack_log_chg_info(tp, rack, 4,
						  rack->r_ctl.rc_hpts_flags,
						  rack->r_ctl.rc_last_output_to,
						  rack->r_ctl.rc_timer_exp);
				if (tov) {
					struct hpts_diag diag;

					(void)tcp_hpts_insert_diag(tp, HPTS_USEC_TO_SLOTS(tov),
								   __LINE__, &diag);
					rack_log_hpts_diag(rack, us_cts, &diag, &rack->r_ctl.act_rcv_time);
				}
			}
		}
		rack_log_rtt_shrinks(rack,  us_cts,  tp->t_rxtcur,
				     __LINE__, RACK_RTTS_INIT);
	}
	return (0);
}

static int
rack_handoff_ok(struct tcpcb *tp)
{
	if ((tp->t_state == TCPS_CLOSED) ||
	    (tp->t_state == TCPS_LISTEN)) {
		/* Sure no problem though it may not stick */
		return (0);
	}
	if ((tp->t_state == TCPS_SYN_SENT) ||
	    (tp->t_state == TCPS_SYN_RECEIVED)) {
		/*
		 * We really don't know if you support sack,
		 * you have to get to ESTAB or beyond to tell.
		 */
		return (EAGAIN);
	}
	if ((tp->t_flags & TF_SENTFIN) && ((tp->snd_max - tp->snd_una) > 1)) {
		/*
		 * Rack will only send a FIN after all data is acknowledged.
		 * So in this case we have more data outstanding. We can't
		 * switch stacks until either all data and only the FIN
		 * is left (in which case rack_init() now knows how
		 * to deal with that) <or> all is acknowledged and we
		 * are only left with incoming data, though why you
		 * would want to switch to rack after all data is acknowledged
		 * I have no idea (rrs)!
		 */
		return (EAGAIN);
	}
	if ((tp->t_flags & TF_SACK_PERMIT) || rack_sack_not_required){
		return (0);
	}
	/*
	 * If we reach here we don't do SACK on this connection so we can
	 * never do rack.
	 */
	return (EINVAL);
}

static void
rack_fini(struct tcpcb *tp, int32_t tcb_is_purged)
{

	if (tp->t_fb_ptr) {
		uint32_t cnt_free = 0;
		struct tcp_rack *rack;
		struct rack_sendmap *rsm;

		tcp_handle_orphaned_packets(tp);
		tp->t_flags &= ~TF_FORCEDATA;
		rack = (struct tcp_rack *)tp->t_fb_ptr;
		rack_log_pacing_delay_calc(rack,
					   0,
					   0,
					   0,
					   rack_get_gp_est(rack), /* delRate */
					   rack_get_lt_bw(rack), /* rttProp */
					   20, __LINE__, NULL, 0);
#ifdef NETFLIX_SHARED_CWND
		if (rack->r_ctl.rc_scw) {
			uint32_t limit;

			if (rack->r_limit_scw)
				limit = max(1, rack->r_ctl.rc_lowest_us_rtt);
			else
				limit = 0;
			tcp_shared_cwnd_free_full(tp, rack->r_ctl.rc_scw,
						  rack->r_ctl.rc_scw_index,
						  limit);
			rack->r_ctl.rc_scw = NULL;
		}
#endif
		if (rack->r_ctl.fsb.tcp_ip_hdr) {
			free(rack->r_ctl.fsb.tcp_ip_hdr, M_TCPFSB);
			rack->r_ctl.fsb.tcp_ip_hdr = NULL;
			rack->r_ctl.fsb.th = NULL;
		}
		if (rack->rc_always_pace == 1) {
			rack_remove_pacing(rack);
		}
		/* Clean up any options if they were not applied */
		while (!TAILQ_EMPTY(&rack->r_ctl.opt_list)) {
			struct deferred_opt_list *dol;

			dol = TAILQ_FIRST(&rack->r_ctl.opt_list);
			TAILQ_REMOVE(&rack->r_ctl.opt_list, dol, next);
			free(dol, M_TCPDO);
		}
		/* rack does not use force data but other stacks may clear it */
		if (rack->r_ctl.crte != NULL) {
			tcp_rel_pacing_rate(rack->r_ctl.crte, tp);
			rack->rack_hdrw_pacing = 0;
			rack->r_ctl.crte = NULL;
		}
#ifdef TCP_BLACKBOX
		tcp_log_flowend(tp);
#endif
		/*
		 * Lets take a different approach to purging just
		 * get each one and free it like a cum-ack would and
		 * not use a foreach loop.
		 */
		rsm = tqhash_min(rack->r_ctl.tqh);
		while (rsm) {
			tqhash_remove(rack->r_ctl.tqh, rsm, REMOVE_TYPE_CUMACK);
			rack->r_ctl.rc_num_maps_alloced--;
			uma_zfree(rack_zone, rsm);
			rsm = tqhash_min(rack->r_ctl.tqh);
		}
		rsm = TAILQ_FIRST(&rack->r_ctl.rc_free);
		while (rsm) {
			TAILQ_REMOVE(&rack->r_ctl.rc_free, rsm, r_tnext);
			rack->r_ctl.rc_num_maps_alloced--;
			rack->rc_free_cnt--;
			cnt_free++;
			uma_zfree(rack_zone, rsm);
			rsm = TAILQ_FIRST(&rack->r_ctl.rc_free);
		}
		if (rack->r_ctl.pcm_s != NULL) {
			free(rack->r_ctl.pcm_s, M_TCPPCM);
			rack->r_ctl.pcm_s = NULL;
			rack->r_ctl.pcm_i.cnt_alloc = 0;
			rack->r_ctl.pcm_i.cnt = 0;
		}
		if ((rack->r_ctl.rc_num_maps_alloced > 0) &&
		    (tcp_bblogging_on(tp))) {
			union tcp_log_stackspecific log;
			struct timeval tv;

			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.flex8 = 10;
			log.u_bbr.flex1 = rack->r_ctl.rc_num_maps_alloced;
			log.u_bbr.flex2 = rack->rc_free_cnt;
			log.u_bbr.flex3 = cnt_free;
			log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
			rsm = tqhash_min(rack->r_ctl.tqh);
			log.u_bbr.delRate = (uint64_t)rsm;
			rsm = TAILQ_FIRST(&rack->r_ctl.rc_free);
			log.u_bbr.cur_del_rate = (uint64_t)rsm;
			log.u_bbr.timeStamp = tcp_get_usecs(&tv);
			log.u_bbr.pkt_epoch = __LINE__;
			(void)tcp_log_event(tp, NULL, NULL, NULL, TCP_LOG_OUT, ERRNO_UNK,
					     0, &log, false, NULL, NULL, 0, &tv);
		}
		KASSERT((rack->r_ctl.rc_num_maps_alloced == 0),
			("rack:%p num_aloc:%u after freeing all?",
			 rack,
			 rack->r_ctl.rc_num_maps_alloced));
		rack->rc_free_cnt = 0;
		free(rack->r_ctl.tqh, M_TCPFSB);
		rack->r_ctl.tqh = NULL;
		uma_zfree(rack_pcb_zone, tp->t_fb_ptr);
		tp->t_fb_ptr = NULL;
	}
	/* Make sure snd_nxt is correctly set */
	tp->snd_nxt = tp->snd_max;
}

static void
rack_set_state(struct tcpcb *tp, struct tcp_rack *rack)
{
	if ((rack->r_state == TCPS_CLOSED) && (tp->t_state != TCPS_CLOSED)) {
		rack->r_is_v6 = (tptoinpcb(tp)->inp_vflag & INP_IPV6) != 0;
	}
	switch (tp->t_state) {
	case TCPS_SYN_SENT:
		rack->r_state = TCPS_SYN_SENT;
		rack->r_substate = rack_do_syn_sent;
		break;
	case TCPS_SYN_RECEIVED:
		rack->r_state = TCPS_SYN_RECEIVED;
		rack->r_substate = rack_do_syn_recv;
		break;
	case TCPS_ESTABLISHED:
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		rack->r_state = TCPS_ESTABLISHED;
		rack->r_substate = rack_do_established;
		break;
	case TCPS_CLOSE_WAIT:
		rack->r_state = TCPS_CLOSE_WAIT;
		rack->r_substate = rack_do_close_wait;
		break;
	case TCPS_FIN_WAIT_1:
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		rack->r_state = TCPS_FIN_WAIT_1;
		rack->r_substate = rack_do_fin_wait_1;
		break;
	case TCPS_CLOSING:
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		rack->r_state = TCPS_CLOSING;
		rack->r_substate = rack_do_closing;
		break;
	case TCPS_LAST_ACK:
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		rack->r_state = TCPS_LAST_ACK;
		rack->r_substate = rack_do_lastack;
		break;
	case TCPS_FIN_WAIT_2:
		rack->r_state = TCPS_FIN_WAIT_2;
		rack->r_substate = rack_do_fin_wait_2;
		break;
	case TCPS_LISTEN:
	case TCPS_CLOSED:
	case TCPS_TIME_WAIT:
	default:
		break;
	};
	if (rack->r_use_cmp_ack && TCPS_HAVEESTABLISHED(tp->t_state))
		rack->rc_tp->t_flags2 |= TF2_MBUF_ACKCMP;

}

static void
rack_timer_audit(struct tcpcb *tp, struct tcp_rack *rack, struct sockbuf *sb)
{
	/*
	 * We received an ack, and then did not
	 * call send or were bounced out due to the
	 * hpts was running. Now a timer is up as well, is
	 * it the right timer?
	 */
	struct rack_sendmap *rsm;
	int tmr_up;

	tmr_up = rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK;
	if (tcp_in_hpts(rack->rc_tp) == 0) {
		/*
		 * Ok we probably need some timer up, but no
		 * matter what the mask we are not in hpts. We
		 * may have received an old ack and thus did nothing.
		 */
		rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
		rack_start_hpts_timer(rack, tp, tcp_get_usecs(NULL), 0, 0, 0);
		return;
	}
	if (rack->rc_in_persist && (tmr_up == PACE_TMR_PERSIT))
		return;
	rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
	if (((rsm == NULL) || (tp->t_state < TCPS_ESTABLISHED)) &&
	    (tmr_up == PACE_TMR_RXT)) {
		/* Should be an RXT */
		return;
	}
	if (rsm == NULL) {
		/* Nothing outstanding? */
		if (tp->t_flags & TF_DELACK) {
			if (tmr_up == PACE_TMR_DELACK)
				/* We are supposed to have delayed ack up and we do */
				return;
		} else if (sbavail(&tptosocket(tp)->so_snd) && (tmr_up == PACE_TMR_RXT)) {
			/*
			 * if we hit enobufs then we would expect the possibility
			 * of nothing outstanding and the RXT up (and the hptsi timer).
			 */
			return;
		} else if (((V_tcp_always_keepalive ||
			     rack->rc_inp->inp_socket->so_options & SO_KEEPALIVE) &&
			    (tp->t_state <= TCPS_CLOSING)) &&
			   (tmr_up == PACE_TMR_KEEP) &&
			   (tp->snd_max == tp->snd_una)) {
			/* We should have keep alive up and we do */
			return;
		}
	}
	if (SEQ_GT(tp->snd_max, tp->snd_una) &&
		   ((tmr_up == PACE_TMR_TLP) ||
		    (tmr_up == PACE_TMR_RACK) ||
		    (tmr_up == PACE_TMR_RXT))) {
		/*
		 * Either a Rack, TLP or RXT is fine if  we
		 * have outstanding data.
		 */
		return;
	} else if (tmr_up == PACE_TMR_DELACK) {
		/*
		 * If the delayed ack was going to go off
		 * before the rtx/tlp/rack timer were going to
		 * expire, then that would be the timer in control.
		 * Note we don't check the time here trusting the
		 * code is correct.
		 */
		return;
	}
	/*
	 * Ok the timer originally started is not what we want now.
	 * We will force the hpts to be stopped if any, and restart
	 * with the slot set to what was in the saved slot.
	 */
	if (tcp_in_hpts(rack->rc_tp)) {
		if (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) {
			uint32_t us_cts;

			us_cts = tcp_get_usecs(NULL);
			if (TSTMP_GT(rack->r_ctl.rc_last_output_to, us_cts)) {
				rack->r_early = 1;
				rack->r_ctl.rc_agg_early += (rack->r_ctl.rc_last_output_to - us_cts);
			}
			rack->r_ctl.rc_hpts_flags &= ~PACE_PKT_OUTPUT;
		}
		tcp_hpts_remove(rack->rc_tp);
	}
	rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
	rack_start_hpts_timer(rack, tp, tcp_get_usecs(NULL), 0, 0, 0);
}


static void
rack_do_win_updates(struct tcpcb *tp, struct tcp_rack *rack, uint32_t tiwin, uint32_t seq, uint32_t ack, uint32_t cts)
{
	if ((SEQ_LT(tp->snd_wl1, seq) ||
	    (tp->snd_wl1 == seq && (SEQ_LT(tp->snd_wl2, ack) ||
	    (tp->snd_wl2 == ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if ((tp->snd_wl2 == ack) && (tiwin > tp->snd_wnd))
			KMOD_TCPSTAT_INC(tcps_rcvwinupd);
		tp->snd_wnd = tiwin;
		rack_validate_fo_sendwin_up(tp, rack);
		tp->snd_wl1 = seq;
		tp->snd_wl2 = ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
	    rack->r_wanted_output = 1;
	} else if ((tp->snd_wl2 == ack) && (tiwin < tp->snd_wnd)) {
		tp->snd_wnd = tiwin;
		rack_validate_fo_sendwin_up(tp, rack);
		tp->snd_wl1 = seq;
		tp->snd_wl2 = ack;
	} else {
		/* Not a valid win update */
		return;
	}
	if (tp->snd_wnd > tp->max_sndwnd)
		tp->max_sndwnd = tp->snd_wnd;
	/* Do we exit persists? */
	if ((rack->rc_in_persist != 0) &&
	    (tp->snd_wnd >= min((rack->r_ctl.rc_high_rwnd/2),
				rack->r_ctl.rc_pace_min_segs))) {
		rack_exit_persist(tp, rack, cts);
	}
	/* Do we enter persists? */
	if ((rack->rc_in_persist == 0) &&
	    (tp->snd_wnd < min((rack->r_ctl.rc_high_rwnd/2), rack->r_ctl.rc_pace_min_segs)) &&
	    TCPS_HAVEESTABLISHED(tp->t_state) &&
	    ((tp->snd_max == tp->snd_una) || rack->rc_has_collapsed) &&
	    sbavail(&tptosocket(tp)->so_snd) &&
	    (sbavail(&tptosocket(tp)->so_snd) > tp->snd_wnd)) {
		/*
		 * Here the rwnd is less than
		 * the pacing size, we are established,
		 * nothing is outstanding, and there is
		 * data to send. Enter persists.
		 */
		rack_enter_persist(tp, rack, rack->r_ctl.rc_rcvtime, ack);
	}
}

static void
rack_log_input_packet(struct tcpcb *tp, struct tcp_rack *rack, struct tcp_ackent *ae, int ackval, uint32_t high_seq)
{

	if (tcp_bblogging_on(rack->rc_tp)) {
		struct inpcb *inp = tptoinpcb(tp);
		union tcp_log_stackspecific log;
		struct timeval ltv;
		char tcp_hdr_buf[60];
		struct tcphdr *th;
		struct timespec ts;
		uint32_t orig_snd_una;
		uint8_t xx = 0;

#ifdef TCP_REQUEST_TRK
		struct tcp_sendfile_track *tcp_req;

		if (SEQ_GT(ae->ack, tp->snd_una)) {
			tcp_req = tcp_req_find_req_for_seq(tp, (ae->ack-1));
		} else {
			tcp_req = tcp_req_find_req_for_seq(tp, ae->ack);
		}
#endif
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		if (rack->rack_no_prr == 0)
			log.u_bbr.flex1 = rack->r_ctl.rc_prr_sndcnt;
		else
			log.u_bbr.flex1 = 0;
		log.u_bbr.use_lt_bw = rack->r_ent_rec_ns;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->r_might_revert;
		log.u_bbr.flex2 = rack->r_ctl.rc_num_maps_alloced;
		log.u_bbr.bbr_state = rack->rc_free_cnt;
		log.u_bbr.inflight = ctf_flight_size(tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = tp->t_maxseg;
		log.u_bbr.flex4 = rack->r_ctl.rc_hpts_flags;
		log.u_bbr.flex7 = 1;
		log.u_bbr.lost = ae->flags;
		log.u_bbr.cwnd_gain = ackval;
		log.u_bbr.pacing_gain = 0x2;
		if (ae->flags & TSTMP_HDWR) {
			/* Record the hardware timestamp if present */
			log.u_bbr.flex3 = M_TSTMP;
			ts.tv_sec = ae->timestamp / 1000000000;
			ts.tv_nsec = ae->timestamp % 1000000000;
			ltv.tv_sec = ts.tv_sec;
			ltv.tv_usec = ts.tv_nsec / 1000;
			log.u_bbr.lt_epoch = tcp_tv_to_usectick(&ltv);
		} else if (ae->flags & TSTMP_LRO) {
			/* Record the LRO the arrival timestamp */
			log.u_bbr.flex3 = M_TSTMP_LRO;
			ts.tv_sec = ae->timestamp / 1000000000;
			ts.tv_nsec = ae->timestamp % 1000000000;
			ltv.tv_sec = ts.tv_sec;
			ltv.tv_usec = ts.tv_nsec / 1000;
			log.u_bbr.flex5 = tcp_tv_to_usectick(&ltv);
		}
		log.u_bbr.timeStamp = tcp_get_usecs(&ltv);
		/* Log the rcv time */
		log.u_bbr.delRate = ae->timestamp;
#ifdef TCP_REQUEST_TRK
		log.u_bbr.applimited = tp->t_tcpreq_closed;
		log.u_bbr.applimited <<= 8;
		log.u_bbr.applimited |= tp->t_tcpreq_open;
		log.u_bbr.applimited <<= 8;
		log.u_bbr.applimited |= tp->t_tcpreq_req;
		if (tcp_req) {
			/* Copy out any client req info */
			/* seconds */
			log.u_bbr.pkt_epoch = (tcp_req->localtime / HPTS_USEC_IN_SEC);
			/* useconds */
			log.u_bbr.delivered = (tcp_req->localtime % HPTS_USEC_IN_SEC);
			log.u_bbr.rttProp = tcp_req->timestamp;
			log.u_bbr.cur_del_rate = tcp_req->start;
			if (tcp_req->flags & TCP_TRK_TRACK_FLG_OPEN) {
				log.u_bbr.flex8 |= 1;
			} else {
				log.u_bbr.flex8 |= 2;
				log.u_bbr.bw_inuse = tcp_req->end;
			}
			log.u_bbr.flex6 = tcp_req->start_seq;
			if (tcp_req->flags & TCP_TRK_TRACK_FLG_COMP) {
				log.u_bbr.flex8 |= 4;
				log.u_bbr.epoch = tcp_req->end_seq;
			}
		}
#endif
		memset(tcp_hdr_buf, 0, sizeof(tcp_hdr_buf));
		th = (struct tcphdr *)tcp_hdr_buf;
		th->th_seq = ae->seq;
		th->th_ack = ae->ack;
		th->th_win = ae->win;
		/* Now fill in the ports */
		th->th_sport = inp->inp_fport;
		th->th_dport = inp->inp_lport;
		tcp_set_flags(th, ae->flags);
		/* Now do we have a timestamp option? */
		if (ae->flags & HAS_TSTMP) {
			u_char *cp;
			uint32_t val;

			th->th_off = ((sizeof(struct tcphdr) + TCPOLEN_TSTAMP_APPA) >> 2);
			cp = (u_char *)(th + 1);
			*cp = TCPOPT_NOP;
			cp++;
			*cp = TCPOPT_NOP;
			cp++;
			*cp = TCPOPT_TIMESTAMP;
			cp++;
			*cp = TCPOLEN_TIMESTAMP;
			cp++;
			val = htonl(ae->ts_value);
			bcopy((char *)&val,
			      (char *)cp, sizeof(uint32_t));
			val = htonl(ae->ts_echo);
			bcopy((char *)&val,
			      (char *)(cp + 4), sizeof(uint32_t));
		} else
			th->th_off = (sizeof(struct tcphdr) >> 2);

		/*
		 * For sane logging we need to play a little trick.
		 * If the ack were fully processed we would have moved
		 * snd_una to high_seq, but since compressed acks are
		 * processed in two phases, at this point (logging) snd_una
		 * won't be advanced. So we would see multiple acks showing
		 * the advancement. We can prevent that by "pretending" that
		 * snd_una was advanced and then un-advancing it so that the
		 * logging code has the right value for tlb_snd_una.
		 */
		if (tp->snd_una != high_seq) {
			orig_snd_una = tp->snd_una;
			tp->snd_una = high_seq;
			xx = 1;
		} else
			xx = 0;
		TCP_LOG_EVENTP(tp, th,
			       &tptosocket(tp)->so_rcv,
			       &tptosocket(tp)->so_snd, TCP_LOG_IN, 0,
			       0, &log, true, &ltv);
		if (xx) {
			tp->snd_una = orig_snd_una;
		}
	}

}

static void
rack_handle_probe_response(struct tcp_rack *rack, uint32_t tiwin, uint32_t us_cts)
{
	uint32_t us_rtt;
	/*
	 * A persist or keep-alive was forced out, update our
	 * min rtt time. Note now worry about lost responses.
	 * When a subsequent keep-alive or persist times out
	 * and forced_ack is still on, then the last probe
	 * was not responded to. In such cases we have a
	 * sysctl that controls the behavior. Either we apply
	 * the rtt but with reduced confidence (0). Or we just
	 * plain don't apply the rtt estimate. Having data flow
	 * will clear the probe_not_answered flag i.e. cum-ack
	 * move forward <or> exiting and reentering persists.
	 */

	rack->forced_ack = 0;
	rack->rc_tp->t_rxtshift = 0;
	if ((rack->rc_in_persist &&
	     (tiwin == rack->rc_tp->snd_wnd)) ||
	    (rack->rc_in_persist == 0)) {
		/*
		 * In persists only apply the RTT update if this is
		 * a response to our window probe. And that
		 * means the rwnd sent must match the current
		 * snd_wnd. If it does not, then we got a
		 * window update ack instead. For keepalive
		 * we allow the answer no matter what the window.
		 *
		 * Note that if the probe_not_answered is set then
		 * the forced_ack_ts is the oldest one i.e. the first
		 * probe sent that might have been lost. This assures
		 * us that if we do calculate an RTT it is longer not
		 * some short thing.
		 */
		if (rack->rc_in_persist)
			counter_u64_add(rack_persists_acks, 1);
		us_rtt = us_cts - rack->r_ctl.forced_ack_ts;
		if (us_rtt == 0)
			us_rtt = 1;
		if (rack->probe_not_answered == 0) {
			rack_apply_updated_usrtt(rack, us_rtt, us_cts);
			tcp_rack_xmit_timer(rack, us_rtt, 0, us_rtt, 3, NULL, 1);
		} else {
			/* We have a retransmitted probe here too */
			if (rack_apply_rtt_with_reduced_conf) {
				rack_apply_updated_usrtt(rack, us_rtt, us_cts);
				tcp_rack_xmit_timer(rack, us_rtt, 0, us_rtt, 0, NULL, 1);
			}
		}
	}
}

static void
rack_new_round_starts(struct tcpcb *tp, struct tcp_rack *rack, uint32_t high_seq)
{
	/*
	 * The next send has occurred mark the end of the round
	 * as when that data gets acknowledged. We can
	 * also do common things we might need to do when
	 * a round begins.
	 */
	rack->r_ctl.roundends = tp->snd_max;
	rack->rc_new_rnd_needed = 0;
	rack_log_hystart_event(rack, tp->snd_max, 4);
}


static void
rack_log_pcm(struct tcp_rack *rack, uint8_t mod, uint32_t flex1, uint32_t flex2,
	     uint32_t flex3)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		
		(void)tcp_get_usecs(&tv);
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.timeStamp = tcp_tv_to_usectick(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.flex8 = mod;
		log.u_bbr.flex1 = flex1;
		log.u_bbr.flex2 = flex2;
		log.u_bbr.flex3 = flex3;
		log.u_bbr.flex4 = rack_pcm_every_n_rounds;
		log.u_bbr.flex5 = rack->r_ctl.pcm_idle_rounds;
		log.u_bbr.bbr_substate = rack->pcm_needed;
		log.u_bbr.bbr_substate <<= 1;
		log.u_bbr.bbr_substate |= rack->pcm_in_progress;
		log.u_bbr.bbr_substate <<= 1;
		log.u_bbr.bbr_substate |= rack->pcm_enabled; /* bits are NIE for Needed, Inprogress, Enabled */
		(void)tcp_log_event(rack->rc_tp, NULL, NULL, NULL, TCP_PCM_MEASURE, ERRNO_UNK,
				    0, &log, false, NULL, NULL, 0, &tv);
	}
}

static void
rack_new_round_setup(struct tcpcb *tp, struct tcp_rack *rack, uint32_t high_seq)
{
	/*
	 * The round (current_round) has ended. We now
	 * setup for the next round by incrementing the
	 * round numnber and doing any round specific
	 * things.
	 */
	rack_log_hystart_event(rack, high_seq, 21);
	rack->r_ctl.current_round++;
	/* New round (current_round) begins at next send */
	rack->rc_new_rnd_needed = 1;
	if ((rack->pcm_enabled == 1) &&
	    (rack->pcm_needed == 0) &&
	    (rack->pcm_in_progress == 0)) {
		/*
		 * If we have enabled PCM, then we need to
		 * check if the round has adanced to the state
		 * where one is required.
		 */
		int rnds;

		rnds = rack->r_ctl.current_round - rack->r_ctl.last_pcm_round;
		if ((rnds + rack->r_ctl.pcm_idle_rounds) >= rack_pcm_every_n_rounds) {
			rack->pcm_needed = 1;
			rack_log_pcm(rack, 3, rack->r_ctl.last_pcm_round, rack_pcm_every_n_rounds, rack->r_ctl.current_round );
		} else if (rack_verbose_logging) {
			rack_log_pcm(rack, 3, rack->r_ctl.last_pcm_round, rack_pcm_every_n_rounds, rack->r_ctl.current_round );
		}
	}
	if (tp->t_ccv.flags & CCF_HYSTART_ALLOWED) {
		/* We have hystart enabled send the round info in */
		if (CC_ALGO(tp)->newround != NULL) {
			CC_ALGO(tp)->newround(&tp->t_ccv, rack->r_ctl.current_round);
		}
	}
	/*
	 * For DGP an initial startup check. We want to validate
	 * that we are not just pushing on slow-start and just
	 * not gaining.. i.e. filling buffers without getting any
	 * boost in b/w during the inital slow-start.
	 */
	if (rack->dgp_on &&
	    (rack->rc_initial_ss_comp == 0) &&
	    (tp->snd_cwnd < tp->snd_ssthresh) &&
	    (rack->r_ctl.num_measurements >= RACK_REQ_AVG) &&
	    (rack->r_ctl.gp_rnd_thresh > 0) &&
	    ((rack->r_ctl.current_round - rack->r_ctl.last_rnd_of_gp_rise) >= rack->r_ctl.gp_rnd_thresh)) {

		/*
		 * We are in the initial SS and we have hd rack_rnd_cnt_req rounds(def:5) where
		 * we have not gained the required amount in the gp_est (120.0% aka 1200). Lets
		 * exit SS.
		 *
		 * Pick up the flight size now as we enter slowstart (not the
		 * cwnd which may be inflated).
		 */
		rack->rc_initial_ss_comp = 1;

		if (tcp_bblogging_on(rack->rc_tp)) {
			union tcp_log_stackspecific log;
			struct timeval tv;

			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.timeStamp = tcp_get_usecs(&tv);
			log.u_bbr.flex1 = rack->r_ctl.current_round;
			log.u_bbr.flex2 = rack->r_ctl.last_rnd_of_gp_rise;
			log.u_bbr.flex3 = rack->r_ctl.gp_rnd_thresh;
			log.u_bbr.flex5 = rack->r_ctl.gate_to_fs;
			log.u_bbr.flex5 = rack->r_ctl.ss_hi_fs;
			log.u_bbr.flex8 = 40;
			(void)tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_CWND, 0,
					    0, &log, false, NULL, __func__, __LINE__,&tv);
		}
		if ((rack->r_ctl.gate_to_fs == 1) &&
		     (tp->snd_cwnd > rack->r_ctl.ss_hi_fs)) {
			tp->snd_cwnd = rack->r_ctl.ss_hi_fs;
		}
		tp->snd_ssthresh = tp->snd_cwnd - 1;
		/* Turn off any fast output running */
		rack->r_fast_output = 0;
	}
}

static int
rack_do_compressed_ack_processing(struct tcpcb *tp, struct socket *so, struct mbuf *m, int nxt_pkt, struct timeval *tv)
{
	/*
	 * Handle a "special" compressed ack mbuf. Each incoming
	 * ack has only four possible dispositions:
	 *
	 * A) It moves the cum-ack forward
	 * B) It is behind the cum-ack.
	 * C) It is a window-update ack.
	 * D) It is a dup-ack.
	 *
	 * Note that we can have between 1 -> TCP_COMP_ACK_ENTRIES
	 * in the incoming mbuf. We also need to still pay attention
	 * to nxt_pkt since there may be another packet after this
	 * one.
	 */
#ifdef TCP_ACCOUNTING
	uint64_t ts_val;
	uint64_t rdstc;
#endif
	int segsiz;
	struct timespec ts;
	struct tcp_rack *rack;
	struct tcp_ackent *ae;
	uint32_t tiwin, ms_cts, cts, acked, acked_amount, high_seq, win_seq, the_win, win_upd_ack;
	int cnt, i, did_out, ourfinisacked = 0;
	struct tcpopt to_holder, *to = NULL;
#ifdef TCP_ACCOUNTING
	int win_up_req = 0;
#endif
	int nsegs = 0;
	int under_pacing = 0;
	int post_recovery = 0;
#ifdef TCP_ACCOUNTING
	sched_pin();
#endif
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack->gp_ready &&
	    (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT))
		under_pacing = 1;

	if (rack->r_state != tp->t_state)
		rack_set_state(tp, rack);
	if ((tp->t_state >= TCPS_FIN_WAIT_1) &&
	    (tp->t_flags & TF_GPUTINPROG)) {
		/*
		 * We have a goodput in progress
		 * and we have entered a late state.
		 * Do we have enough data in the sb
		 * to handle the GPUT request?
		 */
		uint32_t bytes;

		bytes = tp->gput_ack - tp->gput_seq;
		if (SEQ_GT(tp->gput_seq, tp->snd_una))
			bytes += tp->gput_seq - tp->snd_una;
		if (bytes > sbavail(&tptosocket(tp)->so_snd)) {
			/*
			 * There are not enough bytes in the socket
			 * buffer that have been sent to cover this
			 * measurement. Cancel it.
			 */
			rack_log_pacing_delay_calc(rack, (tp->gput_ack - tp->gput_seq) /*flex2*/,
						   rack->r_ctl.rc_gp_srtt /*flex1*/,
						   tp->gput_seq,
						   0, 0, 18, __LINE__, NULL, 0);
			tp->t_flags &= ~TF_GPUTINPROG;
		}
	}
	to = &to_holder;
	to->to_flags = 0;
	KASSERT((m->m_len >= sizeof(struct tcp_ackent)),
		("tp:%p m_cmpack:%p with invalid len:%u", tp, m, m->m_len));
	cnt = m->m_len / sizeof(struct tcp_ackent);
	counter_u64_add(rack_multi_single_eq, cnt);
	high_seq = tp->snd_una;
	the_win = tp->snd_wnd;
	win_seq = tp->snd_wl1;
	win_upd_ack = tp->snd_wl2;
	cts = tcp_tv_to_usectick(tv);
	ms_cts = tcp_tv_to_mssectick(tv);
	rack->r_ctl.rc_rcvtime = cts;
	segsiz = ctf_fixed_maxseg(tp);
	if ((rack->rc_gp_dyn_mul) &&
	    (rack->use_fixed_rate == 0) &&
	    (rack->rc_always_pace)) {
		/* Check in on probertt */
		rack_check_probe_rtt(rack, cts);
	}
	for (i = 0; i < cnt; i++) {
#ifdef TCP_ACCOUNTING
		ts_val = get_cyclecount();
#endif
		rack_clear_rate_sample(rack);
		ae = ((mtod(m, struct tcp_ackent *)) + i);
		if (ae->flags & TH_FIN)
			rack_log_pacing_delay_calc(rack,
						   0,
						   0,
						   0,
						   rack_get_gp_est(rack), /* delRate */
						   rack_get_lt_bw(rack), /* rttProp */
						   20, __LINE__, NULL, 0);
		/* Setup the window */
		tiwin = ae->win << tp->snd_scale;
		if (tiwin > rack->r_ctl.rc_high_rwnd)
			rack->r_ctl.rc_high_rwnd = tiwin;
		/* figure out the type of ack */
		if (SEQ_LT(ae->ack, high_seq)) {
			/* Case B*/
			ae->ack_val_set = ACK_BEHIND;
		} else if (SEQ_GT(ae->ack, high_seq)) {
			/* Case A */
			ae->ack_val_set = ACK_CUMACK;
		} else if ((tiwin == the_win) && (rack->rc_in_persist == 0)){
			/* Case D */
			ae->ack_val_set = ACK_DUPACK;
		} else {
			/* Case C */
			ae->ack_val_set = ACK_RWND;
		}
		if (rack->sack_attack_disable > 0) {
			rack_log_type_bbrsnd(rack, 0, 0, cts, tv, __LINE__);
			rack->r_ctl.ack_during_sd++;
		}
		rack_log_input_packet(tp, rack, ae, ae->ack_val_set, high_seq);
		/* Validate timestamp */
		if (ae->flags & HAS_TSTMP) {
			/* Setup for a timestamp */
			to->to_flags = TOF_TS;
			ae->ts_echo -= tp->ts_offset;
			to->to_tsecr = ae->ts_echo;
			to->to_tsval = ae->ts_value;
			/*
			 * If echoed timestamp is later than the current time, fall back to
			 * non RFC1323 RTT calculation.  Normalize timestamp if syncookies
			 * were used when this connection was established.
			 */
			if (TSTMP_GT(ae->ts_echo, ms_cts))
				to->to_tsecr = 0;
			if (tp->ts_recent &&
			    TSTMP_LT(ae->ts_value, tp->ts_recent)) {
				if (ctf_ts_check_ac(tp, (ae->flags & 0xff))) {
#ifdef TCP_ACCOUNTING
					rdstc = get_cyclecount();
					if (rdstc > ts_val) {
						if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
							tp->tcp_proc_time[ae->ack_val_set] += (rdstc - ts_val);
						}
					}
#endif
					continue;
				}
			}
			if (SEQ_LEQ(ae->seq, tp->last_ack_sent) &&
			    SEQ_LEQ(tp->last_ack_sent, ae->seq)) {
				tp->ts_recent_age = tcp_ts_getticks();
				tp->ts_recent = ae->ts_value;
			}
		} else {
			/* Setup for a no options */
			to->to_flags = 0;
		}
		/* Update the rcv time and perform idle reduction possibly */
		if  (tp->t_idle_reduce &&
		     (tp->snd_max == tp->snd_una) &&
		     (TICKS_2_USEC(ticks - tp->t_rcvtime) >= tp->t_rxtcur)) {
			counter_u64_add(rack_input_idle_reduces, 1);
			rack_cc_after_idle(rack, tp);
		}
		tp->t_rcvtime = ticks;
		/* Now what about ECN of a chain of pure ACKs? */
		if (tcp_ecn_input_segment(tp, ae->flags, 0,
			tcp_packets_this_ack(tp, ae->ack),
			ae->codepoint))
			rack_cong_signal(tp, CC_ECN, ae->ack, __LINE__);
#ifdef TCP_ACCOUNTING
		/* Count for the specific type of ack in */
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[ae->ack_val_set]++;
		}
#endif
		/*
		 * Note how we could move up these in the determination
		 * above, but we don't so that way the timestamp checks (and ECN)
		 * is done first before we do any processing on the ACK.
		 * The non-compressed path through the code has this
		 * weakness (noted by @jtl) that it actually does some
		 * processing before verifying the timestamp information.
		 * We don't take that path here which is why we set
		 * the ack_val_set first, do the timestamp and ecn
		 * processing, and then look at what we have setup.
		 */
		if (ae->ack_val_set == ACK_BEHIND) {
			/*
			 * Case B flag reordering, if window is not closed
			 * or it could be a keep-alive or persists
			 */
			if (SEQ_LT(ae->ack, tp->snd_una) && (sbspace(&so->so_rcv) > segsiz)) {
				rack->r_ctl.rc_reorder_ts = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
				if (rack->r_ctl.rc_reorder_ts == 0)
					rack->r_ctl.rc_reorder_ts = 1;
			}
		} else if (ae->ack_val_set == ACK_DUPACK) {
			/* Case D */
			rack_strike_dupack(rack, ae->ack);
		} else if (ae->ack_val_set == ACK_RWND) {
			/* Case C */
			if ((ae->flags & TSTMP_LRO) || (ae->flags & TSTMP_HDWR)) {
				ts.tv_sec = ae->timestamp / 1000000000;
				ts.tv_nsec = ae->timestamp % 1000000000;
				rack->r_ctl.act_rcv_time.tv_sec = ts.tv_sec;
				rack->r_ctl.act_rcv_time.tv_usec = ts.tv_nsec/1000;
			} else {
				rack->r_ctl.act_rcv_time = *tv;
			}
			if (rack->forced_ack) {
				rack_handle_probe_response(rack, tiwin,
							   tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time));
			}
#ifdef TCP_ACCOUNTING
			win_up_req = 1;
#endif
			win_upd_ack = ae->ack;
			win_seq = ae->seq;
			the_win = tiwin;
			rack_do_win_updates(tp, rack, the_win, win_seq, win_upd_ack, cts);
		} else {
			/* Case A */
			if (SEQ_GT(ae->ack, tp->snd_max)) {
				/*
				 * We just send an ack since the incoming
				 * ack is beyond the largest seq we sent.
				 */
				if ((tp->t_flags & TF_ACKNOW) == 0) {
					ctf_ack_war_checks(tp, &rack->r_ctl.challenge_ack_ts, &rack->r_ctl.challenge_ack_cnt);
					if (tp->t_flags && TF_ACKNOW)
						rack->r_wanted_output = 1;
				}
			} else {
				nsegs++;
				/* If the window changed setup to update */
				if (tiwin != tp->snd_wnd) {
					win_upd_ack = ae->ack;
					win_seq = ae->seq;
					the_win = tiwin;
					rack_do_win_updates(tp, rack, the_win, win_seq, win_upd_ack, cts);
				}
#ifdef TCP_ACCOUNTING
				/* Account for the acks */
				if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
					tp->tcp_cnt_counters[CNT_OF_ACKS_IN] += (((ae->ack - high_seq) + segsiz - 1) / segsiz);
				}
#endif
				high_seq = ae->ack;
				/* Setup our act_rcv_time */
				if ((ae->flags & TSTMP_LRO) || (ae->flags & TSTMP_HDWR)) {
					ts.tv_sec = ae->timestamp / 1000000000;
					ts.tv_nsec = ae->timestamp % 1000000000;
					rack->r_ctl.act_rcv_time.tv_sec = ts.tv_sec;
					rack->r_ctl.act_rcv_time.tv_usec = ts.tv_nsec/1000;
				} else {
					rack->r_ctl.act_rcv_time = *tv;
				}
				rack_process_to_cumack(tp, rack, ae->ack, cts, to,
						       tcp_tv_to_lusectick(&rack->r_ctl.act_rcv_time));
#ifdef TCP_REQUEST_TRK
				rack_req_check_for_comp(rack, high_seq);
#endif
				if (rack->rc_dsack_round_seen) {
					/* Is the dsack round over? */
					if (SEQ_GEQ(ae->ack, rack->r_ctl.dsack_round_end)) {
						/* Yes it is */
						rack->rc_dsack_round_seen = 0;
						rack_log_dsack_event(rack, 3, __LINE__, 0, 0);
					}
				}
			}
		}
		/* And lets be sure to commit the rtt measurements for this ack */
		tcp_rack_xmit_timer_commit(rack, tp);
#ifdef TCP_ACCOUNTING
		rdstc = get_cyclecount();
		if (rdstc > ts_val) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[ae->ack_val_set] += (rdstc - ts_val);
				if (ae->ack_val_set == ACK_CUMACK)
					tp->tcp_proc_time[CYC_HANDLE_MAP] += (rdstc - ts_val);
			}
		}
#endif
	}
#ifdef TCP_ACCOUNTING
	ts_val = get_cyclecount();
#endif
	/* Tend to any collapsed window */
	if (SEQ_GT(tp->snd_max, high_seq) && (tp->snd_wnd < (tp->snd_max - high_seq))) {
		/* The peer collapsed the window */
		rack_collapsed_window(rack, (tp->snd_max - high_seq), high_seq, __LINE__);
	} else if (rack->rc_has_collapsed)
		rack_un_collapse_window(rack, __LINE__);
	if ((rack->r_collapse_point_valid) &&
	    (SEQ_GT(high_seq, rack->r_ctl.high_collapse_point)))
		rack->r_collapse_point_valid = 0;
	acked_amount = acked = (high_seq - tp->snd_una);
	if (acked) {
		/*
		 * The draft (v3) calls for us to use SEQ_GEQ, but that
		 * causes issues when we are just going app limited. Lets
		 * instead use SEQ_GT <or> where its equal but more data
		 * is outstanding.
		 *
		 * Also make sure we are on the last ack of a series. We
		 * have to have all the ack's processed in queue to know
		 * if there is something left outstanding.
		 *
		 */
		if (SEQ_GEQ(high_seq, rack->r_ctl.roundends) &&
		    (rack->rc_new_rnd_needed == 0) &&
		    (nxt_pkt == 0)) {
			/*
			 * We have crossed into a new round with
			 * this th_ack value.
			 */
			rack_new_round_setup(tp, rack, high_seq);
		}
		/*
		 * Clear the probe not answered flag
		 * since cum-ack moved forward.
		 */
		rack->probe_not_answered = 0;
		if (rack->sack_attack_disable == 0)
			rack_do_decay(rack);
		if (acked >= segsiz) {
			/*
			 * You only get credit for
			 * MSS and greater (and you get extra
			 * credit for larger cum-ack moves).
			 */
			int ac;

			ac = acked / segsiz;
			rack->r_ctl.ack_count += ac;
			counter_u64_add(rack_ack_total, ac);
		}
		if (rack->r_ctl.ack_count > 0xfff00000) {
			/*
			 * reduce the number to keep us under
			 * a uint32_t.
			 */
			rack->r_ctl.ack_count /= 2;
			rack->r_ctl.sack_count /= 2;
		}
		if (tp->t_flags & TF_NEEDSYN) {
			/*
			 * T/TCP: Connection was half-synchronized, and our SYN has
			 * been ACK'd (so connection is now fully synchronized).  Go
			 * to non-starred state, increment snd_una for ACK of SYN,
			 * and check if we can do window scaling.
			 */
			tp->t_flags &= ~TF_NEEDSYN;
			tp->snd_una++;
			acked_amount = acked = (high_seq - tp->snd_una);
		}
		if (acked > sbavail(&so->so_snd))
			acked_amount = sbavail(&so->so_snd);
#ifdef TCP_SAD_DETECTION
		/*
		 * We only care on a cum-ack move if we are in a sack-disabled
		 * state. We have already added in to the ack_count, and we never
		 * would disable on a cum-ack move, so we only care to do the
		 * detection if it may "undo" it, i.e. we were in disabled already.
		 */
		if (rack->sack_attack_disable)
			rack_do_detection(tp, rack, acked_amount, segsiz);
#endif
		if (IN_FASTRECOVERY(tp->t_flags) &&
		    (rack->rack_no_prr == 0))
			rack_update_prr(tp, rack, acked_amount, high_seq);
		if (IN_RECOVERY(tp->t_flags)) {
			if (SEQ_LT(high_seq, tp->snd_recover) &&
			    (SEQ_LT(high_seq, tp->snd_max))) {
				tcp_rack_partialack(tp);
			} else {
				rack_post_recovery(tp, high_seq);
				post_recovery = 1;
			}
		}  else if ((rack->rto_from_rec == 1) &&
			    SEQ_GEQ(high_seq, tp->snd_recover)) {
			/*
			 * We were in recovery, hit a rxt timeout
			 * and never re-entered recovery. The timeout(s)
			 * made up all the lost data. In such a case
			 * we need to clear the rto_from_rec flag.
			 */
			rack->rto_from_rec = 0;
		}
		/* Handle the rack-log-ack part (sendmap) */
		if ((sbused(&so->so_snd) == 0) &&
		    (acked > acked_amount) &&
		    (tp->t_state >= TCPS_FIN_WAIT_1) &&
		    (tp->t_flags & TF_SENTFIN)) {
			/*
			 * We must be sure our fin
			 * was sent and acked (we can be
			 * in FIN_WAIT_1 without having
			 * sent the fin).
			 */
			ourfinisacked = 1;
			/*
			 * Lets make sure snd_una is updated
			 * since most likely acked_amount = 0 (it
			 * should be).
			 */
			tp->snd_una = high_seq;
		}
		/* Did we make a RTO error? */
		if ((tp->t_flags & TF_PREVVALID) &&
		    ((tp->t_flags & TF_RCVD_TSTMP) == 0)) {
			tp->t_flags &= ~TF_PREVVALID;
			if (tp->t_rxtshift == 1 &&
			    (int)(ticks - tp->t_badrxtwin) < 0)
				rack_cong_signal(tp, CC_RTO_ERR, high_seq, __LINE__);
		}
		/* Handle the data in the socket buffer */
		KMOD_TCPSTAT_ADD(tcps_rcvackpack, 1);
		KMOD_TCPSTAT_ADD(tcps_rcvackbyte, acked);
		if (acked_amount > 0) {
			uint32_t p_cwnd;
			struct mbuf *mfree;

			if (post_recovery) {
				/*
				 * Grab the segsiz, multiply by 2 and add the snd_cwnd
				 * that is the max the CC should add if we are exiting
				 * recovery and doing a late add.
				 */
				p_cwnd = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
				p_cwnd <<= 1;
				p_cwnd += tp->snd_cwnd;
			}
			rack_ack_received(tp, rack, high_seq, nsegs, CC_ACK, post_recovery);
			if (post_recovery && (tp->snd_cwnd > p_cwnd)) {
				/* Must be non-newreno (cubic) getting too ahead of itself */
				tp->snd_cwnd = p_cwnd;
			}
			SOCKBUF_LOCK(&so->so_snd);
			mfree = sbcut_locked(&so->so_snd, acked_amount);
			tp->snd_una = high_seq;
			/* Note we want to hold the sb lock through the sendmap adjust */
			rack_adjust_sendmap_head(rack, &so->so_snd);
			/* Wake up the socket if we have room to write more */
			rack_log_wakeup(tp,rack, &so->so_snd, acked, 2);
			sowwakeup_locked(so);
			m_freem(mfree);
		}
		/* update progress */
		tp->t_acktime = ticks;
		rack_log_progress_event(rack, tp, tp->t_acktime,
					PROGRESS_UPDATE, __LINE__);
		/* Clear out shifts and such */
		tp->t_rxtshift = 0;
		RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
				   rack_rto_min, rack_rto_max, rack->r_ctl.timer_slop);
		rack->rc_tlp_in_progress = 0;
		rack->r_ctl.rc_tlp_cnt_out = 0;
		/* Send recover and snd_nxt must be dragged along */
		if (SEQ_GT(tp->snd_una, tp->snd_recover))
			tp->snd_recover = tp->snd_una;
		if (SEQ_LT(tp->snd_nxt, tp->snd_max))
			tp->snd_nxt = tp->snd_max;
		/*
		 * If the RXT timer is running we want to
		 * stop it, so we can restart a TLP (or new RXT).
		 */
		if (rack->r_ctl.rc_hpts_flags & PACE_TMR_RXT)
			rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
		tp->snd_wl2 = high_seq;
		tp->t_dupacks = 0;
		if (under_pacing &&
		    (rack->use_fixed_rate == 0) &&
		    (rack->in_probe_rtt == 0) &&
		    rack->rc_gp_dyn_mul &&
		    rack->rc_always_pace) {
			/* Check if we are dragging bottom */
			rack_check_bottom_drag(tp, rack, so);
		}
		if (tp->snd_una == tp->snd_max) {
			tp->t_flags &= ~TF_PREVVALID;
			rack->r_ctl.retran_during_recovery = 0;
			rack->rc_suspicious = 0;
			rack->r_ctl.dsack_byte_cnt = 0;
			rack->r_ctl.rc_went_idle_time = tcp_get_usecs(NULL);
			if (rack->r_ctl.rc_went_idle_time == 0)
				rack->r_ctl.rc_went_idle_time = 1;
			rack_log_progress_event(rack, tp, 0, PROGRESS_CLEAR, __LINE__);
			if (sbavail(&tptosocket(tp)->so_snd) == 0)
				tp->t_acktime = 0;
			/* Set so we might enter persists... */
			rack->r_wanted_output = 1;
			rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
			sack_filter_clear(&rack->r_ctl.rack_sf, tp->snd_una);
			if ((tp->t_state >= TCPS_FIN_WAIT_1) &&
			    (sbavail(&so->so_snd) == 0) &&
			    (tp->t_flags2 & TF2_DROP_AF_DATA)) {
				/*
				 * The socket was gone and the
				 * peer sent data (not now in the past), time to
				 * reset him.
				 */
				rack_timer_cancel(tp, rack, rack->r_ctl.rc_rcvtime, __LINE__);
				/* tcp_close will kill the inp pre-log the Reset */
				tcp_log_end_status(tp, TCP_EI_STATUS_SERVER_RST);
#ifdef TCP_ACCOUNTING
				rdstc = get_cyclecount();
				if (rdstc > ts_val) {
					if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
						tp->tcp_proc_time[ACK_CUMACK] += (rdstc - ts_val);
						tp->tcp_proc_time[CYC_HANDLE_ACK] += (rdstc - ts_val);
					}
				}
#endif
				m_freem(m);
				tp = tcp_close(tp);
				if (tp == NULL) {
#ifdef TCP_ACCOUNTING
					sched_unpin();
#endif
					return (1);
				}
				/*
				 * We would normally do drop-with-reset which would
				 * send back a reset. We can't since we don't have
				 * all the needed bits. Instead lets arrange for
				 * a call to tcp_output(). That way since we
				 * are in the closed state we will generate a reset.
				 *
				 * Note if tcp_accounting is on we don't unpin since
				 * we do that after the goto label.
				 */
				goto send_out_a_rst;
			}
			if ((sbused(&so->so_snd) == 0) &&
			    (tp->t_state >= TCPS_FIN_WAIT_1) &&
			    (tp->t_flags & TF_SENTFIN)) {
				/*
				 * If we can't receive any more data, then closing user can
				 * proceed. Starting the timer is contrary to the
				 * specification, but if we don't get a FIN we'll hang
				 * forever.
				 *
				 */
				if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
					soisdisconnected(so);
					tcp_timer_activate(tp, TT_2MSL,
							   (tcp_fast_finwait2_recycle ?
							    tcp_finwait2_timeout :
							    TP_MAXIDLE(tp)));
				}
				if (ourfinisacked == 0) {
					/*
					 * We don't change to fin-wait-2 if we have our fin acked
					 * which means we are probably in TCPS_CLOSING.
					 */
					tcp_state_change(tp, TCPS_FIN_WAIT_2);
				}
			}
		}
		/* Wake up the socket if we have room to write more */
		if (sbavail(&so->so_snd)) {
			rack->r_wanted_output = 1;
			if (ctf_progress_timeout_check(tp, true)) {
				rack_log_progress_event((struct tcp_rack *)tp->t_fb_ptr,
							tp, tick, PROGRESS_DROP, __LINE__);
				/*
				 * We cheat here and don't send a RST, we should send one
				 * when the pacer drops the connection.
				 */
#ifdef TCP_ACCOUNTING
				rdstc = get_cyclecount();
				if (rdstc > ts_val) {
					if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
						tp->tcp_proc_time[ACK_CUMACK] += (rdstc - ts_val);
						tp->tcp_proc_time[CYC_HANDLE_ACK] += (rdstc - ts_val);
					}
				}
				sched_unpin();
#endif
				(void)tcp_drop(tp, ETIMEDOUT);
				m_freem(m);
				return (1);
			}
		}
		if (ourfinisacked) {
			switch(tp->t_state) {
			case TCPS_CLOSING:
#ifdef TCP_ACCOUNTING
				rdstc = get_cyclecount();
				if (rdstc > ts_val) {
					if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
						tp->tcp_proc_time[ACK_CUMACK] += (rdstc - ts_val);
						tp->tcp_proc_time[CYC_HANDLE_ACK] += (rdstc - ts_val);
					}
				}
				sched_unpin();
#endif
				tcp_twstart(tp);
				m_freem(m);
				return (1);
				break;
			case TCPS_LAST_ACK:
#ifdef TCP_ACCOUNTING
				rdstc = get_cyclecount();
				if (rdstc > ts_val) {
					if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
						tp->tcp_proc_time[ACK_CUMACK] += (rdstc - ts_val);
						tp->tcp_proc_time[CYC_HANDLE_ACK] += (rdstc - ts_val);
					}
				}
				sched_unpin();
#endif
				tp = tcp_close(tp);
				ctf_do_drop(m, tp);
				return (1);
				break;
			case TCPS_FIN_WAIT_1:
#ifdef TCP_ACCOUNTING
				rdstc = get_cyclecount();
				if (rdstc > ts_val) {
					if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
						tp->tcp_proc_time[ACK_CUMACK] += (rdstc - ts_val);
						tp->tcp_proc_time[CYC_HANDLE_ACK] += (rdstc - ts_val);
					}
				}
#endif
				if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
					soisdisconnected(so);
					tcp_timer_activate(tp, TT_2MSL,
							   (tcp_fast_finwait2_recycle ?
							    tcp_finwait2_timeout :
							    TP_MAXIDLE(tp)));
				}
				tcp_state_change(tp, TCPS_FIN_WAIT_2);
				break;
			default:
				break;
			}
		}
		if (rack->r_fast_output) {
			/*
			 * We re doing fast output.. can we expand that?
			 */
			rack_gain_for_fastoutput(rack, tp, so, acked_amount);
		}
#ifdef TCP_ACCOUNTING
		rdstc = get_cyclecount();
		if (rdstc > ts_val) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[ACK_CUMACK] += (rdstc - ts_val);
				tp->tcp_proc_time[CYC_HANDLE_ACK] += (rdstc - ts_val);
			}
		}

	} else if (win_up_req) {
		rdstc = get_cyclecount();
		if (rdstc > ts_val) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[ACK_RWND] += (rdstc - ts_val);
			}
		}
#endif
	}
	/* Now is there a next packet, if so we are done */
	m_freem(m);
	did_out = 0;
	if (nxt_pkt) {
#ifdef TCP_ACCOUNTING
		sched_unpin();
#endif
		rack_log_doseg_done(rack, cts, nxt_pkt, did_out, 5, nsegs);
		return (0);
	}
	rack_handle_might_revert(tp, rack);
	ctf_calc_rwin(so, tp);
	if ((rack->r_wanted_output != 0) ||
	    (rack->r_fast_output != 0) ||
	    (tp->t_flags & TF_ACKNOW )) {
	send_out_a_rst:
		if (tcp_output(tp) < 0) {
#ifdef TCP_ACCOUNTING
			sched_unpin();
#endif
			return (1);
		}
		did_out = 1;
	}
	if (tp->t_flags2 & TF2_HPTS_CALLS)
		tp->t_flags2 &= ~TF2_HPTS_CALLS;
	rack_free_trim(rack);
#ifdef TCP_ACCOUNTING
	sched_unpin();
#endif
	rack_timer_audit(tp, rack, &so->so_snd);
	rack_log_doseg_done(rack, cts, nxt_pkt, did_out, 6, nsegs);
	return (0);
}

#define	TCP_LRO_TS_OPTION \
    ntohl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) | \
	  (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP)

static int
rack_do_segment_nounlock(struct tcpcb *tp, struct mbuf *m, struct tcphdr *th,
    int32_t drop_hdrlen, int32_t tlen, uint8_t iptos, int32_t nxt_pkt,
    struct timeval *tv)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = tptosocket(tp);
#ifdef TCP_ACCOUNTING
	uint64_t ts_val;
#endif
	int32_t thflags, retval, did_out = 0;
	int32_t way_out = 0;
	/*
	 * cts - is the current time from tv (caller gets ts) in microseconds.
	 * ms_cts - is the current time from tv in milliseconds.
	 * us_cts - is the time that LRO or hardware actually got the packet in microseconds.
	 */
	uint32_t cts, us_cts, ms_cts;
	uint32_t tiwin;
	struct timespec ts;
	struct tcpopt to;
	struct tcp_rack *rack;
	struct rack_sendmap *rsm;
	int32_t prev_state = 0;
	int no_output = 0;
	int slot_remaining = 0;
#ifdef TCP_ACCOUNTING
	int ack_val_set = 0xf;
#endif
	int nsegs;

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);

	/*
	 * tv passed from common code is from either M_TSTMP_LRO or
	 * tcp_get_usecs() if no LRO m_pkthdr timestamp is present.
	 */
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack->rack_deferred_inited == 0) {
		/*
		 * If we are the connecting socket we will
		 * hit rack_init() when no sequence numbers
		 * are setup. This makes it so we must defer
		 * some initialization. Call that now.
		 */
		rack_deferred_init(tp, rack);
	}
	/*
	 * Check to see if we need to skip any output plans. This
	 * can happen in the non-LRO path where we are pacing and
	 * must process the ack coming in but need to defer sending
	 * anything becase a pacing timer is running.
	 */
	us_cts = tcp_tv_to_usectick(tv);
	if (m->m_flags & M_ACKCMP) {
		/*
		 * All compressed ack's are ack's by definition so
		 * remove any ack required flag and then do the processing.
		 */
		rack->rc_ack_required = 0;
		return (rack_do_compressed_ack_processing(tp, so, m, nxt_pkt, tv));
	}
	thflags = tcp_get_flags(th);
	if ((rack->rc_always_pace == 1) &&
	    (rack->rc_ack_can_sendout_data == 0) &&
	    (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) &&
	    (TSTMP_LT(us_cts, rack->r_ctl.rc_last_output_to))) {
		/*
		 * Ok conditions are right for queuing the packets
		 * but we do have to check the flags in the inp, it
		 * could be, if a sack is present, we want to be awoken and
		 * so should process the packets.
		 */
		slot_remaining = rack->r_ctl.rc_last_output_to - us_cts;
		if (rack->rc_tp->t_flags2 & TF2_DONT_SACK_QUEUE) {
			no_output = 1;
		} else {
			/*
			 * If there is no options, or just a
			 * timestamp option, we will want to queue
			 * the packets. This is the same that LRO does
			 * and will need to change with accurate ECN.
			 */
			uint32_t *ts_ptr;
			int optlen;

			optlen = (th->th_off << 2) - sizeof(struct tcphdr);
			ts_ptr = (uint32_t *)(th + 1);
			if ((optlen == 0) ||
			    ((optlen == TCPOLEN_TSTAMP_APPA) &&
			     (*ts_ptr == TCP_LRO_TS_OPTION)))
				no_output = 1;
		}
		if ((no_output == 1) && (slot_remaining < tcp_min_hptsi_time)) {
			/*
			 * It is unrealistic to think we can pace in less than
			 * the minimum granularity of the pacer (def:250usec). So
			 * if we have less than that time remaining we should go
			 * ahead and allow output to be "early". We will attempt to
			 * make up for it in any pacing time we try to apply on
			 * the outbound packet.
			 */
			no_output = 0;
		}
	}
	/*
	 * If there is a RST or FIN lets dump out the bw
	 * with a FIN the connection may go on but we
	 * may not.
	 */
	if ((thflags & TH_FIN) || (thflags & TH_RST))
		rack_log_pacing_delay_calc(rack,
					   rack->r_ctl.gp_bw,
					   0,
					   0,
					   rack_get_gp_est(rack), /* delRate */
					   rack_get_lt_bw(rack), /* rttProp */
					   20, __LINE__, NULL, 0);
	if (m->m_flags & M_ACKCMP) {
		panic("Impossible reach m has ackcmp? m:%p tp:%p", m, tp);
	}
	cts = tcp_tv_to_usectick(tv);
	ms_cts =  tcp_tv_to_mssectick(tv);
	nsegs = m->m_pkthdr.lro_nsegs;
	counter_u64_add(rack_proc_non_comp_ack, 1);
#ifdef TCP_ACCOUNTING
	sched_pin();
	if (thflags & TH_ACK)
		ts_val = get_cyclecount();
#endif
	if ((m->m_flags & M_TSTMP) ||
	    (m->m_flags & M_TSTMP_LRO)) {
		mbuf_tstmp2timespec(m, &ts);
		rack->r_ctl.act_rcv_time.tv_sec = ts.tv_sec;
		rack->r_ctl.act_rcv_time.tv_usec = ts.tv_nsec/1000;
	} else
		rack->r_ctl.act_rcv_time = *tv;
	kern_prefetch(rack, &prev_state);
	prev_state = 0;
	/*
	 * Unscale the window into a 32-bit value. For the SYN_SENT state
	 * the scale is zero.
	 */
	tiwin = th->th_win << tp->snd_scale;
#ifdef TCP_ACCOUNTING
	if (thflags & TH_ACK) {
		/*
		 * We have a tradeoff here. We can either do what we are
		 * doing i.e. pinning to this CPU and then doing the accounting
		 * <or> we could do a critical enter, setup the rdtsc and cpu
		 * as in below, and then validate we are on the same CPU on
		 * exit. I have choosen to not do the critical enter since
		 * that often will gain you a context switch, and instead lock
		 * us (line above this if) to the same CPU with sched_pin(). This
		 * means we may be context switched out for a higher priority
		 * interupt but we won't be moved to another CPU.
		 *
		 * If this occurs (which it won't very often since we most likely
		 * are running this code in interupt context and only a higher
		 * priority will bump us ... clock?) we will falsely add in
		 * to the time the interupt processing time plus the ack processing
		 * time. This is ok since its a rare event.
		 */
		ack_val_set = tcp_do_ack_accounting(tp, th, &to, tiwin,
						    ctf_fixed_maxseg(tp));
	}
#endif
	/*
	 * Parse options on any incoming segment.
	 */
	memset(&to, 0, sizeof(to));
	tcp_dooptions(&to, (u_char *)(th + 1),
	    (th->th_off << 2) - sizeof(struct tcphdr),
	    (thflags & TH_SYN) ? TO_SYN : 0);
	KASSERT(tp->t_state > TCPS_LISTEN, ("%s: TCPS_LISTEN",
	    __func__));
	KASSERT(tp->t_state != TCPS_TIME_WAIT, ("%s: TCPS_TIME_WAIT",
	    __func__));

	if ((tp->t_state >= TCPS_FIN_WAIT_1) &&
	    (tp->t_flags & TF_GPUTINPROG)) {
		/*
		 * We have a goodput in progress
		 * and we have entered a late state.
		 * Do we have enough data in the sb
		 * to handle the GPUT request?
		 */
		uint32_t bytes;

		bytes = tp->gput_ack - tp->gput_seq;
		if (SEQ_GT(tp->gput_seq, tp->snd_una))
			bytes += tp->gput_seq - tp->snd_una;
		if (bytes > sbavail(&tptosocket(tp)->so_snd)) {
			/*
			 * There are not enough bytes in the socket
			 * buffer that have been sent to cover this
			 * measurement. Cancel it.
			 */
			rack_log_pacing_delay_calc(rack, (tp->gput_ack - tp->gput_seq) /*flex2*/,
						   rack->r_ctl.rc_gp_srtt /*flex1*/,
						   tp->gput_seq,
						   0, 0, 18, __LINE__, NULL, 0);
			tp->t_flags &= ~TF_GPUTINPROG;
		}
	}
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval ltv;
#ifdef TCP_REQUEST_TRK
		struct tcp_sendfile_track *tcp_req;

		if (SEQ_GT(th->th_ack, tp->snd_una)) {
			tcp_req = tcp_req_find_req_for_seq(tp, (th->th_ack-1));
		} else {
			tcp_req = tcp_req_find_req_for_seq(tp, th->th_ack);
		}
#endif
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		if (rack->rack_no_prr == 0)
			log.u_bbr.flex1 = rack->r_ctl.rc_prr_sndcnt;
		else
			log.u_bbr.flex1 = 0;
		log.u_bbr.use_lt_bw = rack->r_ent_rec_ns;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->r_might_revert;
		log.u_bbr.flex2 = rack->r_ctl.rc_num_maps_alloced;
		log.u_bbr.bbr_state = rack->rc_free_cnt;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.pkts_out = rack->rc_tp->t_maxseg;
		log.u_bbr.flex3 = m->m_flags;
		log.u_bbr.flex4 = rack->r_ctl.rc_hpts_flags;
		log.u_bbr.lost = thflags;
		log.u_bbr.pacing_gain = 0x1;
#ifdef TCP_ACCOUNTING
		log.u_bbr.cwnd_gain = ack_val_set;
#endif
		log.u_bbr.flex7 = 2;
		if (m->m_flags & M_TSTMP) {
			/* Record the hardware timestamp if present */
			mbuf_tstmp2timespec(m, &ts);
			ltv.tv_sec = ts.tv_sec;
			ltv.tv_usec = ts.tv_nsec / 1000;
			log.u_bbr.lt_epoch = tcp_tv_to_usectick(&ltv);
		} else if (m->m_flags & M_TSTMP_LRO) {
			/* Record the LRO the arrival timestamp */
			mbuf_tstmp2timespec(m, &ts);
			ltv.tv_sec = ts.tv_sec;
			ltv.tv_usec = ts.tv_nsec / 1000;
			log.u_bbr.flex5 = tcp_tv_to_usectick(&ltv);
		}
		log.u_bbr.timeStamp = tcp_get_usecs(&ltv);
		/* Log the rcv time */
		log.u_bbr.delRate = m->m_pkthdr.rcv_tstmp;
#ifdef TCP_REQUEST_TRK
		log.u_bbr.applimited = tp->t_tcpreq_closed;
		log.u_bbr.applimited <<= 8;
		log.u_bbr.applimited |= tp->t_tcpreq_open;
		log.u_bbr.applimited <<= 8;
		log.u_bbr.applimited |= tp->t_tcpreq_req;
		if (tcp_req) {
			/* Copy out any client req info */
			/* seconds */
			log.u_bbr.pkt_epoch = (tcp_req->localtime / HPTS_USEC_IN_SEC);
			/* useconds */
			log.u_bbr.delivered = (tcp_req->localtime % HPTS_USEC_IN_SEC);
			log.u_bbr.rttProp = tcp_req->timestamp;
			log.u_bbr.cur_del_rate = tcp_req->start;
			if (tcp_req->flags & TCP_TRK_TRACK_FLG_OPEN) {
				log.u_bbr.flex8 |= 1;
			} else {
				log.u_bbr.flex8 |= 2;
				log.u_bbr.bw_inuse = tcp_req->end;
			}
			log.u_bbr.flex6 = tcp_req->start_seq;
			if (tcp_req->flags & TCP_TRK_TRACK_FLG_COMP) {
				log.u_bbr.flex8 |= 4;
				log.u_bbr.epoch = tcp_req->end_seq;
			}
		}
#endif
		TCP_LOG_EVENTP(tp, th, &so->so_rcv, &so->so_snd, TCP_LOG_IN, 0,
		    tlen, &log, true, &ltv);
	}
	/* Remove ack required flag if set, we have one  */
	if (thflags & TH_ACK)
		rack->rc_ack_required = 0;
	if (rack->sack_attack_disable > 0) {
		rack->r_ctl.ack_during_sd++;
		rack_log_type_bbrsnd(rack, 0, 0, cts, tv, __LINE__);
	}
	if ((thflags & TH_SYN) && (thflags & TH_FIN) && V_drop_synfin) {
		way_out = 4;
		retval = 0;
		m_freem(m);
		goto done_with_input;
	}
	/*
	 * If a segment with the ACK-bit set arrives in the SYN-SENT state
	 * check SEQ.ACK first as described on page 66 of RFC 793, section 3.9.
	 */
	if ((tp->t_state == TCPS_SYN_SENT) && (thflags & TH_ACK) &&
	    (SEQ_LEQ(th->th_ack, tp->iss) || SEQ_GT(th->th_ack, tp->snd_max))) {
		tcp_log_end_status(tp, TCP_EI_STATUS_RST_IN_FRONT);
		ctf_do_dropwithreset(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
#ifdef TCP_ACCOUNTING
		sched_unpin();
#endif
		return (1);
	}
	/*
	 * If timestamps were negotiated during SYN/ACK and a
	 * segment without a timestamp is received, silently drop
	 * the segment, unless it is a RST segment or missing timestamps are
	 * tolerated.
	 * See section 3.2 of RFC 7323.
	 */
	if ((tp->t_flags & TF_RCVD_TSTMP) && !(to.to_flags & TOF_TS) &&
	    ((thflags & TH_RST) == 0) && (V_tcp_tolerate_missing_ts == 0)) {
		way_out = 5;
		retval = 0;
		m_freem(m);
		goto done_with_input;
	}
	/*
	 * Segment received on connection. Reset idle time and keep-alive
	 * timer. XXX: This should be done after segment validation to
	 * ignore broken/spoofed segs.
	 */
	if  (tp->t_idle_reduce &&
	     (tp->snd_max == tp->snd_una) &&
	     (TICKS_2_USEC(ticks - tp->t_rcvtime) >= tp->t_rxtcur)) {
		counter_u64_add(rack_input_idle_reduces, 1);
		rack_cc_after_idle(rack, tp);
	}
	tp->t_rcvtime = ticks;
#ifdef STATS
	stats_voi_update_abs_ulong(tp->t_stats, VOI_TCP_FRWIN, tiwin);
#endif
	if (tiwin > rack->r_ctl.rc_high_rwnd)
		rack->r_ctl.rc_high_rwnd = tiwin;
	/*
	 * TCP ECN processing. XXXJTL: If we ever use ECN, we need to move
	 * this to occur after we've validated the segment.
	 */
	if (tcp_ecn_input_segment(tp, thflags, tlen,
	    tcp_packets_this_ack(tp, th->th_ack),
	    iptos))
		rack_cong_signal(tp, CC_ECN, th->th_ack, __LINE__);

	/*
	 * If echoed timestamp is later than the current time, fall back to
	 * non RFC1323 RTT calculation.  Normalize timestamp if syncookies
	 * were used when this connection was established.
	 */
	if ((to.to_flags & TOF_TS) && (to.to_tsecr != 0)) {
		to.to_tsecr -= tp->ts_offset;
		if (TSTMP_GT(to.to_tsecr, ms_cts))
			to.to_tsecr = 0;
	}
	if ((rack->r_rcvpath_rtt_up == 1) &&
	    (to.to_flags & TOF_TS) &&
	    (TSTMP_GEQ(to.to_tsecr, rack->r_ctl.last_rcv_tstmp_for_rtt))) {
		uint32_t rtt = 0;

		/*
		 * We are receiving only and thus not sending
		 * data to do an RTT. We set a flag when we first
		 * sent this TS to the peer. We now have it back
		 * and have an RTT to share. We log it as a conf
		 * 4, we are not so sure about it.. since we
		 * may have lost an ack.
		 */
		if (TSTMP_GT(cts, rack->r_ctl.last_time_of_arm_rcv))
		    rtt = (cts - rack->r_ctl.last_time_of_arm_rcv);
		rack->r_rcvpath_rtt_up = 0;
		/* Submit and commit the timer */
		if (rtt > 0) {
			tcp_rack_xmit_timer(rack, rtt, 0, rtt, 4, NULL, 1);
			tcp_rack_xmit_timer_commit(rack, tp);
		}
	}
	/*
	 * If its the first time in we need to take care of options and
	 * verify we can do SACK for rack!
	 */
	if (rack->r_state == 0) {
		/* Should be init'd by rack_init() */
		KASSERT(rack->rc_inp != NULL,
		    ("%s: rack->rc_inp unexpectedly NULL", __func__));
		if (rack->rc_inp == NULL) {
			rack->rc_inp = inp;
		}

		/*
		 * Process options only when we get SYN/ACK back. The SYN
		 * case for incoming connections is handled in tcp_syncache.
		 * According to RFC1323 the window field in a SYN (i.e., a
		 * <SYN> or <SYN,ACK>) segment itself is never scaled. XXX
		 * this is traditional behavior, may need to be cleaned up.
		 */
		if (tp->t_state == TCPS_SYN_SENT && (thflags & TH_SYN)) {
			/* Handle parallel SYN for ECN */
			tcp_ecn_input_parallel_syn(tp, thflags, iptos);
			if ((to.to_flags & TOF_SCALE) &&
			    (tp->t_flags & TF_REQ_SCALE)) {
				tp->t_flags |= TF_RCVD_SCALE;
				tp->snd_scale = to.to_wscale;
			} else
				tp->t_flags &= ~TF_REQ_SCALE;
			/*
			 * Initial send window.  It will be updated with the
			 * next incoming segment to the scaled value.
			 */
			tp->snd_wnd = th->th_win;
			rack_validate_fo_sendwin_up(tp, rack);
			if ((to.to_flags & TOF_TS) &&
			    (tp->t_flags & TF_REQ_TSTMP)) {
				tp->t_flags |= TF_RCVD_TSTMP;
				tp->ts_recent = to.to_tsval;
				tp->ts_recent_age = cts;
			} else
				tp->t_flags &= ~TF_REQ_TSTMP;
			if (to.to_flags & TOF_MSS) {
				tcp_mss(tp, to.to_mss);
			}
			if ((tp->t_flags & TF_SACK_PERMIT) &&
			    (to.to_flags & TOF_SACKPERM) == 0)
				tp->t_flags &= ~TF_SACK_PERMIT;
			if (tp->t_flags & TF_FASTOPEN) {
				if (to.to_flags & TOF_FASTOPEN) {
					uint16_t mss;

					if (to.to_flags & TOF_MSS)
						mss = to.to_mss;
					else
						if ((inp->inp_vflag & INP_IPV6) != 0)
							mss = TCP6_MSS;
						else
							mss = TCP_MSS;
					tcp_fastopen_update_cache(tp, mss,
					    to.to_tfo_len, to.to_tfo_cookie);
				} else
					tcp_fastopen_disable_path(tp);
			}
		}
		/*
		 * At this point we are at the initial call. Here we decide
		 * if we are doing RACK or not. We do this by seeing if
		 * TF_SACK_PERMIT is set and the sack-not-required is clear.
		 * The code now does do dup-ack counting so if you don't
		 * switch back you won't get rack & TLP, but you will still
		 * get this stack.
		 */

		if ((rack_sack_not_required == 0) &&
		    ((tp->t_flags & TF_SACK_PERMIT) == 0)) {
			tcp_switch_back_to_default(tp);
			(*tp->t_fb->tfb_tcp_do_segment)(tp, m, th, drop_hdrlen,
			    tlen, iptos);
#ifdef TCP_ACCOUNTING
			sched_unpin();
#endif
			return (1);
		}
		tcp_set_hpts(tp);
		sack_filter_clear(&rack->r_ctl.rack_sf, th->th_ack);
	}
	if (thflags & TH_FIN)
		tcp_log_end_status(tp, TCP_EI_STATUS_CLIENT_FIN);
	us_cts = tcp_tv_to_usectick(&rack->r_ctl.act_rcv_time);
	if ((rack->rc_gp_dyn_mul) &&
	    (rack->use_fixed_rate == 0) &&
	    (rack->rc_always_pace)) {
		/* Check in on probertt */
		rack_check_probe_rtt(rack, cts);
	}
	rack_clear_rate_sample(rack);
	if ((rack->forced_ack) &&
	    ((tcp_get_flags(th) & TH_RST) == 0)) {
		rack_handle_probe_response(rack, tiwin, us_cts);
	}
	/*
	 * This is the one exception case where we set the rack state
	 * always. All other times (timers etc) we must have a rack-state
	 * set (so we assure we have done the checks above for SACK).
	 */
	rack->r_ctl.rc_rcvtime = cts;
	if (rack->r_state != tp->t_state)
		rack_set_state(tp, rack);
	if (SEQ_GT(th->th_ack, tp->snd_una) &&
	    (rsm = tqhash_min(rack->r_ctl.tqh)) != NULL)
		kern_prefetch(rsm, &prev_state);
	prev_state = rack->r_state;
	if ((thflags & TH_RST) &&
	    ((SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
	      SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) ||
	     (tp->rcv_wnd == 0 && tp->last_ack_sent == th->th_seq))) {
		/* The connection will be killed by a reset check the tracepoint */
		tcp_trace_point(rack->rc_tp, TCP_TP_RESET_RCV);
	}
	retval = (*rack->r_substate) (m, th, so,
	    tp, &to, drop_hdrlen,
	    tlen, tiwin, thflags, nxt_pkt, iptos);
	if (retval == 0) {
		/*
		 * If retval is 1 the tcb is unlocked and most likely the tp
		 * is gone.
		 */
		INP_WLOCK_ASSERT(inp);
		if ((rack->rc_gp_dyn_mul) &&
		    (rack->rc_always_pace) &&
		    (rack->use_fixed_rate == 0) &&
		    rack->in_probe_rtt &&
		    (rack->r_ctl.rc_time_probertt_starts == 0)) {
			/*
			 * If we are going for target, lets recheck before
			 * we output.
			 */
			rack_check_probe_rtt(rack, cts);
		}
		if (rack->set_pacing_done_a_iw == 0) {
			/* How much has been acked? */
			if ((tp->snd_una - tp->iss) > (ctf_fixed_maxseg(tp) * 10)) {
				/* We have enough to set in the pacing segment size */
				rack->set_pacing_done_a_iw = 1;
				rack_set_pace_segments(tp, rack, __LINE__, NULL);
			}
		}
		tcp_rack_xmit_timer_commit(rack, tp);
#ifdef TCP_ACCOUNTING
		/*
		 * If we set the ack_val_se to what ack processing we are doing
		 * we also want to track how many cycles we burned. Note
		 * the bits after tcp_output we let be "free". This is because
		 * we are also tracking the tcp_output times as well. Note the
		 * use of 0xf here since we only have 11 counter (0 - 0xa) and
		 * 0xf cannot be returned and is what we initialize it too to
		 * indicate we are not doing the tabulations.
		 */
		if (ack_val_set != 0xf) {
			uint64_t crtsc;

			crtsc = get_cyclecount();
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[ack_val_set] += (crtsc - ts_val);
			}
		}
#endif
		if ((nxt_pkt == 0) && (no_output == 0)) {
			if ((rack->r_wanted_output != 0) ||
			    (tp->t_flags & TF_ACKNOW) ||
			    (rack->r_fast_output != 0)) {

do_output_now:
				if (tcp_output(tp) < 0) {
#ifdef TCP_ACCOUNTING
					sched_unpin();
#endif
					return (1);
				}
				did_out = 1;
			}
			rack_start_hpts_timer(rack, tp, cts, 0, 0, 0);
			rack_free_trim(rack);
		} else if ((nxt_pkt == 0) && (tp->t_flags & TF_ACKNOW)) {
			goto do_output_now;
		} else if ((no_output == 1) &&
			   (nxt_pkt == 0)  &&
			   (tcp_in_hpts(rack->rc_tp) == 0)) {
			/*
			 * We are not in hpts and we had a pacing timer up. Use
			 * the remaining time (slot_remaining) to restart the timer.
			 */
			KASSERT ((slot_remaining != 0), ("slot remaining is zero for rack:%p tp:%p", rack, tp));
			rack_start_hpts_timer(rack, tp, cts, slot_remaining, 0, 0);
			rack_free_trim(rack);
		}
		/* Clear the flag, it may have been cleared by output but we may not have  */
		if ((nxt_pkt == 0) && (tp->t_flags2 & TF2_HPTS_CALLS))
			tp->t_flags2 &= ~TF2_HPTS_CALLS;
		/*
		 * The draft (v3) calls for us to use SEQ_GEQ, but that
		 * causes issues when we are just going app limited. Lets
		 * instead use SEQ_GT <or> where its equal but more data
		 * is outstanding.
		 *
		 * Also make sure we are on the last ack of a series. We
		 * have to have all the ack's processed in queue to know
		 * if there is something left outstanding.
		 */
		if (SEQ_GEQ(tp->snd_una, rack->r_ctl.roundends) &&
		    (rack->rc_new_rnd_needed == 0) &&
		    (nxt_pkt == 0)) {
			/*
			 * We have crossed into a new round with
			 * the new snd_unae.
			 */
			rack_new_round_setup(tp, rack, tp->snd_una);
		}
		if ((nxt_pkt == 0) &&
		    ((rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK) == 0) &&
		    (SEQ_GT(tp->snd_max, tp->snd_una) ||
		     (tp->t_flags & TF_DELACK) ||
		     ((V_tcp_always_keepalive || rack->rc_inp->inp_socket->so_options & SO_KEEPALIVE) &&
		      (tp->t_state <= TCPS_CLOSING)))) {
			/* We could not send (probably in the hpts but stopped the timer earlier)? */
			if ((tp->snd_max == tp->snd_una) &&
			    ((tp->t_flags & TF_DELACK) == 0) &&
			    (tcp_in_hpts(rack->rc_tp)) &&
			    (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT)) {
				/* keep alive not needed if we are hptsi output yet */
				;
			} else {
				int late = 0;
				if (tcp_in_hpts(tp)) {
					if (rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) {
						us_cts = tcp_get_usecs(NULL);
						if (TSTMP_GT(rack->r_ctl.rc_last_output_to, us_cts)) {
							rack->r_early = 1;
							rack->r_ctl.rc_agg_early += (rack->r_ctl.rc_last_output_to - us_cts);
						} else
							late = 1;
						rack->r_ctl.rc_hpts_flags &= ~PACE_PKT_OUTPUT;
					}
					tcp_hpts_remove(tp);
				}
				if (late && (did_out == 0)) {
					/*
					 * We are late in the sending
					 * and we did not call the output
					 * (this probably should not happen).
					 */
					goto do_output_now;
				}
				rack_start_hpts_timer(rack, tp, tcp_get_usecs(NULL), 0, 0, 0);
			}
			way_out = 1;
		} else if (nxt_pkt == 0) {
			/* Do we have the correct timer running? */
			rack_timer_audit(tp, rack, &so->so_snd);
			way_out = 2;
		}
	done_with_input:
		rack_log_doseg_done(rack, cts, nxt_pkt, did_out, way_out, max(1, nsegs));
		if (did_out)
			rack->r_wanted_output = 0;
	}

#ifdef TCP_ACCOUNTING
	sched_unpin();
#endif
	return (retval);
}

static void
rack_do_segment(struct tcpcb *tp, struct mbuf *m, struct tcphdr *th,
    int32_t drop_hdrlen, int32_t tlen, uint8_t iptos)
{
	struct timeval tv;

	/* First lets see if we have old packets */
	if (!STAILQ_EMPTY(&tp->t_inqueue)) {
		if (ctf_do_queued_segments(tp, 1)) {
			m_freem(m);
			return;
		}
	}
	if (m->m_flags & M_TSTMP_LRO) {
		mbuf_tstmp2timeval(m, &tv);
	} else {
		/* Should not be should we kassert instead? */
		tcp_get_usecs(&tv);
	}
	if (rack_do_segment_nounlock(tp, m, th, drop_hdrlen, tlen, iptos, 0,
	    &tv) == 0) {
		INP_WUNLOCK(tptoinpcb(tp));
	}
}

struct rack_sendmap *
tcp_rack_output(struct tcpcb *tp, struct tcp_rack *rack, uint32_t tsused)
{
	struct rack_sendmap *rsm = NULL;
	int32_t idx;
	uint32_t srtt = 0, thresh = 0, ts_low = 0;
	int no_sack = 0;

	/* Return the next guy to be re-transmitted */
	if (tqhash_empty(rack->r_ctl.tqh)) {
		return (NULL);
	}
	if (tp->t_flags & TF_SENTFIN) {
		/* retran the end FIN? */
		return (NULL);
	}
	/* ok lets look at this one */
	rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
	if (rack->r_must_retran && rsm && (rsm->r_flags & RACK_MUST_RXT)) {
		return (rsm);
	}
	if (rsm && ((rsm->r_flags & RACK_ACKED) == 0)) {
		goto check_it;
	}
	rsm = rack_find_lowest_rsm(rack);
	if (rsm == NULL) {
		return (NULL);
	}
check_it:
	if (((rack->rc_tp->t_flags & TF_SACK_PERMIT) == 0) ||
	    (rack->sack_attack_disable > 0)) {
		no_sack = 1;
	}
	if ((no_sack > 0) &&
	    (rsm->r_dupack >= DUP_ACK_THRESHOLD)) {
		/*
		 * No sack so we automatically do the 3 strikes and
		 * retransmit (no rack timer would be started).
		 */
		return (rsm);
	}
	if (rsm->r_flags & RACK_ACKED) {
		return (NULL);
	}
	if (((rsm->r_flags & RACK_SACK_PASSED) == 0) &&
	    (rsm->r_dupack < DUP_ACK_THRESHOLD)) {
		/* Its not yet ready */
		return (NULL);
	}
	srtt = rack_grab_rtt(tp, rack);
	idx = rsm->r_rtr_cnt - 1;
	ts_low = (uint32_t)rsm->r_tim_lastsent[idx];
	thresh = rack_calc_thresh_rack(rack, srtt, tsused, __LINE__, 1);
	if ((tsused == ts_low) ||
	    (TSTMP_LT(tsused, ts_low))) {
		/* No time since sending */
		return (NULL);
	}
	if ((tsused - ts_low) < thresh) {
		/* It has not been long enough yet */
		return (NULL);
	}
	if ((rsm->r_dupack >= DUP_ACK_THRESHOLD) ||
	    ((rsm->r_flags & RACK_SACK_PASSED) &&
	     (rack->sack_attack_disable == 0))) {
		/*
		 * We have passed the dup-ack threshold <or>
		 * a SACK has indicated this is missing.
		 * Note that if you are a declared attacker
		 * it is only the dup-ack threshold that
		 * will cause retransmits.
		 */
		/* log retransmit reason */
		rack_log_retran_reason(rack, rsm, (tsused - ts_low), thresh, 1);
		rack->r_fast_output = 0;
		return (rsm);
	}
	return (NULL);
}

static void
rack_log_pacing_delay_calc (struct tcp_rack *rack, uint32_t len, uint32_t slot,
			   uint64_t bw_est, uint64_t bw, uint64_t len_time, int method,
			   int line, struct rack_sendmap *rsm, uint8_t quality)
{
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		if (rack_verbose_logging == 0) {
			/*
			 * We are not verbose screen out all but
			 * ones we always want.
			 */
			if ((method != 2) &&
			    (method != 3) &&
			    (method != 7) &&
			    (method != 89) &&
			    (method != 14) &&
			    (method != 20)) {
				return;
			}
		}
		memset(&log, 0, sizeof(log));
		log.u_bbr.flex1 = slot;
		log.u_bbr.flex2 = len;
		log.u_bbr.flex3 = rack->r_ctl.rc_pace_min_segs;
		log.u_bbr.flex4 = rack->r_ctl.rc_pace_max_segs;
		log.u_bbr.flex5 = rack->r_ctl.rack_per_of_gp_ss;
		log.u_bbr.flex6 = rack->r_ctl.rack_per_of_gp_ca;
		log.u_bbr.use_lt_bw = rack->rc_ack_can_sendout_data;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->r_late;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->r_early;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->app_limited_needs_set;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->rc_gp_filled;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->measure_saw_probe_rtt;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->in_probe_rtt;
		log.u_bbr.use_lt_bw <<= 1;
		log.u_bbr.use_lt_bw |= rack->gp_ready;
		log.u_bbr.pkt_epoch = line;
		log.u_bbr.epoch = rack->r_ctl.rc_agg_delayed;
		log.u_bbr.lt_epoch = rack->r_ctl.rc_agg_early;
		log.u_bbr.applimited = rack->r_ctl.rack_per_of_gp_rec;
		log.u_bbr.bw_inuse = bw_est;
		log.u_bbr.delRate = bw;
		if (rack->r_ctl.gp_bw == 0)
			log.u_bbr.cur_del_rate = 0;
		else
			log.u_bbr.cur_del_rate = rack_get_bw(rack);
		log.u_bbr.rttProp = len_time;
		log.u_bbr.pkts_out = rack->r_ctl.rc_rack_min_rtt;
		log.u_bbr.lost = rack->r_ctl.rc_probertt_sndmax_atexit;
		log.u_bbr.pacing_gain = rack_get_output_gain(rack, rsm);
		if (rack->r_ctl.cwnd_to_use < rack->rc_tp->snd_ssthresh) {
			/* We are in slow start */
			log.u_bbr.flex7 = 1;
		} else {
			/* we are on congestion avoidance */
			log.u_bbr.flex7 = 0;
		}
		log.u_bbr.flex8 = method;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.cwnd_gain = rack->rc_gp_saw_rec;
		log.u_bbr.cwnd_gain <<= 1;
		log.u_bbr.cwnd_gain |= rack->rc_gp_saw_ss;
		log.u_bbr.cwnd_gain <<= 1;
		log.u_bbr.cwnd_gain |= rack->rc_gp_saw_ca;
		log.u_bbr.bbr_substate = quality;
		log.u_bbr.bbr_state = rack->dgp_on;
		log.u_bbr.bbr_state <<= 1;
		log.u_bbr.bbr_state |= rack->rc_pace_to_cwnd;
		log.u_bbr.bbr_state <<= 2;
		TCP_LOG_EVENTP(rack->rc_tp, NULL,
		    &rack->rc_inp->inp_socket->so_rcv,
		    &rack->rc_inp->inp_socket->so_snd,
		    BBR_LOG_HPTSI_CALC, 0,
		    0, &log, false, &tv);
	}
}

static uint32_t
rack_get_pacing_len(struct tcp_rack *rack, uint64_t bw, uint32_t mss)
{
	uint32_t new_tso, user_max, pace_one;

	user_max = rack->rc_user_set_max_segs * mss;
	if (rack->rc_force_max_seg) {
		return (user_max);
	}
	if (rack->use_fixed_rate &&
	    ((rack->r_ctl.crte == NULL) ||
	     (bw != rack->r_ctl.crte->rate))) {
		/* Use the user mss since we are not exactly matched */
		return (user_max);
	}
	if (rack_pace_one_seg ||
	    (rack->r_ctl.rc_user_set_min_segs == 1))
		pace_one = 1;
	else
		pace_one = 0;

	new_tso = tcp_get_pacing_burst_size_w_divisor(rack->rc_tp, bw, mss,
		     pace_one, rack->r_ctl.crte, NULL, rack->r_ctl.pace_len_divisor);
	if (new_tso > user_max)
		new_tso = user_max;
	if (rack->rc_hybrid_mode && rack->r_ctl.client_suggested_maxseg) {
		if (((uint32_t)rack->r_ctl.client_suggested_maxseg * mss) > new_tso)
			new_tso = (uint32_t)rack->r_ctl.client_suggested_maxseg * mss;
	}
	if (rack->r_ctl.rc_user_set_min_segs &&
	    ((rack->r_ctl.rc_user_set_min_segs * mss) > new_tso))
	    new_tso = rack->r_ctl.rc_user_set_min_segs * mss;
	return (new_tso);
}

static uint64_t
rack_arrive_at_discounted_rate(struct tcp_rack *rack, uint64_t window_input, uint32_t *rate_set, uint32_t *gain_b)
{
	uint64_t reduced_win;
	uint32_t gain;

	if (window_input < rc_init_window(rack)) {
		/*
		 * The cwnd is collapsed to
		 * nearly zero, maybe because of a time-out?
		 * Lets drop back to the lt-bw.
		 */
		reduced_win = rack_get_lt_bw(rack);
		/* Set the flag so the caller knows its a rate and not a reduced window */
		*rate_set = 1;
		gain = 100;
	} else if  (IN_RECOVERY(rack->rc_tp->t_flags)) {
		/*
		 * If we are in recover our cwnd needs to be less for
		 * our pacing consideration.
		 */
		if (rack->rack_hibeta == 0) {
			reduced_win = window_input / 2;
			gain = 50;
		} else {
			reduced_win = window_input * rack->r_ctl.saved_hibeta;
			reduced_win /= 100;
			gain = rack->r_ctl.saved_hibeta;
		}
	} else {
		/*
		 * Apply Timely factor to increase/decrease the
		 * amount we are pacing at.
		 */
		gain = rack_get_output_gain(rack, NULL);
		if (gain > rack_gain_p5_ub) {
			gain = rack_gain_p5_ub;
		}
		reduced_win = window_input * gain;
		reduced_win /= 100;
	}
	if (gain_b != NULL)
		*gain_b = gain;
	/*
	 * What is being returned here is a trimmed down
	 * window values in all cases where rate_set is left
	 * at 0. In one case we actually return the rate (lt_bw).
	 * the "reduced_win" is returned as a slimmed down cwnd that
	 * is then calculated by the caller into a rate when rate_set
	 * is 0.
	 */
	return (reduced_win);
}

static int32_t
pace_to_fill_cwnd(struct tcp_rack *rack, int32_t slot, uint32_t len, uint32_t segsiz, int *capped, uint64_t *rate_wanted, uint8_t non_paced)
{
	uint64_t lentim, fill_bw;

	rack->r_via_fill_cw = 0;
	if (ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked) > rack->r_ctl.cwnd_to_use)
		return (slot);
	if ((ctf_outstanding(rack->rc_tp) + (segsiz-1)) > rack->rc_tp->snd_wnd)
		return (slot);
	if (rack->r_ctl.rc_last_us_rtt == 0)
		return (slot);
	if (rack->rc_pace_fill_if_rttin_range &&
	    (rack->r_ctl.rc_last_us_rtt >=
	     (get_filter_value_small(&rack->r_ctl.rc_gp_min_rtt) * rack->rtt_limit_mul))) {
		/* The rtt is huge, N * smallest, lets not fill */
		return (slot);
	}
	if (rack->r_ctl.fillcw_cap && *rate_wanted >= rack->r_ctl.fillcw_cap)
		return (slot);
	/*
	 * first lets calculate the b/w based on the last us-rtt
	 * and the the smallest send window.
	 */
	fill_bw = min(rack->rc_tp->snd_cwnd, rack->r_ctl.cwnd_to_use);
	if (rack->rc_fillcw_apply_discount) {
		uint32_t rate_set = 0;

		fill_bw = rack_arrive_at_discounted_rate(rack, fill_bw, &rate_set, NULL);
		if (rate_set) {
			goto at_lt_bw;
		}
	}
	/* Take the rwnd if its smaller */
	if (fill_bw > rack->rc_tp->snd_wnd)
		fill_bw = rack->rc_tp->snd_wnd;
	/* Now lets make it into a b/w */
	fill_bw *= (uint64_t)HPTS_USEC_IN_SEC;
	fill_bw /= (uint64_t)rack->r_ctl.rc_last_us_rtt;
	/* Adjust to any cap */
	if (rack->r_ctl.fillcw_cap && fill_bw >= rack->r_ctl.fillcw_cap)
		fill_bw = rack->r_ctl.fillcw_cap;

at_lt_bw:
	if (rack_bw_multipler > 0) {
		/*
		 * We want to limit fill-cw to the some multiplier
		 * of the max(lt_bw, gp_est). The normal default
		 * is 0 for off, so a sysctl has enabled it.
		 */
		uint64_t lt_bw, gp, rate;

		gp = rack_get_gp_est(rack);
		lt_bw = rack_get_lt_bw(rack);
		if (lt_bw > gp)
			rate = lt_bw;
		else
			rate = gp;
		rate *= rack_bw_multipler;
		rate /= 100;
		if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
			union tcp_log_stackspecific log;
			struct timeval tv;

			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.timeStamp = tcp_get_usecs(&tv);
			log.u_bbr.flex1 = rack_bw_multipler;
			log.u_bbr.flex2 = len;
			log.u_bbr.cur_del_rate = gp;
			log.u_bbr.delRate = lt_bw;
			log.u_bbr.bw_inuse = rate;
			log.u_bbr.rttProp = fill_bw;
			log.u_bbr.flex8 = 44;
			tcp_log_event(rack->rc_tp, NULL, NULL, NULL,
				      BBR_LOG_CWND, 0,
				      0, &log, false, NULL,
				      __func__, __LINE__, &tv);
		}
		if (fill_bw > rate)
			fill_bw = rate;
	}
	/* We are below the min b/w */
	if (non_paced)
		*rate_wanted = fill_bw;
	if ((fill_bw < RACK_MIN_BW) || (fill_bw < *rate_wanted))
		return (slot);
	rack->r_via_fill_cw = 1;
	if (rack->r_rack_hw_rate_caps &&
	    (rack->r_ctl.crte != NULL)) {
		uint64_t high_rate;

		high_rate = tcp_hw_highest_rate(rack->r_ctl.crte);
		if (fill_bw > high_rate) {
			/* We are capping bw at the highest rate table entry */
			if (*rate_wanted > high_rate) {
				/* The original rate was also capped */
				rack->r_via_fill_cw = 0;
			}
			rack_log_hdwr_pacing(rack,
					     fill_bw, high_rate, __LINE__,
					     0, 3);
			fill_bw = high_rate;
			if (capped)
				*capped = 1;
		}
	} else if ((rack->r_ctl.crte == NULL) &&
		   (rack->rack_hdrw_pacing == 0) &&
		   (rack->rack_hdw_pace_ena) &&
		   rack->r_rack_hw_rate_caps &&
		   (rack->rack_attempt_hdwr_pace == 0) &&
		   (rack->rc_inp->inp_route.ro_nh != NULL) &&
		   (rack->rc_inp->inp_route.ro_nh->nh_ifp != NULL)) {
		/*
		 * Ok we may have a first attempt that is greater than our top rate
		 * lets check.
		 */
		uint64_t high_rate;

		high_rate = tcp_hw_highest_rate_ifp(rack->rc_inp->inp_route.ro_nh->nh_ifp, rack->rc_inp);
		if (high_rate) {
			if (fill_bw > high_rate) {
				fill_bw = high_rate;
				if (capped)
					*capped = 1;
			}
		}
	}
	if (rack->r_ctl.bw_rate_cap && (fill_bw > rack->r_ctl.bw_rate_cap)) {
		rack_log_hybrid_bw(rack, rack->rc_tp->snd_max,
				   fill_bw, 0, 0, HYBRID_LOG_RATE_CAP, 2, NULL, __LINE__);
		fill_bw = rack->r_ctl.bw_rate_cap;
	}
	/*
	 * Ok fill_bw holds our mythical b/w to fill the cwnd
	 * in an rtt (unless it was capped), what does that
	 * time wise equate too?
	 */
	lentim = (uint64_t)(len) * (uint64_t)HPTS_USEC_IN_SEC;
	lentim /= fill_bw;
	*rate_wanted = fill_bw;
	if (non_paced || (lentim < slot)) {
		rack_log_pacing_delay_calc(rack, len, slot, fill_bw,
					   0, lentim, 12, __LINE__, NULL, 0);
		return ((int32_t)lentim);
	} else
		return (slot);
}

static uint32_t
rack_policer_check_send(struct tcp_rack *rack, uint32_t len, uint32_t segsiz, uint32_t *needs)
{
	uint64_t calc;

	rack->rc_policer_should_pace = 0;
	calc = rack_policer_bucket_reserve * rack->r_ctl.policer_bucket_size;
	calc /= 100;
	/*
	 * Now lets look at if we want more than is in the bucket <or>
	 * we want more than is reserved in the bucket.
	 */
	if (rack_verbose_logging > 0)
		policer_detection_log(rack, len, segsiz, calc, rack->r_ctl.current_policer_bucket, 8);
	if ((calc > rack->r_ctl.current_policer_bucket) ||
	    (len >= (rack->r_ctl.current_policer_bucket - calc))) {
		/*
		 * We may want to pace depending on if we are going
		 * into the reserve or not.
		 */
		uint32_t newlen;

		if (calc > rack->r_ctl.current_policer_bucket) {
			/*
			 * This will eat into the reserve if we
			 * don't have room at all some lines
			 * below will catch it.
			 */
			newlen = rack->r_ctl.policer_max_seg;
			rack->rc_policer_should_pace = 1;
		} else {
			/*
			 * We have all of the reserve plus something in the bucket
			 * that we can give out.
			 */
			newlen = rack->r_ctl.current_policer_bucket - calc;
			if (newlen < rack->r_ctl.policer_max_seg) {
				/*
				 * Into the reserve to get a full policer_max_seg
				 * so we set the len to that and eat into
				 * the reserve. If we go over the code
				 * below will make us wait.
				 */
				newlen = rack->r_ctl.policer_max_seg;
				rack->rc_policer_should_pace = 1;
			}
		}
		if (newlen > rack->r_ctl.current_policer_bucket) {
			/* We have to wait some */
			*needs = newlen - rack->r_ctl.current_policer_bucket;
			return (0);
		}
		if (rack_verbose_logging > 0)
			policer_detection_log(rack, len, segsiz, newlen, 0, 9);
		len = newlen;
	} /* else we have all len available above the reserve */
	if (rack_verbose_logging > 0)
		policer_detection_log(rack, len, segsiz, calc, 0, 10);
	return (len);
}

static uint32_t
rack_policed_sending(struct tcp_rack *rack, struct tcpcb *tp, uint32_t len, uint32_t segsiz, int call_line)
{
	/*
	 * Given a send of len, and a token bucket set at current_policer_bucket_size
	 * are we close enough to the end of the bucket that we need to pace? If so
	 * calculate out a time and return it. Otherwise subtract the tokens from
	 * the bucket.
	 */
	uint64_t calc;

	if ((rack->r_ctl.policer_bw == 0) ||
	    (rack->r_ctl.policer_bucket_size < segsiz)) {
		/*
		 * We should have an estimate here...
		 */
		return (0);
	}
	calc = (uint64_t)rack_policer_bucket_reserve * (uint64_t)rack->r_ctl.policer_bucket_size;
	calc /= 100;
	if ((rack->r_ctl.current_policer_bucket < len) ||
	    (rack->rc_policer_should_pace == 1) ||
	    ((rack->r_ctl.current_policer_bucket - len) <= (uint32_t)calc)) {
		/* we need to pace */
		uint64_t lentim, res;
		uint32_t slot;

		lentim = (uint64_t)len * (uint64_t)HPTS_USEC_IN_SEC;
		res = lentim / rack->r_ctl.policer_bw;
		slot = (uint32_t)res;
		if (rack->r_ctl.current_policer_bucket > len)
			rack->r_ctl.current_policer_bucket -= len;
		else
			rack->r_ctl.current_policer_bucket = 0;
		policer_detection_log(rack, len, slot, (uint32_t)rack_policer_bucket_reserve, call_line, 5);
		rack->rc_policer_should_pace = 0;
		return(slot);
	}
	/* Just take tokens out of the bucket and let rack do whatever it would have */
	policer_detection_log(rack, len, 0, (uint32_t)rack_policer_bucket_reserve, call_line, 6);
	if (len < rack->r_ctl.current_policer_bucket) {
		rack->r_ctl.current_policer_bucket -= len;
	} else {
		rack->r_ctl.current_policer_bucket = 0;
	}
	return (0);
}


static int32_t
rack_get_pacing_delay(struct tcp_rack *rack, struct tcpcb *tp, uint32_t len, struct rack_sendmap *rsm, uint32_t segsiz, int line)
{
	uint64_t srtt;
	int32_t slot = 0;
	int32_t minslot = 0;
	int can_start_hw_pacing = 1;
	int err;
	int pace_one;

	if (rack_pace_one_seg ||
	    (rack->r_ctl.rc_user_set_min_segs == 1))
		pace_one = 1;
	else
		pace_one = 0;
	if (rack->rc_policer_detected == 1) {
		/*
		 * A policer has been detected and we
		 * have all of our data (policer-bw and
		 * policer bucket size) calculated. Call
		 * into the function to find out if we are
		 * overriding the time.
		 */
		slot = rack_policed_sending(rack, tp, len, segsiz, line);
		if (slot) {
			uint64_t logbw;

			logbw = rack->r_ctl.current_policer_bucket;
			logbw <<= 32;
			logbw |= rack->r_ctl.policer_bucket_size;
			rack_log_pacing_delay_calc(rack, len, slot, rack->r_ctl.policer_bw, logbw, 0, 89, __LINE__, NULL, 0);
			return(slot);
		}
	}
	if (rack->rc_always_pace == 0) {
		/*
		 * We use the most optimistic possible cwnd/srtt for
		 * sending calculations. This will make our
		 * calculation anticipate getting more through
		 * quicker then possible. But thats ok we don't want
		 * the peer to have a gap in data sending.
		 */
		uint64_t cwnd, tr_perms = 0;
		int32_t reduce = 0;

	old_method:
		/*
		 * We keep no precise pacing with the old method
		 * instead we use the pacer to mitigate bursts.
		 */
		if (rack->r_ctl.rc_rack_min_rtt)
			srtt = rack->r_ctl.rc_rack_min_rtt;
		else
			srtt = max(tp->t_srtt, 1);
		if (rack->r_ctl.rc_rack_largest_cwnd)
			cwnd = rack->r_ctl.rc_rack_largest_cwnd;
		else
			cwnd = rack->r_ctl.cwnd_to_use;
		/* Inflate cwnd by 1000 so srtt of usecs is in ms */
		tr_perms = (cwnd * 1000) / srtt;
		if (tr_perms == 0) {
			tr_perms = ctf_fixed_maxseg(tp);
		}
		/*
		 * Calculate how long this will take to drain, if
		 * the calculation comes out to zero, thats ok we
		 * will use send_a_lot to possibly spin around for
		 * more increasing tot_len_this_send to the point
		 * that its going to require a pace, or we hit the
		 * cwnd. Which in that case we are just waiting for
		 * a ACK.
		 */
		slot = len / tr_perms;
		/* Now do we reduce the time so we don't run dry? */
		if (slot && rack_slot_reduction) {
			reduce = (slot / rack_slot_reduction);
			if (reduce < slot) {
				slot -= reduce;
			} else
				slot = 0;
		}
		slot *= HPTS_USEC_IN_MSEC;
		if (rack->rc_pace_to_cwnd) {
			uint64_t rate_wanted = 0;

			slot = pace_to_fill_cwnd(rack, slot, len, segsiz, NULL, &rate_wanted, 1);
			rack->rc_ack_can_sendout_data = 1;
			rack_log_pacing_delay_calc(rack, len, slot, rate_wanted, 0, 0, 14, __LINE__, NULL, 0);
		} else
			rack_log_pacing_delay_calc(rack, len, slot, tr_perms, reduce, 0, 7, __LINE__, NULL, 0);
		/*******************************************************/
		/* RRS: We insert non-paced call to stats here for len */
		/*******************************************************/
	} else {
		uint64_t bw_est, res, lentim, rate_wanted;
		uint32_t segs, oh;
		int capped = 0;
		int prev_fill;

		if ((rack->r_rr_config == 1) && rsm) {
			return (rack->r_ctl.rc_min_to);
		}
		if (rack->use_fixed_rate) {
			rate_wanted = bw_est = rack_get_fixed_pacing_bw(rack);
		} else if ((rack->r_ctl.init_rate == 0) &&
			   (rack->r_ctl.gp_bw == 0)) {
			/* no way to yet do an estimate */
			bw_est = rate_wanted = 0;
		} else if (rack->dgp_on) {
			bw_est = rack_get_bw(rack);
			rate_wanted = rack_get_output_bw(rack, bw_est, rsm, &capped);
		} else {
			uint32_t gain, rate_set = 0;

			rate_wanted = min(rack->rc_tp->snd_cwnd, rack->r_ctl.cwnd_to_use);
			rate_wanted = rack_arrive_at_discounted_rate(rack, rate_wanted, &rate_set, &gain);
			if (rate_set == 0) {
				if (rate_wanted > rack->rc_tp->snd_wnd)
					rate_wanted = rack->rc_tp->snd_wnd;
				/* Now lets make it into a b/w */
				rate_wanted *= (uint64_t)HPTS_USEC_IN_SEC;
				rate_wanted /= (uint64_t)rack->r_ctl.rc_last_us_rtt;
			}
			bw_est = rate_wanted;
			rack_log_pacing_delay_calc(rack, rack->rc_tp->snd_cwnd,
						   rack->r_ctl.cwnd_to_use,
						   rate_wanted, bw_est,
						   rack->r_ctl.rc_last_us_rtt,
						   88, __LINE__, NULL, gain);
		}
		if ((bw_est == 0) || (rate_wanted == 0) ||
		    ((rack->gp_ready == 0) && (rack->use_fixed_rate == 0))) {
			/*
			 * No way yet to make a b/w estimate or
			 * our raise is set incorrectly.
			 */
			goto old_method;
		}
		rack_rate_cap_bw(rack, &rate_wanted, &capped);
		/* We need to account for all the overheads */
		segs = (len + segsiz - 1) / segsiz;
		/*
		 * We need the diff between 1514 bytes (e-mtu with e-hdr)
		 * and how much data we put in each packet. Yes this
		 * means we may be off if we are larger than 1500 bytes
		 * or smaller. But this just makes us more conservative.
		 */

		oh =  (tp->t_maxseg - segsiz) + sizeof(struct tcphdr);
		if (rack->r_is_v6) {
#ifdef INET6
			oh += sizeof(struct ip6_hdr);
#endif
		} else {
#ifdef INET
			oh += sizeof(struct ip);
#endif
		}
		/* We add a fixed 14 for the ethernet header */
		oh += 14;
		segs *= oh;
		lentim = (uint64_t)(len + segs) * (uint64_t)HPTS_USEC_IN_SEC;
		res = lentim / rate_wanted;
		slot = (uint32_t)res;
		if (rack_hw_rate_min &&
		    (rate_wanted < rack_hw_rate_min)) {
			can_start_hw_pacing = 0;
			if (rack->r_ctl.crte) {
				/*
				 * Ok we need to release it, we
				 * have fallen too low.
				 */
				tcp_rel_pacing_rate(rack->r_ctl.crte, rack->rc_tp);
				rack->r_ctl.crte = NULL;
				rack->rack_attempt_hdwr_pace = 0;
				rack->rack_hdrw_pacing = 0;
			}
		}
		if (rack->r_ctl.crte &&
		    (tcp_hw_highest_rate(rack->r_ctl.crte) < rate_wanted)) {
			/*
			 * We want more than the hardware can give us,
			 * don't start any hw pacing.
			 */
			can_start_hw_pacing = 0;
			if (rack->r_rack_hw_rate_caps == 0) {
				/*
				 * Ok we need to release it, we
				 * want more than the card can give us and
				 * no rate cap is in place. Set it up so
				 * when we want less we can retry.
				 */
				tcp_rel_pacing_rate(rack->r_ctl.crte, rack->rc_tp);
				rack->r_ctl.crte = NULL;
				rack->rack_attempt_hdwr_pace = 0;
				rack->rack_hdrw_pacing = 0;
			}
		}
		if ((rack->r_ctl.crte != NULL) && (rack->rc_inp->inp_snd_tag == NULL)) {
			/*
			 * We lost our rate somehow, this can happen
			 * if the interface changed underneath us.
			 */
			tcp_rel_pacing_rate(rack->r_ctl.crte, rack->rc_tp);
			rack->r_ctl.crte = NULL;
			/* Lets re-allow attempting to setup pacing */
			rack->rack_hdrw_pacing = 0;
			rack->rack_attempt_hdwr_pace = 0;
			rack_log_hdwr_pacing(rack,
					     rate_wanted, bw_est, __LINE__,
					     0, 6);
		}
		prev_fill = rack->r_via_fill_cw;
		if ((rack->rc_pace_to_cwnd) &&
		    (capped == 0) &&
		    (rack->dgp_on == 1) &&
		    (rack->use_fixed_rate == 0) &&
		    (rack->in_probe_rtt == 0) &&
		    (IN_FASTRECOVERY(rack->rc_tp->t_flags) == 0)) {
			/*
			 * We want to pace at our rate *or* faster to
			 * fill the cwnd to the max if its not full.
			 */
			slot = pace_to_fill_cwnd(rack, slot, (len+segs), segsiz, &capped, &rate_wanted, 0);
			/* Re-check to make sure we are not exceeding our max b/w */
			if ((rack->r_ctl.crte != NULL) &&
			    (tcp_hw_highest_rate(rack->r_ctl.crte) < rate_wanted)) {
				/*
				 * We want more than the hardware can give us,
				 * don't start any hw pacing.
				 */
				can_start_hw_pacing = 0;
				if (rack->r_rack_hw_rate_caps == 0) {
					/*
					 * Ok we need to release it, we
					 * want more than the card can give us and
					 * no rate cap is in place. Set it up so
					 * when we want less we can retry.
					 */
					tcp_rel_pacing_rate(rack->r_ctl.crte, rack->rc_tp);
					rack->r_ctl.crte = NULL;
					rack->rack_attempt_hdwr_pace = 0;
					rack->rack_hdrw_pacing = 0;
					rack_set_pace_segments(rack->rc_tp, rack, __LINE__, NULL);
				}
			}
		}
		if ((rack->rc_inp->inp_route.ro_nh != NULL) &&
		    (rack->rc_inp->inp_route.ro_nh->nh_ifp != NULL)) {
			if ((rack->rack_hdw_pace_ena) &&
			    (can_start_hw_pacing > 0) &&
			    (rack->rack_hdrw_pacing == 0) &&
			    (rack->rack_attempt_hdwr_pace == 0)) {
				/*
				 * Lets attempt to turn on hardware pacing
				 * if we can.
				 */
				rack->rack_attempt_hdwr_pace = 1;
				rack->r_ctl.crte = tcp_set_pacing_rate(rack->rc_tp,
								       rack->rc_inp->inp_route.ro_nh->nh_ifp,
								       rate_wanted,
								       RS_PACING_GEQ,
								       &err, &rack->r_ctl.crte_prev_rate);
				if (rack->r_ctl.crte) {
					rack->rack_hdrw_pacing = 1;
					rack->r_ctl.rc_pace_max_segs = tcp_get_pacing_burst_size_w_divisor(tp, rate_wanted, segsiz,
													   pace_one, rack->r_ctl.crte,
													   NULL, rack->r_ctl.pace_len_divisor);
					rack_log_hdwr_pacing(rack,
							     rate_wanted, rack->r_ctl.crte->rate, __LINE__,
							     err, 0);
					rack->r_ctl.last_hw_bw_req = rate_wanted;
				} else {
					counter_u64_add(rack_hw_pace_init_fail, 1);
				}
			} else if (rack->rack_hdrw_pacing &&
				   (rack->r_ctl.last_hw_bw_req != rate_wanted)) {
				/* Do we need to adjust our rate? */
				const struct tcp_hwrate_limit_table *nrte;

				if (rack->r_up_only &&
				    (rate_wanted < rack->r_ctl.crte->rate)) {
					/**
					 * We have four possible states here
					 * having to do with the previous time
					 * and this time.
					 *   previous  |  this-time
					 * A)     0      |     0   -- fill_cw not in the picture
					 * B)     1      |     0   -- we were doing a fill-cw but now are not
					 * C)     1      |     1   -- all rates from fill_cw
					 * D)     0      |     1   -- we were doing non-fill and now we are filling
					 *
					 * For case A, C and D we don't allow a drop. But for
					 * case B where we now our on our steady rate we do
					 * allow a drop.
					 *
					 */
					if (!((prev_fill == 1) && (rack->r_via_fill_cw == 0)))
						goto done_w_hdwr;
				}
				if ((rate_wanted > rack->r_ctl.crte->rate) ||
				    (rate_wanted <= rack->r_ctl.crte_prev_rate)) {
					if (rack_hw_rate_to_low &&
					    (bw_est < rack_hw_rate_to_low)) {
						/*
						 * The pacing rate is too low for hardware, but
						 * do allow hardware pacing to be restarted.
						 */
						rack_log_hdwr_pacing(rack,
								     bw_est, rack->r_ctl.crte->rate, __LINE__,
								     0, 5);
						tcp_rel_pacing_rate(rack->r_ctl.crte, rack->rc_tp);
						rack->r_ctl.crte = NULL;
						rack->rack_attempt_hdwr_pace = 0;
						rack->rack_hdrw_pacing = 0;
						rack_set_pace_segments(rack->rc_tp, rack, __LINE__, &rate_wanted);
						goto done_w_hdwr;
					}
					nrte = tcp_chg_pacing_rate(rack->r_ctl.crte,
								   rack->rc_tp,
								   rack->rc_inp->inp_route.ro_nh->nh_ifp,
								   rate_wanted,
								   RS_PACING_GEQ,
								   &err, &rack->r_ctl.crte_prev_rate);
					if (nrte == NULL) {
						/*
						 * Lost the rate, lets drop hardware pacing
						 * period.
						 */
						rack->rack_hdrw_pacing = 0;
						rack->r_ctl.crte = NULL;
						rack_log_hdwr_pacing(rack,
								     rate_wanted, 0, __LINE__,
								     err, 1);
						rack_set_pace_segments(rack->rc_tp, rack, __LINE__, &rate_wanted);
						counter_u64_add(rack_hw_pace_lost, 1);
					} else if (nrte != rack->r_ctl.crte) {
						rack->r_ctl.crte = nrte;
						rack->r_ctl.rc_pace_max_segs = tcp_get_pacing_burst_size_w_divisor(tp, rate_wanted,
														   segsiz, pace_one, rack->r_ctl.crte,
														   NULL, rack->r_ctl.pace_len_divisor);
						rack_log_hdwr_pacing(rack,
								     rate_wanted, rack->r_ctl.crte->rate, __LINE__,
								     err, 2);
						rack->r_ctl.last_hw_bw_req = rate_wanted;
					}
				} else {
					/* We just need to adjust the segment size */
					rack_set_pace_segments(rack->rc_tp, rack, __LINE__, &rate_wanted);
					rack_log_hdwr_pacing(rack,
							     rate_wanted, rack->r_ctl.crte->rate, __LINE__,
							     0, 4);
					rack->r_ctl.last_hw_bw_req = rate_wanted;
				}
			}
		}
		if (minslot && (minslot > slot)) {
			rack_log_pacing_delay_calc(rack, minslot, slot, rack->r_ctl.crte->rate, bw_est, lentim,
						   98, __LINE__, NULL, 0);
			slot = minslot;
		}
	done_w_hdwr:
		if (rack_limit_time_with_srtt &&
		    (rack->use_fixed_rate == 0) &&
		    (rack->rack_hdrw_pacing == 0)) {
			/*
			 * Sanity check, we do not allow the pacing delay
			 * to be longer than the SRTT of the path. If it is
			 * a slow path, then adding a packet should increase
			 * the RTT and compensate for this i.e. the srtt will
			 * be greater so the allowed pacing time will be greater.
			 *
			 * Note this restriction is not for where a peak rate
			 * is set, we are doing fixed pacing or hardware pacing.
			 */
			if (rack->rc_tp->t_srtt)
				srtt = rack->rc_tp->t_srtt;
			else
				srtt = RACK_INITIAL_RTO * HPTS_USEC_IN_MSEC;	/* its in ms convert */
			if (srtt < (uint64_t)slot) {
				rack_log_pacing_delay_calc(rack, srtt, slot, rate_wanted, bw_est, lentim, 99, __LINE__, NULL, 0);
				slot = srtt;
			}
		}
		/*******************************************************************/
		/* RRS: We insert paced call to stats here for len and rate_wanted */
		/*******************************************************************/
		rack_log_pacing_delay_calc(rack, len, slot, rate_wanted, bw_est, lentim, 2, __LINE__, rsm, 0);
	}
	if (rack->r_ctl.crte && (rack->r_ctl.crte->rs_num_enobufs > 0)) {
		/*
		 * If this rate is seeing enobufs when it
		 * goes to send then either the nic is out
		 * of gas or we are mis-estimating the time
		 * somehow and not letting the queue empty
		 * completely. Lets add to the pacing time.
		 */
		int hw_boost_delay;

		hw_boost_delay = rack->r_ctl.crte->time_between * rack_enobuf_hw_boost_mult;
		if (hw_boost_delay > rack_enobuf_hw_max)
			hw_boost_delay = rack_enobuf_hw_max;
		else if (hw_boost_delay < rack_enobuf_hw_min)
			hw_boost_delay = rack_enobuf_hw_min;
		slot += hw_boost_delay;
	}
	return (slot);
}

static void
rack_start_gp_measurement(struct tcpcb *tp, struct tcp_rack *rack,
    tcp_seq startseq, uint32_t sb_offset)
{
	struct rack_sendmap *my_rsm = NULL;

	if (tp->t_state < TCPS_ESTABLISHED) {
		/*
		 * We don't start any measurements if we are
		 * not at least established.
		 */
		return;
	}
	if (tp->t_state >= TCPS_FIN_WAIT_1) {
		/*
		 * We will get no more data into the SB
		 * this means we need to have the data available
		 * before we start a measurement.
		 */

		if (sbavail(&tptosocket(tp)->so_snd) <
		    max(rc_init_window(rack),
			(MIN_GP_WIN * ctf_fixed_maxseg(tp)))) {
			/* Nope not enough data */
			return;
		}
	}
	tp->t_flags |= TF_GPUTINPROG;
	rack->r_ctl.rc_gp_cumack_ts = 0;
	rack->r_ctl.rc_gp_lowrtt = 0xffffffff;
	rack->r_ctl.rc_gp_high_rwnd = rack->rc_tp->snd_wnd;
	tp->gput_seq = startseq;
	rack->app_limited_needs_set = 0;
	if (rack->in_probe_rtt)
		rack->measure_saw_probe_rtt = 1;
	else if ((rack->measure_saw_probe_rtt) &&
		 (SEQ_GEQ(tp->gput_seq, rack->r_ctl.rc_probertt_sndmax_atexit)))
		rack->measure_saw_probe_rtt = 0;
	if (rack->rc_gp_filled)
		tp->gput_ts = rack->r_ctl.last_cumack_advance;
	else {
		/* Special case initial measurement */
		struct timeval tv;

		tp->gput_ts = tcp_get_usecs(&tv);
		rack->r_ctl.rc_gp_output_ts = rack_to_usec_ts(&tv);
	}
	/*
	 * We take a guess out into the future,
	 * if we have no measurement and no
	 * initial rate, we measure the first
	 * initial-windows worth of data to
	 * speed up getting some GP measurement and
	 * thus start pacing.
	 */
	if ((rack->rc_gp_filled == 0) && (rack->r_ctl.init_rate == 0)) {
		rack->app_limited_needs_set = 1;
		tp->gput_ack = startseq + max(rc_init_window(rack),
					      (MIN_GP_WIN * ctf_fixed_maxseg(tp)));
		rack_log_pacing_delay_calc(rack,
					   tp->gput_seq,
					   tp->gput_ack,
					   0,
					   tp->gput_ts,
					   (((uint64_t)rack->r_ctl.rc_app_limited_cnt << 32) | (uint64_t)rack->r_ctl.rc_gp_output_ts),
					   9,
					   __LINE__, NULL, 0);
		rack_tend_gp_marks(tp, rack);
		rack_log_gpset(rack, tp->gput_ack, 0, 0, __LINE__, 1, NULL);
		return;
	}
	if (sb_offset) {
		/*
		 * We are out somewhere in the sb
		 * can we use the already outstanding data?
		 */

		if (rack->r_ctl.rc_app_limited_cnt == 0) {
			/*
			 * Yes first one is good and in this case
			 * the tp->gput_ts is correctly set based on
			 * the last ack that arrived (no need to
			 * set things up when an ack comes in).
			 */
			my_rsm = tqhash_min(rack->r_ctl.tqh);
			if ((my_rsm == NULL) ||
			    (my_rsm->r_rtr_cnt != 1)) {
				/* retransmission? */
				goto use_latest;
			}
		} else {
			if (rack->r_ctl.rc_first_appl == NULL) {
				/*
				 * If rc_first_appl is NULL
				 * then the cnt should be 0.
				 * This is probably an error, maybe
				 * a KASSERT would be approprate.
				 */
				goto use_latest;
			}
			/*
			 * If we have a marker pointer to the last one that is
			 * app limited we can use that, but we need to set
			 * things up so that when it gets ack'ed we record
			 * the ack time (if its not already acked).
			 */
			rack->app_limited_needs_set = 1;
			/*
			 * We want to get to the rsm that is either
			 * next with space i.e. over 1 MSS or the one
			 * after that (after the app-limited).
			 */
			my_rsm = tqhash_next(rack->r_ctl.tqh, rack->r_ctl.rc_first_appl);
			if (my_rsm) {
				if ((my_rsm->r_end - my_rsm->r_start) <= ctf_fixed_maxseg(tp))
					/* Have to use the next one */
					my_rsm = tqhash_next(rack->r_ctl.tqh, my_rsm);
				else {
					/* Use after the first MSS of it is acked */
					tp->gput_seq = my_rsm->r_start + ctf_fixed_maxseg(tp);
					goto start_set;
				}
			}
			if ((my_rsm == NULL) ||
			    (my_rsm->r_rtr_cnt != 1)) {
				/*
				 * Either its a retransmit or
				 * the last is the app-limited one.
				 */
				goto use_latest;
			}
		}
		tp->gput_seq = my_rsm->r_start;
start_set:
		if (my_rsm->r_flags & RACK_ACKED) {
			/*
			 * This one has been acked use the arrival ack time
			 */
			struct rack_sendmap *nrsm;

			tp->gput_ts = (uint32_t)my_rsm->r_ack_arrival;
			rack->app_limited_needs_set = 0;
			/*
			 * Ok in this path we need to use the r_end now
			 * since this guy is the starting ack.
			 */
			tp->gput_seq = my_rsm->r_end;
			/*
			 * We also need to adjust up the sendtime
			 * to the send of the next data after my_rsm.
			 */
			nrsm = tqhash_next(rack->r_ctl.tqh, my_rsm);
			if (nrsm != NULL)
				my_rsm = nrsm;
			else {
				/*
				 * The next as not been sent, thats the
				 * case for using the latest.
				 */
				goto use_latest;
			}
		}
		rack->r_ctl.rc_gp_output_ts = my_rsm->r_tim_lastsent[0];
		tp->gput_ack = tp->gput_seq + rack_get_measure_window(tp, rack);
		rack->r_ctl.rc_gp_cumack_ts = 0;
		if ((rack->r_ctl.cleared_app_ack == 1) &&
		    (SEQ_GEQ(rack->r_ctl.cleared_app_ack, tp->gput_seq))) {
			/*
			 * We just cleared an application limited period
			 * so the next seq out needs to skip the first
			 * ack.
			 */
			rack->app_limited_needs_set = 1;
			rack->r_ctl.cleared_app_ack = 0;
		}
		rack_log_pacing_delay_calc(rack,
					   tp->gput_seq,
					   tp->gput_ack,
					   (uint64_t)my_rsm,
					   tp->gput_ts,
					   (((uint64_t)rack->r_ctl.rc_app_limited_cnt << 32) | (uint64_t)rack->r_ctl.rc_gp_output_ts),
					   9,
					   __LINE__, my_rsm, 0);
		/* Now lets make sure all are marked as they should be */
		rack_tend_gp_marks(tp, rack);
		rack_log_gpset(rack, tp->gput_ack, 0, 0, __LINE__, 1, NULL);
		return;
	}

use_latest:
	/*
	 * We don't know how long we may have been
	 * idle or if this is the first-send. Lets
	 * setup the flag so we will trim off
	 * the first ack'd data so we get a true
	 * measurement.
	 */
	rack->app_limited_needs_set = 1;
	tp->gput_ack = startseq + rack_get_measure_window(tp, rack);
	rack->r_ctl.rc_gp_cumack_ts = 0;
	/* Find this guy so we can pull the send time */
	my_rsm = tqhash_find(rack->r_ctl.tqh, startseq);
	if (my_rsm) {
		rack->r_ctl.rc_gp_output_ts = my_rsm->r_tim_lastsent[0];
		if (my_rsm->r_flags & RACK_ACKED) {
			/*
			 * Unlikely since its probably what was
			 * just transmitted (but I am paranoid).
			 */
			tp->gput_ts = (uint32_t)my_rsm->r_ack_arrival;
			rack->app_limited_needs_set = 0;
		}
		if (SEQ_LT(my_rsm->r_start, tp->gput_seq)) {
			/* This also is unlikely */
			tp->gput_seq = my_rsm->r_start;
		}
	} else {
		/*
		 * TSNH unless we have some send-map limit,
		 * and even at that it should not be hitting
		 * that limit (we should have stopped sending).
		 */
		struct timeval tv;

		microuptime(&tv);
		rack->r_ctl.rc_gp_output_ts = rack_to_usec_ts(&tv);
	}
	rack_tend_gp_marks(tp, rack);
	rack_log_pacing_delay_calc(rack,
				   tp->gput_seq,
				   tp->gput_ack,
				   (uint64_t)my_rsm,
				   tp->gput_ts,
				   (((uint64_t)rack->r_ctl.rc_app_limited_cnt << 32) | (uint64_t)rack->r_ctl.rc_gp_output_ts),
				   9, __LINE__, NULL, 0);
	rack_log_gpset(rack, tp->gput_ack, 0, 0, __LINE__, 1, NULL);
}

static inline uint32_t
rack_what_can_we_send(struct tcpcb *tp, struct tcp_rack *rack,  uint32_t cwnd_to_use,
    uint32_t avail, int32_t sb_offset)
{
	uint32_t len;
	uint32_t sendwin;

	if (tp->snd_wnd > cwnd_to_use)
		sendwin = cwnd_to_use;
	else
		sendwin = tp->snd_wnd;
	if (ctf_outstanding(tp) >= tp->snd_wnd) {
		/* We never want to go over our peers rcv-window */
		len = 0;
	} else {
		uint32_t flight;

		flight = ctf_flight_size(tp, rack->r_ctl.rc_sacked);
		if (flight >= sendwin) {
			/*
			 * We have in flight what we are allowed by cwnd (if
			 * it was rwnd blocking it would have hit above out
			 * >= tp->snd_wnd).
			 */
			return (0);
		}
		len = sendwin - flight;
		if ((len + ctf_outstanding(tp)) > tp->snd_wnd) {
			/* We would send too much (beyond the rwnd) */
			len = tp->snd_wnd - ctf_outstanding(tp);
		}
		if ((len + sb_offset) > avail) {
			/*
			 * We don't have that much in the SB, how much is
			 * there?
			 */
			len = avail - sb_offset;
		}
	}
	return (len);
}

static void
rack_log_fsb(struct tcp_rack *rack, struct tcpcb *tp, struct socket *so, uint32_t flags,
	     unsigned ipoptlen, int32_t orig_len, int32_t len, int error,
	     int rsm_is_null, int optlen, int line, uint16_t mode)
{
	if (rack_verbose_logging && tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex1 = error;
		log.u_bbr.flex2 = flags;
		log.u_bbr.flex3 = rsm_is_null;
		log.u_bbr.flex4 = ipoptlen;
		log.u_bbr.flex5 = tp->rcv_numsacks;
		log.u_bbr.flex6 = rack->r_ctl.rc_agg_early;
		log.u_bbr.flex7 = optlen;
		log.u_bbr.flex8 = rack->r_fsb_inited;
		log.u_bbr.applimited = rack->r_fast_output;
		log.u_bbr.bw_inuse = rack_get_bw(rack);
		log.u_bbr.pacing_gain = rack_get_output_gain(rack, NULL);
		log.u_bbr.cwnd_gain = mode;
		log.u_bbr.pkts_out = orig_len;
		log.u_bbr.lt_epoch = len;
		log.u_bbr.delivered = line;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		tcp_log_event(tp, NULL, &so->so_rcv, &so->so_snd, TCP_LOG_FSB, 0,
			       len, &log, false, NULL, __func__, __LINE__, &tv);
	}
}


static struct mbuf *
rack_fo_base_copym(struct mbuf *the_m, uint32_t the_off, int32_t *plen,
		   struct rack_fast_send_blk *fsb,
		   int32_t seglimit, int32_t segsize, int hw_tls)
{
#ifdef KERN_TLS
	struct ktls_session *tls, *ntls;
#ifdef INVARIANTS
	struct mbuf *start;
#endif
#endif
	struct mbuf *m, *n, **np, *smb;
	struct mbuf *top;
	int32_t off, soff;
	int32_t len = *plen;
	int32_t fragsize;
	int32_t len_cp = 0;
	uint32_t mlen, frags;

	soff = off = the_off;
	smb = m = the_m;
	np = &top;
	top = NULL;
#ifdef KERN_TLS
	if (hw_tls && (m->m_flags & M_EXTPG))
		tls = m->m_epg_tls;
	else
		tls = NULL;
#ifdef INVARIANTS
	start = m;
#endif
#endif
	while (len > 0) {
		if (m == NULL) {
			*plen = len_cp;
			break;
		}
#ifdef KERN_TLS
		if (hw_tls) {
			if (m->m_flags & M_EXTPG)
				ntls = m->m_epg_tls;
			else
				ntls = NULL;

			/*
			 * Avoid mixing TLS records with handshake
			 * data or TLS records from different
			 * sessions.
			 */
			if (tls != ntls) {
				MPASS(m != start);
				*plen = len_cp;
				break;
			}
		}
#endif
		mlen = min(len, m->m_len - off);
		if (seglimit) {
			/*
			 * For M_EXTPG mbufs, add 3 segments
			 * + 1 in case we are crossing page boundaries
			 * + 2 in case the TLS hdr/trailer are used
			 * It is cheaper to just add the segments
			 * than it is to take the cache miss to look
			 * at the mbuf ext_pgs state in detail.
			 */
			if (m->m_flags & M_EXTPG) {
				fragsize = min(segsize, PAGE_SIZE);
				frags = 3;
			} else {
				fragsize = segsize;
				frags = 0;
			}

			/* Break if we really can't fit anymore. */
			if ((frags + 1) >= seglimit) {
				*plen =	len_cp;
				break;
			}

			/*
			 * Reduce size if you can't copy the whole
			 * mbuf. If we can't copy the whole mbuf, also
			 * adjust len so the loop will end after this
			 * mbuf.
			 */
			if ((frags + howmany(mlen, fragsize)) >= seglimit) {
				mlen = (seglimit - frags - 1) * fragsize;
				len = mlen;
				*plen = len_cp + len;
			}
			frags += howmany(mlen, fragsize);
			if (frags == 0)
				frags++;
			seglimit -= frags;
			KASSERT(seglimit > 0,
			    ("%s: seglimit went too low", __func__));
		}
		n = m_get(M_NOWAIT, m->m_type);
		*np = n;
		if (n == NULL)
			goto nospace;
		n->m_len = mlen;
		soff += mlen;
		len_cp += n->m_len;
		if (m->m_flags & (M_EXT | M_EXTPG)) {
			n->m_data = m->m_data + off;
			mb_dupcl(n, m);
		} else {
			bcopy(mtod(m, caddr_t)+off, mtod(n, caddr_t),
			    (u_int)n->m_len);
		}
		len -= n->m_len;
		off = 0;
		m = m->m_next;
		np = &n->m_next;
		if (len || (soff == smb->m_len)) {
			/*
			 * We have more so we move forward  or
			 * we have consumed the entire mbuf and
			 * len has fell to 0.
			 */
			soff = 0;
			smb = m;
		}

	}
	if (fsb != NULL) {
		fsb->m = smb;
		fsb->off = soff;
		if (smb) {
			/*
			 * Save off the size of the mbuf. We do
			 * this so that we can recognize when it
			 * has been trimmed by sbcut() as acks
			 * come in.
			 */
			fsb->o_m_len = smb->m_len;
			fsb->o_t_len = M_TRAILINGROOM(smb);
		} else {
			/*
			 * This is the case where the next mbuf went to NULL. This
			 * means with this copy we have sent everything in the sb.
			 * In theory we could clear the fast_output flag, but lets
			 * not since its possible that we could get more added
			 * and acks that call the extend function which would let
			 * us send more.
			 */
			fsb->o_m_len = 0;
			fsb->o_t_len = 0;
		}
	}
	return (top);
nospace:
	if (top)
		m_freem(top);
	return (NULL);

}

/*
 * This is a copy of m_copym(), taking the TSO segment size/limit
 * constraints into account, and advancing the sndptr as it goes.
 */
static struct mbuf *
rack_fo_m_copym(struct tcp_rack *rack, int32_t *plen,
		int32_t seglimit, int32_t segsize, struct mbuf **s_mb, int *s_soff)
{
	struct mbuf *m, *n;
	int32_t soff;

	m = rack->r_ctl.fsb.m;
	if (M_TRAILINGROOM(m) != rack->r_ctl.fsb.o_t_len) {
		/*
		 * The trailing space changed, mbufs can grow
		 * at the tail but they can't shrink from
		 * it, KASSERT that. Adjust the orig_m_len to
		 * compensate for this change.
		 */
		KASSERT((rack->r_ctl.fsb.o_t_len > M_TRAILINGROOM(m)),
			("mbuf:%p rack:%p trailing_space:%jd ots:%u oml:%u mlen:%u\n",
			 m,
			 rack,
			 (intmax_t)M_TRAILINGROOM(m),
			 rack->r_ctl.fsb.o_t_len,
			 rack->r_ctl.fsb.o_m_len,
			 m->m_len));
		rack->r_ctl.fsb.o_m_len += (rack->r_ctl.fsb.o_t_len - M_TRAILINGROOM(m));
		rack->r_ctl.fsb.o_t_len = M_TRAILINGROOM(m);
	}
	if (m->m_len < rack->r_ctl.fsb.o_m_len) {
		/*
		 * Mbuf shrank, trimmed off the top by an ack, our
		 * offset changes.
		 */
		KASSERT((rack->r_ctl.fsb.off >= (rack->r_ctl.fsb.o_m_len - m->m_len)),
			("mbuf:%p len:%u rack:%p oml:%u soff:%u\n",
			 m, m->m_len,
			 rack, rack->r_ctl.fsb.o_m_len,
			 rack->r_ctl.fsb.off));

		if (rack->r_ctl.fsb.off >= (rack->r_ctl.fsb.o_m_len- m->m_len))
			rack->r_ctl.fsb.off -= (rack->r_ctl.fsb.o_m_len - m->m_len);
		else
			rack->r_ctl.fsb.off = 0;
		rack->r_ctl.fsb.o_m_len = m->m_len;
#ifdef INVARIANTS
	} else if (m->m_len > rack->r_ctl.fsb.o_m_len) {
		panic("rack:%p m:%p m_len grew outside of t_space compensation",
		      rack, m);
#endif
	}
	soff = rack->r_ctl.fsb.off;
	KASSERT(soff >= 0, ("%s, negative off %d", __FUNCTION__, soff));
	KASSERT(*plen >= 0, ("%s, negative len %d", __FUNCTION__, *plen));
	KASSERT(soff < m->m_len, ("%s rack:%p len:%u m:%p m->m_len:%u < off?",
				 __FUNCTION__,
				 rack, *plen, m, m->m_len));
	/* Save off the right location before we copy and advance */
	*s_soff = soff;
	*s_mb = rack->r_ctl.fsb.m;
	n = rack_fo_base_copym(m, soff, plen,
			       &rack->r_ctl.fsb,
			       seglimit, segsize, rack->r_ctl.fsb.hw_tls);
	return (n);
}

/* Log the buffer level */
static void
rack_log_queue_level(struct tcpcb *tp, struct tcp_rack *rack,
		     int len, struct timeval *tv,
		     uint32_t cts)
{
	uint32_t p_rate = 0, p_queue = 0, err = 0;
	union tcp_log_stackspecific log;

#ifdef RATELIMIT
	err = in_pcbquery_txrlevel(rack->rc_inp, &p_queue);
	err = in_pcbquery_txrtlmt(rack->rc_inp,	&p_rate);
#endif
	memset(&log.u_bbr, 0, sizeof(log.u_bbr));
	log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
	log.u_bbr.flex1 = p_rate;
	log.u_bbr.flex2 = p_queue;
	log.u_bbr.flex4 = (uint32_t)rack->r_ctl.crte->using;
	log.u_bbr.flex5 = (uint32_t)rack->r_ctl.crte->rs_num_enobufs;
	log.u_bbr.flex6 = rack->r_ctl.crte->time_between;
	log.u_bbr.flex7 = 99;
	log.u_bbr.flex8 = 0;
	log.u_bbr.pkts_out = err;
	log.u_bbr.delRate = rack->r_ctl.crte->rate;
	log.u_bbr.timeStamp = cts;
	log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
	tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_HDWR_PACE, 0,
		       len, &log, false, NULL, __func__, __LINE__, tv);

}

static uint32_t
rack_check_queue_level(struct tcp_rack *rack, struct tcpcb *tp,
		       struct timeval *tv, uint32_t cts, int len, uint32_t segsiz)
{
	uint64_t lentime = 0;
#ifdef RATELIMIT
	uint32_t p_rate = 0, p_queue = 0, err;
	union tcp_log_stackspecific log;
	uint64_t bw;

	err = in_pcbquery_txrlevel(rack->rc_inp, &p_queue);
	/* Failed or queue is zero */
	if (err || (p_queue == 0)) {
		lentime = 0;
		goto out;
	}
	err = in_pcbquery_txrtlmt(rack->rc_inp, &p_rate);
	if (err) {
		lentime = 0;
		goto out;
	}
	/*
	 * If we reach here we have some bytes in
	 * the queue. The number returned is a value
	 * between 0 and 0xffff where ffff is full
	 * and 0 is empty. So how best to make this into
	 * something usable?
	 *
	 * The "safer" way is lets take the b/w gotten
	 * from the query (which should be our b/w rate)
	 * and pretend that a full send (our rc_pace_max_segs)
	 * is outstanding. We factor it so its as if a full
	 * number of our MSS segment is terms of full
	 * ethernet segments are outstanding.
	 */
	bw = p_rate / 8;
	if (bw) {
		lentime = (rack->r_ctl.rc_pace_max_segs / segsiz);
		lentime *= ETHERNET_SEGMENT_SIZE;
		lentime *= (uint64_t)HPTS_USEC_IN_SEC;
		lentime /= bw;
	} else {
		/* TSNH -- KASSERT? */
		lentime = 0;
	}
out:
	if (tcp_bblogging_on(tp)) {
		memset(&log, 0, sizeof(log));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		log.u_bbr.flex1 = p_rate;
		log.u_bbr.flex2 = p_queue;
		log.u_bbr.flex4 = (uint32_t)rack->r_ctl.crte->using;
		log.u_bbr.flex5 = (uint32_t)rack->r_ctl.crte->rs_num_enobufs;
		log.u_bbr.flex6 = rack->r_ctl.crte->time_between;
		log.u_bbr.flex7 = 99;
		log.u_bbr.flex8 = 0;
		log.u_bbr.pkts_out = err;
		log.u_bbr.delRate = rack->r_ctl.crte->rate;
		log.u_bbr.cur_del_rate = lentime;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		tcp_log_event(tp, NULL, NULL, NULL, BBR_LOG_HDWR_PACE, 0,
			       len, &log, false, NULL, __func__, __LINE__,tv);
	}
#endif
	return ((uint32_t)lentime);
}

static int
rack_fast_rsm_output(struct tcpcb *tp, struct tcp_rack *rack, struct rack_sendmap *rsm,
		     uint64_t ts_val, uint32_t cts, uint32_t ms_cts, struct timeval *tv, int len, uint8_t doing_tlp)
{
	/*
	 * Enter the fast retransmit path. We are given that a sched_pin is
	 * in place (if accounting is compliled in) and the cycle count taken
	 * at the entry is in the ts_val. The concept her is that the rsm
	 * now holds the mbuf offsets and such so we can directly transmit
	 * without a lot of overhead, the len field is already set for
	 * us to prohibit us from sending too much (usually its 1MSS).
	 */
	struct ip *ip = NULL;
	struct udphdr *udp = NULL;
	struct tcphdr *th = NULL;
	struct mbuf *m = NULL;
	struct inpcb *inp;
	uint8_t *cpto;
	struct tcp_log_buffer *lgb;
#ifdef TCP_ACCOUNTING
	uint64_t crtsc;
	int cnt_thru = 1;
#endif
	struct tcpopt to;
	u_char opt[TCP_MAXOLEN];
	uint32_t hdrlen, optlen;
	int32_t slot, segsiz, max_val, tso = 0, error = 0, ulen = 0;
	uint16_t flags;
	uint32_t if_hw_tsomaxsegcount = 0, startseq;
	uint32_t if_hw_tsomaxsegsize;
	int32_t ip_sendflag = IP_NO_SND_TAG_RL;

#ifdef INET6
	struct ip6_hdr *ip6 = NULL;

	if (rack->r_is_v6) {
		ip6 = (struct ip6_hdr *)rack->r_ctl.fsb.tcp_ip_hdr;
		hdrlen = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	} else
#endif				/* INET6 */
	{
		ip = (struct ip *)rack->r_ctl.fsb.tcp_ip_hdr;
		hdrlen = sizeof(struct tcpiphdr);
	}
	if (tp->t_port && (V_tcp_udp_tunneling_port == 0)) {
		goto failed;
	}
	if (doing_tlp) {
		/* Its a TLP add the flag, it may already be there but be sure */
		rsm->r_flags |= RACK_TLP;
	} else {
		/* If it was a TLP it is not not on this retransmit */
		rsm->r_flags &= ~RACK_TLP;
	}
	startseq = rsm->r_start;
	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	inp = rack->rc_inp;
	to.to_flags = 0;
	flags = tcp_outflags[tp->t_state];
	if (flags & (TH_SYN|TH_RST)) {
		goto failed;
	}
	if (rsm->r_flags & RACK_HAS_FIN) {
		/* We can't send a FIN here */
		goto failed;
	}
	if (flags & TH_FIN) {
		/* We never send a FIN */
		flags &= ~TH_FIN;
	}
	if (tp->t_flags & TF_RCVD_TSTMP) {
		to.to_tsval = ms_cts + tp->ts_offset;
		to.to_tsecr = tp->ts_recent;
		to.to_flags = TOF_TS;
	}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	/* TCP-MD5 (RFC2385). */
	if (tp->t_flags & TF_SIGNATURE)
		to.to_flags |= TOF_SIGNATURE;
#endif
	optlen = tcp_addoptions(&to, opt);
	hdrlen += optlen;
	udp = rack->r_ctl.fsb.udp;
	if (udp)
		hdrlen += sizeof(struct udphdr);
	if (rack->r_ctl.rc_pace_max_segs)
		max_val = rack->r_ctl.rc_pace_max_segs;
	else if (rack->rc_user_set_max_segs)
		max_val = rack->rc_user_set_max_segs * segsiz;
	else
		max_val = len;
	if ((tp->t_flags & TF_TSO) &&
	    V_tcp_do_tso &&
	    (len > segsiz) &&
	    (tp->t_port == 0))
		tso = 1;
#ifdef INET6
	if (MHLEN < hdrlen + max_linkhdr)
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	else
#endif
		m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		goto failed;
	m->m_data += max_linkhdr;
	m->m_len = hdrlen;
	th = rack->r_ctl.fsb.th;
	/* Establish the len to send */
	if (len > max_val)
		len = max_val;
	if ((tso) && (len + optlen > segsiz)) {
		uint32_t if_hw_tsomax;
		int32_t max_len;

		/* extract TSO information */
		if_hw_tsomax = tp->t_tsomax;
		if_hw_tsomaxsegcount = tp->t_tsomaxsegcount;
		if_hw_tsomaxsegsize = tp->t_tsomaxsegsize;
		/*
		 * Check if we should limit by maximum payload
		 * length:
		 */
		if (if_hw_tsomax != 0) {
			/* compute maximum TSO length */
			max_len = (if_hw_tsomax - hdrlen -
				   max_linkhdr);
			if (max_len <= 0) {
				goto failed;
			} else if (len > max_len) {
				len = max_len;
			}
		}
		if (len <= segsiz) {
			/*
			 * In case there are too many small fragments don't
			 * use TSO:
			 */
			tso = 0;
		}
	} else {
		tso = 0;
	}
	if ((tso == 0) && (len > segsiz))
		len = segsiz;
	(void)tcp_get_usecs(tv);
	if ((len == 0) ||
	    (len <= MHLEN - hdrlen - max_linkhdr)) {
		goto failed;
	}
	th->th_seq = htonl(rsm->r_start);
	th->th_ack = htonl(tp->rcv_nxt);
	/*
	 * The PUSH bit should only be applied
	 * if the full retransmission is made. If
	 * we are sending less than this is the
	 * left hand edge and should not have
	 * the PUSH bit.
	 */
	if ((rsm->r_flags & RACK_HAD_PUSH) &&
	    (len == (rsm->r_end - rsm->r_start)))
		flags |= TH_PUSH;
	th->th_win = htons((u_short)(rack->r_ctl.fsb.recwin >> tp->rcv_scale));
	if (th->th_win == 0) {
		tp->t_sndzerowin++;
		tp->t_flags |= TF_RXWIN0SENT;
	} else
		tp->t_flags &= ~TF_RXWIN0SENT;
	if (rsm->r_flags & RACK_TLP) {
		/*
		 * TLP should not count in retran count, but
		 * in its own bin
		 */
		counter_u64_add(rack_tlp_retran, 1);
		counter_u64_add(rack_tlp_retran_bytes, len);
	} else {
		tp->t_sndrexmitpack++;
		KMOD_TCPSTAT_INC(tcps_sndrexmitpack);
		KMOD_TCPSTAT_ADD(tcps_sndrexmitbyte, len);
	}
#ifdef STATS
	stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_RETXPB,
				 len);
#endif
	if (rsm->m == NULL)
		goto failed;
	if (rsm->m &&
	    ((rsm->orig_m_len != rsm->m->m_len) ||
	     (M_TRAILINGROOM(rsm->m) != rsm->orig_t_space))) {
		/* Fix up the orig_m_len and possibly the mbuf offset */
		rack_adjust_orig_mlen(rsm);
	}
	m->m_next = rack_fo_base_copym(rsm->m, rsm->soff, &len, NULL, if_hw_tsomaxsegcount, if_hw_tsomaxsegsize, rsm->r_hw_tls);
	if (len <= segsiz) {
		/*
		 * Must have ran out of mbufs for the copy
		 * shorten it to no longer need tso. Lets
		 * not put on sendalot since we are low on
		 * mbufs.
		 */
		tso = 0;
	}
	if ((m->m_next == NULL) || (len <= 0)){
		goto failed;
	}
	if (udp) {
		if (rack->r_is_v6)
			ulen = hdrlen + len - sizeof(struct ip6_hdr);
		else
			ulen = hdrlen + len - sizeof(struct ip);
		udp->uh_ulen = htons(ulen);
	}
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	if (TCPS_HAVERCVDSYN(tp->t_state) &&
	    (tp->t_flags2 & (TF2_ECN_PERMIT | TF2_ACE_PERMIT))) {
		int ect = tcp_ecn_output_established(tp, &flags, len, true);
		if ((tp->t_state == TCPS_SYN_RECEIVED) &&
		    (tp->t_flags2 & TF2_ECN_SND_ECE))
		    tp->t_flags2 &= ~TF2_ECN_SND_ECE;
#ifdef INET6
		if (rack->r_is_v6) {
		    ip6->ip6_flow &= ~htonl(IPTOS_ECN_MASK << 20);
		    ip6->ip6_flow |= htonl(ect << 20);
		}
		else
#endif
		{
		    ip->ip_tos &= ~IPTOS_ECN_MASK;
		    ip->ip_tos |= ect;
		}
	}
	if (rack->r_ctl.crte != NULL) {
		/* See if we can send via the hw queue */
		slot = rack_check_queue_level(rack, tp, tv, cts, len, segsiz);
		/* If there is nothing in queue (no pacing time) we can send via the hw queue */
		if (slot == 0)
			ip_sendflag = 0;
	}
	tcp_set_flags(th, flags);
	m->m_pkthdr.len = hdrlen + len;	/* in6_cksum() need this */
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	if (to.to_flags & TOF_SIGNATURE) {
		/*
		 * Calculate MD5 signature and put it into the place
		 * determined before.
		 * NOTE: since TCP options buffer doesn't point into
		 * mbuf's data, calculate offset and use it.
		 */
		if (!TCPMD5_ENABLED() || TCPMD5_OUTPUT(m, th,
						       (u_char *)(th + 1) + (to.to_signature - opt)) != 0) {
			/*
			 * Do not send segment if the calculation of MD5
			 * digest has failed.
			 */
			goto failed;
		}
	}
#endif
#ifdef INET6
	if (rack->r_is_v6) {
		if (tp->t_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in6_cksum_pseudo(ip6, ulen, IPPROTO_UDP, 0);
			th->th_sum = htons(0);
			UDPSTAT_INC(udps_opackets);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in6_cksum_pseudo(ip6,
						      sizeof(struct tcphdr) + optlen + len, IPPROTO_TCP,
						      0);
		}
	}
#endif
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		if (tp->t_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in_pseudo(ip->ip_src.s_addr,
						ip->ip_dst.s_addr, htons(ulen + IPPROTO_UDP));
			th->th_sum = htons(0);
			UDPSTAT_INC(udps_opackets);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in_pseudo(ip->ip_src.s_addr,
					       ip->ip_dst.s_addr, htons(sizeof(struct tcphdr) +
									IPPROTO_TCP + len + optlen));
		}
		/* IP version must be set here for ipv4/ipv6 checking later */
		KASSERT(ip->ip_v == IPVERSION,
			("%s: IP version incorrect: %d", __func__, ip->ip_v));
	}
#endif
	if (tso) {
		/*
		 * Here we use segsiz since we have no added options besides
		 * any standard timestamp options (no DSACKs or SACKS are sent
		 * via either fast-path).
		 */
		KASSERT(len > segsiz,
			("%s: len <= tso_segsz tp:%p", __func__, tp));
		m->m_pkthdr.csum_flags |= CSUM_TSO;
		m->m_pkthdr.tso_segsz = segsiz;
	}
#ifdef INET6
	if (rack->r_is_v6) {
		ip6->ip6_hlim = rack->r_ctl.fsb.hoplimit;
		ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6));
		if (V_path_mtu_discovery && tp->t_maxseg > V_tcp_minmss)
			tp->t_flags2 |= TF2_PLPMTU_PMTUD;
		else
			tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_ttl = rack->r_ctl.fsb.hoplimit;
		if (V_path_mtu_discovery && tp->t_maxseg > V_tcp_minmss) {
			tp->t_flags2 |= TF2_PLPMTU_PMTUD;
			if (tp->t_port == 0 || len < V_tcp_minmss) {
				ip->ip_off |= htons(IP_DF);
			}
		} else {
			tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
		}
	}
#endif
	if (doing_tlp == 0) {
		/* Set we retransmitted */
		rack->rc_gp_saw_rec = 1;
	} else {
		/* Its a TLP set ca or ss */
		if (tp->snd_cwnd > tp->snd_ssthresh) {
			/* Set we sent in CA */
			rack->rc_gp_saw_ca = 1;
		} else {
			/* Set we sent in SS */
			rack->rc_gp_saw_ss = 1;
		}
	}
	/* Time to copy in our header */
	cpto = mtod(m, uint8_t *);
	memcpy(cpto, rack->r_ctl.fsb.tcp_ip_hdr, rack->r_ctl.fsb.tcp_ip_hdr_len);
	th = (struct tcphdr *)(cpto + ((uint8_t *)rack->r_ctl.fsb.th - rack->r_ctl.fsb.tcp_ip_hdr));
	if (optlen) {
		bcopy(opt, th + 1, optlen);
		th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
	} else {
		th->th_off = sizeof(struct tcphdr) >> 2;
	}
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;

		if (rsm->r_flags & RACK_RWND_COLLAPSED) {
			rack_log_collapse(rack, rsm->r_start, rsm->r_end, 0, __LINE__, 5, rsm->r_flags, rsm);
			counter_u64_add(rack_collapsed_win_rxt, 1);
			counter_u64_add(rack_collapsed_win_rxt_bytes, (rsm->r_end - rsm->r_start));
		}
		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		if (rack->rack_no_prr)
			log.u_bbr.flex1 = 0;
		else
			log.u_bbr.flex1 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex2 = rack->r_ctl.rc_pace_min_segs;
		log.u_bbr.flex3 = rack->r_ctl.rc_pace_max_segs;
		log.u_bbr.flex4 = max_val;
		/* Save off the early/late values */
		log.u_bbr.flex6 = rack->r_ctl.rc_agg_early;
		log.u_bbr.applimited = rack->r_ctl.rc_agg_delayed;
		log.u_bbr.bw_inuse = rack_get_bw(rack);
		log.u_bbr.cur_del_rate = rack->r_ctl.gp_bw;
		if (doing_tlp == 0)
			log.u_bbr.flex8 = 1;
		else
			log.u_bbr.flex8 = 2;
		log.u_bbr.pacing_gain = rack_get_output_gain(rack, NULL);
		log.u_bbr.flex7 = 55;
		log.u_bbr.pkts_out = tp->t_maxseg;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		if (rsm && (rsm->r_rtr_cnt > 0)) {
			/*
			 * When we have a retransmit we want to log the
			 * burst at send and flight at send from before.
			 */
			log.u_bbr.flex5 = rsm->r_fas;
			log.u_bbr.bbr_substate = rsm->r_bas;
		} else {
			/*
			 * This is currently unlikely until we do the
			 * packet pair probes but I will add it for completeness.
			 */
			log.u_bbr.flex5 = log.u_bbr.inflight;
			log.u_bbr.bbr_substate = (uint8_t)((len + segsiz - 1)/segsiz);
		}
		log.u_bbr.lt_epoch = rack->r_ctl.cwnd_to_use;
		log.u_bbr.delivered = 0;
		log.u_bbr.rttProp = (uint64_t)rsm;
		log.u_bbr.delRate = rsm->r_flags;
		log.u_bbr.delRate <<= 31;
		log.u_bbr.delRate |= rack->r_must_retran;
		log.u_bbr.delRate <<= 1;
		log.u_bbr.delRate |= 1;
		log.u_bbr.pkt_epoch = __LINE__;
		lgb = tcp_log_event(tp, th, NULL, NULL, TCP_LOG_OUT, ERRNO_UNK,
				     len, &log, false, NULL, __func__, __LINE__, tv);
	} else
		lgb = NULL;
	if ((rack->r_ctl.crte != NULL) &&
	    tcp_bblogging_on(tp)) {
		rack_log_queue_level(tp, rack, len, tv, cts);
	}
#ifdef INET6
	if (rack->r_is_v6) {
		error = ip6_output(m, inp->in6p_outputopts,
				   &inp->inp_route6,
				   ip_sendflag, NULL, NULL, inp);
	}
	else
#endif
#ifdef INET
	{
		error = ip_output(m, NULL,
				  &inp->inp_route,
				  ip_sendflag, 0, inp);
	}
#endif
	m = NULL;
	if (lgb) {
		lgb->tlb_errno = error;
		lgb = NULL;
	}
	/* Move snd_nxt to snd_max so we don't have false retransmissions */
	tp->snd_nxt = tp->snd_max;
	if (error) {
		goto failed;
	} else if (rack->rc_hw_nobuf && (ip_sendflag != IP_NO_SND_TAG_RL)) {
		rack->rc_hw_nobuf = 0;
		rack->r_ctl.rc_agg_delayed = 0;
		rack->r_early = 0;
		rack->r_late = 0;
		rack->r_ctl.rc_agg_early = 0;
	}
	rack_log_output(tp, &to, len, rsm->r_start, flags, error, rack_to_usec_ts(tv),
			rsm, RACK_SENT_FP, rsm->m, rsm->soff, rsm->r_hw_tls, segsiz);
	if (doing_tlp) {
		rack->rc_tlp_in_progress = 1;
		rack->r_ctl.rc_tlp_cnt_out++;
	}
	if (error == 0) {
		counter_u64_add(rack_total_bytes, len);
		tcp_account_for_send(tp, len, 1, doing_tlp, rsm->r_hw_tls);
		if (doing_tlp) {
			rack->rc_last_sent_tlp_past_cumack = 0;
			rack->rc_last_sent_tlp_seq_valid = 1;
			rack->r_ctl.last_sent_tlp_seq = rsm->r_start;
			rack->r_ctl.last_sent_tlp_len = rsm->r_end - rsm->r_start;
		}
		if (rack->r_ctl.rc_prr_sndcnt >= len)
			rack->r_ctl.rc_prr_sndcnt -= len;
		else
			rack->r_ctl.rc_prr_sndcnt = 0;
	}
	tp->t_flags &= ~(TF_ACKNOW | TF_DELACK);
	rack->forced_ack = 0;	/* If we send something zap the FA flag */
	if (IN_FASTRECOVERY(tp->t_flags) && rsm)
		rack->r_ctl.retran_during_recovery += len;
	{
		int idx;

		idx = (len / segsiz) + 3;
		if (idx >= TCP_MSS_ACCT_ATIMER)
			counter_u64_add(rack_out_size[(TCP_MSS_ACCT_ATIMER-1)], 1);
		else
			counter_u64_add(rack_out_size[idx], 1);
	}
	if (tp->t_rtttime == 0) {
		tp->t_rtttime = ticks;
		tp->t_rtseq = startseq;
		KMOD_TCPSTAT_INC(tcps_segstimed);
	}
	counter_u64_add(rack_fto_rsm_send, 1);
	if (error && (error == ENOBUFS)) {
		if (rack->r_ctl.crte != NULL) {
			tcp_trace_point(rack->rc_tp, TCP_TP_HWENOBUF);
			if (tcp_bblogging_on(rack->rc_tp))
				rack_log_queue_level(tp, rack, len, tv, cts);
		} else
			tcp_trace_point(rack->rc_tp, TCP_TP_ENOBUF);
		slot = ((1 + rack->rc_enobuf) * HPTS_USEC_IN_MSEC);
		if (rack->rc_enobuf < 0x7f)
			rack->rc_enobuf++;
		if (slot < (10 * HPTS_USEC_IN_MSEC))
			slot = 10 * HPTS_USEC_IN_MSEC;
		if (rack->r_ctl.crte != NULL) {
			counter_u64_add(rack_saw_enobuf_hw, 1);
			tcp_rl_log_enobuf(rack->r_ctl.crte);
		}
		counter_u64_add(rack_saw_enobuf, 1);
	} else {
		slot = rack_get_pacing_delay(rack, tp, len, NULL, segsiz, __LINE__);
	}
	rack_start_hpts_timer(rack, tp, cts, slot, len, 0);
#ifdef TCP_ACCOUNTING
	crtsc = get_cyclecount();
	if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
		tp->tcp_cnt_counters[SND_OUT_DATA] += cnt_thru;
	}
	if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
		tp->tcp_proc_time[SND_OUT_DATA] += (crtsc - ts_val);
	}
	if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
		tp->tcp_cnt_counters[CNT_OF_MSS_OUT] += ((len + segsiz - 1) / segsiz);
	}
	sched_unpin();
#endif
	return (0);
failed:
	if (m)
		m_free(m);
	return (-1);
}

static void
rack_sndbuf_autoscale(struct tcp_rack *rack)
{
	/*
	 * Automatic sizing of send socket buffer.  Often the send buffer
	 * size is not optimally adjusted to the actual network conditions
	 * at hand (delay bandwidth product).  Setting the buffer size too
	 * small limits throughput on links with high bandwidth and high
	 * delay (eg. trans-continental/oceanic links).  Setting the
	 * buffer size too big consumes too much real kernel memory,
	 * especially with many connections on busy servers.
	 *
	 * The criteria to step up the send buffer one notch are:
	 *  1. receive window of remote host is larger than send buffer
	 *     (with a fudge factor of 5/4th);
	 *  2. send buffer is filled to 7/8th with data (so we actually
	 *     have data to make use of it);
	 *  3. send buffer fill has not hit maximal automatic size;
	 *  4. our send window (slow start and cogestion controlled) is
	 *     larger than sent but unacknowledged data in send buffer.
	 *
	 * Note that the rack version moves things much faster since
	 * we want to avoid hitting cache lines in the rack_fast_output()
	 * path so this is called much less often and thus moves
	 * the SB forward by a percentage.
	 */
	struct socket *so;
	struct tcpcb *tp;
	uint32_t sendwin, scaleup;

	tp = rack->rc_tp;
	so = rack->rc_inp->inp_socket;
	sendwin = min(rack->r_ctl.cwnd_to_use, tp->snd_wnd);
	if (V_tcp_do_autosndbuf && so->so_snd.sb_flags & SB_AUTOSIZE) {
		if ((tp->snd_wnd / 4 * 5) >= so->so_snd.sb_hiwat &&
		    sbused(&so->so_snd) >=
		    (so->so_snd.sb_hiwat / 8 * 7) &&
		    sbused(&so->so_snd) < V_tcp_autosndbuf_max &&
		    sendwin >= (sbused(&so->so_snd) -
		    (tp->snd_max - tp->snd_una))) {
			if (rack_autosndbuf_inc)
				scaleup = (rack_autosndbuf_inc * so->so_snd.sb_hiwat) / 100;
			else
				scaleup = V_tcp_autosndbuf_inc;
			if (scaleup < V_tcp_autosndbuf_inc)
				scaleup = V_tcp_autosndbuf_inc;
			scaleup += so->so_snd.sb_hiwat;
			if (scaleup > V_tcp_autosndbuf_max)
				scaleup = V_tcp_autosndbuf_max;
			if (!sbreserve_locked(so, SO_SND, scaleup, curthread))
				so->so_snd.sb_flags &= ~SB_AUTOSIZE;
		}
	}
}

static int
rack_fast_output(struct tcpcb *tp, struct tcp_rack *rack, uint64_t ts_val,
		 uint32_t cts, uint32_t ms_cts, struct timeval *tv, long tot_len, int *send_err)
{
	/*
	 * Enter to do fast output. We are given that the sched_pin is
	 * in place (if accounting is compiled in) and the cycle count taken
	 * at entry is in place in ts_val. The idea here is that
	 * we know how many more bytes needs to be sent (presumably either
	 * during pacing or to fill the cwnd and that was greater than
	 * the max-burst). We have how much to send and all the info we
	 * need to just send.
	 */
#ifdef INET
	struct ip *ip = NULL;
#endif
	struct udphdr *udp = NULL;
	struct tcphdr *th = NULL;
	struct mbuf *m, *s_mb;
	struct inpcb *inp;
	uint8_t *cpto;
	struct tcp_log_buffer *lgb;
#ifdef TCP_ACCOUNTING
	uint64_t crtsc;
#endif
	struct tcpopt to;
	u_char opt[TCP_MAXOLEN];
	uint32_t hdrlen, optlen;
#ifdef TCP_ACCOUNTING
	int cnt_thru = 1;
#endif
	int32_t slot, segsiz, len, max_val, tso = 0, sb_offset, error, ulen = 0;
	uint16_t flags;
	uint32_t s_soff;
	uint32_t if_hw_tsomaxsegcount = 0, startseq;
	uint32_t if_hw_tsomaxsegsize;
	uint32_t add_flag = RACK_SENT_FP;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;

	if (rack->r_is_v6) {
		ip6 = (struct ip6_hdr *)rack->r_ctl.fsb.tcp_ip_hdr;
		hdrlen = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	} else
#endif				/* INET6 */
	{
#ifdef INET
		ip = (struct ip *)rack->r_ctl.fsb.tcp_ip_hdr;
		hdrlen = sizeof(struct tcpiphdr);
#endif
	}
	if (tp->t_port && (V_tcp_udp_tunneling_port == 0)) {
		m = NULL;
		goto failed;
	}
	rack->r_ctl.cwnd_to_use = tp->snd_cwnd;
	startseq = tp->snd_max;
	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	inp = rack->rc_inp;
	len = rack->r_ctl.fsb.left_to_send;
	to.to_flags = 0;
	flags = rack->r_ctl.fsb.tcp_flags;
	if (tp->t_flags & TF_RCVD_TSTMP) {
		to.to_tsval = ms_cts + tp->ts_offset;
		to.to_tsecr = tp->ts_recent;
		to.to_flags = TOF_TS;
	}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	/* TCP-MD5 (RFC2385). */
	if (tp->t_flags & TF_SIGNATURE)
		to.to_flags |= TOF_SIGNATURE;
#endif
	optlen = tcp_addoptions(&to, opt);
	hdrlen += optlen;
	udp = rack->r_ctl.fsb.udp;
	if (udp)
		hdrlen += sizeof(struct udphdr);
	if (rack->r_ctl.rc_pace_max_segs)
		max_val = rack->r_ctl.rc_pace_max_segs;
	else if (rack->rc_user_set_max_segs)
		max_val = rack->rc_user_set_max_segs * segsiz;
	else
		max_val = len;
	if ((tp->t_flags & TF_TSO) &&
	    V_tcp_do_tso &&
	    (len > segsiz) &&
	    (tp->t_port == 0))
		tso = 1;
again:
#ifdef INET6
	if (MHLEN < hdrlen + max_linkhdr)
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	else
#endif
		m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		goto failed;
	m->m_data += max_linkhdr;
	m->m_len = hdrlen;
	th = rack->r_ctl.fsb.th;
	/* Establish the len to send */
	if (len > max_val)
		len = max_val;
	if ((tso) && (len + optlen > segsiz)) {
		uint32_t if_hw_tsomax;
		int32_t max_len;

		/* extract TSO information */
		if_hw_tsomax = tp->t_tsomax;
		if_hw_tsomaxsegcount = tp->t_tsomaxsegcount;
		if_hw_tsomaxsegsize = tp->t_tsomaxsegsize;
		/*
		 * Check if we should limit by maximum payload
		 * length:
		 */
		if (if_hw_tsomax != 0) {
			/* compute maximum TSO length */
			max_len = (if_hw_tsomax - hdrlen -
				   max_linkhdr);
			if (max_len <= 0) {
				goto failed;
			} else if (len > max_len) {
				len = max_len;
			}
		}
		if (len <= segsiz) {
			/*
			 * In case there are too many small fragments don't
			 * use TSO:
			 */
			tso = 0;
		}
	} else {
		tso = 0;
	}
	if ((tso == 0) && (len > segsiz))
		len = segsiz;
	(void)tcp_get_usecs(tv);
	if ((len == 0) ||
	    (len <= MHLEN - hdrlen - max_linkhdr)) {
		goto failed;
	}
	sb_offset = tp->snd_max - tp->snd_una;
	th->th_seq = htonl(tp->snd_max);
	th->th_ack = htonl(tp->rcv_nxt);
	th->th_win = htons((u_short)(rack->r_ctl.fsb.recwin >> tp->rcv_scale));
	if (th->th_win == 0) {
		tp->t_sndzerowin++;
		tp->t_flags |= TF_RXWIN0SENT;
	} else
		tp->t_flags &= ~TF_RXWIN0SENT;
	tp->snd_up = tp->snd_una;	/* drag it along, its deprecated */
	KMOD_TCPSTAT_INC(tcps_sndpack);
	KMOD_TCPSTAT_ADD(tcps_sndbyte, len);
#ifdef STATS
	stats_voi_update_abs_u64(tp->t_stats, VOI_TCP_TXPB,
				 len);
#endif
	if (rack->r_ctl.fsb.m == NULL)
		goto failed;

	/* s_mb and s_soff are saved for rack_log_output */
	m->m_next = rack_fo_m_copym(rack, &len, if_hw_tsomaxsegcount, if_hw_tsomaxsegsize,
				    &s_mb, &s_soff);
	if (len <= segsiz) {
		/*
		 * Must have ran out of mbufs for the copy
		 * shorten it to no longer need tso. Lets
		 * not put on sendalot since we are low on
		 * mbufs.
		 */
		tso = 0;
	}
	if (rack->r_ctl.fsb.rfo_apply_push &&
	    (len == rack->r_ctl.fsb.left_to_send)) {
		tcp_set_flags(th, flags | TH_PUSH);
		add_flag |= RACK_HAD_PUSH;
	}
	if ((m->m_next == NULL) || (len <= 0)){
		goto failed;
	}
	if (udp) {
		if (rack->r_is_v6)
			ulen = hdrlen + len - sizeof(struct ip6_hdr);
		else
			ulen = hdrlen + len - sizeof(struct ip);
		udp->uh_ulen = htons(ulen);
	}
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	if (TCPS_HAVERCVDSYN(tp->t_state) &&
	    (tp->t_flags2 & (TF2_ECN_PERMIT | TF2_ACE_PERMIT))) {
		int ect = tcp_ecn_output_established(tp, &flags, len, false);
		if ((tp->t_state == TCPS_SYN_RECEIVED) &&
		    (tp->t_flags2 & TF2_ECN_SND_ECE))
			tp->t_flags2 &= ~TF2_ECN_SND_ECE;
#ifdef INET6
		if (rack->r_is_v6) {
			ip6->ip6_flow &= ~htonl(IPTOS_ECN_MASK << 20);
			ip6->ip6_flow |= htonl(ect << 20);
		}
		else
#endif
		{
#ifdef INET
			ip->ip_tos &= ~IPTOS_ECN_MASK;
			ip->ip_tos |= ect;
#endif
		}
	}
	tcp_set_flags(th, flags);
	m->m_pkthdr.len = hdrlen + len;	/* in6_cksum() need this */
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	if (to.to_flags & TOF_SIGNATURE) {
		/*
		 * Calculate MD5 signature and put it into the place
		 * determined before.
		 * NOTE: since TCP options buffer doesn't point into
		 * mbuf's data, calculate offset and use it.
		 */
		if (!TCPMD5_ENABLED() || TCPMD5_OUTPUT(m, th,
						       (u_char *)(th + 1) + (to.to_signature - opt)) != 0) {
			/*
			 * Do not send segment if the calculation of MD5
			 * digest has failed.
			 */
			goto failed;
		}
	}
#endif
#ifdef INET6
	if (rack->r_is_v6) {
		if (tp->t_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in6_cksum_pseudo(ip6, ulen, IPPROTO_UDP, 0);
			th->th_sum = htons(0);
			UDPSTAT_INC(udps_opackets);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in6_cksum_pseudo(ip6,
						      sizeof(struct tcphdr) + optlen + len, IPPROTO_TCP,
						      0);
		}
	}
#endif
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		if (tp->t_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in_pseudo(ip->ip_src.s_addr,
						ip->ip_dst.s_addr, htons(ulen + IPPROTO_UDP));
			th->th_sum = htons(0);
			UDPSTAT_INC(udps_opackets);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in_pseudo(ip->ip_src.s_addr,
					       ip->ip_dst.s_addr, htons(sizeof(struct tcphdr) +
									IPPROTO_TCP + len + optlen));
		}
		/* IP version must be set here for ipv4/ipv6 checking later */
		KASSERT(ip->ip_v == IPVERSION,
			("%s: IP version incorrect: %d", __func__, ip->ip_v));
	}
#endif
	if (tso) {
		/*
		 * Here we use segsiz since we have no added options besides
		 * any standard timestamp options (no DSACKs or SACKS are sent
		 * via either fast-path).
		 */
		KASSERT(len > segsiz,
			("%s: len <= tso_segsz tp:%p", __func__, tp));
		m->m_pkthdr.csum_flags |= CSUM_TSO;
		m->m_pkthdr.tso_segsz = segsiz;
	}
#ifdef INET6
	if (rack->r_is_v6) {
		ip6->ip6_hlim = rack->r_ctl.fsb.hoplimit;
		ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6));
		if (V_path_mtu_discovery && tp->t_maxseg > V_tcp_minmss)
			tp->t_flags2 |= TF2_PLPMTU_PMTUD;
		else
			tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_ttl = rack->r_ctl.fsb.hoplimit;
		if (V_path_mtu_discovery && tp->t_maxseg > V_tcp_minmss) {
			tp->t_flags2 |= TF2_PLPMTU_PMTUD;
			if (tp->t_port == 0 || len < V_tcp_minmss) {
				ip->ip_off |= htons(IP_DF);
			}
		} else {
			tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
		}
	}
#endif
	if (tp->snd_cwnd > tp->snd_ssthresh) {
		/* Set we sent in CA */
		rack->rc_gp_saw_ca = 1;
	} else {
		/* Set we sent in SS */
		rack->rc_gp_saw_ss = 1;
	}
	/* Time to copy in our header */
	cpto = mtod(m, uint8_t *);
	memcpy(cpto, rack->r_ctl.fsb.tcp_ip_hdr, rack->r_ctl.fsb.tcp_ip_hdr_len);
	th = (struct tcphdr *)(cpto + ((uint8_t *)rack->r_ctl.fsb.th - rack->r_ctl.fsb.tcp_ip_hdr));
	if (optlen) {
		bcopy(opt, th + 1, optlen);
		th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
	} else {
		th->th_off = sizeof(struct tcphdr) >> 2;
	}
	if ((rack->r_ctl.crte != NULL) &&
	    tcp_bblogging_on(tp)) {
		rack_log_queue_level(tp, rack, len, tv, cts);
	}
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		if (rack->rack_no_prr)
			log.u_bbr.flex1 = 0;
		else
			log.u_bbr.flex1 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex2 = rack->r_ctl.rc_pace_min_segs;
		log.u_bbr.flex3 = rack->r_ctl.rc_pace_max_segs;
		log.u_bbr.flex4 = max_val;
		/* Save off the early/late values */
		log.u_bbr.flex6 = rack->r_ctl.rc_agg_early;
		log.u_bbr.applimited = rack->r_ctl.rc_agg_delayed;
		log.u_bbr.bw_inuse = rack_get_bw(rack);
		log.u_bbr.cur_del_rate = rack->r_ctl.gp_bw;
		log.u_bbr.flex8 = 0;
		log.u_bbr.pacing_gain = rack_get_output_gain(rack, NULL);
		log.u_bbr.flex7 = 44;
		log.u_bbr.pkts_out = tp->t_maxseg;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		log.u_bbr.flex5 = log.u_bbr.inflight;
		log.u_bbr.lt_epoch = rack->r_ctl.cwnd_to_use;
		log.u_bbr.delivered = 0;
		log.u_bbr.rttProp = 0;
		log.u_bbr.delRate = rack->r_must_retran;
		log.u_bbr.delRate <<= 1;
		log.u_bbr.pkt_epoch = __LINE__;
		/* For fast output no retrans so just inflight and how many mss we send */
		log.u_bbr.flex5 = log.u_bbr.inflight;
		log.u_bbr.bbr_substate = (uint8_t)((len + segsiz - 1)/segsiz);
		lgb = tcp_log_event(tp, th, NULL, NULL, TCP_LOG_OUT, ERRNO_UNK,
				     len, &log, false, NULL, __func__, __LINE__, tv);
	} else
		lgb = NULL;
#ifdef INET6
	if (rack->r_is_v6) {
		error = ip6_output(m, inp->in6p_outputopts,
				   &inp->inp_route6,
				   0, NULL, NULL, inp);
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		error = ip_output(m, NULL,
				  &inp->inp_route,
				  0, 0, inp);
	}
#endif
	if (lgb) {
		lgb->tlb_errno = error;
		lgb = NULL;
	}
	if (error) {
		*send_err = error;
		m = NULL;
		goto failed;
	} else if (rack->rc_hw_nobuf) {
		rack->rc_hw_nobuf = 0;
		rack->r_ctl.rc_agg_delayed = 0;
		rack->r_early = 0;
		rack->r_late = 0;
		rack->r_ctl.rc_agg_early = 0;
	}
	if ((error == 0) && (rack->lt_bw_up == 0)) {
		/* Unlikely */
		rack->r_ctl.lt_timemark = tcp_tv_to_lusectick(tv);
		rack->r_ctl.lt_seq = tp->snd_una;
		rack->lt_bw_up = 1;
	} else if ((error == 0) &&
		   (((tp->snd_max + len) - rack->r_ctl.lt_seq) > 0x7fffffff)) {
		/*
		 * Need to record what we have since we are
		 * approaching seq wrap.
		 */
		struct timeval tv;
		uint64_t tmark;

		rack->r_ctl.lt_bw_bytes += (tp->snd_una - rack->r_ctl.lt_seq);
		rack->r_ctl.lt_seq = tp->snd_una;
		tmark = tcp_get_u64_usecs(&tv);
		if (tmark > rack->r_ctl.lt_timemark) {
			rack->r_ctl.lt_bw_time += (tmark - rack->r_ctl.lt_timemark);
			rack->r_ctl.lt_timemark = tmark;
		}
	}
	rack_log_output(tp, &to, len, tp->snd_max, flags, error, rack_to_usec_ts(tv),
			NULL, add_flag, s_mb, s_soff, rack->r_ctl.fsb.hw_tls, segsiz);
	m = NULL;
	if (tp->snd_una == tp->snd_max) {
		rack->r_ctl.rc_tlp_rxt_last_time = cts;
		rack_log_progress_event(rack, tp, ticks, PROGRESS_START, __LINE__);
		tp->t_acktime = ticks;
	}
	counter_u64_add(rack_total_bytes, len);
	tcp_account_for_send(tp, len, 0, 0, rack->r_ctl.fsb.hw_tls);

	rack->forced_ack = 0;	/* If we send something zap the FA flag */
	tot_len += len;
	if ((tp->t_flags & TF_GPUTINPROG) == 0)
		rack_start_gp_measurement(tp, rack, tp->snd_max, sb_offset);
	tp->snd_max += len;
	tp->snd_nxt = tp->snd_max;
	if (rack->rc_new_rnd_needed) {
		rack_new_round_starts(tp, rack, tp->snd_max);
	}
	{
		int idx;

		idx = (len / segsiz) + 3;
		if (idx >= TCP_MSS_ACCT_ATIMER)
			counter_u64_add(rack_out_size[(TCP_MSS_ACCT_ATIMER-1)], 1);
		else
			counter_u64_add(rack_out_size[idx], 1);
	}
	if (len <= rack->r_ctl.fsb.left_to_send)
		rack->r_ctl.fsb.left_to_send -= len;
	else
		rack->r_ctl.fsb.left_to_send = 0;
	if (rack->r_ctl.fsb.left_to_send < segsiz) {
		rack->r_fast_output = 0;
		rack->r_ctl.fsb.left_to_send = 0;
		/* At the end of fast_output scale up the sb */
		SOCKBUF_LOCK(&rack->rc_inp->inp_socket->so_snd);
		rack_sndbuf_autoscale(rack);
		SOCKBUF_UNLOCK(&rack->rc_inp->inp_socket->so_snd);
	}
	if (tp->t_rtttime == 0) {
		tp->t_rtttime = ticks;
		tp->t_rtseq = startseq;
		KMOD_TCPSTAT_INC(tcps_segstimed);
	}
	if ((rack->r_ctl.fsb.left_to_send >= segsiz) &&
	    (max_val > len) &&
	    (tso == 0)) {
		max_val -= len;
		len = segsiz;
		th = rack->r_ctl.fsb.th;
#ifdef TCP_ACCOUNTING
		cnt_thru++;
#endif
		goto again;
	}
	tp->t_flags &= ~(TF_ACKNOW | TF_DELACK);
	counter_u64_add(rack_fto_send, 1);
	slot = rack_get_pacing_delay(rack, tp, tot_len, NULL, segsiz, __LINE__);
	rack_start_hpts_timer(rack, tp, cts, slot, tot_len, 0);
#ifdef TCP_ACCOUNTING
	crtsc = get_cyclecount();
	if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
		tp->tcp_cnt_counters[SND_OUT_DATA] += cnt_thru;
	}
	if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
		tp->tcp_proc_time[SND_OUT_DATA] += (crtsc - ts_val);
	}
	if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
		tp->tcp_cnt_counters[CNT_OF_MSS_OUT] += ((tot_len + segsiz - 1) / segsiz);
	}
	sched_unpin();
#endif
	return (0);
failed:
	if (m)
		m_free(m);
	rack->r_fast_output = 0;
	return (-1);
}

static inline void
rack_setup_fast_output(struct tcpcb *tp, struct tcp_rack *rack,
		       struct sockbuf *sb,
		       int len, int orig_len, int segsiz, uint32_t pace_max_seg,
		       bool hw_tls,
		       uint16_t flags)
{
	rack->r_fast_output = 1;
	rack->r_ctl.fsb.m = sbsndmbuf(sb, (tp->snd_max - tp->snd_una), &rack->r_ctl.fsb.off);
	rack->r_ctl.fsb.o_m_len = rack->r_ctl.fsb.m->m_len;
	rack->r_ctl.fsb.o_t_len = M_TRAILINGROOM(rack->r_ctl.fsb.m);
	rack->r_ctl.fsb.tcp_flags = flags;
	rack->r_ctl.fsb.left_to_send = orig_len - len;
	if (rack->r_ctl.fsb.left_to_send < pace_max_seg) {
		/* Less than a full sized pace, lets not  */
		rack->r_fast_output = 0;
		return;
	} else {
		/* Round down to the nearest pace_max_seg */
		rack->r_ctl.fsb.left_to_send = rounddown(rack->r_ctl.fsb.left_to_send, pace_max_seg);
	}
	if (hw_tls)
		rack->r_ctl.fsb.hw_tls = 1;
	else
		rack->r_ctl.fsb.hw_tls = 0;
	KASSERT((rack->r_ctl.fsb.left_to_send <= (sbavail(sb) - (tp->snd_max - tp->snd_una))),
		("rack:%p left_to_send:%u sbavail:%u out:%u",
		 rack, rack->r_ctl.fsb.left_to_send, sbavail(sb),
		 (tp->snd_max - tp->snd_una)));
	if (rack->r_ctl.fsb.left_to_send < segsiz)
		rack->r_fast_output = 0;
	else {
		if (rack->r_ctl.fsb.left_to_send == (sbavail(sb) - (tp->snd_max - tp->snd_una)))
			rack->r_ctl.fsb.rfo_apply_push = 1;
		else
			rack->r_ctl.fsb.rfo_apply_push = 0;
	}
}

static uint32_t
rack_get_hpts_pacing_min_for_bw(struct tcp_rack *rack, int32_t segsiz)
{
	uint64_t min_time;
	uint32_t maxlen;

	min_time = (uint64_t)get_hpts_min_sleep_time();
	maxlen = (uint32_t)((rack->r_ctl.gp_bw * min_time) / (uint64_t)HPTS_USEC_IN_SEC);
	maxlen = roundup(maxlen, segsiz);
	return (maxlen);
}

static struct rack_sendmap *
rack_check_collapsed(struct tcp_rack *rack, uint32_t cts)
{
	struct rack_sendmap *rsm = NULL;
	int thresh;

restart:
	rsm = tqhash_find(rack->r_ctl.tqh, rack->r_ctl.last_collapse_point);
	if ((rsm == NULL) || ((rsm->r_flags & RACK_RWND_COLLAPSED) == 0)) {
		/* Nothing, strange turn off validity  */
		rack->r_collapse_point_valid = 0;
		return (NULL);
	}
	/* Can we send it yet? */
	if (rsm->r_end > (rack->rc_tp->snd_una + rack->rc_tp->snd_wnd)) {
		/*
		 * Receiver window has not grown enough for
		 * the segment to be put on the wire.
		 */
		return (NULL);
	}
	if (rsm->r_flags & RACK_ACKED) {
		/*
		 * It has been sacked, lets move to the
		 * next one if possible.
		 */
		rack->r_ctl.last_collapse_point = rsm->r_end;
		/* Are we done? */
		if (SEQ_GEQ(rack->r_ctl.last_collapse_point,
			    rack->r_ctl.high_collapse_point)) {
			rack->r_collapse_point_valid = 0;
			return (NULL);
		}
		goto restart;
	}
	/* Now has it been long enough ? */
	thresh = rack_calc_thresh_rack(rack, rack_grab_rtt(rack->rc_tp, rack), cts, __LINE__, 1);
	if ((cts - ((uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)])) > thresh) {
		rack_log_collapse(rack, rsm->r_start,
				  (cts - ((uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)])),
				  thresh, __LINE__, 6, rsm->r_flags, rsm);
		return (rsm);
	}
	/* Not enough time */
	rack_log_collapse(rack, rsm->r_start,
			  (cts - ((uint32_t)rsm->r_tim_lastsent[(rsm->r_rtr_cnt-1)])),
			  thresh, __LINE__, 7, rsm->r_flags, rsm);
	return (NULL);
}

static void
rack_credit_back_policer_idle_time(struct tcp_rack *rack, uint64_t idle_t, int line)
{
	/*
	 * We were idle some time (idle_t) and so our policer bucket
	 * needs to grow. It can go no higher than policer_bucket_size.
	 */
	uint64_t len;

	len = idle_t * rack->r_ctl.policer_bw;
	len /= HPTS_USEC_IN_SEC;
	rack->r_ctl.current_policer_bucket += (uint32_t)len;
	if (rack->r_ctl.policer_bucket_size < rack->r_ctl.current_policer_bucket) {
		rack->r_ctl.current_policer_bucket = rack->r_ctl.policer_bucket_size;
	}
	if (rack_verbose_logging > 0)
		policer_detection_log(rack, (uint32_t)len, line, (uint32_t)idle_t, 0, 7);
}

static inline void
rack_validate_sizes(struct tcp_rack *rack, int32_t *len, int32_t segsiz, uint32_t pace_max_seg)
{
	if ((rack->full_size_rxt == 0) &&
	    (rack->shape_rxt_to_pacing_min == 0) &&
	    (*len >= segsiz)) {
		*len = segsiz;
	} else if (rack->shape_rxt_to_pacing_min &&
		 rack->gp_ready) {
		/* We use pacing min as shaping len req */
		uint32_t maxlen;

		maxlen = rack_get_hpts_pacing_min_for_bw(rack, segsiz);
		if (*len > maxlen)
			*len = maxlen;
	} else {
		/*
		 * The else is full_size_rxt is on so send it all
		 * note we do need to check this for exceeding
		 * our max segment size due to the fact that
		 * we do sometimes merge chunks together i.e.
		 * we cannot just assume that we will never have
		 * a chunk greater than pace_max_seg
		 */
		if (*len > pace_max_seg)
			*len = pace_max_seg;
	}
}

static int
rack_output(struct tcpcb *tp)
{
	struct socket *so;
	uint32_t recwin;
	uint32_t sb_offset, s_moff = 0;
	int32_t len, error = 0;
	uint16_t flags;
	struct mbuf *m, *s_mb = NULL;
	struct mbuf *mb;
	uint32_t if_hw_tsomaxsegcount = 0;
	uint32_t if_hw_tsomaxsegsize;
	int32_t segsiz, minseg;
	long tot_len_this_send = 0;
#ifdef INET
	struct ip *ip = NULL;
#endif
	struct udphdr *udp = NULL;
	struct tcp_rack *rack;
	struct tcphdr *th;
	uint8_t pass = 0;
	uint8_t mark = 0;
	uint8_t check_done = 0;
	uint8_t wanted_cookie = 0;
	u_char opt[TCP_MAXOLEN];
	unsigned ipoptlen, optlen, hdrlen, ulen=0;
	uint32_t rack_seq;

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	unsigned ipsec_optlen = 0;

#endif
	int32_t idle, sendalot;
	uint32_t tot_idle;
	int32_t sub_from_prr = 0;
	volatile int32_t sack_rxmit;
	struct rack_sendmap *rsm = NULL;
	int32_t tso, mtu;
	struct tcpopt to;
	int32_t slot = 0;
	int32_t sup_rack = 0;
	uint32_t cts, ms_cts, delayed, early;
	uint32_t add_flag = RACK_SENT_SP;
	/* The doing_tlp flag will be set by the actual rack_timeout_tlp() */
	uint8_t doing_tlp = 0;
	uint32_t cwnd_to_use, pace_max_seg;
	int32_t do_a_prefetch = 0;
	int32_t prefetch_rsm = 0;
	int32_t orig_len = 0;
	struct timeval tv;
	int32_t prefetch_so_done = 0;
	struct tcp_log_buffer *lgb;
	struct inpcb *inp = tptoinpcb(tp);
	struct sockbuf *sb;
	uint64_t ts_val = 0;
#ifdef TCP_ACCOUNTING
	uint64_t crtsc;
#endif
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
	int32_t isipv6;
#endif
	bool hpts_calling, hw_tls = false;

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);

	/* setup and take the cache hits here */
	rack = (struct tcp_rack *)tp->t_fb_ptr;
#ifdef TCP_ACCOUNTING
	sched_pin();
	ts_val = get_cyclecount();
#endif
	hpts_calling = !!(tp->t_flags2 & TF2_HPTS_CALLS);
	tp->t_flags2 &= ~TF2_HPTS_CALLS;
#ifdef TCP_OFFLOAD
	if (tp->t_flags & TF_TOE) {
#ifdef TCP_ACCOUNTING
		sched_unpin();
#endif
		return (tcp_offload_output(tp));
	}
#endif
	if (rack->rack_deferred_inited == 0) {
		/*
		 * If we are the connecting socket we will
		 * hit rack_init() when no sequence numbers
		 * are setup. This makes it so we must defer
		 * some initialization. Call that now.
		 */
		rack_deferred_init(tp, rack);
	}
	/*
	 * For TFO connections in SYN_RECEIVED, only allow the initial
	 * SYN|ACK and those sent by the retransmit timer.
	 */
	if ((tp->t_flags & TF_FASTOPEN) &&
	    (tp->t_state == TCPS_SYN_RECEIVED) &&
	    SEQ_GT(tp->snd_max, tp->snd_una) &&    /* initial SYN|ACK sent */
	    (rack->r_ctl.rc_resend == NULL)) {         /* not a retransmit */
#ifdef TCP_ACCOUNTING
		sched_unpin();
#endif
		return (0);
	}
#ifdef INET6
	if (rack->r_state) {
		/* Use the cache line loaded if possible */
		isipv6 = rack->r_is_v6;
	} else {
		isipv6 = (rack->rc_inp->inp_vflag & INP_IPV6) != 0;
	}
#endif
	early = 0;
	cts = tcp_get_usecs(&tv);
	ms_cts = tcp_tv_to_mssectick(&tv);
	if (((rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) == 0) &&
	    tcp_in_hpts(rack->rc_tp)) {
		/*
		 * We are on the hpts for some timer but not hptsi output.
		 * Remove from the hpts unconditionally.
		 */
		rack_timer_cancel(tp, rack, cts, __LINE__);
	}
	/* Are we pacing and late? */
	if ((rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) &&
	    TSTMP_GEQ(cts, rack->r_ctl.rc_last_output_to)) {
		/* We are delayed */
		delayed = cts - rack->r_ctl.rc_last_output_to;
	} else {
		delayed = 0;
	}
	/* Do the timers, which may override the pacer */
	if (rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK) {
		int retval;

		retval = rack_process_timers(tp, rack, cts, hpts_calling,
					     &doing_tlp);
		if (retval != 0) {
			counter_u64_add(rack_out_size[TCP_MSS_ACCT_ATIMER], 1);
#ifdef TCP_ACCOUNTING
			sched_unpin();
#endif
			/*
			 * If timers want tcp_drop(), then pass error out,
			 * otherwise suppress it.
			 */
			return (retval < 0 ? retval : 0);
		}
	}
	if (rack->rc_in_persist) {
		if (tcp_in_hpts(rack->rc_tp) == 0) {
			/* Timer is not running */
			rack_start_hpts_timer(rack, tp, cts, 0, 0, 0);
		}
#ifdef TCP_ACCOUNTING
		sched_unpin();
#endif
		return (0);
	}
	if ((rack->rc_ack_required == 1) &&
	    (rack->r_timer_override == 0)){
		/* A timeout occurred and no ack has arrived */
		if (tcp_in_hpts(rack->rc_tp) == 0) {
			/* Timer is not running */
			rack_start_hpts_timer(rack, tp, cts, 0, 0, 0);
		}
#ifdef TCP_ACCOUNTING
		sched_unpin();
#endif
		return (0);
	}
	if ((rack->r_timer_override) ||
	    (rack->rc_ack_can_sendout_data) ||
	    (delayed) ||
	    (tp->t_state < TCPS_ESTABLISHED)) {
		rack->rc_ack_can_sendout_data = 0;
		if (tcp_in_hpts(rack->rc_tp))
			tcp_hpts_remove(rack->rc_tp);
	} else if (tcp_in_hpts(rack->rc_tp)) {
		/*
		 * On the hpts you can't pass even if ACKNOW is on, we will
		 * when the hpts fires.
		 */
#ifdef TCP_ACCOUNTING
		crtsc = get_cyclecount();
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_proc_time[SND_BLOCKED] += (crtsc - ts_val);
		}
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[SND_BLOCKED]++;
		}
		sched_unpin();
#endif
		counter_u64_add(rack_out_size[TCP_MSS_ACCT_INPACE], 1);
		return (0);
	}
	/* Finish out both pacing early and late accounting */
	if ((rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) &&
	    TSTMP_GT(rack->r_ctl.rc_last_output_to, cts)) {
		early = rack->r_ctl.rc_last_output_to - cts;
	} else
		early = 0;
	if (delayed && (rack->rc_always_pace == 1)) {
		rack->r_ctl.rc_agg_delayed += delayed;
		rack->r_late = 1;
	} else if (early && (rack->rc_always_pace == 1)) {
		rack->r_ctl.rc_agg_early += early;
		rack->r_early = 1;
	} else if (rack->rc_always_pace == 0) {
		/* Non-paced we are not late */
		rack->r_ctl.rc_agg_delayed = rack->r_ctl.rc_agg_early = 0;
		rack->r_early = rack->r_late = 0;
	}
	/* Now that early/late accounting is done turn off the flag */
	rack->r_ctl.rc_hpts_flags &= ~PACE_PKT_OUTPUT;
	rack->r_wanted_output = 0;
	rack->r_timer_override = 0;
	if ((tp->t_state != rack->r_state) &&
	    TCPS_HAVEESTABLISHED(tp->t_state)) {
		rack_set_state(tp, rack);
	}
	if ((rack->r_fast_output) &&
	    (doing_tlp == 0) &&
	    (tp->rcv_numsacks == 0)) {
		int ret;

		error = 0;
		ret = rack_fast_output(tp, rack, ts_val, cts, ms_cts, &tv, tot_len_this_send, &error);
		if (ret >= 0)
			return(ret);
		else if (error) {
			inp = rack->rc_inp;
			so = inp->inp_socket;
			sb = &so->so_snd;
			goto nomore;
		}
	}
	inp = rack->rc_inp;
	/*
	 * For TFO connections in SYN_SENT or SYN_RECEIVED,
	 * only allow the initial SYN or SYN|ACK and those sent
	 * by the retransmit timer.
	 */
	if ((tp->t_flags & TF_FASTOPEN) &&
	    ((tp->t_state == TCPS_SYN_RECEIVED) ||
	     (tp->t_state == TCPS_SYN_SENT)) &&
	    SEQ_GT(tp->snd_max, tp->snd_una) && /* initial SYN or SYN|ACK sent */
	    (tp->t_rxtshift == 0)) {              /* not a retransmit */
		cwnd_to_use = rack->r_ctl.cwnd_to_use = tp->snd_cwnd;
		so = inp->inp_socket;
		sb = &so->so_snd;
		goto just_return_nolock;
	}
	/*
	 * Determine length of data that should be transmitted, and flags
	 * that will be used. If there is some data or critical controls
	 * (SYN, RST) to send, then transmit; otherwise, investigate
	 * further.
	 */
	idle = (tp->t_flags & TF_LASTIDLE) || (tp->snd_max == tp->snd_una);
	if (tp->t_idle_reduce) {
		if (idle && (TICKS_2_USEC(ticks - tp->t_rcvtime) >= tp->t_rxtcur))
			rack_cc_after_idle(rack, tp);
	}
	tp->t_flags &= ~TF_LASTIDLE;
	if (idle) {
		if (tp->t_flags & TF_MORETOCOME) {
			tp->t_flags |= TF_LASTIDLE;
			idle = 0;
		}
	}
	if ((tp->snd_una == tp->snd_max) &&
	    rack->r_ctl.rc_went_idle_time &&
	    (cts > rack->r_ctl.rc_went_idle_time)) {
		tot_idle = (cts - rack->r_ctl.rc_went_idle_time);
		if (tot_idle > rack_min_probertt_hold) {
			/* Count as a probe rtt */
			if (rack->in_probe_rtt == 0) {
				rack->r_ctl.rc_lower_rtt_us_cts = cts;
				rack->r_ctl.rc_time_probertt_entered = rack->r_ctl.rc_lower_rtt_us_cts;
				rack->r_ctl.rc_time_probertt_starts = rack->r_ctl.rc_lower_rtt_us_cts;
				rack->r_ctl.rc_time_of_last_probertt = rack->r_ctl.rc_lower_rtt_us_cts;
			} else {
				rack_exit_probertt(rack, cts);
			}
		}
	}
	if(rack->policer_detect_on) {
		/*
		 * If we are doing policer detetion we at a minium
		 * record the time but if possible add back to
		 * the bucket based on the idle time.
		 */
		uint64_t idle_t, u64_cts;

		segsiz = min(ctf_fixed_maxseg(tp),
			     rack->r_ctl.rc_pace_min_segs);
		u64_cts = tcp_tv_to_lusectick(&tv);
		if ((rack->rc_policer_detected == 1) &&
		    (rack->r_ctl.policer_bucket_size > segsiz) &&
		    (rack->r_ctl.policer_bw > 0) &&
		    (u64_cts > rack->r_ctl.last_sendtime)) {
			/* We are being policed add back the time */
			idle_t = u64_cts - rack->r_ctl.last_sendtime;
			rack_credit_back_policer_idle_time(rack, idle_t, __LINE__);
		}
		rack->r_ctl.last_sendtime = u64_cts;
	}
	if (rack_use_fsb &&
	    (rack->r_ctl.fsb.tcp_ip_hdr) &&
	    (rack->r_fsb_inited == 0) &&
	    (rack->r_state != TCPS_CLOSED))
		rack_init_fsb_block(tp, rack, tcp_outflags[tp->t_state]);
	if (rack->rc_sendvars_notset == 1) {
		rack->r_ctl.idle_snd_una = tp->snd_una;
		rack->rc_sendvars_notset = 0;
		/*
		 * Make sure any TCP timers (keep-alive) is not running.
		 */
		tcp_timer_stop(tp);
	}
	if ((rack->rack_no_prr == 1) &&
	    (rack->rc_always_pace == 0)) {
		/*
		 * Sanity check before sending, if we have
		 * no-pacing enabled and prr is turned off that
		 * is a logistics error. Correct this by turnning
		 * prr back on. A user *must* set some form of
		 * pacing in order to turn PRR off. We do this
		 * in the output path so that we can avoid socket
		 * option ordering issues that would occur if we
		 * tried to do it while setting rack_no_prr on.
		 */
		rack->rack_no_prr = 0;
	}
	if ((rack->pcm_enabled == 1) &&
	    (rack->pcm_needed == 0) &&
	    (tot_idle > 0)) {
		/*
		 * We have been idle some micro seconds. We need
		 * to factor this in to see if a PCM is needed.
		 */
		uint32_t rtts_idle, rnds;

		if (tp->t_srtt)
			rtts_idle = tot_idle / tp->t_srtt;
		else
			rtts_idle = 0;
		rnds = rack->r_ctl.current_round - rack->r_ctl.last_pcm_round;
		rack->r_ctl.pcm_idle_rounds += rtts_idle;
		if ((rnds + rack->r_ctl.pcm_idle_rounds)  >= rack_pcm_every_n_rounds) {
			rack->pcm_needed = 1;
			rack_log_pcm(rack, 8, rack->r_ctl.last_pcm_round, rtts_idle, rack->r_ctl.current_round );
		}
	}
again:
	sendalot = 0;
	cts = tcp_get_usecs(&tv);
	ms_cts = tcp_tv_to_mssectick(&tv);
	tso = 0;
	mtu = 0;
	segsiz = min(ctf_fixed_maxseg(tp), rack->r_ctl.rc_pace_min_segs);
	minseg = segsiz;
	if (rack->r_ctl.rc_pace_max_segs == 0)
		pace_max_seg = rack->rc_user_set_max_segs * segsiz;
	else
		pace_max_seg = rack->r_ctl.rc_pace_max_segs;
	if (TCPS_HAVEESTABLISHED(tp->t_state) &&
	    (rack->r_ctl.pcm_max_seg == 0)) {
		/*
		 * We set in our first send so we know that the ctf_fixed_maxseg
		 * has been fully set. If we do it in rack_init() we most likely
		 * see 512 bytes so we end up at 5120, not desirable.
		 */
		rack->r_ctl.pcm_max_seg = rc_init_window(rack);
		if (rack->r_ctl.pcm_max_seg < (ctf_fixed_maxseg(tp) * 10)) {
			/*
			 * Assure our initial PCM probe is at least 10 MSS.
			 */
			rack->r_ctl.pcm_max_seg = ctf_fixed_maxseg(tp) * 10;
		}
	}
	if ((rack->r_ctl.pcm_max_seg != 0)  && (rack->pcm_needed == 1)) {
		uint32_t rw_avail, cwa;

		if (tp->snd_wnd > ctf_outstanding(tp))
			rw_avail = tp->snd_wnd - ctf_outstanding(tp);
		else
			rw_avail = 0;
		if (tp->snd_cwnd > ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked))
			cwa = tp->snd_cwnd -ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		else
			cwa = 0;
		if ((cwa >= rack->r_ctl.pcm_max_seg) &&
		    (rw_avail > rack->r_ctl.pcm_max_seg)) {
			/* Raise up the max seg for this trip through */
			pace_max_seg = rack->r_ctl.pcm_max_seg;
			/* Disable any fast output */
			rack->r_fast_output = 0;
		}
		if (rack_verbose_logging) {
			rack_log_pcm(rack, 4,
				     cwa, rack->r_ctl.pcm_max_seg, rw_avail);
		}
	}
	sb_offset = tp->snd_max - tp->snd_una;
	cwnd_to_use = rack->r_ctl.cwnd_to_use = tp->snd_cwnd;
	flags = tcp_outflags[tp->t_state];
	while (rack->rc_free_cnt < rack_free_cache) {
		rsm = rack_alloc(rack);
		if (rsm == NULL) {
			if (hpts_calling)
				/* Retry in a ms */
				slot = (1 * HPTS_USEC_IN_MSEC);
			so = inp->inp_socket;
			sb = &so->so_snd;
			goto just_return_nolock;
		}
		TAILQ_INSERT_TAIL(&rack->r_ctl.rc_free, rsm, r_tnext);
		rack->rc_free_cnt++;
		rsm = NULL;
	}
	sack_rxmit = 0;
	len = 0;
	rsm = NULL;
	if (flags & TH_RST) {
		SOCKBUF_LOCK(&inp->inp_socket->so_snd);
		so = inp->inp_socket;
		sb = &so->so_snd;
		goto send;
	}
	if (rack->r_ctl.rc_resend) {
		/* Retransmit timer */
		rsm = rack->r_ctl.rc_resend;
		rack->r_ctl.rc_resend = NULL;
		len = rsm->r_end - rsm->r_start;
		sack_rxmit = 1;
		sendalot = 0;
		KASSERT(SEQ_LEQ(tp->snd_una, rsm->r_start),
			("%s:%d: r.start:%u < SND.UNA:%u; tp:%p, rack:%p, rsm:%p",
			 __func__, __LINE__,
			 rsm->r_start, tp->snd_una, tp, rack, rsm));
		sb_offset = rsm->r_start - tp->snd_una;
		rack_validate_sizes(rack, &len, segsiz, pace_max_seg);
	} else if (rack->r_collapse_point_valid &&
		   ((rsm = rack_check_collapsed(rack, cts)) != NULL)) {
		/*
		 * If an RSM is returned then enough time has passed
		 * for us to retransmit it. Move up the collapse point,
		 * since this rsm has its chance to retransmit now.
		 */
		tcp_trace_point(rack->rc_tp, TCP_TP_COLLAPSED_RXT);
		rack->r_ctl.last_collapse_point = rsm->r_end;
		/* Are we done? */
		if (SEQ_GEQ(rack->r_ctl.last_collapse_point,
			    rack->r_ctl.high_collapse_point))
			rack->r_collapse_point_valid = 0;
		sack_rxmit = 1;
		/* We are not doing a TLP */
		doing_tlp = 0;
		len = rsm->r_end - rsm->r_start;
		sb_offset = rsm->r_start - tp->snd_una;
		sendalot = 0;
		rack_validate_sizes(rack, &len, segsiz, pace_max_seg);
	} else if ((rsm = tcp_rack_output(tp, rack, cts)) != NULL) {
		/* We have a retransmit that takes precedence */
		if ((!IN_FASTRECOVERY(tp->t_flags)) &&
		    ((rsm->r_flags & RACK_MUST_RXT) == 0) &&
		    ((tp->t_flags & TF_WASFRECOVERY) == 0)) {
			/* Enter recovery if not induced by a time-out */
			rack_cong_signal(tp, CC_NDUPACK, tp->snd_una, __LINE__);
		}
#ifdef INVARIANTS
		if (SEQ_LT(rsm->r_start, tp->snd_una)) {
			panic("Huh, tp:%p rack:%p rsm:%p start:%u < snd_una:%u\n",
			      tp, rack, rsm, rsm->r_start, tp->snd_una);
		}
#endif
		len = rsm->r_end - rsm->r_start;
		KASSERT(SEQ_LEQ(tp->snd_una, rsm->r_start),
			("%s:%d: r.start:%u < SND.UNA:%u; tp:%p, rack:%p, rsm:%p",
			 __func__, __LINE__,
			 rsm->r_start, tp->snd_una, tp, rack, rsm));
		sb_offset = rsm->r_start - tp->snd_una;
		sendalot = 0;
		rack_validate_sizes(rack, &len, segsiz, pace_max_seg);
		if (len > 0) {
			sack_rxmit = 1;
			KMOD_TCPSTAT_INC(tcps_sack_rexmits);
			KMOD_TCPSTAT_ADD(tcps_sack_rexmit_bytes,
					 min(len, segsiz));
		}
	} else if (rack->r_ctl.rc_tlpsend) {
		/* Tail loss probe */
		long cwin;
		long tlen;

		/*
		 * Check if we can do a TLP with a RACK'd packet
		 * this can happen if we are not doing the rack
		 * cheat and we skipped to a TLP and it
		 * went off.
		 */
		rsm = rack->r_ctl.rc_tlpsend;
		/* We are doing a TLP make sure the flag is preent */
		rsm->r_flags |= RACK_TLP;
		rack->r_ctl.rc_tlpsend = NULL;
		sack_rxmit = 1;
		tlen = rsm->r_end - rsm->r_start;
		if (tlen > segsiz)
			tlen = segsiz;
		KASSERT(SEQ_LEQ(tp->snd_una, rsm->r_start),
			("%s:%d: r.start:%u < SND.UNA:%u; tp:%p, rack:%p, rsm:%p",
			 __func__, __LINE__,
			 rsm->r_start, tp->snd_una, tp, rack, rsm));
		sb_offset = rsm->r_start - tp->snd_una;
		cwin = min(tp->snd_wnd, tlen);
		len = cwin;
	}
	if (rack->r_must_retran &&
	    (doing_tlp == 0) &&
	    (SEQ_GT(tp->snd_max, tp->snd_una)) &&
	    (rsm == NULL)) {
		/*
		 * There are two different ways that we
		 * can get into this block:
		 * a) This is a non-sack connection, we had a time-out
		 *    and thus r_must_retran was set and everything
		 *    left outstanding as been marked for retransmit.
		 * b) The MTU of the path shrank, so that everything
		 *    was marked to be retransmitted with the smaller
		 *    mtu and r_must_retran was set.
		 *
		 * This means that we expect the sendmap (outstanding)
		 * to all be marked must. We can use the tmap to
		 * look at them.
		 *
		 */
		int sendwin, flight;

		sendwin = min(tp->snd_wnd, tp->snd_cwnd);
		flight = ctf_flight_size(tp, rack->r_ctl.rc_out_at_rto);
		if (flight >= sendwin) {
			/*
			 * We can't send yet.
			 */
			so = inp->inp_socket;
			sb = &so->so_snd;
			goto just_return_nolock;
		}
		/*
		 * This is the case a/b mentioned above. All
		 * outstanding/not-acked should be marked.
		 * We can use the tmap to find them.
		 */
		rsm = TAILQ_FIRST(&rack->r_ctl.rc_tmap);
		if (rsm == NULL) {
			/* TSNH */
			rack->r_must_retran = 0;
			rack->r_ctl.rc_out_at_rto = 0;
			so = inp->inp_socket;
			sb = &so->so_snd;
			goto just_return_nolock;
		}
		if ((rsm->r_flags & RACK_MUST_RXT) == 0) {
			/*
			 * The first one does not have the flag, did we collapse
			 * further up in our list?
			 */
			rack->r_must_retran = 0;
			rack->r_ctl.rc_out_at_rto = 0;
			rsm = NULL;
			sack_rxmit = 0;
		} else {
			sack_rxmit = 1;
			len = rsm->r_end - rsm->r_start;
			sb_offset = rsm->r_start - tp->snd_una;
			sendalot = 0;
			if ((rack->full_size_rxt == 0) &&
			    (rack->shape_rxt_to_pacing_min == 0) &&
			    (len >= segsiz))
				len = segsiz;
			else if (rack->shape_rxt_to_pacing_min &&
				 rack->gp_ready) {
				/* We use pacing min as shaping len req */
				uint32_t maxlen;

				maxlen = rack_get_hpts_pacing_min_for_bw(rack, segsiz);
				if (len > maxlen)
					len = maxlen;
			}
			/*
			 * Delay removing the flag RACK_MUST_RXT so
			 * that the fastpath for retransmit will
			 * work with this rsm.
			 */
		}
	}
	/*
	 * Enforce a connection sendmap count limit if set
	 * as long as we are not retransmiting.
	 */
	if ((rsm == NULL) &&
	    (rack->do_detection == 0) &&
	    (V_tcp_map_entries_limit > 0) &&
	    (rack->r_ctl.rc_num_maps_alloced >= V_tcp_map_entries_limit)) {
		counter_u64_add(rack_to_alloc_limited, 1);
		if (!rack->alloc_limit_reported) {
			rack->alloc_limit_reported = 1;
			counter_u64_add(rack_alloc_limited_conns, 1);
		}
		so = inp->inp_socket;
		sb = &so->so_snd;
		goto just_return_nolock;
	}
	if (rsm && (rsm->r_flags & RACK_HAS_FIN)) {
		/* we are retransmitting the fin */
		len--;
		if (len) {
			/*
			 * When retransmitting data do *not* include the
			 * FIN. This could happen from a TLP probe.
			 */
			flags &= ~TH_FIN;
		}
	}
	if (rsm && rack->r_fsb_inited &&
	    rack_use_rsm_rfo &&
	    ((rsm->r_flags & RACK_HAS_FIN) == 0)) {
		int ret;

		if ((rack->rc_policer_detected == 1) &&
		    (rack->r_ctl.policer_bucket_size > segsiz) &&
		    (rack->r_ctl.policer_bw > 0)) {
			/* Check to see if there is room */
			if (rack->r_ctl.current_policer_bucket < len) {
				goto skip_fast_output;
			}
		}
		ret = rack_fast_rsm_output(tp, rack, rsm, ts_val, cts, ms_cts, &tv, len, doing_tlp);
		if (ret == 0)
			return (0);
	}
skip_fast_output:
	so = inp->inp_socket;
	sb = &so->so_snd;
	if (do_a_prefetch == 0) {
		kern_prefetch(sb, &do_a_prefetch);
		do_a_prefetch = 1;
	}
#ifdef NETFLIX_SHARED_CWND
	if ((tp->t_flags2 & TF2_TCP_SCWND_ALLOWED) &&
	    rack->rack_enable_scwnd) {
		/* We are doing cwnd sharing */
		if (rack->gp_ready &&
		    (rack->rack_attempted_scwnd == 0) &&
		    (rack->r_ctl.rc_scw == NULL) &&
		    tp->t_lib) {
			/* The pcbid is in, lets make an attempt */
			counter_u64_add(rack_try_scwnd, 1);
			rack->rack_attempted_scwnd = 1;
			rack->r_ctl.rc_scw = tcp_shared_cwnd_alloc(tp,
								   &rack->r_ctl.rc_scw_index,
								   segsiz);
		}
		if (rack->r_ctl.rc_scw &&
		    (rack->rack_scwnd_is_idle == 1) &&
		    sbavail(&so->so_snd)) {
			/* we are no longer out of data */
			tcp_shared_cwnd_active(rack->r_ctl.rc_scw, rack->r_ctl.rc_scw_index);
			rack->rack_scwnd_is_idle = 0;
		}
		if (rack->r_ctl.rc_scw) {
			/* First lets update and get the cwnd */
			rack->r_ctl.cwnd_to_use = cwnd_to_use = tcp_shared_cwnd_update(rack->r_ctl.rc_scw,
										       rack->r_ctl.rc_scw_index,
										       tp->snd_cwnd, tp->snd_wnd, segsiz);
		}
	}
#endif
	/*
	 * Get standard flags, and add SYN or FIN if requested by 'hidden'
	 * state flags.
	 */
	if (tp->t_flags & TF_NEEDFIN)
		flags |= TH_FIN;
	if (tp->t_flags & TF_NEEDSYN)
		flags |= TH_SYN;
	if ((sack_rxmit == 0) && (prefetch_rsm == 0)) {
		void *end_rsm;
		end_rsm = TAILQ_LAST_FAST(&rack->r_ctl.rc_tmap, rack_sendmap, r_tnext);
		if (end_rsm)
			kern_prefetch(end_rsm, &prefetch_rsm);
		prefetch_rsm = 1;
	}
	SOCKBUF_LOCK(sb);
	if ((sack_rxmit == 0) &&
	    (TCPS_HAVEESTABLISHED(tp->t_state) ||
	    (tp->t_flags & TF_FASTOPEN))) {
		/*
		 * We are not retransmitting (sack_rxmit is 0) so we
		 * are sending new data. This is always based on snd_max.
		 * Now in theory snd_max may be equal to snd_una, if so
		 * then nothing is outstanding and the offset would be 0.
		 */
		uint32_t avail;

		avail = sbavail(sb);
		if (SEQ_GT(tp->snd_max, tp->snd_una) && avail)
			sb_offset = tp->snd_max - tp->snd_una;
		else
			sb_offset = 0;
		if ((IN_FASTRECOVERY(tp->t_flags) == 0) || rack->rack_no_prr) {
			if (rack->r_ctl.rc_tlp_new_data) {
				/* TLP is forcing out new data */
				if (rack->r_ctl.rc_tlp_new_data > (uint32_t) (avail - sb_offset)) {
					rack->r_ctl.rc_tlp_new_data = (uint32_t) (avail - sb_offset);
				}
				if ((rack->r_ctl.rc_tlp_new_data + sb_offset) > tp->snd_wnd) {
					if (tp->snd_wnd > sb_offset)
						len = tp->snd_wnd - sb_offset;
					else
						len = 0;
				} else {
					len = rack->r_ctl.rc_tlp_new_data;
				}
				rack->r_ctl.rc_tlp_new_data = 0;
			}  else {
				len = rack_what_can_we_send(tp, rack, cwnd_to_use, avail, sb_offset);
			}
			if ((rack->r_ctl.crte == NULL) &&
			    IN_FASTRECOVERY(tp->t_flags) &&
			    (rack->full_size_rxt == 0) &&
			    (rack->shape_rxt_to_pacing_min == 0) &&
			    (len > segsiz)) {
				/*
				 * For prr=off, we need to send only 1 MSS
				 * at a time. We do this because another sack could
				 * be arriving that causes us to send retransmits and
				 * we don't want to be on a long pace due to a larger send
				 * that keeps us from sending out the retransmit.
				 */
				len = segsiz;
			} else if (rack->shape_rxt_to_pacing_min &&
				   rack->gp_ready) {
				/* We use pacing min as shaping len req */
				uint32_t maxlen;

				maxlen = rack_get_hpts_pacing_min_for_bw(rack, segsiz);
				if (len > maxlen)
					len = maxlen;
			}/* The else is full_size_rxt is on so send it all */
		} else {
			uint32_t outstanding;
			/*
			 * We are inside of a Fast recovery episode, this
			 * is caused by a SACK or 3 dup acks. At this point
			 * we have sent all the retransmissions and we rely
			 * on PRR to dictate what we will send in the form of
			 * new data.
			 */

			outstanding = tp->snd_max - tp->snd_una;
			if ((rack->r_ctl.rc_prr_sndcnt + outstanding) > tp->snd_wnd) {
				if (tp->snd_wnd > outstanding) {
					len = tp->snd_wnd - outstanding;
					/* Check to see if we have the data */
					if ((sb_offset + len) > avail) {
						/* It does not all fit */
						if (avail > sb_offset)
							len = avail - sb_offset;
						else
							len = 0;
					}
				} else {
					len = 0;
				}
			} else if (avail > sb_offset) {
				len = avail - sb_offset;
			} else {
				len = 0;
			}
			if (len > 0) {
				if (len > rack->r_ctl.rc_prr_sndcnt) {
					len = rack->r_ctl.rc_prr_sndcnt;
				}
				if (len > 0) {
					sub_from_prr = 1;
				}
			}
			if (len > segsiz) {
				/*
				 * We should never send more than a MSS when
				 * retransmitting or sending new data in prr
				 * mode unless the override flag is on. Most
				 * likely the PRR algorithm is not going to
				 * let us send a lot as well :-)
				 */
				if (rack->r_ctl.rc_prr_sendalot == 0) {
					len = segsiz;
				}
			} else if (len < segsiz) {
				/*
				 * Do we send any? The idea here is if the
				 * send empty's the socket buffer we want to
				 * do it. However if not then lets just wait
				 * for our prr_sndcnt to get bigger.
				 */
				long leftinsb;

				leftinsb = sbavail(sb) - sb_offset;
				if (leftinsb > len) {
					/* This send does not empty the sb */
					len = 0;
				}
			}
		}
	} else if (!TCPS_HAVEESTABLISHED(tp->t_state)) {
		/*
		 * If you have not established
		 * and are not doing FAST OPEN
		 * no data please.
		 */
		if ((sack_rxmit == 0) &&
		    !(tp->t_flags & TF_FASTOPEN)) {
			len = 0;
			sb_offset = 0;
		}
	}
	if (prefetch_so_done == 0) {
		kern_prefetch(so, &prefetch_so_done);
		prefetch_so_done = 1;
	}
	orig_len = len;
	if ((rack->rc_policer_detected == 1) &&
	    (rack->r_ctl.policer_bucket_size > segsiz) &&
	    (rack->r_ctl.policer_bw > 0) &&
	    (len > 0)) {
		/*
		 * Ok we believe we have a policer watching
		 * what we send, can we send len? If not can
		 * we tune it down to a smaller value?
		 */
		uint32_t plen, buck_needs;

		plen = rack_policer_check_send(rack, len, segsiz, &buck_needs);
		if (plen == 0) {
			/*
			 * We are not allowed to send. How long
			 * do we need to pace for i.e. how long
			 * before len is available to send?
			 */
			uint64_t lentime;

			lentime = buck_needs;
			lentime *= HPTS_USEC_IN_SEC;
			lentime /= rack->r_ctl.policer_bw;
			slot = (uint32_t)lentime;
			tot_len_this_send = 0;
			SOCKBUF_UNLOCK(sb);
			if (rack_verbose_logging > 0)
				policer_detection_log(rack, len, slot, buck_needs, 0, 12);
			rack_start_hpts_timer(rack, tp, cts, slot, 0, 0);
			rack_log_type_just_return(rack, cts, 0, slot, hpts_calling, 0, cwnd_to_use);
			goto just_return_clean;
		}
		if (plen < len) {
			sendalot = 0;
			len = plen;
		}
	}
	/*
	 * Lop off SYN bit if it has already been sent.  However, if this is
	 * SYN-SENT state and if segment contains data and if we don't know
	 * that foreign host supports TAO, suppress sending segment.
	 */
	if ((flags & TH_SYN) &&
	    SEQ_GT(tp->snd_max, tp->snd_una) &&
	    ((sack_rxmit == 0) &&
	     (tp->t_rxtshift == 0))) {
		/*
		 * When sending additional segments following a TFO SYN|ACK,
		 * do not include the SYN bit.
		 */
		if ((tp->t_flags & TF_FASTOPEN) &&
		    (tp->t_state == TCPS_SYN_RECEIVED))
			flags &= ~TH_SYN;
	}
	/*
	 * Be careful not to send data and/or FIN on SYN segments. This
	 * measure is needed to prevent interoperability problems with not
	 * fully conformant TCP implementations.
	 */
	if ((flags & TH_SYN) && (tp->t_flags & TF_NOOPT)) {
		len = 0;
		flags &= ~TH_FIN;
	}
	/*
	 * On TFO sockets, ensure no data is sent in the following cases:
	 *
	 *  - When retransmitting SYN|ACK on a passively-created socket
	 *
	 *  - When retransmitting SYN on an actively created socket
	 *
	 *  - When sending a zero-length cookie (cookie request) on an
	 *    actively created socket
	 *
	 *  - When the socket is in the CLOSED state (RST is being sent)
	 */
	if ((tp->t_flags & TF_FASTOPEN) &&
	    (((flags & TH_SYN) && (tp->t_rxtshift > 0)) ||
	     ((tp->t_state == TCPS_SYN_SENT) &&
	      (tp->t_tfo_client_cookie_len == 0)) ||
	     (flags & TH_RST))) {
		sack_rxmit = 0;
		len = 0;
	}
	/* Without fast-open there should never be data sent on a SYN */
	if ((flags & TH_SYN) && !(tp->t_flags & TF_FASTOPEN)) {
		len = 0;
	}
	if ((len > segsiz) && (tcp_dsack_block_exists(tp))) {
		/* We only send 1 MSS if we have a DSACK block */
		add_flag |= RACK_SENT_W_DSACK;
		len = segsiz;
	}
	if (len <= 0) {
		/*
		 * We have nothing to send, or the window shrank, or
		 * is closed, do we need to go into persists?
		 */
		len = 0;
		if ((tp->snd_wnd == 0) &&
		    (TCPS_HAVEESTABLISHED(tp->t_state)) &&
		    (tp->snd_una == tp->snd_max) &&
		    (sb_offset < (int)sbavail(sb))) {
			rack_enter_persist(tp, rack, cts, tp->snd_una);
		}
	} else if ((rsm == NULL) &&
		   (doing_tlp == 0) &&
		   (len < pace_max_seg)) {
		/*
		 * We are not sending a maximum sized segment for
		 * some reason. Should we not send anything (think
		 * sws or persists)?
		 */
		if ((tp->snd_wnd < min((rack->r_ctl.rc_high_rwnd/2), minseg)) &&
		    (TCPS_HAVEESTABLISHED(tp->t_state)) &&
		    (len < minseg) &&
		    (len < (int)(sbavail(sb) - sb_offset))) {
			/*
			 * Here the rwnd is less than
			 * the minimum pacing size, this is not a retransmit,
			 * we are established and
			 * the send is not the last in the socket buffer
			 * we send nothing, and we may enter persists
			 * if nothing is outstanding.
			 */
			len = 0;
			if (tp->snd_max == tp->snd_una) {
				/*
				 * Nothing out we can
				 * go into persists.
				 */
				rack_enter_persist(tp, rack, cts, tp->snd_una);
			}
		} else if ((cwnd_to_use >= max(minseg, (segsiz * 4))) &&
			   (ctf_flight_size(tp, rack->r_ctl.rc_sacked) > (2 * segsiz)) &&
			   (len < (int)(sbavail(sb) - sb_offset)) &&
			   (len < minseg)) {
			/*
			 * Here we are not retransmitting, and
			 * the cwnd is not so small that we could
			 * not send at least a min size (rxt timer
			 * not having gone off), We have 2 segments or
			 * more already in flight, its not the tail end
			 * of the socket buffer  and the cwnd is blocking
			 * us from sending out a minimum pacing segment size.
			 * Lets not send anything.
			 */
			len = 0;
		} else if (((tp->snd_wnd - ctf_outstanding(tp)) <
			    min((rack->r_ctl.rc_high_rwnd/2), minseg)) &&
			   (ctf_flight_size(tp, rack->r_ctl.rc_sacked) > (2 * segsiz)) &&
			   (len < (int)(sbavail(sb) - sb_offset)) &&
			   (TCPS_HAVEESTABLISHED(tp->t_state))) {
			/*
			 * Here we have a send window but we have
			 * filled it up and we can't send another pacing segment.
			 * We also have in flight more than 2 segments
			 * and we are not completing the sb i.e. we allow
			 * the last bytes of the sb to go out even if
			 * its not a full pacing segment.
			 */
			len = 0;
		} else if ((rack->r_ctl.crte != NULL) &&
			   (tp->snd_wnd >= (pace_max_seg * max(1, rack_hw_rwnd_factor))) &&
			   (cwnd_to_use >= (pace_max_seg + (4 * segsiz))) &&
			   (ctf_flight_size(tp, rack->r_ctl.rc_sacked) >= (2 * segsiz)) &&
			   (len < (int)(sbavail(sb) - sb_offset))) {
			/*
			 * Here we are doing hardware pacing, this is not a TLP,
			 * we are not sending a pace max segment size, there is rwnd
			 * room to send at least N pace_max_seg, the cwnd is greater
			 * than or equal to a full pacing segments plus 4 mss and we have 2 or
			 * more segments in flight and its not the tail of the socket buffer.
			 *
			 * We don't want to send instead we need to get more ack's in to
			 * allow us to send a full pacing segment. Normally, if we are pacing
			 * about the right speed, we should have finished our pacing
			 * send as most of the acks have come back if we are at the
			 * right rate. This is a bit fuzzy since return path delay
			 * can delay the acks, which is why we want to make sure we
			 * have cwnd space to have a bit more than a max pace segments in flight.
			 *
			 * If we have not gotten our acks back we are pacing at too high a
			 * rate delaying will not hurt and will bring our GP estimate down by
			 * injecting the delay. If we don't do this we will send
			 * 2 MSS out in response to the acks being clocked in which
			 * defeats the point of hw-pacing (i.e. to help us get
			 * larger TSO's out).
			 */
			len = 0;
		}

	}
	/* len will be >= 0 after this point. */
	KASSERT(len >= 0, ("[%s:%d]: len < 0", __func__, __LINE__));
	rack_sndbuf_autoscale(rack);
	/*
	 * Decide if we can use TCP Segmentation Offloading (if supported by
	 * hardware).
	 *
	 * TSO may only be used if we are in a pure bulk sending state.  The
	 * presence of TCP-MD5, SACK retransmits, SACK advertizements and IP
	 * options prevent using TSO.  With TSO the TCP header is the same
	 * (except for the sequence number) for all generated packets.  This
	 * makes it impossible to transmit any options which vary per
	 * generated segment or packet.
	 *
	 * IPv4 handling has a clear separation of ip options and ip header
	 * flags while IPv6 combines both in in6p_outputopts. ip6_optlen() does
	 * the right thing below to provide length of just ip options and thus
	 * checking for ipoptlen is enough to decide if ip options are present.
	 */
	ipoptlen = 0;
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/*
	 * Pre-calculate here as we save another lookup into the darknesses
	 * of IPsec that way and can actually decide if TSO is ok.
	 */
#ifdef INET6
	if (isipv6 && IPSEC_ENABLED(ipv6))
		ipsec_optlen = IPSEC_HDRSIZE(ipv6, inp);
#ifdef INET
	else
#endif
#endif				/* INET6 */
#ifdef INET
		if (IPSEC_ENABLED(ipv4))
			ipsec_optlen = IPSEC_HDRSIZE(ipv4, inp);
#endif				/* INET */
#endif

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	ipoptlen += ipsec_optlen;
#endif
	if ((tp->t_flags & TF_TSO) && V_tcp_do_tso && len > segsiz &&
	    (tp->t_port == 0) &&
	    ((tp->t_flags & TF_SIGNATURE) == 0) &&
	    tp->rcv_numsacks == 0 && sack_rxmit == 0 &&
	    ipoptlen == 0)
		tso = 1;
	{
		uint32_t outstanding __unused;

		outstanding = tp->snd_max - tp->snd_una;
		if (tp->t_flags & TF_SENTFIN) {
			/*
			 * If we sent a fin, snd_max is 1 higher than
			 * snd_una
			 */
			outstanding--;
		}
		if (sack_rxmit) {
			if ((rsm->r_flags & RACK_HAS_FIN) == 0)
				flags &= ~TH_FIN;
		}
	}
	recwin = lmin(lmax(sbspace(&so->so_rcv), 0),
		      (long)TCP_MAXWIN << tp->rcv_scale);

	/*
	 * Sender silly window avoidance.   We transmit under the following
	 * conditions when len is non-zero:
	 *
	 * - We have a full segment (or more with TSO) - This is the last
	 * buffer in a write()/send() and we are either idle or running
	 * NODELAY - we've timed out (e.g. persist timer) - we have more
	 * then 1/2 the maximum send window's worth of data (receiver may be
	 * limited the window size) - we need to retransmit
	 */
	if (len) {
		if (len >= segsiz) {
			goto send;
		}
		/*
		 * NOTE! on localhost connections an 'ack' from the remote
		 * end may occur synchronously with the output and cause us
		 * to flush a buffer queued with moretocome.  XXX
		 *
		 */
		if (!(tp->t_flags & TF_MORETOCOME) &&	/* normal case */
		    (idle || (tp->t_flags & TF_NODELAY)) &&
		    ((uint32_t)len + (uint32_t)sb_offset >= sbavail(sb)) &&
		    (tp->t_flags & TF_NOPUSH) == 0) {
			pass = 2;
			goto send;
		}
		if ((tp->snd_una == tp->snd_max) && len) {	/* Nothing outstanding */
			pass = 22;
			goto send;
		}
		if (len >= tp->max_sndwnd / 2 && tp->max_sndwnd > 0) {
			pass = 4;
			goto send;
		}
		if (sack_rxmit) {
			pass = 6;
			goto send;
		}
		if (((tp->snd_wnd - ctf_outstanding(tp)) < segsiz) &&
		    (ctf_outstanding(tp) < (segsiz * 2))) {
			/*
			 * We have less than two MSS outstanding (delayed ack)
			 * and our rwnd will not let us send a full sized
			 * MSS. Lets go ahead and let this small segment
			 * out because we want to try to have at least two
			 * packets inflight to not be caught by delayed ack.
			 */
			pass = 12;
			goto send;
		}
	}
	/*
	 * Sending of standalone window updates.
	 *
	 * Window updates are important when we close our window due to a
	 * full socket buffer and are opening it again after the application
	 * reads data from it.  Once the window has opened again and the
	 * remote end starts to send again the ACK clock takes over and
	 * provides the most current window information.
	 *
	 * We must avoid the silly window syndrome whereas every read from
	 * the receive buffer, no matter how small, causes a window update
	 * to be sent.  We also should avoid sending a flurry of window
	 * updates when the socket buffer had queued a lot of data and the
	 * application is doing small reads.
	 *
	 * Prevent a flurry of pointless window updates by only sending an
	 * update when we can increase the advertized window by more than
	 * 1/4th of the socket buffer capacity.  When the buffer is getting
	 * full or is very small be more aggressive and send an update
	 * whenever we can increase by two mss sized segments. In all other
	 * situations the ACK's to new incoming data will carry further
	 * window increases.
	 *
	 * Don't send an independent window update if a delayed ACK is
	 * pending (it will get piggy-backed on it) or the remote side
	 * already has done a half-close and won't send more data.  Skip
	 * this if the connection is in T/TCP half-open state.
	 */
	if (recwin > 0 && !(tp->t_flags & TF_NEEDSYN) &&
	    !(tp->t_flags & TF_DELACK) &&
	    !TCPS_HAVERCVDFIN(tp->t_state)) {
		/*
		 * "adv" is the amount we could increase the window, taking
		 * into account that we are limited by TCP_MAXWIN <<
		 * tp->rcv_scale.
		 */
		int32_t adv;
		int oldwin;

		adv = recwin;
		if (SEQ_GT(tp->rcv_adv, tp->rcv_nxt)) {
			oldwin = (tp->rcv_adv - tp->rcv_nxt);
			if (adv > oldwin)
				adv -= oldwin;
			else {
				/* We can't increase the window */
				adv = 0;
			}
		} else
			oldwin = 0;

		/*
		 * If the new window size ends up being the same as or less
		 * than the old size when it is scaled, then don't force
		 * a window update.
		 */
		if (oldwin >> tp->rcv_scale >= (adv + oldwin) >> tp->rcv_scale)
			goto dontupdate;

		if (adv >= (int32_t)(2 * segsiz) &&
		    (adv >= (int32_t)(so->so_rcv.sb_hiwat / 4) ||
		     recwin <= (int32_t)(so->so_rcv.sb_hiwat / 8) ||
		     so->so_rcv.sb_hiwat <= 8 * segsiz)) {
			pass = 7;
			goto send;
		}
		if (2 * adv >= (int32_t) so->so_rcv.sb_hiwat) {
			pass = 23;
			goto send;
		}
	}
dontupdate:

	/*
	 * Send if we owe the peer an ACK, RST, SYN, or urgent data.  ACKNOW
	 * is also a catch-all for the retransmit timer timeout case.
	 */
	if (tp->t_flags & TF_ACKNOW) {
		pass = 8;
		goto send;
	}
	if (((flags & TH_SYN) && (tp->t_flags & TF_NEEDSYN) == 0)) {
		pass = 9;
		goto send;
	}
	/*
	 * If our state indicates that FIN should be sent and we have not
	 * yet done so, then we need to send.
	 */
	if ((flags & TH_FIN) &&
	    (tp->snd_max == tp->snd_una)) {
		pass = 11;
		goto send;
	}
	/*
	 * No reason to send a segment, just return.
	 */
just_return:
	SOCKBUF_UNLOCK(sb);
just_return_nolock:
	{
		int app_limited = CTF_JR_SENT_DATA;

		if ((tp->t_flags & TF_FASTOPEN) == 0 &&
		    (flags & TH_FIN) &&
		    (len == 0) &&
		    (sbused(sb) == (tp->snd_max - tp->snd_una)) &&
		    ((tp->snd_max - tp->snd_una) <= segsiz)) {
			/*
			 * Ok less than or right at a MSS is
			 * outstanding. The original FreeBSD stack would
			 * have sent a FIN, which can speed things up for
			 * a transactional application doing a MSG_WAITALL.
			 * To speed things up since we do *not* send a FIN
			 * if data is outstanding, we send a "challenge ack".
			 * The idea behind that is instead of having to have
			 * the peer wait for the delayed-ack timer to run off
			 * we send an ack that makes the peer send us an ack.
			 */
			rack_send_ack_challange(rack);
		}
		if (tot_len_this_send > 0) {
			rack->r_ctl.fsb.recwin = recwin;
			slot = rack_get_pacing_delay(rack, tp, tot_len_this_send, NULL, segsiz, __LINE__);
			if ((error == 0) &&
			    (rack->rc_policer_detected == 0)  &&
			    rack_use_rfo &&
			    ((flags & (TH_SYN|TH_FIN)) == 0) &&
			    (ipoptlen == 0) &&
			    (tp->rcv_numsacks == 0) &&
			    rack->r_fsb_inited &&
			    TCPS_HAVEESTABLISHED(tp->t_state) &&
			    ((IN_RECOVERY(tp->t_flags)) == 0) &&
			    (rack->r_must_retran == 0) &&
			    ((tp->t_flags & TF_NEEDFIN) == 0) &&
			    (len > 0) && (orig_len > 0) &&
			    (orig_len > len) &&
			    ((orig_len - len) >= segsiz) &&
			    ((optlen == 0) ||
			     ((optlen == TCPOLEN_TSTAMP_APPA) && (to.to_flags & TOF_TS)))) {
				/* We can send at least one more MSS using our fsb */
				rack_setup_fast_output(tp, rack, sb, len, orig_len,
						       segsiz, pace_max_seg, hw_tls, flags);
			} else
				rack->r_fast_output = 0;
			rack_log_fsb(rack, tp, so, flags,
				     ipoptlen, orig_len, len, 0,
				     1, optlen, __LINE__, 1);
			/* Assure when we leave that snd_nxt will point to top */
			if (SEQ_GT(tp->snd_max, tp->snd_nxt))
				tp->snd_nxt = tp->snd_max;
		} else {
			int end_window = 0;
			uint32_t seq = tp->gput_ack;

			rsm = tqhash_max(rack->r_ctl.tqh);
			if (rsm) {
				/*
				 * Mark the last sent that we just-returned (hinting
				 * that delayed ack may play a role in any rtt measurement).
				 */
				rsm->r_just_ret = 1;
			}
			counter_u64_add(rack_out_size[TCP_MSS_ACCT_JUSTRET], 1);
			rack->r_ctl.rc_agg_delayed = 0;
			rack->r_early = 0;
			rack->r_late = 0;
			rack->r_ctl.rc_agg_early = 0;
			if ((ctf_outstanding(tp) +
			     min(max(segsiz, (rack->r_ctl.rc_high_rwnd/2)),
				 minseg)) >= tp->snd_wnd) {
				/* We are limited by the rwnd */
				app_limited = CTF_JR_RWND_LIMITED;
				if (IN_FASTRECOVERY(tp->t_flags))
					rack->r_ctl.rc_prr_sndcnt = 0;
			} else if (ctf_outstanding(tp) >= sbavail(sb)) {
				/* We are limited by whats available -- app limited */
				app_limited = CTF_JR_APP_LIMITED;
				if (IN_FASTRECOVERY(tp->t_flags))
					rack->r_ctl.rc_prr_sndcnt = 0;
			} else if ((idle == 0) &&
				   ((tp->t_flags & TF_NODELAY) == 0) &&
				   ((uint32_t)len + (uint32_t)sb_offset >= sbavail(sb)) &&
				   (len < segsiz)) {
				/*
				 * No delay is not on and the
				 * user is sending less than 1MSS. This
				 * brings out SWS avoidance so we
				 * don't send. Another app-limited case.
				 */
				app_limited = CTF_JR_APP_LIMITED;
			} else if (tp->t_flags & TF_NOPUSH) {
				/*
				 * The user has requested no push of
				 * the last segment and we are
				 * at the last segment. Another app
				 * limited case.
				 */
				app_limited = CTF_JR_APP_LIMITED;
			} else if ((ctf_outstanding(tp) + minseg) > cwnd_to_use) {
				/* Its the cwnd */
				app_limited = CTF_JR_CWND_LIMITED;
			} else if (IN_FASTRECOVERY(tp->t_flags) &&
				   (rack->rack_no_prr == 0) &&
				   (rack->r_ctl.rc_prr_sndcnt < segsiz)) {
				app_limited = CTF_JR_PRR;
			} else {
				/* Now why here are we not sending? */
#ifdef NOW
#ifdef INVARIANTS
				panic("rack:%p hit JR_ASSESSING case cwnd_to_use:%u?", rack, cwnd_to_use);
#endif
#endif
				app_limited = CTF_JR_ASSESSING;
			}
			/*
			 * App limited in some fashion, for our pacing GP
			 * measurements we don't want any gap (even cwnd).
			 * Close  down the measurement window.
			 */
			if (rack_cwnd_block_ends_measure &&
			    ((app_limited == CTF_JR_CWND_LIMITED) ||
			     (app_limited == CTF_JR_PRR))) {
				/*
				 * The reason we are not sending is
				 * the cwnd (or prr). We have been configured
				 * to end the measurement window in
				 * this case.
				 */
				end_window = 1;
			} else if (rack_rwnd_block_ends_measure &&
				   (app_limited == CTF_JR_RWND_LIMITED)) {
				/*
				 * We are rwnd limited and have been
				 * configured to end the measurement
				 * window in this case.
				 */
				end_window = 1;
			} else if (app_limited == CTF_JR_APP_LIMITED) {
				/*
				 * A true application limited period, we have
				 * ran out of data.
				 */
				end_window = 1;
			} else if (app_limited == CTF_JR_ASSESSING) {
				/*
				 * In the assessing case we hit the end of
				 * the if/else and had no known reason
				 * This will panic us under invariants..
				 *
				 * If we get this out in logs we need to
				 * investagate which reason we missed.
				 */
				end_window = 1;
			}
			if (end_window) {
				uint8_t log = 0;

				/* Adjust the Gput measurement */
				if ((tp->t_flags & TF_GPUTINPROG) &&
				    SEQ_GT(tp->gput_ack, tp->snd_max)) {
					tp->gput_ack = tp->snd_max;
					if ((tp->gput_ack - tp->gput_seq) < (MIN_GP_WIN * segsiz)) {
						/*
						 * There is not enough to measure.
						 */
						tp->t_flags &= ~TF_GPUTINPROG;
						rack_log_pacing_delay_calc(rack, (tp->gput_ack - tp->gput_seq) /*flex2*/,
									   rack->r_ctl.rc_gp_srtt /*flex1*/,
									   tp->gput_seq,
									   0, 0, 18, __LINE__, NULL, 0);
					} else
						log = 1;
				}
				/* Mark the last packet has app limited */
				rsm = tqhash_max(rack->r_ctl.tqh);
				if (rsm && ((rsm->r_flags & RACK_APP_LIMITED) == 0)) {
					if (rack->r_ctl.rc_app_limited_cnt == 0)
						rack->r_ctl.rc_end_appl = rack->r_ctl.rc_first_appl = rsm;
					else {
						/*
						 * Go out to the end app limited and mark
						 * this new one as next and move the end_appl up
						 * to this guy.
						 */
						if (rack->r_ctl.rc_end_appl)
							rack->r_ctl.rc_end_appl->r_nseq_appl = rsm->r_start;
						rack->r_ctl.rc_end_appl = rsm;
					}
					rsm->r_flags |= RACK_APP_LIMITED;
					rack->r_ctl.rc_app_limited_cnt++;
				}
				if (log)
					rack_log_pacing_delay_calc(rack,
								   rack->r_ctl.rc_app_limited_cnt, seq,
								   tp->gput_ack, 0, 0, 4, __LINE__, NULL, 0);
			}
		}
		/* Check if we need to go into persists or not */
		if ((tp->snd_max == tp->snd_una) &&
		    TCPS_HAVEESTABLISHED(tp->t_state) &&
		    sbavail(sb) &&
		    (sbavail(sb) > tp->snd_wnd) &&
		    (tp->snd_wnd < min((rack->r_ctl.rc_high_rwnd/2), minseg))) {
			/* Yes lets make sure to move to persist before timer-start */
			rack_enter_persist(tp, rack, rack->r_ctl.rc_rcvtime, tp->snd_una);
		}
		rack_start_hpts_timer(rack, tp, cts, slot, tot_len_this_send, sup_rack);
		rack_log_type_just_return(rack, cts, tot_len_this_send, slot, hpts_calling, app_limited, cwnd_to_use);
	}
just_return_clean:
#ifdef NETFLIX_SHARED_CWND
	if ((sbavail(sb) == 0) &&
	    rack->r_ctl.rc_scw) {
		tcp_shared_cwnd_idle(rack->r_ctl.rc_scw, rack->r_ctl.rc_scw_index);
		rack->rack_scwnd_is_idle = 1;
	}
#endif
#ifdef TCP_ACCOUNTING
	if (tot_len_this_send > 0) {
		crtsc = get_cyclecount();
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[SND_OUT_DATA]++;
		}
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_proc_time[SND_OUT_DATA] += (crtsc - ts_val);
		}
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[CNT_OF_MSS_OUT] += ((tot_len_this_send + segsiz - 1) / segsiz);
		}
	} else {
		crtsc = get_cyclecount();
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[SND_LIMITED]++;
		}
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_proc_time[SND_LIMITED] += (crtsc - ts_val);
		}
	}
	sched_unpin();
#endif
	return (0);

send:
	if ((rack->r_ctl.crte != NULL) &&
	    (rsm == NULL) &&
	    ((rack->rc_hw_nobuf == 1) ||
	     (rack_hw_check_queue && (check_done == 0)))) {
		/*
		 * We only want to do this once with the hw_check_queue,
		 * for the enobuf case we would only do it once if
		 * we come around to again, the flag will be clear.
		 */
		check_done = 1;
		slot = rack_check_queue_level(rack, tp, &tv, cts, len, segsiz);
		if (slot) {
			rack->r_ctl.rc_agg_delayed = 0;
			rack->r_ctl.rc_agg_early = 0;
			rack->r_early = 0;
			rack->r_late = 0;
			SOCKBUF_UNLOCK(&so->so_snd);
			goto skip_all_send;
		}
	}
	if (rsm || sack_rxmit)
		counter_u64_add(rack_nfto_resend, 1);
	else
		counter_u64_add(rack_non_fto_send, 1);
	if ((flags & TH_FIN) &&
	    sbavail(sb)) {
		/*
		 * We do not transmit a FIN
		 * with data outstanding. We
		 * need to make it so all data
		 * is acked first.
		 */
		flags &= ~TH_FIN;
		if ((sbused(sb) == (tp->snd_max - tp->snd_una)) &&
		    ((tp->snd_max - tp->snd_una) <= segsiz)) {
			/*
			 * Ok less than or right at a MSS is
			 * outstanding. The original FreeBSD stack would
			 * have sent a FIN, which can speed things up for
			 * a transactional application doing a MSG_WAITALL.
			 * To speed things up since we do *not* send a FIN
			 * if data is outstanding, we send a "challenge ack".
			 * The idea behind that is instead of having to have
			 * the peer wait for the delayed-ack timer to run off
			 * we send an ack that makes the peer send us an ack.
			 */
			rack_send_ack_challange(rack);
		}
	}
	/* Enforce stack imposed max seg size if we have one */
	if (pace_max_seg &&
	    (len > pace_max_seg)) {
		mark = 1;
		len = pace_max_seg;
	}
	if ((rsm == NULL) &&
	    (rack->pcm_in_progress == 0) &&
	    (rack->r_ctl.pcm_max_seg > 0) &&
	    (len >= rack->r_ctl.pcm_max_seg)) {
		/* It is large enough for a measurement */
		add_flag |= RACK_IS_PCM;
		rack_log_pcm(rack, 5, len, rack->r_ctl.pcm_max_seg,  add_flag);
	} else if (rack_verbose_logging) {
		rack_log_pcm(rack, 6, len, rack->r_ctl.pcm_max_seg,  add_flag);
	}

	SOCKBUF_LOCK_ASSERT(sb);
	if (len > 0) {
		if (len >= segsiz)
			tp->t_flags2 |= TF2_PLPMTU_MAXSEGSNT;
		else
			tp->t_flags2 &= ~TF2_PLPMTU_MAXSEGSNT;
	}
	/*
	 * Before ESTABLISHED, force sending of initial options unless TCP
	 * set not to do any options. NOTE: we assume that the IP/TCP header
	 * plus TCP options always fit in a single mbuf, leaving room for a
	 * maximum link header, i.e. max_linkhdr + sizeof (struct tcpiphdr)
	 * + optlen <= MCLBYTES
	 */
	optlen = 0;
#ifdef INET6
	if (isipv6)
		hdrlen = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	else
#endif
		hdrlen = sizeof(struct tcpiphdr);

	/*
	 * Ok what seq are we sending from. If we have
	 * no rsm to use, then we look at various bits,
	 * if we are putting out a SYN it will be ISS.
	 * If we are retransmitting a FIN it will
	 * be snd_max-1 else its snd_max.
	 */
	if (rsm == NULL) {
		if (flags & TH_SYN)
			rack_seq = tp->iss;
		else if ((flags & TH_FIN) &&
			 (tp->t_flags & TF_SENTFIN))
			rack_seq = tp->snd_max - 1;
		else
			rack_seq = tp->snd_max;
	} else {
		rack_seq = rsm->r_start;
	}
	/*
	 * Compute options for segment. We only have to care about SYN and
	 * established connection segments.  Options for SYN-ACK segments
	 * are handled in TCP syncache.
	 */
	to.to_flags = 0;
	if ((tp->t_flags & TF_NOOPT) == 0) {
		/* Maximum segment size. */
		if (flags & TH_SYN) {
			to.to_mss = tcp_mssopt(&inp->inp_inc);
			if (tp->t_port)
				to.to_mss -= V_tcp_udp_tunneling_overhead;
			to.to_flags |= TOF_MSS;

			/*
			 * On SYN or SYN|ACK transmits on TFO connections,
			 * only include the TFO option if it is not a
			 * retransmit, as the presence of the TFO option may
			 * have caused the original SYN or SYN|ACK to have
			 * been dropped by a middlebox.
			 */
			if ((tp->t_flags & TF_FASTOPEN) &&
			    (tp->t_rxtshift == 0)) {
				if (tp->t_state == TCPS_SYN_RECEIVED) {
					to.to_tfo_len = TCP_FASTOPEN_COOKIE_LEN;
					to.to_tfo_cookie =
						(u_int8_t *)&tp->t_tfo_cookie.server;
					to.to_flags |= TOF_FASTOPEN;
					wanted_cookie = 1;
				} else if (tp->t_state == TCPS_SYN_SENT) {
					to.to_tfo_len =
						tp->t_tfo_client_cookie_len;
					to.to_tfo_cookie =
						tp->t_tfo_cookie.client;
					to.to_flags |= TOF_FASTOPEN;
					wanted_cookie = 1;
					/*
					 * If we wind up having more data to
					 * send with the SYN than can fit in
					 * one segment, don't send any more
					 * until the SYN|ACK comes back from
					 * the other end.
					 */
					sendalot = 0;
				}
			}
		}
		/* Window scaling. */
		if ((flags & TH_SYN) && (tp->t_flags & TF_REQ_SCALE)) {
			to.to_wscale = tp->request_r_scale;
			to.to_flags |= TOF_SCALE;
		}
		/* Timestamps. */
		if ((tp->t_flags & TF_RCVD_TSTMP) ||
		    ((flags & TH_SYN) && (tp->t_flags & TF_REQ_TSTMP))) {
			uint32_t ts_to_use;

			if ((rack->r_rcvpath_rtt_up == 1) &&
			    (ms_cts == rack->r_ctl.last_rcv_tstmp_for_rtt)) {
				/*
				 * When we are doing a rcv_rtt probe all
				 * other timestamps use the next msec. This
				 * is safe since our previous ack is in the
				 * air and we will just have a few more
				 * on the next ms. This assures that only
				 * the one ack has the ms_cts that was on
				 * our ack-probe.
				 */
				ts_to_use = ms_cts + 1;
			} else {
				ts_to_use = ms_cts;
			}
			to.to_tsval = ts_to_use + tp->ts_offset;
			to.to_tsecr = tp->ts_recent;
			to.to_flags |= TOF_TS;
			if ((len == 0) &&
			    (TCPS_HAVEESTABLISHED(tp->t_state)) &&
			    ((ms_cts - rack->r_ctl.last_rcv_tstmp_for_rtt) > RCV_PATH_RTT_MS) &&
			    (tp->snd_una == tp->snd_max) &&
			    (flags & TH_ACK) &&
			    (sbavail(sb) == 0) &&
			    (rack->r_ctl.current_round != 0) &&
			    ((flags & (TH_SYN|TH_FIN)) == 0) &&
			    (rack->r_rcvpath_rtt_up == 0)) {
				rack->r_ctl.last_rcv_tstmp_for_rtt = ms_cts;
				rack->r_ctl.last_time_of_arm_rcv = cts;
				rack->r_rcvpath_rtt_up = 1;
				/* Subtract 1 from seq to force a response */
				rack_seq--;
			}
		}
		/* Set receive buffer autosizing timestamp. */
		if (tp->rfbuf_ts == 0 &&
		    (so->so_rcv.sb_flags & SB_AUTOSIZE)) {
			tp->rfbuf_ts = ms_cts;
		}
		/* Selective ACK's. */
		if (tp->t_flags & TF_SACK_PERMIT) {
			if (flags & TH_SYN)
				to.to_flags |= TOF_SACKPERM;
			else if (TCPS_HAVEESTABLISHED(tp->t_state) &&
				 tp->rcv_numsacks > 0) {
				to.to_flags |= TOF_SACK;
				to.to_nsacks = tp->rcv_numsacks;
				to.to_sacks = (u_char *)tp->sackblks;
			}
		}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		/* TCP-MD5 (RFC2385). */
		if (tp->t_flags & TF_SIGNATURE)
			to.to_flags |= TOF_SIGNATURE;
#endif

		/* Processing the options. */
		hdrlen += optlen = tcp_addoptions(&to, opt);
		/*
		 * If we wanted a TFO option to be added, but it was unable
		 * to fit, ensure no data is sent.
		 */
		if ((tp->t_flags & TF_FASTOPEN) && wanted_cookie &&
		    !(to.to_flags & TOF_FASTOPEN))
			len = 0;
	}
	if (tp->t_port) {
		if (V_tcp_udp_tunneling_port == 0) {
			/* The port was removed?? */
			SOCKBUF_UNLOCK(&so->so_snd);
#ifdef TCP_ACCOUNTING
			crtsc = get_cyclecount();
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[SND_OUT_FAIL]++;
			}
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[SND_OUT_FAIL] += (crtsc - ts_val);
			}
			sched_unpin();
#endif
			return (EHOSTUNREACH);
		}
		hdrlen += sizeof(struct udphdr);
	}
#ifdef INET6
	if (isipv6)
		ipoptlen = ip6_optlen(inp);
	else
#endif
		if (inp->inp_options)
			ipoptlen = inp->inp_options->m_len -
				offsetof(struct ipoption, ipopt_list);
		else
			ipoptlen = 0;
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	ipoptlen += ipsec_optlen;
#endif

	/*
	 * Adjust data length if insertion of options will bump the packet
	 * length beyond the t_maxseg length. Clear the FIN bit because we
	 * cut off the tail of the segment.
	 */
	if (len + optlen + ipoptlen > tp->t_maxseg) {
		if (tso) {
			uint32_t if_hw_tsomax;
			uint32_t moff;
			int32_t max_len;

			/* extract TSO information */
			if_hw_tsomax = tp->t_tsomax;
			if_hw_tsomaxsegcount = tp->t_tsomaxsegcount;
			if_hw_tsomaxsegsize = tp->t_tsomaxsegsize;
			KASSERT(ipoptlen == 0,
				("%s: TSO can't do IP options", __func__));

			/*
			 * Check if we should limit by maximum payload
			 * length:
			 */
			if (if_hw_tsomax != 0) {
				/* compute maximum TSO length */
				max_len = (if_hw_tsomax - hdrlen -
					   max_linkhdr);
				if (max_len <= 0) {
					len = 0;
				} else if (len > max_len) {
					sendalot = 1;
					len = max_len;
					mark = 2;
				}
			}
			/*
			 * Prevent the last segment from being fractional
			 * unless the send sockbuf can be emptied:
			 */
			max_len = (tp->t_maxseg - optlen);
			if ((sb_offset + len) < sbavail(sb)) {
				moff = len % (u_int)max_len;
				if (moff != 0) {
					mark = 3;
					len -= moff;
				}
			}
			/*
			 * In case there are too many small fragments don't
			 * use TSO:
			 */
			if (len <= max_len) {
				mark = 4;
				tso = 0;
			}
			/*
			 * Send the FIN in a separate segment after the bulk
			 * sending is done. We don't trust the TSO
			 * implementations to clear the FIN flag on all but
			 * the last segment.
			 */
			if (tp->t_flags & TF_NEEDFIN) {
				sendalot = 4;
			}
		} else {
			mark = 5;
			if (optlen + ipoptlen >= tp->t_maxseg) {
				/*
				 * Since we don't have enough space to put
				 * the IP header chain and the TCP header in
				 * one packet as required by RFC 7112, don't
				 * send it. Also ensure that at least one
				 * byte of the payload can be put into the
				 * TCP segment.
				 */
				SOCKBUF_UNLOCK(&so->so_snd);
				error = EMSGSIZE;
				sack_rxmit = 0;
				goto out;
			}
			len = tp->t_maxseg - optlen - ipoptlen;
			sendalot = 5;
		}
	} else {
		tso = 0;
		mark = 6;
	}
	KASSERT(len + hdrlen + ipoptlen <= IP_MAXPACKET,
		("%s: len > IP_MAXPACKET", __func__));
#ifdef DIAGNOSTIC
#ifdef INET6
	if (max_linkhdr + hdrlen > MCLBYTES)
#else
		if (max_linkhdr + hdrlen > MHLEN)
#endif
			panic("tcphdr too big");
#endif

	/*
	 * This KASSERT is here to catch edge cases at a well defined place.
	 * Before, those had triggered (random) panic conditions further
	 * down.
	 */
	KASSERT(len >= 0, ("[%s:%d]: len < 0", __func__, __LINE__));
	if ((len == 0) &&
	    (flags & TH_FIN) &&
	    (sbused(sb))) {
		/*
		 * We have outstanding data, don't send a fin by itself!.
		 *
		 * Check to see if we need to send a challenge ack.
		 */
		if ((sbused(sb) == (tp->snd_max - tp->snd_una)) &&
		    ((tp->snd_max - tp->snd_una) <= segsiz)) {
			/*
			 * Ok less than or right at a MSS is
			 * outstanding. The original FreeBSD stack would
			 * have sent a FIN, which can speed things up for
			 * a transactional application doing a MSG_WAITALL.
			 * To speed things up since we do *not* send a FIN
			 * if data is outstanding, we send a "challenge ack".
			 * The idea behind that is instead of having to have
			 * the peer wait for the delayed-ack timer to run off
			 * we send an ack that makes the peer send us an ack.
			 */
			rack_send_ack_challange(rack);
		}
		goto just_return;
	}
	/*
	 * Grab a header mbuf, attaching a copy of data to be transmitted,
	 * and initialize the header from the template for sends on this
	 * connection.
	 */
	hw_tls = tp->t_nic_ktls_xmit != 0;
	if (len) {
		uint32_t max_val;
		uint32_t moff;

		if (pace_max_seg)
			max_val = pace_max_seg;
		else
			max_val = len;
		/*
		 * We allow a limit on sending with hptsi.
		 */
		if (len > max_val) {
			mark = 7;
			len = max_val;
		}
#ifdef INET6
		if (MHLEN < hdrlen + max_linkhdr)
			m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		else
#endif
			m = m_gethdr(M_NOWAIT, MT_DATA);

		if (m == NULL) {
			SOCKBUF_UNLOCK(sb);
			error = ENOBUFS;
			sack_rxmit = 0;
			goto out;
		}
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;

		/*
		 * Start the m_copy functions from the closest mbuf to the
		 * sb_offset in the socket buffer chain.
		 */
		mb = sbsndptr_noadv(sb, sb_offset, &moff);
		s_mb = mb;
		s_moff = moff;
		if (len <= MHLEN - hdrlen - max_linkhdr && !hw_tls) {
			m_copydata(mb, moff, (int)len,
				   mtod(m, caddr_t)+hdrlen);
			/*
			 * If we are not retransmitting advance the
			 * sndptr to help remember the next place in
			 * the sb.
			 */
			if (rsm == NULL)
				sbsndptr_adv(sb, mb, len);
			m->m_len += len;
		} else {
			struct sockbuf *msb;

			/*
			 * If we are not retransmitting pass in msb so
			 * the socket buffer can be advanced. Otherwise
			 * set it to NULL if its a retransmission since
			 * we don't want to change the sb remembered
			 * location.
			 */
			if (rsm == NULL)
				msb = sb;
			else
				msb = NULL;
			m->m_next = tcp_m_copym(
				mb, moff, &len,
				if_hw_tsomaxsegcount, if_hw_tsomaxsegsize, msb,
				((rsm == NULL) ? hw_tls : 0)
#ifdef NETFLIX_COPY_ARGS
				, &s_mb, &s_moff
#endif
				);
			if (len <= (tp->t_maxseg - optlen)) {
				/*
				 * Must have ran out of mbufs for the copy
				 * shorten it to no longer need tso. Lets
				 * not put on sendalot since we are low on
				 * mbufs.
				 */
				tso = 0;
			}
			if (m->m_next == NULL) {
				SOCKBUF_UNLOCK(sb);
				(void)m_free(m);
				error = ENOBUFS;
				sack_rxmit = 0;
				goto out;
			}
		}
		if (sack_rxmit) {
			if (rsm && (rsm->r_flags & RACK_TLP)) {
				/*
				 * TLP should not count in retran count, but
				 * in its own bin
				 */
				counter_u64_add(rack_tlp_retran, 1);
				counter_u64_add(rack_tlp_retran_bytes, len);
			} else {
				tp->t_sndrexmitpack++;
				KMOD_TCPSTAT_INC(tcps_sndrexmitpack);
				KMOD_TCPSTAT_ADD(tcps_sndrexmitbyte, len);
			}
#ifdef STATS
			stats_voi_update_abs_u32(tp->t_stats, VOI_TCP_RETXPB,
						 len);
#endif
		} else {
			KMOD_TCPSTAT_INC(tcps_sndpack);
			KMOD_TCPSTAT_ADD(tcps_sndbyte, len);
#ifdef STATS
			stats_voi_update_abs_u64(tp->t_stats, VOI_TCP_TXPB,
						 len);
#endif
		}
		/*
		 * If we're sending everything we've got, set PUSH. (This
		 * will keep happy those implementations which only give
		 * data to the user when a buffer fills or a PUSH comes in.)
		 */
		if (sb_offset + len == sbused(sb) &&
		    sbused(sb) &&
		    !(flags & TH_SYN)) {
			flags |= TH_PUSH;
			add_flag |= RACK_HAD_PUSH;
		}

		SOCKBUF_UNLOCK(sb);
	} else {
		SOCKBUF_UNLOCK(sb);
		if (tp->t_flags & TF_ACKNOW)
			KMOD_TCPSTAT_INC(tcps_sndacks);
		else if (flags & (TH_SYN | TH_FIN | TH_RST))
			KMOD_TCPSTAT_INC(tcps_sndctrl);
		else
			KMOD_TCPSTAT_INC(tcps_sndwinup);

		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (m == NULL) {
			error = ENOBUFS;
			sack_rxmit = 0;
			goto out;
		}
#ifdef INET6
		if (isipv6 && (MHLEN < hdrlen + max_linkhdr) &&
		    MHLEN >= hdrlen) {
			M_ALIGN(m, hdrlen);
		} else
#endif
			m->m_data += max_linkhdr;
		m->m_len = hdrlen;
	}
	SOCKBUF_UNLOCK_ASSERT(sb);
	m->m_pkthdr.rcvif = (struct ifnet *)0;
#ifdef MAC
	mac_inpcb_create_mbuf(inp, m);
#endif
	if ((ipoptlen == 0) && (rack->r_ctl.fsb.tcp_ip_hdr) &&  rack->r_fsb_inited) {
#ifdef INET6
		if (isipv6)
			ip6 = (struct ip6_hdr *)rack->r_ctl.fsb.tcp_ip_hdr;
		else
#endif				/* INET6 */
#ifdef INET
			ip = (struct ip *)rack->r_ctl.fsb.tcp_ip_hdr;
#endif
		th = rack->r_ctl.fsb.th;
		udp = rack->r_ctl.fsb.udp;
		if (udp) {
#ifdef INET6
			if (isipv6)
				ulen = hdrlen + len - sizeof(struct ip6_hdr);
			else
#endif				/* INET6 */
				ulen = hdrlen + len - sizeof(struct ip);
			udp->uh_ulen = htons(ulen);
		}
	} else {
#ifdef INET6
		if (isipv6) {
			ip6 = mtod(m, struct ip6_hdr *);
			if (tp->t_port) {
				udp = (struct udphdr *)((caddr_t)ip6 + sizeof(struct ip6_hdr));
				udp->uh_sport = htons(V_tcp_udp_tunneling_port);
				udp->uh_dport = tp->t_port;
				ulen = hdrlen + len - sizeof(struct ip6_hdr);
				udp->uh_ulen = htons(ulen);
				th = (struct tcphdr *)(udp + 1);
			} else
				th = (struct tcphdr *)(ip6 + 1);
			tcpip_fillheaders(inp, tp->t_port, ip6, th);
		} else
#endif				/* INET6 */
		{
#ifdef INET
			ip = mtod(m, struct ip *);
			if (tp->t_port) {
				udp = (struct udphdr *)((caddr_t)ip + sizeof(struct ip));
				udp->uh_sport = htons(V_tcp_udp_tunneling_port);
				udp->uh_dport = tp->t_port;
				ulen = hdrlen + len - sizeof(struct ip);
				udp->uh_ulen = htons(ulen);
				th = (struct tcphdr *)(udp + 1);
			} else
				th = (struct tcphdr *)(ip + 1);
			tcpip_fillheaders(inp, tp->t_port, ip, th);
#endif
		}
	}
	/*
	 * If we are starting a connection, send ECN setup SYN packet. If we
	 * are on a retransmit, we may resend those bits a number of times
	 * as per RFC 3168.
	 */
	if (tp->t_state == TCPS_SYN_SENT && V_tcp_do_ecn) {
		flags |= tcp_ecn_output_syn_sent(tp);
	}
	/* Also handle parallel SYN for ECN */
	if (TCPS_HAVERCVDSYN(tp->t_state) &&
	    (tp->t_flags2 & (TF2_ECN_PERMIT | TF2_ACE_PERMIT))) {
		int ect = tcp_ecn_output_established(tp, &flags, len, sack_rxmit);
		if ((tp->t_state == TCPS_SYN_RECEIVED) &&
		    (tp->t_flags2 & TF2_ECN_SND_ECE))
			tp->t_flags2 &= ~TF2_ECN_SND_ECE;
#ifdef INET6
		if (isipv6) {
			ip6->ip6_flow &= ~htonl(IPTOS_ECN_MASK << 20);
			ip6->ip6_flow |= htonl(ect << 20);
		}
		else
#endif
		{
#ifdef INET
			ip->ip_tos &= ~IPTOS_ECN_MASK;
			ip->ip_tos |= ect;
#endif
		}
	}
	th->th_seq = htonl(rack_seq);
	th->th_ack = htonl(tp->rcv_nxt);
	tcp_set_flags(th, flags);
	/*
	 * Calculate receive window.  Don't shrink window, but avoid silly
	 * window syndrome.
	 * If a RST segment is sent, advertise a window of zero.
	 */
	if (flags & TH_RST) {
		recwin = 0;
	} else {
		if (recwin < (long)(so->so_rcv.sb_hiwat / 4) &&
		    recwin < (long)segsiz) {
			recwin = 0;
		}
		if (SEQ_GT(tp->rcv_adv, tp->rcv_nxt) &&
		    recwin < (long)(tp->rcv_adv - tp->rcv_nxt))
			recwin = (long)(tp->rcv_adv - tp->rcv_nxt);
	}

	/*
	 * According to RFC1323 the window field in a SYN (i.e., a <SYN> or
	 * <SYN,ACK>) segment itself is never scaled.  The <SYN,ACK> case is
	 * handled in syncache.
	 */
	if (flags & TH_SYN)
		th->th_win = htons((u_short)
				   (min(sbspace(&so->so_rcv), TCP_MAXWIN)));
	else {
		/* Avoid shrinking window with window scaling. */
		recwin = roundup2(recwin, 1 << tp->rcv_scale);
		th->th_win = htons((u_short)(recwin >> tp->rcv_scale));
	}
	/*
	 * Adjust the RXWIN0SENT flag - indicate that we have advertised a 0
	 * window.  This may cause the remote transmitter to stall.  This
	 * flag tells soreceive() to disable delayed acknowledgements when
	 * draining the buffer.  This can occur if the receiver is
	 * attempting to read more data than can be buffered prior to
	 * transmitting on the connection.
	 */
	if (th->th_win == 0) {
		tp->t_sndzerowin++;
		tp->t_flags |= TF_RXWIN0SENT;
	} else
		tp->t_flags &= ~TF_RXWIN0SENT;
	tp->snd_up = tp->snd_una;	/* drag it along, its deprecated */
	/* Now are we using fsb?, if so copy the template data to the mbuf */
	if ((ipoptlen == 0) && (rack->r_ctl.fsb.tcp_ip_hdr) && rack->r_fsb_inited) {
		uint8_t *cpto;

		cpto = mtod(m, uint8_t *);
		memcpy(cpto, rack->r_ctl.fsb.tcp_ip_hdr, rack->r_ctl.fsb.tcp_ip_hdr_len);
		/*
		 * We have just copied in:
		 * IP/IP6
		 * <optional udphdr>
		 * tcphdr (no options)
		 *
		 * We need to grab the correct pointers into the mbuf
		 * for both the tcp header, and possibly the udp header (if tunneling).
		 * We do this by using the offset in the copy buffer and adding it
		 * to the mbuf base pointer (cpto).
		 */
#ifdef INET6
		if (isipv6)
			ip6 = mtod(m, struct ip6_hdr *);
		else
#endif				/* INET6 */
#ifdef INET
			ip = mtod(m, struct ip *);
#endif
		th = (struct tcphdr *)(cpto + ((uint8_t *)rack->r_ctl.fsb.th - rack->r_ctl.fsb.tcp_ip_hdr));
		/* If we have a udp header lets set it into the mbuf as well */
		if (udp)
			udp = (struct udphdr *)(cpto + ((uint8_t *)rack->r_ctl.fsb.udp - rack->r_ctl.fsb.tcp_ip_hdr));
	}
	if (optlen) {
		bcopy(opt, th + 1, optlen);
		th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
	}
	/*
	 * Put TCP length in extended header, and then checksum extended
	 * header and data.
	 */
	m->m_pkthdr.len = hdrlen + len;	/* in6_cksum() need this */
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	if (to.to_flags & TOF_SIGNATURE) {
		/*
		 * Calculate MD5 signature and put it into the place
		 * determined before.
		 * NOTE: since TCP options buffer doesn't point into
		 * mbuf's data, calculate offset and use it.
		 */
		if (!TCPMD5_ENABLED() || TCPMD5_OUTPUT(m, th,
						       (u_char *)(th + 1) + (to.to_signature - opt)) != 0) {
			/*
			 * Do not send segment if the calculation of MD5
			 * digest has failed.
			 */
			goto out;
		}
	}
#endif
#ifdef INET6
	if (isipv6) {
		/*
		 * ip6_plen is not need to be filled now, and will be filled
		 * in ip6_output.
		 */
		if (tp->t_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in6_cksum_pseudo(ip6, ulen, IPPROTO_UDP, 0);
			th->th_sum = htons(0);
			UDPSTAT_INC(udps_opackets);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in6_cksum_pseudo(ip6,
						      sizeof(struct tcphdr) + optlen + len, IPPROTO_TCP,
						      0);
		}
	}
#endif
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		if (tp->t_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in_pseudo(ip->ip_src.s_addr,
						ip->ip_dst.s_addr, htons(ulen + IPPROTO_UDP));
			th->th_sum = htons(0);
			UDPSTAT_INC(udps_opackets);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in_pseudo(ip->ip_src.s_addr,
					       ip->ip_dst.s_addr, htons(sizeof(struct tcphdr) +
									IPPROTO_TCP + len + optlen));
		}
		/* IP version must be set here for ipv4/ipv6 checking later */
		KASSERT(ip->ip_v == IPVERSION,
			("%s: IP version incorrect: %d", __func__, ip->ip_v));
	}
#endif
	/*
	 * Enable TSO and specify the size of the segments. The TCP pseudo
	 * header checksum is always provided. XXX: Fixme: This is currently
	 * not the case for IPv6.
	 */
	if (tso) {
		/*
		 * Here we must use t_maxseg and the optlen since
		 * the optlen may include SACK's (or DSACK).
		 */
		KASSERT(len > tp->t_maxseg - optlen,
			("%s: len <= tso_segsz", __func__));
		m->m_pkthdr.csum_flags |= CSUM_TSO;
		m->m_pkthdr.tso_segsz = tp->t_maxseg - optlen;
	}
	KASSERT(len + hdrlen == m_length(m, NULL),
		("%s: mbuf chain different than expected: %d + %u != %u",
		 __func__, len, hdrlen, m_length(m, NULL)));

#ifdef TCP_HHOOK
	/* Run HHOOK_TCP_ESTABLISHED_OUT helper hooks. */
	hhook_run_tcp_est_out(tp, th, &to, len, tso);
#endif
	if ((rack->r_ctl.crte != NULL) &&
	    (rack->rc_hw_nobuf == 0) &&
	    tcp_bblogging_on(tp)) {
		rack_log_queue_level(tp, rack, len, &tv, cts);
	}
	/* We're getting ready to send; log now. */
	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(rack->rc_tp);
		if (rack->rack_no_prr)
			log.u_bbr.flex1 = 0;
		else
			log.u_bbr.flex1 = rack->r_ctl.rc_prr_sndcnt;
		log.u_bbr.flex2 = rack->r_ctl.rc_pace_min_segs;
		log.u_bbr.flex3 = rack->r_ctl.rc_pace_max_segs;
		log.u_bbr.flex4 = orig_len;
		/* Save off the early/late values */
		log.u_bbr.flex6 = rack->r_ctl.rc_agg_early;
		log.u_bbr.applimited = rack->r_ctl.rc_agg_delayed;
		log.u_bbr.bw_inuse = rack_get_bw(rack);
		log.u_bbr.cur_del_rate = rack->r_ctl.gp_bw;
		log.u_bbr.flex8 = 0;
		if (rsm) {
			if (rsm->r_flags & RACK_RWND_COLLAPSED) {
				rack_log_collapse(rack, rsm->r_start, rsm->r_end, 0, __LINE__, 5, rsm->r_flags, rsm);
				counter_u64_add(rack_collapsed_win_rxt, 1);
				counter_u64_add(rack_collapsed_win_rxt_bytes, (rsm->r_end - rsm->r_start));
			}
			if (doing_tlp)
				log.u_bbr.flex8 = 2;
			else
				log.u_bbr.flex8 = 1;
		} else {
			if (doing_tlp)
				log.u_bbr.flex8 = 3;
		}
		log.u_bbr.pacing_gain = rack_get_output_gain(rack, rsm);
		log.u_bbr.flex7 = mark;
		log.u_bbr.flex7 <<= 8;
		log.u_bbr.flex7 |= pass;
		log.u_bbr.pkts_out = tp->t_maxseg;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
		if (rsm && (rsm->r_rtr_cnt > 0)) {
			/*
			 * When we have a retransmit we want to log the
			 * burst at send and flight at send from before.
			 */
			log.u_bbr.flex5 = rsm->r_fas;
			log.u_bbr.bbr_substate = rsm->r_bas;
		} else {
			/*
			 * New transmits we log in flex5 the inflight again as
			 * well as the number of segments in our send in the
			 * substate field.
			 */
			log.u_bbr.flex5 = log.u_bbr.inflight;
			log.u_bbr.bbr_substate = (uint8_t)((len + segsiz - 1)/segsiz);
		}
		log.u_bbr.lt_epoch = cwnd_to_use;
		log.u_bbr.delivered = sendalot;
		log.u_bbr.rttProp = (uint64_t)rsm;
		log.u_bbr.pkt_epoch = __LINE__;
		if (rsm) {
			log.u_bbr.delRate = rsm->r_flags;
			log.u_bbr.delRate <<= 31;
			log.u_bbr.delRate |= rack->r_must_retran;
			log.u_bbr.delRate <<= 1;
			log.u_bbr.delRate |= (sack_rxmit & 0x00000001);
		} else {
			log.u_bbr.delRate = rack->r_must_retran;
			log.u_bbr.delRate <<= 1;
			log.u_bbr.delRate |= (sack_rxmit & 0x00000001);
		}
		lgb = tcp_log_event(tp, th, &so->so_rcv, &so->so_snd, TCP_LOG_OUT, ERRNO_UNK,
				    len, &log, false, NULL, __func__, __LINE__, &tv);
	} else
		lgb = NULL;

	/*
	 * Fill in IP length and desired time to live and send to IP level.
	 * There should be a better way to handle ttl and tos; we could keep
	 * them in the template, but need a way to checksum without them.
	 */
	/*
	 * m->m_pkthdr.len should have been set before cksum calcuration,
	 * because in6_cksum() need it.
	 */
#ifdef INET6
	if (isipv6) {
		/*
		 * we separately set hoplimit for every segment, since the
		 * user might want to change the value via setsockopt. Also,
		 * desired default hop limit might be changed via Neighbor
		 * Discovery.
		 */
		rack->r_ctl.fsb.hoplimit = ip6->ip6_hlim = in6_selecthlim(inp, NULL);

		/*
		 * Set the packet size here for the benefit of DTrace
		 * probes. ip6_output() will set it properly; it's supposed
		 * to include the option header lengths as well.
		 */
		ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6));

		if (V_path_mtu_discovery && tp->t_maxseg > V_tcp_minmss)
			tp->t_flags2 |= TF2_PLPMTU_PMTUD;
		else
			tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;

		if (tp->t_state == TCPS_SYN_SENT)
			TCP_PROBE5(connect__request, NULL, tp, ip6, tp, th);

		TCP_PROBE5(send, NULL, tp, ip6, tp, th);
		/* TODO: IPv6 IP6TOS_ECT bit on */
		error = ip6_output(m,
				   inp->in6p_outputopts,
				   &inp->inp_route6,
				   ((rsm || sack_rxmit) ? IP_NO_SND_TAG_RL : 0),
				   NULL, NULL, inp);

		if (error == EMSGSIZE && inp->inp_route6.ro_nh != NULL)
			mtu = inp->inp_route6.ro_nh->nh_mtu;
	}
#endif				/* INET6 */
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		ip->ip_len = htons(m->m_pkthdr.len);
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6PROTO)
			ip->ip_ttl = in6_selecthlim(inp, NULL);
#endif				/* INET6 */
		rack->r_ctl.fsb.hoplimit = ip->ip_ttl;
		/*
		 * If we do path MTU discovery, then we set DF on every
		 * packet. This might not be the best thing to do according
		 * to RFC3390 Section 2. However the tcp hostcache migitates
		 * the problem so it affects only the first tcp connection
		 * with a host.
		 *
		 * NB: Don't set DF on small MTU/MSS to have a safe
		 * fallback.
		 */
		if (V_path_mtu_discovery && tp->t_maxseg > V_tcp_minmss) {
			tp->t_flags2 |= TF2_PLPMTU_PMTUD;
			if (tp->t_port == 0 || len < V_tcp_minmss) {
				ip->ip_off |= htons(IP_DF);
			}
		} else {
			tp->t_flags2 &= ~TF2_PLPMTU_PMTUD;
		}

		if (tp->t_state == TCPS_SYN_SENT)
			TCP_PROBE5(connect__request, NULL, tp, ip, tp, th);

		TCP_PROBE5(send, NULL, tp, ip, tp, th);

		error = ip_output(m,
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
				  inp->inp_options,
#else
				  NULL,
#endif
				  &inp->inp_route,
				  ((rsm || sack_rxmit) ? IP_NO_SND_TAG_RL : 0), 0,
				  inp);
		if (error == EMSGSIZE && inp->inp_route.ro_nh != NULL)
			mtu = inp->inp_route.ro_nh->nh_mtu;
	}
#endif				/* INET */
	if (lgb) {
		lgb->tlb_errno = error;
		lgb = NULL;
	}

out:
	/*
	 * In transmit state, time the transmission and arrange for the
	 * retransmit.  In persist state, just set snd_max.
	 */
	rack_log_output(tp, &to, len, rack_seq, (uint8_t) flags, error,
			rack_to_usec_ts(&tv),
			rsm, add_flag, s_mb, s_moff, hw_tls, segsiz);
	if (error == 0) {
		if (add_flag & RACK_IS_PCM) {
			/* We just launched a PCM */
			/* rrs here log */
			rack->pcm_in_progress = 1;
			rack->pcm_needed = 0;
			rack_log_pcm(rack, 7, len, rack->r_ctl.pcm_max_seg,  add_flag);
		}
		if (rsm == NULL) {
			if (rack->lt_bw_up == 0) {
				rack->r_ctl.lt_timemark = tcp_tv_to_lusectick(&tv);
				rack->r_ctl.lt_seq = tp->snd_una;
				rack->lt_bw_up = 1;
			} else if (((rack_seq + len) - rack->r_ctl.lt_seq) > 0x7fffffff) {
				/*
				 * Need to record what we have since we are
				 * approaching seq wrap.
				 */
				uint64_t tmark;

				rack->r_ctl.lt_bw_bytes += (tp->snd_una - rack->r_ctl.lt_seq);
				rack->r_ctl.lt_seq = tp->snd_una;
				tmark = tcp_get_u64_usecs(&tv);
				if (tmark > rack->r_ctl.lt_timemark) {
					rack->r_ctl.lt_bw_time += (tmark - rack->r_ctl.lt_timemark);
					rack->r_ctl.lt_timemark = tmark;
				}
			}
		}
		rack->forced_ack = 0;	/* If we send something zap the FA flag */
		counter_u64_add(rack_total_bytes, len);
		tcp_account_for_send(tp, len, (rsm != NULL), doing_tlp, hw_tls);
		if (rsm && doing_tlp) {
			rack->rc_last_sent_tlp_past_cumack = 0;
			rack->rc_last_sent_tlp_seq_valid = 1;
			rack->r_ctl.last_sent_tlp_seq = rsm->r_start;
			rack->r_ctl.last_sent_tlp_len = rsm->r_end - rsm->r_start;
		}
		if (rack->rc_hw_nobuf) {
			rack->rc_hw_nobuf = 0;
			rack->r_ctl.rc_agg_delayed = 0;
			rack->r_early = 0;
			rack->r_late = 0;
			rack->r_ctl.rc_agg_early = 0;
		}
		if (rsm && (doing_tlp == 0)) {
			/* Set we retransmitted */
			rack->rc_gp_saw_rec = 1;
		} else {
			if (cwnd_to_use > tp->snd_ssthresh) {
				/* Set we sent in CA */
				rack->rc_gp_saw_ca = 1;
			} else {
				/* Set we sent in SS */
				rack->rc_gp_saw_ss = 1;
			}
		}
		if (TCPS_HAVEESTABLISHED(tp->t_state) &&
		    (tp->t_flags & TF_SACK_PERMIT) &&
		    tp->rcv_numsacks > 0)
			tcp_clean_dsack_blocks(tp);
		tot_len_this_send += len;
		if (len == 0) {
			counter_u64_add(rack_out_size[TCP_MSS_ACCT_SNDACK], 1);
		} else {
			int idx;

			idx = (len / segsiz) + 3;
			if (idx >= TCP_MSS_ACCT_ATIMER)
				counter_u64_add(rack_out_size[(TCP_MSS_ACCT_ATIMER-1)], 1);
			else
				counter_u64_add(rack_out_size[idx], 1);
		}
	}
	if ((rack->rack_no_prr == 0) &&
	    sub_from_prr &&
	    (error == 0)) {
		if (rack->r_ctl.rc_prr_sndcnt >= len)
			rack->r_ctl.rc_prr_sndcnt -= len;
		else
			rack->r_ctl.rc_prr_sndcnt = 0;
	}
	sub_from_prr = 0;
	if (doing_tlp) {
		/* Make sure the TLP is added */
		add_flag |= RACK_TLP;
	} else if (rsm) {
		/* If its a resend without TLP then it must not have the flag */
		rsm->r_flags &= ~RACK_TLP;
	}


	if ((error == 0) &&
	    (len > 0) &&
	    (tp->snd_una == tp->snd_max))
		rack->r_ctl.rc_tlp_rxt_last_time = cts;

	{
		/*
		 * This block is not associated with the above error == 0 test.
		 * It is used to advance snd_max if we have a new transmit.
		 */
		tcp_seq startseq = tp->snd_max;


		if (rsm && (doing_tlp == 0))
			rack->r_ctl.rc_loss_count += rsm->r_end - rsm->r_start;
		if (error)
			/* We don't log or do anything with errors */
			goto nomore;
		if (doing_tlp == 0) {
			if (rsm == NULL) {
				/*
				 * Not a retransmission of some
				 * sort, new data is going out so
				 * clear our TLP count and flag.
				 */
				rack->rc_tlp_in_progress = 0;
				rack->r_ctl.rc_tlp_cnt_out = 0;
			}
		} else {
			/*
			 * We have just sent a TLP, mark that it is true
			 * and make sure our in progress is set so we
			 * continue to check the count.
			 */
			rack->rc_tlp_in_progress = 1;
			rack->r_ctl.rc_tlp_cnt_out++;
		}
		/*
		 * If we are retransmitting we are done, snd_max
		 * does not get updated.
		 */
		if (sack_rxmit)
			goto nomore;
		if ((tp->snd_una == tp->snd_max) && (len > 0)) {
			/*
			 * Update the time we just added data since
			 * nothing was outstanding.
			 */
			rack_log_progress_event(rack, tp, ticks, PROGRESS_START, __LINE__);
			tp->t_acktime = ticks;
		}
		/*
		 * Now for special SYN/FIN handling.
		 */
		if (flags & (TH_SYN | TH_FIN)) {
			if ((flags & TH_SYN) &&
			    ((tp->t_flags & TF_SENTSYN) == 0)) {
				tp->snd_max++;
				tp->t_flags |= TF_SENTSYN;
			}
			if ((flags & TH_FIN) &&
			    ((tp->t_flags & TF_SENTFIN) == 0)) {
				tp->snd_max++;
				tp->t_flags |= TF_SENTFIN;
			}
		}
		tp->snd_max += len;
		if (rack->rc_new_rnd_needed) {
			rack_new_round_starts(tp, rack, tp->snd_max);
		}
		/*
		 * Time this transmission if not a retransmission and
		 * not currently timing anything.
		 * This is only relevant in case of switching back to
		 * the base stack.
		 */
		if (tp->t_rtttime == 0) {
			tp->t_rtttime = ticks;
			tp->t_rtseq = startseq;
			KMOD_TCPSTAT_INC(tcps_segstimed);
		}
		if (len &&
		    ((tp->t_flags & TF_GPUTINPROG) == 0))
			rack_start_gp_measurement(tp, rack, startseq, sb_offset);
		/*
		 * If we are doing FO we need to update the mbuf position and subtract
		 * this happens when the peer sends us duplicate information and
		 * we thus want to send a DSACK.
		 *
		 * XXXRRS: This brings to mind a ?, when we send a DSACK block is TSO
		 * turned off? If not then we are going to echo multiple DSACK blocks
		 * out (with the TSO), which we should not be doing.
		 */
		if (rack->r_fast_output && len) {
			if (rack->r_ctl.fsb.left_to_send > len)
				rack->r_ctl.fsb.left_to_send -= len;
			else
				rack->r_ctl.fsb.left_to_send = 0;
			if (rack->r_ctl.fsb.left_to_send < segsiz)
				rack->r_fast_output = 0;
			if (rack->r_fast_output) {
				rack->r_ctl.fsb.m = sbsndmbuf(sb, (tp->snd_max - tp->snd_una), &rack->r_ctl.fsb.off);
				rack->r_ctl.fsb.o_m_len = rack->r_ctl.fsb.m->m_len;
				rack->r_ctl.fsb.o_t_len = M_TRAILINGROOM(rack->r_ctl.fsb.m);
			}
		}
		if (rack_pcm_blast == 0) {
			if ((orig_len > len) &&
			    (add_flag & RACK_IS_PCM) &&
			    (len < pace_max_seg) &&
			    ((pace_max_seg - len) > segsiz)) {
				/*
				 * We are doing a PCM measurement and we did
				 * not get enough data in the TSO to meet the
				 * burst requirement.
				 */
				uint32_t n_len;

				n_len = (orig_len - len);
				orig_len -= len;
				pace_max_seg -= len;
				len = n_len;
				sb_offset = tp->snd_max - tp->snd_una;
				/* Re-lock for the next spin */
				SOCKBUF_LOCK(sb);
				goto send;
			}
		} else {
			if ((orig_len > len) &&
			    (add_flag & RACK_IS_PCM) &&
			    ((orig_len - len) > segsiz)) {
				/*
				 * We are doing a PCM measurement and we did
				 * not get enough data in the TSO to meet the
				 * burst requirement.
				 */
				uint32_t n_len;

				n_len = (orig_len - len);
				orig_len -= len;
				len = n_len;
				sb_offset = tp->snd_max - tp->snd_una;
				/* Re-lock for the next spin */
				SOCKBUF_LOCK(sb);
				goto send;
			}
		}
	}
nomore:
	if (error) {
		rack->r_ctl.rc_agg_delayed = 0;
		rack->r_early = 0;
		rack->r_late = 0;
		rack->r_ctl.rc_agg_early = 0;
		SOCKBUF_UNLOCK_ASSERT(sb);	/* Check gotos. */
		/*
		 * Failures do not advance the seq counter above. For the
		 * case of ENOBUFS we will fall out and retry in 1ms with
		 * the hpts. Everything else will just have to retransmit
		 * with the timer.
		 *
		 * In any case, we do not want to loop around for another
		 * send without a good reason.
		 */
		sendalot = 0;
		switch (error) {
		case EPERM:
		case EACCES:
			tp->t_softerror = error;
#ifdef TCP_ACCOUNTING
			crtsc = get_cyclecount();
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[SND_OUT_FAIL]++;
			}
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[SND_OUT_FAIL] += (crtsc - ts_val);
			}
			sched_unpin();
#endif
			return (error);
		case ENOBUFS:
			/*
			 * Pace us right away to retry in a some
			 * time
			 */
			if (rack->r_ctl.crte != NULL) {
				tcp_trace_point(rack->rc_tp, TCP_TP_HWENOBUF);
				if (tcp_bblogging_on(rack->rc_tp))
					rack_log_queue_level(tp, rack, len, &tv, cts);
			} else
				tcp_trace_point(rack->rc_tp, TCP_TP_ENOBUF);
			slot = ((1 + rack->rc_enobuf) * HPTS_USEC_IN_MSEC);
			if (rack->rc_enobuf < 0x7f)
				rack->rc_enobuf++;
			if (slot < (10 * HPTS_USEC_IN_MSEC))
				slot = 10 * HPTS_USEC_IN_MSEC;
			if (rack->r_ctl.crte != NULL) {
				counter_u64_add(rack_saw_enobuf_hw, 1);
				tcp_rl_log_enobuf(rack->r_ctl.crte);
			}
			counter_u64_add(rack_saw_enobuf, 1);
			goto enobufs;
		case EMSGSIZE:
			/*
			 * For some reason the interface we used initially
			 * to send segments changed to another or lowered
			 * its MTU. If TSO was active we either got an
			 * interface without TSO capabilits or TSO was
			 * turned off. If we obtained mtu from ip_output()
			 * then update it and try again.
			 */
			if (tso)
				tp->t_flags &= ~TF_TSO;
			if (mtu != 0) {
				int saved_mtu;

				saved_mtu = tp->t_maxseg;
				tcp_mss_update(tp, -1, mtu, NULL, NULL);
				if (saved_mtu > tp->t_maxseg) {
					goto again;
				}
			}
			slot = 10 * HPTS_USEC_IN_MSEC;
			rack_start_hpts_timer(rack, tp, cts, slot, 0, 0);
#ifdef TCP_ACCOUNTING
			crtsc = get_cyclecount();
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[SND_OUT_FAIL]++;
			}
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[SND_OUT_FAIL] += (crtsc - ts_val);
			}
			sched_unpin();
#endif
			return (error);
		case ENETUNREACH:
			counter_u64_add(rack_saw_enetunreach, 1);
		case EHOSTDOWN:
		case EHOSTUNREACH:
		case ENETDOWN:
			if (TCPS_HAVERCVDSYN(tp->t_state)) {
				tp->t_softerror = error;
			}
			/* FALLTHROUGH */
		default:
			slot = 10 * HPTS_USEC_IN_MSEC;
			rack_start_hpts_timer(rack, tp, cts, slot, 0, 0);
#ifdef TCP_ACCOUNTING
			crtsc = get_cyclecount();
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[SND_OUT_FAIL]++;
			}
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_proc_time[SND_OUT_FAIL] += (crtsc - ts_val);
			}
			sched_unpin();
#endif
			return (error);
		}
	} else {
		rack->rc_enobuf = 0;
		if (IN_FASTRECOVERY(tp->t_flags) && rsm)
			rack->r_ctl.retran_during_recovery += len;
	}
	KMOD_TCPSTAT_INC(tcps_sndtotal);

	/*
	 * Data sent (as far as we can tell). If this advertises a larger
	 * window than any other segment, then remember the size of the
	 * advertised window. Any pending ACK has now been sent.
	 */
	if (recwin > 0 && SEQ_GT(tp->rcv_nxt + recwin, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + recwin;

	tp->last_ack_sent = tp->rcv_nxt;
	tp->t_flags &= ~(TF_ACKNOW | TF_DELACK);
enobufs:
	if (sendalot) {
		/* Do we need to turn off sendalot? */
		if (pace_max_seg &&
		    (tot_len_this_send >= pace_max_seg)) {
			/* We hit our max. */
			sendalot = 0;
		}
	}
	if ((error == 0) && (flags & TH_FIN))
		tcp_log_end_status(tp, TCP_EI_STATUS_SERVER_FIN);
	if (flags & TH_RST) {
		/*
		 * We don't send again after sending a RST.
		 */
		slot = 0;
		sendalot = 0;
		if (error == 0)
			tcp_log_end_status(tp, TCP_EI_STATUS_SERVER_RST);
	} else if ((slot == 0) && (sendalot == 0) && tot_len_this_send) {
		/*
		 * Get our pacing rate, if an error
		 * occurred in sending (ENOBUF) we would
		 * hit the else if with slot preset. Other
		 * errors return.
		 */
		slot = rack_get_pacing_delay(rack, tp, tot_len_this_send, rsm, segsiz, __LINE__);
	}
	/* We have sent clear the flag */
	rack->r_ent_rec_ns = 0;
	if (rack->r_must_retran) {
		if (rsm) {
			rack->r_ctl.rc_out_at_rto -= (rsm->r_end - rsm->r_start);
			if (SEQ_GEQ(rsm->r_end, rack->r_ctl.rc_snd_max_at_rto)) {
				/*
				 * We have retransmitted all.
				 */
				rack->r_must_retran = 0;
				rack->r_ctl.rc_out_at_rto = 0;
			}
		} else if (SEQ_GEQ(tp->snd_max, rack->r_ctl.rc_snd_max_at_rto)) {
			/*
			 * Sending new data will also kill
			 * the loop.
			 */
			rack->r_must_retran = 0;
			rack->r_ctl.rc_out_at_rto = 0;
		}
	}
	rack->r_ctl.fsb.recwin = recwin;
	if ((tp->t_flags & (TF_WASCRECOVERY|TF_WASFRECOVERY)) &&
	    SEQ_GT(tp->snd_max, rack->r_ctl.rc_snd_max_at_rto)) {
		/*
		 * We hit an RTO and now have past snd_max at the RTO
		 * clear all the WAS flags.
		 */
		tp->t_flags &= ~(TF_WASCRECOVERY|TF_WASFRECOVERY);
	}
	if (slot) {
		/* set the rack tcb into the slot N */
		if ((error == 0) &&
		    rack_use_rfo &&
		    ((flags & (TH_SYN|TH_FIN)) == 0) &&
		    (rsm == NULL) &&
		    (ipoptlen == 0) &&
		    (tp->rcv_numsacks == 0) &&
		    (rack->rc_policer_detected == 0)  &&
		    rack->r_fsb_inited &&
		    TCPS_HAVEESTABLISHED(tp->t_state) &&
		    ((IN_RECOVERY(tp->t_flags)) == 0) &&
		    (rack->r_must_retran == 0) &&
		    ((tp->t_flags & TF_NEEDFIN) == 0) &&
		    (len > 0) && (orig_len > 0) &&
		    (orig_len > len) &&
		    ((orig_len - len) >= segsiz) &&
		    ((optlen == 0) ||
		     ((optlen == TCPOLEN_TSTAMP_APPA) && (to.to_flags & TOF_TS)))) {
			/* We can send at least one more MSS using our fsb */
			rack_setup_fast_output(tp, rack, sb, len, orig_len,
					       segsiz, pace_max_seg, hw_tls, flags);
		} else
			rack->r_fast_output = 0;
		rack_log_fsb(rack, tp, so, flags,
			     ipoptlen, orig_len, len, error,
			     (rsm == NULL), optlen, __LINE__, 2);
	} else if (sendalot) {
		int ret;

		sack_rxmit = 0;
		if ((error == 0) &&
		    rack_use_rfo &&
		    ((flags & (TH_SYN|TH_FIN)) == 0) &&
		    (rsm == NULL) &&
		    (ipoptlen == 0) &&
		    (tp->rcv_numsacks == 0) &&
		    (rack->r_must_retran == 0) &&
		    rack->r_fsb_inited &&
		    TCPS_HAVEESTABLISHED(tp->t_state) &&
		    ((IN_RECOVERY(tp->t_flags)) == 0) &&
		    ((tp->t_flags & TF_NEEDFIN) == 0) &&
		    (len > 0) && (orig_len > 0) &&
		    (orig_len > len) &&
		    ((orig_len - len) >= segsiz) &&
		    ((optlen == 0) ||
		     ((optlen == TCPOLEN_TSTAMP_APPA) && (to.to_flags & TOF_TS)))) {
			/* we can use fast_output for more */
			rack_setup_fast_output(tp, rack, sb, len, orig_len,
					       segsiz, pace_max_seg, hw_tls, flags);
			if (rack->r_fast_output) {
				error = 0;
				ret = rack_fast_output(tp, rack, ts_val, cts, ms_cts, &tv, tot_len_this_send, &error);
				if (ret >= 0)
					return (ret);
			        else if (error)
					goto nomore;

			}
		}
		goto again;
	}
skip_all_send:
	/* Assure when we leave that snd_nxt will point to top */
	if (SEQ_GT(tp->snd_max, tp->snd_nxt))
		tp->snd_nxt = tp->snd_max;
	rack_start_hpts_timer(rack, tp, cts, slot, tot_len_this_send, 0);
#ifdef TCP_ACCOUNTING
	crtsc = get_cyclecount() - ts_val;
	if (tot_len_this_send) {
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[SND_OUT_DATA]++;
		}
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_proc_time[SND_OUT_DATA] += crtsc;
		}
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[CNT_OF_MSS_OUT] += ((tot_len_this_send + segsiz - 1) /segsiz);
		}
	} else {
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_cnt_counters[SND_OUT_ACK]++;
		}
		if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
			tp->tcp_proc_time[SND_OUT_ACK] += crtsc;
		}
	}
	sched_unpin();
#endif
	if (error == ENOBUFS)
		error = 0;
	return (error);
}

static void
rack_update_seg(struct tcp_rack *rack)
{
	uint32_t orig_val;

	orig_val = rack->r_ctl.rc_pace_max_segs;
	rack_set_pace_segments(rack->rc_tp, rack, __LINE__, NULL);
	if (orig_val != rack->r_ctl.rc_pace_max_segs)
		rack_log_pacing_delay_calc(rack, 0, 0, orig_val, 0, 0, 15, __LINE__, NULL, 0);
}

static void
rack_mtu_change(struct tcpcb *tp)
{
	/*
	 * The MSS may have changed
	 */
	struct tcp_rack *rack;
	struct rack_sendmap *rsm;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack->r_ctl.rc_pace_min_segs != ctf_fixed_maxseg(tp)) {
		/*
		 * The MTU has changed we need to resend everything
		 * since all we have sent is lost. We first fix
		 * up the mtu though.
		 */
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		/* We treat this like a full retransmit timeout without the cwnd adjustment */
		rack_remxt_tmr(tp);
		rack->r_fast_output = 0;
		rack->r_ctl.rc_out_at_rto = ctf_flight_size(tp,
						rack->r_ctl.rc_sacked);
		rack->r_ctl.rc_snd_max_at_rto = tp->snd_max;
		rack->r_must_retran = 1;
		/* Mark all inflight to needing to be rxt'd */
		TAILQ_FOREACH(rsm, &rack->r_ctl.rc_tmap, r_tnext) {
			rsm->r_flags |= (RACK_MUST_RXT|RACK_PMTU_CHG);
		}
	}
	sack_filter_clear(&rack->r_ctl.rack_sf, tp->snd_una);
	/* We don't use snd_nxt to retransmit */
	tp->snd_nxt = tp->snd_max;
}

static int
rack_set_dgp(struct tcp_rack *rack)
{
	if (rack->dgp_on == 1)
		return(0);
	if ((rack->use_fixed_rate == 1) &&
	    (rack->rc_always_pace == 1)) {
		/*
		 * We are already pacing another
		 * way.
		 */
		return (EBUSY);
	}
	if (rack->rc_always_pace == 1) {
		rack_remove_pacing(rack);
	}
	if (tcp_incr_dgp_pacing_cnt() == 0)
		return (ENOSPC);
	rack->r_ctl.pacing_method |= RACK_DGP_PACING;
	rack->rc_fillcw_apply_discount = 0;
	rack->dgp_on = 1;
	rack->rc_always_pace = 1;
	rack->rc_pace_dnd = 1;
	rack->use_fixed_rate = 0;
	if (rack->gp_ready)
		rack_set_cc_pacing(rack);
	rack->rc_tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
	rack->rack_attempt_hdwr_pace = 0;
	/* rxt settings */
	rack->full_size_rxt = 1;
	rack->shape_rxt_to_pacing_min  = 0;
	/* cmpack=1 */
	rack->r_use_cmp_ack = 1;
	if (TCPS_HAVEESTABLISHED(rack->rc_tp->t_state) &&
	    rack->r_use_cmp_ack)
		rack->rc_tp->t_flags2 |= TF2_MBUF_ACKCMP;
	/* scwnd=1 */
	rack->rack_enable_scwnd = 1;
	/* dynamic=100 */
	rack->rc_gp_dyn_mul = 1;
	/* gp_inc_ca */
	rack->r_ctl.rack_per_of_gp_ca = 100;
	/* rrr_conf=3 */
	rack->r_rr_config = 3;
	/* npush=2 */
	rack->r_ctl.rc_no_push_at_mrtt = 2;
	/* fillcw=1 */
	rack->rc_pace_to_cwnd = 1;
	rack->rc_pace_fill_if_rttin_range = 0;
	rack->rtt_limit_mul = 0;
	/* noprr=1 */
	rack->rack_no_prr = 1;
	/* lscwnd=1 */
	rack->r_limit_scw = 1;
	/* gp_inc_rec */
	rack->r_ctl.rack_per_of_gp_rec = 90;
	return (0);
}

static int
rack_set_profile(struct tcp_rack *rack, int prof)
{
	int err = EINVAL;
	if (prof == 1) {
		/*
		 * Profile 1 is "standard" DGP. It ignores
		 * client buffer level.
		 */
		err = rack_set_dgp(rack);
		if (err)
			return (err);
	} else if (prof == 6) {
		err = rack_set_dgp(rack);
		if (err)
			return (err);
		/*
		 * Profile 6 tweaks DGP so that it will apply to
		 * fill-cw the same settings that profile5 does
		 * to replace DGP. It gets then the max(dgp-rate, fillcw(discounted).
		 */
		rack->rc_fillcw_apply_discount = 1;
	} else if (prof == 0) {
		/* This changes things back to the default settings */
		if (rack->rc_always_pace == 1) {
			rack_remove_pacing(rack);
		} else {
			/* Make sure any stray flags are off */
			rack->dgp_on = 0;
			rack->rc_hybrid_mode = 0;
			rack->use_fixed_rate = 0;
		}
		err = 0;
		if (rack_fill_cw_state)
			rack->rc_pace_to_cwnd = 1;
		else
			rack->rc_pace_to_cwnd = 0;

		if (rack_pace_every_seg && tcp_can_enable_pacing()) {
			rack->r_ctl.pacing_method |= RACK_REG_PACING;
			rack->rc_always_pace = 1;
			if (rack->rack_hibeta)
				rack_set_cc_pacing(rack);
		} else
			rack->rc_always_pace = 0;
		if (rack_dsack_std_based & 0x1) {
			/* Basically this means all rack timers are at least (srtt + 1/4 srtt) */
			rack->rc_rack_tmr_std_based = 1;
		}
		if (rack_dsack_std_based & 0x2) {
			/* Basically this means  rack timers are extended based on dsack by up to (2 * srtt) */
			rack->rc_rack_use_dsack = 1;
		}
		if (rack_use_cmp_acks)
			rack->r_use_cmp_ack = 1;
		else
			rack->r_use_cmp_ack = 0;
		if (rack_disable_prr)
			rack->rack_no_prr = 1;
		else
			rack->rack_no_prr = 0;
		if (rack_gp_no_rec_chg)
			rack->rc_gp_no_rec_chg = 1;
		else
			rack->rc_gp_no_rec_chg = 0;
		if (rack_enable_mqueue_for_nonpaced || rack->r_use_cmp_ack) {
			rack->r_mbuf_queue = 1;
			if (TCPS_HAVEESTABLISHED(rack->rc_tp->t_state))
				rack->rc_tp->t_flags2 |= TF2_MBUF_ACKCMP;
			rack->rc_tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
		} else {
			rack->r_mbuf_queue = 0;
			rack->rc_tp->t_flags2 &= ~TF2_SUPPORTS_MBUFQ;
		}
		if (rack_enable_shared_cwnd)
			rack->rack_enable_scwnd = 1;
		else
			rack->rack_enable_scwnd = 0;
		if (rack_do_dyn_mul) {
			/* When dynamic adjustment is on CA needs to start at 100% */
			rack->rc_gp_dyn_mul = 1;
			if (rack_do_dyn_mul >= 100)
				rack->r_ctl.rack_per_of_gp_ca = rack_do_dyn_mul;
		} else {
			rack->r_ctl.rack_per_of_gp_ca = rack_per_of_gp_ca;
			rack->rc_gp_dyn_mul = 0;
		}
		rack->r_rr_config = 0;
		rack->r_ctl.rc_no_push_at_mrtt = 0;
		rack->rc_pace_fill_if_rttin_range = 0;
		rack->rtt_limit_mul = 0;

		if (rack_enable_hw_pacing)
			rack->rack_hdw_pace_ena = 1;
		else
			rack->rack_hdw_pace_ena = 0;
		if (rack_disable_prr)
			rack->rack_no_prr = 1;
		else
			rack->rack_no_prr = 0;
		if (rack_limits_scwnd)
			rack->r_limit_scw  = 1;
		else
			rack->r_limit_scw  = 0;
		rack_init_retransmit_value(rack, rack_rxt_controls);
		err = 0;
	}
	return (err);
}

static int
rack_add_deferred_option(struct tcp_rack *rack, int sopt_name, uint64_t loptval)
{
	struct deferred_opt_list *dol;

	dol = malloc(sizeof(struct deferred_opt_list),
		     M_TCPDO, M_NOWAIT|M_ZERO);
	if (dol == NULL) {
		/*
		 * No space yikes -- fail out..
		 */
		return (0);
	}
	dol->optname = sopt_name;
	dol->optval = loptval;
	TAILQ_INSERT_TAIL(&rack->r_ctl.opt_list, dol, next);
	return (1);
}

static int
process_hybrid_pacing(struct tcp_rack *rack, struct tcp_hybrid_req *hybrid)
{
#ifdef TCP_REQUEST_TRK
	struct tcp_sendfile_track *sft;
	struct timeval tv;
	tcp_seq seq;
	int err;

	microuptime(&tv);

	/* Make sure no fixed rate is on */
	rack->use_fixed_rate = 0;
	rack->r_ctl.rc_fixed_pacing_rate_rec = 0;
	rack->r_ctl.rc_fixed_pacing_rate_ca = 0;
	rack->r_ctl.rc_fixed_pacing_rate_ss = 0;
	/* Now allocate or find our entry that will have these settings */
	sft = tcp_req_alloc_req_full(rack->rc_tp, &hybrid->req, tcp_tv_to_lusectick(&tv), 0);
	if (sft == NULL) {
		rack->rc_tp->tcp_hybrid_error++;
		/* no space, where would it have gone? */
		seq = rack->rc_tp->snd_una + rack->rc_tp->t_inpcb.inp_socket->so_snd.sb_ccc;
		rack_log_hybrid(rack, seq, NULL, HYBRID_LOG_NO_ROOM, __LINE__, 0);
		return (ENOSPC);
	}
	/* mask our internal flags */
	hybrid->hybrid_flags &= TCP_HYBRID_PACING_USER_MASK;
	/* The seq will be snd_una + everything in the buffer */
	seq = sft->start_seq;
	if ((hybrid->hybrid_flags & TCP_HYBRID_PACING_ENABLE) == 0) {
		/* Disabling hybrid pacing */
		if (rack->rc_hybrid_mode) {
			rack_set_profile(rack, 0);
			rack->rc_tp->tcp_hybrid_stop++;
		}
		rack_log_hybrid(rack, seq, sft, HYBRID_LOG_TURNED_OFF, __LINE__, 0);
		return (0);
	}
	if (rack->dgp_on == 0) {
		/*
		 * If we have not yet turned DGP on, do so
		 * now setting pure DGP mode, no buffer level
		 * response.
		 */
		if ((err = rack_set_profile(rack, 1)) != 0){
			/* Failed to turn pacing on */
			rack->rc_tp->tcp_hybrid_error++;
			rack_log_hybrid(rack, seq, sft, HYBRID_LOG_NO_PACING, __LINE__, 0);
			return (err);
		}
	}
	/*
	 * Now we must switch to hybrid mode as well which also
	 * means moving to regular pacing.
	 */
	if (rack->rc_hybrid_mode == 0) {
		/* First time */
		if (tcp_can_enable_pacing()) {
			rack->r_ctl.pacing_method |= RACK_REG_PACING;
			rack->rc_hybrid_mode = 1;
		} else {
			return (ENOSPC);
		}
		if (rack->r_ctl.pacing_method & RACK_DGP_PACING) {
			/*
			 * This should be true.
			 */
			tcp_dec_dgp_pacing_cnt();
			rack->r_ctl.pacing_method &= ~RACK_DGP_PACING;
		}
	}
	/* Now set in our flags */
	sft->hybrid_flags = hybrid->hybrid_flags | TCP_HYBRID_PACING_WASSET;
	if (hybrid->hybrid_flags & TCP_HYBRID_PACING_CSPR)
		sft->cspr = hybrid->cspr;
	else
		sft->cspr = 0;
	if (hybrid->hybrid_flags & TCP_HYBRID_PACING_H_MS)
		sft->hint_maxseg = hybrid->hint_maxseg;
	else
		sft->hint_maxseg = 0;
	rack->rc_tp->tcp_hybrid_start++;
	rack_log_hybrid(rack, seq, sft, HYBRID_LOG_RULES_SET, __LINE__,0);
	return (0);
#else
	return (ENOTSUP);
#endif
}

static int
rack_stack_information(struct tcpcb *tp, struct stack_specific_info *si)
{
	/*
	 * Gather rack specific information.
	 */
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	/* We pulled a SSI info log out what was there */
	policer_detection_log(rack, rack->rc_highly_buffered, 0, 0, 0, 20);
	if (rack->policer_detect_on) {
		si->policer_detection_enabled = 1;
		if (rack->rc_policer_detected) {
			si->policer_detected = 1;
			si->policer_bucket_size = rack->r_ctl.policer_bucket_size;
			si->policer_last_bw = rack->r_ctl.policer_bw;
		} else {
			si->policer_detected = 0;
			si->policer_bucket_size = 0;
			si->policer_last_bw = 0;
		}
		si->current_round = rack->r_ctl.current_round;
		si->highly_buffered = rack->rc_highly_buffered;
	}
	si->bytes_transmitted = tp->t_sndbytes;
	si->bytes_retransmitted = tp->t_snd_rxt_bytes;
	return (0);
}

static int
rack_process_option(struct tcpcb *tp, struct tcp_rack *rack, int sopt_name,
		    uint32_t optval, uint64_t loptval, struct tcp_hybrid_req *hybrid)

{
	struct epoch_tracker et;
	struct sockopt sopt;
	struct cc_newreno_opts opt;
	uint64_t val;
	int error = 0;
	uint16_t ca, ss;

	switch (sopt_name) {
	case TCP_RACK_SET_RXT_OPTIONS:
		if ((optval >= 0) && (optval <= 2)) {
			rack_init_retransmit_value(rack, optval);
		} else {
			/*
			 * You must send in 0, 1 or 2 all else is
			 * invalid.
			 */
			error = EINVAL;
		}
		break;
	case TCP_RACK_DSACK_OPT:
		RACK_OPTS_INC(tcp_rack_dsack_opt);
		if (optval & 0x1) {
			rack->rc_rack_tmr_std_based = 1;
		} else {
			rack->rc_rack_tmr_std_based = 0;
		}
		if (optval & 0x2) {
			rack->rc_rack_use_dsack = 1;
		} else {
			rack->rc_rack_use_dsack = 0;
		}
		rack_log_dsack_event(rack, 5, __LINE__, 0, 0);
		break;
	case TCP_RACK_PACING_DIVISOR:
		RACK_OPTS_INC(tcp_rack_pacing_divisor);
		if (optval == 0) {
			rack->r_ctl.pace_len_divisor = rack_default_pacing_divisor;
		} else {
			if (optval < RL_MIN_DIVISOR)
				rack->r_ctl.pace_len_divisor = RL_MIN_DIVISOR;
			else
				rack->r_ctl.pace_len_divisor = optval;
		}
		break;
	case TCP_RACK_HI_BETA:
		RACK_OPTS_INC(tcp_rack_hi_beta);
		if (optval > 0) {
			rack->rack_hibeta = 1;
			if ((optval >= 50) &&
			    (optval <= 100)) {
				/*
				 * User wants to set a custom beta.
				 */
				rack->r_ctl.saved_hibeta = optval;
				if (rack->rc_pacing_cc_set)
					rack_undo_cc_pacing(rack);
				rack->r_ctl.rc_saved_beta.beta = optval;
			}
			if (rack->rc_pacing_cc_set == 0)
				rack_set_cc_pacing(rack);
		} else {
			rack->rack_hibeta = 0;
			if (rack->rc_pacing_cc_set)
				rack_undo_cc_pacing(rack);
		}
		break;
	case TCP_RACK_PACING_BETA:
		error = EINVAL;
		break;
	case TCP_RACK_TIMER_SLOP:
		RACK_OPTS_INC(tcp_rack_timer_slop);
		rack->r_ctl.timer_slop = optval;
		if (rack->rc_tp->t_srtt) {
			/*
			 * If we have an SRTT lets update t_rxtcur
			 * to have the new slop.
			 */
			RACK_TCPT_RANGESET(tp->t_rxtcur, RACK_REXMTVAL(tp),
					   rack_rto_min, rack_rto_max,
					   rack->r_ctl.timer_slop);
		}
		break;
	case TCP_RACK_PACING_BETA_ECN:
		RACK_OPTS_INC(tcp_rack_beta_ecn);
		if (strcmp(tp->t_cc->name, CCALGONAME_NEWRENO) != 0) {
			/* This only works for newreno. */
			error = EINVAL;
			break;
		}
		if (rack->rc_pacing_cc_set) {
			/*
			 * Set them into the real CC module
			 * whats in the rack pcb is the old values
			 * to be used on restoral/
			 */
			sopt.sopt_dir = SOPT_SET;
			opt.name = CC_NEWRENO_BETA_ECN;
			opt.val = optval;
			if (CC_ALGO(tp)->ctl_output != NULL)
				error = CC_ALGO(tp)->ctl_output(&tp->t_ccv, &sopt, &opt);
			else
				error = ENOENT;
		} else {
			/*
			 * Not pacing yet so set it into our local
			 * rack pcb storage.
			 */
			rack->r_ctl.rc_saved_beta.beta_ecn = optval;
			rack->r_ctl.rc_saved_beta.newreno_flags = CC_NEWRENO_BETA_ECN_ENABLED;
		}
		break;
	case TCP_DEFER_OPTIONS:
		RACK_OPTS_INC(tcp_defer_opt);
		if (optval) {
			if (rack->gp_ready) {
				/* Too late */
				error = EINVAL;
				break;
			}
			rack->defer_options = 1;
		} else
			rack->defer_options = 0;
		break;
	case TCP_RACK_MEASURE_CNT:
		RACK_OPTS_INC(tcp_rack_measure_cnt);
		if (optval && (optval <= 0xff)) {
			rack->r_ctl.req_measurements = optval;
		} else
			error = EINVAL;
		break;
	case TCP_REC_ABC_VAL:
		RACK_OPTS_INC(tcp_rec_abc_val);
		if (optval > 0)
			rack->r_use_labc_for_rec = 1;
		else
			rack->r_use_labc_for_rec = 0;
		break;
	case TCP_RACK_ABC_VAL:
		RACK_OPTS_INC(tcp_rack_abc_val);
		if ((optval > 0) && (optval < 255))
			rack->rc_labc = optval;
		else
			error = EINVAL;
		break;
	case TCP_HDWR_UP_ONLY:
		RACK_OPTS_INC(tcp_pacing_up_only);
		if (optval)
			rack->r_up_only = 1;
		else
			rack->r_up_only = 0;
		break;
	case TCP_FILLCW_RATE_CAP:		/*  URL:fillcw_cap */
		RACK_OPTS_INC(tcp_fillcw_rate_cap);
		rack->r_ctl.fillcw_cap = loptval;
		break;
	case TCP_PACING_RATE_CAP:
		RACK_OPTS_INC(tcp_pacing_rate_cap);
		if ((rack->dgp_on == 1) &&
		    (rack->r_ctl.pacing_method & RACK_DGP_PACING)) {
			/*
			 * If we are doing DGP we need to switch
			 * to using the pacing limit.
			 */
			if (tcp_can_enable_pacing() == 0) {
				error = ENOSPC;
				break;
			}
			/*
			 * Now change up the flags and counts to be correct.
			 */
			rack->r_ctl.pacing_method |= RACK_REG_PACING;
			tcp_dec_dgp_pacing_cnt();
			rack->r_ctl.pacing_method &= ~RACK_DGP_PACING;
		}
		rack->r_ctl.bw_rate_cap = loptval;
		break;
	case TCP_HYBRID_PACING:
		if (hybrid == NULL) {
			error = EINVAL;
			break;
		}
		if (rack->r_ctl.side_chan_dis_mask & HYBRID_DIS_MASK) {
			error = EPERM;
			break;
		}
		error = process_hybrid_pacing(rack, hybrid);
		break;
	case TCP_SIDECHAN_DIS:			/*  URL:scodm */
		if (optval)
			rack->r_ctl.side_chan_dis_mask = optval;
		else
			rack->r_ctl.side_chan_dis_mask = 0;
		break;
	case TCP_RACK_PROFILE:
		RACK_OPTS_INC(tcp_profile);
		error = rack_set_profile(rack, optval);
		break;
	case TCP_USE_CMP_ACKS:
		RACK_OPTS_INC(tcp_use_cmp_acks);
		if ((optval == 0) && (tp->t_flags2 & TF2_MBUF_ACKCMP)) {
			/* You can't turn it off once its on! */
			error = EINVAL;
		} else if ((optval == 1) && (rack->r_use_cmp_ack == 0)) {
			rack->r_use_cmp_ack = 1;
			rack->r_mbuf_queue = 1;
			tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
		}
		if (rack->r_use_cmp_ack && TCPS_HAVEESTABLISHED(tp->t_state))
			tp->t_flags2 |= TF2_MBUF_ACKCMP;
		break;
	case TCP_SHARED_CWND_TIME_LIMIT:
		RACK_OPTS_INC(tcp_lscwnd);
		if (optval)
			rack->r_limit_scw = 1;
		else
			rack->r_limit_scw = 0;
		break;
	case TCP_RACK_DGP_IN_REC:
		error = EINVAL;
		break;
	case TCP_POLICER_DETECT:		/*  URL:pol_det */
		RACK_OPTS_INC(tcp_pol_detect);
		rack_translate_policer_detect(rack, optval);
		break;
	case TCP_POLICER_MSS:
		RACK_OPTS_INC(tcp_pol_mss);
		rack->r_ctl.policer_del_mss = (uint8_t)optval;
		if (optval & 0x00000100) {
			/*
			 * Value is setup like so:
			 * VVVV VVVV VVVV VVVV VVVV VVAI MMMM MMMM
			 * Where MMMM MMMM is MSS setting
			 * I (9th bit) is the Postive value that
			 * says it is being set (if its 0 then the
			 * upper bits 11 - 32 have no meaning.
			 * This allows setting it off with
			 * 0x000001MM.
			 *
			 * The 10th bit is used to turn on the
			 * alternate median (not the expanded one).
			 *
			 */
			rack->r_ctl.pol_bw_comp = (optval >> 10);
		}
		if (optval & 0x00000200) {
			rack->r_ctl.policer_alt_median = 1;
		} else {
			rack->r_ctl.policer_alt_median = 0;
		}
		break;
 	case TCP_RACK_PACE_TO_FILL:
		RACK_OPTS_INC(tcp_fillcw);
		if (optval == 0)
			rack->rc_pace_to_cwnd = 0;
		else {
			rack->rc_pace_to_cwnd = 1;
		}
		if ((optval >= rack_gp_rtt_maxmul) &&
		    rack_gp_rtt_maxmul &&
		    (optval < 0xf)) {
			rack->rc_pace_fill_if_rttin_range = 1;
			rack->rtt_limit_mul = optval;
		} else {
			rack->rc_pace_fill_if_rttin_range = 0;
			rack->rtt_limit_mul = 0;
		}
		break;
	case TCP_RACK_NO_PUSH_AT_MAX:
		RACK_OPTS_INC(tcp_npush);
		if (optval == 0)
			rack->r_ctl.rc_no_push_at_mrtt = 0;
		else if (optval < 0xff)
			rack->r_ctl.rc_no_push_at_mrtt = optval;
		else
			error = EINVAL;
		break;
	case TCP_SHARED_CWND_ENABLE:
		RACK_OPTS_INC(tcp_rack_scwnd);
		if (optval == 0)
			rack->rack_enable_scwnd = 0;
		else
			rack->rack_enable_scwnd = 1;
		break;
	case TCP_RACK_MBUF_QUEUE:
		/* Now do we use the LRO mbuf-queue feature */
		RACK_OPTS_INC(tcp_rack_mbufq);
		if (optval || rack->r_use_cmp_ack)
			rack->r_mbuf_queue = 1;
		else
			rack->r_mbuf_queue = 0;
		if  (rack->r_mbuf_queue || rack->rc_always_pace || rack->r_use_cmp_ack)
			tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
		else
			tp->t_flags2 &= ~TF2_SUPPORTS_MBUFQ;
		break;
	case TCP_RACK_NONRXT_CFG_RATE:
		RACK_OPTS_INC(tcp_rack_cfg_rate);
		if (optval == 0)
			rack->rack_rec_nonrxt_use_cr = 0;
		else
			rack->rack_rec_nonrxt_use_cr = 1;
		break;
	case TCP_NO_PRR:
		RACK_OPTS_INC(tcp_rack_noprr);
		if (optval == 0)
			rack->rack_no_prr = 0;
		else if (optval == 1)
			rack->rack_no_prr = 1;
		else if (optval == 2)
			rack->no_prr_addback = 1;
		else
			error = EINVAL;
		break;
	case RACK_CSPR_IS_FCC:			/*  URL:csprisfcc */
		if (optval > 0)
			rack->cspr_is_fcc = 1;
		else
			rack->cspr_is_fcc = 0;
		break;
	case TCP_TIMELY_DYN_ADJ:
		RACK_OPTS_INC(tcp_timely_dyn);
		if (optval == 0)
			rack->rc_gp_dyn_mul = 0;
		else {
			rack->rc_gp_dyn_mul = 1;
			if (optval >= 100) {
				/*
				 * If the user sets something 100 or more
				 * its the gp_ca value.
				 */
				rack->r_ctl.rack_per_of_gp_ca  = optval;
			}
		}
		break;
	case TCP_RACK_DO_DETECTION:
		RACK_OPTS_INC(tcp_rack_do_detection);
		if (optval == 0)
			rack->do_detection = 0;
		else
			rack->do_detection = 1;
		break;
	case TCP_RACK_TLP_USE:
		if ((optval < TLP_USE_ID) || (optval > TLP_USE_TWO_TWO)) {
			error = EINVAL;
			break;
		}
		RACK_OPTS_INC(tcp_tlp_use);
		rack->rack_tlp_threshold_use = optval;
		break;
	case TCP_RACK_TLP_REDUCE:
		/* RACK TLP cwnd reduction (bool) */
		RACK_OPTS_INC(tcp_rack_tlp_reduce);
		rack->r_ctl.rc_tlp_cwnd_reduce = optval;
		break;
		/*  Pacing related ones */
	case TCP_RACK_PACE_ALWAYS:
		/*
		 * zero is old rack method, 1 is new
		 * method using a pacing rate.
		 */
		RACK_OPTS_INC(tcp_rack_pace_always);
		if (rack->r_ctl.side_chan_dis_mask & CCSP_DIS_MASK) {
			error = EPERM;
			break;
		}
		if (optval > 0) {
			if (rack->rc_always_pace) {
				error = EALREADY;
				break;
			} else if (tcp_can_enable_pacing()) {
				rack->r_ctl.pacing_method |= RACK_REG_PACING;
				rack->rc_always_pace = 1;
				if (rack->rack_hibeta)
					rack_set_cc_pacing(rack);
			}
			else {
				error = ENOSPC;
				break;
			}
		} else {
			if (rack->rc_always_pace == 1) {
				rack_remove_pacing(rack);
			}
		}
		if  (rack->r_mbuf_queue || rack->rc_always_pace || rack->r_use_cmp_ack)
			tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
		else
			tp->t_flags2 &= ~TF2_SUPPORTS_MBUFQ;
		/* A rate may be set irate or other, if so set seg size */
		rack_update_seg(rack);
		break;
	case TCP_BBR_RACK_INIT_RATE:
		RACK_OPTS_INC(tcp_initial_rate);
		val = optval;
		/* Change from kbits per second to bytes per second */
		val *= 1000;
		val /= 8;
		rack->r_ctl.init_rate = val;
		if (rack->rc_always_pace)
			rack_update_seg(rack);
		break;
	case TCP_BBR_IWINTSO:
		error = EINVAL;
		break;
	case TCP_RACK_FORCE_MSEG:
		RACK_OPTS_INC(tcp_rack_force_max_seg);
		if (optval)
			rack->rc_force_max_seg = 1;
		else
			rack->rc_force_max_seg = 0;
		break;
	case TCP_RACK_PACE_MIN_SEG:
		RACK_OPTS_INC(tcp_rack_min_seg);
		rack->r_ctl.rc_user_set_min_segs = (0x0000ffff & optval);
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		break;
	case TCP_RACK_PACE_MAX_SEG:
		/* Max segments size in a pace in bytes */
		RACK_OPTS_INC(tcp_rack_max_seg);
		if ((rack->dgp_on == 1) &&
		    (rack->r_ctl.pacing_method & RACK_DGP_PACING)) {
			/*
			 * If we set a max-seg and are doing DGP then
			 * we now fall under the pacing limits not the
			 * DGP ones.
			 */
			if (tcp_can_enable_pacing() == 0) {
				error = ENOSPC;
				break;
			}
			/*
			 * Now change up the flags and counts to be correct.
			 */
			rack->r_ctl.pacing_method |= RACK_REG_PACING;
			tcp_dec_dgp_pacing_cnt();
			rack->r_ctl.pacing_method &= ~RACK_DGP_PACING;
		}
		if (optval <= MAX_USER_SET_SEG)
			rack->rc_user_set_max_segs = optval;
		else
			rack->rc_user_set_max_segs = MAX_USER_SET_SEG;
		rack_set_pace_segments(tp, rack, __LINE__, NULL);
		break;
	case TCP_RACK_PACE_RATE_REC:
		/* Set the fixed pacing rate in Bytes per second ca */
		RACK_OPTS_INC(tcp_rack_pace_rate_rec);
		if (rack->r_ctl.side_chan_dis_mask & CCSP_DIS_MASK) {
			error = EPERM;
			break;
		}
		if (rack->dgp_on) {
			/*
			 * We are already pacing another
			 * way.
			 */
			error = EBUSY;
			break;
		}
		rack->r_ctl.rc_fixed_pacing_rate_rec = optval;
		if (rack->r_ctl.rc_fixed_pacing_rate_ca == 0)
			rack->r_ctl.rc_fixed_pacing_rate_ca = optval;
		if (rack->r_ctl.rc_fixed_pacing_rate_ss == 0)
			rack->r_ctl.rc_fixed_pacing_rate_ss = optval;
		rack->use_fixed_rate = 1;
		if (rack->rack_hibeta)
			rack_set_cc_pacing(rack);
		rack_log_pacing_delay_calc(rack,
					   rack->r_ctl.rc_fixed_pacing_rate_ss,
					   rack->r_ctl.rc_fixed_pacing_rate_ca,
					   rack->r_ctl.rc_fixed_pacing_rate_rec, 0, 0, 8,
					   __LINE__, NULL,0);
		break;

	case TCP_RACK_PACE_RATE_SS:
		/* Set the fixed pacing rate in Bytes per second ca */
		RACK_OPTS_INC(tcp_rack_pace_rate_ss);
		if (rack->r_ctl.side_chan_dis_mask & CCSP_DIS_MASK) {
			error = EPERM;
			break;
		}
		if (rack->dgp_on) {
			/*
			 * We are already pacing another
			 * way.
			 */
			error = EBUSY;
			break;
		}
		rack->r_ctl.rc_fixed_pacing_rate_ss = optval;
		if (rack->r_ctl.rc_fixed_pacing_rate_ca == 0)
			rack->r_ctl.rc_fixed_pacing_rate_ca = optval;
		if (rack->r_ctl.rc_fixed_pacing_rate_rec == 0)
			rack->r_ctl.rc_fixed_pacing_rate_rec = optval;
		rack->use_fixed_rate = 1;
		if (rack->rack_hibeta)
			rack_set_cc_pacing(rack);
		rack_log_pacing_delay_calc(rack,
					   rack->r_ctl.rc_fixed_pacing_rate_ss,
					   rack->r_ctl.rc_fixed_pacing_rate_ca,
					   rack->r_ctl.rc_fixed_pacing_rate_rec, 0, 0, 8,
					   __LINE__, NULL, 0);
		break;

	case TCP_RACK_PACE_RATE_CA:
		/* Set the fixed pacing rate in Bytes per second ca */
		RACK_OPTS_INC(tcp_rack_pace_rate_ca);
		if (rack->r_ctl.side_chan_dis_mask & CCSP_DIS_MASK) {
			error = EPERM;
			break;
		}
		if (rack->dgp_on) {
			/*
			 * We are already pacing another
			 * way.
			 */
			error = EBUSY;
			break;
		}
		rack->r_ctl.rc_fixed_pacing_rate_ca = optval;
		if (rack->r_ctl.rc_fixed_pacing_rate_ss == 0)
			rack->r_ctl.rc_fixed_pacing_rate_ss = optval;
		if (rack->r_ctl.rc_fixed_pacing_rate_rec == 0)
			rack->r_ctl.rc_fixed_pacing_rate_rec = optval;
		rack->use_fixed_rate = 1;
		if (rack->rack_hibeta)
			rack_set_cc_pacing(rack);
		rack_log_pacing_delay_calc(rack,
					   rack->r_ctl.rc_fixed_pacing_rate_ss,
					   rack->r_ctl.rc_fixed_pacing_rate_ca,
					   rack->r_ctl.rc_fixed_pacing_rate_rec, 0, 0, 8,
					   __LINE__, NULL, 0);
		break;
	case TCP_RACK_GP_INCREASE_REC:
		RACK_OPTS_INC(tcp_gp_inc_rec);
		rack->r_ctl.rack_per_of_gp_rec = optval;
		rack_log_pacing_delay_calc(rack,
					   rack->r_ctl.rack_per_of_gp_ss,
					   rack->r_ctl.rack_per_of_gp_ca,
					   rack->r_ctl.rack_per_of_gp_rec, 0, 0, 1,
					   __LINE__, NULL, 0);
		break;
	case TCP_RACK_GP_INCREASE_CA:
		RACK_OPTS_INC(tcp_gp_inc_ca);
		ca = optval;
		if (ca < 100) {
			/*
			 * We don't allow any reduction
			 * over the GP b/w.
			 */
			error = EINVAL;
			break;
		}
		rack->r_ctl.rack_per_of_gp_ca = ca;
		rack_log_pacing_delay_calc(rack,
					   rack->r_ctl.rack_per_of_gp_ss,
					   rack->r_ctl.rack_per_of_gp_ca,
					   rack->r_ctl.rack_per_of_gp_rec, 0, 0, 1,
					   __LINE__, NULL, 0);
		break;
	case TCP_RACK_GP_INCREASE_SS:
		RACK_OPTS_INC(tcp_gp_inc_ss);
		ss = optval;
		if (ss < 100) {
			/*
			 * We don't allow any reduction
			 * over the GP b/w.
			 */
			error = EINVAL;
			break;
		}
		rack->r_ctl.rack_per_of_gp_ss = ss;
		rack_log_pacing_delay_calc(rack,
					   rack->r_ctl.rack_per_of_gp_ss,
					   rack->r_ctl.rack_per_of_gp_ca,
					   rack->r_ctl.rack_per_of_gp_rec, 0, 0, 1,
					   __LINE__, NULL, 0);
		break;
	case TCP_RACK_RR_CONF:
		RACK_OPTS_INC(tcp_rack_rrr_no_conf_rate);
		if (optval && optval <= 3)
			rack->r_rr_config = optval;
		else
			rack->r_rr_config = 0;
		break;
	case TCP_PACING_DND:			/*  URL:dnd */
		if (optval > 0)
			rack->rc_pace_dnd = 1;
		else
			rack->rc_pace_dnd = 0;
		break;
	case TCP_HDWR_RATE_CAP:
		RACK_OPTS_INC(tcp_hdwr_rate_cap);
		if (optval) {
			if (rack->r_rack_hw_rate_caps == 0)
				rack->r_rack_hw_rate_caps = 1;
			else
				error = EALREADY;
		} else {
			rack->r_rack_hw_rate_caps = 0;
		}
		break;
	case TCP_DGP_UPPER_BOUNDS:
	{
		uint8_t val;
		val = optval & 0x0000ff;
		rack->r_ctl.rack_per_upper_bound_ca = val;
		val = (optval >> 16) & 0x0000ff;
		rack->r_ctl.rack_per_upper_bound_ss = val;
		break;
	}
	case TCP_SS_EEXIT:			/*  URL:eexit */
		if (optval > 0) {
			rack->r_ctl.gp_rnd_thresh =  optval & 0x0ff;
			if (optval & 0x10000) {
				rack->r_ctl.gate_to_fs = 1;
			} else {
				rack->r_ctl.gate_to_fs = 0;
			}
			if (optval & 0x20000) {
				rack->r_ctl.use_gp_not_last = 1;
			} else {
				rack->r_ctl.use_gp_not_last = 0;
			}
			if (optval & 0xfffc0000) {
				uint32_t v;

				v = (optval >> 18) & 0x00003fff;
				if (v >= 1000)
					rack->r_ctl.gp_gain_req = v;
			}
		} else {
			/* We do not do ss early exit at all */
			rack->rc_initial_ss_comp = 1;
			rack->r_ctl.gp_rnd_thresh = 0;
		}
		break;
	case TCP_RACK_SPLIT_LIMIT:
		RACK_OPTS_INC(tcp_split_limit);
		rack->r_ctl.rc_split_limit = optval;
		break;
	case TCP_BBR_HDWR_PACE:
		RACK_OPTS_INC(tcp_hdwr_pacing);
		if (optval){
			if (rack->rack_hdrw_pacing == 0) {
				rack->rack_hdw_pace_ena = 1;
				rack->rack_attempt_hdwr_pace = 0;
			} else
				error = EALREADY;
		} else {
			rack->rack_hdw_pace_ena = 0;
#ifdef RATELIMIT
			if (rack->r_ctl.crte != NULL) {
				rack->rack_hdrw_pacing = 0;
				rack->rack_attempt_hdwr_pace = 0;
				tcp_rel_pacing_rate(rack->r_ctl.crte, tp);
				rack->r_ctl.crte = NULL;
			}
#endif
		}
		break;
		/*  End Pacing related ones */
	case TCP_RACK_PRR_SENDALOT:
		/* Allow PRR to send more than one seg */
		RACK_OPTS_INC(tcp_rack_prr_sendalot);
		rack->r_ctl.rc_prr_sendalot = optval;
		break;
	case TCP_RACK_MIN_TO:
		/* Minimum time between rack t-o's in ms */
		RACK_OPTS_INC(tcp_rack_min_to);
		rack->r_ctl.rc_min_to = optval;
		break;
	case TCP_RACK_EARLY_SEG:
		/* If early recovery max segments */
		RACK_OPTS_INC(tcp_rack_early_seg);
		rack->r_ctl.rc_early_recovery_segs = optval;
		break;
	case TCP_RACK_ENABLE_HYSTART:
	{
		if (optval) {
			tp->t_ccv.flags |= CCF_HYSTART_ALLOWED;
			if (rack_do_hystart > RACK_HYSTART_ON)
				tp->t_ccv.flags |= CCF_HYSTART_CAN_SH_CWND;
			if (rack_do_hystart > RACK_HYSTART_ON_W_SC)
				tp->t_ccv.flags |= CCF_HYSTART_CONS_SSTH;
		} else {
			tp->t_ccv.flags &= ~(CCF_HYSTART_ALLOWED|CCF_HYSTART_CAN_SH_CWND|CCF_HYSTART_CONS_SSTH);
		}
	}
	break;
	case TCP_RACK_REORD_THRESH:
		/* RACK reorder threshold (shift amount) */
		RACK_OPTS_INC(tcp_rack_reord_thresh);
		if ((optval > 0) && (optval < 31))
			rack->r_ctl.rc_reorder_shift = optval;
		else
			error = EINVAL;
		break;
	case TCP_RACK_REORD_FADE:
		/* Does reordering fade after ms time */
		RACK_OPTS_INC(tcp_rack_reord_fade);
		rack->r_ctl.rc_reorder_fade = optval;
		break;
	case TCP_RACK_TLP_THRESH:
		/* RACK TLP theshold i.e. srtt+(srtt/N) */
		RACK_OPTS_INC(tcp_rack_tlp_thresh);
		if (optval)
			rack->r_ctl.rc_tlp_threshold = optval;
		else
			error = EINVAL;
		break;
	case TCP_BBR_USE_RACK_RR:
		RACK_OPTS_INC(tcp_rack_rr);
		if (optval)
			rack->use_rack_rr = 1;
		else
			rack->use_rack_rr = 0;
		break;
	case TCP_RACK_PKT_DELAY:
		/* RACK added ms i.e. rack-rtt + reord + N */
		RACK_OPTS_INC(tcp_rack_pkt_delay);
		rack->r_ctl.rc_pkt_delay = optval;
		break;
	case TCP_DELACK:
		RACK_OPTS_INC(tcp_rack_delayed_ack);
		if (optval == 0)
			tp->t_delayed_ack = 0;
		else
			tp->t_delayed_ack = 1;
		if (tp->t_flags & TF_DELACK) {
			tp->t_flags &= ~TF_DELACK;
			tp->t_flags |= TF_ACKNOW;
			NET_EPOCH_ENTER(et);
			rack_output(tp);
			NET_EPOCH_EXIT(et);
		}
		break;

	case TCP_BBR_RACK_RTT_USE:
		RACK_OPTS_INC(tcp_rack_rtt_use);
		if ((optval != USE_RTT_HIGH) &&
		    (optval != USE_RTT_LOW) &&
		    (optval != USE_RTT_AVG))
			error = EINVAL;
		else
			rack->r_ctl.rc_rate_sample_method = optval;
		break;
	case TCP_HONOR_HPTS_MIN:
		RACK_OPTS_INC(tcp_honor_hpts);
		if (optval) {
			rack->r_use_hpts_min = 1;
			/*
			 * Must be between 2 - 80% to be a reduction else
			 * we keep the default (10%).
			 */
			if ((optval > 1) && (optval <= 80)) {
				rack->r_ctl.max_reduction = optval;
			}
		} else
			rack->r_use_hpts_min = 0;
		break;
	case TCP_REC_IS_DYN:			/*  URL:dynrec */
		RACK_OPTS_INC(tcp_dyn_rec);
		if (optval)
			rack->rc_gp_no_rec_chg = 1;
		else
			rack->rc_gp_no_rec_chg = 0;
		break;
	case TCP_NO_TIMELY:
		RACK_OPTS_INC(tcp_notimely);
		if (optval) {
			rack->rc_skip_timely = 1;
			rack->r_ctl.rack_per_of_gp_rec = 90;
			rack->r_ctl.rack_per_of_gp_ca = 100;
			rack->r_ctl.rack_per_of_gp_ss = 250;
		} else {
			rack->rc_skip_timely = 0;
		}
		break;
	case TCP_GP_USE_LTBW:
		if (optval == 0) {
			rack->use_lesser_lt_bw = 0;
			rack->dis_lt_bw = 1;
		} else if (optval == 1) {
			rack->use_lesser_lt_bw = 1;
			rack->dis_lt_bw = 0;
		} else if (optval == 2) {
			rack->use_lesser_lt_bw = 0;
			rack->dis_lt_bw = 0;
		}
		break;
	case TCP_DATA_AFTER_CLOSE:
		RACK_OPTS_INC(tcp_data_after_close);
		if (optval)
			rack->rc_allow_data_af_clo = 1;
		else
			rack->rc_allow_data_af_clo = 0;
		break;
	default:
		break;
	}
	tcp_log_socket_option(tp, sopt_name, optval, error);
	return (error);
}

static void
rack_inherit(struct tcpcb *tp, struct inpcb *parent)
{
	/*
	 * A new connection has been created (tp) and
	 * the parent is the inpcb given. We want to
	 * apply a read-lock to the parent (we are already
	 * holding a write lock on the tp) and copy anything
	 * out of the rack specific data as long as its tfb is
	 * the same as ours i.e. we are the same stack. Otherwise
	 * we just return.
	 */
	struct tcpcb *par;
	struct tcp_rack *dest, *src;
	int cnt = 0;

	par = intotcpcb(parent);
	if (par->t_fb != tp->t_fb) {
		/* Not the same stack */
		tcp_log_socket_option(tp, 0, 0, 1);
		return;
	}
	/* Ok if we reach here lets setup the two rack pointers */
	dest = (struct tcp_rack *)tp->t_fb_ptr;
	src = (struct tcp_rack *)par->t_fb_ptr;
	if ((src == NULL) || (dest == NULL)) {
		/* Huh? */
		tcp_log_socket_option(tp, 0, 0, 2);
		return;
	}
	/* Now copy out anything we wish to inherit i.e. things in socket-options */
	/* TCP_RACK_PROFILE we can't know but we can set DGP if its on */
	if ((src->dgp_on) && (dest->dgp_on == 0)) {
		/* Profile 1 had to be set via sock opt */
		rack_set_dgp(dest);
		cnt++;
	}
	/* TCP_RACK_SET_RXT_OPTIONS */
	if (dest->full_size_rxt != src->full_size_rxt) {
		dest->full_size_rxt = src->full_size_rxt;
		cnt++;
	}
	if (dest->shape_rxt_to_pacing_min  != src->shape_rxt_to_pacing_min) {
		dest->shape_rxt_to_pacing_min = src->shape_rxt_to_pacing_min;
		cnt++;
	}
	/* TCP_RACK_DSACK_OPT */
	if (dest->rc_rack_tmr_std_based != src->rc_rack_tmr_std_based) {
		dest->rc_rack_tmr_std_based = src->rc_rack_tmr_std_based;
		cnt++;
	}
	if (dest->rc_rack_use_dsack != src->rc_rack_use_dsack) {
		dest->rc_rack_use_dsack = src->rc_rack_use_dsack;
		cnt++;
	}
	/* TCP_RACK_PACING_DIVISOR */
	if (dest->r_ctl.pace_len_divisor != src->r_ctl.pace_len_divisor) {
		dest->r_ctl.pace_len_divisor = src->r_ctl.pace_len_divisor;
		cnt++;
	}
	/* TCP_RACK_HI_BETA */
	if (src->rack_hibeta != dest->rack_hibeta) {
		cnt++;
		if (src->rack_hibeta) {
			dest->r_ctl.rc_saved_beta.beta = src->r_ctl.rc_saved_beta.beta;
			dest->rack_hibeta = 1;
		} else {
			dest->rack_hibeta = 0;
		}
	}
	/* TCP_RACK_TIMER_SLOP */
	if (dest->r_ctl.timer_slop != src->r_ctl.timer_slop) {
		dest->r_ctl.timer_slop = src->r_ctl.timer_slop;
		cnt++;
	}
	/* TCP_RACK_PACING_BETA_ECN */
	if (dest->r_ctl.rc_saved_beta.beta_ecn != src->r_ctl.rc_saved_beta.beta_ecn) {
		dest->r_ctl.rc_saved_beta.beta_ecn = src->r_ctl.rc_saved_beta.beta_ecn;
		cnt++;
	}
	if (dest->r_ctl.rc_saved_beta.newreno_flags != src->r_ctl.rc_saved_beta.newreno_flags) {
		dest->r_ctl.rc_saved_beta.newreno_flags = src->r_ctl.rc_saved_beta.newreno_flags;
		cnt++;
	}
	/* We do not do TCP_DEFER_OPTIONS */
	/* TCP_RACK_MEASURE_CNT */
	if (dest->r_ctl.req_measurements != src->r_ctl.req_measurements) {
		dest->r_ctl.req_measurements = src->r_ctl.req_measurements;
		cnt++;
	}
	/* TCP_HDWR_UP_ONLY */
	if (dest->r_up_only != src->r_up_only) {
		dest->r_up_only = src->r_up_only;
		cnt++;
	}
	/* TCP_FILLCW_RATE_CAP */
	if (dest->r_ctl.fillcw_cap != src->r_ctl.fillcw_cap) {
		dest->r_ctl.fillcw_cap = src->r_ctl.fillcw_cap;
		cnt++;
	}
	/* TCP_PACING_RATE_CAP */
	if (dest->r_ctl.bw_rate_cap != src->r_ctl.bw_rate_cap) {
		dest->r_ctl.bw_rate_cap = src->r_ctl.bw_rate_cap;
		cnt++;
	}
	/* A listener can't set TCP_HYBRID_PACING */
	/* TCP_SIDECHAN_DIS */
	if (dest->r_ctl.side_chan_dis_mask != src->r_ctl.side_chan_dis_mask) {
		dest->r_ctl.side_chan_dis_mask = src->r_ctl.side_chan_dis_mask;
		cnt++;
	}
	/* TCP_SHARED_CWND_TIME_LIMIT */
	if (dest->r_limit_scw != src->r_limit_scw) {
		dest->r_limit_scw = src->r_limit_scw;
		cnt++;
	}
	/* TCP_POLICER_DETECT */
	if (dest->r_ctl.policer_rxt_threshold != src->r_ctl.policer_rxt_threshold) {
		dest->r_ctl.policer_rxt_threshold = src->r_ctl.policer_rxt_threshold;
		cnt++;
	}
	if (dest->r_ctl.policer_avg_threshold != src->r_ctl.policer_avg_threshold) {
		dest->r_ctl.policer_avg_threshold = src->r_ctl.policer_avg_threshold;
		cnt++;
	}
	if (dest->r_ctl.policer_med_threshold != src->r_ctl.policer_med_threshold) {
		dest->r_ctl.policer_med_threshold = src->r_ctl.policer_med_threshold;
		cnt++;
	}
	if (dest->policer_detect_on != src->policer_detect_on) {
		dest->policer_detect_on = src->policer_detect_on;
		cnt++;
	}

	if (dest->r_ctl.saved_policer_val != src->r_ctl.saved_policer_val) {
		dest->r_ctl.saved_policer_val = src->r_ctl.saved_policer_val;
		cnt++;
	}
	/* TCP_POLICER_MSS */
	if (dest->r_ctl.policer_del_mss != src->r_ctl.policer_del_mss) {
		dest->r_ctl.policer_del_mss = src->r_ctl.policer_del_mss;
		cnt++;
	}

	if (dest->r_ctl.pol_bw_comp != src->r_ctl.pol_bw_comp) {
		dest->r_ctl.pol_bw_comp = src->r_ctl.pol_bw_comp;
		cnt++;
	}

	if (dest->r_ctl.policer_alt_median != src->r_ctl.policer_alt_median) {
		dest->r_ctl.policer_alt_median = src->r_ctl.policer_alt_median;
		cnt++;
	}
	/* TCP_RACK_PACE_TO_FILL */
	if (dest->rc_pace_to_cwnd != src->rc_pace_to_cwnd) {
		dest->rc_pace_to_cwnd = src->rc_pace_to_cwnd;
		cnt++;
	}
	if (dest->rc_pace_fill_if_rttin_range != src->rc_pace_fill_if_rttin_range) {
		dest->rc_pace_fill_if_rttin_range = src->rc_pace_fill_if_rttin_range;
		cnt++;
	}
	if (dest->rtt_limit_mul != src->rtt_limit_mul) {
		dest->rtt_limit_mul = src->rtt_limit_mul;
		cnt++;
	}
	/* TCP_RACK_NO_PUSH_AT_MAX */
	if (dest->r_ctl.rc_no_push_at_mrtt != src->r_ctl.rc_no_push_at_mrtt) {
		dest->r_ctl.rc_no_push_at_mrtt = src->r_ctl.rc_no_push_at_mrtt;
		cnt++;
	}
	/* TCP_SHARED_CWND_ENABLE */
	if (dest->rack_enable_scwnd != src->rack_enable_scwnd) {
		dest->rack_enable_scwnd = src->rack_enable_scwnd;
		cnt++;
	}
	/* TCP_USE_CMP_ACKS */
	if (dest->r_use_cmp_ack != src->r_use_cmp_ack) {
		dest->r_use_cmp_ack = src->r_use_cmp_ack;
		cnt++;
	}

	if (dest->r_mbuf_queue != src->r_mbuf_queue) {
		dest->r_mbuf_queue = src->r_mbuf_queue;
		cnt++;
	}
	/* TCP_RACK_MBUF_QUEUE */
	if (dest->r_mbuf_queue != src->r_mbuf_queue) {
		dest->r_mbuf_queue = src->r_mbuf_queue;
		cnt++;
	}
	if  (dest->r_mbuf_queue || dest->rc_always_pace || dest->r_use_cmp_ack) {
		tp->t_flags2 |= TF2_SUPPORTS_MBUFQ;
	} else {
		tp->t_flags2 &= ~TF2_SUPPORTS_MBUFQ;
	}
	if (dest->r_use_cmp_ack && TCPS_HAVEESTABLISHED(tp->t_state)) {
		tp->t_flags2 |= TF2_MBUF_ACKCMP;
	}
	/* TCP_RACK_NONRXT_CFG_RATE */
	if (dest->rack_rec_nonrxt_use_cr != src->rack_rec_nonrxt_use_cr) {
		dest->rack_rec_nonrxt_use_cr = src->rack_rec_nonrxt_use_cr;
		cnt++;
	}
	/* TCP_NO_PRR */
	if (dest->rack_no_prr != src->rack_no_prr) {
		dest->rack_no_prr = src->rack_no_prr;
		cnt++;
	}
	if (dest->no_prr_addback != src->no_prr_addback) {
		dest->no_prr_addback = src->no_prr_addback;
		cnt++;
	}
	/* RACK_CSPR_IS_FCC */
	if (dest->cspr_is_fcc != src->cspr_is_fcc) {
		dest->cspr_is_fcc = src->cspr_is_fcc;
		cnt++;
	}
	/* TCP_TIMELY_DYN_ADJ */
	if (dest->rc_gp_dyn_mul != src->rc_gp_dyn_mul) {
		dest->rc_gp_dyn_mul = src->rc_gp_dyn_mul;
		cnt++;
	}
	if (dest->r_ctl.rack_per_of_gp_ca != src->r_ctl.rack_per_of_gp_ca) {
		dest->r_ctl.rack_per_of_gp_ca = src->r_ctl.rack_per_of_gp_ca;
		cnt++;
	}
	/* TCP_RACK_DO_DETECTION */
	if (dest->do_detection != src->do_detection) {
		dest->do_detection = src->do_detection;
		cnt++;
	}
	/* TCP_RACK_TLP_USE */
	if (dest->rack_tlp_threshold_use != src->rack_tlp_threshold_use) {
		dest->rack_tlp_threshold_use = src->rack_tlp_threshold_use;
		cnt++;
	}
	/* we don't allow inheritence of TCP_RACK_PACE_ALWAYS */
	/* TCP_BBR_RACK_INIT_RATE */
	if (dest->r_ctl.init_rate != src->r_ctl.init_rate) {
		dest->r_ctl.init_rate = src->r_ctl.init_rate;
		cnt++;
	}
	/* TCP_RACK_FORCE_MSEG */
	if (dest->rc_force_max_seg != src->rc_force_max_seg) {
		dest->rc_force_max_seg = src->rc_force_max_seg;
		cnt++;
	}
	/* TCP_RACK_PACE_MIN_SEG */
	if (dest->r_ctl.rc_user_set_min_segs != src->r_ctl.rc_user_set_min_segs) {
		dest->r_ctl.rc_user_set_min_segs = src->r_ctl.rc_user_set_min_segs;
		cnt++;
	}
	/* we don't allow TCP_RACK_PACE_MAX_SEG */
	/* TCP_RACK_PACE_RATE_REC, TCP_RACK_PACE_RATE_SS,  TCP_RACK_PACE_RATE_CA */
	if (dest->r_ctl.rc_fixed_pacing_rate_ca != src->r_ctl.rc_fixed_pacing_rate_ca) {
		dest->r_ctl.rc_fixed_pacing_rate_ca = src->r_ctl.rc_fixed_pacing_rate_ca;
		cnt++;
	}
	if (dest->r_ctl.rc_fixed_pacing_rate_ss != src->r_ctl.rc_fixed_pacing_rate_ss) {
		dest->r_ctl.rc_fixed_pacing_rate_ss = src->r_ctl.rc_fixed_pacing_rate_ss;
		cnt++;
	}
	if (dest->r_ctl.rc_fixed_pacing_rate_rec != src->r_ctl.rc_fixed_pacing_rate_rec) {
		dest->r_ctl.rc_fixed_pacing_rate_rec = src->r_ctl.rc_fixed_pacing_rate_rec;
		cnt++;
	}
	/* TCP_RACK_GP_INCREASE_REC, TCP_RACK_GP_INCREASE_CA, TCP_RACK_GP_INCREASE_SS */
	if (dest->r_ctl.rack_per_of_gp_rec != src->r_ctl.rack_per_of_gp_rec) {
		dest->r_ctl.rack_per_of_gp_rec = src->r_ctl.rack_per_of_gp_rec;
		cnt++;
	}
	if (dest->r_ctl.rack_per_of_gp_ca != src->r_ctl.rack_per_of_gp_ca) {
		dest->r_ctl.rack_per_of_gp_ca = src->r_ctl.rack_per_of_gp_ca;
		cnt++;
	}

	if (dest->r_ctl.rack_per_of_gp_ss != src->r_ctl.rack_per_of_gp_ss) {
		dest->r_ctl.rack_per_of_gp_ss = src->r_ctl.rack_per_of_gp_ss;
		cnt++;
	}
	/* TCP_RACK_RR_CONF */
	if (dest->r_rr_config != src->r_rr_config) {
		dest->r_rr_config = src->r_rr_config;
		cnt++;
	}
	/* TCP_PACING_DND */
	if (dest->rc_pace_dnd != src->rc_pace_dnd) {
		dest->rc_pace_dnd = src->rc_pace_dnd;
		cnt++;
	}
	/* TCP_HDWR_RATE_CAP */
	if (dest->r_rack_hw_rate_caps != src->r_rack_hw_rate_caps) {
		dest->r_rack_hw_rate_caps = src->r_rack_hw_rate_caps;
		cnt++;
	}
	/* TCP_DGP_UPPER_BOUNDS */
	if (dest->r_ctl.rack_per_upper_bound_ca != src->r_ctl.rack_per_upper_bound_ca) {
		dest->r_ctl.rack_per_upper_bound_ca = src->r_ctl.rack_per_upper_bound_ca;
		cnt++;
	}
	if (dest->r_ctl.rack_per_upper_bound_ss != src->r_ctl.rack_per_upper_bound_ss) {
		dest->r_ctl.rack_per_upper_bound_ss = src->r_ctl.rack_per_upper_bound_ss;
		cnt++;
	}
	/* TCP_SS_EEXIT */
	if (dest->r_ctl.gp_rnd_thresh != src->r_ctl.gp_rnd_thresh) {
		dest->r_ctl.gp_rnd_thresh = src->r_ctl.gp_rnd_thresh;
		cnt++;
	}
	if (dest->r_ctl.gate_to_fs != src->r_ctl.gate_to_fs) {
		dest->r_ctl.gate_to_fs = src->r_ctl.gate_to_fs;
		cnt++;
	}
	if (dest->r_ctl.use_gp_not_last != src->r_ctl.use_gp_not_last) {
		dest->r_ctl.use_gp_not_last = src->r_ctl.use_gp_not_last;
		cnt++;
	}
	if (dest->r_ctl.gp_gain_req != src->r_ctl.gp_gain_req) {
		dest->r_ctl.gp_gain_req = src->r_ctl.gp_gain_req;
		cnt++;
	}
	/* TCP_BBR_HDWR_PACE */
	if (dest->rack_hdw_pace_ena != src->rack_hdw_pace_ena) {
		dest->rack_hdw_pace_ena = src->rack_hdw_pace_ena;
		cnt++;
	}
	if (dest->rack_attempt_hdwr_pace != src->rack_attempt_hdwr_pace) {
		dest->rack_attempt_hdwr_pace = src->rack_attempt_hdwr_pace;
		cnt++;
	}
	/* TCP_RACK_PRR_SENDALOT */
	if (dest->r_ctl.rc_prr_sendalot != src->r_ctl.rc_prr_sendalot) {
		dest->r_ctl.rc_prr_sendalot = src->r_ctl.rc_prr_sendalot;
		cnt++;
	}
	/* TCP_RACK_MIN_TO */
	if (dest->r_ctl.rc_min_to != src->r_ctl.rc_min_to) {
		dest->r_ctl.rc_min_to = src->r_ctl.rc_min_to;
		cnt++;
	}
	/* TCP_RACK_EARLY_SEG */
	if (dest->r_ctl.rc_early_recovery_segs != src->r_ctl.rc_early_recovery_segs) {
		dest->r_ctl.rc_early_recovery_segs = src->r_ctl.rc_early_recovery_segs;
		cnt++;
	}
	/* TCP_RACK_ENABLE_HYSTART */
	if (par->t_ccv.flags != tp->t_ccv.flags) {
		cnt++;
		if (par->t_ccv.flags & CCF_HYSTART_ALLOWED) {
			tp->t_ccv.flags |= CCF_HYSTART_ALLOWED;
			if (rack_do_hystart > RACK_HYSTART_ON)
				tp->t_ccv.flags |= CCF_HYSTART_CAN_SH_CWND;
			if (rack_do_hystart > RACK_HYSTART_ON_W_SC)
				tp->t_ccv.flags |= CCF_HYSTART_CONS_SSTH;
		} else {
			tp->t_ccv.flags &= ~(CCF_HYSTART_ALLOWED|CCF_HYSTART_CAN_SH_CWND|CCF_HYSTART_CONS_SSTH);
		}
	}
	/* TCP_RACK_REORD_THRESH */
	if (dest->r_ctl.rc_reorder_shift != src->r_ctl.rc_reorder_shift) {
		dest->r_ctl.rc_reorder_shift = src->r_ctl.rc_reorder_shift;
		cnt++;
	}
	/* TCP_RACK_REORD_FADE */
	if (dest->r_ctl.rc_reorder_fade != src->r_ctl.rc_reorder_fade) {
		dest->r_ctl.rc_reorder_fade = src->r_ctl.rc_reorder_fade;
		cnt++;
	}
	/* TCP_RACK_TLP_THRESH */
	if (dest->r_ctl.rc_tlp_threshold != src->r_ctl.rc_tlp_threshold) {
		dest->r_ctl.rc_tlp_threshold = src->r_ctl.rc_tlp_threshold;
		cnt++;
	}
	/* TCP_BBR_USE_RACK_RR */
	if (dest->use_rack_rr != src->use_rack_rr) {
		dest->use_rack_rr = src->use_rack_rr;
		cnt++;
	}
	/* TCP_RACK_PKT_DELAY */
	if (dest->r_ctl.rc_pkt_delay != src->r_ctl.rc_pkt_delay) {
		dest->r_ctl.rc_pkt_delay = src->r_ctl.rc_pkt_delay;
		cnt++;
	}
	/* TCP_DELACK will get copied via the main code if applicable */
	/* TCP_BBR_RACK_RTT_USE */
	if (dest->r_ctl.rc_rate_sample_method != src->r_ctl.rc_rate_sample_method) {
		dest->r_ctl.rc_rate_sample_method = src->r_ctl.rc_rate_sample_method;
		cnt++;
	}
	/* TCP_HONOR_HPTS_MIN */
	if (dest->r_use_hpts_min != src->r_use_hpts_min) {
		dest->r_use_hpts_min = src->r_use_hpts_min;
		cnt++;
	}
	if (dest->r_ctl.max_reduction != src->r_ctl.max_reduction) {
		dest->r_ctl.max_reduction = src->r_ctl.max_reduction;
		cnt++;
	}
	/* TCP_REC_IS_DYN */
	if (dest->rc_gp_no_rec_chg != src->rc_gp_no_rec_chg) {
		dest->rc_gp_no_rec_chg = src->rc_gp_no_rec_chg;
		cnt++;
	}
	if (dest->rc_skip_timely != src->rc_skip_timely) {
		dest->rc_skip_timely = src->rc_skip_timely;
		cnt++;
	}
	/* TCP_DATA_AFTER_CLOSE */
	if (dest->rc_allow_data_af_clo != src->rc_allow_data_af_clo) {
		dest->rc_allow_data_af_clo = src->rc_allow_data_af_clo;
		cnt++;
	}
	/* TCP_GP_USE_LTBW */
	if (src->use_lesser_lt_bw != dest->use_lesser_lt_bw) {
		dest->use_lesser_lt_bw = src->use_lesser_lt_bw;
		cnt++;
	}
	if (dest->dis_lt_bw != src->dis_lt_bw) {
		dest->dis_lt_bw = src->dis_lt_bw;
		cnt++;
	}
	tcp_log_socket_option(tp, 0, cnt, 0);
}


static void
rack_apply_deferred_options(struct tcp_rack *rack)
{
	struct deferred_opt_list *dol, *sdol;
	uint32_t s_optval;

	TAILQ_FOREACH_SAFE(dol, &rack->r_ctl.opt_list, next, sdol) {
		TAILQ_REMOVE(&rack->r_ctl.opt_list, dol, next);
		/* Disadvantage of deferal is you loose the error return */
		s_optval = (uint32_t)dol->optval;
		(void)rack_process_option(rack->rc_tp, rack, dol->optname, s_optval, dol->optval, NULL);
		free(dol, M_TCPDO);
	}
}

static void
rack_hw_tls_change(struct tcpcb *tp, int chg)
{
	/* Update HW tls state */
	struct tcp_rack *rack;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (chg)
		rack->r_ctl.fsb.hw_tls = 1;
	else
		rack->r_ctl.fsb.hw_tls = 0;
}

static int
rack_pru_options(struct tcpcb *tp, int flags)
{
	if (flags & PRUS_OOB)
		return (EOPNOTSUPP);
	return (0);
}

static bool
rack_wake_check(struct tcpcb *tp)
{
	struct tcp_rack *rack;
	struct timeval tv;
	uint32_t cts;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack->r_ctl.rc_hpts_flags) {
		cts = tcp_get_usecs(&tv);
		if ((rack->r_ctl.rc_hpts_flags & PACE_PKT_OUTPUT) == PACE_PKT_OUTPUT){
			/*
			 * Pacing timer is up, check if we are ready.
			 */
			if (TSTMP_GEQ(cts, rack->r_ctl.rc_last_output_to))
				return (true);
		} else if ((rack->r_ctl.rc_hpts_flags & PACE_TMR_MASK) != 0) {
			/*
			 * A timer is up, check if we are ready.
			 */
			if (TSTMP_GEQ(cts, rack->r_ctl.rc_timer_exp))
				return (true);
		}
	}
	return (false);
}

static struct tcp_function_block __tcp_rack = {
	.tfb_tcp_block_name = __XSTRING(STACKNAME),
	.tfb_tcp_output = rack_output,
	.tfb_do_queued_segments = ctf_do_queued_segments,
	.tfb_do_segment_nounlock = rack_do_segment_nounlock,
	.tfb_tcp_do_segment = rack_do_segment,
	.tfb_tcp_ctloutput = rack_ctloutput,
	.tfb_tcp_fb_init = rack_init,
	.tfb_tcp_fb_fini = rack_fini,
	.tfb_tcp_timer_stop_all = rack_stopall,
	.tfb_tcp_rexmit_tmr = rack_remxt_tmr,
	.tfb_tcp_handoff_ok = rack_handoff_ok,
	.tfb_tcp_mtu_chg = rack_mtu_change,
	.tfb_pru_options = rack_pru_options,
	.tfb_hwtls_change = rack_hw_tls_change,
	.tfb_chg_query = rack_chg_query,
	.tfb_switch_failed = rack_switch_failed,
	.tfb_early_wake_check = rack_wake_check,
	.tfb_compute_pipe = rack_compute_pipe,
	.tfb_stack_info = rack_stack_information,
	.tfb_inherit = rack_inherit,
	.tfb_flags = TCP_FUNC_OUTPUT_CANDROP,

};

/*
 * rack_ctloutput() must drop the inpcb lock before performing copyin on
 * socket option arguments.  When it re-acquires the lock after the copy, it
 * has to revalidate that the connection is still valid for the socket
 * option.
 */
static int
rack_set_sockopt(struct tcpcb *tp, struct sockopt *sopt)
{
	struct inpcb *inp = tptoinpcb(tp);
#ifdef INET
	struct ip *ip;
#endif
	struct tcp_rack *rack;
	struct tcp_hybrid_req hybrid;
	uint64_t loptval;
	int32_t error = 0, optval;

	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack == NULL) {
		INP_WUNLOCK(inp);
		return (EINVAL);
	}
#ifdef INET
	ip = (struct ip *)rack->r_ctl.fsb.tcp_ip_hdr;
#endif

	switch (sopt->sopt_level) {
#ifdef INET6
	case IPPROTO_IPV6:
		MPASS(inp->inp_vflag & INP_IPV6PROTO);
		switch (sopt->sopt_name) {
		case IPV6_USE_MIN_MTU:
			tcp6_use_min_mtu(tp);
			break;
		}
		INP_WUNLOCK(inp);
		return (0);
#endif
#ifdef INET
	case IPPROTO_IP:
		switch (sopt->sopt_name) {
		case IP_TOS:
			/*
			 * The DSCP codepoint has changed, update the fsb.
			 */
			ip->ip_tos = rack->rc_inp->inp_ip_tos;
			break;
		case IP_TTL:
			/*
			 * The TTL has changed, update the fsb.
			 */
			ip->ip_ttl = rack->rc_inp->inp_ip_ttl;
			break;
		}
		INP_WUNLOCK(inp);
		return (0);
#endif
#ifdef SO_PEERPRIO
	case SOL_SOCKET:
		switch (sopt->sopt_name) {
		case SO_PEERPRIO:			/*  SC-URL:bs */
			/* Already read in and sanity checked in sosetopt(). */
			if (inp->inp_socket) {
				rack->client_bufferlvl = inp->inp_socket->so_peerprio;
			}
			break;
		}
		INP_WUNLOCK(inp);
		return (0);
#endif
	case IPPROTO_TCP:
		switch (sopt->sopt_name) {
		case TCP_RACK_TLP_REDUCE:		/*  URL:tlp_reduce */
		/*  Pacing related ones */
		case TCP_RACK_PACE_ALWAYS:		/*  URL:pace_always */
		case TCP_BBR_RACK_INIT_RATE:		/*  URL:irate */
		case TCP_RACK_PACE_MIN_SEG:		/*  URL:pace_min_seg */
		case TCP_RACK_PACE_MAX_SEG:		/*  URL:pace_max_seg */
		case TCP_RACK_FORCE_MSEG:		/*  URL:force_max_seg */
		case TCP_RACK_PACE_RATE_CA:		/*  URL:pr_ca */
		case TCP_RACK_PACE_RATE_SS:		/*  URL:pr_ss*/
		case TCP_RACK_PACE_RATE_REC:		/*  URL:pr_rec */
		case TCP_RACK_GP_INCREASE_CA:		/*  URL:gp_inc_ca */
		case TCP_RACK_GP_INCREASE_SS:		/*  URL:gp_inc_ss */
		case TCP_RACK_GP_INCREASE_REC:		/*  URL:gp_inc_rec */
		case TCP_RACK_RR_CONF:			/*  URL:rrr_conf */
		case TCP_BBR_HDWR_PACE:			/*  URL:hdwrpace */
		case TCP_HDWR_RATE_CAP:			/*  URL:hdwrcap boolean */
		case TCP_PACING_RATE_CAP:		/*  URL:cap  -- used by side-channel */
		case TCP_HDWR_UP_ONLY:			/*  URL:uponly -- hardware pacing  boolean */
		case TCP_FILLCW_RATE_CAP:		/*  URL:fillcw_cap */
		case TCP_RACK_PACING_BETA_ECN:		/*  URL:pacing_beta_ecn */
		case TCP_RACK_PACE_TO_FILL:		/*  URL:fillcw */
			/* End pacing related */
		case TCP_POLICER_DETECT:		/*  URL:pol_det */
		case TCP_POLICER_MSS:			/*  URL:pol_mss */
		case TCP_DELACK:			/*  URL:delack (in base TCP i.e. tcp_hints along with cc etc ) */
		case TCP_RACK_PRR_SENDALOT:		/*  URL:prr_sendalot */
		case TCP_RACK_MIN_TO:			/*  URL:min_to */
		case TCP_RACK_EARLY_SEG:		/*  URL:early_seg */
		case TCP_RACK_REORD_THRESH:		/*  URL:reord_thresh */
		case TCP_RACK_REORD_FADE:		/*  URL:reord_fade */
		case TCP_RACK_TLP_THRESH:		/*  URL:tlp_thresh */
		case TCP_RACK_PKT_DELAY:		/*  URL:pkt_delay */
		case TCP_RACK_TLP_USE:			/*  URL:tlp_use */
		case TCP_BBR_RACK_RTT_USE:		/*  URL:rttuse */
		case TCP_BBR_USE_RACK_RR:		/*  URL:rackrr */
		case TCP_RACK_DO_DETECTION:		/*  URL:detect */
		case TCP_NO_PRR:			/*  URL:noprr */
		case TCP_TIMELY_DYN_ADJ:      		/*  URL:dynamic */
		case TCP_DATA_AFTER_CLOSE:		/*  no URL */
		case TCP_RACK_NONRXT_CFG_RATE:		/*  URL:nonrxtcr */
		case TCP_SHARED_CWND_ENABLE:		/*  URL:scwnd */
		case TCP_RACK_MBUF_QUEUE:		/*  URL:mqueue */
		case TCP_RACK_NO_PUSH_AT_MAX:		/*  URL:npush */
		case TCP_SHARED_CWND_TIME_LIMIT:	/*  URL:lscwnd */
		case TCP_RACK_PROFILE:			/*  URL:profile */
		case TCP_SIDECHAN_DIS:			/*  URL:scodm */
		case TCP_HYBRID_PACING:			/*  URL:pacing=hybrid */
		case TCP_USE_CMP_ACKS:			/*  URL:cmpack */
		case TCP_RACK_ABC_VAL:			/*  URL:labc */
		case TCP_REC_ABC_VAL:			/*  URL:reclabc */
		case TCP_RACK_MEASURE_CNT:		/*  URL:measurecnt */
		case TCP_DEFER_OPTIONS:			/*  URL:defer */
		case TCP_RACK_DSACK_OPT:		/*  URL:dsack */
		case TCP_RACK_TIMER_SLOP:		/*  URL:timer_slop */
		case TCP_RACK_ENABLE_HYSTART:		/*  URL:hystart */
		case TCP_RACK_SET_RXT_OPTIONS:		/*  URL:rxtsz */
		case TCP_RACK_HI_BETA:			/*  URL:hibeta */
		case TCP_RACK_SPLIT_LIMIT:		/*  URL:split */
		case TCP_SS_EEXIT:			/*  URL:eexit */
		case TCP_DGP_UPPER_BOUNDS:		/*  URL:upper */
		case TCP_RACK_PACING_DIVISOR:		/*  URL:divisor */
		case TCP_PACING_DND:			/*  URL:dnd */
		case TCP_NO_TIMELY:			/*  URL:notimely */
		case RACK_CSPR_IS_FCC:			/*  URL:csprisfcc */
		case TCP_HONOR_HPTS_MIN:		/*  URL:hptsmin */
		case TCP_REC_IS_DYN:			/*  URL:dynrec */
		case TCP_GP_USE_LTBW:			/*  URL:useltbw */
			goto process_opt;
			break;
		default:
			/* Filter off all unknown options to the base stack */
			return (tcp_default_ctloutput(tp, sopt));
			break;
		}
	default:
		INP_WUNLOCK(inp);
		return (0);
	}
process_opt:
	INP_WUNLOCK(inp);
	if ((sopt->sopt_name == TCP_PACING_RATE_CAP) ||
	    (sopt->sopt_name == TCP_FILLCW_RATE_CAP)) {
		error = sooptcopyin(sopt, &loptval, sizeof(loptval), sizeof(loptval));
		/*
		 * We truncate it down to 32 bits for the socket-option trace this
		 * means rates > 34Gbps won't show right, but thats probably ok.
		 */
		optval = (uint32_t)loptval;
	} else if (sopt->sopt_name == TCP_HYBRID_PACING) {
		error = sooptcopyin(sopt, &hybrid, sizeof(hybrid), sizeof(hybrid));
	} else {
		error = sooptcopyin(sopt, &optval, sizeof(optval), sizeof(optval));
		/* Save it in 64 bit form too */
		loptval = optval;
	}
	if (error)
		return (error);
	INP_WLOCK(inp);
	if (tp->t_fb != &__tcp_rack) {
		INP_WUNLOCK(inp);
		return (ENOPROTOOPT);
	}
	if (rack->defer_options && (rack->gp_ready == 0) &&
	    (sopt->sopt_name != TCP_DEFER_OPTIONS) &&
	    (sopt->sopt_name != TCP_HYBRID_PACING) &&
	    (sopt->sopt_name != TCP_RACK_SET_RXT_OPTIONS) &&
	    (sopt->sopt_name != TCP_RACK_PACING_BETA_ECN) &&
	    (sopt->sopt_name != TCP_RACK_MEASURE_CNT)) {
		/* Options are being deferred */
		if (rack_add_deferred_option(rack, sopt->sopt_name, loptval)) {
			INP_WUNLOCK(inp);
			return (0);
		} else {
			/* No memory to defer, fail */
			INP_WUNLOCK(inp);
			return (ENOMEM);
		}
	}
	error = rack_process_option(tp, rack, sopt->sopt_name, optval, loptval, &hybrid);
	INP_WUNLOCK(inp);
	return (error);
}

static void
rack_fill_info(struct tcpcb *tp, struct tcp_info *ti)
{

	INP_WLOCK_ASSERT(tptoinpcb(tp));
	bzero(ti, sizeof(*ti));

	ti->tcpi_state = tp->t_state;
	if ((tp->t_flags & TF_REQ_TSTMP) && (tp->t_flags & TF_RCVD_TSTMP))
		ti->tcpi_options |= TCPI_OPT_TIMESTAMPS;
	if (tp->t_flags & TF_SACK_PERMIT)
		ti->tcpi_options |= TCPI_OPT_SACK;
	if ((tp->t_flags & TF_REQ_SCALE) && (tp->t_flags & TF_RCVD_SCALE)) {
		ti->tcpi_options |= TCPI_OPT_WSCALE;
		ti->tcpi_snd_wscale = tp->snd_scale;
		ti->tcpi_rcv_wscale = tp->rcv_scale;
	}
	if (tp->t_flags2 & (TF2_ECN_PERMIT | TF2_ACE_PERMIT))
		ti->tcpi_options |= TCPI_OPT_ECN;
	if (tp->t_flags & TF_FASTOPEN)
		ti->tcpi_options |= TCPI_OPT_TFO;
	/* still kept in ticks is t_rcvtime */
	ti->tcpi_last_data_recv = ((uint32_t)ticks - tp->t_rcvtime) * tick;
	/* Since we hold everything in precise useconds this is easy */
	ti->tcpi_rtt = tp->t_srtt;
	ti->tcpi_rttvar = tp->t_rttvar;
	ti->tcpi_rto = tp->t_rxtcur;
	ti->tcpi_snd_ssthresh = tp->snd_ssthresh;
	ti->tcpi_snd_cwnd = tp->snd_cwnd;
	/*
	 * FreeBSD-specific extension fields for tcp_info.
	 */
	ti->tcpi_rcv_space = tp->rcv_wnd;
	ti->tcpi_rcv_nxt = tp->rcv_nxt;
	ti->tcpi_snd_wnd = tp->snd_wnd;
	ti->tcpi_snd_bwnd = 0;		/* Unused, kept for compat. */
	ti->tcpi_snd_nxt = tp->snd_nxt;
	ti->tcpi_snd_mss = tp->t_maxseg;
	ti->tcpi_rcv_mss = tp->t_maxseg;
	ti->tcpi_snd_rexmitpack = tp->t_sndrexmitpack;
	ti->tcpi_rcv_ooopack = tp->t_rcvoopack;
	ti->tcpi_snd_zerowin = tp->t_sndzerowin;
	ti->tcpi_total_tlp = tp->t_sndtlppack;
	ti->tcpi_total_tlp_bytes = tp->t_sndtlpbyte;
	ti->tcpi_rttmin = tp->t_rttlow;
#ifdef NETFLIX_STATS
	memcpy(&ti->tcpi_rxsyninfo, &tp->t_rxsyninfo, sizeof(struct tcpsyninfo));
#endif
#ifdef TCP_OFFLOAD
	if (tp->t_flags & TF_TOE) {
		ti->tcpi_options |= TCPI_OPT_TOE;
		tcp_offload_tcp_info(tp, ti);
	}
#endif
}

static int
rack_get_sockopt(struct tcpcb *tp, struct sockopt *sopt)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct tcp_rack *rack;
	int32_t error, optval;
	uint64_t val, loptval;
	struct	tcp_info ti;
	/*
	 * Because all our options are either boolean or an int, we can just
	 * pull everything into optval and then unlock and copy. If we ever
	 * add a option that is not a int, then this will have quite an
	 * impact to this routine.
	 */
	error = 0;
	rack = (struct tcp_rack *)tp->t_fb_ptr;
	if (rack == NULL) {
		INP_WUNLOCK(inp);
		return (EINVAL);
	}
	switch (sopt->sopt_name) {
	case TCP_INFO:
		/* First get the info filled */
		rack_fill_info(tp, &ti);
		/* Fix up the rtt related fields if needed */
		INP_WUNLOCK(inp);
		error = sooptcopyout(sopt, &ti, sizeof ti);
		return (error);
	/*
	 * Beta is the congestion control value for NewReno that influences how
	 * much of a backoff happens when loss is detected. It is normally set
	 * to 50 for 50% i.e. the cwnd is reduced to 50% of its previous value
	 * when you exit recovery.
	 */
	case TCP_RACK_PACING_BETA:
		break;
		/*
		 * Beta_ecn is the congestion control value for NewReno that influences how
		 * much of a backoff happens when a ECN mark is detected. It is normally set
		 * to 80 for 80% i.e. the cwnd is reduced by 20% of its previous value when
		 * you exit recovery. Note that classic ECN has a beta of 50, it is only
		 * ABE Ecn that uses this "less" value, but we do too with pacing :)
		 */

	case TCP_RACK_PACING_BETA_ECN:
		if (strcmp(tp->t_cc->name, CCALGONAME_NEWRENO) != 0)
			error = EINVAL;
		else if (rack->rc_pacing_cc_set == 0)
			optval = rack->r_ctl.rc_saved_beta.beta_ecn;
		else {
			/*
			 * Reach out into the CC data and report back what
			 * I have previously set. Yeah it looks hackish but
			 * we don't want to report the saved values.
			 */
			if (tp->t_ccv.cc_data)
				optval = ((struct newreno *)tp->t_ccv.cc_data)->beta_ecn;
			else
				error = EINVAL;
		}
		break;
	case TCP_RACK_DSACK_OPT:
		optval = 0;
		if (rack->rc_rack_tmr_std_based) {
			optval |= 1;
		}
		if (rack->rc_rack_use_dsack) {
			optval |= 2;
		}
		break;
	case TCP_RACK_ENABLE_HYSTART:
	{
		if (tp->t_ccv.flags & CCF_HYSTART_ALLOWED) {
			optval = RACK_HYSTART_ON;
			if (tp->t_ccv.flags & CCF_HYSTART_CAN_SH_CWND)
				optval = RACK_HYSTART_ON_W_SC;
			if (tp->t_ccv.flags & CCF_HYSTART_CONS_SSTH)
				optval = RACK_HYSTART_ON_W_SC_C;
		} else {
			optval = RACK_HYSTART_OFF;
		}
	}
	break;
	case TCP_RACK_DGP_IN_REC:
		error = EINVAL;
		break;
	case TCP_RACK_HI_BETA:
		optval = rack->rack_hibeta;
		break;
	case TCP_POLICER_MSS:
		optval = rack->r_ctl.policer_del_mss;
		break;
	case TCP_POLICER_DETECT:
		optval = rack->r_ctl.saved_policer_val;
		break;
	case TCP_DEFER_OPTIONS:
		optval = rack->defer_options;
		break;
	case TCP_RACK_MEASURE_CNT:
		optval = rack->r_ctl.req_measurements;
		break;
	case TCP_REC_ABC_VAL:
		optval = rack->r_use_labc_for_rec;
		break;
	case TCP_RACK_ABC_VAL:
		optval = rack->rc_labc;
		break;
	case TCP_HDWR_UP_ONLY:
		optval= rack->r_up_only;
		break;
	case TCP_FILLCW_RATE_CAP:
		loptval = rack->r_ctl.fillcw_cap;
		break;
	case TCP_PACING_RATE_CAP:
		loptval = rack->r_ctl.bw_rate_cap;
		break;
	case TCP_RACK_PROFILE:
		/* You cannot retrieve a profile, its write only */
		error = EINVAL;
		break;
	case TCP_SIDECHAN_DIS:
		optval = rack->r_ctl.side_chan_dis_mask;
		break;
	case TCP_HYBRID_PACING:
		/* You cannot retrieve hybrid pacing information, its write only */
		error = EINVAL;
		break;
	case TCP_USE_CMP_ACKS:
		optval = rack->r_use_cmp_ack;
		break;
	case TCP_RACK_PACE_TO_FILL:
		optval = rack->rc_pace_to_cwnd;
		break;
	case TCP_RACK_NO_PUSH_AT_MAX:
		optval = rack->r_ctl.rc_no_push_at_mrtt;
		break;
	case TCP_SHARED_CWND_ENABLE:
		optval = rack->rack_enable_scwnd;
		break;
	case TCP_RACK_NONRXT_CFG_RATE:
		optval = rack->rack_rec_nonrxt_use_cr;
		break;
	case TCP_NO_PRR:
		if (rack->rack_no_prr  == 1)
			optval = 1;
		else if (rack->no_prr_addback == 1)
			optval = 2;
		else
			optval = 0;
		break;
	case TCP_GP_USE_LTBW:
		if (rack->dis_lt_bw) {
			/* It is not used */
			optval = 0;
		} else if (rack->use_lesser_lt_bw) {
			/* we use min() */
			optval = 1;
		} else {
			/* we use max() */
			optval = 2;
		}
		break;
	case TCP_RACK_DO_DETECTION:
		optval = rack->do_detection;
		break;
	case TCP_RACK_MBUF_QUEUE:
		/* Now do we use the LRO mbuf-queue feature */
		optval = rack->r_mbuf_queue;
		break;
	case RACK_CSPR_IS_FCC:
		optval = rack->cspr_is_fcc;
		break;
	case TCP_TIMELY_DYN_ADJ:
		optval = rack->rc_gp_dyn_mul;
		break;
	case TCP_BBR_IWINTSO:
		error = EINVAL;
		break;
	case TCP_RACK_TLP_REDUCE:
		/* RACK TLP cwnd reduction (bool) */
		optval = rack->r_ctl.rc_tlp_cwnd_reduce;
		break;
	case TCP_BBR_RACK_INIT_RATE:
		val = rack->r_ctl.init_rate;
		/* convert to kbits per sec */
		val *= 8;
		val /= 1000;
		optval = (uint32_t)val;
		break;
	case TCP_RACK_FORCE_MSEG:
		optval = rack->rc_force_max_seg;
		break;
	case TCP_RACK_PACE_MIN_SEG:
		optval = rack->r_ctl.rc_user_set_min_segs;
		break;
	case TCP_RACK_PACE_MAX_SEG:
		/* Max segments in a pace */
		optval = rack->rc_user_set_max_segs;
		break;
	case TCP_RACK_PACE_ALWAYS:
		/* Use the always pace method */
		optval = rack->rc_always_pace;
		break;
	case TCP_RACK_PRR_SENDALOT:
		/* Allow PRR to send more than one seg */
		optval = rack->r_ctl.rc_prr_sendalot;
		break;
	case TCP_RACK_MIN_TO:
		/* Minimum time between rack t-o's in ms */
		optval = rack->r_ctl.rc_min_to;
		break;
	case TCP_RACK_SPLIT_LIMIT:
		optval = rack->r_ctl.rc_split_limit;
		break;
	case TCP_RACK_EARLY_SEG:
		/* If early recovery max segments */
		optval = rack->r_ctl.rc_early_recovery_segs;
		break;
	case TCP_RACK_REORD_THRESH:
		/* RACK reorder threshold (shift amount) */
		optval = rack->r_ctl.rc_reorder_shift;
		break;
	case TCP_SS_EEXIT:
		if (rack->r_ctl.gp_rnd_thresh) {
			uint32_t v;

			v = rack->r_ctl.gp_gain_req;
			v <<= 17;
			optval = v | (rack->r_ctl.gp_rnd_thresh & 0xff);
			if (rack->r_ctl.gate_to_fs == 1)
				optval |= 0x10000;
		} else
			optval = 0;
		break;
	case TCP_RACK_REORD_FADE:
		/* Does reordering fade after ms time */
		optval = rack->r_ctl.rc_reorder_fade;
		break;
	case TCP_BBR_USE_RACK_RR:
		/* Do we use the rack cheat for rxt */
		optval = rack->use_rack_rr;
		break;
	case TCP_RACK_RR_CONF:
		optval = rack->r_rr_config;
		break;
	case TCP_HDWR_RATE_CAP:
		optval = rack->r_rack_hw_rate_caps;
		break;
	case TCP_BBR_HDWR_PACE:
		optval = rack->rack_hdw_pace_ena;
		break;
	case TCP_RACK_TLP_THRESH:
		/* RACK TLP theshold i.e. srtt+(srtt/N) */
		optval = rack->r_ctl.rc_tlp_threshold;
		break;
	case TCP_RACK_PKT_DELAY:
		/* RACK added ms i.e. rack-rtt + reord + N */
		optval = rack->r_ctl.rc_pkt_delay;
		break;
	case TCP_RACK_TLP_USE:
		optval = rack->rack_tlp_threshold_use;
		break;
	case TCP_PACING_DND:
		optval = rack->rc_pace_dnd;
		break;
	case TCP_RACK_PACE_RATE_CA:
		optval = rack->r_ctl.rc_fixed_pacing_rate_ca;
		break;
	case TCP_RACK_PACE_RATE_SS:
		optval = rack->r_ctl.rc_fixed_pacing_rate_ss;
		break;
	case TCP_RACK_PACE_RATE_REC:
		optval = rack->r_ctl.rc_fixed_pacing_rate_rec;
		break;
	case TCP_DGP_UPPER_BOUNDS:
		optval = rack->r_ctl.rack_per_upper_bound_ss;
		optval <<= 16;
		optval |= rack->r_ctl.rack_per_upper_bound_ca;
		break;
	case TCP_RACK_GP_INCREASE_SS:
		optval = rack->r_ctl.rack_per_of_gp_ca;
		break;
	case TCP_RACK_GP_INCREASE_CA:
		optval = rack->r_ctl.rack_per_of_gp_ss;
		break;
	case TCP_RACK_PACING_DIVISOR:
		optval = rack->r_ctl.pace_len_divisor;
		break;
	case TCP_BBR_RACK_RTT_USE:
		optval = rack->r_ctl.rc_rate_sample_method;
		break;
	case TCP_DELACK:
		optval = tp->t_delayed_ack;
		break;
	case TCP_DATA_AFTER_CLOSE:
		optval = rack->rc_allow_data_af_clo;
		break;
	case TCP_SHARED_CWND_TIME_LIMIT:
		optval = rack->r_limit_scw;
		break;
	case TCP_HONOR_HPTS_MIN:
		if (rack->r_use_hpts_min)
			optval = rack->r_ctl.max_reduction;
		else
			optval = 0;
		break;
	case TCP_REC_IS_DYN:
		optval = rack->rc_gp_no_rec_chg;
		break;
	case TCP_NO_TIMELY:
		optval = rack->rc_skip_timely;
		break;
	case TCP_RACK_TIMER_SLOP:
		optval = rack->r_ctl.timer_slop;
		break;
	default:
		return (tcp_default_ctloutput(tp, sopt));
		break;
	}
	INP_WUNLOCK(inp);
	if (error == 0) {
		if ((sopt->sopt_name == TCP_PACING_RATE_CAP) ||
		    (sopt->sopt_name == TCP_FILLCW_RATE_CAP))
			error = sooptcopyout(sopt, &loptval, sizeof loptval);
		else
			error = sooptcopyout(sopt, &optval, sizeof optval);
	}
	return (error);
}

static int
rack_ctloutput(struct tcpcb *tp, struct sockopt *sopt)
{
	if (sopt->sopt_dir == SOPT_SET) {
		return (rack_set_sockopt(tp, sopt));
	} else if (sopt->sopt_dir == SOPT_GET) {
		return (rack_get_sockopt(tp, sopt));
	} else {
		panic("%s: sopt_dir $%d", __func__, sopt->sopt_dir);
	}
}

static const char *rack_stack_names[] = {
	__XSTRING(STACKNAME),
#ifdef STACKALIAS
	__XSTRING(STACKALIAS),
#endif
};

static int
rack_ctor(void *mem, int32_t size, void *arg, int32_t how)
{
	memset(mem, 0, size);
	return (0);
}

static void
rack_dtor(void *mem, int32_t size, void *arg)
{

}

static bool rack_mod_inited = false;

static int
tcp_addrack(module_t mod, int32_t type, void *data)
{
	int32_t err = 0;
	int num_stacks;

	switch (type) {
	case MOD_LOAD:
		rack_zone = uma_zcreate(__XSTRING(MODNAME) "_map",
		    sizeof(struct rack_sendmap),
		    rack_ctor, rack_dtor, NULL, NULL, UMA_ALIGN_PTR, 0);

		rack_pcb_zone = uma_zcreate(__XSTRING(MODNAME) "_pcb",
		    sizeof(struct tcp_rack),
		    rack_ctor, NULL, NULL, NULL, UMA_ALIGN_CACHE, 0);

		sysctl_ctx_init(&rack_sysctl_ctx);
		rack_sysctl_root = SYSCTL_ADD_NODE(&rack_sysctl_ctx,
		    SYSCTL_STATIC_CHILDREN(_net_inet_tcp),
		    OID_AUTO,
#ifdef STACKALIAS
		    __XSTRING(STACKALIAS),
#else
		    __XSTRING(STACKNAME),
#endif
		    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
		    "");
		if (rack_sysctl_root == NULL) {
			printf("Failed to add sysctl node\n");
			err = EFAULT;
			goto free_uma;
		}
		rack_init_sysctls();
		num_stacks = nitems(rack_stack_names);
		err = register_tcp_functions_as_names(&__tcp_rack, M_WAITOK,
		    rack_stack_names, &num_stacks);
		if (err) {
			printf("Failed to register %s stack name for "
			    "%s module\n", rack_stack_names[num_stacks],
			    __XSTRING(MODNAME));
			sysctl_ctx_free(&rack_sysctl_ctx);
free_uma:
			uma_zdestroy(rack_zone);
			uma_zdestroy(rack_pcb_zone);
			rack_counter_destroy();
			printf("Failed to register rack module -- err:%d\n", err);
			return (err);
		}
		tcp_lro_reg_mbufq();
		rack_mod_inited = true;
		break;
	case MOD_QUIESCE:
		err = deregister_tcp_functions(&__tcp_rack, true, false);
		break;
	case MOD_UNLOAD:
		err = deregister_tcp_functions(&__tcp_rack, false, true);
		if (err == EBUSY)
			break;
		if (rack_mod_inited) {
			uma_zdestroy(rack_zone);
			uma_zdestroy(rack_pcb_zone);
			sysctl_ctx_free(&rack_sysctl_ctx);
			rack_counter_destroy();
			rack_mod_inited = false;
		}
		tcp_lro_dereg_mbufq();
		err = 0;
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (err);
}

static moduledata_t tcp_rack = {
	.name = __XSTRING(MODNAME),
	.evhand = tcp_addrack,
	.priv = 0
};

MODULE_VERSION(MODNAME, 1);
DECLARE_MODULE(MODNAME, tcp_rack, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
MODULE_DEPEND(MODNAME, tcphpts, 1, 1, 1);

#endif /* #if !defined(INET) && !defined(INET6) */
