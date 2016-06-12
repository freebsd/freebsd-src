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
 @File          fm_pcd.c

 @Description   FM PCD ...
*//***************************************************************************/
#include "std_ext.h"
#include "error_ext.h"
#include "string_ext.h"
#include "debug_ext.h"
#include "net_ext.h"

#include "fm_common.h"
#include "fm_pcd.h"
#include "fm_pcd_ipc.h"


t_Handle PrsConfig(t_FmPcd *p_FmPcd,t_FmPcdParams *p_FmPcdParams)
{
    t_FmPcdPrs  *p_FmPcdPrs;
    uintptr_t   baseAddr;

    UNUSED(p_FmPcd);
    UNUSED(p_FmPcdParams);

    p_FmPcdPrs = (t_FmPcdPrs *) XX_Malloc(sizeof(t_FmPcdPrs));
    if (!p_FmPcdPrs)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM Parser structure allocation FAILED"));
        return NULL;
    }
    memset(p_FmPcdPrs, 0, sizeof(t_FmPcdPrs));

    if (p_FmPcd->guestId == NCSW_MASTER_ID)
    {
        baseAddr = FmGetPcdPrsBaseAddr(p_FmPcdParams->h_Fm);
        p_FmPcdPrs->p_SwPrsCode  = (uint32_t *)UINT_TO_PTR(baseAddr);
        p_FmPcdPrs->p_FmPcdPrsRegs  = (t_FmPcdPrsRegs *)UINT_TO_PTR(baseAddr + PRS_REGS_OFFSET);
    }

    p_FmPcdPrs->fmPcdPrsPortIdStatistics             = 0;
    p_FmPcd->p_FmPcdDriverParam->prsMaxParseCycleLimit   = DEFAULT_prsMaxParseCycleLimit;
    p_FmPcd->exceptions |= (DEFAULT_fmPcdPrsErrorExceptions | DEFAULT_fmPcdPrsExceptions);

    return p_FmPcdPrs;
}

static void PcdPrsErrorException(t_Handle h_FmPcd)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint32_t                event, mask, force;

    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);
    event = GET_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->perr);
    mask = GET_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->perer);

    event &= mask;

    /* clear the forced events */
    force = GET_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->perfr);
    if(force & event)
        WRITE_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->perfr, force & ~event);

    WRITE_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->perr, event);

    DBG(TRACE, ("parser error - 0x%08x\n",event));

    if(event & FM_PCD_PRS_DOUBLE_ECC)
        p_FmPcd->f_Exception(p_FmPcd->h_App,e_FM_PCD_PRS_EXCEPTION_DOUBLE_ECC);
}

static void PcdPrsException(t_Handle h_FmPcd)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint32_t            event, force;

    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);
    event = GET_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->pevr);
    event &= GET_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->pever);

    ASSERT_COND(event & FM_PCD_PRS_SINGLE_ECC);

    DBG(TRACE, ("parser event - 0x%08x\n",event));

    /* clear the forced events */
    force = GET_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->pevfr);
    if(force & event)
        WRITE_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->pevfr, force & ~event);

    WRITE_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->pevr, event);

    p_FmPcd->f_Exception(p_FmPcd->h_App,e_FM_PCD_PRS_EXCEPTION_SINGLE_ECC);
}

static uint32_t GetSwPrsOffset(t_Handle h_FmPcd,  e_NetHeaderType hdr, uint8_t  indexPerHdr)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    int                     i;
    t_FmPcdPrsLabelParams   *p_Label;

    SANITY_CHECK_RETURN_VALUE(p_FmPcd, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_HANDLE, 0);

    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);
    ASSERT_COND(p_FmPcd->p_FmPcdPrs->currLabel < FM_PCD_PRS_NUM_OF_LABELS);

    for (i=0; i < p_FmPcd->p_FmPcdPrs->currLabel; i++)
    {
        p_Label = &p_FmPcd->p_FmPcdPrs->labelsTable[i];

        if ((hdr == p_Label->hdr) && (indexPerHdr == p_Label->indexPerHdr))
            return p_Label->instructionOffset;
    }

    REPORT_ERROR(MAJOR, E_NOT_FOUND, ("Sw Parser attachment Not found"));
    return (uint32_t)ILLEGAL_BASE;
}

t_Error PrsInit(t_FmPcd *p_FmPcd)
{
    t_FmPcdDriverParam  *p_Param = p_FmPcd->p_FmPcdDriverParam;
    t_FmPcdPrsRegs      *p_Regs = p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs;
    uint32_t            tmpReg;

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
        return E_OK;

    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);

