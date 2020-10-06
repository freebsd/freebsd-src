/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018-2019 Chelsio Communications, Inc.
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

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "common/t4_tcb.h"
#include "t4_l2t.h"
#include "t4_clip.h"
#include "t4_mp_ring.h"
#include "crypto/t4_crypto.h"

#if defined(INET) || defined(INET6)

#define SALT_SIZE		4

#define GCM_TAG_SIZE			16
#define TLS_HEADER_LENGTH		5

#define	TLS_KEY_CONTEXT_SZ	roundup2(sizeof(struct tls_keyctx), 32)

struct tls_scmd {
	__be32 seqno_numivs;
	__be32 ivgen_hdrlen;
};

struct tls_key_req {
	/* FW_ULPTX_WR */
	__be32 wr_hi;
	__be32 wr_mid;
        __be32 ftid;
        __u8   reneg_to_write_rx;
        __u8   protocol;
        __be16 mfs;
	/* master command */
	__be32 cmd;
	__be32 len16;             /* command length */
	__be32 dlen;              /* data length in 32-byte units */
	__be32 kaddr;
	/* sub-command */
	__be32 sc_more;
	__be32 sc_len;
}__packed;

struct tls_keyctx {
	struct tx_keyctx_hdr {
		__u8   ctxlen;
		__u8   r2;
		__be16 dualck_to_txvalid;
		__u8   txsalt[4];
		__be64 r5;
	} txhdr;
        struct keys {
                __u8   edkey[32];
                __u8   ipad[64];
                __u8   opad[64];
        } keys;
};

#define S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT 11
#define M_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT)
#define G_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT)
#define F_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT \
    V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_SALT_PRESENT 10
#define M_TLS_KEYCTX_TX_WR_SALT_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_SALT_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_SALT_PRESENT)
#define G_TLS_KEYCTX_TX_WR_SALT_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_SALT_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_SALT_PRESENT)
#define F_TLS_KEYCTX_TX_WR_SALT_PRESENT \
    V_TLS_KEYCTX_TX_WR_SALT_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_TXCK_SIZE 6
#define M_TLS_KEYCTX_TX_WR_TXCK_SIZE 0xf
#define V_TLS_KEYCTX_TX_WR_TXCK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXCK_SIZE)
#define G_TLS_KEYCTX_TX_WR_TXCK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXCK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_TXCK_SIZE)

#define S_TLS_KEYCTX_TX_WR_TXMK_SIZE 2
#define M_TLS_KEYCTX_TX_WR_TXMK_SIZE 0xf
#define V_TLS_KEYCTX_TX_WR_TXMK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXMK_SIZE)
#define G_TLS_KEYCTX_TX_WR_TXMK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXMK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_TXMK_SIZE)

#define S_TLS_KEYCTX_TX_WR_TXVALID   0
#define M_TLS_KEYCTX_TX_WR_TXVALID   0x1
#define V_TLS_KEYCTX_TX_WR_TXVALID(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXVALID)
#define G_TLS_KEYCTX_TX_WR_TXVALID(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXVALID) & M_TLS_KEYCTX_TX_WR_TXVALID)
#define F_TLS_KEYCTX_TX_WR_TXVALID   V_TLS_KEYCTX_TX_WR_TXVALID(1U)

/* Key Context Programming Operation type */
#define KEY_WRITE_RX			0x1
#define KEY_WRITE_TX			0x2
#define KEY_DELETE_RX			0x4
#define KEY_DELETE_TX			0x8

struct tlspcb {
	struct m_snd_tag com;
	struct vi_info *vi;	/* virtual interface */
	struct adapter *sc;
	struct l2t_entry *l2te;	/* L2 table entry used by this connection */
	int tid;		/* Connection identifier */

	int tx_key_addr;
	bool inline_key;
	bool using_timestamps;
	unsigned char enc_mode;

	struct tls_scmd scmd0;
	struct tls_scmd scmd0_short;

	unsigned int tx_key_info_size;

	uint32_t prev_seq;
	uint32_t prev_ack;
	uint32_t prev_tsecr;
	uint16_t prev_win;
	uint16_t prev_mss;

	/* Only used outside of setup and teardown when using inline keys. */
	struct tls_keyctx keyctx;

	/* Fields only used during setup and teardown. */
	struct inpcb *inp;	/* backpointer to host stack's PCB */
	struct sge_txq *txq;
	struct sge_wrq *ctrlq;
	struct clip_entry *ce;	/* CLIP table entry used by this tid */

	unsigned char auth_mode;
	unsigned char hmac_ctrl;
	unsigned char mac_first;
	unsigned char iv_size;

	unsigned int frag_size;
	unsigned int cipher_secret_size;
	int proto_ver;

	bool open_pending;
};

static int ktls_setup_keys(struct tlspcb *tlsp,
    const struct ktls_session *tls, struct sge_txq *txq);

static inline struct tlspcb *
mst_to_tls(struct m_snd_tag *t)
{
	return (__containerof(t, struct tlspcb, com));
}

/* XXX: There are similar versions of these two in tom/t4_tls.c. */
static int
get_new_keyid(struct tlspcb *tlsp)
{
	vmem_addr_t addr;

	if (vmem_alloc(tlsp->sc->key_map, TLS_KEY_CONTEXT_SZ,
	    M_NOWAIT | M_FIRSTFIT, &addr) != 0)
		return (-1);

	return (addr);
}

static void
free_keyid(struct tlspcb *tlsp, int keyid)
{

	CTR3(KTR_CXGBE, "%s: tid %d key addr %#x", __func__, tlsp->tid, keyid);
	vmem_free(tlsp->sc->key_map, keyid, TLS_KEY_CONTEXT_SZ);
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

	m_snd_tag_init(&tlsp->com, ifp, IF_SND_TAG_TYPE_TLS);
	tlsp->vi = vi;
	tlsp->sc = sc;
	tlsp->ctrlq = &sc->sge.ctrlq[pi->port_id];
	tlsp->tid = -1;
	tlsp->tx_key_addr = -1;

	return (tlsp);
}

static void
init_ktls_key_params(struct tlspcb *tlsp, const struct ktls_session *tls)
{
	int mac_key_size;

	if (tls->params.tls_vminor == TLS_MINOR_VER_ONE)
		tlsp->proto_ver = SCMD_PROTO_VERSION_TLS_1_1;
	else
		tlsp->proto_ver = SCMD_PROTO_VERSION_TLS_1_2;
	tlsp->cipher_secret_size = tls->params.cipher_key_len;
	tlsp->tx_key_info_size = sizeof(struct tx_keyctx_hdr) +
	    tlsp->cipher_secret_size;
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16) {
		tlsp->auth_mode = SCMD_AUTH_MODE_GHASH;
		tlsp->enc_mode = SCMD_CIPH_MODE_AES_GCM;
		tlsp->iv_size = 4;
		tlsp->mac_first = 0;
		tlsp->hmac_ctrl = SCMD_HMAC_CTRL_NOP;
		tlsp->tx_key_info_size += GMAC_BLOCK_LEN;
	} else {
		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			mac_key_size = roundup2(SHA1_HASH_LEN, 16);
			tlsp->auth_mode = SCMD_AUTH_MODE_SHA1;
			break;
		case CRYPTO_SHA2_256_HMAC:
			mac_key_size = SHA2_256_HASH_LEN;
			tlsp->auth_mode = SCMD_AUTH_MODE_SHA256;
			break;
		case CRYPTO_SHA2_384_HMAC:
			mac_key_size = SHA2_512_HASH_LEN;
			tlsp->auth_mode = SCMD_AUTH_MODE_SHA512_384;
			break;
		}
		tlsp->enc_mode = SCMD_CIPH_MODE_AES_CBC;
		tlsp->iv_size = 8; /* for CBC, iv is 16B, unit of 2B */
		tlsp->mac_first = 1;
		tlsp->hmac_ctrl = SCMD_HMAC_CTRL_NO_TRUNC;
		tlsp->tx_key_info_size += mac_key_size * 2;
	}

	tlsp->frag_size = tls->params.max_frame_len;
}

static int
ktls_act_open_cpl_size(bool isipv6)
{

	if (isipv6)
		return (sizeof(struct cpl_t6_act_open_req6));
	else
		return (sizeof(struct cpl_t6_act_open_req));
}

static void
mk_ktls_act_open_req(struct adapter *sc, struct vi_info *vi, struct inpcb *inp,
    struct tlspcb *tlsp, int atid, void *dst)
{
	struct tcpcb *tp = intotcpcb(inp);
	struct cpl_t6_act_open_req *cpl6;
	struct cpl_act_open_req *cpl;
	uint64_t options;
	int qid_atid;

	cpl6 = dst;
	cpl = (struct cpl_act_open_req *)cpl6;
	INIT_TP_WR(cpl6, 0);
	qid_atid = V_TID_QID(sc->sge.fwq.abs_id) | V_TID_TID(atid) |
	    V_TID_COOKIE(CPL_COOKIE_KERN_TLS);
	OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ,
		qid_atid));
	inp_4tuple_get(inp, &cpl->local_ip, &cpl->local_port,
	    &cpl->peer_ip, &cpl->peer_port);

	options = F_TCAM_BYPASS | V_ULP_MODE(ULP_MODE_NONE);
	options |= V_SMAC_SEL(vi->smt_idx) | V_TX_CHAN(vi->pi->tx_chan);
	options |= F_NON_OFFLOAD;
	cpl->opt0 = htobe64(options);

	options = V_TX_QUEUE(sc->params.tp.tx_modq[vi->pi->tx_chan]);
	if (tp->t_flags & TF_REQ_TSTMP)
		options |= F_TSTAMPS_EN;
	cpl->opt2 = htobe32(options);
}

static void
mk_ktls_act_open_req6(struct adapter *sc, struct vi_info *vi,
    struct inpcb *inp, struct tlspcb *tlsp, int atid, void *dst)
{
	struct tcpcb *tp = intotcpcb(inp);
	struct cpl_t6_act_open_req6 *cpl6;
	struct cpl_act_open_req6 *cpl;
	uint64_t options;
	int qid_atid;

