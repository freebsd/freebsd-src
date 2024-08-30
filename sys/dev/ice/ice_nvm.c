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

#define GL_MNG_DEF_DEVID 0x000B611C

/**
 * ice_aq_read_nvm
 * @hw: pointer to the HW struct
 * @module_typeid: module pointer location in words from the NVM beginning
 * @offset: byte offset from the module beginning
 * @length: length of the section to be read (in bytes from the offset)
 * @data: command buffer (size [bytes] = length)
 * @last_command: tells if this is the last command in a series
 * @read_shadow_ram: tell if this is a shadow RAM read
 * @cd: pointer to command details structure or NULL
 *
 * Read the NVM using the admin queue commands (0x0701)
 */
int
ice_aq_read_nvm(struct ice_hw *hw, u16 module_typeid, u32 offset, u16 length,
		void *data, bool last_command, bool read_shadow_ram,
		struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	struct ice_aqc_nvm *cmd;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.nvm;

	if (offset > ICE_AQC_NVM_MAX_OFFSET)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_read);

	if (!read_shadow_ram && module_typeid == ICE_AQC_NVM_START_POINT)
		cmd->cmd_flags |= ICE_AQC_NVM_FLASH_ONLY;

	/* If this is the last command in a series, set the proper flag. */
	if (last_command)
		cmd->cmd_flags |= ICE_AQC_NVM_LAST_CMD;
	cmd->module_typeid = CPU_TO_LE16(module_typeid);
	cmd->offset_low = CPU_TO_LE16(offset & 0xFFFF);
	cmd->offset_high = (offset >> 16) & 0xFF;
	cmd->length = CPU_TO_LE16(length);

	return ice_aq_send_cmd(hw, &desc, data, length, cd);
}

/**
 * ice_read_flat_nvm - Read portion of NVM by flat offset
 * @hw: pointer to the HW struct
 * @offset: offset from beginning of NVM
 * @length: (in) number of bytes to read; (out) number of bytes actually read
 * @data: buffer to return data in (sized to fit the specified length)
 * @read_shadow_ram: if true, read from shadow RAM instead of NVM
 *
 * Reads a portion of the NVM, as a flat memory space. This function correctly
 * breaks read requests across Shadow RAM sectors and ensures that no single
 * read request exceeds the maximum 4KB read for a single AdminQ command.
 *
 * Returns a status code on failure. Note that the data pointer may be
 * partially updated if some reads succeed before a failure.
 */
int
ice_read_flat_nvm(struct ice_hw *hw, u32 offset, u32 *length, u8 *data,
		  bool read_shadow_ram)
{
	u32 inlen = *length;
	u32 bytes_read = 0;
	bool last_cmd;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	*length = 0;

	/* Verify the length of the read if this is for the Shadow RAM */
	if (read_shadow_ram && ((offset + inlen) > (hw->flash.sr_words * 2u))) {
		ice_debug(hw, ICE_DBG_NVM, "NVM error: requested data is beyond Shadow RAM limit\n");
		return ICE_ERR_PARAM;
	}

	do {
		u32 read_size, sector_offset;

		/* ice_aq_read_nvm cannot read more than 4KB at a time.
		 * Additionally, a read from the Shadow RAM may not cross over
		 * a sector boundary. Conveniently, the sector size is also
		 * 4KB.
		 */
		sector_offset = offset % ICE_AQ_MAX_BUF_LEN;
		read_size = MIN_T(u32, ICE_AQ_MAX_BUF_LEN - sector_offset,
				  inlen - bytes_read);

		last_cmd = !(bytes_read + read_size < inlen);

		/* ice_aq_read_nvm takes the length as a u16. Our read_size is
		 * calculated using a u32, but the ICE_AQ_MAX_BUF_LEN maximum
		 * size guarantees that it will fit within the 2 bytes.
		 */
		status = ice_aq_read_nvm(hw, ICE_AQC_NVM_START_POINT,
					 offset, (u16)read_size,
					 data + bytes_read, last_cmd,
					 read_shadow_ram, NULL);
		if (status)
			break;

		bytes_read += read_size;
		offset += read_size;
	} while (!last_cmd);

	*length = bytes_read;
	return status;
}

/**
 * ice_aq_update_nvm
 * @hw: pointer to the HW struct
 * @module_typeid: module pointer location in words from the NVM beginning
 * @offset: byte offset from the module beginning
 * @length: length of the section to be written (in bytes from the offset)
 * @data: command buffer (size [bytes] = length)
 * @last_command: tells if this is the last command in a series
 * @command_flags: command parameters
 * @cd: pointer to command details structure or NULL
 *
 * Update the NVM using the admin queue commands (0x0703)
 */
int
ice_aq_update_nvm(struct ice_hw *hw, u16 module_typeid, u32 offset,
		  u16 length, void *data, bool last_command, u8 command_flags,
		  struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	struct ice_aqc_nvm *cmd;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.nvm;

	/* In offset the highest byte must be zeroed. */
	if (offset & 0xFF000000)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_write);

	cmd->cmd_flags |= command_flags;

	/* If this is the last command in a series, set the proper flag. */
	if (last_command)
		cmd->cmd_flags |= ICE_AQC_NVM_LAST_CMD;
	cmd->module_typeid = CPU_TO_LE16(module_typeid);
	cmd->offset_low = CPU_TO_LE16(offset & 0xFFFF);
	cmd->offset_high = (offset >> 16) & 0xFF;
	cmd->length = CPU_TO_LE16(length);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	return ice_aq_send_cmd(hw, &desc, data, length, cd);
}

/**
 * ice_aq_erase_nvm
 * @hw: pointer to the HW struct
 * @module_typeid: module pointer location in words from the NVM beginning
 * @cd: pointer to command details structure or NULL
 *
 * Erase the NVM sector using the admin queue commands (0x0702)
 */
int ice_aq_erase_nvm(struct ice_hw *hw, u16 module_typeid, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	struct ice_aqc_nvm *cmd;
	int status;
	__le16 len;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* read a length value from SR, so module_typeid is equal to 0 */
	/* calculate offset where module size is placed from bytes to words */
	/* set last command and read from SR values to true */
	status = ice_aq_read_nvm(hw, 0, 2 * module_typeid + 2, 2, &len, true,
				 true, NULL);
	if (status)
		return status;

	cmd = &desc.params.nvm;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_erase);

	cmd->module_typeid = CPU_TO_LE16(module_typeid);
	cmd->length = len;
	cmd->offset_low = 0;
	cmd->offset_high = 0;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_read_nvm_cfg - read an NVM config block
 * @hw: pointer to the HW struct
 * @cmd_flags: NVM access admin command bits
 * @field_id: field or feature ID
 * @data: buffer for result
 * @buf_size: buffer size
 * @elem_count: pointer to count of elements read by FW
 * @cd: pointer to command details structure or NULL
 *
 * Reads single or multiple feature/field ID and data (0x0704)
 */
int
ice_aq_read_nvm_cfg(struct ice_hw *hw, u8 cmd_flags, u16 field_id, void *data,
		    u16 buf_size, u16 *elem_count, struct ice_sq_cd *cd)
{
	struct ice_aqc_nvm_cfg *cmd;
	struct ice_aq_desc desc;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.nvm_cfg;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_cfg_read);

	cmd->cmd_flags = cmd_flags;
	cmd->id = CPU_TO_LE16(field_id);

	status = ice_aq_send_cmd(hw, &desc, data, buf_size, cd);
	if (!status && elem_count)
		*elem_count = LE16_TO_CPU(cmd->count);

	return status;
}

/**
 * ice_aq_write_nvm_cfg - write an NVM config block
 * @hw: pointer to the HW struct
 * @cmd_flags: NVM access admin command bits
 * @data: buffer for result
 * @buf_size: buffer size
 * @elem_count: count of elements to be written
 * @cd: pointer to command details structure or NULL
 *
 * Writes single or multiple feature/field ID and data (0x0705)
 */
int
ice_aq_write_nvm_cfg(struct ice_hw *hw, u8 cmd_flags, void *data, u16 buf_size,
		     u16 elem_count, struct ice_sq_cd *cd)
{
	struct ice_aqc_nvm_cfg *cmd;
	struct ice_aq_desc desc;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.nvm_cfg;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_cfg_write);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	cmd->count = CPU_TO_LE16(elem_count);
	cmd->cmd_flags = cmd_flags;

	return ice_aq_send_cmd(hw, &desc, data, buf_size, cd);
}

/**
 * ice_check_sr_access_params - verify params for Shadow RAM R/W operations
 * @hw: pointer to the HW structure
 * @offset: offset in words from module start
 * @words: number of words to access
 */
