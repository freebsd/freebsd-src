/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017-2018 Chelsio Communications, Inc.
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

#include "opt_inet.h"
#include "opt_kern_tls.h"

#include <sys/cdefs.h>
#ifdef KERN_TLS
#include <sys/param.h>
#include <sys/ktr.h>
#include <sys/ktls.h>
#include <sys/sglist.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#ifdef TCP_OFFLOAD
#include "common/common.h"
#include "common/t4_tcb.h"
#include "crypto/t4_crypto.h"
#include "tom/t4_tom_l2t.h"
#include "tom/t4_tom.h"

/*
 * The TCP sequence number of a CPL_TLS_DATA mbuf is saved here while
 * the mbuf is in the ulp_pdu_reclaimq.
 */
#define	tls_tcp_seq	PH_loc.thirtytwo[0]

static void
t4_set_tls_tcb_field(struct toepcb *toep, uint16_t word, uint64_t mask,
    uint64_t val, int reply, int cookie)
{
	struct adapter *sc = td_adapter(toep->td);
	struct mbuf *m;

	m = alloc_raw_wr_mbuf(sizeof(struct cpl_set_tcb_field));
	if (m == NULL) {
		/* XXX */
		panic("%s: out of memory", __func__);
	}

	write_set_tcb_field(sc, mtod(m, void *), toep, word, mask, val, reply,
	    cookie);

	t4_raw_wr_tx(sc, toep, m);
}

/* TLS and DTLS common routines */
bool
can_tls_offload(struct adapter *sc)
{

	return (sc->tt.tls && sc->cryptocaps & FW_CAPS_CONFIG_TLSKEYS);
}

int
tls_tx_key(struct toepcb *toep)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;

	return (tls_ofld->tx_key_addr >= 0);
}

/* Set TF_RX_QUIESCE to pause receive. */
static void
t4_set_rx_quiesce(struct toepcb *toep)
{

	t4_set_tls_tcb_field(toep, W_TCB_T_FLAGS, V_TF_RX_QUIESCE(1),
	    V_TF_RX_QUIESCE(1), 1, CPL_COOKIE_TOM);
}

/* Clear TF_RX_QUIESCE to re-enable receive. */
static void
t4_clear_rx_quiesce(struct toepcb *toep)
{

	t4_set_tls_tcb_field(toep, W_TCB_T_FLAGS, V_TF_RX_QUIESCE(1), 0, 0, 0);
}

/* TLS/DTLS content type  for CPL SFO */
static inline unsigned char
tls_content_type(unsigned char content_type)
{
	switch (content_type) {
	case CONTENT_TYPE_CCS:
		return CPL_TX_TLS_SFO_TYPE_CCS;
	case CONTENT_TYPE_ALERT:
		return CPL_TX_TLS_SFO_TYPE_ALERT;
	case CONTENT_TYPE_HANDSHAKE:
		return CPL_TX_TLS_SFO_TYPE_HANDSHAKE;
	case CONTENT_TYPE_APP_DATA:
		return CPL_TX_TLS_SFO_TYPE_DATA;
	default:
		return CPL_TX_TLS_SFO_TYPE_CUSTOM;
	}
}

/* TLS Key memory management */
static void
clear_tls_keyid(struct toepcb *toep)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;
	struct adapter *sc = td_adapter(toep->td);

	if (tls_ofld->rx_key_addr >= 0) {
		t4_free_tls_keyid(sc, tls_ofld->rx_key_addr);
		tls_ofld->rx_key_addr = -1;
	}
	if (tls_ofld->tx_key_addr >= 0) {
		t4_free_tls_keyid(sc, tls_ofld->tx_key_addr);
		tls_ofld->tx_key_addr = -1;
	}
}

static int
get_tp_plen_max(struct ktls_session *tls)
{
	int plen = ((min(3*4096, TP_TX_PG_SZ))/1448) * 1448;

	return (tls->params.max_frame_len <= 8192 ? plen : FC_TP_PLEN_MAX);
}

/* Send request to save the key in on-card memory. */
static int
tls_program_key_id(struct toepcb *toep, struct ktls_session *tls,
    int direction)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;
	struct adapter *sc = td_adapter(toep->td);
	int keyid;
	struct mbuf *m;
	struct tls_key_req *kwr;
	struct tls_keyctx *kctx;

#ifdef INVARIANTS
	int kwrlen, kctxlen, len;

	kwrlen = sizeof(*kwr);
	kctxlen = roundup2(sizeof(*kctx), 32);
	len = roundup2(kwrlen + kctxlen, 16);
	MPASS(TLS_KEY_WR_SZ == len);
#endif
	if (toep->txsd_avail == 0)
		return (EAGAIN);

	if ((keyid = t4_alloc_tls_keyid(sc)) < 0) {
		return (ENOSPC);
	}

	m = alloc_raw_wr_mbuf(TLS_KEY_WR_SZ);
	if (m == NULL) {
		t4_free_tls_keyid(sc, keyid);
		return (ENOMEM);
	}
	kwr = mtod(m, struct tls_key_req *);
	memset(kwr, 0, TLS_KEY_WR_SZ);

	t4_write_tlskey_wr(tls, direction, toep->tid, F_FW_WR_COMPL, keyid,
	    kwr);
	kctx = (struct tls_keyctx *)(kwr + 1);
	if (direction == KTLS_TX)
		tls_ofld->tx_key_addr = keyid;
	else
		tls_ofld->rx_key_addr = keyid;
	t4_tls_key_ctx(tls, direction, kctx);

	t4_raw_wr_tx(sc, toep, m);

	return (0);
}

