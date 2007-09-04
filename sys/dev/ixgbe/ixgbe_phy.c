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
#include "ixgbe_phy.h"

/**
 *  ixgbe_init_shared_code_phy - Initialize PHY shared code
 *  @hw: pointer to hardware structure
 **/
s32 ixgbe_init_shared_code_phy(struct ixgbe_hw *hw)
{
	/* Assign function pointers */
	ixgbe_assign_func_pointers_phy(hw);

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_assign_func_pointers_phy -  Assigns PHY-specific function pointers
 *  @hw: pointer to hardware structure
 *
 *  Note, generic function pointers have already been assigned, so the
 *  function pointers set here are only for PHY-specific functions.
 **/
s32 ixgbe_assign_func_pointers_phy(struct ixgbe_hw *hw)
{
	hw->func.ixgbe_func_reset_phy =
			    &ixgbe_reset_phy_generic;
	hw->func.ixgbe_func_read_phy_reg =
			    &ixgbe_read_phy_reg_generic;
	hw->func.ixgbe_func_write_phy_reg =
			    &ixgbe_write_phy_reg_generic;
	hw->func.ixgbe_func_identify_phy =
			    &ixgbe_identify_phy_generic;

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_identify_phy_generic - Get physical layer module
 *  @hw: pointer to hardware structure
 *
 *  Determines the physical layer module found on the current adapter.
 **/
s32 ixgbe_identify_phy_generic(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_ERR_PHY_ADDR_INVALID;
	u32 phy_addr;

	for (phy_addr = 0; phy_addr < IXGBE_MAX_PHY_ADDR; phy_addr++) {
		if (ixgbe_validate_phy_addr(hw, phy_addr)) {
			hw->phy.addr = phy_addr;
			ixgbe_get_phy_id(hw);
			hw->phy.type = ixgbe_get_phy_type_from_id(hw->phy.id);
			status = IXGBE_SUCCESS;
			break;
		}
	}
	return status;
}

/**
 *  ixgbe_validate_phy_addr - Determines phy address is valid
 *  @hw: pointer to hardware structure
 *
 **/
bool ixgbe_validate_phy_addr(struct ixgbe_hw *hw, u32 phy_addr)
{
	u16 phy_id = 0;
	bool valid = FALSE;

	hw->phy.addr = phy_addr;
	ixgbe_read_phy_reg_generic(hw,
				   IXGBE_MDIO_PHY_ID_HIGH,
				   IXGBE_MDIO_PMA_PMD_DEV_TYPE,
				   &phy_id);

	if (phy_id != 0xFFFF && phy_id != 0x0)
		valid = TRUE;

	return valid;
}

/**
 *  ixgbe_get_phy_id - Get the phy type
 *  @hw: pointer to hardware structure
 *
 **/
s32 ixgbe_get_phy_id(struct ixgbe_hw *hw)
{
	u32 status;
	u16 phy_id_high = 0;
	u16 phy_id_low = 0;

	status = ixgbe_read_phy_reg_generic(hw,
				   IXGBE_MDIO_PHY_ID_HIGH,
				   IXGBE_MDIO_PMA_PMD_DEV_TYPE,
				   &phy_id_high);

	if (status == IXGBE_SUCCESS) {
		hw->phy.id = (u32)(phy_id_high << 16);
		status = ixgbe_read_phy_reg_generic(hw,
					   IXGBE_MDIO_PHY_ID_LOW,
					   IXGBE_MDIO_PMA_PMD_DEV_TYPE,
					   &phy_id_low);
		hw->phy.id |= (u32)(phy_id_low & IXGBE_PHY_REVISION_MASK);
		hw->phy.revision = (u32)(phy_id_low & ~IXGBE_PHY_REVISION_MASK);
	}

	return status;
}

/**
 *  ixgbe_get_phy_type_from_id - Get the phy type
 *  @hw: pointer to hardware structure
 *
 **/
enum ixgbe_phy_type ixgbe_get_phy_type_from_id(u32 phy_id)
{
	enum ixgbe_phy_type phy_type;

	switch (phy_id) {
	case QT2022_PHY_ID:
		phy_type = ixgbe_phy_qt;
		break;
	default:
		phy_type = ixgbe_phy_unknown;
		break;
	}

	return phy_type;
}

/**
 *  ixgbe_reset_phy_generic - Performs a PHY reset
 *  @hw: pointer to hardware structure
 **/
s32 ixgbe_reset_phy_generic(struct ixgbe_hw *hw)
{
	/*
	 * Perform soft PHY reset to the PHY_XS.
	 * This will cause a soft reset to the PHY
	 */
	return ixgbe_write_phy_reg(hw, IXGBE_MDIO_PHY_XS_CONTROL,
				   IXGBE_MDIO_PHY_XS_DEV_TYPE,
				   IXGBE_MDIO_PHY_XS_RESET);
}

/**
 *  ixgbe_read_phy_reg_generic - Reads a value from a specified PHY register
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit address of PHY register to read
 *  @phy_data: Pointer to read data from PHY register
 **/
s32 ixgbe_read_phy_reg_generic(struct ixgbe_hw *hw, u32 reg_addr,
			       u32 device_type, u16 *phy_data)
{
	u32 command;
	u32 i;
	u32 data;
	s32 status = IXGBE_SUCCESS;
	u16 gssr;

	if (IXGBE_READ_REG(hw, IXGBE_STATUS) & IXGBE_STATUS_LAN_ID_1)
		gssr = IXGBE_GSSR_PHY1_SM;
	else
		gssr = IXGBE_GSSR_PHY0_SM;

	if (ixgbe_acquire_swfw_sync(hw, gssr) != IXGBE_SUCCESS)
		status = IXGBE_ERR_SWFW_SYNC;

	if (status == IXGBE_SUCCESS) {
		/* Setup and write the address cycle command */
		command = ((reg_addr << IXGBE_MSCA_NP_ADDR_SHIFT)  |
			   (device_type << IXGBE_MSCA_DEV_TYPE_SHIFT) |
			   (hw->phy.addr << IXGBE_MSCA_PHY_ADDR_SHIFT) |
			   (IXGBE_MSCA_ADDR_CYCLE | IXGBE_MSCA_MDI_COMMAND));

		IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

		/*
		 * Check every 10 usec to see if the address cycle completed.
		 * The MDI Command bit will clear when the operation is
		 * complete
		 */
		for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
			usec_delay(10);

			command = IXGBE_READ_REG(hw, IXGBE_MSCA);

			if ((command & IXGBE_MSCA_MDI_COMMAND) == 0) {
				break;
			}
		}

		if ((command & IXGBE_MSCA_MDI_COMMAND) != 0) {
			DEBUGOUT("PHY address command did not complete.\n");
			status = IXGBE_ERR_PHY;
		}

		if (status == IXGBE_SUCCESS) {
			/*
			 * Address cycle complete, setup and write the read
			 * command
			 */
			command = ((reg_addr << IXGBE_MSCA_NP_ADDR_SHIFT)  |
				   (device_type << IXGBE_MSCA_DEV_TYPE_SHIFT) |
				   (hw->phy.addr << IXGBE_MSCA_PHY_ADDR_SHIFT) |
				   (IXGBE_MSCA_READ | IXGBE_MSCA_MDI_COMMAND));

			IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

			/*
			 * Check every 10 usec to see if the address cycle
			 * completed. The MDI Command bit will clear when the
			 * operation is complete
			 */
			for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
				usec_delay(10);

				command = IXGBE_READ_REG(hw, IXGBE_MSCA);

				if ((command & IXGBE_MSCA_MDI_COMMAND) == 0)
					break;
			}

			if ((command & IXGBE_MSCA_MDI_COMMAND) != 0) {
				DEBUGOUT("PHY read command didn't complete\n");
				status = IXGBE_ERR_PHY;
			} else {
				/*
				 * Read operation is complete.  Get the data
				 * from MSRWD
				 */
				data = IXGBE_READ_REG(hw, IXGBE_MSRWD);
				data >>= IXGBE_MSRWD_READ_DATA_SHIFT;
				*phy_data = (u16)(data);
			}
		}

		ixgbe_release_swfw_sync(hw, gssr);
	}
	return status;
}