static int
ice_check_sr_access_params(struct ice_hw *hw, u32 offset, u16 words)
{
	if ((offset + words) > hw->flash.sr_words) {
		ice_debug(hw, ICE_DBG_NVM, "NVM error: offset beyond SR lmt.\n");
		return ICE_ERR_PARAM;
	}

	if (words > ICE_SR_SECTOR_SIZE_IN_WORDS) {
		/* We can access only up to 4KB (one sector), in one AQ write */
		ice_debug(hw, ICE_DBG_NVM, "NVM error: tried to access %d words, limit is %d.\n",
			  words, ICE_SR_SECTOR_SIZE_IN_WORDS);
		return ICE_ERR_PARAM;
	}

	if (((offset + (words - 1)) / ICE_SR_SECTOR_SIZE_IN_WORDS) !=
	    (offset / ICE_SR_SECTOR_SIZE_IN_WORDS)) {
		/* A single access cannot spread over two sectors */
		ice_debug(hw, ICE_DBG_NVM, "NVM error: cannot spread over two sectors.\n");
		return ICE_ERR_PARAM;
	}

	return 0;
}

/**
 * ice_read_sr_word_aq - Reads Shadow RAM via AQ
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @data: word read from the Shadow RAM
 *
 * Reads one 16 bit word from the Shadow RAM using ice_read_flat_nvm.
 */
int ice_read_sr_word_aq(struct ice_hw *hw, u16 offset, u16 *data)
{
	u32 bytes = sizeof(u16);
	__le16 data_local;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* Note that ice_read_flat_nvm checks if the read is past the Shadow
	 * RAM size, and ensures we don't read across a Shadow RAM sector
	 * boundary
	 */
	status = ice_read_flat_nvm(hw, offset * sizeof(u16), &bytes,
				   (_FORCE_ u8 *)&data_local, true);
	if (status)
		return status;

	*data = LE16_TO_CPU(data_local);
	return 0;
}

/**
 * ice_write_sr_aq - Writes Shadow RAM
 * @hw: pointer to the HW structure
 * @offset: offset in words from module start
 * @words: number of words to write
 * @data: buffer with words to write to the Shadow RAM
 * @last_command: tells the AdminQ that this is the last command
 *
 * Writes a 16 bit words buffer to the Shadow RAM using the admin command.
 */
static int
ice_write_sr_aq(struct ice_hw *hw, u32 offset, u16 words, __le16 *data,
		bool last_command)
{
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	status = ice_check_sr_access_params(hw, offset, words);
	if (!status)
		status = ice_aq_update_nvm(hw, 0, 2 * offset, 2 * words, data,
					   last_command, 0, NULL);

	return status;
}

/**
 * ice_read_sr_buf_aq - Reads Shadow RAM buf via AQ
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @words: (in) number of words to read; (out) number of words actually read
 * @data: words read from the Shadow RAM
 *
 * Reads 16 bit words (data buf) from the Shadow RAM. Ownership of the NVM is
 * taken before reading the buffer and later released.
 */
static int
ice_read_sr_buf_aq(struct ice_hw *hw, u16 offset, u16 *words, u16 *data)
{
	u32 bytes = *words * 2, i;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* ice_read_flat_nvm takes into account the 4KB AdminQ and Shadow RAM
	 * sector restrictions necessary when reading from the NVM.
	 */
	status = ice_read_flat_nvm(hw, offset * 2, &bytes, (u8 *)data, true);

	/* Report the number of words successfully read */
	*words = (u16)(bytes / 2);

	/* Byte swap the words up to the amount we actually read */
	for (i = 0; i < *words; i++)
		data[i] = LE16_TO_CPU(((_FORCE_ __le16 *)data)[i]);

	return status;
}

/**
 * ice_acquire_nvm - Generic request for acquiring the NVM ownership
 * @hw: pointer to the HW structure
 * @access: NVM access type (read or write)
 *
 * This function will request NVM ownership.
 */
int ice_acquire_nvm(struct ice_hw *hw, enum ice_aq_res_access_type access)
{
	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);
	if (hw->flash.blank_nvm_mode)
		return 0;

	return ice_acquire_res(hw, ICE_NVM_RES_ID, access, ICE_NVM_TIMEOUT);
}

/**
 * ice_release_nvm - Generic request for releasing the NVM ownership
 * @hw: pointer to the HW structure
 *
 * This function will release NVM ownership.
 */
void ice_release_nvm(struct ice_hw *hw)
{
	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	if (hw->flash.blank_nvm_mode)
		return;

	ice_release_res(hw, ICE_NVM_RES_ID);
}

/**
 * ice_get_flash_bank_offset - Get offset into requested flash bank
 * @hw: pointer to the HW structure
 * @bank: whether to read from the active or inactive flash bank
 * @module: the module to read from
 *
 * Based on the module, lookup the module offset from the beginning of the
 * flash.
 *
 * Returns the flash offset. Note that a value of zero is invalid and must be
 * treated as an error.
 */
static u32 ice_get_flash_bank_offset(struct ice_hw *hw, enum ice_bank_select bank, u16 module)
{
	struct ice_bank_info *banks = &hw->flash.banks;
	enum ice_flash_bank active_bank;
	bool second_bank_active;
	u32 offset, size;

	switch (module) {
	case ICE_SR_1ST_NVM_BANK_PTR:
		offset = banks->nvm_ptr;
		size = banks->nvm_size;
		active_bank = banks->nvm_bank;
		break;
	case ICE_SR_1ST_OROM_BANK_PTR:
		offset = banks->orom_ptr;
		size = banks->orom_size;
		active_bank = banks->orom_bank;
		break;
	case ICE_SR_NETLIST_BANK_PTR:
		offset = banks->netlist_ptr;
		size = banks->netlist_size;
		active_bank = banks->netlist_bank;
		break;
	default:
		ice_debug(hw, ICE_DBG_NVM, "Unexpected value for flash module: 0x%04x\n", module);
		return 0;
	}

	switch (active_bank) {
	case ICE_1ST_FLASH_BANK:
		second_bank_active = false;
		break;
	case ICE_2ND_FLASH_BANK:
		second_bank_active = true;
		break;
	default:
		ice_debug(hw, ICE_DBG_NVM, "Unexpected value for active flash bank: %u\n",
			  active_bank);
		return 0;
	}

	/* The second flash bank is stored immediately following the first
	 * bank. Based on whether the 1st or 2nd bank is active, and whether
	 * we want the active or inactive bank, calculate the desired offset.
	 */
	switch (bank) {
	case ICE_ACTIVE_FLASH_BANK:
		return offset + (second_bank_active ? size : 0);
	case ICE_INACTIVE_FLASH_BANK:
		return offset + (second_bank_active ? 0 : size);
	}

	ice_debug(hw, ICE_DBG_NVM, "Unexpected value for flash bank selection: %u\n", bank);
	return 0;
}

/**
 * ice_read_flash_module - Read a word from one of the main NVM modules
 * @hw: pointer to the HW structure
 * @bank: which bank of the module to read
 * @module: the module to read
 * @offset: the offset into the module in bytes
 * @data: storage for the word read from the flash
 * @length: bytes of data to read
 *
 * Read data from the specified flash module. The bank parameter indicates
 * whether or not to read from the active bank or the inactive bank of that
 * module.
 *
 * The word will be read using flat NVM access, and relies on the
 * hw->flash.banks data being setup by ice_determine_active_flash_banks()
 * during initialization.
 */
static int
ice_read_flash_module(struct ice_hw *hw, enum ice_bank_select bank, u16 module,
		      u32 offset, u8 *data, u32 length)
{
	int status;
	u32 start;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	start = ice_get_flash_bank_offset(hw, bank, module);
	if (!start) {
		ice_debug(hw, ICE_DBG_NVM, "Unable to calculate flash bank offset for module 0x%04x\n",
			  module);
		return ICE_ERR_PARAM;
	}

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	status = ice_read_flat_nvm(hw, start + offset, &length, data, false);

	ice_release_nvm(hw);

	return status;
}

/**
 * ice_read_nvm_module - Read from the active main NVM module
 * @hw: pointer to the HW structure
 * @bank: whether to read from active or inactive NVM module
 * @offset: offset into the NVM module to read, in words
 * @data: storage for returned word value
 *
 * Read the specified word from the active NVM module. This includes the CSS
 * header at the start of the NVM module.
 */
static int
ice_read_nvm_module(struct ice_hw *hw, enum ice_bank_select bank, u32 offset, u16 *data)
{
	__le16 data_local;
	int status;

	status = ice_read_flash_module(hw, bank, ICE_SR_1ST_NVM_BANK_PTR, offset * sizeof(u16),
				       (_FORCE_ u8 *)&data_local, sizeof(u16));
	if (!status)
		*data = LE16_TO_CPU(data_local);

	return status;
}

/**
 * ice_get_nvm_css_hdr_len - Read the CSS header length from the NVM CSS header
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @hdr_len: storage for header length in words
 *
 * Read the CSS header length from the NVM CSS header and add the Authentication
 * header size, and then convert to words.
 */
