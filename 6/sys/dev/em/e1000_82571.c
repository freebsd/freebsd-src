/*******************************************************************************

  Copyright (c) 2001-2007, Intel Corporation 
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

*******************************************************************************/
/* $FreeBSD$ */


/* e1000_82571
 * e1000_82572
 * e1000_82573
 */

#include "e1000_api.h"
#include "e1000_82571.h"

void e1000_init_function_pointers_82571(struct e1000_hw *hw);

STATIC s32  e1000_init_phy_params_82571(struct e1000_hw *hw);
STATIC s32  e1000_init_nvm_params_82571(struct e1000_hw *hw);
STATIC s32  e1000_init_mac_params_82571(struct e1000_hw *hw);
STATIC s32  e1000_acquire_nvm_82571(struct e1000_hw *hw);
STATIC void e1000_release_nvm_82571(struct e1000_hw *hw);
STATIC s32  e1000_write_nvm_82571(struct e1000_hw *hw, u16 offset,
                                  u16 words, u16 *data);
STATIC s32  e1000_update_nvm_checksum_82571(struct e1000_hw *hw);
STATIC s32  e1000_validate_nvm_checksum_82571(struct e1000_hw *hw);
STATIC s32  e1000_get_cfg_done_82571(struct e1000_hw *hw);
STATIC s32  e1000_set_d0_lplu_state_82571(struct e1000_hw *hw,
                                          bool active);
STATIC s32  e1000_reset_hw_82571(struct e1000_hw *hw);
STATIC s32  e1000_init_hw_82571(struct e1000_hw *hw);
STATIC void e1000_clear_vfta_82571(struct e1000_hw *hw);
STATIC void e1000_update_mc_addr_list_82571(struct e1000_hw *hw,
                                            u8 *mc_addr_list, u32 mc_addr_count,
                                            u32 rar_used_count, u32 rar_count);
STATIC s32  e1000_setup_link_82571(struct e1000_hw *hw);
STATIC s32  e1000_setup_copper_link_82571(struct e1000_hw *hw);
STATIC s32  e1000_setup_fiber_serdes_link_82571(struct e1000_hw *hw);
STATIC s32  e1000_valid_led_default_82571(struct e1000_hw *hw, u16 *data);
STATIC void e1000_clear_hw_cntrs_82571(struct e1000_hw *hw);
static s32  e1000_get_hw_semaphore_82571(struct e1000_hw *hw);
static s32  e1000_fix_nvm_checksum_82571(struct e1000_hw *hw);
static s32  e1000_get_phy_id_82571(struct e1000_hw *hw);
static void e1000_put_hw_semaphore_82571(struct e1000_hw *hw);
static void e1000_initialize_hw_bits_82571(struct e1000_hw *hw);
static s32  e1000_write_nvm_eewr_82571(struct e1000_hw *hw, u16 offset,
                                       u16 words, u16 *data);
STATIC s32  e1000_read_mac_addr_82571(struct e1000_hw *hw);

struct e1000_dev_spec_82571 {
	bool laa_is_present;
};

/**
 *  e1000_init_phy_params_82571 - Init PHY func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_phy_params_82571(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	struct e1000_functions *func = &hw->func;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_init_phy_params_82571");

	if (hw->phy.media_type != e1000_media_type_copper) {
		phy->type        = e1000_phy_none;
		goto out;
	}

	phy->addr                        = 1;
	phy->autoneg_mask                = AUTONEG_ADVERTISE_SPEED_DEFAULT;
	phy->reset_delay_us              = 100;

	func->acquire_phy                = e1000_get_hw_semaphore_82571;
	func->check_polarity             = e1000_check_polarity_igp;
	func->check_reset_block          = e1000_check_reset_block_generic;
	func->release_phy                = e1000_put_hw_semaphore_82571;
	func->reset_phy                  = e1000_phy_hw_reset_generic;
	func->set_d0_lplu_state          = e1000_set_d0_lplu_state_82571;
	func->set_d3_lplu_state          = e1000_set_d3_lplu_state_generic;

	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		phy->type                = e1000_phy_igp_2;
		func->get_cfg_done       = e1000_get_cfg_done_82571;
		func->get_phy_info       = e1000_get_phy_info_igp;
		func->force_speed_duplex = e1000_phy_force_speed_duplex_igp;
		func->get_cable_length   = e1000_get_cable_length_igp_2;
		func->read_phy_reg       = e1000_read_phy_reg_igp;
		func->write_phy_reg      = e1000_write_phy_reg_igp;

		/* This uses above function pointers */
		ret_val = e1000_get_phy_id_82571(hw);

		/* Verify PHY ID */
		if (phy->id != IGP01E1000_I_PHY_ID) {
			ret_val = -E1000_ERR_PHY;
			goto out;
		}
		break;
	case e1000_82573:
		phy->type                = e1000_phy_m88;
		func->get_cfg_done       = e1000_get_cfg_done_generic;
		func->get_phy_info       = e1000_get_phy_info_m88;
		func->commit_phy         = e1000_phy_sw_reset_generic;
		func->force_speed_duplex = e1000_phy_force_speed_duplex_m88;
		func->get_cable_length   = e1000_get_cable_length_m88;
		func->read_phy_reg       = e1000_read_phy_reg_m88;
		func->write_phy_reg      = e1000_write_phy_reg_m88;

		/* This uses above function pointers */
		ret_val = e1000_get_phy_id_82571(hw);

		/* Verify PHY ID */
		if (phy->id != M88E1111_I_PHY_ID) {
			ret_val = -E1000_ERR_PHY;
			goto out;
		}
		break;

	default:
		ret_val = -E1000_ERR_PHY;
		goto out;
		break;
	}

