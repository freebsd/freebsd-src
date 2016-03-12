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
 @File          dtsec.h

 @Description   FM dTSEC ...
*//***************************************************************************/
#ifndef __DTSEC_H
#define __DTSEC_H

#include "std_ext.h"
#include "error_ext.h"
#include "list_ext.h"
#include "dtsec_mii_acc.h"
#include "fm_mac.h"


#define PEMASK_TSRE                 0x00010000

#define IMASK_BREN                  0x80000000
#define IMASK_RXCEN                 0x40000000
#define IMASK_MSROEN                0x04000000
#define IMASK_GTSCEN                0x02000000
#define IMASK_BTEN                  0x01000000
#define IMASK_TXCEN                 0x00800000
#define IMASK_TXEEN                 0x00400000
#define IMASK_LCEN                  0x00040000
#define IMASK_CRLEN                 0x00020000
#define IMASK_XFUNEN                0x00010000
#define IMASK_ABRTEN                0x00008000
#define IMASK_IFERREN               0x00004000
#define IMASK_MAGEN                 0x00000800
#define IMASK_MMRDEN                0x00000400
#define IMASK_MMWREN                0x00000200
#define IMASK_GRSCEN                0x00000100
#define IMASK_TDPEEN                0x00000002
#define IMASK_RDPEEN                0x00000001

#define EVENTS_MASK                 ((uint32_t)(IMASK_BREN    | \
                                                IMASK_RXCEN   | \
                                                IMASK_MSROEN  | \
                                                IMASK_GTSCEN  | \
                                                IMASK_BTEN    | \
                                                IMASK_TXCEN   | \
                                                IMASK_TXEEN   | \
                                                IMASK_ABRTEN  | \
                                                IMASK_LCEN    | \
                                                IMASK_CRLEN   | \
                                                IMASK_XFUNEN  | \
                                                IMASK_IFERREN | \
                                                IMASK_MAGEN   | \
                                                IMASK_MMRDEN  | \
                                                IMASK_MMWREN  | \
                                                IMASK_GRSCEN  | \
                                                IMASK_TDPEEN  | \
                                                IMASK_RDPEEN))

#define GET_EXCEPTION_FLAG(bitMask, exception)       switch(exception){ \
    case e_FM_MAC_EX_1G_BAB_RX:                                   \
        bitMask = IMASK_BREN; break;                              \
    case e_FM_MAC_EX_1G_RX_CTL:                                   \
        bitMask = IMASK_RXCEN; break;                             \
    case e_FM_MAC_EX_1G_GRATEFUL_TX_STP_COMPLET:                  \
        bitMask = IMASK_GTSCEN ; break;                           \
    case e_FM_MAC_EX_1G_BAB_TX:                                   \
        bitMask = IMASK_BTEN   ; break;                           \
    case e_FM_MAC_EX_1G_TX_CTL:                                   \
        bitMask = IMASK_TXCEN  ; break;                           \
    case e_FM_MAC_EX_1G_TX_ERR:                                   \
        bitMask = IMASK_TXEEN  ; break;                           \
    case e_FM_MAC_EX_1G_LATE_COL:                                 \
        bitMask = IMASK_LCEN   ; break;                           \
    case e_FM_MAC_EX_1G_COL_RET_LMT:                              \
        bitMask = IMASK_CRLEN  ; break;                           \
    case e_FM_MAC_EX_1G_TX_FIFO_UNDRN:                            \
        bitMask = IMASK_XFUNEN ; break;                           \
    case e_FM_MAC_EX_1G_MAG_PCKT:                                 \
        bitMask = IMASK_MAGEN ; break;                            \
    case e_FM_MAC_EX_1G_MII_MNG_RD_COMPLET:                       \
        bitMask = IMASK_MMRDEN; break;                            \
    case e_FM_MAC_EX_1G_MII_MNG_WR_COMPLET:                       \
        bitMask = IMASK_MMWREN  ; break;                          \
    case e_FM_MAC_EX_1G_GRATEFUL_RX_STP_COMPLET:                  \
        bitMask = IMASK_GRSCEN; break;                            \
    case e_FM_MAC_EX_1G_TX_DATA_ERR:                              \
        bitMask = IMASK_TDPEEN; break;                            \
    case e_FM_MAC_EX_1G_RX_MIB_CNT_OVFL:                          \
        bitMask = IMASK_MSROEN ; break;                           \
    default: bitMask = 0;break;}


