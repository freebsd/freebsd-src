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
 * Description: IB Verbs interpreter (header)
 */

#ifndef __BNXT_RE_IB_VERBS_H__
#define __BNXT_RE_IB_VERBS_H__

#include <rdma/ib_addr.h>
#include "bnxt_re-abi.h"
#include "qplib_res.h"
#include "qplib_fp.h"

struct bnxt_re_dev;

#define BNXT_RE_ROCE_V2_UDP_SPORT	0x8CD1
#define BNXT_RE_QP_RANDOM_QKEY		0x81818181

#ifndef IB_MTU_8192
#define IB_MTU_8192 8192
#endif

#ifndef SPEED_1000
#define SPEED_1000		1000
#endif

#ifndef SPEED_10000
#define SPEED_10000		10000
#endif

#ifndef SPEED_20000
#define SPEED_20000		20000
#endif

#ifndef SPEED_25000
#define SPEED_25000		25000
#endif

#ifndef SPEED_40000
#define SPEED_40000		40000
#endif

#ifndef SPEED_50000
#define SPEED_50000		50000
#endif

#ifndef SPEED_100000
#define SPEED_100000		100000
#endif

#ifndef SPEED_200000
#define SPEED_200000		200000
#endif

#ifndef IB_SPEED_HDR
#define IB_SPEED_HDR		64
#endif

#define RDMA_NETWORK_IPV4	1
#define RDMA_NETWORK_IPV6	2

#define ROCE_DMAC(x) (x)->dmac

#define dma_rmb()       rmb()

#define compat_ib_alloc_device(size) ib_alloc_device(size);

#define rdev_from_cq_in(cq_in) to_bnxt_re_dev(cq_in->device, ibdev)

#define GET_UVERBS_ABI_VERSION(ibdev)	(ibdev->uverbs_abi_ver)

#define CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_256MB 0x1cUL

#define IB_POLL_UNBOUND_WORKQUEUE       IB_POLL_WORKQUEUE

#define BNXT_RE_LEGACY_FENCE_BYTES	64
#define BNXT_RE_LEGACY_FENCE_PBL_SIZE	DIV_ROUND_UP(BNXT_RE_LEGACY_FENCE_BYTES, PAGE_SIZE)


static inline struct
bnxt_re_cq *__get_cq_from_cq_in(struct ib_cq *cq_in,
				struct bnxt_re_dev *rdev);
static inline struct
bnxt_re_qp *__get_qp_from_qp_in(struct ib_pd *qp_in,
				struct bnxt_re_dev *rdev);

static inline bool
bnxt_re_check_if_vlan_valid(struct bnxt_re_dev *rdev, u16 vlan_id);

#define bnxt_re_compat_qfwstr(void)			\
	bnxt_re_query_fw_str(struct ib_device *ibdev,	\
			     char *str, size_t str_len)

static inline
struct scatterlist *get_ib_umem_sgl(struct ib_umem *umem, u32 *nmap);

struct bnxt_re_gid_ctx {
	u32			idx;
	u32			refcnt;
};

struct bnxt_re_legacy_fence_data {
	u32 size;
	void *va;
	dma_addr_t dma_addr;
	struct bnxt_re_mr *mr;
	struct ib_mw *mw;
	struct bnxt_qplib_swqe bind_wqe;
	u32 bind_rkey;
};

struct bnxt_re_pd {
	struct ib_pd		ibpd;
	struct bnxt_re_dev	*rdev;
	struct bnxt_qplib_pd	qplib_pd;
	struct bnxt_re_legacy_fence_data fence;
};

struct bnxt_re_ah {
	struct ib_ah		ibah;
	struct bnxt_re_dev	*rdev;
	struct bnxt_qplib_ah	qplib_ah;
};

struct bnxt_re_srq {
	struct ib_srq		ibsrq;
	struct bnxt_re_dev	*rdev;
	u32			srq_limit;
	struct bnxt_qplib_srq	qplib_srq;
	struct ib_umem		*umem;
	spinlock_t		lock;
};

union ip_addr {
	u32 ipv4_addr;
	u8  ipv6_addr[16];
};

struct bnxt_re_qp_info_entry {
	union ib_gid		sgid;
	union ib_gid 		dgid;
	union ip_addr		s_ip;
	union ip_addr		d_ip;
	u16			s_port;
#define BNXT_RE_QP_DEST_PORT	4791
	u16			d_port;
};

