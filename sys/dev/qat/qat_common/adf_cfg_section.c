/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "adf_cfg_instance.h"
#include "adf_cfg_device.h"
#include "adf_cfg_section.h"

static bool
adf_cfg_is_svc_enabled(struct adf_accel_dev *accel_dev, const u8 svc)
{
	int ring_pair_index = 0;
	u8 serv_type = NA;
	struct adf_hw_device_data *hw_data = GET_HW_DATA(accel_dev);

	for (ring_pair_index = 0; ring_pair_index < ADF_CFG_NUM_SERVICES;
	     ring_pair_index++) {
		serv_type =
		    GET_SRV_TYPE(hw_data->ring_to_svc_map, ring_pair_index);
		if (serv_type == svc)
			return true;
	}
	return false;
}

static int
adf_cfg_set_core_number_for_instance(struct adf_accel_dev *accel_dev,
				     const char *sec_name,
				     const char *inst_name,
				     int process_num,
				     unsigned long *core_number)
{
	char *core_val = NULL;
	char *pos = NULL;
	char **tokens = NULL;
	int token_index = 0;
	int core_arr_index = 0;
	int i = 0;
	int ret = EFAULT;
	unsigned long *core_num_arr = NULL;
	unsigned long core_num;
	unsigned long start, end;

	/* do memory allocation */
	core_val =
	    malloc(ADF_CFG_MAX_VAL_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	tokens = malloc(sizeof(char *) * ADF_CFG_MAX_TOKENS,
			M_QAT,
			M_WAITOK | M_ZERO);

	for (i = 0; i < ADF_CFG_MAX_TOKENS; i++) {
		tokens[i] =
		    malloc(ADF_CFG_MAX_TOKEN_LEN, M_QAT, M_WAITOK | M_ZERO);
	}

	core_num_arr = malloc(sizeof(unsigned long) * ADF_CFG_MAX_CORE_NUM,
			      M_QAT,
			      M_WAITOK | M_ZERO);

	/* parse the core_val */
	ret = EFAULT;
	if (adf_cfg_get_param_value(accel_dev, sec_name, inst_name, core_val))
		goto failed;

	pos = strchr(core_val, ',');
	while (pos) {
		pos[0] = '\0';
		strlcpy(tokens[token_index++], core_val, ADF_CFG_MAX_TOKEN_LEN);
		strlcpy(core_val, pos + 1, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
		pos = strchr(core_val, ',');
		if (!pos)
			strlcpy(tokens[token_index++],
				core_val,
				ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	}

	/* in case there is only N-M */
	if (token_index == 0)
		strlcpy(tokens[token_index++],
			core_val,
			ADF_CFG_MAX_VAL_LEN_IN_BYTES);

	/* parse the tokens such as N-M */
	for (i = 0; i < token_index; i++) {
		pos = strchr(tokens[i], '-');
		if (pos) {
			pos[0] = '\0';
			ret = compat_strtoul(tokens[i], 10, &start);
			if (ret)
				goto failed;
			ret = compat_strtoul(pos + 1, 10, &end);
			if (ret)
				goto failed;
			if (start > end) {
				ret = EFAULT;
				goto failed;
			}
			for (core_num = start; core_num < end + 1; core_num++)
				core_num_arr[core_arr_index++] = core_num;
		} else {
			ret = compat_strtoul(tokens[i], 10, &core_num);
			if (ret)
				goto failed;
			core_num_arr[core_arr_index++] = core_num;
		}
	}

	if (core_arr_index == 0) {
		ret = compat_strtoul(core_val, 10, &core_num);
		if (ret)
			goto failed;
		else
			core_num_arr[core_arr_index++] = core_num;
	}

	*core_number = core_num_arr[process_num % core_arr_index];
	ret = 0;
failed:
	free(core_val, M_QAT);
	if (tokens) {
		for (i = 0; i < ADF_CFG_MAX_TOKENS; i++)
			free(tokens[i], M_QAT);
		free(tokens, M_QAT);
	}
	free(core_num_arr, M_QAT);

	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Get core number failed with error %d\n",
			      ret);
	return ret;
}

static int
adf_cfg_set_value(struct adf_accel_dev *accel_dev,
		  const char *sec,
		  const char *key,
		  unsigned long *value)
{
	char *val = NULL;
	int ret = EFAULT;

	val = malloc(ADF_CFG_MAX_VAL_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	if (adf_cfg_get_param_value(accel_dev, sec, key, val))
		goto out;

	/* as the key type can be either ADF_DEC or ADF_HEX */
	if (compat_strtoul(val, 10, value) && compat_strtoul(val, 16, value))
		goto out;

	ret = 0;
out:
	free(val, M_QAT);
	return ret;
}

static void
adf_cfg_add_cy_inst_info(struct adf_accel_dev *accel_dev,
			 struct adf_cfg_instance *crypto_inst,
			 const char *derived_sec,
			 int inst_index)
{
	char *key = NULL;
	unsigned long bank_number = 0;
	unsigned long ring_number = 0;
	unsigned long asym_req = 0;
	unsigned long sym_req = 0;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_BANK_NUM_FORMAT,
		 inst_index);
	bank_number = crypto_inst->bundle;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&bank_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_ASYM_TX_FORMAT,
		 inst_index);
	ring_number = crypto_inst->asym_tx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_SYM_TX_FORMAT,
		 inst_index);
	ring_number = crypto_inst->sym_tx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_ASYM_RX_FORMAT,
		 inst_index);
	ring_number = crypto_inst->asym_rx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_SYM_RX_FORMAT,
		 inst_index);
	ring_number = crypto_inst->sym_rx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	strlcpy(key, ADF_CY_RING_ASYM_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, ADF_GENERAL_SEC, key, &asym_req))
		asym_req = ADF_CFG_DEF_CY_RING_ASYM_SIZE;

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_RING_ASYM_SIZE_FORMAT,
		 inst_index);
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&asym_req, ADF_DEC);

	strlcpy(key, ADF_CY_RING_SYM_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, ADF_GENERAL_SEC, key, &sym_req))
		sym_req = ADF_CFG_DEF_CY_RING_SYM_SIZE;

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_RING_SYM_SIZE_FORMAT,
		 inst_index);
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&sym_req, ADF_DEC);

	free(key, M_QAT);
}

