/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/endian.h>
#include <sys/gsb_crc32.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libnvmf.h"
#include "internal.h"
#include "nvmf_tcp.h"

struct nvmf_tcp_qpair;

struct nvmf_tcp_command_buffer {
	struct nvmf_tcp_qpair *qp;

	void	*data;
	size_t	data_len;
	size_t	data_xfered;
	uint32_t data_offset;

	uint16_t cid;
	uint16_t ttag;

	LIST_ENTRY(nvmf_tcp_command_buffer) link;
};

LIST_HEAD(nvmf_tcp_command_buffer_list, nvmf_tcp_command_buffer);

struct nvmf_tcp_association {
	struct nvmf_association na;

	uint32_t ioccsz;
};

struct nvmf_tcp_rxpdu {
	struct nvme_tcp_common_pdu_hdr *hdr;
	uint32_t data_len;
};

struct nvmf_tcp_capsule {
	struct nvmf_capsule nc;

	struct nvmf_tcp_rxpdu rx_pdu;
	struct nvmf_tcp_command_buffer *cb;

	TAILQ_ENTRY(nvmf_tcp_capsule) link;
};

struct nvmf_tcp_qpair {
	struct nvmf_qpair qp;
	int s;

	uint8_t	txpda;
	uint8_t rxpda;
	bool header_digests;
	bool data_digests;
	uint32_t maxr2t;
	uint32_t maxh2cdata;
	uint32_t max_icd;	/* Host only */
	uint16_t next_ttag;	/* Controller only */

	struct nvmf_tcp_command_buffer_list tx_buffers;
	struct nvmf_tcp_command_buffer_list rx_buffers;
	TAILQ_HEAD(, nvmf_tcp_capsule) rx_capsules;
};

#define	TASSOC(nc)	((struct nvmf_tcp_association *)(na))
#define	TCAP(nc)	((struct nvmf_tcp_capsule *)(nc))
#define	CTCAP(nc)	((const struct nvmf_tcp_capsule *)(nc))
#define	TQP(qp)		((struct nvmf_tcp_qpair *)(qp))

static const char zero_padding[NVME_TCP_PDU_PDO_MAX_OFFSET];

static uint32_t
compute_digest(const void *buf, size_t len)
{
	return (calculate_crc32c(0xffffffff, buf, len) ^ 0xffffffff);
}

static struct nvmf_tcp_command_buffer *
tcp_alloc_command_buffer(struct nvmf_tcp_qpair *qp, void *data,
    uint32_t data_offset, size_t data_len, uint16_t cid, uint16_t ttag,
    bool receive)
{
	struct nvmf_tcp_command_buffer *cb;

	cb = malloc(sizeof(*cb));
	cb->qp = qp;
	cb->data = data;
	cb->data_offset = data_offset;
	cb->data_len = data_len;
	cb->data_xfered = 0;
	cb->cid = cid;
	cb->ttag = ttag;

	if (receive)
		LIST_INSERT_HEAD(&qp->rx_buffers, cb, link);
	else
		LIST_INSERT_HEAD(&qp->tx_buffers, cb, link);
	return (cb);
}

static struct nvmf_tcp_command_buffer *
tcp_find_command_buffer(struct nvmf_tcp_qpair *qp, uint16_t cid, uint16_t ttag,
    bool receive)
{
	struct nvmf_tcp_command_buffer_list *list;
	struct nvmf_tcp_command_buffer *cb;

	list = receive ? &qp->rx_buffers : &qp->tx_buffers;
	LIST_FOREACH(cb, list, link) {
		if (cb->cid == cid && cb->ttag == ttag)
			return (cb);
	}
	return (NULL);
}

static void
tcp_purge_command_buffer(struct nvmf_tcp_qpair *qp, uint16_t cid, uint16_t ttag,
    bool receive)
{
	struct nvmf_tcp_command_buffer *cb;

	cb = tcp_find_command_buffer(qp, cid, ttag, receive);
	if (cb != NULL)
		LIST_REMOVE(cb, link);
}

static void
tcp_free_command_buffer(struct nvmf_tcp_command_buffer *cb)
{
	LIST_REMOVE(cb, link);
	free(cb);
}

static int
nvmf_tcp_write_pdu(struct nvmf_tcp_qpair *qp, const void *pdu, size_t len)
{
	ssize_t nwritten;
	const char *cp;

	cp = pdu;
	while (len != 0) {
		nwritten = write(qp->s, cp, len);
		if (nwritten < 0)
			return (errno);
		len -= nwritten;
		cp += nwritten;
	}
	return (0);
}

static int
nvmf_tcp_write_pdu_iov(struct nvmf_tcp_qpair *qp, struct iovec *iov,
    u_int iovcnt, size_t len)
{
	ssize_t nwritten;

	for (;;) {
		nwritten = writev(qp->s, iov, iovcnt);
		if (nwritten < 0)
			return (errno);

		len -= nwritten;
		if (len == 0)
			return (0);

		while (iov->iov_len <= (size_t)nwritten) {
			nwritten -= iov->iov_len;
			iovcnt--;
			iov++;
		}

		iov->iov_base = (char *)iov->iov_base + nwritten;
		iov->iov_len -= nwritten;
	}
}

