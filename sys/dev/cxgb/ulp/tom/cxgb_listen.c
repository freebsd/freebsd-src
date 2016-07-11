/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
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

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/refcount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_fib.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/tcp_timer.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>

#include "cxgb_include.h"
#include "ulp/tom/cxgb_tom.h"
#include "ulp/tom/cxgb_l2t.h"
#include "ulp/tom/cxgb_toepcb.h"

static void t3_send_reset_synqe(struct toedev *, struct synq_entry *);

static int
alloc_stid(struct tid_info *t, void *ctx)
{
	int stid = -1;

	mtx_lock(&t->stid_lock);
	if (t->sfree) {
		union listen_entry *p = t->sfree;

		stid = (p - t->stid_tab) + t->stid_base;
		t->sfree = p->next;
		p->ctx = ctx;
		t->stids_in_use++;
	}
	mtx_unlock(&t->stid_lock);
	return (stid);
}

static void
free_stid(struct tid_info *t, int stid)
{
	union listen_entry *p = stid2entry(t, stid);

	mtx_lock(&t->stid_lock);
	p->next = t->sfree;
	t->sfree = p;
	t->stids_in_use--;
	mtx_unlock(&t->stid_lock);
}

static struct listen_ctx *
alloc_lctx(struct tom_data *td, struct inpcb *inp, int qset)
{
	struct listen_ctx *lctx;

	INP_WLOCK_ASSERT(inp);

	lctx = malloc(sizeof(struct listen_ctx), M_CXGB, M_NOWAIT | M_ZERO);
	if (lctx == NULL)
		return (NULL);

	lctx->stid = alloc_stid(&td->tid_maps, lctx);
	if (lctx->stid < 0) {
		free(lctx, M_CXGB);
		return (NULL);
	}

	lctx->inp = inp;
	in_pcbref(inp);

	lctx->qset = qset;
	refcount_init(&lctx->refcnt, 1);
	TAILQ_INIT(&lctx->synq);

	return (lctx);
}

/* Don't call this directly, use release_lctx instead */
static int
free_lctx(struct tom_data *td, struct listen_ctx *lctx)
{
	struct inpcb *inp = lctx->inp;

	INP_WLOCK_ASSERT(inp);
	KASSERT(lctx->refcnt == 0,
	    ("%s: refcnt %d", __func__, lctx->refcnt));
	KASSERT(TAILQ_EMPTY(&lctx->synq),
	    ("%s: synq not empty.", __func__));
	KASSERT(lctx->stid >= 0, ("%s: bad stid %d.", __func__, lctx->stid));

	CTR4(KTR_CXGB, "%s: stid %u, lctx %p, inp %p",
	    __func__, lctx->stid, lctx, lctx->inp);

	free_stid(&td->tid_maps, lctx->stid);
	free(lctx, M_CXGB);

	return in_pcbrele_wlocked(inp);
}

static void
hold_lctx(struct listen_ctx *lctx)
{

	refcount_acquire(&lctx->refcnt);
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
listen_hash_add(struct tom_data *td, struct listen_ctx *lctx)
{
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
listen_hash_find(struct tom_data *td, struct inpcb *inp)
{
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
listen_hash_del(struct tom_data *td, struct inpcb *inp)
{
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
release_lctx(struct tom_data *td, struct listen_ctx *lctx)
{
	struct inpcb *inp = lctx->inp;
	int inp_freed = 0;

	INP_WLOCK_ASSERT(inp);
	if (refcount_release(&lctx->refcnt))
		inp_freed = free_lctx(td, lctx);

	return (inp_freed ? NULL : inp);
}

static int
create_server(struct adapter *sc, struct listen_ctx *lctx)
{
	struct mbuf *m;
	struct cpl_pass_open_req *req;
	struct inpcb *inp = lctx->inp;

	m = M_GETHDR_OFLD(lctx->qset, CPL_PRIORITY_CONTROL, req);
	if (m == NULL)
		return (ENOMEM);

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_PASS_OPEN_REQ, lctx->stid));
	req->local_port = inp->inp_lport; 
	memcpy(&req->local_ip, &inp->inp_laddr, 4);
	req->peer_port = 0;
	req->peer_ip = 0;
	req->peer_netmask = 0;
	req->opt0h = htonl(F_DELACK | F_TCAM_BYPASS);
	req->opt0l = htonl(V_RCV_BUFSIZ(16));
	req->opt1 = htonl(V_CONN_POLICY(CPL_CONN_POLICY_ASK));

	t3_offload_tx(sc, m);

	return (0);
}

static int
destroy_server(struct adapter *sc, struct listen_ctx *lctx)
{
	struct mbuf *m;
	struct cpl_close_listserv_req *req;

	m = M_GETHDR_OFLD(lctx->qset, CPL_PRIORITY_CONTROL, req);
	if (m == NULL)
		return (ENOMEM);

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_LISTSRV_REQ,
	    lctx->stid));
	req->cpu_idx = 0;

	t3_offload_tx(sc, m);

	return (0);
}

