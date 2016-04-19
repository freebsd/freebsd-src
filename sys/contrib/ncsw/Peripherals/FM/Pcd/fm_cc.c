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
 @File          fm_cc.c

 @Description   FM CC ...
*//***************************************************************************/
#include "std_ext.h"
#include "error_ext.h"
#include "string_ext.h"
#include "debug_ext.h"
#include "fm_pcd_ext.h"
#include "fm_muram_ext.h"

#include "fm_common.h"
#include "fm_hc.h"
#include "fm_cc.h"


#if defined(FM_CAPWAP_SUPPORT)
#define FM_PCD_CC_MANIP
#endif /* defined(FM_CAPWAP_SUPPORT) || ... */


t_Handle   FmPcdCcTreeGetSavedManipParams(t_Handle h_FmTree, uint8_t manipIndx)
{
    t_FmPcdCcTree *p_FmPcdCcTree = (t_FmPcdCcTree *)h_FmTree;

    ASSERT_COND(p_FmPcdCcTree);

    return p_FmPcdCcTree->fmPcdCcSavedManipParams[manipIndx];
}

void   FmPcdCcTreeSetSavedManipParams(t_Handle h_FmTree, t_Handle h_SavedManipParams, uint8_t   manipIndx)
{
    t_FmPcdCcTree *p_FmPcdCcTree = (t_FmPcdCcTree *)h_FmTree;

    ASSERT_COND(p_FmPcdCcTree);

    p_FmPcdCcTree->fmPcdCcSavedManipParams[manipIndx] = h_SavedManipParams;
}

uint8_t FmPcdCcGetParseCode(t_Handle h_CcNode)
{
    t_FmPcdCcNode *p_FmPcdCcNode = (t_FmPcdCcNode *)h_CcNode;

    ASSERT_COND(p_FmPcdCcNode);
    return p_FmPcdCcNode->parseCode;
}

uint8_t FmPcdCcGetOffset(t_Handle h_CcNode)
{
    t_FmPcdCcNode *p_FmPcdCcNode = (t_FmPcdCcNode *)h_CcNode;

    ASSERT_COND(p_FmPcdCcNode);
    return p_FmPcdCcNode->offset;
}

uint16_t FmPcdCcGetNumOfKeys(t_Handle h_CcNode)
{
    t_FmPcdCcNode *p_FmPcdCcNode = (t_FmPcdCcNode *)h_CcNode;

    ASSERT_COND(p_FmPcdCcNode);
    return p_FmPcdCcNode->numOfKeys;
}
static void EnqueueNodeInfoToRelevantLst(t_List *p_List, t_CcNodeInformation *p_CcInfo)
{
    t_CcNodeInformation *p_CcInformation;
    uint32_t            intFlags;

    p_CcInformation = (t_CcNodeInformation *)XX_Malloc(sizeof(t_CcNodeInformation));
    if (p_CcInformation)
    {
        memset(p_CcInformation, 0, sizeof(t_CcNodeInformation));
        memcpy(p_CcInformation, p_CcInfo, sizeof(t_CcNodeInformation));
        INIT_LIST(&p_CcInformation->node);

        intFlags = XX_DisableAllIntr();
        LIST_AddToTail(&p_CcInformation->node, p_List);
        XX_RestoreAllIntr(intFlags);
    }
    else
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("CC Node Information"));
}


static t_CcNodeInformation* FindNodeInfoInReleventLst(t_List *p_List, t_Handle h_Info)
{
    t_CcNodeInformation *p_CcInformation;
    t_List *p_Pos;
    uint32_t            intFlags;

    intFlags = XX_DisableAllIntr();
    for (p_Pos = NCSW_LIST_FIRST(p_List); p_Pos != (p_List); p_Pos = NCSW_LIST_NEXT(p_Pos))
    {
        p_CcInformation = CC_NODE_F_OBJECT(p_Pos);
        ASSERT_COND(p_CcInformation->h_CcNode);
        if(p_CcInformation->h_CcNode == h_Info)
        {
            XX_RestoreAllIntr(intFlags);
            return p_CcInformation;
        }
    }
    XX_RestoreAllIntr(intFlags);
    return NULL;
}

static void DequeueNodeInfoFromRelevantLst(t_List *p_List, t_Handle h_Info)
{
    t_CcNodeInformation *p_CcInformation = NULL;
    uint32_t            intFlags;
    t_List              *p_Pos;

    intFlags = XX_DisableAllIntr();
    if (LIST_IsEmpty(p_List))
    {
        XX_RestoreAllIntr(intFlags);
        return;
    }

    for (p_Pos = NCSW_LIST_FIRST(p_List); p_Pos != (p_List); p_Pos = NCSW_LIST_NEXT(p_Pos))
    {
        p_CcInformation = CC_NODE_F_OBJECT(p_Pos);
        ASSERT_COND(p_CcInformation->h_CcNode);
        if (p_CcInformation->h_CcNode == h_Info)
            break;
    }
    if (p_CcInformation)
        LIST_DelAndInit(&p_CcInformation->node);
    XX_RestoreAllIntr(intFlags);
}

static t_Error FmPcdCcSetRequiredAction(t_Handle h_FmPcd, uint32_t requiredAction, t_FmPcdCcNextEngineAndRequiredActionParams *p_CcNextEngineParamsTmp,
                                        t_Handle h_AdTmp, uint16_t numOfEntries, t_Handle h_Tree)
{

    t_AdOfTypeResult    *p_AdTmp = (t_AdOfTypeResult *)h_AdTmp;
    uint32_t            tmpReg32;
    t_Error             err;
    t_FmPcdCcNode       *p_FmPcdCcNode;
    int                 i = 0;
    uint16_t            tmp = 0;
    uint16_t            profileId;
    uint8_t             relativeSchemeId, physicalSchemeId;
    t_CcNodeInformation ccNodeInfo;

     for(i = 0; i < numOfEntries; i++)
     {
        if(i == 0)
            h_AdTmp = PTR_MOVE(h_AdTmp, i*FM_PCD_CC_AD_ENTRY_SIZE);
        else
            h_AdTmp = PTR_MOVE(h_AdTmp, FM_PCD_CC_AD_ENTRY_SIZE);

        if(p_CcNextEngineParamsTmp[i].shadowAction & requiredAction)
            continue;
        switch(p_CcNextEngineParamsTmp[i].nextEngineParams.nextEngine)
        {
            case(e_FM_PCD_CC):
                if(requiredAction)
                {
                    p_FmPcdCcNode = p_CcNextEngineParamsTmp[i].nextEngineParams.params.ccParams.h_CcNode;
                    ASSERT_COND(p_FmPcdCcNode);
                    if(p_FmPcdCcNode->shadowAction == requiredAction)
                        break;
                    if((requiredAction & UPDATE_CC_WITH_TREE) && !(p_FmPcdCcNode->shadowAction & UPDATE_CC_WITH_TREE))
                    {

                        ASSERT_COND(LIST_NumOfObjs(&p_FmPcdCcNode->ccTreesLst) == 0);
                        if(p_FmPcdCcNode->shadowAction & UPDATE_CC_WITH_DELETE_TREE)
                            p_FmPcdCcNode->shadowAction &= ~UPDATE_CC_WITH_DELETE_TREE;
                        memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
                        ccNodeInfo.h_CcNode = h_Tree;
                        EnqueueNodeInfoToRelevantLst(&p_FmPcdCcNode->ccTreesLst, &ccNodeInfo);
                        p_CcNextEngineParamsTmp[i].shadowAction |= UPDATE_CC_WITH_TREE;
                    }
                    if((requiredAction & UPDATE_CC_WITH_DELETE_TREE) && !(p_FmPcdCcNode->shadowAction & UPDATE_CC_WITH_DELETE_TREE))
                    {
                        ASSERT_COND(LIST_NumOfObjs(&p_FmPcdCcNode->ccTreesLst) == 1);
                        if(p_FmPcdCcNode->shadowAction & UPDATE_CC_WITH_TREE)
                            p_FmPcdCcNode->shadowAction &= ~UPDATE_CC_WITH_TREE;
                        DequeueNodeInfoFromRelevantLst(&p_FmPcdCcNode->ccTreesLst, h_Tree);
                        p_CcNextEngineParamsTmp[i].shadowAction |= UPDATE_CC_WITH_DELETE_TREE;
                    }
                    if(p_FmPcdCcNode->nextEngineAndRequiredAction[p_FmPcdCcNode->numOfKeys].nextEngineParams.nextEngine != e_FM_PCD_INVALID)
                        tmp  = (uint8_t)(p_FmPcdCcNode->numOfKeys + 1);
                    else
                        tmp = p_FmPcdCcNode->numOfKeys;
                    err = FmPcdCcSetRequiredAction(h_FmPcd, requiredAction, p_FmPcdCcNode->nextEngineAndRequiredAction, p_FmPcdCcNode->h_AdTable, tmp, h_Tree);
                    if(err != E_OK)
                        return err;
                    p_FmPcdCcNode->shadowAction |= requiredAction;
                }
                break;

            case(e_FM_PCD_KG):
                if((requiredAction & UPDATE_NIA_ENQ_WITHOUT_DMA) && !(p_CcNextEngineParamsTmp[i].shadowAction & UPDATE_NIA_ENQ_WITHOUT_DMA))
                {
                    physicalSchemeId = (uint8_t)(PTR_TO_UINT(p_CcNextEngineParamsTmp[i].nextEngineParams.params.kgParams.h_DirectScheme)-1);
                    relativeSchemeId = FmPcdKgGetRelativeSchemeId(h_FmPcd, physicalSchemeId);
                    if(relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
                        RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);
                    if (!FmPcdKgIsSchemeValidSw(h_FmPcd, relativeSchemeId))
                         RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid direct scheme."));
                    if(!KgIsSchemeAlwaysDirect(h_FmPcd, relativeSchemeId))
                        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("For this action scheme has to be direct."));
                    err = FmPcdKgCcGetSetParams(h_FmPcd, p_CcNextEngineParamsTmp[i].nextEngineParams.params.kgParams.h_DirectScheme, requiredAction);
                    if(err != E_OK)
                        RETURN_ERROR(MAJOR, err, NO_MSG);
                    p_CcNextEngineParamsTmp[i].shadowAction |= requiredAction;
                }
                break;

            case(e_FM_PCD_PLCR):
                if((requiredAction & UPDATE_NIA_ENQ_WITHOUT_DMA) && !(p_CcNextEngineParamsTmp[i].shadowAction & UPDATE_NIA_ENQ_WITHOUT_DMA))
                {
                    if(!p_CcNextEngineParamsTmp[i].nextEngineParams.params.plcrParams.overrideParams)
                        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("In this intialization only overrideFqid can be intiizliaes"));
                    if(!p_CcNextEngineParamsTmp[i].nextEngineParams.params.plcrParams.sharedProfile)
                        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("In this intialization only overrideFqid can be intiizliaes"));
                    err =  FmPcdPlcrGetAbsoluteProfileId(h_FmPcd, e_FM_PCD_PLCR_SHARED, NULL, p_CcNextEngineParamsTmp[i].nextEngineParams.params.plcrParams.newRelativeProfileId, &profileId);
                    if(err!= E_OK)
                        RETURN_ERROR(MAJOR, err, NO_MSG);
                    err = FmPcdPlcrCcGetSetParams(h_FmPcd, profileId, requiredAction);
                    if(err != E_OK)
                        RETURN_ERROR(MAJOR, err, NO_MSG);
                    p_CcNextEngineParamsTmp[i].shadowAction |= requiredAction;
                }
                break;

            case(e_FM_PCD_DONE):
                if((requiredAction & UPDATE_NIA_ENQ_WITHOUT_DMA) && !(p_CcNextEngineParamsTmp[i].shadowAction & UPDATE_NIA_ENQ_WITHOUT_DMA))
                {
                    tmpReg32 = GET_UINT32(p_AdTmp->nia);
                    if((tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)) != (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME))
                        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine was previosely assigned not as PCD_DONE"));
                    tmpReg32 |= NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;
                    WRITE_UINT32(p_AdTmp->nia, tmpReg32);
                    p_CcNextEngineParamsTmp[i].shadowAction |= requiredAction;
                }
                break;

            default:
                break;
        }
     }

     return E_OK;
}

static t_Error CcUpdateParam(t_Handle                                   h_FmPcd,
                             t_Handle                                   h_FmPort,
                             t_FmPcdCcNextEngineAndRequiredActionParams *p_CcNextEngineParams,
                             uint16_t                                   numOfEntries,
                             t_Handle                                   h_Ad,
                             bool                                       validate,
                             uint16_t                                   level,
                             t_Handle                                   h_FmTree,
                             bool                                       modify)
{
    t_CcNodeInformation *p_CcNodeInfo;
    t_FmPcdCcNode       *p_FmPcdCcNode;
    t_Error             err;
    uint16_t            tmp = 0;
    int                 i = 0;

    level++;

    if(numOfEntries)
    {
        for(i = 0; i < numOfEntries; i++)
        {
            if(i == 0)
                h_Ad = PTR_MOVE(h_Ad, i*FM_PCD_CC_AD_ENTRY_SIZE);
            else
                h_Ad = PTR_MOVE(h_Ad, FM_PCD_CC_AD_ENTRY_SIZE);

            if(p_CcNextEngineParams[i].nextEngineParams.nextEngine == e_FM_PCD_CC)
            {
                p_FmPcdCcNode = p_CcNextEngineParams[i].nextEngineParams.params.ccParams.h_CcNode;
                ASSERT_COND(p_FmPcdCcNode);
                p_CcNodeInfo = FindNodeInfoInReleventLst(&p_FmPcdCcNode->ccTreesLst,h_FmTree);
                ASSERT_COND(p_CcNodeInfo);
                p_CcNodeInfo->index = level;
#ifdef FM_PCD_CC_MANIP
                if(p_CcNextEngineParams[i].nextEngineParams.h_Manip)
                {
                    err = FmPcdManipUpdate(h_FmPcd, h_FmPort, p_CcNextEngineParams[i].nextEngineParams.h_Manip, h_Ad, validate, p_CcNodeInfo->index, h_FmTree, modify);
                    if(err)
                        RETURN_ERROR(MAJOR, err, NO_MSG);
                }
#endif /* FM_PCD_CC_MANIP */

                if(p_FmPcdCcNode->nextEngineAndRequiredAction[p_FmPcdCcNode->numOfKeys].nextEngineParams.nextEngine != e_FM_PCD_INVALID)
                    tmp  = (uint8_t)(p_FmPcdCcNode->numOfKeys + 1);
                else
                    tmp = p_FmPcdCcNode->numOfKeys;

                err = CcUpdateParam(h_FmPcd, h_FmPort, p_FmPcdCcNode->nextEngineAndRequiredAction, tmp, p_FmPcdCcNode->h_AdTable, validate,level, h_FmTree, modify);
                if(err)
                    RETURN_ERROR(MAJOR, err, NO_MSG);
            }
#ifdef FM_PCD_CC_MANIP
            else
            {
                if(p_CcNextEngineParams[i].nextEngineParams.h_Manip)
                {
                    err = FmPcdManipUpdate(h_FmPcd, h_FmPort, p_CcNextEngineParams[i].nextEngineParams.h_Manip, h_Ad, validate, level,h_FmTree, modify);
                    if(err)
                        RETURN_ERROR(MAJOR, err, NO_MSG);
                }
            }
#endif /* FM_PCD_CC_MANIP */
          }
    }

    return E_OK;
}
static bool IsNodeInModifiedState(t_Handle h_CcNode)
{
    t_FmPcdCcNode *p_CcNode = (t_FmPcdCcNode *)h_CcNode;

    ASSERT_COND(p_CcNode);

    return p_CcNode->modifiedState;
}

static void UpdateNodeWithModifiedState(t_Handle h_CcNode, bool modifiedState)
{
    t_FmPcdCcNode *p_FmPcdCcNode = (t_FmPcdCcNode *)h_CcNode;

    ASSERT_COND(p_FmPcdCcNode);

    p_FmPcdCcNode->modifiedState = modifiedState;
}

static ccPrivateInfo_t IcDefineCode(t_FmPcdCcNodeParams *p_CcNodeParam)
{
    switch (p_CcNodeParam->extractCcParams.extractNonHdr.action)
    {
        case(e_FM_PCD_ACTION_EXACT_MATCH):
            switch(p_CcNodeParam->extractCcParams.extractNonHdr.src)
            {
                case(e_FM_PCD_EXTRACT_FROM_KEY):
                    return CC_PRIVATE_INFO_IC_KEY_EXACT_MATCH;
                case(e_FM_PCD_EXTRACT_FROM_HASH):
                    return CC_PRIVATE_INFO_IC_HASH_EXACT_MATCH;
                default:
                    return CC_PRIVATE_INFO_NONE;
            }
        case(e_FM_PCD_ACTION_INDEXED_LOOKUP):
            switch(p_CcNodeParam->extractCcParams.extractNonHdr.src)
            {
                case(e_FM_PCD_EXTRACT_FROM_HASH):
                    return CC_PRIVATE_INFO_IC_HASH_INDEX_LOOKUP;
                case(e_FM_PCD_EXTRACT_FROM_FLOW_ID):
                    return CC_PRIVATE_INFO_IC_DEQ_FQID_INDEX_LOOKUP;
                default:
                  return  CC_PRIVATE_INFO_NONE;
            }
       default:
           break;
    }
    return CC_PRIVATE_INFO_NONE;
}

static t_CcNodeInformation * DequeueAdditionalInfoFromRelevantLst(t_List *p_List)
{
    t_CcNodeInformation   *p_CcNodeInfo = NULL;
    uint32_t        intFlags;

    intFlags = XX_DisableAllIntr();
    if (!LIST_IsEmpty(p_List))
    {
        p_CcNodeInfo = CC_NODE_F_OBJECT(p_List->p_Next);
        LIST_DelAndInit(&p_CcNodeInfo->node);
    }
    XX_RestoreAllIntr(intFlags);
    return p_CcNodeInfo;
}

static void ReleaseLst(t_List *p_List)
{
    t_CcNodeInformation   *p_CcNodeInfo = NULL;

    if(!LIST_IsEmpty(p_List))
    {
        p_CcNodeInfo = DequeueAdditionalInfoFromRelevantLst(p_List);
        while (p_CcNodeInfo)
        {
            XX_Free(p_CcNodeInfo);
            p_CcNodeInfo = DequeueAdditionalInfoFromRelevantLst(p_List);
        }
    }
    LIST_DelAndInit(p_List);
}

void FmPcdCcTreeReleaseLock(t_Handle h_FmPcdCcTree)
{
    RELEASE_LOCK(((t_FmPcdCcTree *)h_FmPcdCcTree)->lock);
}

void FmPcdCcNodeTreeReleaseLock(t_List *p_List)
{
    t_List              *p_Pos;
    t_CcNodeInformation *p_CcNodeInfo;
    t_Handle            h_FmPcdCcTree;

    LIST_FOR_EACH(p_Pos, p_List)
    {
        p_CcNodeInfo = CC_NODE_F_OBJECT(p_Pos);
        h_FmPcdCcTree = p_CcNodeInfo->h_CcNode;
        FmPcdCcTreeReleaseLock(h_FmPcdCcTree);
    }
    ReleaseLst(p_List);
}

static void DeleteNode(t_FmPcdCcNode *p_FmPcdCcNode)
{
    if(p_FmPcdCcNode)
    {
        if(p_FmPcdCcNode->p_GlblMask)
        {
            XX_Free(p_FmPcdCcNode->p_GlblMask);
            p_FmPcdCcNode->p_GlblMask = NULL;
        }
        if(p_FmPcdCcNode->h_KeysMatchTable)
        {
            FM_MURAM_FreeMem(FmPcdGetMuramHandle(p_FmPcdCcNode->h_FmPcd), p_FmPcdCcNode->h_KeysMatchTable);
            p_FmPcdCcNode->h_KeysMatchTable = NULL;
        }
        if(p_FmPcdCcNode->h_AdTable)
        {
            FM_MURAM_FreeMem(FmPcdGetMuramHandle(p_FmPcdCcNode->h_FmPcd), p_FmPcdCcNode->h_AdTable);
            p_FmPcdCcNode->h_AdTable = NULL;
        }

        ReleaseLst(&p_FmPcdCcNode->ccPrevNodesLst);
        ReleaseLst(&p_FmPcdCcNode->ccTreeIdLst);
        ReleaseLst(&p_FmPcdCcNode->ccTreesLst);

        XX_Free(p_FmPcdCcNode);
    }
}