int
tls_alloc_ktls(struct toepcb *toep, struct ktls_session *tls, int direction)
{
	struct adapter *sc = td_adapter(toep->td);
	int error, iv_size, mac_first;

	if (!can_tls_offload(sc))
		return (EINVAL);

	if (direction == KTLS_RX) {
		if (ulp_mode(toep) != ULP_MODE_NONE)
			return (EINVAL);
		if ((toep->flags & TPF_TLS_STARTING) != 0)
			return (EINVAL);
	} else {
		switch (ulp_mode(toep)) {
		case ULP_MODE_NONE:
		case ULP_MODE_TLS:
		case ULP_MODE_TCPDDP:
			break;
		default:
			return (EINVAL);
		}
	}

	/* TLS 1.1 through TLS 1.3 are currently supported. */
	if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
	    tls->params.tls_vminor < TLS_MINOR_VER_ONE ||
	    tls->params.tls_vminor > TLS_MINOR_VER_THREE) {
		return (EPROTONOSUPPORT);
	}

	/* TLS 1.3 is only supported on T7+. */
	if (tls->params.tls_vminor == TLS_MINOR_VER_THREE) {
		if (is_t6(sc)) {
			return (EPROTONOSUPPORT);
		}
	}

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

	/* Bail if we already have a key. */
	if (direction == KTLS_TX) {
		if (toep->tls.tx_key_addr != -1)
			return (EOPNOTSUPP);
	} else {
		if (toep->tls.rx_key_addr != -1)
			return (EOPNOTSUPP);
	}

	error = tls_program_key_id(toep, tls, direction);
	if (error)
		return (error);

	toep->tls.tls13 = tls->params.tls_vminor == TLS_MINOR_VER_THREE;
	if (direction == KTLS_TX) {
		toep->tls.scmd0.seqno_numivs =
			(V_SCMD_SEQ_NO_CTRL(3) |
			 V_SCMD_PROTO_VERSION(t4_tls_proto_ver(tls)) |
			 V_SCMD_ENC_DEC_CTRL(SCMD_ENCDECCTRL_ENCRYPT) |
			 V_SCMD_CIPH_AUTH_SEQ_CTRL((mac_first == 0)) |
			 V_SCMD_CIPH_MODE(t4_tls_cipher_mode(tls)) |
			 V_SCMD_AUTH_MODE(t4_tls_auth_mode(tls)) |
			 V_SCMD_HMAC_CTRL(t4_tls_hmac_ctrl(tls)) |
			 V_SCMD_IV_SIZE(iv_size / 2));

		toep->tls.scmd0.ivgen_hdrlen =
			(V_SCMD_IV_GEN_CTRL(1) |
			 V_SCMD_KEY_CTX_INLINE(0) |
			 V_SCMD_TLS_FRAG_ENABLE(1));

		toep->tls.iv_len = iv_size;
		toep->tls.frag_size = tls->params.max_frame_len;
		toep->tls.fcplenmax = get_tp_plen_max(tls);
		toep->tls.expn_per_ulp = tls->params.tls_hlen +
		    tls->params.tls_tlen;
		toep->tls.pdus_per_ulp = 1;
		toep->tls.adjusted_plen = toep->tls.expn_per_ulp +
		    tls->params.max_frame_len;
		toep->tls.tx_key_info_size = t4_tls_key_info_size(tls);
	} else {
		toep->flags |= TPF_TLS_STARTING | TPF_TLS_RX_QUIESCING;
		toep->tls.rx_version = tls->params.tls_vmajor << 8 |
		    tls->params.tls_vminor;

		CTR2(KTR_CXGBE, "%s: tid %d setting RX_QUIESCE", __func__,
		    toep->tid);
		t4_set_rx_quiesce(toep);
	}

	return (0);
}

void
tls_init_toep(struct toepcb *toep)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;

	tls_ofld->rx_key_addr = -1;
	tls_ofld->tx_key_addr = -1;
}

void
tls_uninit_toep(struct toepcb *toep)
{

	clear_tls_keyid(toep);
}

#define MAX_OFLD_TX_CREDITS (SGE_MAX_WR_LEN / 16)
#define	MIN_OFLD_TLSTX_CREDITS(toep)					\
	(howmany(sizeof(struct fw_tlstx_data_wr) +			\
	    sizeof(struct cpl_tx_tls_sfo) + sizeof(struct ulptx_idata) + \
	    sizeof(struct ulptx_sc_memrd) +				\
	    AES_BLOCK_LEN + 1, 16))

static void
write_tlstx_wr(struct fw_tlstx_data_wr *txwr, struct toepcb *toep,
    unsigned int plen, unsigned int expn, uint8_t credits, int shove,
    int num_ivs)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;
	unsigned int len = plen + expn;

	txwr->op_to_immdlen = htobe32(V_WR_OP(FW_TLSTX_DATA_WR) |
	    V_FW_TLSTX_DATA_WR_COMPL(1) |
	    V_FW_TLSTX_DATA_WR_IMMDLEN(0));
	txwr->flowid_len16 = htobe32(V_FW_TLSTX_DATA_WR_FLOWID(toep->tid) |
	    V_FW_TLSTX_DATA_WR_LEN16(credits));
	txwr->plen = htobe32(len);
	txwr->lsodisable_to_flags = htobe32(V_TX_ULP_MODE(ULP_MODE_TLS) |
	    V_TX_URG(0) | /* F_T6_TX_FORCE | */ V_TX_SHOVE(shove));
	txwr->ctxloc_to_exp = htobe32(V_FW_TLSTX_DATA_WR_NUMIVS(num_ivs) |
	    V_FW_TLSTX_DATA_WR_EXP(expn) |
	    V_FW_TLSTX_DATA_WR_CTXLOC(TLS_SFO_WR_CONTEXTLOC_DDR) |
	    V_FW_TLSTX_DATA_WR_IVDSGL(0) |
	    V_FW_TLSTX_DATA_WR_KEYSIZE(tls_ofld->tx_key_info_size >> 4));
	txwr->mfs = htobe16(tls_ofld->frag_size);
	txwr->adjustedplen_pkd = htobe16(
	    V_FW_TLSTX_DATA_WR_ADJUSTEDPLEN(tls_ofld->adjusted_plen));
	txwr->expinplenmax_pkd = htobe16(
	    V_FW_TLSTX_DATA_WR_EXPINPLENMAX(tls_ofld->expn_per_ulp));
	txwr->pdusinplenmax_pkd =
	    V_FW_TLSTX_DATA_WR_PDUSINPLENMAX(tls_ofld->pdus_per_ulp);
}

