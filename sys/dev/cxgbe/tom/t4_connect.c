/*-
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
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

/* atid services */
static int alloc_atid(struct adapter *, void *);
static void *lookup_atid(struct adapter *, int);
static void free_atid(struct adapter *, int);

static int
alloc_atid(struct adapter *sc, void *ctx)
{
	struct tid_info *t = &sc->tids;
	int atid = -1;

	mtx_lock(&t->atid_lock);
	if (t->afree) {
		union aopen_entry *p = t->afree;

		atid = p - t->atid_tab;
		t->afree = p->next;
		p->data = ctx;
		t->atids_in_use++;
	}
	mtx_unlock(&t->atid_lock);
	return (atid);
}

static void *
lookup_atid(struct adapter *sc, int atid)
{
	struct tid_info *t = &sc->tids;

	return (t->atid_tab[atid].data);
}

static void
free_atid(struct adapter *sc, int atid)
{
	struct tid_info *t = &sc->tids;
	union aopen_entry *p = &t->atid_tab[atid];

	mtx_lock(&t->atid_lock);
	p->next = t->afree;
	t->afree = p;
	t->atids_in_use--;
	mtx_unlock(&t->atid_lock);
}

/*
 * Active open failed.
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

	INP_WLOCK(inp);
	toep->tid = tid;
	insert_tid(sc, tid, toep);
	if (inp->inp_flags & INP_DROPPED) {

		/* socket closed by the kernel before hw told us it connected */

		send_flowc_wr(toep, NULL);
		send_reset(sc, toep, be32toh(cpl->snd_isn));
		goto done;
	}

	make_established(toep, cpl->snd_isn, cpl->rcv_isn, cpl->tcp_opt);
done:
	INP_WUNLOCK(inp);
	return (0);
}

/*
 * Convert an ACT_OPEN_RPL status to an errno.
 */
static inline int
act_open_rpl_status_to_errno(int status)
{

	switch (status) {
	case CPL_ERR_CONN_RESET:
		return (ECONNREFUSED);
	case CPL_ERR_ARP_MISS:
		return (EHOSTUNREACH);
	case CPL_ERR_CONN_TIMEDOUT:
		return (ETIMEDOUT);
	case CPL_ERR_TCAM_FULL:
		return (EAGAIN);
	case CPL_ERR_CONN_EXIST:
		log(LOG_ERR, "ACTIVE_OPEN_RPL: 4-tuple in use\n");
		return (EAGAIN);
	default:
		return (EIO);
	}
}

void
act_open_failure_cleanup(struct adapter *sc, u_int atid, u_int status)
{
	struct toepcb *toep = lookup_atid(sc, atid);
	struct inpcb *inp = toep->inp;
	struct toedev *tod = &toep->td->tod;

	free_atid(sc, atid);
	toep->tid = -1;

	if (status != EAGAIN)
		INP_INFO_RLOCK(&V_tcbinfo);
	INP_WLOCK(inp);
	toe_connect_failed(tod, inp, status);
	final_cpl_received(toep);	/* unlocks inp */
	if (status != EAGAIN)
		INP_INFO_RUNLOCK(&V_tcbinfo);
}

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

/*
 * Options2 for active open.
 */
static uint32_t
calc_opt2a(struct socket *so, struct toepcb *toep)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct port_info *pi = toep->vi->pi;
	struct adapter *sc = pi->adapter;
	uint32_t opt2;

	opt2 = V_TX_QUEUE(sc->params.tp.tx_modq[pi->tx_chan]) |
	    F_RSS_QUEUE_VALID | V_RSS_QUEUE(toep->ofld_rxq->iq.abs_id);

	if (tp->t_flags & TF_SACK_PERMIT)
		opt2 |= F_SACK_EN;

	if (tp->t_flags & TF_REQ_TSTMP)
		opt2 |= F_TSTAMPS_EN;

	if (tp->t_flags & TF_REQ_SCALE)
		opt2 |= F_WND_SCALE_EN;

	if (V_tcp_do_ecn)
		opt2 |= F_CCTRL_ECN;

	/* RX_COALESCE is always a valid value (M_RX_COALESCE). */
	if (is_t4(sc))
		opt2 |= F_RX_COALESCE_VALID;
	else {
		opt2 |= F_T5_OPT_2_VALID;
		opt2 |= F_T5_ISS;
	}
	if (sc->tt.rx_coalesce)
		opt2 |= V_RX_COALESCE(M_RX_COALESCE);