#define MAX_PACKET_ALIGNMENT        31
#define MAX_INTER_PACKET_GAP        0x7f
#define MAX_INTER_PALTERNATE_BEB    0x0f
#define MAX_RETRANSMISSION          0x0f
#define MAX_COLLISION_WINDOW        0x03ff


/********************* From mac ext ******************************************/
typedef  uint32_t t_ErrorDisable;

#define ERROR_DISABLE_TRANSMIT              0x00400000
#define ERROR_DISABLE_LATE_COLLISION        0x00040000
#define ERROR_DISABLE_COLLISION_RETRY_LIMIT 0x00020000
#define ERROR_DISABLE_TxFIFO_UNDERRUN       0x00010000
#define ERROR_DISABLE_TxABORT               0x00008000
#define ERROR_DISABLE_INTERFACE             0x00004000
#define ERROR_DISABLE_TxDATA_PARITY         0x00000002
#define ERROR_DISABLE_RxDATA_PARITY         0x00000001

/*****************************************************************************/
#define DTSEC_NUM_OF_PADDRS             15  /* number of pattern match registers (entries) */

#define GROUP_ADDRESS                   0x0000010000000000LL /* Group address bit indication */

#define HASH_TABLE_SIZE                 256 /* Hash table size (= 32 bits * 8 regs) */

#define DTSEC_TO_MII_OFFSET             0x1120  /* number of pattern match registers (entries) */

#define DEFAULT_errorDisabled           0
#define DEFAULT_promiscuousEnable       FALSE
#define DEFAULT_pauseExtended           0x0
#define DEFAULT_pauseTime               0xf000
#define DEFAULT_halfDuplex              FALSE
#define DEFAULT_halfDulexFlowControlEn  FALSE
#define DEFAULT_txTimeStampEn           FALSE
#define DEFAULT_rxTimeStampEn           FALSE
#define DEFAULT_packetAlignment         0
#define DEFAULT_controlFrameAccept      FALSE
#define DEFAULT_groupHashExtend         FALSE
#define DEFAULT_broadcReject            FALSE
#define DEFAULT_rxShortFrame            TRUE
#define DEFAULT_exactMatch              FALSE
#define DEFAULT_debugMode               FALSE
#define DEFAULT_loopback                FALSE
#define DEFAULT_actOnRxPauseFrame       TRUE
#define DEFAULT_actOnTxPauseFrame       TRUE

#define DEFAULT_PreAmLength             0x7
#define DEFAULT_PreAmRxEn               FALSE
#define DEFAULT_PreAmTxEn               FALSE
#define DEFAULT_lengthCheckEnable       FALSE
#define DEFAULT_padAndCrcEnable         TRUE
#define DEFAULT_crcEnable               FALSE

#define DEFAULT_nonBackToBackIpg1       0x40
#define DEFAULT_nonBackToBackIpg2       0x60
#define DEFAULT_minIfgEnforcement       0x50
#define DEFAULT_backToBackIpg           0x60

#define DEFAULT_altBackoffVal           0x0A
#define DEFAULT_altBackoffEnable        FALSE
#define DEFAULT_backPressureNoBackoff   FALSE
#define DEFAULT_noBackoff               FALSE
#define DEFAULT_excessDefer             TRUE
#define DEFAULT_maxRetransmission       0x0F
#define DEFAULT_collisionWindow         0x37

