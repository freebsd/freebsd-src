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
/* $FreeBSD: src/sys/dev/em/e1000_80003es2lan.c,v 1.3.4.1 2007/11/28 23:24:37 jfv Exp $ */

/* e1000_80003es2lan
 */

#include "e1000_api.h"
#include "e1000_80003es2lan.h"

void e1000_init_function_pointers_80003es2lan(struct e1000_hw *hw);

STATIC s32  e1000_init_phy_params_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_init_nvm_params_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_init_mac_params_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_acquire_phy_80003es2lan(struct e1000_hw *hw);
STATIC void e1000_release_phy_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_acquire_nvm_80003es2lan(struct e1000_hw *hw);
STATIC void e1000_release_nvm_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_read_phy_reg_gg82563_80003es2lan(struct e1000_hw *hw,
                                                   u32 offset,
                                                   u16 *data);
STATIC s32  e1000_write_phy_reg_gg82563_80003es2lan(struct e1000_hw *hw,
                                                    u32 offset,
                                                    u16 data);
STATIC s32  e1000_write_nvm_80003es2lan(struct e1000_hw *hw, u16 offset,
                                        u16 words, u16 *data);
STATIC s32  e1000_get_cfg_done_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_phy_force_speed_duplex_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_get_cable_length_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_get_link_up_info_80003es2lan(struct e1000_hw *hw, u16 *speed,
                                               u16 *duplex);
STATIC s32  e1000_reset_hw_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_init_hw_80003es2lan(struct e1000_hw *hw);
STATIC s32  e1000_setup_copper_link_80003es2lan(struct e1000_hw *hw);
STATIC void e1000_clear_hw_cntrs_80003es2lan(struct e1000_hw *hw);
static s32  e1000_acquire_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask);
static s32  e1000_cfg_kmrn_10_100_80003es2lan(struct e1000_hw *hw, u16 duplex);
static s32  e1000_cfg_kmrn_1000_80003es2lan(struct e1000_hw *hw);
static s32  e1000_copper_link_setup_gg82563_80003es2lan(struct e1000_hw *hw);
static void e1000_initialize_hw_bits_80003es2lan(struct e1000_hw *hw);
static void e1000_release_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask);
STATIC void e1000_power_down_phy_copper_80003es2lan(struct e1000_hw *hw);

/*
 * A table for the GG82563 cable length where the range is defined
 * with a lower bound at "index" and the upper bound at
 * "index + 5".
 */
static const u16 e1000_gg82563_cable_length_table[] =
         { 0, 60, 115, 150, 150, 60, 115, 150, 180, 180, 0xFF };
#define GG82563_CABLE_LENGTH_TABLE_SIZE \
                (sizeof(e1000_gg82563_cable_length_table) / \
                 sizeof(e1000_gg82563_cable_length_table[0]))

/**
 *  e1000_init_phy_params_80003es2lan - Init ESB2 PHY func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_phy_params_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	struct e1000_functions *func = &hw->func;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_init_phy_params_80003es2lan");

	if (hw->phy.media_type != e1000_media_type_copper) {
		phy->type        = e1000_phy_none;
		goto out;
	} else {
		func->power_up_phy = e1000_power_up_phy_copper;
		func->power_down_phy = e1000_power_down_phy_copper_80003es2lan;
	}

	phy->addr                = 1;
	phy->autoneg_mask        = AUTONEG_ADVERTISE_SPEED_DEFAULT;
	phy->reset_delay_us      = 100;
	phy->type                = e1000_phy_gg82563;

	func->acquire_phy        = e1000_acquire_phy_80003es2lan;
	func->check_polarity     = e1000_check_polarity_m88;
	func->check_reset_block  = e1000_check_reset_block_generic;
	func->commit_phy         = e1000_phy_sw_reset_generic;
	func->get_cfg_done       = e1000_get_cfg_done_80003es2lan;
	func->get_phy_info       = e1000_get_phy_info_m88;
	func->release_phy        = e1000_release_phy_80003es2lan;
	func->reset_phy          = e1000_phy_hw_reset_generic;
	func->set_d3_lplu_state  = e1000_set_d3_lplu_state_generic;

	func->force_speed_duplex = e1000_phy_force_speed_duplex_80003es2lan;
	func->get_cable_length   = e1000_get_cable_length_80003es2lan;
	func->read_phy_reg       = e1000_read_phy_reg_gg82563_80003es2lan;
	func->write_phy_reg      = e1000_write_phy_reg_gg82563_80003es2lan;

	/* This can only be done after all function pointers are setup. */
	ret_val = e1000_get_phy_id(hw);

	/* Verify phy id */
	if (phy->id != GG82563_E_PHY_ID) {
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_init_nvm_params_80003es2lan - Init ESB2 NVM func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_nvm_params_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_functions *func = &hw->func;
	u32 eecd = E1000_READ_REG(hw, E1000_EECD);
	u16 size;

	DEBUGFUNC("e1000_init_nvm_params_80003es2lan");

	nvm->opcode_bits        = 8;
	nvm->delay_usec         = 1;
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

	nvm->type               = e1000_nvm_eeprom_spi;

	size = (u16)((eecd & E1000_EECD_SIZE_EX_MASK) >>
	                  E1000_EECD_SIZE_EX_SHIFT);

	/*
	 * Added to a constant, "size" becomes the left-shift value
	 * for setting word_size.
	 */
	size += NVM_WORD_SIZE_BASE_SHIFT;

	/* EEPROM access above 16k is unsupported */
	if (size > 14)
		size = 14;
	nvm->word_size	= 1 << size;

	/* Function Pointers */
	func->acquire_nvm       = e1000_acquire_nvm_80003es2lan;
	func->read_nvm          = e1000_read_nvm_eerd;
	func->release_nvm       = e1000_release_nvm_80003es2lan;
	func->update_nvm        = e1000_update_nvm_checksum_generic;
	func->valid_led_default = e1000_valid_led_default_generic;
	func->validate_nvm      = e1000_validate_nvm_checksum_generic;
	func->write_nvm         = e1000_write_nvm_80003es2lan;

	return E1000_SUCCESS;
}

