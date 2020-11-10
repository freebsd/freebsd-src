/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include "opt_kern_tls.h"
#include "opt_ratelimit.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/refcount.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/scope6_var.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>
#include <netinet/cc/cc.h>

#ifdef TCP_OFFLOAD
#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "common/t4_tcb.h"
#include "t4_clip.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"
#include "tom/t4_tls.h"

static struct protosw toe_protosw;
static struct pr_usrreqs toe_usrreqs;

static struct protosw toe6_protosw;
static struct pr_usrreqs toe6_usrreqs;

/* Module ops */
static int t4_tom_mod_load(void);
static int t4_tom_mod_unload(void);
static int t4_tom_modevent(module_t, int, void *);

/* ULD ops and helpers */
static int t4_tom_activate(struct adapter *);
static int t4_tom_deactivate(struct adapter *);

static struct uld_info tom_uld_info = {
	.uld_id = ULD_TOM,
	.activate = t4_tom_activate,
	.deactivate = t4_tom_deactivate,
};

static void release_offload_resources(struct toepcb *);
static int alloc_tid_tabs(struct tid_info *);
static void free_tid_tabs(struct tid_info *);
static void free_tom_data(struct adapter *, struct tom_data *);
static void reclaim_wr_resources(void *, int);

struct toepcb *
alloc_toepcb(struct vi_info *vi, int flags)
{
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct toepcb *toep;
	int tx_credits, txsd_total, len;

	/*
	 * The firmware counts tx work request credits in units of 16 bytes
	 * each.  Reserve room for an ABORT_REQ so the driver never has to worry
	 * about tx credits if it wants to abort a connection.
	 */
	tx_credits = sc->params.ofldq_wr_cred;
	tx_credits -= howmany(sizeof(struct cpl_abort_req), 16);

	/*
	 * Shortest possible tx work request is a fw_ofld_tx_data_wr + 1 byte
	 * immediate payload, and firmware counts tx work request credits in
	 * units of 16 byte.  Calculate the maximum work requests possible.
	 */
	txsd_total = tx_credits /
	    howmany(sizeof(struct fw_ofld_tx_data_wr) + 1, 16);

	len = offsetof(struct toepcb, txsd) +
	    txsd_total * sizeof(struct ofld_tx_sdesc);

	toep = malloc(len, M_CXGBE, M_ZERO | flags);
	if (toep == NULL)
		return (NULL);

	refcount_init(&toep->refcount, 1);
	toep->td = sc->tom_softc;
	toep->vi = vi;
	toep->tid = -1;
	toep->tx_total = tx_credits;
	toep->tx_credits = tx_credits;
	mbufq_init(&toep->ulp_pduq, INT_MAX);
	mbufq_init(&toep->ulp_pdu_reclaimq, INT_MAX);
	toep->txsd_total = txsd_total;
	toep->txsd_avail = txsd_total;
	toep->txsd_pidx = 0;
	toep->txsd_cidx = 0;
	aiotx_init_toep(toep);

	return (toep);
}

/*
 * Initialize a toepcb after its params have been filled out.
 */
int
init_toepcb(struct vi_info *vi, struct toepcb *toep)
{
	struct conn_params *cp = &toep->params;
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct tx_cl_rl_params *tc;

	if (cp->tc_idx >= 0 && cp->tc_idx < sc->chip_params->nsched_cls) {
		tc = &pi->sched_params->cl_rl[cp->tc_idx];
		mtx_lock(&sc->tc_lock);
		if (tc->flags & CLRL_ERR) {
			log(LOG_ERR,
			    "%s: failed to associate traffic class %u with tid %u\n",
			    device_get_nameunit(vi->dev), cp->tc_idx,
			    toep->tid);
			cp->tc_idx = -1;
		} else {
			tc->refcount++;
		}
		mtx_unlock(&sc->tc_lock);
	}
	toep->ofld_txq = &sc->sge.ofld_txq[cp->txq_idx];
	toep->ofld_rxq = &sc->sge.ofld_rxq[cp->rxq_idx];
	toep->ctrlq = &sc->sge.ctrlq[pi->port_id];

	tls_init_toep(toep);
	if (ulp_mode(toep) == ULP_MODE_TCPDDP)
		ddp_init_toep(toep);

	toep->flags |= TPF_INITIALIZED;

	return (0);
}

struct toepcb *
hold_toepcb(struct toepcb *toep)
{

	refcount_acquire(&toep->refcount);
	return (toep);
}

void
free_toepcb(struct toepcb *toep)
{

	if (refcount_release(&toep->refcount) == 0)
		return;

	KASSERT(!(toep->flags & TPF_ATTACHED),
	    ("%s: attached to an inpcb", __func__));
	KASSERT(!(toep->flags & TPF_CPL_PENDING),
	    ("%s: CPL pending", __func__));

	if (toep->flags & TPF_INITIALIZED) {
		if (ulp_mode(toep) == ULP_MODE_TCPDDP)
			ddp_uninit_toep(toep);
		tls_uninit_toep(toep);
	}
	free(toep, M_CXGBE);
}

/*
 * Set up the socket for TCP offload.
 */
void
offload_socket(struct socket *so, struct toepcb *toep)
{
	struct tom_data *td = toep->td;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct sockbuf *sb;

	INP_WLOCK_ASSERT(inp);

	/* Update socket */
	sb = &so->so_snd;
	SOCKBUF_LOCK(sb);
	sb->sb_flags |= SB_NOCOALESCE;
	SOCKBUF_UNLOCK(sb);
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	sb->sb_flags |= SB_NOCOALESCE;
	if (inp->inp_vflag & INP_IPV6)
		so->so_proto = &toe6_protosw;
	else
		so->so_proto = &toe_protosw;
	SOCKBUF_UNLOCK(sb);

	/* Update TCP PCB */
	tp->tod = &td->tod;
	tp->t_toe = toep;
	tp->t_flags |= TF_TOE;

	/* Install an extra hold on inp */
	toep->inp = inp;
	toep->flags |= TPF_ATTACHED;
	in_pcbref(inp);

	/* Add the TOE PCB to the active list */
	mtx_lock(&td->toep_list_lock);
	TAILQ_INSERT_HEAD(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);
}

/* This is _not_ the normal way to "unoffload" a socket. */
void
undo_offload_socket(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct toepcb *toep = tp->t_toe;
	struct tom_data *td = toep->td;
	struct sockbuf *sb;

	INP_WLOCK_ASSERT(inp);

	sb = &so->so_snd;
	SOCKBUF_LOCK(sb);
	sb->sb_flags &= ~SB_NOCOALESCE;
	SOCKBUF_UNLOCK(sb);
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	sb->sb_flags &= ~SB_NOCOALESCE;
	SOCKBUF_UNLOCK(sb);

	tp->tod = NULL;
	tp->t_toe = NULL;
	tp->t_flags &= ~TF_TOE;

	toep->inp = NULL;
	toep->flags &= ~TPF_ATTACHED;
	if (in_pcbrele_wlocked(inp))
		panic("%s: inp freed.", __func__);

	mtx_lock(&td->toep_list_lock);
	TAILQ_REMOVE(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);
}

static void
release_offload_resources(struct toepcb *toep)
{
	struct tom_data *td = toep->td;
	struct adapter *sc = td_adapter(td);
	int tid = toep->tid;

	KASSERT(!(toep->flags & TPF_CPL_PENDING),
	    ("%s: %p has CPL pending.", __func__, toep));
	KASSERT(!(toep->flags & TPF_ATTACHED),
	    ("%s: %p is still attached.", __func__, toep));

	CTR5(KTR_CXGBE, "%s: toep %p (tid %d, l2te %p, ce %p)",
	    __func__, toep, tid, toep->l2te, toep->ce);

	/*
	 * These queues should have been emptied at approximately the same time
	 * that a normal connection's socket's so_snd would have been purged or
	 * drained.  Do _not_ clean up here.
	 */
	MPASS(mbufq_len(&toep->ulp_pduq) == 0);
	MPASS(mbufq_len(&toep->ulp_pdu_reclaimq) == 0);
#ifdef INVARIANTS
	if (ulp_mode(toep) == ULP_MODE_TCPDDP)
		ddp_assert_empty(toep);
#endif
	MPASS(TAILQ_EMPTY(&toep->aiotx_jobq));

	if (toep->l2te)
		t4_l2t_release(toep->l2te);

	if (tid >= 0) {
		remove_tid(sc, tid, toep->ce ? 2 : 1);
		release_tid(sc, tid, toep->ctrlq);
	}

	if (toep->ce)
		t4_release_lip(sc, toep->ce);

	if (toep->params.tc_idx != -1)
		t4_release_cl_rl(sc, toep->vi->pi->port_id, toep->params.tc_idx);

	mtx_lock(&td->toep_list_lock);
	TAILQ_REMOVE(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);

	free_toepcb(toep);
}