static void DeleteTree(t_FmPcdCcTree *p_FmPcdTree, t_FmPcd *p_FmPcd)
{
    if(p_FmPcdTree)
    {
        if(p_FmPcdTree->ccTreeBaseAddr)
        {
            FM_MURAM_FreeMem(FmPcdGetMuramHandle(p_FmPcd), UINT_TO_PTR(p_FmPcdTree->ccTreeBaseAddr));
            p_FmPcdTree->ccTreeBaseAddr = 0;
        }

        ReleaseLst(&p_FmPcdTree->fmPortsLst);

        XX_Free(p_FmPcdTree);
    }
}

static void  UpdateNodeOwner(t_FmPcdCcNode *p_FmPcdCcNode, bool add)
{
    ASSERT_COND(p_FmPcdCcNode);

    if(add)
        p_FmPcdCcNode->owners++;
    else
    {
        ASSERT_COND(p_FmPcdCcNode->owners);
        p_FmPcdCcNode->owners--;
    }
}

static void  GetCcExtractKeySize(uint8_t parseCodeRealSize, uint8_t *parseCodeCcSize)
{
    if((parseCodeRealSize > 0) && (parseCodeRealSize < 2))
        *parseCodeCcSize = 1;
    else if(parseCodeRealSize == 2)
        *parseCodeCcSize = 2;
    else if((parseCodeRealSize > 2)    && (parseCodeRealSize <= 4))
        *parseCodeCcSize = 4;
    else if((parseCodeRealSize > 4)    && (parseCodeRealSize <= 8))
        *parseCodeCcSize = 8;
    else if((parseCodeRealSize > 8)    && (parseCodeRealSize <= 16))
        *parseCodeCcSize = 16;
    else if((parseCodeRealSize  > 16)  && (parseCodeRealSize <= 24))
        *parseCodeCcSize = 24;
    else if((parseCodeRealSize  > 24)  && (parseCodeRealSize <= 32))
        *parseCodeCcSize = 32;
    else if((parseCodeRealSize  > 32)  && (parseCodeRealSize <= 40))
        *parseCodeCcSize = 40;
    else if((parseCodeRealSize  > 40)  && (parseCodeRealSize <= 48))
        *parseCodeCcSize = 48;
    else if((parseCodeRealSize  > 48)  && (parseCodeRealSize <= 56))
        *parseCodeCcSize = 56;
    else
        *parseCodeCcSize = 0;
}

static void  GetSizeHeaderField(e_NetHeaderType hdr,t_FmPcdFields field,uint8_t *parseCodeRealSize)
{
    switch(hdr)
    {
        case (HEADER_TYPE_ETH):
            switch(field.eth)
            {
                case(NET_HEADER_FIELD_ETH_DA):
                    *parseCodeRealSize = 6;
                    break;
                case(NET_HEADER_FIELD_ETH_SA):
                    *parseCodeRealSize = 6;
                    break;
                case(NET_HEADER_FIELD_ETH_TYPE):
                    *parseCodeRealSize = 2;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported1"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case(HEADER_TYPE_PPPoE):
            switch(field.pppoe)
            {
                case(NET_HEADER_FIELD_PPPoE_PID):
                    *parseCodeRealSize = 2;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported1"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_VLAN):
            switch(field.vlan)
            {
               case(NET_HEADER_FIELD_VLAN_TCI):
                    *parseCodeRealSize = 2;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported2"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_MPLS):
            switch(field.mpls)
            {
                case(NET_HEADER_FIELD_MPLS_LABEL_STACK):
                    *parseCodeRealSize = 4;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported3"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_IPv4):
            switch(field.ipv4)
            {
                case(NET_HEADER_FIELD_IPv4_DST_IP):
                case(NET_HEADER_FIELD_IPv4_SRC_IP):
                    *parseCodeRealSize = 4;
                    break;
                case(NET_HEADER_FIELD_IPv4_TOS):
                case(NET_HEADER_FIELD_IPv4_PROTO):
                    *parseCodeRealSize = 1;
                    break;
                case(NET_HEADER_FIELD_IPv4_DST_IP | NET_HEADER_FIELD_IPv4_SRC_IP):
                    *parseCodeRealSize = 8;
                    break;
                case(NET_HEADER_FIELD_IPv4_TTL):
                    *parseCodeRealSize = 1;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported4"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_IPv6):
            switch(field.ipv6)
            {
                case(NET_HEADER_FIELD_IPv6_VER | NET_HEADER_FIELD_IPv6_FL | NET_HEADER_FIELD_IPv6_TC):
                   *parseCodeRealSize = 4;
                    break;
                case(NET_HEADER_FIELD_IPv6_NEXT_HDR):
                case(NET_HEADER_FIELD_IPv6_HOP_LIMIT):
                   *parseCodeRealSize = 1;
                    break;
                case(NET_HEADER_FIELD_IPv6_DST_IP):
                case(NET_HEADER_FIELD_IPv6_SRC_IP):
                   *parseCodeRealSize = 16;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported5"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_GRE):
            switch(field.gre)
            {
                case(NET_HEADER_FIELD_GRE_TYPE):
                   *parseCodeRealSize = 2;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported6"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_MINENCAP):
            switch(field.minencap)
            {
                case(NET_HEADER_FIELD_MINENCAP_TYPE):
                   *parseCodeRealSize = 1;
                    break;
                case(NET_HEADER_FIELD_MINENCAP_DST_IP):
                 case(NET_HEADER_FIELD_MINENCAP_SRC_IP):
                  *parseCodeRealSize = 4;
                    break;
                 case(NET_HEADER_FIELD_MINENCAP_SRC_IP | NET_HEADER_FIELD_MINENCAP_DST_IP):
                  *parseCodeRealSize = 8;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported7"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_TCP):
            switch(field.tcp)
            {
                case(NET_HEADER_FIELD_TCP_PORT_SRC):
                case(NET_HEADER_FIELD_TCP_PORT_DST):
                   *parseCodeRealSize = 2;
                    break;
                 case(NET_HEADER_FIELD_TCP_PORT_SRC | NET_HEADER_FIELD_TCP_PORT_DST):
                  *parseCodeRealSize = 4;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported8"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
        case (HEADER_TYPE_UDP):
            switch(field.udp)
            {
                case(NET_HEADER_FIELD_UDP_PORT_SRC):
                case(NET_HEADER_FIELD_UDP_PORT_DST):
                   *parseCodeRealSize = 2;
                    break;
                 case(NET_HEADER_FIELD_UDP_PORT_SRC | NET_HEADER_FIELD_UDP_PORT_DST):
                  *parseCodeRealSize = 4;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported9"));
                    *parseCodeRealSize = CC_SIZE_ILLEGAL;
                    break;
            }
            break;
       default:
            REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported10"));
            *parseCodeRealSize = CC_SIZE_ILLEGAL;
            break;
    }
}

static t_Error ValidateNextEngineParams(t_Handle h_FmPcd, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams)
{
    uint16_t                    absoluteProfileId;
    t_Error                     err = E_OK;
    uint8_t                     relativeSchemeId;

    switch(p_FmPcdCcNextEngineParams->nextEngine)
    {
         case(e_FM_PCD_INVALID):
             err = E_NOT_SUPPORTED;
             break;
         case(e_FM_PCD_DONE):
             if(p_FmPcdCcNextEngineParams->params.enqueueParams.action == e_FM_PCD_ENQ_FRAME)
             {
                 if(p_FmPcdCcNextEngineParams->params.enqueueParams.overrideFqid &&
                         !p_FmPcdCcNextEngineParams->params.enqueueParams.newFqid)
                     RETURN_ERROR(MAJOR, E_INVALID_STATE, ("not defined fqid for control flow for BMI next engine "));
                 if(p_FmPcdCcNextEngineParams->params.enqueueParams.newFqid & ~0x00FFFFFF)
                     RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fqidForCtrlFlow must be between 1 and 2^24-1"));
             }
            break;
        case(e_FM_PCD_KG):
            relativeSchemeId = FmPcdKgGetRelativeSchemeId(h_FmPcd, (uint8_t)(PTR_TO_UINT(p_FmPcdCcNextEngineParams->params.kgParams.h_DirectScheme)-1));
            if(relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
                RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

            if(!FmPcdKgIsSchemeValidSw(h_FmPcd, relativeSchemeId))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("not valid schemeIndex in KG next engine param"));
            if(!KgIsSchemeAlwaysDirect(h_FmPcd, relativeSchemeId))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("CC Node may point only to a scheme that is always direct."));
            break;
        case(e_FM_PCD_PLCR):
            if(p_FmPcdCcNextEngineParams->params.plcrParams.overrideParams)
            {
                /* if private policer profile, it may be uninitialized yet, therefor no checks are done at this stage */
                if(p_FmPcdCcNextEngineParams->params.plcrParams.sharedProfile)
                {
                    err = FmPcdPlcrGetAbsoluteProfileId(h_FmPcd,e_FM_PCD_PLCR_SHARED,NULL,p_FmPcdCcNextEngineParams->params.plcrParams.newRelativeProfileId, &absoluteProfileId);
                    if(err)
                        RETURN_ERROR(MAJOR, err, ("Shared profile offset is out of range"));
                    if(!FmPcdPlcrIsProfileValid(h_FmPcd, absoluteProfileId))
                        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid profile"));
                }
                else
                {
                }
                /* TODO - add check according to the revision of the chip.
                if(!p_FmPcdCcNextEngineParams->params.plcrParams.newFqid ||
                   (p_FmPcdCcNextEngineParams->params.plcrParams.newFqid & ~0x00FFFFFF))
                    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("newFqid  must be between 1 and 2^24-1"));
                */
            }
            break;
        case(e_FM_PCD_CC):
            if(!p_FmPcdCcNextEngineParams->params.ccParams.h_CcNode)
                RETURN_ERROR(MAJOR, E_NULL_POINTER, ("handler to next Node is NULL"));
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine is not correct"));
    }
    return err;
}

static uint8_t GetGenParseCode(e_FmPcdExtractFrom src, uint32_t offset, bool glblMask, uint8_t *parseArrayOffset, bool fromIc, ccPrivateInfo_t icCode)
{
    if(!fromIc)
    {
        switch(src)
        {
            case(e_FM_PCD_EXTRACT_FROM_FRAME_START):
                if(glblMask)
                    return CC_PC_GENERIC_WITH_MASK ;
                else
                  return CC_PC_GENERIC_WITHOUT_MASK;
            case(e_FM_PCD_EXTRACT_FROM_CURR_END_OF_PARSE):
                *parseArrayOffset = CC_PC_PR_NEXT_HEADER_OFFSET;
                if(offset)
                    return CC_PR_OFFSET;
                else
                    return CC_PR_WITHOUT_OFFSET;
            default:
                REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("Illegal 'extract from' src"));
                return CC_PC_ILLEGAL;
        }
    }
    else
    {
        switch (icCode)
        {
            case(CC_PRIVATE_INFO_IC_KEY_EXACT_MATCH):
                 *parseArrayOffset = 0x50;
                 return CC_PC_GENERIC_IC_GMASK;
            case(CC_PRIVATE_INFO_IC_HASH_EXACT_MATCH):
               *parseArrayOffset = 0x48;
               return CC_PC_GENERIC_IC_GMASK;
            case(CC_PRIVATE_INFO_IC_HASH_INDEX_LOOKUP):
                *parseArrayOffset = 0x48;
                 return CC_PC_GENERIC_IC_HASH_INDEXED;
            case(CC_PRIVATE_INFO_IC_DEQ_FQID_INDEX_LOOKUP):
                *parseArrayOffset = 0x16;
                 return CC_PC_GENERIC_IC_HASH_INDEXED;
            default:
                REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("Illegal 'extract from' src"));
                break;
        }
    }
    return CC_PC_ILLEGAL;
}

static uint8_t GetFullFieldParseCode(e_NetHeaderType hdr, e_FmPcdHdrIndex index, t_FmPcdFields field)
{

      switch(hdr)
        {
            case(HEADER_TYPE_NONE):
                ASSERT_COND(FALSE);
                return CC_PC_ILLEGAL;

       case(HEADER_TYPE_ETH):
                switch(field.eth)
                {
                    case(NET_HEADER_FIELD_ETH_DA):
                        return CC_PC_FF_MACDST;
                    case(NET_HEADER_FIELD_ETH_SA):
                         return CC_PC_FF_MACSRC;
                    case(NET_HEADER_FIELD_ETH_TYPE):
                         return CC_PC_FF_ETYPE;
                    default:
                        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                        return CC_PC_ILLEGAL;
                }

         case(HEADER_TYPE_VLAN):
            switch(field.vlan)
            {
                case(NET_HEADER_FIELD_VLAN_TCI):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_TCI1;
                    if(index == e_FM_PCD_HDR_INDEX_LAST)
                        return CC_PC_FF_TCI2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
                default:
                        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                        return CC_PC_ILLEGAL;
            }

        case(HEADER_TYPE_MPLS):
            switch(field.mpls)
            {
                case(NET_HEADER_FIELD_MPLS_LABEL_STACK):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_MPLS1;
                    if(index == e_FM_PCD_HDR_INDEX_LAST)
                        return CC_PC_FF_MPLS_LAST;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal MPLS index"));
                    return CC_PC_ILLEGAL;
               default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
             }

        case(HEADER_TYPE_IPv4):
            switch(field.ipv4)
            {
                case(NET_HEADER_FIELD_IPv4_DST_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV4DST1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV4DST2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv4_TOS):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV4IPTOS_TC1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV4IPTOS_TC2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv4_PROTO):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV4PTYPE1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV4PTYPE2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv4_SRC_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV4SRC1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV4SRC2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv4_SRC_IP | NET_HEADER_FIELD_IPv4_DST_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV4SRC1_IPV4DST1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV4SRC2_IPV4DST2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv4_TTL):
                    return CC_PC_FF_IPV4TTL;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
            }

        case(HEADER_TYPE_IPv6):
             switch(field.ipv6)
            {
                case(NET_HEADER_FIELD_IPv6_VER | NET_HEADER_FIELD_IPv6_FL | NET_HEADER_FIELD_IPv6_TC):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPTOS_IPV6TC1_IPV6FLOW1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPTOS_IPV6TC2_IPV6FLOW2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv6_NEXT_HDR):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV6PTYPE1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV6PTYPE2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv6_DST_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV6DST1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV6DST2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv6_SRC_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return CC_PC_FF_IPV6SRC1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return CC_PC_FF_IPV6SRC2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return CC_PC_ILLEGAL;
                case(NET_HEADER_FIELD_IPv6_HOP_LIMIT):
                    return CC_PC_FF_IPV6HOP_LIMIT;
                 default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
            }

        case(HEADER_TYPE_GRE):
            switch(field.gre)
            {
                case(NET_HEADER_FIELD_GRE_TYPE):
                    return CC_PC_FF_GREPTYPE;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
           }
        case(HEADER_TYPE_MINENCAP):
            switch(field.minencap)
            {
                case(NET_HEADER_FIELD_MINENCAP_TYPE):
                    return CC_PC_FF_MINENCAP_PTYPE;
                case(NET_HEADER_FIELD_MINENCAP_DST_IP):
                    return CC_PC_FF_MINENCAP_IPDST;
                case(NET_HEADER_FIELD_MINENCAP_SRC_IP):
                    return CC_PC_FF_MINENCAP_IPSRC;
                case(NET_HEADER_FIELD_MINENCAP_SRC_IP | NET_HEADER_FIELD_MINENCAP_DST_IP):
                    return CC_PC_FF_MINENCAP_IPSRC_IPDST;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
           }

        case(HEADER_TYPE_TCP):
            switch(field.tcp)
            {
                case(NET_HEADER_FIELD_TCP_PORT_SRC):
                    return CC_PC_FF_L4PSRC;
                case(NET_HEADER_FIELD_TCP_PORT_DST):
                    return CC_PC_FF_L4PDST;
                case(NET_HEADER_FIELD_TCP_PORT_DST | NET_HEADER_FIELD_TCP_PORT_SRC):
                    return CC_PC_FF_L4PSRC_L4PDST;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
            }

        case(HEADER_TYPE_PPPoE):
            switch(field.pppoe)
            {
                case(NET_HEADER_FIELD_PPPoE_PID):
                    return CC_PC_FF_PPPPID;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
            }

        case(HEADER_TYPE_UDP):
            switch(field.udp)
            {
                case(NET_HEADER_FIELD_UDP_PORT_SRC):
                    return CC_PC_FF_L4PSRC;
                case(NET_HEADER_FIELD_UDP_PORT_DST):
                    return CC_PC_FF_L4PDST;
                case(NET_HEADER_FIELD_UDP_PORT_DST | NET_HEADER_FIELD_UDP_PORT_SRC):
                    return CC_PC_FF_L4PSRC_L4PDST;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
            }

         default:
            REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
            return CC_PC_ILLEGAL;
    }
}

static uint8_t GetPrParseCode(e_NetHeaderType hdr, e_FmPcdHdrIndex hdrIndex, uint32_t offset, bool glblMask, uint8_t *parseArrayOffset)
{
    bool offsetRelevant = FALSE;

    if(offset)
        offsetRelevant = TRUE;

    switch(hdr){
        case(HEADER_TYPE_NONE):
            ASSERT_COND(FALSE);
            return CC_PC_ILLEGAL;
        case(HEADER_TYPE_ETH):
            *parseArrayOffset = (uint8_t)CC_PC_PR_ETH_OFFSET;
            break;
        case(HEADER_TYPE_USER_DEFINED_SHIM1):
            if(offset || glblMask)
                *parseArrayOffset = (uint8_t)CC_PC_PR_USER_DEFINED_SHIM1_OFFSET;
            else
                return CC_PC_PR_SHIM1;
            break;
        case(HEADER_TYPE_USER_DEFINED_SHIM2):
            if(offset || glblMask)
                *parseArrayOffset = (uint8_t)CC_PC_PR_USER_DEFINED_SHIM2_OFFSET;
            else
                return CC_PC_PR_SHIM2;
            break;
      case(HEADER_TYPE_LLC_SNAP):
            *parseArrayOffset = CC_PC_PR_USER_LLC_SNAP_OFFSET;
            break;
        case(HEADER_TYPE_PPPoE):
            *parseArrayOffset = CC_PC_PR_PPPOE_OFFSET;
            break;
            case(HEADER_TYPE_MPLS):
                 if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                        *parseArrayOffset = CC_PC_PR_MPLS1_OFFSET;
                else if(hdrIndex == e_FM_PCD_HDR_INDEX_LAST)
                        *parseArrayOffset = CC_PC_PR_MPLS_LAST_OFFSET;
                else
                {
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal MPLS header index"));
                    return CC_PC_ILLEGAL;
                }
                break;
            case(HEADER_TYPE_IPv4):
            case(HEADER_TYPE_IPv6):
              if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                    *parseArrayOffset = CC_PC_PR_IP1_OFFSET;
              else if(hdrIndex == e_FM_PCD_HDR_INDEX_2)
                    *parseArrayOffset = CC_PC_PR_IP_LAST_OFFSET;
              else
              {
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IP header index"));
                return CC_PC_ILLEGAL;

              }
                break;
            case(HEADER_TYPE_MINENCAP):
                *parseArrayOffset = CC_PC_PR_MINENC_OFFSET;
                break;
            case(HEADER_TYPE_GRE):
                *parseArrayOffset = CC_PC_PR_GRE_OFFSET;
                break;
            case(HEADER_TYPE_TCP):
            case(HEADER_TYPE_UDP):
            case(HEADER_TYPE_IPSEC_AH):
            case(HEADER_TYPE_IPSEC_ESP):
            case(HEADER_TYPE_DCCP):
            case(HEADER_TYPE_SCTP):
                *parseArrayOffset = CC_PC_PR_L4_OFFSET;
                break;

            default:
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IP header for this type of operation"));
                return CC_PC_ILLEGAL;
     }

        if(offsetRelevant)
            return CC_PR_OFFSET;
        else
            return CC_PR_WITHOUT_OFFSET;
}

static uint8_t GetFieldParseCode(e_NetHeaderType hdr, t_FmPcdFields field, uint32_t offset, uint8_t *parseArrayOffset, e_FmPcdHdrIndex hdrIndex)
{
    bool offsetRelevant = FALSE;

    if(offset)
        offsetRelevant = TRUE;

    switch(hdr)
    {
        case(HEADER_TYPE_NONE):
                ASSERT_COND(FALSE);
        case(HEADER_TYPE_ETH):
            switch(field.eth)
            {
                case(NET_HEADER_FIELD_ETH_TYPE):
                    *parseArrayOffset = CC_PC_PR_ETYPE_LAST_OFFSET;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
            }
            break;
        case(HEADER_TYPE_VLAN):
            switch(field.vlan)
            {
                case(NET_HEADER_FIELD_VLAN_TCI):
                    if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                        *parseArrayOffset = CC_PC_PR_VLAN1_OFFSET;
                    else if(hdrIndex == e_FM_PCD_HDR_INDEX_LAST)
                        *parseArrayOffset = CC_PC_PR_VLAN2_OFFSET;
                    break;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return CC_PC_ILLEGAL;
            }
        break;
        default:
            REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal header "));
            return CC_PC_ILLEGAL;
    }
    if(offsetRelevant)
        return CC_PR_OFFSET;
    else
        return CC_PR_WITHOUT_OFFSET;
}

static void FillAdOfTypeResult(t_Handle p_Ad, t_FmPcd *p_FmPcd, t_FmPcdCcNextEngineParams *p_CcNextEngineParams)
{
    t_AdOfTypeResult                *p_AdResult = (t_AdOfTypeResult*)p_Ad;
    uint32_t                        tmp = 0, tmpNia = 0;
    uint16_t                        profileId;
    t_Handle                        p_AdNewPtr = NULL;

    p_AdNewPtr = p_AdResult;

#ifdef FM_PCD_CC_MANIP
    if (p_CcNextEngineParams->h_Manip)
        FmPcdManipUpdateAdResultForCc(p_CcNextEngineParams->h_Manip, p_Ad, &p_AdNewPtr);
#endif /* FM_PCD_CC_MANIP */

    if(p_AdNewPtr)
    {
        switch(p_CcNextEngineParams->nextEngine)
        {
            case(e_FM_PCD_DONE):
                if(p_CcNextEngineParams->params.enqueueParams.action == e_FM_PCD_ENQ_FRAME)
                {
                    if(p_CcNextEngineParams->params.enqueueParams.overrideFqid)
                    {
                       tmp = FM_PCD_AD_RESULT_CONTRL_FLOW_TYPE;
                       tmp |= p_CcNextEngineParams->params.enqueueParams.newFqid;
                    }
                    else
                    {
                       tmp = FM_PCD_AD_RESULT_DATA_FLOW_TYPE;
                       tmp |= FM_PCD_AD_RESULT_PLCR_DIS;
                    }
                }
            if(p_CcNextEngineParams->params.enqueueParams.action == e_FM_PCD_DROP_FRAME)
                tmpNia |= (NIA_ENG_BMI |NIA_BMI_AC_DISCARD);
            else
                tmpNia |= (NIA_ENG_BMI |NIA_BMI_AC_ENQ_FRAME);
            if(p_CcNextEngineParams->params.enqueueParams.statisticsEn)
                tmpNia |=  FM_PCD_AD_RESULT_EXTENDED_MODE |  FM_PCD_AD_RESULT_STATISTICS_EN;
                break;
            case(e_FM_PCD_KG):
                if(p_CcNextEngineParams->params.kgParams.overrideFqid)
                {
                    tmp = FM_PCD_AD_RESULT_CONTRL_FLOW_TYPE;
                    tmp |= p_CcNextEngineParams->params.kgParams.newFqid;
                }
                else
                {
                    tmp = FM_PCD_AD_RESULT_DATA_FLOW_TYPE;
                    tmp |= FM_PCD_AD_RESULT_PLCR_DIS;
                }
                tmpNia = NIA_KG_DIRECT;
                tmpNia |= NIA_ENG_KG;
                tmpNia |= (uint8_t)(PTR_TO_UINT(p_CcNextEngineParams->params.kgParams.h_DirectScheme)-1);
            if(p_CcNextEngineParams->params.kgParams.statisticsEn)
                tmpNia |=  FM_PCD_AD_RESULT_EXTENDED_MODE |  FM_PCD_AD_RESULT_STATISTICS_EN;
            break;
            case(e_FM_PCD_PLCR):
                tmp = 0;
                if(p_CcNextEngineParams->params.plcrParams.overrideParams)
                {
                    tmp = FM_PCD_AD_RESULT_CONTRL_FLOW_TYPE;

                    /* if private policer profile, it may be uninitialized yet, therefor no checks are done at this stage */
                    if(p_CcNextEngineParams->params.plcrParams.sharedProfile)
                    {
                        tmpNia |= NIA_PLCR_ABSOLUTE;
                        FmPcdPlcrGetAbsoluteProfileId((t_Handle)p_FmPcd,e_FM_PCD_PLCR_SHARED,NULL,p_CcNextEngineParams->params.plcrParams.newRelativeProfileId, &profileId);
                    }
                    else
                        profileId = p_CcNextEngineParams->params.plcrParams.newRelativeProfileId;

                    tmp |= p_CcNextEngineParams->params.plcrParams.newFqid;
                    WRITE_UINT32(p_AdResult->plcrProfile,(uint32_t)((uint32_t)profileId << FM_PCD_AD_PROFILEID_FOR_CNTRL_SHIFT));
                }
                else
                   tmp = FM_PCD_AD_RESULT_DATA_FLOW_TYPE;
                tmpNia |= NIA_ENG_PLCR | p_CcNextEngineParams->params.plcrParams.newRelativeProfileId;
            if(p_CcNextEngineParams->params.kgParams.statisticsEn)
                tmpNia |=  FM_PCD_AD_RESULT_EXTENDED_MODE |  FM_PCD_AD_RESULT_STATISTICS_EN;
               break;
            default:
                return;
        }
        WRITE_UINT32(p_AdResult->fqid, tmp);

#ifdef FM_PCD_CC_MANIP
        if(p_CcNextEngineParams->h_Manip)
        {
            tmp = GET_UINT32(p_AdResult->plcrProfile);
            tmp |= (uint32_t)(XX_VirtToPhys(p_AdNewPtr) - (p_FmPcd->physicalMuramBase)) >> 4;
            WRITE_UINT32(p_AdResult->plcrProfile, tmp);

            tmpNia |= FM_PCD_AD_RESULT_EXTENDED_MODE;
            tmpNia |= FM_PCD_AD_RESULT_NADEN;
        }
#endif /* FM_PCD_CC_MANIP */

        WRITE_UINT32(p_AdResult->nia, tmpNia);
    }
}

static void FillAdOfTypeContLookup(t_Handle p_Ad,  t_Handle h_FmPcd, t_Handle p_FmPcdCcNode, t_Handle h_Manip)
{
    t_FmPcdCcNode           *p_Node = (t_FmPcdCcNode *)p_FmPcdCcNode;
    t_AdOfTypeContLookup    *p_AdContLookup = (t_AdOfTypeContLookup *)p_Ad;
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t                tmpReg32;
    t_Handle                p_AdNewPtr = NULL;

    p_AdNewPtr = p_AdContLookup;

#ifdef FM_PCD_CC_MANIP
    if (h_Manip)
        FmPcdManipUpdateAdContLookupForCc(h_Manip, p_Ad, &p_AdNewPtr, (uint32_t)((XX_VirtToPhys(p_Node->h_AdTable) - p_FmPcd->physicalMuramBase)));
#else
    UNUSED(h_Manip);
#endif /* FM_PCD_CC_MANIP */

    if(p_AdNewPtr)
    {
        tmpReg32 = 0;
        tmpReg32 |= FM_PCD_AD_CONT_LOOKUP_TYPE;
        tmpReg32 |= p_Node->sizeOfExtraction ? ((p_Node->sizeOfExtraction - 1) << 24) : 0;
        tmpReg32 |= (uint32_t)(XX_VirtToPhys(p_Node->h_AdTable) - p_FmPcd->physicalMuramBase);
        WRITE_UINT32(p_AdContLookup->ccAdBase, tmpReg32);

        tmpReg32 = 0;
        tmpReg32 |= p_Node->numOfKeys << 24;
        tmpReg32 |= (p_Node->lclMask ? FM_PCD_AD_CONT_LOOKUP_LCL_MASK : 0);
        tmpReg32 |= p_Node->h_KeysMatchTable ?
                        (uint32_t)(XX_VirtToPhys(p_Node->h_KeysMatchTable) - p_FmPcd->physicalMuramBase) : 0;
        WRITE_UINT32(p_AdContLookup->matchTblPtr, tmpReg32);

        tmpReg32 = 0;
        tmpReg32 |= p_Node->prsArrayOffset << 24;
        tmpReg32 |= p_Node->offset << 16;
        tmpReg32 |= p_Node->parseCode;
        WRITE_UINT32(p_AdContLookup->pcAndOffsets, tmpReg32);

        Mem2IOCpy32((void*)&p_AdContLookup->gmask, p_Node->p_GlblMask, CC_GLBL_MASK_SIZE);
    }
}

static void NextStepAd(t_Handle p_Ad, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams, t_FmPcd *p_FmPcd)
{
    switch(p_FmPcdCcNextEngineParams->nextEngine)
    {
        case(e_FM_PCD_KG):
        case(e_FM_PCD_PLCR):
        case(e_FM_PCD_DONE):
            FillAdOfTypeResult(p_Ad, p_FmPcd, p_FmPcdCcNextEngineParams);
            break;
        case(e_FM_PCD_CC):
            FillAdOfTypeContLookup(p_Ad,
                                   p_FmPcd,
                                   p_FmPcdCcNextEngineParams->params.ccParams.h_CcNode,
#ifdef FM_PCD_CC_MANIP
                                   p_FmPcdCcNextEngineParams->h_Manip
#else
                                   NULL
#endif /* FM_PCD_CC_MANIP */
                                   );
            UpdateNodeOwner (p_FmPcdCcNextEngineParams->params.ccParams.h_CcNode,
                            TRUE);
            break;
         default:
             return;
    }
}


static void ReleaseNewNodeCommonPart(t_FmPcdModifyCcKeyAdditionalParams *p_AdditionalInfo)
{
    if(p_AdditionalInfo->p_AdTableNew)
        FM_MURAM_FreeMem(FmPcdGetMuramHandle(((t_FmPcdCcNode *)(p_AdditionalInfo->h_CurrentNode))->h_FmPcd), p_AdditionalInfo->p_AdTableNew);
    if(p_AdditionalInfo->p_KeysMatchTableNew)
        FM_MURAM_FreeMem(FmPcdGetMuramHandle(((t_FmPcdCcNode *)(p_AdditionalInfo->h_CurrentNode))->h_FmPcd), p_AdditionalInfo->p_KeysMatchTableNew);
}

static t_Error UpdateGblMask(t_FmPcdCcNode *p_FmPcdCcNode, uint8_t keySize, uint8_t *p_Mask)
{
    if (p_Mask &&
        !p_FmPcdCcNode->glblMaskUpdated &&
        (keySize <= 4) &&
        !p_FmPcdCcNode->lclMask )
    {
        memcpy(p_FmPcdCcNode->p_GlblMask, p_Mask, (sizeof(uint8_t))*keySize);
        p_FmPcdCcNode->glblMaskUpdated = TRUE;
        p_FmPcdCcNode->glblMaskSize = 4;
    }
    else if (p_Mask &&
             (keySize <= 4) &&
             !p_FmPcdCcNode->lclMask)
    {
        if (memcmp(p_FmPcdCcNode->p_GlblMask, p_Mask, keySize) != 0)
        {
            p_FmPcdCcNode->lclMask = TRUE;
            p_FmPcdCcNode->glblMaskSize = 0;
        }
    }
    else if (!p_Mask && (p_FmPcdCcNode->glblMaskUpdated) && (keySize <= 4))
    {
        uint32_t tmpMask = 0xffffffff;
        if (memcmp(p_FmPcdCcNode->p_GlblMask, &tmpMask, 4) != 0)
        {
            p_FmPcdCcNode->lclMask = TRUE;
            p_FmPcdCcNode->glblMaskSize = 0;
        }
    }
    else if (p_Mask)
    {
        p_FmPcdCcNode->lclMask = TRUE;
        p_FmPcdCcNode->glblMaskSize = 0;
    }

    return E_OK;
}

static t_Error BuildNewNodeCommonPart(t_FmPcdCcNode                         *p_FmPcdCcNode,
                                      int                                   *size,
                                      t_FmPcdModifyCcKeyAdditionalParams    *p_AdditionalInfo)
{

    p_AdditionalInfo->p_AdTableNew = (t_Handle)FM_MURAM_AllocMem(FmPcdGetMuramHandle(p_FmPcdCcNode->h_FmPcd),
                                     (uint32_t)( (p_AdditionalInfo->numOfKeys+1) * FM_PCD_CC_AD_ENTRY_SIZE),
                                     FM_PCD_CC_AD_TABLE_ALIGN);
    if(!p_AdditionalInfo->p_AdTableNew)
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("No memory in MURAM for AD table "));

    IOMemSet32((uint8_t*)p_AdditionalInfo->p_AdTableNew, 0, (uint32_t)((p_AdditionalInfo->numOfKeys+1) * FM_PCD_CC_AD_ENTRY_SIZE));

    if(p_FmPcdCcNode->lclMask)
        *size = 2 * p_FmPcdCcNode->ccKeySizeAccExtraction;
    else
        *size = p_FmPcdCcNode->ccKeySizeAccExtraction;

    p_AdditionalInfo->p_KeysMatchTableNew =
        (t_Handle)FM_MURAM_AllocMem(FmPcdGetMuramHandle(p_FmPcdCcNode->h_FmPcd),
                                    (uint32_t)(*size * sizeof(uint8_t) * (p_AdditionalInfo->numOfKeys + 1)),
                                    FM_PCD_CC_KEYS_MATCH_TABLE_ALIGN);
    if(!p_AdditionalInfo->p_KeysMatchTableNew)
    {
        FM_MURAM_FreeMem(FmPcdGetMuramHandle(p_FmPcdCcNode->h_FmPcd), p_AdditionalInfo->p_AdTableNew);
        p_AdditionalInfo->p_AdTableNew = NULL;
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("No memory in MURAM for KEY MATCH table"));
    }
    IOMemSet32((uint8_t*)p_AdditionalInfo->p_KeysMatchTableNew, 0, *size * sizeof(uint8_t) * (p_AdditionalInfo->numOfKeys + 1));

    p_AdditionalInfo->p_AdTableOld          = p_FmPcdCcNode->h_AdTable;
    p_AdditionalInfo->p_KeysMatchTableOld   = p_FmPcdCcNode->h_KeysMatchTable;

    return E_OK;
}

static t_Error BuildNewNodeAddOrMdfyKeyAndNextEngine(t_Handle h_FmPcd ,t_FmPcdCcNode *p_FmPcdCcNode, uint8_t keyIndex, t_FmPcdCcKeyParams  *p_KeyParams,t_FmPcdModifyCcKeyAdditionalParams *p_AdditionalInfo, bool add)
{
    t_Error                 err = E_OK;
    t_Handle                p_AdTableNewTmp, p_KeysMatchTableNewTmp;
    t_Handle                p_KeysMatchTableOldTmp, p_AdTableOldTmp;
    int                     size;
    int                     i = 0, j = 0;
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t                requiredAction = 0;
    bool                    prvLclMask;
    t_CcNodeInformation     *p_CcNodeInformation;
    t_List                  *p_Pos;

    /*check that new NIA is legal*/
    err = ValidateNextEngineParams(h_FmPcd, &p_KeyParams->ccNextEngineParams);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    prvLclMask = p_FmPcdCcNode->lclMask;

    /*check that new key is not require update of localMask*/
    err = UpdateGblMask(p_FmPcdCcNode,
                        p_FmPcdCcNode->ccKeySizeAccExtraction,
                        p_KeyParams->p_Mask);
    if (err != E_OK)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    /*update internal data structure for next engine per index (index - key)*/
    memcpy(&p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].nextEngineParams,&p_KeyParams->ccNextEngineParams, sizeof(t_FmPcdCcNextEngineParams));

    /*update numOfKeys*/
    if(add)
        p_AdditionalInfo->numOfKeys = (uint8_t)(p_FmPcdCcNode->numOfKeys + 1);
    else
        p_AdditionalInfo->numOfKeys = (uint8_t)p_FmPcdCcNode->numOfKeys;
    /*function which build in the memory new KeyTbl, AdTbl*/
    err = BuildNewNodeCommonPart(p_FmPcdCcNode, &size, p_AdditionalInfo);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

#ifdef FM_PCD_CC_MANIP
    /*check that manip is legal and what requiredAction is necessary for this manip*/
    if(p_KeyParams->ccNextEngineParams.h_Manip)
    {
        err = FmPcdManipCheckParamsForCcNextEgine(&p_KeyParams->ccNextEngineParams,&requiredAction);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));

    }
