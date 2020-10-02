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

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/refcount.h>
#include <sys/domain.h>
#include <sys/fnv_hash.h>
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
#include <netinet/in_fib.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/in6_fib.h>
#include <netinet6/scope6_var.h>
#include <netinet/tcp_timer.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>
#include <netinet/cc/cc.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "t4_clip.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

/* stid services */
static int alloc_stid(struct adapter *, struct listen_ctx *, int);
static struct listen_ctx *lookup_stid(struct adapter *, int);
static void free_stid(struct adapter *, struct listen_ctx *);

/* lctx services */
static struct listen_ctx *alloc_lctx(struct adapter *, struct inpcb *,
    struct vi_info *);
static int free_lctx(struct adapter *, struct listen_ctx *);
static void hold_lctx(struct listen_ctx *);
static void listen_hash_add(struct adapter *, struct listen_ctx *);
static struct listen_ctx *listen_hash_find(struct adapter *, struct inpcb *);
static struct listen_ctx *listen_hash_del(struct adapter *, struct inpcb *);
static struct inpcb *release_lctx(struct adapter *, struct listen_ctx *);

static void send_reset_synqe(struct toedev *, struct synq_entry *);

static int
alloc_stid(struct adapter *sc, struct listen_ctx *lctx, int isipv6)
{
	struct tid_info *t = &sc->tids;
	u_int stid, n, f, mask;
	struct stid_region *sr = &lctx->stid_region;

	/*
	 * An IPv6 server needs 2 naturally aligned stids (1 stid = 4 cells) in
	 * the TCAM.  The start of the stid region is properly aligned (the chip
	 * requires each region to be 128-cell aligned).
	 */
	n = isipv6 ? 2 : 1;
	mask = n - 1;
	KASSERT((t->stid_base & mask) == 0 && (t->nstids & mask) == 0,
	    ("%s: stid region (%u, %u) not properly aligned.  n = %u",
	    __func__, t->stid_base, t->nstids, n));

	mtx_lock(&t->stid_lock);
	if (n > t->nstids - t->stids_in_use) {
		mtx_unlock(&t->stid_lock);
		return (-1);
	}

	if (t->nstids_free_head >= n) {
		/*
		 * This allocation will definitely succeed because the region
		 * starts at a good alignment and we just checked we have enough
		 * stids free.
		 */
		f = t->nstids_free_head & mask;
		t->nstids_free_head -= n + f;
		stid = t->nstids_free_head;
		TAILQ_INSERT_HEAD(&t->stids, sr, link);
	} else {
		struct stid_region *s;

		stid = t->nstids_free_head;
		TAILQ_FOREACH(s, &t->stids, link) {
			stid += s->used + s->free;
			f = stid & mask;
			if (s->free >= n + f) {
				stid -= n + f;
				s->free -= n + f;
				TAILQ_INSERT_AFTER(&t->stids, s, sr, link);
				goto allocated;
			}
		}

		if (__predict_false(stid != t->nstids)) {
			panic("%s: stids TAILQ (%p) corrupt."
			    "  At %d instead of %d at the end of the queue.",
			    __func__, &t->stids, stid, t->nstids);
		}

		mtx_unlock(&t->stid_lock);
		return (-1);
	}

allocated:
	sr->used = n;
	sr->free = f;
	t->stids_in_use += n;
	t->stid_tab[stid] = lctx;
	mtx_unlock(&t->stid_lock);

	KASSERT(((stid + t->stid_base) & mask) == 0,
	    ("%s: EDOOFUS.", __func__));
	return (stid + t->stid_base);
}

static struct listen_ctx *
lookup_stid(struct adapter *sc, int stid)
{
	struct tid_info *t = &sc->tids;

	return (t->stid_tab[stid - t->stid_base]);
}

static void
free_stid(struct adapter *sc, struct listen_ctx *lctx)
{
	struct tid_info *t = &sc->tids;
	struct stid_region *sr = &lctx->stid_region;
	struct stid_region *s;

	KASSERT(sr->used > 0, ("%s: nonsense free (%d)", __func__, sr->used));

	mtx_lock(&t->stid_lock);
	s = TAILQ_PREV(sr, stid_head, link);
	if (s != NULL)
		s->free += sr->used + sr->free;
	else
		t->nstids_free_head += sr->used + sr->free;
	KASSERT(t->stids_in_use >= sr->used,
	    ("%s: stids_in_use (%u) < stids being freed (%u)", __func__,
	    t->stids_in_use, sr->used));
	t->stids_in_use -= sr->used;
	TAILQ_REMOVE(&t->stids, sr, link);
	mtx_unlock(&t->stid_lock);
}

static struct listen_ctx *
alloc_lctx(struct adapter *sc, struct inpcb *inp, struct vi_info *vi)
{
	struct listen_ctx *lctx;

	INP_WLOCK_ASSERT(inp);

	lctx = malloc(sizeof(struct listen_ctx), M_CXGBE, M_NOWAIT | M_ZERO);
	if (lctx == NULL)
		return (NULL);

	lctx->stid = alloc_stid(sc, lctx, inp->inp_vflag & INP_IPV6);
	if (lctx->stid < 0) {
		free(lctx, M_CXGBE);
		return (NULL);
	}

	if (inp->inp_vflag & INP_IPV6 &&
	    !IN6_ARE_ADDR_EQUAL(&in6addr_any, &inp->in6p_laddr)) {
		lctx->ce = t4_hold_lip(sc, &inp->in6p_laddr, NULL);
		if (lctx->ce == NULL) {
			free(lctx, M_CXGBE);
			return (NULL);
		}
	}

	lctx->ctrlq = &sc->sge.ctrlq[vi->pi->port_id];
	lctx->ofld_rxq = &sc->sge.ofld_rxq[vi->first_ofld_rxq];
	refcount_init(&lctx->refcount, 1);

	lctx->inp = inp;
	lctx->vnet = inp->inp_socket->so_vnet;
	in_pcbref(inp);

	return (lctx);
}

/* Don't call this directly, use release_lctx instead */
static int
free_lctx(struct adapter *sc, struct listen_ctx *lctx)
{
	struct inpcb *inp = lctx->inp;

	INP_WLOCK_ASSERT(inp);
	KASSERT(lctx->refcount == 0,
	    ("%s: refcount %d", __func__, lctx->refcount));
	KASSERT(lctx->stid >= 0, ("%s: bad stid %d.", __func__, lctx->stid));

	CTR4(KTR_CXGBE, "%s: stid %u, lctx %p, inp %p",
	    __func__, lctx->stid, lctx, lctx->inp);

	if (lctx->ce)
		t4_release_lip(sc, lctx->ce);
	free_stid(sc, lctx);
	free(lctx, M_CXGBE);

	return (in_pcbrele_wlocked(inp));
}

