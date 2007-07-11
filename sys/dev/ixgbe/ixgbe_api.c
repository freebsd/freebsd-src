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

#include "ixgbe_api.h"
#include "ixgbe_common.h"

extern s32 ixgbe_init_shared_code_82598(struct ixgbe_hw *hw);
extern s32 ixgbe_init_shared_code_phy(struct ixgbe_hw *hw);

/**
 *  ixgbe_init_shared_code - Initialize the shared code
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers and assign the MAC type and PHY code.
 *  Does not touch the hardware. This function must be called prior to any
 *  other function in the shared code. The ixgbe_hw structure should be
 *  memset to 0 prior to calling this function.  The following fields in
 *  hw structure should be filled in prior to calling this function:
 *  hw_addr, back, device_id, vendor_id, subsystem_device_id,
 *   subsystem_vendor_id, and revision_id
 **/
s32 ixgbe_init_shared_code(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_ERR_DEVICE_NOT_SUPPORTED;

	/*
	 * Assign generic function pointers before entering adapter-specific
	 * init
	 */
	ixgbe_assign_func_pointers_generic(hw);

	if (hw->vendor_id == IXGBE_INTEL_VENDOR_ID) {
		switch (hw->device_id) {
		case IXGBE_DEV_ID_82598:
		case IXGBE_DEV_ID_82598_FPGA:
		case IXGBE_DEV_ID_82598AF_SINGLE_PORT:
		case IXGBE_DEV_ID_82598AF_DUAL_PORT:
		case IXGBE_DEV_ID_82598AT_DUAL_PORT:
			status = ixgbe_init_shared_code_82598(hw);
			status = ixgbe_init_shared_code_phy(hw);
			break;
		default:
			status = IXGBE_ERR_DEVICE_NOT_SUPPORTED;
			break;
		}
	}

	return status;
}

/**
 *  ixgbe_init_hw - Initialize the hardware
 *  @hw: pointer to hardware structure
 *
 *  Initialize the hardware by resetting and then starting the hardware
 **/
