/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2018 - 2022 Intel Corporation
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

void
irdma_get_dev_fw_str(struct ib_device *dev,
		     char *str,
		     size_t str_len)
{
	struct irdma_device *iwdev = to_iwdev(dev);

	snprintf(str, str_len, "%u.%u",
		 irdma_fw_major_ver(&iwdev->rf->sc_dev),
		 irdma_fw_minor_ver(&iwdev->rf->sc_dev));
}

int
irdma_add_gid(struct ib_device *device,
	      u8 port_num,
	      unsigned int index,
	      const union ib_gid *gid,
	      const struct ib_gid_attr *attr,
	      void **context)
{
	return 0;
}

int
irdma_del_gid(struct ib_device *device,
	      u8 port_num,
	      unsigned int index,
	      void **context)
{
	return 0;
}

/**
 * irdma_alloc_mr - register stag for fast memory registration
 * @pd: ibpd pointer
 * @mr_type: memory for stag registrion
 * @max_num_sg: man number of pages
 * @udata: user data
 */
struct ib_mr *
irdma_alloc_mr(struct ib_pd *pd, enum ib_mr_type mr_type,
	       u32 max_num_sg, struct ib_udata *udata)
{
	struct irdma_device *iwdev = to_iwdev(pd->device);
	struct irdma_pble_alloc *palloc;
	struct irdma_pbl *iwpbl;
	struct irdma_mr *iwmr;
	int status;
	u32 stag;
	int err_code = -ENOMEM;

	iwmr = kzalloc(sizeof(*iwmr), GFP_KERNEL);
	if (!iwmr)
		return ERR_PTR(-ENOMEM);

	stag = irdma_create_stag(iwdev);
	if (!stag) {
		err_code = -ENOMEM;
		goto err;
	}

	iwmr->stag = stag;
	iwmr->ibmr.rkey = stag;
	iwmr->ibmr.lkey = stag;
	iwmr->ibmr.pd = pd;
	iwmr->ibmr.device = pd->device;
	iwpbl = &iwmr->iwpbl;
	iwpbl->iwmr = iwmr;
	iwmr->type = IRDMA_MEMREG_TYPE_MEM;
	palloc = &iwpbl->pble_alloc;
	iwmr->page_cnt = max_num_sg;
	status = irdma_get_pble(iwdev->rf->pble_rsrc, palloc, iwmr->page_cnt,
				true);
	if (status)
		goto err_get_pble;

	err_code = irdma_hw_alloc_stag(iwdev, iwmr);
	if (err_code)
		goto err_alloc_stag;

	iwpbl->pbl_allocated = true;

	return &iwmr->ibmr;
err_alloc_stag:
	irdma_free_pble(iwdev->rf->pble_rsrc, palloc);
err_get_pble:
	irdma_free_stag(iwdev, stag);
err:
	kfree(iwmr);

	return ERR_PTR(err_code);
}

/**
 * irdma_alloc_ucontext - Allocate the user context data structure
 * @uctx: context
 * @udata: user data
 *
 * This keeps track of all objects associated with a particular
 * user-mode client.
 */
int
irdma_alloc_ucontext(struct ib_ucontext *uctx, struct ib_udata *udata)
{
	struct ib_device *ibdev = uctx->device;
	struct irdma_device *iwdev = to_iwdev(ibdev);
	struct irdma_alloc_ucontext_req req;
	struct irdma_alloc_ucontext_resp uresp = {0};
	struct irdma_ucontext *ucontext = to_ucontext(uctx);
	struct irdma_uk_attrs *uk_attrs;

	if (ib_copy_from_udata(&req, udata, min(sizeof(req), udata->inlen)))
		return -EINVAL;

	if (req.userspace_ver < 4 || req.userspace_ver > IRDMA_ABI_VER)
		goto ver_error;

	ucontext->iwdev = iwdev;
	ucontext->abi_ver = req.userspace_ver;

	uk_attrs = &iwdev->rf->sc_dev.hw_attrs.uk_attrs;
	/* GEN_1 support for libi40iw */
	if (udata->outlen < sizeof(uresp)) {
		if (uk_attrs->hw_rev != IRDMA_GEN_1)
			return -EOPNOTSUPP;

		ucontext->legacy_mode = true;
		uresp.max_qps = iwdev->rf->max_qp;
		uresp.max_pds = iwdev->rf->sc_dev.hw_attrs.max_hw_pds;
		uresp.wq_size = iwdev->rf->sc_dev.hw_attrs.max_qp_wr * 2;
		uresp.kernel_ver = req.userspace_ver;
		if (ib_copy_to_udata(udata, &uresp, min(sizeof(uresp), udata->outlen)))
			return -EFAULT;
	} else {
		u64 bar_off =
		(uintptr_t)iwdev->rf->sc_dev.hw_regs[IRDMA_DB_ADDR_OFFSET];
		ucontext->db_mmap_entry =
		    irdma_user_mmap_entry_insert(ucontext, bar_off,
						 IRDMA_MMAP_IO_NC,
						 &uresp.db_mmap_key);
		if (!ucontext->db_mmap_entry) {
			return -ENOMEM;
		}
		uresp.kernel_ver = IRDMA_ABI_VER;
		uresp.feature_flags = uk_attrs->feature_flags;
		uresp.max_hw_wq_frags = uk_attrs->max_hw_wq_frags;
		uresp.max_hw_read_sges = uk_attrs->max_hw_read_sges;
		uresp.max_hw_inline = uk_attrs->max_hw_inline;
		uresp.max_hw_rq_quanta = uk_attrs->max_hw_rq_quanta;
		uresp.max_hw_wq_quanta = uk_attrs->max_hw_wq_quanta;
		uresp.max_hw_sq_chunk = uk_attrs->max_hw_sq_chunk;
		uresp.max_hw_cq_size = uk_attrs->max_hw_cq_size;
		uresp.min_hw_cq_size = uk_attrs->min_hw_cq_size;
		uresp.hw_rev = uk_attrs->hw_rev;
		if (ib_copy_to_udata(udata, &uresp,
				     min(sizeof(uresp), udata->outlen))) {
			rdma_user_mmap_entry_remove(ucontext->db_mmap_entry);
			return -EFAULT;
		}
	}

	INIT_LIST_HEAD(&ucontext->cq_reg_mem_list);
	spin_lock_init(&ucontext->cq_reg_mem_list_lock);
	INIT_LIST_HEAD(&ucontext->qp_reg_mem_list);
	spin_lock_init(&ucontext->qp_reg_mem_list_lock);
	INIT_LIST_HEAD(&ucontext->vma_list);
	mutex_init(&ucontext->vma_list_mutex);

	return 0;

ver_error:
	irdma_dev_err(&iwdev->rf->sc_dev,
		      "Invalid userspace driver version detected. Detected version %d, should be %d\n",
		      req.userspace_ver, IRDMA_ABI_VER);
	return -EINVAL;
}

/**
 * irdma_dealloc_ucontext - deallocate the user context data structure
 * @context: user context created during alloc
 */
void
irdma_dealloc_ucontext(struct ib_ucontext *context)
{
	struct irdma_ucontext *ucontext = to_ucontext(context);

	rdma_user_mmap_entry_remove(ucontext->db_mmap_entry);

	return;
}

/**
 * irdma_alloc_pd - allocate protection domain
 * @pd: protection domain
 * @udata: user data
 */
