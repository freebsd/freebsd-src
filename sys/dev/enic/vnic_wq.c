/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic.h"
#include "vnic_dev.h"
#include "vnic_wq.h"

int vnic_dev_alloc_desc_ring(struct vnic_dev *vdev,
    struct vnic_dev_ring *ring, unsigned int desc_count, unsigned int desc_size)
{
	iflib_dma_info_t ifdip;
	int err;

	if ((ifdip = malloc(sizeof(struct iflib_dma_info),
	    M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		device_printf(dev_from_vnic_dev(vdev),
		"Unable to allocate DMA info memory\n");
		return (ENOMEM);
	}

	err = iflib_dma_alloc(vdev->softc->ctx, desc_count * desc_size,
	    ifdip, 0);
	if (err) {
		device_printf(dev_from_vnic_dev(vdev),
		    "Unable to allocate DEVCMD2 descriptors\n");
		err = ENOMEM;
		goto err_out_alloc;
	}

	ring->base_addr = ifdip->idi_paddr;
	ring->descs = ifdip->idi_vaddr;
	ring->ifdip = ifdip;
	ring->desc_size = desc_size;
	ring->desc_count = desc_count;
	ring->last_count = 0;
	ring->desc_avail = ring->desc_count - 1;

	ring->base_align = 512;
	ring->size_unaligned = ring->desc_count * ring->desc_size \
	    + ring->base_align;

	return (err);

	iflib_dma_free(ifdip);

err_out_alloc:
	free(ifdip, M_DEVBUF);
	return (err);
}

void vnic_dev_free_desc_ring(struct vnic_dev *vdev, struct vnic_dev_ring *ring)
{
	if (ring && ring->descs) {
		iflib_dma_free(ring->ifdip);
		free(ring->ifdip, M_DEVBUF);
		ring->descs = NULL;
	}
}

void vnic_wq_free(struct vnic_wq *wq) {
	vnic_dev_free_desc_ring(wq->vdev, &wq->ring);
	wq->ctrl = NULL;
}

int enic_wq_devcmd2_alloc(struct vnic_dev *vdev, struct vnic_wq *wq,
                          unsigned int desc_count, unsigned int desc_size)
{
	int err;

	wq->index = 0;
	wq->vdev = vdev;


	wq->ctrl = vnic_dev_get_res(vdev, RES_TYPE_DEVCMD2, 0);
	if (!wq->ctrl)
		return (EINVAL);
	vnic_wq_disable(wq);
	err = vnic_dev_alloc_desc_ring(vdev, &wq->ring, desc_count, desc_size);

	return (err);
}

void vnic_dev_deinit_devcmd2(struct vnic_dev *vdev)
{
	if (vdev->devcmd2) {
		vnic_wq_disable(&vdev->devcmd2->wq);
		if (vdev->devcmd2->wq_ctrl)
			vnic_wq_free(&vdev->devcmd2->wq);
		if (vdev->devcmd2->result)
			vnic_dev_free_desc_ring(vdev, &vdev->devcmd2->results_ring);
		free(vdev->devcmd2, M_DEVBUF);
		vdev->devcmd2 = NULL;
	}
}

int vnic_dev_deinit(struct vnic_dev *vdev) {
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	return (vnic_dev_cmd(vdev, CMD_DEINIT, &a0, &a1, wait));
	return (0);
}

void enic_wq_init_start(struct vnic_wq *wq, unsigned int cq_index,
    unsigned int fetch_index, unsigned int posted_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset)
{
	u64 paddr;
	unsigned int count = wq->ring.desc_count;

	paddr = (u64)wq->ring.base_addr | VNIC_PADDR_TARGET;
	ENIC_BUS_WRITE_8(wq->ctrl, TX_RING_BASE, paddr);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_RING_SIZE, count);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_FETCH_INDEX, fetch_index);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_POSTED_INDEX, posted_index);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_CQ_INDEX, cq_index);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_ERROR_INTR_ENABLE, error_interrupt_enable);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_ERROR_INTR_OFFSET, error_interrupt_offset);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_ERROR_STATUS, 0);

	wq->head_idx = fetch_index;
	wq->tail_idx = wq->head_idx;
}

void vnic_wq_init(struct vnic_wq *wq, unsigned int cq_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset)
{
	enic_wq_init_start(wq, cq_index, 0, 0,
		error_interrupt_enable,
		error_interrupt_offset);
	wq->cq_pend = 0;
	wq->last_completed_index = 0;
}

unsigned int vnic_wq_error_status(struct vnic_wq *wq)
{
	return (ENIC_BUS_READ_4(wq->ctrl, TX_ERROR_STATUS));
}

void vnic_wq_enable(struct vnic_wq *wq)
{
	ENIC_BUS_WRITE_4(wq->ctrl, TX_ENABLE, 1);
}

int vnic_wq_disable(struct vnic_wq *wq)
{
	unsigned int wait;

	ENIC_BUS_WRITE_4(wq->ctrl, TX_ENABLE, 0);

	/* Wait for HW to ACK disable request */
	for (wait = 0; wait < 1000; wait++) {
		if (!(ENIC_BUS_READ_4(wq->ctrl, TX_RUNNING)))
			return 0;
		udelay(10);
	}

	pr_err("Failed to disable WQ[%d]\n", wq->index);

	return (ETIMEDOUT);
}

void vnic_wq_clean(struct vnic_wq *wq)
{
	unsigned int  to_clean = wq->tail_idx;

	while (vnic_wq_desc_used(wq) > 0) {
		to_clean = buf_idx_incr(wq->ring.desc_count, to_clean);
		wq->ring.desc_avail++;
	}

	wq->head_idx = 0;
	wq->tail_idx = 0;
	wq->last_completed_index = 0;

	ENIC_BUS_WRITE_4(wq->ctrl, TX_FETCH_INDEX, 0);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_POSTED_INDEX, 0);
	ENIC_BUS_WRITE_4(wq->ctrl, TX_ERROR_STATUS, 0);
}
