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
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/sockstate.h>
#include <sys/sockopt.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockbuf.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/priv.h>
#include <sys/sglist.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

#include <netinet/ip.h>
#include <netinet/tcp_var.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/toecore.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <net/route.h>

#include "cxgb_include.h"
#include "ulp/tom/cxgb_l2t.h"
#include "ulp/tom/cxgb_tom.h"
#include "ulp/tom/cxgb_toepcb.h"

VNET_DECLARE(int, tcp_do_autosndbuf);
#define V_tcp_do_autosndbuf VNET(tcp_do_autosndbuf)
VNET_DECLARE(int, tcp_autosndbuf_inc);
#define V_tcp_autosndbuf_inc VNET(tcp_autosndbuf_inc)
VNET_DECLARE(int, tcp_autosndbuf_max);
#define V_tcp_autosndbuf_max VNET(tcp_autosndbuf_max)
VNET_DECLARE(int, tcp_do_autorcvbuf);
#define V_tcp_do_autorcvbuf VNET(tcp_do_autorcvbuf)
VNET_DECLARE(int, tcp_autorcvbuf_inc);
#define V_tcp_autorcvbuf_inc VNET(tcp_autorcvbuf_inc)
VNET_DECLARE(int, tcp_autorcvbuf_max);
#define V_tcp_autorcvbuf_max VNET(tcp_autorcvbuf_max)
extern int always_keepalive;

/*
 * For ULP connections HW may add headers, e.g., for digests, that aren't part
 * of the messages sent by the host but that are part of the TCP payload and
 * therefore consume TCP sequence space.  Tx connection parameters that
 * operate in TCP sequence space are affected by the HW additions and need to
 * compensate for them to accurately track TCP sequence numbers. This array
 * contains the compensating extra lengths for ULP packets.  It is indexed by
 * a packet's ULP submode.
 */
const unsigned int t3_ulp_extra_len[] = {0, 4, 4, 8};

/*
 * Max receive window supported by HW in bytes.  Only a small part of it can
 * be set through option0, the rest needs to be set through RX_DATA_ACK.
 */
#define MAX_RCV_WND ((1U << 27) - 1)

/*
 * Min receive window.  We want it to be large enough to accommodate receive
 * coalescing, handle jumbo frames, and not trigger sender SWS avoidance.
 */
#define MIN_RCV_WND (24 * 1024U)
#define INP_TOS(inp) ((inp_ip_tos_get(inp) >> 2) & M_TOS)

static void t3_release_offload_resources(struct toepcb *);
static void send_reset(struct toepcb *toep);

/*
 * Called after the last CPL for the toepcb has been received.
 *
 * The inp must be wlocked on entry and is unlocked (or maybe destroyed) by the
 * time this function exits.
 */
static int
toepcb_release(struct toepcb *toep)
{
	struct inpcb *inp = toep->tp_inp;
	struct toedev *tod = toep->tp_tod;
	struct tom_data *td = t3_tomdata(tod);
	int rc;

	INP_WLOCK_ASSERT(inp);
	KASSERT(!(toep->tp_flags & TP_CPL_DONE),
	    ("%s: double release?", __func__));

	CTR2(KTR_CXGB, "%s: tid %d", __func__, toep->tp_tid);

	toep->tp_flags |= TP_CPL_DONE;
	toep->tp_inp = NULL;

	mtx_lock(&td->toep_list_lock);
	TAILQ_REMOVE(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);

	if (!(toep->tp_flags & TP_ATTACHED))
		t3_release_offload_resources(toep);

	rc = in_pcbrele_wlocked(inp);
	if (!rc)
		INP_WUNLOCK(inp);
	return (rc);
}

/*
 * One sided detach.  The tcpcb is going away and we need to unhook the toepcb
 * hanging off it.  If the TOE driver is also done with the toepcb we'll release
 * all offload resources.
 */
static void
toepcb_detach(struct inpcb *inp)
{
	struct toepcb *toep;
	struct tcpcb *tp;

	KASSERT(inp, ("%s: inp is NULL", __func__));
	INP_WLOCK_ASSERT(inp);

	tp = intotcpcb(inp);
	toep = tp->t_toe;

	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));
	KASSERT(toep->tp_flags & TP_ATTACHED, ("%s: not attached", __func__));

	CTR6(KTR_CXGB, "%s: %s %u, toep %p, inp %p, tp %p", __func__,
	    tp->t_state == TCPS_SYN_SENT ? "atid" : "tid", toep->tp_tid,
	    toep, inp, tp);

	tp->t_toe = NULL;
	tp->t_flags &= ~TF_TOE;
	toep->tp_flags &= ~TP_ATTACHED;

	if (toep->tp_flags & TP_CPL_DONE)
		t3_release_offload_resources(toep);
}

void
t3_pcb_detach(struct toedev *tod __unused, struct tcpcb *tp)
{

	toepcb_detach(tp->t_inpcb);
}

static int
alloc_atid(struct tid_info *t, void *ctx)
{
	int atid = -1;

	mtx_lock(&t->atid_lock);
	if (t->afree) {
		union active_open_entry *p = t->afree;

		atid = (p - t->atid_tab) + t->atid_base;
		t->afree = p->next;
		p->ctx = ctx;
		t->atids_in_use++;
	}
	mtx_unlock(&t->atid_lock);

	return (atid);
}

static void
free_atid(struct tid_info *t, int atid)
{
	union active_open_entry *p = atid2entry(t, atid);

	mtx_lock(&t->atid_lock);
	p->next = t->afree;
	t->afree = p;
	t->atids_in_use--;
	mtx_unlock(&t->atid_lock);
}

void
insert_tid(struct tom_data *td, void *ctx, unsigned int tid)
{
	struct tid_info *t = &td->tid_maps;

	t->tid_tab[tid] = ctx;
	atomic_add_int(&t->tids_in_use, 1);
}

void
update_tid(struct tom_data *td, void *ctx, unsigned int tid)
{
	struct tid_info *t = &td->tid_maps;

	t->tid_tab[tid] = ctx;
}

void
remove_tid(struct tom_data *td, unsigned int tid)
{
	struct tid_info *t = &td->tid_maps;

	t->tid_tab[tid] = NULL;
	atomic_add_int(&t->tids_in_use, -1);
}

/* use ctx as a next pointer in the tid release list */
void
queue_tid_release(struct toedev *tod, unsigned int tid)
{
	struct tom_data *td = t3_tomdata(tod);
	void **p = &td->tid_maps.tid_tab[tid];
	struct adapter *sc = tod->tod_softc;

	mtx_lock(&td->tid_release_lock);
	*p = td->tid_release_list;
	td->tid_release_list = p;
	if (!*p)
		taskqueue_enqueue(sc->tq, &td->tid_release_task);
	mtx_unlock(&td->tid_release_lock);
}

/*
 * Populate a TID_RELEASE WR.
 */
static inline void
mk_tid_release(struct cpl_tid_release *cpl, unsigned int tid)
{

	cpl->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(cpl) = htonl(MK_OPCODE_TID(CPL_TID_RELEASE, tid));
}

