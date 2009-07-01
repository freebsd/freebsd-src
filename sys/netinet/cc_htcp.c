/*-
 * Copyright (c) 2007-2009
 * 	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2009 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart and James Healy,
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
 * HTCP
 *
 * An implementation of HTCP congestion algorithm for FreeBSD.
 * The algorithm is based on the one described in "H-TCP: A framework
 * for congestion control in high-speed and long-distance networks" by
 * Leith, Shorten and Lee.
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
#include <sys/vimage.h>

#include <net/if.h>

#include <netinet/cc.h>
#include <netinet/cc_module.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/vinet.h>

/* useful defines */
#define MODNAME "HTCP congestion control"
#define MODVERSION  "0.9.1"
#define HTCP_SHIFT 8
#define HTCP_ALPHA_INC_SHIFT 4
#define HTCP_INIT_ALPHA 1
#define HTCP_DELTA_L hz /* 1 sec in ticks */
#define HTCP_MINBETA 128 /* 0.5 with a shift << 8 */
#define HTCP_MAXBETA 204 /* ~0.8 with a shift << 8 */
#define HTCP_MINROWE 26 /* ~0.1 with a shift << 8 */
#define HTCP_MAXROWE 512 /* 2 with a shift << 8 */
#define HTCP_RTT_REF 100 /* reference RTT in milliseconds used in the calculation of */
                         /* alpha if htcp_rtt_scaling=1 */
#define HTCP_MIN_RTT_SAMPLES 8 /* don't trust the TCP stack's smoothed rtt estimate */
                               /* until this many samples have been taken */
#define CAST_PTR_INT(X) (*((int*)(X)))

/*
 * HTCP_CALC_ALPHA performs a fixed point math calculation to
 * determine the value of alpha, based on the function defined in
 * H-TCP: A framework for congestion control in high-speed and long distance networks"
 *
 * i.e. 1 + 10(delta - delta_l) + ((delta - delta_l) / 2) ^ 2
 *
 * "diff" is passed in to the macro as "delta - delta_l" and is
 * expected to be in units of ticks.
 *
 * The joyousnous of fixed point maths means our function implementation
 * looks a little funky...
 *
 * In order to maintain some precision in the calculations, a fixed point
 * shift HTCP_ALPHA_INC_SHIFT is used to ensure the integer divisions don't
 * truncate the results too badly.
 *
 * The "16" value is the "1" term in the alpha function shifted up by HTCP_ALPHA_INC_SHIFT
 *
 * The "160" value is the "10" multiplier in the alpha function multiplied by 2^HTCP_ALPHA_INC_SHIFT
 *
 * Specifying these as constants reduces the computations required.
 * After up-shifting all the terms in the function and performing the
 * required calculations, we down-shift the final result by 
 * HTCP_ALPHA_INC_SHIFT to ensure it is back in the correct range.
 *
 * The "hz" terms are required as kernels can be configured to run
 * with different tick timers, which we have to adjust for in the
 * alpha calculation (which originally was defined in terms of seconds).
 *
 * We also have to be careful to constrain the value of diff such that it
 * won't overflow whilst performing the calculation. The middle term i.e.
 * (160 * diff) / hz is the limiting factor in the calculation. We must
 * constrain diff to be less than the max size of an unsigned long divided
 * by the constant 160 figure i.e.
 * diff < [(2 ^ (sizeof(u_long) * 8)) - 1] / 160
 *
 * NB: Changing HTCP_ALPHA_INC_SHIFT will require you to MANUALLY update
 * the constants used in this function!
 */
#define HTCP_CALC_ALPHA(diff) \
(\
	(\
		(16) + \
		((160 * (diff)) / hz) + \
		(((diff) / hz) * (((diff) << HTCP_ALPHA_INC_SHIFT) / (4 * hz))) \
	) >> HTCP_ALPHA_INC_SHIFT \
)

/* function prototypes */
int htcp_mod_init(void);
int htcp_cb_init(struct tcpcb *tp);
void htcp_cb_destroy(struct tcpcb *tp);
void htcp_recalc_alpha(struct tcpcb *tp);
void htcp_recalc_beta(struct tcpcb *tp);
void htcp_pre_fr(struct tcpcb *tp, struct tcphdr *th);
void htcp_post_fr(struct tcpcb *tp, struct tcphdr *th);
void htcp_ack_received(struct tcpcb *tp, struct tcphdr *th);
void htcp_after_timeout(struct tcpcb *tp);
void htcp_ssthresh_update(struct tcpcb *tp);
void htcp_record_rtt(struct tcpcb *tp);