static void
hold_lctx(struct listen_ctx *lctx)
{

	refcount_acquire(&lctx->refcount);
}

static inline uint32_t
listen_hashfn(void *key, u_long mask)
{

	return (fnv_32_buf(&key, sizeof(key), FNV1_32_INIT) & mask);
}

/*
 * Add a listen_ctx entry to the listen hash table.
 */
static void
listen_hash_add(struct adapter *sc, struct listen_ctx *lctx)
{
	struct tom_data *td = sc->tom_softc;
	int bucket = listen_hashfn(lctx->inp, td->listen_mask);

	mtx_lock(&td->lctx_hash_lock);
	LIST_INSERT_HEAD(&td->listen_hash[bucket], lctx, link);
	td->lctx_count++;
	mtx_unlock(&td->lctx_hash_lock);
}

/*
 * Look for the listening socket's context entry in the hash and return it.
 */
static struct listen_ctx *
listen_hash_find(struct adapter *sc, struct inpcb *inp)
{
	struct tom_data *td = sc->tom_softc;
	int bucket = listen_hashfn(inp, td->listen_mask);
	struct listen_ctx *lctx;

	mtx_lock(&td->lctx_hash_lock);
	LIST_FOREACH(lctx, &td->listen_hash[bucket], link) {
		if (lctx->inp == inp)
			break;
	}
	mtx_unlock(&td->lctx_hash_lock);

	return (lctx);
}

/*
 * Removes the listen_ctx structure for inp from the hash and returns it.
 */
static struct listen_ctx *
listen_hash_del(struct adapter *sc, struct inpcb *inp)
{
	struct tom_data *td = sc->tom_softc;
	int bucket = listen_hashfn(inp, td->listen_mask);
	struct listen_ctx *lctx, *l;

	mtx_lock(&td->lctx_hash_lock);
	LIST_FOREACH_SAFE(lctx, &td->listen_hash[bucket], link, l) {
		if (lctx->inp == inp) {
			LIST_REMOVE(lctx, link);
			td->lctx_count--;
			break;
		}
	}
	mtx_unlock(&td->lctx_hash_lock);

	return (lctx);
}

/*
 * Releases a hold on the lctx.  Must be called with the listening socket's inp
 * locked.  The inp may be freed by this function and it returns NULL to
 * indicate this.
 */
static struct inpcb *
release_lctx(struct adapter *sc, struct listen_ctx *lctx)
{
	struct inpcb *inp = lctx->inp;
	int inp_freed = 0;

	INP_WLOCK_ASSERT(inp);
	if (refcount_release(&lctx->refcount))
		inp_freed = free_lctx(sc, lctx);

	return (inp_freed ? NULL : inp);
}

static void
send_flowc_wr_synqe(struct adapter *sc, struct synq_entry *synqe)
{
	struct mbuf *m = synqe->syn;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct vi_info *vi = ifp->if_softc;
	struct port_info *pi = vi->pi;
	struct wrqe *wr;
	struct fw_flowc_wr *flowc;
	struct sge_wrq *ofld_txq;
	struct sge_ofld_rxq *ofld_rxq;
	const int nparams = 6;
	const int flowclen = sizeof(*flowc) + nparams * sizeof(struct fw_flowc_mnemval);
	const u_int pfvf = sc->pf << S_FW_VIID_PFN;

	INP_WLOCK_ASSERT(synqe->lctx->inp);
	MPASS((synqe->flags & TPF_FLOWC_WR_SENT) == 0);

	ofld_txq = &sc->sge.ofld_txq[synqe->params.txq_idx];
	ofld_rxq = &sc->sge.ofld_rxq[synqe->params.rxq_idx];

	wr = alloc_wrqe(roundup2(flowclen, 16), ofld_txq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	flowc = wrtod(wr);
	memset(flowc, 0, wr->wr_len);
	flowc->op_to_nparams = htobe32(V_FW_WR_OP(FW_FLOWC_WR) |
	    V_FW_FLOWC_WR_NPARAMS(nparams));
	flowc->flowid_len16 = htonl(V_FW_WR_LEN16(howmany(flowclen, 16)) |
	    V_FW_WR_FLOWID(synqe->tid));
	flowc->mnemval[0].mnemonic = FW_FLOWC_MNEM_PFNVFN;
	flowc->mnemval[0].val = htobe32(pfvf);
	flowc->mnemval[1].mnemonic = FW_FLOWC_MNEM_CH;
	flowc->mnemval[1].val = htobe32(pi->tx_chan);
	flowc->mnemval[2].mnemonic = FW_FLOWC_MNEM_PORT;
	flowc->mnemval[2].val = htobe32(pi->tx_chan);
	flowc->mnemval[3].mnemonic = FW_FLOWC_MNEM_IQID;
	flowc->mnemval[3].val = htobe32(ofld_rxq->iq.abs_id);
	flowc->mnemval[4].mnemonic = FW_FLOWC_MNEM_SNDBUF;
	flowc->mnemval[4].val = htobe32(512);
	flowc->mnemval[5].mnemonic = FW_FLOWC_MNEM_MSS;
	flowc->mnemval[5].val = htobe32(512);

	synqe->flags |= TPF_FLOWC_WR_SENT;
	t4_wrq_tx(sc, wr);
}

static void
send_reset_synqe(struct toedev *tod, struct synq_entry *synqe)
{
	struct adapter *sc = tod->tod_softc;
	struct wrqe *wr;
	struct cpl_abort_req *req;

	INP_WLOCK_ASSERT(synqe->lctx->inp);

	CTR5(KTR_CXGBE, "%s: synqe %p (0x%x), tid %d%s",
	    __func__, synqe, synqe->flags, synqe->tid,
	    synqe->flags & TPF_ABORT_SHUTDOWN ?
	    " (abort already in progress)" : "");
	if (synqe->flags & TPF_ABORT_SHUTDOWN)
		return;	/* abort already in progress */
	synqe->flags |= TPF_ABORT_SHUTDOWN;

	if (!(synqe->flags & TPF_FLOWC_WR_SENT))
		send_flowc_wr_synqe(sc, synqe);

	wr = alloc_wrqe(sizeof(*req), &sc->sge.ofld_txq[synqe->params.txq_idx]);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);
	INIT_TP_WR_MIT_CPL(req, CPL_ABORT_REQ, synqe->tid);
	req->rsvd0 = 0;	/* don't have a snd_nxt */
	req->rsvd1 = 1;	/* no data sent yet */
	req->cmd = CPL_ABORT_SEND_RST;

	t4_l2t_send(sc, wr, &sc->l2t->l2tab[synqe->params.l2t_idx]);
}

