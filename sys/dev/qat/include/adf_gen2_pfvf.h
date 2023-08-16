/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_GEN2_PFVF_H
#define ADF_GEN2_PFVF_H

#include <linux/types.h>
#include "adf_accel_devices.h"

#define ADF_GEN2_ERRSOU3 (0x3A000 + 0x0C)
#define ADF_GEN2_ERRSOU5 (0x3A000 + 0xD8)
#define ADF_GEN2_ERRMSK3 (0x3A000 + 0x1C)
#define ADF_GEN2_ERRMSK5 (0x3A000 + 0xDC)

static inline void
adf_gen2_init_pf_pfvf_ops(struct adf_pfvf_ops *pfvf_ops)
{
	pfvf_ops->enable_comms = adf_pfvf_comms_disabled;
}

static inline void
adf_gen2_init_vf_pfvf_ops(struct adf_pfvf_ops *pfvf_ops)
{
	pfvf_ops->enable_comms = adf_pfvf_comms_disabled;
}

#endif /* ADF_GEN2_PFVF_H */
