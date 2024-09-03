/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012, 2015 Chelsio Communications, Inc.
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_kern_tls.h"
#include "opt_ratelimit.h"

#ifdef TCP_OFFLOAD
#include <sys/param.h>
#include <sys/aio.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sglist.h>
#include <sys/taskqueue.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <dev/iscsi/iscsi_proto.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "common/t4_tcb.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

static void	t4_aiotx_cancel(struct kaiocb *job);
static void	t4_aiotx_queue_toep(struct socket *so, struct toepcb *toep);

void
send_flowc_wr(struct toepcb *toep, struct tcpcb *tp)
{
	struct wrqe *wr;
	struct fw_flowc_wr *flowc;
	unsigned int nparams, flowclen, paramidx;
	struct vi_info *vi = toep->vi;
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	unsigned int pfvf = sc->pf << S_FW_VIID_PFN;
	struct ofld_tx_sdesc *txsd = &toep->txsd[toep->txsd_pidx];

	KASSERT(!(toep->flags & TPF_FLOWC_WR_SENT),
	    ("%s: flowc for tid %u sent already", __func__, toep->tid));

	if (tp != NULL)
		nparams = 8;
	else
		nparams = 6;
	if (toep->params.tc_idx != -1) {
		MPASS(toep->params.tc_idx >= 0 &&
		    toep->params.tc_idx < sc->params.nsched_cls);
		nparams++;
	}

	flowclen = sizeof(*flowc) + nparams * sizeof(struct fw_flowc_mnemval);

	wr = alloc_wrqe(roundup2(flowclen, 16), &toep->ofld_txq->wrq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	flowc = wrtod(wr);
	memset(flowc, 0, wr->wr_len);

	flowc->op_to_nparams = htobe32(V_FW_WR_OP(FW_FLOWC_WR) |
	    V_FW_FLOWC_WR_NPARAMS(nparams));
	flowc->flowid_len16 = htonl(V_FW_WR_LEN16(howmany(flowclen, 16)) |
	    V_FW_WR_FLOWID(toep->tid));

#define FLOWC_PARAM(__m, __v) \
	do { \
		flowc->mnemval[paramidx].mnemonic = FW_FLOWC_MNEM_##__m; \
		flowc->mnemval[paramidx].val = htobe32(__v); \
		paramidx++; \
	} while (0)

	paramidx = 0;

	FLOWC_PARAM(PFNVFN, pfvf);
	FLOWC_PARAM(CH, pi->tx_chan);
	FLOWC_PARAM(PORT, pi->tx_chan);
	FLOWC_PARAM(IQID, toep->ofld_rxq->iq.abs_id);
	FLOWC_PARAM(SNDBUF, toep->params.sndbuf);
	if (tp) {
		FLOWC_PARAM(MSS, toep->params.emss);
		FLOWC_PARAM(SNDNXT, tp->snd_nxt);
		FLOWC_PARAM(RCVNXT, tp->rcv_nxt);
	} else
		FLOWC_PARAM(MSS, 512);
	CTR6(KTR_CXGBE,
	    "%s: tid %u, mss %u, sndbuf %u, snd_nxt 0x%x, rcv_nxt 0x%x",
	    __func__, toep->tid, toep->params.emss, toep->params.sndbuf,
	    tp ? tp->snd_nxt : 0, tp ? tp->rcv_nxt : 0);

	if (toep->params.tc_idx != -1)
		FLOWC_PARAM(SCHEDCLASS, toep->params.tc_idx);
#undef FLOWC_PARAM

	KASSERT(paramidx == nparams, ("nparams mismatch"));

	txsd->tx_credits = howmany(flowclen, 16);
	txsd->plen = 0;
	KASSERT(toep->tx_credits >= txsd->tx_credits && toep->txsd_avail > 0,
	    ("%s: not enough credits (%d)", __func__, toep->tx_credits));
	toep->tx_credits -= txsd->tx_credits;
	if (__predict_false(++toep->txsd_pidx == toep->txsd_total))
		toep->txsd_pidx = 0;
	toep->txsd_avail--;

	toep->flags |= TPF_FLOWC_WR_SENT;
        t4_wrq_tx(sc, wr);
}

#ifdef RATELIMIT
/*
 * Input is Bytes/second (so_max_pacing_rate), chip counts in Kilobits/second.
 */
static int
update_tx_rate_limit(struct adapter *sc, struct toepcb *toep, u_int Bps)
{
	int tc_idx, rc;
	const u_int kbps = (u_int) (uint64_t)Bps * 8ULL / 1000;
	const int port_id = toep->vi->pi->port_id;

	CTR3(KTR_CXGBE, "%s: tid %u, rate %uKbps", __func__, toep->tid, kbps);

	if (kbps == 0) {
		/* unbind */
		tc_idx = -1;
	} else {
		rc = t4_reserve_cl_rl_kbps(sc, port_id, kbps, &tc_idx);
		if (rc != 0)
			return (rc);
		MPASS(tc_idx >= 0 && tc_idx < sc->params.nsched_cls);
	}

	if (toep->params.tc_idx != tc_idx) {
		struct wrqe *wr;
		struct fw_flowc_wr *flowc;
		int nparams = 1, flowclen, flowclen16;
		struct ofld_tx_sdesc *txsd = &toep->txsd[toep->txsd_pidx];

		flowclen = sizeof(*flowc) + nparams * sizeof(struct
		    fw_flowc_mnemval);
		flowclen16 = howmany(flowclen, 16);
		if (toep->tx_credits < flowclen16 || toep->txsd_avail == 0 ||
		    (wr = alloc_wrqe(roundup2(flowclen, 16),
		    &toep->ofld_txq->wrq)) == NULL) {
			if (tc_idx >= 0)
				t4_release_cl_rl(sc, port_id, tc_idx);
			return (ENOMEM);
		}

		flowc = wrtod(wr);
		memset(flowc, 0, wr->wr_len);

		flowc->op_to_nparams = htobe32(V_FW_WR_OP(FW_FLOWC_WR) |
		    V_FW_FLOWC_WR_NPARAMS(nparams));
		flowc->flowid_len16 = htonl(V_FW_WR_LEN16(flowclen16) |
		    V_FW_WR_FLOWID(toep->tid));

		flowc->mnemval[0].mnemonic = FW_FLOWC_MNEM_SCHEDCLASS;
		if (tc_idx == -1)
			flowc->mnemval[0].val = htobe32(0xff);
		else
			flowc->mnemval[0].val = htobe32(tc_idx);

		txsd->tx_credits = flowclen16;
		txsd->plen = 0;
		toep->tx_credits -= txsd->tx_credits;
		if (__predict_false(++toep->txsd_pidx == toep->txsd_total))
			toep->txsd_pidx = 0;
		toep->txsd_avail--;
		t4_wrq_tx(sc, wr);
	}

	if (toep->params.tc_idx >= 0)
		t4_release_cl_rl(sc, port_id, toep->params.tc_idx);
	toep->params.tc_idx = tc_idx;

	return (0);
}
#endif

void
send_reset(struct adapter *sc, struct toepcb *toep, uint32_t snd_nxt)
{
	struct wrqe *wr;
	struct cpl_abort_req *req;
	int tid = toep->tid;
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);	/* don't use if INP_DROPPED */

	INP_WLOCK_ASSERT(inp);

	CTR6(KTR_CXGBE, "%s: tid %d (%s), toep_flags 0x%x, inp_flags 0x%x%s",
	    __func__, toep->tid,
	    inp->inp_flags & INP_DROPPED ? "inp dropped" :
	    tcpstates[tp->t_state],
	    toep->flags, inp->inp_flags,
	    toep->flags & TPF_ABORT_SHUTDOWN ?
	    " (abort already in progress)" : "");

	if (toep->flags & TPF_ABORT_SHUTDOWN)
		return;	/* abort already in progress */

	toep->flags |= TPF_ABORT_SHUTDOWN;

	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %d.", __func__, tid));

	wr = alloc_wrqe(sizeof(*req), &toep->ofld_txq->wrq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_ABORT_REQ, tid);
	if (inp->inp_flags & INP_DROPPED)
		req->rsvd0 = htobe32(snd_nxt);
	else
		req->rsvd0 = htobe32(tp->snd_nxt);
	req->rsvd1 = !(toep->flags & TPF_TX_DATA_SENT);
	req->cmd = CPL_ABORT_SEND_RST;

	/*
	 * XXX: What's the correct way to tell that the inp hasn't been detached
	 * from its socket?  Should I even be flushing the snd buffer here?
	 */
	if ((inp->inp_flags & INP_DROPPED) == 0) {
		struct socket *so = inp->inp_socket;

		if (so != NULL)	/* because I'm not sure.  See comment above */
			sbflush(&so->so_snd);
	}

	t4_l2t_send(sc, wr, toep->l2te);
}

/*
 * Called when a connection is established to translate the TCP options
 * reported by HW to FreeBSD's native format.
 */
static void
assign_rxopt(struct tcpcb *tp, uint16_t opt)
{
	struct toepcb *toep = tp->t_toe;
	struct inpcb *inp = tptoinpcb(tp);
	struct adapter *sc = td_adapter(toep->td);

	INP_LOCK_ASSERT(inp);

	toep->params.mtu_idx = G_TCPOPT_MSS(opt);
	tp->t_maxseg = sc->params.mtus[toep->params.mtu_idx];
	if (inp->inp_inc.inc_flags & INC_ISIPV6)
		tp->t_maxseg -= sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	else
		tp->t_maxseg -= sizeof(struct ip) + sizeof(struct tcphdr);

	toep->params.emss = tp->t_maxseg;
	if (G_TCPOPT_TSTAMP(opt)) {
		toep->params.tstamp = 1;
		toep->params.emss -= TCPOLEN_TSTAMP_APPA;
		tp->t_flags |= TF_RCVD_TSTMP;	/* timestamps ok */
		tp->ts_recent = 0;		/* hmmm */
		tp->ts_recent_age = tcp_ts_getticks();
	} else
		toep->params.tstamp = 0;

	if (G_TCPOPT_SACK(opt)) {
		toep->params.sack = 1;
		tp->t_flags |= TF_SACK_PERMIT;	/* should already be set */
	} else {
		toep->params.sack = 0;
		tp->t_flags &= ~TF_SACK_PERMIT;	/* sack disallowed by peer */
	}

	if (G_TCPOPT_WSCALE_OK(opt))
		tp->t_flags |= TF_RCVD_SCALE;

	/* Doing window scaling? */
	if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
	    (TF_RCVD_SCALE | TF_REQ_SCALE)) {
		tp->rcv_scale = tp->request_r_scale;
		tp->snd_scale = G_TCPOPT_SND_WSCALE(opt);
	} else
		toep->params.wscale = 0;

	CTR6(KTR_CXGBE,
	    "assign_rxopt: tid %d, mtu_idx %u, emss %u, ts %u, sack %u, wscale %u",
	    toep->tid, toep->params.mtu_idx, toep->params.emss,
	    toep->params.tstamp, toep->params.sack, toep->params.wscale);
}

