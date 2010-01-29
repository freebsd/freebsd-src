/*-
 * Copyright (c) 2009-2010
 * 	Swinburne University of Technology, Melbourne, Australia
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by David Hayes and Lawrence Stewart,
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
 * Hamilton Delay-Based CC
 *
 * An implementation of the Hamilton Institute's delay based
 * congestion algorithm for FreeBSD.
 * The algorithm is based on the one described in "Delay-based AIMD congestion
 * control" by D. Leith, R. Shorten, G. McCullagh and J. Heffner.
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

#include <net/if.h>

#include <netinet/cc.h>
#include <netinet/cc_module.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#define CAST_PTR_INT(X) (*((int*)(X)))


struct cc_algo hd_cc_algo = {
	.name = "hd",
	.mod_init = hd_mod_init,
	.pre_fr = hd_pre_fr,
	.post_fr = hd_post_fr,
};

static int
hd_qthresh_handler(SYSCTL_HANDLER_ARGS)
{
	INIT_VNET_INET(TD_TO_VNET(req->td));
	int error, new;

	new = V_delaycc_queue_thresh;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
	  if (1000*new < hz) /* if less than kernel tick rate */
	    error = EINVAL;
	  else
	    V_delaycc_queue_thresh = new*hz/1000; /* number of kernel ticks */
	}
	return (error);
}

static int
hd_qmin_handler(SYSCTL_HANDLER_ARGS)
{
	if(req-V>newptr != NULL) {
		if(CAST_PTR_INT(req->newptr) < 1)
			return (EINVAL);
	}

	return sysctl_handle_int(oidp, arg1, arg2, req);
}


static int
hd_pmax_handler(SYSCTL_HANDLER_ARGS)
{
	if(req->newptr != NULL) {
		if(CAST_PTR_INT(req->newptr) == 0 ||
		    CAST_PTR_INT(req->newptr) > 100)
			return (EINVAL);
	}

	return sysctl_handle_int(oidp, arg1, arg2, req);
}


static int
hd_wnd_backoff_handler(SYSCTL_HANDLER_ARGS)
{
	if(req->newptr != NULL) {
		if(CAST_PTR_INT(req->newptr) == 0 ||
		    CAST_PTR_INT(req->newptr) > 100)
			return (EINVAL);
	}

	return sysctl_handle_int(oidp, arg1, arg2, req);
}

/* Modifications to impliment the Hamilton delay based congestion control
   algorithm -- David Hayes The key differences between delay_tcp_congestion_exp
   and tcp_congestion_exp are:

        1. instead of ssthresh being set to half cwnd, it is set to:

	       delta * minrtt/rtt * cwnd.

	The basic back off factor is the ratio between the current measured rtt
	and the lowest measured rtt.  To ensure a good minrtt measurment, this
	is modified by 0 < delta < 1 . Delta is the window_backoff_modifier/100.
	(see D.Leith, R.Shorten, J.Heffner, L.Dunn, F.Baker
	Delay-based AIMD Congestion Control, Proc. PFLDnet 2007)

        2. snd_cwnd = snd_ssthresh. Since no packet has been lost
	the normal fast recovery mechanism is not necessary.
*/
/* invbeta*8 for interger arithmetic */
static void inline
beta_tcp_congestion_exp(struct tcpcb *tp, int invbeta)
{
  u_int win;
  if (invbeta < 8 || invbeta > 16)
    invbeta=16; /* for safety, must reduce but not by more than 1/2 */
    
  win = min(tp->snd_wnd, tp->snd_cwnd) * 8/
    invbeta / tp->t_maxseg;
  if (win < 2)
    win = 2;
  tp->snd_ssthresh = win * tp->t_maxseg;
  tp->snd_recover = tp->snd_max;
  if (tp->t_flags & TF_ECN_PERMIT)
    tp->t_flags |= TF_ECN_SND_CWR;
  tp->snd_cwnd = tp->snd_ssthresh;
  ENTER_DELAYRATERECOVERY(tp);
}


/* Hamilto backoff function (see reference below) */
static int inline
prob_backoff_func(int Qdly, int maxQdly)
{
  int p;
  if (Qdly < V_delaycc_queue_thresh)
    p = INT_MAX / 100 *  V_delaycc_pmax 
      / (V_delaycc_queue_thresh - V_delaycc_queue_min)
      * (Qdly - V_delaycc_queue_min);
  else
    if (Qdly > V_delaycc_queue_thresh)
      p = INT_MAX / 100 *  V_delaycc_pmax  
      / (maxQdly - V_delaycc_queue_thresh)
	* (maxQdly - Qdly);
    else
      p = INT_MAX / 100 *  V_delaycc_pmax;
  return(p);
}

