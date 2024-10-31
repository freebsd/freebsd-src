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
 * Description: IB Verbs interpreter
 */

#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <rdma/uverbs_ioctl.h>

#include "bnxt_re.h"
#include "ib_verbs.h"

static inline
struct scatterlist *get_ib_umem_sgl(struct ib_umem *umem, u32 *nmap)
{

	*nmap = umem->nmap;
	return umem->sg_head.sgl;
}

static inline void bnxt_re_peer_mem_release(struct ib_umem *umem)
{
	dev_dbg(NULL, "ib_umem_release getting invoked \n");
	ib_umem_release(umem);
}

void bnxt_re_resolve_dmac_task(struct work_struct *work)
{
	int rc = -1;
	struct bnxt_re_dev *rdev;
	struct ib_ah_attr	*ah_attr;
	struct bnxt_re_resolve_dmac_work *dmac_work =
			container_of(work, struct bnxt_re_resolve_dmac_work, work);

	rdev = dmac_work->rdev;
	ah_attr = dmac_work->ah_attr;
	rc = ib_resolve_eth_dmac(&rdev->ibdev, ah_attr);
	if (rc)
		dev_err(rdev_to_dev(dmac_work->rdev),
			"Failed to resolve dest mac rc = %d\n", rc);
	atomic_set(&dmac_work->status_wait, rc << 8);
}

static int __from_ib_access_flags(int iflags)
{
	int qflags = 0;

	if (iflags & IB_ACCESS_LOCAL_WRITE)
		qflags |= BNXT_QPLIB_ACCESS_LOCAL_WRITE;
	if (iflags & IB_ACCESS_REMOTE_READ)
		qflags |= BNXT_QPLIB_ACCESS_REMOTE_READ;
	if (iflags & IB_ACCESS_REMOTE_WRITE)
		qflags |= BNXT_QPLIB_ACCESS_REMOTE_WRITE;
	if (iflags & IB_ACCESS_REMOTE_ATOMIC)
		qflags |= BNXT_QPLIB_ACCESS_REMOTE_ATOMIC;
	if (iflags & IB_ACCESS_MW_BIND)
		qflags |= BNXT_QPLIB_ACCESS_MW_BIND;
	if (iflags & IB_ZERO_BASED)
		qflags |= BNXT_QPLIB_ACCESS_ZERO_BASED;
	if (iflags & IB_ACCESS_ON_DEMAND)
		qflags |= BNXT_QPLIB_ACCESS_ON_DEMAND;
	return qflags;
};

static enum ib_access_flags __to_ib_access_flags(int qflags)
{
	enum ib_access_flags iflags = 0;

	if (qflags & BNXT_QPLIB_ACCESS_LOCAL_WRITE)
		iflags |= IB_ACCESS_LOCAL_WRITE;
	if (qflags & BNXT_QPLIB_ACCESS_REMOTE_WRITE)
		iflags |= IB_ACCESS_REMOTE_WRITE;
	if (qflags & BNXT_QPLIB_ACCESS_REMOTE_READ)
		iflags |= IB_ACCESS_REMOTE_READ;
	if (qflags & BNXT_QPLIB_ACCESS_REMOTE_ATOMIC)
		iflags |= IB_ACCESS_REMOTE_ATOMIC;
	if (qflags & BNXT_QPLIB_ACCESS_MW_BIND)
		iflags |= IB_ACCESS_MW_BIND;
	if (qflags & BNXT_QPLIB_ACCESS_ZERO_BASED)
		iflags |= IB_ZERO_BASED;
	if (qflags & BNXT_QPLIB_ACCESS_ON_DEMAND)
		iflags |= IB_ACCESS_ON_DEMAND;
	return iflags;
};

static int bnxt_re_copy_to_udata(struct bnxt_re_dev *rdev, void *data,
				 int len, struct ib_udata *udata)
{
	int rc;

	rc = ib_copy_to_udata(udata, data, len);
	if (rc)
		dev_err(rdev_to_dev(rdev),
			"ucontext copy failed from %ps rc %d\n",
			__builtin_return_address(0), rc);

	return rc;
}

struct ifnet *bnxt_re_get_netdev(struct ib_device *ibdev,
				 u8 port_num)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct ifnet *netdev = NULL;

	rcu_read_lock();

	if (!rdev || !rdev->netdev)
		goto end;

	netdev = rdev->netdev;

	/* In case of active-backup bond mode, return active slave */
	if (netdev)
		dev_hold(netdev);

end:
	rcu_read_unlock();
	return netdev;
}

int bnxt_re_query_device(struct ib_device *ibdev,
			 struct ib_device_attr *ib_attr,
			 struct ib_udata *udata)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;

	memset(ib_attr, 0, sizeof(*ib_attr));

	memcpy(&ib_attr->fw_ver, dev_attr->fw_ver, 4);
	bnxt_qplib_get_guid(rdev->dev_addr, (u8 *)&ib_attr->sys_image_guid);
	ib_attr->max_mr_size = BNXT_RE_MAX_MR_SIZE;
	ib_attr->page_size_cap = dev_attr->page_size_cap;
	ib_attr->vendor_id = rdev->en_dev->pdev->vendor;
	ib_attr->vendor_part_id = rdev->en_dev->pdev->device;
	ib_attr->hw_ver = rdev->en_dev->pdev->subsystem_device;
	ib_attr->max_qp = dev_attr->max_qp;
	ib_attr->max_qp_wr = dev_attr->max_qp_wqes;
	/*
	 * Read and set from the module param 'min_tx_depth'
	 * only once after the driver load
	 */
	if (rdev->min_tx_depth == 1 &&
	    min_tx_depth < dev_attr->max_qp_wqes)
		rdev->min_tx_depth = min_tx_depth;
	ib_attr->device_cap_flags =
				    IB_DEVICE_CURR_QP_STATE_MOD
				    | IB_DEVICE_RC_RNR_NAK_GEN
				    | IB_DEVICE_SHUTDOWN_PORT
				    | IB_DEVICE_SYS_IMAGE_GUID
				    | IB_DEVICE_LOCAL_DMA_LKEY
				    | IB_DEVICE_RESIZE_MAX_WR
				    | IB_DEVICE_PORT_ACTIVE_EVENT
				    | IB_DEVICE_N_NOTIFY_CQ
				    | IB_DEVICE_MEM_WINDOW
				    | IB_DEVICE_MEM_WINDOW_TYPE_2B
				    | IB_DEVICE_MEM_MGT_EXTENSIONS;
	ib_attr->max_send_sge = dev_attr->max_qp_sges;
	ib_attr->max_recv_sge = dev_attr->max_qp_sges;
	ib_attr->max_sge_rd = dev_attr->max_qp_sges;
	ib_attr->max_cq = dev_attr->max_cq;
	ib_attr->max_cqe = dev_attr->max_cq_wqes;
	ib_attr->max_mr = dev_attr->max_mr;
	ib_attr->max_pd = dev_attr->max_pd;
	ib_attr->max_qp_rd_atom = dev_attr->max_qp_rd_atom;
	ib_attr->max_qp_init_rd_atom = dev_attr->max_qp_init_rd_atom;
	if (dev_attr->is_atomic) {
		ib_attr->atomic_cap = IB_ATOMIC_GLOB;
		ib_attr->masked_atomic_cap = IB_ATOMIC_GLOB;
	}
	ib_attr->max_ee_rd_atom = 0;
	ib_attr->max_res_rd_atom = 0;
	ib_attr->max_ee_init_rd_atom = 0;
	ib_attr->max_ee = 0;
	ib_attr->max_rdd = 0;
	ib_attr->max_mw = dev_attr->max_mw;
	ib_attr->max_raw_ipv6_qp = 0;
	ib_attr->max_raw_ethy_qp = dev_attr->max_raw_ethy_qp;
	ib_attr->max_mcast_grp = 0;
	ib_attr->max_mcast_qp_attach = 0;
	ib_attr->max_total_mcast_qp_attach = 0;
	ib_attr->max_ah = dev_attr->max_ah;
	ib_attr->max_srq = dev_attr->max_srq;
	ib_attr->max_srq_wr = dev_attr->max_srq_wqes;
	ib_attr->max_srq_sge = dev_attr->max_srq_sges;

	ib_attr->max_fast_reg_page_list_len = MAX_PBL_LVL_1_PGS;
	ib_attr->max_pkeys = 1;
	ib_attr->local_ca_ack_delay = BNXT_RE_DEFAULT_ACK_DELAY;
	ib_attr->sig_prot_cap = 0;
	ib_attr->sig_guard_cap = 0;
	ib_attr->odp_caps.general_caps = 0;

	return 0;
}

int bnxt_re_modify_device(struct ib_device *ibdev,
			  int device_modify_mask,
			  struct ib_device_modify *device_modify)
{
	dev_dbg(rdev_to_dev(rdev), "Modify device with mask 0x%x\n",
		device_modify_mask);

	switch (device_modify_mask) {
	case IB_DEVICE_MODIFY_SYS_IMAGE_GUID:
		/* Modify the GUID requires the modification of the GID table */
		/* GUID should be made as READ-ONLY */
		break;
	case IB_DEVICE_MODIFY_NODE_DESC:
		/* Node Desc should be made as READ-ONLY */
		break;
	default:
		break;
	}
	return 0;
}

static void __to_ib_speed_width(u32 espeed, u8 *speed, u8 *width)
{
	switch (espeed) {
	case SPEED_1000:
		*speed = IB_SPEED_SDR;
		*width = IB_WIDTH_1X;
		break;
	case SPEED_10000:
		*speed = IB_SPEED_QDR;
		*width = IB_WIDTH_1X;
		break;
	case SPEED_20000:
		*speed = IB_SPEED_DDR;
		*width = IB_WIDTH_4X;
		break;
	case SPEED_25000:
		*speed = IB_SPEED_EDR;
		*width = IB_WIDTH_1X;
		break;
	case SPEED_40000:
		*speed = IB_SPEED_QDR;
		*width = IB_WIDTH_4X;
		break;
	case SPEED_50000:
		*speed = IB_SPEED_EDR;
		*width = IB_WIDTH_2X;
		break;
	case SPEED_100000:
		*speed = IB_SPEED_EDR;
		*width = IB_WIDTH_4X;
		break;
	case SPEED_200000:
		*speed = IB_SPEED_HDR;
		*width = IB_WIDTH_4X;
		break;
	default:
		*speed = IB_SPEED_SDR;
		*width = IB_WIDTH_1X;
		break;
	}
}

/* Port */
int bnxt_re_query_port(struct ib_device *ibdev, u8 port_num,
		       struct ib_port_attr *port_attr)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;
	u8 active_speed = 0, active_width = 0;

	dev_dbg(rdev_to_dev(rdev), "QUERY PORT with port_num 0x%x\n", port_num);
	memset(port_attr, 0, sizeof(*port_attr));

	port_attr->phys_state = IB_PORT_PHYS_STATE_DISABLED;
	port_attr->state = bnxt_re_get_link_state(rdev);
	if (port_attr->state == IB_PORT_ACTIVE)
		port_attr->phys_state = IB_PORT_PHYS_STATE_LINK_UP;
	port_attr->max_mtu = IB_MTU_4096;
	port_attr->active_mtu = iboe_get_mtu(if_getmtu(rdev->netdev));
	port_attr->gid_tbl_len = dev_attr->max_sgid;
	port_attr->port_cap_flags = IB_PORT_CM_SUP | IB_PORT_REINIT_SUP |
				    IB_PORT_DEVICE_MGMT_SUP |
				    IB_PORT_VENDOR_CLASS_SUP |
				    IB_PORT_IP_BASED_GIDS;

	port_attr->max_msg_sz = (u32)BNXT_RE_MAX_MR_SIZE_LOW;
	port_attr->bad_pkey_cntr = 0;
	port_attr->qkey_viol_cntr = 0;
	port_attr->pkey_tbl_len = dev_attr->max_pkey;
	port_attr->lid = 0;
	port_attr->sm_lid = 0;
	port_attr->lmc = 0;
	port_attr->max_vl_num = 4;
	port_attr->sm_sl = 0;
	port_attr->subnet_timeout = 0;
	port_attr->init_type_reply = 0;
	rdev->espeed = rdev->en_dev->espeed;

	if (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags))
		__to_ib_speed_width(rdev->espeed, &active_speed,
				    &active_width);

	port_attr->active_speed = active_speed;
	port_attr->active_width = active_width;

	return 0;
}

int bnxt_re_modify_port(struct ib_device *ibdev, u8 port_num,
			int port_modify_mask,
			struct ib_port_modify *port_modify)
{
	dev_dbg(rdev_to_dev(rdev), "Modify port with mask 0x%x\n",
		port_modify_mask);

	switch (port_modify_mask) {
	case IB_PORT_SHUTDOWN:
		break;
	case IB_PORT_INIT_TYPE:
		break;
	case IB_PORT_RESET_QKEY_CNTR:
		break;
	default:
		break;
	}
	return 0;
}

int bnxt_re_get_port_immutable(struct ib_device *ibdev, u8 port_num,
			       struct ib_port_immutable *immutable)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct ib_port_attr port_attr;

	if (bnxt_re_query_port(ibdev, port_num, &port_attr))
		return -EINVAL;

	immutable->pkey_tbl_len = port_attr.pkey_tbl_len;
	immutable->gid_tbl_len = port_attr.gid_tbl_len;
	if (rdev->roce_mode == BNXT_RE_FLAG_ROCEV1_CAP)
		immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE;
	else if (rdev->roce_mode == BNXT_RE_FLAG_ROCEV2_CAP)
		immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE_UDP_ENCAP;
	else
		immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE |
					    RDMA_CORE_PORT_IBA_ROCE_UDP_ENCAP;
	immutable->max_mad_size = IB_MGMT_MAD_SIZE;
	return 0;
}

void bnxt_re_compat_qfwstr(void)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);

	sprintf(str, "%d.%d.%d.%d", rdev->dev_attr->fw_ver[0],
		rdev->dev_attr->fw_ver[1], rdev->dev_attr->fw_ver[2],
		rdev->dev_attr->fw_ver[3]);
}

int bnxt_re_query_pkey(struct ib_device *ibdev, u8 port_num,
		       u16 index, u16 *pkey)
{
	if (index > 0)
		return -EINVAL;

	*pkey = IB_DEFAULT_PKEY_FULL;

	return 0;
}

int bnxt_re_query_gid(struct ib_device *ibdev, u8 port_num,
		      int index, union ib_gid *gid)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	int rc = 0;

	/* Ignore port_num */
	memset(gid, 0, sizeof(*gid));
	rc = bnxt_qplib_get_sgid(&rdev->qplib_res,
				 &rdev->qplib_res.sgid_tbl, index,
				 (struct bnxt_qplib_gid *)gid);
	return rc;
}

int bnxt_re_del_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, void **context)
{
	int rc = 0;
	struct bnxt_re_gid_ctx *ctx, **ctx_tbl;
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;
	struct bnxt_qplib_gid *gid_to_del;
	u16 vlan_id = 0xFFFF;

	/* Delete the entry from the hardware */
	ctx = *context;
	if (!ctx) {
		dev_err(rdev_to_dev(rdev), "GID entry has no ctx?!\n");
		return -EINVAL;
	}
	if (sgid_tbl && sgid_tbl->active) {
		if (ctx->idx >= sgid_tbl->max) {
			dev_dbg(rdev_to_dev(rdev), "GID index out of range?!\n");
			return -EINVAL;
		}
		gid_to_del = &sgid_tbl->tbl[ctx->idx].gid;
		vlan_id = sgid_tbl->tbl[ctx->idx].vlan_id;
		ctx->refcnt--;
		/* DEL_GID is called via WQ context(netdevice_event_work_handler)
		 * or via the ib_unregister_device path. In the former case QP1
		 * may not be destroyed yet, in which case just return as FW
		 * needs that entry to be present and will fail it's deletion.
		 * We could get invoked again after QP1 is destroyed OR get an
		 * ADD_GID call with a different GID value for the same index
		 * where we issue MODIFY_GID cmd to update the GID entry -- TBD
		 */
		if (ctx->idx == 0 &&
		    rdma_link_local_addr((struct in6_addr *)gid_to_del) &&
		    (rdev->gsi_ctx.gsi_sqp ||
		     rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_UD)) {
			dev_dbg(rdev_to_dev(rdev),
				"Trying to delete GID0 while QP1 is alive\n");
			if (!ctx->refcnt) {
				rdev->gid_map[index] = -1;
				ctx_tbl = sgid_tbl->ctx;
				ctx_tbl[ctx->idx] = NULL;
				kfree(ctx);
			}
			return 0;
		}
		rdev->gid_map[index] = -1;
		if (!ctx->refcnt) {
			rc = bnxt_qplib_del_sgid(sgid_tbl, gid_to_del,
						 vlan_id, true);
			if (!rc) {
				dev_dbg(rdev_to_dev(rdev), "GID remove success\n");
				ctx_tbl = sgid_tbl->ctx;
				ctx_tbl[ctx->idx] = NULL;
				kfree(ctx);
			} else {
				dev_err(rdev_to_dev(rdev),
					"Remove GID failed rc = 0x%x\n", rc);
			}
		}
	} else {
		dev_dbg(rdev_to_dev(rdev), "GID sgid_tbl does not exist!\n");
		return -EINVAL;
	}
	return rc;
}

int bnxt_re_add_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context)
{
	int rc;
	u32 tbl_idx = 0;
	u16 vlan_id = 0xFFFF;
	struct bnxt_re_gid_ctx *ctx, **ctx_tbl;
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;
	if ((attr->ndev) && is_vlan_dev(attr->ndev))
		vlan_id = vlan_dev_vlan_id(attr->ndev);

	rc = bnxt_qplib_add_sgid(sgid_tbl, gid,
				 rdev->dev_addr,
				 vlan_id, true, &tbl_idx);
	if (rc == -EALREADY) {
		dev_dbg(rdev_to_dev(rdev), "GID %pI6 is already present\n", gid);
		ctx_tbl = sgid_tbl->ctx;
		if (!ctx_tbl[tbl_idx]) {
			ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
			if (!ctx)
				return -ENOMEM;
			ctx->idx = tbl_idx;
			ctx->refcnt = 1;
			ctx_tbl[tbl_idx] = ctx;
		} else {
			ctx_tbl[tbl_idx]->refcnt++;
		}
		*context = ctx_tbl[tbl_idx];
		/* tbl_idx is the HW table index and index is the stack index */
		rdev->gid_map[index] = tbl_idx;
		return 0;
	} else if (rc < 0) {
		dev_err(rdev_to_dev(rdev), "Add GID failed rc = 0x%x\n", rc);
		return rc;
	} else {
		ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
		if (!ctx) {
			dev_err(rdev_to_dev(rdev), "Add GID ctx failed\n");
			return -ENOMEM;
		}
		ctx_tbl = sgid_tbl->ctx;
		ctx->idx = tbl_idx;
		ctx->refcnt = 1;
		ctx_tbl[tbl_idx] = ctx;
		/* tbl_idx is the HW table index and index is the stack index */
		rdev->gid_map[index] = tbl_idx;
		*context = ctx;
	}
	return rc;
}

enum rdma_link_layer bnxt_re_get_link_layer(struct ib_device *ibdev,
					    u8 port_num)
{
	return IB_LINK_LAYER_ETHERNET;
}

static void bnxt_re_legacy_create_fence_wqe(struct bnxt_re_pd *pd)
{
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct ib_mr *ib_mr = &fence->mr->ib_mr;
	struct bnxt_qplib_swqe *wqe = &fence->bind_wqe;
	struct bnxt_re_dev *rdev = pd->rdev;

	if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
		return;

	memset(wqe, 0, sizeof(*wqe));
	wqe->type = BNXT_QPLIB_SWQE_TYPE_BIND_MW;
	wqe->wr_id = BNXT_QPLIB_FENCE_WRID;
	wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	wqe->bind.zero_based = false;
	wqe->bind.parent_l_key = ib_mr->lkey;
	wqe->bind.va = (u64)fence->va;
	wqe->bind.length = fence->size;
	wqe->bind.access_cntl = __from_ib_access_flags(IB_ACCESS_REMOTE_READ);
	wqe->bind.mw_type = SQ_BIND_MW_TYPE_TYPE1;

	/* Save the initial rkey in fence structure for now;
	 * wqe->bind.r_key will be set at (re)bind time.
	 */
	fence->bind_rkey = ib_inc_rkey(fence->mw->rkey);
}

static int bnxt_re_legacy_bind_fence_mw(struct bnxt_qplib_qp *qplib_qp)
{
	struct bnxt_re_qp *qp = container_of(qplib_qp, struct bnxt_re_qp,
					     qplib_qp);
	struct ib_pd *ib_pd = qp->ib_qp.pd;
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct bnxt_qplib_swqe *fence_wqe = &fence->bind_wqe;
	struct bnxt_qplib_swqe wqe;
	int rc;

	/* TODO: Need SQ locking here when Fence WQE
	 * posting moves up into bnxt_re from bnxt_qplib.
	 */
	memcpy(&wqe, fence_wqe, sizeof(wqe));
	wqe.bind.r_key = fence->bind_rkey;
	fence->bind_rkey = ib_inc_rkey(fence->bind_rkey);

	dev_dbg(rdev_to_dev(qp->rdev),
		"Posting bind fence-WQE: rkey: %#x QP: %d PD: %p\n",
		wqe.bind.r_key, qp->qplib_qp.id, pd);
	rc = bnxt_qplib_post_send(&qp->qplib_qp, &wqe);
	if (rc) {
		dev_err(rdev_to_dev(qp->rdev), "Failed to bind fence-WQE\n");
		return rc;
	}
	bnxt_qplib_post_send_db(&qp->qplib_qp);

	return rc;
}

static int bnxt_re_legacy_create_fence_mr(struct bnxt_re_pd *pd)
{
	int mr_access_flags = IB_ACCESS_LOCAL_WRITE | IB_ACCESS_MW_BIND;
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_qplib_mrinfo mrinfo;
	struct bnxt_re_mr *mr = NULL;
	struct ib_mw *ib_mw = NULL;
	dma_addr_t dma_addr = 0;
	u32 max_mr_count;
	u64 pbl_tbl;
	int rc;

	if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
		return 0;

	memset(&mrinfo, 0, sizeof(mrinfo));
	/* Allocate a small chunk of memory and dma-map it */
	fence->va = kzalloc(BNXT_RE_LEGACY_FENCE_BYTES, GFP_KERNEL);
	if (!fence->va)
		return -ENOMEM;
	dma_addr = ib_dma_map_single(&rdev->ibdev, fence->va,
				     BNXT_RE_LEGACY_FENCE_BYTES,
				     DMA_BIDIRECTIONAL);
	rc = ib_dma_mapping_error(&rdev->ibdev, dma_addr);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to dma-map fence-MR-mem\n");
		rc = -EIO;
		fence->dma_addr = 0;
		goto free_va;
	}
	fence->dma_addr = dma_addr;

	/* Allocate a MR */
	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		goto free_dma_addr;
	fence->mr = mr;
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_PMR;
	mr->qplib_mr.flags = __from_ib_access_flags(mr_access_flags);
	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr)) {
		rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Failed to alloc fence-HW-MR\n");
			goto free_mr;
		}
		/* Register MR */
		mr->ib_mr.lkey = mr->qplib_mr.lkey;
	}
	mr->qplib_mr.va         = (u64)fence->va;
	mr->qplib_mr.total_size = BNXT_RE_LEGACY_FENCE_BYTES;
	pbl_tbl = dma_addr;

	mrinfo.mrw = &mr->qplib_mr;
	mrinfo.ptes = &pbl_tbl;
	mrinfo.sg.npages = BNXT_RE_LEGACY_FENCE_PBL_SIZE;

	mrinfo.sg.nmap = 0;
	mrinfo.sg.sghead = 0;
	mrinfo.sg.pgshft = PAGE_SHIFT;
	mrinfo.sg.pgsize = PAGE_SIZE;
	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to register fence-MR\n");
		goto free_mr;
	}
	mr->ib_mr.lkey = mr->qplib_mr.lkey;
	mr->ib_mr.rkey = mr->qplib_mr.rkey;
	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > (atomic_read(&rdev->stats.rsors.max_mr_count)))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);

	ib_mw = bnxt_re_alloc_mw(&pd->ibpd, IB_MW_TYPE_1, NULL);
	/* Create a fence MW only for kernel consumers */
	if (!ib_mw) {
		dev_err(rdev_to_dev(rdev),
			"Failed to create fence-MW for PD: %p\n", pd);
		rc = -EINVAL;
		goto free_mr;
	}
	fence->mw = ib_mw;

	bnxt_re_legacy_create_fence_wqe(pd);
	return 0;

free_mr:
	if (mr->ib_mr.lkey) {
		bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr);
		atomic_dec(&rdev->stats.rsors.mr_count);
	}
	kfree(mr);
	fence->mr = NULL;

free_dma_addr:
	ib_dma_unmap_single(&rdev->ibdev, fence->dma_addr,
			    BNXT_RE_LEGACY_FENCE_BYTES, DMA_BIDIRECTIONAL);
	fence->dma_addr = 0;

free_va:
	kfree(fence->va);
	fence->va = NULL;
	return rc;
}

