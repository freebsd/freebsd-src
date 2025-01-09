/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic.h"
#include "vnic_dev.h"
#include "vnic_cq.h"

void vnic_cq_init(struct vnic_cq *cq, unsigned int flow_control_enable,
	unsigned int color_enable, unsigned int cq_head, unsigned int cq_tail,
	unsigned int cq_tail_color, unsigned int interrupt_enable,
	unsigned int cq_entry_enable, unsigned int cq_message_enable,
	unsigned int interrupt_offset, u64 cq_message_addr)
{
	u64 paddr;

	paddr = (u64)cq->ring.base_addr | VNIC_PADDR_TARGET;
	ENIC_BUS_WRITE_8(cq->ctrl, CQ_RING_BASE, paddr);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_RING_SIZE, cq->ring.desc_count);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_FLOW_CONTROL_ENABLE, flow_control_enable);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_COLOR_ENABLE, color_enable);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_HEAD, cq_head);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_TAIL, cq_tail);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_TAIL_COLOR, cq_tail_color);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_INTR_ENABLE, interrupt_enable);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_ENTRY_ENABLE, cq_entry_enable);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_MESSAGE_ENABLE, cq_message_enable);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_INTR_OFFSET, interrupt_offset);
	ENIC_BUS_WRITE_8(cq->ctrl, CQ_MESSAGE_ADDR, cq_message_addr);

	cq->interrupt_offset = interrupt_offset;
}

void vnic_cq_clean(struct vnic_cq *cq)
{
	cq->to_clean = 0;
	cq->last_color = 0;

	ENIC_BUS_WRITE_4(cq->ctrl, CQ_HEAD, 0);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_TAIL, 0);
	ENIC_BUS_WRITE_4(cq->ctrl, CQ_TAIL_COLOR, 1);
}
