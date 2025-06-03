/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/bitmap.h>
#include <linux/rcupdate.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/iflib.h>

#include "hsi_struct_def.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ulp.h"

void bnxt_destroy_irq(struct bnxt_softc *softc);

static int bnxt_register_dev(struct bnxt_en_dev *edev, int ulp_id,
			     struct bnxt_ulp_ops *ulp_ops, void *handle)
{
	struct bnxt_softc *bp = edev->softc;
	struct bnxt_ulp *ulp;
	int rc = 0;

	if (ulp_id >= BNXT_MAX_ULP)
		return -EINVAL;

	mtx_lock(&bp->en_ops_lock);
	ulp = &edev->ulp_tbl[ulp_id];
	if (rcu_access_pointer(ulp->ulp_ops)) {
		device_printf(bp->dev, "ulp id %d already registered\n", ulp_id);
		rc = -EBUSY;
		goto exit;
	}

	edev->flags &= ~BNXT_EN_FLAG_ULP_STOPPED;
	atomic_set(&ulp->ref_count, 0);
	ulp->handle = handle;
	rcu_assign_pointer(ulp->ulp_ops, ulp_ops);

	if (ulp_id == BNXT_ROCE_ULP) {
		if (test_bit(BNXT_STATE_OPEN, &bp->state) && bp->is_dev_init)
			bnxt_hwrm_vnic_cfg(bp, &bp->vnic_info);
	}

exit:
	mtx_unlock(&bp->en_ops_lock);
	return rc;
}

static int bnxt_unregister_dev(struct bnxt_en_dev *edev, int ulp_id)
{
	struct bnxt_softc *bp = edev->softc;
	struct bnxt_ulp *ulp;
	int i = 0;

	if (ulp_id >= BNXT_MAX_ULP)
		return -EINVAL;

	ulp = &edev->ulp_tbl[ulp_id];
	if (!rcu_access_pointer(ulp->ulp_ops)) {
		device_printf(bp->dev, "ulp id %d not registered\n", ulp_id);
		return -EINVAL;
	}
	if (ulp_id == BNXT_ROCE_ULP && ulp->msix_requested)
		edev->en_ops->bnxt_free_msix(edev, ulp_id);

	mtx_lock(&bp->en_ops_lock);
	RCU_INIT_POINTER(ulp->ulp_ops, NULL);
	synchronize_rcu();
	ulp->max_async_event_id = 0;
	ulp->async_events_bmap = NULL;
	while (atomic_read(&ulp->ref_count) != 0 && i < 10) {
		msleep(100);
		i++;
	}
	mtx_unlock(&bp->en_ops_lock);
	return 0;
}

static void bnxt_fill_msix_vecs(struct bnxt_softc *bp, struct bnxt_msix_entry *ent)
{
	struct bnxt_en_dev *edev = bp->edev;
	int num_msix, idx, i;

	num_msix = edev->ulp_tbl[BNXT_ROCE_ULP].msix_requested;
	idx = edev->ulp_tbl[BNXT_ROCE_ULP].msix_base;
	for (i = 0; i < num_msix; i++) {
		ent[i].vector = bp->irq_tbl[idx + i].vector;
		ent[i].ring_idx = idx + i;
		if (BNXT_CHIP_P5_PLUS(bp))
			ent[i].db_offset = DB_PF_OFFSET_P5;
		else
			ent[i].db_offset = (idx + i) * 0x80;

	}
}

static int bnxt_req_msix_vecs(struct bnxt_en_dev *edev, int ulp_id,
			      struct bnxt_msix_entry *ent, int num_msix)
{
	struct bnxt_softc *bp = edev->softc;
	int avail_msix, idx;

	if (ulp_id != BNXT_ROCE_ULP)
		return -EINVAL;

	if (edev->ulp_tbl[ulp_id].msix_requested)
		return -EAGAIN;

	idx = bp->total_irqs - BNXT_ROCE_IRQ_COUNT;
	avail_msix = BNXT_ROCE_IRQ_COUNT;

	mtx_lock(&bp->en_ops_lock);
	edev->ulp_tbl[ulp_id].msix_base = idx;
	edev->ulp_tbl[ulp_id].msix_requested = avail_msix;

	bnxt_fill_msix_vecs(bp, ent);
	edev->flags |= BNXT_EN_FLAG_MSIX_REQUESTED;
	mtx_unlock(&bp->en_ops_lock);
	return avail_msix;
}