/*
 * Process a CPL_CLOSE_LISTSRV_RPL message.  If the status is good we release
 * the STID.
 */
static int
do_close_server_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct cpl_close_listserv_rpl *rpl = mtod(m, void *);
	unsigned int stid = GET_TID(rpl);
	struct listen_ctx *lctx = lookup_stid(&td->tid_maps, stid);
	struct inpcb *inp = lctx->inp;

	CTR3(KTR_CXGB, "%s: stid %u, status %u", __func__, stid, rpl->status);

	if (rpl->status != CPL_ERR_NONE) {
		log(LOG_ERR, "%s: failed (%u) to close listener for stid %u",
		    __func__, rpl->status, stid);
	} else {
		INP_WLOCK(inp);
		KASSERT(listen_hash_del(td, lctx->inp) == NULL,
		    ("%s: inp %p still in listen hash", __func__, inp));
		if (release_lctx(td, lctx) != NULL)
			INP_WUNLOCK(inp);
	}

	m_freem(m);
	return (0);
}

/*
 * Process a CPL_PASS_OPEN_RPL message.  Remove the lctx from the listen hash
 * table and free it if there was any error, otherwise nothing to do.
 */
static int
do_pass_open_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
       	struct cpl_pass_open_rpl *rpl = mtod(m, void *);
	int stid = GET_TID(rpl);
	struct listen_ctx *lctx;
	struct inpcb *inp;

	/*
	 * We get these replies also when setting up HW filters.  Just throw
	 * those away.
	 */
	if (stid >= td->tid_maps.stid_base + td->tid_maps.nstids)
		goto done;

	lctx = lookup_stid(&td->tid_maps, stid);
	inp = lctx->inp;

	INP_WLOCK(inp);

	CTR4(KTR_CXGB, "%s: stid %u, status %u, flags 0x%x",
	    __func__, stid, rpl->status, lctx->flags);

	lctx->flags &= ~LCTX_RPL_PENDING;

	if (rpl->status != CPL_ERR_NONE) {
		log(LOG_ERR, "%s: %s: hw listen (stid %d) failed: %d\n",
		    __func__, device_get_nameunit(sc->dev), stid, rpl->status);
	}

#ifdef INVARIANTS
	/*
	 * If the inp has been dropped (listening socket closed) then
	 * listen_stop must have run and taken the inp out of the hash.
	 */
	if (inp->inp_flags & INP_DROPPED) {
		KASSERT(listen_hash_del(td, inp) == NULL,
		    ("%s: inp %p still in listen hash", __func__, inp));
	}
#endif

	if (inp->inp_flags & INP_DROPPED && rpl->status != CPL_ERR_NONE) {
		if (release_lctx(td, lctx) != NULL)
			INP_WUNLOCK(inp);
		goto done;
	}

	/*
	 * Listening socket stopped listening earlier and now the chip tells us
	 * it has started the hardware listener.  Stop it; the lctx will be
	 * released in do_close_server_rpl.
	 */
	if (inp->inp_flags & INP_DROPPED) {
		destroy_server(sc, lctx);
		INP_WUNLOCK(inp);
		goto done;
	}

	/*
	 * Failed to start hardware listener.  Take inp out of the hash and
	 * release our reference on it.  An error message has been logged
	 * already.
	 */
	if (rpl->status != CPL_ERR_NONE) {
		listen_hash_del(td, inp);
		if (release_lctx(td, lctx) != NULL)
			INP_WUNLOCK(inp);
		goto done;
	}

	/* hardware listener open for business */

	INP_WUNLOCK(inp);
done:
	m_freem(m);
	return (0);
}

static void
pass_accept_req_to_protohdrs(const struct cpl_pass_accept_req *cpl,
    struct in_conninfo *inc, struct tcphdr *th, struct tcpopt *to)
{
	const struct tcp_options *t3opt = &cpl->tcp_options;

