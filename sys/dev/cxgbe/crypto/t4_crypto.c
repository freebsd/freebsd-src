/*-
 * Copyright (c) 2017 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/sglist.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include "cryptodev_if.h"

#include "common/common.h"
#include "crypto/t4_crypto.h"

/*
 * Requests consist of:
 *
 * +-------------------------------+
 * | struct fw_crypto_lookaside_wr |
 * +-------------------------------+
 * | struct ulp_txpkt              |
 * +-------------------------------+
 * | struct ulptx_idata            |
 * +-------------------------------+
 * | struct cpl_tx_sec_pdu         |
 * +-------------------------------+
 * | struct cpl_tls_tx_scmd_fmt    |
 * +-------------------------------+
 * | key context header            |
 * +-------------------------------+
 * | AES key                       |  ----- For requests with AES
 * +-------------------------------+
 * | Hash state                    |  ----- For hash-only requests
 * +-------------------------------+ -
 * | IPAD (16-byte aligned)        |  \
 * +-------------------------------+  +---- For requests with HMAC
 * | OPAD (16-byte aligned)        |  /
 * +-------------------------------+ -
 * | GMAC H                        |  ----- For AES-GCM
 * +-------------------------------+ -
 * | struct cpl_rx_phys_dsgl       |  \
 * +-------------------------------+  +---- Destination buffer for
 * | PHYS_DSGL entries             |  /     non-hash-only requests
 * +-------------------------------+ -
 * | 16 dummy bytes                |  ----- Only for HMAC/hash-only requests
 * +-------------------------------+
 * | IV                            |  ----- If immediate IV
 * +-------------------------------+
 * | Payload                       |  ----- If immediate Payload
 * +-------------------------------+ -
 * | struct ulptx_sgl              |  \
 * +-------------------------------+  +---- If payload via SGL
 * | SGL entries                   |  /
 * +-------------------------------+ -
 *
 * Note that the key context must be padded to ensure 16-byte alignment.
 * For HMAC requests, the key consists of the partial hash of the IPAD
 * followed by the partial hash of the OPAD.
 *
 * Replies consist of:
 *
 * +-------------------------------+
 * | struct cpl_fw6_pld            |
 * +-------------------------------+
 * | hash digest                   |  ----- For HMAC request with
 * +-------------------------------+        'hash_size' set in work request
 *
 * A 32-bit big-endian error status word is supplied in the last 4
 * bytes of data[0] in the CPL_FW6_PLD message.  bit 0 indicates a
 * "MAC" error and bit 1 indicates a "PAD" error.
 *
 * The 64-bit 'cookie' field from the fw_crypto_lookaside_wr message
 * in the request is returned in data[1] of the CPL_FW6_PLD message.
 *
 * For block cipher replies, the updated IV is supplied in data[2] and
 * data[3] of the CPL_FW6_PLD message.
 *
 * For hash replies where the work request set 'hash_size' to request
 * a copy of the hash in the reply, the hash digest is supplied
 * immediately following the CPL_FW6_PLD message.
 */

/*
 * The crypto engine supports a maximum AAD size of 511 bytes.
 */
#define	MAX_AAD_LEN		511

/*
 * The documentation for CPL_RX_PHYS_DSGL claims a maximum of 32 SG
 * entries.  While the CPL includes a 16-bit length field, the T6 can
 * sometimes hang if an error occurs while processing a request with a
 * single DSGL entry larger than 2k.
 */
#define	MAX_RX_PHYS_DSGL_SGE	32
#define	DSGL_SGE_MAXLEN		2048

/*
 * The adapter only supports requests with a total input or output
 * length of 64k-1 or smaller.  Longer requests either result in hung
 * requests or incorrect results.
 */
#define	MAX_REQUEST_SIZE	65535

static MALLOC_DEFINE(M_CCR, "ccr", "Chelsio T6 crypto");

struct ccr_session_hmac {
	struct auth_hash *auth_hash;
	int hash_len;
	unsigned int partial_digest_len;
	unsigned int auth_mode;
	unsigned int mk_size;
	char pads[CHCR_HASH_MAX_BLOCK_SIZE_128 * 2];
};

struct ccr_session_gmac {
	int hash_len;
	char ghash_h[GMAC_BLOCK_LEN];
};

struct ccr_session_ccm_mac {
	int hash_len;
};

struct ccr_session_blkcipher {
	unsigned int cipher_mode;
	unsigned int key_len;
	unsigned int iv_len;
	__be32 key_ctx_hdr;
	char enckey[CHCR_AES_MAX_KEY_LEN];
	char deckey[CHCR_AES_MAX_KEY_LEN];
};

struct ccr_port {
	struct sge_wrq *txq;
	struct sge_rxq *rxq;
	int tx_channel_id;
	u_int active_sessions;
};

struct ccr_session {
#ifdef INVARIANTS
	int pending;
#endif
	enum { HASH, HMAC, BLKCIPHER, ETA, GCM, CCM } mode;
	struct ccr_port *port;
	union {
		struct ccr_session_hmac hmac;
		struct ccr_session_gmac gmac;
		struct ccr_session_ccm_mac ccm_mac;
	};
	struct ccr_session_blkcipher blkcipher;
	struct mtx lock;

	/*
	 * Pre-allocate S/G lists used when preparing a work request.
	 * 'sg_input' contains an sglist describing the entire input
	 * buffer for a 'struct cryptop'.  'sg_output' contains an
	 * sglist describing the entire output buffer.  'sg_ulptx' is
	 * used to describe the data the engine should DMA as input
	 * via ULPTX_SGL.  'sg_dsgl' is used to describe the
	 * destination that cipher text and a tag should be written
	 * to.
	 */
	struct sglist *sg_input;
	struct sglist *sg_output;
	struct sglist *sg_ulptx;
	struct sglist *sg_dsgl;
};

struct ccr_softc {
	struct adapter *adapter;
	device_t dev;
	uint32_t cid;
	struct mtx lock;
	bool detaching;
	struct ccr_port ports[MAX_NPORTS];
	u_int port_mask;

	/*
	 * Pre-allocate a dummy output buffer for the IV and AAD for
	 * AEAD requests.
	 */
	char *iv_aad_buf;
	struct sglist *sg_iv_aad;

	/* Statistics. */
	counter_u64_t stats_blkcipher_encrypt;
	counter_u64_t stats_blkcipher_decrypt;
	counter_u64_t stats_hash;
	counter_u64_t stats_hmac;
	counter_u64_t stats_eta_encrypt;
	counter_u64_t stats_eta_decrypt;
	counter_u64_t stats_gcm_encrypt;
	counter_u64_t stats_gcm_decrypt;
	counter_u64_t stats_ccm_encrypt;
	counter_u64_t stats_ccm_decrypt;
	counter_u64_t stats_wr_nomem;
	counter_u64_t stats_inflight;
	counter_u64_t stats_mac_error;
	counter_u64_t stats_pad_error;
	counter_u64_t stats_sglist_error;
	counter_u64_t stats_process_error;
	counter_u64_t stats_sw_fallback;
};

/*
 * Crypto requests involve two kind of scatter/gather lists.
 *
 * Non-hash-only requests require a PHYS_DSGL that describes the
 * location to store the results of the encryption or decryption
 * operation.  This SGL uses a different format (PHYS_DSGL) and should
 * exclude the skip bytes at the start of the data as well as any AAD
 * or IV.  For authenticated encryption requests it should include the
 * destination of the hash or tag.
 *
 * The input payload may either be supplied inline as immediate data,
 * or via a standard ULP_TX SGL.  This SGL should include AAD,
 * ciphertext, and the hash or tag for authenticated decryption
 * requests.
 *
 * These scatter/gather lists can describe different subsets of the
 * buffers described by the crypto operation.  ccr_populate_sglist()
 * generates a scatter/gather list that covers an entire crypto
 * operation buffer that is then used to construct the other
 * scatter/gather lists.
 */
static int
ccr_populate_sglist(struct sglist *sg, struct crypto_buffer *cb)
{
	int error;

	sglist_reset(sg);
	switch (cb->cb_type) {
	case CRYPTO_BUF_MBUF:
		error = sglist_append_mbuf(sg, cb->cb_mbuf);
		break;
	case CRYPTO_BUF_UIO:
		error = sglist_append_uio(sg, cb->cb_uio);
		break;
	case CRYPTO_BUF_CONTIG:
		error = sglist_append(sg, cb->cb_buf, cb->cb_buf_len);
		break;
	default:
		error = EINVAL;
	}
	return (error);
}

/*
 * Segments in 'sg' larger than 'maxsegsize' are counted as multiple
 * segments.
 */
static int
ccr_count_sgl(struct sglist *sg, int maxsegsize)
{
	int i, nsegs;

	nsegs = 0;
	for (i = 0; i < sg->sg_nseg; i++)
		nsegs += howmany(sg->sg_segs[i].ss_len, maxsegsize);
	return (nsegs);
}

/* These functions deal with PHYS_DSGL for the reply buffer. */
static inline int
ccr_phys_dsgl_len(int nsegs)
{
	int len;

	len = (nsegs / 8) * sizeof(struct phys_sge_pairs);
	if ((nsegs % 8) != 0) {
		len += sizeof(uint16_t) * 8;
		len += roundup2(nsegs % 8, 2) * sizeof(uint64_t);
	}
	return (len);
}

static void
ccr_write_phys_dsgl(struct ccr_session *s, void *dst, int nsegs)
{
	struct sglist *sg;
	struct cpl_rx_phys_dsgl *cpl;
	struct phys_sge_pairs *sgl;
	vm_paddr_t paddr;
	size_t seglen;
	u_int i, j;

	sg = s->sg_dsgl;
	cpl = dst;
	cpl->op_to_tid = htobe32(V_CPL_RX_PHYS_DSGL_OPCODE(CPL_RX_PHYS_DSGL) |
	    V_CPL_RX_PHYS_DSGL_ISRDMA(0));
	cpl->pcirlxorder_to_noofsgentr = htobe32(
	    V_CPL_RX_PHYS_DSGL_PCIRLXORDER(0) |
	    V_CPL_RX_PHYS_DSGL_PCINOSNOOP(0) |
	    V_CPL_RX_PHYS_DSGL_PCITPHNTENB(0) | V_CPL_RX_PHYS_DSGL_DCAID(0) |
	    V_CPL_RX_PHYS_DSGL_NOOFSGENTR(nsegs));
	cpl->rss_hdr_int.opcode = CPL_RX_PHYS_ADDR;
	cpl->rss_hdr_int.qid = htobe16(s->port->rxq->iq.abs_id);
	cpl->rss_hdr_int.hash_val = 0;
	sgl = (struct phys_sge_pairs *)(cpl + 1);
	j = 0;
	for (i = 0; i < sg->sg_nseg; i++) {
		seglen = sg->sg_segs[i].ss_len;
		paddr = sg->sg_segs[i].ss_paddr;
		do {
			sgl->addr[j] = htobe64(paddr);
			if (seglen > DSGL_SGE_MAXLEN) {
				sgl->len[j] = htobe16(DSGL_SGE_MAXLEN);
				paddr += DSGL_SGE_MAXLEN;
				seglen -= DSGL_SGE_MAXLEN;
			} else {
				sgl->len[j] = htobe16(seglen);
				seglen = 0;
			}
			j++;
			if (j == 8) {
				sgl++;
				j = 0;
			}
		} while (seglen != 0);
	}
	MPASS(j + 8 * (sgl - (struct phys_sge_pairs *)(cpl + 1)) == nsegs);
}

/* These functions deal with the ULPTX_SGL for input payload. */
static inline int
ccr_ulptx_sgl_len(int nsegs)
{
	u_int n;

	nsegs--; /* first segment is part of ulptx_sgl */
	n = sizeof(struct ulptx_sgl) + 8 * ((3 * nsegs) / 2 + (nsegs & 1));
	return (roundup2(n, 16));
}

static void
ccr_write_ulptx_sgl(struct ccr_session *s, void *dst, int nsegs)
{
	struct ulptx_sgl *usgl;
	struct sglist *sg;
	struct sglist_seg *ss;
	int i;

	sg = s->sg_ulptx;
	MPASS(nsegs == sg->sg_nseg);
	ss = &sg->sg_segs[0];
	usgl = dst;
	usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
	    V_ULPTX_NSGE(nsegs));
	usgl->len0 = htobe32(ss->ss_len);
	usgl->addr0 = htobe64(ss->ss_paddr);
	ss++;
	for (i = 0; i < sg->sg_nseg - 1; i++) {
		usgl->sge[i / 2].len[i & 1] = htobe32(ss->ss_len);
		usgl->sge[i / 2].addr[i & 1] = htobe64(ss->ss_paddr);
		ss++;
	}
}

static bool
ccr_use_imm_data(u_int transhdr_len, u_int input_len)
{

	if (input_len > CRYPTO_MAX_IMM_TX_PKT_LEN)
		return (false);
	if (roundup2(transhdr_len, 16) + roundup2(input_len, 16) >
	    SGE_MAX_WR_LEN)
		return (false);
	return (true);
}