/*
 * The kernel is done with the TCP PCB and this is our opportunity to unhook the
 * toepcb hanging off of it.  If the TOE driver is also done with the toepcb (no
 * pending CPL) then it is time to release all resources tied to the toepcb.
 *
 * Also gets called when an offloaded active open fails and the TOM wants the
 * kernel to take the TCP PCB back.
 */
static void
t4_pcb_detach(struct toedev *tod __unused, struct tcpcb *tp)
{
#if defined(KTR) || defined(INVARIANTS)
	struct inpcb *inp = tp->t_inpcb;
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);

	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));
	KASSERT(toep->flags & TPF_ATTACHED,
	    ("%s: not attached", __func__));

#ifdef KTR
	if (tp->t_state == TCPS_SYN_SENT) {
		CTR6(KTR_CXGBE, "%s: atid %d, toep %p (0x%x), inp %p (0x%x)",
		    __func__, toep->tid, toep, toep->flags, inp,
		    inp->inp_flags);
	} else {
		CTR6(KTR_CXGBE,
		    "t4_pcb_detach: tid %d (%s), toep %p (0x%x), inp %p (0x%x)",
		    toep->tid, tcpstates[tp->t_state], toep, toep->flags, inp,
		    inp->inp_flags);
	}
#endif

	tp->tod = NULL;
	tp->t_toe = NULL;
	tp->t_flags &= ~TF_TOE;
	toep->flags &= ~TPF_ATTACHED;

	if (!(toep->flags & TPF_CPL_PENDING))
		release_offload_resources(toep);
}

/*
 * setsockopt handler.
 */
static void
t4_ctloutput(struct toedev *tod, struct tcpcb *tp, int dir, int name)
{
	struct adapter *sc = tod->tod_softc;
	struct toepcb *toep = tp->t_toe;

	if (dir == SOPT_GET)
		return;

	CTR4(KTR_CXGBE, "%s: tp %p, dir %u, name %u", __func__, tp, dir, name);

	switch (name) {
	case TCP_NODELAY:
		if (tp->t_state != TCPS_ESTABLISHED)
			break;
		toep->params.nagle = tp->t_flags & TF_NODELAY ? 0 : 1;
		t4_set_tcb_field(sc, toep->ctrlq, toep, W_TCB_T_FLAGS,
		    V_TF_NAGLE(1), V_TF_NAGLE(toep->params.nagle), 0, 0);
		break;
	default:
		break;
	}
}

static inline uint64_t
get_tcb_tflags(const uint64_t *tcb)
{

	return ((be64toh(tcb[14]) << 32) | (be64toh(tcb[15]) >> 32));
}

static inline uint32_t
get_tcb_field(const uint64_t *tcb, u_int word, uint32_t mask, u_int shift)
{
#define LAST_WORD ((TCB_SIZE / 4) - 1)
	uint64_t t1, t2;
	int flit_idx;

	MPASS(mask != 0);
	MPASS(word <= LAST_WORD);
	MPASS(shift < 32);

	flit_idx = (LAST_WORD - word) / 2;
	if (word & 0x1)
		shift += 32;
	t1 = be64toh(tcb[flit_idx]) >> shift;
	t2 = 0;
	if (fls(mask) > 64 - shift) {
		/*
		 * Will spill over into the next logical flit, which is the flit
		 * before this one.  The flit_idx before this one must be valid.
		 */
		MPASS(flit_idx > 0);
		t2 = be64toh(tcb[flit_idx - 1]) << (64 - shift);
	}
	return ((t2 | t1) & mask);
#undef LAST_WORD
}
#define GET_TCB_FIELD(tcb, F) \
    get_tcb_field(tcb, W_TCB_##F, M_TCB_##F, S_TCB_##F)

/*
 * Issues a CPL_GET_TCB to read the entire TCB for the tid.
 */
static int
send_get_tcb(struct adapter *sc, u_int tid)
{
	struct cpl_get_tcb *cpl;
	struct wrq_cookie cookie;

	MPASS(tid < sc->tids.ntids);

	cpl = start_wrq_wr(&sc->sge.ctrlq[0], howmany(sizeof(*cpl), 16),
	    &cookie);
	if (__predict_false(cpl == NULL))
		return (ENOMEM);
	bzero(cpl, sizeof(*cpl));
	INIT_TP_WR(cpl, tid);
	OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_GET_TCB, tid));
	cpl->reply_ctrl = htobe16(V_REPLY_CHAN(0) |
	    V_QUEUENO(sc->sge.ofld_rxq[0].iq.cntxt_id));
	cpl->cookie = 0xff;
	commit_wrq_wr(&sc->sge.ctrlq[0], cpl, &cookie);

	return (0);
}

static struct tcb_histent *
alloc_tcb_histent(struct adapter *sc, u_int tid, int flags)
{
	struct tcb_histent *te;

	MPASS(flags == M_NOWAIT || flags == M_WAITOK);

	te = malloc(sizeof(*te), M_CXGBE, M_ZERO | flags);
	if (te == NULL)
		return (NULL);
	mtx_init(&te->te_lock, "TCB entry", NULL, MTX_DEF);
	callout_init_mtx(&te->te_callout, &te->te_lock, 0);
	te->te_adapter = sc;
	te->te_tid = tid;

	return (te);
}

static void
free_tcb_histent(struct tcb_histent *te)
{

	mtx_destroy(&te->te_lock);
	free(te, M_CXGBE);
}

/*
 * Start tracking the tid in the TCB history.
 */
int
add_tid_to_history(struct adapter *sc, u_int tid)
{
	struct tcb_histent *te = NULL;
	struct tom_data *td = sc->tom_softc;
	int rc;

	MPASS(tid < sc->tids.ntids);

	if (td->tcb_history == NULL)
		return (ENXIO);

	rw_wlock(&td->tcb_history_lock);
	if (td->tcb_history[tid] != NULL) {
		rc = EEXIST;
		goto done;
	}
	te = alloc_tcb_histent(sc, tid, M_NOWAIT);
	if (te == NULL) {
		rc = ENOMEM;
		goto done;
	}
	mtx_lock(&te->te_lock);
	rc = send_get_tcb(sc, tid);
	if (rc == 0) {
		te->te_flags |= TE_RPL_PENDING;
		td->tcb_history[tid] = te;
	} else {
		free(te, M_CXGBE);
	}
	mtx_unlock(&te->te_lock);
done:
	rw_wunlock(&td->tcb_history_lock);
	return (rc);
}

static void
remove_tcb_histent(struct tcb_histent *te)
{
	struct adapter *sc = te->te_adapter;
	struct tom_data *td = sc->tom_softc;

	rw_assert(&td->tcb_history_lock, RA_WLOCKED);
	mtx_assert(&te->te_lock, MA_OWNED);
	MPASS(td->tcb_history[te->te_tid] == te);

	td->tcb_history[te->te_tid] = NULL;
	free_tcb_histent(te);
	rw_wunlock(&td->tcb_history_lock);
}

