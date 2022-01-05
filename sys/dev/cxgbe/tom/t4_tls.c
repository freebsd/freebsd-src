/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");

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
    uint64_t val)
{
	struct adapter *sc = td_adapter(toep->td);

	t4_set_tcb_field(sc, &toep->ofld_txq->wrq, toep, word, mask, val, 0, 0);
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

/* Set TLS Key-Id in TCB */
static void
t4_set_tls_keyid(struct toepcb *toep, unsigned int key_id)
{

	t4_set_tls_tcb_field(toep, W_TCB_RX_TLS_KEY_TAG,
			 V_TCB_RX_TLS_KEY_TAG(M_TCB_RX_TLS_BUF_TAG),
			 V_TCB_RX_TLS_KEY_TAG(key_id));
}

/* Clear TF_RX_QUIESCE to re-enable receive. */
static void
t4_clear_rx_quiesce(struct toepcb *toep)
{

	t4_set_tls_tcb_field(toep, W_TCB_T_FLAGS, V_TF_RX_QUIESCE(1), 0);
}

static void
tls_clr_ofld_mode(struct toepcb *toep)
{

	tls_stop_handshake_timer(toep);

	KASSERT(toep->tls.rx_key_addr == -1,
	    ("%s: tid %d has RX key", __func__, toep->tid));

	/* Switch to plain TOE mode. */
	t4_set_tls_tcb_field(toep, W_TCB_ULP_RAW,
	    V_TCB_ULP_RAW(V_TF_TLS_ENABLE(1)),
	    V_TCB_ULP_RAW(V_TF_TLS_ENABLE(0)));
	t4_set_tls_tcb_field(toep, W_TCB_ULP_TYPE,
	    V_TCB_ULP_TYPE(M_TCB_ULP_TYPE), V_TCB_ULP_TYPE(ULP_MODE_NONE));
	t4_clear_rx_quiesce(toep);

	toep->flags &= ~(TPF_FORCE_CREDITS | TPF_TLS_ESTABLISHED);
	toep->params.ulp_mode = ULP_MODE_NONE;
}

/* TLS/DTLS content type  for CPL SFO */
static inline unsigned char
tls_content_type(unsigned char content_type)
{
	/*
	 * XXX: Shouldn't this map CONTENT_TYPE_APP_DATA to DATA and
	 * default to "CUSTOM" for all other types including
	 * heartbeat?
	 */
	switch (content_type) {
	case CONTENT_TYPE_CCS:
		return CPL_TX_TLS_SFO_TYPE_CCS;
	case CONTENT_TYPE_ALERT:
		return CPL_TX_TLS_SFO_TYPE_ALERT;
	case CONTENT_TYPE_HANDSHAKE:
		return CPL_TX_TLS_SFO_TYPE_HANDSHAKE;
	case CONTENT_TYPE_HEARTBEAT:
		return CPL_TX_TLS_SFO_TYPE_HEARTBEAT;
	}
	return CPL_TX_TLS_SFO_TYPE_DATA;
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

/* Send request to get the key-id */
static int
tls_program_key_id(struct toepcb *toep, struct ktls_session *tls,
    int direction)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;
	struct adapter *sc = td_adapter(toep->td);
	struct ofld_tx_sdesc *txsd;
	int keyid;
	struct wrqe *wr;
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

	wr = alloc_wrqe(TLS_KEY_WR_SZ, &toep->ofld_txq->wrq);
	if (wr == NULL) {
		t4_free_tls_keyid(sc, keyid);
		return (ENOMEM);
	}
	kwr = wrtod(wr);
	memset(kwr, 0, TLS_KEY_WR_SZ);

	t4_write_tlskey_wr(tls, direction, toep->tid, F_FW_WR_COMPL, keyid,
	    kwr);
	kctx = (struct tls_keyctx *)(kwr + 1);
	if (direction == KTLS_TX)
		tls_ofld->tx_key_addr = keyid;
	else
		tls_ofld->rx_key_addr = keyid;
	t4_tls_key_ctx(tls, direction, kctx);

	txsd = &toep->txsd[toep->txsd_pidx];
	txsd->tx_credits = DIV_ROUND_UP(TLS_KEY_WR_SZ, 16);
	txsd->plen = 0;
	toep->tx_credits -= txsd->tx_credits;
	if (__predict_false(++toep->txsd_pidx == toep->txsd_total))
		toep->txsd_pidx = 0;
	toep->txsd_avail--;

	t4_wrq_tx(sc, wr);

	return (0);
}

/*
 * In some cases a client connection can hang without sending the
 * ServerHelloDone message from the NIC to the host.  Send a dummy
 * RX_DATA_ACK with RX_MODULATE to unstick the connection.
 */
static void
tls_send_handshake_ack(void *arg)
{
	struct toepcb *toep = arg;
	struct tls_ofld_info *tls_ofld = &toep->tls;
	struct adapter *sc = td_adapter(toep->td);

	/* Bail without rescheduling if the connection has closed. */
	if ((toep->flags & (TPF_FIN_SENT | TPF_ABORT_SHUTDOWN)) != 0)
		return;

	/*
	 * If this connection has timed out without receiving more
	 * data, downgrade to plain TOE mode and don't re-arm the
	 * timer.
	 */
	if (sc->tt.tls_rx_timeout != 0) {
		struct inpcb *inp;
		struct tcpcb *tp;

		inp = toep->inp;
		tp = intotcpcb(inp);
		if ((ticks - tp->t_rcvtime) >= sc->tt.tls_rx_timeout) {
			CTR2(KTR_CXGBE, "%s: tid %d clr_ofld_mode", __func__,
			    toep->tid);
			tls_clr_ofld_mode(toep);
			return;
		}
	}

	/*
	 * XXX: Does not have the t4_get_tcb() checks to refine the
	 * workaround.
	 */
	callout_schedule(&tls_ofld->handshake_timer, TLS_SRV_HELLO_RD_TM * hz);

	CTR2(KTR_CXGBE, "%s: tid %d sending RX_DATA_ACK", __func__, toep->tid);
	send_rx_modulate(sc, toep);
}

static void
tls_start_handshake_timer(struct toepcb *toep)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;

	INP_WLOCK_ASSERT(toep->inp);
	callout_reset(&tls_ofld->handshake_timer, TLS_SRV_HELLO_BKOFF_TM * hz,
	    tls_send_handshake_ack, toep);
}

