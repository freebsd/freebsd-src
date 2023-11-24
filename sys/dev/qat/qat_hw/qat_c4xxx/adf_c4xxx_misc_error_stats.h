/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_C4XXX_MISC_ERROR_STATS_H_
#define ADF_C4XXX_MISC_ERROR_STATS_H_

#include "adf_accel_devices.h"

int adf_misc_error_add_c4xxx(struct adf_accel_dev *accel_dev);
void adf_misc_error_remove_c4xxx(struct adf_accel_dev *accel_dev);

#endif