static inline struct tcb_histent *
lookup_tcb_histent(struct adapter *sc, u_int tid, bool addrem)
{
	struct tcb_histent *te;
	struct tom_data *td = sc->tom_softc;

	MPASS(tid < sc->tids.ntids);

	if (td->tcb_history == NULL)
		return (NULL);

	if (addrem)
		rw_wlock(&td->tcb_history_lock);
	else
		rw_rlock(&td->tcb_history_lock);
	te = td->tcb_history[tid];
	if (te != NULL) {
		mtx_lock(&te->te_lock);
		return (te);	/* with both locks held */
	}
	if (addrem)
		rw_wunlock(&td->tcb_history_lock);
	else
		rw_runlock(&td->tcb_history_lock);

	return (te);
}

static inline void
release_tcb_histent(struct tcb_histent *te)
{
	struct adapter *sc = te->te_adapter;
	struct tom_data *td = sc->tom_softc;

	mtx_assert(&te->te_lock, MA_OWNED);
	mtx_unlock(&te->te_lock);
	rw_assert(&td->tcb_history_lock, RA_RLOCKED);
	rw_runlock(&td->tcb_history_lock);
}

static void
request_tcb(void *arg)
{
	struct tcb_histent *te = arg;

	mtx_assert(&te->te_lock, MA_OWNED);

	/* Noone else is supposed to update the histent. */
	MPASS(!(te->te_flags & TE_RPL_PENDING));
	if (send_get_tcb(te->te_adapter, te->te_tid) == 0)
		te->te_flags |= TE_RPL_PENDING;
	else
		callout_schedule(&te->te_callout, hz / 100);
}

static void
update_tcb_histent(struct tcb_histent *te, const uint64_t *tcb)
{
	struct tom_data *td = te->te_adapter->tom_softc;
	uint64_t tflags = get_tcb_tflags(tcb);
	uint8_t sample = 0;

	if (GET_TCB_FIELD(tcb, SND_MAX_RAW) != GET_TCB_FIELD(tcb, SND_UNA_RAW)) {
		if (GET_TCB_FIELD(tcb, T_RXTSHIFT) != 0)
			sample |= TS_RTO;
		if (GET_TCB_FIELD(tcb, T_DUPACKS) != 0)
			sample |= TS_DUPACKS;
		if (GET_TCB_FIELD(tcb, T_DUPACKS) >= td->dupack_threshold)
			sample |= TS_FASTREXMT;
	}

	if (GET_TCB_FIELD(tcb, SND_MAX_RAW) != 0) {
		uint32_t snd_wnd;

		sample |= TS_SND_BACKLOGGED;	/* for whatever reason. */

		snd_wnd = GET_TCB_FIELD(tcb, RCV_ADV);
		if (tflags & V_TF_RECV_SCALE(1))
			snd_wnd <<= GET_TCB_FIELD(tcb, RCV_SCALE);
		if (GET_TCB_FIELD(tcb, SND_CWND) < snd_wnd)
			sample |= TS_CWND_LIMITED;	/* maybe due to CWND */
	}

	if (tflags & V_TF_CCTRL_ECN(1)) {

		/*
		 * CE marker on incoming IP hdr, echoing ECE back in the TCP
		 * hdr.  Indicates congestion somewhere on the way from the peer
		 * to this node.
		 */
		if (tflags & V_TF_CCTRL_ECE(1))
			sample |= TS_ECN_ECE;

		/*
		 * ECE seen and CWR sent (or about to be sent).  Might indicate
		 * congestion on the way to the peer.  This node is reducing its
		 * congestion window in response.
		 */
		if (tflags & (V_TF_CCTRL_CWR(1) | V_TF_CCTRL_RFR(1)))
			sample |= TS_ECN_CWR;
	}

	te->te_sample[te->te_pidx] = sample;
	if (++te->te_pidx == nitems(te->te_sample))
		te->te_pidx = 0;
	memcpy(te->te_tcb, tcb, TCB_SIZE);
	te->te_flags |= TE_ACTIVE;
}

static int
do_get_tcb_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_get_tcb_rpl *cpl = mtod(m, const void *);
	const uint64_t *tcb = (const uint64_t *)(const void *)(cpl + 1);
	struct tcb_histent *te;
	const u_int tid = GET_TID(cpl);
	bool remove;

	remove = GET_TCB_FIELD(tcb, T_STATE) == TCPS_CLOSED;
	te = lookup_tcb_histent(sc, tid, remove);
	if (te == NULL) {
		/* Not in the history.  Who issued the GET_TCB for this? */
		device_printf(sc->dev, "tcb %u: flags 0x%016jx, state %u, "
		    "srtt %u, sscale %u, rscale %u, cookie 0x%x\n", tid,
		    (uintmax_t)get_tcb_tflags(tcb), GET_TCB_FIELD(tcb, T_STATE),
		    GET_TCB_FIELD(tcb, T_SRTT), GET_TCB_FIELD(tcb, SND_SCALE),
		    GET_TCB_FIELD(tcb, RCV_SCALE), cpl->cookie);
		goto done;
	}

	MPASS(te->te_flags & TE_RPL_PENDING);
	te->te_flags &= ~TE_RPL_PENDING;
	if (remove) {
		remove_tcb_histent(te);
	} else {
		update_tcb_histent(te, tcb);
		callout_reset(&te->te_callout, hz / 10, request_tcb, te);
		release_tcb_histent(te);
	}
done:
	m_freem(m);
	return (0);
}

static void
fill_tcp_info_from_tcb(struct adapter *sc, uint64_t *tcb, struct tcp_info *ti)
{
	uint32_t v;

	ti->tcpi_state = GET_TCB_FIELD(tcb, T_STATE);

	v = GET_TCB_FIELD(tcb, T_SRTT);
	ti->tcpi_rtt = tcp_ticks_to_us(sc, v);

	v = GET_TCB_FIELD(tcb, T_RTTVAR);
	ti->tcpi_rttvar = tcp_ticks_to_us(sc, v);

	ti->tcpi_snd_ssthresh = GET_TCB_FIELD(tcb, SND_SSTHRESH);
	ti->tcpi_snd_cwnd = GET_TCB_FIELD(tcb, SND_CWND);
	ti->tcpi_rcv_nxt = GET_TCB_FIELD(tcb, RCV_NXT);

	v = GET_TCB_FIELD(tcb, TX_MAX);
	ti->tcpi_snd_nxt = v - GET_TCB_FIELD(tcb, SND_NXT_RAW);

	/* Receive window being advertised by us. */
	ti->tcpi_rcv_wscale = GET_TCB_FIELD(tcb, SND_SCALE);	/* Yes, SND. */
	ti->tcpi_rcv_space = GET_TCB_FIELD(tcb, RCV_WND);

	/* Send window */
	ti->tcpi_snd_wscale = GET_TCB_FIELD(tcb, RCV_SCALE);	/* Yes, RCV. */
	ti->tcpi_snd_wnd = GET_TCB_FIELD(tcb, RCV_ADV);
	if (get_tcb_tflags(tcb) & V_TF_RECV_SCALE(1))
		ti->tcpi_snd_wnd <<= ti->tcpi_snd_wscale;
	else
		ti->tcpi_snd_wscale = 0;

}

static void
fill_tcp_info_from_history(struct adapter *sc, struct tcb_histent *te,
    struct tcp_info *ti)
{

	fill_tcp_info_from_tcb(sc, te->te_tcb, ti);
}

/*
 * Reads the TCB for the given tid using a memory window and copies it to 'buf'
 * in the same format as CPL_GET_TCB_RPL.
 */
static void
read_tcb_using_memwin(struct adapter *sc, u_int tid, uint64_t *buf)
{
	int i, j, k, rc;
	uint32_t addr;
	u_char *tcb, tmp;

	MPASS(tid < sc->tids.ntids);

	addr = t4_read_reg(sc, A_TP_CMM_TCB_BASE) + tid * TCB_SIZE;
	rc = read_via_memwin(sc, 2, addr, (uint32_t *)buf, TCB_SIZE);
	if (rc != 0)
		return;

	tcb = (u_char *)buf;
	for (i = 0, j = TCB_SIZE - 16; i < j; i += 16, j -= 16) {
		for (k = 0; k < 16; k++) {
			tmp = tcb[i + k];
			tcb[i + k] = tcb[j + k];
			tcb[j + k] = tmp;
		}
	}
}