static void
adf_cfg_add_dc_inst_info(struct adf_accel_dev *accel_dev,
			 struct adf_cfg_instance *dc_inst,
			 const char *derived_sec,
			 int inst_index)
{
	char *key = NULL;
	unsigned long bank_number = 0;
	unsigned long ring_number = 0;
	unsigned long dc_req = 0;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	snprintf(key, ADF_CFG_MAX_STR_LEN, ADF_DC_BANK_NUM_FORMAT, inst_index);
	bank_number = dc_inst->bundle;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&bank_number, ADF_DEC);

	snprintf(key, ADF_CFG_MAX_STR_LEN, ADF_DC_TX_FORMAT, inst_index);
	ring_number = dc_inst->dc_tx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	snprintf(key, ADF_CFG_MAX_STR_LEN, ADF_DC_RX_FORMAT, inst_index);
	ring_number = dc_inst->dc_rx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	strlcpy(key, ADF_DC_RING_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, ADF_GENERAL_SEC, key, &dc_req))
		dc_req = ADF_CFG_DEF_DC_RING_SIZE;

	snprintf(key, ADF_CFG_MAX_STR_LEN, ADF_DC_RING_SIZE_FORMAT, inst_index);
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&dc_req, ADF_DEC);

	free(key, M_QAT);
}

static void
adf_cfg_add_asym_inst_info(struct adf_accel_dev *accel_dev,
			   struct adf_cfg_instance *asym_inst,
			   const char *derived_sec,
			   int inst_index)
{
	char *key = NULL;
	unsigned long bank_number = 0;
	unsigned long ring_number = 0;
	unsigned long asym_req = 0;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_BANK_NUM_FORMAT,
		 inst_index);
	bank_number = asym_inst->bundle;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&bank_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_ASYM_TX_FORMAT,
		 inst_index);
	ring_number = asym_inst->asym_tx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_ASYM_RX_FORMAT,
		 inst_index);
	ring_number = asym_inst->asym_rx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	strlcpy(key, ADF_CY_RING_ASYM_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, ADF_GENERAL_SEC, key, &asym_req))
		asym_req = ADF_CFG_DEF_CY_RING_ASYM_SIZE;

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_RING_ASYM_SIZE_FORMAT,
		 inst_index);
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&asym_req, ADF_DEC);

	free(key, M_QAT);
}

