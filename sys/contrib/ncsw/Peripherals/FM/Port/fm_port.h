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
 @File          fm_port.h

 @Description   FM Port internal structures and definitions.
*//***************************************************************************/
#ifndef __FM_PORT_H
#define __FM_PORT_H

#include "error_ext.h"
#include "std_ext.h"
#include "fm_port_ext.h"

#include "fm_common.h"


#define __ERR_MODULE__  MODULE_FM_PORT


#define MIN_EXT_BUF_SIZE                                64
#define DATA_ALIGNMENT                                  64
#define MAX_LIODN_OFFSET                                64

/**************************************************************************//**
 @Description       Memory Map defines
*//***************************************************************************/
#define BMI_PORT_REGS_OFFSET                            0
#define QMI_PORT_REGS_OFFSET                            0x400
#define PRS_PORT_REGS_OFFSET                            0x800

/**************************************************************************//**
 @Description       defaults
*//***************************************************************************/
#define DEFAULT_PORT_deqHighPriority                    TRUE
#define DEFAULT_PORT_deqType                            e_FM_PORT_DEQ_TYPE1
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
#define DEFAULT_PORT_deqPrefetchOption                  e_FM_PORT_DEQ_FULL_PREFETCH
#define DEFAULT_PORT_deqPrefetchOption_HC               e_FM_PORT_DEQ_NO_PREFETCH
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */
#define DEFAULT_PORT_deqByteCnt                         2000
#define DEFAULT_PORT_bufferPrefixContent_privDataSize   0
#define DEFAULT_PORT_bufferPrefixContent_passPrsResult  FALSE
#define DEFAULT_PORT_bufferPrefixContent_passTimeStamp  FALSE
#define DEFAULT_PORT_bufferPrefixContent_allOtherPCDInfo    FALSE
#ifdef DEBUG
#define DEFAULT_PORT_bufferPrefixContent_debugInfo      FALSE
#endif /* DEBUG */
#define DEFAULT_PORT_bufferPrefixContent_dataAlign      DATA_ALIGNMENT
#define DEFAULT_PORT_cheksumLastBytesIgnore             0
#define DEFAULT_PORT_cutBytesFromEnd                    4
#define DEFAULT_PORT_txFifoMinFillLevel                 0
#define DEFAULT_PORT_txFifoDeqPipelineDepth_IM          2
#define DEFAULT_PORT_txFifoDeqPipelineDepth_1G          2
#define DEFAULT_PORT_txFifoDeqPipelineDepth_10G         8
#define DEFAULT_PORT_txFifoLowComfLevel                 (5*KILOBYTE)
#define DEFAULT_PORT_rxFifoPriElevationLevel            BMI_MAX_FIFO_SIZE
#define DEFAULT_PORT_rxFifoThreshold                    (BMI_MAX_FIFO_SIZE*3/4)
#define DEFAULT_PORT_frmDiscardOverride                 FALSE
#define DEFAULT_PORT_dmaSwapData                        e_FM_PORT_DMA_NO_SWP
#define DEFAULT_PORT_dmaIntContextCacheAttr             e_FM_PORT_DMA_NO_STASH
#define DEFAULT_PORT_dmaHeaderCacheAttr                 e_FM_PORT_DMA_NO_STASH
#define DEFAULT_PORT_dmaScatterGatherCacheAttr          e_FM_PORT_DMA_NO_STASH
#define DEFAULT_PORT_dmaWriteOptimize                   TRUE
#define DEFAULT_PORT_forwardIntContextReuse             FALSE
#define DEFAULT_PORT_BufMargins_startMargins            32
#define DEFAULT_PORT_BufMargins_endMargins              0
#define DEFAULT_PORT_syncReq                            TRUE
#define DEFAULT_PORT_syncReqForHc                       FALSE
#define DEFAULT_PORT_color                              e_FM_PORT_COLOR_GREEN
#define DEFAULT_PORT_errorsToDiscard                    FM_PORT_FRM_ERR_CLS_DISCARD
#define DEFAULT_dualRateLimitScaleDown                  e_FM_PORT_DUAL_RATE_LIMITER_NONE
#define DEFAULT_rateLimitBurstSizeHighGranularity       FALSE
#define DEFAULT_exception                               IM_EV_BSY

/* Host command port MUST NOT be changed to more than 1 !!! */
#define DEFAULT_PORT_numOfTasks(type)                       \
    (uint32_t)((((type) == e_FM_PORT_TYPE_RX_10G) ||        \
                ((type) == e_FM_PORT_TYPE_TX_10G)) ? 16 :   \
               ((((type) == e_FM_PORT_TYPE_RX) ||           \
                 ((type) == e_FM_PORT_TYPE_TX) ||           \
                 ((type) == e_FM_PORT_TYPE_OH_OFFLINE_PARSING)) ? 3 : 1))

#define DEFAULT_PORT_extraNumOfTasks(type)                  \
    (uint32_t)((((type) == e_FM_PORT_TYPE_RX_10G) ||        \
                ((type) == e_FM_PORT_TYPE_TX_10G)) ? 8 :    \
               ((((type) == e_FM_PORT_TYPE_RX) ||           \
                 ((type) == e_FM_PORT_TYPE_TX) ||           \
                 ((type) == e_FM_PORT_TYPE_OH_OFFLINE_PARSING)) ? 2 : 0))

#define DEFAULT_PORT_numOfOpenDmas(type)                \
    (uint32_t)(((type) == e_FM_PORT_TYPE_TX_10G) ? 8 :  \
               (((type) == e_FM_PORT_TYPE_RX_10G) ? 4 : 1))

#define DEFAULT_PORT_extraNumOfOpenDmas(type)               \
    (uint32_t)((((type) == e_FM_PORT_TYPE_RX_10G) ||        \
                ((type) == e_FM_PORT_TYPE_TX_10G)) ? 8 :    \
               ((((type) == e_FM_PORT_TYPE_RX) ||           \
                 ((type) == e_FM_PORT_TYPE_TX) ||           \
                 ((type) == e_FM_PORT_TYPE_OH_OFFLINE_PARSING)) ? 1 : 0))

#if defined(CONFIG_FMAN_RESOURCE_ALLOCATION_ALGORITHM)
/* Let LLD to set minimum fifosize, otherwise fifosize settings will not work */
#define DEFAULT_PORT_sizeOfFifo(type)                                   \
    (uint32_t)(KILOBYTE) 
