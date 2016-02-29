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
 @File          fm.c

 @Description   FM driver routines implementation.
*//***************************************************************************/
#include "std_ext.h"
#include "error_ext.h"
#include "xx_ext.h"
#include "string_ext.h"
#include "sprint_ext.h"
#include "debug_ext.h"
#include "fm_muram_ext.h"

#include "fm_common.h"
#include "fm_ipc.h"
#include "fm.h"


/****************************************/
/*       static functions               */
/****************************************/

static volatile bool blockingFlag = FALSE;
static void IpcMsgCompletionCB(t_Handle   h_Fm,
                               uint8_t    *p_Msg,
                               uint8_t    *p_Reply,
                               uint32_t   replyLength,
                               t_Error    status)
{
    UNUSED(h_Fm);UNUSED(p_Msg);UNUSED(p_Reply);UNUSED(replyLength);UNUSED(status);
    blockingFlag = FALSE;
}

static bool IsFmanCtrlCodeLoaded(t_Fm *p_Fm)
{
    t_FMIramRegs    *p_Iram;

    ASSERT_COND(p_Fm);
    p_Iram = (t_FMIramRegs *)UINT_TO_PTR(p_Fm->baseAddr + FM_MM_IMEM);

    return (bool)!!(GET_UINT32(p_Iram->iready) & IRAM_READY);
}

static t_Error CheckFmParameters(t_Fm *p_Fm)
{
    if (IsFmanCtrlCodeLoaded(p_Fm) && !p_Fm->p_FmDriverParam->resetOnInit)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Old FMan CTRL code is loaded; FM must be reset!"));
    if(!p_Fm->p_FmDriverParam->dmaAxiDbgNumOfBeats || (p_Fm->p_FmDriverParam->dmaAxiDbgNumOfBeats > DMA_MODE_MAX_AXI_DBG_NUM_OF_BEATS))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("axiDbgNumOfBeats has to be in the range 1 - %d", DMA_MODE_MAX_AXI_DBG_NUM_OF_BEATS));
    if(p_Fm->p_FmDriverParam->dmaCamNumOfEntries % DMA_CAM_UNITS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaCamNumOfEntries has to be divisble by %d", DMA_CAM_UNITS));
    if(!p_Fm->p_FmDriverParam->dmaCamNumOfEntries || (p_Fm->p_FmDriverParam->dmaCamNumOfEntries > DMA_MODE_MAX_CAM_NUM_OF_ENTRIES))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaCamNumOfEntries has to be in the range 1 - %d", DMA_MODE_MAX_CAM_NUM_OF_ENTRIES));
    if(p_Fm->p_FmDriverParam->dmaCommQThresholds.assertEmergency > DMA_THRESH_MAX_COMMQ)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaCommQThresholds.assertEmergency can not be larger than %d", DMA_THRESH_MAX_COMMQ));
    if(p_Fm->p_FmDriverParam->dmaCommQThresholds.clearEmergency > DMA_THRESH_MAX_COMMQ)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaCommQThresholds.clearEmergency can not be larger than %d", DMA_THRESH_MAX_COMMQ));
    if(p_Fm->p_FmDriverParam->dmaCommQThresholds.clearEmergency >= p_Fm->p_FmDriverParam->dmaCommQThresholds.assertEmergency)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaCommQThresholds.clearEmergency must be smaller than dmaCommQThresholds.assertEmergency"));
    if(p_Fm->p_FmDriverParam->dmaReadBufThresholds.assertEmergency > DMA_THRESH_MAX_BUF)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaReadBufThresholds.assertEmergency can not be larger than %d", DMA_THRESH_MAX_BUF));
    if(p_Fm->p_FmDriverParam->dmaReadBufThresholds.clearEmergency > DMA_THRESH_MAX_BUF)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaReadBufThresholds.clearEmergency can not be larger than %d", DMA_THRESH_MAX_BUF));
    if(p_Fm->p_FmDriverParam->dmaReadBufThresholds.clearEmergency >= p_Fm->p_FmDriverParam->dmaReadBufThresholds.assertEmergency)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaReadBufThresholds.clearEmergency must be smaller than dmaReadBufThresholds.assertEmergency"));
    if(p_Fm->p_FmDriverParam->dmaWriteBufThresholds.assertEmergency > DMA_THRESH_MAX_BUF)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaWriteBufThresholds.assertEmergency can not be larger than %d", DMA_THRESH_MAX_BUF));
    if(p_Fm->p_FmDriverParam->dmaWriteBufThresholds.clearEmergency > DMA_THRESH_MAX_BUF)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaWriteBufThresholds.clearEmergency can not be larger than %d", DMA_THRESH_MAX_BUF));
    if(p_Fm->p_FmDriverParam->dmaWriteBufThresholds.clearEmergency >= p_Fm->p_FmDriverParam->dmaWriteBufThresholds.assertEmergency)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dmaWriteBufThresholds.clearEmergency must be smaller than dmaWriteBufThresholds.assertEmergency"));

    if(!p_Fm->p_FmStateStruct->fmClkFreq)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fmClkFreq must be set."));
    if (USEC_TO_CLK(p_Fm->p_FmDriverParam->dmaWatchdog, p_Fm->p_FmStateStruct->fmClkFreq) > DMA_MAX_WATCHDOG)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                     ("dmaWatchdog depends on FM clock. dmaWatchdog(in microseconds) * clk (in Mhz), may not exceed 0x08x", DMA_MAX_WATCHDOG));

#ifdef FM_PARTITION_ARRAY
    {
        t_FmRevisionInfo revInfo;
        uint8_t     i;

        FM_GetRevision(p_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            for (i=0; i<FM_SIZE_OF_LIODN_TABLE; i++)
                if (p_Fm->p_FmDriverParam->liodnBasePerPort[i] & ~FM_LIODN_BASE_MASK)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("liodn number is out of range"));
    }
#endif /* FM_PARTITION_ARRAY */

    if(p_Fm->p_FmStateStruct->totalFifoSize % BMI_FIFO_UNITS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("totalFifoSize number has to be divisible by %d", BMI_FIFO_UNITS));
    if(!p_Fm->p_FmStateStruct->totalFifoSize || (p_Fm->p_FmStateStruct->totalFifoSize > BMI_MAX_FIFO_SIZE))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("totalFifoSize number has to be in the range 256 - %d", BMI_MAX_FIFO_SIZE));
    if(!p_Fm->p_FmStateStruct->totalNumOfTasks || (p_Fm->p_FmStateStruct->totalNumOfTasks > BMI_MAX_NUM_OF_TASKS))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("totalNumOfTasks number has to be in the range 1 - %d", BMI_MAX_NUM_OF_TASKS));
    if(!p_Fm->p_FmStateStruct->maxNumOfOpenDmas || (p_Fm->p_FmStateStruct->maxNumOfOpenDmas > BMI_MAX_NUM_OF_DMAS))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("maxNumOfOpenDmas number has to be in the range 1 - %d", BMI_MAX_NUM_OF_DMAS));

    if(p_Fm->p_FmDriverParam->thresholds.dispLimit > FPM_MAX_DISP_LIMIT)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("thresholds.dispLimit can't be greater than %d", FPM_MAX_DISP_LIMIT));

    if(!p_Fm->f_Exception)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Exceptions callback not provided"));
    if(!p_Fm->f_BusError)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Exceptions callback not provided"));

    return E_OK;
}

static void SendIpcIsr(t_Fm *p_Fm, uint32_t macEvent, uint32_t pendingReg)
{
    t_Error     err;
    t_FmIpcIsr  fmIpcIsr;
    t_FmIpcMsg  msg;

    ASSERT_COND(p_Fm->guestId == NCSW_MASTER_ID);
    ASSERT_COND(p_Fm->h_IpcSessions[p_Fm->intrMng[macEvent].guestId]);
    if (p_Fm->intrMng[macEvent].guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_GUEST_ISR;
        fmIpcIsr.pendingReg = pendingReg;
        fmIpcIsr.boolErr = FALSE;
        memcpy(msg.msgBody, &fmIpcIsr, sizeof(fmIpcIsr));
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[p_Fm->intrMng[macEvent].guestId],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) + sizeof(fmIpcIsr),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            REPORT_ERROR(MINOR, err, NO_MSG);
        return;
    }
    else
        p_Fm->intrMng[macEvent].f_Isr(p_Fm->intrMng[macEvent].h_SrcHandle);
}

static void    BmiErrEvent(t_Fm *p_Fm)
{
    uint32_t    event, mask, force;

    event = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_ievr);
    mask = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier);
    event &= mask;

    /* clear the forced events */
    force = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_ifr);
    if(force & event)
        WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ifr, force & ~event);


    /* clear the acknowledged events */
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ievr, event);

    if(event & BMI_ERR_INTR_EN_PIPELINE_ECC)
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_BMI_PIPELINE_ECC);
    if(event & BMI_ERR_INTR_EN_LIST_RAM_ECC)
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_BMI_LIST_RAM_ECC);
    if(event & BMI_ERR_INTR_EN_STATISTICS_RAM_ECC)
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_BMI_STATISTICS_RAM_ECC);
    if(event & BMI_ERR_INTR_EN_DISPATCH_RAM_ECC)
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_BMI_DISPATCH_RAM_ECC);
}

static void    QmiErrEvent(t_Fm *p_Fm)
{
    uint32_t    event, mask, force;

    event = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_eie);
    mask = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_eien);

    event &= mask;

    /* clear the forced events */
    force = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_eif);
    if(force & event)
        WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eif, force & ~event);

    /* clear the acknowledged events */
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eie, event);

    if(event & QMI_ERR_INTR_EN_DOUBLE_ECC)
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_QMI_DOUBLE_ECC);
    if(event & QMI_ERR_INTR_EN_DEQ_FROM_DEF)
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID);
}

static void    DmaErrEvent(t_Fm *p_Fm)
{
    uint64_t            addr=0;
    uint32_t            status, mask, tmpReg=0;
    uint8_t             tnum;
    uint8_t             hardwarePortId;
    uint8_t             relativePortId;
    uint16_t            liodn;

    status = GET_UINT32(p_Fm->p_FmDmaRegs->fmdmsr);
    mask = GET_UINT32(p_Fm->p_FmDmaRegs->fmdmmr);

    /* get bus error regs befor clearing BER */
    if ((status & DMA_STATUS_BUS_ERR) && (mask & DMA_MODE_BER))
    {
        addr = (uint64_t)GET_UINT32(p_Fm->p_FmDmaRegs->fmdmtal);
        addr |= ((uint64_t)(GET_UINT32(p_Fm->p_FmDmaRegs->fmdmtah)) << 32);

        /* get information about the owner of that bus error */
        tmpReg = GET_UINT32(p_Fm->p_FmDmaRegs->fmdmtcid);
    }

    /* clear set events */
    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmsr, status);

    if ((status & DMA_STATUS_BUS_ERR) && (mask & DMA_MODE_BER))
    {
        hardwarePortId = (uint8_t)(((tmpReg & DMA_TRANSFER_PORTID_MASK) >> DMA_TRANSFER_PORTID_SHIFT));
        HW_PORT_ID_TO_SW_PORT_ID(relativePortId, hardwarePortId);
        tnum = (uint8_t)((tmpReg & DMA_TRANSFER_TNUM_MASK) >> DMA_TRANSFER_TNUM_SHIFT);
        liodn = (uint16_t)(tmpReg & DMA_TRANSFER_LIODN_MASK);
        ASSERT_COND(p_Fm->p_FmStateStruct->portsTypes[hardwarePortId] != e_FM_PORT_TYPE_DUMMY);
        p_Fm->f_BusError(p_Fm->h_App, p_Fm->p_FmStateStruct->portsTypes[hardwarePortId], relativePortId, addr, tnum, liodn);
    }
    if(mask & DMA_MODE_ECC)
    {
        if (status & DMA_STATUS_READ_ECC)
            p_Fm->f_Exception(p_Fm->h_App, e_FM_EX_DMA_READ_ECC);
        if (status & DMA_STATUS_SYSTEM_WRITE_ECC)
            p_Fm->f_Exception(p_Fm->h_App, e_FM_EX_DMA_SYSTEM_WRITE_ECC);
        if (status & DMA_STATUS_FM_WRITE_ECC)
            p_Fm->f_Exception(p_Fm->h_App, e_FM_EX_DMA_FM_WRITE_ECC);
    }
}

static void    FpmErrEvent(t_Fm *p_Fm)
{
    uint32_t    event;

    event = GET_UINT32(p_Fm->p_FmFpmRegs->fpmem);

    /* clear the all occurred events */
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmem, event);

    if((event  & FPM_EV_MASK_DOUBLE_ECC) && (event & FPM_EV_MASK_DOUBLE_ECC_EN))
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_FPM_DOUBLE_ECC);
    if((event  & FPM_EV_MASK_STALL) && (event & FPM_EV_MASK_STALL_EN))
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_FPM_STALL_ON_TASKS);
    if((event  & FPM_EV_MASK_SINGLE_ECC) && (event & FPM_EV_MASK_SINGLE_ECC_EN))
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_FPM_SINGLE_ECC);
}

static void    MuramErrIntr(t_Fm *p_Fm)
{
    uint32_t    event, mask;

    event = GET_UINT32(p_Fm->p_FmFpmRegs->fmrcr);
    mask = GET_UINT32(p_Fm->p_FmFpmRegs->fmrie);

    /* clear MURAM event bit */
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, event & ~FPM_RAM_CTL_IRAM_ECC);

    ASSERT_COND(event & FPM_RAM_CTL_MURAM_ECC);
    ASSERT_COND(event & FPM_RAM_CTL_RAMS_ECC_EN);

    if ((mask & FPM_MURAM_ECC_ERR_EX_EN))
        p_Fm->f_Exception(p_Fm->h_App, e_FM_EX_MURAM_ECC);
}

static void IramErrIntr(t_Fm *p_Fm)
{
    uint32_t    event, mask;

    event = GET_UINT32(p_Fm->p_FmFpmRegs->fmrcr) ;
    mask = GET_UINT32(p_Fm->p_FmFpmRegs->fmrie);
    /* clear the acknowledged events (do not clear IRAM event) */
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, event & ~FPM_RAM_CTL_MURAM_ECC);

    ASSERT_COND(event & FPM_RAM_CTL_IRAM_ECC);
    ASSERT_COND(event & FPM_RAM_CTL_IRAM_ECC_EN);

    if ((mask & FPM_IRAM_ECC_ERR_EX_EN))
        p_Fm->f_Exception(p_Fm->h_App, e_FM_EX_IRAM_ECC);
}

static void QmiEvent(t_Fm *p_Fm)
{
    uint32_t    event, mask, force;

    event = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_ie);
    mask = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_ien);

    event &= mask;

    /* clear the forced events */
    force = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_if);
    if(force & event)
        WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_if, force & ~event);

    /* clear the acknowledged events */
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_ie, event);

    if(event & QMI_INTR_EN_SINGLE_ECC)
        p_Fm->f_Exception(p_Fm->h_App,e_FM_EX_QMI_SINGLE_ECC);
}

static void UnimplementedIsr(t_Handle h_Arg)
{
    UNUSED(h_Arg);

    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Unimplemented Isr!"));
}

static void UnimplementedFmanCtrlIsr(t_Handle h_Arg, uint32_t event)
{
    UNUSED(h_Arg); UNUSED(event);

    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Unimplemented FmCtl Isr!"));
}

static void FmEnableTimeStamp(t_Fm *p_Fm)
{
    uint32_t                tmpReg;
    uint64_t                fraction;
    uint32_t                integer;
    uint8_t                 count1MicroBit = 8;
    uint32_t                tsFrequency = (uint32_t)(1<<count1MicroBit); /* in Mhz */

    /* configure timestamp so that bit 8 will count 1 microsecond */
    /* Find effective count rate at TIMESTAMP least significant bits:
       Effective_Count_Rate = 1MHz x 2^8 = 256MHz
       Find frequency ratio between effective count rate and the clock:
       Effective_Count_Rate / CLK e.g. for 600 MHz clock:
       256/600 = 0.4266666... */
    integer = tsFrequency/p_Fm->p_FmStateStruct->fmClkFreq;
    /* we multiply by 2^16 to keep the fraction of the division */
    /* we do not divid back, since we write this value as fraction - see spec */
    fraction = ((tsFrequency << 16) - (integer << 16)*p_Fm->p_FmStateStruct->fmClkFreq)/p_Fm->p_FmStateStruct->fmClkFreq;
    /* we check remainder of the division in order to round up if not integer */
    if(((tsFrequency << 16) - (integer << 16)*p_Fm->p_FmStateStruct->fmClkFreq) % p_Fm->p_FmStateStruct->fmClkFreq)
        fraction++;

    tmpReg = (integer << FPM_TS_INT_SHIFT) | (uint16_t)fraction;
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmtsc2, tmpReg);

    /* enable timestamp with original clock */
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmtsc1, FPM_TS_CTL_EN);

    p_Fm->p_FmStateStruct->count1MicroBit = count1MicroBit;
    p_Fm->p_FmStateStruct->enabledTimeStamp = TRUE;
}

static void FreeInitResources(t_Fm *p_Fm)
{
    if (p_Fm->camBaseAddr)
       FM_MURAM_FreeMem(p_Fm->h_FmMuram, UINT_TO_PTR(p_Fm->camBaseAddr));
    if (p_Fm->fifoBaseAddr)
       FM_MURAM_FreeMem(p_Fm->h_FmMuram, UINT_TO_PTR(p_Fm->fifoBaseAddr));
    if (p_Fm->resAddr)
       FM_MURAM_FreeMem(p_Fm->h_FmMuram, UINT_TO_PTR(p_Fm->resAddr));
}

static t_Error ClearIRam(t_Fm *p_Fm)
{
    t_FMIramRegs    *p_Iram;
    int             i;

    ASSERT_COND(p_Fm);
    p_Iram = (t_FMIramRegs *)UINT_TO_PTR(p_Fm->baseAddr + FM_MM_IMEM);

    /* Enable the auto-increment */
    WRITE_UINT32(p_Iram->iadd, IRAM_IADD_AIE);
    while (GET_UINT32(p_Iram->iadd) != IRAM_IADD_AIE) ;

    for (i=0; i < (FM_IRAM_SIZE/4); i++)
        WRITE_UINT32(p_Iram->idata, 0xffffffff);

    WRITE_UINT32(p_Iram->iadd, FM_IRAM_SIZE - 4);
    CORE_MemoryBarrier();
    while (GET_UINT32(p_Iram->idata) != 0xffffffff) ;

    return E_OK;
}

static t_Error LoadFmanCtrlCode(t_Fm *p_Fm)
{
    t_FMIramRegs    *p_Iram;
    int             i;
    uint32_t        tmp;
    uint8_t         compTo16;

    ASSERT_COND(p_Fm);
    p_Iram = (t_FMIramRegs *)UINT_TO_PTR(p_Fm->baseAddr + FM_MM_IMEM);

    /* Enable the auto-increment */
    WRITE_UINT32(p_Iram->iadd, IRAM_IADD_AIE);
    while (GET_UINT32(p_Iram->iadd) != IRAM_IADD_AIE) ;

    for (i=0; i < (p_Fm->p_FmDriverParam->firmware.size / 4); i++)
        WRITE_UINT32(p_Iram->idata, p_Fm->p_FmDriverParam->firmware.p_Code[i]);

    compTo16 = (uint8_t)(p_Fm->p_FmDriverParam->firmware.size % 16);
    if(compTo16)
        for (i=0; i < ((16-compTo16) / 4); i++)
            WRITE_UINT32(p_Iram->idata, 0xffffffff);

    WRITE_UINT32(p_Iram->iadd,p_Fm->p_FmDriverParam->firmware.size-4);
    while(GET_UINT32(p_Iram->iadd) != (p_Fm->p_FmDriverParam->firmware.size-4)) ;

    /* verify that writing has completed */
    while (GET_UINT32(p_Iram->idata) != p_Fm->p_FmDriverParam->firmware.p_Code[(p_Fm->p_FmDriverParam->firmware.size / 4)-1]) ;

    if (p_Fm->p_FmDriverParam->fwVerify)
    {
        WRITE_UINT32(p_Iram->iadd, IRAM_IADD_AIE);
        while (GET_UINT32(p_Iram->iadd) != IRAM_IADD_AIE) ;
        for (i=0; i < (p_Fm->p_FmDriverParam->firmware.size / 4); i++)
            if ((tmp=GET_UINT32(p_Iram->idata)) != p_Fm->p_FmDriverParam->firmware.p_Code[i])
                RETURN_ERROR(MAJOR, E_WRITE_FAILED,
                             ("UCode write error : write 0x%x, read 0x%x",
                              p_Fm->p_FmDriverParam->firmware.p_Code[i],tmp));
        WRITE_UINT32(p_Iram->iadd, 0x0);
    }

    /* Enable patch from IRAM */
    WRITE_UINT32(p_Iram->iready, IRAM_READY);
    XX_UDelay(1000);

    DBG(INFO, ("FMan-Controller code (ver %d.%d) loaded to IRAM.",
               ((uint8_t *)p_Fm->p_FmDriverParam->firmware.p_Code)[5],
               ((uint8_t *)p_Fm->p_FmDriverParam->firmware.p_Code)[7]));

    return E_OK;
}

static void GuestErrorIsr(t_Fm *p_Fm, uint32_t pending)
{
#define FM_G_CALL_1G_MAC_ERR_ISR(_id)   \
do {                                    \
    p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_ERR_1G_MAC0+_id)].f_Isr(p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_ERR_1G_MAC0+_id)].h_SrcHandle);\
} while (0)
#define FM_G_CALL_10G_MAC_ERR_ISR(_id)  \
do {                                    \
    p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_ERR_10G_MAC0+_id)].f_Isr(p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_ERR_10G_MAC0+_id)].h_SrcHandle);\
} while (0)

    /* error interrupts */
    if (pending & ERR_INTR_EN_1G_MAC0)
        FM_G_CALL_1G_MAC_ERR_ISR(0);
    if (pending & ERR_INTR_EN_1G_MAC1)
        FM_G_CALL_1G_MAC_ERR_ISR(1);
    if (pending & ERR_INTR_EN_1G_MAC2)
        FM_G_CALL_1G_MAC_ERR_ISR(2);
    if (pending & ERR_INTR_EN_1G_MAC3)
        FM_G_CALL_1G_MAC_ERR_ISR(3);
    if (pending & ERR_INTR_EN_1G_MAC4)
        FM_G_CALL_1G_MAC_ERR_ISR(4);
    if (pending & ERR_INTR_EN_10G_MAC0)
        FM_G_CALL_10G_MAC_ERR_ISR(0);
}

