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
 @File          fm_plcr.c

 @Description   FM PCD POLICER...
*//***************************************************************************/
#include "std_ext.h"
#include "error_ext.h"
#include "string_ext.h"
#include "debug_ext.h"
#include "net_ext.h"
#include "fm_ext.h"

#include "fm_common.h"
#include "fm_pcd.h"
#include "fm_hc.h"
#include "fm_pcd_ipc.h"


static bool FmPcdPlcrIsProfileShared(t_Handle h_FmPcd, uint16_t absoluteProfileId)
{
    t_FmPcd         *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint16_t        i;

    SANITY_CHECK_RETURN_VALUE(p_FmPcd, E_INVALID_HANDLE, FALSE);

    for(i=0;i<p_FmPcd->p_FmPcdPlcr->numOfSharedProfiles;i++)
        if(p_FmPcd->p_FmPcdPlcr->sharedProfilesIds[i] == absoluteProfileId)
            return TRUE;
    return FALSE;
}

static t_Error SetProfileNia(t_FmPcd *p_FmPcd, e_FmPcdEngine nextEngine, u_FmPcdPlcrNextEngineParams *p_NextEngineParams, uint32_t *nextAction)
{
    uint32_t    nia;
    uint16_t    absoluteProfileId = (uint16_t)(PTR_TO_UINT(p_NextEngineParams->h_Profile)-1);
    uint8_t     relativeSchemeId, physicatSchemeId;

    nia = FM_PCD_PLCR_NIA_VALID;

    switch (nextEngine)
    {
        case e_FM_PCD_DONE :
            switch (p_NextEngineParams->action)
            {
                case e_FM_PCD_DROP_FRAME :
                    nia |= (NIA_ENG_BMI | NIA_BMI_AC_DISCARD);
                    break;
                case e_FM_PCD_ENQ_FRAME:
                    nia |= (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME);
                    break;
                default:
                    RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            }
            break;
        case e_FM_PCD_KG:
            physicatSchemeId = (uint8_t)(PTR_TO_UINT(p_NextEngineParams->h_DirectScheme)-1);
            relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmPcd, physicatSchemeId);
            if(relativeSchemeId == FM_PCD_KG_NUM_OF_SCHEMES)
                RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);
            if (!FmPcdKgIsSchemeValidSw(p_FmPcd, relativeSchemeId))
                 RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid direct scheme."));
            if(!KgIsSchemeAlwaysDirect(p_FmPcd, relativeSchemeId))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Policer Profile may point only to a scheme that is always direct."));
            nia |= NIA_ENG_KG | NIA_KG_DIRECT | physicatSchemeId;
            break;
        case e_FM_PCD_PLCR:
             if(!FmPcdPlcrIsProfileShared(p_FmPcd, absoluteProfileId))
               RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next profile must be a shared profile"));
             if(!FmPcdPlcrIsProfileValid(p_FmPcd, absoluteProfileId))
               RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid profile "));
            nia |= NIA_ENG_PLCR | NIA_PLCR_ABSOLUTE | absoluteProfileId;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
    }

    *nextAction =  nia;

    return E_OK;
}

static uint32_t FPP_Function(uint32_t fpp)
{
    if(fpp > 15)
        return 15 - (0x1f - fpp);
    else
        return 16 + fpp;
}

static void GetInfoRateReg(e_FmPcdPlcrRateMode rateMode,
                                uint32_t rate,
                                uint64_t tsuInTenthNano,
                                uint32_t fppShift,
                                uint64_t *p_Integer,
                                uint64_t *p_Fraction)
{
    uint64_t tmp, div;

    if(rateMode == e_FM_PCD_PLCR_BYTE_MODE)
    {
        /* now we calculate the initial integer for the bigger rate */
        /* from Kbps to Bytes/TSU */
        tmp = (uint64_t)rate;
        tmp *= 1000; /* kb --> b */
        tmp *= tsuInTenthNano; /* bps --> bpTsu(in 10nano) */

        div = 1000000000;   /* nano */
        div *= 10;          /* 10 nano */
        div *= 8;           /* bit to byte */
    }
    else
    {
        /* now we calculate the initial integer for the bigger rate */
        /* from Kbps to Bytes/TSU */
        tmp = (uint64_t)rate;
        tmp *= tsuInTenthNano; /* bps --> bpTsu(in 10nano) */

        div = 1000000000;   /* nano */
        div *= 10;          /* 10 nano */
    }
    *p_Integer = (tmp<<fppShift)/div;

    /* for calculating the fraction, we will recalculate cir and deduct the integer.
     * For precision, we will multiply by 2^16. we do not divid back, since we write
     * this value as fraction - see spec.
     */
    *p_Fraction = (((tmp<<fppShift)<<16) - ((*p_Integer<<16)*div))/div;
}

/* .......... */

static void calcRates(t_Handle h_FmPcd, t_FmPcdPlcrNonPassthroughAlgParams *p_NonPassthroughAlgParam,
                        uint32_t *cir, uint32_t *cbs, uint32_t *pir_eir, uint32_t *pbs_ebs, uint32_t *fpp)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint64_t    integer, fraction;
    uint32_t    temp, tsuInTenthNanos, bitFor1Micro;
    uint8_t     fppShift=0;

    bitFor1Micro = FmGetTimeStampScale(p_FmPcd->h_Fm);  /* TimeStamp per nano seconds units */
    /* we want the tsu to count 10 nano for better precision normally tsu is 3.9 nano, now we will get 39 */
    tsuInTenthNanos = (uint32_t)(1000*10/(1<<bitFor1Micro));

    /* we choose the faster rate to calibrate fpp */
    if (p_NonPassthroughAlgParam->comittedInfoRate > p_NonPassthroughAlgParam->peakOrAccessiveInfoRate)
        GetInfoRateReg(p_NonPassthroughAlgParam->rateMode, p_NonPassthroughAlgParam->comittedInfoRate, tsuInTenthNanos, 0, &integer, &fraction);
    else
        GetInfoRateReg(p_NonPassthroughAlgParam->rateMode, p_NonPassthroughAlgParam->peakOrAccessiveInfoRate, tsuInTenthNanos, 0, &integer, &fraction);


    /* we shift integer, as in cir/pir it is represented by the MSB 16 bits, and
     * the LSB bits are for the fraction */
    temp = (uint32_t)((integer<<16) & 0x00000000FFFFFFFF);
    /* temp is effected by the rate. For low rates it may be as low as 0, and then we'll
     * take max fpp=31.
     * For high rates it will never exceed the 32 bit reg (after the 16 shift), as it is
     * limited by the 10G physical port.
     */
    if(temp != 0)
    {
        /* count zeroes left of the higher used bit (in order to shift the value such that
         * unused bits may be used for fraction).
         */
        while ((temp & 0x80000000) == 0)
        {
            temp = temp << 1;
            fppShift++;
        }
        if(fppShift > 15)
        {
            REPORT_ERROR(MAJOR, E_INVALID_SELECTION, ("timeStampPeriod to Information rate ratio is too small"));
            return;
        }
    }
    else
    {
        temp = (uint32_t)fraction; /* fraction will alyas be smaller than 2^16 */
        if(!temp)
            /* integer and fraction are 0, we set fpp to its max val */
            fppShift = 31;
        else
        {
            /* integer was 0 but fraction is not. fpp is 16 for the integer,
             * + all left zeroes of the fraction. */
            fppShift=16;
            /* count zeroes left of the higher used bit (in order to shift the value such that
             * unused bits may be used for fraction).
             */
            while ((temp & 0x8000) == 0)
            {
                temp = temp << 1;
                fppShift++;
            }
        }
    }

    /*
     * This means that the FM TS register will now be used so that 'count' bits are for
     * fraction and the rest for integer */
    /* now we re-calculate cir */
    GetInfoRateReg(p_NonPassthroughAlgParam->rateMode, p_NonPassthroughAlgParam->comittedInfoRate, tsuInTenthNanos, fppShift, &integer, &fraction);
    *cir = (uint32_t)(integer << 16 | (fraction & 0xFFFF));
    GetInfoRateReg(p_NonPassthroughAlgParam->rateMode, p_NonPassthroughAlgParam->peakOrAccessiveInfoRate, tsuInTenthNanos, fppShift, &integer, &fraction);
    *pir_eir = (uint32_t)(integer << 16 | (fraction & 0xFFFF));

    *cbs     =  p_NonPassthroughAlgParam->comittedBurstSize;
    *pbs_ebs =  p_NonPassthroughAlgParam->peakOrAccessiveBurstSize;

    /* get fpp as it should be written to reg.*/
    *fpp = FPP_Function(fppShift);

}