static void
write_tlstx_cpl(struct cpl_tx_tls_sfo *cpl, struct toepcb *toep,
    struct tls_hdr *tls_hdr, unsigned int plen, uint8_t rec_type,
    uint64_t seqno)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;
	int data_type, seglen;

	seglen = plen;
	data_type = tls_content_type(rec_type);
	cpl->op_to_seg_len = htobe32(V_CPL_TX_TLS_SFO_OPCODE(CPL_TX_TLS_SFO) |
	    V_CPL_TX_TLS_SFO_DATA_TYPE(data_type) |
	    V_CPL_TX_TLS_SFO_CPL_LEN(2) | V_CPL_TX_TLS_SFO_SEG_LEN(seglen));
	cpl->pld_len = htobe32(plen);
	if (data_type == CPL_TX_TLS_SFO_TYPE_CUSTOM)
		cpl->type_protover = htobe32(V_CPL_TX_TLS_SFO_TYPE(rec_type));
	cpl->seqno_numivs = htobe32(tls_ofld->scmd0.seqno_numivs |
	    V_SCMD_NUM_IVS(1));
	cpl->ivgen_hdrlen = htobe32(tls_ofld->scmd0.ivgen_hdrlen);
	cpl->scmd1 = htobe64(seqno);
}

static int
count_ext_pgs_segs(struct mbuf *m)
{
	vm_paddr_t nextpa;
	u_int i, nsegs;

	MPASS(m->m_epg_npgs > 0);
	nsegs = 1;
	nextpa = m->m_epg_pa[0] + PAGE_SIZE;
	for (i = 1; i < m->m_epg_npgs; i++) {
		if (nextpa != m->m_epg_pa[i])
			nsegs++;
		nextpa = m->m_epg_pa[i] + PAGE_SIZE;
	}
	return (nsegs);
}

static void
write_ktlstx_sgl(void *dst, struct mbuf *m, int nsegs)
{
	struct ulptx_sgl *usgl = dst;
	vm_paddr_t pa;
	uint32_t len;
	int i, j;

	KASSERT(nsegs > 0, ("%s: nsegs 0", __func__));

	usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
	    V_ULPTX_NSGE(nsegs));

	/* Figure out the first S/G length. */
	pa = m->m_epg_pa[0] + m->m_epg_1st_off;
	usgl->addr0 = htobe64(pa);
	len = m_epg_pagelen(m, 0, m->m_epg_1st_off);
	pa += len;
	for (i = 1; i < m->m_epg_npgs; i++) {
		if (m->m_epg_pa[i] != pa)
			break;
		len += m_epg_pagelen(m, i, 0);
		pa += m_epg_pagelen(m, i, 0);
	}
	usgl->len0 = htobe32(len);
#ifdef INVARIANTS
	nsegs--;
#endif

	j = -1;
	for (; i < m->m_epg_npgs; i++) {
		if (j == -1 || m->m_epg_pa[i] != pa) {
			if (j >= 0)
				usgl->sge[j / 2].len[j & 1] = htobe32(len);
			j++;
#ifdef INVARIANTS
			nsegs--;
#endif
			pa = m->m_epg_pa[i];
			usgl->sge[j / 2].addr[j & 1] = htobe64(pa);
			len = m_epg_pagelen(m, i, 0);
			pa += len;
		} else {
			len += m_epg_pagelen(m, i, 0);
			pa += m_epg_pagelen(m, i, 0);
		}
	}
	if (j >= 0) {
		usgl->sge[j / 2].len[j & 1] = htobe32(len);

		if ((j & 1) == 0)
			usgl->sge[j / 2].len[1] = htobe32(0);
	}
	KASSERT(nsegs == 0, ("%s: nsegs %d, m %p", __func__, nsegs, m));
}

/*
 * Similar to t4_push_frames() but handles sockets that contain TLS
 * record mbufs.
 */
