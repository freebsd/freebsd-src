/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications
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

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_kern_tls.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ktr.h>
#include <sys/ktls.h>
#include <sys/sglist.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockbuf.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp_var.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "common/t4_tcb.h"
#include "t4_l2t.h"
#include "t4_clip.h"
#include "t4_mp_ring.h"
#include "crypto/t4_crypto.h"

#if defined(INET) || defined(INET6)

#define TLS_HEADER_LENGTH		5

struct tls_scmd {
	__be32 seqno_numivs;
	__be32 ivgen_hdrlen;
};

struct tlspcb {
	struct m_snd_tag com;
	struct vi_info *vi;	/* virtual interface */
	struct adapter *sc;
	struct sge_txq *txq;

	int tx_key_addr;
	bool inline_key;
	bool tls13;
	unsigned char enc_mode;

	struct tls_scmd scmd0;
	struct tls_scmd scmd0_partial;
	struct tls_scmd scmd0_short;

	unsigned int tx_key_info_size;

	uint16_t prev_mss;

	/* Fields used for GCM records using GHASH state. */
	uint16_t ghash_offset;
	uint64_t ghash_tls_seqno;
	char ghash[AES_GMAC_HASH_LEN];
	bool ghash_valid;
	bool ghash_pending;
	bool ghash_lcb;
	bool queue_mbufs;
	uint8_t rx_chid;
	uint16_t rx_qid;
	struct mbufq pending_mbufs;

	/*
	 * Only used outside of setup and teardown when using inline
	 * keys or for partial GCM mode.
	 */
	struct tls_keyctx keyctx;
};

static void t7_tls_tag_free(struct m_snd_tag *mst);
static int ktls_setup_keys(struct tlspcb *tlsp,
    const struct ktls_session *tls, struct sge_txq *txq);

static void *zero_buffer;
static vm_paddr_t zero_buffer_pa;

static const struct if_snd_tag_sw t7_tls_tag_sw = {
	.snd_tag_free = t7_tls_tag_free,
	.type = IF_SND_TAG_TYPE_TLS
};

static inline struct tlspcb *
mst_to_tls(struct m_snd_tag *t)
{
	return (__containerof(t, struct tlspcb, com));
}

static struct tlspcb *
alloc_tlspcb(struct ifnet *ifp, struct vi_info *vi, int flags)
{
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct tlspcb *tlsp;

	tlsp = malloc(sizeof(*tlsp), M_CXGBE, M_ZERO | flags);
	if (tlsp == NULL)
		return (NULL);

	m_snd_tag_init(&tlsp->com, ifp, &t7_tls_tag_sw);
	tlsp->vi = vi;
	tlsp->sc = sc;
	tlsp->tx_key_addr = -1;
	tlsp->ghash_offset = -1;
	tlsp->rx_chid = pi->rx_chan;
	tlsp->rx_qid = sc->sge.rxq[pi->vi->first_rxq].iq.abs_id;
	mbufq_init(&tlsp->pending_mbufs, INT_MAX);

	return (tlsp);
}

int
t7_tls_tag_alloc(struct ifnet *ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **pt)
{
	const struct ktls_session *tls;
	struct tlspcb *tlsp;
	struct adapter *sc;
	struct vi_info *vi;
	struct inpcb *inp;
	struct sge_txq *txq;
	int error, iv_size, keyid, mac_first;

	tls = params->tls.tls;

	/* TLS 1.1 through TLS 1.3 are currently supported. */
	if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
	    tls->params.tls_vminor < TLS_MINOR_VER_ONE ||
	    tls->params.tls_vminor > TLS_MINOR_VER_THREE)
		return (EPROTONOSUPPORT);

	/* Sanity check values in *tls. */
	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		/* XXX: Explicitly ignore any provided IV. */
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 192 / 8:
		case 256 / 8:
			break;
		default:
			return (EINVAL);
		}
		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
			break;
		default:
			return (EPROTONOSUPPORT);
		}
		iv_size = AES_BLOCK_LEN;
		mac_first = 1;
		break;
	case CRYPTO_AES_NIST_GCM_16:
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 192 / 8:
		case 256 / 8:
			break;
		default:
			return (EINVAL);
		}

		/*
		 * The IV size for TLS 1.2 is the explicit IV in the
		 * record header.  For TLS 1.3 it is the size of the
		 * sequence number.
		 */
		iv_size = 8;
		mac_first = 0;
		break;
	default:
		return (EPROTONOSUPPORT);
	}

	vi = if_getsoftc(ifp);
	sc = vi->adapter;

	tlsp = alloc_tlspcb(ifp, vi, M_WAITOK);

	/*
	 * Pointers with the low bit set in the pointer can't
	 * be stored as the cookie in the CPL_FW6_PLD reply.
	 */
	if (((uintptr_t)tlsp & CPL_FW6_COOKIE_MASK) != 0) {
		error = EINVAL;
		goto failed;
	}

	tlsp->tls13 = tls->params.tls_vminor == TLS_MINOR_VER_THREE;

	if (sc->tlst.inline_keys)
		keyid = -1;
	else
		keyid = t4_alloc_tls_keyid(sc);
	if (keyid < 0) {
		CTR(KTR_CXGBE, "%s: %p using immediate key ctx", __func__,
		    tlsp);
		tlsp->inline_key = true;
	} else {
		tlsp->tx_key_addr = keyid;
		CTR(KTR_CXGBE, "%s: %p allocated TX key addr %#x", __func__,
		    tlsp, tlsp->tx_key_addr);
	}

	inp = params->tls.inp;
	INP_RLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_RUNLOCK(inp);
		error = ECONNRESET;
		goto failed;
	}

	txq = &sc->sge.txq[vi->first_txq];
	if (inp->inp_flowtype != M_HASHTYPE_NONE)
		txq += ((inp->inp_flowid % (vi->ntxq - vi->rsrv_noflowq)) +
		    vi->rsrv_noflowq);
	tlsp->txq = txq;
	INP_RUNLOCK(inp);

	error = ktls_setup_keys(tlsp, tls, txq);
	if (error)
		goto failed;

	tlsp->enc_mode = t4_tls_cipher_mode(tls);
	tlsp->tx_key_info_size = t4_tls_key_info_size(tls);

	/* The SCMD fields used when encrypting a full TLS record. */
	if (tlsp->tls13)
		tlsp->scmd0.seqno_numivs = V_SCMD_SEQ_NO_CTRL(0);
	else
		tlsp->scmd0.seqno_numivs = V_SCMD_SEQ_NO_CTRL(3);
	tlsp->scmd0.seqno_numivs |=
	    V_SCMD_PROTO_VERSION(t4_tls_proto_ver(tls)) |
	    V_SCMD_ENC_DEC_CTRL(SCMD_ENCDECCTRL_ENCRYPT) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL((mac_first == 0)) |
	    V_SCMD_CIPH_MODE(tlsp->enc_mode) |
	    V_SCMD_AUTH_MODE(t4_tls_auth_mode(tls)) |
	    V_SCMD_HMAC_CTRL(t4_tls_hmac_ctrl(tls)) |
	    V_SCMD_IV_SIZE(iv_size / 2) | V_SCMD_NUM_IVS(1);
	tlsp->scmd0.seqno_numivs = htobe32(tlsp->scmd0.seqno_numivs);

	tlsp->scmd0.ivgen_hdrlen = V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_TLS_FRAG_ENABLE(0);
	if (tlsp->inline_key)
		tlsp->scmd0.ivgen_hdrlen |= V_SCMD_KEY_CTX_INLINE(1);

	/*
	 * The SCMD fields used when encrypting a short TLS record
	 * (no trailer and possibly a truncated payload).
	 */
	tlsp->scmd0_short.seqno_numivs = V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_ENC_DEC_CTRL(SCMD_ENCDECCTRL_ENCRYPT) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL((mac_first == 0)) |
	    V_SCMD_AUTH_MODE(SCMD_AUTH_MODE_NOP) |
	    V_SCMD_HMAC_CTRL(SCMD_HMAC_CTRL_NOP) |
	    V_SCMD_IV_SIZE(AES_BLOCK_LEN / 2) | V_SCMD_NUM_IVS(0);
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM)
		tlsp->scmd0_short.seqno_numivs |=
		    V_SCMD_CIPH_MODE(SCMD_CIPH_MODE_AES_CTR);
	else
		tlsp->scmd0_short.seqno_numivs |=
		    V_SCMD_CIPH_MODE(tlsp->enc_mode);
	tlsp->scmd0_short.seqno_numivs =
	    htobe32(tlsp->scmd0_short.seqno_numivs);

	tlsp->scmd0_short.ivgen_hdrlen = V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_TLS_FRAG_ENABLE(0) | V_SCMD_AADIVDROP(1);
	if (tlsp->inline_key)
		tlsp->scmd0_short.ivgen_hdrlen |= V_SCMD_KEY_CTX_INLINE(1);

	/*
	 * The SCMD fields used when encrypting a short TLS record
	 * using a partial GHASH.
	 */
	tlsp->scmd0_partial.seqno_numivs = V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_ENC_DEC_CTRL(SCMD_ENCDECCTRL_ENCRYPT) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL((mac_first == 0)) |
	    V_SCMD_CIPH_MODE(tlsp->enc_mode) |
	    V_SCMD_AUTH_MODE(t4_tls_auth_mode(tls)) |
	    V_SCMD_HMAC_CTRL(t4_tls_hmac_ctrl(tls)) |
	    V_SCMD_IV_SIZE(AES_BLOCK_LEN / 2) | V_SCMD_NUM_IVS(1);
	tlsp->scmd0_partial.seqno_numivs =
	    htobe32(tlsp->scmd0_partial.seqno_numivs);

	tlsp->scmd0_partial.ivgen_hdrlen = V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_TLS_FRAG_ENABLE(0) | V_SCMD_AADIVDROP(1) |
	    V_SCMD_KEY_CTX_INLINE(1);

	TXQ_LOCK(txq);
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM)
		txq->kern_tls_gcm++;
	else
		txq->kern_tls_cbc++;
	TXQ_UNLOCK(txq);
	*pt = &tlsp->com;
	return (0);