static void WritePar(t_FmPcd *p_FmPcd, uint32_t par)
{
    t_FmPcdPlcrRegs *p_FmPcdPlcrRegs    = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    WRITE_UINT32(p_FmPcdPlcrRegs->fmpl_par, par);

    while(GET_UINT32(p_FmPcdPlcrRegs->fmpl_par) & FM_PCD_PLCR_PAR_GO) ;

}

/*********************************************/
/*............Policer Exception..............*/
/*********************************************/
static void PcdPlcrException(t_Handle h_FmPcd)
{
    t_FmPcd *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint32_t event, mask, force;

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    event = GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_evr);
    mask = GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_ier);

    event &= mask;

    /* clear the forced events */
    force = GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_ifr);
    if(force & event)
        WRITE_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_ifr, force & ~event);


    WRITE_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_evr, event);

    if(event & FM_PCD_PLCR_PRAM_SELF_INIT_COMPLETE)
        p_FmPcd->f_Exception(p_FmPcd->h_App,e_FM_PCD_PLCR_EXCEPTION_PRAM_SELF_INIT_COMPLETE);
    if(event & FM_PCD_PLCR_ATOMIC_ACTION_COMPLETE)
        p_FmPcd->f_Exception(p_FmPcd->h_App,e_FM_PCD_PLCR_EXCEPTION_ATOMIC_ACTION_COMPLETE);

}

/* ..... */

static void PcdPlcrErrorException(t_Handle h_FmPcd)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint32_t            event, force, captureReg, mask;

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    event = GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_eevr);
    mask = GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_eier);

    event &= mask;

    /* clear the forced events */
    force = GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_eifr);
    if(force & event)
        WRITE_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_eifr, force & ~event);

    WRITE_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_eevr, event);

    if(event & FM_PCD_PLCR_DOUBLE_ECC)
        p_FmPcd->f_Exception(p_FmPcd->h_App,e_FM_PCD_PLCR_EXCEPTION_DOUBLE_ECC);
    if(event & FM_PCD_PLCR_INIT_ENTRY_ERROR)
    {
        captureReg = GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_upcr);
        /*ASSERT_COND(captureReg & PLCR_ERR_UNINIT_CAP);
        p_UnInitCapt->profileNum = (uint8_t)(captureReg & PLCR_ERR_UNINIT_NUM_MASK);
        p_UnInitCapt->portId = (uint8_t)((captureReg & PLCR_ERR_UNINIT_PID_MASK) >>PLCR_ERR_UNINIT_PID_SHIFT) ;
        p_UnInitCapt->absolute = (bool)(captureReg & PLCR_ERR_UNINIT_ABSOLUTE_MASK);*/
        p_FmPcd->f_FmPcdIndexedException(p_FmPcd->h_App,e_FM_PCD_PLCR_EXCEPTION_INIT_ENTRY_ERROR,(uint16_t)(captureReg & PLCR_ERR_UNINIT_NUM_MASK));
        WRITE_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_upcr, PLCR_ERR_UNINIT_CAP);
    }
}

void FmPcdPlcrUpatePointedOwner(t_Handle h_FmPcd, uint16_t absoluteProfileId, bool add)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid);

    if(add)
        p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].pointedOwners++;
    else
        p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].pointedOwners--;
}

uint32_t FmPcdPlcrGetPointedOwners(t_Handle h_FmPcd, uint16_t absoluteProfileId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid);

    return p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].pointedOwners;
}
uint32_t FmPcdPlcrGetRequiredAction(t_Handle h_FmPcd, uint16_t absoluteProfileId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid);

    return p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].requiredAction;
}

t_Error  FmPcdPlcrAllocProfiles(t_Handle h_FmPcd, uint8_t hardwarePortId, uint16_t numOfProfiles)
{
    t_FmPcd                     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdIpcPlcrAllocParams   ipcPlcrParams;
    t_Error                     err = E_OK;
    uint16_t                    base;
    uint16_t                    swPortIndex = 0;
    t_FmPcdIpcMsg               msg;
    uint32_t                    replyLength;
    t_FmPcdIpcReply             reply;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);

    if(!numOfProfiles)
        return E_OK;

    memset(&ipcPlcrParams, 0, sizeof(ipcPlcrParams));

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        /* Alloc resources using IPC messaging */
        memset(&reply, 0, sizeof(reply));
        memset(&msg, 0, sizeof(msg));
        ipcPlcrParams.num = numOfProfiles;
        ipcPlcrParams.hardwarePortId = hardwarePortId;
        msg.msgId = FM_PCD_ALLOC_PROFILES;
        memcpy(msg.msgBody, &ipcPlcrParams, sizeof(ipcPlcrParams));
        replyLength = sizeof(uint32_t) + sizeof(uint16_t);
        if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) +sizeof(ipcPlcrParams),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MAJOR, err,NO_MSG);
        if (replyLength != sizeof(uint32_t) + sizeof(uint16_t))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        if((t_Error)reply.error != E_OK)
            RETURN_ERROR(MAJOR, (t_Error)reply.error, ("PLCR profiles allocation failed"));

        memcpy((uint8_t*)&base, reply.replyBody, sizeof(uint16_t));
    }
    else /* master */
    {
        err = PlcrAllocProfiles(p_FmPcd, hardwarePortId, numOfProfiles, &base);
        if(err)
            RETURN_ERROR(MAJOR, err,NO_MSG);
    }
    HW_PORT_ID_TO_SW_PORT_INDX(swPortIndex, hardwarePortId);

    p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].numOfProfiles = numOfProfiles;
    p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].profilesBase = base;

    return E_OK;
}

t_Error  FmPcdPlcrFreeProfiles(t_Handle h_FmPcd, uint8_t hardwarePortId)
{
    t_FmPcd                     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdIpcPlcrAllocParams   ipcPlcrParams;
    t_Error                     err = E_OK;
    uint16_t                    swPortIndex = 0;
    t_FmPcdIpcMsg               msg;
    uint32_t                    replyLength;
    t_FmPcdIpcReply             reply;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);

    HW_PORT_ID_TO_SW_PORT_INDX(swPortIndex, hardwarePortId);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        /* Alloc resources using IPC messaging */
        memset(&reply, 0, sizeof(reply));
        memset(&msg, 0, sizeof(msg));
        ipcPlcrParams.num = p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].numOfProfiles;
        ipcPlcrParams.hardwarePortId = hardwarePortId;
        ipcPlcrParams.plcrProfilesBase = p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].profilesBase;
        msg.msgId = FM_PCD_FREE_PROFILES;
        memcpy(msg.msgBody, &ipcPlcrParams, sizeof(ipcPlcrParams));
        replyLength = sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) +sizeof(ipcPlcrParams),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MAJOR, err,NO_MSG);
        if (replyLength != sizeof(uint32_t))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        if ((t_Error)reply.error != E_OK)
            RETURN_ERROR(MINOR, (t_Error)reply.error, ("PLCR Free Profiles failed"));
    }
    else /* master */
    {
        err = PlcrFreeProfiles(p_FmPcd, hardwarePortId, p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].numOfProfiles, p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].profilesBase);
        if(err)
            RETURN_ERROR(MAJOR, err,NO_MSG);
    }
    p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].numOfProfiles = 0;
    p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].profilesBase = 0;

    return E_OK;
}

bool    FmPcdPlcrIsProfileValid(t_Handle h_FmPcd, uint16_t absoluteProfileId)
{
    t_FmPcd         *p_FmPcd            = (t_FmPcd*)h_FmPcd;
    t_FmPcdPlcr     *p_FmPcdPlcr        = p_FmPcd->p_FmPcdPlcr;

    return p_FmPcdPlcr->profiles[absoluteProfileId].valid;
}

