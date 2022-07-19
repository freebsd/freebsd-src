/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_CNVNR_CTRS_DBG_H_
#define ADF_CNVNR_CTRS_DBG_H_

struct adf_accel_dev;
int adf_cnvnr_freq_counters_add(struct adf_accel_dev *accel_dev);
void adf_cnvnr_freq_counters_remove(struct adf_accel_dev *accel_dev);

#endif /* ADF_CNVNR_CTRS_DBG_H_ */