out:
	return ret_val;
}

/**
 *  e1000_init_nvm_params_82571 - Init NVM func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_nvm_params_82571(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_functions *func = &hw->func;
	u32 eecd = E1000_READ_REG(hw, E1000_EECD);
	u16 size;

	DEBUGFUNC("e1000_init_nvm_params_82571");

	nvm->opcode_bits          = 8;
	nvm->delay_usec           = 1;
	switch (nvm->override) {
	case e1000_nvm_override_spi_large:
		nvm->page_size    = 32;
		nvm->address_bits = 16;
		break;
	case e1000_nvm_override_spi_small:
		nvm->page_size    = 8;
		nvm->address_bits = 8;
		break;
	default:
		nvm->page_size    = eecd & E1000_EECD_ADDR_BITS ? 32 : 8;
		nvm->address_bits = eecd & E1000_EECD_ADDR_BITS ? 16 : 8;
		break;
	}

	switch (hw->mac.type) {
	case e1000_82573:
		if (((eecd >> 15) & 0x3) == 0x3) {
			nvm->type = e1000_nvm_flash_hw;
			nvm->word_size = 2048;
			/*
			 * Autonomous Flash update bit must be cleared due
			 * to Flash update issue.
			 */
			eecd &= ~E1000_EECD_AUPDEN;
			E1000_WRITE_REG(hw, E1000_EECD, eecd);
			break;
		}
		/* Fall Through */
	default:
		nvm->type	= e1000_nvm_eeprom_spi;
		size = (u16)((eecd & E1000_EECD_SIZE_EX_MASK) >>
		                  E1000_EECD_SIZE_EX_SHIFT);
		/*
		 * Added to a constant, "size" becomes the left-shift value
		 * for setting word_size.
		 */
		size += NVM_WORD_SIZE_BASE_SHIFT;
		nvm->word_size	= 1 << size;
		break;
	}

	/* Function Pointers */
	func->acquire_nvm       = e1000_acquire_nvm_82571;
	func->read_nvm          = (hw->mac.type == e1000_82573)
	                          ? e1000_read_nvm_eerd
	                          : e1000_read_nvm_spi;
	func->release_nvm       = e1000_release_nvm_82571;
	func->update_nvm        = e1000_update_nvm_checksum_82571;
	func->validate_nvm      = e1000_validate_nvm_checksum_82571;
	func->valid_led_default = e1000_valid_led_default_82571;
	func->write_nvm         = e1000_write_nvm_82571;

	return E1000_SUCCESS;
}

/**
 *  e1000_init_mac_params_82571 - Init MAC func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_mac_params_82571(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_functions *func = &hw->func;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_init_mac_params_82571");

	/* Set media type */
	switch (hw->device_id) {
	case E1000_DEV_ID_82571EB_FIBER:
	case E1000_DEV_ID_82572EI_FIBER:
	case E1000_DEV_ID_82571EB_QUAD_FIBER:
		hw->phy.media_type = e1000_media_type_fiber;
		break;
	case E1000_DEV_ID_82571EB_SERDES:
	case E1000_DEV_ID_82571EB_SERDES_DUAL:
	case E1000_DEV_ID_82571EB_SERDES_QUAD:
	case E1000_DEV_ID_82572EI_SERDES:
		hw->phy.media_type = e1000_media_type_internal_serdes;
		break;
	default:
		hw->phy.media_type = e1000_media_type_copper;
		break;
	}

	/* Set mta register count */
	mac->mta_reg_count = 128;
	/* Set rar entry count */
	mac->rar_entry_count = E1000_RAR_ENTRIES;
	/* Set if part includes ASF firmware */
	mac->asf_firmware_present = TRUE;
	/* Set if manageability features are enabled. */
	mac->arc_subsystem_valid =
	        (E1000_READ_REG(hw, E1000_FWSM) & E1000_FWSM_MODE_MASK)
	                ? TRUE : FALSE;

	/* Function pointers */

	/* bus type/speed/width */
	func->get_bus_info = e1000_get_bus_info_pcie_generic;
	/* reset */
	func->reset_hw = e1000_reset_hw_82571;
	/* hw initialization */
	func->init_hw = e1000_init_hw_82571;
	/* link setup */
	func->setup_link = e1000_setup_link_82571;
	/* physical interface link setup */
	func->setup_physical_interface =
	        (hw->phy.media_type == e1000_media_type_copper)
	                ? e1000_setup_copper_link_82571
	                : e1000_setup_fiber_serdes_link_82571;
	/* check for link */
	switch (hw->phy.media_type) {
	case e1000_media_type_copper:
		func->check_for_link = e1000_check_for_copper_link_generic;
		break;
	case e1000_media_type_fiber:
		func->check_for_link = e1000_check_for_fiber_link_generic;
		break;
	case e1000_media_type_internal_serdes:
		func->check_for_link = e1000_check_for_serdes_link_generic;
		break;
	default:
		ret_val = -E1000_ERR_CONFIG;
		goto out;
		break;
	}
	/* check management mode */
	func->check_mng_mode = e1000_check_mng_mode_generic;
	/* multicast address update */
	func->update_mc_addr_list = e1000_update_mc_addr_list_82571;
	/* writing VFTA */
	func->write_vfta = e1000_write_vfta_generic;
	/* clearing VFTA */
	func->clear_vfta = e1000_clear_vfta_82571;
	/* setting MTA */
	func->mta_set = e1000_mta_set_generic;
	/* read mac address */
	func->read_mac_addr = e1000_read_mac_addr_82571;
	/* blink LED */
	func->blink_led = e1000_blink_led_generic;
	/* setup LED */
	func->setup_led = e1000_setup_led_generic;
	/* cleanup LED */
	func->cleanup_led = e1000_cleanup_led_generic;
	/* turn on/off LED */
	func->led_on = e1000_led_on_generic;
	func->led_off = e1000_led_off_generic;
	/* remove device */
	func->remove_device = e1000_remove_device_generic;
	/* clear hardware counters */
	func->clear_hw_cntrs = e1000_clear_hw_cntrs_82571;
	/* link info */
	func->get_link_up_info =
	        (hw->phy.media_type == e1000_media_type_copper)
	                ? e1000_get_speed_and_duplex_copper_generic
	                : e1000_get_speed_and_duplex_fiber_serdes_generic;

	hw->dev_spec_size = sizeof(struct e1000_dev_spec_82571);

	/* Device-specific structure allocation */
	ret_val = e1000_alloc_zeroed_dev_spec_struct(hw, hw->dev_spec_size);

