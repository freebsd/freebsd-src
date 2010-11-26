/******************************************************************************

  Copyright (c) 2001-2010, Intel Corporation 
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


#include "ixgbe_api.h"
#include "ixgbe_type.h"
#include "ixgbe_vf.h"

s32 ixgbe_init_ops_vf(struct ixgbe_hw *hw);
s32 ixgbe_init_hw_vf(struct ixgbe_hw *hw);
s32 ixgbe_start_hw_vf(struct ixgbe_hw *hw);
s32 ixgbe_reset_hw_vf(struct ixgbe_hw *hw);
s32 ixgbe_stop_hw_vf(struct ixgbe_hw *hw);
u32 ixgbe_get_num_of_tx_queues_vf(struct ixgbe_hw *hw);
u32 ixgbe_get_num_of_rx_queues_vf(struct ixgbe_hw *hw);
s32 ixgbe_get_mac_addr_vf(struct ixgbe_hw *hw, u8 *mac_addr);
s32 ixgbe_setup_mac_link_vf(struct ixgbe_hw *hw,
                                  ixgbe_link_speed speed, bool autoneg,
                                  bool autoneg_wait_to_complete);
s32 ixgbe_check_mac_link_vf(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
                            bool *link_up, bool autoneg_wait_to_complete);
s32 ixgbe_set_rar_vf(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
		      u32 enable_addr);
s32 ixgbe_update_mc_addr_list_vf(struct ixgbe_hw *hw, u8 *mc_addr_list,
				 u32 mc_addr_count, ixgbe_mc_addr_itr);
s32 ixgbe_set_vfta_vf(struct ixgbe_hw *hw, u32 vlan, u32 vind, bool vlan_on);

#ifndef IXGBE_VFWRITE_REG
#define IXGBE_VFWRITE_REG IXGBE_WRITE_REG
#endif
#ifndef IXGBE_VFREAD_REG
#define IXGBE_VFREAD_REG IXGBE_READ_REG
#endif

/**
 *  ixgbe_init_ops_vf - Initialize the pointers for vf
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers, adapter-specific functions can
 *  override the assignment of generic function pointers by assigning
 *  their own adapter-specific function pointers.
 *  Does not touch the hardware.
 **/