struct bnxt_re_qp {
	struct ib_qp		ib_qp;
	struct list_head	list;
	struct bnxt_re_dev	*rdev;
	spinlock_t		sq_lock;
	spinlock_t		rq_lock;
	struct bnxt_qplib_qp	qplib_qp;
	struct ib_umem		*sumem;
	struct ib_umem		*rumem;
	/* QP1 */
	u32			send_psn;
	struct ib_ud_header	qp1_hdr;
	struct bnxt_re_cq	*scq;
	struct bnxt_re_cq	*rcq;
	struct dentry		*qp_info_pdev_dentry;
	struct bnxt_re_qp_info_entry qp_info_entry;
	void			*qp_data;
};

struct bnxt_re_cq {
	struct ib_cq		ibcq;
	struct list_head	cq_list;
	struct bnxt_re_dev	*rdev;
	struct bnxt_re_ucontext *uctx;
	spinlock_t              cq_lock;
	u16			cq_count;
	u16			cq_period;
	struct bnxt_qplib_cq	qplib_cq;
	struct bnxt_qplib_cqe	*cql;
#define MAX_CQL_PER_POLL	1024
	u32			max_cql;
	struct ib_umem		*umem;
	struct ib_umem		*resize_umem;
	struct ib_ucontext	*context;
	int			resize_cqe;
	/* list of cq per uctx. Used only for Thor-2 */
	void			*uctx_cq_page;
	void			*dbr_recov_cq_page;
	bool			is_dbr_soft_cq;
};

struct bnxt_re_mr {
	struct bnxt_re_dev	*rdev;
	struct ib_mr		ib_mr;
	struct ib_umem		*ib_umem;
	struct bnxt_qplib_mrw	qplib_mr;
	u32			npages;
	u64			*pages;
	struct bnxt_qplib_frpl	qplib_frpl;
	bool                    is_invalcb_active;
};

struct bnxt_re_frpl {
	struct bnxt_re_dev		*rdev;
	struct bnxt_qplib_frpl		qplib_frpl;
	u64				*page_list;
};

struct bnxt_re_mw {
	struct bnxt_re_dev	*rdev;
	struct ib_mw		ib_mw;
	struct bnxt_qplib_mrw	qplib_mw;
};

struct bnxt_re_ucontext {
	struct ib_ucontext	ibucontext;
	struct bnxt_re_dev	*rdev;
	struct list_head	cq_list;
	struct bnxt_qplib_dpi	dpi;
	struct bnxt_qplib_dpi	wcdpi;
	void			*shpg;
	spinlock_t		sh_lock;
	uint64_t		cmask;
	struct mutex		cq_lock;	/* Protect cq list */
	void			*dbr_recov_cq_page;
	struct bnxt_re_cq	*dbr_recov_cq;
};

struct bnxt_re_ah_info {
	union ib_gid		sgid;
	struct ib_gid_attr	sgid_attr;
	u16			vlan_tag;
	u8			nw_type;
};

struct ifnet *bnxt_re_get_netdev(struct ib_device *ibdev,
				 u8 port_num);

int bnxt_re_query_device(struct ib_device *ibdev,
			 struct ib_device_attr *ib_attr,
			 struct ib_udata *udata);
int bnxt_re_modify_device(struct ib_device *ibdev,
			  int device_modify_mask,
			  struct ib_device_modify *device_modify);
int bnxt_re_query_port(struct ib_device *ibdev, u8 port_num,
		       struct ib_port_attr *port_attr);
int bnxt_re_modify_port(struct ib_device *ibdev, u8 port_num,
			int port_modify_mask,
			struct ib_port_modify *port_modify);
int bnxt_re_get_port_immutable(struct ib_device *ibdev, u8 port_num,
			       struct ib_port_immutable *immutable);
void bnxt_re_compat_qfwstr(void);
int bnxt_re_query_pkey(struct ib_device *ibdev, u8 port_num,
		       u16 index, u16 *pkey);
int bnxt_re_del_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, void **context);
int bnxt_re_add_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context);
int bnxt_re_query_gid(struct ib_device *ibdev, u8 port_num,
		      int index, union ib_gid *gid);
enum rdma_link_layer bnxt_re_get_link_layer(struct ib_device *ibdev,
					    u8 port_num);
int bnxt_re_alloc_pd(struct ib_pd *pd_in, struct ib_udata *udata);
void bnxt_re_dealloc_pd(struct ib_pd *ib_pd, struct ib_udata *udata);

int bnxt_re_create_ah(struct ib_ah *ah_in, struct ib_ah_attr *attr,
		      u32 flags, struct ib_udata *udata);

int bnxt_re_modify_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr);
int bnxt_re_query_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr);

