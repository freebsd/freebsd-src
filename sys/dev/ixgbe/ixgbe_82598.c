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

#include "ixgbe_type.h"
#include "ixgbe_api.h"
#include "ixgbe_common.h"
#include "ixgbe_phy.h"

#define IXGBE_82598_MAX_TX_QUEUES 32
#define IXGBE_82598_MAX_RX_QUEUES 64
#define IXGBE_82598_RAR_ENTRIES   16

s32 ixgbe_init_shared_code_82598(struct ixgbe_hw *hw);
s32 ixgbe_assign_func_pointers_82598(struct ixgbe_hw *hw);
s32 ixgbe_get_link_settings_82598(struct ixgbe_hw *hw,
				  ixgbe_link_speed *speed,
				  bool *autoneg);
s32 ixgbe_get_copper_link_settings_82598(struct ixgbe_hw *hw,
					 ixgbe_link_speed *speed,
					 bool *autoneg);
enum ixgbe_media_type ixgbe_get_media_type_82598(struct ixgbe_hw *hw);
u32 ixgbe_get_num_of_tx_queues_82598(struct ixgbe_hw *hw);
u32 ixgbe_get_num_of_rx_queues_82598(struct ixgbe_hw *hw);
s32 ixgbe_setup_mac_link_82598(struct ixgbe_hw *hw);
s32 ixgbe_check_mac_link_82598(struct ixgbe_hw *hw,
			       ixgbe_link_speed *speed,
			       bool *link_up);
s32 ixgbe_setup_mac_link_speed_82598(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed,
				     bool autoneg,
				     bool autoneg_wait_to_complete);
s32 ixgbe_setup_copper_link_82598(struct ixgbe_hw *hw);
s32 ixgbe_check_copper_link_82598(struct ixgbe_hw *hw,
				  ixgbe_link_speed *speed,
				  bool *link_up);
s32 ixgbe_setup_copper_link_speed_82598(struct ixgbe_hw *hw,
					ixgbe_link_speed speed,
					bool autoneg,
					bool autoneg_wait_to_complete);
#ifndef NO_82598_A0_SUPPORT
s32 ixgbe_reset_hw_rev_0_82598(struct ixgbe_hw *hw);
#endif
s32 ixgbe_reset_hw_82598(struct ixgbe_hw *hw);
u32 ixgbe_get_num_rx_addrs_82598(struct ixgbe_hw *hw);


/**
 *  ixgbe_init_shared_code_82598 - Inits func ptrs and MAC type
 *  @hw: pointer to hardware structure
 *
 *  Initialize the shared code for 82598. This will assign function pointers
 *  and assign the MAC type.  Does not touch the hardware.
 **/
