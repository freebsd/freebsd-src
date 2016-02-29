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
 @File          fm_manip.h

 @Description   FM PCD manip...
*//***************************************************************************/
#ifndef __FM_MANIP_H
#define __FM_MANIP_H

#include "std_ext.h"
#include "error_ext.h"
#include "list_ext.h"

#include "fm_cc.h"


/***********************************************************************/
/*          Header manipulations defines                              */
/***********************************************************************/

#define HMAN_OC_RMV_N_OR_INSRT_INT_FRM_HDR                      0x2e
#define HMAN_OC_INSRT_HDR_BY_TEMPL_N_OR_FRAG_AFTER              0x31
#define HMAN_OC_CAPWAP_FRAGMENTATION                            0x33
#define HMAN_OC_IPSEC                                           0x34
#define HMAN_OC_IP_FRAGMENTATION                                0x74
#define HMAN_OC_IP_REASSEMBLY                                   0xB4
#define HMAN_OC_MV_INT_FRAME_HDR_FROM_FRM_TO_BUFFER_PREFFIX     0x2f
#define HMAN_OC_CAPWAP_RMV_DTLS_IF_EXIST                        0x30
#define HMAN_OC_CAPWAP_REASSEMBLY                               0x11 /* dummy */
#define HMAN_OC_CAPWAP_INDEXED_STATS                            0x32 /* dummy */

#define HMAN_RMV_HDR                               0x80000000
#define HMAN_INSRT_INT_FRM_HDR                     0x40000000

#define UDP_UDPHECKSUM_FIELD_OFFSET_FROM_UDP        6
#define UDP_UDPCHECKSUM_FIELD_SIZE                  2

#define IP_DSCECN_FIELD_OFFSET_FROM_IP              1
#define IP_TOTALLENGTH_FIELD_OFFSET_FROM_IP         2
#define IP_HDRCHECKSUM_FIELD_OFFSET_FROM_IP         10
#define VLAN_TAG_FIELD_OFFSET_FROM_ETH              12
#define IP_ID_FIELD_OFFSET_FROM_IP                  4

#define FM_PCD_MANIP_CAPWAP_REASM_TABLE_SIZE               80
#define FM_PCD_MANIP_CAPWAP_REASM_TABLE_ALIGN              8
#define FM_PCD_MANIP_CAPWAP_REASM_RFD_SIZE                 32
#define FM_PCD_MANIP_CAPWAP_REASM_AUTO_LEARNING_HASH_ENTRY_SIZE 4
#define FM_PCD_MANIP_CAPWAP_REASM_TIME_OUT_ENTRY_SIZE      8


#define FM_PCD_MANIP_CAPWAP_REASM_TIME_OUT_BETWEEN_FRAMES          0x40000000
#define FM_PCD_MANIP_CAPWAP_REASM_HALT_ON_DUPLICATE_FRAG           0x10000000
#define FM_PCD_MANIP_CAPWAP_REASM_AUTOMATIC_LEARNIN_HASH_8_WAYS    0x08000000
#define FM_PCD_MANIP_CAPWAP_REASM_PR_COPY                          0x00800000

#define FM_PCD_MANIP_CAPWAP_FRAG_COMPR_OPTION_FIELD_EN             0x80000000

#define FM_PCD_MANIP_INDEXED_STATS_ENTRY_SIZE               4
#define FM_PCD_MANIP_INDEXED_STATS_CNIA                     0x20000000
#define FM_PCD_MANIP_INDEXED_STATS_DPD                      0x10000000

#define FM_PCD_MANIP_IPSEC_CALC_UDP_LENGTH                  0x01000000
#define FM_PCD_MANIP_IPSEC_CNIA                             0x20000000

#define e_FM_MANIP_CAPWAP_INDX                              0

#ifdef UNDER_CONSTRUCTION_FRAG_REASSEMBLY
#define FM_PCD_MANIP_IP_REASM_TABLE_SIZE                    0x40
#define FM_PCD_MANIP_IP_REASM_TABLE_ALIGN                   8

