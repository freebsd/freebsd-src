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
 @File          fm_rtc.h

 @Description   Memory map and internal definitions for FM RTC IEEE1588 Timer driver.

 @Cautions      None
*//***************************************************************************/

#ifndef __FM_RTC_H__
#define __FM_RTC_H__

#include "std_ext.h"
#include "fm_rtc_ext.h"


#define __ERR_MODULE__  MODULE_FM_RTC

/* General definitions */

#define NANOSEC_PER_ONE_HZ_TICK         1000000000
#define MIN_RTC_CLK_FREQ_HZ             1000
#define MHz                             1000000

#define ACCUMULATOR_OVERFLOW            ((uint64_t)(1LL << 32))

/* RTC default values */
#define DEFAULT_srcClock                e_FM_RTC_SOURCE_CLOCK_SYSTEM
#define DEFAULT_bypass      FALSE
#define DEFAULT_invertInputClkPhase     FALSE
#define DEFAULT_invertOutputClkPhase    FALSE
#define DEFAULT_outputClockDivisor      0x00000002
#define DEFAULT_alarmPolarity           e_FM_RTC_ALARM_POLARITY_ACTIVE_HIGH
#define DEFAULT_triggerPolarity         e_FM_RTC_TRIGGER_ON_FALLING_EDGE
#define DEFAULT_pulseRealign            FALSE
#define DEFAULT_clockPeriod             1000

/* FM RTC Registers definitions */
#define TMR_CTRL_ALMP1                  0x80000000
#define TMR_CTRL_ALMP2                  0x40000000
#define TMR_CTRL_FS                     0x10000000
#define TMR_CTRL_PP1L                   0x08000000
#define TMR_CTRL_PP2L                   0x04000000
#define TMR_CTRL_TCLK_PERIOD_MASK       0x03FF0000
#define TMR_CTRL_FRD                    0x00004000
#define TMR_CTRL_SLV                    0x00002000
#define TMR_CTRL_ETEP1                  0x00000100
#define TMR_CTRL_COPH                   0x00000080
#define TMR_CTRL_CIPH                   0x00000040
#define TMR_CTRL_TMSR                   0x00000020
#define TMR_CTRL_DBG                    0x00000010
#define TMR_CTRL_BYP                    0x00000008
#define TMR_CTRL_TE                     0x00000004
#define TMR_CTRL_CKSEL_OSC_CLK          0x00000003
#define TMR_CTRL_CKSEL_MAC_CLK          0x00000001
#define TMR_CTRL_CKSEL_EXT_CLK          0x00000000
#define TMR_CTRL_TCLK_PERIOD_SHIFT      16

#define TMR_TEVENT_ETS2                 0x02000000
#define TMR_TEVENT_ETS1                 0x01000000
#define TMR_TEVENT_ALM2                 0x00020000
#define TMR_TEVENT_ALM1                 0x00010000
#define TMR_TEVENT_PP1                  0x00000080
#define TMR_TEVENT_PP2                  0x00000040
#define TMR_TEVENT_PP3                  0x00000020
#define TMR_TEVENT_ALL                  (TMR_TEVENT_ETS2 | TMR_TEVENT_ETS1 | \
                                         TMR_TEVENT_ALM2 | TMR_TEVENT_ALM1 | \
                                         TMR_TEVENT_PP1 | TMR_TEVENT_PP2 | TMR_TEVENT_PP3)

#define TMR_PRSC_OCK_MASK               0x0000FFFF


/**************************************************************************//**
 @Description       Memory Mapped Registers
*//***************************************************************************/

#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

/**************************************************************************//**
 @Description FM RTC timer alarm
*//***************************************************************************/
typedef _Packed struct t_TmrAlaram
{
    volatile uint32_t   tmr_alarm_h;    /**<  */
    volatile uint32_t   tmr_alarm_l;    /**<  */
} _PackedType t_TmrAlaram;

/**************************************************************************//**
 @Description FM RTC timer Ex trigger
*//***************************************************************************/
typedef _Packed struct t_TmrExtTrigger
{
    volatile uint32_t   tmr_etts_h;     /**<  */
    volatile uint32_t   tmr_etts_l;     /**<  */
} _PackedType t_TmrExtTrigger;

