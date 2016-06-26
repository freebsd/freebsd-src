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
 @File          fm_cc.h

 @Description   FM PCD CC ...
*//***************************************************************************/
#ifndef __FM_CC_H
#define __FM_CC_H

#include "std_ext.h"
#include "error_ext.h"
#include "list_ext.h"

#include "fm_pcd.h"


/***********************************************************************/
/*          Coarse classification defines                              */
/***********************************************************************/

#define CC_PC_FF_MACDST                     0x00
#define CC_PC_FF_MACSRC                     0x01
#define CC_PC_FF_ETYPE                      0x02

#define CC_PC_FF_TCI1                       0x03
#define CC_PC_FF_TCI2                       0x04

#define CC_PC_FF_MPLS1                      0x06
#define CC_PC_FF_MPLS_LAST                  0x07

#define CC_PC_FF_IPV4DST1                   0x08
#define CC_PC_FF_IPV4DST2                   0x16
#define CC_PC_FF_IPV4IPTOS_TC1              0x09
#define CC_PC_FF_IPV4IPTOS_TC2              0x17
#define CC_PC_FF_IPV4PTYPE1                 0x0A
#define CC_PC_FF_IPV4PTYPE2                 0x18
#define CC_PC_FF_IPV4SRC1                   0x0b
#define CC_PC_FF_IPV4SRC2                   0x19
#define CC_PC_FF_IPV4SRC1_IPV4DST1          0x0c
#define CC_PC_FF_IPV4SRC2_IPV4DST2          0x1a
#define CC_PC_FF_IPV4TTL                    0x29


#define CC_PC_FF_IPTOS_IPV6TC1_IPV6FLOW1    0x0d /*TODO - CLASS - what is it? TOS*/
#define CC_PC_FF_IPTOS_IPV6TC2_IPV6FLOW2    0x1b
#define CC_PC_FF_IPV6PTYPE1                 0x0e
#define CC_PC_FF_IPV6PTYPE2                 0x1c
#define CC_PC_FF_IPV6DST1                   0x0f
#define CC_PC_FF_IPV6DST2                   0x1d
#define CC_PC_FF_IPV6SRC1                   0x10
#define CC_PC_FF_IPV6SRC2                   0x1e
#define CC_PC_FF_IPV6HOP_LIMIT              0x2a
#define CC_PC_FF_GREPTYPE                   0x11

#define CC_PC_FF_MINENCAP_PTYPE             0x12
#define CC_PC_FF_MINENCAP_IPDST             0x13
#define CC_PC_FF_MINENCAP_IPSRC             0x14
#define CC_PC_FF_MINENCAP_IPSRC_IPDST       0x15

#define CC_PC_FF_L4PSRC                     0x1f
#define CC_PC_FF_L4PDST                     0x20
#define CC_PC_FF_L4PSRC_L4PDST              0x21

#define CC_PC_FF_PPPPID                     0x05

#define CC_PC_PR_SHIM1                      0x22
#define CC_PC_PR_SHIM2                      0x23

#define CC_PC_GENERIC_WITHOUT_MASK          0x27
#define CC_PC_GENERIC_WITH_MASK             0x28
#define CC_PC_GENERIC_IC_GMASK              0x2B
#define CC_PC_GENERIC_IC_HASH_INDEXED       0x2C

#define CC_PR_OFFSET                        0x25
#define CC_PR_WITHOUT_OFFSET                0x26

#define CC_PC_PR_ETH_OFFSET                 19
#define CC_PC_PR_USER_DEFINED_SHIM1_OFFSET  16
#define CC_PC_PR_USER_DEFINED_SHIM2_OFFSET  17
#define CC_PC_PR_USER_LLC_SNAP_OFFSET       20
#define CC_PC_PR_VLAN1_OFFSET               21
#define CC_PC_PR_VLAN2_OFFSET               22
#define CC_PC_PR_PPPOE_OFFSET               24
#define CC_PC_PR_MPLS1_OFFSET               25
#define CC_PC_PR_MPLS_LAST_OFFSET           26
#define CC_PC_PR_IP1_OFFSET                 27
#define CC_PC_PR_IP_LAST_OFFSET             28
#define CC_PC_PR_MINENC_OFFSET              28
#define CC_PC_PR_L4_OFFSET                  30
#define CC_PC_PR_GRE_OFFSET                 29
#define CC_PC_PR_ETYPE_LAST_OFFSET          23
#define CC_PC_PR_NEXT_HEADER_OFFSET         31

#define CC_PC_ILLEGAL                       0xff
#define CC_SIZE_ILLEGAL                     0

#define FM_PCD_CC_KEYS_MATCH_TABLE_ALIGN    16
#define FM_PCD_CC_AD_TABLE_ALIGN            256
#define FM_PCD_CC_AD_ENTRY_SIZE             16
#define FM_PCD_CC_NUM_OF_KEYS               255

#define FM_PCD_AD_RESULT_CONTRL_FLOW_TYPE   0x00000000
#define FM_PCD_AD_RESULT_DATA_FLOW_TYPE     0x80000000
#define FM_PCD_AD_RESULT_PLCR_DIS           0x20000000
#define FM_PCD_AD_RESULT_EXTENDED_MODE      0x80000000
#define FM_PCD_AD_RESULT_NADEN              0x20000000
#define FM_PCD_AD_RESULT_STATISTICS_EN      0x40000000


#define FM_PCD_AD_CONT_LOOKUP_TYPE          0x40000000
#define FM_PCD_AD_CONT_LOOKUP_LCL_MASK      0x00800000

