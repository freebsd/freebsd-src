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

/**
 *  e1000_remove_device_generic - Free device specific structure
 *  @hw: pointer to the HW structure
 *
 *  If a device specific structure was allocated, this function will
 *  free it.
 **/
void e1000_remove_device_generic(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_remove_device_generic");

	/* Freeing the dev_spec member of e1000_hw structure */
	e1000_free_dev_spec_struct(hw);
}

/**
 *  e1000_get_bus_info_pci_generic - Get PCI(x) bus information
 *  @hw: pointer to the HW structure
 *
 *  Determines and stores the system bus information for a particular
 *  network interface.  The following bus information is determined and stored:
 *  bus speed, bus width, type (PCI/PCIx), and PCI(-x) function.
 **/
s32 e1000_get_bus_info_pci_generic(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;
	u32 status = E1000_READ_REG(hw, E1000_STATUS);
	s32 ret_val = E1000_SUCCESS;
	u16 pci_header_type;

	DEBUGFUNC("e1000_get_bus_info_pci_generic");

	/* PCI or PCI-X? */
	bus->type = (status & E1000_STATUS_PCIX_MODE)
			? e1000_bus_type_pcix
			: e1000_bus_type_pci;

	/* Bus speed */
	if (bus->type == e1000_bus_type_pci) {
		bus->speed = (status & E1000_STATUS_PCI66)
		             ? e1000_bus_speed_66
		             : e1000_bus_speed_33;
	} else {
		switch (status & E1000_STATUS_PCIX_SPEED) {
		case E1000_STATUS_PCIX_SPEED_66:
			bus->speed = e1000_bus_speed_66;
			break;
		case E1000_STATUS_PCIX_SPEED_100:
			bus->speed = e1000_bus_speed_100;
			break;
		case E1000_STATUS_PCIX_SPEED_133:
			bus->speed = e1000_bus_speed_133;
			break;
		default:
			bus->speed = e1000_bus_speed_reserved;
			break;
		}
	}

	/* Bus width */
	bus->width = (status & E1000_STATUS_BUS64)
	             ? e1000_bus_width_64
	             : e1000_bus_width_32;

	/* Which PCI(-X) function? */
	e1000_read_pci_cfg(hw, PCI_HEADER_TYPE_REGISTER, &pci_header_type);
	if (pci_header_type & PCI_HEADER_TYPE_MULTIFUNC)
		bus->func = (status & E1000_STATUS_FUNC_MASK)
		            >> E1000_STATUS_FUNC_SHIFT;
	else
		bus->func = 0;

	return ret_val;
}

/**
 *  e1000_get_bus_info_pcie_generic - Get PCIe bus information
 *  @hw: pointer to the HW structure
 *
 *  Determines and stores the system bus information for a particular
 *  network interface.  The following bus information is determined and stored:
 *  bus speed, bus width, type (PCIe), and PCIe function.
 **/
s32 e1000_get_bus_info_pcie_generic(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;
	s32 ret_val;
	u32 status;
	u16 pcie_link_status, pci_header_type;

	DEBUGFUNC("e1000_get_bus_info_pcie_generic");

	bus->type = e1000_bus_type_pci_express;
	bus->speed = e1000_bus_speed_2500;

	ret_val = e1000_read_pcie_cap_reg(hw,
	                                  PCIE_LINK_STATUS,
	                                  &pcie_link_status);
	if (ret_val)
		bus->width = e1000_bus_width_unknown;
	else
		bus->width = (e1000_bus_width)((pcie_link_status &
		                                PCIE_LINK_WIDTH_MASK) >>
		                               PCIE_LINK_WIDTH_SHIFT);

	e1000_read_pci_cfg(hw, PCI_HEADER_TYPE_REGISTER, &pci_header_type);
	if (pci_header_type & PCI_HEADER_TYPE_MULTIFUNC) {
		status = E1000_READ_REG(hw, E1000_STATUS);
		bus->func = (status & E1000_STATUS_FUNC_MASK)
		            >> E1000_STATUS_FUNC_SHIFT;
	} else {
		bus->func = 0;
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_clear_vfta_generic - Clear VLAN filter table
 *  @hw: pointer to the HW structure
 *
 *  Clears the register array which contains the VLAN filter table by
 *  setting all the values to 0.
 **/
void e1000_clear_vfta_generic(struct e1000_hw *hw)
{
	u32 offset;

	DEBUGFUNC("e1000_clear_vfta_generic");

	for (offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, offset, 0);
		E1000_WRITE_FLUSH(hw);
	}
}

/**
 *  e1000_write_vfta_generic - Write value to VLAN filter table
 *  @hw: pointer to the HW structure
 *  @offset: register offset in VLAN filter table
 *  @value: register value written to VLAN filter table
 *
 *  Writes value at the given offset in the register array which stores
 *  the VLAN filter table.
 **/
void e1000_write_vfta_generic(struct e1000_hw *hw, u32 offset, u32 value)
{
	DEBUGFUNC("e1000_write_vfta_generic");

	E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, offset, value);
	E1000_WRITE_FLUSH(hw);
}

/**
 *  e1000_init_rx_addrs_generic - Initialize receive address's
 *  @hw: pointer to the HW structure
 *  @rar_count: receive address registers
 *
 *  Setups the receive address registers by setting the base receive address
 *  register to the devices MAC address and clearing all the other receive
 *  address registers to 0.
 **/
void e1000_init_rx_addrs_generic(struct e1000_hw *hw, u16 rar_count)
{
	u32 i;

	DEBUGFUNC("e1000_init_rx_addrs_generic");

	/* Setup the receive address */
	DEBUGOUT("Programming MAC Address into RAR[0]\n");

	e1000_rar_set_generic(hw, hw->mac.addr, 0);

	/* Zero out the other (rar_entry_count - 1) receive addresses */
	DEBUGOUT1("Clearing RAR[1-%u]\n", rar_count-1);
	for (i = 1; i < rar_count; i++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_RA, (i << 1), 0);
		E1000_WRITE_FLUSH(hw);
		E1000_WRITE_REG_ARRAY(hw, E1000_RA, ((i << 1) + 1), 0);
		E1000_WRITE_FLUSH(hw);
	}
}

/**
 *  e1000_check_alt_mac_addr_generic - Check for alternate MAC addr
 *  @hw: pointer to the HW structure
 *
 *  Checks the nvm for an alternate MAC address.  An alternate MAC address
 *  can be setup by pre-boot software and must be treated like a permanent
 *  address and must override the actual permanent MAC address.  If an
 *  alternate MAC address is fopund it is saved in the hw struct and
 *  prgrammed into RAR0 and the cuntion returns success, otherwise the
 *  fucntion returns an error.
 **/
s32 e1000_check_alt_mac_addr_generic(struct e1000_hw *hw)
{
	u32 i;
	s32 ret_val = E1000_SUCCESS;
	u16 offset, nvm_alt_mac_addr_offset, nvm_data;
	u8 alt_mac_addr[ETH_ADDR_LEN];

	DEBUGFUNC("e1000_check_alt_mac_addr_generic");

	ret_val = e1000_read_nvm(hw, NVM_ALT_MAC_ADDR_PTR, 1, &nvm_alt_mac_addr_offset);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		goto out;
	}

	if (nvm_alt_mac_addr_offset == 0xFFFF) {
		ret_val = -(E1000_NOT_IMPLEMENTED);
		goto out;
	}

	if (hw->bus.func == E1000_FUNC_1)
		nvm_alt_mac_addr_offset += ETH_ADDR_LEN/sizeof(u16);

	for (i = 0; i < ETH_ADDR_LEN; i += 2) {
		offset = nvm_alt_mac_addr_offset + (i >> 1);
		ret_val = e1000_read_nvm(hw, offset, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error\n");
			goto out;
		}

		alt_mac_addr[i] = (u8)(nvm_data & 0xFF);
		alt_mac_addr[i + 1] = (u8)(nvm_data >> 8);
	}

	/* if multicast bit is set, the alternate address will not be used */
	if (alt_mac_addr[0] & 0x01) {
		ret_val = -(E1000_NOT_IMPLEMENTED);
		goto out;
	}

	for (i = 0; i < ETH_ADDR_LEN; i++)
		hw->mac.addr[i] = hw->mac.perm_addr[i] = alt_mac_addr[i];

	e1000_rar_set(hw, hw->mac.perm_addr, 0);