#else
#define DEFAULT_PORT_sizeOfFifo(type)                                   \
    (uint32_t)((((type) == e_FM_PORT_TYPE_RX_10G) ||                    \
                ((type) == e_FM_PORT_TYPE_TX_10G)) ? (16*KILOBYTE) :    \
               ((((type) == e_FM_PORT_TYPE_RX) ||                       \
                 ((type) == e_FM_PORT_TYPE_TX) ||                       \
                 ((type) == e_FM_PORT_TYPE_OH_OFFLINE_PARSING)) ? (4*KILOBYTE) : (1536)))
#endif

#define DEFAULT_PORT_extraSizeOfFifo(type)                          \
    (uint32_t)(((type) == e_FM_PORT_TYPE_RX_10G) ? (8*KILOBYTE) :   \
               (((type) == e_FM_PORT_TYPE_RX) ? (4*KILOBYTE) : (0)))

#define DEFAULT_PORT_txBdRingLength                 16
#define DEFAULT_PORT_rxBdRingLength                 128
#define DEFAULT_PORT_ImfwExtStructsMemId            0
#define DEFAULT_PORT_ImfwExtStructsMemAttr          MEMORY_ATTR_CACHEABLE

#define OH_PIPELINE_DEPTH                           2

/**************************************************************************//**
 @Description       Memory Mapped Registers
*//***************************************************************************/

#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

#define FM_PORT_MAX_NUM_OF_EXT_POOLS_ALL_INTEGRATIONS   8
#define FM_PORT_NUM_OF_CONGESTION_GRPS_ALL_INTEGRATIONS 256

typedef _Packed struct
{
    volatile uint32_t   fmbm_rcfg;      /**< Rx Configuration */
    volatile uint32_t   fmbm_rst;       /**< Rx Status */
    volatile uint32_t   fmbm_rda;       /**< Rx DMA attributes*/
    volatile uint32_t   fmbm_rfp;       /**< Rx FIFO Parameters*/
    volatile uint32_t   fmbm_rfed;      /**< Rx Frame End Data*/
    volatile uint32_t   fmbm_ricp;      /**< Rx Internal Context Parameters*/
    volatile uint32_t   fmbm_rim;       /**< Rx Internal Buffer Margins*/
    volatile uint32_t   fmbm_rebm;      /**< Rx External Buffer Margins*/
    volatile uint32_t   fmbm_rfne;      /**< Rx Frame Next Engine*/
    volatile uint32_t   fmbm_rfca;      /**< Rx Frame Command Attributes.*/
    volatile uint32_t   fmbm_rfpne;     /**< Rx Frame Parser Next Engine*/
    volatile uint32_t   fmbm_rpso;      /**< Rx Parse Start Offset*/
    volatile uint32_t   fmbm_rpp;       /**< Rx Policer Profile  */
    volatile uint32_t   fmbm_rccb;      /**< Rx Coarse Classification Base */
    volatile uint32_t   fmbm_reth;      /**< Rx Excessive Threshold */
    volatile uint32_t   reserved1[1];   /**< (0x03C 0x03F) */
    volatile uint32_t   fmbm_rprai[FM_PORT_PRS_RESULT_NUM_OF_WORDS];
                                        /**< Rx Parse Results Array Initialization*/
    volatile uint32_t   fmbm_rfqid;     /**< Rx Frame Queue ID*/
    volatile uint32_t   fmbm_refqid;    /**< Rx Error Frame Queue ID*/
    volatile uint32_t   fmbm_rfsdm;     /**< Rx Frame Status Discard Mask*/
    volatile uint32_t   fmbm_rfsem;     /**< Rx Frame Status Error Mask*/
    volatile uint32_t   fmbm_rfene;     /**< Rx Frame Enqueue Next Engine */
    volatile uint32_t   reserved2[0x23];/**< (0x074 0x0FF)  */
    volatile uint32_t   fmbm_ebmpi[FM_PORT_MAX_NUM_OF_EXT_POOLS_ALL_INTEGRATIONS];
                                        /**< Buffer Manager pool Information-*/
    volatile uint32_t   fmbm_acnt[FM_PORT_MAX_NUM_OF_EXT_POOLS_ALL_INTEGRATIONS];
                                        /**< Allocate Counter-*/
    volatile uint32_t   reserved3[8];
                                        /**< 0x130/0x140 - 0x15F reserved -*/
    volatile uint32_t   fmbm_cgm[FM_PORT_NUM_OF_CONGESTION_GRPS_ALL_INTEGRATIONS/32];
                                        /**< Congestion Group Map*/
    volatile uint32_t   fmbm_mpd;       /**< BM Pool Depletion  */
    volatile uint32_t   reserved4[0x1F];/**< (0x184 0x1FF) */
    volatile uint32_t   fmbm_rstc;      /**< Rx Statistics Counters*/
    volatile uint32_t   fmbm_rfrc;      /**< Rx Frame Counter*/
    volatile uint32_t   fmbm_rfbc;      /**< Rx Bad Frames Counter*/
    volatile uint32_t   fmbm_rlfc;      /**< Rx Large Frames Counter*/
    volatile uint32_t   fmbm_rffc;      /**< Rx Filter Frames Counter*/
    volatile uint32_t   fmbm_rfcd;      /**< Rx Frame Discard Counter*/
    volatile uint32_t   fmbm_rfldec;    /**< Rx Frames List DMA Error Counter*/
    volatile uint32_t   fmbm_rodc;      /**< Rx Out of Buffers Discard Counter-*/
    volatile uint32_t   fmbm_rbdc;      /**< Rx Buffers Deallocate Counter-*/
    volatile uint32_t   reserved5[0x17];/**< (0x224 0x27F) */
    volatile uint32_t   fmbm_rpc;       /**< Rx Performance Counters*/
    volatile uint32_t   fmbm_rpcp;      /**< Rx Performance Count Parameters*/
    volatile uint32_t   fmbm_rccn;      /**< Rx Cycle Counter*/
    volatile uint32_t   fmbm_rtuc;      /**< Rx Tasks Utilization Counter*/
    volatile uint32_t   fmbm_rrquc;     /**< Rx Receive Queue Utilization Counter*/
    volatile uint32_t   fmbm_rduc;      /**< Rx DMA Utilization Counter*/
    volatile uint32_t   fmbm_rfuc;      /**< Rx FIFO Utilization Counter*/
    volatile uint32_t   fmbm_rpac;      /**< Rx Pause Activation Counter*/
    volatile uint32_t   reserved6[0x18];/**< (0x2A0 0x2FF) */
    volatile uint32_t   fmbm_rdbg;      /**< Rx Debug-*/
} _PackedType t_FmPortRxBmiRegs;

