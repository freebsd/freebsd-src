/*-
 * Copyright (c) 2016-2018 Netflix, Inc.
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
 * Author: Lawrence Stewart <lstewart@netflix.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/arb.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/qmath.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#ifdef _KERNEL
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/systm.h>
#endif
#include <sys/stats.h>

#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>

#include <netinet/cc/cc.h>

VNET_DEFINE(int, tcp_perconn_stats_dflt_tpl) = -1;

#ifndef _KERNEL
#define	V_tcp_perconn_stats_enable	VNET(tcp_perconn_stats_enable)
#define	V_tcp_perconn_stats_dflt_tpl	VNET(tcp_perconn_stats_dflt_tpl)
#else /* _KERNEL */

VNET_DEFINE(int, tcp_perconn_stats_enable) = 2;
VNET_DEFINE_STATIC(struct stats_tpl_sample_rate *, tcp_perconn_stats_sample_rates);
VNET_DEFINE_STATIC(int, tcp_stats_nrates) = 0;
#define	V_tcp_perconn_stats_sample_rates VNET(tcp_perconn_stats_sample_rates)
#define	V_tcp_stats_nrates		VNET(tcp_stats_nrates)

static struct rmlock tcp_stats_tpl_sampling_lock;
static int tcp_stats_tpl_sr_cb(enum stats_tpl_sr_cb_action action,
    struct stats_tpl_sample_rate **rates, int *nrates, void *ctx);

SYSCTL_INT(_net_inet_tcp, OID_AUTO, perconn_stats_enable,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(tcp_perconn_stats_enable), 0,
    "Enable per-connection TCP stats gathering; 1 enables for all connections, "
    "2 enables random sampling across log id connection groups");
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, perconn_stats_sample_rates,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_NEEDGIANT, tcp_stats_tpl_sr_cb,
    sizeof(struct rm_priotracker), stats_tpl_sample_rates, "A",
    "TCP stats per template random sampling rates, in CSV tpl_spec=percent "
    "key-value pairs (see stats(9) for template spec details)");
#endif /* _KERNEL */

#ifdef _KERNEL
int
#else
static int
/* Ensure all templates are also added to the userland template list. */
__attribute__ ((constructor))
#endif
tcp_stats_init()
{
	int err, lasterr;

	err = lasterr = 0;

	V_tcp_perconn_stats_dflt_tpl = stats_tpl_alloc("TCP_DEFAULT", 0);
	if (V_tcp_perconn_stats_dflt_tpl < 0)
		return (-V_tcp_perconn_stats_dflt_tpl);

	struct voistatspec vss_sum[] = {
		STATS_VSS_SUM(),
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_TXPB, "TCP_TXPB", VSD_DTYPE_INT_U64,
	    NVSS(vss_sum), vss_sum, 0);
	lasterr = err ? err : lasterr;
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_RETXPB, "TCP_RETXPB", VSD_DTYPE_INT_U32,
	    NVSS(vss_sum), vss_sum, 0);
	lasterr = err ? err : lasterr;

	struct voistatspec vss_max[] = {
		STATS_VSS_MAX(),
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_FRWIN, "TCP_FRWIN", VSD_DTYPE_INT_ULONG,
	    NVSS(vss_max), vss_max, 0);
	lasterr = err ? err : lasterr;
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_LCWIN, "TCP_LCWIN", VSD_DTYPE_INT_ULONG,
	    NVSS(vss_max), vss_max, 0);
	lasterr = err ? err : lasterr;

	struct voistatspec vss_rtt[] = {
		STATS_VSS_MAX(),
		STATS_VSS_MIN(),
		STATS_VSS_TDGSTCLUST32(20, 4),
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_RTT, "TCP_RTT", VSD_DTYPE_INT_U32,
	    NVSS(vss_rtt), vss_rtt, 0);
	lasterr = err ? err : lasterr;

	struct voistatspec vss_congsig[] = {
		STATS_VSS_DVHIST32_USR(HBKTS(DVBKT(CC_ECN), DVBKT(CC_RTO),
		    DVBKT(CC_RTO_ERR), DVBKT(CC_NDUPACK)), 0)
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_CSIG, "TCP_CSIG", VSD_DTYPE_INT_U32,
	    NVSS(vss_congsig), vss_congsig, 0);
	lasterr = err ? err : lasterr;

	struct voistatspec vss_gput[] = {
		STATS_VSS_MAX(),
		STATS_VSS_TDGSTCLUST32(20, 4),
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_GPUT, "TCP_GPUT", VSD_DTYPE_INT_U32,
	    NVSS(vss_gput), vss_gput, 0);
	lasterr = err ? err : lasterr;

	struct voistatspec vss_gput_nd[] = {
		STATS_VSS_TDGSTCLUST32(10, 4),
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_GPUT_ND, "TCP_GPUT_ND", VSD_DTYPE_INT_S32,
	    NVSS(vss_gput_nd), vss_gput_nd, 0);
	lasterr = err ? err : lasterr;

	struct voistatspec vss_windiff[] = {
		STATS_VSS_CRHIST32_USR(HBKTS(CRBKT(0)), VSD_HIST_LBOUND_INF)
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_CALCFRWINDIFF, "TCP_CALCFRWINDIFF", VSD_DTYPE_INT_S32,
	    NVSS(vss_windiff), vss_windiff, 0);
	lasterr = err ? err : lasterr;

	struct voistatspec vss_acklen[] = {
		STATS_VSS_MAX(),
		STATS_VSS_CRHIST32_LIN(0, 9, 1, VSD_HIST_UBOUND_INF)
	};
	err |= stats_tpl_add_voistats(V_tcp_perconn_stats_dflt_tpl,
	    VOI_TCP_ACKLEN, "TCP_ACKLEN", VSD_DTYPE_INT_U32,
	    NVSS(vss_acklen), vss_acklen, 0);
	lasterr = err ? err : lasterr;

	return (lasterr);
}

