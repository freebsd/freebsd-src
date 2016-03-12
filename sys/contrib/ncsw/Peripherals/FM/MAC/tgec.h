/* Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
 @File          tgec.h

 @Description   FM 10G MAC ...
*//***************************************************************************/
#ifndef __TGEC_H
#define __TGEC_H

#include "std_ext.h"
#include "error_ext.h"
#include "list_ext.h"
#include "tgec_mii_acc.h"
#include "fm_mac.h"


/* Interrupt Mask Register (IMASK) */
#define IMASK_MDIO_SCAN_EVENTMDIO   0x00010000  /* MDIO_SCAN_EVENTMDIO scan event interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_MDIO_CMD_CMPL         0x00008000  /* 16 MDIO_CMD_CMPL MDIO command completion interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_REM_FAULT             0x00004000  /* 17 REM_FAULT Remote fault interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_LOC_FAULT             0x00002000  /* 18 LOC_FAULT Local fault interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_1TX_ECC_ER            0x00001000  /* 19 TX_ECC_ER Transmit frame ECC error interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_TX_FIFO_UNFL          0x00000800  /* 20 TX_FIFO_UNFL Transmit FIFO underflow interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_TX_FIFO_OVFL          0x00000400  /* 21 TX_FIFO_OVFL Transmit FIFO overflow interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_TX_ER                 0x00000200  /* 22 TX_ER Transmit frame error interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_FIFO_OVFL          0x00000100  /* 23 RX_FIFO_OVFL Receive FIFO overflow interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_ECC_ER             0x00000080  /* 24 RX_ECC_ER Receive frame ECC error interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_JAB_FRM            0x00000040  /* 25 RX_JAB_FRM Receive jabber frame interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_OVRSZ_FRM          0x00000020  /* 26 RX_OVRSZ_FRM Receive oversized frame interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_RUNT_FRM           0x00000010  /* 27 RX_RUNT_FRM Receive runt frame interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_FRAG_FRM           0x00000008  /* 28 RX_FRAG_FRM Receive fragment frame interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_LEN_ER             0x00000004  /* 29 RX_LEN_ER Receive payload length error interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_CRC_ER             0x00000002  /* 30 RX_CRC_ER Receive CRC error interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */
#define IMASK_RX_ALIGN_ER           0x00000001  /* 31 RX_ALIGN_ER Receive alignment error interrupt mask.
                                                 * 0 masked
                                                 * 1 enabled
                                                 */

#define EVENTS_MASK                 ((uint32_t)(IMASK_MDIO_SCAN_EVENTMDIO |  \
                                                IMASK_MDIO_CMD_CMPL       |  \
                                                IMASK_REM_FAULT           |  \
                                                IMASK_LOC_FAULT           |  \
                                                IMASK_1TX_ECC_ER          |  \
                                                IMASK_TX_FIFO_UNFL        |  \
                                                IMASK_TX_FIFO_OVFL        |  \
                                                IMASK_TX_ER               |  \
                                                IMASK_RX_FIFO_OVFL        |  \
                                                IMASK_RX_ECC_ER           |  \
                                                IMASK_RX_JAB_FRM          |  \
                                                IMASK_RX_OVRSZ_FRM        |  \
                                                IMASK_RX_RUNT_FRM         |  \
                                                IMASK_RX_FRAG_FRM         |  \
                                                IMASK_RX_LEN_ER           |  \
                                                IMASK_RX_CRC_ER           |  \
                                                IMASK_RX_ALIGN_ER))

