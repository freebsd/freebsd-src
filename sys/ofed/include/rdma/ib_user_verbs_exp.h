/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
 * Copyright (c) 2006 Mellanox Technologies.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
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

#ifndef IB_USER_VERBS_EXP_H
#define IB_USER_VERBS_EXP_H

#include <rdma/ib_user_verbs.h>

enum {
	IB_USER_VERBS_EXP_CMD_FIRST = 64
};

enum {
	IB_USER_VERBS_EXP_CMD_CREATE_QP,
	IB_USER_VERBS_EXP_CMD_MODIFY_CQ,
	IB_USER_VERBS_EXP_CMD_MODIFY_QP,
	IB_USER_VERBS_EXP_CMD_CREATE_CQ,
	IB_USER_VERBS_EXP_CMD_QUERY_DEVICE,
	IB_USER_VERBS_EXP_CMD_CREATE_DCT,
	IB_USER_VERBS_EXP_CMD_DESTROY_DCT,
	IB_USER_VERBS_EXP_CMD_QUERY_DCT,
};

/*
 * Make sure that all structs defined in this file remain laid out so
 * that they pack the same way on 32-bit and 64-bit architectures (to
 * avoid incompatibility between 32-bit userspace and 64-bit kernels).
 * Specifically:
 *  - Do not use pointer types -- pass pointers in __u64 instead.
 *  - Make sure that any structure larger than 4 bytes is padded to a
 *    multiple of 8 bytes.  Otherwise the structure size will be
 *    different between 32-bit and 64-bit architectures.
 */

enum ib_uverbs_exp_create_qp_comp_mask {
	IB_UVERBS_EXP_CREATE_QP_CAP_FLAGS          = (1ULL << 0),
	IB_UVERBS_EXP_CREATE_QP_INL_RECV           = (1ULL << 1),
	IB_UVERBS_EXP_CREATE_QP_QPG                = (1ULL << 2)
};

struct ib_uverbs_qpg_init_attrib {
	__u32 tss_child_count;
	__u32 rss_child_count;
};

struct ib_uverbs_qpg {
	__u32 qpg_type;
	union {
		struct {
			__u32 parent_handle;
			__u32 reserved;
		};
		struct ib_uverbs_qpg_init_attrib parent_attrib;
	};
	__u32 reserved2;
};

struct ib_uverbs_exp_create_qp {
	__u64 comp_mask;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 send_cq_handle;
	__u32 recv_cq_handle;
	__u32 srq_handle;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u8  sq_sig_all;
	__u8  qp_type;
	__u8  is_srq;
	__u8  reserved;
	__u64 qp_cap_flags;
	__u32 max_inl_recv;
	__u32 reserved1;
	struct ib_uverbs_qpg qpg;
	__u64 driver_data[0];
};

enum ib_uverbs_exp_create_qp_resp_comp_mask {
	IB_UVERBS_EXP_CREATE_QP_RESP_INL_RECV	= (1ULL << 0),
};

struct ib_uverbs_exp_create_qp_resp {
	__u64 comp_mask;
	__u32 qp_handle;
	__u32 qpn;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 max_inl_recv;
};

struct ib_uverbs_create_dct {
	__u64	comp_mask;
	__u64	user_handle;
	__u32	pd_handle;
	__u32	cq_handle;
	__u32	srq_handle;
	__u32	access_flags;
	__u32	flow_label;
	__u64	dc_key;
	__u8	min_rnr_timer;
	__u8	tclass;
	__u8	port;
	__u8	pkey_index;
	__u8	gid_index;
	__u8	hop_limit;
	__u8	mtu;
	__u8	rsvd;
	__u32	create_flags;
	__u64	driver_data[0];
};

struct ib_uverbs_create_dct_resp {
	__u32 dct_handle;
	__u32 dctn;
};

struct ib_uverbs_destroy_dct {
	__u64 comp_mask;
	__u64 user_handle;
};

struct ib_uverbs_destroy_dct_resp {
	__u64	reserved;
};

struct ib_uverbs_query_dct {
	__u64	comp_mask;
	__u64	dct_handle;
	__u64	driver_data[0];
};

struct ib_uverbs_query_dct_resp {
	__u64	dc_key;
	__u32	access_flags;
	__u32	flow_label;
	__u32	key_violations;
	__u8	port;
	__u8	min_rnr_timer;
	__u8	tclass;
	__u8	mtu;
	__u8	pkey_index;
	__u8	gid_index;
	__u8	hop_limit;
	__u8	state;
	__u32	rsvd;
	__u64	driver_data[0];
};

struct ib_uverbs_exp_query_device {
	__u64 comp_mask;
	__u64 driver_data[0];
};

struct ib_uverbs_exp_query_device_resp {
	__u64					comp_mask;
	struct ib_uverbs_query_device_resp	base;
	__u64					timestamp_mask;
	__u64					hca_core_clock;
	__u64					device_cap_flags2;
	__u32					dc_rd_req;
	__u32					dc_rd_res;
	__u32					inline_recv_sz;
	__u32					max_rss_tbl_sz;
};

#endif /* IB_USER_VERBS_EXP_H */