static void
fill_tcp_info(struct adapter *sc, u_int tid, struct tcp_info *ti)
{
	uint64_t tcb[TCB_SIZE / sizeof(uint64_t)];
	struct tcb_histent *te;

	ti->tcpi_toe_tid = tid;
	te = lookup_tcb_histent(sc, tid, false);
	if (te != NULL) {
		fill_tcp_info_from_history(sc, te, ti);
		release_tcb_histent(te);
	} else {
		if (!(sc->debug_flags & DF_DISABLE_TCB_CACHE)) {
			/* XXX: tell firmware to flush TCB cache. */
		}
		read_tcb_using_memwin(sc, tid, tcb);
		fill_tcp_info_from_tcb(sc, tcb, ti);
	}
}

/*
 * Called by the kernel to allow the TOE driver to "refine" values filled up in
 * the tcp_info for an offloaded connection.
 */
static void
t4_tcp_info(struct toedev *tod, struct tcpcb *tp, struct tcp_info *ti)
{
	struct adapter *sc = tod->tod_softc;
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(tp->t_inpcb);
	MPASS(ti != NULL);

	fill_tcp_info(sc, toep->tid, ti);
}

#ifdef KERN_TLS
static int
t4_alloc_tls_session(struct toedev *tod, struct tcpcb *tp,
    struct ktls_session *tls, int direction)
{
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(tp->t_inpcb);
	MPASS(tls != NULL);

	return (tls_alloc_ktls(toep, tls, direction));
}
#endif

/*
 * The TOE driver will not receive any more CPLs for the tid associated with the
 * toepcb; release the hold on the inpcb.
 */
void
final_cpl_received(struct toepcb *toep)
{
	struct inpcb *inp = toep->inp;

	KASSERT(inp != NULL, ("%s: inp is NULL", __func__));
	INP_WLOCK_ASSERT(inp);
	KASSERT(toep->flags & TPF_CPL_PENDING,
	    ("%s: CPL not pending already?", __func__));

	CTR6(KTR_CXGBE, "%s: tid %d, toep %p (0x%x), inp %p (0x%x)",
	    __func__, toep->tid, toep, toep->flags, inp, inp->inp_flags);

	if (ulp_mode(toep) == ULP_MODE_TCPDDP)
		release_ddp_resources(toep);
	toep->inp = NULL;
	toep->flags &= ~TPF_CPL_PENDING;
	mbufq_drain(&toep->ulp_pdu_reclaimq);

	if (!(toep->flags & TPF_ATTACHED))
		release_offload_resources(toep);

	if (!in_pcbrele_wlocked(inp))
		INP_WUNLOCK(inp);
}

void
insert_tid(struct adapter *sc, int tid, void *ctx, int ntids)
{
	struct tid_info *t = &sc->tids;

	MPASS(tid >= t->tid_base);
	MPASS(tid - t->tid_base < t->ntids);

	t->tid_tab[tid - t->tid_base] = ctx;
	atomic_add_int(&t->tids_in_use, ntids);
}

void *
lookup_tid(struct adapter *sc, int tid)
{
	struct tid_info *t = &sc->tids;

	return (t->tid_tab[tid - t->tid_base]);
}

void
update_tid(struct adapter *sc, int tid, void *ctx)
{
	struct tid_info *t = &sc->tids;

	t->tid_tab[tid - t->tid_base] = ctx;
}

void
remove_tid(struct adapter *sc, int tid, int ntids)
{
	struct tid_info *t = &sc->tids;

	t->tid_tab[tid - t->tid_base] = NULL;
	atomic_subtract_int(&t->tids_in_use, ntids);
}

/*
 * What mtu_idx to use, given a 4-tuple.  Note that both s->mss and tcp_mssopt
 * have the MSS that we should advertise in our SYN.  Advertised MSS doesn't
 * account for any TCP options so the effective MSS (only payload, no headers or
 * options) could be different.
 */
static int
find_best_mtu_idx(struct adapter *sc, struct in_conninfo *inc,
    struct offload_settings *s)
{
	unsigned short *mtus = &sc->params.mtus[0];
	int i, mss, mtu;

	MPASS(inc != NULL);

	mss = s->mss > 0 ? s->mss : tcp_mssopt(inc);
	if (inc->inc_flags & INC_ISIPV6)
		mtu = mss + sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	else
		mtu = mss + sizeof(struct ip) + sizeof(struct tcphdr);

	for (i = 0; i < NMTUS - 1 && mtus[i + 1] <= mtu; i++)
		continue;

	return (i);
}

/*
 * Determine the receive window size for a socket.
 */
u_long
select_rcv_wnd(struct socket *so)
{
	unsigned long wnd;

	SOCKBUF_LOCK_ASSERT(&so->so_rcv);

	wnd = sbspace(&so->so_rcv);
	if (wnd < MIN_RCV_WND)
		wnd = MIN_RCV_WND;

	return min(wnd, MAX_RCV_WND);
}

int
select_rcv_wscale(void)
{
	int wscale = 0;
	unsigned long space = sb_max;

	if (space > MAX_RCV_WND)
		space = MAX_RCV_WND;

	while (wscale < TCP_MAX_WINSHIFT && (TCP_MAXWIN << wscale) < space)
		wscale++;

	return (wscale);
}

__be64
calc_options0(struct vi_info *vi, struct conn_params *cp)
{
	uint64_t opt0 = 0;

	opt0 |= F_TCAM_BYPASS;

	MPASS(cp->wscale >= 0 && cp->wscale <= M_WND_SCALE);
	opt0 |= V_WND_SCALE(cp->wscale);

	MPASS(cp->mtu_idx >= 0 && cp->mtu_idx < NMTUS);
	opt0 |= V_MSS_IDX(cp->mtu_idx);

	MPASS(cp->ulp_mode >= 0 && cp->ulp_mode <= M_ULP_MODE);
	opt0 |= V_ULP_MODE(cp->ulp_mode);

	MPASS(cp->opt0_bufsize >= 0 && cp->opt0_bufsize <= M_RCV_BUFSIZ);
	opt0 |= V_RCV_BUFSIZ(cp->opt0_bufsize);

	MPASS(cp->l2t_idx >= 0 && cp->l2t_idx < vi->adapter->vres.l2t.size);
	opt0 |= V_L2T_IDX(cp->l2t_idx);

	opt0 |= V_SMAC_SEL(vi->smt_idx);
	opt0 |= V_TX_CHAN(vi->pi->tx_chan);

	MPASS(cp->keepalive == 0 || cp->keepalive == 1);
	opt0 |= V_KEEP_ALIVE(cp->keepalive);

	MPASS(cp->nagle == 0 || cp->nagle == 1);
	opt0 |= V_NAGLE(cp->nagle);

	return (htobe64(opt0));
}

__be32
calc_options2(struct vi_info *vi, struct conn_params *cp)
{
	uint32_t opt2 = 0;
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;

	/*
	 * rx flow control, rx coalesce, congestion control, and tx pace are all
	 * explicitly set by the driver.  On T5+ the ISS is also set by the
	 * driver to the value picked by the kernel.
	 */
	if (is_t4(sc)) {
		opt2 |= F_RX_FC_VALID | F_RX_COALESCE_VALID;
		opt2 |= F_CONG_CNTRL_VALID | F_PACE_VALID;
	} else {
		opt2 |= F_T5_OPT_2_VALID;	/* all 4 valid */
		opt2 |= F_T5_ISS;		/* ISS provided in CPL */
	}

	MPASS(cp->sack == 0 || cp->sack == 1);
	opt2 |= V_SACK_EN(cp->sack);

	MPASS(cp->tstamp == 0 || cp->tstamp == 1);
	opt2 |= V_TSTAMPS_EN(cp->tstamp);

	if (cp->wscale > 0)
		opt2 |= F_WND_SCALE_EN;

	MPASS(cp->ecn == 0 || cp->ecn == 1);
	opt2 |= V_CCTRL_ECN(cp->ecn);

	/* XXX: F_RX_CHANNEL for multiple rx c-chan support goes here. */

	opt2 |= V_TX_QUEUE(sc->params.tp.tx_modq[pi->tx_chan]);
	opt2 |= V_PACE(0);
	opt2 |= F_RSS_QUEUE_VALID;
	opt2 |= V_RSS_QUEUE(sc->sge.ofld_rxq[cp->rxq_idx].iq.abs_id);

	MPASS(cp->cong_algo >= 0 && cp->cong_algo <= M_CONG_CNTRL);
	opt2 |= V_CONG_CNTRL(cp->cong_algo);

	MPASS(cp->rx_coalesce == 0 || cp->rx_coalesce == 1);
	if (cp->rx_coalesce == 1)
		opt2 |= V_RX_COALESCE(M_RX_COALESCE);

	opt2 |= V_RX_FC_DDP(0) | V_RX_FC_DISABLE(0);
#ifdef USE_DDP_RX_FLOW_CONTROL
	if (cp->ulp_mode == ULP_MODE_TCPDDP)
		opt2 |= F_RX_FC_DDP;
#endif

	return (htobe32(opt2));
}