	bzero(inc, sizeof(*inc));
	inc->inc_faddr.s_addr = cpl->peer_ip;
	inc->inc_laddr.s_addr = cpl->local_ip;
	inc->inc_fport = cpl->peer_port;
	inc->inc_lport = cpl->local_port;

	bzero(th, sizeof(*th));
	th->th_sport = cpl->peer_port;
	th->th_dport = cpl->local_port;
	th->th_seq = be32toh(cpl->rcv_isn); /* as in tcp_fields_to_host */
	th->th_flags = TH_SYN;

	bzero(to, sizeof(*to));
	if (t3opt->mss) {
		to->to_flags |= TOF_MSS;
		to->to_mss = be16toh(t3opt->mss);
	}
	if (t3opt->wsf) {
		to->to_flags |= TOF_SCALE;
		to->to_wscale = t3opt->wsf;
	}
	if (t3opt->tstamp)
		to->to_flags |= TOF_TS;
	if (t3opt->sack)
		to->to_flags |= TOF_SACKPERM;
}

static inline void
hold_synqe(struct synq_entry *synqe)
{

	refcount_acquire(&synqe->refcnt);
}

static inline void
release_synqe(struct synq_entry *synqe)
{

	if (refcount_release(&synqe->refcnt))
		m_freem(synqe->m);
}

/*
 * Use the trailing space in the mbuf in which the PASS_ACCEPT_REQ arrived to
 * store some state temporarily.  There will be enough room in the mbuf's
 * trailing space as the CPL is not that large.
 *
 * XXX: bad hack.
 */
static struct synq_entry *
mbuf_to_synq_entry(struct mbuf *m)
{
	int len = roundup(sizeof (struct synq_entry), 8);

	if (__predict_false(M_TRAILINGSPACE(m) < len)) {
	    panic("%s: no room for synq_entry (%td, %d)\n", __func__,
	    M_TRAILINGSPACE(m), len);
	}

	return ((void *)(M_START(m) + M_SIZE(m) - len));
}

#ifdef KTR
#define REJECT_PASS_ACCEPT()	do { \
	reject_reason = __LINE__; \
	goto reject; \
} while (0)
#else
#define REJECT_PASS_ACCEPT()	do { goto reject; } while (0)
#endif

/*
 * The context associated with a tid entry via insert_tid could be a synq_entry
 * or a toepcb.  The only way CPL handlers can tell is via a bit in these flags.
 */
CTASSERT(offsetof(struct toepcb, tp_flags) == offsetof(struct synq_entry, flags));

/*
 * Handle a CPL_PASS_ACCEPT_REQ message.
 */