int
irdma_alloc_pd(struct ib_pd *pd, struct ib_udata *udata)
{
	struct irdma_pd *iwpd = to_iwpd(pd);
	struct irdma_device *iwdev = to_iwdev(pd->device);
	struct irdma_sc_dev *dev = &iwdev->rf->sc_dev;
	struct irdma_pci_f *rf = iwdev->rf;
	struct irdma_alloc_pd_resp uresp = {0};
	struct irdma_sc_pd *sc_pd;
	u32 pd_id = 0;
	int err;

	err = irdma_alloc_rsrc(rf, rf->allocated_pds, rf->max_pd, &pd_id,
			       &rf->next_pd);
	if (err)
		return err;

	sc_pd = &iwpd->sc_pd;
	if (udata) {
		struct irdma_ucontext *ucontext =
		rdma_udata_to_drv_context(udata, struct irdma_ucontext,
					  ibucontext);

		irdma_sc_pd_init(dev, sc_pd, pd_id, ucontext->abi_ver);
		uresp.pd_id = pd_id;
		if (ib_copy_to_udata(udata, &uresp,
				     min(sizeof(uresp), udata->outlen))) {
			err = -EFAULT;
			goto error;
		}
	} else {
		irdma_sc_pd_init(dev, sc_pd, pd_id, IRDMA_ABI_VER);
	}

	return 0;

error:

	irdma_free_rsrc(rf, rf->allocated_pds, pd_id);

	return err;
}

void
irdma_dealloc_pd(struct ib_pd *ibpd, struct ib_udata *udata)
{
	struct irdma_pd *iwpd = to_iwpd(ibpd);
	struct irdma_device *iwdev = to_iwdev(ibpd->device);

	irdma_free_rsrc(iwdev->rf, iwdev->rf->allocated_pds, iwpd->sc_pd.pd_id);
}

static void
irdma_fill_ah_info(struct irdma_ah_info *ah_info,
		   const struct ib_gid_attr *sgid_attr,
		   struct sockaddr *sgid_addr, struct sockaddr *dgid_addr,
		   u8 *dmac, u8 net_type)
{
	if (net_type == RDMA_NETWORK_IPV4) {
		ah_info->ipv4_valid = true;
		ah_info->dest_ip_addr[0] =
		    ntohl(((struct sockaddr_in *)dgid_addr)->sin_addr.s_addr);
		ah_info->src_ip_addr[0] =
		    ntohl(((struct sockaddr_in *)sgid_addr)->sin_addr.s_addr);
		ah_info->do_lpbk = irdma_ipv4_is_lpb(ah_info->src_ip_addr[0],
						     ah_info->dest_ip_addr[0]);
		if (ipv4_is_multicast(((struct sockaddr_in *)dgid_addr)->sin_addr.s_addr)) {
			irdma_mcast_mac_v4(ah_info->dest_ip_addr, dmac);
		}
	} else {
		irdma_copy_ip_ntohl(ah_info->dest_ip_addr,
				    ((struct sockaddr_in6 *)dgid_addr)->sin6_addr.__u6_addr.__u6_addr32);
		irdma_copy_ip_ntohl(ah_info->src_ip_addr,
				    ((struct sockaddr_in6 *)sgid_addr)->sin6_addr.__u6_addr.__u6_addr32);
		ah_info->do_lpbk = irdma_ipv6_is_lpb(ah_info->src_ip_addr,
						     ah_info->dest_ip_addr);
		if (rdma_is_multicast_addr(&((struct sockaddr_in6 *)dgid_addr)->sin6_addr)) {
			irdma_mcast_mac_v6(ah_info->dest_ip_addr, dmac);
		}
	}
}

static int
irdma_create_ah_vlan_tag(struct irdma_device *iwdev,
			 struct irdma_ah_info *ah_info,
			 const struct ib_gid_attr *sgid_attr,
			 u8 *dmac)
{
	if (sgid_attr->ndev && is_vlan_dev(sgid_attr->ndev))
		ah_info->vlan_tag = vlan_dev_vlan_id(sgid_attr->ndev);
	else
		ah_info->vlan_tag = VLAN_N_VID;

	ah_info->dst_arpindex = irdma_add_arp(iwdev->rf, ah_info->dest_ip_addr, dmac);

	if (ah_info->dst_arpindex == -1)
		return -EINVAL;

	if (ah_info->vlan_tag >= VLAN_N_VID && iwdev->dcb_vlan_mode)
		ah_info->vlan_tag = 0;

	if (ah_info->vlan_tag < VLAN_N_VID) {
		ah_info->insert_vlan_tag = true;
		ah_info->vlan_tag |=
		    rt_tos2priority(ah_info->tc_tos) << VLAN_PRIO_SHIFT;
	}
	return 0;
}

static int
irdma_create_ah_wait(struct irdma_pci_f *rf,
		     struct irdma_sc_ah *sc_ah, bool sleep)
{
	if (!sleep) {
		int cnt = CQP_COMPL_WAIT_TIME_MS * CQP_TIMEOUT_THRESHOLD;

		do {
			irdma_cqp_ce_handler(rf, &rf->ccq.sc_cq);
			mdelay(1);
		} while (!sc_ah->ah_info.ah_valid && --cnt);

		if (!cnt)
			return -ETIMEDOUT;
	}
	return 0;
}

/**
 * irdma_create_ah - create address handle
 * @ib_ah: ptr to AH
 * @attr: address handle attributes
 * @flags: AH flags to wait
 * @udata: user data
 *
 * returns 0 on success, error otherwise
 */
int
irdma_create_ah(struct ib_ah *ib_ah,
		struct ib_ah_attr *attr, u32 flags,
		struct ib_udata *udata)
{
	struct irdma_pd *pd = to_iwpd(ib_ah->pd);
	struct irdma_ah *ah = container_of(ib_ah, struct irdma_ah, ibah);
	struct irdma_device *iwdev = to_iwdev(ib_ah->pd->device);
	union ib_gid sgid;
	struct ib_gid_attr sgid_attr;
	struct irdma_pci_f *rf = iwdev->rf;
	struct irdma_sc_ah *sc_ah;
	u32 ah_id = 0;
	struct irdma_ah_info *ah_info;
	struct irdma_create_ah_resp uresp;
	union {
		struct sockaddr saddr;
		struct sockaddr_in saddr_in;
		struct sockaddr_in6 saddr_in6;
	} sgid_addr, dgid_addr;
	int err;
	u8 dmac[ETH_ALEN];
	bool sleep;

	err = irdma_alloc_rsrc(rf, rf->allocated_ahs,
			       rf->max_ah, &ah_id, &rf->next_ah);

	if (err)
		return err;