uint64_t
select_ntuple(struct vi_info *vi, struct l2t_entry *e)
{
	struct adapter *sc = vi->adapter;
	struct tp_params *tp = &sc->params.tp;
	uint64_t ntuple = 0;

	/*
	 * Initialize each of the fields which we care about which are present
	 * in the Compressed Filter Tuple.
	 */
	if (tp->vlan_shift >= 0 && EVL_VLANOFTAG(e->vlan) != CPL_L2T_VLAN_NONE)
		ntuple |= (uint64_t)(F_FT_VLAN_VLD | e->vlan) << tp->vlan_shift;

	if (tp->port_shift >= 0)
		ntuple |= (uint64_t)e->lport << tp->port_shift;

	if (tp->protocol_shift >= 0)
		ntuple |= (uint64_t)IPPROTO_TCP << tp->protocol_shift;

	if (tp->vnic_shift >= 0 && tp->ingress_config & F_VNIC) {
		ntuple |= (uint64_t)(V_FT_VNID_ID_VF(vi->vin) |
		    V_FT_VNID_ID_PF(sc->pf) | V_FT_VNID_ID_VLD(vi->vfvld)) <<
		    tp->vnic_shift;
	}

	if (is_t4(sc))
		return (htobe32((uint32_t)ntuple));
	else
		return (htobe64(V_FILTER_TUPLE(ntuple)));
}

static int
is_tls_sock(struct socket *so, struct adapter *sc)
{
	struct inpcb *inp = sotoinpcb(so);
	int i, rc;

	/* XXX: Eventually add a SO_WANT_TLS socket option perhaps? */
	rc = 0;
	ADAPTER_LOCK(sc);
	for (i = 0; i < sc->tt.num_tls_rx_ports; i++) {
		if (inp->inp_lport == htons(sc->tt.tls_rx_ports[i]) ||
		    inp->inp_fport == htons(sc->tt.tls_rx_ports[i])) {
			rc = 1;
			break;
		}
	}
	ADAPTER_UNLOCK(sc);
	return (rc);
}

/*
 * Initialize various connection parameters.
 */
void
init_conn_params(struct vi_info *vi , struct offload_settings *s,
    struct in_conninfo *inc, struct socket *so,
    const struct tcp_options *tcpopt, int16_t l2t_idx, struct conn_params *cp)
{
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct tom_tunables *tt = &sc->tt;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	u_long wnd;

	MPASS(s->offload != 0);

	/* Congestion control algorithm */
	if (s->cong_algo >= 0)
		cp->cong_algo = s->cong_algo & M_CONG_CNTRL;
	else if (sc->tt.cong_algorithm >= 0)
		cp->cong_algo = tt->cong_algorithm & M_CONG_CNTRL;
	else {
		struct cc_algo *cc = CC_ALGO(tp);

		if (strcasecmp(cc->name, "reno") == 0)
			cp->cong_algo = CONG_ALG_RENO;
		else if (strcasecmp(cc->name, "tahoe") == 0)
			cp->cong_algo = CONG_ALG_TAHOE;
		if (strcasecmp(cc->name, "newreno") == 0)
			cp->cong_algo = CONG_ALG_NEWRENO;
		if (strcasecmp(cc->name, "highspeed") == 0)
			cp->cong_algo = CONG_ALG_HIGHSPEED;
		else {
			/*
			 * Use newreno in case the algorithm selected by the
			 * host stack is not supported by the hardware.
			 */
			cp->cong_algo = CONG_ALG_NEWRENO;
		}
	}

	/* Tx traffic scheduling class. */
	if (s->sched_class >= 0 &&
	    s->sched_class < sc->chip_params->nsched_cls) {
	    cp->tc_idx = s->sched_class;
	} else
	    cp->tc_idx = -1;

	/* Nagle's algorithm. */
	if (s->nagle >= 0)
		cp->nagle = s->nagle > 0 ? 1 : 0;
	else
		cp->nagle = tp->t_flags & TF_NODELAY ? 0 : 1;

	/* TCP Keepalive. */
	if (V_tcp_always_keepalive || so_options_get(so) & SO_KEEPALIVE)
		cp->keepalive = 1;
	else
		cp->keepalive = 0;

	/* Optimization that's specific to T5 @ 40G. */
	if (tt->tx_align >= 0)
		cp->tx_align =  tt->tx_align > 0 ? 1 : 0;
	else if (chip_id(sc) == CHELSIO_T5 &&
	    (port_top_speed(pi) > 10 || sc->params.nports > 2))
		cp->tx_align = 1;
	else
		cp->tx_align = 0;

	/* ULP mode. */
	if (can_tls_offload(sc) &&
	    (s->tls > 0 || (s->tls < 0 && is_tls_sock(so, sc))))
		cp->ulp_mode = ULP_MODE_TLS;
	else if (s->ddp > 0 ||
	    (s->ddp < 0 && sc->tt.ddp && (so_options_get(so) & SO_NO_DDP) == 0))
		cp->ulp_mode = ULP_MODE_TCPDDP;
	else
		cp->ulp_mode = ULP_MODE_NONE;

	/* Rx coalescing. */
	if (s->rx_coalesce >= 0)
		cp->rx_coalesce = s->rx_coalesce > 0 ? 1 : 0;
	else if (cp->ulp_mode == ULP_MODE_TLS)
		cp->rx_coalesce = 0;
	else if (tt->rx_coalesce >= 0)
		cp->rx_coalesce = tt->rx_coalesce > 0 ? 1 : 0;
	else
		cp->rx_coalesce = 1;	/* default */

	/*
	 * Index in the PMTU table.  This controls the MSS that we announce in
	 * our SYN initially, but after ESTABLISHED it controls the MSS that we
	 * use to send data.
	 */
	cp->mtu_idx = find_best_mtu_idx(sc, inc, s);

	/* Tx queue for this connection. */
	if (s->txq >= 0 && s->txq < vi->nofldtxq)
		cp->txq_idx = s->txq;
	else
		cp->txq_idx = arc4random() % vi->nofldtxq;
	cp->txq_idx += vi->first_ofld_txq;

	/* Rx queue for this connection. */
	if (s->rxq >= 0 && s->rxq < vi->nofldrxq)
		cp->rxq_idx = s->rxq;
	else
		cp->rxq_idx = arc4random() % vi->nofldrxq;
	cp->rxq_idx += vi->first_ofld_rxq;

	if (SOLISTENING(so)) {
		/* Passive open */
		MPASS(tcpopt != NULL);

		/* TCP timestamp option */
		if (tcpopt->tstamp &&
		    (s->tstamp > 0 || (s->tstamp < 0 && V_tcp_do_rfc1323)))
			cp->tstamp = 1;
		else
			cp->tstamp = 0;

		/* SACK */
		if (tcpopt->sack &&
		    (s->sack > 0 || (s->sack < 0 && V_tcp_do_sack)))
			cp->sack = 1;
		else
			cp->sack = 0;

		/* Receive window scaling. */
		if (tcpopt->wsf > 0 && tcpopt->wsf < 15 && V_tcp_do_rfc1323)
			cp->wscale = select_rcv_wscale();
		else
			cp->wscale = 0;

		/* ECN */
		if (tcpopt->ecn &&	/* XXX: review. */
		    (s->ecn > 0 || (s->ecn < 0 && V_tcp_do_ecn)))
			cp->ecn = 1;
		else
			cp->ecn = 0;

		wnd = max(so->sol_sbrcv_hiwat, MIN_RCV_WND);
		cp->opt0_bufsize = min(wnd >> 10, M_RCV_BUFSIZ);

		if (tt->sndbuf > 0)
			cp->sndbuf = tt->sndbuf;
		else if (so->sol_sbsnd_flags & SB_AUTOSIZE &&
		    V_tcp_do_autosndbuf)
			cp->sndbuf = 256 * 1024;
		else
			cp->sndbuf = so->sol_sbsnd_hiwat;
	} else {
		/* Active open */

		/* TCP timestamp option */
		if (s->tstamp > 0 ||
		    (s->tstamp < 0 && (tp->t_flags & TF_REQ_TSTMP)))
			cp->tstamp = 1;
		else
			cp->tstamp = 0;

		/* SACK */
		if (s->sack > 0 ||
		    (s->sack < 0 && (tp->t_flags & TF_SACK_PERMIT)))
			cp->sack = 1;
		else
			cp->sack = 0;

		/* Receive window scaling */
		if (tp->t_flags & TF_REQ_SCALE)
			cp->wscale = select_rcv_wscale();
		else
			cp->wscale = 0;

		/* ECN */
		if (s->ecn > 0 || (s->ecn < 0 && V_tcp_do_ecn == 1))
			cp->ecn = 1;
		else
			cp->ecn = 0;

		SOCKBUF_LOCK(&so->so_rcv);
		wnd = max(select_rcv_wnd(so), MIN_RCV_WND);
		SOCKBUF_UNLOCK(&so->so_rcv);
		cp->opt0_bufsize = min(wnd >> 10, M_RCV_BUFSIZ);

		if (tt->sndbuf > 0)
			cp->sndbuf = tt->sndbuf;
		else {
			SOCKBUF_LOCK(&so->so_snd);
			if (so->so_snd.sb_flags & SB_AUTOSIZE &&
			    V_tcp_do_autosndbuf)
				cp->sndbuf = 256 * 1024;
			else
				cp->sndbuf = so->so_snd.sb_hiwat;
			SOCKBUF_UNLOCK(&so->so_snd);
		}
	}

	cp->l2t_idx = l2t_idx;

	/* This will be initialized on ESTABLISHED. */
	cp->emss = 0;
}