failed:
	m_snd_tag_rele(&tlsp->com);
	return (error);
}

static int
ktls_setup_keys(struct tlspcb *tlsp, const struct ktls_session *tls,
    struct sge_txq *txq)
{
	struct tls_key_req *kwr;
	struct tls_keyctx *kctx;
	void *items[1];
	struct mbuf *m;
	int error;

	/*
	 * Store the salt and keys in the key context.  For
	 * connections with an inline key, this key context is passed
	 * as immediate data in each work request.  For connections
	 * storing the key in DDR, a work request is used to store a
	 * copy of the key context in DDR.
	 */
	t4_tls_key_ctx(tls, KTLS_TX, &tlsp->keyctx);
	if (tlsp->inline_key)
		return (0);

	/* Populate key work request. */
        m = alloc_wr_mbuf(TLS_KEY_WR_SZ, M_NOWAIT);
	if (m == NULL) {
		CTR(KTR_CXGBE, "%s: %p failed to alloc WR mbuf", __func__,
		    tlsp);
		return (ENOMEM);
	}
	m->m_pkthdr.snd_tag = m_snd_tag_ref(&tlsp->com);
	m->m_pkthdr.csum_flags |= CSUM_SND_TAG;
	kwr = mtod(m, void *);
	memset(kwr, 0, TLS_KEY_WR_SZ);

	t4_write_tlskey_wr(tls, KTLS_TX, 0, 0, tlsp->tx_key_addr, kwr);
	kctx = (struct tls_keyctx *)(kwr + 1);
	memcpy(kctx, &tlsp->keyctx, sizeof(*kctx));

	/*
	 * Place the key work request in the transmit queue.  It
	 * should be sent to the NIC before any TLS packets using this
	 * session.
	 */
	items[0] = m;
	error = mp_ring_enqueue(txq->r, items, 1, 1);
	if (error)
		m_free(m);
	else
		CTR(KTR_CXGBE, "%s: %p sent key WR", __func__, tlsp);
	return (error);
}

static u_int
ktls_base_wr_size(struct tlspcb *tlsp, bool inline_key)
{
	u_int wr_len;

	wr_len = sizeof(struct fw_ulptx_wr);	// 16
	wr_len += sizeof(struct ulp_txpkt);	// 8
	wr_len += sizeof(struct ulptx_idata);	// 8
	wr_len += sizeof(struct cpl_tx_sec_pdu);// 32
	if (inline_key)
		wr_len += tlsp->tx_key_info_size;
	else {
		wr_len += sizeof(struct ulptx_sc_memrd);// 8
		wr_len += sizeof(struct ulptx_idata);	// 8
	}
	/* SplitMode CPL_RX_PHYS_DSGL here if needed. */
	/* CPL_TX_*_LSO here if needed. */
	wr_len += sizeof(struct cpl_tx_pkt_core);// 16
	return (wr_len);
}

static u_int
ktls_sgl_size(u_int nsegs)
{
	u_int wr_len;

	/* First segment is part of ulptx_sgl. */
	nsegs--;

	wr_len = sizeof(struct ulptx_sgl);
	wr_len += 8 * ((3 * nsegs) / 2 + (nsegs & 1));
	return (wr_len);
}

/*
 * A request that doesn't need to generate the TLS trailer is a short
 * record.  For these requests, part of the TLS record payload is
 * encrypted without invoking the MAC.
 *
 * Returns true if this record should be sent as a short record.  In
 * either case, the remaining outputs describe the how much of the
 * TLS record to send as input to the crypto block and the amount of
 * crypto output to trim via SplitMode:
 *
 * *header_len - Number of bytes of TLS header to pass as immediate
 *               data
 *
 * *offset - Start offset of TLS record payload to pass as DSGL data
 *
 * *plen - Length of TLS record payload to pass as DSGL data
 *
 * *leading_waste - amount of non-packet-header bytes to drop at the
 *                  start of the crypto output
 *
 * *trailing_waste - amount of crypto output to drop from the end
 */
static bool
ktls_is_short_record(struct tlspcb *tlsp, struct mbuf *m_tls, u_int tlen,
    u_int rlen, u_int *header_len, u_int *offset, u_int *plen,
    u_int *leading_waste, u_int *trailing_waste, bool send_partial_ghash,
    bool request_ghash)
{
	u_int new_tlen, trailer_len;

	MPASS(tlen > m_tls->m_epg_hdrlen);

	/*
	 * For TLS 1.3 treat the inner record type stored as the first
	 * byte of the trailer as part of the payload rather than part
	 * of the trailer.
	 */
	trailer_len = m_tls->m_epg_trllen;
	if (tlsp->tls13)
		trailer_len--;

	/*
	 * Default to sending the full record as input to the crypto
	 * engine and relying on SplitMode to drop any waste.
	 */
	*header_len = m_tls->m_epg_hdrlen;
	*offset = 0;
	*plen = rlen - (m_tls->m_epg_hdrlen + trailer_len);
	*leading_waste = mtod(m_tls, vm_offset_t);
	*trailing_waste = rlen - tlen;
	if (!tlsp->sc->tlst.short_records)
		return (false);

	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_CBC) {
		/*
		 * For AES-CBC we have to send input from the start of
		 * the TLS record payload that is a multiple of the
		 * block size.  new_tlen rounds up tlen to the end of
		 * the containing AES block.  If this last block
		 * overlaps with the trailer, send the full record to
		 * generate the MAC.
		 */
		new_tlen = TLS_HEADER_LENGTH +
		    roundup2(tlen - TLS_HEADER_LENGTH, AES_BLOCK_LEN);
		if (rlen - new_tlen < trailer_len)
			return (false);

		*trailing_waste = new_tlen - tlen;
		*plen = new_tlen - m_tls->m_epg_hdrlen;
	} else {
		if (rlen - tlen < trailer_len ||
		    (rlen - tlen == trailer_len && request_ghash)) {
			/*
			 * For AES-GCM we have to send the full record
			 * if the end overlaps with the trailer and a
			 * partial GHASH isn't being sent.
			 */
			if (!send_partial_ghash)
				return (false);

			/*
			 * Will need to treat any excess trailer bytes as
			 * trailing waste.  *trailing_waste is already
			 * correct.
			 */
		} else {
			/*
			 * We can use AES-CTR or AES-GCM in partial GHASH
			 * mode to encrypt a partial PDU.
			 *
			 * The last block can be partially encrypted
			 * without any trailing waste.
			 */
			*trailing_waste = 0;
			*plen = tlen - m_tls->m_epg_hdrlen;
		}

		/*
		 * If this request starts at the first byte of the
		 * payload (so the previous request sent the full TLS
		 * header as a tunnel packet) and a partial GHASH is
		 * being requested, the full TLS header must be sent
		 * as input for the GHASH.
		 */
		if (mtod(m_tls, vm_offset_t) == m_tls->m_epg_hdrlen &&
		    request_ghash)
			return (true);

		/*
		 * In addition, we can minimize leading waste by
		 * starting encryption at the start of the closest AES
		 * block.
		 */
		if (mtod(m_tls, vm_offset_t) >= m_tls->m_epg_hdrlen) {
			*header_len = 0;
			*offset = mtod(m_tls, vm_offset_t) -
			    m_tls->m_epg_hdrlen;
			if (*offset >= *plen)
				*offset = *plen;
			else
				*offset = rounddown2(*offset, AES_BLOCK_LEN);

			/*
			 * If the request is just bytes from the trailer,
			 * trim the offset to the end of the payload.
			 */
			*offset = min(*offset, *plen);
			*plen -= *offset;
			*leading_waste -= (m_tls->m_epg_hdrlen + *offset);
		}
	}
	return (true);
}

/* Size of the AES-GCM TLS AAD for a given connection. */
static int
ktls_gcm_aad_len(struct tlspcb *tlsp)
{
	return (tlsp->tls13 ? sizeof(struct tls_aead_data_13) :
	    sizeof(struct tls_aead_data));
}

