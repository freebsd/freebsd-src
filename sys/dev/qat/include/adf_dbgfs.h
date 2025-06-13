/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

#ifndef ADF_DBGFS_H
#define ADF_DBGFS_H

void adf_dbgfs_init(struct adf_accel_dev *accel_dev);
void adf_dbgfs_add(struct adf_accel_dev *accel_dev);
void adf_dbgfs_rm(struct adf_accel_dev *accel_dev);
void adf_dbgfs_exit(struct adf_accel_dev *accel_dev);
#endif
