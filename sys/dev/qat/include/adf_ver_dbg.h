/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_VER_DBG_H_
#define ADF_VER_DBG_H_

struct adf_accel_dev;
int adf_ver_dbg_add(struct adf_accel_dev *accel_dev);
void adf_ver_dbg_del(struct adf_accel_dev *accel_dev);

#endif /* ADF_VER_DBG_H_ */
