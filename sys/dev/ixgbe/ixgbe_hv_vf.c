/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2024 Intel Corporation
 */

#include "ixgbe_common.h"
#include "ixgbe_vf.h"
#include "ixgbe_hv_vf.h"

/* Reading these PCI configuration bytes resets a VF on Hyper-V. */
#define IXGBE_HV_RESET_OFFSET	0x201

/* Hyper-V emulates the VF queue limit in PCI configuration space. */
#define IXGBE_HV_QUEUE_COUNT_OFFSET	0x207

/* E610 exposes VFLINKS in four emulated PCI configuration bytes. */
#define IXGBE_HV_LINK_STATUS_OFFSET	0x209
#define IXGBE_HV_LINK_STATUS_SIZE	4

/**
 * ixgbevf_hv_get_queues - Discover supported Hyper-V RSS queues at attach
 * @hw: pointer to hardware structure
 */
u32
ixgbevf_hv_get_queues(struct ixgbe_hw *hw)
{
	u32 grant, queues = 1;

	switch (hw->mac.type) {
	case ixgbe_mac_X550_vf:
	case ixgbe_mac_X550EM_x_vf:
	case ixgbe_mac_X550EM_a_vf:
		/* 0x208 is a separate DCB flag on these PFs, not count bits. */
		grant = IXGBE_READ_PCIE_BYTE(hw, IXGBE_HV_QUEUE_COUNT_OFFSET);
		break;
	case ixgbe_mac_E610_vf:
		/* E610 reports the assigned count as a little-endian word. */
		grant = IXGBE_READ_PCIE_BYTE(hw, IXGBE_HV_QUEUE_COUNT_OFFSET);
		grant |= (u32)IXGBE_READ_PCIE_BYTE(hw,
		    IXGBE_HV_QUEUE_COUNT_OFFSET + 1) << NBBY;
		break;
	default:
		/* Older PFs do not provide VF-owned RSS on Hyper-V. */
		grant = 1;
		break;
	}
	/* Valid limits are 1, 2 and 4; use at most two symmetric RSS queues. */
	if (grant == 2 || grant == 4)
		queues = 2;

	/* Shared stop must disable every queue that iflib can initialize. */
	hw->mac.max_tx_queues = queues;
	hw->mac.max_rx_queues = queues;
	return (queues);
}

/**
 * ixgbevf_hv_update_mc_addr_list_vf - Hyper-V variant - just a stub.
 * @hw: unused
 * @mc_addr_list: unused
 * @mc_addr_count: unused
 * @next: unused
 * @clear: unused
 */
static s32
ixgbevf_hv_update_mc_addr_list_vf(struct ixgbe_hw *hw, u8 *mc_addr_list,
    u32 mc_addr_count, ixgbe_mc_addr_itr next, bool clear)
{
	UNREFERENCED_5PARAMETER(hw, mc_addr_list, mc_addr_count, next, clear);

	return (IXGBE_ERR_FEATURE_NOT_SUPPORTED);
}

/**
 * ixgbevf_hv_update_xcast_mode - Leave receive-mode policy to Hyper-V
 * @hw: unused
 * @xcast_mode: unused
 */
static s32
ixgbevf_hv_update_xcast_mode(struct ixgbe_hw *hw, int xcast_mode)
{
	UNREFERENCED_2PARAMETER(hw, xcast_mode);

	return (IXGBE_SUCCESS);
}

/**
 * ixgbevf_hv_set_vfta_vf - Hyper-V variant - just a stub.
 * @hw: unused
 * @vlan: unused
 * @vind: unused
 * @vlan_on: unused
 * @vlvf_bypass: unused
 */
static s32
ixgbevf_hv_set_vfta_vf(struct ixgbe_hw *hw, u32 vlan, u32 vind,
    bool vlan_on, bool vlvf_bypass)
{
	UNREFERENCED_5PARAMETER(hw, vlan, vind, vlan_on, vlvf_bypass);

	return (IXGBE_ERR_FEATURE_NOT_SUPPORTED);
}

static s32
ixgbevf_hv_set_uc_addr_vf(struct ixgbe_hw *hw, u32 index, u8 *addr)
{
	UNREFERENCED_3PARAMETER(hw, index, addr);

	return (IXGBE_ERR_FEATURE_NOT_SUPPORTED);
}

