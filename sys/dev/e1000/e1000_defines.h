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
/*$FreeBSD$*/

#ifndef _E1000_DEFINES_H_
#define _E1000_DEFINES_H_

/* Number of Transmit and Receive Descriptors must be a multiple of 8 */
#define REQ_TX_DESCRIPTOR_MULTIPLE  8
#define REQ_RX_DESCRIPTOR_MULTIPLE  8

/* Definitions for power management and wakeup registers */
/* Wake Up Control */
#define E1000_WUC_APME       0x00000001 /* APM Enable */
#define E1000_WUC_PME_EN     0x00000002 /* PME Enable */
#define E1000_WUC_PME_STATUS 0x00000004 /* PME Status */
#define E1000_WUC_APMPME     0x00000008 /* Assert PME on APM Wakeup */
#define E1000_WUC_LSCWE      0x00000010 /* Link Status wake up enable */
#define E1000_WUC_LSCWO      0x00000020 /* Link Status wake up override */
#define E1000_WUC_SPM        0x80000000 /* Enable SPM */
#define E1000_WUC_PHY_WAKE   0x00000100 /* if PHY supports wakeup */
#define E1000_WUC_FLX6_PHY  0x4000 /* Flexible Filter 6 Enable */
#define E1000_WUC_FLX7_PHY  0x8000 /* Flexible Filter 7 Enable */

/* Wake Up Filter Control */
#define E1000_WUFC_LNKC 0x00000001 /* Link Status Change Wakeup Enable */
#define E1000_WUFC_MAG  0x00000002 /* Magic Packet Wakeup Enable */
#define E1000_WUFC_EX   0x00000004 /* Directed Exact Wakeup Enable */
#define E1000_WUFC_MC   0x00000008 /* Directed Multicast Wakeup Enable */
#define E1000_WUFC_BC   0x00000010 /* Broadcast Wakeup Enable */
#define E1000_WUFC_ARP  0x00000020 /* ARP Request Packet Wakeup Enable */
#define E1000_WUFC_IPV4 0x00000040 /* Directed IPv4 Packet Wakeup Enable */
#define E1000_WUFC_IPV6 0x00000080 /* Directed IPv6 Packet Wakeup Enable */
#define E1000_WUFC_IGNORE_TCO_PHY 0x00000800 /* Ignore WakeOn TCO packets */
#define E1000_WUFC_FLX0_PHY      0x00001000 /* Flexible Filter 0 Enable */
#define E1000_WUFC_FLX1_PHY      0x00002000 /* Flexible Filter 1 Enable */
#define E1000_WUFC_FLX2_PHY      0x00004000 /* Flexible Filter 2 Enable */
#define E1000_WUFC_FLX3_PHY      0x00008000 /* Flexible Filter 3 Enable */
#define E1000_WUFC_FLX4_PHY      0x00000200 /* Flexible Filter 4 Enable */
#define E1000_WUFC_FLX5_PHY      0x00000400 /* Flexible Filter 5 Enable */
#define E1000_WUFC_IGNORE_TCO   0x00008000 /* Ignore WakeOn TCO packets */
#define E1000_WUFC_FLX0 0x00010000 /* Flexible Filter 0 Enable */
#define E1000_WUFC_FLX1 0x00020000 /* Flexible Filter 1 Enable */
#define E1000_WUFC_FLX2 0x00040000 /* Flexible Filter 2 Enable */
#define E1000_WUFC_FLX3 0x00080000 /* Flexible Filter 3 Enable */
#define E1000_WUFC_FLX4 0x00100000 /* Flexible Filter 4 Enable */
#define E1000_WUFC_FLX5 0x00200000 /* Flexible Filter 5 Enable */
#define E1000_WUFC_FLX6  0x00400000 /* Flexible Filter 6 Enable */
#define E1000_WUFC_FLX7  0x00800000 /* Flexible Filter 7 Enable */
#define E1000_WUFC_ALL_FILTERS_PHY_4 0x0000F0FF /*Mask for all wakeup filters*/
#define E1000_WUFC_FLX_OFFSET_PHY 12 /* Offset to the Flexible Filters bits */
#define E1000_WUFC_FLX_FILTERS_PHY_4 0x0000F000 /*Mask for 4 flexible filters*/
#define E1000_WUFC_ALL_FILTERS_PHY_6 0x0000F6FF /*Mask for 6 wakeup filters */
#define E1000_WUFC_FLX_FILTERS_PHY_6 0x0000F600 /*Mask for 6 flexible filters*/
#define E1000_WUFC_ALL_FILTERS  0x000F00FF /* Mask for all wakeup filters */
#define E1000_WUFC_ALL_FILTERS_6  0x003F00FF /* Mask for all 6 wakeup filters*/
#define E1000_WUFC_ALL_FILTERS_8  0x00FF00FF /* Mask for all 8 wakeup filters*/
#define E1000_WUFC_FLX_OFFSET   16 /* Offset to the Flexible Filters bits */
#define E1000_WUFC_FLX_FILTERS  0x000F0000 /*Mask for the 4 flexible filters */
#define E1000_WUFC_FLX_FILTERS_6  0x003F0000 /* Mask for 6 flexible filters */
#define E1000_WUFC_FLX_FILTERS_8  0x00FF0000 /* Mask for 8 flexible filters */
/*
 * For 82576 to utilize Extended filter masks in addition to
 * existing (filter) masks
 */
#define E1000_WUFC_EXT_FLX_FILTERS      0x00300000 /* Ext. FLX filter mask */

/* Wake Up Status */
#define E1000_WUS_LNKC         E1000_WUFC_LNKC
#define E1000_WUS_MAG          E1000_WUFC_MAG
#define E1000_WUS_EX           E1000_WUFC_EX
#define E1000_WUS_MC           E1000_WUFC_MC
#define E1000_WUS_BC           E1000_WUFC_BC
#define E1000_WUS_ARP          E1000_WUFC_ARP
#define E1000_WUS_IPV4         E1000_WUFC_IPV4
#define E1000_WUS_IPV6         E1000_WUFC_IPV6
#define E1000_WUS_FLX0_PHY      E1000_WUFC_FLX0_PHY
#define E1000_WUS_FLX1_PHY      E1000_WUFC_FLX1_PHY
#define E1000_WUS_FLX2_PHY      E1000_WUFC_FLX2_PHY
#define E1000_WUS_FLX3_PHY      E1000_WUFC_FLX3_PHY
#define E1000_WUS_FLX_FILTERS_PHY_4        E1000_WUFC_FLX_FILTERS_PHY_4
#define E1000_WUS_FLX0         E1000_WUFC_FLX0
#define E1000_WUS_FLX1         E1000_WUFC_FLX1
#define E1000_WUS_FLX2         E1000_WUFC_FLX2
#define E1000_WUS_FLX3         E1000_WUFC_FLX3
#define E1000_WUS_FLX4         E1000_WUFC_FLX4
#define E1000_WUS_FLX5         E1000_WUFC_FLX5
#define E1000_WUS_FLX6         E1000_WUFC_FLX6
#define E1000_WUS_FLX7         E1000_WUFC_FLX7
#define E1000_WUS_FLX4_PHY         E1000_WUFC_FLX4_PHY
#define E1000_WUS_FLX5_PHY         E1000_WUFC_FLX5_PHY
#define E1000_WUS_FLX6_PHY         0x0400
#define E1000_WUS_FLX7_PHY         0x0800
#define E1000_WUS_FLX_FILTERS  E1000_WUFC_FLX_FILTERS
#define E1000_WUS_FLX_FILTERS_6  E1000_WUFC_FLX_FILTERS_6
#define E1000_WUS_FLX_FILTERS_8  E1000_WUFC_FLX_FILTERS_8
#define E1000_WUS_FLX_FILTERS_PHY_6  E1000_WUFC_FLX_FILTERS_PHY_6

/* Wake Up Packet Length */
#define E1000_WUPL_LENGTH_MASK 0x0FFF   /* Only the lower 12 bits are valid */

/* Four Flexible Filters are supported */
#define E1000_FLEXIBLE_FILTER_COUNT_MAX 4
/* Six Flexible Filters are supported */
#define E1000_FLEXIBLE_FILTER_COUNT_MAX_6   6
/* Eight Flexible Filters are supported */
#define E1000_FLEXIBLE_FILTER_COUNT_MAX_8   8
/* Two Extended Flexible Filters are supported (82576) */
#define E1000_EXT_FLEXIBLE_FILTER_COUNT_MAX     2
#define E1000_FHFT_LENGTH_OFFSET        0xFC /* Length byte in FHFT */
#define E1000_FHFT_LENGTH_MASK          0x0FF /* Length in lower byte */

/* Each Flexible Filter is at most 128 (0x80) bytes in length */
#define E1000_FLEXIBLE_FILTER_SIZE_MAX  128

#define E1000_FFLT_SIZE E1000_FLEXIBLE_FILTER_COUNT_MAX
#define E1000_FFLT_SIZE_6 E1000_FLEXIBLE_FILTER_COUNT_MAX_6
#define E1000_FFLT_SIZE_8 E1000_FLEXIBLE_FILTER_COUNT_MAX_8
#define E1000_FFMT_SIZE E1000_FLEXIBLE_FILTER_SIZE_MAX
#define E1000_FFVT_SIZE E1000_FLEXIBLE_FILTER_SIZE_MAX

/* Extended Device Control */
#define E1000_CTRL_EXT_GPI0_EN   0x00000001 /* Maps SDP4 to GPI0 */
#define E1000_CTRL_EXT_GPI1_EN   0x00000002 /* Maps SDP5 to GPI1 */
#define E1000_CTRL_EXT_PHYINT_EN E1000_CTRL_EXT_GPI1_EN
#define E1000_CTRL_EXT_GPI2_EN   0x00000004 /* Maps SDP6 to GPI2 */
#define E1000_CTRL_EXT_GPI3_EN   0x00000008 /* Maps SDP7 to GPI3 */
/* Reserved (bits 4,5) in >= 82575 */
#define E1000_CTRL_EXT_SDP4_DATA 0x00000010 /* Value of SW Definable Pin 4 */
#define E1000_CTRL_EXT_SDP5_DATA 0x00000020 /* Value of SW Definable Pin 5 */
#define E1000_CTRL_EXT_PHY_INT   E1000_CTRL_EXT_SDP5_DATA
#define E1000_CTRL_EXT_SDP6_DATA 0x00000040 /* Value of SW Definable Pin 6 */
#define E1000_CTRL_EXT_SDP3_DATA 0x00000080 /* Value of SW Definable Pin 3 */
/* SDP 4/5 (bits 8,9) are reserved in >= 82575 */
#define E1000_CTRL_EXT_SDP4_DIR  0x00000100 /* Direction of SDP4 0=in 1=out */
#define E1000_CTRL_EXT_SDP5_DIR  0x00000200 /* Direction of SDP5 0=in 1=out */
#define E1000_CTRL_EXT_SDP6_DIR  0x00000400 /* Direction of SDP6 0=in 1=out */
#define E1000_CTRL_EXT_SDP3_DIR  0x00000800 /* Direction of SDP3 0=in 1=out */
#define E1000_CTRL_EXT_ASDCHK    0x00001000 /* Initiate an ASD sequence */
#define E1000_CTRL_EXT_EE_RST    0x00002000 /* Reinitialize from EEPROM */
#define E1000_CTRL_EXT_IPS       0x00004000 /* Invert Power State */
/* Physical Func Reset Done Indication */
#define E1000_CTRL_EXT_PFRSTD    0x00004000
#define E1000_CTRL_EXT_SPD_BYPS  0x00008000 /* Speed Select Bypass */
#define E1000_CTRL_EXT_RO_DIS    0x00020000 /* Relaxed Ordering disable */
#define E1000_CTRL_EXT_DMA_DYN_CLK_EN 0x00080000 /* DMA Dynamic Clock Gating */
#define E1000_CTRL_EXT_LINK_MODE_MASK 0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_82580_MASK 0x01C00000 /*82580 bit 24:22*/
#define E1000_CTRL_EXT_LINK_MODE_1000BASE_KX  0x00400000
#define E1000_CTRL_EXT_LINK_MODE_GMII 0x00000000
#define E1000_CTRL_EXT_LINK_MODE_TBI  0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_KMRN    0x00000000
#define E1000_CTRL_EXT_LINK_MODE_PCIE_SERDES  0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_PCIX_SERDES  0x00800000
#define E1000_CTRL_EXT_LINK_MODE_SGMII   0x00800000
#define E1000_CTRL_EXT_EIAME          0x01000000
#define E1000_CTRL_EXT_IRCA           0x00000001
#define E1000_CTRL_EXT_WR_WMARK_MASK  0x03000000
#define E1000_CTRL_EXT_WR_WMARK_256   0x00000000
#define E1000_CTRL_EXT_WR_WMARK_320   0x01000000
#define E1000_CTRL_EXT_WR_WMARK_384   0x02000000
#define E1000_CTRL_EXT_WR_WMARK_448   0x03000000
#define E1000_CTRL_EXT_CANC           0x04000000 /* Int delay cancellation */
#define E1000_CTRL_EXT_DRV_LOAD       0x10000000 /* Driver loaded bit for FW */
/* IAME enable bit (27) was removed in >= 82575 */
#define E1000_CTRL_EXT_IAME          0x08000000 /* Int acknowledge Auto-mask */
#define E1000_CRTL_EXT_PB_PAREN       0x01000000 /* packet buffer parity error
                                                  * detection enabled */
#define E1000_CTRL_EXT_DF_PAREN       0x02000000 /* descriptor FIFO parity
                                                  * error detection enable */
