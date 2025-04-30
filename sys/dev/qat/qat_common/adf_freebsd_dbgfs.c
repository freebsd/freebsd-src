/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

#include "adf_accel_devices.h"
#include "adf_cfg_dev_dbg.h"
#include "adf_cnvnr_freq_counters.h"
#include "adf_common_drv.h"
#include "adf_dbgfs.h"
#include "adf_fw_counters.h"
#include "adf_freebsd_pfvf_ctrs_dbg.h"
#include "adf_heartbeat_dbg.h"
#include "adf_ver_dbg.h"

/**
 * adf_dbgfs_init() - add persistent debugfs entries
 * @accel_dev:  Pointer to acceleration device.
 *
 * This function creates debugfs entries that are persistent through a device
 * state change (from up to down or vice versa).
 */
void
adf_dbgfs_init(struct adf_accel_dev *accel_dev)
{
	adf_cfg_dev_dbg_add(accel_dev);
}

/**
 * adf_dbgfs_exit() - remove persistent debugfs entries
 * @accel_dev:  Pointer to acceleration device.
 */
void
adf_dbgfs_exit(struct adf_accel_dev *accel_dev)
{
	adf_cfg_dev_dbg_remove(accel_dev);
}

/**
 * adf_dbgfs_add() - add non-persistent debugfs entries
 * @accel_dev:  Pointer to acceleration device.
 *
 * This function creates debugfs entries that are not persistent through
 * a device state change (from up to down or vice versa).
 */
void
adf_dbgfs_add(struct adf_accel_dev *accel_dev)
{
	if (!accel_dev->is_vf) {
		adf_heartbeat_dbg_add(accel_dev);
		adf_ver_dbg_add(accel_dev);
		adf_fw_counters_add(accel_dev);
		adf_cnvnr_freq_counters_add(accel_dev);
	}
}

/**
 * adf_dbgfs_rm() - remove non-persistent debugfs entries
 * @accel_dev:  Pointer to acceleration device.
 */
void
adf_dbgfs_rm(struct adf_accel_dev *accel_dev)
{
	if (!accel_dev->is_vf) {
		adf_cnvnr_freq_counters_remove(accel_dev);
		adf_fw_counters_remove(accel_dev);
		adf_ver_dbg_del(accel_dev);
		adf_heartbeat_dbg_del(accel_dev);
	}
}