	cpl6 = dst;
	cpl = (struct cpl_act_open_req6 *)cpl6;
	INIT_TP_WR(cpl6, 0);
	qid_atid = V_TID_QID(sc->sge.fwq.abs_id) | V_TID_TID(atid) |
	    V_TID_COOKIE(CPL_COOKIE_KERN_TLS);
	OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ6,
		qid_atid));
	cpl->local_port = inp->inp_lport;
	cpl->local_ip_hi = *(uint64_t *)&inp->in6p_laddr.s6_addr[0];
	cpl->local_ip_lo = *(uint64_t *)&inp->in6p_laddr.s6_addr[8];
	cpl->peer_port = inp->inp_fport;
	cpl->peer_ip_hi = *(uint64_t *)&inp->in6p_faddr.s6_addr[0];
	cpl->peer_ip_lo = *(uint64_t *)&inp->in6p_faddr.s6_addr[8];

	options = F_TCAM_BYPASS | V_ULP_MODE(ULP_MODE_NONE);
	options |= V_SMAC_SEL(vi->smt_idx) | V_TX_CHAN(vi->pi->tx_chan);
	options |= F_NON_OFFLOAD;
	cpl->opt0 = htobe64(options);

	options = V_TX_QUEUE(sc->params.tp.tx_modq[vi->pi->tx_chan]);
	if (tp->t_flags & TF_REQ_TSTMP)
		options |= F_TSTAMPS_EN;
	cpl->opt2 = htobe32(options);
}

static int
send_ktls_act_open_req(struct adapter *sc, struct vi_info *vi,
    struct inpcb *inp, struct tlspcb *tlsp, int atid)
{
	struct wrqe *wr;
	bool isipv6;

	isipv6 = (inp->inp_vflag & INP_IPV6) != 0;
	if (isipv6) {
		tlsp->ce = t4_hold_lip(sc, &inp->in6p_laddr, NULL);
		if (tlsp->ce == NULL)
			return (ENOENT);
	}

	wr = alloc_wrqe(ktls_act_open_cpl_size(isipv6), tlsp->ctrlq);
	if (wr == NULL) {
		CTR2(KTR_CXGBE, "%s: atid %d failed to alloc WR", __func__,
		    atid);
		return (ENOMEM);
	}

	if (isipv6)
		mk_ktls_act_open_req6(sc, vi, inp, tlsp, atid, wrtod(wr));
	else
		mk_ktls_act_open_req(sc, vi, inp, tlsp, atid, wrtod(wr));

	tlsp->open_pending = true;
	t4_wrq_tx(sc, wr);
	return (0);
}

static int
ktls_act_open_rpl(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_act_open_rpl *cpl = (const void *)(rss + 1);
	u_int atid = G_TID_TID(G_AOPEN_ATID(be32toh(cpl->atid_status)));
	u_int status = G_AOPEN_STATUS(be32toh(cpl->atid_status));
	struct tlspcb *tlsp = lookup_atid(sc, atid);
	struct inpcb *inp = tlsp->inp;

	CTR3(KTR_CXGBE, "%s: atid %d status %d", __func__, atid, status);
	free_atid(sc, atid);
	if (status == 0)
		tlsp->tid = GET_TID(cpl);

	INP_WLOCK(inp);
	tlsp->open_pending = false;
	wakeup(tlsp);
	INP_WUNLOCK(inp);
	return (0);
}

/* SET_TCB_FIELD sent as a ULP command looks like this */
#define LEN__SET_TCB_FIELD_ULP (sizeof(struct ulp_txpkt) + \
    sizeof(struct ulptx_idata) + sizeof(struct cpl_set_tcb_field_core))

_Static_assert((LEN__SET_TCB_FIELD_ULP + sizeof(struct ulptx_idata)) % 16 == 0,
    "CPL_SET_TCB_FIELD ULP command not 16-byte aligned");

static void
write_set_tcb_field_ulp(struct tlspcb *tlsp, void *dst, struct sge_txq *txq,
    uint16_t word, uint64_t mask, uint64_t val)
{
	struct ulp_txpkt *txpkt;
	struct ulptx_idata *idata;
	struct cpl_set_tcb_field_core *cpl;

	/* ULP_TXPKT */
	txpkt = dst;
	txpkt->cmd_dest = htobe32(V_ULPTX_CMD(ULP_TX_PKT) |
	    V_ULP_TXPKT_DATAMODIFY(0) |
	    V_ULP_TXPKT_CHANNELID(tlsp->vi->pi->port_id) | V_ULP_TXPKT_DEST(0) |
	    V_ULP_TXPKT_FID(txq->eq.cntxt_id) | V_ULP_TXPKT_RO(1));
	txpkt->len = htobe32(howmany(LEN__SET_TCB_FIELD_ULP, 16));

	/* ULPTX_IDATA sub-command */
	idata = (struct ulptx_idata *)(txpkt + 1);
	idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	idata->len = htobe32(sizeof(*cpl));

	/* CPL_SET_TCB_FIELD */
	cpl = (struct cpl_set_tcb_field_core *)(idata + 1);
	OPCODE_TID(cpl) = htobe32(MK_OPCODE_TID(CPL_SET_TCB_FIELD, tlsp->tid));
	cpl->reply_ctrl = htobe16(F_NO_REPLY);
	cpl->word_cookie = htobe16(V_WORD(word));
	cpl->mask = htobe64(mask);
	cpl->val = htobe64(val);

	/* ULPTX_NOOP */
	idata = (struct ulptx_idata *)(cpl + 1);
	idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
	idata->len = htobe32(0);
}

static int
ktls_set_tcb_fields(struct tlspcb *tlsp, struct tcpcb *tp, struct sge_txq *txq)
{
	struct fw_ulptx_wr *wr;
	struct mbuf *m;
	char *dst;
	void *items[1];
	int error, len;

	len = sizeof(*wr) + 3 * roundup2(LEN__SET_TCB_FIELD_ULP, 16);
	if (tp->t_flags & TF_REQ_TSTMP)
		len += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
	m = alloc_wr_mbuf(len, M_NOWAIT);
	if (m == NULL) {
		CTR2(KTR_CXGBE, "%s: tid %d failed to alloc WR mbuf", __func__,
		    tlsp->tid);
		return (ENOMEM);
	}
	m->m_pkthdr.snd_tag = m_snd_tag_ref(&tlsp->com);
	m->m_pkthdr.csum_flags |= CSUM_SND_TAG;

	/* FW_ULPTX_WR */
	wr = mtod(m, void *);
	wr->op_to_compl = htobe32(V_FW_WR_OP(FW_ULPTX_WR));
	wr->flowid_len16 = htobe32(F_FW_ULPTX_WR_DATA |
	    V_FW_WR_LEN16(len / 16));
	wr->cookie = 0;
	dst = (char *)(wr + 1);

        /* Clear TF_NON_OFFLOAD and set TF_CORE_BYPASS */
	write_set_tcb_field_ulp(tlsp, dst, txq, W_TCB_T_FLAGS,
	    V_TCB_T_FLAGS(V_TF_CORE_BYPASS(1) | V_TF_NON_OFFLOAD(1)),
	    V_TCB_T_FLAGS(V_TF_CORE_BYPASS(1)));
	dst += roundup2(LEN__SET_TCB_FIELD_ULP, 16);

	/* Clear the SND_UNA_RAW, SND_NXT_RAW, and SND_MAX_RAW offsets. */
	write_set_tcb_field_ulp(tlsp, dst, txq, W_TCB_SND_UNA_RAW,
	    V_TCB_SND_NXT_RAW(M_TCB_SND_NXT_RAW) |
	    V_TCB_SND_UNA_RAW(M_TCB_SND_UNA_RAW),
	    V_TCB_SND_NXT_RAW(0) | V_TCB_SND_UNA_RAW(0));
	dst += roundup2(LEN__SET_TCB_FIELD_ULP, 16);

	write_set_tcb_field_ulp(tlsp, dst, txq, W_TCB_SND_MAX_RAW,
	    V_TCB_SND_MAX_RAW(M_TCB_SND_MAX_RAW), V_TCB_SND_MAX_RAW(0));
	dst += roundup2(LEN__SET_TCB_FIELD_ULP, 16);

	if (tp->t_flags & TF_REQ_TSTMP) {
		write_set_tcb_field_ulp(tlsp, dst, txq, W_TCB_TIMESTAMP_OFFSET,
		    V_TCB_TIMESTAMP_OFFSET(M_TCB_TIMESTAMP_OFFSET),
		    V_TCB_TIMESTAMP_OFFSET(tp->ts_offset >> 28));
		dst += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
	}

	KASSERT(dst - (char *)wr == len, ("%s: length mismatch", __func__));

	items[0] = m;
	error = mp_ring_enqueue(txq->r, items, 1, 1);
	if (error)
		m_free(m);
	return (error);
}

