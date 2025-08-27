/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
#include <sys/file.h>
#include <sys/gsb_crc32.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/nv.h>
#include <sys/protosw.h>
#include <sys/refcount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_proto.h>
#include <dev/nvmf/nvmf_tcp.h>
#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/nvmf_transport_internal.h>

struct nvmf_tcp_capsule;
struct nvmf_tcp_qpair;

struct nvmf_tcp_command_buffer {
	struct nvmf_tcp_qpair *qp;

	struct nvmf_io_request io;
	size_t	data_len;
	size_t	data_xfered;
	uint32_t data_offset;

	u_int	refs;
	int	error;

	uint16_t cid;
	uint16_t ttag;

	TAILQ_ENTRY(nvmf_tcp_command_buffer) link;

	/* Controller only */
	struct nvmf_tcp_capsule *tc;
};

struct nvmf_tcp_command_buffer_list {
	TAILQ_HEAD(, nvmf_tcp_command_buffer) head;
	struct mtx lock;
};

struct nvmf_tcp_qpair {
	struct nvmf_qpair qp;

	struct socket *so;

	volatile u_int refs;	/* Every allocated capsule holds a reference */
	uint8_t	txpda;
	uint8_t rxpda;
	bool header_digests;
	bool data_digests;
	uint32_t maxr2t;
	uint32_t maxh2cdata;	/* Controller only */
	uint32_t max_tx_data;
	uint32_t max_icd;	/* Host only */
	uint16_t next_ttag;	/* Controller only */
	u_int num_ttags;	/* Controller only */
	u_int active_ttags;	/* Controller only */
	bool send_success;	/* Controller only */

	/* Receive state. */
	struct thread *rx_thread;
	struct cv rx_cv;
	bool	rx_shutdown;

	/* Transmit state. */
	struct thread *tx_thread;
	struct cv tx_cv;
	bool	tx_shutdown;
	struct mbufq tx_pdus;
	STAILQ_HEAD(, nvmf_tcp_capsule) tx_capsules;

	struct nvmf_tcp_command_buffer_list tx_buffers;
	struct nvmf_tcp_command_buffer_list rx_buffers;

	/*
	 * For the controller, an RX command buffer can be in one of
	 * two locations, all protected by the rx_buffers.lock.  If a
	 * receive request is waiting for either an R2T slot for its
	 * command (due to exceeding MAXR2T), or a transfer tag it is
	 * placed on the rx_buffers list.  When a request is allocated
	 * an active transfer tag, it moves to the open_ttags[] array
	 * (indexed by the tag) until it completes.
	 */
	struct nvmf_tcp_command_buffer **open_ttags;	/* Controller only */
};

struct nvmf_tcp_rxpdu {
	struct mbuf *m;
	const struct nvme_tcp_common_pdu_hdr *hdr;
	uint32_t data_len;
	bool data_digest_mismatch;
};

struct nvmf_tcp_capsule {
	struct nvmf_capsule nc;

	volatile u_int refs;

	struct nvmf_tcp_rxpdu rx_pdu;

	uint32_t active_r2ts;		/* Controller only */
#ifdef INVARIANTS
	uint32_t tx_data_offset;	/* Controller only */
	u_int pending_r2ts;		/* Controller only */
#endif

	STAILQ_ENTRY(nvmf_tcp_capsule) link;
};

#define	TCAP(nc)	((struct nvmf_tcp_capsule *)(nc))
#define	TQP(qp)		((struct nvmf_tcp_qpair *)(qp))

static void	tcp_release_capsule(struct nvmf_tcp_capsule *tc);
static void	tcp_free_qpair(struct nvmf_qpair *nq);

SYSCTL_NODE(_kern_nvmf, OID_AUTO, tcp, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "TCP transport");
static u_int tcp_max_transmit_data = 256 * 1024;
SYSCTL_UINT(_kern_nvmf_tcp, OID_AUTO, max_transmit_data, CTLFLAG_RWTUN,
    &tcp_max_transmit_data, 0,
    "Maximum size of data payload in a transmitted PDU");

static MALLOC_DEFINE(M_NVMF_TCP, "nvmf_tcp", "NVMe over TCP");

static int
mbuf_crc32c_helper(void *arg, void *data, u_int len)
{
	uint32_t *digestp = arg;

	*digestp = calculate_crc32c(*digestp, data, len);
	return (0);
}

static uint32_t
mbuf_crc32c(struct mbuf *m, u_int offset, u_int len)
{
	uint32_t digest = 0xffffffff;

	m_apply(m, offset, len, mbuf_crc32c_helper, &digest);
	digest = digest ^ 0xffffffff;

	return (digest);
}

static uint32_t
compute_digest(const void *buf, size_t len)
{
	return (calculate_crc32c(0xffffffff, buf, len) ^ 0xffffffff);
}

static struct nvmf_tcp_command_buffer *
tcp_alloc_command_buffer(struct nvmf_tcp_qpair *qp,
    const struct nvmf_io_request *io, uint32_t data_offset, size_t data_len,
    uint16_t cid)
{
	struct nvmf_tcp_command_buffer *cb;

	cb = malloc(sizeof(*cb), M_NVMF_TCP, M_WAITOK);
	cb->qp = qp;
	cb->io = *io;
	cb->data_offset = data_offset;
	cb->data_len = data_len;
	cb->data_xfered = 0;
	refcount_init(&cb->refs, 1);
	cb->error = 0;
	cb->cid = cid;
	cb->ttag = 0;
	cb->tc = NULL;

	return (cb);
}

static void
tcp_hold_command_buffer(struct nvmf_tcp_command_buffer *cb)
{
	refcount_acquire(&cb->refs);
}

static void
tcp_free_command_buffer(struct nvmf_tcp_command_buffer *cb)
{
	nvmf_complete_io_request(&cb->io, cb->data_xfered, cb->error);
	if (cb->tc != NULL)
		tcp_release_capsule(cb->tc);
	free(cb, M_NVMF_TCP);
}

static void
tcp_release_command_buffer(struct nvmf_tcp_command_buffer *cb)
{
	if (refcount_release(&cb->refs))
		tcp_free_command_buffer(cb);
}

static void
tcp_add_command_buffer(struct nvmf_tcp_command_buffer_list *list,
    struct nvmf_tcp_command_buffer *cb)
{
	mtx_assert(&list->lock, MA_OWNED);
	TAILQ_INSERT_HEAD(&list->head, cb, link);
}

static struct nvmf_tcp_command_buffer *
tcp_find_command_buffer(struct nvmf_tcp_command_buffer_list *list,
    uint16_t cid, uint16_t ttag)
{
	struct nvmf_tcp_command_buffer *cb;

	mtx_assert(&list->lock, MA_OWNED);
	TAILQ_FOREACH(cb, &list->head, link) {
		if (cb->cid == cid && cb->ttag == ttag)
			return (cb);
	}
	return (NULL);
}

static void
tcp_remove_command_buffer(struct nvmf_tcp_command_buffer_list *list,
    struct nvmf_tcp_command_buffer *cb)
{
	mtx_assert(&list->lock, MA_OWNED);
	TAILQ_REMOVE(&list->head, cb, link);
}

static void
tcp_purge_command_buffer(struct nvmf_tcp_command_buffer_list *list,
    uint16_t cid, uint16_t ttag)
{
	struct nvmf_tcp_command_buffer *cb;

	mtx_lock(&list->lock);
	cb = tcp_find_command_buffer(list, cid, ttag);
	if (cb != NULL) {
		tcp_remove_command_buffer(list, cb);
		mtx_unlock(&list->lock);
		tcp_release_command_buffer(cb);
	} else
		mtx_unlock(&list->lock);
}