static int
do_pass_accept_req(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct toedev *tod = &td->tod;
	const struct cpl_pass_accept_req *req = mtod(m, void *);
	unsigned int stid = G_PASS_OPEN_TID(ntohl(req->tos_tid));
	unsigned int tid = GET_TID(req);
	struct listen_ctx *lctx = lookup_stid(&td->tid_maps, stid);
	struct l2t_entry *e = NULL;
	struct nhop4_basic nh4;
	struct sockaddr_in nam;
	struct inpcb *inp;
	struct socket *so;
	struct port_info *pi;
	struct ifnet *ifp;
	struct in_conninfo inc;
	struct tcphdr th;
	struct tcpopt to;
	struct synq_entry *synqe = NULL;
	int i;
#ifdef KTR
	int reject_reason;
#endif

	CTR4(KTR_CXGB, "%s: stid %u, tid %u, lctx %p", __func__, stid, tid,
	    lctx);

	pass_accept_req_to_protohdrs(req, &inc, &th, &to);

	/*
	 * Don't offload if the interface that received the SYN doesn't have
	 * IFCAP_TOE enabled.
	 */
	pi = NULL;
	for_each_port(sc, i) {
		if (memcmp(sc->port[i].hw_addr, req->dst_mac, ETHER_ADDR_LEN))
			continue;
		pi = &sc->port[i];
		break;
	}
	if (pi == NULL)
		REJECT_PASS_ACCEPT();
	ifp = pi->ifp;
	if ((ifp->if_capenable & IFCAP_TOE4) == 0)
		REJECT_PASS_ACCEPT();

	/*
	 * Don't offload if the outgoing interface for the route back to the
	 * peer is not the same as the interface that received the SYN.
	 */
	bzero(&nam, sizeof(nam));
	nam.sin_len = sizeof(nam);
	nam.sin_family = AF_INET;
	nam.sin_addr = inc.inc_faddr;
	if (fib4_lookup_nh_basic(RT_DEFAULT_FIB, nam.sin_addr, 0, 0, &nh4) != 0)
		REJECT_PASS_ACCEPT();
	else {
		nam.sin_addr = nh4.nh_addr;
		if (nh4.nh_ifp == ifp)
			e = t3_l2t_get(pi, ifp, (struct sockaddr *)&nam);
		if (e == NULL)
			REJECT_PASS_ACCEPT();	/* no l2te, or ifp mismatch */
	}

	INP_INFO_RLOCK(&V_tcbinfo);

	/* Don't offload if the 4-tuple is already in use */
	if (toe_4tuple_check(&inc, &th, ifp) != 0) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
		REJECT_PASS_ACCEPT();
	}

	inp = lctx->inp;	/* listening socket (not owned by the TOE) */
	INP_WLOCK(inp);
	if (__predict_false(inp->inp_flags & INP_DROPPED)) {
		/*
		 * The listening socket has closed.  The reply from the TOE to
		 * our CPL_CLOSE_LISTSRV_REQ will ultimately release all
		 * resources tied to this listen context.
		 */
		INP_WUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);
		REJECT_PASS_ACCEPT();
	}
	so = inp->inp_socket;

	/* Reuse the mbuf that delivered the CPL to us */
	synqe = mbuf_to_synq_entry(m);
	synqe->flags = TP_IS_A_SYNQ_ENTRY;
	synqe->m = m;
	synqe->lctx = lctx;
	synqe->tid = tid;
	synqe->e = e;
	synqe->opt0h = calc_opt0h(so, 0, 0, e);
	synqe->qset = pi->first_qset + (arc4random() % pi->nqsets);
	SOCKBUF_LOCK(&so->so_rcv);
	synqe->rx_credits = min(select_rcv_wnd(so) >> 10, M_RCV_BUFSIZ);
	SOCKBUF_UNLOCK(&so->so_rcv);
	refcount_init(&synqe->refcnt, 1);
	atomic_store_rel_int(&synqe->reply, RPL_OK);

	insert_tid(td, synqe, tid);
	TAILQ_INSERT_TAIL(&lctx->synq, synqe, link);
	hold_synqe(synqe);
	hold_lctx(lctx);

	/* syncache_add releases both pcbinfo and pcb locks */
	toe_syncache_add(&inc, &to, &th, inp, tod, synqe);
	INP_UNLOCK_ASSERT(inp);
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);

	/*
	 * If we replied during syncache_add (reply is RPL_DONE), good.
	 * Otherwise (reply is unchanged - RPL_OK) it's no longer ok to reply.
	 * The mbuf will stick around as long as the entry is in the syncache.
	 * The kernel is free to retry syncache_respond but we'll ignore it due
	 * to RPL_DONT.
	 */
	if (atomic_cmpset_int(&synqe->reply, RPL_OK, RPL_DONT)) {

		INP_WLOCK(inp);
		if (__predict_false(inp->inp_flags & INP_DROPPED)) {
			/* listener closed.  synqe must have been aborted. */
			KASSERT(synqe->flags & TP_ABORT_SHUTDOWN,
			    ("%s: listener %p closed but synqe %p not aborted",
			    __func__, inp, synqe));

			CTR5(KTR_CXGB,
			    "%s: stid %u, tid %u, lctx %p, synqe %p, ABORTED",
			    __func__, stid, tid, lctx, synqe);
			INP_WUNLOCK(inp);
			release_synqe(synqe);
			return (__LINE__);
		}

		KASSERT(!(synqe->flags & TP_ABORT_SHUTDOWN),
		    ("%s: synqe %p aborted, but listener %p not dropped.",
		    __func__, synqe, inp));

		TAILQ_REMOVE(&lctx->synq, synqe, link);
		release_synqe(synqe);	/* removed from synq list */
		inp = release_lctx(td, lctx);
		if (inp)
			INP_WUNLOCK(inp);

		release_synqe(synqe);	/* about to exit function */
		REJECT_PASS_ACCEPT();
	}

	KASSERT(synqe->reply == RPL_DONE,
	    ("%s: reply %d", __func__, synqe->reply));

	CTR3(KTR_CXGB, "%s: stid %u, tid %u, OK", __func__, stid, tid);
	release_synqe(synqe);
	return (0);