out:
	return ret_val;
}

/**
 *  e1000_init_function_pointers_82571 - Init func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  The only function explicitly called by the api module to initialize
 *  all function pointers and parameters.
 **/
void e1000_init_function_pointers_82571(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_init_function_pointers_82571");

	hw->func.init_mac_params = e1000_init_mac_params_82571;
	hw->func.init_nvm_params = e1000_init_nvm_params_82571;
	hw->func.init_phy_params = e1000_init_phy_params_82571;
}

/**
 *  e1000_get_phy_id_82571 - Retrieve the PHY ID and revision
 *  @hw: pointer to the HW structure
 *
 *  Reads the PHY registers and stores the PHY ID and possibly the PHY
 *  revision in the hardware structure.
 **/
static s32 e1000_get_phy_id_82571(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_get_phy_id_82571");

	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		/*
		 * The 82571 firmware may still be configuring the PHY.
		 * In this case, we cannot access the PHY until the
		 * configuration is done.  So we explicitly set the
		 * PHY ID.
		 */
		phy->id = IGP01E1000_I_PHY_ID;
		break;
	case e1000_82573:
		ret_val = e1000_get_phy_id(hw);
		break;
	default:
		ret_val = -E1000_ERR_PHY;
		break;
	}

	return ret_val;
}

/**
 *  e1000_get_hw_semaphore_82571 - Acquire hardware semaphore
 *  @hw: pointer to the HW structure
 *
 *  Acquire the HW semaphore to access the PHY or NVM
 **/
s32 e1000_get_hw_semaphore_82571(struct e1000_hw *hw)
{
	u32 swsm;
	s32 ret_val = E1000_SUCCESS;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	DEBUGFUNC("e1000_get_hw_semaphore_82571");

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
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_put_hw_semaphore_82571 - Release hardware semaphore
 *  @hw: pointer to the HW structure
 *
 *  Release hardware semaphore used to access the PHY or NVM
 **/
void e1000_put_hw_semaphore_82571(struct e1000_hw *hw)
{
	u32 swsm;

	DEBUGFUNC("e1000_put_hw_semaphore_82571");

	swsm = E1000_READ_REG(hw, E1000_SWSM);

	swsm &= ~E1000_SWSM_SWESMBI;

	E1000_WRITE_REG(hw, E1000_SWSM, swsm);
}

/**
 *  e1000_acquire_nvm_82571 - Request for access to the EEPROM
 *  @hw: pointer to the HW structure
 *
 *  To gain access to the EEPROM, first we must obtain a hardware semaphore.
 *  Then for non-82573 hardware, set the EEPROM access request bit and wait
 *  for EEPROM access grant bit.  If the access grant bit is not set, release
 *  hardware semaphore.
 **/
STATIC s32 e1000_acquire_nvm_82571(struct e1000_hw *hw)
{
	s32 ret_val;

	DEBUGFUNC("e1000_acquire_nvm_82571");

	ret_val = e1000_get_hw_semaphore_82571(hw);
	if (ret_val)
		goto out;

	if (hw->mac.type != e1000_82573)
		ret_val = e1000_acquire_nvm_generic(hw);

	if (ret_val)
		e1000_put_hw_semaphore_82571(hw);

out:
	return ret_val;
}

/**
 *  e1000_release_nvm_82571 - Release exclusive access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Stop any current commands to the EEPROM and clear the EEPROM request bit.
 **/
STATIC void e1000_release_nvm_82571(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_release_nvm_82571");

	e1000_release_nvm_generic(hw);
	e1000_put_hw_semaphore_82571(hw);
}

/**
 *  e1000_write_nvm_82571 - Write to EEPROM using appropriate interface
 *  @hw: pointer to the HW structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  For non-82573 silicon, write data to EEPROM at offset using SPI interface.
 *
 *  If e1000_update_nvm_checksum is not called after this function, the
 *  EEPROM will most likley contain an invalid checksum.
 **/
STATIC s32 e1000_write_nvm_82571(struct e1000_hw *hw, u16 offset, u16 words,
                                 u16 *data)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_write_nvm_82571");

	switch (hw->mac.type) {
	case e1000_82573:
		ret_val = e1000_write_nvm_eewr_82571(hw, offset, words, data);
		break;
	case e1000_82571:
	case e1000_82572:
		ret_val = e1000_write_nvm_spi(hw, offset, words, data);
		break;
	default:
		ret_val = -E1000_ERR_NVM;
		break;
	}

	return ret_val;
}