typedef _Packed struct
{
    volatile uint32_t   fmbm_tcfg;      /**< Tx Configuration */
    volatile uint32_t   fmbm_tst;       /**< Tx Status */
    volatile uint32_t   fmbm_tda;       /**< Tx DMA attributes */
    volatile uint32_t   fmbm_tfp;       /**< Tx FIFO Parameters */
    volatile uint32_t   fmbm_tfed;      /**< Tx Frame End Data */
    volatile uint32_t   fmbm_ticp;      /**< Tx Internal Context Parameters */
    volatile uint32_t   fmbm_tfne;      /**< Tx Frame Next Engine. */
    volatile uint32_t   fmbm_tfca;      /**< Tx Frame Command attribute. */
    volatile uint32_t   fmbm_tcfqid;    /**< Tx Confirmation Frame Queue ID. */
    volatile uint32_t   fmbm_tfeqid;    /**< Tx Frame Error Queue ID */
    volatile uint32_t   fmbm_tfene;     /**< Tx Frame Enqueue Next Engine */
    volatile uint32_t   fmbm_trlmts;    /**< Tx Rate Limiter Scale */
    volatile uint32_t   fmbm_trlmt;     /**< Tx Rate Limiter */
    volatile uint32_t   reserved0[0x73];/**< (0x038-0x200) */
    volatile uint32_t   fmbm_tstc;      /**< Tx Statistics Counters */
    volatile uint32_t   fmbm_tfrc;      /**< Tx Frame Counter */
    volatile uint32_t   fmbm_tfdc;      /**< Tx Frames Discard Counter */
    volatile uint32_t   fmbm_tfledc;    /**< Tx Frame Length error discard counter */
    volatile uint32_t   fmbm_tfufdc;    /**< Tx Frame unsupported format discard Counter */
    volatile uint32_t   fmbm_tbdc;      /**< Tx Buffers Deallocate Counter */
    volatile uint32_t   reserved1[0x1A];/**< (0x218-0x280) */
    volatile uint32_t   fmbm_tpc;       /**< Tx Performance Counters*/
    volatile uint32_t   fmbm_tpcp;      /**< Tx Performance Count Parameters*/
    volatile uint32_t   fmbm_tccn;      /**< Tx Cycle Counter*/
    volatile uint32_t   fmbm_ttuc;      /**< Tx Tasks Utilization Counter*/
    volatile uint32_t   fmbm_ttcquc;    /**< Tx Transmit Confirm Queue Utilization Counter*/
    volatile uint32_t   fmbm_tduc;      /**< Tx DMA Utilization Counter*/
    volatile uint32_t   fmbm_tfuc;      /**< Tx FIFO Utilization Counter*/
} _PackedType t_FmPortTxBmiRegs;

typedef _Packed struct
{
    volatile uint32_t   fmbm_ocfg;      /**< O/H Configuration  */
    volatile uint32_t   fmbm_ost;       /**< O/H Status */
    volatile uint32_t   fmbm_oda;       /**< O/H DMA attributes  */
    volatile uint32_t   fmbm_oicp;      /**< O/H Internal Context Parameters  */
    volatile uint32_t   fmbm_ofdne;     /**< O/H Frame Dequeue Next Engine  */
    volatile uint32_t   fmbm_ofne;      /**< O/H Frame Next Engine  */
    volatile uint32_t   fmbm_ofca;      /**< O/H Frame Command Attributes.  */
    volatile uint32_t   fmbm_ofpne;     /**< O/H Frame Parser Next Engine  */
    volatile uint32_t   fmbm_opso;      /**< O/H Parse Start Offset  */
    volatile uint32_t   fmbm_opp;       /**< O/H Policer Profile */
    volatile uint32_t   fmbm_occb;      /**< O/H Coarse Classification base */
    volatile uint32_t   fmbm_oim;       /**< O/H Internal margins*/
    volatile uint32_t   reserved0[4];   /**< (0x030 - 0x03F) */
    volatile uint32_t   fmbm_oprai[FM_PORT_PRS_RESULT_NUM_OF_WORDS];
                                        /**< O/H Parse Results Array Initialization  */
    volatile uint32_t   fmbm_ofqid;     /**< O/H Frame Queue ID  */
    volatile uint32_t   fmbm_oefqid;    /**< O/H Error Frame Queue ID  */
    volatile uint32_t   fmbm_ofsdm;     /**< O/H Frame Status Discard Mask  */
    volatile uint32_t   fmbm_ofsem;     /**< O/H Frame Status Error Mask  */
    volatile uint32_t   fmbm_ofene;     /**< O/H Frame Enqueue Next Engine  */
    volatile uint32_t   fmbm_orlmts;    /**< O/H Rate Limiter Scale  */
    volatile uint32_t   fmbm_orlmt;     /**< O/H Rate Limiter  */
    volatile uint32_t   reserved0a[0x21];
                                        /**< 0x07C - 0x0FF Reserved */
    union
    {
        volatile uint32_t   fmbm_oebmpi[FM_PORT_MAX_NUM_OF_OBSERVED_EXT_POOLS];
                                        /**< Buffer Manager Observed Pool Information */
        volatile uint32_t   reserved0b[0x18];
    };                                  /**< 0x100 - 0x15F Reserved */
    volatile uint32_t   fmbm_ocgm;      /**< Observed Congestion Group Map */
    volatile uint32_t   reserved0c[0x7];/**< 0x164 - 0x17F Reserved */
    volatile uint32_t   fmbm_ompd;      /**< Observed BMan Pool Depletion */
    volatile uint32_t   reserved0d[0x1F];
                                        /**< 0x184 - 0x1FF Reserved */
    volatile uint32_t   fmbm_ostc;      /**< O/H Statistics Counters  */
    volatile uint32_t   fmbm_ofrc;      /**< O/H Frame Counter  */
    volatile uint32_t   fmbm_ofdc;      /**< O/H Frames Discard Counter  */
    volatile uint32_t   fmbm_ofledc;    /**< O/H Frames Length Error Discard Counter  */
    volatile uint32_t   fmbm_ofufdc;    /**< O/H Frames Unsupported Format Discard Counter  */
    volatile uint32_t   fmbm_offc;      /**< O/H Filter Frames Counter  */
    volatile uint32_t   fmbm_ofwdc;     /**< - Rx Frames WRED Discard Counter  */
    volatile uint32_t   fmbm_ofldec;    /**< O/H Frames List DMA Error Counter */
    volatile uint32_t   fmbm_obdc;      /**< O/H Buffers Deallocate Counter */
    volatile uint32_t   reserved2[0x17];/**< (0x218 - 0x27F) */
    volatile uint32_t   fmbm_opc;       /**< O/H Performance Counters  */
    volatile uint32_t   fmbm_opcp;      /**< O/H Performance Count Parameters  */
    volatile uint32_t   fmbm_occn;      /**< O/H Cycle Counter  */
    volatile uint32_t   fmbm_otuc;      /**< O/H Tasks Utilization Counter  */
    volatile uint32_t   fmbm_oduc;      /**< O/H DMA Utilization Counter */
    volatile uint32_t   fmbm_ofuc;      /**< O/H FIFO Utilization Counter */
} _PackedType t_FmPortOhBmiRegs;