t_Error  PlcrAllocProfiles(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, uint16_t numOfProfiles, uint16_t *p_Base)
{
    t_FmPcdPlcrRegs *p_Regs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;
    uint32_t        profilesFound, log2Num, tmpReg32;
    uint32_t        intFlags;
    uint16_t        first, i;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    if(!numOfProfiles)
        return E_OK;

    ASSERT_COND(hardwarePortId);

    if (numOfProfiles>FM_PCD_PLCR_NUM_ENTRIES)
        RETURN_ERROR(MINOR, E_INVALID_VALUE, ("numProfiles is too big."));

    if (!POWER_OF_2(numOfProfiles))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numProfiles must be a power of 2."));

    intFlags = FmPcdLock(p_FmPcd);

    if(GET_UINT32(p_Regs->fmpl_pmr[hardwarePortId-1]) & FM_PCD_PLCR_PMR_V)
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("The requesting port has already an allocated profiles window."));
    }

    first = 0;
    profilesFound = 0;
    for(i=0;i<FM_PCD_PLCR_NUM_ENTRIES;)
    {
        if(!p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.allocated)
        {
            profilesFound++;
            i++;
            if(profilesFound == numOfProfiles)
                break;
        }
        else
        {
            profilesFound = 0;
            /* advance i to the next aligned address */
            first = i = (uint8_t)(first + numOfProfiles);
        }
    }
    if(profilesFound == numOfProfiles)
    {
        for(i = first; i<first + numOfProfiles; i++)
        {
            p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.allocated = TRUE;
            p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.ownerId = hardwarePortId;
        }
    }
    else
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        RETURN_ERROR(MINOR, E_FULL, ("No profiles."));
    }

    /**********************FMPL_PMRx******************/
    LOG2((uint64_t)numOfProfiles, log2Num);
    tmpReg32 = first;
    tmpReg32 |= log2Num << 16;
    tmpReg32 |= FM_PCD_PLCR_PMR_V;
    WRITE_UINT32(p_Regs->fmpl_pmr[hardwarePortId-1], tmpReg32);

    *p_Base = first;

    FmPcdUnlock(p_FmPcd, intFlags);

    return E_OK;
}

t_Error  PlcrAllocSharedProfiles(t_FmPcd *p_FmPcd, uint16_t numOfProfiles, uint16_t *profilesIds)
{
    uint32_t        profilesFound;
    uint16_t        i, k=0;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    if(!numOfProfiles)
        return E_OK;

    if (numOfProfiles>FM_PCD_PLCR_NUM_ENTRIES)
        RETURN_ERROR(MINOR, E_INVALID_VALUE, ("numProfiles is too big."));

    profilesFound = 0;
    for(i=0;i<FM_PCD_PLCR_NUM_ENTRIES; i++)
    {
        if(!p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.allocated)
        {
            profilesFound++;
            profilesIds[k] = i;
            k++;
            if(profilesFound == numOfProfiles)
                break;
        }
    }
    if(profilesFound != numOfProfiles)
        RETURN_ERROR(MAJOR, E_INVALID_STATE,NO_MSG);
    for(i = 0;i<k;i++)
    {
        p_FmPcd->p_FmPcdPlcr->profiles[profilesIds[i]].profilesMng.allocated = TRUE;
        p_FmPcd->p_FmPcdPlcr->profiles[profilesIds[i]].profilesMng.ownerId = 0;
    }

    return E_OK;
}

t_Error  PlcrFreeProfiles(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, uint16_t numOfProfiles, uint16_t base)
{
    t_FmPcdPlcrRegs *p_Regs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;
    uint16_t        i;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_HANDLE);

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    ASSERT_COND(IN_RANGE(1, hardwarePortId, 63));
    WRITE_UINT32(p_Regs->fmpl_pmr[hardwarePortId-1], 0);

    for(i = base; i<base+numOfProfiles;i++)
    {
        ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.ownerId == hardwarePortId);
        ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.allocated);

        p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.allocated = FALSE;
        p_FmPcd->p_FmPcdPlcr->profiles[i].profilesMng.ownerId = 0;
    }

    return E_OK;
}

void  PlcrFreeSharedProfiles(t_FmPcd *p_FmPcd, uint16_t numOfProfiles, uint16_t *profilesIds)
{
    uint16_t        i;

    SANITY_CHECK_RETURN(p_FmPcd, E_INVALID_HANDLE);

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    for(i=0;i<numOfProfiles; i++)
    {
        ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[profilesIds[i]].profilesMng.allocated);
        p_FmPcd->p_FmPcdPlcr->profiles[profilesIds[i]].profilesMng.allocated = FALSE;
    }
}

void PlcrEnable(t_FmPcd *p_FmPcd)
{
    t_FmPcdPlcrRegs             *p_Regs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;

    WRITE_UINT32(p_Regs->fmpl_gcr, GET_UINT32(p_Regs->fmpl_gcr) | FM_PCD_PLCR_GCR_EN);
}

void PlcrDisable(t_FmPcd *p_FmPcd)
{
    t_FmPcdPlcrRegs             *p_Regs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;

    WRITE_UINT32(p_Regs->fmpl_gcr, GET_UINT32(p_Regs->fmpl_gcr) & ~FM_PCD_PLCR_GCR_EN);
}

t_Error FM_PCD_SetPlcrStatistics(t_Handle h_FmPcd, bool enable)
{
   t_FmPcd  *p_FmPcd = (t_FmPcd*)h_FmPcd;
   uint32_t tmpReg32;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE);

    if(!FmIsMaster(p_FmPcd->h_Fm))
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("FM_PCD_SetPlcrStatistics - guest mode!"));

    tmpReg32 =  GET_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_gcr);
    if(enable)
        tmpReg32 |= FM_PCD_PLCR_GCR_STEN;
    else
        tmpReg32 &= ~FM_PCD_PLCR_GCR_STEN;

    WRITE_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_gcr, tmpReg32);
    return E_OK;
}

t_Error FM_PCD_ConfigPlcrAutoRefreshMode(t_Handle h_FmPcd, bool enable)
{
   t_FmPcd *p_FmPcd = (t_FmPcd*)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdDriverParam, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE);

    if(!FmIsMaster(p_FmPcd->h_Fm))
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("FM_PCD_ConfigPlcrAutoRefreshMode - guest mode!"));

    p_FmPcd->p_FmPcdDriverParam->plcrAutoRefresh = enable;

    return E_OK;
}


t_Error FmPcdPlcrBuildProfile(t_Handle h_FmPcd, t_FmPcdPlcrProfileParams *p_Profile, t_FmPcdPlcrInterModuleProfileRegs *p_PlcrRegs)
{

    t_FmPcd         *p_FmPcd            = (t_FmPcd*)h_FmPcd;
    t_Error         err = E_OK;
    uint32_t        pemode, gnia, ynia, rnia;

/* Set G, Y, R Nia */
    err = SetProfileNia(p_FmPcd, p_Profile->nextEngineOnGreen,  &(p_Profile->paramsOnGreen), &gnia);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);
    err = SetProfileNia(p_FmPcd, p_Profile->nextEngineOnYellow, &(p_Profile->paramsOnYellow), &ynia);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);
    err = SetProfileNia(p_FmPcd, p_Profile->nextEngineOnRed,    &(p_Profile->paramsOnRed), &rnia);
   if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);


/* Mode fmpl_pemode */
    pemode = FM_PCD_PLCR_PEMODE_PI;

    switch (p_Profile->algSelection)
    {
        case    e_FM_PCD_PLCR_PASS_THROUGH:
            p_PlcrRegs->fmpl_pecir         = 0;
            p_PlcrRegs->fmpl_pecbs         = 0;
            p_PlcrRegs->fmpl_pepepir_eir   = 0;
            p_PlcrRegs->fmpl_pepbs_ebs     = 0;
            p_PlcrRegs->fmpl_pelts         = 0;
            p_PlcrRegs->fmpl_pects         = 0;
            p_PlcrRegs->fmpl_pepts_ets     = 0;
            pemode &= ~FM_PCD_PLCR_PEMODE_ALG_MASK;
            switch (p_Profile->colorMode)
            {
                case    e_FM_PCD_PLCR_COLOR_BLIND:
                    pemode |= FM_PCD_PLCR_PEMODE_CBLND;
                    switch (p_Profile->color.dfltColor)
                    {
                        case e_FM_PCD_PLCR_GREEN:
                            pemode &= ~FM_PCD_PLCR_PEMODE_DEFC_MASK;
                            break;
                        case e_FM_PCD_PLCR_YELLOW:
                            pemode |= FM_PCD_PLCR_PEMODE_DEFC_Y;
                            break;
                        case e_FM_PCD_PLCR_RED:
                            pemode |= FM_PCD_PLCR_PEMODE_DEFC_R;
                            break;
                        case e_FM_PCD_PLCR_OVERRIDE:
                            pemode |= FM_PCD_PLCR_PEMODE_DEFC_OVERRIDE;
                            break;
                        default:
                            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
                    }

                    break;
                case    e_FM_PCD_PLCR_COLOR_AWARE:
                    pemode &= ~FM_PCD_PLCR_PEMODE_CBLND;
                    break;
                default:
                    RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            }
            break;

        case    e_FM_PCD_PLCR_RFC_2698:
            /* Select algorithm MODE[ALG] = "01" */
            pemode |= FM_PCD_PLCR_PEMODE_ALG_RFC2698;
            if (p_Profile->nonPassthroughAlgParams.comittedInfoRate > p_Profile->nonPassthroughAlgParams.peakOrAccessiveInfoRate)
                RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("in RFC2698 Peak rate must be equal or larger than comittedInfoRate."));
            goto cont_rfc;
        case    e_FM_PCD_PLCR_RFC_4115:
            /* Select algorithm MODE[ALG] = "10" */
            pemode |= FM_PCD_PLCR_PEMODE_ALG_RFC4115;