static void
adf_cfg_add_sym_inst_info(struct adf_accel_dev *accel_dev,
			  struct adf_cfg_instance *sym_inst,
			  const char *derived_sec,
			  int inst_index)
{
	char *key = NULL;
	unsigned long bank_number = 0;
	unsigned long ring_number = 0;
	unsigned long sym_req = 0;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_BANK_NUM_FORMAT,
		 inst_index);
	bank_number = sym_inst->bundle;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&bank_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_SYM_TX_FORMAT,
		 inst_index);
	ring_number = sym_inst->sym_tx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_SYM_RX_FORMAT,
		 inst_index);
	ring_number = sym_inst->sym_rx;
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&ring_number, ADF_DEC);

	strlcpy(key, ADF_CY_RING_SYM_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, ADF_GENERAL_SEC, key, &sym_req))
		sym_req = ADF_CFG_DEF_CY_RING_SYM_SIZE;

	snprintf(key,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
		 ADF_CY_RING_SYM_SIZE_FORMAT,
		 inst_index);
	adf_cfg_add_key_value_param(
	    accel_dev, derived_sec, key, (void *)&sym_req, ADF_DEC);

	free(key, M_QAT);
}

static int
adf_cfg_section_copy(struct adf_accel_dev *accel_dev,
		     const char *processed_sec,
		     const char *derived_sec)
{
	unsigned long val = 0;
	struct list_head *list;
	struct adf_cfg_section *sec_process =
	    adf_cfg_sec_find(accel_dev, processed_sec);
	if (!sec_process)
		return EFAULT;

	list_for_each(list, &sec_process->param_head)
	{
		struct adf_cfg_key_val *ptr =
		    list_entry(list, struct adf_cfg_key_val, list);

		/*
		 * ignore CoreAffinity since it will be generated later, and
		 * there is no need to keep NumProcesses and LimitDevAccess.
		 */
		if (strstr(ptr->key, ADF_ETRMGR_CORE_AFFINITY) ||
		    strstr(ptr->key, ADF_NUM_PROCESSES) ||
		    strstr(ptr->key, ADF_LIMIT_DEV_ACCESS))
			continue;

		if (ptr->type == ADF_DEC) {
			if (!compat_strtoul(ptr->val, 10, &val))
				adf_cfg_add_key_value_param(accel_dev,
							    derived_sec,
							    ptr->key,
							    (void *)&val,
							    ptr->type);
		} else if (ptr->type == ADF_STR) {
			adf_cfg_add_key_value_param(accel_dev,
						    derived_sec,
						    ptr->key,
						    (void *)ptr->val,
						    ptr->type);
		} else if (ptr->type == ADF_HEX) {
			if (!compat_strtoul(ptr->val, 16, &val))
				adf_cfg_add_key_value_param(accel_dev,
							    derived_sec,
							    ptr->key,
							    (void *)val,
							    ptr->type);
		}
	}
	return 0;
}

static int
adf_cfg_create_rings_entries_for_cy_inst(struct adf_accel_dev *accel_dev,
					 const char *processed_sec,
					 const char *derived_sec,
					 int process_num,
					 enum adf_cfg_service_type serv_type)
{
	int i = 0;
	int ret = EFAULT;
	unsigned long num_inst = 0, num_dc_inst = 0;
	unsigned long core_number = 0;
	unsigned long polling_mode = 0;
	struct adf_cfg_instance *crypto_inst = NULL;