static int bnxt_free_msix_vecs(struct bnxt_en_dev *edev, int ulp_id)
{
	struct bnxt_softc *bp = edev->softc;

	if (ulp_id != BNXT_ROCE_ULP)
		return -EINVAL;

	if (!(edev->flags & BNXT_EN_FLAG_MSIX_REQUESTED))
		return 0;

	mtx_lock(&bp->en_ops_lock);
	edev->ulp_tbl[ulp_id].msix_requested = 0;
	edev->flags &= ~BNXT_EN_FLAG_MSIX_REQUESTED;
	if (edev->flags & BNXT_EN_FLAG_ULP_STOPPED)
		goto stopped;

stopped:
	mtx_unlock(&bp->en_ops_lock);

	return 0;
}

int bnxt_get_ulp_msix_num(struct bnxt_softc *bp)
{
	if (bnxt_ulp_registered(bp->edev, BNXT_ROCE_ULP)) {
		struct bnxt_en_dev *edev = bp->edev;

		return edev->ulp_tbl[BNXT_ROCE_ULP].msix_requested;
	}
	return 0;
}

int bnxt_get_ulp_msix_base(struct bnxt_softc *bp)
{
	if (bnxt_ulp_registered(bp->edev, BNXT_ROCE_ULP)) {
		struct bnxt_en_dev *edev = bp->edev;

		if (edev->ulp_tbl[BNXT_ROCE_ULP].msix_requested)
			return edev->ulp_tbl[BNXT_ROCE_ULP].msix_base;
	}
	return 0;
}

static int bnxt_send_msg(struct bnxt_en_dev *edev, int ulp_id,
			 struct bnxt_fw_msg *fw_msg)
{
	struct bnxt_softc *softc = edev->softc;
	int rc;

	if ((ulp_id != BNXT_ROCE_ULP) && softc->fw_reset_state)
		return -EBUSY;

	rc = bnxt_hwrm_passthrough(softc, fw_msg->msg, fw_msg->msg_len, fw_msg->resp,
				    fw_msg->resp_max_len, fw_msg->timeout);
	return rc;
}

static void bnxt_ulp_get(struct bnxt_ulp *ulp)
{
	atomic_inc(&ulp->ref_count);
}

static void bnxt_ulp_put(struct bnxt_ulp *ulp)
{
	atomic_dec(&ulp->ref_count);
}

void bnxt_ulp_stop(struct bnxt_softc *bp)
{
	struct bnxt_en_dev *edev = bp->edev;
	struct bnxt_ulp_ops *ops;
	int i;

	if (!edev)
		return;

	edev->flags |= BNXT_EN_FLAG_ULP_STOPPED;
	edev->en_state = bp->state;
	for (i = 0; i < BNXT_MAX_ULP; i++) {
		struct bnxt_ulp *ulp = &edev->ulp_tbl[i];

		ops = ulp->ulp_ops;
		if (!ops || !ops->ulp_stop)
			continue;
		ops->ulp_stop(ulp->handle);
	}
}

void bnxt_ulp_start(struct bnxt_softc *bp, int err)
{
	struct bnxt_en_dev *edev = bp->edev;
	struct bnxt_ulp_ops *ops;
	int i;

	if (!edev)
		return;

	edev->flags &= ~BNXT_EN_FLAG_ULP_STOPPED;
	edev->en_state = bp->state;

	if (err)
		return;

	for (i = 0; i < BNXT_MAX_ULP; i++) {
		struct bnxt_ulp *ulp = &edev->ulp_tbl[i];

		ops = ulp->ulp_ops;
		if (!ops || !ops->ulp_start)
			continue;
		ops->ulp_start(ulp->handle);
	}
}

void bnxt_ulp_sriov_cfg(struct bnxt_softc *bp, int num_vfs)
{
	struct bnxt_en_dev *edev = bp->edev;
	struct bnxt_ulp_ops *ops;
	int i;

	if (!edev)
		return;

	for (i = 0; i < BNXT_MAX_ULP; i++) {
		struct bnxt_ulp *ulp = &edev->ulp_tbl[i];

		rcu_read_lock();
		ops = rcu_dereference(ulp->ulp_ops);
		if (!ops || !ops->ulp_sriov_config) {
			rcu_read_unlock();
			continue;
		}
		bnxt_ulp_get(ulp);
		rcu_read_unlock();
		ops->ulp_sriov_config(ulp->handle, num_vfs);
		bnxt_ulp_put(ulp);
	}
}

