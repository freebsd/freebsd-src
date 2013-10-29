/*
 * Copyright (c) 2009-2013 Chelsio, Inc. All rights reserved.
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#ifdef TCP_OFFLOAD
#include <asm/pgtable.h>
#include <linux/page.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>

#include "iw_cxgbe.h"
#include "user.h"

static int fastreg_support = 1;
module_param(fastreg_support, int, 0644);
MODULE_PARM_DESC(fastreg_support, "Advertise fastreg support (default = 1)");

static int c4iw_modify_port(struct ib_device *ibdev,
			    u8 port, int port_modify_mask,
			    struct ib_port_modify *props)
{
	return -ENOSYS;
}

static struct ib_ah *c4iw_ah_create(struct ib_pd *pd,
				    struct ib_ah_attr *ah_attr)
{
	return ERR_PTR(-ENOSYS);
}

static int c4iw_ah_destroy(struct ib_ah *ah)
{
	return -ENOSYS;
}

static int c4iw_multicast_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	return -ENOSYS;
}

static int c4iw_multicast_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	return -ENOSYS;
}

static int c4iw_process_mad(struct ib_device *ibdev, int mad_flags,
			    u8 port_num, struct ib_wc *in_wc,
			    struct ib_grh *in_grh, struct ib_mad *in_mad,
			    struct ib_mad *out_mad)
{
	return -ENOSYS;
}

static int c4iw_dealloc_ucontext(struct ib_ucontext *context)
{
	struct c4iw_dev *rhp = to_c4iw_dev(context->device);
	struct c4iw_ucontext *ucontext = to_c4iw_ucontext(context);
	struct c4iw_mm_entry *mm, *tmp;

	CTR2(KTR_IW_CXGBE, "%s context %p", __func__, context);
	list_for_each_entry_safe(mm, tmp, &ucontext->mmaps, entry)
		kfree(mm);
	c4iw_release_dev_ucontext(&rhp->rdev, &ucontext->uctx);
	kfree(ucontext);
	return 0;
}

static struct ib_ucontext *c4iw_alloc_ucontext(struct ib_device *ibdev,
					       struct ib_udata *udata)
{
	struct c4iw_ucontext *context;
	struct c4iw_dev *rhp = to_c4iw_dev(ibdev);

	CTR2(KTR_IW_CXGBE, "%s ibdev %p", __func__, ibdev);
	context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (!context)
		return ERR_PTR(-ENOMEM);
	c4iw_init_dev_ucontext(&rhp->rdev, &context->uctx);
	INIT_LIST_HEAD(&context->mmaps);
	spin_lock_init(&context->mmap_lock);
	return &context->ibucontext;
}

static inline pgprot_t t4_pgprot_wc(pgprot_t prot)
{
    return pgprot_writecombine(prot);
}

static int c4iw_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
	int len = vma->vm_end - vma->vm_start;
	u32 key = vma->vm_pgoff << PAGE_SHIFT;
	struct c4iw_rdev *rdev;
	int ret = 0;
	struct c4iw_mm_entry *mm;
	struct c4iw_ucontext *ucontext;
	u64 addr, paddr;

	u64 va_regs_res = 0, va_udbs_res = 0;
	u64 len_regs_res = 0, len_udbs_res = 0;

	CTR3(KTR_IW_CXGBE, "%s:1 ctx %p vma %p", __func__, context, vma);

	CTR4(KTR_IW_CXGBE, "%s:1a pgoff 0x%lx key 0x%x len %d", __func__,
	    vma->vm_pgoff, key, len);

	if (vma->vm_start & (PAGE_SIZE-1)) {
		CTR3(KTR_IW_CXGBE, "%s:2 unaligned vm_start %u vma %p",
		    __func__, vma->vm_start, vma);
		return -EINVAL;
	}

	rdev = &(to_c4iw_dev(context->device)->rdev);
	ucontext = to_c4iw_ucontext(context);

	mm = remove_mmap(ucontext, key, len);
	if (!mm) {
		CTR4(KTR_IW_CXGBE, "%s:3 ucontext %p key %u len %u", __func__,
		    ucontext, key, len);
		return -EINVAL;
	}
	addr = mm->addr;
	kfree(mm);

	va_regs_res = (u64)rman_get_virtual(rdev->adap->regs_res);
	len_regs_res = (u64)rman_get_size(rdev->adap->regs_res);
	va_udbs_res = (u64)rman_get_virtual(rdev->adap->udbs_res);
	len_udbs_res = (u64)rman_get_size(rdev->adap->udbs_res);

	CTR6(KTR_IW_CXGBE,
	    "%s:4 addr %p, masync region %p:%p, udb region %p:%p", __func__,
	    addr, va_regs_res, va_regs_res+len_regs_res, va_udbs_res,
	    va_udbs_res+len_udbs_res);

	if (addr >= va_regs_res && addr < va_regs_res + len_regs_res) {
		CTR4(KTR_IW_CXGBE, "%s:5 MA_SYNC addr %p region %p, reglen %u",
		    __func__, addr, va_regs_res, len_regs_res);
		/*
		 * MA_SYNC register...
		 */
		paddr = vtophys(addr);
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		ret = io_remap_pfn_range(vma, vma->vm_start,
				paddr >> PAGE_SHIFT,
				len, vma->vm_page_prot);
	} else {

		if (addr >= va_udbs_res && addr < va_udbs_res + len_udbs_res) {
			/*
			* Map user DB or OCQP memory...
			*/
			paddr = vtophys(addr);
			CTR4(KTR_IW_CXGBE,
			    "%s:6 USER DB-GTS addr %p region %p, reglen %u",
			    __func__, addr, va_udbs_res, len_udbs_res);
#ifdef DOT5
			if (is_t5(rdev->lldi.adapter_type) && map_udb_as_wc)
				vma->vm_page_prot = t4_pgprot_wc(vma->vm_page_prot);
			else
#endif
				vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
			ret = io_remap_pfn_range(vma, vma->vm_start,
					paddr >> PAGE_SHIFT,
					len, vma->vm_page_prot);
		} else {
			/*
			 * Map WQ or CQ contig dma memory...
			 */
			CTR4(KTR_IW_CXGBE,
			    "%s:7 WQ/CQ addr %p vm_start %u vma %p", __func__,
			    addr, vma->vm_start, vma);
			ret = io_remap_pfn_range(vma, vma->vm_start,
				addr >> PAGE_SHIFT,
				len, vma->vm_page_prot);
		}
	}
	CTR4(KTR_IW_CXGBE, "%s:8 ctx %p vma %p ret %u", __func__, context, vma,
	    ret);
	return ret;
}

