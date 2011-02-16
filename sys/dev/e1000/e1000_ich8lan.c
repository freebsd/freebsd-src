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
/*$FreeBSD: src/sys/dev/e1000/e1000_ich8lan.c,v 1.5.2.3.2.1 2010/12/21 17:09:25 kensmith Exp $*/

/*
 * 82562G 10/100 Network Connection
 * 82562G-2 10/100 Network Connection
 * 82562GT 10/100 Network Connection
 * 82562GT-2 10/100 Network Connection
 * 82562V 10/100 Network Connection
 * 82562V-2 10/100 Network Connection
 * 82566DC-2 Gigabit Network Connection
 * 82566DC Gigabit Network Connection
 * 82566DM-2 Gigabit Network Connection
 * 82566DM Gigabit Network Connection
 * 82566MC Gigabit Network Connection
 * 82566MM Gigabit Network Connection
 * 82567LM Gigabit Network Connection
 * 82567LF Gigabit Network Connection
 * 82567V Gigabit Network Connection
 * 82567LM-2 Gigabit Network Connection
 * 82567LF-2 Gigabit Network Connection
 * 82567V-2 Gigabit Network Connection
 * 82567LF-3 Gigabit Network Connection
 * 82567LM-3 Gigabit Network Connection
 * 82567LM-4 Gigabit Network Connection
 * 82577LM Gigabit Network Connection
 * 82577LC Gigabit Network Connection
 * 82578DM Gigabit Network Connection
 * 82578DC Gigabit Network Connection
 * 82579LM Gigabit Network Connection
 * 82579V Gigabit Network Connection
 */

#include "e1000_api.h"

static s32  e1000_init_phy_params_ich8lan(struct e1000_hw *hw);
static s32 e1000_init_phy_params_pchlan(struct e1000_hw *hw);
static s32  e1000_init_nvm_params_ich8lan(struct e1000_hw *hw);
static s32  e1000_init_mac_params_ich8lan(struct e1000_hw *hw);
static s32  e1000_acquire_swflag_ich8lan(struct e1000_hw *hw);
static void e1000_release_swflag_ich8lan(struct e1000_hw *hw);
static s32  e1000_acquire_nvm_ich8lan(struct e1000_hw *hw);
static void e1000_release_nvm_ich8lan(struct e1000_hw *hw);
static bool e1000_check_mng_mode_ich8lan(struct e1000_hw *hw);
static bool e1000_check_mng_mode_pchlan(struct e1000_hw *hw);
static void e1000_rar_set_pch2lan(struct e1000_hw *hw, u8 *addr, u32 index);
static s32  e1000_check_reset_block_ich8lan(struct e1000_hw *hw);
static s32  e1000_phy_hw_reset_ich8lan(struct e1000_hw *hw);
static s32  e1000_set_lplu_state_pchlan(struct e1000_hw *hw, bool active);
static s32  e1000_set_d0_lplu_state_ich8lan(struct e1000_hw *hw,
                                            bool active);
static s32  e1000_set_d3_lplu_state_ich8lan(struct e1000_hw *hw,
                                            bool active);
static s32  e1000_read_nvm_ich8lan(struct e1000_hw *hw, u16 offset,
                                   u16 words, u16 *data);
static s32  e1000_write_nvm_ich8lan(struct e1000_hw *hw, u16 offset,
                                    u16 words, u16 *data);
static s32  e1000_validate_nvm_checksum_ich8lan(struct e1000_hw *hw);
static s32  e1000_update_nvm_checksum_ich8lan(struct e1000_hw *hw);
static s32  e1000_valid_led_default_ich8lan(struct e1000_hw *hw,
                                            u16 *data);
static s32 e1000_id_led_init_pchlan(struct e1000_hw *hw);
static s32  e1000_get_bus_info_ich8lan(struct e1000_hw *hw);
static s32  e1000_reset_hw_ich8lan(struct e1000_hw *hw);
static s32  e1000_init_hw_ich8lan(struct e1000_hw *hw);
static s32  e1000_setup_link_ich8lan(struct e1000_hw *hw);
static s32  e1000_setup_copper_link_ich8lan(struct e1000_hw *hw);
static s32  e1000_get_link_up_info_ich8lan(struct e1000_hw *hw,
                                           u16 *speed, u16 *duplex);
static s32  e1000_cleanup_led_ich8lan(struct e1000_hw *hw);
static s32  e1000_led_on_ich8lan(struct e1000_hw *hw);
static s32  e1000_led_off_ich8lan(struct e1000_hw *hw);
static s32  e1000_k1_gig_workaround_hv(struct e1000_hw *hw, bool link);
static s32  e1000_setup_led_pchlan(struct e1000_hw *hw);
static s32  e1000_cleanup_led_pchlan(struct e1000_hw *hw);
static s32  e1000_led_on_pchlan(struct e1000_hw *hw);
static s32  e1000_led_off_pchlan(struct e1000_hw *hw);
static void e1000_clear_hw_cntrs_ich8lan(struct e1000_hw *hw);
static s32  e1000_erase_flash_bank_ich8lan(struct e1000_hw *hw, u32 bank);
static s32  e1000_flash_cycle_ich8lan(struct e1000_hw *hw, u32 timeout);
static s32  e1000_flash_cycle_init_ich8lan(struct e1000_hw *hw);
static void e1000_initialize_hw_bits_ich8lan(struct e1000_hw *hw);
static s32  e1000_kmrn_lock_loss_workaround_ich8lan(struct e1000_hw *hw);
static s32  e1000_read_flash_byte_ich8lan(struct e1000_hw *hw,
                                          u32 offset, u8 *data);
static s32  e1000_read_flash_data_ich8lan(struct e1000_hw *hw, u32 offset,
                                          u8 size, u16 *data);
static s32  e1000_read_flash_word_ich8lan(struct e1000_hw *hw,
                                          u32 offset, u16 *data);
static s32  e1000_retry_write_flash_byte_ich8lan(struct e1000_hw *hw,
                                                 u32 offset, u8 byte);
static s32  e1000_write_flash_byte_ich8lan(struct e1000_hw *hw,
                                           u32 offset, u8 data);
static s32  e1000_write_flash_data_ich8lan(struct e1000_hw *hw, u32 offset,
                                           u8 size, u16 data);
static s32 e1000_get_cfg_done_ich8lan(struct e1000_hw *hw);
static void e1000_power_down_phy_copper_ich8lan(struct e1000_hw *hw);
static s32 e1000_check_for_copper_link_ich8lan(struct e1000_hw *hw);
static void e1000_lan_init_done_ich8lan(struct e1000_hw *hw);
static s32 e1000_sw_lcd_config_ich8lan(struct e1000_hw *hw);
static s32 e1000_set_mdio_slow_mode_hv(struct e1000_hw *hw);
static s32 e1000_k1_workaround_lv(struct e1000_hw *hw);
static void e1000_gate_hw_phy_config_ich8lan(struct e1000_hw *hw, bool gate);

/* ICH GbE Flash Hardware Sequencing Flash Status Register bit breakdown */
/* Offset 04h HSFSTS */
union ich8_hws_flash_status {
	struct ich8_hsfsts {
		u16 flcdone    :1; /* bit 0 Flash Cycle Done */
		u16 flcerr     :1; /* bit 1 Flash Cycle Error */
		u16 dael       :1; /* bit 2 Direct Access error Log */
		u16 berasesz   :2; /* bit 4:3 Sector Erase Size */
		u16 flcinprog  :1; /* bit 5 flash cycle in Progress */
		u16 reserved1  :2; /* bit 13:6 Reserved */
		u16 reserved2  :6; /* bit 13:6 Reserved */
		u16 fldesvalid :1; /* bit 14 Flash Descriptor Valid */
		u16 flockdn    :1; /* bit 15 Flash Config Lock-Down */
	} hsf_status;
	u16 regval;
};

/* ICH GbE Flash Hardware Sequencing Flash control Register bit breakdown */
/* Offset 06h FLCTL */
union ich8_hws_flash_ctrl {
	struct ich8_hsflctl {
		u16 flcgo      :1;   /* 0 Flash Cycle Go */
		u16 flcycle    :2;   /* 2:1 Flash Cycle */
		u16 reserved   :5;   /* 7:3 Reserved  */
		u16 fldbcount  :2;   /* 9:8 Flash Data Byte Count */
		u16 flockdn    :6;   /* 15:10 Reserved */
	} hsf_ctrl;
	u16 regval;
};

/* ICH Flash Region Access Permissions */
union ich8_hws_flash_regacc {
	struct ich8_flracc {
		u32 grra      :8; /* 0:7 GbE region Read Access */
		u32 grwa      :8; /* 8:15 GbE region Write Access */
		u32 gmrag     :8; /* 23:16 GbE Master Read Access Grant */
		u32 gmwag     :8; /* 31:24 GbE Master Write Access Grant */
	} hsf_flregacc;
	u16 regval;
};

/**
 *  e1000_init_phy_params_pchlan - Initialize PHY function pointers
 *  @hw: pointer to the HW structure
 *
 *  Initialize family-specific PHY parameters and function pointers.
 **/
static s32 e1000_init_phy_params_pchlan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 ctrl, fwsm;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_init_phy_params_pchlan");

	phy->addr                     = 1;
	phy->reset_delay_us           = 100;

	phy->ops.acquire              = e1000_acquire_swflag_ich8lan;
	phy->ops.check_reset_block    = e1000_check_reset_block_ich8lan;
	phy->ops.get_cfg_done         = e1000_get_cfg_done_ich8lan;
	phy->ops.read_reg             = e1000_read_phy_reg_hv;
	phy->ops.read_reg_locked      = e1000_read_phy_reg_hv_locked;
	phy->ops.release              = e1000_release_swflag_ich8lan;
	phy->ops.reset                = e1000_phy_hw_reset_ich8lan;
	phy->ops.set_d0_lplu_state    = e1000_set_lplu_state_pchlan;
	phy->ops.set_d3_lplu_state    = e1000_set_lplu_state_pchlan;
	phy->ops.write_reg            = e1000_write_phy_reg_hv;
	phy->ops.write_reg_locked     = e1000_write_phy_reg_hv_locked;
	phy->ops.power_up             = e1000_power_up_phy_copper;
	phy->ops.power_down           = e1000_power_down_phy_copper_ich8lan;
	phy->autoneg_mask             = AUTONEG_ADVERTISE_SPEED_DEFAULT;

	/*
	 * The MAC-PHY interconnect may still be in SMBus mode
	 * after Sx->S0.  If the manageability engine (ME) is
	 * disabled, then toggle the LANPHYPC Value bit to force
	 * the interconnect to PCIe mode.
	 */
	fwsm = E1000_READ_REG(hw, E1000_FWSM);
	if (!(fwsm & E1000_ICH_FWSM_FW_VALID)) {
		ctrl = E1000_READ_REG(hw, E1000_CTRL);
		ctrl |=  E1000_CTRL_LANPHYPC_OVERRIDE;
		ctrl &= ~E1000_CTRL_LANPHYPC_VALUE;
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl);
		usec_delay(10);
		ctrl &= ~E1000_CTRL_LANPHYPC_OVERRIDE;
		E1000_WRITE_REG(hw, E1000_CTRL, ctrl);
		msec_delay(50);

		/*
		 * Gate automatic PHY configuration by hardware on
		 * non-managed 82579
		 */
		if (hw->mac.type == e1000_pch2lan)
			e1000_gate_hw_phy_config_ich8lan(hw, TRUE);
	}

	/*
	 * Reset the PHY before any acccess to it.  Doing so, ensures that
	 * the PHY is in a known good state before we read/write PHY registers.
	 * The generic reset is sufficient here, because we haven't determined
	 * the PHY type yet.
	 */
	ret_val = e1000_phy_hw_reset_generic(hw);
	if (ret_val)
		goto out;

	/* Ungate automatic PHY configuration on non-managed 82579 */
	if ((hw->mac.type == e1000_pch2lan)  &&
	    !(fwsm & E1000_ICH_FWSM_FW_VALID)) {
		msec_delay(10);
		e1000_gate_hw_phy_config_ich8lan(hw, FALSE);
	}

	phy->id = e1000_phy_unknown;
	switch (hw->mac.type) {
	default:
		ret_val = e1000_get_phy_id(hw);
		if (ret_val)
			goto out;
		if ((phy->id != 0) && (phy->id != PHY_REVISION_MASK))
			break;
		/* fall-through */
	case e1000_pch2lan:
		/*
		 * In case the PHY needs to be in mdio slow mode,
		 * set slow mode and try to get the PHY id again.
		 */
		ret_val = e1000_set_mdio_slow_mode_hv(hw);
		if (ret_val)
			goto out;
		ret_val = e1000_get_phy_id(hw);
		if (ret_val)
			goto out;
		break;
	}
	phy->type = e1000_get_phy_type_from_id(phy->id);

	switch (phy->type) {
	case e1000_phy_82577:
	case e1000_phy_82579:
		phy->ops.check_polarity = e1000_check_polarity_82577;
		phy->ops.force_speed_duplex =
			e1000_phy_force_speed_duplex_82577;
		phy->ops.get_cable_length = e1000_get_cable_length_82577;
		phy->ops.get_info = e1000_get_phy_info_82577;
		phy->ops.commit = e1000_phy_sw_reset_generic;
		break;
	case e1000_phy_82578:
		phy->ops.check_polarity = e1000_check_polarity_m88;
		phy->ops.force_speed_duplex = e1000_phy_force_speed_duplex_m88;
		phy->ops.get_cable_length = e1000_get_cable_length_m88;
		phy->ops.get_info = e1000_get_phy_info_m88;
		break;
	default:
		ret_val = -E1000_ERR_PHY;
		break;
	}

out:
	return ret_val;
}

/**
 *  e1000_init_phy_params_ich8lan - Initialize PHY function pointers
 *  @hw: pointer to the HW structure
 *
 *  Initialize family-specific PHY parameters and function pointers.
 **/