#define E1000_CTRL_EXT_GHOST_PAREN    0x40000000
#define E1000_CTRL_EXT_PBA_CLR        0x80000000 /* PBA Clear */
#define E1000_CTRL_EXT_LSECCK         0x00001000
#define E1000_CTRL_EXT_PHYPDEN        0x00100000
#define E1000_I2CCMD_REG_ADDR_SHIFT   16
#define E1000_I2CCMD_REG_ADDR         0x00FF0000
#define E1000_I2CCMD_PHY_ADDR_SHIFT   24
#define E1000_I2CCMD_PHY_ADDR         0x07000000
#define E1000_I2CCMD_OPCODE_READ      0x08000000
#define E1000_I2CCMD_OPCODE_WRITE     0x00000000
#define E1000_I2CCMD_RESET            0x10000000
#define E1000_I2CCMD_READY            0x20000000
#define E1000_I2CCMD_INTERRUPT_ENA    0x40000000
#define E1000_I2CCMD_ERROR            0x80000000
#define E1000_MAX_SGMII_PHY_REG_ADDR  255
#define E1000_I2CCMD_PHY_TIMEOUT      200
#define E1000_IVAR_VALID        0x80
#define E1000_GPIE_NSICR        0x00000001
#define E1000_GPIE_MSIX_MODE    0x00000010
#define E1000_GPIE_EIAME        0x40000000
#define E1000_GPIE_PBA          0x80000000

/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */
#define E1000_RXD_STAT_IXSM     0x04    /* Ignore checksum */
#define E1000_RXD_STAT_VP       0x08    /* IEEE VLAN Packet */
#define E1000_RXD_STAT_UDPCS    0x10    /* UDP xsum calculated */
#define E1000_RXD_STAT_TCPCS    0x20    /* TCP xsum calculated */
#define E1000_RXD_STAT_IPCS     0x40    /* IP xsum calculated */
#define E1000_RXD_STAT_PIF      0x80    /* passed in-exact filter */
#define E1000_RXD_STAT_CRCV     0x100   /* Speculative CRC Valid */
#define E1000_RXD_STAT_IPIDV    0x200   /* IP identification valid */
#define E1000_RXD_STAT_UDPV     0x400   /* Valid UDP checksum */
#define E1000_RXD_STAT_DYNINT   0x800   /* Pkt caused INT via DYNINT */
#define E1000_RXD_STAT_ACK      0x8000  /* ACK Packet indication */
#define E1000_RXD_ERR_CE        0x01    /* CRC Error */
#define E1000_RXD_ERR_SE        0x02    /* Symbol Error */
#define E1000_RXD_ERR_SEQ       0x04    /* Sequence Error */
#define E1000_RXD_ERR_CXE       0x10    /* Carrier Extension Error */
#define E1000_RXD_ERR_TCPE      0x20    /* TCP/UDP Checksum Error */
#define E1000_RXD_ERR_IPE       0x40    /* IP Checksum Error */
#define E1000_RXD_ERR_RXE       0x80    /* Rx Data Error */
#define E1000_RXD_SPC_VLAN_MASK 0x0FFF  /* VLAN ID is in lower 12 bits */
#define E1000_RXD_SPC_PRI_MASK  0xE000  /* Priority is in upper 3 bits */
#define E1000_RXD_SPC_PRI_SHIFT 13
#define E1000_RXD_SPC_CFI_MASK  0x1000  /* CFI is bit 12 */
#define E1000_RXD_SPC_CFI_SHIFT 12

#define E1000_RXDEXT_STATERR_CE    0x01000000
#define E1000_RXDEXT_STATERR_SE    0x02000000
#define E1000_RXDEXT_STATERR_SEQ   0x04000000
#define E1000_RXDEXT_STATERR_CXE   0x10000000
#define E1000_RXDEXT_STATERR_TCPE  0x20000000
#define E1000_RXDEXT_STATERR_IPE   0x40000000
#define E1000_RXDEXT_STATERR_RXE   0x80000000

#define E1000_RXDEXT_LSECH                0x01000000
#define E1000_RXDEXT_LSECE_MASK           0x60000000
#define E1000_RXDEXT_LSECE_NO_ERROR       0x00000000
#define E1000_RXDEXT_LSECE_NO_SA_MATCH    0x20000000
#define E1000_RXDEXT_LSECE_REPLAY_DETECT  0x40000000
#define E1000_RXDEXT_LSECE_BAD_SIG        0x60000000

/* mask to determine if packets should be dropped due to frame errors */
#define E1000_RXD_ERR_FRAME_ERR_MASK ( \
    E1000_RXD_ERR_CE  |                \
    E1000_RXD_ERR_SE  |                \
    E1000_RXD_ERR_SEQ |                \
    E1000_RXD_ERR_CXE |                \
    E1000_RXD_ERR_RXE)

/* Same mask, but for extended and packet split descriptors */
#define E1000_RXDEXT_ERR_FRAME_ERR_MASK ( \
    E1000_RXDEXT_STATERR_CE  |            \
    E1000_RXDEXT_STATERR_SE  |            \
    E1000_RXDEXT_STATERR_SEQ |            \
    E1000_RXDEXT_STATERR_CXE |            \
    E1000_RXDEXT_STATERR_RXE)

#define E1000_MRQC_ENABLE_MASK                 0x00000007
#define E1000_MRQC_ENABLE_RSS_2Q               0x00000001
#define E1000_MRQC_ENABLE_RSS_INT              0x00000004
#define E1000_MRQC_RSS_FIELD_MASK              0xFFFF0000
#define E1000_MRQC_RSS_FIELD_IPV4_TCP          0x00010000
#define E1000_MRQC_RSS_FIELD_IPV4              0x00020000
#define E1000_MRQC_RSS_FIELD_IPV6_TCP_EX       0x00040000
#define E1000_MRQC_RSS_FIELD_IPV6_EX           0x00080000
#define E1000_MRQC_RSS_FIELD_IPV6              0x00100000
#define E1000_MRQC_RSS_FIELD_IPV6_TCP          0x00200000

#define E1000_RXDPS_HDRSTAT_HDRSP              0x00008000
#define E1000_RXDPS_HDRSTAT_HDRLEN_MASK        0x000003FF

/* Management Control */
#define E1000_MANC_SMBUS_EN      0x00000001 /* SMBus Enabled - RO */
#define E1000_MANC_ASF_EN        0x00000002 /* ASF Enabled - RO */
#define E1000_MANC_R_ON_FORCE    0x00000004 /* Reset on Force TCO - RO */
#define E1000_MANC_RMCP_EN       0x00000100 /* Enable RCMP 026Fh Filtering */
#define E1000_MANC_0298_EN       0x00000200 /* Enable RCMP 0298h Filtering */
#define E1000_MANC_IPV4_EN       0x00000400 /* Enable IPv4 */
#define E1000_MANC_IPV6_EN       0x00000800 /* Enable IPv6 */
#define E1000_MANC_SNAP_EN       0x00001000 /* Accept LLC/SNAP */
#define E1000_MANC_ARP_EN        0x00002000 /* Enable ARP Request Filtering */
/* Enable Neighbor Discovery Filtering */
#define E1000_MANC_NEIGHBOR_EN   0x00004000
#define E1000_MANC_ARP_RES_EN    0x00008000 /* Enable ARP response Filtering */
#define E1000_MANC_TCO_RESET     0x00010000 /* TCO Reset Occurred */
#define E1000_MANC_RCV_TCO_EN    0x00020000 /* Receive TCO Packets Enabled */
#define E1000_MANC_REPORT_STATUS 0x00040000 /* Status Reporting Enabled */
#define E1000_MANC_RCV_ALL       0x00080000 /* Receive All Enabled */
#define E1000_MANC_BLK_PHY_RST_ON_IDE   0x00040000 /* Block phy resets */
/* Enable MAC address filtering */
#define E1000_MANC_EN_MAC_ADDR_FILTER   0x00100000
/* Enable MNG packets to host memory */
#define E1000_MANC_EN_MNG2HOST   0x00200000
/* Enable IP address filtering */
#define E1000_MANC_EN_IP_ADDR_FILTER    0x00400000
#define E1000_MANC_EN_XSUM_FILTER   0x00800000 /* Enable checksum filtering */
#define E1000_MANC_BR_EN            0x01000000 /* Enable broadcast filtering */
#define E1000_MANC_SMB_REQ       0x01000000 /* SMBus Request */
#define E1000_MANC_SMB_GNT       0x02000000 /* SMBus Grant */
#define E1000_MANC_SMB_CLK_IN    0x04000000 /* SMBus Clock In */
#define E1000_MANC_SMB_DATA_IN   0x08000000 /* SMBus Data In */
#define E1000_MANC_SMB_DATA_OUT  0x10000000 /* SMBus Data Out */
#define E1000_MANC_SMB_CLK_OUT   0x20000000 /* SMBus Clock Out */

#define E1000_MANC_SMB_DATA_OUT_SHIFT  28 /* SMBus Data Out Shift */
#define E1000_MANC_SMB_CLK_OUT_SHIFT   29 /* SMBus Clock Out Shift */

#define E1000_MANC2H_PORT_623    0x00000020 /* Port 0x26f */
#define E1000_MANC2H_PORT_664    0x00000040 /* Port 0x298 */
#define E1000_MDEF_PORT_623      0x00000800 /* Port 0x26f */
#define E1000_MDEF_PORT_664      0x00000400 /* Port 0x298 */

/* Receive Control */
#define E1000_RCTL_RST            0x00000001    /* Software reset */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SBP            0x00000004    /* store bad packet */
#define E1000_RCTL_UPE            0x00000008    /* unicast promisc enable */
#define E1000_RCTL_MPE            0x00000010    /* multicast promisc enable */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_LBM_MAC        0x00000040    /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP        0x00000080    /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR       0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
#define E1000_RCTL_DTYP_PS        0x00000400    /* Packet Split descriptor */
#define E1000_RCTL_RDMTS_HALF     0x00000000    /* rx desc min thresh size */
#define E1000_RCTL_RDMTS_QUAT     0x00000100    /* rx desc min thresh size */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min thresh size */
#define E1000_RCTL_MO_SHIFT       12            /* multicast offset shift */
#define E1000_RCTL_MO_0           0x00000000    /* multicast offset 11:0 */
#define E1000_RCTL_MO_1           0x00001000    /* multicast offset 12:1 */
#define E1000_RCTL_MO_2           0x00002000    /* multicast offset 13:2 */
#define E1000_RCTL_MO_3           0x00003000    /* multicast offset 15:4 */
#define E1000_RCTL_MDR            0x00004000    /* multicast desc ring 0 */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384       0x00010000    /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192        0x00020000    /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096        0x00030000    /* rx buffer size 4096 */
#define E1000_RCTL_VFE            0x00040000    /* vlan filter enable */
#define E1000_RCTL_CFIEN          0x00080000    /* canonical form enable */
#define E1000_RCTL_CFI            0x00100000    /* canonical form indicator */
#define E1000_RCTL_DPF            0x00400000    /* discard pause frames */
#define E1000_RCTL_PMCF           0x00800000    /* pass MAC control frames */
#define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_FLXBUF_MASK    0x78000000    /* Flexible buffer size */
#define E1000_RCTL_FLXBUF_SHIFT   27            /* Flexible buffer shift */

/*
 * Use byte values for the following shift parameters
 * Usage:
 *     psrctl |= (((ROUNDUP(value0, 128) >> E1000_PSRCTL_BSIZE0_SHIFT) &
 *                  E1000_PSRCTL_BSIZE0_MASK) |
 *                ((ROUNDUP(value1, 1024) >> E1000_PSRCTL_BSIZE1_SHIFT) &
 *                  E1000_PSRCTL_BSIZE1_MASK) |
 *                ((ROUNDUP(value2, 1024) << E1000_PSRCTL_BSIZE2_SHIFT) &
 *                  E1000_PSRCTL_BSIZE2_MASK) |
 *                ((ROUNDUP(value3, 1024) << E1000_PSRCTL_BSIZE3_SHIFT) |;
 *                  E1000_PSRCTL_BSIZE3_MASK))
 * where value0 = [128..16256],  default=256
 *       value1 = [1024..64512], default=4096
 *       value2 = [0..64512],    default=4096
 *       value3 = [0..64512],    default=0
 */

#define E1000_PSRCTL_BSIZE0_MASK   0x0000007F
#define E1000_PSRCTL_BSIZE1_MASK   0x00003F00
#define E1000_PSRCTL_BSIZE2_MASK   0x003F0000
#define E1000_PSRCTL_BSIZE3_MASK   0x3F000000

#define E1000_PSRCTL_BSIZE0_SHIFT  7            /* Shift _right_ 7 */
#define E1000_PSRCTL_BSIZE1_SHIFT  2            /* Shift _right_ 2 */
#define E1000_PSRCTL_BSIZE2_SHIFT  6            /* Shift _left_ 6 */
#define E1000_PSRCTL_BSIZE3_SHIFT 14            /* Shift _left_ 14 */

/* SWFW_SYNC Definitions */
#define E1000_SWFW_EEP_SM   0x01
#define E1000_SWFW_PHY0_SM  0x02
#define E1000_SWFW_PHY1_SM  0x04
#define E1000_SWFW_CSR_SM   0x08
#define E1000_SWFW_PHY2_SM  0x20
#define E1000_SWFW_PHY3_SM  0x40

/* FACTPS Definitions */
#define E1000_FACTPS_LFS    0x40000000  /* LAN Function Select */
/* Device Control */
#define E1000_CTRL_FD       0x00000001  /* Full duplex.0=half; 1=full */
#define E1000_CTRL_BEM      0x00000002  /* Endian Mode.0=little,1=big */
#define E1000_CTRL_PRIOR    0x00000004  /* Priority on PCI. 0=rx,1=fair */
#define E1000_CTRL_GIO_MASTER_DISABLE 0x00000004 /*Blocks new Master reqs */
#define E1000_CTRL_LRST     0x00000008  /* Link reset. 0=normal,1=reset */
#define E1000_CTRL_TME      0x00000010  /* Test mode. 0=normal,1=test */
#define E1000_CTRL_SLE      0x00000020  /* Serial Link on 0=dis,1=en */
#define E1000_CTRL_ASDE     0x00000020  /* Auto-speed detect enable */
#define E1000_CTRL_SLU      0x00000040  /* Set link up (Force Link) */
#define E1000_CTRL_ILOS     0x00000080  /* Invert Loss-Of Signal */
#define E1000_CTRL_SPD_SEL  0x00000300  /* Speed Select Mask */
#define E1000_CTRL_SPD_10   0x00000000  /* Force 10Mb */
#define E1000_CTRL_SPD_100  0x00000100  /* Force 100Mb */
#define E1000_CTRL_SPD_1000 0x00000200  /* Force 1Gb */
#define E1000_CTRL_BEM32    0x00000400  /* Big Endian 32 mode */
#define E1000_CTRL_FRCSPD   0x00000800  /* Force Speed */
#define E1000_CTRL_FRCDPX   0x00001000  /* Force Duplex */
#define E1000_CTRL_D_UD_EN  0x00002000  /* Dock/Undock enable */
#define E1000_CTRL_D_UD_POLARITY 0x00004000 /* Defined polarity of Dock/Undock
                                             * indication in SDP[0] */