static void GuestEventIsr(t_Fm *p_Fm, uint32_t pending)
{
#define FM_G_CALL_1G_MAC_TMR_ISR(_id)   \
do {                                    \
    p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_1G_MAC0_TMR+_id)].f_Isr(p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_1G_MAC0_TMR+_id)].h_SrcHandle);\
} while (0)

    if (pending & INTR_EN_1G_MAC0_TMR)
        FM_G_CALL_1G_MAC_TMR_ISR(0);
    if (pending & INTR_EN_1G_MAC1_TMR)
        FM_G_CALL_1G_MAC_TMR_ISR(1);
    if (pending & INTR_EN_1G_MAC2_TMR)
        FM_G_CALL_1G_MAC_TMR_ISR(2);
    if (pending & INTR_EN_1G_MAC3_TMR)
        FM_G_CALL_1G_MAC_TMR_ISR(3);
    if (pending & INTR_EN_1G_MAC4_TMR)
        FM_G_CALL_1G_MAC_TMR_ISR(4);
    if(pending & INTR_EN_TMR)
        p_Fm->intrMng[e_FM_EV_TMR].f_Isr(p_Fm->intrMng[e_FM_EV_TMR].h_SrcHandle);
}


/****************************************/
/*       Inter-Module functions         */
/****************************************/
static t_Error FmGuestHandleIpcMsgCB(t_Handle  h_Fm,
                                     uint8_t   *p_Msg,
                                     uint32_t  msgLength,
                                     uint8_t   *p_Reply,
                                     uint32_t  *p_ReplyLength)
{
    t_Fm            *p_Fm       = (t_Fm*)h_Fm;
    t_FmIpcMsg      *p_IpcMsg   = (t_FmIpcMsg*)p_Msg;

    UNUSED(p_Reply);
    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR((msgLength > sizeof(uint32_t)), E_INVALID_VALUE);

#ifdef DISABLE_SANITY_CHECKS
    UNUSED(msgLength);
#endif /* DISABLE_SANITY_CHECKS */

    ASSERT_COND(p_Msg);

    *p_ReplyLength = 0;

    switch(p_IpcMsg->msgId)
    {
        case (FM_GUEST_ISR):
        {
            t_FmIpcIsr ipcIsr;

            memcpy((uint8_t*)&ipcIsr, p_IpcMsg->msgBody, sizeof(t_FmIpcIsr));
            if(ipcIsr.boolErr)
                GuestErrorIsr(p_Fm, ipcIsr.pendingReg);
            else
                GuestEventIsr(p_Fm, ipcIsr.pendingReg);
            break;
        }
        default:
            *p_ReplyLength = 0;
            RETURN_ERROR(MINOR, E_INVALID_SELECTION, ("command not found!!!"));
    }
    return E_OK;
}

static t_Error FmHandleIpcMsgCB(t_Handle  h_Fm,
                                uint8_t   *p_Msg,
                                uint32_t  msgLength,
                                uint8_t   *p_Reply,
                                uint32_t  *p_ReplyLength)
{
    t_Fm            *p_Fm       = (t_Fm*)h_Fm;
    t_FmIpcMsg      *p_IpcMsg   = (t_FmIpcMsg*)p_Msg;
    t_FmIpcReply    *p_IpcReply = (t_FmIpcReply*)p_Reply;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR((msgLength >= sizeof(uint32_t)), E_INVALID_VALUE);

#ifdef DISABLE_SANITY_CHECKS
    UNUSED(msgLength);
#endif /* DISABLE_SANITY_CHECKS */

    ASSERT_COND(p_IpcMsg);

    memset(p_IpcReply, 0, (sizeof(uint8_t) * FM_IPC_MAX_REPLY_SIZE));
    *p_ReplyLength = 0;

    switch(p_IpcMsg->msgId)
    {
        case (FM_GET_SET_PORT_PARAMS):
        {
            t_FmIpcPortInInitParams         ipcInitParams;
            t_FmInterModulePortInitParams   initParams;
            t_FmIpcPhysAddr                 ipcPhysAddr;

            memcpy((uint8_t*)&ipcInitParams, p_IpcMsg->msgBody, sizeof(t_FmIpcPortInInitParams));
            initParams.hardwarePortId = ipcInitParams.hardwarePortId;
            initParams.portType = (e_FmPortType)ipcInitParams.enumPortType;
            initParams.independentMode = (bool)(ipcInitParams.boolIndependentMode);
            initParams.liodnOffset = ipcInitParams.liodnOffset;
            initParams.numOfTasks = ipcInitParams.numOfTasks;
            initParams.numOfExtraTasks = ipcInitParams.numOfExtraTasks;
            initParams.numOfOpenDmas = ipcInitParams.numOfOpenDmas;
            initParams.numOfExtraOpenDmas = ipcInitParams.numOfExtraOpenDmas;
            initParams.sizeOfFifo = ipcInitParams.sizeOfFifo;
            initParams.extraSizeOfFifo = ipcInitParams.extraSizeOfFifo;
            initParams.deqPipelineDepth = ipcInitParams.deqPipelineDepth;
            initParams.liodnBase = ipcInitParams.liodnBase;

            p_IpcReply->error = (uint32_t)FmGetSetPortParams(h_Fm, &initParams);
            ipcPhysAddr.high = initParams.fmMuramPhysBaseAddr.high;
            ipcPhysAddr.low = initParams.fmMuramPhysBaseAddr.low;
            memcpy(p_IpcReply->replyBody, (uint8_t*)&ipcPhysAddr, sizeof(t_FmIpcPhysAddr));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(t_FmIpcPhysAddr);
            break;
        }
        case (FM_SET_SIZE_OF_FIFO):
        {
            t_FmIpcPortFifoParams               ipcPortFifoParams;
            t_FmInterModulePortRxPoolsParams    rxPoolsParams;

            memcpy((uint8_t*)&ipcPortFifoParams, p_IpcMsg->msgBody, sizeof(t_FmIpcPortFifoParams));
            rxPoolsParams.numOfPools = ipcPortFifoParams.numOfPools;
            rxPoolsParams.secondLargestBufSize = ipcPortFifoParams.secondLargestBufSize;
            rxPoolsParams.largestBufSize = ipcPortFifoParams.largestBufSize;

            p_IpcReply->error = (uint32_t)FmSetSizeOfFifo(h_Fm, ipcPortFifoParams.rsrcParams.hardwarePortId,
                                                (e_FmPortType)ipcPortFifoParams.enumPortType,
                                                (bool)ipcPortFifoParams.boolIndependentMode,
                                                &ipcPortFifoParams.rsrcParams.val,
                                                ipcPortFifoParams.rsrcParams.extra,
                                                ipcPortFifoParams.deqPipelineDepth,
                                                &rxPoolsParams,
                                                (bool)ipcPortFifoParams.boolInitialConfig);
            memcpy(p_IpcReply->replyBody, (uint8_t*)&ipcPortFifoParams.rsrcParams.val, sizeof(uint32_t));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint32_t);
            break;
        }
        case (FM_SET_NUM_OF_TASKS):
        {
            t_FmIpcPortRsrcParams   ipcPortRsrcParams;

            memcpy((uint8_t*)&ipcPortRsrcParams, p_IpcMsg->msgBody, sizeof(t_FmIpcPortRsrcParams));
            p_IpcReply->error = (uint32_t)FmSetNumOfTasks(h_Fm, ipcPortRsrcParams.hardwarePortId,
                                                          (uint8_t)ipcPortRsrcParams.val,
                                                          (uint8_t)ipcPortRsrcParams.extra,
                                                          (bool)ipcPortRsrcParams.boolInitialConfig);
            *p_ReplyLength = sizeof(uint32_t);
            break;
        }
        case (FM_SET_NUM_OF_OPEN_DMAS):
        {
            t_FmIpcPortRsrcParams   ipcPortRsrcParams;

            memcpy((uint8_t*)&ipcPortRsrcParams, p_IpcMsg->msgBody, sizeof(t_FmIpcPortRsrcParams));
            p_IpcReply->error = (uint32_t)FmSetNumOfOpenDmas(h_Fm, ipcPortRsrcParams.hardwarePortId,
                                                               (uint8_t)ipcPortRsrcParams.val,
                                                               (uint8_t)ipcPortRsrcParams.extra,
                                                               (bool)ipcPortRsrcParams.boolInitialConfig);
            *p_ReplyLength = sizeof(uint32_t);
            break;
        }
        case (FM_RESUME_STALLED_PORT):
            *p_ReplyLength = sizeof(uint32_t);
            p_IpcReply->error = (uint32_t)FmResumeStalledPort(h_Fm, p_IpcMsg->msgBody[0]);
            break;
        case (FM_MASTER_IS_ALIVE):
        {
            uint8_t guestId = p_IpcMsg->msgBody[0];
            /* build the FM master partition IPC address */
            memset(p_Fm->fmIpcHandlerModuleName[guestId], 0, (sizeof(char)) * MODULE_NAME_SIZE);
            if(Sprint (p_Fm->fmIpcHandlerModuleName[guestId], "FM_%d_%d",p_Fm->p_FmStateStruct->fmId, guestId) != (guestId<10 ? 6:7))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Sprint failed"));
            p_Fm->h_IpcSessions[guestId] = XX_IpcInitSession(p_Fm->fmIpcHandlerModuleName[guestId], p_Fm->fmModuleName);
            if (p_Fm->h_IpcSessions[guestId] == NULL)
                RETURN_ERROR(MAJOR, E_NOT_AVAILABLE, ("FM Master IPC session for guest %d", guestId));
            *(uint8_t*)(p_IpcReply->replyBody) = 1;
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint8_t);
            break;
        }
        case (FM_IS_PORT_STALLED):
        {
            bool tmp;

            p_IpcReply->error = (uint32_t)FmIsPortStalled(h_Fm, p_IpcMsg->msgBody[0], &tmp);
            *(uint8_t*)(p_IpcReply->replyBody) = (uint8_t)tmp;
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint8_t);
            break;
        }
        case (FM_RESET_MAC):
        {
            t_FmIpcMacParams    ipcMacParams;

            memcpy((uint8_t*)&ipcMacParams, p_IpcMsg->msgBody, sizeof(t_FmIpcMacParams));
            p_IpcReply->error = (uint32_t)FmResetMac(p_Fm,
                                                     (e_FmMacType)(ipcMacParams.enumType),
                                                     ipcMacParams.id);
            *p_ReplyLength = sizeof(uint32_t);
            break;
        }
        case (FM_SET_MAC_MAX_FRAME):
        {
            t_Error                     err;
            t_FmIpcMacMaxFrameParams    ipcMacMaxFrameParams;

            memcpy((uint8_t*)&ipcMacMaxFrameParams, p_IpcMsg->msgBody, sizeof(t_FmIpcMacMaxFrameParams));
            if ((err = FmSetMacMaxFrame(p_Fm,
                                        (e_FmMacType)(ipcMacMaxFrameParams.macParams.enumType),
                                        ipcMacMaxFrameParams.macParams.id,
                                        ipcMacMaxFrameParams.maxFrameLength)) != E_OK)
                REPORT_ERROR(MINOR, err, NO_MSG);
            break;
        }
        case (FM_GET_CLK_FREQ):
            memcpy(p_IpcReply->replyBody, (uint8_t*)&p_Fm->p_FmStateStruct->fmClkFreq, sizeof(uint16_t));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint16_t);
            break;
        case (FM_FREE_PORT):
        {
            t_FmInterModulePortFreeParams   portParams;
            t_FmIpcPortFreeParams           ipcPortParams;

            memcpy((uint8_t*)&ipcPortParams, p_IpcMsg->msgBody, sizeof(t_FmIpcPortFreeParams));
            portParams.hardwarePortId = ipcPortParams.hardwarePortId;
            portParams.portType = (e_FmPortType)(ipcPortParams.enumPortType);
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
            portParams.deqPipelineDepth = ipcPortParams.deqPipelineDepth;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */
            FmFreePortParams(h_Fm, &portParams);
            break;
        }
        case (FM_REGISTER_INTR):
        {
            t_FmIpcRegisterIntr ipcRegIntr;

            memcpy((uint8_t*)&ipcRegIntr, p_IpcMsg->msgBody, sizeof(ipcRegIntr));
            p_Fm->intrMng[ipcRegIntr.event].guestId = ipcRegIntr.guestId;
            break;
        }
#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
        case (FM_DUMP_REGS):
        {
            t_Error     err;
            if ((err = FM_DumpRegs(h_Fm)) != E_OK)
                REPORT_ERROR(MINOR, err, NO_MSG);
            break;
        }
        case (FM_DUMP_PORT_REGS):
        {
            t_Error     err;

            if ((err = FmDumpPortRegs(h_Fm, p_IpcMsg->msgBody[0])) != E_OK)
                REPORT_ERROR(MINOR, err, NO_MSG);
            break;
        }
#endif /* (defined(DEBUG_ERRORS) && ... */
        case (FM_GET_REV):
        {
            t_FmRevisionInfo    revInfo;
            t_FmIpcRevisionInfo ipcRevInfo;

            p_IpcReply->error = (uint32_t)FM_GetRevision(h_Fm, &revInfo);
            ipcRevInfo.majorRev = revInfo.majorRev;
            ipcRevInfo.minorRev = revInfo.minorRev;
            memcpy(p_IpcReply->replyBody, (uint8_t*)&ipcRevInfo, sizeof(t_FmIpcRevisionInfo));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(t_FmIpcRevisionInfo);
            break;
        }
        case (FM_DMA_STAT):
        {
            t_FmDmaStatus       dmaStatus;
            t_FmIpcDmaStatus    ipcDmaStatus;

            FM_GetDmaStatus(h_Fm, &dmaStatus);
            ipcDmaStatus.boolCmqNotEmpty = (uint8_t)dmaStatus.cmqNotEmpty;
            ipcDmaStatus.boolBusError = (uint8_t)dmaStatus.busError;
            ipcDmaStatus.boolReadBufEccError = (uint8_t)dmaStatus.readBufEccError;
            ipcDmaStatus.boolWriteBufEccSysError = (uint8_t)dmaStatus.writeBufEccSysError;
            ipcDmaStatus.boolWriteBufEccFmError = (uint8_t)dmaStatus.writeBufEccFmError;
            memcpy(p_IpcReply->replyBody, (uint8_t*)&ipcDmaStatus, sizeof(t_FmIpcDmaStatus));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(t_FmIpcDmaStatus);
            break;
        }
        case (FM_ALLOC_FMAN_CTRL_EVENT_REG):
            p_IpcReply->error = (uint32_t)FmAllocFmanCtrlEventReg(h_Fm, (uint8_t*)p_IpcReply->replyBody);
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint8_t);
            break;
        case (FM_FREE_FMAN_CTRL_EVENT_REG):
            FmFreeFmanCtrlEventReg(h_Fm, p_IpcMsg->msgBody[0]);
            break;
        case (FM_GET_TIMESTAMP_SCALE):
        {
            uint32_t    timeStamp = FmGetTimeStampScale(h_Fm);

            memcpy(p_IpcReply->replyBody, (uint8_t*)&timeStamp, sizeof(uint32_t));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint32_t);
            break;
        }
        case (FM_GET_COUNTER):
        {
            e_FmCounters    inCounter;
            uint32_t        outCounter;

            memcpy((uint8_t*)&inCounter, p_IpcMsg->msgBody, sizeof(uint32_t));
            outCounter = FM_GetCounter(h_Fm, inCounter);
            memcpy(p_IpcReply->replyBody, (uint8_t*)&outCounter, sizeof(uint32_t));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint32_t);
            break;
        }
        case (FM_SET_FMAN_CTRL_EVENTS_ENABLE):
        {
            t_FmIpcFmanEvents ipcFmanEvents;

            memcpy((uint8_t*)&ipcFmanEvents, p_IpcMsg->msgBody, sizeof(t_FmIpcFmanEvents));
            FmSetFmanCtrlIntr(h_Fm,
                              ipcFmanEvents.eventRegId,
                              ipcFmanEvents.enableEvents);
            break;
        }
        case (FM_GET_FMAN_CTRL_EVENTS_ENABLE):
        {
            uint32_t    tmp = FmGetFmanCtrlIntr(h_Fm, p_IpcMsg->msgBody[0]);

            memcpy(p_IpcReply->replyBody, (uint8_t*)&tmp, sizeof(uint32_t));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(uint32_t);
            break;
        }
        case (FM_GET_PHYS_MURAM_BASE):
        {
            t_FmPhysAddr        physAddr;
            t_FmIpcPhysAddr     ipcPhysAddr;

            FmGetPhysicalMuramBase(h_Fm, &physAddr);
            ipcPhysAddr.high    = physAddr.high;
            ipcPhysAddr.low     = physAddr.low;
            memcpy(p_IpcReply->replyBody, (uint8_t*)&ipcPhysAddr, sizeof(t_FmIpcPhysAddr));
            *p_ReplyLength = sizeof(uint32_t) + sizeof(t_FmIpcPhysAddr);
            break;
        }
        case (FM_ENABLE_RAM_ECC):
        {
            t_Error     err;

            if (((err = FM_EnableRamsEcc(h_Fm)) != E_OK) ||
                ((err = FM_SetException(h_Fm, e_FM_EX_IRAM_ECC, TRUE)) != E_OK) ||
                ((err = FM_SetException(h_Fm, e_FM_EX_MURAM_ECC, TRUE)) != E_OK))
                REPORT_ERROR(MINOR, err, NO_MSG);
            break;
        }
        case (FM_DISABLE_RAM_ECC):
        {
            t_Error     err;

            if (((err = FM_SetException(h_Fm, e_FM_EX_IRAM_ECC, FALSE)) != E_OK) ||
                ((err = FM_SetException(h_Fm, e_FM_EX_MURAM_ECC, FALSE)) != E_OK) ||
                ((err = FM_DisableRamsEcc(h_Fm)) != E_OK))
                REPORT_ERROR(MINOR, err, NO_MSG);
            break;
        }
        case (FM_SET_NUM_OF_FMAN_CTRL):
        {
            t_Error                     err;
            t_FmIpcPortNumOfFmanCtrls   ipcPortNumOfFmanCtrls;

            memcpy((uint8_t*)&ipcPortNumOfFmanCtrls, p_IpcMsg->msgBody, sizeof(t_FmIpcPortNumOfFmanCtrls));
            if ((err = FmSetNumOfRiscsPerPort(h_Fm,
                                              ipcPortNumOfFmanCtrls.hardwarePortId,
                                              ipcPortNumOfFmanCtrls.numOfFmanCtrls)) != E_OK)
                REPORT_ERROR(MINOR, err, NO_MSG);
            break;
        }
#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
        case (FM_10G_TX_ECC_WA):
            p_IpcReply->error = (uint32_t)Fm10GTxEccWorkaround(h_Fm, p_IpcMsg->msgBody[0]);
            *p_ReplyLength = sizeof(uint32_t);
            break;
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */
        default:
            *p_ReplyLength = 0;
            RETURN_ERROR(MINOR, E_INVALID_SELECTION, ("command not found!!!"));
    }
    return E_OK;
}

static void ErrorIsrCB(t_Handle h_Fm)
{
#define FM_M_CALL_1G_MAC_ERR_ISR(_id)   \
    {                                   \
       if (p_Fm->guestId != p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_ERR_1G_MAC0+_id)].guestId) \
            SendIpcIsr(p_Fm, (e_FmInterModuleEvent)(e_FM_EV_ERR_1G_MAC0+_id), pending);             \
       else                                                                                         \
            p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_ERR_1G_MAC0+_id)].f_Isr(p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_ERR_1G_MAC0+_id)].h_SrcHandle);\
    }
    t_Fm                    *p_Fm = (t_Fm*)h_Fm;
    uint32_t                pending;

    SANITY_CHECK_RETURN(h_Fm, E_INVALID_HANDLE);

    /* error interrupts */
    pending = GET_UINT32(p_Fm->p_FmFpmRegs->fmepi);
    if (!pending)
        return;

    if(pending & ERR_INTR_EN_BMI)
        BmiErrEvent(p_Fm);
    if(pending & ERR_INTR_EN_QMI)
        QmiErrEvent(p_Fm);
    if(pending & ERR_INTR_EN_FPM)
        FpmErrEvent(p_Fm);
    if(pending & ERR_INTR_EN_DMA)
        DmaErrEvent(p_Fm);
    if(pending & ERR_INTR_EN_IRAM)
        IramErrIntr(p_Fm);
    if(pending & ERR_INTR_EN_MURAM)
        MuramErrIntr(p_Fm);
    if(pending & ERR_INTR_EN_PRS)
        p_Fm->intrMng[e_FM_EV_ERR_PRS].f_Isr(p_Fm->intrMng[e_FM_EV_ERR_PRS].h_SrcHandle);
    if(pending & ERR_INTR_EN_PLCR)
        p_Fm->intrMng[e_FM_EV_ERR_PLCR].f_Isr(p_Fm->intrMng[e_FM_EV_ERR_PLCR].h_SrcHandle);
    if(pending & ERR_INTR_EN_KG)
        p_Fm->intrMng[e_FM_EV_ERR_KG].f_Isr(p_Fm->intrMng[e_FM_EV_ERR_KG].h_SrcHandle);

    /* MAC events may belong to different partitions */
    if(pending & ERR_INTR_EN_1G_MAC0)
        FM_M_CALL_1G_MAC_ERR_ISR(0);
    if(pending & ERR_INTR_EN_1G_MAC1)
        FM_M_CALL_1G_MAC_ERR_ISR(1);
    if(pending & ERR_INTR_EN_1G_MAC2)
        FM_M_CALL_1G_MAC_ERR_ISR(2);
    if(pending & ERR_INTR_EN_1G_MAC3)
        FM_M_CALL_1G_MAC_ERR_ISR(3);
    if(pending & ERR_INTR_EN_1G_MAC4)
        FM_M_CALL_1G_MAC_ERR_ISR(4);
    if(pending & ERR_INTR_EN_10G_MAC0)
    {
       if (p_Fm->guestId != p_Fm->intrMng[e_FM_EV_ERR_10G_MAC0].guestId)
            SendIpcIsr(p_Fm, e_FM_EV_ERR_10G_MAC0, pending);
        else
            p_Fm->intrMng[e_FM_EV_ERR_10G_MAC0].f_Isr(p_Fm->intrMng[e_FM_EV_ERR_10G_MAC0].h_SrcHandle);
    }
}


#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
t_Error Fm10GTxEccWorkaround(t_Handle h_Fm, uint8_t macId)
{
    t_Fm            *p_Fm = (t_Fm*)h_Fm;
    int             timeout = 1000;
    t_Error         err = E_OK;
    t_FmIpcMsg      msg;
    t_FmIpcReply    reply;
    uint32_t        replyLength;
    uint8_t         rxHardwarePortId, txHardwarePortId;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_10G_TX_ECC_WA;
        msg.msgBody[0] = macId;
        replyLength = sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+sizeof(macId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != sizeof(uint32_t))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        return (t_Error)(reply.error);
    }

    SANITY_CHECK_RETURN_ERROR((macId == 0), E_NOT_SUPPORTED);
    SANITY_CHECK_RETURN_ERROR(IsFmanCtrlCodeLoaded(p_Fm), E_INVALID_STATE);

    SW_PORT_ID_TO_HW_PORT_ID(rxHardwarePortId, e_FM_PORT_TYPE_RX_10G, macId);
    SW_PORT_ID_TO_HW_PORT_ID(txHardwarePortId, e_FM_PORT_TYPE_TX_10G, macId);
    if ((p_Fm->p_FmStateStruct->portsTypes[rxHardwarePortId] != e_FM_PORT_TYPE_DUMMY) ||
        (p_Fm->p_FmStateStruct->portsTypes[txHardwarePortId] != e_FM_PORT_TYPE_DUMMY))
        RETURN_ERROR(MAJOR, E_INVALID_STATE,
                     ("MAC should be initialized prior to rx and tx ports!"));
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmextc, 0x40000000);
    CORE_MemoryBarrier();
    while ((GET_UINT32(p_Fm->p_FmFpmRegs->fpmextc) & 0x40000000) &&
           --timeout) ;
    if (!timeout)
        return ERROR_CODE(E_TIMEOUT);
    return E_OK;
}
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */

