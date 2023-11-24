/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2017 - 2022 Intel Corporation
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
/*$FreeBSD$*/

#ifndef IRDMA_H
#define IRDMA_H

#define RDMA_BIT2(type, a) ((u##type) 1UL << a)
#define RDMA_MASK3(type, mask, shift)	((u##type) mask << shift)
#define MAKEMASK(m, s) ((m) << (s))

#define IRDMA_WQEALLOC_WQE_DESC_INDEX_S 20
#define IRDMA_WQEALLOC_WQE_DESC_INDEX GENMASK(31, 20)

enum irdma_vers {
	IRDMA_GEN_RSVD = 0,
	IRDMA_GEN_1 = 1,
	IRDMA_GEN_2 = 2,
	IRDMA_GEN_MAX = 2,
};

struct irdma_uk_attrs {
	u64 feature_flags;
	u32 max_hw_wq_frags;
	u32 max_hw_read_sges;
	u32 max_hw_inline;
	u32 max_hw_rq_quanta;
	u32 max_hw_wq_quanta;
	u32 min_hw_cq_size;
	u32 max_hw_cq_size;
	u16 max_hw_sq_chunk;
	u16 min_hw_wq_size;
	u8 hw_rev;
};

struct irdma_hw_attrs {
	struct irdma_uk_attrs uk_attrs;
	u64 max_hw_outbound_msg_size;
	u64 max_hw_inbound_msg_size;
	u64 max_mr_size;
	u64 page_size_cap;
	u32 min_hw_qp_id;
	u32 min_hw_aeq_size;
	u32 max_hw_aeq_size;
	u32 min_hw_ceq_size;
	u32 max_hw_ceq_size;
	u32 max_hw_device_pages;
	u32 max_hw_vf_fpm_id;
	u32 first_hw_vf_fpm_id;
	u32 max_hw_ird;
	u32 max_hw_ord;
	u32 max_hw_wqes;
	u32 max_hw_pds;
	u32 max_hw_ena_vf_count;
	u32 max_qp_wr;
	u32 max_pe_ready_count;
	u32 max_done_count;
	u32 max_sleep_count;
	u32 max_cqp_compl_wait_time_ms;
	u16 max_stat_inst;
	u16 max_stat_idx;
};

#endif /* IRDMA_H*/
