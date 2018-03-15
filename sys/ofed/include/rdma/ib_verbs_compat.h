/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018, Mellanox Technologies, Ltd.  All rights reserved.
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

#if !defined(IB_VERBS_COMPAT_H)
#define	IB_VERBS_COMPAT_H

#include <rdma/ib_verbs.h>

enum ib_device_attr_comp_mask {
	IB_DEVICE_ATTR_WITH_TIMESTAMP_MASK = 1ULL << 1,
	IB_DEVICE_ATTR_WITH_HCA_CORE_CLOCK = 1ULL << 2
};

struct ib_protocol_stats {
	/* TBD... */
};

struct iw_protocol_stats {
	u64	ipInReceives;
	u64	ipInHdrErrors;
	u64	ipInTooBigErrors;
	u64	ipInNoRoutes;
	u64	ipInAddrErrors;
	u64	ipInUnknownProtos;
	u64	ipInTruncatedPkts;
	u64	ipInDiscards;
	u64	ipInDelivers;
	u64	ipOutForwDatagrams;
	u64	ipOutRequests;
	u64	ipOutDiscards;
	u64	ipOutNoRoutes;
	u64	ipReasmTimeout;
	u64	ipReasmReqds;
	u64	ipReasmOKs;
	u64	ipReasmFails;
	u64	ipFragOKs;
	u64	ipFragFails;
	u64	ipFragCreates;
	u64	ipInMcastPkts;
	u64	ipOutMcastPkts;
	u64	ipInBcastPkts;
	u64	ipOutBcastPkts;

	u64	tcpRtoAlgorithm;
	u64	tcpRtoMin;
	u64	tcpRtoMax;
	u64	tcpMaxConn;
	u64	tcpActiveOpens;
	u64	tcpPassiveOpens;
	u64	tcpAttemptFails;
	u64	tcpEstabResets;
	u64	tcpCurrEstab;
	u64	tcpInSegs;
	u64	tcpOutSegs;
	u64	tcpRetransSegs;
	u64	tcpInErrs;
	u64	tcpOutRsts;
};

union rdma_protocol_stats {
	struct ib_protocol_stats ib;
	struct iw_protocol_stats iw;
};

enum ib_mr_create_flags {
	IB_MR_SIGNATURE_EN = 1,
};

struct ib_mr_init_attr {
	int	max_reg_descriptors;
	u32	flags;
};

enum ib_qpg_type {
	IB_QPG_NONE = 0,
	IB_QPG_PARENT = (1 << 0),
	IB_QPG_CHILD_RX = (1 << 1),
	IB_QPG_CHILD_TX = (1 << 2)
};

struct ib_qpg_init_attrib {
	u32	tss_child_count;
	u32	rss_child_count;
};

enum {
	IB_DCT_CREATE_FLAG_RCV_INLINE = 1 << 0,
	IB_DCT_CREATE_FLAGS_MASK = IB_DCT_CREATE_FLAG_RCV_INLINE,
};

struct ib_dct_init_attr {
	struct ib_pd *pd;
	struct ib_cq *cq;
	struct ib_srq *srq;
	u64	dc_key;
	u8	port;
	u32	access_flags;
	u8	min_rnr_timer;
	u8	tclass;
	u32	flow_label;
	enum ib_mtu mtu;
	u8	pkey_index;
	u8	gid_index;
	u8	hop_limit;
	u32	create_flags;
};

struct ib_dct_attr {
	u64	dc_key;
	u8	port;
	u32	access_flags;
	u8	min_rnr_timer;
	u8	tclass;
	u32	flow_label;
	enum ib_mtu mtu;
	u8	pkey_index;
	u8	gid_index;
	u8	hop_limit;
	u32	key_violations;
	u8	state;
};

struct ib_fast_reg_page_list {
	struct ib_device *device;
	u64    *page_list;
	unsigned int max_page_list_len;
};

struct ib_mw_bind_info {
	struct ib_mr *mr;
	u64	addr;
	u64	length;
	int	mw_access_flags;
};

struct ib_mr_attr {
	struct ib_pd *pd;
	u64	device_virt_addr;
	u64	size;
	int	mr_access_flags;
	u32	lkey;
	u32	rkey;
};

struct ib_mw_bind {
	u64	wr_id;
	int	send_flags;
	struct ib_mw_bind_info bind_info;
};

enum ib_cq_attr_mask {
	IB_CQ_MODERATION = (1 << 0),
	IB_CQ_CAP_FLAGS = (1 << 1)
};

enum ib_cq_cap_flags {
	IB_CQ_IGNORE_OVERRUN = (1 << 0)
};

struct ib_cq_attr {
	struct {
		u16	cq_count;
		u16	cq_period;
	}	moderation;
	u32	cq_cap_flags;
};

struct ib_dct {
	struct ib_device *device;
	struct ib_uobject *uobject;
	struct ib_pd *pd;
	struct ib_cq *cq;
	struct ib_srq *srq;
	u32	dct_num;
};

enum verbs_values_mask {
	IBV_VALUES_HW_CLOCK = 1 << 0
};

struct ib_device_values {
	int	values_mask;
	uint64_t hwclock;
};