#define DEFAULT_maxFrameLength          0x600

#define DEFAULT_collisionWindow         0x37

#define DEFAULT_fifoTxThr               0x10
#define DEFAULT_fifoTxWatermarkH        0x7e
#define DEFAULT_fifoRxWatermarkL        0x08
#define DEFAULT_tbiPhyAddr              5

#define DEFAULT_exceptions              ((uint32_t)(IMASK_BREN    | \
                                                    IMASK_RXCEN   | \
                                                    IMASK_BTEN    | \
                                                    IMASK_TXCEN   | \
                                                    IMASK_TXEEN   | \
                                                    IMASK_ABRTEN  | \
                                                    IMASK_LCEN    | \
                                                    IMASK_CRLEN   | \
                                                    IMASK_XFUNEN  | \
                                                    IMASK_IFERREN | \
                                                    IMASK_MAGEN   | \
                                                    IMASK_TDPEEN  | \
                                                    IMASK_RDPEEN))


#define MAX_PHYS                    32 /* maximum number of phys */

#define DTSEC_ID1_ID                0xffff0000
#define DTSEC_ID1_REV_MJ            0x0000FF00
#define DTSEC_ID1_REV_MN            0x000000ff

#define ID2_INT_REDUCED_OFF         0x00010000
#define ID2_INT_NORMAL_OFF          0x00020000

#define ECNTRL_CLRCNT               0x00004000
#define ECNTRL_AUTOZ                0x00002000
#define ECNTRL_STEN                 0x00001000
#define ECNTRL_CFG_RO               0x80000000
#define ECNTRL_GMIIM                0x00000040
#define ECNTRL_TBIM                 0x00000020
#define ECNTRL_SGMIIM               0x00000002
#define ECNTRL_RPM                  0x00000010
#define ECNTRL_R100M                0x00000008
#define ECNTRL_RMM                  0x00000004
#define ECNTRL_QSGMIIM              0x00000001

#define TCTRL_THDF                  0x00000800
#define TCTRL_TTSE                  0x00000040
#define TCTRL_GTS                   0x00000020
#define TCTRL_TFC_PAUSE             0x00000010

/* PTV offsets */
#define PTV_PTE_OFST                16

#define RCTRL_CFA                   0x00008000
#define RCTRL_GHTX                  0x00000400
#define RCTRL_RTSE                  0x00000040
#define RCTRL_GRS                   0x00000020
#define RCTRL_BC_REJ                0x00000010
#define RCTRL_MPROM                 0x00000008
#define RCTRL_RSF                   0x00000004
#define RCTRL_EMEN                  0x00000002
#define RCTRL_UPROM                 0x00000001
#define RCTRL_PROM                  (RCTRL_UPROM | RCTRL_MPROM)

#define TMR_CTL_ESFDP               0x00000800
#define TMR_CTL_ESFDE               0x00000400

#define TSEC_ID1_DEBUG              0x00e00c00
#define DEBUG_ENABLE                0x80000000
#define DPERROR_Tx_ERROR_ON_SEC     0x00400000
#define DPERROR_Tx_ERROR_ON_WRITE   0x10000000
#define DPERROR_Rx_ERROR_ON_SEC     0x00000040
#define DPERROR_Rx_ERROR_ON_WRITE   0x00001000
#define DPERROR_STT                 0x80000000
#define DPERROR_STR                 0x00008000

#define MACCFG1_SOFT_RESET          0x80000000
#define MACCFG1_LOOPBACK            0x00000100
#define MACCFG1_RX_FLOW             0x00000020
#define MACCFG1_TX_FLOW             0x00000010
#define MACCFG1_TX_EN               0x00000001
#define MACCFG1_RX_EN               0x00000004
#define MACCFG1_RESET_RxMC          0x00080000
#define MACCFG1_RESET_TxMC          0x00040000
#define MACCFG1_RESET_RxFUN         0x00020000
#define MACCFG1_RESET_TxFUN         0x00010000