void bnxt_ulp_shutdown(struct bnxt_softc *bp)
{
	struct bnxt_en_dev *edev = bp->edev;
	struct bnxt_ulp_ops *ops;
	int i;

	if (!edev)
		return;

	for (i = 0; i < BNXT_MAX_ULP; i++) {
		struct bnxt_ulp *ulp = &edev->ulp_tbl[i];

		ops = ulp->ulp_ops;
		if (!ops || !ops->ulp_shutdown)
			continue;
		ops->ulp_shutdown(ulp->handle);
	}
}

void bnxt_ulp_irq_stop(struct bnxt_softc *bp)
{
	struct bnxt_en_dev *edev = bp->edev;
	struct bnxt_ulp_ops *ops;

	if (!edev || !(edev->flags & BNXT_EN_FLAG_MSIX_REQUESTED))
		return;

	if (bnxt_ulp_registered(bp->edev, BNXT_ROCE_ULP)) {
		struct bnxt_ulp *ulp = &edev->ulp_tbl[BNXT_ROCE_ULP];

		if (!ulp->msix_requested)
			return;

		ops = ulp->ulp_ops;
		if (!ops || !ops->ulp_irq_stop)
			return;
		ops->ulp_irq_stop(ulp->handle);
	}
}

void bnxt_ulp_async_events(struct bnxt_softc *bp, struct hwrm_async_event_cmpl *cmpl)
{
	u16 event_id = le16_to_cpu(cmpl->event_id);
	struct bnxt_en_dev *edev = bp->edev;
	struct bnxt_ulp_ops *ops;
	int i;

	if (!edev)
		return;

	rcu_read_lock();
	for (i = 0; i < BNXT_MAX_ULP; i++) {
		struct bnxt_ulp *ulp = &edev->ulp_tbl[i];

		ops = rcu_dereference(ulp->ulp_ops);
		if (!ops || !ops->ulp_async_notifier)
			continue;
		if (!ulp->async_events_bmap ||
		    event_id > ulp->max_async_event_id)
			continue;

		/* Read max_async_event_id first before testing the bitmap. */
		rmb();
		if (edev->flags & BNXT_EN_FLAG_ULP_STOPPED)
			continue;

		if (test_bit(event_id, ulp->async_events_bmap))
			ops->ulp_async_notifier(ulp->handle, cmpl);
	}
	rcu_read_unlock();
}

static int bnxt_register_async_events(struct bnxt_en_dev *edev, int ulp_id,
				      unsigned long *events_bmap, u16 max_id)
{
	struct bnxt_softc *bp = edev->softc;
	struct bnxt_ulp *ulp;

	if (ulp_id >= BNXT_MAX_ULP)
		return -EINVAL;

	mtx_lock(&bp->en_ops_lock);
	ulp = &edev->ulp_tbl[ulp_id];
	ulp->async_events_bmap = events_bmap;
	wmb();
	ulp->max_async_event_id = max_id;
	bnxt_hwrm_func_drv_rgtr(bp, events_bmap, max_id + 1, true);
	mtx_unlock(&bp->en_ops_lock);
	return 0;
}

void bnxt_destroy_irq(struct bnxt_softc *softc)
{
	kfree(softc->irq_tbl);
}

static int bnxt_populate_irq(struct bnxt_softc *softc)
{
	struct resource_list *rl = NULL;
	struct resource_list_entry *rle = NULL;
	struct bnxt_msix_tbl *irq_tbl = NULL;
	struct pci_devinfo *dinfo = NULL;
	int i;

	softc->total_irqs = softc->scctx->isc_nrxqsets + BNXT_ROCE_IRQ_COUNT;
	irq_tbl = kzalloc(softc->total_irqs * sizeof(*softc->irq_tbl), GFP_KERNEL);

	if (!irq_tbl) {
		device_printf(softc->dev, "Failed to allocate IRQ table\n");
		return -1;
	}
	dinfo = device_get_ivars(softc->pdev->dev.bsddev);
	rl = &dinfo->resources;
	rle = resource_list_find(rl, SYS_RES_IRQ, 1);
	softc->pdev->dev.irq_start = rle->start;
	softc->pdev->dev.irq_end = rle->start + softc->total_irqs;

	for (i = 0; i < softc->total_irqs; i++) {
		irq_tbl[i].entry = i;
		irq_tbl[i].vector = softc->pdev->dev.irq_start + i;
	}

	softc->irq_tbl = irq_tbl;

	return 0;
}

