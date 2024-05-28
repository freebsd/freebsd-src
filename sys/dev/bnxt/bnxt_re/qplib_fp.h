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
 * Description: Fast Path Operators (header)
 */

#ifndef __BNXT_QPLIB_FP_H__
#define __BNXT_QPLIB_FP_H__

/* Temp header structures for SQ */
struct sq_ud_ext_hdr {
	__le32	dst_qp;
	__le32	avid;
	__le64	rsvd;
};

struct sq_raw_ext_hdr {
	__le32	cfa_meta;
	__le32	rsvd0;
	__le64	rsvd1;
};

struct sq_rdma_ext_hdr {
	__le64	remote_va;
	__le32	remote_key;
	__le32	rsvd;
};

struct sq_atomic_ext_hdr {
	__le64	swap_data;
	__le64	cmp_data;
};

struct sq_fr_pmr_ext_hdr {
	__le64	pblptr;
	__le64	va;
};

struct sq_bind_ext_hdr {
	__le64	va;
	__le32	length_lo;
	__le32	length_hi;
};

struct rq_ext_hdr {
	__le64	rsvd1;
	__le64	rsvd2;
};

#define BNXT_QPLIB_ETHTYPE_ROCEV1	0x8915

struct bnxt_qplib_srq {
	struct bnxt_qplib_pd		*pd;
	struct bnxt_qplib_dpi		*dpi;
	struct bnxt_qplib_chip_ctx	*cctx;
	struct bnxt_qplib_cq		*cq;
	struct bnxt_qplib_swq		*swq;
	struct bnxt_qplib_hwq		hwq;
	struct bnxt_qplib_db_info	dbinfo;
	struct bnxt_qplib_sg_info	sginfo;
	u64				srq_handle;
	u32				id;
	u16				wqe_size;
	u32				max_wqe;
	u32				max_sge;
	u32				threshold;
	bool				arm_req;
	int				start_idx;
	int				last_idx;
	u16				eventq_hw_ring_id;
	bool				is_user;
	spinlock_t			lock;
};

struct bnxt_qplib_sge {
	u64				addr;
	u32				size;
	u32				lkey;
};

/*
 * Buffer space for ETH(14), IP or GRH(40), UDP header(8)
 * and ib_bth + ib_deth (20).
 * Max required is 82 when RoCE V2 is enabled
 */

/*
 *		RoCE V1 (38 bytes needed)
 * +------------+----------+--------+--------+-------+
 * |Eth-hdr(14B)| GRH (40B)|bth+deth|  Mad   | iCRC  |
 * |		| supplied |  20B   |payload |  4B   |
 * |		| by user  |supplied| 256B   |	     |
 * |		| mad      |        |by user |	     |
 * |		|	   |	    |	     |       |
 * |    sge 1	|  sge 2   | sge 3  | sge 4  | sge 5 |
 * +------------+----------+--------+--------+-------+
 */

/*
 *		RoCE V2-IPv4 (46 Bytes needed)
 * +------------+----------+--------+--------+-------+
 * |Eth-hdr(14B)| IP-hdr   |UDP-hdr |  Mad   | iCRC  |
 * |		| supplied |   8B   |payload |  4B   |
 * |		| by user  |bth+deth|  256B  |	     |
 * |		| mad lower|   20B  |supplied|       |
 * |		| 20B out  | (sge 3)|by user |       |
 * |		| of 40B   |	    |	     |	     |
 * |		| grh space|	    |	     |	     |
 * |	sge 1	|  sge 2   |  sge 3 |  sge 4 | sge 5 |
 * +------------+----------+--------+--------+-------+
 */

/*
 *		RoCE V2-IPv6 (46 Bytes needed)
 * +------------+----------+--------+--------+-------+
 * |Eth-hdr(14B)| IPv6     |UDP-hdr |  Mad   | iCRC  |
 * |		| supplied |   8B   |payload |  4B   |
 * |		| by user  |bth+deth| 256B   |       |
 * |		| mad lower|   20B  |supplied|       |
 * |		| 40 bytes |        |by user |       |
 * |		| grh space|	    |        | 	     |
 * |		|	   |	    |	     |	     |
 * |	sge 1	|  sge 2   |  sge 3 |  sge 4 | sge 5 |
 * +------------+----------+--------+--------+-------+
 */