#endif /* FM_PCD_CC_MANIP */

    p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].requiredAction = requiredAction;

    p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].requiredAction |= UPDATE_CC_WITH_TREE;


    /*update new Ad and new Key Table according to new requirement*/
    i = 0;
    for(j = 0; j < p_AdditionalInfo->numOfKeys; j++)
    {
        p_AdTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableNew, j*FM_PCD_CC_AD_ENTRY_SIZE);
        if(j == keyIndex)
         {
            NextStepAd(p_AdTableNewTmp,&p_KeyParams->ccNextEngineParams, p_FmPcd);
            p_KeysMatchTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_KeysMatchTableNew, j*size * sizeof(uint8_t));
            Mem2IOCpy32((void*)p_KeysMatchTableNewTmp, p_KeyParams->p_Key, p_FmPcdCcNode->userSizeOfExtraction);
            if(p_FmPcdCcNode->lclMask)
            {
                if(p_KeyParams->p_Mask)
                    Mem2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), p_KeyParams->p_Mask, p_FmPcdCcNode->userSizeOfExtraction);
                else if (p_FmPcdCcNode->ccKeySizeAccExtraction > 4)
                    IOMemSet32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), 0xff, p_FmPcdCcNode->userSizeOfExtraction);
                else
                    Mem2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction),p_FmPcdCcNode->p_GlblMask, p_FmPcdCcNode->userSizeOfExtraction);
            }
            if(!add)
                i++;
         }
         else
         {
            p_AdTableOldTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableOld, i*FM_PCD_CC_AD_ENTRY_SIZE);
            IO2IOCpy32(p_AdTableNewTmp, p_AdTableOldTmp,  FM_PCD_CC_AD_ENTRY_SIZE);
            p_KeysMatchTableNewTmp  = PTR_MOVE(p_AdditionalInfo->p_KeysMatchTableNew, j*size * sizeof(uint8_t));
            p_KeysMatchTableOldTmp  = PTR_MOVE(p_AdditionalInfo->p_KeysMatchTableOld, i*size * sizeof(uint8_t));

            if(p_FmPcdCcNode->lclMask)
            {
                if(prvLclMask)
                    IO2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction),
                               PTR_MOVE(p_KeysMatchTableOldTmp, p_FmPcdCcNode->ccKeySizeAccExtraction),
                               p_FmPcdCcNode->ccKeySizeAccExtraction);
                else
                {
                    p_KeysMatchTableOldTmp  = PTR_MOVE(p_FmPcdCcNode->h_KeysMatchTable, i*p_FmPcdCcNode->ccKeySizeAccExtraction*sizeof(uint8_t));

                    if (p_FmPcdCcNode->ccKeySizeAccExtraction > 4)
                        IOMemSet32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), 0xff, p_FmPcdCcNode->userSizeOfExtraction);
                    else
                        IO2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), p_FmPcdCcNode->p_GlblMask, p_FmPcdCcNode->userSizeOfExtraction);
                }
            }
            IO2IOCpy32(p_KeysMatchTableNewTmp, p_KeysMatchTableOldTmp, p_FmPcdCcNode->ccKeySizeAccExtraction);
           i++;
         }
    }

    p_AdTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableNew, j*FM_PCD_CC_AD_ENTRY_SIZE);
    p_AdTableOldTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableOld, i*FM_PCD_CC_AD_ENTRY_SIZE);
    IO2IOCpy32(p_AdTableNewTmp, p_AdTableOldTmp, FM_PCD_CC_AD_ENTRY_SIZE);


    if(!LIST_IsEmpty(&p_FmPcdCcNode->ccTreesLst))
    {
        LIST_FOR_EACH(p_Pos, &p_FmPcdCcNode->ccTreesLst)
        {
            p_CcNodeInformation = CC_NODE_F_OBJECT(p_Pos);
            ASSERT_COND(p_CcNodeInformation->h_CcNode);
            /*update the manipulation which has to be updated from parameters of the port*/
            /*it's has to be updated with restrictions defined in the function*/
                err = FmPcdCcSetRequiredAction(p_FmPcdCcNode->h_FmPcd,
                                               p_FmPcdCcNode->shadowAction | p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].requiredAction,
                                               &p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex],
                                               PTR_MOVE(p_AdditionalInfo->p_AdTableNew, keyIndex*FM_PCD_CC_AD_ENTRY_SIZE),
                                               1,
                                               p_CcNodeInformation->h_CcNode);
                if (err)
                    RETURN_ERROR(MAJOR, err, (NO_MSG));

                err = CcUpdateParam(p_FmPcdCcNode->h_FmPcd,
                                    NULL,
                                    &p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex],
                                    1,
                                    PTR_MOVE(p_AdditionalInfo->p_AdTableNew, keyIndex*FM_PCD_CC_AD_ENTRY_SIZE),
                                    TRUE,
                                    p_CcNodeInformation->index,
                                    p_CcNodeInformation->h_CcNode,
                                    TRUE);
                if (err)
                    RETURN_ERROR(MAJOR, err, (NO_MSG));
        }
   }

    if(p_FmPcdCcNode->lclMask)
        memset(p_FmPcdCcNode->p_GlblMask, 0xff, CC_GLBL_MASK_SIZE * sizeof(uint8_t));


    if(p_KeyParams->ccNextEngineParams.nextEngine == e_FM_PCD_CC)
        p_AdditionalInfo->h_NodeForAdd = p_KeyParams->ccNextEngineParams.params.ccParams.h_CcNode;

    if(!add)
    {
        if(p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_CC)
            p_AdditionalInfo->h_NodeForRmv = p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.params.ccParams.h_CcNode;
#ifdef FM_PCD_CC_MANIP
        if(p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip)
            p_AdditionalInfo->h_ManipForRmv = p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip;
#endif /* FM_PCD_CC_MANIP */
    }

    return E_OK;
}