static int
ktls_wr_len(struct tlspcb *tlsp, struct mbuf *m, struct mbuf *m_tls,
    int *nsegsp)
{
	const struct tls_record_layer *hdr;
	u_int header_len, imm_len, offset, plen, rlen, tlen, wr_len;
	u_int leading_waste, trailing_waste;
	bool inline_key, last_ghash_frag, request_ghash, send_partial_ghash;
	bool short_record;

	M_ASSERTEXTPG(m_tls);

	/*
	 * The relative offset of the last byte to send from the TLS
	 * record.
	 */
	tlen = mtod(m_tls, vm_offset_t) + m_tls->m_len;
	if (tlen <= m_tls->m_epg_hdrlen) {
		/*
		 * For requests that only want to send the TLS header,
		 * send a tunnelled packet as immediate data.
		 */
		wr_len = sizeof(struct fw_eth_tx_pkt_wr) +
		    sizeof(struct cpl_tx_pkt_core) +
		    roundup2(m->m_len + m_tls->m_len, 16);
		if (wr_len > SGE_MAX_WR_LEN) {
			CTR(KTR_CXGBE,
		    "%s: %p TLS header-only packet too long (len %d)",
			    __func__, tlsp, m->m_len + m_tls->m_len);
		}

		/* This should always be the last TLS record in a chain. */
		MPASS(m_tls->m_next == NULL);
		*nsegsp = 0;
		return (wr_len);
	}

	hdr = (void *)m_tls->m_epg_hdr;
	rlen = TLS_HEADER_LENGTH + ntohs(hdr->tls_length);

	/*
	 * See if this request might make use of GHASH state.  This
	 * errs on the side of over-budgeting the WR size.
	 */
	last_ghash_frag = false;
	request_ghash = false;
	send_partial_ghash = false;
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM &&
	    tlsp->sc->tlst.partial_ghash && tlsp->sc->tlst.short_records) {
		u_int trailer_len;

		trailer_len = m_tls->m_epg_trllen;
		if (tlsp->tls13)
			trailer_len--;
		KASSERT(trailer_len == AES_GMAC_HASH_LEN,
		    ("invalid trailer length for AES-GCM"));

		/* Is this the start of a TLS record? */
		if (mtod(m_tls, vm_offset_t) <= m_tls->m_epg_hdrlen) {
			/*
			 * Might use partial GHASH if this doesn't
			 * send the full record.
			 */
			if (tlen < rlen) {
				if (tlen < (rlen - trailer_len))
					send_partial_ghash = true;
				request_ghash = true;
			}
		} else {
			send_partial_ghash = true;
			if (tlen < rlen)
				request_ghash = true;
			if (tlen >= (rlen - trailer_len))
				last_ghash_frag = true;
		}
	}

	/*
	 * Assume not sending partial GHASH for this call to get the
	 * larger size.
	 */
	short_record = ktls_is_short_record(tlsp, m_tls, tlen, rlen,
	    &header_len, &offset, &plen, &leading_waste, &trailing_waste,
	    false, request_ghash);

	inline_key = send_partial_ghash || tlsp->inline_key;

	/* Calculate the size of the work request. */
	wr_len = ktls_base_wr_size(tlsp, inline_key);

	if (send_partial_ghash)
		wr_len += AES_GMAC_HASH_LEN;

	if (leading_waste != 0 || trailing_waste != 0) {
		/*
		 * Partial records might require a SplitMode
		 * CPL_RX_PHYS_DSGL.
		 */
		wr_len += sizeof(struct cpl_t7_rx_phys_dsgl);
	}

	/* Budget for an LSO header even if we don't use it. */
	wr_len += sizeof(struct cpl_tx_pkt_lso_core);

	/*
	 * Headers (including the TLS header) are always sent as
	 * immediate data.  Short records include a raw AES IV as
	 * immediate data.  TLS 1.3 non-short records include a
	 * placeholder for the sequence number as immediate data.
	 * Short records using a partial hash may also need to send
	 * TLS AAD.  If a partial hash might be sent, assume a short
	 * record to get the larger size.
	 */
	imm_len = m->m_len + header_len;
	if (short_record || send_partial_ghash) {
		imm_len += AES_BLOCK_LEN;
		if (send_partial_ghash && header_len != 0)
			imm_len += ktls_gcm_aad_len(tlsp);
	} else if (tlsp->tls13)
		imm_len += sizeof(uint64_t);
	wr_len += roundup2(imm_len, 16);

	/*
	 * TLS record payload via DSGL.  For partial GCM mode we
	 * might need an extra SG entry for a placeholder.
	 */
	*nsegsp = sglist_count_mbuf_epg(m_tls, m_tls->m_epg_hdrlen + offset,
	    plen);
	wr_len += ktls_sgl_size(*nsegsp + (last_ghash_frag ? 1 : 0));

	if (request_ghash) {
		/* AES-GCM records might return a partial hash. */
		wr_len += sizeof(struct ulp_txpkt);
		wr_len += sizeof(struct ulptx_idata);
		wr_len += sizeof(struct cpl_tx_tls_ack);
		wr_len += sizeof(struct rss_header) +
		    sizeof(struct cpl_fw6_pld);
		wr_len += AES_GMAC_HASH_LEN;
	}

	wr_len = roundup2(wr_len, 16);
	return (wr_len);
}

/* Queue the next pending packet. */
static void
ktls_queue_next_packet(struct tlspcb *tlsp, bool enqueue_only)
{
#ifdef KTR
	struct ether_header *eh;
	struct tcphdr *tcp;
	tcp_seq tcp_seqno;
#endif
	struct mbuf *m;
	void *items[1];
	int rc;

	TXQ_LOCK_ASSERT_OWNED(tlsp->txq);
	KASSERT(tlsp->queue_mbufs, ("%s: mbufs not being queued for %p",
	    __func__, tlsp));
	for (;;) {
		m = mbufq_dequeue(&tlsp->pending_mbufs);
		if (m == NULL) {
			tlsp->queue_mbufs = false;
			return;
		}

#ifdef KTR
		eh = mtod(m, struct ether_header *);
		tcp = (struct tcphdr *)((char *)eh + m->m_pkthdr.l2hlen +
		    m->m_pkthdr.l3hlen);
		tcp_seqno = ntohl(tcp->th_seq);
#ifdef VERBOSE_TRACES
		CTR(KTR_CXGBE, "%s: pkt len %d TCP seq %u", __func__,
		    m->m_pkthdr.len, tcp_seqno);
#endif
#endif

		items[0] = m;
		if (enqueue_only)
			rc = mp_ring_enqueue_only(tlsp->txq->r, items, 1);
		else {
			TXQ_UNLOCK(tlsp->txq);
			rc = mp_ring_enqueue(tlsp->txq->r, items, 1, 256);
			TXQ_LOCK(tlsp->txq);
		}
		if (__predict_true(rc == 0))
			return;

		CTR(KTR_CXGBE, "%s: pkt len %d TCP seq %u dropped", __func__,
		    m->m_pkthdr.len, tcp_seqno);
		m_freem(m);
	}
}

int
t7_ktls_parse_pkt(struct mbuf *m)
{
	struct tlspcb *tlsp;
	struct ether_header *eh;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *tcp;
	struct mbuf *m_tls;
	void *items[1];
	int error, nsegs;
	u_int wr_len, tot_len;
	uint16_t eh_type;

	/*
	 * Locate headers in initial mbuf.
	 *
	 * XXX: This assumes all of the headers are in the initial mbuf.
	 * Could perhaps use m_advance() like parse_pkt() if that turns
	 * out to not be true.
	 */
	M_ASSERTPKTHDR(m);
	MPASS(m->m_pkthdr.snd_tag != NULL);
	tlsp = mst_to_tls(m->m_pkthdr.snd_tag);

	if (m->m_len <= sizeof(*eh) + sizeof(*ip)) {
		CTR(KTR_CXGBE, "%s: %p header mbuf too short", __func__, tlsp);
		return (EINVAL);
	}
	eh = mtod(m, struct ether_header *);
	eh_type = ntohs(eh->ether_type);
	if (eh_type == ETHERTYPE_VLAN) {
		struct ether_vlan_header *evh = (void *)eh;

		eh_type = ntohs(evh->evl_proto);
		m->m_pkthdr.l2hlen = sizeof(*evh);
	} else
		m->m_pkthdr.l2hlen = sizeof(*eh);

	switch (eh_type) {
	case ETHERTYPE_IP:
		ip = (struct ip *)(eh + 1);
		if (ip->ip_p != IPPROTO_TCP) {
			CTR(KTR_CXGBE, "%s: %p mbuf not IPPROTO_TCP", __func__,
			    tlsp);
			return (EINVAL);
		}
		m->m_pkthdr.l3hlen = ip->ip_hl * 4;
		break;
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(eh + 1);
		if (ip6->ip6_nxt != IPPROTO_TCP) {
			CTR(KTR_CXGBE, "%s: %p, mbuf not IPPROTO_TCP (%u)",
			    __func__, tlsp, ip6->ip6_nxt);
			return (EINVAL);
		}
		m->m_pkthdr.l3hlen = sizeof(struct ip6_hdr);
		break;
	default:
		CTR(KTR_CXGBE, "%s: %p mbuf not ETHERTYPE_IP{,V6}", __func__,
		    tlsp);
		return (EINVAL);
	}
	if (m->m_len < m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen +
	    sizeof(*tcp)) {
		CTR(KTR_CXGBE, "%s: %p header mbuf too short (2)", __func__,
		    tlsp);
		return (EINVAL);
	}
	tcp = (struct tcphdr *)((char *)(eh + 1) + m->m_pkthdr.l3hlen);
	m->m_pkthdr.l4hlen = tcp->th_off * 4;

	/* Bail if there is TCP payload before the TLS record. */
	if (m->m_len != m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen +
	    m->m_pkthdr.l4hlen) {
		CTR(KTR_CXGBE,
		    "%s: %p header mbuf bad length (%d + %d + %d != %d)",
		    __func__, tlsp, m->m_pkthdr.l2hlen, m->m_pkthdr.l3hlen,
		    m->m_pkthdr.l4hlen, m->m_len);
		return (EINVAL);
	}

	/* Assume all headers are in 'm' for now. */
	MPASS(m->m_next != NULL);
	MPASS(m->m_next->m_flags & M_EXTPG);

	tot_len = 0;

	/*
	 * Each of the remaining mbufs in the chain should reference a
	 * TLS record.
	 */
	for (m_tls = m->m_next; m_tls != NULL; m_tls = m_tls->m_next) {
		MPASS(m_tls->m_flags & M_EXTPG);

		wr_len = ktls_wr_len(tlsp, m, m_tls, &nsegs);
#ifdef VERBOSE_TRACES
		CTR(KTR_CXGBE, "%s: %p wr_len %d nsegs %d", __func__, tlsp,
		    wr_len, nsegs);
#endif
		if (wr_len > SGE_MAX_WR_LEN || nsegs > TX_SGL_SEGS)
			return (EFBIG);
		tot_len += roundup2(wr_len, EQ_ESIZE);

		/*
		 * Store 'nsegs' for the first TLS record in the
		 * header mbuf's metadata.
		 */
		if (m_tls == m->m_next)
			set_mbuf_nsegs(m, nsegs);
	}

	MPASS(tot_len != 0);
	set_mbuf_len16(m, tot_len / 16);

	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM) {
		/* Defer packets beyond what has been sent so far. */
		TXQ_LOCK(tlsp->txq);
		if (tlsp->queue_mbufs) {
			error = mbufq_enqueue(&tlsp->pending_mbufs, m);
			if (error == 0) {
#ifdef VERBOSE_TRACES
				CTR(KTR_CXGBE,
				    "%s: %p len16 %d nsegs %d TCP seq %u deferred",
				    __func__, tlsp, mbuf_len16(m),
				    mbuf_nsegs(m), ntohl(tcp->th_seq));
#endif
			}
			TXQ_UNLOCK(tlsp->txq);
			return (error);
		}
		tlsp->queue_mbufs = true;
		TXQ_UNLOCK(tlsp->txq);
	}

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: %p len16 %d nsegs %d", __func__, tlsp,
	    mbuf_len16(m), mbuf_nsegs(m));