cont_rfc:
            /* Select Color-Blind / Color-Aware operation (MODE[CBLND]) */
            switch (p_Profile->colorMode)
            {
                case    e_FM_PCD_PLCR_COLOR_BLIND:
                    pemode |= FM_PCD_PLCR_PEMODE_CBLND;
                    break;
                case    e_FM_PCD_PLCR_COLOR_AWARE:
                    pemode &= ~FM_PCD_PLCR_PEMODE_CBLND;
                    /*In color aware more select override color interpretation (MODE[OVCLR]) */
                    switch (p_Profile->color.override)
                    {
                        case e_FM_PCD_PLCR_GREEN:
                            pemode &= ~FM_PCD_PLCR_PEMODE_OVCLR_MASK;
                            break;
                        case e_FM_PCD_PLCR_YELLOW:
                            pemode |= FM_PCD_PLCR_PEMODE_OVCLR_Y;
                            break;
                        case e_FM_PCD_PLCR_RED:
                            pemode |= FM_PCD_PLCR_PEMODE_OVCLR_R;
                            break;
                        case e_FM_PCD_PLCR_OVERRIDE:
                            pemode |= FM_PCD_PLCR_PEMODE_OVCLR_G_NC;
                            break;
                        default:
                            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
                    }
                    break;
                default:
                    RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            }
            /* Select Measurement Unit Mode to BYTE or PACKET (MODE[PKT]) */
            switch (p_Profile->nonPassthroughAlgParams.rateMode)
            {
                case e_FM_PCD_PLCR_BYTE_MODE :
                    pemode &= ~FM_PCD_PLCR_PEMODE_PKT;
                        switch (p_Profile->nonPassthroughAlgParams.byteModeParams.frameLengthSelection)
                        {
                            case e_FM_PCD_PLCR_L2_FRM_LEN:
                                pemode |= FM_PCD_PLCR_PEMODE_FLS_L2;
                                break;
                            case e_FM_PCD_PLCR_L3_FRM_LEN:
                                pemode |= FM_PCD_PLCR_PEMODE_FLS_L3;
                                break;
                            case e_FM_PCD_PLCR_L4_FRM_LEN:
                                pemode |= FM_PCD_PLCR_PEMODE_FLS_L4;
                                break;
                            case e_FM_PCD_PLCR_FULL_FRM_LEN:
                                pemode |= FM_PCD_PLCR_PEMODE_FLS_FULL;
                                break;
                            default:
                                RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
                        }
                        switch (p_Profile->nonPassthroughAlgParams.byteModeParams.rollBackFrameSelection)
                        {
                            case e_FM_PCD_PLCR_ROLLBACK_L2_FRM_LEN:
                                pemode &= ~FM_PCD_PLCR_PEMODE_RBFLS;
                                break;
                            case e_FM_PCD_PLCR_ROLLBACK_FULL_FRM_LEN:
                                pemode |= FM_PCD_PLCR_PEMODE_RBFLS;
                                break;
                            default:
                                RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
                        }
                    break;
                case e_FM_PCD_PLCR_PACKET_MODE :
                    pemode |= FM_PCD_PLCR_PEMODE_PKT;
                    break;
                default:
                    RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            }
            /* Select timeStamp floating point position (MODE[FPP]) to fit the actual traffic rates. For PACKET
               mode with low traffic rates move the fixed point to the left to increase fraction accuracy. For BYTE
               mode with high traffic rates move the fixed point to the right to increase integer accuracy. */

            /* Configure Traffic Parameters*/
            {
                uint32_t cir=0, cbs=0, pir_eir=0, pbs_ebs=0, fpp=0;

                calcRates(h_FmPcd, &p_Profile->nonPassthroughAlgParams, &cir, &cbs, &pir_eir, &pbs_ebs, &fpp);

                /*  Set Committed Information Rate (CIR) */
                p_PlcrRegs->fmpl_pecir = cir;
                /*  Set Committed Burst Size (CBS). */
                p_PlcrRegs->fmpl_pecbs =  cbs;
                /*  Set Peak Information Rate (PIR_EIR used as PIR) */
                p_PlcrRegs->fmpl_pepepir_eir = pir_eir;
                /*   Set Peak Burst Size (PBS_EBS used as PBS) */
                p_PlcrRegs->fmpl_pepbs_ebs = pbs_ebs;

                /* Initialize the Metering Buckets to be full (write them with 0xFFFFFFFF. */
                /* Peak Rate Token Bucket Size (PTS_ETS used as PTS) */
                p_PlcrRegs->fmpl_pepts_ets = 0xFFFFFFFF;
                /* Committed Rate Token Bucket Size (CTS) */
                p_PlcrRegs->fmpl_pects = 0xFFFFFFFF;

                /* Set the FPP based on calculation */
                pemode |= (fpp << FM_PCD_PLCR_PEMODE_FPP_SHIFT);
            }
            break;  /* FM_PCD_PLCR_PEMODE_ALG_RFC2698 , FM_PCD_PLCR_PEMODE_ALG_RFC4115 */
        default:
            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
    }

    p_PlcrRegs->fmpl_pemode = pemode;

    p_PlcrRegs->fmpl_pegnia = gnia;
    p_PlcrRegs->fmpl_peynia = ynia;
    p_PlcrRegs->fmpl_pernia = rnia;

    /* Zero Counters */
    p_PlcrRegs->fmpl_pegpc     = 0;
    p_PlcrRegs->fmpl_peypc     = 0;
    p_PlcrRegs->fmpl_perpc     = 0;
    p_PlcrRegs->fmpl_perypc    = 0;
    p_PlcrRegs->fmpl_perrpc    = 0;

    return E_OK;
}

void  FmPcdPlcrValidateProfileSw(t_Handle h_FmPcd, uint16_t absoluteProfileId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    ASSERT_COND(!p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid);
    p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid = TRUE;
}

void  FmPcdPlcrInvalidateProfileSw(t_Handle h_FmPcd, uint16_t absoluteProfileId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid);
    p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid = FALSE;
}

t_Handle PlcrConfig(t_FmPcd *p_FmPcd, t_FmPcdParams *p_FmPcdParams)
{
    t_FmPcdPlcr *p_FmPcdPlcr;
    /*uint8_t i=0;*/

    UNUSED(p_FmPcd);
    UNUSED(p_FmPcdParams);

    p_FmPcdPlcr = (t_FmPcdPlcr *) XX_Malloc(sizeof(t_FmPcdPlcr));
    if (!p_FmPcdPlcr)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM Policer structure allocation FAILED"));
        return NULL;
    }
    memset(p_FmPcdPlcr, 0, sizeof(t_FmPcdPlcr));
    if(p_FmPcd->guestId == NCSW_MASTER_ID)
    {
        p_FmPcdPlcr->p_FmPcdPlcrRegs  = (t_FmPcdPlcrRegs *)UINT_TO_PTR(FmGetPcdPlcrBaseAddr(p_FmPcdParams->h_Fm));
        p_FmPcd->p_FmPcdDriverParam->plcrAutoRefresh    = DEFAULT_plcrAutoRefresh;
        p_FmPcd->exceptions |= (DEFAULT_fmPcdPlcrExceptions | DEFAULT_fmPcdPlcrErrorExceptions);
    }

    p_FmPcdPlcr->numOfSharedProfiles = DEFAULT_numOfSharedPlcrProfiles;

    return p_FmPcdPlcr;
}

