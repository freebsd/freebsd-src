/*-
 * Copyright (c) 2007-2008
 *	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2009-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2014 Midori Kato <katoon@sfc.wide.ad.jp>
 * Copyright (c) 2014 The FreeBSD Foundation
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

/*
 * An implementation of the DCTCP algorithm for FreeBSD, based on
 * "Data Center TCP (DCTCP)" by M. Alizadeh, A. Greenberg, D. A. Maltz,
 * J. Padhye, P. Patel, B. Prabhakar, S. Sengupta, and M. Sridharan.,
 * in ACM Conference on SIGCOMM 2010, New York, USA,
 * Originally released as the contribution of Microsoft Research project.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/vnet.h>

#include <net/route.h>
#include <net/route/nhop.h>

#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/cc/cc.h>
#include <netinet/cc/cc_module.h>

#define DCTCP_SHIFT 10
#define MAX_ALPHA_VALUE (1<<DCTCP_SHIFT)
VNET_DEFINE_STATIC(uint32_t, dctcp_alpha) = MAX_ALPHA_VALUE;
#define V_dctcp_alpha	    VNET(dctcp_alpha)
VNET_DEFINE_STATIC(uint32_t, dctcp_shift_g) = 4;
#define	V_dctcp_shift_g	    VNET(dctcp_shift_g)
VNET_DEFINE_STATIC(uint32_t, dctcp_slowstart) = 0;
#define	V_dctcp_slowstart   VNET(dctcp_slowstart)
VNET_DEFINE_STATIC(uint32_t, dctcp_ect1) = 0;
#define	V_dctcp_ect1	    VNET(dctcp_ect1)

struct dctcp {
	uint32_t bytes_ecn;	  /* # of marked bytes during a RTT */
	uint32_t bytes_total;	  /* # of acked bytes during a RTT */
	int      alpha;		  /* the fraction of marked bytes */
	int      ce_prev;	  /* CE state of the last segment */
	tcp_seq  save_sndnxt;	  /* end sequence number of the current window */
	int      ece_curr;	  /* ECE flag in this segment */
	int      ece_prev;	  /* ECE flag in the last segment */
	uint32_t num_cong_events; /* # of congestion events */
};

static void	dctcp_ack_received(struct cc_var *ccv, ccsignal_t type);
static void	dctcp_after_idle(struct cc_var *ccv);
static void	dctcp_cb_destroy(struct cc_var *ccv);
static int	dctcp_cb_init(struct cc_var *ccv, void *ptr);
static void	dctcp_cong_signal(struct cc_var *ccv, ccsignal_t type);
static void	dctcp_conn_init(struct cc_var *ccv);
static void	dctcp_post_recovery(struct cc_var *ccv);
static void	dctcp_ecnpkt_handler(struct cc_var *ccv);
static void	dctcp_update_alpha(struct cc_var *ccv);
static size_t	dctcp_data_sz(void);

struct cc_algo dctcp_cc_algo = {
	.name = "dctcp",
	.ack_received = dctcp_ack_received,
	.cb_destroy = dctcp_cb_destroy,
	.cb_init = dctcp_cb_init,
	.cong_signal = dctcp_cong_signal,
	.conn_init = dctcp_conn_init,
	.post_recovery = dctcp_post_recovery,
	.ecnpkt_handler = dctcp_ecnpkt_handler,
	.after_idle = dctcp_after_idle,
	.cc_data_sz = dctcp_data_sz,
};

