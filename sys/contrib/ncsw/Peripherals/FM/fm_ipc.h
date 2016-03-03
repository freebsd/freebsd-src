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

/**************************************************************************//**
 @File          fm_ipc.h

 @Description   FM Inter-Partition prototypes, structures and definitions.
*//***************************************************************************/
#ifndef __FM_IPC_H
#define __FM_IPC_H

#include "error_ext.h"
#include "std_ext.h"


/**************************************************************************//**
 @Group         FM_grp Frame Manager API

 @Description   FM API functions, definitions and enums

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Group         FM_IPC_grp FM Inter-Partition messaging Unit

 @Description   FM Inter-Partition messaging unit API definitions and enums.

 @{
*//***************************************************************************/

#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

/**************************************************************************//**
 @Description   enum for defining MAC types
*//***************************************************************************/

/**************************************************************************//**
 @Description   A structure of parameters for specifying a MAC.
*//***************************************************************************/
typedef _Packed struct
{
    uint8_t         id;
    uint32_t        enumType;
} _PackedType t_FmIpcMacParams;

/**************************************************************************//**
 @Description   A structure of parameters for specifying a MAC.
*//***************************************************************************/
typedef _Packed struct
{
    t_FmIpcMacParams    macParams;
    uint16_t            maxFrameLength;
} _PackedType t_FmIpcMacMaxFrameParams;

/**************************************************************************//**
 @Description   FM physical Address
*//***************************************************************************/
typedef _Packed struct t_FmIpcPhysAddr
{
    volatile uint8_t    high;
    volatile uint32_t   low;
} _PackedType t_FmIpcPhysAddr;

/**************************************************************************//**
 @Description   Structure for IPC communication during FM_PORT_Init.
*//***************************************************************************/
typedef _Packed struct t_FmIpcPortInInitParams {
    uint8_t             hardwarePortId;     /**< IN. port Id */
    uint32_t            enumPortType;       /**< IN. Port type */
    uint8_t             boolIndependentMode;/**< IN. TRUE if FM Port operates in independent mode */
    uint16_t            liodnOffset;        /**< IN. Port's requested resource */
    uint8_t             numOfTasks;         /**< IN. Port's requested resource */
    uint8_t             numOfExtraTasks;    /**< IN. Port's requested resource */
    uint8_t             numOfOpenDmas;      /**< IN. Port's requested resource */
    uint8_t             numOfExtraOpenDmas; /**< IN. Port's requested resource */
    uint32_t            sizeOfFifo;         /**< IN. Port's requested resource */
    uint32_t            extraSizeOfFifo;    /**< IN. Port's requested resource */
    uint8_t             deqPipelineDepth;   /**< IN. Port's requested resource */
    uint16_t            liodnBase;          /**< IN. Irrelevant for P4080 rev 1.
                                                 LIODN base for this port, to be
                                                 used together with LIODN offset. */
} _PackedType t_FmIpcPortInInitParams;


/**************************************************************************//**
 @Description   Structure for IPC communication between port and FM
                regarding tasks and open DMA resources management.
*//***************************************************************************/
typedef _Packed struct t_FmIpcPortRsrcParams {
    uint8_t             hardwarePortId;     /**< IN. port Id */
    uint32_t            val;                /**< IN. Port's requested resource */
    uint32_t            extra;              /**< IN. Port's requested resource */
    uint8_t             boolInitialConfig;
} _PackedType t_FmIpcPortRsrcParams;


/**************************************************************************//**
 @Description   Structure for IPC communication between port and FM
                regarding tasks and open DMA resources management.
*//***************************************************************************/
typedef _Packed struct t_FmIpcPortFifoParams {
    t_FmIpcPortRsrcParams   rsrcParams;
    uint32_t                enumPortType;
    uint8_t                 boolIndependentMode;
    uint8_t                 deqPipelineDepth;
    uint8_t                 numOfPools;
    uint16_t                secondLargestBufSize;
    uint16_t                largestBufSize;
    uint8_t                 boolInitialConfig;
} _PackedType t_FmIpcPortFifoParams;

/**************************************************************************//**
 @Description   Structure for port-FM communication during FM_PORT_Free.
*//***************************************************************************/
typedef _Packed struct t_FmIpcPortFreeParams {
    uint8_t             hardwarePortId;         /**< IN. port Id */
    uint32_t            enumPortType;           /**< IN. Port type */
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    uint8_t             deqPipelineDepth;       /**< IN. Port's requested resource */
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */
} _PackedType t_FmIpcPortFreeParams;