/*
 * Completes some final bits of initialization for just established connections
 * and changes their state to TCPS_ESTABLISHED.
 *
 * The ISNs are from the exchange of SYNs.
 */
void
make_established(struct toepcb *toep, uint32_t iss, uint32_t irs, uint16_t opt)
{
	struct inpcb *inp = toep->inp;
	struct socket *so = inp->inp_socket;
	struct tcpcb *tp = intotcpcb(inp);
	uint16_t tcpopt = be16toh(opt);

	INP_WLOCK_ASSERT(inp);
	KASSERT(tp->t_state == TCPS_SYN_SENT ||
	    tp->t_state == TCPS_SYN_RECEIVED,
	    ("%s: TCP state %s", __func__, tcpstates[tp->t_state]));

	CTR6(KTR_CXGBE, "%s: tid %d, so %p, inp %p, tp %p, toep %p",
	    __func__, toep->tid, so, inp, tp, toep);

	tcp_state_change(tp, TCPS_ESTABLISHED);
	tp->t_starttime = ticks;
	TCPSTAT_INC(tcps_connects);

	tp->irs = irs;
	tcp_rcvseqinit(tp);
	tp->rcv_wnd = (u_int)toep->params.opt0_bufsize << 10;
	tp->rcv_adv += tp->rcv_wnd;
	tp->last_ack_sent = tp->rcv_nxt;

	tp->iss = iss;
	tcp_sendseqinit(tp);
	tp->snd_una = iss + 1;
	tp->snd_nxt = iss + 1;
	tp->snd_max = iss + 1;

	assign_rxopt(tp, tcpopt);
	send_flowc_wr(toep, tp);

	soisconnected(so);
}

int
send_rx_credits(struct adapter *sc, struct toepcb *toep, int credits)
{
	struct wrqe *wr;
	struct cpl_rx_data_ack *req;
	uint32_t dack = F_RX_DACK_CHANGE | V_RX_DACK_MODE(1);

	KASSERT(credits >= 0, ("%s: %d credits", __func__, credits));

	wr = alloc_wrqe(sizeof(*req), toep->ctrlq);
	if (wr == NULL)
		return (0);
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_RX_DATA_ACK, toep->tid);
	req->credit_dack = htobe32(dack | V_RX_CREDITS(credits));

	t4_wrq_tx(sc, wr);
	return (credits);
}

void
t4_rcvd_locked(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = inp->inp_socket;
	struct sockbuf *sb = &so->so_rcv;
	struct toepcb *toep = tp->t_toe;
	int rx_credits;

	INP_WLOCK_ASSERT(inp);
	SOCKBUF_LOCK_ASSERT(sb);

	rx_credits = sbspace(sb) > tp->rcv_wnd ? sbspace(sb) - tp->rcv_wnd : 0;
	if (rx_credits > 0 &&
	    (tp->rcv_wnd <= 32 * 1024 || rx_credits >= 64 * 1024 ||
	    (rx_credits >= 16 * 1024 && tp->rcv_wnd <= 128 * 1024) ||
	    sbused(sb) + tp->rcv_wnd < sb->sb_lowat)) {
		rx_credits = send_rx_credits(sc, toep, rx_credits);
		tp->rcv_wnd += rx_credits;
		tp->rcv_adv += rx_credits;
	}
}

void
t4_rcvd(struct toedev *tod, struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = inp->inp_socket;
	struct sockbuf *sb = &so->so_rcv;

	SOCKBUF_LOCK(sb);
	t4_rcvd_locked(tod, tp);
	SOCKBUF_UNLOCK(sb);
}

/*
 * Close a connection by sending a CPL_CLOSE_CON_REQ message.
 */
int
t4_close_conn(struct adapter *sc, struct toepcb *toep)
{
	struct wrqe *wr;
	struct cpl_close_con_req *req;
	unsigned int tid = toep->tid;

	CTR3(KTR_CXGBE, "%s: tid %u%s", __func__, toep->tid,
	    toep->flags & TPF_FIN_SENT ? ", IGNORED" : "");

	if (toep->flags & TPF_FIN_SENT)
		return (0);

	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %u.", __func__, tid));

	wr = alloc_wrqe(sizeof(*req), &toep->ofld_txq->wrq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

        req->wr.wr_hi = htonl(V_FW_WR_OP(FW_TP_WR) |
	    V_FW_WR_IMMDLEN(sizeof(*req) - sizeof(req->wr)));
	req->wr.wr_mid = htonl(V_FW_WR_LEN16(howmany(sizeof(*req), 16)) |
	    V_FW_WR_FLOWID(tid));
        req->wr.wr_lo = cpu_to_be64(0);
        OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_CON_REQ, tid));
	req->rsvd = 0;

	toep->flags |= TPF_FIN_SENT;
	toep->flags &= ~TPF_SEND_FIN;
	t4_l2t_send(sc, wr, toep->l2te);

	return (0);
}

#define MAX_OFLD_TX_CREDITS (SGE_MAX_WR_LEN / 16)
#define MIN_OFLD_TX_CREDITS (howmany(sizeof(struct fw_ofld_tx_data_wr) + 1, 16))
#define MIN_ISO_TX_CREDITS  (howmany(sizeof(struct cpl_tx_data_iso), 16))
#define MIN_TX_CREDITS(iso)						\
	(MIN_OFLD_TX_CREDITS + ((iso) ? MIN_ISO_TX_CREDITS : 0))

/* Maximum amount of immediate data we could stuff in a WR */
static inline int
max_imm_payload(int tx_credits, int iso)
{
	const int iso_cpl_size = iso ? sizeof(struct cpl_tx_data_iso) : 0;
	const int n = 1;	/* Use no more than one desc for imm. data WR */

	KASSERT(tx_credits >= 0 &&
		tx_credits <= MAX_OFLD_TX_CREDITS,
		("%s: %d credits", __func__, tx_credits));

	if (tx_credits < MIN_TX_CREDITS(iso))
		return (0);

	if (tx_credits >= (n * EQ_ESIZE) / 16)
		return ((n * EQ_ESIZE) - sizeof(struct fw_ofld_tx_data_wr) -
		    iso_cpl_size);
	else
		return (tx_credits * 16 - sizeof(struct fw_ofld_tx_data_wr) -
		    iso_cpl_size);
}

/* Maximum number of SGL entries we could stuff in a WR */
static inline int
max_dsgl_nsegs(int tx_credits, int iso)
{
	int nseg = 1;	/* ulptx_sgl has room for 1, rest ulp_tx_sge_pair */
	int sge_pair_credits = tx_credits - MIN_TX_CREDITS(iso);

	KASSERT(tx_credits >= 0 &&
		tx_credits <= MAX_OFLD_TX_CREDITS,
		("%s: %d credits", __func__, tx_credits));

	if (tx_credits < MIN_TX_CREDITS(iso))
		return (0);

	nseg += 2 * (sge_pair_credits * 16 / 24);
	if ((sge_pair_credits * 16) % 24 == 16)
		nseg++;

	return (nseg);
}

static inline void
write_tx_wr(void *dst, struct toepcb *toep, int fw_wr_opcode,
    unsigned int immdlen, unsigned int plen, uint8_t credits, int shove,
    int ulp_submode)
{
	struct fw_ofld_tx_data_wr *txwr = dst;

	txwr->op_to_immdlen = htobe32(V_WR_OP(fw_wr_opcode) |
	    V_FW_WR_IMMDLEN(immdlen));
	txwr->flowid_len16 = htobe32(V_FW_WR_FLOWID(toep->tid) |
	    V_FW_WR_LEN16(credits));
	txwr->lsodisable_to_flags = htobe32(V_TX_ULP_MODE(ulp_mode(toep)) |
	    V_TX_ULP_SUBMODE(ulp_submode) | V_TX_URG(0) | V_TX_SHOVE(shove));
	txwr->plen = htobe32(plen);

	if (toep->params.tx_align > 0) {
		if (plen < 2 * toep->params.emss)
			txwr->lsodisable_to_flags |=
			    htobe32(F_FW_OFLD_TX_DATA_WR_LSODISABLE);
		else
			txwr->lsodisable_to_flags |=
			    htobe32(F_FW_OFLD_TX_DATA_WR_ALIGNPLD |
				(toep->params.nagle == 0 ? 0 :
				F_FW_OFLD_TX_DATA_WR_ALIGNPLDSHOVE));
	}
}

/*
 * Generate a DSGL from a starting mbuf.  The total number of segments and the
 * maximum segments in any one mbuf are provided.
 */
static void
write_tx_sgl(void *dst, struct mbuf *start, struct mbuf *stop, int nsegs, int n)
{
	struct mbuf *m;
	struct ulptx_sgl *usgl = dst;
	int i, j, rc;
	struct sglist sg;
	struct sglist_seg segs[n];

	KASSERT(nsegs > 0, ("%s: nsegs 0", __func__));

	sglist_init(&sg, n, segs);
	usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
	    V_ULPTX_NSGE(nsegs));

	i = -1;
	for (m = start; m != stop; m = m->m_next) {
		if (m->m_flags & M_EXTPG)
			rc = sglist_append_mbuf_epg(&sg, m,
			    mtod(m, vm_offset_t), m->m_len);
		else
			rc = sglist_append(&sg, mtod(m, void *), m->m_len);
		if (__predict_false(rc != 0))
			panic("%s: sglist_append %d", __func__, rc);

		for (j = 0; j < sg.sg_nseg; i++, j++) {
			if (i < 0) {
				usgl->len0 = htobe32(segs[j].ss_len);
				usgl->addr0 = htobe64(segs[j].ss_paddr);
			} else {
				usgl->sge[i / 2].len[i & 1] =
				    htobe32(segs[j].ss_len);
				usgl->sge[i / 2].addr[i & 1] =
				    htobe64(segs[j].ss_paddr);
			}
#ifdef INVARIANTS
			nsegs--;
#endif
		}
		sglist_reset(&sg);
	}
	if (i & 1)
		usgl->sge[i / 2].len[1] = htobe32(0);
	KASSERT(nsegs == 0, ("%s: nsegs %d, start %p, stop %p",
	    __func__, nsegs, start, stop));
}

