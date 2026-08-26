/******************************************************************************
  SPDX-License-Identifier: BSD-3-Clause

  Copyright (c) 2001-2024, Intel Corporation
  All rights reserved.
  Copyright (c) 2026 Adrian Chadd <adrian@FreeBSD.org>

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

#include "ixgbe_api.h"
#include "ixgbe_common.h"
#include "ixgbe_phy.h"

#include "if_ix_mdio_hw.h"

/*
 * These routines are separate from the rest of ixgbe for now to make merging
 * easier.
 */

static s32
ixgbe_read_mdio_unlocked_c22(struct ixgbe_hw *hw, u16 phy, u16 reg, u16 *phy_data)
{
	u32 i, data, command;

	/* Setup and write the read command */
	command = (reg << IXGBE_MSCA_DEV_TYPE_SHIFT) |
		  (phy << IXGBE_MSCA_PHY_ADDR_SHIFT) |
		  IXGBE_MSCA_OLD_PROTOCOL | IXGBE_MSCA_READ_AUTOINC |
		  IXGBE_MSCA_MDI_COMMAND;

	IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

	/* Check every 10 usec to see if the access completed.
	 * The MDI Command bit will clear when the operation is
	 * complete
	 */
	for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
		usec_delay(10);

		command = IXGBE_READ_REG(hw, IXGBE_MSCA);
		if (!(command & IXGBE_MSCA_MDI_COMMAND))
			break;
	}

	if (command & IXGBE_MSCA_MDI_COMMAND)
		return IXGBE_ERR_PHY;

	/* Read operation is complete.  Get the data from MSRWD */
	data = IXGBE_READ_REG(hw, IXGBE_MSRWD);
	data >>= IXGBE_MSRWD_READ_DATA_SHIFT;
	*phy_data = (u16)data;

	return IXGBE_SUCCESS;
}

static s32
ixgbe_write_mdio_unlocked_c22(struct ixgbe_hw *hw, u16 phy, u16 reg, u16 phy_data)
{
	u32 i, command;

	/* Put the data in the MDI single read and write data register*/
	IXGBE_WRITE_REG(hw, IXGBE_MSRWD, (u32)phy_data);

	/* Setup and write the write command */
	command = (reg << IXGBE_MSCA_DEV_TYPE_SHIFT) |
		  (phy << IXGBE_MSCA_PHY_ADDR_SHIFT) |
		  IXGBE_MSCA_OLD_PROTOCOL | IXGBE_MSCA_WRITE |
		  IXGBE_MSCA_MDI_COMMAND;

	IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

	/* Check every 10 usec to see if the access completed.
	 * The MDI Command bit will clear when the operation is
	 * complete
	 */
	for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
		usec_delay(10);

		command = IXGBE_READ_REG(hw, IXGBE_MSCA);
		if (!(command & IXGBE_MSCA_MDI_COMMAND))
			break;
	}

	if (command & IXGBE_MSCA_MDI_COMMAND)
		return IXGBE_ERR_PHY;

	return IXGBE_SUCCESS;
}

/*
 * Return true if the MAC is an X55x backplane.
 *
 * These have a single MDIO PHY semaphore (PHY0) and also require the
 * token semaphore.
 */
static bool
ixgbe_check_mdio_is_x550em(struct ixgbe_hw *hw)
{

	switch (hw->device_id) {
	case IXGBE_DEV_ID_X550EM_A_KR:
	case IXGBE_DEV_ID_X550EM_A_KR_L:
	case IXGBE_DEV_ID_X550EM_A_SFP_N:
	case IXGBE_DEV_ID_X550EM_A_SGMII:
	case IXGBE_DEV_ID_X550EM_A_SGMII_L:
	case IXGBE_DEV_ID_X550EM_A_10G_T:
	case IXGBE_DEV_ID_X550EM_A_SFP:
	case IXGBE_DEV_ID_X550EM_A_1G_T:
	case IXGBE_DEV_ID_X550EM_A_1G_T_L:
		return true;
	default:
		return false;
	}
}

s32
ixgbe_read_mdio_c22(struct ixgbe_hw *hw, u16 phy, u16 reg, u16 *phy_data)
{
	u32 gssr = hw->phy.phy_semaphore_mask;
	s32 ret;

	if (ixgbe_check_mdio_is_x550em(hw))
		gssr |= IXGBE_GSSR_PHY0_SM | IXGBE_GSSR_TOKEN_SM;

	if (hw->mac.ops.acquire_swfw_sync(hw, gssr)) {
		*phy_data = -1;
		return IXGBE_ERR_TIMEOUT;
	}

	ret = ixgbe_read_mdio_unlocked_c22(hw, phy, reg, phy_data);
	if (ret != IXGBE_SUCCESS)
		*phy_data = -1;

	hw->mac.ops.release_swfw_sync(hw, gssr);
	return ret;
}

s32
ixgbe_write_mdio_c22(struct ixgbe_hw *hw, u16 phy, u16 reg, u16 data)
{
	u32 gssr = hw->phy.phy_semaphore_mask;
	s32 ret;

	if (ixgbe_check_mdio_is_x550em(hw))
		gssr |= IXGBE_GSSR_PHY0_SM | IXGBE_GSSR_TOKEN_SM;

	if (hw->mac.ops.acquire_swfw_sync(hw, gssr))
		return IXGBE_ERR_TIMEOUT;

	ret = ixgbe_write_mdio_unlocked_c22(hw, phy, reg, data);

	hw->mac.ops.release_swfw_sync(hw, gssr);
	return ret;
}
