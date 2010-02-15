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
#include <netinet/h_ertt.h>
#include <netinet/helper.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#define CAST_PTR_INT(X) (*((int*)(X)))

int vegas_mod_init(void);
void vegas_conn_init(struct tcpcb *tp);
int vegas_cb_init(struct tcpcb *tp);
void vegas_cb_destroy(struct tcpcb *tp);
void vegas_ack_received(struct tcpcb *tp, struct tcphdr *th);
void vegas_pre_fr(struct tcpcb *tp, struct tcphdr *th);
void vegas_post_fr(struct tcpcb *tp, struct tcphdr *th);
void vegas_after_idle(struct tcpcb *tp);
void vegas_after_timeout(struct tcpcb *tp);

struct vegas {
	int rtt_ctr; /*counts rtts for vegas slow start */
};

MALLOC_DECLARE(M_VEGAS);
MALLOC_DEFINE(M_VEGAS, "vegas data",
    "Per connection data required for the VEGAS congestion algorithm");

/* function pointers for various hooks into the TCP stack */
struct cc_algo vegas_cc_algo = {
	.name = "vegas",
	.mod_init = vegas_mod_init,
	.cb_init = vegas_cb_init,
	.cb_destroy = vegas_cb_destroy,
	.ack_received = vegas_ack_received
};

static VNET_DEFINE(uint32_t, vegas_alpha);
static VNET_DEFINE(uint32_t, vegas_beta);
static VNET_DEFINE(int, ertt_id);
#define	V_vegas_alpha	VNET(vegas_alpha)
#define	V_vegas_beta	VNET(vegas_beta)
#define	V_ertt_id	VNET(ertt_id)


int
vegas_mod_init(void)
{
	V_vegas_alpha = 1;
	V_vegas_beta = 3;
	V_ertt_id = get_helper_id("ertt");
	vegas_cc_algo.pre_fr = newreno_cc_algo.pre_fr;
	vegas_cc_algo.post_fr = newreno_cc_algo.post_fr;
	vegas_cc_algo.after_idle = newreno_cc_algo.after_idle;
	vegas_cc_algo.after_timeout = newreno_cc_algo.after_timeout;

	return (0);
}

/* Create struct to store VEGAS specific data */
int
vegas_cb_init(struct tcpcb *tp)
{
	struct vegas *vegas_data;
	
	vegas_data = malloc(sizeof(struct vegas), M_VEGAS, M_NOWAIT);

	
	if (vegas_data == NULL)
		return (ENOMEM);
	
	vegas_data->rtt_ctr = 1;

	CC_DATA(tp) = vegas_data;
	

	return (0);
}

/*
 * Free the struct used to store VEGAS specific data for the specified
 * TCP control block.
 */
void
vegas_cb_destroy(struct tcpcb *tp)
{
	if (CC_DATA(tp) != NULL)
		free(CC_DATA(tp), M_VEGAS);
}

static int
vegas_alpha_handler(SYSCTL_HANDLER_ARGS)
{
	if (req->newptr != NULL) {
		if(CAST_PTR_INT(req->newptr) < 1 ||
		    CAST_PTR_INT(req->newptr) > V_vegas_beta)
			return (EINVAL);
	}

	return sysctl_handle_int(oidp, arg1, arg2, req);
}

static int
vegas_beta_handler(SYSCTL_HANDLER_ARGS)
{
	if (req->newptr != NULL) {
		if(CAST_PTR_INT(req->newptr) < 1 ||
		    CAST_PTR_INT(req->newptr) < V_vegas_alpha)
			return (EINVAL);
	}

	return sysctl_handle_int(oidp, arg1, arg2, req);
}

void
vegas_ack_received(struct tcpcb *tp, struct tcphdr *th)
{
	struct ertt *e_t = (struct ertt *) get_helper_dblock(tp->hdbs,
	    V_ertt_id);
	struct vegas *vegas_data = CC_DATA(tp);
	long expected_tx_rate, actual_tx_rate;

	if (e_t->flags & ERTT_NEW_MEASUREMENT) {

		expected_tx_rate = e_t->marked_snd_cwnd/e_t->minrtt;
		actual_tx_rate = e_t->bytes_tx_in_marked_rtt/e_t->markedpkt_rtt;

		long ndiff = (expected_tx_rate - actual_tx_rate)*e_t->minrtt/tp->t_maxseg;

		if (ndiff < V_vegas_alpha) {
			if (tp->snd_cwnd < tp->snd_ssthresh) {
				vegas_data->rtt_ctr += 1;
				if (vegas_data->rtt_ctr > 1) {
					newreno_cc_algo.ack_received(tp, th); /* reno slow start every second RTT */
					vegas_data->rtt_ctr = 0;
				}
			} else {
				tp->snd_cwnd = min(tp->snd_cwnd + tp->t_maxseg, TCP_MAXWIN<<tp->snd_scale);
			}
		} else if (ndiff > V_vegas_beta) {
			tp->snd_cwnd = max(2*tp->t_maxseg,tp->snd_cwnd-tp->t_maxseg);
			if (tp->snd_cwnd < tp->snd_ssthresh)
				tp->snd_ssthresh = tp->snd_cwnd; /* exit slow start */

		}
		e_t->flags &= ~ERTT_NEW_MEASUREMENT;
	}
}




SYSCTL_DECL(_net_inet_tcp_cc_vegas);
SYSCTL_NODE(_net_inet_tcp_cc, OID_AUTO, vegas, CTLFLAG_RW, NULL,
    "VEGAS related settings");

SYSCTL_OID(_net_inet_tcp_cc_vegas, OID_AUTO, vegas_alpha,
    CTLTYPE_UINT|CTLFLAG_RW, &V_vegas_alpha, 1,
    &vegas_alpha_handler, "IU",
    "vegas alpha parameter - Entered in terms of number \"buffers\" (0 < alpha < beta)");

SYSCTL_OID(_net_inet_tcp_cc_vegas, OID_AUTO, vegas_beta,
    CTLTYPE_UINT|CTLFLAG_RW, &V_vegas_beta, 3,
    &vegas_beta_handler, "IU",
    "vegas beta parameter - Entered in terms of number \"buffers\" (0 < alpha < beta)");

DECLARE_CC_MODULE(vegas, &vegas_cc_algo);
MODULE_DEPEND(vegas, ertt, 1, 1, 1);