/**
 *  e1000_init_mac_params_80003es2lan - Init ESB2 MAC func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_mac_params_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_functions *func = &hw->func;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_init_mac_params_80003es2lan");

	/* Set media type */
	switch (hw->device_id) {
	case E1000_DEV_ID_80003ES2LAN_SERDES_DPT:
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
	func->reset_hw = e1000_reset_hw_80003es2lan;
	/* hw initialization */
	func->init_hw = e1000_init_hw_80003es2lan;
	/* link setup */
	func->setup_link = e1000_setup_link_generic;
	/* physical interface link setup */
	func->setup_physical_interface =
	        (hw->phy.media_type == e1000_media_type_copper)
	                ? e1000_setup_copper_link_80003es2lan
	                : e1000_setup_fiber_serdes_link_generic;
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
	func->update_mc_addr_list = e1000_update_mc_addr_list_generic;
	/* writing VFTA */
	func->write_vfta = e1000_write_vfta_generic;
	/* clearing VFTA */
	func->clear_vfta = e1000_clear_vfta_generic;
	/* setting MTA */
	func->mta_set = e1000_mta_set_generic;
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
	func->clear_hw_cntrs = e1000_clear_hw_cntrs_80003es2lan;
	/* link info */
	func->get_link_up_info = e1000_get_link_up_info_80003es2lan;

out:
	return ret_val;
}

/**
 *  e1000_init_function_pointers_80003es2lan - Init ESB2 func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  The only function explicitly called by the api module to initialize
 *  all function pointers and parameters.
 **/
void e1000_init_function_pointers_80003es2lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_init_function_pointers_80003es2lan");

	hw->func.init_mac_params = e1000_init_mac_params_80003es2lan;
	hw->func.init_nvm_params = e1000_init_nvm_params_80003es2lan;
	hw->func.init_phy_params = e1000_init_phy_params_80003es2lan;
}

/**
 *  e1000_acquire_phy_80003es2lan - Acquire rights to access PHY
 *  @hw: pointer to the HW structure
 *
 *  A wrapper to acquire access rights to the correct PHY.  This is a
 *  function pointer entry point called by the api module.
 **/
STATIC s32 e1000_acquire_phy_80003es2lan(struct e1000_hw *hw)
{
	u16 mask;

	DEBUGFUNC("e1000_acquire_phy_80003es2lan");

	mask = hw->bus.func ? E1000_SWFW_PHY1_SM : E1000_SWFW_PHY0_SM;

	return e1000_acquire_swfw_sync_80003es2lan(hw, mask);
}

/**
 *  e1000_release_phy_80003es2lan - Release rights to access PHY
 *  @hw: pointer to the HW structure
 *
 *  A wrapper to release access rights to the correct PHY.  This is a
 *  function pointer entry point called by the api module.
 **/
STATIC void e1000_release_phy_80003es2lan(struct e1000_hw *hw)
{
	u16 mask;

	DEBUGFUNC("e1000_release_phy_80003es2lan");

	mask = hw->bus.func ? E1000_SWFW_PHY1_SM : E1000_SWFW_PHY0_SM;
	e1000_release_swfw_sync_80003es2lan(hw, mask);
}

/**
 *  e1000_acquire_nvm_80003es2lan - Acquire rights to access NVM
 *  @hw: pointer to the HW structure
 *
 *  Acquire the semaphore to access the EEPROM.  This is a function
 *  pointer entry point called by the api module.
 **/
STATIC s32 e1000_acquire_nvm_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val;

	DEBUGFUNC("e1000_acquire_nvm_80003es2lan");

	ret_val = e1000_acquire_swfw_sync_80003es2lan(hw, E1000_SWFW_EEP_SM);
	if (ret_val)
		goto out;

	ret_val = e1000_acquire_nvm_generic(hw);

	if (ret_val)
		e1000_release_swfw_sync_80003es2lan(hw, E1000_SWFW_EEP_SM);

