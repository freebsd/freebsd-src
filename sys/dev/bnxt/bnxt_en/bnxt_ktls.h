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

#ifndef BNXT_KTLS_H
#define BNXT_KTLS_H

#include <sys/ktls.h>
#include <sys/queue.h>
#include <sys/bitstring.h>
#include <sys/condvar.h>
#include <sys/taskqueue.h>
#include <machine/atomic.h>

#include "bnxt.h"
#include "bnxt_mpc.h"

#define BNXT_TX_CRYPTO_KEY_TYPE HWRM_FUNC_KEY_CTX_ALLOC_INPUT_KEY_CTX_TYPE_TX
#define BNXT_RX_CRYPTO_KEY_TYPE HWRM_FUNC_KEY_CTX_ALLOC_INPUT_KEY_CTX_TYPE_RX

#define BNXT_PARTITION_CAP_BITS                                                \
	(HWRM_FUNC_QCAPS_OUTPUT_XID_PARTITION_CAP_TX_CK |                      \
	 HWRM_FUNC_QCAPS_OUTPUT_XID_PARTITION_CAP_RX_CK)

#define BNXT_PARTITION_CAP(resp)                                               \
	((le32toh((resp)->flags_ext2) &                                    \
	  HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_KEY_XID_PARTITION_SUPPORTED) &&    \
	 ((le16toh((resp)->xid_partition_cap) &                            \
	   BNXT_PARTITION_CAP_BITS) == BNXT_PARTITION_CAP_BITS))

#define BNXT_KID_BATCH_SIZE 128

#define	BNXT_KTLS_ENTRIES_DEFAULT	102400
#define	BNXT_KTLS_MAX_ENTRIES		512000

struct bnxt_kid_info {
	TAILQ_ENTRY(bnxt_kid_info) list;
	uint32_t start_id;
	uint32_t count;
	bitstr_t *ids;
};

struct bnxt_kctx {
	TAILQ_HEAD(, bnxt_kid_info) list;
	/* to serialize update to the linked list and total_alloc */
	struct mtx lock;
	struct cv alloc_pending_cv;
	uint8_t type;
	uint32_t total_alloc;
	uint32_t max_ctx;
	volatile uint32_t alloc_pending;
#define BNXT_KCTX_ALLOC_PENDING_MAX 8
	bitstr_t *partition_bmap;
	int partition_bmap_nbits;	/* number of bits in partition_bmap */
	unsigned int next;
};

#define BNXT_KCTX_ALLOC_OK(kctx)                                               \
	(atomic_load_32(&((kctx)->alloc_pending)) < BNXT_KCTX_ALLOC_PENDING_MAX)

#define BNXT_MAX_CRYPTO_KEY_TYPE (BNXT_RX_CRYPTO_KEY_TYPE + 1)

enum bnxt_ktls_counters {
	BNXT_KTLS_TX_ADD = 0,
	BNXT_KTLS_TX_DEL,
	BNXT_KTLS_TX_HW_PKT,
	BNXT_KTLS_TX_FAILED,
	BNXT_KTLS_TX_OOO,
	BNXT_KTLS_TX_RETRANS,
	BNXT_KTLS_TX_REPLAY,
	BNXT_KTLS_TX_SEQ_FWD,
	BNXT_KTLS_TX_SEQ_FWD_TLS_HDR,
	BNXT_KTLS_TX_SEQ_FWD_SILENT_DROPS,
	BNXT_KTLS_TX_SEQ_FWD_REPLAY,
	BNXT_KTLS_TX_MBUF_ALLOCS,
	BNXT_KTLS_TX_MBUF_FREES,
	BNXT_KTLS_TX_MBUF_ERRORS,
	BNXT_KTLS_RX_ADD,
	BNXT_KTLS_RX_DEL,
	BNXT_KTLS_RX_HW_PKT,
	BNXT_KTLS_RX_SW_PKT,
	BNXT_KTLS_RX_RESYNC_REQ,
	BNXT_KTLS_RX_RESYNC_ACK,
	BNXT_KTLS_RX_RESYNC_DISCARD,
	BNXT_KTLS_RX_RESYNC_NAK,
	BNXT_KTLS_MAX_COUNTERS,
};