/**************************************************************************//**
 @Description   Structure for defining DMA status
*//***************************************************************************/
typedef _Packed struct t_FmIpcDmaStatus {
    uint8_t    boolCmqNotEmpty;            /**< Command queue is not empty */
    uint8_t    boolBusError;               /**< Bus error occurred */
    uint8_t    boolReadBufEccError;        /**< Double ECC error on buffer Read */
    uint8_t    boolWriteBufEccSysError;    /**< Double ECC error on buffer write from system side */
    uint8_t    boolWriteBufEccFmError;     /**< Double ECC error on buffer write from FM side */
} _PackedType t_FmIpcDmaStatus;

typedef _Packed struct t_FmIpcRegisterIntr
{
    uint8_t         guestId;        /* IN */
    uint32_t        event;          /* IN */
} _PackedType t_FmIpcRegisterIntr;

typedef _Packed struct t_FmIpcIsr
{
    uint8_t         boolErr;        /* IN */
    uint32_t        pendingReg;     /* IN */
} _PackedType t_FmIpcIsr;

/**************************************************************************//**
 @Description   structure for returning revision information
*//***************************************************************************/
typedef _Packed struct t_FmIpcRevisionInfo {
    uint8_t         majorRev;               /**< OUT: Major revision */
    uint8_t         minorRev;               /**< OUT: Minor revision */
} _PackedType t_FmIpcRevisionInfo;

/**************************************************************************//**
 @Description   Structure for defining Fm number of Fman controlers
*//***************************************************************************/
typedef _Packed struct t_FmIpcPortNumOfFmanCtrls {
    uint8_t             hardwarePortId;         /**< IN. port Id */
    uint8_t             numOfFmanCtrls;         /**< IN. Port type */
} t_FmIpcPortNumOfFmanCtrls;

/**************************************************************************//**
 @Description   structure for setting Fman contriller events
*//***************************************************************************/
typedef _Packed struct t_FmIpcFmanEvents {
    uint8_t         eventRegId;               /**< IN: Fman controller event register id */
    uint32_t        enableEvents;             /**< IN/OUT: required enabled events mask */
} _PackedType t_FmIpcFmanEvents;

#define FM_IPC_MAX_REPLY_BODY_SIZE  16
#define FM_IPC_MAX_REPLY_SIZE       (FM_IPC_MAX_REPLY_BODY_SIZE + sizeof(uint32_t))
#define FM_IPC_MAX_MSG_SIZE         30
typedef _Packed struct t_FmIpcMsg
{
    uint32_t    msgId;
    uint8_t     msgBody[FM_IPC_MAX_MSG_SIZE];
} _PackedType t_FmIpcMsg;

typedef _Packed struct t_FmIpcReply
{
    uint32_t    error;
    uint8_t     replyBody[FM_IPC_MAX_REPLY_BODY_SIZE];
} _PackedType t_FmIpcReply;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


/***************************************************************************/
/************************ FRONT-END-TO-BACK-END*****************************/
/***************************************************************************/

/**************************************************************************//**
 @Function      FM_GET_TIMESTAMP_SCALE

 @Description   Used by FM front-end.

 @Param[out]    uint32_t Pointer
*//***************************************************************************/
#define FM_GET_TIMESTAMP_SCALE      1

/**************************************************************************//**
 @Function      FM_GET_COUNTER

 @Description   Used by FM front-end.

 @Param[in/out] t_FmIpcGetCounter Pointer
*//***************************************************************************/
#define FM_GET_COUNTER              2

/**************************************************************************//**
 @Function      FM_DUMP_REGS

 @Description   Used by FM front-end for the PORT module in order to set and get
                parameters in/from master FM module on FM PORT initialization time.

 @Param         None
*//***************************************************************************/
#define FM_DUMP_REGS                3

/**************************************************************************//**
 @Function      FM_GET_SET_PORT_PARAMS

 @Description   Used by FM front-end for the PORT module in order to set and get
                parameters in/from master FM module on FM PORT initialization time.

 @Param[in/out] t_FmIcPortInitParams Pointer
*//***************************************************************************/
#define FM_GET_SET_PORT_PARAMS      4

/**************************************************************************//**
 @Function      FM_FREE_PORT

 @Description   Used by FM front-end for the PORT module when a port is freed
                to free all FM PORT resources.

 @Param[in]     uint8_t Pointer
*//***************************************************************************/
#define FM_FREE_PORT                5

/**************************************************************************//**
 @Function      FM_RESET_MAC

 @Description   Used by front-end for the MAC module to reset the MAC registers

 @Param[in]     t_FmIpcMacParams Pointer .
*//***************************************************************************/
#define FM_RESET_MAC                6

/**************************************************************************//**
 @Function      FM_RESUME_STALLED_PORT

 @Description   Used by FM front-end for the PORT module in order to
                release a stalled FM Port.

 @Param[in]     uint8_t Pointer
*//***************************************************************************/
#define FM_RESUME_STALLED_PORT      7