static s32 e1000_init_phy_params_ich8lan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = E1000_SUCCESS;
	u16 i = 0;

	DEBUGFUNC("e1000_init_phy_params_ich8lan");

	phy->addr                     = 1;
	phy->reset_delay_us           = 100;

	phy->ops.acquire              = e1000_acquire_swflag_ich8lan;
	phy->ops.check_reset_block    = e1000_check_reset_block_ich8lan;
	phy->ops.get_cable_length     = e1000_get_cable_length_igp_2;
	phy->ops.get_cfg_done         = e1000_get_cfg_done_ich8lan;
	phy->ops.read_reg             = e1000_read_phy_reg_igp;
	phy->ops.release              = e1000_release_swflag_ich8lan;
	phy->ops.reset                = e1000_phy_hw_reset_ich8lan;
	phy->ops.set_d0_lplu_state    = e1000_set_d0_lplu_state_ich8lan;
	phy->ops.set_d3_lplu_state    = e1000_set_d3_lplu_state_ich8lan;
	phy->ops.write_reg            = e1000_write_phy_reg_igp;
	phy->ops.power_up             = e1000_power_up_phy_copper;
	phy->ops.power_down           = e1000_power_down_phy_copper_ich8lan;

	/*
	 * We may need to do this twice - once for IGP and if that fails,
	 * we'll set BM func pointers and try again
	 */
	ret_val = e1000_determine_phy_address(hw);
	if (ret_val) {
		phy->ops.write_reg = e1000_write_phy_reg_bm;
		phy->ops.read_reg  = e1000_read_phy_reg_bm;
		ret_val = e1000_determine_phy_address(hw);
		if (ret_val) {
			DEBUGOUT("Cannot determine PHY addr. Erroring out\n");
			goto out;
		}
	}

	phy->id = 0;
	while ((e1000_phy_unknown == e1000_get_phy_type_from_id(phy->id)) &&
	       (i++ < 100)) {
		msec_delay(1);
		ret_val = e1000_get_phy_id(hw);
		if (ret_val)
			goto out;
	}

	/* Verify phy id */
	switch (phy->id) {
	case IGP03E1000_E_PHY_ID:
		phy->type = e1000_phy_igp_3;
		phy->autoneg_mask = AUTONEG_ADVERTISE_SPEED_DEFAULT;
		phy->ops.read_reg_locked = e1000_read_phy_reg_igp_locked;
		phy->ops.write_reg_locked = e1000_write_phy_reg_igp_locked;
		phy->ops.get_info = e1000_get_phy_info_igp;
		phy->ops.check_polarity = e1000_check_polarity_igp;
		phy->ops.force_speed_duplex = e1000_phy_force_speed_duplex_igp;
		break;
	case IFE_E_PHY_ID:
	case IFE_PLUS_E_PHY_ID:
	case IFE_C_E_PHY_ID:
		phy->type = e1000_phy_ife;
		phy->autoneg_mask = E1000_ALL_NOT_GIG;
		phy->ops.get_info = e1000_get_phy_info_ife;
		phy->ops.check_polarity = e1000_check_polarity_ife;
		phy->ops.force_speed_duplex = e1000_phy_force_speed_duplex_ife;
		break;
	case BME1000_E_PHY_ID:
		phy->type = e1000_phy_bm;
		phy->autoneg_mask = AUTONEG_ADVERTISE_SPEED_DEFAULT;
		phy->ops.read_reg = e1000_read_phy_reg_bm;
		phy->ops.write_reg = e1000_write_phy_reg_bm;
		phy->ops.commit = e1000_phy_sw_reset_generic;
		phy->ops.get_info = e1000_get_phy_info_m88;
		phy->ops.check_polarity = e1000_check_polarity_m88;
		phy->ops.force_speed_duplex = e1000_phy_force_speed_duplex_m88;
		break;
	default:
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_init_nvm_params_ich8lan - Initialize NVM function pointers
 *  @hw: pointer to the HW structure
 *
 *  Initialize family-specific NVM parameters and function
 *  pointers.
 **/
static s32 e1000_init_nvm_params_ich8lan(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_dev_spec_ich8lan *dev_spec = &hw->dev_spec.ich8lan;
	u32 gfpreg, sector_base_addr, sector_end_addr;
	s32 ret_val = E1000_SUCCESS;
	u16 i;

	DEBUGFUNC("e1000_init_nvm_params_ich8lan");

	/* Can't read flash registers if the register set isn't mapped. */
	if (!hw->flash_address) {
		DEBUGOUT("ERROR: Flash registers not mapped\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	nvm->type = e1000_nvm_flash_sw;

	gfpreg = E1000_READ_FLASH_REG(hw, ICH_FLASH_GFPREG);

	/*
	 * sector_X_addr is a "sector"-aligned address (4096 bytes)
	 * Add 1 to sector_end_addr since this sector is included in
	 * the overall size.
	 */
	sector_base_addr = gfpreg & FLASH_GFPREG_BASE_MASK;
	sector_end_addr = ((gfpreg >> 16) & FLASH_GFPREG_BASE_MASK) + 1;

	/* flash_base_addr is byte-aligned */
	nvm->flash_base_addr = sector_base_addr << FLASH_SECTOR_ADDR_SHIFT;

	/*
	 * find total size of the NVM, then cut in half since the total
	 * size represents two separate NVM banks.
	 */
	nvm->flash_bank_size = (sector_end_addr - sector_base_addr)
	                          << FLASH_SECTOR_ADDR_SHIFT;
	nvm->flash_bank_size /= 2;
	/* Adjust to word count */
	nvm->flash_bank_size /= sizeof(u16);

	nvm->word_size = E1000_SHADOW_RAM_WORDS;

	/* Clear shadow ram */
	for (i = 0; i < nvm->word_size; i++) {
		dev_spec->shadow_ram[i].modified = FALSE;
		dev_spec->shadow_ram[i].value    = 0xFFFF;
	}

	E1000_MUTEX_INIT(&dev_spec->nvm_mutex);
	E1000_MUTEX_INIT(&dev_spec->swflag_mutex);

	/* Function Pointers */
	nvm->ops.acquire       = e1000_acquire_nvm_ich8lan;
	nvm->ops.release       = e1000_release_nvm_ich8lan;
	nvm->ops.read          = e1000_read_nvm_ich8lan;
	nvm->ops.update        = e1000_update_nvm_checksum_ich8lan;
	nvm->ops.valid_led_default = e1000_valid_led_default_ich8lan;
	nvm->ops.validate      = e1000_validate_nvm_checksum_ich8lan;
	nvm->ops.write         = e1000_write_nvm_ich8lan;

out:
	return ret_val;
}

/**
 *  e1000_init_mac_params_ich8lan - Initialize MAC function pointers
 *  @hw: pointer to the HW structure
 *
 *  Initialize family-specific MAC parameters and function
 *  pointers.
 **/
static s32 e1000_init_mac_params_ich8lan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u16 pci_cfg;

	DEBUGFUNC("e1000_init_mac_params_ich8lan");

	/* Set media type function pointer */
	hw->phy.media_type = e1000_media_type_copper;

	/* Set mta register count */
	mac->mta_reg_count = 32;
	/* Set rar entry count */
	mac->rar_entry_count = E1000_ICH_RAR_ENTRIES;
	if (mac->type == e1000_ich8lan)
		mac->rar_entry_count--;
	/* Set if part includes ASF firmware */
	mac->asf_firmware_present = TRUE;
	/* FWSM register */
	mac->has_fwsm = TRUE;
	/* ARC subsystem not supported */
	mac->arc_subsystem_valid = FALSE;
	/* Adaptive IFS supported */
	mac->adaptive_ifs = TRUE;

	/* Function pointers */

	/* bus type/speed/width */
	mac->ops.get_bus_info = e1000_get_bus_info_ich8lan;
	/* function id */
	mac->ops.set_lan_id = e1000_set_lan_id_single_port;
	/* reset */
	mac->ops.reset_hw = e1000_reset_hw_ich8lan;
	/* hw initialization */
	mac->ops.init_hw = e1000_init_hw_ich8lan;
	/* link setup */
	mac->ops.setup_link = e1000_setup_link_ich8lan;
	/* physical interface setup */
	mac->ops.setup_physical_interface = e1000_setup_copper_link_ich8lan;
	/* check for link */
	mac->ops.check_for_link = e1000_check_for_copper_link_ich8lan;
	/* link info */
	mac->ops.get_link_up_info = e1000_get_link_up_info_ich8lan;
	/* multicast address update */
	mac->ops.update_mc_addr_list = e1000_update_mc_addr_list_generic;
	/* clear hardware counters */
	mac->ops.clear_hw_cntrs = e1000_clear_hw_cntrs_ich8lan;

	/* LED operations */
	switch (mac->type) {
	case e1000_ich8lan:
	case e1000_ich9lan:
	case e1000_ich10lan:
		/* check management mode */
		mac->ops.check_mng_mode = e1000_check_mng_mode_ich8lan;
		/* ID LED init */
		mac->ops.id_led_init = e1000_id_led_init_generic;
		/* blink LED */
		mac->ops.blink_led = e1000_blink_led_generic;
		/* setup LED */
		mac->ops.setup_led = e1000_setup_led_generic;
		/* cleanup LED */
		mac->ops.cleanup_led = e1000_cleanup_led_ich8lan;
		/* turn on/off LED */
		mac->ops.led_on = e1000_led_on_ich8lan;
		mac->ops.led_off = e1000_led_off_ich8lan;
		break;
	case e1000_pch2lan:
		mac->rar_entry_count = E1000_PCH2_RAR_ENTRIES;
		mac->ops.rar_set = e1000_rar_set_pch2lan;
		/* fall-through */
	case e1000_pchlan:
		/* save PCH revision_id */
		e1000_read_pci_cfg(hw, 0x2, &pci_cfg);
		hw->revision_id = (u8)(pci_cfg &= 0x000F);
		/* check management mode */
		mac->ops.check_mng_mode = e1000_check_mng_mode_pchlan;
		/* ID LED init */
		mac->ops.id_led_init = e1000_id_led_init_pchlan;
		/* setup LED */
		mac->ops.setup_led = e1000_setup_led_pchlan;
		/* cleanup LED */
		mac->ops.cleanup_led = e1000_cleanup_led_pchlan;
		/* turn on/off LED */
		mac->ops.led_on = e1000_led_on_pchlan;
		mac->ops.led_off = e1000_led_off_pchlan;
		break;
	default:
		break;
	}

	/* Enable PCS Lock-loss workaround for ICH8 */
	if (mac->type == e1000_ich8lan)
		e1000_set_kmrn_lock_loss_workaround_ich8lan(hw, TRUE);

	/* Gate automatic PHY configuration by hardware on managed 82579 */
	if ((mac->type == e1000_pch2lan) &&
	    (E1000_READ_REG(hw, E1000_FWSM) & E1000_ICH_FWSM_FW_VALID))
		e1000_gate_hw_phy_config_ich8lan(hw, TRUE);

	return E1000_SUCCESS;
}

/**
 *  e1000_set_eee_pchlan - Enable/disable EEE support
 *  @hw: pointer to the HW structure
 *
 *  Enable/disable EEE based on setting in dev_spec structure.  The bits in
 *  the LPI Control register will remain set only if/when link is up.
 **/
static s32 e1000_set_eee_pchlan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 phy_reg;

	DEBUGFUNC("e1000_set_eee_pchlan");

	if (hw->phy.type != e1000_phy_82579)
		goto out;

	ret_val = hw->phy.ops.read_reg(hw, I82579_LPI_CTRL, &phy_reg);
	if (ret_val)
		goto out;

	if (hw->dev_spec.ich8lan.eee_disable)
		phy_reg &= ~I82579_LPI_CTRL_ENABLE_MASK;
	else
		phy_reg |= I82579_LPI_CTRL_ENABLE_MASK;

	ret_val = hw->phy.ops.write_reg(hw, I82579_LPI_CTRL, phy_reg);
out:
	return ret_val;
}

/**
 *  e1000_check_for_copper_link_ich8lan - Check for link (Copper)
 *  @hw: pointer to the HW structure
 *
 *  Checks to see of the link status of the hardware has changed.  If a
 *  change in link status has been detected, then we read the PHY registers
 *  to get the current speed/duplex if link exists.
 **/
static s32 e1000_check_for_copper_link_ich8lan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	bool link;

	DEBUGFUNC("e1000_check_for_copper_link_ich8lan");

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

	if (hw->mac.type == e1000_pchlan) {
		ret_val = e1000_k1_gig_workaround_hv(hw, link);
		if (ret_val)
			goto out;
	}

	if (!link)
		goto out; /* No link detected */

	mac->get_link_status = FALSE;

	if (hw->phy.type == e1000_phy_82578) {
		ret_val = e1000_link_stall_workaround_hv(hw);
		if (ret_val)
			goto out;
	}

	if (hw->mac.type == e1000_pch2lan) {
		ret_val = e1000_k1_workaround_lv(hw);
		if (ret_val)
			goto out;
	}

	/*
	 * Check if there was DownShift, must be checked
	 * immediately after link-up
	 */
	e1000_check_downshift_generic(hw);

	/* Enable/Disable EEE after link up */
	ret_val = e1000_set_eee_pchlan(hw);
	if (ret_val)
		goto out;

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
	if (ret_val)
		DEBUGOUT("Error configuring flow control\n");

out:
	return ret_val;
}

/**
 *  e1000_init_function_pointers_ich8lan - Initialize ICH8 function pointers
 *  @hw: pointer to the HW structure
 *
 *  Initialize family-specific function pointers for PHY, MAC, and NVM.
 **/
void e1000_init_function_pointers_ich8lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_init_function_pointers_ich8lan");

	hw->mac.ops.init_params = e1000_init_mac_params_ich8lan;
	hw->nvm.ops.init_params = e1000_init_nvm_params_ich8lan;
	switch (hw->mac.type) {
	case e1000_ich8lan:
	case e1000_ich9lan:
	case e1000_ich10lan:
		hw->phy.ops.init_params = e1000_init_phy_params_ich8lan;
		break;
	case e1000_pchlan:
	case e1000_pch2lan:
		hw->phy.ops.init_params = e1000_init_phy_params_pchlan;
		break;
	default:
		break;
	}
}

/**
 *  e1000_acquire_nvm_ich8lan - Acquire NVM mutex
 *  @hw: pointer to the HW structure
 *
 *  Acquires the mutex for performing NVM operations.
 **/
static s32 e1000_acquire_nvm_ich8lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_acquire_nvm_ich8lan");

	E1000_MUTEX_LOCK(&hw->dev_spec.ich8lan.nvm_mutex);

	return E1000_SUCCESS;
}

/**
 *  e1000_release_nvm_ich8lan - Release NVM mutex
 *  @hw: pointer to the HW structure
 *
 *  Releases the mutex used while performing NVM operations.
 **/
static void e1000_release_nvm_ich8lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_release_nvm_ich8lan");

	E1000_MUTEX_UNLOCK(&hw->dev_spec.ich8lan.nvm_mutex);

	return;
}

/**
 *  e1000_acquire_swflag_ich8lan - Acquire software control flag
 *  @hw: pointer to the HW structure
 *
 *  Acquires the software control flag for performing PHY and select
 *  MAC CSR accesses.
 **/
static s32 e1000_acquire_swflag_ich8lan(struct e1000_hw *hw)
{
	u32 extcnf_ctrl, timeout = PHY_CFG_TIMEOUT;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_acquire_swflag_ich8lan");

	E1000_MUTEX_LOCK(&hw->dev_spec.ich8lan.swflag_mutex);

	while (timeout) {
		extcnf_ctrl = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);
		if (!(extcnf_ctrl & E1000_EXTCNF_CTRL_SWFLAG))
			break;

		msec_delay_irq(1);
		timeout--;
	}

	if (!timeout) {
		DEBUGOUT("SW/FW/HW has locked the resource for too long.\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	timeout = SW_FLAG_TIMEOUT;

	extcnf_ctrl |= E1000_EXTCNF_CTRL_SWFLAG;
	E1000_WRITE_REG(hw, E1000_EXTCNF_CTRL, extcnf_ctrl);

	while (timeout) {
		extcnf_ctrl = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);
		if (extcnf_ctrl & E1000_EXTCNF_CTRL_SWFLAG)
			break;

		msec_delay_irq(1);
		timeout--;
	}

	if (!timeout) {
		DEBUGOUT("Failed to acquire the semaphore.\n");
		extcnf_ctrl &= ~E1000_EXTCNF_CTRL_SWFLAG;
		E1000_WRITE_REG(hw, E1000_EXTCNF_CTRL, extcnf_ctrl);
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

out:
	if (ret_val)
		E1000_MUTEX_UNLOCK(&hw->dev_spec.ich8lan.swflag_mutex);

	return ret_val;
}

/**
 *  e1000_release_swflag_ich8lan - Release software control flag
 *  @hw: pointer to the HW structure
 *
 *  Releases the software control flag for performing PHY and select
 *  MAC CSR accesses.
 **/
static void e1000_release_swflag_ich8lan(struct e1000_hw *hw)
{
	u32 extcnf_ctrl;

	DEBUGFUNC("e1000_release_swflag_ich8lan");

	extcnf_ctrl = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);
	extcnf_ctrl &= ~E1000_EXTCNF_CTRL_SWFLAG;
	E1000_WRITE_REG(hw, E1000_EXTCNF_CTRL, extcnf_ctrl);

	E1000_MUTEX_UNLOCK(&hw->dev_spec.ich8lan.swflag_mutex);

	return;
}

/**
 *  e1000_check_mng_mode_ich8lan - Checks management mode
 *  @hw: pointer to the HW structure
 *
 *  This checks if the adapter has any manageability enabled.
 *  This is a function pointer entry point only called by read/write
 *  routines for the PHY and NVM parts.
 **/
static bool e1000_check_mng_mode_ich8lan(struct e1000_hw *hw)
{
	u32 fwsm;

	DEBUGFUNC("e1000_check_mng_mode_ich8lan");

	fwsm = E1000_READ_REG(hw, E1000_FWSM);

	return (fwsm & E1000_ICH_FWSM_FW_VALID) &&
	       ((fwsm & E1000_FWSM_MODE_MASK) ==
		(E1000_ICH_MNG_IAMT_MODE << E1000_FWSM_MODE_SHIFT));
}

/**
 *  e1000_check_mng_mode_pchlan - Checks management mode
 *  @hw: pointer to the HW structure
 *
 *  This checks if the adapter has iAMT enabled.
 *  This is a function pointer entry point only called by read/write
 *  routines for the PHY and NVM parts.
 **/
static bool e1000_check_mng_mode_pchlan(struct e1000_hw *hw)
{
	u32 fwsm;

	DEBUGFUNC("e1000_check_mng_mode_pchlan");

	fwsm = E1000_READ_REG(hw, E1000_FWSM);

	return (fwsm & E1000_ICH_FWSM_FW_VALID) &&
	       (fwsm & (E1000_ICH_MNG_IAMT_MODE << E1000_FWSM_MODE_SHIFT));
}

/**
 *  e1000_rar_set_pch2lan - Set receive address register
 *  @hw: pointer to the HW structure
 *  @addr: pointer to the receive address
 *  @index: receive address array register
 *
 *  Sets the receive address array register at index to the address passed
 *  in by addr.  For 82579, RAR[0] is the base address register that is to
 *  contain the MAC address but RAR[1-6] are reserved for manageability (ME).
 *  Use SHRA[0-3] in place of those reserved for ME.
 **/
static void e1000_rar_set_pch2lan(struct e1000_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	DEBUGFUNC("e1000_rar_set_pch2lan");

	/*
	 * HW expects these in little endian so we reverse the byte order
	 * from network order (big endian) to little endian
	 */
	rar_low = ((u32) addr[0] |
	           ((u32) addr[1] << 8) |
	           ((u32) addr[2] << 16) | ((u32) addr[3] << 24));

	rar_high = ((u32) addr[4] | ((u32) addr[5] << 8));

	/* If MAC address zero, no need to set the AV bit */
	if (rar_low || rar_high)
		rar_high |= E1000_RAH_AV;

	if (index == 0) {
		E1000_WRITE_REG(hw, E1000_RAL(index), rar_low);
		E1000_WRITE_FLUSH(hw);
		E1000_WRITE_REG(hw, E1000_RAH(index), rar_high);
		E1000_WRITE_FLUSH(hw);
		return;
	}

	if (index < hw->mac.rar_entry_count) {
		E1000_WRITE_REG(hw, E1000_SHRAL(index - 1), rar_low);
		E1000_WRITE_FLUSH(hw);
		E1000_WRITE_REG(hw, E1000_SHRAH(index - 1), rar_high);
		E1000_WRITE_FLUSH(hw);

		/* verify the register updates */
		if ((E1000_READ_REG(hw, E1000_SHRAL(index - 1)) == rar_low) &&
		    (E1000_READ_REG(hw, E1000_SHRAH(index - 1)) == rar_high))
			return;

		DEBUGOUT2("SHRA[%d] might be locked by ME - FWSM=0x%8.8x\n",
			 (index - 1), E1000_READ_REG(hw, E1000_FWSM));
	}

	DEBUGOUT1("Failed to write receive address at index %d\n", index);
}

/**
 *  e1000_check_reset_block_ich8lan - Check if PHY reset is blocked
 *  @hw: pointer to the HW structure
 *
 *  Checks if firmware is blocking the reset of the PHY.
 *  This is a function pointer entry point only called by
 *  reset routines.
 **/
static s32 e1000_check_reset_block_ich8lan(struct e1000_hw *hw)
{
	u32 fwsm;

	DEBUGFUNC("e1000_check_reset_block_ich8lan");

	if (hw->phy.reset_disable)
		return E1000_BLK_PHY_RESET;

	fwsm = E1000_READ_REG(hw, E1000_FWSM);

	return (fwsm & E1000_ICH_FWSM_RSPCIPHY) ? E1000_SUCCESS
	                                        : E1000_BLK_PHY_RESET;
}

/**
 *  e1000_write_smbus_addr - Write SMBus address to PHY needed during Sx states
 *  @hw: pointer to the HW structure
 *
 *  Assumes semaphore already acquired.
 *
 **/