void
t4_push_ktls(struct adapter *sc, struct toepcb *toep, int drop)
{
	struct tls_hdr *thdr;
	struct fw_tlstx_data_wr *txwr;
	struct cpl_tx_tls_sfo *cpl;
	struct ulptx_idata *idata;
	struct ulptx_sc_memrd *memrd;
	struct wrqe *wr;
	struct mbuf *m;
	u_int nsegs, credits, wr_len;
	u_int expn_size;
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp = intotcpcb(inp);
	struct socket *so = inp->inp_socket;
	struct sockbuf *sb = &so->so_snd;
	struct mbufq *pduq = &toep->ulp_pduq;
	int tls_size, tx_credits, shove, sowwakeup;
	struct ofld_tx_sdesc *txsd;
	char *buf;
	bool tls13;

	INP_WLOCK_ASSERT(inp);
	KASSERT(toep->flags & TPF_FLOWC_WR_SENT,
	    ("%s: flowc_wr not sent for tid %u.", __func__, toep->tid));

	KASSERT(ulp_mode(toep) == ULP_MODE_NONE ||
	    ulp_mode(toep) == ULP_MODE_TCPDDP || ulp_mode(toep) == ULP_MODE_TLS,
	    ("%s: ulp_mode %u for toep %p", __func__, ulp_mode(toep), toep));
	KASSERT(tls_tx_key(toep),
	    ("%s: TX key not set for toep %p", __func__, toep));

#ifdef VERBOSE_TRACES
	CTR4(KTR_CXGBE, "%s: tid %d toep flags %#x tp flags %#x drop %d",
	    __func__, toep->tid, toep->flags, tp->t_flags);
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

	tls13 = toep->tls.tls13;
	txsd = &toep->txsd[toep->txsd_pidx];
	for (;;) {
		tx_credits = min(toep->tx_credits, MAX_OFLD_TX_CREDITS);

		if (__predict_false((m = mbufq_first(pduq)) != NULL)) {
			if (!t4_push_raw_wr(sc, toep, m)) {
				toep->flags |= TPF_TX_SUSPENDED;
				return;
			}

			(void)mbufq_dequeue(pduq);

			txsd = &toep->txsd[toep->txsd_pidx];
			continue;
		}

		SOCKBUF_LOCK(sb);
		sowwakeup = drop;
		if (drop) {
			sbdrop_locked(sb, drop);
			drop = 0;
		}

		m = sb->sb_sndptr != NULL ? sb->sb_sndptr->m_next : sb->sb_mb;

		/*
		 * Send a FIN if requested, but only if there's no
		 * more data to send.
		 */
		if (m == NULL && toep->flags & TPF_SEND_FIN) {
			if (sowwakeup)
				sowwakeup_locked(so);
			else
				SOCKBUF_UNLOCK(sb);
			SOCKBUF_UNLOCK_ASSERT(sb);
			t4_close_conn(sc, toep);
			return;
		}

		/*
		 * If there is no ready data to send, wait until more
		 * data arrives.
		 */
		if (m == NULL || (m->m_flags & M_NOTREADY) != 0) {
			if (sowwakeup)
				sowwakeup_locked(so);
			else
				SOCKBUF_UNLOCK(sb);
			SOCKBUF_UNLOCK_ASSERT(sb);
#ifdef VERBOSE_TRACES
			CTR2(KTR_CXGBE, "%s: tid %d no ready data to send",
			    __func__, toep->tid);
#endif
			return;
		}

		KASSERT(m->m_flags & M_EXTPG, ("%s: mbuf %p is not NOMAP",
		    __func__, m));
		KASSERT(m->m_epg_tls != NULL,
		    ("%s: mbuf %p doesn't have TLS session", __func__, m));

		/* Calculate WR length. */
		wr_len = sizeof(struct fw_tlstx_data_wr) +
		    sizeof(struct cpl_tx_tls_sfo) +
		    sizeof(struct ulptx_idata) + sizeof(struct ulptx_sc_memrd);

		if (!tls13) {
			/* Explicit IVs for AES-CBC and AES-GCM are <= 16. */
			MPASS(toep->tls.iv_len <= AES_BLOCK_LEN);
			wr_len += AES_BLOCK_LEN;
		}

		/* Account for SGL in work request length. */
		nsegs = count_ext_pgs_segs(m);
		wr_len += sizeof(struct ulptx_sgl) +
		    ((3 * (nsegs - 1)) / 2 + ((nsegs - 1) & 1)) * 8;

		/* Not enough credits for this work request. */
		if (howmany(wr_len, 16) > tx_credits) {
			if (sowwakeup)
				sowwakeup_locked(so);
			else
				SOCKBUF_UNLOCK(sb);
			SOCKBUF_UNLOCK_ASSERT(sb);
#ifdef VERBOSE_TRACES
			CTR5(KTR_CXGBE,
	    "%s: tid %d mbuf %p requires %d credits, but only %d available",
			    __func__, toep->tid, m, howmany(wr_len, 16),
			    tx_credits);
#endif
			toep->flags |= TPF_TX_SUSPENDED;
			return;
		}

		/* Shove if there is no additional data pending. */
		shove = ((m->m_next == NULL ||
		    (m->m_next->m_flags & M_NOTREADY) != 0)) &&
		    (tp->t_flags & TF_MORETOCOME) == 0;

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
		if (sowwakeup)
			sowwakeup_locked(so);
		else
			SOCKBUF_UNLOCK(sb);
		SOCKBUF_UNLOCK_ASSERT(sb);

		if (__predict_false(toep->flags & TPF_FIN_SENT))
			panic("%s: excess tx.", __func__);

		wr = alloc_wrqe(roundup2(wr_len, 16), &toep->ofld_txq->wrq);
		if (wr == NULL) {
			/* XXX: how will we recover from this? */
			toep->flags |= TPF_TX_SUSPENDED;
			return;
		}

		thdr = (struct tls_hdr *)&m->m_epg_hdr;
#ifdef VERBOSE_TRACES
		CTR5(KTR_CXGBE, "%s: tid %d TLS record %ju type %d len %#x",
		    __func__, toep->tid, m->m_epg_seqno, thdr->type,
		    m->m_len);
#endif
		txwr = wrtod(wr);
		cpl = (struct cpl_tx_tls_sfo *)(txwr + 1);
		memset(txwr, 0, roundup2(wr_len, 16));
		credits = howmany(wr_len, 16);
		expn_size = m->m_epg_hdrlen +
		    m->m_epg_trllen;
		tls_size = m->m_len - expn_size;
		write_tlstx_wr(txwr, toep, tls_size, expn_size, credits, shove,
		    tls13 ? 0 : 1);
		write_tlstx_cpl(cpl, toep, thdr, tls_size,
		    tls13 ? m->m_epg_record_type : thdr->type, m->m_epg_seqno);

		idata = (struct ulptx_idata *)(cpl + 1);
		idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
		idata->len = htobe32(0);
		memrd = (struct ulptx_sc_memrd *)(idata + 1);
		memrd->cmd_to_len = htobe32(V_ULPTX_CMD(ULP_TX_SC_MEMRD) |
		    V_ULP_TX_SC_MORE(1) |
		    V_ULPTX_LEN16(toep->tls.tx_key_info_size >> 4));
		memrd->addr = htobe32(toep->tls.tx_key_addr >> 5);

		buf = (char *)(memrd + 1);
		if (!tls13) {
			/* Copy IV. */
			memcpy(buf, thdr + 1, toep->tls.iv_len);
			buf += AES_BLOCK_LEN;
		}

		write_ktlstx_sgl(buf, m, nsegs);

		KASSERT(toep->tx_credits >= credits,
			("%s: not enough credits", __func__));

		toep->tx_credits -= credits;

		tp->snd_nxt += m->m_len;
		tp->snd_max += m->m_len;

		SOCKBUF_LOCK(sb);
		sb->sb_sndptr = m;
		SOCKBUF_UNLOCK(sb);

		toep->flags |= TPF_TX_DATA_SENT;
		if (toep->tx_credits < MIN_OFLD_TLSTX_CREDITS(toep))
			toep->flags |= TPF_TX_SUSPENDED;

		KASSERT(toep->txsd_avail > 0, ("%s: no txsd", __func__));
		KASSERT(m->m_len <= MAX_OFLD_TX_SDESC_PLEN,
		    ("%s: plen %u too large", __func__, m->m_len));
		txsd->plen = m->m_len;
		txsd->tx_credits = credits;
		txsd++;
		if (__predict_false(++toep->txsd_pidx == toep->txsd_total)) {
			toep->txsd_pidx = 0;
			txsd = &toep->txsd[0];
		}
		toep->txsd_avail--;

		counter_u64_add(toep->ofld_txq->tx_toe_tls_records, 1);
		counter_u64_add(toep->ofld_txq->tx_toe_tls_octets, m->m_len);

		t4_l2t_send(sc, wr, toep->l2te);
	}
}

