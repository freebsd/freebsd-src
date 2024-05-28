/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: statistics related data structures
 */

#ifndef __STATS_H__
#define __STATS_H__

#define BNXT_RE_CFA_STAT_BYTES_MASK 0xFFFFFFFFF
#define BNXT_RE_CFA_STAT_PKTS_MASK 0xFFFFFFF
enum {
	BYTE_MASK = 0,
	PKTS_MASK = 1
};

struct bnxt_re_cnp_counters {
	u64	cnp_tx_pkts;
	u64	cnp_tx_bytes;
	u64	cnp_rx_pkts;
	u64	cnp_rx_bytes;
	u64	ecn_marked;
};

struct bnxt_re_ro_counters {
	u64	tx_pkts;
	u64	tx_bytes;
	u64	rx_pkts;
	u64	rx_bytes;
};

struct bnxt_re_flow_counters {
	struct bnxt_re_ro_counters ro_stats;
	struct bnxt_re_cnp_counters cnp_stats;
};

struct bnxt_re_ext_cntr {
	u64	atomic_req;
	u64	read_req;
	u64	read_resp;
	u64	write_req;
	u64	send_req;
};

struct bnxt_re_ext_good {
	u64	rx_pkts;
	u64	rx_bytes;
};

struct bnxt_re_ext_rstat {
	struct bnxt_re_ext_cntr tx;
	struct bnxt_re_ext_cntr rx;
	struct bnxt_re_ext_good	grx;
	u64  rx_dcn_payload_cut;
	u64  te_bypassed;
};

struct bnxt_re_rdata_counters {
	u64  tx_ucast_pkts;
	u64  tx_mcast_pkts;
	u64  tx_bcast_pkts;
	u64  tx_discard_pkts;
	u64  tx_error_pkts;
	u64  tx_ucast_bytes;
	u64  tx_mcast_bytes;
	u64  tx_bcast_bytes;
	u64  rx_ucast_pkts;
	u64  rx_mcast_pkts;
	u64  rx_bcast_pkts;
	u64  rx_discard_pkts;
	u64  rx_error_pkts;
	u64  rx_ucast_bytes;
	u64  rx_mcast_bytes;
	u64  rx_bcast_bytes;
	u64  rx_agg_pkts;
	u64  rx_agg_bytes;
	u64  rx_agg_events;
	u64  rx_agg_aborts;
};

struct bnxt_re_cc_stat {
	struct bnxt_re_cnp_counters prev[2];
	struct bnxt_re_cnp_counters cur[2];
	bool is_first;
};

struct bnxt_re_ext_roce_stats {
	u64	oob;
	u64	oos;
	u64	seq_err_naks_rcvd;
	u64	rnr_naks_rcvd;
	u64	missing_resp;
	u64	to_retransmits;
	u64	dup_req;
};

struct bnxt_re_rstat {
	struct bnxt_re_ro_counters	prev[2];
	struct bnxt_re_ro_counters	cur[2];
	struct bnxt_re_rdata_counters	rstat[2];
	struct bnxt_re_ext_rstat	ext_rstat[2];
	struct bnxt_re_ext_roce_stats	e_errs;
	struct bnxt_qplib_roce_stats	errs;
	unsigned long long		prev_oob;
};

struct bnxt_re_res_cntrs {
	atomic_t qp_count;
	atomic_t rc_qp_count;
	atomic_t ud_qp_count;
	atomic_t cq_count;
	atomic_t srq_count;
	atomic_t mr_count;
	atomic_t mw_count;
	atomic_t ah_count;
	atomic_t pd_count;
	atomic_t resize_count;
	atomic_t max_qp_count;
	atomic_t max_rc_qp_count;
	atomic_t max_ud_qp_count;
	atomic_t max_cq_count;
	atomic_t max_srq_count;
	atomic_t max_mr_count;
	atomic_t max_mw_count;
	atomic_t max_ah_count;
	atomic_t max_pd_count;
};

struct bnxt_re_device_stats {
	struct bnxt_re_rstat            dstat;
	struct bnxt_re_res_cntrs        rsors;
	struct bnxt_re_cc_stat          cnps;
	unsigned long                   read_tstamp;
	/* To be used in case to disable stats query from worker or change
	 * query interval. 0 means stats_query disabled.
	 */
	u32				stats_query_sec;
	/* A free running counter to be used along with stats_query_sec to
	 * decide whether to issue the command to FW.
	 */
	u32				stats_query_counter;
};