enum bnxt_mpc_cmp_time_counters {
	BNXT_MPC_CMP_TIME_1US = 0,
	BNXT_MPC_CMP_TIME_1_5US,
	BNXT_MPC_CMP_TIME_5_10US,
	BNXT_MPC_CMP_TIME_10_15US,
	BNXT_MPC_CMP_TIME_15_20US,
	BNXT_MPC_CMP_TIME_20_25US,
	BNXT_MPC_CMP_TIME_25_50US,
	BNXT_MPC_CMP_TIME_50_100US,
	BNXT_MPC_CMP_TIME_100_200US,
	BNXT_MPC_CMP_TIME_200_500US,
	BNXT_MPC_CMP_TIME_500_1000US,
	BNXT_MPC_CMP_TIME_1000_1500US,
	BNXT_MPC_CMP_TIME_1500_2000US,
	BNXT_MPC_CMP_TIME_2000_2500US,
	BNXT_MPC_CMP_TIME_2500_3000US,
	BNXT_MPC_CMP_TIME_3000_3500US,
	BNXT_MPC_CMP_TIME_3500_4000US,
	BNXT_MPC_CMP_TIME_4000_4500US,
	BNXT_MPC_CMP_TIME_4500_5000US,
	BNXT_MPC_CMP_TIME_5000_PLUS_US,
	BNXT_MPC_CMP_TIME_MAX_COUNTERS,
};

enum bnxt_ktls_err_counters {
	BNXT_KTLS_TX_DEV_ADD_FAILED = 0,
	BNXT_KTLS_TX_MPC_TIMEOUT,
	BNXT_KTLS_TX_UNINITIALIZED,
	BNXT_KTLS_TX_CTX_ALLOC_FAILED,
	BNXT_KTLS_TX_MPC_FAILED,
	BNXT_KTLS_TX_TCE_NOT_READY,
	BNXT_KTLS_TX_TCE_READY_DELAYED,
	BNXT_KTLS_TX_TCE_BAD,
	BNXT_KTLS_TX_UNSUPPORTED_TLS_HDR,
	BNXT_KTLS_TX_INVALID_TAG,
	BNXT_KTLS_TX_ZERO_TCP_PAYLEN,
	BNXT_KTLS_TX_INVALID_REC_SN,
	BNXT_KTLS_TX_TCE_FREE,
	BNXT_KTLS_TX_MPC_RING_BUSY,
	BNXT_KTLS_ERR_MAX_COUNTERS,
};

struct bnxt_tls_info
{
	uint16_t max_key_ctxs_alloc;
	uint16_t ctxs_per_partition;
	uint8_t partition_mode : 1;
	/* Bumped by bnxt_clear_ktls(); lets bnxt_ktls_xmit() detect a tag
	 * whose firmware KID was erased by a reset since it went READY. */
	volatile uint32_t reset_gen;

	struct bnxt_kctx kctx[BNXT_MAX_CRYPTO_KEY_TYPE];

	uma_zone_t mpc_zone;

	volatile uint32_t pending;
	volatile uint32_t snd_tag_alloc_ref;	/* threads inside bnxt_tls_snd_tag_alloc() */

#define BNXT_MAX_TLS_FILTER 460

	bus_dma_tag_t		dma_tag;

	struct taskqueue *tq;
	uma_zone_t zone;
	uint32_t max_resources;			/* max number of resources */
	volatile uint32_t num_resources;	/* current number of resources */
	int init;				/* set when ready */
	char zname[32];

	counter_u64_t counters[BNXT_KTLS_MAX_COUNTERS];
	counter_u64_t err_counters[BNXT_KTLS_ERR_MAX_COUNTERS];

	counter_u64_t mpc_cmp_time[BNXT_MPC_CMP_TIME_MAX_COUNTERS];
};

/*
 * Function-like macros, not object-like #defines, to avoid mangling any
 * unrelated `tck`/`rck` local variable wherever this header is included.
 */