static void
ccr_populate_wreq(struct ccr_softc *sc, struct ccr_session *s,
    struct chcr_wr *crwr, u_int kctx_len, u_int wr_len, u_int imm_len,
    u_int sgl_len, u_int hash_size, struct cryptop *crp)
{
	u_int cctx_size, idata_len;

	cctx_size = sizeof(struct _key_ctx) + kctx_len;
	crwr->wreq.op_to_cctx_size = htobe32(
	    V_FW_CRYPTO_LOOKASIDE_WR_OPCODE(FW_CRYPTO_LOOKASIDE_WR) |
	    V_FW_CRYPTO_LOOKASIDE_WR_COMPL(0) |
	    V_FW_CRYPTO_LOOKASIDE_WR_IMM_LEN(imm_len) |
	    V_FW_CRYPTO_LOOKASIDE_WR_CCTX_LOC(1) |
	    V_FW_CRYPTO_LOOKASIDE_WR_CCTX_SIZE(cctx_size >> 4));
	crwr->wreq.len16_pkd = htobe32(
	    V_FW_CRYPTO_LOOKASIDE_WR_LEN16(wr_len / 16));
	crwr->wreq.session_id = 0;
	crwr->wreq.rx_chid_to_rx_q_id = htobe32(
	    V_FW_CRYPTO_LOOKASIDE_WR_RX_CHID(s->port->tx_channel_id) |
	    V_FW_CRYPTO_LOOKASIDE_WR_LCB(0) |
	    V_FW_CRYPTO_LOOKASIDE_WR_PHASH(0) |
	    V_FW_CRYPTO_LOOKASIDE_WR_IV(IV_NOP) |
	    V_FW_CRYPTO_LOOKASIDE_WR_FQIDX(0) |
	    V_FW_CRYPTO_LOOKASIDE_WR_TX_CH(0) |
	    V_FW_CRYPTO_LOOKASIDE_WR_RX_Q_ID(s->port->rxq->iq.abs_id));
	crwr->wreq.key_addr = 0;
	crwr->wreq.pld_size_hash_size = htobe32(
	    V_FW_CRYPTO_LOOKASIDE_WR_PLD_SIZE(sgl_len) |
	    V_FW_CRYPTO_LOOKASIDE_WR_HASH_SIZE(hash_size));
	crwr->wreq.cookie = htobe64((uintptr_t)crp);

	crwr->ulptx.cmd_dest = htobe32(V_ULPTX_CMD(ULP_TX_PKT) |
	    V_ULP_TXPKT_DATAMODIFY(0) |
	    V_ULP_TXPKT_CHANNELID(s->port->tx_channel_id) |
	    V_ULP_TXPKT_DEST(0) |
	    V_ULP_TXPKT_FID(s->port->rxq->iq.abs_id) | V_ULP_TXPKT_RO(1));
	crwr->ulptx.len = htobe32(
	    ((wr_len - sizeof(struct fw_crypto_lookaside_wr)) / 16));

	crwr->sc_imm.cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM) |
	    V_ULP_TX_SC_MORE(sgl_len != 0 ? 1 : 0));
	idata_len = wr_len - offsetof(struct chcr_wr, sec_cpl) - sgl_len;
	if (imm_len % 16 != 0)
		idata_len -= 16 - imm_len % 16;
	crwr->sc_imm.len = htobe32(idata_len);
}

static int
ccr_hash(struct ccr_softc *sc, struct ccr_session *s, struct cryptop *crp)
{
	struct chcr_wr *crwr;
	struct wrqe *wr;
	struct auth_hash *axf;
	char *dst;
	u_int hash_size_in_response, kctx_flits, kctx_len, transhdr_len, wr_len;
	u_int hmac_ctrl, imm_len, iopad_size;
	int error, sgl_nsegs, sgl_len, use_opad;

	/* Reject requests with too large of an input buffer. */
	if (crp->crp_payload_length > MAX_REQUEST_SIZE)
		return (EFBIG);

	axf = s->hmac.auth_hash;

	if (s->mode == HMAC) {
		use_opad = 1;
		hmac_ctrl = SCMD_HMAC_CTRL_NO_TRUNC;
	} else {
		use_opad = 0;
		hmac_ctrl = SCMD_HMAC_CTRL_NOP;
	}

	/* PADs must be 128-bit aligned. */
	iopad_size = roundup2(s->hmac.partial_digest_len, 16);

	/*
	 * The 'key' part of the context includes the aligned IPAD and
	 * OPAD.
	 */
	kctx_len = iopad_size;
	if (use_opad)
		kctx_len += iopad_size;
	hash_size_in_response = axf->hashsize;
	transhdr_len = HASH_TRANSHDR_SIZE(kctx_len);

	if (crp->crp_payload_length == 0) {
		imm_len = axf->blocksize;
		sgl_nsegs = 0;
		sgl_len = 0;
	} else if (ccr_use_imm_data(transhdr_len, crp->crp_payload_length)) {
		imm_len = crp->crp_payload_length;
		sgl_nsegs = 0;
		sgl_len = 0;
	} else {
		imm_len = 0;
		sglist_reset(s->sg_ulptx);
		error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
		if (error)
			return (error);
		sgl_nsegs = s->sg_ulptx->sg_nseg;
		sgl_len = ccr_ulptx_sgl_len(sgl_nsegs);
	}

	wr_len = roundup2(transhdr_len, 16) + roundup2(imm_len, 16) + sgl_len;
	if (wr_len > SGE_MAX_WR_LEN)
		return (EFBIG);
	wr = alloc_wrqe(wr_len, s->port->txq);
	if (wr == NULL) {
		counter_u64_add(sc->stats_wr_nomem, 1);
		return (ENOMEM);
	}
	crwr = wrtod(wr);
	memset(crwr, 0, wr_len);

	ccr_populate_wreq(sc, s, crwr, kctx_len, wr_len, imm_len, sgl_len,
	    hash_size_in_response, crp);

	crwr->sec_cpl.op_ivinsrtofst = htobe32(
	    V_CPL_TX_SEC_PDU_OPCODE(CPL_TX_SEC_PDU) |
	    V_CPL_TX_SEC_PDU_RXCHID(s->port->tx_channel_id) |
	    V_CPL_TX_SEC_PDU_ACKFOLLOWS(0) | V_CPL_TX_SEC_PDU_ULPTXLPBK(1) |
	    V_CPL_TX_SEC_PDU_CPLLEN(2) | V_CPL_TX_SEC_PDU_PLACEHOLDER(0) |
	    V_CPL_TX_SEC_PDU_IVINSRTOFST(0));

	crwr->sec_cpl.pldlen = htobe32(crp->crp_payload_length == 0 ?
	    axf->blocksize : crp->crp_payload_length);

	crwr->sec_cpl.cipherstop_lo_authinsert = htobe32(
	    V_CPL_TX_SEC_PDU_AUTHSTART(1) | V_CPL_TX_SEC_PDU_AUTHSTOP(0));

	/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
	crwr->sec_cpl.seqno_numivs = htobe32(
	    V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_CIPH_MODE(SCMD_CIPH_MODE_NOP) |
	    V_SCMD_AUTH_MODE(s->hmac.auth_mode) |
	    V_SCMD_HMAC_CTRL(hmac_ctrl));
	crwr->sec_cpl.ivgen_hdrlen = htobe32(
	    V_SCMD_LAST_FRAG(0) |
	    V_SCMD_MORE_FRAGS(crp->crp_payload_length == 0 ? 1 : 0) |
	    V_SCMD_MAC_ONLY(1));

	memcpy(crwr->key_ctx.key, s->hmac.pads, kctx_len);

	/* XXX: F_KEY_CONTEXT_SALT_PRESENT set, but 'salt' not set. */
	kctx_flits = (sizeof(struct _key_ctx) + kctx_len) / 16;
	crwr->key_ctx.ctx_hdr = htobe32(V_KEY_CONTEXT_CTX_LEN(kctx_flits) |
	    V_KEY_CONTEXT_OPAD_PRESENT(use_opad) |
	    V_KEY_CONTEXT_SALT_PRESENT(1) |
	    V_KEY_CONTEXT_CK_SIZE(CHCR_KEYCTX_NO_KEY) |
	    V_KEY_CONTEXT_MK_SIZE(s->hmac.mk_size) | V_KEY_CONTEXT_VALID(1));

	dst = (char *)(crwr + 1) + kctx_len + DUMMY_BYTES;
	if (crp->crp_payload_length == 0) {
		dst[0] = 0x80;
		if (s->mode == HMAC)
			*(uint64_t *)(dst + axf->blocksize - sizeof(uint64_t)) =
			    htobe64(axf->blocksize << 3);
	} else if (imm_len != 0)
		crypto_copydata(crp, crp->crp_payload_start,
		    crp->crp_payload_length, dst);
	else
		ccr_write_ulptx_sgl(s, dst, sgl_nsegs);

	/* XXX: TODO backpressure */
	t4_wrq_tx(sc->adapter, wr);

	return (0);
}

static int
ccr_hash_done(struct ccr_softc *sc, struct ccr_session *s, struct cryptop *crp,
    const struct cpl_fw6_pld *cpl, int error)
{
	uint8_t hash[HASH_MAX_LEN];

	if (error)
		return (error);

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		crypto_copydata(crp, crp->crp_digest_start, s->hmac.hash_len,
		    hash);
		if (timingsafe_bcmp((cpl + 1), hash, s->hmac.hash_len) != 0)
			return (EBADMSG);
	} else
		crypto_copyback(crp, crp->crp_digest_start, s->hmac.hash_len,
		    (cpl + 1));
	return (0);
}

