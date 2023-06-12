/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_GEN4_TIMER_H_
#define ADF_GEN4_TIMER_H_

struct adf_accel_dev;

struct adf_hb_timer_data {
	struct adf_accel_dev *accel_dev;
	struct work_struct hb_int_timer_work;
};

int adf_int_timer_init(struct adf_accel_dev *accel_dev);
void adf_int_timer_exit(struct adf_accel_dev *accel_dev);

#endif /* ADF_GEN4_TIMER_H_ */