static int
c4iw_deallocate_pd(struct ib_pd *pd)
{
	struct c4iw_pd *php = to_c4iw_pd(pd);
	struct c4iw_dev *rhp = php->rhp;

	CTR3(KTR_IW_CXGBE, "%s: pd %p, pdid 0x%x", __func__, pd, php->pdid);

	c4iw_put_resource(&rhp->rdev.resource.pdid_table, php->pdid);
	mutex_lock(&rhp->rdev.stats.lock);
	rhp->rdev.stats.pd.cur--;
	mutex_unlock(&rhp->rdev.stats.lock);
	kfree(php);

	return (0);
}

static struct ib_pd *
c4iw_allocate_pd(struct ib_device *ibdev, struct ib_ucontext *context,
    struct ib_udata *udata)
{
	struct c4iw_pd *php;
	u32 pdid;
	struct c4iw_dev *rhp;

	CTR4(KTR_IW_CXGBE, "%s: ibdev %p, context %p, data %p", __func__, ibdev,
	    context, udata);
	rhp = (struct c4iw_dev *) ibdev;
	pdid =  c4iw_get_resource(&rhp->rdev.resource.pdid_table);
	if (!pdid)
		return ERR_PTR(-EINVAL);
	php = kzalloc(sizeof(*php), GFP_KERNEL);
	if (!php) {
		c4iw_put_resource(&rhp->rdev.resource.pdid_table, pdid);
		return ERR_PTR(-ENOMEM);
	}
	php->pdid = pdid;
	php->rhp = rhp;
	if (context) {
		if (ib_copy_to_udata(udata, &php->pdid, sizeof(u32))) {
			c4iw_deallocate_pd(&php->ibpd);
			return ERR_PTR(-EFAULT);
		}
	}
	mutex_lock(&rhp->rdev.stats.lock);
	rhp->rdev.stats.pd.cur++;
	if (rhp->rdev.stats.pd.cur > rhp->rdev.stats.pd.max)
		rhp->rdev.stats.pd.max = rhp->rdev.stats.pd.cur;
	mutex_unlock(&rhp->rdev.stats.lock);

