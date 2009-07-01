/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2007-2009
 *	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2009 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/vimage.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/cc.h>
#include <netinet/cc_module.h>
#include <netinet/tcp_seq.h>
#include <netinet/vinet.h>

/*
 * NewReno CC functions
 */
void	newreno_ack_received(struct tcpcb *tp, struct tcphdr *th);
void	newreno_ssthresh_update(struct tcpcb *tp, struct tcphdr *th);
void	newreno_post_fr(struct tcpcb *tp, struct tcphdr *th);
void	newreno_after_idle(struct tcpcb *tp);
void	newreno_after_timeout(struct tcpcb *tp);

/* newreno cc function pointers */
struct cc_algo newreno_cc_algo = {
	.name = "newreno",
	.ack_received = newreno_ack_received,
	.pre_fr = newreno_ssthresh_update,
	.post_fr = newreno_post_fr,
	.after_idle = newreno_after_idle,
	.after_timeout = newreno_after_timeout
};

/*
 * increase cwnd on receipt of a successful ACK
 * if cwnd <= ssthresh, increases by 1 MSS per ACK
 * if cwnd > ssthresh, increase by ~1 MSS per RTT
 */
void
newreno_ack_received(struct tcpcb *tp, struct tcphdr *th)
{
	u_int cw = tp->snd_cwnd;
	u_int incr = tp->t_maxseg;

	/*
	 * If cwnd <= ssthresh, open exponentially (maxseg per packet).
	 * Otherwise, open linearly (approx. maxseg per RTT
	 * i.e. maxseg^2 / cwnd per ACK received).
	 * If cwnd > maxseg^2, fix the cwnd increment at 1 byte
	 * to avoid capping cwnd (as suggested in RFC2581).
	 */
	if (cw > tp->snd_ssthresh)
		incr = max((incr * incr / cw), 1);

	tp->snd_cwnd = min(cw+incr, TCP_MAXWIN<<tp->snd_scale);
}

/*
 * update ssthresh to approx 1/2 of cwnd
 * argument "th" is unsued but required so that the function can
 * masquerade as a pre_fr hook function
 */
void
newreno_ssthresh_update(struct tcpcb *tp, struct tcphdr *th)
{
	u_int win;

	/* reset ssthresh */
	win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;

	if (win < 2)
		win = 2;

	tp->snd_ssthresh = win * tp->t_maxseg;
}

/*
 * decrease the cwnd in response to packet loss or a transmit timeout.
 * th can be null, in which case cwnd will be set according to reno instead
 * of new reno.
 */
void
newreno_post_fr(struct tcpcb *tp, struct tcphdr *th)
{
	/*
	* Out of fast recovery.
	* Window inflation should have left us
	* with approximately snd_ssthresh
	* outstanding data.
	* But in case we would be inclined to
	* send a burst, better to do it via
	* the slow start mechanism.
	*/
	if (th && SEQ_GT(th->th_ack + tp->snd_ssthresh, tp->snd_max))
		tp->snd_cwnd = tp->snd_max - th->th_ack + tp->t_maxseg;
	else
		tp->snd_cwnd = tp->snd_ssthresh;
}

/*
 * if a connection has been idle for a while and more data is ready to be sent,
 * reset cwnd
 */
void
newreno_after_idle(struct tcpcb *tp)
{
	/*
	* We have been idle for "a while" and no acks are
	* expected to clock out any data we send --
	* slow start to get ack "clock" running again.
	*
	* Set the slow-start flight size depending on whether
	* this is a local network or not.
	*
	* Set the slow-start flight size depending on whether
	* this is a local network or not.
	*/
	int ss = V_ss_fltsz;

#ifdef INET6
	if (isipv6) {
		if (in6_localaddr(&tp->t_inpcb->in6p_faddr))
			ss = V_ss_fltsz_local;
	} else
#endif /* INET6 */

	if (in_localaddr(tp->t_inpcb->inp_faddr))
		ss = V_ss_fltsz_local;

	tp->snd_cwnd = tp->t_maxseg * ss;
}

/*
 * reset the cwnd after a transmission timeout.
 */
void
newreno_after_timeout(struct tcpcb *tp)
{
	newreno_ssthresh_update(tp, NULL);

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
	tp->snd_cwnd = tp->t_maxseg;
}

DECLARE_CC_MODULE(newreno, &newreno_cc_algo);