int
negative_advice(int status)
{

	return (status == CPL_ERR_RTX_NEG_ADVICE ||
	    status == CPL_ERR_PERSIST_NEG_ADVICE ||
	    status == CPL_ERR_KEEPALV_NEG_ADVICE);
}

static int
alloc_tid_tab(struct tid_info *t, int flags)
{

	MPASS(t->ntids > 0);
	MPASS(t->tid_tab == NULL);

	t->tid_tab = malloc(t->ntids * sizeof(*t->tid_tab), M_CXGBE,
	    M_ZERO | flags);
	if (t->tid_tab == NULL)
		return (ENOMEM);
	atomic_store_rel_int(&t->tids_in_use, 0);

	return (0);
}

static void
free_tid_tab(struct tid_info *t)
{

	KASSERT(t->tids_in_use == 0,
	    ("%s: %d tids still in use.", __func__, t->tids_in_use));

	free(t->tid_tab, M_CXGBE);
	t->tid_tab = NULL;
}

static int
alloc_stid_tab(struct tid_info *t, int flags)
{

	MPASS(t->nstids > 0);
	MPASS(t->stid_tab == NULL);

	t->stid_tab = malloc(t->nstids * sizeof(*t->stid_tab), M_CXGBE,
	    M_ZERO | flags);
	if (t->stid_tab == NULL)
		return (ENOMEM);
	mtx_init(&t->stid_lock, "stid lock", NULL, MTX_DEF);
	t->stids_in_use = 0;
	TAILQ_INIT(&t->stids);
	t->nstids_free_head = t->nstids;

	return (0);
}

static void
free_stid_tab(struct tid_info *t)
{

	KASSERT(t->stids_in_use == 0,
	    ("%s: %d tids still in use.", __func__, t->stids_in_use));

	if (mtx_initialized(&t->stid_lock))
		mtx_destroy(&t->stid_lock);
	free(t->stid_tab, M_CXGBE);
	t->stid_tab = NULL;
}

static void
free_tid_tabs(struct tid_info *t)
{

	free_tid_tab(t);
	free_stid_tab(t);
}

static int
alloc_tid_tabs(struct tid_info *t)
{
	int rc;

	rc = alloc_tid_tab(t, M_NOWAIT);
	if (rc != 0)
		goto failed;

	rc = alloc_stid_tab(t, M_NOWAIT);
	if (rc != 0)
		goto failed;

	return (0);
failed:
	free_tid_tabs(t);
	return (rc);
}

static inline void
alloc_tcb_history(struct adapter *sc, struct tom_data *td)
{

	if (sc->tids.ntids == 0 || sc->tids.ntids > 1024)
		return;
	rw_init(&td->tcb_history_lock, "TCB history");
	td->tcb_history = malloc(sc->tids.ntids * sizeof(*td->tcb_history),
	    M_CXGBE, M_ZERO | M_NOWAIT);
	td->dupack_threshold = G_DUPACKTHRESH(t4_read_reg(sc, A_TP_PARA_REG0));
}

static inline void
free_tcb_history(struct adapter *sc, struct tom_data *td)
{
#ifdef INVARIANTS
	int i;

	if (td->tcb_history != NULL) {
		for (i = 0; i < sc->tids.ntids; i++) {
			MPASS(td->tcb_history[i] == NULL);
		}
	}
#endif
	free(td->tcb_history, M_CXGBE);
	if (rw_initialized(&td->tcb_history_lock))
		rw_destroy(&td->tcb_history_lock);
}

static void
free_tom_data(struct adapter *sc, struct tom_data *td)
{

	ASSERT_SYNCHRONIZED_OP(sc);

	KASSERT(TAILQ_EMPTY(&td->toep_list),
	    ("%s: TOE PCB list is not empty.", __func__));
	KASSERT(td->lctx_count == 0,
	    ("%s: lctx hash table is not empty.", __func__));

	t4_free_ppod_region(&td->pr);

	if (td->listen_mask != 0)
		hashdestroy(td->listen_hash, M_CXGBE, td->listen_mask);

	if (mtx_initialized(&td->unsent_wr_lock))
		mtx_destroy(&td->unsent_wr_lock);
	if (mtx_initialized(&td->lctx_hash_lock))
		mtx_destroy(&td->lctx_hash_lock);
	if (mtx_initialized(&td->toep_list_lock))
		mtx_destroy(&td->toep_list_lock);

	free_tcb_history(sc, td);
	free_tid_tabs(&sc->tids);
	free(td, M_CXGBE);
}