/**
 *  ixgbe_write_phy_reg_generic - Writes a value to specified PHY register
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @device_type: 5 bit device type
 *  @phy_data: Data to write to the PHY register
 **/
s32 ixgbe_write_phy_reg_generic(struct ixgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data)
{
	u32 command;
	u32 i;
	s32 status = IXGBE_SUCCESS;
	u16 gssr;

	if (IXGBE_READ_REG(hw, IXGBE_STATUS) & IXGBE_STATUS_LAN_ID_1)
		gssr = IXGBE_GSSR_PHY1_SM;
	else
		gssr = IXGBE_GSSR_PHY0_SM;

	if (ixgbe_acquire_swfw_sync(hw, gssr) != IXGBE_SUCCESS)
		status = IXGBE_ERR_SWFW_SYNC;

	if (status == IXGBE_SUCCESS) {
		/* Put the data in the MDI single read and write data register*/
		IXGBE_WRITE_REG(hw, IXGBE_MSRWD, (u32)phy_data);

		/* Setup and write the address cycle command */
		command = ((reg_addr << IXGBE_MSCA_NP_ADDR_SHIFT)  |
			   (device_type << IXGBE_MSCA_DEV_TYPE_SHIFT) |
			   (hw->phy.addr << IXGBE_MSCA_PHY_ADDR_SHIFT) |
			   (IXGBE_MSCA_ADDR_CYCLE | IXGBE_MSCA_MDI_COMMAND));

		IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

		/*
		 * Check every 10 usec to see if the address cycle completed.
		 * The MDI Command bit will clear when the operation is
		 * complete
		 */
		for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
			usec_delay(10);

			command = IXGBE_READ_REG(hw, IXGBE_MSCA);

			if ((command & IXGBE_MSCA_MDI_COMMAND) == 0) {
				DEBUGFUNC("PHY address cmd didn't complete\n");
				break;
			}
		}

		if ((command & IXGBE_MSCA_MDI_COMMAND) != 0)
			status = IXGBE_ERR_PHY;

		if (status == IXGBE_SUCCESS) {
			/*
			 * Address cycle complete, setup and write the write
			 * command
			 */
			command = ((reg_addr << IXGBE_MSCA_NP_ADDR_SHIFT)  |
				   (device_type << IXGBE_MSCA_DEV_TYPE_SHIFT) |
				   (hw->phy.addr << IXGBE_MSCA_PHY_ADDR_SHIFT) |
				   (IXGBE_MSCA_WRITE | IXGBE_MSCA_MDI_COMMAND));

			IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

			/*
			 * Check every 10 usec to see if the address cycle
			 * completed. The MDI Command bit will clear when the
			 * operation is complete
			 */
			for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
				usec_delay(10);

				command = IXGBE_READ_REG(hw, IXGBE_MSCA);

				if ((command & IXGBE_MSCA_MDI_COMMAND) == 0) {
					DEBUGFUNC("PHY write command did not "
						  "complete.\n");
					break;
				}
			}

			if ((command & IXGBE_MSCA_MDI_COMMAND) != 0)
				status = IXGBE_ERR_PHY;
		}

		ixgbe_release_swfw_sync(hw, gssr);
	}

	return status;
}