uintptr_t FmGetPcdPrsBaseAddr(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_VALUE(p_Fm, E_INVALID_HANDLE, 0);

    if(p_Fm->guestId != NCSW_MASTER_ID)
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Guset"));

    return (p_Fm->baseAddr + FM_MM_PRS);
}

uintptr_t FmGetPcdKgBaseAddr(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_VALUE(p_Fm, E_INVALID_HANDLE, 0);

    if(p_Fm->guestId != NCSW_MASTER_ID)
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Guset"));

    return (p_Fm->baseAddr + FM_MM_KG);
}

uintptr_t FmGetPcdPlcrBaseAddr(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_VALUE(p_Fm, E_INVALID_HANDLE, 0);

    if(p_Fm->guestId != NCSW_MASTER_ID)
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Guset"));

    return (p_Fm->baseAddr + FM_MM_PLCR);
}

t_Handle FmGetMuramHandle(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_VALUE(p_Fm, E_INVALID_HANDLE, NULL);

    return (p_Fm->h_FmMuram);
}

void FmGetPhysicalMuramBase(t_Handle h_Fm, t_FmPhysAddr *p_FmPhysAddr)
{
    t_Fm            *p_Fm = (t_Fm*)h_Fm;
    t_Error         err;
    t_FmIpcMsg      msg;
    t_FmIpcReply    reply;
    uint32_t        replyLength;
    t_FmIpcPhysAddr ipcPhysAddr;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_GET_PHYS_MURAM_BASE;
        replyLength = sizeof(uint32_t) + sizeof(t_FmPhysAddr);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
        {
            REPORT_ERROR(MINOR, err, NO_MSG);
            return;
        }
        if (replyLength != (sizeof(uint32_t) + sizeof(t_FmPhysAddr)))
        {
            REPORT_ERROR(MINOR, E_INVALID_VALUE,("IPC reply length mismatch"));
            return;
        }
        memcpy((uint8_t*)&ipcPhysAddr, reply.replyBody, sizeof(t_FmIpcPhysAddr));
        p_FmPhysAddr->high = ipcPhysAddr.high;
        p_FmPhysAddr->low  = ipcPhysAddr.low;
        return ;
    }

    /* General FM driver initialization */
    p_FmPhysAddr->low = (uint32_t)p_Fm->fmMuramPhysBaseAddr;
    p_FmPhysAddr->high = (uint8_t)((p_Fm->fmMuramPhysBaseAddr & 0x000000ff00000000LL) >> 32);
}

t_Error FmAllocFmanCtrlEventReg(t_Handle h_Fm, uint8_t *p_EventId)
{
    t_Fm            *p_Fm = (t_Fm*)h_Fm;
    uint8_t         i;
    t_Error         err;
    t_FmIpcMsg      msg;
    t_FmIpcReply    reply;
    uint32_t        replyLength;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_ALLOC_FMAN_CTRL_EVENT_REG;
        replyLength = sizeof(uint32_t) + sizeof(uint8_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MAJOR, err, NO_MSG);
        if (replyLength != (sizeof(uint32_t) + sizeof(uint8_t)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));

        *p_EventId = *(uint8_t*)(reply.replyBody);

        return (t_Error)(reply.error);
    }

    for(i=0;i<FM_NUM_OF_FMAN_CTRL_EVENT_REGS;i++)
        if (!p_Fm->usedEventRegs[i])
        {
            p_Fm->usedEventRegs[i] = TRUE;
            *p_EventId = i;
            break;
        }

    if (i==FM_NUM_OF_FMAN_CTRL_EVENT_REGS)
        RETURN_ERROR(MAJOR, E_BUSY, ("No resource - Fman controller event register."));

    return E_OK;
}

void FmFreeFmanCtrlEventReg(t_Handle h_Fm, uint8_t eventId)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;
    t_Error     err;
    t_FmIpcMsg  msg;

    if(((t_Fm *)h_Fm)->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_FREE_FMAN_CTRL_EVENT_REG;
        msg.msgBody[0] = eventId;
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+sizeof(eventId),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            REPORT_ERROR(MINOR, err, NO_MSG);
        return;
    }

    ((t_Fm*)h_Fm)->usedEventRegs[eventId] = FALSE;
}

void FmRegisterIntr(t_Handle h_Fm,
                        e_FmEventModules        module,
                        uint8_t                 modId,
                        e_FmIntrType            intrType,
                        void (*f_Isr) (t_Handle h_Arg),
                        t_Handle    h_Arg)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    uint8_t             event= 0;
    t_FmIpcRegisterIntr fmIpcRegisterIntr;
    t_Error             err;
    t_FmIpcMsg          msg;

    ASSERT_COND(h_Fm);

    GET_FM_MODULE_EVENT(module, modId,intrType, event);

    /* register in local FM structure */
    ASSERT_COND(event != e_FM_EV_DUMMY_LAST);
    p_Fm->intrMng[event].f_Isr = f_Isr;
    p_Fm->intrMng[event].h_SrcHandle = h_Arg;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        if(p_Fm->h_IpcSessions[0])
        {
            /* register in Master FM structure */
            fmIpcRegisterIntr.event = event;
            fmIpcRegisterIntr.guestId = p_Fm->guestId;
            memset(&msg, 0, sizeof(msg));
            msg.msgId = FM_REGISTER_INTR;
            memcpy(msg.msgBody, &fmIpcRegisterIntr, sizeof(fmIpcRegisterIntr));
            if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                         (uint8_t*)&msg,
                                         sizeof(msg.msgId) + sizeof(fmIpcRegisterIntr),
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL)) != E_OK)
                REPORT_ERROR(MINOR, err, NO_MSG);
        }
        else
            DBG(WARNING,("'Register interrupt' - unavailable - No IPC"));
    }

}

void FmUnregisterIntr(t_Handle h_Fm,
                        e_FmEventModules        module,
                        uint8_t                 modId,
                        e_FmIntrType            intrType)
{
    t_Fm       *p_Fm = (t_Fm*)h_Fm;
    uint8_t     event= 0;

    ASSERT_COND(h_Fm);

    GET_FM_MODULE_EVENT(module, modId,intrType, event);

    ASSERT_COND(event != e_FM_EV_DUMMY_LAST);
    p_Fm->intrMng[event].f_Isr = UnimplementedIsr;
    p_Fm->intrMng[event].h_SrcHandle = NULL;
}

void FmSetFmanCtrlIntr(t_Handle h_Fm, uint8_t   eventRegId, uint32_t enableEvents)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    t_FmIpcFmanEvents   fmanCtrl;
    t_Error             err;
    t_FmIpcMsg          msg;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        fmanCtrl.eventRegId = eventRegId;
        fmanCtrl.enableEvents = enableEvents;
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_SET_FMAN_CTRL_EVENTS_ENABLE;
        memcpy(msg.msgBody, &fmanCtrl, sizeof(fmanCtrl));
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+sizeof(fmanCtrl),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            REPORT_ERROR(MINOR, err, NO_MSG);
        return;
    }

    ASSERT_COND(eventRegId < FM_NUM_OF_FMAN_CTRL_EVENT_REGS);
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fmfpfcee[eventRegId], enableEvents);
}

uint32_t FmGetFmanCtrlIntr(t_Handle h_Fm, uint8_t eventRegId)
{
    t_Fm            *p_Fm = (t_Fm*)h_Fm;
    t_Error         err;
    t_FmIpcMsg      msg;
    t_FmIpcReply    reply;
    uint32_t        replyLength, ctrlIntr;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_GET_FMAN_CTRL_EVENTS_ENABLE;
        msg.msgBody[0] = eventRegId;
        replyLength = sizeof(uint32_t) + sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+sizeof(eventRegId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
        {
            REPORT_ERROR(MINOR, err, NO_MSG);
            return 0;
        }
        if (replyLength != (sizeof(uint32_t) + sizeof(uint32_t)))
        {
            REPORT_ERROR(MINOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
            return 0;
        }
        memcpy((uint8_t*)&ctrlIntr, reply.replyBody, sizeof(uint32_t));
        return ctrlIntr;
    }

    return GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcee[eventRegId]);
}

void  FmRegisterFmanCtrlIntr(t_Handle h_Fm, uint8_t eventRegId, void (*f_Isr) (t_Handle h_Arg, uint32_t event), t_Handle    h_Arg)
{
    t_Fm       *p_Fm = (t_Fm*)h_Fm;

    ASSERT_COND(eventRegId<FM_NUM_OF_FMAN_CTRL_EVENT_REGS);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        ASSERT_COND(0);
        /* TODO */
    }

    p_Fm->fmanCtrlIntr[eventRegId].f_Isr = f_Isr;
    p_Fm->fmanCtrlIntr[eventRegId].h_SrcHandle = h_Arg;
}

void  FmUnregisterFmanCtrlIntr(t_Handle h_Fm, uint8_t eventRegId)
{
    t_Fm       *p_Fm = (t_Fm*)h_Fm;

    ASSERT_COND(eventRegId<FM_NUM_OF_FMAN_CTRL_EVENT_REGS);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        ASSERT_COND(0);
        /* TODO */
    }

    p_Fm->fmanCtrlIntr[eventRegId].f_Isr = UnimplementedFmanCtrlIsr;
    p_Fm->fmanCtrlIntr[eventRegId].h_SrcHandle = NULL;
}

void  FmRegisterPcd(t_Handle h_Fm, t_Handle h_FmPcd)
{
    t_Fm       *p_Fm = (t_Fm*)h_Fm;

    if(p_Fm->h_Pcd)
        REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("PCD already set"));

    p_Fm->h_Pcd = h_FmPcd;

}

void  FmUnregisterPcd(t_Handle h_Fm)
{
    t_Fm       *p_Fm = (t_Fm*)h_Fm;

    if(!p_Fm->h_Pcd)
        REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("No PCD"));

    p_Fm->h_Pcd = NULL;

}

t_Handle  FmGetPcdHandle(t_Handle h_Fm)
{
    t_Fm       *p_Fm = (t_Fm*)h_Fm;

    return p_Fm->h_Pcd;
}

uint8_t FmGetId(t_Handle h_Fm)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_VALUE(p_Fm, E_INVALID_HANDLE, 0xff);

    return p_Fm->p_FmStateStruct->fmId;
}

t_Error FmSetNumOfRiscsPerPort(t_Handle h_Fm, uint8_t hardwarePortId, uint8_t numOfFmanCtrls)
{

    t_Fm                        *p_Fm = (t_Fm*)h_Fm;
    uint32_t                    tmpReg = 0;
    t_Error                     err;
    t_FmIpcPortNumOfFmanCtrls   params;
    t_FmIpcMsg                  msg;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(((numOfFmanCtrls > 0) && (numOfFmanCtrls < 3)) , E_INVALID_HANDLE);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        params.hardwarePortId = hardwarePortId;
        params.numOfFmanCtrls = numOfFmanCtrls;
        msg.msgId = FM_SET_NUM_OF_FMAN_CTRL;
        memcpy(msg.msgBody, &params, sizeof(params));
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) +sizeof(params),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);

        return E_OK;
    }

    XX_LockSpinlock(p_Fm->h_Spinlock);

    tmpReg = (uint32_t)(hardwarePortId << FPM_PORT_FM_CTL_PORTID_SHIFT);

    /*TODO - maybe to put CTL# according to another criteria*/

    if(numOfFmanCtrls == 2)
        tmpReg = FPM_PORT_FM_CTL2 | FPM_PORT_FM_CTL1;

    /* order restoration */
    if(hardwarePortId%2)
        tmpReg |= (FPM_PORT_FM_CTL1 << FPM_PRC_ORA_FM_CTL_SEL_SHIFT) | FPM_PORT_FM_CTL1;
    else
        tmpReg |= (FPM_PORT_FM_CTL2 << FPM_PRC_ORA_FM_CTL_SEL_SHIFT) | FPM_PORT_FM_CTL2;

    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmpr, tmpReg);
    XX_UnlockSpinlock(p_Fm->h_Spinlock);

    return E_OK;
}

t_Error FmGetSetPortParams(t_Handle h_Fm,t_FmInterModulePortInitParams *p_PortParams)
{
    t_Fm                    *p_Fm = (t_Fm*)h_Fm;
    uint32_t                tmpReg;
    uint8_t                 hardwarePortId = p_PortParams->hardwarePortId;
    t_FmIpcPortInInitParams portInParams;
    t_FmIpcPhysAddr         ipcPhysAddr;
    t_Error                 err;
    t_FmIpcMsg              msg;
    t_FmIpcReply            reply;
    uint32_t                replyLength;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        portInParams.hardwarePortId = p_PortParams->hardwarePortId;
        portInParams.enumPortType = (uint32_t)p_PortParams->portType;
        portInParams.boolIndependentMode = (uint8_t)p_PortParams->independentMode;
        portInParams.liodnOffset = p_PortParams->liodnOffset;
        portInParams.numOfTasks = p_PortParams->numOfTasks;
        portInParams.numOfExtraTasks = p_PortParams->numOfExtraTasks;
        portInParams.numOfOpenDmas = p_PortParams->numOfOpenDmas;
        portInParams.numOfExtraOpenDmas = p_PortParams->numOfExtraOpenDmas;
        portInParams.sizeOfFifo = p_PortParams->sizeOfFifo;
        portInParams.extraSizeOfFifo = p_PortParams->extraSizeOfFifo;
        portInParams.deqPipelineDepth = p_PortParams->deqPipelineDepth;
        portInParams.liodnBase = p_PortParams->liodnBase;
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_GET_SET_PORT_PARAMS;
        memcpy(msg.msgBody, &portInParams, sizeof(portInParams));
        replyLength = (sizeof(uint32_t) + sizeof(p_PortParams->fmMuramPhysBaseAddr));
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) +sizeof(portInParams),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != (sizeof(uint32_t) + sizeof(p_PortParams->fmMuramPhysBaseAddr)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        memcpy((uint8_t*)&ipcPhysAddr, reply.replyBody, sizeof(t_FmIpcPhysAddr));
        p_PortParams->fmMuramPhysBaseAddr.high = ipcPhysAddr.high;
        p_PortParams->fmMuramPhysBaseAddr.low  = ipcPhysAddr.low;

        return (t_Error)(reply.error);
    }

    ASSERT_COND(IN_RANGE(1, hardwarePortId, 63));
    XX_LockSpinlock(p_Fm->h_Spinlock);

    if(p_PortParams->independentMode)
    {
        /* set port parameters */
        p_Fm->independentMode = p_PortParams->independentMode;
        /* disable dispatch limit */
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmflc, 0);
    }

    if(p_PortParams->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND)
    {
        if(p_Fm->hcPortInitialized)
        {
            XX_UnlockSpinlock(p_Fm->h_Spinlock);
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Only one host command port is allowed."));
        }
        else
            p_Fm->hcPortInitialized = TRUE;
    }
    p_Fm->p_FmStateStruct->portsTypes[hardwarePortId] = p_PortParams->portType;

    err = FmSetNumOfTasks(p_Fm, p_PortParams->hardwarePortId, p_PortParams->numOfTasks, p_PortParams->numOfExtraTasks, TRUE);
    if(err)
    {
        XX_UnlockSpinlock(p_Fm->h_Spinlock);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    if((p_PortParams->portType != e_FM_PORT_TYPE_RX) && (p_PortParams->portType != e_FM_PORT_TYPE_RX_10G))
    /* for transmit & O/H ports */
    {
        uint8_t     enqTh;
        uint8_t     deqTh;
        bool        update = FALSE;

        /* update qmi ENQ/DEQ threshold */
        p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums += p_PortParams->deqPipelineDepth;
        tmpReg = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_gc);
        enqTh = (uint8_t)(tmpReg>>8);
        /* if enqTh is too big, we reduce it to the max value that is still OK */
        if(enqTh >= (QMI_MAX_NUM_OF_TNUMS - p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums))
        {
            enqTh = (uint8_t)(QMI_MAX_NUM_OF_TNUMS - p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums - 1);
            tmpReg &= ~QMI_CFG_ENQ_MASK;
            tmpReg |= ((uint32_t)enqTh << 8);
            update = TRUE;
        }

        deqTh = (uint8_t)tmpReg;
        /* if deqTh is too small, we enlarge it to the min value that is still OK.
         deqTh may not be larger than 63 (QMI_MAX_NUM_OF_TNUMS-1). */
        if((deqTh <= p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums)  && (deqTh < QMI_MAX_NUM_OF_TNUMS-1))
        {
            deqTh = (uint8_t)(p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums + 1);
            tmpReg &= ~QMI_CFG_DEQ_MASK;
            tmpReg |= (uint32_t)deqTh;
            update = TRUE;
        }
        if(update)
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_gc, tmpReg);
    }
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

#ifdef FM_LOW_END_RESTRICTION
    if((hardwarePortId==0x1) || (hardwarePortId==0x29))
    {
        if(p_Fm->p_FmStateStruct->lowEndRestriction)
        {
            XX_UnlockSpinlock(p_Fm->h_Spinlock);
            RETURN_ERROR(MAJOR, E_NOT_AVAILABLE, ("OP #0 cannot work with Tx Port #1."));
        }
        else
            p_Fm->p_FmStateStruct->lowEndRestriction = TRUE;
    }
#endif /* FM_LOW_END_RESTRICTION */

    err = FmSetSizeOfFifo(p_Fm,
                            p_PortParams->hardwarePortId,
                            p_PortParams->portType,
                            p_PortParams->independentMode,
                            &p_PortParams->sizeOfFifo,
                            p_PortParams->extraSizeOfFifo,
                            p_PortParams->deqPipelineDepth,
                            NULL,
                            TRUE);
    if(err)
    {
        XX_UnlockSpinlock(p_Fm->h_Spinlock);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    err = FmSetNumOfOpenDmas(p_Fm, p_PortParams->hardwarePortId, p_PortParams->numOfOpenDmas, p_PortParams->numOfExtraOpenDmas, TRUE);
    if(err)
    {
        XX_UnlockSpinlock(p_Fm->h_Spinlock);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ppid[hardwarePortId-1], (uint32_t)p_PortParams->liodnOffset);

    tmpReg = (uint32_t)(hardwarePortId << FPM_PORT_FM_CTL_PORTID_SHIFT);
    if(p_PortParams->independentMode)
    {
        if((p_PortParams->portType==e_FM_PORT_TYPE_RX) || (p_PortParams->portType==e_FM_PORT_TYPE_RX_10G))
            tmpReg |= (FPM_PORT_FM_CTL1 << FPM_PRC_ORA_FM_CTL_SEL_SHIFT) |FPM_PORT_FM_CTL1;
        else
            tmpReg |= (FPM_PORT_FM_CTL2 << FPM_PRC_ORA_FM_CTL_SEL_SHIFT) |FPM_PORT_FM_CTL2;
    }
    else
    {
        tmpReg |= (FPM_PORT_FM_CTL2|FPM_PORT_FM_CTL1);

        /* order restoration */
        if(hardwarePortId%2)
            tmpReg |= (FPM_PORT_FM_CTL1 << FPM_PRC_ORA_FM_CTL_SEL_SHIFT);
        else
            tmpReg |= (FPM_PORT_FM_CTL2 << FPM_PRC_ORA_FM_CTL_SEL_SHIFT);
    }
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmpr, tmpReg);

    {
#ifdef FM_PARTITION_ARRAY
        t_FmRevisionInfo revInfo;

        FM_GetRevision(p_Fm, &revInfo);
        if (revInfo.majorRev >= 2)
#endif /* FM_PARTITION_ARRAY */
        {
            /* set LIODN base for this port */
            tmpReg = GET_UINT32(p_Fm->p_FmDmaRegs->fmdmplr[hardwarePortId/2]);
            if(hardwarePortId%2)
            {
                tmpReg &= ~FM_LIODN_BASE_MASK;
                tmpReg |= (uint32_t)p_PortParams->liodnBase;
            }
            else
            {
                tmpReg &= ~(FM_LIODN_BASE_MASK<< DMA_LIODN_SHIFT);
                tmpReg |= (uint32_t)p_PortParams->liodnBase << DMA_LIODN_SHIFT;
            }
            WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmplr[hardwarePortId/2], tmpReg);
        }
    }

    FmGetPhysicalMuramBase(p_Fm, &p_PortParams->fmMuramPhysBaseAddr);
    XX_UnlockSpinlock(p_Fm->h_Spinlock);

    return E_OK;
}

void FmFreePortParams(t_Handle h_Fm,t_FmInterModulePortFreeParams *p_PortParams)
{
    t_Fm                    *p_Fm = (t_Fm*)h_Fm;
    uint32_t                tmpReg;
    uint8_t                 hardwarePortId = p_PortParams->hardwarePortId;
    uint8_t                 numOfTasks;
    t_Error                 err;
    t_FmIpcPortFreeParams   portParams;
    t_FmIpcMsg              msg;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        portParams.hardwarePortId = p_PortParams->hardwarePortId;
        portParams.enumPortType = (uint32_t)p_PortParams->portType;
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
        portParams.deqPipelineDepth = p_PortParams->deqPipelineDepth;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_FREE_PORT;
        memcpy(msg.msgBody, &portParams, sizeof(portParams));
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+sizeof(portParams),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            REPORT_ERROR(MINOR, err, NO_MSG);
        return;
    }

    ASSERT_COND(IN_RANGE(1, hardwarePortId, 63));
    XX_LockSpinlock(p_Fm->h_Spinlock);


    if(p_PortParams->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND)
    {
        ASSERT_COND(p_Fm->hcPortInitialized);
        p_Fm->hcPortInitialized = FALSE;
    }

    p_Fm->p_FmStateStruct->portsTypes[hardwarePortId] = e_FM_PORT_TYPE_DUMMY;

    tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1]);
    /* free numOfTasks */
    numOfTasks = (uint8_t)(((tmpReg & BMI_NUM_OF_TASKS_MASK) >> BMI_NUM_OF_TASKS_SHIFT) + 1);
    ASSERT_COND(p_Fm->p_FmStateStruct->accumulatedNumOfTasks >= numOfTasks);
    p_Fm->p_FmStateStruct->accumulatedNumOfTasks -= numOfTasks;

    /* free numOfOpenDmas */
    ASSERT_COND(p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas >= ((tmpReg & BMI_NUM_OF_DMAS_MASK) >> BMI_NUM_OF_DMAS_SHIFT) + 1);
    p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas -= (((tmpReg & BMI_NUM_OF_DMAS_MASK) >> BMI_NUM_OF_DMAS_SHIFT) + 1);

    /* update total num of DMA's with committed number of open DMAS, and max uncommitted pool. */
    tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg2) & ~BMI_CFG2_DMAS_MASK;
    tmpReg |= (uint32_t)(p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas + p_Fm->p_FmStateStruct->extraOpenDmasPoolSize - 1) << BMI_CFG2_DMAS_SHIFT;
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg2,  tmpReg);

    /* free sizeOfFifo */
    tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_pfs[hardwarePortId-1]);
    ASSERT_COND(p_Fm->p_FmStateStruct->accumulatedFifoSize >=
                (((tmpReg & BMI_FIFO_SIZE_MASK) + 1) * BMI_FIFO_UNITS));
    p_Fm->p_FmStateStruct->accumulatedFifoSize -=
        (((tmpReg & BMI_FIFO_SIZE_MASK) + 1) * BMI_FIFO_UNITS);

    /* clear registers */
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1], 0);
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_pfs[hardwarePortId-1], 0);
    /* WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ppid[hardwarePortId-1], 0); */