s32 ixgbe_init_shared_code_82598(struct ixgbe_hw *hw)
{
	/* Set MAC type */
	hw->mac.type = ixgbe_mac_82598EB;

	/* Assign function pointers */
	ixgbe_assign_func_pointers_82598(hw);

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_assign_func_pointers_82598 - Assigns 82598-specific funtion pointers
 *  @hw: pointer to hardware structure
 *
 *  Note - Generic function pointers have already been assigned, so the
 *  function pointers set here are only for 82598-specific functions.
 **/
s32 ixgbe_assign_func_pointers_82598(struct ixgbe_hw *hw)
{

	hw->func.ixgbe_func_get_media_type =
			       &ixgbe_get_media_type_82598;
	hw->func.ixgbe_func_get_num_of_tx_queues =
			       &ixgbe_get_num_of_tx_queues_82598;
	hw->func.ixgbe_func_get_num_of_rx_queues =
			       &ixgbe_get_num_of_rx_queues_82598;
#ifndef NO_82598_A0_SUPPORT
	if (hw->revision_id == 0) {
		hw->func.ixgbe_func_reset_hw =
			       &ixgbe_reset_hw_rev_0_82598;
	} else {
		hw->func.ixgbe_func_reset_hw = &ixgbe_reset_hw_82598;
	}
#else
	hw->func.ixgbe_func_reset_hw = &ixgbe_reset_hw_82598;
#endif

	hw->func.ixgbe_func_get_num_rx_addrs =
			       &ixgbe_get_num_rx_addrs_82598;

	/* Link */
	if (ixgbe_get_media_type(hw) == ixgbe_media_type_copper) {
		hw->func.ixgbe_func_setup_link =
			       &ixgbe_setup_copper_link_82598;
		hw->func.ixgbe_func_check_link =
			       &ixgbe_check_copper_link_82598;
		hw->func.ixgbe_func_setup_link_speed =
			       &ixgbe_setup_copper_link_speed_82598;
		hw->func.ixgbe_func_get_link_settings =
			       &ixgbe_get_copper_link_settings_82598;
	} else {
		hw->func.ixgbe_func_setup_link =
			       &ixgbe_setup_mac_link_82598;
		hw->func.ixgbe_func_check_link =
			       &ixgbe_check_mac_link_82598;
		hw->func.ixgbe_func_setup_link_speed =
			       &ixgbe_setup_mac_link_speed_82598;
		hw->func.ixgbe_func_get_link_settings =
			       &ixgbe_get_link_settings_82598;
	}

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_get_link_settings_82598 - Determines default link settings
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @autoneg: boolean auto-negotiation value
 *
 *  Determines the default link settings by reading the AUTOC register.
 **/
s32 ixgbe_get_link_settings_82598(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
				  bool *autoneg)
{
	s32 status = IXGBE_SUCCESS;
	s32 autoc_reg;

	autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	if (hw->mac.link_settings_loaded) {
		autoc_reg &= ~IXGBE_AUTOC_LMS_ATTACH_TYPE;
		autoc_reg &= ~IXGBE_AUTOC_LMS_MASK;
		autoc_reg |= hw->mac.link_attach_type;
		autoc_reg |= hw->mac.link_mode_select;
	}

	switch (autoc_reg & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = FALSE;
		break;

	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*autoneg = FALSE;
		break;

	case IXGBE_AUTOC_LMS_1G_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = TRUE;
		break;

	case IXGBE_AUTOC_LMS_KX4_AN:
	case IXGBE_AUTOC_LMS_KX4_AN_1G_AN:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		if (autoc_reg & IXGBE_AUTOC_KX4_SUPP) {
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		}
		if (autoc_reg & IXGBE_AUTOC_KX_SUPP) {
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		}
		*autoneg = TRUE;
		break;

	default:
		status = IXGBE_ERR_LINK_SETUP;
		break;
	}

	return status;
}

/**
 *  ixgbe_get_copper_link_settings_82598 - Determines default link settings
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @autoneg: boolean auto-negotiation value
 *
 *  Determines the default link settings by reading the AUTOC register.
 **/
s32 ixgbe_get_copper_link_settings_82598(struct ixgbe_hw *hw,
					 ixgbe_link_speed *speed,
					 bool *autoneg)
{
	s32 status = IXGBE_ERR_LINK_SETUP;
	u16 speed_ability;

	*speed = 0;
	*autoneg = TRUE;

	status = ixgbe_read_phy_reg(hw, IXGBE_MDIO_PHY_SPEED_ABILITY,
				    IXGBE_MDIO_PMA_PMD_DEV_TYPE,
				    &speed_ability);

	if (status == IXGBE_SUCCESS) {
		if (speed_ability & IXGBE_MDIO_PHY_SPEED_10G)
		    *speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (speed_ability & IXGBE_MDIO_PHY_SPEED_1G)
		    *speed |= IXGBE_LINK_SPEED_1GB_FULL;
	}

	return status;
}

/**
 *  ixgbe_get_media_type_82598 - Determines media type
 *  @hw: pointer to hardware structure
 *
 *  Returns the media type (fiber, copper, backplane)
 **/
enum ixgbe_media_type ixgbe_get_media_type_82598(struct ixgbe_hw *hw)
{
	enum ixgbe_media_type media_type;

	/* Media type for I82598 is based on device ID */
	switch (hw->device_id) {
	case IXGBE_DEV_ID_82598:
		/* Default device ID is mezzanine card KX/KX4 */
		media_type = ixgbe_media_type_backplane;
		break;
	case IXGBE_DEV_ID_82598_FPGA:
	case IXGBE_DEV_ID_82598AF_DUAL_PORT:
	case IXGBE_DEV_ID_82598AF_SINGLE_PORT:
		media_type = ixgbe_media_type_fiber;
		break;
	case IXGBE_DEV_ID_82598AT_DUAL_PORT:
		media_type = ixgbe_media_type_copper;
		break;
	default:
		media_type = ixgbe_media_type_unknown;
		break;
	}

	return media_type;
}

/**
 *  ixgbe_get_num_of_tx_queues_82598 - Get number of TX queues
 *  @hw: pointer to hardware structure
 *
 *  Returns the number of transmit queues for the given adapter.
 **/
u32 ixgbe_get_num_of_tx_queues_82598(struct ixgbe_hw *hw)
{
	if (hw->device_id == IXGBE_DEV_ID_82598_FPGA)
		return 8;

	return IXGBE_82598_MAX_TX_QUEUES;
}

/**
 *  ixgbe_get_num_of_rx_queues_82598 - Get number of RX queues
 *  @hw: pointer to hardware structure
 *
 *  Returns the number of receive queues for the given adapter.
 **/
u32 ixgbe_get_num_of_rx_queues_82598(struct ixgbe_hw *hw)
{
	if (hw->device_id == IXGBE_DEV_ID_82598_FPGA)
		return 8;

	return IXGBE_82598_MAX_RX_QUEUES;
}

/**
 *  ixgbe_setup_mac_link_82598 - Configures MAC link settings
 *  @hw: pointer to hardware structure
 *
 *  Configures link settings based on values in the ixgbe_hw struct.
 *  Restarts the link.  Performs autonegotiation if needed.
 **/
s32 ixgbe_setup_mac_link_82598(struct ixgbe_hw *hw)
{
	u32 autoc_reg;
	u32 links_reg;
	u32 i;
	s32 status = IXGBE_SUCCESS;

	autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	if (hw->mac.link_settings_loaded) {
		autoc_reg &= ~IXGBE_AUTOC_LMS_ATTACH_TYPE;
		autoc_reg &= ~IXGBE_AUTOC_LMS_MASK;
		autoc_reg |= hw->mac.link_attach_type;
		autoc_reg |= hw->mac.link_mode_select;

		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);
		msec_delay(50);
	}

	/* Restart link */
	autoc_reg |= IXGBE_AUTOC_AN_RESTART;
	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);

	/* Only poll for autoneg to complete if specified to do so */
	if (hw->phy.autoneg_wait_to_complete) {
		if (hw->mac.link_mode_select == IXGBE_AUTOC_LMS_KX4_AN ||
		    hw->mac.link_mode_select == IXGBE_AUTOC_LMS_KX4_AN_1G_AN) {
			links_reg = 0; /* Just in case Autoneg time = 0 */
			for (i = 0; i < IXGBE_AUTO_NEG_TIME; i++) {
				links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
				if (links_reg & IXGBE_LINKS_KX_AN_COMP)
					break;
				msec_delay(100);
			}
			if (!(links_reg & IXGBE_LINKS_KX_AN_COMP)) {
				status = IXGBE_ERR_AUTONEG_NOT_COMPLETE;
				DEBUGOUT("Autonegotiation did not complete.\n");
			}
		}
	}

	/* Add delay to filter out noises during initial link setup */
	msec_delay(50);

	return status;
}

