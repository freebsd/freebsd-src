/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2015 - 2022 Intel Corporation
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

#include "irdma_main.h"

/**
 * irdma_query_device - get device attributes
 * @ibdev: device pointer from stack
 * @props: returning device attributes
 * @udata: user data
 */
static int
irdma_query_device(struct ib_device *ibdev,
		   struct ib_device_attr *props,
		   struct ib_udata *udata)
{
	struct irdma_device *iwdev = to_iwdev(ibdev);
	struct irdma_pci_f *rf = iwdev->rf;
	struct pci_dev *pcidev = iwdev->rf->pcidev;
	struct irdma_hw_attrs *hw_attrs = &rf->sc_dev.hw_attrs;

	if (udata->inlen || udata->outlen)
		return -EINVAL;

	memset(props, 0, sizeof(*props));
	ether_addr_copy((u8 *)&props->sys_image_guid, IF_LLADDR(iwdev->netdev));
	props->fw_ver = (u64)irdma_fw_major_ver(&rf->sc_dev) << 32 |
	    irdma_fw_minor_ver(&rf->sc_dev);
	props->device_cap_flags = iwdev->device_cap_flags;
	props->vendor_id = pcidev->vendor;
	props->vendor_part_id = pcidev->device;
	props->hw_ver = pcidev->revision;
	props->page_size_cap = SZ_4K | SZ_2M | SZ_1G;
	props->max_mr_size = hw_attrs->max_mr_size;
	props->max_qp = rf->max_qp - rf->used_qps;
	props->max_qp_wr = hw_attrs->max_qp_wr;
	set_max_sge(props, rf);
	props->max_cq = rf->max_cq - rf->used_cqs;
	props->max_cqe = rf->max_cqe;
	props->max_mr = rf->max_mr - rf->used_mrs;
	props->max_mw = props->max_mr;
	props->max_pd = rf->max_pd - rf->used_pds;
	props->max_sge_rd = hw_attrs->uk_attrs.max_hw_read_sges;
	props->max_qp_rd_atom = hw_attrs->max_hw_ird;
	props->max_qp_init_rd_atom = hw_attrs->max_hw_ord;
	if (rdma_protocol_roce(ibdev, 1))
		props->max_pkeys = IRDMA_PKEY_TBL_SZ;
	props->max_ah = rf->max_ah;
	props->max_mcast_grp = rf->max_mcg;
	props->max_mcast_qp_attach = IRDMA_MAX_MGS_PER_CTX;
	props->max_total_mcast_qp_attach = rf->max_qp * IRDMA_MAX_MGS_PER_CTX;
	props->max_fast_reg_page_list_len = IRDMA_MAX_PAGES_PER_FMR;
#define HCA_CLOCK_TIMESTAMP_MASK 0x1ffff
	if (hw_attrs->uk_attrs.hw_rev >= IRDMA_GEN_2)
		props->timestamp_mask = HCA_CLOCK_TIMESTAMP_MASK;

	return 0;
}

static int
irdma_mmap_legacy(struct irdma_ucontext *ucontext,
		  struct vm_area_struct *vma)
{
	u64 pfn;

	if (vma->vm_pgoff || vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	vma->vm_private_data = ucontext;
	pfn = ((uintptr_t)ucontext->iwdev->rf->sc_dev.hw_regs[IRDMA_DB_ADDR_OFFSET] +
	       pci_resource_start(ucontext->iwdev->rf->pcidev, 0)) >> PAGE_SHIFT;

	return rdma_user_mmap_io(&ucontext->ibucontext, vma, pfn, PAGE_SIZE,
				 pgprot_noncached(vma->vm_page_prot), NULL);
}

static void
irdma_mmap_free(struct rdma_user_mmap_entry *rdma_entry)
{
	struct irdma_user_mmap_entry *entry = to_irdma_mmap_entry(rdma_entry);

	kfree(entry);
}

struct rdma_user_mmap_entry *
irdma_user_mmap_entry_insert(struct irdma_ucontext *ucontext, u64 bar_offset,
			     enum irdma_mmap_flag mmap_flag, u64 *mmap_offset)
{
	struct irdma_user_mmap_entry *entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	int ret;

	if (!entry)
		return NULL;

	entry->bar_offset = bar_offset;
	entry->mmap_flag = mmap_flag;

	ret = rdma_user_mmap_entry_insert(&ucontext->ibucontext,
					  &entry->rdma_entry, PAGE_SIZE);
	if (ret) {
		kfree(entry);
		return NULL;
	}
	*mmap_offset = rdma_user_mmap_get_offset(&entry->rdma_entry);

	return &entry->rdma_entry;
}

/**
 * irdma_mmap - user memory map
 * @context: context created during alloc
 * @vma: kernel info for user memory map
 */
static int
irdma_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
	struct rdma_user_mmap_entry *rdma_entry;
	struct irdma_user_mmap_entry *entry;
	struct irdma_ucontext *ucontext;
	u64 pfn;
	int ret;

	ucontext = to_ucontext(context);

	/* Legacy support for libi40iw with hard-coded mmap key */
	if (ucontext->legacy_mode)
		return irdma_mmap_legacy(ucontext, vma);

	rdma_entry = rdma_user_mmap_entry_get(&ucontext->ibucontext, vma);
	if (!rdma_entry) {
		irdma_debug(iwdev_to_idev(ucontext->iwdev), IRDMA_DEBUG_VERBS,
			    "pgoff[0x%lx] does not have valid entry\n",
			    vma->vm_pgoff);
		return -EINVAL;
	}

	entry = to_irdma_mmap_entry(rdma_entry);
	irdma_debug(iwdev_to_idev(ucontext->iwdev), IRDMA_DEBUG_VERBS,
		    "bar_offset [0x%lx] mmap_flag [%d]\n", entry->bar_offset,
		    entry->mmap_flag);

	pfn = (entry->bar_offset +
	       pci_resource_start(ucontext->iwdev->rf->pcidev, 0)) >> PAGE_SHIFT;

	switch (entry->mmap_flag) {
	case IRDMA_MMAP_IO_NC:
		ret = rdma_user_mmap_io(context, vma, pfn, PAGE_SIZE,
					pgprot_noncached(vma->vm_page_prot),
					rdma_entry);
		break;
	case IRDMA_MMAP_IO_WC:
		ret = rdma_user_mmap_io(context, vma, pfn, PAGE_SIZE,
					pgprot_writecombine(vma->vm_page_prot),
					rdma_entry);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret)
		irdma_debug(iwdev_to_idev(ucontext->iwdev), IRDMA_DEBUG_VERBS,
			    "bar_offset [0x%lx] mmap_flag[%d] err[%d]\n",
			    entry->bar_offset, entry->mmap_flag, ret);
	rdma_user_mmap_entry_put(rdma_entry);

	return ret;
}

/**
 * irdma_alloc_push_page - allocate a push page for qp
 * @iwqp: qp pointer
 */
static void
irdma_alloc_push_page(struct irdma_qp *iwqp)
{
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_sc_qp *qp = &iwqp->sc_qp;
	int status;

	cqp_request = irdma_alloc_and_get_cqp_request(&iwdev->rf->cqp, true);
	if (!cqp_request)
		return;

	cqp_info = &cqp_request->info;
	cqp_info->cqp_cmd = IRDMA_OP_MANAGE_PUSH_PAGE;
	cqp_info->post_sq = 1;
	cqp_info->in.u.manage_push_page.info.push_idx = 0;
	cqp_info->in.u.manage_push_page.info.qs_handle =
	    qp->vsi->qos[qp->user_pri].qs_handle;
	cqp_info->in.u.manage_push_page.info.free_page = 0;
	cqp_info->in.u.manage_push_page.info.push_page_type = 0;
	cqp_info->in.u.manage_push_page.cqp = &iwdev->rf->cqp.sc_cqp;
	cqp_info->in.u.manage_push_page.scratch = (uintptr_t)cqp_request;

	status = irdma_handle_cqp_op(iwdev->rf, cqp_request);
	if (!status && cqp_request->compl_info.op_ret_val <
	    iwdev->rf->sc_dev.hw_attrs.max_hw_device_pages) {
		qp->push_idx = cqp_request->compl_info.op_ret_val;
		qp->push_offset = 0;
	}

	irdma_put_cqp_request(&iwdev->rf->cqp, cqp_request);
}

/**
 * irdma_get_pbl - Retrieve pbl from a list given a virtual
 * address
 * @va: user virtual address
 * @pbl_list: pbl list to search in (QP's or CQ's)
 */
struct irdma_pbl *
irdma_get_pbl(unsigned long va,
	      struct list_head *pbl_list)
{
	struct irdma_pbl *iwpbl;

	list_for_each_entry(iwpbl, pbl_list, list) {
		if (iwpbl->user_base == va) {
			list_del(&iwpbl->list);
			iwpbl->on_list = false;
			return iwpbl;
		}
	}

	return NULL;
}

/**
 * irdma_clean_cqes - clean cq entries for qp
 * @iwqp: qp ptr (user or kernel)
 * @iwcq: cq ptr
 */
void
irdma_clean_cqes(struct irdma_qp *iwqp, struct irdma_cq *iwcq)
{
	struct irdma_cq_uk *ukcq = &iwcq->sc_cq.cq_uk;
	unsigned long flags;

	spin_lock_irqsave(&iwcq->lock, flags);
	irdma_uk_clean_cq(&iwqp->sc_qp.qp_uk, ukcq);
	spin_unlock_irqrestore(&iwcq->lock, flags);
}

void
irdma_remove_push_mmap_entries(struct irdma_qp *iwqp)
{
	if (iwqp->push_db_mmap_entry) {
		rdma_user_mmap_entry_remove(iwqp->push_db_mmap_entry);
		iwqp->push_db_mmap_entry = NULL;
	}
	if (iwqp->push_wqe_mmap_entry) {
		rdma_user_mmap_entry_remove(iwqp->push_wqe_mmap_entry);
		iwqp->push_wqe_mmap_entry = NULL;
	}
}

static int
irdma_setup_push_mmap_entries(struct irdma_ucontext *ucontext,
			      struct irdma_qp *iwqp,
			      u64 *push_wqe_mmap_key,
			      u64 *push_db_mmap_key)
{
	struct irdma_device *iwdev = ucontext->iwdev;
	u64 bar_off;

	WARN_ON_ONCE(iwdev->rf->sc_dev.hw_attrs.uk_attrs.hw_rev < IRDMA_GEN_2);

	bar_off = (uintptr_t)iwdev->rf->sc_dev.hw_regs[IRDMA_DB_ADDR_OFFSET];

	if (iwdev->rf->sc_dev.hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_2) {
		/* skip over db page */
		bar_off += IRDMA_HW_PAGE_SIZE;
		/* skip over reserved space */
		bar_off += IRDMA_PF_BAR_RSVD;
	}

	/* push wqe page */
	bar_off += iwqp->sc_qp.push_idx * IRDMA_HW_PAGE_SIZE;
	iwqp->push_wqe_mmap_entry = irdma_user_mmap_entry_insert(ucontext,
								 bar_off, IRDMA_MMAP_IO_WC,
								 push_wqe_mmap_key);
	if (!iwqp->push_wqe_mmap_entry)
		return -ENOMEM;

	/* push doorbell page */
	bar_off += IRDMA_HW_PAGE_SIZE;
	iwqp->push_db_mmap_entry = irdma_user_mmap_entry_insert(ucontext,
								bar_off, IRDMA_MMAP_IO_NC,
								push_db_mmap_key);
	if (!iwqp->push_db_mmap_entry) {
		rdma_user_mmap_entry_remove(iwqp->push_wqe_mmap_entry);
		return -ENOMEM;
	}

	return 0;
}

/**
 * irdma_setup_virt_qp - setup for allocation of virtual qp
 * @iwdev: irdma device
 * @iwqp: qp ptr
 * @init_info: initialize info to return
 */
void
irdma_setup_virt_qp(struct irdma_device *iwdev,
		    struct irdma_qp *iwqp,
		    struct irdma_qp_init_info *init_info)
{
	struct irdma_pbl *iwpbl = iwqp->iwpbl;
	struct irdma_qp_mr *qpmr = &iwpbl->qp_mr;

	iwqp->page = qpmr->sq_page;
	init_info->shadow_area_pa = qpmr->shadow;
	if (iwpbl->pbl_allocated) {
		init_info->virtual_map = true;
		init_info->sq_pa = qpmr->sq_pbl.idx;
		init_info->rq_pa = qpmr->rq_pbl.idx;
	} else {
		init_info->sq_pa = qpmr->sq_pbl.addr;
		init_info->rq_pa = qpmr->rq_pbl.addr;
	}
}

/**
 * irdma_setup_kmode_qp - setup initialization for kernel mode qp
 * @iwdev: iwarp device
 * @iwqp: qp ptr (user or kernel)
 * @info: initialize info to return
 * @init_attr: Initial QP create attributes
 */
int
irdma_setup_kmode_qp(struct irdma_device *iwdev,
		     struct irdma_qp *iwqp,
		     struct irdma_qp_init_info *info,
		     struct ib_qp_init_attr *init_attr)
{
	struct irdma_dma_mem *mem = &iwqp->kqp.dma_mem;
	u32 sqdepth, rqdepth;
	u8 sqshift, rqshift;
	u32 size;
	int status;
	struct irdma_qp_uk_init_info *ukinfo = &info->qp_uk_init_info;
	struct irdma_uk_attrs *uk_attrs = &iwdev->rf->sc_dev.hw_attrs.uk_attrs;

	irdma_get_wqe_shift(uk_attrs,
			    uk_attrs->hw_rev >= IRDMA_GEN_2 ? ukinfo->max_sq_frag_cnt + 1 :
			    ukinfo->max_sq_frag_cnt,
			    ukinfo->max_inline_data, &sqshift);
	status = irdma_get_sqdepth(uk_attrs->max_hw_wq_quanta, ukinfo->sq_size,
				   sqshift, &sqdepth);
	if (status)
		return status;

	if (uk_attrs->hw_rev == IRDMA_GEN_1)
		rqshift = IRDMA_MAX_RQ_WQE_SHIFT_GEN1;
	else
		irdma_get_wqe_shift(uk_attrs, ukinfo->max_rq_frag_cnt, 0,
				    &rqshift);

	status = irdma_get_rqdepth(uk_attrs->max_hw_rq_quanta, ukinfo->rq_size,
				   rqshift, &rqdepth);
	if (status)
		return status;

	iwqp->kqp.sq_wrid_mem =
	    kcalloc(sqdepth, sizeof(*iwqp->kqp.sq_wrid_mem), GFP_KERNEL);
	if (!iwqp->kqp.sq_wrid_mem)
		return -ENOMEM;

	iwqp->kqp.rq_wrid_mem =
	    kcalloc(rqdepth, sizeof(*iwqp->kqp.rq_wrid_mem), GFP_KERNEL);
	if (!iwqp->kqp.rq_wrid_mem) {
		kfree(iwqp->kqp.sq_wrid_mem);
		iwqp->kqp.sq_wrid_mem = NULL;
		return -ENOMEM;
	}

	iwqp->kqp.sig_trk_mem = kcalloc(sqdepth, sizeof(u32), GFP_KERNEL);
	memset(iwqp->kqp.sig_trk_mem, 0, sqdepth * sizeof(u32));
	if (!iwqp->kqp.sig_trk_mem) {
		kfree(iwqp->kqp.sq_wrid_mem);
		iwqp->kqp.sq_wrid_mem = NULL;
		kfree(iwqp->kqp.rq_wrid_mem);
		iwqp->kqp.rq_wrid_mem = NULL;
		return -ENOMEM;
	}
	ukinfo->sq_sigwrtrk_array = (void *)iwqp->kqp.sig_trk_mem;
	ukinfo->sq_wrtrk_array = iwqp->kqp.sq_wrid_mem;
	ukinfo->rq_wrid_array = iwqp->kqp.rq_wrid_mem;

	size = (sqdepth + rqdepth) * IRDMA_QP_WQE_MIN_SIZE;
	size += (IRDMA_SHADOW_AREA_SIZE << 3);

	mem->size = size;
	mem->va = irdma_allocate_dma_mem(&iwdev->rf->hw, mem, mem->size,
					 256);
	if (!mem->va) {
		kfree(iwqp->kqp.sq_wrid_mem);
		iwqp->kqp.sq_wrid_mem = NULL;
		kfree(iwqp->kqp.rq_wrid_mem);
		iwqp->kqp.rq_wrid_mem = NULL;
		return -ENOMEM;
	}

