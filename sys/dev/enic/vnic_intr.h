/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_INTR_H_
#define _VNIC_INTR_H_


#include "vnic_dev.h"

#define VNIC_INTR_TIMER_TYPE_ABS	0
#define VNIC_INTR_TIMER_TYPE_QUIET	1

/* Interrupt control */
struct vnic_intr_ctrl {
	u32 coalescing_timer;		/* 0x00 */
#define INTR_COALESCING_TIMER		   0x00
	u32 pad0;
	u32 coalescing_value;		/* 0x08 */
#define INTR_COALESCING_VALUE		   0x08
	u32 pad1;
	u32 coalescing_type;		/* 0x10 */
#define INTR_COALESCING_TYPE		   0x10
	u32 pad2;
	u32 mask_on_assertion;		/* 0x18 */
#define INTR_MASK_ON_ASSERTION		   0x18
	u32 pad3;
	u32 mask;			/* 0x20 */
#define INTR_MASK			   0x20
	u32 pad4;
	u32 int_credits;		/* 0x28 */
#define INTR_CREDITS			   0x28
	u32 pad5;
	u32 int_credit_return;		/* 0x30 */
#define INTR_CREDIT_RETURN		   0x30
	u32 pad6;
};

struct vnic_intr {
	unsigned int index;
	struct vnic_dev *vdev;
	struct vnic_res *ctrl;
};

static inline void vnic_intr_mask(struct vnic_intr *intr)
{
	ENIC_BUS_WRITE_4(intr->ctrl, INTR_MASK, 1);
}

static inline int vnic_intr_masked(struct vnic_intr *intr)
{
	int ret;

	ret = ENIC_BUS_READ_4(intr->ctrl, INTR_MASK);
	return ret;
}

static inline void vnic_intr_unmask(struct vnic_intr *intr)
{
	ENIC_BUS_WRITE_4(intr->ctrl, INTR_MASK, 0);
}

static inline void vnic_intr_return_credits(struct vnic_intr *intr,
	unsigned int credits, int unmask, int reset_timer)
{
#define VNIC_INTR_UNMASK_SHIFT		16
#define VNIC_INTR_RESET_TIMER_SHIFT	17

	u32 int_credit_return = (credits & 0xffff) |
		(unmask ? (1 << VNIC_INTR_UNMASK_SHIFT) : 0) |
		(reset_timer ? (1 << VNIC_INTR_RESET_TIMER_SHIFT) : 0);

	ENIC_BUS_WRITE_4(intr->ctrl, INTR_CREDIT_RETURN, int_credit_return);
}

static inline unsigned int vnic_intr_credits(struct vnic_intr *intr)
{
	return (ENIC_BUS_READ_4(intr->ctrl, INTR_CREDITS));
}

static inline void vnic_intr_return_all_credits(struct vnic_intr *intr)
{
	unsigned int credits = vnic_intr_credits(intr);
	int unmask = 1;
	int reset_timer = 1;

	vnic_intr_return_credits(intr, credits, unmask, reset_timer);
}

void vnic_intr_free(struct vnic_intr *intr);
int vnic_intr_alloc(struct vnic_dev *vdev, struct vnic_intr *intr,
	unsigned int index);
void vnic_intr_init(struct vnic_intr *intr, u32 coalescing_timer,
	unsigned int coalescing_type, unsigned int mask_on_assertion);
void vnic_intr_coalescing_timer_set(struct vnic_intr *intr,
	u32 coalescing_timer);
void vnic_intr_clean(struct vnic_intr *intr);

#endif /* _VNIC_INTR_H_ */
