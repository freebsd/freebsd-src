/******************************************************************************
  SPDX-License-Identifier: BSD-3-Clause

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef _IXGBE_E610_H_
#define _IXGBE_E610_H_

#include "ixgbe_type.h"

void ixgbe_init_aci(struct ixgbe_hw *hw);
void ixgbe_shutdown_aci(struct ixgbe_hw *hw);
s32 ixgbe_aci_send_cmd(struct ixgbe_hw *hw, struct ixgbe_aci_desc *desc,
		       void *buf, u16 buf_size);
bool ixgbe_aci_check_event_pending(struct ixgbe_hw *hw);
s32 ixgbe_aci_get_event(struct ixgbe_hw *hw, struct ixgbe_aci_event *e,
			bool *pending);

void ixgbe_fill_dflt_direct_cmd_desc(struct ixgbe_aci_desc *desc, u16 opcode);

s32 ixgbe_aci_get_fw_ver(struct ixgbe_hw *hw);
s32 ixgbe_aci_send_driver_ver(struct ixgbe_hw *hw, struct ixgbe_driver_ver *dv);
s32 ixgbe_aci_set_pf_context(struct ixgbe_hw *hw, u8 pf_id);

s32 ixgbe_acquire_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		      enum ixgbe_aci_res_access_type access, u32 timeout);
void ixgbe_release_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res);
s32 ixgbe_aci_list_caps(struct ixgbe_hw *hw, void *buf, u16 buf_size,
			u32 *cap_count, enum ixgbe_aci_opc opc);
s32 ixgbe_discover_dev_caps(struct ixgbe_hw *hw,
			    struct ixgbe_hw_dev_caps *dev_caps);
s32 ixgbe_discover_func_caps(struct ixgbe_hw* hw,
			     struct ixgbe_hw_func_caps* func_caps);
s32 ixgbe_get_caps(struct ixgbe_hw *hw);
s32 ixgbe_aci_disable_rxen(struct ixgbe_hw *hw);
s32 ixgbe_aci_get_phy_caps(struct ixgbe_hw *hw, bool qual_mods, u8 report_mode,
			   struct ixgbe_aci_cmd_get_phy_caps_data *pcaps);
bool ixgbe_phy_caps_equals_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *caps,
			       struct ixgbe_aci_cmd_set_phy_cfg_data *cfg);
void ixgbe_copy_phy_caps_to_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *caps,
				struct ixgbe_aci_cmd_set_phy_cfg_data *cfg);
s32 ixgbe_aci_set_phy_cfg(struct ixgbe_hw *hw,
			  struct ixgbe_aci_cmd_set_phy_cfg_data *cfg);
s32 ixgbe_aci_set_link_restart_an(struct ixgbe_hw *hw, bool ena_link);
s32 ixgbe_update_link_info(struct ixgbe_hw *hw);
s32 ixgbe_get_link_status(struct ixgbe_hw *hw, bool *link_up);
s32 ixgbe_aci_get_link_info(struct ixgbe_hw *hw, bool ena_lse,
			    struct ixgbe_link_status *link);
s32 ixgbe_aci_set_event_mask(struct ixgbe_hw *hw, u8 port_num, u16 mask);
s32 ixgbe_configure_lse(struct ixgbe_hw *hw, bool activate, u16 mask);

s32 ixgbe_aci_get_netlist_node(struct ixgbe_hw *hw,
			       struct ixgbe_aci_cmd_get_link_topo *cmd,
			       u8 *node_part_number, u16 *node_handle);
s32 ixgbe_find_netlist_node(struct ixgbe_hw *hw, u8 node_type_ctx,
			    u8 node_part_number, u16 *node_handle);
s32 ixgbe_aci_read_i2c(struct ixgbe_hw *hw,
		       struct ixgbe_aci_cmd_link_topo_addr topo_addr,
		       u16 bus_addr, __le16 addr, u8 params, u8 *data);
s32 ixgbe_aci_write_i2c(struct ixgbe_hw *hw,
			struct ixgbe_aci_cmd_link_topo_addr topo_addr,
			u16 bus_addr, __le16 addr, u8 params, u8 *data);

s32 ixgbe_aci_set_port_id_led(struct ixgbe_hw *hw, bool orig_mode);
s32 ixgbe_aci_set_gpio(struct ixgbe_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx,
		       bool value);
s32 ixgbe_aci_get_gpio(struct ixgbe_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx,
		       bool *value);
s32 ixgbe_aci_sff_eeprom(struct ixgbe_hw *hw, u16 lport, u8 bus_addr,
			 u16 mem_addr, u8 page, u8 page_bank_ctrl, u8 *data,
			 u8 length, bool write);
s32 ixgbe_aci_prog_topo_dev_nvm(struct ixgbe_hw *hw,
			struct ixgbe_aci_cmd_link_topo_params *topo_params);
s32 ixgbe_aci_read_topo_dev_nvm(struct ixgbe_hw *hw,
			struct ixgbe_aci_cmd_link_topo_params *topo_params,
			u32 start_address, u8 *data, u8 data_size);

s32 ixgbe_acquire_nvm(struct ixgbe_hw *hw,
		      enum ixgbe_aci_res_access_type access);
void ixgbe_release_nvm(struct ixgbe_hw *hw);

s32 ixgbe_aci_read_nvm(struct ixgbe_hw *hw, u16 module_typeid, u32 offset,
		       u16 length, void *data, bool last_command,
		       bool read_shadow_ram);

s32 ixgbe_aci_erase_nvm(struct ixgbe_hw *hw, u16 module_typeid);
s32 ixgbe_aci_update_nvm(struct ixgbe_hw *hw, u16 module_typeid,
			 u32 offset, u16 length, void *data,
			 bool last_command, u8 command_flags);

s32 ixgbe_aci_read_nvm_cfg(struct ixgbe_hw *hw, u8 cmd_flags,
			   u16 field_id, void *data, u16 buf_size,
			   u16 *elem_count);
s32 ixgbe_aci_write_nvm_cfg(struct ixgbe_hw *hw, u8 cmd_flags,
			    void *data, u16 buf_size, u16 elem_count);

s32 ixgbe_nvm_validate_checksum(struct ixgbe_hw *hw);
s32 ixgbe_nvm_recalculate_checksum(struct ixgbe_hw *hw);

s32 ixgbe_nvm_write_activate(struct ixgbe_hw *hw, u16 cmd_flags,
			     u8 *response_flags);

s32 ixgbe_get_nvm_minsrevs(struct ixgbe_hw *hw, struct ixgbe_minsrev_info *minsrevs);
s32 ixgbe_update_nvm_minsrevs(struct ixgbe_hw *hw, struct ixgbe_minsrev_info *minsrevs);

s32 ixgbe_get_inactive_nvm_ver(struct ixgbe_hw *hw, struct ixgbe_nvm_info *nvm);
s32 ixgbe_get_active_nvm_ver(struct ixgbe_hw *hw, struct ixgbe_nvm_info *nvm);

s32 ixgbe_get_inactive_netlist_ver(struct ixgbe_hw *hw, struct ixgbe_netlist_info *netlist);
s32 ixgbe_init_nvm(struct ixgbe_hw *hw);

s32 ixgbe_sanitize_operate(struct ixgbe_hw *hw);
s32 ixgbe_sanitize_nvm(struct ixgbe_hw *hw, u8 cmd_flags, u8 *values);

s32 ixgbe_read_sr_word_aci(struct ixgbe_hw  *hw, u16 offset, u16 *data);
s32 ixgbe_read_sr_buf_aci(struct ixgbe_hw *hw, u16 offset, u16 *words, u16 *data);
s32 ixgbe_read_flat_nvm(struct ixgbe_hw  *hw, u32 offset, u32 *length,
			u8 *data, bool read_shadow_ram);

s32 ixgbe_write_sr_word_aci(struct ixgbe_hw *hw, u32 offset, const u16 *data);
s32 ixgbe_write_sr_buf_aci(struct ixgbe_hw *hw, u32 offset, u16 words, const u16 *data);

s32 ixgbe_aci_alternate_write(struct ixgbe_hw *hw, u32 reg_addr0,
			      u32 reg_val0, u32 reg_addr1, u32 reg_val1);
s32 ixgbe_aci_alternate_read(struct ixgbe_hw *hw, u32 reg_addr0,
			     u32 *reg_val0, u32 reg_addr1, u32 *reg_val1);
s32 ixgbe_aci_alternate_write_done(struct ixgbe_hw *hw, u8 bios_mode,
				   bool *reset_needed);
s32 ixgbe_aci_alternate_clear(struct ixgbe_hw *hw);

s32 ixgbe_aci_get_internal_data(struct ixgbe_hw *hw, u16 cluster_id,
				u16 table_id, u32 start, void *buf,
				u16 buf_size, u16 *ret_buf_size,
				u16 *ret_next_cluster, u16 *ret_next_table,
				u32 *ret_next_index);

s32 ixgbe_handle_nvm_access(struct ixgbe_hw *hw,
				struct ixgbe_nvm_access_cmd *cmd,
				struct ixgbe_nvm_access_data *data);

s32 ixgbe_aci_set_health_status_config(struct ixgbe_hw *hw, u8 event_source);

/* E610 operations */
s32 ixgbe_init_ops_E610(struct ixgbe_hw *hw);
s32 ixgbe_reset_hw_E610(struct ixgbe_hw *hw);
s32 ixgbe_start_hw_E610(struct ixgbe_hw *hw);
enum ixgbe_media_type ixgbe_get_media_type_E610(struct ixgbe_hw *hw);
u64 ixgbe_get_supported_physical_layer_E610(struct ixgbe_hw *hw);
s32 ixgbe_setup_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			  bool autoneg_wait);
s32 ixgbe_check_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
			  bool *link_up, bool link_up_wait_to_complete);
