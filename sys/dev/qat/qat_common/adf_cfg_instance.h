/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_CFG_INSTANCE_H_
#define ADF_CFG_INSTANCE_H_

#include "adf_accel_devices.h"
#include "adf_cfg_common.h"
#include "adf_cfg_bundle.h"

void crypto_instance_init(struct adf_cfg_instance *instance,
			  struct adf_cfg_bundle *bundle);
void dc_instance_init(struct adf_cfg_instance *instance,
		      struct adf_cfg_bundle *bundle);
void asym_instance_init(struct adf_cfg_instance *instance,
			struct adf_cfg_bundle *bundle);
void sym_instance_init(struct adf_cfg_instance *instance,
		       struct adf_cfg_bundle *bundle);
#endif