out:
	return ret_val;
}

/**
 *  e1000_rar_set_generic - Set receive address register
 *  @hw: pointer to the HW structure
 *  @addr: pointer to the receive address
 *  @index: receive address array register
 *
 *  Sets the receive address array register at index to the address passed
 *  in by addr.
 **/
void e1000_rar_set_generic(struct e1000_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	DEBUGFUNC("e1000_rar_set_generic");

	/*
	 * HW expects these in little endian so we reverse the byte order
	 * from network order (big endian) to little endian
	 */
	rar_low = ((u32) addr[0] |
		   ((u32) addr[1] << 8) |
		    ((u32) addr[2] << 16) | ((u32) addr[3] << 24));

	rar_high = ((u32) addr[4] | ((u32) addr[5] << 8));

	if (!hw->mac.disable_av)
		rar_high |= E1000_RAH_AV;

	E1000_WRITE_REG_ARRAY(hw, E1000_RA, (index << 1), rar_low);
	E1000_WRITE_REG_ARRAY(hw, E1000_RA, ((index << 1) + 1), rar_high);
}

/**
 *  e1000_mta_set_generic - Set multicast filter table address
 *  @hw: pointer to the HW structure
 *  @hash_value: determines the MTA register and bit to set
 *
 *  The multicast table address is a register array of 32-bit registers.
 *  The hash_value is used to determine what register the bit is in, the
 *  current value is read, the new bit is OR'd in and the new value is
 *  written back into the register.
 **/
void e1000_mta_set_generic(struct e1000_hw *hw, u32 hash_value)
{
	u32 hash_bit, hash_reg, mta;

	DEBUGFUNC("e1000_mta_set_generic");
	/*
	 * The MTA is a register array of 32-bit registers. It is
	 * treated like an array of (32*mta_reg_count) bits.  We want to
	 * set bit BitArray[hash_value]. So we figure out what register
	 * the bit is in, read it, OR in the new bit, then write
	 * back the new value.  The (hw->mac.mta_reg_count - 1) serves as a
	 * mask to bits 31:5 of the hash value which gives us the
	 * register we're modifying.  The hash bit within that register
	 * is determined by the lower 5 bits of the hash value.
	 */
	hash_reg = (hash_value >> 5) & (hw->mac.mta_reg_count - 1);
	hash_bit = hash_value & 0x1F;

	mta = E1000_READ_REG_ARRAY(hw, E1000_MTA, hash_reg);

	mta |= (1 << hash_bit);

	E1000_WRITE_REG_ARRAY(hw, E1000_MTA, hash_reg, mta);
	E1000_WRITE_FLUSH(hw);
}

/**
 *  e1000_update_mc_addr_list_generic - Update Multicast addresses
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
void e1000_update_mc_addr_list_generic(struct e1000_hw *hw,
                                       u8 *mc_addr_list, u32 mc_addr_count,
                                       u32 rar_used_count, u32 rar_count)
{
	u32 hash_value;
	u32 i;

	DEBUGFUNC("e1000_update_mc_addr_list_generic");

	/*
	 * Load the first set of multicast addresses into the exact
	 * filters (RAR).  If there are not enough to fill the RAR
	 * array, clear the filters.
	 */
	for (i = rar_used_count; i < rar_count; i++) {
		if (mc_addr_count) {
			e1000_rar_set(hw, mc_addr_list, i);
			mc_addr_count--;
			mc_addr_list += ETH_ADDR_LEN;
		} else {
			E1000_WRITE_REG_ARRAY(hw, E1000_RA, i << 1, 0);
			E1000_WRITE_FLUSH(hw);
			E1000_WRITE_REG_ARRAY(hw, E1000_RA, (i << 1) + 1, 0);
			E1000_WRITE_FLUSH(hw);
		}
	}

	/* Clear the old settings from the MTA */
	DEBUGOUT("Clearing MTA\n");
	for (i = 0; i < hw->mac.mta_reg_count; i++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);
		E1000_WRITE_FLUSH(hw);
	}

	/* Load any remaining multicast addresses into the hash table. */
	for (; mc_addr_count > 0; mc_addr_count--) {
		hash_value = e1000_hash_mc_addr(hw, mc_addr_list);
		DEBUGOUT1("Hash value = 0x%03X\n", hash_value);
		e1000_mta_set(hw, hash_value);
		mc_addr_list += ETH_ADDR_LEN;
	}
}

/**
 *  e1000_hash_mc_addr_generic - Generate a multicast hash value
 *  @hw: pointer to the HW structure
 *  @mc_addr: pointer to a multicast address
 *
 *  Generates a multicast address hash value which is used to determine
 *  the multicast filter table array address and new table value.  See
 *  e1000_mta_set_generic()
 **/
u32 e1000_hash_mc_addr_generic(struct e1000_hw *hw, u8 *mc_addr)
{
	u32 hash_value, hash_mask;
	u8 bit_shift = 0;

	DEBUGFUNC("e1000_hash_mc_addr_generic");

	/* Register count multiplied by bits per register */
	hash_mask = (hw->mac.mta_reg_count * 32) - 1;

	/*
	 * For a mc_filter_type of 0, bit_shift is the number of left-shifts
	 * where 0xFF would still fall within the hash mask.
	 */
	while (hash_mask >> bit_shift != 0xFF)
		bit_shift++;

	/*
	 * The portion of the address that is used for the hash table
	 * is determined by the mc_filter_type setting.
	 * The algorithm is such that there is a total of 8 bits of shifting.
	 * The bit_shift for a mc_filter_type of 0 represents the number of
	 * left-shifts where the MSB of mc_addr[5] would still fall within
	 * the hash_mask.  Case 0 does this exactly.  Since there are a total
	 * of 8 bits of shifting, then mc_addr[4] will shift right the
	 * remaining number of bits. Thus 8 - bit_shift.  The rest of the
	 * cases are a variation of this algorithm...essentially raising the
	 * number of bits to shift mc_addr[5] left, while still keeping the
	 * 8-bit shifting total.
	 *
	 * For example, given the following Destination MAC Address and an
	 * mta register count of 128 (thus a 4096-bit vector and 0xFFF mask),
	 * we can see that the bit_shift for case 0 is 4.  These are the hash
	 * values resulting from each mc_filter_type...
	 * [0] [1] [2] [3] [4] [5]
	 * 01  AA  00  12  34  56
	 * LSB                 MSB
	 *
	 * case 0: hash_value = ((0x34 >> 4) | (0x56 << 4)) & 0xFFF = 0x563
	 * case 1: hash_value = ((0x34 >> 3) | (0x56 << 5)) & 0xFFF = 0xAC6
	 * case 2: hash_value = ((0x34 >> 2) | (0x56 << 6)) & 0xFFF = 0x163
	 * case 3: hash_value = ((0x34 >> 0) | (0x56 << 8)) & 0xFFF = 0x634
	 */
	switch (hw->mac.mc_filter_type) {
		default:
		case 0:
			break;
		case 1:
			bit_shift += 1;
			break;
		case 2:
			bit_shift += 2;
			break;
		case 3:
			bit_shift += 4;
			break;
	}

	hash_value = hash_mask & (((mc_addr[4] >> (8 - bit_shift)) |
	                          (((u16) mc_addr[5]) << bit_shift)));

	return hash_value;
}

/**
 *  e1000_pcix_mmrbc_workaround_generic - Fix incorrect MMRBC value
 *  @hw: pointer to the HW structure
 *
 *  In certain situations, a system BIOS may report that the PCIx maximum
 *  memory read byte count (MMRBC) value is higher than than the actual
 *  value. We check the PCIx command regsiter with the current PCIx status
 *  regsiter.
 **/
