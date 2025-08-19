/******************************************************************************

  Copyright (c) 2001-2020, Intel Corporation
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

#include "ixgbe.h"

inline device_t
ixgbe_dev_from_hw(struct ixgbe_hw *hw)
{
	return ((struct ixgbe_softc *)hw->back)->dev;
}

inline u16
ixgbe_read_pci_cfg(struct ixgbe_hw *hw, u32 reg)
{
	return pci_read_config(((struct ixgbe_softc *)hw->back)->dev, reg, 2);
}

inline void
ixgbe_write_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 value)
{
	pci_write_config(((struct ixgbe_softc *)hw->back)->dev, reg, value, 2);
}

inline u32
ixgbe_read_reg(struct ixgbe_hw *hw, u32 reg)
{
	return bus_space_read_4(((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_tag,
	    ((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_handle, reg);
}

inline void
ixgbe_write_reg(struct ixgbe_hw *hw, u32 reg, u32 val)
{
	bus_space_write_4(((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_tag,
	    ((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_handle,
	    reg, val);
}

inline u32
ixgbe_read_reg_array(struct ixgbe_hw *hw, u32 reg, u32 offset)
{
	return bus_space_read_4(((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_tag,
	    ((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_handle,
	    reg + (offset << 2));
}

inline void
ixgbe_write_reg_array(struct ixgbe_hw *hw, u32 reg, u32 offset, u32 val)
{
	bus_space_write_4(((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_tag,
	    ((struct ixgbe_softc *)hw->back)->osdep.mem_bus_space_handle,
	    reg + (offset << 2), val);
}

uint64_t
ixgbe_link_speed_to_baudrate(ixgbe_link_speed speed)
{
	uint64_t baudrate;

	switch (speed) {
	case IXGBE_LINK_SPEED_10GB_FULL:
		baudrate = IF_Gbps(10);
		break;
	case IXGBE_LINK_SPEED_5GB_FULL:
		baudrate = IF_Gbps(5);
		break;
	case IXGBE_LINK_SPEED_2_5GB_FULL:
		baudrate = IF_Mbps(2500);
		break;
	case IXGBE_LINK_SPEED_1GB_FULL:
		baudrate = IF_Gbps(1);
		break;
	case IXGBE_LINK_SPEED_100_FULL:
		baudrate = IF_Mbps(100);
		break;
	case IXGBE_LINK_SPEED_10_FULL:
		baudrate = IF_Mbps(10);
		break;
	case IXGBE_LINK_SPEED_UNKNOWN:
	default:
		baudrate = 0;
		break;
	}

	return baudrate;
}

void
ixgbe_init_lock(struct ixgbe_lock *lock)
{
	mtx_init(&lock->mutex, "mutex",
	    "ixgbe ACI lock", MTX_DEF | MTX_DUPOK);
}

void
ixgbe_acquire_lock(struct ixgbe_lock *lock)
{
	mtx_lock(&lock->mutex);
}

void
ixgbe_release_lock(struct ixgbe_lock *lock)
{
	mtx_unlock(&lock->mutex);
}

void
ixgbe_destroy_lock(struct ixgbe_lock *lock)
{
	if (mtx_initialized(&lock->mutex))
		mtx_destroy(&lock->mutex);
}