static void
nvmf_tcp_report_error(struct nvmf_association *na, struct nvmf_tcp_qpair *qp,
    uint16_t fes, uint32_t fei, const void *rx_pdu, size_t pdu_len, u_int hlen)
{
	struct nvme_tcp_term_req_hdr hdr;
	struct iovec iov[2];

	if (hlen != 0) {
		if (hlen > NVME_TCP_TERM_REQ_ERROR_DATA_MAX_SIZE)
			hlen = NVME_TCP_TERM_REQ_ERROR_DATA_MAX_SIZE;
		if (hlen > pdu_len)
			hlen = pdu_len;
	}

	memset(&hdr, 0, sizeof(hdr));
	hdr.common.pdu_type = na->na_controller ?
	    NVME_TCP_PDU_TYPE_C2H_TERM_REQ : NVME_TCP_PDU_TYPE_H2C_TERM_REQ;
	hdr.common.hlen = sizeof(hdr);
	hdr.common.plen = sizeof(hdr) + hlen;
	hdr.fes = htole16(fes);
	le32enc(hdr.fei, fei);
	iov[0].iov_base = &hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = __DECONST(void *, rx_pdu);
	iov[1].iov_len = hlen;

	(void)nvmf_tcp_write_pdu_iov(qp, iov, nitems(iov), sizeof(hdr) + hlen);
	close(qp->s);
	qp->s = -1;
}

static int
nvmf_tcp_validate_pdu(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu,
    size_t pdu_len)
{
	const struct nvme_tcp_common_pdu_hdr *ch;
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

	error = nvmf_tcp_validate_pdu_header(ch,
	    qp->qp.nq_association->na_controller, qp->header_digests,
	    qp->data_digests, qp->rxpda, &data_len, &fes, &fei);
	if (error != 0) {
		if (error == ECONNRESET) {
			close(qp->s);
			qp->s = -1;
		} else {
			nvmf_tcp_report_error(qp->qp.nq_association, qp,
			    fes, fei, ch, pdu_len, hlen);
		}
		return (error);
	}

	/* Check header digest if present. */
	if ((ch->flags & NVME_TCP_CH_FLAGS_HDGSTF) != 0) {
		digest = compute_digest(ch, ch->hlen);
		memcpy(&rx_digest, (const char *)ch + ch->hlen,
		    sizeof(rx_digest));
		if (digest != rx_digest) {
			printf("NVMe/TCP: Header digest mismatch\n");
			nvmf_tcp_report_error(qp->qp.nq_association, qp,
			    NVME_TCP_TERM_REQ_FES_HDGST_ERROR, rx_digest, ch,
			    pdu_len, hlen);
			return (EBADMSG);
		}
	}

	/* Check data digest if present. */
	if ((ch->flags & NVME_TCP_CH_FLAGS_DDGSTF) != 0) {
		digest = compute_digest((const char *)ch + ch->pdo, data_len);
		memcpy(&rx_digest, (const char *)ch + plen - sizeof(rx_digest),
		    sizeof(rx_digest));
		if (digest != rx_digest) {
			printf("NVMe/TCP: Data digest mismatch\n");
			return (EBADMSG);
		}
	}

	pdu->data_len = data_len;
	return (0);
}

/*
 * Read data from a socket, retrying until the data has been fully
 * read or an error occurs.
 */
static int
nvmf_tcp_read_buffer(int s, void *buf, size_t len)
{
	ssize_t nread;
	char *cp;

	cp = buf;
	while (len != 0) {
		nread = read(s, cp, len);
		if (nread < 0)
			return (errno);
		if (nread == 0)
			return (ECONNRESET);
		len -= nread;
		cp += nread;
	}
	return (0);
}

static int
nvmf_tcp_read_pdu(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	struct nvme_tcp_common_pdu_hdr ch;
	uint32_t plen;
	int error;

	memset(pdu, 0, sizeof(*pdu));
	error = nvmf_tcp_read_buffer(qp->s, &ch, sizeof(ch));
	if (error != 0)
		return (error);

	plen = le32toh(ch.plen);

	/*
	 * Validate a header with garbage lengths to trigger
	 * an error message without reading more.
	 */
	if (plen < sizeof(ch) || ch.hlen > plen) {
		pdu->hdr = &ch;
		error = nvmf_tcp_validate_pdu(qp, pdu, sizeof(ch));
		pdu->hdr = NULL;
		assert(error != 0);
		return (error);
	}

	/* Read the rest of the PDU. */
	pdu->hdr = malloc(plen);
	memcpy(pdu->hdr, &ch, sizeof(ch));
	error = nvmf_tcp_read_buffer(qp->s, pdu->hdr + 1, plen - sizeof(ch));
	if (error != 0)
		return (error);
	error = nvmf_tcp_validate_pdu(qp, pdu, plen);
	if (error != 0) {
		free(pdu->hdr);
		pdu->hdr = NULL;
	}
	return (error);
}

static void
nvmf_tcp_free_pdu(struct nvmf_tcp_rxpdu *pdu)
{
	free(pdu->hdr);
	pdu->hdr = NULL;
}

static int
nvmf_tcp_handle_term_req(struct nvmf_tcp_rxpdu *pdu)
{
	struct nvme_tcp_term_req_hdr *hdr;

	hdr = (void *)pdu->hdr;

	printf("NVMe/TCP: Received termination request: fes %#x fei %#x\n",
	    le16toh(hdr->fes), le32dec(hdr->fei));
	nvmf_tcp_free_pdu(pdu);
	return (ECONNRESET);
}

static int
nvmf_tcp_save_command_capsule(struct nvmf_tcp_qpair *qp,
    struct nvmf_tcp_rxpdu *pdu)
{
	struct nvme_tcp_cmd *cmd;
	struct nvmf_capsule *nc;
	struct nvmf_tcp_capsule *tc;

	cmd = (void *)pdu->hdr;

	nc = nvmf_allocate_command(&qp->qp, &cmd->ccsqe);
	if (nc == NULL)
		return (ENOMEM);

	tc = TCAP(nc);
	tc->rx_pdu = *pdu;

	TAILQ_INSERT_TAIL(&qp->rx_capsules, tc, link);
	return (0);
}

static int
nvmf_tcp_save_response_capsule(struct nvmf_tcp_qpair *qp,
    struct nvmf_tcp_rxpdu *pdu)
{
	struct nvme_tcp_rsp *rsp;
	struct nvmf_capsule *nc;
	struct nvmf_tcp_capsule *tc;