t_Error PlcrInit(t_FmPcd *p_FmPcd)
{
    t_FmPcdDriverParam              *p_Param = p_FmPcd->p_FmPcdDriverParam;
    t_FmPcdPlcr                     *p_FmPcdPlcr = p_FmPcd->p_FmPcdPlcr;
    uint32_t                        tmpReg32 = 0;
    t_Error                         err = E_OK;
    t_FmPcdPlcrRegs                 *p_Regs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;
    t_FmPcdIpcMsg                   msg;
    uint32_t                        replyLength;
    t_FmPcdIpcReply                 reply;

    if ((p_FmPcd->guestId != NCSW_MASTER_ID) &&
        (p_FmPcdPlcr->numOfSharedProfiles))
    {
        int         i, j, index = 0;
        uint32_t    walking1Mask = 0x80000000;
        uint32_t    sharedProfilesMask[FM_PCD_PLCR_NUM_ENTRIES/32];

        memset(sharedProfilesMask, 0, FM_PCD_PLCR_NUM_ENTRIES/32 * sizeof(uint32_t));
        memset(&reply, 0, sizeof(reply));
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_PCD_ALLOC_SHARED_PROFILES;
        memcpy(msg.msgBody, (uint8_t *)&p_FmPcdPlcr->numOfSharedProfiles, sizeof(uint16_t));
        replyLength = sizeof(uint32_t) + sizeof(sharedProfilesMask);
        if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+ sizeof(p_FmPcdPlcr->numOfSharedProfiles),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MAJOR, err,NO_MSG);
        if (replyLength != (sizeof(uint32_t) + sizeof(sharedProfilesMask)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));

        memcpy(sharedProfilesMask, reply.replyBody, sizeof(sharedProfilesMask));
        /* translate 8 regs of 32 bits masks into an array of up to 256 indexes. */
        for(i = 0; i<FM_PCD_PLCR_NUM_ENTRIES/32 ; i++)
        {
            if(sharedProfilesMask[i])
            {
                for(j = 0 ; j<32 ; j++)
                {
                    if(sharedProfilesMask[i] & walking1Mask)
                        p_FmPcd->p_FmPcdPlcr->sharedProfilesIds[index++] = (uint16_t)(i*32+j);
                    walking1Mask >>= 1;
                }
                walking1Mask = 0x80000000;
            }
        }
        return (t_Error)reply.error;
    }

    if(p_FmPcdPlcr->numOfSharedProfiles)
    {
        err = PlcrAllocSharedProfiles(p_FmPcd, p_FmPcdPlcr->numOfSharedProfiles, p_FmPcd->p_FmPcdPlcr->sharedProfilesIds);
        if(err)
            RETURN_ERROR(MAJOR, err,NO_MSG);
    }

    /**********************FMPL_GCR******************/
    tmpReg32 = 0;
    tmpReg32 |= FM_PCD_PLCR_GCR_STEN;
    if(p_Param->plcrAutoRefresh)
        tmpReg32 |= FM_PCD_PLCR_GCR_DAR;
    tmpReg32 |= NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME;

    WRITE_UINT32(p_Regs->fmpl_gcr, tmpReg32);
    /**********************FMPL_GCR******************/

    /**********************FMPL_EEVR******************/
    WRITE_UINT32(p_Regs->fmpl_eevr, (FM_PCD_PLCR_DOUBLE_ECC | FM_PCD_PLCR_INIT_ENTRY_ERROR));
    /**********************FMPL_EEVR******************/
    /**********************FMPL_EIER******************/
    tmpReg32 = 0;
    if(p_FmPcd->exceptions & FM_PCD_EX_PLCR_DOUBLE_ECC)
    {
        FmEnableRamsEcc(p_FmPcd->h_Fm);
        tmpReg32 |= FM_PCD_PLCR_DOUBLE_ECC;
    }
    if(p_FmPcd->exceptions & FM_PCD_EX_PLCR_INIT_ENTRY_ERROR)
        tmpReg32 |= FM_PCD_PLCR_INIT_ENTRY_ERROR;
    WRITE_UINT32(p_Regs->fmpl_eier, tmpReg32);
    /**********************FMPL_EIER******************/

    /**********************FMPL_EVR******************/
    WRITE_UINT32(p_Regs->fmpl_evr, (FM_PCD_PLCR_PRAM_SELF_INIT_COMPLETE | FM_PCD_PLCR_ATOMIC_ACTION_COMPLETE));
    /**********************FMPL_EVR******************/
    /**********************FMPL_IER******************/
    tmpReg32 = 0;
    if(p_FmPcd->exceptions & FM_PCD_EX_PLCR_PRAM_SELF_INIT_COMPLETE)
        tmpReg32 |= FM_PCD_PLCR_PRAM_SELF_INIT_COMPLETE;
    if(p_FmPcd->exceptions & FM_PCD_EX_PLCR_ATOMIC_ACTION_COMPLETE )
        tmpReg32 |= FM_PCD_PLCR_ATOMIC_ACTION_COMPLETE;
    WRITE_UINT32(p_Regs->fmpl_ier, tmpReg32);
    /**********************FMPL_IER******************/

    /* register even if no interrupts enabled, to allow future enablement */
    FmRegisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PLCR, 0, e_FM_INTR_TYPE_ERR, PcdPlcrErrorException, p_FmPcd);
    FmRegisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PLCR, 0, e_FM_INTR_TYPE_NORMAL, PcdPlcrException, p_FmPcd);

    /* driver initializes one DFLT profile at the last entry*/
    /**********************FMPL_DPMR******************/
    tmpReg32 = 0;
    WRITE_UINT32(p_Regs->fmpl_dpmr, tmpReg32);
    p_FmPcd->p_FmPcdPlcr->profiles[0].profilesMng.allocated = TRUE;

    return E_OK;
}

t_Error PlcrFree(t_FmPcd *p_FmPcd)
{
    t_Error                             err;
    t_FmPcdIpcSharedPlcrAllocParams     ipcSharedPlcrParams;
    t_FmPcdIpcMsg                       msg;

    FmUnregisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PLCR, 0, e_FM_INTR_TYPE_ERR);
    FmUnregisterIntr(p_FmPcd->h_Fm, e_FM_MOD_PLCR, 0, e_FM_INTR_TYPE_NORMAL);

    if(p_FmPcd->p_FmPcdPlcr->numOfSharedProfiles)
    {
        if(p_FmPcd->guestId != NCSW_MASTER_ID)
        {
            int i;
            memset(ipcSharedPlcrParams.sharedProfilesMask, 0, sizeof(ipcSharedPlcrParams.sharedProfilesMask));
            /* Free resources using IPC messaging */
            ipcSharedPlcrParams.num = p_FmPcd->p_FmPcdPlcr->numOfSharedProfiles;

            /* translate the allocated profile id's to a 32bit * 8regs mask */
            for(i = 0;i<p_FmPcd->p_FmPcdPlcr->numOfSharedProfiles;i++)
                ipcSharedPlcrParams.sharedProfilesMask[p_FmPcd->p_FmPcdPlcr->sharedProfilesIds[i]/32] |= (0x80000000 >> (p_FmPcd->p_FmPcdPlcr->sharedProfilesIds[i] % 32));

            memset(&msg, 0, sizeof(msg));
            msg.msgId = FM_PCD_FREE_SHARED_PROFILES;
            memcpy(msg.msgBody, &ipcSharedPlcrParams, sizeof(ipcSharedPlcrParams));
            if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                         (uint8_t*)&msg,
                                         sizeof(msg.msgId)+sizeof(ipcSharedPlcrParams),
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL)) != E_OK)
                RETURN_ERROR(MAJOR, err,NO_MSG);
        }
       /* else
            PlcrFreeSharedProfiles(p_FmPcd, p_FmPcd->p_FmPcdPlcr->numOfSharedProfiles, p_FmPcd->p_FmPcdPlcr->sharedProfilesIds);*/
    }
    return E_OK;
}

t_Error     FmPcdPlcrGetAbsoluteProfileId(t_Handle                      h_FmPcd,
                                          e_FmPcdProfileTypeSelection   profileType,
                                          t_Handle                      h_FmPort,
                                          uint16_t                      relativeProfile,
                                          uint16_t                      *p_AbsoluteId)
{
    t_FmPcd         *p_FmPcd            = (t_FmPcd*)h_FmPcd;
    t_FmPcdPlcr     *p_FmPcdPlcr        = p_FmPcd->p_FmPcdPlcr;
    uint8_t         i;

    switch (profileType)
    {
        case e_FM_PCD_PLCR_PORT_PRIVATE:
            /* get port PCD id from port handle */
            for(i=0;i<FM_MAX_NUM_OF_PORTS;i++)
                if(p_FmPcd->p_FmPcdPlcr->portsMapping[i].h_FmPort == h_FmPort)
                    break;
            if (i ==  FM_MAX_NUM_OF_PORTS)
                RETURN_ERROR(MAJOR, E_INVALID_STATE , ("Invalid port handle."));

            if(!p_FmPcd->p_FmPcdPlcr->portsMapping[i].numOfProfiles)
                RETURN_ERROR(MAJOR, E_INVALID_SELECTION , ("Port has no allocated profiles"));
            if(relativeProfile >= p_FmPcd->p_FmPcdPlcr->portsMapping[i].numOfProfiles)
                RETURN_ERROR(MAJOR, E_INVALID_SELECTION , ("Profile id is out of range"));
            *p_AbsoluteId = (uint16_t)(p_FmPcd->p_FmPcdPlcr->portsMapping[i].profilesBase + relativeProfile);
            break;
        case e_FM_PCD_PLCR_SHARED:
            if(relativeProfile >= p_FmPcdPlcr->numOfSharedProfiles)
                RETURN_ERROR(MAJOR, E_INVALID_SELECTION , ("Profile id is out of range"));
            *p_AbsoluteId = (uint16_t)(p_FmPcdPlcr->sharedProfilesIds[relativeProfile]);
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Invalid policer profile type"));
    }
    return E_OK;
}