#ifdef FM_PRS_MEM_ERRATA_FMAN_SW003
    {
        uint32_t            i;
        uint32_t            regsToGlobalOffset = 0x840;
        uint32_t            firstPortToGlobalOffset = 0x45800;
        uint64_t            globalAddr = PTR_TO_UINT(p_Regs) - regsToGlobalOffset;
        uint32_t            firstPortAddr = (uint32_t)(globalAddr - (uint64_t)firstPortToGlobalOffset);
        uint32_t            portSize = 0x1000;
        t_FmRevisionInfo    revInfo;

        FM_GetRevision(p_FmPcd->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        {
            /* clear all parser memory */
            IOMemSet32(UINT_TO_PTR(globalAddr), 0x00000000, 0x800);
            for(i = 0;i<16;i++)
                IOMemSet32(UINT_TO_PTR(firstPortAddr+i*portSize), (uint8_t)0x00000000, (uint32_t)0x80);
        }
    }
#endif /* FM_PRS_MEM_ERRATA_FMAN_SW003 */

    /**********************RPCLIM******************/
    WRITE_UINT32(p_Regs->rpclim, (uint32_t)p_Param->prsMaxParseCycleLimit);
    /**********************FMPL_RPCLIM******************/

    /* register even if no interrupts enabled, to allow future enablement */
    FmRegisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PRS, 0, e_FM_INTR_TYPE_ERR, PcdPrsErrorException, p_FmPcd);

    /* register even if no interrupts enabled, to allow future enablement */
    FmRegisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PRS, 0, e_FM_INTR_TYPE_NORMAL, PcdPrsException, p_FmPcd);

    /**********************PEVR******************/
    WRITE_UINT32(p_Regs->pevr, (FM_PCD_PRS_SINGLE_ECC | FM_PCD_PRS_PORT_IDLE_STS) );
    /**********************PEVR******************/

    /**********************PEVER******************/
    if(p_FmPcd->exceptions & FM_PCD_EX_PRS_SINGLE_ECC)
    {
        FmEnableRamsEcc(p_FmPcd->h_Fm);
        WRITE_UINT32(p_Regs->pever, FM_PCD_PRS_SINGLE_ECC);
    }
    else
        WRITE_UINT32(p_Regs->pever, 0);
    /**********************PEVER******************/

    /**********************PERR******************/
    WRITE_UINT32(p_Regs->perr, FM_PCD_PRS_DOUBLE_ECC);

    /**********************PERR******************/

    /**********************PERER******************/
    tmpReg = 0;
    if(p_FmPcd->exceptions & FM_PCD_EX_PRS_DOUBLE_ECC)
    {
        FmEnableRamsEcc(p_FmPcd->h_Fm);
        tmpReg |= FM_PCD_PRS_DOUBLE_ECC;
    }
    WRITE_UINT32(p_Regs->perer, tmpReg);
    /**********************PERER******************/

    /**********************PPCS******************/
    WRITE_UINT32(p_Regs->ppsc, p_FmPcd->p_FmPcdPrs->fmPcdPrsPortIdStatistics);
    /**********************PPCS******************/

#ifdef FM_PRS_L4_SHELL_ERRATA_FMANb
    {
        uint32_t            i, j;
        t_FmRevisionInfo    revInfo;
        uint8_t             swPrsL4Patch[] = SW_PRS_L4_PATCH;

        FM_GetRevision(p_FmPcd->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        {
            /* load sw parser L4 patch */
            for(i=0;i<sizeof(swPrsL4Patch)/4;i++)
            {
               tmpReg = 0;
               for(j =0;j<4;j++)
               {
                  tmpReg <<= 8;
                  tmpReg |= swPrsL4Patch[i*4+j];

               }
                WRITE_UINT32(*(p_FmPcd->p_FmPcdPrs->p_SwPrsCode+ FM_PCD_PRS_SW_OFFSET/4 + i), tmpReg);
            }
            p_FmPcd->p_FmPcdPrs->p_CurrSwPrs = FM_PCD_PRS_SW_OFFSET/4 + p_FmPcd->p_FmPcdPrs->p_SwPrsCode+sizeof(swPrsL4Patch)/4;
        }
    }
#endif /* FM_PRS_L4_SHELL_ERRATA_FMANb */

    return E_OK;
}

