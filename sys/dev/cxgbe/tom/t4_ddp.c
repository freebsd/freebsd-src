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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/tcp_var.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/toecore.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>

#ifdef TCP_OFFLOAD
#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "common/t4_tcb.h"
#include "tom/t4_tom.h"

VNET_DECLARE(int, tcp_do_autorcvbuf);
#define V_tcp_do_autorcvbuf VNET(tcp_do_autorcvbuf)
VNET_DECLARE(int, tcp_autorcvbuf_inc);
#define V_tcp_autorcvbuf_inc VNET(tcp_autorcvbuf_inc)
VNET_DECLARE(int, tcp_autorcvbuf_max);
#define V_tcp_autorcvbuf_max VNET(tcp_autorcvbuf_max)

static struct mbuf *get_ddp_mbuf(int len);

#define MAX_DDP_BUFFER_SIZE		(M_TCB_RX_DDP_BUF0_LEN)

static struct ddp_buffer *
alloc_ddp_buffer(vm_page_t *pages, int npages, int offset, int len)
{
	struct ddp_buffer *db;

	db = malloc(sizeof(*db), M_CXGBE, M_NOWAIT | M_ZERO);
	if (db == NULL) {
		CTR1(KTR_CXGBE, "%s: malloc failed.", __func__);
		return (NULL);
	}

	db->npages = npages;
	db->pages = pages;
	db->offset = offset;
	db->len = len;

	return (db);
}

static void
free_ddp_buffer(struct ddp_buffer *db)
{

	if (db == NULL)
		return;

	if (db->pages)
		free(db->pages, M_CXGBE);

	if (db->prsv.prsv_nppods > 0)
		t4_free_page_pods(&db->prsv);

	free(db, M_CXGBE);
}

void
release_ddp_resources(struct toepcb *toep)
{
	int i;

	for (i = 0; i < nitems(toep->db); i++) {
		if (toep->db[i] != NULL) {
			free_ddp_buffer(toep->db[i]);
			toep->db[i] = NULL;
		}
	}
}

/* XXX: handle_ddp_data code duplication */
void
insert_ddp_data(struct toepcb *toep, uint32_t n)
{
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);
	struct sockbuf *sb = &inp->inp_socket->so_rcv;
	struct mbuf *m;

	INP_WLOCK_ASSERT(inp);
	SOCKBUF_LOCK_ASSERT(sb);

	m = get_ddp_mbuf(n);
	tp->rcv_nxt += n;
#ifndef USE_DDP_RX_FLOW_CONTROL
	KASSERT(tp->rcv_wnd >= n, ("%s: negative window size", __func__));
	tp->rcv_wnd -= n;
#endif

	KASSERT(toep->sb_cc >= sb->sb_cc,
	    ("%s: sb %p has more data (%d) than last time (%d).",
	    __func__, sb, sb->sb_cc, toep->sb_cc));
	toep->rx_credits += toep->sb_cc - sb->sb_cc;
#ifdef USE_DDP_RX_FLOW_CONTROL
	toep->rx_credits -= n;	/* adjust for F_RX_FC_DDP */
#endif
	sbappendstream_locked(sb, m);
	toep->sb_cc = sb->sb_cc;
}

/* SET_TCB_FIELD sent as a ULP command looks like this */
#define LEN__SET_TCB_FIELD_ULP (sizeof(struct ulp_txpkt) + \
    sizeof(struct ulptx_idata) + sizeof(struct cpl_set_tcb_field_core))

/* RX_DATA_ACK sent as a ULP command looks like this */
#define LEN__RX_DATA_ACK_ULP (sizeof(struct ulp_txpkt) + \
    sizeof(struct ulptx_idata) + sizeof(struct cpl_rx_data_ack_core))

static inline void *
mk_set_tcb_field_ulp(struct ulp_txpkt *ulpmc, struct toepcb *toep,
    uint64_t word, uint64_t mask, uint64_t val)
{
	struct ulptx_idata *ulpsc;
	struct cpl_set_tcb_field_core *req;

	ulpmc->cmd_dest = htonl(V_ULPTX_CMD(ULP_TX_PKT) | V_ULP_TXPKT_DEST(0));
	ulpmc->len = htobe32(howmany(LEN__SET_TCB_FIELD_ULP, 16));

	ulpsc = (struct ulptx_idata *)(ulpmc + 1);
	ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	ulpsc->len = htobe32(sizeof(*req));

	req = (struct cpl_set_tcb_field_core *)(ulpsc + 1);
	OPCODE_TID(req) = htobe32(MK_OPCODE_TID(CPL_SET_TCB_FIELD, toep->tid));
	req->reply_ctrl = htobe16(V_NO_REPLY(1) |
	    V_QUEUENO(toep->ofld_rxq->iq.abs_id));
	req->word_cookie = htobe16(V_WORD(word) | V_COOKIE(0));
        req->mask = htobe64(mask);
        req->val = htobe64(val);

	ulpsc = (struct ulptx_idata *)(req + 1);
	if (LEN__SET_TCB_FIELD_ULP % 16) {
		ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
		ulpsc->len = htobe32(0);
		return (ulpsc + 1);
	}
	return (ulpsc);
}

static inline void *
mk_rx_data_ack_ulp(struct ulp_txpkt *ulpmc, struct toepcb *toep)
{
	struct ulptx_idata *ulpsc;
	struct cpl_rx_data_ack_core *req;

	ulpmc->cmd_dest = htonl(V_ULPTX_CMD(ULP_TX_PKT) | V_ULP_TXPKT_DEST(0));
	ulpmc->len = htobe32(howmany(LEN__RX_DATA_ACK_ULP, 16));

	ulpsc = (struct ulptx_idata *)(ulpmc + 1);
	ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	ulpsc->len = htobe32(sizeof(*req));

	req = (struct cpl_rx_data_ack_core *)(ulpsc + 1);
	OPCODE_TID(req) = htobe32(MK_OPCODE_TID(CPL_RX_DATA_ACK, toep->tid));
	req->credit_dack = htobe32(F_RX_MODULATE_RX);

	ulpsc = (struct ulptx_idata *)(req + 1);
	if (LEN__RX_DATA_ACK_ULP % 16) {
		ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
		ulpsc->len = htobe32(0);
		return (ulpsc + 1);
	}
	return (ulpsc);
}