#define	IB_WR_FAST_REG_MR -2		/* not implemented */

struct ib_send_wr_compat {
	union {
		/*
		 * NOTE: The following structure must be kept in sync
		 * with "struct ib_send_wr":
		 */
		struct {
			struct ib_send_wr_compat *next;
			union {
				u64	wr_id;
				struct ib_cqe *wr_cqe;
			};
			struct ib_sge *sg_list;
			int	num_sge;
			enum ib_wr_opcode opcode;
			int	send_flags;
			union {
				__be32	imm_data;
				u32	invalidate_rkey;
			}	ex;
		};
		union {
			struct ib_rdma_wr rdma;
			struct ib_atomic_wr atomic;
			struct ib_ud_wr ud;
			struct ib_sig_handover_wr sig_handover;
			struct {
				struct ib_send_wr wr;
				u64	iova_start;
				struct ib_fast_reg_page_list *page_list;
				unsigned int page_shift;
				unsigned int page_list_len;
				u32	length;
				int	access_flags;
				u32	rkey;
			}	fast_reg;
			struct {
				struct ib_send_wr wr;
				int	npages;
				int	access_flags;
				u32	mkey;
				struct ib_pd *pd;
				u64	virt_addr;
				u64	length;
				int	page_shift;
			}	umr;
			struct {
				struct ib_send_wr wr;
				struct ib_mw *mw;
				/* The new rkey for the memory window. */
				u32	rkey;
				struct ib_mw_bind_info bind_info;
			}	bind_mw;
		}	wr;
	};
	u32	xrc_remote_srq_num;	/* XRC TGT QPs only */
};

static inline int
ib_post_send_compat(struct ib_qp *qp,
    struct ib_send_wr_compat *send_wr,
    struct ib_send_wr_compat **bad_send_wr)
{
	return (ib_post_send(qp, (struct ib_send_wr *)send_wr,
	    (struct ib_send_wr **)bad_send_wr));
}

#undef ib_post_send
#define	ib_post_send(...) \
	ib_post_send_compat(__VA_ARGS__)

#define	ib_send_wr \
	ib_send_wr_compat

static inline int
ib_query_device_compat(struct ib_device *device,
    struct ib_device_attr *device_attr)
{
	*device_attr = device->attrs;
	return (0);
}

#undef ib_query_device
#define	ib_query_device(...) \
	ib_query_device_compat(__VA_ARGS__)

static inline int
ib_query_gid_compat(struct ib_device *device,
    u8 port_num, int index, union ib_gid *gid)
{
	return (ib_query_gid(device, port_num, index, gid, NULL));
}

#undef ib_query_gid
#define	ib_query_gid(...) \
	ib_query_gid_compat(__VA_ARGS__)

static inline int
ib_find_gid_compat(struct ib_device *device, union ib_gid *gid,
    u8 * port_num, u16 * index)
{
	return (ib_find_gid(device, gid, IB_GID_TYPE_IB, NULL, port_num, index));
}

#undef ib_find_gid
#define	ib_find_gid(...) \
	ib_find_gid_compat(__VA_ARGS__)

static inline struct ib_pd *
ib_alloc_pd_compat(struct ib_device *device)
{
	return (ib_alloc_pd(device, 0));
}

#undef ib_alloc_pd
#define	ib_alloc_pd(...) \
	ib_alloc_pd_compat(__VA_ARGS__)

static inline struct ib_cq *
ib_create_cq_compat(struct ib_device *device,
    ib_comp_handler comp_handler,
    void (*event_handler) (struct ib_event *, void *),
    void *cq_context, int cqe, int comp_vector)
{
	const struct ib_cq_init_attr cq_attr = {.cqe = cqe,.comp_vector = comp_vector};

	return (ib_create_cq(device, comp_handler, event_handler, cq_context, &cq_attr));
}

#undef ib_create_cq
#define	ib_create_cq(...) \
	ib_create_cq_compat(__VA_ARGS__)

static inline int
ib_modify_cq_compat(struct ib_cq *cq,
    struct ib_cq_attr *cq_attr,
    int cq_attr_mask)
{
	if (cq_attr_mask & IB_CQ_MODERATION) {
		return (ib_modify_cq(cq, cq_attr->moderation.cq_count,
		    cq_attr->moderation.cq_period));
	} else {
		return (0);
	}
}

#undef ib_modify_cq
#define	ib_modify_cq(...) \
	ib_modify_cq_compat(__VA_ARGS__)

static inline struct ib_mr *
ib_get_dma_mr(struct ib_pd *pd, int mr_access_flags)
{
	struct ib_mr *mr;
	int err;

	err = ib_check_mr_access(mr_access_flags);
	if (err)
		return ERR_PTR(err);

	if (!pd->device->get_dma_mr)
		return ERR_PTR(-ENOSYS);

	mr = pd->device->get_dma_mr(pd, mr_access_flags);
	if (IS_ERR(mr))
		return ERR_CAST(mr);

	mr->device = pd->device;
	mr->pd = pd;
	mr->uobject = NULL;
	mr->need_inval = false;
	atomic_inc(&pd->usecnt);

	return (mr);
}