reject:
	CTR4(KTR_CXGB, "%s: stid %u, tid %u, REJECT (%d)", __func__, stid, tid,
	    reject_reason);

	if (synqe == NULL)
		m_freem(m);
	if (e)
		l2t_release(td->l2t, e);
	queue_tid_release(tod, tid);

	return (0);
}

static void
pass_establish_to_protohdrs(const struct cpl_pass_establish *cpl,
    struct in_conninfo *inc, struct tcphdr *th, struct tcpopt *to)
{
	uint16_t tcp_opt = be16toh(cpl->tcp_opt);

	bzero(inc, sizeof(*inc));
	inc->inc_faddr.s_addr = cpl->peer_ip;
	inc->inc_laddr.s_addr = cpl->local_ip;
	inc->inc_fport = cpl->peer_port;
	inc->inc_lport = cpl->local_port;

	bzero(th, sizeof(*th));
	th->th_sport = cpl->peer_port;
	th->th_dport = cpl->local_port;
	th->th_flags = TH_ACK;
	th->th_seq = be32toh(cpl->rcv_isn); /* as in tcp_fields_to_host */
	th->th_ack = be32toh(cpl->snd_isn); /* ditto */

	bzero(to, sizeof(*to));
	if (G_TCPOPT_TSTAMP(tcp_opt))
		to->to_flags |= TOF_TS;
}

/*
 * Process a CPL_PASS_ESTABLISH message.  The T3 has already established a
 * connection and we need to do the software side setup.
 */
static int
do_pass_establish(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct cpl_pass_establish *cpl = mtod(m, void *);
	struct toedev *tod = &td->tod;
	unsigned int tid = GET_TID(cpl);
	struct synq_entry *synqe = lookup_tid(&td->tid_maps, tid);
	struct toepcb *toep;
	struct socket *so;
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp, *new_inp;
	struct tcpopt to;
	struct tcphdr th;
	struct in_conninfo inc;
#ifdef KTR
	int stid = G_PASS_OPEN_TID(ntohl(cpl->tos_tid));
#endif

	CTR5(KTR_CXGB, "%s: stid %u, tid %u, lctx %p, inp_flags 0x%x",
	    __func__, stid, tid, lctx, inp->inp_flags);

	KASSERT(qs->idx == synqe->qset,
	    ("%s qset mismatch %d %d", __func__, qs->idx, synqe->qset));

	INP_INFO_RLOCK(&V_tcbinfo);	/* for syncache_expand */
	INP_WLOCK(inp);

	if (__predict_false(inp->inp_flags & INP_DROPPED)) {
		/*
		 * The listening socket has closed.  The TOM must have aborted
		 * all the embryonic connections (including this one) that were
		 * on the lctx's synq.  do_abort_rpl for the tid is responsible
		 * for cleaning up.
		 */
		KASSERT(synqe->flags & TP_ABORT_SHUTDOWN,
		    ("%s: listen socket dropped but tid %u not aborted.",
		    __func__, tid));
		INP_WUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);
		m_freem(m);
		return (0);
	}

	pass_establish_to_protohdrs(cpl, &inc, &th, &to);

	/* Lie in order to pass the checks in syncache_expand */
	to.to_tsecr = synqe->ts;
	th.th_ack = synqe->iss + 1;

	toep = toepcb_alloc(tod);
	if (toep == NULL) {
reset:
		t3_send_reset_synqe(tod, synqe);
		INP_WUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);
		m_freem(m);
		return (0);
	}
	toep->tp_qset = qs->idx;
	toep->tp_l2t = synqe->e;
	toep->tp_tid = tid;
	toep->tp_rx_credits = synqe->rx_credits;

	synqe->toep = toep;
	synqe->cpl = cpl;

	so = inp->inp_socket;
	if (!toe_syncache_expand(&inc, &to, &th, &so) || so == NULL) {
		toepcb_free(toep);
		goto reset;
	}

	/* New connection inpcb is already locked by syncache_expand(). */
	new_inp = sotoinpcb(so);
	INP_WLOCK_ASSERT(new_inp);

	if (__predict_false(!(synqe->flags & TP_SYNQE_EXPANDED))) {
		tcp_timer_activate(intotcpcb(new_inp), TT_KEEP, 0);
		t3_offload_socket(tod, synqe, so);
	}

	INP_WUNLOCK(new_inp);

	/* Remove the synq entry and release its reference on the lctx */
	TAILQ_REMOVE(&lctx->synq, synqe, link);
	inp = release_lctx(td, lctx);
	if (inp)
		INP_WUNLOCK(inp);
	INP_INFO_RUNLOCK(&V_tcbinfo);
	release_synqe(synqe);

	m_freem(m);
	return (0);
}