#ifdef FM_PORT_DISABLED_ERRATA_FMANx9
    /* this errata means that when a port is taken down, other port may not use its
     * resources for a while as it may still be using it (in case of reject).
     */
        {
            t_FmRevisionInfo revInfo;
            FM_GetRevision(p_Fm, &revInfo);
            if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                XX_UDelay(100000);
        }
#endif /* FM_PORT_DISABLED_ERRATA_FMANx9 */

#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    if((p_PortParams->portType != e_FM_PORT_TYPE_RX) && (p_PortParams->portType != e_FM_PORT_TYPE_RX_10G))
    /* for transmit & O/H ports */
    {
        uint8_t     enqTh;
        uint8_t     deqTh;

        tmpReg = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_gc);
        /* update qmi ENQ/DEQ threshold */
        p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums -= p_PortParams->deqPipelineDepth;

        /* p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums is now smaller,
           so we can enlarge enqTh */
        enqTh = (uint8_t)(QMI_MAX_NUM_OF_TNUMS - p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums - 1);
        tmpReg &= ~QMI_CFG_ENQ_MASK;
        tmpReg |= ((uint32_t)enqTh << QMI_CFG_ENQ_SHIFT);

         /* p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums is now smaller,
           so we can reduce deqTh */
        deqTh = (uint8_t)(p_Fm->p_FmStateStruct->accumulatedNumOfDeqTnums + 1);
        tmpReg &= ~QMI_CFG_DEQ_MASK;
        tmpReg |= (uint32_t)deqTh;

        WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_gc, tmpReg);
    }
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

#ifdef FM_LOW_END_RESTRICTION
    if((hardwarePortId==0x1) || (hardwarePortId==0x29))
        p_Fm->p_FmStateStruct->lowEndRestriction = FALSE;
#endif /* FM_LOW_END_RESTRICTION */
    XX_UnlockSpinlock(p_Fm->h_Spinlock);
}

t_Error FmIsPortStalled(t_Handle h_Fm, uint8_t hardwarePortId, bool *p_IsStalled)
{
    t_Fm            *p_Fm = (t_Fm*)h_Fm;
    uint32_t        tmpReg;
    t_Error         err;
    t_FmIpcMsg      msg;
    t_FmIpcReply    reply;
    uint32_t        replyLength;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_IS_PORT_STALLED;
        msg.msgBody[0] = hardwarePortId;
        replyLength = sizeof(uint32_t) + sizeof(uint8_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+sizeof(hardwarePortId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != (sizeof(uint32_t) + sizeof(uint8_t)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));

        *p_IsStalled = (bool)!!(*(uint8_t*)(reply.replyBody));

        return (t_Error)(reply.error);
    }

    tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fmfp_ps[hardwarePortId]);
    *p_IsStalled = (bool)!!(tmpReg & FPM_PS_STALLED);

    return E_OK;
}

t_Error FmResumeStalledPort(t_Handle h_Fm, uint8_t hardwarePortId)
{
    t_Fm            *p_Fm = (t_Fm*)h_Fm;
    uint32_t        tmpReg;
    t_Error         err;
    bool            isStalled;
    t_FmIpcMsg      msg;
    t_FmIpcReply    reply;
    uint32_t        replyLength;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_RESUME_STALLED_PORT;
        msg.msgBody[0] = hardwarePortId;
        replyLength = sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) + sizeof(hardwarePortId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != sizeof(uint32_t))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        return (t_Error)(reply.error);
    }

    /* Get port status */
    err = FmIsPortStalled(h_Fm, hardwarePortId, &isStalled);
    if(err)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Can't get port status"));
    if (!isStalled)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Port is not stalled"));

    tmpReg = (uint32_t)((hardwarePortId << FPM_PORT_FM_CTL_PORTID_SHIFT) | FPM_PRC_REALSE_STALLED);
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmpr, tmpReg);

    return E_OK;
}

t_Error FmResetMac(t_Handle h_Fm, e_FmMacType type, uint8_t macId)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    uint32_t            bitMask, timeout = 1000;
    t_FmIpcMacParams    macParams;
    t_Error             err;
    t_FmIpcMsg          msg;
    t_FmIpcReply        reply;
    uint32_t            replyLength;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        if(p_Fm->h_IpcSessions[0])
        {
            memset(&msg, 0, sizeof(msg));
            memset(&reply, 0, sizeof(reply));
            macParams.id = macId;
            macParams.enumType = (uint32_t)type;
            msg.msgId = FM_RESET_MAC;
            memcpy(msg.msgBody,  &macParams, sizeof(macParams));
            replyLength = sizeof(uint32_t);
            if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                         (uint8_t*)&msg,
                                         sizeof(msg.msgId)+sizeof(macParams),
                                         (uint8_t*)&reply,
                                         &replyLength,
                                         NULL,
                                         NULL)) != E_OK)
                RETURN_ERROR(MINOR, err, NO_MSG);
            if (replyLength != sizeof(uint32_t))
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
            return (t_Error)(reply.error);
        }
        else
            if(!p_Fm->p_FmFpmRegs)
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("No IPC and no registers address"));
    }

    /* Get the relevant bit mask */
    if (type == e_FM_MAC_10G)
    {
        switch(macId)
        {
            case(0):
                bitMask = FPM_RSTC_10G0_RESET;
                break;
            default:
                RETURN_ERROR(MINOR, E_INVALID_VALUE, ("Illegal MAC Id"));
        }
    }
    else
    {
        switch(macId)
        {
            case(0):
                bitMask = FPM_RSTC_1G0_RESET;
                break;
            case(1):
                bitMask = FPM_RSTC_1G1_RESET;
                break;
            case(2):
                bitMask = FPM_RSTC_1G2_RESET;
                break;
            case(3):
                bitMask = FPM_RSTC_1G3_RESET;
                break;
            case(4):
                bitMask = FPM_RSTC_1G4_RESET;
                break;
            default:
                RETURN_ERROR(MINOR, E_INVALID_VALUE, ("Illegal MAC Id"));
        }
    }

    /* reset */
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrstc, bitMask);
    while ((GET_UINT32(p_Fm->p_FmFpmRegs->fmrstc) & bitMask) &&
           --timeout) ;
    if (!timeout)
        return ERROR_CODE(E_TIMEOUT);
    return E_OK;
}

t_Error FmSetMacMaxFrame(t_Handle h_Fm, e_FmMacType type, uint8_t macId, uint16_t mtu)
{
    t_Fm                        *p_Fm = (t_Fm*)h_Fm;
    t_FmIpcMacMaxFrameParams    macMaxFrameLengthParams;
    t_Error                     err;
    t_FmIpcMsg                  msg;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        macMaxFrameLengthParams.macParams.id = macId;
        macMaxFrameLengthParams.macParams.enumType = (uint32_t)type;
        macMaxFrameLengthParams.maxFrameLength = (uint16_t)mtu;
        msg.msgId = FM_SET_MAC_MAX_FRAME;
        memcpy(msg.msgBody,  &macMaxFrameLengthParams, sizeof(macMaxFrameLengthParams));
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId)+sizeof(macMaxFrameLengthParams),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        return E_OK;
    }

#if (defined(FM_MAX_NUM_OF_10G_MACS) && (FM_MAX_NUM_OF_10G_MACS))
    if (type == e_FM_MAC_10G)
        p_Fm->p_FmStateStruct->macMaxFrameLengths10G[macId] = mtu;
    else
#else
    UNUSED(type);
#endif /* (defined(FM_MAX_NUM_OF_10G_MACS) && ... */
        p_Fm->p_FmStateStruct->macMaxFrameLengths1G[macId] = mtu;

    return E_OK;
}

uint16_t FmGetClockFreq(t_Handle h_Fm)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;
    /* for MC environment: this depends on the
     * fact that fmClkFreq was properly initialized at "init". */
    return p_Fm->p_FmStateStruct->fmClkFreq;
}

uint32_t FmGetTimeStampScale(t_Handle h_Fm)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    t_Error             err;
    t_FmIpcMsg          msg;
    t_FmIpcReply        reply;
    uint32_t            replyLength, timeStamp;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_GET_TIMESTAMP_SCALE;
        replyLength = sizeof(uint32_t) + sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if(replyLength != (sizeof(uint32_t) + sizeof(uint32_t)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));

        memcpy((uint8_t*)&timeStamp, reply.replyBody, sizeof(uint32_t));
        return timeStamp;
    }

    if(!p_Fm->p_FmStateStruct->enabledTimeStamp)
        FmEnableTimeStamp(p_Fm);

    return p_Fm->p_FmStateStruct->count1MicroBit;
}

bool FmRamsEccIsExternalCtl(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;
    uint32_t    tmpReg;

    tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fmrcr);
    if(tmpReg & FPM_RAM_CTL_RAMS_ECC_EN_SRC_SEL)
        return TRUE;
    else
        return FALSE;
}

t_Error FmEnableRamsEcc(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    p_Fm->p_FmStateStruct->ramsEccOwners++;
    p_Fm->p_FmStateStruct->internalCall = TRUE;

    return FM_EnableRamsEcc(p_Fm);
}

t_Error FmDisableRamsEcc(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    ASSERT_COND(p_Fm->p_FmStateStruct->ramsEccOwners);
    p_Fm->p_FmStateStruct->ramsEccOwners--;

    if(p_Fm->p_FmStateStruct->ramsEccOwners==0)
    {
        p_Fm->p_FmStateStruct->internalCall = TRUE;
        return FM_DisableRamsEcc(p_Fm);
    }
    return E_OK;
}

uint8_t FmGetGuestId(t_Handle h_Fm)
{
    t_Fm     *p_Fm = (t_Fm*)h_Fm;

    return p_Fm->guestId;
}

bool FmIsMaster(t_Handle h_Fm)
{
    t_Fm     *p_Fm = (t_Fm*)h_Fm;

    return (p_Fm->guestId == NCSW_MASTER_ID);
}

t_Error FmSetSizeOfFifo(t_Handle                            h_Fm,
                        uint8_t                             hardwarePortId,
                        e_FmPortType                        portType,
                        bool                                independentMode,
                        uint32_t                            *p_SizeOfFifo,
                        uint32_t                            extraSizeOfFifo,
                        uint8_t                             deqPipelineDepth,
                        t_FmInterModulePortRxPoolsParams    *p_RxPoolsParams,
                        bool                                initialConfig)
{
    t_Fm                    *p_Fm = (t_Fm*)h_Fm;
    uint8_t                 relativePortId;
    uint16_t                macMaxFrameLength = 0, oldVal;
    uint32_t                minFifoSizeRequired = 0, sizeOfFifo, tmpReg = 0;
    t_FmIpcPortFifoParams   fifoParams;
    t_Error                 err;

    ASSERT_COND(IN_RANGE(1, hardwarePortId, 63));
    ASSERT_COND(initialConfig || p_RxPoolsParams);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        t_FmIpcMsg          msg;
        t_FmIpcReply        reply;
        uint32_t            replyLength;

        ASSERT_COND(p_RxPoolsParams);

        memset(&fifoParams, 0, sizeof(fifoParams));
        fifoParams.rsrcParams.hardwarePortId = hardwarePortId;
        fifoParams.rsrcParams.val = *p_SizeOfFifo;
        fifoParams.rsrcParams.extra = extraSizeOfFifo;
        fifoParams.enumPortType = (uint32_t)portType;
        fifoParams.boolIndependentMode = (uint8_t)independentMode;
        fifoParams.deqPipelineDepth = deqPipelineDepth;
        fifoParams.numOfPools = p_RxPoolsParams->numOfPools;
        fifoParams.secondLargestBufSize = p_RxPoolsParams->secondLargestBufSize;
        fifoParams.largestBufSize = p_RxPoolsParams->largestBufSize;
        fifoParams.boolInitialConfig = (uint8_t)initialConfig;

        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_SET_SIZE_OF_FIFO;
        memcpy(msg.msgBody, &fifoParams, sizeof(fifoParams));
        replyLength = sizeof(uint32_t) + sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) + sizeof(fifoParams),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != (sizeof(uint32_t) + sizeof(uint32_t)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        memcpy((uint8_t*)p_SizeOfFifo, reply.replyBody, sizeof(uint32_t));

        return (t_Error)(reply.error);
    }
    sizeOfFifo = *p_SizeOfFifo;
    /* if neseccary (cases where frame length is relevant), update sizeOfFifo field. */
    if((portType == e_FM_PORT_TYPE_TX) || ((portType == e_FM_PORT_TYPE_RX) && independentMode))
    {
        HW_PORT_ID_TO_SW_PORT_ID(relativePortId, hardwarePortId);
        ASSERT_COND(relativePortId < FM_MAX_NUM_OF_1G_MACS);
        macMaxFrameLength = p_Fm->p_FmStateStruct->macMaxFrameLengths1G[relativePortId];
    }

#if (defined(FM_MAX_NUM_OF_10G_MACS) && (FM_MAX_NUM_OF_10G_MACS))
    if((portType == e_FM_PORT_TYPE_TX_10G) || ((portType == e_FM_PORT_TYPE_RX_10G)  && independentMode))
    {
        HW_PORT_ID_TO_SW_PORT_ID(relativePortId, hardwarePortId);
        ASSERT_COND(relativePortId < FM_MAX_NUM_OF_10G_MACS);
        macMaxFrameLength = p_Fm->p_FmStateStruct->macMaxFrameLengths10G[relativePortId];
    }
#endif /* (defined(FM_MAX_NUM_OF_10G_MACS) && ... */

    /*************************/
    /*    TX PORTS           */
    /*************************/
    if((portType == e_FM_PORT_TYPE_TX) || (portType == e_FM_PORT_TYPE_TX_10G))
    {
        if(independentMode)
            minFifoSizeRequired = (uint32_t)((macMaxFrameLength % BMI_FIFO_UNITS ?
                                (macMaxFrameLength/BMI_FIFO_UNITS + 1) * BMI_FIFO_UNITS :
                                macMaxFrameLength) +
                                (3*BMI_FIFO_UNITS));
        else
            minFifoSizeRequired = (uint32_t)((macMaxFrameLength % BMI_FIFO_UNITS ?
                                   (macMaxFrameLength/BMI_FIFO_UNITS + 1) * BMI_FIFO_UNITS :
                                   macMaxFrameLength) +
                                   (deqPipelineDepth+3)*BMI_FIFO_UNITS);
    }
    /*************************/
    /*    RX IM PORTS        */
    /*************************/
    else if(((portType == e_FM_PORT_TYPE_RX) || (portType == e_FM_PORT_TYPE_RX_10G)) && independentMode)
        minFifoSizeRequired = (uint32_t)(((macMaxFrameLength % BMI_FIFO_UNITS) ?
                                         ((macMaxFrameLength/BMI_FIFO_UNITS + 1) * BMI_FIFO_UNITS) :
                                         macMaxFrameLength) +
                                         (4*BMI_FIFO_UNITS));

    /* for Rx (non-Im) ports or OP, buffer pools are relevant for fifo size.
       If this routine is called as part of the "GetSet" routine, initialConfig is TRUE
       and these checks where done in the port routine.
       If it is called by an explicit user request ("SetSizeOfFifo"), than these parameters
       should be checked/updated */
    if(!initialConfig &&
      ((portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) ||
      (((portType == e_FM_PORT_TYPE_RX) || (portType == e_FM_PORT_TYPE_RX_10G)) && !independentMode)))
    {
        if((portType == e_FM_PORT_TYPE_RX) || (portType == e_FM_PORT_TYPE_RX_10G))
        {
            /*************************/
            /*    RX non-IM PORTS    */
            /*************************/
#ifdef FM_FIFO_ALLOCATION_OLD_ALG
            t_FmRevisionInfo revInfo;

            FM_GetRevision(p_Fm, &revInfo);
            if(revInfo.majorRev != 4)
                minFifoSizeRequired = (uint32_t)(((p_RxPoolsParams->largestBufSize % BMI_FIFO_UNITS) ?
                                        ((p_RxPoolsParams->largestBufSize/BMI_FIFO_UNITS + 1) * BMI_FIFO_UNITS) :
                                        p_RxPoolsParams->largestBufSize) +
                                        (7*BMI_FIFO_UNITS));
            else
#endif /* FM_FIFO_ALLOCATION_OLD_ALG */
            {
                if(p_RxPoolsParams->numOfPools == 1)
                    minFifoSizeRequired = 8*BMI_FIFO_UNITS;
                else
                {
                    minFifoSizeRequired = (uint32_t)(((p_RxPoolsParams->secondLargestBufSize % BMI_FIFO_UNITS) ?
                                        ((p_RxPoolsParams->secondLargestBufSize/BMI_FIFO_UNITS + 1) * BMI_FIFO_UNITS) :
                                        p_RxPoolsParams->secondLargestBufSize) +
                                        (7*BMI_FIFO_UNITS));
                    if((sizeOfFifo < minFifoSizeRequired))
                    {
                        DBG(WARNING, ("User set FIFO size for Rx port is not optimized. (not modified by driver)"));
                        minFifoSizeRequired = 8*BMI_FIFO_UNITS;
                    }
                }
            }
        }
        else
        {
            /*************************/
            /*    OP PORTS           */
            /*************************/
            /* check if pool size is not too big */
            if(p_RxPoolsParams->largestBufSize > sizeOfFifo )
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Largest pool size is bigger than ports committed fifo size"));
        }
    }


    if (minFifoSizeRequired && (sizeOfFifo < minFifoSizeRequired))
    {
        sizeOfFifo = minFifoSizeRequired;
        DBG(WARNING, ("FIFO size enlarged to %d for port %#x", minFifoSizeRequired, hardwarePortId));
    }

    if(initialConfig)
        oldVal = 0;
    else
    {
        tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_pfs[hardwarePortId-1]);
        /* read into oldVal the current extra fifo size */
        oldVal = (uint16_t)((((tmpReg & BMI_EXTRA_FIFO_SIZE_MASK) + 1)*BMI_FIFO_UNITS) >> BMI_EXTRA_FIFO_SIZE_SHIFT);
    }

    if(extraSizeOfFifo > oldVal)
        p_Fm->p_FmStateStruct->extraFifoPoolSize = NCSW_MAX(p_Fm->p_FmStateStruct->extraFifoPoolSize, extraSizeOfFifo);

    if(!initialConfig)
        /* read into oldVal the current num of tasks */
        oldVal = (uint16_t)(((tmpReg & BMI_FIFO_SIZE_MASK) + 1)*BMI_FIFO_UNITS);

    /* check that there are enough uncommitted fifo size */
    if((p_Fm->p_FmStateStruct->accumulatedFifoSize - oldVal + sizeOfFifo) >
       (p_Fm->p_FmStateStruct->totalFifoSize - p_Fm->p_FmStateStruct->extraFifoPoolSize))
        RETURN_ERROR(MAJOR, E_NOT_AVAILABLE, ("Requested fifo size and extra size exceed total FIFO size."));
    else
    {
        /* update acummulated */
        ASSERT_COND(p_Fm->p_FmStateStruct->accumulatedFifoSize >= oldVal);
        p_Fm->p_FmStateStruct->accumulatedFifoSize -= oldVal;
        p_Fm->p_FmStateStruct->accumulatedFifoSize += sizeOfFifo;
        /* calculate reg */
        tmpReg = (uint32_t)((sizeOfFifo/BMI_FIFO_UNITS - 1) |
                            ((extraSizeOfFifo/BMI_FIFO_UNITS) << BMI_EXTRA_FIFO_SIZE_SHIFT));
        WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_pfs[hardwarePortId-1], tmpReg);
    }
    *p_SizeOfFifo = sizeOfFifo;

    return E_OK;
}

t_Error FmSetNumOfTasks(t_Handle    h_Fm,
                        uint8_t     hardwarePortId,
                        uint8_t     numOfTasks,
                        uint8_t     numOfExtraTasks,
                        bool        initialConfig)
{
    t_Fm                    *p_Fm = (t_Fm *)h_Fm;
    uint8_t                 oldVal;
    uint32_t                tmpReg = 0;
    t_FmIpcPortRsrcParams   rsrcParams;
    t_Error                 err;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        t_FmIpcMsg          msg;
        t_FmIpcReply        reply;
        uint32_t            replyLength;

        rsrcParams.hardwarePortId = hardwarePortId;
        rsrcParams.val = numOfTasks;
        rsrcParams.extra = numOfExtraTasks;
        rsrcParams.boolInitialConfig = (uint8_t)initialConfig;

        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_SET_NUM_OF_TASKS;
        memcpy(msg.msgBody, &rsrcParams, sizeof(rsrcParams));
        replyLength = sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) + sizeof(rsrcParams),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != sizeof(uint32_t))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        return (t_Error)(reply.error);
    }

    ASSERT_COND(IN_RANGE(1, hardwarePortId, 63));

    if(initialConfig)
        oldVal = 0;
    else
    {
        tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1]);
        /* read into oldVal the current extra tasks */
        oldVal = (uint8_t)((tmpReg & BMI_NUM_OF_EXTRA_TASKS_MASK) >> BMI_EXTRA_NUM_OF_TASKS_SHIFT);
    }

    if(numOfExtraTasks > oldVal)
        p_Fm->p_FmStateStruct->extraTasksPoolSize = (uint8_t)NCSW_MAX(p_Fm->p_FmStateStruct->extraTasksPoolSize, numOfExtraTasks);

    if(!initialConfig)
        /* read into oldVal the current num of tasks */
        oldVal = (uint8_t)(((tmpReg & BMI_NUM_OF_TASKS_MASK) >> BMI_NUM_OF_TASKS_SHIFT) + 1);

    /* check that there are enough uncommitted tasks */
    if((p_Fm->p_FmStateStruct->accumulatedNumOfTasks - oldVal + numOfTasks) >
       (p_Fm->p_FmStateStruct->totalNumOfTasks - p_Fm->p_FmStateStruct->extraTasksPoolSize))
        RETURN_ERROR(MAJOR, E_NOT_AVAILABLE,
                     ("Requested numOfTasks and extra tasks pool for fm%d exceed total numOfTasks.",
                      p_Fm->p_FmStateStruct->fmId));
    else
    {
        ASSERT_COND(p_Fm->p_FmStateStruct->accumulatedNumOfTasks >= oldVal);
        /* update acummulated */
        p_Fm->p_FmStateStruct->accumulatedNumOfTasks -= oldVal;
        p_Fm->p_FmStateStruct->accumulatedNumOfTasks += numOfTasks;
        /* calculate reg */
        tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1]) & ~(BMI_NUM_OF_TASKS_MASK | BMI_NUM_OF_EXTRA_TASKS_MASK);
        tmpReg |= (uint32_t)(((numOfTasks-1) << BMI_NUM_OF_TASKS_SHIFT) |
                    (numOfExtraTasks << BMI_EXTRA_NUM_OF_TASKS_SHIFT));
        WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1],tmpReg);
    }

    return E_OK;
}

