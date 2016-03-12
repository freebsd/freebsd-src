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

#include "std_ext.h"
#include "error_ext.h"
#include "sprint_ext.h"
#include "string_ext.h"

#include "fm_common.h"
#include "fm_hc.h"


#define HC_HCOR_OPCODE_PLCR_PRFL                                0x0
#define HC_HCOR_OPCODE_KG_SCM                                   0x1
#define HC_HCOR_OPCODE_SYNC                                     0x2
#define HC_HCOR_OPCODE_CC                                       0x3
#define HC_HCOR_OPCODE_CC_CAPWAP_REASSM_TIMEOUT                 0x5

#define HC_HCOR_GBL                         0x20000000

#define SIZE_OF_HC_FRAME_PORT_REGS          (sizeof(t_HcFrame)-sizeof(t_FmPcdKgInterModuleSchemeRegs)+sizeof(t_FmPcdKgPortRegs))
#define SIZE_OF_HC_FRAME_SCHEME_REGS        sizeof(t_HcFrame)
#define SIZE_OF_HC_FRAME_PROFILES_REGS      (sizeof(t_HcFrame)-sizeof(t_FmPcdKgInterModuleSchemeRegs)+sizeof(t_FmPcdPlcrInterModuleProfileRegs))
#define SIZE_OF_HC_FRAME_PROFILE_CNT        (sizeof(t_HcFrame)-sizeof(t_FmPcdPlcrInterModuleProfileRegs)+sizeof(uint32_t))
#define SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC 16

#define BUILD_FD(len)                     \
do {                                      \
    memset(&fmFd, 0, sizeof(t_DpaaFD));   \
    DPAA_FD_SET_ADDR(&fmFd, p_HcFrame);    \
    DPAA_FD_SET_OFFSET(&fmFd, 0);         \
    DPAA_FD_SET_LENGTH(&fmFd, len);       \
} while (0)


#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

/**************************************************************************//**
 @Description   PCD KG scheme registers
*//***************************************************************************/
typedef _Packed struct t_FmPcdKgSchemeRegsWithoutCounter {
    volatile uint32_t kgse_mode;    /**< MODE */
    volatile uint32_t kgse_ekfc;    /**< Extract Known Fields Command */
    volatile uint32_t kgse_ekdv;    /**< Extract Known Default Value */
    volatile uint32_t kgse_bmch;    /**< Bit Mask Command High */
    volatile uint32_t kgse_bmcl;    /**< Bit Mask Command Low */
    volatile uint32_t kgse_fqb;     /**< Frame Queue Base */
    volatile uint32_t kgse_hc;      /**< Hash Command */
    volatile uint32_t kgse_ppc;     /**< Policer Profile Command */
    volatile uint32_t kgse_gec[FM_PCD_KG_NUM_OF_GENERIC_REGS];
                                    /**< Generic Extract Command */
    volatile uint32_t kgse_dv0;     /**< KeyGen Scheme Entry Default Value 0 */
    volatile uint32_t kgse_dv1;     /**< KeyGen Scheme Entry Default Value 1 */
    volatile uint32_t kgse_ccbs;    /**< KeyGen Scheme Entry Coarse Classification Bit*/
    volatile uint32_t kgse_mv;      /**< KeyGen Scheme Entry Match vector */
} _PackedType t_FmPcdKgSchemeRegsWithoutCounter;

typedef _Packed struct t_FmPcdKgPortRegs {
    volatile uint32_t                       spReg;
    volatile uint32_t                       cppReg;
} _PackedType t_FmPcdKgPortRegs;

typedef _Packed struct t_HcFrame {
    volatile uint32_t                           opcode;
    volatile uint32_t                           actionReg;
    volatile uint32_t                           extraReg;
    volatile uint32_t                           commandSequence;
    union {
        t_FmPcdKgInterModuleSchemeRegs          schemeRegs;
        t_FmPcdKgInterModuleSchemeRegs          schemeRegsWithoutCounter;
        t_FmPcdPlcrInterModuleProfileRegs       profileRegs;
        volatile uint32_t                       singleRegForWrite;    /* for writing SP, CPP, profile counter */
        t_FmPcdKgPortRegs                       portRegsForRead;
        volatile uint32_t                       clsPlanEntries[CLS_PLAN_NUM_PER_GRP];
        t_FmPcdCcCapwapReassmTimeoutParams      ccCapwapReassmTimeout;
    } hcSpecificData;
} _PackedType t_HcFrame;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


typedef struct t_FmHc {
    t_Handle                    h_FmPcd;
    t_Handle                    h_HcPortDev;
    t_FmPcdQmEnqueueCallback    *f_QmEnqueue;     /**< A callback for enqueuing frames to the QM */
    t_Handle                    h_QmArg;          /**< A handle to the QM module */
    uint8_t                     padTill16;

    uint32_t                    seqNum;
    volatile bool               wait[32];
} t_FmHc;


static __inline__ t_Error EnQFrm(t_FmHc *p_FmHc, t_DpaaFD *p_FmFd, volatile uint32_t *p_SeqNum)
{
    t_Error     err = E_OK;
    uint32_t    savedSeqNum;
    uint32_t    intFlags;
    uint32_t    timeout=100;

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);
    *p_SeqNum = p_FmHc->seqNum;
    savedSeqNum = p_FmHc->seqNum;
    p_FmHc->seqNum = (uint32_t)((p_FmHc->seqNum+1)%32);
    ASSERT_COND(!p_FmHc->wait[savedSeqNum]);
    p_FmHc->wait[savedSeqNum] = TRUE;
    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
    DBG(TRACE, ("Send Hc, SeqNum %d, FD@0x%x, fd offset 0x%x",
                savedSeqNum,DPAA_FD_GET_ADDR(p_FmFd),DPAA_FD_GET_OFFSET(p_FmFd)));
    err = p_FmHc->f_QmEnqueue(p_FmHc->h_QmArg, (void *)p_FmFd);
    if(err)
        RETURN_ERROR(MINOR, err, ("HC enqueue failed"));

    while (p_FmHc->wait[savedSeqNum] && --timeout)
        XX_UDelay(100);

    if (!timeout)
        RETURN_ERROR(MINOR, E_TIMEOUT, ("HC Callback, timeout exceeded"));

    return err;
}

static t_Error CcHcDoDynamicChange(t_FmHc *p_FmHc, t_Handle p_OldPointer, t_Handle p_NewPointer)
{
    t_HcFrame               *p_HcFrame;
    t_DpaaFD                fmFd;
    t_Error                 err = E_OK;

    ASSERT_COND(p_FmHc);

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));

    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_CC);
    p_HcFrame->actionReg  = FmPcdCcGetNodeAddrOffsetFromNodeInfo(p_FmHc->h_FmPcd, p_NewPointer);
    if(p_HcFrame->actionReg == (uint32_t)ILLEGAL_BASE)
    {
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Something wrong with base address"));
    }

    p_HcFrame->actionReg  |=  0xc0000000;
        p_HcFrame->extraReg   = FmPcdCcGetNodeAddrOffsetFromNodeInfo(p_FmHc->h_FmPcd, p_OldPointer);
    if(p_HcFrame->extraReg == (uint32_t)ILLEGAL_BASE)
    {
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Something wrong with base address"));
    }

    BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    XX_FreeSmart(p_HcFrame);

    return E_OK;
}