/*
 * Max number of SGL entries an offload tx work request can have.  This is 41
 * (1 + 40) for a full 512B work request.
 * fw_ofld_tx_data_wr(16B) + ulptx_sgl(16B, 1) + ulptx_sge_pair(480B, 40)
 */
#define OFLD_SGL_LEN (41)

/*
 * Send data and/or a FIN to the peer.
 *
 * The socket's so_snd buffer consists of a stream of data starting with sb_mb
 * and linked together with m_next.  sb_sndptr, if set, is the last mbuf that
 * was transmitted.
 *
 * drop indicates the number of bytes that should be dropped from the head of
 * the send buffer.  It is an optimization that lets do_fw4_ack avoid creating
 * contention on the send buffer lock (before this change it used to do
 * sowwakeup and then t4_push_frames right after that when recovering from tx
 * stalls).  When drop is set this function MUST drop the bytes and wake up any
 * writers.
 */
void
t4_push_frames(struct adapter *sc, struct toepcb *toep, int drop)
{
	struct mbuf *sndptr, *m, *sb_sndptr;
	struct fw_ofld_tx_data_wr *txwr;
	struct wrqe *wr;
	u_int plen, nsegs, credits, max_imm, max_nsegs, max_nsegs_1mbuf;
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);
	struct socket *so = inp->inp_socket;
	struct sockbuf *sb = &so->so_snd;
	int tx_credits, shove, compl, sowwakeup;
	struct ofld_tx_sdesc *txsd;
	bool nomap_mbuf_seen;

	INP_WLOCK_ASSERT(inp);
	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %u.", __func__, toep->tid));

	KASSERT(ulp_mode(toep) == ULP_MODE_NONE ||
	    ulp_mode(toep) == ULP_MODE_TCPDDP ||
	    ulp_mode(toep) == ULP_MODE_TLS ||
	    ulp_mode(toep) == ULP_MODE_RDMA,
	    ("%s: ulp_mode %u for toep %p", __func__, ulp_mode(toep), toep));

#ifdef VERBOSE_TRACES
	CTR5(KTR_CXGBE, "%s: tid %d toep flags %#x tp flags %#x drop %d",
	    __func__, toep->tid, toep->flags, tp->t_flags, drop);
#endif
	if (__predict_false(toep->flags & TPF_ABORT_SHUTDOWN))
		return;

#ifdef RATELIMIT
	if (__predict_false(inp->inp_flags2 & INP_RATE_LIMIT_CHANGED) &&
	    (update_tx_rate_limit(sc, toep, so->so_max_pacing_rate) == 0)) {
		inp->inp_flags2 &= ~INP_RATE_LIMIT_CHANGED;
	}
#endif

	/*
	 * This function doesn't resume by itself.  Someone else must clear the
	 * flag and call this function.
	 */
	if (__predict_false(toep->flags & TPF_TX_SUSPENDED)) {
		KASSERT(drop == 0,
		    ("%s: drop (%d) != 0 but tx is suspended", __func__, drop));
		return;
	}

	txsd = &toep->txsd[toep->txsd_pidx];
	do {
		tx_credits = min(toep->tx_credits, MAX_OFLD_TX_CREDITS);
		max_imm = max_imm_payload(tx_credits, 0);
		max_nsegs = max_dsgl_nsegs(tx_credits, 0);

		SOCKBUF_LOCK(sb);
		sowwakeup = drop;
		if (drop) {
			sbdrop_locked(sb, drop);
			drop = 0;
		}
		sb_sndptr = sb->sb_sndptr;
		sndptr = sb_sndptr ? sb_sndptr->m_next : sb->sb_mb;
		plen = 0;
		nsegs = 0;
		max_nsegs_1mbuf = 0; /* max # of SGL segments in any one mbuf */
		nomap_mbuf_seen = false;
		for (m = sndptr; m != NULL; m = m->m_next) {
			int n;

			if ((m->m_flags & M_NOTAVAIL) != 0)
				break;
			if (m->m_flags & M_EXTPG) {
#ifdef KERN_TLS
				if (m->m_epg_tls != NULL) {
					toep->flags |= TPF_KTLS;
					if (plen == 0) {
						SOCKBUF_UNLOCK(sb);
						t4_push_ktls(sc, toep, 0);
						return;
					}
					break;
				}
#endif
				n = sglist_count_mbuf_epg(m,
				    mtod(m, vm_offset_t), m->m_len);
			} else
				n = sglist_count(mtod(m, void *), m->m_len);

			nsegs += n;
			plen += m->m_len;

			/* This mbuf sent us _over_ the nsegs limit, back out */
			if (plen > max_imm && nsegs > max_nsegs) {
				nsegs -= n;
				plen -= m->m_len;
				if (plen == 0) {
					/* Too few credits */
					toep->flags |= TPF_TX_SUSPENDED;
					if (sowwakeup) {
						if (!TAILQ_EMPTY(
						    &toep->aiotx_jobq))
							t4_aiotx_queue_toep(so,
							    toep);
						sowwakeup_locked(so);
					} else
						SOCKBUF_UNLOCK(sb);
					SOCKBUF_UNLOCK_ASSERT(sb);
					return;
				}
				break;
			}

			if (m->m_flags & M_EXTPG)
				nomap_mbuf_seen = true;
			if (max_nsegs_1mbuf < n)
				max_nsegs_1mbuf = n;
			sb_sndptr = m;	/* new sb->sb_sndptr if all goes well */

			/* This mbuf put us right at the max_nsegs limit */
			if (plen > max_imm && nsegs == max_nsegs) {
				m = m->m_next;
				break;
			}
		}

		if (sbused(sb) > sb->sb_hiwat * 5 / 8 &&
		    toep->plen_nocompl + plen >= sb->sb_hiwat / 4)
			compl = 1;
		else
			compl = 0;

		if (sb->sb_flags & SB_AUTOSIZE &&
		    V_tcp_do_autosndbuf &&
		    sb->sb_hiwat < V_tcp_autosndbuf_max &&
		    sbused(sb) >= sb->sb_hiwat * 7 / 8) {
			int newsize = min(sb->sb_hiwat + V_tcp_autosndbuf_inc,
			    V_tcp_autosndbuf_max);

			if (!sbreserve_locked(so, SO_SND, newsize, NULL))
				sb->sb_flags &= ~SB_AUTOSIZE;
			else
				sowwakeup = 1;	/* room available */
		}
		if (sowwakeup) {
			if (!TAILQ_EMPTY(&toep->aiotx_jobq))
				t4_aiotx_queue_toep(so, toep);
			sowwakeup_locked(so);
		} else
			SOCKBUF_UNLOCK(sb);
		SOCKBUF_UNLOCK_ASSERT(sb);

		/* nothing to send */
		if (plen == 0) {
			KASSERT(m == NULL || (m->m_flags & M_NOTAVAIL) != 0,
			    ("%s: nothing to send, but m != NULL is ready",
			    __func__));
			break;
		}

		if (__predict_false(toep->flags & TPF_FIN_SENT))
			panic("%s: excess tx.", __func__);

		shove = m == NULL && !(tp->t_flags & TF_MORETOCOME);
		if (plen <= max_imm && !nomap_mbuf_seen) {

			/* Immediate data tx */

			wr = alloc_wrqe(roundup2(sizeof(*txwr) + plen, 16),
					&toep->ofld_txq->wrq);
			if (wr == NULL) {
				/* XXX: how will we recover from this? */
				toep->flags |= TPF_TX_SUSPENDED;
				return;
			}
			txwr = wrtod(wr);
			credits = howmany(wr->wr_len, 16);
			write_tx_wr(txwr, toep, FW_OFLD_TX_DATA_WR, plen, plen,
			    credits, shove, 0);
			m_copydata(sndptr, 0, plen, (void *)(txwr + 1));
			nsegs = 0;
		} else {
			int wr_len;

			/* DSGL tx */

			wr_len = sizeof(*txwr) + sizeof(struct ulptx_sgl) +
			    ((3 * (nsegs - 1)) / 2 + ((nsegs - 1) & 1)) * 8;
			wr = alloc_wrqe(roundup2(wr_len, 16),
			    &toep->ofld_txq->wrq);
			if (wr == NULL) {
				/* XXX: how will we recover from this? */
				toep->flags |= TPF_TX_SUSPENDED;
				return;
			}
			txwr = wrtod(wr);
			credits = howmany(wr_len, 16);
			write_tx_wr(txwr, toep, FW_OFLD_TX_DATA_WR, 0, plen,
			    credits, shove, 0);
			write_tx_sgl(txwr + 1, sndptr, m, nsegs,
			    max_nsegs_1mbuf);
			if (wr_len & 0xf) {
				uint64_t *pad = (uint64_t *)
				    ((uintptr_t)txwr + wr_len);
				*pad = 0;
			}
		}

		KASSERT(toep->tx_credits >= credits,
			("%s: not enough credits", __func__));

		toep->tx_credits -= credits;
		toep->tx_nocompl += credits;
		toep->plen_nocompl += plen;
		if (toep->tx_credits <= toep->tx_total * 3 / 8 &&
		    toep->tx_nocompl >= toep->tx_total / 4)
			compl = 1;

		if (compl || ulp_mode(toep) == ULP_MODE_RDMA) {
			txwr->op_to_immdlen |= htobe32(F_FW_WR_COMPL);
			toep->tx_nocompl = 0;
			toep->plen_nocompl = 0;
		}

		tp->snd_nxt += plen;
		tp->snd_max += plen;

		SOCKBUF_LOCK(sb);
		KASSERT(sb_sndptr, ("%s: sb_sndptr is NULL", __func__));
		sb->sb_sndptr = sb_sndptr;
		SOCKBUF_UNLOCK(sb);

		toep->flags |= TPF_TX_DATA_SENT;
		if (toep->tx_credits < MIN_OFLD_TX_CREDITS)
			toep->flags |= TPF_TX_SUSPENDED;

		KASSERT(toep->txsd_avail > 0, ("%s: no txsd", __func__));
		txsd->plen = plen;
		txsd->tx_credits = credits;
		txsd++;
		if (__predict_false(++toep->txsd_pidx == toep->txsd_total)) {
			toep->txsd_pidx = 0;
			txsd = &toep->txsd[0];
		}
		toep->txsd_avail--;

		t4_l2t_send(sc, wr, toep->l2te);
	} while (m != NULL && (m->m_flags & M_NOTAVAIL) == 0);

	/* Send a FIN if requested, but only if there's no more data to send */
	if (m == NULL && toep->flags & TPF_SEND_FIN)
		t4_close_conn(sc, toep);
}