void
tls_stop_handshake_timer(struct toepcb *toep)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;

	INP_WLOCK_ASSERT(toep->inp);
	callout_stop(&tls_ofld->handshake_timer);
}

int
tls_alloc_ktls(struct toepcb *toep, struct ktls_session *tls, int direction)
{
	struct adapter *sc = td_adapter(toep->td);
	int error, explicit_iv_size, key_offset, mac_first;

	if (!can_tls_offload(td_adapter(toep->td)))
		return (EINVAL);
	switch (ulp_mode(toep)) {
	case ULP_MODE_TLS:
		break;
	case ULP_MODE_NONE:
	case ULP_MODE_TCPDDP:
		if (direction != KTLS_TX)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		/* XXX: Explicitly ignore any provided IV. */
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 192 / 8:
		case 256 / 8:
			break;
		default:
			error = EINVAL;
			goto clr_ofld;
		}
		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
			break;
		default:
			error = EPROTONOSUPPORT;
			goto clr_ofld;
		}
		explicit_iv_size = AES_BLOCK_LEN;
		mac_first = 1;
		break;
	case CRYPTO_AES_NIST_GCM_16:
		if (tls->params.iv_len != SALT_SIZE) {
			error = EINVAL;
			goto clr_ofld;
		}
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 192 / 8:
		case 256 / 8:
			break;
		default:
			error = EINVAL;
			goto clr_ofld;
		}
		explicit_iv_size = 8;
		mac_first = 0;
		break;
	default:
		error = EPROTONOSUPPORT;
		goto clr_ofld;
	}

	/* Only TLS 1.1 and TLS 1.2 are currently supported. */
	if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
	    tls->params.tls_vminor < TLS_MINOR_VER_ONE ||
	    tls->params.tls_vminor > TLS_MINOR_VER_TWO) {
		error = EPROTONOSUPPORT;
		goto clr_ofld;
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
	if (error) {
		if (direction == KTLS_RX)
			goto clr_ofld;
		return (error);
	}

	if (direction == KTLS_TX) {
		toep->tls.scmd0.seqno_numivs =
			(V_SCMD_SEQ_NO_CTRL(3) |
			 V_SCMD_PROTO_VERSION(t4_tls_proto_ver(tls)) |
			 V_SCMD_ENC_DEC_CTRL(SCMD_ENCDECCTRL_ENCRYPT) |
			 V_SCMD_CIPH_AUTH_SEQ_CTRL((mac_first == 0)) |
			 V_SCMD_CIPH_MODE(t4_tls_cipher_mode(tls)) |
			 V_SCMD_AUTH_MODE(t4_tls_auth_mode(tls)) |
			 V_SCMD_HMAC_CTRL(t4_tls_hmac_ctrl(tls)) |
			 V_SCMD_IV_SIZE(explicit_iv_size / 2));

		toep->tls.scmd0.ivgen_hdrlen =
			(V_SCMD_IV_GEN_CTRL(1) |
			 V_SCMD_KEY_CTX_INLINE(0) |
			 V_SCMD_TLS_FRAG_ENABLE(1));

		if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
			toep->tls.iv_len = 8;
		else
			toep->tls.iv_len = AES_BLOCK_LEN;

		toep->tls.frag_size = tls->params.max_frame_len;
		toep->tls.fcplenmax = get_tp_plen_max(tls);
		toep->tls.expn_per_ulp = tls->params.tls_hlen +
		    tls->params.tls_tlen;
		toep->tls.pdus_per_ulp = 1;
		toep->tls.adjusted_plen = toep->tls.expn_per_ulp +
		    tls->params.max_frame_len;
		toep->tls.tx_key_info_size = t4_tls_key_info_size(tls);
	} else {
		/* Stop timer on handshake completion */
		tls_stop_handshake_timer(toep);

		toep->flags &= ~TPF_FORCE_CREDITS;
		toep->flags |= TPF_TLS_RECEIVE;
		toep->tls.rx_version = tls->params.tls_vmajor << 8 |
		    tls->params.tls_vminor;

		/*
		 * RX key tags are an index into the key portion of MA
		 * memory stored as an offset from the base address in
		 * units of 64 bytes.
		 */
		key_offset = toep->tls.rx_key_addr - sc->vres.key.start;
		t4_set_tls_keyid(toep, key_offset / 64);
		t4_set_tls_tcb_field(toep, W_TCB_ULP_RAW,
				 V_TCB_ULP_RAW(M_TCB_ULP_RAW),
				 V_TCB_ULP_RAW((V_TF_TLS_KEY_SIZE(3) |
						V_TF_TLS_CONTROL(1) |
						V_TF_TLS_ACTIVE(1) |
						V_TF_TLS_ENABLE(1))));
		t4_set_tls_tcb_field(toep, W_TCB_TLS_SEQ,
				 V_TCB_TLS_SEQ(M_TCB_TLS_SEQ),
				 V_TCB_TLS_SEQ(0));
		t4_clear_rx_quiesce(toep);
	}

	return (0);

clr_ofld:
	if (ulp_mode(toep) == ULP_MODE_TLS) {
		CTR2(KTR_CXGBE, "%s: tid %d clr_ofld_mode", __func__,
		    toep->tid);
		tls_clr_ofld_mode(toep);
	}
	return (error);
}