static inline uint64_t
select_ddp_flags(struct socket *so, int flags, int db_idx)
{
	uint64_t ddp_flags = V_TF_DDP_INDICATE_OUT(0);
	int waitall = flags & MSG_WAITALL;
	int nb = so->so_state & SS_NBIO || flags & (MSG_DONTWAIT | MSG_NBIO);

	KASSERT(db_idx == 0 || db_idx == 1,
	    ("%s: bad DDP buffer index %d", __func__, db_idx));

	if (db_idx == 0) {
		ddp_flags |= V_TF_DDP_BUF0_VALID(1) | V_TF_DDP_ACTIVE_BUF(0);
		if (waitall)
			ddp_flags |= V_TF_DDP_PUSH_DISABLE_0(1);
		else if (nb)
			ddp_flags |= V_TF_DDP_BUF0_FLUSH(1);
		else
			ddp_flags |= V_TF_DDP_BUF0_FLUSH(0);
	} else {
		ddp_flags |= V_TF_DDP_BUF1_VALID(1) | V_TF_DDP_ACTIVE_BUF(1);
		if (waitall)
			ddp_flags |= V_TF_DDP_PUSH_DISABLE_1(1);
		else if (nb)
			ddp_flags |= V_TF_DDP_BUF1_FLUSH(1);
		else
			ddp_flags |= V_TF_DDP_BUF1_FLUSH(0);
	}

	return (ddp_flags);
}

static struct wrqe *
mk_update_tcb_for_ddp(struct adapter *sc, struct toepcb *toep, int db_idx,
    int offset, uint64_t ddp_flags)
{
	struct ddp_buffer *db = toep->db[db_idx];
	struct wrqe *wr;
	struct work_request_hdr *wrh;
	struct ulp_txpkt *ulpmc;
	int len;

	KASSERT(db_idx == 0 || db_idx == 1,
	    ("%s: bad DDP buffer index %d", __func__, db_idx));

	/*
	 * We'll send a compound work request that has 3 SET_TCB_FIELDs and an
	 * RX_DATA_ACK (with RX_MODULATE to speed up delivery).
	 *
	 * The work request header is 16B and always ends at a 16B boundary.
	 * The ULPTX master commands that follow must all end at 16B boundaries
	 * too so we round up the size to 16.
	 */
	len = sizeof(*wrh) + 3 * roundup2(LEN__SET_TCB_FIELD_ULP, 16) +
	    roundup2(LEN__RX_DATA_ACK_ULP, 16);

	wr = alloc_wrqe(len, toep->ctrlq);
	if (wr == NULL)
		return (NULL);
	wrh = wrtod(wr);
	INIT_ULPTX_WRH(wrh, len, 1, 0);	/* atomic */
	ulpmc = (struct ulp_txpkt *)(wrh + 1);

	/* Write the buffer's tag */
	ulpmc = mk_set_tcb_field_ulp(ulpmc, toep,
	    W_TCB_RX_DDP_BUF0_TAG + db_idx,
	    V_TCB_RX_DDP_BUF0_TAG(M_TCB_RX_DDP_BUF0_TAG),
	    V_TCB_RX_DDP_BUF0_TAG(db->prsv.prsv_tag));

	/* Update the current offset in the DDP buffer and its total length */
	if (db_idx == 0)
		ulpmc = mk_set_tcb_field_ulp(ulpmc, toep,
		    W_TCB_RX_DDP_BUF0_OFFSET,
		    V_TCB_RX_DDP_BUF0_OFFSET(M_TCB_RX_DDP_BUF0_OFFSET) |
		    V_TCB_RX_DDP_BUF0_LEN(M_TCB_RX_DDP_BUF0_LEN),
		    V_TCB_RX_DDP_BUF0_OFFSET(offset) |
		    V_TCB_RX_DDP_BUF0_LEN(db->len));
	else
		ulpmc = mk_set_tcb_field_ulp(ulpmc, toep,
		    W_TCB_RX_DDP_BUF1_OFFSET,
		    V_TCB_RX_DDP_BUF1_OFFSET(M_TCB_RX_DDP_BUF1_OFFSET) |
		    V_TCB_RX_DDP_BUF1_LEN((u64)M_TCB_RX_DDP_BUF1_LEN << 32),
		    V_TCB_RX_DDP_BUF1_OFFSET(offset) |
		    V_TCB_RX_DDP_BUF1_LEN((u64)db->len << 32));

	/* Update DDP flags */
	ulpmc = mk_set_tcb_field_ulp(ulpmc, toep, W_TCB_RX_DDP_FLAGS,
	    V_TF_DDP_BUF0_FLUSH(1) | V_TF_DDP_BUF1_FLUSH(1) |
	    V_TF_DDP_PUSH_DISABLE_0(1) | V_TF_DDP_PUSH_DISABLE_1(1) |
	    V_TF_DDP_BUF0_VALID(1) | V_TF_DDP_BUF1_VALID(1) |
	    V_TF_DDP_ACTIVE_BUF(1) | V_TF_DDP_INDICATE_OUT(1), ddp_flags);

	/* Gratuitous RX_DATA_ACK with RX_MODULATE set to speed up delivery. */
	ulpmc = mk_rx_data_ack_ulp(ulpmc, toep);

	return (wr);
}

static void
discourage_ddp(struct toepcb *toep)
{

	if (toep->ddp_score && --toep->ddp_score == 0) {
		toep->ddp_flags &= ~DDP_OK;
		toep->ddp_disabled = time_uptime;
		CTR3(KTR_CXGBE, "%s: tid %u !DDP_OK @ %u",
		    __func__, toep->tid, time_uptime);
	}
}

static int
handle_ddp_data(struct toepcb *toep, __be32 ddp_report, __be32 rcv_nxt, int len)
{
	uint32_t report = be32toh(ddp_report);
	unsigned int db_flag;
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp;
	struct socket *so;
	struct sockbuf *sb;
	struct mbuf *m;

	db_flag = report & F_DDP_BUF_IDX ? DDP_BUF1_ACTIVE : DDP_BUF0_ACTIVE;

	if (__predict_false(!(report & F_DDP_INV)))
		CXGBE_UNIMPLEMENTED("DDP buffer still valid");

	INP_WLOCK(inp);
	so = inp_inpcbtosocket(inp);
	sb = &so->so_rcv;
	if (__predict_false(inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT))) {

		/*
		 * XXX: think a bit more.
		 * tcpcb probably gone, but socket should still be around
		 * because we always wait for DDP completion in soreceive no
		 * matter what.  Just wake it up and let it clean up.
		 */

		CTR5(KTR_CXGBE, "%s: tid %u, seq 0x%x, len %d, inp_flags 0x%x",
		    __func__, toep->tid, be32toh(rcv_nxt), len, inp->inp_flags);
		SOCKBUF_LOCK(sb);
		goto wakeup;
	}

	tp = intotcpcb(inp);

	/*
	 * For RX_DDP_COMPLETE, len will be zero and rcv_nxt is the
	 * sequence number of the next byte to receive.  The length of
	 * the data received for this message must be computed by
	 * comparing the new and old values of rcv_nxt.
	 * 
	 * For RX_DATA_DDP, len might be non-zero, but it is only the
	 * length of the most recent DMA.  It does not include the
	 * total length of the data received since the previous update
	 * for this DDP buffer.  rcv_nxt is the sequence number of the
	 * first received byte from the most recent DMA.
	 */
	len += be32toh(rcv_nxt) - tp->rcv_nxt;
	tp->rcv_nxt += len;
	tp->t_rcvtime = ticks;