int
cxgbe_tls_tag_alloc(struct ifnet *ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **pt)
{
	const struct ktls_session *tls;
	struct tlspcb *tlsp;
	struct adapter *sc;
	struct vi_info *vi;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sge_txq *txq;
	int atid, error, keyid;

	tls = params->tls.tls;

	/* Only TLS 1.1 and TLS 1.2 are currently supported. */
	if (tls->params.tls_vmajor != TLS_MAJOR_VER_ONE ||
	    tls->params.tls_vminor < TLS_MINOR_VER_ONE ||
	    tls->params.tls_vminor > TLS_MINOR_VER_TWO)
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
		break;
	case CRYPTO_AES_NIST_GCM_16:
		if (tls->params.iv_len != SALT_SIZE)
			return (EINVAL);
		switch (tls->params.cipher_key_len) {
		case 128 / 8:
		case 192 / 8:
		case 256 / 8:
			break;
		default:
			return (EINVAL);
		}
		break;
	default:
		return (EPROTONOSUPPORT);
	}

	vi = ifp->if_softc;
	sc = vi->adapter;

	tlsp = alloc_tlspcb(ifp, vi, M_WAITOK);

	atid = alloc_atid(sc, tlsp);
	if (atid < 0) {
		error = ENOMEM;
		goto failed;
	}

	if (sc->tlst.inline_keys)
		keyid = -1;
	else
		keyid = get_new_keyid(tlsp);
	if (keyid < 0) {
		CTR2(KTR_CXGBE, "%s: atid %d using immediate key ctx", __func__,
		    atid);
		tlsp->inline_key = true;
	} else {
		tlsp->tx_key_addr = keyid;
		CTR3(KTR_CXGBE, "%s: atid %d allocated TX key addr %#x",
		    __func__,
		    atid, tlsp->tx_key_addr);
	}

	inp = params->tls.inp;
	INP_RLOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		INP_RUNLOCK(inp);
		error = ECONNRESET;
		goto failed;
	}
	tlsp->inp = inp;

	tp = inp->inp_ppcb;
	if (tp->t_flags & TF_REQ_TSTMP) {
		tlsp->using_timestamps = true;
		if ((tp->ts_offset & 0xfffffff) != 0) {
			INP_RUNLOCK(inp);
			error = EINVAL;
			goto failed;
		}
	} else
		tlsp->using_timestamps = false;

	error = send_ktls_act_open_req(sc, vi, inp, tlsp, atid);
	if (error) {
		INP_RUNLOCK(inp);
		goto failed;
	}

	/* Wait for reply to active open. */
	CTR2(KTR_CXGBE, "%s: atid %d sent CPL_ACT_OPEN_REQ", __func__,
	    atid);
	while (tlsp->open_pending) {
		/*
		 * XXX: PCATCH?  We would then have to discard the PCB
		 * when the completion CPL arrived.
		 */
		error = rw_sleep(tlsp, &inp->inp_lock, 0, "t6tlsop", 0);
	}

	atid = -1;
	if (tlsp->tid < 0) {
		INP_RUNLOCK(inp);
		error = ENOMEM;
		goto failed;
	}

	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		INP_RUNLOCK(inp);
		error = ECONNRESET;
		goto failed;
	}

	txq = &sc->sge.txq[vi->first_txq];
	if (inp->inp_flowtype != M_HASHTYPE_NONE)
		txq += ((inp->inp_flowid % (vi->ntxq - vi->rsrv_noflowq)) +
		    vi->rsrv_noflowq);
	tlsp->txq = txq;

	error = ktls_set_tcb_fields(tlsp, tp, txq);
	INP_RUNLOCK(inp);
	if (error)
		goto failed;

	init_ktls_key_params(tlsp, tls);

	error = ktls_setup_keys(tlsp, tls, txq);
	if (error)
		goto failed;

	/* The SCMD fields used when encrypting a full TLS record. */
	tlsp->scmd0.seqno_numivs = htobe32(V_SCMD_SEQ_NO_CTRL(3) |
	    V_SCMD_PROTO_VERSION(tlsp->proto_ver) |
	    V_SCMD_ENC_DEC_CTRL(SCMD_ENCDECCTRL_ENCRYPT) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL((tlsp->mac_first == 0)) |
	    V_SCMD_CIPH_MODE(tlsp->enc_mode) |
	    V_SCMD_AUTH_MODE(tlsp->auth_mode) |
	    V_SCMD_HMAC_CTRL(tlsp->hmac_ctrl) |
	    V_SCMD_IV_SIZE(tlsp->iv_size) | V_SCMD_NUM_IVS(1));

	tlsp->scmd0.ivgen_hdrlen = V_SCMD_IV_GEN_CTRL(0) |
	    V_SCMD_TLS_FRAG_ENABLE(0);
	if (tlsp->inline_key)
		tlsp->scmd0.ivgen_hdrlen |= V_SCMD_KEY_CTX_INLINE(1);
	tlsp->scmd0.ivgen_hdrlen = htobe32(tlsp->scmd0.ivgen_hdrlen);

	/*
	 * The SCMD fields used when encrypting a partial TLS record
	 * (no trailer and possibly a truncated payload).
	 */
	tlsp->scmd0_short.seqno_numivs = V_SCMD_SEQ_NO_CTRL(0) |
	    V_SCMD_PROTO_VERSION(SCMD_PROTO_VERSION_GENERIC) |
	    V_SCMD_ENC_DEC_CTRL(SCMD_ENCDECCTRL_ENCRYPT) |
	    V_SCMD_CIPH_AUTH_SEQ_CTRL((tlsp->mac_first == 0)) |
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
	    V_SCMD_TLS_FRAG_ENABLE(0) |
	    V_SCMD_AADIVDROP(1);
	if (tlsp->inline_key)
		tlsp->scmd0_short.ivgen_hdrlen |= V_SCMD_KEY_CTX_INLINE(1);

	TXQ_LOCK(txq);
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM)
		txq->kern_tls_gcm++;
	else
		txq->kern_tls_cbc++;
	TXQ_UNLOCK(txq);
	*pt = &tlsp->com;
	return (0);

failed:
	if (atid >= 0)
		free_atid(sc, atid);
	m_snd_tag_rele(&tlsp->com);
	return (error);
}

static int
ktls_setup_keys(struct tlspcb *tlsp, const struct ktls_session *tls,
    struct sge_txq *txq)
{
	struct auth_hash *axf;
	int error, keyid, kwrlen, kctxlen, len;
	struct tls_key_req *kwr;
	struct tls_keyctx *kctx;
	void *items[1], *key;
	struct tx_keyctx_hdr *khdr;
	unsigned int ck_size, mk_size, partial_digest_len;
	struct mbuf *m;

	/*
	 * Store the salt and keys in the key context.  For
	 * connections with an inline key, this key context is passed
	 * as immediate data in each work request.  For connections
	 * storing the key in DDR, a work request is used to store a
	 * copy of the key context in DDR.
	 */
	kctx = &tlsp->keyctx;
	khdr = &kctx->txhdr;

	switch (tlsp->cipher_secret_size) {
	case 128 / 8:
		ck_size = CHCR_KEYCTX_CIPHER_KEY_SIZE_128;
		break;
	case 192 / 8:
		ck_size = CHCR_KEYCTX_CIPHER_KEY_SIZE_192;
		break;
	case 256 / 8:
		ck_size = CHCR_KEYCTX_CIPHER_KEY_SIZE_256;
		break;
	default:
		panic("bad key size");
	}
	axf = NULL;
	partial_digest_len = 0;
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM)
		mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_512;
	else {
		switch (tlsp->auth_mode) {
		case SCMD_AUTH_MODE_SHA1:
			axf = &auth_hash_hmac_sha1;
			mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_160;
			partial_digest_len = SHA1_HASH_LEN;
			break;
		case SCMD_AUTH_MODE_SHA256:
			axf = &auth_hash_hmac_sha2_256;
			mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_256;
			partial_digest_len = SHA2_256_HASH_LEN;
			break;
		case SCMD_AUTH_MODE_SHA512_384:
			axf = &auth_hash_hmac_sha2_384;
			mk_size = CHCR_KEYCTX_MAC_KEY_SIZE_512;
			partial_digest_len = SHA2_512_HASH_LEN;
			break;
		default:
			panic("bad auth mode");
		}
	}

	khdr->ctxlen = (tlsp->tx_key_info_size >> 4);
	khdr->dualck_to_txvalid = V_TLS_KEYCTX_TX_WR_SALT_PRESENT(1) |
	    V_TLS_KEYCTX_TX_WR_TXCK_SIZE(ck_size) |
	    V_TLS_KEYCTX_TX_WR_TXMK_SIZE(mk_size) |
	    V_TLS_KEYCTX_TX_WR_TXVALID(1);
	if (tlsp->enc_mode != SCMD_CIPH_MODE_AES_GCM)
		khdr->dualck_to_txvalid |= V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(1);
	khdr->dualck_to_txvalid = htobe16(khdr->dualck_to_txvalid);
	key = kctx->keys.edkey;
	memcpy(key, tls->params.cipher_key, tls->params.cipher_key_len);
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM) {
		memcpy(khdr->txsalt, tls->params.iv, SALT_SIZE);
		t4_init_gmac_hash(tls->params.cipher_key,
		    tls->params.cipher_key_len,
		    (char *)key + tls->params.cipher_key_len);
	} else {
		t4_init_hmac_digest(axf, partial_digest_len,
		    tls->params.auth_key, tls->params.auth_key_len,
		    (char *)key + tls->params.cipher_key_len);
	}

	if (tlsp->inline_key)
		return (0);

	keyid = tlsp->tx_key_addr;

	/* Populate key work request. */
	kwrlen = sizeof(*kwr);
	kctxlen = roundup2(sizeof(*kctx), 32);
	len = kwrlen + kctxlen;

        m = alloc_wr_mbuf(len, M_NOWAIT);
	if (m == NULL) {
		CTR2(KTR_CXGBE, "%s: tid %d failed to alloc WR mbuf", __func__,
		    tlsp->tid);
		return (ENOMEM);
	}
	m->m_pkthdr.snd_tag = m_snd_tag_ref(&tlsp->com);
	m->m_pkthdr.csum_flags |= CSUM_SND_TAG;
	kwr = mtod(m, void *);
	memset(kwr, 0, len);

	kwr->wr_hi = htobe32(V_FW_WR_OP(FW_ULPTX_WR) |
	    F_FW_WR_ATOMIC);
	kwr->wr_mid = htobe32(V_FW_WR_LEN16(DIV_ROUND_UP(len, 16)));
	kwr->protocol = tlsp->proto_ver;
	kwr->mfs = htons(tlsp->frag_size);
	kwr->reneg_to_write_rx = KEY_WRITE_TX;

	/* master command */
	kwr->cmd = htobe32(V_ULPTX_CMD(ULP_TX_MEM_WRITE) |
	    V_T5_ULP_MEMIO_ORDER(1) | V_T5_ULP_MEMIO_IMM(1));
	kwr->dlen = htobe32(V_ULP_MEMIO_DATA_LEN(kctxlen >> 5));
	kwr->len16 = htobe32((tlsp->tid << 8) |
	    DIV_ROUND_UP(len - sizeof(struct work_request_hdr), 16));
	kwr->kaddr = htobe32(V_ULP_MEMIO_ADDR(keyid >> 5));

	/* sub command */
	kwr->sc_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	kwr->sc_len = htobe32(kctxlen);

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
		CTR2(KTR_CXGBE, "%s: tid %d sent key WR", __func__, tlsp->tid);
	return (error);
}

