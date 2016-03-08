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
 @File          fm_rtc.c

 @Description   FM RTC driver implementation.

 @Cautions      None
*//***************************************************************************/

#include "error_ext.h"
#include "debug_ext.h"
#include "string_ext.h"
#include "part_ext.h"
#include "xx_ext.h"
#include "ncsw_ext.h"

#include "fm_rtc.h"
#include "fm_common.h"


/*****************************************************************************/
static void SetDefaultParam(t_FmRtc *p_Rtc)
{
    t_FmRtcDriverParam  *p_RtcDriverParam = p_Rtc->p_RtcDriverParam;
    int                 i;

    p_Rtc->outputClockDivisor = DEFAULT_outputClockDivisor;
    p_Rtc->p_RtcDriverParam->bypass = DEFAULT_bypass;
    p_RtcDriverParam->srcClk = DEFAULT_srcClock;
    p_RtcDriverParam->invertInputClkPhase = DEFAULT_invertInputClkPhase;
    p_RtcDriverParam->invertOutputClkPhase = DEFAULT_invertOutputClkPhase;
    p_RtcDriverParam->pulseRealign = DEFAULT_pulseRealign;
    for (i=0; i < FM_RTC_NUM_OF_ALARMS; i++)
    {
        p_RtcDriverParam->alarmPolarity[i] = DEFAULT_alarmPolarity;
    }
    for (i=0; i < FM_RTC_NUM_OF_EXT_TRIGGERS; i++)
    {
        p_RtcDriverParam->triggerPolarity[i] = DEFAULT_triggerPolarity;
    }
    p_Rtc->clockPeriodNanoSec = DEFAULT_clockPeriod; /* 1 usec */
}

/*****************************************************************************/
static t_Error CheckInitParameters(t_FmRtc *p_Rtc)
{
    t_FmRtcDriverParam  *p_RtcDriverParam = p_Rtc->p_RtcDriverParam;
    int                 i;

    if ((p_RtcDriverParam->srcClk != e_FM_RTC_SOURCE_CLOCK_EXTERNAL) &&
        (p_RtcDriverParam->srcClk != e_FM_RTC_SOURCE_CLOCK_SYSTEM) &&
        (p_RtcDriverParam->srcClk != e_FM_RTC_SOURCE_CLOCK_OSCILATOR))
        RETURN_ERROR(MAJOR, E_INVALID_CLOCK, ("Source clock undefined"));

    if (p_Rtc->outputClockDivisor == 0)
    {
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                     ("Divisor for output clock (should be positive)"));
    }

    for (i=0; i < FM_RTC_NUM_OF_ALARMS; i++)
    {
        if ((p_RtcDriverParam->alarmPolarity[i] != e_FM_RTC_ALARM_POLARITY_ACTIVE_LOW) &&
            (p_RtcDriverParam->alarmPolarity[i] != e_FM_RTC_ALARM_POLARITY_ACTIVE_HIGH))
        {
            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Alarm %d signal polarity", i));
        }
    }
    for (i=0; i < FM_RTC_NUM_OF_EXT_TRIGGERS; i++)
    {
        if ((p_RtcDriverParam->triggerPolarity[i] != e_FM_RTC_TRIGGER_ON_FALLING_EDGE) &&
            (p_RtcDriverParam->triggerPolarity[i] != e_FM_RTC_TRIGGER_ON_RISING_EDGE))
        {
            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Trigger %d signal polarity", i));
        }
    }

#ifdef FM_1588_SRC_CLK_ERRATA_FMAN1
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Rtc->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0)&&
           ((p_RtcDriverParam->srcClk==e_FM_RTC_SOURCE_CLOCK_SYSTEM) && p_RtcDriverParam->invertInputClkPhase))
            RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Can not use invertInputClkPhase when source clock is e_FM_RTC_SOURCE_CLOCK_SYSTEM"));
    }
#endif /* FM_1588_SRC_CLK_ERRATA_FMAN1 */

    return E_OK;
}