static t_Error BuildNewNodeRemoveKey(t_FmPcdCcNode *p_FmPcdCcNode, uint8_t keyIndex, t_FmPcdModifyCcKeyAdditionalParams *p_AdditionalInfo)
{
    int         i = 0, j = 0;
    t_Handle    p_AdTableNewTmp,p_KeysMatchTableNewTmp;
    t_Handle    p_KeysMatchTableOldTmp, p_AdTableOldTmp;
    int         size;
    t_Error     err = E_OK;

    /*save new numOfKeys*/
    p_AdditionalInfo->numOfKeys = (uint16_t)(p_FmPcdCcNode->numOfKeys - 1);

    /*function which allocates in the memory new KeyTbl, AdTbl*/
    err = BuildNewNodeCommonPart(p_FmPcdCcNode, &size, p_AdditionalInfo);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    /*update new Ad and new Key Table according to new requirement*/
    for(i = 0, j = 0; j < p_FmPcdCcNode->numOfKeys; i++, j++)
    {
        if(j == keyIndex)
        {
            p_AdTableOldTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableOld, j*FM_PCD_CC_AD_ENTRY_SIZE);
            j++;
        }
        if(j == p_FmPcdCcNode->numOfKeys)
            break;
         p_AdTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableNew, i*FM_PCD_CC_AD_ENTRY_SIZE);
         p_AdTableOldTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableOld, j*FM_PCD_CC_AD_ENTRY_SIZE);
         IO2IOCpy32(p_AdTableNewTmp,p_AdTableOldTmp,  FM_PCD_CC_AD_ENTRY_SIZE);
         p_KeysMatchTableOldTmp = PTR_MOVE(p_AdditionalInfo->p_KeysMatchTableOld, j*size * sizeof(uint8_t));
         p_KeysMatchTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_KeysMatchTableNew, i*size * sizeof(uint8_t));
         IO2IOCpy32(p_KeysMatchTableNewTmp,p_KeysMatchTableOldTmp,  size * sizeof(uint8_t));
    }

    p_AdTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableNew, i*FM_PCD_CC_AD_ENTRY_SIZE);
    p_AdTableOldTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableOld, j*FM_PCD_CC_AD_ENTRY_SIZE);
    IO2IOCpy32(p_AdTableNewTmp, p_AdTableOldTmp,  FM_PCD_CC_AD_ENTRY_SIZE);

    if(p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_CC)
        p_AdditionalInfo->h_NodeForRmv = p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.params.ccParams.h_CcNode;
#ifdef FM_PCD_CC_MANIP
    if(p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip)
        p_AdditionalInfo->h_ManipForRmv = p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip;
#endif /* FM_PCD_CC_MANIP */

   return E_OK;
}

static t_Error BuildNewNodeModifyKey(t_FmPcdCcNode *p_FmPcdCcNode, uint8_t keyIndex, uint8_t  *p_Key, uint8_t *p_Mask,t_FmPcdModifyCcKeyAdditionalParams *p_AdditionalInfo)
{
    t_Error                 err = E_OK;
    t_Handle                p_AdTableNewTmp, p_KeysMatchTableNewTmp;
    t_Handle                p_KeysMatchTableOldTmp, p_AdTableOldTmp;
    int                     size;
    int                     i = 0, j = 0;
    bool                    prvLclMask;

    p_AdditionalInfo->numOfKeys =  p_FmPcdCcNode->numOfKeys;

    prvLclMask = p_FmPcdCcNode->lclMask;

    /*check that new key is not require update of localMask*/
    err = UpdateGblMask(p_FmPcdCcNode,
                        p_FmPcdCcNode->sizeOfExtraction,
                        p_Mask);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    /*function which build in the memory new KeyTbl, AdTbl*/
    err = BuildNewNodeCommonPart(p_FmPcdCcNode, &size,  p_AdditionalInfo);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    /*fill the New AdTable and New KeyTable*/
    for(j = 0, i = 0; j < p_AdditionalInfo->numOfKeys; j++, i++)
    {
        p_AdTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableNew, j*FM_PCD_CC_AD_ENTRY_SIZE);
        p_AdTableOldTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableOld, i*FM_PCD_CC_AD_ENTRY_SIZE);
        IO2IOCpy32(p_AdTableNewTmp, p_AdTableOldTmp,  FM_PCD_CC_AD_ENTRY_SIZE);
        if(j == keyIndex)
        {
            p_KeysMatchTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_KeysMatchTableNew, j*size * sizeof(uint8_t));
            Mem2IOCpy32(p_KeysMatchTableNewTmp, p_Key, p_FmPcdCcNode->userSizeOfExtraction);
            if(p_FmPcdCcNode->lclMask)
            {
                if(p_Mask)
                    Mem2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), p_Mask, p_FmPcdCcNode->userSizeOfExtraction);
                else if (p_FmPcdCcNode->ccKeySizeAccExtraction > 4)
                    IOMemSet32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), 0xff, p_FmPcdCcNode->userSizeOfExtraction);
                else
                    Mem2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction),p_FmPcdCcNode->p_GlblMask, p_FmPcdCcNode->userSizeOfExtraction);
            }
        }
        else
        {
            p_KeysMatchTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_KeysMatchTableNew, j*size * sizeof(uint8_t));
            p_KeysMatchTableOldTmp = PTR_MOVE(p_FmPcdCcNode->h_KeysMatchTable, i*size * sizeof(uint8_t));
            if (p_FmPcdCcNode->lclMask)
            {
                if(prvLclMask)
                    IO2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction),
                               PTR_MOVE(p_KeysMatchTableOldTmp, p_FmPcdCcNode->ccKeySizeAccExtraction),
                               p_FmPcdCcNode->userSizeOfExtraction);
                else
                {
                    p_KeysMatchTableOldTmp = PTR_MOVE(p_FmPcdCcNode->h_KeysMatchTable, i*p_FmPcdCcNode->ccKeySizeAccExtraction * sizeof(uint8_t));

                    if (p_FmPcdCcNode->ccKeySizeAccExtraction > 4)
                        IOMemSet32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), 0xff, p_FmPcdCcNode->userSizeOfExtraction);
                    else
                        IO2IOCpy32(PTR_MOVE(p_KeysMatchTableNewTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), p_FmPcdCcNode->p_GlblMask, p_FmPcdCcNode->userSizeOfExtraction);
                }
            }
            IO2IOCpy32((void*)p_KeysMatchTableNewTmp, p_KeysMatchTableOldTmp, p_FmPcdCcNode->ccKeySizeAccExtraction);
        }
    }

    p_AdTableNewTmp = PTR_MOVE(p_AdditionalInfo->p_AdTableNew, j*FM_PCD_CC_AD_ENTRY_SIZE);
    p_AdTableOldTmp = PTR_MOVE(p_FmPcdCcNode->h_AdTable, i*FM_PCD_CC_AD_ENTRY_SIZE);
    IO2IOCpy32(p_AdTableNewTmp, p_AdTableOldTmp, FM_PCD_CC_AD_ENTRY_SIZE);

    return E_OK;
}

static t_Error BuildNewNodeModifyNextEngine(t_Handle h_FmPcd ,t_Handle h_FmPcdCcNodeOrTree, uint16_t keyIndex,t_FmPcdCcNextEngineParams   *p_CcNextEngineParams, t_List *h_OldLst, t_List *h_NewLst,t_FmPcdModifyCcKeyAdditionalParams *p_AdditionalInfo)
{

    t_Error      err = E_OK;
    uint32_t     requiredAction = 0;
    t_List       *p_Pos;
    t_CcNodeInformation *p_CcNodeInformation, ccNodeInfo;
    t_Handle     p_Ad;
    t_FmPcdCcNode *p_FmPcdCcNode1 = NULL;
    t_FmPcdCcTree *p_FmPcdCcTree = NULL;

    ASSERT_COND(p_CcNextEngineParams);
    /*check that new NIA is legal*/
    err = ValidateNextEngineParams(h_FmPcd, p_CcNextEngineParams);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    /*update internal data structure for next engine per index (index - key)*/
    memcpy(&p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].nextEngineParams,p_CcNextEngineParams, sizeof(t_FmPcdCcNextEngineParams));

#ifdef FM_PCD_CC_MANIP
    /*check that manip is legal and what requiredAction is necessary for this manip*/
    if(p_CcNextEngineParams->h_Manip)
    {
        err = FmPcdManipCheckParamsForCcNextEgine(p_CcNextEngineParams,&requiredAction);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));

    }
#endif /* FM_PCD_CC_MANIP */

    if(!p_AdditionalInfo->tree)
    {
        p_FmPcdCcNode1 = (t_FmPcdCcNode *)h_FmPcdCcNodeOrTree;
        p_Ad = p_FmPcdCcNode1->h_AdTable;
        if(p_FmPcdCcNode1->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_CC)
            p_AdditionalInfo->h_NodeForRmv = p_FmPcdCcNode1->nextEngineAndRequiredAction[keyIndex].nextEngineParams.params.ccParams.h_CcNode;
#ifdef FM_PCD_CC_MANIP
        if(p_FmPcdCcNode1->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip)
            p_AdditionalInfo->h_ManipForRmv = p_FmPcdCcNode1->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip;
#endif /* FM_PCD_CC_MANIP */
    }
    else
    {
        p_FmPcdCcTree = (t_FmPcdCcTree *)h_FmPcdCcNodeOrTree;
        p_Ad = UINT_TO_PTR(p_FmPcdCcTree->ccTreeBaseAddr);
        if(p_FmPcdCcTree->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_CC)
            p_AdditionalInfo->h_NodeForRmv = p_FmPcdCcTree->nextEngineAndRequiredAction[keyIndex].nextEngineParams.params.ccParams.h_CcNode;
#ifdef FM_PCD_CC_MANIP
        if(p_FmPcdCcTree->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip)
            p_AdditionalInfo->h_ManipForRmv = p_FmPcdCcTree->nextEngineAndRequiredAction[keyIndex].nextEngineParams.h_Manip;
#endif /* FM_PCD_CC_MANIP */
    }
    ASSERT_COND(p_Ad);
    memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
    ccNodeInfo.h_CcNode = PTR_MOVE(p_Ad, keyIndex * FM_PCD_CC_AD_ENTRY_SIZE);
    EnqueueNodeInfoToRelevantLst(h_OldLst, &ccNodeInfo);

    memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
    p_Ad = (t_Handle)FM_MURAM_AllocMem(FmPcdGetMuramHandle(h_FmPcd),
                                         FM_PCD_CC_AD_ENTRY_SIZE,
                                         FM_PCD_CC_AD_TABLE_ALIGN);

    if(!p_Ad)
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("Memory allocation in MURAM FAILED"));

    IOMemSet32((uint8_t *)p_Ad, 0,  FM_PCD_CC_AD_ENTRY_SIZE);
    if(p_CcNextEngineParams)
        NextStepAd(p_Ad,p_CcNextEngineParams, h_FmPcd);
    ccNodeInfo.h_CcNode = p_Ad;
    EnqueueNodeInfoToRelevantLst(h_NewLst, &ccNodeInfo);

    p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].requiredAction = requiredAction;

    p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].requiredAction |= UPDATE_CC_WITH_TREE;

    if(!p_AdditionalInfo->tree)
    {
        ASSERT_COND(p_FmPcdCcNode1);
        if(!LIST_IsEmpty(&p_FmPcdCcNode1->ccTreesLst))
        {
            LIST_FOR_EACH(p_Pos, &p_FmPcdCcNode1->ccTreesLst)
            {
                p_CcNodeInformation = CC_NODE_F_OBJECT(p_Pos);
                ASSERT_COND(p_CcNodeInformation->h_CcNode);
                /*update the manipulation which has to be updated from parameters of the port*/
                /*it's has to be updated with restrictions defined in the function*/
                    err = FmPcdCcSetRequiredAction(p_FmPcdCcNode1->h_FmPcd, p_FmPcdCcNode1->shadowAction | p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].requiredAction, &p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex],
                                                   p_Ad, 1, p_CcNodeInformation->h_CcNode);
                    if(err)
                        RETURN_ERROR(MAJOR, err, (NO_MSG));
                     err = CcUpdateParam(p_FmPcdCcNode1->h_FmPcd, NULL, &p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex],1, p_Ad, TRUE, p_CcNodeInformation->index, p_CcNodeInformation->h_CcNode, TRUE);
                    if(err)
                        RETURN_ERROR(MAJOR, err, (NO_MSG));
            }
        }
    }
    else
    {
       ASSERT_COND(p_FmPcdCcTree);
       err = FmPcdCcSetRequiredAction(h_FmPcd, p_FmPcdCcTree->requiredAction | p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex].requiredAction, &p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex],
                                       p_Ad, 1, (t_Handle)p_FmPcdCcTree);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));
         err = CcUpdateParam(h_FmPcd, NULL, &p_AdditionalInfo->nextEngineAndRequiredAction[keyIndex],1, p_Ad, TRUE, 0, (t_Handle)p_FmPcdCcTree, TRUE);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));
    }

    if(p_CcNextEngineParams->nextEngine == e_FM_PCD_CC)
        p_AdditionalInfo->h_NodeForAdd = p_CcNextEngineParams->params.ccParams.h_CcNode;
    return E_OK;
}

static t_Handle BuildNewAd(t_FmPcdModifyCcKeyAdditionalParams   *p_FmPcdModifyCcKeyAdditionalParams,
                           t_FmPcdCcNode                        *p_FmPcdCcNode,
                           t_FmPcdCcNextEngineParams            *p_FmPcdCcNextEngineParams)
{

    t_Handle        p_Ad;
    t_FmPcdCcNode   *p_FmPcdCcNodeTmp;

    p_Ad = (t_Handle)FM_MURAM_AllocMem(((t_FmPcd *)(p_FmPcdCcNode->h_FmPcd))->h_FmMuram,
                                         FM_PCD_CC_AD_ENTRY_SIZE,
                                         FM_PCD_CC_AD_TABLE_ALIGN);
    if(!p_Ad)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("MURAM for AD"));
        return NULL;
    }
    IOMemSet32(p_Ad, 0,  FM_PCD_CC_AD_ENTRY_SIZE);

    p_FmPcdCcNodeTmp = (t_FmPcdCcNode*)XX_Malloc(sizeof(t_FmPcdCcNode));
    if(!p_FmPcdCcNodeTmp)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("p_FmPcdCcNodeTmp"));
        return NULL;
    }
    memset(p_FmPcdCcNodeTmp, 0, sizeof(t_FmPcdCcNode));

    p_FmPcdCcNodeTmp->numOfKeys = p_FmPcdModifyCcKeyAdditionalParams->numOfKeys;
    p_FmPcdCcNodeTmp->h_KeysMatchTable = p_FmPcdModifyCcKeyAdditionalParams->p_KeysMatchTableNew;
    p_FmPcdCcNodeTmp->h_AdTable = p_FmPcdModifyCcKeyAdditionalParams->p_AdTableNew;

    p_FmPcdCcNodeTmp->lclMask = p_FmPcdCcNode->lclMask;
    p_FmPcdCcNodeTmp->parseCode = p_FmPcdCcNode->parseCode;
    p_FmPcdCcNodeTmp->offset = p_FmPcdCcNode->offset;
    p_FmPcdCcNodeTmp->prsArrayOffset = p_FmPcdCcNode->prsArrayOffset;
    p_FmPcdCcNodeTmp->ctrlFlow = p_FmPcdCcNode->ctrlFlow;
    p_FmPcdCcNodeTmp->ccKeySizeAccExtraction = p_FmPcdCcNode->ccKeySizeAccExtraction;
    p_FmPcdCcNodeTmp->sizeOfExtraction = p_FmPcdCcNode->sizeOfExtraction;
    p_FmPcdCcNodeTmp->glblMaskSize = p_FmPcdCcNode->glblMaskSize;
    p_FmPcdCcNodeTmp->p_GlblMask = p_FmPcdCcNode->p_GlblMask;

    if (p_FmPcdCcNextEngineParams->nextEngine == e_FM_PCD_CC)
        FillAdOfTypeContLookup(p_Ad,
                               p_FmPcdCcNode->h_FmPcd,
                               p_FmPcdCcNodeTmp,
#ifdef FM_PCD_CC_MANIP
                               p_FmPcdCcNextEngineParams->h_Manip
#else
                               NULL
#endif /* FM_PCD_CC_MANIP */
                               );

    XX_Free(p_FmPcdCcNodeTmp);

    return p_Ad;
}