static void
nvmf_tcp_write_pdu(struct nvmf_tcp_qpair *qp, struct mbuf *m)
{
	struct socket *so = qp->so;

	SOCKBUF_LOCK(&so->so_snd);
	mbufq_enqueue(&qp->tx_pdus, m);
	/* XXX: Do we need to handle sb_hiwat being wrong? */
	if (sowriteable(so))
		cv_signal(&qp->tx_cv);
	SOCKBUF_UNLOCK(&so->so_snd);
}

static void
nvmf_tcp_report_error(struct nvmf_tcp_qpair *qp, uint16_t fes, uint32_t fei,
    struct mbuf *rx_pdu, u_int hlen)
{
	struct nvme_tcp_term_req_hdr *hdr;
	struct mbuf *m;

	if (hlen != 0) {
		hlen = min(hlen, NVME_TCP_TERM_REQ_ERROR_DATA_MAX_SIZE);
		hlen = min(hlen, m_length(rx_pdu, NULL));
	}

	m = m_get2(sizeof(*hdr) + hlen, M_WAITOK, MT_DATA, 0);
	m->m_len = sizeof(*hdr) + hlen;
	hdr = mtod(m, void *);
	memset(hdr, 0, sizeof(*hdr));
	hdr->common.pdu_type = qp->qp.nq_controller ?
	    NVME_TCP_PDU_TYPE_C2H_TERM_REQ : NVME_TCP_PDU_TYPE_H2C_TERM_REQ;
	hdr->common.hlen = sizeof(*hdr);
	hdr->common.plen = sizeof(*hdr) + hlen;
	hdr->fes = htole16(fes);
	le32enc(hdr->fei, fei);
	if (hlen != 0)
		m_copydata(rx_pdu, 0, hlen, (caddr_t)(hdr + 1));

	nvmf_tcp_write_pdu(qp, m);
}

static int
nvmf_tcp_validate_pdu(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	const struct nvme_tcp_common_pdu_hdr *ch;
	struct mbuf *m = pdu->m;
	uint32_t data_len, fei, plen;
	uint32_t digest, rx_digest;
	u_int hlen;
	int error;
	uint16_t fes;

	/* Determine how large of a PDU header to return for errors. */
	ch = pdu->hdr;
	hlen = ch->hlen;
	plen = le32toh(ch->plen);
	if (hlen < sizeof(*ch) || hlen > plen)
		hlen = sizeof(*ch);

	error = nvmf_tcp_validate_pdu_header(ch, qp->qp.nq_controller,
	    qp->header_digests, qp->data_digests, qp->rxpda, &data_len, &fes,
	    &fei);
	if (error != 0) {
		if (error != ECONNRESET)
			nvmf_tcp_report_error(qp, fes, fei, m, hlen);
		return (error);
	}

	/* Check header digest if present. */
	if ((ch->flags & NVME_TCP_CH_FLAGS_HDGSTF) != 0) {
		digest = mbuf_crc32c(m, 0, ch->hlen);
		m_copydata(m, ch->hlen, sizeof(rx_digest), (caddr_t)&rx_digest);
		if (digest != rx_digest) {
			printf("NVMe/TCP: Header digest mismatch\n");
			nvmf_tcp_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_HDGST_ERROR, rx_digest, m,
			    hlen);
			return (EBADMSG);
		}
	}

	/* Check data digest if present. */
	pdu->data_digest_mismatch = false;
	if ((ch->flags & NVME_TCP_CH_FLAGS_DDGSTF) != 0) {
		digest = mbuf_crc32c(m, ch->pdo, data_len);
		m_copydata(m, plen - sizeof(rx_digest), sizeof(rx_digest),
		    (caddr_t)&rx_digest);
		if (digest != rx_digest) {
			printf("NVMe/TCP: Data digest mismatch\n");
			pdu->data_digest_mismatch = true;
		}
	}

	pdu->data_len = data_len;
	return (0);
}

static void
nvmf_tcp_free_pdu(struct nvmf_tcp_rxpdu *pdu)
{
	m_freem(pdu->m);
	pdu->m = NULL;
	pdu->hdr = NULL;
}

static int
nvmf_tcp_handle_term_req(struct nvmf_tcp_rxpdu *pdu)
{
	const struct nvme_tcp_term_req_hdr *hdr;

	hdr = (const void *)pdu->hdr;

	printf("NVMe/TCP: Received termination request: fes %#x fei %#x\n",
	    le16toh(hdr->fes), le32dec(hdr->fei));
	nvmf_tcp_free_pdu(pdu);
	return (ECONNRESET);
}

static int
nvmf_tcp_save_command_capsule(struct nvmf_tcp_qpair *qp,
    struct nvmf_tcp_rxpdu *pdu)
{
	const struct nvme_tcp_cmd *cmd;
	struct nvmf_capsule *nc;
	struct nvmf_tcp_capsule *tc;

	cmd = (const void *)pdu->hdr;

	nc = nvmf_allocate_command(&qp->qp, &cmd->ccsqe, M_WAITOK);

	tc = TCAP(nc);
	tc->rx_pdu = *pdu;

	nvmf_capsule_received(&qp->qp, nc);
	return (0);
}

static int
nvmf_tcp_save_response_capsule(struct nvmf_tcp_qpair *qp,
    struct nvmf_tcp_rxpdu *pdu)
{
	const struct nvme_tcp_rsp *rsp;
	struct nvmf_capsule *nc;
	struct nvmf_tcp_capsule *tc;

	rsp = (const void *)pdu->hdr;

	nc = nvmf_allocate_response(&qp->qp, &rsp->rccqe, M_WAITOK);

	nc->nc_sqhd_valid = true;
	tc = TCAP(nc);
	tc->rx_pdu = *pdu;

	/*
	 * Once the CQE has been received, no further transfers to the
	 * command buffer for the associated CID can occur.
	 */
	tcp_purge_command_buffer(&qp->rx_buffers, rsp->rccqe.cid, 0);
	tcp_purge_command_buffer(&qp->tx_buffers, rsp->rccqe.cid, 0);

	nvmf_capsule_received(&qp->qp, nc);
	return (0);
}

/*
 * Construct a PDU that contains an optional data payload.  This
 * includes dealing with digests and the length fields in the common
 * header.
 */
static struct mbuf *
nvmf_tcp_construct_pdu(struct nvmf_tcp_qpair *qp, void *hdr, size_t hlen,
    struct mbuf *data, uint32_t data_len)
{
	struct nvme_tcp_common_pdu_hdr *ch;
	struct mbuf *top;
	uint32_t digest, pad, pdo, plen, mlen;

	plen = hlen;
	if (qp->header_digests)
		plen += sizeof(digest);
	if (data_len != 0) {
		KASSERT(m_length(data, NULL) == data_len, ("length mismatch"));
		pdo = roundup(plen, qp->txpda);
		pad = pdo - plen;
		plen = pdo + data_len;
		if (qp->data_digests)
			plen += sizeof(digest);
		mlen = pdo;
	} else {
		KASSERT(data == NULL, ("payload mbuf with zero length"));
		pdo = 0;
		pad = 0;
		mlen = plen;
	}

	top = m_get2(mlen, M_WAITOK, MT_DATA, 0);
	top->m_len = mlen;
	ch = mtod(top, void *);
	memcpy(ch, hdr, hlen);
	ch->hlen = hlen;
	if (qp->header_digests)
		ch->flags |= NVME_TCP_CH_FLAGS_HDGSTF;
	if (qp->data_digests && data_len != 0)
		ch->flags |= NVME_TCP_CH_FLAGS_DDGSTF;
	ch->pdo = pdo;
	ch->plen = htole32(plen);