static const struct bnxt_en_ops bnxt_en_ops_tbl = {
	.bnxt_register_device	= bnxt_register_dev,
	.bnxt_unregister_device	= bnxt_unregister_dev,
	.bnxt_request_msix	= bnxt_req_msix_vecs,
	.bnxt_free_msix		= bnxt_free_msix_vecs,
	.bnxt_send_fw_msg	= bnxt_send_msg,
	.bnxt_register_fw_async_events	= bnxt_register_async_events,
};

void bnxt_aux_dev_release(struct device *dev)
{
	struct bnxt_aux_dev *bnxt_adev =
		container_of(dev, struct bnxt_aux_dev, aux_dev.dev);
	struct bnxt_softc *bp = bnxt_adev->edev->softc;

	kfree(bnxt_adev->edev);
	bnxt_adev->edev = NULL;
	bp->edev = NULL;
}

static inline void bnxt_set_edev_info(struct bnxt_en_dev *edev, struct bnxt_softc *bp)
{
	edev->en_ops = &bnxt_en_ops_tbl;
	edev->net = bp->ifp;
	edev->pdev = bp->pdev;
	edev->softc = bp;
	edev->l2_db_size = bp->db_size;
	mtx_init(&bp->en_ops_lock, "Ethernet ops lock", NULL, MTX_DEF);

	if (bp->flags & BNXT_FLAG_ROCEV1_CAP)
		edev->flags |= BNXT_EN_FLAG_ROCEV1_CAP;
	if (bp->flags & BNXT_FLAG_ROCEV2_CAP)
		edev->flags |= BNXT_EN_FLAG_ROCEV2_CAP;
	if (bp->is_asym_q)
		edev->flags |= BNXT_EN_FLAG_ASYM_Q;
	edev->hwrm_bar = bp->hwrm_bar;
	edev->port_partition_type = bp->port_partition_type;
	edev->ulp_version = BNXT_ULP_VERSION;
}

int bnxt_rdma_aux_device_del(struct bnxt_softc *softc)
{
	struct bnxt_aux_dev *bnxt_adev = softc->aux_dev;
	struct auxiliary_device *adev;

	adev = &bnxt_adev->aux_dev;
	auxiliary_device_delete(adev);
	auxiliary_device_uninit(adev);
	bnxt_destroy_irq(softc);

	return 0;
}

int bnxt_rdma_aux_device_add(struct bnxt_softc *bp)
{
	struct bnxt_aux_dev *bnxt_adev = bp->aux_dev;
	struct bnxt_en_dev *edev = bnxt_adev->edev;
	struct auxiliary_device *aux_dev;
	int ret = -1;

	if (bnxt_populate_irq(bp))
		return ret;

	device_printf(bp->dev, "V:D:SV:SD %x:%x:%x:%x, irq 0x%x, "
		      "devfn 0x%x, cla 0x%x, rev 0x%x, msi_en 0x%x\n",
		      bp->pdev->vendor, bp->pdev->device, bp->pdev->subsystem_vendor,
		      bp->pdev->subsystem_device, bp->pdev->irq, bp->pdev->devfn,
		      bp->pdev->class, bp->pdev->revision, bp->pdev->msi_enabled);

	aux_dev = &bnxt_adev->aux_dev;
	aux_dev->id = bnxt_adev->id;
	aux_dev->name = "rdma";
	aux_dev->dev.parent = &bp->pdev->dev;
	aux_dev->dev.release = bnxt_aux_dev_release;

	if (!edev) {
		edev = kzalloc(sizeof(*edev), GFP_KERNEL);
		if (!edev)
			return -ENOMEM;
	}

	bnxt_set_edev_info(edev, bp);
	bnxt_adev->edev = edev;
	bp->edev = edev;

	ret = auxiliary_device_init(aux_dev);
	if (ret)
		goto err_free_edev;

	ret = auxiliary_device_add(aux_dev);
	if (ret)
		goto err_dev_uninit;

	return 0;
err_dev_uninit:
	auxiliary_device_uninit(aux_dev);
err_free_edev:
	kfree(edev);
	bnxt_adev->edev = NULL;
	bp->edev = NULL;
	return ret;
}