static void
dctcp_ack_received(struct cc_var *ccv, ccsignal_t type)
{
	struct dctcp *dctcp_data;
	int bytes_acked = 0;
	uint32_t mss = tcp_fixed_maxseg(ccv->tp);

	dctcp_data = ccv->cc_data;

	if (CCV(ccv, t_flags2) & TF2_ECN_PERMIT) {
		/*
		 * DCTCP doesn't treat receipt of ECN marked packet as a
		 * congestion event. Thus, DCTCP always executes the ACK
		 * processing out of congestion recovery.
		 */
		if (IN_CONGRECOVERY(CCV(ccv, t_flags))) {
			EXIT_CONGRECOVERY(CCV(ccv, t_flags));
			newreno_cc_ack_received(ccv, type);
			ENTER_CONGRECOVERY(CCV(ccv, t_flags));
		} else
			newreno_cc_ack_received(ccv, type);

		if (type == CC_DUPACK)
			bytes_acked = min(ccv->bytes_this_ack, mss);

		if (type == CC_ACK)
			bytes_acked = ccv->bytes_this_ack;

		/* Update total bytes. */
		dctcp_data->bytes_total += bytes_acked;

		/* Update total marked bytes. */
		if (dctcp_data->ece_curr) {
			//XXRMS: For fluid-model DCTCP, update
			//cwnd here during for RTT fairness
			if (!dctcp_data->ece_prev
			    && bytes_acked > mss) {
				dctcp_data->bytes_ecn +=
				    (bytes_acked - mss);
			} else
				dctcp_data->bytes_ecn += bytes_acked;
			dctcp_data->ece_prev = 1;
		} else {
			if (dctcp_data->ece_prev
			    && bytes_acked > mss)
				dctcp_data->bytes_ecn += mss;
			dctcp_data->ece_prev = 0;
		}
		dctcp_data->ece_curr = 0;

		/*
		 * Update the fraction of marked bytes at the end of
		 * current window size.
		 */
		if (!IN_FASTRECOVERY(CCV(ccv, t_flags)) &&
		    SEQ_GT(ccv->curack, dctcp_data->save_sndnxt))
			dctcp_update_alpha(ccv);
	} else
		newreno_cc_ack_received(ccv, type);
}

static size_t
dctcp_data_sz(void)
{
	return (sizeof(struct dctcp));
}

static void
dctcp_after_idle(struct cc_var *ccv)
{
	struct dctcp *dctcp_data;

	if (CCV(ccv, t_flags2) & TF2_ECN_PERMIT) {
		dctcp_data = ccv->cc_data;

		/* Initialize internal parameters after idle time */
		dctcp_data->bytes_ecn = 0;
		dctcp_data->bytes_total = 0;
		dctcp_data->save_sndnxt = CCV(ccv, snd_nxt);
		dctcp_data->alpha = V_dctcp_alpha;
		dctcp_data->ece_curr = 0;
		dctcp_data->ece_prev = 0;
		dctcp_data->num_cong_events = 0;
	}

	newreno_cc_after_idle(ccv);
}

static void
dctcp_cb_destroy(struct cc_var *ccv)
{
	free(ccv->cc_data, M_CC_MEM);
}

static int
dctcp_cb_init(struct cc_var *ccv, void *ptr)
{
	struct dctcp *dctcp_data;

	INP_WLOCK_ASSERT(tptoinpcb(ccv->tp));
	if (ptr == NULL) {
		dctcp_data = malloc(sizeof(struct dctcp), M_CC_MEM, M_NOWAIT|M_ZERO);
		if (dctcp_data == NULL)
			return (ENOMEM);
	} else
		dctcp_data = ptr;
	/* Initialize some key variables with sensible defaults. */
	dctcp_data->bytes_ecn = 0;
	dctcp_data->bytes_total = 0;
	/*
	 * When alpha is set to 0 in the beginning, DCTCP sender transfers as
	 * much data as possible until the value converges which may expand the
	 * queueing delay at the switch. When alpha is set to 1, queueing delay
	 * is kept small.
	 * Throughput-sensitive applications should have alpha = 0
	 * Latency-sensitive applications should have alpha = 1
	 *
	 * Note: DCTCP draft suggests initial alpha to be 1 but we've decided to
	 * keep it 0 as default.
	 */
	dctcp_data->alpha = V_dctcp_alpha;
	dctcp_data->save_sndnxt = 0;
	dctcp_data->ce_prev = 0;
	dctcp_data->ece_curr = 0;
	dctcp_data->ece_prev = 0;
	dctcp_data->num_cong_events = 0;

	ccv->cc_data = dctcp_data;
	return (0);
}

/*
 * Perform any necessary tasks before we enter congestion recovery.
 */