static int
create_server(struct adapter *sc, struct listen_ctx *lctx)
{
	struct wrqe *wr;
	struct cpl_pass_open_req *req;
	struct inpcb *inp = lctx->inp;

	wr = alloc_wrqe(sizeof(*req), lctx->ctrlq);
	if (wr == NULL) {
		log(LOG_ERR, "%s: allocation failure", __func__);
		return (ENOMEM);
	}
	req = wrtod(wr);

	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htobe32(MK_OPCODE_TID(CPL_PASS_OPEN_REQ, lctx->stid));
	req->local_port = inp->inp_lport;
	req->peer_port = 0;
	req->local_ip = inp->inp_laddr.s_addr;
	req->peer_ip = 0;
	req->opt0 = htobe64(V_TX_CHAN(lctx->ctrlq->eq.tx_chan));
	req->opt1 = htobe64(V_CONN_POLICY(CPL_CONN_POLICY_ASK) |
	    F_SYN_RSS_ENABLE | V_SYN_RSS_QUEUE(lctx->ofld_rxq->iq.abs_id));

	t4_wrq_tx(sc, wr);
	return (0);
}

static int
create_server6(struct adapter *sc, struct listen_ctx *lctx)
{
	struct wrqe *wr;
	struct cpl_pass_open_req6 *req;
	struct inpcb *inp = lctx->inp;

	wr = alloc_wrqe(sizeof(*req), lctx->ctrlq);
	if (wr == NULL) {
		log(LOG_ERR, "%s: allocation failure", __func__);
		return (ENOMEM);
	}
	req = wrtod(wr);

	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htobe32(MK_OPCODE_TID(CPL_PASS_OPEN_REQ6, lctx->stid));
	req->local_port = inp->inp_lport;
	req->peer_port = 0;
	req->local_ip_hi = *(uint64_t *)&inp->in6p_laddr.s6_addr[0];
	req->local_ip_lo = *(uint64_t *)&inp->in6p_laddr.s6_addr[8];
	req->peer_ip_hi = 0;
	req->peer_ip_lo = 0;
	req->opt0 = htobe64(V_TX_CHAN(lctx->ctrlq->eq.tx_chan));
	req->opt1 = htobe64(V_CONN_POLICY(CPL_CONN_POLICY_ASK) |
	    F_SYN_RSS_ENABLE | V_SYN_RSS_QUEUE(lctx->ofld_rxq->iq.abs_id));

	t4_wrq_tx(sc, wr);
	return (0);
}

static int
destroy_server(struct adapter *sc, struct listen_ctx *lctx)
{
	struct wrqe *wr;
	struct cpl_close_listsvr_req *req;

	wr = alloc_wrqe(sizeof(*req), lctx->ctrlq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_LISTSRV_REQ,
	    lctx->stid));
	req->reply_ctrl = htobe16(lctx->ofld_rxq->iq.abs_id);
	req->rsvd = htobe16(0);

	t4_wrq_tx(sc, wr);
	return (0);
}

/*
 * Start a listening server by sending a passive open request to HW.
 *
 * Can't take adapter lock here and access to sc->flags,
 * sc->offload_map, if_capenable are all race prone.
 */
int
t4_listen_start(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
	struct vi_info *vi;
	struct port_info *pi;
	struct inpcb *inp = tp->t_inpcb;
	struct listen_ctx *lctx;
	int i, rc, v;
	struct offload_settings settings;

	INP_WLOCK_ASSERT(inp);

	rw_rlock(&sc->policy_lock);
	settings = *lookup_offload_policy(sc, OPEN_TYPE_LISTEN, NULL,
	    EVL_MAKETAG(0xfff, 0, 0), inp);
	rw_runlock(&sc->policy_lock);
	if (!settings.offload)
		return (0);

	/* Don't start a hardware listener for any loopback address. */
	if (inp->inp_vflag & INP_IPV6 && IN6_IS_ADDR_LOOPBACK(&inp->in6p_laddr))
		return (0);
	if (!(inp->inp_vflag & INP_IPV6) &&
	    IN_LOOPBACK(ntohl(inp->inp_laddr.s_addr)))
		return (0);
	if (sc->flags & KERN_TLS_OK)
		return (0);
#if 0
	ADAPTER_LOCK(sc);
	if (IS_BUSY(sc)) {
		log(LOG_ERR, "%s: listen request ignored, %s is busy",
		    __func__, device_get_nameunit(sc->dev));
		goto done;
	}

	KASSERT(uld_active(sc, ULD_TOM),
	    ("%s: TOM not initialized", __func__));
#endif

	/*
	 * Find an initialized VI with IFCAP_TOE (4 or 6).  We'll use the first
	 * such VI's queues to send the passive open and receive the reply to
	 * it.
	 *
	 * XXX: need a way to mark a port in use by offload.  if_cxgbe should
	 * then reject any attempt to bring down such a port (and maybe reject
	 * attempts to disable IFCAP_TOE on that port too?).
	 */
	for_each_port(sc, i) {
		pi = sc->port[i];
		for_each_vi(pi, v, vi) {
			if (vi->flags & VI_INIT_DONE &&
			    vi->ifp->if_capenable & IFCAP_TOE)
				goto found;
		}
	}
	goto done;	/* no port that's UP with IFCAP_TOE enabled */
found:

	if (listen_hash_find(sc, inp) != NULL)
		goto done;	/* already setup */

	lctx = alloc_lctx(sc, inp, vi);
	if (lctx == NULL) {
		log(LOG_ERR,
		    "%s: listen request ignored, %s couldn't allocate lctx\n",
		    __func__, device_get_nameunit(sc->dev));
		goto done;
	}
	listen_hash_add(sc, lctx);

	CTR6(KTR_CXGBE, "%s: stid %u (%s), lctx %p, inp %p vflag 0x%x",
	    __func__, lctx->stid, tcpstates[tp->t_state], lctx, inp,
	    inp->inp_vflag);

	if (inp->inp_vflag & INP_IPV6)
		rc = create_server6(sc, lctx);
	else
		rc = create_server(sc, lctx);
	if (rc != 0) {
		log(LOG_ERR, "%s: %s failed to create hw listener: %d.\n",
		    __func__, device_get_nameunit(sc->dev), rc);
		(void) listen_hash_del(sc, inp);
		inp = release_lctx(sc, lctx);
		/* can't be freed, host stack has a reference */
		KASSERT(inp != NULL, ("%s: inp freed", __func__));
		goto done;
	}
	lctx->flags |= LCTX_RPL_PENDING;
done:
#if 0
	ADAPTER_UNLOCK(sc);
#endif
	return (0);
}

