/*-
 * Copyright (c) 2013-2017, Mellanox Technologies, Ltd.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef MLX5_QP_H
#define MLX5_QP_H

#include <dev/mlx5/driver.h>

#define MLX5_INVALID_LKEY	0x100
#define MLX5_SIG_WQE_SIZE	(MLX5_SEND_WQE_BB * 5)
#define MLX5_DIF_SIZE		8
#define MLX5_STRIDE_BLOCK_OP	0x400
#define MLX5_CPY_GRD_MASK	0xc0
#define MLX5_CPY_APP_MASK	0x30
#define MLX5_CPY_REF_MASK	0x0f
#define MLX5_BSF_INC_REFTAG	(1 << 6)
#define MLX5_BSF_INL_VALID	(1 << 15)
#define MLX5_BSF_REFRESH_DIF	(1 << 14)
#define MLX5_BSF_REPEAT_BLOCK	(1 << 7)
#define MLX5_BSF_APPTAG_ESCAPE	0x1
#define MLX5_BSF_APPREF_ESCAPE	0x2
#define MLX5_WQE_DS_UNITS 16

enum mlx5_qp_optpar {
	MLX5_QP_OPTPAR_ALT_ADDR_PATH		= 1 << 0,
	MLX5_QP_OPTPAR_RRE			= 1 << 1,
	MLX5_QP_OPTPAR_RAE			= 1 << 2,
	MLX5_QP_OPTPAR_RWE			= 1 << 3,
	MLX5_QP_OPTPAR_PKEY_INDEX		= 1 << 4,
	MLX5_QP_OPTPAR_Q_KEY			= 1 << 5,
	MLX5_QP_OPTPAR_RNR_TIMEOUT		= 1 << 6,
	MLX5_QP_OPTPAR_PRIMARY_ADDR_PATH	= 1 << 7,
	MLX5_QP_OPTPAR_SRA_MAX			= 1 << 8,
	MLX5_QP_OPTPAR_RRA_MAX			= 1 << 9,
	MLX5_QP_OPTPAR_PM_STATE			= 1 << 10,
	MLX5_QP_OPTPAR_RETRY_COUNT		= 1 << 12,
	MLX5_QP_OPTPAR_RNR_RETRY		= 1 << 13,
	MLX5_QP_OPTPAR_ACK_TIMEOUT		= 1 << 14,
	MLX5_QP_OPTPAR_PRI_PORT			= 1 << 16,
	MLX5_QP_OPTPAR_SRQN			= 1 << 18,
	MLX5_QP_OPTPAR_CQN_RCV			= 1 << 19,
	MLX5_QP_OPTPAR_DC_HS			= 1 << 20,
	MLX5_QP_OPTPAR_DC_KEY			= 1 << 21,
};

enum mlx5_qp_state {
	MLX5_QP_STATE_RST			= 0,
	MLX5_QP_STATE_INIT			= 1,
	MLX5_QP_STATE_RTR			= 2,
	MLX5_QP_STATE_RTS			= 3,
	MLX5_QP_STATE_SQER			= 4,
	MLX5_QP_STATE_SQD			= 5,
	MLX5_QP_STATE_ERR			= 6,
	MLX5_QP_STATE_SQ_DRAINING		= 7,
	MLX5_QP_STATE_SUSPENDED			= 9,
	MLX5_QP_NUM_STATE,
	MLX5_QP_STATE,
	MLX5_QP_STATE_BAD,
};

enum {
	MLX5_SQ_STATE_NA	= MLX5_SQC_STATE_ERR + 1,
	MLX5_SQ_NUM_STATE	= MLX5_SQ_STATE_NA + 1,
	MLX5_RQ_STATE_NA	= MLX5_RQC_STATE_ERR + 1,
	MLX5_RQ_NUM_STATE	= MLX5_RQ_STATE_NA + 1,
};

enum {
	MLX5_QP_ST_RC				= 0x0,
	MLX5_QP_ST_UC				= 0x1,
	MLX5_QP_ST_UD				= 0x2,
	MLX5_QP_ST_XRC				= 0x3,
	MLX5_QP_ST_MLX				= 0x4,
	MLX5_QP_ST_DCI				= 0x5,
	MLX5_QP_ST_DCT				= 0x6,
	MLX5_QP_ST_QP0				= 0x7,
	MLX5_QP_ST_QP1				= 0x8,
	MLX5_QP_ST_RAW_ETHERTYPE		= 0x9,
	MLX5_QP_ST_RAW_IPV6			= 0xa,
	MLX5_QP_ST_SNIFFER			= 0xb,
	MLX5_QP_ST_SYNC_UMR			= 0xe,
	MLX5_QP_ST_PTP_1588			= 0xd,
	MLX5_QP_ST_REG_UMR			= 0xc,
	MLX5_QP_ST_SW_CNAK			= 0x10,
	MLX5_QP_ST_MAX
};

enum {
	MLX5_NON_ZERO_RQ	= 0x0,
	MLX5_SRQ_RQ		= 0x1,
	MLX5_CRQ_RQ		= 0x2,
	MLX5_ZERO_LEN_RQ	= 0x3
};

enum {
	/* params1 */
	MLX5_QP_BIT_SRE				= 1 << 15,
	MLX5_QP_BIT_SWE				= 1 << 14,
	MLX5_QP_BIT_SAE				= 1 << 13,
	/* params2 */
	MLX5_QP_BIT_RRE				= 1 << 15,
	MLX5_QP_BIT_RWE				= 1 << 14,
	MLX5_QP_BIT_RAE				= 1 << 13,
	MLX5_QP_BIT_RIC				= 1 <<	4,
	MLX5_QP_BIT_COLL_SYNC_RQ                = 1 << 2,
	MLX5_QP_BIT_COLL_SYNC_SQ                = 1 << 1,
	MLX5_QP_BIT_COLL_MASTER                 = 1 << 0
};