#endif
	items[0] = m;
	error = mp_ring_enqueue(tlsp->txq->r, items, 1, 256);
	if (__predict_false(error != 0)) {
		if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM) {
			TXQ_LOCK(tlsp->txq);
			ktls_queue_next_packet(tlsp, false);
			TXQ_UNLOCK(tlsp->txq);
		}
	}
	return (error);
}

static inline bool
needs_vlan_insertion(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);

	return (m->m_flags & M_VLANTAG);
}

static inline uint64_t
pkt_ctrl1(struct sge_txq *txq, struct mbuf *m, uint16_t eh_type)
{
	uint64_t ctrl1;

	/* Checksums are always offloaded */
	if (eh_type == ETHERTYPE_IP) {
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	} else {
		MPASS(m->m_pkthdr.l3hlen == sizeof(struct ip6_hdr));
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP6) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	}
	txq->txcsum++;

	/* VLAN tag insertion */
	if (needs_vlan_insertion(m)) {
		ctrl1 |= F_TXPKT_VLAN_VLD |
		    V_TXPKT_VLAN(m->m_pkthdr.ether_vtag);
		txq->vlan_insertion++;
	}

	return (ctrl1);
}

static inline void *
write_lso_cpl(void *cpl, struct mbuf *m0, uint16_t mss, uint16_t eh_type,
    int total_len)
{
	struct cpl_tx_pkt_lso_core *lso;
	uint32_t ctrl;

	KASSERT(m0->m_pkthdr.l2hlen > 0 && m0->m_pkthdr.l3hlen > 0 &&
	    m0->m_pkthdr.l4hlen > 0,
	    ("%s: mbuf %p needs TSO but missing header lengths",
		__func__, m0));

	ctrl = V_LSO_OPCODE(CPL_TX_PKT_LSO) |
	    F_LSO_FIRST_SLICE | F_LSO_LAST_SLICE |
	    V_LSO_ETHHDR_LEN((m0->m_pkthdr.l2hlen - ETHER_HDR_LEN) >> 2) |
	    V_LSO_IPHDR_LEN(m0->m_pkthdr.l3hlen >> 2) |
	    V_LSO_TCPHDR_LEN(m0->m_pkthdr.l4hlen >> 2);
	if (eh_type == ETHERTYPE_IPV6)
		ctrl |= F_LSO_IPV6;

	lso = cpl;
	lso->lso_ctrl = htobe32(ctrl);
	lso->ipid_ofst = htobe16(0);
	lso->mss = htobe16(mss);
	lso->seqno_offset = htobe32(0);
	lso->len = htobe32(total_len);

	return (lso + 1);
}

static inline void *
write_tx_tls_ack(void *dst, u_int rx_chid, u_int hash_len, bool ghash_lcb)
{
	struct cpl_tx_tls_ack *cpl;
	uint32_t flags;

	flags = ghash_lcb ? F_CPL_TX_TLS_ACK_LCB : F_CPL_TX_TLS_ACK_PHASH;
	cpl = dst;
	cpl->op_to_Rsvd2 = htobe32(V_CPL_TX_TLS_ACK_OPCODE(CPL_TX_TLS_ACK) |
	    V_T7_CPL_TX_TLS_ACK_RXCHID(rx_chid) | F_CPL_TX_TLS_ACK_ULPTXLPBK |
	    flags);

	/* 32 == AckEncCpl, 16 == LCB */
	cpl->PldLen = htobe32(V_CPL_TX_TLS_ACK_PLDLEN(32 + 16 + hash_len));
	cpl->Rsvd3 = 0;

	return (cpl + 1);
}

static inline void *
write_fw6_pld(void *dst, u_int rx_chid, u_int rx_qid, u_int hash_len,
    uint64_t cookie)
{
	struct rss_header *rss;
	struct cpl_fw6_pld *cpl;

	rss = dst;
	memset(rss, 0, sizeof(*rss));
	rss->opcode = CPL_FW6_PLD;
	rss->qid = htobe16(rx_qid);
	rss->channel = rx_chid;

	cpl = (void *)(rss + 1);
	memset(cpl, 0, sizeof(*cpl));
	cpl->opcode = CPL_FW6_PLD;
	cpl->len = htobe16(hash_len);
	cpl->data[1] = htobe64(cookie);

	return (cpl + 1);
}

static inline void *
write_split_mode_rx_phys(void *dst, struct mbuf *m, struct mbuf *m_tls,
    u_int crypto_hdr_len, u_int leading_waste, u_int trailing_waste)
{
	struct cpl_t7_rx_phys_dsgl *cpl;
	uint16_t *len;
	uint8_t numsge;

	/* Forward first (3) and third (1) segments. */
	numsge = 0xa;

	cpl = dst;
	cpl->ot.opcode = CPL_RX_PHYS_DSGL;
	cpl->PhysAddrFields_lo_to_NumSGE =
	    htobe32(F_CPL_T7_RX_PHYS_DSGL_SPLITMODE |
	    V_CPL_T7_RX_PHYS_DSGL_NUMSGE(numsge));

	len = (uint16_t *)(cpl->RSSCopy);

	/*
	 * First segment always contains packet headers as well as
	 * transmit-related CPLs.
	 */
	len[0] = htobe16(crypto_hdr_len);

	/*
	 * Second segment is "gap" of data to drop at the front of the
	 * TLS record.
	 */
	len[1] = htobe16(leading_waste);

	/* Third segment is how much of the TLS record to send. */
	len[2] = htobe16(m_tls->m_len);

	/* Fourth segment is how much data to drop at the end. */
	len[3] = htobe16(trailing_waste);

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: forward %u skip %u forward %u skip %u",
	    __func__, be16toh(len[0]), be16toh(len[1]), be16toh(len[2]),
	    be16toh(len[3]));
#endif
	return (cpl + 1);
}

/*
 * If the SGL ends on an address that is not 16 byte aligned, this function will
 * add a 0 filled flit at the end.
 */
static void *
write_gl_to_buf(struct sglist *gl, caddr_t to)
{
	struct sglist_seg *seg;
	__be64 *flitp;
	struct ulptx_sgl *usgl;
	int i, nflits, nsegs;

	KASSERT(((uintptr_t)to & 0xf) == 0,
	    ("%s: SGL must start at a 16 byte boundary: %p", __func__, to));

	nsegs = gl->sg_nseg;
	MPASS(nsegs > 0);

	nflits = (3 * (nsegs - 1)) / 2 + ((nsegs - 1) & 1) + 2;
	flitp = (__be64 *)to;
	seg = &gl->sg_segs[0];
	usgl = (void *)flitp;

	usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
	    V_ULPTX_NSGE(nsegs));
	usgl->len0 = htobe32(seg->ss_len);
	usgl->addr0 = htobe64(seg->ss_paddr);
	seg++;

	for (i = 0; i < nsegs - 1; i++, seg++) {
		usgl->sge[i / 2].len[i & 1] = htobe32(seg->ss_len);
		usgl->sge[i / 2].addr[i & 1] = htobe64(seg->ss_paddr);
	}
	if (i & 1)
		usgl->sge[i / 2].len[1] = htobe32(0);
	flitp += nflits;

	if (nflits & 1) {
		MPASS(((uintptr_t)flitp) & 0xf);
		*flitp++ = 0;
	}

	MPASS((((uintptr_t)flitp) & 0xf) == 0);
	return (flitp);
}

static inline void
copy_to_txd(struct sge_eq *eq, const char *from, caddr_t *to, int len)
{

	MPASS((uintptr_t)(*to) >= (uintptr_t)&eq->desc[0]);
	MPASS((uintptr_t)(*to) < (uintptr_t)&eq->desc[eq->sidx]);

	if (__predict_true((uintptr_t)(*to) + len <=
	    (uintptr_t)&eq->desc[eq->sidx])) {
		bcopy(from, *to, len);
		(*to) += len;
		if ((uintptr_t)(*to) == (uintptr_t)&eq->desc[eq->sidx])
			(*to) = (caddr_t)eq->desc;
	} else {
		int portion = (uintptr_t)&eq->desc[eq->sidx] - (uintptr_t)(*to);

		bcopy(from, *to, portion);
		from += portion;
		portion = len - portion;	/* remaining */
		bcopy(from, (void *)eq->desc, portion);
		(*to) = (caddr_t)eq->desc + portion;
	}
}