#ifndef USE_DDP_RX_FLOW_CONTROL
	KASSERT(tp->rcv_wnd >= len, ("%s: negative window size", __func__));
	tp->rcv_wnd -= len;
#endif
	m = get_ddp_mbuf(len);

	SOCKBUF_LOCK(sb);
	if (report & F_DDP_BUF_COMPLETE)
		toep->ddp_score = DDP_HIGH_SCORE;
	else
		discourage_ddp(toep);

	/* receive buffer autosize */
	if (sb->sb_flags & SB_AUTOSIZE &&
	    V_tcp_do_autorcvbuf &&
	    sb->sb_hiwat < V_tcp_autorcvbuf_max &&
	    len > (sbspace(sb) / 8 * 7)) {
		unsigned int hiwat = sb->sb_hiwat;
		unsigned int newsize = min(hiwat + V_tcp_autorcvbuf_inc,
		    V_tcp_autorcvbuf_max);

		if (!sbreserve_locked(sb, newsize, so, NULL))
			sb->sb_flags &= ~SB_AUTOSIZE;
		else
			toep->rx_credits += newsize - hiwat;
	}

	KASSERT(toep->sb_cc >= sb->sb_cc,
	    ("%s: sb %p has more data (%d) than last time (%d).",
	    __func__, sb, sb->sb_cc, toep->sb_cc));
	toep->rx_credits += toep->sb_cc - sb->sb_cc;
#ifdef USE_DDP_RX_FLOW_CONTROL
	toep->rx_credits -= len;	/* adjust for F_RX_FC_DDP */
#endif
	sbappendstream_locked(sb, m);
	toep->sb_cc = sb->sb_cc;
wakeup:
	KASSERT(toep->ddp_flags & db_flag,
	    ("%s: DDP buffer not active. toep %p, ddp_flags 0x%x, report 0x%x",
	    __func__, toep, toep->ddp_flags, report));
	toep->ddp_flags &= ~db_flag;
	sorwakeup_locked(so);
	SOCKBUF_UNLOCK_ASSERT(sb);

	INP_WUNLOCK(inp);
	return (0);
}

void
handle_ddp_close(struct toepcb *toep, struct tcpcb *tp, struct sockbuf *sb,
    __be32 rcv_nxt)
{
	struct mbuf *m;
	int len;

	SOCKBUF_LOCK_ASSERT(sb);
	INP_WLOCK_ASSERT(toep->inp);
	len = be32toh(rcv_nxt) - tp->rcv_nxt;

	/* Signal handle_ddp() to break out of its sleep loop. */
	toep->ddp_flags &= ~(DDP_BUF0_ACTIVE | DDP_BUF1_ACTIVE);
	if (len == 0)
		return;

	tp->rcv_nxt += len;
	KASSERT(toep->sb_cc >= sb->sb_cc,
	    ("%s: sb %p has more data (%d) than last time (%d).",
	    __func__, sb, sb->sb_cc, toep->sb_cc));
	toep->rx_credits += toep->sb_cc - sb->sb_cc;
#ifdef USE_DDP_RX_FLOW_CONTROL
	toep->rx_credits -= len;	/* adjust for F_RX_FC_DDP */
#endif

	m = get_ddp_mbuf(len);

	sbappendstream_locked(sb, m);
	toep->sb_cc = sb->sb_cc;
}

#define DDP_ERR (F_DDP_PPOD_MISMATCH | F_DDP_LLIMIT_ERR | F_DDP_ULIMIT_ERR |\
	 F_DDP_PPOD_PARITY_ERR | F_DDP_PADDING_ERR | F_DDP_OFFSET_ERR |\
	 F_DDP_INVALID_TAG | F_DDP_COLOR_ERR | F_DDP_TID_MISMATCH |\
	 F_DDP_INVALID_PPOD | F_DDP_HDRCRC_ERR | F_DDP_DATACRC_ERR)

extern cpl_handler_t t4_cpl_handler[];

static int
do_rx_data_ddp(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_rx_data_ddp *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	uint32_t vld;
	struct toepcb *toep = lookup_tid(sc, tid);

	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == tid, ("%s: toep tid/atid mismatch", __func__));
	KASSERT(!(toep->flags & TPF_SYNQE),
	    ("%s: toep %p claims to be a synq entry", __func__, toep));

	vld = be32toh(cpl->ddpvld);
	if (__predict_false(vld & DDP_ERR)) {
		panic("%s: DDP error 0x%x (tid %d, toep %p)",
		    __func__, vld, tid, toep);
	}

	if (toep->ulp_mode == ULP_MODE_ISCSI) {
		t4_cpl_handler[CPL_RX_ISCSI_DDP](iq, rss, m);
		return (0);
	}

	handle_ddp_data(toep, cpl->u.ddp_report, cpl->seq, be16toh(cpl->len));

	return (0);
}

static int
do_rx_ddp_complete(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_rx_ddp_complete *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);

	KASSERT(m == NULL, ("%s: wasn't expecting payload", __func__));
	KASSERT(toep->tid == tid, ("%s: toep tid/atid mismatch", __func__));
	KASSERT(!(toep->flags & TPF_SYNQE),
	    ("%s: toep %p claims to be a synq entry", __func__, toep));

	handle_ddp_data(toep, cpl->ddp_report, cpl->rcv_nxt, 0);

	return (0);
}

void
enable_ddp(struct adapter *sc, struct toepcb *toep)
{

	KASSERT((toep->ddp_flags & (DDP_ON | DDP_OK | DDP_SC_REQ)) == DDP_OK,
	    ("%s: toep %p has bad ddp_flags 0x%x",
	    __func__, toep, toep->ddp_flags));

	CTR3(KTR_CXGBE, "%s: tid %u (time %u)",
	    __func__, toep->tid, time_uptime);

	toep->ddp_flags |= DDP_SC_REQ;
	t4_set_tcb_field(sc, toep->ctrlq, toep->tid, W_TCB_RX_DDP_FLAGS,
	    V_TF_DDP_OFF(1) | V_TF_DDP_INDICATE_OUT(1) |
	    V_TF_DDP_BUF0_INDICATE(1) | V_TF_DDP_BUF1_INDICATE(1) |
	    V_TF_DDP_BUF0_VALID(1) | V_TF_DDP_BUF1_VALID(1),
	    V_TF_DDP_BUF0_INDICATE(1) | V_TF_DDP_BUF1_INDICATE(1), 0, 0,
	    toep->ofld_rxq->iq.abs_id);
	t4_set_tcb_field(sc, toep->ctrlq, toep->tid, W_TCB_T_FLAGS,
	    V_TF_RCV_COALESCE_ENABLE(1), 0, 0, 0, toep->ofld_rxq->iq.abs_id);
}