#define BNXT_QPLIB_MAX_QP1_SQ_HDR_SIZE		74
#define BNXT_QPLIB_MAX_QP1_SQ_HDR_SIZE_V2	86
#define BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE		46
#define BNXT_QPLIB_MAX_QP1_RQ_ETH_HDR_SIZE	14
#define BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE_V2	512
#define BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV4	20
#define BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV6	40
#define BNXT_QPLIB_MAX_QP1_RQ_BDETH_HDR_SIZE	20
#define BNXT_QPLIB_MAX_SQSZ			0xFFFF

struct bnxt_qplib_hdrbuf {
	dma_addr_t	dma_map;
	void		*va;
	u32		len;
	u32		step;
};

struct bnxt_qplib_swq {
	u64				wr_id;
	int				next_idx;
	u8				type;
	u8				flags;
	u32				start_psn;
	u32				next_psn;
	u32				slot_idx;
	u8				slots;
	/* WIP: make it void * to handle legacy also */
	struct sq_psn_search		*psn_search;
	void				*inline_data;
};

struct bnxt_qplib_swqe {
	/* General */
#define	BNXT_QPLIB_FENCE_WRID	0x46454E43	/* "FENC" */
#define	BNXT_QPLIB_QP1_DUMMY_WRID 0x44554D59 /* "DUMY" */
	u64				wr_id;
	u8				reqs_type;
	u8				type;
#define BNXT_QPLIB_SWQE_TYPE_SEND			0
#define BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM		1
#define BNXT_QPLIB_SWQE_TYPE_SEND_WITH_INV		2
#define BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE			4
#define BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM	5
#define BNXT_QPLIB_SWQE_TYPE_RDMA_READ			6
#define BNXT_QPLIB_SWQE_TYPE_ATOMIC_CMP_AND_SWP		8
#define BNXT_QPLIB_SWQE_TYPE_ATOMIC_FETCH_AND_ADD	11
#define BNXT_QPLIB_SWQE_TYPE_LOCAL_INV			12
#define BNXT_QPLIB_SWQE_TYPE_FAST_REG_MR		13
#define BNXT_QPLIB_SWQE_TYPE_REG_MR			13
#define BNXT_QPLIB_SWQE_TYPE_BIND_MW			14
#define BNXT_QPLIB_SWQE_TYPE_RECV			128
#define BNXT_QPLIB_SWQE_TYPE_RECV_RDMA_IMM		129
	u8				flags;
#define BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP		(1 << 0)
#define BNXT_QPLIB_SWQE_FLAGS_RD_ATOMIC_FENCE		(1 << 1)
#define BNXT_QPLIB_SWQE_FLAGS_UC_FENCE			(1 << 2)
#define BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT		(1 << 3)
#define BNXT_QPLIB_SWQE_FLAGS_INLINE			(1 << 4)
	struct bnxt_qplib_sge		*sg_list;
	int				num_sge;

	union {
		/* Send, with imm, inval key */
		struct {
			union {
				__be32  imm_data;
				u32     inv_key;
			};
			u32		q_key;
			u32		dst_qp;
			u16		avid;
		} send;

		/* Send Raw Ethernet and QP1 */
		struct {
			u16		lflags;
			u16		cfa_action;
			u32		cfa_meta;
		} rawqp1;

		/* RDMA write, with imm, read */
		struct {
			union {
				__be32  imm_data;
				u32     inv_key;
			};
			u64		remote_va;
			u32		r_key;
		} rdma;

		/* Atomic cmp/swap, fetch/add */
		struct {
			u64		remote_va;
			u32		r_key;
			u64		swap_data;
			u64		cmp_data;
		} atomic;

		/* Local Invalidate */
		struct {
			u32		inv_l_key;
		} local_inv;