/**
 *  ixgbe_check_mac_link_82598 - Get link/speed status
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @link_up: TRUE is link is up, FALSE otherwise
 *
 *  Reads the links register to determine if link is up and the current speed
 **/
s32 ixgbe_check_mac_link_82598(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
			       bool *link_up)
{
	u32 links_reg;

	links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);

	if (links_reg & IXGBE_LINKS_UP)
		*link_up = TRUE;
	else
		*link_up = FALSE;

	if (links_reg & IXGBE_LINKS_SPEED)
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
	else
		*speed = IXGBE_LINK_SPEED_1GB_FULL;

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_setup_mac_link_speed_82598 - Set MAC link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if auto-negotiation enabled
 *  @autoneg_wait_to_complete: TRUE if waiting is needed to complete
 *
 *  Set the link speed in the AUTOC register and restarts link.
 **/
s32 ixgbe_setup_mac_link_speed_82598(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed, bool autoneg,
				     bool autoneg_wait_to_complete)
{
	s32 status = IXGBE_SUCCESS;

	/* If speed is 10G, then check for CX4 or XAUI. */
	if ((speed == IXGBE_LINK_SPEED_10GB_FULL) &&
	    (!(hw->mac.link_attach_type & IXGBE_AUTOC_10G_KX4)))
		hw->mac.link_mode_select = IXGBE_AUTOC_LMS_10G_LINK_NO_AN;
	else if ((speed == IXGBE_LINK_SPEED_1GB_FULL) && (!autoneg))
		hw->mac.link_mode_select = IXGBE_AUTOC_LMS_1G_LINK_NO_AN;
	else if (autoneg) {
		/* BX mode - Autonegotiate 1G */
		if (!(hw->mac.link_attach_type & IXGBE_AUTOC_1G_PMA_PMD))
			hw->mac.link_mode_select = IXGBE_AUTOC_LMS_1G_AN;
		else /* KX/KX4 mode */
			hw->mac.link_mode_select = IXGBE_AUTOC_LMS_KX4_AN_1G_AN;
	} else {
		status = IXGBE_ERR_LINK_SETUP;
	}

	if (status == IXGBE_SUCCESS) {
		hw->phy.autoneg_wait_to_complete = autoneg_wait_to_complete;

		hw->mac.link_settings_loaded = TRUE;
		/*
		 * Setup and restart the link based on the new values in
		 * ixgbe_hw This will write the AUTOC register based on the new
		 * stored values
		 */
		ixgbe_setup_mac_link_82598(hw);
	}

	return status;
}