s32 ixgbe_get_link_capabilities_E610(struct ixgbe_hw *hw,
				     ixgbe_link_speed *speed,
				     bool *autoneg);
s32 ixgbe_cfg_phy_fc(struct ixgbe_hw *hw,
		     struct ixgbe_aci_cmd_set_phy_cfg_data *cfg,
		     enum ixgbe_fc_mode req_mode);
s32 ixgbe_setup_fc_E610(struct ixgbe_hw *hw);
void ixgbe_fc_autoneg_E610(struct ixgbe_hw *hw);
s32 ixgbe_set_fw_drv_ver_E610(struct ixgbe_hw *hw, u8 maj, u8 min, u8 build,
			      u8 sub, u16 len, const char *driver_ver);
void ixgbe_disable_rx_E610(struct ixgbe_hw *hw);
s32 ixgbe_setup_eee_E610(struct ixgbe_hw *hw, bool enable_eee);
bool ixgbe_fw_recovery_mode_E610(struct ixgbe_hw *hw);
bool ixgbe_fw_rollback_mode_E610(struct ixgbe_hw *hw);
bool ixgbe_get_fw_tsam_mode_E610(struct ixgbe_hw *hw);
s32 ixgbe_init_phy_ops_E610(struct ixgbe_hw *hw);
s32 ixgbe_identify_phy_E610(struct ixgbe_hw *hw);
s32 ixgbe_identify_module_E610(struct ixgbe_hw *hw);
s32 ixgbe_setup_phy_link_E610(struct ixgbe_hw *hw);
s32 ixgbe_get_phy_firmware_version_E610(struct ixgbe_hw *hw,
					u16 *firmware_version);