/*
 * For TLS data we place received mbufs received via CPL_TLS_DATA into
 * an mbufq in the TLS offload state.  When CPL_RX_TLS_CMP is
 * received, the completed PDUs are placed into the socket receive
 * buffer.
 *
 * The TLS code reuses the ulp_pdu_reclaimq to hold the pending mbufs.
 */
static int
do_tls_data(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_tls_data *cpl = mtod(m, const void *);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp;
	int len;

	/* XXX: Should this match do_rx_data instead? */
	KASSERT(!(toep->flags & TPF_SYNQE),
	    ("%s: toep %p claims to be a synq entry", __func__, toep));

	KASSERT(toep->tid == tid, ("%s: toep tid/atid mismatch", __func__));

	/* strip off CPL header */
	m_adj(m, sizeof(*cpl));
	len = m->m_pkthdr.len;

	toep->ofld_rxq->rx_toe_tls_octets += len;

	KASSERT(len == G_CPL_TLS_DATA_LENGTH(be32toh(cpl->length_pkd)),
	    ("%s: payload length mismatch", __func__));

	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		CTR4(KTR_CXGBE, "%s: tid %u, rx (%d bytes), inp_flags 0x%x",
		    __func__, tid, len, inp->inp_flags);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}

	/* Save TCP sequence number. */
	m->m_pkthdr.tls_tcp_seq = be32toh(cpl->seq);

	if (mbufq_enqueue(&toep->ulp_pdu_reclaimq, m)) {
#ifdef INVARIANTS
		panic("Failed to queue TLS data packet");
#else
		printf("%s: Failed to queue TLS data packet\n", __func__);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
#endif
	}

	tp = intotcpcb(inp);
	tp->t_rcvtime = ticks;

#ifdef VERBOSE_TRACES
	CTR4(KTR_CXGBE, "%s: tid %u len %d seq %u", __func__, tid, len,
	    be32toh(cpl->seq));
#endif

	INP_WUNLOCK(inp);
	return (0);
}

static int
do_rx_tls_cmp(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_rx_tls_cmp *cpl = mtod(m, const void *);
	struct tlsrx_hdr_pkt *tls_hdr_pkt;
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct tcpcb *tp;
	struct socket *so;
	struct sockbuf *sb;
	struct mbuf *tls_data;
	struct tls_get_record *tgr;
	struct mbuf *control, *n;
	int pdu_length, resid, trailer_len;
#if defined(KTR) || defined(INVARIANTS)
	int len;
#endif

	KASSERT(toep->tid == tid, ("%s: toep tid/atid mismatch", __func__));
	KASSERT(!(toep->flags & TPF_SYNQE),
	    ("%s: toep %p claims to be a synq entry", __func__, toep));

	/* strip off CPL header */
	m_adj(m, sizeof(*cpl));
#if defined(KTR) || defined(INVARIANTS)
	len = m->m_pkthdr.len;
#endif

	toep->ofld_rxq->rx_toe_tls_records++;

	KASSERT(len == G_CPL_RX_TLS_CMP_LENGTH(be32toh(cpl->pdulength_length)),
	    ("%s: payload length mismatch", __func__));

	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		CTR4(KTR_CXGBE, "%s: tid %u, rx (%d bytes), inp_flags 0x%x",
		    __func__, tid, len, inp->inp_flags);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}

	pdu_length = G_CPL_RX_TLS_CMP_PDULENGTH(be32toh(cpl->pdulength_length));

	so = inp_inpcbtosocket(inp);
	tp = intotcpcb(inp);

#ifdef VERBOSE_TRACES
	CTR6(KTR_CXGBE, "%s: tid %u PDU len %d len %d seq %u, rcv_nxt %u",
	    __func__, tid, pdu_length, len, be32toh(cpl->seq), tp->rcv_nxt);