/**
 *  ixgbe_setup_copper_link_82598 - Setup copper link settings
 *  @hw: pointer to hardware structure
 *
 *  Configures link settings based on values in the ixgbe_hw struct.
 *  Restarts the link.  Performs autonegotiation if needed.  Restart
 *  phy and wait for autonegotiate to finish.  Then synchronize the
 *  MAC and PHY.
 **/
s32 ixgbe_setup_copper_link_82598(struct ixgbe_hw *hw)
{
	s32 status;
	ixgbe_link_speed speed = 0;
	bool link_up = FALSE;

	/* Set up MAC */
	ixgbe_setup_mac_link_82598(hw);

	/* Restart autonegotiation on PHY */
	status = ixgbe_setup_phy_link(hw);

	/* Synchronize MAC to PHY speed */
	if (status == IXGBE_SUCCESS)
		status = ixgbe_check_link(hw, &speed, &link_up);

	return status;
}

/**
 *  ixgbe_check_copper_link_82598 - Syncs MAC & PHY link settings
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @link_up: TRUE if link is up, FALSE otherwise
 *
 *  Reads the mac link, phy link, and synchronizes the MAC to PHY.
 **/
s32 ixgbe_check_copper_link_82598(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
				  bool *link_up)
{
	s32 status;
	ixgbe_link_speed phy_speed = 0;
	bool phy_link = FALSE;

	/* This is the speed and link the MAC is set at */
	ixgbe_check_mac_link_82598(hw, speed, link_up);

	/*
	 * Check current speed and link status of the PHY register.
	 * This is a vendor specific register and may have to
	 * be changed for other copper PHYs.
	 */
	status = ixgbe_check_phy_link(hw, &phy_speed, &phy_link);

	if ((status == IXGBE_SUCCESS) && (phy_link)) {
		/*
		 * Check current link status of the MACs link's register
		 * matches that of the speed in the PHY register
		 */
		if (*speed != phy_speed) {
			/*
			 * The copper PHY requires 82598 attach type to be XAUI
			 * for 10G and BX for 1G
			 */
			hw->mac.link_attach_type =
				(IXGBE_AUTOC_10G_XAUI | IXGBE_AUTOC_1G_BX);

			/* Synchronize the MAC speed to the PHY speed */
			status = ixgbe_setup_mac_link_speed_82598(hw, phy_speed,
								  FALSE, FALSE);
			if (status == IXGBE_SUCCESS)
				ixgbe_check_mac_link_82598(hw, speed, link_up);
			else
				status = IXGBE_ERR_LINK_SETUP;
		}
	} else {
		*link_up = phy_link;
	}

	return status;
}