int
t4_listen_stop(struct toedev *tod, struct tcpcb *tp)
{
	struct listen_ctx *lctx;
	struct adapter *sc = tod->tod_softc;
	struct inpcb *inp = tp->t_inpcb;

	INP_WLOCK_ASSERT(inp);

	lctx = listen_hash_del(sc, inp);
	if (lctx == NULL)
		return (ENOENT);	/* no hardware listener for this inp */

	CTR4(KTR_CXGBE, "%s: stid %u, lctx %p, flags %x", __func__, lctx->stid,
	    lctx, lctx->flags);

	/*
	 * If the reply to the PASS_OPEN is still pending we'll wait for it to
	 * arrive and clean up when it does.
	 */
	if (lctx->flags & LCTX_RPL_PENDING) {
		return (EINPROGRESS);
	}

	destroy_server(sc, lctx);
	return (0);
}

static inline struct synq_entry *
alloc_synqe(struct adapter *sc __unused, struct listen_ctx *lctx, int flags)
{
	struct synq_entry *synqe;

	INP_WLOCK_ASSERT(lctx->inp);
	MPASS(flags == M_WAITOK || flags == M_NOWAIT);

	synqe = malloc(sizeof(*synqe), M_CXGBE, flags);
	if (__predict_true(synqe != NULL)) {
		synqe->flags = TPF_SYNQE;
		refcount_init(&synqe->refcnt, 1);
		synqe->lctx = lctx;
		hold_lctx(lctx);	/* Every synqe has a ref on its lctx. */
		synqe->syn = NULL;
	}

	return (synqe);
}

static inline void
hold_synqe(struct synq_entry *synqe)
{

	refcount_acquire(&synqe->refcnt);
}

static inline struct inpcb *
release_synqe(struct adapter *sc, struct synq_entry *synqe)
{
	struct inpcb *inp;

	MPASS(synqe->flags & TPF_SYNQE);
	MPASS(synqe->lctx != NULL);

	inp = synqe->lctx->inp;
	MPASS(inp != NULL);
	INP_WLOCK_ASSERT(inp);

	if (refcount_release(&synqe->refcnt)) {
		inp = release_lctx(sc, synqe->lctx);
		m_freem(synqe->syn);
		free(synqe, M_CXGBE);
	}

	return (inp);
}

void
t4_syncache_added(struct toedev *tod __unused, void *arg)
{
	struct synq_entry *synqe = arg;

	hold_synqe(synqe);
}

void
t4_syncache_removed(struct toedev *tod, void *arg)
{
	struct adapter *sc = tod->tod_softc;
	struct synq_entry *synqe = arg;
	struct inpcb *inp = synqe->lctx->inp;

	/*
	 * XXX: this is a LOR but harmless when running from the softclock.
	 */
	INP_WLOCK(inp);
	inp = release_synqe(sc, synqe);
	if (inp != NULL)
		INP_WUNLOCK(inp);
}

int
t4_syncache_respond(struct toedev *tod, void *arg, struct mbuf *m)
{
	struct synq_entry *synqe = arg;

	if (atomic_fetchadd_int(&synqe->ok_to_respond, 1) == 0) {
		struct tcpopt to;
		struct ip *ip = mtod(m, struct ip *);
		struct tcphdr *th;

		if (ip->ip_v == IPVERSION)
			th = (void *)(ip + 1);
		else
			th = (void *)((struct ip6_hdr *)ip + 1);
		bzero(&to, sizeof(to));
		tcp_dooptions(&to, (void *)(th + 1),
		    (th->th_off << 2) - sizeof(*th), TO_SYN);

		/* save these for later */
		synqe->iss = be32toh(th->th_seq);
		synqe->irs = be32toh(th->th_ack) - 1;
		synqe->ts = to.to_tsval;
	}

	m_freem(m);	/* don't need this any more */
	return (0);
}

static int
do_pass_open_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_pass_open_rpl *cpl = (const void *)(rss + 1);
	int stid = GET_TID(cpl);
	unsigned int status = cpl->status;
	struct listen_ctx *lctx = lookup_stid(sc, stid);
	struct inpcb *inp = lctx->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_PASS_OPEN_RPL,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));

	INP_WLOCK(inp);

	CTR4(KTR_CXGBE, "%s: stid %d, status %u, flags 0x%x",
	    __func__, stid, status, lctx->flags);

	lctx->flags &= ~LCTX_RPL_PENDING;

	if (status != CPL_ERR_NONE)
		log(LOG_ERR, "listener (stid %u) failed: %d\n", stid, status);

#ifdef INVARIANTS
	/*
	 * If the inp has been dropped (listening socket closed) then
	 * listen_stop must have run and taken the inp out of the hash.
	 */
	if (inp->inp_flags & INP_DROPPED) {
		KASSERT(listen_hash_del(sc, inp) == NULL,
		    ("%s: inp %p still in listen hash", __func__, inp));
	}
#endif

	if (inp->inp_flags & INP_DROPPED && status != CPL_ERR_NONE) {
		if (release_lctx(sc, lctx) != NULL)
			INP_WUNLOCK(inp);
		return (status);
	}

	/*
	 * Listening socket stopped listening earlier and now the chip tells us
	 * it has started the hardware listener.  Stop it; the lctx will be
	 * released in do_close_server_rpl.
	 */
	if (inp->inp_flags & INP_DROPPED) {
		destroy_server(sc, lctx);
		INP_WUNLOCK(inp);
		return (status);
	}

	/*
	 * Failed to start hardware listener.  Take inp out of the hash and
	 * release our reference on it.  An error message has been logged
	 * already.
	 */
	if (status != CPL_ERR_NONE) {
		listen_hash_del(sc, inp);
		if (release_lctx(sc, lctx) != NULL)
			INP_WUNLOCK(inp);
		return (status);
	}

	/* hardware listener open for business */

	INP_WUNLOCK(inp);
	return (status);
}

