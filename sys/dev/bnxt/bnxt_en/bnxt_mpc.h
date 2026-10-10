/*
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BNXT_MPC_H
#define BNXT_MPC_H

#define BNXT_MPC_TCE_TYPE HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TCE
#define BNXT_MPC_RCE_TYPE HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RCE
#define BNXT_MPC_TE_CFA_TYPE HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA
#define BNXT_MPC_RE_CFA_TYPE HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA
#define BNXT_MPC_TYPE_MAX (BNXT_MPC_RE_CFA_TYPE + 1)

#define BNXT_MAX_MPC 8

#define BNXT_MIN_MPC_TCE 1
#define BNXT_MIN_MPC_RCE 1
#define BNXT_DFLT_MPC_TCE BNXT_MAX_MPC
#define BNXT_DFLT_MPC_RCE BNXT_MAX_MPC

#define BNXT_MIN_MPC_TE_CFA 1
#define BNXT_MIN_MPC_RE_CFA 1
#define BNXT_DFLT_MPC_TE_CFA BNXT_MAX_MPC
#define BNXT_DFLT_MPC_RE_CFA BNXT_MAX_MPC

#define BNXT_MPC_TMO_USECS (1000 * 1000)

struct bnxt_mpc_info {
	uint8_t mpc_chnls_cap;
	uint8_t mpc_cp_rings_count;
	uint8_t mpc_ring_count[BNXT_MPC_TYPE_MAX];
	struct bnxt_ring *mpc_rings[BNXT_MPC_TYPE_MAX];
	struct bnxt_cp_ring *mpc_cp_rings;
	struct bnxt_cp_ring *mpc_nq_rings;
	struct iflib_dma_info   tx_stats[BNXT_MAX_NUM_QUEUES];
};

#define BNXT_RING_MPC_IDX_TYPE_MASK 0xf0
#define BNXT_RING_MPC_IDX_TYPE_SFT 4
#define BNXT_RING_MPC_IDX_RING_MASK 0x0f

enum bnxt_mpc_type {
	BNXT_MPC_CRYPTO,
	BNXT_MPC_CFA,
};

#define BNXT_MPC_CRYPTO_CAP                                                    \
	(HWRM_FUNC_QCAPS_OUTPUT_MPC_CHNLS_CAP_TCE |                            \
	 HWRM_FUNC_QCAPS_OUTPUT_MPC_CHNLS_CAP_RCE)

#define BNXT_MPC_CRYPTO_CAPABLE(softc)                                         \
	((softc)->mpc_info ? ((softc)->mpc_info->mpc_chnls_cap &               \
			      BNXT_MPC_CRYPTO_CAP) == BNXT_MPC_CRYPTO_CAP      \
			   : false)

#define BNXT_MPC_CFA_CAP                                                       \
	(HWRM_FUNC_QCAPS_OUTPUT_MPC_CHNLS_CAP_TE_CFA |                         \
	 HWRM_FUNC_QCAPS_OUTPUT_MPC_CHNLS_CAP_RE_CFA)

#define BNXT_MPC_CFA_CAPABLE(bp)                                               \
	((bp)->mpc_info ? ((bp)->mpc_info->mpc_chnls_cap &                     \
			   BNXT_MPC_CFA_CAP) == BNXT_MPC_CFA_CAP               \
			: false)

#define MPC_CMP_RING_MASK(cpr) ((cpr)->ring.ring_size - 1)
#define MPC_RING_CMP(cpr, idx) ((idx) & MPC_CMP_RING_MASK(cpr))

#define RING_TX(txr, idx) ((idx) & TX_RING_MASK(txr))
#define NEXT_TX(txr, idx) ((idx + 1) & TX_RING_MASK(txr))
#define ADVANCE_TX_BY(txr, idx, count) ((idx + count) & TX_RING_MASK(txr))

#define TX_OPAQUE_IDX_MASK 0x0000ffff
#define TX_OPAQUE_BDS_MASK 0x00ff0000
#define TX_OPAQUE_BDS_SHIFT 16
#define TX_OPAQUE_RING_MASK 0xff000000
#define TX_OPAQUE_RING_SHIFT 24

#define SET_TX_OPAQUE(txr, prod, bds)                                          \
	(((txr)->idx << TX_OPAQUE_RING_SHIFT) |                                \
	 ((bds) << TX_OPAQUE_BDS_SHIFT) |                                      \
	 (((prod) & TX_RING_MASK(txr)) & TX_OPAQUE_IDX_MASK))

#define TX_OPAQUE_IDX(opq) ((opq) & TX_OPAQUE_IDX_MASK)
#define TX_OPAQUE_RING(opq)                                                    \
	(((opq) & TX_OPAQUE_RING_MASK) >> TX_OPAQUE_RING_SHIFT)
#define TX_OPAQUE_BDS(opq) (((opq) & TX_OPAQUE_BDS_MASK) >> TX_OPAQUE_BDS_SHIFT)

#define BNXT_INV_MPC_HDL (-1UL)

struct bnxt_cmpl_entry {
	void *cmpl;
	uint32_t len;
};

struct mpc_cmp {
	__le32 mpc_cmp_client_subtype_type;
#define MPC_CMP_TYPE (0x3f << 0)
#define MPC_CMP_TYPE_MID_PATH_SHORT 0x1e
#define MPC_CMP_TYPE_MID_PATH_LONG 0x1f

#define MPC_CMP_SUBTYPE 0xf00
#define MPC_CMP_SUBTYPE_SFT 8
#define MPC_CMP_SUBTYPE_SOLICITED (0x0 << 8)
#define MPC_CMP_SUBTYPE_ERR (0x1 << 8)
#define MPC_CMP_SUBTYPE_RESYNC (0x2 << 8)

#define MPC_CMP_CLIENT (0xf << 12)
#define MPC_CMP_CLIENT_SFT 12
#define MPC_CMP_CLIENT_TCE (0x0 << 12)
#define MPC_CMP_CLIENT_RCE (0x1 << 12)
#define MPC_CMP_CLIENT_TE_CFA (0x2 << 12)
#define MPC_CMP_CLIENT_RE_CFA (0x3 << 12)

	uint32_t mpc_cmp_opaque;
	__le32 mpc_cmp_v;
#define MPC_CMP_V (1 << 0)
	__le32 mpc_cmp_filler;
};

#define MPC_CMP_CMP_TYPE(mpcmp)                                                \
	(le32toh((mpcmp)->mpc_cmp_client_subtype_type) & MPC_CMP_TYPE)

#define MPC_CMP_CLIENT_TYPE(mpcmp)                                             \
	(le32toh((mpcmp)->mpc_cmp_client_subtype_type) & MPC_CMP_CLIENT)

/*
 * ERR and RESYNC are both unsolicited (firmware-initiated, no matching TX
 * descriptor); only SOLICITED completions carry a valid TX opaque to parse.
 */