static inline void
rqdrop_locked(struct mbufq *q, int plen)
{
	struct mbuf *m;

	while (plen > 0) {
		m = mbufq_dequeue(q);

		/* Too many credits. */
		MPASS(m != NULL);
		M_ASSERTPKTHDR(m);

		/* Partial credits. */
		MPASS(plen >= m->m_pkthdr.len);

		plen -= m->m_pkthdr.len;
		m_freem(m);
	}
}

/*
 * Not a bit in the TCB, but is a bit in the ulp_submode field of the
 * CPL_TX_DATA flags field in FW_ISCSI_TX_DATA_WR.
 */
#define	ULP_ISO		G_TX_ULP_SUBMODE(F_FW_ISCSI_TX_DATA_WR_ULPSUBMODE_ISO)

static void
write_tx_data_iso(void *dst, u_int ulp_submode, uint8_t flags, uint16_t mss,
    int len, int npdu)
{
	struct cpl_tx_data_iso *cpl;
	unsigned int burst_size;
	unsigned int last;

	/*
	 * The firmware will set the 'F' bit on the last PDU when
	 * either condition is true:
	 *
	 * - this large PDU is marked as the "last" slice
	 *
	 * - the amount of data payload bytes equals the burst_size
	 *
	 * The strategy used here is to always set the burst_size
	 * artificially high (len includes the size of the template
	 * BHS) and only set the "last" flag if the original PDU had
	 * 'F' set.
	 */
	burst_size = len;
	last = !!(flags & CXGBE_ISO_F);

	cpl = (struct cpl_tx_data_iso *)dst;
	cpl->op_to_scsi = htonl(V_CPL_TX_DATA_ISO_OP(CPL_TX_DATA_ISO) |
	    V_CPL_TX_DATA_ISO_FIRST(1) | V_CPL_TX_DATA_ISO_LAST(last) |
	    V_CPL_TX_DATA_ISO_CPLHDRLEN(0) |
	    V_CPL_TX_DATA_ISO_HDRCRC(!!(ulp_submode & ULP_CRC_HEADER)) |
	    V_CPL_TX_DATA_ISO_PLDCRC(!!(ulp_submode & ULP_CRC_DATA)) |
	    V_CPL_TX_DATA_ISO_IMMEDIATE(0) |
	    V_CPL_TX_DATA_ISO_SCSI(CXGBE_ISO_TYPE(flags)));

	cpl->ahs_len = 0;
	cpl->mpdu = htons(DIV_ROUND_UP(mss, 4));
	cpl->burst_size = htonl(DIV_ROUND_UP(burst_size, 4));
	cpl->len = htonl(len);
	cpl->reserved2_seglen_offset = htonl(0);
	cpl->datasn_offset = htonl(0);
	cpl->buffer_offset = htonl(0);
	cpl->reserved3 = 0;
}

static struct wrqe *
write_iscsi_mbuf_wr(struct toepcb *toep, struct mbuf *sndptr)
{
	struct mbuf *m;
	struct fw_ofld_tx_data_wr *txwr;
	struct cpl_tx_data_iso *cpl_iso;
	void *p;
	struct wrqe *wr;
	u_int plen, nsegs, credits, max_imm, max_nsegs, max_nsegs_1mbuf;
	u_int adjusted_plen, imm_data, ulp_submode;
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);
	int tx_credits, shove, npdu, wr_len;
	uint16_t iso_mss;
	static const u_int ulp_extra_len[] = {0, 4, 4, 8};
	bool iso, nomap_mbuf_seen;

	M_ASSERTPKTHDR(sndptr);

	tx_credits = min(toep->tx_credits, MAX_OFLD_TX_CREDITS);
	if (mbuf_raw_wr(sndptr)) {
		plen = sndptr->m_pkthdr.len;
		KASSERT(plen <= SGE_MAX_WR_LEN,
		    ("raw WR len %u is greater than max WR len", plen));
		if (plen > tx_credits * 16)
			return (NULL);

		wr = alloc_wrqe(roundup2(plen, 16), &toep->ofld_txq->wrq);
		if (__predict_false(wr == NULL))
			return (NULL);

		m_copydata(sndptr, 0, plen, wrtod(wr));
		return (wr);
	}

	iso = mbuf_iscsi_iso(sndptr);
	max_imm = max_imm_payload(tx_credits, iso);
	max_nsegs = max_dsgl_nsegs(tx_credits, iso);
	iso_mss = mbuf_iscsi_iso_mss(sndptr);

	plen = 0;
	nsegs = 0;
	max_nsegs_1mbuf = 0; /* max # of SGL segments in any one mbuf */
	nomap_mbuf_seen = false;
	for (m = sndptr; m != NULL; m = m->m_next) {
		int n;

		if (m->m_flags & M_EXTPG)
			n = sglist_count_mbuf_epg(m, mtod(m, vm_offset_t),
			    m->m_len);
		else
			n = sglist_count(mtod(m, void *), m->m_len);

		nsegs += n;
		plen += m->m_len;

		/*
		 * This mbuf would send us _over_ the nsegs limit.
		 * Suspend tx because the PDU can't be sent out.
		 */
		if ((nomap_mbuf_seen || plen > max_imm) && nsegs > max_nsegs)
			return (NULL);

		if (m->m_flags & M_EXTPG)
			nomap_mbuf_seen = true;
		if (max_nsegs_1mbuf < n)
			max_nsegs_1mbuf = n;
	}

	if (__predict_false(toep->flags & TPF_FIN_SENT))
		panic("%s: excess tx.", __func__);

	/*
	 * We have a PDU to send.  All of it goes out in one WR so 'm'
	 * is NULL.  A PDU's length is always a multiple of 4.
	 */
	MPASS(m == NULL);
	MPASS((plen & 3) == 0);
	MPASS(sndptr->m_pkthdr.len == plen);

	shove = !(tp->t_flags & TF_MORETOCOME);

	/*
	 * plen doesn't include header and data digests, which are
	 * generated and inserted in the right places by the TOE, but
	 * they do occupy TCP sequence space and need to be accounted
	 * for.
	 */
	ulp_submode = mbuf_ulp_submode(sndptr);
	MPASS(ulp_submode < nitems(ulp_extra_len));
	npdu = iso ? howmany(plen - ISCSI_BHS_SIZE, iso_mss) : 1;
	adjusted_plen = plen + ulp_extra_len[ulp_submode] * npdu;
	if (iso)
		adjusted_plen += ISCSI_BHS_SIZE * (npdu - 1);
	wr_len = sizeof(*txwr);
	if (iso)
		wr_len += sizeof(struct cpl_tx_data_iso);
	if (plen <= max_imm && !nomap_mbuf_seen) {
		/* Immediate data tx */
		imm_data = plen;
		wr_len += plen;
		nsegs = 0;
	} else {
		/* DSGL tx */
		imm_data = 0;
		wr_len += sizeof(struct ulptx_sgl) +
		    ((3 * (nsegs - 1)) / 2 + ((nsegs - 1) & 1)) * 8;
	}

	wr = alloc_wrqe(roundup2(wr_len, 16), &toep->ofld_txq->wrq);
	if (wr == NULL) {
		/* XXX: how will we recover from this? */
		return (NULL);
	}
	txwr = wrtod(wr);
	credits = howmany(wr->wr_len, 16);

	if (iso) {
		write_tx_wr(txwr, toep, FW_ISCSI_TX_DATA_WR,
		    imm_data + sizeof(struct cpl_tx_data_iso),
		    adjusted_plen, credits, shove, ulp_submode | ULP_ISO);
		cpl_iso = (struct cpl_tx_data_iso *)(txwr + 1);
		MPASS(plen == sndptr->m_pkthdr.len);
		write_tx_data_iso(cpl_iso, ulp_submode,
		    mbuf_iscsi_iso_flags(sndptr), iso_mss, plen, npdu);
		p = cpl_iso + 1;
	} else {
		write_tx_wr(txwr, toep, FW_OFLD_TX_DATA_WR, imm_data,
		    adjusted_plen, credits, shove, ulp_submode);
		p = txwr + 1;
	}

	if (imm_data != 0) {
		m_copydata(sndptr, 0, plen, p);
	} else {
		write_tx_sgl(p, sndptr, m, nsegs, max_nsegs_1mbuf);
		if (wr_len & 0xf) {
			uint64_t *pad = (uint64_t *)((uintptr_t)txwr + wr_len);
			*pad = 0;
		}
	}

	KASSERT(toep->tx_credits >= credits,
	    ("%s: not enough credits: credits %u "
		"toep->tx_credits %u tx_credits %u nsegs %u "
		"max_nsegs %u iso %d", __func__, credits,
		toep->tx_credits, tx_credits, nsegs, max_nsegs, iso));

	tp->snd_nxt += adjusted_plen;
	tp->snd_max += adjusted_plen;

	counter_u64_add(toep->ofld_txq->tx_iscsi_pdus, npdu);
	counter_u64_add(toep->ofld_txq->tx_iscsi_octets, plen);
	if (iso)
		counter_u64_add(toep->ofld_txq->tx_iscsi_iso_wrs, 1);

	return (wr);
}