static char *
prepare_pkt(int open_type, uint16_t vtag, struct inpcb *inp, int *pktlen,
    int *buflen)
{
	char *pkt;
	struct tcphdr *th;
	int ipv6, len;
	const int maxlen =
	    max(sizeof(struct ether_header), sizeof(struct ether_vlan_header)) +
	    max(sizeof(struct ip), sizeof(struct ip6_hdr)) +
	    sizeof(struct tcphdr);

	MPASS(open_type == OPEN_TYPE_ACTIVE || open_type == OPEN_TYPE_LISTEN);

	pkt = malloc(maxlen, M_CXGBE, M_ZERO | M_NOWAIT);
	if (pkt == NULL)
		return (NULL);

	ipv6 = inp->inp_vflag & INP_IPV6;
	len = 0;

	if (EVL_VLANOFTAG(vtag) == 0xfff) {
		struct ether_header *eh = (void *)pkt;

		if (ipv6)
			eh->ether_type = htons(ETHERTYPE_IPV6);
		else
			eh->ether_type = htons(ETHERTYPE_IP);

		len += sizeof(*eh);
	} else {
		struct ether_vlan_header *evh = (void *)pkt;

		evh->evl_encap_proto = htons(ETHERTYPE_VLAN);
		evh->evl_tag = htons(vtag);
		if (ipv6)
			evh->evl_proto = htons(ETHERTYPE_IPV6);
		else
			evh->evl_proto = htons(ETHERTYPE_IP);

		len += sizeof(*evh);
	}

	if (ipv6) {
		struct ip6_hdr *ip6 = (void *)&pkt[len];

		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_nxt = IPPROTO_TCP;
		if (open_type == OPEN_TYPE_ACTIVE) {
			ip6->ip6_src = inp->in6p_laddr;
			ip6->ip6_dst = inp->in6p_faddr;
		} else if (open_type == OPEN_TYPE_LISTEN) {
			ip6->ip6_src = inp->in6p_laddr;
			ip6->ip6_dst = ip6->ip6_src;
		}

		len += sizeof(*ip6);
	} else {
		struct ip *ip = (void *)&pkt[len];

		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_tos = inp->inp_ip_tos;
		ip->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr));
		ip->ip_ttl = inp->inp_ip_ttl;
		ip->ip_p = IPPROTO_TCP;
		if (open_type == OPEN_TYPE_ACTIVE) {
			ip->ip_src = inp->inp_laddr;
			ip->ip_dst = inp->inp_faddr;
		} else if (open_type == OPEN_TYPE_LISTEN) {
			ip->ip_src = inp->inp_laddr;
			ip->ip_dst = ip->ip_src;
		}

		len += sizeof(*ip);
	}

	th = (void *)&pkt[len];
	if (open_type == OPEN_TYPE_ACTIVE) {
		th->th_sport = inp->inp_lport;	/* network byte order already */
		th->th_dport = inp->inp_fport;	/* ditto */
	} else if (open_type == OPEN_TYPE_LISTEN) {
		th->th_sport = inp->inp_lport;	/* network byte order already */
		th->th_dport = th->th_sport;
	}
	len += sizeof(th);

	*pktlen = *buflen = len;
	return (pkt);
}

const struct offload_settings *
lookup_offload_policy(struct adapter *sc, int open_type, struct mbuf *m,
    uint16_t vtag, struct inpcb *inp)
{
	const struct t4_offload_policy *op;
	char *pkt;
	struct offload_rule *r;
	int i, matched, pktlen, buflen;
	static const struct offload_settings allow_offloading_settings = {
		.offload = 1,
		.rx_coalesce = -1,
		.cong_algo = -1,
		.sched_class = -1,
		.tstamp = -1,
		.sack = -1,
		.nagle = -1,
		.ecn = -1,
		.ddp = -1,
		.tls = -1,
		.txq = -1,
		.rxq = -1,
		.mss = -1,
	};
	static const struct offload_settings disallow_offloading_settings = {
		.offload = 0,
		/* rest is irrelevant when offload is off. */
	};

	rw_assert(&sc->policy_lock, RA_LOCKED);

	/*
	 * If there's no Connection Offloading Policy attached to the device
	 * then we need to return a default static policy.  If
	 * "cop_managed_offloading" is true, then we need to disallow
	 * offloading until a COP is attached to the device.  Otherwise we
	 * allow offloading ...
	 */
	op = sc->policy;
	if (op == NULL) {
		if (sc->tt.cop_managed_offloading)
			return (&disallow_offloading_settings);
		else
			return (&allow_offloading_settings);
	}

	switch (open_type) {
	case OPEN_TYPE_ACTIVE:
	case OPEN_TYPE_LISTEN:
		pkt = prepare_pkt(open_type, vtag, inp, &pktlen, &buflen);
		break;
	case OPEN_TYPE_PASSIVE:
		MPASS(m != NULL);
		pkt = mtod(m, char *);
		MPASS(*pkt == CPL_PASS_ACCEPT_REQ);
		pkt += sizeof(struct cpl_pass_accept_req);
		pktlen = m->m_pkthdr.len - sizeof(struct cpl_pass_accept_req);
		buflen = m->m_len - sizeof(struct cpl_pass_accept_req);
		break;
	default:
		MPASS(0);
		return (&disallow_offloading_settings);
	}

	if (pkt == NULL || pktlen == 0 || buflen == 0)
		return (&disallow_offloading_settings);

	matched = 0;
	r = &op->rule[0];
	for (i = 0; i < op->nrules; i++, r++) {
		if (r->open_type != open_type &&
		    r->open_type != OPEN_TYPE_DONTCARE) {
			continue;
		}
		matched = bpf_filter(r->bpf_prog.bf_insns, pkt, pktlen, buflen);
		if (matched)
			break;
	}

	if (open_type == OPEN_TYPE_ACTIVE || open_type == OPEN_TYPE_LISTEN)
		free(pkt, M_CXGBE);

	return (matched ? &r->settings : &disallow_offloading_settings);
}

static void
reclaim_wr_resources(void *arg, int count)
{
	struct tom_data *td = arg;
	STAILQ_HEAD(, wrqe) twr_list = STAILQ_HEAD_INITIALIZER(twr_list);
	struct cpl_act_open_req *cpl;
	u_int opcode, atid, tid;
	struct wrqe *wr;
	struct adapter *sc = td_adapter(td);

	mtx_lock(&td->unsent_wr_lock);
	STAILQ_SWAP(&td->unsent_wr_list, &twr_list, wrqe);
	mtx_unlock(&td->unsent_wr_lock);

	while ((wr = STAILQ_FIRST(&twr_list)) != NULL) {
		STAILQ_REMOVE_HEAD(&twr_list, link);

		cpl = wrtod(wr);
		opcode = GET_OPCODE(cpl);

		switch (opcode) {
		case CPL_ACT_OPEN_REQ:
		case CPL_ACT_OPEN_REQ6:
			atid = G_TID_TID(be32toh(OPCODE_TID(cpl)));
			CTR2(KTR_CXGBE, "%s: atid %u ", __func__, atid);
			act_open_failure_cleanup(sc, atid, EHOSTUNREACH);
			free(wr, M_CXGBE);
			break;
		case CPL_PASS_ACCEPT_RPL:
			tid = GET_TID(cpl);
			CTR2(KTR_CXGBE, "%s: tid %u ", __func__, tid);
			synack_failure_cleanup(sc, tid);
			free(wr, M_CXGBE);
			break;
		default:
			log(LOG_ERR, "%s: leaked work request %p, wr_len %d, "
			    "opcode %x\n", __func__, wr, wr->wr_len, opcode);
			/* WR not freed here; go look at it with a debugger.  */
		}
	}
}

/*
 * Ground control to Major TOM
 * Commencing countdown, engines on
 */