static void UpdateAdPtrOfNodesWhichPointsOnCrntMdfNode(t_FmPcdCcNode *p_CrntMdfNode ,t_FmPcdModifyCcKeyAdditionalParams *p_FmPcdModifyCcKeyAdditionalParams, t_List  *h_OldLst, t_List  *h_NewLst)
{
    t_CcNodeInformation     *p_CcNodeInformation;
    t_FmPcdCcNode           *p_NodePtrOnCurrentMdfNode = NULL;
    t_List                  *p_Pos;
    int                     i = 0;
    t_Handle                p_AdTablePtOnCrntCurrentMdfNode, p_AdTableNewModified;
    t_CcNodeInformation     ccNodeInfo;

    LIST_FOR_EACH(p_Pos, &p_CrntMdfNode->ccPrevNodesLst)
    {
        p_CcNodeInformation = CC_NODE_F_OBJECT(p_Pos);
        p_NodePtrOnCurrentMdfNode = (t_FmPcdCcNode *)p_CcNodeInformation->h_CcNode;
        ASSERT_COND(p_NodePtrOnCurrentMdfNode);
        /*search in the prev node which exact index points on this current modified node for getting AD */
        for(i = 0; i < p_NodePtrOnCurrentMdfNode->numOfKeys + 1; i++)
        {
            if(p_NodePtrOnCurrentMdfNode->nextEngineAndRequiredAction[i].nextEngineParams.nextEngine == e_FM_PCD_CC)
            {
                if(p_NodePtrOnCurrentMdfNode->nextEngineAndRequiredAction[i].nextEngineParams.params.ccParams.h_CcNode == (t_Handle)p_CrntMdfNode)
                {
                    p_AdTablePtOnCrntCurrentMdfNode = PTR_MOVE(p_NodePtrOnCurrentMdfNode->h_AdTable, i*FM_PCD_CC_AD_ENTRY_SIZE);
                    memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
                    ccNodeInfo.h_CcNode = p_AdTablePtOnCrntCurrentMdfNode;
                    EnqueueNodeInfoToRelevantLst(h_OldLst, &ccNodeInfo);

                    p_AdTableNewModified = BuildNewAd(p_FmPcdModifyCcKeyAdditionalParams, p_CrntMdfNode, &p_NodePtrOnCurrentMdfNode->nextEngineAndRequiredAction[i].nextEngineParams);
                    memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
                    ccNodeInfo.h_CcNode = p_AdTableNewModified;
                    EnqueueNodeInfoToRelevantLst(h_NewLst, &ccNodeInfo);
                }
            }
        }
        ASSERT_COND(i != p_NodePtrOnCurrentMdfNode->numOfKeys);
    }
}

static void UpdateAdPtrOfTreesWhichPointsOnCrntMdfNode(t_FmPcdCcNode *p_CrntMdfNode ,t_FmPcdModifyCcKeyAdditionalParams *p_FmPcdModifyCcKeyAdditionalParams, t_List  *h_OldLst, t_List  *h_NewLst)
{
    t_CcNodeInformation     *p_CcNodeInformation;
    t_FmPcdCcTree           *p_TreePtrOnCurrentMdfNode = NULL;
    t_List                  *p_Pos;
    int                     i = 0;
    t_Handle                p_AdTableTmp, p_AdTableTmp1;
    t_CcNodeInformation     ccNodeInfo;

    LIST_FOR_EACH(p_Pos, &p_CrntMdfNode->ccTreeIdLst)
    {
        p_CcNodeInformation = CC_NODE_F_OBJECT(p_Pos);
        p_TreePtrOnCurrentMdfNode = (t_FmPcdCcTree *)p_CcNodeInformation->h_CcNode;

        ASSERT_COND(p_TreePtrOnCurrentMdfNode);
        /*search in the trees which exact index points on this current modified node for getting AD
        */
        for(i = 0; i < p_TreePtrOnCurrentMdfNode->numOfEntries; i++)
        {
            if(p_TreePtrOnCurrentMdfNode->nextEngineAndRequiredAction[i].nextEngineParams.nextEngine == e_FM_PCD_CC)
            {
                if(p_TreePtrOnCurrentMdfNode->nextEngineAndRequiredAction[i].nextEngineParams.params.ccParams.h_CcNode == (t_Handle)p_CrntMdfNode)
                {
                    p_AdTableTmp = UINT_TO_PTR(p_TreePtrOnCurrentMdfNode->ccTreeBaseAddr + i*FM_PCD_CC_AD_ENTRY_SIZE);
                    memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
                    ccNodeInfo.h_CcNode = p_AdTableTmp;
                    EnqueueNodeInfoToRelevantLst(h_OldLst, &ccNodeInfo);

                    p_AdTableTmp1 = BuildNewAd(p_FmPcdModifyCcKeyAdditionalParams, p_CrntMdfNode, &p_TreePtrOnCurrentMdfNode->nextEngineAndRequiredAction[i].nextEngineParams);
                    memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
                    ccNodeInfo.h_CcNode = p_AdTableTmp1;
                    EnqueueNodeInfoToRelevantLst(h_NewLst, &ccNodeInfo);
                }
        }
    }
        ASSERT_COND(i == p_TreePtrOnCurrentMdfNode->numOfEntries);
     }
}

static t_Error ModifyKeyCommonPart1(t_Handle h_FmPcdCcNodeOrTree,  uint16_t keyIndex, t_Handle *h_Params, e_ModifyState modifyState, bool check, bool tree)
{
    t_FmPcdModifyCcKeyAdditionalParams          *p_FmPcdModifyCcKeyAdditionalParams;
    int                                         i = 0, j = 0;
    bool                                        wasUpdate = FALSE;
    t_FmPcdCcNode                               *p_FmPcdCcNode = NULL;
    t_FmPcdCcTree                               *p_FmPcdCcTree;
    uint16_t                                    numOfKeys;
    t_FmPcdCcNextEngineAndRequiredActionParams  *p_nextEngineAndRequiredAction = NULL;

    SANITY_CHECK_RETURN_ERROR(h_FmPcdCcNodeOrTree,E_INVALID_HANDLE);

    p_nextEngineAndRequiredAction = XX_Malloc(FM_PCD_MAX_NUM_OF_KEYS * sizeof(*p_nextEngineAndRequiredAction));
    if(!p_nextEngineAndRequiredAction)
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("failed to allocate memory for p_nextEngineAndRequiredAction"));

    memset(p_nextEngineAndRequiredAction, 0, FM_PCD_MAX_NUM_OF_KEYS * sizeof(*p_nextEngineAndRequiredAction));

    if(!tree)
    {
        p_FmPcdCcNode = (t_FmPcdCcNode *)h_FmPcdCcNodeOrTree;
        numOfKeys = p_FmPcdCcNode->numOfKeys;

        /*node has to be pointed by another node or tree*/
        if (!LIST_NumOfObjs(&p_FmPcdCcNode->ccPrevNodesLst) &&
            !LIST_NumOfObjs(&p_FmPcdCcNode->ccTreeIdLst))
        {
            XX_Free(p_nextEngineAndRequiredAction);
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("node has to be pointed by node or tree"));
        }

        if(!LIST_NumOfObjs(&p_FmPcdCcNode->ccTreesLst) ||
            (LIST_NumOfObjs(&p_FmPcdCcNode->ccTreesLst) != 1))
        {
            XX_Free(p_nextEngineAndRequiredAction);
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("node has to be belonging to some tree and only to one tree"));
        }

        memcpy(p_nextEngineAndRequiredAction,
               p_FmPcdCcNode->nextEngineAndRequiredAction,
               FM_PCD_MAX_NUM_OF_KEYS * sizeof(t_FmPcdCcNextEngineAndRequiredActionParams));

        if(check)
        {
            if((p_FmPcdCcNode->parseCode == CC_PC_FF_IPV4TTL) ||
               (p_FmPcdCcNode->parseCode == CC_PC_FF_IPV6HOP_LIMIT) ||
               (p_FmPcdCcNode->parseCode == CC_PC_GENERIC_IC_HASH_INDEXED))
            {
                XX_Free(p_nextEngineAndRequiredAction);
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("nodeId of CC_PC_FF_IPV4TTL or CC_PC_FF_IPV6HOP_LIMIT can not be used for addKey, removeKey, modifyKey"));
            }
        }
    }
    else
    {
        p_FmPcdCcTree = (t_FmPcdCcTree *)h_FmPcdCcNodeOrTree;
        numOfKeys = p_FmPcdCcTree->numOfEntries;
        memcpy(p_nextEngineAndRequiredAction,
               p_FmPcdCcTree->nextEngineAndRequiredAction,
               FM_PCD_MAX_NUM_OF_KEYS * sizeof(t_FmPcdCcNextEngineAndRequiredActionParams));
    }

    p_FmPcdModifyCcKeyAdditionalParams =
        (t_FmPcdModifyCcKeyAdditionalParams *)XX_Malloc(sizeof(t_FmPcdModifyCcKeyAdditionalParams));
    if(!p_FmPcdModifyCcKeyAdditionalParams)
    {
        XX_Free(p_nextEngineAndRequiredAction);
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("Allocation of internal data structure FAILED"));
    }
    memset(p_FmPcdModifyCcKeyAdditionalParams, 0, sizeof(t_FmPcdModifyCcKeyAdditionalParams));

    p_FmPcdModifyCcKeyAdditionalParams->h_CurrentNode = h_FmPcdCcNodeOrTree;
    p_FmPcdModifyCcKeyAdditionalParams->keyIndex = keyIndex;

    while(i < numOfKeys)
    {
        if((j == keyIndex) && !wasUpdate)
        {
            if(modifyState == e_MODIFY_STATE_ADD)
                j++;
            else if(modifyState == e_MODIFY_STATE_REMOVE)
                i++;
            wasUpdate = TRUE;
        }
        else
        {
            memcpy(&p_FmPcdModifyCcKeyAdditionalParams->nextEngineAndRequiredAction[j], &p_nextEngineAndRequiredAction[i], sizeof(t_FmPcdCcNextEngineAndRequiredActionParams));
            i++;
            j++;
        }
    }

    if (keyIndex == numOfKeys)
    {
        if (modifyState == e_MODIFY_STATE_ADD)
            j++;
        else if(modifyState == e_MODIFY_STATE_REMOVE)
            i++;
    }

    memcpy(&p_FmPcdModifyCcKeyAdditionalParams->nextEngineAndRequiredAction[j], &p_nextEngineAndRequiredAction[numOfKeys], sizeof(t_FmPcdCcNextEngineAndRequiredActionParams));

    XX_Free(p_nextEngineAndRequiredAction);
    *h_Params = p_FmPcdModifyCcKeyAdditionalParams;

    return E_OK;
}

static t_Error UpdatePtrWhichPointOnCrntMdfNode(t_FmPcdCcNode *p_FmPcdCcNode, t_FmPcdModifyCcKeyAdditionalParams *p_FmPcdModifyCcKeyAdditionalParams ,t_List *h_OldLst, t_List *h_NewLst)
{
    if(!LIST_IsEmpty(&p_FmPcdCcNode->ccPrevNodesLst))
        UpdateAdPtrOfNodesWhichPointsOnCrntMdfNode(p_FmPcdCcNode, p_FmPcdModifyCcKeyAdditionalParams, h_OldLst, h_NewLst);

    if(!LIST_IsEmpty(&p_FmPcdCcNode->ccTreeIdLst))
        UpdateAdPtrOfTreesWhichPointsOnCrntMdfNode(p_FmPcdCcNode, p_FmPcdModifyCcKeyAdditionalParams, h_OldLst, h_NewLst);

    return E_OK;
}

static void  FmPcdCcUpdateTreeOwner(t_FmPcdCcTree *p_FmPcdCcTree, bool add)
{
    ASSERT_COND(p_FmPcdCcTree);

    if(add)
        p_FmPcdCcTree->owners++;
    else
    {
        ASSERT_COND(p_FmPcdCcTree->owners);
        p_FmPcdCcTree->owners--;
    }
}

#ifdef FM_PCD_CC_MANIP
static t_Error CheckAndSetManipParamsWithCcNodeParams(t_FmPcdCcNode *p_FmPcdCcNode)
{
    t_Error err = E_OK;
    int     i = 0;

    for(i = 0; i < p_FmPcdCcNode->numOfKeys; i++)
    {
        if(p_FmPcdCcNode->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip)
        {
            err = FmPcdManipCheckParamsWithCcNodeParams(p_FmPcdCcNode->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip, (t_Handle)p_FmPcdCcNode);
            if(err)
                return err;
        }
    }

    return err;
}
#endif /* FM_PCD_CC_MANIP */

static t_Error CcUpdateParams(t_Handle                         h_FmPcd,
                              t_Handle                         h_FmPort,
                              t_Handle                         h_FmTree,
                              bool                             validate)
{
    t_FmPcdCcTree       *p_CcTree = (t_FmPcdCcTree *) h_FmTree;

    return CcUpdateParam(h_FmPcd,
                         h_FmPort,
                         p_CcTree->nextEngineAndRequiredAction,
                         p_CcTree->numOfEntries,
                         UINT_TO_PTR(p_CcTree->ccTreeBaseAddr),
                         validate,
                         0,
                         h_FmTree,
                         FALSE);
}

static t_Error CheckParams(t_Handle             h_FmPcd,
                           t_FmPcdCcNodeParams  *p_CcNodeParam,
                           t_FmPcdCcNode        *p_FmPcdCcNode,
                           bool                 *isKeyTblAlloc)
{
    int                     tmp = 0;
    t_FmPcdCcKeyParams      *p_KeyParams;
    t_Error                 err;
    uint32_t                requiredAction = 0;

    err = ValidateNextEngineParams(h_FmPcd, &p_CcNodeParam->keysParams.ccNextEngineParamsForMiss);
    if(err)
        RETURN_ERROR(MAJOR, err, ("For this node MissNextEngineParams are not valid"));

#ifdef FM_PCD_CC_MANIP
    if(p_CcNodeParam->keysParams.ccNextEngineParamsForMiss.h_Manip)
    {
        err = FmPcdManipCheckParamsForCcNextEgine(&p_CcNodeParam->keysParams.ccNextEngineParamsForMiss, &requiredAction);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));
    }
#endif /* FM_PCD_CC_MANIP */

    memcpy(&p_FmPcdCcNode->nextEngineAndRequiredAction[p_FmPcdCcNode->numOfKeys].nextEngineParams,&p_CcNodeParam->keysParams.ccNextEngineParamsForMiss, sizeof(t_FmPcdCcNextEngineParams));
    p_FmPcdCcNode->nextEngineAndRequiredAction[p_FmPcdCcNode->numOfKeys].requiredAction = requiredAction;

    for(tmp = 0 ; tmp < p_FmPcdCcNode->numOfKeys; tmp++)
    {
        p_KeyParams = &p_CcNodeParam->keysParams.keyParams[tmp];

        if(!p_KeyParams->p_Key)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("p_Key is not initialized"));


       err = ValidateNextEngineParams(h_FmPcd, &p_KeyParams->ccNextEngineParams);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));

        err = UpdateGblMask(p_FmPcdCcNode,
                            p_CcNodeParam->keysParams.keySize,
                            p_KeyParams->p_Mask);

#ifdef FM_PCD_CC_MANIP
        if(p_KeyParams->ccNextEngineParams.h_Manip)
        {
            err = FmPcdManipCheckParamsForCcNextEgine(&p_KeyParams->ccNextEngineParams, &requiredAction);
            if(err)
                RETURN_ERROR(MAJOR, err, (NO_MSG));
        }
#endif /* FM_PCD_CC_MANIP */

        memcpy(&p_FmPcdCcNode->nextEngineAndRequiredAction[tmp],&p_KeyParams->ccNextEngineParams, sizeof(t_FmPcdCcNextEngineParams));
        p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].requiredAction = requiredAction;
    }

    *isKeyTblAlloc = TRUE;
    return E_OK;
}

static t_Error Ipv4TtlOrIpv6HopLimiCheckParams(  t_Handle h_FmPcd,
                                                    t_FmPcdCcNodeParams *p_CcNodeParam, t_FmPcdCcNode *p_FmPcdCcNode,
                                                    bool *isKeyTblAlloc)
{
    int                 tmp = 0;
    t_FmPcdCcKeyParams  *p_KeyParams;
    t_Error             err;
    uint8_t             key = 0x01;
    uint32_t            requiredAction = 0;

    if(p_FmPcdCcNode->numOfKeys != 1 )
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("for IPV4TTL and IPV6_HOP_LIMIT has to be only 1 key - TTL = 1, otherwise it's Miss"));

    err = ValidateNextEngineParams(h_FmPcd, &p_CcNodeParam->keysParams.ccNextEngineParamsForMiss);
    if(err)
        RETURN_ERROR(MAJOR, err, ("For this node MissNextEngineParams are not valid"));

#ifdef FM_PCD_CC_MANIP
    if(p_CcNodeParam->keysParams.ccNextEngineParamsForMiss.h_Manip)
    {
        err = FmPcdManipCheckParamsForCcNextEgine(&p_CcNodeParam->keysParams.ccNextEngineParamsForMiss, &requiredAction);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));
    }
#endif /* FM_PCD_CC_MANIP */

    memcpy(&p_FmPcdCcNode->nextEngineAndRequiredAction[p_FmPcdCcNode->numOfKeys].nextEngineParams, &p_CcNodeParam->keysParams.ccNextEngineParamsForMiss, sizeof(t_FmPcdCcNextEngineParams));
    p_FmPcdCcNode->nextEngineAndRequiredAction[p_FmPcdCcNode->numOfKeys].requiredAction = requiredAction;

    for(tmp = 0 ; tmp < p_FmPcdCcNode->numOfKeys; tmp++)
    {
        p_KeyParams = &p_CcNodeParam->keysParams.keyParams[tmp];
        if(p_KeyParams->p_Mask)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("If node of the type IPV4_TTL or IPV6_HOP_LIMIT p_Mask can not be initialized"));
        if(memcmp(p_KeyParams->p_Key, &key, 1) != 0)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("If node of the type IPV4_TTL or IPV6_HOP_LIMIT p_Key has to be 1"));
        err = ValidateNextEngineParams(h_FmPcd, &p_KeyParams->ccNextEngineParams);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));

#ifdef FM_PCD_CC_MANIP
    if(p_KeyParams->ccNextEngineParams.h_Manip)
    {
        err = FmPcdManipCheckParamsForCcNextEgine(&p_KeyParams->ccNextEngineParams, &requiredAction);
        if(err)
            RETURN_ERROR(MAJOR, err, (NO_MSG));
    }
#endif /* FM_PCD_CC_MANIP */

        memcpy(&p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].nextEngineParams, &p_KeyParams->ccNextEngineParams, sizeof(t_FmPcdCcNextEngineParams));
        p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].requiredAction = requiredAction;
    }

    *isKeyTblAlloc = FALSE;
    return E_OK;
}

static t_Error IcHashIndexedCheckParams(t_Handle h_FmPcd,
                                            t_FmPcdCcNodeParams *p_CcNodeParam,
                                            t_FmPcdCcNode *p_FmPcdCcNode,
                                            /*uint16_t *ccInfo,*/
                                            /*t_List *ccNextDifferentNodesLst,*/
                                            bool *isKeyTblAlloc)
{
    int                 tmp = 0, countOnes = 0;
    t_FmPcdCcKeyParams  *p_KeyParams;
    t_Error             err;
    uint16_t            glblMask = p_CcNodeParam->extractCcParams.extractNonHdr.icIndxMask;
    uint16_t            countMask = (uint16_t)(glblMask >> 4);
#ifdef FM_PCD_CC_MANIP
    uint32_t            requiredAction;
#endif /* FM_PCD_CC_MANIP */

    if (glblMask & 0x000f)
       RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("icIndxMask has to be with last nibble 0"));

    while (countMask)
    {
        countOnes++;
        countMask=(uint16_t)(countMask>>1);
    }

    if (!POWER_OF_2(p_FmPcdCcNode->numOfKeys))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("For Node of the type INDEXED numOfKeys has to be powerOfTwo"));
    if (p_FmPcdCcNode->numOfKeys != ((uint32_t)1<<countOnes ))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("For Node of the type IC_HASH_INDEXED numOfKeys has to be powerOfTwo"));

    err = ValidateNextEngineParams(h_FmPcd, &p_CcNodeParam->keysParams.ccNextEngineParamsForMiss);
    if(GET_ERROR_TYPE(err)!= E_NOT_SUPPORTED)
        RETURN_ERROR(MAJOR, err, ("MissNextEngineParams for the node of the type IC_INDEX_HASH has to be UnInitialized"));

    for(tmp = 0 ; tmp < p_FmPcdCcNode->numOfKeys; tmp++)
    {
        p_KeyParams = &p_CcNodeParam->keysParams.keyParams[tmp];
        if(p_KeyParams->p_Mask || p_KeyParams->p_Key)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("For Node of the type IC_HASH_INDEXED p_Key or p_Mask has to be NULL"));

        if((glblMask & (tmp * 16)) == (tmp * 16))
        {
            err = ValidateNextEngineParams(h_FmPcd, &p_KeyParams->ccNextEngineParams);
            if(err)
                RETURN_ERROR(MAJOR, err, ("This index has to be initialized for the node of the type IC_INDEX_HASH according to settings of GlobalMask "));

#ifdef FM_PCD_CC_MANIP
            if(p_KeyParams->ccNextEngineParams.h_Manip)
            {
                err = FmPcdManipCheckParamsForCcNextEgine(&p_KeyParams->ccNextEngineParams, &requiredAction);
                if(err)
                    RETURN_ERROR(MAJOR, err, (NO_MSG));
            }
            p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].requiredAction = requiredAction;
