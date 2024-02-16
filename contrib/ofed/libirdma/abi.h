/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (C) 2019 - 2023 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PROVIDER_IRDMA_ABI_H
#define PROVIDER_IRDMA_ABI_H

#include "irdma.h"
#include <infiniband/kern-abi.h>
#include "irdma-abi.h"

#define IRDMA_MIN_ABI_VERSION	0
#define IRDMA_MAX_ABI_VERSION	5

struct irdma_ualloc_pd_resp {
	struct ibv_alloc_pd_resp	ibv_resp;
	__u32 pd_id;
	__u8 rsvd[4];

};
struct irdma_ucreate_cq {
	struct ibv_create_cq	ibv_cmd;
	__aligned_u64 user_cq_buf;
	__aligned_u64 user_shadow_area;

};
struct irdma_ucreate_cq_resp {
	struct ibv_create_cq_resp	ibv_resp;
	__u32 cq_id;
	__u32 cq_size;

};
struct irdma_ucreate_cq_ex {
	struct ibv_create_cq_ex	ibv_cmd;
	__aligned_u64 user_cq_buf;
	__aligned_u64 user_shadow_area;

};
struct irdma_ucreate_cq_ex_resp {
	struct ibv_create_cq_resp_ex	ibv_resp;
	__u32 cq_id;
	__u32 cq_size;

};
struct irdma_uresize_cq {
	struct ibv_resize_cq	ibv_cmd;
	__aligned_u64 user_cq_buffer;

};
struct irdma_uresize_cq_resp {
	struct ibv_resize_cq_resp	ibv_resp;

};
struct irdma_ucreate_qp {
	struct ibv_create_qp	ibv_cmd;
	__aligned_u64 user_wqe_bufs;
	__aligned_u64 user_compl_ctx;
	__aligned_u64 comp_mask;

};
struct irdma_ucreate_qp_resp {
	struct ibv_create_qp_resp	ibv_resp;
	__u32 qp_id;
	__u32 actual_sq_size;
	__u32 actual_rq_size;
	__u32 irdma_drv_opt;
	__u16 push_idx;
	__u8 lsmm;
	__u8 rsvd;
	__u32 qp_caps;
	__aligned_u64 comp_mask;
	__u8 start_wqe_idx;
	__u8 rsvd2[7];

};
struct irdma_umodify_qp_resp {
	struct ibv_modify_qp_resp_ex	ibv_resp;
	__aligned_u64 push_wqe_mmap_key;
	__aligned_u64 push_db_mmap_key;
	__u16 push_offset;
	__u8 push_valid;
	__u8 rd_fence_rate;
	__u8 rsvd[4];

};
struct irdma_get_context {
	struct ibv_get_context	ibv_cmd;
	__u32 rsvd32;
	__u8 userspace_ver;
	__u8 rsvd8[3];
	__aligned_u64 comp_mask;

};
struct irdma_get_context_resp {
	struct ibv_get_context_resp	ibv_resp;
	__u32 max_pds;
	__u32 max_qps;
	__u32 wq_size; /* size of the WQs (SQ+RQ) in the mmaped area */
	__u8 kernel_ver;
	__u8 rsvd[3];
	__aligned_u64 feature_flags;
	__aligned_u64 db_mmap_key;
	__u32 max_hw_wq_frags;
	__u32 max_hw_read_sges;
	__u32 max_hw_inline;
	__u32 max_hw_rq_quanta;
	__u32 max_hw_wq_quanta;
	__u32 min_hw_cq_size;
	__u32 max_hw_cq_size;
	__u16 max_hw_sq_chunk;
	__u8 hw_rev;
	__u8 rsvd2;
	__aligned_u64 comp_mask;
	__u16 min_hw_wq_size;
	__u8 rsvd3[6];

};
struct irdma_ureg_mr {
	struct ibv_reg_mr	ibv_cmd;
	__u16 reg_type; /* enum irdma_memreg_type */
	__u16 cq_pages;
	__u16 rq_pages;
	__u16 sq_pages;

};
struct irdma_urereg_mr {
	struct ibv_rereg_mr	ibv_cmd;
	__u16 reg_type; /* enum irdma_memreg_type */
	__u16 cq_pages;
	__u16 rq_pages;
	__u16 sq_pages;

};
struct irdma_ucreate_ah_resp {
	struct ibv_create_ah_resp	ibv_resp;
	__u32 ah_id;
	__u8 rsvd[4];

};

struct irdma_modify_qp_cmd {
	struct ibv_modify_qp_ex ibv_cmd;
	__u8 sq_flush;
	__u8 rq_flush;
	__u8 rsvd[6];
};

struct irdma_query_device_ex {
	struct ibv_query_device_ex ibv_cmd;
};

struct irdma_query_device_ex_resp {
	struct ibv_query_device_resp_ex ibv_resp;
	__u32				comp_mask;
	__u32				response_length;
	struct ibv_odp_caps_resp	odp_caps;
	__u64				timestamp_mask;
	__u64				hca_core_clock;
	__u64				device_cap_flags_ex;
	struct ibv_rss_caps_resp	rss_caps;
	__u32				max_wq_type_rq;
	__u32				raw_packet_caps;
	struct ibv_tso_caps		tso_caps;
};
#endif /* PROVIDER_IRDMA_ABI_H */