void
tls_init_toep(struct toepcb *toep)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;

	tls_ofld->rx_key_addr = -1;
	tls_ofld->tx_key_addr = -1;
}

void
tls_establish(struct toepcb *toep)
{

	/*
	 * Enable PDU extraction.
	 *
	 * XXX: Supposedly this should be done by the firmware when
	 * the ULP_MODE FLOWC parameter is set in send_flowc_wr(), but
	 * in practice this seems to be required.
	 */
	CTR2(KTR_CXGBE, "%s: tid %d setting TLS_ENABLE", __func__, toep->tid);
	t4_set_tls_tcb_field(toep, W_TCB_ULP_RAW, V_TCB_ULP_RAW(M_TCB_ULP_RAW),
	    V_TCB_ULP_RAW(V_TF_TLS_ENABLE(1)));

	toep->flags |= TPF_FORCE_CREDITS | TPF_TLS_ESTABLISHED;

	callout_init_rw(&toep->tls.handshake_timer, &toep->inp->inp_lock, 0);
	tls_start_handshake_timer(toep);
}

void
tls_detach(struct toepcb *toep)
{

	if (toep->flags & TPF_TLS_ESTABLISHED) {
		tls_stop_handshake_timer(toep);
		toep->flags &= ~TPF_TLS_ESTABLISHED;
	}
}