void
t3_init_listen_cpl_handlers(struct adapter *sc)
{
	t3_register_cpl_handler(sc, CPL_PASS_OPEN_RPL, do_pass_open_rpl);
	t3_register_cpl_handler(sc, CPL_CLOSE_LISTSRV_RPL, do_close_server_rpl);
	t3_register_cpl_handler(sc, CPL_PASS_ACCEPT_REQ, do_pass_accept_req);
	t3_register_cpl_handler(sc, CPL_PASS_ESTABLISH, do_pass_establish);
}

/*
 * Start a listening server by sending a passive open request to HW.
 *
 * Can't take adapter lock here and access to sc->flags, sc->open_device_map,
 * sc->offload_map, if_capenable are all race prone.
 */
int
t3_listen_start(struct toedev *tod, struct tcpcb *tp)
{
	struct tom_data *td = t3_tomdata(tod);
	struct adapter *sc = tod->tod_softc;
	struct port_info *pi;
	struct inpcb *inp = tp->t_inpcb;
	struct listen_ctx *lctx;
	int i;

	INP_WLOCK_ASSERT(inp);

	if ((inp->inp_vflag & INP_IPV4) == 0)
		return (0);

#ifdef notyet
	ADAPTER_LOCK(sc);
	if (IS_BUSY(sc)) {
		log(LOG_ERR, "%s: listen request ignored, %s is busy",
		    __func__, device_get_nameunit(sc->dev));
		goto done;
	}

	KASSERT(sc->flags & TOM_INIT_DONE,
	    ("%s: TOM not initialized", __func__));
#endif

	if ((sc->open_device_map & sc->offload_map) == 0)
		goto done;	/* no port that's UP with IFCAP_TOE enabled */

	/*
	 * Find a running port with IFCAP_TOE4.  We'll use the first such port's
	 * queues to send the passive open and receive the reply to it.
	 *
	 * XXX: need a way to mark an port in use by offload.  if_cxgbe should
	 * then reject any attempt to bring down such a port (and maybe reject
	 * attempts to disable IFCAP_TOE on that port too?).
	 */
	for_each_port(sc, i) {
		if (isset(&sc->open_device_map, i) &&
		    sc->port[i].ifp->if_capenable & IFCAP_TOE4)
				break;
	}
	KASSERT(i < sc->params.nports,
	    ("%s: no running port with TOE capability enabled.", __func__));
	pi = &sc->port[i];

	if (listen_hash_find(td, inp) != NULL)
		goto done;	/* already setup */

	lctx = alloc_lctx(td, inp, pi->first_qset);
	if (lctx == NULL) {
		log(LOG_ERR,
		    "%s: listen request ignored, %s couldn't allocate lctx\n",
		    __func__, device_get_nameunit(sc->dev));
		goto done;
	}
	listen_hash_add(td, lctx);

	CTR5(KTR_CXGB, "%s: stid %u (%s), lctx %p, inp %p", __func__,
	    lctx->stid, tcpstates[tp->t_state], lctx, inp);

	if (create_server(sc, lctx) != 0) {
		log(LOG_ERR, "%s: %s failed to create hw listener.\n", __func__,
		    device_get_nameunit(sc->dev));
		(void) listen_hash_del(td, inp);
		inp = release_lctx(td, lctx);
		/* can't be freed, host stack has a reference */
		KASSERT(inp != NULL, ("%s: inp freed", __func__));
		goto done;
	}
	lctx->flags |= LCTX_RPL_PENDING;
done:
#ifdef notyet
	ADAPTER_UNLOCK(sc);
#endif
	return (0);
}

/*
 * Stop a listening server by sending a close_listsvr request to HW.
 * The server TID is freed when we get the reply.
 */
