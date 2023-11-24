/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_CFG_USER_H_
#define ADF_CFG_USER_H_

#include "adf_cfg_common.h"
#include "adf_cfg_strings.h"

struct adf_user_cfg_key_val {
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	union {
		struct adf_user_cfg_key_val *next;
		uint64_t padding3;
	};
	enum adf_cfg_val_type type;
};

struct adf_user_cfg_section {
	char name[ADF_CFG_MAX_SECTION_LEN_IN_BYTES];
	union {
		struct adf_user_cfg_key_val *params;
		uint64_t padding1;
	};
	union {
		struct adf_user_cfg_section *next;
		uint64_t padding3;
	};
};

struct adf_user_cfg_ctl_data {
	union {
		struct adf_user_cfg_section *config_section;
		uint64_t padding;
	};
	u32 device_id;
};

struct adf_user_reserve_ring {
	u32 accel_id;
	u32 bank_nr;
	u32 ring_mask;
};

#endif