static void bnxt_re_legacy_destroy_fence_mr(struct bnxt_re_pd *pd)
{
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_mr *mr = fence->mr;

	if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
		return;

	if (fence->mw) {
		bnxt_re_dealloc_mw(fence->mw);
		fence->mw = NULL;
	}
	if (mr) {
		if (mr->ib_mr.rkey)
			bnxt_qplib_dereg_mrw(&rdev->qplib_res, &mr->qplib_mr,
					     false);
		if (mr->ib_mr.lkey)
			bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr);
		kfree(mr);
		fence->mr = NULL;
		atomic_dec(&rdev->stats.rsors.mr_count);
	}
	if (fence->dma_addr) {
		ib_dma_unmap_single(&rdev->ibdev, fence->dma_addr,
				    BNXT_RE_LEGACY_FENCE_BYTES,
				    DMA_BIDIRECTIONAL);
		fence->dma_addr = 0;
	}
	kfree(fence->va);
	fence->va = NULL;
}


static int bnxt_re_get_user_dpi(struct bnxt_re_dev *rdev,
				struct bnxt_re_ucontext *cntx)
{
	struct bnxt_qplib_chip_ctx *cctx = rdev->chip_ctx;
	int ret = 0;
	u8 type;
	/* Allocate DPI in alloc_pd or in create_cq to avoid failing of
	 * ibv_devinfo and family of application when DPIs are depleted.
	 */
	type = BNXT_QPLIB_DPI_TYPE_UC;
	ret = bnxt_qplib_alloc_dpi(&rdev->qplib_res, &cntx->dpi, cntx, type);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "Alloc doorbell page failed!\n");
		goto out;
	}

	if (cctx->modes.db_push) {
		type = BNXT_QPLIB_DPI_TYPE_WC;
		ret = bnxt_qplib_alloc_dpi(&rdev->qplib_res, &cntx->wcdpi,
					   cntx, type);
		if (ret)
			dev_err(rdev_to_dev(rdev), "push dp alloc failed\n");
	}
out:
	return ret;
}

/* Protection Domains */
void bnxt_re_dealloc_pd(struct ib_pd *ib_pd, struct ib_udata *udata)
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	struct bnxt_re_dev *rdev = pd->rdev;
	int rc;

	bnxt_re_legacy_destroy_fence_mr(pd);

	rc = bnxt_qplib_dealloc_pd(&rdev->qplib_res,
				   &rdev->qplib_res.pd_tbl,
				   &pd->qplib_pd);
	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev),
				    "%s failed rc = %d\n", __func__, rc);
	atomic_dec(&rdev->stats.rsors.pd_count);

	return;
}

int bnxt_re_alloc_pd(struct ib_pd *pd_in,
		     struct ib_udata *udata)
{
	struct ib_pd *ibpd = pd_in;
	struct ib_device *ibdev = ibpd->device;
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_re_ucontext *ucntx =
		rdma_udata_to_drv_context(udata, struct bnxt_re_ucontext,
					  ibucontext);
	u32 max_pd_count;
	int rc;
	struct bnxt_re_pd *pd = container_of(ibpd, struct bnxt_re_pd, ibpd);

	pd->rdev = rdev;
	if (bnxt_qplib_alloc_pd(&rdev->qplib_res, &pd->qplib_pd)) {
		dev_err(rdev_to_dev(rdev),
			"Allocate HW Protection Domain failed!\n");
		rc = -ENOMEM;
		goto fail;
	}

	if (udata) {
		struct bnxt_re_pd_resp resp = {};

		if (!ucntx->dpi.dbr) {
			rc = bnxt_re_get_user_dpi(rdev, ucntx);
			if (rc)
				goto dbfail;
		}

		resp.pdid = pd->qplib_pd.id;
		/* Still allow mapping this DBR to the new user PD. */
		resp.dpi = ucntx->dpi.dpi;
		resp.dbr = (u64)ucntx->dpi.umdbr;
		/* Copy only on a valid wcpdi */
		if (ucntx->wcdpi.dpi) {
			resp.wcdpi = ucntx->wcdpi.dpi;
			resp.comp_mask = BNXT_RE_COMP_MASK_PD_HAS_WC_DPI;
		}
		if (rdev->dbr_pacing) {
			WARN_ON(!rdev->dbr_bar_addr);
			resp.dbr_bar_addr = (u64)rdev->dbr_bar_addr;
			resp.comp_mask |= BNXT_RE_COMP_MASK_PD_HAS_DBR_BAR_ADDR;
		}

		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc)
			goto dbfail;
	}

	if (!udata)
		if (bnxt_re_legacy_create_fence_mr(pd))
			dev_warn(rdev_to_dev(rdev),
				 "Failed to create Fence-MR\n");

	atomic_inc(&rdev->stats.rsors.pd_count);
	max_pd_count = atomic_read(&rdev->stats.rsors.pd_count);
	if (max_pd_count > atomic_read(&rdev->stats.rsors.max_pd_count))
		atomic_set(&rdev->stats.rsors.max_pd_count, max_pd_count);

	return 0;
dbfail:
	(void)bnxt_qplib_dealloc_pd(&rdev->qplib_res, &rdev->qplib_res.pd_tbl,
				    &pd->qplib_pd);
fail:
	return rc;
}

/* Address Handles */
void bnxt_re_destroy_ah(struct ib_ah *ib_ah, u32 flags)
{
	struct bnxt_re_ah *ah = to_bnxt_re(ib_ah, struct bnxt_re_ah, ibah);
	struct bnxt_re_dev *rdev = ah->rdev;
	int rc = 0;
	bool block = true;

	block = !(flags & RDMA_DESTROY_AH_SLEEPABLE);

	rc = bnxt_qplib_destroy_ah(&rdev->qplib_res, &ah->qplib_ah, block);
	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev),
				   "%s id = %d blocking %d failed rc = %d\n",
				    __func__, ah->qplib_ah.id, block, rc);
	atomic_dec(&rdev->stats.rsors.ah_count);

	return;
}

static u8 _to_bnxt_re_nw_type(enum rdma_network_type ntype)
{
	u8 nw_type;
	switch (ntype) {
		case RDMA_NETWORK_IPV4:
			nw_type = CMDQ_CREATE_AH_TYPE_V2IPV4;
			break;
		case RDMA_NETWORK_IPV6:
			nw_type = CMDQ_CREATE_AH_TYPE_V2IPV6;
			break;
		default:
			nw_type = CMDQ_CREATE_AH_TYPE_V1;
			break;
	}
	return nw_type;
}

static inline int
bnxt_re_get_cached_gid(struct ib_device *dev, u8 port_num, int index,
		       union ib_gid *sgid, struct ib_gid_attr **sgid_attr,
		       struct ib_global_route *grh, struct ib_ah *ah)
{
	int ret = 0;

	ret = ib_get_cached_gid(dev, port_num, index, sgid, *sgid_attr);
	return ret;
}

static inline enum rdma_network_type
bnxt_re_gid_to_network_type(struct ib_gid_attr *sgid_attr,
			    union ib_gid *sgid)
{
	return ib_gid_to_network_type(sgid_attr->gid_type, sgid);
}

static int bnxt_re_get_ah_info(struct bnxt_re_dev *rdev,
			       struct ib_ah_attr *ah_attr,
			       struct bnxt_re_ah_info *ah_info)
{
	struct ib_gid_attr *gattr;
	enum rdma_network_type ib_ntype;
	u8 ntype;
	union ib_gid *gid;
	int rc = 0;

	gid = &ah_info->sgid;
	gattr = &ah_info->sgid_attr;

	rc = bnxt_re_get_cached_gid(&rdev->ibdev, 1, ah_attr->grh.sgid_index,
				    gid, &gattr, &ah_attr->grh, NULL);
	if (rc)
		return rc;

	/* Get vlan tag */
	if (gattr->ndev) {
		if (is_vlan_dev(gattr->ndev))
			ah_info->vlan_tag = vlan_dev_vlan_id(gattr->ndev);
		if_rele(gattr->ndev);
	}

	/* Get network header type for this GID */

	ib_ntype = bnxt_re_gid_to_network_type(gattr, gid);
	ntype = _to_bnxt_re_nw_type(ib_ntype);
	ah_info->nw_type = ntype;

	return rc;
}

static u8 _get_sgid_index(struct bnxt_re_dev *rdev, u8 gindx)
{
	gindx = rdev->gid_map[gindx];
	return gindx;
}

static int bnxt_re_init_dmac(struct bnxt_re_dev *rdev, struct ib_ah_attr *ah_attr,
			     struct bnxt_re_ah_info *ah_info, bool is_user,
			     struct bnxt_re_ah *ah)
{
	int rc = 0;
	u8 *dmac;

	if (is_user && !rdma_is_multicast_addr((struct in6_addr *)
						ah_attr->grh.dgid.raw) &&
	    !rdma_link_local_addr((struct in6_addr *)ah_attr->grh.dgid.raw)) {

		u32 retry_count = BNXT_RE_RESOLVE_RETRY_COUNT_US;
		struct bnxt_re_resolve_dmac_work *resolve_dmac_work;


		resolve_dmac_work = kzalloc(sizeof(*resolve_dmac_work), GFP_ATOMIC);

		resolve_dmac_work->rdev = rdev;
		resolve_dmac_work->ah_attr = ah_attr;
		resolve_dmac_work->ah_info = ah_info;

		atomic_set(&resolve_dmac_work->status_wait, 1);
		INIT_WORK(&resolve_dmac_work->work, bnxt_re_resolve_dmac_task);
		queue_work(rdev->resolve_wq, &resolve_dmac_work->work);

		do {
			rc = atomic_read(&resolve_dmac_work->status_wait) & 0xFF;
			if (!rc)
				break;
			udelay(1);
		} while (--retry_count);
		if (atomic_read(&resolve_dmac_work->status_wait)) {
			INIT_LIST_HEAD(&resolve_dmac_work->list);
			list_add_tail(&resolve_dmac_work->list,
					&rdev->mac_wq_list);
			return -EFAULT;
		}
		kfree(resolve_dmac_work);
	}
	dmac = ROCE_DMAC(ah_attr);
	if (dmac)
		memcpy(ah->qplib_ah.dmac, dmac, ETH_ALEN);
	return rc;
}

int bnxt_re_create_ah(struct ib_ah *ah_in, struct ib_ah_attr *attr,
		      u32 flags, struct ib_udata *udata)
{

	struct ib_ah *ib_ah = ah_in;
	struct ib_pd *ib_pd = ib_ah->pd;
	struct bnxt_re_ah *ah = container_of(ib_ah, struct bnxt_re_ah, ibah);
	struct bnxt_re_pd *pd = container_of(ib_pd, struct bnxt_re_pd, ibpd);
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_ah_info ah_info;
	u32 max_ah_count;
	bool is_user;
	int rc;
	bool block = true;
	struct ib_ah_attr *ah_attr = attr;
	block = !(flags & RDMA_CREATE_AH_SLEEPABLE);

	if (!(ah_attr->ah_flags & IB_AH_GRH))
		dev_err(rdev_to_dev(rdev), "ah_attr->ah_flags GRH is not set\n");

	ah->rdev = rdev;
	ah->qplib_ah.pd = &pd->qplib_pd;
	is_user = ib_pd->uobject ? true : false;

	/* Supply the configuration for the HW */
	memcpy(ah->qplib_ah.dgid.data, ah_attr->grh.dgid.raw,
			sizeof(union ib_gid));
	ah->qplib_ah.sgid_index = _get_sgid_index(rdev, ah_attr->grh.sgid_index);
	if (ah->qplib_ah.sgid_index == 0xFF) {
		dev_err(rdev_to_dev(rdev), "invalid sgid_index!\n");
		rc = -EINVAL;
		goto fail;
	}
	ah->qplib_ah.host_sgid_index = ah_attr->grh.sgid_index;
	ah->qplib_ah.traffic_class = ah_attr->grh.traffic_class;
	ah->qplib_ah.flow_label = ah_attr->grh.flow_label;
	ah->qplib_ah.hop_limit = ah_attr->grh.hop_limit;
	ah->qplib_ah.sl = ah_attr->sl;
	rc = bnxt_re_get_ah_info(rdev, ah_attr, &ah_info);
	if (rc)
		goto fail;
	ah->qplib_ah.nw_type = ah_info.nw_type;

	rc = bnxt_re_init_dmac(rdev, ah_attr, &ah_info, is_user, ah);
	if (rc)
		goto fail;

	rc = bnxt_qplib_create_ah(&rdev->qplib_res, &ah->qplib_ah, block);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Allocate HW Address Handle failed!\n");
		goto fail;
	}

	/* Write AVID to shared page. */
	if (ib_pd->uobject) {
		struct ib_ucontext *ib_uctx = ib_pd->uobject->context;
		struct bnxt_re_ucontext *uctx;
		unsigned long flag;
		u32 *wrptr;

		uctx = to_bnxt_re(ib_uctx, struct bnxt_re_ucontext, ibucontext);
		spin_lock_irqsave(&uctx->sh_lock, flag);
		wrptr = (u32 *)((u8 *)uctx->shpg + BNXT_RE_AVID_OFFT);
		*wrptr = ah->qplib_ah.id;
		wmb(); /* make sure cache is updated. */
		spin_unlock_irqrestore(&uctx->sh_lock, flag);
	}
	atomic_inc(&rdev->stats.rsors.ah_count);
	max_ah_count = atomic_read(&rdev->stats.rsors.ah_count);
	if (max_ah_count > atomic_read(&rdev->stats.rsors.max_ah_count))
		atomic_set(&rdev->stats.rsors.max_ah_count, max_ah_count);

	return 0;
fail:
	return rc;
}

int bnxt_re_modify_ah(struct ib_ah *ib_ah, struct ib_ah_attr *ah_attr)
{
	return 0;
}

int bnxt_re_query_ah(struct ib_ah *ib_ah, struct ib_ah_attr *ah_attr)
{
	struct bnxt_re_ah *ah = to_bnxt_re(ib_ah, struct bnxt_re_ah, ibah);

	memcpy(ah_attr->grh.dgid.raw, ah->qplib_ah.dgid.data,
	       sizeof(union ib_gid));
	ah_attr->grh.sgid_index = ah->qplib_ah.host_sgid_index;
	ah_attr->grh.traffic_class = ah->qplib_ah.traffic_class;
	ah_attr->sl = ah->qplib_ah.sl;
	memcpy(ROCE_DMAC(ah_attr), ah->qplib_ah.dmac, ETH_ALEN);
	ah_attr->ah_flags = IB_AH_GRH;
	ah_attr->port_num = 1;
	ah_attr->static_rate = 0;

	return 0;
}

/* Shared Receive Queues */
void bnxt_re_destroy_srq(struct ib_srq *ib_srq,
			 struct ib_udata *udata)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq, ibsrq);
	struct bnxt_re_dev *rdev = srq->rdev;
	struct bnxt_qplib_srq *qplib_srq = &srq->qplib_srq;
	int rc = 0;


	rc = bnxt_qplib_destroy_srq(&rdev->qplib_res, qplib_srq);
	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev),
				   "%s id = %d failed rc = %d\n",
				    __func__, qplib_srq->id, rc);

	if (srq->umem && !IS_ERR(srq->umem))
		ib_umem_release(srq->umem);

	atomic_dec(&rdev->stats.rsors.srq_count);

	return;
}

static u16 _max_rwqe_sz(int nsge)
{
	return sizeof(struct rq_wqe_hdr) + (nsge * sizeof(struct sq_sge));
}

static u16 bnxt_re_get_rwqe_size(struct bnxt_qplib_qp *qplqp,
				 int rsge, int max)
{
	if (qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC)
		rsge = max;

	return _max_rwqe_sz(rsge);
}

static inline
struct ib_umem *ib_umem_get_compat(struct bnxt_re_dev *rdev,
				   struct ib_ucontext *ucontext,
				   struct ib_udata *udata,
				   unsigned long addr,
				   size_t size, int access, int dmasync)
{
	return ib_umem_get(ucontext, addr, size, access, dmasync);
}

static inline
struct ib_umem *ib_umem_get_flags_compat(struct bnxt_re_dev *rdev,
					 struct ib_ucontext *ucontext,
					 struct ib_udata *udata,
					 unsigned long addr,
					 size_t size, int access, int dmasync)
{
	return ib_umem_get_compat(rdev, ucontext, udata, addr, size,
				  access, 0);
}

static inline size_t ib_umem_num_pages_compat(struct ib_umem *umem)
{
	return ib_umem_num_pages(umem);
}

static int bnxt_re_init_user_srq(struct bnxt_re_dev *rdev,
				 struct bnxt_re_pd *pd,
				 struct bnxt_re_srq *srq,
				 struct ib_udata *udata)
{
	struct bnxt_qplib_sg_info *sginfo;
	struct bnxt_qplib_srq *qplib_srq;
	struct bnxt_re_ucontext *cntx;
	struct ib_ucontext *context;
	struct bnxt_re_srq_req ureq;
	struct ib_umem *umem;
	int rc, bytes = 0;

	context = pd->ibpd.uobject->context;
	cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ibucontext);
	qplib_srq = &srq->qplib_srq;
	sginfo = &qplib_srq->sginfo;

	if (udata->inlen < sizeof(ureq))
		dev_warn(rdev_to_dev(rdev),
			 "Update the library ulen %d klen %d\n",
			 (unsigned int)udata->inlen,
			 (unsigned int)sizeof(ureq));

	rc = ib_copy_from_udata(&ureq, udata,
				min(udata->inlen, sizeof(ureq)));
	if (rc)
		return rc;

	bytes = (qplib_srq->max_wqe * qplib_srq->wqe_size);
	bytes = PAGE_ALIGN(bytes);
	umem = ib_umem_get_compat(rdev, context, udata, ureq.srqva, bytes,
				  IB_ACCESS_LOCAL_WRITE, 1);
	if (IS_ERR(umem)) {
		dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed with %ld\n",
			__func__, PTR_ERR(umem));
		return PTR_ERR(umem);
	}

	srq->umem = umem;
	sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
	sginfo->npages = ib_umem_num_pages_compat(umem);
	qplib_srq->srq_handle = ureq.srq_handle;
	qplib_srq->dpi = &cntx->dpi;
	qplib_srq->is_user = true;

	return 0;
}

int bnxt_re_create_srq(struct ib_srq *srq_in, struct ib_srq_init_attr *srq_init_attr,
		       struct ib_udata *udata)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *cntx = NULL;
	struct ib_ucontext *context;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_pd *pd;
	int rc, entries;
	struct ib_srq *ib_srq = srq_in;
	struct ib_pd *ib_pd = ib_srq->pd;
	struct bnxt_re_srq *srq =
		container_of(ib_srq, struct bnxt_re_srq, ibsrq);
	u32 max_srq_count;

	pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	rdev = pd->rdev;
	dev_attr = rdev->dev_attr;

	if (rdev->mod_exit) {
		dev_dbg(rdev_to_dev(rdev), "%s(): in mod_exit, just return!\n", __func__);
		rc = -EIO;
		goto exit;
	}

	if (srq_init_attr->srq_type != IB_SRQT_BASIC) {
		dev_err(rdev_to_dev(rdev), "SRQ type not supported\n");
		rc = -ENOTSUPP;
		goto exit;
	}

	if (udata) {
		context = pd->ibpd.uobject->context;
		cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ibucontext);
	}

	if (atomic_read(&rdev->stats.rsors.srq_count) >= dev_attr->max_srq) {
		dev_err(rdev_to_dev(rdev), "Create SRQ failed - max exceeded(SRQs)\n");
		rc = -EINVAL;
		goto exit;
	}

	if (srq_init_attr->attr.max_wr >= dev_attr->max_srq_wqes) {
		dev_err(rdev_to_dev(rdev), "Create SRQ failed - max exceeded(SRQ_WQs)\n");
		rc = -EINVAL;
		goto exit;
	}

	srq->rdev = rdev;
	srq->qplib_srq.pd = &pd->qplib_pd;
	srq->qplib_srq.dpi = &rdev->dpi_privileged;

	/* Allocate 1 more than what's provided so posting max doesn't
	   mean empty */
	entries = srq_init_attr->attr.max_wr + 1;
	entries = bnxt_re_init_depth(entries, cntx);
	if (entries > dev_attr->max_srq_wqes + 1)
		entries = dev_attr->max_srq_wqes + 1;

	srq->qplib_srq.wqe_size = _max_rwqe_sz(6); /* 128 byte wqe size */
	srq->qplib_srq.max_wqe = entries;
	srq->qplib_srq.max_sge = srq_init_attr->attr.max_sge;
	srq->qplib_srq.threshold = srq_init_attr->attr.srq_limit;
	srq->srq_limit = srq_init_attr->attr.srq_limit;
	srq->qplib_srq.eventq_hw_ring_id = rdev->nqr.nq[0].ring_id;
	srq->qplib_srq.sginfo.pgsize = PAGE_SIZE;
	srq->qplib_srq.sginfo.pgshft = PAGE_SHIFT;

	if (udata) {
		rc = bnxt_re_init_user_srq(rdev, pd, srq, udata);
		if (rc)
			goto fail;
	}

	rc = bnxt_qplib_create_srq(&rdev->qplib_res, &srq->qplib_srq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Create HW SRQ failed!\n");
		goto fail;
	}

	if (udata) {
		struct bnxt_re_srq_resp resp;

		resp.srqid = srq->qplib_srq.id;
		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc) {
			bnxt_qplib_destroy_srq(&rdev->qplib_res, &srq->qplib_srq);
			goto fail;
		}
	}
	atomic_inc(&rdev->stats.rsors.srq_count);
	max_srq_count = atomic_read(&rdev->stats.rsors.srq_count);
	if (max_srq_count > atomic_read(&rdev->stats.rsors.max_srq_count))
		atomic_set(&rdev->stats.rsors.max_srq_count, max_srq_count);
	spin_lock_init(&srq->lock);

	return 0;
fail:
	if (udata && srq->umem && !IS_ERR(srq->umem)) {
		ib_umem_release(srq->umem);
		srq->umem = NULL;
	}
exit:
	return rc;
}

int bnxt_re_modify_srq(struct ib_srq *ib_srq, struct ib_srq_attr *srq_attr,
		       enum ib_srq_attr_mask srq_attr_mask,
		       struct ib_udata *udata)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq,
					     ibsrq);
	struct bnxt_re_dev *rdev = srq->rdev;
	int rc;

	switch (srq_attr_mask) {
	case IB_SRQ_MAX_WR:
		/* SRQ resize is not supported */
		break;
	case IB_SRQ_LIMIT:
		/* Change the SRQ threshold */
		if (srq_attr->srq_limit > srq->qplib_srq.max_wqe)
			return -EINVAL;

		srq->qplib_srq.threshold = srq_attr->srq_limit;
		rc = bnxt_qplib_modify_srq(&rdev->qplib_res, &srq->qplib_srq);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Modify HW SRQ failed!\n");
			return rc;
		}
		/* On success, update the shadow */
		srq->srq_limit = srq_attr->srq_limit;

		if (udata) {
			/* Build and send response back to udata */
			rc = bnxt_re_copy_to_udata(rdev, srq, 0, udata);
			if (rc)
				return rc;
		}
		break;
	default:
		dev_err(rdev_to_dev(rdev),
			"Unsupported srq_attr_mask 0x%x\n", srq_attr_mask);
		return -EINVAL;
	}
	return 0;
}

int bnxt_re_query_srq(struct ib_srq *ib_srq, struct ib_srq_attr *srq_attr)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq,
					     ibsrq);
	struct bnxt_re_dev *rdev = srq->rdev;
	int rc;

	rc = bnxt_qplib_query_srq(&rdev->qplib_res, &srq->qplib_srq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Query HW SRQ (0x%x) failed! rc = %d\n",
			srq->qplib_srq.id, rc);
		return rc;
	}
	srq_attr->max_wr = srq->qplib_srq.max_wqe;
	srq_attr->max_sge = srq->qplib_srq.max_sge;
	srq_attr->srq_limit = srq->qplib_srq.threshold;

	return 0;
}

int bnxt_re_post_srq_recv(struct ib_srq *ib_srq, const struct ib_recv_wr *wr,
			  const struct ib_recv_wr **bad_wr)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq,
					     ibsrq);
	struct bnxt_qplib_swqe wqe = {};
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&srq->lock, flags);
	while (wr) {
		/* Transcribe each ib_recv_wr to qplib_swqe */
		wqe.num_sge = wr->num_sge;
		wqe.sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
		wqe.wr_id = wr->wr_id;
		wqe.type = BNXT_QPLIB_SWQE_TYPE_RECV;
		rc = bnxt_qplib_post_srq_recv(&srq->qplib_srq, &wqe);
		if (rc) {
			*bad_wr = wr;
			break;
		}
		wr = wr->next;
	}
	spin_unlock_irqrestore(&srq->lock, flags);

	return rc;
}

unsigned long bnxt_re_lock_cqs(struct bnxt_re_qp *qp)
{
	unsigned long flags;

	spin_lock_irqsave(&qp->scq->cq_lock, flags);
	if (qp->rcq && qp->rcq != qp->scq)
		spin_lock(&qp->rcq->cq_lock);

	return flags;
}

void bnxt_re_unlock_cqs(struct bnxt_re_qp *qp,
				  unsigned long flags)
{
	if (qp->rcq && qp->rcq != qp->scq)
		spin_unlock(&qp->rcq->cq_lock);
	spin_unlock_irqrestore(&qp->scq->cq_lock, flags);
}