static int
ice_get_nvm_css_hdr_len(struct ice_hw *hw, enum ice_bank_select bank,
			u32 *hdr_len)
{
	u16 hdr_len_l, hdr_len_h;
	u32 hdr_len_dword;
	int status;

	status = ice_read_nvm_module(hw, bank, ICE_NVM_CSS_HDR_LEN_L,
				     &hdr_len_l);
	if (status)
		return status;

	status = ice_read_nvm_module(hw, bank, ICE_NVM_CSS_HDR_LEN_H,
				     &hdr_len_h);
	if (status)
		return status;

	/* CSS header length is in DWORD, so convert to words and add
	 * authentication header size
	 */
	hdr_len_dword = hdr_len_h << 16 | hdr_len_l;
	*hdr_len = (hdr_len_dword * 2) + ICE_NVM_AUTH_HEADER_LEN;

	return 0;
}

/**
 * ice_read_nvm_sr_copy - Read a word from the Shadow RAM copy in the NVM bank
 * @hw: pointer to the HW structure
 * @bank: whether to read from the active or inactive NVM module
 * @offset: offset into the Shadow RAM copy to read, in words
 * @data: storage for returned word value
 *
 * Read the specified word from the copy of the Shadow RAM found in the
 * specified NVM module.
 */
static int
ice_read_nvm_sr_copy(struct ice_hw *hw, enum ice_bank_select bank, u32 offset, u16 *data)
{
	u32 hdr_len;
	int status;

	status = ice_get_nvm_css_hdr_len(hw, bank, &hdr_len);
	if (status)
		return status;

	hdr_len = ROUND_UP(hdr_len, 32);

	return ice_read_nvm_module(hw, bank, hdr_len + offset, data);
}

/**
 * ice_read_orom_module - Read from the active Option ROM module
 * @hw: pointer to the HW structure
 * @bank: whether to read from active or inactive OROM module
 * @offset: offset into the OROM module to read, in words
 * @data: storage for returned word value
 *
 * Read the specified word from the active Option ROM module of the flash.
 * Note that unlike the NVM module, the CSS data is stored at the end of the
 * module instead of at the beginning.
 */
static int
ice_read_orom_module(struct ice_hw *hw, enum ice_bank_select bank, u32 offset, u16 *data)
{
	__le16 data_local;
	int status;

	status = ice_read_flash_module(hw, bank, ICE_SR_1ST_OROM_BANK_PTR, offset * sizeof(u16),
				       (_FORCE_ u8 *)&data_local, sizeof(u16));
	if (!status)
		*data = LE16_TO_CPU(data_local);

	return status;
}

/**
 * ice_read_netlist_module - Read data from the netlist module area
 * @hw: pointer to the HW structure
 * @bank: whether to read from the active or inactive module
 * @offset: offset into the netlist to read from
 * @data: storage for returned word value
 *
 * Read a word from the specified netlist bank.
 */
static int
ice_read_netlist_module(struct ice_hw *hw, enum ice_bank_select bank, u32 offset, u16 *data)
{
	__le16 data_local;
	int status;

	status = ice_read_flash_module(hw, bank, ICE_SR_NETLIST_BANK_PTR, offset * sizeof(u16),
				       (_FORCE_ u8 *)&data_local, sizeof(u16));
	if (!status)
		*data = LE16_TO_CPU(data_local);

	return status;
}

/**
 * ice_read_sr_word - Reads Shadow RAM word and acquire NVM if necessary
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @data: word read from the Shadow RAM
 *
 * Reads one 16 bit word from the Shadow RAM using the ice_read_sr_word_aq.
 */
int ice_read_sr_word(struct ice_hw *hw, u16 offset, u16 *data)
{
	int status;

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (!status) {
		status = ice_read_sr_word_aq(hw, offset, data);
		ice_release_nvm(hw);
	}

	return status;
}

#define check_add_overflow __builtin_add_overflow

/**
 * ice_get_pfa_module_tlv - Reads sub module TLV from NVM PFA
 * @hw: pointer to hardware structure
 * @module_tlv: pointer to module TLV to return
 * @module_tlv_len: pointer to module TLV length to return
 * @module_type: module type requested
 *
 * Finds the requested sub module TLV type from the Preserved Field
 * Area (PFA) and returns the TLV pointer and length. The caller can
 * use these to read the variable length TLV value.
 */
int
ice_get_pfa_module_tlv(struct ice_hw *hw, u16 *module_tlv, u16 *module_tlv_len,
		       u16 module_type)
{
	u16 pfa_len, pfa_ptr, next_tlv, max_tlv;
	int status;

	status = ice_read_sr_word(hw, ICE_SR_PFA_PTR, &pfa_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Preserved Field Array pointer.\n");
		return status;
	}
	status = ice_read_sr_word(hw, pfa_ptr, &pfa_len);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read PFA length.\n");
		return status;
	}

	if (check_add_overflow(pfa_ptr, (u16)(pfa_len - 1), &max_tlv)) {
		ice_debug(hw, ICE_DBG_INIT, "PFA starts at offset %u. PFA length of %u caused 16-bit arithmetic overflow.\n",
				  pfa_ptr, pfa_len);
		return ICE_ERR_INVAL_SIZE;
	}

	/* The Preserved Fields Area contains a sequence of TLVs which define
	 * its contents. The PFA length includes all of the TLVs, plus its
	 * initial length word itself, *and* one final word at the end of all
	 * of the TLVs.
	 *
	 * Starting with first TLV after PFA length, iterate through the list
	 * of TLVs to find the requested one.
	 */
	next_tlv = pfa_ptr + 1;
	while (next_tlv < max_tlv) {
		u16 tlv_sub_module_type;
		u16 tlv_len;

		/* Read TLV type */
		status = ice_read_sr_word(hw, (u16)next_tlv,
					  &tlv_sub_module_type);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read TLV type.\n");
			break;
		}
		/* Read TLV length */
		status = ice_read_sr_word(hw, (u16)(next_tlv + 1), &tlv_len);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read TLV length.\n");
			break;
		}
		if (tlv_sub_module_type == module_type) {
			if (tlv_len) {
				*module_tlv = (u16)next_tlv;
				*module_tlv_len = tlv_len;
				return 0;
			}
			return ICE_ERR_INVAL_SIZE;
		}

		if (check_add_overflow(next_tlv, (u16)2, &next_tlv) ||
		    check_add_overflow(next_tlv, tlv_len, &next_tlv)) {
			ice_debug(hw, ICE_DBG_INIT, "TLV of type %u and length 0x%04x caused 16-bit arithmetic overflow. The PFA starts at 0x%04x and has length of 0x%04x\n",
					  tlv_sub_module_type, tlv_len, pfa_ptr, pfa_len);
			return ICE_ERR_INVAL_SIZE;
		}
	}
	/* Module does not exist */
	return ICE_ERR_DOES_NOT_EXIST;
}

/**
 * ice_read_pba_string - Reads part number string from NVM
 * @hw: pointer to hardware structure
 * @pba_num: stores the part number string from the NVM
 * @pba_num_size: part number string buffer length
 *
 * Reads the part number string from the NVM.
 */
int ice_read_pba_string(struct ice_hw *hw, u8 *pba_num, u32 pba_num_size)
{
	u16 pba_tlv, pba_tlv_len;
	u16 pba_word, pba_size;
	int status;
	u16 i;

	status = ice_get_pfa_module_tlv(hw, &pba_tlv, &pba_tlv_len,
					ICE_SR_PBA_BLOCK_PTR);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read PBA Block TLV.\n");
		return status;
	}

	/* pba_size is the next word */
	status = ice_read_sr_word(hw, (pba_tlv + 2), &pba_size);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read PBA Section size.\n");
		return status;
	}

	if (pba_tlv_len < pba_size) {
		ice_debug(hw, ICE_DBG_INIT, "Invalid PBA Block TLV size.\n");
		return ICE_ERR_INVAL_SIZE;
	}

	/* Subtract one to get PBA word count (PBA Size word is included in
	 * total size)
	 */
	pba_size--;
	if (pba_num_size < (((u32)pba_size * 2) + 1)) {
		ice_debug(hw, ICE_DBG_INIT, "Buffer too small for PBA data.\n");
		return ICE_ERR_PARAM;
	}

	for (i = 0; i < pba_size; i++) {
		status = ice_read_sr_word(hw, (pba_tlv + 2 + 1) + i, &pba_word);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read PBA Block word %d.\n", i);
			return status;
		}

		pba_num[(i * 2)] = (pba_word >> 8) & 0xFF;
		pba_num[(i * 2) + 1] = pba_word & 0xFF;
	}
	pba_num[(pba_size * 2)] = '\0';

	return status;
}

/**
 * ice_get_nvm_srev - Read the security revision from the NVM CSS header
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @srev: storage for security revision
 *
 * Read the security revision out of the CSS header of the active NVM module
 * bank.
 */
static int ice_get_nvm_srev(struct ice_hw *hw, enum ice_bank_select bank, u32 *srev)
{
	u16 srev_l, srev_h;
	int status;

	status = ice_read_nvm_module(hw, bank, ICE_NVM_CSS_SREV_L, &srev_l);
	if (status)
		return status;

	status = ice_read_nvm_module(hw, bank, ICE_NVM_CSS_SREV_H, &srev_h);
	if (status)
		return status;

	*srev = srev_h << 16 | srev_l;

	return 0;
}