static inline void
disable_ddp(struct adapter *sc, struct toepcb *toep)
{

	KASSERT((toep->ddp_flags & (DDP_ON | DDP_SC_REQ)) == DDP_ON,
	    ("%s: toep %p has bad ddp_flags 0x%x",
	    __func__, toep, toep->ddp_flags));

	CTR3(KTR_CXGBE, "%s: tid %u (time %u)",
	    __func__, toep->tid, time_uptime);

	toep->ddp_flags |= DDP_SC_REQ;
	t4_set_tcb_field(sc, toep->ctrlq, toep->tid, W_TCB_T_FLAGS,
	    V_TF_RCV_COALESCE_ENABLE(1), V_TF_RCV_COALESCE_ENABLE(1), 0, 0,
	    toep->ofld_rxq->iq.abs_id);
	t4_set_tcb_field(sc, toep->ctrlq, toep->tid, W_TCB_RX_DDP_FLAGS,
	    V_TF_DDP_OFF(1), V_TF_DDP_OFF(1), 0, 0, toep->ofld_rxq->iq.abs_id);
}

static int
hold_uio(struct uio *uio, vm_page_t **ppages, int *pnpages)
{
	struct vm_map *map;
	struct iovec *iov;
	vm_offset_t start, end;
	vm_page_t *pp;
	int n;

	KASSERT(uio->uio_iovcnt == 1,
	    ("%s: uio_iovcnt %d", __func__, uio->uio_iovcnt));
	KASSERT(uio->uio_td->td_proc == curproc,
	    ("%s: uio proc (%p) is not curproc (%p)",
	    __func__, uio->uio_td->td_proc, curproc));

	map = &curproc->p_vmspace->vm_map;
	iov = &uio->uio_iov[0];
	start = trunc_page((uintptr_t)iov->iov_base);
	end = round_page((vm_offset_t)iov->iov_base + iov->iov_len);
	n = howmany(end - start, PAGE_SIZE);

	if (end - start > MAX_DDP_BUFFER_SIZE)
		return (E2BIG);

	pp = malloc(n * sizeof(vm_page_t), M_CXGBE, M_NOWAIT);
	if (pp == NULL)
		return (ENOMEM);

	if (vm_fault_quick_hold_pages(map, (vm_offset_t)iov->iov_base,
	    iov->iov_len, VM_PROT_WRITE, pp, n) < 0) {
		free(pp, M_CXGBE);
		return (EFAULT);
	}

	*ppages = pp;
	*pnpages = n;

	return (0);
}

static int
bufcmp(struct ddp_buffer *db, vm_page_t *pages, int npages, int offset, int len)
{
	int i;

	if (db == NULL || db->npages != npages || db->offset != offset ||
	    db->len != len)
		return (1);

	for (i = 0; i < npages; i++) {
		if (pages[i]->phys_addr != db->pages[i]->phys_addr)
			return (1);
	}

	return (0);
}

static int
calculate_hcf(int n1, int n2)
{
	int a, b, t;

	if (n1 <= n2) {
		a = n1;
		b = n2;
	} else {
		a = n2;
		b = n1;
	}

	while (a != 0) {
		t = a;
		a = b % a;
		b = t;
	}

	return (b);
}

static inline int
pages_to_nppods(int npages, int ddp_page_shift)
{

	MPASS(ddp_page_shift >= PAGE_SHIFT);

	return (howmany(npages >> (ddp_page_shift - PAGE_SHIFT), PPOD_PAGES));
}

static int
alloc_page_pods(struct ppod_region *pr, u_int nppods, u_int pgsz_idx,
    struct ppod_reservation *prsv)
{
	vmem_addr_t addr;       /* relative to start of region */

	if (vmem_alloc(pr->pr_arena, PPOD_SZ(nppods), M_NOWAIT | M_FIRSTFIT,
	    &addr) != 0)
		return (ENOMEM);

	CTR5(KTR_CXGBE, "%-17s arena %p, addr 0x%08x, nppods %d, pgsz %d",
	    __func__, pr->pr_arena, (uint32_t)addr & pr->pr_tag_mask,
	    nppods, 1 << pr->pr_page_shift[pgsz_idx]);

	/*
	 * The hardware tagmask includes an extra invalid bit but the arena was
	 * seeded with valid values only.  An allocation out of this arena will
	 * fit inside the tagmask but won't have the invalid bit set.
	 */
	MPASS((addr & pr->pr_tag_mask) == addr);
	MPASS((addr & pr->pr_invalid_bit) == 0);

	prsv->prsv_pr = pr;
	prsv->prsv_tag = V_PPOD_PGSZ(pgsz_idx) | addr;
	prsv->prsv_nppods = nppods;

	return (0);
}

int
t4_alloc_page_pods_for_db(struct ppod_region *pr, struct ddp_buffer *db)
{
	int i, hcf, seglen, idx, nppods;
	struct ppod_reservation *prsv = &db->prsv;

	KASSERT(prsv->prsv_nppods == 0,
	    ("%s: page pods already allocated", __func__));

	/*
	 * The DDP page size is unrelated to the VM page size.  We combine
	 * contiguous physical pages into larger segments to get the best DDP
	 * page size possible.  This is the largest of the four sizes in
	 * A_ULP_RX_TDDP_PSZ that evenly divides the HCF of the segment sizes in
	 * the page list.
	 */
	hcf = 0;
	for (i = 0; i < db->npages; i++) {
		seglen = PAGE_SIZE;
		while (i < db->npages - 1 &&
		    db->pages[i]->phys_addr + PAGE_SIZE ==
		    db->pages[i + 1]->phys_addr) {
			seglen += PAGE_SIZE;
			i++;
		}

		hcf = calculate_hcf(hcf, seglen);
		if (hcf < (1 << pr->pr_page_shift[1])) {
			idx = 0;
			goto have_pgsz;	/* give up, short circuit */
		}
	}

#define PR_PAGE_MASK(x) ((1 << pr->pr_page_shift[(x)]) - 1)
	MPASS((hcf & PR_PAGE_MASK(0)) == 0); /* PAGE_SIZE is >= 4K everywhere */
	for (idx = nitems(pr->pr_page_shift) - 1; idx > 0; idx--) {
		if ((hcf & PR_PAGE_MASK(idx)) == 0)
			break;
	}
#undef PR_PAGE_MASK

have_pgsz:
	MPASS(idx <= M_PPOD_PGSZ);

	nppods = pages_to_nppods(db->npages, pr->pr_page_shift[idx]);
	if (alloc_page_pods(pr, nppods, idx, prsv) != 0)
		return (0);
	MPASS(prsv->prsv_nppods > 0);

	return (1);
}

int
t4_alloc_page_pods_for_buf(struct ppod_region *pr, vm_offset_t buf, int len,
    struct ppod_reservation *prsv)
{
	int hcf, seglen, idx, npages, nppods;
	uintptr_t start_pva, end_pva, pva, p1;

