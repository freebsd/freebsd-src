/*-
 * Copyright (c) 2008-2009 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed while studying at the Centre for Advanced
 * Internet Architectures, Swinburne University (http://caia.swin.edu.au),
 * made possible in part by a grant from the Cisco University Research Program
 * Fund at Community Foundation Silicon Valley.
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
 * An implementation of the CUBIC congestion algorithm for FreeBSD,
 * based on the Internet Draft "draft-rhee-tcpm-cubic-02.txt" by
 * Rhee, Xu and Ha.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vimage.h>

#include <net/if.h>

#include <netinet/cc.h>
#include <netinet/cc_cubic.h>
#include <netinet/cc_module.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/vinet.h>

/* function prototypes */
int cubic_mod_init(void);
int cubic_cb_init(struct tcpcb *tp);
void cubic_cb_destroy(struct tcpcb *tp);
void cubic_pre_fr(struct tcpcb *tp, struct tcphdr *th);
void cubic_post_fr(struct tcpcb *tp, struct tcphdr *th);
void cubic_ack_received(struct tcpcb *tp, struct tcphdr *th);
void cubic_after_timeout(struct tcpcb *tp);
void cubic_after_idle(struct tcpcb *tp);
void cubic_ssthresh_update(struct tcpcb *tp);
void cubic_conn_init(struct tcpcb *tp);
void cubic_record_rtt(struct tcpcb *tp);

struct cubic {
	/* cwnd at the most recent congestion event */
	u_long max_cwnd;
	/* cwnd at the previous congestion event */
	u_long prev_max_cwnd;
	/* time of last congestion event in ticks */
	u_long t_last_cong;
	/* minimum observed rtt in ticks */
	u_long min_rtt_ticks;
	/* number of congestion events */
	u_long num_cong_events;
};

MALLOC_DECLARE(M_CUBIC);
MALLOC_DEFINE(M_CUBIC, "cubic data",
    "Per connection data required for the CUBIC congestion algorithm");

/* function pointers for various hooks into the TCP stack */
struct cc_algo cubic_cc_algo = {
	.name = "cubic",
	.mod_init = cubic_mod_init,
	.cb_init = cubic_cb_init,
	.cb_destroy = cubic_cb_destroy,
	.conn_init = cubic_conn_init,
	.ack_received = cubic_ack_received,
	.pre_fr = cubic_pre_fr,
	.post_fr = cubic_post_fr,
	.after_timeout = cubic_after_timeout
};

int
cubic_mod_init(void)
{
	cubic_cc_algo.after_idle = newreno_cc_algo.after_idle;
	return (0);
}

void
cubic_conn_init(struct tcpcb *tp)
{
	struct cubic *cubic_data = CC_DATA(tp);

	/*
	 * Ensure we have a sane initial value for max_cwnd recorded.
	 * Without this here bad things happen when entries from
	 * the TCP hostcache get used.
	 */
	cubic_data->max_cwnd = tp->snd_cwnd;
}

/*
 * Initialise CUBIC on the specified TCP control block. Creates a
 * new struct for CUBIC specific data and sticks a pointer to it
 * in the control block
 */
int
cubic_cb_init(struct tcpcb *tp)
{
	struct cubic *cubic_data;
	
	cubic_data = malloc(sizeof(struct cubic), M_CUBIC, M_NOWAIT);
	
	if (cubic_data == NULL)
		return 1;
	
	/* init some key cubic values with sensible defaults */
	cubic_data->t_last_cong = ticks;
	cubic_data->min_rtt_ticks = TCPTV_SRTTBASE;
	cubic_data->max_cwnd = 0;
	cubic_data->prev_max_cwnd = 0;
	cubic_data->num_cong_events = 0;
	
	CC_DATA(tp) = cubic_data;
	
	return 0;
}

/*
 * Free the struct used to store CUBIC specific data for the specified
 * TCP control block.
 */
void
cubic_cb_destroy(struct tcpcb *tp)
{
	if (CC_DATA(tp) != NULL)
		free(CC_DATA(tp), M_CUBIC);
}

/*
 * Perform any necesary tasks before we enter fast recovery
 */
