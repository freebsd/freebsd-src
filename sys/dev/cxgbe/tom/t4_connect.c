/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_inet.h"
#include "opt_inet6.h"

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>
#include <netinet/cc/cc.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "t4_clip.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

/*
 * Active open succeeded.
 */
static int
do_act_establish(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_act_establish *cpl = (const void *)(rss + 1);
	u_int tid = GET_TID(cpl);
	u_int atid = G_TID_TID(ntohl(cpl->tos_atid));
	struct toepcb *toep = lookup_atid(sc, atid);
	struct inpcb *inp = toep->inp;

	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == atid, ("%s: toep tid/atid mismatch", __func__));

	CTR3(KTR_CXGBE, "%s: atid %u, tid %u", __func__, atid, tid);
	free_atid(sc, atid);

	CURVNET_SET(toep->vnet);
	INP_WLOCK(inp);
	toep->tid = tid;
	insert_tid(sc, tid, toep, inp->inp_vflag & INP_IPV6 ? 2 : 1);
	if (inp->inp_flags & INP_DROPPED) {

		/* socket closed by the kernel before hw told us it connected */

		send_flowc_wr(toep, NULL);
		send_reset(sc, toep, be32toh(cpl->snd_isn));
		goto done;
	}

	make_established(toep, be32toh(cpl->snd_isn) - 1,
	    be32toh(cpl->rcv_isn) - 1, cpl->tcp_opt);
	inp->inp_flowtype = M_HASHTYPE_OPAQUE;
	inp->inp_flowid = tid;

done:
	INP_WUNLOCK(inp);
	CURVNET_RESTORE();
	return (0);
}

void
act_open_failure_cleanup(struct adapter *sc, u_int atid, u_int status)
{
	struct toepcb *toep = lookup_atid(sc, atid);
	struct inpcb *inp = toep->inp;
	struct toedev *tod = &toep->td->tod;
	struct epoch_tracker et;

	free_atid(sc, atid);
	toep->tid = -1;

	CURVNET_SET(toep->vnet);
	if (status != EAGAIN)
		NET_EPOCH_ENTER(et);
	INP_WLOCK(inp);
	toe_connect_failed(tod, inp, status);
	final_cpl_received(toep);	/* unlocks inp */
	if (status != EAGAIN)
		NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
}

/*
 * Active open failed.
 */
static int
do_act_open_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_act_open_rpl *cpl = (const void *)(rss + 1);
	u_int atid = G_TID_TID(G_AOPEN_ATID(be32toh(cpl->atid_status)));
	u_int status = G_AOPEN_STATUS(be32toh(cpl->atid_status));
	struct toepcb *toep = lookup_atid(sc, atid);
	int rc;

	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == atid, ("%s: toep tid/atid mismatch", __func__));

	CTR3(KTR_CXGBE, "%s: atid %u, status %u ", __func__, atid, status);

	/* Ignore negative advice */
	if (negative_advice(status))
		return (0);

	if (status && act_open_has_tid(status))
		release_tid(sc, GET_TID(cpl), toep->ctrlq);

	rc = act_open_rpl_status_to_errno(status);
	act_open_failure_cleanup(sc, atid, rc);

	return (0);
}

void
t4_init_connect_cpl_handlers(void)
{

	t4_register_cpl_handler(CPL_ACT_ESTABLISH, do_act_establish);
	t4_register_shared_cpl_handler(CPL_ACT_OPEN_RPL, do_act_open_rpl,
	    CPL_COOKIE_TOM);
}

void
t4_uninit_connect_cpl_handlers(void)
{

	t4_register_cpl_handler(CPL_ACT_ESTABLISH, NULL);
	t4_register_shared_cpl_handler(CPL_ACT_OPEN_RPL, NULL, CPL_COOKIE_TOM);
}

#ifdef KTR
#define DONT_OFFLOAD_ACTIVE_OPEN(x)	do { \
	reason = __LINE__; \
	rc = (x); \
	goto failed; \
} while (0)
#else
#define DONT_OFFLOAD_ACTIVE_OPEN(x)	do { \
	rc = (x); \
	goto failed; \
} while (0)
#endif

