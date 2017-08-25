/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef MLX5_IB_USER_H
#define MLX5_IB_USER_H

#include <linux/types.h>

enum {
	MLX5_QP_FLAG_SIGNATURE		= 1 << 0,
};

enum {
	MLX5_SRQ_FLAG_SIGNATURE		= 1 << 0,
};

enum {
	MLX5_WQ_FLAG_SIGNATURE		= 1 << 0,
};


/* Increment this value if any changes that break userspace ABI
 * compatibility are made.
 */
#define MLX5_IB_UVERBS_ABI_VERSION	1

/* Make sure that all structs defined in this file remain laid out so
 * that they pack the same way on 32-bit and 64-bit architectures (to
 * avoid incompatibility between 32-bit userspace and 64-bit kernels).
 * In particular do not use pointer types -- pass pointers in __u64
 * instead.
 */

struct mlx5_ib_alloc_ucontext_req {
	__u32	total_num_uuars;
	__u32	num_low_latency_uuars;
};

struct mlx5_ib_alloc_ucontext_req_v2 {
	__u32	total_num_uuars;
	__u32	num_low_latency_uuars;
	__u32	flags;
	__u32	reserved;
};

struct mlx5_ib_alloc_ucontext_resp {
	__u32	qp_tab_size;
	__u32	bf_reg_size;
	__u32	tot_uuars;
	__u32	cache_line_size;
	__u16	max_sq_desc_sz;
	__u16	max_rq_desc_sz;
	__u32	max_send_wqebb;
	__u32	max_recv_wr;
	__u32	max_srq_recv_wr;
	__u16	num_ports;
	__u16	reserved;
	__u32	max_desc_sz_sq_dc;
	__u32	atomic_arg_sizes_dc;
	__u32	reserved1;
	__u32	flags;
	__u32	reserved2[5];
};

enum mlx5_exp_ib_alloc_ucontext_data_resp_mask {
	MLX5_EXP_ALLOC_CTX_RESP_MASK_CQE_COMP_MAX_NUM		= 1 << 0,
	MLX5_EXP_ALLOC_CTX_RESP_MASK_CQE_VERSION		= 1 << 1,
	MLX5_EXP_ALLOC_CTX_RESP_MASK_RROCE_UDP_SPORT_MIN	= 1 << 2,
	MLX5_EXP_ALLOC_CTX_RESP_MASK_RROCE_UDP_SPORT_MAX	= 1 << 3,
	MLX5_EXP_ALLOC_CTX_RESP_MASK_HCA_CORE_CLOCK_OFFSET	= 1 << 4,
};

struct mlx5_exp_ib_alloc_ucontext_data_resp {
	__u32   comp_mask; /* use mlx5_ib_exp_alloc_ucontext_data_resp_mask */
	__u16	cqe_comp_max_num;
	__u8	cqe_version;
	__u8	reserved;
	__u16	rroce_udp_sport_min;
	__u16	rroce_udp_sport_max;
	__u32	hca_core_clock_offset;
};

struct mlx5_exp_ib_alloc_ucontext_resp {
	__u32						qp_tab_size;
	__u32						bf_reg_size;
	__u32						tot_uuars;
	__u32						cache_line_size;
	__u16						max_sq_desc_sz;
	__u16						max_rq_desc_sz;
	__u32						max_send_wqebb;
	__u32						max_recv_wr;
	__u32						max_srq_recv_wr;
	__u16						num_ports;
	__u16						reserved;
	__u32						max_desc_sz_sq_dc;
	__u32						atomic_arg_sizes_dc;
	__u32						reserved1;
	__u32						flags;
	__u32						reserved2[5];
	/* Some more reserved fields for
	 * future growth of mlx5_ib_alloc_ucontext_resp */
	__u64						prefix_reserved[8];
	struct mlx5_exp_ib_alloc_ucontext_data_resp	exp_data;
};

struct mlx5_ib_alloc_pd_resp {
	__u32	pdn;
};

struct mlx5_ib_create_cq {
	__u64	buf_addr;
	__u64	db_addr;
	__u32	cqe_size;
	__u32	reserved; /* explicit padding (optional on i386) */
};

enum mlx5_exp_ib_create_cq_mask {
	MLX5_EXP_CREATE_CQ_MASK_CQE_COMP_EN		= 1 << 0,
	MLX5_EXP_CREATE_CQ_MASK_CQE_COMP_RECV_TYPE      = 1 << 1,
	MLX5_EXP_CREATE_CQ_MASK_RESERVED		= 1 << 2,
};

enum mlx5_exp_cqe_comp_recv_type {
	MLX5_IB_CQE_FORMAT_HASH,
	MLX5_IB_CQE_FORMAT_CSUM,
};

struct mlx5_exp_ib_create_cq_data {
	__u32   comp_mask; /* use mlx5_exp_ib_creaet_cq_mask */
	__u8    cqe_comp_en;
	__u8    cqe_comp_recv_type; /* use mlx5_exp_cqe_comp_recv_type */
	__u16	reserved;
};

struct mlx5_exp_ib_create_cq {
	__u64					buf_addr;
	__u64					db_addr;
	__u32					cqe_size;
	__u32					reserved; /* explicit padding (optional on i386) */

