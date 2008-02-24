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
/* $FreeBSD: src/sys/dev/em/e1000_82542.c,v 1.3.4.1 2007/11/28 23:24:37 jfv Exp $ */


/* e1000_82542 (rev 1 & 2)
 */

#include "e1000_api.h"

void e1000_init_function_pointers_82542(struct e1000_hw *hw);

STATIC s32  e1000_init_phy_params_82542(struct e1000_hw *hw);
STATIC s32  e1000_init_nvm_params_82542(struct e1000_hw *hw);
STATIC s32  e1000_init_mac_params_82542(struct e1000_hw *hw);
STATIC s32  e1000_get_bus_info_82542(struct e1000_hw *hw);
STATIC s32  e1000_reset_hw_82542(struct e1000_hw *hw);
STATIC s32  e1000_init_hw_82542(struct e1000_hw *hw);
STATIC s32  e1000_setup_link_82542(struct e1000_hw *hw);
STATIC s32  e1000_led_on_82542(struct e1000_hw *hw);
STATIC s32  e1000_led_off_82542(struct e1000_hw *hw);
STATIC void e1000_clear_hw_cntrs_82542(struct e1000_hw *hw);

struct e1000_dev_spec_82542 {
	bool dma_fairness;
};

/**
 *  e1000_init_phy_params_82542 - Init PHY func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_phy_params_82542(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_init_phy_params_82542");

	phy->type               = e1000_phy_none;

	return ret_val;
}

/**
 *  e1000_init_nvm_params_82542 - Init NVM func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_nvm_params_82542(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_functions *func = &hw->func;

	DEBUGFUNC("e1000_init_nvm_params_82542");

	nvm->address_bits       =  6;
	nvm->delay_usec         = 50;
	nvm->opcode_bits        =  3;
	nvm->type               = e1000_nvm_eeprom_microwire;
	nvm->word_size          = 64;

	/* Function Pointers */
	func->read_nvm          = e1000_read_nvm_microwire;
	func->release_nvm       = e1000_stop_nvm;
	func->write_nvm         = e1000_write_nvm_microwire;
	func->update_nvm        = e1000_update_nvm_checksum_generic;
	func->validate_nvm      = e1000_validate_nvm_checksum_generic;

	return E1000_SUCCESS;
}

/**
 *  e1000_init_mac_params_82542 - Init MAC func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  This is a function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_mac_params_82542(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_functions *func = &hw->func;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_init_mac_params_82542");

	/* Set media type */
	hw->phy.media_type = e1000_media_type_fiber;

	/* Set mta register count */
	mac->mta_reg_count = 128;
	/* Set rar entry count */
	mac->rar_entry_count = E1000_RAR_ENTRIES;

	/* Function pointers */

	/* bus type/speed/width */
	func->get_bus_info = e1000_get_bus_info_82542;
	/* reset */
	func->reset_hw = e1000_reset_hw_82542;
	/* hw initialization */
	func->init_hw = e1000_init_hw_82542;
	/* link setup */
	func->setup_link = e1000_setup_link_82542;
	/* phy/fiber/serdes setup */
	func->setup_physical_interface = e1000_setup_fiber_serdes_link_generic;
	/* check for link */
	func->check_for_link = e1000_check_for_fiber_link_generic;
	/* multicast address update */
	func->update_mc_addr_list = e1000_update_mc_addr_list_generic;
	/* writing VFTA */
	func->write_vfta = e1000_write_vfta_generic;
	/* clearing VFTA */
	func->clear_vfta = e1000_clear_vfta_generic;
	/* setting MTA */
	func->mta_set = e1000_mta_set_generic;
	/* turn on/off LED */
	func->led_on = e1000_led_on_82542;
	func->led_off = e1000_led_off_82542;
	/* remove device */
	func->remove_device = e1000_remove_device_generic;
	/* clear hardware counters */
	func->clear_hw_cntrs = e1000_clear_hw_cntrs_82542;
	/* link info */
	func->get_link_up_info = e1000_get_speed_and_duplex_fiber_serdes_generic;

	hw->dev_spec_size = sizeof(struct e1000_dev_spec_82542);

	/* Device-specific structure allocation */
	ret_val = e1000_alloc_zeroed_dev_spec_struct(hw, hw->dev_spec_size);

	return ret_val;
}

/**
 *  e1000_init_function_pointers_82542 - Init func ptrs.
 *  @hw: pointer to the HW structure
 *
 *  The only function explicitly called by the api module to initialize
 *  all function pointers and parameters.
 **/
void e1000_init_function_pointers_82542(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_init_function_pointers_82542");

	hw->func.init_mac_params = e1000_init_mac_params_82542;
	hw->func.init_nvm_params = e1000_init_nvm_params_82542;
	hw->func.init_phy_params = e1000_init_phy_params_82542;
}

/**
 *  e1000_get_bus_info_82542 - Obtain bus information for adapter
 *  @hw: pointer to the HW structure
 *
 *  This will obtain information about the HW bus for which the
 *  adaper is attached and stores it in the hw structure.  This is a function
 *  pointer entry point called by the api module.
 **/
