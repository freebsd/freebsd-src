/*-
 * Copyright (c) 2024- Netflix, Inc.
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

MALLOC_DECLARE(M_TCPPCM);

void
rack_update_pcm_ack(struct tcp_rack *rack, int was_cumack, uint32_t start, uint32_t end)
{
	struct rack_pcm_stats *e;
	int i, completed = 0;
	uint64_t ack_arrival;
	int segsiz;

	if (rack->pcm_in_progress == 0)
		return;

	if (SEQ_LEQ(end, rack->r_ctl.pcm_i.sseq)) {
		/*
		 * Its not in our range of data sent, it
		 * is before our first seq.
		 */
		return;
	}
	/* We take away 1 mss from the end to avoid delayed ack */
	segsiz = ctf_fixed_maxseg(rack->rc_tp);
	if (SEQ_GEQ(end, (rack->r_ctl.pcm_i.eseq - segsiz))) {
		/*
		 * We have reached beyond the end of the
		 * initial send. Even though things may
		 * still be lost and this could be something
		 * from a different send than our burst.
		 */
		completed = 1;
		rack->pcm_in_progress = 0;
		rack->r_ctl.last_pcm_round = rack->r_ctl.current_round;
		rack->r_ctl.pcm_idle_rounds = 0;
	}
	if (SEQ_GEQ(start, rack->r_ctl.pcm_i.eseq)) {
		/*
		 * This is outside the scope
		 * of the measurement itself and
		 * is likely a sack above our burst.
		 */
		goto skip_ack_accounting;
	}
	/*
	 * Record ACK data. 
	 */
	ack_arrival = tcp_tv_to_lusectick(&rack->r_ctl.act_rcv_time);
	if (SEQ_GT(end, rack->r_ctl.pcm_i.eseq)) {
		/* Trim the end to the end of our range if it is beyond */
		end = rack->r_ctl.pcm_i.eseq;
	}
	if ((rack->r_ctl.pcm_i.cnt + 1) > rack->r_ctl.pcm_i.cnt_alloc) {
		/* Need to expand, first is there any present? */
		size_t sz;

		if (rack->r_ctl.pcm_i.cnt_alloc == 0) {
			/*
			 * Failed at rack_init I suppose.
			 */
			rack->r_ctl.pcm_i.cnt_alloc = RACK_DEFAULT_PCM_ARRAY;
			sz = (sizeof(struct rack_pcm_stats) * rack->r_ctl.pcm_i.cnt_alloc);
			rack->r_ctl.pcm_s = malloc(sz, M_TCPPCM, M_NOWAIT);
			if (rack->r_ctl.pcm_s == NULL) {
				rack->r_ctl.pcm_i.cnt_alloc = 0;
				rack->pcm_in_progress = 0;
				return;
			}
		} else {
			/* Need to expand the array */
			struct rack_pcm_stats *n;
			uint16_t new_cnt;

			new_cnt = rack->r_ctl.pcm_i.cnt_alloc * 2;
			sz = (sizeof(struct rack_pcm_stats) * new_cnt);
			n = malloc(sz,M_TCPPCM, M_NOWAIT);
			if (n == NULL) {
				/* We are dead, no memory */
				rack->pcm_in_progress = 0;
				rack->r_ctl.pcm_i.cnt = 0;
				return;
			}
			sz = (sizeof(struct rack_pcm_stats) * rack->r_ctl.pcm_i.cnt_alloc);
			memcpy(n, rack->r_ctl.pcm_s, sz);
			free(rack->r_ctl.pcm_s, M_TCPPCM);
			rack->r_ctl.pcm_s = n;
			rack->r_ctl.pcm_i.cnt_alloc = new_cnt;
		}
	}
	e = &rack->r_ctl.pcm_s[rack->r_ctl.pcm_i.cnt];
	rack->r_ctl.pcm_i.cnt++;
	e->sseq = start;
	e->eseq = end;
	e->ack_time = ack_arrival;