out:
	return ret_val;
}

/**
 *  e1000_release_nvm_80003es2lan - Relinquish rights to access NVM
 *  @hw: pointer to the HW structure
 *
 *  Release the semaphore used to access the EEPROM.  This is a
 *  function pointer entry point called by the api module.
 **/
STATIC void e1000_release_nvm_80003es2lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_release_nvm_80003es2lan");

	e1000_release_nvm_generic(hw);
	e1000_release_swfw_sync_80003es2lan(hw, E1000_SWFW_EEP_SM);
}

/**
 *  e1000_acquire_swfw_sync_80003es2lan - Acquire SW/FW semaphore
 *  @hw: pointer to the HW structure
 *  @mask: specifies which semaphore to acquire
 *
 *  Acquire the SW/FW semaphore to access the PHY or NVM.  The mask
 *  will also specify which port we're acquiring the lock for.
 **/
static s32 e1000_acquire_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;
	u32 fwmask = mask << 16;
	s32 ret_val = E1000_SUCCESS;
	s32 i = 0, timeout = 200;

	DEBUGFUNC("e1000_acquire_swfw_sync_80003es2lan");

	while (i < timeout) {
		if (e1000_get_hw_semaphore_generic(hw)) {
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
 *  e1000_release_swfw_sync_80003es2lan - Release SW/FW semaphore
 *  @hw: pointer to the HW structure
 *  @mask: specifies which semaphore to acquire
 *
 *  Release the SW/FW semaphore used to access the PHY or NVM.  The mask
 *  will also specify which port we're releasing the lock for.
 **/
static void e1000_release_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;

	DEBUGFUNC("e1000_release_swfw_sync_80003es2lan");

	while (e1000_get_hw_semaphore_generic(hw) != E1000_SUCCESS);
	/* Empty */

	swfw_sync = E1000_READ_REG(hw, E1000_SW_FW_SYNC);
	swfw_sync &= ~mask;
	E1000_WRITE_REG(hw, E1000_SW_FW_SYNC, swfw_sync);

	e1000_put_hw_semaphore_generic(hw);
}

/**
 *  e1000_read_phy_reg_gg82563_80003es2lan - Read GG82563 PHY register
 *  @hw: pointer to the HW structure
 *  @offset: offset of the register to read
 *  @data: pointer to the data returned from the operation
 *
 *  Read the GG82563 PHY register.  This is a function pointer entry
 *  point called by the api module.
 **/
STATIC s32 e1000_read_phy_reg_gg82563_80003es2lan(struct e1000_hw *hw,
                                                  u32 offset, u16 *data)
{
	s32 ret_val;
	u32 page_select;
	u16 temp;

	DEBUGFUNC("e1000_read_phy_reg_gg82563_80003es2lan");

	/* Select Configuration Page */
	if ((offset & MAX_PHY_REG_ADDRESS) < GG82563_MIN_ALT_REG) {
		page_select = GG82563_PHY_PAGE_SELECT;
	} else {
		/*
		 * Use Alternative Page Select register to access
		 * registers 30 and 31
		 */
		page_select = GG82563_PHY_PAGE_SELECT_ALT;
	}

	temp = (u16)((u16)offset >> GG82563_PAGE_SHIFT);
	ret_val = e1000_write_phy_reg_m88(hw, page_select, temp);
	if (ret_val)
		goto out;

	/*
	 * The "ready" bit in the MDIC register may be incorrectly set
	 * before the device has completed the "Page Select" MDI
	 * transaction.  So we wait 200us after each MDI command...
	 */
	usec_delay(200);

	/* ...and verify the command was successful. */
	ret_val = e1000_read_phy_reg_m88(hw, page_select, &temp);

	if (((u16)offset >> GG82563_PAGE_SHIFT) != temp) {
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

	usec_delay(200);

	ret_val = e1000_read_phy_reg_m88(hw,
	                                 MAX_PHY_REG_ADDRESS & offset,
	                                 data);

	usec_delay(200);

out:
	return ret_val;
}

/**
 *  e1000_write_phy_reg_gg82563_80003es2lan - Write GG82563 PHY register
 *  @hw: pointer to the HW structure
 *  @offset: offset of the register to read
 *  @data: value to write to the register
 *
 *  Write to the GG82563 PHY register.  This is a function pointer entry
 *  point called by the api module.
 **/
STATIC s32 e1000_write_phy_reg_gg82563_80003es2lan(struct e1000_hw *hw,
                                                   u32 offset, u16 data)
{
	s32 ret_val;
	u32 page_select;
	u16 temp;

	DEBUGFUNC("e1000_write_phy_reg_gg82563_80003es2lan");

	/* Select Configuration Page */
	if ((offset & MAX_PHY_REG_ADDRESS) < GG82563_MIN_ALT_REG) {
		page_select = GG82563_PHY_PAGE_SELECT;
	} else {
		/*
		 * Use Alternative Page Select register to access
		 * registers 30 and 31
		 */
		page_select = GG82563_PHY_PAGE_SELECT_ALT;
	}

	temp = (u16)((u16)offset >> GG82563_PAGE_SHIFT);
	ret_val = e1000_write_phy_reg_m88(hw, page_select, temp);
	if (ret_val)
		goto out;


	/*
	 * The "ready" bit in the MDIC register may be incorrectly set
	 * before the device has completed the "Page Select" MDI
	 * transaction.  So we wait 200us after each MDI command...
	 */
	usec_delay(200);

	/* ...and verify the command was successful. */
	ret_val = e1000_read_phy_reg_m88(hw, page_select, &temp);

	if (((u16)offset >> GG82563_PAGE_SHIFT) != temp) {
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

	usec_delay(200);

	ret_val = e1000_write_phy_reg_m88(hw,
	                                  MAX_PHY_REG_ADDRESS & offset,
	                                  data);

	usec_delay(200);

out:
	return ret_val;
}

/**
 *  e1000_write_nvm_80003es2lan - Write to ESB2 NVM
 *  @hw: pointer to the HW structure
 *  @offset: offset of the register to read
 *  @words: number of words to write
 *  @data: buffer of data to write to the NVM
 *
 *  Write "words" of data to the ESB2 NVM.  This is a function
 *  pointer entry point called by the api module.
 **/
STATIC s32 e1000_write_nvm_80003es2lan(struct e1000_hw *hw, u16 offset,
                            u16 words, u16 *data)
{
	DEBUGFUNC("e1000_write_nvm_80003es2lan");

	return e1000_write_nvm_spi(hw, offset, words, data);
}

/**
 *  e1000_get_cfg_done_80003es2lan - Wait for configuration to complete
 *  @hw: pointer to the HW structure
 *
 *  Wait a specific amount of time for manageability processes to complete.
 *  This is a function pointer entry point called by the phy module.
 **/
STATIC s32 e1000_get_cfg_done_80003es2lan(struct e1000_hw *hw)
{
	s32 timeout = PHY_CFG_TIMEOUT;
	s32 ret_val = E1000_SUCCESS;
	u32 mask = E1000_NVM_CFG_DONE_PORT_0;

	DEBUGFUNC("e1000_get_cfg_done_80003es2lan");

	if (hw->bus.func == 1)
		mask = E1000_NVM_CFG_DONE_PORT_1;

	while (timeout) {
		if (E1000_READ_REG(hw, E1000_EEMNGCTL) & mask)
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
 *  e1000_phy_force_speed_duplex_80003es2lan - Force PHY speed and duplex
 *  @hw: pointer to the HW structure
 *
 *  Force the speed and duplex settings onto the PHY.  This is a
 *  function pointer entry point called by the phy module.
 **/
STATIC s32 e1000_phy_force_speed_duplex_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;
	bool link;

	DEBUGFUNC("e1000_phy_force_speed_duplex_80003es2lan");

	/*
	 * Clear Auto-Crossover to force MDI manually.  M88E1000 requires MDI
	 * forced whenever speed and duplex are forced.
	 */
	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy_data &= ~GG82563_PSCR_CROSSOVER_MODE_AUTO;
	ret_val = e1000_write_phy_reg(hw, GG82563_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		goto out;

	DEBUGOUT1("GG82563 PSCR: %X\n", phy_data);

	ret_val = e1000_read_phy_reg(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		goto out;

	e1000_phy_force_speed_duplex_setup(hw, &phy_data);

	/* Reset the phy to commit changes. */
	phy_data |= MII_CR_RESET;

	ret_val = e1000_write_phy_reg(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		goto out;

	usec_delay(1);

	if (hw->phy.autoneg_wait_to_complete) {
		DEBUGOUT("Waiting for forced speed/duplex link "
		         "on GG82563 phy.\n");

		ret_val = e1000_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
		                                     100000, &link);
		if (ret_val)
			goto out;

		if (!link) {
			/*
			 * We didn't get link.
			 * Reset the DSP and cross our fingers.
			 */
			ret_val = e1000_phy_reset_dsp_generic(hw);
			if (ret_val)
				goto out;
		}

		/* Try once more */
		ret_val = e1000_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
		                                     100000, &link);
		if (ret_val)
			goto out;
	}

	ret_val = e1000_read_phy_reg(hw, GG82563_PHY_MAC_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	/*
	 * Resetting the phy means we need to verify the TX_CLK corresponds
	 * to the link speed.  10Mbps -> 2.5MHz, else 25MHz.
	 */
	phy_data &= ~GG82563_MSCR_TX_CLK_MASK;
	if (hw->mac.forced_speed_duplex & E1000_ALL_10_SPEED)
		phy_data |= GG82563_MSCR_TX_CLK_10MBPS_2_5;
	else
		phy_data |= GG82563_MSCR_TX_CLK_100MBPS_25;

	/*
	 * In addition, we must re-enable CRS on Tx for both half and full
	 * duplex.
	 */
	phy_data |= GG82563_MSCR_ASSERT_CRS_ON_TX;
	ret_val = e1000_write_phy_reg(hw, GG82563_PHY_MAC_SPEC_CTRL, phy_data);

out:
	return ret_val;
}

/**
 *  e1000_get_cable_length_80003es2lan - Set approximate cable length
 *  @hw: pointer to the HW structure
 *
 *  Find the approximate cable length as measured by the GG82563 PHY.
 *  This is a function pointer entry point called by the phy module.
 **/
STATIC s32 e1000_get_cable_length_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, index;

	DEBUGFUNC("e1000_get_cable_length_80003es2lan");

	ret_val = e1000_read_phy_reg(hw, GG82563_PHY_DSP_DISTANCE, &phy_data);
	if (ret_val)
		goto out;

	index = phy_data & GG82563_DSPD_CABLE_LENGTH;
	phy->min_cable_length = e1000_gg82563_cable_length_table[index];
	phy->max_cable_length = e1000_gg82563_cable_length_table[index+5];

	phy->cable_length = (phy->min_cable_length + phy->max_cable_length) / 2;

out:
	return ret_val;
}

/**
 *  e1000_get_link_up_info_80003es2lan - Report speed and duplex
 *  @hw: pointer to the HW structure
 *  @speed: pointer to speed buffer
 *  @duplex: pointer to duplex buffer
 *
 *  Retrieve the current speed and duplex configuration.
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_get_link_up_info_80003es2lan(struct e1000_hw *hw, u16 *speed,
                                              u16 *duplex)
{
	s32 ret_val;

	DEBUGFUNC("e1000_get_link_up_info_80003es2lan");

	if (hw->phy.media_type == e1000_media_type_copper) {
		ret_val = e1000_get_speed_and_duplex_copper_generic(hw,
		                                                    speed,
		                                                    duplex);
		if (ret_val)
			goto out;
		if (*speed == SPEED_1000)
			ret_val = e1000_cfg_kmrn_1000_80003es2lan(hw);
		else
			ret_val = e1000_cfg_kmrn_10_100_80003es2lan(hw,
			                                      *duplex);
	} else {
		ret_val = e1000_get_speed_and_duplex_fiber_serdes_generic(hw,
		                                                  speed,
		                                                  duplex);
	}

out:
	return ret_val;
}

/**
 *  e1000_reset_hw_80003es2lan - Reset the ESB2 controller
 *  @hw: pointer to the HW structure
 *
 *  Perform a global reset to the ESB2 controller.
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_reset_hw_80003es2lan(struct e1000_hw *hw)
{
	u32 ctrl, icr;
	s32 ret_val;

	DEBUGFUNC("e1000_reset_hw_80003es2lan");

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

	ctrl = E1000_READ_REG(hw, E1000_CTRL);

	DEBUGOUT("Issuing a global reset to MAC\n");
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl | E1000_CTRL_RST);

	ret_val = e1000_get_auto_rd_done_generic(hw);
	if (ret_val)
		/* We don't want to continue accessing MAC registers. */
		goto out;

	/* Clear any pending interrupt events. */
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	icr = E1000_READ_REG(hw, E1000_ICR);

out:
	return ret_val;
}

/**
 *  e1000_init_hw_80003es2lan - Initialize the ESB2 controller
 *  @hw: pointer to the HW structure
 *
 *  Initialize the hw bits, LED, VFTA, MTA, link and hw counters.
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_hw_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 reg_data;
	s32 ret_val;
	u16 i;

	DEBUGFUNC("e1000_init_hw_80003es2lan");

	e1000_initialize_hw_bits_80003es2lan(hw);

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
	e1000_init_rx_addrs_generic(hw, mac->rar_entry_count);

	/* Zero out the Multicast HASH table */
	DEBUGOUT("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);

	/* Setup link and flow control */
	ret_val = e1000_setup_link(hw);

	/* Set the transmit descriptor write-back policy */
	reg_data = E1000_READ_REG(hw, E1000_TXDCTL(0));
	reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
	           E1000_TXDCTL_FULL_TX_DESC_WB | E1000_TXDCTL_COUNT_DESC;
	E1000_WRITE_REG(hw, E1000_TXDCTL(0), reg_data);

	/* ...for both queues. */
	reg_data = E1000_READ_REG(hw, E1000_TXDCTL(1));
	reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
	           E1000_TXDCTL_FULL_TX_DESC_WB | E1000_TXDCTL_COUNT_DESC;
	E1000_WRITE_REG(hw, E1000_TXDCTL(1), reg_data);

	/* Enable retransmit on late collisions */
	reg_data = E1000_READ_REG(hw, E1000_TCTL);
	reg_data |= E1000_TCTL_RTLC;
	E1000_WRITE_REG(hw, E1000_TCTL, reg_data);

	/* Configure Gigabit Carry Extend Padding */
	reg_data = E1000_READ_REG(hw, E1000_TCTL_EXT);
	reg_data &= ~E1000_TCTL_EXT_GCEX_MASK;
	reg_data |= DEFAULT_TCTL_EXT_GCEX_80003ES2LAN;
	E1000_WRITE_REG(hw, E1000_TCTL_EXT, reg_data);

	/* Configure Transmit Inter-Packet Gap */
	reg_data = E1000_READ_REG(hw, E1000_TIPG);
	reg_data &= ~E1000_TIPG_IPGT_MASK;
	reg_data |= DEFAULT_TIPG_IPGT_1000_80003ES2LAN;
	E1000_WRITE_REG(hw, E1000_TIPG, reg_data);

	reg_data = E1000_READ_REG_ARRAY(hw, E1000_FFLT, 0x0001);
	reg_data &= ~0x00100000;
	E1000_WRITE_REG_ARRAY(hw, E1000_FFLT, 0x0001, reg_data);

	/*
	 * Clear all of the statistics registers (clear on read).  It is
	 * important that we do this after we have tried to establish link
	 * because the symbol error count will increment wildly if there
	 * is no link.
	 */
	e1000_clear_hw_cntrs_80003es2lan(hw);

	return ret_val;
}

/**
 *  e1000_initialize_hw_bits_80003es2lan - Init hw bits of ESB2
 *  @hw: pointer to the HW structure
 *
 *  Initializes required hardware-dependent bits needed for normal operation.
 **/
static void e1000_initialize_hw_bits_80003es2lan(struct e1000_hw *hw)
{
	u32 reg;

	DEBUGFUNC("e1000_initialize_hw_bits_80003es2lan");

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
	if (hw->phy.media_type != e1000_media_type_copper)
		reg &= ~(1 << 20);
	E1000_WRITE_REG(hw, E1000_TARC(0), reg);

	/* Transmit Arbitration Control 1 */
	reg = E1000_READ_REG(hw, E1000_TARC(1));
	if (E1000_READ_REG(hw, E1000_TCTL) & E1000_TCTL_MULR)
		reg &= ~(1 << 28);
	else
		reg |= (1 << 28);
	E1000_WRITE_REG(hw, E1000_TARC(1), reg);

out:
	return;
}

/**
 *  e1000_copper_link_setup_gg82563_80003es2lan - Configure GG82563 Link
 *  @hw: pointer to the HW structure
 *
 *  Setup some GG82563 PHY registers for obtaining link
 **/
static s32 e1000_copper_link_setup_gg82563_80003es2lan(struct e1000_hw *hw)
{
	struct   e1000_phy_info *phy = &hw->phy;
	s32  ret_val;
	u32 ctrl_ext;
	u16 data;

	DEBUGFUNC("e1000_copper_link_setup_gg82563_80003es2lan");

	if (!phy->reset_disable) {
		ret_val = e1000_read_phy_reg(hw, GG82563_PHY_MAC_SPEC_CTRL,
		                             &data);
		if (ret_val)
			goto out;

		data |= GG82563_MSCR_ASSERT_CRS_ON_TX;
		/* Use 25MHz for both link down and 1000Base-T for Tx clock. */
		data |= GG82563_MSCR_TX_CLK_1000MBPS_25;

		ret_val = e1000_write_phy_reg(hw, GG82563_PHY_MAC_SPEC_CTRL,
		                              data);
		if (ret_val)
			goto out;

		/*
		 * Options:
		 *   MDI/MDI-X = 0 (default)
		 *   0 - Auto for all speeds
		 *   1 - MDI mode
		 *   2 - MDI-X mode
		 *   3 - Auto for 1000Base-T only (MDI-X for 10/100Base-T modes)
		 */
		ret_val = e1000_read_phy_reg(hw, GG82563_PHY_SPEC_CTRL, &data);
		if (ret_val)
			goto out;

		data &= ~GG82563_PSCR_CROSSOVER_MODE_MASK;

		switch (phy->mdix) {
		case 1:
			data |= GG82563_PSCR_CROSSOVER_MODE_MDI;
			break;
		case 2:
			data |= GG82563_PSCR_CROSSOVER_MODE_MDIX;
			break;
		case 0:
		default:
			data |= GG82563_PSCR_CROSSOVER_MODE_AUTO;
			break;
		}

		/*
		 * Options:
		 *   disable_polarity_correction = 0 (default)
		 *       Automatic Correction for Reversed Cable Polarity
		 *   0 - Disabled
		 *   1 - Enabled
		 */
		data &= ~GG82563_PSCR_POLARITY_REVERSAL_DISABLE;
		if (phy->disable_polarity_correction)
			data |= GG82563_PSCR_POLARITY_REVERSAL_DISABLE;

		ret_val = e1000_write_phy_reg(hw, GG82563_PHY_SPEC_CTRL, data);
		if (ret_val)
			goto out;

		/* SW Reset the PHY so all changes take effect */
		ret_val = e1000_phy_commit(hw);
		if (ret_val) {
			DEBUGOUT("Error Resetting the PHY\n");
			goto out;
		}

	}

	/* Bypass Rx and Tx FIFO's */
	ret_val = e1000_write_kmrn_reg(hw,
	                        E1000_KMRNCTRLSTA_OFFSET_FIFO_CTRL,
	                        E1000_KMRNCTRLSTA_FIFO_CTRL_RX_BYPASS |
	                                E1000_KMRNCTRLSTA_FIFO_CTRL_TX_BYPASS);
	if (ret_val)
		goto out;

	ret_val = e1000_read_kmrn_reg(hw,
				      E1000_KMRNCTRLSTA_OFFSET_MAC2PHY_OPMODE,
				      &data);
	if (ret_val)
		goto out;
	data |= E1000_KMRNCTRLSTA_OPMODE_E_IDLE;
	ret_val = e1000_write_kmrn_reg(hw,
				       E1000_KMRNCTRLSTA_OFFSET_MAC2PHY_OPMODE,
				       data);
	if (ret_val)
		goto out;

	ret_val = e1000_read_phy_reg(hw, GG82563_PHY_SPEC_CTRL_2, &data);
	if (ret_val)
		goto out;

	data &= ~GG82563_PSCR2_REVERSE_AUTO_NEG;
	ret_val = e1000_write_phy_reg(hw, GG82563_PHY_SPEC_CTRL_2, data);
	if (ret_val)
		goto out;

	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
	ctrl_ext &= ~(E1000_CTRL_EXT_LINK_MODE_MASK);
	E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext);

	ret_val = e1000_read_phy_reg(hw, GG82563_PHY_PWR_MGMT_CTRL, &data);
	if (ret_val)
		goto out;

	/*
	 * Do not init these registers when the HW is in IAMT mode, since the
	 * firmware will have already initialized them.  We only initialize
	 * them if the HW is not in IAMT mode.
	 */
	if (!(e1000_check_mng_mode(hw))) {
		/* Enable Electrical Idle on the PHY */
		data |= GG82563_PMCR_ENABLE_ELECTRICAL_IDLE;
		ret_val = e1000_write_phy_reg(hw,
		                             GG82563_PHY_PWR_MGMT_CTRL,
		                             data);
		if (ret_val)
			goto out;

		ret_val = e1000_read_phy_reg(hw,
		                            GG82563_PHY_KMRN_MODE_CTRL,
		                            &data);
		if (ret_val)
			goto out;

		data &= ~GG82563_KMCR_PASS_FALSE_CARRIER;
		ret_val = e1000_write_phy_reg(hw,
		                             GG82563_PHY_KMRN_MODE_CTRL,
		                             data);

		if (ret_val)
			goto out;
	}

	/*
	 * Workaround: Disable padding in Kumeran interface in the MAC
	 * and in the PHY to avoid CRC errors.
	 */
	ret_val = e1000_read_phy_reg(hw, GG82563_PHY_INBAND_CTRL, &data);
	if (ret_val)
		goto out;

	data |= GG82563_ICR_DIS_PADDING;
	ret_val = e1000_write_phy_reg(hw, GG82563_PHY_INBAND_CTRL, data);
	if (ret_val)
		goto out;

out:
	return ret_val;
}

/**
 *  e1000_setup_copper_link_80003es2lan - Setup Copper Link for ESB2
 *  @hw: pointer to the HW structure
 *
 *  Essentially a wrapper for setting up all things "copper" related.
 *  This is a function pointer entry point called by the mac module.
 **/
STATIC s32 e1000_setup_copper_link_80003es2lan(struct e1000_hw *hw)
{
	u32 ctrl;
	s32  ret_val;
	u16 reg_data;

	DEBUGFUNC("e1000_setup_copper_link_80003es2lan");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

	/*
	 * Set the mac to wait the maximum time between each
	 * iteration and increase the max iterations when
	 * polling the phy; this fixes erroneous timeouts at 10Mbps.
	 */
	ret_val = e1000_write_kmrn_reg(hw, GG82563_REG(0x34, 4), 0xFFFF);
	if (ret_val)
		goto out;
	ret_val = e1000_read_kmrn_reg(hw, GG82563_REG(0x34, 9), &reg_data);
	if (ret_val)
		goto out;
	reg_data |= 0x3F;
	ret_val = e1000_write_kmrn_reg(hw, GG82563_REG(0x34, 9), reg_data);
	if (ret_val)
		goto out;
	ret_val = e1000_read_kmrn_reg(hw,
	                              E1000_KMRNCTRLSTA_OFFSET_INB_CTRL,
	                              &reg_data);
	if (ret_val)
		goto out;
	reg_data |= E1000_KMRNCTRLSTA_INB_CTRL_DIS_PADDING;
	ret_val = e1000_write_kmrn_reg(hw,
	                               E1000_KMRNCTRLSTA_OFFSET_INB_CTRL,
	                               reg_data);
	if (ret_val)
		goto out;

	ret_val = e1000_copper_link_setup_gg82563_80003es2lan(hw);
	if (ret_val)
		goto out;

	ret_val = e1000_setup_copper_link_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_cfg_kmrn_10_100_80003es2lan - Apply "quirks" for 10/100 operation
 *  @hw: pointer to the HW structure
 *  @duplex: current duplex setting
 *
 *  Configure the KMRN interface by applying last minute quirks for
 *  10/100 operation.
 **/
static s32 e1000_cfg_kmrn_10_100_80003es2lan(struct e1000_hw *hw, u16 duplex)
{
	s32 ret_val = E1000_SUCCESS;
	u32 tipg;
	u32 i = 0;
	u16 reg_data, reg_data2;

	DEBUGFUNC("e1000_configure_kmrn_for_10_100");

	reg_data = E1000_KMRNCTRLSTA_HD_CTRL_10_100_DEFAULT;
	ret_val = e1000_write_kmrn_reg(hw,
	                               E1000_KMRNCTRLSTA_OFFSET_HD_CTRL,
	                               reg_data);
	if (ret_val)
		goto out;

	/* Configure Transmit Inter-Packet Gap */
	tipg = E1000_READ_REG(hw, E1000_TIPG);
	tipg &= ~E1000_TIPG_IPGT_MASK;
	tipg |= DEFAULT_TIPG_IPGT_10_100_80003ES2LAN;
	E1000_WRITE_REG(hw, E1000_TIPG, tipg);


	do {
		ret_val = e1000_read_phy_reg(hw, GG82563_PHY_KMRN_MODE_CTRL,
		                             &reg_data);
		if (ret_val)
			goto out;

		ret_val = e1000_read_phy_reg(hw, GG82563_PHY_KMRN_MODE_CTRL,
		                             &reg_data2);
		if (ret_val)
			goto out;
		i++;
	} while ((reg_data != reg_data2) && (i < GG82563_MAX_KMRN_RETRY));

	if (duplex == HALF_DUPLEX)
		reg_data |= GG82563_KMCR_PASS_FALSE_CARRIER;
	else
		reg_data &= ~GG82563_KMCR_PASS_FALSE_CARRIER;

	ret_val = e1000_write_phy_reg(hw, GG82563_PHY_KMRN_MODE_CTRL, reg_data);

out:
	return ret_val;
}

/**
 *  e1000_cfg_kmrn_1000_80003es2lan - Apply "quirks" for gigabit operation
 *  @hw: pointer to the HW structure
 *
 *  Configure the KMRN interface by applying last minute quirks for
 *  gigabit operation.
 **/
static s32 e1000_cfg_kmrn_1000_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 reg_data, reg_data2;
	u32 tipg;
	u32 i = 0;

	DEBUGFUNC("e1000_configure_kmrn_for_1000");

	reg_data = E1000_KMRNCTRLSTA_HD_CTRL_1000_DEFAULT;
	ret_val = e1000_write_kmrn_reg(hw,
	                               E1000_KMRNCTRLSTA_OFFSET_HD_CTRL,
	                               reg_data);
	if (ret_val)
		goto out;

	/* Configure Transmit Inter-Packet Gap */
	tipg = E1000_READ_REG(hw, E1000_TIPG);
	tipg &= ~E1000_TIPG_IPGT_MASK;
	tipg |= DEFAULT_TIPG_IPGT_1000_80003ES2LAN;
	E1000_WRITE_REG(hw, E1000_TIPG, tipg);


	do {
		ret_val = e1000_read_phy_reg(hw, GG82563_PHY_KMRN_MODE_CTRL,
		                             &reg_data);
		if (ret_val)
			goto out;

		ret_val = e1000_read_phy_reg(hw, GG82563_PHY_KMRN_MODE_CTRL,
		                             &reg_data2);
		if (ret_val)
			goto out;
		i++;
	} while ((reg_data != reg_data2) && (i < GG82563_MAX_KMRN_RETRY));

	reg_data &= ~GG82563_KMCR_PASS_FALSE_CARRIER;
	ret_val = e1000_write_phy_reg(hw, GG82563_PHY_KMRN_MODE_CTRL, reg_data);

out:
	return ret_val;
}

/**
 * e1000_power_down_phy_copper_80003es2lan - Remove link during PHY power down
 * @hw: pointer to the HW structure
 *
 * In the case of a PHY power down to save power, or to turn off link during a
 * driver unload, or wake on lan is not enabled, remove the link.
 **/
STATIC void e1000_power_down_phy_copper_80003es2lan(struct e1000_hw *hw)
{
	/* If the management interface is not enabled, then power down */
	if (!(e1000_check_mng_mode(hw) || e1000_check_reset_block(hw)))
		e1000_power_down_phy_copper(hw);

	return;
}

/**
 *  e1000_clear_hw_cntrs_80003es2lan - Clear device specific hardware counters
 *  @hw: pointer to the HW structure
 *
 *  Clears the hardware counters by reading the counter registers.
 **/
STATIC void e1000_clear_hw_cntrs_80003es2lan(struct e1000_hw *hw)
{
	volatile u32 temp;

	DEBUGFUNC("e1000_clear_hw_cntrs_80003es2lan");

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