	ukinfo->sq = mem->va;
	info->sq_pa = mem->pa;
	ukinfo->rq = &ukinfo->sq[sqdepth];
	info->rq_pa = info->sq_pa + (sqdepth * IRDMA_QP_WQE_MIN_SIZE);
	ukinfo->shadow_area = ukinfo->rq[rqdepth].elem;
	info->shadow_area_pa = info->rq_pa + (rqdepth * IRDMA_QP_WQE_MIN_SIZE);
	ukinfo->sq_size = sqdepth >> sqshift;
	ukinfo->rq_size = rqdepth >> rqshift;
	ukinfo->qp_id = iwqp->ibqp.qp_num;

	init_attr->cap.max_send_wr = (sqdepth - IRDMA_SQ_RSVD) >> sqshift;
	init_attr->cap.max_recv_wr = (rqdepth - IRDMA_RQ_RSVD) >> rqshift;

	return 0;
}

int
irdma_cqp_create_qp_cmd(struct irdma_qp *iwqp)
{
	struct irdma_pci_f *rf = iwqp->iwdev->rf;
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	struct irdma_create_qp_info *qp_info;
	int status;

	cqp_request = irdma_alloc_and_get_cqp_request(&rf->cqp, true);
	if (!cqp_request)
		return -ENOMEM;

	cqp_info = &cqp_request->info;
	qp_info = &cqp_request->info.in.u.qp_create.info;
	memset(qp_info, 0, sizeof(*qp_info));
	qp_info->mac_valid = true;
	qp_info->cq_num_valid = true;
	qp_info->next_iwarp_state = IRDMA_QP_STATE_IDLE;

	cqp_info->cqp_cmd = IRDMA_OP_QP_CREATE;
	cqp_info->post_sq = 1;
	cqp_info->in.u.qp_create.qp = &iwqp->sc_qp;
	cqp_info->in.u.qp_create.scratch = (uintptr_t)cqp_request;
	status = irdma_handle_cqp_op(rf, cqp_request);
	irdma_put_cqp_request(&rf->cqp, cqp_request);

	return status;
}

void
irdma_roce_fill_and_set_qpctx_info(struct irdma_qp *iwqp,
				   struct irdma_qp_host_ctx_info *ctx_info)
{
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_sc_dev *dev = &iwdev->rf->sc_dev;
	struct irdma_roce_offload_info *roce_info;
	struct irdma_udp_offload_info *udp_info;

	udp_info = &iwqp->udp_info;
	udp_info->snd_mss = ib_mtu_enum_to_int(ib_mtu_int_to_enum(iwdev->vsi.mtu));
	udp_info->cwnd = iwdev->roce_cwnd;
	udp_info->rexmit_thresh = 2;
	udp_info->rnr_nak_thresh = 2;
	udp_info->src_port = 0xc000;
	udp_info->dst_port = ROCE_V2_UDP_DPORT;
	roce_info = &iwqp->roce_info;
	ether_addr_copy(roce_info->mac_addr, IF_LLADDR(iwdev->netdev));

	roce_info->rd_en = true;
	roce_info->wr_rdresp_en = true;
	roce_info->bind_en = true;
	roce_info->dcqcn_en = false;
	roce_info->rtomin = 5;

	roce_info->ack_credits = iwdev->roce_ackcreds;
	roce_info->ird_size = dev->hw_attrs.max_hw_ird;
	roce_info->ord_size = dev->hw_attrs.max_hw_ord;

	if (!iwqp->user_mode) {
		roce_info->priv_mode_en = true;
		roce_info->fast_reg_en = true;
		roce_info->udprivcq_en = true;
	}
	roce_info->roce_tver = 0;

	ctx_info->roce_info = &iwqp->roce_info;
	ctx_info->udp_info = &iwqp->udp_info;
	irdma_sc_qp_setctx_roce(&iwqp->sc_qp, iwqp->host_ctx.va, ctx_info);
}

void
irdma_iw_fill_and_set_qpctx_info(struct irdma_qp *iwqp,
				 struct irdma_qp_host_ctx_info *ctx_info)
{
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_sc_dev *dev = &iwdev->rf->sc_dev;
	struct irdma_iwarp_offload_info *iwarp_info;

	iwarp_info = &iwqp->iwarp_info;
	ether_addr_copy(iwarp_info->mac_addr, IF_LLADDR(iwdev->netdev));
	iwarp_info->rd_en = true;
	iwarp_info->wr_rdresp_en = true;
	iwarp_info->bind_en = true;
	iwarp_info->ecn_en = true;
	iwarp_info->rtomin = 5;

	if (dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2)
		iwarp_info->ib_rd_en = true;
	if (!iwqp->user_mode) {
		iwarp_info->priv_mode_en = true;
		iwarp_info->fast_reg_en = true;
	}
	iwarp_info->ddp_ver = 1;
	iwarp_info->rdmap_ver = 1;

	ctx_info->iwarp_info = &iwqp->iwarp_info;
	ctx_info->iwarp_info_valid = true;
	irdma_sc_qp_setctx(&iwqp->sc_qp, iwqp->host_ctx.va, ctx_info);
	ctx_info->iwarp_info_valid = false;
}

int
irdma_validate_qp_attrs(struct ib_qp_init_attr *init_attr,
			struct irdma_device *iwdev)
{
	struct irdma_sc_dev *dev = &iwdev->rf->sc_dev;
	struct irdma_uk_attrs *uk_attrs = &dev->hw_attrs.uk_attrs;

	if (init_attr->create_flags)
		return -EOPNOTSUPP;

	if (init_attr->cap.max_inline_data > uk_attrs->max_hw_inline ||
	    init_attr->cap.max_send_sge > uk_attrs->max_hw_wq_frags ||
	    init_attr->cap.max_recv_sge > uk_attrs->max_hw_wq_frags)
		return -EINVAL;

	if (rdma_protocol_roce(&iwdev->ibdev, 1)) {
		if (init_attr->qp_type != IB_QPT_RC &&
		    init_attr->qp_type != IB_QPT_UD &&
		    init_attr->qp_type != IB_QPT_GSI)
			return -EOPNOTSUPP;
	} else {
		if (init_attr->qp_type != IB_QPT_RC)
			return -EOPNOTSUPP;
	}

	return 0;
}

void
irdma_flush_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct irdma_qp *iwqp = container_of(dwork, struct irdma_qp, dwork_flush);
	unsigned long flags;

	spin_lock_irqsave(&iwqp->lock, flags);	/* Don't allow more posting while generating completions */
	irdma_generate_flush_completions(iwqp);
	spin_unlock_irqrestore(&iwqp->lock, flags);
}

static int
irdma_get_ib_acc_flags(struct irdma_qp *iwqp)
{
	int acc_flags = 0;

	if (rdma_protocol_roce(iwqp->ibqp.device, 1)) {
		if (iwqp->roce_info.wr_rdresp_en) {
			acc_flags |= IB_ACCESS_LOCAL_WRITE;
			acc_flags |= IB_ACCESS_REMOTE_WRITE;
		}
		if (iwqp->roce_info.rd_en)
			acc_flags |= IB_ACCESS_REMOTE_READ;
		if (iwqp->roce_info.bind_en)
			acc_flags |= IB_ACCESS_MW_BIND;
	} else {
		if (iwqp->iwarp_info.wr_rdresp_en) {
			acc_flags |= IB_ACCESS_LOCAL_WRITE;
			acc_flags |= IB_ACCESS_REMOTE_WRITE;
		}
		if (iwqp->iwarp_info.rd_en)
			acc_flags |= IB_ACCESS_REMOTE_READ;
		if (iwqp->iwarp_info.bind_en)
			acc_flags |= IB_ACCESS_MW_BIND;
	}
	return acc_flags;
}

/**
 * irdma_query_qp - query qp attributes
 * @ibqp: qp pointer
 * @attr: attributes pointer
 * @attr_mask: Not used
 * @init_attr: qp attributes to return
 */
static int
irdma_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
	       int attr_mask, struct ib_qp_init_attr *init_attr)
{
	struct irdma_qp *iwqp = to_iwqp(ibqp);
	struct irdma_sc_qp *qp = &iwqp->sc_qp;

	memset(attr, 0, sizeof(*attr));
	memset(init_attr, 0, sizeof(*init_attr));

	attr->qp_state = iwqp->ibqp_state;
	attr->cur_qp_state = iwqp->ibqp_state;
	attr->cap.max_send_wr = iwqp->max_send_wr;
	attr->cap.max_recv_wr = iwqp->max_recv_wr;
	attr->cap.max_inline_data = qp->qp_uk.max_inline_data;
	attr->cap.max_send_sge = qp->qp_uk.max_sq_frag_cnt;
	attr->cap.max_recv_sge = qp->qp_uk.max_rq_frag_cnt;
	attr->qp_access_flags = irdma_get_ib_acc_flags(iwqp);
	attr->port_num = 1;
	if (rdma_protocol_roce(ibqp->device, 1)) {
		attr->path_mtu = ib_mtu_int_to_enum(iwqp->udp_info.snd_mss);
		attr->qkey = iwqp->roce_info.qkey;
		attr->rq_psn = iwqp->udp_info.epsn;
		attr->sq_psn = iwqp->udp_info.psn_nxt;
		attr->dest_qp_num = iwqp->roce_info.dest_qp;
		attr->pkey_index = iwqp->roce_info.p_key;
		attr->retry_cnt = iwqp->udp_info.rexmit_thresh;
		attr->rnr_retry = iwqp->udp_info.rnr_nak_thresh;
		attr->max_rd_atomic = iwqp->roce_info.ord_size;
		attr->max_dest_rd_atomic = iwqp->roce_info.ird_size;
	}

	init_attr->event_handler = iwqp->ibqp.event_handler;
	init_attr->qp_context = iwqp->ibqp.qp_context;
	init_attr->send_cq = iwqp->ibqp.send_cq;
	init_attr->recv_cq = iwqp->ibqp.recv_cq;
	init_attr->cap = attr->cap;

	return 0;
}

/**
 * irdma_modify_qp_roce - modify qp request
 * @ibqp: qp's pointer for modify
 * @attr: access attributes
 * @attr_mask: state mask
 * @udata: user data
 */