	/* HDGST */
	if (qp->header_digests) {
		digest = compute_digest(ch, hlen);
		memcpy((char *)ch + hlen, &digest, sizeof(digest));
	}

	if (pad != 0) {
		/* PAD */
		memset((char *)ch + pdo - pad, 0, pad);
	}

	if (data_len != 0) {
		/* DATA */
		top->m_next = data;

		/* DDGST */
		if (qp->data_digests) {
			digest = mbuf_crc32c(data, 0, data_len);

			/* XXX: Can't use m_append as it uses M_NOWAIT. */
			while (data->m_next != NULL)
				data = data->m_next;

			data->m_next = m_get(M_WAITOK, MT_DATA);
			data->m_next->m_len = sizeof(digest);
			memcpy(mtod(data->m_next, void *), &digest,
			    sizeof(digest));
		}
	}

	return (top);
}

/* Find the next command buffer eligible to schedule for R2T. */
static struct nvmf_tcp_command_buffer *
nvmf_tcp_next_r2t(struct nvmf_tcp_qpair *qp)
{
	struct nvmf_tcp_command_buffer *cb;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);
	MPASS(qp->active_ttags < qp->num_ttags);

	TAILQ_FOREACH(cb, &qp->rx_buffers.head, link) {
		/* NB: maxr2t is 0's based. */
		if (cb->tc->active_r2ts > qp->maxr2t)
			continue;
#ifdef INVARIANTS
		cb->tc->pending_r2ts--;
#endif
		TAILQ_REMOVE(&qp->rx_buffers.head, cb, link);
		return (cb);
	}
	return (NULL);
}

/* Allocate the next free transfer tag and assign it to cb. */
static void
nvmf_tcp_allocate_ttag(struct nvmf_tcp_qpair *qp,
    struct nvmf_tcp_command_buffer *cb)
{
	uint16_t ttag;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	ttag = qp->next_ttag;
	for (;;) {
		if (qp->open_ttags[ttag] == NULL)
			break;
		if (ttag == qp->num_ttags - 1)
			ttag = 0;
		else
			ttag++;
		MPASS(ttag != qp->next_ttag);
	}
	if (ttag == qp->num_ttags - 1)
		qp->next_ttag = 0;
	else
		qp->next_ttag = ttag + 1;

	cb->tc->active_r2ts++;
	qp->active_ttags++;
	qp->open_ttags[ttag] = cb;

	/*
	 * Don't bother byte-swapping ttag as it is just a cookie
	 * value returned by the other end as-is.
	 */
	cb->ttag = ttag;
}

/* NB: cid and ttag are both little-endian already. */
static void
tcp_send_r2t(struct nvmf_tcp_qpair *qp, uint16_t cid, uint16_t ttag,
    uint32_t data_offset, uint32_t data_len)
{
	struct nvme_tcp_r2t_hdr r2t;
	struct mbuf *m;

	memset(&r2t, 0, sizeof(r2t));
	r2t.common.pdu_type = NVME_TCP_PDU_TYPE_R2T;
	r2t.cccid = cid;
	r2t.ttag = ttag;
	r2t.r2to = htole32(data_offset);
	r2t.r2tl = htole32(data_len);

	m = nvmf_tcp_construct_pdu(qp, &r2t, sizeof(r2t), NULL, 0);
	nvmf_tcp_write_pdu(qp, m);
}

/*
 * Release a transfer tag and schedule another R2T.
 *
 * NB: This drops the rx_buffers.lock mutex.
 */
static void
nvmf_tcp_send_next_r2t(struct nvmf_tcp_qpair *qp,
    struct nvmf_tcp_command_buffer *cb)
{
	struct nvmf_tcp_command_buffer *ncb;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);
	MPASS(qp->open_ttags[cb->ttag] == cb);

	/* Release this transfer tag. */
	qp->open_ttags[cb->ttag] = NULL;
	qp->active_ttags--;
	cb->tc->active_r2ts--;

	/* Schedule another R2T. */
	ncb = nvmf_tcp_next_r2t(qp);
	if (ncb != NULL) {
		nvmf_tcp_allocate_ttag(qp, ncb);
		mtx_unlock(&qp->rx_buffers.lock);
		tcp_send_r2t(qp, ncb->cid, ncb->ttag, ncb->data_offset,
		    ncb->data_len);
	} else
		mtx_unlock(&qp->rx_buffers.lock);
}

/*
 * Copy len bytes starting at offset skip from an mbuf chain into an
 * I/O buffer at destination offset io_offset.
 */
static void
mbuf_copyto_io(struct mbuf *m, u_int skip, u_int len,
    struct nvmf_io_request *io, u_int io_offset)
{
	u_int todo;

	while (m->m_len <= skip) {
		skip -= m->m_len;
		m = m->m_next;
	}
	while (len != 0) {
		MPASS((m->m_flags & M_EXTPG) == 0);

		todo = min(m->m_len - skip, len);
		memdesc_copyback(&io->io_mem, io_offset, todo, mtodo(m, skip));
		skip = 0;
		io_offset += todo;
		len -= todo;
		m = m->m_next;
	}
}

static int
nvmf_tcp_handle_h2c_data(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	const struct nvme_tcp_h2c_data_hdr *h2c;
	struct nvmf_tcp_command_buffer *cb;
	uint32_t data_len, data_offset;
	uint16_t ttag;

	h2c = (const void *)pdu->hdr;
	if (le32toh(h2c->datal) > qp->maxh2cdata) {
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_LIMIT_EXCEEDED, 0,
		    pdu->m, pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	/*
	 * NB: Don't bother byte-swapping ttag as we don't byte-swap
	 * it when sending.
	 */
	ttag = h2c->ttag;
	if (ttag >= qp->num_ttags) {
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_h2c_data_hdr, ttag), pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	mtx_lock(&qp->rx_buffers.lock);
	cb = qp->open_ttags[ttag];
	if (cb == NULL) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_h2c_data_hdr, ttag), pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}
	MPASS(cb->ttag == ttag);

	/* For a data digest mismatch, fail the I/O request. */
	if (pdu->data_digest_mismatch) {
		nvmf_tcp_send_next_r2t(qp, cb);
		cb->error = EINTEGRITY;
		tcp_release_command_buffer(cb);
		nvmf_tcp_free_pdu(pdu);
		return (0);
	}

	data_len = le32toh(h2c->datal);
	if (data_len != pdu->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_h2c_data_hdr, datal), pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(h2c->datao);
	if (data_offset < cb->data_offset ||
	    data_offset + data_len > cb->data_offset + cb->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if (data_offset != cb->data_offset + cb->data_xfered) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if ((cb->data_xfered + data_len == cb->data_len) !=
	    ((pdu->hdr->flags & NVME_TCP_H2C_DATA_FLAGS_LAST_PDU) != 0)) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	data_offset -= cb->data_offset;
	if (cb->data_xfered == cb->data_len) {
		nvmf_tcp_send_next_r2t(qp, cb);
	} else {
		tcp_hold_command_buffer(cb);
		mtx_unlock(&qp->rx_buffers.lock);
	}

	mbuf_copyto_io(pdu->m, pdu->hdr->pdo, data_len, &cb->io, data_offset);

	tcp_release_command_buffer(cb);
	nvmf_tcp_free_pdu(pdu);
	return (0);
}

