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

#ifndef _ICE_NVM_H_
#define _ICE_NVM_H_

#define ICE_NVM_CMD_READ		0x0000000B
#define ICE_NVM_CMD_WRITE		0x0000000C

/* NVM Access config bits */
#define ICE_NVM_CFG_MODULE_M		MAKEMASK(0xFF, 0)
#define ICE_NVM_CFG_MODULE_S		0
#define ICE_NVM_CFG_FLAGS_M		MAKEMASK(0xF, 8)
#define ICE_NVM_CFG_FLAGS_S		8
#define ICE_NVM_CFG_EXT_FLAGS_M		MAKEMASK(0xF, 12)
#define ICE_NVM_CFG_EXT_FLAGS_S		12
#define ICE_NVM_CFG_ADAPTER_INFO_M	MAKEMASK(0xFFFF, 16)
#define ICE_NVM_CFG_ADAPTER_INFO_S	16

/* NVM Read Get Driver Features */
#define ICE_NVM_GET_FEATURES_MODULE	0xE
#define ICE_NVM_GET_FEATURES_FLAGS	0xF

/* NVM Read/Write Mapped Space */
#define ICE_NVM_REG_RW_MODULE	0x0
#define ICE_NVM_REG_RW_FLAGS	0x1

#pragma pack(1)
struct ice_orom_civd_info {
	u8 signature[4];	/* Must match ASCII '$CIV' characters */
	u8 checksum;		/* Simple modulo 256 sum of all structure bytes must equal 0 */
	__le32 combo_ver;	/* Combo Image Version number */
	u8 combo_name_len;	/* Length of the unicode combo image version string, max of 32 */
	__le16 combo_name[32];	/* Unicode string representing the Combo Image version */
};
#pragma pack()

#define ICE_NVM_ACCESS_MAJOR_VER	0
#define ICE_NVM_ACCESS_MINOR_VER	5

/* NVM Access feature flags. Other bits in the features field are reserved and
 * should be set to zero when reporting the ice_nvm_features structure.
 */
#define ICE_NVM_FEATURES_0_REG_ACCESS	BIT(1)

/* NVM Access Features */
struct ice_nvm_features {
	u8 major;		/* Major version (informational only) */
	u8 minor;		/* Minor version (informational only) */
	u16 size;		/* size of ice_nvm_features structure */
	u8 features[12];	/* Array of feature bits */
};

/* NVM Access command */
struct ice_nvm_access_cmd {
	u32 command;		/* NVM command: READ or WRITE */
	u32 config;		/* NVM command configuration */
	u32 offset;		/* offset to read/write, in bytes */
	u32 data_size;		/* size of data field, in bytes */
};

/* NVM Access data */
union ice_nvm_access_data {
	u32 regval;	/* Storage for register value */
	struct ice_nvm_features drv_features; /* NVM features */
};

u32 ice_nvm_access_get_module(struct ice_nvm_access_cmd *cmd);
u32 ice_nvm_access_get_flags(struct ice_nvm_access_cmd *cmd);
u32 ice_nvm_access_get_adapter(struct ice_nvm_access_cmd *cmd);
enum ice_status
ice_nvm_access_read(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		    union ice_nvm_access_data *data);
enum ice_status
ice_nvm_access_write(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		     union ice_nvm_access_data *data);
enum ice_status
ice_nvm_access_get_features(struct ice_nvm_access_cmd *cmd,
			    union ice_nvm_access_data *data);
enum ice_status
ice_handle_nvm_access(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		      union ice_nvm_access_data *data);

enum ice_status
ice_acquire_nvm(struct ice_hw *hw, enum ice_aq_res_access_type access);
void ice_release_nvm(struct ice_hw *hw);
enum ice_status
ice_aq_read_nvm(struct ice_hw *hw, u16 module_typeid, u32 offset, u16 length,
		void *data, bool last_command, bool read_shadow_ram,
		struct ice_sq_cd *cd);
enum ice_status
ice_read_flat_nvm(struct ice_hw *hw, u32 offset, u32 *length, u8 *data,
		  bool read_shadow_ram);
enum ice_status
ice_get_pfa_module_tlv(struct ice_hw *hw, u16 *module_tlv, u16 *module_tlv_len,
		       u16 module_type);
enum ice_status
ice_get_nvm_minsrevs(struct ice_hw *hw, struct ice_minsrev_info *minsrevs);
enum ice_status
ice_update_nvm_minsrevs(struct ice_hw *hw, struct ice_minsrev_info *minsrevs);
enum ice_status
ice_get_inactive_orom_ver(struct ice_hw *hw, struct ice_orom_info *orom);
enum ice_status
ice_get_inactive_nvm_ver(struct ice_hw *hw, struct ice_nvm_info *nvm);
enum ice_status
ice_get_inactive_netlist_ver(struct ice_hw *hw, struct ice_netlist_info *netlist);
enum ice_status
ice_read_pba_string(struct ice_hw *hw, u8 *pba_num, u32 pba_num_size);
enum ice_status ice_init_nvm(struct ice_hw *hw);
enum ice_status ice_read_sr_word(struct ice_hw *hw, u16 offset, u16 *data);
enum ice_status ice_read_sr_word_aq(struct ice_hw *hw, u16 offset, u16 *data);
enum ice_status
ice_read_sr_buf(struct ice_hw *hw, u16 offset, u16 *words, u16 *data);
enum ice_status
ice_aq_erase_nvm(struct ice_hw *hw, u16 module_typeid, struct ice_sq_cd *cd);
enum ice_status
ice_aq_update_nvm(struct ice_hw *hw, u16 module_typeid, u32 offset,
		  u16 length, void *data, bool last_command, u8 command_flags,
		  struct ice_sq_cd *cd);
enum ice_status
ice_aq_read_nvm_cfg(struct ice_hw *hw, u8 cmd_flags, u16 field_id, void *data,
		    u16 buf_size, u16 *elem_count, struct ice_sq_cd *cd);
enum ice_status
ice_aq_write_nvm_cfg(struct ice_hw *hw, u8 cmd_flags, void *data, u16 buf_size,
		     u16 elem_count, struct ice_sq_cd *cd);
enum ice_status ice_update_sr_checksum(struct ice_hw *hw);
enum ice_status ice_validate_sr_checksum(struct ice_hw *hw, u16 *checksum);
enum ice_status ice_nvm_validate_checksum(struct ice_hw *hw);
enum ice_status ice_nvm_recalculate_checksum(struct ice_hw *hw);
enum ice_status
ice_nvm_write_activate(struct ice_hw *hw, u16 cmd_flags, u8 *response_flags);
#endif /* _ICE_NVM_H_ */