void
cubic_pre_fr(struct tcpcb *tp, struct tcphdr *th)
{
	struct cubic *cubic_data = CC_DATA(tp);

	cubic_data->num_cong_events++;

	cubic_ssthresh_update(tp);

	/*
	 * record the current cwnd at the point of congestion so it can be used
	 * as the basis for resetting cwnd after exiting fr
	 */
	cubic_data->max_cwnd = tp->snd_cwnd;

	printf("congestion event started (max_cwnd: %ld)\n", cubic_data->max_cwnd);
}

/*
 * Decrease cwnd in the event of packet loss.
 */
void
cubic_post_fr(struct tcpcb *tp, struct tcphdr *th)
{
	struct cubic *cubic_data = CC_DATA(tp);

	/*
	 * grab the current time and record it so we know when the most recent
	 * congestion event was
	 */
	cubic_data->t_last_cong = ticks;

	/* fast convergence heuristic */
	if (cubic_data->max_cwnd < cubic_data->prev_max_cwnd) {
		printf("fast convergence heuristic kicked in! (max_cwnd: %d\t prev_max_cwnd: %d\n", (int)cubic_data->max_cwnd, (int)cubic_data->prev_max_cwnd);
		cubic_data->prev_max_cwnd = cubic_data->max_cwnd;
		cubic_data->max_cwnd = (cubic_data->max_cwnd * CUBIC_FC_FACTOR)
		    >> CUBIC_SHIFT;
	}
	else
		cubic_data->prev_max_cwnd = cubic_data->max_cwnd;

	/*
	 * if inflight data is less than ssthresh, set cwnd conservatively
	 * to avoid a burst of data, as suggested in the NewReno RFC.
	 * Otherwise, use the CUBIC method.
	 */
	if (th && SEQ_GT(th->th_ack + tp->snd_ssthresh, tp->snd_max))
		tp->snd_cwnd = tp->snd_max - th->th_ack + tp->t_maxseg;
	else {
		/* update cwnd based on beta and adjusted max_cwnd */
		tp->snd_cwnd = max(1,((CUBIC_BETA * cubic_data->max_cwnd)
		    >> CUBIC_SHIFT));
		printf("cubic_post_fr - cwnd: %ld\tmax_cwnd: %ld\n", tp->snd_cwnd, cubic_data->max_cwnd);
	}

	printf("congestion event over\n");
}

void
cubic_record_rtt(struct tcpcb *tp)
{
	struct cubic *cubic_data = CC_DATA(tp);
	int t_srtt_ticks = tp->t_srtt / TCP_RTT_SCALE;

	/* XXX: Should there be some hysteresis for minrtt? */

	/*
	 * record the current SRTT as our minrtt if it's the smallest we've
	 * seen or minrtt is currently equal to its initialised value.
	 * Ignore srtt until a min number of samples have been taken
	 */
	if ((t_srtt_ticks < cubic_data->min_rtt_ticks ||
	    cubic_data->min_rtt_ticks == TCPTV_SRTTBASE) &&
		(tp->t_rttupdated >= CUBIC_MIN_RTT_SAMPLES))
		cubic_data->min_rtt_ticks = max(1, t_srtt_ticks);
}

/*
 * Increase cwnd on the arrival of an ACK.
 */