static int
ccr_blkcipher(struct ccr_softc *sc, struct ccr_session *s, struct cryptop *crp)
{
	char iv[CHCR_MAX_CRYPTO_IV_LEN];
	struct chcr_wr *crwr;
	struct wrqe *wr;
	char *dst;
	u_int kctx_len, key_half, op_type, transhdr_len, wr_len;
	u_int imm_len, iv_len;
	int dsgl_nsegs, dsgl_len;
	int sgl_nsegs, sgl_len;
	int error;

	if (s->blkcipher.key_len == 0 || crp->crp_payload_length == 0)
		return (EINVAL);
	if (s->blkcipher.cipher_mode == SCMD_CIPH_MODE_AES_CBC &&
	    (crp->crp_payload_length % AES_BLOCK_LEN) != 0)
		return (EINVAL);

	/* Reject requests with too large of an input buffer. */
	if (crp->crp_payload_length > MAX_REQUEST_SIZE)
		return (EFBIG);

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		op_type = CHCR_ENCRYPT_OP;
	else
		op_type = CHCR_DECRYPT_OP;

	sglist_reset(s->sg_dsgl);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
		error = sglist_append_sglist(s->sg_dsgl, s->sg_output,
		    crp->crp_payload_output_start, crp->crp_payload_length);
	else
		error = sglist_append_sglist(s->sg_dsgl, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
	if (error)
		return (error);
	dsgl_nsegs = ccr_count_sgl(s->sg_dsgl, DSGL_SGE_MAXLEN);
	if (dsgl_nsegs > MAX_RX_PHYS_DSGL_SGE)
		return (EFBIG);
	dsgl_len = ccr_phys_dsgl_len(dsgl_nsegs);

	/* The 'key' must be 128-bit aligned. */
	kctx_len = roundup2(s->blkcipher.key_len, 16);
	transhdr_len = CIPHER_TRANSHDR_SIZE(kctx_len, dsgl_len);

	/* For AES-XTS we send a 16-byte IV in the work request. */
	if (s->blkcipher.cipher_mode == SCMD_CIPH_MODE_AES_XTS)
		iv_len = AES_BLOCK_LEN;
	else
		iv_len = s->blkcipher.iv_len;

	if (ccr_use_imm_data(transhdr_len, crp->crp_payload_length + iv_len)) {
		imm_len = crp->crp_payload_length;
		sgl_nsegs = 0;
		sgl_len = 0;
	} else {
		imm_len = 0;
		sglist_reset(s->sg_ulptx);
		error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
		if (error)
			return (error);
		sgl_nsegs = s->sg_ulptx->sg_nseg;
		sgl_len = ccr_ulptx_sgl_len(sgl_nsegs);
	}

	wr_len = roundup2(transhdr_len, 16) + iv_len +
	    roundup2(imm_len, 16) + sgl_len;
	if (wr_len > SGE_MAX_WR_LEN)
		return (EFBIG);
	wr = alloc_wrqe(wr_len, s->port->txq);
	if (wr == NULL) {
		counter_u64_add(sc->stats_wr_nomem, 1);
		return (ENOMEM);
	}
	crwr = wrtod(wr);
	memset(crwr, 0, wr_len);

	crypto_read_iv(crp, iv);

	/* Zero the remainder of the IV for AES-XTS. */
	memset(iv + s->blkcipher.iv_len, 0, iv_len - s->blkcipher.iv_len);

	ccr_populate_wreq(sc, s, crwr, kctx_len, wr_len, imm_len, sgl_len, 0,
	    crp);

	crwr->sec_cpl.op_ivinsrtofst = htobe32(
	    V_CPL_TX_SEC_PDU_OPCODE(CPL_TX_SEC_PDU) |
	    V_CPL_TX_SEC_PDU_RXCHID(s->port->tx_channel_id) |
	    V_CPL_TX_SEC_PDU_ACKFOLLOWS(0) | V_CPL_TX_SEC_PDU_ULPTXLPBK(1) |
	    V_CPL_TX_SEC_PDU_CPLLEN(2) | V_CPL_TX_SEC_PDU_PLACEHOLDER(0) |
	    V_CPL_TX_SEC_PDU_IVINSRTOFST(1));

	crwr->sec_cpl.pldlen = htobe32(iv_len + crp->crp_payload_length);

	crwr->sec_cpl.aadstart_cipherstop_hi = htobe32(
	    V_CPL_TX_SEC_PDU_CIPHERSTART(iv_len + 1) |
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_HI(0));
	crwr->sec_cpl.cipherstop_lo_authinsert = htobe32(
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_LO(0));

	/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
	crwr->sec_cpl.seqno_numivs = htobe32(
	    V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_ENC_DEC_CTRL(op_type) |
	    V_SCMD_CIPH_MODE(s->blkcipher.cipher_mode) |
	    V_SCMD_AUTH_MODE(SCMD_AUTH_MODE_NOP) |
	    V_SCMD_HMAC_CTRL(SCMD_HMAC_CTRL_NOP) |
	    V_SCMD_IV_SIZE(iv_len / 2) |
	    V_SCMD_NUM_IVS(0));
	crwr->sec_cpl.ivgen_hdrlen = htobe32(
	    V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_MORE_FRAGS(0) | V_SCMD_LAST_FRAG(0) | V_SCMD_MAC_ONLY(0) |
	    V_SCMD_AADIVDROP(1) | V_SCMD_HDR_LEN(dsgl_len));

	crwr->key_ctx.ctx_hdr = s->blkcipher.key_ctx_hdr;
	switch (s->blkcipher.cipher_mode) {
	case SCMD_CIPH_MODE_AES_CBC:
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			memcpy(crwr->key_ctx.key, s->blkcipher.enckey,
			    s->blkcipher.key_len);
		else
			memcpy(crwr->key_ctx.key, s->blkcipher.deckey,
			    s->blkcipher.key_len);
		break;
	case SCMD_CIPH_MODE_AES_CTR:
		memcpy(crwr->key_ctx.key, s->blkcipher.enckey,
		    s->blkcipher.key_len);
		break;
	case SCMD_CIPH_MODE_AES_XTS:
		key_half = s->blkcipher.key_len / 2;
		memcpy(crwr->key_ctx.key, s->blkcipher.enckey + key_half,
		    key_half);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			memcpy(crwr->key_ctx.key + key_half,
			    s->blkcipher.enckey, key_half);
		else
			memcpy(crwr->key_ctx.key + key_half,
			    s->blkcipher.deckey, key_half);
		break;
	}

	dst = (char *)(crwr + 1) + kctx_len;
	ccr_write_phys_dsgl(s, dst, dsgl_nsegs);
	dst += sizeof(struct cpl_rx_phys_dsgl) + dsgl_len;
	memcpy(dst, iv, iv_len);
	dst += iv_len;
	if (imm_len != 0)
		crypto_copydata(crp, crp->crp_payload_start,
		    crp->crp_payload_length, dst);
	else
		ccr_write_ulptx_sgl(s, dst, sgl_nsegs);

	/* XXX: TODO backpressure */
	t4_wrq_tx(sc->adapter, wr);

	explicit_bzero(iv, sizeof(iv));
	return (0);
}

static int
ccr_blkcipher_done(struct ccr_softc *sc, struct ccr_session *s,
    struct cryptop *crp, const struct cpl_fw6_pld *cpl, int error)
{

	/*
	 * The updated IV to permit chained requests is at
	 * cpl->data[2], but OCF doesn't permit chained requests.
	 */
	return (error);
}

/*
 * 'hashsize' is the length of a full digest.  'authsize' is the
 * requested digest length for this operation which may be less
 * than 'hashsize'.
 */
static int
ccr_hmac_ctrl(unsigned int hashsize, unsigned int authsize)
{

	if (authsize == 10)
		return (SCMD_HMAC_CTRL_TRUNC_RFC4366);
	if (authsize == 12)
		return (SCMD_HMAC_CTRL_IPSEC_96BIT);
	if (authsize == hashsize / 2)
		return (SCMD_HMAC_CTRL_DIV2);
	return (SCMD_HMAC_CTRL_NO_TRUNC);
}

static int
ccr_eta(struct ccr_softc *sc, struct ccr_session *s, struct cryptop *crp)
{
	char iv[CHCR_MAX_CRYPTO_IV_LEN];
	struct chcr_wr *crwr;
	struct wrqe *wr;
	struct auth_hash *axf;
	char *dst;
	u_int kctx_len, key_half, op_type, transhdr_len, wr_len;
	u_int hash_size_in_response, imm_len, iopad_size, iv_len;
	u_int aad_start, aad_stop;
	u_int auth_insert;
	u_int cipher_start, cipher_stop;
	u_int hmac_ctrl, input_len;
	int dsgl_nsegs, dsgl_len;
	int sgl_nsegs, sgl_len;
	int error;

	/*
	 * If there is a need in the future, requests with an empty
	 * payload could be supported as HMAC-only requests.
	 */
	if (s->blkcipher.key_len == 0 || crp->crp_payload_length == 0)
		return (EINVAL);
	if (s->blkcipher.cipher_mode == SCMD_CIPH_MODE_AES_CBC &&
	    (crp->crp_payload_length % AES_BLOCK_LEN) != 0)
		return (EINVAL);

	/* For AES-XTS we send a 16-byte IV in the work request. */
	if (s->blkcipher.cipher_mode == SCMD_CIPH_MODE_AES_XTS)
		iv_len = AES_BLOCK_LEN;
	else
		iv_len = s->blkcipher.iv_len;

	if (crp->crp_aad_length + iv_len > MAX_AAD_LEN)
		return (EINVAL);

	axf = s->hmac.auth_hash;
	hash_size_in_response = s->hmac.hash_len;
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		op_type = CHCR_ENCRYPT_OP;
	else
		op_type = CHCR_DECRYPT_OP;

	/*
	 * The output buffer consists of the cipher text followed by
	 * the hash when encrypting.  For decryption it only contains
	 * the plain text.
	 *
	 * Due to a firmware bug, the output buffer must include a
	 * dummy output buffer for the IV and AAD prior to the real
	 * output buffer.
	 */
	if (op_type == CHCR_ENCRYPT_OP) {
		if (iv_len + crp->crp_aad_length + crp->crp_payload_length +
		    hash_size_in_response > MAX_REQUEST_SIZE)
			return (EFBIG);
	} else {
		if (iv_len + crp->crp_aad_length + crp->crp_payload_length >
		    MAX_REQUEST_SIZE)
			return (EFBIG);
	}
	sglist_reset(s->sg_dsgl);
	error = sglist_append_sglist(s->sg_dsgl, sc->sg_iv_aad, 0,
	    iv_len + crp->crp_aad_length);
	if (error)
		return (error);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
		error = sglist_append_sglist(s->sg_dsgl, s->sg_output,
		    crp->crp_payload_output_start, crp->crp_payload_length);
	else
		error = sglist_append_sglist(s->sg_dsgl, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
	if (error)
		return (error);
	if (op_type == CHCR_ENCRYPT_OP) {
		if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
			error = sglist_append_sglist(s->sg_dsgl, s->sg_output,
			    crp->crp_digest_start, hash_size_in_response);
		else
			error = sglist_append_sglist(s->sg_dsgl, s->sg_input,
			    crp->crp_digest_start, hash_size_in_response);
		if (error)
			return (error);
	}
	dsgl_nsegs = ccr_count_sgl(s->sg_dsgl, DSGL_SGE_MAXLEN);
	if (dsgl_nsegs > MAX_RX_PHYS_DSGL_SGE)
		return (EFBIG);
	dsgl_len = ccr_phys_dsgl_len(dsgl_nsegs);

	/* PADs must be 128-bit aligned. */
	iopad_size = roundup2(s->hmac.partial_digest_len, 16);

	/*
	 * The 'key' part of the key context consists of the key followed
	 * by the IPAD and OPAD.
	 */
	kctx_len = roundup2(s->blkcipher.key_len, 16) + iopad_size * 2;
	transhdr_len = CIPHER_TRANSHDR_SIZE(kctx_len, dsgl_len);

	/*
	 * The input buffer consists of the IV, any AAD, and then the
	 * cipher/plain text.  For decryption requests the hash is
	 * appended after the cipher text.
	 *
	 * The IV is always stored at the start of the input buffer
	 * even though it may be duplicated in the payload.  The
	 * crypto engine doesn't work properly if the IV offset points
	 * inside of the AAD region, so a second copy is always
	 * required.
	 */
	input_len = crp->crp_aad_length + crp->crp_payload_length;

	/*
	 * The firmware hangs if sent a request which is a
	 * bit smaller than MAX_REQUEST_SIZE.  In particular, the
	 * firmware appears to require 512 - 16 bytes of spare room
	 * along with the size of the hash even if the hash isn't
	 * included in the input buffer.
	 */
	if (input_len + roundup2(axf->hashsize, 16) + (512 - 16) >
	    MAX_REQUEST_SIZE)
		return (EFBIG);
	if (op_type == CHCR_DECRYPT_OP)
		input_len += hash_size_in_response;

	if (ccr_use_imm_data(transhdr_len, iv_len + input_len)) {
		imm_len = input_len;
		sgl_nsegs = 0;
		sgl_len = 0;
	} else {
		imm_len = 0;
		sglist_reset(s->sg_ulptx);
		if (crp->crp_aad_length != 0) {
			if (crp->crp_aad != NULL)
				error = sglist_append(s->sg_ulptx,
				    crp->crp_aad, crp->crp_aad_length);
			else
				error = sglist_append_sglist(s->sg_ulptx,
				    s->sg_input, crp->crp_aad_start,
				    crp->crp_aad_length);
			if (error)
				return (error);
		}
		error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
		if (error)
			return (error);
		if (op_type == CHCR_DECRYPT_OP) {
			error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
			    crp->crp_digest_start, hash_size_in_response);
			if (error)
				return (error);
		}
		sgl_nsegs = s->sg_ulptx->sg_nseg;
		sgl_len = ccr_ulptx_sgl_len(sgl_nsegs);
	}

	/* Any AAD comes after the IV. */
	if (crp->crp_aad_length != 0) {
		aad_start = iv_len + 1;
		aad_stop = aad_start + crp->crp_aad_length - 1;
	} else {
		aad_start = 0;
		aad_stop = 0;
	}
	cipher_start = iv_len + crp->crp_aad_length + 1;
	if (op_type == CHCR_DECRYPT_OP)
		cipher_stop = hash_size_in_response;
	else
		cipher_stop = 0;
	if (op_type == CHCR_DECRYPT_OP)
		auth_insert = hash_size_in_response;
	else
		auth_insert = 0;

	wr_len = roundup2(transhdr_len, 16) + iv_len + roundup2(imm_len, 16) +
	    sgl_len;
	if (wr_len > SGE_MAX_WR_LEN)
		return (EFBIG);
	wr = alloc_wrqe(wr_len, s->port->txq);
	if (wr == NULL) {
		counter_u64_add(sc->stats_wr_nomem, 1);
		return (ENOMEM);
	}
	crwr = wrtod(wr);
	memset(crwr, 0, wr_len);

	crypto_read_iv(crp, iv);

	/* Zero the remainder of the IV for AES-XTS. */
	memset(iv + s->blkcipher.iv_len, 0, iv_len - s->blkcipher.iv_len);

	ccr_populate_wreq(sc, s, crwr, kctx_len, wr_len, imm_len, sgl_len,
	    op_type == CHCR_DECRYPT_OP ? hash_size_in_response : 0, crp);

	crwr->sec_cpl.op_ivinsrtofst = htobe32(
	    V_CPL_TX_SEC_PDU_OPCODE(CPL_TX_SEC_PDU) |
	    V_CPL_TX_SEC_PDU_RXCHID(s->port->tx_channel_id) |
	    V_CPL_TX_SEC_PDU_ACKFOLLOWS(0) | V_CPL_TX_SEC_PDU_ULPTXLPBK(1) |
	    V_CPL_TX_SEC_PDU_CPLLEN(2) | V_CPL_TX_SEC_PDU_PLACEHOLDER(0) |
	    V_CPL_TX_SEC_PDU_IVINSRTOFST(1));

	crwr->sec_cpl.pldlen = htobe32(iv_len + input_len);

	crwr->sec_cpl.aadstart_cipherstop_hi = htobe32(
	    V_CPL_TX_SEC_PDU_AADSTART(aad_start) |
	    V_CPL_TX_SEC_PDU_AADSTOP(aad_stop) |
	    V_CPL_TX_SEC_PDU_CIPHERSTART(cipher_start) |
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_HI(cipher_stop >> 4));
	crwr->sec_cpl.cipherstop_lo_authinsert = htobe32(
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_LO(cipher_stop & 0xf) |
	    V_CPL_TX_SEC_PDU_AUTHSTART(cipher_start) |
	    V_CPL_TX_SEC_PDU_AUTHSTOP(cipher_stop) |
	    V_CPL_TX_SEC_PDU_AUTHINSERT(auth_insert));

	/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
	hmac_ctrl = ccr_hmac_ctrl(axf->hashsize, hash_size_in_response);
	crwr->sec_cpl.seqno_numivs = htobe32(
	    V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_ENC_DEC_CTRL(op_type) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL(op_type == CHCR_ENCRYPT_OP ? 1 : 0) |
	    V_SCMD_CIPH_MODE(s->blkcipher.cipher_mode) |
	    V_SCMD_AUTH_MODE(s->hmac.auth_mode) |
	    V_SCMD_HMAC_CTRL(hmac_ctrl) |
	    V_SCMD_IV_SIZE(iv_len / 2) |
	    V_SCMD_NUM_IVS(0));
	crwr->sec_cpl.ivgen_hdrlen = htobe32(
	    V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_MORE_FRAGS(0) | V_SCMD_LAST_FRAG(0) | V_SCMD_MAC_ONLY(0) |
	    V_SCMD_AADIVDROP(0) | V_SCMD_HDR_LEN(dsgl_len));

	crwr->key_ctx.ctx_hdr = s->blkcipher.key_ctx_hdr;
	switch (s->blkcipher.cipher_mode) {
	case SCMD_CIPH_MODE_AES_CBC:
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			memcpy(crwr->key_ctx.key, s->blkcipher.enckey,
			    s->blkcipher.key_len);
		else
			memcpy(crwr->key_ctx.key, s->blkcipher.deckey,
			    s->blkcipher.key_len);
		break;
	case SCMD_CIPH_MODE_AES_CTR:
		memcpy(crwr->key_ctx.key, s->blkcipher.enckey,
		    s->blkcipher.key_len);
		break;
	case SCMD_CIPH_MODE_AES_XTS:
		key_half = s->blkcipher.key_len / 2;
		memcpy(crwr->key_ctx.key, s->blkcipher.enckey + key_half,
		    key_half);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			memcpy(crwr->key_ctx.key + key_half,
			    s->blkcipher.enckey, key_half);
		else
			memcpy(crwr->key_ctx.key + key_half,
			    s->blkcipher.deckey, key_half);
		break;
	}

	dst = crwr->key_ctx.key + roundup2(s->blkcipher.key_len, 16);
	memcpy(dst, s->hmac.pads, iopad_size * 2);

	dst = (char *)(crwr + 1) + kctx_len;
	ccr_write_phys_dsgl(s, dst, dsgl_nsegs);
	dst += sizeof(struct cpl_rx_phys_dsgl) + dsgl_len;
	memcpy(dst, iv, iv_len);
	dst += iv_len;
	if (imm_len != 0) {
		if (crp->crp_aad_length != 0) {
			if (crp->crp_aad != NULL)
				memcpy(dst, crp->crp_aad, crp->crp_aad_length);
			else
				crypto_copydata(crp, crp->crp_aad_start,
				    crp->crp_aad_length, dst);
			dst += crp->crp_aad_length;
		}
		crypto_copydata(crp, crp->crp_payload_start,
		    crp->crp_payload_length, dst);
		dst += crp->crp_payload_length;
		if (op_type == CHCR_DECRYPT_OP)
			crypto_copydata(crp, crp->crp_digest_start,
			    hash_size_in_response, dst);
	} else
		ccr_write_ulptx_sgl(s, dst, sgl_nsegs);

	/* XXX: TODO backpressure */
	t4_wrq_tx(sc->adapter, wr);

	explicit_bzero(iv, sizeof(iv));
	return (0);
}

static int
ccr_eta_done(struct ccr_softc *sc, struct ccr_session *s,
    struct cryptop *crp, const struct cpl_fw6_pld *cpl, int error)
{

	/*
	 * The updated IV to permit chained requests is at
	 * cpl->data[2], but OCF doesn't permit chained requests.
	 */
	return (error);
}

static int
ccr_gcm(struct ccr_softc *sc, struct ccr_session *s, struct cryptop *crp)
{
	char iv[CHCR_MAX_CRYPTO_IV_LEN];
	struct chcr_wr *crwr;
	struct wrqe *wr;
	char *dst;
	u_int iv_len, kctx_len, op_type, transhdr_len, wr_len;
	u_int hash_size_in_response, imm_len;
	u_int aad_start, aad_stop, cipher_start, cipher_stop, auth_insert;
	u_int hmac_ctrl, input_len;
	int dsgl_nsegs, dsgl_len;
	int sgl_nsegs, sgl_len;
	int error;

	if (s->blkcipher.key_len == 0)
		return (EINVAL);

	/*
	 * The crypto engine doesn't handle GCM requests with an empty
	 * payload, so handle those in software instead.
	 */
	if (crp->crp_payload_length == 0)
		return (EMSGSIZE);

	if (crp->crp_aad_length + AES_BLOCK_LEN > MAX_AAD_LEN)
		return (EMSGSIZE);

	hash_size_in_response = s->gmac.hash_len;
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		op_type = CHCR_ENCRYPT_OP;
	else
		op_type = CHCR_DECRYPT_OP;

	/*
	 * The IV handling for GCM in OCF is a bit more complicated in
	 * that IPSec provides a full 16-byte IV (including the
	 * counter), whereas the /dev/crypto interface sometimes
	 * provides a full 16-byte IV (if no IV is provided in the
	 * ioctl) and sometimes a 12-byte IV (if the IV was explicit).
	 *
	 * When provided a 12-byte IV, assume the IV is really 16 bytes
	 * with a counter in the last 4 bytes initialized to 1.
	 *
	 * While iv_len is checked below, the value is currently
	 * always set to 12 when creating a GCM session in this driver
	 * due to limitations in OCF (there is no way to know what the
	 * IV length of a given request will be).  This means that the
	 * driver always assumes as 12-byte IV for now.
	 */
	if (s->blkcipher.iv_len == 12)
		iv_len = AES_BLOCK_LEN;
	else
		iv_len = s->blkcipher.iv_len;

	/*
	 * GCM requests should always provide an explicit IV.
	 */
	if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
		return (EINVAL);

	/*
	 * The output buffer consists of the cipher text followed by
	 * the tag when encrypting.  For decryption it only contains
	 * the plain text.
	 *
	 * Due to a firmware bug, the output buffer must include a
	 * dummy output buffer for the IV and AAD prior to the real
	 * output buffer.
	 */
	if (op_type == CHCR_ENCRYPT_OP) {
		if (iv_len + crp->crp_aad_length + crp->crp_payload_length +
		    hash_size_in_response > MAX_REQUEST_SIZE)
			return (EFBIG);
	} else {
		if (iv_len + crp->crp_aad_length + crp->crp_payload_length >
		    MAX_REQUEST_SIZE)
			return (EFBIG);
	}
	sglist_reset(s->sg_dsgl);
	error = sglist_append_sglist(s->sg_dsgl, sc->sg_iv_aad, 0, iv_len +
	    crp->crp_aad_length);
	if (error)
		return (error);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
		error = sglist_append_sglist(s->sg_dsgl, s->sg_output,
		    crp->crp_payload_output_start, crp->crp_payload_length);
	else
		error = sglist_append_sglist(s->sg_dsgl, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
	if (error)
		return (error);
	if (op_type == CHCR_ENCRYPT_OP) {
		if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
			error = sglist_append_sglist(s->sg_dsgl, s->sg_output,
			    crp->crp_digest_start, hash_size_in_response);
		else
			error = sglist_append_sglist(s->sg_dsgl, s->sg_input,
			    crp->crp_digest_start, hash_size_in_response);
		if (error)
			return (error);
	}
	dsgl_nsegs = ccr_count_sgl(s->sg_dsgl, DSGL_SGE_MAXLEN);
	if (dsgl_nsegs > MAX_RX_PHYS_DSGL_SGE)
		return (EFBIG);
	dsgl_len = ccr_phys_dsgl_len(dsgl_nsegs);

	/*
	 * The 'key' part of the key context consists of the key followed
	 * by the Galois hash key.
	 */
	kctx_len = roundup2(s->blkcipher.key_len, 16) + GMAC_BLOCK_LEN;
	transhdr_len = CIPHER_TRANSHDR_SIZE(kctx_len, dsgl_len);

	/*
	 * The input buffer consists of the IV, any AAD, and then the
	 * cipher/plain text.  For decryption requests the hash is
	 * appended after the cipher text.
	 *
	 * The IV is always stored at the start of the input buffer
	 * even though it may be duplicated in the payload.  The
	 * crypto engine doesn't work properly if the IV offset points
	 * inside of the AAD region, so a second copy is always
	 * required.
	 */
	input_len = crp->crp_aad_length + crp->crp_payload_length;
	if (op_type == CHCR_DECRYPT_OP)
		input_len += hash_size_in_response;
	if (input_len > MAX_REQUEST_SIZE)
		return (EFBIG);
	if (ccr_use_imm_data(transhdr_len, iv_len + input_len)) {
		imm_len = input_len;
		sgl_nsegs = 0;
		sgl_len = 0;
	} else {
		imm_len = 0;
		sglist_reset(s->sg_ulptx);
		if (crp->crp_aad_length != 0) {
			if (crp->crp_aad != NULL)
				error = sglist_append(s->sg_ulptx,
				    crp->crp_aad, crp->crp_aad_length);
			else
				error = sglist_append_sglist(s->sg_ulptx,
				    s->sg_input, crp->crp_aad_start,
				    crp->crp_aad_length);
			if (error)
				return (error);
		}
		error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
		if (error)
			return (error);
		if (op_type == CHCR_DECRYPT_OP) {
			error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
			    crp->crp_digest_start, hash_size_in_response);
			if (error)
				return (error);
		}
		sgl_nsegs = s->sg_ulptx->sg_nseg;
		sgl_len = ccr_ulptx_sgl_len(sgl_nsegs);
	}

	if (crp->crp_aad_length != 0) {
		aad_start = iv_len + 1;
		aad_stop = aad_start + crp->crp_aad_length - 1;
	} else {
		aad_start = 0;
		aad_stop = 0;
	}
	cipher_start = iv_len + crp->crp_aad_length + 1;
	if (op_type == CHCR_DECRYPT_OP)
		cipher_stop = hash_size_in_response;
	else
		cipher_stop = 0;
	if (op_type == CHCR_DECRYPT_OP)
		auth_insert = hash_size_in_response;
	else
		auth_insert = 0;

	wr_len = roundup2(transhdr_len, 16) + iv_len + roundup2(imm_len, 16) +
	    sgl_len;
	if (wr_len > SGE_MAX_WR_LEN)
		return (EFBIG);
	wr = alloc_wrqe(wr_len, s->port->txq);
	if (wr == NULL) {
		counter_u64_add(sc->stats_wr_nomem, 1);
		return (ENOMEM);
	}
	crwr = wrtod(wr);
	memset(crwr, 0, wr_len);

	memcpy(iv, crp->crp_iv, s->blkcipher.iv_len);
	if (s->blkcipher.iv_len == 12)
		*(uint32_t *)&iv[12] = htobe32(1);

	ccr_populate_wreq(sc, s, crwr, kctx_len, wr_len, imm_len, sgl_len, 0,
	    crp);

	crwr->sec_cpl.op_ivinsrtofst = htobe32(
	    V_CPL_TX_SEC_PDU_OPCODE(CPL_TX_SEC_PDU) |
	    V_CPL_TX_SEC_PDU_RXCHID(s->port->tx_channel_id) |
	    V_CPL_TX_SEC_PDU_ACKFOLLOWS(0) | V_CPL_TX_SEC_PDU_ULPTXLPBK(1) |
	    V_CPL_TX_SEC_PDU_CPLLEN(2) | V_CPL_TX_SEC_PDU_PLACEHOLDER(0) |
	    V_CPL_TX_SEC_PDU_IVINSRTOFST(1));

	crwr->sec_cpl.pldlen = htobe32(iv_len + input_len);

	/*
	 * NB: cipherstop is explicitly set to 0.  On encrypt it
	 * should normally be set to 0 anyway.  However, for decrypt
	 * the cipher ends before the tag in the ETA case (and
	 * authstop is set to stop before the tag), but for GCM the
	 * cipher still runs to the end of the buffer.  Not sure if
	 * this is intentional or a firmware quirk, but it is required
	 * for working tag validation with GCM decryption.
	 */
	crwr->sec_cpl.aadstart_cipherstop_hi = htobe32(
	    V_CPL_TX_SEC_PDU_AADSTART(aad_start) |
	    V_CPL_TX_SEC_PDU_AADSTOP(aad_stop) |
	    V_CPL_TX_SEC_PDU_CIPHERSTART(cipher_start) |
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_HI(0));
	crwr->sec_cpl.cipherstop_lo_authinsert = htobe32(
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_LO(0) |
	    V_CPL_TX_SEC_PDU_AUTHSTART(cipher_start) |
	    V_CPL_TX_SEC_PDU_AUTHSTOP(cipher_stop) |
	    V_CPL_TX_SEC_PDU_AUTHINSERT(auth_insert));

	/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
	hmac_ctrl = ccr_hmac_ctrl(AES_GMAC_HASH_LEN, hash_size_in_response);
	crwr->sec_cpl.seqno_numivs = htobe32(
	    V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_ENC_DEC_CTRL(op_type) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL(op_type == CHCR_ENCRYPT_OP ? 1 : 0) |
	    V_SCMD_CIPH_MODE(SCMD_CIPH_MODE_AES_GCM) |
	    V_SCMD_AUTH_MODE(SCMD_AUTH_MODE_GHASH) |
	    V_SCMD_HMAC_CTRL(hmac_ctrl) |
	    V_SCMD_IV_SIZE(iv_len / 2) |
	    V_SCMD_NUM_IVS(0));
	crwr->sec_cpl.ivgen_hdrlen = htobe32(
	    V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_MORE_FRAGS(0) | V_SCMD_LAST_FRAG(0) | V_SCMD_MAC_ONLY(0) |
	    V_SCMD_AADIVDROP(0) | V_SCMD_HDR_LEN(dsgl_len));

	crwr->key_ctx.ctx_hdr = s->blkcipher.key_ctx_hdr;
	memcpy(crwr->key_ctx.key, s->blkcipher.enckey, s->blkcipher.key_len);
	dst = crwr->key_ctx.key + roundup2(s->blkcipher.key_len, 16);
	memcpy(dst, s->gmac.ghash_h, GMAC_BLOCK_LEN);

	dst = (char *)(crwr + 1) + kctx_len;
	ccr_write_phys_dsgl(s, dst, dsgl_nsegs);
	dst += sizeof(struct cpl_rx_phys_dsgl) + dsgl_len;
	memcpy(dst, iv, iv_len);
	dst += iv_len;
	if (imm_len != 0) {
		if (crp->crp_aad_length != 0) {
			if (crp->crp_aad != NULL)
				memcpy(dst, crp->crp_aad, crp->crp_aad_length);
			else
				crypto_copydata(crp, crp->crp_aad_start,
				    crp->crp_aad_length, dst);
			dst += crp->crp_aad_length;
		}
		crypto_copydata(crp, crp->crp_payload_start,
		    crp->crp_payload_length, dst);
		dst += crp->crp_payload_length;
		if (op_type == CHCR_DECRYPT_OP)
			crypto_copydata(crp, crp->crp_digest_start,
			    hash_size_in_response, dst);
	} else
		ccr_write_ulptx_sgl(s, dst, sgl_nsegs);

	/* XXX: TODO backpressure */
	t4_wrq_tx(sc->adapter, wr);

	explicit_bzero(iv, sizeof(iv));
	return (0);
}

static int
ccr_gcm_done(struct ccr_softc *sc, struct ccr_session *s,
    struct cryptop *crp, const struct cpl_fw6_pld *cpl, int error)
{

	/*
	 * The updated IV to permit chained requests is at
	 * cpl->data[2], but OCF doesn't permit chained requests.
	 *
	 * Note that the hardware should always verify the GMAC hash.
	 */
	return (error);
}

/*
 * Handle a GCM request that is not supported by the crypto engine by
 * performing the operation in software.  Derived from swcr_authenc().
 */
static void
ccr_gcm_soft(struct ccr_session *s, struct cryptop *crp)
{
	struct auth_hash *axf;
	struct enc_xform *exf;
	void *auth_ctx, *kschedule;
	char block[GMAC_BLOCK_LEN];
	char digest[GMAC_DIGEST_LEN];
	char iv[AES_BLOCK_LEN];
	int error, i, len;

	auth_ctx = NULL;
	kschedule = NULL;

	/* Initialize the MAC. */
	switch (s->blkcipher.key_len) {
	case 16:
		axf = &auth_hash_nist_gmac_aes_128;
		break;
	case 24:
		axf = &auth_hash_nist_gmac_aes_192;
		break;
	case 32:
		axf = &auth_hash_nist_gmac_aes_256;
		break;
	default:
		error = EINVAL;
		goto out;
	}
	auth_ctx = malloc(axf->ctxsize, M_CCR, M_NOWAIT);
	if (auth_ctx == NULL) {
		error = ENOMEM;
		goto out;
	}
	axf->Init(auth_ctx);
	axf->Setkey(auth_ctx, s->blkcipher.enckey, s->blkcipher.key_len);

	/* Initialize the cipher. */
	exf = &enc_xform_aes_nist_gcm;
	kschedule = malloc(exf->ctxsize, M_CCR, M_NOWAIT);
	if (kschedule == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = exf->setkey(kschedule, s->blkcipher.enckey,
	    s->blkcipher.key_len);
	if (error)
		goto out;

	/*
	 * This assumes a 12-byte IV from the crp.  See longer comment
	 * above in ccr_gcm() for more details.
	 */
	if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0) {
		error = EINVAL;
		goto out;
	}
	memcpy(iv, crp->crp_iv, 12);
	*(uint32_t *)&iv[12] = htobe32(1);

	axf->Reinit(auth_ctx, iv, sizeof(iv));

	/* MAC the AAD. */
	if (crp->crp_aad != NULL) {
		len = rounddown(crp->crp_aad_length, sizeof(block));
		if (len != 0)
			axf->Update(auth_ctx, crp->crp_aad, len);
		if (crp->crp_aad_length != len) {
			memset(block, 0, sizeof(block));
			memcpy(block, (char *)crp->crp_aad + len,
			    crp->crp_aad_length - len);
			axf->Update(auth_ctx, block, sizeof(block));
		}
	} else {
		for (i = 0; i < crp->crp_aad_length; i += sizeof(block)) {
			len = imin(crp->crp_aad_length - i, sizeof(block));
			crypto_copydata(crp, crp->crp_aad_start + i, len,
			    block);
			bzero(block + len, sizeof(block) - len);
			axf->Update(auth_ctx, block, sizeof(block));
		}
	}

	exf->reinit(kschedule, iv);

	/* Do encryption with MAC */
	for (i = 0; i < crp->crp_payload_length; i += sizeof(block)) {
		len = imin(crp->crp_payload_length - i, sizeof(block));
		crypto_copydata(crp, crp->crp_payload_start + i, len, block);
		bzero(block + len, sizeof(block) - len);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			exf->encrypt(kschedule, block, block);
			axf->Update(auth_ctx, block, len);
			crypto_copyback(crp, crp->crp_payload_start + i, len,
			    block);
		} else {
			axf->Update(auth_ctx, block, len);
		}
	}

	/* Length block. */
	bzero(block, sizeof(block));
	((uint32_t *)block)[1] = htobe32(crp->crp_aad_length * 8);
	((uint32_t *)block)[3] = htobe32(crp->crp_payload_length * 8);
	axf->Update(auth_ctx, block, sizeof(block));

	/* Finalize MAC. */
	axf->Final(digest, auth_ctx);

	/* Inject or validate tag. */
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		crypto_copyback(crp, crp->crp_digest_start, sizeof(digest),
		    digest);
		error = 0;
	} else {
		char digest2[GMAC_DIGEST_LEN];

		crypto_copydata(crp, crp->crp_digest_start, sizeof(digest2),
		    digest2);
		if (timingsafe_bcmp(digest, digest2, sizeof(digest)) == 0) {
			error = 0;

			/* Tag matches, decrypt data. */
			for (i = 0; i < crp->crp_payload_length;
			     i += sizeof(block)) {
				len = imin(crp->crp_payload_length - i,
				    sizeof(block));
				crypto_copydata(crp, crp->crp_payload_start + i,
				    len, block);
				bzero(block + len, sizeof(block) - len);
				exf->decrypt(kschedule, block, block);
				crypto_copyback(crp, crp->crp_payload_start + i,
				    len, block);
			}
		} else
			error = EBADMSG;
		explicit_bzero(digest2, sizeof(digest2));
	}

