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

#ifndef _E100_CONFIG_INC_
#define _E100_CONFIG_INC_

#include "e100.h"

#define E100_CONFIG(bdp, X) ((bdp)->config[0] = max_t(u8, (bdp)->config[0], (X)+1))

#define CB_CFIG_MIN_PARAMS         8

/* byte 0 bit definitions*/
#define CB_CFIG_BYTE_COUNT_MASK     BIT_0_5	/* Byte count occupies bit 5-0 */

/* byte 1 bit definitions*/
#define CB_CFIG_RXFIFO_LIMIT_MASK   BIT_0_4	/* RxFifo limit mask */
#define CB_CFIG_TXFIFO_LIMIT_MASK   BIT_4_7	/* TxFifo limit mask */

/* byte 2 bit definitions -- ADAPTIVE_IFS*/

/* word 3 bit definitions -- RESERVED*/
/* Changed for 82558 enhancements */
/* byte 3 bit definitions */
#define CB_CFIG_MWI_EN      BIT_0	/* Enable MWI on PCI bus */
#define CB_CFIG_TYPE_EN     BIT_1	/* Type Enable */
#define CB_CFIG_READAL_EN   BIT_2	/* Enable Read Align */
#define CB_CFIG_TERMCL_EN   BIT_3	/* Cache line write  */

/* byte 4 bit definitions*/
#define CB_CFIG_RX_MIN_DMA_MASK     BIT_0_6	/* Rx minimum DMA count mask */

/* byte 5 bit definitions*/
#define CB_CFIG_TX_MIN_DMA_MASK BIT_0_6	/* Tx minimum DMA count mask */
#define CB_CFIG_DMBC_EN         BIT_7	/* Enable Tx/Rx min. DMA counts */

/* Changed for 82558 enhancements */
/* byte 6 bit definitions*/
#define CB_CFIG_LATE_SCB           BIT_0	/* Update SCB After New Tx Start */
#define CB_CFIG_DIRECT_DMA_DIS     BIT_1	/* Direct DMA mode */
#define CB_CFIG_TNO_INT            BIT_2	/* Tx Not OK Interrupt */
#define CB_CFIG_TCO_STAT           BIT_2	/* TCO statistics in 559 and above */
#define CB_CFIG_CI_INT             BIT_3	/* Command Complete Interrupt */
#define CB_CFIG_EXT_TCB_DIS        BIT_4	/* Extended TCB */
#define CB_CFIG_EXT_STAT_DIS       BIT_5	/* Extended Stats */
#define CB_CFIG_SAVE_BAD_FRAMES    BIT_7	/* Save Bad Frames Enabled */

/* byte 7 bit definitions*/
#define CB_CFIG_DISC_SHORT_FRAMES   BIT_0	/* Discard Short Frames */
#define CB_CFIG_DYNTBD_EN           BIT_7	/* Enable dynamic TBD */
/* Enable extended RFD's on D102 */
#define CB_CFIG_EXTENDED_RFD        BIT_5

/* byte 8 bit definitions*/
#define CB_CFIG_503_MII             BIT_0	/* 503 vs. MII mode */

/* byte 9 bit definitions -- pre-defined all zeros*/
#define CB_LINK_STATUS_WOL	BIT_5

/* byte 10 bit definitions*/
#define CB_CFIG_NO_SRCADR       BIT_3	/* No Source Address Insertion */
#define CB_CFIG_PREAMBLE_LEN    BIT_4_5	/* Preamble Length */
#define CB_CFIG_LOOPBACK_MODE   BIT_6_7	/* Loopback Mode */
#define CB_CFIG_LOOPBACK_NORMAL 0
#define CB_CFIG_LOOPBACK_INTERNAL BIT_6
#define CB_CFIG_LOOPBACK_EXTERNAL BIT_6_7

/* byte 11 bit definitions*/
#define CB_CFIG_LINEAR_PRIORITY     BIT_0_2	/* Linear Priority */

/* byte 12 bit definitions*/
#define CB_CFIG_LINEAR_PRI_MODE     BIT_0	/* Linear Priority mode */
#define CB_CFIG_IFS_MASK            BIT_4_7	/* Interframe Spacing mask */

/* byte 13 bit definitions -- pre-defined all zeros*/