/**
 * ixgbevf_hv_reset_hw_vf - Reset through the Hyper-V PCI side channel
 * @hw: pointer to hardware structure
 *
 * Hyper-V returns the permanent VF address when the guest reads six bytes
 * beginning at offset 0x201 in PCI configuration space.  The reads also
 * perform the host-side VF reset handshake.
 */
static s32
ixgbevf_hv_reset_hw_vf(struct ixgbe_hw *hw)
{
	int i;

	hw->api_version = ixgbe_mbox_api_10;
	for (i = 0; i < IXGBE_ETH_LENGTH_OF_ADDRESS; i++)
		hw->mac.perm_addr[i] = IXGBE_READ_PCIE_BYTE(hw,
		    IXGBE_HV_RESET_OFFSET + i);
	if (ixgbe_validate_mac_addr(hw->mac.perm_addr) != IXGBE_SUCCESS)
		return (IXGBE_ERR_INVALID_MAC_ADDR);

	return (IXGBE_SUCCESS);
}

/**
 * ixgbevf_hv_set_rar_vf - Hyper-V variant - just a stub.
 * @hw: unused
 * @index: unused
 * @addr: unused
 * @vmdq: unused
 * @enable_addr: unused
 */
static s32
ixgbevf_hv_set_rar_vf(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
    u32 enable_addr)
{
	UNREFERENCED_5PARAMETER(hw, index, addr, vmdq, enable_addr);

	return (IXGBE_ERR_FEATURE_NOT_SUPPORTED);
}

static u32
ixgbevf_hv_read_links(struct ixgbe_hw *hw)
{
	u32 links_reg;
	int i;

	if (hw->mac.type != ixgbe_mac_E610_vf)
		return (IXGBE_READ_REG(hw, IXGBE_VFLINKS));

	links_reg = 0;
	for (i = 0; i < IXGBE_HV_LINK_STATUS_SIZE; i++)
		links_reg |= (u32)IXGBE_READ_PCIE_BYTE(hw,
		    IXGBE_HV_LINK_STATUS_OFFSET + i) << (i * NBBY);
	return (links_reg);
}

/**
 * ixgbevf_hv_check_mac_link_vf - Check link without mailbox communication
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @link_up: true is link is up, false otherwise
 * @autoneg_wait_to_complete: unused
 */
static s32
ixgbevf_hv_check_mac_link_vf(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
    bool *link_up, bool autoneg_wait_to_complete)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	struct ixgbe_mac_info *mac = &hw->mac;
	u32 links_reg;

	UNREFERENCED_1PARAMETER(autoneg_wait_to_complete);

	/* If we were hit with a reset, drop the cached link state. */
	if (!mbx->ops[0].check_for_rst(hw, 0) || !mbx->timeout)
		mac->get_link_status = true;
	if (!mac->get_link_status)
		goto out;

	links_reg = ixgbevf_hv_read_links(hw);
	if (hw->mac.type == ixgbe_mac_E610_vf) {
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		if (links_reg == UINT32_MAX)
			goto out;
		if (links_reg == 0) {
			/*
			 * Some hosts do not emulate the PCI link-status query.
			 * VFLINKS still reports carrier, but its speed is not the
			 * negotiated line rate.  Do not report its default 10 Gb/s.
			 */
			links_reg = IXGBE_READ_REG(hw, IXGBE_VFLINKS);
			if (links_reg != UINT32_MAX &&
			    (links_reg & IXGBE_LINKS_UP) != 0)
				mac->get_link_status = false;
			goto out;
		}
	}
	if (!(links_reg & IXGBE_LINKS_UP))
		goto out;

	/* Link status can take up to 500 usec to settle on 82599. */
	if (mac->type == ixgbe_mac_82599_vf) {
		int i;

		for (i = 0; i < 5; i++) {
			usec_delay(100);
			links_reg = ixgbevf_hv_read_links(hw);
			if (!(links_reg & IXGBE_LINKS_UP))
				goto out;
		}
	}

	switch (links_reg & IXGBE_LINKS_SPEED_82599) {
	case IXGBE_LINKS_SPEED_10G_82599:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		if (hw->mac.type >= ixgbe_mac_X550_vf &&
		    (links_reg & IXGBE_LINKS_SPEED_NON_STD) != 0)
			*speed = IXGBE_LINK_SPEED_2_5GB_FULL;
		break;
	case IXGBE_LINKS_SPEED_1G_82599:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		break;
	case IXGBE_LINKS_SPEED_100_82599:
		*speed = IXGBE_LINK_SPEED_100_FULL;
		if ((hw->mac.type == ixgbe_mac_X550_vf ||
		    hw->mac.type == ixgbe_mac_E610_vf) &&
		    (links_reg & IXGBE_LINKS_SPEED_NON_STD) != 0)
			*speed = IXGBE_LINK_SPEED_5GB_FULL;
		break;
	case IXGBE_LINKS_SPEED_10_X550EM_A:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		if (hw->mac.type >= ixgbe_mac_X550_vf)
			*speed = IXGBE_LINK_SPEED_10_FULL;
		break;
	default:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
	}

	mac->get_link_status = false;