#define FM_PCD_MANIP_IP_REASM_COMMON_PARAM_TABLE_SIZE       64
#define FM_PCD_MANIP_IP_REASM_COMMON_PARAM_TABLE_ALIGN      8
#define FM_PCD_MANIP_IP_REASM_TIME_OUT_BETWEEN_FRAMES              0x80000000
#define e_FM_MANIP_IP_INDX                                  1
#define FM_PCD_MANIP_IP_REASM_LIODN_MASK                    0x000003F0
#define FM_PCD_MANIP_IP_REASM_LIODN_SHIFT                   56
#define FM_PCD_MANIP_IP_REASM_ELIODN_MASK                   0x0000000F
#define FM_PCD_MANIP_IP_REASM_ELIODN_SHIFT                  44

#endif /* UNDER_CONSTRUCTION_FRAG_REASSEMBLY */


/***********************************************************************/
/*          Memory map                                                 */
/***********************************************************************/
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */

typedef _Packed struct {
    volatile uint32_t mode;
    volatile uint32_t autoLearnHashTblPtr;
    volatile uint32_t intStatsTblPtr;
    volatile uint32_t reasmFrmDescPoolTblPtr;
    volatile uint32_t reasmFrmDescIndexPoolTblPtr;
    volatile uint32_t timeOutTblPtr;
    volatile uint32_t bufferPoolIdAndRisc1SetIndexes;
    volatile uint32_t risc23SetIndexes;
    volatile uint32_t risc4SetIndexesAndExtendedStatsTblPtr;
    volatile uint32_t extendedStatsTblPtr;
    volatile uint32_t expirationDelay;
    volatile uint32_t totalProcessedFragCounter;
    volatile uint32_t totalUnsuccessfulReasmFramesCounter;
    volatile uint32_t totalDuplicatedFragCounter;
    volatile uint32_t totalMalformdFragCounter;
    volatile uint32_t totalTimeOutCounter;
    volatile uint32_t totalSetBusyCounter;
    volatile uint32_t totalRfdPoolBusyCounter;
    volatile uint32_t totalDiscardedFragsCounter;
    volatile uint32_t totalMoreThan16FramesCounter;
    volatile uint32_t internalBufferBusy;
    volatile uint32_t externalBufferBusy;
    volatile uint8_t res[16];
} _PackedType t_CapwapReasmPram;

#ifdef UNDER_CONSTRUCTION_FRAG_REASSEMBLY
typedef _Packed struct t_IpReasmPram{
    volatile uint16_t waysNumAndSetSize;
    volatile uint16_t autoLearnHashKeyMask;
    volatile uint32_t ipReassCommonPrmTblPtr;
    volatile uint32_t liodnAlAndAutoLearnHashTblPtrHi;
    volatile uint32_t autoLearnHashTblPtrLow;
    volatile uint32_t liodnSlAndAutoLearnSetLockTblPtrHi;
    volatile uint32_t autoLearnSetLockTblPtrLow;
    volatile uint16_t minFragSize;
    volatile uint16_t reserved1;
    volatile uint32_t totalSuccessfullyReasmFramesCounter;
    volatile uint32_t totalValidFragmentCounter;
    volatile uint32_t totalProcessedFragCounter;
    volatile uint32_t totalMalformdFragCounter;
    volatile uint32_t totalSetBusyCounter;
    volatile uint32_t totalDiscardedFragsCounter;
    volatile uint32_t totalMoreThan16FramesCounter;
    volatile uint32_t reserved2[2];
} _PackedType t_IpReasmPram;

typedef _Packed struct t_IpReasmCommonTbl{
    volatile uint32_t timeoutModeAndFqid;
    volatile uint32_t reassFrmDescIndexPoolTblPtr;
    volatile uint32_t liodnAndReassFrmDescPoolPtrHi;
    volatile uint32_t reassFrmDescPoolPtrLow;
    volatile uint32_t timeOutTblPtr;
    volatile uint32_t expirationDelay;
    volatile uint32_t reseervd1;
    volatile uint32_t reseervd2;
    volatile uint32_t totalTimeOutCounter;
    volatile uint32_t totalRfdPoolBusyCounter;
    volatile uint32_t totalInternalBufferBusy;
    volatile uint32_t totalExternalBufferBusy;
    volatile uint32_t reserved3[4];
} _PackedType t_IpReasmCommonTbl;

#endif /*UNDER_CONSTRUCTION_FRAG_REASSEMBLY*/

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


/***********************************************************************/
/*  Driver's internal structures                                       */
/***********************************************************************/