static u_int
ktls_base_wr_size(struct tlspcb *tlsp)
{
	u_int wr_len;

	wr_len = sizeof(struct fw_ulptx_wr);	// 16
	wr_len += sizeof(struct ulp_txpkt);	// 8
	wr_len += sizeof(struct ulptx_idata);	// 8
	wr_len += sizeof(struct cpl_tx_sec_pdu);// 32
	if (tlsp->inline_key)
		wr_len += tlsp->tx_key_info_size;
	else {
		wr_len += sizeof(struct ulptx_sc_memrd);// 8
		wr_len += sizeof(struct ulptx_idata);	// 8
	}
	wr_len += sizeof(struct cpl_tx_data);	// 16
	return (wr_len);
}

/* How many bytes of TCP payload to send for a given TLS record. */
static u_int
ktls_tcp_payload_length(struct tlspcb *tlsp, struct mbuf *m_tls)
{
	struct tls_record_layer *hdr;
	u_int plen, mlen;

	M_ASSERTEXTPG(m_tls);
	hdr = (void *)m_tls->m_epg_hdr;
	plen = ntohs(hdr->tls_length);

	/*
	 * What range of the TLS record is the mbuf requesting to be
	 * sent.
	 */
	mlen = mtod(m_tls, vm_offset_t) + m_tls->m_len;

	/* Always send complete records. */
	if (mlen == TLS_HEADER_LENGTH + plen)
		return (mlen);

	/*
	 * If the host stack has asked to send part of the trailer,
	 * trim the length to avoid sending any of the trailer.  There
	 * is no way to send a partial trailer currently.
	 */
	if (mlen > TLS_HEADER_LENGTH + plen - m_tls->m_epg_trllen)
		mlen = TLS_HEADER_LENGTH + plen - m_tls->m_epg_trllen;


	/*
	 * For AES-CBC adjust the ciphertext length for the block
	 * size.
	 */
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_CBC &&
	    mlen > TLS_HEADER_LENGTH) {
		mlen = TLS_HEADER_LENGTH + rounddown(mlen - TLS_HEADER_LENGTH,
		    AES_BLOCK_LEN);
	}

#ifdef VERBOSE_TRACES
	CTR4(KTR_CXGBE, "%s: tid %d short TLS record (%u vs %u)",
	    __func__, tlsp->tid, mlen, TLS_HEADER_LENGTH + plen);
#endif
	return (mlen);
}

/*
 * For a "short" TLS record, determine the offset into the TLS record
 * payload to send.  This offset does not include the TLS header, but
 * a non-zero offset implies that a header will not be sent.
 */
static u_int
ktls_payload_offset(struct tlspcb *tlsp, struct mbuf *m_tls)
{
	struct tls_record_layer *hdr;
	u_int offset, plen;
#ifdef INVARIANTS
	u_int mlen;
#endif

	M_ASSERTEXTPG(m_tls);
	hdr = (void *)m_tls->m_epg_hdr;
	plen = ntohs(hdr->tls_length);
#ifdef INVARIANTS
	mlen = mtod(m_tls, vm_offset_t) + m_tls->m_len;
	MPASS(mlen < TLS_HEADER_LENGTH + plen);
#endif
	if (mtod(m_tls, vm_offset_t) <= m_tls->m_epg_hdrlen)
		return (0);
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM) {
		/*
		 * Always send something.  This function is only called
		 * if we aren't sending the tag at all, but if the
		 * request starts in the tag then we are in an odd
		 * state where would effectively send nothing.  Cap
		 * the offset at the last byte of the record payload
		 * to send the last cipher block.
		 */
		offset = min(mtod(m_tls, vm_offset_t) - m_tls->m_epg_hdrlen,
		    (plen - TLS_HEADER_LENGTH - m_tls->m_epg_trllen) - 1);
		return (rounddown(offset, AES_BLOCK_LEN));
	}
	return (0);
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

static int
ktls_wr_len(struct tlspcb *tlsp, struct mbuf *m, struct mbuf *m_tls,
    int *nsegsp)
{
	struct tls_record_layer *hdr;
	u_int imm_len, offset, plen, wr_len, tlen;

	M_ASSERTEXTPG(m_tls);

	/*
	 * Determine the size of the TLS record payload to send
	 * excluding header and trailer.
	 */
	tlen = ktls_tcp_payload_length(tlsp, m_tls);
	if (tlen <= m_tls->m_epg_hdrlen) {
		/*
		 * For requests that only want to send the TLS header,
		 * send a tunnelled packet as immediate data.
		 */
		wr_len = sizeof(struct fw_eth_tx_pkt_wr) +
		    sizeof(struct cpl_tx_pkt_core) +
		    roundup2(m->m_len + m_tls->m_len, 16);
		if (wr_len > SGE_MAX_WR_LEN) {
			CTR3(KTR_CXGBE,
		    "%s: tid %d TLS header-only packet too long (len %d)",
			    __func__, tlsp->tid, m->m_len + m_tls->m_len);
		}

		/* This should always be the last TLS record in a chain. */
		MPASS(m_tls->m_next == NULL);

		/*
		 * XXX: Set a bogus 'nsegs' value to avoid tripping an
		 * assertion in mbuf_nsegs() in t4_sge.c.
		 */
		*nsegsp = 1;
		return (wr_len);
	}

	hdr = (void *)m_tls->m_epg_hdr;
	plen = TLS_HEADER_LENGTH + ntohs(hdr->tls_length) - m_tls->m_epg_trllen;
	if (tlen < plen) {
		plen = tlen;
		offset = ktls_payload_offset(tlsp, m_tls);
	} else
		offset = 0;

	/* Calculate the size of the work request. */
	wr_len = ktls_base_wr_size(tlsp);

	/*
	 * Full records and short records with an offset of 0 include
	 * the TLS header as immediate data.  Short records include a
	 * raw AES IV as immediate data.
	 */
	imm_len = 0;
	if (offset == 0)
		imm_len += m_tls->m_epg_hdrlen;
	if (plen == tlen)
		imm_len += AES_BLOCK_LEN;
	wr_len += roundup2(imm_len, 16);

	/* TLS record payload via DSGL. */
	*nsegsp = sglist_count_mbuf_epg(m_tls, m_tls->m_epg_hdrlen + offset,
	    plen - (m_tls->m_epg_hdrlen + offset));
	wr_len += ktls_sgl_size(*nsegsp);

	wr_len = roundup2(wr_len, 16);
	return (wr_len);
}

/*
 * See if we have any TCP options requiring a dedicated options-only
 * packet.
 */
static int
ktls_has_tcp_options(struct tcphdr *tcp)
{
	u_char *cp;
	int cnt, opt, optlen;

	cp = (u_char *)(tcp + 1);
	cnt = tcp->th_off * 4 - sizeof(struct tcphdr);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		switch (opt) {
		case TCPOPT_NOP:
		case TCPOPT_TIMESTAMP:
			break;
		default:
			return (1);
		}
	}
	return (0);
}

/*
 * Find the TCP timestamp option.
 */
static void *
ktls_find_tcp_timestamps(struct tcphdr *tcp)
{
	u_char *cp;
	int cnt, opt, optlen;

	cp = (u_char *)(tcp + 1);
	cnt = tcp->th_off * 4 - sizeof(struct tcphdr);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		if (opt == TCPOPT_TIMESTAMP && optlen == TCPOLEN_TIMESTAMP)
			return (cp + 2);
	}
	return (NULL);
}

int
t6_ktls_parse_pkt(struct mbuf *m, int *nsegsp, int *len16p)
{
	struct tlspcb *tlsp;
	struct ether_header *eh;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *tcp;
	struct mbuf *m_tls;
	int nsegs;
	u_int wr_len, tot_len;

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
		CTR2(KTR_CXGBE, "%s: tid %d header mbuf too short", __func__,
		    tlsp->tid);
		return (EINVAL);
	}
	eh = mtod(m, struct ether_header *);
	if (ntohs(eh->ether_type) != ETHERTYPE_IP &&
	    ntohs(eh->ether_type) != ETHERTYPE_IPV6) {
		CTR2(KTR_CXGBE, "%s: tid %d mbuf not ETHERTYPE_IP{,V6}",
		    __func__, tlsp->tid);
		return (EINVAL);
	}
	m->m_pkthdr.l2hlen = sizeof(*eh);

	/* XXX: Reject unsupported IP options? */
	if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
		ip = (struct ip *)(eh + 1);
		if (ip->ip_p != IPPROTO_TCP) {
			CTR2(KTR_CXGBE, "%s: tid %d mbuf not IPPROTO_TCP",
			    __func__, tlsp->tid);
			return (EINVAL);
		}
		m->m_pkthdr.l3hlen = ip->ip_hl * 4;
	} else {
		ip6 = (struct ip6_hdr *)(eh + 1);
		if (ip6->ip6_nxt != IPPROTO_TCP) {
			CTR3(KTR_CXGBE, "%s: tid %d mbuf not IPPROTO_TCP (%u)",
			    __func__, tlsp->tid, ip6->ip6_nxt);
			return (EINVAL);
		}
		m->m_pkthdr.l3hlen = sizeof(struct ip6_hdr);
	}
	if (m->m_len < m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen +
	    sizeof(*tcp)) {
		CTR2(KTR_CXGBE, "%s: tid %d header mbuf too short (2)",
		    __func__, tlsp->tid);
		return (EINVAL);
	}
	tcp = (struct tcphdr *)((char *)(eh + 1) + m->m_pkthdr.l3hlen);
	m->m_pkthdr.l4hlen = tcp->th_off * 4;

	/* Bail if there is TCP payload before the TLS record. */
	if (m->m_len != m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen +
	    m->m_pkthdr.l4hlen) {
		CTR6(KTR_CXGBE,
		    "%s: tid %d header mbuf bad length (%d + %d + %d != %d)",
		    __func__, tlsp->tid, m->m_pkthdr.l2hlen,
		    m->m_pkthdr.l3hlen, m->m_pkthdr.l4hlen, m->m_len);
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
	*nsegsp = 0;
	for (m_tls = m->m_next; m_tls != NULL; m_tls = m_tls->m_next) {
		MPASS(m_tls->m_flags & M_EXTPG);

		wr_len = ktls_wr_len(tlsp, m, m_tls, &nsegs);
#ifdef VERBOSE_TRACES
		CTR4(KTR_CXGBE, "%s: tid %d wr_len %d nsegs %d", __func__,
		    tlsp->tid, wr_len, nsegs);
#endif
		if (wr_len > SGE_MAX_WR_LEN || nsegs > TX_SGL_SEGS)
			return (EFBIG);
		tot_len += roundup2(wr_len, EQ_ESIZE);

		/*
		 * Store 'nsegs' for the first TLS record in the
		 * header mbuf's metadata.
		 */
		if (*nsegsp == 0)
			*nsegsp = nsegs;
	}

	MPASS(tot_len != 0);

	/*
	 * See if we have any TCP options or a FIN requiring a
	 * dedicated packet.
	 */
	if ((tcp->th_flags & TH_FIN) != 0 || ktls_has_tcp_options(tcp)) {
		wr_len = sizeof(struct fw_eth_tx_pkt_wr) +
		    sizeof(struct cpl_tx_pkt_core) + roundup2(m->m_len, 16);
		if (wr_len > SGE_MAX_WR_LEN) {
			CTR3(KTR_CXGBE,
			    "%s: tid %d options-only packet too long (len %d)",
			    __func__, tlsp->tid, m->m_len);
			return (EINVAL);
		}
		tot_len += roundup2(wr_len, EQ_ESIZE);
	}

	/* Include room for a TP work request to program an L2T entry. */
	tot_len += EQ_ESIZE;

	/*
	 * Include room for a ULPTX work request including up to 5
	 * CPL_SET_TCB_FIELD commands before the first TLS work
	 * request.
	 */
	wr_len = sizeof(struct fw_ulptx_wr) +
	    5 * roundup2(LEN__SET_TCB_FIELD_ULP, 16);

	/*
	 * If timestamps are present, reserve 1 more command for
	 * setting the echoed timestamp.
	 */
	if (tlsp->using_timestamps)
		wr_len += roundup2(LEN__SET_TCB_FIELD_ULP, 16);

	tot_len += roundup2(wr_len, EQ_ESIZE);

	*len16p = tot_len / 16;
#ifdef VERBOSE_TRACES
	CTR4(KTR_CXGBE, "%s: tid %d len16 %d nsegs %d", __func__,
	    tlsp->tid, *len16p, *nsegsp);
#endif
	return (0);
}