t_Error FmSetNumOfOpenDmas(t_Handle h_Fm,
                            uint8_t hardwarePortId,
                            uint8_t numOfOpenDmas,
                            uint8_t numOfExtraOpenDmas,
                            bool    initialConfig)

{
    t_Fm                    *p_Fm = (t_Fm *)h_Fm;
    uint8_t                 oldVal;
    uint32_t                tmpReg = 0;
    t_FmIpcPortRsrcParams   rsrcParams;
    t_Error                 err;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        t_FmIpcMsg          msg;
        t_FmIpcReply        reply;
        uint32_t            replyLength;

        rsrcParams.hardwarePortId = hardwarePortId;
        rsrcParams.val = numOfOpenDmas;
        rsrcParams.extra = numOfExtraOpenDmas;
        rsrcParams.boolInitialConfig = (uint8_t)initialConfig;

        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_SET_NUM_OF_OPEN_DMAS;
        memcpy(msg.msgBody, &rsrcParams, sizeof(rsrcParams));
        replyLength = sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) + sizeof(rsrcParams),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != sizeof(uint32_t))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        return (t_Error)(reply.error);
    }

    ASSERT_COND(IN_RANGE(1, hardwarePortId, 63));

    if(initialConfig)
        oldVal = 0;
    else
    {
        tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1]);
        /* read into oldVal the current extra tasks */
        oldVal = (uint8_t)((tmpReg & BMI_NUM_OF_EXTRA_DMAS_MASK) >> BMI_EXTRA_NUM_OF_DMAS_SHIFT);
    }

    if(numOfExtraOpenDmas > oldVal)
        p_Fm->p_FmStateStruct->extraOpenDmasPoolSize = (uint8_t)NCSW_MAX(p_Fm->p_FmStateStruct->extraOpenDmasPoolSize, numOfExtraOpenDmas);

    if(!initialConfig)
        /* read into oldVal the current num of tasks */
        oldVal = (uint8_t)(((tmpReg & BMI_NUM_OF_DMAS_MASK) >> BMI_NUM_OF_DMAS_SHIFT) + 1);

    /* check that there are enough uncommitted open DMA's */
    ASSERT_COND(p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas >= oldVal);
    if((p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas - oldVal + numOfOpenDmas) >
       p_Fm->p_FmStateStruct->maxNumOfOpenDmas)
        RETURN_ERROR(MAJOR, E_NOT_AVAILABLE,
                     ("Requested numOfOpenDmas for fm%d exceeds total numOfOpenDmas.",
                      p_Fm->p_FmStateStruct->fmId));
    else
    {
        /* update acummulated */
        p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas -= oldVal;
        p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas += numOfOpenDmas;
        /* calculate reg */
        tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1]) & ~(BMI_NUM_OF_DMAS_MASK | BMI_NUM_OF_EXTRA_DMAS_MASK);
        tmpReg |= (uint32_t)(((numOfOpenDmas-1) << BMI_NUM_OF_DMAS_SHIFT) |
                    (numOfExtraOpenDmas << BMI_EXTRA_NUM_OF_DMAS_SHIFT));
        WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1], tmpReg);

        /* update total num of DMA's with committed number of open DMAS, and max uncommitted pool. */
        tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg2) & ~BMI_CFG2_DMAS_MASK;
        tmpReg |= (uint32_t)(p_Fm->p_FmStateStruct->accumulatedNumOfOpenDmas + p_Fm->p_FmStateStruct->extraOpenDmasPoolSize - 1) << BMI_CFG2_DMAS_SHIFT;
        WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg2,  tmpReg);
    }

    return E_OK;
}

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FmDumpPortRegs (t_Handle h_Fm,uint8_t hardwarePortId)
{
    t_Fm            *p_Fm = (t_Fm *)h_Fm;
    t_FmIpcMsg      msg;
    t_Error         err;

    DECLARE_DUMP;

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_DUMP_PORT_REGS;
        msg.msgBody[0] = hardwarePortId;
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                    (uint8_t*)&msg,
                                    sizeof(msg.msgId)+sizeof(hardwarePortId),
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        return E_OK;
    }

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    DUMP_TITLE(&p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1], ("fmbm_pp for port %u", (hardwarePortId)));
    DUMP_MEMORY(&p_Fm->p_FmBmiRegs->fmbm_pp[hardwarePortId-1], sizeof(uint32_t));

    DUMP_TITLE(&p_Fm->p_FmBmiRegs->fmbm_pfs[hardwarePortId-1], ("fmbm_pfs for port %u", (hardwarePortId )));
    DUMP_MEMORY(&p_Fm->p_FmBmiRegs->fmbm_pfs[hardwarePortId-1], sizeof(uint32_t));

    DUMP_TITLE(&p_Fm->p_FmBmiRegs->fmbm_ppid[hardwarePortId-1], ("bm_ppid for port %u", (hardwarePortId)));
    DUMP_MEMORY(&p_Fm->p_FmBmiRegs->fmbm_ppid[hardwarePortId-1], sizeof(uint32_t));

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0)) */


/*****************************************************************************/
/*                      API Init unit functions                              */
/*****************************************************************************/
t_Handle FM_Config(t_FmParams *p_FmParam)
{
    t_Fm        *p_Fm;
    uint8_t     i;
    uintptr_t   baseAddr;

    SANITY_CHECK_RETURN_VALUE(p_FmParam, E_NULL_POINTER, NULL);
    SANITY_CHECK_RETURN_VALUE(((p_FmParam->firmware.p_Code && p_FmParam->firmware.size) ||
                               (!p_FmParam->firmware.p_Code && !p_FmParam->firmware.size)),
                              E_INVALID_VALUE, NULL);

    baseAddr = p_FmParam->baseAddr;

    /* Allocate FM structure */
    p_Fm = (t_Fm *) XX_Malloc(sizeof(t_Fm));
    if (!p_Fm)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM driver structure"));
        return NULL;
    }
    memset(p_Fm, 0, sizeof(t_Fm));

    p_Fm->p_FmStateStruct = (t_FmStateStruct *) XX_Malloc(sizeof(t_FmStateStruct));
    if (!p_Fm->p_FmStateStruct)
    {
        XX_Free(p_Fm);
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM Status structure"));
        return NULL;
    }
    memset(p_Fm->p_FmStateStruct, 0, sizeof(t_FmStateStruct));

    /* Initialize FM parameters which will be kept by the driver */
    p_Fm->p_FmStateStruct->fmId = p_FmParam->fmId;
    p_Fm->guestId               = p_FmParam->guestId;

    for(i=0; i<FM_MAX_NUM_OF_HW_PORT_IDS; i++)
        p_Fm->p_FmStateStruct->portsTypes[i] = e_FM_PORT_TYPE_DUMMY;

    /* Allocate the FM driver's parameters structure */
    p_Fm->p_FmDriverParam = (t_FmDriverParam *)XX_Malloc(sizeof(t_FmDriverParam));
    if (!p_Fm->p_FmDriverParam)
    {
        XX_Free(p_Fm->p_FmStateStruct);
        XX_Free(p_Fm);
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM driver parameters"));
        return NULL;
    }
    memset(p_Fm->p_FmDriverParam, 0, sizeof(t_FmDriverParam));

    /* Initialize FM parameters which will be kept by the driver */
    p_Fm->p_FmStateStruct->fmId              = p_FmParam->fmId;
    p_Fm->h_FmMuram         = p_FmParam->h_FmMuram;
    p_Fm->h_App             = p_FmParam->h_App;
    p_Fm->p_FmStateStruct->fmClkFreq         = p_FmParam->fmClkFreq;
    p_Fm->f_Exception       = p_FmParam->f_Exception;
    p_Fm->f_BusError        = p_FmParam->f_BusError;
    p_Fm->p_FmFpmRegs       = (t_FmFpmRegs *)UINT_TO_PTR(baseAddr + FM_MM_FPM);
    p_Fm->p_FmBmiRegs       = (t_FmBmiRegs *)UINT_TO_PTR(baseAddr + FM_MM_BMI);
    p_Fm->p_FmQmiRegs       = (t_FmQmiRegs *)UINT_TO_PTR(baseAddr + FM_MM_QMI);
    p_Fm->p_FmDmaRegs       = (t_FmDmaRegs *)UINT_TO_PTR(baseAddr + FM_MM_DMA);
    p_Fm->baseAddr          = baseAddr;
    p_Fm->p_FmStateStruct->irq               = p_FmParam->irq;
    p_Fm->p_FmStateStruct->errIrq            = p_FmParam->errIrq;
    p_Fm->hcPortInitialized = FALSE;
    p_Fm->independentMode   = FALSE;
    p_Fm->p_FmStateStruct->ramsEccEnable     = FALSE;
    p_Fm->p_FmStateStruct->totalNumOfTasks   = DEFAULT_totalNumOfTasks;
    p_Fm->p_FmStateStruct->totalFifoSize     = DEFAULT_totalFifoSize;
    p_Fm->p_FmStateStruct->maxNumOfOpenDmas  = DEFAULT_maxNumOfOpenDmas;
    p_Fm->p_FmStateStruct->extraFifoPoolSize = FM_MAX_NUM_OF_RX_PORTS*BMI_FIFO_UNITS;
    p_Fm->p_FmStateStruct->exceptions        = DEFAULT_exceptions;
    for(i = 0;i<FM_MAX_NUM_OF_1G_MACS;i++)
        p_Fm->p_FmStateStruct->macMaxFrameLengths1G[i] = DEFAULT_mtu;
#if defined(FM_MAX_NUM_OF_10G_MACS) && (FM_MAX_NUM_OF_10G_MACS)
    for(i = 0;i<FM_MAX_NUM_OF_10G_MACS;i++)
        p_Fm->p_FmStateStruct->macMaxFrameLengths10G[i] = DEFAULT_mtu;
#endif /*defined(FM_MAX_NUM_OF_10G_MACS) && (FM_MAX_NUM_OF_10G_MACS)*/

    p_Fm->h_Spinlock = XX_InitSpinlock();
    if (!p_Fm->h_Spinlock)
    {
        XX_Free(p_Fm->p_FmDriverParam);
        XX_Free(p_Fm->p_FmStateStruct);
        XX_Free(p_Fm);
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("cant allocate spinlock!"));
        return NULL;
    }

#ifdef FM_PARTITION_ARRAY
    /* Initialize FM driver parameters parameters (for initialization phase only) */
    memcpy(p_Fm->p_FmDriverParam->liodnBasePerPort, p_FmParam->liodnBasePerPort, FM_SIZE_OF_LIODN_TABLE*sizeof(uint16_t));
#endif /* FM_PARTITION_ARRAY */

    /*p_Fm->p_FmDriverParam->numOfPartitions                      = p_FmParam->numOfPartitions;    */
    p_Fm->p_FmDriverParam->enCounters                           = FALSE;

    p_Fm->p_FmDriverParam->resetOnInit                          = DEFAULT_resetOnInit;

    p_Fm->p_FmDriverParam->thresholds.dispLimit                 = DEFAULT_dispLimit;
    p_Fm->p_FmDriverParam->thresholds.prsDispTh                 = DEFAULT_prsDispTh;
    p_Fm->p_FmDriverParam->thresholds.plcrDispTh                = DEFAULT_plcrDispTh;
    p_Fm->p_FmDriverParam->thresholds.kgDispTh                  = DEFAULT_kgDispTh;
    p_Fm->p_FmDriverParam->thresholds.bmiDispTh                 = DEFAULT_bmiDispTh;
    p_Fm->p_FmDriverParam->thresholds.qmiEnqDispTh              = DEFAULT_qmiEnqDispTh;
    p_Fm->p_FmDriverParam->thresholds.qmiDeqDispTh              = DEFAULT_qmiDeqDispTh;
    p_Fm->p_FmDriverParam->thresholds.fmCtl1DispTh              = DEFAULT_fmCtl1DispTh;
    p_Fm->p_FmDriverParam->thresholds.fmCtl2DispTh              = DEFAULT_fmCtl2DispTh;

    p_Fm->p_FmDriverParam->dmaStopOnBusError                    = DEFAULT_dmaStopOnBusError;

    p_Fm->p_FmDriverParam->dmaCacheOverride                     = DEFAULT_cacheOverride;
    p_Fm->p_FmDriverParam->dmaAidMode                           = DEFAULT_aidMode;
    p_Fm->p_FmDriverParam->dmaAidOverride                       = DEFAULT_aidOverride;
    p_Fm->p_FmDriverParam->dmaAxiDbgNumOfBeats                  = DEFAULT_axiDbgNumOfBeats;
    p_Fm->p_FmDriverParam->dmaCamNumOfEntries                   = DEFAULT_dmaCamNumOfEntries;
    p_Fm->p_FmDriverParam->dmaWatchdog                          = DEFAULT_dmaWatchdog;

    p_Fm->p_FmDriverParam->dmaCommQThresholds.clearEmergency        = DEFAULT_dmaCommQLow;
    p_Fm->p_FmDriverParam->dmaCommQThresholds.assertEmergency       = DEFAULT_dmaCommQHigh;
    p_Fm->p_FmDriverParam->dmaReadBufThresholds.clearEmergency      = DEFAULT_dmaReadIntBufLow;
    p_Fm->p_FmDriverParam->dmaReadBufThresholds.assertEmergency     = DEFAULT_dmaReadIntBufHigh;
    p_Fm->p_FmDriverParam->dmaWriteBufThresholds.clearEmergency     = DEFAULT_dmaWriteIntBufLow;
    p_Fm->p_FmDriverParam->dmaWriteBufThresholds.assertEmergency    = DEFAULT_dmaWriteIntBufHigh;
    p_Fm->p_FmDriverParam->dmaSosEmergency                          = DEFAULT_dmaSosEmergency;

    p_Fm->p_FmDriverParam->dmaDbgCntMode                        = DEFAULT_dmaDbgCntMode;

    p_Fm->p_FmDriverParam->dmaEnEmergency                       = FALSE;
    p_Fm->p_FmDriverParam->dmaEnEmergencySmoother               = FALSE;
    p_Fm->p_FmDriverParam->catastrophicErr                      = DEFAULT_catastrophicErr;
    p_Fm->p_FmDriverParam->dmaErr                               = DEFAULT_dmaErr;
    p_Fm->p_FmDriverParam->haltOnExternalActivation             = DEFAULT_haltOnExternalActivation;
    p_Fm->p_FmDriverParam->haltOnUnrecoverableEccError          = DEFAULT_haltOnUnrecoverableEccError;
    p_Fm->p_FmDriverParam->enIramTestMode                       = FALSE;
    p_Fm->p_FmDriverParam->enMuramTestMode                      = FALSE;
    p_Fm->p_FmDriverParam->externalEccRamsEnable                = DEFAULT_externalEccRamsEnable;

    p_Fm->p_FmDriverParam->fwVerify                             = DEFAULT_VerifyUcode;
    p_Fm->p_FmDriverParam->firmware.size                        = p_FmParam->firmware.size;
    if (p_Fm->p_FmDriverParam->firmware.size)
    {
        p_Fm->p_FmDriverParam->firmware.p_Code = (uint32_t *)XX_Malloc(p_Fm->p_FmDriverParam->firmware.size);
        if (!p_Fm->p_FmDriverParam->firmware.p_Code)
        {
            XX_FreeSpinlock(p_Fm->h_Spinlock);
            XX_Free(p_Fm->p_FmStateStruct);
            XX_Free(p_Fm->p_FmDriverParam);
            XX_Free(p_Fm);
            REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM firmware code"));
            return NULL;
        }
        memcpy(p_Fm->p_FmDriverParam->firmware.p_Code, p_FmParam->firmware.p_Code, p_Fm->p_FmDriverParam->firmware.size);
    }

    return p_Fm;
}

/**************************************************************************//**
 @Function      FM_Init

 @Description   Initializes the FM module

 @Param[in]     h_Fm - FM module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_Init(t_Handle h_Fm)
{
    t_Fm                    *p_Fm = (t_Fm*)h_Fm;
    t_FmDriverParam         *p_FmDriverParam = NULL;
    t_Error                 err = E_OK;
    uint32_t                tmpReg, cfgReg = 0;
    int                     i;
    uint16_t                periodInFmClocks;
    uint8_t                 remainder;
    t_FmRevisionInfo        revInfo;

    SANITY_CHECK_RETURN_ERROR(h_Fm, E_INVALID_HANDLE);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        uint8_t             isMasterAlive;
        t_FmIpcMsg          msg;
        t_FmIpcReply        reply;
        uint32_t            replyLength;

        /* build the FM guest partition IPC address */
        if(Sprint (p_Fm->fmModuleName, "FM_%d_%d",p_Fm->p_FmStateStruct->fmId, p_Fm->guestId) != (p_Fm->guestId<10 ? 6:7))
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Sprint failed"));

        /* build the FM master partition IPC address */
        memset(p_Fm->fmIpcHandlerModuleName, 0, (sizeof(char)) * MODULE_NAME_SIZE);
        if(Sprint (p_Fm->fmIpcHandlerModuleName[0], "FM_%d_%d",p_Fm->p_FmStateStruct->fmId, NCSW_MASTER_ID) != 6)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Sprint failed"));

        for(i=0;i<e_FM_EV_DUMMY_LAST;i++)
            p_Fm->intrMng[i].f_Isr = UnimplementedIsr;

        p_Fm->h_IpcSessions[0] = XX_IpcInitSession(p_Fm->fmIpcHandlerModuleName[0], p_Fm->fmModuleName);
        if (p_Fm->h_IpcSessions[0])
        {
            err = XX_IpcRegisterMsgHandler(p_Fm->fmModuleName, FmGuestHandleIpcMsgCB, p_Fm, FM_IPC_MAX_REPLY_SIZE);
            if(err)
                RETURN_ERROR(MAJOR, err, NO_MSG);

            memset(&msg, 0, sizeof(msg));
            memset(&reply, 0, sizeof(reply));
            msg.msgId = FM_MASTER_IS_ALIVE;
            msg.msgBody[0] = p_Fm->guestId;
            replyLength = sizeof(uint32_t) + sizeof(uint8_t);
            do
            {
                blockingFlag = TRUE;
                if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                             (uint8_t*)&msg,
                                             sizeof(msg.msgId)+sizeof(p_Fm->guestId),
                                             (uint8_t*)&reply,
                                             &replyLength,
                                             IpcMsgCompletionCB,
                                             h_Fm)) != E_OK)
                    REPORT_ERROR(MINOR, err, NO_MSG);
                while(blockingFlag) ;
                if(replyLength != (sizeof(uint32_t) + sizeof(uint8_t)))
                    REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
                isMasterAlive = *(uint8_t*)(reply.replyBody);
            } while (!isMasterAlive);

            memset(&msg, 0, sizeof(msg));
            memset(&reply, 0, sizeof(reply));
            msg.msgId = FM_GET_CLK_FREQ;
            replyLength = sizeof(uint32_t) + sizeof(p_Fm->p_FmStateStruct->fmClkFreq);
            if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                         (uint8_t*)&msg,
                                         sizeof(msg.msgId),
                                         (uint8_t*)&reply,
                                         &replyLength,
                                         NULL,
                                         NULL)) != E_OK)
                RETURN_ERROR(MAJOR, err, NO_MSG);
            if(replyLength != (sizeof(uint32_t) + sizeof(p_Fm->p_FmStateStruct->fmClkFreq)))
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
            memcpy((uint8_t*)&p_Fm->p_FmStateStruct->fmClkFreq, reply.replyBody, sizeof(uint16_t));
        }
        else
        {
            DBG(WARNING, ("FM Guest mode - without IPC"));
            if(!p_Fm->p_FmStateStruct->fmClkFreq )
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("No fmClkFreq configured for guest without IPC"));
            if(!p_Fm->baseAddr)
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("No baseAddr configured for guest without IPC"));
        }

        XX_Free(p_Fm->p_FmDriverParam);
        p_Fm->p_FmDriverParam = NULL;

        if ((p_Fm->guestId == NCSW_MASTER_ID) ||
            (p_Fm->h_IpcSessions[0]))
        {
            FM_DisableRamsEcc(p_Fm);
            FmMuramClear(p_Fm->h_FmMuram);
            FM_EnableRamsEcc(p_Fm);
        }

        return E_OK;
    }

    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    FM_GetRevision(p_Fm, &revInfo);

#ifdef FM_NO_DISPATCH_RAM_ECC
    if (revInfo.majorRev != 4)
        p_Fm->p_FmStateStruct->exceptions &= ~FM_EX_BMI_DISPATCH_RAM_ECC;
#endif /* FM_NO_DISPATCH_RAM_ECC */

#ifdef FM_RAM_LIST_ERR_IRQ_ERRATA_FMAN8
    if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        p_Fm->p_FmStateStruct->exceptions  &= ~FM_EX_BMI_LIST_RAM_ECC;
#endif   /* FM_RAM_LIST_ERR_IRQ_ERRATA_FMAN8 */

#ifdef FM_BMI_PIPELINE_ERR_IRQ_ERRATA_FMAN9
    if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        p_Fm->p_FmStateStruct->exceptions  &= ~FM_EX_BMI_PIPELINE_ECC;
#endif /* FM_BMI_PIPELINE_ERR_IRQ_ERRATA_FMAN9 */

#ifdef FM_QMI_NO_ECC_EXCEPTIONS
    if (revInfo.majorRev == 4)
        p_Fm->p_FmStateStruct->exceptions  &= ~(FM_EX_QMI_SINGLE_ECC | FM_EX_QMI_DOUBLE_ECC);
#endif /* FM_QMI_NO_ECC_EXCEPTIONS */

    CHECK_INIT_PARAMETERS(p_Fm, CheckFmParameters);

    p_FmDriverParam = p_Fm->p_FmDriverParam;

    FmMuramClear(p_Fm->h_FmMuram);