int
irdma_modify_qp_roce(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		     int attr_mask, struct ib_udata *udata)
{
	struct irdma_pd *iwpd = to_iwpd(ibqp->pd);
	struct irdma_qp *iwqp = to_iwqp(ibqp);
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_sc_dev *dev = &iwdev->rf->sc_dev;
	struct irdma_qp_host_ctx_info *ctx_info;
	struct irdma_roce_offload_info *roce_info;
	struct irdma_udp_offload_info *udp_info;
	struct irdma_modify_qp_info info = {0};
	struct irdma_modify_qp_resp uresp = {};
	struct irdma_modify_qp_req ureq = {};
	unsigned long flags;
	u8 issue_modify_qp = 0;
	int ret = 0;

	ctx_info = &iwqp->ctx_info;
	roce_info = &iwqp->roce_info;
	udp_info = &iwqp->udp_info;

	if (attr_mask & ~IB_QP_ATTR_STANDARD_BITS)
		return -EOPNOTSUPP;

	if (attr_mask & IB_QP_DEST_QPN)
		roce_info->dest_qp = attr->dest_qp_num;

	if (attr_mask & IB_QP_PKEY_INDEX) {
		ret = irdma_query_pkey(ibqp->device, 0, attr->pkey_index,
				       &roce_info->p_key);
		if (ret)
			return ret;
	}

	if (attr_mask & IB_QP_QKEY)
		roce_info->qkey = attr->qkey;

	if (attr_mask & IB_QP_PATH_MTU)
		udp_info->snd_mss = ib_mtu_enum_to_int(attr->path_mtu);

	if (attr_mask & IB_QP_SQ_PSN) {
		udp_info->psn_nxt = attr->sq_psn;
		udp_info->lsn = 0xffff;
		udp_info->psn_una = attr->sq_psn;
		udp_info->psn_max = attr->sq_psn;
	}

	if (attr_mask & IB_QP_RQ_PSN)
		udp_info->epsn = attr->rq_psn;

	if (attr_mask & IB_QP_RNR_RETRY)
		udp_info->rnr_nak_thresh = attr->rnr_retry;

	if (attr_mask & IB_QP_RETRY_CNT)
		udp_info->rexmit_thresh = attr->retry_cnt;

	ctx_info->roce_info->pd_id = iwpd->sc_pd.pd_id;

	if (attr_mask & IB_QP_AV) {
		struct irdma_av *av = &iwqp->roce_ah.av;
		u16 vlan_id = VLAN_N_VID;
		u32 local_ip[4] = {};

		memset(&iwqp->roce_ah, 0, sizeof(iwqp->roce_ah));
		if (attr->ah_attr.ah_flags & IB_AH_GRH) {
			udp_info->ttl = attr->ah_attr.grh.hop_limit;
			udp_info->flow_label = attr->ah_attr.grh.flow_label;
			udp_info->tos = attr->ah_attr.grh.traffic_class;
			irdma_qp_rem_qos(&iwqp->sc_qp);
			dev->ws_remove(iwqp->sc_qp.vsi, ctx_info->user_pri);
			if (iwqp->sc_qp.vsi->dscp_mode)
				ctx_info->user_pri =
				    iwqp->sc_qp.vsi->dscp_map[irdma_tos2dscp(udp_info->tos)];
			else
				ctx_info->user_pri = rt_tos2priority(udp_info->tos);
			iwqp->sc_qp.user_pri = ctx_info->user_pri;
			if (dev->ws_add(iwqp->sc_qp.vsi, ctx_info->user_pri))
				return -ENOMEM;
			irdma_qp_add_qos(&iwqp->sc_qp);
		}
		ret = kc_irdma_set_roce_cm_info(iwqp, attr, &vlan_id);
		if (ret)
			return ret;

		if (vlan_id >= VLAN_N_VID && iwdev->dcb_vlan_mode)
			vlan_id = 0;
		if (vlan_id < VLAN_N_VID) {
			udp_info->insert_vlan_tag = true;
			udp_info->vlan_tag = vlan_id |
			    ctx_info->user_pri << VLAN_PRIO_SHIFT;
		} else {
			udp_info->insert_vlan_tag = false;
		}

		av->attrs = attr->ah_attr;
		rdma_gid2ip((struct sockaddr *)&av->dgid_addr, &attr->ah_attr.grh.dgid);
		roce_info->local_qp = ibqp->qp_num;
		if (av->sgid_addr.saddr.sa_family == AF_INET6) {
			__be32 *daddr =
			av->dgid_addr.saddr_in6.sin6_addr.__u6_addr.__u6_addr32;
			__be32 *saddr =
			av->sgid_addr.saddr_in6.sin6_addr.__u6_addr.__u6_addr32;

			irdma_copy_ip_ntohl(&udp_info->dest_ip_addr[0], daddr);
			irdma_copy_ip_ntohl(&udp_info->local_ipaddr[0], saddr);

			udp_info->ipv4 = false;
			irdma_copy_ip_ntohl(local_ip, daddr);

			udp_info->arp_idx = irdma_arp_table(iwdev->rf, local_ip,
							    NULL, IRDMA_ARP_RESOLVE);
		} else {
			__be32 saddr = av->sgid_addr.saddr_in.sin_addr.s_addr;
			__be32 daddr = av->dgid_addr.saddr_in.sin_addr.s_addr;

			local_ip[0] = ntohl(daddr);

			udp_info->ipv4 = true;
			udp_info->dest_ip_addr[0] = 0;
			udp_info->dest_ip_addr[1] = 0;
			udp_info->dest_ip_addr[2] = 0;
			udp_info->dest_ip_addr[3] = local_ip[0];

			udp_info->local_ipaddr[0] = 0;
			udp_info->local_ipaddr[1] = 0;
			udp_info->local_ipaddr[2] = 0;
			udp_info->local_ipaddr[3] = ntohl(saddr);
		}
		udp_info->arp_idx =
		    irdma_add_arp(iwdev->rf, local_ip,
				  ah_attr_to_dmac(attr->ah_attr));
	}

	if (attr_mask & IB_QP_MAX_QP_RD_ATOMIC) {
		if (attr->max_rd_atomic > dev->hw_attrs.max_hw_ord) {
			ibdev_err(&iwdev->ibdev,
				  "rd_atomic = %d, above max_hw_ord=%d\n",
				  attr->max_rd_atomic,
				  dev->hw_attrs.max_hw_ord);
			return -EINVAL;
		}
		if (attr->max_rd_atomic)
			roce_info->ord_size = attr->max_rd_atomic;
		info.ord_valid = true;
	}

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) {
		if (attr->max_dest_rd_atomic > dev->hw_attrs.max_hw_ird) {
			ibdev_err(&iwdev->ibdev,
				  "rd_atomic = %d, above max_hw_ird=%d\n",
				  attr->max_rd_atomic,
				  dev->hw_attrs.max_hw_ird);
			return -EINVAL;
		}
		if (attr->max_dest_rd_atomic)
			roce_info->ird_size = attr->max_dest_rd_atomic;
	}

	if (attr_mask & IB_QP_ACCESS_FLAGS) {
		if (attr->qp_access_flags & IB_ACCESS_LOCAL_WRITE)
			roce_info->wr_rdresp_en = true;
		if (attr->qp_access_flags & IB_ACCESS_REMOTE_WRITE)
			roce_info->wr_rdresp_en = true;
		if (attr->qp_access_flags & IB_ACCESS_REMOTE_READ)
			roce_info->rd_en = true;
	}

	wait_event(iwqp->mod_qp_waitq, !atomic_read(&iwqp->hw_mod_qp_pend));

	irdma_debug(dev, IRDMA_DEBUG_VERBS,
		    "caller: %pS qp_id=%d to_ibqpstate=%d ibqpstate=%d irdma_qpstate=%d attr_mask=0x%x\n",
		    __builtin_return_address(0), ibqp->qp_num, attr->qp_state, iwqp->ibqp_state,
		    iwqp->iwarp_state, attr_mask);

	spin_lock_irqsave(&iwqp->lock, flags);
	if (attr_mask & IB_QP_STATE) {
		if (!ib_modify_qp_is_ok(iwqp->ibqp_state, attr->qp_state, iwqp->ibqp.qp_type, attr_mask)) {
			irdma_print("modify_qp invalid for qp_id=%d, old_state=0x%x, new_state=0x%x\n",
				    iwqp->ibqp.qp_num, iwqp->ibqp_state,
				    attr->qp_state);
			ret = -EINVAL;
			goto exit;
		}
		info.curr_iwarp_state = iwqp->iwarp_state;

		switch (attr->qp_state) {
		case IB_QPS_INIT:
			if (iwqp->iwarp_state > IRDMA_QP_STATE_IDLE) {
				ret = -EINVAL;
				goto exit;
			}

			if (iwqp->iwarp_state == IRDMA_QP_STATE_INVALID) {
				info.next_iwarp_state = IRDMA_QP_STATE_IDLE;
				issue_modify_qp = 1;
			}
			break;
		case IB_QPS_RTR:
			if (iwqp->iwarp_state > IRDMA_QP_STATE_IDLE) {
				ret = -EINVAL;
				goto exit;
			}
			info.arp_cache_idx_valid = true;
			info.cq_num_valid = true;
			info.next_iwarp_state = IRDMA_QP_STATE_RTR;
			issue_modify_qp = 1;
			break;
		case IB_QPS_RTS:
			if (iwqp->ibqp_state < IB_QPS_RTR ||
			    iwqp->ibqp_state == IB_QPS_ERR) {
				ret = -EINVAL;
				goto exit;
			}

			info.arp_cache_idx_valid = true;
			info.cq_num_valid = true;
			info.ord_valid = true;
			info.next_iwarp_state = IRDMA_QP_STATE_RTS;
			issue_modify_qp = 1;
			if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_2)
				iwdev->rf->check_fc(&iwdev->vsi, &iwqp->sc_qp);
			udp_info->cwnd = iwdev->roce_cwnd;
			roce_info->ack_credits = iwdev->roce_ackcreds;
			if (iwdev->push_mode && udata &&
			    iwqp->sc_qp.push_idx == IRDMA_INVALID_PUSH_PAGE_INDEX &&
			    dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
				spin_unlock_irqrestore(&iwqp->lock, flags);
				irdma_alloc_push_page(iwqp);
				spin_lock_irqsave(&iwqp->lock, flags);
			}
			break;
		case IB_QPS_SQD:
			if (iwqp->iwarp_state == IRDMA_QP_STATE_SQD)
				goto exit;

			if (iwqp->iwarp_state != IRDMA_QP_STATE_RTS) {
				ret = -EINVAL;
				goto exit;
			}

			info.next_iwarp_state = IRDMA_QP_STATE_SQD;
			issue_modify_qp = 1;
			break;
		case IB_QPS_SQE:
		case IB_QPS_ERR:
		case IB_QPS_RESET:
			if (iwqp->iwarp_state == IRDMA_QP_STATE_RTS) {
				spin_unlock_irqrestore(&iwqp->lock, flags);
				info.next_iwarp_state = IRDMA_QP_STATE_SQD;
				irdma_hw_modify_qp(iwdev, iwqp, &info, true);
				spin_lock_irqsave(&iwqp->lock, flags);
			}

			if (iwqp->iwarp_state == IRDMA_QP_STATE_ERROR) {
				spin_unlock_irqrestore(&iwqp->lock, flags);
				if (udata) {
					if (ib_copy_from_udata(&ureq, udata,
							       min(sizeof(ureq), udata->inlen)))
						return -EINVAL;

					irdma_flush_wqes(iwqp,
							 (ureq.sq_flush ? IRDMA_FLUSH_SQ : 0) |
							 (ureq.rq_flush ? IRDMA_FLUSH_RQ : 0) |
							 IRDMA_REFLUSH);
				}
				return 0;
			}

			info.next_iwarp_state = IRDMA_QP_STATE_ERROR;
			issue_modify_qp = 1;
			break;
		default:
			ret = -EINVAL;
			goto exit;
		}

		iwqp->ibqp_state = attr->qp_state;
	}

	ctx_info->send_cq_num = iwqp->iwscq->sc_cq.cq_uk.cq_id;
	ctx_info->rcv_cq_num = iwqp->iwrcq->sc_cq.cq_uk.cq_id;
	irdma_sc_qp_setctx_roce(&iwqp->sc_qp, iwqp->host_ctx.va, ctx_info);
	spin_unlock_irqrestore(&iwqp->lock, flags);

	if (attr_mask & IB_QP_STATE) {
		if (issue_modify_qp) {
			ctx_info->rem_endpoint_idx = udp_info->arp_idx;
			if (irdma_hw_modify_qp(iwdev, iwqp, &info, true))
				return -EINVAL;
			spin_lock_irqsave(&iwqp->lock, flags);
			if (iwqp->iwarp_state == info.curr_iwarp_state) {
				iwqp->iwarp_state = info.next_iwarp_state;
				iwqp->ibqp_state = attr->qp_state;
			}
			if (iwqp->ibqp_state > IB_QPS_RTS &&
			    !iwqp->flush_issued) {
				iwqp->flush_issued = 1;
				if (!iwqp->user_mode)
					queue_delayed_work(iwqp->iwdev->cleanup_wq,
							   &iwqp->dwork_flush,
							   msecs_to_jiffies(IRDMA_FLUSH_DELAY_MS));
				spin_unlock_irqrestore(&iwqp->lock, flags);
				irdma_flush_wqes(iwqp, IRDMA_FLUSH_SQ |
						 IRDMA_FLUSH_RQ |
						 IRDMA_FLUSH_WAIT);
			} else {
				spin_unlock_irqrestore(&iwqp->lock, flags);
			}
		} else {
			iwqp->ibqp_state = attr->qp_state;
		}
		if (udata && dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
			struct irdma_ucontext *ucontext;

			ucontext = rdma_udata_to_drv_context(udata,
							     struct irdma_ucontext, ibucontext);
			if (iwqp->sc_qp.push_idx != IRDMA_INVALID_PUSH_PAGE_INDEX &&
			    !iwqp->push_wqe_mmap_entry &&
			    !irdma_setup_push_mmap_entries(ucontext, iwqp,
							   &uresp.push_wqe_mmap_key, &uresp.push_db_mmap_key)) {
				uresp.push_valid = 1;
				uresp.push_offset = iwqp->sc_qp.push_offset;
			}
			uresp.rd_fence_rate = iwdev->rd_fence_rate;
			ret = ib_copy_to_udata(udata, &uresp, min(sizeof(uresp),
								  udata->outlen));
			if (ret) {
				irdma_remove_push_mmap_entries(iwqp);
				irdma_debug(iwdev_to_idev(iwdev),
					    IRDMA_DEBUG_VERBS,
					    "copy_to_udata failed\n");
				return ret;
			}
		}
	}

	return 0;
exit:
	spin_unlock_irqrestore(&iwqp->lock, flags);

	return ret;
}

/**
 * irdma_modify_qp - modify qp request
 * @ibqp: qp's pointer for modify
 * @attr: access attributes
 * @attr_mask: state mask
 * @udata: user data
 */
int
irdma_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr, int attr_mask,
		struct ib_udata *udata)
{
	struct irdma_qp *iwqp = to_iwqp(ibqp);
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_sc_dev *dev = &iwdev->rf->sc_dev;
	struct irdma_qp_host_ctx_info *ctx_info;
	struct irdma_tcp_offload_info *tcp_info;
	struct irdma_iwarp_offload_info *offload_info;
	struct irdma_modify_qp_info info = {0};
	struct irdma_modify_qp_resp uresp = {};
	struct irdma_modify_qp_req ureq = {};
	u8 issue_modify_qp = 0;
	u8 dont_wait = 0;
	int err;
	unsigned long flags;

	if (attr_mask & ~IB_QP_ATTR_STANDARD_BITS)
		return -EOPNOTSUPP;

	ctx_info = &iwqp->ctx_info;
	offload_info = &iwqp->iwarp_info;
	tcp_info = &iwqp->tcp_info;
	wait_event(iwqp->mod_qp_waitq, !atomic_read(&iwqp->hw_mod_qp_pend));
	irdma_debug(dev, IRDMA_DEBUG_VERBS,
		    "caller: %pS qp_id=%d to_ibqpstate=%d ibqpstate=%d irdma_qpstate=%d last_aeq=%d hw_tcp_state=%d hw_iwarp_state=%d attr_mask=0x%x\n",
		    __builtin_return_address(0), ibqp->qp_num, attr->qp_state, iwqp->ibqp_state, iwqp->iwarp_state,
		    iwqp->last_aeq, iwqp->hw_tcp_state, iwqp->hw_iwarp_state, attr_mask);

	spin_lock_irqsave(&iwqp->lock, flags);
	if (attr_mask & IB_QP_STATE) {
		info.curr_iwarp_state = iwqp->iwarp_state;
		switch (attr->qp_state) {
		case IB_QPS_INIT:
		case IB_QPS_RTR:
			if (iwqp->iwarp_state > IRDMA_QP_STATE_IDLE) {
				err = -EINVAL;
				goto exit;
			}

			if (iwqp->iwarp_state == IRDMA_QP_STATE_INVALID) {
				info.next_iwarp_state = IRDMA_QP_STATE_IDLE;
				issue_modify_qp = 1;
			}
			if (iwdev->push_mode && udata &&
			    iwqp->sc_qp.push_idx == IRDMA_INVALID_PUSH_PAGE_INDEX &&
			    dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
				spin_unlock_irqrestore(&iwqp->lock, flags);
				irdma_alloc_push_page(iwqp);
				spin_lock_irqsave(&iwqp->lock, flags);
			}
			break;
		case IB_QPS_RTS:
			if (iwqp->iwarp_state > IRDMA_QP_STATE_RTS ||
			    !iwqp->cm_id) {
				err = -EINVAL;
				goto exit;
			}

			issue_modify_qp = 1;
			iwqp->hw_tcp_state = IRDMA_TCP_STATE_ESTABLISHED;
			iwqp->hte_added = 1;
			info.next_iwarp_state = IRDMA_QP_STATE_RTS;
			info.tcp_ctx_valid = true;
			info.ord_valid = true;
			info.arp_cache_idx_valid = true;
			info.cq_num_valid = true;
			break;
		case IB_QPS_SQD:
			if (iwqp->hw_iwarp_state > IRDMA_QP_STATE_RTS) {
				err = 0;
				goto exit;
			}

			if (iwqp->iwarp_state == IRDMA_QP_STATE_CLOSING ||
			    iwqp->iwarp_state < IRDMA_QP_STATE_RTS) {
				err = 0;
				goto exit;
			}

			if (iwqp->iwarp_state > IRDMA_QP_STATE_CLOSING) {
				err = -EINVAL;
				goto exit;
			}

			info.next_iwarp_state = IRDMA_QP_STATE_CLOSING;
			issue_modify_qp = 1;
			break;
		case IB_QPS_SQE:
			if (iwqp->iwarp_state >= IRDMA_QP_STATE_TERMINATE) {
				err = -EINVAL;
				goto exit;
			}

			/* fallthrough */
		case IB_QPS_ERR:
		case IB_QPS_RESET:
			if (iwqp->iwarp_state == IRDMA_QP_STATE_ERROR) {
				spin_unlock_irqrestore(&iwqp->lock, flags);
				if (udata) {
					if (ib_copy_from_udata(&ureq, udata,
							       min(sizeof(ureq), udata->inlen)))
						return -EINVAL;

					irdma_flush_wqes(iwqp,
							 (ureq.sq_flush ? IRDMA_FLUSH_SQ : 0) |
							 (ureq.rq_flush ? IRDMA_FLUSH_RQ : 0) |
							 IRDMA_REFLUSH);
				}
				return 0;
			}

			if (iwqp->sc_qp.term_flags) {
				spin_unlock_irqrestore(&iwqp->lock, flags);
				irdma_terminate_del_timer(&iwqp->sc_qp);
				spin_lock_irqsave(&iwqp->lock, flags);
			}
			info.next_iwarp_state = IRDMA_QP_STATE_ERROR;
			if (iwqp->hw_tcp_state > IRDMA_TCP_STATE_CLOSED &&
			    iwdev->iw_status &&
			    iwqp->hw_tcp_state != IRDMA_TCP_STATE_TIME_WAIT)
				info.reset_tcp_conn = true;
			else
				dont_wait = 1;

			issue_modify_qp = 1;
			info.next_iwarp_state = IRDMA_QP_STATE_ERROR;
			break;
		default:
			err = -EINVAL;
			goto exit;
		}

		iwqp->ibqp_state = attr->qp_state;
	}
	if (attr_mask & IB_QP_ACCESS_FLAGS) {
		ctx_info->iwarp_info_valid = true;
		if (attr->qp_access_flags & IB_ACCESS_LOCAL_WRITE)
			offload_info->wr_rdresp_en = true;
		if (attr->qp_access_flags & IB_ACCESS_REMOTE_WRITE)
			offload_info->wr_rdresp_en = true;
		if (attr->qp_access_flags & IB_ACCESS_REMOTE_READ)
			offload_info->rd_en = true;
	}

	if (ctx_info->iwarp_info_valid) {
		ctx_info->send_cq_num = iwqp->iwscq->sc_cq.cq_uk.cq_id;
		ctx_info->rcv_cq_num = iwqp->iwrcq->sc_cq.cq_uk.cq_id;
		irdma_sc_qp_setctx(&iwqp->sc_qp, iwqp->host_ctx.va, ctx_info);
	}
	spin_unlock_irqrestore(&iwqp->lock, flags);

	if (attr_mask & IB_QP_STATE) {
		if (issue_modify_qp) {
			ctx_info->rem_endpoint_idx = tcp_info->arp_idx;
			if (irdma_hw_modify_qp(iwdev, iwqp, &info, true))
				return -EINVAL;
		}

		spin_lock_irqsave(&iwqp->lock, flags);
		if (iwqp->iwarp_state == info.curr_iwarp_state) {
			iwqp->iwarp_state = info.next_iwarp_state;
			iwqp->ibqp_state = attr->qp_state;
		}
		spin_unlock_irqrestore(&iwqp->lock, flags);
	}

	if (issue_modify_qp && iwqp->ibqp_state > IB_QPS_RTS) {
		if (dont_wait) {
			if (iwqp->cm_id && iwqp->hw_tcp_state) {
				spin_lock_irqsave(&iwqp->lock, flags);
				iwqp->hw_tcp_state = IRDMA_TCP_STATE_CLOSED;
				iwqp->last_aeq = IRDMA_AE_RESET_SENT;
				spin_unlock_irqrestore(&iwqp->lock, flags);
				irdma_cm_disconn(iwqp);
			}
		} else {
			int close_timer_started;

			spin_lock_irqsave(&iwdev->cm_core.ht_lock, flags);

			if (iwqp->cm_node) {
				atomic_inc(&iwqp->cm_node->refcnt);
				spin_unlock_irqrestore(&iwdev->cm_core.ht_lock, flags);
				close_timer_started = atomic_inc_return(&iwqp->close_timer_started);
				if (iwqp->cm_id && close_timer_started == 1)
					irdma_schedule_cm_timer(iwqp->cm_node,
								(struct irdma_puda_buf *)iwqp,
								IRDMA_TIMER_TYPE_CLOSE, 1, 0);

				irdma_rem_ref_cm_node(iwqp->cm_node);
			} else {
				spin_unlock_irqrestore(&iwdev->cm_core.ht_lock, flags);
			}
		}
	}
	if (attr_mask & IB_QP_STATE && udata &&
	    dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2) {
		struct irdma_ucontext *ucontext;

		ucontext = rdma_udata_to_drv_context(udata,
						     struct irdma_ucontext, ibucontext);
		if (iwqp->sc_qp.push_idx != IRDMA_INVALID_PUSH_PAGE_INDEX &&
		    !iwqp->push_wqe_mmap_entry &&
		    !irdma_setup_push_mmap_entries(ucontext, iwqp,
						   &uresp.push_wqe_mmap_key, &uresp.push_db_mmap_key)) {
			uresp.push_valid = 1;
			uresp.push_offset = iwqp->sc_qp.push_offset;
		}
		uresp.rd_fence_rate = iwdev->rd_fence_rate;

		err = ib_copy_to_udata(udata, &uresp, min(sizeof(uresp),
							  udata->outlen));
		if (err) {
			irdma_remove_push_mmap_entries(iwqp);
			irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
				    "copy_to_udata failed\n");
			return err;
		}
	}

	return 0;
