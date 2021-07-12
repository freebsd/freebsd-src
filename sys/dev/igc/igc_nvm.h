/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef _IGC_NVM_H_
#define _IGC_NVM_H_

void igc_init_nvm_ops_generic(struct igc_hw *hw);
s32  igc_null_read_nvm(struct igc_hw *hw, u16 a, u16 b, u16 *c);
void igc_null_nvm_generic(struct igc_hw *hw);
s32  igc_null_led_default(struct igc_hw *hw, u16 *data);
s32  igc_null_write_nvm(struct igc_hw *hw, u16 a, u16 b, u16 *c);
s32  igc_acquire_nvm_generic(struct igc_hw *hw);

s32  igc_poll_eerd_eewr_done(struct igc_hw *hw, int ee_reg);
s32  igc_read_mac_addr_generic(struct igc_hw *hw);
s32  igc_read_pba_string_generic(struct igc_hw *hw, u8 *pba_num,
				   u32 pba_num_size);
s32  igc_read_nvm_eerd(struct igc_hw *hw, u16 offset, u16 words,
			 u16 *data);
s32  igc_valid_led_default_generic(struct igc_hw *hw, u16 *data);
s32  igc_validate_nvm_checksum_generic(struct igc_hw *hw);
s32  igc_write_nvm_spi(struct igc_hw *hw, u16 offset, u16 words,
			 u16 *data);
s32  igc_update_nvm_checksum_generic(struct igc_hw *hw);
void igc_release_nvm_generic(struct igc_hw *hw);

#endif