/**
 * ice_get_nvm_ver_info - Read NVM version information
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @nvm: pointer to NVM info structure
 *
 * Read the NVM EETRACK ID and map version of the main NVM image bank, filling
 * in the NVM info structure.
 */
static int
ice_get_nvm_ver_info(struct ice_hw *hw, enum ice_bank_select bank, struct ice_nvm_info *nvm)
{
	u16 eetrack_lo, eetrack_hi, ver;
	int status;

	status = ice_read_nvm_sr_copy(hw, bank, ICE_SR_NVM_DEV_STARTER_VER, &ver);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read DEV starter version.\n");
		return status;
	}

	nvm->major = (ver & ICE_NVM_VER_HI_MASK) >> ICE_NVM_VER_HI_SHIFT;
	nvm->minor = (ver & ICE_NVM_VER_LO_MASK) >> ICE_NVM_VER_LO_SHIFT;

	status = ice_read_nvm_sr_copy(hw, bank, ICE_SR_NVM_EETRACK_LO, &eetrack_lo);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read EETRACK lo.\n");
		return status;
	}
	status = ice_read_nvm_sr_copy(hw, bank, ICE_SR_NVM_EETRACK_HI, &eetrack_hi);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read EETRACK hi.\n");
		return status;
	}

	nvm->eetrack = (eetrack_hi << 16) | eetrack_lo;

	status = ice_get_nvm_srev(hw, bank, &nvm->srev);
	if (status)
		ice_debug(hw, ICE_DBG_NVM, "Failed to read NVM security revision.\n");

	return 0;
}

/**
 * ice_get_inactive_nvm_ver - Read Option ROM version from the inactive bank
 * @hw: pointer to the HW structure
 * @nvm: storage for Option ROM version information
 *
 * Reads the NVM EETRACK ID, Map version, and security revision of the
 * inactive NVM bank. Used to access version data for a pending update that
 * has not yet been activated.
 */
int ice_get_inactive_nvm_ver(struct ice_hw *hw, struct ice_nvm_info *nvm)
{
	return ice_get_nvm_ver_info(hw, ICE_INACTIVE_FLASH_BANK, nvm);
}

/**
 * ice_get_orom_srev - Read the security revision from the OROM CSS header
 * @hw: pointer to the HW struct
 * @bank: whether to read from active or inactive flash module
 * @srev: storage for security revision
 *
 * Read the security revision out of the CSS header of the active OROM module
 * bank.
 */
static int ice_get_orom_srev(struct ice_hw *hw, enum ice_bank_select bank, u32 *srev)
{
	u32 orom_size_word = hw->flash.banks.orom_size / 2;
	u16 srev_l, srev_h;
	u32 css_start;
	u32 hdr_len;
	int status;

	status = ice_get_nvm_css_hdr_len(hw, bank, &hdr_len);
	if (status)
		return status;

	if (orom_size_word < hdr_len) {
		ice_debug(hw, ICE_DBG_NVM, "Unexpected Option ROM Size of %u\n",
			  hw->flash.banks.orom_size);
		return ICE_ERR_CFG;
	}

	/* calculate how far into the Option ROM the CSS header starts. Note
	 * that ice_read_orom_module takes a word offset
	 */
	css_start = orom_size_word - hdr_len;
	status = ice_read_orom_module(hw, bank, css_start + ICE_NVM_CSS_SREV_L, &srev_l);
	if (status)
		return status;

	status = ice_read_orom_module(hw, bank, css_start + ICE_NVM_CSS_SREV_H, &srev_h);
	if (status)
		return status;

	*srev = srev_h << 16 | srev_l;

	return 0;
}

/**
 * ice_get_orom_civd_data - Get the combo version information from Option ROM
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash module
 * @civd: storage for the Option ROM CIVD data.
 *
 * Searches through the Option ROM flash contents to locate the CIVD data for
 * the image.
 */
static int
ice_get_orom_civd_data(struct ice_hw *hw, enum ice_bank_select bank,
		       struct ice_orom_civd_info *civd)
{
	struct ice_orom_civd_info civd_data_section;
	int status;
	u32 offset;
	u32 tmp;

	/* The CIVD section is located in the Option ROM aligned to 512 bytes.
	 * The first 4 bytes must contain the ASCII characters "$CIV".
	 * A simple modulo 256 sum of all of the bytes of the structure must
	 * equal 0.
	 *
	 * The exact location is unknown and varies between images but is
	 * usually somewhere in the middle of the bank. We need to scan the
	 * Option ROM bank to locate it.
	 *
	 */

	/* Scan the memory buffer to locate the CIVD data section */
	for (offset = 0; (offset + 512) <= hw->flash.banks.orom_size; offset += 512) {
		u8 sum = 0, i;

		status = ice_read_flash_module(hw, bank, ICE_SR_1ST_OROM_BANK_PTR,
					       offset, (u8 *)&tmp, sizeof(tmp));
		if (status) {
			ice_debug(hw, ICE_DBG_NVM, "Unable to read Option ROM data\n");
			return status;
		}

		/* Skip forward until we find a matching signature */
		if (memcmp("$CIV", &tmp, sizeof(tmp)) != 0)
			continue;

		ice_debug(hw, ICE_DBG_NVM, "Found CIVD section at offset %u\n",
			  offset);

		status = ice_read_flash_module(hw, bank, ICE_SR_1ST_OROM_BANK_PTR,
					       offset, (u8 *)&civd_data_section,
					       sizeof(civd_data_section));
		if (status) {
			ice_debug(hw, ICE_DBG_NVM, "Unable to read CIVD data\n");
			goto exit_error;
		}

		/* Verify that the simple checksum is zero */
		for (i = 0; i < sizeof(civd_data_section); i++)
			sum += ((u8 *)&civd_data_section)[i];

		if (sum) {
			ice_debug(hw, ICE_DBG_NVM, "Found CIVD data with invalid checksum of %u\n",
				  sum);
			status = ICE_ERR_NVM;
			goto exit_error;
		}

		*civd = civd_data_section;

		return 0;
	}

	status = ICE_ERR_NVM;
	ice_debug(hw, ICE_DBG_NVM, "Unable to locate CIVD data within the Option ROM\n");

exit_error:
	return status;
}

/**
 * ice_get_orom_ver_info - Read Option ROM version information
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash module
 * @orom: pointer to Option ROM info structure
 *
 * Read Option ROM version and security revision from the Option ROM flash
 * section.
 */
static int
ice_get_orom_ver_info(struct ice_hw *hw, enum ice_bank_select bank, struct ice_orom_info *orom)
{
	struct ice_orom_civd_info civd;
	u32 combo_ver;
	int status;

	status = ice_get_orom_civd_data(hw, bank, &civd);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to locate valid Option ROM CIVD data\n");
		return status;
	}

	combo_ver = LE32_TO_CPU(civd.combo_ver);

	orom->major = (u8)((combo_ver & ICE_OROM_VER_MASK) >> ICE_OROM_VER_SHIFT);
	orom->patch = (u8)(combo_ver & ICE_OROM_VER_PATCH_MASK);
	orom->build = (u16)((combo_ver & ICE_OROM_VER_BUILD_MASK) >> ICE_OROM_VER_BUILD_SHIFT);

	status = ice_get_orom_srev(hw, bank, &orom->srev);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read Option ROM security revision.\n");
		return status;
	}

	return 0;
}

/**
 * ice_get_inactive_orom_ver - Read Option ROM version from the inactive bank
 * @hw: pointer to the HW structure
 * @orom: storage for Option ROM version information
 *
 * Reads the Option ROM version and security revision data for the inactive
 * section of flash. Used to access version data for a pending update that has
 * not yet been activated.
 */
int ice_get_inactive_orom_ver(struct ice_hw *hw, struct ice_orom_info *orom)
{
	return ice_get_orom_ver_info(hw, ICE_INACTIVE_FLASH_BANK, orom);
}

/**
 * ice_get_netlist_info
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @netlist: pointer to netlist version info structure
 *
 * Get the netlist version information from the requested bank. Reads the Link
 * Topology section to find the Netlist ID block and extract the relevant
 * information into the netlist version structure.
 */
static int
ice_get_netlist_info(struct ice_hw *hw, enum ice_bank_select bank,
		     struct ice_netlist_info *netlist)
{
	u16 module_id, length, node_count, i;
	u16 *id_blk;
	int status;

	status = ice_read_netlist_module(hw, bank, ICE_NETLIST_TYPE_OFFSET, &module_id);
	if (status)
		return status;

	if (module_id != ICE_NETLIST_LINK_TOPO_MOD_ID) {
		ice_debug(hw, ICE_DBG_NVM, "Expected netlist module_id ID of 0x%04x, but got 0x%04x\n",
			  ICE_NETLIST_LINK_TOPO_MOD_ID, module_id);
		return ICE_ERR_NVM;
	}