static t_Error HcDynamicChange(t_FmHc *p_FmHc,t_List *h_OldPointersLst, t_List *h_NewPointersLst, t_Handle *h_Params)
{

    t_List      *p_PosOld, *p_PosNew;
    uint16_t    i = 0;
    t_Error     err = E_OK;
    uint8_t     numOfModifiedPtr;

    SANITY_CHECK_RETURN_ERROR((LIST_NumOfObjs(h_NewPointersLst) == LIST_NumOfObjs(h_OldPointersLst)),E_INVALID_STATE);

    numOfModifiedPtr = (uint8_t)LIST_NumOfObjs(h_NewPointersLst);
    p_PosNew = NCSW_LIST_FIRST(h_NewPointersLst);
    p_PosOld = NCSW_LIST_FIRST(h_OldPointersLst);
    for(i = 0; i < numOfModifiedPtr; i++)
    {
        err = CcHcDoDynamicChange(p_FmHc, p_PosOld, p_PosNew);
        if(err)
        {
            FmPcdCcReleaseModifiedDataStructure(p_FmHc->h_FmPcd, h_OldPointersLst, h_NewPointersLst, i, h_Params);
            RETURN_ERROR(MAJOR, err, ("For part of nodes changes are done - situation is danger"));
        }
        p_PosNew = NCSW_LIST_NEXT(p_PosNew);
        p_PosOld = NCSW_LIST_NEXT(p_PosOld);
    }

    err = FmPcdCcReleaseModifiedDataStructure(p_FmHc->h_FmPcd, h_OldPointersLst, h_NewPointersLst, i, h_Params);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    return E_OK;
}


t_Handle FmHcConfigAndInit(t_FmHcParams *p_FmHcParams)
{
    t_FmHc          *p_FmHc;
    t_FmPortParams  fmPortParam;
    t_Error         err = E_OK;

    p_FmHc = (t_FmHc *)XX_Malloc(sizeof(t_FmHc));
    if (!p_FmHc)
    {
        REPORT_ERROR(MINOR, E_NO_MEMORY, ("HC obj"));
        return NULL;
    }
    memset(p_FmHc,0,sizeof(t_FmHc));

    p_FmHc->h_FmPcd             = p_FmHcParams->h_FmPcd;
    p_FmHc->f_QmEnqueue         = p_FmHcParams->params.f_QmEnqueue;
    p_FmHc->h_QmArg             = p_FmHcParams->params.h_QmArg;

    if (!FmIsMaster(p_FmHcParams->h_Fm))
        return (t_Handle)p_FmHc;

/*
TKT056919 - axi12axi0 can hang if read request follows the single byte write on the very next cycle
TKT038900 - FM dma lockup occur due to AXI slave protocol violation
*/
#ifdef FM_LOCKUP_ALIGNMENT_ERRATA_FMAN_SW004
    p_FmHc->padTill16 = 16 - (sizeof(t_FmHc) % 16);
#endif /* FM_LOCKUP_ALIGNMENT_ERRATA_FMAN_SW004 */
    memset(&fmPortParam, 0, sizeof(fmPortParam));
    fmPortParam.baseAddr    = p_FmHcParams->params.portBaseAddr;
    fmPortParam.portType    = e_FM_PORT_TYPE_OH_HOST_COMMAND;
    fmPortParam.portId      = p_FmHcParams->params.portId;
    fmPortParam.liodnBase   = p_FmHcParams->params.liodnBase;
    fmPortParam.h_Fm        = p_FmHcParams->h_Fm;

    fmPortParam.specificParams.nonRxParams.errFqid      = p_FmHcParams->params.errFqid;
    fmPortParam.specificParams.nonRxParams.dfltFqid     = p_FmHcParams->params.confFqid;
    fmPortParam.specificParams.nonRxParams.qmChannel    = p_FmHcParams->params.qmChannel;

    p_FmHc->h_HcPortDev = FM_PORT_Config(&fmPortParam);
    if(!p_FmHc->h_HcPortDev)
    {
        REPORT_ERROR(MAJOR, E_INVALID_HANDLE, ("FM HC port!"));
        XX_Free(p_FmHc);
        return NULL;
    }

    /* final init */
    if ((err = FM_PORT_Init(p_FmHc->h_HcPortDev)) != E_OK)
    {
        REPORT_ERROR(MAJOR, err, ("FM HC port!"));
        FmHcFree(p_FmHc);
        return NULL;
    }

    if ((err = FM_PORT_Enable(p_FmHc->h_HcPortDev)) != E_OK)
    {
        REPORT_ERROR(MAJOR, err, ("FM HC port!"));
        FmHcFree(p_FmHc);
        return NULL;
    }

    return (t_Handle)p_FmHc;
}

void FmHcFree(t_Handle h_FmHc)
{
    t_FmHc  *p_FmHc = (t_FmHc*)h_FmHc;

    if (!p_FmHc)
        return;

    if (p_FmHc->h_HcPortDev)
        FM_PORT_Free(p_FmHc->h_HcPortDev);

    XX_Free(p_FmHc);
}

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FmHcDumpRegs(t_Handle h_FmHc)
{
    t_FmHc  *p_FmHc = (t_FmHc*)h_FmHc;

    SANITY_CHECK_RETURN_ERROR(p_FmHc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmHc->h_HcPortDev, E_INVALID_HANDLE);

    return  FM_PORT_DumpRegs(p_FmHc->h_HcPortDev);

}
#endif /* (defined(DEBUG_ERRORS) && ... */

void FmHcTxConf(t_Handle h_FmHc, t_DpaaFD *p_Fd)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_HcFrame   *p_HcFrame;
    uint32_t    intFlags;

    ASSERT_COND(p_FmHc);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);
    p_HcFrame  = (t_HcFrame *)PTR_MOVE(DPAA_FD_GET_ADDR(p_Fd), DPAA_FD_GET_OFFSET(p_Fd));

    DBG(TRACE, ("Hc Conf, SeqNum %d, FD@0x%x, fd offset 0x%x",
                p_HcFrame->commandSequence, DPAA_FD_GET_ADDR(p_Fd), DPAA_FD_GET_OFFSET(p_Fd)));

    if (!(p_FmHc->wait[p_HcFrame->commandSequence]))
        REPORT_ERROR(MINOR, E_INVALID_FRAME, ("Not an Host-Command frame received!"));
    else
        p_FmHc->wait[p_HcFrame->commandSequence] = FALSE;
    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
}