typedef _Packed union
{
    t_FmPortRxBmiRegs rxPortBmiRegs;
    t_FmPortTxBmiRegs txPortBmiRegs;
    t_FmPortOhBmiRegs ohPortBmiRegs;
} _PackedType u_FmPortBmiRegs;

typedef _Packed struct
{
    volatile uint32_t   reserved1[2];   /**<   0xn024 - 0x02B */
    volatile uint32_t   fmqm_pndn;      /**<   PortID n Dequeue NIA Register */
    volatile uint32_t   fmqm_pndc;      /**<   PortID n Dequeue Config Register */
    volatile uint32_t   fmqm_pndtfc;    /**<   PortID n Dequeue Total Frame Counter */
    volatile uint32_t   fmqm_pndfdc;    /**<   PortID n Dequeue FQID from Default Counter */
    volatile uint32_t   fmqm_pndcc;     /**<   PortID n Dequeue Confirm Counter */
} _PackedType t_FmPortNonRxQmiRegs;

typedef _Packed struct
{
    volatile uint32_t   fmqm_pnc;       /**<   PortID n Configuration Register */
    volatile uint32_t   fmqm_pns;       /**<   PortID n Status Register */
    volatile uint32_t   fmqm_pnts;      /**<   PortID n Task Status Register */
    volatile uint32_t   reserved0[4];   /**<   0xn00C - 0xn01B */
    volatile uint32_t   fmqm_pnen;      /**<   PortID n Enqueue NIA Register */
    volatile uint32_t   fmqm_pnetfc;    /**<   PortID n Enqueue Total Frame Counter */
    t_FmPortNonRxQmiRegs    nonRxQmiRegs;  /**<   Registers for Tx Hc & Op ports */
} _PackedType t_FmPortQmiRegs;

typedef _Packed struct
{
    _Packed struct
    {
        volatile uint32_t   softSeqAttach;  /**<   Soft Sequence Attachment */
        volatile uint32_t   lcv;            /**<   Line-up Enable Confirmation Mask */
    } _PackedType hdrs[FM_PCD_PRS_NUM_OF_HDRS];
    volatile uint8_t    reserved0[0x378];
    volatile uint32_t   pcac;               /**<   Parse Internal Memory Configuration Access Control Register */
    volatile uint32_t   pctpid;             /**<   Parse Internal Memory Configured TPID Register */
} _PackedType t_FmPortPrsRegs;

/**************************************************************************//*
 @Description   Basic buffer descriptor (BD) structure
*//***************************************************************************/
typedef _Packed struct
{
    volatile uint16_t       status;
    volatile uint16_t       length;
    volatile uint8_t        reserved0[0x6];
    volatile uint8_t        reserved1[0x1];
    volatile t_FmPhysAddr   buff;
} _PackedType t_FmImBd;

typedef _Packed struct
{
    volatile uint16_t       gen;                /**< tbd */
    volatile uint8_t        reserved0[0x1];
    volatile t_FmPhysAddr   bdRingBase;         /**< tbd */
    volatile uint16_t       bdRingSize;         /**< tbd */
    volatile uint16_t       offsetIn;           /**< tbd */
    volatile uint16_t       offsetOut;          /**< tbd */
    volatile uint8_t        reserved1[0x12];    /**< 0x0e - 0x1f */
} _PackedType t_FmPortImQd;

typedef _Packed struct
{
    volatile uint32_t   mode;               /**< Mode register */
    volatile uint32_t   rxQdPtr;            /**< tbd */
    volatile uint32_t   txQdPtr;            /**< tbd */
    volatile uint16_t   mrblr;              /**< tbd */
    volatile uint16_t   rxQdBsyCnt;         /**< tbd */
    volatile uint8_t    reserved0[0x10];    /**< 0x10 - 0x1f */
    t_FmPortImQd        rxQd;
    t_FmPortImQd        txQd;
    volatile uint8_t    reserved1[0xa0];    /**< 0x60 - 0xff */
} _PackedType t_FmPortImPram;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


/**************************************************************************//**
 @Description       Registers bit fields
*//***************************************************************************/

/**************************************************************************//**
 @Description       BMI defines
*//***************************************************************************/
#define BMI_PORT_CFG_EN                         0x80000000
#define BMI_PORT_CFG_EN_MACSEC                  0x00800000
#define BMI_PORT_CFG_FDOVR                      0x02000000
#define BMI_PORT_CFG_IM                         0x01000000
#define BMI_PORT_STATUS_BSY                     0x80000000
#define BMI_COUNTERS_EN                         0x80000000
#define BMI_DMA_ATTR_WRITE_OPTIMIZE             0x00100000
#define BMI_PORT_RFNE_FRWD_DCL4C                0x10000000
#define BMI_PORT_RFNE_FRWD_RPD                  0x40000000
#define BMI_RFNE_FDCS_MASK                      0xFF000000