	ah->pd = pd;
	sc_ah = &ah->sc_ah;
	sc_ah->ah_info.ah_idx = ah_id;
	sc_ah->ah_info.vsi = &iwdev->vsi;
	irdma_sc_init_ah(&rf->sc_dev, sc_ah);
	ah->sgid_index = attr->grh.sgid_index;
	memcpy(&ah->dgid, &attr->grh.dgid, sizeof(ah->dgid));
	rcu_read_lock();
	err = ib_get_cached_gid(&iwdev->ibdev, attr->port_num,
				attr->grh.sgid_index, &sgid, &sgid_attr);
	rcu_read_unlock();
	if (err) {
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "GID lookup at idx=%d with port=%d failed\n",
			    attr->grh.sgid_index, attr->port_num);
		err = -EINVAL;
		goto error;
	}
	rdma_gid2ip((struct sockaddr *)&sgid_addr, &sgid);
	rdma_gid2ip((struct sockaddr *)&dgid_addr, &attr->grh.dgid);
	ah->av.attrs = *attr;
	ah->av.net_type = kc_rdma_gid_attr_network_type(sgid_attr,
							sgid_attr.gid_type,
							&sgid);

	if (sgid_attr.ndev)
		dev_put(sgid_attr.ndev);

	ah->av.sgid_addr.saddr = sgid_addr.saddr;
	ah->av.dgid_addr.saddr = dgid_addr.saddr;
	ah_info = &sc_ah->ah_info;
	ah_info->ah_idx = ah_id;
	ah_info->pd_idx = pd->sc_pd.pd_id;
	ether_addr_copy(ah_info->mac_addr, IF_LLADDR(iwdev->netdev));

	if (attr->ah_flags & IB_AH_GRH) {
		ah_info->flow_label = attr->grh.flow_label;
		ah_info->hop_ttl = attr->grh.hop_limit;
		ah_info->tc_tos = attr->grh.traffic_class;
	}

	ether_addr_copy(dmac, attr->dmac);

	irdma_fill_ah_info(ah_info, &sgid_attr, &sgid_addr.saddr, &dgid_addr.saddr,
			   dmac, ah->av.net_type);

	err = irdma_create_ah_vlan_tag(iwdev, ah_info, &sgid_attr, dmac);
	if (err)
		goto error;

	sleep = flags & RDMA_CREATE_AH_SLEEPABLE;

	err = irdma_ah_cqp_op(iwdev->rf, sc_ah, IRDMA_OP_AH_CREATE,
			      sleep, irdma_gsi_ud_qp_ah_cb, sc_ah);
	if (err) {
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "CQP-OP Create AH fail");
		goto error;
	}

	err = irdma_create_ah_wait(rf, sc_ah, sleep);
	if (err) {
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "CQP create AH timed out");
		goto error;
	}

	if (udata) {
		uresp.ah_id = ah->sc_ah.ah_info.ah_idx;
		err = ib_copy_to_udata(udata, &uresp, sizeof(uresp));
	}
	return 0;
error:
	irdma_free_rsrc(iwdev->rf, iwdev->rf->allocated_ahs, ah_id);
	return err;
}

void
irdma_ether_copy(u8 *dmac, struct ib_ah_attr *attr)
{
	ether_addr_copy(dmac, attr->dmac);
}

int
irdma_create_ah_stub(struct ib_ah *ib_ah,
		     struct ib_ah_attr *attr, u32 flags,
		     struct ib_udata *udata)
{
	return -ENOSYS;
}

void
irdma_destroy_ah_stub(struct ib_ah *ibah, u32 flags)
{
	return;
}

/**
 * irdma_free_qp_rsrc - free up memory resources for qp
 * @iwqp: qp ptr (user or kernel)
 */
void
irdma_free_qp_rsrc(struct irdma_qp *iwqp)
{
	struct irdma_device *iwdev = iwqp->iwdev;
	struct irdma_pci_f *rf = iwdev->rf;
	u32 qp_num = iwqp->ibqp.qp_num;

	irdma_ieq_cleanup_qp(iwdev->vsi.ieq, &iwqp->sc_qp);
	irdma_dealloc_push_page(rf, &iwqp->sc_qp);
	if (iwqp->sc_qp.vsi) {
		irdma_qp_rem_qos(&iwqp->sc_qp);
		iwqp->sc_qp.dev->ws_remove(iwqp->sc_qp.vsi,
					   iwqp->sc_qp.user_pri);
	}

	if (qp_num > 2)
		irdma_free_rsrc(rf, rf->allocated_qps, qp_num);
	irdma_free_dma_mem(rf->sc_dev.hw, &iwqp->q2_ctx_mem);
	irdma_free_dma_mem(rf->sc_dev.hw, &iwqp->kqp.dma_mem);
	kfree(iwqp->kqp.sig_trk_mem);
	iwqp->kqp.sig_trk_mem = NULL;
	kfree(iwqp->kqp.sq_wrid_mem);
	kfree(iwqp->kqp.rq_wrid_mem);
	kfree(iwqp->sg_list);
	kfree(iwqp);
}

/**
 * irdma_create_qp - create qp
 * @ibpd: ptr of pd
 * @init_attr: attributes for qp
 * @udata: user data for create qp
 */
struct ib_qp *
irdma_create_qp(struct ib_pd *ibpd,
		struct ib_qp_init_attr *init_attr,
		struct ib_udata *udata)
{
	struct irdma_pd *iwpd = to_iwpd(ibpd);
	struct irdma_device *iwdev = to_iwdev(ibpd->device);
	struct irdma_pci_f *rf = iwdev->rf;
	struct irdma_qp *iwqp;
	struct irdma_create_qp_req req;
	struct irdma_create_qp_resp uresp = {0};
	u32 qp_num = 0;
	int ret;
	int err_code;
	int sq_size;
	int rq_size;
	struct irdma_sc_qp *qp;
	struct irdma_sc_dev *dev = &rf->sc_dev;
	struct irdma_uk_attrs *uk_attrs = &dev->hw_attrs.uk_attrs;
	struct irdma_qp_init_info init_info = {{0}};
	struct irdma_qp_host_ctx_info *ctx_info;
	unsigned long flags;

	err_code = irdma_validate_qp_attrs(init_attr, iwdev);
	if (err_code)
		return ERR_PTR(err_code);

	sq_size = init_attr->cap.max_send_wr;
	rq_size = init_attr->cap.max_recv_wr;

	init_info.vsi = &iwdev->vsi;
	init_info.qp_uk_init_info.uk_attrs = uk_attrs;
	init_info.qp_uk_init_info.sq_size = sq_size;
	init_info.qp_uk_init_info.rq_size = rq_size;
	init_info.qp_uk_init_info.max_sq_frag_cnt = init_attr->cap.max_send_sge;
	init_info.qp_uk_init_info.max_rq_frag_cnt = init_attr->cap.max_recv_sge;
	init_info.qp_uk_init_info.max_inline_data = init_attr->cap.max_inline_data;

	iwqp = kzalloc(sizeof(*iwqp), GFP_KERNEL);
	if (!iwqp)
		return ERR_PTR(-ENOMEM);

	iwqp->sg_list = kcalloc(uk_attrs->max_hw_wq_frags, sizeof(*iwqp->sg_list),
				GFP_KERNEL);
	if (!iwqp->sg_list) {
		kfree(iwqp);
		return ERR_PTR(-ENOMEM);
	}

	qp = &iwqp->sc_qp;
	qp->qp_uk.back_qp = iwqp;
	qp->qp_uk.lock = &iwqp->lock;
	qp->push_idx = IRDMA_INVALID_PUSH_PAGE_INDEX;

	iwqp->iwdev = iwdev;
	iwqp->q2_ctx_mem.size = IRDMA_Q2_BUF_SIZE + IRDMA_QP_CTX_SIZE;
	iwqp->q2_ctx_mem.va = irdma_allocate_dma_mem(dev->hw, &iwqp->q2_ctx_mem,
						     iwqp->q2_ctx_mem.size,
						     256);
	if (!iwqp->q2_ctx_mem.va) {
		kfree(iwqp->sg_list);
		kfree(iwqp);
		return ERR_PTR(-ENOMEM);
	}

	init_info.q2 = iwqp->q2_ctx_mem.va;
	init_info.q2_pa = iwqp->q2_ctx_mem.pa;
	init_info.host_ctx = (__le64 *) (init_info.q2 + IRDMA_Q2_BUF_SIZE);
	init_info.host_ctx_pa = init_info.q2_pa + IRDMA_Q2_BUF_SIZE;