#define E1000_CTRL_FORCE_PHY_RESET 0x00008000 /* Reset both PHY ports, through
                                               * PHYRST_N pin */
#define E1000_CTRL_EXT_LINK_EN 0x00010000 /* enable link status from external
                                           * LINK_0 and LINK_1 pins */
#define E1000_CTRL_LANPHYPC_OVERRIDE 0x00010000 /* SW control of LANPHYPC */
#define E1000_CTRL_LANPHYPC_VALUE    0x00020000 /* SW value of LANPHYPC */
#define E1000_CTRL_SWDPIN0  0x00040000  /* SWDPIN 0 value */
#define E1000_CTRL_SWDPIN1  0x00080000  /* SWDPIN 1 value */
#define E1000_CTRL_SWDPIN2  0x00100000  /* SWDPIN 2 value */
#define E1000_CTRL_ADVD3WUC 0x00100000  /* D3 WUC */
#define E1000_CTRL_SWDPIN3  0x00200000  /* SWDPIN 3 value */
#define E1000_CTRL_SWDPIO0  0x00400000  /* SWDPIN 0 Input or output */
#define E1000_CTRL_SWDPIO1  0x00800000  /* SWDPIN 1 input or output */
#define E1000_CTRL_SWDPIO2  0x01000000  /* SWDPIN 2 input or output */
#define E1000_CTRL_SWDPIO3  0x02000000  /* SWDPIN 3 input or output */
#define E1000_CTRL_RST      0x04000000  /* Global reset */
#define E1000_CTRL_RFCE     0x08000000  /* Receive Flow Control enable */
#define E1000_CTRL_TFCE     0x10000000  /* Transmit flow control enable */
#define E1000_CTRL_RTE      0x20000000  /* Routing tag enable */
#define E1000_CTRL_VME      0x40000000  /* IEEE VLAN mode enable */
#define E1000_CTRL_PHY_RST  0x80000000  /* PHY Reset */
#define E1000_CTRL_SW2FW_INT 0x02000000 /* Initiate an interrupt to ME */
#define E1000_CTRL_I2C_ENA  0x02000000  /* I2C enable */

/*
 * Bit definitions for the Management Data IO (MDIO) and Management Data
 * Clock (MDC) pins in the Device Control Register.
 */
#define E1000_CTRL_PHY_RESET_DIR  E1000_CTRL_SWDPIO0
#define E1000_CTRL_PHY_RESET      E1000_CTRL_SWDPIN0
#define E1000_CTRL_MDIO_DIR       E1000_CTRL_SWDPIO2
#define E1000_CTRL_MDIO           E1000_CTRL_SWDPIN2
#define E1000_CTRL_MDC_DIR        E1000_CTRL_SWDPIO3
#define E1000_CTRL_MDC            E1000_CTRL_SWDPIN3
#define E1000_CTRL_PHY_RESET_DIR4 E1000_CTRL_EXT_SDP4_DIR
#define E1000_CTRL_PHY_RESET4     E1000_CTRL_EXT_SDP4_DATA

#define E1000_CONNSW_ENRGSRC             0x4
#define E1000_PCS_CFG_PCS_EN             8
#define E1000_PCS_LCTL_FLV_LINK_UP       1
#define E1000_PCS_LCTL_FSV_10            0
#define E1000_PCS_LCTL_FSV_100           2
#define E1000_PCS_LCTL_FSV_1000          4
#define E1000_PCS_LCTL_FDV_FULL          8
#define E1000_PCS_LCTL_FSD               0x10
#define E1000_PCS_LCTL_FORCE_LINK        0x20
#define E1000_PCS_LCTL_LOW_LINK_LATCH    0x40
#define E1000_PCS_LCTL_FORCE_FCTRL       0x80
#define E1000_PCS_LCTL_AN_ENABLE         0x10000
#define E1000_PCS_LCTL_AN_RESTART        0x20000
#define E1000_PCS_LCTL_AN_TIMEOUT        0x40000
#define E1000_PCS_LCTL_AN_SGMII_BYPASS   0x80000
#define E1000_PCS_LCTL_AN_SGMII_TRIGGER  0x100000
#define E1000_PCS_LCTL_FAST_LINK_TIMER   0x1000000
#define E1000_PCS_LCTL_LINK_OK_FIX       0x2000000
#define E1000_PCS_LCTL_CRS_ON_NI         0x4000000
#define E1000_ENABLE_SERDES_LOOPBACK     0x0410

#define E1000_PCS_LSTS_LINK_OK           1
#define E1000_PCS_LSTS_SPEED_10          0
#define E1000_PCS_LSTS_SPEED_100         2
#define E1000_PCS_LSTS_SPEED_1000        4
#define E1000_PCS_LSTS_DUPLEX_FULL       8
#define E1000_PCS_LSTS_SYNK_OK           0x10
#define E1000_PCS_LSTS_AN_COMPLETE       0x10000
#define E1000_PCS_LSTS_AN_PAGE_RX        0x20000
#define E1000_PCS_LSTS_AN_TIMED_OUT      0x40000
#define E1000_PCS_LSTS_AN_REMOTE_FAULT   0x80000
#define E1000_PCS_LSTS_AN_ERROR_RWS      0x100000

/* Device Status */
#define E1000_STATUS_FD         0x00000001      /* Full duplex.0=half,1=full */
#define E1000_STATUS_LU         0x00000002      /* Link up.0=no,1=link */
#define E1000_STATUS_FUNC_MASK  0x0000000C      /* PCI Function Mask */
#define E1000_STATUS_FUNC_SHIFT 2
#define E1000_STATUS_FUNC_0     0x00000000      /* Function 0 */
#define E1000_STATUS_FUNC_1     0x00000004      /* Function 1 */
#define E1000_STATUS_TXOFF      0x00000010      /* transmission paused */
#define E1000_STATUS_TBIMODE    0x00000020      /* TBI mode */
#define E1000_STATUS_SPEED_MASK 0x000000C0
#define E1000_STATUS_SPEED_10   0x00000000      /* Speed 10Mb/s */
#define E1000_STATUS_SPEED_100  0x00000040      /* Speed 100Mb/s */
#define E1000_STATUS_SPEED_1000 0x00000080      /* Speed 1000Mb/s */
#define E1000_STATUS_LAN_INIT_DONE 0x00000200  /* Lan Init Completion by NVM */
#define E1000_STATUS_ASDV       0x00000300      /* Auto speed detect value */
#define E1000_STATUS_PHYRA      0x00000400      /* PHY Reset Asserted */
#define E1000_STATUS_DOCK_CI    0x00000800      /* Change in Dock/Undock state.
                                                 * Clear on write '0'. */
#define E1000_STATUS_GIO_MASTER_ENABLE 0x00080000 /* Master request status */
#define E1000_STATUS_MTXCKOK    0x00000400      /* MTX clock running OK */
#define E1000_STATUS_PCI66      0x00000800      /* In 66Mhz slot */
#define E1000_STATUS_BUS64      0x00001000      /* In 64 bit slot */
#define E1000_STATUS_PCIX_MODE  0x00002000      /* PCI-X mode */
#define E1000_STATUS_PCIX_SPEED 0x0000C000      /* PCI-X bus speed */
#define E1000_STATUS_BMC_SKU_0  0x00100000 /* BMC USB redirect disabled */
#define E1000_STATUS_BMC_SKU_1  0x00200000 /* BMC SRAM disabled */
#define E1000_STATUS_BMC_SKU_2  0x00400000 /* BMC SDRAM disabled */
#define E1000_STATUS_BMC_CRYPTO 0x00800000 /* BMC crypto disabled */
#define E1000_STATUS_BMC_LITE   0x01000000 /* BMC external code execution
                                            * disabled */
#define E1000_STATUS_RGMII_ENABLE 0x02000000 /* RGMII disabled */
#define E1000_STATUS_FUSE_8       0x04000000
#define E1000_STATUS_FUSE_9       0x08000000
#define E1000_STATUS_SERDES0_DIS  0x10000000 /* SERDES disabled on port 0 */
#define E1000_STATUS_SERDES1_DIS  0x20000000 /* SERDES disabled on port 1 */

/* Constants used to interpret the masked PCI-X bus speed. */
#define E1000_STATUS_PCIX_SPEED_66  0x00000000 /* PCI-X bus speed 50-66 MHz */
#define E1000_STATUS_PCIX_SPEED_100 0x00004000 /* PCI-X bus speed 66-100 MHz */
#define E1000_STATUS_PCIX_SPEED_133 0x00008000 /*PCI-X bus speed 100-133 MHz*/

#define SPEED_10    10
#define SPEED_100   100
#define SPEED_1000  1000
#define HALF_DUPLEX 1
#define FULL_DUPLEX 2

#define PHY_FORCE_TIME   20

#define ADVERTISE_10_HALF                 0x0001
#define ADVERTISE_10_FULL                 0x0002
#define ADVERTISE_100_HALF                0x0004
#define ADVERTISE_100_FULL                0x0008
#define ADVERTISE_1000_HALF               0x0010 /* Not used, just FYI */
#define ADVERTISE_1000_FULL               0x0020

/* 1000/H is not supported, nor spec-compliant. */
#define E1000_ALL_SPEED_DUPLEX  (ADVERTISE_10_HALF |   ADVERTISE_10_FULL | \
                                ADVERTISE_100_HALF |  ADVERTISE_100_FULL | \
                                                     ADVERTISE_1000_FULL)
#define E1000_ALL_NOT_GIG       (ADVERTISE_10_HALF |   ADVERTISE_10_FULL | \
                                ADVERTISE_100_HALF |  ADVERTISE_100_FULL)
#define E1000_ALL_100_SPEED    (ADVERTISE_100_HALF |  ADVERTISE_100_FULL)
#define E1000_ALL_10_SPEED      (ADVERTISE_10_HALF |   ADVERTISE_10_FULL)
#define E1000_ALL_FULL_DUPLEX   (ADVERTISE_10_FULL |  ADVERTISE_100_FULL | \
                                                     ADVERTISE_1000_FULL)
#define E1000_ALL_HALF_DUPLEX   (ADVERTISE_10_HALF |  ADVERTISE_100_HALF)

#define AUTONEG_ADVERTISE_SPEED_DEFAULT   E1000_ALL_SPEED_DUPLEX

/* LED Control */
#define E1000_PHY_LED0_MODE_MASK          0x00000007
#define E1000_PHY_LED0_IVRT               0x00000008
#define E1000_PHY_LED0_BLINK              0x00000010
#define E1000_PHY_LED0_MASK               0x0000001F

#define E1000_LEDCTL_LED0_MODE_MASK       0x0000000F
#define E1000_LEDCTL_LED0_MODE_SHIFT      0
#define E1000_LEDCTL_LED0_BLINK_RATE      0x00000020
#define E1000_LEDCTL_LED0_IVRT            0x00000040
#define E1000_LEDCTL_LED0_BLINK           0x00000080
#define E1000_LEDCTL_LED1_MODE_MASK       0x00000F00
#define E1000_LEDCTL_LED1_MODE_SHIFT      8
#define E1000_LEDCTL_LED1_BLINK_RATE      0x00002000
#define E1000_LEDCTL_LED1_IVRT            0x00004000
#define E1000_LEDCTL_LED1_BLINK           0x00008000
#define E1000_LEDCTL_LED2_MODE_MASK       0x000F0000
#define E1000_LEDCTL_LED2_MODE_SHIFT      16
#define E1000_LEDCTL_LED2_BLINK_RATE      0x00200000
#define E1000_LEDCTL_LED2_IVRT            0x00400000
#define E1000_LEDCTL_LED2_BLINK           0x00800000
#define E1000_LEDCTL_LED3_MODE_MASK       0x0F000000
#define E1000_LEDCTL_LED3_MODE_SHIFT      24
#define E1000_LEDCTL_LED3_BLINK_RATE      0x20000000
#define E1000_LEDCTL_LED3_IVRT            0x40000000
#define E1000_LEDCTL_LED3_BLINK           0x80000000

#define E1000_LEDCTL_MODE_LINK_10_1000  0x0
#define E1000_LEDCTL_MODE_LINK_100_1000 0x1
#define E1000_LEDCTL_MODE_LINK_UP       0x2
#define E1000_LEDCTL_MODE_ACTIVITY      0x3
#define E1000_LEDCTL_MODE_LINK_ACTIVITY 0x4
#define E1000_LEDCTL_MODE_LINK_10       0x5
#define E1000_LEDCTL_MODE_LINK_100      0x6
#define E1000_LEDCTL_MODE_LINK_1000     0x7
#define E1000_LEDCTL_MODE_PCIX_MODE     0x8
#define E1000_LEDCTL_MODE_FULL_DUPLEX   0x9
#define E1000_LEDCTL_MODE_COLLISION     0xA
#define E1000_LEDCTL_MODE_BUS_SPEED     0xB
#define E1000_LEDCTL_MODE_BUS_SIZE      0xC
#define E1000_LEDCTL_MODE_PAUSED        0xD
#define E1000_LEDCTL_MODE_LED_ON        0xE
#define E1000_LEDCTL_MODE_LED_OFF       0xF