t_Handle FmHcPcdKgSetScheme(t_Handle h_FmHc, t_FmPcdKgSchemeParams *p_Scheme)
{
    t_FmHc                              *p_FmHc = (t_FmHc*)h_FmHc;
    t_Error                             err = E_OK;
    t_FmPcdKgInterModuleSchemeRegs      schemeRegs;
    t_HcFrame                           *p_HcFrame;
    t_DpaaFD                            fmFd;
    uint32_t                            intFlags;
    uint8_t                             physicalSchemeId, relativeSchemeId;

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
    {
        REPORT_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
        return NULL;
    }

    if(!p_Scheme->modify)
    {
        /* check that schemeId is in range */
        if(p_Scheme->id.relativeSchemeId >= FmPcdKgGetNumOfPartitionSchemes(p_FmHc->h_FmPcd))
        {
            REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, ("Scheme is out of range"));
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }

        relativeSchemeId = p_Scheme->id.relativeSchemeId;

        if (FmPcdKgSchemeTryLock(p_FmHc->h_FmPcd, relativeSchemeId, FALSE))
        {
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }

        physicalSchemeId = FmPcdKgGetPhysicalSchemeId(p_FmHc->h_FmPcd, relativeSchemeId);

        memset(p_HcFrame, 0, sizeof(t_HcFrame));
        p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
        p_HcFrame->actionReg  = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
        p_HcFrame->extraReg = 0xFFFFF800;

        BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

        if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
        {
            FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
            REPORT_ERROR(MINOR, err, NO_MSG);
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }

        /* check if this scheme is already used */
        if (FmPcdKgHwSchemeIsValid(p_HcFrame->hcSpecificData.schemeRegs.kgse_mode))
        {
            FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
            REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("Scheme is already used"));
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }
    }
    else
    {
        intFlags = FmPcdLock(p_FmHc->h_FmPcd);
        physicalSchemeId = (uint8_t)(PTR_TO_UINT(p_Scheme->id.h_Scheme)-1);
        relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmHc->h_FmPcd, physicalSchemeId);
        if( relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
        {
            FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
            REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }
        err = FmPcdKgSchemeTryLock(p_FmHc->h_FmPcd, relativeSchemeId, TRUE);
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        if (err)
        {
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }
    }

    err = FmPcdKgBuildScheme(p_FmHc->h_FmPcd, p_Scheme, &schemeRegs);
    if(err)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        REPORT_ERROR(MAJOR, err, NO_MSG);
        XX_FreeSmart(p_HcFrame);
        return NULL;
    }

    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
    p_HcFrame->actionReg  = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, p_Scheme->schemeCounter.update);
    p_HcFrame->extraReg = 0xFFFFF800;
    memcpy(&p_HcFrame->hcSpecificData.schemeRegs, &schemeRegs, sizeof(t_FmPcdKgInterModuleSchemeRegs));
    if(!p_Scheme->schemeCounter.update)
    {
        p_HcFrame->hcSpecificData.schemeRegs.kgse_dv0   = schemeRegs.kgse_dv0;
        p_HcFrame->hcSpecificData.schemeRegs.kgse_dv1   = schemeRegs.kgse_dv1;
        p_HcFrame->hcSpecificData.schemeRegs.kgse_ccbs  = schemeRegs.kgse_ccbs;
        p_HcFrame->hcSpecificData.schemeRegs.kgse_mv    = schemeRegs.kgse_mv;
    }

    BUILD_FD(sizeof(t_HcFrame));

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        REPORT_ERROR(MINOR, err, NO_MSG);
        XX_FreeSmart(p_HcFrame);
        return NULL;
    }

    FmPcdKgValidateSchemeSw(p_FmHc->h_FmPcd, relativeSchemeId);

    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);

    XX_FreeSmart(p_HcFrame);

    return (t_Handle)(UINT_TO_PTR(physicalSchemeId + 1));
}

t_Error FmHcPcdKgDeleteScheme(t_Handle h_FmHc, t_Handle h_Scheme)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Error     err = E_OK;
    t_HcFrame   *p_HcFrame;
    t_DpaaFD    fmFd;
    uint8_t     relativeSchemeId;
    uint8_t     physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);

    relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmHc->h_FmPcd, physicalSchemeId);

    if ((err = FmPcdKgSchemeTryLock(p_FmHc->h_FmPcd, relativeSchemeId, FALSE)) != E_OK)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    if(relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);
    }

    err = FmPcdKgCheckInvalidateSchemeSw(p_FmHc->h_FmPcd, relativeSchemeId);
    if (err)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
    }
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
    p_HcFrame->actionReg  = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, TRUE);
    p_HcFrame->extraReg = 0xFFFFF800;
    memset(&p_HcFrame->hcSpecificData.schemeRegs, 0, sizeof(t_FmPcdKgInterModuleSchemeRegs));

    BUILD_FD(sizeof(t_HcFrame));

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    FmPcdKgInvalidateSchemeSw(p_FmHc->h_FmPcd, relativeSchemeId);

    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);

    XX_FreeSmart(p_HcFrame);

    return E_OK;
}