/*
 * If the SGL ends on an address that is not 16 byte aligned, this function will
 * add a 0 filled flit at the end.
 */
static void
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
}

static inline void
copy_to_txd(struct sge_eq *eq, caddr_t from, caddr_t *to, int len)
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
ktls_write_tcp_options(struct sge_txq *txq, void *dst, struct mbuf *m,
    u_int available, u_int pidx)
{
	struct tx_sdesc *txsd;
	struct fw_eth_tx_pkt_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	uint32_t ctrl;
	uint64_t ctrl1;
	int len16, ndesc, pktlen;
	struct ether_header *eh;
	struct ip *ip, newip;
	struct ip6_hdr *ip6, newip6;
	struct tcphdr *tcp, newtcp;
	caddr_t out;

	TXQ_LOCK_ASSERT_OWNED(txq);
	M_ASSERTPKTHDR(m);

	wr = dst;
	pktlen = m->m_len;
	ctrl = sizeof(struct cpl_tx_pkt_core) + pktlen;
	len16 = howmany(sizeof(struct fw_eth_tx_pkt_wr) + ctrl, 16);
	ndesc = tx_len16_to_desc(len16);
	MPASS(ndesc <= available);

	/* Firmware work request header */
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
	if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
		ip = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip = *ip;
		newip.ip_len = htons(pktlen - m->m_pkthdr.l2hlen);
		copy_to_txd(&txq->eq, (caddr_t)&newip, &out, sizeof(newip));
		if (m->m_pkthdr.l3hlen > sizeof(*ip))
			copy_to_txd(&txq->eq, (caddr_t)(ip + 1), &out,
			    m->m_pkthdr.l3hlen - sizeof(*ip));
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	} else {
		ip6 = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip6 = *ip6;
		newip6.ip6_plen = htons(pktlen - m->m_pkthdr.l2hlen);
		copy_to_txd(&txq->eq, (caddr_t)&newip6, &out, sizeof(newip6));
		MPASS(m->m_pkthdr.l3hlen == sizeof(*ip6));
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP6) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	}
	cpl->ctrl1 = htobe64(ctrl1);
	txq->txcsum++;

	/* Clear PUSH and FIN in the TCP header if present. */
	tcp = (void *)((char *)eh + m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen);
	newtcp = *tcp;
	newtcp.th_flags &= ~(TH_PUSH | TH_FIN);
	copy_to_txd(&txq->eq, (caddr_t)&newtcp, &out, sizeof(newtcp));

	/* Copy rest of packet. */
	copy_to_txd(&txq->eq, (caddr_t)(tcp + 1), &out, pktlen -
	    (m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen + sizeof(*tcp)));
	txq->imm_wrs++;

	txq->txpkt_wrs++;

	txq->kern_tls_options++;

	txsd = &txq->sdesc[pidx];
	txsd->m = NULL;
	txsd->desc_used = ndesc;

	return (ndesc);
}

static int
ktls_write_tunnel_packet(struct sge_txq *txq, void *dst, struct mbuf *m,
    struct mbuf *m_tls, u_int available, tcp_seq tcp_seqno, u_int pidx)
{
	struct tx_sdesc *txsd;
	struct fw_eth_tx_pkt_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	uint32_t ctrl;
	uint64_t ctrl1;
	int len16, ndesc, pktlen;
	struct ether_header *eh;
	struct ip *ip, newip;
	struct ip6_hdr *ip6, newip6;
	struct tcphdr *tcp, newtcp;
	caddr_t out;

	TXQ_LOCK_ASSERT_OWNED(txq);
	M_ASSERTPKTHDR(m);

	/* Locate the template TLS header. */
	M_ASSERTEXTPG(m_tls);

	/* This should always be the last TLS record in a chain. */
	MPASS(m_tls->m_next == NULL);

	wr = dst;
	pktlen = m->m_len + m_tls->m_len;
	ctrl = sizeof(struct cpl_tx_pkt_core) + pktlen;
	len16 = howmany(sizeof(struct fw_eth_tx_pkt_wr) + ctrl, 16);
	ndesc = tx_len16_to_desc(len16);
	MPASS(ndesc <= available);

	/* Firmware work request header */
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
	if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
		ip = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip = *ip;
		newip.ip_len = htons(pktlen - m->m_pkthdr.l2hlen);
		copy_to_txd(&txq->eq, (caddr_t)&newip, &out, sizeof(newip));
		if (m->m_pkthdr.l3hlen > sizeof(*ip))
			copy_to_txd(&txq->eq, (caddr_t)(ip + 1), &out,
			    m->m_pkthdr.l3hlen - sizeof(*ip));
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	} else {
		ip6 = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip6 = *ip6;
		newip6.ip6_plen = htons(pktlen - m->m_pkthdr.l2hlen);
		copy_to_txd(&txq->eq, (caddr_t)&newip6, &out, sizeof(newip6));
		MPASS(m->m_pkthdr.l3hlen == sizeof(*ip6));
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP6) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	}
	cpl->ctrl1 = htobe64(ctrl1);
	txq->txcsum++;

	/* Set sequence number in TCP header. */
	tcp = (void *)((char *)eh + m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen);
	newtcp = *tcp;
	newtcp.th_seq = htonl(tcp_seqno + mtod(m_tls, vm_offset_t));
	copy_to_txd(&txq->eq, (caddr_t)&newtcp, &out, sizeof(newtcp));

	/* Copy rest of TCP header. */
	copy_to_txd(&txq->eq, (caddr_t)(tcp + 1), &out, m->m_len -
	    (m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen + sizeof(*tcp)));

	/* Copy the subset of the TLS header requested. */
	copy_to_txd(&txq->eq, (char *)m_tls->m_epg_hdr +
	    mtod(m_tls, vm_offset_t), &out, m_tls->m_len);
	txq->imm_wrs++;

	txq->txpkt_wrs++;

	txq->kern_tls_header++;

	txsd = &txq->sdesc[pidx];
	txsd->m = m;
	txsd->desc_used = ndesc;

	return (ndesc);
}

_Static_assert(sizeof(struct cpl_set_tcb_field) <= EQ_ESIZE,
    "CPL_SET_TCB_FIELD must be smaller than a single TX descriptor");
_Static_assert(W_TCB_SND_UNA_RAW == W_TCB_SND_NXT_RAW,
    "SND_NXT_RAW and SND_UNA_RAW are in different words");