void e1000_pcix_mmrbc_workaround_generic(struct e1000_hw *hw)
{
	u16 cmd_mmrbc;
	u16 pcix_cmd;
	u16 pcix_stat_hi_word;
	u16 stat_mmrbc;

	DEBUGFUNC("e1000_pcix_mmrbc_workaround_generic");

	/* Workaround for PCI-X issue when BIOS sets MMRBC incorrectly */
	if (hw->bus.type != e1000_bus_type_pcix)
		return;

	e1000_read_pci_cfg(hw, PCIX_COMMAND_REGISTER, &pcix_cmd);
	e1000_read_pci_cfg(hw, PCIX_STATUS_REGISTER_HI, &pcix_stat_hi_word);
	cmd_mmrbc = (pcix_cmd & PCIX_COMMAND_MMRBC_MASK) >>
		    PCIX_COMMAND_MMRBC_SHIFT;
	stat_mmrbc = (pcix_stat_hi_word & PCIX_STATUS_HI_MMRBC_MASK) >>
		     PCIX_STATUS_HI_MMRBC_SHIFT;
	if (stat_mmrbc == PCIX_STATUS_HI_MMRBC_4K)
		stat_mmrbc = PCIX_STATUS_HI_MMRBC_2K;
	if (cmd_mmrbc > stat_mmrbc) {
		pcix_cmd &= ~PCIX_COMMAND_MMRBC_MASK;
		pcix_cmd |= stat_mmrbc << PCIX_COMMAND_MMRBC_SHIFT;
		e1000_write_pci_cfg(hw, PCIX_COMMAND_REGISTER, &pcix_cmd);
	}
}

/**
 *  e1000_clear_hw_cntrs_base_generic - Clear base hardware counters
 *  @hw: pointer to the HW structure
 *
 *  Clears the base hardware counters by reading the counter registers.
 **/
void e1000_clear_hw_cntrs_base_generic(struct e1000_hw *hw)
{
	volatile u32 temp;

	DEBUGFUNC("e1000_clear_hw_cntrs_base_generic");

	temp = E1000_READ_REG(hw, E1000_CRCERRS);
	temp = E1000_READ_REG(hw, E1000_SYMERRS);
	temp = E1000_READ_REG(hw, E1000_MPC);
	temp = E1000_READ_REG(hw, E1000_SCC);
	temp = E1000_READ_REG(hw, E1000_ECOL);
	temp = E1000_READ_REG(hw, E1000_MCC);
	temp = E1000_READ_REG(hw, E1000_LATECOL);
	temp = E1000_READ_REG(hw, E1000_COLC);
	temp = E1000_READ_REG(hw, E1000_DC);
	temp = E1000_READ_REG(hw, E1000_SEC);
	temp = E1000_READ_REG(hw, E1000_RLEC);
	temp = E1000_READ_REG(hw, E1000_XONRXC);
	temp = E1000_READ_REG(hw, E1000_XONTXC);
	temp = E1000_READ_REG(hw, E1000_XOFFRXC);
	temp = E1000_READ_REG(hw, E1000_XOFFTXC);
	temp = E1000_READ_REG(hw, E1000_FCRUC);
	temp = E1000_READ_REG(hw, E1000_GPRC);
	temp = E1000_READ_REG(hw, E1000_BPRC);
	temp = E1000_READ_REG(hw, E1000_MPRC);
	temp = E1000_READ_REG(hw, E1000_GPTC);
	temp = E1000_READ_REG(hw, E1000_GORCL);
	temp = E1000_READ_REG(hw, E1000_GORCH);
	temp = E1000_READ_REG(hw, E1000_GOTCL);
	temp = E1000_READ_REG(hw, E1000_GOTCH);
	temp = E1000_READ_REG(hw, E1000_RNBC);
	temp = E1000_READ_REG(hw, E1000_RUC);
	temp = E1000_READ_REG(hw, E1000_RFC);
	temp = E1000_READ_REG(hw, E1000_ROC);
	temp = E1000_READ_REG(hw, E1000_RJC);
	temp = E1000_READ_REG(hw, E1000_TORL);
	temp = E1000_READ_REG(hw, E1000_TORH);
	temp = E1000_READ_REG(hw, E1000_TOTL);
	temp = E1000_READ_REG(hw, E1000_TOTH);
	temp = E1000_READ_REG(hw, E1000_TPR);
	temp = E1000_READ_REG(hw, E1000_TPT);
	temp = E1000_READ_REG(hw, E1000_MPTC);
	temp = E1000_READ_REG(hw, E1000_BPTC);
}

/**
 *  e1000_check_for_copper_link_generic - Check for link (Copper)
 *  @hw: pointer to the HW structure
 *
 *  Checks to see of the link status of the hardware has changed.  If a
 *  change in link status has been detected, then we read the PHY registers
 *  to get the current speed/duplex if link exists.
 **/
s32 e1000_check_for_copper_link_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	bool link;

	DEBUGFUNC("e1000_check_for_copper_link");

	/*
	 * We only want to go out to the PHY registers to see if Auto-Neg
	 * has completed and/or if our link status has changed.  The
	 * get_link_status flag is set upon receiving a Link Status
	 * Change or Rx Sequence Error interrupt.
	 */
	if (!mac->get_link_status) {
		ret_val = E1000_SUCCESS;
		goto out;
	}

	/*
	 * First we want to see if the MII Status Register reports
	 * link.  If so, then we want to get the current speed/duplex
	 * of the PHY.
	 */
	ret_val = e1000_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		goto out;

	if (!link)
		goto out; /* No link detected */

	mac->get_link_status = FALSE;

	/*
	 * Check if there was DownShift, must be checked
	 * immediately after link-up
	 */
	e1000_check_downshift_generic(hw);

	/*
	 * If we are forcing speed/duplex, then we simply return since
	 * we have already determined whether we have link or not.
	 */
	if (!mac->autoneg) {
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	/*
	 * Auto-Neg is enabled.  Auto Speed Detection takes care
	 * of MAC speed/duplex configuration.  So we only need to
	 * configure Collision Distance in the MAC.
	 */
	e1000_config_collision_dist_generic(hw);

	/*
	 * Configure Flow Control now that Auto-Neg has completed.
	 * First, we need to restore the desired flow control
	 * settings because we may have had to re-autoneg with a
	 * different link partner.
	 */
	ret_val = e1000_config_fc_after_link_up_generic(hw);
	if (ret_val) {
		DEBUGOUT("Error configuring flow control\n");
	}

out:
	return ret_val;
}

/**
 *  e1000_check_for_fiber_link_generic - Check for link (Fiber)
 *  @hw: pointer to the HW structure
 *
 *  Checks for link up on the hardware.  If link is not up and we have
 *  a signal, then we need to force link up.
 **/