static int
t4_tom_activate(struct adapter *sc)
{
	struct tom_data *td;
	struct toedev *tod;
	struct vi_info *vi;
	int i, rc, v;

	ASSERT_SYNCHRONIZED_OP(sc);

	/* per-adapter softc for TOM */
	td = malloc(sizeof(*td), M_CXGBE, M_ZERO | M_NOWAIT);
	if (td == NULL)
		return (ENOMEM);

	/* List of TOE PCBs and associated lock */
	mtx_init(&td->toep_list_lock, "PCB list lock", NULL, MTX_DEF);
	TAILQ_INIT(&td->toep_list);

	/* Listen context */
	mtx_init(&td->lctx_hash_lock, "lctx hash lock", NULL, MTX_DEF);
	td->listen_hash = hashinit_flags(LISTEN_HASH_SIZE, M_CXGBE,
	    &td->listen_mask, HASH_NOWAIT);

	/* List of WRs for which L2 resolution failed */
	mtx_init(&td->unsent_wr_lock, "Unsent WR list lock", NULL, MTX_DEF);
	STAILQ_INIT(&td->unsent_wr_list);
	TASK_INIT(&td->reclaim_wr_resources, 0, reclaim_wr_resources, td);

	/* TID tables */
	rc = alloc_tid_tabs(&sc->tids);
	if (rc != 0)
		goto done;

	rc = t4_init_ppod_region(&td->pr, &sc->vres.ddp,
	    t4_read_reg(sc, A_ULP_RX_TDDP_PSZ), "TDDP page pods");
	if (rc != 0)
		goto done;
	t4_set_reg_field(sc, A_ULP_RX_TDDP_TAGMASK,
	    V_TDDPTAGMASK(M_TDDPTAGMASK), td->pr.pr_tag_mask);

	alloc_tcb_history(sc, td);

	/* toedev ops */
	tod = &td->tod;
	init_toedev(tod);
	tod->tod_softc = sc;
	tod->tod_connect = t4_connect;
	tod->tod_listen_start = t4_listen_start;
	tod->tod_listen_stop = t4_listen_stop;
	tod->tod_rcvd = t4_rcvd;
	tod->tod_output = t4_tod_output;
	tod->tod_send_rst = t4_send_rst;
	tod->tod_send_fin = t4_send_fin;
	tod->tod_pcb_detach = t4_pcb_detach;
	tod->tod_l2_update = t4_l2_update;
	tod->tod_syncache_added = t4_syncache_added;
	tod->tod_syncache_removed = t4_syncache_removed;
	tod->tod_syncache_respond = t4_syncache_respond;
	tod->tod_offload_socket = t4_offload_socket;
	tod->tod_ctloutput = t4_ctloutput;
	tod->tod_tcp_info = t4_tcp_info;
#ifdef KERN_TLS
	tod->tod_alloc_tls_session = t4_alloc_tls_session;
#endif

	for_each_port(sc, i) {
		for_each_vi(sc->port[i], v, vi) {
			TOEDEV(vi->ifp) = &td->tod;
		}
	}

	sc->tom_softc = td;
	register_toedev(sc->tom_softc);

done:
	if (rc != 0)
		free_tom_data(sc, td);
	return (rc);
}

static int
t4_tom_deactivate(struct adapter *sc)
{
	int rc = 0;
	struct tom_data *td = sc->tom_softc;

	ASSERT_SYNCHRONIZED_OP(sc);

	if (td == NULL)
		return (0);	/* XXX. KASSERT? */

	if (sc->offload_map != 0)
		return (EBUSY);	/* at least one port has IFCAP_TOE enabled */

	if (uld_active(sc, ULD_IWARP) || uld_active(sc, ULD_ISCSI))
		return (EBUSY);	/* both iWARP and iSCSI rely on the TOE. */

	mtx_lock(&td->toep_list_lock);
	if (!TAILQ_EMPTY(&td->toep_list))
		rc = EBUSY;
	mtx_unlock(&td->toep_list_lock);

	mtx_lock(&td->lctx_hash_lock);
	if (td->lctx_count > 0)
		rc = EBUSY;
	mtx_unlock(&td->lctx_hash_lock);

	taskqueue_drain(taskqueue_thread, &td->reclaim_wr_resources);
	mtx_lock(&td->unsent_wr_lock);
	if (!STAILQ_EMPTY(&td->unsent_wr_list))
		rc = EBUSY;
	mtx_unlock(&td->unsent_wr_lock);

	if (rc == 0) {
		unregister_toedev(sc->tom_softc);
		free_tom_data(sc, td);
		sc->tom_softc = NULL;
	}

	return (rc);
}

static int
t4_aio_queue_tom(struct socket *so, struct kaiocb *job)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	int error;

	if (ulp_mode(toep) == ULP_MODE_TCPDDP) {
		error = t4_aio_queue_ddp(so, job);
		if (error != EOPNOTSUPP)
			return (error);
	}

	return (t4_aio_queue_aiotx(so, job));
}

static int
t4_ctloutput_tom(struct socket *so, struct sockopt *sopt)
{

	if (sopt->sopt_level != IPPROTO_TCP)
		return (tcp_ctloutput(so, sopt));

	switch (sopt->sopt_name) {
	case TCP_TLSOM_SET_TLS_CONTEXT:
	case TCP_TLSOM_GET_TLS_TOM:
	case TCP_TLSOM_CLR_TLS_TOM:
	case TCP_TLSOM_CLR_QUIES:
		return (t4_ctloutput_tls(so, sopt));
	default:
		return (tcp_ctloutput(so, sopt));
	}
}

static int
t4_tom_mod_load(void)
{
	struct protosw *tcp_protosw, *tcp6_protosw;

	/* CPL handlers */
	t4_register_cpl_handler(CPL_GET_TCB_RPL, do_get_tcb_rpl);
	t4_register_shared_cpl_handler(CPL_L2T_WRITE_RPL, do_l2t_write_rpl2,
	    CPL_COOKIE_TOM);
	t4_init_connect_cpl_handlers();
	t4_init_listen_cpl_handlers();
	t4_init_cpl_io_handlers();

	t4_ddp_mod_load();
	t4_tls_mod_load();

	tcp_protosw = pffindproto(PF_INET, IPPROTO_TCP, SOCK_STREAM);
	if (tcp_protosw == NULL)
		return (ENOPROTOOPT);
	bcopy(tcp_protosw, &toe_protosw, sizeof(toe_protosw));
	bcopy(tcp_protosw->pr_usrreqs, &toe_usrreqs, sizeof(toe_usrreqs));
	toe_usrreqs.pru_aio_queue = t4_aio_queue_tom;
	toe_protosw.pr_ctloutput = t4_ctloutput_tom;
	toe_protosw.pr_usrreqs = &toe_usrreqs;

	tcp6_protosw = pffindproto(PF_INET6, IPPROTO_TCP, SOCK_STREAM);
	if (tcp6_protosw == NULL)
		return (ENOPROTOOPT);
	bcopy(tcp6_protosw, &toe6_protosw, sizeof(toe6_protosw));
	bcopy(tcp6_protosw->pr_usrreqs, &toe6_usrreqs, sizeof(toe6_usrreqs));
	toe6_usrreqs.pru_aio_queue = t4_aio_queue_tom;
	toe6_protosw.pr_ctloutput = t4_ctloutput_tom;
	toe6_protosw.pr_usrreqs = &toe6_usrreqs;

	return (t4_register_uld(&tom_uld_info));
}

static void
tom_uninit(struct adapter *sc, void *arg __unused)
{
	if (begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t4tomun"))
		return;

	/* Try to free resources (works only if no port has IFCAP_TOE) */
	if (uld_active(sc, ULD_TOM))
		t4_deactivate_uld(sc, ULD_TOM);

	end_synchronized_op(sc, 0);
}

static int
t4_tom_mod_unload(void)
{
	t4_iterate(tom_uninit, NULL);

	if (t4_unregister_uld(&tom_uld_info) == EBUSY)
		return (EBUSY);

	t4_tls_mod_unload();
	t4_ddp_mod_unload();

	t4_uninit_connect_cpl_handlers();
	t4_uninit_listen_cpl_handlers();
	t4_uninit_cpl_io_handlers();
	t4_register_shared_cpl_handler(CPL_L2T_WRITE_RPL, NULL, CPL_COOKIE_TOM);
	t4_register_cpl_handler(CPL_GET_TCB_RPL, NULL);

	return (0);
}
#endif	/* TCP_OFFLOAD */

static int
t4_tom_modevent(module_t mod, int cmd, void *arg)
{
	int rc = 0;

#ifdef TCP_OFFLOAD
	switch (cmd) {
	case MOD_LOAD:
		rc = t4_tom_mod_load();
		break;

	case MOD_UNLOAD:
		rc = t4_tom_mod_unload();
		break;

	default:
		rc = EINVAL;
	}
#else
	printf("t4_tom: compiled without TCP_OFFLOAD support.\n");
	rc = EOPNOTSUPP;
#endif
	return (rc);
}

static moduledata_t t4_tom_moddata= {
	"t4_tom",
	t4_tom_modevent,
	0
};

MODULE_VERSION(t4_tom, 1);
MODULE_DEPEND(t4_tom, toecore, 1, 1, 1);
MODULE_DEPEND(t4_tom, t4nex, 1, 1, 1);
DECLARE_MODULE(t4_tom, t4_tom_moddata, SI_SUB_EXEC, SI_ORDER_ANY);
