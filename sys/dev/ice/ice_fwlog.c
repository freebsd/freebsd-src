/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "ice_common.h"
#include "ice_fwlog.h"

/**
 * cache_cfg - Cache FW logging config
 * @hw: pointer to the HW structure
 * @cfg: config to cache
 */
static void cache_cfg(struct ice_hw *hw, struct ice_fwlog_cfg *cfg)
{
	hw->fwlog_cfg = *cfg;
}

/**
 * valid_module_entries - validate all the module entry IDs and log levels
 * @hw: pointer to the HW structure
 * @entries: entries to validate
 * @num_entries: number of entries to validate
 */
static bool
valid_module_entries(struct ice_hw *hw, struct ice_fwlog_module_entry *entries,
		     u16 num_entries)
{
	u16 i;

	if (!entries) {
		ice_debug(hw, ICE_DBG_FW_LOG, "Null ice_fwlog_module_entry array\n");
		return false;
	}

	if (!num_entries) {
		ice_debug(hw, ICE_DBG_FW_LOG, "num_entries must be non-zero\n");
		return false;
	}

	for (i = 0; i < num_entries; i++) {
		struct ice_fwlog_module_entry *entry = &entries[i];

		if (entry->module_id >= ICE_AQC_FW_LOG_ID_MAX) {
			ice_debug(hw, ICE_DBG_FW_LOG, "Invalid module_id %u, max valid module_id is %u\n",
				  entry->module_id, ICE_AQC_FW_LOG_ID_MAX - 1);
			return false;
		}

		if (entry->log_level >= ICE_FWLOG_LEVEL_INVALID) {
			ice_debug(hw, ICE_DBG_FW_LOG, "Invalid log_level %u, max valid log_level is %u\n",
				  entry->log_level,
				  ICE_AQC_FW_LOG_ID_MAX - 1);
			return false;
		}
	}

	return true;
}

/**
 * valid_cfg - validate entire configuration
 * @hw: pointer to the HW structure
 * @cfg: config to validate
 */
static bool valid_cfg(struct ice_hw *hw, struct ice_fwlog_cfg *cfg)
{
	if (!cfg) {
		ice_debug(hw, ICE_DBG_FW_LOG, "Null ice_fwlog_cfg\n");
		return false;
	}

	if (cfg->log_resolution < ICE_AQC_FW_LOG_MIN_RESOLUTION ||
	    cfg->log_resolution > ICE_AQC_FW_LOG_MAX_RESOLUTION) {
		ice_debug(hw, ICE_DBG_FW_LOG, "Unsupported log_resolution %u, must be between %u and %u\n",
			  cfg->log_resolution, ICE_AQC_FW_LOG_MIN_RESOLUTION,
			  ICE_AQC_FW_LOG_MAX_RESOLUTION);
		return false;
	}

	if (!valid_module_entries(hw, cfg->module_entries,
				  ICE_AQC_FW_LOG_ID_MAX))
		return false;

	return true;
}

/**
 * ice_fwlog_init - Initialize cached structures for tracking FW logging
 * @hw: pointer to the HW structure
 * @cfg: config used to initialize the cached structures
 *
 * This function should be called on driver initialization and before calling
 * ice_init_hw(). Firmware logging will be configured based on these settings
 * and also the PF will be registered on init.
 */
enum ice_status
ice_fwlog_init(struct ice_hw *hw, struct ice_fwlog_cfg *cfg)
{
	if (!valid_cfg(hw, cfg))
		return ICE_ERR_PARAM;

	cache_cfg(hw, cfg);

	return ICE_SUCCESS;
}

/**
 * ice_aq_fwlog_set - Set FW logging configuration AQ command (0xFF30)
 * @hw: pointer to the HW structure
 * @entries: entries to configure
 * @num_entries: number of @entries
 * @options: options from ice_fwlog_cfg->options structure
 * @log_resolution: logging resolution
 */
