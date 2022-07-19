/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_CFG_SECTION_H_
#define ADF_CFG_SECTION_H_

#include <linux/rwsem.h>
#include "adf_accel_devices.h"
#include "adf_cfg_common.h"
#include "adf_cfg_strings.h"

int adf_cfg_process_section(struct adf_accel_dev *accel_dev,
			    const char *section_name,
			    int dev);

int adf_cfg_cleanup_section(struct adf_accel_dev *accel_dev,
			    const char *section_name,
			    int dev);
#endif
