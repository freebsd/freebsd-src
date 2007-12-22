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


#ifndef _E1000_PHY_H_
#define _E1000_PHY_H_

typedef enum {
	e1000_ms_hw_default = 0,
	e1000_ms_force_master,
	e1000_ms_force_slave,
	e1000_ms_auto
} e1000_ms_type;

typedef enum {
	e1000_smart_speed_default = 0,
	e1000_smart_speed_on,
	e1000_smart_speed_off
} e1000_smart_speed;

s32  e1000_check_downshift_generic(struct e1000_hw *hw);
s32  e1000_check_polarity_m88(struct e1000_hw *hw);
s32  e1000_check_polarity_igp(struct e1000_hw *hw);
s32  e1000_check_reset_block_generic(struct e1000_hw *hw);
s32  e1000_copper_link_autoneg(struct e1000_hw *hw);
s32  e1000_phy_force_speed_duplex(struct e1000_hw *hw);
s32  e1000_copper_link_setup_igp(struct e1000_hw *hw);
s32  e1000_copper_link_setup_m88(struct e1000_hw *hw);
s32  e1000_phy_force_speed_duplex_igp(struct e1000_hw *hw);
s32  e1000_phy_force_speed_duplex_m88(struct e1000_hw *hw);
s32  e1000_get_cable_length_m88(struct e1000_hw *hw);
s32  e1000_get_cable_length_igp_2(struct e1000_hw *hw);
s32  e1000_get_cfg_done_generic(struct e1000_hw *hw);
s32  e1000_get_phy_id(struct e1000_hw *hw);
s32  e1000_get_phy_info_igp(struct e1000_hw *hw);
s32  e1000_get_phy_info_m88(struct e1000_hw *hw);
s32  e1000_phy_sw_reset_generic(struct e1000_hw *hw);
void e1000_phy_force_speed_duplex_setup(struct e1000_hw *hw, u16 *phy_ctrl);
s32  e1000_phy_hw_reset_generic(struct e1000_hw *hw);
s32  e1000_phy_reset_dsp_generic(struct e1000_hw *hw);
s32  e1000_phy_setup_autoneg(struct e1000_hw *hw);
s32  e1000_read_kmrn_reg_generic(struct e1000_hw *hw, u32 offset, u16 *data);
s32  e1000_read_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 *data);
s32  e1000_read_phy_reg_m88(struct e1000_hw *hw, u32 offset, u16 *data);
s32  e1000_set_d3_lplu_state_generic(struct e1000_hw *hw, bool active);
s32  e1000_setup_copper_link_generic(struct e1000_hw *hw);
s32  e1000_wait_autoneg_generic(struct e1000_hw *hw);
s32  e1000_write_kmrn_reg_generic(struct e1000_hw *hw, u32 offset, u16 data);
s32  e1000_write_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 data);
s32  e1000_write_phy_reg_m88(struct e1000_hw *hw, u32 offset, u16 data);
s32  e1000_phy_reset_dsp(struct e1000_hw *hw);
s32  e1000_phy_has_link_generic(struct e1000_hw *hw, u32 iterations,
                                u32 usec_interval, bool *success);
s32  e1000_phy_init_script_igp3(struct e1000_hw *hw);
e1000_phy_type e1000_get_phy_type_from_id(u32 phy_id);
void e1000_power_up_phy_copper(struct e1000_hw *hw);
void e1000_power_down_phy_copper(struct e1000_hw *hw);

#define E1000_MAX_PHY_ADDR                4

/* IGP01E1000 Specific Registers */
#define IGP01E1000_PHY_PORT_CONFIG        0x10 /* Port Config */
#define IGP01E1000_PHY_PORT_STATUS        0x11 /* Status */
#define IGP01E1000_PHY_PORT_CTRL          0x12 /* Control */
#define IGP01E1000_PHY_LINK_HEALTH        0x13 /* PHY Link Health */
#define IGP01E1000_GMII_FIFO              0x14 /* GMII FIFO */
#define IGP01E1000_PHY_CHANNEL_QUALITY    0x15 /* PHY Channel Quality */
#define IGP02E1000_PHY_POWER_MGMT         0x19 /* Power Management */
#define IGP01E1000_PHY_PAGE_SELECT        0x1F /* Page Select */
#define BM_PHY_PAGE_SELECT                22   /* Page Select for BM */
#define IGP_PAGE_SHIFT                    5
#define PHY_REG_MASK                      0x1F