#define MACCFG2_NIBBLE_MODE         0x00000100
#define MACCFG2_BYTE_MODE           0x00000200
#define MACCFG2_PRE_AM_Rx_EN        0x00000080
#define MACCFG2_PRE_AM_Tx_EN        0x00000040
#define MACCFG2_LENGTH_CHECK        0x00000010
#define MACCFG2_MAGIC_PACKET_EN     0x00000008
#define MACCFG2_PAD_CRC_EN          0x00000004
#define MACCFG2_CRC_EN              0x00000002
#define MACCFG2_FULL_DUPLEX         0x00000001

#define PREAMBLE_LENGTH_SHIFT       12

#define IPGIFG_NON_BACK_TO_BACK_IPG_1_SHIFT    24
#define IPGIFG_NON_BACK_TO_BACK_IPG_2_SHIFT    16
#define IPGIFG_MIN_IFG_ENFORCEMENT_SHIFT        8

#define IPGIFG_NON_BACK_TO_BACK_IPG_1    0x7F000000
#define IPGIFG_NON_BACK_TO_BACK_IPG_2    0x007F0000
#define IPGIFG_MIN_IFG_ENFORCEMENT       0x0000FF00
#define IPGIFG_BACK_TO_BACK_IPG          0x0000007F

#define HAFDUP_ALT_BEB                   0x00080000
#define HAFDUP_BP_NO_BACKOFF             0x00040000
#define HAFDUP_NO_BACKOFF                0x00020000
#define HAFDUP_EXCESS_DEFER              0x00010000
#define HAFDUP_COLLISION_WINDOW          0x000003ff

#define HAFDUP_ALTERNATE_BEB_TRUNCATION_SHIFT    20
#define HAFDUP_RETRANSMISSION_MAX_SHIFT          12
#define HAFDUP_RETRANSMISSION_MAX       0x0000f000

#define NUM_OF_HASH_REGS     8 /* Number of hash table registers */

#define DEBUG_GET_FIFO_READ_INDEX       0x007f0000
#define DEBUG_GET_FIFO_WRITE_INDEX      0x0000007f
/* Pause Time Value Register  */
#define PTV_PTE_SHIFT    16

#define     MASK22BIT   0x003FFFFF
#define     MASK16BIT   0x0000FFFF
#define     MASK12BIT   0x00000FFF
#define     MASK8BIT    0x000000FF

#define     VAL32BIT    0x100000000LL
#define     VAL22BIT    0x00400000
#define     VAL16BIT    0x00010000
#define     VAL12BIT    0x00001000

/* PHY Control Register */
#define PHY_CR_LOOPBACK     0x4000
#define PHY_CR_SPEED0       0x2000
#define PHY_CR_ANE          0x1000
#define PHY_CR_FULLDUPLEX   0x0100
#define PHY_CR_SPEED1       0x0040

#define PHY_TBICON_SRESET   0x8000
#define PHY_TBICON_SPEED2   0x0020

/* CAR1/2 bits */
#define CAR1_TR64   0x80000000
#define CAR1_TR127  0x40000000
#define CAR1_TR255  0x20000000
#define CAR1_TR511  0x10000000
#define CAR1_TRK1   0x08000000
#define CAR1_TRMAX  0x04000000
#define CAR1_TRMGV  0x02000000

#define CAR1_RBYT   0x00010000
#define CAR1_RPKT   0x00008000
#define CAR1_RMCA   0x00002000
#define CAR1_RBCA   0x00001000
#define CAR1_RXPF   0x00000400
#define CAR1_RALN   0x00000100
#define CAR1_RFLR   0x00000080
#define CAR1_RCDE   0x00000040
#define CAR1_RCSE   0x00000020
#define CAR1_RUND   0x00000010
#define CAR1_ROVR   0x00000008
#define CAR1_RFRG   0x00000004
#define CAR1_RJBR   0x00000002
#define CAR1_RDRP   0x00000001