s32 e1000_check_for_fiber_link_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_check_for_fiber_link_generic");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	status = E1000_READ_REG(hw, E1000_STATUS);
	rxcw = E1000_READ_REG(hw, E1000_RXCW);

	/*
	 * If we don't have link (auto-negotiation failed or link partner
	 * cannot auto-negotiate), the cable is plugged in (we have signal),
	 * and our link partner is not trying to auto-negotiate with us (we
	 * are receiving idles or data), we need to force link up. We also
	 * need to give auto-negotiation time to complete, in case the cable
	 * was just plugged in. The autoneg_failed flag does this.
	 */
	/* (ctrl & E1000_CTRL_SWDPIN1) == 1 == have signal */
	if ((ctrl & E1000_CTRL_SWDPIN1) && (!(status & E1000_STATUS_LU)) &&
	    (!(rxcw & E1000_RXCW_C))) {
		if (mac->autoneg_failed == 0) {
			mac->autoneg_failed = 1;
			goto out;
		}
		DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\n");

		/* Disable auto-negotiation in the TXCW register */
		E1000_WRITE_REG(hw, E1000_TXCW, (mac->txcw & ~E1000_TXCW_ANE));

		/* Force link-up and also force full-duplex. */
		ctrl = E1000_READ_REG(hw, E1000_CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

		/* Configure Flow Control after forcing link up. */
		ret_val = e1000_config_fc_after_link_up_generic(hw);
		if (ret_val) {
			DEBUGOUT("Error configuring flow control\n");
			goto out;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		/*
		 * If we are forcing link and we are receiving /C/ ordered
		 * sets, re-enable auto-negotiation in the TXCW register
		 * and disable forced link in the Device Control register
		 * in an attempt to auto-negotiate with our link partner.
		 */
		DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\n");
		E1000_WRITE_REG(hw, E1000_TXCW, mac->txcw);
		E1000_WRITE_REG(hw, E1000_CTRL, (ctrl & ~E1000_CTRL_SLU));

		mac->serdes_has_link = TRUE;
	}

out:
	return ret_val;
}

/**
 *  e1000_check_for_serdes_link_generic - Check for link (Serdes)
 *  @hw: pointer to the HW structure
 *
 *  Checks for link up on the hardware.  If link is not up and we have
 *  a signal, then we need to force link up.
 **/
s32 e1000_check_for_serdes_link_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_check_for_serdes_link_generic");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	status = E1000_READ_REG(hw, E1000_STATUS);
	rxcw = E1000_READ_REG(hw, E1000_RXCW);

	/*
	 * If we don't have link (auto-negotiation failed or link partner
	 * cannot auto-negotiate), and our link partner is not trying to
	 * auto-negotiate with us (we are receiving idles or data),
	 * we need to force link up. We also need to give auto-negotiation
	 * time to complete.
	 */
	/* (ctrl & E1000_CTRL_SWDPIN1) == 1 == have signal */
	if ((!(status & E1000_STATUS_LU)) && (!(rxcw & E1000_RXCW_C))) {
		if (mac->autoneg_failed == 0) {
			mac->autoneg_failed = 1;
			goto out;
		}
		DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\n");

		/* Disable auto-negotiation in the TXCW register */
		E1000_WRITE_REG(hw, E1000_TXCW, (mac->txcw & ~E1000_TXCW_ANE));

		/* Force link-up and also force full-duplex. */
		ctrl = E1000_READ_REG(hw, E1000_CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

		/* Configure Flow Control after forcing link up. */
		ret_val = e1000_config_fc_after_link_up_generic(hw);
		if (ret_val) {
			DEBUGOUT("Error configuring flow control\n");
			goto out;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		/*
		 * If we are forcing link and we are receiving /C/ ordered
		 * sets, re-enable auto-negotiation in the TXCW register
		 * and disable forced link in the Device Control register
		 * in an attempt to auto-negotiate with our link partner.
		 */
		DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\n");
		E1000_WRITE_REG(hw, E1000_TXCW, mac->txcw);
		E1000_WRITE_REG(hw, E1000_CTRL, (ctrl & ~E1000_CTRL_SLU));

		mac->serdes_has_link = TRUE;
	} else if (!(E1000_TXCW_ANE & E1000_READ_REG(hw, E1000_TXCW))) {
		/*
		 * If we force link for non-auto-negotiation switch, check
		 * link status based on MAC synchronization for internal
		 * serdes media type.
		 */
		/* SYNCH bit and IV bit are sticky. */
		usec_delay(10);
		if (E1000_RXCW_SYNCH & E1000_READ_REG(hw, E1000_RXCW)) {
			if (!(rxcw & E1000_RXCW_IV)) {
				mac->serdes_has_link = TRUE;
				DEBUGOUT("SERDES: Link is up.\n");
			}
		} else {
			mac->serdes_has_link = FALSE;
			DEBUGOUT("SERDES: Link is down.\n");
		}
	}

	if (E1000_TXCW_ANE & E1000_READ_REG(hw, E1000_TXCW)) {
		status = E1000_READ_REG(hw, E1000_STATUS);
		mac->serdes_has_link = (status & E1000_STATUS_LU)
					? TRUE
					: FALSE;
	}

out:
	return ret_val;
}

/**
 *  e1000_setup_link_generic - Setup flow control and link settings
 *  @hw: pointer to the HW structure
 *
 *  Determines which flow control settings to use, then configures flow
 *  control.  Calls the appropriate media-specific link configuration
 *  function.  Assuming the adapter has a valid link partner, a valid link
 *  should be established.  Assumes the hardware has previously been reset
 *  and the transmitter and receiver are not enabled.
 **/
s32 e1000_setup_link_generic(struct e1000_hw *hw)
{
	struct e1000_functions *func = &hw->func;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_setup_link_generic");

	/*
	 * In the case of the phy reset being blocked, we already have a link.
	 * We do not need to set it up again.
	 */
	if (e1000_check_reset_block(hw))
		goto out;

	ret_val = e1000_set_default_fc_generic(hw);
	if (ret_val)
		goto out;

	/*
	 * We want to save off the original Flow Control configuration just
	 * in case we get disconnected and then reconnected into a different
	 * hub or switch with different Flow Control capabilities.
	 */
	hw->fc.original_type = hw->fc.type;

	DEBUGOUT1("After fix-ups FlowControl is now = %x\n", hw->fc.type);

	/* Call the necessary media_type subroutine to configure the link. */
	ret_val = func->setup_physical_interface(hw);
	if (ret_val)
		goto out;

	/*
	 * Initialize the flow control address, type, and PAUSE timer
	 * registers to their default values.  This is done even if flow
	 * control is disabled, because it does not hurt anything to
	 * initialize these registers.
	 */
	DEBUGOUT("Initializing the Flow Control address, type and timer regs\n");
	E1000_WRITE_REG(hw, E1000_FCT, FLOW_CONTROL_TYPE);
	E1000_WRITE_REG(hw, E1000_FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	E1000_WRITE_REG(hw, E1000_FCAL, FLOW_CONTROL_ADDRESS_LOW);

	E1000_WRITE_REG(hw, E1000_FCTTV, hw->fc.pause_time);

	ret_val = e1000_set_fc_watermarks_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_setup_fiber_serdes_link_generic - Setup link for fiber/serdes
 *  @hw: pointer to the HW structure
 *
 *  Configures collision distance and flow control for fiber and serdes
 *  links.  Upon successful setup, poll for link.
 **/
s32 e1000_setup_fiber_serdes_link_generic(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_setup_fiber_serdes_link_generic");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);

	/* Take the link out of reset */
	ctrl &= ~E1000_CTRL_LRST;

	e1000_config_collision_dist_generic(hw);

	ret_val = e1000_commit_fc_settings_generic(hw);
	if (ret_val)
		goto out;

	/*
	 * Since auto-negotiation is enabled, take the link out of reset (the
	 * link will be in reset, because we previously reset the chip). This
	 * will restart auto-negotiation.  If auto-negotiation is successful
	 * then the link-up status bit will be set and the flow control enable
	 * bits (RFCE and TFCE) will be set according to their negotiated value.
	 */
	DEBUGOUT("Auto-negotiation enabled\n");

	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);
	E1000_WRITE_FLUSH(hw);
	msec_delay(1);

	/*
	 * For these adapters, the SW defineable pin 1 is set when the optics
	 * detect a signal.  If we have a signal, then poll for a "Link-Up"
	 * indication.
	 */
	if (hw->phy.media_type == e1000_media_type_internal_serdes ||
	    (E1000_READ_REG(hw, E1000_CTRL) & E1000_CTRL_SWDPIN1)) {
		ret_val = e1000_poll_fiber_serdes_link_generic(hw);
	} else {
		DEBUGOUT("No signal detected\n");
	}

out:
	return ret_val;
}

/**
 *  e1000_config_collision_dist_generic - Configure collision distance
 *  @hw: pointer to the HW structure
 *
 *  Configures the collision distance to the default value and is used
 *  during link setup. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
void e1000_config_collision_dist_generic(struct e1000_hw *hw)
{
	u32 tctl;

	DEBUGFUNC("e1000_config_collision_dist_generic");

	tctl = E1000_READ_REG(hw, E1000_TCTL);

	tctl &= ~E1000_TCTL_COLD;
	tctl |= E1000_COLLISION_DISTANCE << E1000_COLD_SHIFT;

	E1000_WRITE_REG(hw, E1000_TCTL, tctl);
	E1000_WRITE_FLUSH(hw);
}

/**
 *  e1000_poll_fiber_serdes_link_generic - Poll for link up
 *  @hw: pointer to the HW structure
 *
 *  Polls for link up by reading the status register, if link fails to come
 *  up with auto-negotiation, then the link is forced if a signal is detected.
 **/
s32 e1000_poll_fiber_serdes_link_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 i, status;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_poll_fiber_serdes_link_generic");

	/*
	 * If we have a signal (the cable is plugged in, or assumed true for
	 * serdes media) then poll for a "Link-Up" indication in the Device
	 * Status Register.  Time-out if a link isn't seen in 500 milliseconds
	 * seconds (Auto-negotiation should complete in less than 500
	 * milliseconds even if the other end is doing it in SW).
	 */
	for (i = 0; i < FIBER_LINK_UP_LIMIT; i++) {
		msec_delay(10);
		status = E1000_READ_REG(hw, E1000_STATUS);
		if (status & E1000_STATUS_LU)
			break;
	}
	if (i == FIBER_LINK_UP_LIMIT) {
		DEBUGOUT("Never got a valid link from auto-neg!!!\n");
		mac->autoneg_failed = 1;
		/*
		 * AutoNeg failed to achieve a link, so we'll call
		 * mac->check_for_link. This routine will force the
		 * link up if we detect a signal. This will allow us to
		 * communicate with non-autonegotiating link partners.
		 */
		ret_val = e1000_check_for_link(hw);
		if (ret_val) {
			DEBUGOUT("Error while checking for link\n");
			goto out;
		}
		mac->autoneg_failed = 0;
	} else {
		mac->autoneg_failed = 0;
		DEBUGOUT("Valid Link Found\n");
	}

out:
	return ret_val;
}

/**
 *  e1000_commit_fc_settings_generic - Configure flow control
 *  @hw: pointer to the HW structure
 *
 *  Write the flow control settings to the Transmit Config Word Register (TXCW)
 *  base on the flow control settings in e1000_mac_info.
 **/
s32 e1000_commit_fc_settings_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 txcw;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_commit_fc_settings_generic");

	/*
	 * Check for a software override of the flow control settings, and
	 * setup the device accordingly.  If auto-negotiation is enabled, then
	 * software will have to set the "PAUSE" bits to the correct value in
	 * the Transmit Config Word Register (TXCW) and re-start auto-
	 * negotiation.  However, if auto-negotiation is disabled, then
	 * software will have to manually configure the two flow control enable
	 * bits in the CTRL register.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames,
	 *          but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames but we
	 *          do not support receiving pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) are enabled.
	 */
	switch (hw->fc.type) {
	case e1000_fc_none:
		/* Flow control completely disabled by a software over-ride. */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD);
		break;
	case e1000_fc_rx_pause:
		/*
		 * RX Flow control is enabled and TX Flow control is disabled
		 * by a software over-ride. Since there really isn't a way to
		 * advertise that we are capable of RX Pause ONLY, we will
		 * advertise that we support both symmetric and asymmetric RX
		 * PAUSE.  Later, we will disable the adapter's ability to send
		 * PAUSE frames.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	case e1000_fc_tx_pause:
		/*
		 * TX Flow control is enabled, and RX Flow control is disabled,
		 * by a software over-ride.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
		break;
	case e1000_fc_full:
		/*
		 * Flow control (both RX and TX) is enabled by a software
		 * over-ride.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
		break;
	}

	E1000_WRITE_REG(hw, E1000_TXCW, txcw);
	mac->txcw = txcw;

out:
	return ret_val;
}

/**
 *  e1000_set_fc_watermarks_generic - Set flow control high/low watermarks
 *  @hw: pointer to the HW structure
 *
 *  Sets the flow control high/low threshold (watermark) registers.  If
 *  flow control XON frame transmission is enabled, then set XON frame
 *  tansmission as well.
 **/
s32 e1000_set_fc_watermarks_generic(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u32 fcrtl = 0, fcrth = 0;

	DEBUGFUNC("e1000_set_fc_watermarks_generic");

	/*
	 * Set the flow control receive threshold registers.  Normally,
	 * these registers will be set to a default threshold that may be
	 * adjusted later by the driver's runtime code.  However, if the
	 * ability to transmit pause frames is not enabled, then these
	 * registers will be set to 0.
	 */
	if (hw->fc.type & e1000_fc_tx_pause) {
		/*
		 * We need to set up the Receive Threshold high and low water
		 * marks as well as (optionally) enabling the transmission of
		 * XON frames.
		 */
		fcrtl = hw->fc.low_water;
		if (hw->fc.send_xon)
			fcrtl |= E1000_FCRTL_XONE;

		fcrth = hw->fc.high_water;
	}
	E1000_WRITE_REG(hw, E1000_FCRTL, fcrtl);
	E1000_WRITE_REG(hw, E1000_FCRTH, fcrth);

	return ret_val;
}

/**
 *  e1000_set_default_fc_generic - Set flow control default values
 *  @hw: pointer to the HW structure
 *
 *  Read the EEPROM for the default values for flow control and store the
 *  values.
 **/
s32 e1000_set_default_fc_generic(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 nvm_data;

	DEBUGFUNC("e1000_set_default_fc_generic");

	/*
	 * Read and store word 0x0F of the EEPROM. This word contains bits
	 * that determine the hardware's default PAUSE (flow control) mode,
	 * a bit that determines whether the HW defaults to enabling or
	 * disabling auto-negotiation, and the direction of the
	 * SW defined pins. If there is no SW over-ride of the flow
	 * control setting, then the variable hw->fc will
	 * be initialized based on a value in the EEPROM.
	 */
	ret_val = e1000_read_nvm(hw, NVM_INIT_CONTROL2_REG, 1, &nvm_data);

	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		goto out;
	}

	if ((nvm_data & NVM_WORD0F_PAUSE_MASK) == 0)
		hw->fc.type = e1000_fc_none;
	else if ((nvm_data & NVM_WORD0F_PAUSE_MASK) ==
		 NVM_WORD0F_ASM_DIR)
		hw->fc.type = e1000_fc_tx_pause;
	else
		hw->fc.type = e1000_fc_full;

out:
	return ret_val;
}

/**
 *  e1000_force_mac_fc_generic - Force the MAC's flow control settings
 *  @hw: pointer to the HW structure
 *
 *  Force the MAC's flow control settings.  Sets the TFCE and RFCE bits in the
 *  device control register to reflect the adapter settings.  TFCE and RFCE
 *  need to be explicitly set by software when a copper PHY is used because
 *  autonegotiation is managed by the PHY rather than the MAC.  Software must
 *  also configure these bits when link is forced on a fiber connection.
 **/
s32 e1000_force_mac_fc_generic(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_force_mac_fc_generic");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);

	/*
	 * Because we didn't get link via the internal auto-negotiation
	 * mechanism (we either forced link or we got link via PHY
	 * auto-neg), we have to manually enable/disable transmit an
	 * receive flow control.
	 *
	 * The "Case" statement below enables/disable flow control
	 * according to the "hw->fc.type" parameter.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause
	 *          frames but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *          frames but we do not receive pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) is enabled.
	 *  other:  No other values should be possible at this point.
	 */
	DEBUGOUT1("hw->fc.type = %u\n", hw->fc.type);

	switch (hw->fc.type) {
	case e1000_fc_none:
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		break;
	case e1000_fc_rx_pause:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;
		break;
	case e1000_fc_tx_pause:
		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case e1000_fc_full:
		ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

out:
	return ret_val;
}

/**
 *  e1000_config_fc_after_link_up_generic - Configures flow control after link
 *  @hw: pointer to the HW structure
 *
 *  Checks the status of auto-negotiation after link up to ensure that the
 *  speed and duplex were not forced.  If the link needed to be forced, then
 *  flow control needs to be forced also.  If auto-negotiation is enabled
 *  and did not fail, then we configure flow control based on our link
 *  partner.
 **/
s32 e1000_config_fc_after_link_up_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val = E1000_SUCCESS;
	u16 mii_status_reg, mii_nway_adv_reg, mii_nway_lp_ability_reg;
	u16 speed, duplex;

	DEBUGFUNC("e1000_config_fc_after_link_up_generic");

	/*
	 * Check for the case where we have fiber media and auto-neg failed
	 * so we had to force link.  In this case, we need to force the
	 * configuration of the MAC to match the "fc" parameter.
	 */
	if (mac->autoneg_failed) {
		if (hw->phy.media_type == e1000_media_type_fiber ||
		    hw->phy.media_type == e1000_media_type_internal_serdes)
			ret_val = e1000_force_mac_fc_generic(hw);
	} else {
		if (hw->phy.media_type == e1000_media_type_copper)
			ret_val = e1000_force_mac_fc_generic(hw);
	}

	if (ret_val) {
		DEBUGOUT("Error forcing flow control settings\n");
		goto out;
	}

	/*
	 * Check for the case where we have copper media and auto-neg is
	 * enabled.  In this case, we need to check and see if Auto-Neg
	 * has completed, and if so, how the PHY and link partner has
	 * flow control configured.
	 */
	if ((hw->phy.media_type == e1000_media_type_copper) && mac->autoneg) {
		/*
		 * Read the MII Status Register and check to see if AutoNeg
		 * has completed.  We read this twice because this reg has
		 * some "sticky" (latched) bits.
		 */
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			goto out;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			goto out;

		if (!(mii_status_reg & MII_SR_AUTONEG_COMPLETE)) {
			DEBUGOUT("Copper PHY and Auto Neg "
			         "has not completed.\n");
			goto out;
		}

		/*
		 * The AutoNeg process has completed, so we now need to
		 * read both the Auto Negotiation Advertisement
		 * Register (Address 4) and the Auto_Negotiation Base
		 * Page Ability Register (Address 5) to determine how
		 * flow control was negotiated.
		 */
		ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_ADV,
					    &mii_nway_adv_reg);
		if (ret_val)
			goto out;
		ret_val = e1000_read_phy_reg(hw, PHY_LP_ABILITY,
					    &mii_nway_lp_ability_reg);
		if (ret_val)
			goto out;

		/*
		 * Two bits in the Auto Negotiation Advertisement Register
		 * (Address 4) and two bits in the Auto Negotiation Base
		 * Page Ability Register (Address 5) determine flow control
		 * for both the PHY and the link partner.  The following
		 * table, taken out of the IEEE 802.3ab/D6.0 dated March 25,
		 * 1999, describes these PAUSE resolution bits and how flow
		 * control is determined based upon these settings.
		 * NOTE:  DC = Don't Care
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
		 *-------|---------|-------|---------|--------------------
		 *   0   |    0    |  DC   |   DC    | e1000_fc_none
		 *   0   |    1    |   0   |   DC    | e1000_fc_none
		 *   0   |    1    |   1   |    0    | e1000_fc_none
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 *   1   |    0    |   0   |   DC    | e1000_fc_none
		 *   1   |   DC    |   1   |   DC    | e1000_fc_full
		 *   1   |    1    |   0   |    0    | e1000_fc_none
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 *
		 * Are both PAUSE bits set to 1?  If so, this implies
		 * Symmetric Flow Control is enabled at both ends.  The
		 * ASM_DIR bits are irrelevant per the spec.
		 *
		 * For Symmetric Flow Control:
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |   DC    |   1   |   DC    | E1000_fc_full
		 *
		 */
		if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
			/*
			 * Now we need to check if the user selected RX ONLY
			 * of pause frames.  In this case, we had to advertise
			 * FULL flow control because we could not advertise RX
			 * ONLY. Hence, we must now check to see if we need to
			 * turn OFF  the TRANSMISSION of PAUSE frames.
			 */
			if (hw->fc.original_type == e1000_fc_full) {
				hw->fc.type = e1000_fc_full;
				DEBUGOUT("Flow Control = FULL.\r\n");
			} else {
				hw->fc.type = e1000_fc_rx_pause;
				DEBUGOUT("Flow Control = "
				         "RX PAUSE frames only.\r\n");
			}
		}
		/*
		 * For receiving PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 */
		else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		          (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
		          (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
		          (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.type = e1000_fc_tx_pause;
			DEBUGOUT("Flow Control = TX PAUSE frames only.\r\n");
		}
		/*
		 * For transmitting PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 */
		else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		         (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
		         !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
		         (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.type = e1000_fc_rx_pause;
			DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
		}
		/*
		 * Per the IEEE spec, at this point flow control should be
		 * disabled.  However, we want to consider that we could
		 * be connected to a legacy switch that doesn't advertise
		 * desired flow control, but can be forced on the link
		 * partner.  So if we advertised no flow control, that is
		 * what we will resolve to.  If we advertised some kind of
		 * receive capability (Rx Pause Only or Full Flow Control)
		 * and the link partner advertised none, we will configure
		 * ourselves to enable Rx Flow Control only.  We can do
		 * this safely for two reasons:  If the link partner really
		 * didn't want flow control enabled, and we enable Rx, no
		 * harm done since we won't be receiving any PAUSE frames
		 * anyway.  If the intent on the link partner was to have
		 * flow control enabled, then by us enabling RX only, we
		 * can at least receive pause frames and process them.
		 * This is a good idea because in most cases, since we are
		 * predominantly a server NIC, more times than not we will
		 * be asked to delay transmission of packets than asking
		 * our link partner to pause transmission of frames.
		 */
		else if ((hw->fc.original_type == e1000_fc_none ||
		          hw->fc.original_type == e1000_fc_tx_pause) ||
		         hw->fc.strict_ieee) {
			hw->fc.type = e1000_fc_none;
			DEBUGOUT("Flow Control = NONE.\r\n");
		} else {
			hw->fc.type = e1000_fc_rx_pause;
			DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
		}

		/*
		 * Now we need to do one last check...  If we auto-
		 * negotiated to HALF DUPLEX, flow control should not be
		 * enabled per IEEE 802.3 spec.
		 */
		ret_val = e1000_get_speed_and_duplex(hw, &speed, &duplex);
		if (ret_val) {
			DEBUGOUT("Error getting link speed and duplex\n");
			goto out;
		}

		if (duplex == HALF_DUPLEX)
			hw->fc.type = e1000_fc_none;

		/*
		 * Now we call a subroutine to actually force the MAC
		 * controller to use the correct flow control settings.
		 */
		ret_val = e1000_force_mac_fc_generic(hw);
		if (ret_val) {
			DEBUGOUT("Error forcing flow control settings\n");
			goto out;
		}
	}

out:
	return ret_val;
}

/**
 *  e1000_get_speed_and_duplex_copper_generic - Retreive current speed/duplex
 *  @hw: pointer to the HW structure
 *  @speed: stores the current speed
 *  @duplex: stores the current duplex
 *
 *  Read the status register for the current speed/duplex and store the current
 *  speed and duplex for copper connections.
 **/
s32 e1000_get_speed_and_duplex_copper_generic(struct e1000_hw *hw, u16 *speed,
                                              u16 *duplex)
{
	u32 status;

	DEBUGFUNC("e1000_get_speed_and_duplex_copper_generic");

	status = E1000_READ_REG(hw, E1000_STATUS);
	if (status & E1000_STATUS_SPEED_1000) {
		*speed = SPEED_1000;
		DEBUGOUT("1000 Mbs, ");
	} else if (status & E1000_STATUS_SPEED_100) {
		*speed = SPEED_100;
		DEBUGOUT("100 Mbs, ");
	} else {
		*speed = SPEED_10;
		DEBUGOUT("10 Mbs, ");
	}

	if (status & E1000_STATUS_FD) {
		*duplex = FULL_DUPLEX;
		DEBUGOUT("Full Duplex\n");
	} else {
		*duplex = HALF_DUPLEX;
		DEBUGOUT("Half Duplex\n");
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_get_speed_and_duplex_fiber_generic - Retreive current speed/duplex
 *  @hw: pointer to the HW structure
 *  @speed: stores the current speed
 *  @duplex: stores the current duplex
 *
 *  Sets the speed and duplex to gigabit full duplex (the only possible option)
 *  for fiber/serdes links.
 **/
s32 e1000_get_speed_and_duplex_fiber_serdes_generic(struct e1000_hw *hw,
                                                    u16 *speed, u16 *duplex)
{
	DEBUGFUNC("e1000_get_speed_and_duplex_fiber_serdes_generic");
	UNREFERENCED_PARAMETER(hw);

	*speed = SPEED_1000;
	*duplex = FULL_DUPLEX;

	return E1000_SUCCESS;
}

/**
 *  e1000_get_hw_semaphore_generic - Acquire hardware semaphore
 *  @hw: pointer to the HW structure
 *
 *  Acquire the HW semaphore to access the PHY or NVM
 **/
s32 e1000_get_hw_semaphore_generic(struct e1000_hw *hw)
{
	u32 swsm;
	s32 ret_val = E1000_SUCCESS;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	DEBUGFUNC("e1000_get_hw_semaphore_generic");

	/* Get the SW semaphore */
	while (i < timeout) {
		swsm = E1000_READ_REG(hw, E1000_SWSM);
		if (!(swsm & E1000_SWSM_SMBI))
			break;

		usec_delay(50);
		i++;
	}

	if (i == timeout) {
		DEBUGOUT("Driver can't access device - SMBI bit is set.\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
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
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_put_hw_semaphore_generic - Release hardware semaphore
 *  @hw: pointer to the HW structure
 *
 *  Release hardware semaphore used to access the PHY or NVM
 **/
void e1000_put_hw_semaphore_generic(struct e1000_hw *hw)
{
	u32 swsm;

	DEBUGFUNC("e1000_put_hw_semaphore_generic");

	swsm = E1000_READ_REG(hw, E1000_SWSM);

	swsm &= ~(E1000_SWSM_SMBI | E1000_SWSM_SWESMBI);

	E1000_WRITE_REG(hw, E1000_SWSM, swsm);
}

/**
 *  e1000_get_auto_rd_done_generic - Check for auto read completion
 *  @hw: pointer to the HW structure
 *
 *  Check EEPROM for Auto Read done bit.
 **/
s32 e1000_get_auto_rd_done_generic(struct e1000_hw *hw)
{
	s32 i = 0;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_get_auto_rd_done_generic");

	while (i < AUTO_READ_DONE_TIMEOUT) {
		if (E1000_READ_REG(hw, E1000_EECD) & E1000_EECD_AUTO_RD)
			break;
		msec_delay(1);
		i++;
	}

	if (i == AUTO_READ_DONE_TIMEOUT) {
		DEBUGOUT("Auto read by HW from NVM has not completed.\n");
		ret_val = -E1000_ERR_RESET;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_valid_led_default_generic - Verify a valid default LED config
 *  @hw: pointer to the HW structure
 *  @data: pointer to the NVM (EEPROM)
 *
 *  Read the EEPROM for the current default LED configuration.  If the
 *  LED configuration is not valid, set to a valid LED configuration.
 **/
s32 e1000_valid_led_default_generic(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	DEBUGFUNC("e1000_valid_led_default_generic");

	ret_val = e1000_read_nvm(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		goto out;
	}

	if (*data == ID_LED_RESERVED_0000 || *data == ID_LED_RESERVED_FFFF)
		*data = ID_LED_DEFAULT;

out:
	return ret_val;
}

/**
 *  e1000_id_led_init_generic -
 *  @hw: pointer to the HW structure
 *
 **/
s32 e1000_id_led_init_generic(struct e1000_hw * hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	const u32 ledctl_mask = 0x000000FF;
	const u32 ledctl_on = E1000_LEDCTL_MODE_LED_ON;
	const u32 ledctl_off = E1000_LEDCTL_MODE_LED_OFF;
	u16 data, i, temp;
	const u16 led_mask = 0x0F;

	DEBUGFUNC("e1000_id_led_init_generic");

	ret_val = hw->func.valid_led_default(hw, &data);
	if (ret_val)
		goto out;

	mac->ledctl_default = E1000_READ_REG(hw, E1000_LEDCTL);
	mac->ledctl_mode1 = mac->ledctl_default;
	mac->ledctl_mode2 = mac->ledctl_default;

	for (i = 0; i < 4; i++) {
		temp = (data >> (i << 2)) & led_mask;
		switch (temp) {
		case ID_LED_ON1_DEF2:
		case ID_LED_ON1_ON2:
		case ID_LED_ON1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_on << (i << 3);
			break;
		case ID_LED_OFF1_DEF2:
		case ID_LED_OFF1_ON2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
		switch (temp) {
		case ID_LED_DEF1_ON2:
		case ID_LED_ON1_ON2:
		case ID_LED_OFF1_ON2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_on << (i << 3);
			break;
		case ID_LED_DEF1_OFF2:
		case ID_LED_ON1_OFF2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
	}

out:
	return ret_val;
}

/**
 *  e1000_setup_led_generic - Configures SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This prepares the SW controllable LED for use and saves the current state
 *  of the LED so it can be later restored.
 **/
s32 e1000_setup_led_generic(struct e1000_hw *hw)
{
	u32 ledctl;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_setup_led_generic");

	if (hw->func.setup_led != e1000_setup_led_generic) {
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	if (hw->phy.media_type == e1000_media_type_fiber) {
		ledctl = E1000_READ_REG(hw, E1000_LEDCTL);
		hw->mac.ledctl_default = ledctl;
		/* Turn off LED0 */
		ledctl &= ~(E1000_LEDCTL_LED0_IVRT |
		            E1000_LEDCTL_LED0_BLINK |
		            E1000_LEDCTL_LED0_MODE_MASK);
		ledctl |= (E1000_LEDCTL_MODE_LED_OFF <<
		           E1000_LEDCTL_LED0_MODE_SHIFT);
		E1000_WRITE_REG(hw, E1000_LEDCTL, ledctl);
	} else if (hw->phy.media_type == e1000_media_type_copper) {
		E1000_WRITE_REG(hw, E1000_LEDCTL, hw->mac.ledctl_mode1);
	}

out:
	return ret_val;
}

/**
 *  e1000_cleanup_led_generic - Set LED config to default operation
 *  @hw: pointer to the HW structure
 *
 *  Remove the current LED configuration and set the LED configuration
 *  to the default value, saved from the EEPROM.
 **/
s32 e1000_cleanup_led_generic(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_cleanup_led_generic");

	if (hw->func.cleanup_led != e1000_cleanup_led_generic) {
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	E1000_WRITE_REG(hw, E1000_LEDCTL, hw->mac.ledctl_default);

out:
	return ret_val;
}

/**
 *  e1000_blink_led_generic - Blink LED
 *  @hw: pointer to the HW structure
 *
 *  Blink the led's which are set to be on.
 **/
s32 e1000_blink_led_generic(struct e1000_hw *hw)
{
	u32 ledctl_blink = 0;
	u32 i;

	DEBUGFUNC("e1000_blink_led_generic");

	if (hw->phy.media_type == e1000_media_type_fiber) {
		/* always blink LED0 for PCI-E fiber */
		ledctl_blink = E1000_LEDCTL_LED0_BLINK |
		     (E1000_LEDCTL_MODE_LED_ON << E1000_LEDCTL_LED0_MODE_SHIFT);
	} else {
		/*
		 * set the blink bit for each LED that's "on" (0x0E)
		 * in ledctl_mode2
		 */
		ledctl_blink = hw->mac.ledctl_mode2;
		for (i = 0; i < 4; i++)
			if (((hw->mac.ledctl_mode2 >> (i * 8)) & 0xFF) ==
			    E1000_LEDCTL_MODE_LED_ON)
				ledctl_blink |= (E1000_LEDCTL_LED0_BLINK <<
				                 (i * 8));
	}

	E1000_WRITE_REG(hw, E1000_LEDCTL, ledctl_blink);

	return E1000_SUCCESS;
}

/**
 *  e1000_led_on_generic - Turn LED on
 *  @hw: pointer to the HW structure
 *
 *  Turn LED on.
 **/
s32 e1000_led_on_generic(struct e1000_hw *hw)
{
	u32 ctrl;

	DEBUGFUNC("e1000_led_on_generic");

	switch (hw->phy.media_type) {
	case e1000_media_type_fiber:
		ctrl = E1000_READ_REG(hw, E1000_CTRL);
		ctrl &= ~E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl);
		break;
	case e1000_media_type_copper:
		E1000_WRITE_REG(hw, E1000_LEDCTL, hw->mac.ledctl_mode2);
		break;
	default:
		break;
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_led_off_generic - Turn LED off
 *  @hw: pointer to the HW structure
 *
 *  Turn LED off.
 **/
s32 e1000_led_off_generic(struct e1000_hw *hw)
{
	u32 ctrl;

	DEBUGFUNC("e1000_led_off_generic");

	switch (hw->phy.media_type) {
	case e1000_media_type_fiber:
		ctrl = E1000_READ_REG(hw, E1000_CTRL);
		ctrl |= E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl);
		break;
	case e1000_media_type_copper:
		E1000_WRITE_REG(hw, E1000_LEDCTL, hw->mac.ledctl_mode1);
		break;
	default:
		break;
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_set_pcie_no_snoop_generic - Set PCI-express capabilities
 *  @hw: pointer to the HW structure
 *  @no_snoop: bitmap of snoop events
 *
 *  Set the PCI-express register to snoop for events enabled in 'no_snoop'.
 **/
void e1000_set_pcie_no_snoop_generic(struct e1000_hw *hw, u32 no_snoop)
{
	u32 gcr;

	DEBUGFUNC("e1000_set_pcie_no_snoop_generic");

	if (hw->bus.type != e1000_bus_type_pci_express)
		goto out;

	if (no_snoop) {
		gcr = E1000_READ_REG(hw, E1000_GCR);
		gcr &= ~(PCIE_NO_SNOOP_ALL);
		gcr |= no_snoop;
		E1000_WRITE_REG(hw, E1000_GCR, gcr);
	}
out:
	return;
}

/**
 *  e1000_disable_pcie_master_generic - Disables PCI-express master access
 *  @hw: pointer to the HW structure
 *
 *  Returns 0 (E1000_SUCCESS) if successful, else returns -10
 *  (-E1000_ERR_MASTER_REQUESTS_PENDING) if master disable bit has not casued
 *  the master requests to be disabled.
 *
 *  Disables PCI-Express master access and verifies there are no pending
 *  requests.
 **/
s32 e1000_disable_pcie_master_generic(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 timeout = MASTER_DISABLE_TIMEOUT;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_disable_pcie_master_generic");

	if (hw->bus.type != e1000_bus_type_pci_express)
		goto out;

	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	ctrl |= E1000_CTRL_GIO_MASTER_DISABLE;
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

	while (timeout) {
		if (!(E1000_READ_REG(hw, E1000_STATUS) &
		      E1000_STATUS_GIO_MASTER_ENABLE))
			break;
		usec_delay(100);
		timeout--;
	}

	if (!timeout) {
		DEBUGOUT("Master requests are pending.\n");
		ret_val = -E1000_ERR_MASTER_REQUESTS_PENDING;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_reset_adaptive_generic - Reset Adaptive Interframe Spacing
 *  @hw: pointer to the HW structure
 *
 *  Reset the Adaptive Interframe Spacing throttle to default values.
 **/
void e1000_reset_adaptive_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;

	DEBUGFUNC("e1000_reset_adaptive_generic");

	if (!mac->adaptive_ifs) {
		DEBUGOUT("Not in Adaptive IFS mode!\n");
		goto out;
	}

	if (!mac->ifs_params_forced) {
		mac->current_ifs_val = 0;
		mac->ifs_min_val = IFS_MIN;
		mac->ifs_max_val = IFS_MAX;
		mac->ifs_step_size = IFS_STEP;
		mac->ifs_ratio = IFS_RATIO;
	}

	mac->in_ifs_mode = FALSE;
	E1000_WRITE_REG(hw, E1000_AIT, 0);
out:
	return;
}

/**
 *  e1000_update_adaptive_generic - Update Adaptive Interframe Spacing
 *  @hw: pointer to the HW structure
 *
 *  Update the Adaptive Interframe Spacing Throttle value based on the
 *  time between transmitted packets and time between collisions.
 **/
void e1000_update_adaptive_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;

	DEBUGFUNC("e1000_update_adaptive_generic");

	if (!mac->adaptive_ifs) {
		DEBUGOUT("Not in Adaptive IFS mode!\n");
		goto out;
	}

	if ((mac->collision_delta * mac->ifs_ratio) > mac->tx_packet_delta) {
		if (mac->tx_packet_delta > MIN_NUM_XMITS) {
			mac->in_ifs_mode = TRUE;
			if (mac->current_ifs_val < mac->ifs_max_val) {
				if (!mac->current_ifs_val)
					mac->current_ifs_val = mac->ifs_min_val;
				else
					mac->current_ifs_val +=
						mac->ifs_step_size;
				E1000_WRITE_REG(hw, E1000_AIT, mac->current_ifs_val);
			}
		}
	} else {
		if (mac->in_ifs_mode &&
		    (mac->tx_packet_delta <= MIN_NUM_XMITS)) {
			mac->current_ifs_val = 0;
			mac->in_ifs_mode = FALSE;
			E1000_WRITE_REG(hw, E1000_AIT, 0);
		}
	}
out:
	return;
}

/**
 *  e1000_validate_mdi_setting_generic - Verify MDI/MDIx settings
 *  @hw: pointer to the HW structure
 *
 *  Verify that when not using auto-negotitation that MDI/MDIx is correctly
 *  set, which is forced to MDI mode only.
 **/
s32 e1000_validate_mdi_setting_generic(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_validate_mdi_setting_generic");

	if (!hw->mac.autoneg && (hw->phy.mdix == 0 || hw->phy.mdix == 3)) {
		DEBUGOUT("Invalid MDI setting detected\n");
		hw->phy.mdix = 1;
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_write_8bit_ctrl_reg_generic - Write a 8bit CTRL register
 *  @hw: pointer to the HW structure
 *  @reg: 32bit register offset such as E1000_SCTL
 *  @offset: register offset to write to
 *  @data: data to write at register offset
 *
 *  Writes an address/data control type register.  There are several of these
 *  and they all have the format address << 8 | data and bit 31 is polled for
 *  completion.
 **/
s32 e1000_write_8bit_ctrl_reg_generic(struct e1000_hw *hw, u32 reg,
                                      u32 offset, u8 data)
{
	u32 i, regvalue = 0;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_write_8bit_ctrl_reg_generic");

	/* Set up the address and data */
	regvalue = ((u32)data) | (offset << E1000_GEN_CTL_ADDRESS_SHIFT);
	E1000_WRITE_REG(hw, reg, regvalue);

	/* Poll the ready bit to see if the MDI read completed */
	for (i = 0; i < E1000_GEN_POLL_TIMEOUT; i++) {
		usec_delay(5);
		regvalue = E1000_READ_REG(hw, reg);
		if (regvalue & E1000_GEN_CTL_READY)
			break;
	}
	if (!(regvalue & E1000_GEN_CTL_READY)) {
		DEBUGOUT1("Reg %08x did not indicate ready\n", reg);
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

out:
	return ret_val;
}