#define MPC_CMP_UNSOLICIT_SUBTYPE(mpcmp)                                       \
	(((le32toh((mpcmp)->mpc_cmp_client_subtype_type) &                     \
	   MPC_CMP_SUBTYPE) == MPC_CMP_SUBTYPE_ERR) ||                         \
	 ((le32toh((mpcmp)->mpc_cmp_client_subtype_type) &                     \
	   MPC_CMP_SUBTYPE) == MPC_CMP_SUBTYPE_RESYNC))

#define MPC_CMP_VALID(cpr, mpcmp, raw_cons)                                    \
	(!!((mpcmp)->mpc_cmp_v & htole32(MPC_CMP_V)) ==                    \
	 !((raw_cons) & (cpr)->ring.ring_size))

void bnxt_alloc_mpc_info(struct bnxt_softc *softc, uint8_t mpc_chnls_cap);
void bnxt_free_mpc_info(struct bnxt_softc *softc);
void bnxt_set_dflt_mpc_rings(struct bnxt_softc *softc);
int bnxt_alloc_mpcs(struct bnxt_softc *softc);
void bnxt_free_mpcs(struct bnxt_softc *softc);
int bnxt_alloc_mpc_rings(struct bnxt_softc *softc);
void bnxt_free_mpc_rings(struct bnxt_softc *softc);
int bnxt_mpc_irq_setup(struct bnxt_softc *softc);
void bnxt_mpc_irq_cleanup(struct bnxt_softc *softc);
int bnxt_hwrm_mpc_ring_alloc(struct bnxt_softc *softc);
int bnxt_hwrm_mpc_ring_free(struct bnxt_softc *softc);
int bnxt_start_xmit_mpc(struct bnxt_softc *softc, struct bnxt_ring *txr,
			void *data, uint len, unsigned long handle);
int bnxt_mpc_cmp(struct bnxt_softc *softc, struct bnxt_cp_ring *cpr,
		 uint32_t *raw_cons);
void bnxt_clear_mpc_rings_ids(struct bnxt_softc *softc);
struct bnxt_ring *bnxt_select_mpc_ring(struct bnxt_mpc_info *mpc);

#endif