#endif

	tp->rcv_nxt += pdu_length;
	KASSERT(tp->rcv_wnd >= pdu_length,
	    ("%s: negative window size", __func__));
	tp->rcv_wnd -= pdu_length;

	/* XXX: Not sure what to do about urgent data. */

	/*
	 * The payload of this CPL is the TLS header followed by
	 * additional fields.  For TLS 1.3 the type field holds the
	 * inner record type and the length field has been updated to
	 * strip the inner record type, padding, and MAC.
	 */
	KASSERT(m->m_len >= sizeof(*tls_hdr_pkt),
	    ("%s: payload too small", __func__));
	tls_hdr_pkt = mtod(m, void *);

	tls_data = mbufq_dequeue(&toep->ulp_pdu_reclaimq);
	if (tls_data != NULL) {
		KASSERT(be32toh(cpl->seq) == tls_data->m_pkthdr.tls_tcp_seq,
		    ("%s: sequence mismatch", __func__));
	}

	/*
	 * Report decryption errors as EBADMSG.
	 *
	 * XXX: To support rekeying for TLS 1.3 this will eventually
	 * have to be updated to recrypt the data with the old key and
	 * then decrypt with the new key.  Punt for now as KTLS
	 * doesn't yet support rekeying.
	 */
	if ((tls_hdr_pkt->res_to_mac_error & M_TLSRX_HDR_PKT_ERROR) != 0) {
		CTR4(KTR_CXGBE, "%s: tid %u TLS error %#x ddp_vld %#x",
		    __func__, toep->tid, tls_hdr_pkt->res_to_mac_error,
		    be32toh(cpl->ddp_valid));
		m_freem(m);
		m_freem(tls_data);

		CURVNET_SET(toep->vnet);
		so->so_error = EBADMSG;
		sorwakeup(so);

		INP_WUNLOCK(inp);
		CURVNET_RESTORE();

		return (0);
	}

	/* For TLS 1.3 trim the header and trailer. */
	if (toep->tls.tls13) {
		KASSERT(tls_data != NULL, ("%s: TLS 1.3 record without data",
		    __func__));
		MPASS(tls_data->m_pkthdr.len == pdu_length);
		m_adj(tls_data, sizeof(struct tls_record_layer));
		if (tls_data->m_pkthdr.len > be16toh(tls_hdr_pkt->length))
			tls_data->m_pkthdr.len = be16toh(tls_hdr_pkt->length);
		resid = tls_data->m_pkthdr.len;
		if (resid == 0) {
			m_freem(tls_data);
			tls_data = NULL;
		} else {
			for (n = tls_data;; n = n->m_next) {
				if (n->m_len < resid) {
					resid -= n->m_len;
					continue;
				}

				n->m_len = resid;
				m_freem(n->m_next);
				n->m_next = NULL;
				break;
			}
		}
	}

	/* Handle data received after the socket is closed. */
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	if (__predict_false(sb->sb_state & SBS_CANTRCVMORE)) {
		struct epoch_tracker et;

		CTR3(KTR_CXGBE, "%s: tid %u, excess rx (%d bytes)",
		    __func__, tid, pdu_length);
		m_freem(m);
		m_freem(tls_data);
		SOCKBUF_UNLOCK(sb);
		INP_WUNLOCK(inp);

		CURVNET_SET(toep->vnet);
		NET_EPOCH_ENTER(et);
		INP_WLOCK(inp);
		tp = tcp_drop(tp, ECONNRESET);
		if (tp != NULL)
			INP_WUNLOCK(inp);
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();

		return (0);
	}

	/*
	 * If there is any data in the 'sb_mtls' chain of the socket
	 * or we aren't able to allocate the control mbuf, append the
	 * record as a CSUM_TLS_DECRYPTED packet to 'sb_mtls' rather
	 * than as a decrypted record to 'sb_m'.
	 */
	if (sb->sb_mtls != NULL)
		control = NULL;
	else
		control = sbcreatecontrol(NULL, sizeof(*tgr), TLS_GET_RECORD,
		    IPPROTO_TCP, M_NOWAIT);

	if (control != NULL) {
		tgr = (struct tls_get_record *)
		    CMSG_DATA(mtod(control, struct cmsghdr *));
		memset(tgr, 0, sizeof(*tgr));
		tgr->tls_type = tls_hdr_pkt->type;
		tgr->tls_vmajor = be16toh(tls_hdr_pkt->version) >> 8;
		tgr->tls_vminor = be16toh(tls_hdr_pkt->version) & 0xff;
		if (tls_data != NULL) {
			m_last(tls_data)->m_flags |= M_EOR;
			tgr->tls_length = htobe16(tls_data->m_pkthdr.len);
		} else
			tgr->tls_length = 0;

		m_freem(m);
		m = tls_data;
	} else {
		M_ASSERTPKTHDR(m);

		/* It's ok that any explicit IV is missing. */
		m->m_len = sb->sb_tls_info->params.tls_hlen;
		m->m_pkthdr.csum_flags |= CSUM_TLS_DECRYPTED;
		m->m_pkthdr.len = m->m_len;
		if (tls_data != NULL) {
			m->m_pkthdr.len += tls_data->m_pkthdr.len;
			m_demote_pkthdr(tls_data);
			m->m_next = tls_data;
		}

		/*
		 * Grow the chain by the trailer, but without
		 * contents.  The trailer will be thrown away by
		 * ktls_decrypt.  Note that ktls_decrypt assumes the
		 * trailer is tls_tlen bytes long, so append that many
		 * bytes not the actual trailer size computed from
		 * pdu_length.
		 */
		trailer_len = sb->sb_tls_info->params.tls_tlen;
		if (tls_data != NULL) {
			m_last(tls_data)->m_len += trailer_len;
			tls_data = NULL;
		} else
			m->m_len += trailer_len;
		m->m_pkthdr.len += trailer_len;
		tls_hdr_pkt->length = htobe16(m->m_pkthdr.len -
		    sizeof(struct tls_record_layer));
	}

	/* receive buffer autosize */
	MPASS(toep->vnet == so->so_vnet);
	CURVNET_SET(toep->vnet);
	if (sb->sb_flags & SB_AUTOSIZE &&
	    V_tcp_do_autorcvbuf &&
	    sb->sb_hiwat < V_tcp_autorcvbuf_max &&
	    m->m_pkthdr.len > (sbspace(sb) / 8 * 7)) {
		unsigned int hiwat = sb->sb_hiwat;
		unsigned int newsize = min(hiwat + sc->tt.autorcvbuf_inc,
		    V_tcp_autorcvbuf_max);

		if (!sbreserve_locked(so, SO_RCV, newsize, NULL))
			sb->sb_flags &= ~SB_AUTOSIZE;
	}

	if (control != NULL)
		sbappendcontrol_locked(sb, m, control, 0);
	else
		sbappendstream_locked(sb, m, 0);
	t4_rcvd_locked(&toep->td->tod, tp);

	sorwakeup_locked(so);
	SOCKBUF_UNLOCK_ASSERT(sb);

	INP_WUNLOCK(inp);
	CURVNET_RESTORE();
	return (0);
}

void
do_rx_data_tls(const struct cpl_rx_data *cpl, struct toepcb *toep,
    struct mbuf *m)
{
	struct inpcb *inp = toep->inp;
	struct tls_ofld_info *tls_ofld = &toep->tls;
	struct tls_hdr *hdr;
	struct tcpcb *tp;
	struct socket *so;
	struct sockbuf *sb;
	int len;

	len = m->m_pkthdr.len;

	INP_WLOCK_ASSERT(inp);

	so = inp_inpcbtosocket(inp);
	tp = intotcpcb(inp);
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	CURVNET_SET(toep->vnet);

	tp->rcv_nxt += len;
	KASSERT(tp->rcv_wnd >= len, ("%s: negative window size", __func__));
	tp->rcv_wnd -= len;

	/* Do we have a full TLS header? */
	if (len < sizeof(*hdr)) {
		CTR3(KTR_CXGBE, "%s: tid %u len %d: too short for a TLS header",
		    __func__, toep->tid, len);
		so->so_error = EMSGSIZE;
		goto out;
	}
	hdr = mtod(m, struct tls_hdr *);

	/* Is the header valid? */
	if (be16toh(hdr->version) != tls_ofld->rx_version) {
		CTR3(KTR_CXGBE, "%s: tid %u invalid version %04x",
		    __func__, toep->tid, be16toh(hdr->version));
		so->so_error = EINVAL;
		goto out;
	}
	if (be16toh(hdr->length) < sizeof(*hdr)) {
		CTR3(KTR_CXGBE, "%s: tid %u invalid length %u",
		    __func__, toep->tid, be16toh(hdr->length));
		so->so_error = EBADMSG;
		goto out;
	}