#define BMI_CMD_MR_LEAC                         0x00200000
#define BMI_CMD_MR_SLEAC                        0x00100000
#define BMI_CMD_MR_MA                           0x00080000
#define BMI_CMD_MR_DEAS                         0x00040000
#define BMI_CMD_TX_MR_DEF                       (0)
#define BMI_CMD_RX_MR_DEF                       (BMI_CMD_MR_LEAC | \
                                                 BMI_CMD_MR_SLEAC | \
                                                 BMI_CMD_MR_MA | \
                                                 BMI_CMD_MR_DEAS)
#define BMI_CMD_ATTR_ORDER                      0x80000000
#define BMI_CMD_ATTR_SYNC                       0x02000000
#define BMI_CMD_ATTR_MACCMD_MASK                0x0000ff00
#define BMI_CMD_ATTR_MACCMD_OVERRIDE            0x00008000
#define BMI_CMD_ATTR_MACCMD_SECURED             0x00001000
#define BMI_CMD_ATTR_MACCMD_SC_MASK             0x00000f00

#define BMI_EXT_BUF_POOL_VALID                  0x80000000
#define BMI_EXT_BUF_POOL_EN_COUNTER             0x40000000
#define BMI_EXT_BUF_POOL_BACKUP                 0x20000000
#define BMI_EXT_BUF_POOL_ID_MASK                0x003F0000
#define BMI_STATUS_RX_MASK_UNUSED               (uint32_t)(~(FM_PORT_FRM_ERR_DMA                    | \
                                                             FM_PORT_FRM_ERR_PHYSICAL               | \
                                                             FM_PORT_FRM_ERR_SIZE                   | \
                                                             FM_PORT_FRM_ERR_CLS_DISCARD            | \
                                                             FM_PORT_FRM_ERR_EXTRACTION             | \
                                                             FM_PORT_FRM_ERR_NO_SCHEME              | \
                                                             FM_PORT_FRM_ERR_COLOR_RED              | \
                                                             FM_PORT_FRM_ERR_COLOR_YELLOW           | \
                                                             FM_PORT_FRM_ERR_ILL_PLCR               | \
                                                             FM_PORT_FRM_ERR_PLCR_FRAME_LEN         | \
                                                             FM_PORT_FRM_ERR_PRS_TIMEOUT            | \
                                                             FM_PORT_FRM_ERR_PRS_ILL_INSTRUCT       | \
                                                             FM_PORT_FRM_ERR_BLOCK_LIMIT_EXCEEDED   | \
                                                             FM_PORT_FRM_ERR_PRS_HDR_ERR            | \
                                                             FM_PORT_FRM_ERR_PROCESS_TIMEOUT        | \
                                                             FM_PORT_FRM_ERR_KEYSIZE_OVERFLOW))

#define BMI_STATUS_OP_MASK_UNUSED               (uint32_t)(BMI_STATUS_RX_MASK_UNUSED &                \
                                                           ~(FM_PORT_FRM_ERR_LENGTH                 | \
                                                             FM_PORT_FRM_ERR_UNSUPPORTED_FORMAT))

#define BMI_RATE_LIMIT_EN                       0x80000000
#define BMI_RATE_LIMIT_BURST_SIZE_GRAN          0x80000000
#define BMI_RATE_LIMIT_SCALE_BY_2               0x00000001
#define BMI_RATE_LIMIT_SCALE_BY_4               0x00000002
#define BMI_RATE_LIMIT_SCALE_BY_8               0x00000003

#define BMI_RX_FIFO_THRESHOLD_BC                0x80000000

#define BMI_PRS_RESULT_HIGH                     0x00000000
#define BMI_PRS_RESULT_LOW                      0xFFFFFFFF

#define RX_ERRS_TO_ENQ                          (FM_PORT_FRM_ERR_DMA                    | \
                                                 FM_PORT_FRM_ERR_PHYSICAL               | \
                                                 FM_PORT_FRM_ERR_SIZE                   | \
                                                 FM_PORT_FRM_ERR_EXTRACTION             | \
                                                 FM_PORT_FRM_ERR_NO_SCHEME              | \
                                                 FM_PORT_FRM_ERR_ILL_PLCR               | \
                                                 FM_PORT_FRM_ERR_PLCR_FRAME_LEN         | \
                                                 FM_PORT_FRM_ERR_PRS_TIMEOUT            | \
                                                 FM_PORT_FRM_ERR_PRS_ILL_INSTRUCT       | \
                                                 FM_PORT_FRM_ERR_BLOCK_LIMIT_EXCEEDED   | \
                                                 FM_PORT_FRM_ERR_PRS_HDR_ERR            | \
                                                 FM_PORT_FRM_ERR_PROCESS_TIMEOUT        | \
                                                 FM_PORT_FRM_ERR_KEYSIZE_OVERFLOW)

#ifdef FM_CAPWAP_SUPPORT
#define OP_ERRS_TO_ENQ                          (RX_ERRS_TO_ENQ                         | \
                                                 FM_PORT_FRM_ERR_LENGTH                 | \
                                                 FM_PORT_FRM_ERR_NON_FM                 | \
                                                 FM_PORT_FRM_ERR_UNSUPPORTED_FORMAT)

#else
#define OP_ERRS_TO_ENQ                          (RX_ERRS_TO_ENQ                         | \
                                                 FM_PORT_FRM_ERR_LENGTH                 | \
                                                 FM_PORT_FRM_ERR_UNSUPPORTED_FORMAT)
#endif /* FM_CAPWAP_SUPPORT */

/* shifts */
#define BMI_PORT_CFG_MS_SEL_SHIFT               16
#define BMI_DMA_ATTR_SWP_SHIFT                  30
#define BMI_DMA_ATTR_IC_CACHE_SHIFT             28
#define BMI_DMA_ATTR_HDR_CACHE_SHIFT            26
#define BMI_DMA_ATTR_SG_CACHE_SHIFT             24

#define BMI_IM_FOF_SHIFT                        28
#define BMI_PR_PORTID_SHIFT                     24

#define BMI_RX_FIFO_PRI_ELEVATION_SHIFT         16
#define BMI_RX_FIFO_THRESHOLD_SHIFT             0

#define BMI_RX_FRAME_END_CS_IGNORE_SHIFT        24
#define BMI_RX_FRAME_END_CUT_SHIFT              16