static inline int
act_open_cpl_size(struct adapter *sc, int isipv6)
{
	int idx;
	static const int sz_table[3][2] = {
		{
			sizeof (struct cpl_act_open_req),
			sizeof (struct cpl_act_open_req6)
		},
		{
			sizeof (struct cpl_t5_act_open_req),
			sizeof (struct cpl_t5_act_open_req6)
		},
		{
			sizeof (struct cpl_t6_act_open_req),
			sizeof (struct cpl_t6_act_open_req6)
		},
	};

	MPASS(chip_id(sc) >= CHELSIO_T4);
	idx = min(chip_id(sc) - CHELSIO_T4, 2);

	return (sz_table[idx][!!isipv6]);
}

/*
 * active open (soconnect).
 *
 * State of affairs on entry:
 * soisconnecting (so_state |= SS_ISCONNECTING)
 * tcbinfo not locked (This has changed - used to be WLOCKed)
 * inp WLOCKed
 * tp->t_state = TCPS_SYN_SENT
 * rtalloc1, RT_UNLOCK on rt.
 */
int
t4_connect(struct toedev *tod, struct socket *so, struct nhop_object *nh,
    struct sockaddr *nam)
{
	struct adapter *sc = tod->tod_softc;
	struct toepcb *toep = NULL;
	struct wrqe *wr = NULL;
	if_t rt_ifp = nh->nh_ifp;
	struct vi_info *vi;
	int qid_atid, rc, isipv6;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
#ifdef KTR
	int reason;
#endif
	struct offload_settings settings;
	struct epoch_tracker et;
	uint16_t vid = 0xfff, pcp = 0;

	INP_WLOCK_ASSERT(inp);
	KASSERT(nam->sa_family == AF_INET || nam->sa_family == AF_INET6,
	    ("%s: dest addr %p has family %u", __func__, nam, nam->sa_family));