enum {
	MLX5_DCT_BIT_RRE		= 1 << 19,
	MLX5_DCT_BIT_RWE		= 1 << 18,
	MLX5_DCT_BIT_RAE		= 1 << 17,
};

enum {
	MLX5_WQE_CTRL_CQ_UPDATE		= 2 << 2,
	MLX5_WQE_CTRL_CQ_UPDATE_AND_EQE	= 3 << 2,
	MLX5_WQE_CTRL_SOLICITED		= 1 << 1,
};

#define	MLX5_SEND_WQE_DS	16
#define	MLX5_SEND_WQE_BB	64
#define MLX5_SEND_WQEBB_NUM_DS	(MLX5_SEND_WQE_BB / MLX5_SEND_WQE_DS)
#define MLX5_WQE_CTRL_QPN_SHIFT 8
#define MLX5_WQE_CTRL_WQE_INDEX_SHIFT 8

enum {
	MLX5_SEND_WQE_MAX_WQEBBS	= 16,
};

enum {
	MLX5_WQE_FMR_PERM_LOCAL_READ	= 1 << 27,
	MLX5_WQE_FMR_PERM_LOCAL_WRITE	= 1 << 28,
	MLX5_WQE_FMR_PERM_REMOTE_READ	= 1 << 29,
	MLX5_WQE_FMR_PERM_REMOTE_WRITE	= 1 << 30,
	MLX5_WQE_FMR_PERM_ATOMIC	= 1U << 31
};

enum {
	MLX5_FENCE_MODE_NONE			= 0 << 5,
	MLX5_FENCE_MODE_INITIATOR_SMALL		= 1 << 5,
	MLX5_FENCE_MODE_FENCE			= 2 << 5,
	MLX5_FENCE_MODE_STRONG_ORDERING		= 3 << 5,
	MLX5_FENCE_MODE_SMALL_AND_FENCE		= 4 << 5,
};

enum {
	MLX5_RCV_DBR	= 0,
	MLX5_SND_DBR	= 1,
};