t_Error FmHcPcdKgCcGetSetParams(t_Handle h_FmHc, t_Handle  h_Scheme, uint32_t requiredAction)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Error     err = E_OK;
    t_HcFrame   *p_HcFrame;
    t_DpaaFD    fmFd;
    uint8_t     relativeSchemeId;
    uint8_t     physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);
    uint32_t    tmpReg32 = 0;

    relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmHc->h_FmPcd, physicalSchemeId);
    if( relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
        RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

    if (FmPcdKgSchemeTryLock(p_FmHc->h_FmPcd, relativeSchemeId, FALSE))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Lock of the scheme FAILED"));

    if(!FmPcdKgGetPointedOwners(p_FmHc->h_FmPcd, relativeSchemeId) ||
       !(FmPcdKgGetRequiredAction(p_FmHc->h_FmPcd, relativeSchemeId) & requiredAction))
    {

        if(requiredAction & UPDATE_NIA_ENQ_WITHOUT_DMA)
        {
            if((FmPcdKgGetNextEngine(p_FmHc->h_FmPcd, relativeSchemeId) == e_FM_PCD_DONE) && (FmPcdKgGetDoneAction(p_FmHc->h_FmPcd, relativeSchemeId) ==  e_FM_PCD_ENQ_FRAME))

            {
                p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
                if (!p_HcFrame)
                {
                    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
                    RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
                }
                memset(p_HcFrame, 0, sizeof(t_HcFrame));
                p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
                p_HcFrame->actionReg  = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
                p_HcFrame->extraReg = 0xFFFFF800;
                BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);
                if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
                {
                    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
                    XX_FreeSmart(p_HcFrame);
                    RETURN_ERROR(MINOR, err, NO_MSG);
                }

                /* check if this scheme is already used */
                if (!FmPcdKgHwSchemeIsValid(p_HcFrame->hcSpecificData.schemeRegs.kgse_mode))
                {
                    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
                    XX_FreeSmart(p_HcFrame);
                    RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, ("Scheme is already used"));
                }
                tmpReg32 = p_HcFrame->hcSpecificData.schemeRegs.kgse_mode;

                ASSERT_COND(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME));

                p_HcFrame->hcSpecificData.schemeRegs.kgse_mode =  tmpReg32 | NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;

                p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
                p_HcFrame->actionReg  = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, FALSE);
                p_HcFrame->extraReg = 0x80000000;

                BUILD_FD(sizeof(t_HcFrame));

                if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
                {
                    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
                    XX_FreeSmart(p_HcFrame);
                    RETURN_ERROR(MINOR, err, NO_MSG);
                }

                XX_FreeSmart(p_HcFrame);
            }
            else if (FmPcdKgGetNextEngine(p_FmHc->h_FmPcd, relativeSchemeId) == e_FM_PCD_PLCR)
            {

                if((FmPcdKgIsDirectPlcr(p_FmHc->h_FmPcd, relativeSchemeId) == FALSE) ||
                    (FmPcdKgIsDistrOnPlcrProfile(p_FmHc->h_FmPcd, relativeSchemeId) == TRUE))
                 {
                    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
                    RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("In this situation PP can not be with distribution and has to be shared"));
                 }
                err = FmPcdPlcrCcGetSetParams(p_FmHc->h_FmPcd, FmPcdKgGetRelativeProfileId(p_FmHc->h_FmPcd, relativeSchemeId), requiredAction);
                if(err)
                {
                    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
                    RETURN_ERROR(MAJOR, err, NO_MSG);
                }
        }
      }
    }

    FmPcdKgUpatePointedOwner(p_FmHc->h_FmPcd, relativeSchemeId,TRUE);
    FmPcdKgUpdateRequiredAction(p_FmHc->h_FmPcd, relativeSchemeId,requiredAction);
    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);

    return E_OK;
}

uint32_t  FmHcPcdKgGetSchemeCounter(t_Handle h_FmHc, t_Handle h_Scheme)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Error     err = E_OK;
    t_HcFrame   *p_HcFrame;
    t_DpaaFD    fmFd;
    uint32_t    retVal;
    uint8_t     relativeSchemeId;
    uint8_t     physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);

    relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmHc->h_FmPcd, physicalSchemeId);
    if( relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
    {
        REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);
        return 0;
    }

    if ((err = FmPcdKgSchemeTryLock(p_FmHc->h_FmPcd, relativeSchemeId, FALSE)) != E_OK)
    {
        REPORT_ERROR(MAJOR, err, ("Scheme lock"));
        return 0;
    }

    /* first read scheme and check that it is valid */
    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
    {
        REPORT_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
        return 0;
    }
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
    p_HcFrame->actionReg  = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
    p_HcFrame->extraReg = 0xFFFFF800;

    BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        REPORT_ERROR(MINOR, err, NO_MSG);
        XX_FreeSmart(p_HcFrame);
        return 0;
    }

    if (!FmPcdKgHwSchemeIsValid(p_HcFrame->hcSpecificData.schemeRegs.kgse_mode))
    {
        REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("Scheme is invalid"));
        XX_FreeSmart(p_HcFrame);
        return 0;
    }

    retVal = p_HcFrame->hcSpecificData.schemeRegs.kgse_spc;

    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);

    XX_FreeSmart(p_HcFrame);

    return retVal;
}

t_Error  FmHcPcdKgSetSchemeCounter(t_Handle h_FmHc, t_Handle h_Scheme, uint32_t value)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Error     err = E_OK;
    t_HcFrame   *p_HcFrame;
    t_DpaaFD    fmFd;
    uint8_t     relativeSchemeId, physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);

    relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmHc->h_FmPcd, physicalSchemeId);
    if( relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
        RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

    if ((err = FmPcdKgSchemeTryLock(p_FmHc->h_FmPcd, relativeSchemeId, FALSE)) != E_OK)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    /* first read scheme and check that it is valid */
    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
    p_HcFrame->actionReg  = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
    p_HcFrame->extraReg = 0xFFFFF800;

    BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    /* check that scheme is valid */
    if (!FmPcdKgHwSchemeIsValid(p_HcFrame->hcSpecificData.schemeRegs.kgse_mode))
    {
        FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, ("Scheme is invalid"));
    }

    /* Write scheme back, with modified counter */
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
    p_HcFrame->actionReg  = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, TRUE);
    p_HcFrame->extraReg = 0xFFFFF800;
    /* write counter */
    p_HcFrame->hcSpecificData.schemeRegs.kgse_spc = value;

    BUILD_FD(sizeof(t_HcFrame));

    err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence);

    FmPcdKgReleaseSchemeLock(p_FmHc->h_FmPcd, relativeSchemeId);
    XX_FreeSmart(p_HcFrame);

    return err;
}

t_Error FmHcPcdKgSetClsPlan(t_Handle h_FmHc, t_FmPcdKgInterModuleClsPlanSet *p_Set)
{
    t_FmHc                  *p_FmHc = (t_FmHc*)h_FmHc;
    t_HcFrame               *p_HcFrame;
    t_DpaaFD                fmFd;
    uint32_t                i;
    t_Error                 err = E_OK;

    ASSERT_COND(p_FmHc);

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));

    for(i=p_Set->baseEntry;i<p_Set->baseEntry+p_Set->numOfClsPlanEntries;i+=8)
    {
        memset(p_HcFrame, 0, sizeof(t_HcFrame));
        p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
        p_HcFrame->actionReg  = FmPcdKgBuildWriteClsPlanBlockActionReg((uint8_t)(i / CLS_PLAN_NUM_PER_GRP));
        p_HcFrame->extraReg = 0xFFFFF800;
        memcpy((void*)&p_HcFrame->hcSpecificData.clsPlanEntries, (void *)&p_Set->vectors[i-p_Set->baseEntry], CLS_PLAN_NUM_PER_GRP*sizeof(uint32_t));

        BUILD_FD(sizeof(t_HcFrame));

        if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
        {
            XX_FreeSmart(p_HcFrame);
            RETURN_ERROR(MINOR, err, NO_MSG);
        }
    }
    XX_FreeSmart(p_HcFrame);

    return err;
}