static inline u64 bnxt_re_get_cfa_stat_mask(struct bnxt_qplib_chip_ctx *cctx,
					    bool type)
{
	u64 mask;

	if (type == BYTE_MASK) {
		mask = BNXT_RE_CFA_STAT_BYTES_MASK; /* 36 bits */
		if (_is_chip_gen_p5_p7(cctx))
			mask >>= 0x01; /* 35 bits */
	} else {
		mask = BNXT_RE_CFA_STAT_PKTS_MASK; /* 28 bits */
		if (_is_chip_gen_p5_p7(cctx))
			mask |= (0x10000000ULL); /* 29 bits */
	}

	return mask;
}

static inline u64 bnxt_re_stat_diff(u64 cur, u64 *prev, u64 mask)
{
	u64 diff;

	if (!cur)
		return 0;
	diff = (cur - *prev) & mask;
	if (diff)
		*prev = cur;
	return diff;
}

static inline void bnxt_re_clear_rsors_stat(struct bnxt_re_res_cntrs *rsors)
{
	atomic_set(&rsors->qp_count, 0);
	atomic_set(&rsors->cq_count, 0);
	atomic_set(&rsors->srq_count, 0);
	atomic_set(&rsors->mr_count, 0);
	atomic_set(&rsors->mw_count, 0);
	atomic_set(&rsors->ah_count, 0);
	atomic_set(&rsors->pd_count, 0);
	atomic_set(&rsors->resize_count, 0);
	atomic_set(&rsors->max_qp_count, 0);
	atomic_set(&rsors->max_cq_count, 0);
	atomic_set(&rsors->max_srq_count, 0);
	atomic_set(&rsors->max_mr_count, 0);
	atomic_set(&rsors->max_mw_count, 0);
	atomic_set(&rsors->max_ah_count, 0);
	atomic_set(&rsors->max_pd_count, 0);
	atomic_set(&rsors->max_rc_qp_count, 0);
	atomic_set(&rsors->max_ud_qp_count, 0);
}