out:
	zfree(kschedule, M_CCR);
	zfree(auth_ctx, M_CCR);
	explicit_bzero(block, sizeof(block));
	explicit_bzero(iv, sizeof(iv));
	explicit_bzero(digest, sizeof(digest));
	crp->crp_etype = error;
	crypto_done(crp);
}

static void
generate_ccm_b0(struct cryptop *crp, u_int hash_size_in_response,
    const char *iv, char *b0)
{
	u_int i, payload_len;

	/* NB: L is already set in the first byte of the IV. */
	memcpy(b0, iv, CCM_B0_SIZE);

	/* Set length of hash in bits 3 - 5. */
	b0[0] |= (((hash_size_in_response - 2) / 2) << 3);

	/* Store the payload length as a big-endian value. */
	payload_len = crp->crp_payload_length;
	for (i = 0; i < iv[0]; i++) {
		b0[CCM_CBC_BLOCK_LEN - 1 - i] = payload_len;
		payload_len >>= 8;
	}

	/*
	 * If there is AAD in the request, set bit 6 in the flags
	 * field and store the AAD length as a big-endian value at the
	 * start of block 1.  This only assumes a 16-bit AAD length
	 * since T6 doesn't support large AAD sizes.
	 */
	if (crp->crp_aad_length != 0) {
		b0[0] |= (1 << 6);
		*(uint16_t *)(b0 + CCM_B0_SIZE) = htobe16(crp->crp_aad_length);
	}
}