static int
ktls_write_tunnel_packet(struct sge_txq *txq, void *dst, struct mbuf *m,
    const void *src, u_int len, u_int available, tcp_seq tcp_seqno, u_int pidx,
    uint16_t eh_type, bool last_wr)
{
	struct tx_sdesc *txsd;
	struct fw_eth_tx_pkt_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	uint32_t ctrl;
	int len16, ndesc, pktlen;
	struct ether_header *eh;
	struct ip *ip, newip;
	struct ip6_hdr *ip6, newip6;
	struct tcphdr *tcp, newtcp;
	caddr_t out;

	TXQ_LOCK_ASSERT_OWNED(txq);
	M_ASSERTPKTHDR(m);

	wr = dst;
	pktlen = m->m_len + len;
	ctrl = sizeof(struct cpl_tx_pkt_core) + pktlen;
	len16 = howmany(sizeof(struct fw_eth_tx_pkt_wr) + ctrl, 16);
	ndesc = tx_len16_to_desc(len16);
	MPASS(ndesc <= available);

	/* Firmware work request header */
	/* TODO: Handle VF work request. */
	wr->op_immdlen = htobe32(V_FW_WR_OP(FW_ETH_TX_PKT_WR) |
	    V_FW_ETH_TX_PKT_WR_IMMDLEN(ctrl));

	ctrl = V_FW_WR_LEN16(len16);
	wr->equiq_to_len16 = htobe32(ctrl);
	wr->r3 = 0;

	cpl = (void *)(wr + 1);

	/* CPL header */
	cpl->ctrl0 = txq->cpl_ctrl0;
	cpl->pack = 0;
	cpl->len = htobe16(pktlen);

	out = (void *)(cpl + 1);

	/* Copy over Ethernet header. */
	eh = mtod(m, struct ether_header *);
	copy_to_txd(&txq->eq, (caddr_t)eh, &out, m->m_pkthdr.l2hlen);

	/* Fixup length in IP header and copy out. */
	if (eh_type == ETHERTYPE_IP) {
		ip = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip = *ip;
		newip.ip_len = htons(pktlen - m->m_pkthdr.l2hlen);
		copy_to_txd(&txq->eq, (caddr_t)&newip, &out, sizeof(newip));
		if (m->m_pkthdr.l3hlen > sizeof(*ip))
			copy_to_txd(&txq->eq, (caddr_t)(ip + 1), &out,
			    m->m_pkthdr.l3hlen - sizeof(*ip));
	} else {
		ip6 = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip6 = *ip6;
		newip6.ip6_plen = htons(pktlen - m->m_pkthdr.l2hlen -
		    sizeof(*ip6));
		copy_to_txd(&txq->eq, (caddr_t)&newip6, &out, sizeof(newip6));
		MPASS(m->m_pkthdr.l3hlen == sizeof(*ip6));
	}
	cpl->ctrl1 = htobe64(pkt_ctrl1(txq, m, eh_type));

	/* Set sequence number in TCP header. */
	tcp = (void *)((char *)eh + m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen);
	newtcp = *tcp;
	newtcp.th_seq = htonl(tcp_seqno);
	copy_to_txd(&txq->eq, (caddr_t)&newtcp, &out, sizeof(newtcp));

	/* Copy rest of TCP header. */
	copy_to_txd(&txq->eq, (caddr_t)(tcp + 1), &out, m->m_len -
	    (m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen + sizeof(*tcp)));

	/* Copy the payload data. */
	copy_to_txd(&txq->eq, src, &out, len);
	txq->imm_wrs++;

	txq->txpkt_wrs++;

	txsd = &txq->sdesc[pidx];
	if (last_wr)
		txsd->m = m;
	else
		txsd->m = NULL;
	txsd->desc_used = ndesc;

	return (ndesc);
}

static int
ktls_write_tls_wr(struct tlspcb *tlsp, struct sge_txq *txq,
    void *dst, struct mbuf *m, struct tcphdr *tcp, struct mbuf *m_tls,
    u_int available, tcp_seq tcp_seqno, u_int pidx, uint16_t eh_type,
    uint16_t mss)
{
	struct sge_eq *eq = &txq->eq;
	struct tx_sdesc *txsd;
	struct fw_ulptx_wr *wr;
	struct ulp_txpkt *txpkt;
	struct ulptx_sc_memrd *memrd;
	struct ulptx_idata *idata;
	struct cpl_tx_sec_pdu *sec_pdu;
	struct cpl_tx_pkt_core *tx_pkt;
	const struct tls_record_layer *hdr;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *newtcp;
	char *iv, *out;
	u_int aad_start, aad_stop;
	u_int auth_start, auth_stop, auth_insert;
	u_int cipher_start, cipher_stop, iv_offset;
	u_int header_len, offset, plen, rlen, tlen;
	u_int imm_len, ndesc, nsegs, txpkt_lens[2], wr_len;
	u_int cpl_len, crypto_hdr_len, post_key_context_len;
	u_int leading_waste, trailing_waste;
	u_short ip_len;
	bool inline_key, ghash_lcb, last_ghash_frag, last_wr, need_lso;
	bool request_ghash, send_partial_ghash, short_record, split_mode;
	bool using_scratch;

	MPASS(tlsp->txq == txq);
	M_ASSERTEXTPG(m_tls);

	/* Final work request for this mbuf chain? */
	last_wr = (m_tls->m_next == NULL);

	/*
	 * The relative offset of the last byte to send from the TLS
	 * record.
	 */
	tlen = mtod(m_tls, vm_offset_t) + m_tls->m_len;
	if (tlen <= m_tls->m_epg_hdrlen) {
		/*
		 * For requests that only want to send the TLS header,
		 * send a tunnelled packet as immediate data.
		 */
#ifdef VERBOSE_TRACES
		CTR(KTR_CXGBE, "%s: %p header-only TLS record %u", __func__,
		    tlsp, (u_int)m_tls->m_epg_seqno);
#endif
		/* This should always be the last TLS record in a chain. */
		MPASS(last_wr);

		txq->kern_tls_header++;

		return (ktls_write_tunnel_packet(txq, dst, m,
		    (char *)m_tls->m_epg_hdr + mtod(m_tls, vm_offset_t),
		    m_tls->m_len, available, tcp_seqno, pidx, eh_type,
		    last_wr));
	}

	/* Locate the TLS header. */
	hdr = (void *)m_tls->m_epg_hdr;
	rlen = TLS_HEADER_LENGTH + ntohs(hdr->tls_length);

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: offset %lu len %u TCP seq %u TLS record %u",
	    __func__, mtod(m_tls, vm_offset_t), m_tls->m_len, tcp_seqno,
	    (u_int)m_tls->m_epg_seqno);