	rsp = (void *)pdu->hdr;

	nc = nvmf_allocate_response(&qp->qp, &rsp->rccqe);
	if (nc == NULL)
		return (ENOMEM);

	nc->nc_sqhd_valid = true;
	tc = TCAP(nc);
	tc->rx_pdu = *pdu;

	TAILQ_INSERT_TAIL(&qp->rx_capsules, tc, link);

	/*
	 * Once the CQE has been received, no further transfers to the
	 * command buffer for the associated CID can occur.
	 */
	tcp_purge_command_buffer(qp, rsp->rccqe.cid, 0, true);
	tcp_purge_command_buffer(qp, rsp->rccqe.cid, 0, false);

	return (0);
}

/*
 * Construct and send a PDU that contains an optional data payload.
 * This includes dealing with digests and the length fields in the
 * common header.
 */
static int
nvmf_tcp_construct_pdu(struct nvmf_tcp_qpair *qp, void *hdr, size_t hlen,
    void *data, uint32_t data_len)
{
	struct nvme_tcp_common_pdu_hdr *ch;
	struct iovec iov[5];
	u_int iovcnt;
	uint32_t header_digest, data_digest, pad, pdo, plen;

	plen = hlen;
	if (qp->header_digests)
		plen += sizeof(header_digest);
	if (data_len != 0) {
		pdo = roundup(plen, qp->txpda);
		pad = pdo - plen;
		plen = pdo + data_len;
		if (qp->data_digests)
			plen += sizeof(data_digest);
	} else {
		assert(data == NULL);
		pdo = 0;
		pad = 0;
	}

	ch = hdr;
	ch->hlen = hlen;
	if (qp->header_digests)
		ch->flags |= NVME_TCP_CH_FLAGS_HDGSTF;
	if (qp->data_digests && data_len != 0)
		ch->flags |= NVME_TCP_CH_FLAGS_DDGSTF;
	ch->pdo = pdo;
	ch->plen = htole32(plen);

	/* CH + PSH */
	iov[0].iov_base = hdr;
	iov[0].iov_len = hlen;
	iovcnt = 1;

	/* HDGST */
	if (qp->header_digests) {
		header_digest = compute_digest(hdr, hlen);
		iov[iovcnt].iov_base = &header_digest;
		iov[iovcnt].iov_len = sizeof(header_digest);
		iovcnt++;
	}

	if (pad != 0) {
		/* PAD */
		iov[iovcnt].iov_base = __DECONST(char *, zero_padding);
		iov[iovcnt].iov_len = pad;
		iovcnt++;
	}

	if (data_len != 0) {
		/* DATA */
		iov[iovcnt].iov_base = data;
		iov[iovcnt].iov_len = data_len;
		iovcnt++;

		/* DDGST */
		if (qp->data_digests) {
			data_digest = compute_digest(data, data_len);
			iov[iovcnt].iov_base = &data_digest;
			iov[iovcnt].iov_len = sizeof(data_digest);
			iovcnt++;
		}
	}

	return (nvmf_tcp_write_pdu_iov(qp, iov, iovcnt, plen));
}

static int
nvmf_tcp_handle_h2c_data(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	struct nvme_tcp_h2c_data_hdr *h2c;
	struct nvmf_tcp_command_buffer *cb;
	uint32_t data_len, data_offset;
	const char *icd;

	h2c = (void *)pdu->hdr;
	if (le32toh(h2c->datal) > qp->maxh2cdata) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_LIMIT_EXCEEDED, 0,
		    pdu->hdr, le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	cb = tcp_find_command_buffer(qp, h2c->cccid, h2c->ttag, true);
	if (cb == NULL) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_h2c_data_hdr, ttag), pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_len = le32toh(h2c->datal);
	if (data_len != pdu->data_len) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_h2c_data_hdr, datal), pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(h2c->datao);
	if (data_offset < cb->data_offset ||
	    data_offset + data_len > cb->data_offset + cb->data_len) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0,
		    pdu->hdr, le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if (data_offset != cb->data_offset + cb->data_xfered) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if ((cb->data_xfered + data_len == cb->data_len) !=
	    ((pdu->hdr->flags & NVME_TCP_H2C_DATA_FLAGS_LAST_PDU) != 0)) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	data_offset -= cb->data_offset;
	icd = (const char *)pdu->hdr + pdu->hdr->pdo;
	memcpy((char *)cb->data + data_offset, icd, data_len);

	nvmf_tcp_free_pdu(pdu);
	return (0);
}

static int
nvmf_tcp_handle_c2h_data(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	struct nvme_tcp_c2h_data_hdr *c2h;
	struct nvmf_tcp_command_buffer *cb;
	uint32_t data_len, data_offset;
	const char *icd;

	c2h = (void *)pdu->hdr;

	cb = tcp_find_command_buffer(qp, c2h->cccid, 0, true);
	if (cb == NULL) {
		/*
		 * XXX: Could be PDU sequence error if cccid is for a
		 * command that doesn't use a command buffer.
		 */
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_c2h_data_hdr, cccid), pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_len = le32toh(c2h->datal);
	if (data_len != pdu->data_len) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_c2h_data_hdr, datal), pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(c2h->datao);
	if (data_offset < cb->data_offset ||
	    data_offset + data_len > cb->data_offset + cb->data_len) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0,
		    pdu->hdr, le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if (data_offset != cb->data_offset + cb->data_xfered) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	if ((cb->data_xfered + data_len == cb->data_len) !=
	    ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_LAST_PDU) != 0)) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	data_offset -= cb->data_offset;
	icd = (const char *)pdu->hdr + pdu->hdr->pdo;
	memcpy((char *)cb->data + data_offset, icd, data_len);

	if ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_SUCCESS) != 0) {
		struct nvme_completion cqe;
		struct nvmf_tcp_capsule *tc;
		struct nvmf_capsule *nc;

		memset(&cqe, 0, sizeof(cqe));
		cqe.cid = cb->cid;

		nc = nvmf_allocate_response(&qp->qp, &cqe);
		if (nc == NULL) {
			nvmf_tcp_free_pdu(pdu);
			return (ENOMEM);
		}
		nc->nc_sqhd_valid = false;

		tc = TCAP(nc);
		TAILQ_INSERT_TAIL(&qp->rx_capsules, tc, link);
	}

	nvmf_tcp_free_pdu(pdu);
	return (0);
}