/* byte 14 bit definitions -- pre-defined 0xf2*/

/* byte 15 bit definitions*/
#define CB_CFIG_PROMISCUOUS         BIT_0	/* Promiscuous Mode Enable */
#define CB_CFIG_BROADCAST_DIS       BIT_1	/* Broadcast Mode Disable */
#define CB_CFIG_CRS_OR_CDT          BIT_7	/* CRS Or CDT */

/* byte 16 bit definitions -- pre-defined all zeros*/
#define DFLT_FC_DELAY_LSB  0x1f	/* Delay for outgoing Pause frames */
#define DFLT_NO_FC_DELAY_LSB  0x00	/* no flow control default value */

/* byte 17 bit definitions -- pre-defined 0x40*/
#define DFLT_FC_DELAY_MSB  0x01	/* Delay for outgoing Pause frames */
#define DFLT_NO_FC_DELAY_MSB  0x40	/* no flow control default value */

/* byte 18 bit definitions*/
#define CB_CFIG_STRIPPING           BIT_0	/* Padding Disabled */
#define CB_CFIG_PADDING             BIT_1	/* Padding Disabled */
#define CB_CFIG_CRC_IN_MEM          BIT_2	/* Transfer CRC To Memory */

/* byte 19 bit definitions*/
#define CB_CFIG_TX_ADDR_WAKE        BIT_0	/* Address Wakeup */
#define CB_DISABLE_MAGPAK_WAKE      BIT_1	/* Magic Packet Wakeup disable */
/* Changed TX_FC_EN to TX_FC_DIS because 0 enables, 1 disables. Jul 8, 1999 */
#define CB_CFIG_TX_FC_DIS           BIT_2	/* Tx Flow Control Disable */
#define CB_CFIG_FC_RESTOP           BIT_3	/* Rx Flow Control Restop */
#define CB_CFIG_FC_RESTART          BIT_4	/* Rx Flow Control Restart */
#define CB_CFIG_FC_REJECT           BIT_5	/* Rx Flow Control Restart */
#define CB_CFIG_FC_OPTS (CB_CFIG_FC_RESTOP | CB_CFIG_FC_RESTART | CB_CFIG_FC_REJECT)

/* end 82558/9 specifics */

#define CB_CFIG_FORCE_FDX           BIT_6	/* Force Full Duplex */
#define CB_CFIG_FDX_ENABLE          BIT_7	/* Full Duplex Enabled */

/* byte 20 bit definitions*/
#define CB_CFIG_MULTI_IA            BIT_6	/* Multiple IA Addr */

/* byte 21 bit definitions*/
#define CB_CFIG_MULTICAST_ALL       BIT_3	/* Multicast All */

/* byte 22 bit defines */
#define CB_CFIG_RECEIVE_GAMLA_MODE  BIT_0	/* D102 receive mode */
#define CB_CFIG_VLAN_DROP_ENABLE    BIT_1	/* vlan stripping */

#define CB_CFIG_LONG_RX_OK	    BIT_3

#define NO_LOOPBACK	0	
#define MAC_LOOPBACK	0x01
#define PHY_LOOPBACK	0x02

/* function prototypes */
extern void e100_config_init(struct e100_private *bdp);
extern void e100_config_init_82557(struct e100_private *bdp);
extern unsigned char e100_force_config(struct e100_private *bdp);
extern unsigned char e100_config(struct e100_private *bdp);
extern void e100_config_fc(struct e100_private *bdp);
extern void e100_config_promisc(struct e100_private *bdp, unsigned char enable);
extern void e100_config_brdcast_dsbl(struct e100_private *bdp);
extern void e100_config_mulcast_enbl(struct e100_private *bdp,
				     unsigned char enable);
extern void e100_config_ifs(struct e100_private *bdp);
extern void e100_config_force_dplx(struct e100_private *bdp);
extern u8 e100_config_loopback_mode(struct e100_private *bdp, u8 mode);
extern u8 e100_config_dynamic_tbd(struct e100_private *bdp, u8 enable);
extern u8 e100_config_tcb_ext_enable(struct e100_private *bdp, u8 enable);
extern void e100_config_vlan_drop(struct e100_private *bdp, unsigned char enable);
#endif /* _E100_CONFIG_INC_ */