void bnxt_re_destroy_ah(struct ib_ah *ib_ah, u32 flags);
int bnxt_re_create_srq(struct ib_srq *srq_in,
		       struct ib_srq_init_attr *srq_init_attr,
		       struct ib_udata *udata);
int bnxt_re_modify_srq(struct ib_srq *srq, struct ib_srq_attr *srq_attr,
		       enum ib_srq_attr_mask srq_attr_mask,
		       struct ib_udata *udata);
int bnxt_re_query_srq(struct ib_srq *srq, struct ib_srq_attr *srq_attr);
void bnxt_re_destroy_srq(struct ib_srq *ib_srq,
			 struct ib_udata *udata);
int bnxt_re_post_srq_recv(struct ib_srq *ib_srq, const struct ib_recv_wr *wr,
			  const struct ib_recv_wr **bad_wr);
struct ib_qp *bnxt_re_create_qp(struct ib_pd *qp_in,
			       struct ib_qp_init_attr *qp_init_attr,
			       struct ib_udata *udata);
int bnxt_re_modify_qp(struct ib_qp *qp, struct ib_qp_attr *qp_attr,
		      int qp_attr_mask, struct ib_udata *udata);
int bnxt_re_query_qp(struct ib_qp *qp, struct ib_qp_attr *qp_attr,
		     int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr);
int bnxt_re_destroy_qp(struct ib_qp *ib_qp, struct ib_udata *udata);
int bnxt_re_post_send(struct ib_qp *ib_qp, const struct ib_send_wr *wr,
		      const struct ib_send_wr **bad_wr);
int bnxt_re_post_recv(struct ib_qp *ib_qp, const struct ib_recv_wr *wr,
		      const struct ib_recv_wr **bad_wr);
int bnxt_re_create_cq(struct ib_cq *cq_in,
		      const struct ib_cq_init_attr *attr,
		      struct ib_udata *udata);
void bnxt_re_destroy_cq(struct ib_cq *ib_cq, struct ib_udata *udata);
int bnxt_re_modify_cq(struct ib_cq *cq, u16 cq_count, u16 cq_period);
int bnxt_re_resize_cq(struct ib_cq *cq, int cqe, struct ib_udata *udata);
int bnxt_re_poll_cq(struct ib_cq *cq, int num_entries, struct ib_wc *wc);
int bnxt_re_req_notify_cq(struct ib_cq *cq, enum ib_cq_notify_flags flags);
struct ib_mr *bnxt_re_get_dma_mr(struct ib_pd *pd, int mr_access_flags);
int bnxt_re_map_mr_sg(struct ib_mr *ib_mr, struct scatterlist *sg,
		      int sg_nents, unsigned int *sg_offset);
struct ib_mr *bnxt_re_alloc_mr(struct ib_pd *ib_pd, enum ib_mr_type type,
			       u32 max_num_sg, struct ib_udata *udata);
int bnxt_re_dereg_mr(struct ib_mr *ib_mr, struct ib_udata *udata);
struct ib_mw *bnxt_re_alloc_mw(struct ib_pd *ib_pd, enum ib_mw_type type,
			       struct ib_udata *udata);
int bnxt_re_dealloc_mw(struct ib_mw *mw);
struct ib_mr *bnxt_re_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
				  u64 virt_addr, int mr_access_flags,
				  struct ib_udata *udata);
int
bnxt_re_rereg_user_mr(struct ib_mr *mr, int flags, u64 start, u64 length,
		      u64 virt_addr, int mr_access_flags, struct ib_pd *pd,
		      struct ib_udata *udata);
int bnxt_re_alloc_ucontext(struct ib_ucontext *uctx_in,
			   struct ib_udata *udata);
void bnxt_re_dealloc_ucontext(struct ib_ucontext *ib_uctx);
int bnxt_re_mmap(struct ib_ucontext *context, struct vm_area_struct *vma);
int bnxt_re_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			const struct ib_wc *wc, const struct ib_grh *grh,
			const struct ib_mad_hdr *in_mad, size_t in_mad_size,
			struct ib_mad_hdr *out_mad, size_t *out_mad_size,
			u16 *out_mad_pkey_index);
unsigned long bnxt_re_lock_cqs(struct bnxt_re_qp *qp);
void bnxt_re_unlock_cqs(struct bnxt_re_qp *qp, unsigned long flags);
void bnxt_re_disassociate_ucntx(struct ib_ucontext *ibcontext);
static inline int __bnxt_re_set_vma_data(void *bnxt_re_uctx,
					 struct vm_area_struct *vma);