	CTR6(KTR_IW_CXGBE,
	    "%s: ibdev %p, context %p, data %p, pddid 0x%x, pd %p", __func__,
	    ibdev, context, udata, pdid, php);
	return (&php->ibpd);
}

static int
c4iw_query_pkey(struct ib_device *ibdev, u8 port, u16 index, u16 *pkey)
{

	CTR5(KTR_IW_CXGBE, "%s ibdev %p, port %d, index %d, pkey %p", __func__,
	    ibdev, port, index, pkey);

	*pkey = 0;
	return (0);
}

static int
c4iw_query_gid(struct ib_device *ibdev, u8 port, int index, union ib_gid *gid)
{
	struct c4iw_dev *dev;
	struct port_info *pi;
	struct adapter *sc;

	CTR5(KTR_IW_CXGBE, "%s ibdev %p, port %d, index %d, gid %p", __func__,
	    ibdev, port, index, gid);

	memset(&gid->raw[0], 0, sizeof(gid->raw));
	dev = to_c4iw_dev(ibdev);
	sc = dev->rdev.adap;
	if (port == 0 || port > sc->params.nports)
		return (-EINVAL);
	pi = sc->port[port - 1];
	memcpy(&gid->raw[0], pi->hw_addr, sizeof(pi->hw_addr));
	return (0);
}

static int
c4iw_query_device(struct ib_device *ibdev, struct ib_device_attr *props)
{
	struct c4iw_dev *dev = to_c4iw_dev(ibdev);
	struct adapter *sc = dev->rdev.adap;

	CTR3(KTR_IW_CXGBE, "%s ibdev %p, props %p", __func__, ibdev, props);

	memset(props, 0, sizeof *props);
	memcpy(&props->sys_image_guid, sc->port[0]->hw_addr, 6);
	props->hw_ver = sc->params.chipid;
	props->fw_ver = sc->params.fw_vers;
	props->device_cap_flags = dev->device_cap_flags;
	props->page_size_cap = T4_PAGESIZE_MASK;
	props->vendor_id = pci_get_vendor(sc->dev);
	props->vendor_part_id = pci_get_device(sc->dev);
	props->max_mr_size = T4_MAX_MR_SIZE;
	props->max_qp = T4_MAX_NUM_QP;
	props->max_qp_wr = T4_MAX_QP_DEPTH;
	props->max_sge = T4_MAX_RECV_SGE;
	props->max_sge_rd = 1;
	props->max_qp_rd_atom = c4iw_max_read_depth;
	props->max_qp_init_rd_atom = c4iw_max_read_depth;
	props->max_cq = T4_MAX_NUM_CQ;
	props->max_cqe = T4_MAX_CQ_DEPTH;
	props->max_mr = c4iw_num_stags(&dev->rdev);
	props->max_pd = T4_MAX_NUM_PD;
	props->local_ca_ack_delay = 0;
	props->max_fast_reg_page_list_len = T4_MAX_FR_DEPTH;

	return (0);
}

/*
 * Returns -errno on failure.
 */