static int
do_close_server_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_close_listsvr_rpl *cpl = (const void *)(rss + 1);
	int stid = GET_TID(cpl);
	unsigned int status = cpl->status;
	struct listen_ctx *lctx = lookup_stid(sc, stid);
	struct inpcb *inp = lctx->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_CLOSE_LISTSRV_RPL,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));

	CTR3(KTR_CXGBE, "%s: stid %u, status %u", __func__, stid, status);

	if (status != CPL_ERR_NONE) {
		log(LOG_ERR, "%s: failed (%u) to close listener for stid %u\n",
		    __func__, status, stid);
		return (status);
	}

	INP_WLOCK(inp);
	inp = release_lctx(sc, lctx);
	if (inp != NULL)
		INP_WUNLOCK(inp);

	return (status);
}

static void
done_with_synqe(struct adapter *sc, struct synq_entry *synqe)
{
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
	struct l2t_entry *e = &sc->l2t->l2tab[synqe->params.l2t_idx];
	int ntids;

	INP_WLOCK_ASSERT(inp);
	ntids = inp->inp_vflag & INP_IPV6 ? 2 : 1;

	remove_tid(sc, synqe->tid, ntids);
	release_tid(sc, synqe->tid, lctx->ctrlq);
	t4_l2t_release(e);
	inp = release_synqe(sc, synqe);
	if (inp)
		INP_WUNLOCK(inp);
}

void
synack_failure_cleanup(struct adapter *sc, int tid)
{
	struct synq_entry *synqe = lookup_tid(sc, tid);

	INP_WLOCK(synqe->lctx->inp);
	done_with_synqe(sc, synqe);
}

int
do_abort_req_synqe(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_req_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct synq_entry *synqe = lookup_tid(sc, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
	struct sge_wrq *ofld_txq;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_REQ_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(synqe->tid == tid, ("%s: toep tid mismatch", __func__));

	CTR6(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x), lctx %p, status %d",
	    __func__, tid, synqe, synqe->flags, synqe->lctx, cpl->status);

	if (negative_advice(cpl->status))
		return (0);	/* Ignore negative advice */

	INP_WLOCK(inp);

	ofld_txq = &sc->sge.ofld_txq[synqe->params.txq_idx];

	if (!(synqe->flags & TPF_FLOWC_WR_SENT))
		send_flowc_wr_synqe(sc, synqe);

	/*
	 * If we'd initiated an abort earlier the reply to it is responsible for
	 * cleaning up resources.  Otherwise we tear everything down right here
	 * right now.  We owe the T4 a CPL_ABORT_RPL no matter what.
	 */
	if (synqe->flags & TPF_ABORT_SHUTDOWN) {
		INP_WUNLOCK(inp);
		goto done;
	}

	done_with_synqe(sc, synqe);
	/* inp lock released by done_with_synqe */
done:
	send_abort_rpl(sc, ofld_txq, tid, CPL_ABORT_NO_RST);
	return (0);
}

int
do_abort_rpl_synqe(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_rpl_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct synq_entry *synqe = lookup_tid(sc, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_RPL_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(synqe->tid == tid, ("%s: toep tid mismatch", __func__));

	CTR6(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x), lctx %p, status %d",
	    __func__, tid, synqe, synqe->flags, synqe->lctx, cpl->status);

	INP_WLOCK(inp);
	KASSERT(synqe->flags & TPF_ABORT_SHUTDOWN,
	    ("%s: wasn't expecting abort reply for synqe %p (0x%x)",
	    __func__, synqe, synqe->flags));

	done_with_synqe(sc, synqe);
	/* inp lock released by done_with_synqe */

	return (0);
}

void
t4_offload_socket(struct toedev *tod, void *arg, struct socket *so)
{
	struct adapter *sc = tod->tod_softc;
	struct synq_entry *synqe = arg;
	struct inpcb *inp = sotoinpcb(so);
	struct toepcb *toep = synqe->toep;

	NET_EPOCH_ASSERT();	/* prevents bad race with accept() */
	INP_WLOCK_ASSERT(inp);
	KASSERT(synqe->flags & TPF_SYNQE,
	    ("%s: %p not a synq_entry?", __func__, arg));
	MPASS(toep->tid == synqe->tid);

	offload_socket(so, toep);
	make_established(toep, synqe->iss, synqe->irs, synqe->tcp_opt);
	toep->flags |= TPF_CPL_PENDING;
	update_tid(sc, synqe->tid, toep);
	synqe->flags |= TPF_SYNQE_EXPANDED;
	inp->inp_flowtype = (inp->inp_vflag & INP_IPV6) ?
	    M_HASHTYPE_RSS_TCP_IPV6 : M_HASHTYPE_RSS_TCP_IPV4;
	inp->inp_flowid = synqe->rss_hash;
}

static void
t4opt_to_tcpopt(const struct tcp_options *t4opt, struct tcpopt *to)
{
	bzero(to, sizeof(*to));

	if (t4opt->mss) {
		to->to_flags |= TOF_MSS;
		to->to_mss = be16toh(t4opt->mss);
	}

	if (t4opt->wsf > 0 && t4opt->wsf < 15) {
		to->to_flags |= TOF_SCALE;
		to->to_wscale = t4opt->wsf;
	}

	if (t4opt->tstamp)
		to->to_flags |= TOF_TS;

	if (t4opt->sack)
		to->to_flags |= TOF_SACKPERM;
}

static void
pass_accept_req_to_protohdrs(struct adapter *sc, const struct mbuf *m,
    struct in_conninfo *inc, struct tcphdr *th, uint8_t *iptos)
{
	const struct cpl_pass_accept_req *cpl = mtod(m, const void *);
	const struct ether_header *eh;
	unsigned int hlen = be32toh(cpl->hdr_len);
	uintptr_t l3hdr;
	const struct tcphdr *tcp;

	eh = (const void *)(cpl + 1);
	if (chip_id(sc) >= CHELSIO_T6) {
		l3hdr = ((uintptr_t)eh + G_T6_ETH_HDR_LEN(hlen));
		tcp = (const void *)(l3hdr + G_T6_IP_HDR_LEN(hlen));
	} else {
		l3hdr = ((uintptr_t)eh + G_ETH_HDR_LEN(hlen));
		tcp = (const void *)(l3hdr + G_IP_HDR_LEN(hlen));
	}

	/* extract TOS (DiffServ + ECN) byte for AccECN */
	if (iptos) {
		if (((struct ip *)l3hdr)->ip_v == IPVERSION) {
			const struct ip *ip = (const void *)l3hdr;
			*iptos = ip->ip_tos;
		}
#ifdef INET6
		else
		if (((struct ip *)l3hdr)->ip_v == (IPV6_VERSION >> 4)) {
			const struct ip6_hdr *ip6 = (const void *)l3hdr;
			*iptos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		}
#endif /* INET */
	}