static s32 e1000_write_smbus_addr(struct e1000_hw *hw)
{
	u16 phy_data;
	u32 strap = E1000_READ_REG(hw, E1000_STRAP);
	s32 ret_val = E1000_SUCCESS;

	strap &= E1000_STRAP_SMBUS_ADDRESS_MASK;

	ret_val = e1000_read_phy_reg_hv_locked(hw, HV_SMB_ADDR, &phy_data);
	if (ret_val)
		goto out;

	phy_data &= ~HV_SMB_ADDR_MASK;
	phy_data |= (strap >> E1000_STRAP_SMBUS_ADDRESS_SHIFT);
	phy_data |= HV_SMB_ADDR_PEC_EN | HV_SMB_ADDR_VALID;
	ret_val = e1000_write_phy_reg_hv_locked(hw, HV_SMB_ADDR, phy_data);

out:
	return ret_val;
}

/**
 *  e1000_sw_lcd_config_ich8lan - SW-based LCD Configuration
 *  @hw:   pointer to the HW structure
 *
 *  SW should configure the LCD from the NVM extended configuration region
 *  as a workaround for certain parts.
 **/
static s32 e1000_sw_lcd_config_ich8lan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, data, cnf_size, cnf_base_addr, sw_cfg_mask;
	s32 ret_val = E1000_SUCCESS;
	u16 word_addr, reg_data, reg_addr, phy_page = 0;

	DEBUGFUNC("e1000_sw_lcd_config_ich8lan");

	/*
	 * Initialize the PHY from the NVM on ICH platforms.  This
	 * is needed due to an issue where the NVM configuration is
	 * not properly autoloaded after power transitions.
	 * Therefore, after each PHY reset, we will load the
	 * configuration data out of the NVM manually.
	 */
	switch (hw->mac.type) {
	case e1000_ich8lan:
		if (phy->type != e1000_phy_igp_3)
			return ret_val;

		if ((hw->device_id == E1000_DEV_ID_ICH8_IGP_AMT) ||
		    (hw->device_id == E1000_DEV_ID_ICH8_IGP_C)) {
			sw_cfg_mask = E1000_FEXTNVM_SW_CONFIG;
			break;
		}
		/* Fall-thru */
	case e1000_pchlan:
	case e1000_pch2lan:
		sw_cfg_mask = E1000_FEXTNVM_SW_CONFIG_ICH8M;
		break;
	default:
		return ret_val;
	}

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	data = E1000_READ_REG(hw, E1000_FEXTNVM);
	if (!(data & sw_cfg_mask))
		goto out;

	/*
	 * Make sure HW does not configure LCD from PHY
	 * extended configuration before SW configuration
	 */
	data = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);
	if (!(hw->mac.type == e1000_pch2lan)) {
		if (data & E1000_EXTCNF_CTRL_LCD_WRITE_ENABLE)
			goto out;
	}

	cnf_size = E1000_READ_REG(hw, E1000_EXTCNF_SIZE);
	cnf_size &= E1000_EXTCNF_SIZE_EXT_PCIE_LENGTH_MASK;
	cnf_size >>= E1000_EXTCNF_SIZE_EXT_PCIE_LENGTH_SHIFT;
	if (!cnf_size)
		goto out;

	cnf_base_addr = data & E1000_EXTCNF_CTRL_EXT_CNF_POINTER_MASK;
	cnf_base_addr >>= E1000_EXTCNF_CTRL_EXT_CNF_POINTER_SHIFT;

	if ((!(data & E1000_EXTCNF_CTRL_OEM_WRITE_ENABLE) &&
	    (hw->mac.type == e1000_pchlan)) ||
	     (hw->mac.type == e1000_pch2lan)) {
		/*
		 * HW configures the SMBus address and LEDs when the
		 * OEM and LCD Write Enable bits are set in the NVM.
		 * When both NVM bits are cleared, SW will configure
		 * them instead.
		 */
		ret_val = e1000_write_smbus_addr(hw);
		if (ret_val)
			goto out;

		data = E1000_READ_REG(hw, E1000_LEDCTL);
		ret_val = e1000_write_phy_reg_hv_locked(hw, HV_LED_CONFIG,
							(u16)data);
		if (ret_val)
			goto out;
	}

	/* Configure LCD from extended configuration region. */

	/* cnf_base_addr is in DWORD */
	word_addr = (u16)(cnf_base_addr << 1);

	for (i = 0; i < cnf_size; i++) {
		ret_val = hw->nvm.ops.read(hw, (word_addr + i * 2), 1,
					   &reg_data);
		if (ret_val)
			goto out;

		ret_val = hw->nvm.ops.read(hw, (word_addr + i * 2 + 1),
					   1, &reg_addr);
		if (ret_val)
			goto out;

		/* Save off the PHY page for future writes. */
		if (reg_addr == IGP01E1000_PHY_PAGE_SELECT) {
			phy_page = reg_data;
			continue;
		}

		reg_addr &= PHY_REG_MASK;
		reg_addr |= phy_page;

		ret_val = phy->ops.write_reg_locked(hw, (u32)reg_addr,
						    reg_data);
		if (ret_val)
			goto out;
	}

out:
	hw->phy.ops.release(hw);
	return ret_val;
}

/**
 *  e1000_k1_gig_workaround_hv - K1 Si workaround
 *  @hw:   pointer to the HW structure
 *  @link: link up bool flag
 *
 *  If K1 is enabled for 1Gbps, the MAC might stall when transitioning
 *  from a lower speed.  This workaround disables K1 whenever link is at 1Gig
 *  If link is down, the function will restore the default K1 setting located
 *  in the NVM.
 **/
static s32 e1000_k1_gig_workaround_hv(struct e1000_hw *hw, bool link)
{
	s32 ret_val = E1000_SUCCESS;
	u16 status_reg = 0;
	bool k1_enable = hw->dev_spec.ich8lan.nvm_k1_enabled;

	DEBUGFUNC("e1000_k1_gig_workaround_hv");

	if (hw->mac.type != e1000_pchlan)
		goto out;

	/* Wrap the whole flow with the sw flag */
	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	/* Disable K1 when link is 1Gbps, otherwise use the NVM setting */
	if (link) {
		if (hw->phy.type == e1000_phy_82578) {
			ret_val = hw->phy.ops.read_reg_locked(hw, BM_CS_STATUS,
			                                      &status_reg);
			if (ret_val)
				goto release;

			status_reg &= BM_CS_STATUS_LINK_UP |
			              BM_CS_STATUS_RESOLVED |
			              BM_CS_STATUS_SPEED_MASK;

			if (status_reg == (BM_CS_STATUS_LINK_UP |
			                   BM_CS_STATUS_RESOLVED |
			                   BM_CS_STATUS_SPEED_1000))
				k1_enable = FALSE;
		}

		if (hw->phy.type == e1000_phy_82577) {
			ret_val = hw->phy.ops.read_reg_locked(hw, HV_M_STATUS,
			                                      &status_reg);
			if (ret_val)
				goto release;

			status_reg &= HV_M_STATUS_LINK_UP |
			              HV_M_STATUS_AUTONEG_COMPLETE |
			              HV_M_STATUS_SPEED_MASK;

			if (status_reg == (HV_M_STATUS_LINK_UP |
			                   HV_M_STATUS_AUTONEG_COMPLETE |
			                   HV_M_STATUS_SPEED_1000))
				k1_enable = FALSE;
		}

		/* Link stall fix for link up */
		ret_val = hw->phy.ops.write_reg_locked(hw, PHY_REG(770, 19),
		                                       0x0100);
		if (ret_val)
			goto release;

	} else {
		/* Link stall fix for link down */
		ret_val = hw->phy.ops.write_reg_locked(hw, PHY_REG(770, 19),
		                                       0x4100);
		if (ret_val)
			goto release;
	}

	ret_val = e1000_configure_k1_ich8lan(hw, k1_enable);

release:
	hw->phy.ops.release(hw);
out:
	return ret_val;
}

/**
 *  e1000_configure_k1_ich8lan - Configure K1 power state
 *  @hw: pointer to the HW structure
 *  @enable: K1 state to configure
 *
 *  Configure the K1 power state based on the provided parameter.
 *  Assumes semaphore already acquired.
 *
 *  Success returns 0, Failure returns -E1000_ERR_PHY (-2)
 **/
s32 e1000_configure_k1_ich8lan(struct e1000_hw *hw, bool k1_enable)
{
	s32 ret_val = E1000_SUCCESS;
	u32 ctrl_reg = 0;
	u32 ctrl_ext = 0;
	u32 reg = 0;
	u16 kmrn_reg = 0;

	DEBUGFUNC("e1000_configure_k1_ich8lan");

	ret_val = e1000_read_kmrn_reg_locked(hw,
	                                     E1000_KMRNCTRLSTA_K1_CONFIG,
	                                     &kmrn_reg);
	if (ret_val)
		goto out;

	if (k1_enable)
		kmrn_reg |= E1000_KMRNCTRLSTA_K1_ENABLE;
	else
		kmrn_reg &= ~E1000_KMRNCTRLSTA_K1_ENABLE;

	ret_val = e1000_write_kmrn_reg_locked(hw,
	                                      E1000_KMRNCTRLSTA_K1_CONFIG,
	                                      kmrn_reg);
	if (ret_val)
		goto out;

	usec_delay(20);
	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
	ctrl_reg = E1000_READ_REG(hw, E1000_CTRL);

	reg = ctrl_reg & ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
	reg |= E1000_CTRL_FRCSPD;
	E1000_WRITE_REG(hw, E1000_CTRL, reg);

	E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext | E1000_CTRL_EXT_SPD_BYPS);
	usec_delay(20);
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl_reg);
	E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext);
	usec_delay(20);

out:
	return ret_val;
}

/**
 *  e1000_oem_bits_config_ich8lan - SW-based LCD Configuration
 *  @hw:       pointer to the HW structure
 *  @d0_state: boolean if entering d0 or d3 device state
 *
 *  SW will configure Gbe Disable and LPLU based on the NVM. The four bits are
 *  collectively called OEM bits.  The OEM Write Enable bit and SW Config bit
 *  in NVM determines whether HW should configure LPLU and Gbe Disable.
 **/
s32 e1000_oem_bits_config_ich8lan(struct e1000_hw *hw, bool d0_state)
{
	s32 ret_val = 0;
	u32 mac_reg;
	u16 oem_reg;

	DEBUGFUNC("e1000_oem_bits_config_ich8lan");

	if ((hw->mac.type != e1000_pch2lan) && (hw->mac.type != e1000_pchlan))
		return ret_val;

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	if (!(hw->mac.type == e1000_pch2lan)) {
		mac_reg = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);
		if (mac_reg & E1000_EXTCNF_CTRL_OEM_WRITE_ENABLE)
			goto out;
	}

	mac_reg = E1000_READ_REG(hw, E1000_FEXTNVM);
	if (!(mac_reg & E1000_FEXTNVM_SW_CONFIG_ICH8M))
		goto out;

	mac_reg = E1000_READ_REG(hw, E1000_PHY_CTRL);

	ret_val = hw->phy.ops.read_reg_locked(hw, HV_OEM_BITS, &oem_reg);
	if (ret_val)
		goto out;

	oem_reg &= ~(HV_OEM_BITS_GBE_DIS | HV_OEM_BITS_LPLU);

	if (d0_state) {
		if (mac_reg & E1000_PHY_CTRL_GBE_DISABLE)
			oem_reg |= HV_OEM_BITS_GBE_DIS;

		if (mac_reg & E1000_PHY_CTRL_D0A_LPLU)
			oem_reg |= HV_OEM_BITS_LPLU;
	} else {
		if (mac_reg & E1000_PHY_CTRL_NOND0A_GBE_DISABLE)
			oem_reg |= HV_OEM_BITS_GBE_DIS;

		if (mac_reg & E1000_PHY_CTRL_NOND0A_LPLU)
			oem_reg |= HV_OEM_BITS_LPLU;
	}
	/* Restart auto-neg to activate the bits */
	if (!hw->phy.ops.check_reset_block(hw))
		oem_reg |= HV_OEM_BITS_RESTART_AN;
	ret_val = hw->phy.ops.write_reg_locked(hw, HV_OEM_BITS, oem_reg);

out:
	hw->phy.ops.release(hw);

	return ret_val;
}


/**
 *  e1000_hv_phy_powerdown_workaround_ich8lan - Power down workaround on Sx
 *  @hw: pointer to the HW structure
 **/
s32 e1000_hv_phy_powerdown_workaround_ich8lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_hv_phy_powerdown_workaround_ich8lan");

	if ((hw->phy.type != e1000_phy_82577) || (hw->revision_id > 2))
		return E1000_SUCCESS;

	return hw->phy.ops.write_reg(hw, PHY_REG(768, 25), 0x0444);
}

/**
 *  e1000_set_mdio_slow_mode_hv - Set slow MDIO access mode
 *  @hw:   pointer to the HW structure
 **/
static s32 e1000_set_mdio_slow_mode_hv(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 data;

	DEBUGFUNC("e1000_set_mdio_slow_mode_hv");

	ret_val = hw->phy.ops.read_reg(hw, HV_KMRN_MODE_CTRL, &data);
	if (ret_val)
		return ret_val;

	data |= HV_KMRN_MDIO_SLOW;

	ret_val = hw->phy.ops.write_reg(hw, HV_KMRN_MODE_CTRL, data);

	return ret_val;
}

/**
 *  e1000_hv_phy_workarounds_ich8lan - A series of Phy workarounds to be
 *  done after every PHY reset.
 **/
static s32 e1000_hv_phy_workarounds_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 phy_data;

	DEBUGFUNC("e1000_hv_phy_workarounds_ich8lan");

	if (hw->mac.type != e1000_pchlan)
		goto out;

	/* Set MDIO slow mode before any other MDIO access */
	if (hw->phy.type == e1000_phy_82577) {
		ret_val = e1000_set_mdio_slow_mode_hv(hw);
		if (ret_val)
			goto out;
	}

	/* Hanksville M Phy init for IEEE. */
	if ((hw->revision_id == 2) &&
	    (hw->phy.type == e1000_phy_82577) &&
	    ((hw->phy.revision == 2) || (hw->phy.revision == 3))) {
		hw->phy.ops.write_reg(hw, 0x10, 0x8823);
		hw->phy.ops.write_reg(hw, 0x11, 0x0018);
		hw->phy.ops.write_reg(hw, 0x10, 0x8824);
		hw->phy.ops.write_reg(hw, 0x11, 0x0016);
		hw->phy.ops.write_reg(hw, 0x10, 0x8825);
		hw->phy.ops.write_reg(hw, 0x11, 0x001A);
		hw->phy.ops.write_reg(hw, 0x10, 0x888C);
		hw->phy.ops.write_reg(hw, 0x11, 0x0007);
		hw->phy.ops.write_reg(hw, 0x10, 0x888D);
		hw->phy.ops.write_reg(hw, 0x11, 0x0007);
		hw->phy.ops.write_reg(hw, 0x10, 0x888E);
		hw->phy.ops.write_reg(hw, 0x11, 0x0007);
		hw->phy.ops.write_reg(hw, 0x10, 0x8827);
		hw->phy.ops.write_reg(hw, 0x11, 0x0001);
		hw->phy.ops.write_reg(hw, 0x10, 0x8835);
		hw->phy.ops.write_reg(hw, 0x11, 0x0001);
		hw->phy.ops.write_reg(hw, 0x10, 0x8834);
		hw->phy.ops.write_reg(hw, 0x11, 0x0001);
		hw->phy.ops.write_reg(hw, 0x10, 0x8833);
		hw->phy.ops.write_reg(hw, 0x11, 0x0002);
	}

	if (((hw->phy.type == e1000_phy_82577) &&
	     ((hw->phy.revision == 1) || (hw->phy.revision == 2))) ||
	    ((hw->phy.type == e1000_phy_82578) && (hw->phy.revision == 1))) {
		/* Disable generation of early preamble */
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 25), 0x4431);
		if (ret_val)
			goto out;

		/* Preamble tuning for SSC */
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(770, 16), 0xA204);
		if (ret_val)
			goto out;
	}

	if (hw->phy.type == e1000_phy_82578) {
		if (hw->revision_id < 3) {
			/* PHY config */
			ret_val = hw->phy.ops.write_reg(hw, (1 << 6) | 0x29,
			                                0x66C0);
			if (ret_val)
				goto out;

			/* PHY config */
			ret_val = hw->phy.ops.write_reg(hw, (1 << 6) | 0x1E,
			                                0xFFFF);
			if (ret_val)
				goto out;
		}

		/*
		 * Return registers to default by doing a soft reset then
		 * writing 0x3140 to the control register.
		 */
		if (hw->phy.revision < 2) {
			e1000_phy_sw_reset_generic(hw);
			ret_val = hw->phy.ops.write_reg(hw, PHY_CONTROL,
			                                0x3140);
		}
	}

	if ((hw->revision_id == 2) &&
	    (hw->phy.type == e1000_phy_82577) &&
	    ((hw->phy.revision == 2) || (hw->phy.revision == 3))) {
		/*
		 * Workaround for OEM (GbE) not operating after reset -
		 * restart AN (twice)
		 */
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(768, 25), 0x0400);
		if (ret_val)
			goto out;
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(768, 25), 0x0400);
		if (ret_val)
			goto out;
	}

	/* Select page 0 */
	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	hw->phy.addr = 1;
	ret_val = e1000_write_phy_reg_mdic(hw, IGP01E1000_PHY_PAGE_SELECT, 0);
	hw->phy.ops.release(hw);
	if (ret_val)
		goto out;

	/*
	 * Configure the K1 Si workaround during phy reset assuming there is
	 * link so that it disables K1 if link is in 1Gbps.
	 */
	ret_val = e1000_k1_gig_workaround_hv(hw, TRUE);
	if (ret_val)
		goto out;

	/* Workaround for link disconnects on a busy hub in half duplex */
	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;
	ret_val = hw->phy.ops.read_reg_locked(hw,
	                                      PHY_REG(BM_PORT_CTRL_PAGE, 17),
	                                      &phy_data);
	if (ret_val)
		goto release;
	ret_val = hw->phy.ops.write_reg_locked(hw,
	                                       PHY_REG(BM_PORT_CTRL_PAGE, 17),
	                                       phy_data & 0x00FF);
release:
	hw->phy.ops.release(hw);
out:
	return ret_val;
}

/**
 *  e1000_copy_rx_addrs_to_phy_ich8lan - Copy Rx addresses from MAC to PHY
 *  @hw:   pointer to the HW structure
 **/
