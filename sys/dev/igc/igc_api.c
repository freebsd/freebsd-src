/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/cdefs.h>
#include "igc_api.h"

/**
 *  igc_init_mac_params - Initialize MAC function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the MAC
 *  set of functions.  Called by drivers or by igc_setup_init_funcs.
 **/
s32 igc_init_mac_params(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;

	if (hw->mac.ops.init_params) {
		ret_val = hw->mac.ops.init_params(hw);
		if (ret_val) {
			DEBUGOUT("MAC Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("mac.init_mac_params was NULL\n");
		ret_val = -IGC_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  igc_init_nvm_params - Initialize NVM function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the NVM
 *  set of functions.  Called by drivers or by igc_setup_init_funcs.
 **/
s32 igc_init_nvm_params(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;

	if (hw->nvm.ops.init_params) {
		ret_val = hw->nvm.ops.init_params(hw);
		if (ret_val) {
			DEBUGOUT("NVM Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("nvm.init_nvm_params was NULL\n");
		ret_val = -IGC_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  igc_init_phy_params - Initialize PHY function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the PHY
 *  set of functions.  Called by drivers or by igc_setup_init_funcs.
 **/
s32 igc_init_phy_params(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;

	if (hw->phy.ops.init_params) {
		ret_val = hw->phy.ops.init_params(hw);
		if (ret_val) {
			DEBUGOUT("PHY Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("phy.init_phy_params was NULL\n");
		ret_val =  -IGC_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  igc_set_mac_type - Sets MAC type
 *  @hw: pointer to the HW structure
 *
 *  This function sets the mac type of the adapter based on the
 *  device ID stored in the hw structure.
 *  MUST BE FIRST FUNCTION CALLED (explicitly or through
 *  igc_setup_init_funcs()).
 **/
s32 igc_set_mac_type(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	s32 ret_val = IGC_SUCCESS;

	DEBUGFUNC("igc_set_mac_type");

	switch (hw->device_id) {
	case IGC_DEV_ID_I225_LM:
	case IGC_DEV_ID_I225_V:
	case IGC_DEV_ID_I225_K:
	case IGC_DEV_ID_I225_I:
	case IGC_DEV_ID_I220_V:
	case IGC_DEV_ID_I225_K2:
	case IGC_DEV_ID_I225_LMVP:
	case IGC_DEV_ID_I226_K:
	case IGC_DEV_ID_I226_LMVP:
	case IGC_DEV_ID_I225_IT:
	case IGC_DEV_ID_I226_LM:
	case IGC_DEV_ID_I226_V:
	case IGC_DEV_ID_I226_IT:
	case IGC_DEV_ID_I221_V:
	case IGC_DEV_ID_I226_BLANK_NVM:
	case IGC_DEV_ID_I225_BLANK_NVM:
		mac->type = igc_i225;
		break;
	default:
		/* Should never have loaded on this device */
		ret_val = -IGC_ERR_MAC_INIT;
		break;
	}

	return ret_val;
}

/**
 *  igc_setup_init_funcs - Initializes function pointers
 *  @hw: pointer to the HW structure
 *  @init_device: true will initialize the rest of the function pointers
 *		  getting the device ready for use.  FALSE will only set
 *		  MAC type and the function pointers for the other init
 *		  functions.  Passing FALSE will not generate any hardware
 *		  reads or writes.
 *
 *  This function must be called by a driver in order to use the rest
 *  of the 'shared' code files. Called by drivers only.
 **/
s32 igc_setup_init_funcs(struct igc_hw *hw, bool init_device)
{
	s32 ret_val;

	/* Can't do much good without knowing the MAC type. */
	ret_val = igc_set_mac_type(hw);
	if (ret_val) {
		DEBUGOUT("ERROR: MAC type could not be set properly.\n");
		goto out;
	}

	if (!hw->hw_addr) {
		DEBUGOUT("ERROR: Registers not mapped\n");
		ret_val = -IGC_ERR_CONFIG;
		goto out;
	}

	/*
	 * Init function pointers to generic implementations. We do this first
	 * allowing a driver module to override it afterward.
	 */
	igc_init_mac_ops_generic(hw);
	igc_init_phy_ops_generic(hw);
	igc_init_nvm_ops_generic(hw);

	/*
	 * Set up the init function pointers. These are functions within the
	 * adapter family file that sets up function pointers for the rest of
	 * the functions in that family.
	 */
	switch (hw->mac.type) {
	case igc_i225:
		igc_init_function_pointers_i225(hw);
		break;
	default:
		DEBUGOUT("Hardware not supported\n");
		ret_val = -IGC_ERR_CONFIG;
		break;
	}

	/*
	 * Initialize the rest of the function pointers. These require some
	 * register reads/writes in some cases.
	 */
	if (!(ret_val) && init_device) {
		ret_val = igc_init_mac_params(hw);
		if (ret_val)
			goto out;

		ret_val = igc_init_nvm_params(hw);
		if (ret_val)
			goto out;

		ret_val = igc_init_phy_params(hw);
		if (ret_val)
			goto out;
	}

out:
	return ret_val;
}

/**
 *  igc_get_bus_info - Obtain bus information for adapter
 *  @hw: pointer to the HW structure
 *
 *  This will obtain information about the HW bus for which the
 *  adapter is attached and stores it in the hw structure. This is a
 *  function pointer entry point called by drivers.
 **/
s32 igc_get_bus_info(struct igc_hw *hw)
{
	if (hw->mac.ops.get_bus_info)
		return hw->mac.ops.get_bus_info(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_clear_vfta - Clear VLAN filter table
 *  @hw: pointer to the HW structure
 *
 *  This clears the VLAN filter table on the adapter. This is a function
 *  pointer entry point called by drivers.
 **/
void igc_clear_vfta(struct igc_hw *hw)
{
	if (hw->mac.ops.clear_vfta)
		hw->mac.ops.clear_vfta(hw);
}

/**
 *  igc_write_vfta - Write value to VLAN filter table
 *  @hw: pointer to the HW structure
 *  @offset: the 32-bit offset in which to write the value to.
 *  @value: the 32-bit value to write at location offset.
 *
 *  This writes a 32-bit value to a 32-bit offset in the VLAN filter
 *  table. This is a function pointer entry point called by drivers.
 **/
void igc_write_vfta(struct igc_hw *hw, u32 offset, u32 value)
{
	if (hw->mac.ops.write_vfta)
		hw->mac.ops.write_vfta(hw, offset, value);
}

/**
 *  igc_update_mc_addr_list - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *
 *  Updates the Multicast Table Array.
 *  The caller must have a packed mc_addr_list of multicast addresses.
 **/
void igc_update_mc_addr_list(struct igc_hw *hw, u8 *mc_addr_list,
			       u32 mc_addr_count)
{
	if (hw->mac.ops.update_mc_addr_list)
		hw->mac.ops.update_mc_addr_list(hw, mc_addr_list,
						mc_addr_count);
}

/**
 *  igc_force_mac_fc - Force MAC flow control
 *  @hw: pointer to the HW structure
 *
 *  Force the MAC's flow control settings. Currently no func pointer exists
 *  and all implementations are handled in the generic version of this
 *  function.
 **/
s32 igc_force_mac_fc(struct igc_hw *hw)
{
	return igc_force_mac_fc_generic(hw);
}

/**
 *  igc_check_for_link - Check/Store link connection
 *  @hw: pointer to the HW structure
 *
 *  This checks the link condition of the adapter and stores the
 *  results in the hw->mac structure. This is a function pointer entry
 *  point called by drivers.
 **/
s32 igc_check_for_link(struct igc_hw *hw)
{
	if (hw->mac.ops.check_for_link)
		return hw->mac.ops.check_for_link(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_reset_hw - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets the hardware into a known state. This is a function pointer
 *  entry point called by drivers.
 **/
s32 igc_reset_hw(struct igc_hw *hw)
{
	if (hw->mac.ops.reset_hw)
		return hw->mac.ops.reset_hw(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_init_hw - Initialize hardware
 *  @hw: pointer to the HW structure
 *
 *  This inits the hardware readying it for operation. This is a function
 *  pointer entry point called by drivers.
 **/
s32 igc_init_hw(struct igc_hw *hw)
{
	if (hw->mac.ops.init_hw)
		return hw->mac.ops.init_hw(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_setup_link - Configures link and flow control
 *  @hw: pointer to the HW structure
 *
 *  This configures link and flow control settings for the adapter. This
 *  is a function pointer entry point called by drivers. While modules can
 *  also call this, they probably call their own version of this function.
 **/
s32 igc_setup_link(struct igc_hw *hw)
{
	if (hw->mac.ops.setup_link)
		return hw->mac.ops.setup_link(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_get_speed_and_duplex - Returns current speed and duplex
 *  @hw: pointer to the HW structure
 *  @speed: pointer to a 16-bit value to store the speed
 *  @duplex: pointer to a 16-bit value to store the duplex.
 *
 *  This returns the speed and duplex of the adapter in the two 'out'
 *  variables passed in. This is a function pointer entry point called
 *  by drivers.
 **/
s32 igc_get_speed_and_duplex(struct igc_hw *hw, u16 *speed, u16 *duplex)
{
	if (hw->mac.ops.get_link_up_info)
		return hw->mac.ops.get_link_up_info(hw, speed, duplex);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_disable_pcie_master - Disable PCI-Express master access
 *  @hw: pointer to the HW structure
 *
 *  Disables PCI-Express master access and verifies there are no pending
 *  requests. Currently no func pointer exists and all implementations are
 *  handled in the generic version of this function.
 **/
s32 igc_disable_pcie_master(struct igc_hw *hw)
{
	return igc_disable_pcie_master_generic(hw);
}

/**
 *  igc_config_collision_dist - Configure collision distance
 *  @hw: pointer to the HW structure
 *
 *  Configures the collision distance to the default value and is used
 *  during link setup.
 **/
void igc_config_collision_dist(struct igc_hw *hw)
{
	if (hw->mac.ops.config_collision_dist)
		hw->mac.ops.config_collision_dist(hw);
}

/**
 *  igc_rar_set - Sets a receive address register
 *  @hw: pointer to the HW structure
 *  @addr: address to set the RAR to
 *  @index: the RAR to set
 *
 *  Sets a Receive Address Register (RAR) to the specified address.
 **/
int igc_rar_set(struct igc_hw *hw, u8 *addr, u32 index)
{
	if (hw->mac.ops.rar_set)
		return hw->mac.ops.rar_set(hw, addr, index);

	return IGC_SUCCESS;
}

/**
 *  igc_validate_mdi_setting - Ensures valid MDI/MDIX SW state
 *  @hw: pointer to the HW structure
 *
 *  Ensures that the MDI/MDIX SW state is valid.
 **/
s32 igc_validate_mdi_setting(struct igc_hw *hw)
{
	if (hw->mac.ops.validate_mdi_setting)
		return hw->mac.ops.validate_mdi_setting(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_hash_mc_addr - Determines address location in multicast table
 *  @hw: pointer to the HW structure
 *  @mc_addr: Multicast address to hash.
 *
 *  This hashes an address to determine its location in the multicast
 *  table. Currently no func pointer exists and all implementations
 *  are handled in the generic version of this function.
 **/
u32 igc_hash_mc_addr(struct igc_hw *hw, u8 *mc_addr)
{
	return igc_hash_mc_addr_generic(hw, mc_addr);
}

/**
 *  igc_check_reset_block - Verifies PHY can be reset
 *  @hw: pointer to the HW structure
 *
 *  Checks if the PHY is in a state that can be reset or if manageability
 *  has it tied up. This is a function pointer entry point called by drivers.
 **/
s32 igc_check_reset_block(struct igc_hw *hw)
{
	if (hw->phy.ops.check_reset_block)
		return hw->phy.ops.check_reset_block(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_read_phy_reg - Reads PHY register
 *  @hw: pointer to the HW structure
 *  @offset: the register to read
 *  @data: the buffer to store the 16-bit read.
 *
 *  Reads the PHY register and returns the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 igc_read_phy_reg(struct igc_hw *hw, u32 offset, u16 *data)
{
	if (hw->phy.ops.read_reg)
		return hw->phy.ops.read_reg(hw, offset, data);

	return IGC_SUCCESS;
}

/**
 *  igc_write_phy_reg - Writes PHY register
 *  @hw: pointer to the HW structure
 *  @offset: the register to write
 *  @data: the value to write.
 *
 *  Writes the PHY register at offset with the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 igc_write_phy_reg(struct igc_hw *hw, u32 offset, u16 data)
{
	if (hw->phy.ops.write_reg)
		return hw->phy.ops.write_reg(hw, offset, data);

	return IGC_SUCCESS;
}

/**
 *  igc_release_phy - Generic release PHY
 *  @hw: pointer to the HW structure
 *
 *  Return if silicon family does not require a semaphore when accessing the
 *  PHY.
 **/
void igc_release_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.release)
		hw->phy.ops.release(hw);
}

/**
 *  igc_acquire_phy - Generic acquire PHY
 *  @hw: pointer to the HW structure
 *
 *  Return success if silicon family does not require a semaphore when
 *  accessing the PHY.
 **/
s32 igc_acquire_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.acquire)
		return hw->phy.ops.acquire(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_get_phy_info - Retrieves PHY information from registers
 *  @hw: pointer to the HW structure
 *
 *  This function gets some information from various PHY registers and
 *  populates hw->phy values with it. This is a function pointer entry
 *  point called by drivers.
 **/
s32 igc_get_phy_info(struct igc_hw *hw)
{
	if (hw->phy.ops.get_info)
		return hw->phy.ops.get_info(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_phy_hw_reset - Hard PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Performs a hard PHY reset. This is a function pointer entry point called
 *  by drivers.
 **/
s32 igc_phy_hw_reset(struct igc_hw *hw)
{
	if (hw->phy.ops.reset)
		return hw->phy.ops.reset(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_set_d0_lplu_state - Sets low power link up state for D0
 *  @hw: pointer to the HW structure
 *  @active: boolean used to enable/disable lplu
 *
 *  Success returns 0, Failure returns 1
 *
 *  The low power link up (lplu) state is set to the power management level D0
 *  and SmartSpeed is disabled when active is true, else clear lplu for D0
 *  and enable Smartspeed.  LPLU and Smartspeed are mutually exclusive.  LPLU
 *  is used during Dx states where the power conservation is most important.
 *  During driver activity, SmartSpeed should be enabled so performance is
 *  maintained.  This is a function pointer entry point called by drivers.
 **/
s32 igc_set_d0_lplu_state(struct igc_hw *hw, bool active)
{
	if (hw->phy.ops.set_d0_lplu_state)
		return hw->phy.ops.set_d0_lplu_state(hw, active);

	return IGC_SUCCESS;
}

/**
 *  igc_set_d3_lplu_state - Sets low power link up state for D3
 *  @hw: pointer to the HW structure
 *  @active: boolean used to enable/disable lplu
 *
 *  Success returns 0, Failure returns 1
 *
 *  The low power link up (lplu) state is set to the power management level D3
 *  and SmartSpeed is disabled when active is true, else clear lplu for D3
 *  and enable Smartspeed.  LPLU and Smartspeed are mutually exclusive.  LPLU
 *  is used during Dx states where the power conservation is most important.
 *  During driver activity, SmartSpeed should be enabled so performance is
 *  maintained.  This is a function pointer entry point called by drivers.
 **/
s32 igc_set_d3_lplu_state(struct igc_hw *hw, bool active)
{
	if (hw->phy.ops.set_d3_lplu_state)
		return hw->phy.ops.set_d3_lplu_state(hw, active);

	return IGC_SUCCESS;
}

/**
 *  igc_read_mac_addr - Reads MAC address
 *  @hw: pointer to the HW structure
 *
 *  Reads the MAC address out of the adapter and stores it in the HW structure.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 igc_read_mac_addr(struct igc_hw *hw)
{
	if (hw->mac.ops.read_mac_addr)
		return hw->mac.ops.read_mac_addr(hw);

	return igc_read_mac_addr_generic(hw);
}

/**
 *  igc_read_pba_string - Read device part number string
 *  @hw: pointer to the HW structure
 *  @pba_num: pointer to device part number
 *  @pba_num_size: size of part number buffer
 *
 *  Reads the product board assembly (PBA) number from the EEPROM and stores
 *  the value in pba_num.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 igc_read_pba_string(struct igc_hw *hw, u8 *pba_num, u32 pba_num_size)
{
	return igc_read_pba_string_generic(hw, pba_num, pba_num_size);
}

/**
 *  igc_validate_nvm_checksum - Verifies NVM (EEPROM) checksum
 *  @hw: pointer to the HW structure
 *
 *  Validates the NVM checksum is correct. This is a function pointer entry
 *  point called by drivers.
 **/
s32 igc_validate_nvm_checksum(struct igc_hw *hw)
{
	if (hw->nvm.ops.validate)
		return hw->nvm.ops.validate(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_update_nvm_checksum - Updates NVM (EEPROM) checksum
 *  @hw: pointer to the HW structure
 *
 *  Updates the NVM checksum. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
s32 igc_update_nvm_checksum(struct igc_hw *hw)
{
	if (hw->nvm.ops.update)
		return hw->nvm.ops.update(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_reload_nvm - Reloads EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Reloads the EEPROM by setting the "Reinitialize from EEPROM" bit in the
 *  extended control register.
 **/
void igc_reload_nvm(struct igc_hw *hw)
{
	if (hw->nvm.ops.reload)
		hw->nvm.ops.reload(hw);
}

/**
 *  igc_read_nvm - Reads NVM (EEPROM)
 *  @hw: pointer to the HW structure
 *  @offset: the word offset to read
 *  @words: number of 16-bit words to read
 *  @data: pointer to the properly sized buffer for the data.
 *
 *  Reads 16-bit chunks of data from the NVM (EEPROM). This is a function
 *  pointer entry point called by drivers.
 **/
s32 igc_read_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data)
{
	if (hw->nvm.ops.read)
		return hw->nvm.ops.read(hw, offset, words, data);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_write_nvm - Writes to NVM (EEPROM)
 *  @hw: pointer to the HW structure
 *  @offset: the word offset to read
 *  @words: number of 16-bit words to write
 *  @data: pointer to the properly sized buffer for the data.
 *
 *  Writes 16-bit chunks of data to the NVM (EEPROM). This is a function
 *  pointer entry point called by drivers.
 **/
s32 igc_write_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data)
{
	if (hw->nvm.ops.write)
		return hw->nvm.ops.write(hw, offset, words, data);

	return IGC_SUCCESS;
}

/**
 * igc_power_up_phy - Restores link in case of PHY power down
 * @hw: pointer to the HW structure
 *
 * The phy may be powered down to save power, to turn off link when the
 * driver is unloaded, or wake on lan is not enabled (among others).
 **/
void igc_power_up_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.power_up)
		hw->phy.ops.power_up(hw);

	igc_setup_link(hw);
}

/**
 * igc_power_down_phy - Power down PHY
 * @hw: pointer to the HW structure
 *
 * The phy may be powered down to save power, to turn off link when the
 * driver is unloaded, or wake on lan is not enabled (among others).
 **/
void igc_power_down_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.power_down)
		hw->phy.ops.power_down(hw);
}