/**
 *  e1000_update_nvm_checksum_82571 - Update EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Updates the EEPROM checksum by reading/adding each word of the EEPROM
 *  up to the checksum.  Then calculates the EEPROM checksum and writes the
 *  value to the EEPROM.
 **/
STATIC s32 e1000_update_nvm_checksum_82571(struct e1000_hw *hw)
{
	u32 eecd;
	s32 ret_val;
	u16 i;

	DEBUGFUNC("e1000_update_nvm_checksum_82571");

	ret_val = e1000_update_nvm_checksum_generic(hw);
	if (ret_val)
		goto out;

	/*
	 * If our nvm is an EEPROM, then we're done
	 * otherwise, commit the checksum to the flash NVM.
	 */
	if (hw->nvm.type != e1000_nvm_flash_hw)
		goto out;

	/* Check for pending operations. */
	for (i = 0; i < E1000_FLASH_UPDATES; i++) {
		msec_delay(1);
		if ((E1000_READ_REG(hw, E1000_EECD) & E1000_EECD_FLUPD) == 0)
			break;
	}

	if (i == E1000_FLASH_UPDATES) {
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	/* Reset the firmware if using STM opcode. */
	if ((E1000_READ_REG(hw, E1000_FLOP) & 0xFF00) == E1000_STM_OPCODE) {
		/*
		 * The enabling of and the actual reset must be done
		 * in two write cycles.
		 */
		E1000_WRITE_REG(hw, E1000_HICR, E1000_HICR_FW_RESET_ENABLE);
		E1000_WRITE_FLUSH(hw);
		E1000_WRITE_REG(hw, E1000_HICR, E1000_HICR_FW_RESET);
	}

	/* Commit the write to flash */
	eecd = E1000_READ_REG(hw, E1000_EECD) | E1000_EECD_FLUPD;
	E1000_WRITE_REG(hw, E1000_EECD, eecd);

	for (i = 0; i < E1000_FLASH_UPDATES; i++) {
		msec_delay(1);
		if ((E1000_READ_REG(hw, E1000_EECD) & E1000_EECD_FLUPD) == 0)
			break;
	}

	if (i == E1000_FLASH_UPDATES) {
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_validate_nvm_checksum_82571 - Validate EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Calculates the EEPROM checksum by reading/adding each word of the EEPROM
 *  and then verifies that the sum of the EEPROM is equal to 0xBABA.
 **/
STATIC s32 e1000_validate_nvm_checksum_82571(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_validate_nvm_checksum_82571");

	if (hw->nvm.type == e1000_nvm_flash_hw)
		e1000_fix_nvm_checksum_82571(hw);

	return e1000_validate_nvm_checksum_generic(hw);
}

/**
 *  e1000_write_nvm_eewr_82571 - Write to EEPROM for 82573 silicon
 *  @hw: pointer to the HW structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  After checking for invalid values, poll the EEPROM to ensure the previous
 *  command has completed before trying to write the next word.  After write
 *  poll for completion.
 *
 *  If e1000_update_nvm_checksum is not called after this function, the
 *  EEPROM will most likley contain an invalid checksum.
 **/
static s32 e1000_write_nvm_eewr_82571(struct e1000_hw *hw, u16 offset,
                                      u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i, eewr = 0;
	s32 ret_val = 0;

	DEBUGFUNC("e1000_write_nvm_eewr_82571");

	/*
	 * A check for invalid values:  offset too large, too many words,
	 * and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	for (i = 0; i < words; i++) {
		eewr = (data[i] << E1000_NVM_RW_REG_DATA) |
		       ((offset+i) << E1000_NVM_RW_ADDR_SHIFT) |
		       E1000_NVM_RW_REG_START;

		ret_val = e1000_poll_eerd_eewr_done(hw, E1000_NVM_POLL_WRITE);
		if (ret_val)
			break;

		E1000_WRITE_REG(hw, E1000_EEWR, eewr);

		ret_val = e1000_poll_eerd_eewr_done(hw, E1000_NVM_POLL_WRITE);
		if (ret_val)
			break;
	}

out:
	return ret_val;
}

/**
 *  e1000_get_cfg_done_82571 - Poll for configuration done
 *  @hw: pointer to the HW structure
 *
 *  Reads the management control register for the config done bit to be set.
 **/
STATIC s32 e1000_get_cfg_done_82571(struct e1000_hw *hw)
{
	s32 timeout = PHY_CFG_TIMEOUT;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_get_cfg_done_82571");

	while (timeout) {
		if (E1000_READ_REG(hw, E1000_EEMNGCTL) & E1000_NVM_CFG_DONE_PORT_0)
			break;
		msec_delay(1);
		timeout--;
	}
	if (!timeout) {
		DEBUGOUT("MNG configuration cycle has not completed.\n");
		ret_val = -E1000_ERR_RESET;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_set_d0_lplu_state_82571 - Set Low Power Linkup D0 state
 *  @hw: pointer to the HW structure
 *  @active: TRUE to enable LPLU, FALSE to disable
 *
 *  Sets the LPLU D0 state according to the active flag.  When activating LPLU
 *  this function also disables smart speed and vice versa.  LPLU will not be
 *  activated unless the device autonegotiation advertisement meets standards
 *  of either 10 or 10/100 or 10/100/1000 at all duplexes.  This is a function
 *  pointer entry point only called by PHY setup routines.
 **/
STATIC s32 e1000_set_d0_lplu_state_82571(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	DEBUGFUNC("e1000_set_d0_lplu_state_82571");

	ret_val = e1000_read_phy_reg(hw, IGP02E1000_PHY_POWER_MGMT, &data);
	if (ret_val)
		goto out;

	if (active) {
		data |= IGP02E1000_PM_D0_LPLU;
		ret_val = e1000_write_phy_reg(hw,
		                              IGP02E1000_PHY_POWER_MGMT,
		                              data);
		if (ret_val)
			goto out;

		/* When LPLU is enabled, we should disable SmartSpeed */
		ret_val = e1000_read_phy_reg(hw,
		                             IGP01E1000_PHY_PORT_CONFIG,
		                             &data);
		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = e1000_write_phy_reg(hw,
		                              IGP01E1000_PHY_PORT_CONFIG,
		                              data);
		if (ret_val)
			goto out;
	} else {
		data &= ~IGP02E1000_PM_D0_LPLU;
		ret_val = e1000_write_phy_reg(hw,
		                              IGP02E1000_PHY_POWER_MGMT,
		                              data);
		/*
		 * LPLU and SmartSpeed are mutually exclusive.  LPLU is used
		 * during Dx states where the power conservation is most
		 * important.  During driver activity we should enable
		 * SmartSpeed, so performance is maintained.
		 */
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = e1000_read_phy_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             &data);
			if (ret_val)
				goto out;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1000_write_phy_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             data);
			if (ret_val)
				goto out;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = e1000_read_phy_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             &data);
			if (ret_val)
				goto out;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1000_write_phy_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             data);
			if (ret_val)
				goto out;
		}
	}

out:
	return ret_val;
}

/**
 *  e1000_reset_hw_82571 - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets the hardware into a known state.  This is a
 *  function pointer entry point called by the api module.
 **/
STATIC s32 e1000_reset_hw_82571(struct e1000_hw *hw)
{
	u32 ctrl, extcnf_ctrl, ctrl_ext, icr;
	s32 ret_val;
	u16 i = 0;

	DEBUGFUNC("e1000_reset_hw_82571");

	/*
	 * Prevent the PCI-E bus from sticking if there is no TLP connection
	 * on the last TLP read/write transaction when MAC is reset.
	 */
	ret_val = e1000_disable_pcie_master_generic(hw);
	if (ret_val) {
		DEBUGOUT("PCI-E Master disable polling has failed.\n");
	}

	DEBUGOUT("Masking off all interrupts\n");
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);

	E1000_WRITE_REG(hw, E1000_RCTL, 0);
	E1000_WRITE_REG(hw, E1000_TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH(hw);

	msec_delay(10);

	/*
	 * Must acquire the MDIO ownership before MAC reset.
	 * Ownership defaults to firmware after a reset.
	 */
	if (hw->mac.type == e1000_82573) {
		extcnf_ctrl = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);
		extcnf_ctrl |= E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP;

		do {
			E1000_WRITE_REG(hw, E1000_EXTCNF_CTRL, extcnf_ctrl);
			extcnf_ctrl = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);

			if (extcnf_ctrl & E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP)
				break;

			extcnf_ctrl |= E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP;

			msec_delay(2);
			i++;
		} while (i < MDIO_OWNERSHIP_TIMEOUT);
	}

	ctrl = E1000_READ_REG(hw, E1000_CTRL);

	DEBUGOUT("Issuing a global reset to MAC\n");
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl | E1000_CTRL_RST);

	if (hw->nvm.type == e1000_nvm_flash_hw) {
		usec_delay(10);
		ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_EE_RST;
		E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH(hw);
	}

	ret_val = e1000_get_auto_rd_done_generic(hw);
	if (ret_val)
		/* We don't want to continue accessing MAC registers. */
		goto out;

	/*
	 * Phy configuration from NVM just starts after EECD_AUTO_RD is set.
	 * Need to wait for Phy configuration completion before accessing
	 * NVM and Phy.
	 */
	if (hw->mac.type == e1000_82573)
		msec_delay(25);

	/* Clear any pending interrupt events. */
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	icr = E1000_READ_REG(hw, E1000_ICR);

	e1000_check_alt_mac_addr_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_init_hw_82571 - Initialize hardware
 *  @hw: pointer to the HW structure
 *
 *  This inits the hardware readying it for operation.
 **/