#endif /* FM_PCD_CC_MANIP */

            memcpy(&p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].nextEngineParams,&p_KeyParams->ccNextEngineParams, sizeof(t_FmPcdCcNextEngineParams));
        }
        else
        {
            err = ValidateNextEngineParams(h_FmPcd, &p_KeyParams->ccNextEngineParams);
            if(GET_ERROR_TYPE(err)!= E_NOT_SUPPORTED)
                RETURN_ERROR(MAJOR, err, ("This index has to be UnInitialized for the node of the type IC_INDEX_HASH according to settings of GlobalMask"));
        }
    }
    *isKeyTblAlloc = FALSE;
    memcpy(PTR_MOVE(p_FmPcdCcNode->p_GlblMask, 2), &glblMask, 2);

    return E_OK;
}

t_Error FmPcdCcModifyNextEngineParamTree(t_Handle h_FmPcd, t_Handle h_FmPcdCcTree, uint8_t grpId, uint8_t index, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams, t_List *h_OldLst, t_List *h_NewLst, t_Handle *h_AdditionalParams)
{
    t_FmPcdCcTree                       *p_FmPcdCcTree = (t_FmPcdCcTree *)h_FmPcdCcTree;
    t_Error                             err = E_OK;
    uint16_t                            keyIndex;
    t_FmPcdModifyCcKeyAdditionalParams *p_ModifyKeyParams;

    SANITY_CHECK_RETURN_ERROR((grpId <= 7),E_INVALID_VALUE);
    SANITY_CHECK_RETURN_ERROR(h_FmPcdCcTree,E_INVALID_VALUE);

    if(grpId >= p_FmPcdCcTree->numOfGrps)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("grpId you asked > numOfGroup of relevant tree"));

    if(index >= p_FmPcdCcTree->fmPcdGroupParam[grpId].numOfEntriesInGroup)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("index > numOfEntriesInGroup"));

    keyIndex = (uint16_t)(p_FmPcdCcTree->fmPcdGroupParam[grpId].baseGroupEntry + index);

    err =  ModifyKeyCommonPart1(h_FmPcdCcTree, keyIndex, h_AdditionalParams, e_MODIFY_STATE_CHANGE, TRUE, TRUE);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    p_ModifyKeyParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_AdditionalParams;
    p_ModifyKeyParams->tree = TRUE;

    err = BuildNewNodeModifyNextEngine (h_FmPcd, h_FmPcdCcTree, keyIndex,p_FmPcdCcNextEngineParams, h_OldLst, h_NewLst, p_ModifyKeyParams);
    if(err)
    {
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }
    return E_OK;

}

t_Error FmPcdCcRemoveKey(t_Handle h_FmPcd, t_Handle h_FmPcdCcNode, uint8_t keyIndex, t_List *h_OldLst, t_List *h_NewLst, t_Handle *h_AdditionalParams)
{

    t_FmPcdCcNode                       *p_FmPcdCcNode = (t_FmPcdCcNode *) h_FmPcdCcNode;
    t_Error                             err = E_OK;
    t_FmPcdModifyCcKeyAdditionalParams  *p_ModifyKeyParams;

    if(keyIndex >= p_FmPcdCcNode->numOfKeys)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("impossible to remove key when numOfKeys <= keyIndex"));

    if(!p_FmPcdCcNode->numOfKeys)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("keyIndex you asked > numOfKeys of relevant node that was initialized"));

    if(p_FmPcdCcNode->h_FmPcd != h_FmPcd)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("handler to FmPcd is diferent from one which was assigned to the node in the Init time"));

    err =  ModifyKeyCommonPart1(p_FmPcdCcNode, keyIndex, h_AdditionalParams, e_MODIFY_STATE_REMOVE, TRUE, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    p_ModifyKeyParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_AdditionalParams;
    err = BuildNewNodeRemoveKey (p_FmPcdCcNode, keyIndex, p_ModifyKeyParams);
    if(err)
    {
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err = UpdatePtrWhichPointOnCrntMdfNode(p_FmPcdCcNode, p_ModifyKeyParams, h_OldLst, h_NewLst);
    if(err)
    {
        ReleaseNewNodeCommonPart(p_ModifyKeyParams);
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    return E_OK;

}

t_Error FmPcdCcModifyKey(t_Handle h_FmPcd, t_Handle h_FmPcdCcNode, uint8_t keyIndex, uint8_t keySize, uint8_t *p_Key, uint8_t *p_Mask, t_List *h_OldLst, t_List *h_NewLst,t_Handle *h_AdditionalParams)
{
    t_FmPcdCcNode                       *p_FmPcdCcNode = (t_FmPcdCcNode *)h_FmPcdCcNode;
    t_Error                             err = E_OK;
    t_FmPcdModifyCcKeyAdditionalParams  *p_ModifyKeyParams;

    if(keyIndex >= p_FmPcdCcNode->numOfKeys)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("keyIndex > previousely cleared last index + 1"));

    if((p_FmPcdCcNode->numOfKeys + 1) > FM_PCD_MAX_NUM_OF_CC_NODES)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfKeys with new key can not be larger than 255"));

    if(keySize != p_FmPcdCcNode->userSizeOfExtraction)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("size for ModifyKey has to be the same as defined in SetNode"));

    if(p_FmPcdCcNode->h_FmPcd != h_FmPcd)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("handler to FmPcd is diferent from one which was assigned to the node in the Init time"));

    err =  ModifyKeyCommonPart1(p_FmPcdCcNode, keyIndex, h_AdditionalParams, e_MODIFY_STATE_CHANGE, TRUE, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    p_ModifyKeyParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_AdditionalParams;

    err = BuildNewNodeModifyKey (p_FmPcdCcNode, keyIndex, p_Key, p_Mask, p_ModifyKeyParams);
    if(err)
    {
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err = UpdatePtrWhichPointOnCrntMdfNode(p_FmPcdCcNode, p_ModifyKeyParams, h_OldLst, h_NewLst);
    if(err)
    {
        ReleaseNewNodeCommonPart(p_ModifyKeyParams);
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }
    return E_OK;
}


t_Error     FmPcdCcModiyNextEngineParamNode(t_Handle h_FmPcd,t_Handle h_FmPcdCcNode, uint8_t keyIndex,t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams,t_List *h_OldPointer, t_List *h_NewPointer,t_Handle *h_AdditionalParams)
{
    t_FmPcdCcNode                   *p_FmPcdCcNode = (t_FmPcdCcNode *)h_FmPcdCcNode;
    t_Error                         err = E_OK;
    t_FmPcdModifyCcKeyAdditionalParams *p_ModifyKeyParams;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd,E_INVALID_VALUE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcdCcNode,E_INVALID_HANDLE);

    if(keyIndex >= p_FmPcdCcNode->numOfKeys)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("keyIndex > previousely cleared last index + 1"));

    if((p_FmPcdCcNode->numOfKeys + 1) > FM_PCD_MAX_NUM_OF_CC_NODES)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfKeys with new key can not be larger than 255"));

    err =  ModifyKeyCommonPart1(p_FmPcdCcNode, keyIndex, h_AdditionalParams, e_MODIFY_STATE_CHANGE, FALSE, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    p_ModifyKeyParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_AdditionalParams;

    err = BuildNewNodeModifyNextEngine (h_FmPcd, p_FmPcdCcNode, keyIndex,p_FmPcdCcNextEngineParams, h_OldPointer, h_NewPointer, p_ModifyKeyParams);
    if(err)
    {
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }
    return E_OK;
}