	if (inc) {
		bzero(inc, sizeof(*inc));
		inc->inc_fport = tcp->th_sport;
		inc->inc_lport = tcp->th_dport;
		if (((struct ip *)l3hdr)->ip_v == IPVERSION) {
			const struct ip *ip = (const void *)l3hdr;

			inc->inc_faddr = ip->ip_src;
			inc->inc_laddr = ip->ip_dst;
		} else {
			const struct ip6_hdr *ip6 = (const void *)l3hdr;

			inc->inc_flags |= INC_ISIPV6;
			inc->inc6_faddr = ip6->ip6_src;
			inc->inc6_laddr = ip6->ip6_dst;
		}
	}

	if (th) {
		bcopy(tcp, th, sizeof(*th));
		tcp_fields_to_host(th);		/* just like tcp_input */
	}
}

static struct l2t_entry *
get_l2te_for_nexthop(struct port_info *pi, struct ifnet *ifp,
    struct in_conninfo *inc)
{
	struct l2t_entry *e;
	struct sockaddr_in6 sin6;
	struct sockaddr *dst = (void *)&sin6;
	struct nhop_object *nh;

	if (inc->inc_flags & INC_ISIPV6) {
		bzero(dst, sizeof(struct sockaddr_in6));
		dst->sa_len = sizeof(struct sockaddr_in6);
		dst->sa_family = AF_INET6;

		if (IN6_IS_ADDR_LINKLOCAL(&inc->inc6_laddr)) {
			/* no need for route lookup */
			e = t4_l2t_get(pi, ifp, dst);
			return (e);
		}

		nh = fib6_lookup(RT_DEFAULT_FIB, &inc->inc6_faddr, 0, NHR_NONE, 0);
		if (nh == NULL)
			return (NULL);
		if (nh->nh_ifp != ifp)
			return (NULL);
		if (nh->nh_flags & NHF_GATEWAY)
			((struct sockaddr_in6 *)dst)->sin6_addr = nh->gw6_sa.sin6_addr;
		else
			((struct sockaddr_in6 *)dst)->sin6_addr = inc->inc6_faddr;
	} else {
		dst->sa_len = sizeof(struct sockaddr_in);
		dst->sa_family = AF_INET;

		nh = fib4_lookup(RT_DEFAULT_FIB, inc->inc_faddr, 0, NHR_NONE, 0);
		if (nh == NULL)
			return (NULL);
		if (nh->nh_ifp != ifp)
			return (NULL);
		if (nh->nh_flags & NHF_GATEWAY)
			((struct sockaddr_in *)dst)->sin_addr = nh->gw4_sa.sin_addr;
		else
			((struct sockaddr_in *)dst)->sin_addr = inc->inc_faddr;
	}

	e = t4_l2t_get(pi, ifp, dst);
	return (e);
}

static int
send_synack(struct adapter *sc, struct synq_entry *synqe, uint64_t opt0,
    uint32_t opt2, int tid)
{
	struct wrqe *wr;
	struct cpl_pass_accept_rpl *rpl;
	struct l2t_entry *e = &sc->l2t->l2tab[synqe->params.l2t_idx];

	wr = alloc_wrqe(is_t4(sc) ? sizeof(struct cpl_pass_accept_rpl) :
	    sizeof(struct cpl_t5_pass_accept_rpl), &sc->sge.ctrlq[0]);
	if (wr == NULL)
		return (ENOMEM);
	rpl = wrtod(wr);

	if (is_t4(sc))
		INIT_TP_WR_MIT_CPL(rpl, CPL_PASS_ACCEPT_RPL, tid);
	else {
		struct cpl_t5_pass_accept_rpl *rpl5 = (void *)rpl;

		INIT_TP_WR_MIT_CPL(rpl5, CPL_PASS_ACCEPT_RPL, tid);
		rpl5->iss = htobe32(synqe->iss);
	}
	rpl->opt0 = opt0;
	rpl->opt2 = opt2;

	return (t4_l2t_send(sc, wr, e));
}

#define REJECT_PASS_ACCEPT_REQ(tunnel)	do { \
	if (!tunnel) { \
		m_freem(m); \
		m = NULL; \
	} \
	reject_reason = __LINE__; \
	goto reject; \
} while (0)

/*
 * The context associated with a tid entry via insert_tid could be a synq_entry
 * or a toepcb.  The only way CPL handlers can tell is via a bit in these flags.
 */
CTASSERT(offsetof(struct toepcb, flags) == offsetof(struct synq_entry, flags));

/*
 * Incoming SYN on a listening socket.
 *
 * XXX: Every use of ifp in this routine has a bad race with up/down, toe/-toe,
 * etc.
 */
static int
do_pass_accept_req(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct toedev *tod;
	const struct cpl_pass_accept_req *cpl = mtod(m, const void *);
	unsigned int stid = G_PASS_OPEN_TID(be32toh(cpl->tos_stid));
	unsigned int tid = GET_TID(cpl);
	struct listen_ctx *lctx = lookup_stid(sc, stid);
	struct inpcb *inp;
	struct socket *so;
	struct in_conninfo inc;
	struct tcphdr th;
	struct tcpopt to;
	struct port_info *pi;
	struct vi_info *vi;
	struct ifnet *hw_ifp, *ifp;
	struct l2t_entry *e = NULL;
	struct synq_entry *synqe = NULL;
	int reject_reason, v, ntids;
	uint16_t vid, l2info;
	struct epoch_tracker et;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif
	struct offload_settings settings;
	uint8_t iptos;

	KASSERT(opcode == CPL_PASS_ACCEPT_REQ,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));

	CTR4(KTR_CXGBE, "%s: stid %u, tid %u, lctx %p", __func__, stid, tid,
	    lctx);

	CURVNET_SET(lctx->vnet);	/* before any potential REJECT */

	/*
	 * Use the MAC index to lookup the associated VI.  If this SYN didn't
	 * match a perfect MAC filter, punt.
	 */
	l2info = be16toh(cpl->l2info);
	pi = sc->port[G_SYN_INTF(l2info)];
	if (!(l2info & F_SYN_XACT_MATCH)) {
		REJECT_PASS_ACCEPT_REQ(false);
	}
	for_each_vi(pi, v, vi) {
		if (vi->xact_addr_filt == G_SYN_MAC_IDX(l2info))
			goto found;
	}
	REJECT_PASS_ACCEPT_REQ(false);
