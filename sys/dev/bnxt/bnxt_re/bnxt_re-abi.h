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
 * Description: Uverbs ABI header file
 */

#ifndef __BNXT_RE_UVERBS_ABI_H__
#define __BNXT_RE_UVERBS_ABI_H__

#include <asm/types.h>
#include <linux/types.h>

#define BNXT_RE_ABI_VERSION	6

enum {
	BNXT_RE_COMP_MASK_UCNTX_WC_DPI_ENABLED = 0x01,
	BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED = 0x02,
	BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED = 0x04,
	BNXT_RE_COMP_MASK_UCNTX_MQP_EX_SUPPORTED = 0x08,
	BNXT_RE_COMP_MASK_UCNTX_DBR_PACING_ENABLED = 0x10,
	BNXT_RE_COMP_MASK_UCNTX_DBR_RECOVERY_ENABLED = 0x20,
	BNXT_RE_COMP_MASK_UCNTX_HW_RETX_ENABLED = 0x40
};

enum {
	BNXT_RE_COMP_MASK_REQ_UCNTX_POW2_SUPPORT = 0x01,
	BNXT_RE_COMP_MASK_REQ_UCNTX_RSVD_WQE = 0x02
};

struct bnxt_re_uctx_req {
	__aligned_u64 comp_mask;
};

#define BNXT_RE_CHIP_ID0_CHIP_NUM_SFT		0x00
#define BNXT_RE_CHIP_ID0_CHIP_REV_SFT		0x10
#define BNXT_RE_CHIP_ID0_CHIP_MET_SFT		0x18
struct bnxt_re_uctx_resp {
	__u32 dev_id;
	__u32 max_qp;
	__u32 pg_size;
	__u32 cqe_sz;
	__u32 max_cqd;
	__u32 chip_id0;
	__u32 chip_id1;
	__u32 modes;
	__aligned_u64 comp_mask;
} __attribute__((packed));

enum {
	BNXT_RE_COMP_MASK_PD_HAS_WC_DPI = 0x01,
	BNXT_RE_COMP_MASK_PD_HAS_DBR_BAR_ADDR = 0x02,
};

struct bnxt_re_pd_resp {
	__u32 pdid;
	__u32 dpi;
	__u64 dbr;
	__u64 comp_mask;
	__u32 wcdpi;
	__u64 dbr_bar_addr;
} __attribute__((packed));

enum {
	BNXT_RE_COMP_MASK_CQ_HAS_DB_INFO = 0x01,
	BNXT_RE_COMP_MASK_CQ_HAS_WC_DPI = 0x02,
	BNXT_RE_COMP_MASK_CQ_HAS_CQ_PAGE = 0x04,
};

enum {
	BNXT_RE_COMP_MASK_CQ_REQ_HAS_CAP_MASK = 0x1
};

enum {
	BNXT_RE_COMP_MASK_CQ_REQ_CAP_DBR_RECOVERY = 0x1,
	BNXT_RE_COMP_MASK_CQ_REQ_CAP_DBR_PACING_NOTIFY = 0x2
};

#define BNXT_RE_IS_DBR_PACING_NOTIFY_CQ(_req)				\
	(_req.comp_mask & BNXT_RE_COMP_MASK_CQ_REQ_HAS_CAP_MASK &&	\
	 _req.cq_capability & BNXT_RE_COMP_MASK_CQ_REQ_CAP_DBR_PACING_NOTIFY)

#define BNXT_RE_IS_DBR_RECOV_CQ(_req)					\
	(_req.comp_mask & BNXT_RE_COMP_MASK_CQ_REQ_HAS_CAP_MASK &&	\
	 _req.cq_capability & BNXT_RE_COMP_MASK_CQ_REQ_CAP_DBR_RECOVERY)

struct bnxt_re_cq_req {
	__u64 cq_va;
	__u64 cq_handle;
	__aligned_u64 comp_mask;
	__u16 cq_capability;
} __attribute__((packed));

struct bnxt_re_cq_resp {
	__u32 cqid;
	__u32 tail;
	__u32 phase;
	__u32 rsvd;
	__aligned_u64 comp_mask;
	__u32 dpi;
	__u64 dbr;
	__u32 wcdpi;
	__u64 uctx_cq_page;
} __attribute__((packed));

struct bnxt_re_resize_cq_req {
	__u64 cq_va;
} __attribute__((packed));

struct bnxt_re_qp_req {
	__u64 qpsva;
	__u64 qprva;
	__u64 qp_handle;
} __attribute__((packed));

struct bnxt_re_qp_resp {
	__u32 qpid;
} __attribute__((packed));

struct bnxt_re_srq_req {
	__u64 srqva;
	__u64 srq_handle;
} __attribute__((packed));

struct bnxt_re_srq_resp {
	__u32 srqid;
} __attribute__((packed));

/* Modify QP */
enum {
	BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN_MASK = 0x1,
	BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN	 = 0x1,
	BNXT_RE_COMP_MASK_MQP_EX_PATH_MTU_MASK	 = 0x2
};

struct bnxt_re_modify_qp_ex_req {
	__aligned_u64 comp_mask;
	__u32 dpi;
	__u32 rsvd;
} __packed;

struct bnxt_re_modify_qp_ex_resp {
	__aligned_u64 comp_mask;
	__u32 ppp_st_idx;
	__u32 path_mtu;
} __packed;

enum bnxt_re_shpg_offt {
	BNXT_RE_BEG_RESV_OFFT	= 0x00,
	BNXT_RE_AVID_OFFT	= 0x10,
	BNXT_RE_AVID_SIZE	= 0x04,
	BNXT_RE_END_RESV_OFFT	= 0xFF0
};
#endif