s32 ixgbe_init_hw(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_init_hw, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_reset_hw - Performs a hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks and
 *  clears all interrupts, performs a PHY reset, and performs a MAC reset
 **/
s32 ixgbe_reset_hw(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_reset_hw, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_start_hw - Prepares hardware for TX/TX
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware by filling the bus info structure and media type,
 *  clears all on chip counters, initializes receive address registers,
 *  multicast table, VLAN filter table, calls routine to setup link and
 *  flow control settings, and leaves transmit and receive units disabled
 *  and uninitialized.
 **/
s32 ixgbe_start_hw(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_start_hw, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_clear_hw_cntrs - Clear hardware counters
 *  @hw: pointer to hardware structure
 *
 *  Clears all hardware statistics counters by reading them from the hardware
 *  Statistics counters are clear on read.
 **/
s32 ixgbe_clear_hw_cntrs(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_clear_hw_cntrs, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_get_media_type - Get media type
 *  @hw: pointer to hardware structure
 *
 *  Returns the media type (fiber, copper, backplane)
 **/
enum ixgbe_media_type ixgbe_get_media_type(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_get_media_type, (hw),
			       ixgbe_media_type_unknown);
}

/**
 *  ixgbe_get_mac_addr - Get MAC address
 *  @hw: pointer to hardware structure
 *  @mac_addr: Adapter MAC address
 *
 *  Reads the adapter's MAC address from the first Receive Address Register
 *  (RAR0) A reset of the adapter must have been performed prior to calling this
 *  function in order for the MAC address to have been loaded from the EEPROM
 *  into RAR0
 **/
s32 ixgbe_get_mac_addr(struct ixgbe_hw *hw, u8 *mac_addr)
{
	return ixgbe_call_func(hw, ixgbe_func_get_mac_addr,
			       (hw, mac_addr), IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_get_bus_info - Set PCI bus info
 *  @hw: pointer to hardware structure
 *
 *  Sets the PCI bus info (speed, width, type) within the ixgbe_hw structure
 **/
s32 ixgbe_get_bus_info(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_get_bus_info, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_get_num_of_tx_queues - Get TX queues
 *  @hw: pointer to hardware structure
 *
 *  Returns the number of transmit queues for the given adapter.
 **/
u32 ixgbe_get_num_of_tx_queues(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_get_num_of_tx_queues,
			       (hw), 0);
}

/**
 *  ixgbe_get_num_of_rx_queues - Get RX queues
 *  @hw: pointer to hardware structure
 *
 *  Returns the number of receive queues for the given adapter.
 **/
u32 ixgbe_get_num_of_rx_queues(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_get_num_of_rx_queues,
			       (hw), 0);
}

/**
 *  ixgbe_stop_adapter - Disable TX/TX units
 *  @hw: pointer to hardware structure
 *
 *  Sets the adapter_stopped flag within ixgbe_hw struct. Clears interrupts,
 *  disables transmit and receive units. The adapter_stopped flag is used by
 *  the shared code and drivers to determine if the adapter is in a stopped
 *  state and should not touch the hardware.
 **/
s32 ixgbe_stop_adapter(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_stop_adapter, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_identify_phy - Get PHY type
 *  @hw: pointer to hardware structure
 *
 *  Determines the physical layer module found on the current adapter.
 **/
s32 ixgbe_identify_phy(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_SUCCESS;

	if (hw->phy.type == ixgbe_phy_unknown) {
		status = ixgbe_call_func(hw,
					 ixgbe_func_identify_phy,
					 (hw),
					 IXGBE_NOT_IMPLEMENTED);
	}

	return status;
}

/**
 *  ixgbe_reset_phy - Perform a PHY reset
 *  @hw: pointer to hardware structure
 **/
s32 ixgbe_reset_phy(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_SUCCESS;

	if (hw->phy.type == ixgbe_phy_unknown) {
		if (ixgbe_identify_phy(hw) != IXGBE_SUCCESS) {
		    status = IXGBE_ERR_PHY;
		}
	}

	if (status == IXGBE_SUCCESS) {
		status = ixgbe_call_func(hw,
					 ixgbe_func_reset_phy,
					 (hw),
					 IXGBE_NOT_IMPLEMENTED);
	}
	return status;
}

/**
 *  ixgbe_read_phy_reg - Read PHY register
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit address of PHY register to read
 *  @phy_data: Pointer to read data from PHY register
 *
 *  Reads a value from a specified PHY register
 **/
s32 ixgbe_read_phy_reg(struct ixgbe_hw *hw, u32 reg_addr, u32 device_type,
		       u16 *phy_data)
{
	s32 status = IXGBE_SUCCESS;

	if (hw->phy.type == ixgbe_phy_unknown) {
		if (ixgbe_identify_phy(hw) != IXGBE_SUCCESS) {
		    status = IXGBE_ERR_PHY;
		}
	}

	if (status == IXGBE_SUCCESS) {
		status = ixgbe_call_func(hw,
					 ixgbe_func_read_phy_reg,
					 (hw, reg_addr, device_type, phy_data),
					 IXGBE_NOT_IMPLEMENTED);
	}
	return status;
}

/**
 *  ixgbe_write_phy_reg - Write PHY register
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @phy_data: Data to write to the PHY register
 *
 *  Writes a value to specified PHY register
 **/
s32 ixgbe_write_phy_reg(struct ixgbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 phy_data)
{
	s32 status = IXGBE_SUCCESS;

	if (hw->phy.type == ixgbe_phy_unknown) {
		if (ixgbe_identify_phy(hw) != IXGBE_SUCCESS) {
		    status = IXGBE_ERR_PHY;
		}
	}

	if (status == IXGBE_SUCCESS) {
		status = ixgbe_call_func(hw,
					 ixgbe_func_write_phy_reg,
					 (hw, reg_addr, device_type, phy_data),
					 IXGBE_NOT_IMPLEMENTED);
	}
	return status;
}

/**
 *  ixgbe_setup_link - Configure link settings
 *  @hw: pointer to hardware structure
 *
 *  Configures link settings based on values in the ixgbe_hw struct.
 *  Restarts the link.  Performs autonegotiation if needed.
 **/
s32 ixgbe_setup_link(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_setup_link, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_check_link - Get link and speed status
 *  @hw: pointer to hardware structure
 *
 *  Reads the links register to determine if link is up and the current speed
 **/
s32 ixgbe_check_link(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
		     bool *link_up)
{
	return ixgbe_call_func(hw, ixgbe_func_check_link, (hw, speed, link_up),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_setup_link_speed - Set link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *
 *  Set the link speed and restarts the link.
 **/
s32 ixgbe_setup_link_speed(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			   bool autoneg,
			   bool autoneg_wait_to_complete)
{
	return ixgbe_call_func(hw, ixgbe_func_setup_link_speed, (hw, speed,
			       autoneg, autoneg_wait_to_complete),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_get_link_settings - Set link settings to default
 *  @hw: pointer to hardware structure
 *
 *  Sets the default link settings based on attach type in the hw struct.
 **/
s32 ixgbe_get_link_settings(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
			    bool *autoneg)
{
	return ixgbe_call_func(hw, ixgbe_func_get_link_settings, (hw, speed,
			       autoneg), IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_led_on - Turn on LED's
 *  @hw: pointer to hardware structure
 *  @index: led number to turn on
 *
 *  Turns on the software controllable LEDs.
 **/
s32 ixgbe_led_on(struct ixgbe_hw *hw, u32 index)
{
	return ixgbe_call_func(hw, ixgbe_func_led_on, (hw, index),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_led_off - Turn off LED's
 *  @hw: pointer to hardware structure
 *  @index: led number to turn off
 *
 *  Turns off the software controllable LEDs.
 **/
s32 ixgbe_led_off(struct ixgbe_hw *hw, u32 index)
{
	return ixgbe_call_func(hw, ixgbe_func_led_off, (hw, index),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_blink_led_start - Blink LED's
 *  @hw: pointer to hardware structure
 *  @index: led number to blink
 *
 *  Blink LED based on index.
 **/
s32 ixgbe_blink_led_start(struct ixgbe_hw *hw, u32 index)
{
	return ixgbe_call_func(hw, ixgbe_func_blink_led_start, (hw, index),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_blink_led_stop - Stop blinking LED's
 *  @hw: pointer to hardware structure
 *
 *  Stop blinking LED based on index.
 **/
s32 ixgbe_blink_led_stop(struct ixgbe_hw *hw, u32 index)
{
	return ixgbe_call_func(hw, ixgbe_func_blink_led_stop, (hw, index),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_init_eeprom_params - Initialiaze EEPROM parameters
 *  @hw: pointer to hardware structure
 *
 *  Initializes the EEPROM parameters ixgbe_eeprom_info within the
 *  ixgbe_hw struct in order to set up EEPROM access.
 **/
s32 ixgbe_init_eeprom_params(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_init_eeprom_params, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}


/**
 *  ixgbe_write_eeprom - Write word to EEPROM
 *  @hw: pointer to hardware structure
 *  @offset: offset within the EEPROM to be written to
 *  @data: 16 bit word to be written to the EEPROM
 *
 *  Writes 16 bit value to EEPROM. If ixgbe_eeprom_update_checksum is not
 *  called after this function, the EEPROM will most likely contain an
 *  invalid checksum.
 **/
s32 ixgbe_write_eeprom(struct ixgbe_hw *hw, u16 offset, u16 data)
{
	s32 status;

	/*
	 * Initialize EEPROM parameters.  This will not do anything if the
	 * EEPROM structure has already been initialized
	 */
	ixgbe_init_eeprom_params(hw);

	/* Check for invalid offset */
	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
	} else {
		status = ixgbe_call_func(hw,
					 ixgbe_func_write_eeprom,
					 (hw, offset, data),
					 IXGBE_NOT_IMPLEMENTED);
	}

	return status;
}

/**
 *  ixgbe_read_eeprom - Read word from EEPROM
 *  @hw: pointer to hardware structure
 *  @offset: offset within the EEPROM to be read
 *  @data: read 16 bit value from EEPROM
 *
 *  Reads 16 bit value from EEPROM
 **/
s32 ixgbe_read_eeprom(struct ixgbe_hw *hw, u16 offset, u16 *data)
{
	s32 status;

	/*
	 * Initialize EEPROM parameters.  This will not do anything if the
	 * EEPROM structure has already been initialized
	 */
	ixgbe_init_eeprom_params(hw);

	/* Check for invalid offset */
	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
	} else {
		status = ixgbe_call_func(hw,
					 ixgbe_func_read_eeprom,
					 (hw, offset, data),
					 IXGBE_NOT_IMPLEMENTED);
	}

	return status;
}

/**
 *  ixgbe_validate_eeprom_checksum - Validate EEPROM checksum
 *  @hw: pointer to hardware structure
 *  @checksum_val: calculated checksum
 *
 *  Performs checksum calculation and validates the EEPROM checksum
 **/
s32 ixgbe_validate_eeprom_checksum(struct ixgbe_hw *hw, u16 *checksum_val)
{
	return ixgbe_call_func(hw, ixgbe_func_validate_eeprom_checksum,
			       (hw, checksum_val), IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_eeprom_update_checksum - Updates the EEPROM checksum
 *  @hw: pointer to hardware structure
 **/
s32 ixgbe_update_eeprom_checksum(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_update_eeprom_checksum, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_set_rar - Set RX address register
 *  @hw: pointer to hardware structure
 *  @addr: Address to put into receive address register
 *  @index: Receive address register to write
 *  @vind: Vind to set RAR to
 *  @enable_addr: set flag that address is active
 *
 *  Puts an ethernet address into a receive address register.
 **/
s32 ixgbe_set_rar(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vind,
		  u32 enable_addr)
{
	return ixgbe_call_func(hw, ixgbe_func_set_rar, (hw, index, addr, vind,
			       enable_addr), IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_init_rx_addrs - Initializes receive address filters.
 *  @hw: pointer to hardware structure
 *
 *  Places the MAC address in receive address register 0 and clears the rest
 *  of the receive addresss registers. Clears the multicast table. Assumes
 *  the receiver is in reset when the routine is called.
 **/
s32 ixgbe_init_rx_addrs(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_init_rx_addrs, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_get_num_rx_addrs - Returns the number of RAR entries.
 *  @hw: pointer to hardware structure
 **/
u32 ixgbe_get_num_rx_addrs(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_get_num_rx_addrs, (hw), 0);
}

/**
 *  ixgbe_update_mc_addr_list - Updates the MAC's list of multicast addresses
 *  @hw: pointer to hardware structure
 *  @mc_addr_list: the list of new multicast addresses
 *  @mc_addr_count: number of addresses
 *  @pad: number of bytes between addresses in the list
 *
 *  The given list replaces any existing list. Clears the MC addrs from receive
 *  address registers and the multicast table. Uses unsed receive address
 *  registers for the first multicast addresses, and hashes the rest into the
 *  multicast table.
 **/
s32 ixgbe_update_mc_addr_list(struct ixgbe_hw *hw, u8 *mc_addr_list,
			      u32 mc_addr_count, u32 pad)
{
	return ixgbe_call_func(hw, ixgbe_func_update_mc_addr_list, (hw,
			       mc_addr_list, mc_addr_count,  pad),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_enable_mc - Enable multicast address in RAR
 *  @hw: pointer to hardware structure
 *
 *  Enables multicast address in RAR and the use of the multicast hash table.
 **/
s32 ixgbe_enable_mc(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_enable_mc, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_disable_mc - Disable multicast address in RAR
 *  @hw: pointer to hardware structure
 *
 *  Disables multicast address in RAR and the use of the multicast hash table.
 **/
s32 ixgbe_disable_mc(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_disable_mc, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_clear_vfta - Clear VLAN filter table
 *  @hw: pointer to hardware structure
 *
 *  Clears the VLAN filer table, and the VMDq index associated with the filter
 **/
s32 ixgbe_clear_vfta(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_clear_vfta, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_set_vfta - Set VLAN filter table
 *  @hw: pointer to hardware structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vind: VMDq output index that maps queue to VLAN id in VFTA
 *  @vlan_on: boolean flag to turn on/off VLAN in VFTA
 *
 *  Turn on/off specified VLAN in the VLAN filter table.
 **/
s32 ixgbe_set_vfta(struct ixgbe_hw *hw, u32 vlan, u32 vind, bool vlan_on)
{
	return ixgbe_call_func(hw, ixgbe_func_set_vfta, (hw, vlan, vind,
			       vlan_on), IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_setup_fc - Set flow control
 *  @hw: pointer to hardware structure
 *  @packetbuf_num: packet buffer number (0-7)
 *
 *  Configures the flow control settings based on SW configuration.
 **/
s32 ixgbe_setup_fc(struct ixgbe_hw *hw, s32 packetbuf_num)
{
	return ixgbe_call_func(hw, ixgbe_func_setup_fc, (hw, packetbuf_num),
			       IXGBE_NOT_IMPLEMENTED);
}