void PrsFree(t_FmPcd *p_FmPcd )
{
    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);
    FmUnregisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PRS, 0, e_FM_INTR_TYPE_ERR);
    /* register even if no interrupts enabled, to allow future enablement */
    FmUnregisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PRS, 0, e_FM_INTR_TYPE_NORMAL);
}

void PrsEnable(t_FmPcd *p_FmPcd )
{
    t_FmPcdPrsRegs      *p_Regs = p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs;

    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);
    WRITE_UINT32(p_Regs->rpimac, GET_UINT32(p_Regs->rpimac) | FM_PCD_PRS_RPIMAC_EN);
}

void PrsDisable(t_FmPcd *p_FmPcd )
{
    t_FmPcdPrsRegs      *p_Regs = p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs;

    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);
    WRITE_UINT32(p_Regs->rpimac, GET_UINT32(p_Regs->rpimac) & ~FM_PCD_PRS_RPIMAC_EN);
}

t_Error PrsIncludePortInStatistics(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, bool include)
{
    uint32_t    bitMask = 0;
    uint8_t     prsPortId;

    SANITY_CHECK_RETURN_ERROR((hardwarePortId >=1 && hardwarePortId <= 16), E_INVALID_VALUE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPrs, E_INVALID_HANDLE);

    GET_FM_PCD_PRS_PORT_ID(prsPortId, hardwarePortId);
    GET_FM_PCD_INDEX_FLAG(bitMask, prsPortId);

    if(include)
        p_FmPcd->p_FmPcdPrs->fmPcdPrsPortIdStatistics |= bitMask;
    else
        p_FmPcd->p_FmPcdPrs->fmPcdPrsPortIdStatistics &= ~bitMask;

    WRITE_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->ppsc, p_FmPcd->p_FmPcdPrs->fmPcdPrsPortIdStatistics);

    return E_OK;
}

t_Error FmPcdPrsIncludePortInStatistics(t_Handle h_FmPcd, uint8_t hardwarePortId, bool include)
{
    t_FmPcd                     *p_FmPcd = (t_FmPcd *)h_FmPcd;
    t_FmPcdIpcPrsIncludePort    prsIncludePortParams;
    t_FmPcdIpcMsg               msg;
    t_Error                     err;

    SANITY_CHECK_RETURN_ERROR((hardwarePortId >=1 && hardwarePortId <= 16), E_INVALID_VALUE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPrs, E_INVALID_HANDLE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        prsIncludePortParams.hardwarePortId = hardwarePortId;
        prsIncludePortParams.include = include;
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_PCD_PRS_INC_PORT_STATS;
        memcpy(msg.msgBody, &prsIncludePortParams, sizeof(prsIncludePortParams));
        if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) +sizeof(prsIncludePortParams),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MAJOR, err, NO_MSG);
        return E_OK;
    }
    return PrsIncludePortInStatistics(p_FmPcd, hardwarePortId, include);
}

uint32_t FmPcdGetSwPrsOffset(t_Handle h_FmPcd, e_NetHeaderType hdr, uint8_t indexPerHdr)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd *)h_FmPcd;
    t_Error                 err = E_OK;
    t_FmPcdIpcSwPrsLable    labelParams;
    t_FmPcdIpcMsg           msg;
    uint32_t                prsOffset = 0;
    t_FmPcdIpcReply         reply;
    uint32_t                replyLength;

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        memset(&reply, 0, sizeof(reply));
        memset(&msg, 0, sizeof(msg));
        labelParams.enumHdr = (uint32_t)hdr;
        labelParams.indexPerHdr = indexPerHdr;
        msg.msgId = FM_PCD_GET_SW_PRS_OFFSET;
        memcpy(msg.msgBody, &labelParams, sizeof(labelParams));
        replyLength = sizeof(uint32_t) + sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) +sizeof(labelParams),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MAJOR, err, NO_MSG);
        if(replyLength != sizeof(uint32_t) + sizeof(uint32_t))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));

        memcpy((uint8_t*)&prsOffset, reply.replyBody, sizeof(uint32_t));
        return prsOffset;
    }

    return GetSwPrsOffset(h_FmPcd, hdr, indexPerHdr);
}

void FM_PCD_SetPrsStatistics(t_Handle h_FmPcd, bool enable)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;

    SANITY_CHECK_RETURN(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(p_FmPcd->p_FmPcdPrs, E_INVALID_HANDLE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("FM_PCD_SetPrsStatistics - guest mode!"));
        return;
    }
    if(enable)
        WRITE_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->ppsc, FM_PCD_PRS_PPSC_ALL_PORTS);
    else
        WRITE_UINT32(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs->ppsc, 0);

}