/* NB: cid and ttag and little-endian already. */
static int
tcp_send_h2c_pdu(struct nvmf_tcp_qpair *qp, uint16_t cid, uint16_t ttag,
    uint32_t data_offset, void *buf, size_t len, bool last_pdu)
{
	struct nvme_tcp_h2c_data_hdr h2c;

	memset(&h2c, 0, sizeof(h2c));
	h2c.common.pdu_type = NVME_TCP_PDU_TYPE_H2C_DATA;
	if (last_pdu)
		h2c.common.flags |= NVME_TCP_H2C_DATA_FLAGS_LAST_PDU;
	h2c.cccid = cid;
	h2c.ttag = ttag;
	h2c.datao = htole32(data_offset);
	h2c.datal = htole32(len);

	return (nvmf_tcp_construct_pdu(qp, &h2c, sizeof(h2c), buf, len));
}

/* Sends one or more H2C_DATA PDUs, subject to MAXH2CDATA. */
static int
tcp_send_h2c_pdus(struct nvmf_tcp_qpair *qp, uint16_t cid, uint16_t ttag,
    uint32_t data_offset, void *buf, size_t len, bool last_pdu)
{
	char *p;

	p = buf;
	while (len != 0) {
		size_t todo;
		int error;

		todo = len;
		if (todo > qp->maxh2cdata)
			todo = qp->maxh2cdata;
		error = tcp_send_h2c_pdu(qp, cid, ttag, data_offset, p, todo,
		    last_pdu && todo == len);
		if (error != 0)
			return (error);
		p += todo;
		len -= todo;
	}
	return (0);
}

static int
nvmf_tcp_handle_r2t(struct nvmf_tcp_qpair *qp, struct nvmf_tcp_rxpdu *pdu)
{
	struct nvmf_tcp_command_buffer *cb;
	struct nvme_tcp_r2t_hdr *r2t;
	uint32_t data_len, data_offset;
	int error;

	r2t = (void *)pdu->hdr;

	cb = tcp_find_command_buffer(qp, r2t->cccid, 0, false);
	if (cb == NULL) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_r2t_hdr, cccid), pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(r2t->r2to);
	if (data_offset != cb->data_xfered) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->hdr,
		    le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	/*
	 * XXX: The spec does not specify how to handle R2T transfers
	 * out of range of the original command.
	 */
	data_len = le32toh(r2t->r2tl);
	if (data_offset + data_len > cb->data_len) {
		nvmf_tcp_report_error(qp->qp.nq_association, qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0,
		    pdu->hdr, le32toh(pdu->hdr->plen), pdu->hdr->hlen);
		nvmf_tcp_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;

	/*
	 * Write out one or more H2C_DATA PDUs containing the
	 * requested data.
	 */
	error = tcp_send_h2c_pdus(qp, r2t->cccid, r2t->ttag,
	    data_offset, (char *)cb->data + data_offset, data_len, true);

	nvmf_tcp_free_pdu(pdu);
	return (error);
}

static int
nvmf_tcp_receive_pdu(struct nvmf_tcp_qpair *qp)
{
	struct nvmf_tcp_rxpdu pdu;
	int error;

	error = nvmf_tcp_read_pdu(qp, &pdu);
	if (error != 0)
		return (error);

	switch (pdu.hdr->pdu_type) {
	default:
		__unreachable();
		break;
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
		return (nvmf_tcp_handle_term_req(&pdu));
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
		return (nvmf_tcp_save_command_capsule(qp, &pdu));
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
		return (nvmf_tcp_save_response_capsule(qp, &pdu));
	case NVME_TCP_PDU_TYPE_H2C_DATA:
		return (nvmf_tcp_handle_h2c_data(qp, &pdu));
	case NVME_TCP_PDU_TYPE_C2H_DATA:
		return (nvmf_tcp_handle_c2h_data(qp, &pdu));
	case NVME_TCP_PDU_TYPE_R2T:
		return (nvmf_tcp_handle_r2t(qp, &pdu));
	}
}