	/* Did we get a truncated record? */
	if (len < be16toh(hdr->length)) {
		CTR4(KTR_CXGBE, "%s: tid %u truncated TLS record (%d vs %u)",
		    __func__, toep->tid, len, be16toh(hdr->length));

		so->so_error = EMSGSIZE;
		goto out;
	}

	/* Is the header type unknown? */
	switch (hdr->type) {
	case CONTENT_TYPE_CCS:
	case CONTENT_TYPE_ALERT:
	case CONTENT_TYPE_APP_DATA:
	case CONTENT_TYPE_HANDSHAKE:
		break;
	default:
		CTR3(KTR_CXGBE, "%s: tid %u invalid TLS record type %u",
		    __func__, toep->tid, hdr->type);
		so->so_error = EBADMSG;
		goto out;
	}

	/*
	 * Just punt.  Although this could fall back to software
	 * decryption, this case should never really happen.
	 */
	CTR4(KTR_CXGBE, "%s: tid %u dropping TLS record type %u, length %u",
	    __func__, toep->tid, hdr->type, be16toh(hdr->length));
	so->so_error = EBADMSG;

out:
	sorwakeup_locked(so);
	SOCKBUF_UNLOCK_ASSERT(sb);

	INP_WUNLOCK(inp);
	CURVNET_RESTORE();

	m_freem(m);
}

/*
 * Send a work request setting one or more TCB fields to partially or
 * fully enable ULP_MODE_TLS.
 *
 * - If resid == 0, the socket buffer ends at a record boundary
 *   (either empty or contains one or more complete records).  Switch
 *   to ULP_MODE_TLS (if not already) and enable TLS decryption.
 *
 * - If resid != 0, the socket buffer contains a partial record.  In
 *   this case, switch to ULP_MODE_TLS partially and configure the TCB
 *   to pass along the remaining resid bytes undecrypted.  Once they
 *   arrive, this is called again with resid == 0 and enables TLS
 *   decryption.
 */
static void
tls_update_tcb(struct adapter *sc, struct toepcb *toep, uint64_t seqno,
    size_t resid)
{
	struct mbuf *m;
	struct work_request_hdr *wrh;
	struct ulp_txpkt *ulpmc;
	int fields, key_offset, len;

	/*
	 * If we are already in ULP_MODE_TLS, then we should now be at
	 * a record boundary and ready to finish enabling TLS RX.
	 */
	KASSERT(resid == 0 || ulp_mode(toep) == ULP_MODE_NONE,
	    ("%s: tid %d needs %zu more data but already ULP_MODE_TLS",
	    __func__, toep->tid, resid));

	fields = 0;
	if (ulp_mode(toep) == ULP_MODE_NONE) {
		/* 2 writes for the overlay region */
		fields += 2;
	}

	if (resid == 0) {
		/* W_TCB_TLS_SEQ */
		fields++;

		/* W_TCB_ULP_RAW */
		fields++;
	} else {
		/* W_TCB_PDU_LEN */
		fields++;

		/* W_TCB_ULP_RAW */
		fields++;
	}

	if (ulp_mode(toep) == ULP_MODE_NONE) {
		/* W_TCB_ULP_TYPE */
		fields ++;
	}

	/* W_TCB_T_FLAGS */
	fields++;

	len = sizeof(*wrh) + fields * roundup2(LEN__SET_TCB_FIELD_ULP, 16);
	KASSERT(len <= SGE_MAX_WR_LEN,
	    ("%s: WR with %d TCB field updates too large", __func__, fields));

	m = alloc_raw_wr_mbuf(len);
	if (m == NULL) {
		/* XXX */
		panic("%s: out of memory", __func__);
	}

	wrh = mtod(m, struct work_request_hdr *);
	INIT_ULPTX_WRH(wrh, len, 1, toep->tid);	/* atomic */
	ulpmc = (struct ulp_txpkt *)(wrh + 1);

	if (ulp_mode(toep) == ULP_MODE_NONE) {
		/*
		 * Clear the TLS overlay region: 1023:832.
		 *
		 * Words 26/27 are always set to zero.  Words 28/29
		 * contain seqno and are set when enabling TLS
		 * decryption.  Word 30 is zero and Word 31 contains
		 * the keyid.
		 */
		ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid, 26,
		    0xffffffffffffffff, 0);

		/*
		 * RX key tags are an index into the key portion of MA
		 * memory stored as an offset from the base address in
		 * units of 64 bytes.
		 */
		key_offset = toep->tls.rx_key_addr - sc->vres.key.start;
		ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid, 30,
		    0xffffffffffffffff,
		    (uint64_t)V_TCB_RX_TLS_KEY_TAG(key_offset / 64) << 32);
	}

	if (resid == 0) {
		/*
		 * The socket buffer is empty or only contains
		 * complete TLS records: Set the sequence number and
		 * enable TLS decryption.
		 */
		CTR3(KTR_CXGBE, "%s: tid %d enable TLS seqno %lu", __func__,
		    toep->tid, seqno);
		ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid,
		    W_TCB_RX_TLS_SEQ, V_TCB_RX_TLS_SEQ(M_TCB_RX_TLS_SEQ),
		    V_TCB_RX_TLS_SEQ(seqno));
		ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid,
		    W_TCB_ULP_RAW, V_TCB_ULP_RAW(M_TCB_ULP_RAW),
		    V_TCB_ULP_RAW((V_TF_TLS_KEY_SIZE(3) | V_TF_TLS_CONTROL(1) |
		    V_TF_TLS_ACTIVE(1) | V_TF_TLS_ENABLE(1))));

		toep->flags &= ~TPF_TLS_STARTING;
		toep->flags |= TPF_TLS_RECEIVE;
	} else {
		/*
		 * The socket buffer ends with a partial record with a
		 * full header and needs at least 6 bytes.
		 *
		 * Set PDU length.  This is treating the 'resid' bytes
		 * as a TLS PDU, so the first 5 bytes are a fake
		 * header and the rest are the PDU length.
		 */
		ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid,
		    W_TCB_PDU_LEN, V_TCB_PDU_LEN(M_TCB_PDU_LEN),
		    V_TCB_PDU_LEN(resid - sizeof(struct tls_hdr)));
		CTR3(KTR_CXGBE, "%s: tid %d setting PDU_LEN to %zu",
		    __func__, toep->tid, resid - sizeof(struct tls_hdr));

		/* Clear all bits in ULP_RAW except for ENABLE. */
		ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid,
		    W_TCB_ULP_RAW, V_TCB_ULP_RAW(M_TCB_ULP_RAW),
		    V_TCB_ULP_RAW(V_TF_TLS_ENABLE(1)));

		/* Wait for 'resid' bytes to be delivered as CPL_RX_DATA. */
		toep->tls.rx_resid = resid;
	}

	if (ulp_mode(toep) == ULP_MODE_NONE) {
		/* Set the ULP mode to ULP_MODE_TLS. */
		toep->params.ulp_mode = ULP_MODE_TLS;
		ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid,
		    W_TCB_ULP_TYPE, V_TCB_ULP_TYPE(M_TCB_ULP_TYPE),
		    V_TCB_ULP_TYPE(ULP_MODE_TLS));
	}

	/* Clear TF_RX_QUIESCE. */
	ulpmc = mk_set_tcb_field_ulp(sc, ulpmc, toep->tid, W_TCB_T_FLAGS,
	    V_TF_RX_QUIESCE(1), 0);

	t4_raw_wr_tx(sc, toep, m);
}