static void
dctcp_cong_signal(struct cc_var *ccv, ccsignal_t type)
{
	struct dctcp *dctcp_data;
	uint32_t cwin, mss, pipe;

	if (CCV(ccv, t_flags2) & TF2_ECN_PERMIT) {
		dctcp_data = ccv->cc_data;
		cwin = CCV(ccv, snd_cwnd);
		mss = tcp_fixed_maxseg(ccv->tp);

		switch (type) {
		case CC_NDUPACK:
			if (!IN_FASTRECOVERY(CCV(ccv, t_flags))) {
				if (!IN_CONGRECOVERY(CCV(ccv, t_flags))) {
					CCV(ccv, snd_ssthresh) =
					    max(cwin / 2, 2 * mss);
					dctcp_data->num_cong_events++;
				} else {
					/* cwnd has already updated as congestion
					 * recovery. Reverse cwnd value using
					 * snd_cwnd_prev and recalculate snd_ssthresh
					 */
					cwin = CCV(ccv, snd_cwnd_prev);
					CCV(ccv, snd_ssthresh) =
					    max(cwin / 2, 2 * mss);
				}
				ENTER_RECOVERY(CCV(ccv, t_flags));
			}
			break;
		case CC_ECN:
			/*
			 * Save current snd_cwnd when the host encounters both
			 * congestion recovery and fast recovery.
			 */
			CCV(ccv, snd_cwnd_prev) = cwin;
			if (!IN_CONGRECOVERY(CCV(ccv, t_flags))) {
				if (V_dctcp_slowstart &&
				    dctcp_data->num_cong_events++ == 0) {
					CCV(ccv, snd_ssthresh) =
					    max(cwin / 2, 2 * mss);
					dctcp_data->alpha = MAX_ALPHA_VALUE;
					dctcp_data->bytes_ecn = 0;
					dctcp_data->bytes_total = 0;
					dctcp_data->save_sndnxt = CCV(ccv, snd_nxt);
				} else
					CCV(ccv, snd_ssthresh) =
					    max((cwin - (((uint64_t)cwin *
					    dctcp_data->alpha) >> (DCTCP_SHIFT+1))),
					    2 * mss);
				CCV(ccv, snd_cwnd) = CCV(ccv, snd_ssthresh);
				ENTER_CONGRECOVERY(CCV(ccv, t_flags));
			}
			dctcp_data->ece_curr = 1;
			break;
		case CC_RTO:
			if (CCV(ccv, t_rxtshift) == 1) {
				pipe = tcp_compute_pipe(ccv->tp);
				CCV(ccv, snd_ssthresh) = max(2,
					min(CCV(ccv, snd_wnd), pipe) / 2 / mss) * mss;
			}
			CCV(ccv, snd_cwnd) = mss;
			dctcp_update_alpha(ccv);
			dctcp_data->save_sndnxt += mss;
			dctcp_data->num_cong_events++;
			break;
		default:
			break;
		}
	} else
		newreno_cc_cong_signal(ccv, type);
}

static void
dctcp_conn_init(struct cc_var *ccv)
{
	struct dctcp *dctcp_data;

	dctcp_data = ccv->cc_data;

	if (CCV(ccv, t_flags2) & TF2_ECN_PERMIT) {
		dctcp_data->save_sndnxt = CCV(ccv, snd_nxt);
		if (V_dctcp_ect1)
			CCV(ccv, t_flags2) |= TF2_ECN_USE_ECT1;
	}
}

/*
 * Perform any necessary tasks before we exit congestion recovery.
 */
static void
dctcp_post_recovery(struct cc_var *ccv)
{
	newreno_cc_post_recovery(ccv);

	if (CCV(ccv, t_flags2) & TF2_ECN_PERMIT)
		dctcp_update_alpha(ccv);
}

/*
 * Execute an additional ECN processing using ECN field in IP header
 * and the CWR bit in TCP header.
 */
static void
dctcp_ecnpkt_handler(struct cc_var *ccv)
{
	struct dctcp *dctcp_data;
	uint32_t ccflag;
	int acknow;

	dctcp_data = ccv->cc_data;
	ccflag = ccv->flags;
	acknow = 0;

	/*
	 * DCTCP responds with an ACK immediately when the CE state
	 * in between this segment and the last segment has changed.
	 */
	if (ccflag & CCF_IPHDR_CE) {
		if (!dctcp_data->ce_prev) {
			acknow = 1;
			dctcp_data->ce_prev = 1;
			CCV(ccv, t_flags2) |= TF2_ECN_SND_ECE;
		}
	} else {
		if (dctcp_data->ce_prev) {
			acknow = 1;
			dctcp_data->ce_prev = 0;
			CCV(ccv, t_flags2) &= ~TF2_ECN_SND_ECE;
		}
	}

	if ((acknow) || (ccflag & CCF_TCPHDR_CWR)) {
		ccv->flags |= CCF_ACKNOW;
	} else {
		ccv->flags &= ~CCF_ACKNOW;
	}
}