	status = ice_read_netlist_module(hw, bank, ICE_LINK_TOPO_MODULE_LEN, &length);
	if (status)
		return status;

	/* sanity check that we have at least enough words to store the netlist ID block */
	if (length < ICE_NETLIST_ID_BLK_SIZE) {
		ice_debug(hw, ICE_DBG_NVM, "Netlist Link Topology module too small. Expected at least %u words, but got %u words.\n",
			  ICE_NETLIST_ID_BLK_SIZE, length);
		return ICE_ERR_NVM;
	}

	status = ice_read_netlist_module(hw, bank, ICE_LINK_TOPO_NODE_COUNT, &node_count);
	if (status)
		return status;
	node_count &= ICE_LINK_TOPO_NODE_COUNT_M;

	id_blk = (u16 *)ice_calloc(hw, ICE_NETLIST_ID_BLK_SIZE, sizeof(*id_blk));
	if (!id_blk)
		return ICE_ERR_NO_MEMORY;

	/* Read out the entire Netlist ID Block at once. */
	status = ice_read_flash_module(hw, bank, ICE_SR_NETLIST_BANK_PTR,
				       ICE_NETLIST_ID_BLK_OFFSET(node_count) * sizeof(u16),
				       (u8 *)id_blk, ICE_NETLIST_ID_BLK_SIZE * sizeof(u16));
	if (status)
		goto exit_error;

	for (i = 0; i < ICE_NETLIST_ID_BLK_SIZE; i++)
		id_blk[i] = LE16_TO_CPU(((_FORCE_ __le16 *)id_blk)[i]);

	netlist->major = id_blk[ICE_NETLIST_ID_BLK_MAJOR_VER_HIGH] << 16 |
			 id_blk[ICE_NETLIST_ID_BLK_MAJOR_VER_LOW];
	netlist->minor = id_blk[ICE_NETLIST_ID_BLK_MINOR_VER_HIGH] << 16 |
			 id_blk[ICE_NETLIST_ID_BLK_MINOR_VER_LOW];
	netlist->type = id_blk[ICE_NETLIST_ID_BLK_TYPE_HIGH] << 16 |
			id_blk[ICE_NETLIST_ID_BLK_TYPE_LOW];
	netlist->rev = id_blk[ICE_NETLIST_ID_BLK_REV_HIGH] << 16 |
		       id_blk[ICE_NETLIST_ID_BLK_REV_LOW];
	netlist->cust_ver = id_blk[ICE_NETLIST_ID_BLK_CUST_VER];
	/* Read the left most 4 bytes of SHA */
	netlist->hash = id_blk[ICE_NETLIST_ID_BLK_SHA_HASH_WORD(15)] << 16 |
			id_blk[ICE_NETLIST_ID_BLK_SHA_HASH_WORD(14)];

exit_error:
	ice_free(hw, id_blk);

	return status;
}

/**
 * ice_get_netlist_ver_info
 * @hw: pointer to the HW struct
 * @netlist: pointer to netlist version info structure
 *
 * Get the netlist version information
 */
int ice_get_netlist_ver_info(struct ice_hw *hw, struct ice_netlist_info *netlist)
{
	return ice_get_netlist_info(hw, ICE_ACTIVE_FLASH_BANK, netlist);
}

/**
 * ice_get_inactive_netlist_ver
 * @hw: pointer to the HW struct
 * @netlist: pointer to netlist version info structure
 *
 * Read the netlist version data from the inactive netlist bank. Used to
 * extract version data of a pending flash update in order to display the
 * version data.
 */
int ice_get_inactive_netlist_ver(struct ice_hw *hw, struct ice_netlist_info *netlist)
{
	return ice_get_netlist_info(hw, ICE_INACTIVE_FLASH_BANK, netlist);
}

/**
 * ice_discover_flash_size - Discover the available flash size
 * @hw: pointer to the HW struct
 *
 * The device flash could be up to 16MB in size. However, it is possible that
 * the actual size is smaller. Use bisection to determine the accessible size
 * of flash memory.
 */
static int ice_discover_flash_size(struct ice_hw *hw)
{
	u32 min_size = 0, max_size = ICE_AQC_NVM_MAX_OFFSET + 1;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	while ((max_size - min_size) > 1) {
		u32 offset = (max_size + min_size) / 2;
		u32 len = 1;
		u8 data;

		status = ice_read_flat_nvm(hw, offset, &len, &data, false);
		if (status == ICE_ERR_AQ_ERROR &&
		    hw->adminq.sq_last_status == ICE_AQ_RC_EINVAL) {
			ice_debug(hw, ICE_DBG_NVM, "%s: New upper bound of %u bytes\n",
				  __func__, offset);
			status = 0;
			max_size = offset;
		} else if (!status) {
			ice_debug(hw, ICE_DBG_NVM, "%s: New lower bound of %u bytes\n",
				  __func__, offset);
			min_size = offset;
		} else {
			/* an unexpected error occurred */
			goto err_read_flat_nvm;
		}
	}

	ice_debug(hw, ICE_DBG_NVM, "Predicted flash size is %u bytes\n", max_size);

	hw->flash.flash_size = max_size;

err_read_flat_nvm:
	ice_release_nvm(hw);

	return status;
}

/**
 * ice_read_sr_pointer - Read the value of a Shadow RAM pointer word
 * @hw: pointer to the HW structure
 * @offset: the word offset of the Shadow RAM word to read
 * @pointer: pointer value read from Shadow RAM
 *
 * Read the given Shadow RAM word, and convert it to a pointer value specified
 * in bytes. This function assumes the specified offset is a valid pointer
 * word.
 *
 * Each pointer word specifies whether it is stored in word size or 4KB
 * sector size by using the highest bit. The reported pointer value will be in
 * bytes, intended for flat NVM reads.
 */
static int ice_read_sr_pointer(struct ice_hw *hw, u16 offset, u32 *pointer)
{
	int status;
	u16 value;

	status = ice_read_sr_word(hw, offset, &value);
	if (status)
		return status;

	/* Determine if the pointer is in 4KB or word units */
	if (value & ICE_SR_NVM_PTR_4KB_UNITS)
		*pointer = (value & ~ICE_SR_NVM_PTR_4KB_UNITS) * 4 * 1024;
	else
		*pointer = value * 2;

	return 0;
}

/**
 * ice_read_sr_area_size - Read an area size from a Shadow RAM word
 * @hw: pointer to the HW structure
 * @offset: the word offset of the Shadow RAM to read
 * @size: size value read from the Shadow RAM
 *
 * Read the given Shadow RAM word, and convert it to an area size value
 * specified in bytes. This function assumes the specified offset is a valid
 * area size word.
 *
 * Each area size word is specified in 4KB sector units. This function reports
 * the size in bytes, intended for flat NVM reads.
 */
static int ice_read_sr_area_size(struct ice_hw *hw, u16 offset, u32 *size)
{
	int status;
	u16 value;

	status = ice_read_sr_word(hw, offset, &value);
	if (status)
		return status;

	/* Area sizes are always specified in 4KB units */
	*size = value * 4 * 1024;

	return 0;
}

/**
 * ice_determine_active_flash_banks - Discover active bank for each module
 * @hw: pointer to the HW struct
 *
 * Read the Shadow RAM control word and determine which banks are active for
 * the NVM, OROM, and Netlist modules. Also read and calculate the associated
 * pointer and size. These values are then cached into the ice_flash_info
 * structure for later use in order to calculate the correct offset to read
 * from the active module.
 */
static int ice_determine_active_flash_banks(struct ice_hw *hw)
{
	struct ice_bank_info *banks = &hw->flash.banks;
	u16 ctrl_word;
	int status;

	status = ice_read_sr_word(hw, ICE_SR_NVM_CTRL_WORD, &ctrl_word);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read the Shadow RAM control word\n");
		return status;
	}

	/* Check that the control word indicates validity */
	if ((ctrl_word & ICE_SR_CTRL_WORD_1_M) >> ICE_SR_CTRL_WORD_1_S != ICE_SR_CTRL_WORD_VALID) {
		ice_debug(hw, ICE_DBG_NVM, "Shadow RAM control word is invalid\n");
		return ICE_ERR_CFG;
	}

	if (!(ctrl_word & ICE_SR_CTRL_WORD_NVM_BANK))
		banks->nvm_bank = ICE_1ST_FLASH_BANK;
	else
		banks->nvm_bank = ICE_2ND_FLASH_BANK;

	if (!(ctrl_word & ICE_SR_CTRL_WORD_OROM_BANK))
		banks->orom_bank = ICE_1ST_FLASH_BANK;
	else
		banks->orom_bank = ICE_2ND_FLASH_BANK;

	if (!(ctrl_word & ICE_SR_CTRL_WORD_NETLIST_BANK))
		banks->netlist_bank = ICE_1ST_FLASH_BANK;
	else
		banks->netlist_bank = ICE_2ND_FLASH_BANK;

	status = ice_read_sr_pointer(hw, ICE_SR_1ST_NVM_BANK_PTR, &banks->nvm_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read NVM bank pointer\n");
		return status;
	}

	status = ice_read_sr_area_size(hw, ICE_SR_NVM_BANK_SIZE, &banks->nvm_size);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read NVM bank area size\n");
		return status;
	}

	status = ice_read_sr_pointer(hw, ICE_SR_1ST_OROM_BANK_PTR, &banks->orom_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read OROM bank pointer\n");
		return status;
	}

	status = ice_read_sr_area_size(hw, ICE_SR_OROM_BANK_SIZE, &banks->orom_size);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read OROM bank area size\n");
		return status;
	}

	status = ice_read_sr_pointer(hw, ICE_SR_NETLIST_BANK_PTR, &banks->netlist_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read Netlist bank pointer\n");
		return status;
	}

	status = ice_read_sr_area_size(hw, ICE_SR_NETLIST_BANK_SIZE, &banks->netlist_size);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read Netlist bank area size\n");
		return status;
	}

	return 0;
}