void
t4_push_pdus(struct adapter *sc, struct toepcb *toep, int drop)
{
	struct mbuf *sndptr, *m;
	struct fw_wr_hdr *wrhdr;
	struct wrqe *wr;
	u_int plen, credits;
	struct inpcb *inp = toep->inp;
	struct ofld_tx_sdesc *txsd = &toep->txsd[toep->txsd_pidx];
	struct mbufq *pduq = &toep->ulp_pduq;

	INP_WLOCK_ASSERT(inp);
	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %u.", __func__, toep->tid));
	KASSERT(ulp_mode(toep) == ULP_MODE_ISCSI,
	    ("%s: ulp_mode %u for toep %p", __func__, ulp_mode(toep), toep));

	if (__predict_false(toep->flags & TPF_ABORT_SHUTDOWN))
		return;

	/*
	 * This function doesn't resume by itself.  Someone else must clear the
	 * flag and call this function.
	 */
	if (__predict_false(toep->flags & TPF_TX_SUSPENDED)) {
		KASSERT(drop == 0,
		    ("%s: drop (%d) != 0 but tx is suspended", __func__, drop));
		return;
	}

	if (drop) {
		struct socket *so = inp->inp_socket;
		struct sockbuf *sb = &so->so_snd;
		int sbu;

		/*
		 * An unlocked read is ok here as the data should only
		 * transition from a non-zero value to either another
		 * non-zero value or zero.  Once it is zero it should
		 * stay zero.
		 */
		if (__predict_false(sbused(sb)) > 0) {
			SOCKBUF_LOCK(sb);
			sbu = sbused(sb);
			if (sbu > 0) {
				/*
				 * The data transmitted before the
				 * tid's ULP mode changed to ISCSI is
				 * still in so_snd.  Incoming credits
				 * should account for so_snd first.
				 */
				sbdrop_locked(sb, min(sbu, drop));
				drop -= min(sbu, drop);
			}
			sowwakeup_locked(so);	/* unlocks so_snd */
		}
		rqdrop_locked(&toep->ulp_pdu_reclaimq, drop);
	}

	while ((sndptr = mbufq_first(pduq)) != NULL) {
		wr = write_iscsi_mbuf_wr(toep, sndptr);
		if (wr == NULL) {
			toep->flags |= TPF_TX_SUSPENDED;
			return;
		}

		plen = sndptr->m_pkthdr.len;
		credits = howmany(wr->wr_len, 16);
		KASSERT(toep->tx_credits >= credits,
			("%s: not enough credits", __func__));

		m = mbufq_dequeue(pduq);
		MPASS(m == sndptr);
		mbufq_enqueue(&toep->ulp_pdu_reclaimq, m);

		toep->tx_credits -= credits;
		toep->tx_nocompl += credits;
		toep->plen_nocompl += plen;

		/*
		 * Ensure there are enough credits for a full-sized WR
		 * as page pod WRs can be full-sized.
		 */
		if (toep->tx_credits <= SGE_MAX_WR_LEN * 5 / 4 &&
		    toep->tx_nocompl >= toep->tx_total / 4) {
			wrhdr = wrtod(wr);
			wrhdr->hi |= htobe32(F_FW_WR_COMPL);
			toep->tx_nocompl = 0;
			toep->plen_nocompl = 0;
		}

		toep->flags |= TPF_TX_DATA_SENT;
		if (toep->tx_credits < MIN_OFLD_TX_CREDITS)
			toep->flags |= TPF_TX_SUSPENDED;

		KASSERT(toep->txsd_avail > 0, ("%s: no txsd", __func__));
		txsd->plen = plen;
		txsd->tx_credits = credits;
		txsd++;
		if (__predict_false(++toep->txsd_pidx == toep->txsd_total)) {
			toep->txsd_pidx = 0;
			txsd = &toep->txsd[0];
		}
		toep->txsd_avail--;

		t4_l2t_send(sc, wr, toep->l2te);
	}

	/* Send a FIN if requested, but only if there are no more PDUs to send */
	if (mbufq_first(pduq) == NULL && toep->flags & TPF_SEND_FIN)
		t4_close_conn(sc, toep);
}

static inline void
t4_push_data(struct adapter *sc, struct toepcb *toep, int drop)
{

	if (ulp_mode(toep) == ULP_MODE_ISCSI)
		t4_push_pdus(sc, toep, drop);
	else if (toep->flags & TPF_KTLS)
		t4_push_ktls(sc, toep, drop);
	else
		t4_push_frames(sc, toep, drop);
}

int
t4_tod_output(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
#ifdef INVARIANTS
	struct inpcb *inp = tptoinpcb(tp);
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("%s: inp %p dropped.", __func__, inp));
	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));

	t4_push_data(sc, toep, 0);

	return (0);
}

int
t4_send_fin(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
#ifdef INVARIANTS
	struct inpcb *inp = tptoinpcb(tp);
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("%s: inp %p dropped.", __func__, inp));
	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));

	toep->flags |= TPF_SEND_FIN;
	if (tp->t_state >= TCPS_ESTABLISHED)
		t4_push_data(sc, toep, 0);

	return (0);
}

int
t4_send_rst(struct toedev *tod, struct tcpcb *tp)
{
	struct adapter *sc = tod->tod_softc;
#if defined(INVARIANTS)
	struct inpcb *inp = tptoinpcb(tp);
#endif
	struct toepcb *toep = tp->t_toe;

	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("%s: inp %p dropped.", __func__, inp));
	KASSERT(toep != NULL, ("%s: toep is NULL", __func__));

	/* hmmmm */
	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc for tid %u [%s] not sent already",
	    __func__, toep->tid, tcpstates[tp->t_state]));

	send_reset(sc, toep, 0);
	return (0);
}

/*
 * Peer has sent us a FIN.
 */
static int
do_peer_close(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_peer_close *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = NULL;
	struct socket *so;
	struct epoch_tracker et;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_PEER_CLOSE,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));

	if (__predict_false(toep->flags & TPF_SYNQE)) {
		/*
		 * do_pass_establish must have run before do_peer_close and if
		 * this is still a synqe instead of a toepcb then the connection
		 * must be getting aborted.
		 */
		MPASS(toep->flags & TPF_ABORT_SHUTDOWN);
		CTR4(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x)", __func__, tid,
		    toep, toep->flags);
		return (0);
	}

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	CURVNET_SET(toep->vnet);
	NET_EPOCH_ENTER(et);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	CTR6(KTR_CXGBE,
	    "%s: tid %u (%s), toep_flags 0x%x, ddp_flags 0x%x, inp %p",
	    __func__, tid, tp ? tcpstates[tp->t_state] : "no tp", toep->flags,
	    toep->ddp.flags, inp);

	if (toep->flags & TPF_ABORT_SHUTDOWN)
		goto done;

	if (ulp_mode(toep) == ULP_MODE_TCPDDP) {
		DDP_LOCK(toep);
		if (__predict_false(toep->ddp.flags &
		    (DDP_BUF0_ACTIVE | DDP_BUF1_ACTIVE)))
			handle_ddp_close(toep, tp, cpl->rcv_nxt);
		DDP_UNLOCK(toep);
	}
	so = inp->inp_socket;
	socantrcvmore(so);

	if (ulp_mode(toep) == ULP_MODE_RDMA ||
	    (ulp_mode(toep) == ULP_MODE_ISCSI && chip_id(sc) >= CHELSIO_T6)) {
		/*
		 * There might be data received via DDP before the FIN
		 * not reported to the driver.  Just assume the
		 * sequence number in the CPL is correct as the
		 * sequence number of the FIN.
		 */
	} else {
		KASSERT(tp->rcv_nxt + 1 == be32toh(cpl->rcv_nxt),
		    ("%s: rcv_nxt mismatch: %u %u", __func__, tp->rcv_nxt,
		    be32toh(cpl->rcv_nxt)));
	}

	tp->rcv_nxt = be32toh(cpl->rcv_nxt);

	switch (tp->t_state) {
	case TCPS_SYN_RECEIVED:
		tp->t_starttime = ticks;
		/* FALLTHROUGH */

	case TCPS_ESTABLISHED:
		tcp_state_change(tp, TCPS_CLOSE_WAIT);
		break;

	case TCPS_FIN_WAIT_1:
		tcp_state_change(tp, TCPS_CLOSING);
		break;

	case TCPS_FIN_WAIT_2:
		restore_so_proto(so, inp->inp_vflag & INP_IPV6);
		tcp_twstart(tp);
		INP_UNLOCK_ASSERT(inp);	 /* safe, we have a ref on the inp */
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();

		INP_WLOCK(inp);
		final_cpl_received(toep);
		return (0);

	default:
		log(LOG_ERR, "%s: TID %u received CPL_PEER_CLOSE in state %d\n",
		    __func__, tid, tp->t_state);
	}
done:
	INP_WUNLOCK(inp);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (0);
}

/*
 * Peer has ACK'd our FIN.
 */
static int
do_close_con_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_close_con_rpl *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = NULL;
	struct socket *so = NULL;
	struct epoch_tracker et;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_CLOSE_CON_RPL,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	CURVNET_SET(toep->vnet);
	NET_EPOCH_ENTER(et);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	CTR4(KTR_CXGBE, "%s: tid %u (%s), toep_flags 0x%x",
	    __func__, tid, tp ? tcpstates[tp->t_state] : "no tp", toep->flags);

	if (toep->flags & TPF_ABORT_SHUTDOWN)
		goto done;

	so = inp->inp_socket;
	tp->snd_una = be32toh(cpl->snd_nxt) - 1;	/* exclude FIN */

	switch (tp->t_state) {
	case TCPS_CLOSING:	/* see TCPS_FIN_WAIT_2 in do_peer_close too */
		restore_so_proto(so, inp->inp_vflag & INP_IPV6);
		tcp_twstart(tp);
release:
		INP_UNLOCK_ASSERT(inp);	/* safe, we have a ref on the  inp */
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();

		INP_WLOCK(inp);
		final_cpl_received(toep);	/* no more CPLs expected */

		return (0);
	case TCPS_LAST_ACK:
		if (tcp_close(tp))
			INP_WUNLOCK(inp);
		goto release;

	case TCPS_FIN_WAIT_1:
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
			soisdisconnected(so);
		tcp_state_change(tp, TCPS_FIN_WAIT_2);
		break;

	default:
		log(LOG_ERR,
		    "%s: TID %u received CPL_CLOSE_CON_RPL in state %s\n",
		    __func__, tid, tcpstates[tp->t_state]);
	}
