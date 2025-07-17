/*
 * Copyright (c) 2018-2019 Cavium, Inc.
 * All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef __QLNXR_VERBS_H__
#define __QLNXR_VERBS_H__

extern int qlnxr_iw_query_gid(struct ib_device *,
			uint8_t port,
			int index,
			union ib_gid *gid);

extern int qlnxr_query_gid(struct ib_device *,
			u8 port,
			int index,
			union ib_gid *gid);

extern int qlnxr_create_srq(struct ib_srq *ibsrq,
			struct ib_srq_init_attr *,
			struct ib_udata *);

extern void qlnxr_destroy_srq(struct ib_srq *,
			struct ib_udata *);

extern int qlnxr_modify_srq(struct ib_srq *,
			struct ib_srq_attr *,
			enum ib_srq_attr_mask,
			struct ib_udata *);

extern int qlnxr_query_srq(struct ib_srq *,
			struct ib_srq_attr *);

extern int qlnxr_post_srq_recv(struct ib_srq *,
			const struct ib_recv_wr *,
			const struct ib_recv_wr **bad_recv_wr);

extern int qlnxr_query_device(struct ib_device *, struct ib_device_attr *,
		struct ib_udata *);
extern int qlnxr_get_port_immutable(struct ib_device *ibdev, u8 port_num,
		struct ib_port_immutable *immutable);

extern int qlnxr_query_port(struct ib_device *,
			u8 port,
			struct ib_port_attr *props);

extern int qlnxr_modify_port(struct ib_device *,
			u8 port,
			int mask,
			struct ib_port_modify *props);

extern enum rdma_link_layer qlnxr_link_layer(struct ib_device *device,
			uint8_t port_num);

extern int qlnxr_alloc_pd(struct ib_pd *ibpd, struct ib_udata *);

extern void qlnxr_dealloc_pd(struct ib_pd *pd, struct ib_udata *udata);

extern int qlnxr_create_cq(struct ib_cq *ibcq,
		   const struct ib_cq_init_attr *attr,
		   struct ib_udata *udata);

extern void qlnxr_destroy_cq(struct ib_cq *, struct ib_udata *);

extern int qlnxr_resize_cq(struct ib_cq *,
			int cqe,
			struct ib_udata *);

extern int qlnxr_poll_cq(struct ib_cq *,
			int num_entries,
			struct ib_wc *wc);

extern struct ib_qp *qlnxr_create_qp(struct ib_pd *,
		       struct ib_qp_init_attr *attrs,
		       struct ib_udata *);

extern int qlnxr_modify_qp(struct ib_qp *,
			struct ib_qp_attr *attr,
			int attr_mask,
			struct ib_udata *udata);

extern int qlnxr_query_qp(struct ib_qp *,
			struct ib_qp_attr *qp_attr,
			int qp_attr_mask,
			struct ib_qp_init_attr *);

extern int qlnxr_destroy_qp(struct ib_qp *, struct ib_udata *);

extern int qlnxr_query_pkey(struct ib_device *,
			u8 port,
			u16 index,
			u16 *pkey);

extern int qlnxr_create_ah(struct ib_ah *ibah,
			struct ib_ah_attr *attr, u32 flags,
			struct ib_udata *udata);
extern void qlnxr_destroy_ah(struct ib_ah *ibah, u32 flags);

extern int qlnxr_query_ah(struct ib_ah *ibah,
			struct ib_ah_attr *attr);

extern int qlnxr_modify_ah(struct ib_ah *ibah,
			struct ib_ah_attr *attr);

extern int qlnxr_process_mad(struct ib_device *ibdev,
			int process_mad_flags,
			u8 port_num,
			const struct ib_wc *in_wc,
			const struct ib_grh *in_grh,
			const struct ib_mad_hdr *mad_hdr,
			size_t in_mad_size,
			struct ib_mad_hdr *out_mad,
			size_t *out_mad_size,
			u16 *out_mad_pkey_index);

extern int qlnxr_post_send(struct ib_qp *,
			const struct ib_send_wr *,
			const struct ib_send_wr **bad_wr);

extern int qlnxr_post_recv(struct ib_qp *,
			const struct ib_recv_wr *,
			const struct ib_recv_wr **bad_wr);

extern int qlnxr_arm_cq(struct ib_cq *,
			enum ib_cq_notify_flags flags);

extern struct ib_mr *qlnxr_get_dma_mr(struct ib_pd *,
			int acc);

extern int qlnxr_dereg_mr(struct ib_mr *, struct ib_udata *);

extern struct ib_mr *qlnxr_reg_user_mr(struct ib_pd *,
			u64 start,
			u64 length,
			u64 virt,
			int acc,
			struct ib_udata *);

extern struct ib_mr *qlnxr_alloc_mr(struct ib_pd *pd,
			enum ib_mr_type mr_type, u32 max_num_sg,
			struct ib_udata *udata);

extern int qlnxr_map_mr_sg(struct ib_mr *mr, struct scatterlist *sg,
			int sg_nents, unsigned int *sg_offset);

extern int qlnxr_alloc_ucontext(struct ib_ucontext *uctx,
				struct ib_udata *udata);

extern void qlnxr_dealloc_ucontext(struct ib_ucontext *ibctx);

extern int qlnxr_mmap(struct ib_ucontext *, struct vm_area_struct *vma);

extern int qlnxr_iw_connect(struct iw_cm_id *cm_id,
		    struct iw_cm_conn_param *conn_param);

extern int qlnxr_iw_create_listen(struct iw_cm_id *cm_id, int backlog);

void qlnxr_iw_destroy_listen(struct iw_cm_id *cm_id);

extern int qlnxr_iw_accept(struct iw_cm_id *cm_id,
		      struct iw_cm_conn_param *conn_param);

extern int qlnxr_iw_reject(struct iw_cm_id *cm_id, const void *pdata, u8 pdata_len);

extern void qlnxr_iw_qp_add_ref(struct ib_qp *qp);

extern void qlnxr_iw_qp_rem_ref(struct ib_qp *qp);

extern struct ib_qp *qlnxr_iw_get_qp(struct ib_device *dev, int qpn);

#endif /* #ifndef __QLNXR_VERBS_H__ */