#define BMI_IC_TO_EXT_SHIFT                     16
#define BMI_IC_FROM_INT_SHIFT                   8
#define BMI_IC_SIZE_SHIFT                       0

#define BMI_INT_BUF_MARG_SHIFT                  28

#define BMI_EXT_BUF_MARG_START_SHIFT            16
#define BMI_EXT_BUF_MARG_END_SHIFT              0

#define BMI_CMD_ATTR_COLOR_SHIFT                26
#define BMI_CMD_ATTR_COM_MODE_SHIFT             16
#define BMI_CMD_ATTR_MACCMD_SHIFT               8
#define BMI_CMD_ATTR_MACCMD_OVERRIDE_SHIFT      15
#define BMI_CMD_ATTR_MACCMD_SECURED_SHIFT       12
#define BMI_CMD_ATTR_MACCMD_SC_SHIFT            8

#define BMI_POOL_DEP_NUM_OF_POOLS_SHIFT         16
#define BMI_POOL_DEP_NUM_OF_POOLS_VECTOR_SHIFT  24

#define BMI_EXT_BUF_POOL_ID_SHIFT               16

#define BMI_TX_FIFO_MIN_FILL_SHIFT              16
#define BMI_TX_FIFO_PIPELINE_DEPTH_SHIFT        12
#define BMI_TX_LOW_COMF_SHIFT                   0

#define BMI_TX_FRAME_END_CS_IGNORE_SHIFT        24

#define BMI_PERFORMANCE_TASK_COMP_SHIFT         24
#define BMI_PERFORMANCE_PORT_COMP_SHIFT         16
#define BMI_PERFORMANCE_DMA_COMP_SHIFT          12
#define BMI_PERFORMANCE_FIFO_COMP_SHIFT         0

#define BMI_MAX_BURST_SHIFT                     16
#define BMI_COUNT_RATE_UNIT_SHIFT               16

/* sizes */
#define FRAME_END_DATA_SIZE                     16
#define OFFSET_UNITS                            16
#define FRAME_OFFSET_UNITS                      16
#define MAX_EXT_OFFSET                          496
#define MAX_EXT_BUFFER_OFFSET                   511
#define MAX_INT_OFFSET                          240
#define MIN_TX_INT_OFFSET                       16
#define MAX_IC_SIZE                             256
#define MAX_FRAME_OFFSET                        64
#define MAX_FIFO_PIPELINE_DEPTH                 8
#define MAX_PERFORMANCE_TASK_COMP               64
#define MAX_PERFORMANCE_TX_QUEUE_COMP           8
#define MAX_PERFORMANCE_RX_QUEUE_COMP           64
#define MAX_PERFORMANCE_DMA_COMP                16
#define MAX_NUM_OF_TASKS                        64
#define MAX_NUM_OF_EXTRA_TASKS                  8
#define MAX_NUM_OF_DMAS                         16
#define MAX_NUM_OF_EXTRA_DMAS                   8
#define MAX_BURST_SIZE                          1024
#define FRAG_EXTRA_SPACE                        32

/**************************************************************************//**
 @Description       QMI defines
*//***************************************************************************/
/* masks */
#define QMI_PORT_CFG_EN                         0x80000000
#define QMI_PORT_CFG_EN_COUNTERS                0x10000000
#define QMI_PORT_STATUS_DEQ_TNUM_BSY            0x80000000
#define QMI_PORT_STATUS_DEQ_FD_BSY              0x20000000

#define QMI_DEQ_CFG_PREFETCH_NO_TNUM            0x02000000
#define QMI_DEQ_CFG_PREFETCH_WAITING_TNUM       0
#define QMI_DEQ_CFG_PREFETCH_1_FRAME            0
#define QMI_DEQ_CFG_PREFETCH_3_FRAMES           0x01000000

#define QMI_DEQ_CFG_PRI                         0x80000000
#define QMI_DEQ_CFG_TYPE1                       0x10000000
#define QMI_DEQ_CFG_TYPE2                       0x20000000
#define QMI_DEQ_CFG_TYPE3                       0x30000000

#define QMI_DEQ_CFG_SUBPORTAL_MASK              0x1f
#define QMI_DEQ_CFG_SUBPORTAL_SHIFT             20

/**************************************************************************//**
 @Description       PARSER defines
*//***************************************************************************/
/* masks */
#define PRS_HDR_ERROR_DIS                       0x00000800
#define PRS_HDR_SW_PRS_EN                       0x00000400
#define PRS_CP_OFFSET_MASK                      0x0000000F
#define PRS_TPID1_MASK                          0xFFFF0000
#define PRS_TPID2_MASK                          0x0000FFFF
#define PRS_TPID_DFLT                           0x91009100

#define PRS_HDR_MPLS_LBL_INTER_EN               0x00200000
#define PRS_HDR_IPV6_ROUTE_HDR_DIS              0x00008000
#define PRS_HDR_PPPOE_MTU_CHECK_EN              0x80000000
#define PRS_HDR_UDP_PAD_REMOVAL                 0x80000000
#define PRS_HDR_TCP_PAD_REMOVAL                 0x80000000
#define PRS_CAC_STOP                            0x00000001
#define PRS_CAC_ACTIVE                          0x00000100

/* shifts */
#define PRS_PCTPID_SHIFT                        16
#define PRS_HDR_MPLS_NEXT_HDR_SHIFT             22
#define PRS_HDR_ETH_BC_SHIFT                    28
#define PRS_HDR_ETH_MC_SHIFT                    24
#define PRS_HDR_VLAN_STACKED_SHIFT              16
#define PRS_HDR_MPLS_STACKED_SHIFT              16
#define PRS_HDR_IPV4_1_BC_SHIFT                 28
#define PRS_HDR_IPV4_1_MC_SHIFT                 24
#define PRS_HDR_IPV4_2_UC_SHIFT                 20
#define PRS_HDR_IPV4_2_MC_BC_SHIFT              16
#define PRS_HDR_IPV6_1_MC_SHIFT                 24
#define PRS_HDR_IPV6_2_UC_SHIFT                 20
#define PRS_HDR_IPV6_2_MC_SHIFT                 16