struct htcp {
	u_int alpha;		/* alpha param, used for cwnd increase */
	u_int beta;		/* beta param, used for cwnd increase and decreade */
	u_long prev_cwnd;	/* the value of cwnd before entering fast recovery */
				/* used when setting the cwnd after exiting FR */
	u_long t_last_cong;	/* time of last congestion event in ticks */
	u_int minrtt;		/* the shortest rtt seen for a flow */
	u_int maxrtt;		/* the largest rtt seen for a flow */
};

static u_long htcp_max_diff;
static u_int htcp_rtt_scaling = 0;
static u_int htcp_adaptive_backoff = 0;
static u_int htcp_rtt_ref;

#ifdef HTCP_DEBUG
static u_int htcp_debug_ticks = 1000;
#endif

MALLOC_DECLARE(M_HTCP);
MALLOC_DEFINE(M_HTCP, "htcp data", "Per connection data required for the HTCP congestion algorithm");

/* function pointers for various hooks into the TCP stack */
struct cc_algo htcp_cc_algo = {
	.name = "htcp",
	.mod_init = htcp_mod_init,
	.cb_init = htcp_cb_init,
	.cb_destroy = htcp_cb_destroy,
	.ack_received = htcp_ack_received,
	.pre_fr = htcp_pre_fr,
	.post_fr = htcp_post_fr,
	.after_timeout = htcp_after_timeout
};

/*
 * Initialise HTCP on the specified TCP control block. Creates a
 * new struct for HTCP specific data and sticks a pointer to it
 * in the control block
 */
int
htcp_cb_init(struct tcpcb *tp)
{
	struct htcp *htcp_data;
	
#ifdef HTCP_DEBUG
	printf("initialising tcp connection with htcp congestion control\n");
#endif
	
	MALLOC(htcp_data, struct htcp *, sizeof(struct htcp), M_HTCP, M_NOWAIT);
	
	if (htcp_data == NULL)
		return 1;
	
	/* init some key htcp values with sensible defaults */
	htcp_data->alpha = HTCP_INIT_ALPHA;
	htcp_data->beta = HTCP_MINBETA;
	htcp_data->t_last_cong = ticks;
	htcp_data->prev_cwnd = 0;
	htcp_data->minrtt = TCPTV_SRTTBASE;
	htcp_data->maxrtt = TCPTV_SRTTBASE;
	
	CC_DATA(tp) = htcp_data;
	
	return 0;
}

/*
 * Free the struct used to store HTCP specific data for the specified
 * TCP control block.
 */
void
htcp_cb_destroy(struct tcpcb *tp)
{
#ifdef HTCP_DEBUG
	printf("deinitialising tcp connection with htcp congestion control\n");
#endif
	
	if (CC_DATA(tp) != NULL)
		FREE(CC_DATA(tp), M_HTCP);
}

/*
 * Recalculate the alpha value used for scaling cwnd up.
 * This is currently called once for each ACK that is received.
 */