void e1000_copy_rx_addrs_to_phy_ich8lan(struct e1000_hw *hw)
{
	u32 mac_reg;
	u16 i;

	DEBUGFUNC("e1000_copy_rx_addrs_to_phy_ich8lan");

	/* Copy both RAL/H (rar_entry_count) and SHRAL/H (+4) to PHY */
	for (i = 0; i < (hw->mac.rar_entry_count + 4); i++) {
		mac_reg = E1000_READ_REG(hw, E1000_RAL(i));
		hw->phy.ops.write_reg(hw, BM_RAR_L(i), (u16)(mac_reg & 0xFFFF));
		hw->phy.ops.write_reg(hw, BM_RAR_M(i), (u16)((mac_reg >> 16) & 0xFFFF));
		mac_reg = E1000_READ_REG(hw, E1000_RAH(i));
		hw->phy.ops.write_reg(hw, BM_RAR_H(i), (u16)(mac_reg & 0xFFFF));
		hw->phy.ops.write_reg(hw, BM_RAR_CTRL(i), (u16)((mac_reg >> 16) & 0x8000));
	}
}

static u32 e1000_calc_rx_da_crc(u8 mac[])
{
	u32 poly = 0xEDB88320;	/* Polynomial for 802.3 CRC calculation */
	u32 i, j, mask, crc;

	DEBUGFUNC("e1000_calc_rx_da_crc");

	crc = 0xffffffff;
	for (i = 0; i < 6; i++) {
		crc = crc ^ mac[i];
		for (j = 8; j > 0; j--) {
			mask = (crc & 1) * (-1);
			crc = (crc >> 1) ^ (poly & mask);
		}
	}
	return ~crc;
}

/**
 *  e1000_lv_jumbo_workaround_ich8lan - required for jumbo frame operation
 *  with 82579 PHY
 *  @hw: pointer to the HW structure
 *  @enable: flag to enable/disable workaround when enabling/disabling jumbos
 **/
s32 e1000_lv_jumbo_workaround_ich8lan(struct e1000_hw *hw, bool enable)
{
	s32 ret_val = E1000_SUCCESS;
	u16 phy_reg, data;
	u32 mac_reg;
	u16 i;

	DEBUGFUNC("e1000_lv_jumbo_workaround_ich8lan");

	if (hw->mac.type != e1000_pch2lan)
		goto out;

	/* disable Rx path while enabling/disabling workaround */
	hw->phy.ops.read_reg(hw, PHY_REG(769, 20), &phy_reg);
	ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 20), phy_reg | (1 << 14));
	if (ret_val)
		goto out;

	if (enable) {
		/*
		 * Write Rx addresses (rar_entry_count for RAL/H, +4 for
		 * SHRAL/H) and initial CRC values to the MAC
		 */
		for (i = 0; i < (hw->mac.rar_entry_count + 4); i++) {
			u8 mac_addr[ETH_ADDR_LEN] = {0};
			u32 addr_high, addr_low;

			addr_high = E1000_READ_REG(hw, E1000_RAH(i));
			if (!(addr_high & E1000_RAH_AV))
				continue;
			addr_low = E1000_READ_REG(hw, E1000_RAL(i));
			mac_addr[0] = (addr_low & 0xFF);
			mac_addr[1] = ((addr_low >> 8) & 0xFF);
			mac_addr[2] = ((addr_low >> 16) & 0xFF);
			mac_addr[3] = ((addr_low >> 24) & 0xFF);
			mac_addr[4] = (addr_high & 0xFF);
			mac_addr[5] = ((addr_high >> 8) & 0xFF);

			E1000_WRITE_REG(hw, E1000_PCH_RAICC(i),
					e1000_calc_rx_da_crc(mac_addr));
		}

		/* Write Rx addresses to the PHY */
		e1000_copy_rx_addrs_to_phy_ich8lan(hw);

		/* Enable jumbo frame workaround in the MAC */
		mac_reg = E1000_READ_REG(hw, E1000_FFLT_DBG);
		mac_reg &= ~(1 << 14);
		mac_reg |= (7 << 15);
		E1000_WRITE_REG(hw, E1000_FFLT_DBG, mac_reg);

		mac_reg = E1000_READ_REG(hw, E1000_RCTL);
		mac_reg |= E1000_RCTL_SECRC;
		E1000_WRITE_REG(hw, E1000_RCTL, mac_reg);

		ret_val = e1000_read_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_CTRL_OFFSET,
						&data);
		if (ret_val)
			goto out;
		ret_val = e1000_write_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_CTRL_OFFSET,
						data | (1 << 0));
		if (ret_val)
			goto out;
		ret_val = e1000_read_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_HD_CTRL,
						&data);
		if (ret_val)
			goto out;
		data &= ~(0xF << 8);
		data |= (0xB << 8);
		ret_val = e1000_write_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_HD_CTRL,
						data);
		if (ret_val)
			goto out;

		/* Enable jumbo frame workaround in the PHY */
		hw->phy.ops.read_reg(hw, PHY_REG(769, 23), &data);
		data &= ~(0x7F << 5);
		data |= (0x37 << 5);
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 23), data);
		if (ret_val)
			goto out;
		hw->phy.ops.read_reg(hw, PHY_REG(769, 16), &data);
		data &= ~(1 << 13);
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 16), data);
		if (ret_val)
			goto out;
		hw->phy.ops.read_reg(hw, PHY_REG(776, 20), &data);
		data &= ~(0x3FF << 2);
		data |= (0x1A << 2);
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(776, 20), data);
		if (ret_val)
			goto out;
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(776, 23), 0xFE00);
		if (ret_val)
			goto out;
		hw->phy.ops.read_reg(hw, HV_PM_CTRL, &data);
		ret_val = hw->phy.ops.write_reg(hw, HV_PM_CTRL, data | (1 << 10));
		if (ret_val)
			goto out;
	} else {
		/* Write MAC register values back to h/w defaults */
		mac_reg = E1000_READ_REG(hw, E1000_FFLT_DBG);
		mac_reg &= ~(0xF << 14);
		E1000_WRITE_REG(hw, E1000_FFLT_DBG, mac_reg);

		mac_reg = E1000_READ_REG(hw, E1000_RCTL);
		mac_reg &= ~E1000_RCTL_SECRC;
		E1000_WRITE_REG(hw, E1000_RCTL, mac_reg);

		ret_val = e1000_read_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_CTRL_OFFSET,
						&data);
		if (ret_val)
			goto out;
		ret_val = e1000_write_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_CTRL_OFFSET,
						data & ~(1 << 0));
		if (ret_val)
			goto out;
		ret_val = e1000_read_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_HD_CTRL,
						&data);
		if (ret_val)
			goto out;
		data &= ~(0xF << 8);
		data |= (0xB << 8);
		ret_val = e1000_write_kmrn_reg_generic(hw,
						E1000_KMRNCTRLSTA_HD_CTRL,
						data);
		if (ret_val)
			goto out;

		/* Write PHY register values back to h/w defaults */
		hw->phy.ops.read_reg(hw, PHY_REG(769, 23), &data);
		data &= ~(0x7F << 5);
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 23), data);
		if (ret_val)
			goto out;
		hw->phy.ops.read_reg(hw, PHY_REG(769, 16), &data);
		data |= (1 << 13);
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 16), data);
		if (ret_val)
			goto out;
		hw->phy.ops.read_reg(hw, PHY_REG(776, 20), &data);
		data &= ~(0x3FF << 2);
		data |= (0x8 << 2);
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(776, 20), data);
		if (ret_val)
			goto out;
		ret_val = hw->phy.ops.write_reg(hw, PHY_REG(776, 23), 0x7E00);
		if (ret_val)
			goto out;
		hw->phy.ops.read_reg(hw, HV_PM_CTRL, &data);
		ret_val = hw->phy.ops.write_reg(hw, HV_PM_CTRL, data & ~(1 << 10));
		if (ret_val)
			goto out;
	}

	/* re-enable Rx path after enabling/disabling workaround */
	ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 20), phy_reg & ~(1 << 14));

out:
	return ret_val;
}

/**
 *  e1000_lv_phy_workarounds_ich8lan - A series of Phy workarounds to be
 *  done after every PHY reset.
 **/
static s32 e1000_lv_phy_workarounds_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_lv_phy_workarounds_ich8lan");

	if (hw->mac.type != e1000_pch2lan)
		goto out;

	/* Set MDIO slow mode before any other MDIO access */
	ret_val = e1000_set_mdio_slow_mode_hv(hw);

out:
	return ret_val;
}

/**
 *  e1000_k1_gig_workaround_lv - K1 Si workaround
 *  @hw:   pointer to the HW structure
 *
 *  Workaround to set the K1 beacon duration for 82579 parts
 **/
static s32 e1000_k1_workaround_lv(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 status_reg = 0;
	u32 mac_reg;

	DEBUGFUNC("e1000_k1_workaround_lv");

	if (hw->mac.type != e1000_pch2lan)
		goto out;

	/* Set K1 beacon duration based on 1Gbps speed or otherwise */
	ret_val = hw->phy.ops.read_reg(hw, HV_M_STATUS, &status_reg);
	if (ret_val)
		goto out;

	if ((status_reg & (HV_M_STATUS_LINK_UP | HV_M_STATUS_AUTONEG_COMPLETE))
	    == (HV_M_STATUS_LINK_UP | HV_M_STATUS_AUTONEG_COMPLETE)) {
		mac_reg = E1000_READ_REG(hw, E1000_FEXTNVM4);
		mac_reg &= ~E1000_FEXTNVM4_BEACON_DURATION_MASK;

		if (status_reg & HV_M_STATUS_SPEED_1000)
			mac_reg |= E1000_FEXTNVM4_BEACON_DURATION_8USEC;
		else
			mac_reg |= E1000_FEXTNVM4_BEACON_DURATION_16USEC;

		E1000_WRITE_REG(hw, E1000_FEXTNVM4, mac_reg);
	}

out:
	return ret_val;
}

/**
 *  e1000_gate_hw_phy_config_ich8lan - disable PHY config via hardware
 *  @hw:   pointer to the HW structure
 *  @gate: boolean set to TRUE to gate, FALSE to un-gate
 *
 *  Gate/ungate the automatic PHY configuration via hardware; perform
 *  the configuration via software instead.
 **/
static void e1000_gate_hw_phy_config_ich8lan(struct e1000_hw *hw, bool gate)
{
	u32 extcnf_ctrl;

	DEBUGFUNC("e1000_gate_hw_phy_config_ich8lan");

	if (hw->mac.type != e1000_pch2lan)
		return;

	extcnf_ctrl = E1000_READ_REG(hw, E1000_EXTCNF_CTRL);

	if (gate)
		extcnf_ctrl |= E1000_EXTCNF_CTRL_GATE_PHY_CFG;
	else
		extcnf_ctrl &= ~E1000_EXTCNF_CTRL_GATE_PHY_CFG;

	E1000_WRITE_REG(hw, E1000_EXTCNF_CTRL, extcnf_ctrl);
	return;
}

/**
 *  e1000_hv_phy_tuning_workaround_ich8lan - This is a Phy tuning work around
 *  needed for Nahum3 + Hanksville testing, requested by HW team
 **/
static s32 e1000_hv_phy_tuning_workaround_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_hv_phy_tuning_workaround_ich8lan");

	ret_val = hw->phy.ops.write_reg(hw, PHY_REG(769, 25), 0x4431);
	if (ret_val)
		goto out;

	ret_val = hw->phy.ops.write_reg(hw, PHY_REG(770, 16), 0xA204);
	if (ret_val)
		goto out;

	ret_val = hw->phy.ops.write_reg(hw, (1 << 6) | 0x29, 0x66C0);
	if (ret_val)
		goto out;

	ret_val = hw->phy.ops.write_reg(hw, (1 << 6) | 0x1E, 0xFFFF);

out:
	return ret_val;
}

/**
 *  e1000_lan_init_done_ich8lan - Check for PHY config completion
 *  @hw: pointer to the HW structure
 *
 *  Check the appropriate indication the MAC has finished configuring the
 *  PHY after a software reset.
 **/
static void e1000_lan_init_done_ich8lan(struct e1000_hw *hw)
{
	u32 data, loop = E1000_ICH8_LAN_INIT_TIMEOUT;

	DEBUGFUNC("e1000_lan_init_done_ich8lan");

	/* Wait for basic configuration completes before proceeding */
	do {
		data = E1000_READ_REG(hw, E1000_STATUS);
		data &= E1000_STATUS_LAN_INIT_DONE;
		usec_delay(100);
	} while ((!data) && --loop);

	/*
	 * If basic configuration is incomplete before the above loop
	 * count reaches 0, loading the configuration from NVM will
	 * leave the PHY in a bad state possibly resulting in no link.
	 */
	if (loop == 0)
		DEBUGOUT("LAN_INIT_DONE not set, increase timeout\n");

	/* Clear the Init Done bit for the next init event */
	data = E1000_READ_REG(hw, E1000_STATUS);
	data &= ~E1000_STATUS_LAN_INIT_DONE;
	E1000_WRITE_REG(hw, E1000_STATUS, data);
}

/**
 *  e1000_post_phy_reset_ich8lan - Perform steps required after a PHY reset
 *  @hw: pointer to the HW structure
 **/
static s32 e1000_post_phy_reset_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 reg;

	DEBUGFUNC("e1000_post_phy_reset_ich8lan");

	if (hw->phy.ops.check_reset_block(hw))
		goto out;

	/* Allow time for h/w to get to quiescent state after reset */
	msec_delay(10);

	/* Perform any necessary post-reset workarounds */
	switch (hw->mac.type) {
	case e1000_pchlan:
		ret_val = e1000_hv_phy_workarounds_ich8lan(hw);
		if (ret_val)
			goto out;
		break;
	case e1000_pch2lan:
		ret_val = e1000_lv_phy_workarounds_ich8lan(hw);
		if (ret_val)
			goto out;
		break;
	default:
		break;
	}

	if (hw->device_id == E1000_DEV_ID_ICH10_HANKSVILLE) {
		ret_val = e1000_hv_phy_tuning_workaround_ich8lan(hw);
		if (ret_val)
			goto out;
	}

	/* Dummy read to clear the phy wakeup bit after lcd reset */
	if (hw->mac.type >= e1000_pchlan)
		hw->phy.ops.read_reg(hw, BM_WUC, &reg);

	/* Configure the LCD with the extended configuration region in NVM */
	ret_val = e1000_sw_lcd_config_ich8lan(hw);
	if (ret_val)
		goto out;

	/* Configure the LCD with the OEM bits in NVM */
	ret_val = e1000_oem_bits_config_ich8lan(hw, TRUE);

	/* Ungate automatic PHY configuration on non-managed 82579 */
	if ((hw->mac.type == e1000_pch2lan) &&
	    !(E1000_READ_REG(hw, E1000_FWSM) & E1000_ICH_FWSM_FW_VALID)) {
		msec_delay(10);
		e1000_gate_hw_phy_config_ich8lan(hw, FALSE);
	}

out:
	return ret_val;
}

/**
 *  e1000_phy_hw_reset_ich8lan - Performs a PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Resets the PHY
 *  This is a function pointer entry point called by drivers
 *  or other shared routines.
 **/
static s32 e1000_phy_hw_reset_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_phy_hw_reset_ich8lan");

	/* Gate automatic PHY configuration by hardware on non-managed 82579 */
	if ((hw->mac.type == e1000_pch2lan) &&
	    !(E1000_READ_REG(hw, E1000_FWSM) & E1000_ICH_FWSM_FW_VALID))
		e1000_gate_hw_phy_config_ich8lan(hw, TRUE);

	ret_val = e1000_phy_hw_reset_generic(hw);
	if (ret_val)
		goto out;

	ret_val = e1000_post_phy_reset_ich8lan(hw);

out:
	return ret_val;
}

/**
 *  e1000_set_lplu_state_pchlan - Set Low Power Link Up state
 *  @hw: pointer to the HW structure
 *  @active: TRUE to enable LPLU, FALSE to disable
 *
 *  Sets the LPLU state according to the active flag.  For PCH, if OEM write
 *  bit are disabled in the NVM, writing the LPLU bits in the MAC will not set
 *  the phy speed. This function will manually set the LPLU bit and restart
 *  auto-neg as hw would do. D3 and D0 LPLU will call the same function
 *  since it configures the same bit.
 **/
static s32 e1000_set_lplu_state_pchlan(struct e1000_hw *hw, bool active)
{
	s32 ret_val = E1000_SUCCESS;
	u16 oem_reg;

	DEBUGFUNC("e1000_set_lplu_state_pchlan");

	ret_val = hw->phy.ops.read_reg(hw, HV_OEM_BITS, &oem_reg);
	if (ret_val)
		goto out;

	if (active)
		oem_reg |= HV_OEM_BITS_LPLU;
	else
		oem_reg &= ~HV_OEM_BITS_LPLU;

	oem_reg |= HV_OEM_BITS_RESTART_AN;
	ret_val = hw->phy.ops.write_reg(hw, HV_OEM_BITS, oem_reg);

out:
	return ret_val;
}

/**
 *  e1000_set_d0_lplu_state_ich8lan - Set Low Power Linkup D0 state
 *  @hw: pointer to the HW structure
 *  @active: TRUE to enable LPLU, FALSE to disable
 *
 *  Sets the LPLU D0 state according to the active flag.  When
 *  activating LPLU this function also disables smart speed
 *  and vice versa.  LPLU will not be activated unless the
 *  device autonegotiation advertisement meets standards of
 *  either 10 or 10/100 or 10/100/1000 at all duplexes.
 *  This is a function pointer entry point only called by
 *  PHY setup routines.
 **/