/* Queue Pairs */
static int bnxt_re_destroy_gsi_sqp(struct bnxt_re_qp *qp)
{
	struct bnxt_re_qp *gsi_sqp;
	struct bnxt_re_ah *gsi_sah;
	struct bnxt_re_dev *rdev;
	unsigned long flags;
	int rc = 0;

	rdev = qp->rdev;
	gsi_sqp = rdev->gsi_ctx.gsi_sqp;
	gsi_sah = rdev->gsi_ctx.gsi_sah;

	/* remove from active qp list */
	mutex_lock(&rdev->qp_lock);
	list_del(&gsi_sqp->list);
	mutex_unlock(&rdev->qp_lock);

	if (gsi_sah) {
		dev_dbg(rdev_to_dev(rdev), "Destroy the shadow AH\n");
		rc = bnxt_qplib_destroy_ah(&rdev->qplib_res, &gsi_sah->qplib_ah,
					   true);
		if (rc)
			dev_err(rdev_to_dev(rdev),
				"Destroy HW AH for shadow QP failed!\n");
		atomic_dec(&rdev->stats.rsors.ah_count);
	}

	dev_dbg(rdev_to_dev(rdev), "Destroy the shadow QP\n");
	rc = bnxt_qplib_destroy_qp(&rdev->qplib_res, &gsi_sqp->qplib_qp);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Destroy Shadow QP failed\n");

	/* Clean the CQ for shadow QP completions */
	flags = bnxt_re_lock_cqs(gsi_sqp);
	bnxt_qplib_clean_qp(&gsi_sqp->qplib_qp);
	bnxt_re_unlock_cqs(gsi_sqp, flags);

	bnxt_qplib_free_qp_res(&rdev->qplib_res, &gsi_sqp->qplib_qp);
	bnxt_qplib_free_hdr_buf(&rdev->qplib_res, &gsi_sqp->qplib_qp);
	kfree(rdev->gsi_ctx.sqp_tbl);
	kfree(gsi_sah);
	kfree(gsi_sqp);
	rdev->gsi_ctx.gsi_sqp = NULL;
	rdev->gsi_ctx.gsi_sah = NULL;
	rdev->gsi_ctx.sqp_tbl = NULL;
	atomic_dec(&rdev->stats.rsors.qp_count);

	return 0;
}

static void bnxt_re_dump_debug_stats(struct bnxt_re_dev *rdev, u32 active_qps)
{
	u32	total_qp = 0;
	u64	avg_time = 0;
	int	i;

	if (!rdev->rcfw.sp_perf_stats_enabled)
		return;

	switch (active_qps) {
	case 1:
		/* Potential hint for Test Stop */
		for (i = 0; i < RCFW_MAX_STAT_INDEX; i++) {
			if (rdev->rcfw.qp_destroy_stats[i]) {
				total_qp++;
				avg_time += rdev->rcfw.qp_destroy_stats[i];
			}
		}
		if (total_qp >= 0 || avg_time >= 0)
			dev_dbg(rdev_to_dev(rdev),
				"Perf Debug: %ps Total (%d) QP destroyed in (%ld) msec\n",
				__builtin_return_address(0), total_qp,
				(long)jiffies_to_msecs(avg_time));
		break;
	case 2:
		/* Potential hint for Test Start */
		dev_dbg(rdev_to_dev(rdev),
			"Perf Debug: %ps active_qps = %d\n",
			__builtin_return_address(0), active_qps);
		break;
	default:
		/* Potential hint to know latency of QP destroy.
		 * Average time taken for 1K QP Destroy.
		 */
		if (active_qps > 1024 && !(active_qps % 1024))
			dev_dbg(rdev_to_dev(rdev),
				"Perf Debug: %ps Active QP (%d) Watermark (%d)\n",
				__builtin_return_address(0), active_qps,
				atomic_read(&rdev->stats.rsors.max_qp_count));
		break;
	}
}

int bnxt_re_destroy_qp(struct ib_qp *ib_qp, struct ib_udata *udata)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_re_dev *rdev = qp->rdev;
	unsigned long flags;
	u32 active_qps;
	int rc;

	mutex_lock(&rdev->qp_lock);
	list_del(&qp->list);
	active_qps = atomic_dec_return(&rdev->stats.rsors.qp_count);
	if (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_RC)
		atomic_dec(&rdev->stats.rsors.rc_qp_count);
	else if (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_UD)
		atomic_dec(&rdev->stats.rsors.ud_qp_count);
	mutex_unlock(&rdev->qp_lock);

	rc = bnxt_qplib_destroy_qp(&rdev->qplib_res, &qp->qplib_qp);
	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev),
				   "%s id = %d failed rc = %d\n",
				    __func__, qp->qplib_qp.id, rc);

	if (!ib_qp->uobject) {
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_clean_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);
	}

	bnxt_qplib_free_qp_res(&rdev->qplib_res, &qp->qplib_qp);
	if (ib_qp->qp_type == IB_QPT_GSI &&
	    rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
		if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL &&
		    rdev->gsi_ctx.gsi_sqp) {
			bnxt_re_destroy_gsi_sqp(qp);
		}
		bnxt_qplib_free_hdr_buf(&rdev->qplib_res, &qp->qplib_qp);
	}

	if (qp->rumem && !IS_ERR(qp->rumem))
		ib_umem_release(qp->rumem);
	if (qp->sumem && !IS_ERR(qp->sumem))
		ib_umem_release(qp->sumem);
	kfree(qp);

	bnxt_re_dump_debug_stats(rdev, active_qps);

	return 0;
}

static u8 __from_ib_qp_type(enum ib_qp_type type)
{
	switch (type) {
	case IB_QPT_GSI:
		return CMDQ_CREATE_QP1_TYPE_GSI;
	case IB_QPT_RC:
		return CMDQ_CREATE_QP_TYPE_RC;
	case IB_QPT_UD:
		return CMDQ_CREATE_QP_TYPE_UD;
	case IB_QPT_RAW_ETHERTYPE:
		return CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE;
	default:
		return IB_QPT_MAX;
	}
}

static u16 _get_swqe_sz(int nsge)
{
	return sizeof(struct sq_send_hdr) + nsge * sizeof(struct sq_sge);
}

static int bnxt_re_get_swqe_size(int ilsize, int nsge)
{
	u16 wqe_size, calc_ils;

	wqe_size = _get_swqe_sz(nsge);
	if (ilsize) {
		calc_ils = (sizeof(struct sq_send_hdr) + ilsize);
		wqe_size = max_t(int, calc_ils, wqe_size);
		wqe_size = ALIGN(wqe_size, 32);
	}
	return wqe_size;
}

static int bnxt_re_setup_swqe_size(struct bnxt_re_qp *qp,
				   struct ib_qp_init_attr *init_attr)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_q *sq;
	int align, ilsize;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	sq = &qplqp->sq;
	dev_attr = rdev->dev_attr;

	align = sizeof(struct sq_send_hdr);
	ilsize = ALIGN(init_attr->cap.max_inline_data, align);

	sq->wqe_size = bnxt_re_get_swqe_size(ilsize, sq->max_sge);
	if (sq->wqe_size > _get_swqe_sz(dev_attr->max_qp_sges))
		return -EINVAL;
	/* For Cu/Wh and gen p5 backward compatibility mode
	 * wqe size is fixed to 128 bytes
	 */
	if (sq->wqe_size < _get_swqe_sz(dev_attr->max_qp_sges) &&
	    qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC)
		sq->wqe_size = _get_swqe_sz(dev_attr->max_qp_sges);

	if (init_attr->cap.max_inline_data) {
		qplqp->max_inline_data = sq->wqe_size -
					 sizeof(struct sq_send_hdr);
		init_attr->cap.max_inline_data = qplqp->max_inline_data;
		if (qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC)
			sq->max_sge = qplqp->max_inline_data /
				      sizeof(struct sq_sge);
	}

	return 0;
}

static int bnxt_re_init_user_qp(struct bnxt_re_dev *rdev,
				struct bnxt_re_pd *pd, struct bnxt_re_qp *qp,
				struct ib_udata *udata)
{
	struct bnxt_qplib_sg_info *sginfo;
	struct bnxt_qplib_qp *qplib_qp;
	struct bnxt_re_ucontext *cntx;
	struct ib_ucontext *context;
	struct bnxt_re_qp_req ureq;
	struct ib_umem *umem;
	int rc, bytes = 0;
	int psn_nume;
	int psn_sz;

	qplib_qp = &qp->qplib_qp;
	context = pd->ibpd.uobject->context;
	cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ibucontext);
	sginfo = &qplib_qp->sq.sginfo;

	if (udata->inlen < sizeof(ureq))
		dev_warn(rdev_to_dev(rdev),
			 "Update the library ulen %d klen %d\n",
			 (unsigned int)udata->inlen,
			 (unsigned int)sizeof(ureq));

	rc = ib_copy_from_udata(&ureq, udata,
				min(udata->inlen, sizeof(ureq)));
	if (rc)
		return rc;

	bytes = (qplib_qp->sq.max_wqe * qplib_qp->sq.wqe_size);
	/* Consider mapping PSN search memory only for RC QPs. */
	if (qplib_qp->type == CMDQ_CREATE_QP_TYPE_RC) {
		psn_sz = _is_chip_gen_p5_p7(rdev->chip_ctx) ?
				sizeof(struct sq_psn_search_ext) :
				sizeof(struct sq_psn_search);
		if (rdev->dev_attr && BNXT_RE_HW_RETX(rdev->dev_attr->dev_cap_flags))
			psn_sz = sizeof(struct sq_msn_search);
		psn_nume = (qplib_qp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
			    qplib_qp->sq.max_wqe :
			    ((qplib_qp->sq.max_wqe * qplib_qp->sq.wqe_size) /
			     sizeof(struct bnxt_qplib_sge));
		if (BNXT_RE_HW_RETX(rdev->dev_attr->dev_cap_flags))
			psn_nume = roundup_pow_of_two(psn_nume);

		bytes += (psn_nume * psn_sz);
	}
	bytes = PAGE_ALIGN(bytes);
	umem = ib_umem_get_compat(rdev, context, udata, ureq.qpsva, bytes,
				  IB_ACCESS_LOCAL_WRITE, 1);
	if (IS_ERR(umem)) {
		dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed with %ld\n",
			__func__, PTR_ERR(umem));
		return PTR_ERR(umem);
	}

	qp->sumem = umem;
	/* pgsize and pgshft were initialize already. */
	sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
	sginfo->npages = ib_umem_num_pages_compat(umem);
	qplib_qp->qp_handle = ureq.qp_handle;

	if (!qp->qplib_qp.srq) {
		sginfo = &qplib_qp->rq.sginfo;
		bytes = (qplib_qp->rq.max_wqe * qplib_qp->rq.wqe_size);
		bytes = PAGE_ALIGN(bytes);
		umem = ib_umem_get_compat(rdev,
					  context, udata, ureq.qprva, bytes,
					  IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(umem)) {
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed ret =%ld\n",
				__func__, PTR_ERR(umem));
			goto rqfail;
		}
		qp->rumem = umem;
		/* pgsize and pgshft were initialize already. */
		sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
		sginfo->npages = ib_umem_num_pages_compat(umem);
	}

	qplib_qp->dpi = &cntx->dpi;
	qplib_qp->is_user = true;

	return 0;
rqfail:
	ib_umem_release(qp->sumem);
	qp->sumem = NULL;
	qplib_qp->sq.sginfo.sghead = NULL;
	qplib_qp->sq.sginfo.nmap = 0;

	return PTR_ERR(umem);
}

static struct bnxt_re_ah *bnxt_re_create_shadow_qp_ah(struct bnxt_re_pd *pd,
					       struct bnxt_qplib_res *qp1_res,
					       struct bnxt_qplib_qp *qp1_qp)
{
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_ah *ah;
	union ib_gid sgid;
	int rc;

	ah = kzalloc(sizeof(*ah), GFP_KERNEL);
	if (!ah) {
		dev_err(rdev_to_dev(rdev), "Allocate Address Handle failed!\n");
		return NULL;
	}
	memset(ah, 0, sizeof(*ah));
	ah->rdev = rdev;
	ah->qplib_ah.pd = &pd->qplib_pd;

	rc = bnxt_re_query_gid(&rdev->ibdev, 1, 0, &sgid);
	if (rc)
		goto fail;

	/* supply the dgid data same as sgid */
	memcpy(ah->qplib_ah.dgid.data, &sgid.raw,
	       sizeof(union ib_gid));
	ah->qplib_ah.sgid_index = 0;

	ah->qplib_ah.traffic_class = 0;
	ah->qplib_ah.flow_label = 0;
	ah->qplib_ah.hop_limit = 1;
	ah->qplib_ah.sl = 0;
	/* Have DMAC same as SMAC */
	ether_addr_copy(ah->qplib_ah.dmac, rdev->dev_addr);
	dev_dbg(rdev_to_dev(rdev), "ah->qplib_ah.dmac = %x:%x:%x:%x:%x:%x\n",
		ah->qplib_ah.dmac[0], ah->qplib_ah.dmac[1], ah->qplib_ah.dmac[2],
		ah->qplib_ah.dmac[3], ah->qplib_ah.dmac[4], ah->qplib_ah.dmac[5]);

	rc = bnxt_qplib_create_ah(&rdev->qplib_res, &ah->qplib_ah, true);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Allocate HW AH for Shadow QP failed!\n");
		goto fail;
	}
	dev_dbg(rdev_to_dev(rdev), "AH ID = %d\n", ah->qplib_ah.id);
	atomic_inc(&rdev->stats.rsors.ah_count);

	return ah;
fail:
	kfree(ah);
	return NULL;
}

void bnxt_re_update_shadow_ah(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *gsi_qp;
	struct bnxt_re_ah *sah;
	struct bnxt_re_pd *pd;
	struct ib_pd *ib_pd;
	int rc;

	if (!rdev)
		return;

	sah = rdev->gsi_ctx.gsi_sah;

	dev_dbg(rdev_to_dev(rdev), "Updating the AH\n");
	if (sah) {
		/* Check if the AH created with current mac address */
		if (!compare_ether_header(sah->qplib_ah.dmac, rdev->dev_addr)) {
			dev_dbg(rdev_to_dev(rdev),
				"Not modifying shadow AH during AH update\n");
			return;
		}

		gsi_qp = rdev->gsi_ctx.gsi_qp;
		ib_pd = gsi_qp->ib_qp.pd;
		pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
		rc = bnxt_qplib_destroy_ah(&rdev->qplib_res,
					   &sah->qplib_ah, false);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to destroy shadow AH during AH update\n");
			return;
		}
		atomic_dec(&rdev->stats.rsors.ah_count);
		kfree(sah);
		rdev->gsi_ctx.gsi_sah = NULL;

		sah = bnxt_re_create_shadow_qp_ah(pd, &rdev->qplib_res,
						  &gsi_qp->qplib_qp);
		if (!sah) {
			dev_err(rdev_to_dev(rdev),
				"Failed to update AH for ShadowQP\n");
			return;
		}
		rdev->gsi_ctx.gsi_sah = sah;
		atomic_inc(&rdev->stats.rsors.ah_count);
	}
}

static struct bnxt_re_qp *bnxt_re_create_shadow_qp(struct bnxt_re_pd *pd,
					    struct bnxt_qplib_res *qp1_res,
					    struct bnxt_qplib_qp *qp1_qp)
{
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_qp *qp;
	int rc;

	qp = kzalloc(sizeof(*qp), GFP_KERNEL);
	if (!qp) {
		dev_err(rdev_to_dev(rdev), "Allocate internal UD QP failed!\n");
		return NULL;
	}
	memset(qp, 0, sizeof(*qp));
	qp->rdev = rdev;

	/* Initialize the shadow QP structure from the QP1 values */
	ether_addr_copy(qp->qplib_qp.smac, rdev->dev_addr);
	qp->qplib_qp.pd = &pd->qplib_pd;
	qp->qplib_qp.qp_handle = (u64)&qp->qplib_qp;
	qp->qplib_qp.type = IB_QPT_UD;

	qp->qplib_qp.max_inline_data = 0;
	qp->qplib_qp.sig_type = true;

	/* Shadow QP SQ depth should be same as QP1 RQ depth */
	qp->qplib_qp.sq.wqe_size = bnxt_re_get_swqe_size(0, 6);
	qp->qplib_qp.sq.max_wqe = qp1_qp->rq.max_wqe;
	qp->qplib_qp.sq.max_sge = 2;
	/* Q full delta can be 1 since it is internal QP */
	qp->qplib_qp.sq.q_full_delta = 1;
	qp->qplib_qp.sq.sginfo.pgsize = PAGE_SIZE;
	qp->qplib_qp.sq.sginfo.pgshft = PAGE_SHIFT;

	qp->qplib_qp.scq = qp1_qp->scq;
	qp->qplib_qp.rcq = qp1_qp->rcq;

	qp->qplib_qp.rq.wqe_size = _max_rwqe_sz(6); /* 128 Byte wqe size */
	qp->qplib_qp.rq.max_wqe = qp1_qp->rq.max_wqe;
	qp->qplib_qp.rq.max_sge = qp1_qp->rq.max_sge;
	qp->qplib_qp.rq.sginfo.pgsize = PAGE_SIZE;
	qp->qplib_qp.rq.sginfo.pgshft = PAGE_SHIFT;
	/* Q full delta can be 1 since it is internal QP */
	qp->qplib_qp.rq.q_full_delta = 1;
	qp->qplib_qp.mtu = qp1_qp->mtu;
	qp->qplib_qp.dpi = &rdev->dpi_privileged;

	rc = bnxt_qplib_alloc_hdr_buf(qp1_res, &qp->qplib_qp, 0,
				      BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV6);
	if (rc)
		goto fail;

	rc = bnxt_qplib_create_qp(qp1_res, &qp->qplib_qp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "create HW QP failed!\n");
		goto qp_fail;
	}

	dev_dbg(rdev_to_dev(rdev), "Created shadow QP with ID = %d\n",
		qp->qplib_qp.id);
	spin_lock_init(&qp->sq_lock);
	INIT_LIST_HEAD(&qp->list);
	mutex_lock(&rdev->qp_lock);
	list_add_tail(&qp->list, &rdev->qp_list);
	atomic_inc(&rdev->stats.rsors.qp_count);
	mutex_unlock(&rdev->qp_lock);
	return qp;
qp_fail:
	bnxt_qplib_free_hdr_buf(qp1_res, &qp->qplib_qp);
fail:
	kfree(qp);
	return NULL;
}

static int bnxt_re_init_rq_attr(struct bnxt_re_qp *qp,
				struct ib_qp_init_attr *init_attr, void *cntx)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_q *rq;
	int entries;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	rq = &qplqp->rq;
	dev_attr = rdev->dev_attr;

	if (init_attr->srq) {
		struct bnxt_re_srq *srq;

		srq = to_bnxt_re(init_attr->srq, struct bnxt_re_srq, ibsrq);
		if (!srq) {
			dev_err(rdev_to_dev(rdev), "SRQ not found\n");
			return -EINVAL;
		}
		qplqp->srq = &srq->qplib_srq;
		rq->max_wqe = 0;
	} else {
		rq->max_sge = init_attr->cap.max_recv_sge;
		if (rq->max_sge > dev_attr->max_qp_sges)
			rq->max_sge = dev_attr->max_qp_sges;
		init_attr->cap.max_recv_sge = rq->max_sge;
		rq->wqe_size = bnxt_re_get_rwqe_size(qplqp, rq->max_sge,
						     dev_attr->max_qp_sges);

		/* Allocate 1 more than what's provided so posting max doesn't
		   mean empty */
		entries = init_attr->cap.max_recv_wr + 1;
		entries = bnxt_re_init_depth(entries, cntx);
		rq->max_wqe = min_t(u32, entries, dev_attr->max_qp_wqes + 1);
		rq->q_full_delta = 0;
		rq->sginfo.pgsize = PAGE_SIZE;
		rq->sginfo.pgshft = PAGE_SHIFT;
	}

	return 0;
}

static void bnxt_re_adjust_gsi_rq_attr(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	dev_attr = rdev->dev_attr;

	if (rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD)
		qplqp->rq.max_sge = dev_attr->max_qp_sges;
}

static int bnxt_re_init_sq_attr(struct bnxt_re_qp *qp,
				struct ib_qp_init_attr *init_attr,
				void *cntx)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_q *sq;
	int diff = 0;
	int entries;
	int rc;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	sq = &qplqp->sq;
	dev_attr = rdev->dev_attr;

	sq->max_sge = init_attr->cap.max_send_sge;
	if (sq->max_sge > dev_attr->max_qp_sges) {
		sq->max_sge = dev_attr->max_qp_sges;
		init_attr->cap.max_send_sge = sq->max_sge;
	}
	rc = bnxt_re_setup_swqe_size(qp, init_attr);
	if (rc)
		return rc;
	/*
	 * Change the SQ depth if user has requested minimum using
	 * configfs. Only supported for kernel consumers. Setting
	 * min_tx_depth to 4096 to handle iser SQ full condition
	 * in most of the newer OS distros
	 */
	entries = init_attr->cap.max_send_wr;
	if (!cntx && rdev->min_tx_depth && init_attr->qp_type != IB_QPT_GSI) {
		/*
		 * If users specify any value greater than 1 use min_tx_depth
		 * provided by user for comparison. Else, compare it with the
		 * BNXT_RE_MIN_KERNEL_QP_TX_DEPTH and adjust it accordingly.
		 */
		if (rdev->min_tx_depth > 1 && entries < rdev->min_tx_depth)
			entries = rdev->min_tx_depth;
		else if (entries < BNXT_RE_MIN_KERNEL_QP_TX_DEPTH)
			entries = BNXT_RE_MIN_KERNEL_QP_TX_DEPTH;
	}
	diff = bnxt_re_get_diff(cntx, rdev->chip_ctx);
	entries = bnxt_re_init_depth(entries + diff + 1, cntx);
	sq->max_wqe = min_t(u32, entries, dev_attr->max_qp_wqes + diff + 1);
	sq->q_full_delta = diff + 1;
	/*
	 * Reserving one slot for Phantom WQE. Application can
	 * post one extra entry in this case. But allowing this to avoid
	 * unexpected Queue full condition
	 */
	sq->q_full_delta -= 1; /* becomes 0 for gen-p5 */
	sq->sginfo.pgsize = PAGE_SIZE;
	sq->sginfo.pgshft = PAGE_SHIFT;
	return 0;
}

static void bnxt_re_adjust_gsi_sq_attr(struct bnxt_re_qp *qp,
				       struct ib_qp_init_attr *init_attr,
				       void *cntx)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	int entries;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	dev_attr = rdev->dev_attr;

	if (rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
		entries = init_attr->cap.max_send_wr + 1;
		entries = bnxt_re_init_depth(entries, cntx);
		qplqp->sq.max_wqe = min_t(u32, entries,
					  dev_attr->max_qp_wqes + 1);
		qplqp->sq.q_full_delta = qplqp->sq.max_wqe -
					 init_attr->cap.max_send_wr;
		qplqp->sq.max_sge++; /* Need one extra sge to put UD header */
		if (qplqp->sq.max_sge > dev_attr->max_qp_sges)
			qplqp->sq.max_sge = dev_attr->max_qp_sges;
	}
}

static int bnxt_re_init_qp_type(struct bnxt_re_dev *rdev,
				struct ib_qp_init_attr *init_attr)
{
	struct bnxt_qplib_chip_ctx *chip_ctx;
	struct bnxt_re_gsi_context *gsi_ctx;
	int qptype;

	chip_ctx = rdev->chip_ctx;
	gsi_ctx = &rdev->gsi_ctx;

	qptype = __from_ib_qp_type(init_attr->qp_type);
	if (qptype == IB_QPT_MAX) {
		dev_err(rdev_to_dev(rdev), "QP type 0x%x not supported\n",
			qptype);
		qptype = -EINVAL;
		goto out;
	}

	if (_is_chip_gen_p5_p7(chip_ctx) && init_attr->qp_type == IB_QPT_GSI) {
		/* For Thor always force UD mode. */
		qptype = CMDQ_CREATE_QP_TYPE_GSI;
		gsi_ctx->gsi_qp_mode = BNXT_RE_GSI_MODE_UD;
	}
out:
	return qptype;
}

static int bnxt_re_init_qp_wqe_mode(struct bnxt_re_dev *rdev)
{
	return rdev->chip_ctx->modes.wqe_mode;
}