		/* FR-PMR */
		struct {
			u8		access_cntl;
			u8		pg_sz_log;
			bool		zero_based;
			u32		l_key;
			u32		length;
			u8		pbl_pg_sz_log;
#define BNXT_QPLIB_SWQE_PAGE_SIZE_4K			0
#define BNXT_QPLIB_SWQE_PAGE_SIZE_8K			1
#define BNXT_QPLIB_SWQE_PAGE_SIZE_64K			4
#define BNXT_QPLIB_SWQE_PAGE_SIZE_256K			6
#define BNXT_QPLIB_SWQE_PAGE_SIZE_1M			8
#define BNXT_QPLIB_SWQE_PAGE_SIZE_2M			9
#define BNXT_QPLIB_SWQE_PAGE_SIZE_4M			10
#define BNXT_QPLIB_SWQE_PAGE_SIZE_1G			18
			u8		levels;
#define PAGE_SHIFT_4K	12
			__le64		*pbl_ptr;
			dma_addr_t	pbl_dma_ptr;
			u64		*page_list;
			u16		page_list_len;
			u64		va;
		} frmr;

		/* Bind */
		struct {
			u8		access_cntl;
#define BNXT_QPLIB_BIND_SWQE_ACCESS_LOCAL_WRITE		(1 << 0)
#define BNXT_QPLIB_BIND_SWQE_ACCESS_REMOTE_READ		(1 << 1)
#define BNXT_QPLIB_BIND_SWQE_ACCESS_REMOTE_WRITE	(1 << 2)
#define BNXT_QPLIB_BIND_SWQE_ACCESS_REMOTE_ATOMIC	(1 << 3)
#define BNXT_QPLIB_BIND_SWQE_ACCESS_WINDOW_BIND		(1 << 4)
			bool		zero_based;
			u8		mw_type;
			u32		parent_l_key;
			u32		r_key;
			u64		va;
			u32		length;
		} bind;
	};
};

struct bnxt_qplib_q {
	struct bnxt_qplib_swq		*swq;
	struct bnxt_qplib_db_info	dbinfo;
	struct bnxt_qplib_sg_info	sginfo;
	struct bnxt_qplib_hwq		hwq;
	u32				max_wqe;
	u16				max_sge;
	u16				wqe_size;
	u16				q_full_delta;
	u32				psn;
	bool				condition;
	bool				single;
	bool				legacy_send_phantom;
	u32				phantom_wqe_cnt;
	u32				phantom_cqe_cnt;
	u32				next_cq_cons;
	bool				flushed;
	u32				swq_start;
	u32				swq_last;
};

#define BNXT_QPLIB_PPP_REQ		0x1
#define BNXT_QPLIB_PPP_ST_IDX_SHIFT	0x1

struct bnxt_qplib_ppp {
	u32 dpi;
	u8 req;
	u8 st_idx_en;
};

struct bnxt_qplib_qp {
	struct bnxt_qplib_pd		*pd;
	struct bnxt_qplib_dpi		*dpi;
	struct bnxt_qplib_chip_ctx	*cctx;
	u64				qp_handle;
#define	BNXT_QPLIB_QP_ID_INVALID	0xFFFFFFFF
	u32				id;
	u8				type;
	u8				sig_type;
	u8				wqe_mode;
	u8				state;
	u8				cur_qp_state;
	u8				is_user;
	u64				modify_flags;
	u32				max_inline_data;
	u32				mtu;
	u32				path_mtu;
	bool				en_sqd_async_notify;
	u16				pkey_index;
	u32				qkey;
	u32				dest_qp_id;
	u8				access;
	u8				timeout;
	u8				retry_cnt;
	u8				rnr_retry;
	u64				wqe_cnt;
	u32				min_rnr_timer;
	u32				max_rd_atomic;
	u32				max_dest_rd_atomic;
	u32				dest_qpn;
	u8				smac[6];
	u16				vlan_id;
	u8				nw_type;
	u16				port_id;
	struct bnxt_qplib_ah		ah;
	struct bnxt_qplib_ppp		ppp;

#define BTH_PSN_MASK			((1 << 24) - 1)
	/* SQ */
	struct bnxt_qplib_q		sq;
	/* RQ */
	struct bnxt_qplib_q		rq;
	/* SRQ */
	struct bnxt_qplib_srq		*srq;
	/* CQ */
	struct bnxt_qplib_cq		*scq;
	struct bnxt_qplib_cq		*rcq;
	/* IRRQ and ORRQ */
	struct bnxt_qplib_hwq		irrq;
	struct bnxt_qplib_hwq		orrq;
	/* Header buffer for QP1 */
	struct bnxt_qplib_hdrbuf	*sq_hdr_buf;
	struct bnxt_qplib_hdrbuf	*rq_hdr_buf;