s32 ixgbe_read_i2c_sff8472_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 *sff8472_data);
s32 ixgbe_read_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
			       u8 *eeprom_data);
s32 ixgbe_write_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 eeprom_data);
s32 ixgbe_check_overtemp_E610(struct ixgbe_hw *hw);
s32 ixgbe_set_phy_power_E610(struct ixgbe_hw *hw, bool on);
s32 ixgbe_enter_lplu_E610(struct ixgbe_hw *hw);
s32 ixgbe_init_eeprom_params_E610(struct ixgbe_hw *hw);
s32 ixgbe_read_ee_aci_E610(struct ixgbe_hw *hw, u16 offset, u16 *data);
s32 ixgbe_read_ee_aci_buffer_E610(struct ixgbe_hw *hw, u16 offset,
				  u16 words, u16 *data);
s32 ixgbe_write_ee_aci_E610(struct ixgbe_hw *hw, u16 offset, u16 data);
s32 ixgbe_write_ee_aci_buffer_E610(struct ixgbe_hw *hw, u16 offset,
				   u16 words, u16 *data);
s32 ixgbe_calc_eeprom_checksum_E610(struct ixgbe_hw *hw);
s32 ixgbe_update_eeprom_checksum_E610(struct ixgbe_hw *hw);
s32 ixgbe_validate_eeprom_checksum_E610(struct ixgbe_hw *hw, u16 *checksum_val);
s32 ixgbe_read_pba_string_E610(struct ixgbe_hw *hw, u8 *pba_num, u32 pba_num_size);

#endif /* _IXGBE_E610_H_ */