found:
	hw_ifp = vi->ifp;	/* the cxgbe ifnet */
	m->m_pkthdr.rcvif = hw_ifp;
	tod = TOEDEV(hw_ifp);

	/*
	 * Don't offload if the peer requested a TCP option that's not known to
	 * the silicon.  Send the SYN to the kernel instead.
	 */
	if (__predict_false(cpl->tcpopt.unknown))
		REJECT_PASS_ACCEPT_REQ(true);

	/*
	 * Figure out if there is a pseudo interface (vlan, lagg, etc.)
	 * involved.  Don't offload if the SYN had a VLAN tag and the vid
	 * doesn't match anything on this interface.
	 *
	 * XXX: lagg support, lagg + vlan support.
	 */
	vid = EVL_VLANOFTAG(be16toh(cpl->vlan));
	if (vid != 0xfff && vid != 0) {
		ifp = VLAN_DEVAT(hw_ifp, vid);
		if (ifp == NULL)
			REJECT_PASS_ACCEPT_REQ(true);
	} else
		ifp = hw_ifp;

	/*
	 * Don't offload if the ifnet that the SYN came in on is not in the same
	 * vnet as the listening socket.
	 */
	if (lctx->vnet != ifp->if_vnet)
		REJECT_PASS_ACCEPT_REQ(true);

	pass_accept_req_to_protohdrs(sc, m, &inc, &th, &iptos);
	if (inc.inc_flags & INC_ISIPV6) {

		/* Don't offload if the ifcap isn't enabled */
		if ((ifp->if_capenable & IFCAP_TOE6) == 0)
			REJECT_PASS_ACCEPT_REQ(true);

		/*
		 * SYN must be directed to an IP6 address on this ifnet.  This
		 * is more restrictive than in6_localip.
		 */
		NET_EPOCH_ENTER(et);
		if (!in6_ifhasaddr(ifp, &inc.inc6_laddr)) {
			NET_EPOCH_EXIT(et);
			REJECT_PASS_ACCEPT_REQ(true);
		}

		ntids = 2;
	} else {

		/* Don't offload if the ifcap isn't enabled */
		if ((ifp->if_capenable & IFCAP_TOE4) == 0)
			REJECT_PASS_ACCEPT_REQ(true);

		/*
		 * SYN must be directed to an IP address on this ifnet.  This
		 * is more restrictive than in_localip.
		 */
		NET_EPOCH_ENTER(et);
		if (!in_ifhasaddr(ifp, inc.inc_laddr)) {
			NET_EPOCH_EXIT(et);
			REJECT_PASS_ACCEPT_REQ(true);
		}

		ntids = 1;
	}

	e = get_l2te_for_nexthop(pi, ifp, &inc);
	if (e == NULL) {
		NET_EPOCH_EXIT(et);
		REJECT_PASS_ACCEPT_REQ(true);
	}

	/* Don't offload if the 4-tuple is already in use */
	if (toe_4tuple_check(&inc, &th, ifp) != 0) {
		NET_EPOCH_EXIT(et);
		REJECT_PASS_ACCEPT_REQ(false);
	}

	inp = lctx->inp;		/* listening socket, not owned by TOE */
	INP_WLOCK(inp);

	/* Don't offload if the listening socket has closed */
	if (__predict_false(inp->inp_flags & INP_DROPPED)) {
		INP_WUNLOCK(inp);
		NET_EPOCH_EXIT(et);
		REJECT_PASS_ACCEPT_REQ(false);
	}
	so = inp->inp_socket;
	rw_rlock(&sc->policy_lock);
	settings = *lookup_offload_policy(sc, OPEN_TYPE_PASSIVE, m,
	    EVL_MAKETAG(0xfff, 0, 0), inp);
	rw_runlock(&sc->policy_lock);
	if (!settings.offload) {
		INP_WUNLOCK(inp);
		NET_EPOCH_EXIT(et);
		REJECT_PASS_ACCEPT_REQ(true);	/* Rejected by COP. */
	}

	synqe = alloc_synqe(sc, lctx, M_NOWAIT);
	if (synqe == NULL) {
		INP_WUNLOCK(inp);
		NET_EPOCH_EXIT(et);
		REJECT_PASS_ACCEPT_REQ(true);
	}
	MPASS(rss->hash_type == RSS_HASH_TCP);
	synqe->rss_hash = be32toh(rss->hash_val);
	atomic_store_int(&synqe->ok_to_respond, 0);

	init_conn_params(vi, &settings, &inc, so, &cpl->tcpopt, e->idx,
	    &synqe->params);

	/*
	 * If all goes well t4_syncache_respond will get called during
	 * syncache_add.  Note that syncache_add releases the pcb lock.
	 */
	t4opt_to_tcpopt(&cpl->tcpopt, &to);
	toe_syncache_add(&inc, &to, &th, inp, tod, synqe, iptos);

	if (atomic_load_int(&synqe->ok_to_respond) > 0) {
		uint64_t opt0;
		uint32_t opt2;

		opt0 = calc_options0(vi, &synqe->params);
		opt2 = calc_options2(vi, &synqe->params);

		insert_tid(sc, tid, synqe, ntids);
		synqe->tid = tid;
		synqe->syn = m;
		m = NULL;

		if (send_synack(sc, synqe, opt0, opt2, tid) != 0) {
			remove_tid(sc, tid, ntids);
			m = synqe->syn;
			synqe->syn = NULL;
			NET_EPOCH_EXIT(et);
			REJECT_PASS_ACCEPT_REQ(true);
		}

		CTR6(KTR_CXGBE,
		    "%s: stid %u, tid %u, synqe %p, opt0 %#016lx, opt2 %#08x",
		    __func__, stid, tid, synqe, be64toh(opt0), be32toh(opt2));
	} else {
		NET_EPOCH_EXIT(et);
		REJECT_PASS_ACCEPT_REQ(false);
	}

	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (0);
reject:
	CURVNET_RESTORE();
	CTR4(KTR_CXGBE, "%s: stid %u, tid %u, REJECT (%d)", __func__, stid, tid,
	    reject_reason);

	if (e)
		t4_l2t_release(e);
	release_tid(sc, tid, lctx->ctrlq);
	if (synqe) {
		inp = synqe->lctx->inp;
		INP_WLOCK(inp);
		inp = release_synqe(sc, synqe);
		if (inp)
			INP_WUNLOCK(inp);
	}

	if (m) {
		/*
		 * The connection request hit a TOE listener but is being passed
		 * on to the kernel sw stack instead of getting offloaded.
		 */
		m_adj(m, sizeof(*cpl));
		m->m_pkthdr.csum_flags |= (CSUM_IP_CHECKED | CSUM_IP_VALID |
		    CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
		m->m_pkthdr.csum_data = 0xffff;
		hw_ifp->if_input(hw_ifp, m);
	}

	return (reject_reason);
}