STATIC s32 e1000_init_hw_82571(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 reg_data;
	s32 ret_val;
	u16 i, rar_count = mac->rar_entry_count;

	DEBUGFUNC("e1000_init_hw_82571");

	e1000_initialize_hw_bits_82571(hw);

	/* Initialize identification LED */
	ret_val = e1000_id_led_init_generic(hw);
	if (ret_val) {
		DEBUGOUT("Error initializing identification LED\n");
		/* This is not fatal and we should not stop init due to this */
	}

	/* Disabling VLAN filtering */
	DEBUGOUT("Initializing the IEEE VLAN\n");
	e1000_clear_vfta(hw);

	/* Setup the receive address. */
	/*
	 * If, however, a locally administered address was assigned to the
	 * 82571, we must reserve a RAR for it to work around an issue where
	 * resetting one port will reload the MAC on the other port.
	 */
	if (e1000_get_laa_state_82571(hw))
		rar_count--;
	e1000_init_rx_addrs_generic(hw, rar_count);

	/* Zero out the Multicast HASH table */
	DEBUGOUT("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);

	/* Setup link and flow control */
	ret_val = e1000_setup_link(hw);

	/* Set the transmit descriptor write-back policy */
	reg_data = E1000_READ_REG(hw, E1000_TXDCTL(0));
	reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
	           E1000_TXDCTL_FULL_TX_DESC_WB |
	           E1000_TXDCTL_COUNT_DESC;
	E1000_WRITE_REG(hw, E1000_TXDCTL(0), reg_data);

	/* ...for both queues. */
	if (mac->type != e1000_82573) {
		reg_data = E1000_READ_REG(hw, E1000_TXDCTL(1));
		reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
		           E1000_TXDCTL_FULL_TX_DESC_WB |
		           E1000_TXDCTL_COUNT_DESC;
		E1000_WRITE_REG(hw, E1000_TXDCTL(1), reg_data);
	} else {
		e1000_enable_tx_pkt_filtering(hw);
		reg_data = E1000_READ_REG(hw, E1000_GCR);
		reg_data |= E1000_GCR_L1_ACT_WITHOUT_L0S_RX;
		E1000_WRITE_REG(hw, E1000_GCR, reg_data);
	}

	/*
	 * Clear all of the statistics registers (clear on read).  It is
	 * important that we do this after we have tried to establish link
	 * because the symbol error count will increment wildly if there
	 * is no link.
	 */
	e1000_clear_hw_cntrs_82571(hw);

	return ret_val;
}

/**
 *  e1000_initialize_hw_bits_82571 - Initialize hardware-dependent bits
 *  @hw: pointer to the HW structure
 *
 *  Initializes required hardware-dependent bits needed for normal operation.
 **/
static void e1000_initialize_hw_bits_82571(struct e1000_hw *hw)
{
	u32 reg;

	DEBUGFUNC("e1000_initialize_hw_bits_82571");

	if (hw->mac.disable_hw_init_bits)
		goto out;

	/* Transmit Descriptor Control 0 */
	reg = E1000_READ_REG(hw, E1000_TXDCTL(0));
	reg |= (1 << 22);
	E1000_WRITE_REG(hw, E1000_TXDCTL(0), reg);

	/* Transmit Descriptor Control 1 */
	reg = E1000_READ_REG(hw, E1000_TXDCTL(1));
	reg |= (1 << 22);
	E1000_WRITE_REG(hw, E1000_TXDCTL(1), reg);

	/* Transmit Arbitration Control 0 */
	reg = E1000_READ_REG(hw, E1000_TARC(0));
	reg &= ~(0xF << 27); /* 30:27 */
	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		reg |= (1 << 23) | (1 << 24) | (1 << 25) | (1 << 26);
		break;
	default:
		break;
	}
	E1000_WRITE_REG(hw, E1000_TARC(0), reg);

	/* Transmit Arbitration Control 1 */
	reg = E1000_READ_REG(hw, E1000_TARC(1));
	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		reg &= ~((1 << 29) | (1 << 30));
		reg |= (1 << 22) | (1 << 24) | (1 << 25) | (1 << 26);
		if (E1000_READ_REG(hw, E1000_TCTL) & E1000_TCTL_MULR)
			reg &= ~(1 << 28);
		else
			reg |= (1 << 28);
		E1000_WRITE_REG(hw, E1000_TARC(1), reg);
		break;
	default:
		break;
	}

	/* Device Control */
	if (hw->mac.type == e1000_82573) {
		reg = E1000_READ_REG(hw, E1000_CTRL);
		reg &= ~(1 << 29);
		E1000_WRITE_REG(hw, E1000_CTRL, reg);
	}

	/* Extended Device Control */
	if (hw->mac.type == e1000_82573) {
		reg = E1000_READ_REG(hw, E1000_CTRL_EXT);
		reg &= ~(1 << 23);
		reg |= (1 << 22);
		E1000_WRITE_REG(hw, E1000_CTRL_EXT, reg);
	}