/* Transmit Descriptor bit definitions */
#define E1000_TXD_DTYP_D     0x00100000 /* Data Descriptor */
#define E1000_TXD_DTYP_C     0x00000000 /* Context Descriptor */
#define E1000_TXD_POPTS_SHIFT 8         /* POPTS shift */
#define E1000_TXD_POPTS_IXSM 0x01       /* Insert IP checksum */
#define E1000_TXD_POPTS_TXSM 0x02       /* Insert TCP/UDP checksum */
#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02000000 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04000000 /* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */
#define E1000_TXD_CMD_RPS    0x10000000 /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20000000 /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40000000 /* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80000000 /* Enable Tidv register */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_STAT_EC    0x00000002 /* Excess Collisions */
#define E1000_TXD_STAT_LC    0x00000004 /* Late Collisions */
#define E1000_TXD_STAT_TU    0x00000008 /* Transmit underrun */
#define E1000_TXD_CMD_TCP    0x01000000 /* TCP packet */
#define E1000_TXD_CMD_IP     0x02000000 /* IP packet */
#define E1000_TXD_CMD_TSE    0x04000000 /* TCP Seg enable */
#define E1000_TXD_STAT_TC    0x00000004 /* Tx Underrun */
/* Extended desc bits for Linksec and timesync */
#define E1000_TXD_CMD_LINKSEC     0x10000000 /* Apply LinkSec on packet */
#define E1000_TXD_EXTCMD_TSTAMP   0x00000010 /* IEEE1588 Timestamp packet */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* Transmit Arbitration Count */
#define E1000_TARC0_ENABLE     0x00000400   /* Enable Tx Queue 0 */

/* SerDes Control */
#define E1000_SCTL_DISABLE_SERDES_LOOPBACK 0x0400

/* Receive Checksum Control */
#define E1000_RXCSUM_PCSS_MASK 0x000000FF   /* Packet Checksum Start */
#define E1000_RXCSUM_IPOFL     0x00000100   /* IPv4 checksum offload */
#define E1000_RXCSUM_TUOFL     0x00000200   /* TCP / UDP checksum offload */
#define E1000_RXCSUM_IPV6OFL   0x00000400   /* IPv6 checksum offload */
#define E1000_RXCSUM_CRCOFL    0x00000800   /* CRC32 offload enable */
#define E1000_RXCSUM_IPPCSE    0x00001000   /* IP payload checksum enable */
#define E1000_RXCSUM_PCSD      0x00002000   /* packet checksum disabled */

/* Header split receive */
#define E1000_RFCTL_ISCSI_DIS           0x00000001
#define E1000_RFCTL_ISCSI_DWC_MASK      0x0000003E
#define E1000_RFCTL_ISCSI_DWC_SHIFT     1
#define E1000_RFCTL_NFSW_DIS            0x00000040
#define E1000_RFCTL_NFSR_DIS            0x00000080
#define E1000_RFCTL_NFS_VER_MASK        0x00000300
#define E1000_RFCTL_NFS_VER_SHIFT       8
#define E1000_RFCTL_IPV6_DIS            0x00000400
#define E1000_RFCTL_IPV6_XSUM_DIS       0x00000800
#define E1000_RFCTL_ACK_DIS             0x00001000
#define E1000_RFCTL_ACKD_DIS            0x00002000
#define E1000_RFCTL_IPFRSP_DIS          0x00004000
#define E1000_RFCTL_EXTEN               0x00008000
#define E1000_RFCTL_IPV6_EX_DIS         0x00010000
#define E1000_RFCTL_NEW_IPV6_EXT_DIS    0x00020000
#define E1000_RFCTL_LEF                 0x00040000

/* Collision related configuration parameters */
#define E1000_COLLISION_THRESHOLD       15
#define E1000_CT_SHIFT                  4
#define E1000_COLLISION_DISTANCE        63
#define E1000_COLD_SHIFT                12

/* Default values for the transmit IPG register */
#define DEFAULT_82542_TIPG_IPGT        10
#define DEFAULT_82543_TIPG_IPGT_FIBER  9
#define DEFAULT_82543_TIPG_IPGT_COPPER 8

#define E1000_TIPG_IPGT_MASK  0x000003FF
#define E1000_TIPG_IPGR1_MASK 0x000FFC00
#define E1000_TIPG_IPGR2_MASK 0x3FF00000

#define DEFAULT_82542_TIPG_IPGR1 2
#define DEFAULT_82543_TIPG_IPGR1 8
#define E1000_TIPG_IPGR1_SHIFT  10

#define DEFAULT_82542_TIPG_IPGR2 10
#define DEFAULT_82543_TIPG_IPGR2 6
#define DEFAULT_80003ES2LAN_TIPG_IPGR2 7
#define E1000_TIPG_IPGR2_SHIFT  20

/* Ethertype field values */
#define ETHERNET_IEEE_VLAN_TYPE 0x8100  /* 802.3ac packet */

#define ETHERNET_FCS_SIZE       4
#define MAX_JUMBO_FRAME_SIZE    0x3F00

/* Extended Configuration Control and Size */
#define E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP      0x00000020
#define E1000_EXTCNF_CTRL_LCD_WRITE_ENABLE       0x00000001
#define E1000_EXTCNF_CTRL_OEM_WRITE_ENABLE       0x00000008
#define E1000_EXTCNF_CTRL_SWFLAG                 0x00000020
#define E1000_EXTCNF_CTRL_GATE_PHY_CFG           0x00000080
#define E1000_EXTCNF_SIZE_EXT_PCIE_LENGTH_MASK   0x00FF0000
#define E1000_EXTCNF_SIZE_EXT_PCIE_LENGTH_SHIFT          16
#define E1000_EXTCNF_CTRL_EXT_CNF_POINTER_MASK   0x0FFF0000
#define E1000_EXTCNF_CTRL_EXT_CNF_POINTER_SHIFT          16

#define E1000_PHY_CTRL_SPD_EN             0x00000001
#define E1000_PHY_CTRL_D0A_LPLU           0x00000002
#define E1000_PHY_CTRL_NOND0A_LPLU        0x00000004
#define E1000_PHY_CTRL_NOND0A_GBE_DISABLE 0x00000008
#define E1000_PHY_CTRL_GBE_DISABLE        0x00000040

#define E1000_KABGTXD_BGSQLBIAS           0x00050000

/* PBA constants */
#define E1000_PBA_6K  0x0006    /* 6KB */
#define E1000_PBA_8K  0x0008    /* 8KB */
#define E1000_PBA_10K 0x000A    /* 10KB */
#define E1000_PBA_12K 0x000C    /* 12KB */
#define E1000_PBA_14K 0x000E    /* 14KB */
#define E1000_PBA_16K 0x0010    /* 16KB */
#define E1000_PBA_18K 0x0012
#define E1000_PBA_20K 0x0014
#define E1000_PBA_22K 0x0016
#define E1000_PBA_24K 0x0018
#define E1000_PBA_26K 0x001A
#define E1000_PBA_30K 0x001E
#define E1000_PBA_32K 0x0020
#define E1000_PBA_34K 0x0022
#define E1000_PBA_35K 0x0023
#define E1000_PBA_38K 0x0026
#define E1000_PBA_40K 0x0028
#define E1000_PBA_48K 0x0030    /* 48KB */
#define E1000_PBA_64K 0x0040    /* 64KB */

#define E1000_PBS_16K E1000_PBA_16K
#define E1000_PBS_24K E1000_PBA_24K

#define IFS_MAX       80
#define IFS_MIN       40
#define IFS_RATIO     4
#define IFS_STEP      10
#define MIN_NUM_XMITS 1000

/* SW Semaphore Register */
#define E1000_SWSM_SMBI         0x00000001 /* Driver Semaphore bit */
#define E1000_SWSM_SWESMBI      0x00000002 /* FW Semaphore bit */
#define E1000_SWSM_WMNG         0x00000004 /* Wake MNG Clock */
#define E1000_SWSM_DRV_LOAD     0x00000008 /* Driver Loaded Bit */

#define E1000_SWSM2_LOCK        0x00000002 /* Secondary driver semaphore bit */

/* Interrupt Cause Read */
#define E1000_ICR_TXDW          0x00000001 /* Transmit desc written back */
#define E1000_ICR_TXQE          0x00000002 /* Transmit Queue empty */
#define E1000_ICR_LSC           0x00000004 /* Link Status Change */
#define E1000_ICR_RXSEQ         0x00000008 /* rx sequence error */
#define E1000_ICR_RXDMT0        0x00000010 /* rx desc min. threshold (0) */
#define E1000_ICR_RXO           0x00000040 /* rx overrun */
#define E1000_ICR_RXT0          0x00000080 /* rx timer intr (ring 0) */
#define E1000_ICR_VMMB          0x00000100 /* VM MB event */
#define E1000_ICR_MDAC          0x00000200 /* MDIO access complete */
#define E1000_ICR_RXCFG         0x00000400 /* Rx /c/ ordered set */
#define E1000_ICR_GPI_EN0       0x00000800 /* GP Int 0 */
#define E1000_ICR_GPI_EN1       0x00001000 /* GP Int 1 */
#define E1000_ICR_GPI_EN2       0x00002000 /* GP Int 2 */
#define E1000_ICR_GPI_EN3       0x00004000 /* GP Int 3 */
#define E1000_ICR_TXD_LOW       0x00008000
#define E1000_ICR_SRPD          0x00010000
#define E1000_ICR_ACK           0x00020000 /* Receive Ack frame */
#define E1000_ICR_MNG           0x00040000 /* Manageability event */
#define E1000_ICR_DOCK          0x00080000 /* Dock/Undock */
#define E1000_ICR_DRSTA         0x40000000 /* Device Reset Asserted */
#define E1000_ICR_INT_ASSERTED  0x80000000 /* If this bit asserted, the driver
                                            * should claim the interrupt */
#define E1000_ICR_RXD_FIFO_PAR0 0x00100000 /* Q0 Rx desc FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR0 0x00200000 /* Q0 Tx desc FIFO parity error */
#define E1000_ICR_HOST_ARB_PAR 0x00400000 /* host arb read buffer parity err */
#define E1000_ICR_PB_PAR        0x00800000 /* packet buffer parity error */
#define E1000_ICR_RXD_FIFO_PAR1 0x01000000 /* Q1 Rx desc FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR1 0x02000000 /* Q1 Tx desc FIFO parity error */
#define E1000_ICR_ALL_PARITY    0x03F00000 /* all parity error bits */
#define E1000_ICR_DSW           0x00000020 /* FW changed the status of DISSW
                                            * bit in the FWSM */
#define E1000_ICR_PHYINT        0x00001000 /* LAN connected device generates
                                            * an interrupt */
#define E1000_ICR_DOUTSYNC      0x10000000 /* NIC DMA out of sync */
#define E1000_ICR_EPRST         0x00100000 /* ME hardware reset occurs */
#define E1000_ICR_RXQ0          0x00100000 /* Rx Queue 0 Interrupt */
#define E1000_ICR_RXQ1          0x00200000 /* Rx Queue 1 Interrupt */
#define E1000_ICR_TXQ0          0x00400000 /* Tx Queue 0 Interrupt */
#define E1000_ICR_TXQ1          0x00800000 /* Tx Queue 1 Interrupt */
#define E1000_ICR_OTHER         0x01000000 /* Other Interrupts */
#define E1000_ICR_FER           0x00400000 /* Fatal Error */

/* PBA ECC Register */
#define E1000_PBA_ECC_COUNTER_MASK  0xFFF00000 /* ECC counter mask */
#define E1000_PBA_ECC_COUNTER_SHIFT 20         /* ECC counter shift value */
#define E1000_PBA_ECC_CORR_EN      0x00000001 /* Enable ECC error correction */
#define E1000_PBA_ECC_STAT_CLR      0x00000002 /* Clear ECC error counter */
#define E1000_PBA_ECC_INT_EN     0x00000004 /* Enable ICR bit 5 on ECC error */

/* Extended Interrupt Cause Read */
#define E1000_EICR_RX_QUEUE0    0x00000001 /* Rx Queue 0 Interrupt */
#define E1000_EICR_RX_QUEUE1    0x00000002 /* Rx Queue 1 Interrupt */
#define E1000_EICR_RX_QUEUE2    0x00000004 /* Rx Queue 2 Interrupt */
#define E1000_EICR_RX_QUEUE3    0x00000008 /* Rx Queue 3 Interrupt */
#define E1000_EICR_TX_QUEUE0    0x00000100 /* Tx Queue 0 Interrupt */
#define E1000_EICR_TX_QUEUE1    0x00000200 /* Tx Queue 1 Interrupt */
#define E1000_EICR_TX_QUEUE2    0x00000400 /* Tx Queue 2 Interrupt */
#define E1000_EICR_TX_QUEUE3    0x00000800 /* Tx Queue 3 Interrupt */
#define E1000_EICR_TCP_TIMER    0x40000000 /* TCP Timer */
#define E1000_EICR_OTHER        0x80000000 /* Interrupt Cause Active */
/* TCP Timer */
#define E1000_TCPTIMER_KS       0x00000100 /* KickStart */
#define E1000_TCPTIMER_COUNT_ENABLE       0x00000200 /* Count Enable */
#define E1000_TCPTIMER_COUNT_FINISH       0x00000400 /* Count finish */
#define E1000_TCPTIMER_LOOP     0x00000800 /* Loop */

/*
 * This defines the bits that are set in the Interrupt Mask
 * Set/Read Register.  Each bit is documented below:
 *   o RXDMT0 = Receive Descriptor Minimum Threshold hit (ring 0)
 *   o RXSEQ  = Receive Sequence Error
 */
#define POLL_IMS_ENABLE_MASK ( \
    E1000_IMS_RXDMT0 |    \
    E1000_IMS_RXSEQ)

/*
 * This defines the bits that are set in the Interrupt Mask
 * Set/Read Register.  Each bit is documented below:
 *   o RXT0   = Receiver Timer Interrupt (ring 0)
 *   o TXDW   = Transmit Descriptor Written Back
 *   o RXDMT0 = Receive Descriptor Minimum Threshold hit (ring 0)
 *   o RXSEQ  = Receive Sequence Error
 *   o LSC    = Link Status Change
 */
#define IMS_ENABLE_MASK ( \
    E1000_IMS_RXT0   |    \
    E1000_IMS_TXDW   |    \
    E1000_IMS_RXDMT0 |    \
    E1000_IMS_RXSEQ  |    \
    E1000_IMS_LSC)

