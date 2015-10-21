/*
 * Copyright (c) 2004 Mellanox Technologies Ltd.  All rights reserved.
 * Copyright (c) 2004 Infinicon Corporation.  All rights reserved.
 * Copyright (c) 2004 Intel Corporation.  All rights reserved.
 * Copyright (c) 2004 Topspin Corporation.  All rights reserved.
 * Copyright (c) 2004 Voltaire Corporation.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005, 2006, 2007 Cisco Systems.  All rights reserved.
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

#ifndef IB_VERBS_EXP_H
#define IB_VERBS_EXP_H

#include <rdma/ib_verbs.h>


enum ib_exp_device_cap_flags2 {
	IB_EXP_DEVICE_DC_TRANSPORT		= 1 << 0,
	IB_EXP_DEVICE_QPG			= 1 << 1,
	IB_EXP_DEVICE_UD_RSS			= 1 << 2,
	IB_EXP_DEVICE_UD_TSS			= 1 << 3
};

enum ib_exp_device_attr_comp_mask {
	IB_EXP_DEVICE_ATTR_WITH_TIMESTAMP_MASK	= 1ULL << 1,
	IB_EXP_DEVICE_ATTR_WITH_HCA_CORE_CLOCK	= 1ULL << 2,
	IB_EXP_DEVICE_ATTR_CAP_FLAGS2		= 1ULL << 3,
	IB_EXP_DEVICE_ATTR_DC_REQ_RD		= 1ULL << 4,
	IB_EXP_DEVICE_ATTR_DC_RES_RD		= 1ULL << 5,
	IB_EXP_DEVICE_ATTR_INLINE_RECV_SZ	= 1ULL << 6,
	IB_EXP_DEVICE_ATTR_RSS_TBL_SZ		= 1ULL << 7,
};

struct ib_exp_device_attr {
	struct ib_device_attr	base;
	/* Use IB_EXP_DEVICE_ATTR_... for exp_comp_mask */
	uint32_t		exp_comp_mask;
	uint64_t		device_cap_flags2;
	uint32_t		dc_rd_req;
	uint32_t		dc_rd_res;
	uint32_t		inline_recv_sz;
	uint32_t		max_rss_tbl_sz;
};

struct ib_exp_qp_init_attr {
	void                  (*event_handler)(struct ib_event *, void *);
	void		       *qp_context;
	struct ib_cq	       *send_cq;
	struct ib_cq	       *recv_cq;
	struct ib_srq	       *srq;
	struct ib_xrcd	       *xrcd;     /* XRC TGT QPs only */
	struct ib_qp_cap	cap;
	union {
		struct ib_qp *qpg_parent; /* see qpg_type */
		struct ib_qpg_init_attrib parent_attrib;
	};
	enum ib_sig_type	sq_sig_type;
	enum ib_qp_type		qp_type;
	enum ib_qp_create_flags	create_flags;
	enum ib_qpg_type	qpg_type;
	u8			port_num; /* special QP types only */
	u32			max_inl_recv;
};


int ib_exp_query_device(struct ib_device *device,
			struct ib_exp_device_attr *device_attr);




#endif /* IB_VERBS_EXP_H */