out:
	return;
}

/**
 *  e1000_clear_vfta_82571 - Clear VLAN filter table
 *  @hw: pointer to the HW structure
 *
 *  Clears the register array which contains the VLAN filter table by
 *  setting all the values to 0.
 **/
STATIC void e1000_clear_vfta_82571(struct e1000_hw *hw)
{
	u32 offset;
	u32 vfta_value = 0;
	u32 vfta_offset = 0;
	u32 vfta_bit_in_reg = 0;

	DEBUGFUNC("e1000_clear_vfta_82571");

	if (hw->mac.type == e1000_82573) {
		if (hw->mng_cookie.vlan_id != 0) {
			/*
			 * The VFTA is a 4096b bit-field, each identifying
			 * a single VLAN ID.  The following operations
			 * determine which 32b entry (i.e. offset) into the
			 * array we want to set the VLAN ID (i.e. bit) of
			 * the manageability unit.
			 */
			vfta_offset = (hw->mng_cookie.vlan_id >>
			               E1000_VFTA_ENTRY_SHIFT) &
			              E1000_VFTA_ENTRY_MASK;
			vfta_bit_in_reg = 1 << (hw->mng_cookie.vlan_id &
			                       E1000_VFTA_ENTRY_BIT_SHIFT_MASK);
		}
	}
	for (offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++) {
		/*
		 * If the offset we want to clear is the same offset of the
		 * manageability VLAN ID, then clear all bits except that of
		 * the manageability unit.
		 */
		vfta_value = (offset == vfta_offset) ? vfta_bit_in_reg : 0;
		E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, offset, vfta_value);
		E1000_WRITE_FLUSH(hw);
	}
}

