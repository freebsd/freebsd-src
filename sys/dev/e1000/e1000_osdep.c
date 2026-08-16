/******************************************************************************
  SPDX-License-Identifier: BSD-3-Clause

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

#include "e1000_api.h"

int e1000_use_pause_delay = 0;

/*
 * Wait while the 82579 Management Engine owns the PCIm2PCI arbiter.  DELAY
 * is required because CSR writes also occur from interrupt and datapath
 * contexts where sleeping is forbidden.
 */
static void
e1000_pcim2pci_arbiter_wait(struct e1000_osdep *osdep)
{
	int i;

	i = E1000_ICH_FWSM_PCIM2PCI_COUNT;
	while ((bus_space_read_4(osdep->mem_bus_space_tag,
	    osdep->mem_bus_space_handle, E1000_FWSM) &
	    E1000_ICH_FWSM_PCIM2PCI) != 0 && --i != 0)
		DELAY(50);
}

/*
 * Serialize an 82579 MAC CSR write against the Management Engine.  The
 * FreeBSD driver exposes one queue on this controller, so recognize its two
 * tail registers here and verify them without imposing tail-specific APIs on
 * the rest of the e1000 family.
 */
void
e1000_pcim2pci_write(struct e1000_osdep *osdep, uint32_t reg, uint32_t value)
{
	uint32_t control, control_reg, enable;
	const char *direction;

	e1000_pcim2pci_arbiter_wait(osdep);
	bus_space_write_4(osdep->mem_bus_space_tag,
	    osdep->mem_bus_space_handle, reg, value);

	if (reg == E1000_TDT(0)) {
		control_reg = E1000_TCTL;
		enable = E1000_TCTL_EN;
		direction = "transmit";
	} else if (reg == E1000_RDT(0)) {
		control_reg = E1000_RCTL;
		enable = E1000_RCTL_EN;
		direction = "receive";
	} else {
		return;
	}
	if (bus_space_read_4(osdep->mem_bus_space_tag,
	    osdep->mem_bus_space_handle, reg) == value)
		return;

	control = bus_space_read_4(osdep->mem_bus_space_tag,
	    osdep->mem_bus_space_handle, control_reg);
	e1000_pcim2pci_arbiter_wait(osdep);
	bus_space_write_4(osdep->mem_bus_space_tag,
	    osdep->mem_bus_space_handle, control_reg, control & ~enable);
	device_printf(osdep->dev,
	    "Management Engine caused an invalid %s tail write; "
	    "requesting reset\n", direction);
	iflib_request_reset(osdep->ctx);
	iflib_admin_intr_deferred(osdep->ctx);
}

static void
e1000_enable_pause_delay(void *use_pause_delay)
{
	*((int *)use_pause_delay) = 1;
}

SYSINIT(enable_pause_delay, SI_SUB_CLOCKS, SI_ORDER_ANY, e1000_enable_pause_delay, &e1000_use_pause_delay);

/*
 * NOTE: the following routines using the e1000 
 * 	naming style are provided to the shared
 *	code but are OS specific
 */

void
e1000_write_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	pci_write_config(((struct e1000_osdep *)hw->back)->dev, reg, *value, 2);
}

void
e1000_read_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	*value = pci_read_config(((struct e1000_osdep *)hw->back)->dev, reg, 2);
}

void
e1000_pci_set_mwi(struct e1000_hw *hw)
{
	pci_write_config(((struct e1000_osdep *)hw->back)->dev, PCIR_COMMAND,
	    (hw->bus.pci_cmd_word | CMD_MEM_WRT_INVALIDATE), 2);
}

void
e1000_pci_clear_mwi(struct e1000_hw *hw)
{
	pci_write_config(((struct e1000_osdep *)hw->back)->dev, PCIR_COMMAND,
	    (hw->bus.pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE), 2);
}

/*
 * Read the PCI Express capabilities
 */
int32_t
e1000_read_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	device_t dev = ((struct e1000_osdep *)hw->back)->dev;
	u32	offset;

	pci_find_cap(dev, PCIY_EXPRESS, &offset);
	*value = pci_read_config(dev, offset + reg, 2);
	return (E1000_SUCCESS);
}

/*
 * Write the PCI Express capabilities
 */
int32_t
e1000_write_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	device_t dev = ((struct e1000_osdep *)hw->back)->dev;
	u32	offset;

	pci_find_cap(dev, PCIY_EXPRESS, &offset);
	pci_write_config(dev, offset + reg, *value, 2);
	return (E1000_SUCCESS);
}