static void
synqe_to_protohdrs(struct adapter *sc, struct synq_entry *synqe,
    const struct cpl_pass_establish *cpl, struct in_conninfo *inc,
    struct tcphdr *th, struct tcpopt *to)
{
	uint16_t tcp_opt = be16toh(cpl->tcp_opt);
	uint8_t iptos;

	/* start off with the original SYN */
	pass_accept_req_to_protohdrs(sc, synqe->syn, inc, th, &iptos);

	/* modify parts to make it look like the ACK to our SYN|ACK */
	th->th_flags = TH_ACK;
	th->th_ack = synqe->iss + 1;
	th->th_seq = be32toh(cpl->rcv_isn);
	bzero(to, sizeof(*to));
	if (G_TCPOPT_TSTAMP(tcp_opt)) {
		to->to_flags |= TOF_TS;
		to->to_tsecr = synqe->ts;
	}
}

static int
do_pass_establish(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct vi_info *vi;
	struct ifnet *ifp;
	const struct cpl_pass_establish *cpl = (const void *)(rss + 1);
#if defined(KTR) || defined(INVARIANTS)
	unsigned int stid = G_PASS_OPEN_TID(be32toh(cpl->tos_stid));
#endif
	unsigned int tid = GET_TID(cpl);
	struct synq_entry *synqe = lookup_tid(sc, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp, *new_inp;
	struct socket *so;
	struct tcphdr th;
	struct tcpopt to;
	struct in_conninfo inc;
	struct toepcb *toep;
	struct epoch_tracker et;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_PASS_ESTABLISH,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(lctx->stid == stid, ("%s: lctx stid mismatch", __func__));
	KASSERT(synqe->flags & TPF_SYNQE,
	    ("%s: tid %u (ctx %p) not a synqe", __func__, tid, synqe));

	CURVNET_SET(lctx->vnet);
	NET_EPOCH_ENTER(et);	/* for syncache_expand */
	INP_WLOCK(inp);

	CTR6(KTR_CXGBE,
	    "%s: stid %u, tid %u, synqe %p (0x%x), inp_flags 0x%x",
	    __func__, stid, tid, synqe, synqe->flags, inp->inp_flags);

	ifp = synqe->syn->m_pkthdr.rcvif;
	vi = ifp->if_softc;
	KASSERT(vi->adapter == sc,
	    ("%s: vi %p, sc %p mismatch", __func__, vi, sc));

	if (__predict_false(inp->inp_flags & INP_DROPPED)) {
reset:
		send_reset_synqe(TOEDEV(ifp), synqe);
		INP_WUNLOCK(inp);
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();
		return (0);
	}

	KASSERT(synqe->params.rxq_idx == iq_to_ofld_rxq(iq) - &sc->sge.ofld_rxq[0],
	    ("%s: CPL arrived on unexpected rxq.  %d %d", __func__,
	    synqe->params.rxq_idx,
	    (int)(iq_to_ofld_rxq(iq) - &sc->sge.ofld_rxq[0])));

	toep = alloc_toepcb(vi, M_NOWAIT);
	if (toep == NULL)
		goto reset;
	toep->tid = tid;
	toep->l2te = &sc->l2t->l2tab[synqe->params.l2t_idx];
	toep->vnet = lctx->vnet;
	bcopy(&synqe->params, &toep->params, sizeof(toep->params));
	init_toepcb(vi, toep);

	MPASS(be32toh(cpl->snd_isn) - 1 == synqe->iss);
	MPASS(be32toh(cpl->rcv_isn) - 1 == synqe->irs);
	synqe->tcp_opt = cpl->tcp_opt;
	synqe->toep = toep;

	/* Come up with something that syncache_expand should be ok with. */
	synqe_to_protohdrs(sc, synqe, cpl, &inc, &th, &to);
	if (inc.inc_flags & INC_ISIPV6)
		toep->ce = t4_hold_lip(sc, &inc.inc6_laddr, lctx->ce);
	so = inp->inp_socket;
	KASSERT(so != NULL, ("%s: socket is NULL", __func__));

	if (!toe_syncache_expand(&inc, &to, &th, &so) || so == NULL) {
		free_toepcb(toep);
		goto reset;
	}

	/* New connection inpcb is already locked by syncache_expand(). */
	new_inp = sotoinpcb(so);
	INP_WLOCK_ASSERT(new_inp);
	MPASS(so->so_vnet == lctx->vnet);

	/*
	 * This is for expansion from syncookies.
	 *
	 * XXX: we've held the tcbinfo lock throughout so there's no risk of
	 * anyone accept'ing a connection before we've installed our hooks, but
	 * this somewhat defeats the purpose of having a tod_offload_socket :-(
	 */
	if (__predict_false(!(synqe->flags & TPF_SYNQE_EXPANDED))) {
		tcp_timer_activate(intotcpcb(new_inp), TT_KEEP, 0);
		t4_offload_socket(TOEDEV(ifp), synqe, so);
	}

	INP_WUNLOCK(new_inp);

	/* Done with the synqe */
	inp = release_synqe(sc, synqe);
	if (inp != NULL)
		INP_WUNLOCK(inp);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (0);
}

void
t4_init_listen_cpl_handlers(void)
{

	t4_register_cpl_handler(CPL_PASS_OPEN_RPL, do_pass_open_rpl);
	t4_register_cpl_handler(CPL_CLOSE_LISTSRV_RPL, do_close_server_rpl);
	t4_register_cpl_handler(CPL_PASS_ACCEPT_REQ, do_pass_accept_req);
	t4_register_cpl_handler(CPL_PASS_ESTABLISH, do_pass_establish);
}

void
t4_uninit_listen_cpl_handlers(void)
{

	t4_register_cpl_handler(CPL_PASS_OPEN_RPL, NULL);
	t4_register_cpl_handler(CPL_CLOSE_LISTSRV_RPL, NULL);
	t4_register_cpl_handler(CPL_PASS_ACCEPT_REQ, NULL);
	t4_register_cpl_handler(CPL_PASS_ESTABLISH, NULL);
}
#endif