void
release_tid(struct toedev *tod, unsigned int tid, int qset)
{
	struct tom_data *td = t3_tomdata(tod);
	struct adapter *sc = tod->tod_softc;
	struct mbuf *m;
	struct cpl_tid_release *cpl;
#ifdef INVARIANTS
	struct tid_info *t = &td->tid_maps;
#endif

	KASSERT(tid >= 0 && tid < t->ntids,
	    ("%s: tid=%d, ntids=%d", __func__, tid, t->ntids));

	m = M_GETHDR_OFLD(qset, CPL_PRIORITY_CONTROL, cpl);
	if (m) {
		mk_tid_release(cpl, tid);
		t3_offload_tx(sc, m);
		remove_tid(td, tid);
	} else
		queue_tid_release(tod, tid);

}

void
t3_process_tid_release_list(void *data, int pending)
{
	struct mbuf *m;
	struct tom_data *td = data;
	struct adapter *sc = td->tod.tod_softc;

	mtx_lock(&td->tid_release_lock);
	while (td->tid_release_list) {
		void **p = td->tid_release_list;
		unsigned int tid = p - td->tid_maps.tid_tab;
		struct cpl_tid_release *cpl;

		td->tid_release_list = (void **)*p;
		m = M_GETHDR_OFLD(0, CPL_PRIORITY_CONTROL, cpl); /* qs 0 here */
		if (m == NULL)
			break;	/* XXX: who reschedules the release task? */
		mtx_unlock(&td->tid_release_lock);
		mk_tid_release(cpl, tid);
		t3_offload_tx(sc, m);
		remove_tid(td, tid);
		mtx_lock(&td->tid_release_lock);
	}
	mtx_unlock(&td->tid_release_lock);
}

static void
close_conn(struct adapter *sc, struct toepcb *toep)
{
	struct mbuf *m;
	struct cpl_close_con_req *req;

	if (toep->tp_flags & TP_FIN_SENT)
		return;

	m = M_GETHDR_OFLD(toep->tp_qset, CPL_PRIORITY_DATA, req);
	if (m == NULL)
		CXGB_UNIMPLEMENTED();

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_CLOSE_CON));
	req->wr.wrh_lo = htonl(V_WR_TID(toep->tp_tid));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_CON_REQ, toep->tp_tid));
	req->rsvd = 0;

	toep->tp_flags |= TP_FIN_SENT;
	t3_offload_tx(sc, m);
}

static inline void
make_tx_data_wr(struct socket *so, struct tx_data_wr *req, int len,
    struct mbuf *tail)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct sockbuf *snd;

	inp_lock_assert(tp->t_inpcb);
	snd = so_sockbuf_snd(so);

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_TX_DATA));
	req->wr.wrh_lo = htonl(V_WR_TID(toep->tp_tid));
	/* len includes the length of any HW ULP additions */
	req->len = htonl(len);
	req->param = htonl(V_TX_PORT(toep->tp_l2t->smt_idx));
	/* V_TX_ULP_SUBMODE sets both the mode and submode */
	req->flags = htonl(V_TX_ULP_SUBMODE(toep->tp_ulp_mode) | V_TX_URG(0) |
	    V_TX_SHOVE(!(tp->t_flags & TF_MORETOCOME) && (tail ? 0 : 1)));
	req->sndseq = htonl(tp->snd_nxt);
	if (__predict_false((toep->tp_flags & TP_DATASENT) == 0)) {
		struct adapter *sc = toep->tp_tod->tod_softc;
		int cpu_idx = sc->rrss_map[toep->tp_qset];

		req->flags |= htonl(V_TX_ACK_PAGES(2) | F_TX_INIT |
		    V_TX_CPU_IDX(cpu_idx));

		/* Sendbuffer is in units of 32KB. */
		if (V_tcp_do_autosndbuf && snd->sb_flags & SB_AUTOSIZE) 
			req->param |= htonl(V_TX_SNDBUF(VNET(tcp_autosndbuf_max) >> 15));
		else
			req->param |= htonl(V_TX_SNDBUF(snd->sb_hiwat >> 15));

		toep->tp_flags |= TP_DATASENT;
	}
}

/*
 * TOM_XXX_DUPLICATION sgl_len, calc_tx_descs, calc_tx_descs_ofld, mbuf_wrs, etc.
 * TOM_XXX_MOVE to some common header file.
 */
/*
 * IMM_LEN: # of bytes that can be tx'd as immediate data.  There are 16 flits
 * in a tx desc; subtract 3 for tx_data_wr (including the WR header), and 1 more
 * for the second gen bit flit.  This leaves us with 12 flits.
 *
 * descs_to_sgllen: # of SGL entries that can fit into the given # of tx descs.
 * The first desc has a tx_data_wr (which includes the WR header), the rest have
 * the WR header only.  All descs have the second gen bit flit.
 *
 * sgllen_to_descs: # of tx descs used up by an sgl of given length.  The first
 * desc has a tx_data_wr (which includes the WR header), the rest have the WR
 * header only.  All descs have the second gen bit flit.
 *
 * flits_to_sgllen: # of SGL entries that can be fit in the given # of flits.
 *
 */
#define IMM_LEN 96
static int descs_to_sgllen[TX_MAX_DESC + 1] = {0, 8, 17, 26, 35};
static int sgllen_to_descs[TX_MAX_SEGS] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 2,	/*  0 -  9 */
	2, 2, 2, 2, 2, 2, 2, 2, 3, 3,	/* 10 - 19 */
	3, 3, 3, 3, 3, 3, 3, 4, 4, 4,	/* 20 - 29 */
	4, 4, 4, 4, 4, 4		/* 30 - 35 */
};
#if 0
static int flits_to_sgllen[TX_DESC_FLITS + 1] = {
	0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 8, 8, 9, 10, 10
};
#endif
#if SGE_NUM_GENBITS != 2
#error "SGE_NUM_GENBITS really must be 2"
#endif