static int
ktls_write_tls_wr(struct tlspcb *tlsp, struct sge_txq *txq,
    void *dst, struct mbuf *m, struct tcphdr *tcp, struct mbuf *m_tls,
    u_int nsegs, u_int available, tcp_seq tcp_seqno, uint32_t *tsopt,
    u_int pidx, bool set_l2t_idx)
{
	struct sge_eq *eq = &txq->eq;
	struct tx_sdesc *txsd;
	struct fw_ulptx_wr *wr;
	struct ulp_txpkt *txpkt;
	struct ulptx_sc_memrd *memrd;
	struct ulptx_idata *idata;
	struct cpl_tx_sec_pdu *sec_pdu;
	struct cpl_tx_data *tx_data;
	struct tls_record_layer *hdr;
	char *iv, *out;
	u_int aad_start, aad_stop;
	u_int auth_start, auth_stop, auth_insert;
	u_int cipher_start, cipher_stop, iv_offset;
	u_int imm_len, mss, ndesc, offset, plen, tlen, twr_len, wr_len;
	u_int fields, tx_max_offset, tx_max;
	bool first_wr, last_wr, using_scratch;

	ndesc = 0;
	MPASS(tlsp->txq == txq);

	first_wr = (tlsp->prev_seq == 0 && tlsp->prev_ack == 0 &&
	    tlsp->prev_win == 0);

	/*
	 * Use the per-txq scratch pad if near the end of the ring to
	 * simplify handling of wrap-around.  This uses a simple but
	 * not quite perfect test of using the scratch buffer if we
	 * can't fit a maximal work request in without wrapping.
	 */
	using_scratch = (eq->sidx - pidx < SGE_MAX_WR_LEN / EQ_ESIZE);

	/* Locate the TLS header. */
	M_ASSERTEXTPG(m_tls);
	hdr = (void *)m_tls->m_epg_hdr;
	plen = TLS_HEADER_LENGTH + ntohs(hdr->tls_length) - m_tls->m_epg_trllen;

	/* Determine how much of the TLS record to send. */
	tlen = ktls_tcp_payload_length(tlsp, m_tls);
	if (tlen <= m_tls->m_epg_hdrlen) {
		/*
		 * For requests that only want to send the TLS header,
		 * send a tunnelled packet as immediate data.
		 */
#ifdef VERBOSE_TRACES
		CTR3(KTR_CXGBE, "%s: tid %d header-only TLS record %u",
		    __func__, tlsp->tid, (u_int)m_tls->m_epg_seqno);
#endif
		return (ktls_write_tunnel_packet(txq, dst, m, m_tls, available,
		    tcp_seqno, pidx));
	}
	if (tlen < plen) {
		plen = tlen;
		offset = ktls_payload_offset(tlsp, m_tls);
#ifdef VERBOSE_TRACES
		CTR4(KTR_CXGBE, "%s: tid %d short TLS record %u with offset %u",
		    __func__, tlsp->tid, (u_int)m_tls->m_epg_seqno, offset);
#endif
		if (m_tls->m_next == NULL && (tcp->th_flags & TH_FIN) != 0) {
			txq->kern_tls_fin_short++;
#ifdef INVARIANTS
			panic("%s: FIN on short TLS record", __func__);
#endif
		}
	} else
		offset = 0;

	/*
	 * This is the last work request for a given TLS mbuf chain if
	 * it is the last mbuf in the chain and FIN is not set.  If
	 * FIN is set, then ktls_write_tcp_fin() will write out the
	 * last work request.
	 */
	last_wr = m_tls->m_next == NULL && (tcp->th_flags & TH_FIN) == 0;

	/*
	 * The host stack may ask us to not send part of the start of
	 * a TLS record.  (For example, the stack might have
	 * previously sent a "short" TLS record and might later send
	 * down an mbuf that requests to send the remainder of the TLS
	 * record.)  The crypto engine must process a TLS record from
	 * the beginning if computing a GCM tag or HMAC, so we always
	 * send the TLS record from the beginning as input to the
	 * crypto engine and via CPL_TX_DATA to TP.  However, TP will
	 * drop individual packets after they have been chopped up
	 * into MSS-sized chunks if the entire sequence range of those
	 * packets is less than SND_UNA.  SND_UNA is computed as
	 * TX_MAX - SND_UNA_RAW.  Thus, use the offset stored in
	 * m_data to set TX_MAX to the first byte in the TCP sequence
	 * space the host actually wants us to send and set
	 * SND_UNA_RAW to 0.
	 *
	 * If the host sends us back to back requests that span the
	 * trailer of a single TLS record (first request ends "in" the
	 * trailer and second request starts at the next byte but
	 * still "in" the trailer), the initial bytes of the trailer
	 * that the first request drops will not be retransmitted.  If
	 * the host uses the same requests when retransmitting the
	 * connection will hang.  To handle this, always transmit the
	 * full trailer for a request that begins "in" the trailer
	 * (the second request in the example above).  This should
	 * also help to avoid retransmits for the common case.
	 *
	 * A similar condition exists when using CBC for back to back
	 * requests that span a single AES block.  The first request
	 * will be truncated to end at the end of the previous AES
	 * block.  To handle this, always begin transmission at the
	 * start of the current AES block.
	 */
	tx_max_offset = mtod(m_tls, vm_offset_t);
	if (tx_max_offset > TLS_HEADER_LENGTH + ntohs(hdr->tls_length) -
	    m_tls->m_epg_trllen) {
		/* Always send the full trailer. */
		tx_max_offset = TLS_HEADER_LENGTH + ntohs(hdr->tls_length) -
		    m_tls->m_epg_trllen;
	}
	if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_CBC &&
	    tx_max_offset > TLS_HEADER_LENGTH) {
		/* Always send all of the first AES block. */
		tx_max_offset = TLS_HEADER_LENGTH +
		    rounddown(tx_max_offset - TLS_HEADER_LENGTH,
		    AES_BLOCK_LEN);
	}
	tx_max = tcp_seqno + tx_max_offset;

	/*
	 * Update TCB fields.  Reserve space for the FW_ULPTX_WR header
	 * but don't populate it until we know how many field updates
	 * are required.
	 */
	if (using_scratch)
		wr = (void *)txq->ss;
	else
		wr = dst;
	out = (void *)(wr + 1);
	fields = 0;
	if (set_l2t_idx) {
		KASSERT(nsegs != 0,
		    ("trying to set L2T_IX for subsequent TLS WR"));
#ifdef VERBOSE_TRACES
		CTR3(KTR_CXGBE, "%s: tid %d set L2T_IX to %d", __func__,
		    tlsp->tid, tlsp->l2te->idx);
#endif
		write_set_tcb_field_ulp(tlsp, out, txq, W_TCB_L2T_IX,
		    V_TCB_L2T_IX(M_TCB_L2T_IX), V_TCB_L2T_IX(tlsp->l2te->idx));
		out += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
		fields++;
	}
	if (tsopt != NULL && tlsp->prev_tsecr != ntohl(tsopt[1])) {
		KASSERT(nsegs != 0,
		    ("trying to set T_RTSEQ_RECENT for subsequent TLS WR"));
#ifdef VERBOSE_TRACES
		CTR2(KTR_CXGBE, "%s: tid %d wrote updated T_RTSEQ_RECENT",
		    __func__, tlsp->tid);
#endif
		write_set_tcb_field_ulp(tlsp, out, txq, W_TCB_T_RTSEQ_RECENT,
		    V_TCB_T_RTSEQ_RECENT(M_TCB_T_RTSEQ_RECENT),
		    V_TCB_T_RTSEQ_RECENT(ntohl(tsopt[1])));
		out += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
		fields++;

		tlsp->prev_tsecr = ntohl(tsopt[1]);
	}

	if (first_wr || tlsp->prev_seq != tx_max) {
		KASSERT(nsegs != 0,
		    ("trying to set TX_MAX for subsequent TLS WR"));
#ifdef VERBOSE_TRACES
		CTR4(KTR_CXGBE,
		    "%s: tid %d setting TX_MAX to %u (tcp_seqno %u)",
		    __func__, tlsp->tid, tx_max, tcp_seqno);
#endif
		write_set_tcb_field_ulp(tlsp, out, txq, W_TCB_TX_MAX,
		    V_TCB_TX_MAX(M_TCB_TX_MAX), V_TCB_TX_MAX(tx_max));
		out += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
		fields++;
	}

	/*
	 * If there is data to drop at the beginning of this TLS
	 * record or if this is a retransmit,
	 * reset SND_UNA_RAW to 0 so that SND_UNA == TX_MAX.
	 */
	if (tlsp->prev_seq != tx_max || mtod(m_tls, vm_offset_t) != 0) {
		KASSERT(nsegs != 0,
		    ("trying to clear SND_UNA_RAW for subsequent TLS WR"));
#ifdef VERBOSE_TRACES
		CTR2(KTR_CXGBE, "%s: tid %d clearing SND_UNA_RAW", __func__,
		    tlsp->tid);
#endif
		write_set_tcb_field_ulp(tlsp, out, txq, W_TCB_SND_UNA_RAW,
		    V_TCB_SND_UNA_RAW(M_TCB_SND_UNA_RAW),
		    V_TCB_SND_UNA_RAW(0));
		out += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
		fields++;
	}

	/*
	 * Store the expected sequence number of the next byte after
	 * this record.
	 */
	tlsp->prev_seq = tcp_seqno + tlen;

	if (first_wr || tlsp->prev_ack != ntohl(tcp->th_ack)) {
		KASSERT(nsegs != 0,
		    ("trying to set RCV_NXT for subsequent TLS WR"));
		write_set_tcb_field_ulp(tlsp, out, txq, W_TCB_RCV_NXT,
		    V_TCB_RCV_NXT(M_TCB_RCV_NXT),
		    V_TCB_RCV_NXT(ntohl(tcp->th_ack)));
		out += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
		fields++;

		tlsp->prev_ack = ntohl(tcp->th_ack);
	}

	if (first_wr || tlsp->prev_win != ntohs(tcp->th_win)) {
		KASSERT(nsegs != 0,
		    ("trying to set RCV_WND for subsequent TLS WR"));
		write_set_tcb_field_ulp(tlsp, out, txq, W_TCB_RCV_WND,
		    V_TCB_RCV_WND(M_TCB_RCV_WND),
		    V_TCB_RCV_WND(ntohs(tcp->th_win)));
		out += roundup2(LEN__SET_TCB_FIELD_ULP, 16);
		fields++;

		tlsp->prev_win = ntohs(tcp->th_win);
	}

	/* Recalculate 'nsegs' if cached value is not available. */
	if (nsegs == 0)
		nsegs = sglist_count_mbuf_epg(m_tls, m_tls->m_epg_hdrlen +
		    offset, plen - (m_tls->m_epg_hdrlen + offset));

	/* Calculate the size of the TLS work request. */
	twr_len = ktls_base_wr_size(tlsp);

	imm_len = 0;
	if (offset == 0)
		imm_len += m_tls->m_epg_hdrlen;
	if (plen == tlen)
		imm_len += AES_BLOCK_LEN;
	twr_len += roundup2(imm_len, 16);
	twr_len += ktls_sgl_size(nsegs);

	/*
	 * If any field updates were required, determine if they can
	 * be included in the TLS work request.  If not, use the
	 * FW_ULPTX_WR work request header at 'wr' as a dedicated work
	 * request for the field updates and start a new work request
	 * for the TLS work request afterward.
	 */
	if (fields != 0) {
		wr_len = fields * roundup2(LEN__SET_TCB_FIELD_ULP, 16);
		if (twr_len + wr_len <= SGE_MAX_WR_LEN &&
		    tlsp->sc->tlst.combo_wrs) {
			wr_len += twr_len;
			txpkt = (void *)out;
		} else {
			wr_len += sizeof(*wr);
			wr->op_to_compl = htobe32(V_FW_WR_OP(FW_ULPTX_WR));
			wr->flowid_len16 = htobe32(F_FW_ULPTX_WR_DATA |
			    V_FW_WR_LEN16(wr_len / 16));
			wr->cookie = 0;

			/*
			 * If we were using scratch space, copy the
			 * field updates work request to the ring.
			 */
			if (using_scratch) {
				out = dst;
				copy_to_txd(eq, txq->ss, &out, wr_len);
			}

			ndesc = howmany(wr_len, EQ_ESIZE);
			MPASS(ndesc <= available);

			txq->raw_wrs++;
			txsd = &txq->sdesc[pidx];
			txsd->m = NULL;
			txsd->desc_used = ndesc;
			IDXINCR(pidx, ndesc, eq->sidx);
			dst = &eq->desc[pidx];

			/*
			 * Determine if we should use scratch space
			 * for the TLS work request based on the
			 * available space after advancing pidx for
			 * the field updates work request.
			 */
			wr_len = twr_len;
			using_scratch = (eq->sidx - pidx <
			    howmany(wr_len, EQ_ESIZE));
			if (using_scratch)
				wr = (void *)txq->ss;
			else
				wr = dst;
			txpkt = (void *)(wr + 1);
		}
	} else {
		wr_len = twr_len;
		txpkt = (void *)out;
	}

	wr_len = roundup2(wr_len, 16);
	MPASS(ndesc + howmany(wr_len, EQ_ESIZE) <= available);

	/* FW_ULPTX_WR */
	wr->op_to_compl = htobe32(V_FW_WR_OP(FW_ULPTX_WR));
	wr->flowid_len16 = htobe32(F_FW_ULPTX_WR_DATA |
	    V_FW_WR_LEN16(wr_len / 16));
	wr->cookie = 0;

	/* ULP_TXPKT */
	txpkt->cmd_dest = htobe32(V_ULPTX_CMD(ULP_TX_PKT) |
	    V_ULP_TXPKT_DATAMODIFY(0) |
	    V_ULP_TXPKT_CHANNELID(tlsp->vi->pi->port_id) | V_ULP_TXPKT_DEST(0) |
	    V_ULP_TXPKT_FID(txq->eq.cntxt_id) | V_ULP_TXPKT_RO(1));
	txpkt->len = htobe32(howmany(twr_len - sizeof(*wr), 16));

	/* ULPTX_IDATA sub-command */
	idata = (void *)(txpkt + 1);
	idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM) |
	    V_ULP_TX_SC_MORE(1));
	idata->len = sizeof(struct cpl_tx_sec_pdu);

	/*
	 * The key context, CPL_TX_DATA, and immediate data are part
	 * of this ULPTX_IDATA when using an inline key.  When reading
	 * the key from memory, the CPL_TX_DATA and immediate data are
	 * part of a separate ULPTX_IDATA.
	 */
	if (tlsp->inline_key)
		idata->len += tlsp->tx_key_info_size +
		    sizeof(struct cpl_tx_data) + imm_len;
	idata->len = htobe32(idata->len);

	/* CPL_TX_SEC_PDU */
	sec_pdu = (void *)(idata + 1);

	/*
	 * For short records, AAD is counted as header data in SCMD0,
	 * the IV is next followed by a cipher region for the payload.
	 */
	if (plen == tlen) {
		aad_start = 0;
		aad_stop = 0;
		iv_offset = 1;
		auth_start = 0;
		auth_stop = 0;
		auth_insert = 0;
		cipher_start = AES_BLOCK_LEN + 1;
		cipher_stop = 0;

		sec_pdu->pldlen = htobe32(16 + plen -
		    (m_tls->m_epg_hdrlen + offset));

		/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
		sec_pdu->seqno_numivs = tlsp->scmd0_short.seqno_numivs;
		sec_pdu->ivgen_hdrlen = htobe32(
		    tlsp->scmd0_short.ivgen_hdrlen |
		    V_SCMD_HDR_LEN(offset == 0 ? m_tls->m_epg_hdrlen : 0));

		txq->kern_tls_short++;
	} else {
		/*
		 * AAD is TLS header.  IV is after AAD.  The cipher region
		 * starts after the IV.  See comments in ccr_authenc() and
		 * ccr_gmac() in t4_crypto.c regarding cipher and auth
		 * start/stop values.
		 */
		aad_start = 1;
		aad_stop = TLS_HEADER_LENGTH;
		iv_offset = TLS_HEADER_LENGTH + 1;
		cipher_start = m_tls->m_epg_hdrlen + 1;
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

		sec_pdu->pldlen = htobe32(plen);

		/* These two flits are actually a CPL_TLS_TX_SCMD_FMT. */
		sec_pdu->seqno_numivs = tlsp->scmd0.seqno_numivs;
		sec_pdu->ivgen_hdrlen = tlsp->scmd0.ivgen_hdrlen;

		if (mtod(m_tls, vm_offset_t) == 0)
			txq->kern_tls_full++;
		else
			txq->kern_tls_partial++;
	}
	sec_pdu->op_ivinsrtofst = htobe32(
	    V_CPL_TX_SEC_PDU_OPCODE(CPL_TX_SEC_PDU) |
	    V_CPL_TX_SEC_PDU_CPLLEN(2) | V_CPL_TX_SEC_PDU_PLACEHOLDER(0) |
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

	sec_pdu->scmd1 = htobe64(m_tls->m_epg_seqno);

	/* Key context */
	out = (void *)(sec_pdu + 1);
	if (tlsp->inline_key) {
		memcpy(out, &tlsp->keyctx, tlsp->tx_key_info_size);
		out += tlsp->tx_key_info_size;
	} else {
		/* ULPTX_SC_MEMRD to read key context. */
		memrd = (void *)out;
		memrd->cmd_to_len = htobe32(V_ULPTX_CMD(ULP_TX_SC_MEMRD) |
		    V_ULP_TX_SC_MORE(1) |
		    V_ULPTX_LEN16(tlsp->tx_key_info_size >> 4));
		memrd->addr = htobe32(tlsp->tx_key_addr >> 5);

		/* ULPTX_IDATA for CPL_TX_DATA and TLS header. */
		idata = (void *)(memrd + 1);
		idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM) |
		    V_ULP_TX_SC_MORE(1));
		idata->len = htobe32(sizeof(struct cpl_tx_data) + imm_len);

		out = (void *)(idata + 1);
	}

	/* CPL_TX_DATA */
	tx_data = (void *)out;
	OPCODE_TID(tx_data) = htonl(MK_OPCODE_TID(CPL_TX_DATA, tlsp->tid));
	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		mss = m->m_pkthdr.tso_segsz;
		tlsp->prev_mss = mss;
	} else if (tlsp->prev_mss != 0)
		mss = tlsp->prev_mss;
	else
		mss = tlsp->vi->ifp->if_mtu -
		    (m->m_pkthdr.l3hlen + m->m_pkthdr.l4hlen);
	if (offset == 0) {
		tx_data->len = htobe32(V_TX_DATA_MSS(mss) | V_TX_LENGTH(tlen));
		tx_data->rsvd = htobe32(tcp_seqno);
	} else {
		tx_data->len = htobe32(V_TX_DATA_MSS(mss) |
		    V_TX_LENGTH(tlen - (m_tls->m_epg_hdrlen + offset)));
		tx_data->rsvd = htobe32(tcp_seqno + m_tls->m_epg_hdrlen + offset);
	}
	tx_data->flags = htobe32(F_TX_BYPASS);
	if (last_wr && tcp->th_flags & TH_PUSH)
		tx_data->flags |= htobe32(F_TX_PUSH | F_TX_SHOVE);

	/* Populate the TLS header */
	out = (void *)(tx_data + 1);
	if (offset == 0) {
		memcpy(out, m_tls->m_epg_hdr, m_tls->m_epg_hdrlen);
		out += m_tls->m_epg_hdrlen;
	}

	/* AES IV for a short record. */
	if (plen == tlen) {
		iv = out;
		if (tlsp->enc_mode == SCMD_CIPH_MODE_AES_GCM) {
			memcpy(iv, tlsp->keyctx.txhdr.txsalt, SALT_SIZE);
			memcpy(iv + 4, hdr + 1, 8);
			*(uint32_t *)(iv + 12) = htobe32(2 +
			    offset / AES_BLOCK_LEN);
		} else
			memcpy(iv, hdr + 1, AES_BLOCK_LEN);
		out += AES_BLOCK_LEN;
	}

	if (imm_len % 16 != 0) {
		/* Zero pad to an 8-byte boundary. */
		memset(out, 0, 8 - (imm_len % 8));
		out += 8 - (imm_len % 8);

		/*
		 * Insert a ULP_TX_SC_NOOP if needed so the SGL is
		 * 16-byte aligned.
		 */
		if (imm_len % 16 <= 8) {
			idata = (void *)out;
			idata->cmd_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
			idata->len = htobe32(0);
			out = (void *)(idata + 1);
		}
	}

	/* SGL for record payload */
	sglist_reset(txq->gl);
	if (sglist_append_mbuf_epg(txq->gl, m_tls, m_tls->m_epg_hdrlen + offset,
	    plen - (m_tls->m_epg_hdrlen + offset)) != 0) {
#ifdef INVARIANTS
		panic("%s: failed to append sglist", __func__);
#endif
	}
	write_gl_to_buf(txq->gl, out);

	if (using_scratch) {
		out = dst;
		copy_to_txd(eq, txq->ss, &out, wr_len);
	}

	ndesc += howmany(wr_len, EQ_ESIZE);
	MPASS(ndesc <= available);

	txq->kern_tls_records++;
	txq->kern_tls_octets += tlen - mtod(m_tls, vm_offset_t);
	if (mtod(m_tls, vm_offset_t) != 0) {
		if (offset == 0)
			txq->kern_tls_waste += mtod(m_tls, vm_offset_t);
		else
			txq->kern_tls_waste += mtod(m_tls, vm_offset_t) -
			    (m_tls->m_epg_hdrlen + offset);
	}

	txsd = &txq->sdesc[pidx];
	if (last_wr)
		txsd->m = m;
	else
		txsd->m = NULL;
	txsd->desc_used = howmany(wr_len, EQ_ESIZE);

	return (ndesc);
}