t_Error FmHcPcdKgDeleteClsPlan(t_Handle h_FmHc, uint8_t  grpId)
{
    t_FmHc                              *p_FmHc = (t_FmHc*)h_FmHc;
    t_FmPcdKgInterModuleClsPlanSet      *p_ClsPlanSet;

    /* clear clsPlan entries in memory */
    p_ClsPlanSet = (t_FmPcdKgInterModuleClsPlanSet *)XX_Malloc(sizeof(t_FmPcdKgInterModuleClsPlanSet));
    if (!p_ClsPlanSet)
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("memory allocation failed for p_ClsPlanSetd"));
    memset(p_ClsPlanSet, 0, sizeof(t_FmPcdKgInterModuleClsPlanSet));

    p_ClsPlanSet->baseEntry = FmPcdKgGetClsPlanGrpBase(p_FmHc->h_FmPcd, grpId);
    p_ClsPlanSet->numOfClsPlanEntries = FmPcdKgGetClsPlanGrpSize(p_FmHc->h_FmPcd, grpId);
    ASSERT_COND(p_ClsPlanSet->numOfClsPlanEntries <= FM_PCD_MAX_NUM_OF_CLS_PLANS);

    if (FmHcPcdKgSetClsPlan(p_FmHc, p_ClsPlanSet) != E_OK)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
    XX_Free(p_ClsPlanSet);

    FmPcdKgDestroyClsPlanGrp(p_FmHc->h_FmPcd, grpId);

    return E_OK;
}

t_Error FmHcPcdCcCapwapTimeoutReassm(t_Handle h_FmHc, t_FmPcdCcCapwapReassmTimeoutParams *p_CcCapwapReassmTimeoutParams )
{
    t_FmHc                              *p_FmHc = (t_FmHc*)h_FmHc;
    t_HcFrame                           *p_HcFrame;
    uint32_t                            intFlags;
    t_DpaaFD                            fmFd;
    t_Error                             err;

    SANITY_CHECK_RETURN_VALUE(h_FmHc, E_INVALID_HANDLE,0);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);
    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_CC_CAPWAP_REASSM_TIMEOUT);
    memcpy(&p_HcFrame->hcSpecificData.ccCapwapReassmTimeout, p_CcCapwapReassmTimeoutParams, sizeof(t_FmPcdCcCapwapReassmTimeoutParams));
    BUILD_FD(sizeof(t_HcFrame));

    err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence);

    XX_FreeSmart(p_HcFrame);
    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
    return err;
}


t_Error FmHcPcdPlcrCcGetSetParams(t_Handle h_FmHc,uint16_t absoluteProfileId, uint32_t requiredAction)
{
    t_FmHc              *p_FmHc = (t_FmHc*)h_FmHc;
    t_HcFrame           *p_HcFrame;
    t_DpaaFD            fmFd;
    t_Error             err;
    uint32_t            tmpReg32 = 0;
    uint32_t            requiredActionTmp, pointedOwnersTmp;

    SANITY_CHECK_RETURN_VALUE(h_FmHc, E_INVALID_HANDLE,0);

    if (absoluteProfileId >= FM_PCD_PLCR_NUM_ENTRIES)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,("Policer profile out of range"));

    if (FmPcdPlcrProfileTryLock(p_FmHc->h_FmPcd, absoluteProfileId, FALSE))
        return ERROR_CODE(E_BUSY);


    requiredActionTmp = FmPcdPlcrGetRequiredAction(p_FmHc->h_FmPcd, absoluteProfileId);
    pointedOwnersTmp = FmPcdPlcrGetPointedOwners(p_FmHc->h_FmPcd, absoluteProfileId);

    if(!pointedOwnersTmp || !(requiredActionTmp & requiredAction))
    {

        if(requiredAction & UPDATE_NIA_ENQ_WITHOUT_DMA)
        {

            p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
            if (!p_HcFrame)
                RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
            /* first read scheme and check that it is valid */
            memset(p_HcFrame, 0, sizeof(t_HcFrame));
            p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
            p_HcFrame->actionReg  = FmPcdPlcrBuildReadPlcrActionReg(absoluteProfileId);
            p_HcFrame->extraReg = 0x00008000;

            BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

            if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
            {
                FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MINOR, err, NO_MSG);
            }

            /* check that profile is valid */
            if (!FmPcdPlcrHwProfileIsValid(p_HcFrame->hcSpecificData.profileRegs.fmpl_pemode))
            {
                FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, ("Policer is already used"));
            }

            tmpReg32 = p_HcFrame->hcSpecificData.profileRegs.fmpl_pegnia;
            if(!(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)))
            {
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine of this policer profile has to be assigned to FM_PCD_DONE"));
            }
            tmpReg32 |= NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;

            p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
            p_HcFrame->actionReg  = FmPcdPlcrBuildWritePlcrActionReg(absoluteProfileId);
            p_HcFrame->actionReg |= FmPcdPlcrBuildNiaProfileReg(TRUE, FALSE, FALSE);
            p_HcFrame->extraReg = 0x00008000;
            p_HcFrame->hcSpecificData.singleRegForWrite = tmpReg32;

            BUILD_FD(SIZE_OF_HC_FRAME_PROFILE_CNT);

            if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
            {
                FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MINOR, err, NO_MSG);
            }

            tmpReg32 = p_HcFrame->hcSpecificData.profileRegs.fmpl_peynia;
            if(!(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)))
            {
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine of this policer profile has to be assigned to FM_PCD_DONE"));
            }
            tmpReg32 |= NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;

            p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
            p_HcFrame->actionReg  = FmPcdPlcrBuildWritePlcrActionReg(absoluteProfileId);
            p_HcFrame->actionReg |= FmPcdPlcrBuildNiaProfileReg(FALSE, TRUE, FALSE);
            p_HcFrame->extraReg = 0x00008000;
            p_HcFrame->hcSpecificData.singleRegForWrite = tmpReg32;

            BUILD_FD(SIZE_OF_HC_FRAME_PROFILE_CNT);

            if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
            {
                FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MINOR, err, NO_MSG);
            }

            tmpReg32 = p_HcFrame->hcSpecificData.profileRegs.fmpl_pernia;
            if(!(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)))
            {
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine of this policer profile has to be assigned to FM_PCD_DONE"));
            }
            tmpReg32 |= NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;

            p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
            p_HcFrame->actionReg  = FmPcdPlcrBuildWritePlcrActionReg(absoluteProfileId);
            p_HcFrame->actionReg |= FmPcdPlcrBuildNiaProfileReg(FALSE, FALSE, TRUE);
            p_HcFrame->extraReg = 0x00008000;
            p_HcFrame->hcSpecificData.singleRegForWrite = tmpReg32;

            BUILD_FD(SIZE_OF_HC_FRAME_PROFILE_CNT);

            if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
            {
                FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
                XX_FreeSmart(p_HcFrame);
                RETURN_ERROR(MINOR, err, NO_MSG);
            }
            XX_FreeSmart(p_HcFrame);
        }
    }

    FmPcdPlcrUpatePointedOwner(p_FmHc->h_FmPcd, absoluteProfileId, TRUE);
    FmPcdPlcrUpdateRequiredAction(p_FmHc->h_FmPcd, absoluteProfileId, requiredAction);

    FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);

    return E_OK;
}