/**************************************************************************//**
 @Function      FM_IS_PORT_STALLED

 @Description   Used by FM front-end for the PORT module in order to check whether
                an FM port is stalled.

 @Param[in/out] t_FmIcPortIsStalled Pointer
*//***************************************************************************/
#define FM_IS_PORT_STALLED          8

/**************************************************************************//**
 @Function      FM_DUMP_PORT_REGS

 @Description   Used by FM front-end for the PORT module in order to dump
                all port registers.

 @Param[in]     uint8_t Pointer
*//***************************************************************************/
#define FM_DUMP_PORT_REGS           9

/**************************************************************************//**
 @Function      FM_GET_REV

 @Description   Used by FM front-end for the PORT module in order to dump
                all port registers.

 @Param[in]     uint8_t Pointer
*//***************************************************************************/
#define FM_GET_REV                  10

/**************************************************************************//**
 @Function      FM_REGISTER_INTR

 @Description   Used by FM front-end to register an interrupt handler to
                be called upon interrupt for guest.

 @Param[out]    t_FmIpcRegisterIntr Pointer
*//***************************************************************************/
#define FM_REGISTER_INTR            11

/**************************************************************************//**
 @Function      FM_GET_CLK_FREQ

 @Description   Used by FM Front-end to read the FM clock frequency.

 @Param[out]    uint32_t Pointer
*//***************************************************************************/
#define FM_GET_CLK_FREQ             12

/**************************************************************************//**
 @Function      FM_DMA_STAT

 @Description   Used by FM front-end to read the FM DMA status.

 @Param[out]    t_FmIpcDmaStatus Pointer
*//***************************************************************************/
#define FM_DMA_STAT                 13

/**************************************************************************//**
 @Function      FM_ALLOC_FMAN_CTRL_EVENT_REG

 @Description   Used by FM front-end to allocate event register.

 @Param[out]    Event register id Pointer
*//***************************************************************************/
#define FM_ALLOC_FMAN_CTRL_EVENT_REG 14

/**************************************************************************//**
 @Function      FM_FREE_FMAN_CTRL_EVENT_REG

 @Description   Used by FM front-end to free locate event register.

 @Param[in]    uint8_t Pointer - Event register id
*//***************************************************************************/
#define FM_FREE_FMAN_CTRL_EVENT_REG 15

/**************************************************************************//**
 @Function      FM_SET_FMAN_CTRL_EVENTS_ENABLE

 @Description   Used by FM front-end to enable events in the FPM
                Fman controller event register.

 @Param[in]    t_FmIpcFmanEvents Pointer
*//***************************************************************************/
#define FM_SET_FMAN_CTRL_EVENTS_ENABLE 16

/**************************************************************************//**
 @Function      FM_SET_FMAN_CTRL_EVENTS_ENABLE

 @Description   Used by FM front-end to enable events in the FPM
                Fman controller event register.

 @Param[in/out] t_FmIpcFmanEvents Pointer
*//***************************************************************************/
#define FM_GET_FMAN_CTRL_EVENTS_ENABLE 17

/**************************************************************************//**
 @Function      FM_SET_MAC_MAX_FRAME

 @Description   Used by FM front-end to set MAC's MTU/RTU's in
                back-end.

 @Param[in/out] t_FmIpcMacMaxFrameParams Pointer
*//***************************************************************************/
#define FM_SET_MAC_MAX_FRAME 18

/**************************************************************************//**
 @Function      FM_GET_PHYS_MURAM_BASE

 @Description   Used by FM front-end in order to get MURAM base address

 @Param[in/out] t_FmIpcPhysAddr Pointer
*//***************************************************************************/
#define FM_GET_PHYS_MURAM_BASE  19

/**************************************************************************//**
 @Function      FM_MASTER_IS_ALIVE

 @Description   Used by FM front-end in order to verify Master is up

 @Param[in/out] bool
*//***************************************************************************/
#define FM_MASTER_IS_ALIVE          20

#define FM_ENABLE_RAM_ECC           21
#define FM_DISABLE_RAM_ECC          22
#define FM_SET_NUM_OF_FMAN_CTRL     23
#define FM_SET_SIZE_OF_FIFO         24
#define FM_SET_NUM_OF_TASKS         25
#define FM_SET_NUM_OF_OPEN_DMAS     26

#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
#define FM_10G_TX_ECC_WA            100
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */

/***************************************************************************/
/************************ BACK-END-TO-FRONT-END*****************************/
/***************************************************************************/

/**************************************************************************//**
 @Function      FM_GUEST_ISR

 @Description   Used by FM back-end to report an interrupt to the front-end.

 @Param[out]    t_FmIpcIsr Pointer
*//***************************************************************************/
#define FM_GUEST_ISR                1



/** @} */ /* end of FM_IPC_grp group */
/** @} */ /* end of FM_grp group */


#endif /* __FM_IPC_H */