	/* ToS */
	u8				tos_ecn;
	u8				tos_dscp;
	/* To track the SQ and RQ flush list */
	struct list_head		sq_flush;
	struct list_head		rq_flush;
	/* 4 bytes of QP's scrabled mac received from FW */
	u32				lag_src_mac;
	u32				msn;
	u32				msn_tbl_sz;
	/* get devflags in PI code */
	u16				dev_cap_flags;
};


#define CQE_CMP_VALID(hdr, pass)				\
	(!!((hdr)->cqe_type_toggle & CQ_BASE_TOGGLE) ==		\
	   !(pass & BNXT_QPLIB_FLAG_EPOCH_CONS_MASK))

static inline u32 __bnxt_qplib_get_avail(struct bnxt_qplib_hwq *hwq)
{
	int cons, prod, avail;

	/* False full is possible retrying post-send makes sense */
	cons = hwq->cons;
	prod = hwq->prod;
	avail = cons - prod;
	if (cons <= prod)
		avail += hwq->depth;
	return avail;
}

static inline bool bnxt_qplib_queue_full(struct bnxt_qplib_hwq *hwq, u8 slots)
{
	return __bnxt_qplib_get_avail(hwq) <= slots;
}

struct bnxt_qplib_cqe {
	u8				status;
	u8				type;
	u8				opcode;
	u32				length;
	/* Lower 16 is cfa_metadata0, Upper 16 is cfa_metadata1 */
	u32				cfa_meta;
#define BNXT_QPLIB_META1_SHIFT		16
#define	BNXT_QPLIB_CQE_CFA_META1_VALID  0x80000UL
	u64				wr_id;
	union {
		__be32                  immdata;
		u32                     invrkey;
	};
	u64				qp_handle;
	u64				mr_handle;
	u16				flags;
	u8				smac[6];
	u32				src_qp;
	u16				raweth_qp1_flags;
	u16				raweth_qp1_errors;
	u16				raweth_qp1_cfa_code;
	u32				raweth_qp1_flags2;
	u32				raweth_qp1_metadata;
	u8				raweth_qp1_payload_offset;
	u16				pkey_index;
};

#define BNXT_QPLIB_QUEUE_START_PERIOD		0x01
struct bnxt_qplib_cq {
	struct bnxt_qplib_dpi		*dpi;
	struct bnxt_qplib_chip_ctx	*cctx;
	struct bnxt_qplib_nq		*nq;
	struct bnxt_qplib_db_info	dbinfo;
	struct bnxt_qplib_sg_info	sginfo;
	struct bnxt_qplib_hwq		hwq;
	struct bnxt_qplib_hwq		resize_hwq;
	struct list_head		sqf_head;
	struct list_head		rqf_head;
	u32				max_wqe;
	u32				id;
	u16				count;
	u16				period;
	u32				cnq_hw_ring_id;
	u64				cq_handle;
	atomic_t			arm_state;
#define CQ_RESIZE_WAIT_TIME_MS		500
	unsigned long			flags;
#define CQ_FLAGS_RESIZE_IN_PROG		1
	wait_queue_head_t		waitq;
	spinlock_t			flush_lock; /* lock flush queue list */
	spinlock_t			compl_lock; /* synch CQ handlers */
	u16				cnq_events;
	bool				is_cq_err_event;
	bool				destroyed;
	u8				toggle;
};

#define BNXT_QPLIB_MAX_IRRQE_ENTRY_SIZE	sizeof(struct xrrq_irrq)
#define BNXT_QPLIB_MAX_ORRQE_ENTRY_SIZE	sizeof(struct xrrq_orrq)
#define IRD_LIMIT_TO_IRRQ_SLOTS(x)	(2 * x + 2)
#define IRRQ_SLOTS_TO_IRD_LIMIT(s)	((s >> 1) - 1)
#define ORD_LIMIT_TO_ORRQ_SLOTS(x)	(x + 1)
#define ORRQ_SLOTS_TO_ORD_LIMIT(s)	(s - 1)