#define CAR2_TFCS   0x00040000
#define CAR2_TBYT   0x00002000
#define CAR2_TPKT   0x00001000
#define CAR2_TMCA   0x00000800
#define CAR2_TBCA   0x00000400
#define CAR2_TXPF   0x00000200
#define CAR2_TDRP   0x00000001

#define CAM1_ERRORS_ONLY (CAR1_RXPF |   \
                            CAR1_RALN | \
                            CAR1_RFLR | \
                            CAR1_RCDE | \
                            CAR1_RCSE | \
                            CAR1_RUND | \
                            CAR1_ROVR | \
                            CAR1_RFRG | \
                            CAR1_RJBR | \
                            CAR1_RDRP)

#define CAM2_ERRORS_ONLY (CAR2_TFCS | CAR2_TXPF | CAR2_TDRP)

typedef struct t_InternalStatistics
{
    uint64_t    tr64;
    uint64_t    tr127;
    uint64_t    tr255;
    uint64_t    tr511;
    uint64_t    tr1k;
    uint64_t    trmax;
    uint64_t    trmgv;
    uint64_t    rfrg;
    uint64_t    rjbr;
    uint64_t    rdrp;
    uint64_t    raln;
    uint64_t    rund;
    uint64_t    rovr;
    uint64_t    rxpf;
    uint64_t    txpf;
    uint64_t    rbyt;
    uint64_t    rpkt;
    uint64_t    rmca;
    uint64_t    rbca;
    uint64_t    rflr;
    uint64_t    rcde;
    uint64_t    rcse;
    uint64_t    tbyt;
    uint64_t    tpkt;
    uint64_t    tmca;
    uint64_t    tbca;
    uint64_t    tdrp;
    uint64_t    tfcs;
} t_InternalStatistics;

#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

typedef _Packed struct
{
    uint32_t exact_match1; /* octets 1-4 */
    uint32_t exact_match2; /* octets 5-6 */
} _PackedType macRegs;

