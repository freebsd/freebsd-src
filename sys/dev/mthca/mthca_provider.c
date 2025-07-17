/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies. All rights reserved.
 * Copyright (c) 2004 Voltaire, Inc. All rights reserved.
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

#include <rdma/ib_smi.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/uverbs_ioctl.h>

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>

#include "mthca_dev.h"
#include "mthca_cmd.h"
#include <rdma/mthca-abi.h>
#include "mthca_memfree.h"

static void init_query_mad(struct ib_smp *mad)
{
	mad->base_version  = 1;
	mad->mgmt_class    = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	mad->class_version = 1;
	mad->method    	   = IB_MGMT_METHOD_GET;
}

static int mthca_query_device(struct ib_device *ibdev, struct ib_device_attr *props,
			      struct ib_udata *uhw)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;
	struct mthca_dev *mdev = to_mdev(ibdev);

	if (uhw->inlen || uhw->outlen)
		return -EINVAL;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	memset(props, 0, sizeof *props);

	props->fw_ver              = mdev->fw_ver;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mthca_MAD_IFC(mdev, 1, 1,
			    1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	props->device_cap_flags    = mdev->device_cap_flags;
	props->vendor_id           = be32_to_cpup((__be32 *) (out_mad->data + 36)) &
		0xffffff;
	props->vendor_part_id      = be16_to_cpup((__be16 *) (out_mad->data + 30));
	props->hw_ver              = be32_to_cpup((__be32 *) (out_mad->data + 32));
	memcpy(&props->sys_image_guid, out_mad->data +  4, 8);

	props->max_mr_size         = ~0ull;
	props->page_size_cap       = mdev->limits.page_size_cap;
	props->max_qp              = mdev->limits.num_qps - mdev->limits.reserved_qps;
	props->max_qp_wr           = mdev->limits.max_wqes;
	props->max_sge             = mdev->limits.max_sg;
	props->max_sge_rd          = props->max_sge;
	props->max_cq              = mdev->limits.num_cqs - mdev->limits.reserved_cqs;
	props->max_cqe             = mdev->limits.max_cqes;
	props->max_mr              = mdev->limits.num_mpts - mdev->limits.reserved_mrws;
	props->max_pd              = mdev->limits.num_pds - mdev->limits.reserved_pds;
	props->max_qp_rd_atom      = 1 << mdev->qp_table.rdb_shift;
	props->max_qp_init_rd_atom = mdev->limits.max_qp_init_rdma;
	props->max_res_rd_atom     = props->max_qp_rd_atom * props->max_qp;
	props->max_srq             = mdev->limits.num_srqs - mdev->limits.reserved_srqs;
	props->max_srq_wr          = mdev->limits.max_srq_wqes;
	props->max_srq_sge         = mdev->limits.max_srq_sge;
	props->local_ca_ack_delay  = mdev->limits.local_ca_ack_delay;
	props->atomic_cap          = mdev->limits.flags & DEV_LIM_FLAG_ATOMIC ?
					IB_ATOMIC_HCA : IB_ATOMIC_NONE;
	props->max_pkeys           = mdev->limits.pkey_table_len;
	props->max_mcast_grp       = mdev->limits.num_mgms + mdev->limits.num_amgms;
	props->max_mcast_qp_attach = MTHCA_QP_PER_MGM;
	props->max_total_mcast_qp_attach = props->max_mcast_qp_attach *
					   props->max_mcast_grp;
	/*
	 * If Sinai memory key optimization is being used, then only
	 * the 8-bit key portion will change.  For other HCAs, the
	 * unused index bits will also be used for FMR remapping.
	 */
	if (mdev->mthca_flags & MTHCA_FLAG_SINAI_OPT)
		props->max_map_per_fmr = 255;
	else
		props->max_map_per_fmr =
			(1 << (32 - ilog2(mdev->limits.num_mpts))) - 1;

	err = 0;
 out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mthca_query_port(struct ib_device *ibdev,
			    u8 port, struct ib_port_attr *props)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	memset(props, 0, sizeof *props);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mthca_MAD_IFC(to_mdev(ibdev), 1, 1,
			    port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	props->lid               = be16_to_cpup((__be16 *) (out_mad->data + 16));
	props->lmc               = out_mad->data[34] & 0x7;
	props->sm_lid            = be16_to_cpup((__be16 *) (out_mad->data + 18));
	props->sm_sl             = out_mad->data[36] & 0xf;
	props->state             = out_mad->data[32] & 0xf;
	props->phys_state        = out_mad->data[33] >> 4;
	props->port_cap_flags    = be32_to_cpup((__be32 *) (out_mad->data + 20));
	props->gid_tbl_len       = to_mdev(ibdev)->limits.gid_table_len;
	props->max_msg_sz        = 0x80000000;
	props->pkey_tbl_len      = to_mdev(ibdev)->limits.pkey_table_len;
	props->bad_pkey_cntr     = be16_to_cpup((__be16 *) (out_mad->data + 46));
	props->qkey_viol_cntr    = be16_to_cpup((__be16 *) (out_mad->data + 48));
	props->active_width      = out_mad->data[31] & 0xf;
	props->active_speed      = out_mad->data[35] >> 4;
	props->max_mtu           = out_mad->data[41] & 0xf;
	props->active_mtu        = out_mad->data[36] >> 4;
	props->subnet_timeout    = out_mad->data[51] & 0x1f;
	props->max_vl_num        = out_mad->data[37] >> 4;
	props->init_type_reply   = out_mad->data[41] >> 4;

 out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mthca_modify_device(struct ib_device *ibdev,
			       int mask,
			       struct ib_device_modify *props)
{
	if (mask & ~IB_DEVICE_MODIFY_NODE_DESC)
		return -EOPNOTSUPP;

	if (mask & IB_DEVICE_MODIFY_NODE_DESC) {
		if (mutex_lock_interruptible(&to_mdev(ibdev)->cap_mask_mutex))
			return -ERESTARTSYS;
		memcpy(ibdev->node_desc, props->node_desc,
		       IB_DEVICE_NODE_DESC_MAX);
		mutex_unlock(&to_mdev(ibdev)->cap_mask_mutex);
	}

	return 0;
}

static int mthca_modify_port(struct ib_device *ibdev,
			     u8 port, int port_modify_mask,
			     struct ib_port_modify *props)
{
	struct mthca_set_ib_param set_ib;
	struct ib_port_attr attr;
	int err;

	if (mutex_lock_interruptible(&to_mdev(ibdev)->cap_mask_mutex))
		return -ERESTARTSYS;

	err = mthca_query_port(ibdev, port, &attr);
	if (err)
		goto out;

	set_ib.set_si_guid     = 0;
	set_ib.reset_qkey_viol = !!(port_modify_mask & IB_PORT_RESET_QKEY_CNTR);

	set_ib.cap_mask = (attr.port_cap_flags | props->set_port_cap_mask) &
		~props->clr_port_cap_mask;

	err = mthca_SET_IB(to_mdev(ibdev), &set_ib, port);
	if (err)
		goto out;
out:
	mutex_unlock(&to_mdev(ibdev)->cap_mask_mutex);
	return err;
}

static int mthca_query_pkey(struct ib_device *ibdev,
			    u8 port, u16 index, u16 *pkey)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PKEY_TABLE;
	in_mad->attr_mod = cpu_to_be32(index / 32);

	err = mthca_MAD_IFC(to_mdev(ibdev), 1, 1,
			    port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	*pkey = be16_to_cpu(((__be16 *) out_mad->data)[index % 32]);

 out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mthca_query_gid(struct ib_device *ibdev, u8 port,
			   int index, union ib_gid *gid)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mthca_MAD_IFC(to_mdev(ibdev), 1, 1,
			    port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw, out_mad->data + 8, 8);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_GUID_INFO;
	in_mad->attr_mod = cpu_to_be32(index / 8);

	err = mthca_MAD_IFC(to_mdev(ibdev), 1, 1,
			    port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw + 8, out_mad->data + (index % 8) * 8, 8);

 out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mthca_alloc_ucontext(struct ib_ucontext *uctx,
				struct ib_udata *udata)
{
	struct ib_device *ibdev = uctx->device;
	struct mthca_alloc_ucontext_resp uresp = {};
	struct mthca_ucontext *context = to_mucontext(uctx);
	int                              err;

	if (!(to_mdev(ibdev)->active))
		return -EAGAIN;

	uresp.qp_tab_size = to_mdev(ibdev)->limits.num_qps;
	if (mthca_is_memfree(to_mdev(ibdev)))
		uresp.uarc_size = to_mdev(ibdev)->uar_table.uarc_size;
	else
		uresp.uarc_size = 0;

	err = mthca_uar_alloc(to_mdev(ibdev), &context->uar);
	if (err)
		return err;

	context->db_tab = mthca_init_user_db_tab(to_mdev(ibdev));
	if (IS_ERR(context->db_tab)) {
		err = PTR_ERR(context->db_tab);
		mthca_uar_free(to_mdev(ibdev), &context->uar);
		return err;
	}

	if (ib_copy_to_udata(udata, &uresp, sizeof(uresp))) {
		mthca_cleanup_user_db_tab(to_mdev(ibdev), &context->uar, context->db_tab);
		mthca_uar_free(to_mdev(ibdev), &context->uar);
		return -EFAULT;
	}

	context->reg_mr_warned = 0;

	return 0;
}

static void mthca_dealloc_ucontext(struct ib_ucontext *context)
{
	mthca_cleanup_user_db_tab(to_mdev(context->device), &to_mucontext(context)->uar,
				  to_mucontext(context)->db_tab);
	mthca_uar_free(to_mdev(context->device), &to_mucontext(context)->uar);
}

static int mthca_mmap_uar(struct ib_ucontext *context,
			  struct vm_area_struct *vma)
{
	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma, vma->vm_start,
			       to_mucontext(context)->uar.pfn,
			       PAGE_SIZE, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static int mthca_alloc_pd(struct ib_pd *ibpd, struct ib_udata *udata)
{
	struct ib_device *ibdev = ibpd->device;
	struct mthca_pd *pd = to_mpd(ibpd);
	int err;

	err = mthca_pd_alloc(to_mdev(ibdev), !udata, pd);
	if (err)
		return err;

	if (udata) {
		if (ib_copy_to_udata(udata, &pd->pd_num, sizeof (__u32))) {
			mthca_pd_free(to_mdev(ibdev), pd);
			return -EFAULT;
		}
	}

	return 0;
}

static void mthca_dealloc_pd(struct ib_pd *pd, struct ib_udata *udata)
{
	mthca_pd_free(to_mdev(pd->device), to_mpd(pd));
}

static int mthca_ah_create(struct ib_ah *ibah,
			   struct ib_ah_attr *init_attr, u32 flags,
			   struct ib_udata *udata)

{
	struct mthca_ah *ah = to_mah(ibah);

	return mthca_create_ah(to_mdev(ibah->device), to_mpd(ibah->pd),
			       init_attr, ah);
}

static void mthca_ah_destroy(struct ib_ah *ah, u32 flags)
{
	mthca_destroy_ah(to_mdev(ah->device), to_mah(ah));
}

static int mthca_create_srq(struct ib_srq *ibsrq,
			    struct ib_srq_init_attr *init_attr,
			    struct ib_udata *udata)
{
	struct mthca_create_srq ucmd;
	struct mthca_ucontext *context = rdma_udata_to_drv_context(
		udata, struct mthca_ucontext, ibucontext);
	struct mthca_srq *srq = to_msrq(ibsrq);
	int err;

	if (init_attr->srq_type != IB_SRQT_BASIC)
		return -EOPNOTSUPP;

	if (udata) {
		if (ib_copy_from_udata(&ucmd, udata, sizeof(ucmd)))
			return -EFAULT;

		err = mthca_map_user_db(to_mdev(ibsrq->device), &context->uar,
					context->db_tab, ucmd.db_index,
					ucmd.db_page);

		if (err)
			return err;

		srq->mr.ibmr.lkey = ucmd.lkey;
		srq->db_index     = ucmd.db_index;
	}

	err = mthca_alloc_srq(to_mdev(ibsrq->device), to_mpd(ibsrq->pd),
			      &init_attr->attr, srq, udata);

	if (err && udata)
		mthca_unmap_user_db(to_mdev(ibsrq->device), &context->uar,
				    context->db_tab, ucmd.db_index);

	if (err)
		return err;

	if (context && ib_copy_to_udata(udata, &srq->srqn, sizeof(__u32))) {
		mthca_free_srq(to_mdev(ibsrq->device), srq);
		return -EFAULT;
	}

	return 0;
}

static void mthca_destroy_srq(struct ib_srq *srq, struct ib_udata *udata)
{
	if (udata) {
		struct mthca_ucontext *context =
			rdma_udata_to_drv_context(
				udata,
				struct mthca_ucontext,
				ibucontext);

		mthca_unmap_user_db(to_mdev(srq->device), &context->uar,
				    context->db_tab, to_msrq(srq)->db_index);
	}

	mthca_free_srq(to_mdev(srq->device), to_msrq(srq));
}

static struct ib_qp *mthca_create_qp(struct ib_pd *pd,
				     struct ib_qp_init_attr *init_attr,
				     struct ib_udata *udata)
{
	struct mthca_ucontext *context = rdma_udata_to_drv_context(
		udata, struct mthca_ucontext, ibucontext);
	struct mthca_create_qp ucmd;
	struct mthca_qp *qp;
	int err;

	if (init_attr->create_flags)
		return ERR_PTR(-EINVAL);

	switch (init_attr->qp_type) {
	case IB_QPT_RC:
	case IB_QPT_UC:
	case IB_QPT_UD:
	{
		qp = kzalloc(sizeof(*qp), GFP_KERNEL);
		if (!qp)
			return ERR_PTR(-ENOMEM);

		if (udata) {
			if (ib_copy_from_udata(&ucmd, udata, sizeof ucmd)) {
				kfree(qp);
				return ERR_PTR(-EFAULT);
			}

			err = mthca_map_user_db(to_mdev(pd->device), &context->uar,
						context->db_tab,
						ucmd.sq_db_index, ucmd.sq_db_page);
			if (err) {
				kfree(qp);
				return ERR_PTR(err);
			}

			err = mthca_map_user_db(to_mdev(pd->device), &context->uar,
						context->db_tab,
						ucmd.rq_db_index, ucmd.rq_db_page);
			if (err) {
				mthca_unmap_user_db(to_mdev(pd->device),
						    &context->uar,
						    context->db_tab,
						    ucmd.sq_db_index);
				kfree(qp);
				return ERR_PTR(err);
			}

			qp->mr.ibmr.lkey = ucmd.lkey;
			qp->sq.db_index  = ucmd.sq_db_index;
			qp->rq.db_index  = ucmd.rq_db_index;
		}

		err = mthca_alloc_qp(to_mdev(pd->device), to_mpd(pd),
				     to_mcq(init_attr->send_cq),
				     to_mcq(init_attr->recv_cq),
				     init_attr->qp_type, init_attr->sq_sig_type,
				     &init_attr->cap, qp, udata);

		if (err && udata) {
			mthca_unmap_user_db(to_mdev(pd->device),
					    &context->uar,
					    context->db_tab,
					    ucmd.sq_db_index);
			mthca_unmap_user_db(to_mdev(pd->device),
					    &context->uar,
					    context->db_tab,
					    ucmd.rq_db_index);
		}

		qp->ibqp.qp_num = qp->qpn;
		break;
	}
	case IB_QPT_SMI:
	case IB_QPT_GSI:
	{
		qp = kzalloc(sizeof(*qp), GFP_KERNEL);
		if (!qp)
			return ERR_PTR(-ENOMEM);

		qp->ibqp.qp_num = init_attr->qp_type == IB_QPT_SMI ? 0 : 1;

		err = mthca_alloc_sqp(to_mdev(pd->device), to_mpd(pd),
				      to_mcq(init_attr->send_cq),
				      to_mcq(init_attr->recv_cq),
				      init_attr->sq_sig_type, &init_attr->cap,
				      qp->ibqp.qp_num, init_attr->port_num,
				      to_msqp(qp), udata);
		break;
	}
	default:
		/* Don't support raw QPs */
		return ERR_PTR(-ENOSYS);
	}

	if (err) {
		kfree(qp);
		return ERR_PTR(err);
	}

	init_attr->cap.max_send_wr     = qp->sq.max;
	init_attr->cap.max_recv_wr     = qp->rq.max;
	init_attr->cap.max_send_sge    = qp->sq.max_gs;
	init_attr->cap.max_recv_sge    = qp->rq.max_gs;
	init_attr->cap.max_inline_data = qp->max_inline_data;

	return &qp->ibqp;
}

static int mthca_destroy_qp(struct ib_qp *qp, struct ib_udata *udata)
{
	if (udata) {
		struct mthca_ucontext *context =
			rdma_udata_to_drv_context(
				udata,
				struct mthca_ucontext,
				ibucontext);

		mthca_unmap_user_db(to_mdev(qp->device),
				    &context->uar,
				    context->db_tab,
				    to_mqp(qp)->sq.db_index);
		mthca_unmap_user_db(to_mdev(qp->device),
				    &context->uar,
				    context->db_tab,
				    to_mqp(qp)->rq.db_index);
	}
	mthca_free_qp(to_mdev(qp->device), to_mqp(qp));
	kfree(to_mqp(qp));
	return 0;
}

static int mthca_create_cq(struct ib_cq *ibcq,
			   const struct ib_cq_init_attr *attr,
			   struct ib_udata *udata)
{
	struct ib_device *ibdev = ibcq->device;
	int entries = attr->cqe;
	struct mthca_create_cq ucmd;
	struct mthca_cq *cq;
	int nent;
	int err;
	struct mthca_ucontext *context = rdma_udata_to_drv_context(
		udata, struct mthca_ucontext, ibucontext);

	if (attr->flags)
		return -EOPNOTSUPP;

	if (entries < 1 || entries > to_mdev(ibdev)->limits.max_cqes)
		return -EINVAL;

	if (udata) {
		if (ib_copy_from_udata(&ucmd, udata, sizeof(ucmd)))
			return -EFAULT;

		err = mthca_map_user_db(to_mdev(ibdev), &context->uar,
					context->db_tab, ucmd.set_db_index,
					ucmd.set_db_page);
		if (err)
			return err;

		err = mthca_map_user_db(to_mdev(ibdev), &context->uar,
					context->db_tab, ucmd.arm_db_index,
					ucmd.arm_db_page);
		if (err)
			goto err_unmap_set;
	}

	cq = to_mcq(ibcq);

	if (udata) {
		cq->buf.mr.ibmr.lkey = ucmd.lkey;
		cq->set_ci_db_index  = ucmd.set_db_index;
		cq->arm_db_index     = ucmd.arm_db_index;
	}

	for (nent = 1; nent <= entries; nent <<= 1)
		; /* nothing */

	err = mthca_init_cq(to_mdev(ibdev), nent, context,
			    udata ? ucmd.pdn : to_mdev(ibdev)->driver_pd.pd_num,
			    cq);
	if (err)
		goto err_unmap_arm;

	if (udata && ib_copy_to_udata(udata, &cq->cqn, sizeof(__u32))) {
		mthca_free_cq(to_mdev(ibdev), cq);
		err = -EFAULT;
		goto err_unmap_arm;
	}

	cq->resize_buf = NULL;

	return 0;

err_unmap_arm:
	if (udata)
		mthca_unmap_user_db(to_mdev(ibdev), &context->uar,
				    context->db_tab, ucmd.arm_db_index);

err_unmap_set:
	if (udata)
		mthca_unmap_user_db(to_mdev(ibdev), &context->uar,
				    context->db_tab, ucmd.set_db_index);

	return err;
}

static int mthca_alloc_resize_buf(struct mthca_dev *dev, struct mthca_cq *cq,
				  int entries)
{
	int ret;

	spin_lock_irq(&cq->lock);
	if (cq->resize_buf) {
		ret = -EBUSY;
		goto unlock;
	}

	cq->resize_buf = kmalloc(sizeof *cq->resize_buf, GFP_ATOMIC);
	if (!cq->resize_buf) {
		ret = -ENOMEM;
		goto unlock;
	}

	cq->resize_buf->state = CQ_RESIZE_ALLOC;

	ret = 0;

unlock:
	spin_unlock_irq(&cq->lock);

	if (ret)
		return ret;

	ret = mthca_alloc_cq_buf(dev, &cq->resize_buf->buf, entries);
	if (ret) {
		spin_lock_irq(&cq->lock);
		kfree(cq->resize_buf);
		cq->resize_buf = NULL;
		spin_unlock_irq(&cq->lock);
		return ret;
	}

	cq->resize_buf->cqe = entries - 1;

	spin_lock_irq(&cq->lock);
	cq->resize_buf->state = CQ_RESIZE_READY;
	spin_unlock_irq(&cq->lock);

	return 0;
}

static int mthca_resize_cq(struct ib_cq *ibcq, int entries, struct ib_udata *udata)
{
	struct mthca_dev *dev = to_mdev(ibcq->device);
	struct mthca_cq *cq = to_mcq(ibcq);
	struct mthca_resize_cq ucmd;
	u32 lkey;
	int ret;

	if (entries < 1 || entries > dev->limits.max_cqes)
		return -EINVAL;

	mutex_lock(&cq->mutex);

	entries = roundup_pow_of_two(entries + 1);
	if (entries == ibcq->cqe + 1) {
		ret = 0;
		goto out;
	}

	if (cq->is_kernel) {
		ret = mthca_alloc_resize_buf(dev, cq, entries);
		if (ret)
			goto out;
		lkey = cq->resize_buf->buf.mr.ibmr.lkey;
	} else {
		if (ib_copy_from_udata(&ucmd, udata, sizeof ucmd)) {
			ret = -EFAULT;
			goto out;
		}
		lkey = ucmd.lkey;
	}

	ret = mthca_RESIZE_CQ(dev, cq->cqn, lkey, ilog2(entries));

	if (ret) {
		if (cq->resize_buf) {
			mthca_free_cq_buf(dev, &cq->resize_buf->buf,
					  cq->resize_buf->cqe);
			kfree(cq->resize_buf);
			spin_lock_irq(&cq->lock);
			cq->resize_buf = NULL;
			spin_unlock_irq(&cq->lock);
		}
		goto out;
	}

	if (cq->is_kernel) {
		struct mthca_cq_buf tbuf;
		int tcqe;

		spin_lock_irq(&cq->lock);
		if (cq->resize_buf->state == CQ_RESIZE_READY) {
			mthca_cq_resize_copy_cqes(cq);
			tbuf         = cq->buf;
			tcqe         = cq->ibcq.cqe;
			cq->buf      = cq->resize_buf->buf;
			cq->ibcq.cqe = cq->resize_buf->cqe;
		} else {
			tbuf = cq->resize_buf->buf;
			tcqe = cq->resize_buf->cqe;
		}

		kfree(cq->resize_buf);
		cq->resize_buf = NULL;
		spin_unlock_irq(&cq->lock);

		mthca_free_cq_buf(dev, &tbuf, tcqe);
	} else
		ibcq->cqe = entries - 1;

out:
	mutex_unlock(&cq->mutex);

	return ret;
}

static void mthca_destroy_cq(struct ib_cq *cq, struct ib_udata *udata)
{
	if (udata) {
		struct mthca_ucontext *context =
			rdma_udata_to_drv_context(
				udata,
				struct mthca_ucontext,
				ibucontext);

		mthca_unmap_user_db(to_mdev(cq->device),
				    &context->uar,
				    context->db_tab,
				    to_mcq(cq)->arm_db_index);
		mthca_unmap_user_db(to_mdev(cq->device),
				    &context->uar,
				    context->db_tab,
				    to_mcq(cq)->set_ci_db_index);
	}
	mthca_free_cq(to_mdev(cq->device), to_mcq(cq));
}

static inline u32 convert_access(int acc)
{
	return (acc & IB_ACCESS_REMOTE_ATOMIC ? MTHCA_MPT_FLAG_ATOMIC       : 0) |
	       (acc & IB_ACCESS_REMOTE_WRITE  ? MTHCA_MPT_FLAG_REMOTE_WRITE : 0) |
	       (acc & IB_ACCESS_REMOTE_READ   ? MTHCA_MPT_FLAG_REMOTE_READ  : 0) |
	       (acc & IB_ACCESS_LOCAL_WRITE   ? MTHCA_MPT_FLAG_LOCAL_WRITE  : 0) |
	       MTHCA_MPT_FLAG_LOCAL_READ;
}

static struct ib_mr *mthca_get_dma_mr(struct ib_pd *pd, int acc)
{
	struct mthca_mr *mr;
	int err;

	mr = kmalloc(sizeof *mr, GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	err = mthca_mr_alloc_notrans(to_mdev(pd->device),
				     to_mpd(pd)->pd_num,
				     convert_access(acc), mr);

	if (err) {
		kfree(mr);
		return ERR_PTR(err);
	}

	mr->umem = NULL;

	return &mr->ibmr;
}

static struct ib_mr *mthca_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
				       u64 virt, int acc, struct ib_udata *udata)
{
	struct mthca_dev *dev = to_mdev(pd->device);
	struct scatterlist *sg;
	struct mthca_mr *mr;
	struct mthca_reg_mr ucmd;
	u64 *pages;
	int shift, n, len;
	int i, k, entry;
	int err = 0;
	int write_mtt_size;

	if (udata->inlen - sizeof (struct ib_uverbs_cmd_hdr) < sizeof ucmd) {
		if (!to_mucontext(pd->uobject->context)->reg_mr_warned) {
			mthca_warn(dev, "Process '%s' did not pass in MR attrs.\n",
				   current->comm);
			mthca_warn(dev, "  Update libmthca to fix this.\n");
		}
		++to_mucontext(pd->uobject->context)->reg_mr_warned;
		ucmd.mr_attrs = 0;
	} else if (ib_copy_from_udata(&ucmd, udata, sizeof ucmd))
		return ERR_PTR(-EFAULT);

	mr = kmalloc(sizeof *mr, GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	mr->umem = ib_umem_get(pd->uobject->context, start, length, acc,
			       ucmd.mr_attrs & MTHCA_MR_DMASYNC);

	if (IS_ERR(mr->umem)) {
		err = PTR_ERR(mr->umem);
		goto err;
	}

	shift = ffs(mr->umem->page_size) - 1;
	n = mr->umem->nmap;

	mr->mtt = mthca_alloc_mtt(dev, n);
	if (IS_ERR(mr->mtt)) {
		err = PTR_ERR(mr->mtt);
		goto err_umem;
	}

	pages = (u64 *) __get_free_page(GFP_KERNEL);
	if (!pages) {
		err = -ENOMEM;
		goto err_mtt;
	}

	i = n = 0;

	write_mtt_size = min(mthca_write_mtt_size(dev), (int) (PAGE_SIZE / sizeof *pages));

	for_each_sg(mr->umem->sg_head.sgl, sg, mr->umem->nmap, entry) {
		len = sg_dma_len(sg) >> shift;
		for (k = 0; k < len; ++k) {
			pages[i++] = sg_dma_address(sg) +
				mr->umem->page_size * k;
			/*
			 * Be friendly to write_mtt and pass it chunks
			 * of appropriate size.
			 */
			if (i == write_mtt_size) {
				err = mthca_write_mtt(dev, mr->mtt, n, pages, i);
				if (err)
					goto mtt_done;
				n += i;
				i = 0;
			}
		}
	}

	if (i)
		err = mthca_write_mtt(dev, mr->mtt, n, pages, i);
mtt_done:
	free_page((unsigned long) pages);
	if (err)
		goto err_mtt;

	err = mthca_mr_alloc(dev, to_mpd(pd)->pd_num, shift, virt, length,
			     convert_access(acc), mr);

	if (err)
		goto err_mtt;

	return &mr->ibmr;

err_mtt:
	mthca_free_mtt(dev, mr->mtt);

err_umem:
	ib_umem_release(mr->umem);

err:
	kfree(mr);
	return ERR_PTR(err);
}

static int mthca_dereg_mr(struct ib_mr *mr, struct ib_udata *udata)
{
	struct mthca_mr *mmr = to_mmr(mr);

	mthca_free_mr(to_mdev(mr->device), mmr);
	ib_umem_release(mmr->umem);
	kfree(mmr);

	return 0;
}

static struct ib_fmr *mthca_alloc_fmr(struct ib_pd *pd, int mr_access_flags,
				      struct ib_fmr_attr *fmr_attr)
{
	struct mthca_fmr *fmr;
	int err;

	fmr = kmalloc(sizeof *fmr, GFP_KERNEL);
	if (!fmr)
		return ERR_PTR(-ENOMEM);

	memcpy(&fmr->attr, fmr_attr, sizeof *fmr_attr);
	err = mthca_fmr_alloc(to_mdev(pd->device), to_mpd(pd)->pd_num,
			     convert_access(mr_access_flags), fmr);

	if (err) {
		kfree(fmr);
		return ERR_PTR(err);
	}

	return &fmr->ibmr;
}

static int mthca_dealloc_fmr(struct ib_fmr *fmr)
{
	struct mthca_fmr *mfmr = to_mfmr(fmr);
	int err;

	err = mthca_free_fmr(to_mdev(fmr->device), mfmr);
	if (err)
		return err;

	kfree(mfmr);
	return 0;
}

static int mthca_unmap_fmr(struct list_head *fmr_list)
{
	struct ib_fmr *fmr;
	int err;
	struct mthca_dev *mdev = NULL;

	list_for_each_entry(fmr, fmr_list, list) {
		if (mdev && to_mdev(fmr->device) != mdev)
			return -EINVAL;
		mdev = to_mdev(fmr->device);
	}

	if (!mdev)
		return 0;

	if (mthca_is_memfree(mdev)) {
		list_for_each_entry(fmr, fmr_list, list)
			mthca_arbel_fmr_unmap(mdev, to_mfmr(fmr));

		wmb();
	} else
		list_for_each_entry(fmr, fmr_list, list)
			mthca_tavor_fmr_unmap(mdev, to_mfmr(fmr));

	err = mthca_SYNC_TPT(mdev);
	return err;
}

static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mthca_dev *dev =
		container_of(device, struct mthca_dev, ib_dev.dev);
	return sprintf(buf, "%x\n", dev->rev_id);
}

static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mthca_dev *dev =
		container_of(device, struct mthca_dev, ib_dev.dev);
	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_MELLANOX_TAVOR:
		return sprintf(buf, "MT23108\n");
	case PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT:
		return sprintf(buf, "MT25208 (MT23108 compat mode)\n");
	case PCI_DEVICE_ID_MELLANOX_ARBEL:
		return sprintf(buf, "MT25208\n");
	case PCI_DEVICE_ID_MELLANOX_SINAI:
	case PCI_DEVICE_ID_MELLANOX_SINAI_OLD:
		return sprintf(buf, "MT25204\n");
	default:
		return sprintf(buf, "unknown\n");
	}
}

static ssize_t show_board(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct mthca_dev *dev =
		container_of(device, struct mthca_dev, ib_dev.dev);
	return sprintf(buf, "%.*s\n", MTHCA_BOARD_ID_LEN, dev->board_id);
}

static DEVICE_ATTR(hw_rev,   S_IRUGO, show_rev,    NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca,    NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_board,  NULL);

static struct device_attribute *mthca_dev_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_hca_type,
	&dev_attr_board_id
};

static int mthca_init_node_data(struct mthca_dev *dev)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_DESC;

	err = mthca_MAD_IFC(dev, 1, 1,
			    1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(dev->ib_dev.node_desc, out_mad->data, IB_DEVICE_NODE_DESC_MAX);

	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mthca_MAD_IFC(dev, 1, 1,
			    1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	if (mthca_is_memfree(dev))
		dev->rev_id = be32_to_cpup((__be32 *) (out_mad->data + 32));
	memcpy(&dev->ib_dev.node_guid, out_mad->data + 12, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mthca_port_immutable(struct ib_device *ibdev, u8 port_num,
			        struct ib_port_immutable *immutable)
{
	struct ib_port_attr attr;
	int err;

	err = mthca_query_port(ibdev, port_num, &attr);
	if (err)
		return err;

	immutable->pkey_tbl_len = attr.pkey_tbl_len;
	immutable->gid_tbl_len = attr.gid_tbl_len;
	immutable->core_cap_flags = RDMA_CORE_PORT_IBA_IB;
	immutable->max_mad_size = IB_MGMT_MAD_SIZE;

	return 0;
}

static void get_dev_fw_str(struct ib_device *device, char *str,
			   size_t str_len)
{
	struct mthca_dev *dev =
		container_of(device, struct mthca_dev, ib_dev);
	snprintf(str, str_len, "%d.%d.%d",
		 (int) (dev->fw_ver >> 32),
		 (int) (dev->fw_ver >> 16) & 0xffff,
		 (int) dev->fw_ver & 0xffff);
}

int mthca_register_device(struct mthca_dev *dev)
{
	int ret;
	int i;

	ret = mthca_init_node_data(dev);
	if (ret)
		return ret;

#define	mthca_ib_ah mthca_ah
#define	mthca_ib_cq mthca_cq
#define	mthca_ib_pd mthca_pd
#define	mthca_ib_qp mthca_qp
#define	mthca_ib_srq mthca_srq
#define	mthca_ib_ucontext mthca_ucontext
	INIT_IB_DEVICE_OPS(&dev->ib_dev.ops, mthca, MTHCA);
	strlcpy(dev->ib_dev.name, "mthca%d", IB_DEVICE_NAME_MAX);
	dev->ib_dev.owner                = THIS_MODULE;

	dev->ib_dev.uverbs_abi_ver	 = MTHCA_UVERBS_ABI_VERSION;
	dev->ib_dev.uverbs_cmd_mask	 =
		(1ull << IB_USER_VERBS_CMD_GET_CONTEXT)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_DEVICE)	|
		(1ull << IB_USER_VERBS_CMD_QUERY_PORT)		|
		(1ull << IB_USER_VERBS_CMD_ALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_DEALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_REG_MR)		|
		(1ull << IB_USER_VERBS_CMD_DEREG_MR)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_RESIZE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_CQ)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_QP)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_QP)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_QP)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_QP)		|
		(1ull << IB_USER_VERBS_CMD_ATTACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_DETACH_MCAST);
	dev->ib_dev.node_type            = RDMA_NODE_IB_CA;
	dev->ib_dev.phys_port_cnt        = dev->limits.num_ports;
	dev->ib_dev.num_comp_vectors     = 1;
	dev->ib_dev.dma_device           = &dev->pdev->dev;
	dev->ib_dev.query_device         = mthca_query_device;
	dev->ib_dev.query_port           = mthca_query_port;
	dev->ib_dev.modify_device        = mthca_modify_device;
	dev->ib_dev.modify_port          = mthca_modify_port;
	dev->ib_dev.query_pkey           = mthca_query_pkey;
	dev->ib_dev.query_gid            = mthca_query_gid;
	dev->ib_dev.alloc_ucontext       = mthca_alloc_ucontext;
	dev->ib_dev.dealloc_ucontext     = mthca_dealloc_ucontext;
	dev->ib_dev.mmap                 = mthca_mmap_uar;
	dev->ib_dev.alloc_pd             = mthca_alloc_pd;
	dev->ib_dev.dealloc_pd           = mthca_dealloc_pd;
	dev->ib_dev.create_ah            = mthca_ah_create;
	dev->ib_dev.query_ah             = mthca_ah_query;
	dev->ib_dev.destroy_ah           = mthca_ah_destroy;

	if (dev->mthca_flags & MTHCA_FLAG_SRQ) {
		dev->ib_dev.create_srq           = mthca_create_srq;
		dev->ib_dev.modify_srq           = mthca_modify_srq;
		dev->ib_dev.query_srq            = mthca_query_srq;
		dev->ib_dev.destroy_srq          = mthca_destroy_srq;
		dev->ib_dev.uverbs_cmd_mask	|=
			(1ull << IB_USER_VERBS_CMD_CREATE_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_MODIFY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_SRQ);

		if (mthca_is_memfree(dev))
			dev->ib_dev.post_srq_recv = mthca_arbel_post_srq_recv;
		else
			dev->ib_dev.post_srq_recv = mthca_tavor_post_srq_recv;
	}

	dev->ib_dev.create_qp            = mthca_create_qp;
	dev->ib_dev.modify_qp            = mthca_modify_qp;
	dev->ib_dev.query_qp             = mthca_query_qp;
	dev->ib_dev.destroy_qp           = mthca_destroy_qp;
	dev->ib_dev.create_cq            = mthca_create_cq;
	dev->ib_dev.resize_cq            = mthca_resize_cq;
	dev->ib_dev.destroy_cq           = mthca_destroy_cq;
	dev->ib_dev.poll_cq              = mthca_poll_cq;
	dev->ib_dev.get_dma_mr           = mthca_get_dma_mr;
	dev->ib_dev.reg_user_mr          = mthca_reg_user_mr;
	dev->ib_dev.dereg_mr             = mthca_dereg_mr;
	dev->ib_dev.get_port_immutable   = mthca_port_immutable;
	dev->ib_dev.get_dev_fw_str       = get_dev_fw_str;

	if (dev->mthca_flags & MTHCA_FLAG_FMR) {
		dev->ib_dev.alloc_fmr            = mthca_alloc_fmr;
		dev->ib_dev.unmap_fmr            = mthca_unmap_fmr;
		dev->ib_dev.dealloc_fmr          = mthca_dealloc_fmr;
		if (mthca_is_memfree(dev))
			dev->ib_dev.map_phys_fmr = mthca_arbel_map_phys_fmr;
		else
			dev->ib_dev.map_phys_fmr = mthca_tavor_map_phys_fmr;
	}

	dev->ib_dev.attach_mcast         = mthca_multicast_attach;
	dev->ib_dev.detach_mcast         = mthca_multicast_detach;
	dev->ib_dev.process_mad          = mthca_process_mad;

	if (mthca_is_memfree(dev)) {
		dev->ib_dev.req_notify_cq = mthca_arbel_arm_cq;
		dev->ib_dev.post_send     = mthca_arbel_post_send;
		dev->ib_dev.post_recv     = mthca_arbel_post_receive;
	} else {
		dev->ib_dev.req_notify_cq = mthca_tavor_arm_cq;
		dev->ib_dev.post_send     = mthca_tavor_post_send;
		dev->ib_dev.post_recv     = mthca_tavor_post_receive;
	}

	mutex_init(&dev->cap_mask_mutex);

	ret = ib_register_device(&dev->ib_dev, NULL);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(mthca_dev_attributes); ++i) {
		ret = device_create_file(&dev->ib_dev.dev,
					 mthca_dev_attributes[i]);
		if (ret) {
			ib_unregister_device(&dev->ib_dev);
			return ret;
		}
	}

	mthca_start_catas_poll(dev);

	return 0;
}

void mthca_unregister_device(struct mthca_dev *dev)
{
	mthca_stop_catas_poll(dev);
	ib_unregister_device(&dev->ib_dev);
}