	char *key = NULL;
	char *val = NULL;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	val = malloc(ADF_CFG_MAX_VAL_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		goto failed;
	if ((!strncmp(val, ADF_CFG_CY, ADF_CFG_MAX_VAL_LEN_IN_BYTES)) ||
	    (!strncmp(val, ADF_CFG_ASYM, ADF_CFG_MAX_VAL_LEN_IN_BYTES)) ||
	    (!strncmp(val, ADF_CFG_SYM, ADF_CFG_MAX_VAL_LEN_IN_BYTES))) {
		strlcpy(key, ADF_NUM_DC, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
		if (adf_cfg_set_value(
			accel_dev, processed_sec, key, &num_dc_inst))
			goto failed;
		if (num_dc_inst > 0) {
			device_printf(
			    GET_DEV(accel_dev),
			    "NumDcInstances > 0,when CY only is enabled\n");
			goto failed;
		}
	}
	ret = EFAULT;

	strlcpy(key, ADF_NUM_CY, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, processed_sec, key, &num_inst))
		goto failed;

	crypto_inst = malloc(sizeof(*crypto_inst), M_QAT, M_WAITOK | M_ZERO);

	for (i = 0; i < num_inst; i++) {
		memset(crypto_inst, 0, sizeof(*crypto_inst));
		crypto_inst->stype = serv_type;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY_CORE_AFFINITY_FORMAT,
			 i);
		if (adf_cfg_set_core_number_for_instance(accel_dev,
							 processed_sec,
							 key,
							 process_num,
							 &core_number))
			goto failed;

		if (strcmp(processed_sec, ADF_KERNEL_SEC) &&
		    strcmp(processed_sec, ADF_KERNEL_SAL_SEC))
			adf_cfg_add_key_value_param(accel_dev,
						    derived_sec,
						    key,
						    (void *)&core_number,
						    ADF_DEC);

		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY_NAME_FORMAT,
			 i);
		if (adf_cfg_get_param_value(accel_dev, processed_sec, key, val))
			goto failed;

		strlcpy(crypto_inst->name, val, sizeof(crypto_inst->name));

		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY_POLL_MODE_FORMAT,
			 i);
		if (adf_cfg_set_value(
			accel_dev, processed_sec, key, &polling_mode))
			goto failed;

		crypto_inst->polling_mode = polling_mode;
		CPU_ZERO(&crypto_inst->affinity_mask);
		CPU_SET(core_number, &crypto_inst->affinity_mask);

		if (adf_cfg_get_ring_pairs(accel_dev->cfg->dev,
					   crypto_inst,
					   derived_sec,
					   accel_dev))
			goto failed;

		switch (serv_type) {
		case CRYPTO:
			adf_cfg_add_cy_inst_info(accel_dev,
						 crypto_inst,
						 derived_sec,
						 i);
			break;
		case ASYM:
			adf_cfg_add_asym_inst_info(accel_dev,
						   crypto_inst,
						   derived_sec,
						   i);
			break;
		case SYM:
			adf_cfg_add_sym_inst_info(accel_dev,
						  crypto_inst,
						  derived_sec,
						  i);
			break;
		default:
			pr_err("unknown crypto instance type %d.\n", serv_type);
			goto failed;
		}
	}

	ret = 0;
failed:
	free(crypto_inst, M_QAT);
	free(val, M_QAT);
	free(key, M_QAT);

	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Failed to create rings for cy\n");

	return ret;
}

