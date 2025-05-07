/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef ADF_CFG_DEVICE_H_
#define ADF_CFG_DEVICE_H_

#include "adf_cfg.h"
#include "sal_statistics_strings.h"

#define ADF_CFG_STATIC_CONF_VER 2
#define ADF_CFG_STATIC_CONF_CY_ASYM_RING_SIZE 64
#define ADF_CFG_STATIC_CONF_CY_SYM_RING_SIZE 512
#define ADF_CFG_STATIC_CONF_DC_INTER_BUF_SIZE 64
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_ENABLED 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DC 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DH 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DRBG 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DSA 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_ECC 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_KEYGEN 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_LN 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_PRIME 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_RSA 1
#define ADF_CFG_STATIC_CONF_SAL_STATS_CFG_SYM 1
#define ADF_CFG_STATIC_CONF_POLL 1
#define ADF_CFG_STATIC_CONF_IRQ 0
#define ADF_CFG_STATIC_CONF_AUTO_RESET 0
#define ADF_CFG_STATIC_CONF_NUM_DC_ACCEL_UNITS 2
#define ADF_CFG_STATIC_CONF_NUM_INLINE_ACCEL_UNITS 0
#define ADF_CFG_STATIC_CONF_INST_NUM_DC 2
#define ADF_CFG_STATIC_CONF_INST_NUM_CY_POLL 6
#define ADF_CFG_STATIC_CONF_INST_NUM_CY_IRQ 2
#define ADF_CFG_STATIC_CONF_USER_PROCESSES_NUM 2
#define ADF_CFG_STATIC_CONF_USER_INST_NUM_CY 6
#define ADF_CFG_STATIC_CONF_USER_INST_NUM_DC 2
#define ADF_CFG_STATIC_CONF_INST_NUM_CY_POLL_VF 1
#define ADF_CFG_STATIC_CONF_INST_NUM_CY_IRQ_VF 1
#define ADF_CFG_STATIC_CONF_INST_NUM_DC_VF 2
#define ADF_CFG_STATIC_CONF_USER_INST_NUM_CY_VF 2
#define ADF_CFG_STATIC_CONF_USER_INST_NUM_DC_VF 2

#define ADF_CFG_FW_STRING_TO_ID(str, acc, id)                                  \
	do {                                                                   \
		typeof(id) id_ = (id);                                         \
		typeof(str) str_;                                              \
		memcpy(str_, (str), sizeof(str_));                             \
		if (!strncmp(str_,                                             \
			     ADF_SERVICES_DEFAULT,                             \
			     sizeof(ADF_SERVICES_DEFAULT)))                    \
			*id_ = ADF_FW_IMAGE_DEFAULT;                           \
		else if (!strncmp(str_,                                        \
				  ADF_SERVICES_CRYPTO,                         \
				  sizeof(ADF_SERVICES_CRYPTO)))                \
			*id_ = ADF_FW_IMAGE_CRYPTO;                            \
		else if (!strncmp(str_,                                        \
				  ADF_SERVICES_COMPRESSION,                    \
				  sizeof(ADF_SERVICES_COMPRESSION)))           \
			*id_ = ADF_FW_IMAGE_COMPRESSION;                       \
		else if (!strncmp(str_,                                        \
				  ADF_SERVICES_CUSTOM1,                        \
				  sizeof(ADF_SERVICES_CUSTOM1)))               \
			*id_ = ADF_FW_IMAGE_CUSTOM1;                           \
		else {                                                         \
			*id_ = ADF_FW_IMAGE_DEFAULT;                           \
			device_printf(GET_DEV(acc),                            \
				      "Invalid SerivesProfile: %s,"            \
				      "Using DEFAULT image\n",                 \
				      str_);                                   \
		}                                                              \
	} while (0)

int adf_cfg_get_ring_pairs(struct adf_cfg_device *device,
			   struct adf_cfg_instance *inst,
			   const char *process_name,
			   struct adf_accel_dev *accel_dev);

int adf_cfg_device_init(struct adf_cfg_device *device,
			struct adf_accel_dev *accel_dev);

void adf_cfg_device_clear(struct adf_cfg_device *device,
			  struct adf_accel_dev *accel_dev);

void adf_cfg_device_clear_all(struct adf_accel_dev *accel_dev);

#endif