#ifdef USE_DDP_RX_FLOW_CONTROL
	if (toep->ulp_mode == ULP_MODE_TCPDDP)
		opt2 |= F_RX_FC_VALID | F_RX_FC_DDP;
#endif

	return (htobe32(opt2));
}

void
t4_init_connect_cpl_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_ACT_ESTABLISH, do_act_establish);
	t4_register_cpl_handler(sc, CPL_ACT_OPEN_RPL, do_act_open_rpl);
}

#define DONT_OFFLOAD_ACTIVE_OPEN(x)	do { \
	reason = __LINE__; \
	rc = (x); \
	goto failed; \
} while (0)

static inline int
act_open_cpl_size(struct adapter *sc, int isipv6)
{
	static const int sz_t4[] = {
		sizeof (struct cpl_act_open_req),
		sizeof (struct cpl_act_open_req6)
	};
	static const int sz_t5[] = {
		sizeof (struct cpl_t5_act_open_req),
		sizeof (struct cpl_t5_act_open_req6)
	};

	if (is_t4(sc))
		return (sz_t4[!!isipv6]);
	else
		return (sz_t5[!!isipv6]);
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
t4_connect(struct toedev *tod, struct socket *so, struct rtentry *rt,
    struct sockaddr *nam)
{
	struct adapter *sc = tod->tod_softc;
	struct tom_data *td = tod_td(tod);
	struct toepcb *toep = NULL;
	struct wrqe *wr = NULL;
	struct ifnet *rt_ifp = rt->rt_ifp;
	struct vi_info *vi;
	int mtu_idx, rscale, qid_atid, rc, isipv6;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	int reason;

	INP_WLOCK_ASSERT(inp);
	KASSERT(nam->sa_family == AF_INET || nam->sa_family == AF_INET6,
	    ("%s: dest addr %p has family %u", __func__, nam, nam->sa_family));

	if (rt_ifp->if_type == IFT_ETHER)
		vi = rt_ifp->if_softc;
	else if (rt_ifp->if_type == IFT_L2VLAN) {
		struct ifnet *ifp = VLAN_COOKIE(rt_ifp);

		vi = ifp->if_softc;
	} else if (rt_ifp->if_type == IFT_IEEE8023ADLAG)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOSYS); /* XXX: implement lagg+TOE */
	else
		DONT_OFFLOAD_ACTIVE_OPEN(ENOTSUP);

	toep = alloc_toepcb(vi, -1, -1, M_NOWAIT);
	if (toep == NULL)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	toep->tid = alloc_atid(sc, toep);
	if (toep->tid < 0)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	toep->l2te = t4_l2t_get(vi->pi, rt_ifp,
	    rt->rt_flags & RTF_GATEWAY ? rt->rt_gateway : nam);
	if (toep->l2te == NULL)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	isipv6 = nam->sa_family == AF_INET6;
	wr = alloc_wrqe(act_open_cpl_size(sc, isipv6), toep->ctrlq);
	if (wr == NULL)
		DONT_OFFLOAD_ACTIVE_OPEN(ENOMEM);

	if (sc->tt.ddp && (so->so_options & SO_NO_DDP) == 0)
		set_tcpddp_ulp_mode(toep);
	else
		toep->ulp_mode = ULP_MODE_NONE;
	SOCKBUF_LOCK(&so->so_rcv);
	/* opt0 rcv_bufsiz initially, assumes its normal meaning later */
	toep->rx_credits = min(select_rcv_wnd(so) >> 10, M_RCV_BUFSIZ);
	SOCKBUF_UNLOCK(&so->so_rcv);

	/*
	 * The kernel sets request_r_scale based on sb_max whereas we need to
	 * take hardware's MAX_RCV_WND into account too.  This is normally a
	 * no-op as MAX_RCV_WND is much larger than the default sb_max.
	 */
	if (tp->t_flags & TF_REQ_SCALE)
		rscale = tp->request_r_scale = select_rcv_wscale();
	else
		rscale = 0;
	mtu_idx = find_best_mtu_idx(sc, &inp->inp_inc, 0);
	qid_atid = (toep->ofld_rxq->iq.abs_id << 14) | toep->tid;

	if (isipv6) {
		struct cpl_act_open_req6 *cpl = wrtod(wr);

		if ((inp->inp_vflag & INP_IPV6) == 0) {
			/* XXX think about this a bit more */
			log(LOG_ERR,
			    "%s: time to think about AF_INET6 + vflag 0x%x.\n",
			    __func__, inp->inp_vflag);
			DONT_OFFLOAD_ACTIVE_OPEN(ENOTSUP);
		}

		toep->ce = hold_lip(td, &inp->in6p_laddr);
		if (toep->ce == NULL)
			DONT_OFFLOAD_ACTIVE_OPEN(ENOENT);

		if (is_t4(sc)) {
			INIT_TP_WR(cpl, 0);
			cpl->params = select_ntuple(vi, toep->l2te);
		} else {
			struct cpl_t5_act_open_req6 *c5 = (void *)cpl;

			INIT_TP_WR(c5, 0);
			c5->iss = htobe32(tp->iss);
			c5->params = select_ntuple(vi, toep->l2te);
		}
		OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ6,
		    qid_atid));
		cpl->local_port = inp->inp_lport;
		cpl->local_ip_hi = *(uint64_t *)&inp->in6p_laddr.s6_addr[0];
		cpl->local_ip_lo = *(uint64_t *)&inp->in6p_laddr.s6_addr[8];
		cpl->peer_port = inp->inp_fport;
		cpl->peer_ip_hi = *(uint64_t *)&inp->in6p_faddr.s6_addr[0];
		cpl->peer_ip_lo = *(uint64_t *)&inp->in6p_faddr.s6_addr[8];
		cpl->opt0 = calc_opt0(so, vi, toep->l2te, mtu_idx, rscale,
		    toep->rx_credits, toep->ulp_mode);
		cpl->opt2 = calc_opt2a(so, toep);
	} else {
		struct cpl_act_open_req *cpl = wrtod(wr);

		if (is_t4(sc)) {
			INIT_TP_WR(cpl, 0);
			cpl->params = select_ntuple(vi, toep->l2te);
		} else {
			struct cpl_t5_act_open_req *c5 = (void *)cpl;

			INIT_TP_WR(c5, 0);
			c5->iss = htobe32(tp->iss);
			c5->params = select_ntuple(vi, toep->l2te);
		}
		OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ,
		    qid_atid));
		inp_4tuple_get(inp, &cpl->local_ip, &cpl->local_port,
		    &cpl->peer_ip, &cpl->peer_port);
		cpl->opt0 = calc_opt0(so, vi, toep->l2te, mtu_idx, rscale,
		    toep->rx_credits, toep->ulp_mode);
		cpl->opt2 = calc_opt2a(so, toep);
	}

	CTR5(KTR_CXGBE, "%s: atid %u (%s), toep %p, inp %p", __func__,
	    toep->tid, tcpstates[tp->t_state], toep, inp);

	offload_socket(so, toep);
	rc = t4_l2t_send(sc, wr, toep->l2te);
	if (rc == 0) {
		toep->flags |= TPF_CPL_PENDING;
		return (0);
	}

	undo_offload_socket(so);
	reason = __LINE__;
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
			release_lip(td, toep->ce);
		free_toepcb(toep);
	}

	return (rc);
}
#endif
