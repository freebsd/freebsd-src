/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_CFG_COMMON_H_
#define ADF_CFG_COMMON_H_

#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/cpuset.h>

#define ADF_CFG_MAX_STR_LEN 128
#define ADF_CFG_MAX_KEY_LEN_IN_BYTES ADF_CFG_MAX_STR_LEN
/*
 * Max value length increased to 128 to support more length of values.
 * like Dc0CoreAffinity = 0, 1, 2,... config values to max cores
 */
#define ADF_CFG_MAX_VAL_LEN_IN_BYTES 128
#define ADF_CFG_MAX_SECTION_LEN_IN_BYTES ADF_CFG_MAX_STR_LEN
#define ADF_CFG_NULL_TERM_SIZE 1
#define ADF_CFG_BASE_DEC 10
#define ADF_CFG_BASE_HEX 16
#define ADF_CFG_ALL_DEVICES 0xFFFE
#define ADF_CFG_NO_DEVICE 0xFFFF
#define ADF_CFG_AFFINITY_WHATEVER 0xFF
#define MAX_DEVICE_NAME_SIZE 32
#define ADF_MAX_DEVICES (32 * 32)
#define ADF_MAX_ACCELENGINES 12
#define ADF_CFG_STORAGE_ENABLED 1
#define ADF_DEVS_ARRAY_SIZE BITS_TO_LONGS(ADF_MAX_DEVICES)
#define ADF_SSM_WDT_PKE_DEFAULT_VALUE 0x3000000
#define ADF_WDT_TIMER_SYM_COMP_MS 3
#define ADF_MIN_HB_TIMER_MS 100
#define ADF_CFG_MAX_NUM_OF_SECTIONS 16
#define ADF_CFG_MAX_NUM_OF_TOKENS 16
#define ADF_CFG_MAX_TOKENS_IN_CONFIG 8
#define ADF_CFG_RESP_POLL 1
#define ADF_CFG_RESP_EPOLL 2
#define ADF_CFG_DEF_CY_RING_ASYM_SIZE 64
#define ADF_CFG_DEF_CY_RING_SYM_SIZE 512
#define ADF_CFG_DEF_DC_RING_SIZE 512
#define ADF_CFG_MAX_CORE_NUM 256
#define ADF_CFG_MAX_TOKENS ADF_CFG_MAX_CORE_NUM
#define ADF_CFG_MAX_TOKEN_LEN 10
#define ADF_CFG_ACCEL_DEF_COALES 1
#define ADF_CFG_ACCEL_DEF_COALES_TIMER 10000
#define ADF_CFG_ACCEL_DEF_COALES_NUM_MSG 0
#define ADF_CFG_ASYM_SRV_MASK 1
#define ADF_CFG_SYM_SRV_MASK 2
#define ADF_CFG_DC_SRV_MASK 8
#define ADF_CFG_UNKNOWN_SRV_MASK 0
#define ADF_CFG_DEF_ASYM_MASK 0x03
#define ADF_CFG_MAX_SERVICES 4
#define ADF_MAX_SERVICES 3

enum adf_svc_type {
	ADF_SVC_ASYM = 0,
	ADF_SVC_SYM = 1,
	ADF_SVC_DC = 2,
	ADF_SVC_NONE = 3
};

struct adf_pci_address {
	unsigned char bus;
	unsigned char dev;
	unsigned char func;
} __packed;

#define ADF_CFG_SERV_RING_PAIR_0_SHIFT 0
#define ADF_CFG_SERV_RING_PAIR_1_SHIFT 3
#define ADF_CFG_SERV_RING_PAIR_2_SHIFT 6
#define ADF_CFG_SERV_RING_PAIR_3_SHIFT 9

enum adf_cfg_service_type { NA = 0, CRYPTO, COMP, SYM, ASYM, USED };

enum adf_cfg_bundle_type { FREE, KERNEL, USER };

enum adf_cfg_val_type { ADF_DEC, ADF_HEX, ADF_STR };

enum adf_device_type {
	DEV_UNKNOWN = 0,
	DEV_DH895XCC,
	DEV_DH895XCCVF,
	DEV_C62X,
	DEV_C62XVF,
	DEV_C3XXX,
	DEV_C3XXXVF,
	DEV_200XX,
	DEV_200XXVF,
	DEV_C4XXX,
	DEV_C4XXXVF
};