#define GET_EXCEPTION_FLAG(bitMask, exception)       switch(exception){ \
    case e_FM_MAC_EX_10G_MDIO_SCAN_EVENTMDIO:                                    \
        bitMask = IMASK_MDIO_SCAN_EVENTMDIO; break;                              \
    case e_FM_MAC_EX_10G_MDIO_CMD_CMPL:                                          \
        bitMask = IMASK_MDIO_CMD_CMPL      ; break;                              \
    case e_FM_MAC_EX_10G_REM_FAULT:                                              \
        bitMask = IMASK_REM_FAULT          ; break;                              \
    case e_FM_MAC_EX_10G_LOC_FAULT:                                              \
        bitMask = IMASK_LOC_FAULT          ; break;                              \
    case e_FM_MAC_EX_10G_1TX_ECC_ER:                                             \
        bitMask = IMASK_1TX_ECC_ER         ; break;                              \
    case e_FM_MAC_EX_10G_TX_FIFO_UNFL:                                           \
        bitMask = IMASK_TX_FIFO_UNFL       ; break;                              \
    case e_FM_MAC_EX_10G_TX_FIFO_OVFL:                                           \
        bitMask = IMASK_TX_FIFO_OVFL       ; break;                              \
    case e_FM_MAC_EX_10G_TX_ER:                                                  \
        bitMask = IMASK_TX_ER              ; break;                              \
    case e_FM_MAC_EX_10G_RX_FIFO_OVFL:                                           \
        bitMask = IMASK_RX_FIFO_OVFL       ; break;                              \
    case e_FM_MAC_EX_10G_RX_ECC_ER:                                              \
        bitMask = IMASK_RX_ECC_ER          ; break;                              \
    case e_FM_MAC_EX_10G_RX_JAB_FRM:                                             \
        bitMask = IMASK_RX_JAB_FRM         ; break;                              \
    case e_FM_MAC_EX_10G_RX_OVRSZ_FRM:                                           \
        bitMask = IMASK_RX_OVRSZ_FRM       ; break;                              \
    case e_FM_MAC_EX_10G_RX_RUNT_FRM:                                            \
        bitMask = IMASK_RX_RUNT_FRM        ; break;                              \
    case e_FM_MAC_EX_10G_RX_FRAG_FRM:                                            \
        bitMask = IMASK_RX_FRAG_FRM        ; break;                              \
    case e_FM_MAC_EX_10G_RX_LEN_ER:                                              \
        bitMask = IMASK_RX_LEN_ER          ; break;                              \
    case e_FM_MAC_EX_10G_RX_CRC_ER:                                              \
        bitMask = IMASK_RX_CRC_ER          ; break;                              \
    case e_FM_MAC_EX_10G_RX_ALIGN_ER:                                            \
        bitMask = IMASK_RX_ALIGN_ER        ; break;                              \
    default: bitMask = 0;break;}


/* Default Config Params */
#define DEFAULT_wanModeEnable               FALSE
#define DEFAULT_promiscuousModeEnable       FALSE


#define DEFAULT_pauseForwardEnable          FALSE
#define DEFAULT_pauseIgnore                 FALSE
#define DEFAULT_txAddrInsEnable             FALSE

#define DEFAULT_loopbackEnable              FALSE
#define DEFAULT_cmdFrameEnable              FALSE
#define DEFAULT_rxErrorDiscard              FALSE
#define DEFAULT_phyTxenaOn                  FALSE
#define DEFAULT_sendIdleEnable              FALSE
#define DEFAULT_noLengthCheckEnable         TRUE
#define DEFAULT_lgthCheckNostdr             FALSE
#define DEFAULT_timeStampEnable             FALSE
#define DEFAULT_rxSfdAny                    FALSE
#define DEFAULT_rxPblFwd                    FALSE
#define DEFAULT_txPblFwd                    FALSE
#define DEFAULT_txIpgLength                 12

#define DEFAULT_maxFrameLength              0x600

#define DEFAULT_debugMode                   FALSE
#define DEFAULT_pauseTime                   0xf000
#define DEFAULT_imask                       0xf000
#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
#define DEFAULT_skipFman11Workaround        FALSE
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */

#define DEFAULT_exceptions          ((uint32_t)(IMASK_MDIO_SCAN_EVENTMDIO |  \
                                                IMASK_REM_FAULT           |  \
                                                IMASK_LOC_FAULT           |  \
                                                IMASK_1TX_ECC_ER          |  \
                                                IMASK_TX_FIFO_UNFL        |  \
                                                IMASK_TX_FIFO_OVFL        |  \
                                                IMASK_TX_ER               |  \
                                                IMASK_RX_FIFO_OVFL        |  \
                                                IMASK_RX_ECC_ER           |  \
                                                IMASK_RX_JAB_FRM          |  \
                                                IMASK_RX_OVRSZ_FRM        |  \
                                                IMASK_RX_RUNT_FRM         |  \
                                                IMASK_RX_FRAG_FRM         |  \
                                                IMASK_RX_CRC_ER           |  \
                                                IMASK_RX_ALIGN_ER))

#define MAX_PACKET_ALIGNMENT        31
#define MAX_INTER_PACKET_GAP        0x7f
#define MAX_INTER_PALTERNATE_BEB    0x0f
#define MAX_RETRANSMISSION          0x0f
#define MAX_COLLISION_WINDOW        0x03ff


#define TGEC_NUM_OF_PADDRS          1                   /* number of pattern match registers (entries) */

#define GROUP_ADDRESS               0x0000010000000000LL /* Group address bit indication */

#define HASH_TABLE_SIZE             512                 /* Hash table size (= 32 bits * 8 regs) */

#define TGEC_TO_MII_OFFSET          0x1030              /* Offset from the MEM map to the MDIO mem map */

/* 10-gigabit Ethernet MAC Controller ID (10GEC_ID) */
#define TGEC_ID_ID                  0xffff0000
#define TGEC_ID_MAC_VERSION         0x0000FF00
#define TGEC_ID_MAC_REV             0x000000ff

/* Command and Configuration Register (COMMAND_CONFIG) */
#define CMD_CFG_TX_PBL_FWD          0x00800000  /* 08 Transmit Preamble Forwarding (custom preamble).
                                                 */
#define CMD_CFG_RX_PBL_FWD          0x00400000  /* 09 Receive Preamble Forwarding (custom preamble).
                                                 */
#define RX_SFD_ANY                  0x00200000  /* 10 Enables, when set, that any character is allowed at the SFD position of the preamble and the frame will be accepted.
                                                 */
#define CMD_CFG_EN_TIMESTAMP        0x00100000  /* 11 EN_TIMESTAMP IEEE 1588 timeStamp functionality control.
                                                 * 0 disabled
                                                 * 1 enabled
                                                 */
#define CMD_CFG_TX_ADDR_INS_SEL     0x00080000  /* 12 TX_ADDR_INS_SEL Transmit MAC address select
                                                 * 0 insert using first MAC address
                                                 * 1 insert using second MAC address
                                                 */
#define CMD_CFG_LEN_CHK_NOSTDR      0x00040000  /* 13 LEN_CHK_NOSTDR
                                                 */
#define CMD_CFG_NO_LEN_CHK          0x00020000  /* 14 NO_LEN_CHK Payload length check disable
                                                 * 0 MAC compares the frame payload length with the frame length/type field.
                                                 * 1 Payload length check is disabled.
                                                 */
#define CMD_CFG_SEND_IDLE           0x00010000  /* 15 SEND_IDLE Force idle generation
                                                 * 0 Normal operation.
                                                * 1 MAC permanently sends XGMII idle sequences even when faults are received.
                                                 */
#define CMD_CFG_PHY_TX_EN           0x00008000  /* 16 PHY_TX_EN PHY transmit enable
                                                 * 0 PHY transmit is disabled.
                                                 * 1 PHY transmit is enabled.
                                                 */
#define CMD_CFG_RX_ER_DISC          0x00004000  /* 17 RX_ER_DISC Receive error frame discard enable
                                                 * 0 Received error frames are processed.
                                                 * 1 Any frame received with an error is discarded.
                                                 */
#define CMD_CFG_CMD_FRM_EN          0x00002000  /* 18 CMD_FRM_EN Command frame reception enable
                                                 * 0 Only Pause frames are accepted (all other command frames are rejected).
                                                 * 1 All command frames are accepted.
                                                 */