exit:
	spin_unlock_irqrestore(&iwqp->lock, flags);

	return err;
}

/**
 * irdma_cq_free_rsrc - free up resources for cq
 * @rf: RDMA PCI function
 * @iwcq: cq ptr
 */
void
irdma_cq_free_rsrc(struct irdma_pci_f *rf, struct irdma_cq *iwcq)
{
	struct irdma_sc_cq *cq = &iwcq->sc_cq;

	if (!iwcq->user_mode) {
		irdma_free_dma_mem(rf->sc_dev.hw, &iwcq->kmem);
		irdma_free_dma_mem(rf->sc_dev.hw, &iwcq->kmem_shadow);
	}

	irdma_free_rsrc(rf, rf->allocated_cqs, cq->cq_uk.cq_id);
}

/**
 * irdma_free_cqbuf - worker to free a cq buffer
 * @work: provides access to the cq buffer to free
 */
static void
irdma_free_cqbuf(struct work_struct *work)
{
	struct irdma_cq_buf *cq_buf = container_of(work, struct irdma_cq_buf, work);

	irdma_free_dma_mem(cq_buf->hw, &cq_buf->kmem_buf);
	kfree(cq_buf);
}

/**
 * irdma_process_resize_list - remove resized cq buffers from the resize_list
 * @iwcq: cq which owns the resize_list
 * @iwdev: irdma device
 * @lcqe_buf: the buffer where the last cqe is received
 */
int
irdma_process_resize_list(struct irdma_cq *iwcq,
			  struct irdma_device *iwdev,
			  struct irdma_cq_buf *lcqe_buf)
{
	struct list_head *tmp_node, *list_node;
	struct irdma_cq_buf *cq_buf;
	int cnt = 0;

	list_for_each_safe(list_node, tmp_node, &iwcq->resize_list) {
		cq_buf = list_entry(list_node, struct irdma_cq_buf, list);
		if (cq_buf == lcqe_buf)
			return cnt;

		list_del(&cq_buf->list);
		queue_work(iwdev->cleanup_wq, &cq_buf->work);
		cnt++;
	}

	return cnt;
}

/**
 * irdma_resize_cq - resize cq
 * @ibcq: cq to be resized
 * @entries: desired cq size
 * @udata: user data
 */
static int
irdma_resize_cq(struct ib_cq *ibcq, int entries,
		struct ib_udata *udata)
{
	struct irdma_cq *iwcq = to_iwcq(ibcq);
	struct irdma_sc_dev *dev = iwcq->sc_cq.dev;
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	struct irdma_modify_cq_info *m_info;
	struct irdma_modify_cq_info info = {0};
	struct irdma_dma_mem kmem_buf;
	struct irdma_cq_mr *cqmr_buf;
	struct irdma_pbl *iwpbl_buf;
	struct irdma_device *iwdev;
	struct irdma_pci_f *rf;
	struct irdma_cq_buf *cq_buf = NULL;
	unsigned long flags;
	int ret;

	iwdev = to_iwdev(ibcq->device);
	rf = iwdev->rf;

	if (!(rf->sc_dev.hw_attrs.uk_attrs.feature_flags &
	      IRDMA_FEATURE_CQ_RESIZE))
		return -EOPNOTSUPP;

	if (entries > rf->max_cqe)
		return -EINVAL;

	if (!iwcq->user_mode) {
		entries++;
		if (rf->sc_dev.hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2)
			entries *= 2;
	}

	info.cq_size = max(entries, 4);

	if (info.cq_size == iwcq->sc_cq.cq_uk.cq_size - 1)
		return 0;

	if (udata) {
		struct irdma_resize_cq_req req = {0};
		struct irdma_ucontext *ucontext =
		rdma_udata_to_drv_context(udata, struct irdma_ucontext,
					  ibucontext);

		/* CQ resize not supported with legacy GEN_1 libi40iw */
		if (ucontext->legacy_mode)
			return -EOPNOTSUPP;

		if (ib_copy_from_udata(&req, udata,
				       min(sizeof(req), udata->inlen)))
			return -EINVAL;

		spin_lock_irqsave(&ucontext->cq_reg_mem_list_lock, flags);
		iwpbl_buf = irdma_get_pbl((unsigned long)req.user_cq_buffer,
					  &ucontext->cq_reg_mem_list);
		spin_unlock_irqrestore(&ucontext->cq_reg_mem_list_lock, flags);

		if (!iwpbl_buf)
			return -ENOMEM;

		cqmr_buf = &iwpbl_buf->cq_mr;
		if (iwpbl_buf->pbl_allocated) {
			info.virtual_map = true;
			info.pbl_chunk_size = 1;
			info.first_pm_pbl_idx = cqmr_buf->cq_pbl.idx;
		} else {
			info.cq_pa = cqmr_buf->cq_pbl.addr;
		}
	} else {
		/* Kmode CQ resize */
		int rsize;

		rsize = info.cq_size * sizeof(struct irdma_cqe);
		kmem_buf.size = round_up(rsize, 256);
		kmem_buf.va = irdma_allocate_dma_mem(dev->hw, &kmem_buf,
						     kmem_buf.size, 256);
		if (!kmem_buf.va)
			return -ENOMEM;

		info.cq_base = kmem_buf.va;
		info.cq_pa = kmem_buf.pa;
		cq_buf = kzalloc(sizeof(*cq_buf), GFP_KERNEL);
		if (!cq_buf) {
			ret = -ENOMEM;
			goto error;
		}
	}

	cqp_request = irdma_alloc_and_get_cqp_request(&rf->cqp, true);
	if (!cqp_request) {
		ret = -ENOMEM;
		goto error;
	}

	info.shadow_read_threshold = iwcq->sc_cq.shadow_read_threshold;
	info.cq_resize = true;

	cqp_info = &cqp_request->info;
	m_info = &cqp_info->in.u.cq_modify.info;
	memcpy(m_info, &info, sizeof(*m_info));

	cqp_info->cqp_cmd = IRDMA_OP_CQ_MODIFY;
	cqp_info->in.u.cq_modify.cq = &iwcq->sc_cq;
	cqp_info->in.u.cq_modify.scratch = (uintptr_t)cqp_request;
	cqp_info->post_sq = 1;
	ret = irdma_handle_cqp_op(rf, cqp_request);
	irdma_put_cqp_request(&rf->cqp, cqp_request);
	if (ret)
		goto error;

	spin_lock_irqsave(&iwcq->lock, flags);
	if (cq_buf) {
		cq_buf->kmem_buf = iwcq->kmem;
		cq_buf->hw = dev->hw;
		memcpy(&cq_buf->cq_uk, &iwcq->sc_cq.cq_uk, sizeof(cq_buf->cq_uk));
		INIT_WORK(&cq_buf->work, irdma_free_cqbuf);
		list_add_tail(&cq_buf->list, &iwcq->resize_list);
		iwcq->kmem = kmem_buf;
	}

	irdma_sc_cq_resize(&iwcq->sc_cq, &info);
	ibcq->cqe = info.cq_size - 1;
	spin_unlock_irqrestore(&iwcq->lock, flags);

	return 0;
error:
	if (!udata)
		irdma_free_dma_mem(dev->hw, &kmem_buf);
	kfree(cq_buf);

	return ret;
}

/**
 * irdma_get_mr_access - get hw MR access permissions from IB access flags
 * @access: IB access flags
 */
static inline u16 irdma_get_mr_access(int access){
	u16 hw_access = 0;

	hw_access |= (access & IB_ACCESS_LOCAL_WRITE) ?
	    IRDMA_ACCESS_FLAGS_LOCALWRITE : 0;
	hw_access |= (access & IB_ACCESS_REMOTE_WRITE) ?
	    IRDMA_ACCESS_FLAGS_REMOTEWRITE : 0;
	hw_access |= (access & IB_ACCESS_REMOTE_READ) ?
	    IRDMA_ACCESS_FLAGS_REMOTEREAD : 0;
	hw_access |= (access & IB_ACCESS_MW_BIND) ?
	    IRDMA_ACCESS_FLAGS_BIND_WINDOW : 0;
	hw_access |= (access & IB_ZERO_BASED) ?
	    IRDMA_ACCESS_FLAGS_ZERO_BASED : 0;
	hw_access |= IRDMA_ACCESS_FLAGS_LOCALREAD;

	return hw_access;
}

/**
 * irdma_free_stag - free stag resource
 * @iwdev: irdma device
 * @stag: stag to free
 */
void
irdma_free_stag(struct irdma_device *iwdev, u32 stag)
{
	u32 stag_idx;

	stag_idx = (stag & iwdev->rf->mr_stagmask) >> IRDMA_CQPSQ_STAG_IDX_S;
	irdma_free_rsrc(iwdev->rf, iwdev->rf->allocated_mrs, stag_idx);
}

/**
 * irdma_create_stag - create random stag
 * @iwdev: irdma device
 */
u32
irdma_create_stag(struct irdma_device *iwdev)
{
	u32 stag = 0;
	u32 stag_index = 0;
	u32 next_stag_index;
	u32 driver_key;
	u32 random;
	u8 consumer_key;
	int ret;

	get_random_bytes(&random, sizeof(random));
	consumer_key = (u8)random;

	driver_key = random & ~iwdev->rf->mr_stagmask;
	next_stag_index = (random & iwdev->rf->mr_stagmask) >> 8;
	next_stag_index %= iwdev->rf->max_mr;

	ret = irdma_alloc_rsrc(iwdev->rf, iwdev->rf->allocated_mrs,
			       iwdev->rf->max_mr, &stag_index,
			       &next_stag_index);
	if (ret)
		return stag;
	stag = stag_index << IRDMA_CQPSQ_STAG_IDX_S;
	stag |= driver_key;
	stag += (u32)consumer_key;

	return stag;
}

/**
 * irdma_check_mem_contiguous - check if pbls stored in arr are contiguous
 * @arr: lvl1 pbl array
 * @npages: page count
 * @pg_size: page size
 *
 */
static bool
irdma_check_mem_contiguous(u64 *arr, u32 npages, u32 pg_size)
{
	u32 pg_idx;

	for (pg_idx = 0; pg_idx < npages; pg_idx++) {
		if ((*arr + (pg_size * pg_idx)) != arr[pg_idx])
			return false;
	}

	return true;
}

/**
 * irdma_check_mr_contiguous - check if MR is physically contiguous
 * @palloc: pbl allocation struct
 * @pg_size: page size
 */
static bool
irdma_check_mr_contiguous(struct irdma_pble_alloc *palloc,
			  u32 pg_size)
{
	struct irdma_pble_level2 *lvl2 = &palloc->level2;
	struct irdma_pble_info *leaf = lvl2->leaf;
	u64 *arr = NULL;
	u64 *start_addr = NULL;
	int i;
	bool ret;

	if (palloc->level == PBLE_LEVEL_1) {
		arr = palloc->level1.addr;
		ret = irdma_check_mem_contiguous(arr, palloc->total_cnt,
						 pg_size);
		return ret;
	}

	start_addr = leaf->addr;

	for (i = 0; i < lvl2->leaf_cnt; i++, leaf++) {
		arr = leaf->addr;
		if ((*start_addr + (i * pg_size * PBLE_PER_PAGE)) != *arr)
			return false;
		ret = irdma_check_mem_contiguous(arr, leaf->cnt, pg_size);
		if (!ret)
			return false;
	}

	return true;
}

/**
 * irdma_setup_pbles - copy user pg address to pble's
 * @rf: RDMA PCI function
 * @iwmr: mr pointer for this memory registration
 * @use_pbles: flag if to use pble's
 */
static int
irdma_setup_pbles(struct irdma_pci_f *rf, struct irdma_mr *iwmr,
		  bool use_pbles)
{
	struct irdma_pbl *iwpbl = &iwmr->iwpbl;
	struct irdma_pble_alloc *palloc = &iwpbl->pble_alloc;
	struct irdma_pble_info *pinfo;
	u64 *pbl;
	int status;
	enum irdma_pble_level level = PBLE_LEVEL_1;

	if (use_pbles) {
		status = irdma_get_pble(rf->pble_rsrc, palloc, iwmr->page_cnt,
					false);
		if (status)
			return status;

		iwpbl->pbl_allocated = true;
		level = palloc->level;
		pinfo = (level == PBLE_LEVEL_1) ? &palloc->level1 :
		    palloc->level2.leaf;
		pbl = pinfo->addr;
	} else {
		pbl = iwmr->pgaddrmem;
	}

	irdma_copy_user_pgaddrs(iwmr, pbl, level);

	if (use_pbles)
		iwmr->pgaddrmem[0] = *pbl;

	return 0;
}

/**
 * irdma_handle_q_mem - handle memory for qp and cq
 * @iwdev: irdma device
 * @req: information for q memory management
 * @iwpbl: pble struct
 * @use_pbles: flag to use pble
 */
static int
irdma_handle_q_mem(struct irdma_device *iwdev,
		   struct irdma_mem_reg_req *req,
		   struct irdma_pbl *iwpbl, bool use_pbles)
{
	struct irdma_pble_alloc *palloc = &iwpbl->pble_alloc;
	struct irdma_mr *iwmr = iwpbl->iwmr;
	struct irdma_qp_mr *qpmr = &iwpbl->qp_mr;
	struct irdma_cq_mr *cqmr = &iwpbl->cq_mr;
	struct irdma_hmc_pble *hmc_p;
	u64 *arr = iwmr->pgaddrmem;
	u32 pg_size, total;
	int err = 0;
	bool ret = true;

	pg_size = iwmr->page_size;
	err = irdma_setup_pbles(iwdev->rf, iwmr, use_pbles);
	if (err)
		return err;

	if (use_pbles && palloc->level != PBLE_LEVEL_1) {
		irdma_free_pble(iwdev->rf->pble_rsrc, palloc);
		iwpbl->pbl_allocated = false;
		return -ENOMEM;
	}

	if (use_pbles)
		arr = palloc->level1.addr;

	switch (iwmr->type) {
	case IRDMA_MEMREG_TYPE_QP:
		total = req->sq_pages + req->rq_pages;
		hmc_p = &qpmr->sq_pbl;
		qpmr->shadow = (dma_addr_t) arr[total];
		if (use_pbles) {
			ret = irdma_check_mem_contiguous(arr, req->sq_pages,
							 pg_size);
			if (ret)
				ret = irdma_check_mem_contiguous(&arr[req->sq_pages],
								 req->rq_pages,
								 pg_size);
		}

		if (!ret) {
			hmc_p->idx = palloc->level1.idx;
			hmc_p = &qpmr->rq_pbl;
			hmc_p->idx = palloc->level1.idx + req->sq_pages;
		} else {
			hmc_p->addr = arr[0];
			hmc_p = &qpmr->rq_pbl;
			hmc_p->addr = arr[req->sq_pages];
		}
		break;
	case IRDMA_MEMREG_TYPE_CQ:
		hmc_p = &cqmr->cq_pbl;

		if (!cqmr->split)
			cqmr->shadow = (dma_addr_t) arr[req->cq_pages];

		if (use_pbles)
			ret = irdma_check_mem_contiguous(arr, req->cq_pages,
							 pg_size);

		if (!ret)
			hmc_p->idx = palloc->level1.idx;
		else
			hmc_p->addr = arr[0];
		break;
	default:
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "MR type error\n");
		err = -EINVAL;
	}

	if (use_pbles && ret) {
		irdma_free_pble(iwdev->rf->pble_rsrc, palloc);
		iwpbl->pbl_allocated = false;
	}

	return err;
}

/**
 * irdma_hw_alloc_mw - create the hw memory window
 * @iwdev: irdma device
 * @iwmr: pointer to memory window info
 */