#ifdef FM_UCODE_NOT_RESET_ERRATA_BUGZILLA6173
    if (p_FmDriverParam->resetOnInit)
    {
        t_FMIramRegs    *p_Iram = (t_FMIramRegs *)UINT_TO_PTR(p_Fm->baseAddr + FM_MM_IMEM);
        uint32_t        debug_reg;

        /* write to IRAM first location the debug instruction */
        WRITE_UINT32(p_Iram->iadd, 0);
        while (GET_UINT32(p_Iram->iadd) != 0) ;
        WRITE_UINT32(p_Iram->idata, FM_UCODE_DEBUG_INSTRUCTION);

        WRITE_UINT32(p_Iram->iadd, 0);
        while (GET_UINT32(p_Iram->iadd) != 0) ;
        while (GET_UINT32(p_Iram->idata) != FM_UCODE_DEBUG_INSTRUCTION) ;

        /* Enable patch from IRAM */
        WRITE_UINT32(p_Iram->iready, IRAM_READY);
        XX_UDelay(100);

        /* reset FMAN */
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrstc, FPM_RSTC_FM_RESET);
        XX_UDelay(100);

        /* verify breakpoint debug status register */
        debug_reg = GET_UINT32(*(uint32_t *)UINT_TO_PTR(p_Fm->baseAddr + FM_DEBUG_STATUS_REGISTER_OFFSET));
#ifndef NCSW_LINUX
        if(!debug_reg)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Invalid debug status register value = 0"));
#else
        if(!debug_reg)
            DBG(INFO,("Invalid debug status register value = 0"));
#endif
        /*************************************/
        /* Load FMan-Controller code to Iram */
        /*************************************/
        if (ClearIRam(p_Fm) != E_OK)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
        if (p_Fm->p_FmDriverParam->firmware.p_Code &&
            (LoadFmanCtrlCode(p_Fm) != E_OK))
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
         XX_UDelay(100);

        /* reset FMAN again to start the microcode */
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrstc, FPM_RSTC_FM_RESET);
        XX_UDelay(1000);
    }
    else
    {
#endif /* FM_UCODE_NOT_RESET_ERRATA_BUGZILLA6173 */
    if(p_FmDriverParam->resetOnInit)
    {
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrstc, FPM_RSTC_FM_RESET);
        XX_UDelay(100);
    }

    /*************************************/
    /* Load FMan-Controller code to Iram */
    /*************************************/
    if (ClearIRam(p_Fm) != E_OK)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
    if (p_Fm->p_FmDriverParam->firmware.p_Code &&
        (LoadFmanCtrlCode(p_Fm) != E_OK))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
#ifdef FM_UCODE_NOT_RESET_ERRATA_BUGZILLA6173
    }
#endif /* FM_UCODE_NOT_RESET_ERRATA_BUGZILLA6173 */

#ifdef FM_CAPWAP_SUPPORT
    /* save first 256 byte in MURAM */
    p_Fm->resAddr = PTR_TO_UINT(FM_MURAM_AllocMem(p_Fm->h_FmMuram, 256, 0));
    if (!p_Fm->resAddr)
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("MURAM alloc for reserved Area failed"));

    WRITE_BLOCK(UINT_TO_PTR(p_Fm->resAddr), 0, 256);
#endif /* FM_CAPWAP_SUPPORT */

    /* General FM driver initialization */
    p_Fm->fmMuramPhysBaseAddr = (uint64_t)(XX_VirtToPhys(UINT_TO_PTR(p_Fm->baseAddr + FM_MM_MURAM)));
    for(i=0;i<e_FM_EV_DUMMY_LAST;i++)
        p_Fm->intrMng[i].f_Isr = UnimplementedIsr;
    for(i=0;i<FM_NUM_OF_FMAN_CTRL_EVENT_REGS;i++)
        p_Fm->fmanCtrlIntr[i].f_Isr = UnimplementedFmanCtrlIsr;

    /**********************/
    /* Init DMA Registers */
    /**********************/
    /* clear status reg events */
    tmpReg = (DMA_STATUS_BUS_ERR | DMA_STATUS_READ_ECC | DMA_STATUS_SYSTEM_WRITE_ECC | DMA_STATUS_FM_WRITE_ECC);
    /*tmpReg |= (DMA_STATUS_SYSTEM_DPEXT_ECC | DMA_STATUS_FM_DPEXT_ECC | DMA_STATUS_SYSTEM_DPDAT_ECC | DMA_STATUS_FM_DPDAT_ECC | DMA_STATUS_FM_SPDAT_ECC);*/
    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmsr, GET_UINT32(p_Fm->p_FmDmaRegs->fmdmsr) | tmpReg);

    /* configure mode register */
    tmpReg = 0;
    tmpReg |= p_FmDriverParam->dmaCacheOverride << DMA_MODE_CACHE_OR_SHIFT;
    if(p_FmDriverParam->dmaAidOverride)
        tmpReg |= DMA_MODE_AID_OR;
    if (p_Fm->p_FmStateStruct->exceptions & FM_EX_DMA_BUS_ERROR)
        tmpReg |= DMA_MODE_BER;
    if ((p_Fm->p_FmStateStruct->exceptions & FM_EX_DMA_SYSTEM_WRITE_ECC) | (p_Fm->p_FmStateStruct->exceptions & FM_EX_DMA_READ_ECC) | (p_Fm->p_FmStateStruct->exceptions & FM_EX_DMA_FM_WRITE_ECC))
        tmpReg |= DMA_MODE_ECC;
    if(p_FmDriverParam->dmaStopOnBusError)
        tmpReg |= DMA_MODE_SBER;
    tmpReg |= (uint32_t)(p_FmDriverParam->dmaAxiDbgNumOfBeats - 1) << DMA_MODE_AXI_DBG_SHIFT;
    if (p_FmDriverParam->dmaEnEmergency)
    {
        tmpReg |= p_FmDriverParam->dmaEmergency.emergencyBusSelect;
        tmpReg |= p_FmDriverParam->dmaEmergency.emergencyLevel << DMA_MODE_EMERGENCY_LEVEL_SHIFT;
        if(p_FmDriverParam->dmaEnEmergencySmoother)
            WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmemsr, p_FmDriverParam->dmaEmergencySwitchCounter);
     }
    tmpReg |= ((p_FmDriverParam->dmaCamNumOfEntries/DMA_CAM_UNITS) - 1) << DMA_MODE_CEN_SHIFT;

    tmpReg |= DMA_MODE_SECURE_PROT;
    tmpReg |= p_FmDriverParam->dmaDbgCntMode << DMA_MODE_DBG_SHIFT;
    tmpReg |= p_FmDriverParam->dmaAidMode << DMA_MODE_AID_MODE_SHIFT;

#ifdef FM_PEDANTIC_DMA
    tmpReg |= DMA_MODE_EMERGENCY_READ;
#endif /* FM_PEDANTIC_DMA */

    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmmr, tmpReg);

    /* configure thresholds register */
    tmpReg = ((uint32_t)p_FmDriverParam->dmaCommQThresholds.assertEmergency << DMA_THRESH_COMMQ_SHIFT) |
                ((uint32_t)p_FmDriverParam->dmaReadBufThresholds.assertEmergency << DMA_THRESH_READ_INT_BUF_SHIFT) |
                ((uint32_t)p_FmDriverParam->dmaWriteBufThresholds.assertEmergency);
    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmtr, tmpReg);

    /* configure hysteresis register */
    tmpReg = ((uint32_t)p_FmDriverParam->dmaCommQThresholds.clearEmergency << DMA_THRESH_COMMQ_SHIFT) |
                ((uint32_t)p_FmDriverParam->dmaReadBufThresholds.clearEmergency << DMA_THRESH_READ_INT_BUF_SHIFT) |
                ((uint32_t)p_FmDriverParam->dmaWriteBufThresholds.clearEmergency);
    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmhy, tmpReg);

    /* configure emergency threshold */
    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmsetr, p_FmDriverParam->dmaSosEmergency);

    /* configure Watchdog */
    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmwcr, USEC_TO_CLK(p_FmDriverParam->dmaWatchdog, p_Fm->p_FmStateStruct->fmClkFreq));

    /* Allocate MURAM for CAM */
    p_Fm->camBaseAddr = PTR_TO_UINT(FM_MURAM_AllocMem(p_Fm->h_FmMuram,
                                                      (uint32_t)(p_FmDriverParam->dmaCamNumOfEntries*DMA_CAM_SIZEOF_ENTRY),
                                                      DMA_CAM_ALIGN));
    if (!p_Fm->camBaseAddr )
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("MURAM alloc for DMA CAM failed"));

    WRITE_BLOCK(UINT_TO_PTR(p_Fm->camBaseAddr), 0, (uint32_t)(p_FmDriverParam->dmaCamNumOfEntries*DMA_CAM_SIZEOF_ENTRY));

    /* VirtToPhys */
    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmebcr,
                 (uint32_t)(XX_VirtToPhys(UINT_TO_PTR(p_Fm->camBaseAddr)) - p_Fm->fmMuramPhysBaseAddr));

#ifdef FM_PARTITION_ARRAY
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            /* liodn-partitions */
            for (i=0 ; i<FM_SIZE_OF_LIODN_TABLE ; i+=2)
            {
                tmpReg = (((uint32_t)p_FmDriverParam->liodnBasePerPort[i] << DMA_LIODN_SHIFT) |
                            (uint32_t)p_FmDriverParam->liodnBasePerPort[i+1]);
                WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmplr[i/2], tmpReg);
            }
    }
#endif /* FM_PARTITION_ARRAY */

    /**********************/
    /* Init FPM Registers */
    /**********************/
    tmpReg = (uint32_t)(p_FmDriverParam->thresholds.dispLimit << FPM_DISP_LIMIT_SHIFT);
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmflc, tmpReg);

    tmpReg =   (((uint32_t)p_FmDriverParam->thresholds.prsDispTh  << FPM_THR1_PRS_SHIFT) |
                ((uint32_t)p_FmDriverParam->thresholds.kgDispTh  << FPM_THR1_KG_SHIFT) |
                ((uint32_t)p_FmDriverParam->thresholds.plcrDispTh  << FPM_THR1_PLCR_SHIFT) |
                ((uint32_t)p_FmDriverParam->thresholds.bmiDispTh  << FPM_THR1_BMI_SHIFT));
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmdis1, tmpReg);

    tmpReg =   (((uint32_t)p_FmDriverParam->thresholds.qmiEnqDispTh  << FPM_THR2_QMI_ENQ_SHIFT) |
                ((uint32_t)p_FmDriverParam->thresholds.qmiDeqDispTh  << FPM_THR2_QMI_DEQ_SHIFT) |
                ((uint32_t)p_FmDriverParam->thresholds.fmCtl1DispTh  << FPM_THR2_FM_CTL1_SHIFT) |
                ((uint32_t)p_FmDriverParam->thresholds.fmCtl2DispTh  << FPM_THR2_FM_CTL2_SHIFT));
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmdis2, tmpReg);

    /* define exceptions and error behavior */
    tmpReg = 0;
    /* Clear events */
    tmpReg |= (FPM_EV_MASK_STALL | FPM_EV_MASK_DOUBLE_ECC | FPM_EV_MASK_SINGLE_ECC);
    /* enable interrupts */
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_FPM_STALL_ON_TASKS)
        tmpReg |= FPM_EV_MASK_STALL_EN;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_FPM_SINGLE_ECC)
        tmpReg |= FPM_EV_MASK_SINGLE_ECC_EN;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_FPM_DOUBLE_ECC)
        tmpReg |= FPM_EV_MASK_DOUBLE_ECC_EN;
    tmpReg |= (p_Fm->p_FmDriverParam->catastrophicErr  << FPM_EV_MASK_CAT_ERR_SHIFT);
    tmpReg |= (p_Fm->p_FmDriverParam->dmaErr << FPM_EV_MASK_DMA_ERR_SHIFT);
    if(!p_Fm->p_FmDriverParam->haltOnExternalActivation)
        tmpReg |= FPM_EV_MASK_EXTERNAL_HALT;
    if(!p_Fm->p_FmDriverParam->haltOnUnrecoverableEccError)
        tmpReg |= FPM_EV_MASK_ECC_ERR_HALT;
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmem, tmpReg);

    /* clear all fmCtls event registers */
    for(i=0;i<FM_NUM_OF_FMAN_CTRL_EVENT_REGS;i++)
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmcev[i], 0xFFFFFFFF);

    /* RAM ECC -  enable and clear events*/
    /* first we need to clear all parser memory, as it is uninitialized and
    may cause ECC errors */
    tmpReg = 0;
    /* event bits */
    tmpReg = (FPM_RAM_CTL_MURAM_ECC | FPM_RAM_CTL_IRAM_ECC);
    /* Rams enable is not effected by the RCR bit, but by a COP configuration */
    if(p_Fm->p_FmDriverParam->externalEccRamsEnable)
        tmpReg |= FPM_RAM_CTL_RAMS_ECC_EN_SRC_SEL;

    /* enable test mode */
    if(p_FmDriverParam->enMuramTestMode)
        tmpReg |= FPM_RAM_CTL_MURAM_TEST_ECC;
    if(p_FmDriverParam->enIramTestMode)
        tmpReg |= FPM_RAM_CTL_IRAM_TEST_ECC;
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, tmpReg);

    tmpReg = 0;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_IRAM_ECC)
    {
        tmpReg |= FPM_IRAM_ECC_ERR_EX_EN;
        FmEnableRamsEcc(p_Fm);
    }
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_NURAM_ECC)
    {
        tmpReg |= FPM_MURAM_ECC_ERR_EX_EN;
        FmEnableRamsEcc(p_Fm);
    }
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrie, tmpReg);

    /**********************/
    /* Init BMI Registers */
    /**********************/

    /* define common resources */
    /* allocate MURAM for FIFO according to total size */
    p_Fm->fifoBaseAddr = PTR_TO_UINT(FM_MURAM_AllocMem(p_Fm->h_FmMuram,
                                                       p_Fm->p_FmStateStruct->totalFifoSize,
                                                       BMI_FIFO_ALIGN));
    if (!p_Fm->fifoBaseAddr)
    {
        FreeInitResources(p_Fm);
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("MURAM alloc for FIFO failed"));
    }

    tmpReg = (uint32_t)(XX_VirtToPhys(UINT_TO_PTR(p_Fm->fifoBaseAddr)) - p_Fm->fmMuramPhysBaseAddr);
    tmpReg = tmpReg / BMI_FIFO_ALIGN;

    tmpReg |= ((p_Fm->p_FmStateStruct->totalFifoSize/BMI_FIFO_UNITS - 1) << BMI_CFG1_FIFO_SIZE_SHIFT);
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg1, tmpReg);

    tmpReg =  ((uint32_t)(p_Fm->p_FmStateStruct->totalNumOfTasks - 1) << BMI_CFG2_TASKS_SHIFT );
    /* num of DMA's will be dynamically updated when each port is set */
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg2, tmpReg);

    /* define unmaskable exceptions, enable and clear events */
    tmpReg = 0;
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ievr, (BMI_ERR_INTR_EN_LIST_RAM_ECC |
                                                BMI_ERR_INTR_EN_PIPELINE_ECC |
                                                BMI_ERR_INTR_EN_STATISTICS_RAM_ECC |
                                                BMI_ERR_INTR_EN_DISPATCH_RAM_ECC));
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_LIST_RAM_ECC)
        tmpReg |= BMI_ERR_INTR_EN_LIST_RAM_ECC;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_PIPELINE_ECC)
        tmpReg |= BMI_ERR_INTR_EN_PIPELINE_ECC;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_STATISTICS_RAM_ECC)
        tmpReg |= BMI_ERR_INTR_EN_STATISTICS_RAM_ECC;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_DISPATCH_RAM_ECC)
        tmpReg |= BMI_ERR_INTR_EN_DISPATCH_RAM_ECC;
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier, tmpReg);

    /**********************/
    /* Init QMI Registers */
    /**********************/
     /* Clear error interrupt events */
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eie, (QMI_ERR_INTR_EN_DOUBLE_ECC | QMI_ERR_INTR_EN_DEQ_FROM_DEF));
    tmpReg = 0;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID)
        tmpReg |= QMI_ERR_INTR_EN_DEQ_FROM_DEF;
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_QMI_DOUBLE_ECC)
        tmpReg |= QMI_ERR_INTR_EN_DOUBLE_ECC;
    /* enable events */
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eien, tmpReg);

    if(p_Fm->p_FmDriverParam->tnumAgingPeriod)
    {
        /* tnumAgingPeriod is in units of microseconds, p_FmClockFreq is in Mhz */
        periodInFmClocks = (uint16_t)(p_Fm->p_FmDriverParam->tnumAgingPeriod*p_Fm->p_FmStateStruct->fmClkFreq);
        /* periodInFmClocks must be a 64 multiply */
        remainder = (uint8_t)(periodInFmClocks % 64);
        if (remainder > 64)
            tmpReg = (uint32_t)((periodInFmClocks/64) + 1);
        else
        {
            tmpReg = (uint32_t)(periodInFmClocks/64);
            if(!tmpReg)
                tmpReg = 1;
        }
        tmpReg <<= QMI_TAPC_TAP;
        WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_tapc, tmpReg);

    }
    tmpReg = 0;
    /* Clear interrupt events */
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_ie, QMI_INTR_EN_SINGLE_ECC);
    if(p_Fm->p_FmStateStruct->exceptions & FM_EX_QMI_SINGLE_ECC)
        tmpReg |= QMI_INTR_EN_SINGLE_ECC;
    /* enable events */
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_ien, tmpReg);

    /* clear & enable global counters  - calculate reg and save for later,
       because it's the same reg for QMI enable */
    if(p_Fm->p_FmDriverParam->enCounters)
        cfgReg = QMI_CFG_EN_COUNTERS;
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    cfgReg |= (uint32_t)(((QMI_DEF_TNUMS_THRESH) << 8) |  (uint32_t)QMI_DEF_TNUMS_THRESH);
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

    if (p_Fm->p_FmStateStruct->irq != NO_IRQ)
    {
        XX_SetIntr(p_Fm->p_FmStateStruct->irq, FM_EventIsr, p_Fm);
        XX_EnableIntr(p_Fm->p_FmStateStruct->irq);
    }

    if (p_Fm->p_FmStateStruct->errIrq != NO_IRQ)
    {
        XX_SetIntr(p_Fm->p_FmStateStruct->errIrq, ErrorIsrCB, p_Fm);
        XX_EnableIntr(p_Fm->p_FmStateStruct->errIrq);
    }

    /* build the FM master partition IPC address */
    if (Sprint (p_Fm->fmModuleName, "FM_%d_%d",p_Fm->p_FmStateStruct->fmId, NCSW_MASTER_ID) != 6)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Sprint failed"));

    err = XX_IpcRegisterMsgHandler(p_Fm->fmModuleName, FmHandleIpcMsgCB, p_Fm, FM_IPC_MAX_REPLY_SIZE);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    /**********************/
    /* Enable all modules */
    /**********************/
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_init, BMI_INIT_START);
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_gc, cfgReg | QMI_CFG_ENQ_EN | QMI_CFG_DEQ_EN);

    if (p_Fm->p_FmDriverParam->firmware.p_Code)
    {
        XX_Free(p_Fm->p_FmDriverParam->firmware.p_Code);
        p_Fm->p_FmDriverParam->firmware.p_Code = NULL;
    }

    XX_Free(p_Fm->p_FmDriverParam);
    p_Fm->p_FmDriverParam = NULL;

    return E_OK;
}

/**************************************************************************//**
 @Function      FM_Free

 @Description   Frees all resources that were assigned to FM module.

                Calling this routine invalidates the descriptor.

 @Param[in]     h_Fm - FM module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_Free(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    if (p_Fm->guestId != NCSW_MASTER_ID)
    {
        XX_IpcUnregisterMsgHandler(p_Fm->fmModuleName);

        if(!p_Fm->recoveryMode)
            XX_Free(p_Fm->p_FmStateStruct);

        XX_Free(p_Fm);

        return E_OK;
    }

    /* disable BMI and QMI */
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_init, 0);
    WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_gc, 0);

    /* release BMI resources */
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg2, 0);
    WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_cfg1, 0);

    /* disable ECC */
    WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, 0);

    if ((p_Fm->guestId == NCSW_MASTER_ID) && (p_Fm->fmModuleName[0] != 0))
        XX_IpcUnregisterMsgHandler(p_Fm->fmModuleName);

    if (p_Fm->p_FmStateStruct)
    {
        if (p_Fm->p_FmStateStruct->irq != NO_IRQ)
        {
            XX_DisableIntr(p_Fm->p_FmStateStruct->irq);
            XX_FreeIntr(p_Fm->p_FmStateStruct->irq);
        }
        if (p_Fm->p_FmStateStruct->errIrq != NO_IRQ)
        {
            XX_DisableIntr(p_Fm->p_FmStateStruct->errIrq);
            XX_FreeIntr(p_Fm->p_FmStateStruct->errIrq);
        }
    }

    if (p_Fm->h_Spinlock)
        XX_FreeSpinlock(p_Fm->h_Spinlock);

    if (p_Fm->p_FmDriverParam)
    {
        if (p_Fm->p_FmDriverParam->firmware.p_Code)
            XX_Free(p_Fm->p_FmDriverParam->firmware.p_Code);
        XX_Free(p_Fm->p_FmDriverParam);
        p_Fm->p_FmDriverParam = NULL;
    }

    FreeInitResources(p_Fm);

    if (!p_Fm->recoveryMode && p_Fm->p_FmStateStruct)
        XX_Free(p_Fm->p_FmStateStruct);

    XX_Free(p_Fm);

    return E_OK;
}

/*************************************************/
/*       API Advanced Init unit functions        */
/*************************************************/

t_Error FM_ConfigResetOnInit(t_Handle h_Fm, bool enable)
{

    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->resetOnInit = enable;

    return E_OK;
}


t_Error FM_ConfigTotalNumOfTasks(t_Handle h_Fm, uint8_t totalNumOfTasks)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmStateStruct->totalNumOfTasks = totalNumOfTasks;

    return E_OK;
}

t_Error FM_ConfigTotalFifoSize(t_Handle h_Fm, uint32_t totalFifoSize)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmStateStruct->totalFifoSize = totalFifoSize;

    return E_OK;
}

t_Error FM_ConfigMaxNumOfOpenDmas(t_Handle h_Fm, uint8_t maxNumOfOpenDmas)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmStateStruct->maxNumOfOpenDmas = maxNumOfOpenDmas;

    return E_OK;
}

t_Error FM_ConfigThresholds(t_Handle h_Fm, t_FmThresholds *p_FmThresholds)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    memcpy(&p_Fm->p_FmDriverParam->thresholds, p_FmThresholds, sizeof(t_FmThresholds));

    return E_OK;
}

t_Error FM_ConfigDmaCacheOverride(t_Handle h_Fm, e_FmDmaCacheOverride cacheOverride)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaCacheOverride = cacheOverride;

    return E_OK;
}