/**
 * ice_init_nvm - initializes NVM setting
 * @hw: pointer to the HW struct
 *
 * This function reads and populates NVM settings such as Shadow RAM size,
 * max_timeout, and blank_nvm_mode
 */
int ice_init_nvm(struct ice_hw *hw)
{
	struct ice_flash_info *flash = &hw->flash;
	u32 fla, gens_stat;
	u8 sr_size;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* The SR size is stored regardless of the NVM programming mode
	 * as the blank mode may be used in the factory line.
	 */
	gens_stat = rd32(hw, GLNVM_GENS);
	sr_size = (gens_stat & GLNVM_GENS_SR_SIZE_M) >> GLNVM_GENS_SR_SIZE_S;

	/* Switching to words (sr_size contains power of 2) */
	flash->sr_words = BIT(sr_size) * ICE_SR_WORDS_IN_1KB;

	/* Check if we are in the normal or blank NVM programming mode */
	fla = rd32(hw, GLNVM_FLA);
	if (fla & GLNVM_FLA_LOCKED_M) { /* Normal programming mode */
		flash->blank_nvm_mode = false;
	} else {
		/* Blank programming mode */
		flash->blank_nvm_mode = true;
		ice_debug(hw, ICE_DBG_NVM, "NVM init error: unsupported blank mode.\n");
		return ICE_ERR_NVM_BLANK_MODE;
	}

	status = ice_discover_flash_size(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "NVM init error: failed to discover flash size.\n");
		return status;
	}

	status = ice_determine_active_flash_banks(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to determine active flash banks.\n");
		return status;
	}

	status = ice_get_nvm_ver_info(hw, ICE_ACTIVE_FLASH_BANK, &flash->nvm);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read NVM info.\n");
		return status;
	}

	status = ice_get_orom_ver_info(hw, ICE_ACTIVE_FLASH_BANK, &flash->orom);
	if (status)
		ice_debug(hw, ICE_DBG_INIT, "Failed to read Option ROM info.\n");

	/* read the netlist version information */
	status = ice_get_netlist_info(hw, ICE_ACTIVE_FLASH_BANK, &flash->netlist);
	if (status)
		ice_debug(hw, ICE_DBG_INIT, "Failed to read netlist info.\n");

	return 0;
}

/**
 * ice_read_sr_buf - Reads Shadow RAM buf and acquire lock if necessary
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @words: (in) number of words to read; (out) number of words actually read
 * @data: words read from the Shadow RAM
 *
 * Reads 16 bit words (data buf) from the SR using the ice_read_nvm_buf_aq
 * method. The buf read is preceded by the NVM ownership take
 * and followed by the release.
 */
int
ice_read_sr_buf(struct ice_hw *hw, u16 offset, u16 *words, u16 *data)
{
	int status;

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (!status) {
		status = ice_read_sr_buf_aq(hw, offset, words, data);
		ice_release_nvm(hw);
	}

	return status;
}

/**
 * __ice_write_sr_word - Writes Shadow RAM word
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to write
 * @data: word to write to the Shadow RAM
 *
 * Writes a 16 bit word to the SR using the ice_write_sr_aq method.
 * NVM ownership have to be acquired and released (on ARQ completion event
 * reception) by caller. To commit SR to NVM update checksum function
 * should be called.
 */
int
__ice_write_sr_word(struct ice_hw *hw, u32 offset, const u16 *data)
{
	__le16 data_local = CPU_TO_LE16(*data);

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* Value 0x00 below means that we treat SR as a flat mem */
	return ice_write_sr_aq(hw, offset, 1, &data_local, false);
}

/**
 * __ice_write_sr_buf - Writes Shadow RAM buf
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM buffer to write
 * @words: number of words to write
 * @data: words to write to the Shadow RAM
 *
 * Writes a 16 bit words buffer to the Shadow RAM using the admin command.
 * NVM ownership must be acquired before calling this function and released
 * on ARQ completion event reception by caller. To commit SR to NVM update
 * checksum function should be called.
 */
int
__ice_write_sr_buf(struct ice_hw *hw, u32 offset, u16 words, const u16 *data)
{
	__le16 *data_local;
	int status;
	void *vmem;
	u32 i;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	vmem = ice_calloc(hw, words, sizeof(u16));
	if (!vmem)
		return ICE_ERR_NO_MEMORY;
	data_local = (_FORCE_ __le16 *)vmem;

	for (i = 0; i < words; i++)
		data_local[i] = CPU_TO_LE16(data[i]);

	/* Here we will only write one buffer as the size of the modules
	 * mirrored in the Shadow RAM is always less than 4K.
	 */
	status = ice_write_sr_aq(hw, offset, words, data_local, false);

	ice_free(hw, vmem);

	return status;
}

/**
 * ice_calc_sr_checksum - Calculates and returns Shadow RAM SW checksum
 * @hw: pointer to hardware structure
 * @checksum: pointer to the checksum
 *
 * This function calculates SW Checksum that covers the whole 64kB shadow RAM
 * except the VPD and PCIe ALT Auto-load modules. The structure and size of VPD
 * is customer specific and unknown. Therefore, this function skips all maximum
 * possible size of VPD (1kB).
 */
static int ice_calc_sr_checksum(struct ice_hw *hw, u16 *checksum)
{
	u16 pcie_alt_module = 0;
	u16 checksum_local = 0;
	u16 vpd_module;
	int status = 0;
	void *vmem;
	u16 *data;
	u16 i;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	vmem = ice_calloc(hw, ICE_SR_SECTOR_SIZE_IN_WORDS, sizeof(u16));
	if (!vmem)
		return ICE_ERR_NO_MEMORY;
	data = (u16 *)vmem;

	/* read pointer to VPD area */
	status = ice_read_sr_word_aq(hw, ICE_SR_VPD_PTR, &vpd_module);
	if (status)
		goto ice_calc_sr_checksum_exit;

	/* read pointer to PCIe Alt Auto-load module */
	status = ice_read_sr_word_aq(hw, ICE_SR_PCIE_ALT_AUTO_LOAD_PTR,
				     &pcie_alt_module);
	if (status)
		goto ice_calc_sr_checksum_exit;

	/* Calculate SW checksum that covers the whole 64kB shadow RAM
	 * except the VPD and PCIe ALT Auto-load modules
	 */
	for (i = 0; i < hw->flash.sr_words; i++) {
		/* Read SR page */
		if ((i % ICE_SR_SECTOR_SIZE_IN_WORDS) == 0) {
			u16 words = ICE_SR_SECTOR_SIZE_IN_WORDS;

			status = ice_read_sr_buf_aq(hw, i, &words, data);
			if (status)
				goto ice_calc_sr_checksum_exit;
		}

		/* Skip Checksum word */
		if (i == ICE_SR_SW_CHECKSUM_WORD)
			continue;
		/* Skip VPD module (convert byte size to word count) */
		if (i >= (u32)vpd_module &&
		    i < ((u32)vpd_module + ICE_SR_VPD_SIZE_WORDS))
			continue;
		/* Skip PCIe ALT module (convert byte size to word count) */
		if (i >= (u32)pcie_alt_module &&
		    i < ((u32)pcie_alt_module + ICE_SR_PCIE_ALT_SIZE_WORDS))
			continue;

		checksum_local += data[i % ICE_SR_SECTOR_SIZE_IN_WORDS];
	}

	*checksum = (u16)ICE_SR_SW_CHECKSUM_BASE - checksum_local;

ice_calc_sr_checksum_exit:
	ice_free(hw, vmem);
	return status;
}

/**
 * ice_update_sr_checksum - Updates the Shadow RAM SW checksum
 * @hw: pointer to hardware structure
 *
 * NVM ownership must be acquired before calling this function and released
 * on ARQ completion event reception by caller.
 * This function will commit SR to NVM.
 */
int ice_update_sr_checksum(struct ice_hw *hw)
{
	__le16 le_sum;
	u16 checksum;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	status = ice_calc_sr_checksum(hw, &checksum);
	if (!status) {
		le_sum = CPU_TO_LE16(checksum);
		status = ice_write_sr_aq(hw, ICE_SR_SW_CHECKSUM_WORD, 1,
					 &le_sum, true);
	}
	return status;
}