t_Handle FmHcPcdPlcrSetProfile(t_Handle h_FmHc,t_FmPcdPlcrProfileParams *p_Profile)
{
    t_FmHc                              *p_FmHc = (t_FmHc*)h_FmHc;
    t_FmPcdPlcrInterModuleProfileRegs   profileRegs;
    t_Error                             err = E_OK;
    uint32_t                            intFlags;
    uint16_t                            profileIndx;
    t_HcFrame                           *p_HcFrame;
    t_DpaaFD                            fmFd;

    if (p_Profile->modify)
    {
        profileIndx = (uint16_t)(PTR_TO_UINT(p_Profile->id.h_Profile)-1);
        if (FmPcdPlcrProfileTryLock(p_FmHc->h_FmPcd, profileIndx, FALSE))
            return NULL;
    }
    else
    {
        intFlags = FmPcdLock(p_FmHc->h_FmPcd);
        err = FmPcdPlcrGetAbsoluteProfileId(p_FmHc->h_FmPcd,
                                            p_Profile->id.newParams.profileType,
                                            p_Profile->id.newParams.h_FmPort,
                                            p_Profile->id.newParams.relativeProfileId,
                                            &profileIndx);
        if (err)
        {
            REPORT_ERROR(MAJOR, err, NO_MSG);
            return NULL;
        }
        err = FmPcdPlcrProfileTryLock(p_FmHc->h_FmPcd, profileIndx, TRUE);
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        if (err)
            return NULL;
    }

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
    {
        REPORT_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
        return NULL;
    }

    if(!p_Profile->modify)
    {
        memset(p_HcFrame, 0, sizeof(t_HcFrame));
        p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
        p_HcFrame->actionReg  = FmPcdPlcrBuildReadPlcrActionReg(profileIndx);
        p_HcFrame->extraReg = 0x00008000;

        BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

        if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
        {
            FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, profileIndx);
            REPORT_ERROR(MINOR, err, NO_MSG);
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }

        /* check if this scheme is already used */
        if (FmPcdPlcrHwProfileIsValid(p_HcFrame->hcSpecificData.profileRegs.fmpl_pemode))
        {
            FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, profileIndx);
            REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("Policer is already used"));
            XX_FreeSmart(p_HcFrame);
            return NULL;
        }
    }

    memset(&profileRegs, 0, sizeof(t_FmPcdPlcrInterModuleProfileRegs));
    err = FmPcdPlcrBuildProfile(p_FmHc->h_FmPcd, p_Profile, &profileRegs);
    if(err)
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, profileIndx);
        REPORT_ERROR(MAJOR, err, NO_MSG);
        XX_FreeSmart(p_HcFrame);
        return NULL;
    }

    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
    p_HcFrame->actionReg  = FmPcdPlcrBuildWritePlcrActionRegs(profileIndx);
    p_HcFrame->extraReg = 0x00008000;
    memcpy(&p_HcFrame->hcSpecificData.profileRegs, &profileRegs, sizeof(t_FmPcdPlcrInterModuleProfileRegs));

    BUILD_FD(sizeof(t_HcFrame));

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, profileIndx);
        REPORT_ERROR(MINOR, err, NO_MSG);
        XX_FreeSmart(p_HcFrame);
        return NULL;
    }

    FmPcdPlcrValidateProfileSw(p_FmHc->h_FmPcd, profileIndx);

    FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, profileIndx);

    XX_FreeSmart(p_HcFrame);

    return UINT_TO_PTR((uint64_t)profileIndx+1);
}

t_Error FmHcPcdPlcrDeleteProfile(t_Handle h_FmHc, t_Handle h_Profile)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    uint16_t    absoluteProfileId = (uint16_t)(PTR_TO_UINT(h_Profile)-1);
    t_Error     err = E_OK;
    t_HcFrame   *p_HcFrame;
    t_DpaaFD    fmFd;

    if (FmPcdPlcrProfileTryLock(p_FmHc->h_FmPcd, absoluteProfileId, FALSE))
        return ERROR_CODE(E_BUSY);

    FmPcdPlcrInvalidateProfileSw(p_FmHc->h_FmPcd, absoluteProfileId);

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
    p_HcFrame->actionReg  = FmPcdPlcrBuildWritePlcrActionReg(absoluteProfileId);
    p_HcFrame->actionReg  |= 0x00008000;
    p_HcFrame->extraReg = 0x00008000;
    memset(&p_HcFrame->hcSpecificData.profileRegs, 0, sizeof(t_FmPcdPlcrInterModuleProfileRegs));

    BUILD_FD(sizeof(t_HcFrame));

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);

    XX_FreeSmart(p_HcFrame);

    return E_OK;
}

t_Error  FmHcPcdPlcrSetProfileCounter(t_Handle h_FmHc, t_Handle h_Profile, e_FmPcdPlcrProfileCounters counter, uint32_t value)
{

    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    uint16_t    absoluteProfileId = (uint16_t)(PTR_TO_UINT(h_Profile)-1);
    t_Error     err = E_OK;
    t_HcFrame   *p_HcFrame;
    t_DpaaFD    fmFd;

    if (FmPcdPlcrProfileTryLock(p_FmHc->h_FmPcd, absoluteProfileId, FALSE))
        return ERROR_CODE(E_BUSY);

    /* first read scheme and check that it is valid */
    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
    p_HcFrame->actionReg  = FmPcdPlcrBuildReadPlcrActionReg(absoluteProfileId);
    p_HcFrame->extraReg = 0x00008000;

    BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    /* check that profile is valid */
    if (!FmPcdPlcrHwProfileIsValid(p_HcFrame->hcSpecificData.profileRegs.fmpl_pemode))
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, ("Policer is already used"));
    }

    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
    p_HcFrame->actionReg  = FmPcdPlcrBuildWritePlcrActionReg(absoluteProfileId);
    p_HcFrame->actionReg |= FmPcdPlcrBuildCounterProfileReg(counter);
    p_HcFrame->extraReg = 0x00008000;
    p_HcFrame->hcSpecificData.singleRegForWrite = value;

    BUILD_FD(SIZE_OF_HC_FRAME_PROFILE_CNT);

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);

    XX_FreeSmart(p_HcFrame);

    return E_OK;
}