#endif

	/* Should this request make use of GHASH state? */
	ghash_lcb = false;
	last_ghash_frag = false;
	request_ghash = false;
	send_partial_ghash = false;
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM &&
	    tlsp->sc->tlst.partial_ghash && tlsp->sc->tlst.short_records) {
		u_int trailer_len;

		trailer_len = m_tls->m_epg_trllen;
		if (tlsp->tls13)
			trailer_len--;
		KASSERT(trailer_len == AES_GMAC_HASH_LEN,
		    ("invalid trailer length for AES-GCM"));

		/* Is this the start of a TLS record? */
		if (mtod(m_tls, vm_offset_t) <= m_tls->m_epg_hdrlen) {
			/*
			 * If this is the very first TLS record or
			 * if this is a newer TLS record, request a partial
			 * hash, but not if we are going to send the whole
			 * thing.
			 */
			if ((tlsp->ghash_tls_seqno == 0 ||
			    tlsp->ghash_tls_seqno < m_tls->m_epg_seqno) &&
			    tlen < rlen) {
				/*
				 * If we are only missing part or all
				 * of the trailer, send a normal full
				 * record but request the hash.
				 * Otherwise, use partial GHASH mode.
				 */
				if (tlen >= (rlen - trailer_len))
					ghash_lcb = true;
				else
					send_partial_ghash = true;
				request_ghash = true;
				tlsp->ghash_tls_seqno = m_tls->m_epg_seqno;
			}
		} else if (tlsp->ghash_tls_seqno == m_tls->m_epg_seqno &&
		    tlsp->ghash_valid) {
			/*
			 * Compute the offset of the first AES block as
			 * is done in ktls_is_short_record.
			 */
			if (rlen - tlen < trailer_len)
				plen = rlen - (m_tls->m_epg_hdrlen +
				    trailer_len);
			else
				plen = tlen - m_tls->m_epg_hdrlen;
			offset = mtod(m_tls, vm_offset_t) - m_tls->m_epg_hdrlen;
			if (offset >= plen)
				offset = plen;
			else
				offset = rounddown2(offset, AES_BLOCK_LEN);
			if (tlsp->ghash_offset == offset) {
				if (offset == plen) {
					/*
					 * Send a partial trailer as a
					 * tunnelled packet as
					 * immediate data.
					 */
#ifdef VERBOSE_TRACES
					CTR(KTR_CXGBE,
					    "%s: %p trailer-only TLS record %u",
					    __func__, tlsp,
					    (u_int)m_tls->m_epg_seqno);
#endif

					txq->kern_tls_trailer++;

					offset = mtod(m_tls, vm_offset_t) -
					    (m_tls->m_epg_hdrlen + plen);
					KASSERT(offset <= AES_GMAC_HASH_LEN,
					    ("offset outside of trailer"));
					return (ktls_write_tunnel_packet(txq,
					    dst, m, tlsp->ghash + offset,
					    m_tls->m_len, available, tcp_seqno,
					    pidx, eh_type, last_wr));
				}

				/*
				 * If this request sends the end of
				 * the payload, it is the last
				 * fragment.
				 */
				if (tlen >= (rlen - trailer_len)) {
					last_ghash_frag = true;
					ghash_lcb = true;
				}

				/*
				 * Only use partial GCM mode (rather
				 * than an AES-CTR short record) if
				 * there is input auth data to pass to
				 * the GHASH.  That is true so long as
				 * there is at least one full block of
				 * payload data, or if the remaining
				 * payload data is the final partial
				 * block.
				 */
				if (plen - offset >= GMAC_BLOCK_LEN ||
				    last_ghash_frag) {
					send_partial_ghash = true;

					/*
					 * If not sending the complete
					 * end of the record, this is
					 * a middle request so needs
					 * to request an updated
					 * partial hash.
					 */
					if (tlen < rlen)
						request_ghash = true;
				}
			}
		}
	}

	short_record = ktls_is_short_record(tlsp, m_tls, tlen, rlen,
	    &header_len, &offset, &plen, &leading_waste, &trailing_waste,
	    send_partial_ghash, request_ghash);

	if (short_record) {
#ifdef VERBOSE_TRACES
		CTR(KTR_CXGBE,
		    "%s: %p short TLS record %u hdr %u offs %u plen %u",
		    __func__, tlsp, (u_int)m_tls->m_epg_seqno, header_len,
		    offset, plen);
		if (send_partial_ghash) {
			if (header_len != 0)
				CTR(KTR_CXGBE, "%s: %p sending initial GHASH",
				    __func__, tlsp);
			else
				CTR(KTR_CXGBE, "%s: %p sending partial GHASH for offset %u%s",
				    __func__, tlsp, tlsp->ghash_offset,
				    last_ghash_frag ? ", last_frag" : "");
		}
#endif
		KASSERT(send_partial_ghash || !request_ghash,
		    ("requesting but not sending partial hash for short record"));
	} else {
		KASSERT(!send_partial_ghash,
		    ("sending partial hash with full record"));
	}

	if (tlen < rlen && m_tls->m_next == NULL &&
	    (tcp->th_flags & TH_FIN) != 0) {
		txq->kern_tls_fin_short++;
#ifdef INVARIANTS
		panic("%s: FIN on short TLS record", __func__);
#endif
	}

	/*
	 * Use cached value for first record in chain if not using
	 * partial GCM mode. ktls_parse_pkt() calculates nsegs based
	 * on send_partial_ghash being false.
	 */
	if (m->m_next == m_tls && !send_partial_ghash)
		nsegs = mbuf_nsegs(m);
	else
		nsegs = sglist_count_mbuf_epg(m_tls,
		    m_tls->m_epg_hdrlen + offset, plen);

	/* Determine if we need an LSO header. */
	need_lso = (m_tls->m_len > mss);

	/* Calculate the size of the TLS work request. */
	inline_key = send_partial_ghash || tlsp->inline_key;
	wr_len = ktls_base_wr_size(tlsp, inline_key);

	if (send_partial_ghash) {
		/* Inline key context includes partial hash in OPAD. */
		wr_len += AES_GMAC_HASH_LEN;
	}

	/*
	 * SplitMode is required if there is any thing we need to trim
	 * from the crypto output, either at the front or end of the
	 * record.  Note that short records might not need trimming.
	 */
	split_mode = leading_waste != 0 || trailing_waste != 0;
	if (split_mode) {
		/*
		 * Partial records require a SplitMode
		 * CPL_RX_PHYS_DSGL.
		 */
		wr_len += sizeof(struct cpl_t7_rx_phys_dsgl);
	}

	if (need_lso)
		wr_len += sizeof(struct cpl_tx_pkt_lso_core);

	imm_len = m->m_len + header_len;
	if (short_record) {
		imm_len += AES_BLOCK_LEN;
		if (send_partial_ghash && header_len != 0)
			imm_len += ktls_gcm_aad_len(tlsp);
	} else if (tlsp->tls13)
		imm_len += sizeof(uint64_t);
	wr_len += roundup2(imm_len, 16);
	wr_len += ktls_sgl_size(nsegs + (last_ghash_frag ? 1 : 0));
	wr_len = roundup2(wr_len, 16);
	txpkt_lens[0] = wr_len - sizeof(*wr);

	if (request_ghash) {
		/*
		 * Requesting the hash entails a second ULP_TX_PKT
		 * containing CPL_TX_TLS_ACK, CPL_FW6_PLD, and space
		 * for the hash.
		 */
		txpkt_lens[1] = sizeof(struct ulp_txpkt);
		txpkt_lens[1] += sizeof(struct ulptx_idata);
		txpkt_lens[1] += sizeof(struct cpl_tx_tls_ack);
		txpkt_lens[1] += sizeof(struct rss_header) +
		    sizeof(struct cpl_fw6_pld);
		txpkt_lens[1] += AES_GMAC_HASH_LEN;
		wr_len += txpkt_lens[1];
	} else
		txpkt_lens[1] = 0;

	ndesc = howmany(wr_len, EQ_ESIZE);
	MPASS(ndesc <= available);

	/*
	 * Use the per-txq scratch pad if near the end of the ring to
	 * simplify handling of wrap-around.
	 */
	using_scratch = (eq->sidx - pidx < ndesc);
	if (using_scratch)
		wr = (void *)txq->ss;
	else
		wr = dst;

	/* FW_ULPTX_WR */
	wr->op_to_compl = htobe32(V_FW_WR_OP(FW_ULPTX_WR));
	wr->flowid_len16 = htobe32(F_FW_ULPTX_WR_DATA |
	    V_FW_WR_LEN16(wr_len / 16));
	wr->cookie = 0;

	/* ULP_TXPKT */
	txpkt = (void *)(wr + 1);
	txpkt->cmd_dest = htobe32(V_ULPTX_CMD(ULP_TX_PKT) |
	    V_ULP_TXPKT_DATAMODIFY(0) |
	    V_T7_ULP_TXPKT_CHANNELID(tlsp->vi->pi->port_id) |
	    V_ULP_TXPKT_DEST(0) |
	    V_ULP_TXPKT_CMDMORE(request_ghash ? 1 : 0) |
	    V_ULP_TXPKT_FID(txq->eq.cntxt_id) | V_ULP_TXPKT_RO(1));
	txpkt->len = htobe32(howmany(txpkt_lens[0], 16));

	/* ULPTX_IDATA sub-command */
	idata = (void *)(txpkt + 1);
	idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM) |
	    V_ULP_TX_SC_MORE(1));
	idata->len = sizeof(struct cpl_tx_sec_pdu);

	/*
	 * After the key context comes CPL_RX_PHYS_DSGL, CPL_TX_*, and
	 * immediate data containing headers.  When using an inline
	 * key, these are counted as part of this ULPTX_IDATA.  When
	 * reading the key from memory, these are part of a separate
	 * ULPTX_IDATA.
	 */
	cpl_len = sizeof(struct cpl_tx_pkt_core);
	if (need_lso)
		cpl_len += sizeof(struct cpl_tx_pkt_lso_core);
	if (split_mode)
		cpl_len += sizeof(struct cpl_t7_rx_phys_dsgl);
	post_key_context_len = cpl_len + imm_len;

	if (inline_key) {
		idata->len += tlsp->tx_key_info_size + post_key_context_len;
		if (send_partial_ghash) {
			/* Partial GHASH in key context. */
			idata->len += AES_GMAC_HASH_LEN;
		}
	}
	idata->len = htobe32(idata->len);

	/* CPL_TX_SEC_PDU */
	sec_pdu = (void *)(idata + 1);

	/*
	 * Packet headers are passed through unchanged by the crypto
	 * engine by marking them as header data in SCMD0.
	 */
	crypto_hdr_len = m->m_len;

	if (send_partial_ghash) {
		/*
		 * For short records using a partial hash, the TLS
		 * header is counted as header data in SCMD0.  TLS AAD
		 * is next (if AAD is present) followed by the AES-CTR
		 * IV.  Last is the cipher region for the payload.
		 */
		if (header_len != 0) {
			aad_start = 1;
			aad_stop = ktls_gcm_aad_len(tlsp);
		} else {
			aad_start = 0;
			aad_stop = 0;
		}
		iv_offset = aad_stop + 1;
		cipher_start = iv_offset + AES_BLOCK_LEN;
		cipher_stop = 0;
		if (last_ghash_frag) {
			auth_start = cipher_start;
			auth_stop = AES_GMAC_HASH_LEN;
			auth_insert = auth_stop;
		} else if (plen < GMAC_BLOCK_LEN) {
			/*
			 * A request that sends part of the first AES
			 * block will only have AAD.
			 */
			KASSERT(header_len != 0,
			    ("%s: partial GHASH with no auth", __func__));
			auth_start = 0;
			auth_stop = 0;
			auth_insert = 0;
		} else {
			auth_start = cipher_start;
			auth_stop = plen % GMAC_BLOCK_LEN;
			auth_insert = 0;
		}

		sec_pdu->pldlen = htobe32(aad_stop + AES_BLOCK_LEN + plen +
		    (last_ghash_frag ? AES_GMAC_HASH_LEN : 0));

		/*
		 * For short records, the TLS header is treated as
		 * header data.
		 */
		crypto_hdr_len += header_len;

		/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
		sec_pdu->seqno_numivs = tlsp->scmd0_partial.seqno_numivs;
		sec_pdu->ivgen_hdrlen = tlsp->scmd0_partial.ivgen_hdrlen;
		if (last_ghash_frag)
			sec_pdu->ivgen_hdrlen |= V_SCMD_LAST_FRAG(1);
		else
			sec_pdu->ivgen_hdrlen |= V_SCMD_MORE_FRAGS(1);
		sec_pdu->ivgen_hdrlen = htobe32(sec_pdu->ivgen_hdrlen |
		    V_SCMD_HDR_LEN(crypto_hdr_len));

		txq->kern_tls_partial_ghash++;
	} else if (short_record) {
		/*
		 * For short records without a partial hash, the TLS
		 * header is counted as header data in SCMD0 and the
		 * IV is next, followed by a cipher region for the
		 * payload.
		 */
		aad_start = 0;
		aad_stop = 0;
		iv_offset = 1;
		auth_start = 0;
		auth_stop = 0;
		auth_insert = 0;
		cipher_start = AES_BLOCK_LEN + 1;
		cipher_stop = 0;

		sec_pdu->pldlen = htobe32(AES_BLOCK_LEN + plen);

		/*
		 * For short records, the TLS header is treated as
		 * header data.
		 */
		crypto_hdr_len += header_len;

		/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
		sec_pdu->seqno_numivs = tlsp->scmd0_short.seqno_numivs;
		sec_pdu->ivgen_hdrlen = htobe32(
		    tlsp->scmd0_short.ivgen_hdrlen |
		    V_SCMD_HDR_LEN(crypto_hdr_len));

		txq->kern_tls_short++;
	} else {
		/*
		 * AAD is TLS header.  IV is after AAD for TLS < 1.3.
		 * For TLS 1.3, a placeholder for the TLS sequence
		 * number is provided as an IV before the AAD.  The
		 * cipher region starts after the AAD and IV.  See
		 * comments in ccr_authenc() and ccr_gmac() in
		 * t4_crypto.c regarding cipher and auth start/stop
		 * values.
		 */
		if (tlsp->tls13) {
			iv_offset = 1;
			aad_start = 1 + sizeof(uint64_t);
			aad_stop = sizeof(uint64_t) + TLS_HEADER_LENGTH;
			cipher_start = aad_stop + 1;
		} else {
			aad_start = 1;
			aad_stop = TLS_HEADER_LENGTH;
			iv_offset = TLS_HEADER_LENGTH + 1;
			cipher_start = m_tls->m_epg_hdrlen + 1;
		}
		if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM) {
			cipher_stop = 0;
			auth_start = cipher_start;
			auth_stop = 0;
			auth_insert = 0;
		} else {
			cipher_stop = 0;
			auth_start = cipher_start;
			auth_stop = 0;
			auth_insert = 0;
		}

		sec_pdu->pldlen = htobe32((tlsp->tls13 ? sizeof(uint64_t) : 0) +
		    m_tls->m_epg_hdrlen + plen);

		/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
		sec_pdu->seqno_numivs = tlsp->scmd0.seqno_numivs;
		sec_pdu->ivgen_hdrlen = htobe32(tlsp->scmd0.ivgen_hdrlen |
		    V_SCMD_HDR_LEN(crypto_hdr_len));

		if (split_mode)
			txq->kern_tls_partial++;
		else
			txq->kern_tls_full++;
	}
	sec_pdu->op_ivinsrtofst = htobe32(
	    V_CPL_TX_SEC_PDU_OPCODE(CPL_TX_SEC_PDU) |
	    V_CPL_TX_SEC_PDU_CPLLEN(cpl_len / 8) |
	    V_CPL_TX_SEC_PDU_PLACEHOLDER(send_partial_ghash ? 1 : 0) |
	    V_CPL_TX_SEC_PDU_IVINSRTOFST(iv_offset));
	sec_pdu->aadstart_cipherstop_hi = htobe32(
	    V_CPL_TX_SEC_PDU_AADSTART(aad_start) |
	    V_CPL_TX_SEC_PDU_AADSTOP(aad_stop) |
	    V_CPL_TX_SEC_PDU_CIPHERSTART(cipher_start) |
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_HI(cipher_stop >> 4));
	sec_pdu->cipherstop_lo_authinsert = htobe32(
	    V_CPL_TX_SEC_PDU_CIPHERSTOP_LO(cipher_stop & 0xf) |
	    V_CPL_TX_SEC_PDU_AUTHSTART(auth_start) |
	    V_CPL_TX_SEC_PDU_AUTHSTOP(auth_stop) |
	    V_CPL_TX_SEC_PDU_AUTHINSERT(auth_insert));

	if (send_partial_ghash && last_ghash_frag) {
		uint64_t aad_len, cipher_len;

		aad_len = ktls_gcm_aad_len(tlsp);
		cipher_len = rlen - (m_tls->m_epg_hdrlen + AES_GMAC_HASH_LEN);
		sec_pdu->scmd1 = htobe64(aad_len << 44 | cipher_len);
	} else
		sec_pdu->scmd1 = htobe64(m_tls->m_epg_seqno);

	/* Key context */
	out = (void *)(sec_pdu + 1);
	if (inline_key) {
		memcpy(out, &tlsp->keyctx, tlsp->tx_key_info_size);
		if (send_partial_ghash) {
			struct tls_keyctx *keyctx = (void *)out;

			keyctx->u.txhdr.ctxlen++;
			keyctx->u.txhdr.dualck_to_txvalid &= ~htobe16(
			    V_KEY_CONTEXT_MK_SIZE(M_KEY_CONTEXT_MK_SIZE));
			keyctx->u.txhdr.dualck_to_txvalid |= htobe16(
			    F_KEY_CONTEXT_OPAD_PRESENT |
			    V_KEY_CONTEXT_MK_SIZE(0));
		}
		out += tlsp->tx_key_info_size;
		if (send_partial_ghash) {
			if (header_len != 0)
				memset(out, 0, AES_GMAC_HASH_LEN);
			else
				memcpy(out, tlsp->ghash, AES_GMAC_HASH_LEN);
			out += AES_GMAC_HASH_LEN;
		}
	} else {
		/* ULPTX_SC_MEMRD to read key context. */
		memrd = (void *)out;
		memrd->cmd_to_len = htobe32(V_ULPTX_CMD(ULP_TX_SC_MEMRD) |
		    V_ULP_TX_SC_MORE(1) |
		    V_ULPTX_LEN16(tlsp->tx_key_info_size >> 4));
		memrd->addr = htobe32(tlsp->tx_key_addr >> 5);

		/* ULPTX_IDATA for CPL_TX_* and headers. */
		idata = (void *)(memrd + 1);
		idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM) |
		    V_ULP_TX_SC_MORE(1));
		idata->len = htobe32(post_key_context_len);

		out = (void *)(idata + 1);
	}

	/* CPL_RX_PHYS_DSGL */
	if (split_mode) {
		crypto_hdr_len = sizeof(struct cpl_tx_pkt_core);
		if (need_lso)
			crypto_hdr_len += sizeof(struct cpl_tx_pkt_lso_core);
		crypto_hdr_len += m->m_len;
		out = write_split_mode_rx_phys(out, m, m_tls, crypto_hdr_len,
		    leading_waste, trailing_waste);
	}

	/* CPL_TX_PKT_LSO */
	if (need_lso) {
		out = write_lso_cpl(out, m, mss, eh_type, m->m_len +
		    m_tls->m_len);
		txq->tso_wrs++;
	}

	/* CPL_TX_PKT_XT */
	tx_pkt = (void *)out;
	tx_pkt->ctrl0 = txq->cpl_ctrl0;
	tx_pkt->ctrl1 = htobe64(pkt_ctrl1(txq, m, eh_type));
	tx_pkt->pack = 0;
	tx_pkt->len = htobe16(m->m_len + m_tls->m_len);

	/* Copy the packet headers. */
	out = (void *)(tx_pkt + 1);
	memcpy(out, mtod(m, char *), m->m_len);

	/* Modify the packet length in the IP header. */
	ip_len = m->m_len + m_tls->m_len - m->m_pkthdr.l2hlen;
	if (eh_type == ETHERTYPE_IP) {
		ip = (void *)(out + m->m_pkthdr.l2hlen);
		be16enc(&ip->ip_len, ip_len);
	} else {
		ip6 = (void *)(out + m->m_pkthdr.l2hlen);
		be16enc(&ip6->ip6_plen, ip_len - sizeof(*ip6));
	}

	/* Modify sequence number and flags in TCP header. */
	newtcp = (void *)(out + m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen);
	be32enc(&newtcp->th_seq, tcp_seqno);
	if (!last_wr)
		newtcp->th_flags = tcp->th_flags & ~(TH_PUSH | TH_FIN);
	out += m->m_len;

	/*
	 * Insert placeholder for sequence number as IV for TLS 1.3
	 * non-short records.
	 */
	if (tlsp->tls13 && !short_record) {
		memset(out, 0, sizeof(uint64_t));
		out += sizeof(uint64_t);
	}

	/* Populate the TLS header */
	memcpy(out, m_tls->m_epg_hdr, header_len);
	out += header_len;

	/* TLS AAD for short records using a partial hash. */
	if (send_partial_ghash && header_len != 0) {
		if (tlsp->tls13) {
			struct tls_aead_data_13 ad;

			ad.type = hdr->tls_type;
			ad.tls_vmajor = hdr->tls_vmajor;
			ad.tls_vminor = hdr->tls_vminor;
			ad.tls_length = hdr->tls_length;
			memcpy(out, &ad, sizeof(ad));
			out += sizeof(ad);
		} else {
			struct tls_aead_data ad;
			uint16_t cipher_len;

			cipher_len = rlen -
			    (m_tls->m_epg_hdrlen + AES_GMAC_HASH_LEN);
			ad.seq = htobe64(m_tls->m_epg_seqno);
			ad.type = hdr->tls_type;
			ad.tls_vmajor = hdr->tls_vmajor;
			ad.tls_vminor = hdr->tls_vminor;
			ad.tls_length = htons(cipher_len);
			memcpy(out, &ad, sizeof(ad));
			out += sizeof(ad);
		}
	}

	/* AES IV for a short record. */
	if (short_record) {
		iv = out;
		if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM) {
			memcpy(iv, tlsp->keyctx.u.txhdr.txsalt, SALT_SIZE);
			if (tlsp->tls13) {
				uint64_t value;

				value = be64dec(tlsp->keyctx.u.txhdr.txsalt +
				    4);
				value ^= m_tls->m_epg_seqno;
				be64enc(iv + 4, value);
			} else
				memcpy(iv + 4, hdr + 1, 8);
			if (send_partial_ghash)
				be32enc(iv + 12, 1 + offset / AES_BLOCK_LEN);
			else
				be32enc(iv + 12, 2 + offset / AES_BLOCK_LEN);
		} else
			memcpy(iv, hdr + 1, AES_BLOCK_LEN);
		out += AES_BLOCK_LEN;
	}

	if (imm_len % 16 != 0) {
		if (imm_len % 8 != 0) {
			/* Zero pad to an 8-byte boundary. */
			memset(out, 0, 8 - (imm_len % 8));
			out += 8 - (imm_len % 8);
		}

		/*
		 * Insert a ULP_TX_SC_NOOP if needed so the SGL is
		 * 16-byte aligned.
		 */
		if (imm_len % 16 <= 8) {
			idata = (void *)out;
			idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP) |
			    V_ULP_TX_SC_MORE(1));
			idata->len = htobe32(0);
			out = (void *)(idata + 1);
		}
	}

	/* SGL for record payload */
	sglist_reset(txq->gl);
	if (sglist_append_mbuf_epg(txq->gl, m_tls, m_tls->m_epg_hdrlen + offset,
	    plen) != 0) {
#ifdef INVARIANTS
		panic("%s: failed to append sglist", __func__);
#endif
	}
	if (last_ghash_frag) {
		if (sglist_append_phys(txq->gl, zero_buffer_pa,
		    AES_GMAC_HASH_LEN) != 0) {
#ifdef INVARIANTS
			panic("%s: failed to append sglist (2)", __func__);
#endif
		}
	}
	out = write_gl_to_buf(txq->gl, out);

	if (request_ghash) {
		/* ULP_TXPKT */
		txpkt = (void *)out;
		txpkt->cmd_dest = htobe32(V_ULPTX_CMD(ULP_TX_PKT) |
		    V_ULP_TXPKT_DATAMODIFY(0) |
		    V_T7_ULP_TXPKT_CHANNELID(tlsp->vi->pi->port_id) |
		    V_ULP_TXPKT_DEST(0) |
		    V_ULP_TXPKT_FID(txq->eq.cntxt_id) | V_ULP_TXPKT_RO(1));
		txpkt->len = htobe32(howmany(txpkt_lens[1], 16));

		/* ULPTX_IDATA sub-command */
		idata = (void *)(txpkt + 1);
		idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM) |
		    V_ULP_TX_SC_MORE(0));
		idata->len = sizeof(struct cpl_tx_tls_ack);
		idata->len += sizeof(struct rss_header) +
		    sizeof(struct cpl_fw6_pld);
		idata->len += AES_GMAC_HASH_LEN;
		idata->len = htobe32(idata->len);
		out = (void *)(idata + 1);

		/* CPL_TX_TLS_ACK */
		out = write_tx_tls_ack(out, tlsp->rx_chid, AES_GMAC_HASH_LEN,
		    ghash_lcb);

		/* CPL_FW6_PLD */
		out = write_fw6_pld(out, tlsp->rx_chid, tlsp->rx_qid,
		    AES_GMAC_HASH_LEN, (uintptr_t)tlsp | CPL_FW6_COOKIE_KTLS);

		/* Space for partial hash. */
		memset(out, 0, AES_GMAC_HASH_LEN);
		out += AES_GMAC_HASH_LEN;

		tlsp->ghash_pending = true;
		tlsp->ghash_valid = false;
		tlsp->ghash_lcb = ghash_lcb;
		if (last_ghash_frag)
			tlsp->ghash_offset = offset + plen;
		else
			tlsp->ghash_offset = rounddown2(offset + plen,
			    GMAC_BLOCK_LEN);