/**
 *  e1000_update_mc_addr_list_82571 - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *  @rar_used_count: the first RAR register free to program
 *  @rar_count: total number of supported Receive Address Registers
 *
 *  Updates the Receive Address Registers and Multicast Table Array.
 *  The caller must have a packed mc_addr_list of multicast addresses.
 *  The parameter rar_count will usually be hw->mac.rar_entry_count
 *  unless there are workarounds that change this.
 **/
STATIC void e1000_update_mc_addr_list_82571(struct e1000_hw *hw,
                                            u8 *mc_addr_list, u32 mc_addr_count,
                                            u32 rar_used_count, u32 rar_count)
{
	DEBUGFUNC("e1000_update_mc_addr_list_82571");

	if (e1000_get_laa_state_82571(hw))
		rar_count--;

	e1000_update_mc_addr_list_generic(hw, mc_addr_list, mc_addr_count,
	                                  rar_used_count, rar_count);
}

/**
 *  e1000_setup_link_82571 - Setup flow control and link settings
 *  @hw: pointer to the HW structure
 *
 *  Determines which flow control settings to use, then configures flow
 *  control.  Calls the appropriate media-specific link configuration
 *  function.  Assuming the adapter has a valid link partner, a valid link
 *  should be established.  Assumes the hardware has previously been reset
 *  and the transmitter and receiver are not enabled.
 **/
STATIC s32 e1000_setup_link_82571(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_setup_link_82571");

	/*
	 * 82573 does not have a word in the NVM to determine
	 * the default flow control setting, so we explicitly
	 * set it to full.
	 */
	if (hw->mac.type == e1000_82573)
		hw->fc.type = e1000_fc_full;

	return e1000_setup_link_generic(hw);
}

/**
 *  e1000_setup_copper_link_82571 - Configure copper link settings
 *  @hw: pointer to the HW structure
 *
 *  Configures the link for auto-neg or forced speed and duplex.  Then we check
 *  for link, once link is established calls to configure collision distance
 *  and flow control are called.
 **/
STATIC s32 e1000_setup_copper_link_82571(struct e1000_hw *hw)
{
	u32 ctrl, led_ctrl;
	s32  ret_val;

	DEBUGFUNC("e1000_setup_copper_link_82571");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

	switch (hw->phy.type) {
	case e1000_phy_m88:
		ret_val = e1000_copper_link_setup_m88(hw);
		break;
	case e1000_phy_igp_2:
		ret_val = e1000_copper_link_setup_igp(hw);
		/* Setup activity LED */
		led_ctrl = E1000_READ_REG(hw, E1000_LEDCTL);
		led_ctrl &= IGP_ACTIVITY_LED_MASK;
		led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
		E1000_WRITE_REG(hw, E1000_LEDCTL, led_ctrl);
		break;
	default:
		ret_val = -E1000_ERR_PHY;
		break;
	}

	if (ret_val)
		goto out;

	ret_val = e1000_setup_copper_link_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_setup_fiber_serdes_link_82571 - Setup link for fiber/serdes
 *  @hw: pointer to the HW structure
 *
 *  Configures collision distance and flow control for fiber and serdes links.
 *  Upon successful setup, poll for link.
 **/
STATIC s32 e1000_setup_fiber_serdes_link_82571(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_setup_fiber_serdes_link_82571");

	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		/*
		 * If SerDes loopback mode is entered, there is no form
		 * of reset to take the adapter out of that mode.  So we
		 * have to explicitly take the adapter out of loopback
		 * mode.  This prevents drivers from twidling their thumbs
		 * if another tool failed to take it out of loopback mode.
		 */
		E1000_WRITE_REG(hw, E1000_SCTL, E1000_SCTL_DISABLE_SERDES_LOOPBACK);
		break;
	default:
		break;
	}

	return e1000_setup_fiber_serdes_link_generic(hw);
}

/**
 *  e1000_valid_led_default_82571 - Verify a valid default LED config
 *  @hw: pointer to the HW structure
 *  @data: pointer to the NVM (EEPROM)
 *
 *  Read the EEPROM for the current default LED configuration.  If the
 *  LED configuration is not valid, set to a valid LED configuration.
 **/
STATIC s32 e1000_valid_led_default_82571(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	DEBUGFUNC("e1000_valid_led_default_82571");

	ret_val = e1000_read_nvm(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		goto out;
	}

	if (hw->mac.type == e1000_82573 &&
	    *data == ID_LED_RESERVED_F746)
		*data = ID_LED_DEFAULT_82573;
	else if (*data == ID_LED_RESERVED_0000 ||
	         *data == ID_LED_RESERVED_FFFF)
		*data = ID_LED_DEFAULT;
out:
	return ret_val;
}

/**
 *  e1000_get_laa_state_82571 - Get locally administered address state
 *  @hw: pointer to the HW structure
 *
 *  Retrieve and return the current locally administed address state.
 **/
bool e1000_get_laa_state_82571(struct e1000_hw *hw)
{
	struct e1000_dev_spec_82571 *dev_spec;
	bool state = FALSE;

	DEBUGFUNC("e1000_get_laa_state_82571");

	if (hw->mac.type != e1000_82571)
		goto out;

	dev_spec = (struct e1000_dev_spec_82571 *)hw->dev_spec;

	state = dev_spec->laa_is_present;

out:
	return state;
}

/**
 *  e1000_set_laa_state_82571 - Set locally administered address state
 *  @hw: pointer to the HW structure
 *  @state: enable/disable locally administered address
 *
 *  Enable/Disable the current locally administed address state.
 **/