static int
nvmf_tcp_handle_c2h_data(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	const struct nvme_tcp_c2h_data_hdr *c2h;
	struct nvmf_tcp_command_buffer *cb;
	uint32_t data_len, data_offset;

	c2h = (const void *)pdu->hdr;

	mtx_lock(&qp->rx_buffers.lock);
	cb = tcp_find_command_buffer(&qp->rx_buffers, c2h->cccid, 0);
	if (cb == NULL) {
		mtx_unlock(&qp->rx_buffers.lock);
		/*
		 * XXX: Could be PDU sequence error if cccid is for a
		 * command that doesn't use a command buffer.
		 */
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_c2h_data_hdr, cccid), pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	/* For a data digest mismatch, fail the I/O request. */
	if (pdu->data_digest_mismatch) {
		cb->error = EINTEGRITY;
		tcp_remove_command_buffer(&qp->rx_buffers, cb);
		mtx_unlock(&qp->rx_buffers.lock);
		tcp_release_command_buffer(cb);
		nvmf_tcp_free_pdu(pdu);
		return (0);
	}

	data_len = le32toh(c2h->datal);
	if (data_len != pdu->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_c2h_data_hdr, datal), pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(c2h->datao);
	if (data_offset < cb->data_offset ||
	    data_offset + data_len > cb->data_offset + cb->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0,
		    pdu->m, pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if (data_offset != cb->data_offset + cb->data_xfered) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if ((cb->data_xfered + data_len == cb->data_len) !=
	    ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_LAST_PDU) != 0)) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	data_offset -= cb->data_offset;
	if (cb->data_xfered == cb->data_len)
		tcp_remove_command_buffer(&qp->rx_buffers, cb);
	else
		tcp_hold_command_buffer(cb);
	mtx_unlock(&qp->rx_buffers.lock);

	mbuf_copyto_io(pdu->m, pdu->hdr->pdo, data_len, &cb->io, data_offset);

	tcp_release_command_buffer(cb);

	if ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_SUCCESS) != 0) {
		struct nvme_completion cqe;
		struct nvmf_capsule *nc;

		memset(&cqe, 0, sizeof(cqe));
		cqe.cid = c2h->cccid;

		nc = nvmf_allocate_response(&qp->qp, &cqe, M_WAITOK);
		nc->nc_sqhd_valid = false;

		nvmf_capsule_received(&qp->qp, nc);
	}

	nvmf_tcp_free_pdu(pdu);
	return (0);
}

/* Called when m_free drops refcount to 0. */
static void
nvmf_tcp_mbuf_done(struct mbuf *m)
{
	struct nvmf_tcp_command_buffer *cb = m->m_ext.ext_arg1;

	tcp_free_command_buffer(cb);
}

static struct mbuf *
nvmf_tcp_mbuf(void *arg, int how, void *data, size_t len)
{
	struct nvmf_tcp_command_buffer *cb = arg;
	struct mbuf *m;

	m = m_get(how, MT_DATA);
	m->m_flags |= M_RDONLY;
	m_extaddref(m, data, len, &cb->refs, nvmf_tcp_mbuf_done, cb, NULL);
	m->m_len = len;
	return (m);
}

static void
nvmf_tcp_free_mext_pg(struct mbuf *m)
{
	struct nvmf_tcp_command_buffer *cb = m->m_ext.ext_arg1;

	M_ASSERTEXTPG(m);
	tcp_release_command_buffer(cb);
}

static struct mbuf *
nvmf_tcp_mext_pg(void *arg, int how)
{
	struct nvmf_tcp_command_buffer *cb = arg;
	struct mbuf *m;

	m = mb_alloc_ext_pgs(how, nvmf_tcp_free_mext_pg, M_RDONLY);
	m->m_ext.ext_arg1 = cb;
	tcp_hold_command_buffer(cb);
	return (m);
}

/*
 * Return an mbuf chain for a range of data belonging to a command
 * buffer.
 *
 * The mbuf chain uses M_EXT mbufs which hold references on the
 * command buffer so that it remains "alive" until the data has been
 * fully transmitted.  If truncate_ok is true, then the mbuf chain
 * might return a short chain to avoid gratuitously splitting up a
 * page.
 */
static struct mbuf *
nvmf_tcp_command_buffer_mbuf(struct nvmf_tcp_command_buffer *cb,
    uint32_t data_offset, uint32_t data_len, uint32_t *actual_len,
    bool can_truncate)
{
	struct mbuf *m;
	size_t len;

	m = memdesc_alloc_ext_mbufs(&cb->io.io_mem, nvmf_tcp_mbuf,
	    nvmf_tcp_mext_pg, cb, M_WAITOK, data_offset, data_len, &len,
	    can_truncate);
	if (actual_len != NULL)
		*actual_len = len;
	return (m);
}

/* NB: cid and ttag and little-endian already. */
static void
tcp_send_h2c_pdu(struct nvmf_tcp_qpair *qp, uint16_t cid, uint16_t ttag,
    uint32_t data_offset, struct mbuf *m, size_t len, bool last_pdu)
{
	struct nvme_tcp_h2c_data_hdr h2c;
	struct mbuf *top;

	memset(&h2c, 0, sizeof(h2c));
	h2c.common.pdu_type = NVME_TCP_PDU_TYPE_H2C_DATA;
	if (last_pdu)
		h2c.common.flags |= NVME_TCP_H2C_DATA_FLAGS_LAST_PDU;
	h2c.cccid = cid;
	h2c.ttag = ttag;
	h2c.datao = htole32(data_offset);
	h2c.datal = htole32(len);

	top = nvmf_tcp_construct_pdu(qp, &h2c, sizeof(h2c), m, len);
	nvmf_tcp_write_pdu(qp, top);
}