void
htcp_recalc_alpha(struct tcpcb *tp)
{
	u_int alpha, now;
	struct htcp *htcp_data = CC_DATA(tp);
	u_long delta = 0, diff = 0;
	
#ifdef HTCP_DEBUG
	static u_int debug_counter = 0;
#endif
	
	now = ticks;
	
	/*
	 * if ticks has wrapped around (will happen approximately once
	 * every 49 days on a machine running at 1000hz) and a flow straddles
	 * the wrap point, our alpha calcs will be completely wrong.
	 * We cut our losses and restart alpha from scratch
	 * by setting t_last_cong = now + delta_l
	 *
	 * This does not deflate our cwnd at all. It simply slows the rate
	 * cwnd is growing by until alpha regains the value it held
	 * prior to taking this drastic measure.
	 */
	if (now < htcp_data->t_last_cong)
		htcp_data->t_last_cong = now + HTCP_DELTA_L;
	
	delta = now - htcp_data->t_last_cong;
	
	/*
	 * if its been less than HTCP_DELTA_L ticks since congestion,
	 * set alpha to 1. Otherwise, use the function defined in
	 * the key HTCP paper
	 */
	if (delta < HTCP_DELTA_L)
		htcp_data->alpha = 1;
	
	else if ((diff = delta - HTCP_DELTA_L) < htcp_max_diff)
	{
		alpha = HTCP_CALC_ALPHA(diff);
		
		/* Adaptive backoff fairness adjustment: 2 * (1 - beta) * alpha_raw */
		if(htcp_adaptive_backoff)
			alpha = max(1, (2 * ((1 << HTCP_SHIFT) - htcp_data->beta) * alpha) >> HTCP_SHIFT);
		
		/*
		 * RTT scaling: (RTT / RTT_ref) * alpha_raw
		 * alpha will be the raw value from HTCP_CALC_ALPHA() if adaptive backoff
		 * is off, or the adjusted value if adaptive backoff is on
		 */
		if(htcp_rtt_scaling)
			alpha = max(1, (min(max(HTCP_MINROWE, (tp->t_srtt << HTCP_SHIFT) / htcp_rtt_ref), HTCP_MAXROWE) * alpha) >> HTCP_SHIFT);
		
		htcp_data->alpha = alpha;
	}
	
	
#ifdef HTCP_DEBUG
	/* print out a debug message to syslog periodically */
	if(ticks - debug_counter > htcp_debug_ticks)
	{
		debug_counter = ticks;
		
		printf("alpha: %-4u salpha: %-4u beta: %-4d t_last_cong: %-10u delta: %-6u cwnd: %-7u min|max rtt: %5u|%-5u rtt|rtt_ref: %5u|%-5u\n",
			(unsigned int)HTCP_CALC_ALPHA(diff),
			htcp_data->alpha,
			htcp_data->beta,
			(unsigned int)htcp_data->t_last_cong,
			(unsigned int)delta,
			(unsigned int)tp->snd_cwnd,
			htcp_data->minrtt,
			htcp_data->maxrtt,
			tp->t_srtt,
			htcp_rtt_ref
			);
	}
#endif
}

/*
 * Recalculate the beta value used for scaling cwnd up and down.
 * This is currently called once for each ACK that is received
 */
void
htcp_recalc_beta(struct tcpcb *tp)
{
	struct htcp *htcp_data = CC_DATA(tp);
	
	/*
	 * beta is stored as a fixed point number instead of floating point for
	 * efficiency reasons. The decimal point is moved 1 byte to the left,
	 * allowing for a 3 byte whole number and 1 byte fractional
	 *
	 * any time a number is multiplied or divided by beta, the answer must be right
	 * shifted by a byte to get the real answer
	 *
	 * beta is bounded to ensure it is always between HTCP_MINBETA and HTCP_MAXBETA
	 */
	
	/*
	 * TCPTV_SRTTBASE is the initialised value of each connection's srtt, so we only
	 * calc beta if the connection's srtt has been changed from its inital value
	 */
	if (htcp_adaptive_backoff && htcp_data->minrtt != TCPTV_SRTTBASE && htcp_data->maxrtt != TCPTV_SRTTBASE)
		htcp_data->beta = min(max(HTCP_MINBETA, (htcp_data->minrtt << HTCP_SHIFT) / htcp_data->maxrtt), HTCP_MAXBETA);
	else
		htcp_data->beta = HTCP_MINBETA;
}

/*
 * Record the minimum and maximum RTT seen on the connection.
 * These are used in the calculation of beta if adaptive backoff is enabled.
 */
void
htcp_record_rtt(struct tcpcb *tp)
{
	struct htcp *htcp_data = CC_DATA(tp);
	
	/* TODO: Should there be some hysteresis for minrtt? */
	
	/*
	 * record the current SRTT as our minrtt if it's the smalelst we've 
	 * seen or minrtt is currently equal to its initialised value.
	 * Ignore srtt until a min number of samples have been taken
	 */
	if ((tp->t_srtt < htcp_data->minrtt || htcp_data->minrtt == TCPTV_SRTTBASE) && (tp->t_rttupdated >= HTCP_MIN_RTT_SAMPLES))
		htcp_data->minrtt = tp->t_srtt;
	
	/*
	 * record the current SRTT as our maxrtt if it's the largest we've 
	 * seen.
	 * Ignore srtt until a min number of samples have been taken
	 */
	if (tp->t_srtt > htcp_data->maxrtt && tp->t_rttupdated >= HTCP_MIN_RTT_SAMPLES)
		htcp_data->maxrtt = tp->t_srtt;
}