#define BNXT_TCK(t) ((t)->kctx[BNXT_TX_CRYPTO_KEY_TYPE])
#define BNXT_RCK(t) ((t)->kctx[BNXT_RX_CRYPTO_KEY_TYPE])

#define BNXT_TLS_TAG_LOCK(tag) mtx_lock(&(tag)->lock)
#define BNXT_TLS_TAG_UNLOCK(tag) mtx_unlock(&(tag)->lock)

struct bnxt_replay_pkt {
	bus_dmamap_t dma_map;
	struct mbuf *mbuf;
};

struct bnxt_ktls_offload_ctx_tx
{
	struct m_snd_tag tag;
	uint32_t tcp_seq_no;
	uint64_t rec_seq;
	uint32_t kid;
	uint32_t gen;	/* ktls->reset_gen as of last successful dev_add */
	struct mtx lock;
	uint8_t state;
#define BNXT_TLS_STATE_INIT 0
#define BNXT_TLS_STATE_READY 1
#define BNXT_TLS_STATE_BAD  2
#define BNXT_TLS_STATE_FREE 3
#define BNXT_TLS_STATE_FREED 4
	struct bnxt_tls_info	*ktls;
	uint64_t inorder_rec_sn;
	uint32_t inorder_paylen;
	uint32_t inorder_seq;
	struct tls_session_params crypto_info;
	int enobufs;
#define MAX_MPC_RETRY 3
	int mpc_retry;
	struct timeval start;
	struct timeval end;
	struct task work_task;
	/*
	 * In-flight MPC commands holding a raw kctx_tx pointer; must stay
	 * zero before the tag can be freed, or a completion use-after-frees.
	 */
	uint32_t mpc_inflight;
};

/* Resync timeout: 2500 ms in ticks */
#define BNXT_KTLS_RESYNC_TMO ((2500 * hz) / 1000)
#define BNXT_KTLS_MAX_RESYNC_BYTES 32768

#define BNXT_KTLS_MAX_REPLAY_MSS 9000

struct ce_add_cmd
{
	uint32_t ver_algo_kid_opcode;
#define CE_ADD_CMD_OPCODE_MASK 0xfUL
#define CE_ADD_CMD_OPCODE_SFT 0
#define CE_ADD_CMD_OPCODE_ADD 0x1UL
#define CE_ADD_CMD_KID_MASK 0xfffff0UL
#define CE_ADD_CMD_KID_SFT 4
#define CE_ADD_CMD_ALGORITHM_MASK 0xf000000UL
#define CE_ADD_CMD_ALGORITHM_SFT 24
#define CE_ADD_CMD_ALGORITHM_AES_GCM_128 0x1000000UL
#define CE_ADD_CMD_ALGORITHM_AES_GCM_256 0x2000000UL
#define CE_ADD_CMD_VERSION_MASK 0xf0000000UL
#define CE_ADD_CMD_VERSION_SFT 28
#define CE_ADD_CMD_VERSION_TLS1_2 (0x0UL << 28)
#define CE_ADD_CMD_VERSION_TLS1_3 (0x1UL << 28)
	uint8_t ctx_kind;
#define CE_ADD_CMD_CTX_KIND_MASK 0x1fUL
#define CE_ADD_CMD_CTX_KIND_SFT 0
#define CE_ADD_CMD_CTX_KIND_CK_TX 0x11UL
#define CE_ADD_CMD_CTX_KIND_CK_RX 0x12UL
	uint8_t unused0[3];
	uint8_t salt[4];
	uint8_t unused1[4];
	uint32_t pkt_tcp_seq_num;
	uint32_t tls_header_tcp_seq_num;
	uint8_t record_seq_num[8];
	uint8_t session_key[32];
	uint8_t addl_iv[8];
};