t_Error FM_ConfigDmaAidOverride(t_Handle h_Fm, bool aidOverride)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaAidOverride = aidOverride;

    return E_OK;
}

t_Error FM_ConfigDmaAidMode(t_Handle h_Fm, e_FmDmaAidMode aidMode)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaAidMode = aidMode;

    return E_OK;
}

t_Error FM_ConfigDmaAxiDbgNumOfBeats(t_Handle h_Fm, uint8_t axiDbgNumOfBeats)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaAxiDbgNumOfBeats = axiDbgNumOfBeats;

    return E_OK;
}

t_Error FM_ConfigDmaCamNumOfEntries(t_Handle h_Fm, uint8_t numOfEntries)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaCamNumOfEntries = numOfEntries;

    return E_OK;
}

t_Error FM_ConfigDmaWatchdog(t_Handle h_Fm, uint32_t watchdogValue)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

#ifdef FM_NO_WATCHDOG
    {
        t_FmRevisionInfo    revInfo;
        FM_GetRevision(h_Fm, &revInfo);
        if (revInfo.majorRev != 4)
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("watchdog!"));
    }
#endif /* FM_NO_WATCHDOG */

    p_Fm->p_FmDriverParam->dmaWatchdog = watchdogValue;

    return E_OK;
}

t_Error FM_ConfigDmaWriteBufThresholds(t_Handle h_Fm, t_FmDmaThresholds *p_FmDmaThresholds)

{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    memcpy(&p_Fm->p_FmDriverParam->dmaWriteBufThresholds, p_FmDmaThresholds, sizeof(t_FmDmaThresholds));

    return E_OK;
}

t_Error FM_ConfigDmaCommQThresholds(t_Handle h_Fm, t_FmDmaThresholds *p_FmDmaThresholds)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    memcpy(&p_Fm->p_FmDriverParam->dmaCommQThresholds, p_FmDmaThresholds, sizeof(t_FmDmaThresholds));

    return E_OK;
}

t_Error FM_ConfigDmaReadBufThresholds(t_Handle h_Fm, t_FmDmaThresholds *p_FmDmaThresholds)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    memcpy(&p_Fm->p_FmDriverParam->dmaReadBufThresholds, p_FmDmaThresholds, sizeof(t_FmDmaThresholds));

    return E_OK;
}

t_Error FM_ConfigDmaEmergency(t_Handle h_Fm, t_FmDmaEmergency *p_Emergency)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaEnEmergency = TRUE;
    memcpy(&p_Fm->p_FmDriverParam->dmaEmergency, p_Emergency, sizeof(t_FmDmaEmergency));

    return E_OK;
}

t_Error FM_ConfigDmaEmergencySmoother(t_Handle h_Fm, uint32_t emergencyCnt)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    if(!p_Fm->p_FmDriverParam->dmaEnEmergency)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("FM_ConfigEnDmaEmergencySmoother may be called only after FM_ConfigEnDmaEmergency"));

    p_Fm->p_FmDriverParam->dmaEnEmergencySmoother = TRUE;
    p_Fm->p_FmDriverParam->dmaEmergencySwitchCounter = emergencyCnt;

    return E_OK;
}

t_Error FM_ConfigDmaDbgCounter(t_Handle h_Fm, e_FmDmaDbgCntMode fmDmaDbgCntMode)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaDbgCntMode = fmDmaDbgCntMode;

    return E_OK;
}

t_Error FM_ConfigDmaStopOnBusErr(t_Handle h_Fm, bool stop)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaStopOnBusError = stop;

    return E_OK;
}

t_Error FM_ConfigDmaSosEmergencyThreshold(t_Handle h_Fm, uint32_t dmaSosEmergency)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaSosEmergency = dmaSosEmergency;

    return E_OK;
}

t_Error FM_ConfigEnableCounters(t_Handle h_Fm)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->enCounters = TRUE;

    return E_OK;
}

t_Error FM_ConfigDmaErr(t_Handle h_Fm, e_FmDmaErr dmaErr)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->dmaErr = dmaErr;

    return E_OK;
}

t_Error FM_ConfigCatastrophicErr(t_Handle h_Fm, e_FmCatastrophicErr catastrophicErr)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->catastrophicErr = catastrophicErr;

    return E_OK;
}

t_Error FM_ConfigEnableMuramTestMode(t_Handle h_Fm)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->enMuramTestMode = TRUE;

    return E_OK;
}

t_Error FM_ConfigEnableIramTestMode(t_Handle h_Fm)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->enIramTestMode = TRUE;

    return E_OK;
}

t_Error FM_ConfigHaltOnExternalActivation(t_Handle h_Fm, bool enable)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

#ifdef FM_HALT_SIG_ERRATA_GEN12
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("HaltOnExternalActivation!"));
    }
#endif /* FM_HALT_SIG_ERRATA_GEN12 */

    p_Fm->p_FmDriverParam->haltOnExternalActivation = enable;

    return E_OK;
}

t_Error FM_ConfigHaltOnUnrecoverableEccError(t_Handle h_Fm, bool enable)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

#ifdef FM_ECC_HALT_NO_SYNC_ERRATA_10GMAC_A008
    RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("HaltOnEccError!"));
#endif /* FM_ECC_HALT_NO_SYNC_ERRATA_10GMAC_A008 */

    p_Fm->p_FmDriverParam->haltOnUnrecoverableEccError = enable;

    return E_OK;
}

t_Error FM_ConfigException(t_Handle h_Fm, e_FmExceptions exception, bool enable)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    uint32_t            bitMask = 0;
    t_FmRevisionInfo    revInfo;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    FM_GetRevision(p_Fm, &revInfo);
#ifdef FM_BMI_PIPELINE_ERR_IRQ_ERRATA_FMAN9
    if((exception == e_FM_EX_BMI_PIPELINE_ECC) && (enable))
    {
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        {
            REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_BMI_PIPELINE_ECC!"));
            return E_OK;
        }
    }
#endif /* FM_BMI_PIPELINE_ERR_IRQ_ERRATA_FMAN9 */
#ifdef FM_RAM_LIST_ERR_IRQ_ERRATA_FMAN8
    if((exception == e_FM_EX_BMI_LIST_RAM_ECC) && (enable))
    {
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        {
            REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_BMI_LIST_RAM_ECC!"));
            return E_OK;
        }
    }
#endif   /* FM_RAM_LIST_ERR_IRQ_ERRATA_FMAN8 */
#ifdef FM_QMI_NO_ECC_EXCEPTIONS
    if(((exception == e_FM_EX_QMI_SINGLE_ECC) || (exception == e_FM_EX_QMI_DOUBLE_ECC)) &&
            enable)
    {
        if (revInfo.majorRev == 4)
        {
            REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("QMI ECC exception!"));
            return E_OK;
        }
    }
#endif   /* FM_QMI_NO_ECC_EXCEPTIONS */
#ifdef FM_NO_DISPATCH_RAM_ECC
    if((exception == e_FM_EX_BMI_DISPATCH_RAM_ECC) && (enable))
    {
        if (revInfo.majorRev != 4)
        {
            REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_BMI_DISPATCH_RAM_ECC!"));
            return E_OK;
        }
    }
#endif   /* FM_NO_DISPATCH_RAM_ECC */

    GET_EXCEPTION_FLAG(bitMask, exception);
    if(bitMask)
    {
        if (enable)
            p_Fm->p_FmStateStruct->exceptions |= bitMask;
        else
            p_Fm->p_FmStateStruct->exceptions &= ~bitMask;
   }
    else
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));

    return E_OK;
}

t_Error FM_ConfigExternalEccRamsEnable(t_Handle h_Fm, bool enable)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    p_Fm->p_FmDriverParam->externalEccRamsEnable = enable;

    return E_OK;
}

t_Error FM_ConfigTnumAgingPeriod(t_Handle h_Fm, uint16_t tnumAgingPeriod)
{
    t_Fm             *p_Fm = (t_Fm*)h_Fm;
#ifdef FM_NO_TNUM_AGING
    t_FmRevisionInfo revInfo;
#endif /* FM_NO_TNUM_AGING */

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

#ifdef FM_NO_TNUM_AGING
    FM_GetRevision(h_Fm, &revInfo);
    if (revInfo.majorRev != 4)
        RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("FM_ConfigTnumAgingPeriod!"));
#endif /* FM_NO_TNUM_AGING */

    p_Fm->p_FmDriverParam->tnumAgingPeriod = tnumAgingPeriod;

    return E_OK;

}

/****************************************************/
/*       API Run-time Control uint functions        */
/****************************************************/
t_Handle FM_GetPcdHandle(t_Handle h_Fm)
{
    SANITY_CHECK_RETURN_VALUE(h_Fm, E_INVALID_HANDLE, NULL);
    SANITY_CHECK_RETURN_VALUE(!((t_Fm*)h_Fm)->p_FmDriverParam, E_INVALID_STATE, NULL);

    return ((t_Fm*)h_Fm)->h_Pcd;
}

void FM_EventIsr(t_Handle h_Fm)
{
#define FM_M_CALL_1G_MAC_TMR_ISR(_id)   \
    {                                   \
        if (p_Fm->guestId != p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_1G_MAC0_TMR+_id)].guestId)    \
            SendIpcIsr(p_Fm, (e_FmInterModuleEvent)(e_FM_EV_1G_MAC0_TMR+_id), pending);                 \
        else                                                                                            \
            p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_1G_MAC0_TMR+_id)].f_Isr(p_Fm->intrMng[(e_FmInterModuleEvent)(e_FM_EV_1G_MAC0_TMR+_id)].h_SrcHandle);\
    }
    t_Fm                    *p_Fm = (t_Fm*)h_Fm;
    uint32_t                pending, event;

    SANITY_CHECK_RETURN(h_Fm, E_INVALID_HANDLE);

    /* normal interrupts */
    pending = GET_UINT32(p_Fm->p_FmFpmRegs->fmnpi);
    ASSERT_COND(pending);
    if (pending & INTR_EN_BMI)
        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("BMI Event - undefined!"));
    if (pending & INTR_EN_QMI)
        QmiEvent(p_Fm);
    if (pending & INTR_EN_PRS)
        p_Fm->intrMng[e_FM_EV_PRS].f_Isr(p_Fm->intrMng[e_FM_EV_PRS].h_SrcHandle);
    if (pending & INTR_EN_PLCR)
        p_Fm->intrMng[e_FM_EV_PLCR].f_Isr(p_Fm->intrMng[e_FM_EV_PLCR].h_SrcHandle);
    if (pending & INTR_EN_KG)
        p_Fm->intrMng[e_FM_EV_KG].f_Isr(p_Fm->intrMng[e_FM_EV_KG].h_SrcHandle);
    if (pending & INTR_EN_TMR)
            p_Fm->intrMng[e_FM_EV_TMR].f_Isr(p_Fm->intrMng[e_FM_EV_TMR].h_SrcHandle);

    /* MAC events may belong to different partitions */
    if (pending & INTR_EN_1G_MAC0_TMR)
        FM_M_CALL_1G_MAC_TMR_ISR(0);
    if (pending & INTR_EN_1G_MAC1_TMR)
        FM_M_CALL_1G_MAC_TMR_ISR(1);
    if (pending & INTR_EN_1G_MAC2_TMR)
        FM_M_CALL_1G_MAC_TMR_ISR(2);
    if (pending & INTR_EN_1G_MAC3_TMR)
        FM_M_CALL_1G_MAC_TMR_ISR(3);
    if (pending & INTR_EN_1G_MAC4_TMR)
        FM_M_CALL_1G_MAC_TMR_ISR(4);

    /* IM port events may belong to different partitions */
    if (pending & INTR_EN_REV0)
    {
        event = GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcev[0]) & GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcee[0]);
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmcev[0], event);
        if (p_Fm->guestId != p_Fm->intrMng[e_FM_EV_FMAN_CTRL_0].guestId)
            /*TODO IPC ISR For Fman Ctrl */
            ASSERT_COND(0);
            /* SendIpcIsr(p_Fm, e_FM_EV_FMAN_CTRL_0, pending); */
        else
            p_Fm->fmanCtrlIntr[0].f_Isr(p_Fm->fmanCtrlIntr[0].h_SrcHandle, event);

    }
    if (pending & INTR_EN_REV1)
    {
        event = GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcev[1]) & GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcee[1]);
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmcev[1], event);
        if (p_Fm->guestId != p_Fm->intrMng[e_FM_EV_FMAN_CTRL_1].guestId)
            /*TODO IPC ISR For Fman Ctrl */
            ASSERT_COND(0);
            /* SendIpcIsr(p_Fm, e_FM_EV_FMAN_CTRL_1, pending); */
        else
            p_Fm->fmanCtrlIntr[1].f_Isr(p_Fm->fmanCtrlIntr[1].h_SrcHandle, event);

    }
    if (pending & INTR_EN_REV2)
    {
        event = GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcev[2]) & GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcee[2]);
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmcev[2], event);
        if (p_Fm->guestId != p_Fm->intrMng[e_FM_EV_FMAN_CTRL_2].guestId)
            /*TODO IPC ISR For Fman Ctrl */
            ASSERT_COND(0);
            /* SendIpcIsr(p_Fm, e_FM_EV_FMAN_CTRL_2, pending); */
        else
           p_Fm->fmanCtrlIntr[2].f_Isr(p_Fm->fmanCtrlIntr[2].h_SrcHandle, event);
    }
    if (pending & INTR_EN_REV3)
    {
        event = GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcev[3]) & GET_UINT32(p_Fm->p_FmFpmRegs->fmfpfcee[3]);
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmcev[3], event);
        if (p_Fm->guestId != p_Fm->intrMng[e_FM_EV_FMAN_CTRL_3].guestId)
            /*TODO IPC ISR For Fman Ctrl */
            ASSERT_COND(0);
            /* SendIpcIsr(p_Fm, e_FM_EV_FMAN_CTRL_2, pendin3); */
        else
            p_Fm->fmanCtrlIntr[3].f_Isr(p_Fm->fmanCtrlIntr[3].h_SrcHandle, event);
    }
}

t_Error FM_ErrorIsr(t_Handle h_Fm)
{
    t_Fm                    *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(h_Fm, E_INVALID_HANDLE);

    /* error interrupts */
    if (GET_UINT32(p_Fm->p_FmFpmRegs->fmepi) == 0)
        return ERROR_CODE(E_EMPTY);

    ErrorIsrCB(p_Fm);
    return E_OK;
}

t_Error FM_SetPortsBandwidth(t_Handle h_Fm, t_FmPortsBandwidthParams *p_PortsBandwidth)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;
    int         i;
    uint8_t     sum;
    uint8_t     hardwarePortId;
    uint32_t    tmpRegs[8] = {0,0,0,0,0,0,0,0};
    uint8_t     relativePortId, shift, weight, maxPercent = 0;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    /* check that all ports add up to 100% */
    sum = 0;
    for (i=0;i<p_PortsBandwidth->numOfPorts;i++)
        sum +=p_PortsBandwidth->portsBandwidths[i].bandwidth;
    if (sum != 100)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Sum of ports bandwidth differ from 100%"));

    /* find highest precent */
    for (i=0;i<p_PortsBandwidth->numOfPorts;i++)
    {
        if (p_PortsBandwidth->portsBandwidths[i].bandwidth > maxPercent)
            maxPercent = p_PortsBandwidth->portsBandwidths[i].bandwidth;
    }

    /* calculate weight for each port */
    for (i=0;i<p_PortsBandwidth->numOfPorts;i++)
    {
        weight = (uint8_t)((p_PortsBandwidth->portsBandwidths[i].bandwidth * PORT_MAX_WEIGHT )/maxPercent);
        /* we want even division between 1-to-PORT_MAX_WEIGHT. so if exect division
           is not reached, we round up so that:
           0 until maxPercent/PORT_MAX_WEIGHT get "1"
           maxPercent/PORT_MAX_WEIGHT+1 until (maxPercent/PORT_MAX_WEIGHT)*2 get "2"
           ...
           maxPercent - maxPercent/PORT_MAX_WEIGHT until maxPercent get "PORT_MAX_WEIGHT: */
        if ((uint8_t)((p_PortsBandwidth->portsBandwidths[i].bandwidth * PORT_MAX_WEIGHT ) % maxPercent))
            weight++;

        /* find the location of this port within the register */
        SW_PORT_ID_TO_HW_PORT_ID(hardwarePortId,
                                 p_PortsBandwidth->portsBandwidths[i].type,
                                 p_PortsBandwidth->portsBandwidths[i].relativePortId);
        relativePortId = (uint8_t)(hardwarePortId % 8);
        shift = (uint8_t)(32-4*(relativePortId+1));


        if(weight > 1)
            /* Add this port to tmpReg */
            /* (each 8 ports result in one register)*/
            tmpRegs[hardwarePortId/8] |= ((weight-1) << shift);
    }

    for(i=0;i<8;i++)
        if(tmpRegs[i])
            WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_arb[i], tmpRegs[i]);

    return E_OK;
}

t_Error FM_EnableRamsEcc(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;
    uint32_t    tmpReg;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        t_FmIpcMsg      msg;
        t_Error         err;

        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_ENABLE_RAM_ECC;
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        return E_OK;
    }

    if(!p_Fm->p_FmStateStruct->internalCall)
        p_Fm->p_FmStateStruct->explicitEnable = TRUE;
    p_Fm->p_FmStateStruct->internalCall = FALSE;

    if(p_Fm->p_FmStateStruct->ramsEccEnable)
        return E_OK;
    else
    {
        tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fmrcr);
        if(tmpReg & FPM_RAM_CTL_RAMS_ECC_EN_SRC_SEL)
        {
            DBG(WARNING, ("Rams ECC is configured to be controlled through JTAG"));
            WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, tmpReg | FPM_RAM_CTL_IRAM_ECC_EN);
        }
        else
            WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, tmpReg | (FPM_RAM_CTL_RAMS_ECC_EN | FPM_RAM_CTL_IRAM_ECC_EN));
        p_Fm->p_FmStateStruct->ramsEccEnable = TRUE;
    }

    return E_OK;
}

t_Error FM_DisableRamsEcc(t_Handle h_Fm)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;
    uint32_t    tmpReg;
    bool        explicitDisable = FALSE;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Fm->p_FmDriverParam, E_INVALID_HANDLE);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        t_Error             err;
        t_FmIpcMsg          msg;

        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_DISABLE_RAM_ECC;
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId),
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        return E_OK;
    }

    if(!p_Fm->p_FmStateStruct->internalCall)
        explicitDisable = TRUE;
    p_Fm->p_FmStateStruct->internalCall = FALSE;

    /* if rams are already disabled, or if rams were explicitly enabled and are
       currently called indirectly (not explicitly), ignore this call. */
    if(!p_Fm->p_FmStateStruct->ramsEccEnable || (p_Fm->p_FmStateStruct->explicitEnable && !explicitDisable))
        return E_OK;
    else
    {
        if(p_Fm->p_FmStateStruct->explicitEnable)
            /* This is the case were both explicit are TRUE.
               Turn off this flag for cases were following ramsEnable
               routines are called */
            p_Fm->p_FmStateStruct->explicitEnable = FALSE;

        tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fmrcr);
        if(tmpReg & FPM_RAM_CTL_RAMS_ECC_EN_SRC_SEL)
        {
            DBG(WARNING, ("Rams ECC is configured to be controlled through JTAG"));
            WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, tmpReg & ~FPM_RAM_CTL_IRAM_ECC_EN);
        }
        else
            WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrcr, tmpReg & ~(FPM_RAM_CTL_RAMS_ECC_EN | FPM_RAM_CTL_IRAM_ECC_EN));
        p_Fm->p_FmStateStruct->ramsEccEnable = FALSE;
    }

    return E_OK;
}