/*****************************************************************************/
static void RtcExceptions(t_Handle h_FmRtc)
{
    t_FmRtc             *p_Rtc = (t_FmRtc *)h_FmRtc;
    t_FmRtcMemMap       *p_MemMap;
    register uint32_t   events;

    ASSERT_COND(p_Rtc);
    p_MemMap = p_Rtc->p_MemMap;

    /* Get valid events */
    events =  GET_UINT32(p_MemMap->tmr_tevent);
    events &= GET_UINT32(p_MemMap->tmr_temask);

    /* Clear event bits */
    WRITE_UINT32(p_MemMap->tmr_tevent, events);

    if (events & TMR_TEVENT_ALM1)
    {
        if(p_Rtc->alarmParams[0].clearOnExpiration)
        {
            WRITE_UINT32(p_MemMap->tmr_alarm[0].tmr_alarm_l, 0);
            WRITE_UINT32(p_MemMap->tmr_temask, GET_UINT32(p_MemMap->tmr_temask) & ~TMR_TEVENT_ALM1);
        }
        ASSERT_COND(p_Rtc->alarmParams[0].f_AlarmCallback);
        p_Rtc->alarmParams[0].f_AlarmCallback(p_Rtc->h_App, 0);
    }
    if (events & TMR_TEVENT_ALM2)
    {
        if(p_Rtc->alarmParams[1].clearOnExpiration)
        {
            WRITE_UINT32(p_MemMap->tmr_alarm[1].tmr_alarm_l, 0);
            WRITE_UINT32(p_MemMap->tmr_temask, GET_UINT32(p_MemMap->tmr_temask) & ~TMR_TEVENT_ALM2);
        }
        ASSERT_COND(p_Rtc->alarmParams[1].f_AlarmCallback);
        p_Rtc->alarmParams[1].f_AlarmCallback(p_Rtc->h_App, 1);
    }
    if (events & TMR_TEVENT_PP1)
    {
        ASSERT_COND(p_Rtc->periodicPulseParams[0].f_PeriodicPulseCallback);
        p_Rtc->periodicPulseParams[0].f_PeriodicPulseCallback(p_Rtc->h_App, 0);
    }
    if (events & TMR_TEVENT_PP2)
    {
        ASSERT_COND(p_Rtc->periodicPulseParams[1].f_PeriodicPulseCallback);
        p_Rtc->periodicPulseParams[1].f_PeriodicPulseCallback(p_Rtc->h_App, 1);
    }
    if (events & TMR_TEVENT_ETS1)
    {
        ASSERT_COND(p_Rtc->externalTriggerParams[0].f_ExternalTriggerCallback);
        p_Rtc->externalTriggerParams[0].f_ExternalTriggerCallback(p_Rtc->h_App, 0);
    }
    if (events & TMR_TEVENT_ETS2)
    {
        ASSERT_COND(p_Rtc->externalTriggerParams[1].f_ExternalTriggerCallback);
        p_Rtc->externalTriggerParams[1].f_ExternalTriggerCallback(p_Rtc->h_App, 1);
    }
}


/*****************************************************************************/
t_Handle FM_RTC_Config(t_FmRtcParams *p_FmRtcParam)
{
    t_FmRtc *p_Rtc;

    SANITY_CHECK_RETURN_VALUE(p_FmRtcParam, E_NULL_POINTER, NULL);

    /* Allocate memory for the FM RTC driver parameters */
    p_Rtc = (t_FmRtc *)XX_Malloc(sizeof(t_FmRtc));
    if (!p_Rtc)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM RTC driver structure"));
        return NULL;
    }

    memset(p_Rtc, 0, sizeof(t_FmRtc));

    /* Allocate memory for the FM RTC driver parameters */
    p_Rtc->p_RtcDriverParam = (t_FmRtcDriverParam *)XX_Malloc(sizeof(t_FmRtcDriverParam));
    if (!p_Rtc->p_RtcDriverParam)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM RTC driver parameters"));
        XX_Free(p_Rtc);
        return NULL;
    }

    memset(p_Rtc->p_RtcDriverParam, 0, sizeof(t_FmRtcDriverParam));

    /* Store RTC configuration parameters */
    p_Rtc->h_Fm = p_FmRtcParam->h_Fm;

    /* Set default RTC configuration parameters */
    SetDefaultParam(p_Rtc);

    /* Store RTC parameters in the RTC control structure */
    p_Rtc->p_MemMap = (t_FmRtcMemMap *)UINT_TO_PTR(p_FmRtcParam->baseAddress);
    p_Rtc->h_App    = p_FmRtcParam->h_App;

    return p_Rtc;
}

