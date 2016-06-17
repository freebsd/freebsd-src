/*******************************************************************************

  
  Copyright(c) 1999 - 2004 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
*******************************************************************************/

#ifndef _E100_PHY_INC_
#define _E100_PHY_INC_

#include "e100.h"

/*
 * Auto-polarity enable/disable
 * e100_autopolarity = 0 => disable auto-polarity
 * e100_autopolarity = 1 => enable auto-polarity
 * e100_autopolarity = 2 => let software determine
 */
#define E100_AUTOPOLARITY 2

#define IS_NC3133(bdp) (((bdp)->pdev->subsystem_vendor == 0x0E11) && \
                        ((bdp)->pdev->subsystem_device == 0xB0E1))

#define PHY_503                 0
#define PHY_100_A               0x000003E0
#define PHY_100_C               0x035002A8
#define PHY_NSC_TX              0x5c002000
#define PHY_82562ET             0x033002A8
#define PHY_82562EM             0x032002A8
#define PHY_82562EH             0x017002A8
#define PHY_82555_TX            0x015002a8	/* added this for 82555 */
#define PHY_OTHER               0xFFFF
#define MAX_PHY_ADDR            31
#define MIN_PHY_ADDR            0

#define PHY_MODEL_REV_ID_MASK   0xFFF0FFFF

#define PHY_DEFAULT_ADDRESS 1
#define PHY_ADDRESS_503 32

/* MDI Control register bit definitions */
#define MDI_PHY_READY	    BIT_28	/* PHY is ready for next MDI cycle */

#define MDI_NC3133_CONFIG_REG           0x19
#define MDI_NC3133_100FX_ENABLE         BIT_2
#define MDI_NC3133_INT_ENABLE_REG       0x17
#define MDI_NC3133_INT_ENABLE           BIT_1

/* MDI Control register opcode definitions */
#define MDI_WRITE 1		/* Phy Write */
#define MDI_READ  2		/* Phy read */

/* MDI register set*/
#define AUTO_NEG_NEXT_PAGE_REG	    0x07	/* Auto-negotiation next page xmit */
#define EXTENDED_REG_0		    0x10	/* Extended reg 0 (Phy 100 modes) */
#define EXTENDED_REG_1		    0x14	/* Extended reg 1 (Phy 100 error indications) */
#define NSC_CONG_CONTROL_REG	    0x17	/* National (TX) congestion control */
#define NSC_SPEED_IND_REG	    0x19	/* National (TX) speed indication */

#define HWI_CONTROL_REG             0x1D	/* HWI Control register */
/* MDI/MDI-X Control Register bit definitions */
#define MDI_MDIX_RES_TIMER          BIT_0_3	/* minimum slot time for resolution timer */
#define MDI_MDIX_CONFIG_IS_OK       BIT_4	/* 1 = resolution algorithm completes OK */
#define MDI_MDIX_STATUS             BIT_5	/* 1 = MDIX (croos over), 0 = MDI (straight through) */
#define MDI_MDIX_SWITCH             BIT_6	/* 1 = Forces to MDIX, 0 = Forces to MDI */
#define MDI_MDIX_AUTO_SWITCH_ENABLE BIT_7	/* 1 = MDI/MDI-X feature enabled */
#define MDI_MDIX_CONCT_CONFIG       BIT_8	/* Sets the MDI/MDI-X connectivity configuration (test prupose only) */
#define MDI_MDIX_CONCT_TEST_ENABLE  BIT_9	/* 1 = Enables connectivity testing */
#define MDI_MDIX_RESET_ALL_MASK     0x0000

/* HWI Control Register bit definitions */
#define HWI_TEST_DISTANCE           BIT_0_8	/* distance to cable problem */
#define HWI_TEST_HIGHZ_PROBLEM      BIT_9	/* 1 = Open Circuit */
#define HWI_TEST_LOWZ_PROBLEM       BIT_10	/* 1 = Short Circuit */
#define HWI_TEST_RESERVED           (BIT_11 | BIT_12)	/* reserved */
#define HWI_TEST_EXECUTE            BIT_13	/* 1 = Execute the HWI test on the PHY */
#define HWI_TEST_ABILITY            BIT_14	/* 1 = test passed */
#define HWI_TEST_ENABLE             BIT_15	/* 1 = Enables the HWI feature */
#define HWI_RESET_ALL_MASK          0x0000

/* ############Start of 82555 specific defines################## */

/* Intel 82555 specific registers */
#define PHY_82555_CSR		    0x10	/* 82555 CSR */
#define PHY_82555_SPECIAL_CONTROL   0x11	/* 82555 special control register */

#define PHY_82555_RCV_ERR	    0x15	/* 82555 100BaseTx Receive Error
						 * Frame Counter */
#define PHY_82555_SYMBOL_ERR	    0x16	/* 82555 RCV Symbol Error Counter */
#define PHY_82555_PREM_EOF_ERR	    0x17	/* 82555 100BaseTx RCV Premature End
						 * of Frame Error Counter */
#define PHY_82555_EOF_COUNTER	    0x18	/* 82555 end of frame error counter */
#define PHY_82555_MDI_EQUALIZER_CSR 0x1a	/* 82555 specific equalizer reg. */

/* 82555 CSR bits */
#define PHY_82555_SPEED_BIT    BIT_1
#define PHY_82555_POLARITY_BIT BIT_8

/* 82555 equalizer reg. opcodes */
#define ENABLE_ZERO_FORCING  0x2010	/* write to ASD conf. reg. 0 */
#define DISABLE_ZERO_FORCING 0x2000	/* write to ASD conf. reg. 0 */

/* 82555 special control reg. opcodes */
#define DISABLE_AUTO_POLARITY 0x0010
#define EXTENDED_SQUELCH_BIT  BIT_2

/* ############End of 82555 specific defines##################### */

/* Auto-Negotiation advertisement register bit definitions*/
#define NWAY_AD_FC_SUPPORTED    0x0400	/* Flow Control supported */

/* Auto-Negotiation link partner ability register bit definitions*/
#define NWAY_LP_ABILITY	        0x07e0	/* technologies supported */

/* PHY 100 Extended Register 0 bit definitions*/
#define PHY_100_ER0_FDX_INDIC	BIT_0	/* 1 = FDX, 0 = half duplex */
#define PHY_100_ER0_SPEED_INDIC BIT_1	/* 1 = 100Mbps, 0= 10Mbps */

/* National Semiconductor TX phy congestion control register bit definitions*/
#define NSC_TX_CONG_TXREADY  BIT_10	/* Makes TxReady an input */
#define NSC_TX_CONG_ENABLE   BIT_8	/* Enables congestion control */

/* National Semiconductor TX phy speed indication register bit definitions*/
#define NSC_TX_SPD_INDC_SPEED BIT_6	/* 0 = 100Mbps, 1=10Mbps */

/************* function prototypes ************/
extern unsigned char e100_phy_init(struct e100_private *bdp);
extern unsigned char e100_update_link_state(struct e100_private *bdp);
extern unsigned char e100_phy_check(struct e100_private *bdp);
extern void e100_phy_set_speed_duplex(struct e100_private *bdp,
				      unsigned char force_restart);
extern void e100_phy_autoneg(struct e100_private *bdp);
extern void e100_phy_reset(struct e100_private *bdp);
extern void e100_phy_set_loopback(struct e100_private *bdp);
extern int e100_mdi_write(struct e100_private *, u32, u32, u16);
extern int e100_mdi_read(struct e100_private *, u32, u32, u16 *);

#endif