int
t3_listen_stop(struct toedev *tod, struct tcpcb *tp)
{
	struct listen_ctx *lctx;
	struct adapter *sc = tod->tod_softc;
	struct tom_data *td = t3_tomdata(tod);
	struct inpcb *inp = tp->t_inpcb;
	struct synq_entry *synqe;

	INP_WLOCK_ASSERT(inp);

	lctx = listen_hash_del(td, inp);
	if (lctx == NULL)
		return (ENOENT);	/* no hardware listener for this inp */

	CTR4(KTR_CXGB, "%s: stid %u, lctx %p, flags %x", __func__, lctx->stid,
	    lctx, lctx->flags);

	/*
	 * If the reply to the PASS_OPEN is still pending we'll wait for it to
	 * arrive and clean up when it does.
	 */
	if (lctx->flags & LCTX_RPL_PENDING) {
		KASSERT(TAILQ_EMPTY(&lctx->synq),
		    ("%s: synq not empty.", __func__));
		return (EINPROGRESS);
	}

	/*
	 * The host stack will abort all the connections on the listening
	 * socket's so_comp.  It doesn't know about the connections on the synq
	 * so we need to take care of those.
	 */
	TAILQ_FOREACH(synqe, &lctx->synq, link) {
		KASSERT(synqe->lctx == lctx, ("%s: synq corrupt", __func__));
		t3_send_reset_synqe(tod, synqe);
	}

	destroy_server(sc, lctx);
	return (0);
}

void
t3_syncache_added(struct toedev *tod __unused, void *arg)
{
	struct synq_entry *synqe = arg;

	hold_synqe(synqe);
}

void
t3_syncache_removed(struct toedev *tod __unused, void *arg)
{
	struct synq_entry *synqe = arg;

	release_synqe(synqe);
}

int
t3_syncache_respond(struct toedev *tod, void *arg, struct mbuf *m)
{
	struct adapter *sc = tod->tod_softc;
	struct synq_entry *synqe = arg;
	struct l2t_entry *e = synqe->e;
	struct ip *ip = mtod(m, struct ip *);
	struct tcphdr *th = (void *)(ip + 1);
	struct cpl_pass_accept_rpl *rpl;
	struct mbuf *r;
	struct listen_ctx *lctx = synqe->lctx;
	struct tcpopt to;
	int mtu_idx, cpu_idx;

	/*
	 * The first time we run it's during the call to syncache_add.  That's
	 * the only one we care about.
	 */
	if (atomic_cmpset_int(&synqe->reply, RPL_OK, RPL_DONE) == 0)
		goto done;	/* reply to the CPL only if it's ok to do so */

	r = M_GETHDR_OFLD(lctx->qset, CPL_PRIORITY_CONTROL, rpl);
	if (r == NULL)
		goto done;

	/*
	 * Use only the provided mbuf (with ip and tcp headers) and what's in
	 * synqe.  Avoid looking at the listening socket (lctx->inp) here.
	 *
	 * XXX: if the incoming SYN had the TCP timestamp option but the kernel
	 * decides it doesn't want to use TCP timestamps we have no way of
	 * relaying this info to the chip on a per-tid basis (all we have is a
	 * global knob).
	 */
	bzero(&to, sizeof(to));
	tcp_dooptions(&to, (void *)(th + 1), (th->th_off << 2) - sizeof(*th),
	    TO_SYN);

	/* stash them for later */
	synqe->iss = be32toh(th->th_seq);
	synqe->ts = to.to_tsval;

	mtu_idx = find_best_mtu_idx(sc, NULL, to.to_mss);
	cpu_idx = sc->rrss_map[synqe->qset];

	rpl->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	rpl->wr.wrh_lo = 0;
	OPCODE_TID(rpl) = htonl(MK_OPCODE_TID(CPL_PASS_ACCEPT_RPL, synqe->tid));
	rpl->opt2 = calc_opt2(cpu_idx);
	rpl->rsvd = rpl->opt2;		/* workaround for HW bug */
	rpl->peer_ip = ip->ip_dst.s_addr;
	rpl->opt0h = synqe->opt0h |
	    calc_opt0h(NULL, mtu_idx, to.to_wscale, NULL);
	rpl->opt0l_status = htobe32(CPL_PASS_OPEN_ACCEPT) |
	    calc_opt0l(NULL, synqe->rx_credits);

	l2t_send(sc, r, e);
done:
	m_freem(m);
	return (0);
}

