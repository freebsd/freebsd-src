/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
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

#ifndef _E1000_ICH8LAN_H_
#define _E1000_ICH8LAN_H_

#define ICH_FLASH_GFPREG                 0x0000
#define ICH_FLASH_HSFSTS                 0x0004
#define ICH_FLASH_HSFCTL                 0x0006
#define ICH_FLASH_FADDR                  0x0008
#define ICH_FLASH_FDATA0                 0x0010

#define ICH_FLASH_READ_COMMAND_TIMEOUT   500
#define ICH_FLASH_WRITE_COMMAND_TIMEOUT  500
#define ICH_FLASH_ERASE_COMMAND_TIMEOUT  3000000
#define ICH_FLASH_LINEAR_ADDR_MASK       0x00FFFFFF
#define ICH_FLASH_CYCLE_REPEAT_COUNT     10

#define ICH_CYCLE_READ                   0
#define ICH_CYCLE_WRITE                  2
#define ICH_CYCLE_ERASE                  3

#define FLASH_GFPREG_BASE_MASK           0x1FFF
#define FLASH_SECTOR_ADDR_SHIFT          12

#define ICH_FLASH_SEG_SIZE_256           256
#define ICH_FLASH_SEG_SIZE_4K            4096
#define ICH_FLASH_SEG_SIZE_8K            8192
#define ICH_FLASH_SEG_SIZE_64K           65536
#define ICH_FLASH_SECTOR_SIZE            4096

#define ICH_FLASH_REG_MAPSIZE            0x00A0

#define E1000_ICH_FWSM_RSPCIPHY          0x00000040 /* Reset PHY on PCI Reset */
#define E1000_ICH_FWSM_DISSW             0x10000000 /* FW Disables SW Writes */
/* FW established a valid mode */
#define E1000_ICH_FWSM_FW_VALID          0x00008000

#define E1000_ICH_MNG_IAMT_MODE          0x2

#define ID_LED_DEFAULT_ICH8LAN  ((ID_LED_DEF1_DEF2 << 12) | \
                                 (ID_LED_DEF1_OFF2 <<  8) | \
                                 (ID_LED_DEF1_ON2  <<  4) | \
                                 (ID_LED_DEF1_DEF2))

#define E1000_ICH_NVM_SIG_WORD           0x13
#define E1000_ICH_NVM_SIG_MASK           0xC000

#define E1000_ICH8_LAN_INIT_TIMEOUT      1500

#define E1000_FEXTNVM_SW_CONFIG        1
#define E1000_FEXTNVM_SW_CONFIG_ICH8M (1 << 27) /* Bit redefined for ICH8M */

#define PCIE_ICH8_SNOOP_ALL   PCIE_NO_SNOOP_ALL

#define E1000_ICH_RAR_ENTRIES            7

#define PHY_PAGE_SHIFT 5
#define PHY_REG(page, reg) (((page) << PHY_PAGE_SHIFT) | \
                           ((reg) & MAX_PHY_REG_ADDRESS))
#define IGP3_KMRN_DIAG  PHY_REG(770, 19) /* KMRN Diagnostic */
#define IGP3_VR_CTRL    PHY_REG(776, 18) /* Voltage Regulator Control */
#define IGP3_CAPABILITY PHY_REG(776, 19) /* Capability */
#define IGP3_PM_CTRL    PHY_REG(769, 20) /* Power Management Control */

#define IGP3_KMRN_DIAG_PCS_LOCK_LOSS         0x0002
#define IGP3_VR_CTRL_DEV_POWERDOWN_MODE_MASK 0x0300
#define IGP3_VR_CTRL_MODE_SHUTDOWN           0x0200
#define IGP3_PM_CTRL_FORCE_PWR_DOWN          0x0020

/*
 * Additional interrupts need to be handled for ICH family:
 *  DSW = The FW changed the status of the DISSW bit in FWSM
 *  PHYINT = The LAN connected device generates an interrupt
 *  EPRST = Manageability reset event
 */
#define IMS_ICH_ENABLE_MASK (\
    E1000_IMS_DSW   | \
    E1000_IMS_PHYINT | \
    E1000_IMS_EPRST)

/* Additional interrupt register bit definitions */
#define E1000_ICR_LSECPNC       0x00004000          /* PN threshold - client */
#define E1000_IMS_LSECPNC       E1000_ICR_LSECPNC   /* PN threshold - client */
#define E1000_ICS_LSECPNC       E1000_ICR_LSECPNC   /* PN threshold - client */

/* Security Processing bit Indication */
#define E1000_RXDEXT_LINKSEC_STATUS_LSECH       0x01000000
#define E1000_RXDEXT_LINKSEC_ERROR_BIT_MASK     0x60000000
#define E1000_RXDEXT_LINKSEC_ERROR_NO_SA_MATCH  0x20000000
#define E1000_RXDEXT_LINKSEC_ERROR_REPLAY_ERROR 0x40000000
#define E1000_RXDEXT_LINKSEC_ERROR_BAD_SIG      0x60000000


void e1000_set_kmrn_lock_loss_workaround_ich8lan(struct e1000_hw *hw,
                                                 bool state);
void e1000_igp3_phy_powerdown_workaround_ich8lan(struct e1000_hw *hw);
void e1000_gig_downshift_workaround_ich8lan(struct e1000_hw *hw);
void e1000_disable_gig_wol_ich8lan(struct e1000_hw *hw);

#endif