enum {
	MLX5_FLAGS_INLINE	= 1<<7,
	MLX5_FLAGS_CHECK_FREE   = 1<<5,
};

struct mlx5_wqe_fmr_seg {
	__be32			flags;
	__be32			mem_key;
	__be64			buf_list;
	__be64			start_addr;
	__be64			reg_len;
	__be32			offset;
	__be32			page_size;
	u32			reserved[2];
};

struct mlx5_wqe_ctrl_seg {
	__be32			opmod_idx_opcode;
	__be32			qpn_ds;
	u8			signature;
	u8			rsvd[2];
	u8			fm_ce_se;
	union {
		__be32		imm;
		__be32          general_id;
	};
};

#define MLX5_WQE_CTRL_DS_MASK 0x3f

enum {
	MLX5_MLX_FLAG_MASK_VL15 = 0x40,
	MLX5_MLX_FLAG_MASK_SLR	= 0x20,
	MLX5_MLX_FLAG_MASK_ICRC = 0x8,
	MLX5_MLX_FLAG_MASK_FL	= 4
};

struct mlx5_mlx_seg {
	__be32		rsvd0;
	u8		flags;
	u8		stat_rate_sl;
	u8		rsvd1[8];
	__be16		dlid;
};

enum {
	MLX5_ETH_WQE_L3_INNER_CSUM	= 1 << 4,
	MLX5_ETH_WQE_L4_INNER_CSUM	= 1 << 5,
	MLX5_ETH_WQE_L3_CSUM		= 1 << 6,
	MLX5_ETH_WQE_L4_CSUM		= 1 << 7,
};

enum {
	MLX5_ETH_WQE_SWP_INNER_L3_TYPE = 1 << 0,
	MLX5_ETH_WQE_SWP_INNER_L4_TYPE = 1 << 1,
	MLX5_ETH_WQE_SWP_OUTER_L3_TYPE = 1 << 4,
	MLX5_ETH_WQE_SWP_OUTER_L4_TYPE = 1 << 5,
};

enum {
	MLX5_ETH_WQE_FT_META_IPSEC = BIT(0),
};

struct mlx5_wqe_eth_seg {
	u8              swp_outer_l4_offset;
	u8		swp_outer_l3_offset;
	u8		swp_inner_l4_offset;
	u8		swp_inner_l3_offset;
	u8		cs_flags;
	u8		swp_flags;
	__be16		mss;
	__be32		flow_table_metadata;
	union {
		struct {
			__be16		inline_hdr_sz;
			u8		inline_hdr_start[2];
		};
		struct {
			__be16		vlan_cmd;
			__be16		vlan_hdr;
		};
	};
};

struct mlx5_wqe_xrc_seg {
	__be32			xrc_srqn;
	u8			rsvd[12];
};

struct mlx5_wqe_masked_atomic_seg {
	__be64			swap_add;
	__be64			compare;
	__be64			swap_add_mask;
	__be64			compare_mask;
};

struct mlx5_av {
	union {
		struct {
			__be32	qkey;
			__be32	reserved;
		} qkey;
		__be64	dc_key;
	} key;
	__be32	dqp_dct;
	u8	stat_rate_sl;
	u8	fl_mlid;
	union {
		__be16	rlid;
		__be16  udp_sport;
	};
	u8	reserved0[4];
	u8	rmac[6];
	u8	tclass;
	u8	hop_limit;
	__be32	grh_gid_fl;
	u8	rgid[16];
};

struct mlx5_wqe_datagram_seg {
	struct mlx5_av	av;
};

struct mlx5_wqe_raddr_seg {
	__be64			raddr;
	__be32			rkey;
	u32			reserved;
};

struct mlx5_wqe_atomic_seg {
	__be64			swap_add;
	__be64			compare;
};

struct mlx5_wqe_data_seg {
	__be32			byte_count;
	__be32			lkey;
	__be64			addr;
};