static inline struct ib_mr *
ib_reg_phys_mr(struct ib_pd *pd,
    struct ib_phys_buf *phys_buf_array,
    int num_phys_buf,
    int mr_access_flags,
    u64 * iova_start)
{
	struct ib_mr *mr;
	int err;

	err = ib_check_mr_access(mr_access_flags);
	if (err)
		return ERR_PTR(err);

	if (!pd->device->reg_phys_mr)
		return ERR_PTR(-ENOSYS);

	mr = pd->device->reg_phys_mr(pd, phys_buf_array, num_phys_buf,
				     mr_access_flags, iova_start);
	if (IS_ERR(mr))
		return ERR_CAST(mr);

	mr->device = pd->device;
	mr->pd = pd;
	mr->uobject = NULL;
	atomic_inc(&pd->usecnt);

	return (mr);
}

static inline int
ib_rereg_phys_mr(struct ib_mr *mr,
    int mr_rereg_mask,
    struct ib_pd *pd,
    struct ib_phys_buf *phys_buf_array,
    int num_phys_buf,
    int mr_access_flags,
    u64 * iova_start)
{
	return (-EOPNOTSUPP);
}

static inline int
ib_query_mr(struct ib_mr *mr, struct ib_mr_attr *mr_attr)
{
	return (-EOPNOTSUPP);
}

static inline struct ib_mr *
ib_create_mr(struct ib_pd *pd,
    struct ib_mr_init_attr *mr_init_attr)
{
	return (ERR_PTR(-ENOSYS));
}

static inline int
ib_destroy_mr(struct ib_mr *mr)
{
	return (-EOPNOTSUPP);
}

static inline struct ib_mr *
ib_alloc_fast_reg_mr(struct ib_pd *pd, int max_page_list_len)
{
	return (ERR_PTR(-ENOSYS));
}

static inline struct ib_fast_reg_page_list *
ib_alloc_fast_reg_page_list(struct ib_device *device, int page_list_len)
{
	return (ERR_PTR(-ENOSYS));
}

static inline void
ib_free_fast_reg_page_list(struct ib_fast_reg_page_list *page_list)
{

}

static inline struct ib_mw *
ib_alloc_mw(struct ib_pd *pd, enum ib_mw_type type)
{
	struct ib_mw *mw;

	if (!pd->device->alloc_mw)
		return ERR_PTR(-ENOSYS);

	mw = pd->device->alloc_mw(pd, type, NULL);
	if (!IS_ERR(mw)) {
		mw->device = pd->device;
		mw->pd = pd;
		mw->uobject = NULL;
		mw->type = type;
		atomic_inc(&pd->usecnt);
	}
	return (mw);
}

static inline int
ib_bind_mw(struct ib_qp *qp,
    struct ib_mw *mw,
    struct ib_mw_bind *mw_bind)
{
	return (-EOPNOTSUPP);
}

static inline int
ib_dealloc_mw(struct ib_mw *mw)
{
	struct ib_pd *pd;
	int ret;

	pd = mw->pd;
	ret = mw->device->dealloc_mw(mw);
	if (!ret)
		atomic_dec(&pd->usecnt);
	return (ret);
}

static inline struct ib_dct *
ib_create_dct(struct ib_pd *pd, struct ib_dct_init_attr *attr,
    struct ib_udata *udata)
{
	return (ERR_PTR(-ENOSYS));
}

static inline int
ib_destroy_dct(struct ib_dct *dct)
{
	return (-EOPNOTSUPP);
}

static inline int
ib_query_dct(struct ib_dct *dct, struct ib_dct_attr *attr)
{
	return (-EOPNOTSUPP);
}

static inline int
ib_query_values(struct ib_device *device,
    int q_values, struct ib_device_values *values)
{
	return (-EOPNOTSUPP);
}

static inline void
ib_active_speed_enum_to_rate(u8 active_speed,
    int *rate,
    char **speed)
{
	switch (active_speed) {
	case IB_SPEED_DDR:
		*speed = " DDR";
		*rate = 50;
		break;
	case IB_SPEED_QDR:
		*speed = " QDR";
		*rate = 100;
		break;
	case IB_SPEED_FDR10:
		*speed = " FDR10";
		*rate = 100;
		break;
	case IB_SPEED_FDR:
		*speed = " FDR";
		*rate = 140;
		break;
	case IB_SPEED_EDR:
		*speed = " EDR";
		*rate = 250;
		break;
	case IB_SPEED_SDR:
	default:			/* default to SDR for invalid rates */
		*rate = 25;
		break;
	}
}

#include <rdma/rdma_cm.h>

static inline struct rdma_cm_id *
rdma_create_id_compat(rdma_cm_event_handler event_handler,
    void *context, enum rdma_port_space ps,
    enum ib_qp_type qp_type)
{
	return (rdma_create_id(&init_net, event_handler, context, ps, qp_type));
}

#undef rdma_create_id
#define	rdma_create_id(...) \
	rdma_create_id_compat(__VA_ARGS__)

#endif					/* IB_VERBS_COMPAT_H */