void
cubic_ack_received(struct tcpcb *tp, struct tcphdr *th)
{
	struct cubic *cubic_data = CC_DATA(tp);
	u_long w_newreno, w_cubic_next, ticks_since_cong;
	static u_long last_print_ticks = 0;
	int print = 0;

	cubic_record_rtt(tp);

	if ((tp->snd_cwnd < tp->snd_ssthresh) ||
		(tp->snd_ssthresh == TCP_MAXWIN << TCP_MAX_WINSHIFT) ||
			(cubic_data->min_rtt_ticks == TCPTV_SRTTBASE))
                newreno_cc_algo.ack_received(tp, th);
	else {
		/* num ticks since last congestion */
		ticks_since_cong = ticks - cubic_data->t_last_cong;

		if ((ticks - last_print_ticks) >= (cubic_data->min_rtt_ticks / 2)) {
			
			print = 1;
			last_print_ticks = ticks;
		}

		if (print)
			printf("rtt_ticks: %ld\tticks_since_cong: %ld\n", cubic_data->min_rtt_ticks, ticks_since_cong);
	
		w_newreno = reno_cwnd(	ticks_since_cong,
					cubic_data->min_rtt_ticks,
					cubic_data->max_cwnd,
					tp->t_maxseg
		);

		if (print)
			printf("reno_cwnd(%ld,%ld,%ld,%d): %ld\n", ticks_since_cong, cubic_data->min_rtt_ticks, cubic_data->max_cwnd, tp->t_maxseg, w_newreno);

		//w_cubic = cubic_cwnd(ticks_since_cong, cubic_data->max_cwnd, tp->t_maxseg);
		//printf("cubic_cwnd(%ld,%ld,%d): %ld (w_cubic)\n", ticks_since_cong, cubic_data->max_cwnd, tp->t_maxseg, w_cubic);

		w_cubic_next = cubic_cwnd(	ticks_since_cong +
						    cubic_data->min_rtt_ticks,
						cubic_data->max_cwnd,
						tp->t_maxseg
		);

		if (print)
			printf("cubic_cwnd(%ld,%ld,%d): %ld (w_cubic_next)\n", ticks_since_cong + cubic_data->min_rtt_ticks, cubic_data->max_cwnd, tp->t_maxseg, w_cubic_next);

		if (print)
			printf("pre\tmax_cwnd: %ld\tsnd_cwnd: %ld\tw_newreno: %ld\tw_cubic_next: %ld\n", cubic_data->max_cwnd, tp->snd_cwnd, w_newreno, w_cubic_next);

		
		if (w_cubic_next < w_newreno) {
			/* we are in TCP-friendly region, follow reno cwnd growth */
			tp->snd_cwnd = w_newreno;

		} else if (tp->snd_cwnd < w_cubic_next) {
			/* else we are in the concave or convex growth regions */
			if (print)
				printf("incr: %lu\n", (w_cubic_next * tp->t_maxseg) / tp->snd_cwnd);
			//tp->snd_cwnd += max(1, (w_cubic_next - w_cubic) / tp->snd_cwnd);
			//printf("incr: %d\n", max(1, (w_cubic_next - tp->snd_cwnd) / tp->t_maxseg));
			/* XXX: Test under what conditions the following will truncate */
			tp->snd_cwnd += (u_long)(((uint64_t)(w_cubic_next * tp->t_maxseg)) / tp->snd_cwnd);
		}

		/*
		 * if we're not in slow start and we're probing for a new cwnd limit
		 * at the start of a connection (happens when hostcache has a relevant entry),
		 * keep updating our current estimate of the max_cwnd
		 */
		if (cubic_data->num_cong_events == 0 &&
		    cubic_data->max_cwnd < tp->snd_cwnd)
			cubic_data->max_cwnd = tp->snd_cwnd;

		if (print)
			printf("post\tmax_cwnd: %ld\tsnd_cwnd: %ld\n\n", cubic_data->max_cwnd, tp->snd_cwnd);
	}
}

/*
 * Reset the cwnd after a retransmission timeout
 */
void
cubic_after_timeout(struct tcpcb *tp)
{
	struct cubic *cubic_data = CC_DATA(tp);

	cubic_ssthresh_update(tp);

	/*
	 * grab the current time and record it so we know when the most recent
	 * congestion event was. Only record it when the timeout has fired more
	 * than once, as there is a reasonable chance the first one is a false alarm
	 * and may not indicate congestion.
	 */
	if (tp->t_rxtshift >= 2)
		cubic_data->t_last_cong = ticks;

	newreno_cc_algo.after_timeout(tp);
}

/*
 * Update the ssthresh in the event of congestion.
 */
void
cubic_ssthresh_update(struct tcpcb *tp)
{
	/*
	 * on the first congestion event, set ssthresh to cwnd * 0.5, on
	 * subsequent congestion events, set it to cwnd * beta.
	 */
	if (tp->snd_ssthresh == (TCP_MAXWIN << TCP_MAX_WINSHIFT))
		tp->snd_ssthresh = tp->snd_cwnd >> 1;
	else
		tp->snd_ssthresh = (tp->snd_cwnd * CUBIC_BETA) >> CUBIC_SHIFT;
}

DECLARE_CC_MODULE(cubic, &cubic_cc_algo);