uint32_t FmHcPcdPlcrGetProfileCounter(t_Handle h_FmHc, t_Handle h_Profile, e_FmPcdPlcrProfileCounters counter)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    uint16_t    absoluteProfileId = (uint16_t)(PTR_TO_UINT(h_Profile)-1);
    t_Error     err = E_OK;
    t_HcFrame   *p_HcFrame;
    t_DpaaFD    fmFd;
    uint32_t    retVal = 0;

    SANITY_CHECK_RETURN_VALUE(h_FmHc, E_INVALID_HANDLE,0);

    if (FmPcdPlcrProfileTryLock(p_FmHc->h_FmPcd, absoluteProfileId, FALSE))
        return 0;

    /* first read scheme and check that it is valid */
    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
    {
        REPORT_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
        return 0;
    }
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_PLCR_PRFL);
    p_HcFrame->actionReg  = FmPcdPlcrBuildReadPlcrActionReg(absoluteProfileId);
    p_HcFrame->extraReg = 0x00008000;

    BUILD_FD(SIZE_OF_HC_FRAME_READ_OR_CC_DYNAMIC);

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
        REPORT_ERROR(MINOR, err, NO_MSG);
        XX_FreeSmart(p_HcFrame);
        return 0;
    }

    /* check that profile is valid */
    if (!FmPcdPlcrHwProfileIsValid(p_HcFrame->hcSpecificData.profileRegs.fmpl_pemode))
    {
        FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);
        XX_FreeSmart(p_HcFrame);
        REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("invalid Policer profile"));
        return 0;
    }

    switch (counter)
    {
        case e_FM_PCD_PLCR_PROFILE_GREEN_PACKET_TOTAL_COUNTER:
            retVal = p_HcFrame->hcSpecificData.profileRegs.fmpl_pegpc;
            break;
        case e_FM_PCD_PLCR_PROFILE_YELLOW_PACKET_TOTAL_COUNTER:
            retVal = p_HcFrame->hcSpecificData.profileRegs.fmpl_peypc;
            break;
        case e_FM_PCD_PLCR_PROFILE_RED_PACKET_TOTAL_COUNTER:
            retVal = p_HcFrame->hcSpecificData.profileRegs.fmpl_perpc;
            break;
        case e_FM_PCD_PLCR_PROFILE_RECOLOURED_YELLOW_PACKET_TOTAL_COUNTER:
            retVal = p_HcFrame->hcSpecificData.profileRegs.fmpl_perypc;
            break;
        case e_FM_PCD_PLCR_PROFILE_RECOLOURED_RED_PACKET_TOTAL_COUNTER:
            retVal = p_HcFrame->hcSpecificData.profileRegs.fmpl_perrpc;
            break;
        default:
            REPORT_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
    }

    FmPcdPlcrReleaseProfileLock(p_FmHc->h_FmPcd, absoluteProfileId);

    XX_FreeSmart(p_HcFrame);

    return retVal;
}

t_Error FmHcPcdCcModifyTreeNextEngine(t_Handle h_FmHc, t_Handle h_CcTree, uint8_t grpId, uint8_t index, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Error     err = E_OK;
    uint32_t    intFlags;
    t_List      h_OldPointersLst, h_NewPointersLst;
    t_Handle    h_Params;

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);
    err = FmPcdCcTreeTryLock(h_CcTree);
    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
    if (err)
        return err;

    INIT_LIST(&h_OldPointersLst);
    INIT_LIST(&h_NewPointersLst);

    err = FmPcdCcModifyNextEngineParamTree(p_FmHc->h_FmPcd, h_CcTree, grpId, index, p_FmPcdCcNextEngineParams,
            &h_OldPointersLst, &h_NewPointersLst, &h_Params);
    if(err)
    {
        FmPcdCcTreeReleaseLock(h_CcTree);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err =  HcDynamicChange(p_FmHc, &h_OldPointersLst, &h_NewPointersLst, &h_Params);

    FmPcdCcTreeReleaseLock(h_CcTree);

    return err;
}


t_Error FmHcPcdCcModifyNodeMissNextEngine(t_Handle h_FmHc, t_Handle h_CcNode, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Handle    h_Params;
    t_List      h_OldPointersLst, h_NewPointersLst;
    t_Error     err = E_OK;
    t_List      h_List;
    uint32_t    intFlags;

    INIT_LIST(&h_List);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);

    if ((err = FmPcdCcNodeTreeTryLock(p_FmHc->h_FmPcd, h_CcNode, &h_List)) != E_OK)
    {
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        return err;
    }

    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);

    INIT_LIST(&h_OldPointersLst);
    INIT_LIST(&h_NewPointersLst);

    err = FmPcdCcModifyMissNextEngineParamNode(p_FmHc->h_FmPcd, h_CcNode, p_FmPcdCcNextEngineParams, &h_OldPointersLst, &h_NewPointersLst, &h_Params);
    if(err)
    {
        FmPcdCcNodeTreeReleaseLock(&h_List);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err =  HcDynamicChange(p_FmHc, &h_OldPointersLst, &h_NewPointersLst, &h_Params);

    FmPcdCcNodeTreeReleaseLock(&h_List);


    return E_OK;
}

t_Error FmHcPcdCcRemoveKey(t_Handle h_FmHc, t_Handle h_CcNode, uint8_t keyIndex)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Handle    h_Params;
    t_List      h_OldPointersLst, h_NewPointersLst;
    t_Error     err = E_OK;
    t_List      h_List;
    uint32_t    intFlags;

    INIT_LIST(&h_List);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);

    if ((err = FmPcdCcNodeTreeTryLock(p_FmHc->h_FmPcd, h_CcNode, &h_List)) != E_OK)
    {
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        return err;
    }

    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);

    INIT_LIST(&h_OldPointersLst);
    INIT_LIST(&h_NewPointersLst);


    err = FmPcdCcRemoveKey(p_FmHc->h_FmPcd,h_CcNode,keyIndex, &h_OldPointersLst, &h_NewPointersLst, &h_Params);
    if(err)
    {
        FmPcdCcNodeTreeReleaseLock(&h_List);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err =  HcDynamicChange(p_FmHc, &h_OldPointersLst, &h_NewPointersLst, &h_Params);

    FmPcdCcNodeTreeReleaseLock(&h_List);

    return err;

}

t_Error FmHcPcdCcAddKey(t_Handle h_FmHc, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams  *p_KeyParams)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Handle    h_Params;
    t_List      h_OldPointersLst, h_NewPointersLst;
    t_Error     err = E_OK;
    t_List      h_List;
    uint32_t    intFlags;

    INIT_LIST(&h_List);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);

    if ((err = FmPcdCcNodeTreeTryLock(p_FmHc->h_FmPcd, h_CcNode, &h_List)) != E_OK)
    {
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        return err;
    }

    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);

    INIT_LIST(&h_OldPointersLst);
    INIT_LIST(&h_NewPointersLst);


    err = FmPcdCcAddKey(p_FmHc->h_FmPcd,h_CcNode,keyIndex,keySize, p_KeyParams, &h_OldPointersLst,&h_NewPointersLst, &h_Params);
    if(err)
    {
        FmPcdCcNodeTreeReleaseLock(&h_List);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err =  HcDynamicChange(p_FmHc, &h_OldPointersLst, &h_NewPointersLst, &h_Params);

    FmPcdCcNodeTreeReleaseLock(&h_List);

    return err;
}