static enum ice_status
ice_aq_fwlog_set(struct ice_hw *hw, struct ice_fwlog_module_entry *entries,
		 u16 num_entries, u16 options, u16 log_resolution)
{
	struct ice_aqc_fw_log_cfg_resp *fw_modules;
	struct ice_aqc_fw_log *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;
	u16 i;

	fw_modules = (struct ice_aqc_fw_log_cfg_resp *)
		ice_calloc(hw, num_entries, sizeof(*fw_modules));
	if (!fw_modules)
		return ICE_ERR_NO_MEMORY;

	for (i = 0; i < num_entries; i++) {
		fw_modules[i].module_identifier =
			CPU_TO_LE16(entries[i].module_id);
		fw_modules[i].log_level = entries[i].log_level;
	}

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_fw_logs_config);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	cmd = &desc.params.fw_log;

	cmd->cmd_flags = ICE_AQC_FW_LOG_CONF_SET_VALID;
	cmd->ops.cfg.log_resolution = CPU_TO_LE16(log_resolution);
	cmd->ops.cfg.mdl_cnt = CPU_TO_LE16(num_entries);

	if (options & ICE_FWLOG_OPTION_ARQ_ENA)
		cmd->cmd_flags |= ICE_AQC_FW_LOG_CONF_AQ_EN;
	if (options & ICE_FWLOG_OPTION_UART_ENA)
		cmd->cmd_flags |= ICE_AQC_FW_LOG_CONF_UART_EN;

	status = ice_aq_send_cmd(hw, &desc, fw_modules,
				 sizeof(*fw_modules) * num_entries,
				 NULL);

	ice_free(hw, fw_modules);

	return status;
}

/**
 * ice_fwlog_supported - Cached for whether FW supports FW logging or not
 * @hw: pointer to the HW structure
 *
 * This will always return false if called before ice_init_hw(), so it must be
 * called after ice_init_hw().
 */
bool ice_fwlog_supported(struct ice_hw *hw)
{
	return hw->fwlog_support_ena;
}

/**
 * ice_fwlog_set - Set the firmware logging settings
 * @hw: pointer to the HW structure
 * @cfg: config used to set firmware logging
 *
 * This function should be called whenever the driver needs to set the firmware
 * logging configuration. It can be called on initialization, reset, or during
 * runtime.
 *
 * If the PF wishes to receive FW logging then it must register via
 * ice_fwlog_register. Note, that ice_fwlog_register does not need to be called
 * for init.
 */
enum ice_status
ice_fwlog_set(struct ice_hw *hw, struct ice_fwlog_cfg *cfg)
{
	enum ice_status status;

	if (!ice_fwlog_supported(hw))
		return ICE_ERR_NOT_SUPPORTED;

	if (!valid_cfg(hw, cfg))
		return ICE_ERR_PARAM;

	status = ice_aq_fwlog_set(hw, cfg->module_entries,
				  ICE_AQC_FW_LOG_ID_MAX, cfg->options,
				  cfg->log_resolution);
	if (!status)
		cache_cfg(hw, cfg);

	return status;
}

/**
 * update_cached_entries - Update module entries in cached FW logging config
 * @hw: pointer to the HW structure
 * @entries: entries to cache
 * @num_entries: number of @entries
 */
static void
update_cached_entries(struct ice_hw *hw, struct ice_fwlog_module_entry *entries,
		      u16 num_entries)
{
	u16 i;

	for (i = 0; i < num_entries; i++) {
		struct ice_fwlog_module_entry *updated = &entries[i];
		u16 j;

		for (j = 0; j < ICE_AQC_FW_LOG_ID_MAX; j++) {
			struct ice_fwlog_module_entry *cached =
				&hw->fwlog_cfg.module_entries[j];

			if (cached->module_id == updated->module_id) {
				cached->log_level = updated->log_level;
				break;
			}
		}
	}
}

/**
 * ice_fwlog_update_modules - Update the log level 1 or more FW logging modules
 * @hw: pointer to the HW structure
 * @entries: array of ice_fwlog_module_entry(s)
 * @num_entries: number of entries
 *
 * This function should be called to update the log level of 1 or more FW
 * logging modules via module ID.
 *
 * Only the entries passed in will be affected. All other firmware logging
 * settings will be unaffected.
 */
