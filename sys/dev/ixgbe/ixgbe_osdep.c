/******************************************************************************

  Copyright (c) 2001-2017, Intel Corporation
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

#include "ixgbe.h"

inline u16
ixgbe_read_pci_cfg(struct ixgbe_hw *hw, u32 reg)
{
	return pci_read_config(((struct adapter *)hw->back)->dev, reg, 2);
}

inline void
ixgbe_write_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 value)
{
	pci_write_config(((struct adapter *)hw->back)->dev, reg, value, 2);
}

inline u32
ixgbe_read_reg(struct ixgbe_hw *hw, u32 reg)
{
	struct adapter *adapter = (struct adapter *)hw->back;
	u32 retval;
	u8 i;

	retval = bus_space_read_4(adapter->osdep.mem_bus_space_tag,
	    adapter->osdep.mem_bus_space_handle, reg);

	/* Normal... */
	if ((retval != 0xDEADBEEF) ||
	    !(hw->phy.nw_mng_if_sel & IXGBE_NW_MNG_IF_SEL_SGMII_ENABLE))
		return retval;

	/* Unusual... */

	/*
	 * 10/100 Mb mode has a quirk where it's possible the previous
	 * write to the Phy hasn't completed.  So we keep trying.
	 */
	for (i = 100; retval; i--) {
		if (!i) {
			device_printf(adapter->dev, "Register (0x%08X) writes did not complete: 0x%08X\n",
			    reg, retval);
			break;
		}
		retval = bus_space_read_4(adapter->osdep.mem_bus_space_tag,
		    adapter->osdep.mem_bus_space_handle, IXGBE_MAC_SGMII_BUSY);
	}

	for (i = 10; retval == 0xDEADBEEF; i--) {
		if (!i) {
			device_printf(adapter->dev,
			    "Failed to read register 0x%08X.\n", reg);
			break;
		}
		retval = bus_space_read_4(adapter->osdep.mem_bus_space_tag,
		    adapter->osdep.mem_bus_space_handle, reg);
	}

	return retval;
}

inline void
ixgbe_write_reg(struct ixgbe_hw *hw, u32 reg, u32 val)
{
	bus_space_write_4(((struct adapter *)hw->back)->osdep.mem_bus_space_tag,
	    ((struct adapter *)hw->back)->osdep.mem_bus_space_handle,
	    reg, val);
}

inline u32
ixgbe_read_reg_array(struct ixgbe_hw *hw, u32 reg, u32 offset)
{
	return bus_space_read_4(((struct adapter *)hw->back)->osdep.mem_bus_space_tag,
	    ((struct adapter *)hw->back)->osdep.mem_bus_space_handle,
	    reg + (offset << 2));
}

inline void
ixgbe_write_reg_array(struct ixgbe_hw *hw, u32 reg, u32 offset, u32 val)
{
	bus_space_write_4(((struct adapter *)hw->back)->osdep.mem_bus_space_tag,
	    ((struct adapter *)hw->back)->osdep.mem_bus_space_handle,
	    reg + (offset << 2), val);
}
