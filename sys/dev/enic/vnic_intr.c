/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic.h"
#include "vnic_dev.h"
#include "vnic_intr.h"

void vnic_intr_free(struct vnic_intr *intr)
{
	intr->ctrl = NULL;
}

int vnic_intr_alloc(struct vnic_dev *vdev, struct vnic_intr *intr,
	unsigned int index)
{
	intr->index = index;
	intr->vdev = vdev;

	intr->ctrl = vnic_dev_get_res(vdev, RES_TYPE_INTR_CTRL, index);
	if (!intr->ctrl) {
		pr_err("Failed to hook INTR[%d].ctrl resource\n", index);
		return (EINVAL);
	}

	return 0;
}

void vnic_intr_init(struct vnic_intr *intr, u32 coalescing_timer,
	unsigned int coalescing_type, unsigned int mask_on_assertion)
{
	vnic_intr_coalescing_timer_set(intr, coalescing_timer);
	ENIC_BUS_WRITE_4(intr->ctrl, INTR_COALESCING_TYPE, coalescing_type);
	ENIC_BUS_WRITE_4(intr->ctrl, INTR_MASK_ON_ASSERTION, mask_on_assertion);
	ENIC_BUS_WRITE_4(intr->ctrl, INTR_CREDITS, 0);
}

void vnic_intr_coalescing_timer_set(struct vnic_intr *intr,
	u32 coalescing_timer)
{
	ENIC_BUS_WRITE_4(intr->ctrl, INTR_COALESCING_TIMER,
	    vnic_dev_intr_coal_timer_usec_to_hw(intr->vdev, coalescing_timer));
}

void vnic_intr_clean(struct vnic_intr *intr)
{
	ENIC_BUS_WRITE_4(intr->ctrl, INTR_CREDITS, 0);
}