#define CMD_CFG_STAT_CLR            0x00001000  /* 19 STAT_CLR Clear statistics
                                                 * 0 Normal operations.
                                                 * 1 All statistics counters are cleared.
                                                 */
#define CMD_CFG_LOOPBACK_EN         0x00000400  /* 21 LOOPBAC_EN PHY interface loopback enable
                                                 * 0 Configure PHY for normal operation.
                                                 * 1 Configure PHY for loopback mode.
                                                 */
#define CMD_CFG_TX_ADDR_INS         0x00000200  /* 22 TX_ADDR_INS Transmit source MAC address insertion
                                                 * 0 MAC transmits the source MAC address unmodified.
                                                 * 1 MAC overwrites the source MAC address with address specified by COMMAND_CONFIG[TX_ADDR_INS_SEL].
                                                 */
#define CMD_CFG_PAUSE_IGNORE        0x00000100  /* 23 PAUSE_IGNORE Ignore Pause frame quanta
                                                 * 0 MAC stops transmit process for the duration specified in the Pause frame quanta of a received Pause frame.
                                                 * 1 MAC ignores received Pause frames.
                                                 */
#define CMD_CFG_PAUSE_FWD           0x00000080  /* 24 PAUSE_FWD Terminate/forward received Pause frames
                                                 * 0 MAC terminates and discards received Pause frames.
                                                 * 1 MAC forwards Pause frames to the user application.
                                                 */
#define CMD_CFG_PROMIS_EN           0x00000010  /* 27 PROMIS_EN Promiscuous operation enable
                                                 * 0 Unicast frames with a destination address not matching the core MAC address (defined by registers, MAC_ADDR_0 and MAC_ADDR_1) are rejected.
                                                 * 1 All frames are received without any MAC address filtering.
                                                 */
#define CMD_CFG_WAN_MODE            0x00000008  /* 28 WAN_MODE WAN mode enable
                                                 * 0 Configure MAC for LAN mode.
                                                 * 1 Configure MAC for WAN mode.
                                                 */
#define CMD_CFG_RX_EN               0x00000002  /* 30 RX_EN MAC receive path enable
                                                 * 0 MAC receive path is disabled
                                                 * 1 MAC receive path is enabled.
                                                 */
#define CMD_CFG_TX_EN               0x00000001  /* 31 TX_EN MAC transmit path enable
                                                 * 0 MAC transmit path is disabled
                                                 * 1 MAC transmit path is enabled.
                                                 */

/* Hashtable Control Register (HASHTABLE_CTRL) */
#define HASH_CTRL_MCAST_SHIFT       23

#define HASH_CTRL_MCAST_RD          0x00000400  /* 22 MCAST_READ Entry Multicast frame reception for the hash entry.
                                                 * 0 disabled
                                                 * 1 enabled
                                                 */
#define HASH_CTRL_MCAST_EN          0x00000200  /* 22 MCAST_EN Multicast frame reception for the hash entry.
                                                 * 0 disabled
                                                 * 1 enabled
                                                 */
#define HASH_ADDR_MASK              0x000001ff  /* 23-31 HASH_ADDR Hash table address code.
                                                 */

/* Transmit Inter-Packet Gap Length Register (TX_IPG_LENGTH) */
#define TX_IPG_LENGTH_MASK          0x000003ff



#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

/*
 * 10G memory map
 */