static int
ccr_ccm(struct ccr_softc *sc, struct ccr_session *s, struct cryptop *crp)
{
	char iv[CHCR_MAX_CRYPTO_IV_LEN];
	struct ulptx_idata *idata;
	struct chcr_wr *crwr;
	struct wrqe *wr;
	char *dst;
	u_int iv_len, kctx_len, op_type, transhdr_len, wr_len;
	u_int aad_len, b0_len, hash_size_in_response, imm_len;
	u_int aad_start, aad_stop, cipher_start, cipher_stop, auth_insert;
	u_int hmac_ctrl, input_len;
	int dsgl_nsegs, dsgl_len;
	int sgl_nsegs, sgl_len;
	int error;

	if (s->blkcipher.key_len == 0)
		return (EINVAL);

	/*
	 * The crypto engine doesn't handle CCM requests with an empty
	 * payload, so handle those in software instead.
	 */
	if (crp->crp_payload_length == 0)
		return (EMSGSIZE);

	/*
	 * CCM always includes block 0 in the AAD before AAD from the
	 * request.
	 */
	b0_len = CCM_B0_SIZE;
	if (crp->crp_aad_length != 0)
		b0_len += CCM_AAD_FIELD_SIZE;
	aad_len = b0_len + crp->crp_aad_length;

	/*
	 * CCM requests should always provide an explicit IV (really
	 * the nonce).
	 */
	if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
		return (EINVAL);

	/*
	 * Always assume a 12 byte input nonce for now since that is
	 * what OCF always generates.  The full IV in the work request
	 * is 16 bytes.
	 */
	iv_len = AES_BLOCK_LEN;

	if (iv_len + aad_len > MAX_AAD_LEN)
		return (EMSGSIZE);

	hash_size_in_response = s->ccm_mac.hash_len;
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		op_type = CHCR_ENCRYPT_OP;
	else
		op_type = CHCR_DECRYPT_OP;

	/*
	 * The output buffer consists of the cipher text followed by
	 * the tag when encrypting.  For decryption it only contains
	 * the plain text.
	 *
	 * Due to a firmware bug, the output buffer must include a
	 * dummy output buffer for the IV and AAD prior to the real
	 * output buffer.
	 */
	if (op_type == CHCR_ENCRYPT_OP) {
		if (iv_len + aad_len + crp->crp_payload_length +
		    hash_size_in_response > MAX_REQUEST_SIZE)
			return (EFBIG);
	} else {
		if (iv_len + aad_len + crp->crp_payload_length >
		    MAX_REQUEST_SIZE)
			return (EFBIG);
	}
	sglist_reset(s->sg_dsgl);
	error = sglist_append_sglist(s->sg_dsgl, sc->sg_iv_aad, 0, iv_len +
	    aad_len);
	if (error)
		return (error);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
		error = sglist_append_sglist(s->sg_dsgl, s->sg_output,
		    crp->crp_payload_output_start, crp->crp_payload_length);
	else
		error = sglist_append_sglist(s->sg_dsgl, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
	if (error)
		return (error);
	if (op_type == CHCR_ENCRYPT_OP) {
		if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
			error = sglist_append_sglist(s->sg_dsgl, s->sg_output,
			    crp->crp_digest_start, hash_size_in_response);
		else
			error = sglist_append_sglist(s->sg_dsgl, s->sg_input,
			    crp->crp_digest_start, hash_size_in_response);
		if (error)
			return (error);
	}
	dsgl_nsegs = ccr_count_sgl(s->sg_dsgl, DSGL_SGE_MAXLEN);
	if (dsgl_nsegs > MAX_RX_PHYS_DSGL_SGE)
		return (EFBIG);
	dsgl_len = ccr_phys_dsgl_len(dsgl_nsegs);

	/*
	 * The 'key' part of the key context consists of two copies of
	 * the AES key.
	 */
	kctx_len = roundup2(s->blkcipher.key_len, 16) * 2;
	transhdr_len = CIPHER_TRANSHDR_SIZE(kctx_len, dsgl_len);

	/*
	 * The input buffer consists of the IV, AAD (including block
	 * 0), and then the cipher/plain text.  For decryption
	 * requests the hash is appended after the cipher text.
	 *
	 * The IV is always stored at the start of the input buffer
	 * even though it may be duplicated in the payload.  The
	 * crypto engine doesn't work properly if the IV offset points
	 * inside of the AAD region, so a second copy is always
	 * required.
	 */
	input_len = aad_len + crp->crp_payload_length;
	if (op_type == CHCR_DECRYPT_OP)
		input_len += hash_size_in_response;
	if (input_len > MAX_REQUEST_SIZE)
		return (EFBIG);
	if (ccr_use_imm_data(transhdr_len, iv_len + input_len)) {
		imm_len = input_len;
		sgl_nsegs = 0;
		sgl_len = 0;
	} else {
		/* Block 0 is passed as immediate data. */
		imm_len = b0_len;

		sglist_reset(s->sg_ulptx);
		if (crp->crp_aad_length != 0) {
			if (crp->crp_aad != NULL)
				error = sglist_append(s->sg_ulptx,
				    crp->crp_aad, crp->crp_aad_length);
			else
				error = sglist_append_sglist(s->sg_ulptx,
				    s->sg_input, crp->crp_aad_start,
				    crp->crp_aad_length);
			if (error)
				return (error);
		}
		error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
		    crp->crp_payload_start, crp->crp_payload_length);
		if (error)
			return (error);
		if (op_type == CHCR_DECRYPT_OP) {
			error = sglist_append_sglist(s->sg_ulptx, s->sg_input,
			    crp->crp_digest_start, hash_size_in_response);
			if (error)
				return (error);
		}
		sgl_nsegs = s->sg_ulptx->sg_nseg;
		sgl_len = ccr_ulptx_sgl_len(sgl_nsegs);
	}

	aad_start = iv_len + 1;
	aad_stop = aad_start + aad_len - 1;
	cipher_start = aad_stop + 1;
	if (op_type == CHCR_DECRYPT_OP)
		cipher_stop = hash_size_in_response;
	else
		cipher_stop = 0;
	if (op_type == CHCR_DECRYPT_OP)
		auth_insert = hash_size_in_response;
	else
		auth_insert = 0;

	wr_len = roundup2(transhdr_len, 16) + iv_len + roundup2(imm_len, 16) +
	    sgl_len;
	if (wr_len > SGE_MAX_WR_LEN)
		return (EFBIG);
	wr = alloc_wrqe(wr_len, s->port->txq);
	if (wr == NULL) {
		counter_u64_add(sc->stats_wr_nomem, 1);
		return (ENOMEM);
	}
	crwr = wrtod(wr);
	memset(crwr, 0, wr_len);

	/*
	 * Read the nonce from the request.  Use the nonce to generate
	 * the full IV with the counter set to 0.
	 */
	memset(iv, 0, iv_len);
	iv[0] = (15 - AES_CCM_IV_LEN) - 1;
	memcpy(iv + 1, crp->crp_iv, AES_CCM_IV_LEN);

	ccr_populate_wreq(sc, s, crwr, kctx_len, wr_len, imm_len, sgl_len, 0,
	    crp);

	crwr->sec_cpl.op_ivinsrtofst = htobe32(
	    V_CPL_TX_SEC_PDU_OPCODE(CPL_TX_SEC_PDU) |
	    V_CPL_TX_SEC_PDU_RXCHID(s->port->tx_channel_id) |
	    V_CPL_TX_SEC_PDU_ACKFOLLOWS(0) | V_CPL_TX_SEC_PDU_ULPTXLPBK(1) |
	    V_CPL_TX_SEC_PDU_CPLLEN(2) | V_CPL_TX_SEC_PDU_PLACEHOLDER(0) |
	    V_CPL_TX_SEC_PDU_IVINSRTOFST(1));

	crwr->sec_cpl.pldlen = htobe32(iv_len + input_len);

	/*
	 * NB: cipherstop is explicitly set to 0.  See comments above
	 * in ccr_gcm().
	 */
	crwr->sec_cpl.aadstart_cipherstop_hi = htobe32(
	    V_CPL_TX_SEC_PDU_AADSTART(aad_start) |
	    V_CPL_TX_SEC_PDU_AADSTOP(aad_stop) |
	    V_CPL_TX_SEC_PDU_CIPHERSTART(cipher_start) |
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_HI(0));
	crwr->sec_cpl.cipherstop_lo_authinsert = htobe32(
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_LO(0) |
	    V_CPL_TX_SEC_PDU_AUTHSTART(cipher_start) |
	    V_CPL_TX_SEC_PDU_AUTHSTOP(cipher_stop) |
	    V_CPL_TX_SEC_PDU_AUTHINSERT(auth_insert));

	/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
	hmac_ctrl = ccr_hmac_ctrl(AES_CBC_MAC_HASH_LEN, hash_size_in_response);
	crwr->sec_cpl.seqno_numivs = htobe32(
	    V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_ENC_DEC_CTRL(op_type) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL(op_type == CHCR_ENCRYPT_OP ? 0 : 1) |
	    V_SCMD_CIPH_MODE(SCMD_CIPH_MODE_AES_CCM) |
	    V_SCMD_AUTH_MODE(SCMD_AUTH_MODE_CBCMAC) |
	    V_SCMD_HMAC_CTRL(hmac_ctrl) |
	    V_SCMD_IV_SIZE(iv_len / 2) |
	    V_SCMD_NUM_IVS(0));
	crwr->sec_cpl.ivgen_hdrlen = htobe32(
	    V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_MORE_FRAGS(0) | V_SCMD_LAST_FRAG(0) | V_SCMD_MAC_ONLY(0) |
	    V_SCMD_AADIVDROP(0) | V_SCMD_HDR_LEN(dsgl_len));

	crwr->key_ctx.ctx_hdr = s->blkcipher.key_ctx_hdr;
	memcpy(crwr->key_ctx.key, s->blkcipher.enckey, s->blkcipher.key_len);
	memcpy(crwr->key_ctx.key + roundup(s->blkcipher.key_len, 16),
	    s->blkcipher.enckey, s->blkcipher.key_len);

	dst = (char *)(crwr + 1) + kctx_len;
	ccr_write_phys_dsgl(s, dst, dsgl_nsegs);
	dst += sizeof(struct cpl_rx_phys_dsgl) + dsgl_len;
	memcpy(dst, iv, iv_len);
	dst += iv_len;
	generate_ccm_b0(crp, hash_size_in_response, iv, dst);
	if (sgl_nsegs == 0) {
		dst += b0_len;
		if (crp->crp_aad_length != 0) {
			if (crp->crp_aad != NULL)
				memcpy(dst, crp->crp_aad, crp->crp_aad_length);
			else
				crypto_copydata(crp, crp->crp_aad_start,
				    crp->crp_aad_length, dst);
			dst += crp->crp_aad_length;
		}
		crypto_copydata(crp, crp->crp_payload_start,
		    crp->crp_payload_length, dst);
		dst += crp->crp_payload_length;
		if (op_type == CHCR_DECRYPT_OP)
			crypto_copydata(crp, crp->crp_digest_start,
			    hash_size_in_response, dst);
	} else {
		dst += CCM_B0_SIZE;
		if (b0_len > CCM_B0_SIZE) {
			/*
			 * If there is AAD, insert padding including a
			 * ULP_TX_SC_NOOP so that the ULP_TX_SC_DSGL
			 * is 16-byte aligned.
			 */
			KASSERT(b0_len - CCM_B0_SIZE == CCM_AAD_FIELD_SIZE,
			    ("b0_len mismatch"));
			memset(dst + CCM_AAD_FIELD_SIZE, 0,
			    8 - CCM_AAD_FIELD_SIZE);
			idata = (void *)(dst + 8);
			idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
			idata->len = htobe32(0);
			dst = (void *)(idata + 1);
		}
		ccr_write_ulptx_sgl(s, dst, sgl_nsegs);
	}

	/* XXX: TODO backpressure */
	t4_wrq_tx(sc->adapter, wr);

	explicit_bzero(iv, sizeof(iv));
	return (0);
}