	MPASS(buf > 0);
	MPASS(len > 0);

	/*
	 * The DDP page size is unrelated to the VM page size.  We combine
	 * contiguous physical pages into larger segments to get the best DDP
	 * page size possible.  This is the largest of the four sizes in
	 * A_ULP_RX_ISCSI_PSZ that evenly divides the HCF of the segment sizes
	 * in the page list.
	 */
	hcf = 0;
	start_pva = trunc_page(buf);
	end_pva = trunc_page(buf + len - 1);
	pva = start_pva;
	while (pva <= end_pva) {
		seglen = PAGE_SIZE;
		p1 = pmap_kextract(pva);
		pva += PAGE_SIZE;
		while (pva <= end_pva && p1 + seglen == pmap_kextract(pva)) {
			seglen += PAGE_SIZE;
			pva += PAGE_SIZE;
		}

		hcf = calculate_hcf(hcf, seglen);
		if (hcf < (1 << pr->pr_page_shift[1])) {
			idx = 0;
			goto have_pgsz;	/* give up, short circuit */
		}
	}

#define PR_PAGE_MASK(x) ((1 << pr->pr_page_shift[(x)]) - 1)
	MPASS((hcf & PR_PAGE_MASK(0)) == 0); /* PAGE_SIZE is >= 4K everywhere */
	for (idx = nitems(pr->pr_page_shift) - 1; idx > 0; idx--) {
		if ((hcf & PR_PAGE_MASK(idx)) == 0)
			break;
	}
#undef PR_PAGE_MASK

have_pgsz:
	MPASS(idx <= M_PPOD_PGSZ);

	npages = 1;
	npages += (end_pva - start_pva) >> pr->pr_page_shift[idx];
	nppods = howmany(npages, PPOD_PAGES);
	if (alloc_page_pods(pr, nppods, idx, prsv) != 0)
		return (ENOMEM);
	MPASS(prsv->prsv_nppods > 0);

	return (0);
}

void
t4_free_page_pods(struct ppod_reservation *prsv)
{
	struct ppod_region *pr = prsv->prsv_pr;
	vmem_addr_t addr;

	MPASS(prsv != NULL);
	MPASS(prsv->prsv_nppods != 0);

	addr = prsv->prsv_tag & pr->pr_tag_mask;
	MPASS((addr & pr->pr_invalid_bit) == 0);

	CTR4(KTR_CXGBE, "%-17s arena %p, addr 0x%08x, nppods %d", __func__,
	    pr->pr_arena, addr, prsv->prsv_nppods);

	vmem_free(pr->pr_arena, addr, PPOD_SZ(prsv->prsv_nppods));
	prsv->prsv_nppods = 0;
}

#define NUM_ULP_TX_SC_IMM_PPODS (256 / PPOD_SIZE)

int
t4_write_page_pods_for_db(struct adapter *sc, struct sge_wrq *wrq, int tid,
    struct ddp_buffer *db)
{
	struct wrqe *wr;
	struct ulp_mem_io *ulpmc;
	struct ulptx_idata *ulpsc;
	struct pagepod *ppod;
	int i, j, k, n, chunk, len, ddp_pgsz, idx;
	u_int ppod_addr;
	uint32_t cmd;
	struct ppod_reservation *prsv = &db->prsv;
	struct ppod_region *pr = prsv->prsv_pr;

	MPASS(prsv->prsv_nppods > 0);

	cmd = htobe32(V_ULPTX_CMD(ULP_TX_MEM_WRITE));
	if (is_t4(sc))
		cmd |= htobe32(F_ULP_MEMIO_ORDER);
	else
		cmd |= htobe32(F_T5_ULP_MEMIO_IMM);
	ddp_pgsz = 1 << pr->pr_page_shift[G_PPOD_PGSZ(prsv->prsv_tag)];
	ppod_addr = pr->pr_start + (prsv->prsv_tag & pr->pr_tag_mask);
	for (i = 0; i < prsv->prsv_nppods; ppod_addr += chunk) {

		/* How many page pods are we writing in this cycle */
		n = min(prsv->prsv_nppods - i, NUM_ULP_TX_SC_IMM_PPODS);
		chunk = PPOD_SZ(n);
		len = roundup2(sizeof(*ulpmc) + sizeof(*ulpsc) + chunk, 16);

		wr = alloc_wrqe(len, wrq);
		if (wr == NULL)
			return (ENOMEM);	/* ok to just bail out */
		ulpmc = wrtod(wr);

		INIT_ULPTX_WR(ulpmc, len, 0, 0);
		ulpmc->cmd = cmd;
		ulpmc->dlen = htobe32(V_ULP_MEMIO_DATA_LEN(chunk / 32));
		ulpmc->len16 = htobe32(howmany(len - sizeof(ulpmc->wr), 16));
		ulpmc->lock_addr = htobe32(V_ULP_MEMIO_ADDR(ppod_addr >> 5));

		ulpsc = (struct ulptx_idata *)(ulpmc + 1);
		ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
		ulpsc->len = htobe32(chunk);

		ppod = (struct pagepod *)(ulpsc + 1);
		for (j = 0; j < n; i++, j++, ppod++) {
			ppod->vld_tid_pgsz_tag_color = htobe64(F_PPOD_VALID |
			    V_PPOD_TID(tid) | prsv->prsv_tag);
			ppod->len_offset = htobe64(V_PPOD_LEN(db->len) |
			    V_PPOD_OFST(db->offset));
			ppod->rsvd = 0;
			idx = i * PPOD_PAGES * (ddp_pgsz / PAGE_SIZE);
			for (k = 0; k < nitems(ppod->addr); k++) {
				if (idx < db->npages) {
					ppod->addr[k] =
					    htobe64(db->pages[idx]->phys_addr);
					idx += ddp_pgsz / PAGE_SIZE;
				} else
					ppod->addr[k] = 0;
#if 0
				CTR5(KTR_CXGBE,
				    "%s: tid %d ppod[%d]->addr[%d] = %p",
				    __func__, toep->tid, i, k,
				    htobe64(ppod->addr[k]));
#endif
			}

		}

		t4_wrq_tx(sc, wr);
	}

	return (0);
}

int
t4_write_page_pods_for_buf(struct adapter *sc, struct sge_wrq *wrq, int tid,
    struct ppod_reservation *prsv, vm_offset_t buf, int buflen)
{
	struct wrqe *wr;
	struct ulp_mem_io *ulpmc;
	struct ulptx_idata *ulpsc;
	struct pagepod *ppod;
	int i, j, k, n, chunk, len, ddp_pgsz;
	u_int ppod_addr, offset;
	uint32_t cmd;
	struct ppod_region *pr = prsv->prsv_pr;
	uintptr_t end_pva, pva, pa;