int
t3_push_frames(struct socket *so, int req_completion)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct mbuf *m0, *sndptr, *m;
	struct toedev *tod = toep->tp_tod;
	struct adapter *sc = tod->tod_softc;
	int bytes, ndesc, total_bytes = 0, mlen;
	struct sockbuf *snd;
	struct sglist *sgl;
	struct ofld_hdr *oh;
	caddr_t dst;
	struct tx_data_wr *wr;

	inp_lock_assert(tp->t_inpcb);

	snd = so_sockbuf_snd(so);
	SOCKBUF_LOCK(snd);

	/*
	 * Autosize the send buffer.
	 */
	if (snd->sb_flags & SB_AUTOSIZE && VNET(tcp_do_autosndbuf)) {
		if (sbused(snd) >= (snd->sb_hiwat / 8 * 7) &&
		    sbused(snd) < VNET(tcp_autosndbuf_max)) {
			if (!sbreserve_locked(snd, min(snd->sb_hiwat +
			    VNET(tcp_autosndbuf_inc), VNET(tcp_autosndbuf_max)),
			    so, curthread))
				snd->sb_flags &= ~SB_AUTOSIZE;
		}
	}

	if (toep->tp_m_last && toep->tp_m_last == snd->sb_sndptr)
		sndptr = toep->tp_m_last->m_next;
	else
		sndptr = snd->sb_sndptr ? snd->sb_sndptr : snd->sb_mb;

	/* Nothing to send or no WRs available for sending data */
	if (toep->tp_wr_avail == 0 || sndptr == NULL)
		goto out;

	/* Something to send and at least 1 WR available */
	while (toep->tp_wr_avail && sndptr != NULL) {

		m0 = m_gethdr(M_NOWAIT, MT_DATA);
		if (m0 == NULL)
			break;
		oh = mtod(m0, struct ofld_hdr *);
		wr = (void *)(oh + 1);
		dst = (void *)(wr + 1);

		m0->m_pkthdr.len = m0->m_len = sizeof(*oh) + sizeof(*wr);
		oh->flags = V_HDR_CTRL(CPL_PRIORITY_DATA) | F_HDR_DF |
		    V_HDR_QSET(toep->tp_qset);

		/*
		 * Try to construct an immediate data WR if possible.  Stuff as
		 * much data into it as possible, one whole mbuf at a time.
		 */
		mlen = sndptr->m_len;
		ndesc = bytes = 0;
		while (mlen <= IMM_LEN - bytes) {
			bcopy(sndptr->m_data, dst, mlen);
			bytes += mlen;
			dst += mlen;

			if (!(sndptr = sndptr->m_next))
				break;
			mlen = sndptr->m_len;
		}

		if (bytes) {

			/* Was able to fit 'bytes' bytes in an immediate WR */

			ndesc = 1;
			make_tx_data_wr(so, wr, bytes, sndptr);

			m0->m_len += bytes;
			m0->m_pkthdr.len = m0->m_len;

		} else {
			int wr_avail = min(toep->tp_wr_avail, TX_MAX_DESC);

			/* Need to make an SGL */

			sgl = sglist_alloc(descs_to_sgllen[wr_avail], M_NOWAIT);
			if (sgl == NULL)
				break;

			for (m = sndptr; m != NULL; m = m->m_next) {
				if ((mlen = m->m_len) > 0) {
					if (sglist_append(sgl, m->m_data, mlen))
					    break;
				}
				bytes += mlen;
			}
			sndptr = m;
			if (bytes == 0) {
				sglist_free(sgl);
				break;
			}
			ndesc = sgllen_to_descs[sgl->sg_nseg];
			oh->flags |= F_HDR_SGL;
			oh->sgl = sgl;
			make_tx_data_wr(so, wr, bytes, sndptr);
		}

		oh->flags |= V_HDR_NDESC(ndesc);
		oh->plen = bytes;

		snd->sb_sndptr = sndptr;
		snd->sb_sndptroff += bytes;
		if (sndptr == NULL) {
			snd->sb_sndptr = snd->sb_mbtail;
			snd->sb_sndptroff -= snd->sb_mbtail->m_len;
			toep->tp_m_last = snd->sb_mbtail;
		} else
			toep->tp_m_last = NULL;

		total_bytes += bytes;

		toep->tp_wr_avail -= ndesc;
		toep->tp_wr_unacked += ndesc;

		if ((req_completion && toep->tp_wr_unacked == ndesc) ||
		    toep->tp_wr_unacked >= toep->tp_wr_max / 2) {
			wr->wr.wrh_hi |= htonl(F_WR_COMPL);
			toep->tp_wr_unacked = 0;	
		}

		enqueue_wr(toep, m0);
		l2t_send(sc, m0, toep->tp_l2t);
	}
out:
	SOCKBUF_UNLOCK(snd);

	if (sndptr == NULL && (toep->tp_flags & TP_SEND_FIN))
		close_conn(sc, toep);

	return (total_bytes);
}

static int
send_rx_credits(struct adapter *sc, struct toepcb *toep, int credits)
{
	struct mbuf *m;
	struct cpl_rx_data_ack *req;
	uint32_t dack = F_RX_DACK_CHANGE | V_RX_DACK_MODE(1);

	m = M_GETHDR_OFLD(toep->tp_qset, CPL_PRIORITY_CONTROL, req);
	if (m == NULL)
		return (0);

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wrh_lo = 0;
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_RX_DATA_ACK, toep->tp_tid));
	req->credit_dack = htonl(dack | V_RX_CREDITS(credits));
	t3_offload_tx(sc, m);
	return (credits);
}

void
t3_rcvd(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	struct sockbuf *so_rcv = &so->so_rcv;
	struct toepcb *toep = tp->t_toe;
	int must_send;

	INP_WLOCK_ASSERT(inp);

	SOCKBUF_LOCK(so_rcv);
	KASSERT(toep->tp_enqueued >= sbused(so_rcv),
	    ("%s: sbused(so_rcv) > enqueued", __func__));
	toep->tp_rx_credits += toep->tp_enqueued - sbused(so_rcv);
	toep->tp_enqueued = sbused(so_rcv);
	SOCKBUF_UNLOCK(so_rcv);

	must_send = toep->tp_rx_credits + 16384 >= tp->rcv_wnd;
	if (must_send || toep->tp_rx_credits >= 15 * 1024) {
		int credits;

		credits = send_rx_credits(sc, toep, toep->tp_rx_credits);
		toep->tp_rx_credits -= credits;
		tp->rcv_wnd += credits;
		tp->rcv_adv += credits;
	}
}

static int
do_rx_urg_notify(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct cpl_rx_urg_notify *hdr = mtod(m, void *);
	unsigned int tid = GET_TID(hdr);
	struct toepcb *toep = lookup_tid(&td->tid_maps, tid);

	log(LOG_ERR, "%s: tid %u inp %p", __func__, tid, toep->tp_inp);

	m_freem(m);
	return (0);
}

int
t3_send_fin(struct toedev *tod, struct tcpcb *tp)
{
	struct toepcb *toep = tp->t_toe;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp_inpcbtosocket(inp);
#if defined(KTR)
	unsigned int tid = toep->tp_tid;
#endif

	INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(inp);

	CTR4(KTR_CXGB, "%s: tid %d, toep %p, flags %x", __func__, tid, toep,
	    toep->tp_flags);

	toep->tp_flags |= TP_SEND_FIN;
	t3_push_frames(so, 1);

	return (0);
}

int
t3_tod_output(struct toedev *tod, struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;

	t3_push_frames(so, 1);
	return (0);
}

/* What mtu_idx to use, given a 4-tuple and/or an MSS cap */
int
find_best_mtu_idx(struct adapter *sc, struct in_conninfo *inc, int pmss)
{
	unsigned short *mtus = &sc->params.mtus[0];
	int i = 0, mss;

	KASSERT(inc != NULL || pmss > 0,
	    ("%s: at least one of inc/pmss must be specified", __func__));

	mss = inc ? tcp_mssopt(inc) : pmss;
	if (pmss > 0 && mss > pmss)
		mss = pmss;

	while (i < NMTUS - 1 && mtus[i + 1] <= mss + 40)
		++i;

	return (i);
}

static inline void
purge_wr_queue(struct toepcb *toep)
{
	struct mbuf *m;
	struct ofld_hdr *oh;

	while ((m = mbufq_dequeue(&toep->wr_list)) != NULL) {
		oh = mtod(m, struct ofld_hdr *);
		if (oh->flags & F_HDR_SGL)
			sglist_free(oh->sgl);
		m_freem(m);
	}
}

