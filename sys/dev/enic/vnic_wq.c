/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic.h"
#include "vnic_dev.h"
#include "vnic_wq.h"

void vnic_wq_init_start(struct vnic_wq *wq, unsigned int cq_index,
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
	vnic_wq_init_start(wq, cq_index, 0, 0,
		error_interrupt_enable,
		error_interrupt_offset);
	wq->cq_pend = 0;
	wq->last_completed_index = 0;
}

unsigned int vnic_wq_error_status(struct vnic_wq *wq)
{
	return ENIC_BUS_READ_4(wq->ctrl, TX_ERROR_STATUS);
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

	return -ETIMEDOUT;
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

	vnic_dev_clear_desc_ring(&wq->ring);
}