static int
c4iw_query_port(struct ib_device *ibdev, u8 port, struct ib_port_attr *props)
{
	struct c4iw_dev *dev;
	struct adapter *sc;
	struct port_info *pi;
	struct ifnet *ifp;

	CTR4(KTR_IW_CXGBE, "%s ibdev %p, port %d, props %p", __func__, ibdev,
	    port, props);

	dev = to_c4iw_dev(ibdev);
	sc = dev->rdev.adap;
	if (port > sc->params.nports)
		return (-EINVAL);
	pi = sc->port[port - 1];
	ifp = pi->ifp;

	memset(props, 0, sizeof(struct ib_port_attr));
	props->max_mtu = IB_MTU_4096;
	if (ifp->if_mtu >= 4096)
		props->active_mtu = IB_MTU_4096;
	else if (ifp->if_mtu >= 2048)
		props->active_mtu = IB_MTU_2048;
	else if (ifp->if_mtu >= 1024)
		props->active_mtu = IB_MTU_1024;
	else if (ifp->if_mtu >= 512)
		props->active_mtu = IB_MTU_512;
	else
		props->active_mtu = IB_MTU_256;
	props->state = pi->link_cfg.link_ok ? IB_PORT_ACTIVE : IB_PORT_DOWN;
	props->port_cap_flags =
	    IB_PORT_CM_SUP |
	    IB_PORT_SNMP_TUNNEL_SUP |
	    IB_PORT_REINIT_SUP |
	    IB_PORT_DEVICE_MGMT_SUP |
	    IB_PORT_VENDOR_CLASS_SUP | IB_PORT_BOOT_MGMT_SUP;
	props->gid_tbl_len = 1;
	props->pkey_tbl_len = 1;
	props->active_width = 2;
	props->active_speed = 2;
	props->max_msg_sz = -1;

	return 0;
}

/*
 * Returns -errno on error.
 */