void
tls_uninit_toep(struct toepcb *toep)
{

	MPASS((toep->flags & TPF_TLS_ESTABLISHED) == 0);
	clear_tls_keyid(toep);
}

#define MAX_OFLD_TX_CREDITS (SGE_MAX_WR_LEN / 16)
#define	MIN_OFLD_TLSTX_CREDITS(toep)					\
	(howmany(sizeof(struct fw_tlstx_data_wr) + 			\
	    sizeof(struct cpl_tx_tls_sfo) + sizeof(struct ulptx_idata) + \
	    sizeof(struct ulptx_sc_memrd) +				\
	    AES_BLOCK_LEN + 1, 16))

static void
write_tlstx_wr(struct fw_tlstx_data_wr *txwr, struct toepcb *toep,
    unsigned int immdlen, unsigned int plen, unsigned int expn,
    unsigned int pdus, uint8_t credits, int shove, int imm_ivs)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;
	unsigned int len = plen + expn;

	txwr->op_to_immdlen = htobe32(V_WR_OP(FW_TLSTX_DATA_WR) |
	    V_FW_TLSTX_DATA_WR_COMPL(1) |
	    V_FW_TLSTX_DATA_WR_IMMDLEN(immdlen));
	txwr->flowid_len16 = htobe32(V_FW_TLSTX_DATA_WR_FLOWID(toep->tid) |
	    V_FW_TLSTX_DATA_WR_LEN16(credits));
	txwr->plen = htobe32(len);
	txwr->lsodisable_to_flags = htobe32(V_TX_ULP_MODE(ULP_MODE_TLS) |
	    V_TX_URG(0) | /* F_T6_TX_FORCE | */ V_TX_SHOVE(shove));
	txwr->ctxloc_to_exp = htobe32(V_FW_TLSTX_DATA_WR_NUMIVS(pdus) |
	    V_FW_TLSTX_DATA_WR_EXP(expn) |
	    V_FW_TLSTX_DATA_WR_CTXLOC(TLS_SFO_WR_CONTEXTLOC_DDR) |
	    V_FW_TLSTX_DATA_WR_IVDSGL(!imm_ivs) |
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
    struct tls_hdr *tls_hdr, unsigned int plen, unsigned int pdus)
{
	struct tls_ofld_info *tls_ofld = &toep->tls;
	int data_type, seglen;