int
irdma_hw_alloc_mw(struct irdma_device *iwdev, struct irdma_mr *iwmr)
{
	struct irdma_mw_alloc_info *info;
	struct irdma_pd *iwpd = to_iwpd(iwmr->ibmr.pd);
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	int status;

	cqp_request = irdma_alloc_and_get_cqp_request(&iwdev->rf->cqp, true);
	if (!cqp_request)
		return -ENOMEM;

	cqp_info = &cqp_request->info;
	info = &cqp_info->in.u.mw_alloc.info;
	memset(info, 0, sizeof(*info));
	if (iwmr->ibmw.type == IB_MW_TYPE_1)
		info->mw_wide = true;

	info->page_size = PAGE_SIZE;
	info->mw_stag_index = iwmr->stag >> IRDMA_CQPSQ_STAG_IDX_S;
	info->pd_id = iwpd->sc_pd.pd_id;
	info->remote_access = true;
	cqp_info->cqp_cmd = IRDMA_OP_MW_ALLOC;
	cqp_info->post_sq = 1;
	cqp_info->in.u.mw_alloc.dev = &iwdev->rf->sc_dev;
	cqp_info->in.u.mw_alloc.scratch = (uintptr_t)cqp_request;
	status = irdma_handle_cqp_op(iwdev->rf, cqp_request);
	irdma_put_cqp_request(&iwdev->rf->cqp, cqp_request);

	return status;
}

/**
 * irdma_dealloc_mw - Dealloc memory window
 * @ibmw: memory window structure.
 */
static int
irdma_dealloc_mw(struct ib_mw *ibmw)
{
	struct ib_pd *ibpd = ibmw->pd;
	struct irdma_pd *iwpd = to_iwpd(ibpd);
	struct irdma_mr *iwmr = to_iwmr((struct ib_mr *)ibmw);
	struct irdma_device *iwdev = to_iwdev(ibmw->device);
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	struct irdma_dealloc_stag_info *info;

	cqp_request = irdma_alloc_and_get_cqp_request(&iwdev->rf->cqp, true);
	if (!cqp_request)
		return -ENOMEM;

	cqp_info = &cqp_request->info;
	info = &cqp_info->in.u.dealloc_stag.info;
	memset(info, 0, sizeof(*info));
	info->pd_id = iwpd->sc_pd.pd_id & 0x00007fff;
	info->stag_idx = RS_64_1(ibmw->rkey, IRDMA_CQPSQ_STAG_IDX_S);
	info->mr = false;
	cqp_info->cqp_cmd = IRDMA_OP_DEALLOC_STAG;
	cqp_info->post_sq = 1;
	cqp_info->in.u.dealloc_stag.dev = &iwdev->rf->sc_dev;
	cqp_info->in.u.dealloc_stag.scratch = (uintptr_t)cqp_request;
	irdma_handle_cqp_op(iwdev->rf, cqp_request);
	irdma_put_cqp_request(&iwdev->rf->cqp, cqp_request);
	irdma_free_stag(iwdev, iwmr->stag);
	kfree(iwmr);

	return 0;
}

/**
 * irdma_hw_alloc_stag - cqp command to allocate stag
 * @iwdev: irdma device
 * @iwmr: irdma mr pointer
 */
int
irdma_hw_alloc_stag(struct irdma_device *iwdev,
		    struct irdma_mr *iwmr)
{
	struct irdma_allocate_stag_info *info;
	struct irdma_pd *iwpd = to_iwpd(iwmr->ibmr.pd);
	int status;
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;

	cqp_request = irdma_alloc_and_get_cqp_request(&iwdev->rf->cqp, true);
	if (!cqp_request)
		return -ENOMEM;

	cqp_info = &cqp_request->info;
	info = &cqp_info->in.u.alloc_stag.info;
	memset(info, 0, sizeof(*info));
	info->page_size = PAGE_SIZE;
	info->stag_idx = iwmr->stag >> IRDMA_CQPSQ_STAG_IDX_S;
	info->pd_id = iwpd->sc_pd.pd_id;
	info->total_len = iwmr->len;
	info->remote_access = true;
	cqp_info->cqp_cmd = IRDMA_OP_ALLOC_STAG;
	cqp_info->post_sq = 1;
	cqp_info->in.u.alloc_stag.dev = &iwdev->rf->sc_dev;
	cqp_info->in.u.alloc_stag.scratch = (uintptr_t)cqp_request;
	status = irdma_handle_cqp_op(iwdev->rf, cqp_request);
	irdma_put_cqp_request(&iwdev->rf->cqp, cqp_request);

	return status;
}

/**
 * irdma_set_page - populate pbl list for fmr
 * @ibmr: ib mem to access iwarp mr pointer
 * @addr: page dma address fro pbl list
 */
static int
irdma_set_page(struct ib_mr *ibmr, u64 addr)
{
	struct irdma_mr *iwmr = to_iwmr(ibmr);
	struct irdma_pbl *iwpbl = &iwmr->iwpbl;
	struct irdma_pble_alloc *palloc = &iwpbl->pble_alloc;
	u64 *pbl;

	if (unlikely(iwmr->npages == iwmr->page_cnt))
		return -ENOMEM;

	pbl = palloc->level1.addr;
	pbl[iwmr->npages++] = addr;

	return 0;
}

/**
 * irdma_map_mr_sg - map of sg list for fmr
 * @ibmr: ib mem to access iwarp mr pointer
 * @sg: scatter gather list
 * @sg_nents: number of sg pages
 * @sg_offset: scatter gather list for fmr
 */
static int
irdma_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		int sg_nents, unsigned int *sg_offset)
{
	struct irdma_mr *iwmr = to_iwmr(ibmr);

	iwmr->npages = 0;

	return ib_sg_to_pages(ibmr, sg, sg_nents, sg_offset, irdma_set_page);
}

/**
 * irdma_hwreg_mr - send cqp command for memory registration
 * @iwdev: irdma device
 * @iwmr: irdma mr pointer
 * @access: access for MR
 */
static int
irdma_hwreg_mr(struct irdma_device *iwdev, struct irdma_mr *iwmr,
	       u16 access)
{
	struct irdma_pbl *iwpbl = &iwmr->iwpbl;
	struct irdma_reg_ns_stag_info *stag_info;
	struct irdma_pd *iwpd = to_iwpd(iwmr->ibmr.pd);
	struct irdma_pble_alloc *palloc = &iwpbl->pble_alloc;
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	int ret;

	cqp_request = irdma_alloc_and_get_cqp_request(&iwdev->rf->cqp, true);
	if (!cqp_request)
		return -ENOMEM;

	cqp_info = &cqp_request->info;
	stag_info = &cqp_info->in.u.mr_reg_non_shared.info;
	memset(stag_info, 0, sizeof(*stag_info));
	stag_info->va = iwpbl->user_base;
	stag_info->stag_idx = iwmr->stag >> IRDMA_CQPSQ_STAG_IDX_S;
	stag_info->stag_key = (u8)iwmr->stag;
	stag_info->total_len = iwmr->len;
	stag_info->access_rights = irdma_get_mr_access(access);
	stag_info->pd_id = iwpd->sc_pd.pd_id;
	if (stag_info->access_rights & IRDMA_ACCESS_FLAGS_ZERO_BASED)
		stag_info->addr_type = IRDMA_ADDR_TYPE_ZERO_BASED;
	else
		stag_info->addr_type = IRDMA_ADDR_TYPE_VA_BASED;
	stag_info->page_size = iwmr->page_size;

	if (iwpbl->pbl_allocated) {
		if (palloc->level == PBLE_LEVEL_1) {
			stag_info->first_pm_pbl_index = palloc->level1.idx;
			stag_info->chunk_size = 1;
		} else {
			stag_info->first_pm_pbl_index = palloc->level2.root.idx;
			stag_info->chunk_size = 3;
		}
	} else {
		stag_info->reg_addr_pa = iwmr->pgaddrmem[0];
	}

	cqp_info->cqp_cmd = IRDMA_OP_MR_REG_NON_SHARED;
	cqp_info->post_sq = 1;
	cqp_info->in.u.mr_reg_non_shared.dev = &iwdev->rf->sc_dev;
	cqp_info->in.u.mr_reg_non_shared.scratch = (uintptr_t)cqp_request;
	ret = irdma_handle_cqp_op(iwdev->rf, cqp_request);
	irdma_put_cqp_request(&iwdev->rf->cqp, cqp_request);

	return ret;
}

/**
 * irdma_reg_user_mr - Register a user memory region
 * @pd: ptr of pd
 * @start: virtual start address
 * @len: length of mr
 * @virt: virtual address
 * @access: access of mr
 * @udata: user data
 */
static struct ib_mr *
irdma_reg_user_mr(struct ib_pd *pd, u64 start, u64 len,
		  u64 virt, int access,
		  struct ib_udata *udata)
{
	struct irdma_device *iwdev = to_iwdev(pd->device);
	struct irdma_ucontext *ucontext;
	struct irdma_pble_alloc *palloc;
	struct irdma_pbl *iwpbl;
	struct irdma_mr *iwmr;
	struct ib_umem *region;
	struct irdma_mem_reg_req req;
	u32 total, stag = 0;
	u8 shadow_pgcnt = 1;
	bool use_pbles = false;
	unsigned long flags;
	int err = -EINVAL;
	int ret;

	if (!len || len > iwdev->rf->sc_dev.hw_attrs.max_mr_size)
		return ERR_PTR(-EINVAL);

	region = ib_umem_get(pd->uobject->context, start, len, access, 0);

	if (IS_ERR(region)) {
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "Failed to create ib_umem region\n");
		return (struct ib_mr *)region;
	}

	if (ib_copy_from_udata(&req, udata, min(sizeof(req), udata->inlen))) {
		ib_umem_release(region);
		return ERR_PTR(-EFAULT);
	}

	iwmr = kzalloc(sizeof(*iwmr), GFP_KERNEL);
	if (!iwmr) {
		ib_umem_release(region);
		return ERR_PTR(-ENOMEM);
	}

	iwpbl = &iwmr->iwpbl;
	iwpbl->iwmr = iwmr;
	iwmr->region = region;
	iwmr->ibmr.pd = pd;
	iwmr->ibmr.device = pd->device;
	iwmr->ibmr.iova = virt;
	iwmr->page_size = PAGE_SIZE;

	iwmr->page_msk = PAGE_MASK;
	iwmr->len = region->length;
	iwpbl->user_base = virt;
	palloc = &iwpbl->pble_alloc;
	iwmr->type = req.reg_type;
	iwmr->page_cnt = irdma_ib_umem_num_dma_blocks(region, iwmr->page_size, virt);

	switch (req.reg_type) {
	case IRDMA_MEMREG_TYPE_QP:
		total = req.sq_pages + req.rq_pages + shadow_pgcnt;
		if (total > iwmr->page_cnt) {
			err = -EINVAL;
			goto error;
		}
		total = req.sq_pages + req.rq_pages;
		use_pbles = (total > 2);
		err = irdma_handle_q_mem(iwdev, &req, iwpbl, use_pbles);
		if (err)
			goto error;

		ucontext = rdma_udata_to_drv_context(udata, struct irdma_ucontext,
						     ibucontext);
		spin_lock_irqsave(&ucontext->qp_reg_mem_list_lock, flags);
		list_add_tail(&iwpbl->list, &ucontext->qp_reg_mem_list);
		iwpbl->on_list = true;
		spin_unlock_irqrestore(&ucontext->qp_reg_mem_list_lock, flags);
		break;
	case IRDMA_MEMREG_TYPE_CQ:
		if (iwdev->rf->sc_dev.hw_attrs.uk_attrs.feature_flags & IRDMA_FEATURE_CQ_RESIZE)
			shadow_pgcnt = 0;
		total = req.cq_pages + shadow_pgcnt;
		if (total > iwmr->page_cnt) {
			err = -EINVAL;
			goto error;
		}

		use_pbles = (req.cq_pages > 1);
		err = irdma_handle_q_mem(iwdev, &req, iwpbl, use_pbles);
		if (err)
			goto error;

		ucontext = rdma_udata_to_drv_context(udata, struct irdma_ucontext,
						     ibucontext);
		spin_lock_irqsave(&ucontext->cq_reg_mem_list_lock, flags);
		list_add_tail(&iwpbl->list, &ucontext->cq_reg_mem_list);
		iwpbl->on_list = true;
		spin_unlock_irqrestore(&ucontext->cq_reg_mem_list_lock, flags);
		break;
	case IRDMA_MEMREG_TYPE_MEM:
		use_pbles = (iwmr->page_cnt != 1);

		err = irdma_setup_pbles(iwdev->rf, iwmr, use_pbles);
		if (err)
			goto error;

		if (use_pbles) {
			ret = irdma_check_mr_contiguous(palloc,
							iwmr->page_size);
			if (ret) {
				irdma_free_pble(iwdev->rf->pble_rsrc, palloc);
				iwpbl->pbl_allocated = false;
			}
		}

		stag = irdma_create_stag(iwdev);
		if (!stag) {
			err = -ENOMEM;
			goto error;
		}

		iwmr->stag = stag;
		iwmr->ibmr.rkey = stag;
		iwmr->ibmr.lkey = stag;
		err = irdma_hwreg_mr(iwdev, iwmr, access);
		if (err) {
			irdma_free_stag(iwdev, stag);
			goto error;
		}

		break;
	default:
		goto error;
	}

	iwmr->type = req.reg_type;

	return &iwmr->ibmr;

error:
	if (palloc->level != PBLE_LEVEL_0 && iwpbl->pbl_allocated)
		irdma_free_pble(iwdev->rf->pble_rsrc, palloc);
	ib_umem_release(region);
	kfree(iwmr);

	return ERR_PTR(err);
}

/**
 * irdma_reg_phys_mr - register kernel physical memory
 * @pd: ibpd pointer
 * @addr: physical address of memory to register
 * @size: size of memory to register
 * @access: Access rights
 * @iova_start: start of virtual address for physical buffers
 */
struct ib_mr *
irdma_reg_phys_mr(struct ib_pd *pd, u64 addr, u64 size, int access,
		  u64 *iova_start)
{
	struct irdma_device *iwdev = to_iwdev(pd->device);
	struct irdma_pbl *iwpbl;
	struct irdma_mr *iwmr;
	u32 stag;
	int ret;

	iwmr = kzalloc(sizeof(*iwmr), GFP_KERNEL);
	if (!iwmr)
		return ERR_PTR(-ENOMEM);

	iwmr->ibmr.pd = pd;
	iwmr->ibmr.device = pd->device;
	iwpbl = &iwmr->iwpbl;
	iwpbl->iwmr = iwmr;
	iwmr->type = IRDMA_MEMREG_TYPE_MEM;
	iwpbl->user_base = *iova_start;
	stag = irdma_create_stag(iwdev);
	if (!stag) {
		ret = -ENOMEM;
		goto err;
	}

	iwmr->stag = stag;
	iwmr->ibmr.iova = *iova_start;
	iwmr->ibmr.rkey = stag;
	iwmr->ibmr.lkey = stag;
	iwmr->page_cnt = 1;
	iwmr->pgaddrmem[0] = addr;
	iwmr->len = size;
	iwmr->page_size = SZ_4K;
	ret = irdma_hwreg_mr(iwdev, iwmr, access);
	if (ret) {
		irdma_free_stag(iwdev, stag);
		goto err;
	}

	return &iwmr->ibmr;

err:
	kfree(iwmr);

	return ERR_PTR(ret);
}

/**
 * irdma_get_dma_mr - register physical mem
 * @pd: ptr of pd
 * @acc: access for memory
 */
static struct ib_mr *
irdma_get_dma_mr(struct ib_pd *pd, int acc)
{
	u64 kva = 0;

	return irdma_reg_phys_mr(pd, 0, 0, acc, &kva);
}

/**
 * irdma_del_memlist - Deleting pbl list entries for CQ/QP
 * @iwmr: iwmr for IB's user page addresses
 * @ucontext: ptr to user context
 */
void
irdma_del_memlist(struct irdma_mr *iwmr,
		  struct irdma_ucontext *ucontext)
{
	struct irdma_pbl *iwpbl = &iwmr->iwpbl;
	unsigned long flags;

	switch (iwmr->type) {
	case IRDMA_MEMREG_TYPE_CQ:
		spin_lock_irqsave(&ucontext->cq_reg_mem_list_lock, flags);
		if (iwpbl->on_list) {
			iwpbl->on_list = false;
			list_del(&iwpbl->list);
		}
		spin_unlock_irqrestore(&ucontext->cq_reg_mem_list_lock, flags);
		break;
	case IRDMA_MEMREG_TYPE_QP:
		spin_lock_irqsave(&ucontext->qp_reg_mem_list_lock, flags);
		if (iwpbl->on_list) {
			iwpbl->on_list = false;
			list_del(&iwpbl->list);
		}
		spin_unlock_irqrestore(&ucontext->qp_reg_mem_list_lock, flags);
		break;
	default:
		break;
	}
}