#define PRS_HDR_ETH_BC_MASK                     0x0fffffff
#define PRS_HDR_ETH_MC_MASK                     0xf0ffffff
#define PRS_HDR_VLAN_STACKED_MASK               0xfff0ffff
#define PRS_HDR_MPLS_STACKED_MASK               0xfff0ffff
#define PRS_HDR_IPV4_1_BC_MASK                  0x0fffffff
#define PRS_HDR_IPV4_1_MC_MASK                  0xf0ffffff
#define PRS_HDR_IPV4_2_UC_MASK                  0xff0fffff
#define PRS_HDR_IPV4_2_MC_BC_MASK               0xfff0ffff
#define PRS_HDR_IPV6_1_MC_MASK                  0xf0ffffff
#define PRS_HDR_IPV6_2_UC_MASK                  0xff0fffff
#define PRS_HDR_IPV6_2_MC_MASK                  0xfff0ffff

/* others */
#define PRS_HDR_ENTRY_SIZE                      8
#define DEFAULT_CLS_PLAN_VECTOR                 0xFFFFFFFF

#define IPSEC_SW_PATCH_START                    0x20
#define SCTP_SW_PATCH_START                     0x4D
#define DCCP_SW_PATCH_START                     0x41

#define IP_FRAG_SW_PATCH_IPv4                   0x300
#define IP_FRAG_SW_PATCH_IPv6_0                 0x320
#define IP_FRAG_SW_PATCH_IPv6_1                 0x372

/**************************************************************************//**
 @Description       IM defines
*//***************************************************************************/
#define BD_R_E                                  0x80000000
#define BD_L                                    0x08000000

#define BD_RX_CRE                               0x00080000
#define BD_RX_FTL                               0x00040000
#define BD_RX_FTS                               0x00020000
#define BD_RX_OV                                0x00010000

#define BD_RX_ERRORS                            (BD_RX_CRE | BD_RX_FTL | BD_RX_FTS | BD_RX_OV)
#define BD_ERROR_PASS_FRAME                     BD_RX_ERRORS

#define FM_IM_SIZEOF_BD                         sizeof(t_FmImBd)

#define BD_STATUS_MASK                          0xffff0000
#define BD_LENGTH_MASK                          0x0000ffff

#define BD_STATUS_AND_LENGTH_SET(bd, val)       WRITE_UINT32(*(volatile uint32_t*)(bd), (val))

#define BD_STATUS_AND_LENGTH(bd)                GET_UINT32(*(volatile uint32_t*)(bd))

#define BD_GET(id)                              &p_FmPort->im.p_BdRing[id]

#define IM_ILEGAL_BD_ID                         0xffff

/* others */
#define IM_PRAM_ALIGN                           0x100

/* masks */
#define IM_MODE_GBL                             0x20000000
#define IM_MODE_BO_MASK                         0x18000000
#define IM_MODE_BO_SHIFT                        3
#define IM_MODE_GRC_STP                         0x00800000

#define IM_MODE_SET_BO(val)                     (uint32_t)((val << (31-IM_MODE_BO_SHIFT)) & IM_MODE_BO_MASK)

#define IM_RXQD_BSYINTM                         0x0008
#define IM_RXQD_RXFINTM                         0x0010
#define IM_RXQD_FPMEVT_SEL_MASK                 0x0003

#define IM_EV_BSY                               0x40000000
#define IM_EV_RX                                0x80000000

typedef struct {
    t_Handle                    h_FmMuram;
    t_FmPortImPram              *p_FmPortImPram;
    uint8_t                     fwExtStructsMemId;
    uint32_t                    fwExtStructsMemAttr;
    uint16_t                    bdRingSize;
    t_FmImBd                    *p_BdRing;
    t_Handle                    *p_BdShadow;
    uint16_t                    currBdId;
    uint16_t                    firstBdOfFrameId;

    /* Rx port parameters */
    uint8_t                     dataMemId;          /**< Memory partition ID for data buffers */
    uint32_t                    dataMemAttributes;  /**< Memory attributes for data buffers */
    t_BufferPoolInfo            rxPool;
    uint16_t                    mrblr;
    uint16_t                    rxFrameAccumLength;
    t_FmPortImRxStoreCallback   *f_RxStore;

    /* Tx port parameters */
    uint32_t                    txFirstBdStatus;
    t_FmPortImTxConfCallback    *f_TxConf;
} t_FmMacIm;

/**************************************************************************//**
 @Description   structure for defining internal context copying
*//***************************************************************************/
typedef struct
{
    uint16_t    extBufOffset;       /**< Offset in External buffer to which internal
                                         context is copied to (Rx) or taken from (Tx, Op). */
    uint8_t     intContextOffset;   /**< Offset within internal context to copy from
                                         (Rx) or to copy to (Tx, Op). */
    uint16_t    size;               /**< Internal offset size to be copied */
} t_FmPortIntContextDataCopy;

/**************************************************************************//**
 @Description   struct for defining external buffer margins
*//***************************************************************************/
typedef struct {
    uint16_t    startMargins;           /**< Number of bytes to be left at the beginning
                                             of the external buffer (must be divisible by 16) */
    uint16_t    endMargins;             /**< number of bytes to be left at the end
                                             of the external buffer(must be divisible by 16) */
} t_FmPortBufMargins;

typedef struct {
    uint32_t      dataOffset;
    uint32_t      prsResultOffset;
    uint32_t      timeStampOffset;
    uint32_t      hashResultOffset;
    uint32_t      pcdInfoOffset;
    uint32_t      manipOffset;
#ifdef DEBUG
    uint32_t      debugOffset;
#endif /* DEBUG */
} t_FmPortBufferOffsets;