/*
 * Release cxgb(4) and T3 resources held by an offload connection (TID, L2T
 * entry, etc.)
 */
static void
t3_release_offload_resources(struct toepcb *toep)
{
	struct toedev *tod = toep->tp_tod;
	struct tom_data *td = t3_tomdata(tod);

	/*
	 * The TOM explicitly detaches its toepcb from the system's inp before
	 * it releases the offload resources.
	 */
	if (toep->tp_inp) {
		panic("%s: inp %p still attached to toepcb %p",
		    __func__, toep->tp_inp, toep);
	}

	if (toep->tp_wr_avail != toep->tp_wr_max)
		purge_wr_queue(toep);

	if (toep->tp_l2t) {
		l2t_release(td->l2t, toep->tp_l2t);
		toep->tp_l2t = NULL;
	}

	if (toep->tp_tid >= 0)
		release_tid(tod, toep->tp_tid, toep->tp_qset);

	toepcb_free(toep);
}

/*
 * Determine the receive window size for a socket.
 */
unsigned long
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


/*
 * Set up the socket for TCP offload.
 */
void
offload_socket(struct socket *so, struct toepcb *toep)
{
	struct toedev *tod = toep->tp_tod;
	struct tom_data *td = t3_tomdata(tod);
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);

	INP_WLOCK_ASSERT(inp);

	/* Update socket */
	SOCKBUF_LOCK(&so->so_snd);
	so_sockbuf_snd(so)->sb_flags |= SB_NOCOALESCE;
	SOCKBUF_UNLOCK(&so->so_snd);
	SOCKBUF_LOCK(&so->so_rcv);
	so_sockbuf_rcv(so)->sb_flags |= SB_NOCOALESCE;
	SOCKBUF_UNLOCK(&so->so_rcv);

	/* Update TCP PCB */
	tp->tod = toep->tp_tod;
	tp->t_toe = toep;
	tp->t_flags |= TF_TOE;

	/* Install an extra hold on inp */
	toep->tp_inp = inp;
	toep->tp_flags |= TP_ATTACHED;
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
	struct toedev *tod = toep->tp_tod;
	struct tom_data *td = t3_tomdata(tod);

	INP_WLOCK_ASSERT(inp);

	so_sockbuf_snd(so)->sb_flags &= ~SB_NOCOALESCE;
	so_sockbuf_rcv(so)->sb_flags &= ~SB_NOCOALESCE;

	tp->tod = NULL;
	tp->t_toe = NULL;
	tp->t_flags &= ~TF_TOE;

	toep->tp_inp = NULL;
	toep->tp_flags &= ~TP_ATTACHED;
	if (in_pcbrele_wlocked(inp))
		panic("%s: inp freed.", __func__);

	mtx_lock(&td->toep_list_lock);
	TAILQ_REMOVE(&td->toep_list, toep, link);
	mtx_unlock(&td->toep_list_lock);
}

/*
 * Socket could be a listening socket, and we may not have a toepcb at all at
 * this time.
 */
uint32_t
calc_opt0h(struct socket *so, int mtu_idx, int rscale, struct l2t_entry *e)
{
	uint32_t opt0h = F_TCAM_BYPASS | V_WND_SCALE(rscale) |
	    V_MSS_IDX(mtu_idx);

	if (so != NULL) {
		struct inpcb *inp = sotoinpcb(so);
		struct tcpcb *tp = intotcpcb(inp);
		int keepalive = always_keepalive ||
		    so_options_get(so) & SO_KEEPALIVE;

		opt0h |= V_NAGLE((tp->t_flags & TF_NODELAY) == 0);
		opt0h |= V_KEEP_ALIVE(keepalive != 0);
	}

	if (e != NULL)
		opt0h |= V_L2T_IDX(e->idx) | V_TX_CHANNEL(e->smt_idx);

	return (htobe32(opt0h));
}

uint32_t
calc_opt0l(struct socket *so, int rcv_bufsize)
{
	uint32_t opt0l = V_ULP_MODE(ULP_MODE_NONE) | V_RCV_BUFSIZ(rcv_bufsize);

	KASSERT(rcv_bufsize <= M_RCV_BUFSIZ,
	    ("%s: rcv_bufsize (%d) is too high", __func__, rcv_bufsize));

	if (so != NULL)		/* optional because noone cares about IP TOS */
		opt0l |= V_TOS(INP_TOS(sotoinpcb(so)));

	return (htobe32(opt0l));
}

/*
 * Convert an ACT_OPEN_RPL status to an errno.
 */
static int
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

/*
 * Return whether a failed active open has allocated a TID
 */
static inline int
act_open_has_tid(int status)
{
	return status != CPL_ERR_TCAM_FULL && status != CPL_ERR_CONN_EXIST &&
	       status != CPL_ERR_ARP_MISS;
}

/*
 * Active open failed.
 */
static int
do_act_open_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct toedev *tod = &td->tod;
	struct cpl_act_open_rpl *rpl = mtod(m, void *);
	unsigned int atid = G_TID(ntohl(rpl->atid));
	struct toepcb *toep = lookup_atid(&td->tid_maps, atid);
	struct inpcb *inp = toep->tp_inp;
	int s = rpl->status, rc;

	CTR3(KTR_CXGB, "%s: atid %u, status %u ", __func__, atid, s);

	free_atid(&td->tid_maps, atid);
	toep->tp_tid = -1;

	if (act_open_has_tid(s))
		queue_tid_release(tod, GET_TID(rpl));

	rc = act_open_rpl_status_to_errno(s);
	if (rc != EAGAIN)
		INP_INFO_RLOCK(&V_tcbinfo);
	INP_WLOCK(inp);
	toe_connect_failed(tod, inp, rc);
	toepcb_release(toep);	/* unlocks inp */
	if (rc != EAGAIN)
		INP_INFO_RUNLOCK(&V_tcbinfo);

	m_freem(m);
	return (0);
}

/*
 * Send an active open request.
 *
 * State of affairs on entry:
 * soisconnecting (so_state |= SS_ISCONNECTING)
 * tcbinfo not locked (this has changed - used to be WLOCKed)
 * inp WLOCKed
 * tp->t_state = TCPS_SYN_SENT
 * rtalloc1, RT_UNLOCK on rt.
 */
int
t3_connect(struct toedev *tod, struct socket *so,
    struct rtentry *rt, struct sockaddr *nam)
{
	struct mbuf *m = NULL;
	struct l2t_entry *e = NULL;
	struct tom_data *td = t3_tomdata(tod);
	struct adapter *sc = tod->tod_softc;
	struct cpl_act_open_req *cpl;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct toepcb *toep;
	int atid = -1, mtu_idx, rscale, cpu_idx, qset;
	struct sockaddr *gw;
	struct ifnet *ifp = rt->rt_ifp;
	struct port_info *pi = ifp->if_softc;	/* XXX wrong for VLAN etc. */

	INP_WLOCK_ASSERT(inp);

	toep = toepcb_alloc(tod);
	if (toep == NULL)
		goto failed;

	atid = alloc_atid(&td->tid_maps, toep);
	if (atid < 0)
		goto failed;

	qset = pi->first_qset + (arc4random() % pi->nqsets);

	m = M_GETHDR_OFLD(qset, CPL_PRIORITY_CONTROL, cpl);
	if (m == NULL)
		goto failed;