out:
	*link_up = !mac->get_link_status;
	return (IXGBE_SUCCESS);
}

/**
 * ixgbevf_hv_get_link_state_vf - Hyper-V has no mailbox link policy
 * @hw: unused
 * @link_state: unused
 */
static s32
ixgbevf_hv_get_link_state_vf(struct ixgbe_hw *hw, bool *link_state)
{
	UNREFERENCED_2PARAMETER(hw, link_state);

	return (IXGBE_ERR_FEATURE_NOT_SUPPORTED);
}

/**
 * ixgbevf_hv_set_rlpml_vf - Set the maximum receive packet length
 * @hw: pointer to hardware structure
 * @max_size: maximum frame size
 */
static s32
ixgbevf_hv_set_rlpml_vf(struct ixgbe_hw *hw, u16 max_size)
{
	u32 i, reg;

	/* RLPML is not implemented by the 82599 VF. */
	if (hw->mac.type == ixgbe_mac_82599_vf)
		return (IXGBE_SUCCESS);

	for (i = 0; i < hw->mac.max_rx_queues; i++) {
		reg = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
		reg &= ~IXGBE_RXDCTL_RLPMLMASK;
		reg |= ((max_size + 4) & IXGBE_RXDCTL_RLPMLMASK) |
		    IXGBE_RXDCTL_RLPML_EN;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), reg);
	}

	return (IXGBE_SUCCESS);
}

/**
 * ixgbevf_hv_negotiate_api_version_vf - Negotiate the Hyper-V API version
 * @hw: pointer to hardware structure
 * @api: requested API version
 */
static int
ixgbevf_hv_negotiate_api_version_vf(struct ixgbe_hw *hw, int api)
{
	if (api != ixgbe_mbox_api_10)
		return (IXGBE_ERR_INVALID_ARGUMENT);

	hw->api_version = api;
	return (IXGBE_SUCCESS);
}

/**
 * ixgbevf_hv_init_ops_vf - Initialize Hyper-V VF operations
 * @hw: pointer to hardware structure
 */
s32
ixgbevf_hv_init_ops_vf(struct ixgbe_hw *hw)
{
	s32 status;

	status = ixgbe_init_ops_vf(hw);
	if (status != IXGBE_SUCCESS)
		return (status);

	hw->mac.ops.reset_hw = ixgbevf_hv_reset_hw_vf;
	hw->mac.ops.check_link = ixgbevf_hv_check_mac_link_vf;
	hw->mac.ops.negotiate_api_version =
	    ixgbevf_hv_negotiate_api_version_vf;
	hw->mac.ops.set_rar = ixgbevf_hv_set_rar_vf;
	hw->mac.ops.update_mc_addr_list =
	    ixgbevf_hv_update_mc_addr_list_vf;
	hw->mac.ops.update_xcast_mode = ixgbevf_hv_update_xcast_mode;
	hw->mac.ops.get_link_state = ixgbevf_hv_get_link_state_vf;
	hw->mac.ops.set_uc_addr = ixgbevf_hv_set_uc_addr_vf;
	hw->mac.ops.set_vfta = ixgbevf_hv_set_vfta_vf;
	hw->mac.ops.set_rlpml = ixgbevf_hv_set_rlpml_vf;

	return (IXGBE_SUCCESS);
}