#ifdef VERBOSE_TRACES
		CTR(KTR_CXGBE, "%s: %p requesting GHASH for offset %u",
		    __func__, tlsp, tlsp->ghash_offset);
#endif
		m_snd_tag_ref(&tlsp->com);

		txq->kern_tls_ghash_requested++;
	}

	if (using_scratch) {
		out = dst;
		copy_to_txd(eq, txq->ss, &out, wr_len);
	}

	txq->kern_tls_records++;
	txq->kern_tls_octets += m_tls->m_len;
	if (split_mode) {
		txq->kern_tls_splitmode++;
		txq->kern_tls_waste += leading_waste + trailing_waste;
	}
	if (need_lso)
		txq->kern_tls_lso++;

	txsd = &txq->sdesc[pidx];
	if (last_wr)
		txsd->m = m;
	else
		txsd->m = NULL;
	txsd->desc_used = ndesc;

	return (ndesc);
}

int
t7_ktls_write_wr(struct sge_txq *txq, void *dst, struct mbuf *m,
    u_int available)
{
	struct sge_eq *eq = &txq->eq;
	struct tlspcb *tlsp;
	struct tcphdr *tcp;
	struct mbuf *m_tls;
	struct ether_header *eh;
	tcp_seq tcp_seqno;
	u_int ndesc, pidx, totdesc;
	uint16_t eh_type, mss;

	TXQ_LOCK_ASSERT_OWNED(txq);
	M_ASSERTPKTHDR(m);
	MPASS(m->m_pkthdr.snd_tag != NULL);
	tlsp = mst_to_tls(m->m_pkthdr.snd_tag);

	totdesc = 0;
	eh = mtod(m, struct ether_header *);
	eh_type = ntohs(eh->ether_type);
	if (eh_type == ETHERTYPE_VLAN) {
		struct ether_vlan_header *evh = (void *)eh;

		eh_type = ntohs(evh->evl_proto);
	}

	tcp = (struct tcphdr *)((char *)eh + m->m_pkthdr.l2hlen +
	    m->m_pkthdr.l3hlen);
	pidx = eq->pidx;

	/* Determine MSS. */
	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		mss = m->m_pkthdr.tso_segsz;
		tlsp->prev_mss = mss;
	} else if (tlsp->prev_mss != 0)
		mss = tlsp->prev_mss;
	else
		mss = if_getmtu(tlsp->vi->ifp) -
		    (m->m_pkthdr.l3hlen + m->m_pkthdr.l4hlen);

	/* Fetch the starting TCP sequence number for this chain. */
	tcp_seqno = ntohl(tcp->th_seq);
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: pkt len %d TCP seq %u", __func__, m->m_pkthdr.len,
	    tcp_seqno);