	gw = rt->rt_flags & RTF_GATEWAY ? rt->rt_gateway : nam;
	e = t3_l2t_get(pi, ifp, gw);
	if (e == NULL)
		goto failed;

	toep->tp_l2t = e;
	toep->tp_tid = atid;	/* used to double check response */
	toep->tp_qset = qset;

	SOCKBUF_LOCK(&so->so_rcv);
	/* opt0 rcv_bufsiz initially, assumes its normal meaning later */
	toep->tp_rx_credits = min(select_rcv_wnd(so) >> 10, M_RCV_BUFSIZ);
	SOCKBUF_UNLOCK(&so->so_rcv);

	offload_socket(so, toep);

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
	cpu_idx = sc->rrss_map[qset];

	cpl->wr.wrh_hi = htobe32(V_WR_OP(FW_WROPCODE_FORWARD));
	cpl->wr.wrh_lo = 0;
	OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ, atid)); 
	inp_4tuple_get(inp, &cpl->local_ip, &cpl->local_port, &cpl->peer_ip,
	    &cpl->peer_port);
	cpl->opt0h = calc_opt0h(so, mtu_idx, rscale, e);
	cpl->opt0l = calc_opt0l(so, toep->tp_rx_credits);
	cpl->params = 0;
	cpl->opt2 = calc_opt2(cpu_idx);

	CTR5(KTR_CXGB, "%s: atid %u (%s), toep %p, inp %p", __func__,
	    toep->tp_tid, tcpstates[tp->t_state], toep, inp);

	if (l2t_send(sc, m, e) == 0)
		return (0);

	undo_offload_socket(so);

failed:
	CTR5(KTR_CXGB, "%s: FAILED, atid %d, toep %p, l2te %p, mbuf %p",
	    __func__, atid, toep, e, m);

	if (atid >= 0)
		free_atid(&td->tid_maps, atid);

	if (e)
		l2t_release(td->l2t, e);

	if (toep)
		toepcb_free(toep);

	m_freem(m);

	return (ENOMEM);
}

/*
 * Send an ABORT_REQ message.  Cannot fail.  This routine makes sure we do not
 * send multiple ABORT_REQs for the same connection and also that we do not try
 * to send a message after the connection has closed.
 */
static void
send_reset(struct toepcb *toep)
{

	struct cpl_abort_req *req;
	unsigned int tid = toep->tp_tid;
	struct inpcb *inp = toep->tp_inp;
	struct socket *so = inp->inp_socket;
	struct tcpcb *tp = intotcpcb(inp);
	struct toedev *tod = toep->tp_tod;
	struct adapter *sc = tod->tod_softc;
	struct mbuf *m;

	INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(inp);

	CTR4(KTR_CXGB, "%s: tid %d, toep %p (%x)", __func__, tid, toep,
	    toep->tp_flags);

	if (toep->tp_flags & TP_ABORT_SHUTDOWN)
		return;

	toep->tp_flags |= (TP_ABORT_RPL_PENDING | TP_ABORT_SHUTDOWN);

	/* Purge the send queue */
	sbflush(so_sockbuf_snd(so));
	purge_wr_queue(toep);

	m = M_GETHDR_OFLD(toep->tp_qset, CPL_PRIORITY_DATA, req);
	if (m == NULL)
		CXGB_UNIMPLEMENTED();

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_HOST_ABORT_CON_REQ));
	req->wr.wrh_lo = htonl(V_WR_TID(tid));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_ABORT_REQ, tid));
	req->rsvd0 = htonl(tp->snd_nxt);
	req->rsvd1 = !(toep->tp_flags & TP_DATASENT);
	req->cmd = CPL_ABORT_SEND_RST;

	if (tp->t_state == TCPS_SYN_SENT)
		(void )mbufq_enqueue(&toep->out_of_order_queue, m); /* defer */
	else
		l2t_send(sc, m, toep->tp_l2t);
}

int
t3_send_rst(struct toedev *tod __unused, struct tcpcb *tp)
{

	send_reset(tp->t_toe);
	return (0);
}

/*
 * Handler for RX_DATA CPL messages.
 */
static int
do_rx_data(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct cpl_rx_data *hdr = mtod(m, void *);
	unsigned int tid = GET_TID(hdr);
	struct toepcb *toep = lookup_tid(&td->tid_maps, tid);
	struct inpcb *inp = toep->tp_inp;
	struct tcpcb *tp;
	struct socket *so;
	struct sockbuf *so_rcv;	

	/* Advance over CPL */
	m_adj(m, sizeof(*hdr));

	/* XXX: revisit.  This comes from the T4 TOM */
	if (__predict_false(inp == NULL)) {
		/*
		 * do_pass_establish failed and must be attempting to abort the
		 * connection.  Meanwhile, the T4 has sent us data for such a
		 * connection.
		 */
#ifdef notyet
		KASSERT(toepcb_flag(toep, TPF_ABORT_SHUTDOWN),
		    ("%s: inp NULL and tid isn't being aborted", __func__));
#endif
		m_freem(m);
		return (0);
	}

	INP_WLOCK(inp);
	if (inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) {
		CTR4(KTR_CXGB, "%s: tid %u, rx (%d bytes), inp_flags 0x%x",
		    __func__, tid, m->m_pkthdr.len, inp->inp_flags);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}

	if (__predict_false(hdr->dack_mode != toep->tp_delack_mode))
		toep->tp_delack_mode = hdr->dack_mode;

	tp = intotcpcb(inp);

#ifdef INVARIANTS
	if (__predict_false(tp->rcv_nxt != be32toh(hdr->seq))) {
		log(LOG_ERR,
		    "%s: unexpected seq# %x for TID %u, rcv_nxt %x\n",
		    __func__, be32toh(hdr->seq), toep->tp_tid, tp->rcv_nxt);
	}
#endif
	tp->rcv_nxt += m->m_pkthdr.len;
	KASSERT(tp->rcv_wnd >= m->m_pkthdr.len,
	    ("%s: negative window size", __func__));
	tp->rcv_wnd -= m->m_pkthdr.len;
	tp->t_rcvtime = ticks;

	so  = inp->inp_socket;
	so_rcv = &so->so_rcv;
	SOCKBUF_LOCK(so_rcv);

	if (__predict_false(so_rcv->sb_state & SBS_CANTRCVMORE)) {
		CTR3(KTR_CXGB, "%s: tid %u, excess rx (%d bytes)",
		    __func__, tid, m->m_pkthdr.len);
		SOCKBUF_UNLOCK(so_rcv);
		INP_WUNLOCK(inp);

		INP_INFO_RLOCK(&V_tcbinfo);
		INP_WLOCK(inp);
		tp = tcp_drop(tp, ECONNRESET);
		if (tp)
			INP_WUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);

		m_freem(m);
		return (0);
	}

	/* receive buffer autosize */
	if (so_rcv->sb_flags & SB_AUTOSIZE &&
	    V_tcp_do_autorcvbuf &&
	    so_rcv->sb_hiwat < V_tcp_autorcvbuf_max &&
	    (m->m_pkthdr.len > (sbspace(so_rcv) / 8 * 7) || tp->rcv_wnd < 32768)) {
		unsigned int hiwat = so_rcv->sb_hiwat;
		unsigned int newsize = min(hiwat + V_tcp_autorcvbuf_inc,
		    V_tcp_autorcvbuf_max);

		if (!sbreserve_locked(so_rcv, newsize, so, NULL))
			so_rcv->sb_flags &= ~SB_AUTOSIZE;
		else
			toep->tp_rx_credits += newsize - hiwat;
	}

	toep->tp_enqueued += m->m_pkthdr.len;
	sbappendstream_locked(so_rcv, m, 0);
	sorwakeup_locked(so);
	SOCKBUF_UNLOCK_ASSERT(so_rcv);

	INP_WUNLOCK(inp);
	return (0);
}