t_Error FmPcdCcModifyMissNextEngineParamNode(t_Handle h_FmPcd,t_Handle h_FmPcdCcNode, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams,t_List *h_OldPointer, t_List *h_NewPointer,t_Handle *h_AdditionalParams)
{
    t_FmPcdCcNode                   *p_FmPcdCcNode = (t_FmPcdCcNode *)h_FmPcdCcNode;
    t_Error                         err = E_OK;
    uint16_t                         keyIndex;
    t_FmPcdModifyCcKeyAdditionalParams *p_ModifyKeyParams;

    SANITY_CHECK_RETURN_ERROR(p_FmPcdCcNode,E_INVALID_VALUE);

    keyIndex = p_FmPcdCcNode->numOfKeys;

    err =  ModifyKeyCommonPart1(p_FmPcdCcNode, keyIndex, h_AdditionalParams, e_MODIFY_STATE_CHANGE, TRUE, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    p_ModifyKeyParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_AdditionalParams;

    err = BuildNewNodeModifyNextEngine (h_FmPcd, p_FmPcdCcNode, keyIndex,p_FmPcdCcNextEngineParams, h_OldPointer, h_NewPointer, p_ModifyKeyParams);
    if(err)
    {
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    return E_OK;
}

t_Error FmPcdCcAddKey(t_Handle h_FmPcd, t_Handle h_FmPcdCcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams *p_FmPcdCcKeyParams, t_List *h_OldLst, t_List *h_NewLst, t_Handle *h_AdditionalParams)
{
    t_FmPcdCcNode                       *p_FmPcdCcNode = (t_FmPcdCcNode *)h_FmPcdCcNode;
    t_FmPcdModifyCcKeyAdditionalParams  *p_ModifyKeyParams;
    t_Error                             err = E_OK;

    if(keyIndex > p_FmPcdCcNode->numOfKeys)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("keyIndex > previousely cleared last index + 1"));

    if((p_FmPcdCcNode->numOfKeys + 1) > FM_PCD_MAX_NUM_OF_CC_NODES)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfKeys with new key can not be larger than 255"));

    if(keySize != p_FmPcdCcNode->userSizeOfExtraction)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("keySize has to be defined as it was defined in initialization step."));

    if(p_FmPcdCcNode->h_FmPcd != h_FmPcd)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("handler to FmPcd is diferent from one which was assigned to the node in the Init time"));

    err =  ModifyKeyCommonPart1(p_FmPcdCcNode, keyIndex, h_AdditionalParams, e_MODIFY_STATE_ADD, TRUE, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    p_ModifyKeyParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_AdditionalParams;
    err = BuildNewNodeAddOrMdfyKeyAndNextEngine (h_FmPcd, p_FmPcdCcNode, keyIndex, p_FmPcdCcKeyParams, p_ModifyKeyParams, TRUE);
    if(err)
    {
        XX_Free(p_ModifyKeyParams);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err = UpdatePtrWhichPointOnCrntMdfNode(p_FmPcdCcNode, p_ModifyKeyParams, h_OldLst, h_NewLst);
                if(err)
    {
        ReleaseNewNodeCommonPart(p_ModifyKeyParams);
        XX_Free(p_ModifyKeyParams);
                    RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    return E_OK;
}

t_Error FmPcdCcModifyKeyAndNextEngine(t_Handle h_FmPcd, t_Handle h_FmPcdCcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams *p_FmPcdCcKeyParams, t_List *h_OldLst, t_List *h_NewLst, t_Handle *h_AdditionalParams)
{
    t_FmPcdCcNode                       *p_FmPcdCcNode = (t_FmPcdCcNode *)h_FmPcdCcNode;
    t_FmPcdModifyCcKeyAdditionalParams  *p_ModifyKeyParams;
    t_Error                             err = E_OK;

    if(keyIndex > p_FmPcdCcNode->numOfKeys)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("keyIndex > previousely cleared last index + 1"));

    if((p_FmPcdCcNode->numOfKeys + 1) > FM_PCD_MAX_NUM_OF_CC_NODES)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfKeys with new key can not be larger than 255"));

    if(keySize != p_FmPcdCcNode->userSizeOfExtraction)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("keySize has to be defined as it was defined in initialization step"));

    if(p_FmPcdCcNode->h_FmPcd != h_FmPcd)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("handler to FmPcd is diferent from one which was assigned to the node in the Init time"));

    err =  ModifyKeyCommonPart1(p_FmPcdCcNode, keyIndex, h_AdditionalParams, e_MODIFY_STATE_CHANGE, TRUE, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    p_ModifyKeyParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_AdditionalParams;

    err = BuildNewNodeAddOrMdfyKeyAndNextEngine (h_FmPcd, p_FmPcdCcNode, keyIndex, p_FmPcdCcKeyParams, p_ModifyKeyParams, FALSE);
    if(err)
    {
        ReleaseNewNodeCommonPart(p_ModifyKeyParams);
        XX_Free(p_ModifyKeyParams);
                        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err = UpdatePtrWhichPointOnCrntMdfNode(p_FmPcdCcNode, p_ModifyKeyParams, h_OldLst, h_NewLst);
    if(err)
    {
        ReleaseNewNodeCommonPart(p_ModifyKeyParams);
        XX_Free(p_ModifyKeyParams);
                        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    return E_OK;
}

t_Error FmPcdCcReleaseModifiedDataStructure(t_Handle h_FmPcd, t_List *h_FmPcdOldPointersLst, t_List *h_FmPcdNewPointersLst, uint16_t numOfGoodChanges, t_Handle *h_Params)
{
    t_FmPcdModifyCcKeyAdditionalParams *p_CcNewModifyAdditionalParams = (t_FmPcdModifyCcKeyAdditionalParams *)*h_Params;
    t_List                          *p_Pos;
    t_Error                         err = E_OK;
    t_CcNodeInformation             ccNodeInfo, *p_CcNodeInformation;
    t_Handle                        h_Muram;
    t_FmPcdCcNode                   *p_FmPcdCcNextNode;
    t_List                          *p_UpdateLst;

    UNUSED(numOfGoodChanges);

    SANITY_CHECK_RETURN_ERROR(h_FmPcd,E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_CcNewModifyAdditionalParams->h_CurrentNode,E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(h_FmPcdOldPointersLst,E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(h_FmPcdNewPointersLst,E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR((numOfGoodChanges == LIST_NumOfObjs(h_FmPcdOldPointersLst)),E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR((numOfGoodChanges == LIST_NumOfObjs(h_FmPcdNewPointersLst)),E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR((LIST_NumOfObjs(h_FmPcdOldPointersLst) == LIST_NumOfObjs(h_FmPcdNewPointersLst)),E_INVALID_STATE);

    /*we don't update subtree of the new node with new tree because it was done in the previose stage*/
    if(p_CcNewModifyAdditionalParams->h_NodeForAdd)
    {
        p_FmPcdCcNextNode = (t_FmPcdCcNode*)p_CcNewModifyAdditionalParams->h_NodeForAdd;
        if(!p_CcNewModifyAdditionalParams->tree)
            p_UpdateLst = &p_FmPcdCcNextNode->ccPrevNodesLst;
        else
            p_UpdateLst = &p_FmPcdCcNextNode->ccTreeIdLst;
        p_CcNodeInformation = FindNodeInfoInReleventLst(p_UpdateLst, p_CcNewModifyAdditionalParams->h_CurrentNode);
        if(p_CcNodeInformation)
            p_CcNodeInformation->index++;
        else
        {
            memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
            ccNodeInfo.h_CcNode = (t_Handle)p_CcNewModifyAdditionalParams->h_CurrentNode;
            ccNodeInfo.index = 1;
            EnqueueNodeInfoToRelevantLst(p_UpdateLst, &ccNodeInfo);
        }
    }

     if(p_CcNewModifyAdditionalParams->h_NodeForRmv)
    {

        p_FmPcdCcNextNode = (t_FmPcdCcNode*)p_CcNewModifyAdditionalParams->h_NodeForRmv;
        if(!p_CcNewModifyAdditionalParams->tree)
        {
            p_UpdateLst = &p_FmPcdCcNextNode->ccPrevNodesLst;
            LIST_FOR_EACH(p_Pos, &p_FmPcdCcNextNode->ccTreesLst)
            {
                p_CcNodeInformation = CC_NODE_F_OBJECT(p_Pos);
                ASSERT_COND(p_CcNodeInformation->h_CcNode);
                err = FmPcdCcSetRequiredAction(h_FmPcd,
                                               UPDATE_CC_WITH_DELETE_TREE,
                                               &((t_FmPcdCcNode *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->nextEngineAndRequiredAction[p_CcNewModifyAdditionalParams->keyIndex],
                                               PTR_MOVE(((t_FmPcdCcNode *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->h_AdTable, p_CcNewModifyAdditionalParams->keyIndex*FM_PCD_CC_AD_ENTRY_SIZE),
                                               1,
                                               p_CcNodeInformation->h_CcNode);
            }
        }
        else
        {
            p_UpdateLst = &p_FmPcdCcNextNode->ccTreeIdLst;
            err =  FmPcdCcSetRequiredAction(h_FmPcd,
                                            UPDATE_CC_WITH_DELETE_TREE,
                                            &((t_FmPcdCcTree *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->nextEngineAndRequiredAction[p_CcNewModifyAdditionalParams->keyIndex],
                                            UINT_TO_PTR(((t_FmPcdCcTree *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->ccTreeBaseAddr + p_CcNewModifyAdditionalParams->keyIndex*FM_PCD_CC_AD_ENTRY_SIZE),
                                            1,
                                            p_CcNewModifyAdditionalParams->h_CurrentNode);
        }
        if(err)
            return err;

        /*we remove from the  subtree of the removed node tree because it wasn't done in the previose stage*/
        /*update ccPrevNodesLst or ccTreeIdLst of the removed node*/
        /*update of the nodeOwner*/
        p_CcNodeInformation = FindNodeInfoInReleventLst(p_UpdateLst, p_CcNewModifyAdditionalParams->h_CurrentNode);
        ASSERT_COND(p_CcNodeInformation);
        ASSERT_COND(p_CcNodeInformation->index);
        p_CcNodeInformation->index--;
        if(p_CcNodeInformation->index == 0)
           DequeueNodeInfoFromRelevantLst(p_UpdateLst,p_CcNewModifyAdditionalParams->h_CurrentNode);
        ASSERT_COND(LIST_NumOfObjs(&p_FmPcdCcNextNode->ccTreesLst) == 1);
        UpdateNodeOwner(p_FmPcdCcNextNode, FALSE);
    }

#ifdef FM_PCD_CC_MANIP
    if(p_CcNewModifyAdditionalParams->h_ManipForRmv)
        FmPcdManipUpdateOwner(p_CcNewModifyAdditionalParams->h_ManipForRmv, FALSE);
#endif /* FM_PCD_CC_MANIP */

    h_Muram = FmPcdGetMuramHandle(h_FmPcd);
    ASSERT_COND(h_Muram);

    /*we release new AD which was allocated and updated for copy from to actual AD*/
    LIST_FOR_EACH(p_Pos, h_FmPcdNewPointersLst)
    {
        p_CcNodeInformation = CC_NODE_F_OBJECT(p_Pos);
        ASSERT_COND(p_CcNodeInformation->h_CcNode);
        FM_MURAM_FreeMem(h_Muram, p_CcNodeInformation->h_CcNode);

     }

    /*free Old data structure if it has to be freed - new data structure was allocated*/
    if(p_CcNewModifyAdditionalParams->p_AdTableOld)
        FM_MURAM_FreeMem(h_Muram,p_CcNewModifyAdditionalParams->p_AdTableOld);
    if(p_CcNewModifyAdditionalParams->p_KeysMatchTableOld)
        FM_MURAM_FreeMem(h_Muram,p_CcNewModifyAdditionalParams->p_KeysMatchTableOld);

    /*update current modified node with changed fields if it's required*/
    if(!p_CcNewModifyAdditionalParams->tree)
    {
        if(p_CcNewModifyAdditionalParams->p_AdTableNew)
            ((t_FmPcdCcNode *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->h_AdTable    = p_CcNewModifyAdditionalParams->p_AdTableNew;
        if(p_CcNewModifyAdditionalParams->numOfKeys)
            ((t_FmPcdCcNode *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->numOfKeys    = p_CcNewModifyAdditionalParams->numOfKeys;
        if(p_CcNewModifyAdditionalParams->p_KeysMatchTableNew)
            ((t_FmPcdCcNode *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->h_KeysMatchTable    = p_CcNewModifyAdditionalParams->p_KeysMatchTableNew;
        memcpy(((t_FmPcdCcNode *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->nextEngineAndRequiredAction, &p_CcNewModifyAdditionalParams->nextEngineAndRequiredAction, sizeof(t_FmPcdCcNextEngineAndRequiredActionParams) * (FM_PCD_MAX_NUM_OF_KEYS));
    }
    else
        memcpy(&((t_FmPcdCcTree *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->nextEngineAndRequiredAction, &p_CcNewModifyAdditionalParams->nextEngineAndRequiredAction, sizeof(t_FmPcdCcNextEngineAndRequiredActionParams) * (((t_FmPcdCcTree *)(p_CcNewModifyAdditionalParams->h_CurrentNode))->numOfEntries));

    ReleaseLst(h_FmPcdOldPointersLst);
    ReleaseLst(h_FmPcdNewPointersLst);
    XX_Free(p_CcNewModifyAdditionalParams);

    return E_OK;
}

uint32_t FmPcdCcGetNodeAddrOffsetFromNodeInfo(t_Handle h_FmPcd, t_Handle h_Pointer)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;
    t_CcNodeInformation             *p_CcNodeInfo;

    SANITY_CHECK_RETURN_VALUE(h_FmPcd,E_INVALID_HANDLE, (uint32_t)ILLEGAL_BASE);

    p_CcNodeInfo = CC_NODE_F_OBJECT(h_Pointer);
    return (uint32_t)(XX_VirtToPhys(p_CcNodeInfo->h_CcNode) - p_FmPcd->physicalMuramBase);
}

t_Error FmPcdCcGetGrpParams(t_Handle h_FmPcdCcTree, uint8_t grpId, uint32_t *p_GrpBits, uint8_t *p_GrpBase)
{
    t_FmPcdCcTree *p_FmPcdCcTree = (t_FmPcdCcTree *) h_FmPcdCcTree;

    SANITY_CHECK_RETURN_ERROR(h_FmPcdCcTree, E_INVALID_HANDLE);

    if(grpId >= p_FmPcdCcTree->numOfGrps)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("grpId you asked > numOfGroup of relevant tree"));
    *p_GrpBits = p_FmPcdCcTree->fmPcdGroupParam[grpId].totalBitsMask;
    *p_GrpBase = p_FmPcdCcTree->fmPcdGroupParam[grpId].baseGroupEntry;
    return E_OK;
}

t_Error  FmPcdCcBindTree(t_Handle h_FmPcd, t_Handle  h_FmPcdCcTree,  uint32_t  *p_Offset, t_Handle h_FmPort)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdCcTree       *p_FmPcdCcTree = (t_FmPcdCcTree *)h_FmPcdCcTree;
    t_Error             err = E_OK;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd,E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcdCcTree,E_INVALID_STATE);

    FmPcdCcUpdateTreeOwner(p_FmPcdCcTree, TRUE);

    *p_Offset = (uint32_t)(XX_VirtToPhys(UINT_TO_PTR(p_FmPcdCcTree->ccTreeBaseAddr)) -
                           p_FmPcd->physicalMuramBase);

    err = CcUpdateParams(h_FmPcd, h_FmPort, h_FmPcdCcTree, TRUE);

    return err;
}

t_Error FmPcdCcUnbindTree(t_Handle h_FmPcd, t_Handle  h_FmPcdCcTree)
{
    t_FmPcdCcTree       *p_FmPcdCcTree = (t_FmPcdCcTree *)h_FmPcdCcTree;

    UNUSED(h_FmPcd);

    SANITY_CHECK_RETURN_ERROR(p_FmPcdCcTree,E_INVALID_HANDLE);

    FmPcdCcUpdateTreeOwner(p_FmPcdCcTree, FALSE);

    return E_OK;
}

t_Error FmPcdCcTreeTryLock(t_Handle h_FmPcdCcTree)
{
    if (TRY_LOCK(NULL, &((t_FmPcdCcTree *)h_FmPcdCcTree)->lock))
        return E_OK;
    return ERROR_CODE(E_BUSY);
}

t_Error FmPcdCcNodeTreeTryLock(t_Handle h_FmPcd,t_Handle h_FmPcdCcNode, t_List *p_List)
{
    t_FmPcdCcNode   *p_FmPcdCcNode = (t_FmPcdCcNode *)h_FmPcdCcNode;
    t_List          *p_Pos;
    t_CcNodeInformation    *p_CcNodeInfo, nodeInfo;
    t_Error         err = E_OK;

    UNUSED(h_FmPcd);

    if(LIST_IsEmpty(&p_FmPcdCcNode->ccTreesLst))
        RETURN_ERROR(MAJOR, E_NOT_AVAILABLE, ("asked for more nodes in CC than MAX"))  ;
    LIST_FOR_EACH(p_Pos, &p_FmPcdCcNode->ccTreesLst)
    {
        p_CcNodeInfo = CC_NODE_F_OBJECT(p_Pos);
        ASSERT_COND(p_CcNodeInfo->h_CcNode);
        err = FmPcdCcTreeTryLock(p_CcNodeInfo->h_CcNode);
        if(err == E_OK)
        {
            memset(&nodeInfo, 0, sizeof(t_CcNodeInformation));
            nodeInfo.h_CcNode = p_CcNodeInfo->h_CcNode;
            EnqueueNodeInfoToRelevantLst(p_List, &nodeInfo);
        }
        else
            FmPcdCcNodeTreeReleaseLock(p_List);
    }

    return err;
}

t_Handle FM_PCD_CcBuildTree(t_Handle h_FmPcd, t_FmPcdCcTreeParams *p_PcdGroupsParam)
{
    t_FmPcd                     *p_FmPcd = (t_FmPcd *)h_FmPcd;
    t_Error                     err = E_OK;
    int                         i = 0, j = 0, k = 0;
    t_FmPcdCcTree               *p_FmPcdCcTree;
    uint8_t                     numOfEntries;
    t_Handle                    p_CcTreeTmp;
    t_FmPcdCcGrpParams          *p_FmPcdCcGroupParams;
    t_FmPcdCcNextEngineAndRequiredActionParams   params[16];
    t_NetEnvParams              netEnvParams;
    uint8_t                     lastOne = 0;
    uint32_t                    requiredAction = 0;
    t_FmPcdCcNode               *p_FmPcdCcNextNode;
    t_CcNodeInformation         ccNodeInfo, *p_CcInformation;

    SANITY_CHECK_RETURN_VALUE(h_FmPcd,E_INVALID_HANDLE, NULL);
    SANITY_CHECK_RETURN_VALUE(p_PcdGroupsParam,E_INVALID_HANDLE, NULL);

    if (p_PcdGroupsParam->numOfGrps > FM_PCD_MAX_NUM_OF_CC_GROUPS)
    {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("numOfGrps should not exceed %d", FM_PCD_MAX_NUM_OF_CC_GROUPS));
        return NULL;
    }

    p_FmPcdCcTree = (t_FmPcdCcTree*)XX_Malloc(sizeof(t_FmPcdCcTree));
    if(!p_FmPcdCcTree)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("PCD tree structure"));
        return NULL;
    }
    memset(p_FmPcdCcTree, 0, sizeof(t_FmPcdCcTree)) ;
    memset(params, 0, 16 * sizeof(t_FmPcdCcNextEngineParams));

    INIT_LIST(&p_FmPcdCcTree->fmPortsLst);

    numOfEntries = 0;
    p_FmPcdCcTree->netEnvId = (uint8_t)(PTR_TO_UINT(p_PcdGroupsParam->h_NetEnv)-1);
    for(i = 0; i < p_PcdGroupsParam->numOfGrps; i++)
    {
        p_FmPcdCcGroupParams = &p_PcdGroupsParam->ccGrpParams[i];

        if (p_FmPcdCcGroupParams->numOfDistinctionUnits > FM_PCD_MAX_NUM_OF_CC_UNITS)
        {
            DeleteTree(p_FmPcdCcTree,p_FmPcd);
            REPORT_ERROR(MAJOR, E_INVALID_VALUE,
                         ("numOfDistinctionUnits (group %d) should not exceed %d", i, FM_PCD_MAX_NUM_OF_CC_UNITS));
            return NULL;
        }

        p_FmPcdCcTree->fmPcdGroupParam[i].baseGroupEntry = numOfEntries;
        p_FmPcdCcTree->fmPcdGroupParam[i].numOfEntriesInGroup =(uint8_t)( 0x01 << p_FmPcdCcGroupParams->numOfDistinctionUnits);
        numOfEntries += p_FmPcdCcTree->fmPcdGroupParam[i].numOfEntriesInGroup;
        if(numOfEntries > 16)
        {
            DeleteTree(p_FmPcdCcTree,p_FmPcd);
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("numOfEntries can not be larger than 16"));
            return NULL;
        }
        if(lastOne)
        {
            if(p_FmPcdCcTree->fmPcdGroupParam[i].numOfEntriesInGroup > lastOne)
            {
                DeleteTree(p_FmPcdCcTree,p_FmPcd);
                REPORT_ERROR(MAJOR, E_CONFLICT, ("numOfEntries per group must be set in descending order"));
                return NULL;
            }
        }

        lastOne = p_FmPcdCcTree->fmPcdGroupParam[i].numOfEntriesInGroup;

        netEnvParams.netEnvId = p_FmPcdCcTree->netEnvId;
        netEnvParams.numOfDistinctionUnits = p_FmPcdCcGroupParams->numOfDistinctionUnits;
        memcpy(netEnvParams.unitIds, &p_FmPcdCcGroupParams->unitIds, (sizeof(uint8_t)) * p_FmPcdCcGroupParams->numOfDistinctionUnits);
        err = PcdGetUnitsVector(p_FmPcd, &netEnvParams);
        if(err)
        {
            DeleteTree(p_FmPcdCcTree,p_FmPcd);
            REPORT_ERROR(MAJOR, err, NO_MSG);
            return NULL;
        }

        p_FmPcdCcTree->fmPcdGroupParam[i].totalBitsMask = netEnvParams.vector;
        for(j = 0; j < p_FmPcdCcTree->fmPcdGroupParam[i].numOfEntriesInGroup; j++)
        {
            err = ValidateNextEngineParams(h_FmPcd,&p_FmPcdCcGroupParams->nextEnginePerEntriesInGrp[j]);
            if(err)
            {
                DeleteTree(p_FmPcdCcTree,p_FmPcd);
                REPORT_ERROR(MAJOR, err, (NO_MSG));
                return NULL;
            }

#ifdef FM_PCD_CC_MANIP
            if(p_FmPcdCcGroupParams->nextEnginePerEntriesInGrp[j].h_Manip)
            {
                err = FmPcdManipCheckParamsForCcNextEgine(&p_FmPcdCcGroupParams->nextEnginePerEntriesInGrp[j], &requiredAction);
                if(err)
                {
                    DeleteTree(p_FmPcdCcTree,p_FmPcd);
                    REPORT_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
                    return NULL;
                }
           }
#endif /* FM_PCD_CC_MANIP */

           memcpy(&params[k].nextEngineParams, &p_FmPcdCcGroupParams->nextEnginePerEntriesInGrp[j], sizeof(t_FmPcdCcNextEngineParams));
           requiredAction |= UPDATE_CC_WITH_TREE;
           params[k].requiredAction = requiredAction;
           k++;
        }
    }

    p_FmPcdCcTree->numOfEntries = (uint8_t)k;
    p_FmPcdCcTree->numOfGrps = p_PcdGroupsParam->numOfGrps;
    p_FmPcdCcTree->ccTreeBaseAddr =
        PTR_TO_UINT(FM_MURAM_AllocMem(FmPcdGetMuramHandle(h_FmPcd),
                                      (uint32_t)( k * FM_PCD_CC_AD_ENTRY_SIZE),
                                      FM_PCD_CC_AD_TABLE_ALIGN));

    if(!p_FmPcdCcTree->ccTreeBaseAddr)
    {
        DeleteTree(p_FmPcdCcTree,p_FmPcd);
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("No memory"));
        return NULL;
    }
    IOMemSet32(UINT_TO_PTR(p_FmPcdCcTree->ccTreeBaseAddr), 0, (uint32_t)(k * FM_PCD_CC_AD_ENTRY_SIZE));

    p_CcTreeTmp = UINT_TO_PTR(p_FmPcdCcTree->ccTreeBaseAddr);

    j = 0;
    for(i = 0; i < numOfEntries; i++)
    {
        NextStepAd(p_CcTreeTmp,&params[i].nextEngineParams,p_FmPcd);
        p_CcTreeTmp = PTR_MOVE(p_CcTreeTmp, FM_PCD_CC_AD_ENTRY_SIZE);
        memcpy(&p_FmPcdCcTree->nextEngineAndRequiredAction[i], &params[i], sizeof(t_FmPcdCcNextEngineAndRequiredActionParams));
        if(p_FmPcdCcTree->nextEngineAndRequiredAction[i].nextEngineParams.nextEngine== e_FM_PCD_CC)
        {
            p_FmPcdCcNextNode = (t_FmPcdCcNode*)p_FmPcdCcTree->nextEngineAndRequiredAction[i].nextEngineParams.params.ccParams.h_CcNode;
            if(!IsNodeInModifiedState((t_Handle)p_FmPcdCcNextNode))
            {
                memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
                ccNodeInfo.h_CcNode = (t_Handle)p_FmPcdCcTree;
                ccNodeInfo.index = 1;
                EnqueueNodeInfoToRelevantLst(&p_FmPcdCcNextNode->ccTreeIdLst, &ccNodeInfo);
                UpdateNodeWithModifiedState((t_Handle)p_FmPcdCcNextNode, TRUE);
            }
            else
            {
                p_CcInformation = FindNodeInfoInReleventLst(&p_FmPcdCcNextNode->ccTreeIdLst,(t_Handle)p_FmPcdCcTree);
                ASSERT_COND(p_CcInformation);
                p_CcInformation->index++;
            }
        }
    }

    FmPcdIncNetEnvOwners(h_FmPcd, p_FmPcdCcTree->netEnvId);
    p_CcTreeTmp = UINT_TO_PTR(p_FmPcdCcTree->ccTreeBaseAddr);

    for(i = 0; i < p_FmPcdCcTree->numOfEntries ; i++)
    {
        if(p_FmPcdCcTree->nextEngineAndRequiredAction[i].nextEngineParams.nextEngine == e_FM_PCD_CC)
        {
            p_FmPcdCcNextNode = (t_FmPcdCcNode*)p_FmPcdCcTree->nextEngineAndRequiredAction[i].nextEngineParams.params.ccParams.h_CcNode;

            if(IsNodeInModifiedState((t_Handle)p_FmPcdCcNextNode))
                UpdateNodeWithModifiedState((t_Handle)p_FmPcdCcNextNode, FALSE);
        }
    }

    for(i = 0; i < numOfEntries; i++)
    {
        if(p_FmPcdCcTree->nextEngineAndRequiredAction[i].requiredAction)
        {
            err = FmPcdCcSetRequiredAction(h_FmPcd, p_FmPcdCcTree->nextEngineAndRequiredAction[i].requiredAction, &p_FmPcdCcTree->nextEngineAndRequiredAction[i], p_CcTreeTmp,1, p_FmPcdCcTree);
            if(err)
            {
                DeleteTree(p_FmPcdCcTree,p_FmPcd);
                REPORT_ERROR(MAJOR, E_NO_MEMORY, ("No memory"));
                return NULL;
            }
            p_CcTreeTmp = PTR_MOVE(p_CcTreeTmp, FM_PCD_CC_AD_ENTRY_SIZE);
        }
    }

    return p_FmPcdCcTree;
}

t_Error FM_PCD_CcDeleteTree(t_Handle h_FmPcd, t_Handle h_CcTree)
{
    t_FmPcd                     *p_FmPcd = (t_FmPcd *)h_FmPcd;
    t_FmPcdCcTree               *p_CcTree = (t_FmPcdCcTree *)h_CcTree;
    int                         i= 0;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_CcTree,E_INVALID_STATE);

    FmPcdDecNetEnvOwners(h_FmPcd, p_CcTree->netEnvId);

    if(p_CcTree->owners)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("the tree with this ID can not be removed because this tree is occupied, first - unbind this tree"));

    for(i = 0; i <p_CcTree->numOfEntries; i++)
    {
        if(p_CcTree->nextEngineAndRequiredAction[i].nextEngineParams.nextEngine == e_FM_PCD_CC)
            UpdateNodeOwner(p_CcTree->nextEngineAndRequiredAction[i].nextEngineParams.params.ccParams.h_CcNode, FALSE);
    }

#ifdef FM_PCD_CC_MANIP
    for(i = 0; i < p_CcTree->numOfEntries; i++)
    {
        if(p_CcTree->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip)
            FmPcdManipUpdateOwner(p_CcTree->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip, FALSE);
    }
#endif /* FM_PCD_CC_MANIP */

    DeleteTree(p_CcTree, p_FmPcd);
    return E_OK;
}

t_Handle FM_PCD_CcSetNode(t_Handle h_FmPcd, t_FmPcdCcNodeParams *p_CcNodeParam)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd *) h_FmPcd;
    t_FmPcdCcNode       *p_FmPcdCcNode, *p_FmPcdCcNextNode;
    t_Error             err = E_OK;
    int                 tmp, size;
    bool                glblMask = FALSE;
    t_FmPcdCcKeyParams  *p_KeyParams;
    t_Handle            p_KeysMatchTblTmp;
    t_Handle            p_AdTableTmp;
    bool                fullField = FALSE;
    ccPrivateInfo_t     icCode = CC_PRIVATE_INFO_NONE;
    bool                isKeyTblAlloc, fromIc = FALSE;
    t_CcNodeInformation ccNodeInfo, *p_CcInformation;

    SANITY_CHECK_RETURN_VALUE(h_FmPcd,E_INVALID_HANDLE,NULL);

    /*
    if (!p_CcNodeParam->keysParams.keySize ||
        !p_CcNodeParam->keysParams.numOfKeys)
    {
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("At least one key of keySize > 0 must be defined."));
        return NULL;
    }
    */
    p_FmPcdCcNode = (t_FmPcdCcNode*)XX_Malloc(sizeof(t_FmPcdCcNode));
    if(!p_FmPcdCcNode)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("No memory"));
        return NULL;
    }
    memset(p_FmPcdCcNode, 0, sizeof(t_FmPcdCcNode));

    p_FmPcdCcNode->p_GlblMask = (t_Handle)XX_Malloc(CC_GLBL_MASK_SIZE * sizeof(uint8_t));
    memset(p_FmPcdCcNode->p_GlblMask, 0, CC_GLBL_MASK_SIZE * sizeof(uint8_t));

    p_FmPcdCcNode->numOfKeys = p_CcNodeParam->keysParams.numOfKeys;

    p_FmPcdCcNode->h_FmPcd = h_FmPcd;

    INIT_LIST(&p_FmPcdCcNode->ccPrevNodesLst);
    INIT_LIST(&p_FmPcdCcNode->ccTreeIdLst);
    INIT_LIST(&p_FmPcdCcNode->ccTreesLst);

    if((p_CcNodeParam->extractCcParams.type == e_FM_PCD_EXTRACT_BY_HDR) &&
        ((p_CcNodeParam->extractCcParams.extractByHdr.hdr == HEADER_TYPE_IPv4) ||
        (p_CcNodeParam->extractCcParams.extractByHdr.hdr == HEADER_TYPE_IPv6)) &&
        (p_CcNodeParam->extractCcParams.extractByHdr.type == e_FM_PCD_EXTRACT_FULL_FIELD) &&
        ((p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fullField.ipv6 == NET_HEADER_FIELD_IPv6_HOP_LIMIT) ||
        (p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fullField.ipv4 == NET_HEADER_FIELD_IPv4_TTL)))
    {
            err = Ipv4TtlOrIpv6HopLimiCheckParams(h_FmPcd, p_CcNodeParam, p_FmPcdCcNode, &isKeyTblAlloc);
            glblMask = FALSE;

    }
    else if((p_CcNodeParam->extractCcParams.type == e_FM_PCD_EXTRACT_NON_HDR) &&
        ((p_CcNodeParam->extractCcParams.extractNonHdr.src == e_FM_PCD_EXTRACT_FROM_KEY) ||
           (p_CcNodeParam->extractCcParams.extractNonHdr.src == e_FM_PCD_EXTRACT_FROM_HASH) ||
           (p_CcNodeParam->extractCcParams.extractNonHdr.src == e_FM_PCD_EXTRACT_FROM_FLOW_ID)))
    {
        if((p_CcNodeParam->extractCcParams.extractNonHdr.src == e_FM_PCD_EXTRACT_FROM_FLOW_ID) &&
            (p_CcNodeParam->extractCcParams.extractNonHdr.offset != 0))
        {
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("In the case of the extraction from e_FM_PCD_EXTRACT_FROM_FLOW_ID offset has to be 0"));
            return NULL;
        }

        icCode = IcDefineCode(p_CcNodeParam);
        fromIc = TRUE;
        if(icCode == CC_PRIVATE_INFO_NONE)
        {
            REPORT_ERROR(MAJOR, E_INVALID_STATE, ("user asked extraction from IC and field in internal context or action wasn't initialized in the right way"));
            return NULL;
        }

        if((icCode == CC_PRIVATE_INFO_IC_DEQ_FQID_INDEX_LOOKUP) || (icCode == CC_PRIVATE_INFO_IC_HASH_INDEX_LOOKUP))
        {
            err = IcHashIndexedCheckParams(h_FmPcd, p_CcNodeParam, p_FmPcdCcNode, &isKeyTblAlloc);

            glblMask = TRUE;
        }
        else
        {
            err = CheckParams(h_FmPcd, p_CcNodeParam,p_FmPcdCcNode, &isKeyTblAlloc);
            if(p_FmPcdCcNode->glblMaskSize)
                glblMask = TRUE;
        }
    }
    else
    {
        err = CheckParams(h_FmPcd, p_CcNodeParam,p_FmPcdCcNode, &isKeyTblAlloc);
        if(p_FmPcdCcNode->glblMaskSize)
            glblMask = TRUE;
    }

    if(err)
    {
        DeleteNode(p_FmPcdCcNode);
        REPORT_ERROR(MAJOR, err, NO_MSG);
        return NULL;
    }

    switch(p_CcNodeParam->extractCcParams.type)
    {
        case(e_FM_PCD_EXTRACT_BY_HDR):
            switch(p_CcNodeParam->extractCcParams.extractByHdr.type)
            {
                case(e_FM_PCD_EXTRACT_FULL_FIELD):
                    p_FmPcdCcNode->parseCode = GetFullFieldParseCode(p_CcNodeParam->extractCcParams.extractByHdr.hdr, p_CcNodeParam->extractCcParams.extractByHdr.hdrIndex,
                                                                    p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fullField);
                    GetSizeHeaderField(p_CcNodeParam->extractCcParams.extractByHdr.hdr, p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fullField, &p_FmPcdCcNode->sizeOfExtraction);
                    fullField = TRUE;
                    if((p_FmPcdCcNode->parseCode != CC_PC_FF_TCI1) && (p_FmPcdCcNode->parseCode != CC_PC_FF_TCI2) &&
                       (p_FmPcdCcNode->parseCode != CC_PC_FF_MPLS1) && (p_FmPcdCcNode->parseCode != CC_PC_FF_MPLS1) &&
                       (p_FmPcdCcNode->parseCode != CC_PC_FF_IPV4IPTOS_TC1) &&  (p_FmPcdCcNode->parseCode != CC_PC_FF_IPV4IPTOS_TC2) &&
                       (p_FmPcdCcNode->parseCode != CC_PC_FF_IPTOS_IPV6TC1_IPV6FLOW1) &&  (p_FmPcdCcNode->parseCode != CC_PC_FF_IPTOS_IPV6TC2_IPV6FLOW2) &&
                       glblMask)
                    {
                        glblMask = FALSE;
                        p_FmPcdCcNode->glblMaskSize = 4;
                        p_FmPcdCcNode->lclMask = TRUE;
                    }
                    break;
                case(e_FM_PCD_EXTRACT_FROM_HDR):
                        p_FmPcdCcNode->sizeOfExtraction = p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fromHdr.size;
                        p_FmPcdCcNode->offset =  p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fromHdr.offset;
                        p_FmPcdCcNode->parseCode = GetPrParseCode(p_CcNodeParam->extractCcParams.extractByHdr.hdr, p_CcNodeParam->extractCcParams.extractByHdr.hdrIndex,
                                                                p_FmPcdCcNode->offset,glblMask, &p_FmPcdCcNode->prsArrayOffset);
                        break;
                case(e_FM_PCD_EXTRACT_FROM_FIELD):
                        p_FmPcdCcNode->offset = p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fromField.offset;
                        p_FmPcdCcNode->sizeOfExtraction = p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fromField.size;
                        p_FmPcdCcNode->parseCode = GetFieldParseCode(p_CcNodeParam->extractCcParams.extractByHdr.hdr, p_CcNodeParam->extractCcParams.extractByHdr.extractByHdrType.fromField.field,
                                                    p_FmPcdCcNode->offset,&p_FmPcdCcNode->prsArrayOffset,
                                                    p_CcNodeParam->extractCcParams.extractByHdr.hdrIndex);
                        break;
                default:
                    DeleteNode(p_FmPcdCcNode);
                    REPORT_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
                    return NULL;
            }
            break;
        case(e_FM_PCD_EXTRACT_NON_HDR):
            /* get the field code for the generic extract */
            p_FmPcdCcNode->sizeOfExtraction = p_CcNodeParam->extractCcParams.extractNonHdr.size;
            p_FmPcdCcNode->offset =  p_CcNodeParam->extractCcParams.extractNonHdr.offset;
            p_FmPcdCcNode->parseCode = GetGenParseCode(p_CcNodeParam->extractCcParams.extractNonHdr.src, p_FmPcdCcNode->offset, glblMask, &p_FmPcdCcNode->prsArrayOffset, fromIc,icCode);

            if(p_FmPcdCcNode->parseCode == CC_PC_GENERIC_IC_HASH_INDEXED)
            {
                if((p_FmPcdCcNode->offset + p_FmPcdCcNode->sizeOfExtraction) > 64)
                {
                     DeleteNode(p_FmPcdCcNode);
                     REPORT_ERROR(MAJOR, E_INVALID_SELECTION,("when node of the type CC_PC_GENERIC_IC_HASH_INDEXED offset + size can not be bigger then size of HASH 64 bits (8 bytes)"));
                     return NULL;
                }
            }
            if((p_FmPcdCcNode->parseCode == CC_PC_GENERIC_IC_GMASK) || (p_FmPcdCcNode->parseCode == CC_PC_GENERIC_IC_HASH_INDEXED))
            {
                p_FmPcdCcNode->offset +=  p_FmPcdCcNode->prsArrayOffset;
                p_FmPcdCcNode->prsArrayOffset = 0;
            }
                break;

       default:
            DeleteNode(p_FmPcdCcNode);
            REPORT_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            return NULL;
    }

    if(p_FmPcdCcNode->parseCode == CC_PC_ILLEGAL)
    {
        DeleteNode(p_FmPcdCcNode);
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("illeagl extraction type"));
        return NULL;
    }

    if((p_FmPcdCcNode->sizeOfExtraction > FM_PCD_MAX_SIZE_OF_KEY) || !p_FmPcdCcNode->sizeOfExtraction)
    {
        DeleteNode(p_FmPcdCcNode);
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("sizeOfExatrction can not be greater than 56 and not 0"));
        return NULL;
    }

    if(p_CcNodeParam->keysParams.keySize != p_FmPcdCcNode->sizeOfExtraction)
    {
        DeleteNode(p_FmPcdCcNode);
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("keySize has to be equal to sizeOfExtraction"));
        return NULL;
    }


    p_FmPcdCcNode->userSizeOfExtraction = p_FmPcdCcNode->sizeOfExtraction;

    if(!glblMask)
        memset(p_FmPcdCcNode->p_GlblMask, 0xff, CC_GLBL_MASK_SIZE  * sizeof(uint8_t));

#ifdef FM_PCD_CC_MANIP
    err = CheckAndSetManipParamsWithCcNodeParams(p_FmPcdCcNode);
    if(err != E_OK)
    {
        DeleteNode(p_FmPcdCcNode);
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("keySize has to be equal to sizeOfExtraction"));
        return NULL;
    }
#endif /* FM_PCD_CC_MANIP */

    GetCcExtractKeySize(p_FmPcdCcNode->sizeOfExtraction, &p_FmPcdCcNode->ccKeySizeAccExtraction);

    if(p_FmPcdCcNode->lclMask)
        size = 2 * p_FmPcdCcNode->ccKeySizeAccExtraction;
    else
        size = p_FmPcdCcNode->ccKeySizeAccExtraction;

    if(isKeyTblAlloc)
    {
        p_FmPcdCcNode->h_KeysMatchTable =(t_Handle)FM_MURAM_AllocMem(FmPcdGetMuramHandle(p_FmPcdCcNode->h_FmPcd),
                                         (uint32_t)(size * sizeof(uint8_t) * (p_FmPcdCcNode->numOfKeys + 1)),
                                         FM_PCD_CC_KEYS_MATCH_TABLE_ALIGN);
        if(!p_FmPcdCcNode->h_KeysMatchTable)
        {
            DeleteNode(p_FmPcdCcNode);
            REPORT_ERROR(MAJOR, E_NO_MEMORY, ("No memory in MURAM for KEY MATCH table"));
            return NULL;
        }
        IOMemSet32((uint8_t *)p_FmPcdCcNode->h_KeysMatchTable, 0, size * sizeof(uint8_t) * (p_FmPcdCcNode->numOfKeys + 1));
    }

    p_FmPcdCcNode->h_AdTable = (t_Handle)FM_MURAM_AllocMem(FmPcdGetMuramHandle(p_FmPcdCcNode->h_FmPcd),
                                     (uint32_t)( (p_FmPcdCcNode->numOfKeys+1) * FM_PCD_CC_AD_ENTRY_SIZE),
                                     FM_PCD_CC_AD_TABLE_ALIGN);
    if(!p_FmPcdCcNode->h_AdTable)
    {
        DeleteNode(p_FmPcdCcNode);
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("No memory in MURAM for AD table "));
        return NULL;
    }
    IOMemSet32((uint8_t *)p_FmPcdCcNode->h_AdTable, 0, (uint32_t)((p_FmPcdCcNode->numOfKeys+1) * FM_PCD_CC_AD_ENTRY_SIZE));

    p_KeysMatchTblTmp    = p_FmPcdCcNode->h_KeysMatchTable;
    p_AdTableTmp         = p_FmPcdCcNode->h_AdTable;
    for(tmp = 0 ; tmp < p_FmPcdCcNode->numOfKeys; tmp++)
    {
        p_KeyParams = &p_CcNodeParam->keysParams.keyParams[tmp];

        if(p_KeysMatchTblTmp)
        {
            Mem2IOCpy32((void*)p_KeysMatchTblTmp, p_KeyParams->p_Key, p_FmPcdCcNode->sizeOfExtraction);

            if(p_FmPcdCcNode->lclMask && p_KeyParams->p_Mask)
                Mem2IOCpy32(PTR_MOVE(p_KeysMatchTblTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), p_KeyParams->p_Mask, p_FmPcdCcNode->sizeOfExtraction);
            else if(p_FmPcdCcNode->lclMask)
                IOMemSet32(PTR_MOVE(p_KeysMatchTblTmp, p_FmPcdCcNode->ccKeySizeAccExtraction), 0xff, p_FmPcdCcNode->sizeOfExtraction);
            p_KeysMatchTblTmp = PTR_MOVE(p_KeysMatchTblTmp, size * sizeof(uint8_t));
        }
        NextStepAd(p_AdTableTmp,&p_KeyParams->ccNextEngineParams, p_FmPcd);

        p_AdTableTmp = PTR_MOVE(p_AdTableTmp, FM_PCD_CC_AD_ENTRY_SIZE);

    }
    NextStepAd(p_AdTableTmp,&p_CcNodeParam->keysParams.ccNextEngineParamsForMiss, p_FmPcd);

    if(fullField == TRUE)
        p_FmPcdCcNode->sizeOfExtraction = 0;


    for(tmp = 0; tmp < p_FmPcdCcNode->numOfKeys + 1; tmp++)
    {
        if(p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].nextEngineParams.nextEngine == e_FM_PCD_CC)
        {
            p_FmPcdCcNextNode = (t_FmPcdCcNode*)p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].nextEngineParams.params.ccParams.h_CcNode;

            if(!IsNodeInModifiedState((t_Handle)p_FmPcdCcNextNode))
            {
                memset(&ccNodeInfo, 0, sizeof(t_CcNodeInformation));
                ccNodeInfo.h_CcNode = (t_Handle)p_FmPcdCcNode;
                ccNodeInfo.index = 1;
                EnqueueNodeInfoToRelevantLst(&p_FmPcdCcNextNode->ccPrevNodesLst, &ccNodeInfo);
                UpdateNodeWithModifiedState((t_Handle)p_FmPcdCcNextNode, TRUE);
            }
            else
            {
                p_CcInformation = FindNodeInfoInReleventLst(&p_FmPcdCcNextNode->ccPrevNodesLst,(t_Handle)p_FmPcdCcNode);
                ASSERT_COND(p_CcInformation);
                p_CcInformation->index++;
            }
        }

    }

    for(tmp = 0; tmp < p_FmPcdCcNode->numOfKeys + 1; tmp++)
    {
        if(p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].nextEngineParams.nextEngine == e_FM_PCD_CC)
        {
            p_FmPcdCcNextNode = (t_FmPcdCcNode*)p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].nextEngineParams.params.ccParams.h_CcNode;

            if(IsNodeInModifiedState((t_Handle)p_FmPcdCcNextNode))
                UpdateNodeWithModifiedState((t_Handle)p_FmPcdCcNextNode, FALSE);
        }
    }

    p_AdTableTmp   = p_FmPcdCcNode->h_AdTable;
    for(tmp = 0; tmp < p_FmPcdCcNode->numOfKeys; tmp++)
    {
        if(p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].requiredAction)
        {

             err = FmPcdCcSetRequiredAction(h_FmPcd, p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].requiredAction, &p_FmPcdCcNode->nextEngineAndRequiredAction[tmp], p_AdTableTmp,1, NULL);
            if(err)
            {
                FM_PCD_CcDeleteNode(h_FmPcd, (t_Handle)p_FmPcdCcNode);
                REPORT_ERROR(MAJOR, err, NO_MSG);
                return NULL;
            }
            p_AdTableTmp = PTR_MOVE(p_AdTableTmp, FM_PCD_CC_AD_ENTRY_SIZE);
        }
    }
    if(p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].requiredAction)
    {
         err = FmPcdCcSetRequiredAction(h_FmPcd, p_FmPcdCcNode->nextEngineAndRequiredAction[tmp].requiredAction, &p_FmPcdCcNode->nextEngineAndRequiredAction[tmp], p_AdTableTmp,1, NULL);
         if(err)
        {
            FM_PCD_CcDeleteNode(h_FmPcd, (t_Handle)p_FmPcdCcNode);
            REPORT_ERROR(MAJOR, err, NO_MSG);
            return NULL;
        }

    }


    return p_FmPcdCcNode;
}