/**
 *  ixgbe_setup_phy_link - Restart PHY autoneg
 *  @hw: pointer to hardware structure
 *
 *  Restart autonegotiation and PHY and waits for completion.
 **/
s32 ixgbe_setup_phy_link(struct ixgbe_hw *hw)
{
	return ixgbe_call_func(hw, ixgbe_func_setup_phy_link, (hw),
			       IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_check_phy_link - Determine link and speed status
 *  @hw: pointer to hardware structure
 *
 *  Reads a PHY register to determine if link is up and the current speed for
 *  the PHY.
 **/
s32 ixgbe_check_phy_link(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
			 bool *link_up)
{
	return ixgbe_call_func(hw, ixgbe_func_check_phy_link, (hw, speed,
			       link_up), IXGBE_NOT_IMPLEMENTED);
}

/**
 *  ixgbe_setup_phy_link_speed - Set auto advertise
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *
 *  Sets the auto advertised capabilities
 **/
s32 ixgbe_setup_phy_link_speed(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			       bool autoneg,
			       bool autoneg_wait_to_complete)
{
	return ixgbe_call_func(hw, ixgbe_func_setup_phy_link_speed, (hw, speed,
			       autoneg, autoneg_wait_to_complete),
			       IXGBE_NOT_IMPLEMENTED);
}