struct ce_delete_cmd
{
	uint32_t ctx_kind_kid_opcode;
#define CE_DELETE_CMD_OPCODE_MASK 0xfUL
#define CE_DELETE_CMD_OPCODE_SFT 0
#define CE_DELETE_CMD_OPCODE_DEL 0x2UL
#define CE_DELETE_CMD_KID_MASK 0xfffff0UL
#define CE_DELETE_CMD_KID_SFT 4
#define CE_DELETE_CMD_CTX_KIND_MASK 0x1f000000UL
#define CE_DELETE_CMD_CTX_KIND_SFT 24
#define CE_DELETE_CMD_CTX_KIND_CK_TX (0x11UL << 24)
#define CE_DELETE_CMD_CTX_KIND_CK_RX (0x12UL << 24)
};

struct ce_resync_resp_ack_cmd
{
	uint32_t resync_status_kid_opcode;
#define CE_RESYNC_RESP_ACK_CMD_OPCODE_MASK 0xfUL
#define CE_RESYNC_RESP_ACK_CMD_OPCODE_SFT 0
#define CE_RESYNC_RESP_ACK_CMD_OPCODE_RESYNC 0x3UL
#define CE_RESYNC_RESP_ACK_CMD_KID_MASK 0xfffff0UL
#define CE_RESYNC_RESP_ACK_CMD_KID_SFT 4
#define CE_RESYNC_RESP_ACK_CMD_RESYNC_STATUS 0x1000000UL
#define CE_RESYNC_RESP_ACK_CMD_RESYNC_STATUS_ACK (0x0UL << 24)
#define CE_RESYNC_RESP_ACK_CMD_RESYNC_STATUS_NAK (0x1UL << 24)
	uint32_t resync_record_tcp_seq_num;
	uint8_t resync_record_seq_num[8];
};

#define resync_record_seq_num_end resync_record_seq_num[7]

#define CE_CMD_KID_MASK 0xfffff0UL
#define CE_CMD_KID_SFT 4

#define CE_CMD_KID(cmd_p)                                                      \
	((*(uint32_t*)(cmd_p)&CE_CMD_KID_MASK) >> CE_CMD_KID_SFT)

#define BNXT_KMPC_OPAQUE(client, kid) (((client) << 24) | (kid))

#define BNXT_INV_KMPC_OPAQUE 0xffffffff

struct ce_cmpl
{
	uint16_t client_subtype_type;
#define CE_CMPL_TYPE_MASK 0x3fUL
#define CE_CMPL_TYPE_SFT 0
#define CE_CMPL_TYPE_MID_PATH_SHORT 0x1eUL
#define CE_CMPL_SUBTYPE_MASK 0xf00UL
#define CE_CMPL_SUBTYPE_SFT 8
#define CE_CMPL_SUBTYPE_SOLICITED (0x0UL << 8)
#define CE_CMPL_SUBTYPE_ERR (0x1UL << 8)
#define CE_CMPL_SUBTYPE_RESYNC (0x2UL << 8)
#define CE_CMPL_MP_CLIENT_MASK 0xf000UL
#define CE_CMPL_MP_CLIENT_SFT 12
#define CE_CMPL_MP_CLIENT_TCE (0x0UL << 12)
#define CE_CMPL_MP_CLIENT_RCE (0x1UL << 12)
	uint16_t status;
#define CE_CMPL_STATUS_MASK 0xfUL
#define CE_CMPL_STATUS_SFT 0
#define CE_CMPL_STATUS_OK 0x0UL
#define CE_CMPL_STATUS_CTX_LD_ERR 0x1UL
#define CE_CMPL_STATUS_FID_CHK_ERR 0x2UL
#define CE_CMPL_STATUS_CTX_VER_ERR 0x3UL
#define CE_CMPL_STATUS_DST_ID_ERR 0x4UL
#define CE_CMPL_STATUS_MP_CMD_ERR 0x5UL
	uint32_t opaque;
	uint32_t v;
#define CE_CMPL_V 0x1UL
	uint32_t kid;
#define CE_CMPL_KID_MASK 0xfffffUL
#define CE_CMPL_KID_SFT 0
};

#define CE_CMPL_STATUS(ce_cmpl)                                                \
	(le16toh((ce_cmpl)->status) & CE_CMPL_STATUS_MASK)

#define CE_CMPL_KID(ce_cmpl) (le32toh((ce_cmpl)->kid) & CE_CMPL_KID_MASK)