static int
ktls_write_tcp_fin(struct sge_txq *txq, void *dst, struct mbuf *m,
    u_int available, tcp_seq tcp_seqno, u_int pidx)
{
	struct tx_sdesc *txsd;
	struct fw_eth_tx_pkt_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	uint32_t ctrl;
	uint64_t ctrl1;
	int len16, ndesc, pktlen;
	struct ether_header *eh;
	struct ip *ip, newip;
	struct ip6_hdr *ip6, newip6;
	struct tcphdr *tcp, newtcp;
	caddr_t out;

	TXQ_LOCK_ASSERT_OWNED(txq);
	M_ASSERTPKTHDR(m);

	wr = dst;
	pktlen = m->m_len;
	ctrl = sizeof(struct cpl_tx_pkt_core) + pktlen;
	len16 = howmany(sizeof(struct fw_eth_tx_pkt_wr) + ctrl, 16);
	ndesc = tx_len16_to_desc(len16);
	MPASS(ndesc <= available);

	/* Firmware work request header */
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
	if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
		ip = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip = *ip;
		newip.ip_len = htons(pktlen - m->m_pkthdr.l2hlen);
		copy_to_txd(&txq->eq, (caddr_t)&newip, &out, sizeof(newip));
		if (m->m_pkthdr.l3hlen > sizeof(*ip))
			copy_to_txd(&txq->eq, (caddr_t)(ip + 1), &out,
			    m->m_pkthdr.l3hlen - sizeof(*ip));
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	} else {
		ip6 = (void *)((char *)eh + m->m_pkthdr.l2hlen);
		newip6 = *ip6;
		newip6.ip6_plen = htons(pktlen - m->m_pkthdr.l2hlen);
		copy_to_txd(&txq->eq, (caddr_t)&newip6, &out, sizeof(newip6));
		MPASS(m->m_pkthdr.l3hlen == sizeof(*ip6));
		ctrl1 = V_TXPKT_CSUM_TYPE(TX_CSUM_TCPIP6) |
		    V_T6_TXPKT_ETHHDR_LEN(m->m_pkthdr.l2hlen - ETHER_HDR_LEN) |
		    V_TXPKT_IPHDR_LEN(m->m_pkthdr.l3hlen);
	}
	cpl->ctrl1 = htobe64(ctrl1);
	txq->txcsum++;

	/* Set sequence number in TCP header. */
	tcp = (void *)((char *)eh + m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen);
	newtcp = *tcp;
	newtcp.th_seq = htonl(tcp_seqno);
	copy_to_txd(&txq->eq, (caddr_t)&newtcp, &out, sizeof(newtcp));

	/* Copy rest of packet. */
	copy_to_txd(&txq->eq, (caddr_t)(tcp + 1), &out, m->m_len -
	    (m->m_pkthdr.l2hlen + m->m_pkthdr.l3hlen + sizeof(*tcp)));
	txq->imm_wrs++;

	txq->txpkt_wrs++;

	txq->kern_tls_fin++;

	txsd = &txq->sdesc[pidx];
	txsd->m = m;
	txsd->desc_used = ndesc;

	return (ndesc);
}