int
do_abort_req_synqe(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct toedev *tod = &td->tod;
	const struct cpl_abort_req_rss *req = mtod(m, void *);
	unsigned int tid = GET_TID(req);
	struct synq_entry *synqe = lookup_tid(&td->tid_maps, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;

	KASSERT(synqe->flags & TP_IS_A_SYNQ_ENTRY,
	    ("%s: !SYNQ_ENTRY", __func__));

	CTR6(KTR_CXGB, "%s: tid %u, synqe %p (%x), lctx %p, status %d",
	    __func__, tid, synqe, synqe->flags, synqe->lctx, req->status);

	INP_WLOCK(inp);

	if (!(synqe->flags & TP_ABORT_REQ_RCVD)) {
		synqe->flags |= TP_ABORT_REQ_RCVD;
		synqe->flags |= TP_ABORT_SHUTDOWN;
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}
	synqe->flags &= ~TP_ABORT_REQ_RCVD;

	/*
	 * If we'd sent a reset on this synqe, we'll ignore this and clean up in
	 * the T3's reply to our reset instead.
	 */
	if (synqe->flags & TP_ABORT_RPL_PENDING) {
		synqe->flags |= TP_ABORT_RPL_SENT;
		INP_WUNLOCK(inp);
	} else {
		TAILQ_REMOVE(&lctx->synq, synqe, link);
		inp = release_lctx(td, lctx);
		if (inp)
			INP_WUNLOCK(inp);
		release_tid(tod, tid, qs->idx);
		l2t_release(td->l2t, synqe->e);
		release_synqe(synqe);
	}

	send_abort_rpl(tod, tid, qs->idx);
	m_freem(m);
	return (0);
}

int
do_abort_rpl_synqe(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct toedev *tod = &td->tod;
	const struct cpl_abort_rpl_rss *rpl = mtod(m, void *);
	unsigned int tid = GET_TID(rpl);
	struct synq_entry *synqe = lookup_tid(&td->tid_maps, tid);
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;

	CTR3(KTR_CXGB, "%s: tid %d, synqe %p, status %d", tid, synqe,
	    rpl->status);

	INP_WLOCK(inp);

	if (synqe->flags & TP_ABORT_RPL_PENDING) {
		if (!(synqe->flags & TP_ABORT_RPL_RCVD)) {
			synqe->flags |= TP_ABORT_RPL_RCVD;
			INP_WUNLOCK(inp);
		} else {
			synqe->flags &= ~TP_ABORT_RPL_RCVD;
			synqe->flags &= TP_ABORT_RPL_PENDING;

			TAILQ_REMOVE(&lctx->synq, synqe, link);
			inp = release_lctx(td, lctx);
			if (inp)
				INP_WUNLOCK(inp);
			release_tid(tod, tid, qs->idx);
			l2t_release(td->l2t, synqe->e);
			release_synqe(synqe);
		}
	}

	m_freem(m);
	return (0);
}

static void
t3_send_reset_synqe(struct toedev *tod, struct synq_entry *synqe)
{
	struct cpl_abort_req *req;
	unsigned int tid = synqe->tid;
	struct adapter *sc = tod->tod_softc;
	struct mbuf *m;
#ifdef INVARIANTS
	struct listen_ctx *lctx = synqe->lctx;
	struct inpcb *inp = lctx->inp;
#endif

	INP_WLOCK_ASSERT(inp);

	CTR4(KTR_CXGB, "%s: tid %d, synqe %p (%x)", __func__, tid, synqe,
	    synqe->flags);

	if (synqe->flags & TP_ABORT_SHUTDOWN)
		return;

	synqe->flags |= (TP_ABORT_RPL_PENDING | TP_ABORT_SHUTDOWN);

	m = M_GETHDR_OFLD(synqe->qset, CPL_PRIORITY_DATA, req);
	if (m == NULL)
		CXGB_UNIMPLEMENTED();

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_HOST_ABORT_CON_REQ));
	req->wr.wrh_lo = htonl(V_WR_TID(tid));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_ABORT_REQ, tid));
	req->rsvd0 = 0;
	req->rsvd1 = !(synqe->flags & TP_DATASENT);
	req->cmd = CPL_ABORT_SEND_RST;

	l2t_send(sc, m, synqe->e);
}

void
t3_offload_socket(struct toedev *tod, void *arg, struct socket *so)
{
	struct adapter *sc = tod->tod_softc;
	struct tom_data *td = sc->tom_softc;
	struct synq_entry *synqe = arg;
#ifdef INVARIANTS
	struct inpcb *inp = sotoinpcb(so);
#endif
	struct cpl_pass_establish *cpl = synqe->cpl;
	struct toepcb *toep = synqe->toep;

	INP_INFO_RLOCK_ASSERT(&V_tcbinfo); /* prevents bad race with accept() */
	INP_WLOCK_ASSERT(inp);

	offload_socket(so, toep);
	make_established(so, cpl->snd_isn, cpl->rcv_isn, cpl->tcp_opt);
	update_tid(td, toep, synqe->tid);
	synqe->flags |= TP_SYNQE_EXPANDED;
}
#endif