s32 ixgbe_init_ops_vf(struct ixgbe_hw *hw)
{
	/* MAC */
	hw->mac.ops.init_hw = ixgbe_init_hw_vf;
	hw->mac.ops.reset_hw = ixgbe_reset_hw_vf;
	hw->mac.ops.start_hw = ixgbe_start_hw_vf;
	/* Cannot clear stats on VF */
	hw->mac.ops.clear_hw_cntrs = NULL;
	hw->mac.ops.get_media_type = NULL;
	hw->mac.ops.get_mac_addr = ixgbe_get_mac_addr_vf;
	hw->mac.ops.stop_adapter = ixgbe_stop_hw_vf;
	hw->mac.ops.get_bus_info = NULL;

	/* Link */
	hw->mac.ops.setup_link = ixgbe_setup_mac_link_vf;
	hw->mac.ops.check_link = ixgbe_check_mac_link_vf;
	hw->mac.ops.get_link_capabilities = NULL;

	/* RAR, Multicast, VLAN */
	hw->mac.ops.set_rar = ixgbe_set_rar_vf;
	hw->mac.ops.init_rx_addrs = NULL;
	hw->mac.ops.update_mc_addr_list = ixgbe_update_mc_addr_list_vf;
	hw->mac.ops.enable_mc = NULL;
	hw->mac.ops.disable_mc = NULL;
	hw->mac.ops.clear_vfta = NULL;
	hw->mac.ops.set_vfta = ixgbe_set_vfta_vf;

	hw->mac.max_tx_queues = 1;
	hw->mac.max_rx_queues = 1;

	hw->mbx.ops.init_params = ixgbe_init_mbx_params_vf;

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_start_hw_vf - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware by filling the bus info structure and media type, clears
 *  all on chip counters, initializes receive address registers, multicast
 *  table, VLAN filter table, calls routine to set up link and flow control
 *  settings, and leaves transmit and receive units disabled and uninitialized
 **/
s32 ixgbe_start_hw_vf(struct ixgbe_hw *hw)
{
	/* Clear adapter stopped flag */
	hw->adapter_stopped = FALSE;

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_init_hw_vf - virtual function hardware initialization
 *  @hw: pointer to hardware structure
 *
 *  Initialize the hardware by resetting the hardware and then starting
 *  the hardware
 **/
s32 ixgbe_init_hw_vf(struct ixgbe_hw *hw)
{
	s32 status = hw->mac.ops.start_hw(hw);

	hw->mac.ops.get_mac_addr(hw, hw->mac.addr);

	return status;
}

/**
 *  ixgbe_reset_hw_vf - Performs hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by reseting the transmit and receive units, masks and
 *  clears all interrupts.
 **/
s32 ixgbe_reset_hw_vf(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 timeout = IXGBE_VF_INIT_TIMEOUT;
	s32 ret_val = IXGBE_ERR_INVALID_MAC_ADDR;
	u32 ctrl, msgbuf[IXGBE_VF_PERMADDR_MSG_LEN];
	u8 *addr = (u8 *)(&msgbuf[1]);

	DEBUGFUNC("ixgbevf_reset_hw_vf");

	/* Call adapter stop to disable tx/rx and clear interrupts */
	hw->mac.ops.stop_adapter(hw);

	DEBUGOUT("Issuing a function level reset to MAC\n");
		ctrl = IXGBE_VFREAD_REG(hw, IXGBE_VFCTRL);
		IXGBE_VFWRITE_REG(hw, IXGBE_VFCTRL, (ctrl | IXGBE_CTRL_RST));
		IXGBE_WRITE_FLUSH(hw);

	usec_delay(1);

	/* we cannot reset while the RSTI / RSTD bits are asserted */
	while (!mbx->ops.check_for_rst(hw, 0) && timeout) {
		timeout--;
		usec_delay(5);
	}

	if (timeout) {
		/* mailbox timeout can now become active */
		mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;

		msgbuf[0] = IXGBE_VF_RESET;
		mbx->ops.write_posted(hw, msgbuf, 1, 0);

		msec_delay(10);

		/* set our "perm_addr" based on info provided by PF */
		/* also set up the mc_filter_type which is piggy backed
		 * on the mac address in word 3 */
		ret_val = mbx->ops.read_posted(hw, msgbuf,
					       IXGBE_VF_PERMADDR_MSG_LEN, 0);
		if (!ret_val) {
			if (msgbuf[0] == (IXGBE_VF_RESET |
					  IXGBE_VT_MSGTYPE_ACK)) {
				memcpy(hw->mac.perm_addr, addr,
				       IXGBE_ETH_LENGTH_OF_ADDRESS);
				hw->mac.mc_filter_type =
					msgbuf[IXGBE_VF_MC_TYPE_WORD];
			} else {
				ret_val = IXGBE_ERR_INVALID_MAC_ADDR;
			}
		}
	}

	return ret_val;
}

/**
 *  ixgbe_stop_hw_vf - Generic stop Tx/Rx units
 *  @hw: pointer to hardware structure
 *
 *  Sets the adapter_stopped flag within ixgbe_hw struct. Clears interrupts,
 *  disables transmit and receive units. The adapter_stopped flag is used by
 *  the shared code and drivers to determine if the adapter is in a stopped
 *  state and should not touch the hardware.
 **/
s32 ixgbe_stop_hw_vf(struct ixgbe_hw *hw)
{
	u32 number_of_queues;
	u32 reg_val;
	u16 i;

	/*
	 * Set the adapter_stopped flag so other driver functions stop touching
	 * the hardware
	 */
	hw->adapter_stopped = TRUE;

	/* Disable the receive unit by stopped each queue */
	number_of_queues = hw->mac.max_rx_queues;
	for (i = 0; i < number_of_queues; i++) {
		reg_val = IXGBE_VFREAD_REG(hw, IXGBE_VFRXDCTL(i));
		if (reg_val & IXGBE_RXDCTL_ENABLE) {
			reg_val &= ~IXGBE_RXDCTL_ENABLE;
			IXGBE_VFWRITE_REG(hw, IXGBE_VFRXDCTL(i), reg_val);
		}
	}

	IXGBE_WRITE_FLUSH(hw);

	/* Clear interrupt mask to stop from interrupts being generated */
	IXGBE_VFWRITE_REG(hw, IXGBE_VTEIMC, IXGBE_VF_IRQ_CLEAR_MASK);

	/* Clear any pending interrupts */
	IXGBE_VFREAD_REG(hw, IXGBE_VTEICR);

	/* Disable the transmit unit.  Each queue must be disabled. */
	number_of_queues = hw->mac.max_tx_queues;
	for (i = 0; i < number_of_queues; i++) {
		reg_val = IXGBE_VFREAD_REG(hw, IXGBE_VFTXDCTL(i));
		if (reg_val & IXGBE_TXDCTL_ENABLE) {
			reg_val &= ~IXGBE_TXDCTL_ENABLE;
			IXGBE_VFWRITE_REG(hw, IXGBE_VFTXDCTL(i), reg_val);
		}
	}

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_mta_vector - Determines bit-vector in multicast table to set
 *  @hw: pointer to hardware structure
 *  @mc_addr: the multicast address
 *
 *  Extracts the 12 bits, from a multicast address, to determine which
 *  bit-vector to set in the multicast table. The hardware uses 12 bits, from
 *  incoming rx multicast addresses, to determine the bit-vector to check in
 *  the MTA. Which of the 4 combination, of 12-bits, the hardware uses is set
 *  by the MO field of the MCSTCTRL. The MO field is set during initialization
 *  to mc_filter_type.
 **/
static s32 ixgbe_mta_vector(struct ixgbe_hw *hw, u8 *mc_addr)
{
	u32 vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   /* use bits [47:36] of the address */
		vector = ((mc_addr[4] >> 4) | (((u16)mc_addr[5]) << 4));
		break;
	case 1:   /* use bits [46:35] of the address */
		vector = ((mc_addr[4] >> 3) | (((u16)mc_addr[5]) << 5));
		break;
	case 2:   /* use bits [45:34] of the address */
		vector = ((mc_addr[4] >> 2) | (((u16)mc_addr[5]) << 6));
		break;
	case 3:   /* use bits [43:32] of the address */
		vector = ((mc_addr[4]) | (((u16)mc_addr[5]) << 8));
		break;
	default:  /* Invalid mc_filter_type */
		DEBUGOUT("MC filter type param set incorrectly\n");
		ASSERT(0);
		break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

/**
 *  ixgbe_set_rar_vf - set device MAC address
 *  @hw: pointer to hardware structure
 *  @index: Receive address register to write
 *  @addr: Address to put into receive address register
 *  @vmdq: VMDq "set" or "pool" index
 *  @enable_addr: set flag that address is active
 **/
s32 ixgbe_set_rar_vf(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
		      u32 enable_addr)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[3];
	u8 *msg_addr = (u8 *)(&msgbuf[1]);
	s32 ret_val;
	UNREFERENCED_PARAMETER(vmdq);
	UNREFERENCED_PARAMETER(enable_addr);
	UNREFERENCED_PARAMETER(index);

	memset(msgbuf, 0, 12);
	msgbuf[0] = IXGBE_VF_SET_MAC_ADDR;
	memcpy(msg_addr, addr, 6);
	ret_val = mbx->ops.write_posted(hw, msgbuf, 3, 0);

	if (!ret_val)
		ret_val = mbx->ops.read_posted(hw, msgbuf, 3, 0);

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;

	/* if nacked the address was rejected, use "perm_addr" */
	if (!ret_val &&
	    (msgbuf[0] == (IXGBE_VF_SET_MAC_ADDR | IXGBE_VT_MSGTYPE_NACK)))
		ixgbe_get_mac_addr_vf(hw, hw->mac.addr);

	return ret_val;
}

/**
 *  ixgbe_update_mc_addr_list_vf - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *  @next: caller supplied function to return next address in list
 *
 *  Updates the Multicast Table Array.
 **/
s32 ixgbe_update_mc_addr_list_vf(struct ixgbe_hw *hw, u8 *mc_addr_list,
				  u32 mc_addr_count, ixgbe_mc_addr_itr next)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[IXGBE_VFMAILBOX_SIZE];
	u16 *vector_list = (u16 *)&msgbuf[1];
	u32 vector;
	u32 cnt, i;
	u32 vmdq;

	DEBUGFUNC("ixgbe_update_mc_addr_list_vf");

	/* Each entry in the list uses 1 16 bit word.  We have 30
	 * 16 bit words available in our HW msg buffer (minus 1 for the
	 * msg type).  That's 30 hash values if we pack 'em right.  If
	 * there are more than 30 MC addresses to add then punt the
	 * extras for now and then add code to handle more than 30 later.
	 * It would be unusual for a server to request that many multi-cast
	 * addresses except for in large enterprise network environments.
	 */

	DEBUGOUT1("MC Addr Count = %d\n", mc_addr_count);

	cnt = (mc_addr_count > 30) ? 30 : mc_addr_count;
	msgbuf[0] = IXGBE_VF_SET_MULTICAST;
	msgbuf[0] |= cnt << IXGBE_VT_MSGINFO_SHIFT;

	for (i = 0; i < cnt; i++) {
		vector = ixgbe_mta_vector(hw, next(hw, &mc_addr_list, &vmdq));
		DEBUGOUT1("Hash value = 0x%03X\n", vector);
		vector_list[i] = (u16)vector;
	}

	return mbx->ops.write_posted(hw, msgbuf, IXGBE_VFMAILBOX_SIZE, 0);
}

/**
 *  ixgbe_set_vfta_vf - Set/Unset vlan filter table address
 *  @hw: pointer to the HW structure
 *  @vlan: 12 bit VLAN ID
 *  @vind: unused by VF drivers
 *  @vlan_on: if TRUE then set bit, else clear bit
 **/
s32 ixgbe_set_vfta_vf(struct ixgbe_hw *hw, u32 vlan, u32 vind, bool vlan_on)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[2];
	UNREFERENCED_PARAMETER(vind);

	msgbuf[0] = IXGBE_VF_SET_VLAN;
	msgbuf[1] = vlan;
	/* Setting the 8 bit field MSG INFO to TRUE indicates "add" */
	msgbuf[0] |= vlan_on << IXGBE_VT_MSGINFO_SHIFT;

	return(mbx->ops.write_posted(hw, msgbuf, 2, 0));
}

/**
 *  ixgbe_get_num_of_tx_queues_vf - Get number of TX queues
 *  @hw: pointer to hardware structure
 *
 *  Returns the number of transmit queues for the given adapter.
 **/
u32 ixgbe_get_num_of_tx_queues_vf(struct ixgbe_hw *hw)
{
	UNREFERENCED_PARAMETER(hw);
	return IXGBE_VF_MAX_TX_QUEUES;
}

/**
 *  ixgbe_get_num_of_rx_queues_vf - Get number of RX queues
 *  @hw: pointer to hardware structure
 *
 *  Returns the number of receive queues for the given adapter.
 **/
u32 ixgbe_get_num_of_rx_queues_vf(struct ixgbe_hw *hw)
{
	UNREFERENCED_PARAMETER(hw);
	return IXGBE_VF_MAX_RX_QUEUES;
}

/**
 *  ixgbe_get_mac_addr_vf - Read device MAC address
 *  @hw: pointer to the HW structure
 **/
s32 ixgbe_get_mac_addr_vf(struct ixgbe_hw *hw, u8 *mac_addr)
{
	int i;

	for (i = 0; i < IXGBE_ETH_LENGTH_OF_ADDRESS; i++)
		mac_addr[i] = hw->mac.perm_addr[i];

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_setup_mac_link_vf - Setup MAC link settings
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *  @autoneg_wait_to_complete: TRUE when waiting for completion is needed
 *
 *  Set the link speed in the AUTOC register and restarts link.
 **/
s32 ixgbe_setup_mac_link_vf(struct ixgbe_hw *hw,
                                  ixgbe_link_speed speed, bool autoneg,
                                  bool autoneg_wait_to_complete)
{
	UNREFERENCED_PARAMETER(hw);
	UNREFERENCED_PARAMETER(speed);
	UNREFERENCED_PARAMETER(autoneg);
	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);
	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_check_mac_link_vf - Get link/speed status
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @link_up: TRUE is link is up, FALSE otherwise
 *  @autoneg_wait_to_complete: TRUE when waiting for completion is needed
 *
 *  Reads the links register to determine if link is up and the current speed
 **/
s32 ixgbe_check_mac_link_vf(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
                            bool *link_up, bool autoneg_wait_to_complete)
{
	u32 links_reg;
	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);

	if (!(hw->mbx.ops.check_for_rst(hw, 0))) {
		*link_up = FALSE;
		*speed = 0;
		return -1;
	}

	links_reg = IXGBE_VFREAD_REG(hw, IXGBE_VFLINKS);

	if (links_reg & IXGBE_LINKS_UP)
		*link_up = TRUE;
	else
		*link_up = FALSE;

	if ((links_reg & IXGBE_LINKS_SPEED_10G_82599) ==
	    IXGBE_LINKS_SPEED_10G_82599)
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
	else
		*speed = IXGBE_LINK_SPEED_1GB_FULL;

	return IXGBE_SUCCESS;
}