	if (init_attr->qp_type == IB_QPT_GSI)
		qp_num = 1;
	else
		err_code = irdma_alloc_rsrc(rf, rf->allocated_qps, rf->max_qp,
					    &qp_num, &rf->next_qp);
	if (err_code)
		goto error;

	iwqp->iwpd = iwpd;
	iwqp->ibqp.qp_num = qp_num;
	qp = &iwqp->sc_qp;
	iwqp->iwscq = to_iwcq(init_attr->send_cq);
	iwqp->iwrcq = to_iwcq(init_attr->recv_cq);
	iwqp->host_ctx.va = init_info.host_ctx;
	iwqp->host_ctx.pa = init_info.host_ctx_pa;
	iwqp->host_ctx.size = IRDMA_QP_CTX_SIZE;

	init_info.pd = &iwpd->sc_pd;
	init_info.qp_uk_init_info.qp_id = iwqp->ibqp.qp_num;
	if (!rdma_protocol_roce(&iwdev->ibdev, 1))
		init_info.qp_uk_init_info.first_sq_wq = 1;
	iwqp->ctx_info.qp_compl_ctx = (uintptr_t)qp;
	init_waitqueue_head(&iwqp->waitq);
	init_waitqueue_head(&iwqp->mod_qp_waitq);

	if (udata) {
		err_code = ib_copy_from_udata(&req, udata,
					      min(sizeof(req), udata->inlen));
		if (err_code) {
			irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
				    "ib_copy_from_data fail\n");
			goto error;
		}

		iwqp->ctx_info.qp_compl_ctx = req.user_compl_ctx;
		iwqp->user_mode = 1;
		if (req.user_wqe_bufs) {
			struct irdma_ucontext *ucontext = to_ucontext(ibpd->uobject->context);

			init_info.qp_uk_init_info.legacy_mode = ucontext->legacy_mode;
			spin_lock_irqsave(&ucontext->qp_reg_mem_list_lock, flags);
			iwqp->iwpbl = irdma_get_pbl((unsigned long)req.user_wqe_bufs,
						    &ucontext->qp_reg_mem_list);
			spin_unlock_irqrestore(&ucontext->qp_reg_mem_list_lock, flags);

			if (!iwqp->iwpbl) {
				err_code = -ENODATA;
				irdma_debug(iwdev_to_idev(iwdev),
					    IRDMA_DEBUG_VERBS,
					    "no pbl info\n");
				goto error;
			}
		}
		init_info.qp_uk_init_info.abi_ver = iwpd->sc_pd.abi_ver;
		irdma_setup_virt_qp(iwdev, iwqp, &init_info);
	} else {
		INIT_DELAYED_WORK(&iwqp->dwork_flush, irdma_flush_worker);
		init_info.qp_uk_init_info.abi_ver = IRDMA_ABI_VER;
		err_code = irdma_setup_kmode_qp(iwdev, iwqp, &init_info, init_attr);
	}

	if (err_code) {
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "setup qp failed\n");
		goto error;
	}

	if (rdma_protocol_roce(&iwdev->ibdev, 1)) {
		if (init_attr->qp_type == IB_QPT_RC) {
			init_info.qp_uk_init_info.type = IRDMA_QP_TYPE_ROCE_RC;
			init_info.qp_uk_init_info.qp_caps = IRDMA_SEND_WITH_IMM |
			    IRDMA_WRITE_WITH_IMM |
			    IRDMA_ROCE;
		} else {
			init_info.qp_uk_init_info.type = IRDMA_QP_TYPE_ROCE_UD;
			init_info.qp_uk_init_info.qp_caps = IRDMA_SEND_WITH_IMM |
			    IRDMA_ROCE;
		}
	} else {
		init_info.qp_uk_init_info.type = IRDMA_QP_TYPE_IWARP;
		init_info.qp_uk_init_info.qp_caps = IRDMA_WRITE_WITH_IMM;
	}

	ret = irdma_sc_qp_init(qp, &init_info);
	if (ret) {
		err_code = -EPROTO;
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "qp_init fail\n");
		goto error;
	}

	ctx_info = &iwqp->ctx_info;
	ctx_info->send_cq_num = iwqp->iwscq->sc_cq.cq_uk.cq_id;
	ctx_info->rcv_cq_num = iwqp->iwrcq->sc_cq.cq_uk.cq_id;

	if (rdma_protocol_roce(&iwdev->ibdev, 1))
		irdma_roce_fill_and_set_qpctx_info(iwqp, ctx_info);
	else
		irdma_iw_fill_and_set_qpctx_info(iwqp, ctx_info);

	err_code = irdma_cqp_create_qp_cmd(iwqp);
	if (err_code)
		goto error;

	atomic_set(&iwqp->refcnt, 1);
	spin_lock_init(&iwqp->lock);
	spin_lock_init(&iwqp->sc_qp.pfpdu.lock);
	iwqp->sig_all = (init_attr->sq_sig_type == IB_SIGNAL_ALL_WR) ? 1 : 0;
	rf->qp_table[qp_num] = iwqp;
	iwqp->max_send_wr = sq_size;
	iwqp->max_recv_wr = rq_size;

	if (rdma_protocol_roce(&iwdev->ibdev, 1)) {
		if (dev->ws_add(&iwdev->vsi, 0)) {
			irdma_cqp_qp_destroy_cmd(&rf->sc_dev, &iwqp->sc_qp);
			err_code = -EINVAL;
			goto error;
		}

		irdma_qp_add_qos(&iwqp->sc_qp);
	}

	if (udata) {
		/* GEN_1 legacy support with libi40iw does not have expanded uresp struct */
		if (udata->outlen < sizeof(uresp)) {
			uresp.lsmm = 1;
			uresp.push_idx = IRDMA_INVALID_PUSH_PAGE_INDEX_GEN_1;
		} else {
			if (rdma_protocol_iwarp(&iwdev->ibdev, 1))
				uresp.lsmm = 1;
		}
		uresp.actual_sq_size = sq_size;
		uresp.actual_rq_size = rq_size;
		uresp.qp_id = qp_num;
		uresp.qp_caps = qp->qp_uk.qp_caps;

		err_code = ib_copy_to_udata(udata, &uresp,
					    min(sizeof(uresp), udata->outlen));
		if (err_code) {
			irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
				    "copy_to_udata failed\n");
			kc_irdma_destroy_qp(&iwqp->ibqp, udata);
			return ERR_PTR(err_code);
		}
	}

	init_completion(&iwqp->free_qp);
	return &iwqp->ibqp;

error:
	irdma_free_qp_rsrc(iwqp);

	return ERR_PTR(err_code);
}

/**
 * irdma_destroy_qp - destroy qp
 * @ibqp: qp's ib pointer also to get to device's qp address
 * @udata: user data
 */