STATIC s32 e1000_get_bus_info_82542(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_get_bus_info_82542");

	hw->bus.type = e1000_bus_type_pci;
	hw->bus.speed = e1000_bus_speed_unknown;
	hw->bus.width = e1000_bus_width_unknown;

	return E1000_SUCCESS;
}

/**
 *  e1000_reset_hw_82542 - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets the hardware into a known state.  This is a
 *  function pointer entry point called by the api module.
 **/
STATIC s32 e1000_reset_hw_82542(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;
	s32 ret_val = E1000_SUCCESS;
	u32 ctrl, icr;

	DEBUGFUNC("e1000_reset_hw_82542");

	if (hw->revision_id == E1000_REVISION_2) {
		DEBUGOUT("Disabling MWI on 82542 rev 2\n");
		e1000_pci_clear_mwi(hw);
	}

	DEBUGOUT("Masking off all interrupts\n");
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);

	E1000_WRITE_REG(hw, E1000_RCTL, 0);
	E1000_WRITE_REG(hw, E1000_TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH(hw);

	/*
	 * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
	msec_delay(10);

	ctrl = E1000_READ_REG(hw, E1000_CTRL);

	DEBUGOUT("Issuing a global reset to 82542/82543 MAC\n");
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl | E1000_CTRL_RST);

	e1000_reload_nvm(hw);
	msec_delay(2);

	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	icr = E1000_READ_REG(hw, E1000_ICR);

	if (hw->revision_id == E1000_REVISION_2) {
		if (bus->pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			e1000_pci_set_mwi(hw);
	}

	return ret_val;
}

/**
 *  e1000_init_hw_82542 - Initialize hardware
 *  @hw: pointer to the HW structure
 *
 *  This inits the hardware readying it for operation.  This is a
 *  function pointer entry point called by the api module.
 **/
STATIC s32 e1000_init_hw_82542(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_dev_spec_82542 *dev_spec;
	s32 ret_val = E1000_SUCCESS;
	u32 ctrl;
	u16 i;

	DEBUGFUNC("e1000_init_hw_82542");

	dev_spec = (struct e1000_dev_spec_82542 *)hw->dev_spec;

	/* Disabling VLAN filtering */
	E1000_WRITE_REG(hw, E1000_VET, 0);
	e1000_clear_vfta(hw);

	/* For 82542 (rev 2.0), disable MWI and put the receiver into reset */
	if (hw->revision_id == E1000_REVISION_2) {
		DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
		e1000_pci_clear_mwi(hw);
		E1000_WRITE_REG(hw, E1000_RCTL, E1000_RCTL_RST);
		E1000_WRITE_FLUSH(hw);
		msec_delay(5);
	}

	/* Setup the receive address. */
	e1000_init_rx_addrs_generic(hw, mac->rar_entry_count);

	/* For 82542 (rev 2.0), take the receiver out of reset and enable MWI */
	if (hw->revision_id == E1000_REVISION_2) {
		E1000_WRITE_REG(hw, E1000_RCTL, 0);
		E1000_WRITE_FLUSH(hw);
		msec_delay(1);
		if (hw->bus.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			e1000_pci_set_mwi(hw);
	}

	/* Zero out the Multicast HASH table */
	DEBUGOUT("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);

	/*
	 * Set the PCI priority bit correctly in the CTRL register.  This
	 * determines if the adapter gives priority to receives, or if it
	 * gives equal priority to transmits and receives.
	 */
	if (dev_spec->dma_fairness) {
		ctrl = E1000_READ_REG(hw, E1000_CTRL);
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	/* Setup link and flow control */
	ret_val = e1000_setup_link_82542(hw);

	/*
	 * Clear all of the statistics registers (clear on read).  It is
	 * important that we do this after we have tried to establish link
	 * because the symbol error count will increment wildly if there
	 * is no link.
	 */
	e1000_clear_hw_cntrs_82542(hw);

	return ret_val;
}

/**
 *  e1000_setup_link_82542 - Setup flow control and link settings
 *  @hw: pointer to the HW structure
 *
 *  Determines which flow control settings to use, then configures flow
 *  control.  Calls the appropriate media-specific link configuration
 *  function.  Assuming the adapter has a valid link partner, a valid link
 *  should be established.  Assumes the hardware has previously been reset
 *  and the transmitter and receiver are not enabled.  This is a function
 *  pointer entry point called by the api module.
 **/
STATIC s32 e1000_setup_link_82542(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_functions *func = &hw->func;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_setup_link_82542");

	ret_val = e1000_set_default_fc_generic(hw);
	if (ret_val)
		goto out;

	hw->fc.type &= ~e1000_fc_tx_pause;

	if (mac->report_tx_early == 1)
		hw->fc.type &= ~e1000_fc_rx_pause;

	/*
	 * We want to save off the original Flow Control configuration just in
	 * case we get disconnected and then reconnected into a different hub
	 * or switch with different Flow Control capabilities.
	 */
	hw->fc.original_type = hw->fc.type;

	DEBUGOUT1("After fix-ups FlowControl is now = %x\n", hw->fc.type);

	/* Call the necessary subroutine to configure the link. */
	ret_val = func->setup_physical_interface(hw);
	if (ret_val)
		goto out;

	/*
	 * Initialize the flow control address, type, and PAUSE timer
	 * registers to their default values.  This is done even if flow
	 * control is disabled, because it does not hurt anything to
	 * initialize these registers.
	 */
	DEBUGOUT("Initializing Flow Control address, type and timer regs\n");

	E1000_WRITE_REG(hw, E1000_FCAL, FLOW_CONTROL_ADDRESS_LOW);
	E1000_WRITE_REG(hw, E1000_FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	E1000_WRITE_REG(hw, E1000_FCT, FLOW_CONTROL_TYPE);

	E1000_WRITE_REG(hw, E1000_FCTTV, hw->fc.pause_time);

	ret_val = e1000_set_fc_watermarks_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_led_on_82542 - Turn on SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  Turns the SW defined LED on.  This is a function pointer entry point
 *  called by the api module.
 **/
STATIC s32 e1000_led_on_82542(struct e1000_hw *hw)
{
	u32 ctrl = E1000_READ_REG(hw, E1000_CTRL);

	DEBUGFUNC("e1000_led_on_82542");

	ctrl |= E1000_CTRL_SWDPIN0;
	ctrl |= E1000_CTRL_SWDPIO0;
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

	return E1000_SUCCESS;
}

/**
 *  e1000_led_off_82542 - Turn off SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  Turns the SW defined LED off.  This is a function pointer entry point
 *  called by the api module.
 **/
STATIC s32 e1000_led_off_82542(struct e1000_hw *hw)
{
	u32 ctrl = E1000_READ_REG(hw, E1000_CTRL);

	DEBUGFUNC("e1000_led_off_82542");

	ctrl &= ~E1000_CTRL_SWDPIN0;
	ctrl |= E1000_CTRL_SWDPIO0;
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

	return E1000_SUCCESS;
}

/**
 *  e1000_translate_register_82542 - Translate the proper regiser offset
 *  @reg: e1000 register to be read
 *
 *  Registers in 82542 are located in different offsets than other adapters
 *  even though they function in the same manner.  This function takes in
 *  the name of the register to read and returns the correct offset for
 *  82542 silicon.
 **/
u32 e1000_translate_register_82542(u32 reg)
{
	/*
	 * Some of the 82542 registers are located at different
	 * offsets than they are in newer adapters.
	 * Despite the difference in location, the registers
	 * function in the same manner.
	 */
	switch (reg) {
	case E1000_RA:
		reg = 0x00040;
		break;
	case E1000_RDTR:
		reg = 0x00108;
		break;
	case E1000_RDBAL(0):
		reg = 0x00110;
		break;
	case E1000_RDBAH(0):
		reg = 0x00114;
		break;
	case E1000_RDLEN(0):
		reg = 0x00118;
		break;
	case E1000_RDH(0):
		reg = 0x00120;
		break;
	case E1000_RDT(0):
		reg = 0x00128;
		break;
	case E1000_RDBAL(1):
		reg = 0x00138;
		break;
	case E1000_RDBAH(1):
		reg = 0x0013C;
		break;
	case E1000_RDLEN(1):
		reg = 0x00140;
		break;
	case E1000_RDH(1):
		reg = 0x00148;
		break;
	case E1000_RDT(1):
		reg = 0x00150;
		break;
	case E1000_FCRTH:
		reg = 0x00160;
		break;
	case E1000_FCRTL:
		reg = 0x00168;
		break;
	case E1000_MTA:
		reg = 0x00200;
		break;
	case E1000_TDBAL(0):
		reg = 0x00420;
		break;
	case E1000_TDBAH(0):
		reg = 0x00424;
		break;
	case E1000_TDLEN(0):
		reg = 0x00428;
		break;
	case E1000_TDH(0):
		reg = 0x00430;
		break;
	case E1000_TDT(0):
		reg = 0x00438;
		break;
	case E1000_TIDV:
		reg = 0x00440;
		break;
	case E1000_VFTA:
		reg = 0x00600;
		break;
	case E1000_TDFH:
		reg = 0x08010;
		break;
	case E1000_TDFT:
		reg = 0x08018;
		break;
	default:
		break;
	}

	return reg;
}

/**
 *  e1000_clear_hw_cntrs_82542 - Clear device specific hardware counters
 *  @hw: pointer to the HW structure
 *
 *  Clears the hardware counters by reading the counter registers.
 **/
STATIC void e1000_clear_hw_cntrs_82542(struct e1000_hw *hw)
{
	volatile u32 temp;

	DEBUGFUNC("e1000_clear_hw_cntrs_82542");

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
}