typedef struct
{
    t_Handle p_AutoLearnHashTbl;
    t_Handle p_ReassmFrmDescrPoolTbl;
    t_Handle p_ReassmFrmDescrIndxPoolTbl;
    t_Handle p_TimeOutTbl;
    uint8_t  maxNumFramesInProcess;
    uint8_t  numOfTasks;
    uint8_t  poolId;
    uint8_t  prOffset;
    uint16_t dataOffset;
    uint8_t  poolIndx;
    uint8_t  hwPortId;
    uint32_t fqidForTimeOutFrames;
    uint32_t timeoutRoutineRequestTime;
    uint32_t bitFor1Micro;
} t_FragParams;

#ifdef UNDER_CONSTRUCTION_FRAG_REASSEMBLY
typedef struct
{
    t_Handle h_Frag;
    t_Handle h_FragId;
    uint8_t  poolId;
    uint16_t dataOffset;
    uint8_t  poolIndx;
}t_IpFragParams;

typedef struct t_IpReassmParams
{
    t_Handle            h_Ipv4Ad;
    t_Handle            h_Ipv6Ad;
    e_NetHeaderType     hdr;                /**< Header selection */
    uint32_t            fqidForTimeOutFrames;
    uint16_t            dataOffset;
    t_Handle            h_IpReassCommonParamsTbl;
    t_Handle            h_Ipv4ReassParamsTblPtr;
    t_Handle            h_Ipv6ReassParamsTblPtr;
    t_Handle            h_Ipv4AutoLearnHashTbl;
    t_Handle            h_Ipv6AutoLearnHashTbl;
    t_Handle            h_Ipv4AutoLearnSetLockTblPtr;
    t_Handle            h_Ipv6AutoLearnSetLockTblPtr;
    t_Handle            h_ReassmFrmDescrIndxPoolTbl;
    t_Handle            h_ReassmFrmDescrPoolTbl;
    t_Handle            h_TimeOutTbl;
    uint32_t            maxNumFramesInProcess;
    uint32_t            liodnOffset;
    uint32_t            minFragSize;
    uint8_t             dataMemId;              /**< Memory partition ID for data buffers */
    uint32_t            bpid;
    e_FmPcdManipReassemTimeOutMode  timeOutMode;
    e_FmPcdManipReassemWaysNumber   numOfFramesPerHashEntry;
    uint32_t                        timeoutThresholdForReassmProcess;

}t_IpReassmParams;

typedef struct t_IpCommonReassmParams
{
    uint8_t             numOfTasks;
    uint32_t            bitFor1Micro;
    t_Handle            h_ReassmFrmDescrPoolTbl;
    t_Handle            h_ReassmFrmDescrIndxPoolTbl;
    t_Handle            h_TimeOutTbl;
}t_IpCommonReassmParams;

#endif /*UNDER_CONSTRUCTION_FRAG_REASSEMBLY*/

typedef struct{
    bool                muramAllocate;
    t_Handle            h_Ad;
    uint32_t            type;
    bool                rmv;
    bool                insrt;
    uint8_t             *p_Template;
    t_Handle            h_Frag;
    bool                frag;
    bool                reassm;
    uint16_t            sizeForFragmentation;
    uint8_t             owner;
    uint32_t            updateParams;
    uint32_t            shadowUpdateParams;
    t_FragParams        fragParams;
#ifdef UNDER_CONSTRUCTION_FRAG_REASSEMBLY
    t_IpReassmParams    ipReassmParams;
    t_IpFragParams      ipFragParams;
#endif /* UNDER_CONSTRUCTION_FRAG_REASSEMBLY */
    uint8_t             icOffset;
    uint16_t            ownerTmp;
    bool                cnia;
    t_Handle            p_StatsTbl;
    t_Handle            h_FmPcd;
} t_FmPcdManip;

typedef struct t_FmPcdCcSavedManipParams
{
    union
    {
        struct
        {
            uint16_t    dataOffset;
            uint8_t     poolId;
        }capwapParams;
#ifdef UNDER_CONSTRUCTION_FRAG_REASSEMBLY
        struct
        {
            uint16_t    dataOffset;
            uint8_t     poolId;
        }ipParams;
#endif /*UNDER_CONSTRUCTION_FRAG_REASSEMBLY*/
    };

} t_FmPcdCcSavedManipParams;


#endif /* __FM_MANIP_H */
