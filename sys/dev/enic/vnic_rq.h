/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_RQ_H_
#define _VNIC_RQ_H_

#include "vnic_dev.h"
#include "vnic_cq.h"

/* Receive queue control */
struct vnic_rq_ctrl {
	u64 ring_base;			/* 0x00 */
#define RX_RING_BASE			   0x00
	u32 ring_size;			/* 0x08 */
#define RX_RING_SIZE			   0x08
	u32 pad0;
	u32 posted_index;		/* 0x10 */
#define RX_POSTED_INDEX			   0x10
	u32 pad1;
	u32 cq_index;			/* 0x18 */
#define RX_CQ_INDEX			   0x18
	u32 pad2;
	u32 enable;			/* 0x20 */
#define RX_ENABLE			   0x20
	u32 pad3;
	u32 running;			/* 0x28 */
#define RX_RUNNING			   0x28
	u32 pad4;
	u32 fetch_index;		/* 0x30 */
#define RX_FETCH_INDEX			   0x30
	u32 pad5;
	u32 error_interrupt_enable;	/* 0x38 */
#define RX_ERROR_INTR_ENABLE		   0x38
	u32 pad6;
	u32 error_interrupt_offset;	/* 0x40 */
#define RX_ERROR_INTR_OFFSET		   0x40
	u32 pad7;
	u32 error_status;		/* 0x48 */
#define RX_ERROR_STATUS			   0x48
	u32 pad8;
	u32 tcp_sn;			/* 0x50 */
#define RX_TCP_SN			   0x50
	u32 pad9;
	u32 unused;			/* 0x58 */
	u32 pad10;
	u32 dca_select;			/* 0x60 */
#define RX_DCA_SELECT			   0x60
	u32 pad11;
	u32 dca_value;			/* 0x68 */
#define RX_DCA_VALUE			   0x68
	u32 pad12;
	u32 data_ring;			/* 0x70 */
};

struct vnic_rq {
	unsigned int index;
	unsigned int posted_index;
	struct vnic_dev *vdev;
	struct vnic_res *ctrl;
	struct vnic_dev_ring ring;
	int num_free_mbufs;
	struct rte_mbuf **mbuf_ring;		/* array of allocated mbufs */
	unsigned int mbuf_next_idx;		/* next mb to consume */
	void *os_buf_head;
	unsigned int pkts_outstanding;
	uint16_t rx_nb_hold;
	uint16_t rx_free_thresh;
	unsigned int socket_id;
	struct rte_mempool *mp;
	uint16_t rxst_idx;
	uint32_t tot_pkts;
	uint8_t in_use;
	unsigned int max_mbufs_per_pkt;
	uint16_t tot_nb_desc;
	bool need_initial_post;
	struct iflib_dma_info data;
};

static inline unsigned int vnic_rq_desc_avail(struct vnic_rq *rq)
{
	/* how many does SW own? */
	return rq->ring.desc_avail;
}

static inline unsigned int vnic_rq_desc_used(struct vnic_rq *rq)
{
	/* how many does HW own? */
	return rq->ring.desc_count - rq->ring.desc_avail - 1;
}

enum desc_return_options {
	VNIC_RQ_RETURN_DESC,
	VNIC_RQ_DEFER_RETURN_DESC,
};

static inline int vnic_rq_fill(struct vnic_rq *rq,
	int (*buf_fill)(struct vnic_rq *rq))
{
	int err;

	while (vnic_rq_desc_avail(rq) > 0) {

		err = (*buf_fill)(rq);
		if (err)
			return err;
	}

	return 0;
}

static inline int vnic_rq_fill_count(struct vnic_rq *rq,
	int (*buf_fill)(struct vnic_rq *rq), unsigned int count)
{
	int err;

	while ((vnic_rq_desc_avail(rq) > 0) && (count--)) {

		err = (*buf_fill)(rq);
		if (err)
			return err;
	}

	return 0;
}

void vnic_rq_free(struct vnic_rq *rq);
void vnic_rq_init_start(struct vnic_rq *rq, unsigned int cq_index,
    unsigned int fetch_index, unsigned int posted_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset);
void vnic_rq_init(struct vnic_rq *rq, unsigned int cq_index,
    unsigned int error_interrupt_enable,
    unsigned int error_interrupt_offset);
unsigned int vnic_rq_error_status(struct vnic_rq *rq);
void vnic_rq_enable(struct vnic_rq *rq);
int vnic_rq_disable(struct vnic_rq *rq);
void vnic_rq_clean(struct vnic_rq *rq);

#endif /* _VNIC_RQ_H_ */