static int bnxt_re_init_qp_attr(struct bnxt_re_qp *qp, struct bnxt_re_pd *pd,
				struct ib_qp_init_attr *init_attr,
				struct ib_udata *udata)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *cntx = NULL;
	struct ib_ucontext *context;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_cq *cq;
	int rc = 0, qptype;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	dev_attr = rdev->dev_attr;

	if (udata) {
		context = pd->ibpd.uobject->context;
		cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ibucontext);
	}

	/* Setup misc params */
	qplqp->is_user = false;
	qplqp->pd = &pd->qplib_pd;
	qplqp->qp_handle = (u64)qplqp;
	qplqp->sig_type = ((init_attr->sq_sig_type == IB_SIGNAL_ALL_WR) ?
			    true : false);
	qptype = bnxt_re_init_qp_type(rdev, init_attr);
	if (qptype < 0) {
		rc = qptype;
		goto out;
	}
	qplqp->type = (u8)qptype;
	qplqp->wqe_mode = bnxt_re_init_qp_wqe_mode(rdev);
	ether_addr_copy(qplqp->smac, rdev->dev_addr);

	if (init_attr->qp_type == IB_QPT_RC) {
		qplqp->max_rd_atomic = dev_attr->max_qp_rd_atom;
		qplqp->max_dest_rd_atomic = dev_attr->max_qp_init_rd_atom;
	}
	qplqp->mtu = ib_mtu_enum_to_int(iboe_get_mtu(if_getmtu(rdev->netdev)));
	qplqp->dpi = &rdev->dpi_privileged; /* Doorbell page */
	if (init_attr->create_flags) {
		dev_dbg(rdev_to_dev(rdev),
			"QP create flags 0x%x not supported\n",
			init_attr->create_flags);
		return -EOPNOTSUPP;
	}

	/* Setup CQs */
	if (init_attr->send_cq) {
		cq = to_bnxt_re(init_attr->send_cq, struct bnxt_re_cq, ibcq);
		if (!cq) {
			dev_err(rdev_to_dev(rdev), "Send CQ not found\n");
			rc = -EINVAL;
			goto out;
		}
		qplqp->scq = &cq->qplib_cq;
		qp->scq = cq;
	}

	if (init_attr->recv_cq) {
		cq = to_bnxt_re(init_attr->recv_cq, struct bnxt_re_cq, ibcq);
		if (!cq) {
			dev_err(rdev_to_dev(rdev), "Receive CQ not found\n");
			rc = -EINVAL;
			goto out;
		}
		qplqp->rcq = &cq->qplib_cq;
		qp->rcq = cq;
	}

	/* Setup RQ/SRQ */
	rc = bnxt_re_init_rq_attr(qp, init_attr, cntx);
	if (rc)
		goto out;
	if (init_attr->qp_type == IB_QPT_GSI)
		bnxt_re_adjust_gsi_rq_attr(qp);

	/* Setup SQ */
	rc = bnxt_re_init_sq_attr(qp, init_attr, cntx);
	if (rc)
		goto out;
	if (init_attr->qp_type == IB_QPT_GSI)
		bnxt_re_adjust_gsi_sq_attr(qp, init_attr, cntx);

	if (udata) /* This will update DPI and qp_handle */
		rc = bnxt_re_init_user_qp(rdev, pd, qp, udata);
out:
	return rc;
}

static int bnxt_re_create_shadow_gsi(struct bnxt_re_qp *qp,
				     struct bnxt_re_pd *pd)
{
	struct bnxt_re_sqp_entries *sqp_tbl = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_qp *sqp;
	struct bnxt_re_ah *sah;
	int rc = 0;

	rdev = qp->rdev;
	/* Create a shadow QP to handle the QP1 traffic */
	sqp_tbl = kzalloc(sizeof(*sqp_tbl) * BNXT_RE_MAX_GSI_SQP_ENTRIES,
			  GFP_KERNEL);
	if (!sqp_tbl)
		return -ENOMEM;
	rdev->gsi_ctx.sqp_tbl = sqp_tbl;

	sqp = bnxt_re_create_shadow_qp(pd, &rdev->qplib_res, &qp->qplib_qp);
	if (!sqp) {
		rc = -ENODEV;
		dev_err(rdev_to_dev(rdev),
			"Failed to create Shadow QP for QP1\n");
		goto out;
	}
	rdev->gsi_ctx.gsi_sqp = sqp;

	sqp->rcq = qp->rcq;
	sqp->scq = qp->scq;
	sah = bnxt_re_create_shadow_qp_ah(pd, &rdev->qplib_res,
			&qp->qplib_qp);
	if (!sah) {
		bnxt_qplib_destroy_qp(&rdev->qplib_res,
				&sqp->qplib_qp);
		rc = -ENODEV;
		dev_err(rdev_to_dev(rdev),
				"Failed to create AH entry for ShadowQP\n");
		goto out;
	}
	rdev->gsi_ctx.gsi_sah = sah;

	return 0;
out:
	kfree(sqp_tbl);
	return rc;
}

static int __get_rq_hdr_buf_size(u8 gsi_mode)
{
	return (gsi_mode == BNXT_RE_GSI_MODE_ALL) ?
		BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE_V2 :
		BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE;
}

static int __get_sq_hdr_buf_size(u8 gsi_mode)
{
	return (gsi_mode != BNXT_RE_GSI_MODE_ROCE_V1) ?
		BNXT_QPLIB_MAX_QP1_SQ_HDR_SIZE_V2 :
		BNXT_QPLIB_MAX_QP1_SQ_HDR_SIZE;
}

static int bnxt_re_create_gsi_qp(struct bnxt_re_qp *qp, struct bnxt_re_pd *pd)
{
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_qplib_res *res;
	struct bnxt_re_dev *rdev;
	u32 sstep, rstep;
	u8 gsi_mode;
	int rc = 0;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	res = &rdev->qplib_res;
	gsi_mode = rdev->gsi_ctx.gsi_qp_mode;

	rstep = __get_rq_hdr_buf_size(gsi_mode);
	sstep = __get_sq_hdr_buf_size(gsi_mode);
	rc = bnxt_qplib_alloc_hdr_buf(res, qplqp, sstep, rstep);
	if (rc)
		goto out;

	rc = bnxt_qplib_create_qp1(res, qplqp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "create HW QP1 failed!\n");
		goto out;
	}

	if (gsi_mode == BNXT_RE_GSI_MODE_ALL)
		rc = bnxt_re_create_shadow_gsi(qp, pd);
out:
	return rc;
}

static bool bnxt_re_test_qp_limits(struct bnxt_re_dev *rdev,
				   struct ib_qp_init_attr *init_attr,
				   struct bnxt_qplib_dev_attr *dev_attr)
{
	bool rc = true;
	int ilsize;

	ilsize = ALIGN(init_attr->cap.max_inline_data, sizeof(struct sq_sge));
	if ((init_attr->cap.max_send_wr > dev_attr->max_qp_wqes) ||
	    (init_attr->cap.max_recv_wr > dev_attr->max_qp_wqes) ||
	    (init_attr->cap.max_send_sge > dev_attr->max_qp_sges) ||
	    (init_attr->cap.max_recv_sge > dev_attr->max_qp_sges) ||
	    (ilsize > dev_attr->max_inline_data)) {
		dev_err(rdev_to_dev(rdev), "Create QP failed - max exceeded! "
			"0x%x/0x%x 0x%x/0x%x 0x%x/0x%x "
			"0x%x/0x%x 0x%x/0x%x\n",
			init_attr->cap.max_send_wr, dev_attr->max_qp_wqes,
			init_attr->cap.max_recv_wr, dev_attr->max_qp_wqes,
			init_attr->cap.max_send_sge, dev_attr->max_qp_sges,
			init_attr->cap.max_recv_sge, dev_attr->max_qp_sges,
			init_attr->cap.max_inline_data,
			dev_attr->max_inline_data);
		rc = false;
	}
	return rc;
}

static inline struct
bnxt_re_qp *__get_qp_from_qp_in(struct ib_pd *qp_in,
				struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *qp;

	qp = kzalloc(sizeof(*qp), GFP_KERNEL);
	if (!qp)
		dev_err(rdev_to_dev(rdev), "Allocate QP failed!\n");
	return qp;
}

struct ib_qp *bnxt_re_create_qp(struct ib_pd *qp_in,
			       struct ib_qp_init_attr *qp_init_attr,
			       struct ib_udata *udata)
{
	struct bnxt_re_pd *pd;
	struct ib_pd *ib_pd = qp_in;
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_dev *rdev;
	u32 active_qps, tmp_qps;
	struct bnxt_re_qp *qp;
	int rc;

	pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	rdev = pd->rdev;
	dev_attr = rdev->dev_attr;
	if (rdev->mod_exit) {
		rc = -EIO;
		dev_dbg(rdev_to_dev(rdev), "%s(): in mod_exit, just return!\n", __func__);
		goto exit;
	}

	if (atomic_read(&rdev->stats.rsors.qp_count) >= dev_attr->max_qp) {
		dev_err(rdev_to_dev(rdev), "Create QP failed - max exceeded(QPs Alloc'd %u of max %u)\n",
			atomic_read(&rdev->stats.rsors.qp_count), dev_attr->max_qp);
		rc = -EINVAL;
		goto exit;
	}

	rc = bnxt_re_test_qp_limits(rdev, qp_init_attr, dev_attr);
	if (!rc) {
		rc = -EINVAL;
		goto exit;
	}
	qp = __get_qp_from_qp_in(qp_in, rdev);
	if (!qp) {
		rc = -ENOMEM;
		goto exit;
	}
	qp->rdev = rdev;

	rc = bnxt_re_init_qp_attr(qp, pd, qp_init_attr, udata);
	if (rc)
		goto fail;

	if (qp_init_attr->qp_type == IB_QPT_GSI &&
	    !_is_chip_gen_p5_p7(rdev->chip_ctx)) {
		rc = bnxt_re_create_gsi_qp(qp, pd);
		if (rc == -ENODEV)
			goto qp_destroy;
		if (rc)
			goto fail;
	} else {
		rc = bnxt_qplib_create_qp(&rdev->qplib_res, &qp->qplib_qp);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "create HW QP failed!\n");
			goto free_umem;
		}

		if (udata) {
			struct bnxt_re_qp_resp resp;

			resp.qpid = qp->qplib_qp.id;
			rc = bnxt_re_copy_to_udata(rdev, &resp,
						   min(udata->outlen, sizeof(resp)),
						   udata);
			if (rc)
				goto qp_destroy;
		}
	}

	qp->ib_qp.qp_num = qp->qplib_qp.id;
	if (qp_init_attr->qp_type == IB_QPT_GSI)
		rdev->gsi_ctx.gsi_qp = qp;
	spin_lock_init(&qp->sq_lock);
	spin_lock_init(&qp->rq_lock);
	INIT_LIST_HEAD(&qp->list);
	mutex_lock(&rdev->qp_lock);
	list_add_tail(&qp->list, &rdev->qp_list);
	mutex_unlock(&rdev->qp_lock);
	atomic_inc(&rdev->stats.rsors.qp_count);
	active_qps = atomic_read(&rdev->stats.rsors.qp_count);
	if (active_qps > atomic_read(&rdev->stats.rsors.max_qp_count))
		atomic_set(&rdev->stats.rsors.max_qp_count, active_qps);

	bnxt_re_dump_debug_stats(rdev, active_qps);

	/* Get the counters for RC QPs and UD QPs */
	if (qp_init_attr->qp_type == IB_QPT_RC) {
		tmp_qps = atomic_inc_return(&rdev->stats.rsors.rc_qp_count);
		if (tmp_qps > atomic_read(&rdev->stats.rsors.max_rc_qp_count))
			atomic_set(&rdev->stats.rsors.max_rc_qp_count, tmp_qps);
	} else if (qp_init_attr->qp_type == IB_QPT_UD) {
		tmp_qps = atomic_inc_return(&rdev->stats.rsors.ud_qp_count);
		if (tmp_qps > atomic_read(&rdev->stats.rsors.max_ud_qp_count))
			atomic_set(&rdev->stats.rsors.max_ud_qp_count, tmp_qps);
	}

	return &qp->ib_qp;

qp_destroy:
	bnxt_qplib_destroy_qp(&rdev->qplib_res, &qp->qplib_qp);
free_umem:
	if (udata) {
		if (qp->rumem && !IS_ERR(qp->rumem))
			ib_umem_release(qp->rumem);
		if (qp->sumem && !IS_ERR(qp->sumem))
			ib_umem_release(qp->sumem);
	}
fail:
	kfree(qp);
exit:
	return ERR_PTR(rc);
}

static int bnxt_re_modify_shadow_qp(struct bnxt_re_dev *rdev,
			     struct bnxt_re_qp *qp1_qp,
			     int qp_attr_mask)
{
	struct bnxt_re_qp *qp = rdev->gsi_ctx.gsi_sqp;
	int rc = 0;

	if (qp_attr_mask & IB_QP_STATE) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_STATE;
		qp->qplib_qp.state = qp1_qp->qplib_qp.state;
	}
	if (qp_attr_mask & IB_QP_PKEY_INDEX) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_PKEY;
		qp->qplib_qp.pkey_index = qp1_qp->qplib_qp.pkey_index;
	}

	if (qp_attr_mask & IB_QP_QKEY) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_QKEY;
		/* Using a Random  QKEY */
		qp->qplib_qp.qkey = BNXT_RE_QP_RANDOM_QKEY;
	}
	if (qp_attr_mask & IB_QP_SQ_PSN) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_SQ_PSN;
		qp->qplib_qp.sq.psn = qp1_qp->qplib_qp.sq.psn;
	}

	rc = bnxt_qplib_modify_qp(&rdev->qplib_res, &qp->qplib_qp);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Modify Shadow QP for QP1 failed\n");
	return rc;
}

static u32 ipv4_from_gid(u8 *gid)
{
	return (gid[15] << 24 | gid[14] << 16 | gid[13] << 8 | gid[12]);
}

static u16 get_source_port(struct bnxt_re_dev *rdev,
			   struct bnxt_re_qp *qp)
{
	u8 ip_off, data[48], smac[ETH_ALEN];
	u16 crc = 0, buf_len = 0, i;
	u8 addr_len;
	u32 qpn;

	if (qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV6) {
		addr_len = 6;
		ip_off = 10;
	} else {
		addr_len = 4;
		ip_off = 12;
	}

	memcpy(smac, qp->qplib_qp.smac, ETH_ALEN);

	memset(data, 0, 48);
	memcpy(data, qp->qplib_qp.ah.dmac, ETH_ALEN);
	buf_len += ETH_ALEN;

	memcpy(data + buf_len, smac, ETH_ALEN);
	buf_len += ETH_ALEN;

	memcpy(data + buf_len, qp->qplib_qp.ah.dgid.data + ip_off, addr_len);
	buf_len += addr_len;

	memcpy(data + buf_len, qp->qp_info_entry.sgid.raw + ip_off, addr_len);
	buf_len += addr_len;

	qpn = htonl(qp->qplib_qp.dest_qpn);
	memcpy(data + buf_len, (u8 *)&qpn + 1, 3);
	buf_len += 3;

	for (i = 0; i < buf_len; i++)
		crc = crc16(crc, (data + i), 1);

	return crc;
}

static void bnxt_re_update_qp_info(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	u16 type;

	type = __from_hw_to_ib_qp_type(qp->qplib_qp.type);

	/* User-space can extract ip address with sgid_index. */
	if (ipv6_addr_v4mapped((struct in6_addr *)&qp->qplib_qp.ah.dgid)) {
		qp->qp_info_entry.s_ip.ipv4_addr = ipv4_from_gid(qp->qp_info_entry.sgid.raw);
		qp->qp_info_entry.d_ip.ipv4_addr = ipv4_from_gid(qp->qplib_qp.ah.dgid.data);
	} else {
		memcpy(&qp->qp_info_entry.s_ip.ipv6_addr, qp->qp_info_entry.sgid.raw,
		       sizeof(qp->qp_info_entry.s_ip.ipv6_addr));
		memcpy(&qp->qp_info_entry.d_ip.ipv6_addr, qp->qplib_qp.ah.dgid.data,
		       sizeof(qp->qp_info_entry.d_ip.ipv6_addr));
	}

	if (type == IB_QPT_RC &&
	    (qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV4 ||
	     qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV6)) {
		qp->qp_info_entry.s_port = get_source_port(rdev, qp);
	}
	qp->qp_info_entry.d_port = BNXT_RE_QP_DEST_PORT;
}

static void bnxt_qplib_manage_flush_qp(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_q *rq, *sq;
	unsigned long flags;

	if (qp->sumem)
		return;

	if (qp->qplib_qp.state == CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		rq = &qp->qplib_qp.rq;
		sq = &qp->qplib_qp.sq;

		dev_dbg(rdev_to_dev(qp->rdev),
			"Move QP = %p to flush list\n", qp);
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_add_flush_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);

		if (sq->hwq.prod != sq->hwq.cons)
			bnxt_re_handle_cqn(&qp->scq->qplib_cq);

		if (qp->rcq && (qp->rcq != qp->scq) &&
		    (rq->hwq.prod != rq->hwq.cons))
			bnxt_re_handle_cqn(&qp->rcq->qplib_cq);
	}

	if (qp->qplib_qp.state == CMDQ_MODIFY_QP_NEW_STATE_RESET) {
		dev_dbg(rdev_to_dev(qp->rdev),
			"Move QP = %p out of flush list\n", qp);
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_clean_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);
	}
}

bool ib_modify_qp_is_ok_compat(enum ib_qp_state cur_state,
			       enum ib_qp_state next_state,
			       enum ib_qp_type type,
			       enum ib_qp_attr_mask mask)
{
		return (ib_modify_qp_is_ok(cur_state, next_state,
					   type, mask));
}

int bnxt_re_modify_qp(struct ib_qp *ib_qp, struct ib_qp_attr *qp_attr,
		      int qp_attr_mask, struct ib_udata *udata)
{
	enum ib_qp_state curr_qp_state, new_qp_state;
	struct bnxt_re_modify_qp_ex_resp resp = {};
	struct bnxt_re_modify_qp_ex_req ureq = {};
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_ppp *ppp = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_qp *qp;
	struct ib_gid_attr *sgid_attr;
	struct ib_gid_attr gid_attr;
	union ib_gid sgid, *gid_ptr = NULL;
	u8 nw_type;
	int rc, entries, status;
	bool is_copy_to_udata = false;
	bool is_qpmtu_high = false;

	qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	rdev = qp->rdev;
	dev_attr = rdev->dev_attr;

	qp->qplib_qp.modify_flags = 0;
	ppp = &qp->qplib_qp.ppp;
	if (qp_attr_mask & IB_QP_STATE) {
		curr_qp_state = __to_ib_qp_state(qp->qplib_qp.cur_qp_state);
		new_qp_state = qp_attr->qp_state;
		if (!ib_modify_qp_is_ok_compat(curr_qp_state, new_qp_state,
					       ib_qp->qp_type, qp_attr_mask)) {
			dev_err(rdev_to_dev(rdev),"invalid attribute mask=0x%x"
				" specified for qpn=0x%x of type=0x%x"
				" current_qp_state=0x%x, new_qp_state=0x%x\n",
				qp_attr_mask, ib_qp->qp_num, ib_qp->qp_type,
				curr_qp_state, new_qp_state);
			return -EINVAL;
		}
		dev_dbg(rdev_to_dev(rdev), "%s:%d INFO attribute mask=0x%x qpn=0x%x "
			"of type=0x%x current_qp_state=0x%x, new_qp_state=0x%x\n",
			__func__, __LINE__, qp_attr_mask, ib_qp->qp_num,
			ib_qp->qp_type, curr_qp_state, new_qp_state);
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_STATE;
		qp->qplib_qp.state = __from_ib_qp_state(qp_attr->qp_state);

		if (udata && curr_qp_state == IB_QPS_RESET &&
		    new_qp_state == IB_QPS_INIT) {
			if (!ib_copy_from_udata(&ureq, udata, sizeof(ureq))) {
				if (ureq.comp_mask &
				    BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN_MASK) {
					ppp->req = BNXT_QPLIB_PPP_REQ;
					ppp->dpi = ureq.dpi;
				}
			}
		}
	}
	if (qp_attr_mask & IB_QP_EN_SQD_ASYNC_NOTIFY) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_EN_SQD_ASYNC_NOTIFY;
		qp->qplib_qp.en_sqd_async_notify = true;
	}
	if (qp_attr_mask & IB_QP_ACCESS_FLAGS) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_ACCESS;
		qp->qplib_qp.access =
			__from_ib_access_flags(qp_attr->qp_access_flags);
		/* LOCAL_WRITE access must be set to allow RC receive */
		qp->qplib_qp.access |= BNXT_QPLIB_ACCESS_LOCAL_WRITE;
		qp->qplib_qp.access |= CMDQ_MODIFY_QP_ACCESS_REMOTE_WRITE;
		qp->qplib_qp.access |= CMDQ_MODIFY_QP_ACCESS_REMOTE_READ;
	}
	if (qp_attr_mask & IB_QP_PKEY_INDEX) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_PKEY;
		qp->qplib_qp.pkey_index = qp_attr->pkey_index;
	}
	if (qp_attr_mask & IB_QP_QKEY) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_QKEY;
		qp->qplib_qp.qkey = qp_attr->qkey;
	}
	if (qp_attr_mask & IB_QP_AV) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_DGID |
				     CMDQ_MODIFY_QP_MODIFY_MASK_FLOW_LABEL |
				     CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX |
				     CMDQ_MODIFY_QP_MODIFY_MASK_HOP_LIMIT |
				     CMDQ_MODIFY_QP_MODIFY_MASK_TRAFFIC_CLASS |
				     CMDQ_MODIFY_QP_MODIFY_MASK_DEST_MAC |
				     CMDQ_MODIFY_QP_MODIFY_MASK_VLAN_ID;
		memcpy(qp->qplib_qp.ah.dgid.data, qp_attr->ah_attr.grh.dgid.raw,
		       sizeof(qp->qplib_qp.ah.dgid.data));
		qp->qplib_qp.ah.flow_label = qp_attr->ah_attr.grh.flow_label;
		qp->qplib_qp.ah.sgid_index = _get_sgid_index(rdev,
						qp_attr->ah_attr.grh.sgid_index);
		qp->qplib_qp.ah.host_sgid_index = qp_attr->ah_attr.grh.sgid_index;
		qp->qplib_qp.ah.hop_limit = qp_attr->ah_attr.grh.hop_limit;
		qp->qplib_qp.ah.traffic_class =
					qp_attr->ah_attr.grh.traffic_class;
		qp->qplib_qp.ah.sl = qp_attr->ah_attr.sl;
		ether_addr_copy(qp->qplib_qp.ah.dmac, ROCE_DMAC(&qp_attr->ah_attr));
		sgid_attr = &gid_attr;
		status = bnxt_re_get_cached_gid(&rdev->ibdev, 1,
						qp_attr->ah_attr.grh.sgid_index,
						&sgid, &sgid_attr,
						&qp_attr->ah_attr.grh, NULL);
		if (!status)
			if_rele(sgid_attr->ndev);
		gid_ptr = &sgid;
		if (sgid_attr->ndev) {
			memcpy(qp->qplib_qp.smac, rdev->dev_addr,
			       ETH_ALEN);
			nw_type = bnxt_re_gid_to_network_type(sgid_attr, &sgid);
			dev_dbg(rdev_to_dev(rdev),
				 "Connection using the nw_type %d\n", nw_type);
			switch (nw_type) {
			case RDMA_NETWORK_IPV4:
				qp->qplib_qp.nw_type =
					CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV4;
				break;
			case RDMA_NETWORK_IPV6:
				qp->qplib_qp.nw_type =
					CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV6;
				break;
			default:
				qp->qplib_qp.nw_type =
					CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV1;
				break;
			}
		}
		memcpy(&qp->qp_info_entry.sgid, gid_ptr, sizeof(qp->qp_info_entry.sgid));
	}

	/* MTU settings allowed only during INIT -> RTR */
	if (qp_attr->qp_state == IB_QPS_RTR) {
		bnxt_re_init_qpmtu(qp, if_getmtu(rdev->netdev), qp_attr_mask, qp_attr,
				   &is_qpmtu_high);
		if (udata && !ib_copy_from_udata(&ureq, udata, sizeof(ureq))) {
			if (ureq.comp_mask & BNXT_RE_COMP_MASK_MQP_EX_PATH_MTU_MASK) {
				resp.comp_mask |= BNXT_RE_COMP_MASK_MQP_EX_PATH_MTU_MASK;
				resp.path_mtu = qp->qplib_qp.mtu;
				is_copy_to_udata = true;
			} else if (is_qpmtu_high) {
				dev_err(rdev_to_dev(rdev), "qp %#x invalid mtu\n",
					qp->qplib_qp.id);
				return -EINVAL;
			}
		}
	}

	if (qp_attr_mask & IB_QP_TIMEOUT) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_TIMEOUT;
		qp->qplib_qp.timeout = qp_attr->timeout;
	}
	if (qp_attr_mask & IB_QP_RETRY_CNT) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_RETRY_CNT;
		qp->qplib_qp.retry_cnt = qp_attr->retry_cnt;
	}
	if (qp_attr_mask & IB_QP_RNR_RETRY) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_RNR_RETRY;
		qp->qplib_qp.rnr_retry = qp_attr->rnr_retry;
	}
	if (qp_attr_mask & IB_QP_MIN_RNR_TIMER) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_MIN_RNR_TIMER;
		qp->qplib_qp.min_rnr_timer = qp_attr->min_rnr_timer;
	}
	if (qp_attr_mask & IB_QP_RQ_PSN) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_RQ_PSN;
		qp->qplib_qp.rq.psn = qp_attr->rq_psn;
	}
	if (qp_attr_mask & IB_QP_MAX_QP_RD_ATOMIC) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_MAX_RD_ATOMIC;
		/* Cap the max_rd_atomic to device max */
		if (qp_attr->max_rd_atomic > dev_attr->max_qp_rd_atom)
			dev_dbg(rdev_to_dev(rdev),
				"max_rd_atomic requested %d is > device max %d\n",
				qp_attr->max_rd_atomic,
				dev_attr->max_qp_rd_atom);
		qp->qplib_qp.max_rd_atomic = min_t(u32, qp_attr->max_rd_atomic,
						   dev_attr->max_qp_rd_atom);
	}
	if (qp_attr_mask & IB_QP_SQ_PSN) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_SQ_PSN;
		qp->qplib_qp.sq.psn = qp_attr->sq_psn;
	}
	if (qp_attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) {
		if (qp_attr->max_dest_rd_atomic >
		    dev_attr->max_qp_init_rd_atom) {
			dev_err(rdev_to_dev(rdev),
				"max_dest_rd_atomic requested %d is > device max %d\n",
				qp_attr->max_dest_rd_atomic,
				dev_attr->max_qp_init_rd_atom);
			return -EINVAL;
		}
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_MAX_DEST_RD_ATOMIC;
		qp->qplib_qp.max_dest_rd_atomic = qp_attr->max_dest_rd_atomic;
	}
	if (qp_attr_mask & IB_QP_CAP) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_SQ_SIZE |
				CMDQ_MODIFY_QP_MODIFY_MASK_RQ_SIZE |
				CMDQ_MODIFY_QP_MODIFY_MASK_SQ_SGE |
				CMDQ_MODIFY_QP_MODIFY_MASK_RQ_SGE |
				CMDQ_MODIFY_QP_MODIFY_MASK_MAX_INLINE_DATA;
		if ((qp_attr->cap.max_send_wr >= dev_attr->max_qp_wqes) ||
		    (qp_attr->cap.max_recv_wr >= dev_attr->max_qp_wqes) ||
		    (qp_attr->cap.max_send_sge >= dev_attr->max_qp_sges) ||
		    (qp_attr->cap.max_recv_sge >= dev_attr->max_qp_sges) ||
		    (qp_attr->cap.max_inline_data >=
						dev_attr->max_inline_data)) {
			dev_err(rdev_to_dev(rdev),
				"Create QP failed - max exceeded\n");
			return -EINVAL;
		}
		entries = roundup_pow_of_two(qp_attr->cap.max_send_wr);
		if (entries > dev_attr->max_qp_wqes)
			entries = dev_attr->max_qp_wqes;
		entries = min_t(u32, entries, dev_attr->max_qp_wqes);
		qp->qplib_qp.sq.max_wqe = entries;
		qp->qplib_qp.sq.q_full_delta = qp->qplib_qp.sq.max_wqe -
						qp_attr->cap.max_send_wr;
		/*
		 * Reserving one slot for Phantom WQE. Some application can
		 * post one extra entry in this case. Allowing this to avoid
		 * unexpected Queue full condition
		 */
		qp->qplib_qp.sq.q_full_delta -= 1;
		qp->qplib_qp.sq.max_sge = qp_attr->cap.max_send_sge;
		if (qp->qplib_qp.rq.max_wqe) {
			entries = roundup_pow_of_two(qp_attr->cap.max_recv_wr);
			if (entries > dev_attr->max_qp_wqes)
				entries = dev_attr->max_qp_wqes;
			qp->qplib_qp.rq.max_wqe = entries;
			qp->qplib_qp.rq.q_full_delta = qp->qplib_qp.rq.max_wqe -
						       qp_attr->cap.max_recv_wr;
			qp->qplib_qp.rq.max_sge = qp_attr->cap.max_recv_sge;
		} else {
			/* SRQ was used prior, just ignore the RQ caps */
		}
	}
	if (qp_attr_mask & IB_QP_DEST_QPN) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_DEST_QP_ID;
		qp->qplib_qp.dest_qpn = qp_attr->dest_qp_num;
	}

	rc = bnxt_qplib_modify_qp(&rdev->qplib_res, &qp->qplib_qp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Modify HW QP failed!\n");
		return rc;
	}
	if (qp_attr_mask & IB_QP_STATE)
		bnxt_qplib_manage_flush_qp(qp);
	if (ureq.comp_mask & BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN_MASK &&
	    ppp->st_idx_en & CREQ_MODIFY_QP_RESP_PINGPONG_PUSH_ENABLED) {
		resp.comp_mask |= BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN;
		resp.ppp_st_idx = ppp->st_idx_en >>
				  BNXT_QPLIB_PPP_ST_IDX_SHIFT;
		is_copy_to_udata = true;
	}

	if (is_copy_to_udata) {
		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc)
			return rc;
	}

	if (ib_qp->qp_type == IB_QPT_GSI &&
	    rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL &&
	    rdev->gsi_ctx.gsi_sqp)
		rc = bnxt_re_modify_shadow_qp(rdev, qp, qp_attr_mask);
	/*
	 * Update info when qp_info_info
	 */
	bnxt_re_update_qp_info(rdev, qp);
	return rc;
}