static int
nvmf_tcp_handle_r2t(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	const struct nvme_tcp_r2t_hdr *r2t;
	struct nvmf_tcp_command_buffer *cb;
	uint32_t data_len, data_offset;

	r2t = (const void *)pdu->hdr;

	mtx_lock(&qp->tx_buffers.lock);
	cb = tcp_find_command_buffer(&qp->tx_buffers, r2t->cccid, 0);
	if (cb == NULL) {
		mtx_unlock(&qp->tx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_r2t_hdr, cccid), pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(r2t->r2to);
	if (data_offset != cb->data_xfered) {
		mtx_unlock(&qp->tx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	/*
	 * XXX: The spec does not specify how to handle R2T transfers
	 * out of range of the original command.
	 */
	data_len = le32toh(r2t->r2tl);
	if (data_offset + data_len > cb->data_len) {
		mtx_unlock(&qp->tx_buffers.lock);
		nvmf_tcp_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0,
		    pdu->m, pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	if (cb->data_xfered == cb->data_len)
		tcp_remove_command_buffer(&qp->tx_buffers, cb);
	else
		tcp_hold_command_buffer(cb);
	mtx_unlock(&qp->tx_buffers.lock);

	/*
	 * Queue one or more H2C_DATA PDUs containing the requested
	 * data.
	 */
	while (data_len > 0) {
		struct mbuf *m;
		uint32_t sent, todo;

		todo = min(data_len, qp->max_tx_data);
		m = nvmf_tcp_command_buffer_mbuf(cb, data_offset, todo, &sent,
		    todo < data_len);
		tcp_send_h2c_pdu(qp, r2t->cccid, r2t->ttag, data_offset, m,
		    sent, sent == data_len);

		data_offset += sent;
		data_len -= sent;
	}

	tcp_release_command_buffer(cb);
	nvmf_tcp_free_pdu(pdu);
	return (0);
}

/*
 * A variant of m_pullup that uses M_WAITOK instead of failing.  It
 * also doesn't do anything if enough bytes are already present in the
 * first mbuf.
 */
static struct mbuf *
pullup_pdu_hdr(struct mbuf *m, int len)
{
	struct mbuf *n, *p;

	KASSERT(len <= MCLBYTES, ("%s: len too large", __func__));
	if (m->m_len >= len)
		return (m);

	n = m_get2(len, M_WAITOK, MT_DATA, 0);
	n->m_len = len;
	m_copydata(m, 0, len, mtod(n, void *));

	while (m != NULL && m->m_len <= len) {
		p = m->m_next;
		len -= m->m_len;
		m_free(m);
		m = p;
	}
	if (len > 0) {
		m->m_data += len;
		m->m_len -= len;
	}
	n->m_next = m;
	return (n);
}

static int
nvmf_tcp_dispatch_pdu(struct nvmf_tcp_qpair *qp,
    const struct nvme_tcp_common_pdu_hdr *ch, struct nvmf_tcp_rxpdu *pdu)
{
	/* Ensure the PDU header is contiguous. */
	pdu->m = pullup_pdu_hdr(pdu->m, ch->hlen);
	pdu->hdr = mtod(pdu->m, const void *);

	switch (ch->pdu_type) {
	default:
		__assert_unreachable();
		break;
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
		return (nvmf_tcp_handle_term_req(pdu));
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
		return (nvmf_tcp_save_command_capsule(qp, pdu));
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
		return (nvmf_tcp_save_response_capsule(qp, pdu));
	case NVME_TCP_PDU_TYPE_H2C_DATA:
		return (nvmf_tcp_handle_h2c_data(qp, pdu));
	case NVME_TCP_PDU_TYPE_C2H_DATA:
		return (nvmf_tcp_handle_c2h_data(qp, pdu));
	case NVME_TCP_PDU_TYPE_R2T:
		return (nvmf_tcp_handle_r2t(qp, pdu));
	}
}

static void
nvmf_tcp_receive(void *arg)
{
	struct nvmf_tcp_qpair *qp = arg;
	struct socket *so = qp->so;
	struct nvmf_tcp_rxpdu pdu;
	struct nvme_tcp_common_pdu_hdr ch;
	struct uio uio;
	struct iovec iov[1];
	struct mbuf *m, *n, *tail;
	u_int avail, needed;
	int error, flags, terror;
	bool have_header;

	m = tail = NULL;
	have_header = false;
	SOCKBUF_LOCK(&so->so_rcv);
	while (!qp->rx_shutdown) {
		/* Wait until there is enough data for the next step. */
		if (so->so_error != 0 || so->so_rerror != 0) {
			if (so->so_error != 0)
				error = so->so_error;
			else
				error = so->so_rerror;
			SOCKBUF_UNLOCK(&so->so_rcv);
		error:
			m_freem(m);
			nvmf_qpair_error(&qp->qp, error);
			SOCKBUF_LOCK(&so->so_rcv);
			while (!qp->rx_shutdown)
				cv_wait(&qp->rx_cv, SOCKBUF_MTX(&so->so_rcv));
			break;
		}
		avail = sbavail(&so->so_rcv);
		if ((so->so_rcv.sb_state & SBS_CANTRCVMORE) != 0) {
			if (!have_header && avail == 0)
				error = 0;
			else
				error = ECONNRESET;
			SOCKBUF_UNLOCK(&so->so_rcv);
			goto error;
		}
		if (avail == 0 || (!have_header && avail < sizeof(ch))) {
			cv_wait(&qp->rx_cv, SOCKBUF_MTX(&so->so_rcv));
			continue;
		}
		SOCKBUF_UNLOCK(&so->so_rcv);

		if (!have_header) {
			KASSERT(m == NULL, ("%s: m != NULL but no header",
			    __func__));
			memset(&uio, 0, sizeof(uio));
			iov[0].iov_base = &ch;
			iov[0].iov_len = sizeof(ch);
			uio.uio_iov = iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = sizeof(ch);
			uio.uio_segflg = UIO_SYSSPACE;
			uio.uio_rw = UIO_READ;
			flags = MSG_DONTWAIT | MSG_PEEK;

			error = soreceive(so, NULL, &uio, NULL, NULL, &flags);
			if (error != 0)
				goto error;
			KASSERT(uio.uio_resid == 0, ("%s: short CH read",
			    __func__));

			have_header = true;
			needed = le32toh(ch.plen);

			/*
			 * Malformed PDUs will be reported as errors
			 * by nvmf_tcp_validate_pdu.  Just pass along
			 * garbage headers if the lengths mismatch.
			 */
			if (needed < sizeof(ch) || ch.hlen > needed)
				needed = sizeof(ch);

			memset(&uio, 0, sizeof(uio));
			uio.uio_resid = needed;
		}

		flags = MSG_DONTWAIT;
		error = soreceive(so, NULL, &uio, &n, NULL, &flags);
		if (error != 0)
			goto error;

		if (m == NULL)
			m = n;
		else
			tail->m_next = n;

		if (uio.uio_resid != 0) {
			tail = n;
			while (tail->m_next != NULL)
				tail = tail->m_next;

			SOCKBUF_LOCK(&so->so_rcv);
			continue;
		}
#ifdef INVARIANTS
		tail = NULL;
#endif

		pdu.m = m;
		m = NULL;
		pdu.hdr = &ch;
		error = nvmf_tcp_validate_pdu(qp, &pdu);
		if (error != 0)
			m_freem(pdu.m);
		else
			error = nvmf_tcp_dispatch_pdu(qp, &ch, &pdu);
		if (error != 0) {
			/*
			 * If we received a termination request, close
			 * the connection immediately.
			 */
			if (error == ECONNRESET)
				goto error;

			/*
			 * Wait for up to 30 seconds for the socket to
			 * be closed by the other end.
			 */
			SOCKBUF_LOCK(&so->so_rcv);
			if ((so->so_rcv.sb_state & SBS_CANTRCVMORE) == 0) {
				terror = cv_timedwait(&qp->rx_cv,
				    SOCKBUF_MTX(&so->so_rcv), 30 * hz);
				if (terror == ETIMEDOUT)
					printf("NVMe/TCP: Timed out after sending terminate request\n");
			}
			SOCKBUF_UNLOCK(&so->so_rcv);
			goto error;
		}

		have_header = false;
		SOCKBUF_LOCK(&so->so_rcv);
	}
	SOCKBUF_UNLOCK(&so->so_rcv);
	kthread_exit();
}

static struct mbuf *
tcp_command_pdu(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_capsule *tc)
{
	struct nvmf_capsule *nc = &tc->nc;
	struct nvmf_tcp_command_buffer *cb;
	struct nvme_sgl_descriptor *sgl;
	struct nvme_tcp_cmd cmd;
	struct mbuf *top, *m;
	bool use_icd;

	use_icd = false;
	cb = NULL;
	m = NULL;

	if (nc->nc_data.io_len != 0) {
		cb = tcp_alloc_command_buffer(qp, &nc->nc_data, 0,
		    nc->nc_data.io_len, nc->nc_sqe.cid);

		if (nc->nc_send_data && nc->nc_data.io_len <= qp->max_icd) {
			use_icd = true;
			m = nvmf_tcp_command_buffer_mbuf(cb, 0,
			    nc->nc_data.io_len, NULL, false);
			cb->data_xfered = nc->nc_data.io_len;
			tcp_release_command_buffer(cb);
		} else if (nc->nc_send_data) {
			mtx_lock(&qp->tx_buffers.lock);
			tcp_add_command_buffer(&qp->tx_buffers, cb);
			mtx_unlock(&qp->tx_buffers.lock);
		} else {
			mtx_lock(&qp->rx_buffers.lock);
			tcp_add_command_buffer(&qp->rx_buffers, cb);
			mtx_unlock(&qp->rx_buffers.lock);
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.pdu_type = NVME_TCP_PDU_TYPE_CAPSULE_CMD;
	cmd.ccsqe = nc->nc_sqe;

	/* Populate SGL in SQE. */
	sgl = &cmd.ccsqe.sgl;
	memset(sgl, 0, sizeof(*sgl));
	sgl->address = 0;
	sgl->length = htole32(nc->nc_data.io_len);
	if (use_icd) {
		/* Use in-capsule data. */
		sgl->type = NVME_SGL_TYPE_ICD;
	} else {
		/* Use a command buffer. */
		sgl->type = NVME_SGL_TYPE_COMMAND_BUFFER;
	}

	top = nvmf_tcp_construct_pdu(qp, &cmd, sizeof(cmd), m, m != NULL ?
	    nc->nc_data.io_len : 0);
	return (top);
}

static struct mbuf *
tcp_response_pdu(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_capsule *tc)
{
	struct nvmf_capsule *nc = &tc->nc;
	struct nvme_tcp_rsp rsp;

	memset(&rsp, 0, sizeof(rsp));
	rsp.common.pdu_type = NVME_TCP_PDU_TYPE_CAPSULE_RESP;
	rsp.rccqe = nc->nc_cqe;

	return (nvmf_tcp_construct_pdu(qp, &rsp, sizeof(rsp), NULL, 0));
}

static struct mbuf *
capsule_to_pdu(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_capsule *tc)
{
	if (tc->nc.nc_qe_len == sizeof(struct nvme_command))
		return (tcp_command_pdu(qp, tc));
	else
		return (tcp_response_pdu(qp, tc));
}

static void
nvmf_tcp_send(void *arg)
{
	struct nvmf_tcp_qpair *qp = arg;
	struct nvmf_tcp_capsule *tc;
	struct socket *so = qp->so;
	struct mbuf *m, *n, *p;
	u_long space, tosend;
	int error;

	m = NULL;
	SOCKBUF_LOCK(&so->so_snd);
	while (!qp->tx_shutdown) {
		if (so->so_error != 0) {
			error = so->so_error;
			SOCKBUF_UNLOCK(&so->so_snd);
		error:
			m_freem(m);
			nvmf_qpair_error(&qp->qp, error);
			SOCKBUF_LOCK(&so->so_snd);
			while (!qp->tx_shutdown)
				cv_wait(&qp->tx_cv, SOCKBUF_MTX(&so->so_snd));
			break;
		}

		if (m == NULL) {
			/* Next PDU to send. */
			m = mbufq_dequeue(&qp->tx_pdus);
		}
		if (m == NULL) {
			if (STAILQ_EMPTY(&qp->tx_capsules)) {
				cv_wait(&qp->tx_cv, SOCKBUF_MTX(&so->so_snd));
				continue;
			}

			/* Convert a capsule into a PDU. */
			tc = STAILQ_FIRST(&qp->tx_capsules);
			STAILQ_REMOVE_HEAD(&qp->tx_capsules, link);
			SOCKBUF_UNLOCK(&so->so_snd);

			n = capsule_to_pdu(qp, tc);
			tcp_release_capsule(tc);

			SOCKBUF_LOCK(&so->so_snd);
			mbufq_enqueue(&qp->tx_pdus, n);
			continue;
		}

		/*
		 * Wait until there is enough room to send some data.
		 * If the socket buffer is empty, always send at least
		 * something.
		 */
		space = sbspace(&so->so_snd);
		if (space < m->m_len && sbused(&so->so_snd) != 0) {
			cv_wait(&qp->tx_cv, SOCKBUF_MTX(&so->so_snd));
			continue;
		}
		SOCKBUF_UNLOCK(&so->so_snd);

		/*
		 * If 'm' is too big, then the socket buffer must be
		 * empty.  Split 'm' to make at least some forward
		 * progress.
		 *
		 * Otherwise, chain up as many pending mbufs from 'm'
		 * that will fit.
		 */
		if (m->m_len > space) {
			n = m_split(m, space, M_WAITOK);
		} else {
			tosend = m->m_len;
			n = m->m_next;
			p = m;
			while (n != NULL && tosend + n->m_len <= space) {
				tosend += n->m_len;
				p = n;
				n = n->m_next;
			}
			KASSERT(p->m_next == n, ("%s: p not before n",
			    __func__));
			p->m_next = NULL;

			KASSERT(m_length(m, NULL) == tosend,
			    ("%s: length mismatch", __func__));
		}
		error = sosend(so, NULL, NULL, m, NULL, MSG_DONTWAIT, NULL);
		if (error != 0) {
			m = NULL;
			m_freem(n);
			goto error;
		}
		m = n;
		SOCKBUF_LOCK(&so->so_snd);
	}
	SOCKBUF_UNLOCK(&so->so_snd);
	kthread_exit();
}

static int
nvmf_soupcall_receive(struct socket *so, void *arg, int waitflag)
{
	struct nvmf_tcp_qpair *qp = arg;

	if (soreadable(so))
		cv_signal(&qp->rx_cv);
	return (SU_OK);
}

static int
nvmf_soupcall_send(struct socket *so, void *arg, int waitflag)
{
	struct nvmf_tcp_qpair *qp = arg;

	if (sowriteable(so))
		cv_signal(&qp->tx_cv);
	return (SU_OK);
}

static struct nvmf_qpair *
tcp_allocate_qpair(bool controller, const nvlist_t *nvl)
{
	struct nvmf_tcp_qpair *qp;
	struct socket *so;
	struct file *fp;
	cap_rights_t rights;
	int error;

	if (!nvlist_exists_number(nvl, "fd") ||
	    !nvlist_exists_number(nvl, "rxpda") ||
	    !nvlist_exists_number(nvl, "txpda") ||
	    !nvlist_exists_bool(nvl, "header_digests") ||
	    !nvlist_exists_bool(nvl, "data_digests") ||
	    !nvlist_exists_number(nvl, "maxr2t") ||
	    !nvlist_exists_number(nvl, "maxh2cdata") ||
	    !nvlist_exists_number(nvl, "max_icd"))
		return (NULL);

	error = fget(curthread, nvlist_get_number(nvl, "fd"),
	    cap_rights_init_one(&rights, CAP_SOCK_CLIENT), &fp);
	if (error != 0)
		return (NULL);
	if (fp->f_type != DTYPE_SOCKET) {
		fdrop(fp, curthread);
		return (NULL);
	}
	so = fp->f_data;
	if (so->so_type != SOCK_STREAM ||
	    so->so_proto->pr_protocol != IPPROTO_TCP) {
		fdrop(fp, curthread);
		return (NULL);
	}

	/* Claim socket from file descriptor. */
	fp->f_ops = &badfileops;
	fp->f_data = NULL;
	fdrop(fp, curthread);

	qp = malloc(sizeof(*qp), M_NVMF_TCP, M_WAITOK | M_ZERO);
	qp->so = so;
	refcount_init(&qp->refs, 1);
	qp->txpda = nvlist_get_number(nvl, "txpda");
	qp->rxpda = nvlist_get_number(nvl, "rxpda");
	qp->header_digests = nvlist_get_bool(nvl, "header_digests");
	qp->data_digests = nvlist_get_bool(nvl, "data_digests");
	qp->maxr2t = nvlist_get_number(nvl, "maxr2t");
	if (controller)
		qp->maxh2cdata = nvlist_get_number(nvl, "maxh2cdata");
	qp->max_tx_data = tcp_max_transmit_data;
	if (!controller) {
		qp->max_tx_data = min(qp->max_tx_data,
		    nvlist_get_number(nvl, "maxh2cdata"));
		qp->max_icd = nvlist_get_number(nvl, "max_icd");
	}

	if (controller) {
		/* Use the SUCCESS flag if SQ flow control is disabled. */
		qp->send_success = !nvlist_get_bool(nvl, "sq_flow_control");

		/* NB: maxr2t is 0's based. */
		qp->num_ttags = MIN((u_int)UINT16_MAX + 1,
		    nvlist_get_number(nvl, "qsize") *
		    ((uint64_t)qp->maxr2t + 1));
		qp->open_ttags = mallocarray(qp->num_ttags,
		    sizeof(*qp->open_ttags), M_NVMF_TCP, M_WAITOK | M_ZERO);
	}

	TAILQ_INIT(&qp->rx_buffers.head);
	TAILQ_INIT(&qp->tx_buffers.head);
	mtx_init(&qp->rx_buffers.lock, "nvmf/tcp rx buffers", NULL, MTX_DEF);
	mtx_init(&qp->tx_buffers.lock, "nvmf/tcp tx buffers", NULL, MTX_DEF);

	cv_init(&qp->rx_cv, "-");
	cv_init(&qp->tx_cv, "-");
	mbufq_init(&qp->tx_pdus, 0);
	STAILQ_INIT(&qp->tx_capsules);

	/* Register socket upcalls. */
	SOCKBUF_LOCK(&so->so_rcv);
	soupcall_set(so, SO_RCV, nvmf_soupcall_receive, qp);
	SOCKBUF_UNLOCK(&so->so_rcv);
	SOCKBUF_LOCK(&so->so_snd);
	soupcall_set(so, SO_SND, nvmf_soupcall_send, qp);
	SOCKBUF_UNLOCK(&so->so_snd);

	/* Spin up kthreads. */
	error = kthread_add(nvmf_tcp_receive, qp, NULL, &qp->rx_thread, 0, 0,
	    "nvmef tcp rx");
	if (error != 0) {
		tcp_free_qpair(&qp->qp);
		return (NULL);
	}
	error = kthread_add(nvmf_tcp_send, qp, NULL, &qp->tx_thread, 0, 0,
	    "nvmef tcp tx");
	if (error != 0) {
		tcp_free_qpair(&qp->qp);
		return (NULL);
	}

	return (&qp->qp);
}

static void
tcp_release_qpair(struct nvmf_tcp_qpair *qp)
{
	if (refcount_release(&qp->refs))
		free(qp, M_NVMF_TCP);
}

static void
tcp_free_qpair(struct nvmf_qpair *nq)
{
	struct nvmf_tcp_qpair *qp = TQP(nq);
	struct nvmf_tcp_command_buffer *ncb, *cb;
	struct nvmf_tcp_capsule *ntc, *tc;
	struct socket *so = qp->so;

	/* Shut down kthreads and clear upcalls */
	SOCKBUF_LOCK(&so->so_snd);
	qp->tx_shutdown = true;
	if (qp->tx_thread != NULL) {
		cv_signal(&qp->tx_cv);
		mtx_sleep(qp->tx_thread, SOCKBUF_MTX(&so->so_snd), 0,
		    "nvtcptx", 0);
	}
	soupcall_clear(so, SO_SND);
	SOCKBUF_UNLOCK(&so->so_snd);

	SOCKBUF_LOCK(&so->so_rcv);
	qp->rx_shutdown = true;
	if (qp->rx_thread != NULL) {
		cv_signal(&qp->rx_cv);
		mtx_sleep(qp->rx_thread, SOCKBUF_MTX(&so->so_rcv), 0,
		    "nvtcprx", 0);
	}
	soupcall_clear(so, SO_RCV);
	SOCKBUF_UNLOCK(&so->so_rcv);

	STAILQ_FOREACH_SAFE(tc, &qp->tx_capsules, link, ntc) {
		nvmf_abort_capsule_data(&tc->nc, ECONNABORTED);
		tcp_release_capsule(tc);
	}
	mbufq_drain(&qp->tx_pdus);

	cv_destroy(&qp->tx_cv);
	cv_destroy(&qp->rx_cv);

	if (qp->open_ttags != NULL) {
		for (u_int i = 0; i < qp->num_ttags; i++) {
			cb = qp->open_ttags[i];
			if (cb != NULL) {
				cb->tc->active_r2ts--;
				cb->error = ECONNABORTED;
				tcp_release_command_buffer(cb);
			}
		}
		free(qp->open_ttags, M_NVMF_TCP);
	}

	mtx_lock(&qp->rx_buffers.lock);
	TAILQ_FOREACH_SAFE(cb, &qp->rx_buffers.head, link, ncb) {
		tcp_remove_command_buffer(&qp->rx_buffers, cb);
		mtx_unlock(&qp->rx_buffers.lock);
#ifdef INVARIANTS
		if (cb->tc != NULL)
			cb->tc->pending_r2ts--;
#endif
		cb->error = ECONNABORTED;
		tcp_release_command_buffer(cb);
		mtx_lock(&qp->rx_buffers.lock);
	}
	mtx_destroy(&qp->rx_buffers.lock);

	mtx_lock(&qp->tx_buffers.lock);
	TAILQ_FOREACH_SAFE(cb, &qp->tx_buffers.head, link, ncb) {
		tcp_remove_command_buffer(&qp->tx_buffers, cb);
		mtx_unlock(&qp->tx_buffers.lock);
		cb->error = ECONNABORTED;
		tcp_release_command_buffer(cb);
		mtx_lock(&qp->tx_buffers.lock);
	}
	mtx_destroy(&qp->tx_buffers.lock);

	soclose(so);

	tcp_release_qpair(qp);
}

static struct nvmf_capsule *
tcp_allocate_capsule(struct nvmf_qpair *nq, int how)
{
	struct nvmf_tcp_qpair *qp = TQP(nq);
	struct nvmf_tcp_capsule *tc;

	tc = malloc(sizeof(*tc), M_NVMF_TCP, how | M_ZERO);
	if (tc == NULL)
		return (NULL);
	refcount_init(&tc->refs, 1);
	refcount_acquire(&qp->refs);
	return (&tc->nc);
}

static void
tcp_release_capsule(struct nvmf_tcp_capsule *tc)
{
	struct nvmf_tcp_qpair *qp = TQP(tc->nc.nc_qpair);

	if (!refcount_release(&tc->refs))
		return;

	MPASS(tc->active_r2ts == 0);
	MPASS(tc->pending_r2ts == 0);

	nvmf_tcp_free_pdu(&tc->rx_pdu);
	free(tc, M_NVMF_TCP);
	tcp_release_qpair(qp);
}

static void
tcp_free_capsule(struct nvmf_capsule *nc)
{
	struct nvmf_tcp_capsule *tc = TCAP(nc);

	tcp_release_capsule(tc);
}

static int
tcp_transmit_capsule(struct nvmf_capsule *nc)
{
	struct nvmf_tcp_qpair *qp = TQP(nc->nc_qpair);
	struct nvmf_tcp_capsule *tc = TCAP(nc);
	struct socket *so = qp->so;

	refcount_acquire(&tc->refs);
	SOCKBUF_LOCK(&so->so_snd);
	STAILQ_INSERT_TAIL(&qp->tx_capsules, tc, link);
	if (sowriteable(so))
		cv_signal(&qp->tx_cv);
	SOCKBUF_UNLOCK(&so->so_snd);
	return (0);
}

static uint8_t
tcp_validate_command_capsule(struct nvmf_capsule *nc)
{
	struct nvmf_tcp_capsule *tc = TCAP(nc);
	struct nvme_sgl_descriptor *sgl;

	KASSERT(tc->rx_pdu.hdr != NULL, ("capsule wasn't received"));

	sgl = &nc->nc_sqe.sgl;
	switch (sgl->type) {
	case NVME_SGL_TYPE_ICD:
		if (tc->rx_pdu.data_len != le32toh(sgl->length)) {
			printf("NVMe/TCP: Command Capsule with mismatched ICD length\n");
			return (NVME_SC_DATA_SGL_LENGTH_INVALID);
		}
		break;
	case NVME_SGL_TYPE_COMMAND_BUFFER:
		if (tc->rx_pdu.data_len != 0) {
			printf("NVMe/TCP: Command Buffer SGL with ICD\n");
			return (NVME_SC_INVALID_FIELD);
		}
		break;
	default:
		printf("NVMe/TCP: Invalid SGL type in Command Capsule\n");
		return (NVME_SC_SGL_DESCRIPTOR_TYPE_INVALID);
	}

	if (sgl->address != 0) {
		printf("NVMe/TCP: Invalid SGL offset in Command Capsule\n");
		return (NVME_SC_SGL_OFFSET_INVALID);
	}

	return (NVME_SC_SUCCESS);
}

static size_t
tcp_capsule_data_len(const struct nvmf_capsule *nc)
{
	MPASS(nc->nc_qe_len == sizeof(struct nvme_command));
	return (le32toh(nc->nc_sqe.sgl.length));
}

static void
tcp_receive_r2t_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct nvmf_io_request *io)
{
	struct nvmf_tcp_qpair *qp = TQP(nc->nc_qpair);
	struct nvmf_tcp_capsule *tc = TCAP(nc);
	struct nvmf_tcp_command_buffer *cb;

	cb = tcp_alloc_command_buffer(qp, io, data_offset, io->io_len,
	    nc->nc_sqe.cid);

	cb->tc = tc;
	refcount_acquire(&tc->refs);

	/*
	 * If this command has too many active R2Ts or there are no
	 * available transfer tags, queue the request for later.
	 *
	 * NB: maxr2t is 0's based.
	 */
	mtx_lock(&qp->rx_buffers.lock);
	if (tc->active_r2ts > qp->maxr2t || qp->active_ttags == qp->num_ttags) {
#ifdef INVARIANTS
		tc->pending_r2ts++;
#endif
		TAILQ_INSERT_TAIL(&qp->rx_buffers.head, cb, link);
		mtx_unlock(&qp->rx_buffers.lock);
		return;
	}

	nvmf_tcp_allocate_ttag(qp, cb);
	mtx_unlock(&qp->rx_buffers.lock);

	tcp_send_r2t(qp, nc->nc_sqe.cid, cb->ttag, data_offset, io->io_len);
}

static void
tcp_receive_icd_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct nvmf_io_request *io)
{
	struct nvmf_tcp_capsule *tc = TCAP(nc);

	mbuf_copyto_io(tc->rx_pdu.m, tc->rx_pdu.hdr->pdo + data_offset,
	    io->io_len, io, 0);
	nvmf_complete_io_request(io, io->io_len, 0);
}

static int
tcp_receive_controller_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct nvmf_io_request *io)
{
	struct nvme_sgl_descriptor *sgl;
	size_t data_len;

	if (nc->nc_qe_len != sizeof(struct nvme_command) ||
	    !nc->nc_qpair->nq_controller)
		return (EINVAL);

	sgl = &nc->nc_sqe.sgl;
	data_len = le32toh(sgl->length);
	if (data_offset + io->io_len > data_len)
		return (EFBIG);

	if (sgl->type == NVME_SGL_TYPE_ICD)
		tcp_receive_icd_data(nc, data_offset, io);
	else
		tcp_receive_r2t_data(nc, data_offset, io);
	return (0);
}

/* NB: cid is little-endian already. */
static void
tcp_send_c2h_pdu(struct nvmf_tcp_qpair *qp, uint16_t cid, uint32_t data_offset,
    struct mbuf *m, size_t len, bool last_pdu, bool success)
{
	struct nvme_tcp_c2h_data_hdr c2h;
	struct mbuf *top;

	memset(&c2h, 0, sizeof(c2h));
	c2h.common.pdu_type = NVME_TCP_PDU_TYPE_C2H_DATA;
	if (last_pdu)
		c2h.common.flags |= NVME_TCP_C2H_DATA_FLAGS_LAST_PDU;
	if (success)
		c2h.common.flags |= NVME_TCP_C2H_DATA_FLAGS_SUCCESS;
	c2h.cccid = cid;
	c2h.datao = htole32(data_offset);
	c2h.datal = htole32(len);

	top = nvmf_tcp_construct_pdu(qp, &c2h, sizeof(c2h), m, len);
	nvmf_tcp_write_pdu(qp, top);
}

static u_int
tcp_send_controller_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct mbuf *m, size_t len)
{
	struct nvmf_tcp_qpair *qp = TQP(nc->nc_qpair);
	struct nvme_sgl_descriptor *sgl;
	uint32_t data_len;
	bool last_pdu, last_xfer;

	if (nc->nc_qe_len != sizeof(struct nvme_command) ||
	    !qp->qp.nq_controller) {
		m_freem(m);
		return (NVME_SC_INVALID_FIELD);
	}

	sgl = &nc->nc_sqe.sgl;
	data_len = le32toh(sgl->length);
	if (data_offset + len > data_len) {
		m_freem(m);
		return (NVME_SC_INVALID_FIELD);
	}
	last_xfer = (data_offset + len == data_len);

	if (sgl->type != NVME_SGL_TYPE_COMMAND_BUFFER) {
		m_freem(m);
		return (NVME_SC_INVALID_FIELD);
	}

	KASSERT(data_offset == TCAP(nc)->tx_data_offset,
	    ("%s: starting data_offset %u doesn't match end of previous xfer %u",
	    __func__, data_offset, TCAP(nc)->tx_data_offset));

	/* Queue one more C2H_DATA PDUs containing the data from 'm'. */
	while (m != NULL) {
		struct mbuf *n;
		uint32_t todo;

		if (m->m_len > qp->max_tx_data) {
			n = m_split(m, qp->max_tx_data, M_WAITOK);
			todo = m->m_len;
		} else {
			struct mbuf *p;

			todo = m->m_len;
			p = m;
			n = p->m_next;
			while (n != NULL) {
				if (todo + n->m_len > qp->max_tx_data) {
					p->m_next = NULL;
					break;
				}
				todo += n->m_len;
				p = n;
				n = p->m_next;
			}
			MPASS(m_length(m, NULL) == todo);
		}

		last_pdu = (n == NULL && last_xfer);
		tcp_send_c2h_pdu(qp, nc->nc_sqe.cid, data_offset, m, todo,
		    last_pdu, last_pdu && qp->send_success);

		data_offset += todo;
		data_len -= todo;
		m = n;
	}
	MPASS(data_len == 0);

#ifdef INVARIANTS
	TCAP(nc)->tx_data_offset = data_offset;
#endif
	if (!last_xfer)
		return (NVMF_MORE);
	else if (qp->send_success)
		return (NVMF_SUCCESS_SENT);
	else
		return (NVME_SC_SUCCESS);
}

struct nvmf_transport_ops tcp_ops = {
	.allocate_qpair = tcp_allocate_qpair,
	.free_qpair = tcp_free_qpair,
	.allocate_capsule = tcp_allocate_capsule,
	.free_capsule = tcp_free_capsule,
	.transmit_capsule = tcp_transmit_capsule,
	.validate_command_capsule = tcp_validate_command_capsule,
	.capsule_data_len = tcp_capsule_data_len,
	.receive_controller_data = tcp_receive_controller_data,
	.send_controller_data = tcp_send_controller_data,
	.trtype = NVMF_TRTYPE_TCP,
	.priority = 0,
};

NVMF_TRANSPORT(tcp, tcp_ops);