	cmd = htobe32(V_ULPTX_CMD(ULP_TX_MEM_WRITE));
	if (is_t4(sc))
		cmd |= htobe32(F_ULP_MEMIO_ORDER);
	else
		cmd |= htobe32(F_T5_ULP_MEMIO_IMM);
	ddp_pgsz = 1 << pr->pr_page_shift[G_PPOD_PGSZ(prsv->prsv_tag)];
	offset = buf & PAGE_MASK;
	ppod_addr = pr->pr_start + (prsv->prsv_tag & pr->pr_tag_mask);
	pva = trunc_page(buf);
	end_pva = trunc_page(buf + buflen - 1);
	for (i = 0; i < prsv->prsv_nppods; ppod_addr += chunk) {

		/* How many page pods are we writing in this cycle */
		n = min(prsv->prsv_nppods - i, NUM_ULP_TX_SC_IMM_PPODS);
		MPASS(n > 0);
		chunk = PPOD_SZ(n);
		len = roundup2(sizeof(*ulpmc) + sizeof(*ulpsc) + chunk, 16);

		wr = alloc_wrqe(len, wrq);
		if (wr == NULL)
			return (ENOMEM);	/* ok to just bail out */
		ulpmc = wrtod(wr);

		INIT_ULPTX_WR(ulpmc, len, 0, 0);
		ulpmc->cmd = cmd;
		ulpmc->dlen = htobe32(V_ULP_MEMIO_DATA_LEN(chunk / 32));
		ulpmc->len16 = htobe32(howmany(len - sizeof(ulpmc->wr), 16));
		ulpmc->lock_addr = htobe32(V_ULP_MEMIO_ADDR(ppod_addr >> 5));

		ulpsc = (struct ulptx_idata *)(ulpmc + 1);
		ulpsc->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
		ulpsc->len = htobe32(chunk);

		ppod = (struct pagepod *)(ulpsc + 1);
		for (j = 0; j < n; i++, j++, ppod++) {
			ppod->vld_tid_pgsz_tag_color = htobe64(F_PPOD_VALID |
			    V_PPOD_TID(tid) |
			    (prsv->prsv_tag & ~V_PPOD_PGSZ(M_PPOD_PGSZ)));
			ppod->len_offset = htobe64(V_PPOD_LEN(buflen) |
			    V_PPOD_OFST(offset));
			ppod->rsvd = 0;

			for (k = 0; k < nitems(ppod->addr); k++) {
				if (pva > end_pva)
					ppod->addr[k] = 0;
				else {
					pa = pmap_kextract(pva);
					ppod->addr[k] = htobe64(pa);
					pva += ddp_pgsz;
				}
#if 0
				CTR5(KTR_CXGBE,
				    "%s: tid %d ppod[%d]->addr[%d] = %p",
				    __func__, tid, i, k,
				    htobe64(ppod->addr[k]));
#endif
			}

			/*
			 * Walk back 1 segment so that the first address in the
			 * next pod is the same as the last one in the current
			 * pod.
			 */
			pva -= ddp_pgsz;
		}

		t4_wrq_tx(sc, wr);
	}

	MPASS(pva <= end_pva);

	return (0);
}

/*
 * Reuse, or allocate (and program the page pods for) a new DDP buffer.  The
 * "pages" array is handed over to this function and should not be used in any
 * way by the caller after that.
 */
static int
select_ddp_buffer(struct adapter *sc, struct toepcb *toep, vm_page_t *pages,
    int npages, int db_off, int db_len)
{
	struct ddp_buffer *db;
	struct tom_data *td = sc->tom_softc;
	int i, empty_slot = -1;

	/* Try to reuse */
	for (i = 0; i < nitems(toep->db); i++) {
		if (bufcmp(toep->db[i], pages, npages, db_off, db_len) == 0) {
			free(pages, M_CXGBE);
			return (i);	/* pages still held */
		} else if (toep->db[i] == NULL && empty_slot < 0)
			empty_slot = i;
	}

	/* Allocate new buffer, write its page pods. */
	db = alloc_ddp_buffer(pages, npages, db_off, db_len);
	if (db == NULL) {
		vm_page_unhold_pages(pages, npages);
		free(pages, M_CXGBE);
		return (-1);
	}
	if (t4_alloc_page_pods_for_db(&td->pr, db)) {
		vm_page_unhold_pages(pages, npages);
		free_ddp_buffer(db);
		return (-1);
	}
	if (t4_write_page_pods_for_db(sc, toep->ctrlq, toep->tid, db) != 0) {
		vm_page_unhold_pages(pages, npages);
		free_ddp_buffer(db);
		return (-1);
	}

	i = empty_slot;
	if (i < 0) {
		i = arc4random() % nitems(toep->db);
		free_ddp_buffer(toep->db[i]);
	}
	toep->db[i] = db;

	CTR5(KTR_CXGBE, "%s: tid %d, DDP buffer[%d] = %p (tag 0x%x)",
	    __func__, toep->tid, i, db, db->prsv.prsv_tag);

	return (i);
}

static void
wire_ddp_buffer(struct ddp_buffer *db)
{
	int i;
	vm_page_t p;

	for (i = 0; i < db->npages; i++) {
		p = db->pages[i];
		vm_page_lock(p);
		vm_page_wire(p);
		vm_page_unhold(p);
		vm_page_unlock(p);
	}
}

static void
unwire_ddp_buffer(struct ddp_buffer *db)
{
	int i;
	vm_page_t p;

	for (i = 0; i < db->npages; i++) {
		p = db->pages[i];
		vm_page_lock(p);
		vm_page_unwire(p, 0);
		vm_page_unlock(p);
	}
}