typedef _Packed struct
{
    volatile uint32_t tsec_id1;             /* 0x000 ETSEC_ID register */
    volatile uint32_t tsec_id2;             /* 0x004 ETSEC_ID2 register */
    volatile uint32_t ievent;               /* 0x008 Interrupt event register */
    volatile uint32_t imask;                /* 0x00C Interrupt mask register */
    volatile uint32_t edis;                 /* 0x010 Error disabled register */
    volatile uint32_t ecntrl;               /* 0x014 E control register */
    volatile uint32_t ptv;                  /* 0x018 Pause time value register */
    volatile uint32_t tbipa;                /* 0x01C TBI PHY address register */
    volatile uint32_t tmr_ctrl;             /* 0x020 Time-stamp Control register */
    volatile uint32_t tmr_pevent;           /* 0x024 Time-stamp event register */
    volatile uint32_t tmr_pemask;           /* 0x028 Timer event mask register */
    volatile uint32_t DTSEC_RESERVED2;      /* 0x02C */
    volatile uint32_t iobistctl;            /* 0x030 IO BIST Control register */
    volatile uint32_t DTSEC_RESERVED3[3];   /* 0x034 */

    volatile uint32_t tctrl;                /* 0x040 Transmit control register */
    volatile uint32_t DTSEC_RESERVED4[3];   /* 0x044-0x04C */
    volatile uint32_t rctrl;                /* 0x050 Receive control register */
    volatile uint32_t DTSEC_RESERVED5[11];  /* 0x054- 0x07C */

    volatile uint32_t igaddr[8];            /* 0x080-0x09C Individual/group address registers 0-7 */
    volatile uint32_t gaddr[8];             /* 0x0A0-0x0BC Group address registers 0-7 */
    volatile uint32_t ETSEC_RESERVED6[16];  /* 0x0C0-0x0FC */

    volatile uint32_t maccfg1;              /* 0x100 MAC configuration #1 */
    volatile uint32_t maccfg2;              /* 0x104 MAC configuration #2 */
    volatile uint32_t ipgifg;               /* 0x108 IPG/IFG */
    volatile uint32_t hafdup;               /* 0x10C Half-duplex */
    volatile uint32_t maxfrm;               /* 0x110 Maximum frame */
    volatile uint32_t DTSEC_RESERVED7[3];   /* 0x114-0x11C register */
    t_MiiAccessMemMap miiMemMap;
    volatile uint32_t ifctrl;               /* 0x138 MII Mgmt:interface control */
    volatile uint32_t ifstat;               /* 0x13C Interface status */
    volatile uint32_t macstnaddr1;          /* 0x140 Station Address,part 1 */
    volatile uint32_t macstnaddr2;          /* 0x144 Station Address,part 2  */
    volatile macRegs  macaddr[DTSEC_NUM_OF_PADDRS]; /* 0x148-0x1BC mac exact match addresses 1-15, parts 1-2 */
    volatile uint32_t DTSEC_RESERVED8[16];  /* 0x1C0-0x1FC register */

    /*  RMON MIB REGISTERS  */
    /*  TRANSMIT and RECEIVE COUNTERS   */

    volatile uint32_t tr64;            /* 0x200 transmit and receive 64 byte frame counter */
    volatile uint32_t tr127;           /* 0x204 transmit and receive 65 to 127 byte frame counter */
    volatile uint32_t tr255;           /* 0x208 transmit and receive 128 to 255 byte frame counter */
    volatile uint32_t tr511;           /* 0x20C transmit and receive 256 to 511 byte frame counter */
    volatile uint32_t tr1k;            /* 0x210 transmit and receive 512 to 1023 byte frame counter */
    volatile uint32_t trmax;           /* 0x214 transmit and receive 1024 to 1518 byte frame counter */
    volatile uint32_t trmgv;           /* 0x218 transmit and receive 1519 to 1522 byte good VLAN frame count */

    /* RECEIVE COUNTERS */
    volatile uint32_t rbyt;            /* 0x21C receive byte counter */
    volatile uint32_t rpkt;            /* 0x220 receive packet counter */
    volatile uint32_t rfcs;            /* 0x224 receive FCS error counter */
    volatile uint32_t rmca;            /* 0x228 RMCA receive multicast packet counter */
    volatile uint32_t rbca;            /* 0x22C receive broadcast packet counter */
    volatile uint32_t rxcf;            /* 0x230 receive control frame packet counter */
    volatile uint32_t rxpf;            /* 0x234 receive PAUSE frame packet counter */
    volatile uint32_t rxuo;            /* 0x238 receive unknown OP code counter */
    volatile uint32_t raln;            /* 0x23C receive alignment error counter */
    volatile uint32_t rflr;            /* 0x240 receive frame length error counter */
    volatile uint32_t rcde;            /* 0x244 receive code error counter */
    volatile uint32_t rcse;            /* 0x248 receive carrier sense error counter */
    volatile uint32_t rund;            /* 0x24C receive undersize packet counter */
    volatile uint32_t rovr;            /* 0x250 receive oversize packet counter */
    volatile uint32_t rfrg;            /* 0x254 receive fragments counter */
    volatile uint32_t rjbr;            /* 0x258 receive jabber counter */
    volatile uint32_t rdrp;            /* 0x25C receive drop */

    /* TRANSMIT COUNTERS */
    volatile uint32_t tbyt;            /* 0x260 transmit byte counter */
    volatile uint32_t tpkt;            /* 0x264 transmit packet counter */
    volatile uint32_t tmca;            /* 0x268 transmit multicast packet counter */
    volatile uint32_t tbca;            /* 0x26C transmit broadcast packet counter */
    volatile uint32_t txpf;            /* 0x270 transmit PAUSE control frame counter */
    volatile uint32_t tdfr;            /* 0x274 transmit deferral packet counter */
    volatile uint32_t tedf;            /* 0x278 transmit excessive deferral packet counter */
    volatile uint32_t tscl;            /* 0x27C transmit single collision packet counter */
    volatile uint32_t tmcl;            /* 0x280 transmit multiple collision packet counter */
    volatile uint32_t tlcl;            /* 0x284 transmit late collision packet counter */
    volatile uint32_t txcl;            /* 0x288 transmit excessive collision packet counter */
    volatile uint32_t tncl;            /* 0x28C transmit total collision counter */
    volatile uint32_t DTSEC_RESERVED9; /* 0x290 */
    volatile uint32_t tdrp;            /* 0x294 transmit drop frame counter */
    volatile uint32_t tjbr;            /* 0x298 transmit jabber frame counter */
    volatile uint32_t tfcs;            /* 0x29C transmit FCS error counter */
    volatile uint32_t txcf;            /* 0x2A0 transmit control frame counter */
    volatile uint32_t tovr;            /* 0x2A4 transmit oversize frame counter */
    volatile uint32_t tund;            /* 0x2A8 transmit undersize frame counter */
    volatile uint32_t tfrg;            /* 0x2AC transmit fragments frame counter */

    /* GENERAL REGISTERS */
    volatile uint32_t car1;            /* 0x2B0 carry register one register* */
    volatile uint32_t car2;            /* 0x2B4 carry register two register* */
    volatile uint32_t cam1;            /* 0x2B8 carry register one mask register */
    volatile uint32_t cam2;            /* 0x2BC carry register two mask register */
    volatile uint32_t DTSEC_RESERVED10[16]; /* 0x2C0-0x2FC */

    /* Debug and Factory Test Registers */
    volatile uint32_t debug;            /* 0x300 DEBUG - Debug Register */
    volatile uint32_t dperror;          /* 0x304 DPERROR - Parity Error Register */
    volatile uint32_t hwassert;         /* 0x308 HWASSERT */
    volatile uint32_t RESERVED11;       /* 0x30C Reserved */
    volatile uint32_t rx_fifo_ptr;      /* 0x310 RXFIFOPTR - Rx FIFO R/W Pointer Register */
    volatile uint32_t rx_fifo_dath;     /* 0x314 RXFIFODATH - Rx FIFO Data Register */
    volatile uint32_t rx_fifo_datl;     /* 0x318 RXFIFODATL - Rx FIFO Data Register */
    volatile uint32_t rx_fifo_stat;     /* 0x31C RXFIFOSTAT - Rx FIFO Status Register */
    volatile uint32_t tx_fifo_ptr;      /* 0x320 TXFIFOPTR - Tx FIFO R/W Pointer Register */
    volatile uint32_t tx_fifo_dath;     /* 0x324 TXFIFODATH - Rx FIFO Data Register */
    volatile uint32_t tx_fifo_datl;     /* 0x328 TXFIFODATL - Rx FIFO Data Register */
    volatile uint32_t tx_fifo_stat;     /* 0x32C TXFIFOSTAT - Tx FIFO Status Register */
    volatile uint32_t pkt_rcv_cnt;      /* 0x330 PKTRCVCNT - Number of packets accepted and written to Rx FIFO */
    volatile uint32_t RESERVED12[3];    /* 0x334-0x33C Reserved */
    volatile uint32_t tx_threshold;     /* 0x340 Transmit threshold; Number of entries (4 bytes units) before starting to transmit to the MAC */
    volatile uint32_t tx_watermark_high;/* 0x344 Transmit watermark high; Number of entries (4 byte units) before de-asserting Ready to packet Interface */
    volatile uint32_t rx_watermark_low; /* 0x348 Receive watermark low; Number of entries (4 byte units) before unloading to packet Interface */
} _PackedType t_DtsecMemMap;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