static int
adf_cfg_create_rings_entries_for_dc_inst(struct adf_accel_dev *accel_dev,
					 const char *processed_sec,
					 const char *derived_sec,
					 int process_num)
{
	int i = 0;
	int ret = EFAULT;
	unsigned long num_inst = 0, num_cy_inst = 0;
	unsigned long core_number = 0;
	unsigned long polling_mode = 0;
	struct adf_cfg_instance *dc_inst = NULL;

	char *key = NULL;
	char *val = NULL;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	val = malloc(ADF_CFG_MAX_VAL_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	ret = EFAULT;

	snprintf(key, ADF_CFG_MAX_STR_LEN, ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		goto failed;

	if (!strncmp(val, ADF_CFG_DC, ADF_CFG_MAX_VAL_LEN_IN_BYTES)) {
		strlcpy(key, ADF_NUM_CY, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
		if (adf_cfg_set_value(
			accel_dev, processed_sec, key, &num_cy_inst))
			goto failed;
		if (num_cy_inst > 0) {
			device_printf(
			    GET_DEV(accel_dev),
			    "NumCyInstances > 0,when DC only is enabled\n");
			goto failed;
		}
	}

	strlcpy(key, ADF_NUM_DC, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, processed_sec, key, &num_inst))
		goto failed;

	dc_inst = malloc(sizeof(*dc_inst), M_QAT, M_WAITOK | M_ZERO);

	for (i = 0; i < num_inst; i++) {
		memset(dc_inst, 0, sizeof(*dc_inst));
		dc_inst->stype = COMP;
		snprintf(key,
			 ADF_CFG_MAX_STR_LEN,
			 ADF_DC_CORE_AFFINITY_FORMAT,
			 i);

		if (adf_cfg_set_core_number_for_instance(accel_dev,
							 processed_sec,
							 key,
							 process_num,
							 &core_number))
			goto failed;

		if (strcmp(processed_sec, ADF_KERNEL_SEC) &&
		    strcmp(processed_sec, ADF_KERNEL_SAL_SEC)) {
			adf_cfg_add_key_value_param(accel_dev,
						    derived_sec,
						    key,
						    (void *)&core_number,
						    ADF_DEC);
		}

		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC_NAME_FORMAT,
			 i);
		if (adf_cfg_get_param_value(accel_dev, processed_sec, key, val))
			goto failed;

		strlcpy(dc_inst->name, val, sizeof(dc_inst->name));

		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC_POLL_MODE_FORMAT,
			 i);
		if (adf_cfg_set_value(
			accel_dev, processed_sec, key, &polling_mode))
			goto failed;

		dc_inst->polling_mode = polling_mode;
		CPU_ZERO(&dc_inst->affinity_mask);
		CPU_SET(core_number, &dc_inst->affinity_mask);

		if (adf_cfg_get_ring_pairs(
			accel_dev->cfg->dev, dc_inst, derived_sec, accel_dev))
			goto failed;

		adf_cfg_add_dc_inst_info(accel_dev, dc_inst, derived_sec, i);
	}

	ret = 0;
failed:
	free(dc_inst, M_QAT);
	free(val, M_QAT);
	free(key, M_QAT);

	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Failed to create rings for dc\n");

	return ret;
}

static int
adf_cfg_process_user_section(struct adf_accel_dev *accel_dev,
			     const char *sec_name,
			     int dev)
{
	int i = 0;
	int ret = EFAULT;
	unsigned long num_processes = 0;
	unsigned long limit_dev_acc = 0;
	u8 serv_type = 0;

	char *key = NULL;
	char *val = NULL;
	char *derived_sec_name = NULL;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	val = malloc(ADF_CFG_MAX_VAL_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	derived_sec_name =
	    malloc(ADF_CFG_MAX_STR_LEN, M_QAT, M_WAITOK | M_ZERO);

	strlcpy(key, ADF_NUM_PROCESSES, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, sec_name, key, &num_processes))
		num_processes = 0;

	strlcpy(key, ADF_LIMIT_DEV_ACCESS, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, sec_name, key, &limit_dev_acc))
		limit_dev_acc = 0;

	for (i = 0; i < num_processes; i++) {
		if (limit_dev_acc)
			snprintf(derived_sec_name,
				 ADF_CFG_MAX_STR_LEN,
				 ADF_LIMITED_USER_SECTION_NAME_FORMAT,
				 sec_name,
				 dev,
				 i);
		else
			snprintf(derived_sec_name,
				 ADF_CFG_MAX_STR_LEN,
				 ADF_USER_SECTION_NAME_FORMAT,
				 sec_name,
				 i);

		if (adf_cfg_derived_section_add(accel_dev, derived_sec_name))
			goto failed;

		/* copy items to the derived section */
		adf_cfg_section_copy(accel_dev, sec_name, derived_sec_name);

		for (serv_type = NA; serv_type <= USED; serv_type++) {
			switch (serv_type) {
			case NA:
				break;
			case CRYPTO:
			case ASYM:
			case SYM:
				if (adf_cfg_is_svc_enabled(accel_dev,
							   serv_type))
					if (adf_cfg_create_rings_entries_for_cy_inst(
						accel_dev,
						sec_name,
						derived_sec_name,
						i,
						(enum adf_cfg_service_type)
						    serv_type))
						goto failed;
				break;
			case COMP:
				if (adf_cfg_is_svc_enabled(accel_dev,
							   serv_type))
					if (adf_cfg_create_rings_entries_for_dc_inst(
						accel_dev,
						sec_name,
						derived_sec_name,
						i))
						goto failed;
				break;
			case USED:
				break;
			default:
				pr_err("Unknown service type %d.\n", serv_type);
			}
		}
	}

	ret = 0;