typedef _Packed struct {
/* 10Ge General Control and Status */
    volatile uint32_t   tgec_id;            /* 0x000 10GEC_ID - Controller ID register */
    volatile uint32_t   scratch;            /* 0x004  */
    volatile uint32_t   cmd_conf_ctrl;      /* 0x008 COMMAND_CONFIG - Control and configuration register */
    volatile uint32_t   mac_addr_0;         /* 0x00C MAC_ADDR_0 - Lower 32 bits of the first 48-bit MAC address */
    volatile uint32_t   mac_addr_1;         /* 0x010 MAC_ADDR_1 - Upper 16 bits of the first 48-bit MAC address */
    volatile uint32_t   maxfrm;             /* 0x014 MAXFRM - Maximum frame length register */
    volatile uint32_t   pause_quant;        /* 0x018 PAUSE_QUANT - Pause quanta register */
    volatile uint32_t   rx_fifo_sections;   /* 0x01c  */
    volatile uint32_t   tx_fifo_sections;   /* 0x020  */
    volatile uint32_t   rx_fifo_almost_f_e; /* 0x024  */
    volatile uint32_t   tx_fifo_almost_f_e; /* 0x028  */
    volatile uint32_t   hashtable_ctrl;     /* 0x02C HASHTABLE_CTRL - Hash table control register */
    volatile uint32_t   mdio_cfg_status;    /* 0x030  */
    volatile uint32_t   mdio_command;       /* 0x034  */
    volatile uint32_t   mdio_data;          /* 0x038  */
    volatile uint32_t   mdio_regaddr;       /* 0x03c  */
    volatile uint32_t   status;             /* 0x040  */
    volatile uint32_t   tx_ipg_len;         /* 0x044 TX_IPG_LENGTH - Transmitter inter-packet-gap register */
    volatile uint32_t   mac_addr_2;         /* 0x048 MAC_ADDR_2 - Lower 32 bits of the second 48-bit MAC address */
    volatile uint32_t   mac_addr_3;         /* 0x04C MAC_ADDR_3 - Upper 16 bits of the second 48-bit MAC address */
    volatile uint32_t   rx_fifo_ptr_rd;     /* 0x050  */
    volatile uint32_t   rx_fifo_ptr_wr;     /* 0x054  */
    volatile uint32_t   tx_fifo_ptr_rd;     /* 0x058  */
    volatile uint32_t   tx_fifo_ptr_wr;     /* 0x05c  */
    volatile uint32_t   imask;              /* 0x060 IMASK - Interrupt mask register */
    volatile uint32_t   ievent;             /* 0x064 IEVENT - Interrupt event register */
    volatile uint32_t   udp_port;           /* 0x068 Defines a UDP Port number. When an UDP/IP frame is received with a matching UDP destination port, the receive status indication pin ff_rx_ts_frm will be asserted.*/
    volatile uint32_t   type_1588v2;        /* 0x06c Type field for 1588v2 layer 2 frames. IEEE1588 defines the type 0x88f7 for 1588 frames. */
    volatile uint32_t   TENGEC_RESERVED4[4];
/*10Ge Statistics Counter */
    volatile uint64_t   TFRM;        /* 80 aFramesTransmittedOK */
    volatile uint64_t   RFRM;        /* 88 aFramesReceivedOK */
    volatile uint64_t   RFCS;        /* 90 aFrameCheckSequenceErrors */
    volatile uint64_t   RALN;        /* 98 aAlignmentErrors */
    volatile uint64_t   TXPF;        /* A0 aPAUSEMACCtrlFramesTransmitted */
    volatile uint64_t   RXPF;        /* A8 aPAUSEMACCtrlFramesReceived */
    volatile uint64_t   RLONG;       /* B0 aFrameTooLongErrors */
    volatile uint64_t   RFLR;        /* B8 aInRangeLengthErrors */
    volatile uint64_t   TVLAN;       /* C0 VLANTransmittedOK */
    volatile uint64_t   RVLAN;       /* C8 VLANReceivedOK */
    volatile uint64_t   TOCT;        /* D0 ifOutOctets */
    volatile uint64_t   ROCT;        /* D8 ifInOctets */
    volatile uint64_t   RUCA;        /* E0 ifInUcastPkts */
    volatile uint64_t   RMCA;        /* E8 ifInMulticastPkts */
    volatile uint64_t   RBCA;        /* F0 ifInBroadcastPkts */
    volatile uint64_t   TERR;        /* F8 ifOutErrors */
    volatile uint32_t   TENGEC_RESERVED6[2];
    volatile uint64_t   TUCA;        /* 108 ifOutUcastPkts */
    volatile uint64_t   TMCA;        /* 110 ifOutMulticastPkts */
    volatile uint64_t   TBCA;        /* 118 ifOutBroadcastPkts */
    volatile uint64_t   RDRP;        /* 120 etherStatsDropEvents */
    volatile uint64_t   REOCT;       /* 128 etherStatsOctets */
    volatile uint64_t   RPKT;        /* 130 etherStatsPkts */
    volatile uint64_t   TRUND;       /* 138 etherStatsUndersizePkts */
    volatile uint64_t   R64;         /* 140 etherStatsPkts64Octets */
    volatile uint64_t   R127;        /* 148 etherStatsPkts65to127Octets */
    volatile uint64_t   R255;        /* 150 etherStatsPkts128to255Octets */
    volatile uint64_t   R511;        /* 158 etherStatsPkts256to511Octets */
    volatile uint64_t   R1023;       /* 160 etherStatsPkts512to1023Octets */
    volatile uint64_t   R1518;       /* 168 etherStatsPkts1024to1518Octets */
    volatile uint64_t   R1519X;      /* 170 etherStatsPkts1519toX */
    volatile uint64_t   TROVR;       /* 178 etherStatsOversizePkts */
    volatile uint64_t   TRJBR;       /* 180 etherStatsJabbers */
    volatile uint64_t   TRFRG;       /* 188 etherStatsFragments */
    volatile uint64_t   RERR;        /* 190 ifInErrors */
} _PackedType t_TgecMemMap;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