/*
 * Update the fraction of marked bytes represented as 'alpha'.
 * Also initialize several internal parameters at the end of this function.
 */
static void
dctcp_update_alpha(struct cc_var *ccv)
{
	struct dctcp *dctcp_data;
	int alpha_prev;

	dctcp_data = ccv->cc_data;
	alpha_prev = dctcp_data->alpha;
	dctcp_data->bytes_total = max(dctcp_data->bytes_total, 1);

	/*
	 * Update alpha: alpha = (1 - g) * alpha + g * M.
	 * Here:
	 * g is weight factor
	 *	recommaded to be set to 1/16
	 *	small g = slow convergence between competitive DCTCP flows
	 *	large g = impacts low utilization of bandwidth at switches
	 * M is fraction of marked segments in last RTT
	 *	updated every RTT
	 * Alpha must be round to 0 - MAX_ALPHA_VALUE.
	 */
	dctcp_data->alpha = ulmin(alpha_prev - (alpha_prev >> V_dctcp_shift_g) +
	    ((uint64_t)dctcp_data->bytes_ecn << (DCTCP_SHIFT - V_dctcp_shift_g)) /
	    dctcp_data->bytes_total, MAX_ALPHA_VALUE);

	/* Initialize internal parameters for next alpha calculation */
	dctcp_data->bytes_ecn = 0;
	dctcp_data->bytes_total = 0;
	dctcp_data->save_sndnxt = CCV(ccv, snd_nxt);
}

static int
dctcp_alpha_handler(SYSCTL_HANDLER_ARGS)
{
	uint32_t new;
	int error;

	new = V_dctcp_alpha;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr != NULL) {
		if (new > MAX_ALPHA_VALUE)
			error = EINVAL;
		else
			V_dctcp_alpha = new;
	}

	return (error);
}

static int
dctcp_shift_g_handler(SYSCTL_HANDLER_ARGS)
{
	uint32_t new;
	int error;

	new = V_dctcp_shift_g;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr != NULL) {
		if (new > DCTCP_SHIFT)
			error = EINVAL;
		else
			V_dctcp_shift_g = new;
	}

	return (error);
}

static int
dctcp_slowstart_handler(SYSCTL_HANDLER_ARGS)
{
	uint32_t new;
	int error;

	new = V_dctcp_slowstart;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr != NULL) {
		if (new > 1)
			error = EINVAL;
		else
			V_dctcp_slowstart = new;
	}

	return (error);
}

SYSCTL_DECL(_net_inet_tcp_cc_dctcp);
SYSCTL_NODE(_net_inet_tcp_cc, OID_AUTO, dctcp,
    CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "dctcp congestion control related settings");

SYSCTL_PROC(_net_inet_tcp_cc_dctcp, OID_AUTO, alpha,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(dctcp_alpha), 0, &dctcp_alpha_handler, "IU",
    "dctcp alpha parameter at start of session");

SYSCTL_PROC(_net_inet_tcp_cc_dctcp, OID_AUTO, shift_g,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(dctcp_shift_g), 4, &dctcp_shift_g_handler, "IU",
    "dctcp shift parameter");

SYSCTL_PROC(_net_inet_tcp_cc_dctcp, OID_AUTO, slowstart,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(dctcp_slowstart), 0, &dctcp_slowstart_handler, "IU",
    "half CWND reduction after the first slow start");

SYSCTL_UINT(_net_inet_tcp_cc_dctcp, OID_AUTO, ect1,
    CTLFLAG_VNET | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(dctcp_ect1), 0,
    "Send DCTCP segments with ÍP ECT(0) or ECT(1)");

DECLARE_CC_MODULE(dctcp, &dctcp_cc_algo);
MODULE_VERSION(dctcp, 2);