int
irdma_destroy_qp(struct ib_qp *ibqp, struct ib_udata *udata)
{
	struct irdma_qp *iwqp = to_iwqp(ibqp);
	struct irdma_device *iwdev = iwqp->iwdev;

	if (iwqp->sc_qp.qp_uk.destroy_pending)
		goto free_rsrc;
	iwqp->sc_qp.qp_uk.destroy_pending = true;
	if (iwqp->iwarp_state == IRDMA_QP_STATE_RTS)
		irdma_modify_qp_to_err(&iwqp->sc_qp);

	if (!iwqp->user_mode)
		cancel_delayed_work_sync(&iwqp->dwork_flush);

	irdma_qp_rem_ref(&iwqp->ibqp);
	wait_for_completion(&iwqp->free_qp);
	irdma_free_lsmm_rsrc(iwqp);
	if (!iwdev->rf->reset &&
	    irdma_cqp_qp_destroy_cmd(&iwdev->rf->sc_dev, &iwqp->sc_qp))
		return -ENOTRECOVERABLE;
free_rsrc:
	if (!iwqp->user_mode) {
		if (iwqp->iwscq) {
			irdma_clean_cqes(iwqp, iwqp->iwscq);
			if (iwqp->iwrcq != iwqp->iwscq)
				irdma_clean_cqes(iwqp, iwqp->iwrcq);
		}
	}
	irdma_remove_push_mmap_entries(iwqp);
	irdma_free_qp_rsrc(iwqp);

	return 0;
}

/**
 * irdma_create_cq - create cq
 * @ibcq: CQ allocated
 * @attr: attributes for cq
 * @udata: user data
 */
int
irdma_create_cq(struct ib_cq *ibcq,
		const struct ib_cq_init_attr *attr,
		struct ib_udata *udata)
{
	struct ib_device *ibdev = ibcq->device;
	struct irdma_device *iwdev = to_iwdev(ibdev);
	struct irdma_pci_f *rf = iwdev->rf;
	struct irdma_cq *iwcq = to_iwcq(ibcq);
	u32 cq_num = 0;
	struct irdma_sc_cq *cq;
	struct irdma_sc_dev *dev = &rf->sc_dev;
	struct irdma_cq_init_info info = {0};
	int status;
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	struct irdma_cq_uk_init_info *ukinfo = &info.cq_uk_init_info;
	unsigned long flags;
	int err_code;
	int entries = attr->cqe;

	err_code = cq_validate_flags(attr->flags, dev->hw_attrs.uk_attrs.hw_rev);
	if (err_code)
		return err_code;
	err_code = irdma_alloc_rsrc(rf, rf->allocated_cqs, rf->max_cq, &cq_num,
				    &rf->next_cq);
	if (err_code)
		return err_code;
	cq = &iwcq->sc_cq;
	cq->back_cq = iwcq;
	atomic_set(&iwcq->refcnt, 1);
	spin_lock_init(&iwcq->lock);
	INIT_LIST_HEAD(&iwcq->resize_list);
	INIT_LIST_HEAD(&iwcq->cmpl_generated);
	info.dev = dev;
	ukinfo->cq_size = max(entries, 4);
	ukinfo->cq_id = cq_num;
	iwcq->ibcq.cqe = info.cq_uk_init_info.cq_size;
	if (attr->comp_vector < rf->ceqs_count)
		info.ceq_id = attr->comp_vector;
	info.ceq_id_valid = true;
	info.ceqe_mask = 1;
	info.type = IRDMA_CQ_TYPE_IWARP;
	info.vsi = &iwdev->vsi;

	if (udata) {
		struct irdma_ucontext *ucontext;
		struct irdma_create_cq_req req = {0};
		struct irdma_cq_mr *cqmr;
		struct irdma_pbl *iwpbl;
		struct irdma_pbl *iwpbl_shadow;
		struct irdma_cq_mr *cqmr_shadow;

		iwcq->user_mode = true;
		ucontext = rdma_udata_to_drv_context(udata, struct irdma_ucontext, ibucontext);
		if (ib_copy_from_udata(&req, udata,
				       min(sizeof(req), udata->inlen))) {
			err_code = -EFAULT;
			goto cq_free_rsrc;
		}

		spin_lock_irqsave(&ucontext->cq_reg_mem_list_lock, flags);
		iwpbl = irdma_get_pbl((unsigned long)req.user_cq_buf,
				      &ucontext->cq_reg_mem_list);
		spin_unlock_irqrestore(&ucontext->cq_reg_mem_list_lock, flags);
		if (!iwpbl) {
			err_code = -EPROTO;
			goto cq_free_rsrc;
		}
		iwcq->iwpbl = iwpbl;
		iwcq->cq_mem_size = 0;
		cqmr = &iwpbl->cq_mr;

		if (rf->sc_dev.hw_attrs.uk_attrs.feature_flags &
		    IRDMA_FEATURE_CQ_RESIZE && !ucontext->legacy_mode) {
			spin_lock_irqsave(&ucontext->cq_reg_mem_list_lock, flags);
			iwpbl_shadow = irdma_get_pbl((unsigned long)req.user_shadow_area,
						     &ucontext->cq_reg_mem_list);
			spin_unlock_irqrestore(&ucontext->cq_reg_mem_list_lock, flags);

			if (!iwpbl_shadow) {
				err_code = -EPROTO;
				goto cq_free_rsrc;
			}
			iwcq->iwpbl_shadow = iwpbl_shadow;
			cqmr_shadow = &iwpbl_shadow->cq_mr;
			info.shadow_area_pa = cqmr_shadow->cq_pbl.addr;
			cqmr->split = true;
		} else {
			info.shadow_area_pa = cqmr->shadow;
		}
		if (iwpbl->pbl_allocated) {
			info.virtual_map = true;
			info.pbl_chunk_size = 1;
			info.first_pm_pbl_idx = cqmr->cq_pbl.idx;
		} else {
			info.cq_base_pa = cqmr->cq_pbl.addr;
		}
	} else {
		/* Kmode allocations */
		int rsize;

		if (entries < 1 || entries > rf->max_cqe) {
			err_code = -EINVAL;
			goto cq_free_rsrc;
		}

		entries++;
		if (dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2)
			entries *= 2;
		ukinfo->cq_size = entries;

		rsize = info.cq_uk_init_info.cq_size * sizeof(struct irdma_cqe);
		iwcq->kmem.size = round_up(rsize, 256);
		iwcq->kmem.va = irdma_allocate_dma_mem(dev->hw, &iwcq->kmem,
						       iwcq->kmem.size, 256);
		if (!iwcq->kmem.va) {
			err_code = -ENOMEM;
			goto cq_free_rsrc;
		}

		iwcq->kmem_shadow.size = IRDMA_SHADOW_AREA_SIZE << 3;
		iwcq->kmem_shadow.va = irdma_allocate_dma_mem(dev->hw,
							      &iwcq->kmem_shadow,
							      iwcq->kmem_shadow.size,
							      64);

		if (!iwcq->kmem_shadow.va) {
			err_code = -ENOMEM;
			goto cq_free_rsrc;
		}
		info.shadow_area_pa = iwcq->kmem_shadow.pa;
		ukinfo->shadow_area = iwcq->kmem_shadow.va;
		ukinfo->cq_base = iwcq->kmem.va;
		info.cq_base_pa = iwcq->kmem.pa;
	}

