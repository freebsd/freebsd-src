/******************************************************************************

  Copyright (c) 2001-2015, Intel Corporation 
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
/*$FreeBSD$*/

#include "e1000_api.h"


static s32 e1000_acquire_nvm_i210(struct e1000_hw *hw);
static void e1000_release_nvm_i210(struct e1000_hw *hw);
static s32 e1000_get_hw_semaphore_i210(struct e1000_hw *hw);
static s32 e1000_write_nvm_srwr(struct e1000_hw *hw, u16 offset, u16 words,
				u16 *data);
static s32 e1000_pool_flash_update_done_i210(struct e1000_hw *hw);
static s32 e1000_valid_led_default_i210(struct e1000_hw *hw, u16 *data);

/**
 *  e1000_acquire_nvm_i210 - Request for access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Acquire the necessary semaphores for exclusive access to the EEPROM.
 *  Set the EEPROM access request bit and wait for EEPROM access grant bit.
 *  Return successful if access grant bit set, else clear the request for
 *  EEPROM access and return -E1000_ERR_NVM (-1).
 **/
static s32 e1000_acquire_nvm_i210(struct e1000_hw *hw)
{
	s32 ret_val;

	DEBUGFUNC("e1000_acquire_nvm_i210");

	ret_val = e1000_acquire_swfw_sync_i210(hw, E1000_SWFW_EEP_SM);

	return ret_val;
}

/**
 *  e1000_release_nvm_i210 - Release exclusive access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Stop any current commands to the EEPROM and clear the EEPROM request bit,
 *  then release the semaphores acquired.
 **/
static void e1000_release_nvm_i210(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_release_nvm_i210");

	e1000_release_swfw_sync_i210(hw, E1000_SWFW_EEP_SM);
}

/**
 *  e1000_acquire_swfw_sync_i210 - Acquire SW/FW semaphore
 *  @hw: pointer to the HW structure
 *  @mask: specifies which semaphore to acquire
 *
 *  Acquire the SW/FW semaphore to access the PHY or NVM.  The mask
 *  will also specify which port we're acquiring the lock for.
 **/
