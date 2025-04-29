/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic.h"
#include "vnic_dev.h"
#include "vnic_rq.h"

void vnic_rq_init_start(struct vnic_rq *rq, unsigned int cq_index,
    unsigned int fetch_index, unsigned int posted_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset)
{
	u64 paddr;
	unsigned int count = rq->ring.desc_count;

	paddr = (u64)rq->ring.base_addr | VNIC_PADDR_TARGET;
	ENIC_BUS_WRITE_8(rq->ctrl, RX_RING_BASE, paddr);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_RING_SIZE, count);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_CQ_INDEX, cq_index);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_ERROR_INTR_ENABLE, error_interrupt_enable);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_ERROR_INTR_OFFSET, error_interrupt_offset);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_ERROR_STATUS, 0);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_FETCH_INDEX, fetch_index);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_POSTED_INDEX, posted_index);
}

void vnic_rq_init(struct vnic_rq *rq, unsigned int cq_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset)
{
	u32 fetch_index = 0;

	/* Use current fetch_index as the ring starting point */
	fetch_index = ENIC_BUS_READ_4(rq->ctrl, RX_FETCH_INDEX);

	if (fetch_index == 0xFFFFFFFF) { /* check for hardware gone  */
		/* Hardware surprise removal: reset fetch_index */
		fetch_index = 0;
	}

	fetch_index = 0;
	vnic_rq_init_start(rq, cq_index,
		fetch_index, fetch_index,
		error_interrupt_enable,
		error_interrupt_offset);
	rq->rxst_idx = 0;
	rq->tot_pkts = 0;
}

unsigned int vnic_rq_error_status(struct vnic_rq *rq)
{
	return (ENIC_BUS_READ_4(rq->ctrl, RX_ERROR_STATUS));
}

void vnic_rq_enable(struct vnic_rq *rq)
{
	ENIC_BUS_WRITE_4(rq->ctrl, RX_ENABLE, 1);
}

int vnic_rq_disable(struct vnic_rq *rq)
{
	unsigned int wait;

	ENIC_BUS_WRITE_4(rq->ctrl, RX_ENABLE, 0);

	/* Wait for HW to ACK disable request */
	for (wait = 0; wait < 1000; wait++) {
		if (!(ENIC_BUS_READ_4(rq->ctrl, RX_RUNNING)))
			return 0;
		udelay(10);
	}

	pr_err("Failed to disable RQ[%d]\n", rq->index);

	return (ETIMEDOUT);
}

void vnic_rq_clean(struct vnic_rq *rq)
{
	u32 fetch_index;
	unsigned int count = rq->ring.desc_count;

	rq->ring.desc_avail = count - 1;
	rq->rx_nb_hold = 0;

	/* Use current fetch_index as the ring starting point */
	fetch_index = ENIC_BUS_READ_4(rq->ctrl, RX_FETCH_INDEX);
	if (fetch_index == 0xFFFFFFFF) { /* check for hardware gone  */
		/* Hardware surprise removal: reset fetch_index */
		fetch_index = 0;
	}

	ENIC_BUS_WRITE_4(rq->ctrl, RX_POSTED_INDEX, fetch_index);
}