#define NQE_CMP_VALID(hdr, pass)				\
	(!!(le32_to_cpu((hdr)->info63_v & 0xffffffff) & NQ_BASE_V) ==	\
	   !(pass & BNXT_QPLIB_FLAG_EPOCH_CONS_MASK))

#define BNXT_QPLIB_NQE_MAX_CNT		(128 * 1024)

/* MSN table print macros for debugging */
#define BNXT_RE_MSN_IDX(m) (((m) & SQ_MSN_SEARCH_START_IDX_MASK) >> \
		SQ_MSN_SEARCH_START_IDX_SFT)
#define BNXT_RE_MSN_NPSN(m) (((m) & SQ_MSN_SEARCH_NEXT_PSN_MASK) >> \
		SQ_MSN_SEARCH_NEXT_PSN_SFT)
#define BNXT_RE_MSN_SPSN(m) (((m) & SQ_MSN_SEARCH_START_PSN_MASK) >> \
		SQ_MSN_SEARCH_START_PSN_SFT)
#define BNXT_MSN_TBLE_SGE 6

struct bnxt_qplib_nq_stats {
	u64	num_dbqne_processed;
	u64	num_srqne_processed;
	u64	num_cqne_processed;
	u64	num_tasklet_resched;
	u64	num_nq_rearm;
};

struct bnxt_qplib_nq_db {
	struct bnxt_qplib_reg_desc	reg;
	void __iomem			*db;
	struct bnxt_qplib_db_info	dbinfo;
};

typedef int (*cqn_handler_t)(struct bnxt_qplib_nq *nq,
			     struct bnxt_qplib_cq *cq);
typedef int (*srqn_handler_t)(struct bnxt_qplib_nq *nq,
			      struct bnxt_qplib_srq *srq, u8 event);

struct bnxt_qplib_nq {
	struct bnxt_qplib_res		*res;
	struct bnxt_qplib_hwq		hwq;
	struct bnxt_qplib_nq_db		nq_db;

	char				*name;
	u16				ring_id;
	int				msix_vec;
	bool				requested;
	int				budget;
	u32				load;
	struct mutex			lock;

	cqn_handler_t			cqn_handler;
	srqn_handler_t			srqn_handler;
	struct workqueue_struct		*cqn_wq;
	struct bnxt_qplib_nq_stats	stats;
};

struct bnxt_qplib_nq_work {
	struct work_struct      work;
	struct bnxt_qplib_nq    *nq;
	struct bnxt_qplib_cq	*cq;
};

static inline dma_addr_t
bnxt_qplib_get_qp_buf_from_index(struct bnxt_qplib_qp *qp, u32 index)
{
	struct bnxt_qplib_hdrbuf *buf;

	buf = qp->rq_hdr_buf;
	return (buf->dma_map + index * buf->step);
}

void bnxt_qplib_nq_stop_irq(struct bnxt_qplib_nq *nq, bool kill);
void bnxt_qplib_disable_nq(struct bnxt_qplib_nq *nq);
int bnxt_qplib_nq_start_irq(struct bnxt_qplib_nq *nq, int nq_indx,
			    int msix_vector, bool need_init);
int bnxt_qplib_enable_nq(struct bnxt_qplib_nq *nq, int nq_idx,
			 int msix_vector, int bar_reg_offset,
			 cqn_handler_t cqn_handler,
			 srqn_handler_t srq_handler);
int bnxt_qplib_create_srq(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_srq *srq);
int bnxt_qplib_modify_srq(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_srq *srq);
int bnxt_qplib_query_srq(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_srq *srq);
int bnxt_qplib_destroy_srq(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_srq *srq);
int bnxt_qplib_post_srq_recv(struct bnxt_qplib_srq *srq,
			     struct bnxt_qplib_swqe *wqe);