	if (plen < tls_ofld->frag_size)
		seglen = plen;
	else
		seglen = tls_ofld->frag_size;
	data_type = tls_content_type(tls_hdr->type);
	cpl->op_to_seg_len = htobe32(V_CPL_TX_TLS_SFO_OPCODE(CPL_TX_TLS_SFO) |
	    V_CPL_TX_TLS_SFO_DATA_TYPE(data_type) |
	    V_CPL_TX_TLS_SFO_CPL_LEN(2) | V_CPL_TX_TLS_SFO_SEG_LEN(seglen));
	cpl->pld_len = htobe32(plen);
	if (data_type == CPL_TX_TLS_SFO_TYPE_HEARTBEAT)
		cpl->type_protover = htobe32(
		    V_CPL_TX_TLS_SFO_TYPE(tls_hdr->type));
	cpl->seqno_numivs = htobe32(tls_ofld->scmd0.seqno_numivs |
	    V_SCMD_NUM_IVS(pdus));
	cpl->ivgen_hdrlen = htobe32(tls_ofld->scmd0.ivgen_hdrlen);
	cpl->scmd1 = htobe64(tls_ofld->tx_seq_no);
	tls_ofld->tx_seq_no += pdus;
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
	int tls_size, tx_credits, shove, sowwakeup;
	struct ofld_tx_sdesc *txsd;
	char *buf;

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

	txsd = &toep->txsd[toep->txsd_pidx];
	for (;;) {
		tx_credits = min(toep->tx_credits, MAX_OFLD_TX_CREDITS);

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
		if (m == NULL || (m->m_flags & M_NOTAVAIL) != 0) {
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

		/* Explicit IVs for AES-CBC and AES-GCM are <= 16. */
		MPASS(toep->tls.iv_len <= AES_BLOCK_LEN);
		wr_len += AES_BLOCK_LEN;

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
		    (m->m_next->m_flags & M_NOTAVAIL) != 0)) &&
		    (tp->t_flags & TF_MORETOCOME) == 0;