typedef struct {
    uint32_t    errorDisabled;
    bool        halfDuplex;
    uint16_t    pauseTime;
    uint16_t    pauseExtended;
    uint8_t     tbiPhyAddr;         /**< TBI Physical address  (1-31)     [DEFAULT_tbiPhyAddr] */

    bool        autoZeroCounters;
    bool        promiscuousEnable;

    bool        halfDulexFlowControlEn;
    bool        txTimeStampEn;
    bool        rxTimeStampEn;

    uint8_t     packetAlignmentPadding;
    bool        controlFrameAccept;
    bool        groupHashExtend;
    bool        broadcReject;
    bool        rxShortFrame;
    bool        exactMatch;

    bool        debugMode;

    bool        loopback;
    bool        actOnRxPauseFrame;
    bool        actOnTxPauseFrame;

    uint8_t     nonBackToBackIpg1;
    uint8_t     nonBackToBackIpg2;
    uint8_t     minIfgEnforcement;
    uint8_t     backToBackIpg;

    uint8_t     preambleLength;
    bool        preambleRxEn;
    bool        preambleTxEn;
    bool        lengthCheckEnable;
    bool        magicPacketEnable;
    bool        padAndCrcEnable;
    bool        crcEnable;

    bool        alternateBackoffEnable;
    uint8_t     alternateBackoffVal;
    bool        backPressureNoBackoff;
    bool        noBackoff;
    bool        excessDefer;
    uint8_t     maxRetransmission;
    uint16_t    collisionWindow;

    uint16_t    maxFrameLength;

    uint8_t     fifoTxThr;
    uint8_t     fifoTxWatermarkH;
    uint8_t     fifoRxWatermarkL;
} t_DtsecDriverParam;

