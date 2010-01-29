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
 * VEGAS
 *
 * An implementation of VEGAS congestion algorithm for FreeBSD.
 * The algorithm is based on the one described in "TCP Vegas: End to End
 * Congestion Avoidance on a Global Internet" by Lawrence S. Brakmo and Larry L.
 * Peterson.
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

int vegas_mod_init(void);
void vegas_pre_fr(struct tcpcb *tp, struct tcphdr *th);
void vegas_post_fr(struct tcpcb *tp, struct tcphdr *th);

/* function pointers for various hooks into the TCP stack */
struct cc_algo vegas_cc_algo = {
	.name = "vegas",
	.mod_init = vegas_mod_init,
	.pre_fr = vegas_pre_fr,
	.post_fr = vegas_post_fr,
};

static uint32_t vegas_alpha = 1;
static uint32_t vegas_beta = 3;

static int
vegas_alpha_handler(SYSCTL_HANDLER_ARGS)
{
	if(req->newptr != NULL) {
		if(CAST_PTR_INT(req->newptr) < 1 ||
		    CAST_PTR_INT(req->newptr) > vegas_beta)
			return (EINVAL);
	}

	return sysctl_handle_int(oidp, arg1, arg2, req);
}

static int
vegas_beta_handler(SYSCTL_HANDLER_ARGS)
{
	if(req->newptr != NULL) {
		if(CAST_PTR_INT(req->newptr) < 1 ||
		    CAST_PTR_INT(req->newptr) <= vegas_alpha)
			return (EINVAL);
	}

	return sysctl_handle_int(oidp, arg1, arg2, req);
}

void
vegas_post_fr(struct tcpcb *tp, struct tcphdr *th)
{
/*	struct enhanced_timing *e_t;
	struct rateinfo *r_i;
	e_t = &tp->e_t;
	r_i = &e_t->r_i;

	if (!IN_FASTRECOVERY(tp) && (tp->e_t.flags & DRCC_NEW_MEASUREMENT)) {

		long diff = r_i->expected_tx_rate - r_i->actual_tx_rate;
		if (diff < V_ratecc_vegas_alpha*tp->t_maxseg/e_t->minrtt)
			tp->snd_cwnd = min(tp->snd_cwnd + tp->t_maxseg, TCP_MAXWIN<<tp->snd_scale);
		else if (diff > V_ratecc_vegas_beta*tp->t_maxseg/e_t->minrtt)
			tp->snd_cwnd = max(2*tp->t_maxseg,tp->snd_cwnd-tp->t_maxseg);

		e_t->flags &= ~DRCC_NEW_MEASUREMENT;

	}
*/
}

void
vegas_pre_fr(struct tcpcb *tp, struct tcphdr *th)
{
	//EXIT_RATE_AVOID(tp);
	//EXIT_DELAYRATERECOVERY(tp);
}

int
vegas_mod_init(void)
{
	vegas_cc_algo.ack_received = newreno_cc_algo.ack_received;
	return (0);
}

SYSCTL_DECL(_net_inet_tcp_cc_vegas);
SYSCTL_NODE(_net_inet_tcp_cc, OID_AUTO, vegas, CTLFLAG_RW, NULL,
    "VEGAS related settings");

SYSCTL_OID(_net_inet_tcp_cc_vegas, OID_AUTO, vegas_alpha,
    CTLTYPE_UINT|CTLFLAG_RW, &vegas_alpha, 0,
    &vegas_alpha_handler, "IU",
    "vegas alpha parameter - Entered in terms of number \"buffers\" (0 < alpha < beta)");

SYSCTL_OID(_net_inet_tcp_cc_vegas, OID_AUTO, vegas_beta,
    CTLTYPE_UINT|CTLFLAG_RW, &vegas_beta, 0,
    &vegas_beta_handler, "IU",
    "vegas beta parameter - Entered in terms of number \"buffers\" (0 < alpha < beta)");

DECLARE_CC_MODULE(vegas, &vegas_cc_algo);