/*
 * Perform any necesary tasks before we enter fast recovery
 */
void
htcp_pre_fr(struct tcpcb *tp, struct tcphdr *th)
{
	struct htcp *htcp_data = CC_DATA(tp);
	
	htcp_ssthresh_update(tp);
	
	/*
	 * grab the current time and record it so we know when the most recent
	 * congestion event was
	 */
	htcp_data->t_last_cong = ticks;
	
	/* reset rttmax to ensure reductions in the rtt become visible */
	htcp_data->maxrtt = (htcp_data->minrtt + (htcp_data->maxrtt - htcp_data->minrtt) * 95) / 100;
	
	/*
	 * record the current cwnd so it can be used as the basis for resetting
	 * cwnd after exiting fr
	 */
	htcp_data->prev_cwnd = tp->snd_cwnd;
}

/*
 * Decrease cwnd in the event of packet loss.
 */
void
htcp_post_fr(struct tcpcb *tp, struct tcphdr *th)
{
	u_int cwnd_in_pkts;
	struct htcp *htcp_data = CC_DATA(tp);

	/*
	 * if inflight data is less than ssthresh, set cwnd conservatively to avoid a burst of data, as
	 * suggested in the NewReno RFC. Otherwise, use the HTCP method.
	 */
	if (th && SEQ_GT(th->th_ack + tp->snd_ssthresh, tp->snd_max))
		tp->snd_cwnd = tp->snd_max - th->th_ack + tp->t_maxseg;
	else
	{
		/* update cwnd as a function of beta ensure that it never falls below 1 MSS */
		cwnd_in_pkts = htcp_data->prev_cwnd / tp->t_maxseg;
		tp->snd_cwnd = max(1,((htcp_data->beta * cwnd_in_pkts) >> HTCP_SHIFT)) * tp->t_maxseg;
	}
}

/*
 * Increase cwnd on the arrival of an ACK.
 */
void
htcp_ack_received(struct tcpcb *tp, struct tcphdr *th)
{
	struct htcp *htcp_data = CC_DATA(tp);
	u_int cwnd_in_pkts, incr;
	
	htcp_record_rtt(tp);
	htcp_recalc_beta(tp);
	htcp_recalc_alpha(tp); 
	
	/*
	 * when alpha equals 1 or we're in slow-start, fall back to newreno increase function.
	 * Alpha will equal 1 for the first HTCP_DELTA_L ticks after the flow starts and after congestion
	 */
	if (htcp_data->alpha == 1 || tp->snd_cwnd < tp->snd_ssthresh)
		newreno_cc_algo.ack_received(tp, th);
	else
	{
		/*
		 * increase cwnd as a function of the alpha value, restricting
		 * it to the maximum window sized advertised by the other host
		 * This cap is identical to the one used in the newreno code
		 */
		cwnd_in_pkts = tp->snd_cwnd / tp->t_maxseg;
		incr = (((htcp_data->alpha << HTCP_SHIFT) / cwnd_in_pkts) * tp->t_maxseg) >> HTCP_SHIFT;
		tp->snd_cwnd = min(tp->snd_cwnd + incr, TCP_MAXWIN << tp->snd_scale);
	}
}

/*
 * Reset the cwnd after a retransmission timeout
 */
void
htcp_after_timeout(struct tcpcb *tp)
{
	struct htcp *htcp_data = CC_DATA(tp);

	htcp_ssthresh_update(tp);

	/*
	 * grab the current time and record it so we know when the most recent
	 * congestion event was. Only record it when the timeout has fired more
	 * than once, as there is a reasonable chance the first one is a false alarm
	 * and may not indicate congestion.
	 */
	if (tp->t_rxtshift >= 2)
		htcp_data->t_last_cong = ticks;

	newreno_cc_algo.after_timeout(tp);
}