/**
 * irdma_copy_sg_list - copy sg list for qp
 * @sg_list: copied into sg_list
 * @sgl: copy from sgl
 * @num_sges: count of sg entries
 */
static void
irdma_copy_sg_list(struct irdma_sge *sg_list, struct ib_sge *sgl,
		   int num_sges)
{
	unsigned int i;

	for (i = 0; i < num_sges; i++) {
		sg_list[i].tag_off = sgl[i].addr;
		sg_list[i].len = sgl[i].length;
		sg_list[i].stag = sgl[i].lkey;
	}
}

/**
 * irdma_post_send -  kernel application wr
 * @ibqp: qp ptr for wr
 * @ib_wr: work request ptr
 * @bad_wr: return of bad wr if err
 */
static int
irdma_post_send(struct ib_qp *ibqp,
		const struct ib_send_wr *ib_wr,
		const struct ib_send_wr **bad_wr)
{
	struct irdma_qp *iwqp;
	struct irdma_qp_uk *ukqp;
	struct irdma_sc_dev *dev;
	struct irdma_post_sq_info info;
	int err = 0;
	unsigned long flags;
	bool inv_stag;
	struct irdma_ah *ah;

	iwqp = to_iwqp(ibqp);
	ukqp = &iwqp->sc_qp.qp_uk;
	dev = &iwqp->iwdev->rf->sc_dev;

	spin_lock_irqsave(&iwqp->lock, flags);
	while (ib_wr) {
		memset(&info, 0, sizeof(info));
		inv_stag = false;
		info.wr_id = (ib_wr->wr_id);
		if ((ib_wr->send_flags & IB_SEND_SIGNALED) || iwqp->sig_all)
			info.signaled = true;
		if (ib_wr->send_flags & IB_SEND_FENCE)
			info.read_fence = true;
		switch (ib_wr->opcode) {
		case IB_WR_SEND_WITH_IMM:
			if (ukqp->qp_caps & IRDMA_SEND_WITH_IMM) {
				info.imm_data_valid = true;
				info.imm_data = ntohl(ib_wr->ex.imm_data);
			} else {
				err = -EINVAL;
				break;
			}
			/* fallthrough */
		case IB_WR_SEND:
		case IB_WR_SEND_WITH_INV:
			if (ib_wr->opcode == IB_WR_SEND ||
			    ib_wr->opcode == IB_WR_SEND_WITH_IMM) {
				if (ib_wr->send_flags & IB_SEND_SOLICITED)
					info.op_type = IRDMA_OP_TYPE_SEND_SOL;
				else
					info.op_type = IRDMA_OP_TYPE_SEND;
			} else {
				if (ib_wr->send_flags & IB_SEND_SOLICITED)
					info.op_type = IRDMA_OP_TYPE_SEND_SOL_INV;
				else
					info.op_type = IRDMA_OP_TYPE_SEND_INV;
				info.stag_to_inv = ib_wr->ex.invalidate_rkey;
			}

			if (ib_wr->send_flags & IB_SEND_INLINE) {
				info.op.inline_send.data = (void *)(unsigned long)
				    ib_wr->sg_list[0].addr;
				info.op.inline_send.len = ib_wr->sg_list[0].length;
				if (iwqp->ibqp.qp_type == IB_QPT_UD ||
				    iwqp->ibqp.qp_type == IB_QPT_GSI) {
					ah = to_iwah(ud_wr(ib_wr)->ah);
					info.op.inline_send.ah_id = ah->sc_ah.ah_info.ah_idx;
					info.op.inline_send.qkey = ud_wr(ib_wr)->remote_qkey;
					info.op.inline_send.dest_qp = ud_wr(ib_wr)->remote_qpn;
				}
				err = irdma_uk_inline_send(ukqp, &info, false);
			} else {
				info.op.send.num_sges = ib_wr->num_sge;
				info.op.send.sg_list = (struct irdma_sge *)
				    ib_wr->sg_list;
				if (iwqp->ibqp.qp_type == IB_QPT_UD ||
				    iwqp->ibqp.qp_type == IB_QPT_GSI) {
					ah = to_iwah(ud_wr(ib_wr)->ah);
					info.op.send.ah_id = ah->sc_ah.ah_info.ah_idx;
					info.op.send.qkey = ud_wr(ib_wr)->remote_qkey;
					info.op.send.dest_qp = ud_wr(ib_wr)->remote_qpn;
				}
				err = irdma_uk_send(ukqp, &info, false);
			}
			break;
		case IB_WR_RDMA_WRITE_WITH_IMM:
			if (ukqp->qp_caps & IRDMA_WRITE_WITH_IMM) {
				info.imm_data_valid = true;
				info.imm_data = ntohl(ib_wr->ex.imm_data);
			} else {
				err = -EINVAL;
				break;
			}
			/* fallthrough */
		case IB_WR_RDMA_WRITE:
			if (ib_wr->send_flags & IB_SEND_SOLICITED)
				info.op_type = IRDMA_OP_TYPE_RDMA_WRITE_SOL;
			else
				info.op_type = IRDMA_OP_TYPE_RDMA_WRITE;

			if (ib_wr->send_flags & IB_SEND_INLINE) {
				info.op.inline_rdma_write.data = (void *)(uintptr_t)ib_wr->sg_list[0].addr;
				info.op.inline_rdma_write.len = ib_wr->sg_list[0].length;
				info.op.inline_rdma_write.rem_addr.tag_off = rdma_wr(ib_wr)->remote_addr;
				info.op.inline_rdma_write.rem_addr.stag = rdma_wr(ib_wr)->rkey;
				err = irdma_uk_inline_rdma_write(ukqp, &info, false);
			} else {
				info.op.rdma_write.lo_sg_list = (void *)ib_wr->sg_list;
				info.op.rdma_write.num_lo_sges = ib_wr->num_sge;
				info.op.rdma_write.rem_addr.tag_off = rdma_wr(ib_wr)->remote_addr;
				info.op.rdma_write.rem_addr.stag = rdma_wr(ib_wr)->rkey;
				err = irdma_uk_rdma_write(ukqp, &info, false);
			}
			break;
		case IB_WR_RDMA_READ_WITH_INV:
			inv_stag = true;
			/* fallthrough */
		case IB_WR_RDMA_READ:
			if (ib_wr->num_sge >
			    dev->hw_attrs.uk_attrs.max_hw_read_sges) {
				err = -EINVAL;
				break;
			}
			info.op_type = IRDMA_OP_TYPE_RDMA_READ;
			info.op.rdma_read.rem_addr.tag_off = rdma_wr(ib_wr)->remote_addr;
			info.op.rdma_read.rem_addr.stag = rdma_wr(ib_wr)->rkey;
			info.op.rdma_read.lo_sg_list = (void *)ib_wr->sg_list;
			info.op.rdma_read.num_lo_sges = ib_wr->num_sge;
			err = irdma_uk_rdma_read(ukqp, &info, inv_stag, false);
			break;
		case IB_WR_LOCAL_INV:
			info.op_type = IRDMA_OP_TYPE_INV_STAG;
			info.op.inv_local_stag.target_stag = ib_wr->ex.invalidate_rkey;
			err = irdma_uk_stag_local_invalidate(ukqp, &info, true);
			break;
		case IB_WR_REG_MR:{
				struct irdma_mr *iwmr = to_iwmr(reg_wr(ib_wr)->mr);
				struct irdma_pble_alloc *palloc = &iwmr->iwpbl.pble_alloc;
				struct irdma_fast_reg_stag_info stag_info = {0};

				stag_info.signaled = info.signaled;
				stag_info.read_fence = info.read_fence;
				stag_info.access_rights = irdma_get_mr_access(reg_wr(ib_wr)->access);
				stag_info.stag_key = reg_wr(ib_wr)->key & 0xff;
				stag_info.stag_idx = reg_wr(ib_wr)->key >> 8;
				stag_info.page_size = reg_wr(ib_wr)->mr->page_size;
				stag_info.wr_id = ib_wr->wr_id;
				stag_info.addr_type = IRDMA_ADDR_TYPE_VA_BASED;
				stag_info.va = (void *)(uintptr_t)iwmr->ibmr.iova;
				stag_info.total_len = iwmr->ibmr.length;
				stag_info.reg_addr_pa = *palloc->level1.addr;
				stag_info.first_pm_pbl_index = palloc->level1.idx;
				stag_info.local_fence = ib_wr->send_flags & IB_SEND_FENCE;
				if (iwmr->npages > IRDMA_MIN_PAGES_PER_FMR)
					stag_info.chunk_size = 1;
				err = irdma_sc_mr_fast_register(&iwqp->sc_qp, &stag_info,
								true);
				break;
			}
		default:
			err = -EINVAL;
			irdma_debug(iwdev_to_idev(iwqp->iwdev),
				    IRDMA_DEBUG_VERBS,
				    "upost_send bad opcode = 0x%x\n",
				    ib_wr->opcode);
			break;
		}

		if (err)
			break;
		ib_wr = ib_wr->next;
	}

	if (!iwqp->flush_issued && iwqp->hw_iwarp_state <= IRDMA_QP_STATE_RTS)
		irdma_uk_qp_post_wr(ukqp);
	else if (iwqp->flush_issued)
		mod_delayed_work(iwqp->iwdev->cleanup_wq, &iwqp->dwork_flush, IRDMA_FLUSH_DELAY_MS);
	spin_unlock_irqrestore(&iwqp->lock, flags);
	if (err)
		*bad_wr = ib_wr;

	return err;
}

/**
 * irdma_post_recv - post receive wr for kernel application
 * @ibqp: ib qp pointer
 * @ib_wr: work request for receive
 * @bad_wr: bad wr caused an error
 */
static int
irdma_post_recv(struct ib_qp *ibqp,
		const struct ib_recv_wr *ib_wr,
		const struct ib_recv_wr **bad_wr)
{
	struct irdma_qp *iwqp = to_iwqp(ibqp);
	struct irdma_qp_uk *ukqp = &iwqp->sc_qp.qp_uk;
	struct irdma_post_rq_info post_recv = {0};
	struct irdma_sge *sg_list = iwqp->sg_list;
	unsigned long flags;
	int err = 0;

	spin_lock_irqsave(&iwqp->lock, flags);

	while (ib_wr) {
		if (ib_wr->num_sge > ukqp->max_rq_frag_cnt) {
			err = -EINVAL;
			goto out;
		}
		post_recv.num_sges = ib_wr->num_sge;
		post_recv.wr_id = ib_wr->wr_id;
		irdma_copy_sg_list(sg_list, ib_wr->sg_list, ib_wr->num_sge);
		post_recv.sg_list = sg_list;
		err = irdma_uk_post_receive(ukqp, &post_recv);
		if (err) {
			irdma_debug(iwdev_to_idev(iwqp->iwdev),
				    IRDMA_DEBUG_VERBS, "post_recv err %d\n",
				    err);
			goto out;
		}

		ib_wr = ib_wr->next;
	}

out:
	if (iwqp->flush_issued)
		mod_delayed_work(iwqp->iwdev->cleanup_wq, &iwqp->dwork_flush, IRDMA_FLUSH_DELAY_MS);
	spin_unlock_irqrestore(&iwqp->lock, flags);
	if (err)
		*bad_wr = ib_wr;

	return err;
}

/**
 * irdma_flush_err_to_ib_wc_status - return change flush error code to IB status
 * @opcode: iwarp flush code
 */
static enum ib_wc_status
irdma_flush_err_to_ib_wc_status(enum irdma_flush_opcode opcode)
{
	switch (opcode) {
	case FLUSH_PROT_ERR:
		return IB_WC_LOC_PROT_ERR;
	case FLUSH_REM_ACCESS_ERR:
		return IB_WC_REM_ACCESS_ERR;
	case FLUSH_LOC_QP_OP_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case FLUSH_REM_OP_ERR:
		return IB_WC_REM_OP_ERR;
	case FLUSH_LOC_LEN_ERR:
		return IB_WC_LOC_LEN_ERR;
	case FLUSH_GENERAL_ERR:
		return IB_WC_WR_FLUSH_ERR;
	case FLUSH_MW_BIND_ERR:
		return IB_WC_MW_BIND_ERR;
	case FLUSH_RETRY_EXC_ERR:
		return IB_WC_RETRY_EXC_ERR;
	case FLUSH_FATAL_ERR:
	default:
		return IB_WC_FATAL_ERR;
	}
}

/**
 * irdma_process_cqe - process cqe info
 * @entry: processed cqe
 * @cq_poll_info: cqe info
 */
static void
irdma_process_cqe(struct ib_wc *entry,
		  struct irdma_cq_poll_info *cq_poll_info)
{
	struct irdma_sc_qp *qp;

	entry->wc_flags = 0;
	entry->pkey_index = 0;
	entry->wr_id = cq_poll_info->wr_id;

	qp = cq_poll_info->qp_handle;
	entry->qp = qp->qp_uk.back_qp;

	if (cq_poll_info->error) {
		entry->status = (cq_poll_info->comp_status == IRDMA_COMPL_STATUS_FLUSHED) ?
		    irdma_flush_err_to_ib_wc_status(cq_poll_info->minor_err) : IB_WC_GENERAL_ERR;

		entry->vendor_err = cq_poll_info->major_err << 16 |
		    cq_poll_info->minor_err;
	} else {
		entry->status = IB_WC_SUCCESS;
		if (cq_poll_info->imm_valid) {
			entry->ex.imm_data = htonl(cq_poll_info->imm_data);
			entry->wc_flags |= IB_WC_WITH_IMM;
		}
		if (cq_poll_info->ud_smac_valid) {
			ether_addr_copy(entry->smac, cq_poll_info->ud_smac);
			entry->wc_flags |= IB_WC_WITH_SMAC;
		}

		if (cq_poll_info->ud_vlan_valid) {
			u16 vlan = cq_poll_info->ud_vlan & EVL_VLID_MASK;

			entry->sl = cq_poll_info->ud_vlan >> VLAN_PRIO_SHIFT;
			if (vlan) {
				entry->vlan_id = vlan;
				entry->wc_flags |= IB_WC_WITH_VLAN;
			}
		} else {
			entry->sl = 0;
		}
	}

	switch (cq_poll_info->op_type) {
	case IRDMA_OP_TYPE_RDMA_WRITE:
	case IRDMA_OP_TYPE_RDMA_WRITE_SOL:
		entry->opcode = IB_WC_RDMA_WRITE;
		break;
	case IRDMA_OP_TYPE_RDMA_READ_INV_STAG:
	case IRDMA_OP_TYPE_RDMA_READ:
		entry->opcode = IB_WC_RDMA_READ;
		break;
	case IRDMA_OP_TYPE_SEND_INV:
	case IRDMA_OP_TYPE_SEND_SOL:
	case IRDMA_OP_TYPE_SEND_SOL_INV:
	case IRDMA_OP_TYPE_SEND:
		entry->opcode = IB_WC_SEND;
		break;
	case IRDMA_OP_TYPE_FAST_REG_NSMR:
		entry->opcode = IB_WC_REG_MR;
		break;
	case IRDMA_OP_TYPE_INV_STAG:
		entry->opcode = IB_WC_LOCAL_INV;
		break;
	case IRDMA_OP_TYPE_REC_IMM:
	case IRDMA_OP_TYPE_REC:
		entry->opcode = cq_poll_info->op_type == IRDMA_OP_TYPE_REC_IMM ?
		    IB_WC_RECV_RDMA_WITH_IMM : IB_WC_RECV;
		if (qp->qp_uk.qp_type != IRDMA_QP_TYPE_ROCE_UD &&
		    cq_poll_info->stag_invalid_set) {
			entry->ex.invalidate_rkey = cq_poll_info->inv_stag;
			entry->wc_flags |= IB_WC_WITH_INVALIDATE;
		}
		break;
	default:
		ibdev_err(&iwqp->iwdev->ibdev,
			  "Invalid opcode = %d in CQE\n", cq_poll_info->op_type);
		entry->status = IB_WC_GENERAL_ERR;
		return;
	}

	if (qp->qp_uk.qp_type == IRDMA_QP_TYPE_ROCE_UD) {
		entry->src_qp = cq_poll_info->ud_src_qpn;
		entry->slid = 0;
		entry->wc_flags |=
		    (IB_WC_GRH | IB_WC_WITH_NETWORK_HDR_TYPE);
		entry->network_hdr_type = cq_poll_info->ipv4 ?
		    RDMA_NETWORK_IPV4 :
		    RDMA_NETWORK_IPV6;
	} else {
		entry->src_qp = cq_poll_info->qp_id;
	}

	entry->byte_len = cq_poll_info->bytes_xfered;
}