/*****************************************************************************/
t_Error FM_RTC_Init(t_Handle h_FmRtc)
{
    t_FmRtc             *p_Rtc = (t_FmRtc *)h_FmRtc;
    t_FmRtcDriverParam  *p_RtcDriverParam;
    t_FmRtcMemMap       *p_MemMap;
    uint32_t            freqCompensation;
    uint32_t            tmrCtrl;
    int                 i;
    uint64_t            tmpDouble;

    p_RtcDriverParam = p_Rtc->p_RtcDriverParam;
    p_MemMap = p_Rtc->p_MemMap;

    if(CheckInitParameters(p_Rtc)!=E_OK)
        RETURN_ERROR(MAJOR, E_CONFLICT,
                     ("Init Parameters are not Valid"));

    /* TODO A check must be added here, that no timestamping MAC's
     * are working in this stage. */
    WRITE_UINT32(p_MemMap->tmr_ctrl, TMR_CTRL_TMSR);
    XX_UDelay(10);
    WRITE_UINT32(p_MemMap->tmr_ctrl, 0);

    /* Set the source clock */
    switch (p_RtcDriverParam->srcClk)
    {
        case e_FM_RTC_SOURCE_CLOCK_SYSTEM:
            tmrCtrl = TMR_CTRL_CKSEL_MAC_CLK;
            break;
        case e_FM_RTC_SOURCE_CLOCK_OSCILATOR:
            tmrCtrl = TMR_CTRL_CKSEL_OSC_CLK;
            break;
        default:
            /* Use a clock from the External TMR reference clock.*/
            tmrCtrl = TMR_CTRL_CKSEL_EXT_CLK;
            break;
    }

    /* whatever period the user picked, the timestamp will advance in '1' every time
     * the period passed. */
    tmrCtrl |= ((1 << TMR_CTRL_TCLK_PERIOD_SHIFT) & TMR_CTRL_TCLK_PERIOD_MASK);

    if (p_RtcDriverParam->invertInputClkPhase)
        tmrCtrl |= TMR_CTRL_CIPH;
    if (p_RtcDriverParam->invertOutputClkPhase)
        tmrCtrl |= TMR_CTRL_COPH;

    for (i=0; i < FM_RTC_NUM_OF_ALARMS; i++)
    {
        if (p_RtcDriverParam->alarmPolarity[i] == e_FM_RTC_ALARM_POLARITY_ACTIVE_LOW)
            tmrCtrl |= (TMR_CTRL_ALMP1 >> i);
    }

    for (i=0; i < FM_RTC_NUM_OF_EXT_TRIGGERS; i++)
        if (p_RtcDriverParam->triggerPolarity[i] == e_FM_RTC_TRIGGER_ON_FALLING_EDGE)
            tmrCtrl |= (TMR_CTRL_ETEP1 << i);

    if (!p_RtcDriverParam->timerSlaveMode && p_Rtc->p_RtcDriverParam->bypass)
        tmrCtrl |= TMR_CTRL_BYP;

    WRITE_UINT32(p_MemMap->tmr_ctrl, tmrCtrl);

     for (i=0; i < FM_RTC_NUM_OF_ALARMS; i++)
    {
        /* Clear TMR_ALARM registers */
        WRITE_UINT32(p_MemMap->tmr_alarm[i].tmr_alarm_l, 0xFFFFFFFF);
        WRITE_UINT32(p_MemMap->tmr_alarm[i].tmr_alarm_h, 0xFFFFFFFF);
    }

    /* Clear TMR_TEVENT */
    WRITE_UINT32(p_MemMap->tmr_tevent, TMR_TEVENT_ALL);

    /* Initialize TMR_TEMASK */
    WRITE_UINT32(p_MemMap->tmr_temask, 0);


    /* find source clock frequency in Mhz */
    if (p_Rtc->p_RtcDriverParam->srcClk != e_FM_RTC_SOURCE_CLOCK_SYSTEM)
         p_Rtc->srcClkFreqMhz = p_Rtc->p_RtcDriverParam->extSrcClkFreq;
    else
        p_Rtc->srcClkFreqMhz = (uint32_t)(FmGetClockFreq(p_Rtc->h_Fm)/2);

    /* if timer in Master mode Initialize TMR_CTRL */
    /* We want the counter (TMR_CNT) to count in nano-seconds */
    if (!p_RtcDriverParam->timerSlaveMode && p_Rtc->p_RtcDriverParam->bypass)
    {
        p_Rtc->clockPeriodNanoSec = (1000 / p_Rtc->srcClkFreqMhz);
    }
    else
    {
        /* Initialize TMR_ADD with the initial frequency compensation value:
           freqCompensation = (2^32 / frequency ratio) */
        /* frequency ratio = sorce clock/rtc clock =
         * (p_Rtc->srcClkFreqMhz*1000000))/ 1/(p_Rtc->clockPeriodNanoSec * 1000000000) */
        freqCompensation = (uint32_t)DIV_CEIL(ACCUMULATOR_OVERFLOW * 1000,
                                    p_Rtc->clockPeriodNanoSec * p_Rtc->srcClkFreqMhz);
        WRITE_UINT32(p_MemMap->tmr_add, freqCompensation);
    }
    /* check the legality of the relation between source and destination clocks */
    /* should be larger than 1.0001 */
    tmpDouble = 10000 * (uint64_t)p_Rtc->clockPeriodNanoSec * (uint64_t)p_Rtc->srcClkFreqMhz;
    if((tmpDouble) <= 10001)
        RETURN_ERROR(MAJOR, E_CONFLICT,
              ("Invalid relation between source and destination clocks. Should be larger than 1.0001"));


    for (i=0; i < 2; i++)
        /* Clear TMR_FIPER registers */
        WRITE_UINT32(p_MemMap->tmr_fiper[i], 0xFFFFFFFF);

    /* Initialize TMR_PRSC */
    WRITE_UINT32(p_MemMap->tmr_prsc, p_Rtc->outputClockDivisor);

    /* Clear TMR_OFF */
    WRITE_UINT32(p_MemMap->tmr_off_l, 0);
    WRITE_UINT32(p_MemMap->tmr_off_h, 0);

    /* Register the FM RTC interrupt */
    FmRegisterIntr(p_Rtc->h_Fm, e_FM_MOD_TMR, 0, e_FM_INTR_TYPE_NORMAL, RtcExceptions , p_Rtc);

    /* Free parameters structures */
    XX_Free(p_Rtc->p_RtcDriverParam);
    p_Rtc->p_RtcDriverParam = NULL;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_Free(t_Handle h_FmRtc)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);

    if (p_Rtc->p_RtcDriverParam)
    {
        XX_Free(p_Rtc->p_RtcDriverParam);
    }
    else
    {
        FM_RTC_Disable(h_FmRtc);
    }

    /* Unregister FM RTC interrupt */
    FmUnregisterIntr(p_Rtc->h_Fm, e_FM_MOD_TMR, 0, e_FM_INTR_TYPE_NORMAL);
    XX_Free(p_Rtc);

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigSourceClock(t_Handle         h_FmRtc,
                                    e_FmSrcClk    srcClk,
                                    uint32_t      freqInMhz)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_Rtc->p_RtcDriverParam->srcClk = srcClk;
    if(srcClk != e_FM_RTC_SOURCE_CLOCK_SYSTEM)
        p_Rtc->p_RtcDriverParam->extSrcClkFreq = freqInMhz;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigPeriod(t_Handle h_FmRtc, uint32_t period)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_Rtc->clockPeriodNanoSec = period;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigFrequencyBypass(t_Handle h_FmRtc, bool enabled)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_Rtc->p_RtcDriverParam->bypass = enabled;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigInvertedInputClockPhase(t_Handle h_FmRtc, bool inverted)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_Rtc->p_RtcDriverParam->invertInputClkPhase = inverted;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigInvertedOutputClockPhase(t_Handle h_FmRtc, bool inverted)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_Rtc->p_RtcDriverParam->invertOutputClkPhase = inverted;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigOutputClockDivisor(t_Handle h_FmRtc, uint16_t divisor)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_Rtc->outputClockDivisor = divisor;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigPulseRealignment(t_Handle h_FmRtc, bool enable)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_Rtc->p_RtcDriverParam->pulseRealign = enable;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigAlarmPolarity(t_Handle             h_FmRtc,
                                   uint8_t              alarmId,
                                   e_FmRtcAlarmPolarity alarmPolarity)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    if (alarmId >= FM_RTC_NUM_OF_ALARMS)
    {
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Alarm ID"));
    }

    p_Rtc->p_RtcDriverParam->alarmPolarity[alarmId] = alarmPolarity;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ConfigExternalTriggerPolarity(t_Handle               h_FmRtc,
                                             uint8_t                triggerId,
                                             e_FmRtcTriggerPolarity triggerPolarity)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    if (triggerId >= FM_RTC_NUM_OF_EXT_TRIGGERS)
    {
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("External trigger ID"));
    }

    p_Rtc->p_RtcDriverParam->triggerPolarity[triggerId] = triggerPolarity;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_Enable(t_Handle h_FmRtc, bool resetClock)
{
    t_FmRtc         *p_Rtc = (t_FmRtc *)h_FmRtc;
    uint32_t        tmrCtrl;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    tmrCtrl = GET_UINT32(p_Rtc->p_MemMap->tmr_ctrl);

    /* TODO A check must be added here, that no timestamping MAC's
     * are working in this stage. */
    if (resetClock)
    {
        WRITE_UINT32(p_Rtc->p_MemMap->tmr_ctrl, (tmrCtrl | TMR_CTRL_TMSR));

        XX_UDelay(10);
        /* Clear TMR_OFF */
        WRITE_UINT32(p_Rtc->p_MemMap->tmr_off_l, 0);
        WRITE_UINT32(p_Rtc->p_MemMap->tmr_off_h, 0);
    }

    WRITE_UINT32(p_Rtc->p_MemMap->tmr_ctrl, (tmrCtrl | TMR_CTRL_TE));

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_Disable(t_Handle h_FmRtc)
{
    t_FmRtc         *p_Rtc = (t_FmRtc *)h_FmRtc;
    uint32_t        tmrCtrl;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    /* TODO A check must be added here, that no timestamping MAC's
     * are working in this stage. */
    tmrCtrl = GET_UINT32(p_Rtc->p_MemMap->tmr_ctrl);
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_ctrl, (tmrCtrl & ~(TMR_CTRL_TE)));

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_SetClockOffset(t_Handle h_FmRtc, int64_t offset)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    /* TMR_OFF_L must be written first */
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_off_l, (uint32_t)offset);
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_off_h, (uint32_t)(offset >> 32));

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_SetAlarm(t_Handle h_FmRtc, t_FmRtcAlarmParams *p_FmRtcAlarmParams)
{
    t_FmRtc         *p_Rtc = (t_FmRtc *)h_FmRtc;
    t_FmRtcMemMap   *p_MemMap;
    uint32_t        tmpReg;
    uint64_t        tmpAlarm;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_MemMap = p_Rtc->p_MemMap;

    if (p_FmRtcAlarmParams->alarmId >= FM_RTC_NUM_OF_ALARMS)
    {
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Alarm ID"));
    }

    if(p_FmRtcAlarmParams->alarmTime < p_Rtc->clockPeriodNanoSec)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Alarm time must be equal or larger than RTC period - %d nanoseconds", p_Rtc->clockPeriodNanoSec));
    if(p_FmRtcAlarmParams->alarmTime % (uint64_t)p_Rtc->clockPeriodNanoSec)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Alarm time must be a multiple of RTC period - %d nanoseconds", p_Rtc->clockPeriodNanoSec));
    tmpAlarm = p_FmRtcAlarmParams->alarmTime/(uint64_t)p_Rtc->clockPeriodNanoSec;

    /* TMR_ALARM_L must be written first */
    WRITE_UINT32(p_MemMap->tmr_alarm[p_FmRtcAlarmParams->alarmId].tmr_alarm_l, (uint32_t)tmpAlarm);
    WRITE_UINT32(p_MemMap->tmr_alarm[p_FmRtcAlarmParams->alarmId].tmr_alarm_h,
                 (uint32_t)(tmpAlarm >> 32));

    if (p_FmRtcAlarmParams->f_AlarmCallback)
    {
        p_Rtc->alarmParams[p_FmRtcAlarmParams->alarmId].f_AlarmCallback = p_FmRtcAlarmParams->f_AlarmCallback;
        p_Rtc->alarmParams[p_FmRtcAlarmParams->alarmId].clearOnExpiration = p_FmRtcAlarmParams->clearOnExpiration;

        if(p_FmRtcAlarmParams->alarmId == 0)
            tmpReg = TMR_TEVENT_ALM1;
        else
            tmpReg = TMR_TEVENT_ALM2;
        WRITE_UINT32(p_MemMap->tmr_temask, GET_UINT32(p_MemMap->tmr_temask) | tmpReg);
    }

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_SetPeriodicPulse(t_Handle h_FmRtc, t_FmRtcPeriodicPulseParams *p_FmRtcPeriodicPulseParams)
{
    t_FmRtc         *p_Rtc = (t_FmRtc *)h_FmRtc;
    t_FmRtcMemMap   *p_MemMap;
    uint32_t        tmpReg;
    uint64_t        tmpFiper;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    p_MemMap = p_Rtc->p_MemMap;

    if (p_FmRtcPeriodicPulseParams->periodicPulseId >= FM_RTC_NUM_OF_PERIODIC_PULSES)
    {
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Periodic pulse ID"));
    }
    if(GET_UINT32(p_MemMap->tmr_ctrl) & TMR_CTRL_TE)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Can't set Periodic pulse when RTC is enabled."));
    if(p_FmRtcPeriodicPulseParams->periodicPulsePeriod < p_Rtc->clockPeriodNanoSec)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Periodic pulse must be equal or larger than RTC period - %d nanoseconds", p_Rtc->clockPeriodNanoSec));
    if(p_FmRtcPeriodicPulseParams->periodicPulsePeriod % (uint64_t)p_Rtc->clockPeriodNanoSec)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Periodic pulse must be a multiple of RTC period - %d nanoseconds", p_Rtc->clockPeriodNanoSec));
    tmpFiper = p_FmRtcPeriodicPulseParams->periodicPulsePeriod/(uint64_t)p_Rtc->clockPeriodNanoSec;
    if(tmpFiper & 0xffffffff00000000LL)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Periodic pulse/RTC Period must be smaller than 4294967296", p_Rtc->clockPeriodNanoSec));

    WRITE_UINT32(p_MemMap->tmr_fiper[p_FmRtcPeriodicPulseParams->periodicPulseId], (uint32_t)tmpFiper);

    if (p_FmRtcPeriodicPulseParams->f_PeriodicPulseCallback)
    {
        p_Rtc->periodicPulseParams[p_FmRtcPeriodicPulseParams->periodicPulseId].f_PeriodicPulseCallback =
                                                           p_FmRtcPeriodicPulseParams->f_PeriodicPulseCallback;

        if(p_FmRtcPeriodicPulseParams->periodicPulseId == 0)
            tmpReg = TMR_TEVENT_PP1;
        else
            tmpReg = TMR_TEVENT_PP2;
        WRITE_UINT32(p_MemMap->tmr_temask, GET_UINT32(p_MemMap->tmr_temask) | tmpReg);
    }

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ClearPeriodicPulse(t_Handle h_FmRtc, uint8_t periodicPulseId)
{
    t_FmRtc     *p_Rtc = (t_FmRtc *)h_FmRtc;
    uint32_t    tmpReg;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    if (periodicPulseId >= FM_RTC_NUM_OF_PERIODIC_PULSES)
    {
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Periodic pulse ID"));
    }

    p_Rtc->periodicPulseParams[periodicPulseId].f_PeriodicPulseCallback = NULL;

    if(periodicPulseId == 0)
        tmpReg = TMR_TEVENT_PP1;
    else
        tmpReg = TMR_TEVENT_PP2;
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_temask, GET_UINT32(p_Rtc->p_MemMap->tmr_temask) & ~tmpReg);

    if (GET_UINT32(p_Rtc->p_MemMap->tmr_ctrl) & TMR_CTRL_FS)
        WRITE_UINT32(p_Rtc->p_MemMap->tmr_ctrl, GET_UINT32(p_Rtc->p_MemMap->tmr_ctrl) & ~TMR_CTRL_FS);

    WRITE_UINT32(p_Rtc->p_MemMap->tmr_fiper[periodicPulseId], 0xFFFFFFFF);

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_SetExternalTrigger(t_Handle h_FmRtc, t_FmRtcExternalTriggerParams *p_FmRtcExternalTriggerParams)
{
    t_FmRtc     *p_Rtc = (t_FmRtc *)h_FmRtc;
    uint32_t    tmpReg;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    if (p_FmRtcExternalTriggerParams->externalTriggerId >= FM_RTC_NUM_OF_EXT_TRIGGERS)
    {
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("External Trigger ID"));
    }

    if (p_FmRtcExternalTriggerParams->f_ExternalTriggerCallback)
    {
        p_Rtc->externalTriggerParams[p_FmRtcExternalTriggerParams->externalTriggerId].f_ExternalTriggerCallback = p_FmRtcExternalTriggerParams->f_ExternalTriggerCallback;
        if(p_FmRtcExternalTriggerParams->externalTriggerId == 0)
            tmpReg = TMR_TEVENT_ETS1;
        else
            tmpReg = TMR_TEVENT_ETS2;
        WRITE_UINT32(p_Rtc->p_MemMap->tmr_temask, GET_UINT32(p_Rtc->p_MemMap->tmr_temask) | tmpReg);
    }

    if(p_FmRtcExternalTriggerParams->usePulseAsInput)
    {
        if(p_FmRtcExternalTriggerParams->externalTriggerId == 0)
            tmpReg = TMR_CTRL_PP1L;
        else
            tmpReg = TMR_CTRL_PP2L;
        WRITE_UINT32(p_Rtc->p_MemMap->tmr_ctrl, GET_UINT32(p_Rtc->p_MemMap->tmr_ctrl) | tmpReg);
    }

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_ClearExternalTrigger(t_Handle h_FmRtc, uint8_t externalTriggerId)
{
    t_FmRtc     *p_Rtc = (t_FmRtc *)h_FmRtc;
    uint32_t    tmpReg;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    if (externalTriggerId >= FM_RTC_NUM_OF_EXT_TRIGGERS)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("External Trigger ID"));

    p_Rtc->externalTriggerParams[externalTriggerId].f_ExternalTriggerCallback = NULL;

    if(externalTriggerId == 0)
        tmpReg = TMR_TEVENT_ETS1;
    else
        tmpReg = TMR_TEVENT_ETS2;
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_temask, GET_UINT32(p_Rtc->p_MemMap->tmr_temask) & ~tmpReg);

    if(externalTriggerId == 0)
        tmpReg = TMR_CTRL_PP1L;
    else
        tmpReg = TMR_CTRL_PP2L;

    if (GET_UINT32(p_Rtc->p_MemMap->tmr_ctrl) & tmpReg)
        WRITE_UINT32(p_Rtc->p_MemMap->tmr_ctrl, GET_UINT32(p_Rtc->p_MemMap->tmr_ctrl) & ~tmpReg);

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_GetExternalTriggerTimeStamp(t_Handle             h_FmRtc,
                                              uint8_t           triggerId,
                                              uint64_t          *p_TimeStamp)
{
    t_FmRtc     *p_Rtc = (t_FmRtc *)h_FmRtc;
    uint64_t    timeStamp;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    if (triggerId >= FM_RTC_NUM_OF_EXT_TRIGGERS)
    {
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("External trigger ID"));
    }

    timeStamp = (uint64_t)GET_UINT32(p_Rtc->p_MemMap->tmr_etts[triggerId].tmr_etts_l);
    timeStamp |= ((uint64_t)GET_UINT32(p_Rtc->p_MemMap->tmr_etts[triggerId].tmr_etts_h) << 32);

    timeStamp = timeStamp*p_Rtc->clockPeriodNanoSec;
    *p_TimeStamp = timeStamp;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_GetCurrentTime(t_Handle h_FmRtc, uint64_t *p_Ts)
{
    t_FmRtc     *p_Rtc = (t_FmRtc *)h_FmRtc;
    uint64_t    time;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    /* TMR_CNT_L must be read first to get an accurate value */
    time = (uint64_t)GET_UINT32(p_Rtc->p_MemMap->tmr_cnt_l);
    time |= ((uint64_t)GET_UINT32(p_Rtc->p_MemMap->tmr_cnt_h) << 32);

    time = time*p_Rtc->clockPeriodNanoSec;

    *p_Ts = time;

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_SetCurrentTime(t_Handle h_FmRtc, uint64_t ts)
{
    t_FmRtc     *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    ts = ts/p_Rtc->clockPeriodNanoSec;
    /* TMR_CNT_L must be written first to get an accurate value */
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_cnt_l, (uint32_t)ts);
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_cnt_h, (uint32_t)(ts >> 32));

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_GetFreqCompensation(t_Handle h_FmRtc, uint32_t *p_Compensation)
{
    t_FmRtc     *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    *p_Compensation = (uint32_t)
        DIV_CEIL(ACCUMULATOR_OVERFLOW * 1000,
                 p_Rtc->clockPeriodNanoSec * p_Rtc->srcClkFreqMhz);

    return E_OK;
}

/*****************************************************************************/
t_Error FM_RTC_SetFreqCompensation(t_Handle h_FmRtc, uint32_t freqCompensation)
{
    t_FmRtc *p_Rtc = (t_FmRtc *)h_FmRtc;

    SANITY_CHECK_RETURN_ERROR(p_Rtc, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Rtc->p_RtcDriverParam, E_INVALID_STATE);

    /* set the new freqCompensation */
    WRITE_UINT32(p_Rtc->p_MemMap->tmr_add, freqCompensation);

    return E_OK;
}

/*****************************************************************************/
#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FM_RTC_DumpRegs(t_Handle h_FmRtc)
{
    t_FmRtc         *p_Rtc = (t_FmRtc *)h_FmRtc;
    t_FmRtcMemMap   *p_MemMap = p_Rtc->p_MemMap;
    int             i = 0;

    DECLARE_DUMP;

    if (p_MemMap)
    {

        DUMP_TITLE(p_MemMap, ("RTC:"));
        DUMP_VAR(p_MemMap, tmr_id);
        DUMP_VAR(p_MemMap, tmr_id2);
        DUMP_VAR(p_MemMap, tmr_ctrl);
        DUMP_VAR(p_MemMap, tmr_tevent);
        DUMP_VAR(p_MemMap, tmr_temask);
        DUMP_VAR(p_MemMap, tmr_cnt_h);
        DUMP_VAR(p_MemMap, tmr_cnt_l);
        DUMP_VAR(p_MemMap, tmr_ctrl);
        DUMP_VAR(p_MemMap, tmr_add);
        DUMP_VAR(p_MemMap, tmr_acc);
        DUMP_VAR(p_MemMap, tmr_prsc);
        DUMP_VAR(p_MemMap, tmr_off_h);
        DUMP_VAR(p_MemMap, tmr_off_l);

        DUMP_SUBSTRUCT_ARRAY(i, 2)
        {
            DUMP_VAR(p_MemMap, tmr_alarm[i].tmr_alarm_h);
            DUMP_VAR(p_MemMap, tmr_alarm[i].tmr_alarm_l);
        }
        DUMP_SUBSTRUCT_ARRAY(i, 2)
        {
            DUMP_VAR(p_MemMap, tmr_fiper[i]);
            DUMP_VAR(p_MemMap, tmr_fiper[i]);
        }
        DUMP_SUBSTRUCT_ARRAY(i, 2)
        {
            DUMP_VAR(p_MemMap, tmr_etts[i].tmr_etts_l);
            DUMP_VAR(p_MemMap, tmr_etts[i].tmr_etts_l);
        }
    }

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && ... */