static int
handle_ddp(struct socket *so, struct uio *uio, int flags, int error)
{
	struct sockbuf *sb = &so->so_rcv;
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct adapter *sc = td_adapter(toep->td);
	vm_page_t *pages;
	int npages, db_idx, rc, buf_flag;
	struct ddp_buffer *db;
	struct wrqe *wr;
	uint64_t ddp_flags;

	SOCKBUF_LOCK_ASSERT(sb);

#if 0
	if (sb->sb_cc + sc->tt.ddp_thres > uio->uio_resid) {
		CTR4(KTR_CXGBE, "%s: sb_cc %d, threshold %d, resid %d",
		    __func__, sb->sb_cc, sc->tt.ddp_thres, uio->uio_resid);
	}
#endif

	/* XXX: too eager to disable DDP, could handle NBIO better than this. */
	if (sb->sb_cc >= uio->uio_resid || uio->uio_resid < sc->tt.ddp_thres ||
	    uio->uio_resid > MAX_DDP_BUFFER_SIZE || uio->uio_iovcnt > 1 ||
	    so->so_state & SS_NBIO || flags & (MSG_DONTWAIT | MSG_NBIO) ||
	    error || so->so_error || sb->sb_state & SBS_CANTRCVMORE)
		goto no_ddp;

	/*
	 * Fault in and then hold the pages of the uio buffers.  We'll wire them
	 * a bit later if everything else works out.
	 */
	SOCKBUF_UNLOCK(sb);
	if (hold_uio(uio, &pages, &npages) != 0) {
		SOCKBUF_LOCK(sb);
		goto no_ddp;
	}
	SOCKBUF_LOCK(sb);
	if (__predict_false(so->so_error || sb->sb_state & SBS_CANTRCVMORE)) {
		vm_page_unhold_pages(pages, npages);
		free(pages, M_CXGBE);
		goto no_ddp;
	}

	/*
	 * Figure out which one of the two DDP buffers to use this time.
	 */
	db_idx = select_ddp_buffer(sc, toep, pages, npages,
	    (uintptr_t)uio->uio_iov->iov_base & PAGE_MASK, uio->uio_resid);
	pages = NULL;	/* handed off to select_ddp_buffer */
	if (db_idx < 0)
		goto no_ddp;
	db = toep->db[db_idx];
	buf_flag = db_idx == 0 ? DDP_BUF0_ACTIVE : DDP_BUF1_ACTIVE;

	/*
	 * Build the compound work request that tells the chip where to DMA the
	 * payload.
	 */
	ddp_flags = select_ddp_flags(so, flags, db_idx);
	wr = mk_update_tcb_for_ddp(sc, toep, db_idx, sb->sb_cc, ddp_flags);
	if (wr == NULL) {
		/*
		 * Just unhold the pages.  The DDP buffer's software state is
		 * left as-is in the toep.  The page pods were written
		 * successfully and we may have an opportunity to use it in the
		 * future.
		 */
		vm_page_unhold_pages(db->pages, db->npages);
		goto no_ddp;
	}

	/* Wire (and then unhold) the pages, and give the chip the go-ahead. */
	wire_ddp_buffer(db);
	t4_wrq_tx(sc, wr);
	sb->sb_flags &= ~SB_DDP_INDICATE;
	toep->ddp_flags |= buf_flag;

	/*
	 * Wait for the DDP operation to complete and then unwire the pages.
	 * The return code from the sbwait will be the final return code of this
	 * function.  But we do need to wait for DDP no matter what.
	 */
	rc = sbwait(sb);
	while (toep->ddp_flags & buf_flag) {
		sb->sb_flags |= SB_WAIT;
		msleep(&sb->sb_cc, &sb->sb_mtx, PSOCK , "sbwait", 0);
	}
	unwire_ddp_buffer(db);
	return (rc);
no_ddp:
	disable_ddp(sc, toep);
	discourage_ddp(toep);
	sb->sb_flags &= ~SB_DDP_INDICATE;
	return (0);
}

int
t4_init_ppod_region(struct ppod_region *pr, struct t4_range *r, u_int psz,
    const char *name)
{
	int i;

	MPASS(pr != NULL);
	MPASS(r->size > 0);

	pr->pr_start = r->start;
	pr->pr_len = r->size;
	pr->pr_page_shift[0] = 12 + G_HPZ0(psz);
	pr->pr_page_shift[1] = 12 + G_HPZ1(psz);
	pr->pr_page_shift[2] = 12 + G_HPZ2(psz);
	pr->pr_page_shift[3] = 12 + G_HPZ3(psz);

	/* The SGL -> page pod algorithm requires the sizes to be in order. */
	for (i = 1; i < nitems(pr->pr_page_shift); i++) {
		if (pr->pr_page_shift[i] <= pr->pr_page_shift[i - 1])
			return (ENXIO);
	}

	pr->pr_tag_mask = ((1 << fls(r->size)) - 1) & V_PPOD_TAG(M_PPOD_TAG);
	pr->pr_alias_mask = V_PPOD_TAG(M_PPOD_TAG) & ~pr->pr_tag_mask;
	if (pr->pr_tag_mask == 0 || pr->pr_alias_mask == 0)
		return (ENXIO);
	pr->pr_alias_shift = fls(pr->pr_tag_mask);
	pr->pr_invalid_bit = 1 << (pr->pr_alias_shift - 1);

	pr->pr_arena = vmem_create(name, 0, pr->pr_len, PPOD_SIZE, 0,
	    M_FIRSTFIT | M_NOWAIT);
	if (pr->pr_arena == NULL)
		return (ENOMEM);

	return (0);
}

void
t4_free_ppod_region(struct ppod_region *pr)
{

	MPASS(pr != NULL);

	if (pr->pr_arena)
		vmem_destroy(pr->pr_arena);
	bzero(pr, sizeof(*pr));
}

#define	VNET_SO_ASSERT(so)						\
	VNET_ASSERT(curvnet != NULL,					\
	    ("%s:%d curvnet is NULL, so=%p", __func__, __LINE__, (so)));
#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? 0 : SBL_WAIT)
static int
soreceive_rcvoob(struct socket *so, struct uio *uio, int flags)
{

	CXGBE_UNIMPLEMENTED(__func__);
}

static char ddp_magic_str[] = "nothing to see here";

static struct mbuf *
get_ddp_mbuf(int len)
{
	struct mbuf *m;

	m = m_get(M_NOWAIT, MT_DATA);
	if (m == NULL)
		CXGBE_UNIMPLEMENTED("mbuf alloc failure");
	m->m_len = len;
	m->m_data = &ddp_magic_str[0];

	return (m);
}

static inline int
is_ddp_mbuf(struct mbuf *m)
{

	return (m->m_data == &ddp_magic_str[0]);
}

/*
 * Copy an mbuf chain into a uio limited by len if set.
 */
static int
m_mbuftouio_ddp(struct uio *uio, struct mbuf *m, int len)
{
	int error, length, total;
	int progress = 0;

	if (len > 0)
		total = min(uio->uio_resid, len);
	else
		total = uio->uio_resid;

	/* Fill the uio with data from the mbufs. */
	for (; m != NULL; m = m->m_next) {
		length = min(m->m_len, total - progress);

		if (is_ddp_mbuf(m)) {
			enum uio_seg segflag = uio->uio_segflg;

			uio->uio_segflg	= UIO_NOCOPY;
			error = uiomove(mtod(m, void *), length, uio);
			uio->uio_segflg	= segflag;
		} else
			error = uiomove(mtod(m, void *), length, uio);
		if (error)
			return (error);

		progress += length;
	}

	return (0);
}

/*
 * Based on soreceive_stream() in uipc_socket.c
 */