uint16_t FmPcdPlcrGetPortProfilesBase(t_Handle h_FmPcd, uint8_t hardwarePortId)
{
    t_FmPcd         *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint16_t        swPortIndex = 0;

    HW_PORT_ID_TO_SW_PORT_INDX(swPortIndex, hardwarePortId);

    return p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].profilesBase;
}

uint16_t FmPcdPlcrGetPortNumOfProfiles(t_Handle h_FmPcd, uint8_t hardwarePortId)
{
    t_FmPcd         *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint16_t        swPortIndex = 0;

    HW_PORT_ID_TO_SW_PORT_INDX(swPortIndex, hardwarePortId);

    return p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].numOfProfiles;

}
uint32_t FmPcdPlcrBuildWritePlcrActionReg(uint16_t absoluteProfileId)
{
    return (uint32_t)(FM_PCD_PLCR_PAR_GO |
                      ((uint32_t)absoluteProfileId << FM_PCD_PLCR_PAR_PNUM_SHIFT));
}

uint32_t FmPcdPlcrBuildWritePlcrActionRegs(uint16_t absoluteProfileId)
{
    return (uint32_t)(FM_PCD_PLCR_PAR_GO |
                      ((uint32_t)absoluteProfileId << FM_PCD_PLCR_PAR_PNUM_SHIFT) |
                      FM_PCD_PLCR_PAR_PWSEL_MASK);
}

bool    FmPcdPlcrHwProfileIsValid(uint32_t profileModeReg)
{

    if(profileModeReg & FM_PCD_PLCR_PEMODE_PI)
        return TRUE;
    else
        return FALSE;
}

uint32_t FmPcdPlcrBuildReadPlcrActionReg(uint16_t absoluteProfileId)
{
    return (uint32_t)(FM_PCD_PLCR_PAR_GO |
                      FM_PCD_PLCR_PAR_R |
                      ((uint32_t)absoluteProfileId << FM_PCD_PLCR_PAR_PNUM_SHIFT) |
                      FM_PCD_PLCR_PAR_PWSEL_MASK);
}

uint32_t FmPcdPlcrBuildCounterProfileReg(e_FmPcdPlcrProfileCounters counter)
{
    switch(counter)
    {
        case(e_FM_PCD_PLCR_PROFILE_GREEN_PACKET_TOTAL_COUNTER):
            return FM_PCD_PLCR_PAR_PWSEL_PEGPC;
        case(e_FM_PCD_PLCR_PROFILE_YELLOW_PACKET_TOTAL_COUNTER):
            return FM_PCD_PLCR_PAR_PWSEL_PEYPC;
        case(e_FM_PCD_PLCR_PROFILE_RED_PACKET_TOTAL_COUNTER) :
            return FM_PCD_PLCR_PAR_PWSEL_PERPC;
        case(e_FM_PCD_PLCR_PROFILE_RECOLOURED_YELLOW_PACKET_TOTAL_COUNTER) :
            return FM_PCD_PLCR_PAR_PWSEL_PERYPC;
        case(e_FM_PCD_PLCR_PROFILE_RECOLOURED_RED_PACKET_TOTAL_COUNTER) :
            return FM_PCD_PLCR_PAR_PWSEL_PERRPC;
       default:
            REPORT_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            return 0;
    }
}

uint32_t FmPcdPlcrBuildNiaProfileReg(bool green, bool yellow, bool red)
{

    uint32_t tmpReg32 = 0;

    if(green)
        tmpReg32 |= FM_PCD_PLCR_PAR_PWSEL_PEGNIA;
    if(yellow)
        tmpReg32 |= FM_PCD_PLCR_PAR_PWSEL_PEYNIA;
    if(red)
        tmpReg32 |= FM_PCD_PLCR_PAR_PWSEL_PERNIA;

    return tmpReg32;
}

void FmPcdPlcrUpdateRequiredAction(t_Handle h_FmPcd, uint16_t absoluteProfileId, uint32_t requiredAction)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].valid);

    p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].requiredAction = requiredAction;
}

t_Error FmPcdPlcrProfileTryLock(t_Handle h_FmPcd, uint16_t profileId, bool intr)
{
    t_FmPcd         *p_FmPcd = (t_FmPcd *)h_FmPcd;
    bool            ans;
    if (intr)
        ans = TRY_LOCK(NULL, &p_FmPcd->p_FmPcdPlcr->profiles[profileId].lock);
    else
        ans = TRY_LOCK(p_FmPcd->h_Spinlock, &p_FmPcd->p_FmPcdPlcr->profiles[profileId].lock);
    if (ans)
        return E_OK;
    return ERROR_CODE(E_BUSY);
}

void FmPcdPlcrReleaseProfileLock(t_Handle h_FmPcd, uint16_t profileId)
{
    RELEASE_LOCK(((t_FmPcd*)h_FmPcd)->p_FmPcdPlcr->profiles[profileId].lock);
}

/**************************************************/
/*............Policer API.........................*/
/**************************************************/

t_Handle FM_PCD_PlcrSetProfile(t_Handle     h_FmPcd,
                               t_FmPcdPlcrProfileParams *p_Profile)
{
    t_FmPcd                             *p_FmPcd            = (t_FmPcd*)h_FmPcd;
    t_FmPcdPlcrRegs                     *p_FmPcdPlcrRegs;
    t_FmPcdPlcrInterModuleProfileRegs   plcrProfileReg;
    uint32_t                            intFlags;
    uint16_t                            absoluteProfileId;
    t_Error                             err = E_OK;
    uint32_t                            tmpReg32;

    SANITY_CHECK_RETURN_VALUE(p_FmPcd, E_INVALID_HANDLE, NULL);
    SANITY_CHECK_RETURN_VALUE(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE, NULL);
    SANITY_CHECK_RETURN_VALUE(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE, NULL);

    if (p_FmPcd->h_Hc)
        return FmHcPcdPlcrSetProfile(p_FmPcd->h_Hc, p_Profile);

    p_FmPcdPlcrRegs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;
    SANITY_CHECK_RETURN_VALUE(p_FmPcdPlcrRegs, E_INVALID_HANDLE, NULL);

    if (p_Profile->modify)
    {
        absoluteProfileId = (uint16_t)(PTR_TO_UINT(p_Profile->id.h_Profile)-1);
        if (absoluteProfileId >= FM_PCD_PLCR_NUM_ENTRIES)
        {
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("profileId too Big "));
            return NULL;
        }
        if (FmPcdPlcrProfileTryLock(p_FmPcd, absoluteProfileId, FALSE))
            return NULL;
    }
    else
    {
        intFlags = FmPcdLock(p_FmPcd);
        err = FmPcdPlcrGetAbsoluteProfileId(h_FmPcd,
                                            p_Profile->id.newParams.profileType,
                                            p_Profile->id.newParams.h_FmPort,
                                            p_Profile->id.newParams.relativeProfileId,
                                            &absoluteProfileId);
        if (absoluteProfileId >= FM_PCD_PLCR_NUM_ENTRIES)
        {
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("profileId too Big "));
            return NULL;
        }
        if(err)
        {
            FmPcdUnlock(p_FmPcd, intFlags);
            REPORT_ERROR(MAJOR, err, NO_MSG);
            return NULL;
        }
        err = FmPcdPlcrProfileTryLock(p_FmPcd, absoluteProfileId, TRUE);
        FmPcdUnlock(p_FmPcd, intFlags);
        if (err)
            return NULL;
    }

    /* if no override, check first that this profile is unused */
    if(!p_Profile->modify)
    {
        /* read specified profile into profile registers */
        tmpReg32 = FmPcdPlcrBuildReadPlcrActionReg(absoluteProfileId);
        intFlags = FmPcdLock(p_FmPcd);
        WritePar(p_FmPcd, tmpReg32);
        tmpReg32 = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pemode);
        FmPcdUnlock(p_FmPcd, intFlags);
        if (tmpReg32 & FM_PCD_PLCR_PEMODE_PI)
        {
            RELEASE_LOCK(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].lock);
            REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("Policer Profile is already used"));
            return NULL;
        }
    }

    memset(&plcrProfileReg, 0, sizeof(t_FmPcdPlcrInterModuleProfileRegs));

    err =  FmPcdPlcrBuildProfile(h_FmPcd, p_Profile, &plcrProfileReg);
    if(err)
    {
        RELEASE_LOCK(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].lock);
        REPORT_ERROR(MAJOR, err, NO_MSG);
        return NULL;
    }

    p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].nextEngineOnGreen = p_Profile->nextEngineOnGreen;
    memcpy(&p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].paramsOnGreen, &(p_Profile->paramsOnGreen), sizeof(u_FmPcdPlcrNextEngineParams));

    p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].nextEngineOnYellow = p_Profile->nextEngineOnYellow;
    memcpy(&p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].paramsOnYellow, &(p_Profile->paramsOnYellow), sizeof(u_FmPcdPlcrNextEngineParams));

    p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].nextEngineOnRed = p_Profile->nextEngineOnRed;
    memcpy(&p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].paramsOnRed, &(p_Profile->paramsOnRed), sizeof(u_FmPcdPlcrNextEngineParams));

    intFlags = FmPcdLock(p_FmPcd);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pemode , plcrProfileReg.fmpl_pemode);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pegnia , plcrProfileReg.fmpl_pegnia);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_peynia , plcrProfileReg.fmpl_peynia);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pernia , plcrProfileReg.fmpl_pernia);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pecir  , plcrProfileReg.fmpl_pecir);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pecbs  , plcrProfileReg.fmpl_pecbs);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pepepir_eir,plcrProfileReg.fmpl_pepepir_eir);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pepbs_ebs,plcrProfileReg.fmpl_pepbs_ebs);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pelts  , plcrProfileReg.fmpl_pelts);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pects  , plcrProfileReg.fmpl_pects);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pepts_ets,plcrProfileReg.fmpl_pepts_ets);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pegpc  , plcrProfileReg.fmpl_pegpc);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_peypc  , plcrProfileReg.fmpl_peypc);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perpc  , plcrProfileReg.fmpl_perpc);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perypc , plcrProfileReg.fmpl_perypc);
    WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perrpc , plcrProfileReg.fmpl_perrpc);

    tmpReg32 = FmPcdPlcrBuildWritePlcrActionRegs(absoluteProfileId);
    WritePar(p_FmPcd, tmpReg32);

    FmPcdUnlock(p_FmPcd, intFlags);

    if (!p_Profile->modify)
        FmPcdPlcrValidateProfileSw(p_FmPcd,absoluteProfileId);

    RELEASE_LOCK(p_FmPcd->p_FmPcdPlcr->profiles[absoluteProfileId].lock);

    return UINT_TO_PTR((uint64_t)absoluteProfileId+1);
}