done:
	INP_WUNLOCK(inp);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (0);
}

void
send_abort_rpl(struct adapter *sc, struct sge_ofld_txq *ofld_txq, int tid,
    int rst_status)
{
	struct wrqe *wr;
	struct cpl_abort_rpl *cpl;

	wr = alloc_wrqe(sizeof(*cpl), &ofld_txq->wrq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	cpl = wrtod(wr);

	INIT_TP_WR_MIT_CPL(cpl, CPL_ABORT_RPL, tid);
	cpl->cmd = rst_status;

	t4_wrq_tx(sc, wr);
}

static int
abort_status_to_errno(struct tcpcb *tp, unsigned int abort_reason)
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
 * TCP RST from the peer, timeout, or some other such critical error.
 */
static int
do_abort_req(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_req_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct sge_ofld_txq *ofld_txq = toep->ofld_txq;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct epoch_tracker et;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_REQ_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));

	if (toep->flags & TPF_SYNQE)
		return (do_abort_req_synqe(iq, rss, m));

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	if (negative_advice(cpl->status)) {
		CTR4(KTR_CXGBE, "%s: negative advice %d for tid %d (0x%x)",
		    __func__, cpl->status, tid, toep->flags);
		return (0);	/* Ignore negative advice */
	}

	inp = toep->inp;
	CURVNET_SET(toep->vnet);
	NET_EPOCH_ENTER(et);	/* for tcp_close */
	INP_WLOCK(inp);

	tp = intotcpcb(inp);

	CTR6(KTR_CXGBE,
	    "%s: tid %d (%s), toep_flags 0x%x, inp_flags 0x%x, status %d",
	    __func__, tid, tp ? tcpstates[tp->t_state] : "no tp", toep->flags,
	    inp->inp_flags, cpl->status);

	/*
	 * If we'd initiated an abort earlier the reply to it is responsible for
	 * cleaning up resources.  Otherwise we tear everything down right here
	 * right now.  We owe the T4 a CPL_ABORT_RPL no matter what.
	 */
	if (toep->flags & TPF_ABORT_SHUTDOWN) {
		INP_WUNLOCK(inp);
		goto done;
	}
	toep->flags |= TPF_ABORT_SHUTDOWN;

	if ((inp->inp_flags & INP_DROPPED) == 0) {
		struct socket *so = inp->inp_socket;

		if (so != NULL)
			so_error_set(so, abort_status_to_errno(tp,
			    cpl->status));
		tp = tcp_close(tp);
		if (tp == NULL)
			INP_WLOCK(inp);	/* re-acquire */
	}

	final_cpl_received(toep);
done:
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	send_abort_rpl(sc, ofld_txq, tid, CPL_ABORT_NO_RST);
	return (0);
}

/*
 * Reply to the CPL_ABORT_REQ (send_reset)
 */
static int
do_abort_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_abort_rpl_rss *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	KASSERT(opcode == CPL_ABORT_RPL_RSS,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));

	if (toep->flags & TPF_SYNQE)
		return (do_abort_rpl_synqe(iq, rss, m));

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	CTR5(KTR_CXGBE, "%s: tid %u, toep %p, inp %p, status %d",
	    __func__, tid, toep, inp, cpl->status);

	KASSERT(toep->flags & TPF_ABORT_SHUTDOWN,
	    ("%s: wasn't expecting abort reply", __func__));

	INP_WLOCK(inp);
	final_cpl_received(toep);

	return (0);
}

static int
do_rx_data(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_rx_data *cpl = mtod(m, const void *);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp;
	struct socket *so;
	struct sockbuf *sb;
	struct epoch_tracker et;
	int len;
	uint32_t ddp_placed = 0;

	if (__predict_false(toep->flags & TPF_SYNQE)) {
		/*
		 * do_pass_establish must have run before do_rx_data and if this
		 * is still a synqe instead of a toepcb then the connection must
		 * be getting aborted.
		 */
		MPASS(toep->flags & TPF_ABORT_SHUTDOWN);
		CTR4(KTR_CXGBE, "%s: tid %u, synqe %p (0x%x)", __func__, tid,
		    toep, toep->flags);
		m_freem(m);
		return (0);
	}

	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	/* strip off CPL header */
	m_adj(m, sizeof(*cpl));
	len = m->m_pkthdr.len;

	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		CTR4(KTR_CXGBE, "%s: tid %u, rx (%d bytes), inp_flags 0x%x",
		    __func__, tid, len, inp->inp_flags);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}

	tp = intotcpcb(inp);

	if (__predict_false(ulp_mode(toep) == ULP_MODE_TLS &&
	   toep->flags & TPF_TLS_RECEIVE)) {
		/* Received "raw" data on a TLS socket. */
		CTR3(KTR_CXGBE, "%s: tid %u, raw TLS data (%d bytes)",
		    __func__, tid, len);
		do_rx_data_tls(cpl, toep, m);
		return (0);
	}

	if (__predict_false(tp->rcv_nxt != be32toh(cpl->seq)))
		ddp_placed = be32toh(cpl->seq) - tp->rcv_nxt;

	tp->rcv_nxt += len;
	if (tp->rcv_wnd < len) {
		KASSERT(ulp_mode(toep) == ULP_MODE_RDMA,
				("%s: negative window size", __func__));
	}

	tp->rcv_wnd -= len;
	tp->t_rcvtime = ticks;

	if (ulp_mode(toep) == ULP_MODE_TCPDDP)
		DDP_LOCK(toep);
	so = inp_inpcbtosocket(inp);
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);

	if (__predict_false(sb->sb_state & SBS_CANTRCVMORE)) {
		CTR3(KTR_CXGBE, "%s: tid %u, excess rx (%d bytes)",
		    __func__, tid, len);
		m_freem(m);
		SOCKBUF_UNLOCK(sb);
		if (ulp_mode(toep) == ULP_MODE_TCPDDP)
			DDP_UNLOCK(toep);
		INP_WUNLOCK(inp);

		CURVNET_SET(toep->vnet);
		NET_EPOCH_ENTER(et);
		INP_WLOCK(inp);
		tp = tcp_drop(tp, ECONNRESET);
		if (tp)
			INP_WUNLOCK(inp);
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();

		return (0);
	}

	/* receive buffer autosize */
	MPASS(toep->vnet == so->so_vnet);
	CURVNET_SET(toep->vnet);
	if (sb->sb_flags & SB_AUTOSIZE &&
	    V_tcp_do_autorcvbuf &&
	    sb->sb_hiwat < V_tcp_autorcvbuf_max &&
	    len > (sbspace(sb) / 8 * 7)) {
		unsigned int hiwat = sb->sb_hiwat;
		unsigned int newsize = min(hiwat + sc->tt.autorcvbuf_inc,
		    V_tcp_autorcvbuf_max);

		if (!sbreserve_locked(so, SO_RCV, newsize, NULL))
			sb->sb_flags &= ~SB_AUTOSIZE;
	}

	if (ulp_mode(toep) == ULP_MODE_TCPDDP) {
		int changed = !(toep->ddp.flags & DDP_ON) ^ cpl->ddp_off;

		if (toep->ddp.waiting_count != 0 || toep->ddp.active_count != 0)
			CTR3(KTR_CXGBE, "%s: tid %u, non-ddp rx (%d bytes)",
			    __func__, tid, len);

		if (changed) {
			if (toep->ddp.flags & DDP_SC_REQ)
				toep->ddp.flags ^= DDP_ON | DDP_SC_REQ;
			else if (cpl->ddp_off == 1) {
				/* Fell out of DDP mode */
				toep->ddp.flags &= ~DDP_ON;
				CTR1(KTR_CXGBE, "%s: fell out of DDP mode",
				    __func__);

				insert_ddp_data(toep, ddp_placed);
			} else {
				/*
				 * Data was received while still
				 * ULP_MODE_NONE, just fall through.
				 */
			}
		}

		if (toep->ddp.flags & DDP_ON) {
			/*
			 * CPL_RX_DATA with DDP on can only be an indicate.
			 * Start posting queued AIO requests via DDP.  The
			 * payload that arrived in this indicate is appended
			 * to the socket buffer as usual.
			 */
			handle_ddp_indicate(toep);
		}
	}

	sbappendstream_locked(sb, m, 0);
	t4_rcvd_locked(&toep->td->tod, tp);

	if (ulp_mode(toep) == ULP_MODE_TCPDDP &&
	    (toep->ddp.flags & DDP_AIO) != 0 && toep->ddp.waiting_count > 0 &&
	    sbavail(sb) != 0) {
		CTR2(KTR_CXGBE, "%s: tid %u queueing AIO task", __func__,
		    tid);
		ddp_queue_toep(toep);
	}
	if (toep->flags & TPF_TLS_STARTING)
		tls_received_starting_data(sc, toep, sb, len);
	sorwakeup_locked(so);
	SOCKBUF_UNLOCK_ASSERT(sb);
	if (ulp_mode(toep) == ULP_MODE_TCPDDP)
		DDP_UNLOCK(toep);

	INP_WUNLOCK(inp);
	CURVNET_RESTORE();
	return (0);
}