s32 e1000_acquire_swfw_sync_i210(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;
	u32 fwmask = mask << 16;
	s32 ret_val = E1000_SUCCESS;
	s32 i = 0, timeout = 200; /* FIXME: find real value to use here */

	DEBUGFUNC("e1000_acquire_swfw_sync_i210");

	while (i < timeout) {
		if (e1000_get_hw_semaphore_i210(hw)) {
			ret_val = -E1000_ERR_SWFW_SYNC;
			goto out;
		}

		swfw_sync = E1000_READ_REG(hw, E1000_SW_FW_SYNC);
		if (!(swfw_sync & (fwmask | swmask)))
			break;

		/*
		 * Firmware currently using resource (fwmask)
		 * or other software thread using resource (swmask)
		 */
		e1000_put_hw_semaphore_generic(hw);
		msec_delay_irq(5);
		i++;
	}

	if (i == timeout) {
		DEBUGOUT("Driver can't access resource, SW_FW_SYNC timeout.\n");
		ret_val = -E1000_ERR_SWFW_SYNC;
		goto out;
	}

	swfw_sync |= swmask;
	E1000_WRITE_REG(hw, E1000_SW_FW_SYNC, swfw_sync);

	e1000_put_hw_semaphore_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_release_swfw_sync_i210 - Release SW/FW semaphore
 *  @hw: pointer to the HW structure
 *  @mask: specifies which semaphore to acquire
 *
 *  Release the SW/FW semaphore used to access the PHY or NVM.  The mask
 *  will also specify which port we're releasing the lock for.
 **/
void e1000_release_swfw_sync_i210(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;

	DEBUGFUNC("e1000_release_swfw_sync_i210");

	while (e1000_get_hw_semaphore_i210(hw) != E1000_SUCCESS)
		; /* Empty */

	swfw_sync = E1000_READ_REG(hw, E1000_SW_FW_SYNC);
	swfw_sync &= ~mask;
	E1000_WRITE_REG(hw, E1000_SW_FW_SYNC, swfw_sync);

	e1000_put_hw_semaphore_generic(hw);
}

/**
 *  e1000_get_hw_semaphore_i210 - Acquire hardware semaphore
 *  @hw: pointer to the HW structure
 *
 *  Acquire the HW semaphore to access the PHY or NVM
 **/
static s32 e1000_get_hw_semaphore_i210(struct e1000_hw *hw)
{
	u32 swsm;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	DEBUGFUNC("e1000_get_hw_semaphore_i210");

	/* Get the SW semaphore */
	while (i < timeout) {
		swsm = E1000_READ_REG(hw, E1000_SWSM);
		if (!(swsm & E1000_SWSM_SMBI))
			break;

		usec_delay(50);
		i++;
	}

	if (i == timeout) {
		/* In rare circumstances, the SW semaphore may already be held
		 * unintentionally. Clear the semaphore once before giving up.
		 */
		if (hw->dev_spec._82575.clear_semaphore_once) {
			hw->dev_spec._82575.clear_semaphore_once = FALSE;
			e1000_put_hw_semaphore_generic(hw);
			for (i = 0; i < timeout; i++) {
				swsm = E1000_READ_REG(hw, E1000_SWSM);
				if (!(swsm & E1000_SWSM_SMBI))
					break;

				usec_delay(50);
			}
		}

		/* If we do not have the semaphore here, we have to give up. */
		if (i == timeout) {
			DEBUGOUT("Driver can't access device - SMBI bit is set.\n");
			return -E1000_ERR_NVM;
		}
	}

	/* Get the FW semaphore. */
	for (i = 0; i < timeout; i++) {
		swsm = E1000_READ_REG(hw, E1000_SWSM);
		E1000_WRITE_REG(hw, E1000_SWSM, swsm | E1000_SWSM_SWESMBI);

		/* Semaphore acquired if bit latched */
		if (E1000_READ_REG(hw, E1000_SWSM) & E1000_SWSM_SWESMBI)
			break;

		usec_delay(50);
	}

	if (i == timeout) {
		/* Release semaphores */
		e1000_put_hw_semaphore_generic(hw);
		DEBUGOUT("Driver can't access the NVM\n");
		return -E1000_ERR_NVM;
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_read_nvm_srrd_i210 - Reads Shadow Ram using EERD register
 *  @hw: pointer to the HW structure
 *  @offset: offset of word in the Shadow Ram to read
 *  @words: number of words to read
 *  @data: word read from the Shadow Ram
 *
 *  Reads a 16 bit word from the Shadow Ram using the EERD register.
 *  Uses necessary synchronization semaphores.
 **/
s32 e1000_read_nvm_srrd_i210(struct e1000_hw *hw, u16 offset, u16 words,
			     u16 *data)
{
	s32 status = E1000_SUCCESS;
	u16 i, count;

	DEBUGFUNC("e1000_read_nvm_srrd_i210");

	/* We cannot hold synchronization semaphores for too long,
	 * because of forceful takeover procedure. However it is more efficient
	 * to read in bursts than synchronizing access for each word. */
	for (i = 0; i < words; i += E1000_EERD_EEWR_MAX_COUNT) {
		count = (words - i) / E1000_EERD_EEWR_MAX_COUNT > 0 ?
			E1000_EERD_EEWR_MAX_COUNT : (words - i);
		if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {
			status = e1000_read_nvm_eerd(hw, offset, count,
						     data + i);
			hw->nvm.ops.release(hw);
		} else {
			status = E1000_ERR_SWFW_SYNC;
		}

		if (status != E1000_SUCCESS)
			break;
	}

	return status;
}

/**
 *  e1000_write_nvm_srwr_i210 - Write to Shadow RAM using EEWR
 *  @hw: pointer to the HW structure
 *  @offset: offset within the Shadow RAM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the Shadow RAM
 *
 *  Writes data to Shadow RAM at offset using EEWR register.
 *
 *  If e1000_update_nvm_checksum is not called after this function , the
 *  data will not be committed to FLASH and also Shadow RAM will most likely
 *  contain an invalid checksum.
 *
 *  If error code is returned, data and Shadow RAM may be inconsistent - buffer
 *  partially written.
 **/
s32 e1000_write_nvm_srwr_i210(struct e1000_hw *hw, u16 offset, u16 words,
			      u16 *data)
{
	s32 status = E1000_SUCCESS;
	u16 i, count;

	DEBUGFUNC("e1000_write_nvm_srwr_i210");

	/* We cannot hold synchronization semaphores for too long,
	 * because of forceful takeover procedure. However it is more efficient
	 * to write in bursts than synchronizing access for each word. */
	for (i = 0; i < words; i += E1000_EERD_EEWR_MAX_COUNT) {
		count = (words - i) / E1000_EERD_EEWR_MAX_COUNT > 0 ?
			E1000_EERD_EEWR_MAX_COUNT : (words - i);
		if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {
			status = e1000_write_nvm_srwr(hw, offset, count,
						      data + i);
			hw->nvm.ops.release(hw);
		} else {
			status = E1000_ERR_SWFW_SYNC;
		}

		if (status != E1000_SUCCESS)
			break;
	}

	return status;
}

/**
 *  e1000_write_nvm_srwr - Write to Shadow Ram using EEWR
 *  @hw: pointer to the HW structure
 *  @offset: offset within the Shadow Ram to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the Shadow Ram
 *
 *  Writes data to Shadow Ram at offset using EEWR register.
 *
 *  If e1000_update_nvm_checksum is not called after this function , the
 *  Shadow Ram will most likely contain an invalid checksum.
 **/
static s32 e1000_write_nvm_srwr(struct e1000_hw *hw, u16 offset, u16 words,
				u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i, k, eewr = 0;
	u32 attempts = 100000;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_write_nvm_srwr");

	/*
	 * A check for invalid values:  offset too large, too many words,
	 * too many words for the offset, and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	for (i = 0; i < words; i++) {
		eewr = ((offset+i) << E1000_NVM_RW_ADDR_SHIFT) |
			(data[i] << E1000_NVM_RW_REG_DATA) |
			E1000_NVM_RW_REG_START;

		E1000_WRITE_REG(hw, E1000_SRWR, eewr);

		for (k = 0; k < attempts; k++) {
			if (E1000_NVM_RW_REG_DONE &
			    E1000_READ_REG(hw, E1000_SRWR)) {
				ret_val = E1000_SUCCESS;
				break;
			}
			usec_delay(5);
		}

		if (ret_val != E1000_SUCCESS) {
			DEBUGOUT("Shadow RAM write EEWR timed out\n");
			break;
		}
	}

out:
	return ret_val;
}

/** e1000_read_invm_word_i210 - Reads OTP
 *  @hw: pointer to the HW structure
 *  @address: the word address (aka eeprom offset) to read
 *  @data: pointer to the data read
 *
 *  Reads 16-bit words from the OTP. Return error when the word is not
 *  stored in OTP.
 **/
static s32 e1000_read_invm_word_i210(struct e1000_hw *hw, u8 address, u16 *data)
{
	s32 status = -E1000_ERR_INVM_VALUE_NOT_FOUND;
	u32 invm_dword;
	u16 i;
	u8 record_type, word_address;

	DEBUGFUNC("e1000_read_invm_word_i210");

	for (i = 0; i < E1000_INVM_SIZE; i++) {
		invm_dword = E1000_READ_REG(hw, E1000_INVM_DATA_REG(i));
		/* Get record type */
		record_type = INVM_DWORD_TO_RECORD_TYPE(invm_dword);
		if (record_type == E1000_INVM_UNINITIALIZED_STRUCTURE)
			break;
		if (record_type == E1000_INVM_CSR_AUTOLOAD_STRUCTURE)
			i += E1000_INVM_CSR_AUTOLOAD_DATA_SIZE_IN_DWORDS;
		if (record_type == E1000_INVM_RSA_KEY_SHA256_STRUCTURE)
			i += E1000_INVM_RSA_KEY_SHA256_DATA_SIZE_IN_DWORDS;
		if (record_type == E1000_INVM_WORD_AUTOLOAD_STRUCTURE) {
			word_address = INVM_DWORD_TO_WORD_ADDRESS(invm_dword);
			if (word_address == address) {
				*data = INVM_DWORD_TO_WORD_DATA(invm_dword);
				DEBUGOUT2("Read INVM Word 0x%02x = %x",
					  address, *data);
				status = E1000_SUCCESS;
				break;
			}
		}
	}
	if (status != E1000_SUCCESS)
		DEBUGOUT1("Requested word 0x%02x not found in OTP\n", address);
	return status;
}

/** e1000_read_invm_i210 - Read invm wrapper function for I210/I211
 *  @hw: pointer to the HW structure
 *  @address: the word address (aka eeprom offset) to read
 *  @data: pointer to the data read
 *
 *  Wrapper function to return data formerly found in the NVM.
 **/
static s32 e1000_read_invm_i210(struct e1000_hw *hw, u16 offset,
				u16 E1000_UNUSEDARG words, u16 *data)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_read_invm_i210");

	/* Only the MAC addr is required to be present in the iNVM */
	switch (offset) {
	case NVM_MAC_ADDR:
		ret_val = e1000_read_invm_word_i210(hw, (u8)offset, &data[0]);
		ret_val |= e1000_read_invm_word_i210(hw, (u8)offset+1,
						     &data[1]);
		ret_val |= e1000_read_invm_word_i210(hw, (u8)offset+2,
						     &data[2]);
		if (ret_val != E1000_SUCCESS)
			DEBUGOUT("MAC Addr not found in iNVM\n");
		break;
	case NVM_INIT_CTRL_2:
		ret_val = e1000_read_invm_word_i210(hw, (u8)offset, data);
		if (ret_val != E1000_SUCCESS) {
			*data = NVM_INIT_CTRL_2_DEFAULT_I211;
			ret_val = E1000_SUCCESS;
		}
		break;
	case NVM_INIT_CTRL_4:
		ret_val = e1000_read_invm_word_i210(hw, (u8)offset, data);
		if (ret_val != E1000_SUCCESS) {
			*data = NVM_INIT_CTRL_4_DEFAULT_I211;
			ret_val = E1000_SUCCESS;
		}
		break;
	case NVM_LED_1_CFG:
		ret_val = e1000_read_invm_word_i210(hw, (u8)offset, data);
		if (ret_val != E1000_SUCCESS) {
			*data = NVM_LED_1_CFG_DEFAULT_I211;
			ret_val = E1000_SUCCESS;
		}
		break;
	case NVM_LED_0_2_CFG:
		ret_val = e1000_read_invm_word_i210(hw, (u8)offset, data);
		if (ret_val != E1000_SUCCESS) {
			*data = NVM_LED_0_2_CFG_DEFAULT_I211;
			ret_val = E1000_SUCCESS;
		}
		break;
	case NVM_ID_LED_SETTINGS:
		ret_val = e1000_read_invm_word_i210(hw, (u8)offset, data);
		if (ret_val != E1000_SUCCESS) {
			*data = ID_LED_RESERVED_FFFF;
			ret_val = E1000_SUCCESS;
		}
		break;
	case NVM_SUB_DEV_ID:
		*data = hw->subsystem_device_id;
		break;
	case NVM_SUB_VEN_ID:
		*data = hw->subsystem_vendor_id;
		break;
	case NVM_DEV_ID:
		*data = hw->device_id;
		break;
	case NVM_VEN_ID:
		*data = hw->vendor_id;
		break;
	default:
		DEBUGOUT1("NVM word 0x%02x is not mapped.\n", offset);
		*data = NVM_RESERVED_WORD;
		break;
	}
	return ret_val;
}

/**
 *  e1000_read_invm_version - Reads iNVM version and image type
 *  @hw: pointer to the HW structure
 *  @invm_ver: version structure for the version read
 *
 *  Reads iNVM version and image type.
 **/
s32 e1000_read_invm_version(struct e1000_hw *hw,
			    struct e1000_fw_version *invm_ver)
{
	u32 *record = NULL;
	u32 *next_record = NULL;
	u32 i = 0;
	u32 invm_dword = 0;
	u32 invm_blocks = E1000_INVM_SIZE - (E1000_INVM_ULT_BYTES_SIZE /
					     E1000_INVM_RECORD_SIZE_IN_BYTES);
	u32 buffer[E1000_INVM_SIZE];
	s32 status = -E1000_ERR_INVM_VALUE_NOT_FOUND;
	u16 version = 0;

	DEBUGFUNC("e1000_read_invm_version");

	/* Read iNVM memory */
	for (i = 0; i < E1000_INVM_SIZE; i++) {
		invm_dword = E1000_READ_REG(hw, E1000_INVM_DATA_REG(i));
		buffer[i] = invm_dword;
	}

	/* Read version number */
	for (i = 1; i < invm_blocks; i++) {
		record = &buffer[invm_blocks - i];
		next_record = &buffer[invm_blocks - i + 1];

		/* Check if we have first version location used */
		if ((i == 1) && ((*record & E1000_INVM_VER_FIELD_ONE) == 0)) {
			version = 0;
			status = E1000_SUCCESS;
			break;
		}
		/* Check if we have second version location used */
		else if ((i == 1) &&
			 ((*record & E1000_INVM_VER_FIELD_TWO) == 0)) {
			version = (*record & E1000_INVM_VER_FIELD_ONE) >> 3;
			status = E1000_SUCCESS;
			break;
		}
		/*
		 * Check if we have odd version location
		 * used and it is the last one used
		 */
		else if ((((*record & E1000_INVM_VER_FIELD_ONE) == 0) &&
			 ((*record & 0x3) == 0)) || (((*record & 0x3) != 0) &&
			 (i != 1))) {
			version = (*next_record & E1000_INVM_VER_FIELD_TWO)
				  >> 13;
			status = E1000_SUCCESS;
			break;
		}
		/*
		 * Check if we have even version location
		 * used and it is the last one used
		 */
		else if (((*record & E1000_INVM_VER_FIELD_TWO) == 0) &&
			 ((*record & 0x3) == 0)) {
			version = (*record & E1000_INVM_VER_FIELD_ONE) >> 3;
			status = E1000_SUCCESS;
			break;
		}
	}

	if (status == E1000_SUCCESS) {
		invm_ver->invm_major = (version & E1000_INVM_MAJOR_MASK)
					>> E1000_INVM_MAJOR_SHIFT;
		invm_ver->invm_minor = version & E1000_INVM_MINOR_MASK;
	}
	/* Read Image Type */
	for (i = 1; i < invm_blocks; i++) {
		record = &buffer[invm_blocks - i];
		next_record = &buffer[invm_blocks - i + 1];

		/* Check if we have image type in first location used */
		if ((i == 1) && ((*record & E1000_INVM_IMGTYPE_FIELD) == 0)) {
			invm_ver->invm_img_type = 0;
			status = E1000_SUCCESS;
			break;
		}
		/* Check if we have image type in first location used */
		else if ((((*record & 0x3) == 0) &&
			 ((*record & E1000_INVM_IMGTYPE_FIELD) == 0)) ||
			 ((((*record & 0x3) != 0) && (i != 1)))) {
			invm_ver->invm_img_type =
				(*next_record & E1000_INVM_IMGTYPE_FIELD) >> 23;
			status = E1000_SUCCESS;
			break;
		}
	}
	return status;
}

/**
 *  e1000_validate_nvm_checksum_i210 - Validate EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Calculates the EEPROM checksum by reading/adding each word of the EEPROM
 *  and then verifies that the sum of the EEPROM is equal to 0xBABA.
 **/
s32 e1000_validate_nvm_checksum_i210(struct e1000_hw *hw)
{
	s32 status = E1000_SUCCESS;
	s32 (*read_op_ptr)(struct e1000_hw *, u16, u16, u16 *);

	DEBUGFUNC("e1000_validate_nvm_checksum_i210");

	if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {

		/*
		 * Replace the read function with semaphore grabbing with
		 * the one that skips this for a while.
		 * We have semaphore taken already here.
		 */
		read_op_ptr = hw->nvm.ops.read;
		hw->nvm.ops.read = e1000_read_nvm_eerd;

		status = e1000_validate_nvm_checksum_generic(hw);

		/* Revert original read operation. */
		hw->nvm.ops.read = read_op_ptr;

		hw->nvm.ops.release(hw);
	} else {
		status = E1000_ERR_SWFW_SYNC;
	}

	return status;
}


/**
 *  e1000_update_nvm_checksum_i210 - Update EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Updates the EEPROM checksum by reading/adding each word of the EEPROM
 *  up to the checksum.  Then calculates the EEPROM checksum and writes the
 *  value to the EEPROM. Next commit EEPROM data onto the Flash.
 **/
s32 e1000_update_nvm_checksum_i210(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	DEBUGFUNC("e1000_update_nvm_checksum_i210");

	/*
	 * Read the first word from the EEPROM. If this times out or fails, do
	 * not continue or we could be in for a very long wait while every
	 * EEPROM read fails
	 */
	ret_val = e1000_read_nvm_eerd(hw, 0, 1, &nvm_data);
	if (ret_val != E1000_SUCCESS) {
		DEBUGOUT("EEPROM read failed\n");
		goto out;
	}

	if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {
		/*
		 * Do not use hw->nvm.ops.write, hw->nvm.ops.read
		 * because we do not want to take the synchronization
		 * semaphores twice here.
		 */

		for (i = 0; i < NVM_CHECKSUM_REG; i++) {
			ret_val = e1000_read_nvm_eerd(hw, i, 1, &nvm_data);
			if (ret_val) {
				hw->nvm.ops.release(hw);
				DEBUGOUT("NVM Read Error while updating checksum.\n");
				goto out;
			}
			checksum += nvm_data;
		}
		checksum = (u16) NVM_SUM - checksum;
		ret_val = e1000_write_nvm_srwr(hw, NVM_CHECKSUM_REG, 1,
						&checksum);
		if (ret_val != E1000_SUCCESS) {
			hw->nvm.ops.release(hw);
			DEBUGOUT("NVM Write Error while updating checksum.\n");
			goto out;
		}

		hw->nvm.ops.release(hw);

		ret_val = e1000_update_flash_i210(hw);
	} else {
		ret_val = E1000_ERR_SWFW_SYNC;
	}
out:
	return ret_val;
}

/**
 *  e1000_get_flash_presence_i210 - Check if flash device is detected.
 *  @hw: pointer to the HW structure
 *
 **/
bool e1000_get_flash_presence_i210(struct e1000_hw *hw)
{
	u32 eec = 0;
	bool ret_val = FALSE;

	DEBUGFUNC("e1000_get_flash_presence_i210");

	eec = E1000_READ_REG(hw, E1000_EECD);

	if (eec & E1000_EECD_FLASH_DETECTED_I210)
		ret_val = TRUE;

	return ret_val;
}

/**
 *  e1000_update_flash_i210 - Commit EEPROM to the flash
 *  @hw: pointer to the HW structure
 *
 **/
s32 e1000_update_flash_i210(struct e1000_hw *hw)
{
	s32 ret_val;
	u32 flup;

	DEBUGFUNC("e1000_update_flash_i210");

	ret_val = e1000_pool_flash_update_done_i210(hw);
	if (ret_val == -E1000_ERR_NVM) {
		DEBUGOUT("Flash update time out\n");
		goto out;
	}

	flup = E1000_READ_REG(hw, E1000_EECD) | E1000_EECD_FLUPD_I210;
	E1000_WRITE_REG(hw, E1000_EECD, flup);

	ret_val = e1000_pool_flash_update_done_i210(hw);
	if (ret_val == E1000_SUCCESS)
		DEBUGOUT("Flash update complete\n");
	else
		DEBUGOUT("Flash update time out\n");

out:
	return ret_val;
}

/**
 *  e1000_pool_flash_update_done_i210 - Pool FLUDONE status.
 *  @hw: pointer to the HW structure
 *
 **/
s32 e1000_pool_flash_update_done_i210(struct e1000_hw *hw)
{
	s32 ret_val = -E1000_ERR_NVM;
	u32 i, reg;

	DEBUGFUNC("e1000_pool_flash_update_done_i210");

	for (i = 0; i < E1000_FLUDONE_ATTEMPTS; i++) {
		reg = E1000_READ_REG(hw, E1000_EECD);
		if (reg & E1000_EECD_FLUDONE_I210) {
			ret_val = E1000_SUCCESS;
			break;
		}
		usec_delay(5);
	}

	return ret_val;
}

/**
 *  e1000_init_nvm_params_i210 - Initialize i210 NVM function pointers
 *  @hw: pointer to the HW structure
 *
 *  Initialize the i210/i211 NVM parameters and function pointers.
 **/
static s32 e1000_init_nvm_params_i210(struct e1000_hw *hw)
{
	s32 ret_val;
	struct e1000_nvm_info *nvm = &hw->nvm;

	DEBUGFUNC("e1000_init_nvm_params_i210");

	ret_val = e1000_init_nvm_params_82575(hw);
	nvm->ops.acquire = e1000_acquire_nvm_i210;
	nvm->ops.release = e1000_release_nvm_i210;
	nvm->ops.valid_led_default = e1000_valid_led_default_i210;
	if (e1000_get_flash_presence_i210(hw)) {
		hw->nvm.type = e1000_nvm_flash_hw;
		nvm->ops.read    = e1000_read_nvm_srrd_i210;
		nvm->ops.write   = e1000_write_nvm_srwr_i210;
		nvm->ops.validate = e1000_validate_nvm_checksum_i210;
		nvm->ops.update   = e1000_update_nvm_checksum_i210;
	} else {
		hw->nvm.type = e1000_nvm_invm;
		nvm->ops.read     = e1000_read_invm_i210;
		nvm->ops.write    = e1000_null_write_nvm;
		nvm->ops.validate = e1000_null_ops_generic;
		nvm->ops.update   = e1000_null_ops_generic;
	}
	return ret_val;
}

/**
 *  e1000_init_function_pointers_i210 - Init func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  Called to initialize all function pointers and parameters.
 **/
void e1000_init_function_pointers_i210(struct e1000_hw *hw)
{
	e1000_init_function_pointers_82575(hw);
	hw->nvm.ops.init_params = e1000_init_nvm_params_i210;

	return;
}

/**
 *  e1000_valid_led_default_i210 - Verify a valid default LED config
 *  @hw: pointer to the HW structure
 *  @data: pointer to the NVM (EEPROM)
 *
 *  Read the EEPROM for the current default LED configuration.  If the
 *  LED configuration is not valid, set to a valid LED configuration.
 **/
static s32 e1000_valid_led_default_i210(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	DEBUGFUNC("e1000_valid_led_default_i210");

	ret_val = hw->nvm.ops.read(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		goto out;
	}

	if (*data == ID_LED_RESERVED_0000 || *data == ID_LED_RESERVED_FFFF) {
		switch (hw->phy.media_type) {
		case e1000_media_type_internal_serdes:
			*data = ID_LED_DEFAULT_I210_SERDES;
			break;
		case e1000_media_type_copper:
		default:
			*data = ID_LED_DEFAULT_I210;
			break;
		}
	}
out:
	return ret_val;
}

/**
 *  __e1000_access_xmdio_reg - Read/write XMDIO register
 *  @hw: pointer to the HW structure
 *  @address: XMDIO address to program
 *  @dev_addr: device address to program
 *  @data: pointer to value to read/write from/to the XMDIO address
 *  @read: boolean flag to indicate read or write
 **/
static s32 __e1000_access_xmdio_reg(struct e1000_hw *hw, u16 address,
				    u8 dev_addr, u16 *data, bool read)
{
	s32 ret_val;

	DEBUGFUNC("__e1000_access_xmdio_reg");

	ret_val = hw->phy.ops.write_reg(hw, E1000_MMDAC, dev_addr);
	if (ret_val)
		return ret_val;

	ret_val = hw->phy.ops.write_reg(hw, E1000_MMDAAD, address);
	if (ret_val)
		return ret_val;

	ret_val = hw->phy.ops.write_reg(hw, E1000_MMDAC, E1000_MMDAC_FUNC_DATA |
							 dev_addr);
	if (ret_val)
		return ret_val;

	if (read)
		ret_val = hw->phy.ops.read_reg(hw, E1000_MMDAAD, data);
	else
		ret_val = hw->phy.ops.write_reg(hw, E1000_MMDAAD, *data);
	if (ret_val)
		return ret_val;

	/* Recalibrate the device back to 0 */
	ret_val = hw->phy.ops.write_reg(hw, E1000_MMDAC, 0);
	if (ret_val)
		return ret_val;

	return ret_val;
}

/**
 *  e1000_read_xmdio_reg - Read XMDIO register
 *  @hw: pointer to the HW structure
 *  @addr: XMDIO address to program
 *  @dev_addr: device address to program
 *  @data: value to be read from the EMI address
 **/
s32 e1000_read_xmdio_reg(struct e1000_hw *hw, u16 addr, u8 dev_addr, u16 *data)
{
	DEBUGFUNC("e1000_read_xmdio_reg");

	return __e1000_access_xmdio_reg(hw, addr, dev_addr, data, TRUE);
}

/**
 *  e1000_write_xmdio_reg - Write XMDIO register
 *  @hw: pointer to the HW structure
 *  @addr: XMDIO address to program
 *  @dev_addr: device address to program
 *  @data: value to be written to the XMDIO address
 **/
s32 e1000_write_xmdio_reg(struct e1000_hw *hw, u16 addr, u8 dev_addr, u16 data)
{
	DEBUGFUNC("e1000_read_xmdio_reg");

	return __e1000_access_xmdio_reg(hw, addr, dev_addr, &data, FALSE);
}

/**
 * e1000_pll_workaround_i210
 * @hw: pointer to the HW structure
 *
 * Works around an errata in the PLL circuit where it occasionally
 * provides the wrong clock frequency after power up.
 **/
static s32 e1000_pll_workaround_i210(struct e1000_hw *hw)
{
	s32 ret_val;
	u32 wuc, mdicnfg, ctrl, ctrl_ext, reg_val;
	u16 nvm_word, phy_word, pci_word, tmp_nvm;
	int i;

	/* Get and set needed register values */
	wuc = E1000_READ_REG(hw, E1000_WUC);
	mdicnfg = E1000_READ_REG(hw, E1000_MDICNFG);
	reg_val = mdicnfg & ~E1000_MDICNFG_EXT_MDIO;
	E1000_WRITE_REG(hw, E1000_MDICNFG, reg_val);

	/* Get data from NVM, or set default */
	ret_val = e1000_read_invm_word_i210(hw, E1000_INVM_AUTOLOAD,
					    &nvm_word);
	if (ret_val != E1000_SUCCESS)
		nvm_word = E1000_INVM_DEFAULT_AL;
	tmp_nvm = nvm_word | E1000_INVM_PLL_WO_VAL;
	for (i = 0; i < E1000_MAX_PLL_TRIES; i++) {
		/* check current state directly from internal PHY */
		e1000_read_phy_reg_gs40g(hw, (E1000_PHY_PLL_FREQ_PAGE |
					 E1000_PHY_PLL_FREQ_REG), &phy_word);
		if ((phy_word & E1000_PHY_PLL_UNCONF)
		    != E1000_PHY_PLL_UNCONF) {
			ret_val = E1000_SUCCESS;
			break;
		} else {
			ret_val = -E1000_ERR_PHY;
		}
		/* directly reset the internal PHY */
		ctrl = E1000_READ_REG(hw, E1000_CTRL);
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl|E1000_CTRL_PHY_RST);

		ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
		ctrl_ext |= (E1000_CTRL_EXT_PHYPDEN | E1000_CTRL_EXT_SDLPE);
		E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext);

		E1000_WRITE_REG(hw, E1000_WUC, 0);
		reg_val = (E1000_INVM_AUTOLOAD << 4) | (tmp_nvm << 16);
		E1000_WRITE_REG(hw, E1000_EEARBC_I210, reg_val);

		e1000_read_pci_cfg(hw, E1000_PCI_PMCSR, &pci_word);
		pci_word |= E1000_PCI_PMCSR_D3;
		e1000_write_pci_cfg(hw, E1000_PCI_PMCSR, &pci_word);
		msec_delay(1);
		pci_word &= ~E1000_PCI_PMCSR_D3;
		e1000_write_pci_cfg(hw, E1000_PCI_PMCSR, &pci_word);
		reg_val = (E1000_INVM_AUTOLOAD << 4) | (nvm_word << 16);
		E1000_WRITE_REG(hw, E1000_EEARBC_I210, reg_val);

		/* restore WUC register */
		E1000_WRITE_REG(hw, E1000_WUC, wuc);
	}
	/* restore MDICNFG setting */
	E1000_WRITE_REG(hw, E1000_MDICNFG, mdicnfg);
	return ret_val;
}

/**
 *  e1000_init_hw_i210 - Init hw for I210/I211
 *  @hw: pointer to the HW structure
 *
 *  Called to initialize hw for i210 hw family.
 **/
s32 e1000_init_hw_i210(struct e1000_hw *hw)
{
	s32 ret_val;

	DEBUGFUNC("e1000_init_hw_i210");
	if ((hw->mac.type >= e1000_i210) &&
	    !(e1000_get_flash_presence_i210(hw))) {
		ret_val = e1000_pll_workaround_i210(hw);
		if (ret_val != E1000_SUCCESS)
			return ret_val;
	}
	ret_val = e1000_init_hw_82575(hw);
	return ret_val;
}