/* Interrupt Mask Set */
#define E1000_IMS_TXDW      E1000_ICR_TXDW      /* Tx desc written back */
#define E1000_IMS_TXQE      E1000_ICR_TXQE      /* Transmit Queue empty */
#define E1000_IMS_LSC       E1000_ICR_LSC       /* Link Status Change */
#define E1000_IMS_VMMB      E1000_ICR_VMMB      /* Mail box activity */
#define E1000_IMS_RXSEQ     E1000_ICR_RXSEQ     /* rx sequence error */
#define E1000_IMS_RXDMT0    E1000_ICR_RXDMT0    /* rx desc min. threshold */
#define E1000_IMS_RXO       E1000_ICR_RXO       /* rx overrun */
#define E1000_IMS_RXT0      E1000_ICR_RXT0      /* rx timer intr */
#define E1000_IMS_MDAC      E1000_ICR_MDAC      /* MDIO access complete */
#define E1000_IMS_RXCFG     E1000_ICR_RXCFG     /* Rx /c/ ordered set */
#define E1000_IMS_GPI_EN0   E1000_ICR_GPI_EN0   /* GP Int 0 */
#define E1000_IMS_GPI_EN1   E1000_ICR_GPI_EN1   /* GP Int 1 */
#define E1000_IMS_GPI_EN2   E1000_ICR_GPI_EN2   /* GP Int 2 */
#define E1000_IMS_GPI_EN3   E1000_ICR_GPI_EN3   /* GP Int 3 */
#define E1000_IMS_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_IMS_SRPD      E1000_ICR_SRPD
#define E1000_IMS_ACK       E1000_ICR_ACK       /* Receive Ack frame */
#define E1000_IMS_MNG       E1000_ICR_MNG       /* Manageability event */
#define E1000_IMS_DOCK      E1000_ICR_DOCK      /* Dock/Undock */
#define E1000_IMS_DRSTA     E1000_ICR_DRSTA     /* Device Reset Asserted */
#define E1000_IMS_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0 /* Q0 Rx desc FIFO
                                                         * parity error */
#define E1000_IMS_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0 /* Q0 Tx desc FIFO
                                                         * parity error */
#define E1000_IMS_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR  /* host arb read buffer
                                                         * parity error */
#define E1000_IMS_PB_PAR        E1000_ICR_PB_PAR        /* packet buffer parity
                                                         * error */
#define E1000_IMS_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1 /* Q1 Rx desc FIFO
                                                         * parity error */
#define E1000_IMS_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1 /* Q1 Tx desc FIFO
                                                         * parity error */
#define E1000_IMS_DSW       E1000_ICR_DSW
#define E1000_IMS_PHYINT    E1000_ICR_PHYINT
#define E1000_IMS_DOUTSYNC  E1000_ICR_DOUTSYNC /* NIC DMA out of sync */
#define E1000_IMS_EPRST     E1000_ICR_EPRST
#define E1000_IMS_RXQ0          E1000_ICR_RXQ0 /* Rx Queue 0 Interrupt */
#define E1000_IMS_RXQ1          E1000_ICR_RXQ1 /* Rx Queue 1 Interrupt */
#define E1000_IMS_TXQ0          E1000_ICR_TXQ0 /* Tx Queue 0 Interrupt */
#define E1000_IMS_TXQ1          E1000_ICR_TXQ1 /* Tx Queue 1 Interrupt */
#define E1000_IMS_OTHER         E1000_ICR_OTHER /* Other Interrupts */
#define E1000_IMS_FER           E1000_ICR_FER /* Fatal Error */

/* Extended Interrupt Mask Set */
#define E1000_EIMS_RX_QUEUE0    E1000_EICR_RX_QUEUE0 /* Rx Queue 0 Interrupt */
#define E1000_EIMS_RX_QUEUE1    E1000_EICR_RX_QUEUE1 /* Rx Queue 1 Interrupt */
#define E1000_EIMS_RX_QUEUE2    E1000_EICR_RX_QUEUE2 /* Rx Queue 2 Interrupt */
#define E1000_EIMS_RX_QUEUE3    E1000_EICR_RX_QUEUE3 /* Rx Queue 3 Interrupt */
#define E1000_EIMS_TX_QUEUE0    E1000_EICR_TX_QUEUE0 /* Tx Queue 0 Interrupt */
#define E1000_EIMS_TX_QUEUE1    E1000_EICR_TX_QUEUE1 /* Tx Queue 1 Interrupt */
#define E1000_EIMS_TX_QUEUE2    E1000_EICR_TX_QUEUE2 /* Tx Queue 2 Interrupt */
#define E1000_EIMS_TX_QUEUE3    E1000_EICR_TX_QUEUE3 /* Tx Queue 3 Interrupt */
#define E1000_EIMS_TCP_TIMER    E1000_EICR_TCP_TIMER /* TCP Timer */
#define E1000_EIMS_OTHER        E1000_EICR_OTHER   /* Interrupt Cause Active */

/* Interrupt Cause Set */
#define E1000_ICS_TXDW      E1000_ICR_TXDW      /* Tx desc written back */
#define E1000_ICS_TXQE      E1000_ICR_TXQE      /* Transmit Queue empty */
#define E1000_ICS_LSC       E1000_ICR_LSC       /* Link Status Change */
#define E1000_ICS_RXSEQ     E1000_ICR_RXSEQ     /* rx sequence error */
#define E1000_ICS_RXDMT0    E1000_ICR_RXDMT0    /* rx desc min. threshold */
#define E1000_ICS_RXO       E1000_ICR_RXO       /* rx overrun */
#define E1000_ICS_RXT0      E1000_ICR_RXT0      /* rx timer intr */
#define E1000_ICS_MDAC      E1000_ICR_MDAC      /* MDIO access complete */
#define E1000_ICS_RXCFG     E1000_ICR_RXCFG     /* Rx /c/ ordered set */
#define E1000_ICS_GPI_EN0   E1000_ICR_GPI_EN0   /* GP Int 0 */
#define E1000_ICS_GPI_EN1   E1000_ICR_GPI_EN1   /* GP Int 1 */
#define E1000_ICS_GPI_EN2   E1000_ICR_GPI_EN2   /* GP Int 2 */
#define E1000_ICS_GPI_EN3   E1000_ICR_GPI_EN3   /* GP Int 3 */
#define E1000_ICS_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_ICS_SRPD      E1000_ICR_SRPD
#define E1000_ICS_ACK       E1000_ICR_ACK       /* Receive Ack frame */
#define E1000_ICS_MNG       E1000_ICR_MNG       /* Manageability event */
#define E1000_ICS_DOCK      E1000_ICR_DOCK      /* Dock/Undock */
#define E1000_ICS_DRSTA     E1000_ICR_DRSTA     /* Device Reset Aserted */
#define E1000_ICS_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0 /* Q0 Rx desc FIFO
                                                         * parity error */
#define E1000_ICS_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0 /* Q0 Tx desc FIFO
                                                         * parity error */
#define E1000_ICS_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR  /* host arb read buffer
                                                         * parity error */
#define E1000_ICS_PB_PAR        E1000_ICR_PB_PAR        /* packet buffer parity
                                                         * error */
#define E1000_ICS_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1 /* Q1 Rx desc FIFO
                                                         * parity error */
#define E1000_ICS_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1 /* Q1 Tx desc FIFO
                                                         * parity error */
#define E1000_ICS_DSW       E1000_ICR_DSW
#define E1000_ICS_DOUTSYNC  E1000_ICR_DOUTSYNC /* NIC DMA out of sync */
#define E1000_ICS_PHYINT    E1000_ICR_PHYINT
#define E1000_ICS_EPRST     E1000_ICR_EPRST

/* Extended Interrupt Cause Set */
#define E1000_EICS_RX_QUEUE0    E1000_EICR_RX_QUEUE0 /* Rx Queue 0 Interrupt */
#define E1000_EICS_RX_QUEUE1    E1000_EICR_RX_QUEUE1 /* Rx Queue 1 Interrupt */
#define E1000_EICS_RX_QUEUE2    E1000_EICR_RX_QUEUE2 /* Rx Queue 2 Interrupt */
#define E1000_EICS_RX_QUEUE3    E1000_EICR_RX_QUEUE3 /* Rx Queue 3 Interrupt */
#define E1000_EICS_TX_QUEUE0    E1000_EICR_TX_QUEUE0 /* Tx Queue 0 Interrupt */
#define E1000_EICS_TX_QUEUE1    E1000_EICR_TX_QUEUE1 /* Tx Queue 1 Interrupt */
#define E1000_EICS_TX_QUEUE2    E1000_EICR_TX_QUEUE2 /* Tx Queue 2 Interrupt */
#define E1000_EICS_TX_QUEUE3    E1000_EICR_TX_QUEUE3 /* Tx Queue 3 Interrupt */
#define E1000_EICS_TCP_TIMER    E1000_EICR_TCP_TIMER /* TCP Timer */
#define E1000_EICS_OTHER        E1000_EICR_OTHER   /* Interrupt Cause Active */

#define E1000_EITR_ITR_INT_MASK 0x0000FFFF
/* E1000_EITR_CNT_IGNR is only for 82576 and newer */
#define E1000_EITR_CNT_IGNR	0x80000000 /* Don't reset counters on write */

/* Transmit Descriptor Control */
#define E1000_TXDCTL_PTHRESH 0x0000003F /* TXDCTL Prefetch Threshold */
#define E1000_TXDCTL_HTHRESH 0x00003F00 /* TXDCTL Host Threshold */
#define E1000_TXDCTL_WTHRESH 0x003F0000 /* TXDCTL Writeback Threshold */
#define E1000_TXDCTL_GRAN    0x01000000 /* TXDCTL Granularity */
#define E1000_TXDCTL_LWTHRESH 0xFE000000 /* TXDCTL Low Threshold */
#define E1000_TXDCTL_FULL_TX_DESC_WB 0x01010000 /* GRAN=1, WTHRESH=1 */
#define E1000_TXDCTL_MAX_TX_DESC_PREFETCH 0x0100001F /* GRAN=1, PTHRESH=31 */
/* Enable the counting of descriptors still to be processed. */
#define E1000_TXDCTL_COUNT_DESC 0x00400000

/* Flow Control Constants */
#define FLOW_CONTROL_ADDRESS_LOW  0x00C28001
#define FLOW_CONTROL_ADDRESS_HIGH 0x00000100
#define FLOW_CONTROL_TYPE         0x8808

/* 802.1q VLAN Packet Size */
#define VLAN_TAG_SIZE              4    /* 802.3ac tag (not DMA'd) */
#define E1000_VLAN_FILTER_TBL_SIZE 128  /* VLAN Filter Table (4096 bits) */

/* Receive Address */
/*
 * Number of high/low register pairs in the RAR. The RAR (Receive Address
 * Registers) holds the directed and multicast addresses that we monitor.
 * Technically, we have 16 spots.  However, we reserve one of these spots
 * (RAR[15]) for our directed address used by controllers with
 * manageability enabled, allowing us room for 15 multicast addresses.
 */
#define E1000_RAR_ENTRIES     15
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */
#define E1000_RAL_MAC_ADDR_LEN 4
#define E1000_RAH_MAC_ADDR_LEN 2
#define E1000_RAH_POOL_MASK 0x03FC0000
#define E1000_RAH_POOL_1 0x00040000

/* Error Codes */
#define E1000_SUCCESS      0
#define E1000_ERR_NVM      1
#define E1000_ERR_PHY      2
#define E1000_ERR_CONFIG   3
#define E1000_ERR_PARAM    4
#define E1000_ERR_MAC_INIT 5
#define E1000_ERR_PHY_TYPE 6
#define E1000_ERR_RESET   9
#define E1000_ERR_MASTER_REQUESTS_PENDING 10
#define E1000_ERR_HOST_INTERFACE_COMMAND 11
#define E1000_BLK_PHY_RESET   12
#define E1000_ERR_SWFW_SYNC 13
#define E1000_NOT_IMPLEMENTED 14
#define E1000_ERR_MBX      15
#define E1000_ERR_INVALID_ARGUMENT  16
#define E1000_ERR_NO_SPACE          17
#define E1000_ERR_NVM_PBA_SECTION   18

/* Loop limit on how long we wait for auto-negotiation to complete */
#define FIBER_LINK_UP_LIMIT               50
#define COPPER_LINK_UP_LIMIT              10
#define PHY_AUTO_NEG_LIMIT                45
#define PHY_FORCE_LIMIT                   20
/* Number of 100 microseconds we wait for PCI Express master disable */
#define MASTER_DISABLE_TIMEOUT      800
/* Number of milliseconds we wait for PHY configuration done after MAC reset */
#define PHY_CFG_TIMEOUT             100
/* Number of 2 milliseconds we wait for acquiring MDIO ownership. */
#define MDIO_OWNERSHIP_TIMEOUT      10
/* Number of milliseconds for NVM auto read done after MAC reset. */
#define AUTO_READ_DONE_TIMEOUT      10

/* Flow Control */
#define E1000_FCRTH_RTH  0x0000FFF8     /* Mask Bits[15:3] for RTH */
#define E1000_FCRTH_XFCE 0x80000000     /* External Flow Control Enable */
#define E1000_FCRTL_RTL  0x0000FFF8     /* Mask Bits[15:3] for RTL */
#define E1000_FCRTL_XONE 0x80000000     /* Enable XON frame transmission */

/* Transmit Configuration Word */
#define E1000_TXCW_FD         0x00000020        /* TXCW full duplex */
#define E1000_TXCW_HD         0x00000040        /* TXCW half duplex */
#define E1000_TXCW_PAUSE      0x00000080        /* TXCW sym pause request */
#define E1000_TXCW_ASM_DIR    0x00000100        /* TXCW astm pause direction */
#define E1000_TXCW_PAUSE_MASK 0x00000180        /* TXCW pause request mask */
#define E1000_TXCW_RF         0x00003000        /* TXCW remote fault */
#define E1000_TXCW_NP         0x00008000        /* TXCW next page */
#define E1000_TXCW_CW         0x0000ffff        /* TxConfigWord mask */
#define E1000_TXCW_TXC        0x40000000        /* Transmit Config control */
#define E1000_TXCW_ANE        0x80000000        /* Auto-neg enable */