t_Error FM_PCD_CcDeleteNode(t_Handle h_FmPcd, t_Handle h_CcNode)
{
    t_FmPcdCcNode               *p_CcNode = (t_FmPcdCcNode *)h_CcNode;
    int i = 0;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);

    UNUSED(h_FmPcd);
    if(!p_CcNode)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("the node with this ID is not initialized"));

    if(p_CcNode->owners)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("the node with this ID can not be removed because this node is occupied, first - unbind this node"));

   for(i = 0; i < p_CcNode->numOfKeys; i++)
   {
        if(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.nextEngine == e_FM_PCD_CC)
            UpdateNodeOwner(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.params.ccParams.h_CcNode, FALSE);

    }
    if(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.nextEngine == e_FM_PCD_CC)
        UpdateNodeOwner(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.params.ccParams.h_CcNode, FALSE);

#ifdef FM_PCD_CC_MANIP
    for(i = 0; i < p_CcNode->numOfKeys; i++)
    {
        if(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip)
            FmPcdManipUpdateOwner(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip, FALSE);
    }
    if(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip)
        FmPcdManipUpdateOwner(p_CcNode->nextEngineAndRequiredAction[i].nextEngineParams.h_Manip, FALSE);
#endif /* FM_PCD_CC_MANIP */

   DeleteNode(p_CcNode);

    return E_OK;
}

t_Error FM_PCD_CcNodeAddKey(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams  *p_KeyParams)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->h_Hc, E_INVALID_HANDLE);

    return FmHcPcdCcAddKey(p_FmPcd->h_Hc, h_CcNode, keyIndex, keySize, p_KeyParams);
}

t_Error FM_PCD_CcNodeRemoveKey(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->h_Hc, E_INVALID_HANDLE);

    return FmHcPcdCcRemoveKey(p_FmPcd->h_Hc, h_CcNode, keyIndex);
}

t_Error FM_PCD_CcNodeModifyKey(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, uint8_t  *p_Key, uint8_t *p_Mask)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->h_Hc, E_INVALID_HANDLE);

    return FmHcPcdCcModifyKey(p_FmPcd->h_Hc, h_CcNode, keyIndex, keySize, p_Key, p_Mask);
}

t_Error FM_PCD_CcNodeModifyNextEngine(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->h_Hc, E_INVALID_HANDLE);

    return FmHcPcdCcModifyNodeNextEngine(p_FmPcd->h_Hc, h_CcNode, keyIndex, p_FmPcdCcNextEngineParams);
}

t_Error FM_PCD_CcNodeModifyMissNextEngine(t_Handle h_FmPcd, t_Handle h_CcNode, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->h_Hc, E_INVALID_HANDLE);

    return FmHcPcdCcModifyNodeMissNextEngine(p_FmPcd->h_Hc, h_CcNode, p_FmPcdCcNextEngineParams);
}

t_Error FM_PCD_CcTreeModifyNextEngine(t_Handle h_FmPcd, t_Handle h_CcTree, uint8_t grpId, uint8_t index, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->h_Hc, E_INVALID_HANDLE);

    return FmHcPcdCcModifyTreeNextEngine(p_FmPcd->h_Hc, h_CcTree, grpId, index, p_FmPcdCcNextEngineParams);
}

t_Error FM_PCD_CcNodeModifyKeyAndNextEngine(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams  *p_KeyParams)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->h_Hc, E_INVALID_HANDLE);

    return FmHcPcdCcModifyKeyAndNextEngine(p_FmPcd->h_Hc, h_CcNode, keyIndex, keySize, p_KeyParams);
}

uint32_t FM_PCD_CcNodeGetKeyCounter(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex)
{
    t_FmPcdCcNode       *p_FmPcdCcNode = (t_FmPcdCcNode *)h_CcNode;
    t_AdOfTypeResult    *p_AdResult = NULL;

    SANITY_CHECK_RETURN_ERROR(h_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_VALUE(h_CcNode, E_INVALID_HANDLE, 0);
#ifdef DISABLE_SANITY_CHECKS
UNUSED(h_FmPcd);
#endif /* DISABLE_SANITY_CHECKS */

    if (keyIndex >= p_FmPcdCcNode->numOfKeys)
    {
        REPORT_ERROR(MINOR, E_INVALID_STATE,
                     ("keyIndex > numOfKeys defined for this node"));
        return 0;
    }

    p_AdResult = PTR_MOVE(p_FmPcdCcNode->h_AdTable, keyIndex*FM_PCD_CC_AD_ENTRY_SIZE);
    ASSERT_COND(p_AdResult);

    if (p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_CC)
    {
        REPORT_ERROR(MINOR, E_INVALID_STATE,
                     ("statistics updated only for entries where next engine not CC"));
        return 0;
    }

    if(((p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_DONE) &&
        !p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.params.enqueueParams.statisticsEn) ||
        ((p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_KG) &&
        !p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.params.kgParams.statisticsEn) ||
        ((p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.nextEngine == e_FM_PCD_PLCR) &&
        !p_FmPcdCcNode->nextEngineAndRequiredAction[keyIndex].nextEngineParams.params.plcrParams.statisticsEn))
    {
        REPORT_ERROR(MINOR, E_INVALID_STATE,
                     ("statistics wasn't enable"));
        return 0;
    }

    return  GET_UINT32(p_AdResult->res);
}