static int
do_fw4_ack(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_fw4_ack *cpl = (const void *)(rss + 1);
	unsigned int tid = G_CPL_FW4_ACK_FLOWID(be32toh(OPCODE_TID(cpl)));
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp;
	struct tcpcb *tp;
	struct socket *so;
	uint8_t credits = cpl->credits;
	struct ofld_tx_sdesc *txsd;
	int plen;
#ifdef INVARIANTS
	unsigned int opcode = G_CPL_FW4_ACK_OPCODE(be32toh(OPCODE_TID(cpl)));
#endif

	/*
	 * Very unusual case: we'd sent a flowc + abort_req for a synq entry and
	 * now this comes back carrying the credits for the flowc.
	 */
	if (__predict_false(toep->flags & TPF_SYNQE)) {
		KASSERT(toep->flags & TPF_ABORT_SHUTDOWN,
		    ("%s: credits for a synq entry %p", __func__, toep));
		return (0);
	}

	inp = toep->inp;

	KASSERT(opcode == CPL_FW4_ACK,
	    ("%s: unexpected opcode 0x%x", __func__, opcode));
	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == tid, ("%s: toep tid mismatch", __func__));

	INP_WLOCK(inp);

	if (__predict_false(toep->flags & TPF_ABORT_SHUTDOWN)) {
		INP_WUNLOCK(inp);
		return (0);
	}

	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("%s: inp_flags 0x%x", __func__, inp->inp_flags));

	tp = intotcpcb(inp);

	if (cpl->flags & CPL_FW4_ACK_FLAGS_SEQVAL) {
		tcp_seq snd_una = be32toh(cpl->snd_una);

#ifdef INVARIANTS
		if (__predict_false(SEQ_LT(snd_una, tp->snd_una))) {
			log(LOG_ERR,
			    "%s: unexpected seq# %x for TID %u, snd_una %x\n",
			    __func__, snd_una, toep->tid, tp->snd_una);
		}
#endif

		if (tp->snd_una != snd_una) {
			tp->snd_una = snd_una;
			tp->ts_recent_age = tcp_ts_getticks();
		}
	}

#ifdef VERBOSE_TRACES
	CTR3(KTR_CXGBE, "%s: tid %d credits %u", __func__, tid, credits);
#endif
	so = inp->inp_socket;
	txsd = &toep->txsd[toep->txsd_cidx];
	plen = 0;
	while (credits) {
		KASSERT(credits >= txsd->tx_credits,
		    ("%s: too many (or partial) credits", __func__));
		credits -= txsd->tx_credits;
		toep->tx_credits += txsd->tx_credits;
		plen += txsd->plen;
		txsd++;
		toep->txsd_avail++;
		KASSERT(toep->txsd_avail <= toep->txsd_total,
		    ("%s: txsd avail > total", __func__));
		if (__predict_false(++toep->txsd_cidx == toep->txsd_total)) {
			txsd = &toep->txsd[0];
			toep->txsd_cidx = 0;
		}
	}

	if (toep->tx_credits == toep->tx_total) {
		toep->tx_nocompl = 0;
		toep->plen_nocompl = 0;
	}

	if (toep->flags & TPF_TX_SUSPENDED &&
	    toep->tx_credits >= toep->tx_total / 4) {
#ifdef VERBOSE_TRACES
		CTR2(KTR_CXGBE, "%s: tid %d calling t4_push_frames", __func__,
		    tid);
#endif
		toep->flags &= ~TPF_TX_SUSPENDED;
		CURVNET_SET(toep->vnet);
		t4_push_data(sc, toep, plen);
		CURVNET_RESTORE();
	} else if (plen > 0) {
		struct sockbuf *sb = &so->so_snd;
		int sbu;

		SOCKBUF_LOCK(sb);
		sbu = sbused(sb);
		if (ulp_mode(toep) == ULP_MODE_ISCSI) {
			if (__predict_false(sbu > 0)) {
				/*
				 * The data transmitted before the
				 * tid's ULP mode changed to ISCSI is
				 * still in so_snd.  Incoming credits
				 * should account for so_snd first.
				 */
				sbdrop_locked(sb, min(sbu, plen));
				plen -= min(sbu, plen);
			}
			sowwakeup_locked(so);	/* unlocks so_snd */
			rqdrop_locked(&toep->ulp_pdu_reclaimq, plen);
		} else {
#ifdef VERBOSE_TRACES
			CTR3(KTR_CXGBE, "%s: tid %d dropped %d bytes", __func__,
			    tid, plen);
#endif
			sbdrop_locked(sb, plen);
			if (!TAILQ_EMPTY(&toep->aiotx_jobq))
				t4_aiotx_queue_toep(so, toep);
			sowwakeup_locked(so);	/* unlocks so_snd */
		}
		SOCKBUF_UNLOCK_ASSERT(sb);
	}

	INP_WUNLOCK(inp);

	return (0);
}

void
t4_set_tcb_field(struct adapter *sc, struct sge_wrq *wrq, struct toepcb *toep,
    uint16_t word, uint64_t mask, uint64_t val, int reply, int cookie)
{
	struct wrqe *wr;
	struct cpl_set_tcb_field *req;
	struct ofld_tx_sdesc *txsd;

	MPASS((cookie & ~M_COOKIE) == 0);
	if (reply) {
		MPASS(cookie != CPL_COOKIE_RESERVED);
	}

	wr = alloc_wrqe(sizeof(*req), wrq);
	if (wr == NULL) {
		/* XXX */
		panic("%s: allocation failure.", __func__);
	}
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_SET_TCB_FIELD, toep->tid);
	req->reply_ctrl = htobe16(V_QUEUENO(toep->ofld_rxq->iq.abs_id));
	if (reply == 0)
		req->reply_ctrl |= htobe16(F_NO_REPLY);
	req->word_cookie = htobe16(V_WORD(word) | V_COOKIE(cookie));
	req->mask = htobe64(mask);
	req->val = htobe64(val);
	if (wrq->eq.type == EQ_OFLD) {
		txsd = &toep->txsd[toep->txsd_pidx];
		txsd->tx_credits = howmany(sizeof(*req), 16);
		txsd->plen = 0;
		KASSERT(toep->tx_credits >= txsd->tx_credits &&
		    toep->txsd_avail > 0,
		    ("%s: not enough credits (%d)", __func__,
		    toep->tx_credits));
		toep->tx_credits -= txsd->tx_credits;
		if (__predict_false(++toep->txsd_pidx == toep->txsd_total))
			toep->txsd_pidx = 0;
		toep->txsd_avail--;
	}

	t4_wrq_tx(sc, wr);
}

void
t4_init_cpl_io_handlers(void)
{

	t4_register_cpl_handler(CPL_PEER_CLOSE, do_peer_close);
	t4_register_cpl_handler(CPL_CLOSE_CON_RPL, do_close_con_rpl);
	t4_register_cpl_handler(CPL_ABORT_REQ_RSS, do_abort_req);
	t4_register_shared_cpl_handler(CPL_ABORT_RPL_RSS, do_abort_rpl,
	    CPL_COOKIE_TOM);
	t4_register_cpl_handler(CPL_RX_DATA, do_rx_data);
	t4_register_shared_cpl_handler(CPL_FW4_ACK, do_fw4_ack, CPL_COOKIE_TOM);
}

void
t4_uninit_cpl_io_handlers(void)
{

	t4_register_cpl_handler(CPL_PEER_CLOSE, NULL);
	t4_register_cpl_handler(CPL_CLOSE_CON_RPL, NULL);
	t4_register_cpl_handler(CPL_ABORT_REQ_RSS, NULL);
	t4_register_shared_cpl_handler(CPL_ABORT_RPL_RSS, NULL, CPL_COOKIE_TOM);
	t4_register_cpl_handler(CPL_RX_DATA, NULL);
	t4_register_shared_cpl_handler(CPL_FW4_ACK, NULL, CPL_COOKIE_TOM);
}

/*
 * Use the 'backend1' field in AIO jobs to hold an error that should
 * be reported when the job is completed, the 'backend3' field to
 * store the amount of data sent by the AIO job so far, and the
 * 'backend4' field to hold a reference count on the job.
 *
 * Each unmapped mbuf holds a reference on the job as does the queue
 * so long as the job is queued.
 */
#define	aio_error	backend1
#define	aio_sent	backend3
#define	aio_refs	backend4

#ifdef VERBOSE_TRACES
static int
jobtotid(struct kaiocb *job)
{
	struct socket *so;
	struct tcpcb *tp;
	struct toepcb *toep;

	so = job->fd_file->f_data;
	tp = sototcpcb(so);
	toep = tp->t_toe;
	return (toep->tid);
}
#endif

static void
aiotx_free_job(struct kaiocb *job)
{
	long status;
	int error;

	if (refcount_release(&job->aio_refs) == 0)
		return;

	error = (intptr_t)job->aio_error;
	status = job->aio_sent;
#ifdef VERBOSE_TRACES
	CTR5(KTR_CXGBE, "%s: tid %d completed %p len %ld, error %d", __func__,
	    jobtotid(job), job, status, error);
#endif
	if (error != 0 && status != 0)
		error = 0;
	if (error == ECANCELED)
		aio_cancel(job);
	else if (error)
		aio_complete(job, -1, error);
	else {
		job->msgsnd = 1;
		aio_complete(job, status, 0);
	}
}

static void
aiotx_free_pgs(struct mbuf *m)
{
	struct kaiocb *job;
	vm_page_t pg;

	M_ASSERTEXTPG(m);
	job = m->m_ext.ext_arg1;
#ifdef VERBOSE_TRACES
	CTR3(KTR_CXGBE, "%s: completed %d bytes for tid %d", __func__,
	    m->m_len, jobtotid(job));
#endif

	for (int i = 0; i < m->m_epg_npgs; i++) {
		pg = PHYS_TO_VM_PAGE(m->m_epg_pa[i]);
		vm_page_unwire(pg, PQ_ACTIVE);
	}

	aiotx_free_job(job);
}

/*
 * Allocate a chain of unmapped mbufs describing the next 'len' bytes
 * of an AIO job.
 */