failed:

	free(val, M_QAT);
	free(key, M_QAT);
	free(derived_sec_name, M_QAT);

	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Failed to process user section %s\n",
			      sec_name);

	return ret;
}

static int
adf_cfg_cleanup_user_section(struct adf_accel_dev *accel_dev,
			     const char *sec_name)
{
	struct adf_cfg_section *sec = adf_cfg_sec_find(accel_dev, sec_name);
	struct list_head *head;
	struct list_head *list_ptr, *tmp;

	if (!sec)
		return EFAULT;

	if (sec->is_derived)
		return 0;

	head = &sec->param_head;
	list_for_each_prev_safe(list_ptr, tmp, head)
	{
		struct adf_cfg_key_val *ptr =
		    list_entry(list_ptr, struct adf_cfg_key_val, list);

		if (!strcmp(ptr->key, ADF_LIMIT_DEV_ACCESS))
			continue;

		list_del(list_ptr);
		free(ptr, M_QAT);
	}
	return 0;
}

static int
adf_cfg_process_section_no_op(struct adf_accel_dev *accel_dev,
			      const char *sec_name)
{
	return 0;
}

static int
adf_cfg_cleanup_general_section(struct adf_accel_dev *accel_dev,
				const char *sec_name)
{
	unsigned long first_used_bundle = 0;
	int ret = EFAULT;
	char *key = NULL;
	char *val = NULL;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	val = malloc(ADF_CFG_MAX_VAL_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	/* Remove sections that not needed after processing */
	strlcpy(key, ADF_CONFIG_VERSION, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_remove_key_param(accel_dev, sec_name, key))
		goto failed;

	strlcpy(key, ADF_CY ADF_RING_ASYM_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_remove_key_param(accel_dev, sec_name, key))
		goto failed;

	strlcpy(key, ADF_CY ADF_RING_SYM_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_remove_key_param(accel_dev, sec_name, key))
		goto failed;

	strlcpy(key, ADF_DC ADF_RING_DC_SIZE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_remove_key_param(accel_dev, sec_name, key))
		goto failed;

	/* After all processing done, set the "FirstUserBundle" value */
	first_used_bundle = accel_dev->cfg->dev->max_kernel_bundle_nr + 1;
	strlcpy(key, ADF_FIRST_USER_BUNDLE, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_add_key_value_param(
		accel_dev, sec_name, key, (void *)&first_used_bundle, ADF_DEC))
		goto failed;

	ret = 0;
failed:
	free(key, M_QAT);
	free(val, M_QAT);

	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Failed to clean up general section\n");

	return ret;
}

static int
adf_cfg_process_kernel_section(struct adf_accel_dev *accel_dev,
			       const char *sec_name)
{
	u8 serv_type = 0;

	for (serv_type = NA; serv_type <= USED; serv_type++) {
		switch (serv_type) {
		case NA:
			break;
		case CRYPTO:
		case ASYM:
		case SYM:
			if (adf_cfg_is_svc_enabled(accel_dev, serv_type))
				if (adf_cfg_create_rings_entries_for_cy_inst(
					accel_dev,
					sec_name,
					sec_name,
					0,
					(enum adf_cfg_service_type)serv_type))
					goto failed;
			break;
		case COMP:
			if (adf_cfg_is_svc_enabled(accel_dev, serv_type))
				if (adf_cfg_create_rings_entries_for_dc_inst(
					accel_dev, sec_name, sec_name, 0))
					goto failed;
			break;
		case USED:
			break;
		default:
			pr_err("Unknown service type of instance %d.\n",
			       serv_type);
		}
	}

	return 0;

failed:
	return EFAULT;
}

static int
adf_cfg_cleanup_kernel_section(struct adf_accel_dev *accel_dev,
			       const char *sec_name)
{
	return 0;
}

static int
adf_cfg_create_accel_section(struct adf_accel_dev *accel_dev,
			     const char *sec_name)
{
	/* Find global settings for coalescing. Use defaults if not found */
	unsigned long accel_coales = 0;
	unsigned long accel_coales_timer = 0;
	unsigned long accel_coales_num_msg = 0;
	unsigned long cpu;
	char *key = NULL;
	char *val = NULL;
	int ret = EFAULT;
	int index = 0;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;

	if (!hw_device)
		goto failed;

	key = malloc(ADF_CFG_MAX_KEY_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	val = malloc(ADF_CFG_MAX_VAL_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	strlcpy(key,
		ADF_ETRMGR_COALESCING_ENABLED,
		ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(accel_dev, ADF_GENERAL_SEC, key, &accel_coales))
		accel_coales = ADF_CFG_ACCEL_DEF_COALES;

	strlcpy(key, ADF_ETRMGR_COALESCE_TIMER, ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(
		accel_dev, ADF_GENERAL_SEC, key, &accel_coales_timer))
		accel_coales_timer = ADF_CFG_ACCEL_DEF_COALES_TIMER;

	strlcpy(key,
		ADF_ETRMGR_COALESCING_MSG_ENABLED,
		ADF_CFG_MAX_KEY_LEN_IN_BYTES);
	if (adf_cfg_set_value(
		accel_dev, ADF_GENERAL_SEC, key, &accel_coales_num_msg))
		accel_coales_num_msg = ADF_CFG_ACCEL_DEF_COALES_NUM_MSG;

	for (index = 0; index < hw_device->num_banks; index++) {
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_ETRMGR_COALESCING_ENABLED_FORMAT,
			 index);
		ret = adf_cfg_add_key_value_param(
		    accel_dev, sec_name, key, &accel_coales, ADF_DEC);
		if (ret != 0)
			goto failed;

		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_ETRMGR_COALESCE_TIMER_FORMAT,
			 index);
		ret = adf_cfg_add_key_value_param(
		    accel_dev, sec_name, key, &accel_coales_timer, ADF_DEC);
		if (ret != 0)
			goto failed;

		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_ETRMGR_COALESCING_MSG_ENABLED_FORMAT,
			 index);
		ret = adf_cfg_add_key_value_param(
		    accel_dev, sec_name, key, &accel_coales_num_msg, ADF_DEC);
		if (ret != 0)
			goto failed;

		cpu = ADF_CFG_AFFINITY_WHATEVER;

		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_ETRMGR_CORE_AFFINITY_FORMAT,
			 index);
		ret = adf_cfg_add_key_value_param(
		    accel_dev, sec_name, key, &cpu, ADF_DEC);
		if (ret != 0)
			goto failed;
	}

	ret = 0;