typedef struct {
    bool wanModeEnable;             /* WAN Mode Enable. Sets WAN mode (1) or LAN mode (0, default) of operation. */
    bool promiscuousModeEnable;     /* Enables MAC promiscuous operation. When set to '1', all frames are received without any MAC address filtering, when set to '0' (Reset value) Unicast Frames with a destination address not matching the Core MAC Address (MAC Address programmed in Registers MAC_ADDR_0 and MAC_ADDR_1 or the MAC address programmed in Registers MAC_ADDR_2 and MAC_ADDR_3 ) are rejected. */
    bool pauseForwardEnable;        /* Terminate / Forward Pause Frames. If set to '1' pause frames are forwarded to the user application. When set to '0' (Reset value) pause frames are terminated and discarded within the MAC. */
    bool pauseIgnore;               /* Ignore Pause Frame Quanta. If set to '1' received pause frames are ignored by the MAC. When set to '0' (Reset value) the transmit process is stopped for the amount of time specified in the pause quanta received within a pause frame. */
    bool txAddrInsEnable;           /* Set Source MAC Address on Transmit.
                                        If set to '1' the MAC overwrites the source MAC address received from the Client Interface with one of the MAC addresses (Refer to section 10.4)
                                        If set to '0' (Reset value), the source MAC address from the Client Interface is transmitted unmodified to the line. */
    bool loopbackEnable;            /* PHY Interface Loopback. When set to '1', the signal loop_ena is set to '1', when set to '0' (Reset value) the signal loop_ena is set to '0'. */
    bool cmdFrameEnable;            /* Enables reception of all command frames. When set to '1' all Command Frames are accepted, when set to '0' (Reset Value) only Pause Frames are accepted and all other Command Frames are rejected. */
    bool rxErrorDiscard;            /* Receive Errored Frame Discard Enable. When set to ‘1’, any frame received with an error is discarded in the Core and not forwarded to the Client interface. When set to ‘0’ (Reset value), errored Frames are forwarded to the Client interface with ff_rx_err asserted. */
    bool phyTxenaOn;                /* PHY Transmit Enable. When set to '1', the signal phy_txena is set to '1', when set to '0' (Reset value) the signal phy_txena is set to '0' */
    bool sendIdleEnable;            /* Force Idle Generation. When set to '1', the MAC permanently sends XGMII Idle sequences even when faults are received. */
    bool noLengthCheckEnable;       /* Payload Length Check Disable. When set to ‘0’ (Reset value), the Core checks the frame's payload length with the Frame Length/Type field, when set to ‘1’, the payload length check is disabled. */
    bool lgthCheckNostdr;           /* The Core interprets the Length/Type field differently depending on the value of this Bit */
    bool timeStampEnable;           /* This bit selects between enabling and disabling the IEEE 1588 functionality.
                                        1: IEEE 1588 is enabled.
                                        0: IEEE 1588 is disabled. */
    bool rxSfdAny;                  /* Enables, when set, that any character is allowed at the SFD position of the preamble and the frame will be accepted.
                                        If cleared (default) the frame is accepted only if the 8th byte of the preamble contains the SFD value 0xd5. If another value is received, the frame is discarded and the alignment error counter increments. */
    bool rxPblFwd;                  /* Receive Preamble Forwarding (custom preamble).
                                        If set, the first word (ff_rx_sop) of every received frame contains the preamble of the frame. The frame data starts with the 2nd word from the FIFO.
                                        If the bit is cleared (default) the preamble is removed from the frame before it is written into the receive FIFO. */
    bool txPblFwd;                  /* Transmit Preamble Forwarding (custom preamble).
                                        If set, the first word written into the TX FIFO is considered as frame preamble. The MAC will not add a preamble in front of the frame. Note that bits 7:0 of the preamble word will still be overwritten with the XGMII start character upon transmission.
                                        If cleared (default) the MAC */
    uint32_t txIpgLength;           /*Transmit Inter-Packet-Gap (IPG) value.
                                      A 6-bit value: Depending on LAN or WAN mode of operation (see COMMAND_CONFIG, 19.2.1 page 91) the value has the following meaning:
                                        - LAN Mode: Number of octets in steps of 4. Valid values are 8, 12, 16, ... 100. DIC is fully supported (see 10.6.1 page 49) for any setting. A default of 12 (reset value) must be set to conform to IEEE802.3ae. Warning: When set to 8, PCS layers may not be able to perform clock rate compensation.
                                        - WAN Mode: Stretch factor. Valid values are 4..15. The stretch factor is calculated as (value+1)*8. A default of 12 (reset value) must be set to conform to IEEE 802.3ae (i.e. 13*8=104). A larger value shrinks the IPG (increasing bandwidth). */
/*.. */
    uint16_t    maxFrameLength;
    bool        debugMode;
    uint16_t    pauseTime;
#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
    bool        skipFman11Workaround;
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */
} t_TgecDriverParam;