int bnxt_re_query_qp(struct ib_qp *ib_qp, struct ib_qp_attr *qp_attr,
		     int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_qp *qplib_qp;
	int rc;

	qplib_qp = kcalloc(1, sizeof(*qplib_qp), GFP_KERNEL);
	if (!qplib_qp)
		return -ENOMEM;

	qplib_qp->id = qp->qplib_qp.id;
	qplib_qp->ah.host_sgid_index = qp->qplib_qp.ah.host_sgid_index;

	rc = bnxt_qplib_query_qp(&rdev->qplib_res, qplib_qp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Query HW QP (0x%x) failed! rc = %d\n",
			qplib_qp->id, rc);
		goto free_mem;
	}
	qp_attr->qp_state = __to_ib_qp_state(qplib_qp->state);
	qp_attr->cur_qp_state = __to_ib_qp_state(qplib_qp->cur_qp_state);
	qp_attr->en_sqd_async_notify = qplib_qp->en_sqd_async_notify ? 1 : 0;
	qp_attr->qp_access_flags = __to_ib_access_flags(qplib_qp->access);
	qp_attr->pkey_index = qplib_qp->pkey_index;
	qp_attr->qkey = qplib_qp->qkey;
	memcpy(qp_attr->ah_attr.grh.dgid.raw, qplib_qp->ah.dgid.data,
	       sizeof(qplib_qp->ah.dgid.data));
	qp_attr->ah_attr.grh.flow_label = qplib_qp->ah.flow_label;
	qp_attr->ah_attr.grh.sgid_index = qplib_qp->ah.host_sgid_index;
	qp_attr->ah_attr.grh.hop_limit = qplib_qp->ah.hop_limit;
	qp_attr->ah_attr.grh.traffic_class = qplib_qp->ah.traffic_class;
	qp_attr->ah_attr.sl = qplib_qp->ah.sl;
	ether_addr_copy(ROCE_DMAC(&qp_attr->ah_attr), qplib_qp->ah.dmac);
	qp_attr->path_mtu = __to_ib_mtu(qplib_qp->path_mtu);
	qp_attr->timeout = qplib_qp->timeout;
	qp_attr->retry_cnt = qplib_qp->retry_cnt;
	qp_attr->rnr_retry = qplib_qp->rnr_retry;
	qp_attr->min_rnr_timer = qplib_qp->min_rnr_timer;
	qp_attr->rq_psn = qplib_qp->rq.psn;
	qp_attr->max_rd_atomic = qplib_qp->max_rd_atomic;
	qp_attr->sq_psn = qplib_qp->sq.psn;
	qp_attr->max_dest_rd_atomic = qplib_qp->max_dest_rd_atomic;
	qp_init_attr->sq_sig_type = qplib_qp->sig_type ? IB_SIGNAL_ALL_WR :
							IB_SIGNAL_REQ_WR;
	qp_attr->dest_qp_num = qplib_qp->dest_qpn;

	qp_attr->cap.max_send_wr = qp->qplib_qp.sq.max_wqe;
	qp_attr->cap.max_send_sge = qp->qplib_qp.sq.max_sge;
	qp_attr->cap.max_recv_wr = qp->qplib_qp.rq.max_wqe;
	qp_attr->cap.max_recv_sge = qp->qplib_qp.rq.max_sge;
	qp_attr->cap.max_inline_data = qp->qplib_qp.max_inline_data;
	qp_init_attr->cap = qp_attr->cap;

free_mem:
	kfree(qplib_qp);
	return rc;
}

/* Builders */

/* For Raw, the application is responsible to build the entire packet */
static void bnxt_re_build_raw_send(const struct ib_send_wr *wr,
				   struct bnxt_qplib_swqe *wqe)
{
	switch (wr->send_flags) {
	case IB_SEND_IP_CSUM:
		wqe->rawqp1.lflags |= SQ_SEND_RAWETH_QP1_LFLAGS_IP_CHKSUM;
		break;
	default:
		/* Pad HW RoCE iCRC */
		wqe->rawqp1.lflags |= SQ_SEND_RAWETH_QP1_LFLAGS_ROCE_CRC;
		break;
	}
}

/* For QP1, the driver must build the entire RoCE (v1/v2) packet hdr
 * as according to the sgid and AV
 */
static int bnxt_re_build_qp1_send(struct bnxt_re_qp *qp, const struct ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe, int payload_size)
{
	struct bnxt_re_ah *ah = to_bnxt_re(ud_wr(wr)->ah, struct bnxt_re_ah,
					   ibah);
	struct bnxt_qplib_ah *qplib_ah = &ah->qplib_ah;
	struct bnxt_qplib_sge sge;
	int i, rc = 0;
	union ib_gid sgid;
	u16 vlan_id;
	u8 *ptmac;
	void *buf;

	memset(&qp->qp1_hdr, 0, sizeof(qp->qp1_hdr));

	/* Get sgid */
	rc = bnxt_re_query_gid(&qp->rdev->ibdev, 1, qplib_ah->sgid_index, &sgid);
	if (rc)
		return rc;

	/* ETH */
	qp->qp1_hdr.eth_present = 1;
	ptmac = ah->qplib_ah.dmac;
	memcpy(qp->qp1_hdr.eth.dmac_h, ptmac, 4);
	ptmac += 4;
	memcpy(qp->qp1_hdr.eth.dmac_l, ptmac, 2);

	ptmac = qp->qplib_qp.smac;
	memcpy(qp->qp1_hdr.eth.smac_h, ptmac, 2);
	ptmac += 2;
	memcpy(qp->qp1_hdr.eth.smac_l, ptmac, 4);

	qp->qp1_hdr.eth.type = cpu_to_be16(BNXT_QPLIB_ETHTYPE_ROCEV1);

	/* For vlan, check the sgid for vlan existence */
	vlan_id = rdma_get_vlan_id(&sgid);
	if (vlan_id && vlan_id < 0x1000) {
		qp->qp1_hdr.vlan_present = 1;
		qp->qp1_hdr.eth.type = cpu_to_be16(ETH_P_8021Q);
	}
	/* GRH */
	qp->qp1_hdr.grh_present = 1;
	qp->qp1_hdr.grh.ip_version = 6;
	qp->qp1_hdr.grh.payload_length =
		cpu_to_be16((IB_BTH_BYTES + IB_DETH_BYTES + payload_size + 7)
			    & ~3);
	qp->qp1_hdr.grh.next_header = 0x1b;
	memcpy(qp->qp1_hdr.grh.source_gid.raw, sgid.raw, sizeof(sgid));
	memcpy(qp->qp1_hdr.grh.destination_gid.raw, qplib_ah->dgid.data,
	       sizeof(sgid));

	/* BTH */
	if (wr->opcode == IB_WR_SEND_WITH_IMM) {
		qp->qp1_hdr.bth.opcode = IB_OPCODE_UD_SEND_ONLY_WITH_IMMEDIATE;
		qp->qp1_hdr.immediate_present = 1;
	} else {
		qp->qp1_hdr.bth.opcode = IB_OPCODE_UD_SEND_ONLY;
	}
	if (wr->send_flags & IB_SEND_SOLICITED)
		qp->qp1_hdr.bth.solicited_event = 1;
	qp->qp1_hdr.bth.pad_count = (4 - payload_size) & 3;
	/* P_key for QP1 is for all members */
	qp->qp1_hdr.bth.pkey = cpu_to_be16(0xFFFF);
	qp->qp1_hdr.bth.destination_qpn = IB_QP1;
	qp->qp1_hdr.bth.ack_req = 0;
	qp->send_psn++;
	qp->send_psn &= BTH_PSN_MASK;
	qp->qp1_hdr.bth.psn = cpu_to_be32(qp->send_psn);
	/* DETH */
	/* Use the priviledged Q_Key for QP1 */
	qp->qp1_hdr.deth.qkey = cpu_to_be32(IB_QP1_QKEY);
	qp->qp1_hdr.deth.source_qpn = IB_QP1;

	/* Pack the QP1 to the transmit buffer */
	buf = bnxt_qplib_get_qp1_sq_buf(&qp->qplib_qp, &sge);
	if (!buf) {
		dev_err(rdev_to_dev(qp->rdev), "QP1 buffer is empty!\n");
		rc = -ENOMEM;
	}
	for (i = wqe->num_sge; i; i--) {
		wqe->sg_list[i].addr = wqe->sg_list[i - 1].addr;
		wqe->sg_list[i].lkey = wqe->sg_list[i - 1].lkey;
		wqe->sg_list[i].size = wqe->sg_list[i - 1].size;
	}
	wqe->sg_list[0].addr = sge.addr;
	wqe->sg_list[0].lkey = sge.lkey;
	wqe->sg_list[0].size = sge.size;
	wqe->num_sge++;

	return rc;
}

static int bnxt_re_build_gsi_send(struct bnxt_re_qp *qp,
				  const struct ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_dev *rdev;
	int rc, indx, len = 0;

	rdev = qp->rdev;

	/* Mode UD is applicable to Gen P5 only */
	if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_UD)
		return 0;

	for (indx = 0; indx < wr->num_sge; indx++) {
		wqe->sg_list[indx].addr = wr->sg_list[indx].addr;
		wqe->sg_list[indx].lkey = wr->sg_list[indx].lkey;
		wqe->sg_list[indx].size = wr->sg_list[indx].length;
		len += wr->sg_list[indx].length;
	}
	rc = bnxt_re_build_qp1_send(qp, wr, wqe, len);
	wqe->rawqp1.lflags |= SQ_SEND_RAWETH_QP1_LFLAGS_ROCE_CRC;

	return rc;
}

/* For the MAD layer, it only provides the recv SGE the size of
   ib_grh + MAD datagram.  No Ethernet headers, Ethertype, BTH, DETH,
   nor RoCE iCRC.  The Cu+ solution must provide buffer for the entire
   receive packet (334 bytes) with no VLAN and then copy the GRH
   and the MAD datagram out to the provided SGE.
*/

static int bnxt_re_build_qp1_recv(struct bnxt_re_qp *qp,
				  const struct ib_recv_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_sge ref, sge;
	u8 udp_hdr_size = 0;
	u8 ip_hdr_size = 0;
	int rc = 0;
	int size;

	if (bnxt_qplib_get_qp1_rq_buf(&qp->qplib_qp, &sge)) {
		/* Create 5 SGEs as according to the following:
		 * Ethernet header (14)
		 * ib_grh (40) - as provided from the wr
		 * ib_bth + ib_deth + UDP(RoCE v2 only)  (28)
		 * MAD (256) - as provided from the wr
		 * iCRC (4)
		 */

		/* Set RoCE v2 header size and offsets */
		if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ROCE_V2_IPV4)
			ip_hdr_size = 20;
		if (rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_ROCE_V1)
			udp_hdr_size = 8;

		/* Save the reference from ULP */
		ref.addr = wr->sg_list[0].addr;
		ref.lkey = wr->sg_list[0].lkey;
		ref.size = wr->sg_list[0].length;

		/* SGE 1 */
		size = sge.size;
		wqe->sg_list[0].addr = sge.addr;
		wqe->sg_list[0].lkey = sge.lkey;
		wqe->sg_list[0].size = BNXT_QPLIB_MAX_QP1_RQ_ETH_HDR_SIZE;
		size -= wqe->sg_list[0].size;
		if (size <= 0) {
			dev_err(rdev_to_dev(qp->rdev),"QP1 rq buffer is empty!\n");
			rc = -ENOMEM;
			goto done;
		}
		sge.size = (u32)size;
		sge.addr += wqe->sg_list[0].size;

		/* SGE 2 */
		/* In case of RoCE v2 ipv4 lower 20 bytes should have IP hdr */
		wqe->sg_list[1].addr = ref.addr + ip_hdr_size;
		wqe->sg_list[1].lkey = ref.lkey;
		wqe->sg_list[1].size = sizeof(struct ib_grh) - ip_hdr_size;
		ref.size -= wqe->sg_list[1].size;
		if (ref.size <= 0) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 ref buffer is empty!\n");
			rc = -ENOMEM;
			goto done;
		}
		ref.addr += wqe->sg_list[1].size + ip_hdr_size;

		/* SGE 3 */
		wqe->sg_list[2].addr = sge.addr;
		wqe->sg_list[2].lkey = sge.lkey;
		wqe->sg_list[2].size = BNXT_QPLIB_MAX_QP1_RQ_BDETH_HDR_SIZE +
				       udp_hdr_size;
		size -= wqe->sg_list[2].size;
		if (size <= 0) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 rq buffer is empty!\n");
			rc = -ENOMEM;
			goto done;
		}
		sge.size = (u32)size;
		sge.addr += wqe->sg_list[2].size;

		/* SGE 4 */
		wqe->sg_list[3].addr = ref.addr;
		wqe->sg_list[3].lkey = ref.lkey;
		wqe->sg_list[3].size = ref.size;
		ref.size -= wqe->sg_list[3].size;
		if (ref.size) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 ref buffer is incorrect!\n");
			rc = -ENOMEM;
			goto done;
		}
		/* SGE 5 */
		wqe->sg_list[4].addr = sge.addr;
		wqe->sg_list[4].lkey = sge.lkey;
		wqe->sg_list[4].size = sge.size;
		size -= wqe->sg_list[4].size;
		if (size) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 rq buffer is incorrect!\n");
			rc = -ENOMEM;
			goto done;
		}
		sge.size = (u32)size;
		wqe->num_sge = 5;
	} else {
		dev_err(rdev_to_dev(qp->rdev), "QP1 buffer is empty!\n");
		rc = -ENOMEM;
	}
done:
	return rc;
}

static int bnxt_re_build_qp1_shadow_qp_recv(struct bnxt_re_qp *qp,
					    const struct ib_recv_wr *wr,
					    struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_sqp_entries *sqp_entry;
	struct bnxt_qplib_sge sge;
	struct bnxt_re_dev *rdev;
	u32 rq_prod_index;
	int rc = 0;

	rdev = qp->rdev;

	rq_prod_index = bnxt_qplib_get_rq_prod_index(&qp->qplib_qp);

	if (bnxt_qplib_get_qp1_rq_buf(&qp->qplib_qp, &sge)) {
		/* Create 1 SGE to receive the entire
		 * ethernet packet
		 */
		/* SGE 1 */
		wqe->sg_list[0].addr = sge.addr;
		/* TODO check the lkey to be used */
		wqe->sg_list[0].lkey = sge.lkey;
		wqe->sg_list[0].size = BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE_V2;
		if (sge.size < wqe->sg_list[0].size) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 rq buffer is empty!\n");
			rc = -ENOMEM;
			goto done;
		}

		sqp_entry = &rdev->gsi_ctx.sqp_tbl[rq_prod_index];
		sqp_entry->sge.addr = wr->sg_list[0].addr;
		sqp_entry->sge.lkey = wr->sg_list[0].lkey;
		sqp_entry->sge.size = wr->sg_list[0].length;
		/* Store the wrid for reporting completion */
		sqp_entry->wrid = wqe->wr_id;
		/* change the wqe->wrid to table index */
		wqe->wr_id = rq_prod_index;
	}
done:
	return rc;
}

static bool is_ud_qp(struct bnxt_re_qp *qp)
{
	return (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_UD ||
		qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_GSI);
}

static int bnxt_re_build_send_wqe(struct bnxt_re_qp *qp,
				  const struct ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_ah *ah = NULL;

	if(is_ud_qp(qp)) {
		ah = to_bnxt_re(ud_wr(wr)->ah, struct bnxt_re_ah, ibah);
		wqe->send.q_key = ud_wr(wr)->remote_qkey;
		wqe->send.dst_qp = ud_wr(wr)->remote_qpn;
		wqe->send.avid = ah->qplib_ah.id;
	}
	switch (wr->opcode) {
	case IB_WR_SEND:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_SEND;
		break;
	case IB_WR_SEND_WITH_IMM:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM;
		wqe->send.imm_data = wr->ex.imm_data;
		break;
	case IB_WR_SEND_WITH_INV:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_SEND_WITH_INV;
		wqe->send.inv_key = wr->ex.invalidate_rkey;
		break;
	default:
		dev_err(rdev_to_dev(qp->rdev), "%s Invalid opcode %d!\n",
			__func__, wr->opcode);
		return -EINVAL;
	}
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;
	if (wr->send_flags & IB_SEND_INLINE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_INLINE;

	return 0;
}

static int bnxt_re_build_rdma_wqe(const struct ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	switch (wr->opcode) {
	case IB_WR_RDMA_WRITE:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE;
		break;
	case IB_WR_RDMA_WRITE_WITH_IMM:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM;
		wqe->rdma.imm_data = wr->ex.imm_data;
		break;
	case IB_WR_RDMA_READ:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_RDMA_READ;
		wqe->rdma.inv_key = wr->ex.invalidate_rkey;
		break;
	default:
		return -EINVAL;
	}
	wqe->rdma.remote_va = rdma_wr(wr)->remote_addr;
	wqe->rdma.r_key = rdma_wr(wr)->rkey;
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;
	if (wr->send_flags & IB_SEND_INLINE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_INLINE;

	return 0;
}

static int bnxt_re_build_atomic_wqe(const struct ib_send_wr *wr,
				    struct bnxt_qplib_swqe *wqe)
{
	switch (wr->opcode) {
	case IB_WR_ATOMIC_CMP_AND_SWP:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_ATOMIC_CMP_AND_SWP;
		wqe->atomic.cmp_data = atomic_wr(wr)->compare_add;
		wqe->atomic.swap_data = atomic_wr(wr)->swap;
		break;
	case IB_WR_ATOMIC_FETCH_AND_ADD:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_ATOMIC_FETCH_AND_ADD;
		wqe->atomic.cmp_data = atomic_wr(wr)->compare_add;
		break;
	default:
		return -EINVAL;
	}
	wqe->atomic.remote_va = atomic_wr(wr)->remote_addr;
	wqe->atomic.r_key = atomic_wr(wr)->rkey;
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;
	return 0;
}

static int bnxt_re_build_inv_wqe(const struct ib_send_wr *wr,
				 struct bnxt_qplib_swqe *wqe)
{
	wqe->type = BNXT_QPLIB_SWQE_TYPE_LOCAL_INV;
	wqe->local_inv.inv_l_key = wr->ex.invalidate_rkey;
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;

	return 0;
}

static int bnxt_re_build_reg_wqe(const struct ib_reg_wr *wr,
				 struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_mr *mr = to_bnxt_re(wr->mr, struct bnxt_re_mr, ib_mr);
	struct bnxt_qplib_frpl *qplib_frpl = &mr->qplib_frpl;
	int reg_len, i, access = wr->access;

	if (mr->npages > qplib_frpl->max_pg_ptrs) {
		dev_err_ratelimited(rdev_to_dev(mr->rdev),
			" %s: failed npages %d > %d\n", __func__,
			mr->npages, qplib_frpl->max_pg_ptrs);
		return -EINVAL;
	}

	wqe->frmr.pbl_ptr = (__le64 *)qplib_frpl->hwq.pbl_ptr[0];
	wqe->frmr.pbl_dma_ptr = qplib_frpl->hwq.pbl_dma_ptr[0];
	wqe->frmr.levels = qplib_frpl->hwq.level;
	wqe->frmr.page_list = mr->pages;
	wqe->frmr.page_list_len = mr->npages;
	wqe->type = BNXT_QPLIB_SWQE_TYPE_REG_MR;

	if (wr->wr.send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (access & IB_ACCESS_LOCAL_WRITE)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_LOCAL_WRITE;
	if (access & IB_ACCESS_REMOTE_READ)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_REMOTE_READ;
	if (access & IB_ACCESS_REMOTE_WRITE)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_REMOTE_WRITE;
	if (access & IB_ACCESS_REMOTE_ATOMIC)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_REMOTE_ATOMIC;
	if (access & IB_ACCESS_MW_BIND)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_WINDOW_BIND;

	/* TODO: OFED provides the rkey of the MR instead of the lkey */
	wqe->frmr.l_key = wr->key;
	wqe->frmr.length = wr->mr->length;
	wqe->frmr.pbl_pg_sz_log = ilog2(PAGE_SIZE >> PAGE_SHIFT_4K);
	wqe->frmr.pg_sz_log = ilog2(wr->mr->page_size >> PAGE_SHIFT_4K);
	wqe->frmr.va = wr->mr->iova;
	reg_len = wqe->frmr.page_list_len * wr->mr->page_size;

	if (wqe->frmr.length > reg_len) {
		dev_err_ratelimited(rdev_to_dev(mr->rdev),
				    "%s: bnxt_re_mr 0x%px  len (%d > %d)\n",
				    __func__, (void *)mr, wqe->frmr.length,
				    reg_len);

		for (i = 0; i < mr->npages; i++)
			dev_dbg(rdev_to_dev(mr->rdev),
				"%s: build_reg_wqe page[%d] = 0x%llx\n",
				__func__, i, mr->pages[i]);

		return -EINVAL;
	}

	return 0;
}

static void bnxt_re_set_sg_list(const struct ib_send_wr *wr,
				struct bnxt_qplib_swqe *wqe)
{
	wqe->sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
	wqe->num_sge = wr->num_sge;
}

static void bnxt_ud_qp_hw_stall_workaround(struct bnxt_re_qp *qp)
{
	if ((qp->ib_qp.qp_type == IB_QPT_UD || qp->ib_qp.qp_type == IB_QPT_GSI ||
	    qp->ib_qp.qp_type == IB_QPT_RAW_ETHERTYPE) &&
	    qp->qplib_qp.wqe_cnt == BNXT_RE_UD_QP_HW_STALL) {
		int qp_attr_mask;
		struct ib_qp_attr qp_attr;

		qp_attr_mask = IB_QP_STATE;
		qp_attr.qp_state = IB_QPS_RTS;
		bnxt_re_modify_qp(&qp->ib_qp, &qp_attr, qp_attr_mask, NULL);
		qp->qplib_qp.wqe_cnt = 0;
	}
}

static int bnxt_re_post_send_shadow_qp(struct bnxt_re_dev *rdev,
				       struct bnxt_re_qp *qp,
				       const struct ib_send_wr *wr)
{
	struct bnxt_qplib_swqe wqe;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&qp->sq_lock, flags);
	while (wr) {
		/* House keeping */
		memset(&wqe, 0, sizeof(wqe));
		/* Common */
		if (wr->num_sge > qp->qplib_qp.sq.max_sge) {
			dev_err(rdev_to_dev(rdev),
				"Limit exceeded for Send SGEs\n");
			rc = -EINVAL;
			break;
		}

		bnxt_re_set_sg_list(wr, &wqe);
		wqe.wr_id = wr->wr_id;
		wqe.type = BNXT_QPLIB_SWQE_TYPE_SEND;
		rc = bnxt_re_build_send_wqe(qp, wr, &wqe);
		if (rc)
			break;

		rc = bnxt_qplib_post_send(&qp->qplib_qp, &wqe);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"bad_wr seen with opcode = 0x%x rc = %d\n",
				wr->opcode, rc);
			break;
		}
		wr = wr->next;
	}
	bnxt_qplib_post_send_db(&qp->qplib_qp);
	bnxt_ud_qp_hw_stall_workaround(qp);
	spin_unlock_irqrestore(&qp->sq_lock, flags);
	return rc;
}