/*
 * Handler for PEER_CLOSE CPL messages.
 */
static int
do_peer_close(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	const struct cpl_peer_close *hdr = mtod(m, void *);
	unsigned int tid = GET_TID(hdr);
	struct toepcb *toep = lookup_tid(&td->tid_maps, tid);
	struct inpcb *inp = toep->tp_inp;
	struct tcpcb *tp;
	struct socket *so;

	INP_INFO_RLOCK(&V_tcbinfo);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	CTR5(KTR_CXGB, "%s: tid %u (%s), toep_flags 0x%x, inp %p", __func__,
	    tid, tp ? tcpstates[tp->t_state] : "no tp" , toep->tp_flags, inp);

	if (toep->tp_flags & TP_ABORT_RPL_PENDING)
		goto done;

	so = inp_inpcbtosocket(inp);

	socantrcvmore(so);
	tp->rcv_nxt++;

	switch (tp->t_state) {
	case TCPS_SYN_RECEIVED:
		tp->t_starttime = ticks;
		/* FALLTHROUGH */ 
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_CLOSE_WAIT;
		break;
	case TCPS_FIN_WAIT_1:
		tp->t_state = TCPS_CLOSING;
		break;
	case TCPS_FIN_WAIT_2:
		tcp_twstart(tp);
		INP_UNLOCK_ASSERT(inp);	/* safe, we have a ref on the  inp */
		INP_INFO_RUNLOCK(&V_tcbinfo);

		INP_WLOCK(inp);
		toepcb_release(toep);	/* no more CPLs expected */

		m_freem(m);
		return (0);
	default:
		log(LOG_ERR, "%s: TID %u received PEER_CLOSE in bad state %d\n",
		    __func__, toep->tp_tid, tp->t_state);
	}

done:
	INP_WUNLOCK(inp);
	INP_INFO_RUNLOCK(&V_tcbinfo);

	m_freem(m);
	return (0);
}

/*
 * Handler for CLOSE_CON_RPL CPL messages.  peer ACK to our FIN received.
 */
static int
do_close_con_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	const struct cpl_close_con_rpl *rpl = mtod(m, void *);
	unsigned int tid = GET_TID(rpl);
	struct toepcb *toep = lookup_tid(&td->tid_maps, tid);
	struct inpcb *inp = toep->tp_inp;
	struct tcpcb *tp;
	struct socket *so;

	INP_INFO_RLOCK(&V_tcbinfo);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	CTR4(KTR_CXGB, "%s: tid %u (%s), toep_flags 0x%x", __func__, tid,
	    tp ? tcpstates[tp->t_state] : "no tp", toep->tp_flags);

	if ((toep->tp_flags & TP_ABORT_RPL_PENDING))
		goto done;

	so = inp_inpcbtosocket(inp);
	tp->snd_una = ntohl(rpl->snd_nxt) - 1;  /* exclude FIN */

	switch (tp->t_state) {
	case TCPS_CLOSING:
		tcp_twstart(tp);
release:
		INP_UNLOCK_ASSERT(inp);	/* safe, we have a ref on the  inp */
		INP_INFO_RUNLOCK(&V_tcbinfo);

		INP_WLOCK(inp);
		toepcb_release(toep);	/* no more CPLs expected */
	
		m_freem(m);
		return (0);
	case TCPS_LAST_ACK:
		if (tcp_close(tp))
			INP_WUNLOCK(inp);
		goto release;

	case TCPS_FIN_WAIT_1:
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
			soisdisconnected(so);
		tp->t_state = TCPS_FIN_WAIT_2;
		break;
	default:
		log(LOG_ERR,
		    "%s: TID %u received CLOSE_CON_RPL in bad state %d\n",
		    __func__, toep->tp_tid, tp->t_state);
	}

done:
	INP_WUNLOCK(inp);
	INP_INFO_RUNLOCK(&V_tcbinfo);

	m_freem(m);
	return (0);
}

static int
do_smt_write_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct cpl_smt_write_rpl *rpl = mtod(m, void *);

	if (rpl->status != CPL_ERR_NONE) {
		log(LOG_ERR,
		    "Unexpected SMT_WRITE_RPL status %u for entry %u\n",
		    rpl->status, GET_TID(rpl));
	}

	m_freem(m);
	return (0);
}

static int
do_set_tcb_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct cpl_set_tcb_rpl *rpl = mtod(m, void *);

	if (rpl->status != CPL_ERR_NONE) {
		log(LOG_ERR, "Unexpected SET_TCB_RPL status %u for tid %u\n",
		    rpl->status, GET_TID(rpl));
	}

	m_freem(m);
	return (0);
}

/*
 * Handle an ABORT_RPL_RSS CPL message.
 */
static int
do_abort_rpl(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	const struct cpl_abort_rpl_rss *rpl = mtod(m, void *);
	unsigned int tid = GET_TID(rpl);
	struct toepcb *toep = lookup_tid(&td->tid_maps, tid);
	struct inpcb *inp;

	/*
	 * Ignore replies to post-close aborts indicating that the abort was
	 * requested too late.  These connections are terminated when we get
	 * PEER_CLOSE or CLOSE_CON_RPL and by the time the abort_rpl_rss
	 * arrives the TID is either no longer used or it has been recycled.
	 */
	if (rpl->status == CPL_ERR_ABORT_FAILED) {
		m_freem(m);
		return (0);
	}

	if (toep->tp_flags & TP_IS_A_SYNQ_ENTRY)
		return (do_abort_rpl_synqe(qs, r, m));

	CTR4(KTR_CXGB, "%s: tid %d, toep %p, status %d", __func__, tid, toep,
	    rpl->status);

	inp = toep->tp_inp;
	INP_WLOCK(inp);

	if (toep->tp_flags & TP_ABORT_RPL_PENDING) {
		if (!(toep->tp_flags & TP_ABORT_RPL_RCVD)) {
			toep->tp_flags |= TP_ABORT_RPL_RCVD;
			INP_WUNLOCK(inp);
		} else {
			toep->tp_flags &= ~TP_ABORT_RPL_RCVD;
			toep->tp_flags &= TP_ABORT_RPL_PENDING;
			toepcb_release(toep);	/* no more CPLs expected */
		}
	}

	m_freem(m);
	return (0);
}

/*
 * Convert the status code of an ABORT_REQ into a FreeBSD error code.
 */