/**
 * irdma_poll_one - poll one entry of the CQ
 * @ukcq: ukcq to poll
 * @cur_cqe: current CQE info to be filled in
 * @entry: ibv_wc object to be filled for non-extended CQ or NULL for extended CQ
 *
 * Returns the internal irdma device error code or 0 on success
 */
static inline int
irdma_poll_one(struct irdma_cq_uk *ukcq,
	       struct irdma_cq_poll_info *cur_cqe,
	       struct ib_wc *entry)
{
	int ret = irdma_uk_cq_poll_cmpl(ukcq, cur_cqe);

	if (ret)
		return ret;

	irdma_process_cqe(entry, cur_cqe);

	return 0;
}

/**
 * __irdma_poll_cq - poll cq for completion (kernel apps)
 * @iwcq: cq to poll
 * @num_entries: number of entries to poll
 * @entry: wr of a completed entry
 */
static int
__irdma_poll_cq(struct irdma_cq *iwcq, int num_entries, struct ib_wc *entry)
{
	struct list_head *tmp_node, *list_node;
	struct irdma_cq_buf *last_buf = NULL;
	struct irdma_cq_poll_info *cur_cqe = &iwcq->cur_cqe;
	struct irdma_cq_buf *cq_buf;
	int ret;
	struct irdma_device *iwdev;
	struct irdma_cq_uk *ukcq;
	bool cq_new_cqe = false;
	int resized_bufs = 0;
	int npolled = 0;

	iwdev = to_iwdev(iwcq->ibcq.device);
	ukcq = &iwcq->sc_cq.cq_uk;

	/* go through the list of previously resized CQ buffers */
	list_for_each_safe(list_node, tmp_node, &iwcq->resize_list) {
		cq_buf = container_of(list_node, struct irdma_cq_buf, list);
		while (npolled < num_entries) {
			ret = irdma_poll_one(&cq_buf->cq_uk, cur_cqe, entry + npolled);
			if (!ret) {
				++npolled;
				cq_new_cqe = true;
				continue;
			}
			if (ret == -ENOENT)
				break;
			/* QP using the CQ is destroyed. Skip reporting this CQE */
			if (ret == -EFAULT) {
				cq_new_cqe = true;
				continue;
			}
			goto error;
		}

		/* save the resized CQ buffer which received the last cqe */
		if (cq_new_cqe)
			last_buf = cq_buf;
		cq_new_cqe = false;
	}

	/* check the current CQ for new cqes */
	while (npolled < num_entries) {
		ret = irdma_poll_one(ukcq, cur_cqe, entry + npolled);
		if (ret == -ENOENT) {
			ret = irdma_generated_cmpls(iwcq, cur_cqe);
			if (!ret)
				irdma_process_cqe(entry + npolled, cur_cqe);
		}
		if (!ret) {
			++npolled;
			cq_new_cqe = true;
			continue;
		}

		if (ret == -ENOENT)
			break;
		/* QP using the CQ is destroyed. Skip reporting this CQE */
		if (ret == -EFAULT) {
			cq_new_cqe = true;
			continue;
		}
		goto error;
	}

	if (cq_new_cqe)
		/* all previous CQ resizes are complete */
		resized_bufs = irdma_process_resize_list(iwcq, iwdev, NULL);
	else if (last_buf)
		/* only CQ resizes up to the last_buf are complete */
		resized_bufs = irdma_process_resize_list(iwcq, iwdev, last_buf);
	if (resized_bufs)
		/* report to the HW the number of complete CQ resizes */
		irdma_uk_cq_set_resized_cnt(ukcq, resized_bufs);

	return npolled;
error:
	irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
		    "%s: Error polling CQ, irdma_err: %d\n", __func__, ret);

	return ret;
}

/**
 * irdma_poll_cq - poll cq for completion (kernel apps)
 * @ibcq: cq to poll
 * @num_entries: number of entries to poll
 * @entry: wr of a completed entry
 */
static int
irdma_poll_cq(struct ib_cq *ibcq, int num_entries,
	      struct ib_wc *entry)
{
	struct irdma_cq *iwcq;
	unsigned long flags;
	int ret;

	iwcq = to_iwcq(ibcq);

	spin_lock_irqsave(&iwcq->lock, flags);
	ret = __irdma_poll_cq(iwcq, num_entries, entry);
	spin_unlock_irqrestore(&iwcq->lock, flags);

	return ret;
}

/**
 * irdma_req_notify_cq - arm cq kernel application
 * @ibcq: cq to arm
 * @notify_flags: notofication flags
 */
static int
irdma_req_notify_cq(struct ib_cq *ibcq,
		    enum ib_cq_notify_flags notify_flags)
{
	struct irdma_cq *iwcq;
	struct irdma_cq_uk *ukcq;
	unsigned long flags;
	enum irdma_cmpl_notify cq_notify = IRDMA_CQ_COMPL_EVENT;
	bool promo_event = false;
	int ret = 0;

	iwcq = to_iwcq(ibcq);
	ukcq = &iwcq->sc_cq.cq_uk;

	spin_lock_irqsave(&iwcq->lock, flags);
	if (notify_flags == IB_CQ_SOLICITED) {
		cq_notify = IRDMA_CQ_COMPL_SOLICITED;
	} else {
		if (iwcq->last_notify == IRDMA_CQ_COMPL_SOLICITED)
			promo_event = true;
	}

	if (!iwcq->armed || promo_event) {
		iwcq->armed = true;
		iwcq->last_notify = cq_notify;
		irdma_uk_cq_request_notification(ukcq, cq_notify);
	}

	if ((notify_flags & IB_CQ_REPORT_MISSED_EVENTS) && !irdma_cq_empty(iwcq))
		ret = 1;
	spin_unlock_irqrestore(&iwcq->lock, flags);

	return ret;
}

const char *const irdma_hw_stat_names[] = {
	/* gen1 - 32-bit */
	[IRDMA_HW_STAT_INDEX_IP4RXDISCARD] = "ip4InDiscards",
	[IRDMA_HW_STAT_INDEX_IP4RXTRUNC] = "ip4InTruncatedPkts",
	[IRDMA_HW_STAT_INDEX_IP4TXNOROUTE] = "ip4OutNoRoutes",
	[IRDMA_HW_STAT_INDEX_IP6RXDISCARD] = "ip6InDiscards",
	[IRDMA_HW_STAT_INDEX_IP6RXTRUNC] = "ip6InTruncatedPkts",
	[IRDMA_HW_STAT_INDEX_IP6TXNOROUTE] = "ip6OutNoRoutes",
	[IRDMA_HW_STAT_INDEX_TCPRTXSEG] = "tcpRetransSegs",
	[IRDMA_HW_STAT_INDEX_TCPRXOPTERR] = "tcpInOptErrors",
	[IRDMA_HW_STAT_INDEX_TCPRXPROTOERR] = "tcpInProtoErrors",
	[IRDMA_HW_STAT_INDEX_RXVLANERR] = "rxVlanErrors",
	/* gen1 - 64-bit */
	[IRDMA_HW_STAT_INDEX_IP4RXOCTS] = "ip4InOctets",
	[IRDMA_HW_STAT_INDEX_IP4RXPKTS] = "ip4InPkts",
	[IRDMA_HW_STAT_INDEX_IP4RXFRAGS] = "ip4InReasmRqd",
	[IRDMA_HW_STAT_INDEX_IP4RXMCPKTS] = "ip4InMcastPkts",
	[IRDMA_HW_STAT_INDEX_IP4TXOCTS] = "ip4OutOctets",
	[IRDMA_HW_STAT_INDEX_IP4TXPKTS] = "ip4OutPkts",
	[IRDMA_HW_STAT_INDEX_IP4TXFRAGS] = "ip4OutSegRqd",
	[IRDMA_HW_STAT_INDEX_IP4TXMCPKTS] = "ip4OutMcastPkts",
	[IRDMA_HW_STAT_INDEX_IP6RXOCTS] = "ip6InOctets",
	[IRDMA_HW_STAT_INDEX_IP6RXPKTS] = "ip6InPkts",
	[IRDMA_HW_STAT_INDEX_IP6RXFRAGS] = "ip6InReasmRqd",
	[IRDMA_HW_STAT_INDEX_IP6RXMCPKTS] = "ip6InMcastPkts",
	[IRDMA_HW_STAT_INDEX_IP6TXOCTS] = "ip6OutOctets",
	[IRDMA_HW_STAT_INDEX_IP6TXPKTS] = "ip6OutPkts",
	[IRDMA_HW_STAT_INDEX_IP6TXFRAGS] = "ip6OutSegRqd",
	[IRDMA_HW_STAT_INDEX_IP6TXMCPKTS] = "ip6OutMcastPkts",
	[IRDMA_HW_STAT_INDEX_TCPRXSEGS] = "tcpInSegs",
	[IRDMA_HW_STAT_INDEX_TCPTXSEG] = "tcpOutSegs",
	[IRDMA_HW_STAT_INDEX_RDMARXRDS] = "iwInRdmaReads",
	[IRDMA_HW_STAT_INDEX_RDMARXSNDS] = "iwInRdmaSends",
	[IRDMA_HW_STAT_INDEX_RDMARXWRS] = "iwInRdmaWrites",
	[IRDMA_HW_STAT_INDEX_RDMATXRDS] = "iwOutRdmaReads",
	[IRDMA_HW_STAT_INDEX_RDMATXSNDS] = "iwOutRdmaSends",
	[IRDMA_HW_STAT_INDEX_RDMATXWRS] = "iwOutRdmaWrites",
	[IRDMA_HW_STAT_INDEX_RDMAVBND] = "iwRdmaBnd",
	[IRDMA_HW_STAT_INDEX_RDMAVINV] = "iwRdmaInv",

	/* gen2 - 32-bit */
	[IRDMA_HW_STAT_INDEX_RXRPCNPHANDLED] = "cnpHandled",
	[IRDMA_HW_STAT_INDEX_RXRPCNPIGNORED] = "cnpIgnored",
	[IRDMA_HW_STAT_INDEX_TXNPCNPSENT] = "cnpSent",
	/* gen2 - 64-bit */
	[IRDMA_HW_STAT_INDEX_IP4RXMCOCTS] = "ip4InMcastOctets",
	[IRDMA_HW_STAT_INDEX_IP4TXMCOCTS] = "ip4OutMcastOctets",
	[IRDMA_HW_STAT_INDEX_IP6RXMCOCTS] = "ip6InMcastOctets",
	[IRDMA_HW_STAT_INDEX_IP6TXMCOCTS] = "ip6OutMcastOctets",
	[IRDMA_HW_STAT_INDEX_UDPRXPKTS] = "RxUDP",
	[IRDMA_HW_STAT_INDEX_UDPTXPKTS] = "TxUDP",
	[IRDMA_HW_STAT_INDEX_RXNPECNMARKEDPKTS] = "RxECNMrkd",
};

/**
 * mcast_list_add -  Add a new mcast item to list
 * @rf: RDMA PCI function
 * @new_elem: pointer to element to add
 */
static void
mcast_list_add(struct irdma_pci_f *rf,
	       struct mc_table_list *new_elem)
{
	list_add(&new_elem->list, &rf->mc_qht_list.list);
}

/**
 * mcast_list_del - Remove an mcast item from list
 * @mc_qht_elem: pointer to mcast table list element
 */
static void
mcast_list_del(struct mc_table_list *mc_qht_elem)
{
	if (mc_qht_elem)
		list_del(&mc_qht_elem->list);
}

/**
 * mcast_list_lookup_ip - Search mcast list for address
 * @rf: RDMA PCI function
 * @ip_mcast: pointer to mcast IP address
 */
static struct mc_table_list *
mcast_list_lookup_ip(struct irdma_pci_f *rf,
		     u32 *ip_mcast)
{
	struct mc_table_list *mc_qht_el;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &rf->mc_qht_list.list) {
		mc_qht_el = list_entry(pos, struct mc_table_list, list);
		if (!memcmp(mc_qht_el->mc_info.dest_ip, ip_mcast,
			    sizeof(mc_qht_el->mc_info.dest_ip)))
			return mc_qht_el;
	}

	return NULL;
}

/**
 * irdma_mcast_cqp_op - perform a mcast cqp operation
 * @iwdev: irdma device
 * @mc_grp_ctx: mcast group info
 * @op: operation
 *
 * returns error status
 */
static int
irdma_mcast_cqp_op(struct irdma_device *iwdev,
		   struct irdma_mcast_grp_info *mc_grp_ctx, u8 op)
{
	struct cqp_cmds_info *cqp_info;
	struct irdma_cqp_request *cqp_request;
	int status;

	cqp_request = irdma_alloc_and_get_cqp_request(&iwdev->rf->cqp, true);
	if (!cqp_request)
		return -ENOMEM;

	cqp_request->info.in.u.mc_create.info = *mc_grp_ctx;
	cqp_info = &cqp_request->info;
	cqp_info->cqp_cmd = op;
	cqp_info->post_sq = 1;
	cqp_info->in.u.mc_create.scratch = (uintptr_t)cqp_request;
	cqp_info->in.u.mc_create.cqp = &iwdev->rf->cqp.sc_cqp;
	status = irdma_handle_cqp_op(iwdev->rf, cqp_request);
	irdma_put_cqp_request(&iwdev->rf->cqp, cqp_request);

	return status;
}

/**
 * irdma_attach_mcast - attach a qp to a multicast group
 * @ibqp: ptr to qp
 * @ibgid: pointer to global ID
 * @lid: local ID
 *
 * returns error status
 */
static int
irdma_attach_mcast(struct ib_qp *ibqp, union ib_gid *ibgid, u16 lid)
{
	struct irdma_qp *iwqp = to_iwqp(ibqp);
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_pci_f *rf = iwdev->rf;
	struct mc_table_list *mc_qht_elem;
	struct irdma_mcast_grp_ctx_entry_info mcg_info = {0};
	unsigned long flags;
	u32 ip_addr[4] = {0};
	u32 mgn;
	u32 no_mgs;
	int ret = 0;
	bool ipv4;
	u16 vlan_id;
	union {
		struct sockaddr saddr;
		struct sockaddr_in saddr_in;
		struct sockaddr_in6 saddr_in6;
	} sgid_addr;
	unsigned char dmac[ETH_ALEN];

	rdma_gid2ip((struct sockaddr *)&sgid_addr, ibgid);

	if (!ipv6_addr_v4mapped((struct in6_addr *)ibgid)) {
		irdma_copy_ip_ntohl(ip_addr,
				    sgid_addr.saddr_in6.sin6_addr.__u6_addr.__u6_addr32);
		irdma_netdev_vlan_ipv6(ip_addr, &vlan_id, NULL);
		ipv4 = false;
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "qp_id=%d, IP6address=%pI6\n", ibqp->qp_num,
			    ip_addr);
		irdma_mcast_mac_v6(ip_addr, dmac);
	} else {
		ip_addr[0] = ntohl(sgid_addr.saddr_in.sin_addr.s_addr);
		ipv4 = true;
		vlan_id = irdma_get_vlan_ipv4(ip_addr);
		irdma_mcast_mac_v4(ip_addr, dmac);
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "qp_id=%d, IP4address=%pI4, MAC=%pM\n",
			    ibqp->qp_num, ip_addr, dmac);
	}

	spin_lock_irqsave(&rf->qh_list_lock, flags);
	mc_qht_elem = mcast_list_lookup_ip(rf, ip_addr);
	if (!mc_qht_elem) {
		struct irdma_dma_mem *dma_mem_mc;

		spin_unlock_irqrestore(&rf->qh_list_lock, flags);
		mc_qht_elem = kzalloc(sizeof(*mc_qht_elem), GFP_KERNEL);
		if (!mc_qht_elem)
			return -ENOMEM;

		mc_qht_elem->mc_info.ipv4_valid = ipv4;
		memcpy(mc_qht_elem->mc_info.dest_ip, ip_addr,
		       sizeof(mc_qht_elem->mc_info.dest_ip));
		ret = irdma_alloc_rsrc(rf, rf->allocated_mcgs, rf->max_mcg,
				       &mgn, &rf->next_mcg);
		if (ret) {
			kfree(mc_qht_elem);
			return -ENOMEM;
		}

		mc_qht_elem->mc_info.mgn = mgn;
		dma_mem_mc = &mc_qht_elem->mc_grp_ctx.dma_mem_mc;
		dma_mem_mc->size = sizeof(u64)* IRDMA_MAX_MGS_PER_CTX;
		dma_mem_mc->va = irdma_allocate_dma_mem(&rf->hw, dma_mem_mc,
							dma_mem_mc->size,
							IRDMA_HW_PAGE_SIZE);
		if (!dma_mem_mc->va) {
			irdma_free_rsrc(rf, rf->allocated_mcgs, mgn);
			kfree(mc_qht_elem);
			return -ENOMEM;
		}

		mc_qht_elem->mc_grp_ctx.mg_id = (u16)mgn;
		memcpy(mc_qht_elem->mc_grp_ctx.dest_ip_addr, ip_addr,
		       sizeof(mc_qht_elem->mc_grp_ctx.dest_ip_addr));
		mc_qht_elem->mc_grp_ctx.ipv4_valid = ipv4;
		mc_qht_elem->mc_grp_ctx.vlan_id = vlan_id;
		if (vlan_id < VLAN_N_VID)
			mc_qht_elem->mc_grp_ctx.vlan_valid = true;
		mc_qht_elem->mc_grp_ctx.hmc_fcn_id = iwdev->vsi.fcn_id;
		mc_qht_elem->mc_grp_ctx.qs_handle =
		    iwqp->sc_qp.vsi->qos[iwqp->sc_qp.user_pri].qs_handle;
		ether_addr_copy(mc_qht_elem->mc_grp_ctx.dest_mac_addr, dmac);

		spin_lock_irqsave(&rf->qh_list_lock, flags);
		mcast_list_add(rf, mc_qht_elem);
	} else {
		if (mc_qht_elem->mc_grp_ctx.no_of_mgs ==
		    IRDMA_MAX_MGS_PER_CTX) {
			spin_unlock_irqrestore(&rf->qh_list_lock, flags);
			return -ENOMEM;
		}
	}

	mcg_info.qp_id = iwqp->ibqp.qp_num;
	no_mgs = mc_qht_elem->mc_grp_ctx.no_of_mgs;
	irdma_sc_add_mcast_grp(&mc_qht_elem->mc_grp_ctx, &mcg_info);
	spin_unlock_irqrestore(&rf->qh_list_lock, flags);

	/* Only if there is a change do we need to modify or create */
	if (!no_mgs) {
		ret = irdma_mcast_cqp_op(iwdev, &mc_qht_elem->mc_grp_ctx,
					 IRDMA_OP_MC_CREATE);
	} else if (no_mgs != mc_qht_elem->mc_grp_ctx.no_of_mgs) {
		ret = irdma_mcast_cqp_op(iwdev, &mc_qht_elem->mc_grp_ctx,
					 IRDMA_OP_MC_MODIFY);
	} else {
		return 0;
	}

	if (ret)
		goto error;

	return 0;