	if (if_gettype(rt_ifp) == IFT_ETHER)
		vi = if_getsoftc(rt_ifp);
	else if (if_gettype(rt_ifp) == IFT_L2VLAN) {
		if_t ifp = VLAN_TRUNKDEV(rt_ifp);

		vi = if_getsoftc(ifp);
		VLAN_TAG(rt_ifp, &vid);
		VLAN_PCP(rt_ifp, &pcp);
	} else if (if_gettype(rt_ifp) == IFT_IEEE8023ADLAG)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOSYS); /* XXX: implement lagg+TOE */
	else
		DONT_OFFLOAD_ACTIVE_OPEN(ENOTSUP);
	if (sc->flags & KERN_TLS_ON)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOTSUP);

	rw_rlock(&sc->policy_lock);
	settings = *lookup_offload_policy(sc, OPEN_TYPE_ACTIVE, NULL,
	    EVL_MAKETAG(vid, pcp, 0), inp);
	rw_runlock(&sc->policy_lock);
	if (!settings.offload)
		DONT_OFFLOAD_ACTIVE_OPEN(EPERM);

	toep = alloc_toepcb(vi, M_NOWAIT);
	if (toep == NULL)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	toep->tid = alloc_atid(sc, toep);
	if (toep->tid < 0)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	toep->l2te = t4_l2t_get(vi->pi, rt_ifp,
	    nh->nh_flags & NHF_GATEWAY ? &nh->gw_sa : nam);
	if (toep->l2te == NULL)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	toep->vnet = so->so_vnet;
	init_conn_params(vi, &settings, &inp->inp_inc, so, NULL,
	    toep->l2te->idx, &toep->params);
	init_toepcb(vi, toep);

	isipv6 = nam->sa_family == AF_INET6;
	wr = alloc_wrqe(act_open_cpl_size(sc, isipv6), toep->ctrlq);
	if (wr == NULL)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	qid_atid = V_TID_QID(toep->ofld_rxq->iq.abs_id) | V_TID_TID(toep->tid) |
	    V_TID_COOKIE(CPL_COOKIE_TOM);

	if (isipv6) {
		struct cpl_act_open_req6 *cpl = wrtod(wr);
		struct cpl_t5_act_open_req6 *cpl5 = (void *)cpl;
		struct cpl_t6_act_open_req6 *cpl6 = (void *)cpl;

		if ((inp->inp_vflag & INP_IPV6) == 0)
			DONT_OFFLOAD_ACTIVE_OPEN(ENOTSUP);

		toep->ce = t4_get_clip_entry(sc, &inp->in6p_laddr, true);
		if (toep->ce == NULL)
			DONT_OFFLOAD_ACTIVE_OPEN(ENOENT);

		switch (chip_id(sc)) {
		case CHELSIO_T4:
			INIT_TP_WR(cpl, 0);
			cpl->params = select_ntuple(vi, toep->l2te);
			break;
		case CHELSIO_T5:
			INIT_TP_WR(cpl5, 0);
			cpl5->iss = htobe32(tp->iss);
			cpl5->params = select_ntuple(vi, toep->l2te);
			break;
		case CHELSIO_T6:
		default:
			INIT_TP_WR(cpl6, 0);
			cpl6->iss = htobe32(tp->iss);
			cpl6->params = select_ntuple(vi, toep->l2te);
			break;
		}
		OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ6,
		    qid_atid));
		cpl->local_port = inp->inp_lport;
		cpl->local_ip_hi = *(uint64_t *)&inp->in6p_laddr.s6_addr[0];
		cpl->local_ip_lo = *(uint64_t *)&inp->in6p_laddr.s6_addr[8];
		cpl->peer_port = inp->inp_fport;
		cpl->peer_ip_hi = *(uint64_t *)&inp->in6p_faddr.s6_addr[0];
		cpl->peer_ip_lo = *(uint64_t *)&inp->in6p_faddr.s6_addr[8];
		cpl->opt0 = calc_options0(vi, &toep->params);
		cpl->opt2 = calc_options2(vi, &toep->params);

		CTR6(KTR_CXGBE,
		    "%s: atid %u, toep %p, inp %p, opt0 %#016lx, opt2 %#08x",
		    __func__, toep->tid, toep, inp, be64toh(cpl->opt0),
		    be32toh(cpl->opt2));
	} else {
		struct cpl_act_open_req *cpl = wrtod(wr);
		struct cpl_t5_act_open_req *cpl5 = (void *)cpl;
		struct cpl_t6_act_open_req *cpl6 = (void *)cpl;

		switch (chip_id(sc)) {
		case CHELSIO_T4:
			INIT_TP_WR(cpl, 0);
			cpl->params = select_ntuple(vi, toep->l2te);
			break;
		case CHELSIO_T5:
			INIT_TP_WR(cpl5, 0);
			cpl5->iss = htobe32(tp->iss);
			cpl5->params = select_ntuple(vi, toep->l2te);
			break;
		case CHELSIO_T6:
		default:
			INIT_TP_WR(cpl6, 0);
			cpl6->iss = htobe32(tp->iss);
			cpl6->params = select_ntuple(vi, toep->l2te);
			break;
		}
		OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ,
		    qid_atid));
		inp_4tuple_get(inp, &cpl->local_ip, &cpl->local_port,
		    &cpl->peer_ip, &cpl->peer_port);
		cpl->opt0 = calc_options0(vi, &toep->params);
		cpl->opt2 = calc_options2(vi, &toep->params);

		CTR6(KTR_CXGBE,
		    "%s: atid %u, toep %p, inp %p, opt0 %#016lx, opt2 %#08x",
		    __func__, toep->tid, toep, inp, be64toh(cpl->opt0),
		    be32toh(cpl->opt2));
	}

	offload_socket(so, toep);
	NET_EPOCH_ENTER(et);
	rc = t4_l2t_send(sc, wr, toep->l2te);
	NET_EPOCH_EXIT(et);
	if (rc == 0) {
		toep->flags |= TPF_CPL_PENDING;
		return (0);
	}

	undo_offload_socket(so);
#if defined(KTR)
	reason = __LINE__;
#endif
failed:
	CTR3(KTR_CXGBE, "%s: not offloading (%d), rc %d", __func__, reason, rc);

	if (wr)
		free_wrqe(wr);

	if (toep) {
		if (toep->tid >= 0)
			free_atid(sc, toep->tid);
		if (toep->l2te)
			t4_l2t_release(toep->l2te);
		if (toep->ce)
			t4_release_clip_entry(sc, toep->ce);
		free_toepcb(toep);
	}

	return (rc);
}
#endif