/* Receive Configuration Word */
#define E1000_RXCW_CW         0x0000ffff        /* RxConfigWord mask */
#define E1000_RXCW_NC         0x04000000        /* Receive config no carrier */
#define E1000_RXCW_IV         0x08000000        /* Receive config invalid */
#define E1000_RXCW_CC         0x10000000        /* Receive config change */
#define E1000_RXCW_C          0x20000000        /* Receive config */
#define E1000_RXCW_SYNCH      0x40000000        /* Receive config synch */
#define E1000_RXCW_ANC        0x80000000        /* Auto-neg complete */

#define E1000_TSYNCTXCTL_VALID    0x00000001 /* tx timestamp valid */
#define E1000_TSYNCTXCTL_ENABLED  0x00000010 /* enable tx timestampping */

#define E1000_TSYNCRXCTL_VALID      0x00000001 /* rx timestamp valid */
#define E1000_TSYNCRXCTL_TYPE_MASK  0x0000000E /* rx type mask */
#define E1000_TSYNCRXCTL_TYPE_L2_V2       0x00
#define E1000_TSYNCRXCTL_TYPE_L4_V1       0x02
#define E1000_TSYNCRXCTL_TYPE_L2_L4_V2    0x04
#define E1000_TSYNCRXCTL_TYPE_ALL         0x08
#define E1000_TSYNCRXCTL_TYPE_EVENT_V2    0x0A
#define E1000_TSYNCRXCTL_ENABLED    0x00000010 /* enable rx timestampping */

#define E1000_TSYNCRXCFG_PTP_V1_CTRLT_MASK   0x000000FF
#define E1000_TSYNCRXCFG_PTP_V1_SYNC_MESSAGE       0x00
#define E1000_TSYNCRXCFG_PTP_V1_DELAY_REQ_MESSAGE  0x01
#define E1000_TSYNCRXCFG_PTP_V1_FOLLOWUP_MESSAGE   0x02
#define E1000_TSYNCRXCFG_PTP_V1_DELAY_RESP_MESSAGE 0x03
#define E1000_TSYNCRXCFG_PTP_V1_MANAGEMENT_MESSAGE 0x04

#define E1000_TSYNCRXCFG_PTP_V2_MSGID_MASK               0x00000F00
#define E1000_TSYNCRXCFG_PTP_V2_SYNC_MESSAGE                 0x0000
#define E1000_TSYNCRXCFG_PTP_V2_DELAY_REQ_MESSAGE            0x0100
#define E1000_TSYNCRXCFG_PTP_V2_PATH_DELAY_REQ_MESSAGE       0x0200
#define E1000_TSYNCRXCFG_PTP_V2_PATH_DELAY_RESP_MESSAGE      0x0300
#define E1000_TSYNCRXCFG_PTP_V2_FOLLOWUP_MESSAGE             0x0800
#define E1000_TSYNCRXCFG_PTP_V2_DELAY_RESP_MESSAGE           0x0900
#define E1000_TSYNCRXCFG_PTP_V2_PATH_DELAY_FOLLOWUP_MESSAGE  0x0A00
#define E1000_TSYNCRXCFG_PTP_V2_ANNOUNCE_MESSAGE             0x0B00
#define E1000_TSYNCRXCFG_PTP_V2_SIGNALLING_MESSAGE           0x0C00
#define E1000_TSYNCRXCFG_PTP_V2_MANAGEMENT_MESSAGE           0x0D00

#define E1000_TIMINCA_16NS_SHIFT 24
/* TUPLE Filtering Configuration */
#define E1000_TTQF_DISABLE_MASK   0xF0008000     /* TTQF Disable Mask */
#define E1000_TTQF_QUEUE_ENABLE   0x100          /* TTQF Queue Enable Bit */
#define E1000_TTQF_PROTOCOL_MASK  0xFF           /* TTQF Protocol Mask */
/* TTQF TCP Bit, shift with E1000_TTQF_PROTOCOL SHIFT */
#define E1000_TTQF_PROTOCOL_TCP   0x0
/* TTQF UDP Bit, shift with E1000_TTQF_PROTOCOL_SHIFT */
#define E1000_TTQF_PROTOCOL_UDP   0x1
/* TTQF SCTP Bit, shift with E1000_TTQF_PROTOCOL_SHIFT */
#define E1000_TTQF_PROTOCOL_SCTP  0x2
#define E1000_TTQF_PROTOCOL_SHIFT 5              /* TTQF Protocol Shift */
#define E1000_TTQF_QUEUE_SHIFT    16             /* TTQF Queue Shfit */
#define E1000_TTQF_RX_QUEUE_MASK  0x70000        /* TTQF Queue Mask */
#define E1000_TTQF_MASK_ENABLE    0x10000000     /* TTQF Mask Enable Bit */
#define E1000_IMIR_CLEAR_MASK     0xF001FFFF     /* IMIR Reg Clear Mask */
#define E1000_IMIR_PORT_BYPASS    0x20000        /* IMIR Port Bypass Bit */
#define E1000_IMIR_PRIORITY_SHIFT 29             /* IMIR Priority Shift */
#define E1000_IMIREXT_CLEAR_MASK  0x7FFFF        /* IMIREXT Reg Clear Mask */

#define E1000_MDICNFG_EXT_MDIO    0x80000000      /* MDI ext/int destination */
#define E1000_MDICNFG_COM_MDIO    0x40000000      /* MDI shared w/ lan 0 */
#define E1000_MDICNFG_PHY_MASK    0x03E00000
#define E1000_MDICNFG_PHY_SHIFT   21

/* PCI Express Control */
#define E1000_GCR_RXD_NO_SNOOP          0x00000001
#define E1000_GCR_RXDSCW_NO_SNOOP       0x00000002
#define E1000_GCR_RXDSCR_NO_SNOOP       0x00000004
#define E1000_GCR_TXD_NO_SNOOP          0x00000008
#define E1000_GCR_TXDSCW_NO_SNOOP       0x00000010
#define E1000_GCR_TXDSCR_NO_SNOOP       0x00000020
#define E1000_GCR_CMPL_TMOUT_MASK       0x0000F000
#define E1000_GCR_CMPL_TMOUT_10ms       0x00001000
#define E1000_GCR_CMPL_TMOUT_RESEND     0x00010000
#define E1000_GCR_CAP_VER2              0x00040000

#define PCIE_NO_SNOOP_ALL (E1000_GCR_RXD_NO_SNOOP         | \
                           E1000_GCR_RXDSCW_NO_SNOOP      | \
                           E1000_GCR_RXDSCR_NO_SNOOP      | \
                           E1000_GCR_TXD_NO_SNOOP         | \
                           E1000_GCR_TXDSCW_NO_SNOOP      | \
                           E1000_GCR_TXDSCR_NO_SNOOP)

/* PHY Control Register */
#define MII_CR_SPEED_SELECT_MSB 0x0040  /* bits 6,13: 10=1000, 01=100, 00=10 */
#define MII_CR_COLL_TEST_ENABLE 0x0080  /* Collision test enable */
#define MII_CR_FULL_DUPLEX      0x0100  /* FDX =1, half duplex =0 */
#define MII_CR_RESTART_AUTO_NEG 0x0200  /* Restart auto negotiation */
#define MII_CR_ISOLATE          0x0400  /* Isolate PHY from MII */
#define MII_CR_POWER_DOWN       0x0800  /* Power down */
#define MII_CR_AUTO_NEG_EN      0x1000  /* Auto Neg Enable */
#define MII_CR_SPEED_SELECT_LSB 0x2000  /* bits 6,13: 10=1000, 01=100, 00=10 */
#define MII_CR_LOOPBACK         0x4000  /* 0 = normal, 1 = loopback */
#define MII_CR_RESET            0x8000  /* 0 = normal, 1 = PHY reset */
#define MII_CR_SPEED_1000       0x0040
#define MII_CR_SPEED_100        0x2000
#define MII_CR_SPEED_10         0x0000

/* PHY Status Register */
#define MII_SR_EXTENDED_CAPS     0x0001 /* Extended register capabilities */
#define MII_SR_JABBER_DETECT     0x0002 /* Jabber Detected */
#define MII_SR_LINK_STATUS       0x0004 /* Link Status 1 = link */
#define MII_SR_AUTONEG_CAPS      0x0008 /* Auto Neg Capable */
#define MII_SR_REMOTE_FAULT      0x0010 /* Remote Fault Detect */
#define MII_SR_AUTONEG_COMPLETE  0x0020 /* Auto Neg Complete */
#define MII_SR_PREAMBLE_SUPPRESS 0x0040 /* Preamble may be suppressed */
#define MII_SR_EXTENDED_STATUS   0x0100 /* Ext. status info in Reg 0x0F */
#define MII_SR_100T2_HD_CAPS     0x0200 /* 100T2 Half Duplex Capable */
#define MII_SR_100T2_FD_CAPS     0x0400 /* 100T2 Full Duplex Capable */
#define MII_SR_10T_HD_CAPS       0x0800 /* 10T   Half Duplex Capable */
#define MII_SR_10T_FD_CAPS       0x1000 /* 10T   Full Duplex Capable */
#define MII_SR_100X_HD_CAPS      0x2000 /* 100X  Half Duplex Capable */
#define MII_SR_100X_FD_CAPS      0x4000 /* 100X  Full Duplex Capable */
#define MII_SR_100T4_CAPS        0x8000 /* 100T4 Capable */

/* Autoneg Advertisement Register */
#define NWAY_AR_SELECTOR_FIELD   0x0001   /* indicates IEEE 802.3 CSMA/CD */
#define NWAY_AR_10T_HD_CAPS      0x0020   /* 10T   Half Duplex Capable */
#define NWAY_AR_10T_FD_CAPS      0x0040   /* 10T   Full Duplex Capable */
#define NWAY_AR_100TX_HD_CAPS    0x0080   /* 100TX Half Duplex Capable */
#define NWAY_AR_100TX_FD_CAPS    0x0100   /* 100TX Full Duplex Capable */
#define NWAY_AR_100T4_CAPS       0x0200   /* 100T4 Capable */
#define NWAY_AR_PAUSE            0x0400   /* Pause operation desired */
#define NWAY_AR_ASM_DIR          0x0800   /* Asymmetric Pause Direction bit */
#define NWAY_AR_REMOTE_FAULT     0x2000   /* Remote Fault detected */
#define NWAY_AR_NEXT_PAGE        0x8000   /* Next Page ability supported */

/* Link Partner Ability Register (Base Page) */
#define NWAY_LPAR_SELECTOR_FIELD 0x0000 /* LP protocol selector field */
#define NWAY_LPAR_10T_HD_CAPS    0x0020 /* LP is 10T   Half Duplex Capable */
#define NWAY_LPAR_10T_FD_CAPS    0x0040 /* LP is 10T   Full Duplex Capable */
#define NWAY_LPAR_100TX_HD_CAPS  0x0080 /* LP is 100TX Half Duplex Capable */
#define NWAY_LPAR_100TX_FD_CAPS  0x0100 /* LP is 100TX Full Duplex Capable */
#define NWAY_LPAR_100T4_CAPS     0x0200 /* LP is 100T4 Capable */
#define NWAY_LPAR_PAUSE          0x0400 /* LP Pause operation desired */
#define NWAY_LPAR_ASM_DIR        0x0800 /* LP Asymmetric Pause Direction bit */
#define NWAY_LPAR_REMOTE_FAULT   0x2000 /* LP has detected Remote Fault */
#define NWAY_LPAR_ACKNOWLEDGE    0x4000 /* LP has rx'd link code word */
#define NWAY_LPAR_NEXT_PAGE      0x8000 /* Next Page ability supported */

/* Autoneg Expansion Register */
#define NWAY_ER_LP_NWAY_CAPS      0x0001 /* LP has Auto Neg Capability */
#define NWAY_ER_PAGE_RXD          0x0002 /* LP is 10T   Half Duplex Capable */
#define NWAY_ER_NEXT_PAGE_CAPS    0x0004 /* LP is 10T   Full Duplex Capable */
#define NWAY_ER_LP_NEXT_PAGE_CAPS 0x0008 /* LP is 100TX Half Duplex Capable */
#define NWAY_ER_PAR_DETECT_FAULT  0x0010 /* LP is 100TX Full Duplex Capable */

/* 1000BASE-T Control Register */
#define CR_1000T_ASYM_PAUSE      0x0080 /* Advertise asymmetric pause bit */
#define CR_1000T_HD_CAPS         0x0100 /* Advertise 1000T HD capability */
#define CR_1000T_FD_CAPS         0x0200 /* Advertise 1000T FD capability  */
#define CR_1000T_REPEATER_DTE    0x0400 /* 1=Repeater/switch device port */
                                        /* 0=DTE device */
#define CR_1000T_MS_VALUE        0x0800 /* 1=Configure PHY as Master */
                                        /* 0=Configure PHY as Slave */
#define CR_1000T_MS_ENABLE      0x1000 /* 1=Master/Slave manual config value */
                                        /* 0=Automatic Master/Slave config */
#define CR_1000T_TEST_MODE_NORMAL 0x0000 /* Normal Operation */
#define CR_1000T_TEST_MODE_1     0x2000 /* Transmit Waveform test */
#define CR_1000T_TEST_MODE_2     0x4000 /* Master Transmit Jitter test */
#define CR_1000T_TEST_MODE_3     0x6000 /* Slave Transmit Jitter test */
#define CR_1000T_TEST_MODE_4     0x8000 /* Transmitter Distortion test */

/* 1000BASE-T Status Register */
#define SR_1000T_IDLE_ERROR_CNT   0x00FF /* Num idle errors since last read */
#define SR_1000T_ASYM_PAUSE_DIR  0x0100 /* LP asymmetric pause direction bit */
#define SR_1000T_LP_HD_CAPS       0x0400 /* LP is 1000T HD capable */
#define SR_1000T_LP_FD_CAPS       0x0800 /* LP is 1000T FD capable */
#define SR_1000T_REMOTE_RX_STATUS 0x1000 /* Remote receiver OK */
#define SR_1000T_LOCAL_RX_STATUS  0x2000 /* Local receiver OK */
#define SR_1000T_MS_CONFIG_RES    0x4000 /* 1=Local Tx is Master, 0=Slave */
#define SR_1000T_MS_CONFIG_FAULT  0x8000 /* Master/Slave config fault */