enum ice_status
ice_fwlog_update_modules(struct ice_hw *hw,
			 struct ice_fwlog_module_entry *entries,
			 u16 num_entries)
{
	struct ice_fwlog_cfg *cfg;
	enum ice_status status;

	if (!ice_fwlog_supported(hw))
		return ICE_ERR_NOT_SUPPORTED;

	if (!valid_module_entries(hw, entries, num_entries))
		return ICE_ERR_PARAM;

	cfg = (struct ice_fwlog_cfg *)ice_calloc(hw, 1, sizeof(*cfg));
	if (!cfg)
		return ICE_ERR_NO_MEMORY;

	status = ice_fwlog_get(hw, cfg);
	if (status)
		goto status_out;

	status = ice_aq_fwlog_set(hw, entries, num_entries, cfg->options,
				  cfg->log_resolution);
	if (!status)
		update_cached_entries(hw, entries, num_entries);

status_out:
	ice_free(hw, cfg);
	return status;
}

/**
 * ice_aq_fwlog_register - Register PF for firmware logging events (0xFF31)
 * @hw: pointer to the HW structure
 * @reg: true to register and false to unregister
 */
static enum ice_status ice_aq_fwlog_register(struct ice_hw *hw, bool reg)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_fw_logs_register);

	if (reg)
		desc.params.fw_log.cmd_flags = ICE_AQC_FW_LOG_AQ_REGISTER;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_fwlog_register - Register the PF for firmware logging
 * @hw: pointer to the HW structure
 *
 * After this call the PF will start to receive firmware logging based on the
 * configuration set in ice_fwlog_set.
 */
enum ice_status ice_fwlog_register(struct ice_hw *hw)
{
	enum ice_status status;

	if (!ice_fwlog_supported(hw))
		return ICE_ERR_NOT_SUPPORTED;

	status = ice_aq_fwlog_register(hw, true);
	if (status)
		ice_debug(hw, ICE_DBG_FW_LOG, "Failed to register for firmware logging events over ARQ\n");
	else
		hw->fwlog_cfg.options |= ICE_FWLOG_OPTION_IS_REGISTERED;

	return status;
}

/**
 * ice_fwlog_unregister - Unregister the PF from firmware logging
 * @hw: pointer to the HW structure
 */
enum ice_status ice_fwlog_unregister(struct ice_hw *hw)
{
	enum ice_status status;

	if (!ice_fwlog_supported(hw))
		return ICE_ERR_NOT_SUPPORTED;

	status = ice_aq_fwlog_register(hw, false);
	if (status)
		ice_debug(hw, ICE_DBG_FW_LOG, "Failed to unregister from firmware logging events over ARQ\n");
	else
		hw->fwlog_cfg.options &= ~ICE_FWLOG_OPTION_IS_REGISTERED;

	return status;
}

/**
 * ice_aq_fwlog_get - Get the current firmware logging configuration (0xFF32)
 * @hw: pointer to the HW structure
 * @cfg: firmware logging configuration to populate
 */