failed:
	free(key, M_QAT);
	free(val, M_QAT);

	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Failed to create accel section\n");

	return ret;
}

static int
adf_cfg_cleanup_accel_section(struct adf_accel_dev *accel_dev,
			      const char *sec_name)
{
	return 0;
}

static int
adf_cfg_process_accel_section(struct adf_accel_dev *accel_dev,
			      const char *sec_name)
{
	int accel_num = 0;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	char *derived_name = NULL;
	int ret = EFAULT;

	if (!hw_device)
		goto failed;

	if (hw_device->num_logical_accel == 0)
		goto failed;

	derived_name =
	    malloc(ADF_CFG_MAX_SECTION_LEN_IN_BYTES, M_QAT, M_WAITOK | M_ZERO);

	for (accel_num = 0; accel_num < hw_device->num_logical_accel;
	     accel_num++) {
		snprintf(derived_name,
			 ADF_CFG_MAX_SECTION_LEN_IN_BYTES,
			 ADF_ACCEL_STR,
			 accel_num);
		ret = adf_cfg_section_add(accel_dev, derived_name);
		if (ret != 0)
			goto failed;

		ret = adf_cfg_create_accel_section(accel_dev, derived_name);
		if (ret != 0)
			goto failed;
	}

	ret = 0;
failed:
	free(derived_name, M_QAT);

	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Failed to process accel section\n");

	return ret;
}