#define SR_1000T_PHY_EXCESSIVE_IDLE_ERR_COUNT 5

/* PHY 1000 MII Register/Bit Definitions */
/* PHY Registers defined by IEEE */
#define PHY_CONTROL      0x00 /* Control Register */
#define PHY_STATUS       0x01 /* Status Register */
#define PHY_ID1          0x02 /* Phy Id Reg (word 1) */
#define PHY_ID2          0x03 /* Phy Id Reg (word 2) */
#define PHY_AUTONEG_ADV  0x04 /* Autoneg Advertisement */
#define PHY_LP_ABILITY   0x05 /* Link Partner Ability (Base Page) */
#define PHY_AUTONEG_EXP  0x06 /* Autoneg Expansion Reg */
#define PHY_NEXT_PAGE_TX 0x07 /* Next Page Tx */
#define PHY_LP_NEXT_PAGE 0x08 /* Link Partner Next Page */
#define PHY_1000T_CTRL   0x09 /* 1000Base-T Control Reg */
#define PHY_1000T_STATUS 0x0A /* 1000Base-T Status Reg */
#define PHY_EXT_STATUS   0x0F /* Extended Status Reg */

#define PHY_CONTROL_LB   0x4000 /* PHY Loopback bit */

/* NVM Control */
#define E1000_EECD_SK        0x00000001 /* NVM Clock */
#define E1000_EECD_CS        0x00000002 /* NVM Chip Select */
#define E1000_EECD_DI        0x00000004 /* NVM Data In */
#define E1000_EECD_DO        0x00000008 /* NVM Data Out */
#define E1000_EECD_FWE_MASK  0x00000030
#define E1000_EECD_FWE_DIS   0x00000010 /* Disable FLASH writes */
#define E1000_EECD_FWE_EN    0x00000020 /* Enable FLASH writes */
#define E1000_EECD_FWE_SHIFT 4
#define E1000_EECD_REQ       0x00000040 /* NVM Access Request */
#define E1000_EECD_GNT       0x00000080 /* NVM Access Grant */
#define E1000_EECD_PRES      0x00000100 /* NVM Present */
#define E1000_EECD_SIZE      0x00000200 /* NVM Size (0=64 word 1=256 word) */
/* NVM Addressing bits based on type 0=small, 1=large */
#define E1000_EECD_ADDR_BITS 0x00000400
#define E1000_EECD_TYPE      0x00002000 /* NVM Type (1-SPI, 0-Microwire) */
#ifndef E1000_NVM_GRANT_ATTEMPTS
#define E1000_NVM_GRANT_ATTEMPTS   1000 /* NVM # attempts to gain grant */
#endif
#define E1000_EECD_AUTO_RD          0x00000200  /* NVM Auto Read done */
#define E1000_EECD_SIZE_EX_MASK     0x00007800  /* NVM Size */
#define E1000_EECD_SIZE_EX_SHIFT     11
#define E1000_EECD_NVADDS    0x00018000 /* NVM Address Size */
#define E1000_EECD_SELSHAD   0x00020000 /* Select Shadow RAM */
#define E1000_EECD_INITSRAM  0x00040000 /* Initialize Shadow RAM */
#define E1000_EECD_FLUPD     0x00080000 /* Update FLASH */
#define E1000_EECD_AUPDEN    0x00100000 /* Enable Autonomous FLASH update */
#define E1000_EECD_SHADV     0x00200000 /* Shadow RAM Data Valid */
#define E1000_EECD_SEC1VAL   0x00400000 /* Sector One Valid */
#define E1000_EECD_SECVAL_SHIFT      22
#define E1000_EECD_SEC1VAL_VALID_MASK (E1000_EECD_AUTO_RD | E1000_EECD_PRES)

#define E1000_NVM_SWDPIN0   0x0001   /* SWDPIN 0 NVM Value */
#define E1000_NVM_LED_LOGIC 0x0020   /* Led Logic Word */
#define E1000_NVM_RW_REG_DATA   16  /* Offset to data in NVM read/write regs */
#define E1000_NVM_RW_REG_DONE   2    /* Offset to READ/WRITE done bit */
#define E1000_NVM_RW_REG_START  1    /* Start operation */
#define E1000_NVM_RW_ADDR_SHIFT 2    /* Shift to the address bits */
#define E1000_NVM_POLL_WRITE    1    /* Flag for polling for write complete */
#define E1000_NVM_POLL_READ     0    /* Flag for polling for read complete */
#define E1000_FLASH_UPDATES  2000

/* NVM Word Offsets */
#define NVM_COMPAT                 0x0003
#define NVM_ID_LED_SETTINGS        0x0004
#define NVM_VERSION                0x0005
#define NVM_SERDES_AMPLITUDE       0x0006 /* SERDES output amplitude */
#define NVM_PHY_CLASS_WORD         0x0007
#define NVM_INIT_CONTROL1_REG      0x000A
#define NVM_INIT_CONTROL2_REG      0x000F
#define NVM_SWDEF_PINS_CTRL_PORT_1 0x0010
#define NVM_INIT_CONTROL3_PORT_B   0x0014
#define NVM_INIT_3GIO_3            0x001A
#define NVM_SWDEF_PINS_CTRL_PORT_0 0x0020
#define NVM_INIT_CONTROL3_PORT_A   0x0024
#define NVM_CFG                    0x0012
#define NVM_FLASH_VERSION          0x0032
#define NVM_ALT_MAC_ADDR_PTR       0x0037
#define NVM_CHECKSUM_REG           0x003F

#define E1000_NVM_CFG_DONE_PORT_0  0x040000 /* MNG config cycle done */
#define E1000_NVM_CFG_DONE_PORT_1  0x080000 /* ...for second port */
#define E1000_NVM_CFG_DONE_PORT_2  0x100000 /* ...for third port */
#define E1000_NVM_CFG_DONE_PORT_3  0x200000 /* ...for fourth port */

#define NVM_82580_LAN_FUNC_OFFSET(a) (a ? (0x40 + (0x40 * a)) : 0)

/* Mask bits for fields in Word 0x24 of the NVM */
#define NVM_WORD24_COM_MDIO         0x0008 /* MDIO interface shared */
#define NVM_WORD24_EXT_MDIO         0x0004 /* MDIO accesses routed external */

/* Mask bits for fields in Word 0x0f of the NVM */
#define NVM_WORD0F_PAUSE_MASK       0x3000
#define NVM_WORD0F_PAUSE            0x1000
#define NVM_WORD0F_ASM_DIR          0x2000
#define NVM_WORD0F_ANE              0x0800
#define NVM_WORD0F_SWPDIO_EXT_MASK  0x00F0
#define NVM_WORD0F_LPLU             0x0001

/* Mask bits for fields in Word 0x1a of the NVM */
#define NVM_WORD1A_ASPM_MASK  0x000C

/* Mask bits for fields in Word 0x03 of the EEPROM */
#define NVM_COMPAT_LOM    0x0800

/* length of string needed to store PBA number */
#define E1000_PBANUM_LENGTH             11

/* For checksumming, the sum of all words in the NVM should equal 0xBABA. */
#define NVM_SUM                    0xBABA

#define NVM_MAC_ADDR_OFFSET        0
#define NVM_PBA_OFFSET_0           8
#define NVM_PBA_OFFSET_1           9
#define NVM_PBA_PTR_GUARD          0xFAFA
#define NVM_RESERVED_WORD          0xFFFF
#define NVM_PHY_CLASS_A            0x8000
#define NVM_SERDES_AMPLITUDE_MASK  0x000F
#define NVM_SIZE_MASK              0x1C00
#define NVM_SIZE_SHIFT             10
#define NVM_WORD_SIZE_BASE_SHIFT   6
#define NVM_SWDPIO_EXT_SHIFT       4

/* NVM Commands - Microwire */
#define NVM_READ_OPCODE_MICROWIRE  0x6  /* NVM read opcode */
#define NVM_WRITE_OPCODE_MICROWIRE 0x5  /* NVM write opcode */
#define NVM_ERASE_OPCODE_MICROWIRE 0x7  /* NVM erase opcode */
#define NVM_EWEN_OPCODE_MICROWIRE  0x13 /* NVM erase/write enable */
#define NVM_EWDS_OPCODE_MICROWIRE  0x10 /* NVM erase/write disable */

/* NVM Commands - SPI */
#define NVM_MAX_RETRY_SPI          5000 /* Max wait of 5ms, for RDY signal */
#define NVM_READ_OPCODE_SPI        0x03 /* NVM read opcode */
#define NVM_WRITE_OPCODE_SPI       0x02 /* NVM write opcode */
#define NVM_A8_OPCODE_SPI          0x08 /* opcode bit-3 = address bit-8 */
#define NVM_WREN_OPCODE_SPI        0x06 /* NVM set Write Enable latch */
#define NVM_WRDI_OPCODE_SPI        0x04 /* NVM reset Write Enable latch */
#define NVM_RDSR_OPCODE_SPI        0x05 /* NVM read Status register */
#define NVM_WRSR_OPCODE_SPI        0x01 /* NVM write Status register */

/* SPI NVM Status Register */
#define NVM_STATUS_RDY_SPI         0x01
#define NVM_STATUS_WEN_SPI         0x02
#define NVM_STATUS_BP0_SPI         0x04
#define NVM_STATUS_BP1_SPI         0x08
#define NVM_STATUS_WPEN_SPI        0x80

/* Word definitions for ID LED Settings */
#define ID_LED_RESERVED_0000 0x0000
#define ID_LED_RESERVED_FFFF 0xFFFF
#define ID_LED_DEFAULT       ((ID_LED_OFF1_ON2  << 12) | \
                              (ID_LED_OFF1_OFF2 <<  8) | \
                              (ID_LED_DEF1_DEF2 <<  4) | \
                              (ID_LED_DEF1_DEF2))
#define ID_LED_DEF1_DEF2     0x1
#define ID_LED_DEF1_ON2      0x2
#define ID_LED_DEF1_OFF2     0x3
#define ID_LED_ON1_DEF2      0x4
#define ID_LED_ON1_ON2       0x5
#define ID_LED_ON1_OFF2      0x6
#define ID_LED_OFF1_DEF2     0x7
#define ID_LED_OFF1_ON2      0x8
#define ID_LED_OFF1_OFF2     0x9

#define IGP_ACTIVITY_LED_MASK   0xFFFFF0FF
#define IGP_ACTIVITY_LED_ENABLE 0x0300
#define IGP_LED3_MODE           0x07000000

/* PCI/PCI-X/PCI-EX Config space */
#define PCIX_COMMAND_REGISTER        0xE6
#define PCIX_STATUS_REGISTER_LO      0xE8
#define PCIX_STATUS_REGISTER_HI      0xEA
#define PCI_HEADER_TYPE_REGISTER     0x0E
#define PCIE_LINK_STATUS             0x12
#define PCIE_DEVICE_CONTROL2         0x28

#define PCIX_COMMAND_MMRBC_MASK      0x000C
#define PCIX_COMMAND_MMRBC_SHIFT     0x2
#define PCIX_STATUS_HI_MMRBC_MASK    0x0060
#define PCIX_STATUS_HI_MMRBC_SHIFT   0x5
#define PCIX_STATUS_HI_MMRBC_4K      0x3
#define PCIX_STATUS_HI_MMRBC_2K      0x2
#define PCIX_STATUS_LO_FUNC_MASK     0x7
#define PCI_HEADER_TYPE_MULTIFUNC    0x80
#define PCIE_LINK_WIDTH_MASK         0x3F0
#define PCIE_LINK_WIDTH_SHIFT        4
#define PCIE_LINK_SPEED_MASK         0x0F
#define PCIE_LINK_SPEED_2500         0x01
#define PCIE_LINK_SPEED_5000         0x02
#define PCIE_DEVICE_CONTROL2_16ms    0x0005

#ifndef ETH_ADDR_LEN
#define ETH_ADDR_LEN                 6
#endif

#define PHY_REVISION_MASK      0xFFFFFFF0
#define MAX_PHY_REG_ADDRESS    0x1F  /* 5 bit address bus (0-0x1F) */
#define MAX_PHY_MULTI_PAGE_REG 0xF

/* Bit definitions for valid PHY IDs. */
/*
 * I = Integrated
 * E = External
 */
#define M88E1000_E_PHY_ID    0x01410C50
#define M88E1000_I_PHY_ID    0x01410C30
#define M88E1011_I_PHY_ID    0x01410C20
#define IGP01E1000_I_PHY_ID  0x02A80380
#define M88E1011_I_REV_4     0x04
#define M88E1111_I_PHY_ID    0x01410CC0
#define GG82563_E_PHY_ID     0x01410CA0
#define IGP03E1000_E_PHY_ID  0x02A80390
#define IFE_E_PHY_ID         0x02A80330
#define IFE_PLUS_E_PHY_ID    0x02A80320
#define IFE_C_E_PHY_ID       0x02A80310
#define BME1000_E_PHY_ID     0x01410CB0
#define BME1000_E_PHY_ID_R2  0x01410CB1
#define I82577_E_PHY_ID 0x01540050
#define I82578_E_PHY_ID 0x004DD040
#define I82579_E_PHY_ID    0x01540090
#define I82580_I_PHY_ID      0x015403A0
#define IGP04E1000_E_PHY_ID  0x02A80391
#define M88_VENDOR           0x0141

/* M88E1000 Specific Registers */
#define M88E1000_PHY_SPEC_CTRL     0x10  /* PHY Specific Control Register */
#define M88E1000_PHY_SPEC_STATUS   0x11  /* PHY Specific Status Register */
#define M88E1000_INT_ENABLE        0x12  /* Interrupt Enable Register */
#define M88E1000_INT_STATUS        0x13  /* Interrupt Status Register */
#define M88E1000_EXT_PHY_SPEC_CTRL 0x14  /* Extended PHY Specific Control */
#define M88E1000_RX_ERR_CNTR       0x15  /* Receive Error Counter */