/**
 * ice_validate_sr_checksum - Validate Shadow RAM SW checksum
 * @hw: pointer to hardware structure
 * @checksum: calculated checksum
 *
 * Performs checksum calculation and validates the Shadow RAM SW checksum.
 * If the caller does not need checksum, the value can be NULL.
 */
int ice_validate_sr_checksum(struct ice_hw *hw, u16 *checksum)
{
	u16 checksum_local;
	u16 checksum_sr;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (!status) {
		status = ice_calc_sr_checksum(hw, &checksum_local);
		ice_release_nvm(hw);
		if (status)
			return status;
	} else {
		return status;
	}

	ice_read_sr_word(hw, ICE_SR_SW_CHECKSUM_WORD, &checksum_sr);

	/* Verify read checksum from EEPROM is the same as
	 * calculated checksum
	 */
	if (checksum_local != checksum_sr)
		status = ICE_ERR_NVM_CHECKSUM;

	/* If the user cares, return the calculated checksum */
	if (checksum)
		*checksum = checksum_local;

	return status;
}

/**
 * ice_nvm_validate_checksum
 * @hw: pointer to the HW struct
 *
 * Verify NVM PFA checksum validity (0x0706)
 */
int ice_nvm_validate_checksum(struct ice_hw *hw)
{
	struct ice_aqc_nvm_checksum *cmd;
	struct ice_aq_desc desc;
	int status;

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	cmd = &desc.params.nvm_checksum;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_checksum);
	cmd->flags = ICE_AQC_NVM_CHECKSUM_VERIFY;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	ice_release_nvm(hw);

	if (!status)
		if (LE16_TO_CPU(cmd->checksum) != ICE_AQC_NVM_CHECKSUM_CORRECT)
			status = ICE_ERR_NVM_CHECKSUM;

	return status;
}

/**
 * ice_nvm_recalculate_checksum
 * @hw: pointer to the HW struct
 *
 * Recalculate NVM PFA checksum (0x0706)
 */
int ice_nvm_recalculate_checksum(struct ice_hw *hw)
{
	struct ice_aqc_nvm_checksum *cmd;
	struct ice_aq_desc desc;
	int status;

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	cmd = &desc.params.nvm_checksum;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_checksum);
	cmd->flags = ICE_AQC_NVM_CHECKSUM_RECALC;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);

	ice_release_nvm(hw);

	return status;
}

/**
 * ice_nvm_write_activate
 * @hw: pointer to the HW struct
 * @cmd_flags: flags for write activate command
 * @response_flags: response indicators from firmware
 *
 * Update the control word with the required banks' validity bits
 * and dumps the Shadow RAM to flash (0x0707)
 *
 * cmd_flags controls which banks to activate, the preservation level to use
 * when activating the NVM bank, and whether an EMP reset is required for
 * activation.
 *
 * Note that the 16bit cmd_flags value is split between two separate 1 byte
 * flag values in the descriptor.
 *
 * On successful return of the firmware command, the response_flags variable
 * is updated with the flags reported by firmware indicating certain status,
 * such as whether EMP reset is enabled.
 */
int ice_nvm_write_activate(struct ice_hw *hw, u16 cmd_flags, u8 *response_flags)
{
	struct ice_aqc_nvm *cmd;
	struct ice_aq_desc desc;
	int err;

	cmd = &desc.params.nvm;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_write_activate);

	cmd->cmd_flags = (u8)(cmd_flags & 0xFF);
	cmd->offset_high = (u8)((cmd_flags >> 8) & 0xFF);

	err = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	if (!err && response_flags)
		*response_flags = cmd->cmd_flags;

	return err;
}

/**
 * ice_get_nvm_minsrevs - Get the Minimum Security Revision values from flash
 * @hw: pointer to the HW struct
 * @minsrevs: structure to store NVM and OROM minsrev values
 *
 * Read the Minimum Security Revision TLV and extract the revision values from
 * the flash image into a readable structure for processing.
 */
int
ice_get_nvm_minsrevs(struct ice_hw *hw, struct ice_minsrev_info *minsrevs)
{
	struct ice_aqc_nvm_minsrev data;
	int status;
	u16 valid;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	status = ice_aq_read_nvm(hw, ICE_AQC_NVM_MINSREV_MOD_ID, 0, sizeof(data),
				 &data, true, false, NULL);

	ice_release_nvm(hw);

	if (status)
		return status;

	valid = LE16_TO_CPU(data.validity);

	/* Extract NVM minimum security revision */
	if (valid & ICE_AQC_NVM_MINSREV_NVM_VALID) {
		u16 minsrev_l, minsrev_h;

		minsrev_l = LE16_TO_CPU(data.nvm_minsrev_l);
		minsrev_h = LE16_TO_CPU(data.nvm_minsrev_h);

		minsrevs->nvm = minsrev_h << 16 | minsrev_l;
		minsrevs->nvm_valid = true;
	}

	/* Extract the OROM minimum security revision */
	if (valid & ICE_AQC_NVM_MINSREV_OROM_VALID) {
		u16 minsrev_l, minsrev_h;

		minsrev_l = LE16_TO_CPU(data.orom_minsrev_l);
		minsrev_h = LE16_TO_CPU(data.orom_minsrev_h);

		minsrevs->orom = minsrev_h << 16 | minsrev_l;
		minsrevs->orom_valid = true;
	}

	return 0;
}

/**
 * ice_update_nvm_minsrevs - Update minimum security revision TLV data in flash
 * @hw: pointer to the HW struct
 * @minsrevs: minimum security revision information
 *
 * Update the NVM or Option ROM minimum security revision fields in the PFA
 * area of the flash. Reads the minsrevs->nvm_valid and minsrevs->orom_valid
 * fields to determine what update is being requested. If the valid bit is not
 * set for that module, then the associated minsrev will be left as is.
 */
int
ice_update_nvm_minsrevs(struct ice_hw *hw, struct ice_minsrev_info *minsrevs)
{
	struct ice_aqc_nvm_minsrev data;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	if (!minsrevs->nvm_valid && !minsrevs->orom_valid) {
		ice_debug(hw, ICE_DBG_NVM, "At least one of NVM and OROM MinSrev must be valid");
		return ICE_ERR_PARAM;
	}

	status = ice_acquire_nvm(hw, ICE_RES_WRITE);
	if (status)
		return status;

	/* Get current data */
	status = ice_aq_read_nvm(hw, ICE_AQC_NVM_MINSREV_MOD_ID, 0, sizeof(data),
				 &data, true, false, NULL);
	if (status)
		goto exit_release_res;

	if (minsrevs->nvm_valid) {
		data.nvm_minsrev_l = CPU_TO_LE16(minsrevs->nvm & 0xFFFF);
		data.nvm_minsrev_h = CPU_TO_LE16(minsrevs->nvm >> 16);
		data.validity |= CPU_TO_LE16(ICE_AQC_NVM_MINSREV_NVM_VALID);
	}

	if (minsrevs->orom_valid) {
		data.orom_minsrev_l = CPU_TO_LE16(minsrevs->orom & 0xFFFF);
		data.orom_minsrev_h = CPU_TO_LE16(minsrevs->orom >> 16);
		data.validity |= CPU_TO_LE16(ICE_AQC_NVM_MINSREV_OROM_VALID);
	}

	/* Update flash data */
	status = ice_aq_update_nvm(hw, ICE_AQC_NVM_MINSREV_MOD_ID, 0, sizeof(data), &data,
				   false, ICE_AQC_NVM_SPECIAL_UPDATE, NULL);
	if (status)
		goto exit_release_res;

	/* Dump the Shadow RAM to the flash */
	status = ice_nvm_write_activate(hw, 0, NULL);

exit_release_res:
	ice_release_nvm(hw);

	return status;
}

/**
 * ice_nvm_access_get_features - Return the NVM access features structure
 * @cmd: NVM access command to process
 * @data: storage for the driver NVM features
 *
 * Fill in the data section of the NVM access request with a copy of the NVM
 * features structure.
 */
int
ice_nvm_access_get_features(struct ice_nvm_access_cmd *cmd,
			    union ice_nvm_access_data *data)
{
	/* The provided data_size must be at least as large as our NVM
	 * features structure. A larger size should not be treated as an
	 * error, to allow future extensions to the features structure to
	 * work on older drivers.
	 */
	if (cmd->data_size < sizeof(struct ice_nvm_features))
		return ICE_ERR_NO_MEMORY;

	/* Initialize the data buffer to zeros */
	ice_memset(data, 0, cmd->data_size, ICE_NONDMA_MEM);

	/* Fill in the features data */
	data->drv_features.major = ICE_NVM_ACCESS_MAJOR_VER;
	data->drv_features.minor = ICE_NVM_ACCESS_MINOR_VER;
	data->drv_features.size = sizeof(struct ice_nvm_features);
	data->drv_features.features[0] = ICE_NVM_FEATURES_0_REG_ACCESS;

