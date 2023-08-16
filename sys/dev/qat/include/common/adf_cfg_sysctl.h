/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2023 Intel Corporation */
#ifndef ADF_CFG_SYSCTL_H_
#define ADF_CFG_SYSCTL_H_

#include "adf_accel_devices.h"

int adf_cfg_sysctl_add(struct adf_accel_dev *accel_dev);
void adf_cfg_sysctl_remove(struct adf_accel_dev *accel_dev);

#endif /* ADF_CFG_SYSCTL_H_ */