	/* Some more reserved fields for future growth of mlx5_ib_create_cq */
	__u64					prefix_reserved[8];

	/* sizeof prefix aligned with mlx5_ib_create_cq */
	__u64					size_of_prefix;
	struct mlx5_exp_ib_create_cq_data	exp_data;
};

struct mlx5_ib_create_cq_resp {
	__u32	cqn;
	__u32	reserved;
};

struct mlx5_ib_resize_cq {
	__u64	buf_addr;
	__u16	cqe_size;
	__u16	reserved0;
	__u32	reserved1;
};

struct mlx5_ib_create_srq {
	__u64	buf_addr;
	__u64	db_addr;
	__u32	flags;
	__u32	reserved; /* explicit padding (optional on i386) */
	__u32   uidx;
	__u32   reserved1;
};

struct mlx5_ib_create_srq_resp {
	__u32	srqn;
	__u32	reserved;
};

struct mlx5_ib_create_qp {
	__u64	buf_addr;
	__u64	db_addr;
	__u32	sq_wqe_count;
	__u32	rq_wqe_count;
	__u32	rq_wqe_shift;
	__u32	flags;
};

enum mlx5_exp_ib_create_qp_mask {
	MLX5_EXP_CREATE_QP_MASK_UIDX		= 1 << 0,
	MLX5_EXP_CREATE_QP_MASK_SQ_BUFF_ADD	= 1 << 1,
	MLX5_EXP_CREATE_QP_MASK_WC_UAR_IDX	= 1 << 2,
	MLX5_EXP_CREATE_QP_MASK_FLAGS_IDX	= 1 << 3,
	MLX5_EXP_CREATE_QP_MASK_RESERVED	= 1 << 4,
};

enum mlx5_exp_create_qp_flags {
	MLX5_EXP_CREATE_QP_MULTI_PACKET_WQE_REQ_FLAG = 1 << 0,
};

enum mlx5_exp_drv_create_qp_uar_idx {
	MLX5_EXP_CREATE_QP_DB_ONLY_UUAR = -1
};

struct mlx5_exp_ib_create_qp_data {
	__u32   comp_mask; /* use mlx5_exp_ib_create_qp_mask */
	__u32   uidx;
	__u64	sq_buf_addr;
	__u32   wc_uar_index;
	__u32   flags; /* use mlx5_exp_create_qp_flags */
};

struct mlx5_exp_ib_create_qp {
	/* To allow casting to mlx5_ib_create_qp the prefix is the same as
	 * struct mlx5_ib_create_qp prefix
	 */
	__u64	buf_addr;
	__u64	db_addr;
	__u32	sq_wqe_count;
	__u32	rq_wqe_count;
	__u32	rq_wqe_shift;
	__u32	flags;

	/* Some more reserved fields for future growth of mlx5_ib_create_qp */
	__u64   prefix_reserved[8];

	/* sizeof prefix aligned with mlx5_ib_create_qp */
	__u64   size_of_prefix;

	/* Experimental data
	 * Add new experimental data only inside the exp struct
	 */
	struct mlx5_exp_ib_create_qp_data exp;
};

enum {
	MLX5_EXP_INVALID_UUAR = -1,
};

struct mlx5_ib_create_qp_resp {
	__u32	uuar_index;
	__u32	rsvd;
};

enum mlx5_exp_ib_create_qp_resp_mask {
	MLX5_EXP_CREATE_QP_RESP_MASK_FLAGS_IDX	= 1 << 0,
	MLX5_EXP_CREATE_QP_RESP_MASK_RESERVED	= 1 << 1,
};

enum mlx5_exp_create_qp_resp_flags {
	MLX5_EXP_CREATE_QP_RESP_MULTI_PACKET_WQE_FLAG = 1 << 0,
};

struct mlx5_exp_ib_create_qp_resp_data {
	__u32   comp_mask; /* use mlx5_exp_ib_create_qp_resp_mask */
	__u32   flags; /* use mlx5_exp_create_qp_resp_flags */
};

struct mlx5_exp_ib_create_qp_resp {
	__u32	uuar_index;
	__u32	rsvd;

	/* Some more reserved fields for future growth of mlx5_ib_create_qp_resp */
	__u64   prefix_reserved[8];

	/* sizeof prefix aligned with mlx5_ib_create_qp_resp */
	__u64   size_of_prefix;

	/* Experimental data
	 * Add new experimental data only inside the exp struct
	 */
	struct mlx5_exp_ib_create_qp_resp_data exp;
};

struct mlx5_ib_create_dct {
	__u32   uidx;
	__u32   reserved;
};

struct mlx5_ib_arm_dct {
	__u64	reserved0;
	__u64	reserved1;
};

struct mlx5_ib_arm_dct_resp {
	__u64	reserved0;
	__u64	reserved1;
};

struct mlx5_ib_create_wq {
	__u64	buf_addr;
	__u64	db_addr;
	__u32	rq_wqe_count;
	__u32	rq_wqe_shift;
	__u32	user_index;
	__u32	flags;
};

#endif /* MLX5_IB_USER_H */