static void bnxt_re_legacy_set_uc_fence(struct bnxt_qplib_swqe *wqe)
{
	/* Need unconditional fence for non-wire memory opcode
	 * to work as expected.
	 */
	if (wqe->type == BNXT_QPLIB_SWQE_TYPE_LOCAL_INV ||
	    wqe->type == BNXT_QPLIB_SWQE_TYPE_FAST_REG_MR ||
	    wqe->type == BNXT_QPLIB_SWQE_TYPE_REG_MR ||
	    wqe->type == BNXT_QPLIB_SWQE_TYPE_BIND_MW)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
}

int bnxt_re_post_send(struct ib_qp *ib_qp, const struct ib_send_wr *wr,
		      const struct ib_send_wr **bad_wr)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_qplib_sge sge[6];
	struct bnxt_qplib_swqe wqe;
	struct bnxt_re_dev *rdev;
	unsigned long flags;
	int rc = 0;

	rdev = qp->rdev;
	spin_lock_irqsave(&qp->sq_lock, flags);
	while (wr) {
		/* House keeping */
		memset(&wqe, 0, sizeof(wqe));
		/* Common */
		if (wr->num_sge > qp->qplib_qp.sq.max_sge) {
			dev_err(rdev_to_dev(rdev),
				"Limit exceeded for Send SGEs\n");
			rc = -EINVAL;
			goto bad;
		}

		bnxt_re_set_sg_list(wr, &wqe);
		wqe.wr_id = wr->wr_id;

		switch (wr->opcode) {
		case IB_WR_SEND:
		case IB_WR_SEND_WITH_IMM:
			if (ib_qp->qp_type == IB_QPT_GSI &&
			    rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
				memset(sge, 0, sizeof(sge));
				wqe.sg_list = sge;
				rc = bnxt_re_build_gsi_send(qp, wr, &wqe);
				if (rc)
					goto bad;
			} else if (ib_qp->qp_type == IB_QPT_RAW_ETHERTYPE) {
				bnxt_re_build_raw_send(wr, &wqe);
			}
			switch (wr->send_flags) {
			case IB_SEND_IP_CSUM:
				wqe.rawqp1.lflags |=
					SQ_SEND_RAWETH_QP1_LFLAGS_IP_CHKSUM;
				break;
			default:
				break;
			}
			fallthrough;
		case IB_WR_SEND_WITH_INV:
			rc = bnxt_re_build_send_wqe(qp, wr, &wqe);
			break;
		case IB_WR_RDMA_WRITE:
		case IB_WR_RDMA_WRITE_WITH_IMM:
		case IB_WR_RDMA_READ:
			rc = bnxt_re_build_rdma_wqe(wr, &wqe);
			break;
		case IB_WR_ATOMIC_CMP_AND_SWP:
		case IB_WR_ATOMIC_FETCH_AND_ADD:
			rc = bnxt_re_build_atomic_wqe(wr, &wqe);
			break;
		case IB_WR_RDMA_READ_WITH_INV:
			dev_err(rdev_to_dev(rdev),
				"RDMA Read with Invalidate is not supported\n");
			rc = -EINVAL;
			goto bad;
		case IB_WR_LOCAL_INV:
			rc = bnxt_re_build_inv_wqe(wr, &wqe);
			break;
		case IB_WR_REG_MR:
			rc = bnxt_re_build_reg_wqe(reg_wr(wr), &wqe);
			break;
		default:
			/* Unsupported WRs */
			dev_err(rdev_to_dev(rdev),
				"WR (0x%x) is not supported\n", wr->opcode);
			rc = -EINVAL;
			goto bad;
		}

		if (likely(!rc)) {
			if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
				bnxt_re_legacy_set_uc_fence(&wqe);
			rc = bnxt_qplib_post_send(&qp->qplib_qp, &wqe);
		}
bad:
		if (unlikely(rc)) {
			dev_err(rdev_to_dev(rdev),
				"bad_wr seen with opcode = 0x%x\n", wr->opcode);
			*bad_wr = wr;
			break;
		}
		wr = wr->next;
	}
	bnxt_qplib_post_send_db(&qp->qplib_qp);
	if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
		bnxt_ud_qp_hw_stall_workaround(qp);
	spin_unlock_irqrestore(&qp->sq_lock, flags);

	return rc;
}

static int bnxt_re_post_recv_shadow_qp(struct bnxt_re_dev *rdev,
				struct bnxt_re_qp *qp,
				struct ib_recv_wr *wr)
{
	struct bnxt_qplib_swqe wqe;
	int rc = 0;

	/* rq lock can be pardoned here. */
	while (wr) {
		/* House keeping */
		memset(&wqe, 0, sizeof(wqe));
		/* Common */
		if (wr->num_sge > qp->qplib_qp.rq.max_sge) {
			dev_err(rdev_to_dev(rdev),
				"Limit exceeded for Receive SGEs\n");
			rc = -EINVAL;
			goto bad;
		}

		wqe.sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
		wqe.num_sge = wr->num_sge;
		wqe.wr_id = wr->wr_id;
		wqe.type = BNXT_QPLIB_SWQE_TYPE_RECV;
		rc = bnxt_qplib_post_recv(&qp->qplib_qp, &wqe);
bad:
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"bad_wr seen with RQ post\n");
			break;
		}
		wr = wr->next;
	}
	bnxt_qplib_post_recv_db(&qp->qplib_qp);
	return rc;
}

static int bnxt_re_build_gsi_recv(struct bnxt_re_qp *qp,
				  const struct ib_recv_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_dev *rdev = qp->rdev;
	int rc = 0;

	if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL)
		rc = bnxt_re_build_qp1_shadow_qp_recv(qp, wr, wqe);
	else
		rc = bnxt_re_build_qp1_recv(qp, wr, wqe);

	return rc;
}

int bnxt_re_post_recv(struct ib_qp *ib_qp, const struct ib_recv_wr *wr,
		      const struct ib_recv_wr **bad_wr)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_qplib_sge sge[6];
	struct bnxt_qplib_swqe wqe;
	unsigned long flags;
	u32 count = 0;
	int rc = 0;

	spin_lock_irqsave(&qp->rq_lock, flags);
	while (wr) {
		memset(&wqe, 0, sizeof(wqe));
		if (wr->num_sge > qp->qplib_qp.rq.max_sge) {
			dev_err(rdev_to_dev(qp->rdev),
				"Limit exceeded for Receive SGEs\n");
			rc = -EINVAL;
			goto bad;
		}
		wqe.num_sge = wr->num_sge;
		wqe.sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
		wqe.wr_id = wr->wr_id;
		wqe.type = BNXT_QPLIB_SWQE_TYPE_RECV;

		if (ib_qp->qp_type == IB_QPT_GSI &&
		    qp->rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
			memset(sge, 0, sizeof(sge));
			wqe.sg_list = sge;
			rc = bnxt_re_build_gsi_recv(qp, wr, &wqe);
			if (rc)
				goto bad;
		}
		rc = bnxt_qplib_post_recv(&qp->qplib_qp, &wqe);
bad:
		if (rc) {
			dev_err(rdev_to_dev(qp->rdev),
				"bad_wr seen with RQ post\n");
			*bad_wr = wr;
			break;
		}
		/* Ring DB if the RQEs posted reaches a threshold value */
		if (++count >= BNXT_RE_RQ_WQE_THRESHOLD) {
			bnxt_qplib_post_recv_db(&qp->qplib_qp);
			count = 0;
		}
		wr = wr->next;
	}

	if (count)
		bnxt_qplib_post_recv_db(&qp->qplib_qp);
	spin_unlock_irqrestore(&qp->rq_lock, flags);

	return rc;
}

/* Completion Queues */
void bnxt_re_destroy_cq(struct ib_cq *ib_cq, struct ib_udata *udata)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ibcq);
	struct bnxt_re_dev *rdev = cq->rdev;
	int rc =  0;

	if (cq->uctx_cq_page) {
		BNXT_RE_CQ_PAGE_LIST_DEL(cq->uctx, cq);
		free_page((u64)cq->uctx_cq_page);
		cq->uctx_cq_page = NULL;
	}
	if (cq->is_dbr_soft_cq && cq->uctx) {
		void *dbr_page;

		if (cq->uctx->dbr_recov_cq) {
			dbr_page = cq->uctx->dbr_recov_cq_page;
			cq->uctx->dbr_recov_cq_page = NULL;
			cq->uctx->dbr_recov_cq = NULL;
			free_page((unsigned long)dbr_page);
		}
		goto end;
	}
	/* CQ getting destroyed. Set this state for cqn handler */
	spin_lock_bh(&cq->qplib_cq.compl_lock);
	cq->qplib_cq.destroyed = true;
	spin_unlock_bh(&cq->qplib_cq.compl_lock);
	if (ib_cq->poll_ctx == IB_POLL_WORKQUEUE ||
	    ib_cq->poll_ctx == IB_POLL_UNBOUND_WORKQUEUE)
		cancel_work_sync(&ib_cq->work);

	rc = bnxt_qplib_destroy_cq(&rdev->qplib_res, &cq->qplib_cq);
	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev),
				   "%s id = %d failed rc = %d\n",
				   __func__, cq->qplib_cq.id, rc);

	bnxt_re_put_nq(rdev, cq->qplib_cq.nq);
	if (cq->umem && !IS_ERR(cq->umem))
		ib_umem_release(cq->umem);

	kfree(cq->cql);
	atomic_dec(&rdev->stats.rsors.cq_count);
end:
	return;
}

static inline struct
bnxt_re_cq *__get_cq_from_cq_in(struct ib_cq *cq_in,
				struct bnxt_re_dev *rdev)
{
	struct bnxt_re_cq *cq;
	cq = container_of(cq_in, struct bnxt_re_cq, ibcq);
	return cq;
}

int bnxt_re_create_cq(struct ib_cq *cq_in,
		      const struct ib_cq_init_attr *attr,
		      struct ib_udata *udata)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *uctx = NULL;
	struct ib_ucontext *context = NULL;
	struct bnxt_qplib_cq *qplcq;
	struct bnxt_re_cq_req ureq;
	struct bnxt_re_dev *rdev;
	int rc, entries;
	struct bnxt_re_cq *cq;
	u32 max_active_cqs;
	int cqe = attr->cqe;

	if (attr->flags)
		return -EOPNOTSUPP;

	rdev = rdev_from_cq_in(cq_in);
	if (rdev->mod_exit) {
		rc = -EIO;
		dev_dbg(rdev_to_dev(rdev), "%s(): in mod_exit, just return!\n", __func__);
		goto exit;
	}
	if (udata) {
		uctx = rdma_udata_to_drv_context(udata,
						 struct bnxt_re_ucontext,
						 ibucontext);
		context = &uctx->ibucontext;
	}
	dev_attr = rdev->dev_attr;

	if (atomic_read(&rdev->stats.rsors.cq_count) >= dev_attr->max_cq) {
		dev_err(rdev_to_dev(rdev), "Create CQ failed - max exceeded(CQs)\n");
		rc = -EINVAL;
		goto exit;
	}
	/* Validate CQ fields */
	if (cqe < 1 || cqe > dev_attr->max_cq_wqes) {
		dev_err(rdev_to_dev(rdev), "Create CQ failed - max exceeded(CQ_WQs)\n");
		rc = -EINVAL;
		goto exit;
	}

	cq = __get_cq_from_cq_in(cq_in, rdev);
	if (!cq) {
		rc = -ENOMEM;
		goto exit;
	}
	cq->rdev = rdev;
	cq->uctx = uctx;
	qplcq = &cq->qplib_cq;
	qplcq->cq_handle = (u64)qplcq;
	/*
	 * Since CQ is for QP1 is shared with Shadow CQ, the size
	 * should be double the size. There is no way to identify
	 * whether this CQ is for GSI QP. So assuming that the first
	 * CQ created is for QP1
	 */
	if (!udata && !rdev->gsi_ctx.first_cq_created &&
	    rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL) {
		rdev->gsi_ctx.first_cq_created = true;
		/*
		 * Total CQE required for the CQ = CQE for QP1 RQ +
		 * CQE for Shadow QP SQEs + CQE for Shadow QP RQEs.
		 * Max entries of shadow QP SQ and RQ = QP1 RQEs = cqe
		 */
		cqe *= 3;
	}

	entries = bnxt_re_init_depth(cqe + 1, uctx);
	if (entries > dev_attr->max_cq_wqes + 1)
		entries = dev_attr->max_cq_wqes + 1;

	qplcq->sginfo.pgshft = PAGE_SHIFT;
	qplcq->sginfo.pgsize = PAGE_SIZE;
	if (udata) {
		if (udata->inlen < sizeof(ureq))
			dev_warn(rdev_to_dev(rdev),
				 "Update the library ulen %d klen %d\n",
				 (unsigned int)udata->inlen,
				 (unsigned int)sizeof(ureq));

		rc = ib_copy_from_udata(&ureq, udata,
					min(udata->inlen, sizeof(ureq)));
		if (rc)
			goto fail;

		if (BNXT_RE_IS_DBR_PACING_NOTIFY_CQ(ureq)) {
			cq->is_dbr_soft_cq = true;
			goto success;
		}

		if (BNXT_RE_IS_DBR_RECOV_CQ(ureq)) {
			void *dbr_page;
			u32 *epoch;

			dbr_page = (void *)__get_free_page(GFP_KERNEL);
			if (!dbr_page) {
				dev_err(rdev_to_dev(rdev),
					"DBR recov CQ page allocation failed!");
				rc = -ENOMEM;
				goto fail;
			}

			/* memset the epoch and epoch_ack to 0 */
			epoch = dbr_page;
			epoch[0] = 0x0;
			epoch[1] = 0x0;

			uctx->dbr_recov_cq = cq;
			uctx->dbr_recov_cq_page = dbr_page;

			cq->is_dbr_soft_cq = true;
			goto success;
		}

		cq->umem = ib_umem_get_compat
				      (rdev, context, udata, ureq.cq_va,
				       entries * sizeof(struct cq_base),
				       IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(cq->umem)) {
			rc = PTR_ERR(cq->umem);
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed! rc = %d\n",
				__func__, rc);
			goto fail;
		}
		qplcq->sginfo.sghead = get_ib_umem_sgl(cq->umem,
						       &qplcq->sginfo.nmap);
		qplcq->sginfo.npages = ib_umem_num_pages_compat(cq->umem);
		if (!uctx->dpi.dbr) {
			rc = bnxt_re_get_user_dpi(rdev, uctx);
			if (rc)
				goto c2fail;
		}
		qplcq->dpi = &uctx->dpi;
	} else {
		cq->max_cql = entries > MAX_CQL_PER_POLL ? MAX_CQL_PER_POLL : entries;
		cq->cql = kcalloc(cq->max_cql, sizeof(struct bnxt_qplib_cqe),
				  GFP_KERNEL);
		if (!cq->cql) {
			dev_err(rdev_to_dev(rdev),
				"Allocate CQL for %d failed!\n", cq->max_cql);
			rc = -ENOMEM;
			goto fail;
		}
		qplcq->dpi = &rdev->dpi_privileged;
	}
	/*
	 * Allocating the NQ in a round robin fashion. nq_alloc_cnt is a
	 * used for getting the NQ index.
	 */
	qplcq->max_wqe = entries;
	qplcq->nq = bnxt_re_get_nq(rdev);
	qplcq->cnq_hw_ring_id = qplcq->nq->ring_id;

	rc = bnxt_qplib_create_cq(&rdev->qplib_res, qplcq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Create HW CQ failed!\n");
		goto fail;
	}

	INIT_LIST_HEAD(&cq->cq_list);
	cq->ibcq.cqe = entries;
	cq->cq_period = qplcq->period;

	atomic_inc(&rdev->stats.rsors.cq_count);
	max_active_cqs = atomic_read(&rdev->stats.rsors.cq_count);
	if (max_active_cqs > atomic_read(&rdev->stats.rsors.max_cq_count))
		atomic_set(&rdev->stats.rsors.max_cq_count, max_active_cqs);
	spin_lock_init(&cq->cq_lock);

	if (udata) {
		struct bnxt_re_cq_resp resp;

		resp.cqid = qplcq->id;
		resp.tail = qplcq->hwq.cons;
		resp.phase = qplcq->period;
		resp.comp_mask = 0;
		resp.dbr = (u64)uctx->dpi.umdbr;
		resp.dpi = uctx->dpi.dpi;
		resp.comp_mask |= BNXT_RE_COMP_MASK_CQ_HAS_DB_INFO;
		/* Copy only on a valid wcpdi */
		if (uctx->wcdpi.dpi) {
			resp.wcdpi = uctx->wcdpi.dpi;
			resp.comp_mask |= BNXT_RE_COMP_MASK_CQ_HAS_WC_DPI;
		}

		if (_is_chip_p7(rdev->chip_ctx)) {
			cq->uctx_cq_page = (void *)__get_free_page(GFP_KERNEL);

			if (!cq->uctx_cq_page) {
				dev_err(rdev_to_dev(rdev),
					"CQ page allocation failed!\n");
				bnxt_qplib_destroy_cq(&rdev->qplib_res, qplcq);
				rc = -ENOMEM;
				goto c2fail;
			}

			resp.uctx_cq_page = (u64)cq->uctx_cq_page;
			resp.comp_mask |= BNXT_RE_COMP_MASK_CQ_HAS_CQ_PAGE;
		}

		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc) {
			free_page((u64)cq->uctx_cq_page);
			cq->uctx_cq_page = NULL;
			bnxt_qplib_destroy_cq(&rdev->qplib_res, qplcq);
			goto c2fail;
		}

		if (cq->uctx_cq_page)
			BNXT_RE_CQ_PAGE_LIST_ADD(uctx, cq);
	}

success:
	return 0;
c2fail:
	if (udata && cq->umem && !IS_ERR(cq->umem))
		ib_umem_release(cq->umem);
fail:
	if (cq) {
		if (cq->cql)
			kfree(cq->cql);
	}
exit:
	return rc;
}

int bnxt_re_modify_cq(struct ib_cq *ib_cq, u16 cq_count, u16 cq_period)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ibcq);
	struct bnxt_re_dev *rdev = cq->rdev;
	int rc;

	if ((cq->cq_count != cq_count) || (cq->cq_period != cq_period)) {
		cq->qplib_cq.count = cq_count;
		cq->qplib_cq.period = cq_period;
		rc = bnxt_qplib_modify_cq(&rdev->qplib_res, &cq->qplib_cq);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Modify HW CQ %#x failed!\n",
				cq->qplib_cq.id);
			return rc;
		}
		/* On success, update the shadow */
		cq->cq_count = cq_count;
		cq->cq_period = cq_period;
	}
	return 0;
}

static void bnxt_re_resize_cq_complete(struct bnxt_re_cq *cq)
{
	struct bnxt_re_dev *rdev = cq->rdev;

	bnxt_qplib_resize_cq_complete(&rdev->qplib_res, &cq->qplib_cq);

	cq->qplib_cq.max_wqe = cq->resize_cqe;
	if (cq->resize_umem) {
		ib_umem_release(cq->umem);
		cq->umem = cq->resize_umem;
		cq->resize_umem = NULL;
		cq->resize_cqe = 0;
	}
}

int bnxt_re_resize_cq(struct ib_cq *ib_cq, int cqe, struct ib_udata *udata)
{
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_qplib_dpi *orig_dpi = NULL;
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *uctx = NULL;
	struct bnxt_re_resize_cq_req ureq;
	struct ib_ucontext *context = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_cq *cq;
	int rc, entries;

	/* Don't allow more than one resize request at the same time.
	 * TODO: need a mutex here when we support kernel consumers of resize.
	 */
	cq =  to_bnxt_re(ib_cq, struct bnxt_re_cq, ibcq);
	rdev = cq->rdev;
	dev_attr = rdev->dev_attr;
	if (ib_cq->uobject) {
		uctx = rdma_udata_to_drv_context(udata,
						 struct bnxt_re_ucontext,
						 ibucontext);
		context = &uctx->ibucontext;
	}

	if (cq->resize_umem) {
		dev_err(rdev_to_dev(rdev), "Resize CQ %#x failed - Busy\n",
			cq->qplib_cq.id);
		return -EBUSY;
	}

	/* Check the requested cq depth out of supported depth */
	if (cqe < 1 || cqe > dev_attr->max_cq_wqes) {
		dev_err(rdev_to_dev(rdev), "Resize CQ %#x failed - max exceeded\n",
			cq->qplib_cq.id);
		return -EINVAL;
	}

	entries = bnxt_re_init_depth(cqe + 1, uctx);
	entries = min_t(u32, (u32)entries, dev_attr->max_cq_wqes + 1);

	/* Check to see if the new requested size can be handled by already
	 * existing CQ
	 */
	if (entries == cq->ibcq.cqe) {
		dev_info(rdev_to_dev(rdev), "CQ is already at size %d\n", cqe);
		return 0;
	}

	if (ib_cq->uobject && udata) {
		if (udata->inlen < sizeof(ureq))
			dev_warn(rdev_to_dev(rdev),
				 "Update the library ulen %d klen %d\n",
				 (unsigned int)udata->inlen,
				 (unsigned int)sizeof(ureq));

		rc = ib_copy_from_udata(&ureq, udata,
					min(udata->inlen, sizeof(ureq)));
		if (rc)
			goto fail;

		dev_dbg(rdev_to_dev(rdev), "%s: va %p\n", __func__,
			(void *)ureq.cq_va);
		cq->resize_umem = ib_umem_get_compat
				       (rdev,
					context, udata, ureq.cq_va,
					entries * sizeof(struct cq_base),
					IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(cq->resize_umem)) {
			rc = PTR_ERR(cq->resize_umem);
			cq->resize_umem = NULL;
			dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed! rc = %d\n",
				__func__, rc);
			goto fail;
		}
		cq->resize_cqe = entries;
		dev_dbg(rdev_to_dev(rdev), "%s: ib_umem_get() success\n",
			__func__);
		memcpy(&sginfo, &cq->qplib_cq.sginfo, sizeof(sginfo));
		orig_dpi = cq->qplib_cq.dpi;

		cq->qplib_cq.sginfo.sghead = get_ib_umem_sgl(cq->resize_umem,
						&cq->qplib_cq.sginfo.nmap);
		cq->qplib_cq.sginfo.npages =
				ib_umem_num_pages_compat(cq->resize_umem);
		cq->qplib_cq.sginfo.pgsize = PAGE_SIZE;
		cq->qplib_cq.sginfo.pgshft = PAGE_SHIFT;
		cq->qplib_cq.dpi = &uctx->dpi;
	} else {
		/* TODO: kernel consumer */
	}

	rc = bnxt_qplib_resize_cq(&rdev->qplib_res, &cq->qplib_cq, entries);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Resize HW CQ %#x failed!\n",
			cq->qplib_cq.id);
		goto fail;
	}

	cq->ibcq.cqe = cq->resize_cqe;
	/* For kernel consumers complete resize here. For uverbs consumers,
	 * we complete it in the context of ibv_poll_cq().
	 */
	if (!cq->resize_umem)
		bnxt_qplib_resize_cq_complete(&rdev->qplib_res, &cq->qplib_cq);

	atomic_inc(&rdev->stats.rsors.resize_count);
	return 0;

fail:
	if (cq->resize_umem) {
		ib_umem_release(cq->resize_umem);
		cq->resize_umem = NULL;
		cq->resize_cqe = 0;
		memcpy(&cq->qplib_cq.sginfo, &sginfo, sizeof(sginfo));
		cq->qplib_cq.dpi = orig_dpi;
	}
	return rc;
}