static int
ccr_ccm_done(struct ccr_softc *sc, struct ccr_session *s,
    struct cryptop *crp, const struct cpl_fw6_pld *cpl, int error)
{

	/*
	 * The updated IV to permit chained requests is at
	 * cpl->data[2], but OCF doesn't permit chained requests.
	 *
	 * Note that the hardware should always verify the CBC MAC
	 * hash.
	 */
	return (error);
}

/*
 * Handle a CCM request that is not supported by the crypto engine by
 * performing the operation in software.  Derived from swcr_authenc().
 */
static void
ccr_ccm_soft(struct ccr_session *s, struct cryptop *crp)
{
	struct auth_hash *axf;
	struct enc_xform *exf;
	union authctx *auth_ctx;
	void *kschedule;
	char block[CCM_CBC_BLOCK_LEN];
	char digest[AES_CBC_MAC_HASH_LEN];
	char iv[AES_CCM_IV_LEN];
	int error, i, len;

	auth_ctx = NULL;
	kschedule = NULL;

	/* Initialize the MAC. */
	switch (s->blkcipher.key_len) {
	case 16:
		axf = &auth_hash_ccm_cbc_mac_128;
		break;
	case 24:
		axf = &auth_hash_ccm_cbc_mac_192;
		break;
	case 32:
		axf = &auth_hash_ccm_cbc_mac_256;
		break;
	default:
		error = EINVAL;
		goto out;
	}
	auth_ctx = malloc(axf->ctxsize, M_CCR, M_NOWAIT);
	if (auth_ctx == NULL) {
		error = ENOMEM;
		goto out;
	}
	axf->Init(auth_ctx);
	axf->Setkey(auth_ctx, s->blkcipher.enckey, s->blkcipher.key_len);

	/* Initialize the cipher. */
	exf = &enc_xform_ccm;
	kschedule = malloc(exf->ctxsize, M_CCR, M_NOWAIT);
	if (kschedule == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = exf->setkey(kschedule, s->blkcipher.enckey,
	    s->blkcipher.key_len);
	if (error)
		goto out;

	if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0) {
		error = EINVAL;
		goto out;
	}
	memcpy(iv, crp->crp_iv, AES_CCM_IV_LEN);

	auth_ctx->aes_cbc_mac_ctx.authDataLength = crp->crp_aad_length;
	auth_ctx->aes_cbc_mac_ctx.cryptDataLength = crp->crp_payload_length;
	axf->Reinit(auth_ctx, iv, sizeof(iv));

	/* MAC the AAD. */
	if (crp->crp_aad != NULL)
		error = axf->Update(auth_ctx, crp->crp_aad,
		    crp->crp_aad_length);
	else
		error = crypto_apply(crp, crp->crp_aad_start,
		    crp->crp_aad_length, axf->Update, auth_ctx);
	if (error)
		goto out;

	exf->reinit(kschedule, iv);

	/* Do encryption/decryption with MAC */
	for (i = 0; i < crp->crp_payload_length; i += sizeof(block)) {
		len = imin(crp->crp_payload_length - i, sizeof(block));
		crypto_copydata(crp, crp->crp_payload_start + i, len, block);
		bzero(block + len, sizeof(block) - len);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			axf->Update(auth_ctx, block, len);
			exf->encrypt(kschedule, block, block);
			crypto_copyback(crp, crp->crp_payload_start + i, len,
			    block);
		} else {
			exf->decrypt(kschedule, block, block);
			axf->Update(auth_ctx, block, len);
		}
	}

	/* Finalize MAC. */
	axf->Final(digest, auth_ctx);

	/* Inject or validate tag. */
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		crypto_copyback(crp, crp->crp_digest_start, sizeof(digest),
		    digest);
		error = 0;
	} else {
		char digest2[AES_CBC_MAC_HASH_LEN];

		crypto_copydata(crp, crp->crp_digest_start, sizeof(digest2),
		    digest2);
		if (timingsafe_bcmp(digest, digest2, sizeof(digest)) == 0) {
			error = 0;

			/* Tag matches, decrypt data. */
			exf->reinit(kschedule, iv);
			for (i = 0; i < crp->crp_payload_length;
			     i += sizeof(block)) {
				len = imin(crp->crp_payload_length - i,
				    sizeof(block));
				crypto_copydata(crp, crp->crp_payload_start + i,
				    len, block);
				bzero(block + len, sizeof(block) - len);
				exf->decrypt(kschedule, block, block);
				crypto_copyback(crp, crp->crp_payload_start + i,
				    len, block);
			}
		} else
			error = EBADMSG;
		explicit_bzero(digest2, sizeof(digest2));
	}

out:
	zfree(kschedule, M_CCR);
	zfree(auth_ctx, M_CCR);
	explicit_bzero(block, sizeof(block));
	explicit_bzero(iv, sizeof(iv));
	explicit_bzero(digest, sizeof(digest));
	crp->crp_etype = error;
	crypto_done(crp);
}

static void
ccr_identify(driver_t *driver, device_t parent)
{
	struct adapter *sc;

	sc = device_get_softc(parent);
	if (sc->cryptocaps & FW_CAPS_CONFIG_CRYPTO_LOOKASIDE &&
	    device_find_child(parent, "ccr", -1) == NULL)
		device_add_child(parent, "ccr", -1);
}

static int
ccr_probe(device_t dev)
{

	device_set_desc(dev, "Chelsio Crypto Accelerator");
	return (BUS_PROBE_DEFAULT);
}