void bnxt_re_update_shadow_ah(struct bnxt_re_dev *rdev);
void bnxt_re_handle_cqn(struct bnxt_qplib_cq *cq);
static inline int
bnxt_re_get_cached_gid(struct ib_device *dev, u8 port_num, int index,
		       union ib_gid *sgid, struct ib_gid_attr **sgid_attr,
		       struct ib_global_route *grh, struct ib_ah *ah);
static inline enum rdma_network_type
bnxt_re_gid_to_network_type(struct ib_gid_attr *sgid_attr,
			    union ib_gid *sgid);
static inline
struct ib_umem *ib_umem_get_compat(struct bnxt_re_dev *rdev,
				   struct ib_ucontext *ucontext,
				   struct ib_udata *udata,
				   unsigned long addr,
				   size_t size, int access, int dmasync);
static inline
struct ib_umem *ib_umem_get_flags_compat(struct bnxt_re_dev *rdev,
					 struct ib_ucontext *ucontext,
					 struct ib_udata *udata,
					 unsigned long addr,
					 size_t size, int access, int dmasync);
static inline size_t ib_umem_num_pages_compat(struct ib_umem *umem);
static inline void bnxt_re_peer_mem_release(struct ib_umem *umem);
void bnxt_re_resolve_dmac_task(struct work_struct *work);

static inline enum ib_qp_type  __from_hw_to_ib_qp_type(u8 type)
{
	switch (type) {
	case CMDQ_CREATE_QP1_TYPE_GSI:
	case CMDQ_CREATE_QP_TYPE_GSI:
		return IB_QPT_GSI;
	case CMDQ_CREATE_QP_TYPE_RC:
		return IB_QPT_RC;
	case CMDQ_CREATE_QP_TYPE_UD:
		return IB_QPT_UD;
	case CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE:
		return IB_QPT_RAW_ETHERTYPE;
	default:
		return IB_QPT_MAX;
	}
}

static inline u8 __from_ib_qp_state(enum ib_qp_state state)
{
	switch (state) {
	case IB_QPS_RESET:
		return CMDQ_MODIFY_QP_NEW_STATE_RESET;
	case IB_QPS_INIT:
		return CMDQ_MODIFY_QP_NEW_STATE_INIT;
	case IB_QPS_RTR:
		return CMDQ_MODIFY_QP_NEW_STATE_RTR;
	case IB_QPS_RTS:
		return CMDQ_MODIFY_QP_NEW_STATE_RTS;
	case IB_QPS_SQD:
		return CMDQ_MODIFY_QP_NEW_STATE_SQD;
	case IB_QPS_SQE:
		return CMDQ_MODIFY_QP_NEW_STATE_SQE;
	case IB_QPS_ERR:
	default:
		return CMDQ_MODIFY_QP_NEW_STATE_ERR;
	}
}

static inline u32 __from_ib_mtu(enum ib_mtu mtu)
{
	switch (mtu) {
	case IB_MTU_256:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_256;
	case IB_MTU_512:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_512;
	case IB_MTU_1024:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_1024;
	case IB_MTU_2048:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_2048;
	case IB_MTU_4096:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_4096;
	default:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_2048;
	}
}

static inline enum ib_mtu __to_ib_mtu(u32 mtu)
{
	switch (mtu & CREQ_QUERY_QP_RESP_SB_PATH_MTU_MASK) {
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_256:
		return IB_MTU_256;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_512:
		return IB_MTU_512;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_1024:
		return IB_MTU_1024;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_2048:
		return IB_MTU_2048;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_4096:
		return IB_MTU_4096;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_8192:
		return IB_MTU_8192;
	default:
		return IB_MTU_2048;
	}
}

static inline enum ib_qp_state __to_ib_qp_state(u8 state)
{
	switch (state) {
	case CMDQ_MODIFY_QP_NEW_STATE_RESET:
		return IB_QPS_RESET;
	case CMDQ_MODIFY_QP_NEW_STATE_INIT:
		return IB_QPS_INIT;
	case CMDQ_MODIFY_QP_NEW_STATE_RTR:
		return IB_QPS_RTR;
	case CMDQ_MODIFY_QP_NEW_STATE_RTS:
		return IB_QPS_RTS;
	case CMDQ_MODIFY_QP_NEW_STATE_SQD:
		return IB_QPS_SQD;
	case CMDQ_MODIFY_QP_NEW_STATE_SQE:
		return IB_QPS_SQE;
	case CMDQ_MODIFY_QP_NEW_STATE_ERR:
	default:
		return IB_QPS_ERR;
	}
}