int bnxt_qplib_create_qp1(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp);
int bnxt_qplib_create_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp);
int bnxt_qplib_modify_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp);
int bnxt_qplib_query_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp);
int bnxt_qplib_destroy_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp);
void bnxt_qplib_clean_qp(struct bnxt_qplib_qp *qp);
void bnxt_qplib_free_qp_res(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp);
void *bnxt_qplib_get_qp1_sq_buf(struct bnxt_qplib_qp *qp,
				struct bnxt_qplib_sge *sge);
void *bnxt_qplib_get_qp1_rq_buf(struct bnxt_qplib_qp *qp,
				struct bnxt_qplib_sge *sge);
u32 bnxt_qplib_get_rq_prod_index(struct bnxt_qplib_qp *qp);
void bnxt_qplib_post_send_db(struct bnxt_qplib_qp *qp);
int bnxt_qplib_post_send(struct bnxt_qplib_qp *qp,
			 struct bnxt_qplib_swqe *wqe);
void bnxt_qplib_post_recv_db(struct bnxt_qplib_qp *qp);
int bnxt_qplib_post_recv(struct bnxt_qplib_qp *qp,
			 struct bnxt_qplib_swqe *wqe);
int bnxt_qplib_create_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq);
int bnxt_qplib_modify_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq);
int bnxt_qplib_resize_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq,
			 int new_cqes);
void bnxt_qplib_resize_cq_complete(struct bnxt_qplib_res *res,
				   struct bnxt_qplib_cq *cq);
int bnxt_qplib_destroy_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq);
void bnxt_qplib_free_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq);
int bnxt_qplib_poll_cq(struct bnxt_qplib_cq *cq, struct bnxt_qplib_cqe *cqe,
		       int num, struct bnxt_qplib_qp **qp);
bool bnxt_qplib_is_cq_empty(struct bnxt_qplib_cq *cq);
void bnxt_qplib_req_notify_cq(struct bnxt_qplib_cq *cq, u32 arm_type);
void bnxt_qplib_free_nq_mem(struct bnxt_qplib_nq *nq);
int bnxt_qplib_alloc_nq_mem(struct bnxt_qplib_res *res,
			    struct bnxt_qplib_nq *nq);
void bnxt_qplib_add_flush_qp(struct bnxt_qplib_qp *qp);
void bnxt_qplib_del_flush_qp(struct bnxt_qplib_qp *qp);
int bnxt_qplib_process_flush_list(struct bnxt_qplib_cq *cq,
				struct bnxt_qplib_cqe *cqe,
				int num_cqes);
void bnxt_qplib_flush_cqn_wq(struct bnxt_qplib_qp *qp);
void bnxt_qplib_free_hdr_buf(struct bnxt_qplib_res *res,
			     struct bnxt_qplib_qp *qp);
int bnxt_qplib_alloc_hdr_buf(struct bnxt_qplib_res *res,
			     struct bnxt_qplib_qp *qp, u32 slen, u32 rlen);

static inline bool __can_request_ppp(struct bnxt_qplib_qp *qp)
{
	bool can_request = false;

	if (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_RESET &&
	    qp->state ==  CMDQ_MODIFY_QP_NEW_STATE_INIT &&
	    qp->ppp.req &&
	    !(qp->ppp.st_idx_en &
		    CREQ_MODIFY_QP_RESP_PINGPONG_PUSH_ENABLED))
		can_request = true;
	return can_request;
}

/* MSN table update inlin */
static inline uint64_t bnxt_re_update_msn_tbl(uint32_t st_idx, uint32_t npsn, uint32_t start_psn)
{
	return cpu_to_le64((((u64)(st_idx) << SQ_MSN_SEARCH_START_IDX_SFT) &
		SQ_MSN_SEARCH_START_IDX_MASK) |
		(((u64)(npsn) << SQ_MSN_SEARCH_NEXT_PSN_SFT) &
		SQ_MSN_SEARCH_NEXT_PSN_MASK) |
		(((start_psn) << SQ_MSN_SEARCH_START_PSN_SFT) &
		SQ_MSN_SEARCH_START_PSN_MASK));
}

void bnxt_re_schedule_dbq_event(struct bnxt_qplib_res *res);
#endif