int
t4_soreceive_ddp(struct socket *so, struct sockaddr **psa, struct uio *uio,
    struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	int len = 0, error = 0, flags, oresid, ddp_handled = 0;
	struct sockbuf *sb;
	struct mbuf *m, *n = NULL;

	/* We only do stream sockets. */
	if (so->so_type != SOCK_STREAM)
		return (EINVAL);
	if (psa != NULL)
		*psa = NULL;
	if (controlp != NULL)
		return (EINVAL);
	if (flagsp != NULL)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;
	if (flags & MSG_OOB)
		return (soreceive_rcvoob(so, uio, flags));
	if (mp0 != NULL)
		*mp0 = NULL;

	sb = &so->so_rcv;

	/* Prevent other readers from entering the socket. */
	error = sblock(sb, SBLOCKWAIT(flags));
	SOCKBUF_LOCK(sb);
	if (error)
		goto out;

	/* Easy one, no space to copyout anything. */
	if (uio->uio_resid == 0) {
		error = EINVAL;
		goto out;
	}
	oresid = uio->uio_resid;

	/* We will never ever get anything unless we are or were connected. */
	if (!(so->so_state & (SS_ISCONNECTED|SS_ISDISCONNECTED))) {
		error = ENOTCONN;
		goto out;
	}

restart:
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);

	if (sb->sb_flags & SB_DDP_INDICATE && !ddp_handled) {

		/* uio should be just as it was at entry */
		KASSERT(oresid == uio->uio_resid,
		    ("%s: oresid = %d, uio_resid = %zd, sb_cc = %d",
		    __func__, oresid, uio->uio_resid, sb->sb_cc));

		error = handle_ddp(so, uio, flags, 0);
		ddp_handled = 1;
		if (error)
			goto out;
	}

	/* Abort if socket has reported problems. */
	if (so->so_error) {
		if (sb->sb_cc > 0)
			goto deliver;
		if (oresid > uio->uio_resid)
			goto out;
		error = so->so_error;
		if (!(flags & MSG_PEEK))
			so->so_error = 0;
		goto out;
	}

	/* Door is closed.  Deliver what is left, if any. */
	if (sb->sb_state & SBS_CANTRCVMORE) {
		if (sb->sb_cc > 0)
			goto deliver;
		else
			goto out;
	}

	/* Socket buffer is empty and we shall not block. */
	if (sb->sb_cc == 0 &&
	    ((so->so_state & SS_NBIO) || (flags & (MSG_DONTWAIT|MSG_NBIO)))) {
		error = EAGAIN;
		goto out;
	}

	/* Socket buffer got some data that we shall deliver now. */
	if (sb->sb_cc > 0 && !(flags & MSG_WAITALL) &&
	    ((so->so_state & SS_NBIO) ||
	     (flags & (MSG_DONTWAIT|MSG_NBIO)) ||
	     sb->sb_cc >= sb->sb_lowat ||
	     sb->sb_cc >= uio->uio_resid ||
	     sb->sb_cc >= sb->sb_hiwat) ) {
		goto deliver;
	}

	/* On MSG_WAITALL we must wait until all data or error arrives. */
	if ((flags & MSG_WAITALL) &&
	    (sb->sb_cc >= uio->uio_resid || sb->sb_cc >= sb->sb_lowat))
		goto deliver;

	/*
	 * Wait and block until (more) data comes in.
	 * NB: Drops the sockbuf lock during wait.
	 */
	error = sbwait(sb);
	if (error) {
		if (sb->sb_flags & SB_DDP_INDICATE && !ddp_handled) {
			(void) handle_ddp(so, uio, flags, 1);
			ddp_handled = 1;
		}
		goto out;
	}
	goto restart;

deliver:
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	KASSERT(sb->sb_cc > 0, ("%s: sockbuf empty", __func__));
	KASSERT(sb->sb_mb != NULL, ("%s: sb_mb == NULL", __func__));

	if (sb->sb_flags & SB_DDP_INDICATE && !ddp_handled)
		goto restart;

	/* Statistics. */
	if (uio->uio_td)
		uio->uio_td->td_ru.ru_msgrcv++;

	/* Fill uio until full or current end of socket buffer is reached. */
	len = min(uio->uio_resid, sb->sb_cc);
	if (mp0 != NULL) {
		/* Dequeue as many mbufs as possible. */
		if (!(flags & MSG_PEEK) && len >= sb->sb_mb->m_len) {
			for (*mp0 = m = sb->sb_mb;
			     m != NULL && m->m_len <= len;
			     m = m->m_next) {
				len -= m->m_len;
				uio->uio_resid -= m->m_len;
				sbfree(sb, m);
				n = m;
			}
			sb->sb_mb = m;
			if (sb->sb_mb == NULL)
				SB_EMPTY_FIXUP(sb);
			n->m_next = NULL;
		}
		/* Copy the remainder. */
		if (len > 0) {
			KASSERT(sb->sb_mb != NULL,
			    ("%s: len > 0 && sb->sb_mb empty", __func__));

			m = m_copym(sb->sb_mb, 0, len, M_NOWAIT);
			if (m == NULL)
				len = 0;	/* Don't flush data from sockbuf. */
			else
				uio->uio_resid -= m->m_len;
			if (*mp0 != NULL)
				n->m_next = m;
			else
				*mp0 = m;
			if (*mp0 == NULL) {
				error = ENOBUFS;
				goto out;
			}
		}
	} else {
		/* NB: Must unlock socket buffer as uiomove may sleep. */
		SOCKBUF_UNLOCK(sb);
		error = m_mbuftouio_ddp(uio, sb->sb_mb, len);
		SOCKBUF_LOCK(sb);
		if (error)
			goto out;
	}
	SBLASTRECORDCHK(sb);
	SBLASTMBUFCHK(sb);

	/*
	 * Remove the delivered data from the socket buffer unless we
	 * were only peeking.
	 */
	if (!(flags & MSG_PEEK)) {
		if (len > 0)
			sbdrop_locked(sb, len);

		/* Notify protocol that we drained some data. */
		if ((so->so_proto->pr_flags & PR_WANTRCVD) &&
		    (((flags & MSG_WAITALL) && uio->uio_resid > 0) ||
		     !(flags & MSG_SOCALLBCK))) {
			SOCKBUF_UNLOCK(sb);
			VNET_SO_ASSERT(so);
			(*so->so_proto->pr_usrreqs->pru_rcvd)(so, flags);
			SOCKBUF_LOCK(sb);
		}
	}

	/*
	 * For MSG_WAITALL we may have to loop again and wait for
	 * more data to come in.
	 */
	if ((flags & MSG_WAITALL) && uio->uio_resid > 0)
		goto restart;
out:
	SOCKBUF_LOCK_ASSERT(sb);
	SBLASTRECORDCHK(sb);
	SBLASTMBUFCHK(sb);
	SOCKBUF_UNLOCK(sb);
	sbunlock(sb);
	return (error);
}

int
t4_ddp_mod_load(void)
{

	t4_register_cpl_handler(CPL_RX_DATA_DDP, do_rx_data_ddp);
	t4_register_cpl_handler(CPL_RX_DDP_COMPLETE, do_rx_ddp_complete);
	return (0);
}

void
t4_ddp_mod_unload(void)
{

	t4_register_cpl_handler(CPL_RX_DATA_DDP, NULL);
	t4_register_cpl_handler(CPL_RX_DDP_COMPLETE, NULL);
}
#endif