static void
ccr_sysctls(struct ccr_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid, *port_oid;
	struct sysctl_oid_list *children;
	char buf[16];
	int i;

	ctx = device_get_sysctl_ctx(sc->dev);

	/*
	 * dev.ccr.X.
	 */
	oid = device_get_sysctl_tree(sc->dev);
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "port_mask", CTLFLAG_RW,
	    &sc->port_mask, 0, "Mask of enabled ports");

	/*
	 * dev.ccr.X.stats.
	 */
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "statistics");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "hash", CTLFLAG_RD,
	    &sc->stats_hash, "Hash requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "hmac", CTLFLAG_RD,
	    &sc->stats_hmac, "HMAC requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "cipher_encrypt",
	    CTLFLAG_RD, &sc->stats_blkcipher_encrypt,
	    "Cipher encryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "cipher_decrypt",
	    CTLFLAG_RD, &sc->stats_blkcipher_decrypt,
	    "Cipher decryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "eta_encrypt",
	    CTLFLAG_RD, &sc->stats_eta_encrypt,
	    "Combined AES+HMAC encryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "eta_decrypt",
	    CTLFLAG_RD, &sc->stats_eta_decrypt,
	    "Combined AES+HMAC decryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "gcm_encrypt",
	    CTLFLAG_RD, &sc->stats_gcm_encrypt,
	    "AES-GCM encryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "gcm_decrypt",
	    CTLFLAG_RD, &sc->stats_gcm_decrypt,
	    "AES-GCM decryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "ccm_encrypt",
	    CTLFLAG_RD, &sc->stats_ccm_encrypt,
	    "AES-CCM encryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "ccm_decrypt",
	    CTLFLAG_RD, &sc->stats_ccm_decrypt,
	    "AES-CCM decryption requests submitted");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "wr_nomem", CTLFLAG_RD,
	    &sc->stats_wr_nomem, "Work request memory allocation failures");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "inflight", CTLFLAG_RD,
	    &sc->stats_inflight, "Requests currently pending");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "mac_error", CTLFLAG_RD,
	    &sc->stats_mac_error, "MAC errors");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "pad_error", CTLFLAG_RD,
	    &sc->stats_pad_error, "Padding errors");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "sglist_error",
	    CTLFLAG_RD, &sc->stats_sglist_error,
	    "Requests for which DMA mapping failed");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "process_error",
	    CTLFLAG_RD, &sc->stats_process_error,
	    "Requests failed during queueing");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "sw_fallback",
	    CTLFLAG_RD, &sc->stats_sw_fallback,
	    "Requests processed by falling back to software");

	/*
	 * dev.ccr.X.stats.port
	 */
	port_oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "port",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Per-port statistics");

	for (i = 0; i < nitems(sc->ports); i++) {
		if (sc->ports[i].rxq == NULL)
			continue;

		/*
		 * dev.ccr.X.stats.port.Y
		 */
		snprintf(buf, sizeof(buf), "%d", i);
		oid = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(port_oid), OID_AUTO,
		    buf, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, buf);
		children = SYSCTL_CHILDREN(oid);

		SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "active_sessions",
		    CTLFLAG_RD, &sc->ports[i].active_sessions, 0,
		    "Count of active sessions");
	}
}

static void
ccr_init_port(struct ccr_softc *sc, int port)
{

	sc->ports[port].txq = &sc->adapter->sge.ctrlq[port];
	sc->ports[port].rxq =
	    &sc->adapter->sge.rxq[sc->adapter->port[port]->vi->first_rxq];
	sc->ports[port].tx_channel_id = port;
	_Static_assert(sizeof(sc->port_mask) * NBBY >= MAX_NPORTS - 1,
	    "Too many ports to fit in port_mask");
	sc->port_mask |= 1u << port;
}

static int
ccr_attach(device_t dev)
{
	struct ccr_softc *sc;
	int32_t cid;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->adapter = device_get_softc(device_get_parent(dev));
	for_each_port(sc->adapter, i) {
		ccr_init_port(sc, i);
	}
	cid = crypto_get_driverid(dev, sizeof(struct ccr_session),
	    CRYPTOCAP_F_HARDWARE);
	if (cid < 0) {
		device_printf(dev, "could not get crypto driver id\n");
		return (ENXIO);
	}
	sc->cid = cid;
	sc->adapter->ccr_softc = sc;

	mtx_init(&sc->lock, "ccr", NULL, MTX_DEF);
	sc->iv_aad_buf = malloc(MAX_AAD_LEN, M_CCR, M_WAITOK);
	sc->sg_iv_aad = sglist_build(sc->iv_aad_buf, MAX_AAD_LEN, M_WAITOK);
	sc->stats_blkcipher_encrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_blkcipher_decrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_hash = counter_u64_alloc(M_WAITOK);
	sc->stats_hmac = counter_u64_alloc(M_WAITOK);
	sc->stats_eta_encrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_eta_decrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_gcm_encrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_gcm_decrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_ccm_encrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_ccm_decrypt = counter_u64_alloc(M_WAITOK);
	sc->stats_wr_nomem = counter_u64_alloc(M_WAITOK);
	sc->stats_inflight = counter_u64_alloc(M_WAITOK);
	sc->stats_mac_error = counter_u64_alloc(M_WAITOK);
	sc->stats_pad_error = counter_u64_alloc(M_WAITOK);
	sc->stats_sglist_error = counter_u64_alloc(M_WAITOK);
	sc->stats_process_error = counter_u64_alloc(M_WAITOK);
	sc->stats_sw_fallback = counter_u64_alloc(M_WAITOK);
	ccr_sysctls(sc);

	return (0);
}

static int
ccr_detach(device_t dev)
{
	struct ccr_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock(&sc->lock);
	sc->detaching = true;
	mtx_unlock(&sc->lock);

	crypto_unregister_all(sc->cid);

	mtx_destroy(&sc->lock);
	counter_u64_free(sc->stats_blkcipher_encrypt);
	counter_u64_free(sc->stats_blkcipher_decrypt);
	counter_u64_free(sc->stats_hash);
	counter_u64_free(sc->stats_hmac);
	counter_u64_free(sc->stats_eta_encrypt);
	counter_u64_free(sc->stats_eta_decrypt);
	counter_u64_free(sc->stats_gcm_encrypt);
	counter_u64_free(sc->stats_gcm_decrypt);
	counter_u64_free(sc->stats_ccm_encrypt);
	counter_u64_free(sc->stats_ccm_decrypt);
	counter_u64_free(sc->stats_wr_nomem);
	counter_u64_free(sc->stats_inflight);
	counter_u64_free(sc->stats_mac_error);
	counter_u64_free(sc->stats_pad_error);
	counter_u64_free(sc->stats_sglist_error);
	counter_u64_free(sc->stats_process_error);
	counter_u64_free(sc->stats_sw_fallback);
	sglist_free(sc->sg_iv_aad);
	free(sc->iv_aad_buf, M_CCR);
	sc->adapter->ccr_softc = NULL;
	return (0);
}

static void
ccr_init_hash_digest(struct ccr_session *s)
{
	union authctx auth_ctx;
	struct auth_hash *axf;

	axf = s->hmac.auth_hash;
	axf->Init(&auth_ctx);
	t4_copy_partial_hash(axf->type, &auth_ctx, s->hmac.pads);
}

static bool
ccr_aes_check_keylen(int alg, int klen)
{

	switch (klen * 8) {
	case 128:
	case 192:
		if (alg == CRYPTO_AES_XTS)
			return (false);
		break;
	case 256:
		break;
	case 512:
		if (alg != CRYPTO_AES_XTS)
			return (false);
		break;
	default:
		return (false);
	}
	return (true);
}

static void
ccr_aes_setkey(struct ccr_session *s, const void *key, int klen)
{
	unsigned int ck_size, iopad_size, kctx_flits, kctx_len, kbits, mk_size;
	unsigned int opad_present;

	if (s->blkcipher.cipher_mode == SCMD_CIPH_MODE_AES_XTS)
		kbits = (klen / 2) * 8;
	else
		kbits = klen * 8;
	switch (kbits) {
	case 128:
		ck_size = CHCR_KEYCTX_CIPHER_KEY_SIZE_128;
		break;
	case 192:
		ck_size = CHCR_KEYCTX_CIPHER_KEY_SIZE_192;
		break;
	case 256:
		ck_size = CHCR_KEYCTX_CIPHER_KEY_SIZE_256;
		break;
	default:
		panic("should not get here");
	}

	s->blkcipher.key_len = klen;
	memcpy(s->blkcipher.enckey, key, s->blkcipher.key_len);
	switch (s->blkcipher.cipher_mode) {
	case SCMD_CIPH_MODE_AES_CBC:
	case SCMD_CIPH_MODE_AES_XTS:
		t4_aes_getdeckey(s->blkcipher.deckey, key, kbits);
		break;
	}

	kctx_len = roundup2(s->blkcipher.key_len, 16);
	switch (s->mode) {
	case ETA:
		mk_size = s->hmac.mk_size;
		opad_present = 1;
		iopad_size = roundup2(s->hmac.partial_digest_len, 16);
		kctx_len += iopad_size * 2;
		break;
	case GCM:
		mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_128;
		opad_present = 0;
		kctx_len += GMAC_BLOCK_LEN;
		break;
	case CCM:
		switch (kbits) {
		case 128:
			mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_128;
			break;
		case 192:
			mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_192;
			break;
		case 256:
			mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_256;
			break;
		default:
			panic("should not get here");
		}
		opad_present = 0;
		kctx_len *= 2;
		break;
	default:
		mk_size = CHCR_KEYCTX_NO_KEY;
		opad_present = 0;
		break;
	}
	kctx_flits = (sizeof(struct _key_ctx) + kctx_len) / 16;
	s->blkcipher.key_ctx_hdr = htobe32(V_KEY_CONTEXT_CTX_LEN(kctx_flits) |
	    V_KEY_CONTEXT_DUAL_CK(s->blkcipher.cipher_mode ==
	    SCMD_CIPH_MODE_AES_XTS) |
	    V_KEY_CONTEXT_OPAD_PRESENT(opad_present) |
	    V_KEY_CONTEXT_SALT_PRESENT(1) | V_KEY_CONTEXT_CK_SIZE(ck_size) |
	    V_KEY_CONTEXT_MK_SIZE(mk_size) | V_KEY_CONTEXT_VALID(1));
}

static bool
ccr_auth_supported(const struct crypto_session_params *csp)
{

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		break;
	default:
		return (false);
	}
	return (true);
}

static bool
ccr_cipher_supported(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		break;
	case CRYPTO_AES_ICM:
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		break;
	case CRYPTO_AES_XTS:
		if (csp->csp_ivlen != AES_XTS_IV_LEN)
			return (false);
		break;
	default:
		return (false);
	}
	return (ccr_aes_check_keylen(csp->csp_cipher_alg,
	    csp->csp_cipher_klen));
}

static int
ccr_cipher_mode(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		return (SCMD_CIPH_MODE_AES_CBC);
	case CRYPTO_AES_ICM:
		return (SCMD_CIPH_MODE_AES_CTR);
	case CRYPTO_AES_NIST_GCM_16:
		return (SCMD_CIPH_MODE_AES_GCM);
	case CRYPTO_AES_XTS:
		return (SCMD_CIPH_MODE_AES_XTS);
	case CRYPTO_AES_CCM_16:
		return (SCMD_CIPH_MODE_AES_CCM);
	default:
		return (SCMD_CIPH_MODE_NOP);
	}
}

