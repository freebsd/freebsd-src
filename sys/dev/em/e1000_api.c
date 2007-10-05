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

#include "e1000_api.h"
#include "e1000_mac.h"
#include "e1000_nvm.h"
#include "e1000_phy.h"

#ifndef NO_82542_SUPPORT
extern void    e1000_init_function_pointers_82542(struct e1000_hw *hw);
#endif
extern void    e1000_init_function_pointers_82543(struct e1000_hw *hw);
extern void    e1000_init_function_pointers_82540(struct e1000_hw *hw);
extern void    e1000_init_function_pointers_82571(struct e1000_hw *hw);
extern void    e1000_init_function_pointers_82541(struct e1000_hw *hw);
extern void    e1000_init_function_pointers_80003es2lan(struct e1000_hw *hw);
extern void    e1000_init_function_pointers_ich8lan(struct e1000_hw *hw);
extern void    e1000_init_function_pointers_82575(struct e1000_hw *hw);

/**
 *  e1000_init_mac_params - Initialize MAC function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the MAC
 *  set of functions.  Called by drivers or by e1000_setup_init_funcs.
 **/
s32 e1000_init_mac_params(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	if (hw->func.init_mac_params) {
		ret_val = hw->func.init_mac_params(hw);
		if (ret_val) {
			DEBUGOUT("MAC Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("mac.init_mac_params was NULL\n");
		ret_val = -E1000_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  e1000_init_nvm_params - Initialize NVM function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the NVM
 *  set of functions.  Called by drivers or by e1000_setup_init_funcs.
 **/
s32 e1000_init_nvm_params(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	if (hw->func.init_nvm_params) {
		ret_val = hw->func.init_nvm_params(hw);
		if (ret_val) {
			DEBUGOUT("NVM Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("nvm.init_nvm_params was NULL\n");
		ret_val = -E1000_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  e1000_init_phy_params - Initialize PHY function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the PHY
 *  set of functions.  Called by drivers or by e1000_setup_init_funcs.
 **/
s32 e1000_init_phy_params(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	if (hw->func.init_phy_params) {
		ret_val = hw->func.init_phy_params(hw);
		if (ret_val) {
			DEBUGOUT("PHY Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("phy.init_phy_params was NULL\n");
		ret_val =  -E1000_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  e1000_set_mac_type - Sets MAC type
 *  @hw: pointer to the HW structure
 *
 *  This function sets the mac type of the adapter based on the
 *  device ID stored in the hw structure.
 *  MUST BE FIRST FUNCTION CALLED (explicitly or through
 *  e1000_setup_init_funcs()).
 **/
s32 e1000_set_mac_type(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_set_mac_type");

	switch (hw->device_id) {
#ifndef NO_82542_SUPPORT
	case E1000_DEV_ID_82542:
		mac->type = e1000_82542;
		break;
#endif
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		mac->type = e1000_82543;
		break;
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		mac->type = e1000_82544;
		break;
	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82540EM_LOM:
	case E1000_DEV_ID_82540EP:
	case E1000_DEV_ID_82540EP_LOM:
	case E1000_DEV_ID_82540EP_LP:
		mac->type = e1000_82540;
		break;
	case E1000_DEV_ID_82545EM_COPPER:
	case E1000_DEV_ID_82545EM_FIBER:
		mac->type = e1000_82545;
		break;
	case E1000_DEV_ID_82545GM_COPPER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDES:
		mac->type = e1000_82545_rev_3;
		break;
	case E1000_DEV_ID_82546EB_COPPER:
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546EB_QUAD_COPPER:
		mac->type = e1000_82546;
		break;
	case E1000_DEV_ID_82546GB_COPPER:
	case E1000_DEV_ID_82546GB_FIBER:
	case E1000_DEV_ID_82546GB_SERDES:
	case E1000_DEV_ID_82546GB_PCIE:
	case E1000_DEV_ID_82546GB_QUAD_COPPER:
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		mac->type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541EI:
	case E1000_DEV_ID_82541EI_MOBILE:
	case E1000_DEV_ID_82541ER_LOM:
		mac->type = e1000_82541;
		break;
	case E1000_DEV_ID_82541ER:
	case E1000_DEV_ID_82541GI:
	case E1000_DEV_ID_82541GI_LF:
	case E1000_DEV_ID_82541GI_MOBILE:
		mac->type = e1000_82541_rev_2;
		break;
	case E1000_DEV_ID_82547EI:
	case E1000_DEV_ID_82547EI_MOBILE:
		mac->type = e1000_82547;
		break;
	case E1000_DEV_ID_82547GI:
		mac->type = e1000_82547_rev_2;
		break;
	case E1000_DEV_ID_82571EB_COPPER:
	case E1000_DEV_ID_82571EB_FIBER:
	case E1000_DEV_ID_82571EB_SERDES:
	case E1000_DEV_ID_82571EB_SERDES_DUAL:
	case E1000_DEV_ID_82571EB_SERDES_QUAD:
	case E1000_DEV_ID_82571EB_QUAD_COPPER:
	case E1000_DEV_ID_82571PT_QUAD_COPPER:
	case E1000_DEV_ID_82571EB_QUAD_FIBER:
	case E1000_DEV_ID_82571EB_QUAD_COPPER_LP:
		mac->type = e1000_82571;
		break;
	case E1000_DEV_ID_82572EI:
	case E1000_DEV_ID_82572EI_COPPER:
	case E1000_DEV_ID_82572EI_FIBER:
	case E1000_DEV_ID_82572EI_SERDES:
		mac->type = e1000_82572;
		break;
	case E1000_DEV_ID_82573E:
	case E1000_DEV_ID_82573E_IAMT:
	case E1000_DEV_ID_82573L:
		mac->type = e1000_82573;
		break;
	case E1000_DEV_ID_80003ES2LAN_COPPER_DPT:
	case E1000_DEV_ID_80003ES2LAN_SERDES_DPT:
	case E1000_DEV_ID_80003ES2LAN_COPPER_SPT:
	case E1000_DEV_ID_80003ES2LAN_SERDES_SPT:
		mac->type = e1000_80003es2lan;
		break;
	case E1000_DEV_ID_ICH8_IFE:
	case E1000_DEV_ID_ICH8_IFE_GT:
	case E1000_DEV_ID_ICH8_IFE_G:
	case E1000_DEV_ID_ICH8_IGP_M:
	case E1000_DEV_ID_ICH8_IGP_M_AMT:
	case E1000_DEV_ID_ICH8_IGP_AMT:
	case E1000_DEV_ID_ICH8_IGP_C:
		mac->type = e1000_ich8lan;
		break;
	case E1000_DEV_ID_ICH9_IFE:
	case E1000_DEV_ID_ICH9_IFE_GT:
	case E1000_DEV_ID_ICH9_IFE_G:
	case E1000_DEV_ID_ICH9_IGP_AMT:
	case E1000_DEV_ID_ICH9_IGP_C:
		mac->type = e1000_ich9lan;
		break;
	case E1000_DEV_ID_82575EB_COPPER:
	case E1000_DEV_ID_82575EB_FIBER_SERDES:
	case E1000_DEV_ID_82575GB_QUAD_COPPER:
		mac->type = e1000_82575;
		break;
	default:
		/* Should never have loaded on this device */
		ret_val = -E1000_ERR_MAC_INIT;
		break;
	}

	return ret_val;
}

/**
 *  e1000_setup_init_funcs - Initializes function pointers
 *  @hw: pointer to the HW structure
 *  @init_device: TRUE will initialize the rest of the function pointers
 *                 getting the device ready for use.  FALSE will only set
 *                 MAC type and the function pointers for the other init
 *                 functions.  Passing FALSE will not generate any hardware
 *                 reads or writes.
 *
 *  This function must be called by a driver in order to use the rest
 *  of the 'shared' code files. Called by drivers only.
 **/
s32 e1000_setup_init_funcs(struct e1000_hw *hw, bool init_device)
{
	s32 ret_val;

	/* Can't do much good without knowing the MAC type. */
	ret_val = e1000_set_mac_type(hw);
	if (ret_val) {
		DEBUGOUT("ERROR: MAC type could not be set properly.\n");
		goto out;
	}

	if (!hw->hw_addr) {
		DEBUGOUT("ERROR: Registers not mapped\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	/*
	 * Init some generic function pointers that are currently all pointing
	 * to generic implementations. We do this first allowing a driver
	 * module to override it afterwards.
	 */
	hw->func.config_collision_dist = e1000_config_collision_dist_generic;
	hw->func.rar_set = e1000_rar_set_generic;
	hw->func.validate_mdi_setting = e1000_validate_mdi_setting_generic;
	hw->func.mng_host_if_write = e1000_mng_host_if_write_generic;
	hw->func.mng_write_cmd_header = e1000_mng_write_cmd_header_generic;
	hw->func.mng_enable_host_if = e1000_mng_enable_host_if_generic;
	hw->func.wait_autoneg = e1000_wait_autoneg_generic;
	hw->func.reload_nvm = e1000_reload_nvm_generic;

	/*
	 * Set up the init function pointers. These are functions within the
	 * adapter family file that sets up function pointers for the rest of
	 * the functions in that family.
	 */
	switch (hw->mac.type) {
#ifndef NO_82542_SUPPORT
	case e1000_82542:
		e1000_init_function_pointers_82542(hw);
		break;
#endif
	case e1000_82543:
	case e1000_82544:
		e1000_init_function_pointers_82543(hw);
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		e1000_init_function_pointers_82540(hw);
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		e1000_init_function_pointers_82541(hw);
		break;
	case e1000_82571:
	case e1000_82572:
	case e1000_82573:
		e1000_init_function_pointers_82571(hw);
		break;
	case e1000_80003es2lan:
		e1000_init_function_pointers_80003es2lan(hw);
		break;
	case e1000_ich8lan:
	case e1000_ich9lan:
		e1000_init_function_pointers_ich8lan(hw);
		break;
	case e1000_82575:
		e1000_init_function_pointers_82575(hw);
		break;
	default:
		DEBUGOUT("Hardware not supported\n");
		ret_val = -E1000_ERR_CONFIG;
		break;
	}

	/*
	 * Initialize the rest of the function pointers. These require some
	 * register reads/writes in some cases.
	 */
	if (!(ret_val) && init_device) {
		ret_val = e1000_init_mac_params(hw);
		if (ret_val)
			goto out;

		ret_val = e1000_init_nvm_params(hw);
		if (ret_val)
			goto out;

		ret_val = e1000_init_phy_params(hw);
		if (ret_val)
			goto out;

	}

out:
	return ret_val;
}

/**
 *  e1000_remove_device - Free device specific structure
 *  @hw: pointer to the HW structure
 *
 *  If a device specific structure was allocated, this function will
 *  free it. This is a function pointer entry point called by drivers.
 **/
void e1000_remove_device(struct e1000_hw *hw)
{
	if (hw->func.remove_device)
		hw->func.remove_device(hw);
}

/**
 *  e1000_get_bus_info - Obtain bus information for adapter
 *  @hw: pointer to the HW structure
 *
 *  This will obtain information about the HW bus for which the
 *  adaper is attached and stores it in the hw structure. This is a
 *  function pointer entry point called by drivers.
 **/
s32 e1000_get_bus_info(struct e1000_hw *hw)
{
	if (hw->func.get_bus_info)
		return hw->func.get_bus_info(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_clear_vfta - Clear VLAN filter table
 *  @hw: pointer to the HW structure
 *
 *  This clears the VLAN filter table on the adapter. This is a function
 *  pointer entry point called by drivers.
 **/
void e1000_clear_vfta(struct e1000_hw *hw)
{
	if (hw->func.clear_vfta)
		hw->func.clear_vfta (hw);
}

/**
 *  e1000_write_vfta - Write value to VLAN filter table
 *  @hw: pointer to the HW structure
 *  @offset: the 32-bit offset in which to write the value to.
 *  @value: the 32-bit value to write at location offset.
 *
 *  This writes a 32-bit value to a 32-bit offset in the VLAN filter
 *  table. This is a function pointer entry point called by drivers.
 **/
void e1000_write_vfta(struct e1000_hw *hw, u32 offset, u32 value)
{
	if (hw->func.write_vfta)
		hw->func.write_vfta(hw, offset, value);
}

/**
 *  e1000_update_mc_addr_list - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *  @rar_used_count: the first RAR register free to program
 *  @rar_count: total number of supported Receive Address Registers
 *
 *  Updates the Receive Address Registers and Multicast Table Array.
 *  The caller must have a packed mc_addr_list of multicast addresses.
 *  The parameter rar_count will usually be hw->mac.rar_entry_count
 *  unless there are workarounds that change this.  Currently no func pointer
 *  exists and all implementations are handled in the generic version of this
 *  function.
 **/
void e1000_update_mc_addr_list(struct e1000_hw *hw, u8 *mc_addr_list,
                               u32 mc_addr_count, u32 rar_used_count,
                               u32 rar_count)
{
	if (hw->func.update_mc_addr_list)
		hw->func.update_mc_addr_list(hw,
		                             mc_addr_list,
		                             mc_addr_count,
		                             rar_used_count,
		                             rar_count);
}

/**
 *  e1000_force_mac_fc - Force MAC flow control
 *  @hw: pointer to the HW structure
 *
 *  Force the MAC's flow control settings. Currently no func pointer exists
 *  and all implementations are handled in the generic version of this
 *  function.
 **/
s32 e1000_force_mac_fc(struct e1000_hw *hw)
{
	return e1000_force_mac_fc_generic(hw);
}

/**
 *  e1000_check_for_link - Check/Store link connection
 *  @hw: pointer to the HW structure
 *
 *  This checks the link condition of the adapter and stores the
 *  results in the hw->mac structure. This is a function pointer entry
 *  point called by drivers.
 **/
s32 e1000_check_for_link(struct e1000_hw *hw)
{
	if (hw->func.check_for_link)
		return hw->func.check_for_link(hw);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_check_mng_mode - Check management mode
 *  @hw: pointer to the HW structure
 *
 *  This checks if the adapter has manageability enabled.
 *  This is a function pointer entry point called by drivers.
 **/
bool e1000_check_mng_mode(struct e1000_hw *hw)
{
	if (hw->func.check_mng_mode)
		return hw->func.check_mng_mode(hw);

	return FALSE;
}

/**
 *  e1000_mng_write_dhcp_info - Writes DHCP info to host interface
 *  @hw: pointer to the HW structure
 *  @buffer: pointer to the host interface
 *  @length: size of the buffer
 *
 *  Writes the DHCP information to the host interface.
 **/
s32 e1000_mng_write_dhcp_info(struct e1000_hw *hw, u8 *buffer, u16 length)
{
	return e1000_mng_write_dhcp_info_generic(hw, buffer, length);
}

/**
 *  e1000_reset_hw - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets the hardware into a known state. This is a function pointer
 *  entry point called by drivers.
 **/
s32 e1000_reset_hw(struct e1000_hw *hw)
{
	if (hw->func.reset_hw)
		return hw->func.reset_hw(hw);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_init_hw - Initialize hardware
 *  @hw: pointer to the HW structure
 *
 *  This inits the hardware readying it for operation. This is a function
 *  pointer entry point called by drivers.
 **/
s32 e1000_init_hw(struct e1000_hw *hw)
{
	if (hw->func.init_hw)
		return hw->func.init_hw(hw);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_setup_link - Configures link and flow control
 *  @hw: pointer to the HW structure
 *
 *  This configures link and flow control settings for the adapter. This
 *  is a function pointer entry point called by drivers. While modules can
 *  also call this, they probably call their own version of this function.
 **/
s32 e1000_setup_link(struct e1000_hw *hw)
{
	if (hw->func.setup_link)
		return hw->func.setup_link(hw);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_get_speed_and_duplex - Returns current speed and duplex
 *  @hw: pointer to the HW structure
 *  @speed: pointer to a 16-bit value to store the speed
 *  @duplex: pointer to a 16-bit value to store the duplex.
 *
 *  This returns the speed and duplex of the adapter in the two 'out'
 *  variables passed in. This is a function pointer entry point called
 *  by drivers.
 **/
s32 e1000_get_speed_and_duplex(struct e1000_hw *hw, u16 *speed, u16 *duplex)
{
	if (hw->func.get_link_up_info)
		return hw->func.get_link_up_info(hw, speed, duplex);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_setup_led - Configures SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This prepares the SW controllable LED for use and saves the current state
 *  of the LED so it can be later restored. This is a function pointer entry
 *  point called by drivers.
 **/
s32 e1000_setup_led(struct e1000_hw *hw)
{
	if (hw->func.setup_led)
		return hw->func.setup_led(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_cleanup_led - Restores SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This restores the SW controllable LED to the value saved off by
 *  e1000_setup_led. This is a function pointer entry point called by drivers.
 **/
s32 e1000_cleanup_led(struct e1000_hw *hw)
{
	if (hw->func.cleanup_led)
		return hw->func.cleanup_led(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_blink_led - Blink SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This starts the adapter LED blinking. Request the LED to be setup first
 *  and cleaned up after. This is a function pointer entry point called by
 *  drivers.
 **/
s32 e1000_blink_led(struct e1000_hw *hw)
{
	if (hw->func.blink_led)
		return hw->func.blink_led(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_led_on - Turn on SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  Turns the SW defined LED on. This is a function pointer entry point
 *  called by drivers.
 **/
s32 e1000_led_on(struct e1000_hw *hw)
{
	if (hw->func.led_on)
		return hw->func.led_on(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_led_off - Turn off SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  Turns the SW defined LED off. This is a function pointer entry point
 *  called by drivers.
 **/
s32 e1000_led_off(struct e1000_hw *hw)
{
	if (hw->func.led_off)
		return hw->func.led_off(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_reset_adaptive - Reset adaptive IFS
 *  @hw: pointer to the HW structure
 *
 *  Resets the adaptive IFS. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
void e1000_reset_adaptive(struct e1000_hw *hw)
{
	e1000_reset_adaptive_generic(hw);
}

/**
 *  e1000_update_adaptive - Update adaptive IFS
 *  @hw: pointer to the HW structure
 *
 *  Updates adapter IFS. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
void e1000_update_adaptive(struct e1000_hw *hw)
{
	e1000_update_adaptive_generic(hw);
}

/**
 *  e1000_disable_pcie_master - Disable PCI-Express master access
 *  @hw: pointer to the HW structure
 *
 *  Disables PCI-Express master access and verifies there are no pending
 *  requests. Currently no func pointer exists and all implementations are
 *  handled in the generic version of this function.
 **/
s32 e1000_disable_pcie_master(struct e1000_hw *hw)
{
	return e1000_disable_pcie_master_generic(hw);
}

/**
 *  e1000_config_collision_dist - Configure collision distance
 *  @hw: pointer to the HW structure
 *
 *  Configures the collision distance to the default value and is used
 *  during link setup.
 **/
void e1000_config_collision_dist(struct e1000_hw *hw)
{
	if (hw->func.config_collision_dist)
		hw->func.config_collision_dist(hw);
}

/**
 *  e1000_rar_set - Sets a receive address register
 *  @hw: pointer to the HW structure
 *  @addr: address to set the RAR to
 *  @index: the RAR to set
 *
 *  Sets a Receive Address Register (RAR) to the specified address.
 **/
void e1000_rar_set(struct e1000_hw *hw, u8 *addr, u32 index)
{
	if (hw->func.rar_set)
		hw->func.rar_set(hw, addr, index);
}

/**
 *  e1000_validate_mdi_setting - Ensures valid MDI/MDIX SW state
 *  @hw: pointer to the HW structure
 *
 *  Ensures that the MDI/MDIX SW state is valid.
 **/
s32 e1000_validate_mdi_setting(struct e1000_hw *hw)
{
	if (hw->func.validate_mdi_setting)
		return hw->func.validate_mdi_setting(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_mta_set - Sets multicast table bit
 *  @hw: pointer to the HW structure
 *  @hash_value: Multicast hash value.
 *
 *  This sets the bit in the multicast table corresponding to the
 *  hash value.  This is a function pointer entry point called by drivers.
 **/
void e1000_mta_set(struct e1000_hw *hw, u32 hash_value)
{
	if (hw->func.mta_set)
		hw->func.mta_set(hw, hash_value);
}

/**
 *  e1000_hash_mc_addr - Determines address location in multicast table
 *  @hw: pointer to the HW structure
 *  @mc_addr: Multicast address to hash.
 *
 *  This hashes an address to determine its location in the multicast
 *  table. Currently no func pointer exists and all implementations
 *  are handled in the generic version of this function.
 **/
u32 e1000_hash_mc_addr(struct e1000_hw *hw, u8 *mc_addr)
{
	return e1000_hash_mc_addr_generic(hw, mc_addr);
}

/**
 *  e1000_enable_tx_pkt_filtering - Enable packet filtering on TX
 *  @hw: pointer to the HW structure
 *
 *  Enables packet filtering on transmit packets if manageability is enabled
 *  and host interface is enabled.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
bool e1000_enable_tx_pkt_filtering(struct e1000_hw *hw)
{
	return e1000_enable_tx_pkt_filtering_generic(hw);
}

/**
 *  e1000_mng_host_if_write - Writes to the manageability host interface
 *  @hw: pointer to the HW structure
 *  @buffer: pointer to the host interface buffer
 *  @length: size of the buffer
 *  @offset: location in the buffer to write to
 *  @sum: sum of the data (not checksum)
 *
 *  This function writes the buffer content at the offset given on the host if.
 *  It also does alignment considerations to do the writes in most efficient
 *  way.  Also fills up the sum of the buffer in *buffer parameter.
 **/
s32 e1000_mng_host_if_write(struct e1000_hw * hw, u8 *buffer, u16 length,
                            u16 offset, u8 *sum)
{
	if (hw->func.mng_host_if_write)
		return hw->func.mng_host_if_write(hw, buffer, length, offset,
		                                  sum);

	return E1000_NOT_IMPLEMENTED;
}

/**
 *  e1000_mng_write_cmd_header - Writes manageability command header
 *  @hw: pointer to the HW structure
 *  @hdr: pointer to the host interface command header
 *
 *  Writes the command header after does the checksum calculation.
 **/
s32 e1000_mng_write_cmd_header(struct e1000_hw *hw,
                               struct e1000_host_mng_command_header *hdr)
{
	if (hw->func.mng_write_cmd_header)
		return hw->func.mng_write_cmd_header(hw, hdr);

	return E1000_NOT_IMPLEMENTED;
}

/**
 *  e1000_mng_enable_host_if - Checks host interface is enabled
 *  @hw: pointer to the HW structure
 *
 *  Returns E1000_success upon success, else E1000_ERR_HOST_INTERFACE_COMMAND
 *
 *  This function checks whether the HOST IF is enabled for command operaton
 *  and also checks whether the previous command is completed.  It busy waits
 *  in case of previous command is not completed.
 **/
s32 e1000_mng_enable_host_if(struct e1000_hw * hw)
{
	if (hw->func.mng_enable_host_if)
		return hw->func.mng_enable_host_if(hw);

	return E1000_NOT_IMPLEMENTED;
}

/**
 *  e1000_wait_autoneg - Waits for autonegotiation completion
 *  @hw: pointer to the HW structure
 *
 *  Waits for autoneg to complete. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
s32 e1000_wait_autoneg(struct e1000_hw *hw)
{
	if (hw->func.wait_autoneg)
		return hw->func.wait_autoneg(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_check_reset_block - Verifies PHY can be reset
 *  @hw: pointer to the HW structure
 *
 *  Checks if the PHY is in a state that can be reset or if manageability
 *  has it tied up. This is a function pointer entry point called by drivers.
 **/
s32 e1000_check_reset_block(struct e1000_hw *hw)
{
	if (hw->func.check_reset_block)
		return hw->func.check_reset_block(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_read_phy_reg - Reads PHY register
 *  @hw: pointer to the HW structure
 *  @offset: the register to read
 *  @data: the buffer to store the 16-bit read.
 *
 *  Reads the PHY register and returns the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 e1000_read_phy_reg(struct e1000_hw *hw, u32 offset, u16 *data)
{
	if (hw->func.read_phy_reg)
		return hw->func.read_phy_reg(hw, offset, data);

	return E1000_SUCCESS;
}

/**
 *  e1000_write_phy_reg - Writes PHY register
 *  @hw: pointer to the HW structure
 *  @offset: the register to write
 *  @data: the value to write.
 *
 *  Writes the PHY register at offset with the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 e1000_write_phy_reg(struct e1000_hw *hw, u32 offset, u16 data)
{
	if (hw->func.write_phy_reg)
		return hw->func.write_phy_reg(hw, offset, data);

	return E1000_SUCCESS;
}

/**
 *  e1000_read_kmrn_reg - Reads register using Kumeran interface
 *  @hw: pointer to the HW structure
 *  @offset: the register to read
 *  @data: the location to store the 16-bit value read.
 *
 *  Reads a register out of the Kumeran interface. Currently no func pointer
 *  exists and all implementations are handled in the generic version of
 *  this function.
 **/
s32 e1000_read_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return e1000_read_kmrn_reg_generic(hw, offset, data);
}

/**
 *  e1000_write_kmrn_reg - Writes register using Kumeran interface
 *  @hw: pointer to the HW structure
 *  @offset: the register to write
 *  @data: the value to write.
 *
 *  Writes a register to the Kumeran interface. Currently no func pointer
 *  exists and all implementations are handled in the generic version of
 *  this function.
 **/
s32 e1000_write_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 data)
{
	return e1000_write_kmrn_reg_generic(hw, offset, data);
}

/**
 *  e1000_get_cable_length - Retrieves cable length estimation
 *  @hw: pointer to the HW structure
 *
 *  This function estimates the cable length and stores them in
 *  hw->phy.min_length and hw->phy.max_length. This is a function pointer
 *  entry point called by drivers.
 **/
s32 e1000_get_cable_length(struct e1000_hw *hw)
{
	if (hw->func.get_cable_length)
		return hw->func.get_cable_length(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_get_phy_info - Retrieves PHY information from registers
 *  @hw: pointer to the HW structure
 *
 *  This function gets some information from various PHY registers and
 *  populates hw->phy values with it. This is a function pointer entry
 *  point called by drivers.
 **/
s32 e1000_get_phy_info(struct e1000_hw *hw)
{
	if (hw->func.get_phy_info)
		return hw->func.get_phy_info(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_phy_hw_reset - Hard PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Performs a hard PHY reset. This is a function pointer entry point called
 *  by drivers.
 **/
s32 e1000_phy_hw_reset(struct e1000_hw *hw)
{
	if (hw->func.reset_phy)
		return hw->func.reset_phy(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_phy_commit - Soft PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Performs a soft PHY reset on those that apply. This is a function pointer
 *  entry point called by drivers.
 **/
s32 e1000_phy_commit(struct e1000_hw *hw)
{
	if (hw->func.commit_phy)
		return hw->func.commit_phy(hw);

	return E1000_SUCCESS;
}

/**
 *  e1000_set_d3_lplu_state - Sets low power link up state for D0
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
s32 e1000_set_d0_lplu_state(struct e1000_hw *hw, bool active)
{
	if (hw->func.set_d0_lplu_state)
		return hw->func.set_d0_lplu_state(hw, active);

	return E1000_SUCCESS;
}

/**
 *  e1000_set_d3_lplu_state - Sets low power link up state for D3
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
s32 e1000_set_d3_lplu_state(struct e1000_hw *hw, bool active)
{
	if (hw->func.set_d3_lplu_state)
		return hw->func.set_d3_lplu_state(hw, active);

	return E1000_SUCCESS;
}

/**
 *  e1000_read_mac_addr - Reads MAC address
 *  @hw: pointer to the HW structure
 *
 *  Reads the MAC address out of the adapter and stores it in the HW structure.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 e1000_read_mac_addr(struct e1000_hw *hw)
{
	if (hw->func.read_mac_addr)
		return hw->func.read_mac_addr(hw);

	return e1000_read_mac_addr_generic(hw);
}

/**
 *  e1000_read_part_num - Read device part number
 *  @hw: pointer to the HW structure
 *  @part_num: pointer to device part number
 *
 *  Reads the product board assembly (PBA) number from the EEPROM and stores
 *  the value in part_num.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 e1000_read_part_num(struct e1000_hw *hw, u32 *part_num)
{
	return e1000_read_part_num_generic(hw, part_num);
}

/**
 *  e1000_validate_nvm_checksum - Verifies NVM (EEPROM) checksum
 *  @hw: pointer to the HW structure
 *
 *  Validates the NVM checksum is correct. This is a function pointer entry
 *  point called by drivers.
 **/
s32 e1000_validate_nvm_checksum(struct e1000_hw *hw)
{
	if (hw->func.validate_nvm)
		return hw->func.validate_nvm(hw);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_update_nvm_checksum - Updates NVM (EEPROM) checksum
 *  @hw: pointer to the HW structure
 *
 *  Updates the NVM checksum. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
s32 e1000_update_nvm_checksum(struct e1000_hw *hw)
{
	if (hw->func.update_nvm)
		return hw->func.update_nvm(hw);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_reload_nvm - Reloads EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Reloads the EEPROM by setting the "Reinitialize from EEPROM" bit in the
 *  extended control register.
 **/
void e1000_reload_nvm(struct e1000_hw *hw)
{
	if (hw->func.reload_nvm)
		hw->func.reload_nvm(hw);
}

/**
 *  e1000_read_nvm - Reads NVM (EEPROM)
 *  @hw: pointer to the HW structure
 *  @offset: the word offset to read
 *  @words: number of 16-bit words to read
 *  @data: pointer to the properly sized buffer for the data.
 *
 *  Reads 16-bit chunks of data from the NVM (EEPROM). This is a function
 *  pointer entry point called by drivers.
 **/
s32 e1000_read_nvm(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	if (hw->func.read_nvm)
		return hw->func.read_nvm(hw, offset, words, data);

	return -E1000_ERR_CONFIG;
}

/**
 *  e1000_write_nvm - Writes to NVM (EEPROM)
 *  @hw: pointer to the HW structure
 *  @offset: the word offset to read
 *  @words: number of 16-bit words to write
 *  @data: pointer to the properly sized buffer for the data.
 *
 *  Writes 16-bit chunks of data to the NVM (EEPROM). This is a function
 *  pointer entry point called by drivers.
 **/
s32 e1000_write_nvm(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	if (hw->func.write_nvm)
		return hw->func.write_nvm(hw, offset, words, data);

	return E1000_SUCCESS;
}

/**
 *  e1000_write_8bit_ctrl_reg - Writes 8bit Control register
 *  @hw: pointer to the HW structure
 *  @reg: 32bit register offset
 *  @offset: the register to write
 *  @data: the value to write.
 *
 *  Writes the PHY register at offset with the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 e1000_write_8bit_ctrl_reg(struct e1000_hw *hw, u32 reg, u32 offset, u8 data)
{
	return e1000_write_8bit_ctrl_reg_generic(hw, reg, offset, data);
}