/*
 * Update the ssthresh in the event of congestion.
 */
void
htcp_ssthresh_update(struct tcpcb *tp)
{
	struct htcp *htcp_data = CC_DATA(tp);

	/*
	 * on the first congestion event, set ssthresh to cwnd * 0.5, on
	 * subsequent congestion events, set it to cwnd * beta.
	 */
	if (tp->snd_ssthresh == TCP_MAXWIN << TCP_MAX_WINSHIFT)
		tp->snd_ssthresh = (tp->snd_cwnd * HTCP_MINBETA) >> HTCP_SHIFT;
	else
		tp->snd_ssthresh = (tp->snd_cwnd * htcp_data->beta) >> HTCP_SHIFT;
}


static int
htcp_rtt_scaling_handler(SYSCTL_HANDLER_ARGS)
{
	if(req->newptr == NULL)
		goto skip;

	/* if the value passed in isn't 0 or 1, return an error */
	if(CAST_PTR_INT(req->newptr) != 0 && CAST_PTR_INT(req->newptr) != 1)
		return 1;

skip:
	return sysctl_handle_int(oidp, arg1, arg2, req);
}

static int
htcp_adaptive_backoff_handler(SYSCTL_HANDLER_ARGS)
{
	if(req->newptr == NULL)
		goto skip;

	/* if the value passed in isn't 0 or 1, return an error */
	if(CAST_PTR_INT(req->newptr) != 0 && CAST_PTR_INT(req->newptr) != 1)
		return 1;

skip:
	return sysctl_handle_int(oidp, arg1, arg2, req);
}

#ifdef HTCP_DEBUG
static int
htcp_debug_ticks_handler(SYSCTL_HANDLER_ARGS)
{
	if(req->newptr == NULL)
		goto skip;

	/* if the value passed in is less than 1 */
	if(CAST_PTR_INT(req->newptr) < 1)
		return 1;

skip:
	return sysctl_handle_int(oidp, arg1, arg2, req);
}
#endif


int
htcp_mod_init(void)
{

	htcp_cc_algo.after_idle = newreno_cc_algo.after_idle;

	/*
	 * the maximum time in ticks after a congestion event before alpha stops
	 * increasing, due to the risk of overflow.
	 * see comment above HTCP_CALC_ALPHA for more info
	 */
	htcp_max_diff = (~((u_long)0)) / ((1 << HTCP_ALPHA_INC_SHIFT) * 10);

	/*
	 * HTCP_RTT_REF is defined in ms, and t_srtt in the tcpcb is stored in 
	 * units of TCP_RTT_SCALE*hz.
	 * We perform the following calculation to ensure htcp_rtt_ref
	 * is in the same units as t_srtt.
	 */
	htcp_rtt_ref = (HTCP_RTT_REF * TCP_RTT_SCALE * hz) / 1000;

#ifdef HTCP_DEBUG
	/* set the default debug interval to 1 second */
	htcp_debug_ticks = hz;
#endif

	return 0;
}

SYSCTL_DECL(_net_inet_tcp_cc_htcp);
SYSCTL_NODE(_net_inet_tcp_cc, OID_AUTO, htcp, CTLFLAG_RW, NULL, "H-TCP related settings");
SYSCTL_OID(_net_inet_tcp_cc_htcp, OID_AUTO, rtt_scaling, CTLTYPE_UINT|CTLFLAG_RW, &htcp_rtt_scaling, 0, &htcp_rtt_scaling_handler, "IU", "switch H-TCP RTT scaling on/off");
SYSCTL_OID(_net_inet_tcp_cc_htcp, OID_AUTO, adaptive_backoff, CTLTYPE_UINT|CTLFLAG_RW, &htcp_adaptive_backoff, 0, &htcp_adaptive_backoff_handler, "IU", "switch H-TCP adaptive backoff on/off");

#ifdef HTCP_DEBUG
SYSCTL_OID(_net_inet_tcp_cc_htcp, OID_AUTO, debug_ticks, CTLTYPE_UINT|CTLFLAG_RW, &htcp_debug_ticks, 0, &htcp_debug_ticks_handler, "IU", "set the approximate number of ticks between printing debug messages to syslog");
#endif

DECLARE_CC_MODULE(htcp, &htcp_cc_algo);