skip_ack_accounting:
	if (completed == 0)
		return;
	/*
	 * Ok we are to the point where we can assess what
	 * has happened and make a PCM judgement.
	 */

	if (tcp_bblogging_on(rack->rc_tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		uint64_t prev_time = 0;
		uint64_t tot_byt = 0;
		uint32_t tot_lt_12us = 0;
		uint32_t tot_gt_2mss = 0;

		(void)tcp_get_usecs(&tv);
		for (i=0; i<rack->r_ctl.pcm_i.cnt; i++) {

			e = &rack->r_ctl.pcm_s[i];
			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.timeStamp = tcp_tv_to_usectick(&tv);
			log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
			log.u_bbr.flex8 = 1;
			log.u_bbr.flex1 = e->sseq;
			log.u_bbr.flex2 = e->eseq;
			tot_byt += (e->eseq - e->sseq);
			if ((i > 0) &&
			    (e->ack_time > prev_time)) {
				log.u_bbr.flex3 = (uint32_t)(e->ack_time - prev_time);
			} else {
				log.u_bbr.flex3 = 0;
			}
			if (e->ack_time > rack->r_ctl.pcm_i.send_time) {
				log.u_bbr.flex4 = (uint32_t)(e->ack_time - rack->r_ctl.pcm_i.send_time);
			} else {
				log.u_bbr.flex4 = 0;
			}
			if ((e->eseq - e->sseq) > (segsiz * 2)) {
				tot_gt_2mss++;
			}
			if ((i > 0) &&
			    (log.u_bbr.flex3 < 12)) {
				tot_lt_12us++;
			}
			prev_time = e->ack_time;
			log.u_bbr.cur_del_rate = rack->r_ctl.pcm_i.send_time;
			if ((i > 0) &&
			    (log.u_bbr.flex3 > 0)) {
				/*
				 * Calculate a b/w between this chunk and the previous.
				 */
				log.u_bbr.delRate = (e->eseq - e->sseq);
				log.u_bbr.delRate *= HPTS_USEC_IN_SEC;
				log.u_bbr.delRate /= (uint64_t)log.u_bbr.flex3;
			}
			log.u_bbr.rttProp = e->ack_time;
			(void)tcp_log_event(rack->rc_tp, NULL, NULL, NULL, TCP_PCM_MEASURE, ERRNO_UNK,
					    0, &log, false, NULL, NULL, 0, &tv);
		}
		if (prev_time  > rack->r_ctl.pcm_i.send_time) {
			/*
			 * Prev time holds the last ack arrival time.
			 */
			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.timeStamp = tcp_tv_to_usectick(&tv);
			log.u_bbr.inflight = ctf_flight_size(rack->rc_tp, rack->r_ctl.rc_sacked);
			log.u_bbr.flex8 = 2;
			log.u_bbr.flex1 = rack->r_ctl.pcm_i.sseq;
			log.u_bbr.flex2 = rack->r_ctl.pcm_i.eseq;
			log.u_bbr.flex3 = tot_byt;
			log.u_bbr.flex4 = tot_lt_12us;	/* How many deltas indicate > 2Gbps */
			log.u_bbr.flex5 = tot_gt_2mss;  /* How many acks represent more than 2MSS */
			log.u_bbr.flex7 = rack->r_ctl.pcm_i.cnt;
			log.u_bbr.cwnd_gain = rack->r_ctl.pcm_i.cnt_alloc;
			log.u_bbr.cur_del_rate = rack->r_ctl.pcm_i.send_time;
			log.u_bbr.rttProp = prev_time;
			log.u_bbr.delRate = tot_byt;
			log.u_bbr.delRate *= HPTS_USEC_IN_SEC;
			log.u_bbr.delRate /= (prev_time - rack->r_ctl.pcm_i.send_time);
			(void)tcp_log_event(rack->rc_tp, NULL, NULL, NULL, TCP_PCM_MEASURE, ERRNO_UNK,
					    0, &log, false, NULL, NULL, 0, &tv);
		}
	}
	/* 
	 * Here we need a lot to be added including:
	 * 1) Some form of measurement, where if we think the measurement
	 *    is valid we iterate over the PCM data and come up with a path
	 *    capacity estimate.
	 * 2) We may decide that the PCM is invalid due to ack meddlers and
	 *    thus need to increase the PCM size (which defaults to 10mss).
	 * 3) We may need to think about shrinking the PCM size if we are
	 *    seeing some sort of presistent loss from making the measurement
	 *    (i.e. it got to big and our bursts are causing loss).
	 * 4) If we make a measurement we need to place it somewhere in the
	 *    stack to be reported later somehow. Is it a WMA in the stack or
	 *    the highest or?
	 * 5) Is there a limit on how big we can go PCM size wise, the code
	 *    here will send multiple TSO bursts all at once, but how big
	 *    is too big, and does that then put some bound (I think it does)
	 *    on the largest capacity we can determine?
	 */
	/* New code here */
	/* Clear the cnt we are done */
	rack->r_ctl.pcm_i.cnt = 0;
}

#endif /* #if !defined(INET) && !defined(INET6) */