		if (sb->sb_flags & SB_AUTOSIZE &&
		    V_tcp_do_autosndbuf &&
		    sb->sb_hiwat < V_tcp_autosndbuf_max &&
		    sbused(sb) >= sb->sb_hiwat * 7 / 8) {
			int newsize = min(sb->sb_hiwat + V_tcp_autosndbuf_inc,
			    V_tcp_autosndbuf_max);

			if (!sbreserve_locked(sb, newsize, so, NULL))
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
		write_tlstx_wr(txwr, toep, 0,
		    tls_size, expn_size, 1, credits, shove, 1);
		toep->tls.tx_seq_no = m->m_epg_seqno;
		write_tlstx_cpl(cpl, toep, thdr, tls_size, 1);

		idata = (struct ulptx_idata *)(cpl + 1);
		idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
		idata->len = htobe32(0);
		memrd = (struct ulptx_sc_memrd *)(idata + 1);
		memrd->cmd_to_len = htobe32(V_ULPTX_CMD(ULP_TX_SC_MEMRD) |
		    V_ULP_TX_SC_MORE(1) |
		    V_ULPTX_LEN16(toep->tls.tx_key_info_size >> 4));
		memrd->addr = htobe32(toep->tls.tx_key_addr >> 5);

		/* Copy IV. */
		buf = (char *)(memrd + 1);
		memcpy(buf, thdr + 1, toep->tls.iv_len);
		buf += AES_BLOCK_LEN;

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
	if (inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) {
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
	struct mbuf *control;
	int pdu_length, rx_credits;
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
	if (inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT)) {
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
	 * additional fields.
	 */
	KASSERT(m->m_len >= sizeof(*tls_hdr_pkt),
	    ("%s: payload too small", __func__));
	tls_hdr_pkt = mtod(m, void *);

	tls_data = mbufq_dequeue(&toep->ulp_pdu_reclaimq);
	if (tls_data != NULL) {
		KASSERT(be32toh(cpl->seq) == tls_data->m_pkthdr.tls_tcp_seq,
		    ("%s: sequence mismatch", __func__));
	}

	/* Report decryption errors as EBADMSG. */
	if ((tls_hdr_pkt->res_to_mac_error & M_TLSRX_HDR_PKT_ERROR) != 0) {
		m_freem(m);
		m_freem(tls_data);

		CURVNET_SET(toep->vnet);
		so->so_error = EBADMSG;
		sorwakeup(so);

		INP_WUNLOCK(inp);
		CURVNET_RESTORE();

		return (0);
	}

	/* Allocate the control message mbuf. */
	control = sbcreatecontrol(NULL, sizeof(*tgr), TLS_GET_RECORD,
	    IPPROTO_TCP);
	if (control == NULL) {
		m_freem(m);
		m_freem(tls_data);

		CURVNET_SET(toep->vnet);
		so->so_error = ENOBUFS;
		sorwakeup(so);

		INP_WUNLOCK(inp);
		CURVNET_RESTORE();

		return (0);
	}

	tgr = (struct tls_get_record *)
	    CMSG_DATA(mtod(control, struct cmsghdr *));
	tgr->tls_type = tls_hdr_pkt->type;
	tgr->tls_vmajor = be16toh(tls_hdr_pkt->version) >> 8;
	tgr->tls_vminor = be16toh(tls_hdr_pkt->version) & 0xff;

	m_freem(m);

	if (tls_data != NULL) {
		m_last(tls_data)->m_flags |= M_EOR;
		tgr->tls_length = htobe16(tls_data->m_pkthdr.len);
	} else
		tgr->tls_length = 0;
	m = tls_data;

	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);

	if (__predict_false(sb->sb_state & SBS_CANTRCVMORE)) {
		struct epoch_tracker et;

		CTR3(KTR_CXGBE, "%s: tid %u, excess rx (%d bytes)",
		    __func__, tid, pdu_length);
		m_freem(m);
		m_freem(control);
		SOCKBUF_UNLOCK(sb);
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

	/*
	 * Not all of the bytes on the wire are included in the socket buffer
	 * (e.g. the MAC of the TLS record).  However, those bytes are included
	 * in the TCP sequence space.
	 */

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

		if (!sbreserve_locked(sb, newsize, so, NULL))
			sb->sb_flags &= ~SB_AUTOSIZE;
	}

	sbappendcontrol_locked(sb, m, control, 0);
	rx_credits = sbspace(sb) > tp->rcv_wnd ? sbspace(sb) - tp->rcv_wnd : 0;
#ifdef VERBOSE_TRACES
	CTR4(KTR_CXGBE, "%s: tid %u rx_credits %u rcv_wnd %u",
	    __func__, tid, rx_credits, tp->rcv_wnd);
#endif
	if (rx_credits > 0 && sbused(sb) + tp->rcv_wnd < sb->sb_lowat) {
		rx_credits = send_rx_credits(sc, toep, rx_credits);
		tp->rcv_wnd += rx_credits;
		tp->rcv_adv += rx_credits;
	}

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
	int len, rx_credits;

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
	/*
	 * This connection is going to die anyway, so probably don't
	 * need to bother with returning credits.
	 */
	rx_credits = sbspace(sb) > tp->rcv_wnd ? sbspace(sb) - tp->rcv_wnd : 0;
#ifdef VERBOSE_TRACES
	CTR4(KTR_CXGBE, "%s: tid %u rx_credits %u rcv_wnd %u",
	    __func__, toep->tid, rx_credits, tp->rcv_wnd);
#endif
	if (rx_credits > 0 && sbused(sb) + tp->rcv_wnd < sb->sb_lowat) {
		rx_credits = send_rx_credits(toep->vi->adapter, toep,
		    rx_credits);
		tp->rcv_wnd += rx_credits;
		tp->rcv_adv += rx_credits;
	}

	sorwakeup_locked(so);
	SOCKBUF_UNLOCK_ASSERT(sb);

	INP_WUNLOCK(inp);
	CURVNET_RESTORE();

	m_freem(m);
}

void
t4_tls_mod_load(void)
{

	t4_register_cpl_handler(CPL_TLS_DATA, do_tls_data);
	t4_register_cpl_handler(CPL_RX_TLS_CMP, do_rx_tls_cmp);
}

void
t4_tls_mod_unload(void)
{

	t4_register_cpl_handler(CPL_TLS_DATA, NULL);
	t4_register_cpl_handler(CPL_RX_TLS_CMP, NULL);
}
#endif	/* TCP_OFFLOAD */
#endif	/* KERN_TLS */