static inline int bnxt_re_init_pow2_flag(struct bnxt_re_uctx_req *req,
					 struct bnxt_re_uctx_resp *resp)
{
	resp->comp_mask |= BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED;
	if (!(req->comp_mask & BNXT_RE_COMP_MASK_REQ_UCNTX_POW2_SUPPORT)) {
		resp->comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED;
		return -EINVAL;
	}
	return 0;
}

static inline u32 bnxt_re_init_depth(u32 ent, struct bnxt_re_ucontext *uctx)
{
	return uctx ? (uctx->cmask & BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED) ?
		       ent : roundup_pow_of_two(ent) : ent;
}

static inline int bnxt_re_init_rsvd_wqe_flag(struct bnxt_re_uctx_req *req,
					     struct bnxt_re_uctx_resp *resp,
					     bool genp5)
{
	resp->comp_mask |= BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
	if (!(req->comp_mask & BNXT_RE_COMP_MASK_REQ_UCNTX_RSVD_WQE)) {
		resp->comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
		return -EINVAL;
	} else if (!genp5) {
		resp->comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
	}
	return 0;
}

static inline u32 bnxt_re_get_diff(struct bnxt_re_ucontext *uctx,
				   struct bnxt_qplib_chip_ctx *cctx)
{
	if (!uctx) {
		/* return res-wqe only for gen p4 for user resource */
		return _is_chip_gen_p5_p7(cctx) ? 0 : BNXT_QPLIB_RESERVED_QP_WRS;
	} else if (uctx->cmask & BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED) {
		return 0;
	}
	/* old lib */
	return BNXT_QPLIB_RESERVED_QP_WRS;
}

static inline void bnxt_re_init_qpmtu(struct bnxt_re_qp *qp, int mtu,
				      int mask, struct ib_qp_attr *qp_attr,
				      bool *is_qpmtu_high)
{
	int qpmtu, qpmtu_int;
	int ifmtu, ifmtu_int;

	ifmtu = iboe_get_mtu(mtu);
	ifmtu_int = ib_mtu_enum_to_int(ifmtu);
	qpmtu = ifmtu;
	qpmtu_int = ifmtu_int;
	if (mask & IB_QP_PATH_MTU) {
		qpmtu = qp_attr->path_mtu;
		qpmtu_int = ib_mtu_enum_to_int(qpmtu);
		if (qpmtu_int > ifmtu_int) {
			/* Trim the QP path mtu to interface mtu and update
			 * the new mtu to user qp for retransmission psn
			 * calculations.
			 */
			qpmtu = ifmtu;
			qpmtu_int = ifmtu_int;
			*is_qpmtu_high = true;
		}
	}
	qp->qplib_qp.path_mtu = __from_ib_mtu(qpmtu);
	qp->qplib_qp.mtu = qpmtu_int;
	qp->qplib_qp.modify_flags |=
		CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU;
}

inline unsigned long compare_ether_header(void *a, void *b)
{
	u32 *a32 = (u32 *)((u8 *)a + 2);
	u32 *b32 = (u32 *)((u8 *)b + 2);

	return (*(u16 *)a ^ *(u16 *)b) | (a32[0] ^ b32[0]) |
	       (a32[1] ^ b32[1]) | (a32[2] ^ b32[2]);
}

struct vlan_hdr {
	__be16	h_vlan_TCI;
	__be16	h_vlan_encapsulated_proto;
};

inline uint16_t
crc16(uint16_t crc, const void *buffer, unsigned int len)
{
	const unsigned char *cp = buffer;
	/* CRC table for the CRC-16. The poly is 0x8005 (x16 + x15 + x2 + 1). */
	static uint16_t const crc16_table[256] = {
		0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
		0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
		0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
		0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
		0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
		0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
		0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
		0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
		0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
		0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
		0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
		0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
		0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
		0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
		0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
		0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
		0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
		0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
		0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
		0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
		0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
		0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
		0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
		0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
		0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
		0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
		0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
		0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
		0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
		0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
		0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
		0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
	};

	while (len--)
		crc = (((crc >> 8) & 0xffU) ^
		    crc16_table[(crc ^ *cp++) & 0xffU]) & 0x0000ffffU;
	return crc;
}

static inline int __bnxt_re_set_vma_data(void *bnxt_re_uctx,
					 struct vm_area_struct *vma)
{
	return 0;
}

static inline bool bnxt_re_check_if_vlan_valid(struct bnxt_re_dev *rdev,
					       u16 vlan_id)
{
	bool ret = true;
	/*
	 * Check if the vlan is configured in the host.
	 * If not configured, it  can be a transparent
	 * VLAN. So dont report the vlan id.
	 */
	return ret;
}

#endif