void e1000_set_laa_state_82571(struct e1000_hw *hw, bool state)
{
	struct e1000_dev_spec_82571 *dev_spec;

	DEBUGFUNC("e1000_set_laa_state_82571");

	if (hw->mac.type != e1000_82571)
		goto out;

	dev_spec = (struct e1000_dev_spec_82571 *)hw->dev_spec;

	dev_spec->laa_is_present = state;

	/* If workaround is activated... */
	if (state) {
		/*
		 * Hold a copy of the LAA in RAR[14] This is done so that
		 * between the time RAR[0] gets clobbered and the time it
		 * gets fixed, the actual LAA is in one of the RARs and no
		 * incoming packets directed to this port are dropped.
		 * Eventually the LAA will be in RAR[0] and RAR[14].
		 */
		e1000_rar_set_generic(hw, hw->mac.addr,
		                      hw->mac.rar_entry_count - 1);
	}

out:
	return;
}

/**
 *  e1000_fix_nvm_checksum_82571 - Fix EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Verifies that the EEPROM has completed the update.  After updating the
 *  EEPROM, we need to check bit 15 in work 0x23 for the checksum fix.  If
 *  the checksum fix is not implemented, we need to set the bit and update
 *  the checksum.  Otherwise, if bit 15 is set and the checksum is incorrect,
 *  we need to return bad checksum.
 **/
static s32 e1000_fix_nvm_checksum_82571(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	s32 ret_val = E1000_SUCCESS;
	u16 data;

	DEBUGFUNC("e1000_fix_nvm_checksum_82571");

	if (nvm->type != e1000_nvm_flash_hw)
		goto out;

	/*
	 * Check bit 4 of word 10h.  If it is 0, firmware is done updating
	 * 10h-12h.  Checksum may need to be fixed.
	 */
	ret_val = e1000_read_nvm(hw, 0x10, 1, &data);
	if (ret_val)
		goto out;

	if (!(data & 0x10)) {
		/*
		 * Read 0x23 and check bit 15.  This bit is a 1
		 * when the checksum has already been fixed.  If
		 * the checksum is still wrong and this bit is a
		 * 1, we need to return bad checksum.  Otherwise,
		 * we need to set this bit to a 1 and update the
		 * checksum.
		 */
		ret_val = e1000_read_nvm(hw, 0x23, 1, &data);
		if (ret_val)
			goto out;

		if (!(data & 0x8000)) {
			data |= 0x8000;
			ret_val = e1000_write_nvm(hw, 0x23, 1, &data);
			if (ret_val)
				goto out;
			ret_val = e1000_update_nvm_checksum(hw);
		}
	}

out:
	return ret_val;
}

/**
 *  e1000_read_mac_addr_82571 - Read device MAC address
 *  @hw: pointer to the HW structure
 **/
STATIC s32 e1000_read_mac_addr_82571(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_read_mac_addr_82571");
	if (e1000_check_alt_mac_addr_generic(hw))
		ret_val = e1000_read_mac_addr_generic(hw);

	return ret_val;
}

/**
 *  e1000_clear_hw_cntrs_82571 - Clear device specific hardware counters
 *  @hw: pointer to the HW structure
 *
 *  Clears the hardware counters by reading the counter registers.
 **/
STATIC void e1000_clear_hw_cntrs_82571(struct e1000_hw *hw)
{
	volatile u32 temp;

	DEBUGFUNC("e1000_clear_hw_cntrs_82571");

	e1000_clear_hw_cntrs_base_generic(hw);
	temp = E1000_READ_REG(hw, E1000_PRC64);
	temp = E1000_READ_REG(hw, E1000_PRC127);
	temp = E1000_READ_REG(hw, E1000_PRC255);
	temp = E1000_READ_REG(hw, E1000_PRC511);
	temp = E1000_READ_REG(hw, E1000_PRC1023);
	temp = E1000_READ_REG(hw, E1000_PRC1522);
	temp = E1000_READ_REG(hw, E1000_PTC64);
	temp = E1000_READ_REG(hw, E1000_PTC127);
	temp = E1000_READ_REG(hw, E1000_PTC255);
	temp = E1000_READ_REG(hw, E1000_PTC511);
	temp = E1000_READ_REG(hw, E1000_PTC1023);
	temp = E1000_READ_REG(hw, E1000_PTC1522);

	temp = E1000_READ_REG(hw, E1000_ALGNERRC);
	temp = E1000_READ_REG(hw, E1000_RXERRC);
	temp = E1000_READ_REG(hw, E1000_TNCRS);
	temp = E1000_READ_REG(hw, E1000_CEXTERR);
	temp = E1000_READ_REG(hw, E1000_TSCTC);
	temp = E1000_READ_REG(hw, E1000_TSCTFC);

	temp = E1000_READ_REG(hw, E1000_MGTPRC);
	temp = E1000_READ_REG(hw, E1000_MGTPDC);
	temp = E1000_READ_REG(hw, E1000_MGTPTC);

	temp = E1000_READ_REG(hw, E1000_IAC);
	temp = E1000_READ_REG(hw, E1000_ICRXOC);

	temp = E1000_READ_REG(hw, E1000_ICRXPTC);
	temp = E1000_READ_REG(hw, E1000_ICRXATC);
	temp = E1000_READ_REG(hw, E1000_ICTXPTC);
	temp = E1000_READ_REG(hw, E1000_ICTXATC);
	temp = E1000_READ_REG(hw, E1000_ICTXQEC);
	temp = E1000_READ_REG(hw, E1000_ICTXQMTC);
	temp = E1000_READ_REG(hw, E1000_ICRXDMTC);
}