typedef struct {
    t_FmMacControllerDriver     fmMacControllerDriver;              /**< Upper Mac control block */
    t_Handle                    h_App;                              /**< Handle to the upper layer application  */
    t_TgecMemMap                *p_MemMap;                          /**< pointer to 10G memory mapped registers. */
    t_TgecMiiAccessMemMap       *p_MiiMemMap;                       /**< pointer to MII memory mapped registers.          */
    uint64_t                    addr;                               /**< MAC address of device; */
    e_EnetMode                  enetMode;                           /**< Ethernet physical interface  */
    t_FmMacExceptionCallback    *f_Exception;
    int                         mdioIrq;
    t_FmMacExceptionCallback    *f_Event;
    bool                        indAddrRegUsed[TGEC_NUM_OF_PADDRS]; /**< Whether a particular individual address recognition register is being used */
    uint64_t                    paddr[TGEC_NUM_OF_PADDRS];          /**< MAC address for particular individual address recognition register */
    uint8_t                     numOfIndAddrInRegs;                 /**< Number of individual addresses in registers for this station. */
    t_EthHash                   *p_MulticastAddrHash;               /**< pointer to driver's global address hash table  */
    t_EthHash                   *p_UnicastAddrHash;                 /**< pointer to driver's individual address hash table  */
    bool                        debugMode;
    uint8_t                     macId;
    uint32_t                    exceptions;
    t_TgecDriverParam           *p_TgecDriverParam;
} t_Tgec;


t_Error TGEC_MII_WritePhyReg(t_Handle h_Tgec, uint8_t phyAddr, uint8_t reg, uint16_t data);
t_Error TGEC_MII_ReadPhyReg(t_Handle h_Tgec,  uint8_t phyAddr, uint8_t reg, uint16_t *p_Data);


#endif /* __TGEC_H */