static int
ccr_probesession(device_t dev, const struct crypto_session_params *csp)
{
	unsigned int cipher_mode;

	if ((csp->csp_flags & ~(CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)) !=
	    0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (!ccr_auth_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		if (!ccr_cipher_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return (EINVAL);
			if (csp->csp_auth_mlen < 0 ||
			    csp->csp_auth_mlen > AES_GMAC_HASH_LEN)
				return (EINVAL);
			break;
		case CRYPTO_AES_CCM_16:
			if (csp->csp_ivlen != AES_CCM_IV_LEN)
				return (EINVAL);
			if (csp->csp_auth_mlen < 0 ||
			    csp->csp_auth_mlen > AES_CBC_MAC_HASH_LEN)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_ETA:
		if (!ccr_auth_supported(csp) || !ccr_cipher_supported(csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	if (csp->csp_cipher_klen != 0) {
		cipher_mode = ccr_cipher_mode(csp);
		if (cipher_mode == SCMD_CIPH_MODE_NOP)
			return (EINVAL);
	}

	return (CRYPTODEV_PROBE_HARDWARE);
}

/*
 * Select an available port with the lowest number of active sessions.
 */
static struct ccr_port *
ccr_choose_port(struct ccr_softc *sc)
{
	struct ccr_port *best, *p;
	int i;

	mtx_assert(&sc->lock, MA_OWNED);
	best = NULL;
	for (i = 0; i < nitems(sc->ports); i++) {
		p = &sc->ports[i];

		/* Ignore non-existent ports. */
		if (p->rxq == NULL)
			continue;

		/*
		 * XXX: Ignore ports whose queues aren't initialized.
		 * This is racy as the rxq can be destroyed by the
		 * associated VI detaching.  Eventually ccr should use
		 * dedicated queues.
		 */
		if (p->rxq->iq.adapter == NULL || p->txq->adapter == NULL)
			continue;

		if ((sc->port_mask & (1u << i)) == 0)
			continue;

		if (best == NULL ||
		    p->active_sessions < best->active_sessions)
			best = p;
	}
	return (best);
}

static void
ccr_delete_session(struct ccr_session *s)
{
	sglist_free(s->sg_input);
	sglist_free(s->sg_output);
	sglist_free(s->sg_ulptx);
	sglist_free(s->sg_dsgl);
	mtx_destroy(&s->lock);
}

static int
ccr_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct ccr_softc *sc;
	struct ccr_session *s;
	struct auth_hash *auth_hash;
	unsigned int auth_mode, cipher_mode, mk_size;
	unsigned int partial_digest_len;

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		auth_hash = &auth_hash_hmac_sha1;
		auth_mode = SCMD_AUTH_MODE_SHA1;
		mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_160;
		partial_digest_len = SHA1_HASH_LEN;
		break;
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_224_HMAC:
		auth_hash = &auth_hash_hmac_sha2_224;
		auth_mode = SCMD_AUTH_MODE_SHA224;
		mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_256;
		partial_digest_len = SHA2_256_HASH_LEN;
		break;
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_256_HMAC:
		auth_hash = &auth_hash_hmac_sha2_256;
		auth_mode = SCMD_AUTH_MODE_SHA256;
		mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_256;
		partial_digest_len = SHA2_256_HASH_LEN;
		break;
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_384_HMAC:
		auth_hash = &auth_hash_hmac_sha2_384;
		auth_mode = SCMD_AUTH_MODE_SHA512_384;
		mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_512;
		partial_digest_len = SHA2_512_HASH_LEN;
		break;
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA2_512_HMAC:
		auth_hash = &auth_hash_hmac_sha2_512;
		auth_mode = SCMD_AUTH_MODE_SHA512_512;
		mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_512;
		partial_digest_len = SHA2_512_HASH_LEN;
		break;
	default:
		auth_hash = NULL;
		auth_mode = SCMD_AUTH_MODE_NOP;
		mk_size = 0;
		partial_digest_len = 0;
		break;
	}

	cipher_mode = ccr_cipher_mode(csp);

#ifdef INVARIANTS
	switch (csp->csp_mode) {
	case CSP_MODE_CIPHER:
		if (cipher_mode == SCMD_CIPH_MODE_NOP ||
		    cipher_mode == SCMD_CIPH_MODE_AES_GCM ||
		    cipher_mode == SCMD_CIPH_MODE_AES_CCM)
			panic("invalid cipher algo");
		break;
	case CSP_MODE_DIGEST:
		if (auth_mode == SCMD_AUTH_MODE_NOP)
			panic("invalid auth algo");
		break;
	case CSP_MODE_AEAD:
		if (cipher_mode != SCMD_CIPH_MODE_AES_GCM &&
		    cipher_mode != SCMD_CIPH_MODE_AES_CCM)
			panic("invalid aead cipher algo");
		if (auth_mode != SCMD_AUTH_MODE_NOP)
			panic("invalid aead auth aglo");
		break;
	case CSP_MODE_ETA:
		if (cipher_mode == SCMD_CIPH_MODE_NOP ||
		    cipher_mode == SCMD_CIPH_MODE_AES_GCM ||
		    cipher_mode == SCMD_CIPH_MODE_AES_CCM)
			panic("invalid cipher algo");
		if (auth_mode == SCMD_AUTH_MODE_NOP)
			panic("invalid auth algo");
		break;
	default:
		panic("invalid csp mode");
	}
#endif

	s = crypto_get_driver_session(cses);
	mtx_init(&s->lock, "ccr session", NULL, MTX_DEF);
	s->sg_input = sglist_alloc(TX_SGL_SEGS, M_NOWAIT);
	s->sg_output = sglist_alloc(TX_SGL_SEGS, M_NOWAIT);
	s->sg_ulptx = sglist_alloc(TX_SGL_SEGS, M_NOWAIT);
	s->sg_dsgl = sglist_alloc(MAX_RX_PHYS_DSGL_SGE, M_NOWAIT);
	if (s->sg_input == NULL || s->sg_output == NULL ||
	    s->sg_ulptx == NULL || s->sg_dsgl == NULL) {
		ccr_delete_session(s);
		return (ENOMEM);
	}

	sc = device_get_softc(dev);

	mtx_lock(&sc->lock);
	if (sc->detaching) {
		mtx_unlock(&sc->lock);
		ccr_delete_session(s);
		return (ENXIO);
	}

	s->port = ccr_choose_port(sc);
	if (s->port == NULL) {
		mtx_unlock(&sc->lock);
		ccr_delete_session(s);
		return (ENXIO);
	}

	switch (csp->csp_mode) {
	case CSP_MODE_AEAD:
		if (cipher_mode == SCMD_CIPH_MODE_AES_CCM)
			s->mode = CCM;
		else
			s->mode = GCM;
		break;
	case CSP_MODE_ETA:
		s->mode = ETA;
		break;
	case CSP_MODE_DIGEST:
		if (csp->csp_auth_klen != 0)
			s->mode = HMAC;
		else
			s->mode = HASH;
		break;
	case CSP_MODE_CIPHER:
		s->mode = BLKCIPHER;
		break;
	}

	if (s->mode == GCM) {
		if (csp->csp_auth_mlen == 0)
			s->gmac.hash_len = AES_GMAC_HASH_LEN;
		else
			s->gmac.hash_len = csp->csp_auth_mlen;
		t4_init_gmac_hash(csp->csp_cipher_key, csp->csp_cipher_klen,
		    s->gmac.ghash_h);
	} else if (s->mode == CCM) {
		if (csp->csp_auth_mlen == 0)
			s->ccm_mac.hash_len = AES_CBC_MAC_HASH_LEN;
		else
			s->ccm_mac.hash_len = csp->csp_auth_mlen;
	} else if (auth_mode != SCMD_AUTH_MODE_NOP) {
		s->hmac.auth_hash = auth_hash;
		s->hmac.auth_mode = auth_mode;
		s->hmac.mk_size = mk_size;
		s->hmac.partial_digest_len = partial_digest_len;
		if (csp->csp_auth_mlen == 0)
			s->hmac.hash_len = auth_hash->hashsize;
		else
			s->hmac.hash_len = csp->csp_auth_mlen;
		if (csp->csp_auth_key != NULL)
			t4_init_hmac_digest(auth_hash, partial_digest_len,
			    csp->csp_auth_key, csp->csp_auth_klen,
			    s->hmac.pads);
		else
			ccr_init_hash_digest(s);
	}
	if (cipher_mode != SCMD_CIPH_MODE_NOP) {
		s->blkcipher.cipher_mode = cipher_mode;
		s->blkcipher.iv_len = csp->csp_ivlen;
		if (csp->csp_cipher_key != NULL)
			ccr_aes_setkey(s, csp->csp_cipher_key,
			    csp->csp_cipher_klen);
	}

	s->port->active_sessions++;
	mtx_unlock(&sc->lock);
	return (0);
}

static void
ccr_freesession(device_t dev, crypto_session_t cses)
{
	struct ccr_softc *sc;
	struct ccr_session *s;

	sc = device_get_softc(dev);
	s = crypto_get_driver_session(cses);
#ifdef INVARIANTS
	if (s->pending != 0)
		device_printf(dev,
		    "session %p freed with %d pending requests\n", s,
		    s->pending);
#endif
	mtx_lock(&sc->lock);
	s->port->active_sessions--;
	mtx_unlock(&sc->lock);
	ccr_delete_session(s);
}

static int
ccr_process(device_t dev, struct cryptop *crp, int hint)
{
	const struct crypto_session_params *csp;
	struct ccr_softc *sc;
	struct ccr_session *s;
	int error;

	csp = crypto_get_params(crp->crp_session);
	s = crypto_get_driver_session(crp->crp_session);
	sc = device_get_softc(dev);

	mtx_lock(&s->lock);
	error = ccr_populate_sglist(s->sg_input, &crp->crp_buf);
	if (error == 0 && CRYPTO_HAS_OUTPUT_BUFFER(crp))
		error = ccr_populate_sglist(s->sg_output, &crp->crp_obuf);
	if (error) {
		counter_u64_add(sc->stats_sglist_error, 1);
		goto out;
	}

	switch (s->mode) {
	case HASH:
		error = ccr_hash(sc, s, crp);
		if (error == 0)
			counter_u64_add(sc->stats_hash, 1);
		break;
	case HMAC:
		if (crp->crp_auth_key != NULL)
			t4_init_hmac_digest(s->hmac.auth_hash,
			    s->hmac.partial_digest_len, crp->crp_auth_key,
			    csp->csp_auth_klen, s->hmac.pads);
		error = ccr_hash(sc, s, crp);
		if (error == 0)
			counter_u64_add(sc->stats_hmac, 1);
		break;
	case BLKCIPHER:
		if (crp->crp_cipher_key != NULL)
			ccr_aes_setkey(s, crp->crp_cipher_key,
			    csp->csp_cipher_klen);
		error = ccr_blkcipher(sc, s, crp);
		if (error == 0) {
			if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
				counter_u64_add(sc->stats_blkcipher_encrypt, 1);
			else
				counter_u64_add(sc->stats_blkcipher_decrypt, 1);
		}
		break;
	case ETA:
		if (crp->crp_auth_key != NULL)
			t4_init_hmac_digest(s->hmac.auth_hash,
			    s->hmac.partial_digest_len, crp->crp_auth_key,
			    csp->csp_auth_klen, s->hmac.pads);
		if (crp->crp_cipher_key != NULL)
			ccr_aes_setkey(s, crp->crp_cipher_key,
			    csp->csp_cipher_klen);
		error = ccr_eta(sc, s, crp);
		if (error == 0) {
			if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
				counter_u64_add(sc->stats_eta_encrypt, 1);
			else
				counter_u64_add(sc->stats_eta_decrypt, 1);
		}
		break;
	case GCM:
		if (crp->crp_cipher_key != NULL) {
			t4_init_gmac_hash(crp->crp_cipher_key,
			    csp->csp_cipher_klen, s->gmac.ghash_h);
			ccr_aes_setkey(s, crp->crp_cipher_key,
			    csp->csp_cipher_klen);
		}
		if (crp->crp_payload_length == 0) {
			mtx_unlock(&s->lock);
			ccr_gcm_soft(s, crp);
			return (0);
		}
		error = ccr_gcm(sc, s, crp);
		if (error == EMSGSIZE) {
			counter_u64_add(sc->stats_sw_fallback, 1);
			mtx_unlock(&s->lock);
			ccr_gcm_soft(s, crp);
			return (0);
		}
		if (error == 0) {
			if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
				counter_u64_add(sc->stats_gcm_encrypt, 1);
			else
				counter_u64_add(sc->stats_gcm_decrypt, 1);
		}
		break;
	case CCM:
		if (crp->crp_cipher_key != NULL) {
			ccr_aes_setkey(s, crp->crp_cipher_key,
			    csp->csp_cipher_klen);
		}
		error = ccr_ccm(sc, s, crp);
		if (error == EMSGSIZE) {
			counter_u64_add(sc->stats_sw_fallback, 1);
			mtx_unlock(&s->lock);
			ccr_ccm_soft(s, crp);
			return (0);
		}
		if (error == 0) {
			if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
				counter_u64_add(sc->stats_ccm_encrypt, 1);
			else
				counter_u64_add(sc->stats_ccm_decrypt, 1);
		}
		break;
	}

	if (error == 0) {
#ifdef INVARIANTS
		s->pending++;
#endif
		counter_u64_add(sc->stats_inflight, 1);
	} else
		counter_u64_add(sc->stats_process_error, 1);

out:
	mtx_unlock(&s->lock);

	if (error) {
		crp->crp_etype = error;
		crypto_done(crp);
	}

	return (0);
}

static int
do_cpl6_fw_pld(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct ccr_softc *sc = iq->adapter->ccr_softc;
	struct ccr_session *s;
	const struct cpl_fw6_pld *cpl;
	struct cryptop *crp;
	uint32_t status;
	int error;

	if (m != NULL)
		cpl = mtod(m, const void *);
	else
		cpl = (const void *)(rss + 1);

	crp = (struct cryptop *)(uintptr_t)be64toh(cpl->data[1]);
	s = crypto_get_driver_session(crp->crp_session);
	status = be64toh(cpl->data[0]);
	if (CHK_MAC_ERR_BIT(status) || CHK_PAD_ERR_BIT(status))
		error = EBADMSG;
	else
		error = 0;

#ifdef INVARIANTS
	mtx_lock(&s->lock);
	s->pending--;
	mtx_unlock(&s->lock);
#endif
	counter_u64_add(sc->stats_inflight, -1);

	switch (s->mode) {
	case HASH:
	case HMAC:
		error = ccr_hash_done(sc, s, crp, cpl, error);
		break;
	case BLKCIPHER:
		error = ccr_blkcipher_done(sc, s, crp, cpl, error);
		break;
	case ETA:
		error = ccr_eta_done(sc, s, crp, cpl, error);
		break;
	case GCM:
		error = ccr_gcm_done(sc, s, crp, cpl, error);
		break;
	case CCM:
		error = ccr_ccm_done(sc, s, crp, cpl, error);
		break;
	}

	if (error == EBADMSG) {
		if (CHK_MAC_ERR_BIT(status))
			counter_u64_add(sc->stats_mac_error, 1);
		if (CHK_PAD_ERR_BIT(status))
			counter_u64_add(sc->stats_pad_error, 1);
	}
	crp->crp_etype = error;
	crypto_done(crp);
	m_freem(m);
	return (0);
}

static int
ccr_modevent(module_t mod, int cmd, void *arg)
{

	switch (cmd) {
	case MOD_LOAD:
		t4_register_cpl_handler(CPL_FW6_PLD, do_cpl6_fw_pld);
		return (0);
	case MOD_UNLOAD:
		t4_register_cpl_handler(CPL_FW6_PLD, NULL);
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static device_method_t ccr_methods[] = {
	DEVMETHOD(device_identify,	ccr_identify),
	DEVMETHOD(device_probe,		ccr_probe),
	DEVMETHOD(device_attach,	ccr_attach),
	DEVMETHOD(device_detach,	ccr_detach),

	DEVMETHOD(cryptodev_probesession, ccr_probesession),
	DEVMETHOD(cryptodev_newsession,	ccr_newsession),
	DEVMETHOD(cryptodev_freesession, ccr_freesession),
	DEVMETHOD(cryptodev_process,	ccr_process),

	DEVMETHOD_END
};

static driver_t ccr_driver = {
	"ccr",
	ccr_methods,
	sizeof(struct ccr_softc)
};

static devclass_t ccr_devclass;

DRIVER_MODULE(ccr, t6nex, ccr_driver, ccr_devclass, ccr_modevent, NULL);
MODULE_VERSION(ccr, 1);
MODULE_DEPEND(ccr, crypto, 1, 1, 1);
MODULE_DEPEND(ccr, t6nex, 1, 1, 1);