enum adf_cfg_fw_image_type {
	ADF_FW_IMAGE_DEFAULT = 0,
	ADF_FW_IMAGE_CRYPTO,
	ADF_FW_IMAGE_COMPRESSION,
	ADF_FW_IMAGE_CUSTOM1
};

struct adf_dev_status_info {
	enum adf_device_type type;
	uint16_t accel_id;
	uint16_t instance_id;
	uint8_t num_ae;
	uint8_t num_accel;
	uint8_t num_logical_accel;
	uint8_t banks_per_accel;
	uint8_t state;
	uint8_t bus;
	uint8_t dev;
	uint8_t fun;
	int domain;
	char name[MAX_DEVICE_NAME_SIZE];
	u8 sku;
	u32 node_id;
	u32 device_mem_available;
	u32 pci_device_id;
};

struct adf_cfg_device {
	/* contains all the bundles info */
	struct adf_cfg_bundle **bundles;
	/* contains all the instances info */
	struct adf_cfg_instance **instances;
	int bundle_num;
	int instance_index;
	char name[ADF_CFG_MAX_STR_LEN];
	int dev_id;
	int max_kernel_bundle_nr;
	u16 total_num_inst;
};

enum adf_accel_serv_type {
	ADF_ACCEL_SERV_NA = 0x0,
	ADF_ACCEL_SERV_ASYM,
	ADF_ACCEL_SERV_SYM,
	ADF_ACCEL_SERV_RND,
	ADF_ACCEL_SERV_DC
};

struct adf_cfg_ring {
	u8 mode : 1;
	enum adf_accel_serv_type serv_type;
	u8 number : 4;
};

struct adf_cfg_bundle {
	/* Section(s) name this bundle is shared by */
	char **sections;
	int max_section;
	int section_index;
	int number;
	enum adf_cfg_bundle_type type;
	cpuset_t affinity_mask;
	int polling_mode;
	int instance_num;
	int num_of_rings;
	/* contains all the info about rings */
	struct adf_cfg_ring **rings;
	u16 in_use;
};

struct adf_cfg_instance {
	enum adf_cfg_service_type stype;
	char name[ADF_CFG_MAX_STR_LEN];
	int polling_mode;
	cpuset_t affinity_mask;
	/* rings within an instance for services */
	int asym_tx;
	int asym_rx;
	int sym_tx;
	int sym_rx;
	int dc_tx;
	int dc_rx;
	int bundle;
};

#define ADF_CFG_MAX_CORE_NUM 256
#define ADF_CFG_MAX_TOKENS_IN_CONFIG 8
#define ADF_CFG_MAX_TOKEN_LEN 10
#define ADF_CFG_MAX_TOKENS ADF_CFG_MAX_CORE_NUM
#define ADF_CFG_ACCEL_DEF_COALES 1
#define ADF_CFG_ACCEL_DEF_COALES_TIMER 10000
#define ADF_CFG_ACCEL_DEF_COALES_NUM_MSG 0
#define ADF_CFG_RESP_EPOLL 2
#define ADF_CFG_SERV_RING_PAIR_1_SHIFT 3
#define ADF_CFG_SERV_RING_PAIR_2_SHIFT 6
#define ADF_CFG_SERV_RING_PAIR_3_SHIFT 9
#define ADF_CFG_RESP_POLL 1
#define ADF_CFG_ASYM_SRV_MASK 1
#define ADF_CFG_SYM_SRV_MASK 2
#define ADF_CFG_DC_SRV_MASK 8
#define ADF_CFG_UNKNOWN_SRV_MASK 0
#define ADF_CFG_DEF_ASYM_MASK 0x03
#define ADF_CFG_MAX_SERVICES 4

#define ADF_CFG_HB_DEFAULT_VALUE 500
#define ADF_CFG_HB_COUNT_THRESHOLD 3
#define ADF_MIN_HB_TIMER_MS 100

enum adf_device_heartbeat_status {
	DEV_HB_UNRESPONSIVE = 0,
	DEV_HB_ALIVE,
	DEV_HB_UNSUPPORTED
};

struct adf_dev_heartbeat_status_ctl {
	uint16_t device_id;
	enum adf_device_heartbeat_status status;
};
#endif