/**
 *  ixgbe_setup_copper_link_speed_82598 - Set the PHY autoneg advertised field
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *  @autoneg_wait_to_complete: TRUE if waiting is needed to complete
 *
 *  Sets the link speed in the AUTOC register in the MAC and restarts link.
 **/
s32 ixgbe_setup_copper_link_speed_82598(struct ixgbe_hw *hw,
					ixgbe_link_speed speed,
					bool autoneg,
					bool autoneg_wait_to_complete)
{
	s32 status;
	bool link_up = 0;

	/* Setup the PHY according to input speed */
	status = ixgbe_setup_phy_link_speed(hw, speed, autoneg,
					    autoneg_wait_to_complete);

	/* Synchronize MAC to PHY speed */
	if (status == IXGBE_SUCCESS)
		status = ixgbe_check_link(hw, &speed, &link_up);

	return status;
}

#ifndef NO_82598_A0_SUPPORT
/**
 *  ixgbe_reset_hw_rev_0_82598 - Performs hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by reseting the transmit and receive units, masks and
 *  clears all interrupts, performing a PHY reset, and performing a link (MAC)
 *  reset.
 **/
s32 ixgbe_reset_hw_rev_0_82598(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_SUCCESS;
	u32 ctrl;
	u32 gheccr;
	u32 autoc;
	u32 i;
	u32 resets;

	/* Call adapter stop to disable tx/rx and clear interrupts */
	ixgbe_stop_adapter(hw);

	/* Reset PHY */
	ixgbe_reset_phy(hw);

	for (resets = 0; resets < 10; resets++) {
		/*
		 * Prevent the PCI-E bus from from hanging by disabling PCI-E
		 * master access and verify no pending requests before reset
		 */
		if (ixgbe_disable_pcie_master(hw) != IXGBE_SUCCESS) {
			status = IXGBE_ERR_MASTER_REQUESTS_PENDING;
			DEBUGOUT("PCI-E Master disable polling has failed.\n");
		}

		/*
		 * Issue global reset to the MAC.  This needs to be a SW reset.
		 * If link reset is used, it might reset the MAC when mng is
		 * using it.
		 */
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		IXGBE_WRITE_REG(hw, IXGBE_CTRL, (ctrl | IXGBE_CTRL_RST));
		IXGBE_WRITE_FLUSH(hw);

		/*
		 * Poll for reset bit to self-clear indicating reset is
		 * complete
		 */
		for (i = 0; i < 10; i++) {
			usec_delay(1);
			ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
			if (!(ctrl & IXGBE_CTRL_RST))
				break;
		}
		if (ctrl & IXGBE_CTRL_RST) {
			status = IXGBE_ERR_RESET_FAILED;
			DEBUGOUT("Reset polling failed to complete.\n");
		}
	}

	msec_delay(50);

	gheccr = IXGBE_READ_REG(hw, IXGBE_GHECCR);
	gheccr &= ~((1 << 21) | (1 << 18) | (1 << 9) | (1 << 6));
	IXGBE_WRITE_REG(hw, IXGBE_GHECCR, gheccr);

	/*
	 * AUTOC register which stores link settings gets cleared
	 * and reloaded from EEPROM after reset. We need to restore
	 * our stored value from init in case SW changed the attach
	 * type or speed.  If this is the first time and link settings
	 * have not been stored, store default settings from AUTOC.
	 */
	autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	if (hw->mac.link_settings_loaded) {
		autoc &= ~(IXGBE_AUTOC_LMS_ATTACH_TYPE);
		autoc &= ~(IXGBE_AUTOC_LMS_MASK);
		autoc |= hw->mac.link_attach_type;
		autoc |= hw->mac.link_mode_select;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc);
	} else {
		hw->mac.link_attach_type =
					 (autoc & IXGBE_AUTOC_LMS_ATTACH_TYPE);
		hw->mac.link_mode_select = (autoc & IXGBE_AUTOC_LMS_MASK);
		hw->mac.link_settings_loaded = TRUE;
	}

	/* Store the permanent mac address */
	ixgbe_get_mac_addr(hw, hw->mac.perm_addr);

	return status;
}