static enum ice_status
ice_aq_fwlog_get(struct ice_hw *hw, struct ice_fwlog_cfg *cfg)
{
	struct ice_aqc_fw_log_cfg_resp *fw_modules;
	struct ice_aqc_fw_log *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;
	u16 i, module_id_cnt;
	void *buf;

	ice_memset(cfg, 0, sizeof(*cfg), ICE_NONDMA_MEM);

	buf = ice_calloc(hw, 1, ICE_AQ_MAX_BUF_LEN);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_fw_logs_query);
	cmd = &desc.params.fw_log;

	cmd->cmd_flags = ICE_AQC_FW_LOG_AQ_QUERY;

	status = ice_aq_send_cmd(hw, &desc, buf, ICE_AQ_MAX_BUF_LEN, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_FW_LOG, "Failed to get FW log configuration\n");
		goto status_out;
	}

	module_id_cnt = LE16_TO_CPU(cmd->ops.cfg.mdl_cnt);
	if (module_id_cnt < ICE_AQC_FW_LOG_ID_MAX) {
		ice_debug(hw, ICE_DBG_FW_LOG, "FW returned less than the expected number of FW log module IDs\n");
	} else {
		if (module_id_cnt > ICE_AQC_FW_LOG_ID_MAX)
			ice_debug(hw, ICE_DBG_FW_LOG, "FW returned more than expected number of FW log module IDs, setting module_id_cnt to software expected max %u\n",
				  ICE_AQC_FW_LOG_ID_MAX);
		module_id_cnt = ICE_AQC_FW_LOG_ID_MAX;
	}

	cfg->log_resolution = LE16_TO_CPU(cmd->ops.cfg.log_resolution);
	if (cmd->cmd_flags & ICE_AQC_FW_LOG_CONF_AQ_EN)
		cfg->options |= ICE_FWLOG_OPTION_ARQ_ENA;
	if (cmd->cmd_flags & ICE_AQC_FW_LOG_CONF_UART_EN)
		cfg->options |= ICE_FWLOG_OPTION_UART_ENA;
	if (cmd->cmd_flags & ICE_AQC_FW_LOG_QUERY_REGISTERED)
		cfg->options |= ICE_FWLOG_OPTION_IS_REGISTERED;

	fw_modules = (struct ice_aqc_fw_log_cfg_resp *)buf;

	for (i = 0; i < module_id_cnt; i++) {
		struct ice_aqc_fw_log_cfg_resp *fw_module = &fw_modules[i];

		cfg->module_entries[i].module_id =
			LE16_TO_CPU(fw_module->module_identifier);
		cfg->module_entries[i].log_level = fw_module->log_level;
	}

status_out:
	ice_free(hw, buf);
	return status;
}

/**
 * ice_fwlog_set_support_ena - Set if FW logging is supported by FW
 * @hw: pointer to the HW struct
 *
 * If FW returns success to the ice_aq_fwlog_get call then it supports FW
 * logging, else it doesn't. Set the fwlog_support_ena flag accordingly.
 *
 * This function is only meant to be called during driver init to determine if
 * the FW support FW logging.
 */
void ice_fwlog_set_support_ena(struct ice_hw *hw)
{
	struct ice_fwlog_cfg *cfg;
	enum ice_status status;

	hw->fwlog_support_ena = false;

	cfg = (struct ice_fwlog_cfg *)ice_calloc(hw, 1, sizeof(*cfg));
	if (!cfg)
		return;

	/* don't call ice_fwlog_get() because that would overwrite the cached
	 * configuration from the call to ice_fwlog_init(), which is expected to
	 * be called prior to this function
	 */
	status = ice_aq_fwlog_get(hw, cfg);
	if (status)
		ice_debug(hw, ICE_DBG_FW_LOG, "ice_fwlog_get failed, FW logging is not supported on this version of FW, status %d\n",
			  status);
	else
		hw->fwlog_support_ena = true;

	ice_free(hw, cfg);
}

/**
 * ice_fwlog_get - Get the firmware logging settings
 * @hw: pointer to the HW structure
 * @cfg: config to populate based on current firmware logging settings
 */
enum ice_status
ice_fwlog_get(struct ice_hw *hw, struct ice_fwlog_cfg *cfg)
{
	enum ice_status status;

	if (!ice_fwlog_supported(hw))
		return ICE_ERR_NOT_SUPPORTED;

	if (!cfg)
		return ICE_ERR_PARAM;

	status = ice_aq_fwlog_get(hw, cfg);
	if (status)
		return status;

	cache_cfg(hw, cfg);

	return ICE_SUCCESS;
}

/**
 * ice_fwlog_event_dump - Dump the event received over the Admin Receive Queue
 * @hw: pointer to the HW structure
 * @desc: Admin Receive Queue descriptor
 * @buf: buffer that contains the FW log event data
 *
 * If the driver receives the ice_aqc_opc_fw_logs_event on the Admin Receive
 * Queue, then it should call this function to dump the FW log data.
 */
void
ice_fwlog_event_dump(struct ice_hw *hw, struct ice_aq_desc *desc, void *buf)
{
	if (!ice_fwlog_supported(hw))
		return;

	ice_info_fwlog(hw, 32, 1, (u8 *)buf, LE16_TO_CPU(desc->datalen));
}