enum bnxt_re_hw_stats {
	BNXT_RE_LINK_STATE,
	BNXT_RE_MAX_QP,
	BNXT_RE_MAX_SRQ,
	BNXT_RE_MAX_CQ,
	BNXT_RE_MAX_MR,
	BNXT_RE_MAX_MW,
	BNXT_RE_MAX_AH,
	BNXT_RE_MAX_PD,
	BNXT_RE_ACTIVE_QP,
	BNXT_RE_ACTIVE_RC_QP,
	BNXT_RE_ACTIVE_UD_QP,
	BNXT_RE_ACTIVE_SRQ,
	BNXT_RE_ACTIVE_CQ,
	BNXT_RE_ACTIVE_MR,
	BNXT_RE_ACTIVE_MW,
	BNXT_RE_ACTIVE_AH,
	BNXT_RE_ACTIVE_PD,
	BNXT_RE_QP_WATERMARK,
	BNXT_RE_RC_QP_WATERMARK,
	BNXT_RE_UD_QP_WATERMARK,
	BNXT_RE_SRQ_WATERMARK,
	BNXT_RE_CQ_WATERMARK,
	BNXT_RE_MR_WATERMARK,
	BNXT_RE_MW_WATERMARK,
	BNXT_RE_AH_WATERMARK,
	BNXT_RE_PD_WATERMARK,
	BNXT_RE_RESIZE_CQ_COUNT,
	BNXT_RE_HW_RETRANSMISSION,
	BNXT_RE_RECOVERABLE_ERRORS,
	BNXT_RE_RX_PKTS,
	BNXT_RE_RX_BYTES,
	BNXT_RE_TX_PKTS,
	BNXT_RE_TX_BYTES,
	BNXT_RE_CNP_TX_PKTS,
	BNXT_RE_CNP_TX_BYTES,
	BNXT_RE_CNP_RX_PKTS,
	BNXT_RE_CNP_RX_BYTES,
	BNXT_RE_ROCE_ONLY_RX_PKTS,
	BNXT_RE_ROCE_ONLY_RX_BYTES,
	BNXT_RE_ROCE_ONLY_TX_PKTS,
	BNXT_RE_ROCE_ONLY_TX_BYTES,
	BNXT_RE_RX_ROCE_ERROR_PKTS,
	BNXT_RE_RX_ROCE_DISCARD_PKTS,
	BNXT_RE_TX_ROCE_ERROR_PKTS,
	BNXT_RE_TX_ROCE_DISCARDS_PKTS,
	BNXT_RE_RES_OOB_DROP_COUNT,
	BNXT_RE_TX_ATOMIC_REQ,
	BNXT_RE_RX_ATOMIC_REQ,
	BNXT_RE_TX_READ_REQ,
	BNXT_RE_TX_READ_RESP,
	BNXT_RE_RX_READ_REQ,
	BNXT_RE_RX_READ_RESP,
	BNXT_RE_TX_WRITE_REQ,
	BNXT_RE_RX_WRITE_REQ,
	BNXT_RE_TX_SEND_REQ,
	BNXT_RE_RX_SEND_REQ,
	BNXT_RE_RX_GOOD_PKTS,
	BNXT_RE_RX_GOOD_BYTES,
	BNXT_RE_RX_DCN_PAYLOAD_CUT,
	BNXT_RE_TE_BYPASSED,
	BNXT_RE_RX_ECN_MARKED_PKTS,
	BNXT_RE_MAX_RETRY_EXCEEDED,
	BNXT_RE_TO_RETRANSMITS,
	BNXT_RE_SEQ_ERR_NAKS_RCVD,
	BNXT_RE_RNR_NAKS_RCVD,
	BNXT_RE_MISSING_RESP,
	BNXT_RE_DUP_REQS,
	BNXT_RE_UNRECOVERABLE_ERR,
	BNXT_RE_BAD_RESP_ERR,
	BNXT_RE_LOCAL_QP_OP_ERR,
	BNXT_RE_LOCAL_PROTECTION_ERR,
	BNXT_RE_MEM_MGMT_OP_ERR,
	BNXT_RE_REMOTE_INVALID_REQ_ERR,
	BNXT_RE_REMOTE_ACCESS_ERR,
	BNXT_RE_REMOTE_OP_ERR,
	BNXT_RE_RES_EXCEED_MAX,
	BNXT_RE_RES_LENGTH_MISMATCH,
	BNXT_RE_RES_EXCEEDS_WQE,
	BNXT_RE_RES_OPCODE_ERR,
	BNXT_RE_RES_RX_INVALID_RKEY,
	BNXT_RE_RES_RX_DOMAIN_ERR,
	BNXT_RE_RES_RX_NO_PERM,
	BNXT_RE_RES_RX_RANGE_ERR,
	BNXT_RE_RES_TX_INVALID_RKEY,
	BNXT_RE_RES_TX_DOMAIN_ERR,
	BNXT_RE_RES_TX_NO_PERM,
	BNXT_RE_RES_TX_RANGE_ERR,
	BNXT_RE_RES_IRRQ_OFLOW,
	BNXT_RE_RES_UNSUP_OPCODE,
	BNXT_RE_RES_UNALIGNED_ATOMIC,
	BNXT_RE_RES_REM_INV_ERR,
	BNXT_RE_RES_MEM_ERROR64,
	BNXT_RE_RES_SRQ_ERR,
	BNXT_RE_RES_CMP_ERR,
	BNXT_RE_RES_INVALID_DUP_RKEY,
	BNXT_RE_RES_WQE_FORMAT_ERR,
	BNXT_RE_RES_CQ_LOAD_ERR,
	BNXT_RE_RES_SRQ_LOAD_ERR,
	BNXT_RE_RES_TX_PCI_ERR,
	BNXT_RE_RES_RX_PCI_ERR,
	BNXT_RE_RES_OOS_DROP_COUNT,
	BNXT_RE_NUM_IRQ_STARTED,
	BNXT_RE_NUM_IRQ_STOPPED,
	BNXT_RE_POLL_IN_INTR_EN,
	BNXT_RE_POLL_IN_INTR_DIS,
	BNXT_RE_CMDQ_FULL_DBG_CNT,
	BNXT_RE_FW_SERVICE_PROF_TYPE_SUP,
	BNXT_RE_DBQ_INT_RECV,
	BNXT_RE_DBQ_INT_EN,
	BNXT_RE_DBQ_PACING_RESCHED,
	BNXT_RE_DBQ_PACING_CMPL,
	BNXT_RE_DBQ_PACING_ALERT,
	BNXT_RE_DBQ_DBR_FIFO_REG,
	BNXT_RE_DBQ_NUM_EXT_COUNTERS
};

#define BNXT_RE_NUM_STD_COUNTERS (BNXT_RE_OUT_OF_SEQ_ERR + 1)

struct bnxt_re_stats {
	struct bnxt_qplib_roce_stats    errs;
	struct bnxt_qplib_ext_stat      ext_stat;
};

struct rdma_hw_stats *bnxt_re_alloc_hw_port_stats(struct ib_device *ibdev,
						     u8 port_num);
int bnxt_re_get_hw_stats(struct ib_device *ibdev,
			    struct rdma_hw_stats *stats,
			    u8 port, int index);
int bnxt_re_get_device_stats(struct bnxt_re_dev *rdev);
int bnxt_re_get_flow_stats_from_service_pf(struct bnxt_re_dev *rdev,
				struct bnxt_re_flow_counters *stats,
				struct bnxt_qplib_query_stats_info *sinfo);
int bnxt_re_get_qos_stats(struct bnxt_re_dev *rdev);
#endif /* __STATS_H__ */
