/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2023 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_UIO_H
#define ADF_UIO_H
#include "adf_accel_devices.h"

struct qat_uio_bundle_dev {
	u8 hardware_bundle_number;
	struct adf_uio_control_bundle *bundle;
	struct adf_uio_control_accel *accel;
};

int adf_uio_register(struct adf_accel_dev *accel_dev);
void adf_uio_remove(struct adf_accel_dev *accel_dev);

#endif /* end of include guard: ADF_UIO_H */