#define M88E1000_PHY_EXT_CTRL      0x1A  /* PHY extend control register */
#define M88E1000_PHY_PAGE_SELECT   0x1D  /* Reg 29 for page number setting */
#define M88E1000_PHY_GEN_CONTROL   0x1E  /* Its meaning depends on reg 29 */
#define M88E1000_PHY_VCO_REG_BIT8  0x100 /* Bits 8 & 11 are adjusted for */
#define M88E1000_PHY_VCO_REG_BIT11 0x800    /* improved BER performance */

/* M88E1000 PHY Specific Control Register */
#define M88E1000_PSCR_JABBER_DISABLE    0x0001 /* 1=Jabber Function disabled */
#define M88E1000_PSCR_POLARITY_REVERSAL 0x0002 /* 1=Polarity Reverse enabled */
#define M88E1000_PSCR_SQE_TEST          0x0004 /* 1=SQE Test enabled */
/* 1=CLK125 low, 0=CLK125 toggling */
#define M88E1000_PSCR_CLK125_DISABLE    0x0010
#define M88E1000_PSCR_MDI_MANUAL_MODE  0x0000 /* MDI Crossover Mode bits 6:5 */
                                               /* Manual MDI configuration */
#define M88E1000_PSCR_MDIX_MANUAL_MODE 0x0020  /* Manual MDIX configuration */
/* 1000BASE-T: Auto crossover, 100BASE-TX/10BASE-T: MDI Mode */
#define M88E1000_PSCR_AUTO_X_1000T     0x0040
/* Auto crossover enabled all speeds */
#define M88E1000_PSCR_AUTO_X_MODE      0x0060
/*
 * 1=Enable Extended 10BASE-T distance (Lower 10BASE-T Rx Threshold
 * 0=Normal 10BASE-T Rx Threshold
 */
#define M88E1000_PSCR_EN_10BT_EXT_DIST 0x0080
/* 1=5-bit interface in 100BASE-TX, 0=MII interface in 100BASE-TX */
#define M88E1000_PSCR_MII_5BIT_ENABLE      0x0100
#define M88E1000_PSCR_SCRAMBLER_DISABLE    0x0200 /* 1=Scrambler disable */
#define M88E1000_PSCR_FORCE_LINK_GOOD      0x0400 /* 1=Force link good */
#define M88E1000_PSCR_ASSERT_CRS_ON_TX     0x0800 /* 1=Assert CRS on Tx */

/* M88E1000 PHY Specific Status Register */
#define M88E1000_PSSR_JABBER             0x0001 /* 1=Jabber */
#define M88E1000_PSSR_REV_POLARITY       0x0002 /* 1=Polarity reversed */
#define M88E1000_PSSR_DOWNSHIFT          0x0020 /* 1=Downshifted */
#define M88E1000_PSSR_MDIX               0x0040 /* 1=MDIX; 0=MDI */
/*
 * 0 = <50M
 * 1 = 50-80M
 * 2 = 80-110M
 * 3 = 110-140M
 * 4 = >140M
 */
#define M88E1000_PSSR_CABLE_LENGTH       0x0380
#define M88E1000_PSSR_LINK               0x0400 /* 1=Link up, 0=Link down */
#define M88E1000_PSSR_SPD_DPLX_RESOLVED  0x0800 /* 1=Speed & Duplex resolved */
#define M88E1000_PSSR_PAGE_RCVD          0x1000 /* 1=Page received */
#define M88E1000_PSSR_DPLX               0x2000 /* 1=Duplex 0=Half Duplex */
#define M88E1000_PSSR_SPEED              0xC000 /* Speed, bits 14:15 */
#define M88E1000_PSSR_10MBS              0x0000 /* 00=10Mbs */
#define M88E1000_PSSR_100MBS             0x4000 /* 01=100Mbs */
#define M88E1000_PSSR_1000MBS            0x8000 /* 10=1000Mbs */

#define M88E1000_PSSR_CABLE_LENGTH_SHIFT 7

/* M88E1000 Extended PHY Specific Control Register */
#define M88E1000_EPSCR_FIBER_LOOPBACK 0x4000 /* 1=Fiber loopback */
/*
 * 1 = Lost lock detect enabled.
 * Will assert lost lock and bring
 * link down if idle not seen
 * within 1ms in 1000BASE-T
 */
#define M88E1000_EPSCR_DOWN_NO_IDLE   0x8000
/*
 * Number of times we will attempt to autonegotiate before downshifting if we
 * are the master
 */
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK 0x0C00
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_1X   0x0000
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_2X   0x0400
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_3X   0x0800
#define M88E1000_EPSCR_MASTER_DOWNSHIFT_4X   0x0C00
/*
 * Number of times we will attempt to autonegotiate before downshifting if we
 * are the slave
 */
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK  0x0300
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_DIS   0x0000
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X    0x0100
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_2X    0x0200
#define M88E1000_EPSCR_SLAVE_DOWNSHIFT_3X    0x0300
#define M88E1000_EPSCR_TX_CLK_2_5     0x0060 /* 2.5 MHz TX_CLK */
#define M88E1000_EPSCR_TX_CLK_25      0x0070 /* 25  MHz TX_CLK */
#define M88E1000_EPSCR_TX_CLK_0       0x0000 /* NO  TX_CLK */


/* M88EC018 Rev 2 specific DownShift settings */
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_MASK  0x0E00
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_1X    0x0000
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_2X    0x0200
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_3X    0x0400
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_4X    0x0600
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_5X    0x0800
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_6X    0x0A00
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_7X    0x0C00
#define M88EC018_EPSCR_DOWNSHIFT_COUNTER_8X    0x0E00

#define I82578_EPSCR_DOWNSHIFT_ENABLE          0x0020
#define I82578_EPSCR_DOWNSHIFT_COUNTER_MASK    0x001C

/* BME1000 PHY Specific Control Register */
#define BME1000_PSCR_ENABLE_DOWNSHIFT   0x0800 /* 1 = enable downshift */

/*
 * Bits...
 * 15-5: page
 * 4-0: register offset
 */
#define GG82563_PAGE_SHIFT        5
#define GG82563_REG(page, reg)    \
        (((page) << GG82563_PAGE_SHIFT) | ((reg) & MAX_PHY_REG_ADDRESS))
#define GG82563_MIN_ALT_REG       30

/* GG82563 Specific Registers */
#define GG82563_PHY_SPEC_CTRL           \
        GG82563_REG(0, 16) /* PHY Specific Control */
#define GG82563_PHY_SPEC_STATUS         \
        GG82563_REG(0, 17) /* PHY Specific Status */
#define GG82563_PHY_INT_ENABLE          \
        GG82563_REG(0, 18) /* Interrupt Enable */
#define GG82563_PHY_SPEC_STATUS_2       \
        GG82563_REG(0, 19) /* PHY Specific Status 2 */
#define GG82563_PHY_RX_ERR_CNTR         \
        GG82563_REG(0, 21) /* Receive Error Counter */
#define GG82563_PHY_PAGE_SELECT         \
        GG82563_REG(0, 22) /* Page Select */
#define GG82563_PHY_SPEC_CTRL_2         \
        GG82563_REG(0, 26) /* PHY Specific Control 2 */
#define GG82563_PHY_PAGE_SELECT_ALT     \
        GG82563_REG(0, 29) /* Alternate Page Select */
#define GG82563_PHY_TEST_CLK_CTRL       \
        GG82563_REG(0, 30) /* Test Clock Control (use reg. 29 to select) */

#define GG82563_PHY_MAC_SPEC_CTRL       \
        GG82563_REG(2, 21) /* MAC Specific Control Register */
#define GG82563_PHY_MAC_SPEC_CTRL_2     \
        GG82563_REG(2, 26) /* MAC Specific Control 2 */

#define GG82563_PHY_DSP_DISTANCE    \
        GG82563_REG(5, 26) /* DSP Distance */

/* Page 193 - Port Control Registers */
#define GG82563_PHY_KMRN_MODE_CTRL   \
        GG82563_REG(193, 16) /* Kumeran Mode Control */
#define GG82563_PHY_PORT_RESET          \
        GG82563_REG(193, 17) /* Port Reset */
#define GG82563_PHY_REVISION_ID         \
        GG82563_REG(193, 18) /* Revision ID */
#define GG82563_PHY_DEVICE_ID           \
        GG82563_REG(193, 19) /* Device ID */
#define GG82563_PHY_PWR_MGMT_CTRL       \
        GG82563_REG(193, 20) /* Power Management Control */
#define GG82563_PHY_RATE_ADAPT_CTRL     \
        GG82563_REG(193, 25) /* Rate Adaptation Control */

/* Page 194 - KMRN Registers */
#define GG82563_PHY_KMRN_FIFO_CTRL_STAT \
        GG82563_REG(194, 16) /* FIFO's Control/Status */
#define GG82563_PHY_KMRN_CTRL           \
        GG82563_REG(194, 17) /* Control */
#define GG82563_PHY_INBAND_CTRL         \
        GG82563_REG(194, 18) /* Inband Control */
#define GG82563_PHY_KMRN_DIAGNOSTIC     \
        GG82563_REG(194, 19) /* Diagnostic */
#define GG82563_PHY_ACK_TIMEOUTS        \
        GG82563_REG(194, 20) /* Acknowledge Timeouts */
#define GG82563_PHY_ADV_ABILITY         \
        GG82563_REG(194, 21) /* Advertised Ability */
#define GG82563_PHY_LINK_PARTNER_ADV_ABILITY \
        GG82563_REG(194, 23) /* Link Partner Advertised Ability */
#define GG82563_PHY_ADV_NEXT_PAGE       \
        GG82563_REG(194, 24) /* Advertised Next Page */
#define GG82563_PHY_LINK_PARTNER_ADV_NEXT_PAGE \
        GG82563_REG(194, 25) /* Link Partner Advertised Next page */
#define GG82563_PHY_KMRN_MISC           \
        GG82563_REG(194, 26) /* Misc. */

/* MDI Control */
#define E1000_MDIC_DATA_MASK 0x0000FFFF
#define E1000_MDIC_REG_MASK  0x001F0000
#define E1000_MDIC_REG_SHIFT 16
#define E1000_MDIC_PHY_MASK  0x03E00000
#define E1000_MDIC_PHY_SHIFT 21
#define E1000_MDIC_OP_WRITE  0x04000000
#define E1000_MDIC_OP_READ   0x08000000
#define E1000_MDIC_READY     0x10000000
#define E1000_MDIC_INT_EN    0x20000000
#define E1000_MDIC_ERROR     0x40000000
#define E1000_MDIC_DEST      0x80000000

/* SerDes Control */
#define E1000_GEN_CTL_READY             0x80000000
#define E1000_GEN_CTL_ADDRESS_SHIFT     8
#define E1000_GEN_POLL_TIMEOUT          640

/* LinkSec register fields */
#define E1000_LSECTXCAP_SUM_MASK        0x00FF0000
#define E1000_LSECTXCAP_SUM_SHIFT       16
#define E1000_LSECRXCAP_SUM_MASK        0x00FF0000
#define E1000_LSECRXCAP_SUM_SHIFT       16

#define E1000_LSECTXCTRL_EN_MASK        0x00000003
#define E1000_LSECTXCTRL_DISABLE        0x0
#define E1000_LSECTXCTRL_AUTH           0x1
#define E1000_LSECTXCTRL_AUTH_ENCRYPT   0x2
#define E1000_LSECTXCTRL_AISCI          0x00000020
#define E1000_LSECTXCTRL_PNTHRSH_MASK   0xFFFFFF00
#define E1000_LSECTXCTRL_RSV_MASK       0x000000D8

#define E1000_LSECRXCTRL_EN_MASK        0x0000000C
#define E1000_LSECRXCTRL_EN_SHIFT       2
#define E1000_LSECRXCTRL_DISABLE        0x0
#define E1000_LSECRXCTRL_CHECK          0x1
#define E1000_LSECRXCTRL_STRICT         0x2
#define E1000_LSECRXCTRL_DROP           0x3
#define E1000_LSECRXCTRL_PLSH           0x00000040
#define E1000_LSECRXCTRL_RP             0x00000080
#define E1000_LSECRXCTRL_RSV_MASK       0xFFFFFF33


/* DMA Coalescing register fields */
#define E1000_DMACR_DMACWT_MASK         0x00003FFF /* DMA Coalescing
                                                    * Watchdog Timer */
#define E1000_DMACR_DMACTHR_MASK        0x00FF0000 /* DMA Coalescing Receive
                                                    * Threshold */
#define E1000_DMACR_DMACTHR_SHIFT       16
#define E1000_DMACR_DMAC_LX_MASK        0x30000000 /* Lx when no PCIe
                                                    * transactions */
#define E1000_DMACR_DMAC_LX_SHIFT       28
#define E1000_DMACR_DMAC_EN             0x80000000 /* Enable DMA Coalescing */

#define E1000_DMCTXTH_DMCTTHR_MASK      0x00000FFF /* DMA Coalescing Transmit
                                                    * Threshold */

#define E1000_DMCTLX_TTLX_MASK          0x00000FFF /* Time to LX request */

#define E1000_DMCRTRH_UTRESH_MASK       0x0007FFFF /* Receive Traffic Rate
                                                    * Threshold */
#define E1000_DMCRTRH_LRPRCW            0x80000000 /* Rcv packet rate in
                                                    * current window */

#define E1000_DMCCNT_CCOUNT_MASK        0x01FFFFFF /* DMA Coal Rcv Traffic
                                                    * Current Cnt */

#define E1000_FCRTC_RTH_COAL_MASK       0x0003FFF0 /* Flow ctrl Rcv Threshold
                                                    * High val */
#define E1000_FCRTC_RTH_COAL_SHIFT      4
#define E1000_PCIEMISC_LX_DECISION      0x00000080 /* Lx power decision based
                                                      on DMA coal */


#endif /* _E1000_DEFINES_H_ */
