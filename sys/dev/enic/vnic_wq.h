/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_WQ_H_
#define _VNIC_WQ_H_

#include "vnic_dev.h"
#include "vnic_cq.h"

/* Work queue control */
struct vnic_wq_ctrl {
	u64 ring_base;			/* 0x00 */
#define TX_RING_BASE			   0x00
	u32 ring_size;			/* 0x08 */
#define TX_RING_SIZE			   0x08
	u32 pad0;
	u32 posted_index;		/* 0x10 */
#define TX_POSTED_INDEX			   0x10
	u32 pad1;
	u32 cq_index;			/* 0x18 */
#define TX_CQ_INDEX			   0x18
	u32 pad2;
	u32 enable;			/* 0x20 */
#define TX_ENABLE			   0x20
	u32 pad3;
	u32 running;			/* 0x28 */
#define TX_RUNNING			   0x28
	u32 pad4;
	u32 fetch_index;		/* 0x30 */
#define TX_FETCH_INDEX			   0x30
	u32 pad5;
	u32 dca_value;			/* 0x38 */
#define TX_DCA_VALUE			   0x38
	u32 pad6;
	u32 error_interrupt_enable;	/* 0x40 */
#define TX_ERROR_INTR_ENABLE		   0x40
	u32 pad7;
	u32 error_interrupt_offset;	/* 0x48 */
#define TX_ERROR_INTR_OFFSET		   0x48
	u32 pad8;
	u32 error_status;		/* 0x50 */
#define TX_ERROR_STATUS			   0x50
	u32 pad9;
};

struct vnic_wq {
	unsigned int index;
	uint64_t tx_offload_notsup_mask;
	struct vnic_dev *vdev;
	struct vnic_res *ctrl;
	struct vnic_dev_ring ring;
	unsigned int head_idx;
	unsigned int cq_pend;
	unsigned int tail_idx;
	unsigned int socket_id;
	unsigned int processed;
	const struct rte_memzone *cqmsg_rz;
	uint16_t last_completed_index;
	uint64_t offloads;
};

struct devcmd2_controller {
	struct vnic_res *wq_ctrl;
	struct vnic_devcmd2 *cmd_ring;
	struct devcmd2_result *result;
	u16 next_result;
	u16 result_size;
	int color;
	struct vnic_dev_ring results_ring;
	struct vnic_res *results_ctrl;
	struct vnic_wq wq;
	u32 posted;
};


static inline unsigned int vnic_wq_desc_avail(struct vnic_wq *wq)
{
	/* how many does SW own? */
	return wq->ring.desc_avail;
}

static inline unsigned int vnic_wq_desc_used(struct vnic_wq *wq)
{
	/* how many does HW own? */
	return wq->ring.desc_count - wq->ring.desc_avail - 1;
}

#define PI_LOG2_CACHE_LINE_SIZE        5
#define PI_INDEX_BITS            12
#define PI_INDEX_MASK ((1U << PI_INDEX_BITS) - 1)
#define PI_PREFETCH_LEN_MASK ((1U << PI_LOG2_CACHE_LINE_SIZE) - 1)
#define PI_PREFETCH_LEN_OFF 16
#define PI_PREFETCH_ADDR_BITS 43
#define PI_PREFETCH_ADDR_MASK ((1ULL << PI_PREFETCH_ADDR_BITS) - 1)
#define PI_PREFETCH_ADDR_OFF 21

static inline uint32_t
buf_idx_incr(uint32_t n_descriptors, uint32_t idx)
{
	idx++;
	if (unlikely(idx == n_descriptors))
		idx = 0;
	return idx;
}

void vnic_wq_free(struct vnic_wq *wq);
void enic_wq_init_start(struct vnic_wq *wq, unsigned int cq_index,
    unsigned int fetch_index, unsigned int posted_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset);
void vnic_wq_init(struct vnic_wq *wq, unsigned int cq_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset);
void vnic_wq_error_out(struct vnic_wq *wq, unsigned int error);
unsigned int vnic_wq_error_status(struct vnic_wq *wq);
void vnic_wq_enable(struct vnic_wq *wq);
int vnic_wq_disable(struct vnic_wq *wq);
void vnic_wq_clean(struct vnic_wq *wq);
int enic_wq_devcmd2_alloc(struct vnic_dev *vdev, struct vnic_wq *wq,
    unsigned int desc_count, unsigned int desc_size);

#endif /* _VNIC_WQ_H_ */