	if (dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_2)
		info.shadow_read_threshold = min(info.cq_uk_init_info.cq_size / 2,
						 (u32)IRDMA_MAX_CQ_READ_THRESH);
	if (irdma_sc_cq_init(cq, &info)) {
		irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
			    "init cq fail\n");
		err_code = -EPROTO;
		goto cq_free_rsrc;
	}

	cqp_request = irdma_alloc_and_get_cqp_request(&rf->cqp, true);
	if (!cqp_request) {
		err_code = -ENOMEM;
		goto cq_free_rsrc;
	}
	cqp_info = &cqp_request->info;
	cqp_info->cqp_cmd = IRDMA_OP_CQ_CREATE;
	cqp_info->post_sq = 1;
	cqp_info->in.u.cq_create.cq = cq;
	cqp_info->in.u.cq_create.check_overflow = true;
	cqp_info->in.u.cq_create.scratch = (uintptr_t)cqp_request;
	status = irdma_handle_cqp_op(rf, cqp_request);
	irdma_put_cqp_request(&rf->cqp, cqp_request);
	if (status) {
		err_code = -ENOMEM;
		goto cq_free_rsrc;
	}

	if (udata) {
		struct irdma_create_cq_resp resp = {0};

		resp.cq_id = info.cq_uk_init_info.cq_id;
		resp.cq_size = info.cq_uk_init_info.cq_size;
		if (ib_copy_to_udata(udata, &resp,
				     min(sizeof(resp), udata->outlen))) {
			irdma_debug(iwdev_to_idev(iwdev), IRDMA_DEBUG_VERBS,
				    "copy to user data\n");
			err_code = -EPROTO;
			goto cq_destroy;
		}
	}

	rf->cq_table[cq_num] = iwcq;
	init_completion(&iwcq->free_cq);

	return 0;
cq_destroy:
	irdma_cq_wq_destroy(rf, cq);
cq_free_rsrc:
	irdma_cq_free_rsrc(rf, iwcq);
	return err_code;
}

/**
 * irdma_copy_user_pgaddrs - copy user page address to pble's os locally
 * @iwmr: iwmr for IB's user page addresses
 * @pbl: ple pointer to save 1 level or 0 level pble
 * @level: indicated level 0, 1 or 2
 */

void
irdma_copy_user_pgaddrs(struct irdma_mr *iwmr, u64 *pbl,
			enum irdma_pble_level level)
{
	struct ib_umem *region = iwmr->region;
	struct irdma_pbl *iwpbl = &iwmr->iwpbl;
	int chunk_pages, entry, i;
	struct scatterlist *sg;
	u64 pg_addr = 0;
	struct irdma_pble_alloc *palloc = &iwpbl->pble_alloc;
	struct irdma_pble_info *pinfo;
	u32 idx = 0;
	u32 pbl_cnt = 0;

	pinfo = (level == PBLE_LEVEL_1) ? NULL : palloc->level2.leaf;
	for_each_sg(region->sg_head.sgl, sg, region->nmap, entry) {
		chunk_pages = DIV_ROUND_UP(sg_dma_len(sg), iwmr->page_size);
		if (iwmr->type == IRDMA_MEMREG_TYPE_QP && !iwpbl->qp_mr.sq_page)
			iwpbl->qp_mr.sq_page = sg_page(sg);
		for (i = 0; i < chunk_pages; i++) {
			pg_addr = sg_dma_address(sg) + (i * iwmr->page_size);
			if ((entry + i) == 0)
				*pbl = pg_addr & iwmr->page_msk;
			else if (!(pg_addr & ~iwmr->page_msk))
				*pbl = pg_addr;
			else
				continue;
			if (++pbl_cnt == palloc->total_cnt)
				break;
			pbl = irdma_next_pbl_addr(pbl, &pinfo, &idx);
		}
	}
}

/**
 * irdma_destroy_ah - Destroy address handle
 * @ibah: pointer to address handle
 * @ah_flags: destroy flags
 */

void
irdma_destroy_ah(struct ib_ah *ibah, u32 ah_flags)
{
	struct irdma_device *iwdev = to_iwdev(ibah->device);
	struct irdma_ah *ah = to_iwah(ibah);

	irdma_ah_cqp_op(iwdev->rf, &ah->sc_ah, IRDMA_OP_AH_DESTROY,
			false, NULL, ah);

	irdma_free_rsrc(iwdev->rf, iwdev->rf->allocated_ahs,
			ah->sc_ah.ah_info.ah_idx);
}

int
irdma_dereg_mr(struct ib_mr *ib_mr, struct ib_udata *udata)
{
	struct ib_pd *ibpd = ib_mr->pd;
	struct irdma_pd *iwpd = to_iwpd(ibpd);
	struct irdma_mr *iwmr = to_iwmr(ib_mr);
	struct irdma_device *iwdev = to_iwdev(ib_mr->device);
	struct irdma_dealloc_stag_info *info;
	struct irdma_pbl *iwpbl = &iwmr->iwpbl;
	struct irdma_pble_alloc *palloc = &iwpbl->pble_alloc;
	struct irdma_cqp_request *cqp_request;
	struct cqp_cmds_info *cqp_info;
	int status;

	if (iwmr->type != IRDMA_MEMREG_TYPE_MEM) {
		if (iwmr->region) {
			struct irdma_ucontext *ucontext;

			ucontext = rdma_udata_to_drv_context(udata, struct irdma_ucontext, ibucontext);
			irdma_del_memlist(iwmr, ucontext);
		}
		goto done;
	}

	cqp_request = irdma_alloc_and_get_cqp_request(&iwdev->rf->cqp, true);
	if (!cqp_request)
		return -ENOMEM;

	cqp_info = &cqp_request->info;
	info = &cqp_info->in.u.dealloc_stag.info;
	memset(info, 0, sizeof(*info));
	info->pd_id = iwpd->sc_pd.pd_id & 0x00007fff;
	info->stag_idx = RS_64_1(ib_mr->rkey, IRDMA_CQPSQ_STAG_IDX_S);
	info->mr = true;
	if (iwpbl->pbl_allocated)
		info->dealloc_pbl = true;

	cqp_info->cqp_cmd = IRDMA_OP_DEALLOC_STAG;
	cqp_info->post_sq = 1;
	cqp_info->in.u.dealloc_stag.dev = &iwdev->rf->sc_dev;
	cqp_info->in.u.dealloc_stag.scratch = (uintptr_t)cqp_request;
	status = irdma_handle_cqp_op(iwdev->rf, cqp_request);
	irdma_put_cqp_request(&iwdev->rf->cqp, cqp_request);
	if (status)
		return status;

	irdma_free_stag(iwdev, iwmr->stag);
done:
	if (iwpbl->pbl_allocated)
		irdma_free_pble(iwdev->rf->pble_rsrc, palloc);

	if (iwmr->region)
		ib_umem_release(iwmr->region);
	kfree(iwmr);

	return 0;
}

int
kc_irdma_set_roce_cm_info(struct irdma_qp *iwqp, struct ib_qp_attr *attr,
			  u16 *vlan_id)
{
	int ret;
	union ib_gid sgid;
	struct ib_gid_attr sgid_attr;
	struct irdma_av *av = &iwqp->roce_ah.av;

	ret = ib_get_cached_gid(iwqp->ibqp.device, attr->ah_attr.port_num,
				attr->ah_attr.grh.sgid_index, &sgid,
				&sgid_attr);
	if (ret)
		return ret;

	if (sgid_attr.ndev) {
		*vlan_id = rdma_vlan_dev_vlan_id(sgid_attr.ndev);
		ether_addr_copy(iwqp->ctx_info.roce_info->mac_addr, IF_LLADDR(sgid_attr.ndev));
	}

	rdma_gid2ip((struct sockaddr *)&av->sgid_addr, &sgid);

	dev_put(sgid_attr.ndev);

	return 0;
}

/**
 * irdma_destroy_cq - destroy cq
 * @ib_cq: cq pointer
 * @udata: user data
 */