int
t6_ktls_write_wr(struct sge_txq *txq, void *dst, struct mbuf *m, u_int nsegs,
    u_int available)
{
	struct sge_eq *eq = &txq->eq;
	struct tx_sdesc *txsd;
	struct tlspcb *tlsp;
	struct tcphdr *tcp;
	struct mbuf *m_tls;
	struct ether_header *eh;
	tcp_seq tcp_seqno;
	u_int ndesc, pidx, totdesc;
	uint16_t vlan_tag;
	bool has_fin, set_l2t_idx;
	void *tsopt;

	M_ASSERTPKTHDR(m);
	MPASS(m->m_pkthdr.snd_tag != NULL);
	tlsp = mst_to_tls(m->m_pkthdr.snd_tag);

	totdesc = 0;
	eh = mtod(m, struct ether_header *);
	tcp = (struct tcphdr *)((char *)eh + m->m_pkthdr.l2hlen +
	    m->m_pkthdr.l3hlen);
	pidx = eq->pidx;
	has_fin = (tcp->th_flags & TH_FIN) != 0;

	/*
	 * If this TLS record has a FIN, then we will send any
	 * requested options as part of the FIN packet.
	 */
	if (!has_fin && ktls_has_tcp_options(tcp)) {
		ndesc = ktls_write_tcp_options(txq, dst, m, available, pidx);
		totdesc += ndesc;
		IDXINCR(pidx, ndesc, eq->sidx);
		dst = &eq->desc[pidx];
#ifdef VERBOSE_TRACES
		CTR2(KTR_CXGBE, "%s: tid %d wrote TCP options packet", __func__,
		    tlsp->tid);
#endif
	}

	/*
	 * Allocate a new L2T entry if necessary.  This may write out
	 * a work request to the txq.
	 */
	if (m->m_flags & M_VLANTAG)
		vlan_tag = m->m_pkthdr.ether_vtag;
	else
		vlan_tag = 0xfff;
	set_l2t_idx = false;
	if (tlsp->l2te == NULL || tlsp->l2te->vlan != vlan_tag ||
	    memcmp(tlsp->l2te->dmac, eh->ether_dhost, ETHER_ADDR_LEN) != 0) {
		set_l2t_idx = true;
		if (tlsp->l2te)
			t4_l2t_release(tlsp->l2te);
		tlsp->l2te = t4_l2t_alloc_tls(tlsp->sc, txq, dst, &ndesc,
		    vlan_tag, tlsp->vi->pi->lport, eh->ether_dhost);
		if (tlsp->l2te == NULL)
			CXGBE_UNIMPLEMENTED("failed to allocate TLS L2TE");
		if (ndesc != 0) {
			MPASS(ndesc <= available - totdesc);

			txq->raw_wrs++;
			txsd = &txq->sdesc[pidx];
			txsd->m = NULL;
			txsd->desc_used = ndesc;
			totdesc += ndesc;
			IDXINCR(pidx, ndesc, eq->sidx);
			dst = &eq->desc[pidx];
		}
	}

	/*
	 * Iterate over each TLS record constructing a work request
	 * for that record.
	 */
	for (m_tls = m->m_next; m_tls != NULL; m_tls = m_tls->m_next) {
		MPASS(m_tls->m_flags & M_EXTPG);

		/*
		 * Determine the initial TCP sequence number for this
		 * record.
		 */
		tsopt = NULL;
		if (m_tls == m->m_next) {
			tcp_seqno = ntohl(tcp->th_seq) -
			    mtod(m_tls, vm_offset_t);
			if (tlsp->using_timestamps)
				tsopt = ktls_find_tcp_timestamps(tcp);
		} else {
			MPASS(mtod(m_tls, vm_offset_t) == 0);
			tcp_seqno = tlsp->prev_seq;
		}

		ndesc = ktls_write_tls_wr(tlsp, txq, dst, m, tcp, m_tls,
		    nsegs, available - totdesc, tcp_seqno, tsopt, pidx,
		    set_l2t_idx);
		totdesc += ndesc;
		IDXINCR(pidx, ndesc, eq->sidx);
		dst = &eq->desc[pidx];

		/*
		 * The value of nsegs from the header mbuf's metadata
		 * is only valid for the first TLS record.
		 */
		nsegs = 0;

		/* Only need to set the L2T index once. */
		set_l2t_idx = false;
	}

	if (has_fin) {
		/*
		 * If the TCP header for this chain has FIN sent, then
		 * explicitly send a packet that has FIN set.  This
		 * will also have PUSH set if requested.  This assumes
		 * we sent at least one TLS record work request and
		 * uses the TCP sequence number after that reqeust as
		 * the sequence number for the FIN packet.
		 */
		ndesc = ktls_write_tcp_fin(txq, dst, m, available,
		    tlsp->prev_seq, pidx);
		totdesc += ndesc;
	}

	MPASS(totdesc <= available);
	return (totdesc);
}

void
cxgbe_tls_tag_free(struct m_snd_tag *mst)
{
	struct adapter *sc;
	struct tlspcb *tlsp;

	tlsp = mst_to_tls(mst);
	sc = tlsp->sc;

	CTR2(KTR_CXGBE, "%s: tid %d", __func__, tlsp->tid);

	if (tlsp->l2te)
		t4_l2t_release(tlsp->l2te);
	if (tlsp->tid >= 0)
		release_tid(sc, tlsp->tid, tlsp->ctrlq);
	if (tlsp->ce)
		t4_release_lip(sc, tlsp->ce);
	if (tlsp->tx_key_addr >= 0)
		free_keyid(tlsp, tlsp->tx_key_addr);

	zfree(tlsp, M_CXGBE);
}

void
t6_ktls_modload(void)
{

	t4_register_shared_cpl_handler(CPL_ACT_OPEN_RPL, ktls_act_open_rpl,
	    CPL_COOKIE_KERN_TLS);
}

void
t6_ktls_modunload(void)
{

	t4_register_shared_cpl_handler(CPL_ACT_OPEN_RPL, NULL,
	    CPL_COOKIE_KERN_TLS);
}

#else

int
cxgbe_tls_tag_alloc(struct ifnet *ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **pt)
{
	return (ENXIO);
}

int
t6_ktls_parse_pkt(struct mbuf *m, int *nsegsp, int *len16p)
{
	return (EINVAL);
}

int
t6_ktls_write_wr(struct sge_txq *txq, void *dst, struct mbuf *m, u_int nsegs,
    u_int available)
{
	panic("can't happen");
}

void
cxgbe_tls_tag_free(struct m_snd_tag *mst)
{
	panic("can't happen");
}

void
t6_ktls_modload(void)
{
}

void
t6_ktls_modunload(void)
{
}

#endif