static int
abort_status_to_errno(struct tcpcb *tp, int abort_reason)
{
	switch (abort_reason) {
	case CPL_ERR_BAD_SYN:
	case CPL_ERR_CONN_RESET:
		return (tp->t_state == TCPS_CLOSE_WAIT ? EPIPE : ECONNRESET);
	case CPL_ERR_XMIT_TIMEDOUT:
	case CPL_ERR_PERSIST_TIMEDOUT:
	case CPL_ERR_FINWAIT2_TIMEDOUT:
	case CPL_ERR_KEEPALIVE_TIMEDOUT:
		return (ETIMEDOUT);
	default:
		return (EIO);
	}
}

/*
 * Returns whether an ABORT_REQ_RSS message is a negative advice.
 */
static inline int
is_neg_adv_abort(unsigned int status)
{
	return status == CPL_ERR_RTX_NEG_ADVICE ||
	    status == CPL_ERR_PERSIST_NEG_ADVICE;
}

void
send_abort_rpl(struct toedev *tod, int tid, int qset)
{
	struct mbuf *reply;
	struct cpl_abort_rpl *rpl;
	struct adapter *sc = tod->tod_softc;

	reply = M_GETHDR_OFLD(qset, CPL_PRIORITY_DATA, rpl);
	if (!reply)
		CXGB_UNIMPLEMENTED();

	rpl->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_HOST_ABORT_CON_RPL));
	rpl->wr.wrh_lo = htonl(V_WR_TID(tid));
	OPCODE_TID(rpl) = htonl(MK_OPCODE_TID(CPL_ABORT_RPL, tid));
	rpl->cmd = CPL_ABORT_NO_RST;

	t3_offload_tx(sc, reply);
}

/*
 * Handle an ABORT_REQ_RSS CPL message.  If we're waiting for an ABORT_RPL we
 * ignore this request except that we need to reply to it.
 */
static int
do_abort_req(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct toedev *tod = &td->tod;
	const struct cpl_abort_req_rss *req = mtod(m, void *);
	unsigned int tid = GET_TID(req);
	struct toepcb *toep = lookup_tid(&td->tid_maps, tid);
	struct inpcb *inp;
	struct tcpcb *tp;
	struct socket *so;
	int qset = toep->tp_qset;

	if (is_neg_adv_abort(req->status)) {
		CTR4(KTR_CXGB, "%s: negative advice %d for tid %u (%x)",
		    __func__, req->status, tid, toep->tp_flags);
		m_freem(m);
		return (0);
	}

	if (toep->tp_flags & TP_IS_A_SYNQ_ENTRY)
		return (do_abort_req_synqe(qs, r, m));

	inp = toep->tp_inp;
	INP_INFO_RLOCK(&V_tcbinfo);	/* for tcp_close */
	INP_WLOCK(inp);

	tp = intotcpcb(inp);
	so = inp->inp_socket;

	CTR6(KTR_CXGB, "%s: tid %u (%s), toep %p (%x), status %d",
	    __func__, tid, tcpstates[tp->t_state], toep, toep->tp_flags,
	    req->status);

	if (!(toep->tp_flags & TP_ABORT_REQ_RCVD)) {
		toep->tp_flags |= TP_ABORT_REQ_RCVD;
		toep->tp_flags |= TP_ABORT_SHUTDOWN;
		INP_WUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);
		m_freem(m);
		return (0);
	}
	toep->tp_flags &= ~TP_ABORT_REQ_RCVD;

	/*
	 * If we'd sent a reset on this toep, we'll ignore this and clean up in
	 * the T3's reply to our reset instead.
	 */
	if (toep->tp_flags & TP_ABORT_RPL_PENDING) {
		toep->tp_flags |= TP_ABORT_RPL_SENT;
		INP_WUNLOCK(inp);
	} else {
		so_error_set(so, abort_status_to_errno(tp, req->status));
		tp = tcp_close(tp);
		if (tp == NULL)
			INP_WLOCK(inp);	/* re-acquire */
		toepcb_release(toep);	/* no more CPLs expected */
	}
	INP_INFO_RUNLOCK(&V_tcbinfo);

	send_abort_rpl(tod, tid, qset);
	m_freem(m);
	return (0);
}

static void
assign_rxopt(struct tcpcb *tp, uint16_t tcpopt)
{
	struct toepcb *toep = tp->t_toe;
	struct adapter *sc = toep->tp_tod->tod_softc;

	tp->t_maxseg = tp->t_maxopd = sc->params.mtus[G_TCPOPT_MSS(tcpopt)] - 40;

	if (G_TCPOPT_TSTAMP(tcpopt)) {
		tp->t_flags |= TF_RCVD_TSTMP;
		tp->t_flags |= TF_REQ_TSTMP;	/* forcibly set */
		tp->ts_recent = 0;		/* XXX */
		tp->ts_recent_age = tcp_ts_getticks();
		tp->t_maxseg -= TCPOLEN_TSTAMP_APPA;
	}

	if (G_TCPOPT_SACK(tcpopt))
		tp->t_flags |= TF_SACK_PERMIT;
	else
		tp->t_flags &= ~TF_SACK_PERMIT;

	if (G_TCPOPT_WSCALE_OK(tcpopt))
		tp->t_flags |= TF_RCVD_SCALE;

	if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
	    (TF_RCVD_SCALE | TF_REQ_SCALE)) {
		tp->rcv_scale = tp->request_r_scale;
		tp->snd_scale = G_TCPOPT_SND_WSCALE(tcpopt);
	}

}

/*
 * The ISS and IRS are from after the exchange of SYNs and are off by 1.
 */
void
make_established(struct socket *so, uint32_t cpl_iss, uint32_t cpl_irs,
    uint16_t cpl_tcpopt)
{
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct toepcb *toep = tp->t_toe;
	long bufsize;
	uint32_t iss = be32toh(cpl_iss) - 1;	/* true ISS */
	uint32_t irs = be32toh(cpl_irs) - 1;	/* true IRS */
	uint16_t tcpopt = be16toh(cpl_tcpopt);

	INP_WLOCK_ASSERT(inp);

	tp->t_state = TCPS_ESTABLISHED;
	tp->t_starttime = ticks;
	TCPSTAT_INC(tcps_connects);

	CTR4(KTR_CXGB, "%s tid %u, toep %p, inp %p", tcpstates[tp->t_state],
	    toep->tp_tid, toep, inp);

	tp->irs = irs;
	tcp_rcvseqinit(tp);
	tp->rcv_wnd = toep->tp_rx_credits << 10;
	tp->rcv_adv += tp->rcv_wnd;
	tp->last_ack_sent = tp->rcv_nxt;

	/*
	 * If we were unable to send all rx credits via opt0, save the remainder
	 * in rx_credits so that they can be handed over with the next credit
	 * update.
	 */
	SOCKBUF_LOCK(&so->so_rcv);
	bufsize = select_rcv_wnd(so);
	SOCKBUF_UNLOCK(&so->so_rcv);
	toep->tp_rx_credits = bufsize - tp->rcv_wnd;

	tp->iss = iss;
	tcp_sendseqinit(tp);
	tp->snd_una = iss + 1;
	tp->snd_nxt = iss + 1;
	tp->snd_max = iss + 1;

	assign_rxopt(tp, tcpopt);
	soisconnected(so);
}

/*
 * Fill in the right TID for CPL messages waiting in the out-of-order queue
 * and send them to the TOE.
 */