static enum ib_wc_status __req_to_ib_wc_status(u8 qstatus)
{
	switch(qstatus) {
	case CQ_REQ_STATUS_OK:
		return IB_WC_SUCCESS;
	case CQ_REQ_STATUS_BAD_RESPONSE_ERR:
		return IB_WC_BAD_RESP_ERR;
	case CQ_REQ_STATUS_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case CQ_REQ_STATUS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case CQ_REQ_STATUS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case CQ_REQ_STATUS_MEMORY_MGT_OPERATION_ERR:
		return IB_WC_GENERAL_ERR;
	case CQ_REQ_STATUS_REMOTE_INVALID_REQUEST_ERR:
		return IB_WC_REM_INV_REQ_ERR;
	case CQ_REQ_STATUS_REMOTE_ACCESS_ERR:
		return IB_WC_REM_ACCESS_ERR;
	case CQ_REQ_STATUS_REMOTE_OPERATION_ERR:
		return IB_WC_REM_OP_ERR;
	case CQ_REQ_STATUS_RNR_NAK_RETRY_CNT_ERR:
		return IB_WC_RNR_RETRY_EXC_ERR;
	case CQ_REQ_STATUS_TRANSPORT_RETRY_CNT_ERR:
		return IB_WC_RETRY_EXC_ERR;
	case CQ_REQ_STATUS_WORK_REQUEST_FLUSHED_ERR:
		return IB_WC_WR_FLUSH_ERR;
	default:
		return IB_WC_GENERAL_ERR;
	}
	return 0;
}

static enum ib_wc_status __rawqp1_to_ib_wc_status(u8 qstatus)
{
	switch(qstatus) {
	case CQ_RES_RAWETH_QP1_STATUS_OK:
		return IB_WC_SUCCESS;
	case CQ_RES_RAWETH_QP1_STATUS_LOCAL_ACCESS_ERROR:
		return IB_WC_LOC_ACCESS_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_HW_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_MEMORY_MGT_OPERATION_ERR:
		return IB_WC_GENERAL_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_WORK_REQUEST_FLUSHED_ERR:
		return IB_WC_WR_FLUSH_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_HW_FLUSH_ERR:
		return IB_WC_WR_FLUSH_ERR;
	default:
		return IB_WC_GENERAL_ERR;
	}
}

static enum ib_wc_status __rc_to_ib_wc_status(u8 qstatus)
{
	switch(qstatus) {
	case CQ_RES_RC_STATUS_OK:
		return IB_WC_SUCCESS;
	case CQ_RES_RC_STATUS_LOCAL_ACCESS_ERROR:
		return IB_WC_LOC_ACCESS_ERR;
	case CQ_RES_RC_STATUS_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case CQ_RES_RC_STATUS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case CQ_RES_RC_STATUS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case CQ_RES_RC_STATUS_MEMORY_MGT_OPERATION_ERR:
		return IB_WC_GENERAL_ERR;
	case CQ_RES_RC_STATUS_REMOTE_INVALID_REQUEST_ERR:
		return IB_WC_REM_INV_REQ_ERR;
	case CQ_RES_RC_STATUS_WORK_REQUEST_FLUSHED_ERR:
		return IB_WC_WR_FLUSH_ERR;
	case CQ_RES_RC_STATUS_HW_FLUSH_ERR:
		return IB_WC_WR_FLUSH_ERR;
	default:
		return IB_WC_GENERAL_ERR;
	}
}

static void bnxt_re_process_req_wc(struct ib_wc *wc, struct bnxt_qplib_cqe *cqe)
{
	switch (cqe->type) {
	case BNXT_QPLIB_SWQE_TYPE_SEND:
		wc->opcode = IB_WC_SEND;
		break;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM:
		wc->opcode = IB_WC_SEND;
		wc->wc_flags |= IB_WC_WITH_IMM;
		break;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_INV:
		wc->opcode = IB_WC_SEND;
		wc->wc_flags |= IB_WC_WITH_INVALIDATE;
		break;
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE:
		wc->opcode = IB_WC_RDMA_WRITE;
		break;
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM:
		wc->opcode = IB_WC_RDMA_WRITE;
		wc->wc_flags |= IB_WC_WITH_IMM;
		break;
	case BNXT_QPLIB_SWQE_TYPE_RDMA_READ:
		wc->opcode = IB_WC_RDMA_READ;
		break;
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_CMP_AND_SWP:
		wc->opcode = IB_WC_COMP_SWAP;
		break;
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_FETCH_AND_ADD:
		wc->opcode = IB_WC_FETCH_ADD;
		break;
	case BNXT_QPLIB_SWQE_TYPE_LOCAL_INV:
		wc->opcode = IB_WC_LOCAL_INV;
		break;
	case BNXT_QPLIB_SWQE_TYPE_REG_MR:
		wc->opcode = IB_WC_REG_MR;
		break;
	default:
		wc->opcode = IB_WC_SEND;
		break;
	}

	wc->status = __req_to_ib_wc_status(cqe->status);
}

static int bnxt_re_check_packet_type(u16 raweth_qp1_flags, u16 raweth_qp1_flags2)
{
	bool is_ipv6 = false, is_ipv4 = false;

	/* raweth_qp1_flags Bit 9-6 indicates itype */

	if ((raweth_qp1_flags & CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS_ITYPE_ROCE)
	    != CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS_ITYPE_ROCE)
		return -1;

	if (raweth_qp1_flags2 &
	    CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_IP_CS_CALC &&
	    raweth_qp1_flags2 &
	    CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_L4_CS_CALC) {
		/* raweth_qp1_flags2 Bit 8 indicates ip_type. 0-v4 1 - v6 */
		(raweth_qp1_flags2 &
		 CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_IP_TYPE) ?
			(is_ipv6 = true) : (is_ipv4 = true);
		return ((is_ipv6) ?
			 BNXT_RE_ROCEV2_IPV6_PACKET :
			 BNXT_RE_ROCEV2_IPV4_PACKET);
	} else {
		return BNXT_RE_ROCE_V1_PACKET;
	}
}

static bool bnxt_re_is_loopback_packet(struct bnxt_re_dev *rdev,
					    void *rq_hdr_buf)
{
	u8 *tmp_buf = NULL;
	struct ethhdr *eth_hdr;
	u16 eth_type;
	bool rc = false;

	tmp_buf = (u8 *)rq_hdr_buf;
	/*
	 * If dest mac is not same as I/F mac, this could be a
	 * loopback address or multicast address, check whether
	 * it is a loopback packet
	 */
	if (!ether_addr_equal(tmp_buf, rdev->dev_addr)) {
		tmp_buf += 4;
		/* Check the  ether type */
		eth_hdr = (struct ethhdr *)tmp_buf;
		eth_type = ntohs(eth_hdr->h_proto);
		switch (eth_type) {
		case BNXT_QPLIB_ETHTYPE_ROCEV1:
			rc = true;
			break;
		default:
			break;
		}
	}

	return rc;
}

static bool bnxt_re_is_vlan_in_packet(struct bnxt_re_dev *rdev,
				      void *rq_hdr_buf,
				      struct bnxt_qplib_cqe *cqe)
{
	struct vlan_hdr *vlan_hdr;
	struct ethhdr *eth_hdr;
	u8 *tmp_buf = NULL;
	u16 eth_type;

	tmp_buf = (u8 *)rq_hdr_buf;
	/* Check the  ether type */
	eth_hdr = (struct ethhdr *)tmp_buf;
	eth_type = ntohs(eth_hdr->h_proto);
	if (eth_type == ETH_P_8021Q) {
		tmp_buf += sizeof(struct ethhdr);
		vlan_hdr = (struct vlan_hdr *)tmp_buf;
		cqe->raweth_qp1_metadata =
			ntohs(vlan_hdr->h_vlan_TCI) |
			(eth_type <<
			 CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_TPID_SFT);
		cqe->raweth_qp1_flags2 |=
			CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_META_FORMAT_VLAN;
		return true;
	}

	return false;
}

static int bnxt_re_process_raw_qp_packet_receive(struct bnxt_re_qp *gsi_qp,
						 struct bnxt_qplib_cqe *cqe)
{
	struct bnxt_re_sqp_entries *sqp_entry = NULL;
	struct bnxt_qplib_hdrbuf *hdr_buf;
	dma_addr_t shrq_hdr_buf_map;
	struct ib_sge s_sge[2] = {};
	struct ib_sge r_sge[2] = {};
	struct ib_recv_wr rwr = {};
	struct bnxt_re_ah *gsi_sah;
	struct bnxt_re_qp *gsi_sqp;
	dma_addr_t rq_hdr_buf_map;
	struct bnxt_re_dev *rdev;
	struct ib_send_wr *swr;
	u32 skip_bytes = 0;
	void *rq_hdr_buf;
	int pkt_type = 0;
	u32 offset = 0;
	u32 tbl_idx;
	int rc;
	struct ib_ud_wr udwr = {};

	swr = &udwr.wr;
	rdev = gsi_qp->rdev;
	gsi_sqp = rdev->gsi_ctx.gsi_sqp;
	tbl_idx = cqe->wr_id;

	hdr_buf = gsi_qp->qplib_qp.rq_hdr_buf;
	rq_hdr_buf = (u8 *) hdr_buf->va + tbl_idx * hdr_buf->step;
	rq_hdr_buf_map = bnxt_qplib_get_qp_buf_from_index(&gsi_qp->qplib_qp,
							  tbl_idx);
	/* Shadow QP header buffer */
	shrq_hdr_buf_map = bnxt_qplib_get_qp_buf_from_index(&gsi_sqp->qplib_qp,
							    tbl_idx);
	sqp_entry = &rdev->gsi_ctx.sqp_tbl[tbl_idx];

	/* Find packet type from the cqe */
	pkt_type = bnxt_re_check_packet_type(cqe->raweth_qp1_flags,
					     cqe->raweth_qp1_flags2);
	if (pkt_type < 0) {
		dev_err(rdev_to_dev(rdev), "Not handling this packet\n");
		return -EINVAL;
	}

	/* Adjust the offset for the user buffer and post in the rq */

	if (pkt_type == BNXT_RE_ROCEV2_IPV4_PACKET)
		offset = 20;

	/*
	 * QP1 loopback packet has 4 bytes of internal header before
	 * ether header. Skip these four bytes.
	 */
	if (bnxt_re_is_loopback_packet(rdev, rq_hdr_buf))
		skip_bytes = 4;

	if (bnxt_re_is_vlan_in_packet(rdev, rq_hdr_buf, cqe))
		skip_bytes += VLAN_HLEN;

	/* Store this cqe */
	memcpy(&sqp_entry->cqe, cqe, sizeof(struct bnxt_qplib_cqe));
	sqp_entry->qp1_qp = gsi_qp;

	/* First send SGE . Skip the ether header*/
	s_sge[0].addr = rq_hdr_buf_map + BNXT_QPLIB_MAX_QP1_RQ_ETH_HDR_SIZE
			+ skip_bytes;
	s_sge[0].lkey = 0xFFFFFFFF;
	s_sge[0].length = offset ? BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV4 :
				BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV6;

	/* Second Send SGE */
	s_sge[1].addr = s_sge[0].addr + s_sge[0].length +
			BNXT_QPLIB_MAX_QP1_RQ_BDETH_HDR_SIZE;
	if (pkt_type != BNXT_RE_ROCE_V1_PACKET)
		s_sge[1].addr += 8;
	s_sge[1].lkey = 0xFFFFFFFF;
	s_sge[1].length = 256;

	/* First recv SGE */
	r_sge[0].addr = shrq_hdr_buf_map;
	r_sge[0].lkey = 0xFFFFFFFF;
	r_sge[0].length = 40;

	r_sge[1].addr = sqp_entry->sge.addr + offset;
	r_sge[1].lkey = sqp_entry->sge.lkey;
	r_sge[1].length = BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV6 + 256 - offset;

	/* Create receive work request */
	rwr.num_sge = 2;
	rwr.sg_list = r_sge;
	rwr.wr_id = tbl_idx;
	rwr.next = NULL;

	rc = bnxt_re_post_recv_shadow_qp(rdev, gsi_sqp, &rwr);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to post Rx buffers to shadow QP\n");
		return -ENOMEM;
	}

	swr->num_sge = 2;
	swr->sg_list = s_sge;
	swr->wr_id = tbl_idx;
	swr->opcode = IB_WR_SEND;
	swr->next = NULL;

	gsi_sah = rdev->gsi_ctx.gsi_sah;
	udwr.ah = &gsi_sah->ibah;
	udwr.remote_qpn = gsi_sqp->qplib_qp.id;
	udwr.remote_qkey = gsi_sqp->qplib_qp.qkey;
	/* post data received in the send queue */
	rc = bnxt_re_post_send_shadow_qp(rdev, gsi_sqp, swr);

	return rc;
}

static void bnxt_re_process_res_rawqp1_wc(struct ib_wc *wc,
					  struct bnxt_qplib_cqe *cqe)
{
	wc->opcode = IB_WC_RECV;
	wc->status = __rawqp1_to_ib_wc_status(cqe->status);
	wc->wc_flags |= IB_WC_GRH;
}

static void bnxt_re_process_res_rc_wc(struct ib_wc *wc,
				      struct bnxt_qplib_cqe *cqe)
{
	wc->opcode = IB_WC_RECV;
	wc->status = __rc_to_ib_wc_status(cqe->status);

	if (cqe->flags & CQ_RES_RC_FLAGS_IMM)
		wc->wc_flags |= IB_WC_WITH_IMM;
	if (cqe->flags & CQ_RES_RC_FLAGS_INV)
		wc->wc_flags |= IB_WC_WITH_INVALIDATE;
	if ((cqe->flags & (CQ_RES_RC_FLAGS_RDMA | CQ_RES_RC_FLAGS_IMM)) ==
	    (CQ_RES_RC_FLAGS_RDMA | CQ_RES_RC_FLAGS_IMM))
		wc->opcode = IB_WC_RECV_RDMA_WITH_IMM;
}

/* Returns TRUE if pkt has valid VLAN and if VLAN id is non-zero */
static bool bnxt_re_is_nonzero_vlanid_pkt(struct bnxt_qplib_cqe *orig_cqe,
					  u16 *vid, u8 *sl)
{
	u32 metadata;
	u16 tpid;
	bool ret = false;
	metadata = orig_cqe->raweth_qp1_metadata;
	if (orig_cqe->raweth_qp1_flags2 &
	    CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_META_FORMAT_VLAN) {
		tpid = ((metadata &
			 CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_TPID_MASK) >>
			 CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_TPID_SFT);
		if (tpid == ETH_P_8021Q) {
			*vid = metadata &
			       CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_VID_MASK;
			*sl = (metadata &
			       CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_PRI_MASK) >>
			       CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_PRI_SFT;
			ret = !!(*vid);
		}
	}

	return ret;
}

static void bnxt_re_process_res_shadow_qp_wc(struct bnxt_re_qp *gsi_sqp,
					     struct ib_wc *wc,
					     struct bnxt_qplib_cqe *cqe)
{
	u32 tbl_idx;
	struct bnxt_re_dev *rdev = gsi_sqp->rdev;
	struct bnxt_re_qp *gsi_qp = NULL;
	struct bnxt_qplib_cqe *orig_cqe = NULL;
	struct bnxt_re_sqp_entries *sqp_entry = NULL;
	int nw_type;
	u16 vlan_id;
	u8 sl;

	tbl_idx = cqe->wr_id;

	sqp_entry = &rdev->gsi_ctx.sqp_tbl[tbl_idx];
	gsi_qp = sqp_entry->qp1_qp;
	orig_cqe = &sqp_entry->cqe;

	wc->wr_id = sqp_entry->wrid;
	wc->byte_len = orig_cqe->length;
	wc->qp = &gsi_qp->ib_qp;

	wc->ex.imm_data = orig_cqe->immdata;
	wc->src_qp = orig_cqe->src_qp;
	memcpy(wc->smac, orig_cqe->smac, ETH_ALEN);
	if (bnxt_re_is_nonzero_vlanid_pkt(orig_cqe, &vlan_id, &sl)) {
		if (bnxt_re_check_if_vlan_valid(rdev, vlan_id)) {
			wc->sl = sl;
			wc->vlan_id = vlan_id;
			wc->wc_flags |= IB_WC_WITH_VLAN;
		}
	}
	wc->port_num = 1;
	wc->vendor_err = orig_cqe->status;

	wc->opcode = IB_WC_RECV;
	wc->status = __rawqp1_to_ib_wc_status(orig_cqe->status);
	wc->wc_flags |= IB_WC_GRH;

	nw_type = bnxt_re_check_packet_type(orig_cqe->raweth_qp1_flags,
					    orig_cqe->raweth_qp1_flags2);
	if(nw_type >= 0)
		dev_dbg(rdev_to_dev(rdev), "%s nw_type = %d\n", __func__, nw_type);
}

static void bnxt_re_process_res_ud_wc(struct bnxt_re_dev *rdev,
				      struct bnxt_re_qp *qp, struct ib_wc *wc,
				      struct bnxt_qplib_cqe *cqe)
{
	u16 vlan_id = 0;

	wc->opcode = IB_WC_RECV;
	wc->status = __rc_to_ib_wc_status(cqe->status);
	if (cqe->flags & CQ_RES_UD_FLAGS_IMM)
		wc->wc_flags |= IB_WC_WITH_IMM;
	if (cqe->flags & CQ_RES_RC_FLAGS_INV)
		wc->wc_flags |= IB_WC_WITH_INVALIDATE;
	/* report only on GSI QP for Thor */
	if (rdev->gsi_ctx.gsi_qp->qplib_qp.id == qp->qplib_qp.id &&
	    rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_UD) {
		wc->wc_flags |= IB_WC_GRH;
		memcpy(wc->smac, cqe->smac, ETH_ALEN);
		wc->wc_flags |= IB_WC_WITH_SMAC;
		if (_is_cqe_v2_supported(rdev->dev_attr->dev_cap_flags)) {
			if (cqe->flags & CQ_RES_UD_V2_FLAGS_META_FORMAT_MASK) {
				if (cqe->cfa_meta &
				    BNXT_QPLIB_CQE_CFA_META1_VALID)
					vlan_id = (cqe->cfa_meta & 0xFFF);
			}
		} else if (cqe->flags & CQ_RES_UD_FLAGS_META_FORMAT_VLAN) {
			vlan_id = (cqe->cfa_meta & 0xFFF);
		}
		/* Mark only if vlan_id is non zero */
		if (vlan_id && bnxt_re_check_if_vlan_valid(rdev, vlan_id)) {
			wc->vlan_id = vlan_id;
			wc->wc_flags |= IB_WC_WITH_VLAN;
		}
	}
}

static int bnxt_re_legacy_send_phantom_wqe(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_qp *lib_qp = &qp->qplib_qp;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&qp->sq_lock, flags);

	rc = bnxt_re_legacy_bind_fence_mw(lib_qp);
	if (!rc) {
		lib_qp->sq.phantom_wqe_cnt++;
		dev_dbg(&lib_qp->sq.hwq.pdev->dev,
			"qp %#x sq->prod %#x sw_prod %#x phantom_wqe_cnt %d\n",
			lib_qp->id, lib_qp->sq.hwq.prod,
			HWQ_CMP(lib_qp->sq.hwq.prod, &lib_qp->sq.hwq),
			lib_qp->sq.phantom_wqe_cnt);
	}

	spin_unlock_irqrestore(&qp->sq_lock, flags);
	return rc;
}

int bnxt_re_poll_cq(struct ib_cq *ib_cq, int num_entries, struct ib_wc *wc)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ibcq);
	struct bnxt_re_dev *rdev = cq->rdev;
	struct bnxt_re_qp *qp;
	struct bnxt_qplib_cqe *cqe;
	int i, ncqe, budget, init_budget;
	struct bnxt_qplib_q *sq;
	struct bnxt_qplib_qp *lib_qp;
	u32 tbl_idx;
	struct bnxt_re_sqp_entries *sqp_entry = NULL;
	unsigned long flags;
	u8 gsi_mode;

	/*
	 * DB recovery CQ; only process the door bell pacing alert from
	 * the user lib
	 */
	if (cq->is_dbr_soft_cq) {
		bnxt_re_pacing_alert(rdev);
		return 0;
	}

	/* User CQ; the only processing we do is to
	 * complete any pending CQ resize operation.
	 */
	if (cq->umem) {
		if (cq->resize_umem)
			bnxt_re_resize_cq_complete(cq);
		return 0;
	}

	spin_lock_irqsave(&cq->cq_lock, flags);

	budget = min_t(u32, num_entries, cq->max_cql);
	init_budget = budget;
	if (!cq->cql) {
		dev_err(rdev_to_dev(rdev), "POLL CQ no CQL to use\n");
		goto exit;
	}
	cqe = &cq->cql[0];
	gsi_mode = rdev->gsi_ctx.gsi_qp_mode;
	while (budget) {
		lib_qp = NULL;
		ncqe = bnxt_qplib_poll_cq(&cq->qplib_cq, cqe, budget, &lib_qp);
		if (lib_qp) {
			sq = &lib_qp->sq;
			if (sq->legacy_send_phantom == true) {
				qp = container_of(lib_qp, struct bnxt_re_qp, qplib_qp);
				if (bnxt_re_legacy_send_phantom_wqe(qp) == -ENOMEM)
					dev_err(rdev_to_dev(rdev),
						"Phantom failed! Scheduled to send again\n");
				else
					sq->legacy_send_phantom = false;
			}
		}
		if (ncqe < budget)
			ncqe += bnxt_qplib_process_flush_list(&cq->qplib_cq,
							      cqe + ncqe,
							      budget - ncqe);

		if (!ncqe)
			break;

		for (i = 0; i < ncqe; i++, cqe++) {
			/* Transcribe each qplib_wqe back to ib_wc */
			memset(wc, 0, sizeof(*wc));

			wc->wr_id = cqe->wr_id;
			wc->byte_len = cqe->length;
			qp = to_bnxt_re((struct bnxt_qplib_qp *)cqe->qp_handle,
					struct bnxt_re_qp, qplib_qp);
			if (!qp) {
				dev_err(rdev_to_dev(rdev),
					"POLL CQ bad QP handle\n");
				continue;
			}
			wc->qp = &qp->ib_qp;
			wc->ex.imm_data = cqe->immdata;
			wc->src_qp = cqe->src_qp;
			memcpy(wc->smac, cqe->smac, ETH_ALEN);
			wc->port_num = 1;
			wc->vendor_err = cqe->status;

			switch(cqe->opcode) {
			case CQ_BASE_CQE_TYPE_REQ:
				if (gsi_mode == BNXT_RE_GSI_MODE_ALL &&
				    qp->qplib_qp.id ==
				    rdev->gsi_ctx.gsi_sqp->qplib_qp.id) {
					/* Handle this completion with
					 * the stored completion */
					 dev_dbg(rdev_to_dev(rdev),
						 "Skipping this UD Send CQ\n");
					memset(wc, 0, sizeof(*wc));
					continue;
				}
				bnxt_re_process_req_wc(wc, cqe);
				break;
			case CQ_BASE_CQE_TYPE_RES_RAWETH_QP1:
				if (gsi_mode == BNXT_RE_GSI_MODE_ALL) {
					if (!cqe->status) {
						int rc = 0;
						rc = bnxt_re_process_raw_qp_packet_receive(qp, cqe);
						if (!rc) {
							memset(wc, 0,
							       sizeof(*wc));
							continue;
						}
						cqe->status = -1;
					}
					/* Errors need not be looped back.
					 * But change the wr_id to the one
					 * stored in the table
					 */
					tbl_idx = cqe->wr_id;
					sqp_entry = &rdev->gsi_ctx.sqp_tbl[tbl_idx];
					wc->wr_id = sqp_entry->wrid;
				}

				bnxt_re_process_res_rawqp1_wc(wc, cqe);
				break;
			case CQ_BASE_CQE_TYPE_RES_RC:
				bnxt_re_process_res_rc_wc(wc, cqe);
				break;
			case CQ_BASE_CQE_TYPE_RES_UD:
				if (gsi_mode == BNXT_RE_GSI_MODE_ALL &&
				    qp->qplib_qp.id ==
				    rdev->gsi_ctx.gsi_sqp->qplib_qp.id) {
					/* Handle this completion with
					 * the stored completion
					 */
					dev_dbg(rdev_to_dev(rdev),
						"Handling the UD receive CQ\n");
					if (cqe->status) {
						/* TODO handle this completion  as a failure in
						 * loopback porocedure
						 */
						continue;
					} else {
						bnxt_re_process_res_shadow_qp_wc(qp, wc, cqe);
						break;
					}
				}
				bnxt_re_process_res_ud_wc(rdev, qp, wc, cqe);
				break;
			default:
				dev_err(rdev_to_dev(cq->rdev),
					"POLL CQ type 0x%x not handled, skip!\n",
					cqe->opcode);
				continue;
			}
			wc++;
			budget--;
		}
	}
exit:
	spin_unlock_irqrestore(&cq->cq_lock, flags);
	return init_budget - budget;
}