static bool
nvmf_tcp_validate_ic_pdu(struct nvmf_association *na, struct nvmf_tcp_qpair *qp,
    const struct nvme_tcp_common_pdu_hdr *ch, size_t pdu_len)
{
	const struct nvme_tcp_ic_req *pdu;
	uint32_t plen;
	u_int hlen;

	/* Determine how large of a PDU header to return for errors. */
	hlen = ch->hlen;
	plen = le32toh(ch->plen);
	if (hlen < sizeof(*ch) || hlen > plen)
		hlen = sizeof(*ch);

	/*
	 * Errors must be reported for the lowest incorrect field
	 * first, so validate fields in order.
	 */

	/* Validate pdu_type. */

	/* Controllers only receive PDUs with a PDU direction of 0. */
	if (na->na_controller != ((ch->pdu_type & 0x01) == 0)) {
		na_error(na, "NVMe/TCP: Invalid PDU type %u", ch->pdu_type);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 0, ch, pdu_len,
		    hlen);
		return (false);
	}

	switch (ch->pdu_type) {
	case NVME_TCP_PDU_TYPE_IC_REQ:
	case NVME_TCP_PDU_TYPE_IC_RESP:
		break;
	default:
		na_error(na, "NVMe/TCP: Invalid PDU type %u", ch->pdu_type);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 0, ch, pdu_len,
		    hlen);
		return (false);
	}

	/* Validate flags. */
	if (ch->flags != 0) {
		na_error(na, "NVMe/TCP: Invalid PDU header flags %#x",
		    ch->flags);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 1, ch, pdu_len,
		    hlen);
		return (false);
	}

	/* Validate hlen. */
	if (ch->hlen != 128) {
		na_error(na, "NVMe/TCP: Invalid PDU header length %u",
		    ch->hlen);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 2, ch, pdu_len,
		    hlen);
		return (false);
	}

	/* Validate pdo. */
	if (ch->pdo != 0) {
		na_error(na, "NVMe/TCP: Invalid PDU data offset %u", ch->pdo);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 3, ch, pdu_len,
		    hlen);
		return (false);
	}

	/* Validate plen. */
	if (plen != 128) {
		na_error(na, "NVMe/TCP: Invalid PDU length %u", plen);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 4, ch, pdu_len,
		    hlen);
		return (false);
	}

	/* Validate fields common to both ICReq and ICResp. */
	pdu = (const struct nvme_tcp_ic_req *)ch;
	if (le16toh(pdu->pfv) != 0) {
		na_error(na, "NVMe/TCP: Unsupported PDU version %u",
		    le16toh(pdu->pfv));
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_DATA_UNSUPPORTED_PARAMETER,
		    8, ch, pdu_len, hlen);
		return (false);
	}

	if (pdu->hpda > NVME_TCP_HPDA_MAX) {
		na_error(na, "NVMe/TCP: Unsupported PDA %u", pdu->hpda);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 10, ch, pdu_len,
		    hlen);
		return (false);
	}

	if (pdu->dgst.bits.reserved != 0) {
		na_error(na, "NVMe/TCP: Invalid digest settings");
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 11, ch, pdu_len,
		    hlen);
		return (false);
	}

	return (true);
}

static bool
nvmf_tcp_read_ic_req(struct nvmf_association *na, struct nvmf_tcp_qpair *qp,
    struct nvme_tcp_ic_req *pdu)
{
	int error;

	error = nvmf_tcp_read_buffer(qp->s, pdu, sizeof(*pdu));
	if (error != 0) {
		na_error(na, "NVMe/TCP: Failed to read IC request: %s",
		    strerror(error));
		return (false);
	}

	return (nvmf_tcp_validate_ic_pdu(na, qp, &pdu->common, sizeof(*pdu)));
}

static bool
nvmf_tcp_read_ic_resp(struct nvmf_association *na, struct nvmf_tcp_qpair *qp,
    struct nvme_tcp_ic_resp *pdu)
{
	int error;

	error = nvmf_tcp_read_buffer(qp->s, pdu, sizeof(*pdu));
	if (error != 0) {
		na_error(na, "NVMe/TCP: Failed to read IC response: %s",
		    strerror(error));
		return (false);
	}

	return (nvmf_tcp_validate_ic_pdu(na, qp, &pdu->common, sizeof(*pdu)));
}

static struct nvmf_association *
tcp_allocate_association(bool controller,
    const struct nvmf_association_params *params)
{
	struct nvmf_tcp_association *ta;

	if (controller) {
		/* 7.4.10.3 */
		if (params->tcp.maxh2cdata < 4096 ||
		    params->tcp.maxh2cdata % 4 != 0)
			return (NULL);
	}

	ta = calloc(1, sizeof(*ta));

	return (&ta->na);
}

static void
tcp_update_association(struct nvmf_association *na,
    const struct nvme_controller_data *cdata)
{
	struct nvmf_tcp_association *ta = TASSOC(na);

	ta->ioccsz = le32toh(cdata->ioccsz);
}

static void
tcp_free_association(struct nvmf_association *na)
{
	free(na);
}

static bool
tcp_connect(struct nvmf_tcp_qpair *qp, struct nvmf_association *na, bool admin)
{
	const struct nvmf_association_params *params = &na->na_params;
	struct nvmf_tcp_association *ta = TASSOC(na);
	struct nvme_tcp_ic_req ic_req;
	struct nvme_tcp_ic_resp ic_resp;
	uint32_t maxh2cdata;
	int error;

	if (!admin) {
		if (ta->ioccsz == 0) {
			na_error(na, "TCP I/O queues require cdata");
			return (false);
		}
		if (ta->ioccsz < 4) {
			na_error(na, "Invalid IOCCSZ %u", ta->ioccsz);
			return (false);
		}
	}

	memset(&ic_req, 0, sizeof(ic_req));
	ic_req.common.pdu_type = NVME_TCP_PDU_TYPE_IC_REQ;
	ic_req.common.hlen = sizeof(ic_req);
	ic_req.common.plen = htole32(sizeof(ic_req));
	ic_req.pfv = htole16(0);
	ic_req.hpda = params->tcp.pda;
	if (params->tcp.header_digests)
		ic_req.dgst.bits.hdgst_enable = 1;
	if (params->tcp.data_digests)
		ic_req.dgst.bits.ddgst_enable = 1;
	ic_req.maxr2t = htole32(params->tcp.maxr2t);

	error = nvmf_tcp_write_pdu(qp, &ic_req, sizeof(ic_req));
	if (error != 0) {
		na_error(na, "Failed to write IC request: %s", strerror(error));
		return (false);
	}

	if (!nvmf_tcp_read_ic_resp(na, qp, &ic_resp))
		return (false);

	/* Ensure the controller didn't enable digests we didn't request. */
	if ((!params->tcp.header_digests &&
	    ic_resp.dgst.bits.hdgst_enable != 0) ||
	    (!params->tcp.data_digests &&
	    ic_resp.dgst.bits.ddgst_enable != 0)) {
		na_error(na, "Controller enabled unrequested digests");
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_DATA_UNSUPPORTED_PARAMETER,
		    11, &ic_resp, sizeof(ic_resp), sizeof(ic_resp));
		return (false);
	}

	/*
	 * XXX: Is there an upper-bound to enforce here?  Perhaps pick
	 * some large value and report larger values as an unsupported
	 * parameter?
	 */
	maxh2cdata = le32toh(ic_resp.maxh2cdata);
	if (maxh2cdata < 4096 || maxh2cdata % 4 != 0) {
		na_error(na, "Invalid MAXH2CDATA %u", maxh2cdata);
		nvmf_tcp_report_error(na, qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD, 12, &ic_resp,
		    sizeof(ic_resp), sizeof(ic_resp));
		return (false);
	}

	qp->rxpda = (params->tcp.pda + 1) * 4;
	qp->txpda = (ic_resp.cpda + 1) * 4;
	qp->header_digests = ic_resp.dgst.bits.hdgst_enable != 0;
	qp->data_digests = ic_resp.dgst.bits.ddgst_enable != 0;
	qp->maxr2t = params->tcp.maxr2t;
	qp->maxh2cdata = maxh2cdata;
	if (admin)
		/* 7.4.3 */
		qp->max_icd = 8192;
	else
		qp->max_icd = (ta->ioccsz - 4) * 16;

	return (0);
}