static void
fixup_and_send_ofo(struct toepcb *toep)
{
	struct mbuf *m;
	struct toedev *tod = toep->tp_tod;
	struct adapter *sc = tod->tod_softc;
	struct inpcb *inp = toep->tp_inp;
	unsigned int tid = toep->tp_tid;

	inp_lock_assert(inp);

	while ((m = mbufq_dequeue(&toep->out_of_order_queue)) != NULL) {
		struct ofld_hdr *oh = mtod(m, void *);
		/*
		 * A variety of messages can be waiting but the fields we'll
		 * be touching are common to all so any message type will do.
		 */
		struct cpl_close_con_req *p = (void *)(oh + 1);

		p->wr.wrh_lo = htonl(V_WR_TID(tid));
		OPCODE_TID(p) = htonl(MK_OPCODE_TID(p->ot.opcode, tid));
		t3_offload_tx(sc, m);
	}
}

/*
 * Process a CPL_ACT_ESTABLISH message.
 */
static int
do_act_establish(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct cpl_act_establish *req = mtod(m, void *);
	unsigned int tid = GET_TID(req);
	unsigned int atid = G_PASS_OPEN_TID(ntohl(req->tos_tid));
	struct toepcb *toep = lookup_atid(&td->tid_maps, atid);
	struct inpcb *inp = toep->tp_inp;
	struct tcpcb *tp;
	struct socket *so; 

	CTR3(KTR_CXGB, "%s: atid %u, tid %u", __func__, atid, tid);

	free_atid(&td->tid_maps, atid);

	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	KASSERT(toep->tp_qset == qs->idx,
	    ("%s qset mismatch %d %d", __func__, toep->tp_qset, qs->idx));
	KASSERT(toep->tp_tid == atid,
	    ("%s atid mismatch %d %d", __func__, toep->tp_tid, atid));

	toep->tp_tid = tid;
	insert_tid(td, toep, tid);

	if (inp->inp_flags & INP_DROPPED) {
		/* socket closed by the kernel before hw told us it connected */
		send_reset(toep);
		goto done;
	}

	KASSERT(tp->t_state == TCPS_SYN_SENT,
	    ("TID %u expected TCPS_SYN_SENT, found %d.", tid, tp->t_state));

	so = inp->inp_socket;
	make_established(so, req->snd_isn, req->rcv_isn, req->tcp_opt);

	/*
	 * Now that we finally have a TID send any CPL messages that we had to
	 * defer for lack of a TID.
	 */
	if (mbufq_len(&toep->out_of_order_queue))
		fixup_and_send_ofo(toep);

done:
	INP_WUNLOCK(inp);
	m_freem(m);
	return (0);
}

/*
 * Process an acknowledgment of WR completion.  Advance snd_una and send the
 * next batch of work requests from the write queue.
 */
static void
wr_ack(struct toepcb *toep, struct mbuf *m)
{
	struct inpcb *inp = toep->tp_inp;
	struct tcpcb *tp;
	struct cpl_wr_ack *hdr = mtod(m, void *);
	struct socket *so;
	unsigned int credits = ntohs(hdr->credits);
	u32 snd_una = ntohl(hdr->snd_una);
	int bytes = 0;
	struct sockbuf *snd;
	struct mbuf *p;
	struct ofld_hdr *oh;

	inp_wlock(inp);
	tp = intotcpcb(inp);
	so = inp->inp_socket;
	toep->tp_wr_avail += credits;
	if (toep->tp_wr_unacked > toep->tp_wr_max - toep->tp_wr_avail)
		toep->tp_wr_unacked = toep->tp_wr_max - toep->tp_wr_avail;

	while (credits) {
		p = peek_wr(toep);

		if (__predict_false(!p)) {
			CTR5(KTR_CXGB, "%s: %u extra WR_ACK credits, "
			    "tid %u, state %u, wr_avail %u", __func__, credits,
			    toep->tp_tid, tp->t_state, toep->tp_wr_avail);

			log(LOG_ERR, "%u WR_ACK credits for TID %u with "
			    "nothing pending, state %u wr_avail=%u\n",
			    credits, toep->tp_tid, tp->t_state, toep->tp_wr_avail);
			break;
		}

		oh = mtod(p, struct ofld_hdr *);

		KASSERT(credits >= G_HDR_NDESC(oh->flags),
		    ("%s: partial credits?  %d %d", __func__, credits,
		    G_HDR_NDESC(oh->flags)));

		dequeue_wr(toep);
		credits -= G_HDR_NDESC(oh->flags);
		bytes += oh->plen;

		if (oh->flags & F_HDR_SGL)
			sglist_free(oh->sgl);
		m_freem(p);
	}

	if (__predict_false(SEQ_LT(snd_una, tp->snd_una)))
		goto out_free;

	if (tp->snd_una != snd_una) {
		tp->snd_una = snd_una;
		tp->ts_recent_age = tcp_ts_getticks();
		if (tp->snd_una == tp->snd_nxt)
			toep->tp_flags &= ~TP_TX_WAIT_IDLE;
	}

	snd = so_sockbuf_snd(so);
	if (bytes) {
		SOCKBUF_LOCK(snd);
		sbdrop_locked(snd, bytes);
		so_sowwakeup_locked(so);
	}

	if (snd->sb_sndptroff < sbused(snd))
		t3_push_frames(so, 0);

out_free:
	inp_wunlock(tp->t_inpcb);
	m_freem(m);
}

/*
 * Handler for TX_DATA_ACK CPL messages.
 */
static int
do_wr_ack(struct sge_qset *qs, struct rsp_desc *r, struct mbuf *m)
{
	struct adapter *sc = qs->adap;
	struct tom_data *td = sc->tom_softc;
	struct cpl_wr_ack *hdr = mtod(m, void *);
	unsigned int tid = GET_TID(hdr);
	struct toepcb *toep = lookup_tid(&td->tid_maps, tid);

	/* XXX bad race */
	if (toep)
		wr_ack(toep, m);

	return (0);
}

void
t3_init_cpl_io(struct adapter *sc)
{
	t3_register_cpl_handler(sc, CPL_ACT_ESTABLISH, do_act_establish);
	t3_register_cpl_handler(sc, CPL_ACT_OPEN_RPL, do_act_open_rpl);
	t3_register_cpl_handler(sc, CPL_RX_URG_NOTIFY, do_rx_urg_notify);
	t3_register_cpl_handler(sc, CPL_RX_DATA, do_rx_data);
	t3_register_cpl_handler(sc, CPL_TX_DMA_ACK, do_wr_ack);
	t3_register_cpl_handler(sc, CPL_PEER_CLOSE, do_peer_close);
	t3_register_cpl_handler(sc, CPL_ABORT_REQ_RSS, do_abort_req);
	t3_register_cpl_handler(sc, CPL_ABORT_RPL_RSS, do_abort_rpl);
	t3_register_cpl_handler(sc, CPL_CLOSE_CON_RPL, do_close_con_rpl);
	t3_register_cpl_handler(sc, CPL_SMT_WRITE_RPL, do_smt_write_rpl);
	t3_register_cpl_handler(sc, CPL_SET_TCB_RPL, do_set_tcb_rpl);
}
#endif