static s32 e1000_set_d0_lplu_state_ich8lan(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 phy_ctrl;
	s32 ret_val = E1000_SUCCESS;
	u16 data;

	DEBUGFUNC("e1000_set_d0_lplu_state_ich8lan");

	if (phy->type == e1000_phy_ife)
		goto out;

	phy_ctrl = E1000_READ_REG(hw, E1000_PHY_CTRL);

	if (active) {
		phy_ctrl |= E1000_PHY_CTRL_D0A_LPLU;
		E1000_WRITE_REG(hw, E1000_PHY_CTRL, phy_ctrl);

		if (phy->type != e1000_phy_igp_3)
			goto out;

		/*
		 * Call gig speed drop workaround on LPLU before accessing
		 * any PHY registers
		 */
		if (hw->mac.type == e1000_ich8lan)
			e1000_gig_downshift_workaround_ich8lan(hw);

		/* When LPLU is enabled, we should disable SmartSpeed */
		ret_val = phy->ops.read_reg(hw,
		                            IGP01E1000_PHY_PORT_CONFIG,
		                            &data);
		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = phy->ops.write_reg(hw,
		                             IGP01E1000_PHY_PORT_CONFIG,
		                             data);
		if (ret_val)
			goto out;
	} else {
		phy_ctrl &= ~E1000_PHY_CTRL_D0A_LPLU;
		E1000_WRITE_REG(hw, E1000_PHY_CTRL, phy_ctrl);

		if (phy->type != e1000_phy_igp_3)
			goto out;

		/*
		 * LPLU and SmartSpeed are mutually exclusive.  LPLU is used
		 * during Dx states where the power conservation is most
		 * important.  During driver activity we should enable
		 * SmartSpeed, so performance is maintained.
		 */
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = phy->ops.read_reg(hw,
			                            IGP01E1000_PHY_PORT_CONFIG,
			                            &data);
			if (ret_val)
				goto out;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             data);
			if (ret_val)
				goto out;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = phy->ops.read_reg(hw,
			                            IGP01E1000_PHY_PORT_CONFIG,
			                            &data);
			if (ret_val)
				goto out;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             data);
			if (ret_val)
				goto out;
		}
	}

out:
	return ret_val;
}

/**
 *  e1000_set_d3_lplu_state_ich8lan - Set Low Power Linkup D3 state
 *  @hw: pointer to the HW structure
 *  @active: TRUE to enable LPLU, FALSE to disable
 *
 *  Sets the LPLU D3 state according to the active flag.  When
 *  activating LPLU this function also disables smart speed
 *  and vice versa.  LPLU will not be activated unless the
 *  device autonegotiation advertisement meets standards of
 *  either 10 or 10/100 or 10/100/1000 at all duplexes.
 *  This is a function pointer entry point only called by
 *  PHY setup routines.
 **/
static s32 e1000_set_d3_lplu_state_ich8lan(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 phy_ctrl;
	s32 ret_val = E1000_SUCCESS;
	u16 data;

	DEBUGFUNC("e1000_set_d3_lplu_state_ich8lan");

	phy_ctrl = E1000_READ_REG(hw, E1000_PHY_CTRL);

	if (!active) {
		phy_ctrl &= ~E1000_PHY_CTRL_NOND0A_LPLU;
		E1000_WRITE_REG(hw, E1000_PHY_CTRL, phy_ctrl);

		if (phy->type != e1000_phy_igp_3)
			goto out;

		/*
		 * LPLU and SmartSpeed are mutually exclusive.  LPLU is used
		 * during Dx states where the power conservation is most
		 * important.  During driver activity we should enable
		 * SmartSpeed, so performance is maintained.
		 */
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = phy->ops.read_reg(hw,
			                            IGP01E1000_PHY_PORT_CONFIG,
			                            &data);
			if (ret_val)
				goto out;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             data);
			if (ret_val)
				goto out;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = phy->ops.read_reg(hw,
			                            IGP01E1000_PHY_PORT_CONFIG,
			                            &data);
			if (ret_val)
				goto out;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
			                             IGP01E1000_PHY_PORT_CONFIG,
			                             data);
			if (ret_val)
				goto out;
		}
	} else if ((phy->autoneg_advertised == E1000_ALL_SPEED_DUPLEX) ||
	           (phy->autoneg_advertised == E1000_ALL_NOT_GIG) ||
	           (phy->autoneg_advertised == E1000_ALL_10_SPEED)) {
		phy_ctrl |= E1000_PHY_CTRL_NOND0A_LPLU;
		E1000_WRITE_REG(hw, E1000_PHY_CTRL, phy_ctrl);

		if (phy->type != e1000_phy_igp_3)
			goto out;

		/*
		 * Call gig speed drop workaround on LPLU before accessing
		 * any PHY registers
		 */
		if (hw->mac.type == e1000_ich8lan)
			e1000_gig_downshift_workaround_ich8lan(hw);

		/* When LPLU is enabled, we should disable SmartSpeed */
		ret_val = phy->ops.read_reg(hw,
		                            IGP01E1000_PHY_PORT_CONFIG,
		                            &data);
		if (ret_val)
			goto out;

		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = phy->ops.write_reg(hw,
		                             IGP01E1000_PHY_PORT_CONFIG,
		                             data);
	}

out:
	return ret_val;
}

/**
 *  e1000_valid_nvm_bank_detect_ich8lan - finds out the valid bank 0 or 1
 *  @hw: pointer to the HW structure
 *  @bank:  pointer to the variable that returns the active bank
 *
 *  Reads signature byte from the NVM using the flash access registers.
 *  Word 0x13 bits 15:14 = 10b indicate a valid signature for that bank.
 **/
static s32 e1000_valid_nvm_bank_detect_ich8lan(struct e1000_hw *hw, u32 *bank)
{
	u32 eecd;
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 bank1_offset = nvm->flash_bank_size * sizeof(u16);
	u32 act_offset = E1000_ICH_NVM_SIG_WORD * 2 + 1;
	u8 sig_byte = 0;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_valid_nvm_bank_detect_ich8lan");

	switch (hw->mac.type) {
	case e1000_ich8lan:
	case e1000_ich9lan:
		eecd = E1000_READ_REG(hw, E1000_EECD);
		if ((eecd & E1000_EECD_SEC1VAL_VALID_MASK) ==
		    E1000_EECD_SEC1VAL_VALID_MASK) {
			if (eecd & E1000_EECD_SEC1VAL)
				*bank = 1;
			else
				*bank = 0;

			goto out;
		}
		DEBUGOUT("Unable to determine valid NVM bank via EEC - "
		         "reading flash signature\n");
		/* fall-thru */
	default:
		/* set bank to 0 in case flash read fails */
		*bank = 0;

		/* Check bank 0 */
		ret_val = e1000_read_flash_byte_ich8lan(hw, act_offset,
		                                        &sig_byte);
		if (ret_val)
			goto out;
		if ((sig_byte & E1000_ICH_NVM_VALID_SIG_MASK) ==
		    E1000_ICH_NVM_SIG_VALUE) {
			*bank = 0;
			goto out;
		}

		/* Check bank 1 */
		ret_val = e1000_read_flash_byte_ich8lan(hw, act_offset +
		                                        bank1_offset,
		                                        &sig_byte);
		if (ret_val)
			goto out;
		if ((sig_byte & E1000_ICH_NVM_VALID_SIG_MASK) ==
		    E1000_ICH_NVM_SIG_VALUE) {
			*bank = 1;
			goto out;
		}

		DEBUGOUT("ERROR: No valid NVM bank present\n");
		ret_val = -E1000_ERR_NVM;
		break;
	}
out:
	return ret_val;
}

/**
 *  e1000_read_nvm_ich8lan - Read word(s) from the NVM
 *  @hw: pointer to the HW structure
 *  @offset: The offset (in bytes) of the word(s) to read.
 *  @words: Size of data to read in words
 *  @data: Pointer to the word(s) to read at offset.
 *
 *  Reads a word(s) from the NVM using the flash access registers.
 **/