static bool
tcp_accept(struct nvmf_tcp_qpair *qp, struct nvmf_association *na)
{
	const struct nvmf_association_params *params = &na->na_params;
	struct nvme_tcp_ic_req ic_req;
	struct nvme_tcp_ic_resp ic_resp;
	int error;

	if (!nvmf_tcp_read_ic_req(na, qp, &ic_req))
		return (false);

	memset(&ic_resp, 0, sizeof(ic_resp));
	ic_resp.common.pdu_type = NVME_TCP_PDU_TYPE_IC_RESP;
	ic_resp.common.hlen = sizeof(ic_req);
	ic_resp.common.plen = htole32(sizeof(ic_req));
	ic_resp.pfv = htole16(0);
	ic_resp.cpda = params->tcp.pda;
	if (params->tcp.header_digests && ic_req.dgst.bits.hdgst_enable != 0)
		ic_resp.dgst.bits.hdgst_enable = 1;
	if (params->tcp.data_digests && ic_req.dgst.bits.ddgst_enable != 0)
		ic_resp.dgst.bits.ddgst_enable = 1;
	ic_resp.maxh2cdata = htole32(params->tcp.maxh2cdata);

	error = nvmf_tcp_write_pdu(qp, &ic_resp, sizeof(ic_resp));
	if (error != 0) {
		na_error(na, "Failed to write IC response: %s",
		    strerror(error));
		return (false);
	}

	qp->rxpda = (params->tcp.pda + 1) * 4;
	qp->txpda = (ic_req.hpda + 1) * 4;
	qp->header_digests = ic_resp.dgst.bits.hdgst_enable != 0;
	qp->data_digests = ic_resp.dgst.bits.ddgst_enable != 0;
	qp->maxr2t = le32toh(ic_req.maxr2t);
	qp->maxh2cdata = params->tcp.maxh2cdata;
	qp->max_icd = 0;	/* XXX */
	return (0);
}

static struct nvmf_qpair *
tcp_allocate_qpair(struct nvmf_association *na,
    const struct nvmf_qpair_params *qparams)
{
	const struct nvmf_association_params *aparams = &na->na_params;
	struct nvmf_tcp_qpair *qp;
	int error;

	if (aparams->tcp.pda > NVME_TCP_CPDA_MAX) {
		na_error(na, "Invalid PDA");
		return (NULL);
	}

	qp = calloc(1, sizeof(*qp));
	qp->s = qparams->tcp.fd;
	LIST_INIT(&qp->rx_buffers);
	LIST_INIT(&qp->tx_buffers);
	TAILQ_INIT(&qp->rx_capsules);
	if (na->na_controller)
		error = tcp_accept(qp, na);
	else
		error = tcp_connect(qp, na, qparams->admin);
	if (error != 0) {
		free(qp);
		return (NULL);
	}

	return (&qp->qp);
}

static void
tcp_free_qpair(struct nvmf_qpair *nq)
{
	struct nvmf_tcp_qpair *qp = TQP(nq);
	struct nvmf_tcp_capsule *ntc, *tc;
	struct nvmf_tcp_command_buffer *ncb, *cb;

	TAILQ_FOREACH_SAFE(tc, &qp->rx_capsules, link, ntc) {
		TAILQ_REMOVE(&qp->rx_capsules, tc, link);
		nvmf_free_capsule(&tc->nc);
	}
	LIST_FOREACH_SAFE(cb, &qp->rx_buffers, link, ncb) {
		tcp_free_command_buffer(cb);
	}
	LIST_FOREACH_SAFE(cb, &qp->tx_buffers, link, ncb) {
		tcp_free_command_buffer(cb);
	}
	free(qp);
}

static void
tcp_kernel_handoff_params(struct nvmf_qpair *nq, nvlist_t *nvl)
{
	struct nvmf_tcp_qpair *qp = TQP(nq);

	nvlist_add_number(nvl, "fd", qp->s);
	nvlist_add_number(nvl, "rxpda", qp->rxpda);
	nvlist_add_number(nvl, "txpda", qp->txpda);
	nvlist_add_bool(nvl, "header_digests", qp->header_digests);
	nvlist_add_bool(nvl, "data_digests", qp->data_digests);
	nvlist_add_number(nvl, "maxr2t", qp->maxr2t);
	nvlist_add_number(nvl, "maxh2cdata", qp->maxh2cdata);
	nvlist_add_number(nvl, "max_icd", qp->max_icd);
}