#ifdef _KERNEL
int
tcp_stats_sample_rollthedice(struct tcpcb *tp, void *seed_bytes,
    size_t seed_len)
{
	struct rm_priotracker tracker;
	int tpl;

	tpl = -1;

	if (V_tcp_stats_nrates > 0) {
		rm_rlock(&tcp_stats_tpl_sampling_lock, &tracker);
		tpl = stats_tpl_sample_rollthedice(V_tcp_perconn_stats_sample_rates,
		    V_tcp_stats_nrates, seed_bytes, seed_len);
		rm_runlock(&tcp_stats_tpl_sampling_lock, &tracker);

		if (tpl >= 0) {
			INP_WLOCK_ASSERT(tp->t_inpcb);
			if (tp->t_stats != NULL)
				stats_blob_destroy(tp->t_stats);
			tp->t_stats = stats_blob_alloc(tpl, 0);
			if (tp->t_stats == NULL)
				tpl = -ENOMEM;
		}
	}

	return (tpl);
}

/*
 * Callback function for stats_tpl_sample_rates() to interact with the TCP
 * subsystem's stats template sample rates list.
 */
int
tcp_stats_tpl_sr_cb(enum stats_tpl_sr_cb_action action,
    struct stats_tpl_sample_rate **rates, int *nrates, void *ctx)
{
	struct stats_tpl_sample_rate *old_rates;
	int old_nrates;

	if (ctx == NULL)
		return (ENOMEM);

	switch (action) {
	case TPL_SR_RLOCKED_GET:
		/*
		 * Return with rlock held i.e. this call must be paired with a
		 * "action == TPL_SR_RUNLOCK" call.
		 */
		rm_assert(&tcp_stats_tpl_sampling_lock, RA_UNLOCKED);
		rm_rlock(&tcp_stats_tpl_sampling_lock,
		    (struct rm_priotracker *)ctx);
		/* FALLTHROUGH */
	case TPL_SR_UNLOCKED_GET:
		if (rates != NULL)
			*rates = V_tcp_perconn_stats_sample_rates;
		if (nrates != NULL)
			*nrates = V_tcp_stats_nrates;
		break;
	case TPL_SR_RUNLOCK:
		rm_assert(&tcp_stats_tpl_sampling_lock, RA_RLOCKED);
		rm_runlock(&tcp_stats_tpl_sampling_lock,
		    (struct rm_priotracker *)ctx);
		break;
	case TPL_SR_PUT:
		KASSERT(rates != NULL && nrates != NULL,
		    ("%s: PUT without new rates", __func__));
		rm_assert(&tcp_stats_tpl_sampling_lock, RA_UNLOCKED);
		if (rates == NULL || nrates == NULL)
			return (EINVAL);
		rm_wlock(&tcp_stats_tpl_sampling_lock);
		old_rates = V_tcp_perconn_stats_sample_rates;
		old_nrates = V_tcp_stats_nrates;
		V_tcp_perconn_stats_sample_rates = *rates;
		V_tcp_stats_nrates = *nrates;
		rm_wunlock(&tcp_stats_tpl_sampling_lock);
		*rates = old_rates;
		*nrates = old_nrates;
		break;
	default:
		return (EINVAL);
		break;
	}

	return (0);
}

RM_SYSINIT(tcp_stats_tpl_sampling_lock, &tcp_stats_tpl_sampling_lock,
    "tcp_stats_tpl_sampling_lock");
#endif /* _KERNEL */