int
c4iw_register_device(struct c4iw_dev *dev)
{
	struct adapter *sc = dev->rdev.adap;
	struct ib_device *ibdev = &dev->ibdev;
	struct iw_cm_verbs *iwcm;
	int ret;

	CTR3(KTR_IW_CXGBE, "%s c4iw_dev %p, adapter %p", __func__, dev, sc);
	BUG_ON(!sc->port[0]);
	strlcpy(ibdev->name, device_get_nameunit(sc->dev), sizeof(ibdev->name));
	memset(&ibdev->node_guid, 0, sizeof(ibdev->node_guid));
	memcpy(&ibdev->node_guid, sc->port[0]->hw_addr, 6);
	ibdev->owner = THIS_MODULE;
	dev->device_cap_flags = IB_DEVICE_LOCAL_DMA_LKEY | IB_DEVICE_MEM_WINDOW;
	if (fastreg_support)
		dev->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;
	ibdev->local_dma_lkey = 0;
	ibdev->uverbs_cmd_mask =
	    (1ull << IB_USER_VERBS_CMD_GET_CONTEXT) |
	    (1ull << IB_USER_VERBS_CMD_QUERY_DEVICE) |
	    (1ull << IB_USER_VERBS_CMD_QUERY_PORT) |
	    (1ull << IB_USER_VERBS_CMD_ALLOC_PD) |
	    (1ull << IB_USER_VERBS_CMD_DEALLOC_PD) |
	    (1ull << IB_USER_VERBS_CMD_REG_MR) |
	    (1ull << IB_USER_VERBS_CMD_DEREG_MR) |
	    (1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL) |
	    (1ull << IB_USER_VERBS_CMD_CREATE_CQ) |
	    (1ull << IB_USER_VERBS_CMD_DESTROY_CQ) |
	    (1ull << IB_USER_VERBS_CMD_REQ_NOTIFY_CQ) |
	    (1ull << IB_USER_VERBS_CMD_CREATE_QP) |
	    (1ull << IB_USER_VERBS_CMD_MODIFY_QP) |
	    (1ull << IB_USER_VERBS_CMD_QUERY_QP) |
	    (1ull << IB_USER_VERBS_CMD_POLL_CQ) |
	    (1ull << IB_USER_VERBS_CMD_DESTROY_QP) |
	    (1ull << IB_USER_VERBS_CMD_POST_SEND) |
	    (1ull << IB_USER_VERBS_CMD_POST_RECV);
	ibdev->node_type = RDMA_NODE_RNIC;
	strlcpy(ibdev->node_desc, C4IW_NODE_DESC, sizeof(ibdev->node_desc));
	ibdev->phys_port_cnt = sc->params.nports;
	ibdev->num_comp_vectors = 1;
	ibdev->dma_device = sc->dev;
	ibdev->query_device = c4iw_query_device;
	ibdev->query_port = c4iw_query_port;
	ibdev->modify_port = c4iw_modify_port;
	ibdev->query_pkey = c4iw_query_pkey;
	ibdev->query_gid = c4iw_query_gid;
	ibdev->alloc_ucontext = c4iw_alloc_ucontext;
	ibdev->dealloc_ucontext = c4iw_dealloc_ucontext;
	ibdev->mmap = c4iw_mmap;
	ibdev->alloc_pd = c4iw_allocate_pd;
	ibdev->dealloc_pd = c4iw_deallocate_pd;
	ibdev->create_ah = c4iw_ah_create;
	ibdev->destroy_ah = c4iw_ah_destroy;
	ibdev->create_qp = c4iw_create_qp;
	ibdev->modify_qp = c4iw_ib_modify_qp;
	ibdev->query_qp = c4iw_ib_query_qp;
	ibdev->destroy_qp = c4iw_destroy_qp;
	ibdev->create_cq = c4iw_create_cq;
	ibdev->destroy_cq = c4iw_destroy_cq;
	ibdev->resize_cq = c4iw_resize_cq;
	ibdev->poll_cq = c4iw_poll_cq;
	ibdev->get_dma_mr = c4iw_get_dma_mr;
	ibdev->reg_phys_mr = c4iw_register_phys_mem;
	ibdev->rereg_phys_mr = c4iw_reregister_phys_mem;
	ibdev->reg_user_mr = c4iw_reg_user_mr;
	ibdev->dereg_mr = c4iw_dereg_mr;
	ibdev->alloc_mw = c4iw_alloc_mw;
	ibdev->bind_mw = c4iw_bind_mw;
	ibdev->dealloc_mw = c4iw_dealloc_mw;
	ibdev->alloc_fast_reg_mr = c4iw_alloc_fast_reg_mr;
	ibdev->alloc_fast_reg_page_list = c4iw_alloc_fastreg_pbl;
	ibdev->free_fast_reg_page_list = c4iw_free_fastreg_pbl;
	ibdev->attach_mcast = c4iw_multicast_attach;
	ibdev->detach_mcast = c4iw_multicast_detach;
	ibdev->process_mad = c4iw_process_mad;
	ibdev->req_notify_cq = c4iw_arm_cq;
	ibdev->post_send = c4iw_post_send;
	ibdev->post_recv = c4iw_post_receive;
	ibdev->uverbs_abi_ver = C4IW_UVERBS_ABI_VERSION;

	iwcm = kmalloc(sizeof(*iwcm), GFP_KERNEL);
	if (iwcm == NULL)
		return (-ENOMEM);

	iwcm->connect = c4iw_connect;
	iwcm->accept = c4iw_accept_cr;
	iwcm->reject = c4iw_reject_cr;
	iwcm->create_listen = c4iw_create_listen;
	iwcm->destroy_listen = c4iw_destroy_listen;
	iwcm->add_ref = c4iw_qp_add_ref;
	iwcm->rem_ref = c4iw_qp_rem_ref;
	iwcm->get_qp = c4iw_get_qp;
	ibdev->iwcm = iwcm;

	ret = ib_register_device(&dev->ibdev, NULL);
	if (ret)
		kfree(iwcm);

	return (ret);
}

void
c4iw_unregister_device(struct c4iw_dev *dev)
{

	CTR3(KTR_IW_CXGBE, "%s c4iw_dev %p, adapter %p", __func__, dev,
	    dev->rdev.adap);
	ib_unregister_device(&dev->ibdev);
	kfree(dev->ibdev.iwcm);
	return;
}
#endif
