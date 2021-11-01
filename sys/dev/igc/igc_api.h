/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef _IGC_API_H_
#define _IGC_API_H_

#include "igc_hw.h"

extern void igc_init_function_pointers_i225(struct igc_hw *hw);

s32 igc_set_mac_type(struct igc_hw *hw);
s32 igc_setup_init_funcs(struct igc_hw *hw, bool init_device);
s32 igc_init_mac_params(struct igc_hw *hw);
s32 igc_init_nvm_params(struct igc_hw *hw);
s32 igc_init_phy_params(struct igc_hw *hw);
s32 igc_get_bus_info(struct igc_hw *hw);
void igc_clear_vfta(struct igc_hw *hw);
void igc_write_vfta(struct igc_hw *hw, u32 offset, u32 value);
s32 igc_force_mac_fc(struct igc_hw *hw);
s32 igc_check_for_link(struct igc_hw *hw);
s32 igc_reset_hw(struct igc_hw *hw);
s32 igc_init_hw(struct igc_hw *hw);
s32 igc_setup_link(struct igc_hw *hw);
s32 igc_get_speed_and_duplex(struct igc_hw *hw, u16 *speed, u16 *duplex);
s32 igc_disable_pcie_master(struct igc_hw *hw);
void igc_config_collision_dist(struct igc_hw *hw);
int igc_rar_set(struct igc_hw *hw, u8 *addr, u32 index);
u32 igc_hash_mc_addr(struct igc_hw *hw, u8 *mc_addr);
void igc_update_mc_addr_list(struct igc_hw *hw, u8 *mc_addr_list,
			       u32 mc_addr_count);
s32 igc_check_reset_block(struct igc_hw *hw);
s32 igc_get_cable_length(struct igc_hw *hw);
s32 igc_validate_mdi_setting(struct igc_hw *hw);
s32 igc_read_phy_reg(struct igc_hw *hw, u32 offset, u16 *data);
s32 igc_write_phy_reg(struct igc_hw *hw, u32 offset, u16 data);
s32 igc_get_phy_info(struct igc_hw *hw);
void igc_release_phy(struct igc_hw *hw);
s32 igc_acquire_phy(struct igc_hw *hw);
s32 igc_phy_hw_reset(struct igc_hw *hw);
void igc_power_up_phy(struct igc_hw *hw);
void igc_power_down_phy(struct igc_hw *hw);
s32 igc_read_mac_addr(struct igc_hw *hw);
s32 igc_read_pba_string(struct igc_hw *hw, u8 *pba_num, u32 pba_num_size);
void igc_reload_nvm(struct igc_hw *hw);
s32 igc_update_nvm_checksum(struct igc_hw *hw);
s32 igc_validate_nvm_checksum(struct igc_hw *hw);
s32 igc_read_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data);
s32 igc_write_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data);
s32 igc_set_d3_lplu_state(struct igc_hw *hw, bool active);
s32 igc_set_d0_lplu_state(struct igc_hw *hw, bool active);

#endif /* _IGC_API_H_ */
