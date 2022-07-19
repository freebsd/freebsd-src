/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_CFG_H_
#define ADF_CFG_H_

#include <linux/rwsem.h>
#include "adf_accel_devices.h"
#include "adf_cfg_common.h"
#include "adf_cfg_strings.h"

struct adf_cfg_key_val {
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	enum adf_cfg_val_type type;
	struct list_head list;
};

struct adf_cfg_section {
	char name[ADF_CFG_MAX_SECTION_LEN_IN_BYTES];
	bool processed;
	bool is_derived;
	struct list_head list;
	struct list_head param_head;
};

struct adf_cfg_device_data {
	struct adf_cfg_device *dev;
	struct list_head sec_list;
	struct sysctl_oid *debug;
	struct sx lock;
};

struct adf_cfg_depot_list {
	struct list_head sec_list;
};

int adf_cfg_dev_add(struct adf_accel_dev *accel_dev);
void adf_cfg_dev_remove(struct adf_accel_dev *accel_dev);
int adf_cfg_depot_restore_all(struct adf_accel_dev *accel_dev,
			      struct adf_cfg_depot_list *dev_hp_cfg);
int adf_cfg_section_add(struct adf_accel_dev *accel_dev, const char *name);
void adf_cfg_del_all(struct adf_accel_dev *accel_dev);
void adf_cfg_depot_del_all(struct list_head *head);
int adf_cfg_add_key_value_param(struct adf_accel_dev *accel_dev,
				const char *section_name,
				const char *key,
				const void *val,
				enum adf_cfg_val_type type);
int adf_cfg_get_param_value(struct adf_accel_dev *accel_dev,
			    const char *section,
			    const char *name,
			    char *value);
int adf_cfg_save_section(struct adf_accel_dev *accel_dev,
			 const char *name,
			 struct adf_cfg_section *section);
int adf_cfg_depot_save_all(struct adf_accel_dev *accel_dev,
			   struct adf_cfg_depot_list *dev_hp_cfg);
struct adf_cfg_section *adf_cfg_sec_find(struct adf_accel_dev *accel_dev,
					 const char *sec_name);
int adf_cfg_derived_section_add(struct adf_accel_dev *accel_dev,
				const char *name);
int adf_cfg_remove_key_param(struct adf_accel_dev *accel_dev,
			     const char *section_name,
			     const char *key);
int adf_cfg_setup_irq(struct adf_accel_dev *accel_dev);
void adf_cfg_set_asym_rings_mask(struct adf_accel_dev *accel_dev);
void adf_cfg_gen_dispatch_arbiter(struct adf_accel_dev *accel_dev,
				  const u32 *thrd_to_arb_map,
				  u32 *thrd_to_arb_map_gen,
				  u32 total_engines);
int adf_cfg_get_fw_image_type(struct adf_accel_dev *accel_dev,
			      enum adf_cfg_fw_image_type *fw_image_type);
int adf_cfg_get_services_enabled(struct adf_accel_dev *accel_dev,
				 u16 *ring_to_svc_map);
int adf_cfg_restore_section(struct adf_accel_dev *accel_dev,
			    struct adf_cfg_section *section);
void adf_cfg_keyval_del_all(struct list_head *head);
#endif