#endif /* NO_A0_SUPPORT */
/**
 *  ixgbe_reset_hw_82598 - Performs hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by reseting the transmit and receive units, masks and
 *  clears all interrupts, performing a PHY reset, and performing a link (MAC)
 *  reset.
 **/
s32 ixgbe_reset_hw_82598(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_SUCCESS;
	u32 ctrl;
	u32 gheccr;
	u32 i;
	u32 autoc;

	/* Call adapter stop to disable tx/rx and clear interrupts */
	ixgbe_stop_adapter(hw);

	/* Reset PHY */
	ixgbe_reset_phy(hw);

	/*
	 * Prevent the PCI-E bus from from hanging by disabling PCI-E master
	 * access and verify no pending requests before reset
	 */
	if (ixgbe_disable_pcie_master(hw) != IXGBE_SUCCESS) {
		status = IXGBE_ERR_MASTER_REQUESTS_PENDING;
		DEBUGOUT("PCI-E Master disable polling has failed.\n");
	}

	/*
	 * Issue global reset to the MAC.  This needs to be a SW reset.
	 * If link reset is used, it might reset the MAC when mng is using it
	 */
	ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, (ctrl | IXGBE_CTRL_RST));
	IXGBE_WRITE_FLUSH(hw);

	/* Poll for reset bit to self-clear indicating reset is complete */
	for (i = 0; i < 10; i++) {
		usec_delay(1);
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		if (!(ctrl & IXGBE_CTRL_RST))
			break;
	}
	if (ctrl & IXGBE_CTRL_RST) {
		status = IXGBE_ERR_RESET_FAILED;
		DEBUGOUT("Reset polling failed to complete.\n");
	}

	msec_delay(50);

	gheccr = IXGBE_READ_REG(hw, IXGBE_GHECCR);
	gheccr &= ~((1 << 21) | (1 << 18) | (1 << 9) | (1 << 6));
	IXGBE_WRITE_REG(hw, IXGBE_GHECCR, gheccr);

	/*
	 * AUTOC register which stores link settings gets cleared
	 * and reloaded from EEPROM after reset. We need to restore
	 * our stored value from init in case SW changed the attach
	 * type or speed.  If this is the first time and link settings
	 * have not been stored, store default settings from AUTOC.
	 */
	autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	if (hw->mac.link_settings_loaded) {
		autoc &= ~(IXGBE_AUTOC_LMS_ATTACH_TYPE);
		autoc &= ~(IXGBE_AUTOC_LMS_MASK);
		autoc |= hw->mac.link_attach_type;
		autoc |= hw->mac.link_mode_select;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc);
	} else {
		hw->mac.link_attach_type =
					 (autoc & IXGBE_AUTOC_LMS_ATTACH_TYPE);
		hw->mac.link_mode_select = (autoc & IXGBE_AUTOC_LMS_MASK);
		hw->mac.link_settings_loaded = TRUE;
	}

	/* Store the permanent mac address */
	ixgbe_get_mac_addr(hw, hw->mac.perm_addr);

	return status;
}

/**
 *  ixgbe_get_num_rx_addrs_82598 - Get RX address registers
 *  @hw: pointer to hardware structure
 *
 *  Returns the of RAR entries for the given adapter.
 **/
u32 ixgbe_get_num_rx_addrs_82598(struct ixgbe_hw *hw)
{
	UNREFERENCED_PARAMETER(hw);

	return IXGBE_82598_RAR_ENTRIES;
}

