/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_RAS_H
#define ADF_RAS_H

#include <linux/types.h>

#define ADF_RAS_CORR 0
#define ADF_RAS_UNCORR 1
#define ADF_RAS_FATAL 2
#define ADF_RAS_ERRORS 3

struct adf_accel_dev;

int adf_init_ras(struct adf_accel_dev *accel_dev);
void adf_exit_ras(struct adf_accel_dev *accel_dev);
bool adf_ras_interrupts(struct adf_accel_dev *accel_dev, bool *reset_required);

#endif /* ADF_RAS_H */