int bnxt_re_req_notify_cq(struct ib_cq *ib_cq,
			  enum ib_cq_notify_flags ib_cqn_flags)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ibcq);
	int type = 0, rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&cq->cq_lock, flags);
	/* Trigger on the very next completion */
	if (ib_cqn_flags & IB_CQ_NEXT_COMP)
		type = DBC_DBC_TYPE_CQ_ARMALL;
	/* Trigger on the next solicited completion */
	else if (ib_cqn_flags & IB_CQ_SOLICITED)
		type = DBC_DBC_TYPE_CQ_ARMSE;

	bnxt_qplib_req_notify_cq(&cq->qplib_cq, type);

	/* Poll to see if there are missed events */
	if ((ib_cqn_flags & IB_CQ_REPORT_MISSED_EVENTS) &&
	    !(bnxt_qplib_is_cq_empty(&cq->qplib_cq)))
		rc = 1;

	spin_unlock_irqrestore(&cq->cq_lock, flags);

	return rc;
}

/* Memory Regions */
struct ib_mr *bnxt_re_get_dma_mr(struct ib_pd *ib_pd, int mr_access_flags)
{
	struct bnxt_qplib_mrinfo mrinfo;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_mr *mr;
	struct bnxt_re_pd *pd;
	u32 max_mr_count;
	u64 pbl = 0;
	int rc;

	memset(&mrinfo, 0, sizeof(mrinfo));
	pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	rdev = pd->rdev;

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr) {
		dev_err(rdev_to_dev(rdev),
			"Allocate memory for DMA MR failed!\n");
		return ERR_PTR(-ENOMEM);
	}
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.flags = __from_ib_access_flags(mr_access_flags);
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_PMR;

	/* Allocate and register 0 as the address */
	rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Allocate DMA MR failed!\n");
		goto fail;
	}
	mr->qplib_mr.total_size = -1; /* Infinite length */
	mrinfo.ptes = &pbl;
	mrinfo.sg.npages = 0;
	mrinfo.sg.pgsize = PAGE_SIZE;
	mrinfo.sg.pgshft = PAGE_SHIFT;
	mrinfo.sg.pgsize = PAGE_SIZE;
	mrinfo.mrw = &mr->qplib_mr;
	mrinfo.is_dma = true;
	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Register DMA MR failed!\n");
		goto fail_mr;
	}
	mr->ib_mr.lkey = mr->qplib_mr.lkey;
	if (mr_access_flags & (IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ |
			       IB_ACCESS_REMOTE_ATOMIC))
		mr->ib_mr.rkey = mr->ib_mr.lkey;
	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > atomic_read(&rdev->stats.rsors.max_mr_count))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);

	return &mr->ib_mr;

fail_mr:
	bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr);
fail:
	kfree(mr);
	return ERR_PTR(rc);
}

int bnxt_re_dereg_mr(struct ib_mr *ib_mr, struct ib_udata *udata)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);
	struct bnxt_re_dev *rdev = mr->rdev;
	int rc = 0;

	rc = bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Dereg MR failed (%d): rc - %#x\n",
			mr->qplib_mr.lkey, rc);

	if (mr->pages) {
		bnxt_qplib_free_fast_reg_page_list(&rdev->qplib_res,
						   &mr->qplib_frpl);
		kfree(mr->pages);
		mr->npages = 0;
		mr->pages = NULL;
	}
	if (!IS_ERR(mr->ib_umem) && mr->ib_umem) {
		mr->is_invalcb_active = false;
		bnxt_re_peer_mem_release(mr->ib_umem);
	}
	kfree(mr);
	atomic_dec(&rdev->stats.rsors.mr_count);
	return 0;
}

static int bnxt_re_set_page(struct ib_mr *ib_mr, u64 addr)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);

	if (unlikely(mr->npages == mr->qplib_frpl.max_pg_ptrs))
		return -ENOMEM;

	mr->pages[mr->npages++] = addr;
	dev_dbg(NULL, "%s: ibdev %p Set MR pages[%d] = 0x%lx\n",
		ROCE_DRV_MODULE_NAME, ib_mr->device, mr->npages - 1,
		mr->pages[mr->npages - 1]);
	return 0;
}

int bnxt_re_map_mr_sg(struct ib_mr *ib_mr, struct scatterlist *sg,
		      int sg_nents, unsigned int *sg_offset)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);

	mr->npages = 0;
	return ib_sg_to_pages(ib_mr, sg, sg_nents,
			      sg_offset, bnxt_re_set_page);
}

struct ib_mr *bnxt_re_alloc_mr(struct ib_pd *ib_pd, enum ib_mr_type type,
			       u32 max_num_sg, struct ib_udata *udata)
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_mr *mr;
	u32 max_mr_count;
	int rc;

	dev_dbg(rdev_to_dev(rdev), "Alloc MR\n");
	if (type != IB_MR_TYPE_MEM_REG) {
		dev_dbg(rdev_to_dev(rdev), "MR type 0x%x not supported\n", type);
		return ERR_PTR(-EINVAL);
	}
	if (max_num_sg > MAX_PBL_LVL_1_PGS) {
		dev_dbg(rdev_to_dev(rdev), "Max SG exceeded\n");
		return ERR_PTR(-EINVAL);
	}
	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr) {
		dev_err(rdev_to_dev(rdev), "Allocate MR mem failed!\n");
		return ERR_PTR(-ENOMEM);
	}
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.flags = BNXT_QPLIB_FR_PMR;
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_PMR;

	rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Allocate MR failed!\n");
		goto fail;
	}
	mr->ib_mr.lkey = mr->qplib_mr.lkey;
	mr->ib_mr.rkey = mr->ib_mr.lkey;
	mr->pages = kzalloc(sizeof(u64) * max_num_sg, GFP_KERNEL);
	if (!mr->pages) {
		dev_err(rdev_to_dev(rdev),
			"Allocate MR page list mem failed!\n");
		rc = -ENOMEM;
		goto fail_mr;
	}
	rc = bnxt_qplib_alloc_fast_reg_page_list(&rdev->qplib_res,
						 &mr->qplib_frpl, max_num_sg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Allocate HW Fast reg page list failed!\n");
		goto free_page;
	}
	dev_dbg(rdev_to_dev(rdev), "Alloc MR pages = 0x%p\n", mr->pages);

	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > atomic_read(&rdev->stats.rsors.max_mr_count))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);
	return &mr->ib_mr;

free_page:
	kfree(mr->pages);
fail_mr:
	bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr);
fail:
	kfree(mr);
	return ERR_PTR(rc);
}

/* Memory Windows */
struct ib_mw *bnxt_re_alloc_mw(struct ib_pd *ib_pd, enum ib_mw_type type,
			       struct ib_udata *udata)
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_mw *mw;
	u32 max_mw_count;
	int rc;

	mw = kzalloc(sizeof(*mw), GFP_KERNEL);
	if (!mw) {
		dev_err(rdev_to_dev(rdev), "Allocate MW failed!\n");
		rc = -ENOMEM;
		goto exit;
	}
	mw->rdev = rdev;
	mw->qplib_mw.pd = &pd->qplib_pd;

	mw->qplib_mw.type = (type == IB_MW_TYPE_1 ?
			       CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE1 :
			       CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2B);
	rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mw->qplib_mw);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Allocate MW failed!\n");
		goto fail;
	}
	mw->ib_mw.rkey = mw->qplib_mw.rkey;
	atomic_inc(&rdev->stats.rsors.mw_count);
	max_mw_count = atomic_read(&rdev->stats.rsors.mw_count);
	if (max_mw_count > atomic_read(&rdev->stats.rsors.max_mw_count))
		atomic_set(&rdev->stats.rsors.max_mw_count, max_mw_count);

	return &mw->ib_mw;
fail:
	kfree(mw);
exit:
	return ERR_PTR(rc);
}

int bnxt_re_dealloc_mw(struct ib_mw *ib_mw)
{
	struct bnxt_re_mw *mw = to_bnxt_re(ib_mw, struct bnxt_re_mw, ib_mw);
	struct bnxt_re_dev *rdev = mw->rdev;
	int rc;

	rc = bnxt_qplib_free_mrw(&rdev->qplib_res, &mw->qplib_mw);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Free MW failed: %#x\n", rc);
		return rc;
	}

	kfree(mw);
	atomic_dec(&rdev->stats.rsors.mw_count);
	return rc;
}

static int bnxt_re_page_size_ok(int page_shift)
{
	switch (page_shift) {
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_4K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_8K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_64K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_2M:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_256K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_1M:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_4M:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_256MB:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_1G:
		return 1;
	default:
		return 0;
	}
}

static int bnxt_re_get_page_shift(struct ib_umem *umem,
				  u64 va, u64 st, u64 cmask)
{
	int pgshft;

	pgshft = ilog2(umem->page_size);

	return pgshft;
}

static int bnxt_re_get_num_pages(struct ib_umem *umem, u64 start, u64 length, int page_shift)
{
	int npages = 0;

	if (page_shift == PAGE_SHIFT) {
		npages = ib_umem_num_pages_compat(umem);
	} else {
		npages = ALIGN(length, BIT(page_shift)) / BIT(page_shift);
		if (start %  BIT(page_shift))
			npages++;
	}
	return npages;
}

/* uverbs */
struct ib_mr *bnxt_re_reg_user_mr(struct ib_pd *ib_pd, u64 start, u64 length,
				  u64 virt_addr, int mr_access_flags,
				  struct ib_udata *udata)
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_qplib_mrinfo mrinfo;
	int umem_pgs, page_shift, rc;
	struct bnxt_re_mr *mr;
	struct ib_umem *umem;
	u32 max_mr_count;
	int npages;

	dev_dbg(rdev_to_dev(rdev), "Reg user MR\n");

	if (bnxt_re_get_total_mr_mw_count(rdev) >= rdev->dev_attr->max_mr)
		return ERR_PTR(-ENOMEM);

	if (rdev->mod_exit) {
		dev_dbg(rdev_to_dev(rdev), "%s(): in mod_exit, just return!\n", __func__);
		return ERR_PTR(-EIO);
	}
	memset(&mrinfo, 0, sizeof(mrinfo));
	if (length > BNXT_RE_MAX_MR_SIZE) {
		dev_err(rdev_to_dev(rdev), "Requested MR Size: %lu "
			"> Max supported: %ld\n", length, BNXT_RE_MAX_MR_SIZE);
		return ERR_PTR(-ENOMEM);
	}
	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr) {
		dev_err(rdev_to_dev(rdev), "Allocate MR failed!\n");
		return ERR_PTR (-ENOMEM);
	}
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.flags = __from_ib_access_flags(mr_access_flags);
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_MR;

	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr)) {
		rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Alloc MR failed!\n");
			goto fail;
		}
		/* The fixed portion of the rkey is the same as the lkey */
		mr->ib_mr.rkey = mr->qplib_mr.rkey;
	}

	umem = ib_umem_get_flags_compat(rdev, ib_pd->uobject->context,
					udata, start, length,
					mr_access_flags, 0);
	if (IS_ERR(umem)) {
		rc = PTR_ERR(umem);
		dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed! rc = %d\n",
			__func__, rc);
		goto free_mr;
	}
	mr->ib_umem = umem;

	mr->qplib_mr.va = virt_addr;
	umem_pgs = ib_umem_num_pages_compat(umem);
	if (!umem_pgs) {
		dev_err(rdev_to_dev(rdev), "umem is invalid!\n");
		rc = -EINVAL;
		goto free_umem;
	}
	mr->qplib_mr.total_size = length;
	page_shift = bnxt_re_get_page_shift(umem, virt_addr, start,
					    rdev->dev_attr->page_size_cap);
	if (!bnxt_re_page_size_ok(page_shift)) {
		dev_err(rdev_to_dev(rdev), "umem page size unsupported!\n");
		rc = -EFAULT;
		goto free_umem;
	}
	npages = bnxt_re_get_num_pages(umem, start, length, page_shift);

	/* Map umem buf ptrs to the PBL */
	mrinfo.sg.npages = npages;
	mrinfo.sg.sghead = get_ib_umem_sgl(umem, &mrinfo.sg.nmap);
	mrinfo.sg.pgshft = page_shift;
	mrinfo.sg.pgsize = BIT(page_shift);

	mrinfo.mrw = &mr->qplib_mr;

	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Reg user MR failed!\n");
		goto free_umem;
	}

	mr->ib_mr.lkey = mr->ib_mr.rkey = mr->qplib_mr.lkey;
	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > atomic_read(&rdev->stats.rsors.max_mr_count))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);

	return &mr->ib_mr;

free_umem:
	bnxt_re_peer_mem_release(mr->ib_umem);
free_mr:
	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr))
		bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr);
fail:
	kfree(mr);
	return ERR_PTR(rc);
}

int
bnxt_re_rereg_user_mr(struct ib_mr *ib_mr, int flags, u64 start, u64 length,
		      u64 virt_addr, int mr_access_flags,
		      struct ib_pd *ib_pd, struct ib_udata *udata)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ibpd);
	int umem_pgs = 0, page_shift = PAGE_SHIFT, rc;
	struct bnxt_re_dev *rdev = mr->rdev;
	struct bnxt_qplib_mrinfo mrinfo;
	struct ib_umem *umem;
	u32 npages;

	/* TODO: Must decipher what to modify based on the flags */
	memset(&mrinfo, 0, sizeof(mrinfo));
	if (flags & IB_MR_REREG_TRANS) {
		umem = ib_umem_get_flags_compat(rdev, ib_pd->uobject->context,
						udata, start, length,
						mr_access_flags, 0);
		if (IS_ERR(umem)) {
			rc = PTR_ERR(umem);
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed! ret =  %d\n",
				__func__, rc);
			goto fail;
		}
		mr->ib_umem = umem;

		mr->qplib_mr.va = virt_addr;
		umem_pgs = ib_umem_num_pages_compat(umem);
		if (!umem_pgs) {
			dev_err(rdev_to_dev(rdev), "umem is invalid!\n");
			rc = -EINVAL;
			goto fail_free_umem;
		}
		mr->qplib_mr.total_size = length;
		page_shift = bnxt_re_get_page_shift(umem, virt_addr, start,
					    rdev->dev_attr->page_size_cap);
		if (!bnxt_re_page_size_ok(page_shift)) {
			dev_err(rdev_to_dev(rdev),
				"umem page size unsupported!\n");
			rc = -EFAULT;
			goto fail_free_umem;
		}
		npages = bnxt_re_get_num_pages(umem, start, length, page_shift);
		/* Map umem buf ptrs to the PBL */
		mrinfo.sg.npages = npages;
		mrinfo.sg.sghead = get_ib_umem_sgl(umem, &mrinfo.sg.nmap);
		mrinfo.sg.pgshft = page_shift;
		mrinfo.sg.pgsize = BIT(page_shift);
	}

	mrinfo.mrw = &mr->qplib_mr;
	if (flags & IB_MR_REREG_PD)
		mr->qplib_mr.pd = &pd->qplib_pd;

	if (flags & IB_MR_REREG_ACCESS)
		mr->qplib_mr.flags = __from_ib_access_flags(mr_access_flags);

	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Rereg user MR failed!\n");
		goto fail_free_umem;
	}
	mr->ib_mr.rkey = mr->qplib_mr.rkey;

	return 0;

fail_free_umem:
	bnxt_re_peer_mem_release(mr->ib_umem);
fail:
	return rc;
}

static int bnxt_re_check_abi_version(struct bnxt_re_dev *rdev)
{
	struct ib_device *ibdev = &rdev->ibdev;
	u32 uverbs_abi_ver;

	uverbs_abi_ver = GET_UVERBS_ABI_VERSION(ibdev);
	dev_dbg(rdev_to_dev(rdev), "ABI version requested %d\n",
		uverbs_abi_ver);
	if (uverbs_abi_ver != BNXT_RE_ABI_VERSION) {
		dev_dbg(rdev_to_dev(rdev), " is different from the device %d \n",
			BNXT_RE_ABI_VERSION);
		return -EPERM;
	}
	return 0;
}

int bnxt_re_alloc_ucontext(struct ib_ucontext *uctx_in,
			   struct ib_udata *udata)
{
	struct ib_ucontext *ctx = uctx_in;
	struct ib_device *ibdev = ctx->device;
	struct bnxt_re_ucontext *uctx =
		container_of(ctx, struct bnxt_re_ucontext, ibucontext);

	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;
	struct bnxt_re_uctx_resp resp = {};
	struct bnxt_re_uctx_req ureq = {};
	struct bnxt_qplib_chip_ctx *cctx;
	u32 chip_met_rev_num;
	bool genp5 = false;
	int rc;

	cctx = rdev->chip_ctx;
	rc = bnxt_re_check_abi_version(rdev);
	if (rc)
		goto fail;

	uctx->rdev = rdev;
	uctx->shpg = (void *)__get_free_page(GFP_KERNEL);
	if (!uctx->shpg) {
		dev_err(rdev_to_dev(rdev), "shared memory allocation failed!\n");
		rc = -ENOMEM;
		goto fail;
	}
	spin_lock_init(&uctx->sh_lock);
	if (BNXT_RE_ABI_VERSION >= 4) {
		chip_met_rev_num = cctx->chip_num;
		chip_met_rev_num |= ((u32)cctx->chip_rev & 0xFF) <<
				     BNXT_RE_CHIP_ID0_CHIP_REV_SFT;
		chip_met_rev_num |= ((u32)cctx->chip_metal & 0xFF) <<
				     BNXT_RE_CHIP_ID0_CHIP_MET_SFT;
		resp.chip_id0 = chip_met_rev_num;
		resp.chip_id1 = 0; /* future extension of chip info */
	}

	if (BNXT_RE_ABI_VERSION != 4) {
		/*Temp, Use idr_alloc instead*/
		resp.dev_id = rdev->en_dev->pdev->devfn;
		resp.max_qp = rdev->qplib_res.hctx->qp_ctx.max;
	}

	genp5 = _is_chip_gen_p5_p7(cctx);
	if (BNXT_RE_ABI_VERSION > 5) {
		resp.modes = genp5 ? cctx->modes.wqe_mode : 0;
		if (rdev->dev_attr && BNXT_RE_HW_RETX(rdev->dev_attr->dev_cap_flags))
			resp.comp_mask = BNXT_RE_COMP_MASK_UCNTX_HW_RETX_ENABLED;
	}

	resp.pg_size = PAGE_SIZE;
	resp.cqe_sz = sizeof(struct cq_base);
	resp.max_cqd = dev_attr->max_cq_wqes;
	if (genp5 && cctx->modes.db_push) {
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_WC_DPI_ENABLED;
		if (_is_chip_p7(cctx) &&
		    !(dev_attr->dev_cap_flags &
		      CREQ_QUERY_FUNC_RESP_SB_PINGPONG_PUSH_MODE))
			resp.comp_mask &=
				~BNXT_RE_COMP_MASK_UCNTX_WC_DPI_ENABLED;
	}

	resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_MQP_EX_SUPPORTED;

	if (rdev->dbr_pacing)
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_DBR_PACING_ENABLED;

	if (rdev->dbr_drop_recov && rdev->user_dbr_drop_recov)
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_DBR_RECOVERY_ENABLED;

	if (udata->inlen >= sizeof(ureq)) {
		rc = ib_copy_from_udata(&ureq, udata,
					min(udata->inlen, sizeof(ureq)));
		if (rc)
			goto cfail;
		if (bnxt_re_init_pow2_flag(&ureq, &resp))
			dev_warn(rdev_to_dev(rdev),
				 "Enabled roundup logic. Library bug?\n");
		if (bnxt_re_init_rsvd_wqe_flag(&ureq, &resp, genp5))
			dev_warn(rdev_to_dev(rdev),
				 "Rsvd wqe in use! Try the updated library.\n");
	} else {
		dev_warn(rdev_to_dev(rdev),
			 "Enabled roundup logic. Update the library!\n");
		resp.comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED;

		dev_warn(rdev_to_dev(rdev),
			 "Rsvd wqe in use. Update the library!\n");
		resp.comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
	}

	uctx->cmask = (uint64_t)resp.comp_mask;
	rc = bnxt_re_copy_to_udata(rdev, &resp,
				   min(udata->outlen, sizeof(resp)),
				   udata);
	if (rc)
		goto cfail;

	INIT_LIST_HEAD(&uctx->cq_list);
	mutex_init(&uctx->cq_lock);

	return 0;
cfail:
	free_page((u64)uctx->shpg);
	uctx->shpg = NULL;
fail:
	return rc;
}

void bnxt_re_dealloc_ucontext(struct ib_ucontext *ib_uctx)
{
	struct bnxt_re_ucontext *uctx = to_bnxt_re(ib_uctx,
						   struct bnxt_re_ucontext,
						   ibucontext);
	struct bnxt_re_dev *rdev = uctx->rdev;
	int rc = 0;

	if (uctx->shpg)
		free_page((u64)uctx->shpg);

	if (uctx->dpi.dbr) {
		/* Free DPI only if this is the first PD allocated by the
		 * application and mark the context dpi as NULL
		 */
		if (_is_chip_gen_p5_p7(rdev->chip_ctx) && uctx->wcdpi.dbr) {
			rc = bnxt_qplib_dealloc_dpi(&rdev->qplib_res,
						    &uctx->wcdpi);
			if (rc)
				dev_err(rdev_to_dev(rdev),
						"dealloc push dp failed\n");
			uctx->wcdpi.dbr = NULL;
		}

		rc = bnxt_qplib_dealloc_dpi(&rdev->qplib_res,
					    &uctx->dpi);
		if (rc)
			dev_err(rdev_to_dev(rdev), "Deallocte HW DPI failed!\n");
			/* Don't fail, continue*/
		uctx->dpi.dbr = NULL;
	}
	return;
}

static struct bnxt_re_cq *is_bnxt_re_cq_page(struct bnxt_re_ucontext *uctx,
				      u64 pg_off)
{
	struct bnxt_re_cq *cq = NULL, *tmp_cq;

	if (!_is_chip_p7(uctx->rdev->chip_ctx))
		return NULL;

	mutex_lock(&uctx->cq_lock);
	list_for_each_entry(tmp_cq, &uctx->cq_list, cq_list) {
		if (((u64)tmp_cq->uctx_cq_page >> PAGE_SHIFT) == pg_off) {
			cq = tmp_cq;
			break;
		}
	}
	mutex_unlock(&uctx->cq_lock);
	return cq;
}

/* Helper function to mmap the virtual memory from user app */
int bnxt_re_mmap(struct ib_ucontext *ib_uctx, struct vm_area_struct *vma)
{
	struct bnxt_re_ucontext *uctx = to_bnxt_re(ib_uctx,
						   struct bnxt_re_ucontext,
						   ibucontext);
	struct bnxt_re_dev *rdev = uctx->rdev;
	struct bnxt_re_cq *cq = NULL;
	int rc = 0;
	u64 pfn;

	switch (vma->vm_pgoff) {
	case BNXT_RE_MAP_SH_PAGE:
		pfn = vtophys(uctx->shpg) >> PAGE_SHIFT;
		return rdma_user_mmap_io(&uctx->ibucontext, vma, pfn, PAGE_SIZE, vma->vm_page_prot, NULL);
		dev_dbg(rdev_to_dev(rdev), "%s:%d uctx->shpg 0x%lx, vtophys(uctx->shpg) 0x%lx, pfn = 0x%lx \n",
				__func__, __LINE__, (u64) uctx->shpg, vtophys(uctx->shpg), pfn);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Shared page mapping failed!\n");
			rc = -EAGAIN;
		}
		return rc;
	case BNXT_RE_MAP_WC:
		vma->vm_page_prot =
			pgprot_writecombine(vma->vm_page_prot);
		pfn = (uctx->wcdpi.umdbr >> PAGE_SHIFT);
		if (!pfn)
			return -EFAULT;
		break;
	case BNXT_RE_DBR_PAGE:
		/* Driver doesn't expect write access request */
		if (vma->vm_flags & VM_WRITE)
			return -EFAULT;

		pfn = vtophys(rdev->dbr_page) >> PAGE_SHIFT;
		if (!pfn)
			return -EFAULT;
		break;
	case BNXT_RE_MAP_DB_RECOVERY_PAGE:
		pfn = vtophys(uctx->dbr_recov_cq_page) >> PAGE_SHIFT;
		if (!pfn)
			return -EFAULT;
		break;
	default:
		cq = is_bnxt_re_cq_page(uctx, vma->vm_pgoff);
		if (cq) {
			pfn = vtophys((void *)cq->uctx_cq_page) >> PAGE_SHIFT;
			rc = rdma_user_mmap_io(&uctx->ibucontext, vma, pfn, PAGE_SIZE, vma->vm_page_prot, NULL);
			if (rc) {
				dev_err(rdev_to_dev(rdev),
					"CQ page mapping failed!\n");
				rc = -EAGAIN;
			}
			goto out;
		} else {
			vma->vm_page_prot =
				pgprot_noncached(vma->vm_page_prot);
			pfn = vma->vm_pgoff;
		}
		break;
	}

	rc = rdma_user_mmap_io(&uctx->ibucontext, vma, pfn, PAGE_SIZE, vma->vm_page_prot, NULL);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "DPI mapping failed!\n");
		return -EAGAIN;
	}
	rc = __bnxt_re_set_vma_data(uctx, vma);
out:
	return rc;
}

int bnxt_re_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			const struct ib_wc *wc, const struct ib_grh *grh,
			const struct ib_mad_hdr *in_mad, size_t in_mad_size,
			struct ib_mad_hdr *out_mad, size_t *out_mad_size,
			u16 *out_mad_pkey_index)
{
	return IB_MAD_RESULT_SUCCESS;
}

void bnxt_re_disassociate_ucntx(struct ib_ucontext *ib_uctx)
{
}