struct mlx5_wqe_umr_ctrl_seg {
	u8		flags;
	u8		rsvd0[3];
	__be16		klm_octowords;
	__be16		bsf_octowords;
	__be64		mkey_mask;
	u8		rsvd1[32];
};

struct mlx5_seg_set_psv {
	__be32		psv_num;
	__be16		syndrome;
	__be16		status;
	__be32		transient_sig;
	__be32		ref_tag;
};

struct mlx5_wqe_qos_remap_seg {
	u8		rsvd0[4];
	u8		rsvd1[4];
	__be32		qos_handle;
	__be32		queue_handle;
};

struct mlx5_seg_get_psv {
	u8		rsvd[19];
	u8		num_psv;
	__be32		l_key;
	__be64		va;
	__be32		psv_index[4];
};

struct mlx5_seg_check_psv {
	u8		rsvd0[2];
	__be16		err_coalescing_op;
	u8		rsvd1[2];
	__be16		xport_err_op;
	u8		rsvd2[2];
	__be16		xport_err_mask;
	u8		rsvd3[7];
	u8		num_psv;
	__be32		l_key;
	__be64		va;
	__be32		psv_index[4];
};

struct mlx5_rwqe_sig {
	u8	rsvd0[4];
	u8	signature;
	u8	rsvd1[11];
};

struct mlx5_wqe_signature_seg {
	u8	rsvd0[4];
	u8	signature;
	u8	rsvd1[11];
};

struct mlx5_wqe_inline_seg {
	__be32	byte_count;
};

enum mlx5_sig_type {
	MLX5_DIF_CRC = 0x1,
	MLX5_DIF_IPCS = 0x2,
};

struct mlx5_bsf_inl {
	__be16		vld_refresh;
	__be16		dif_apptag;
	__be32		dif_reftag;
	u8		sig_type;
	u8		rp_inv_seed;
	u8		rsvd[3];
	u8		dif_inc_ref_guard_check;
	__be16		dif_app_bitmask_check;
};

struct mlx5_bsf {
	struct mlx5_bsf_basic {
		u8		bsf_size_sbs;
		u8		check_byte_mask;
		union {
			u8	copy_byte_mask;
			u8	bs_selector;
			u8	rsvd_wflags;
		} wire;
		union {
			u8	bs_selector;
			u8	rsvd_mflags;
		} mem;
		__be32		raw_data_size;
		__be32		w_bfs_psv;
		__be32		m_bfs_psv;
	} basic;
	struct mlx5_bsf_ext {
		__be32		t_init_gen_pro_size;
		__be32		rsvd_epi_size;
		__be32		w_tfs_psv;
		__be32		m_tfs_psv;
	} ext;
	struct mlx5_bsf_inl	w_inl;
	struct mlx5_bsf_inl	m_inl;
};

struct mlx5_klm {
	__be32		bcount;
	__be32		key;
	__be64		va;
};

struct mlx5_stride_block_entry {
	__be16		stride;
	__be16		bcount;
	__be32		key;
	__be64		va;
};

struct mlx5_stride_block_ctrl_seg {
	__be32		bcount_per_cycle;
	__be32		op;
	__be32		repeat_count;
	u16		rsvd;
	__be16		num_entries;
};

enum mlx5_pagefault_flags {
	MLX5_PFAULT_REQUESTOR = 1 << 0,
	MLX5_PFAULT_WRITE     = 1 << 1,
	MLX5_PFAULT_RDMA      = 1 << 2,
};

/* Contains the details of a pagefault. */
struct mlx5_pagefault {
	u32			bytes_committed;
	u8			event_subtype;
	enum mlx5_pagefault_flags flags;
	union {
		/* Initiator or send message responder pagefault details. */
		struct {
			/* Received packet size, only valid for responders. */
			u32	packet_size;
			/*
			 * WQE index. Refers to either the send queue or
			 * receive queue, according to event_subtype.
			 */
			u16	wqe_index;
		} wqe;
		/* RDMA responder pagefault details */
		struct {
			u32	r_key;
			/*
			 * Received packet size, minimal size page fault
			 * resolution required for forward progress.
			 */
			u32	packet_size;
			u32	rdma_op_len;
			u64	rdma_va;
		} rdma;
	};
};