#define IGP01E1000_PHY_PCS_INIT_REG       0x00B4
#define IGP01E1000_PHY_POLARITY_MASK      0x0078

#define IGP01E1000_PSCR_AUTO_MDIX         0x1000
#define IGP01E1000_PSCR_FORCE_MDI_MDIX    0x2000 /* 0=MDI, 1=MDIX */

#define IGP01E1000_PSCFR_SMART_SPEED      0x0080

/* Enable flexible speed on link-up */
#define IGP01E1000_GMII_FLEX_SPD          0x0010
#define IGP01E1000_GMII_SPD               0x0020 /* Enable SPD */

#define IGP02E1000_PM_SPD                 0x0001 /* Smart Power Down */
#define IGP02E1000_PM_D0_LPLU             0x0002 /* For D0a states */
#define IGP02E1000_PM_D3_LPLU             0x0004 /* For all other states */

#define IGP01E1000_PLHR_SS_DOWNGRADE      0x8000

#define IGP01E1000_PSSR_POLARITY_REVERSED 0x0002
#define IGP01E1000_PSSR_MDIX              0x0008
#define IGP01E1000_PSSR_SPEED_MASK        0xC000
#define IGP01E1000_PSSR_SPEED_1000MBPS    0xC000

#define IGP02E1000_PHY_CHANNEL_NUM        4
#define IGP02E1000_PHY_AGC_A              0x11B1
#define IGP02E1000_PHY_AGC_B              0x12B1
#define IGP02E1000_PHY_AGC_C              0x14B1
#define IGP02E1000_PHY_AGC_D              0x18B1

#define IGP02E1000_AGC_LENGTH_SHIFT       9   /* Course - 15:13, Fine - 12:9 */
#define IGP02E1000_AGC_LENGTH_MASK        0x7F
#define IGP02E1000_AGC_RANGE              15

#define IGP03E1000_PHY_MISC_CTRL          0x1B
#define IGP03E1000_PHY_MISC_DUPLEX_MANUAL_SET  0x1000 /* Manually Set Duplex */

#define E1000_CABLE_LENGTH_UNDEFINED      0xFF

#define E1000_KMRNCTRLSTA_OFFSET          0x001F0000
#define E1000_KMRNCTRLSTA_OFFSET_SHIFT    16
#define E1000_KMRNCTRLSTA_REN             0x00200000
#define E1000_KMRNCTRLSTA_DIAG_OFFSET     0x3    /* Kumeran Diagnostic */
#define E1000_KMRNCTRLSTA_DIAG_NELPBK     0x1000 /* Nearend Loopback mode */

#define IFE_PHY_EXTENDED_STATUS_CONTROL 0x10
#define IFE_PHY_SPECIAL_CONTROL     0x11 /* 100BaseTx PHY Special Control */
#define IFE_PHY_SPECIAL_CONTROL_LED 0x1B /* PHY Special and LED Control */
#define IFE_PHY_MDIX_CONTROL        0x1C /* MDI/MDI-X Control */

/* IFE PHY Extended Status Control */
#define IFE_PESC_POLARITY_REVERSED    0x0100

/* IFE PHY Special Control */
#define IFE_PSC_AUTO_POLARITY_DISABLE      0x0010
#define IFE_PSC_FORCE_POLARITY             0x0020
#define IFE_PSC_DISABLE_DYNAMIC_POWER_DOWN 0x0100

/* IFE PHY Special Control and LED Control */
#define IFE_PSCL_PROBE_MODE            0x0020
#define IFE_PSCL_PROBE_LEDS_OFF        0x0006 /* Force LEDs 0 and 2 off */
#define IFE_PSCL_PROBE_LEDS_ON         0x0007 /* Force LEDs 0 and 2 on */

/* IFE PHY MDIX Control */
#define IFE_PMC_MDIX_STATUS      0x0020 /* 1=MDI-X, 0=MDI */
#define IFE_PMC_FORCE_MDIX       0x0040 /* 1=force MDI-X, 0=force MDI */
#define IFE_PMC_AUTO_MDIX        0x0080 /* 1=enable auto MDI/MDI-X, 0=disable */

#endif