typedef _Packed struct
{
    volatile uint32_t tmr_id;      /* Module ID and version register */
    volatile uint32_t tmr_id2;     /* Module ID and configuration register */
    volatile uint32_t PTP_RESERVED1[30];
    volatile uint32_t tmr_ctrl;    /* timer control register */
    volatile uint32_t tmr_tevent;  /* timer event register */
    volatile uint32_t tmr_temask;  /* timer event mask register */
    volatile uint32_t PTP_RESERVED2[3];
    volatile uint32_t tmr_cnt_h;   /* timer counter high register */
    volatile uint32_t tmr_cnt_l;   /* timer counter low register */
    volatile uint32_t tmr_add;     /* timer drift compensation addend register */
    volatile uint32_t tmr_acc;     /* timer accumulator register */
    volatile uint32_t tmr_prsc;    /* timer prescale */
    volatile uint32_t PTP_RESERVED3;
    volatile uint32_t tmr_off_h;    /* timer offset high */
    volatile uint32_t tmr_off_l;    /* timer offset low  */
    volatile t_TmrAlaram tmr_alarm[FM_RTC_NUM_OF_ALARMS]; /* timer alarm */
    volatile uint32_t PTP_RESERVED4[2];
    volatile uint32_t tmr_fiper[FM_RTC_NUM_OF_PERIODIC_PULSES]; /* timer fixed period interval */
    volatile uint32_t PTP_RESERVED5[2];
    volatile t_TmrExtTrigger tmr_etts[FM_RTC_NUM_OF_EXT_TRIGGERS]; /*time stamp general purpose external */
    volatile uint32_t PTP_RESERVED6[3];
} _PackedType t_FmRtcMemMap;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


/**************************************************************************//**
 @Description   RTC FM driver parameters structure.
*//***************************************************************************/
typedef struct t_FmRtcDriverParam
{
    t_Handle                h_Fm;                   /**<  */
    e_FmSrcClk              srcClk;               /**<  */
    uint32_t                extSrcClkFreq;         /**<  */
    uint32_t                rtcFreqHz;              /**<  */
    bool                    timerSlaveMode;         /*Slave/Master Mode*/
    bool                    invertInputClkPhase;
    bool                    invertOutputClkPhase;
    uint32_t                eventsMask;
    bool                    bypass; /**< Indicates if frequency compensation is bypassed */
    bool                    pulseRealign;
    e_FmRtcAlarmPolarity    alarmPolarity[FM_RTC_NUM_OF_ALARMS];
    e_FmRtcTriggerPolarity  triggerPolarity[FM_RTC_NUM_OF_EXT_TRIGGERS];
} t_FmRtcDriverParam;

typedef struct t_FmRtcAlarm
{
    t_FmRtcExceptionsCallback   *f_AlarmCallback;
    bool                        clearOnExpiration;
} t_FmRtcAlarm;

typedef struct t_FmRtcPeriodicPulse
{
    t_FmRtcExceptionsCallback   *f_PeriodicPulseCallback;
} t_FmRtcPeriodicPulse;

typedef struct t_FmRtcExternalTrigger
{
    t_FmRtcExceptionsCallback   *f_ExternalTriggerCallback;
} t_FmRtcExternalTrigger;


/**************************************************************************//**
 @Description RTC FM driver control structure.
*//***************************************************************************/
typedef struct t_FmRtc
{
    t_Part                  *p_Part;            /**< Pointer to the integration device              */
    t_Handle                h_Fm;
    t_Handle                h_App;              /**< Application handle */
    t_FmRtcMemMap           *p_MemMap;          /**< Pointer to RTC memory map */
    uint32_t                clockPeriodNanoSec; /**< RTC clock period in nano-seconds (for FS mode) */
    uint32_t                srcClkFreqMhz;
    uint16_t                outputClockDivisor; /**< Output clock divisor (for FS mode) */
    t_FmRtcAlarm            alarmParams[FM_RTC_NUM_OF_ALARMS];
    t_FmRtcPeriodicPulse    periodicPulseParams[FM_RTC_NUM_OF_PERIODIC_PULSES];
    t_FmRtcExternalTrigger  externalTriggerParams[FM_RTC_NUM_OF_EXT_TRIGGERS];
    t_FmRtcDriverParam      *p_RtcDriverParam;  /**< RTC Driver parameters (for Init phase) */
} t_FmRtc;


#endif /* __FM_RTC_H__ */