	return 0;
}

/**
 * ice_nvm_access_get_module - Helper function to read module value
 * @cmd: NVM access command structure
 *
 * Reads the module value out of the NVM access config field.
 */
u32 ice_nvm_access_get_module(struct ice_nvm_access_cmd *cmd)
{
	return ((cmd->config & ICE_NVM_CFG_MODULE_M) >> ICE_NVM_CFG_MODULE_S);
}

/**
 * ice_nvm_access_get_flags - Helper function to read flags value
 * @cmd: NVM access command structure
 *
 * Reads the flags value out of the NVM access config field.
 */
u32 ice_nvm_access_get_flags(struct ice_nvm_access_cmd *cmd)
{
	return ((cmd->config & ICE_NVM_CFG_FLAGS_M) >> ICE_NVM_CFG_FLAGS_S);
}

/**
 * ice_nvm_access_get_adapter - Helper function to read adapter info
 * @cmd: NVM access command structure
 *
 * Read the adapter info value out of the NVM access config field.
 */
u32 ice_nvm_access_get_adapter(struct ice_nvm_access_cmd *cmd)
{
	return ((cmd->config & ICE_NVM_CFG_ADAPTER_INFO_M) >>
		ICE_NVM_CFG_ADAPTER_INFO_S);
}

/**
 * ice_validate_nvm_rw_reg - Check than an NVM access request is valid
 * @cmd: NVM access command structure
 *
 * Validates that an NVM access structure is request to read or write a valid
 * register offset. First validates that the module and flags are correct, and
 * then ensures that the register offset is one of the accepted registers.
 */
static int
ice_validate_nvm_rw_reg(struct ice_nvm_access_cmd *cmd)
{
	u32 module, flags, offset;
	u16 i;

	module = ice_nvm_access_get_module(cmd);
	flags = ice_nvm_access_get_flags(cmd);
	offset = cmd->offset;

	/* Make sure the module and flags indicate a read/write request */
	if (module != ICE_NVM_REG_RW_MODULE ||
	    flags != ICE_NVM_REG_RW_FLAGS ||
	    cmd->data_size != FIELD_SIZEOF(union ice_nvm_access_data, regval))
		return ICE_ERR_PARAM;

	switch (offset) {
	case GL_HICR:
	case GL_HICR_EN: /* Note, this register is read only */
	case GL_FWSTS:
	case GL_MNG_FWSM:
	case GLGEN_CSR_DEBUG_C:
	case GLGEN_RSTAT:
	case GLPCI_LBARCTRL:
	case GL_MNG_DEF_DEVID:
	case GLNVM_GENS:
	case GLNVM_FLA:
	case PF_FUNC_RID:
		return 0;
	default:
		break;
	}

	for (i = 0; i <= GL_HIDA_MAX_INDEX; i++)
		if (offset == (u32)GL_HIDA(i))
			return 0;

	for (i = 0; i <= GL_HIBA_MAX_INDEX; i++)
		if (offset == (u32)GL_HIBA(i))
			return 0;

	/* All other register offsets are not valid */
	return ICE_ERR_OUT_OF_RANGE;
}

/**
 * ice_nvm_access_read - Handle an NVM read request
 * @hw: pointer to the HW struct
 * @cmd: NVM access command to process
 * @data: storage for the register value read
 *
 * Process an NVM access request to read a register.
 */
int
ice_nvm_access_read(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		    union ice_nvm_access_data *data)
{
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* Always initialize the output data, even on failure */
	ice_memset(data, 0, cmd->data_size, ICE_NONDMA_MEM);

	/* Make sure this is a valid read/write access request */
	status = ice_validate_nvm_rw_reg(cmd);
	if (status)
		return status;

	ice_debug(hw, ICE_DBG_NVM, "NVM access: reading register %08x\n",
		  cmd->offset);

	/* Read the register and store the contents in the data field */
	data->regval = rd32(hw, cmd->offset);

	return 0;
}

/**
 * ice_nvm_access_write - Handle an NVM write request
 * @hw: pointer to the HW struct
 * @cmd: NVM access command to process
 * @data: NVM access data to write
 *
 * Process an NVM access request to write a register.
 */
int
ice_nvm_access_write(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		     union ice_nvm_access_data *data)
{
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* Make sure this is a valid read/write access request */
	status = ice_validate_nvm_rw_reg(cmd);
	if (status)
		return status;

	/* Reject requests to write to read-only registers */
	if (hw->mac_type == ICE_MAC_E830) {
		if (cmd->offset == E830_GL_HICR_EN)
			return ICE_ERR_OUT_OF_RANGE;
	} else {
		if (cmd->offset == GL_HICR_EN)
			return ICE_ERR_OUT_OF_RANGE;
	}

	if (cmd->offset == GLGEN_RSTAT)
		return ICE_ERR_OUT_OF_RANGE;

	ice_debug(hw, ICE_DBG_NVM, "NVM access: writing register %08x with value %08x\n",
		  cmd->offset, data->regval);

	/* Write the data field to the specified register */
	wr32(hw, cmd->offset, data->regval);

	return 0;
}

/**
 * ice_handle_nvm_access - Handle an NVM access request
 * @hw: pointer to the HW struct
 * @cmd: NVM access command info
 * @data: pointer to read or return data
 *
 * Process an NVM access request. Read the command structure information and
 * determine if it is valid. If not, report an error indicating the command
 * was invalid.
 *
 * For valid commands, perform the necessary function, copying the data into
 * the provided data buffer.
 */
int
ice_handle_nvm_access(struct ice_hw *hw, struct ice_nvm_access_cmd *cmd,
		      union ice_nvm_access_data *data)
{
	u32 module, flags, adapter_info;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* Extended flags are currently reserved and must be zero */
	if ((cmd->config & ICE_NVM_CFG_EXT_FLAGS_M) != 0)
		return ICE_ERR_PARAM;

	/* Adapter info must match the HW device ID */
	adapter_info = ice_nvm_access_get_adapter(cmd);
	if (adapter_info != hw->device_id)
		return ICE_ERR_PARAM;

	switch (cmd->command) {
	case ICE_NVM_CMD_READ:
		module = ice_nvm_access_get_module(cmd);
		flags = ice_nvm_access_get_flags(cmd);

		/* Getting the driver's NVM features structure shares the same
		 * command type as reading a register. Read the config field
		 * to determine if this is a request to get features.
		 */
		if (module == ICE_NVM_GET_FEATURES_MODULE &&
		    flags == ICE_NVM_GET_FEATURES_FLAGS &&
		    cmd->offset == 0)
			return ice_nvm_access_get_features(cmd, data);
		else
			return ice_nvm_access_read(hw, cmd, data);
	case ICE_NVM_CMD_WRITE:
		return ice_nvm_access_write(hw, cmd, data);
	default:
		return ICE_ERR_PARAM;
	}
}

/**
 * ice_nvm_sanitize_operate - Clear the user data
 * @hw: pointer to the HW struct
 *
 * Clear user data from NVM using AQ command (0x070C).
 *
 * Return: the exit code of the operation.
 */
s32 ice_nvm_sanitize_operate(struct ice_hw *hw)
{
	s32 status;
	u8 values;

	u8 cmd_flags = ICE_AQ_NVM_SANITIZE_REQ_OPERATE |
		       ICE_AQ_NVM_SANITIZE_OPERATE_SUBJECT_CLEAR;

	status = ice_nvm_sanitize(hw, cmd_flags, &values);
	if (status)
		return status;
	if ((!(values & ICE_AQ_NVM_SANITIZE_OPERATE_HOST_CLEAN_DONE) &&
	     !(values & ICE_AQ_NVM_SANITIZE_OPERATE_BMC_CLEAN_DONE)) ||
	    ((values & ICE_AQ_NVM_SANITIZE_OPERATE_HOST_CLEAN_DONE) &&
	     !(values & ICE_AQ_NVM_SANITIZE_OPERATE_HOST_CLEAN_SUCCESS)) ||
	    ((values & ICE_AQ_NVM_SANITIZE_OPERATE_BMC_CLEAN_DONE) &&
	     !(values & ICE_AQ_NVM_SANITIZE_OPERATE_BMC_CLEAN_SUCCESS)))
		return ICE_ERR_AQ_ERROR;

	return ICE_SUCCESS;
}

/**
 * ice_nvm_sanitize - Sanitize NVM
 * @hw: pointer to the HW struct
 * @cmd_flags: flag to the ACI command
 * @values: values returned from the command
 *
 * Sanitize NVM using AQ command (0x070C).
 *
 * Return: the exit code of the operation.
 */
s32 ice_nvm_sanitize(struct ice_hw *hw, u8 cmd_flags, u8 *values)
{
	struct ice_aqc_nvm_sanitization *cmd;
	struct ice_aq_desc desc;
	s32 status;

	cmd = &desc.params.sanitization;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_sanitization);
	cmd->cmd_flags = cmd_flags;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	if (values)
		*values = cmd->values;

	return status;
}