struct mlx5_core_qp {
	struct mlx5_core_rsc_common	common; /* must be first */
	void (*event)		(struct mlx5_core_qp *, int);
	int			qpn;
	struct mlx5_rsc_debug	*dbg;
	int			pid;
	u16			uid;
};

struct mlx5_qp_path {
	u8			fl_free_ar;
	u8			rsvd3;
	__be16			pkey_index;
	u8			rsvd0;
	u8			grh_mlid;
	__be16			rlid;
	u8			ackto_lt;
	u8			mgid_index;
	u8			static_rate;
	u8			hop_limit;
	__be32			tclass_flowlabel;
	union {
		u8		rgid[16];
		u8		rip[16];
	};
	u8			f_dscp_ecn_prio;
	u8			ecn_dscp;
	__be16			udp_sport;
	u8			dci_cfi_prio_sl;
	u8			port;
	u8			rmac[6];
};

struct mlx5_qp_context {
	__be32			flags;
	__be32			flags_pd;
	u8			mtu_msgmax;
	u8			rq_size_stride;
	__be16			sq_crq_size;
	__be32			qp_counter_set_usr_page;
	__be32			wire_qpn;
	__be32			log_pg_sz_remote_qpn;
	struct			mlx5_qp_path pri_path;
	struct			mlx5_qp_path alt_path;
	__be32			params1;
	u8			reserved2[4];
	__be32			next_send_psn;
	__be32			cqn_send;
	__be32			deth_sqpn;
	u8			reserved3[4];
	__be32			last_acked_psn;
	__be32			ssn;
	__be32			params2;
	__be32			rnr_nextrecvpsn;
	__be32			xrcd;
	__be32			cqn_recv;
	__be64			db_rec_addr;
	__be32			qkey;
	__be32			rq_type_srqn;
	__be32			rmsn;
	__be16			hw_sq_wqe_counter;
	__be16			sw_sq_wqe_counter;
	__be16			hw_rcyclic_byte_counter;
	__be16			hw_rq_counter;
	__be16			sw_rcyclic_byte_counter;
	__be16			sw_rq_counter;
	u8			rsvd0[5];
	u8			cgs;
	u8			cs_req;
	u8			cs_res;
	__be64			dc_access_key;
	u8			rsvd1[24];
};

struct mlx5_dct_context {
	u8			state;
	u8			rsvd0[7];
	__be32			cqn;
	__be32			flags;
	u8			rsvd1;
	u8			cs_res;
	u8			min_rnr;
	u8			rsvd2;
	__be32			srqn;
	__be32			pdn;
	__be32			tclass_flow_label;
	__be64			access_key;
	u8			mtu;
	u8			port;
	__be16			pkey_index;
	u8			rsvd4;
	u8			mgid_index;
	u8			rsvd5;
	u8			hop_limit;
	__be32			access_violations;
	u8			rsvd[12];
};

static inline struct mlx5_core_qp *__mlx5_qp_lookup(struct mlx5_core_dev *dev, u32 qpn)
{
	return radix_tree_lookup(&dev->priv.qp_table.tree, qpn);
}

static inline struct mlx5_core_mkey *__mlx5_mr_lookup(struct mlx5_core_dev *dev, u32 key)
{
	return radix_tree_lookup(&dev->priv.mr_table.tree, key);
}

int mlx5_core_create_qp(struct mlx5_core_dev *dev,
			struct mlx5_core_qp *qp,
			u32 *in,
			int inlen);
int mlx5_core_qp_modify(struct mlx5_core_dev *dev, u16 opcode,
			u32 opt_param_mask, void *qpc,
			struct mlx5_core_qp *qp);
int mlx5_core_destroy_qp(struct mlx5_core_dev *dev,
			 struct mlx5_core_qp *qp);
