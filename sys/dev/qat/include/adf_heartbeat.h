/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_HEARTBEAT_H_
#define ADF_HEARTBEAT_H_

#include "adf_cfg_common.h"

struct adf_accel_dev;

struct qat_sysctl {
	unsigned int hb_sysctlvar;
	struct sysctl_oid *oid;
};

struct adf_heartbeat {
	unsigned int hb_sent_counter;
	unsigned int hb_failed_counter;
	u64 last_hb_check_time;
	enum adf_device_heartbeat_status last_hb_status;
	struct qat_sysctl heartbeat;
	struct qat_sysctl *heartbeat_sent;
	struct qat_sysctl *heartbeat_failed;
};

int adf_heartbeat_init(struct adf_accel_dev *accel_dev);
void adf_heartbeat_clean(struct adf_accel_dev *accel_dev);

int adf_get_hb_timer(struct adf_accel_dev *accel_dev, unsigned int *value);
int adf_get_heartbeat_status(struct adf_accel_dev *accel_dev);
int adf_heartbeat_status(struct adf_accel_dev *accel_dev,
			 enum adf_device_heartbeat_status *hb_status);
#endif /* ADF_HEARTBEAT_H_ */