void
irdma_destroy_cq(struct ib_cq *ib_cq, struct ib_udata *udata)
{
	struct irdma_device *iwdev = to_iwdev(ib_cq->device);
	struct irdma_cq *iwcq = to_iwcq(ib_cq);
	struct irdma_sc_cq *cq = &iwcq->sc_cq;
	struct irdma_sc_dev *dev = cq->dev;
	struct irdma_sc_ceq *ceq = dev->ceq[cq->ceq_id];
	struct irdma_ceq *iwceq = container_of(ceq, struct irdma_ceq, sc_ceq);
	unsigned long flags;

	spin_lock_irqsave(&iwcq->lock, flags);
	if (!list_empty(&iwcq->cmpl_generated))
		irdma_remove_cmpls_list(iwcq);
	if (!list_empty(&iwcq->resize_list))
		irdma_process_resize_list(iwcq, iwdev, NULL);
	spin_unlock_irqrestore(&iwcq->lock, flags);

	irdma_cq_rem_ref(ib_cq);
	wait_for_completion(&iwcq->free_cq);

	irdma_cq_wq_destroy(iwdev->rf, cq);
	irdma_cq_free_rsrc(iwdev->rf, iwcq);

	spin_lock_irqsave(&iwceq->ce_lock, flags);
	irdma_sc_cleanup_ceqes(cq, ceq);
	spin_unlock_irqrestore(&iwceq->ce_lock, flags);
}

/**
 * irdma_alloc_mw - Allocate memory window
 * @pd: Protection domain
 * @type: Window type
 * @udata: user data pointer
 */
struct ib_mw *
irdma_alloc_mw(struct ib_pd *pd, enum ib_mw_type type,
	       struct ib_udata *udata)
{
	struct irdma_device *iwdev = to_iwdev(pd->device);
	struct irdma_mr *iwmr;
	int err_code;
	u32 stag;

	iwmr = kzalloc(sizeof(*iwmr), GFP_KERNEL);
	if (!iwmr)
		return ERR_PTR(-ENOMEM);

	stag = irdma_create_stag(iwdev);
	if (!stag) {
		kfree(iwmr);
		return ERR_PTR(-ENOMEM);
	}

	iwmr->stag = stag;
	iwmr->ibmw.rkey = stag;
	iwmr->ibmw.pd = pd;
	iwmr->ibmw.type = type;
	iwmr->ibmw.device = pd->device;

	err_code = irdma_hw_alloc_mw(iwdev, iwmr);
	if (err_code) {
		irdma_free_stag(iwdev, stag);
		kfree(iwmr);
		return ERR_PTR(err_code);
	}

	return &iwmr->ibmw;
}

/**
 * kc_set_loc_seq_num_mss - Set local seq number and mss
 * @cm_node: cm node info
 */
void
kc_set_loc_seq_num_mss(struct irdma_cm_node *cm_node)
{
	struct timespec ts;

	getnanotime(&ts);
	cm_node->tcp_cntxt.loc_seq_num = ts.tv_nsec;
	if (cm_node->iwdev->vsi.mtu > 1500 &&
	    2 * cm_node->iwdev->vsi.mtu > cm_node->iwdev->rcv_wnd)
		cm_node->tcp_cntxt.mss = (cm_node->ipv4) ?
		    (1500 - IRDMA_MTU_TO_MSS_IPV4) :
		    (1500 - IRDMA_MTU_TO_MSS_IPV6);
	else
		cm_node->tcp_cntxt.mss = (cm_node->ipv4) ?
		    (cm_node->iwdev->vsi.mtu - IRDMA_MTU_TO_MSS_IPV4) :
		    (cm_node->iwdev->vsi.mtu - IRDMA_MTU_TO_MSS_IPV6);
}

/**
 * irdma_disassociate_ucontext - Disassociate user context
 * @context: ib user context
 */
void
irdma_disassociate_ucontext(struct ib_ucontext *context)
{
}

struct ib_device *
ib_device_get_by_netdev(struct ifnet *netdev, int driver_id)
{
	struct irdma_device *iwdev;
	struct irdma_handler *hdl;
	unsigned long flags;

	spin_lock_irqsave(&irdma_handler_lock, flags);
	list_for_each_entry(hdl, &irdma_handlers, list) {
		iwdev = hdl->iwdev;
		if (netdev == iwdev->netdev) {
			spin_unlock_irqrestore(&irdma_handler_lock,
					       flags);
			return &iwdev->ibdev;
		}
	}
	spin_unlock_irqrestore(&irdma_handler_lock, flags);

	return NULL;
}

void
ib_unregister_device_put(struct ib_device *device)
{
	ib_unregister_device(device);
}

/**
 * irdma_query_gid_roce - Query port GID for Roce
 * @ibdev: device pointer from stack
 * @port: port number
 * @index: Entry index
 * @gid: Global ID
 */
int
irdma_query_gid_roce(struct ib_device *ibdev, u8 port, int index,
		     union ib_gid *gid)
{
	int ret;

	ret = rdma_query_gid(ibdev, port, index, gid);
	if (ret == -EAGAIN) {
		memcpy(gid, &zgid, sizeof(*gid));
		return 0;
	}

	return ret;
}

/**
 * irdma_modify_port - modify port attributes
 * @ibdev: device pointer from stack
 * @port: port number for query
 * @mask: Property mask
 * @props: returning device attributes
 */
int
irdma_modify_port(struct ib_device *ibdev, u8 port, int mask,
		  struct ib_port_modify *props)
{
	if (port > 1)
		return -EINVAL;

	return 0;
}

/**
 * irdma_query_pkey - Query partition key
 * @ibdev: device pointer from stack
 * @port: port number
 * @index: index of pkey
 * @pkey: pointer to store the pkey
 */
int
irdma_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
		 u16 *pkey)
{
	if (index >= IRDMA_PKEY_TBL_SZ)
		return -EINVAL;

	*pkey = IRDMA_DEFAULT_PKEY;
	return 0;
}

int
irdma_roce_port_immutable(struct ib_device *ibdev, u8 port_num,
			  struct ib_port_immutable *immutable)
{
	struct ib_port_attr attr;
	int err;

	immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE_UDP_ENCAP;
	err = ib_query_port(ibdev, port_num, &attr);
	if (err)
		return err;

	immutable->max_mad_size = IB_MGMT_MAD_SIZE;
	immutable->pkey_tbl_len = attr.pkey_tbl_len;
	immutable->gid_tbl_len = attr.gid_tbl_len;

	return 0;
}

int
irdma_iw_port_immutable(struct ib_device *ibdev, u8 port_num,
			struct ib_port_immutable *immutable)
{
	struct ib_port_attr attr;
	int err;

	immutable->core_cap_flags = RDMA_CORE_PORT_IWARP;
	err = ib_query_port(ibdev, port_num, &attr);
	if (err)
		return err;
	immutable->gid_tbl_len = 1;

	return 0;
}

/**
 * irdma_get_eth_speed_and_width - Get IB port speed and width from netdev speed
 * @link_speed: netdev phy link speed
 * @active_speed: IB port speed
 * @active_width: IB port width
 */
void
irdma_get_eth_speed_and_width(u32 link_speed, u8 *active_speed,
			      u8 *active_width)
{
	if (link_speed <= SPEED_1000) {
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_SDR;
	} else if (link_speed <= SPEED_10000) {
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_FDR10;
	} else if (link_speed <= SPEED_20000) {
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_DDR;
	} else if (link_speed <= SPEED_25000) {
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_EDR;
	} else if (link_speed <= SPEED_40000) {
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_FDR10;
	} else {
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_EDR;
	}
}

/**
 * irdma_query_port - get port attributes
 * @ibdev: device pointer from stack
 * @port: port number for query
 * @props: returning device attributes
 */