#define BNXT_SALT_LEN			4
#define BNXT_ADDL_IV_LEN_TLS1_3		8
#define BNXT_EXPLICIT_NONCE_LEN		8

struct crypto_prefix_cmd
{
	uint32_t flags;
#define CRYPTO_PREFIX_CMD_FLAGS_UPDATE_IN_ORDER_VAR 0x1UL
#define CRYPTO_PREFIX_CMD_FLAGS_FULL_REPLAY_RETRAN 0x2UL
	uint32_t header_tcp_seq_num;
	uint32_t start_tcp_seq_num;
	uint32_t end_tcp_seq_num;
	uint8_t explicit_nonce[8];
	uint8_t record_seq_num[8];
};

#define CRYPTO_PREFIX_CMD_SIZE ((uint32_t)sizeof(struct crypto_prefix_cmd))
#define CRYPTO_PREFIX_CMD_BDS (CRYPTO_PREFIX_CMD_SIZE / sizeof(struct tx_bd))
#define CRYPTO_PRESYNC_BDS (CRYPTO_PREFIX_CMD_BDS + 1)

#define CRYPTO_PRESYNC_BD_CMD                                                  \
	(htole32((CRYPTO_PREFIX_CMD_SIZE << TX_BD_LEN_SHIFT) |             \
		     (CRYPTO_PRESYNC_BDS << TX_BD_FLAGS_BD_CNT_SHIFT) |        \
		     TX_BD_TYPE_PRESYNC_TX_BD))

#define BNXT_METADATA_OFF(len) ALIGN(len, 32)

struct bnxt_crypto_cmd_ctx
{
	struct ce_cmpl ce_cmp;
	struct bnxt_ktls_offload_ctx_tx *kctx_tx;
};

static inline bool before(uint32_t seq1, uint32_t seq2)
{
	return (int32_t)(seq1 - seq2) < 0;
}
#define after(seq2, seq1)       before(seq1, seq2)

static inline bool bnxt_ktls_busy(struct bnxt_softc* bp)
{
	return bp->ktls_info && atomic_load_32(&bp->ktls_info->pending) > 0;
}

void bnxt_alloc_ktls_info(struct bnxt_softc* bp, uint16_t max_keys,
			  bool partition_cap, uint16_t ctxs_per_partition);
void bnxt_free_ktls_info(struct bnxt_softc* bp);
void bnxt_free_ktls_counters(struct bnxt_softc* softc);
void bnxt_alloc_ktls_counters(struct bnxt_softc* softc);
void bnxt_hwrm_reserve_pf_key_ctxs(struct bnxt_softc* bp,
				   struct hwrm_func_cfg_input* req,
				   uint8_t type);
int bnxt_ktls_init(struct bnxt_softc* bp);
int bnxt_xmit_crypto_cmd(struct bnxt_softc* softc,
			 void* cmd, uint32_t len,
			 struct bnxt_ktls_offload_ctx_tx* kctx_tx);
int bnxt_key_ctx_alloc_one(struct bnxt_softc* bp, struct bnxt_kctx* kctx,
			   uint32_t* id, uint8_t type);
void bnxt_free_one_kctx(struct bnxt_kctx* kctx, uint32_t id, device_t dev);
int bnxt_set_partition_mode(struct bnxt_softc* bp);
int bnxt_hwrm_key_ctx_alloc(struct bnxt_softc* bp, struct bnxt_kctx* kctx,
			    uint16_t num, uint32_t* id, uint8_t type);
void bnxt_ktls_mpc_cmp(struct bnxt_softc* bp, uint32_t client,
		       unsigned long handle, struct bnxt_cmpl_entry cmpl[],
		       uint32_t entries);
void bnxt_ktls_del_all(struct bnxt_softc *bp);
void bnxt_clear_ktls(struct bnxt_softc *bp);
int
bnxt_alloc_ktls_rexmit_mbuf(struct bnxt_softc *softc, struct bnxt_ring *txr);
void
bnxt_free_ktls_rexmit_mbuf(struct bnxt_ring *txr);
#endif