int
adf_cfg_process_section(struct adf_accel_dev *accel_dev,
			const char *sec_name,
			int dev)
{
	if (!strcmp(sec_name, ADF_GENERAL_SEC) ||
	    !strcmp(sec_name, ADF_INLINE_SEC))
		return adf_cfg_process_section_no_op(accel_dev, sec_name);
	else if (!strcmp(sec_name, ADF_KERNEL_SEC) ||
		 !strcmp(sec_name, ADF_KERNEL_SAL_SEC))
		return adf_cfg_process_kernel_section(accel_dev, sec_name);
	else if (!strcmp(sec_name, ADF_ACCEL_SEC))
		return adf_cfg_process_accel_section(accel_dev, sec_name);
	else
		return adf_cfg_process_user_section(accel_dev, sec_name, dev);
}

int
adf_cfg_cleanup_section(struct adf_accel_dev *accel_dev,
			const char *sec_name,
			int dev)
{
	if (!strcmp(sec_name, ADF_GENERAL_SEC))
		return adf_cfg_cleanup_general_section(accel_dev, sec_name);
	else if (!strcmp(sec_name, ADF_INLINE_SEC))
		return adf_cfg_process_section_no_op(accel_dev, sec_name);
	else if (!strcmp(sec_name, ADF_KERNEL_SEC) ||
		 !strcmp(sec_name, ADF_KERNEL_SAL_SEC))
		return adf_cfg_cleanup_kernel_section(accel_dev, sec_name);
	else if (strstr(sec_name, ADF_ACCEL_SEC))
		return adf_cfg_cleanup_accel_section(accel_dev, sec_name);
	else
		return adf_cfg_cleanup_user_section(accel_dev, sec_name);
}

int
adf_cfg_setup_irq(struct adf_accel_dev *accel_dev)
{
	int ret = EFAULT;
	struct adf_accel_pci *info_pci_dev = &accel_dev->accel_pci_dev;
	struct adf_cfg_device *cfg_dev = NULL;
	struct msix_entry *msixe = NULL;
	u32 num_msix = 0;
	int index = 0;
	int computed_core = 0;

	if (!accel_dev || !accel_dev->cfg || !accel_dev->hw_device)
		goto failed;

	cfg_dev = accel_dev->cfg->dev;
	if (!cfg_dev)
		goto failed;

	msixe =
	    (struct msix_entry *)accel_dev->accel_pci_dev.msix_entries.entries;
	num_msix = accel_dev->accel_pci_dev.msix_entries.num_entries;
	if (!msixe)
		goto cleanup_and_fail;

	/*
	 * Here we want to set the affinity of kernel and epoll mode
	 * bundle into user defined value.
	 * Because in adf_isr.c we setup core affinity by round-robin
	 * we need to reset it after device up done.
	 */
	for (index = 0; index < accel_dev->hw_device->num_banks; index++) {
		struct adf_cfg_bundle *bundle = cfg_dev->bundles[index];

		if (!bundle)
			continue;

		if (bundle->type != KERNEL &&
		    bundle->polling_mode != ADF_CFG_RESP_EPOLL)
			continue;

		if (bundle->number >= num_msix)
			goto cleanup_and_fail;

		computed_core = CPU_FFS(&bundle->affinity_mask) - 1;
		bus_bind_intr(info_pci_dev->pci_dev,
			      msixe[index].irq,
			      computed_core);
	}
	ret = 0;

cleanup_and_fail:
	adf_cfg_device_clear(cfg_dev, accel_dev);
	free(cfg_dev, M_QAT);
	accel_dev->cfg->dev = NULL;

failed:
	return ret;
}