int mlx5_core_qp_query(struct mlx5_core_dev *dev, struct mlx5_core_qp *qp,
		       u32 *out, int outlen);
int mlx5_core_dct_query(struct mlx5_core_dev *dev, struct mlx5_core_dct *dct,
			u32 *out, int outlen);
int mlx5_core_arm_dct(struct mlx5_core_dev *dev, struct mlx5_core_dct *dct);

int mlx5_core_xrcd_alloc(struct mlx5_core_dev *dev, u32 *xrcdn);
int mlx5_core_xrcd_dealloc(struct mlx5_core_dev *dev, u32 xrcdn);
int mlx5_core_create_dct(struct mlx5_core_dev *dev,
			 struct mlx5_core_dct *dct,
			 u32 *in, int inlen,
			 u32 *out, int outlen);
int mlx5_core_destroy_dct(struct mlx5_core_dev *dev,
			  struct mlx5_core_dct *dct);
int mlx5_core_create_rq_tracked(struct mlx5_core_dev *dev, u32 *in, int inlen,
				struct mlx5_core_qp *rq);
void mlx5_core_destroy_rq_tracked(struct mlx5_core_dev *dev,
				  struct mlx5_core_qp *rq);
int mlx5_core_create_sq_tracked(struct mlx5_core_dev *dev, u32 *in, int inlen,
				struct mlx5_core_qp *sq);
void mlx5_core_destroy_sq_tracked(struct mlx5_core_dev *dev,
				  struct mlx5_core_qp *sq);
void mlx5_init_qp_table(struct mlx5_core_dev *dev);
void mlx5_cleanup_qp_table(struct mlx5_core_dev *dev);
int mlx5_debug_qp_add(struct mlx5_core_dev *dev, struct mlx5_core_qp *qp);
void mlx5_debug_qp_remove(struct mlx5_core_dev *dev, struct mlx5_core_qp *qp);

static inline const char *mlx5_qp_type_str(int type)
{
	switch (type) {
	case MLX5_QP_ST_RC: return "RC";
	case MLX5_QP_ST_UC: return "C";
	case MLX5_QP_ST_UD: return "UD";
	case MLX5_QP_ST_XRC: return "XRC";
	case MLX5_QP_ST_MLX: return "MLX";
	case MLX5_QP_ST_DCI: return "DCI";
	case MLX5_QP_ST_QP0: return "QP0";
	case MLX5_QP_ST_QP1: return "QP1";
	case MLX5_QP_ST_RAW_ETHERTYPE: return "RAW_ETHERTYPE";
	case MLX5_QP_ST_RAW_IPV6: return "RAW_IPV6";
	case MLX5_QP_ST_SNIFFER: return "SNIFFER";
	case MLX5_QP_ST_SYNC_UMR: return "SYNC_UMR";
	case MLX5_QP_ST_PTP_1588: return "PTP_1588";
	case MLX5_QP_ST_REG_UMR: return "REG_UMR";
	case MLX5_QP_ST_SW_CNAK: return "DC_CNAK";
	default: return "Invalid transport type";
	}
}

static inline const char *mlx5_qp_state_str(int state)
{
	switch (state) {
	case MLX5_QP_STATE_RST:
	return "RST";
	case MLX5_QP_STATE_INIT:
	return "INIT";
	case MLX5_QP_STATE_RTR:
	return "RTR";
	case MLX5_QP_STATE_RTS:
	return "RTS";
	case MLX5_QP_STATE_SQER:
	return "SQER";
	case MLX5_QP_STATE_SQD:
	return "SQD";
	case MLX5_QP_STATE_ERR:
	return "ERR";
	case MLX5_QP_STATE_SQ_DRAINING:
	return "SQ_DRAINING";
	case MLX5_QP_STATE_SUSPENDED:
	return "SUSPENDED";
	default: return "Invalid QP state";
	}
}

#endif /* MLX5_QP_H */