/*
 * Examine the pending data in the socket buffer and either enable TLS
 * RX or request more encrypted data.
 */
static void
tls_check_rx_sockbuf(struct adapter *sc, struct toepcb *toep,
    struct sockbuf *sb)
{
	uint64_t seqno;
	size_t resid;
	bool have_header;

	SOCKBUF_LOCK_ASSERT(sb);
	MPASS(toep->tls.rx_resid == 0);

	have_header = ktls_pending_rx_info(sb, &seqno, &resid);
	CTR5(KTR_CXGBE, "%s: tid %d have_header %d seqno %lu resid %zu",
	    __func__, toep->tid, have_header, seqno, resid);

	/*
	 * If we have a partial header or we need fewer bytes than the
	 * size of a TLS record, re-enable receive and pause again once
	 * we get more data to try again.
	 */
	if (!have_header || (resid != 0 && (resid < sizeof(struct tls_hdr) ||
	    is_t6(sc)))) {
		CTR(KTR_CXGBE, "%s: tid %d waiting for more data", __func__,
		    toep->tid);
		toep->flags &= ~TPF_TLS_RX_QUIESCED;
		t4_clear_rx_quiesce(toep);
		return;
	}

	tls_update_tcb(sc, toep, seqno, resid);
}

void
tls_received_starting_data(struct adapter *sc, struct toepcb *toep,
    struct sockbuf *sb, int len)
{
	MPASS(toep->flags & TPF_TLS_STARTING);

	/* Data was received before quiescing took effect. */
	if ((toep->flags & TPF_TLS_RX_QUIESCING) != 0)
		return;

	/*
	 * A previous call to tls_check_rx_sockbuf needed more data.
	 * Now that more data has arrived, quiesce receive again and
	 * check the state once the quiesce has completed.
	 */
	if ((toep->flags & TPF_TLS_RX_QUIESCED) == 0) {
		CTR(KTR_CXGBE, "%s: tid %d quiescing", __func__, toep->tid);
		toep->flags |= TPF_TLS_RX_QUIESCING;
		t4_set_rx_quiesce(toep);
		return;
	}

	KASSERT(len <= toep->tls.rx_resid,
	    ("%s: received excess bytes %d (waiting for %zu)", __func__, len,
	    toep->tls.rx_resid));
	toep->tls.rx_resid -= len;
	if (toep->tls.rx_resid != 0)
		return;

	tls_check_rx_sockbuf(sc, toep, sb);
}

static int
do_tls_tcb_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_set_tcb_rpl *cpl = (const void *)(rss + 1);
	unsigned int tid = GET_TID(cpl);
	struct toepcb *toep;
	struct inpcb *inp;
	struct socket *so;
	struct sockbuf *sb;

	if (cpl->status != CPL_ERR_NONE)
		panic("XXX: tcp_rpl failed: %d", cpl->status);

	toep = lookup_tid(sc, tid);
	inp = toep->inp;
	switch (cpl->cookie) {
	case V_WORD(W_TCB_T_FLAGS) | V_COOKIE(CPL_COOKIE_TOM):
		INP_WLOCK(inp);
		if ((toep->flags & TPF_TLS_STARTING) == 0)
			panic("%s: connection is not starting TLS RX\n",
			    __func__);
		MPASS((toep->flags & TPF_TLS_RX_QUIESCING) != 0);

		toep->flags &= ~TPF_TLS_RX_QUIESCING;
		toep->flags |= TPF_TLS_RX_QUIESCED;

		so = inp->inp_socket;
		sb = &so->so_rcv;
		SOCKBUF_LOCK(sb);
		tls_check_rx_sockbuf(sc, toep, sb);
		SOCKBUF_UNLOCK(sb);
		INP_WUNLOCK(inp);
		break;
	default:
		panic("XXX: unknown tcb_rpl offset %#x, cookie %#x",
		    G_WORD(cpl->cookie), G_COOKIE(cpl->cookie));
	}

	return (0);
}

void
t4_tls_mod_load(void)
{

	t4_register_cpl_handler(CPL_TLS_DATA, do_tls_data);
	t4_register_cpl_handler(CPL_RX_TLS_CMP, do_rx_tls_cmp);
	t4_register_shared_cpl_handler(CPL_SET_TCB_RPL, do_tls_tcb_rpl,
	    CPL_COOKIE_TOM);
}

void
t4_tls_mod_unload(void)
{

	t4_register_cpl_handler(CPL_TLS_DATA, NULL);
	t4_register_cpl_handler(CPL_RX_TLS_CMP, NULL);
	t4_register_shared_cpl_handler(CPL_SET_TCB_RPL, NULL, CPL_COOKIE_TOM);
}
#endif	/* TCP_OFFLOAD */
#endif	/* KERN_TLS */