error:
	irdma_sc_del_mcast_grp(&mc_qht_elem->mc_grp_ctx, &mcg_info);
	if (!mc_qht_elem->mc_grp_ctx.no_of_mgs) {
		mcast_list_del(mc_qht_elem);
		irdma_free_dma_mem(&rf->hw,
				   &mc_qht_elem->mc_grp_ctx.dma_mem_mc);
		irdma_free_rsrc(rf, rf->allocated_mcgs,
				mc_qht_elem->mc_grp_ctx.mg_id);
		kfree(mc_qht_elem);
	}

	return ret;
}

/**
 * irdma_detach_mcast - detach a qp from a multicast group
 * @ibqp: ptr to qp
 * @ibgid: pointer to global ID
 * @lid: local ID
 *
 * returns error status
 */
static int
irdma_detach_mcast(struct ib_qp *ibqp, union ib_gid *ibgid, u16 lid)
{
	struct irdma_qp *iwqp = to_iwqp(ibqp);
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_pci_f *rf = iwdev->rf;
	u32 ip_addr[4] = {0};
	struct mc_table_list *mc_qht_elem;
	struct irdma_mcast_grp_ctx_entry_info mcg_info = {0};
	int ret;
	unsigned long flags;
	union {
		struct sockaddr saddr;
		struct sockaddr_in saddr_in;
		struct sockaddr_in6 saddr_in6;
	} sgid_addr;

	rdma_gid2ip((struct sockaddr *)&sgid_addr, ibgid);
	if (!ipv6_addr_v4mapped((struct in6_addr *)ibgid))
		irdma_copy_ip_ntohl(ip_addr,
				    sgid_addr.saddr_in6.sin6_addr.__u6_addr.__u6_addr32);
	else
		ip_addr[0] = ntohl(sgid_addr.saddr_in.sin_addr.s_addr);

	spin_lock_irqsave(&rf->qh_list_lock, flags);
	mc_qht_elem = mcast_list_lookup_ip(rf, ip_addr);
	if (!mc_qht_elem) {
		spin_unlock_irqrestore(&rf->qh_list_lock, flags);
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "address not found MCG\n");
		return 0;
	}

	mcg_info.qp_id = iwqp->ibqp.qp_num;
	irdma_sc_del_mcast_grp(&mc_qht_elem->mc_grp_ctx, &mcg_info);
	if (!mc_qht_elem->mc_grp_ctx.no_of_mgs) {
		mcast_list_del(mc_qht_elem);
		spin_unlock_irqrestore(&rf->qh_list_lock, flags);
		ret = irdma_mcast_cqp_op(iwdev, &mc_qht_elem->mc_grp_ctx,
					 IRDMA_OP_MC_DESTROY);
		if (ret) {
			irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
				    "failed MC_DESTROY MCG\n");
			spin_lock_irqsave(&rf->qh_list_lock, flags);
			mcast_list_add(rf, mc_qht_elem);
			spin_unlock_irqrestore(&rf->qh_list_lock, flags);
			return -EAGAIN;
		}

		irdma_free_dma_mem(&rf->hw,
				   &mc_qht_elem->mc_grp_ctx.dma_mem_mc);
		irdma_free_rsrc(rf, rf->allocated_mcgs,
				mc_qht_elem->mc_grp_ctx.mg_id);
		kfree(mc_qht_elem);
	} else {
		spin_unlock_irqrestore(&rf->qh_list_lock, flags);
		ret = irdma_mcast_cqp_op(iwdev, &mc_qht_elem->mc_grp_ctx,
					 IRDMA_OP_MC_MODIFY);
		if (ret) {
			irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
				    "failed Modify MCG\n");
			return ret;
		}
	}

	return 0;
}

/**
 * irdma_query_ah - Query address handle
 * @ibah: pointer to address handle
 * @ah_attr: address handle attributes
 */
static int
irdma_query_ah(struct ib_ah *ibah, struct ib_ah_attr *ah_attr)
{
	struct irdma_ah *ah = to_iwah(ibah);

	memset(ah_attr, 0, sizeof(*ah_attr));
	if (ah->av.attrs.ah_flags & IB_AH_GRH) {
		ah_attr->ah_flags = IB_AH_GRH;
		ah_attr->grh.flow_label = ah->sc_ah.ah_info.flow_label;
		ah_attr->grh.traffic_class = ah->sc_ah.ah_info.tc_tos;
		ah_attr->grh.hop_limit = ah->sc_ah.ah_info.hop_ttl;
		ah_attr->grh.sgid_index = ah->sgid_index;
		ah_attr->grh.sgid_index = ah->sgid_index;
		memcpy(&ah_attr->grh.dgid, &ah->dgid,
		       sizeof(ah_attr->grh.dgid));
	}

	return 0;
}

static __be64 irdma_mac_to_guid(struct ifnet *ndev){
	unsigned char *mac = IF_LLADDR(ndev);
	__be64 guid;
	unsigned char *dst = (unsigned char *)&guid;

	dst[0] = mac[0] ^ 2;
	dst[1] = mac[1];
	dst[2] = mac[2];
	dst[3] = 0xff;
	dst[4] = 0xfe;
	dst[5] = mac[3];
	dst[6] = mac[4];
	dst[7] = mac[5];

	return guid;
}

static struct ifnet *
irdma_get_netdev(struct ib_device *ibdev, u8 port_num)
{
	struct irdma_device *iwdev = to_iwdev(ibdev);

	if (iwdev->netdev) {
		dev_hold(iwdev->netdev);
		return iwdev->netdev;
	}

	return NULL;
}

static void
irdma_set_device_ops(struct ib_device *ibdev)
{
	struct ib_device *dev_ops = ibdev;

	dev_ops->ops.driver_id = RDMA_DRIVER_I40IW;
	dev_ops->ops.size_ib_ah = IRDMA_SET_RDMA_OBJ_SIZE(ib_ah, irdma_ah, ibah);
	dev_ops->ops.size_ib_cq = IRDMA_SET_RDMA_OBJ_SIZE(ib_cq, irdma_cq, ibcq);
	dev_ops->ops.size_ib_pd = IRDMA_SET_RDMA_OBJ_SIZE(ib_pd, irdma_pd, ibpd);
	dev_ops->ops.size_ib_ucontext = IRDMA_SET_RDMA_OBJ_SIZE(ib_ucontext,
								irdma_ucontext,
								ibucontext);

	dev_ops->alloc_hw_stats = irdma_alloc_hw_stats;
	dev_ops->alloc_mr = irdma_alloc_mr;
	dev_ops->alloc_mw = irdma_alloc_mw;
	dev_ops->alloc_pd = irdma_alloc_pd;
	dev_ops->alloc_ucontext = irdma_alloc_ucontext;
	dev_ops->create_cq = irdma_create_cq;
	dev_ops->create_qp = irdma_create_qp;
	dev_ops->dealloc_mw = irdma_dealloc_mw;
	dev_ops->dealloc_pd = irdma_dealloc_pd;
	dev_ops->dealloc_ucontext = irdma_dealloc_ucontext;
	dev_ops->dereg_mr = irdma_dereg_mr;
	dev_ops->destroy_cq = irdma_destroy_cq;
	dev_ops->destroy_qp = irdma_destroy_qp;
	dev_ops->disassociate_ucontext = irdma_disassociate_ucontext;
	dev_ops->get_dev_fw_str = irdma_get_dev_fw_str;
	dev_ops->get_dma_mr = irdma_get_dma_mr;
	dev_ops->get_hw_stats = irdma_get_hw_stats;
	dev_ops->get_netdev = irdma_get_netdev;
	dev_ops->map_mr_sg = irdma_map_mr_sg;
	dev_ops->mmap = irdma_mmap;
	dev_ops->mmap_free = irdma_mmap_free;
	dev_ops->poll_cq = irdma_poll_cq;
	dev_ops->post_recv = irdma_post_recv;
	dev_ops->post_send = irdma_post_send;
	dev_ops->query_device = irdma_query_device;
	dev_ops->query_port = irdma_query_port;
	dev_ops->modify_port = irdma_modify_port;
	dev_ops->query_qp = irdma_query_qp;
	dev_ops->reg_user_mr = irdma_reg_user_mr;
	dev_ops->req_notify_cq = irdma_req_notify_cq;
	dev_ops->resize_cq = irdma_resize_cq;
}

static void
irdma_set_device_roce_ops(struct ib_device *ibdev)
{
	struct ib_device *dev_ops = ibdev;

	dev_ops->attach_mcast = irdma_attach_mcast;
	dev_ops->create_ah = irdma_create_ah;
	dev_ops->destroy_ah = irdma_destroy_ah;
	dev_ops->detach_mcast = irdma_detach_mcast;
	dev_ops->get_link_layer = irdma_get_link_layer;
	dev_ops->get_port_immutable = irdma_roce_port_immutable;
	dev_ops->modify_qp = irdma_modify_qp_roce;
	dev_ops->query_ah = irdma_query_ah;
	dev_ops->query_gid = irdma_query_gid_roce;
	dev_ops->query_pkey = irdma_query_pkey;
	ibdev->add_gid = irdma_add_gid;
	ibdev->del_gid = irdma_del_gid;
}

static void
irdma_set_device_iw_ops(struct ib_device *ibdev)
{
	struct ib_device *dev_ops = ibdev;

	ibdev->uverbs_cmd_mask |=
	    (1ull << IB_USER_VERBS_CMD_CREATE_AH) |
	    (1ull << IB_USER_VERBS_CMD_DESTROY_AH);

	dev_ops->create_ah = irdma_create_ah_stub;
	dev_ops->destroy_ah = irdma_destroy_ah_stub;
	dev_ops->get_port_immutable = irdma_iw_port_immutable;
	dev_ops->modify_qp = irdma_modify_qp;
	dev_ops->query_gid = irdma_query_gid;
	dev_ops->query_pkey = irdma_iw_query_pkey;
}

/**
 * irdma_init_roce_device - initialization of roce rdma device
 * @iwdev: irdma device
 */
static void
irdma_init_roce_device(struct irdma_device *iwdev)
{
	kc_set_roce_uverbs_cmd_mask(iwdev);
	iwdev->ibdev.node_type = RDMA_NODE_IB_CA;
	iwdev->ibdev.node_guid = irdma_mac_to_guid(iwdev->netdev);
	irdma_set_device_roce_ops(&iwdev->ibdev);
}

/**
 * irdma_init_iw_device - initialization of iwarp rdma device
 * @iwdev: irdma device
 */
static int
irdma_init_iw_device(struct irdma_device *iwdev)
{
	struct ifnet *netdev = iwdev->netdev;

	iwdev->ibdev.node_type = RDMA_NODE_RNIC;
	ether_addr_copy((u8 *)&iwdev->ibdev.node_guid, IF_LLADDR(netdev));
	iwdev->ibdev.iwcm = kzalloc(sizeof(*iwdev->ibdev.iwcm), GFP_KERNEL);
	if (!iwdev->ibdev.iwcm)
		return -ENOMEM;

	iwdev->ibdev.iwcm->add_ref = irdma_qp_add_ref;
	iwdev->ibdev.iwcm->rem_ref = irdma_qp_rem_ref;
	iwdev->ibdev.iwcm->get_qp = irdma_get_qp;
	iwdev->ibdev.iwcm->connect = irdma_connect;
	iwdev->ibdev.iwcm->accept = irdma_accept;
	iwdev->ibdev.iwcm->reject = irdma_reject;
	iwdev->ibdev.iwcm->create_listen = irdma_create_listen;
	iwdev->ibdev.iwcm->destroy_listen = irdma_destroy_listen;
	memcpy(iwdev->ibdev.iwcm->ifname, if_name(netdev),
	       sizeof(iwdev->ibdev.iwcm->ifname));
	irdma_set_device_iw_ops(&iwdev->ibdev);

	return 0;
}

/**
 * irdma_init_rdma_device - initialization of rdma device
 * @iwdev: irdma device
 */
static int
irdma_init_rdma_device(struct irdma_device *iwdev)
{
	struct pci_dev *pcidev = iwdev->rf->pcidev;
	int ret;

	iwdev->ibdev.owner = THIS_MODULE;
	iwdev->ibdev.uverbs_abi_ver = IRDMA_ABI_VER;
	kc_set_rdma_uverbs_cmd_mask(iwdev);

	if (iwdev->roce_mode) {
		irdma_init_roce_device(iwdev);
	} else {
		ret = irdma_init_iw_device(iwdev);
		if (ret)
			return ret;
	}
	iwdev->ibdev.phys_port_cnt = 1;
	iwdev->ibdev.num_comp_vectors = iwdev->rf->ceqs_count;
	iwdev->ibdev.dev.parent = iwdev->rf->dev_ctx.dev;
	set_ibdev_dma_device(iwdev->ibdev, &pcidev->dev);
	irdma_set_device_ops(&iwdev->ibdev);

	return 0;
}

/**
 * irdma_port_ibevent - indicate port event
 * @iwdev: irdma device
 */
void
irdma_port_ibevent(struct irdma_device *iwdev)
{
	struct ib_event event;

	event.device = &iwdev->ibdev;
	event.element.port_num = 1;
	event.event =
	    iwdev->iw_status ? IB_EVENT_PORT_ACTIVE : IB_EVENT_PORT_ERR;
	ib_dispatch_event(&event);
}

/**
 * irdma_ib_unregister_device - unregister rdma device from IB
 * core
 * @iwdev: irdma device
 */
void
irdma_ib_unregister_device(struct irdma_device *iwdev)
{
	iwdev->iw_status = 0;
	irdma_port_ibevent(iwdev);
	ib_unregister_device(&iwdev->ibdev);
	kfree(iwdev->ibdev.iwcm);
	iwdev->ibdev.iwcm = NULL;
}

/**
 * irdma_ib_register_device - register irdma device to IB core
 * @iwdev: irdma device
 */
int
irdma_ib_register_device(struct irdma_device *iwdev)
{
	int ret;

	ret = irdma_init_rdma_device(iwdev);
	if (ret)
		return ret;

	sprintf(iwdev->ibdev.name, "irdma-%s", if_name(iwdev->netdev));
	ret = ib_register_device(&iwdev->ibdev, NULL);
	if (ret)
		goto error;

	iwdev->iw_status = 1;
	irdma_port_ibevent(iwdev);

	return 0;

error:
	kfree(iwdev->ibdev.iwcm);
	iwdev->ibdev.iwcm = NULL;
	if (ret)
		irdma_debug(&iwdev->rf->sc_dev, IRDMA_DEBUG_VERBS,
			    "Register RDMA device fail\n");

	return ret;
}