#endif
	KASSERT(!tlsp->ghash_pending, ("%s: GHASH pending for send", __func__));

	/*
	 * Iterate over each TLS record constructing a work request
	 * for that record.
	 */
	for (m_tls = m->m_next; m_tls != NULL; m_tls = m_tls->m_next) {
		MPASS(m_tls->m_flags & M_EXTPG);

		ndesc = ktls_write_tls_wr(tlsp, txq, dst, m, tcp, m_tls,
		    available - totdesc, tcp_seqno, pidx, eh_type, mss);
		totdesc += ndesc;
		IDXINCR(pidx, ndesc, eq->sidx);
		dst = &eq->desc[pidx];

		tcp_seqno += m_tls->m_len;
	}

	/*
	 * Queue another packet if this was a GCM request that didn't
	 * request a GHASH response.
	 */
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM && !tlsp->ghash_pending)
		ktls_queue_next_packet(tlsp, true);

	MPASS(totdesc <= available);
	return (totdesc);
}

static void
t7_tls_tag_free(struct m_snd_tag *mst)
{
	struct adapter *sc;
	struct tlspcb *tlsp;

	tlsp = mst_to_tls(mst);
	sc = tlsp->sc;

	CTR2(KTR_CXGBE, "%s: %p", __func__, tlsp);

	if (tlsp->tx_key_addr >= 0)
		t4_free_tls_keyid(sc, tlsp->tx_key_addr);

	KASSERT(mbufq_len(&tlsp->pending_mbufs) == 0,
	    ("%s: pending mbufs", __func__));

	zfree(tlsp, M_CXGBE);
}

static int
ktls_fw6_pld(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	const struct cpl_fw6_pld *cpl;
	struct tlspcb *tlsp;
	const void *ghash;

	if (m != NULL)
		cpl = mtod(m, const void *);
	else
		cpl = (const void *)(rss + 1);

	tlsp = (struct tlspcb *)(uintptr_t)CPL_FW6_PLD_COOKIE(cpl);
	KASSERT(cpl->data[0] == 0, ("%s: error status returned", __func__));

	TXQ_LOCK(tlsp->txq);
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: %p received GHASH for offset %u%s", __func__, tlsp,
	    tlsp->ghash_offset, tlsp->ghash_lcb ? " in LCB" : "");
#endif
	if (tlsp->ghash_lcb)
		ghash = &cpl->data[2];
	else
		ghash = cpl + 1;
	memcpy(tlsp->ghash, ghash, AES_GMAC_HASH_LEN);
	tlsp->ghash_valid = true;
	tlsp->ghash_pending = false;
	tlsp->txq->kern_tls_ghash_received++;

	ktls_queue_next_packet(tlsp, false);
	TXQ_UNLOCK(tlsp->txq);

	m_snd_tag_rele(&tlsp->com);
	m_freem(m);
	return (0);
}

void
t7_ktls_modload(void)
{
	zero_buffer = malloc_aligned(AES_GMAC_HASH_LEN, AES_GMAC_HASH_LEN,
	    M_CXGBE, M_ZERO | M_WAITOK);
	zero_buffer_pa = vtophys(zero_buffer);
	t4_register_shared_cpl_handler(CPL_FW6_PLD, ktls_fw6_pld,
	    CPL_FW6_COOKIE_KTLS);
}

void
t7_ktls_modunload(void)
{
	free(zero_buffer, M_CXGBE);
	t4_register_shared_cpl_handler(CPL_FW6_PLD, NULL, CPL_FW6_COOKIE_KTLS);
}

#else

int
t7_tls_tag_alloc(struct ifnet *ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **pt)
{
	return (ENXIO);
}

int
t7_ktls_parse_pkt(struct mbuf *m)
{
	return (EINVAL);
}

int
t7_ktls_write_wr(struct sge_txq *txq, void *dst, struct mbuf *m,
    u_int available)
{
	panic("can't happen");
}

void
t7_ktls_modload(void)
{
}

void
t7_ktls_modunload(void)
{
}

#endif