int
irdma_query_port(struct ib_device *ibdev, u8 port,
		 struct ib_port_attr *props)
{
	struct irdma_device *iwdev = to_iwdev(ibdev);
	struct ifnet *netdev = iwdev->netdev;

	/* no need to zero out pros here. done by caller */

	props->max_mtu = IB_MTU_4096;
	props->active_mtu = ib_mtu_int_to_enum(netdev->if_mtu);
	props->lid = 1;
	props->lmc = 0;
	props->sm_lid = 0;
	props->sm_sl = 0;
	if ((netdev->if_link_state == LINK_STATE_UP) && (netdev->if_drv_flags & IFF_DRV_RUNNING)) {
		props->state = IB_PORT_ACTIVE;
		props->phys_state = IB_PORT_PHYS_STATE_LINK_UP;
	} else {
		props->state = IB_PORT_DOWN;
		props->phys_state = IB_PORT_PHYS_STATE_DISABLED;
	}
	irdma_get_eth_speed_and_width(SPEED_100000, &props->active_speed,
				      &props->active_width);

	if (rdma_protocol_roce(ibdev, 1)) {
		props->gid_tbl_len = 32;
		kc_set_props_ip_gid_caps(props);
		props->pkey_tbl_len = IRDMA_PKEY_TBL_SZ;
	} else {
		props->gid_tbl_len = 1;
	}
	props->qkey_viol_cntr = 0;
	props->port_cap_flags |= IB_PORT_CM_SUP | IB_PORT_REINIT_SUP;
	props->max_msg_sz = iwdev->rf->sc_dev.hw_attrs.max_hw_outbound_msg_size;

	return 0;
}

extern const char *const irdma_hw_stat_names[];

/**
 * irdma_alloc_hw_stats - Allocate a hw stats structure
 * @ibdev: device pointer from stack
 * @port_num: port number
 */
struct rdma_hw_stats *
irdma_alloc_hw_stats(struct ib_device *ibdev,
		     u8 port_num)
{
	struct irdma_device *iwdev = to_iwdev(ibdev);
	struct irdma_sc_dev *dev = &iwdev->rf->sc_dev;

	int num_counters = dev->hw_attrs.max_stat_idx;
	unsigned long lifespan = RDMA_HW_STATS_DEFAULT_LIFESPAN;

	return rdma_alloc_hw_stats_struct(irdma_hw_stat_names, num_counters,
					  lifespan);
}

/**
 * irdma_get_hw_stats - Populates the rdma_hw_stats structure
 * @ibdev: device pointer from stack
 * @stats: stats pointer from stack
 * @port_num: port number
 * @index: which hw counter the stack is requesting we update
 */
int
irdma_get_hw_stats(struct ib_device *ibdev,
		   struct rdma_hw_stats *stats, u8 port_num,
		   int index)
{
	struct irdma_device *iwdev = to_iwdev(ibdev);
	struct irdma_dev_hw_stats *hw_stats = &iwdev->vsi.pestat->hw_stats;

	if (iwdev->rf->rdma_ver >= IRDMA_GEN_2)
		irdma_cqp_gather_stats_cmd(&iwdev->rf->sc_dev, iwdev->vsi.pestat, true);

	memcpy(&stats->value[0], hw_stats, sizeof(u64)* stats->num_counters);

	return stats->num_counters;
}

/**
 * irdma_query_gid - Query port GID
 * @ibdev: device pointer from stack
 * @port: port number
 * @index: Entry index
 * @gid: Global ID
 */
int
irdma_query_gid(struct ib_device *ibdev, u8 port, int index,
		union ib_gid *gid)
{
	struct irdma_device *iwdev = to_iwdev(ibdev);

	memset(gid->raw, 0, sizeof(gid->raw));
	ether_addr_copy(gid->raw, IF_LLADDR(iwdev->netdev));

	return 0;
}

enum rdma_link_layer
irdma_get_link_layer(struct ib_device *ibdev,
		     u8 port_num)
{
	return IB_LINK_LAYER_ETHERNET;
}

inline enum ib_mtu
ib_mtu_int_to_enum(int mtu)
{
	if (mtu >= 4096)
		return IB_MTU_4096;
	else if (mtu >= 2048)
		return IB_MTU_2048;
	else if (mtu >= 1024)
		return IB_MTU_1024;
	else if (mtu >= 512)
		return IB_MTU_512;
	else
		return IB_MTU_256;
}

inline void
kc_set_roce_uverbs_cmd_mask(struct irdma_device *iwdev)
{
	iwdev->ibdev.uverbs_cmd_mask |=
	    BIT_ULL(IB_USER_VERBS_CMD_ATTACH_MCAST) |
	    BIT_ULL(IB_USER_VERBS_CMD_CREATE_AH) |
	    BIT_ULL(IB_USER_VERBS_CMD_DESTROY_AH) |
	    BIT_ULL(IB_USER_VERBS_CMD_DETACH_MCAST);
}

inline void
kc_set_rdma_uverbs_cmd_mask(struct irdma_device *iwdev)
{
	iwdev->ibdev.uverbs_cmd_mask =
	    BIT_ULL(IB_USER_VERBS_CMD_GET_CONTEXT) |
	    BIT_ULL(IB_USER_VERBS_CMD_QUERY_DEVICE) |
	    BIT_ULL(IB_USER_VERBS_CMD_QUERY_PORT) |
	    BIT_ULL(IB_USER_VERBS_CMD_ALLOC_PD) |
	    BIT_ULL(IB_USER_VERBS_CMD_DEALLOC_PD) |
	    BIT_ULL(IB_USER_VERBS_CMD_REG_MR) |
	    BIT_ULL(IB_USER_VERBS_CMD_DEREG_MR) |
	    BIT_ULL(IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL) |
	    BIT_ULL(IB_USER_VERBS_CMD_CREATE_CQ) |
	    BIT_ULL(IB_USER_VERBS_CMD_RESIZE_CQ) |
	    BIT_ULL(IB_USER_VERBS_CMD_DESTROY_CQ) |
	    BIT_ULL(IB_USER_VERBS_CMD_REQ_NOTIFY_CQ) |
	    BIT_ULL(IB_USER_VERBS_CMD_CREATE_QP) |
	    BIT_ULL(IB_USER_VERBS_CMD_MODIFY_QP) |
	    BIT_ULL(IB_USER_VERBS_CMD_QUERY_QP) |
	    BIT_ULL(IB_USER_VERBS_CMD_POLL_CQ) |
	    BIT_ULL(IB_USER_VERBS_CMD_DESTROY_QP) |
	    BIT_ULL(IB_USER_VERBS_CMD_ALLOC_MW) |
	    BIT_ULL(IB_USER_VERBS_CMD_BIND_MW) |
	    BIT_ULL(IB_USER_VERBS_CMD_DEALLOC_MW) |
	    BIT_ULL(IB_USER_VERBS_CMD_POST_RECV) |
	    BIT_ULL(IB_USER_VERBS_CMD_POST_SEND);
	iwdev->ibdev.uverbs_ex_cmd_mask =
	    BIT_ULL(IB_USER_VERBS_EX_CMD_MODIFY_QP) |
	    BIT_ULL(IB_USER_VERBS_EX_CMD_QUERY_DEVICE);

	if (iwdev->rf->rdma_ver >= IRDMA_GEN_2)
		iwdev->ibdev.uverbs_ex_cmd_mask |= BIT_ULL(IB_USER_VERBS_EX_CMD_CREATE_CQ);
}