typedef struct {
    uint32_t                            dfltFqid;
    uint32_t                            confFqid;
    uint32_t                            errFqid;
    uintptr_t                           baseAddr;
    uint8_t                             deqSubPortal;
    bool                                deqHighPriority;
    e_FmPortDeqType                     deqType;
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    e_FmPortDeqPrefetchOption           deqPrefetchOption;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */
    uint16_t                            deqByteCnt;
    uint8_t                             cheksumLastBytesIgnore;
    uint8_t                             cutBytesFromEnd;
    t_FmPortBufPoolDepletion            bufPoolDepletion;
    uint8_t                             pipelineDepth;
    uint16_t                            fifoLowComfLevel;
    bool                                frmDiscardOverride;
    bool                                enRateLimit;
    t_FmPortRateLimit                   rateLimit;
    e_FmPortDualRateLimiterScaleDown    rateLimitDivider;
    bool                                enBufPoolDepletion;
    uint16_t                            liodnOffset;
    uint16_t                            liodnBase;
    t_FmPortExtPools                    extBufPools;
    e_FmPortDmaSwap                     dmaSwapData;
    e_FmPortDmaCache                    dmaIntContextCacheAttr;
    e_FmPortDmaCache                    dmaHeaderCacheAttr;
    e_FmPortDmaCache                    dmaScatterGatherCacheAttr;
    bool                                dmaReadOptimize;
    bool                                dmaWriteOptimize;
    uint32_t                            txFifoMinFillLevel;
    uint32_t                            txFifoLowComfLevel;
    uint32_t                            rxFifoPriElevationLevel;
    uint32_t                            rxFifoThreshold;
    t_FmPortBufMargins                  bufMargins;
    t_FmPortIntContextDataCopy          intContext;
    bool                                syncReq;
    e_FmPortColor                       color;
    fmPortFrameErrSelect_t              errorsToDiscard;
    fmPortFrameErrSelect_t              errorsToEnq;
    uint64_t                            fmMuramPhysBaseAddr;
    bool                                forwardReuseIntContext;
    t_FmPortBufferPrefixContent         bufferPrefixContent;
    uint8_t                             internalBufferOffset;
    t_FmPortBackupBmPools               *p_BackupBmPools;
    bool                                dontReleaseBuf;
} t_FmPortDriverParam;

typedef struct {
    t_Handle                    h_Fm;
    t_Handle                    h_FmPcd;
    uint8_t                     portId;
    e_FmPortType                portType;
    int                         enabled;
    char                        name[MODULE_NAME_SIZE];
    uint8_t                     hardwarePortId;
    uint16_t                    fmClkFreq;
    t_FmPortQmiRegs             *p_FmPortQmiRegs;
    u_FmPortBmiRegs             *p_FmPortBmiRegs;
    t_FmPortPrsRegs             *p_FmPortPrsRegs;
    fmPcdEngines_t              pcdEngines;
    uint32_t                    savedBmiNia;
    uint8_t                     netEnvId;
    uint32_t                    optArray[FM_PCD_MAX_NUM_OF_OPTIONS(FM_PCD_MAX_NUM_OF_CLS_PLANS)];
    uint32_t                    lcvs[FM_PCD_PRS_NUM_OF_HDRS];
    uint8_t                     privateInfo;
    uint32_t                    schemesPerPortVector;
    bool                        useClsPlan;
    uint8_t                     clsPlanGrpId;
    t_Handle                    ccTreeId;
    t_Handle                    completeArg;
    void                        (*f_Complete)(t_Handle arg);
    t_FmPortBufferOffsets       bufferOffsets;
    /* Independent-Mode parameters support */
    bool                        imEn;
    t_FmMacIm                   im;
    uint8_t                     txFifoDeqPipelineDepth;
    volatile bool               lock;
    t_Handle                    h_Spinlock;
    t_FmPortExceptionCallback   *f_Exception;
    t_Handle                    h_App;
    uint8_t                     internalBufferOffset;
    uint8_t                     fmanCtrlEventId;
    uint32_t                    exceptions;
    bool                        polling;
    uint8_t                     numOfTasks;
    t_FmPortExtPools            extBufPools;
    uint32_t                    requiredAction;
    uint32_t                    savedQmiPnen;
    uint32_t                    savedNonRxQmiRegsPndn;
    int                         savedPrsStartOffset;
    t_FmPortRsrc                openDmas;
    t_FmPortRsrc                tasks;
    t_FmPortRsrc                fifoBufs;
    t_FmInterModulePortRxPoolsParams rxPoolsParams;
    t_FmPortDriverParam         *p_FmPortDriverParam;
} t_FmPort;

#define CHECK_FM_CTL_AC_POST_FETCH_PCD(savedBmiNia) \
    ((((savedBmiNia) & NIA_ENG_MASK) == NIA_ENG_FM_CTL) && \
     ((((savedBmiNia) & NIA_FM_CTL_AC_MASK) == NIA_FM_CTL_AC_POST_FETCH_PCD) || \
      (((savedBmiNia) & NIA_FM_CTL_AC_MASK) == NIA_FM_CTL_AC_POST_FETCH_PCD_UDP_LEN)))

void FmPortConfigIM (t_FmPort *p_FmPort, t_FmPortParams *p_FmPortParams);
t_Error FmPortImCheckInitParameters(t_FmPort *p_FmPort);

t_Error FmPortImInit(t_FmPort *p_FmPort);
void    FmPortImFree(t_FmPort *p_FmPort);

t_Error FmPortImEnable  (t_FmPort *p_FmPort);
t_Error FmPortImDisable (t_FmPort *p_FmPort);
t_Error FmPortImRx      (t_FmPort *p_FmPort);

void    FmPortSetMacsecLcv(t_Handle h_FmPort);
void    FmPortSetMacsecCmd(t_Handle h_FmPort, uint8_t dfltSci);


static __inline__ uint8_t * BdBufferGet (t_PhysToVirt *f_PhysToVirt, t_FmImBd *p_Bd)
{
    uint64_t    physAddr = (uint64_t)((uint64_t)GET_UINT8(p_Bd->buff.high) << 32);
    physAddr |= GET_UINT32(p_Bd->buff.low);

    return (uint8_t *)f_PhysToVirt((physAddress_t)(physAddr));
}

static __inline__ void SET_ADDR(volatile t_FmPhysAddr *fmPhysAddr, uint64_t value)
{
    WRITE_UINT8(fmPhysAddr->high,(uint8_t)((value & 0x000000ff00000000LL) >> 32));
    WRITE_UINT32(fmPhysAddr->low,(uint32_t)value);
}

static __inline__ void BdBufferSet(t_VirtToPhys *f_VirtToPhys, t_FmImBd *p_Bd, uint8_t *p_Buffer)
{
    uint64_t    physAddr = (uint64_t)(f_VirtToPhys(p_Buffer));
    SET_ADDR(&p_Bd->buff, physAddr);
}

static __inline__ uint16_t GetNextBdId(t_FmPort *p_FmPort, uint16_t id)
{
    if (id < p_FmPort->im.bdRingSize-1)
        return (uint16_t)(id+1);
    else
        return 0;
}


#endif /* __FM_PORT_H */