typedef struct {
    t_FmMacControllerDriver     fmMacControllerDriver;
    t_Handle                    h_App;            /**< Handle to the upper layer application              */
    t_DtsecMemMap               *p_MemMap;        /**< pointer to dTSEC memory mapped registers.          */
    t_MiiAccessMemMap           *p_MiiMemMap;     /**< pointer to dTSEC MII memory mapped registers.          */
    uint64_t                    addr;             /**< MAC address of device;                             */
    e_EnetMode                  enetMode;         /**< Ethernet physical interface  */
    t_FmMacExceptionCallback    *f_Exception;
    int                         mdioIrq;
    t_FmMacExceptionCallback    *f_Event;
    bool                        indAddrRegUsed[DTSEC_NUM_OF_PADDRS]; /**< Whether a particular individual address recognition register is being used */
    uint64_t                    paddr[DTSEC_NUM_OF_PADDRS]; /**< MAC address for particular individual address recognition register */
    uint8_t                     numOfIndAddrInRegs; /**< Number of individual addresses in registers for this station. */
    bool                        debugMode;
    bool                        halfDuplex;
    t_InternalStatistics        internalStatistics;
    t_EthHash                   *p_MulticastAddrHash;      /* pointer to driver's global address hash table  */
    t_EthHash                   *p_UnicastAddrHash;    /* pointer to driver's individual address hash table  */
    uint8_t                     macId;
    uint32_t                    exceptions;
    bool                        ptpTsuEnabled;
    bool                        enTsuErrExeption;
    e_FmMacStatisticsLevel      statisticsLevel;

    t_DtsecDriverParam          *p_DtsecDriverParam;
} t_Dtsec;


t_Error DTSEC_MII_WritePhyReg(t_Handle h_Dtsec, uint8_t phyAddr, uint8_t reg, uint16_t data);
t_Error DTSEC_MII_ReadPhyReg(t_Handle  h_Dtsec, uint8_t phyAddr, uint8_t reg, uint16_t *p_Data);


#endif /* __DTSEC_H */
