/*-
 * Copyright (c) 2009-2010
 * 	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by David Hayes and Lawrence Stewart,
 * made possible in part by grants from the FreeBSD Foundation and
 * Cisco University Research Program Fund at Community Foundation
 * Silicon Valley.
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
#include <sys/limits.h>
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
#include <netinet/h_ertt.h>i
#include <netinet/helper.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#define CAST_PTR_INT(X) (*((int*)(X)))

int hd_mod_init(void);
void hd_pre_fr(struct tcpcb *tp, struct tcphdr *th);
void hd_post_fr(struct tcpcb *tp, struct tcphdr *th);
void hd_ack_received(struct tcpcb *tp, struct tcphdr *th);


struct cc_algo hd_cc_algo = {
	.name = "hd",
	.mod_init = hd_mod_init,
	.ack_received = hd_ack_received
	/* the rest behaves as newreno */
	/* XXXLAS: Need to explicitly initialise to newreno funcs in mod_init */
};

static VNET_DEFINE(uint32_t, hd_qthresh);
static VNET_DEFINE(uint32_t, hd_qmin);
static VNET_DEFINE(uint32_t, hd_pmax);
static VNET_DEFINE(int, ertt_id);

#define V_hd_qthresh	VNET(hd_qthresh)
#define V_hd_qmin	VNET(hd_qmin)
#define V_hd_pmax	VNET(hd_pmax)
#define V_ertt_id	VNET(ertt_id)

static int
hd_qthresh_handler(SYSCTL_HANDLER_ARGS)
{
	if (req->newptr != NULL) {
		if (CAST_PTR_INT(req->newptr) < 1 || CAST_PTR_INT(req->newptr) <  V_hd_qmin)
			return (EINVAL);
	}
	return sysctl_handle_int(oidp, arg1, arg2, req);

	/* INIT_VNET_INET(TD_TO_VNET(req->td)); */
	/* int error, new; */

	/* new = V_hd_qthresh; */
	/* error = sysctl_handle_int(oidp, &new, 0, req); */
	/* if (error == 0 && req->newptr) { */
	/*   if (new*hz < 1000) /\* if less than kernel tick rate *\/ */
	/*     error = EINVAL; */
	/*   else */
	/*     V_hd_qthresh = new*hz/1000; /\* number of kernel ticks *\/ */
	/* } */
	/* return (error); */
}

static int
hd_qmin_handler(SYSCTL_HANDLER_ARGS)
{
	if (req->newptr != NULL) {
		if (CAST_PTR_INT(req->newptr) > V_hd_qthresh)
			return (EINVAL);
	}
	return sysctl_handle_int(oidp, arg1, arg2, req);

	/* INIT_VNET_INET(TD_TO_VNET(req->td)); */
	/* int error, new; */

	/* new = V_hd_qmin; */
	/* error = sysctl_handle_int(oidp, &new, 0, req); */
	/* if (error == 0 && req->newptr) { */
	/*   if (1000*new < hz) /\* if less than kernel tick rate *\/ */
	/*     error = EINVAL; */
	/*   else */
	/*     V_hd_qmin = new*hz/1000; /\* number of kernel ticks *\/ */
	/* } */
	/* return (error); */
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



/* Hamilto backoff function (see reference below) */
static int inline
prob_backoff_func(int Qdly, int maxQdly)
{
	int p;
	if (Qdly < V_hd_qthresh)
		p = INT_MAX / 100 *  V_hd_pmax 
			/ (V_hd_qthresh - V_hd_qmin)
			* (Qdly - V_hd_qmin);
	else
		if (Qdly > V_hd_qthresh)
			p = INT_MAX / 100 *  V_hd_pmax  
				/ (maxQdly - V_hd_qthresh)
				* (maxQdly - Qdly);
		else
			p = INT_MAX / 100 *  V_hd_pmax;
	return(p);
}

/* half cwnd backoff */
/* XXXLAS: I don't think we need this. */
static void inline
hd_congestion_exp(struct tcpcb *tp)
{
	u_int win, decr;
	win =  min(tp->snd_wnd, tp->snd_cwnd) / tp->t_maxseg;
	decr = win>>1;
	win -= decr;
	if (win < 2)
		win = 2;
	tp->snd_ssthresh = win * tp->t_maxseg;
	tp->snd_recover = tp->snd_max;
	if (tp->t_flags & TF_ECN_PERMIT)
		tp->t_flags |= TF_ECN_SND_CWR;
	tp->snd_cwnd = tp->snd_ssthresh;
}

/* Hamilton delay based congestion control detection and response */
	void
hd_ack_received(struct tcpcb *tp, struct tcphdr *th)
{ 

	struct ertt *e_t = (struct ertt *)get_helper_dblock(tp->dblocks,
	    tp->n_dblocks, V_ertt_id);

	if (e_t->rtt && e_t->minrtt && (V_hd_qthresh > 0)) {
		int Qdly = e_t->rtt - e_t->minrtt;
		if (Qdly > V_hd_qmin) {
			/* based on algorithm developed at the Hamilton Institute, Ireland
			   See Lukasz Budzisz, Rade Stanojevic, Robert Shorton and Fred Baker,
			   "A stratagy for fair coexistence of loss and delay-based congestion
			   control algorithms", to be published IEEE Communication Letters 2009 */
			int p;
			p = prob_backoff_func(Qdly, e_t->maxrtt - e_t->minrtt);
			if (random() < p) {
				hd_congestion_exp(tp); /* halve cwnd */
			}
		}
	}
}

int
hd_mod_init(void)
{
	V_ertt_id = get_helper_id("ertt");
	return (0);
}

SYSCTL_DECL(_net_inet_tcp_cc_hd);
SYSCTL_NODE(_net_inet_tcp_cc, OID_AUTO, hd, CTLFLAG_RW, NULL,
    "Hamilton delay-based congestion control related settings");

SYSCTL_OID(_net_inet_tcp_cc_hd, OID_AUTO, queue_threshold,
    CTLTYPE_UINT|CTLFLAG_RW, &V_hd_qthresh, 20,
    &hd_qthresh_handler, "IU",
    "Queueing congestion threshold in ticks");

SYSCTL_OID(_net_inet_tcp_cc_hd, OID_AUTO, pmax,
    CTLTYPE_UINT|CTLFLAG_RW, &V_hd_pmax, 5,
    &hd_pmax_handler, "IU",
    "Per packet maximum backoff probability as a percentage");

SYSCTL_OID(_net_inet_tcp_cc_hd, OID_AUTO, queue_min,
    CTLTYPE_UINT|CTLFLAG_RW, &V_hd_qmin, 5,
    &hd_qmin_handler, "IU",
    "Minimum queueing delay threshold in ticks");

DECLARE_CC_MODULE(hd, &hd_cc_algo);
MODULE_DEPEND(hd, ertt, 1, 1, 1);