static int
tcp_populate_dle(struct nvmf_qpair *nq, struct nvme_discovery_log_entry *dle)
{
	struct nvmf_tcp_qpair *qp = TQP(nq);
	struct sockaddr_storage ss;
	socklen_t ss_len;

	ss_len = sizeof(ss);
	if (getpeername(qp->s, (struct sockaddr *)&ss, &ss_len) == -1)
		return (errno);

	if (getnameinfo((struct sockaddr *)&ss, ss_len, dle->traddr,
	    sizeof(dle->traddr), dle->trsvcid, sizeof(dle->trsvcid),
	    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		return (EINVAL);

	return (0);
}

static struct nvmf_capsule *
tcp_allocate_capsule(struct nvmf_qpair *qp __unused)
{
	struct nvmf_tcp_capsule *nc;

	nc = calloc(1, sizeof(*nc));
	return (&nc->nc);
}

static void
tcp_free_capsule(struct nvmf_capsule *nc)
{
	struct nvmf_tcp_capsule *tc = TCAP(nc);

	nvmf_tcp_free_pdu(&tc->rx_pdu);
	if (tc->cb != NULL)
		tcp_free_command_buffer(tc->cb);
	free(tc);
}

static int
tcp_transmit_command(struct nvmf_capsule *nc)
{
	struct nvmf_tcp_qpair *qp = TQP(nc->nc_qpair);
	struct nvmf_tcp_capsule *tc = TCAP(nc);
	struct nvme_tcp_cmd cmd;
	struct nvme_sgl_descriptor *sgl;
	int error;
	bool use_icd;

	use_icd = false;
	if (nc->nc_data_len != 0 && nc->nc_send_data &&
	    nc->nc_data_len <= qp->max_icd)
		use_icd = true;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.pdu_type = NVME_TCP_PDU_TYPE_CAPSULE_CMD;
	cmd.ccsqe = nc->nc_sqe;

	/* Populate SGL in SQE. */
	sgl = &cmd.ccsqe.sgl;
	memset(sgl, 0, sizeof(*sgl));
	sgl->address = 0;
	sgl->length = htole32(nc->nc_data_len);
	if (use_icd) {
		/* Use in-capsule data. */
		sgl->type = NVME_SGL_TYPE_ICD;
	} else {
		/* Use a command buffer. */
		sgl->type = NVME_SGL_TYPE_COMMAND_BUFFER;
	}

	/* Send command capsule. */
	error = nvmf_tcp_construct_pdu(qp, &cmd, sizeof(cmd), use_icd ?
	    nc->nc_data : NULL, use_icd ? nc->nc_data_len : 0);
	if (error != 0)
		return (error);

	/*
	 * If data will be transferred using a command buffer, allocate a
	 * buffer structure and queue it.
	 */
	if (nc->nc_data_len != 0 && !use_icd)
		tc->cb = tcp_alloc_command_buffer(qp, nc->nc_data, 0,
		    nc->nc_data_len, cmd.ccsqe.cid, 0, !nc->nc_send_data);

	return (0);
}

static int
tcp_transmit_response(struct nvmf_capsule *nc)
{
	struct nvmf_tcp_qpair *qp = TQP(nc->nc_qpair);
	struct nvme_tcp_rsp rsp;

	memset(&rsp, 0, sizeof(rsp));
	rsp.common.pdu_type = NVME_TCP_PDU_TYPE_CAPSULE_RESP;
	rsp.rccqe = nc->nc_cqe;

	return (nvmf_tcp_construct_pdu(qp, &rsp, sizeof(rsp), NULL, 0));
}

static int
tcp_transmit_capsule(struct nvmf_capsule *nc)
{
	if (nc->nc_qe_len == sizeof(struct nvme_command))
		return (tcp_transmit_command(nc));
	else
		return (tcp_transmit_response(nc));
}

static int
tcp_receive_capsule(struct nvmf_qpair *nq, struct nvmf_capsule **ncp)
{
	struct nvmf_tcp_qpair *qp = TQP(nq);
	struct nvmf_tcp_capsule *tc;
	int error;

	while (TAILQ_EMPTY(&qp->rx_capsules)) {
		error = nvmf_tcp_receive_pdu(qp);
		if (error != 0)
			return (error);
	}
	tc = TAILQ_FIRST(&qp->rx_capsules);
	TAILQ_REMOVE(&qp->rx_capsules, tc, link);
	*ncp = &tc->nc;
	return (0);
}

static uint8_t
tcp_validate_command_capsule(const struct nvmf_capsule *nc)
{
	const struct nvmf_tcp_capsule *tc = CTCAP(nc);
	const struct nvme_sgl_descriptor *sgl;

	assert(tc->rx_pdu.hdr != NULL);

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
	assert(nc->nc_qe_len == sizeof(struct nvme_command));
	return (le32toh(nc->nc_sqe.sgl.length));
}

/* NB: cid and ttag are both little-endian already. */
static int
tcp_send_r2t(struct nvmf_tcp_qpair *qp, uint16_t cid, uint16_t ttag,
    uint32_t data_offset, uint32_t data_len)
{
	struct nvme_tcp_r2t_hdr r2t;

	memset(&r2t, 0, sizeof(r2t));
	r2t.common.pdu_type = NVME_TCP_PDU_TYPE_R2T;
	r2t.cccid = cid;
	r2t.ttag = ttag;
	r2t.r2to = htole32(data_offset);
	r2t.r2tl = htole32(data_len);

	return (nvmf_tcp_construct_pdu(qp, &r2t, sizeof(r2t), NULL, 0));
}

static int
tcp_receive_r2t_data(const struct nvmf_capsule *nc, uint32_t data_offset,
    void *buf, size_t len)
{
	struct nvmf_tcp_qpair *qp = TQP(nc->nc_qpair);
	struct nvmf_tcp_command_buffer *cb;
	int error;
	uint16_t ttag;

	/*
	 * Don't bother byte-swapping ttag as it is just a cookie
	 * value returned by the other end as-is.
	 */
	ttag = qp->next_ttag++;

	error = tcp_send_r2t(qp, nc->nc_sqe.cid, ttag, data_offset, len);
	if (error != 0)
		return (error);

	cb = tcp_alloc_command_buffer(qp, buf, data_offset, len,
	    nc->nc_sqe.cid, ttag, true);

	/* Parse received PDUs until the data transfer is complete. */
	while (cb->data_xfered < cb->data_len) {
		error = nvmf_tcp_receive_pdu(qp);
		if (error != 0)
			break;
	}
	tcp_free_command_buffer(cb);
	return (error);
}

static int
tcp_receive_icd_data(const struct nvmf_capsule *nc, uint32_t data_offset,
    void *buf, size_t len)
{
	const struct nvmf_tcp_capsule *tc = CTCAP(nc);
	const char *icd;

	icd = (const char *)tc->rx_pdu.hdr + tc->rx_pdu.hdr->pdo + data_offset;
	memcpy(buf, icd, len);
	return (0);
}

static int
tcp_receive_controller_data(const struct nvmf_capsule *nc, uint32_t data_offset,
    void *buf, size_t len)
{
	struct nvmf_association *na = nc->nc_qpair->nq_association;
	const struct nvme_sgl_descriptor *sgl;
	size_t data_len;

	if (nc->nc_qe_len != sizeof(struct nvme_command) || !na->na_controller)
		return (EINVAL);

	sgl = &nc->nc_sqe.sgl;
	data_len = le32toh(sgl->length);
	if (data_offset + len > data_len)
		return (EFBIG);

	if (sgl->type == NVME_SGL_TYPE_ICD)
		return (tcp_receive_icd_data(nc, data_offset, buf, len));
	else
		return (tcp_receive_r2t_data(nc, data_offset, buf, len));
}

/* NB: cid is little-endian already. */
static int
tcp_send_c2h_pdu(struct nvmf_tcp_qpair *qp, uint16_t cid,
    uint32_t data_offset, const void *buf, size_t len, bool last_pdu,
    bool success)
{
	struct nvme_tcp_c2h_data_hdr c2h;

	memset(&c2h, 0, sizeof(c2h));
	c2h.common.pdu_type = NVME_TCP_PDU_TYPE_C2H_DATA;
	if (last_pdu)
		c2h.common.flags |= NVME_TCP_C2H_DATA_FLAGS_LAST_PDU;
	if (success)
		c2h.common.flags |= NVME_TCP_C2H_DATA_FLAGS_SUCCESS;
	c2h.cccid = cid;
	c2h.datao = htole32(data_offset);
	c2h.datal = htole32(len);

	return (nvmf_tcp_construct_pdu(qp, &c2h, sizeof(c2h),
	    __DECONST(void *, buf), len));
}

static int
tcp_send_controller_data(const struct nvmf_capsule *nc, const void *buf,
    size_t len)
{
	struct nvmf_association *na = nc->nc_qpair->nq_association;
	struct nvmf_tcp_qpair *qp = TQP(nc->nc_qpair);
	const struct nvme_sgl_descriptor *sgl;
	const char *src;
	size_t todo;
	uint32_t data_len, data_offset;
	int error;
	bool last_pdu, send_success_flag;

	if (nc->nc_qe_len != sizeof(struct nvme_command) || !na->na_controller)
		return (EINVAL);

	sgl = &nc->nc_sqe.sgl;
	data_len = le32toh(sgl->length);
	if (len != data_len) {
		nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
		return (EFBIG);
	}

	if (sgl->type != NVME_SGL_TYPE_COMMAND_BUFFER) {
		nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
		return (EINVAL);
	}

	/* Use the SUCCESS flag if SQ flow control is disabled. */
	send_success_flag = !qp->qp.nq_flow_control;

	/*
	 * Write out one or more C2H_DATA PDUs containing the data.
	 * Each PDU is arbitrarily capped at 256k.
	 */
	data_offset = 0;
	src = buf;
	while (len > 0) {
		if (len > 256 * 1024) {
			todo = 256 * 1024;
			last_pdu = false;
		} else {
			todo = len;
			last_pdu = true;
		}
		error = tcp_send_c2h_pdu(qp, nc->nc_sqe.cid, data_offset,
		    src, todo, last_pdu, last_pdu && send_success_flag);
		if (error != 0) {
			nvmf_send_generic_error(nc,
			    NVME_SC_TRANSIENT_TRANSPORT_ERROR);
			return (error);
		}
		data_offset += todo;
		src += todo;
		len -= todo;
	}
	if (!send_success_flag)
		nvmf_send_success(nc);
	return (0);
}

struct nvmf_transport_ops tcp_ops = {
	.allocate_association = tcp_allocate_association,
	.update_association = tcp_update_association,
	.free_association = tcp_free_association,
	.allocate_qpair = tcp_allocate_qpair,
	.free_qpair = tcp_free_qpair,
	.kernel_handoff_params = tcp_kernel_handoff_params,
	.populate_dle = tcp_populate_dle,
	.allocate_capsule = tcp_allocate_capsule,
	.free_capsule = tcp_free_capsule,
	.transmit_capsule = tcp_transmit_capsule,
	.receive_capsule = tcp_receive_capsule,
	.validate_command_capsule = tcp_validate_command_capsule,
	.capsule_data_len = tcp_capsule_data_len,
	.receive_controller_data = tcp_receive_controller_data,
	.send_controller_data = tcp_send_controller_data,
};