t_Error FM_PCD_PlcrDeleteProfile(t_Handle h_FmPcd, t_Handle h_Profile)
{
    t_FmPcd         *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint16_t        profileIndx = (uint16_t)(PTR_TO_UINT(h_Profile)-1);
    uint32_t        tmpReg32, intFlags;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR((profileIndx < FM_PCD_PLCR_NUM_ENTRIES), E_INVALID_SELECTION);

    if (p_FmPcd->h_Hc)
        return FmHcPcdPlcrDeleteProfile(p_FmPcd->h_Hc, h_Profile);

    FmPcdPlcrInvalidateProfileSw(p_FmPcd,profileIndx);

    intFlags = FmPcdLock(p_FmPcd);
    WRITE_UINT32(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->profileRegs.fmpl_pemode, ~FM_PCD_PLCR_PEMODE_PI);

    tmpReg32 = FmPcdPlcrBuildWritePlcrActionRegs(profileIndx);
    WritePar(p_FmPcd, tmpReg32);
    FmPcdUnlock(p_FmPcd, intFlags);

    return E_OK;
}

/* ......... */
/***************************************************/
/*............Policer Profile Counter..............*/
/***************************************************/
uint32_t FM_PCD_PlcrGetProfileCounter(t_Handle h_FmPcd, t_Handle h_Profile, e_FmPcdPlcrProfileCounters counter)
{
    t_FmPcd         *p_FmPcd    = (t_FmPcd*)h_FmPcd;
    uint16_t        profileIndx = (uint16_t)(PTR_TO_UINT(h_Profile)-1);
    t_FmPcdPlcrRegs *p_FmPcdPlcrRegs;
    uint32_t        intFlags, counterVal = 0;

    SANITY_CHECK_RETURN_VALUE(p_FmPcd, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE, 0);
    SANITY_CHECK_RETURN_VALUE(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE, 0);

    if (p_FmPcd->h_Hc)
        return FmHcPcdPlcrGetProfileCounter(p_FmPcd->h_Hc, h_Profile, counter);

    p_FmPcdPlcrRegs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;
    SANITY_CHECK_RETURN_VALUE(p_FmPcdPlcrRegs, E_INVALID_HANDLE, 0);

    if (profileIndx >= FM_PCD_PLCR_NUM_ENTRIES)
    {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("profileId too Big "));
        return 0;
    }
    intFlags = FmPcdLock(p_FmPcd);
    WritePar(p_FmPcd, FmPcdPlcrBuildReadPlcrActionReg(profileIndx));

    if(!FmPcdPlcrHwProfileIsValid(GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pemode)))
    {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("Uninitialized profile"));
        FmPcdUnlock(p_FmPcd, intFlags);
        return 0;
    }

    switch (counter)
    {
        case e_FM_PCD_PLCR_PROFILE_GREEN_PACKET_TOTAL_COUNTER:
            counterVal = (GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pegpc));
            break;
        case e_FM_PCD_PLCR_PROFILE_YELLOW_PACKET_TOTAL_COUNTER:
            counterVal = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_peypc);
            break;
        case e_FM_PCD_PLCR_PROFILE_RED_PACKET_TOTAL_COUNTER:
            counterVal = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perpc);
            break;
        case e_FM_PCD_PLCR_PROFILE_RECOLOURED_YELLOW_PACKET_TOTAL_COUNTER:
            counterVal = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perypc);
            break;
        case e_FM_PCD_PLCR_PROFILE_RECOLOURED_RED_PACKET_TOTAL_COUNTER:
            counterVal = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perrpc);
            break;
        default:
            REPORT_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            break;
    }
    FmPcdUnlock(p_FmPcd, intFlags);

    return counterVal;
}


t_Error FmPcdPlcrCcGetSetParams(t_Handle h_FmPcd, uint16_t profileIndx ,uint32_t requiredAction)
{
    t_FmPcd         *p_FmPcd           = (t_FmPcd *)h_FmPcd;
    t_FmPcdPlcr     *p_FmPcdPlcr        = p_FmPcd->p_FmPcdPlcr;
    t_FmPcdPlcrRegs *p_FmPcdPlcrRegs    = p_FmPcdPlcr->p_FmPcdPlcrRegs;
    uint32_t        tmpReg32, intFlags;

    if (p_FmPcd->h_Hc)
        return FmHcPcdPlcrCcGetSetParams(p_FmPcd->h_Hc, profileIndx, requiredAction);

    if (profileIndx >= FM_PCD_PLCR_NUM_ENTRIES)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,("Policer profile out of range"));

    if (FmPcdPlcrProfileTryLock(p_FmPcd, profileIndx, FALSE))
        RETURN_ERROR(MAJOR, E_INVALID_STATE,("Lock on PP FAILED"));

    intFlags = FmPcdLock(p_FmPcd);
    WritePar(p_FmPcd, FmPcdPlcrBuildReadPlcrActionReg(profileIndx));

    if(!FmPcdPlcrHwProfileIsValid(GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pemode)))
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        RELEASE_LOCK(p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].lock);
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,("Policer profile is not valid"));
    }

    ASSERT_COND(p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].valid);

    if(!p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].pointedOwners ||
       !(p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].requiredAction & requiredAction))
    {
        if(requiredAction & UPDATE_NIA_ENQ_WITHOUT_DMA)
        {
            if((p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].nextEngineOnGreen!= e_FM_PCD_DONE) ||
               (p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].nextEngineOnYellow!= e_FM_PCD_DONE) ||
               (p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].nextEngineOnRed!= e_FM_PCD_DONE))
            {
                FmPcdUnlock(p_FmPcd, intFlags);
                RETURN_ERROR (MAJOR, E_OK, ("In this case the next engine can be e_FM_PCD_DONE"));
            }

            if(p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].paramsOnGreen.action == e_FM_PCD_ENQ_FRAME)
            {
                tmpReg32 = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pegnia);
                if(!(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)))
                {
                    FmPcdUnlock(p_FmPcd, intFlags);
                    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine of this policer profile has to be assigned to FM_PCD_DONE"));
                }
                tmpReg32 |= NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;
                WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pegnia, tmpReg32);
                tmpReg32 = FmPcdPlcrBuildWritePlcrActionReg(profileIndx);
                tmpReg32 |= FM_PCD_PLCR_PAR_PWSEL_PEGNIA;
                WritePar(p_FmPcd, tmpReg32);
            }

            if(p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].paramsOnYellow.action == e_FM_PCD_ENQ_FRAME)
            {
                tmpReg32 = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_peynia);
                if(!(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)))
                {
                    FmPcdUnlock(p_FmPcd, intFlags);
                    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine of this policer profile has to be assigned to FM_PCD_DONE"));
                }
                tmpReg32 |= NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;
                WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_peynia, tmpReg32);
                tmpReg32 = FmPcdPlcrBuildWritePlcrActionReg(profileIndx);
                tmpReg32 |= FM_PCD_PLCR_PAR_PWSEL_PEYNIA;
                WritePar(p_FmPcd, tmpReg32);
            }

            if(p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].paramsOnRed.action == e_FM_PCD_ENQ_FRAME)
            {
                tmpReg32 = GET_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pernia);
                if(!(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)))
                {
                    FmPcdUnlock(p_FmPcd, intFlags);
                    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next engine of this policer profile has to be assigned to FM_PCD_DONE"));
                }
                tmpReg32 |= NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA;
                WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pernia, tmpReg32);
                tmpReg32 = FmPcdPlcrBuildWritePlcrActionReg(profileIndx);
                tmpReg32 |= FM_PCD_PLCR_PAR_PWSEL_PERNIA;
                WritePar(p_FmPcd, tmpReg32);
            }
        }
    }
    FmPcdUnlock(p_FmPcd, intFlags);

    p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].pointedOwners += 1;
    p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].requiredAction |= requiredAction;

    RELEASE_LOCK(p_FmPcd->p_FmPcdPlcr->profiles[profileIndx].lock);

    return E_OK;
}

