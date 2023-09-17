/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_GEN4_PFVF_H
#define ADF_GEN4_PFVF_H

#include "adf_accel_devices.h"

void adf_gen4_init_vf_pfvf_ops(struct adf_pfvf_ops *pfvf_ops);
static inline void
adf_gen4_init_pf_pfvf_ops(struct adf_pfvf_ops *pfvf_ops)
{
	pfvf_ops->enable_comms = adf_pfvf_comms_disabled;
}

#endif /* ADF_GEN4_PFVF_H */