static s32 e1000_read_nvm_ich8lan(struct e1000_hw *hw, u16 offset, u16 words,
                                  u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_dev_spec_ich8lan *dev_spec = &hw->dev_spec.ich8lan;
	u32 act_offset;
	s32 ret_val = E1000_SUCCESS;
	u32 bank = 0;
	u16 i, word;

	DEBUGFUNC("e1000_read_nvm_ich8lan");

	if ((offset >= nvm->word_size) || (words > nvm->word_size - offset) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	nvm->ops.acquire(hw);

	ret_val = e1000_valid_nvm_bank_detect_ich8lan(hw, &bank);
	if (ret_val != E1000_SUCCESS) {
		DEBUGOUT("Could not detect valid bank, assuming bank 0\n");
		bank = 0;
	}

	act_offset = (bank) ? nvm->flash_bank_size : 0;
	act_offset += offset;

	ret_val = E1000_SUCCESS;
	for (i = 0; i < words; i++) {
		if ((dev_spec->shadow_ram) &&
		    (dev_spec->shadow_ram[offset+i].modified)) {
			data[i] = dev_spec->shadow_ram[offset+i].value;
		} else {
			ret_val = e1000_read_flash_word_ich8lan(hw,
			                                        act_offset + i,
			                                        &word);
			if (ret_val)
				break;
			data[i] = word;
		}
	}

	nvm->ops.release(hw);

out:
	if (ret_val)
		DEBUGOUT1("NVM read error: %d\n", ret_val);

	return ret_val;
}

/**
 *  e1000_flash_cycle_init_ich8lan - Initialize flash
 *  @hw: pointer to the HW structure
 *
 *  This function does initial flash setup so that a new read/write/erase cycle
 *  can be started.
 **/
static s32 e1000_flash_cycle_init_ich8lan(struct e1000_hw *hw)
{
	union ich8_hws_flash_status hsfsts;
	s32 ret_val = -E1000_ERR_NVM;
	s32 i = 0;

	DEBUGFUNC("e1000_flash_cycle_init_ich8lan");

	hsfsts.regval = E1000_READ_FLASH_REG16(hw, ICH_FLASH_HSFSTS);

	/* Check if the flash descriptor is valid */
	if (hsfsts.hsf_status.fldesvalid == 0) {
		DEBUGOUT("Flash descriptor invalid.  "
		         "SW Sequencing must be used.");
		goto out;
	}

	/* Clear FCERR and DAEL in hw status by writing 1 */
	hsfsts.hsf_status.flcerr = 1;
	hsfsts.hsf_status.dael = 1;

	E1000_WRITE_FLASH_REG16(hw, ICH_FLASH_HSFSTS, hsfsts.regval);

	/*
	 * Either we should have a hardware SPI cycle in progress
	 * bit to check against, in order to start a new cycle or
	 * FDONE bit should be changed in the hardware so that it
	 * is 1 after hardware reset, which can then be used as an
	 * indication whether a cycle is in progress or has been
	 * completed.
	 */

	if (hsfsts.hsf_status.flcinprog == 0) {
		/*
		 * There is no cycle running at present,
		 * so we can start a cycle.
		 * Begin by setting Flash Cycle Done.
		 */
		hsfsts.hsf_status.flcdone = 1;
		E1000_WRITE_FLASH_REG16(hw, ICH_FLASH_HSFSTS, hsfsts.regval);
		ret_val = E1000_SUCCESS;
	} else {
		/*
		 * Otherwise poll for sometime so the current
		 * cycle has a chance to end before giving up.
		 */
		for (i = 0; i < ICH_FLASH_READ_COMMAND_TIMEOUT; i++) {
			hsfsts.regval = E1000_READ_FLASH_REG16(hw,
			                                      ICH_FLASH_HSFSTS);
			if (hsfsts.hsf_status.flcinprog == 0) {
				ret_val = E1000_SUCCESS;
				break;
			}
			usec_delay(1);
		}
		if (ret_val == E1000_SUCCESS) {
			/*
			 * Successful in waiting for previous cycle to timeout,
			 * now set the Flash Cycle Done.
			 */
			hsfsts.hsf_status.flcdone = 1;
			E1000_WRITE_FLASH_REG16(hw, ICH_FLASH_HSFSTS,
			                        hsfsts.regval);
		} else {
			DEBUGOUT("Flash controller busy, cannot get access");
		}
	}

out:
	return ret_val;
}

/**
 *  e1000_flash_cycle_ich8lan - Starts flash cycle (read/write/erase)
 *  @hw: pointer to the HW structure
 *  @timeout: maximum time to wait for completion
 *
 *  This function starts a flash cycle and waits for its completion.
 **/
static s32 e1000_flash_cycle_ich8lan(struct e1000_hw *hw, u32 timeout)
{
	union ich8_hws_flash_ctrl hsflctl;
	union ich8_hws_flash_status hsfsts;
	s32 ret_val = -E1000_ERR_NVM;
	u32 i = 0;

	DEBUGFUNC("e1000_flash_cycle_ich8lan");

	/* Start a cycle by writing 1 in Flash Cycle Go in Hw Flash Control */
	hsflctl.regval = E1000_READ_FLASH_REG16(hw, ICH_FLASH_HSFCTL);
	hsflctl.hsf_ctrl.flcgo = 1;
	E1000_WRITE_FLASH_REG16(hw, ICH_FLASH_HSFCTL, hsflctl.regval);

	/* wait till FDONE bit is set to 1 */
	do {
		hsfsts.regval = E1000_READ_FLASH_REG16(hw, ICH_FLASH_HSFSTS);
		if (hsfsts.hsf_status.flcdone == 1)
			break;
		usec_delay(1);
	} while (i++ < timeout);

	if (hsfsts.hsf_status.flcdone == 1 && hsfsts.hsf_status.flcerr == 0)
		ret_val = E1000_SUCCESS;

	return ret_val;
}

/**
 *  e1000_read_flash_word_ich8lan - Read word from flash
 *  @hw: pointer to the HW structure
 *  @offset: offset to data location
 *  @data: pointer to the location for storing the data
 *
 *  Reads the flash word at offset into data.  Offset is converted
 *  to bytes before read.
 **/
static s32 e1000_read_flash_word_ich8lan(struct e1000_hw *hw, u32 offset,
                                         u16 *data)
{
	s32 ret_val;

	DEBUGFUNC("e1000_read_flash_word_ich8lan");

	if (!data) {
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	/* Must convert offset into bytes. */
	offset <<= 1;

	ret_val = e1000_read_flash_data_ich8lan(hw, offset, 2, data);

out:
	return ret_val;
}

/**
 *  e1000_read_flash_byte_ich8lan - Read byte from flash
 *  @hw: pointer to the HW structure
 *  @offset: The offset of the byte to read.
 *  @data: Pointer to a byte to store the value read.
 *
 *  Reads a single byte from the NVM using the flash access registers.
 **/
static s32 e1000_read_flash_byte_ich8lan(struct e1000_hw *hw, u32 offset,
                                         u8 *data)
{
	s32 ret_val = E1000_SUCCESS;
	u16 word = 0;

	ret_val = e1000_read_flash_data_ich8lan(hw, offset, 1, &word);
	if (ret_val)
		goto out;

	*data = (u8)word;

out:
	return ret_val;
}

/**
 *  e1000_read_flash_data_ich8lan - Read byte or word from NVM
 *  @hw: pointer to the HW structure
 *  @offset: The offset (in bytes) of the byte or word to read.
 *  @size: Size of data to read, 1=byte 2=word
 *  @data: Pointer to the word to store the value read.
 *
 *  Reads a byte or word from the NVM using the flash access registers.
 **/
static s32 e1000_read_flash_data_ich8lan(struct e1000_hw *hw, u32 offset,
                                         u8 size, u16 *data)
{
	union ich8_hws_flash_status hsfsts;
	union ich8_hws_flash_ctrl hsflctl;
	u32 flash_linear_addr;
	u32 flash_data = 0;
	s32 ret_val = -E1000_ERR_NVM;
	u8 count = 0;

	DEBUGFUNC("e1000_read_flash_data_ich8lan");

	if (size < 1  || size > 2 || offset > ICH_FLASH_LINEAR_ADDR_MASK)
		goto out;

	flash_linear_addr = (ICH_FLASH_LINEAR_ADDR_MASK & offset) +
	                    hw->nvm.flash_base_addr;

	do {
		usec_delay(1);
		/* Steps */
		ret_val = e1000_flash_cycle_init_ich8lan(hw);
		if (ret_val != E1000_SUCCESS)
			break;

		hsflctl.regval = E1000_READ_FLASH_REG16(hw, ICH_FLASH_HSFCTL);
		/* 0b/1b corresponds to 1 or 2 byte size, respectively. */
		hsflctl.hsf_ctrl.fldbcount = size - 1;
		hsflctl.hsf_ctrl.flcycle = ICH_CYCLE_READ;
		E1000_WRITE_FLASH_REG16(hw, ICH_FLASH_HSFCTL, hsflctl.regval);

		E1000_WRITE_FLASH_REG(hw, ICH_FLASH_FADDR, flash_linear_addr);

		ret_val = e1000_flash_cycle_ich8lan(hw,
		                                ICH_FLASH_READ_COMMAND_TIMEOUT);

		/*
		 * Check if FCERR is set to 1, if set to 1, clear it
		 * and try the whole sequence a few more times, else
		 * read in (shift in) the Flash Data0, the order is
		 * least significant byte first msb to lsb
		 */
		if (ret_val == E1000_SUCCESS) {
			flash_data = E1000_READ_FLASH_REG(hw, ICH_FLASH_FDATA0);
			if (size == 1)
				*data = (u8)(flash_data & 0x000000FF);
			else if (size == 2)
				*data = (u16)(flash_data & 0x0000FFFF);
			break;
		} else {
			/*
			 * If we've gotten here, then things are probably
			 * completely hosed, but if the error condition is
			 * detected, it won't hurt to give it another try...
			 * ICH_FLASH_CYCLE_REPEAT_COUNT times.
			 */
			hsfsts.regval = E1000_READ_FLASH_REG16(hw,
			                                      ICH_FLASH_HSFSTS);
			if (hsfsts.hsf_status.flcerr == 1) {
				/* Repeat for some time before giving up. */
				continue;
			} else if (hsfsts.hsf_status.flcdone == 0) {
				DEBUGOUT("Timeout error - flash cycle "
				         "did not complete.");
				break;
			}
		}
	} while (count++ < ICH_FLASH_CYCLE_REPEAT_COUNT);

out:
	return ret_val;
}

/**
 *  e1000_write_nvm_ich8lan - Write word(s) to the NVM
 *  @hw: pointer to the HW structure
 *  @offset: The offset (in bytes) of the word(s) to write.
 *  @words: Size of data to write in words
 *  @data: Pointer to the word(s) to write at offset.
 *
 *  Writes a byte or word to the NVM using the flash access registers.
 **/
static s32 e1000_write_nvm_ich8lan(struct e1000_hw *hw, u16 offset, u16 words,
                                   u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_dev_spec_ich8lan *dev_spec = &hw->dev_spec.ich8lan;
	s32 ret_val = E1000_SUCCESS;
	u16 i;

	DEBUGFUNC("e1000_write_nvm_ich8lan");

	if ((offset >= nvm->word_size) || (words > nvm->word_size - offset) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	nvm->ops.acquire(hw);

	for (i = 0; i < words; i++) {
		dev_spec->shadow_ram[offset+i].modified = TRUE;
		dev_spec->shadow_ram[offset+i].value = data[i];
	}

	nvm->ops.release(hw);

out:
	return ret_val;
}

/**
 *  e1000_update_nvm_checksum_ich8lan - Update the checksum for NVM
 *  @hw: pointer to the HW structure
 *
 *  The NVM checksum is updated by calling the generic update_nvm_checksum,
 *  which writes the checksum to the shadow ram.  The changes in the shadow
 *  ram are then committed to the EEPROM by processing each bank at a time
 *  checking for the modified bit and writing only the pending changes.
 *  After a successful commit, the shadow ram is cleared and is ready for
 *  future writes.
 **/
static s32 e1000_update_nvm_checksum_ich8lan(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_dev_spec_ich8lan *dev_spec = &hw->dev_spec.ich8lan;
	u32 i, act_offset, new_bank_offset, old_bank_offset, bank;
	s32 ret_val;
	u16 data;

	DEBUGFUNC("e1000_update_nvm_checksum_ich8lan");

	ret_val = e1000_update_nvm_checksum_generic(hw);
	if (ret_val)
		goto out;

	if (nvm->type != e1000_nvm_flash_sw)
		goto out;

	nvm->ops.acquire(hw);

	/*
	 * We're writing to the opposite bank so if we're on bank 1,
	 * write to bank 0 etc.  We also need to erase the segment that
	 * is going to be written
	 */
	ret_val =  e1000_valid_nvm_bank_detect_ich8lan(hw, &bank);
	if (ret_val != E1000_SUCCESS) {
		DEBUGOUT("Could not detect valid bank, assuming bank 0\n");
		bank = 0;
	}

	if (bank == 0) {
		new_bank_offset = nvm->flash_bank_size;
		old_bank_offset = 0;
		ret_val = e1000_erase_flash_bank_ich8lan(hw, 1);
		if (ret_val)
			goto release;
	} else {
		old_bank_offset = nvm->flash_bank_size;
		new_bank_offset = 0;
		ret_val = e1000_erase_flash_bank_ich8lan(hw, 0);
		if (ret_val)
			goto release;
	}

	for (i = 0; i < E1000_SHADOW_RAM_WORDS; i++) {
		/*
		 * Determine whether to write the value stored
		 * in the other NVM bank or a modified value stored
		 * in the shadow RAM
		 */
		if (dev_spec->shadow_ram[i].modified) {
			data = dev_spec->shadow_ram[i].value;
		} else {
			ret_val = e1000_read_flash_word_ich8lan(hw, i +
			                                        old_bank_offset,
			                                        &data);
			if (ret_val)
				break;
		}

		/*
		 * If the word is 0x13, then make sure the signature bits
		 * (15:14) are 11b until the commit has completed.
		 * This will allow us to write 10b which indicates the
		 * signature is valid.  We want to do this after the write
		 * has completed so that we don't mark the segment valid
		 * while the write is still in progress
		 */
		if (i == E1000_ICH_NVM_SIG_WORD)
			data |= E1000_ICH_NVM_SIG_MASK;

		/* Convert offset to bytes. */
		act_offset = (i + new_bank_offset) << 1;

		usec_delay(100);
		/* Write the bytes to the new bank. */
		ret_val = e1000_retry_write_flash_byte_ich8lan(hw,
		                                               act_offset,
		                                               (u8)data);
		if (ret_val)
			break;

		usec_delay(100);
		ret_val = e1000_retry_write_flash_byte_ich8lan(hw,
		                                          act_offset + 1,
		                                          (u8)(data >> 8));
		if (ret_val)
			break;
	}

	/*
	 * Don't bother writing the segment valid bits if sector
	 * programming failed.
	 */
	if (ret_val) {
		DEBUGOUT("Flash commit failed.\n");
		goto release;
	}

	/*
	 * Finally validate the new segment by setting bit 15:14
	 * to 10b in word 0x13 , this can be done without an
	 * erase as well since these bits are 11 to start with
	 * and we need to change bit 14 to 0b
	 */
	act_offset = new_bank_offset + E1000_ICH_NVM_SIG_WORD;
	ret_val = e1000_read_flash_word_ich8lan(hw, act_offset, &data);
	if (ret_val)
		goto release;

	data &= 0xBFFF;
	ret_val = e1000_retry_write_flash_byte_ich8lan(hw,
	                                               act_offset * 2 + 1,
	                                               (u8)(data >> 8));
	if (ret_val)
		goto release;

	/*
	 * And invalidate the previously valid segment by setting
	 * its signature word (0x13) high_byte to 0b. This can be
	 * done without an erase because flash erase sets all bits
	 * to 1's. We can write 1's to 0's without an erase
	 */
	act_offset = (old_bank_offset + E1000_ICH_NVM_SIG_WORD) * 2 + 1;
	ret_val = e1000_retry_write_flash_byte_ich8lan(hw, act_offset, 0);
	if (ret_val)
		goto release;

	/* Great!  Everything worked, we can now clear the cached entries. */
	for (i = 0; i < E1000_SHADOW_RAM_WORDS; i++) {
		dev_spec->shadow_ram[i].modified = FALSE;
		dev_spec->shadow_ram[i].value = 0xFFFF;
	}

release:
	nvm->ops.release(hw);

	/*
	 * Reload the EEPROM, or else modifications will not appear
	 * until after the next adapter reset.
	 */
	if (!ret_val) {
		nvm->ops.reload(hw);
		msec_delay(10);
	}

out:
	if (ret_val)
		DEBUGOUT1("NVM update error: %d\n", ret_val);

	return ret_val;
}

/**
 *  e1000_validate_nvm_checksum_ich8lan - Validate EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Check to see if checksum needs to be fixed by reading bit 6 in word 0x19.
 *  If the bit is 0, that the EEPROM had been modified, but the checksum was not
 *  calculated, in which case we need to calculate the checksum and set bit 6.
 **/
static s32 e1000_validate_nvm_checksum_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 data;

	DEBUGFUNC("e1000_validate_nvm_checksum_ich8lan");

	/*
	 * Read 0x19 and check bit 6.  If this bit is 0, the checksum
	 * needs to be fixed.  This bit is an indication that the NVM
	 * was prepared by OEM software and did not calculate the
	 * checksum...a likely scenario.
	 */
	ret_val = hw->nvm.ops.read(hw, 0x19, 1, &data);
	if (ret_val)
		goto out;

	if ((data & 0x40) == 0) {
		data |= 0x40;
		ret_val = hw->nvm.ops.write(hw, 0x19, 1, &data);
		if (ret_val)
			goto out;
		ret_val = hw->nvm.ops.update(hw);
		if (ret_val)
			goto out;
	}

	ret_val = e1000_validate_nvm_checksum_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_write_flash_data_ich8lan - Writes bytes to the NVM
 *  @hw: pointer to the HW structure
 *  @offset: The offset (in bytes) of the byte/word to read.
 *  @size: Size of data to read, 1=byte 2=word
 *  @data: The byte(s) to write to the NVM.
 *
 *  Writes one/two bytes to the NVM using the flash access registers.
 **/
static s32 e1000_write_flash_data_ich8lan(struct e1000_hw *hw, u32 offset,
                                          u8 size, u16 data)
{
	union ich8_hws_flash_status hsfsts;
	union ich8_hws_flash_ctrl hsflctl;
	u32 flash_linear_addr;
	u32 flash_data = 0;
	s32 ret_val = -E1000_ERR_NVM;
	u8 count = 0;

	DEBUGFUNC("e1000_write_ich8_data");

	if (size < 1 || size > 2 || data > size * 0xff ||
	    offset > ICH_FLASH_LINEAR_ADDR_MASK)
		goto out;

	flash_linear_addr = (ICH_FLASH_LINEAR_ADDR_MASK & offset) +
	                    hw->nvm.flash_base_addr;

	do {
		usec_delay(1);
		/* Steps */
		ret_val = e1000_flash_cycle_init_ich8lan(hw);
		if (ret_val != E1000_SUCCESS)
			break;

		hsflctl.regval = E1000_READ_FLASH_REG16(hw, ICH_FLASH_HSFCTL);
		/* 0b/1b corresponds to 1 or 2 byte size, respectively. */
		hsflctl.hsf_ctrl.fldbcount = size - 1;
		hsflctl.hsf_ctrl.flcycle = ICH_CYCLE_WRITE;
		E1000_WRITE_FLASH_REG16(hw, ICH_FLASH_HSFCTL, hsflctl.regval);

		E1000_WRITE_FLASH_REG(hw, ICH_FLASH_FADDR, flash_linear_addr);

		if (size == 1)
			flash_data = (u32)data & 0x00FF;
		else
			flash_data = (u32)data;

		E1000_WRITE_FLASH_REG(hw, ICH_FLASH_FDATA0, flash_data);

		/*
		 * check if FCERR is set to 1 , if set to 1, clear it
		 * and try the whole sequence a few more times else done
		 */
		ret_val = e1000_flash_cycle_ich8lan(hw,
		                               ICH_FLASH_WRITE_COMMAND_TIMEOUT);
		if (ret_val == E1000_SUCCESS)
			break;

		/*
		 * If we're here, then things are most likely
		 * completely hosed, but if the error condition
		 * is detected, it won't hurt to give it another
		 * try...ICH_FLASH_CYCLE_REPEAT_COUNT times.
		 */
		hsfsts.regval = E1000_READ_FLASH_REG16(hw, ICH_FLASH_HSFSTS);
		if (hsfsts.hsf_status.flcerr == 1)
			/* Repeat for some time before giving up. */
			continue;
		if (hsfsts.hsf_status.flcdone == 0) {
			DEBUGOUT("Timeout error - flash cycle "
				 "did not complete.");
			break;
		}
	} while (count++ < ICH_FLASH_CYCLE_REPEAT_COUNT);

out:
	return ret_val;
}

/**
 *  e1000_write_flash_byte_ich8lan - Write a single byte to NVM
 *  @hw: pointer to the HW structure
 *  @offset: The index of the byte to read.
 *  @data: The byte to write to the NVM.
 *
 *  Writes a single byte to the NVM using the flash access registers.
 **/
static s32 e1000_write_flash_byte_ich8lan(struct e1000_hw *hw, u32 offset,
                                          u8 data)
{
	u16 word = (u16)data;

	DEBUGFUNC("e1000_write_flash_byte_ich8lan");

	return e1000_write_flash_data_ich8lan(hw, offset, 1, word);
}

/**
 *  e1000_retry_write_flash_byte_ich8lan - Writes a single byte to NVM
 *  @hw: pointer to the HW structure
 *  @offset: The offset of the byte to write.
 *  @byte: The byte to write to the NVM.
 *
 *  Writes a single byte to the NVM using the flash access registers.
 *  Goes through a retry algorithm before giving up.
 **/
static s32 e1000_retry_write_flash_byte_ich8lan(struct e1000_hw *hw,
                                                u32 offset, u8 byte)
{
	s32 ret_val;
	u16 program_retries;

	DEBUGFUNC("e1000_retry_write_flash_byte_ich8lan");

	ret_val = e1000_write_flash_byte_ich8lan(hw, offset, byte);
	if (ret_val == E1000_SUCCESS)
		goto out;

	for (program_retries = 0; program_retries < 100; program_retries++) {
		DEBUGOUT2("Retrying Byte %2.2X at offset %u\n", byte, offset);
		usec_delay(100);
		ret_val = e1000_write_flash_byte_ich8lan(hw, offset, byte);
		if (ret_val == E1000_SUCCESS)
			break;
	}
	if (program_retries == 100) {
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_erase_flash_bank_ich8lan - Erase a bank (4k) from NVM
 *  @hw: pointer to the HW structure
 *  @bank: 0 for first bank, 1 for second bank, etc.
 *
 *  Erases the bank specified. Each bank is a 4k block. Banks are 0 based.
 *  bank N is 4096 * N + flash_reg_addr.
 **/
static s32 e1000_erase_flash_bank_ich8lan(struct e1000_hw *hw, u32 bank)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	union ich8_hws_flash_status hsfsts;
	union ich8_hws_flash_ctrl hsflctl;
	u32 flash_linear_addr;
	/* bank size is in 16bit words - adjust to bytes */
	u32 flash_bank_size = nvm->flash_bank_size * 2;
	s32 ret_val = E1000_SUCCESS;
	s32 count = 0;
	s32 j, iteration, sector_size;

	DEBUGFUNC("e1000_erase_flash_bank_ich8lan");

	hsfsts.regval = E1000_READ_FLASH_REG16(hw, ICH_FLASH_HSFSTS);

	/*
	 * Determine HW Sector size: Read BERASE bits of hw flash status
	 * register
	 * 00: The Hw sector is 256 bytes, hence we need to erase 16
	 *     consecutive sectors.  The start index for the nth Hw sector
	 *     can be calculated as = bank * 4096 + n * 256
	 * 01: The Hw sector is 4K bytes, hence we need to erase 1 sector.
	 *     The start index for the nth Hw sector can be calculated
	 *     as = bank * 4096
	 * 10: The Hw sector is 8K bytes, nth sector = bank * 8192
	 *     (ich9 only, otherwise error condition)
	 * 11: The Hw sector is 64K bytes, nth sector = bank * 65536
	 */
	switch (hsfsts.hsf_status.berasesz) {
	case 0:
		/* Hw sector size 256 */
		sector_size = ICH_FLASH_SEG_SIZE_256;
		iteration = flash_bank_size / ICH_FLASH_SEG_SIZE_256;
		break;
	case 1:
		sector_size = ICH_FLASH_SEG_SIZE_4K;
		iteration = 1;
		break;
	case 2:
		sector_size = ICH_FLASH_SEG_SIZE_8K;
		iteration = 1;
		break;
	case 3:
		sector_size = ICH_FLASH_SEG_SIZE_64K;
		iteration = 1;
		break;
	default:
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	/* Start with the base address, then add the sector offset. */
	flash_linear_addr = hw->nvm.flash_base_addr;
	flash_linear_addr += (bank) ? flash_bank_size : 0;

	for (j = 0; j < iteration ; j++) {
		do {
			/* Steps */
			ret_val = e1000_flash_cycle_init_ich8lan(hw);
			if (ret_val)
				goto out;

			/*
			 * Write a value 11 (block Erase) in Flash
			 * Cycle field in hw flash control
			 */
			hsflctl.regval = E1000_READ_FLASH_REG16(hw,
			                                      ICH_FLASH_HSFCTL);
			hsflctl.hsf_ctrl.flcycle = ICH_CYCLE_ERASE;
			E1000_WRITE_FLASH_REG16(hw, ICH_FLASH_HSFCTL,
			                        hsflctl.regval);

			/*
			 * Write the last 24 bits of an index within the
			 * block into Flash Linear address field in Flash
			 * Address.
			 */
			flash_linear_addr += (j * sector_size);
			E1000_WRITE_FLASH_REG(hw, ICH_FLASH_FADDR,
			                      flash_linear_addr);

			ret_val = e1000_flash_cycle_ich8lan(hw,
			                       ICH_FLASH_ERASE_COMMAND_TIMEOUT);
			if (ret_val == E1000_SUCCESS)
				break;

			/*
			 * Check if FCERR is set to 1.  If 1,
			 * clear it and try the whole sequence
			 * a few more times else Done
			 */
			hsfsts.regval = E1000_READ_FLASH_REG16(hw,
						      ICH_FLASH_HSFSTS);
			if (hsfsts.hsf_status.flcerr == 1)
				/* repeat for some time before giving up */
				continue;
			else if (hsfsts.hsf_status.flcdone == 0)
				goto out;
		} while (++count < ICH_FLASH_CYCLE_REPEAT_COUNT);
	}

out:
	return ret_val;
}

/**
 *  e1000_valid_led_default_ich8lan - Set the default LED settings
 *  @hw: pointer to the HW structure
 *  @data: Pointer to the LED settings
 *
 *  Reads the LED default settings from the NVM to data.  If the NVM LED
 *  settings is all 0's or F's, set the LED default to a valid LED default
 *  setting.
 **/
static s32 e1000_valid_led_default_ich8lan(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	DEBUGFUNC("e1000_valid_led_default_ich8lan");

	ret_val = hw->nvm.ops.read(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		goto out;
	}

	if (*data == ID_LED_RESERVED_0000 ||
	    *data == ID_LED_RESERVED_FFFF)
		*data = ID_LED_DEFAULT_ICH8LAN;

out:
	return ret_val;
}

/**
 *  e1000_id_led_init_pchlan - store LED configurations
 *  @hw: pointer to the HW structure
 *
 *  PCH does not control LEDs via the LEDCTL register, rather it uses
 *  the PHY LED configuration register.
 *
 *  PCH also does not have an "always on" or "always off" mode which
 *  complicates the ID feature.  Instead of using the "on" mode to indicate
 *  in ledctl_mode2 the LEDs to use for ID (see e1000_id_led_init_generic()),
 *  use "link_up" mode.  The LEDs will still ID on request if there is no
 *  link based on logic in e1000_led_[on|off]_pchlan().
 **/
static s32 e1000_id_led_init_pchlan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	const u32 ledctl_on = E1000_LEDCTL_MODE_LINK_UP;
	const u32 ledctl_off = E1000_LEDCTL_MODE_LINK_UP | E1000_PHY_LED0_IVRT;
	u16 data, i, temp, shift;

	DEBUGFUNC("e1000_id_led_init_pchlan");

	/* Get default ID LED modes */
	ret_val = hw->nvm.ops.valid_led_default(hw, &data);
	if (ret_val)
		goto out;

	mac->ledctl_default = E1000_READ_REG(hw, E1000_LEDCTL);
	mac->ledctl_mode1 = mac->ledctl_default;
	mac->ledctl_mode2 = mac->ledctl_default;

	for (i = 0; i < 4; i++) {
		temp = (data >> (i << 2)) & E1000_LEDCTL_LED0_MODE_MASK;
		shift = (i * 5);
		switch (temp) {
		case ID_LED_ON1_DEF2:
		case ID_LED_ON1_ON2:
		case ID_LED_ON1_OFF2:
			mac->ledctl_mode1 &= ~(E1000_PHY_LED0_MASK << shift);
			mac->ledctl_mode1 |= (ledctl_on << shift);
			break;
		case ID_LED_OFF1_DEF2:
		case ID_LED_OFF1_ON2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode1 &= ~(E1000_PHY_LED0_MASK << shift);
			mac->ledctl_mode1 |= (ledctl_off << shift);
			break;
		default:
			/* Do nothing */
			break;
		}
		switch (temp) {
		case ID_LED_DEF1_ON2:
		case ID_LED_ON1_ON2:
		case ID_LED_OFF1_ON2:
			mac->ledctl_mode2 &= ~(E1000_PHY_LED0_MASK << shift);
			mac->ledctl_mode2 |= (ledctl_on << shift);
			break;
		case ID_LED_DEF1_OFF2:
		case ID_LED_ON1_OFF2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode2 &= ~(E1000_PHY_LED0_MASK << shift);
			mac->ledctl_mode2 |= (ledctl_off << shift);
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
 *  e1000_get_bus_info_ich8lan - Get/Set the bus type and width
 *  @hw: pointer to the HW structure
 *
 *  ICH8 use the PCI Express bus, but does not contain a PCI Express Capability
 *  register, so the the bus width is hard coded.
 **/
static s32 e1000_get_bus_info_ich8lan(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;
	s32 ret_val;

	DEBUGFUNC("e1000_get_bus_info_ich8lan");

	ret_val = e1000_get_bus_info_pcie_generic(hw);

	/*
	 * ICH devices are "PCI Express"-ish.  They have
	 * a configuration space, but do not contain
	 * PCI Express Capability registers, so bus width
	 * must be hardcoded.
	 */
	if (bus->width == e1000_bus_width_unknown)
		bus->width = e1000_bus_width_pcie_x1;

	return ret_val;
}

/**
 *  e1000_reset_hw_ich8lan - Reset the hardware
 *  @hw: pointer to the HW structure
 *
 *  Does a full reset of the hardware which includes a reset of the PHY and
 *  MAC.
 **/
static s32 e1000_reset_hw_ich8lan(struct e1000_hw *hw)
{
	struct e1000_dev_spec_ich8lan *dev_spec = &hw->dev_spec.ich8lan;
	u16 reg;
	u32 ctrl, icr, kab;
	s32 ret_val;

	DEBUGFUNC("e1000_reset_hw_ich8lan");

	/*
	 * Prevent the PCI-E bus from sticking if there is no TLP connection
	 * on the last TLP read/write transaction when MAC is reset.
	 */
	ret_val = e1000_disable_pcie_master_generic(hw);
	if (ret_val)
		DEBUGOUT("PCI-E Master disable polling has failed.\n");

	DEBUGOUT("Masking off all interrupts\n");
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);

	/*
	 * Disable the Transmit and Receive units.  Then delay to allow
	 * any pending transactions to complete before we hit the MAC
	 * with the global reset.
	 */
	E1000_WRITE_REG(hw, E1000_RCTL, 0);
	E1000_WRITE_REG(hw, E1000_TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH(hw);

	msec_delay(10);

	/* Workaround for ICH8 bit corruption issue in FIFO memory */
	if (hw->mac.type == e1000_ich8lan) {
		/* Set Tx and Rx buffer allocation to 8k apiece. */
		E1000_WRITE_REG(hw, E1000_PBA, E1000_PBA_8K);
		/* Set Packet Buffer Size to 16k. */
		E1000_WRITE_REG(hw, E1000_PBS, E1000_PBS_16K);
	}

	if (hw->mac.type == e1000_pchlan) {
		/* Save the NVM K1 bit setting*/
		ret_val = e1000_read_nvm(hw, E1000_NVM_K1_CONFIG, 1, &reg);
		if (ret_val)
			return ret_val;

		if (reg & E1000_NVM_K1_ENABLE)
			dev_spec->nvm_k1_enabled = TRUE;
		else
			dev_spec->nvm_k1_enabled = FALSE;
	}

	ctrl = E1000_READ_REG(hw, E1000_CTRL);

	if (!hw->phy.ops.check_reset_block(hw)) {
		/*
		 * Full-chip reset requires MAC and PHY reset at the same
		 * time to make sure the interface between MAC and the
		 * external PHY is reset.
		 */
		ctrl |= E1000_CTRL_PHY_RST;

		/*
		 * Gate automatic PHY configuration by hardware on
		 * non-managed 82579
		 */
		if ((hw->mac.type == e1000_pch2lan) &&
		    !(E1000_READ_REG(hw, E1000_FWSM) & E1000_ICH_FWSM_FW_VALID))
			e1000_gate_hw_phy_config_ich8lan(hw, TRUE);
	}
	ret_val = e1000_acquire_swflag_ich8lan(hw);
	DEBUGOUT("Issuing a global reset to ich8lan\n");
	E1000_WRITE_REG(hw, E1000_CTRL, (ctrl | E1000_CTRL_RST));
	msec_delay(20);

	if (!ret_val)
		e1000_release_swflag_ich8lan(hw);

	if (ctrl & E1000_CTRL_PHY_RST) {
		ret_val = hw->phy.ops.get_cfg_done(hw);
		if (ret_val)
			goto out;

		ret_val = e1000_post_phy_reset_ich8lan(hw);
		if (ret_val)
			goto out;
	}

	/*
	 * For PCH, this write will make sure that any noise
	 * will be detected as a CRC error and be dropped rather than show up
	 * as a bad packet to the DMA engine.
	 */
	if (hw->mac.type == e1000_pchlan)
		E1000_WRITE_REG(hw, E1000_CRC_OFFSET, 0x65656565);

	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	icr = E1000_READ_REG(hw, E1000_ICR);

	kab = E1000_READ_REG(hw, E1000_KABGTXD);
	kab |= E1000_KABGTXD_BGSQLBIAS;
	E1000_WRITE_REG(hw, E1000_KABGTXD, kab);

out:
	return ret_val;
}

/**
 *  e1000_init_hw_ich8lan - Initialize the hardware
 *  @hw: pointer to the HW structure
 *
 *  Prepares the hardware for transmit and receive by doing the following:
 *   - initialize hardware bits
 *   - initialize LED identification
 *   - setup receive address registers
 *   - setup flow control
 *   - setup transmit descriptors
 *   - clear statistics
 **/
static s32 e1000_init_hw_ich8lan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 ctrl_ext, txdctl, snoop;
	s32 ret_val;
	u16 i;

	DEBUGFUNC("e1000_init_hw_ich8lan");

	e1000_initialize_hw_bits_ich8lan(hw);

	/* Initialize identification LED */
	ret_val = mac->ops.id_led_init(hw);
	if (ret_val)
		DEBUGOUT("Error initializing identification LED\n");
		/* This is not fatal and we should not stop init due to this */

	/* Setup the receive address. */
	e1000_init_rx_addrs_generic(hw, mac->rar_entry_count);

	/* Zero out the Multicast HASH table */
	DEBUGOUT("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);

	/*
	 * The 82578 Rx buffer will stall if wakeup is enabled in host and
	 * the ME.  Reading the BM_WUC register will clear the host wakeup bit.
	 * Reset the phy after disabling host wakeup to reset the Rx buffer.
	 */
	if (hw->phy.type == e1000_phy_82578) {
		hw->phy.ops.read_reg(hw, BM_WUC, &i);
		ret_val = e1000_phy_hw_reset_ich8lan(hw);
		if (ret_val)
			return ret_val;
	}

	/* Setup link and flow control */
	ret_val = mac->ops.setup_link(hw);

	/* Set the transmit descriptor write-back policy for both queues */
	txdctl = E1000_READ_REG(hw, E1000_TXDCTL(0));
	txdctl = (txdctl & ~E1000_TXDCTL_WTHRESH) |
		 E1000_TXDCTL_FULL_TX_DESC_WB;
	txdctl = (txdctl & ~E1000_TXDCTL_PTHRESH) |
	         E1000_TXDCTL_MAX_TX_DESC_PREFETCH;
	E1000_WRITE_REG(hw, E1000_TXDCTL(0), txdctl);
	txdctl = E1000_READ_REG(hw, E1000_TXDCTL(1));
	txdctl = (txdctl & ~E1000_TXDCTL_WTHRESH) |
		 E1000_TXDCTL_FULL_TX_DESC_WB;
	txdctl = (txdctl & ~E1000_TXDCTL_PTHRESH) |
	         E1000_TXDCTL_MAX_TX_DESC_PREFETCH;
	E1000_WRITE_REG(hw, E1000_TXDCTL(1), txdctl);

	/*
	 * ICH8 has opposite polarity of no_snoop bits.
	 * By default, we should use snoop behavior.
	 */
	if (mac->type == e1000_ich8lan)
		snoop = PCIE_ICH8_SNOOP_ALL;
	else
		snoop = (u32) ~(PCIE_NO_SNOOP_ALL);
	e1000_set_pcie_no_snoop_generic(hw, snoop);

	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
	ctrl_ext |= E1000_CTRL_EXT_RO_DIS;
	E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext);

	/*
	 * Clear all of the statistics registers (clear on read).  It is
	 * important that we do this after we have tried to establish link
	 * because the symbol error count will increment wildly if there
	 * is no link.
	 */
	e1000_clear_hw_cntrs_ich8lan(hw);

	return ret_val;
}
/**
 *  e1000_initialize_hw_bits_ich8lan - Initialize required hardware bits
 *  @hw: pointer to the HW structure
 *
 *  Sets/Clears required hardware bits necessary for correctly setting up the
 *  hardware for transmit and receive.
 **/
static void e1000_initialize_hw_bits_ich8lan(struct e1000_hw *hw)
{
	u32 reg;

	DEBUGFUNC("e1000_initialize_hw_bits_ich8lan");

	/* Extended Device Control */
	reg = E1000_READ_REG(hw, E1000_CTRL_EXT);
	reg |= (1 << 22);
	/* Enable PHY low-power state when MAC is at D3 w/o WoL */
	if (hw->mac.type >= e1000_pchlan)
		reg |= E1000_CTRL_EXT_PHYPDEN;
	E1000_WRITE_REG(hw, E1000_CTRL_EXT, reg);

	/* Transmit Descriptor Control 0 */
	reg = E1000_READ_REG(hw, E1000_TXDCTL(0));
	reg |= (1 << 22);
	E1000_WRITE_REG(hw, E1000_TXDCTL(0), reg);

	/* Transmit Descriptor Control 1 */
	reg = E1000_READ_REG(hw, E1000_TXDCTL(1));
	reg |= (1 << 22);
	E1000_WRITE_REG(hw, E1000_TXDCTL(1), reg);

	/* Transmit Arbitration Control 0 */
	reg = E1000_READ_REG(hw, E1000_TARC(0));
	if (hw->mac.type == e1000_ich8lan)
		reg |= (1 << 28) | (1 << 29);
	reg |= (1 << 23) | (1 << 24) | (1 << 26) | (1 << 27);
	E1000_WRITE_REG(hw, E1000_TARC(0), reg);

	/* Transmit Arbitration Control 1 */
	reg = E1000_READ_REG(hw, E1000_TARC(1));
	if (E1000_READ_REG(hw, E1000_TCTL) & E1000_TCTL_MULR)
		reg &= ~(1 << 28);
	else
		reg |= (1 << 28);
	reg |= (1 << 24) | (1 << 26) | (1 << 30);
	E1000_WRITE_REG(hw, E1000_TARC(1), reg);

	/* Device Status */
	if (hw->mac.type == e1000_ich8lan) {
		reg = E1000_READ_REG(hw, E1000_STATUS);
		reg &= ~(1 << 31);
		E1000_WRITE_REG(hw, E1000_STATUS, reg);
	}

	/*
	 * work-around descriptor data corruption issue during nfs v2 udp
	 * traffic, just disable the nfs filtering capability
	 */
	reg = E1000_READ_REG(hw, E1000_RFCTL);
	reg |= (E1000_RFCTL_NFSW_DIS | E1000_RFCTL_NFSR_DIS);
	E1000_WRITE_REG(hw, E1000_RFCTL, reg);

	return;
}

/**
 *  e1000_setup_link_ich8lan - Setup flow control and link settings
 *  @hw: pointer to the HW structure
 *
 *  Determines which flow control settings to use, then configures flow
 *  control.  Calls the appropriate media-specific link configuration
 *  function.  Assuming the adapter has a valid link partner, a valid link
 *  should be established.  Assumes the hardware has previously been reset
 *  and the transmitter and receiver are not enabled.
 **/
static s32 e1000_setup_link_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_setup_link_ich8lan");

	if (hw->phy.ops.check_reset_block(hw))
		goto out;

	/*
	 * ICH parts do not have a word in the NVM to determine
	 * the default flow control setting, so we explicitly
	 * set it to full.
	 */
	if (hw->fc.requested_mode == e1000_fc_default)
		hw->fc.requested_mode = e1000_fc_full;

	/*
	 * Save off the requested flow control mode for use later.  Depending
	 * on the link partner's capabilities, we may or may not use this mode.
	 */
	hw->fc.current_mode = hw->fc.requested_mode;

	DEBUGOUT1("After fix-ups FlowControl is now = %x\n",
		hw->fc.current_mode);

	/* Continue to configure the copper link. */
	ret_val = hw->mac.ops.setup_physical_interface(hw);
	if (ret_val)
		goto out;

	E1000_WRITE_REG(hw, E1000_FCTTV, hw->fc.pause_time);
	if ((hw->phy.type == e1000_phy_82578) ||
	    (hw->phy.type == e1000_phy_82579) ||
	    (hw->phy.type == e1000_phy_82577)) {
		E1000_WRITE_REG(hw, E1000_FCRTV_PCH, hw->fc.refresh_time);

		ret_val = hw->phy.ops.write_reg(hw,
		                             PHY_REG(BM_PORT_CTRL_PAGE, 27),
		                             hw->fc.pause_time);
		if (ret_val)
			goto out;
	}

	ret_val = e1000_set_fc_watermarks_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_setup_copper_link_ich8lan - Configure MAC/PHY interface
 *  @hw: pointer to the HW structure
 *
 *  Configures the kumeran interface to the PHY to wait the appropriate time
 *  when polling the PHY, then call the generic setup_copper_link to finish
 *  configuring the copper link.
 **/
static s32 e1000_setup_copper_link_ich8lan(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 reg_data;

	DEBUGFUNC("e1000_setup_copper_link_ich8lan");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

	/*
	 * Set the mac to wait the maximum time between each iteration
	 * and increase the max iterations when polling the phy;
	 * this fixes erroneous timeouts at 10Mbps.
	 */
	ret_val = e1000_write_kmrn_reg_generic(hw, E1000_KMRNCTRLSTA_TIMEOUTS,
	                                       0xFFFF);
	if (ret_val)
		goto out;
	ret_val = e1000_read_kmrn_reg_generic(hw,
	                                      E1000_KMRNCTRLSTA_INBAND_PARAM,
	                                      &reg_data);
	if (ret_val)
		goto out;
	reg_data |= 0x3F;
	ret_val = e1000_write_kmrn_reg_generic(hw,
	                                       E1000_KMRNCTRLSTA_INBAND_PARAM,
	                                       reg_data);
	if (ret_val)
		goto out;

	switch (hw->phy.type) {
	case e1000_phy_igp_3:
		ret_val = e1000_copper_link_setup_igp(hw);
		if (ret_val)
			goto out;
		break;
	case e1000_phy_bm:
	case e1000_phy_82578:
		ret_val = e1000_copper_link_setup_m88(hw);
		if (ret_val)
			goto out;
		break;
	case e1000_phy_82577:
	case e1000_phy_82579:
		ret_val = e1000_copper_link_setup_82577(hw);
		if (ret_val)
			goto out;
		break;
	case e1000_phy_ife:
		ret_val = hw->phy.ops.read_reg(hw, IFE_PHY_MDIX_CONTROL,
		                               &reg_data);
		if (ret_val)
			goto out;

		reg_data &= ~IFE_PMC_AUTO_MDIX;

		switch (hw->phy.mdix) {
		case 1:
			reg_data &= ~IFE_PMC_FORCE_MDIX;
			break;
		case 2:
			reg_data |= IFE_PMC_FORCE_MDIX;
			break;
		case 0:
		default:
			reg_data |= IFE_PMC_AUTO_MDIX;
			break;
		}
		ret_val = hw->phy.ops.write_reg(hw, IFE_PHY_MDIX_CONTROL,
		                                reg_data);
		if (ret_val)
			goto out;
		break;
	default:
		break;
	}
	ret_val = e1000_setup_copper_link_generic(hw);

out:
	return ret_val;
}

/**
 *  e1000_get_link_up_info_ich8lan - Get current link speed and duplex
 *  @hw: pointer to the HW structure
 *  @speed: pointer to store current link speed
 *  @duplex: pointer to store the current link duplex
 *
 *  Calls the generic get_speed_and_duplex to retrieve the current link
 *  information and then calls the Kumeran lock loss workaround for links at
 *  gigabit speeds.
 **/
static s32 e1000_get_link_up_info_ich8lan(struct e1000_hw *hw, u16 *speed,
                                          u16 *duplex)
{
	s32 ret_val;

	DEBUGFUNC("e1000_get_link_up_info_ich8lan");

	ret_val = e1000_get_speed_and_duplex_copper_generic(hw, speed, duplex);
	if (ret_val)
		goto out;

	if ((hw->mac.type == e1000_ich8lan) &&
	    (hw->phy.type == e1000_phy_igp_3) &&
	    (*speed == SPEED_1000)) {
		ret_val = e1000_kmrn_lock_loss_workaround_ich8lan(hw);
	}

out:
	return ret_val;
}

/**
 *  e1000_kmrn_lock_loss_workaround_ich8lan - Kumeran workaround
 *  @hw: pointer to the HW structure
 *
 *  Work-around for 82566 Kumeran PCS lock loss:
 *  On link status change (i.e. PCI reset, speed change) and link is up and
 *  speed is gigabit-
 *    0) if workaround is optionally disabled do nothing
 *    1) wait 1ms for Kumeran link to come up
 *    2) check Kumeran Diagnostic register PCS lock loss bit
 *    3) if not set the link is locked (all is good), otherwise...
 *    4) reset the PHY
 *    5) repeat up to 10 times
 *  Note: this is only called for IGP3 copper when speed is 1gb.
 **/
static s32 e1000_kmrn_lock_loss_workaround_ich8lan(struct e1000_hw *hw)
{
	struct e1000_dev_spec_ich8lan *dev_spec = &hw->dev_spec.ich8lan;
	u32 phy_ctrl;
	s32 ret_val = E1000_SUCCESS;
	u16 i, data;
	bool link;

	DEBUGFUNC("e1000_kmrn_lock_loss_workaround_ich8lan");

	if (!(dev_spec->kmrn_lock_loss_workaround_enabled))
		goto out;

	/*
	 * Make sure link is up before proceeding.  If not just return.
	 * Attempting this while link is negotiating fouled up link
	 * stability
	 */
	ret_val = e1000_phy_has_link_generic(hw, 1, 0, &link);
	if (!link) {
		ret_val = E1000_SUCCESS;
		goto out;
	}

	for (i = 0; i < 10; i++) {
		/* read once to clear */
		ret_val = hw->phy.ops.read_reg(hw, IGP3_KMRN_DIAG, &data);
		if (ret_val)
			goto out;
		/* and again to get new status */
		ret_val = hw->phy.ops.read_reg(hw, IGP3_KMRN_DIAG, &data);
		if (ret_val)
			goto out;

		/* check for PCS lock */
		if (!(data & IGP3_KMRN_DIAG_PCS_LOCK_LOSS)) {
			ret_val = E1000_SUCCESS;
			goto out;
		}

		/* Issue PHY reset */
		hw->phy.ops.reset(hw);
		msec_delay_irq(5);
	}
	/* Disable GigE link negotiation */
	phy_ctrl = E1000_READ_REG(hw, E1000_PHY_CTRL);
	phy_ctrl |= (E1000_PHY_CTRL_GBE_DISABLE |
	             E1000_PHY_CTRL_NOND0A_GBE_DISABLE);
	E1000_WRITE_REG(hw, E1000_PHY_CTRL, phy_ctrl);

	/*
	 * Call gig speed drop workaround on Gig disable before accessing
	 * any PHY registers
	 */
	e1000_gig_downshift_workaround_ich8lan(hw);

	/* unable to acquire PCS lock */
	ret_val = -E1000_ERR_PHY;

out:
	return ret_val;
}

/**
 *  e1000_set_kmrn_lock_loss_workaround_ich8lan - Set Kumeran workaround state
 *  @hw: pointer to the HW structure
 *  @state: boolean value used to set the current Kumeran workaround state
 *
 *  If ICH8, set the current Kumeran workaround state (enabled - TRUE
 *  /disabled - FALSE).
 **/
void e1000_set_kmrn_lock_loss_workaround_ich8lan(struct e1000_hw *hw,
                                                 bool state)
{
	struct e1000_dev_spec_ich8lan *dev_spec = &hw->dev_spec.ich8lan;

	DEBUGFUNC("e1000_set_kmrn_lock_loss_workaround_ich8lan");

	if (hw->mac.type != e1000_ich8lan) {
		DEBUGOUT("Workaround applies to ICH8 only.\n");
		return;
	}

	dev_spec->kmrn_lock_loss_workaround_enabled = state;

	return;
}

/**
 *  e1000_ipg3_phy_powerdown_workaround_ich8lan - Power down workaround on D3
 *  @hw: pointer to the HW structure
 *
 *  Workaround for 82566 power-down on D3 entry:
 *    1) disable gigabit link
 *    2) write VR power-down enable
 *    3) read it back
 *  Continue if successful, else issue LCD reset and repeat
 **/
void e1000_igp3_phy_powerdown_workaround_ich8lan(struct e1000_hw *hw)
{
	u32 reg;
	u16 data;
	u8  retry = 0;

	DEBUGFUNC("e1000_igp3_phy_powerdown_workaround_ich8lan");

	if (hw->phy.type != e1000_phy_igp_3)
		goto out;

	/* Try the workaround twice (if needed) */
	do {
		/* Disable link */
		reg = E1000_READ_REG(hw, E1000_PHY_CTRL);
		reg |= (E1000_PHY_CTRL_GBE_DISABLE |
		        E1000_PHY_CTRL_NOND0A_GBE_DISABLE);
		E1000_WRITE_REG(hw, E1000_PHY_CTRL, reg);

		/*
		 * Call gig speed drop workaround on Gig disable before
		 * accessing any PHY registers
		 */
		if (hw->mac.type == e1000_ich8lan)
			e1000_gig_downshift_workaround_ich8lan(hw);

		/* Write VR power-down enable */
		hw->phy.ops.read_reg(hw, IGP3_VR_CTRL, &data);
		data &= ~IGP3_VR_CTRL_DEV_POWERDOWN_MODE_MASK;
		hw->phy.ops.write_reg(hw, IGP3_VR_CTRL,
		                   data | IGP3_VR_CTRL_MODE_SHUTDOWN);

		/* Read it back and test */
		hw->phy.ops.read_reg(hw, IGP3_VR_CTRL, &data);
		data &= IGP3_VR_CTRL_DEV_POWERDOWN_MODE_MASK;
		if ((data == IGP3_VR_CTRL_MODE_SHUTDOWN) || retry)
			break;

		/* Issue PHY reset and repeat at most one more time */
		reg = E1000_READ_REG(hw, E1000_CTRL);
		E1000_WRITE_REG(hw, E1000_CTRL, reg | E1000_CTRL_PHY_RST);
		retry++;
	} while (retry);

out:
	return;
}

/**
 *  e1000_gig_downshift_workaround_ich8lan - WoL from S5 stops working
 *  @hw: pointer to the HW structure
 *
 *  Steps to take when dropping from 1Gb/s (eg. link cable removal (LSC),
 *  LPLU, Gig disable, MDIC PHY reset):
 *    1) Set Kumeran Near-end loopback
 *    2) Clear Kumeran Near-end loopback
 *  Should only be called for ICH8[m] devices with IGP_3 Phy.
 **/
void e1000_gig_downshift_workaround_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u16 reg_data;

	DEBUGFUNC("e1000_gig_downshift_workaround_ich8lan");

	if ((hw->mac.type != e1000_ich8lan) ||
	    (hw->phy.type != e1000_phy_igp_3))
		goto out;

	ret_val = e1000_read_kmrn_reg_generic(hw, E1000_KMRNCTRLSTA_DIAG_OFFSET,
	                                      &reg_data);
	if (ret_val)
		goto out;
	reg_data |= E1000_KMRNCTRLSTA_DIAG_NELPBK;
	ret_val = e1000_write_kmrn_reg_generic(hw,
	                                       E1000_KMRNCTRLSTA_DIAG_OFFSET,
	                                       reg_data);
	if (ret_val)
		goto out;
	reg_data &= ~E1000_KMRNCTRLSTA_DIAG_NELPBK;
	ret_val = e1000_write_kmrn_reg_generic(hw,
	                                       E1000_KMRNCTRLSTA_DIAG_OFFSET,
	                                       reg_data);
out:
	return;
}

/**
 *  e1000_disable_gig_wol_ich8lan - disable gig during WoL
 *  @hw: pointer to the HW structure
 *
 *  During S0 to Sx transition, it is possible the link remains at gig
 *  instead of negotiating to a lower speed.  Before going to Sx, set
 *  'LPLU Enabled' and 'Gig Disable' to force link speed negotiation
 *  to a lower speed.
 *
 *  Should only be called for applicable parts.
 **/
void e1000_disable_gig_wol_ich8lan(struct e1000_hw *hw)
{
	u32 phy_ctrl;
	s32 ret_val;

	DEBUGFUNC("e1000_disable_gig_wol_ich8lan");

	phy_ctrl = E1000_READ_REG(hw, E1000_PHY_CTRL);
	phy_ctrl |= E1000_PHY_CTRL_D0A_LPLU | E1000_PHY_CTRL_GBE_DISABLE;
	E1000_WRITE_REG(hw, E1000_PHY_CTRL, phy_ctrl);

	if (hw->mac.type >= e1000_pchlan) {
		e1000_oem_bits_config_ich8lan(hw, FALSE);
		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return;
		e1000_write_smbus_addr(hw);
		hw->phy.ops.release(hw);
	}

	return;
}

/**
 *  e1000_cleanup_led_ich8lan - Restore the default LED operation
 *  @hw: pointer to the HW structure
 *
 *  Return the LED back to the default configuration.
 **/
static s32 e1000_cleanup_led_ich8lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_cleanup_led_ich8lan");

	if (hw->phy.type == e1000_phy_ife)
		return hw->phy.ops.write_reg(hw, IFE_PHY_SPECIAL_CONTROL_LED,
		                             0);

	E1000_WRITE_REG(hw, E1000_LEDCTL, hw->mac.ledctl_default);
	return E1000_SUCCESS;
}

/**
 *  e1000_led_on_ich8lan - Turn LEDs on
 *  @hw: pointer to the HW structure
 *
 *  Turn on the LEDs.
 **/
static s32 e1000_led_on_ich8lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_led_on_ich8lan");

	if (hw->phy.type == e1000_phy_ife)
		return hw->phy.ops.write_reg(hw, IFE_PHY_SPECIAL_CONTROL_LED,
		                (IFE_PSCL_PROBE_MODE | IFE_PSCL_PROBE_LEDS_ON));

	E1000_WRITE_REG(hw, E1000_LEDCTL, hw->mac.ledctl_mode2);
	return E1000_SUCCESS;
}

/**
 *  e1000_led_off_ich8lan - Turn LEDs off
 *  @hw: pointer to the HW structure
 *
 *  Turn off the LEDs.
 **/
static s32 e1000_led_off_ich8lan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_led_off_ich8lan");

	if (hw->phy.type == e1000_phy_ife)
		return hw->phy.ops.write_reg(hw, IFE_PHY_SPECIAL_CONTROL_LED,
		               (IFE_PSCL_PROBE_MODE | IFE_PSCL_PROBE_LEDS_OFF));

	E1000_WRITE_REG(hw, E1000_LEDCTL, hw->mac.ledctl_mode1);
	return E1000_SUCCESS;
}

/**
 *  e1000_setup_led_pchlan - Configures SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This prepares the SW controllable LED for use.
 **/
static s32 e1000_setup_led_pchlan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_setup_led_pchlan");

	return hw->phy.ops.write_reg(hw, HV_LED_CONFIG,
					(u16)hw->mac.ledctl_mode1);
}

/**
 *  e1000_cleanup_led_pchlan - Restore the default LED operation
 *  @hw: pointer to the HW structure
 *
 *  Return the LED back to the default configuration.
 **/
static s32 e1000_cleanup_led_pchlan(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_cleanup_led_pchlan");

	return hw->phy.ops.write_reg(hw, HV_LED_CONFIG,
					(u16)hw->mac.ledctl_default);
}

/**
 *  e1000_led_on_pchlan - Turn LEDs on
 *  @hw: pointer to the HW structure
 *
 *  Turn on the LEDs.
 **/
static s32 e1000_led_on_pchlan(struct e1000_hw *hw)
{
	u16 data = (u16)hw->mac.ledctl_mode2;
	u32 i, led;

	DEBUGFUNC("e1000_led_on_pchlan");

	/*
	 * If no link, then turn LED on by setting the invert bit
	 * for each LED that's mode is "link_up" in ledctl_mode2.
	 */
	if (!(E1000_READ_REG(hw, E1000_STATUS) & E1000_STATUS_LU)) {
		for (i = 0; i < 3; i++) {
			led = (data >> (i * 5)) & E1000_PHY_LED0_MASK;
			if ((led & E1000_PHY_LED0_MODE_MASK) !=
			    E1000_LEDCTL_MODE_LINK_UP)
				continue;
			if (led & E1000_PHY_LED0_IVRT)
				data &= ~(E1000_PHY_LED0_IVRT << (i * 5));
			else
				data |= (E1000_PHY_LED0_IVRT << (i * 5));
		}
	}

	return hw->phy.ops.write_reg(hw, HV_LED_CONFIG, data);
}

/**
 *  e1000_led_off_pchlan - Turn LEDs off
 *  @hw: pointer to the HW structure
 *
 *  Turn off the LEDs.
 **/
static s32 e1000_led_off_pchlan(struct e1000_hw *hw)
{
	u16 data = (u16)hw->mac.ledctl_mode1;
	u32 i, led;

	DEBUGFUNC("e1000_led_off_pchlan");

	/*
	 * If no link, then turn LED off by clearing the invert bit
	 * for each LED that's mode is "link_up" in ledctl_mode1.
	 */
	if (!(E1000_READ_REG(hw, E1000_STATUS) & E1000_STATUS_LU)) {
		for (i = 0; i < 3; i++) {
			led = (data >> (i * 5)) & E1000_PHY_LED0_MASK;
			if ((led & E1000_PHY_LED0_MODE_MASK) !=
			    E1000_LEDCTL_MODE_LINK_UP)
				continue;
			if (led & E1000_PHY_LED0_IVRT)
				data &= ~(E1000_PHY_LED0_IVRT << (i * 5));
			else
				data |= (E1000_PHY_LED0_IVRT << (i * 5));
		}
	}

	return hw->phy.ops.write_reg(hw, HV_LED_CONFIG, data);
}

/**
 *  e1000_get_cfg_done_ich8lan - Read config done bit after Full or PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Read appropriate register for the config done bit for completion status
 *  and configure the PHY through s/w for EEPROM-less parts.
 *
 *  NOTE: some silicon which is EEPROM-less will fail trying to read the
 *  config done bit, so only an error is logged and continues.  If we were
 *  to return with error, EEPROM-less silicon would not be able to be reset
 *  or change link.
 **/
static s32 e1000_get_cfg_done_ich8lan(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;
	u32 bank = 0;
	u32 status;

	DEBUGFUNC("e1000_get_cfg_done_ich8lan");

	e1000_get_cfg_done_generic(hw);

	/* Wait for indication from h/w that it has completed basic config */
	if (hw->mac.type >= e1000_ich10lan) {
		e1000_lan_init_done_ich8lan(hw);
	} else {
		ret_val = e1000_get_auto_rd_done_generic(hw);
		if (ret_val) {
			/*
			 * When auto config read does not complete, do not
			 * return with an error. This can happen in situations
			 * where there is no eeprom and prevents getting link.
			 */
			DEBUGOUT("Auto Read Done did not complete\n");
			ret_val = E1000_SUCCESS;
		}
	}

	/* Clear PHY Reset Asserted bit */
	status = E1000_READ_REG(hw, E1000_STATUS);
	if (status & E1000_STATUS_PHYRA)
		E1000_WRITE_REG(hw, E1000_STATUS, status & ~E1000_STATUS_PHYRA);
	else
		DEBUGOUT("PHY Reset Asserted not set - needs delay\n");

	/* If EEPROM is not marked present, init the IGP 3 PHY manually */
	if (hw->mac.type <= e1000_ich9lan) {
		if (((E1000_READ_REG(hw, E1000_EECD) & E1000_EECD_PRES) == 0) &&
		    (hw->phy.type == e1000_phy_igp_3)) {
			e1000_phy_init_script_igp3(hw);
		}
	} else {
		if (e1000_valid_nvm_bank_detect_ich8lan(hw, &bank)) {
			/* Maybe we should do a basic PHY config */
			DEBUGOUT("EEPROM not present\n");
			ret_val = -E1000_ERR_CONFIG;
		}
	}

	return ret_val;
}

/**
 * e1000_power_down_phy_copper_ich8lan - Remove link during PHY power down
 * @hw: pointer to the HW structure
 *
 * In the case of a PHY power down to save power, or to turn off link during a
 * driver unload, or wake on lan is not enabled, remove the link.
 **/
static void e1000_power_down_phy_copper_ich8lan(struct e1000_hw *hw)
{
	/* If the management interface is not enabled, then power down */
	if (!(hw->mac.ops.check_mng_mode(hw) ||
	      hw->phy.ops.check_reset_block(hw)))
		e1000_power_down_phy_copper(hw);

	return;
}

/**
 *  e1000_clear_hw_cntrs_ich8lan - Clear statistical counters
 *  @hw: pointer to the HW structure
 *
 *  Clears hardware counters specific to the silicon family and calls
 *  clear_hw_cntrs_generic to clear all general purpose counters.
 **/
static void e1000_clear_hw_cntrs_ich8lan(struct e1000_hw *hw)
{
	u16 phy_data;

	DEBUGFUNC("e1000_clear_hw_cntrs_ich8lan");

	e1000_clear_hw_cntrs_base_generic(hw);

	E1000_READ_REG(hw, E1000_ALGNERRC);
	E1000_READ_REG(hw, E1000_RXERRC);
	E1000_READ_REG(hw, E1000_TNCRS);
	E1000_READ_REG(hw, E1000_CEXTERR);
	E1000_READ_REG(hw, E1000_TSCTC);
	E1000_READ_REG(hw, E1000_TSCTFC);

	E1000_READ_REG(hw, E1000_MGTPRC);
	E1000_READ_REG(hw, E1000_MGTPDC);
	E1000_READ_REG(hw, E1000_MGTPTC);

	E1000_READ_REG(hw, E1000_IAC);
	E1000_READ_REG(hw, E1000_ICRXOC);

	/* Clear PHY statistics registers */
	if ((hw->phy.type == e1000_phy_82578) ||
	    (hw->phy.type == e1000_phy_82579) ||
	    (hw->phy.type == e1000_phy_82577)) {
		hw->phy.ops.read_reg(hw, HV_SCC_UPPER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_SCC_LOWER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_ECOL_UPPER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_ECOL_LOWER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_MCC_UPPER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_MCC_LOWER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_LATECOL_UPPER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_LATECOL_LOWER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_COLC_UPPER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_COLC_LOWER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_DC_UPPER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_DC_LOWER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_TNCRS_UPPER, &phy_data);
		hw->phy.ops.read_reg(hw, HV_TNCRS_LOWER, &phy_data);
	}
}