t_Error FM_PCD_PlcrSetProfileCounter(t_Handle h_FmPcd, t_Handle h_Profile, e_FmPcdPlcrProfileCounters counter, uint32_t value)
{
    t_FmPcd         *p_FmPcd    = (t_FmPcd*)h_FmPcd;
    uint16_t        profileIndx = (uint16_t)(PTR_TO_UINT(h_Profile)-1);
    t_FmPcdPlcrRegs *p_FmPcdPlcrRegs;
    uint32_t        tmpReg32, intFlags;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE);

    if (p_FmPcd->h_Hc)
        return FmHcPcdPlcrSetProfileCounter(p_FmPcd->h_Hc, h_Profile, counter, value);

    p_FmPcdPlcrRegs = p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs;
    SANITY_CHECK_RETURN_ERROR(p_FmPcdPlcrRegs, E_INVALID_HANDLE);

    intFlags = FmPcdLock(p_FmPcd);
    switch (counter)
    {
        case e_FM_PCD_PLCR_PROFILE_GREEN_PACKET_TOTAL_COUNTER:
             WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_pegpc, value);
             break;
        case e_FM_PCD_PLCR_PROFILE_YELLOW_PACKET_TOTAL_COUNTER:
             WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_peypc, value);
             break;
        case e_FM_PCD_PLCR_PROFILE_RED_PACKET_TOTAL_COUNTER:
             WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perpc, value);
             break;
        case e_FM_PCD_PLCR_PROFILE_RECOLOURED_YELLOW_PACKET_TOTAL_COUNTER:
             WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perypc ,value);
             break;
        case e_FM_PCD_PLCR_PROFILE_RECOLOURED_RED_PACKET_TOTAL_COUNTER:
             WRITE_UINT32(p_FmPcdPlcrRegs->profileRegs.fmpl_perrpc ,value);
             break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
    }

    /*  Activate the atomic write action by writing FMPL_PAR with: GO=1, RW=1, PSI=0, PNUM =
     *  Profile Number, PWSEL=0xFFFF (select all words).
     */
    tmpReg32 = FmPcdPlcrBuildWritePlcrActionReg(profileIndx);
    tmpReg32 |= FmPcdPlcrBuildCounterProfileReg(counter);
    WritePar(p_FmPcd, tmpReg32);
    FmPcdUnlock(p_FmPcd, intFlags);

    return E_OK;
}

t_Error FM_PCD_ConfigPlcrNumOfSharedProfiles(t_Handle h_FmPcd, uint16_t numOfSharedPlcrProfiles)
{
   t_FmPcd *p_FmPcd = (t_FmPcd*)h_FmPcd;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdDriverParam, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE);

    p_FmPcd->p_FmPcdPlcr->numOfSharedProfiles = numOfSharedPlcrProfiles;

    return E_OK;
}


/* ... */

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FM_PCD_PlcrDumpRegs(t_Handle h_FmPcd)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    int                 i = 0;
    t_FmPcdIpcMsg       msg;

    DECLARE_DUMP;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_PCD_PLCR_DUMP_REGS;
        return XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                 (uint8_t*)&msg,
                                 sizeof(msg.msgId),
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
    }
    else
    {
        DUMP_SUBTITLE(("\n"));
        DUMP_TITLE(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs, ("FmPcdPlcrRegs Regs"));

        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_gcr);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_gsr);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_evr);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_ier);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_ifr);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_eevr);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_eier);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_eifr);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_rpcnt);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_ypcnt);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_rrpcnt);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_rypcnt);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_tpcnt);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_flmcnt);

        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_serc);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_upcr);
        DUMP_VAR(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs,fmpl_dpmr);


        DUMP_TITLE(&p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_pmr, ("fmpl_pmr"));
        DUMP_SUBSTRUCT_ARRAY(i, 63)
        {
            DUMP_MEMORY(&p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->fmpl_pmr[i], sizeof(uint32_t));
        }

        return E_OK;
    }
}

t_Error FM_PCD_PlcrProfileDumpRegs(t_Handle h_FmPcd, t_Handle h_Profile)
{
    t_FmPcd                             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdPlcrInterModuleProfileRegs   *p_ProfilesRegs;
    uint32_t                            tmpReg, intFlags;
    uint16_t                            profileIndx = (uint16_t)(PTR_TO_UINT(h_Profile)-1);
    t_FmPcdIpcMsg                       msg;

    DECLARE_DUMP;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdPlcr, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_PCD_PLCR_PROFILE_DUMP_REGS;
        memcpy(msg.msgBody, (uint8_t *)&h_Profile, sizeof(uint32_t));
        return XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                 (uint8_t*)&msg,
                                 sizeof(msg.msgId) + sizeof(uint32_t),
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
    }
    else
    {
        DUMP_SUBTITLE(("\n"));
        DUMP_TITLE(p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs, ("FmPcdPlcrRegs Profile Regs"));

        p_ProfilesRegs = &p_FmPcd->p_FmPcdPlcr->p_FmPcdPlcrRegs->profileRegs;

        tmpReg = FmPcdPlcrBuildReadPlcrActionReg((uint16_t)profileIndx);
        intFlags = FmPcdLock(p_FmPcd);
        WritePar(p_FmPcd, tmpReg);

        DUMP_TITLE(p_ProfilesRegs, ("Profile %d regs", profileIndx));

        DUMP_VAR(p_ProfilesRegs, fmpl_pemode);
        DUMP_VAR(p_ProfilesRegs, fmpl_pegnia);
        DUMP_VAR(p_ProfilesRegs, fmpl_peynia);
        DUMP_VAR(p_ProfilesRegs, fmpl_pernia);
        DUMP_VAR(p_ProfilesRegs, fmpl_pecir);
        DUMP_VAR(p_ProfilesRegs, fmpl_pecbs);
        DUMP_VAR(p_ProfilesRegs, fmpl_pepepir_eir);
        DUMP_VAR(p_ProfilesRegs, fmpl_pepbs_ebs);
        DUMP_VAR(p_ProfilesRegs, fmpl_pelts);
        DUMP_VAR(p_ProfilesRegs, fmpl_pects);
        DUMP_VAR(p_ProfilesRegs, fmpl_pepts_ets);
        DUMP_VAR(p_ProfilesRegs, fmpl_pegpc);
        DUMP_VAR(p_ProfilesRegs, fmpl_peypc);
        DUMP_VAR(p_ProfilesRegs, fmpl_perpc);
        DUMP_VAR(p_ProfilesRegs, fmpl_perypc);
        DUMP_VAR(p_ProfilesRegs, fmpl_perrpc);
        FmPcdUnlock(p_FmPcd, intFlags);

        return E_OK;
    }
}
#endif /* (defined(DEBUG_ERRORS) && ... */