static struct mbuf *
alloc_aiotx_mbuf(struct kaiocb *job, int len)
{
	struct vmspace *vm;
	vm_page_t pgs[MBUF_PEXT_MAX_PGS];
	struct mbuf *m, *top, *last;
	vm_map_t map;
	vm_offset_t start;
	int i, mlen, npages, pgoff;

	KASSERT(job->aio_sent + len <= job->uaiocb.aio_nbytes,
	    ("%s(%p, %d): request to send beyond end of buffer", __func__,
	    job, len));

	/*
	 * The AIO subsystem will cancel and drain all requests before
	 * permitting a process to exit or exec, so p_vmspace should
	 * be stable here.
	 */
	vm = job->userproc->p_vmspace;
	map = &vm->vm_map;
	start = (uintptr_t)job->uaiocb.aio_buf + job->aio_sent;
	pgoff = start & PAGE_MASK;

	top = NULL;
	last = NULL;
	while (len > 0) {
		mlen = imin(len, MBUF_PEXT_MAX_PGS * PAGE_SIZE - pgoff);
		KASSERT(mlen == len || ((start + mlen) & PAGE_MASK) == 0,
		    ("%s: next start (%#jx + %#x) is not page aligned",
		    __func__, (uintmax_t)start, mlen));

		npages = vm_fault_quick_hold_pages(map, start, mlen,
		    VM_PROT_WRITE, pgs, nitems(pgs));
		if (npages < 0)
			break;

		m = mb_alloc_ext_pgs(M_WAITOK, aiotx_free_pgs);
		m->m_epg_1st_off = pgoff;
		m->m_epg_npgs = npages;
		if (npages == 1) {
			KASSERT(mlen + pgoff <= PAGE_SIZE,
			    ("%s: single page is too large (off %d len %d)",
			    __func__, pgoff, mlen));
			m->m_epg_last_len = mlen;
		} else {
			m->m_epg_last_len = mlen - (PAGE_SIZE - pgoff) -
			    (npages - 2) * PAGE_SIZE;
		}
		for (i = 0; i < npages; i++)
			m->m_epg_pa[i] = VM_PAGE_TO_PHYS(pgs[i]);

		m->m_len = mlen;
		m->m_ext.ext_size = npages * PAGE_SIZE;
		m->m_ext.ext_arg1 = job;
		refcount_acquire(&job->aio_refs);

#ifdef VERBOSE_TRACES
		CTR5(KTR_CXGBE, "%s: tid %d, new mbuf %p for job %p, npages %d",
		    __func__, jobtotid(job), m, job, npages);
#endif

		if (top == NULL)
			top = m;
		else
			last->m_next = m;
		last = m;

		len -= mlen;
		start += mlen;
		pgoff = 0;
	}

	return (top);
}

static void
t4_aiotx_process_job(struct toepcb *toep, struct socket *so, struct kaiocb *job)
{
	struct sockbuf *sb;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct mbuf *m;
	u_int sent;
	int error, len;
	bool moretocome, sendmore;

	sb = &so->so_snd;
	SOCKBUF_UNLOCK(sb);
	m = NULL;

#ifdef MAC
	error = mac_socket_check_send(job->fd_file->f_cred, so);
	if (error != 0)
		goto out;
#endif

	/* Inline sosend_generic(). */

	error = SOCK_IO_SEND_LOCK(so, SBL_WAIT);
	MPASS(error == 0);

sendanother:
	SOCKBUF_LOCK(sb);
	if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		SOCKBUF_UNLOCK(sb);
		SOCK_IO_SEND_UNLOCK(so);
		if ((so->so_options & SO_NOSIGPIPE) == 0) {
			PROC_LOCK(job->userproc);
			kern_psignal(job->userproc, SIGPIPE);
			PROC_UNLOCK(job->userproc);
		}
		error = EPIPE;
		goto out;
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		SOCKBUF_UNLOCK(sb);
		SOCK_IO_SEND_UNLOCK(so);
		goto out;
	}
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		SOCKBUF_UNLOCK(sb);
		SOCK_IO_SEND_UNLOCK(so);
		error = ENOTCONN;
		goto out;
	}
	if (sbspace(sb) < sb->sb_lowat) {
		MPASS(job->aio_sent == 0 || !(so->so_state & SS_NBIO));

		/*
		 * Don't block if there is too little room in the socket
		 * buffer.  Instead, requeue the request.
		 */
		if (!aio_set_cancel_function(job, t4_aiotx_cancel)) {
			SOCKBUF_UNLOCK(sb);
			SOCK_IO_SEND_UNLOCK(so);
			error = ECANCELED;
			goto out;
		}
		TAILQ_INSERT_HEAD(&toep->aiotx_jobq, job, list);
		SOCKBUF_UNLOCK(sb);
		SOCK_IO_SEND_UNLOCK(so);
		goto out;
	}

	/*
	 * Write as much data as the socket permits, but no more than a
	 * a single sndbuf at a time.
	 */
	len = sbspace(sb);
	if (len > job->uaiocb.aio_nbytes - job->aio_sent) {
		len = job->uaiocb.aio_nbytes - job->aio_sent;
		moretocome = false;
	} else
		moretocome = true;
	if (len > toep->params.sndbuf) {
		len = toep->params.sndbuf;
		sendmore = true;
	} else
		sendmore = false;

	if (!TAILQ_EMPTY(&toep->aiotx_jobq))
		moretocome = true;
	SOCKBUF_UNLOCK(sb);
	MPASS(len != 0);

	m = alloc_aiotx_mbuf(job, len);
	if (m == NULL) {
		SOCK_IO_SEND_UNLOCK(so);
		error = EFAULT;
		goto out;
	}

	/* Inlined tcp_usr_send(). */

	inp = toep->inp;
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		SOCK_IO_SEND_UNLOCK(so);
		error = ECONNRESET;
		goto out;
	}

	sent = m_length(m, NULL);
	job->aio_sent += sent;
	counter_u64_add(toep->ofld_txq->tx_aio_octets, sent);

	sbappendstream(sb, m, 0);
	m = NULL;

	if (!(inp->inp_flags & INP_DROPPED)) {
		tp = intotcpcb(inp);
		if (moretocome)
			tp->t_flags |= TF_MORETOCOME;
		error = tcp_output(tp);
		if (error < 0) {
			INP_UNLOCK_ASSERT(inp);
			SOCK_IO_SEND_UNLOCK(so);
			error = -error;
			goto out;
		}
		if (moretocome)
			tp->t_flags &= ~TF_MORETOCOME;
	}

	INP_WUNLOCK(inp);
	if (sendmore)
		goto sendanother;
	SOCK_IO_SEND_UNLOCK(so);

	if (error)
		goto out;

	/*
	 * If this is a blocking socket and the request has not been
	 * fully completed, requeue it until the socket is ready
	 * again.
	 */
	if (job->aio_sent < job->uaiocb.aio_nbytes &&
	    !(so->so_state & SS_NBIO)) {
		SOCKBUF_LOCK(sb);
		if (!aio_set_cancel_function(job, t4_aiotx_cancel)) {
			SOCKBUF_UNLOCK(sb);
			error = ECANCELED;
			goto out;
		}
		TAILQ_INSERT_HEAD(&toep->aiotx_jobq, job, list);
		return;
	}

	/*
	 * If the request will not be requeued, drop the queue's
	 * reference to the job.  Any mbufs in flight should still
	 * hold a reference, but this drops the reference that the
	 * queue owns while it is waiting to queue mbufs to the
	 * socket.
	 */
	aiotx_free_job(job);
	counter_u64_add(toep->ofld_txq->tx_aio_jobs, 1);

out:
	if (error) {
		job->aio_error = (void *)(intptr_t)error;
		aiotx_free_job(job);
	}
	m_freem(m);
	SOCKBUF_LOCK(sb);
}

static void
t4_aiotx_task(void *context, int pending)
{
	struct toepcb *toep = context;
	struct socket *so;
	struct kaiocb *job;
	struct epoch_tracker et;

	so = toep->aiotx_so;
	CURVNET_SET(toep->vnet);
	NET_EPOCH_ENTER(et);
	SOCKBUF_LOCK(&so->so_snd);
	while (!TAILQ_EMPTY(&toep->aiotx_jobq) && sowriteable(so)) {
		job = TAILQ_FIRST(&toep->aiotx_jobq);
		TAILQ_REMOVE(&toep->aiotx_jobq, job, list);
		if (!aio_clear_cancel_function(job))
			continue;

		t4_aiotx_process_job(toep, so, job);
	}
	toep->aiotx_so = NULL;
	SOCKBUF_UNLOCK(&so->so_snd);
	NET_EPOCH_EXIT(et);

	free_toepcb(toep);
	sorele(so);
	CURVNET_RESTORE();
}

static void
t4_aiotx_queue_toep(struct socket *so, struct toepcb *toep)
{

	SOCKBUF_LOCK_ASSERT(&toep->inp->inp_socket->so_snd);
#ifdef VERBOSE_TRACES
	CTR3(KTR_CXGBE, "%s: queueing aiotx task for tid %d, active = %s",
	    __func__, toep->tid, toep->aiotx_so != NULL ? "true" : "false");
#endif
	if (toep->aiotx_so != NULL)
		return;
	soref(so);
	toep->aiotx_so = so;
	hold_toepcb(toep);
	soaio_enqueue(&toep->aiotx_task);
}

static void
t4_aiotx_cancel(struct kaiocb *job)
{
	struct socket *so;
	struct sockbuf *sb;
	struct tcpcb *tp;
	struct toepcb *toep;

	so = job->fd_file->f_data;
	tp = sototcpcb(so);
	toep = tp->t_toe;
	MPASS(job->uaiocb.aio_lio_opcode == LIO_WRITE);
	sb = &so->so_snd;

	SOCKBUF_LOCK(sb);
	if (!aio_cancel_cleared(job))
		TAILQ_REMOVE(&toep->aiotx_jobq, job, list);
	SOCKBUF_UNLOCK(sb);

	job->aio_error = (void *)(intptr_t)ECANCELED;
	aiotx_free_job(job);
}

int
t4_aio_queue_aiotx(struct socket *so, struct kaiocb *job)
{
	struct tcpcb *tp = sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct adapter *sc = td_adapter(toep->td);

	/* This only handles writes. */
	if (job->uaiocb.aio_lio_opcode != LIO_WRITE)
		return (EOPNOTSUPP);

	if (!sc->tt.tx_zcopy)
		return (EOPNOTSUPP);

	if (tls_tx_key(toep))
		return (EOPNOTSUPP);

	SOCKBUF_LOCK(&so->so_snd);
#ifdef VERBOSE_TRACES
	CTR3(KTR_CXGBE, "%s: queueing %p for tid %u", __func__, job, toep->tid);
#endif
	if (!aio_set_cancel_function(job, t4_aiotx_cancel))
		panic("new job was cancelled");
	refcount_init(&job->aio_refs, 1);
	TAILQ_INSERT_TAIL(&toep->aiotx_jobq, job, list);
	if (sowriteable(so))
		t4_aiotx_queue_toep(so, toep);
	SOCKBUF_UNLOCK(&so->so_snd);
	return (0);
}

void
aiotx_init_toep(struct toepcb *toep)
{

	TAILQ_INIT(&toep->aiotx_jobq);
	TASK_INIT(&toep->aiotx_task, 0, t4_aiotx_task, toep);
}
#endif