t_Error FM_PCD_PrsLoadSw(t_Handle h_FmPcd, t_FmPcdPrsSwParams *p_SwPrs)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t                *p_LoadTarget, tmpReg;
    int                     i, j;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPrs, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_SwPrs, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->enabled, E_INVALID_HANDLE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("FM_PCD_PrsLoadSw - guest mode!"));

    if(!p_SwPrs->override)
    {
        if(p_FmPcd->p_FmPcdPrs->p_CurrSwPrs > p_FmPcd->p_FmPcdPrs->p_SwPrsCode + p_SwPrs->base*2/4)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("SW parser base must be larger than current loaded code"));
    }
    if(p_SwPrs->size > FM_PCD_SW_PRS_SIZE - FM_PCD_PRS_SW_TAIL_SIZE - p_SwPrs->base*2)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("p_SwPrs->size may not be larger than MAX_SW_PRS_CODE_SIZE"));
    if(p_SwPrs->size % 4)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("p_SwPrs->size must be divisible by 4"));

    /* save sw parser labels */
    if(p_SwPrs->override)
        p_FmPcd->p_FmPcdPrs->currLabel = 0;
    if(p_FmPcd->p_FmPcdPrs->currLabel+ p_SwPrs->numOfLabels > FM_PCD_PRS_NUM_OF_LABELS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Exceeded number of labels allowed "));
    memcpy(&p_FmPcd->p_FmPcdPrs->labelsTable[p_FmPcd->p_FmPcdPrs->currLabel], p_SwPrs->labelsTable, p_SwPrs->numOfLabels*sizeof(t_FmPcdPrsLabelParams));
    p_FmPcd->p_FmPcdPrs->currLabel += p_SwPrs->numOfLabels;
    /* load sw parser code */
    p_LoadTarget = p_FmPcd->p_FmPcdPrs->p_SwPrsCode + p_SwPrs->base*2/4;
    for(i=0;i<p_SwPrs->size/4;i++)
    {
        tmpReg = 0;
        for(j =0;j<4;j++)
        {
            tmpReg <<= 8;
            tmpReg |= *(p_SwPrs->p_Code+i*4+j);
        }
        WRITE_UINT32(*(p_LoadTarget + i), tmpReg);
    }
    p_FmPcd->p_FmPcdPrs->p_CurrSwPrs = p_FmPcd->p_FmPcdPrs->p_SwPrsCode + p_SwPrs->base*2/4 + p_SwPrs->size/4;

    /* copy data parameters */
    for(i=0;i<FM_PCD_PRS_NUM_OF_HDRS;i++)
        WRITE_UINT32(*(p_FmPcd->p_FmPcdPrs->p_SwPrsCode+PRS_SW_DATA/4+i), p_SwPrs->swPrsDataParams[i]);


    /* Clear last 4 bytes */
    WRITE_UINT32(*(p_FmPcd->p_FmPcdPrs->p_SwPrsCode+(PRS_SW_DATA-FM_PCD_PRS_SW_TAIL_SIZE)/4), 0);

    return E_OK;
}

t_Error FM_PCD_ConfigPrsMaxCycleLimit(t_Handle h_FmPcd,uint16_t value)
{
    t_FmPcd *p_FmPcd = (t_FmPcd*)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdDriverParam, E_INVALID_HANDLE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("FM_PCD_ConfigPrsMaxCycleLimit - guest mode!"));

    p_FmPcd->p_FmPcdDriverParam->prsMaxParseCycleLimit = value;

    return E_OK;
}


#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FM_PCD_PrsDumpRegs(t_Handle h_FmPcd)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdIpcMsg       msg;

    DECLARE_DUMP;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPrs, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_PCD_PRS_DUMP_REGS;
        return XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                    (uint8_t*)&msg,
                                    sizeof(msg.msgId),
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
    }
    DUMP_SUBTITLE(("\n"));
    DUMP_TITLE(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs, ("FmPcdPrsRegs Regs"));

    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,rpclim);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,rpimac);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,pmeec);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,pevr);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,pever);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,pevfr);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,perr);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,perer);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,perfr);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,ppsc);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,pds);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,l2rrs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,l3rrs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,l4rrs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,srrs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,l2rres);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,l3rres);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,l4rres);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,srres);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,spcs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,spscs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,hxscs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,mrcs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,mwcs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,mrscs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,mwscs);
    DUMP_VAR(p_FmPcd->p_FmPcdPrs->p_FmPcdPrsRegs,fcscs);

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && ... */