#define FM_PCD_AD_TYPE_MASK                 0xc0000000
#define FM_PCD_AD_OPCODE_MASK               0x0000000f

#define FM_PCD_AD_PROFILEID_FOR_CNTRL_SHIFT 16

#define GLBL_MASK_FOR_HASH_INDEXED          0xfff00000
#define CC_GLBL_MASK_SIZE                   4

typedef uint32_t ccPrivateInfo_t; /**< private info of CC: */

#define CC_PRIVATE_INFO_NONE                       0
#define CC_PRIVATE_INFO_IC_HASH_INDEX_LOOKUP       0x80000000
#define CC_PRIVATE_INFO_IC_HASH_EXACT_MATCH        0x40000000
#define CC_PRIVATE_INFO_IC_KEY_EXACT_MATCH         0x20000000
#define CC_PRIVATE_INFO_IC_DEQ_FQID_INDEX_LOOKUP   0x10000000

/***********************************************************************/
/*          Memory map                                                 */
/***********************************************************************/
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

typedef _Packed struct {
    volatile uint32_t fqid;
    volatile uint32_t plcrProfile;
    volatile uint32_t nia;
    volatile uint32_t  res;
} _PackedType t_AdOfTypeResult;

typedef _Packed struct {
    volatile uint32_t ccAdBase;
    volatile uint32_t matchTblPtr;
    volatile uint32_t pcAndOffsets;
    volatile uint32_t gmask;
} _PackedType t_AdOfTypeContLookup;

typedef _Packed union {
    volatile t_AdOfTypeResult        adResult;
    volatile t_AdOfTypeContLookup    adContLookup;
} _PackedType t_Ad;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


/***********************************************************************/
/*  Driver's internal structures                                        */
/***********************************************************************/

typedef enum e_ModifyState {
    e_MODIFY_STATE_ADD = 0,
    e_MODIFY_STATE_REMOVE,
    e_MODIFY_STATE_CHANGE
} e_ModifyState;

typedef struct {
    t_FmPcdCcNextEngineParams   nextEngineParams;
    uint32_t                    requiredAction;
    uint32_t                    shadowAction;
} t_FmPcdCcNextEngineAndRequiredActionParams;

typedef struct {
    t_Handle         p_Ad;
    e_FmPcdEngine    fmPcdEngine;
    bool             adAllocated;
    bool             isTree;

    uint32_t    myInfo;
    t_List      *h_CcNextNodesLst;
    t_Handle    h_AdditionalInfo;
    t_Handle    h_Node;
} t_FmPcdModifyCcAdditionalParams;

typedef struct {
    t_Handle p_AdTableNew;
    t_Handle p_KeysMatchTableNew;
    t_Handle p_AdTableOld;
    t_Handle p_KeysMatchTableOld;
    uint16_t numOfKeys;
    t_Handle h_CurrentNode;
    uint16_t keyIndex;
    t_Handle h_NodeForAdd;
    t_Handle h_NodeForRmv;
    t_Handle h_ManipForRmv;
    bool     tree;

    t_FmPcdCcNextEngineAndRequiredActionParams nextEngineAndRequiredAction[256];
} t_FmPcdModifyCcKeyAdditionalParams;

typedef struct {
    t_Handle h_Manip;
    t_Handle h_CcNode;
} t_CcNextEngineInfo;

typedef struct {
    uint16_t    numOfKeys;
    bool        glblMaskUpdated;
    t_Handle    p_GlblMask;
    bool        lclMask;
    uint8_t     parseCode;
    uint8_t     offset;
    uint8_t     prsArrayOffset;
    bool        ctrlFlow;
    uint8_t     owners;

    uint8_t     ccKeySizeAccExtraction;
    uint8_t     sizeOfExtraction;
    uint8_t     glblMaskSize;

    t_Handle    h_KeysMatchTable;
    t_Handle    h_AdTable;

    t_List      ccPrevNodesLst;

    t_List      ccTreeIdLst;
    t_List      ccTreesLst;

    t_Handle    h_FmPcd;
    uint32_t    shadowAction;
    bool        modifiedState;
    uint8_t     userSizeOfExtraction;

    t_FmPcdCcNextEngineAndRequiredActionParams nextEngineAndRequiredAction[256];
} t_FmPcdCcNode;

typedef struct {
    t_FmPcdCcNode       *p_FmPcdCcNode;
    bool                occupied;
    uint8_t             owners;
    volatile bool       lock;
} t_FmPcdCcNodeArray;

typedef struct {
    uint8_t             numOfEntriesInGroup;
    uint32_t            totalBitsMask;
    uint8_t             baseGroupEntry;
} t_FmPcdCcGroupParam;

typedef struct {
    uint8_t             netEnvId;
    uintptr_t           ccTreeBaseAddr;
    uint8_t             numOfGrps;
    t_FmPcdCcGroupParam fmPcdGroupParam[FM_PCD_MAX_NUM_OF_CC_GROUPS];
    t_List              fmPortsLst;
    volatile bool       lock;
    uint8_t             numOfEntries;
    uint8_t             owners;
    t_Handle            *fmPcdCcSavedManipParams[256];
    bool                modifiedState;
    uint32_t            requiredAction;
    t_FmPcdCcNextEngineAndRequiredActionParams nextEngineAndRequiredAction[FM_PCD_MAX_NUM_OF_KEYS];
} t_FmPcdCcTree;

typedef struct {
    t_FmPcdCcTree       *p_FmPcdCcTree;
    bool                occupied;
    uint8_t             owners;
    volatile bool       lock;
} t_FmPcdCcTreeArray;


bool FmPcdManipIsManipNode(t_Handle h_Ad);


#endif /* __FM_CC_H */