/* half cwnd backoff - David Hayes */V
static void inline
tcp_congestion_exp(struct tcpcb *tp)
{
  u_int win, decr;
  win = tp->snd_cwnd/tp->t_maxseg;
  decr = win>>2;
  win -= decr;
  if (win < 2)
    win = 2;
  tp->snd_ssthresh = win * tp->t_maxseg;
  tp->snd_recover = tp->snd_max;
  if (tp->t_flags & TF_ECN_PERMIT)
    tp->t_flags |= TF_ECN_SND_CWR;
  tp->snd_cwnd = tp->snd_ssthresh;
  ENTER_DELAYRATERECOVERY(tp);
}

/* Hamilton delay based congestion control detection and response
 David Hayes*/
void
hamilton_delay_congestion(struct tcpcb *tp)
{ 
  if (!IN_DELAYRATERECOVERY(tp) && !IN_FASTRECOVERY(tp)) {		
    struct enhanced_timing *e_t;
    e_t = &tp->e_t;
    
    
    if (e_t->rtt && e_t->minrtt && V_delaycc_window_backoff_modifier 
	&& (V_delaycc_queue_thresh > 0)) {
      int Qdly = e_t->rtt - e_t->minrtt;
      if (mod_tests & HD_ProbBackoff && (Qdly > V_delaycc_queue_min)) {
	/* based on algorithm developed at the Hamilton Institute, Ireland
	   See Lukasz Budzisz, Rade Stanojevic, Robert Shorton and Fred Baker,
	   "A stratagy for fair coexistence of loss and delay-based congestion
	   control algorithms", to be published IEEE Communication Letters 2009 */
	int p;
	p = prob_backoff_func(Qdly, e_t->maxrtt - e_t->minrtt);
	if (random() < p) {
	  tcp_congestion_exp(tp); /* halve cwnd */
	}
      } else {
	/* test for congestion using measured rtt as an indicator */
	if ((e_t->rtt - e_t->minrtt) > V_delaycc_queue_thresh) {
	  /* 8 factor to add precision */
	  int invbeta = e_t->rtt *800 / e_t->minrtt / V_delaycc_window_backoff_modifier;
	  beta_tcp_congestion_exp(tp, invbeta);
	}
      }
    }
  }
}

int
hd_mod_init(void)
{
	hd_cc_algo.ack_received = newreno_cc_algo.ack_received;
	return (0);
}

SYSCTL_DECL(_net_inet_tcp_cc_hd);
SYSCTL_NODE(_net_inet_tcp_cc, OID_AUTO, hd, CTLFLAG_RW, NULL,
    "Hamilton delay-based congestion control related settings");

SYSCTL_OID(_net_inet_tcp_cc_hd, OID_AUTO, window_backoff_modifier,
    CTLTYPE_UINT|CTLFLAG_RW, &hd_wnd_backoff_modifier, 0,
    &hd_wnd_backoff_handler, "IU",
    "percentage - When Hamilton delay based congestion control is used, this sets the percent modification to the multiplicative decrease factor");

SYSCTL_OID(_net_inet_tcp_cc_hd, OID_AUTO, queue_threshold
    CTLTYPE_UINT|CTLFLAG_RW, &hd_qthresh, 0,
    &hd_qthresh_handler, "IU",
    "Entered in milliseconds, but converted to kernel ticks - When Hamilton delay based congestion control is used, this sets the queueing congestion threshold");

SYSCTL_OID(_net_inet_tcp_cc_hd, OID_AUTO, pmax
    CTLTYPE_UINT|CTLFLAG_RW, &hd_pmax, 0,
    &hd_pmax_handler, "IU",
    "percentage - When Hamilton delay based congestion control is used, this sets the minimum queueing delay for the probabilistic backoff function");

SYSCTL_OID(_net_inet_tcp_cc_hd, OID_AUTO, queue_min
    CTLTYPE_UINT|CTLFLAG_RW, &hd_qmin, 0,
    &hd_qmin_handler, "IU",
    "Entered in milliseconds, but converted to kernel ticks - When Hamilton delay based congestion control is used, this sets the minimum queueing delay for the probabilistic backoff function");

DECLARE_CC_MODULE(hd, &hd_cc_algo);
