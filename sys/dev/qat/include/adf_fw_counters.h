/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_FW_COUNTERS_H_
#define ADF_FW_COUNTERS_H_

#include <linux/rwsem.h>
#include "adf_accel_devices.h"

#define FW_COUNTERS_MAX_STR_LEN 64
#define FW_COUNTERS_MAX_KEY_LEN_IN_BYTES FW_COUNTERS_MAX_STR_LEN
#define FW_COUNTERS_MAX_VAL_LEN_IN_BYTES FW_COUNTERS_MAX_STR_LEN
#define FW_COUNTERS_MAX_SECTION_LEN_IN_BYTES FW_COUNTERS_MAX_STR_LEN
#define ADF_FW_COUNTERS_NO_RESPONSE -1

struct adf_fw_counters_val {
	char key[FW_COUNTERS_MAX_KEY_LEN_IN_BYTES];
	char val[FW_COUNTERS_MAX_VAL_LEN_IN_BYTES];
	struct list_head list;
};

struct adf_fw_counters_section {
	char name[FW_COUNTERS_MAX_SECTION_LEN_IN_BYTES];
	struct list_head list;
	struct list_head param_head;
};

struct adf_fw_counters_data {
	struct list_head ae_sec_list;
	struct sysctl_oid *debug;
	struct rw_semaphore lock;
};

int adf_fw_counters_add(struct adf_accel_dev *accel_dev);
void adf_fw_counters_remove(struct adf_accel_dev *accel_dev);
int adf_fw_count_ras_event(struct adf_accel_dev *accel_dev,
			   u32 *ras_event,
			   char *aeidstr);

#endif /* ADF_FW_COUNTERS_H_ */