t_Error FM_SetException(t_Handle h_Fm, e_FmExceptions exception, bool enable)
{
    t_Fm        *p_Fm = (t_Fm*)h_Fm;
    uint32_t    bitMask = 0;
    uint32_t    tmpReg;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    GET_EXCEPTION_FLAG(bitMask, exception);
    if(bitMask)
    {
        if (enable)
            p_Fm->p_FmStateStruct->exceptions |= bitMask;
        else
            p_Fm->p_FmStateStruct->exceptions &= ~bitMask;

        switch(exception)
        {
             case(e_FM_EX_DMA_BUS_ERROR):
                tmpReg = GET_UINT32(p_Fm->p_FmDmaRegs->fmdmmr);
                if(enable)
                    tmpReg |= DMA_MODE_BER;
                else
                    tmpReg &= ~DMA_MODE_BER;
                /* disable bus error */
                WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmmr, tmpReg);
                break;
             case(e_FM_EX_DMA_READ_ECC):
             case(e_FM_EX_DMA_SYSTEM_WRITE_ECC):
             case(e_FM_EX_DMA_FM_WRITE_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmDmaRegs->fmdmmr);
                if(enable)
                    tmpReg |= DMA_MODE_ECC;
                else
                    tmpReg &= ~DMA_MODE_ECC;
                WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmmr, tmpReg);
                break;
             case(e_FM_EX_FPM_STALL_ON_TASKS):
                tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fpmem);
                if(enable)
                    tmpReg |= FPM_EV_MASK_STALL_EN;
                else
                    tmpReg &= ~FPM_EV_MASK_STALL_EN;
                WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmem, tmpReg);
                break;
             case(e_FM_EX_FPM_SINGLE_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fpmem);
                if(enable)
                    tmpReg |= FPM_EV_MASK_SINGLE_ECC_EN;
                else
                    tmpReg &= ~FPM_EV_MASK_SINGLE_ECC_EN;
                WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmem, tmpReg);
                break;
            case( e_FM_EX_FPM_DOUBLE_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fpmem);
                if(enable)
                    tmpReg |= FPM_EV_MASK_DOUBLE_ECC_EN;
                else
                    tmpReg &= ~FPM_EV_MASK_DOUBLE_ECC_EN;
                WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmem, tmpReg);
                break;
            case( e_FM_EX_QMI_SINGLE_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_ien);
                if(enable)
                {
#ifdef FM_QMI_NO_ECC_EXCEPTIONS
                    t_FmRevisionInfo revInfo;
                    FM_GetRevision(p_Fm, &revInfo);
                    if (revInfo.majorRev == 4)
                    {
                       REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_QMI_SINGLE_ECC"));
                       return E_OK;
                    }
#endif   /* FM_QMI_NO_ECC_EXCEPTIONS */
                    tmpReg |= QMI_INTR_EN_SINGLE_ECC;
                }
                else
                    tmpReg &= ~QMI_INTR_EN_SINGLE_ECC;
                WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_ien, tmpReg);
                break;
             case(e_FM_EX_QMI_DOUBLE_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_eien);
                if(enable)
                {
#ifdef FM_QMI_NO_ECC_EXCEPTIONS
                    t_FmRevisionInfo revInfo;
                    FM_GetRevision(p_Fm, &revInfo);
                    if (revInfo.majorRev == 4)
                    {
                       REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_QMI_DOUBLE_ECC"));
                       return E_OK;
                    }
#endif   /* FM_QMI_NO_ECC_EXCEPTIONS */
                    tmpReg |= QMI_ERR_INTR_EN_DOUBLE_ECC;
                }
                else
                    tmpReg &= ~QMI_ERR_INTR_EN_DOUBLE_ECC;
                WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eien, tmpReg);
                break;
             case(e_FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID):
                tmpReg = GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_eien);
                if(enable)
                    tmpReg |= QMI_ERR_INTR_EN_DEQ_FROM_DEF;
                else
                    tmpReg &= ~QMI_ERR_INTR_EN_DEQ_FROM_DEF;
                WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eien, tmpReg);
                break;
             case(e_FM_EX_BMI_LIST_RAM_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier);
                if(enable)
                {
#ifdef FM_RAM_LIST_ERR_IRQ_ERRATA_FMAN8
                    t_FmRevisionInfo revInfo;
                    FM_GetRevision(p_Fm, &revInfo);
                    if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                    {
                       REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_BMI_LIST_RAM_ECC"));
                       return E_OK;
                    }
#endif   /* FM_RAM_LIST_ERR_IRQ_ERRATA_FMAN8 */
                    tmpReg |= BMI_ERR_INTR_EN_LIST_RAM_ECC;
                }
                else
                    tmpReg &= ~BMI_ERR_INTR_EN_LIST_RAM_ECC;
                WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier, tmpReg);
                break;
             case(e_FM_EX_BMI_PIPELINE_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier);
                if(enable)
                {
#ifdef FM_BMI_PIPELINE_ERR_IRQ_ERRATA_FMAN9
                    t_FmRevisionInfo revInfo;
                    FM_GetRevision(p_Fm, &revInfo);
                    if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                    {
                       REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_BMI_PIPELINE_ECCBMI_LIST_RAM_ECC"));
                       return E_OK;
                    }
#endif /* FM_BMI_PIPELINE_ERR_IRQ_ERRATA_FMAN9 */
                    tmpReg |= BMI_ERR_INTR_EN_PIPELINE_ECC;
                }
                else
                    tmpReg &= ~BMI_ERR_INTR_EN_PIPELINE_ECC;
                WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier, tmpReg);
                break;
             case(e_FM_EX_BMI_STATISTICS_RAM_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier);
                if(enable)
                    tmpReg |= BMI_ERR_INTR_EN_STATISTICS_RAM_ECC;
                else
                    tmpReg &= ~BMI_ERR_INTR_EN_STATISTICS_RAM_ECC;
                WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier, tmpReg);
                break;
             case(e_FM_EX_BMI_DISPATCH_RAM_ECC):
               tmpReg = GET_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier);
               if(enable)
               {
#ifdef FM_NO_DISPATCH_RAM_ECC
                   t_FmRevisionInfo     revInfo;
                   FM_GetRevision(p_Fm, &revInfo);
                   if (revInfo.majorRev != 4)
                   {
                       REPORT_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_EX_BMI_DISPATCH_RAM_ECC"));
                       return E_OK;
                   }
#endif /* FM_NO_DISPATCH_RAM_ECC */
                   tmpReg |= BMI_ERR_INTR_EN_DISPATCH_RAM_ECC;
               }
               else
                   tmpReg &= ~BMI_ERR_INTR_EN_DISPATCH_RAM_ECC;
               WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ier, tmpReg);
               break;
             case(e_FM_EX_IRAM_ECC):
                 tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fmrie);
                if(enable)
                {
                    /* enable ECC if not enabled */
                    FmEnableRamsEcc(p_Fm);
                    /* enable ECC interrupts */
                    tmpReg |= FPM_IRAM_ECC_ERR_EX_EN;
                }
                else
                {
                    /* ECC mechanism may be disabled, depending on driver status  */
                    FmDisableRamsEcc(p_Fm);
                    tmpReg &= ~FPM_IRAM_ECC_ERR_EX_EN;
                }
                WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrie, tmpReg);
                break;

             case(e_FM_EX_MURAM_ECC):
                tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fmrie);
                if(enable)
                {
                    /* enable ECC if not enabled */
                    FmEnableRamsEcc(p_Fm);
                    /* enable ECC interrupts */
                    tmpReg |= FPM_MURAM_ECC_ERR_EX_EN;
                }
                else
                {
                    /* ECC mechanism may be disabled, depending on driver status  */
                    FmDisableRamsEcc(p_Fm);
                    tmpReg &= ~FPM_MURAM_ECC_ERR_EX_EN;
                }

                WRITE_UINT32(p_Fm->p_FmFpmRegs->fmrie, tmpReg);
                break;
            default:
                RETURN_ERROR(MINOR, E_INVALID_SELECTION, NO_MSG);
        }
    }
    else
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));

    return E_OK;
}

t_Error FM_GetRevision(t_Handle h_Fm, t_FmRevisionInfo *p_FmRevisionInfo)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    uint32_t            tmpReg;
    t_Error             err;
    t_FmIpcMsg          msg;
    t_FmIpcReply        reply;
    uint32_t            replyLength;
    t_FmIpcRevisionInfo ipcRevInfo;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);

    if (p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_GET_REV;
        replyLength = sizeof(uint32_t) + sizeof(t_FmRevisionInfo);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        if (replyLength != (sizeof(uint32_t) + sizeof(t_FmRevisionInfo)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        memcpy((uint8_t*)&ipcRevInfo, reply.replyBody, sizeof(t_FmRevisionInfo));
        p_FmRevisionInfo->majorRev = ipcRevInfo.majorRev;
        p_FmRevisionInfo->minorRev = ipcRevInfo.minorRev;
        return (t_Error)(reply.error);
    }

    /* read revision register 1 */
    tmpReg = GET_UINT32(p_Fm->p_FmFpmRegs->fm_ip_rev_1);
    p_FmRevisionInfo->majorRev = (uint8_t)((tmpReg & FPM_REV1_MAJOR_MASK) >> FPM_REV1_MAJOR_SHIFT);
    p_FmRevisionInfo->minorRev = (uint8_t)((tmpReg & FPM_REV1_MINOR_MASK) >> FPM_REV1_MINOR_SHIFT);

    return E_OK;
}

uint32_t FM_GetCounter(t_Handle h_Fm, e_FmCounters counter)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    t_Error             err;
    uint32_t            counterValue;
    t_FmIpcMsg          msg;
    t_FmIpcReply        reply;
    uint32_t            replyLength, outCounter;

    SANITY_CHECK_RETURN_VALUE(p_Fm, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_Fm->p_FmDriverParam, E_INVALID_STATE, 0);


    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_GET_COUNTER;
        memcpy(msg.msgBody, (uint8_t *)&counter, sizeof(uint32_t));
        replyLength = sizeof(uint32_t) + sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) +sizeof(counterValue),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
            RETURN_ERROR(MAJOR, err, NO_MSG);
        if(replyLength != (sizeof(uint32_t) + sizeof(uint32_t)))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        memcpy((uint8_t*)&outCounter, reply.replyBody, sizeof(uint32_t));

        return outCounter;
     }

    switch(counter)
    {
        case(e_FM_COUNTERS_ENQ_TOTAL_FRAME):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_etfc);
        case(e_FM_COUNTERS_DEQ_TOTAL_FRAME):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dtfc);
        case(e_FM_COUNTERS_DEQ_0):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc0);
        case(e_FM_COUNTERS_DEQ_1):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc1);
        case(e_FM_COUNTERS_DEQ_2):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc2);
        case(e_FM_COUNTERS_DEQ_3):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc3);
        case(e_FM_COUNTERS_DEQ_FROM_DEFAULT):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dfdc);
        case(e_FM_COUNTERS_DEQ_FROM_CONTEXT):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dfcc);
        case(e_FM_COUNTERS_DEQ_FROM_FD):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dffc);
        case(e_FM_COUNTERS_DEQ_CONFIRM):
            return GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_dcc);
        case(e_FM_COUNTERS_SEMAPHOR_ENTRY_FULL_REJECT):
            return GET_UINT32(p_Fm->p_FmDmaRegs->fmdmsefrc);
        case(e_FM_COUNTERS_SEMAPHOR_QUEUE_FULL_REJECT):
            return GET_UINT32(p_Fm->p_FmDmaRegs->fmdmsqfrc);
        case(e_FM_COUNTERS_SEMAPHOR_SYNC_REJECT):
            return GET_UINT32(p_Fm->p_FmDmaRegs->fmdmssrc);
        default:
            break;
    }
    /* should never get here */
    ASSERT_COND(FALSE);

    return 0;
}

t_Error  FM_ModifyCounter(t_Handle h_Fm, e_FmCounters counter, uint32_t val)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

   SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
   SANITY_CHECK_RETURN_ERROR(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    /* When applicable (when there is an 'enable counters' bit,
    check that counters are enabled */
    switch(counter)
    {
        case(e_FM_COUNTERS_ENQ_TOTAL_FRAME):
        case(e_FM_COUNTERS_DEQ_TOTAL_FRAME):
        case(e_FM_COUNTERS_DEQ_0):
        case(e_FM_COUNTERS_DEQ_1):
        case(e_FM_COUNTERS_DEQ_2):
        case(e_FM_COUNTERS_DEQ_3):
        case(e_FM_COUNTERS_DEQ_FROM_DEFAULT):
        case(e_FM_COUNTERS_DEQ_FROM_CONTEXT):
        case(e_FM_COUNTERS_DEQ_FROM_FD):
        case(e_FM_COUNTERS_DEQ_CONFIRM):
            if(!(GET_UINT32(p_Fm->p_FmQmiRegs->fmqm_gc) & QMI_CFG_EN_COUNTERS))
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter was not enabled"));
            break;
        default:
            break;
    }

    /* Set counter */
    switch(counter)
    {
        case(e_FM_COUNTERS_ENQ_TOTAL_FRAME):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_etfc, val);
            break;
        case(e_FM_COUNTERS_DEQ_TOTAL_FRAME):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dtfc, val);
            break;
        case(e_FM_COUNTERS_DEQ_0):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc0, val);
            break;
        case(e_FM_COUNTERS_DEQ_1):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc1, val);
            break;
        case(e_FM_COUNTERS_DEQ_2):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc2, val);
            break;
        case(e_FM_COUNTERS_DEQ_3):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dc3, val);
            break;
        case(e_FM_COUNTERS_DEQ_FROM_DEFAULT):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dfdc, val);
            break;
        case(e_FM_COUNTERS_DEQ_FROM_CONTEXT):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dfcc, val);
            break;
        case(e_FM_COUNTERS_DEQ_FROM_FD):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dffc, val);
            break;
        case(e_FM_COUNTERS_DEQ_CONFIRM):
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_dcc, val);
            break;
        case(e_FM_COUNTERS_SEMAPHOR_ENTRY_FULL_REJECT):
            WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmsefrc, val);
            break;
        case(e_FM_COUNTERS_SEMAPHOR_QUEUE_FULL_REJECT):
            WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmsqfrc, val);
            break;
        case(e_FM_COUNTERS_SEMAPHOR_SYNC_REJECT):
            WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmssrc, val);
            break;
        default:
            break;
    }

    return E_OK;
}

void FM_SetDmaEmergency(t_Handle h_Fm, e_FmDmaMuramPort muramPort, bool enable)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;
    uint32_t    bitMask;

    SANITY_CHECK_RETURN(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    bitMask = (uint32_t)((muramPort==e_FM_DMA_MURAM_PORT_WRITE) ? DMA_MODE_EMERGENCY_WRITE : DMA_MODE_EMERGENCY_READ);

    if(enable)
        WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmmr, GET_UINT32(p_Fm->p_FmDmaRegs->fmdmmr) | bitMask);
    else /* disable */
        WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmmr, GET_UINT32(p_Fm->p_FmDmaRegs->fmdmmr) & ~bitMask);

    return;
}

void FM_SetDmaExtBusPri(t_Handle h_Fm, e_FmDmaExtBusPri pri)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    WRITE_UINT32(p_Fm->p_FmDmaRegs->fmdmmr, GET_UINT32(p_Fm->p_FmDmaRegs->fmdmmr) | ((uint32_t)pri << DMA_MODE_BUS_PRI_SHIFT) );

    return;
}

void FM_GetDmaStatus(t_Handle h_Fm, t_FmDmaStatus *p_FmDmaStatus)
{
    t_Fm                *p_Fm = (t_Fm*)h_Fm;
    uint32_t            tmpReg;
    t_Error             err;
    t_FmIpcMsg          msg;
    t_FmIpcReply        reply;
    uint32_t            replyLength;
    t_FmIpcDmaStatus    ipcDmaStatus;

    SANITY_CHECK_RETURN(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        memset(&reply, 0, sizeof(reply));
        msg.msgId = FM_DMA_STAT;
        replyLength = sizeof(uint32_t) + sizeof(t_FmIpcDmaStatus);
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
        {
            REPORT_ERROR(MINOR, err, NO_MSG);
            return;
        }
        if (replyLength != (sizeof(uint32_t) + sizeof(t_FmIpcDmaStatus)))
        {
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
            return;
        }
        memcpy((uint8_t*)&ipcDmaStatus, reply.replyBody, sizeof(t_FmIpcDmaStatus));

        p_FmDmaStatus->cmqNotEmpty = (bool)ipcDmaStatus.boolCmqNotEmpty;            /**< Command queue is not empty */
        p_FmDmaStatus->busError = (bool)ipcDmaStatus.boolBusError;                  /**< Bus error occurred */
        p_FmDmaStatus->readBufEccError = (bool)ipcDmaStatus.boolReadBufEccError;        /**< Double ECC error on buffer Read */
        p_FmDmaStatus->writeBufEccSysError =(bool)ipcDmaStatus.boolWriteBufEccSysError;    /**< Double ECC error on buffer write from system side */
        p_FmDmaStatus->writeBufEccFmError = (bool)ipcDmaStatus.boolWriteBufEccFmError;     /**< Double ECC error on buffer write from FM side */
        return;
    }

    tmpReg = GET_UINT32(p_Fm->p_FmDmaRegs->fmdmsr);

    p_FmDmaStatus->cmqNotEmpty = (bool)(tmpReg & DMA_STATUS_CMD_QUEUE_NOT_EMPTY);
    p_FmDmaStatus->busError = (bool)(tmpReg & DMA_STATUS_BUS_ERR);
    p_FmDmaStatus->readBufEccError = (bool)(tmpReg & DMA_STATUS_READ_ECC);
    p_FmDmaStatus->writeBufEccSysError = (bool)(tmpReg & DMA_STATUS_SYSTEM_WRITE_ECC);
    p_FmDmaStatus->writeBufEccFmError = (bool)(tmpReg & DMA_STATUS_FM_WRITE_ECC);
    return;
}

t_Error FM_ForceIntr (t_Handle h_Fm, e_FmExceptions exception)
{
    t_Fm *p_Fm = (t_Fm*)h_Fm;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    switch(exception)
    {
        case e_FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID:
            if (!(p_Fm->p_FmStateStruct->exceptions & FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception is masked"));
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eif, QMI_ERR_INTR_EN_DEQ_FROM_DEF);
            break;
        case e_FM_EX_QMI_SINGLE_ECC:
            if (!(p_Fm->p_FmStateStruct->exceptions & FM_EX_QMI_SINGLE_ECC))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception is masked"));
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_if, QMI_INTR_EN_SINGLE_ECC);
            break;
        case e_FM_EX_QMI_DOUBLE_ECC:
            if (!(p_Fm->p_FmStateStruct->exceptions & FM_EX_QMI_DOUBLE_ECC))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception is masked"));
            WRITE_UINT32(p_Fm->p_FmQmiRegs->fmqm_eif, QMI_ERR_INTR_EN_DOUBLE_ECC);
            break;
        case e_FM_EX_BMI_LIST_RAM_ECC:
            if (!(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_LIST_RAM_ECC))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception is masked"));
            WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ifr, BMI_ERR_INTR_EN_LIST_RAM_ECC);
            break;
        case e_FM_EX_BMI_PIPELINE_ECC:
            if (!(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_PIPELINE_ECC))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception is masked"));
            WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ifr, BMI_ERR_INTR_EN_PIPELINE_ECC);
            break;
        case e_FM_EX_BMI_STATISTICS_RAM_ECC:
            if (!(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_STATISTICS_RAM_ECC))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception is masked"));
            WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ifr, BMI_ERR_INTR_EN_STATISTICS_RAM_ECC);
            break;
        case e_FM_EX_BMI_DISPATCH_RAM_ECC:
            if (!(p_Fm->p_FmStateStruct->exceptions & FM_EX_BMI_DISPATCH_RAM_ECC))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception is masked"));
            WRITE_UINT32(p_Fm->p_FmBmiRegs->fmbm_ifr, BMI_ERR_INTR_EN_DISPATCH_RAM_ECC);
            break;
        default:
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("The selected exception may not be forced"));
    }

    return E_OK;
}

void FM_Resume(t_Handle h_Fm)
{
    t_Fm            *p_Fm = (t_Fm*)h_Fm;
    uint32_t        tmpReg;

    SANITY_CHECK_RETURN(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(!p_Fm->p_FmDriverParam, E_INVALID_STATE);

    if (p_Fm->guestId == NCSW_MASTER_ID)
    {
        tmpReg  = GET_UINT32(p_Fm->p_FmFpmRegs->fpmem);
        /* clear tmpReg event bits in order not to clear standing events */
        tmpReg &= ~(FPM_EV_MASK_DOUBLE_ECC | FPM_EV_MASK_STALL | FPM_EV_MASK_SINGLE_ECC);
        WRITE_UINT32(p_Fm->p_FmFpmRegs->fpmem, tmpReg | FPM_EV_MASK_RELEASE_FM);
    }
    else
        ASSERT_COND(0); /* TODO */
}

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FM_DumpRegs(t_Handle h_Fm)
{
    t_Fm            *p_Fm = (t_Fm *)h_Fm;
    uint8_t         i = 0;
    t_Error         err;
    t_FmIpcMsg      msg;

    DECLARE_DUMP;

    SANITY_CHECK_RETURN_ERROR(p_Fm, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Fm->p_FmDriverParam, E_INVALID_STATE);


    if(p_Fm->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_DUMP_REGS;
        if ((err = XX_IpcSendMessage(p_Fm->h_IpcSessions[0],
                                    (uint8_t*)&msg,
                                    sizeof(msg.msgId),
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL)) != E_OK)
            RETURN_ERROR(MINOR, err, NO_MSG);
        return E_OK;
    }


    DUMP_SUBTITLE(("\n"));

    DUMP_TITLE(p_Fm->p_FmFpmRegs, ("FmFpmRegs Regs"));

    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmtnc);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmpr);
    DUMP_VAR(p_Fm->p_FmFpmRegs,brkc);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmflc);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmdis1);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmdis2);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fmepi);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fmrie);

    DUMP_TITLE(&p_Fm->p_FmFpmRegs->fmfpfcev, ("fmfpfcev"));
    DUMP_SUBSTRUCT_ARRAY(i, 4)
    {
        DUMP_MEMORY(&p_Fm->p_FmFpmRegs->fmfpfcev[i], sizeof(uint32_t));
    }

    DUMP_TITLE(&p_Fm->p_FmFpmRegs->fmfpfcee, ("fmfpfcee"));
    DUMP_SUBSTRUCT_ARRAY(i, 4)
    {
        DUMP_MEMORY(&p_Fm->p_FmFpmRegs->fmfpfcee[i], sizeof(uint32_t));
    }

    DUMP_SUBTITLE(("\n"));
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmtsc1);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmtsc2);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmtsp);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmtsf);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fmrcr);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmextc);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmext1);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmext2);

    DUMP_TITLE(&p_Fm->p_FmFpmRegs->fpmdrd, ("fpmdrd"));
    DUMP_SUBSTRUCT_ARRAY(i, 16)
    {
        DUMP_MEMORY(&p_Fm->p_FmFpmRegs->fpmdrd[i], sizeof(uint32_t));
    }

    DUMP_SUBTITLE(("\n"));
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmdra);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fm_ip_rev_1);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fm_ip_rev_2);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fmrstc);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fmcld);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fmnpi);
    DUMP_VAR(p_Fm->p_FmFpmRegs,fpmem);

    DUMP_TITLE(&p_Fm->p_FmFpmRegs->fpmcev, ("fpmcev"));
    DUMP_SUBSTRUCT_ARRAY(i, 4)
    {
        DUMP_MEMORY(&p_Fm->p_FmFpmRegs->fpmcev[i], sizeof(uint32_t));
    }

    DUMP_TITLE(&p_Fm->p_FmFpmRegs->fmfp_ps, ("fmfp_ps"));
    DUMP_SUBSTRUCT_ARRAY(i, 64)
    {
        DUMP_MEMORY(&p_Fm->p_FmFpmRegs->fmfp_ps[i], sizeof(uint32_t));
    }


    DUMP_TITLE(p_Fm->p_FmDmaRegs, ("p_FmDmaRegs Regs"));
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmsr);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmmr);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmtr);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmhy);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmsetr);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmtah);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmtal);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmtcid);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmra);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmrd);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmwcr);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmebcr);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmccqdr);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmccqvr1);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmccqvr2);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmcqvr3);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmcqvr4);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmcqvr5);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmsefrc);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmsqfrc);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmssrc);
    DUMP_VAR(p_Fm->p_FmDmaRegs,fmdmdcr);

    DUMP_TITLE(&p_Fm->p_FmDmaRegs->fmdmplr, ("fmdmplr"));

    DUMP_SUBSTRUCT_ARRAY(i, FM_SIZE_OF_LIODN_TABLE/2)
    {
        DUMP_MEMORY(&p_Fm->p_FmDmaRegs->fmdmplr[i], sizeof(uint32_t));
    }

    DUMP_TITLE(p_Fm->p_FmBmiRegs, ("p_FmBmiRegs COMMON Regs"));
    DUMP_VAR(p_Fm->p_FmBmiRegs,fmbm_init);
    DUMP_VAR(p_Fm->p_FmBmiRegs,fmbm_cfg1);
    DUMP_VAR(p_Fm->p_FmBmiRegs,fmbm_cfg2);
    DUMP_VAR(p_Fm->p_FmBmiRegs,fmbm_ievr);
    DUMP_VAR(p_Fm->p_FmBmiRegs,fmbm_ier);

    DUMP_TITLE(&p_Fm->p_FmBmiRegs->fmbm_arb, ("fmbm_arb"));
    DUMP_SUBSTRUCT_ARRAY(i, 8)
    {
        DUMP_MEMORY(&p_Fm->p_FmBmiRegs->fmbm_arb[i], sizeof(uint32_t));
    }


    DUMP_TITLE(p_Fm->p_FmQmiRegs, ("p_FmQmiRegs COMMON Regs"));
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_gc);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_eie);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_eien);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_eif);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_ie);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_ien);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_if);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_gs);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_ts);
    DUMP_VAR(p_Fm->p_FmQmiRegs,fmqm_etfc);

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && ... */