t_Error FmHcPcdCcModifyKey(t_Handle h_FmHc, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, uint8_t  *p_Key, uint8_t *p_Mask)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_List      h_OldPointersLst, h_NewPointersLst;
    t_Error     err = E_OK;
    t_List      h_List;
    uint32_t    intFlags;
    t_Handle    h_Params;

    UNUSED(keySize);

    INIT_LIST(&h_List);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);

    if ((err = FmPcdCcNodeTreeTryLock(p_FmHc->h_FmPcd, h_CcNode, &h_List)) != E_OK)
    {
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        return err;
    }

    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);

    INIT_LIST(&h_OldPointersLst);
    INIT_LIST(&h_NewPointersLst);

    err = FmPcdCcModifyKey(p_FmHc->h_FmPcd, h_CcNode, keyIndex, keySize, p_Key, p_Mask, &h_OldPointersLst,&h_NewPointersLst,  &h_Params);
    if(err)
    {
        FmPcdCcNodeTreeReleaseLock(&h_List);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err =  HcDynamicChange(p_FmHc, &h_OldPointersLst, &h_NewPointersLst, &h_Params);

    FmPcdCcNodeTreeReleaseLock(&h_List);

    return err;
}

t_Error FmHcPcdCcModifyNodeNextEngine(t_Handle h_FmHc, t_Handle h_CcNode, uint8_t keyIndex, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_Error     err = E_OK;
    t_List      h_OldPointersLst, h_NewPointersLst;
    t_List      h_List;
    uint32_t    intFlags;
    t_Handle    h_Params;

    INIT_LIST(&h_List);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);

    if ((err = FmPcdCcNodeTreeTryLock(p_FmHc->h_FmPcd, h_CcNode, &h_List)) != E_OK)
    {
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        return err;
    }

    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);

    INIT_LIST(&h_OldPointersLst);
    INIT_LIST(&h_NewPointersLst);

    err = FmPcdCcModiyNextEngineParamNode(p_FmHc->h_FmPcd, h_CcNode, keyIndex, p_FmPcdCcNextEngineParams, &h_OldPointersLst, &h_NewPointersLst, &h_Params);
    if(err)
    {
        FmPcdCcNodeTreeReleaseLock(&h_List);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err =  HcDynamicChange(p_FmHc, &h_OldPointersLst, &h_NewPointersLst, &h_Params);
    FmPcdCcNodeTreeReleaseLock(&h_List);
    return err;
}


t_Error FmHcPcdCcModifyKeyAndNextEngine(t_Handle h_FmHc, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams  *p_KeyParams)
{
    t_FmHc      *p_FmHc = (t_FmHc*)h_FmHc;
    t_List      h_OldPointersLst, h_NewPointersLst;
    t_Error     err = E_OK;
    t_List      h_List;
    uint32_t    intFlags;
    t_Handle    h_Params;

    INIT_LIST(&h_OldPointersLst);
    INIT_LIST(&h_NewPointersLst);
    INIT_LIST(&h_List);

    intFlags = FmPcdLock(p_FmHc->h_FmPcd);

    if ((err = FmPcdCcNodeTreeTryLock(p_FmHc->h_FmPcd, h_CcNode, &h_List)) != E_OK)
    {
        FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);
        return err;
    }

    FmPcdUnlock(p_FmHc->h_FmPcd, intFlags);


    err = FmPcdCcModifyKeyAndNextEngine(p_FmHc->h_FmPcd,h_CcNode,keyIndex,keySize, p_KeyParams, &h_OldPointersLst,&h_NewPointersLst, &h_Params);
    if(err)
    {
        FmPcdCcNodeTreeReleaseLock(&h_List);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err =  HcDynamicChange(p_FmHc, &h_OldPointersLst, &h_NewPointersLst, &h_Params);

    FmPcdCcNodeTreeReleaseLock(&h_List);


    return err;
}


t_Error FmHcKgWriteSp(t_Handle h_FmHc, uint8_t hardwarePortId, uint32_t spReg, bool add)
{
    t_FmHc                  *p_FmHc = (t_FmHc*)h_FmHc;
    t_HcFrame               *p_HcFrame;
    t_DpaaFD                fmFd;
    t_Error                 err = E_OK;

    ASSERT_COND(p_FmHc);

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    /* first read SP register */
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
    p_HcFrame->actionReg  = FmPcdKgBuildReadPortSchemeBindActionReg(hardwarePortId);
    p_HcFrame->extraReg = 0xFFFFF800;

    BUILD_FD(SIZE_OF_HC_FRAME_PORT_REGS);

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    /* spReg is the first reg, so we can use it both for read and for write */
    if(add)
        p_HcFrame->hcSpecificData.portRegsForRead.spReg |= spReg;
    else
        p_HcFrame->hcSpecificData.portRegsForRead.spReg &= ~spReg;

    p_HcFrame->actionReg  = FmPcdKgBuildWritePortSchemeBindActionReg(hardwarePortId);

    BUILD_FD(sizeof(t_HcFrame));

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    XX_FreeSmart(p_HcFrame);

    return E_OK;
}

t_Error FmHcKgWriteCpp(t_Handle h_FmHc, uint8_t hardwarePortId, uint32_t cppReg)
{
    t_FmHc                  *p_FmHc = (t_FmHc*)h_FmHc;
    t_HcFrame               *p_HcFrame;
    t_DpaaFD                fmFd;
    t_Error                 err = E_OK;

    ASSERT_COND(p_FmHc);

    p_HcFrame = (t_HcFrame *)XX_MallocSmart((sizeof(t_HcFrame) + p_FmHc->padTill16), 0, 16);
    if (!p_HcFrame)
        RETURN_ERROR(MINOR, E_NO_MEMORY, ("HC Frame obj"));
    memset(p_HcFrame, 0, sizeof(t_HcFrame));
    /* first read SP register */
    p_HcFrame->opcode = (uint32_t)(HC_HCOR_GBL | HC_HCOR_OPCODE_KG_SCM);
    p_HcFrame->actionReg  = FmPcdKgBuildWritePortClsPlanBindActionReg(hardwarePortId);
    p_HcFrame->extraReg = 0xFFFFF800;
    p_HcFrame->hcSpecificData.singleRegForWrite = cppReg;

    BUILD_FD(sizeof(t_HcFrame));

    if ((err = EnQFrm(p_FmHc, &fmFd, &p_HcFrame->commandSequence)) != E_OK)
    {
        XX_FreeSmart(p_HcFrame);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    XX_FreeSmart(p_HcFrame);

    return E_OK;
}

