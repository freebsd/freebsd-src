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
 @File          fm_port.c

 @Description   FM driver routines implementation.
*//***************************************************************************/
#include "error_ext.h"
#include "std_ext.h"
#include "string_ext.h"
#include "sprint_ext.h"
#include "debug_ext.h"
#include "fm_pcd_ext.h"

#include "fm_port.h"


/****************************************/
/*       static functions               */
/****************************************/

static t_Error CheckInitParameters(t_FmPort *p_FmPort)
{
    t_FmPortDriverParam *p_Params = p_FmPort->p_FmPortDriverParam;
    t_Error             ans = E_OK;
    uint32_t            unusedMask;
    uint8_t             i;
    uint8_t             j;
    bool                found;

    if (p_FmPort->imEn)
    {
        if ((ans = FmPortImCheckInitParameters(p_FmPort)) != E_OK)
            return ERROR_CODE(ans);
    }
    else
    {
        /****************************************/
        /*   Rx only                            */
        /****************************************/
        if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
        {
            /* external buffer pools */
            if(!p_Params->extBufPools.numOfPoolsUsed)
                 RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("extBufPools.numOfPoolsUsed=0. At least one buffer pool must be defined"));

            if(p_Params->extBufPools.numOfPoolsUsed > FM_PORT_MAX_NUM_OF_EXT_POOLS)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfPoolsUsed can't be larger than %d", FM_PORT_MAX_NUM_OF_EXT_POOLS));

            for(i=0;i<p_Params->extBufPools.numOfPoolsUsed;i++)
            {
                if(p_Params->extBufPools.extBufPool[i].id >= BM_MAX_NUM_OF_POOLS)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("extBufPools.extBufPool[%d].id can't be larger than %d", i, BM_MAX_NUM_OF_POOLS));
                if(!p_Params->extBufPools.extBufPool[i].size)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("extBufPools.extBufPool[%d].size is 0", i));
            }

            /* backup BM pools indication is valid only for some chip deriviatives
               (limited by the config routine) */
            if(p_Params->p_BackupBmPools)
            {
                if(p_Params->p_BackupBmPools->numOfBackupPools >= p_Params->extBufPools.numOfPoolsUsed)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("p_BackupBmPools must be smaller than extBufPools.numOfPoolsUsed"));
                found = FALSE;
                for(i = 0;i<p_Params->p_BackupBmPools->numOfBackupPools;i++)
                    for(j=0;j<p_Params->extBufPools.numOfPoolsUsed;j++)
                        if(p_Params->p_BackupBmPools->poolIds[i] == p_Params->extBufPools.extBufPool[j].id)
                            found = TRUE;
                if (!found)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("All p_BackupBmPools.poolIds must be included in extBufPools.extBufPool[n].id"));
            }

            /* up to extBufPools.numOfPoolsUsed pools may be defined */
            if(p_Params->bufPoolDepletion.numberOfPoolsModeEnable)
            {
                if((p_Params->bufPoolDepletion.numOfPools > p_Params->extBufPools.numOfPoolsUsed))
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufPoolDepletion.numOfPools can't be larger than %d and can't be larger than numOfPoolsUsed", FM_PORT_MAX_NUM_OF_EXT_POOLS));

                if(!p_Params->bufPoolDepletion.numOfPools)
                  RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufPoolDepletion.numOfPoolsToConsider can not be 0 when numberOfPoolsModeEnable=TRUE"));
            }
            /* Check that part of IC that needs copying is small enough to enter start margin */
            if(p_Params->intContext.size + p_Params->intContext.extBufOffset > p_Params->bufMargins.startMargins)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("intContext.size is larger than start margins"));

            if(p_Params->liodnOffset & ~FM_LIODN_OFFSET_MASK)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("liodnOffset is larger than %d", FM_LIODN_OFFSET_MASK+1));
#ifdef FM_PARTITION_ARRAY
            {
                t_FmRevisionInfo revInfo;
                FM_GetRevision(p_FmPort->h_Fm, &revInfo);
                if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                {
                    if(p_Params->liodnOffset >= MAX_LIODN_OFFSET)
                    {
                        p_Params->liodnOffset = (uint16_t)(p_Params->liodnOffset & (MAX_LIODN_OFFSET-1));
                        DBG(WARNING, ("liodnOffset number is out of rev1 range - MSB bits cleard."));
                    }
                }
            }
#endif /* FM_PARTITION_ARRAY */
        }

        /****************************************/
        /*   Non Rx ports                       */
        /****************************************/
        else
        {
            if(p_Params->deqSubPortal >= MAX_QMI_DEQ_SUBPORTAL)
                 RETURN_ERROR(MAJOR, E_INVALID_VALUE, (" deqSubPortal has to be in the range of 0 - %d", MAX_QMI_DEQ_SUBPORTAL));

            /* to protect HW internal-context from overwrite */
            if((p_Params->intContext.size) && (p_Params->intContext.intContextOffset < MIN_TX_INT_OFFSET))
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("non-Rx intContext.intContextOffset can't be smaller than %d", MIN_TX_INT_OFFSET));
        }

        /****************************************/
        /*   Rx Or Offline Parsing              */
        /****************************************/
        if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
        {

            if(!p_Params->dfltFqid)
                 RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dfltFqid must be between 1 and 2^24-1"));
#if defined(FM_CAPWAP_SUPPORT) && defined(FM_LOCKUP_ALIGNMENT_ERRATA_FMAN_SW004)
            if(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.manipExtraSpace % 16)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufferPrefixContent.manipExtraSpace has to be devidable by 16"));
#endif /* defined(FM_CAPWAP_SUPPORT) && ... */
        }

        /****************************************/
        /*   All ports                          */
        /****************************************/
        /* common BMI registers values */
        /* Check that Queue Id is not larger than 2^24, and is not 0 */
        if((p_Params->errFqid & ~0x00FFFFFF) || !p_Params->errFqid)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("errFqid must be between 1 and 2^24-1"));
        if(p_Params->dfltFqid & ~0x00FFFFFF)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dfltFqid must be between 1 and 2^24-1"));
    }

    /****************************************/
    /*   Rx only                            */
    /****************************************/
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
    {
        /* Check that divisible by 256 and not larger than 256 */
        if(p_Params->rxFifoPriElevationLevel % BMI_FIFO_UNITS)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("rxFifoPriElevationLevel has to be divisible by %d", BMI_FIFO_UNITS));
        if(!p_Params->rxFifoPriElevationLevel || (p_Params->rxFifoPriElevationLevel > BMI_MAX_FIFO_SIZE))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("rxFifoPriElevationLevel has to be in the range of 256 - %d", BMI_MAX_FIFO_SIZE));
        if(p_Params->rxFifoThreshold % BMI_FIFO_UNITS)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("rxFifoThreshold has to be divisible by %d", BMI_FIFO_UNITS));
        if(!p_Params->rxFifoThreshold ||(p_Params->rxFifoThreshold > BMI_MAX_FIFO_SIZE))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("rxFifoThreshold has to be in the range of 256 - %d", BMI_MAX_FIFO_SIZE));

        /* Check that not larger than 16 */
        if(p_Params->cutBytesFromEnd > FRAME_END_DATA_SIZE)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("cutBytesFromEnd can't be larger than %d", FRAME_END_DATA_SIZE));

        /* Check the margin definition */
        if(p_Params->bufMargins.startMargins > MAX_EXT_BUFFER_OFFSET)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufMargins.startMargins can't be larger than %d", MAX_EXT_BUFFER_OFFSET));
        if(p_Params->bufMargins.endMargins > MAX_EXT_BUFFER_OFFSET)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufMargins.endMargins can't be larger than %d", MAX_EXT_BUFFER_OFFSET));

        /* extra FIFO size (allowed only to Rx ports) */
        if(p_FmPort->fifoBufs.extra % BMI_FIFO_UNITS)
             RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fifoBufs.extra has to be divisible by %d", BMI_FIFO_UNITS));

        if(p_Params->bufPoolDepletion.numberOfPoolsModeEnable &&
           !p_Params->bufPoolDepletion.numOfPools)
              RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufPoolDepletion.numOfPoolsToConsider can not be 0 when numberOfPoolsModeEnable=TRUE"));
#ifdef FM_CSI_CFED_LIMIT
        {
            t_FmRevisionInfo revInfo;
            FM_GetRevision(p_FmPort->h_Fm, &revInfo);

            if (revInfo.majorRev == 4)
            {
                /* Check that not larger than 16 */
                if(p_Params->cutBytesFromEnd + p_Params->cheksumLastBytesIgnore > FRAME_END_DATA_SIZE)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("cheksumLastBytesIgnore + cutBytesFromEnd can't be larger than %d", FRAME_END_DATA_SIZE));
            }
        }
#endif /* FM_CSI_CFED_LIMIT */

    }

    /****************************************/
    /*   Non Rx ports                       */
    /****************************************/
    else
        /* extra FIFO size (allowed only to Rx ports) */
        if(p_FmPort->fifoBufs.extra)
             RETURN_ERROR(MAJOR, E_INVALID_VALUE, (" No fifoBufs.extra for non Rx ports"));

    /****************************************/
    /*   Rx & Tx                            */
    /****************************************/
    if((p_FmPort->portType == e_FM_PORT_TYPE_TX) || (p_FmPort->portType == e_FM_PORT_TYPE_TX_10G) ||
        (p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
    {
        /* Check that not larger than 16 */
        if(p_Params->cheksumLastBytesIgnore > FRAME_END_DATA_SIZE)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("cheksumLastBytesIgnore can't be larger than %d", FRAME_END_DATA_SIZE));
    }

    /****************************************/
    /*   Tx only                            */
    /****************************************/
    if((p_FmPort->portType == e_FM_PORT_TYPE_TX) || (p_FmPort->portType == e_FM_PORT_TYPE_TX_10G))
    {
        /* Check that divisible by 256 and not larger than 256 */
        if(p_Params->txFifoMinFillLevel % BMI_FIFO_UNITS)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("txFifoMinFillLevel has to be divisible by %d", BMI_FIFO_UNITS));
        if(p_Params->txFifoMinFillLevel > (BMI_MAX_FIFO_SIZE - 256))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("txFifoMinFillLevel has to be in the range of 0 - %d", BMI_MAX_FIFO_SIZE));
        if(p_Params->txFifoLowComfLevel % BMI_FIFO_UNITS)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("txFifoLowComfLevel has to be divisible by %d", BMI_FIFO_UNITS));
        if(!p_Params->txFifoLowComfLevel || (p_Params->txFifoLowComfLevel > BMI_MAX_FIFO_SIZE))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("txFifoLowComfLevel has to be in the range of 256 - %d", BMI_MAX_FIFO_SIZE));

        /* Check that not larger than 8 */
        if((!p_FmPort->txFifoDeqPipelineDepth) ||( p_FmPort->txFifoDeqPipelineDepth > MAX_FIFO_PIPELINE_DEPTH))
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("txFifoDeqPipelineDepth can't be larger than %d", MAX_FIFO_PIPELINE_DEPTH));
        if(p_FmPort->portType == e_FM_PORT_TYPE_TX)
            if(p_FmPort->txFifoDeqPipelineDepth > 2)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("txFifoDeqPipelineDepth for !G can't be larger than 2"));
    }
    else
    /****************************************/
    /*   Non Tx Ports                       */
    /****************************************/
    {
        /* If discard override was selected , no frames may be discarded. */
        if(p_Params->frmDiscardOverride && p_Params->errorsToDiscard)
            RETURN_ERROR(MAJOR, E_CONFLICT, ("errorsToDiscard is not empty, but frmDiscardOverride selected (all discarded frames to be enqueued to error queue)."));
    }

    /****************************************/
    /*   Rx and Offline parsing             */
    /****************************************/
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G)
        || (p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
    {
        if(p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING)
            unusedMask = BMI_STATUS_OP_MASK_UNUSED;
        else
            unusedMask = BMI_STATUS_RX_MASK_UNUSED;

        /* Check that no common bits with BMI_STATUS_MASK_UNUSED */
        if(p_Params->errorsToDiscard & unusedMask)
            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("errorsToDiscard contains undefined bits"));
    }

    /****************************************/
    /*   All ports                          */
    /****************************************/

   /* Check that divisible by 16 and not larger than 240 */
    if(p_Params->intContext.intContextOffset >MAX_INT_OFFSET)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("intContext.intContextOffset can't be larger than %d", MAX_INT_OFFSET));
    if(p_Params->intContext.intContextOffset % OFFSET_UNITS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("intContext.intContextOffset has to be divisible by %d", OFFSET_UNITS));

    /* check that ic size+ic internal offset, does not exceed ic block size */
    if(p_Params->intContext.size + p_Params->intContext.intContextOffset > MAX_IC_SIZE)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("intContext.size + intContext.intContextOffset has to be smaller than %d", MAX_IC_SIZE));
    /* Check that divisible by 16 and not larger than 256 */
    if(p_Params->intContext.size % OFFSET_UNITS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("intContext.size  has to be divisible by %d", OFFSET_UNITS));

    /* Check that divisible by 16 and not larger than 4K */
    if(p_Params->intContext.extBufOffset > MAX_EXT_OFFSET)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("intContext.extBufOffset can't be larger than %d", MAX_EXT_OFFSET));
    if(p_Params->intContext.extBufOffset % OFFSET_UNITS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("intContext.extBufOffset  has to be divisible by %d", OFFSET_UNITS));

    /* common BMI registers values */
    if((!p_FmPort->tasks.num) || (p_FmPort->tasks.num > MAX_NUM_OF_TASKS))
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("tasks.num can't be larger than %d", MAX_NUM_OF_TASKS));
    if(p_FmPort->tasks.extra > MAX_NUM_OF_EXTRA_TASKS)
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("tasks.extra can't be larger than %d", MAX_NUM_OF_EXTRA_TASKS));
    if((!p_FmPort->openDmas.num) || (p_FmPort->openDmas.num > MAX_NUM_OF_DMAS))
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("openDmas.num can't be larger than %d", MAX_NUM_OF_DMAS));
    if(p_FmPort->openDmas.extra > MAX_NUM_OF_EXTRA_DMAS)
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("openDmas.extra can't be larger than %d", MAX_NUM_OF_EXTRA_DMAS));
    if(!p_FmPort->fifoBufs.num || (p_FmPort->fifoBufs.num > BMI_MAX_FIFO_SIZE))
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fifoBufs.num has to be in the range of 256 - %d", BMI_MAX_FIFO_SIZE));
    if(p_FmPort->fifoBufs.num % BMI_FIFO_UNITS)
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fifoBufs.num has to be divisible by %d", BMI_FIFO_UNITS));

    return E_OK;
}

static void FmPortDriverParamFree(t_FmPort *p_FmPort)
{
    if(p_FmPort->p_FmPortDriverParam)
    {
        XX_Free(p_FmPort->p_FmPortDriverParam);
        p_FmPort->p_FmPortDriverParam = NULL;
    }
}

static t_Error SetExtBufferPools(t_FmPort *p_FmPort)
{
    t_FmPortExtPools            *p_ExtBufPools = &p_FmPort->p_FmPortDriverParam->extBufPools;
    t_FmPortBufPoolDepletion    *p_BufPoolDepletion = &p_FmPort->p_FmPortDriverParam->bufPoolDepletion;
    volatile uint32_t           *p_ExtBufRegs;
    volatile uint32_t           *p_BufPoolDepletionReg;
    bool                        rxPort;
    bool                        found;
    uint8_t                     orderedArray[FM_PORT_MAX_NUM_OF_EXT_POOLS];
    uint16_t                    sizesArray[BM_MAX_NUM_OF_POOLS];
    uint8_t                     count = 0;
    uint8_t                     numOfPools;
    uint16_t                    bufSize = 0, largestBufSize = 0;
    int                         i=0, j=0, k=0;
    uint32_t                    tmpReg, vector, minFifoSizeRequired=0;

    memset(&orderedArray, 0, sizeof(uint8_t) * FM_PORT_MAX_NUM_OF_EXT_POOLS);
    memset(&sizesArray, 0, sizeof(uint16_t) * BM_MAX_NUM_OF_POOLS);
    memcpy(&p_FmPort->extBufPools, p_ExtBufPools, sizeof(t_FmPortExtPools));

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_ExtBufRegs = p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_ebmpi;
            p_BufPoolDepletionReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_mpd;
            rxPort = TRUE;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_ExtBufRegs = p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_oebmpi;
            p_BufPoolDepletionReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ompd;
            rxPort = FALSE;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("Not available for port type"));
    }

    /* First we copy the external buffers pools information to an ordered local array */
    for(i=0;i<p_ExtBufPools->numOfPoolsUsed;i++)
    {
        /* get pool size */
        bufSize = p_ExtBufPools->extBufPool[i].size;

        /* keep sizes in an array according to poolId for direct access */
        sizesArray[p_ExtBufPools->extBufPool[i].id] =  bufSize;

        /* save poolId in an ordered array according to size */
        for (j=0;j<=i;j++)
        {
            /* this is the next free place in the array */
            if (j==i)
                orderedArray[i] = p_ExtBufPools->extBufPool[i].id;
            else
            {
                /* find the right place for this poolId */
                if(bufSize < sizesArray[orderedArray[j]])
                {
                    /* move the poolIds one place ahead to make room for this poolId */
                    for(k=i;k>j;k--)
                       orderedArray[k] = orderedArray[k-1];

                    /* now k==j, this is the place for the new size */
                    orderedArray[k] = p_ExtBufPools->extBufPool[i].id;
                    break;
                }
            }
        }
    }

    /* build the register value */

    for(i=0;i<p_ExtBufPools->numOfPoolsUsed;i++)
    {
        tmpReg = BMI_EXT_BUF_POOL_VALID | BMI_EXT_BUF_POOL_EN_COUNTER;
        tmpReg |= ((uint32_t)orderedArray[i] << BMI_EXT_BUF_POOL_ID_SHIFT);
        tmpReg |= sizesArray[orderedArray[i]];
        /* functionality available only for some deriviatives (limited by config) */
        if(p_FmPort->p_FmPortDriverParam->p_BackupBmPools)
            for(j=0;j<p_FmPort->p_FmPortDriverParam->p_BackupBmPools->numOfBackupPools;j++)
                if(orderedArray[i] == p_FmPort->p_FmPortDriverParam->p_BackupBmPools->poolIds[j])
                {
                    tmpReg |= BMI_EXT_BUF_POOL_BACKUP;
                    break;
                }
        WRITE_UINT32(*(p_ExtBufRegs+i), tmpReg);
    }

    if(p_FmPort->p_FmPortDriverParam->p_BackupBmPools)
        XX_Free(p_FmPort->p_FmPortDriverParam->p_BackupBmPools);

   numOfPools = (uint8_t)(rxPort ? FM_PORT_MAX_NUM_OF_EXT_POOLS:FM_PORT_MAX_NUM_OF_OBSERVED_EXT_POOLS);

    /* clear unused pools */
    for(i=p_ExtBufPools->numOfPoolsUsed;i<numOfPools;i++)
        WRITE_UINT32(*(p_ExtBufRegs+i), 0);

    p_FmPort->rxPoolsParams.largestBufSize = largestBufSize = sizesArray[orderedArray[p_ExtBufPools->numOfPoolsUsed-1]];
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
    {
#ifdef FM_FIFO_ALLOCATION_OLD_ALG
        t_FmRevisionInfo        revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);

        if(revInfo.majorRev != 4)
        {
            minFifoSizeRequired = (uint32_t)(((largestBufSize % BMI_FIFO_UNITS) ?
                                    ((largestBufSize/BMI_FIFO_UNITS + 1) * BMI_FIFO_UNITS) :
                                    largestBufSize) +
                                    (7*BMI_FIFO_UNITS));
        }
        else
#endif /* FM_FIFO_ALLOCATION_OLD_ALG */
        {
            p_FmPort->rxPoolsParams.numOfPools = p_ExtBufPools->numOfPoolsUsed;
            if(p_ExtBufPools->numOfPoolsUsed == 1)
                minFifoSizeRequired = 8*BMI_FIFO_UNITS;
            else
            {
                uint16_t secondLargestBufSize = sizesArray[orderedArray[p_ExtBufPools->numOfPoolsUsed-2]];
                p_FmPort->rxPoolsParams.secondLargestBufSize = secondLargestBufSize;
                minFifoSizeRequired = (uint32_t)(((secondLargestBufSize % BMI_FIFO_UNITS) ?
                                    ((secondLargestBufSize/BMI_FIFO_UNITS + 1) * BMI_FIFO_UNITS) :
                                    secondLargestBufSize) +
                                    (7*BMI_FIFO_UNITS));
            }
        }
        if(p_FmPort->fifoBufs.num < minFifoSizeRequired)
        {
            p_FmPort->fifoBufs.num = minFifoSizeRequired;
            DBG(INFO, ("FIFO size for Rx port enlarged to %d",minFifoSizeRequired));
        }
    }

    /* check if pool size is not too big */
    /* This is a definition problem in which if the fifo for the RX port
       is lower than the largest pool size the hardware will allocate scatter gather
       buffers even though the frame size can fit in a single buffer. */
    if (largestBufSize > p_FmPort->fifoBufs.num)
        DBG(WARNING, ("Frame larger than port Fifo size (%u) will be split to more than a single buffer (S/G) even if shorter than largest buffer size (%u)",
                p_FmPort->fifoBufs.num, largestBufSize));

    /* pool depletion */
    tmpReg = 0;
    if(p_BufPoolDepletion->numberOfPoolsModeEnable)
    {
        /* calculate vector for number of pools depletion */
        found = FALSE;
        vector = 0;
        count = 0;
        for(i=0;i<BM_MAX_NUM_OF_POOLS;i++)
        {
            if(p_BufPoolDepletion->poolsToConsider[i])
            {
                for(j=0;j<p_ExtBufPools->numOfPoolsUsed;j++)
                {
                    if (i == orderedArray[j])
                    {
                        vector |= 0x80000000 >> j;
                        found = TRUE;
                        count++;
                        break;
                    }
                }
                if (!found)
                    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Pools selected for depletion are not used."));
                else
                    found = FALSE;
            }
        }
        if (count < p_BufPoolDepletion->numOfPools)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufPoolDepletion.numOfPools is larger than the number of pools defined."));

        /* configure num of pools and vector for number of pools mode */
        tmpReg |= (((uint32_t)p_BufPoolDepletion->numOfPools - 1) << BMI_POOL_DEP_NUM_OF_POOLS_SHIFT);
        tmpReg |= vector;
    }

    if(p_BufPoolDepletion->singlePoolModeEnable)
    {
        /* calculate vector for number of pools depletion */
        found = FALSE;
        vector = 0;
        count = 0;
        for(i=0;i<BM_MAX_NUM_OF_POOLS;i++)
        {
            if(p_BufPoolDepletion->poolsToConsiderForSingleMode[i])
            {
                for(j=0;j<p_ExtBufPools->numOfPoolsUsed;j++)
                {
                    if (i == orderedArray[j])
                     {
                        vector |= 0x00000080 >> j;
                        found = TRUE;
                        count++;
                        break;
                    }
                }
                if (!found)
                    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Pools selected for depletion are not used."));
                else
                    found = FALSE;
            }
        }
        if (!count)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("No pools defined for single buffer mode pool depletion."));

        /* configure num of pools and vector for number of pools mode */
        tmpReg |= vector;
    }

    WRITE_UINT32(*p_BufPoolDepletionReg, tmpReg);

    return E_OK;
}

static t_Error ClearPerfCnts(t_FmPort *p_FmPort)
{
    FM_PORT_ModifyCounter(p_FmPort, e_FM_PORT_COUNTERS_TASK_UTIL, 0);
    FM_PORT_ModifyCounter(p_FmPort, e_FM_PORT_COUNTERS_QUEUE_UTIL, 0);
    FM_PORT_ModifyCounter(p_FmPort, e_FM_PORT_COUNTERS_DMA_UTIL, 0);
    FM_PORT_ModifyCounter(p_FmPort, e_FM_PORT_COUNTERS_FIFO_UTIL, 0);
    return E_OK;
}

static t_Error BmiRxPortInit(t_FmPort *p_FmPort)
{
    t_FmPortRxBmiRegs       *p_Regs = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs;
    uint32_t                tmpReg;
    t_FmPortDriverParam     *p_Params = p_FmPort->p_FmPortDriverParam;
    uint32_t                errorsToEnq = 0;
    t_FmPortPerformanceCnt  performanceContersParams;
    t_Error                 err;

    /* check that port is not busy */
    if (GET_UINT32(p_Regs->fmbm_rcfg) & BMI_PORT_CFG_EN)
         RETURN_ERROR(MAJOR, E_INVALID_STATE,
                      ("Port(%d,%d) is already enabled",p_FmPort->portType, p_FmPort->portId));

    /* Set Config register */
    tmpReg = 0;
    if (p_FmPort->imEn)
        tmpReg |= BMI_PORT_CFG_IM;
    /* No discard - all error frames go to error queue */
    else if (p_Params->frmDiscardOverride)
        tmpReg |= BMI_PORT_CFG_FDOVR;

    WRITE_UINT32(p_Regs->fmbm_rcfg, tmpReg);

    /* Configure dma attributes */
    tmpReg = 0;
    tmpReg |= (uint32_t)p_Params->dmaSwapData << BMI_DMA_ATTR_SWP_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaIntContextCacheAttr << BMI_DMA_ATTR_IC_CACHE_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaHeaderCacheAttr << BMI_DMA_ATTR_HDR_CACHE_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaScatterGatherCacheAttr << BMI_DMA_ATTR_SG_CACHE_SHIFT;
    if(p_Params->dmaWriteOptimize)
        tmpReg |= BMI_DMA_ATTR_WRITE_OPTIMIZE;

    WRITE_UINT32(p_Regs->fmbm_rda, tmpReg);

    /* Configure Rx Fifo params */
    tmpReg = 0;
    tmpReg |= ((p_Params->rxFifoPriElevationLevel/BMI_FIFO_UNITS - 1) << BMI_RX_FIFO_PRI_ELEVATION_SHIFT);
    tmpReg |= ((p_Params->rxFifoThreshold/BMI_FIFO_UNITS - 1) << BMI_RX_FIFO_THRESHOLD_SHIFT);

    WRITE_UINT32(p_Regs->fmbm_rfp, tmpReg);

    {
#ifdef FM_NO_THRESHOLD_REG
         t_FmRevisionInfo        revInfo;

         FM_GetRevision(p_FmPort->h_Fm, &revInfo);
         if (revInfo.majorRev > 1)
#endif /* FM_NO_THRESHOLD_REG */
            /* always allow access to the extra resources */
            WRITE_UINT32(p_Regs->fmbm_reth, BMI_RX_FIFO_THRESHOLD_BC);
    }

     /* frame end parameters */
    tmpReg = 0;
    tmpReg |= ((uint32_t)p_Params->cheksumLastBytesIgnore << BMI_RX_FRAME_END_CS_IGNORE_SHIFT);
    tmpReg |= ((uint32_t)p_Params->cutBytesFromEnd<< BMI_RX_FRAME_END_CUT_SHIFT);

    WRITE_UINT32(p_Regs->fmbm_rfed, tmpReg);

    /* IC parameters */
    tmpReg = 0;
    tmpReg |= (((uint32_t)p_Params->intContext.extBufOffset/OFFSET_UNITS) << BMI_IC_TO_EXT_SHIFT);
    tmpReg |= (((uint32_t)p_Params->intContext.intContextOffset/OFFSET_UNITS) << BMI_IC_FROM_INT_SHIFT);
    tmpReg |= (((uint32_t)p_Params->intContext.size/OFFSET_UNITS)  << BMI_IC_SIZE_SHIFT);

    WRITE_UINT32(p_Regs->fmbm_ricp, tmpReg);

    if (!p_FmPort->imEn)
    {
        /* check if the largest external buffer pool is large enough */
        if(p_Params->bufMargins.startMargins + MIN_EXT_BUF_SIZE + p_Params->bufMargins.endMargins > p_FmPort->rxPoolsParams.largestBufSize)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("bufMargins.startMargins (%d) + minimum buf size (64) + bufMargins.endMargins (%d) is larger than maximum external buffer size (%d)",
                            p_Params->bufMargins.startMargins, p_Params->bufMargins.endMargins, p_FmPort->rxPoolsParams.largestBufSize));

        /* buffer margins */
        tmpReg = 0;
        tmpReg |= (((uint32_t)p_Params->bufMargins.startMargins) << BMI_EXT_BUF_MARG_START_SHIFT);
        tmpReg |= (((uint32_t)p_Params->bufMargins.endMargins) << BMI_EXT_BUF_MARG_END_SHIFT);

        WRITE_UINT32(p_Regs->fmbm_rebm, tmpReg);
    }


    if(p_FmPort->internalBufferOffset)
    {
        tmpReg = (uint32_t)((p_FmPort->internalBufferOffset % OFFSET_UNITS) ?
                            (p_FmPort->internalBufferOffset/OFFSET_UNITS + 1):
                            (p_FmPort->internalBufferOffset/OFFSET_UNITS));
        p_FmPort->internalBufferOffset = (uint8_t)(tmpReg * OFFSET_UNITS);
        WRITE_UINT32(p_Regs->fmbm_rim, tmpReg << BMI_IM_FOF_SHIFT);
    }

    /* NIA */
    if (p_FmPort->imEn)
        WRITE_UINT32(p_Regs->fmbm_rfne, NIA_ENG_FM_CTL | NIA_FM_CTL_AC_IND_MODE_RX);
    else
    {
        tmpReg = 0;
        if (p_Params->forwardReuseIntContext)
            tmpReg |= BMI_PORT_RFNE_FRWD_RPD;
        /* L3/L4 checksum verify is enabled by default. */
        /*tmpReg |= BMI_PORT_RFNE_FRWD_DCL4C;*/
        WRITE_UINT32(p_Regs->fmbm_rfne, tmpReg | NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME);
    }
    WRITE_UINT32(p_Regs->fmbm_rfene, NIA_ENG_QMI_ENQ | NIA_ORDER_RESTOR);

    /* command attribute */
    tmpReg = BMI_CMD_RX_MR_DEF;
    if (!p_FmPort->imEn)
    {
        tmpReg |= BMI_CMD_ATTR_ORDER;
        if(p_Params->syncReq)
            tmpReg |= BMI_CMD_ATTR_SYNC;
        tmpReg |= ((uint32_t)p_Params->color << BMI_CMD_ATTR_COLOR_SHIFT);
    }

    WRITE_UINT32(p_Regs->fmbm_rfca, tmpReg);

    /* default queues */
    if (!p_FmPort->imEn)
    {
        WRITE_UINT32(p_Regs->fmbm_rfqid, p_Params->dfltFqid);
        WRITE_UINT32(p_Regs->fmbm_refqid, p_Params->errFqid);
    }

    /* set counters */
    WRITE_UINT32(p_Regs->fmbm_rstc, BMI_COUNTERS_EN);

    performanceContersParams.taskCompVal    = (uint8_t)p_FmPort->tasks.num;
    performanceContersParams.queueCompVal   = 1;
    performanceContersParams.dmaCompVal     =(uint8_t) p_FmPort->openDmas.num;
    performanceContersParams.fifoCompVal    = p_FmPort->fifoBufs.num;
    if((err = FM_PORT_SetPerformanceCountersParams(p_FmPort, &performanceContersParams)) != E_OK)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    WRITE_UINT32(p_Regs->fmbm_rpc, BMI_COUNTERS_EN);

    /* error/status mask  - check that if discard OV is set, no
       discard is required for specific errors.*/
    WRITE_UINT32(p_Regs->fmbm_rfsdm, p_Params->errorsToDiscard);

    errorsToEnq = (RX_ERRS_TO_ENQ & ~p_Params->errorsToDiscard);
    WRITE_UINT32(p_Regs->fmbm_rfsem, errorsToEnq);

#ifdef FM_BMI_TO_RISC_ENQ_ERRATA_FMANc
    if((GET_UINT32(p_Regs->fmbm_rfene) && NIA_ENG_MASK)== NIA_ENG_FM_CTL)
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("NIA not supported at this stage"));
#endif /* FM_BMI_TO_RISC_ENQ_ERRATA_FMANc */

    return E_OK;
}

static t_Error BmiTxPortInit(t_FmPort *p_FmPort)
{
    t_FmPortTxBmiRegs   *p_Regs = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs;
    uint32_t            tmpReg;
    t_FmPortDriverParam *p_Params = p_FmPort->p_FmPortDriverParam;
    /*uint32_t            rateCountUnit;*/
    t_FmPortPerformanceCnt  performanceContersParams;

    /* check that port is not busy */
    if (GET_UINT32(p_Regs->fmbm_tcfg) & BMI_PORT_CFG_EN)
         RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Port is already enabled"));

    tmpReg = 0;
    if (p_FmPort->imEn)
        tmpReg |= BMI_PORT_CFG_IM;

    WRITE_UINT32(p_Regs->fmbm_tcfg, tmpReg);

    /* Configure dma attributes */
    tmpReg = 0;
    tmpReg |= (uint32_t)p_Params->dmaSwapData << BMI_DMA_ATTR_SWP_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaIntContextCacheAttr << BMI_DMA_ATTR_IC_CACHE_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaHeaderCacheAttr << BMI_DMA_ATTR_HDR_CACHE_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaScatterGatherCacheAttr << BMI_DMA_ATTR_SG_CACHE_SHIFT;

    WRITE_UINT32(p_Regs->fmbm_tda, tmpReg);

    /* Configure Tx Fifo params */
    tmpReg = 0;
    tmpReg |= ((p_Params->txFifoMinFillLevel/BMI_FIFO_UNITS) << BMI_TX_FIFO_MIN_FILL_SHIFT);
    tmpReg |= (((uint32_t)p_FmPort->txFifoDeqPipelineDepth - 1) << BMI_TX_FIFO_PIPELINE_DEPTH_SHIFT);
    tmpReg |= ((p_Params->txFifoLowComfLevel/BMI_FIFO_UNITS - 1) << BMI_TX_LOW_COMF_SHIFT);

    WRITE_UINT32(p_Regs->fmbm_tfp, tmpReg);

    /* frame end parameters */
    tmpReg = 0;
    tmpReg |= ((uint32_t)p_Params->cheksumLastBytesIgnore << BMI_TX_FRAME_END_CS_IGNORE_SHIFT);

    WRITE_UINT32(p_Regs->fmbm_tfed, tmpReg);

    if (!p_FmPort->imEn)
    {
        /* IC parameters */
        tmpReg = 0;
        tmpReg |= (((uint32_t)p_Params->intContext.extBufOffset/OFFSET_UNITS) << BMI_IC_TO_EXT_SHIFT);
        tmpReg |= (((uint32_t)p_Params->intContext.intContextOffset/OFFSET_UNITS) << BMI_IC_FROM_INT_SHIFT);
        tmpReg |= (((uint32_t)p_Params->intContext.size/OFFSET_UNITS)  << BMI_IC_SIZE_SHIFT);

        WRITE_UINT32(p_Regs->fmbm_ticp, tmpReg);
    }

    /* NIA */
    if (p_FmPort->imEn)
    {
        WRITE_UINT32(p_Regs->fmbm_tfne, NIA_ENG_FM_CTL | NIA_FM_CTL_AC_IND_MODE_TX);
        WRITE_UINT32(p_Regs->fmbm_tfene, NIA_ENG_FM_CTL | NIA_FM_CTL_AC_IND_MODE_TX);
    }
    else
    {
        WRITE_UINT32(p_Regs->fmbm_tfne, NIA_ENG_QMI_DEQ);
        WRITE_UINT32(p_Regs->fmbm_tfene, NIA_ENG_QMI_ENQ | NIA_ORDER_RESTOR);
        /* The line bellow is a trick so the FM will not release the buffer
           to BM nor will try to enq the frame to QM */
        if(!p_Params->dfltFqid && p_Params->dontReleaseBuf)
        {
            /* override fmbm_tcfqid 0 with a false non-0 value. This will force FM to
             * act acording to tfene. Otherwise, if fmbm_tcfqid is 0 the FM will release
             * buffers to BM regardless of fmbm_tfene
             */
            WRITE_UINT32(p_Regs->fmbm_tcfqid, 0xFFFFFF);
            WRITE_UINT32(p_Regs->fmbm_tfene, NIA_ENG_BMI | NIA_BMI_AC_TX_RELEASE);
        }
    }

    /* command attribute */
    tmpReg = BMI_CMD_TX_MR_DEF;
    if (p_FmPort->imEn)
        tmpReg |= BMI_CMD_MR_DEAS;
    else
    {
        tmpReg |= BMI_CMD_ATTR_ORDER;
        /* if we set syncReq, we may get stuck when HC command is running */
        /*if(p_Params->syncReq)
            tmpReg |= BMI_CMD_ATTR_SYNC;*/
        tmpReg |= ((uint32_t)p_Params->color << BMI_CMD_ATTR_COLOR_SHIFT);
    }

    WRITE_UINT32(p_Regs->fmbm_tfca, tmpReg);

    /* default queues */
    if (!p_FmPort->imEn)
    {
        if(p_Params->dfltFqid || !p_Params->dontReleaseBuf)
            WRITE_UINT32(p_Regs->fmbm_tcfqid, p_Params->dfltFqid);
        WRITE_UINT32(p_Regs->fmbm_tfeqid, p_Params->errFqid);
    }

    /* statistics & performance counters */
    WRITE_UINT32(p_Regs->fmbm_tstc, BMI_COUNTERS_EN);

    performanceContersParams.taskCompVal    = (uint8_t)p_FmPort->tasks.num;
    performanceContersParams.queueCompVal   = 1;
    performanceContersParams.dmaCompVal     = (uint8_t)p_FmPort->openDmas.num;
    performanceContersParams.fifoCompVal    = p_FmPort->fifoBufs.num;
    FM_PORT_SetPerformanceCountersParams(p_FmPort, &performanceContersParams);

    WRITE_UINT32(p_Regs->fmbm_tpc, BMI_COUNTERS_EN);

    return E_OK;
}

static t_Error BmiOhPortInit(t_FmPort *p_FmPort)
{
    t_FmPortOhBmiRegs       *p_Regs = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs;
    uint32_t                tmpReg, errorsToEnq = 0;
    t_FmPortDriverParam     *p_Params = p_FmPort->p_FmPortDriverParam;
    t_FmPortPerformanceCnt  performanceContersParams;
    t_Error                 err;

    /* check that port is not busy */
    if (GET_UINT32(p_Regs->fmbm_ocfg) & BMI_PORT_CFG_EN)
         RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Port is already enabled"));

    /* Configure dma attributes */
    tmpReg = 0;
    tmpReg |= (uint32_t)p_Params->dmaSwapData << BMI_DMA_ATTR_SWP_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaIntContextCacheAttr << BMI_DMA_ATTR_IC_CACHE_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaHeaderCacheAttr << BMI_DMA_ATTR_HDR_CACHE_SHIFT;
    tmpReg |= (uint32_t)p_Params->dmaScatterGatherCacheAttr << BMI_DMA_ATTR_SG_CACHE_SHIFT;
    if(p_Params->dmaWriteOptimize)
        tmpReg |= BMI_DMA_ATTR_WRITE_OPTIMIZE;

    WRITE_UINT32(p_Regs->fmbm_oda, tmpReg);

    /* IC parameters */
    tmpReg = 0;
    tmpReg |= (((uint32_t)p_Params->intContext.extBufOffset/OFFSET_UNITS) << BMI_IC_TO_EXT_SHIFT);
    tmpReg |= (((uint32_t)p_Params->intContext.intContextOffset/OFFSET_UNITS) << BMI_IC_FROM_INT_SHIFT);
    tmpReg |= (((uint32_t)p_Params->intContext.size/OFFSET_UNITS)  << BMI_IC_SIZE_SHIFT);

    WRITE_UINT32(p_Regs->fmbm_oicp, tmpReg);

    /* NIA */
    WRITE_UINT32(p_Regs->fmbm_ofdne, NIA_ENG_QMI_DEQ);

    if (p_FmPort->portType==e_FM_PORT_TYPE_OH_HOST_COMMAND)
        WRITE_UINT32(p_Regs->fmbm_ofene, NIA_ENG_QMI_ENQ);
    else
        WRITE_UINT32(p_Regs->fmbm_ofene, NIA_ENG_QMI_ENQ | NIA_ORDER_RESTOR);

    /* command attribute */
    if (p_FmPort->portType==e_FM_PORT_TYPE_OH_HOST_COMMAND)
        tmpReg =  BMI_CMD_MR_DEAS | BMI_CMD_MR_MA;
    else
        tmpReg = BMI_CMD_ATTR_ORDER | BMI_CMD_MR_DEAS | BMI_CMD_MR_MA;

    if(p_Params->syncReq)
        tmpReg |= BMI_CMD_ATTR_SYNC;
    tmpReg |= ((uint32_t)p_Params->color << BMI_CMD_ATTR_COLOR_SHIFT);
    WRITE_UINT32(p_Regs->fmbm_ofca, tmpReg);

    /* No discard - all error frames go to error queue */
    if (p_Params->frmDiscardOverride)
        tmpReg = BMI_PORT_CFG_FDOVR;
    else
        tmpReg = 0;
    WRITE_UINT32(p_Regs->fmbm_ocfg, tmpReg);

    if(p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING)
    {
        WRITE_UINT32(p_Regs->fmbm_ofsdm, p_Params->errorsToDiscard);

        errorsToEnq = (OP_ERRS_TO_ENQ & ~p_Params->errorsToDiscard);
        WRITE_UINT32(p_Regs->fmbm_ofsem, errorsToEnq);

        /* NIA */
        WRITE_UINT32(p_Regs->fmbm_ofne, NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME);
        {
#ifdef FM_NO_OP_OBSERVED_POOLS
            t_FmRevisionInfo        revInfo;

            FM_GetRevision(p_FmPort->h_Fm, &revInfo);
            if ((revInfo.majorRev == 4) && (p_Params->enBufPoolDepletion))
#endif /* FM_NO_OP_OBSERVED_POOLS */
            {
                /* define external buffer pools */
                err = SetExtBufferPools(p_FmPort);
                if(err)
                    RETURN_ERROR(MAJOR, err, NO_MSG);
            }
        }
    }
    else
        /* NIA */
        WRITE_UINT32(p_Regs->fmbm_ofne, NIA_ENG_FM_CTL | NIA_FM_CTL_AC_HC);

    /* default queues */
    WRITE_UINT32(p_Regs->fmbm_ofqid, p_Params->dfltFqid);
    WRITE_UINT32(p_Regs->fmbm_oefqid, p_Params->errFqid);

    if(p_FmPort->internalBufferOffset)
    {
        tmpReg = (uint32_t)((p_FmPort->internalBufferOffset % OFFSET_UNITS) ?
                            (p_FmPort->internalBufferOffset/OFFSET_UNITS + 1):
                            (p_FmPort->internalBufferOffset/OFFSET_UNITS));
        p_FmPort->internalBufferOffset = (uint8_t)(tmpReg * OFFSET_UNITS);
        WRITE_UINT32(p_Regs->fmbm_oim, tmpReg << BMI_IM_FOF_SHIFT);
    }
    /* statistics & performance counters */
    WRITE_UINT32(p_Regs->fmbm_ostc, BMI_COUNTERS_EN);

    performanceContersParams.taskCompVal    = (uint8_t)p_FmPort->tasks.num;
    performanceContersParams.queueCompVal   = 0;
    performanceContersParams.dmaCompVal     = (uint8_t)p_FmPort->openDmas.num;
    performanceContersParams.fifoCompVal    = p_FmPort->fifoBufs.num;
    FM_PORT_SetPerformanceCountersParams(p_FmPort, &performanceContersParams);

    WRITE_UINT32(p_Regs->fmbm_opc, BMI_COUNTERS_EN);

    return E_OK;
}

static t_Error QmiInit(t_FmPort *p_FmPort)
{
    t_FmPortDriverParam             *p_Params = NULL;
    uint32_t                        tmpReg;

    p_Params = p_FmPort->p_FmPortDriverParam;

    /* check that port is not busy */
    if(((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_RX)) &&
       (GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc) & QMI_PORT_CFG_EN))
         RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Port is already enabled"));

    /* enable & clear counters */
    WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc, QMI_PORT_CFG_EN_COUNTERS);

    /* The following is  done for non-Rx ports only */
    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_RX))
    {
        if((p_FmPort->portType == e_FM_PORT_TYPE_TX_10G) ||
                        (p_FmPort->portType == e_FM_PORT_TYPE_TX))
        {
            /* define dequeue NIA */
            WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndn, NIA_ENG_BMI | NIA_BMI_AC_TX);
            /* define enqueue NIA */
            WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnen, NIA_ENG_BMI | NIA_BMI_AC_TX_RELEASE);
        }
        else  /* for HC & OP */
        {
            WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndn, NIA_ENG_BMI | NIA_BMI_AC_FETCH);
            /* define enqueue NIA */
            WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnen, NIA_ENG_BMI | NIA_BMI_AC_RELEASE);
        }

        /* configure dequeue */
        tmpReg = 0;
        if(p_Params->deqHighPriority)
            tmpReg |= QMI_DEQ_CFG_PRI;

        switch(p_Params->deqType)
        {
            case(e_FM_PORT_DEQ_TYPE1):
                tmpReg |= QMI_DEQ_CFG_TYPE1;
                break;
            case(e_FM_PORT_DEQ_TYPE2):
                tmpReg |= QMI_DEQ_CFG_TYPE2;
                break;
            case(e_FM_PORT_DEQ_TYPE3):
                tmpReg |= QMI_DEQ_CFG_TYPE3;
                break;
            default:
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid dequeue type"));
        }

#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
        switch(p_Params->deqPrefetchOption)
        {
            case(e_FM_PORT_DEQ_NO_PREFETCH):
                /* Do nothing - QMI_DEQ_CFG_PREFETCH_WAITING_TNUM | QMI_DEQ_CFG_PREFETCH_1_FRAME = 0 */
                break;
            case(e_FM_PORT_DEQ_PARTIAL_PREFETCH):
                tmpReg |= QMI_DEQ_CFG_PREFETCH_WAITING_TNUM | QMI_DEQ_CFG_PREFETCH_3_FRAMES;
                break;
            case(e_FM_PORT_DEQ_FULL_PREFETCH):
                tmpReg |= QMI_DEQ_CFG_PREFETCH_NO_TNUM | QMI_DEQ_CFG_PREFETCH_3_FRAMES;
                break;
            default:
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid dequeue prefetch option"));
        }
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

        tmpReg |= p_Params->deqByteCnt;
        tmpReg |= (uint32_t)p_Params->deqSubPortal << QMI_DEQ_CFG_SUBPORTAL_SHIFT;

        WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndc, tmpReg);
    }
    else /* rx port */
        /* define enqueue NIA */
        WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnen, NIA_ENG_BMI | NIA_BMI_AC_RELEASE);

    return E_OK;
}

static t_Error BmiRxPortCheckAndGetCounterPtr(t_FmPort *p_FmPort, e_FmPortCounters counter, volatile uint32_t **p_Ptr)
{
    t_FmPortRxBmiRegs   *p_BmiRegs = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs;

     /* check that counters are enabled */
    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_CYCLE):
        case(e_FM_PORT_COUNTERS_TASK_UTIL):
        case(e_FM_PORT_COUNTERS_QUEUE_UTIL):
        case(e_FM_PORT_COUNTERS_DMA_UTIL):
        case(e_FM_PORT_COUNTERS_FIFO_UTIL):
        case(e_FM_PORT_COUNTERS_RX_PAUSE_ACTIVATION):
            /* performance counters - may be read when disabled */
            break;
        case(e_FM_PORT_COUNTERS_FRAME):
        case(e_FM_PORT_COUNTERS_DISCARD_FRAME):
        case(e_FM_PORT_COUNTERS_RX_BAD_FRAME):
        case(e_FM_PORT_COUNTERS_RX_LARGE_FRAME):
        case(e_FM_PORT_COUNTERS_RX_FILTER_FRAME):
        case(e_FM_PORT_COUNTERS_RX_LIST_DMA_ERR):
        case(e_FM_PORT_COUNTERS_RX_OUT_OF_BUFFERS_DISCARD):
        case(e_FM_PORT_COUNTERS_DEALLOC_BUF):
            if(!(GET_UINT32(p_BmiRegs->fmbm_rstc) & BMI_COUNTERS_EN))
               RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter was not enabled"));
            break;
         default:
            RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for Rx ports"));
    }

    /* Set counter */
    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_CYCLE):
            *p_Ptr = &p_BmiRegs->fmbm_rccn;
            break;
        case(e_FM_PORT_COUNTERS_TASK_UTIL):
            *p_Ptr = &p_BmiRegs->fmbm_rtuc;
            break;
        case(e_FM_PORT_COUNTERS_QUEUE_UTIL):
            *p_Ptr = &p_BmiRegs->fmbm_rrquc;
            break;
        case(e_FM_PORT_COUNTERS_DMA_UTIL):
            *p_Ptr = &p_BmiRegs->fmbm_rduc;
            break;
        case(e_FM_PORT_COUNTERS_FIFO_UTIL):
            *p_Ptr = &p_BmiRegs->fmbm_rfuc;
            break;
        case(e_FM_PORT_COUNTERS_RX_PAUSE_ACTIVATION):
            *p_Ptr = &p_BmiRegs->fmbm_rpac;
            break;
        case(e_FM_PORT_COUNTERS_FRAME):
            *p_Ptr = &p_BmiRegs->fmbm_rfrc;
            break;
        case(e_FM_PORT_COUNTERS_DISCARD_FRAME):
            *p_Ptr = &p_BmiRegs->fmbm_rfcd;
            break;
        case(e_FM_PORT_COUNTERS_RX_BAD_FRAME):
            *p_Ptr = &p_BmiRegs->fmbm_rfbc;
            break;
        case(e_FM_PORT_COUNTERS_RX_LARGE_FRAME):
            *p_Ptr = &p_BmiRegs->fmbm_rlfc;
            break;
        case(e_FM_PORT_COUNTERS_RX_FILTER_FRAME):
            *p_Ptr = &p_BmiRegs->fmbm_rffc;
            break;
        case(e_FM_PORT_COUNTERS_RX_LIST_DMA_ERR):
#ifdef FM_PORT_COUNTERS_ERRATA_FMANg
            {
                t_FmRevisionInfo revInfo;
                FM_GetRevision(p_FmPort->h_Fm, &revInfo);
                if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                    RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("Requested counter is not available in rev1"));
            }
#endif /* FM_PORT_COUNTERS_ERRATA_FMANg */
            *p_Ptr = &p_BmiRegs->fmbm_rfldec;
            break;
        case(e_FM_PORT_COUNTERS_RX_OUT_OF_BUFFERS_DISCARD):
            *p_Ptr = &p_BmiRegs->fmbm_rodc;
            break;
        case(e_FM_PORT_COUNTERS_DEALLOC_BUF):
            *p_Ptr = &p_BmiRegs->fmbm_rbdc;
            break;
        default:
            RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for Rx ports"));
    }

    return E_OK;
}

static t_Error BmiTxPortCheckAndGetCounterPtr(t_FmPort *p_FmPort, e_FmPortCounters counter, volatile uint32_t **p_Ptr)
{
    t_FmPortTxBmiRegs   *p_BmiRegs = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs;

     /* check that counters are enabled */
    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_CYCLE):
        case(e_FM_PORT_COUNTERS_TASK_UTIL):
        case(e_FM_PORT_COUNTERS_QUEUE_UTIL):
        case(e_FM_PORT_COUNTERS_DMA_UTIL):
        case(e_FM_PORT_COUNTERS_FIFO_UTIL):
            /* performance counters - may be read when disabled */
            break;
        case(e_FM_PORT_COUNTERS_FRAME):
        case(e_FM_PORT_COUNTERS_DISCARD_FRAME):
        case(e_FM_PORT_COUNTERS_LENGTH_ERR):
        case(e_FM_PORT_COUNTERS_UNSUPPRTED_FORMAT):
        case(e_FM_PORT_COUNTERS_DEALLOC_BUF):
            if(!(GET_UINT32(p_BmiRegs->fmbm_tstc) & BMI_COUNTERS_EN))
               RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter was not enabled"));
            break;
        default:
            RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for Tx ports"));
    }

    /* Set counter */
    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_CYCLE):
           *p_Ptr = &p_BmiRegs->fmbm_tccn;
            break;
        case(e_FM_PORT_COUNTERS_TASK_UTIL):
           *p_Ptr = &p_BmiRegs->fmbm_ttuc;
            break;
        case(e_FM_PORT_COUNTERS_QUEUE_UTIL):
            *p_Ptr = &p_BmiRegs->fmbm_ttcquc;
            break;
        case(e_FM_PORT_COUNTERS_DMA_UTIL):
           *p_Ptr = &p_BmiRegs->fmbm_tduc;
            break;
        case(e_FM_PORT_COUNTERS_FIFO_UTIL):
           *p_Ptr = &p_BmiRegs->fmbm_tfuc;
            break;
        case(e_FM_PORT_COUNTERS_FRAME):
           *p_Ptr = &p_BmiRegs->fmbm_tfrc;
            break;
        case(e_FM_PORT_COUNTERS_DISCARD_FRAME):
           *p_Ptr = &p_BmiRegs->fmbm_tfdc;
            break;
        case(e_FM_PORT_COUNTERS_LENGTH_ERR):
           *p_Ptr = &p_BmiRegs->fmbm_tfledc;
            break;
        case(e_FM_PORT_COUNTERS_UNSUPPRTED_FORMAT):
            *p_Ptr = &p_BmiRegs->fmbm_tfufdc;
            break;
        case(e_FM_PORT_COUNTERS_DEALLOC_BUF):
            *p_Ptr = &p_BmiRegs->fmbm_tbdc;
            break;
        default:
            RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for Tx ports"));
    }

    return E_OK;
}

static t_Error BmiOhPortCheckAndGetCounterPtr(t_FmPort *p_FmPort, e_FmPortCounters counter, volatile uint32_t **p_Ptr)
{
    t_FmPortOhBmiRegs   *p_BmiRegs = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs;

    /* check that counters are enabled */
    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_CYCLE):
        case(e_FM_PORT_COUNTERS_TASK_UTIL):
        case(e_FM_PORT_COUNTERS_DMA_UTIL):
        case(e_FM_PORT_COUNTERS_FIFO_UTIL):
            /* performance counters - may be read when disabled */
            break;
        case(e_FM_PORT_COUNTERS_FRAME):
        case(e_FM_PORT_COUNTERS_DISCARD_FRAME):
        case(e_FM_PORT_COUNTERS_RX_LIST_DMA_ERR):
        case(e_FM_PORT_COUNTERS_WRED_DISCARD):
        case(e_FM_PORT_COUNTERS_LENGTH_ERR):
        case(e_FM_PORT_COUNTERS_UNSUPPRTED_FORMAT):
        case(e_FM_PORT_COUNTERS_DEALLOC_BUF):
            if(!(GET_UINT32(p_BmiRegs->fmbm_ostc) & BMI_COUNTERS_EN))
               RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter was not enabled"));
            break;
        case(e_FM_PORT_COUNTERS_RX_FILTER_FRAME): /* only valid for offline parsing */
            if(p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND)
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for Host Command ports"));
            if(!(GET_UINT32(p_BmiRegs->fmbm_ostc) & BMI_COUNTERS_EN))
               RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter was not enabled"));
            break;
        default:
            RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for O/H ports"));
    }

    /* Set counter */
    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_CYCLE):
           *p_Ptr = &p_BmiRegs->fmbm_occn;
            break;
        case(e_FM_PORT_COUNTERS_TASK_UTIL):
           *p_Ptr = &p_BmiRegs->fmbm_otuc;
            break;
        case(e_FM_PORT_COUNTERS_DMA_UTIL):
           *p_Ptr = &p_BmiRegs->fmbm_oduc;
            break;
        case(e_FM_PORT_COUNTERS_FIFO_UTIL):
           *p_Ptr = &p_BmiRegs->fmbm_ofuc;
            break;
        case(e_FM_PORT_COUNTERS_FRAME):
           *p_Ptr = &p_BmiRegs->fmbm_ofrc;
            break;
        case(e_FM_PORT_COUNTERS_DISCARD_FRAME):
           *p_Ptr = &p_BmiRegs->fmbm_ofdc;
            break;
        case(e_FM_PORT_COUNTERS_RX_FILTER_FRAME):
           *p_Ptr = &p_BmiRegs->fmbm_offc;
            break;
        case(e_FM_PORT_COUNTERS_RX_LIST_DMA_ERR):
#ifdef FM_PORT_COUNTERS_ERRATA_FMANg
        {
            t_FmRevisionInfo revInfo;
            FM_GetRevision(p_FmPort->h_Fm, &revInfo);
            if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("Requested counter is not available in rev1"));
        }
#endif /* FM_PORT_COUNTERS_ERRATA_FMANg */
          *p_Ptr = &p_BmiRegs->fmbm_ofldec;
            break;
        case(e_FM_PORT_COUNTERS_WRED_DISCARD):
           *p_Ptr = &p_BmiRegs->fmbm_ofwdc;
            break;
        case(e_FM_PORT_COUNTERS_LENGTH_ERR):
           *p_Ptr = &p_BmiRegs->fmbm_ofledc;
            break;
        case(e_FM_PORT_COUNTERS_UNSUPPRTED_FORMAT):
            *p_Ptr = &p_BmiRegs->fmbm_ofufdc;
            break;
        case(e_FM_PORT_COUNTERS_DEALLOC_BUF):
            *p_Ptr = &p_BmiRegs->fmbm_obdc;
            break;
        default:
            RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for O/H ports"));
    }

    return E_OK;
}

static t_Error  AdditionalPrsParams(t_FmPort *p_FmPort, t_FmPcdPrsAdditionalHdrParams *p_HdrParams, uint32_t *p_SoftSeqAttachReg)
{
    uint8_t                     hdrNum, Ipv4HdrNum;
    u_FmPcdHdrPrsOpts           *p_prsOpts;
    uint32_t                    tmpReg = 0, tmpPrsOffset;

    if(IS_PRIVATE_HEADER(p_HdrParams->hdr) || IS_SPECIAL_HEADER(p_HdrParams->hdr))
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("No additional parameters for private or special headers."));

    if(p_HdrParams->errDisable)
        tmpReg |= PRS_HDR_ERROR_DIS;

    /* Set parser options */
    if(p_HdrParams->usePrsOpts)
    {
        p_prsOpts = &p_HdrParams->prsOpts;
        switch(p_HdrParams->hdr)
        {
            case(HEADER_TYPE_MPLS):
                if(p_prsOpts->mplsPrsOptions.labelInterpretationEnable)
                    tmpReg |= PRS_HDR_MPLS_LBL_INTER_EN;
                GET_PRS_HDR_NUM(hdrNum, p_prsOpts->mplsPrsOptions.nextParse);
                if(hdrNum == ILLEGAL_HDR_NUM)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, NO_MSG);
                GET_PRS_HDR_NUM(Ipv4HdrNum, HEADER_TYPE_IPv4);
                if(hdrNum < Ipv4HdrNum)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                        ("Header must be equal or higher than IPv4"));
                tmpReg |= ((uint32_t)hdrNum * PRS_HDR_ENTRY_SIZE) << PRS_HDR_MPLS_NEXT_HDR_SHIFT;
                break;
            case(HEADER_TYPE_PPPoE):
                if(p_prsOpts->pppoePrsOptions.enableMTUCheck)
                {
#ifdef FM_PPPOE_NO_MTU_CHECK
                    t_FmRevisionInfo revInfo;
                    FM_GetRevision(p_FmPort->h_Fm, &revInfo);
                    if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Invalid parser option"));
                    else
#endif /* FM_PPPOE_NO_MTU_CHECK */
                        tmpReg |= PRS_HDR_PPPOE_MTU_CHECK_EN;
                }
                break;
            case(HEADER_TYPE_IPv6):
                if(p_prsOpts->ipv6PrsOptions.routingHdrDisable)
                    tmpReg |= PRS_HDR_IPV6_ROUTE_HDR_DIS;
                break;
            case(HEADER_TYPE_TCP):
                if(p_prsOpts->tcpPrsOptions.padIgnoreChecksum)
                   tmpReg |= PRS_HDR_TCP_PAD_REMOVAL;
                break;
            case(HEADER_TYPE_UDP):
                if(p_prsOpts->udpPrsOptions.padIgnoreChecksum)
                   tmpReg |= PRS_HDR_TCP_PAD_REMOVAL;
                break;
            default:
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid header"));
        }
    }

    /* set software parsing (address is devided in 2 since parser uses 2 byte access. */
    if(p_HdrParams->swPrsEnable)
    {
        tmpPrsOffset = FmPcdGetSwPrsOffset(p_FmPort->h_FmPcd, p_HdrParams->hdr, p_HdrParams->indexPerHdr);
        if(tmpPrsOffset == ILLEGAL_BASE)
            RETURN_ERROR(MINOR, E_INVALID_VALUE, NO_MSG);
        tmpReg |= (PRS_HDR_SW_PRS_EN | tmpPrsOffset);
    }
    *p_SoftSeqAttachReg = tmpReg;

    return E_OK;
}

static uint32_t GetPortSchemeBindParams(t_Handle h_FmPort, t_FmPcdKgInterModuleBindPortToSchemes *p_SchemeBind)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t                    walking1Mask = 0x80000000, tmp;
    uint8_t                     idx = 0;

    p_SchemeBind->netEnvId = p_FmPort->netEnvId;
    p_SchemeBind->hardwarePortId = p_FmPort->hardwarePortId;
    p_SchemeBind->useClsPlan = p_FmPort->useClsPlan;
    p_SchemeBind->numOfSchemes = 0;
    tmp = p_FmPort->schemesPerPortVector;
    if(tmp)
    {
        while (tmp)
        {
            if(tmp & walking1Mask)
            {
                p_SchemeBind->schemesIds[p_SchemeBind->numOfSchemes] = FmPcdKgGetSchemeSwId(p_FmPort->h_FmPcd, idx);
                p_SchemeBind->numOfSchemes++;
                tmp &= ~walking1Mask;
            }
            walking1Mask >>= 1;
            idx++;
        }
    }

    return tmp;
}

static t_Error BuildBufferStructure(t_FmPort *p_FmPort)
{
    uint32_t                        tmp;

    ASSERT_COND(p_FmPort);

    /* Align start of internal context data to 16 byte */
    p_FmPort->p_FmPortDriverParam->intContext.extBufOffset =
        (uint16_t)((p_FmPort->p_FmPortDriverParam->bufferPrefixContent.privDataSize & (OFFSET_UNITS-1)) ?
            ((p_FmPort->p_FmPortDriverParam->bufferPrefixContent.privDataSize + OFFSET_UNITS) & ~(uint16_t)(OFFSET_UNITS-1)) :
             p_FmPort->p_FmPortDriverParam->bufferPrefixContent.privDataSize);

    /* Translate margin and intContext params to FM parameters */
#ifdef FM_INCORRECT_CS_ERRATA_FMAN18
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        {
            uint8_t mod = p_FmPort->p_FmPortDriverParam->bufferPrefixContent.dataAlign % 256;
            if(mod)
            {
                p_FmPort->p_FmPortDriverParam->bufferPrefixContent.dataAlign += (256-mod);
                DBG(WARNING,("dataAlign modified to next 256 to conform with FMAN18 errata\n"));
            }
        }
    }
#endif /* FM_INCORRECT_CS_ERRATA_FMAN18 */

    /* Initialize with illegal value. Later we'll set legal values. */
    p_FmPort->bufferOffsets.prsResultOffset = (uint32_t)ILLEGAL_BASE;
    p_FmPort->bufferOffsets.timeStampOffset = (uint32_t)ILLEGAL_BASE;
    p_FmPort->bufferOffsets.hashResultOffset= (uint32_t)ILLEGAL_BASE;
    p_FmPort->bufferOffsets.pcdInfoOffset   = (uint32_t)ILLEGAL_BASE;
#ifdef DEBUG
    p_FmPort->bufferOffsets.debugOffset     = (uint32_t)ILLEGAL_BASE;
#endif /* DEBUG */

    /* Internally the driver supports 4 options
       1. prsResult/timestamp/hashResult selection (in fact 8 options, but for simplicity we'll
          relate to it as 1).
       2. All IC context (from AD) except debug.
       3. Debug information only.
       4. All IC context (from AD) including debug.
       Note, that if user asks for prsResult/timestamp/hashResult and Debug, we give them (4) */

    /* This 'if' covers options  2 & 4. We copy from beginning of context with or without debug. */
    /* If passAllOtherPCDInfo explicitly requested, or passDebugInfo+prs/ts --> we also take passAllOtherPCDInfo */
    if ((p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passAllOtherPCDInfo)
#ifdef DEBUG
        || (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passDebugInfo &&
         (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult ||
          p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passTimeStamp ||
          p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passHashResult))
#endif /* DEBUG */
       )
    {
#ifdef DEBUG
        if(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passDebugInfo)
        {
            p_FmPort->p_FmPortDriverParam->intContext.size = 240;
            p_FmPort->bufferOffsets.debugOffset =
                (uint32_t)(p_FmPort->p_FmPortDriverParam->intContext.extBufOffset + 128);
        }
        else
#endif /* DEBUG */
            p_FmPort->p_FmPortDriverParam->intContext.size = 128; /* must be aligned to 16 */
        /* Start copying data after 16 bytes (FD) from the beginning of the internal context */
        p_FmPort->p_FmPortDriverParam->intContext.intContextOffset = 16;

        if (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passAllOtherPCDInfo)
            p_FmPort->bufferOffsets.pcdInfoOffset = p_FmPort->p_FmPortDriverParam->intContext.extBufOffset;
        if (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult)
            p_FmPort->bufferOffsets.prsResultOffset =
                (uint32_t)(p_FmPort->p_FmPortDriverParam->intContext.extBufOffset + 16);
        if (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passTimeStamp)
            p_FmPort->bufferOffsets.timeStampOffset =
                (uint32_t)(p_FmPort->p_FmPortDriverParam->intContext.extBufOffset + 48);
        if (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passHashResult)
            p_FmPort->bufferOffsets.hashResultOffset =
                (uint32_t)(p_FmPort->p_FmPortDriverParam->intContext.extBufOffset + 56);
    }
    else
    {
#ifdef DEBUG
        if (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passDebugInfo)
        {
            /* This case covers option 3 */
            p_FmPort->p_FmPortDriverParam->intContext.size = 112;
            p_FmPort->p_FmPortDriverParam->intContext.intContextOffset = 144;
            p_FmPort->bufferOffsets.debugOffset = p_FmPort->p_FmPortDriverParam->intContext.extBufOffset;
        }
        else
#endif /* DEBUG */
        {
            /* This case covers the options under 1 */
            /* Copy size must be in 16-byte granularity. */
            p_FmPort->p_FmPortDriverParam->intContext.size =
                (uint16_t)((p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult ? 32 : 0) +
                          ((p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passTimeStamp ||
                          p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passHashResult) ? 16 : 0));

            /* Align start of internal context data to 16 byte */
            p_FmPort->p_FmPortDriverParam->intContext.intContextOffset =
                (uint8_t)(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult ? 32 :
                          ((p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passTimeStamp  ||
                           p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passHashResult) ? 64 : 0));

            if(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult)
                p_FmPort->bufferOffsets.prsResultOffset = p_FmPort->p_FmPortDriverParam->intContext.extBufOffset;
            if(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passTimeStamp)
                p_FmPort->bufferOffsets.timeStampOffset =  p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult ?
                                            (p_FmPort->p_FmPortDriverParam->intContext.extBufOffset + sizeof(t_FmPrsResult)) :
                                            p_FmPort->p_FmPortDriverParam->intContext.extBufOffset;
            if(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passHashResult)
                /* If PR is not requested, whether TS is requested or not, IC will be copied from TS */
                p_FmPort->bufferOffsets.hashResultOffset = p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult ?
                                              (p_FmPort->p_FmPortDriverParam->intContext.extBufOffset + sizeof(t_FmPrsResult) + 8) :
                                              p_FmPort->p_FmPortDriverParam->intContext.extBufOffset + 8;
        }
    }

    p_FmPort->p_FmPortDriverParam->bufMargins.startMargins =
        (uint16_t)(p_FmPort->p_FmPortDriverParam->intContext.extBufOffset +
                   p_FmPort->p_FmPortDriverParam->intContext.size);
#ifdef FM_CAPWAP_SUPPORT
    /* save extra space for manip in both external and internal buffers */
    if(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.manipExtraSpace)
    {
        if((p_FmPort->p_FmPortDriverParam->bufferPrefixContent.manipExtraSpace + FRAG_EXTRA_SPACE) >= 256)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("p_FmPort->p_FmPortDriverParam->bufferPrefixContent.manipExtraSpace + 32 can not be equal or larger to 256"));
        p_FmPort->bufferOffsets.manipOffset = p_FmPort->p_FmPortDriverParam->bufMargins.startMargins;
        p_FmPort->p_FmPortDriverParam->bufMargins.startMargins += (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.manipExtraSpace + FRAG_EXTRA_SPACE);
        p_FmPort->p_FmPortDriverParam->internalBufferOffset =
            (uint8_t)(p_FmPort->p_FmPortDriverParam->bufferPrefixContent.manipExtraSpace + FRAG_EXTRA_SPACE);
    }
#endif /* FM_CAPWAP_SUPPORT */

    /* align data start */
    tmp = (uint32_t)(p_FmPort->p_FmPortDriverParam->bufMargins.startMargins %
                     p_FmPort->p_FmPortDriverParam->bufferPrefixContent.dataAlign);
    if (tmp)
        p_FmPort->p_FmPortDriverParam->bufMargins.startMargins += (p_FmPort->p_FmPortDriverParam->bufferPrefixContent.dataAlign-tmp);
    p_FmPort->bufferOffsets.dataOffset = p_FmPort->p_FmPortDriverParam->bufMargins.startMargins;
    p_FmPort->internalBufferOffset = p_FmPort->p_FmPortDriverParam->internalBufferOffset;

    return E_OK;
}

static t_Error SetPcd(t_Handle h_FmPort, t_FmPortPcdParams *p_PcdParams)
{
    t_FmPort                            *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error                             err = E_OK;
    uint32_t                            tmpReg;
    volatile uint32_t                   *p_BmiNia=NULL;
    volatile uint32_t                   *p_BmiPrsNia=NULL;
    volatile uint32_t                   *p_BmiPrsStartOffset=NULL;
    volatile uint32_t                   *p_BmiInitPrsResult=NULL;
    volatile uint32_t                   *p_BmiCcBase=NULL;
    uint8_t                             hdrNum, L3HdrNum, greHdrNum;
    int                                 i;
    bool                                isEmptyClsPlanGrp;
    uint32_t                            tmpHxs[FM_PCD_PRS_NUM_OF_HDRS];
    uint16_t                            absoluteProfileId;
    uint8_t                             physicalSchemeId;
    uint32_t                            ccTreePhysOffset;
    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for non-independant mode ports only"));

    if ((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_RX) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));

    p_FmPort->netEnvId = (uint8_t)(PTR_TO_UINT(p_PcdParams->h_NetEnv)-1);

    p_FmPort->pcdEngines = 0;

    /* initialize p_FmPort->pcdEngines field in port's structure */
    switch(p_PcdParams->pcdSupport)
    {
        case(e_FM_PORT_PCD_SUPPORT_NONE):
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("No PCD configuration required if e_FM_PORT_PCD_SUPPORT_NONE selected"));
        case(e_FM_PORT_PCD_SUPPORT_PRS_ONLY):
            p_FmPort->pcdEngines |= FM_PCD_PRS;
            break;
        case(e_FM_PORT_PCD_SUPPORT_PLCR_ONLY):
            if (CHECK_FM_CTL_AC_POST_FETCH_PCD(p_FmPort->savedBmiNia))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("parser support is required"));
            p_FmPort->pcdEngines |= FM_PCD_PLCR;
            break;
        case(e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR):
            p_FmPort->pcdEngines |= FM_PCD_PRS;
            p_FmPort->pcdEngines |= FM_PCD_PLCR;
            break;
        case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG):
            p_FmPort->pcdEngines |= FM_PCD_PRS;
            p_FmPort->pcdEngines |= FM_PCD_KG;
            break;
        case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC):
            p_FmPort->pcdEngines |= FM_PCD_PRS;
            p_FmPort->pcdEngines |= FM_PCD_CC;
            p_FmPort->pcdEngines |= FM_PCD_KG;
            break;
        case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC_AND_PLCR):
            p_FmPort->pcdEngines |= FM_PCD_PRS;
            p_FmPort->pcdEngines |= FM_PCD_KG;
            p_FmPort->pcdEngines |= FM_PCD_CC;
            p_FmPort->pcdEngines |= FM_PCD_PLCR;
            break;
        case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_PLCR):
            p_FmPort->pcdEngines |= FM_PCD_PRS;
            p_FmPort->pcdEngines |= FM_PCD_KG;
            p_FmPort->pcdEngines |= FM_PCD_PLCR;
            break;

#ifdef FM_CAPWAP_SUPPORT
        case(e_FM_PORT_PCD_SUPPORT_CC_ONLY):
            if (CHECK_FM_CTL_AC_POST_FETCH_PCD(p_FmPort->savedBmiNia))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("parser support is required"));
            p_FmPort->pcdEngines |= FM_PCD_CC;
            break;
        case(e_FM_PORT_PCD_SUPPORT_CC_AND_KG):
            if (CHECK_FM_CTL_AC_POST_FETCH_PCD(p_FmPort->savedBmiNia))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("parser support is required"));
            p_FmPort->pcdEngines |= FM_PCD_CC;
            p_FmPort->pcdEngines |= FM_PCD_KG;
            break;
        case(e_FM_PORT_PCD_SUPPORT_CC_AND_KG_AND_PLCR):
            if (CHECK_FM_CTL_AC_POST_FETCH_PCD(p_FmPort->savedBmiNia))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("parser support is required"));
            p_FmPort->pcdEngines |= FM_PCD_CC;
            p_FmPort->pcdEngines |= FM_PCD_KG;
            p_FmPort->pcdEngines |= FM_PCD_PLCR;
            break;
#endif /* FM_CAPWAP_SUPPORT */
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("invalid pcdSupport"));
    }

    if((p_FmPort->pcdEngines & FM_PCD_PRS) && (p_PcdParams->p_PrsParams->numOfHdrsWithAdditionalParams > FM_PCD_PRS_NUM_OF_HDRS))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Port parser numOfHdrsWithAdditionalParams may not exceed %d", FM_PCD_PRS_NUM_OF_HDRS));

    /* check that parameters exist for each and only each defined engine */
    if((!!(p_FmPort->pcdEngines & FM_PCD_PRS) != !!p_PcdParams->p_PrsParams) ||
        (!!(p_FmPort->pcdEngines & FM_PCD_KG) != !!p_PcdParams->p_KgParams) ||
        (!!(p_FmPort->pcdEngines & FM_PCD_CC) != !!p_PcdParams->p_CcParams))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("PCD initialization structure is not consistant with pcdSupport"));

    /* get PCD registers pointers */
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne;
            p_BmiPrsNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfpne;
            p_BmiPrsStartOffset = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rpso;
            p_BmiInitPrsResult = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rprai[0];
            p_BmiCcBase = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rccb;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofne;
            p_BmiPrsNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofpne;
            p_BmiPrsStartOffset = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_opso;
            p_BmiInitPrsResult = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_oprai[0];
            p_BmiCcBase = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_occb;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    if(p_FmPort->pcdEngines & FM_PCD_KG)
    {

        if(p_PcdParams->p_KgParams->numOfSchemes == 0)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("For ports using Keygen, at lease one scheme must be bound. "));
        /* for each scheme */
        for(i = 0; i<p_PcdParams->p_KgParams->numOfSchemes; i++)
        {
            physicalSchemeId = (uint8_t)(PTR_TO_UINT(p_PcdParams->p_KgParams->h_Schemes[i])-1);
            /* build vector */
            p_FmPort->schemesPerPortVector |= 1 << (31 - (uint32_t)physicalSchemeId);
        }

        err = FmPcdKgSetOrBindToClsPlanGrp(p_FmPort->h_FmPcd,
                                            p_FmPort->hardwarePortId,
                                            p_FmPort->netEnvId,
                                            p_FmPort->optArray,
                                            &p_FmPort->clsPlanGrpId,
                                            &isEmptyClsPlanGrp);
         if(err)
             RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("FmPcdKgSetOrBindToClsPlanGrp failed. "));

         p_FmPort->useClsPlan = !isEmptyClsPlanGrp;
    }

    /* set PCD port parameter */
    if(p_FmPort->pcdEngines & FM_PCD_CC)
    {
        err = FmPcdCcBindTree(p_FmPort->h_FmPcd, p_PcdParams->p_CcParams->h_CcTree, &ccTreePhysOffset, h_FmPort);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);

        WRITE_UINT32(*p_BmiCcBase, ccTreePhysOffset);
        p_FmPort->ccTreeId = p_PcdParams->p_CcParams->h_CcTree;
    }

    /***************************/
    /* configure NIA after BMI */
    /***************************/
    if (!CHECK_FM_CTL_AC_POST_FETCH_PCD(p_FmPort->savedBmiNia))
        /* rfne may contain FDCS bits, so first we read them. */
        p_FmPort->savedBmiNia = GET_UINT32(*p_BmiNia) & BMI_RFNE_FDCS_MASK;

    /* If policer is used directly after BMI or PRS */
    if((p_FmPort->pcdEngines & FM_PCD_PLCR) &&
        ((p_PcdParams->pcdSupport == e_FM_PORT_PCD_SUPPORT_PLCR_ONLY) ||
                (p_PcdParams->pcdSupport == e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR)))
    {
        absoluteProfileId = (uint16_t)(PTR_TO_UINT(p_PcdParams->p_PlcrParams->h_Profile)-1);

        if(!FmPcdPlcrIsProfileValid(p_FmPort->h_FmPcd, absoluteProfileId))
            RETURN_ERROR(MINOR, E_INVALID_STATE, ("Private port profile not valid."));

        tmpReg = (uint32_t)(absoluteProfileId | NIA_PLCR_ABSOLUTE);

        if(p_FmPort->pcdEngines & FM_PCD_PRS) /* e_FM_PCD_SUPPORT_PRS_AND_PLCR */
        {
            /* update BMI HPNIA */
            WRITE_UINT32(*p_BmiPrsNia, (uint32_t)(NIA_ENG_PLCR | tmpReg));
        }
        else /* e_FM_PCD_SUPPORT_PLCR_ONLY */
            /* update BMI NIA */
            p_FmPort->savedBmiNia |= (uint32_t)(NIA_ENG_PLCR);
    }

#ifdef FM_CAPWAP_SUPPORT
    /* if CC is used directly after BMI */
    if((p_PcdParams->pcdSupport == e_FM_PORT_PCD_SUPPORT_CC_ONLY) ||
        (p_PcdParams->pcdSupport == e_FM_PORT_PCD_SUPPORT_CC_AND_KG) ||
        (p_PcdParams->pcdSupport == e_FM_PORT_PCD_SUPPORT_CC_AND_KG_AND_PLCR))
    {
        if (p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING)
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("e_FM_PORT_PCD_SUPPORT_CC_xx available for offline parsing ports only"));
        p_FmPort->savedBmiNia |= (uint32_t)(NIA_ENG_FM_CTL | NIA_FM_CTL_AC_CC);
         /* check that prs start offset == RIM[FOF] */
    }
#endif /* FM_CAPWAP_SUPPORT */

    if (p_FmPort->pcdEngines & FM_PCD_PRS)
    {
        ASSERT_COND(p_PcdParams->p_PrsParams);
        /* if PRS is used it is always first */
        GET_PRS_HDR_NUM(hdrNum, p_PcdParams->p_PrsParams->firstPrsHdr);
        if (hdrNum == ILLEGAL_HDR_NUM)
            RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Unsupported header."));
        if (!CHECK_FM_CTL_AC_POST_FETCH_PCD(p_FmPort->savedBmiNia))
            p_FmPort->savedBmiNia |= (uint32_t)(NIA_ENG_PRS | (uint32_t)(hdrNum));
        /* set after parser NIA */
        tmpReg = 0;
        switch(p_PcdParams->pcdSupport)
        {
            case(e_FM_PORT_PCD_SUPPORT_PRS_ONLY):
                WRITE_UINT32(*p_BmiPrsNia, NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME);
                break;
            case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC):
            case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC_AND_PLCR):
                tmpReg = NIA_KG_CC_EN;
            case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG):
            case(e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_PLCR):
                if(p_PcdParams->p_KgParams->directScheme)
                {
                    physicalSchemeId = (uint8_t)(PTR_TO_UINT(p_PcdParams->p_KgParams->h_DirectScheme)-1);
                    /* check that this scheme was bound to this port */
                    for(i=0 ; i<p_PcdParams->p_KgParams->numOfSchemes; i++)
                        if(p_PcdParams->p_KgParams->h_DirectScheme == p_PcdParams->p_KgParams->h_Schemes[i])
                            break;
                    if(i == p_PcdParams->p_KgParams->numOfSchemes)
                        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Direct scheme is not one of the port selected schemes."));
                    tmpReg |= (uint32_t)(NIA_KG_DIRECT | physicalSchemeId);
                }
                WRITE_UINT32(*p_BmiPrsNia, NIA_ENG_KG | tmpReg);
                break;
            case(e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR):
                break;
            default:
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid PCD support"));
        }

        /* set start parsing offset */
        /* WRITE_UINT32(*p_BmiPrsStartOffset, p_PcdParams->p_PrsParams->parsingOffset); */

        /************************************/
        /* Parser port parameters           */
        /************************************/
        /* stop before configuring */
        WRITE_UINT32(p_FmPort->p_FmPortPrsRegs->pcac, PRS_CAC_STOP);
        /* wait for parser to be in idle state */
        while(GET_UINT32(p_FmPort->p_FmPortPrsRegs->pcac) & PRS_CAC_ACTIVE) ;

        /* set soft seq attachment register */
        memset(tmpHxs, 0, FM_PCD_PRS_NUM_OF_HDRS*sizeof(uint32_t));

        /* set protocol options */
        for(i=0;p_FmPort->optArray[i];i++)
            switch(p_FmPort->optArray[i])
            {
                case(ETH_BROADCAST):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_ETH)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_ETH_BC_SHIFT;
                    break;
                case(ETH_MULTICAST):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_ETH)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_ETH_MC_SHIFT;
                    break;
                case(VLAN_STACKED):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_VLAN)
                    tmpHxs[hdrNum] |= (i+1)<< PRS_HDR_VLAN_STACKED_SHIFT;
                    break;
                case(MPLS_STACKED):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_MPLS)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_MPLS_STACKED_SHIFT;
                    break;
                case(IPV4_BROADCAST_1):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPv4)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_IPV4_1_BC_SHIFT;
                    break;
                case(IPV4_MULTICAST_1):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPv4)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_IPV4_1_MC_SHIFT;
                    break;
                case(IPV4_UNICAST_2):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPv4)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_IPV4_2_UC_SHIFT;
                    break;
                case(IPV4_MULTICAST_BROADCAST_2):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPv4)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_IPV4_2_MC_BC_SHIFT;
                    break;
                case(IPV6_MULTICAST_1):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPv6)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_IPV6_1_MC_SHIFT;
                    break;
                case(IPV6_UNICAST_2):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPv6)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_IPV6_2_UC_SHIFT;
                    break;
                case(IPV6_MULTICAST_2):
                    GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPv6)
                    tmpHxs[hdrNum] |= (i+1) << PRS_HDR_IPV6_2_MC_SHIFT;
                    break;
            }

        if (FmPcdNetEnvIsHdrExist(p_FmPort->h_FmPcd, p_FmPort->netEnvId, HEADER_TYPE_UDP_ENCAP_ESP))
        {
            p_PcdParams->p_PrsParams->additionalParams
                [p_PcdParams->p_PrsParams->numOfHdrsWithAdditionalParams].hdr = HEADER_TYPE_UDP;
            p_PcdParams->p_PrsParams->additionalParams
                [p_PcdParams->p_PrsParams->numOfHdrsWithAdditionalParams].swPrsEnable = TRUE;
            p_PcdParams->p_PrsParams->numOfHdrsWithAdditionalParams++;
        }

        /* set MPLS default next header - HW reset workaround  */
        GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_MPLS)
        tmpHxs[hdrNum] |= PRS_HDR_MPLS_LBL_INTER_EN;
        GET_PRS_HDR_NUM(L3HdrNum, HEADER_TYPE_USER_DEFINED_L3);
        tmpHxs[hdrNum] |= (uint32_t)L3HdrNum  << PRS_HDR_MPLS_NEXT_HDR_SHIFT;

        /* for GRE, disable errors */
        GET_PRS_HDR_NUM(greHdrNum, HEADER_TYPE_GRE);
        tmpHxs[greHdrNum] |= PRS_HDR_ERROR_DIS;

        /* config additional params for specific headers */
        for(i=0 ; i<p_PcdParams->p_PrsParams->numOfHdrsWithAdditionalParams ; i++)
        {
            GET_PRS_HDR_NUM(hdrNum, p_PcdParams->p_PrsParams->additionalParams[i].hdr);
            if(hdrNum== ILLEGAL_HDR_NUM)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, NO_MSG);
            if(hdrNum==NO_HDR_NUM)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Private headers may not use additional parameters"));

            err = AdditionalPrsParams(p_FmPort, &p_PcdParams->p_PrsParams->additionalParams[i], &tmpReg);
            if(err)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, NO_MSG);

            tmpHxs[hdrNum] |= tmpReg;
        }
#ifdef FM_PRS_L4_SHELL_ERRATA_FMANb
        {
            t_FmRevisionInfo revInfo;
            FM_GetRevision(p_FmPort->h_Fm, &revInfo);
            if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            {
               /* link to sw parser code for L4 shells - only if no other code is applied. */
                GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_SCTP)
                if(!(tmpHxs[hdrNum] & PRS_HDR_SW_PRS_EN))
                    tmpHxs[hdrNum] |= (PRS_HDR_SW_PRS_EN | SCTP_SW_PATCH_START);
                GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_DCCP)
                if(!(tmpHxs[hdrNum] & PRS_HDR_SW_PRS_EN))
                    tmpHxs[hdrNum] |= (PRS_HDR_SW_PRS_EN | DCCP_SW_PATCH_START);
                GET_PRS_HDR_NUM(hdrNum, HEADER_TYPE_IPSEC_AH)
                if(!(tmpHxs[hdrNum] & PRS_HDR_SW_PRS_EN))
                    tmpHxs[hdrNum] |= (PRS_HDR_SW_PRS_EN | IPSEC_SW_PATCH_START);
            }
        }
#endif /* FM_PRS_L4_SHELL_ERRATA_FMANb */

        for(i=0 ; i<FM_PCD_PRS_NUM_OF_HDRS ; i++)
        {
            /* For all header set LCV as taken from netEnv*/
            WRITE_UINT32(p_FmPort->p_FmPortPrsRegs->hdrs[i].lcv,  FmPcdGetLcv(p_FmPort->h_FmPcd, p_FmPort->netEnvId, (uint8_t)i));
            /* set HXS register according to default+Additional params+protocol options */
            WRITE_UINT32(p_FmPort->p_FmPortPrsRegs->hdrs[i].softSeqAttach,  tmpHxs[i]);
        }

        /* set tpid. */
        tmpReg = PRS_TPID_DFLT;
        if(p_PcdParams->p_PrsParams->setVlanTpid1)
        {
            tmpReg &= PRS_TPID2_MASK;
            tmpReg |= (uint32_t)p_PcdParams->p_PrsParams->vlanTpid1 << PRS_PCTPID_SHIFT;
        }
        if(p_PcdParams->p_PrsParams->setVlanTpid2)
        {
            tmpReg &= PRS_TPID1_MASK;
            tmpReg |= (uint32_t)p_PcdParams->p_PrsParams->vlanTpid2;
        }
        WRITE_UINT32(p_FmPort->p_FmPortPrsRegs->pctpid, tmpReg);

        /* enable parser */
        WRITE_UINT32(p_FmPort->p_FmPortPrsRegs->pcac, 0);

        if(p_PcdParams->p_PrsParams->prsResultPrivateInfo)
            p_FmPort->privateInfo = p_PcdParams->p_PrsParams->prsResultPrivateInfo;

    } /* end parser */
    else
        p_FmPort->privateInfo = 0;

    WRITE_UINT32(*p_BmiPrsStartOffset, GET_UINT32(*p_BmiPrsStartOffset) + p_FmPort->internalBufferOffset);

    /* set initial parser result - used for all engines */
    for (i=0;i<FM_PORT_PRS_RESULT_NUM_OF_WORDS;i++)
    {
        if (!i)
            WRITE_UINT32(*(p_BmiInitPrsResult), (uint32_t)(((uint32_t)p_FmPort->privateInfo << BMI_PR_PORTID_SHIFT)
                                                            | BMI_PRS_RESULT_HIGH));
        else
            if (i< FM_PORT_PRS_RESULT_NUM_OF_WORDS/2)
                WRITE_UINT32(*(p_BmiInitPrsResult+i), BMI_PRS_RESULT_HIGH);
            else
                WRITE_UINT32(*(p_BmiInitPrsResult+i), BMI_PRS_RESULT_LOW);
    }

    return E_OK;
}

static t_Error DeletePcd(t_Handle h_FmPort)
{
    t_FmPort                            *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error                             err = E_OK;
    volatile uint32_t                   *p_BmiNia=NULL;

    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for non-independant mode ports only"));

    if ((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_RX) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));

    if(!p_FmPort->pcdEngines)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("called for non PCD port"));

    /* get PCD registers pointers */
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofne;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    if((GET_UINT32(*p_BmiNia) & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)) != (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("port has to be detached previousely"));

    /* "cut" PCD out of the port's flow - go to BMI */
    /* WRITE_UINT32(*p_BmiNia, (p_FmPort->savedBmiNia & BMI_RFNE_FDCS_MASK) | (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)); */

    if(p_FmPort->pcdEngines | FM_PCD_PRS)
    {
        /* stop parser */
        WRITE_UINT32(p_FmPort->p_FmPortPrsRegs->pcac, PRS_CAC_STOP);
        /* wait for parser to be in idle state */
        while(GET_UINT32(p_FmPort->p_FmPortPrsRegs->pcac) & PRS_CAC_ACTIVE) ;
    }

    if(p_FmPort->pcdEngines & FM_PCD_KG)
    {
        err = FmPcdKgDeleteOrUnbindPortToClsPlanGrp(p_FmPort->h_FmPcd, p_FmPort->hardwarePortId, p_FmPort->clsPlanGrpId);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);
        p_FmPort->useClsPlan = FALSE;
    }

    if(p_FmPort->pcdEngines & FM_PCD_CC)
    {
        /* unbind - we need to get the treeId too */
        err = FmPcdCcUnbindTree(p_FmPort->h_FmPcd,  p_FmPort->ccTreeId);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);
    }

    p_FmPort->pcdEngines = 0;

    return E_OK;
}


/********************************************/
/*  Inter-module API                        */
/********************************************/
void FmPortSetMacsecLcv(t_Handle h_FmPort)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t           *p_BmiCfgReg = NULL;
    uint32_t                    macsecEn = BMI_PORT_CFG_EN_MACSEC;
    uint32_t                    lcv, walking1Mask = 0x80000000;
    uint8_t                     cnt = 0;

    SANITY_CHECK_RETURN(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
    {
        REPORT_ERROR(MAJOR, E_INVALID_OPERATION, ("The routine is relevant for Rx ports only"));
        return;
    }

    p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rcfg;
    /* get LCV for MACSEC */
    if ((p_FmPort->h_FmPcd) && ((lcv = FmPcdGetMacsecLcv(p_FmPort->h_FmPcd, p_FmPort->netEnvId))!= 0))
    {
        while(!(lcv & walking1Mask))
        {
            cnt++;
            walking1Mask >>= 1;
        }

        macsecEn |= (uint32_t)cnt << BMI_PORT_CFG_MS_SEL_SHIFT;
     }

     WRITE_UINT32(*p_BmiCfgReg, GET_UINT32(*p_BmiCfgReg) | macsecEn);
}

void FmPortSetMacsecCmd(t_Handle h_FmPort, uint8_t dfltSci)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t           *p_BmiCfgReg = NULL;
    uint32_t                    tmpReg;

    SANITY_CHECK_RETURN(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if ((p_FmPort->portType != e_FM_PORT_TYPE_TX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_TX))
    {
        REPORT_ERROR(MAJOR, E_INVALID_OPERATION, ("The routine is relevant for Tx ports only"));
        return;
    }

    p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_tfca;
    tmpReg = GET_UINT32(*p_BmiCfgReg) & ~BMI_CMD_ATTR_MACCMD_MASK;
    tmpReg |= BMI_CMD_ATTR_MACCMD_SECURED;
    tmpReg |= (((uint32_t)dfltSci << BMI_CMD_ATTR_MACCMD_SC_SHIFT) & BMI_CMD_ATTR_MACCMD_SC_MASK);

    WRITE_UINT32(*p_BmiCfgReg, tmpReg);
}

uint8_t FmPortGetNetEnvId(t_Handle h_FmPort)
{
    return ((t_FmPort*)h_FmPort)->netEnvId;
}

uint8_t FmPortGetHardwarePortId(t_Handle h_FmPort)
{
    return ((t_FmPort*)h_FmPort)->hardwarePortId;
}

uint32_t FmPortGetPcdEngines(t_Handle h_FmPort)
{
    return ((t_FmPort*)h_FmPort)->pcdEngines;
}

t_Error FmPortAttachPCD(t_Handle h_FmPort)
{
    t_FmPort                            *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t                   *p_BmiNia=NULL;

/*TODO - to take care about the chnges that were made in the port because of the previously assigned tree.
pndn, pnen ... maybe were changed because of the Tree requirement*/

    /* get PCD registers pointers */
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofne;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));
    }

    if(p_FmPort->requiredAction  & UPDATE_FMFP_PRC_WITH_ONE_RISC_ONLY)
        if(FmSetNumOfRiscsPerPort(p_FmPort->h_Fm, p_FmPort->hardwarePortId, 1)!= E_OK)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

    /* check that current NIA is BMI to BMI */
    if((GET_UINT32(*p_BmiNia) & ~BMI_RFNE_FDCS_MASK) != (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME))
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("may be called only for ports in BMI-to-BMI state."));

    WRITE_UINT32(*p_BmiNia, p_FmPort->savedBmiNia);

    if(p_FmPort->requiredAction  & UPDATE_NIA_PNEN)
        WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnen, p_FmPort->savedQmiPnen);

    if(p_FmPort->requiredAction  & UPDATE_NIA_PNDN)
        WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndn, p_FmPort->savedNonRxQmiRegsPndn);


    return E_OK;
}

t_Error FmPortGetSetCcParams(t_Handle h_FmPort, t_FmPortGetSetCcParams *p_CcParams)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    int                 tmpInt;
    volatile uint32_t   *p_BmiPrsStartOffset = NULL;

    /* this function called from Cc for pass and receive parameters port params between CC and PORT*/

    if((p_CcParams->getCcParams.type & OFFSET_OF_PR) && (p_FmPort->bufferOffsets.prsResultOffset != ILLEGAL_BASE))
    {
        p_CcParams->getCcParams.prOffset = (uint8_t)p_FmPort->bufferOffsets.prsResultOffset;
        p_CcParams->getCcParams.type &= ~OFFSET_OF_PR;
    }
    if(p_CcParams->getCcParams.type & HW_PORT_ID)
    {
        p_CcParams->getCcParams.hardwarePortId = (uint8_t)p_FmPort->hardwarePortId;
        p_CcParams->getCcParams.type &= ~HW_PORT_ID;
    }
    if((p_CcParams->getCcParams.type & OFFSET_OF_DATA) && (p_FmPort->bufferOffsets.dataOffset != ILLEGAL_BASE))
    {
        p_CcParams->getCcParams.dataOffset = (uint16_t)p_FmPort->bufferOffsets.dataOffset;
        p_CcParams->getCcParams.type &= ~OFFSET_OF_DATA;
    }
    if(p_CcParams->getCcParams.type & NUM_OF_TASKS)
    {
        p_CcParams->getCcParams.numOfTasks = p_FmPort->numOfTasks;
        p_CcParams->getCcParams.type &= ~NUM_OF_TASKS;
    }
    if(p_CcParams->getCcParams.type & BUFFER_POOL_ID_FOR_MANIP)
    {
        if(p_CcParams->getCcParams.poolIndex < p_FmPort->extBufPools.numOfPoolsUsed)
        {
            p_CcParams->getCcParams.poolIdForManip = p_FmPort->extBufPools.extBufPool[p_CcParams->getCcParams.poolIndex].id;
            p_CcParams->getCcParams.type &= ~BUFFER_POOL_ID_FOR_MANIP;
        }
    }

    if((p_CcParams->setCcParams.type & UPDATE_FMFP_PRC_WITH_ONE_RISC_ONLY) && !(p_FmPort->requiredAction & UPDATE_FMFP_PRC_WITH_ONE_RISC_ONLY))
    {
        p_FmPort->requiredAction |= UPDATE_FMFP_PRC_WITH_ONE_RISC_ONLY;
    }

    if((p_CcParams->setCcParams.type & UPDATE_NIA_PNEN) && !(p_FmPort->requiredAction & UPDATE_NIA_PNEN))
    {
        p_FmPort->savedQmiPnen = p_CcParams->setCcParams.nia;
        p_FmPort->requiredAction |= UPDATE_NIA_PNEN;
    }
    else if (p_CcParams->setCcParams.type & UPDATE_NIA_PNEN)
    {
       if(p_FmPort->savedQmiPnen != p_CcParams->setCcParams.nia)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("pnen was defined previously different"));
    }

    if((p_CcParams->setCcParams.type & UPDATE_NIA_PNDN) && !(p_FmPort->requiredAction & UPDATE_NIA_PNDN))
    {
        p_FmPort->savedNonRxQmiRegsPndn = p_CcParams->setCcParams.nia;
        p_FmPort->requiredAction |= UPDATE_NIA_PNDN;
    }
    else if(p_CcParams->setCcParams.type & UPDATE_NIA_PNDN)
    {
        if(p_FmPort->savedNonRxQmiRegsPndn != p_CcParams->setCcParams.nia)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("pndn was defined previously different"));
    }


    if((p_CcParams->setCcParams.type & UPDATE_PSO) && !(p_FmPort->requiredAction & UPDATE_PSO))
    {
        /* get PCD registers pointers */
         switch(p_FmPort->portType)
         {
             case(e_FM_PORT_TYPE_RX_10G):
             case(e_FM_PORT_TYPE_RX):
                 p_BmiPrsStartOffset = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rpso;
                 break;
             case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
                 p_BmiPrsStartOffset = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_opso;
                 break;
             default:
                 RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
         }
        /* set start parsing offset */
        tmpInt = (int)GET_UINT32(*p_BmiPrsStartOffset)+ p_CcParams->setCcParams.psoSize;
        if(tmpInt>0)
            WRITE_UINT32(*p_BmiPrsStartOffset, (uint32_t)tmpInt);

        p_FmPort->requiredAction |= UPDATE_PSO;
        p_FmPort->savedPrsStartOffset = p_CcParams->setCcParams.psoSize;

    }
    else if (p_CcParams->setCcParams.type & UPDATE_PSO)
    {
        if(p_FmPort->savedPrsStartOffset != p_CcParams->setCcParams.psoSize)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("parser start offset was defoned previousley different"));
    }
    return E_OK;
}
/**********************************         End of inter-module routines ********************************/

/****************************************/
/*       API Init unit functions        */
/****************************************/
t_Handle FM_PORT_Config(t_FmPortParams *p_FmPortParams)
{
    t_FmPort    *p_FmPort;
    uintptr_t   baseAddr = p_FmPortParams->baseAddr;

    /* Allocate FM structure */
    p_FmPort = (t_FmPort *) XX_Malloc(sizeof(t_FmPort));
    if (!p_FmPort)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM Port driver structure"));
        return NULL;
    }
    memset(p_FmPort, 0, sizeof(t_FmPort));

    /* Allocate the FM driver's parameters structure */
    p_FmPort->p_FmPortDriverParam = (t_FmPortDriverParam *)XX_Malloc(sizeof(t_FmPortDriverParam));
    if (!p_FmPort->p_FmPortDriverParam)
    {
        XX_Free(p_FmPort);
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM Port driver parameters"));
        return NULL;
    }
    memset(p_FmPort->p_FmPortDriverParam, 0, sizeof(t_FmPortDriverParam));

    /* Initialize FM port parameters which will be kept by the driver */
    p_FmPort->portType      = p_FmPortParams->portType;
    p_FmPort->portId        = p_FmPortParams->portId;
    p_FmPort->pcdEngines    = FM_PCD_NONE;
    p_FmPort->f_Exception   = p_FmPortParams->f_Exception;
    p_FmPort->h_App         = p_FmPortParams->h_App;
    p_FmPort->h_Fm          = p_FmPortParams->h_Fm;

    /* calculate global portId number */
    SW_PORT_ID_TO_HW_PORT_ID(p_FmPort->hardwarePortId, p_FmPort->portType, p_FmPortParams->portId);

    /* Initialize FM port parameters for initialization phase only */
    p_FmPort->p_FmPortDriverParam->baseAddr                         = baseAddr;
    p_FmPort->p_FmPortDriverParam->bufferPrefixContent.privDataSize = DEFAULT_PORT_bufferPrefixContent_privDataSize;
    p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passPrsResult= DEFAULT_PORT_bufferPrefixContent_passPrsResult;
    p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passTimeStamp= DEFAULT_PORT_bufferPrefixContent_passTimeStamp;
    p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passAllOtherPCDInfo
                                                                    = DEFAULT_PORT_bufferPrefixContent_passTimeStamp;
#ifdef DEBUG
    p_FmPort->p_FmPortDriverParam->bufferPrefixContent.passDebugInfo= DEFAULT_PORT_bufferPrefixContent_debugInfo;
#endif /* DEBUG */
    p_FmPort->p_FmPortDriverParam->bufferPrefixContent.dataAlign    = DEFAULT_PORT_bufferPrefixContent_dataAlign;
    p_FmPort->p_FmPortDriverParam->dmaSwapData                      = DEFAULT_PORT_dmaSwapData;
    p_FmPort->p_FmPortDriverParam->dmaIntContextCacheAttr           = DEFAULT_PORT_dmaIntContextCacheAttr;
    p_FmPort->p_FmPortDriverParam->dmaHeaderCacheAttr               = DEFAULT_PORT_dmaHeaderCacheAttr;
    p_FmPort->p_FmPortDriverParam->dmaScatterGatherCacheAttr        = DEFAULT_PORT_dmaScatterGatherCacheAttr;
    p_FmPort->p_FmPortDriverParam->dmaWriteOptimize                 = DEFAULT_PORT_dmaWriteOptimize;
    p_FmPort->p_FmPortDriverParam->liodnBase                        = p_FmPortParams->liodnBase;

    /* resource distribution. */
    p_FmPort->fifoBufs.num                     = DEFAULT_PORT_sizeOfFifo(p_FmPort->portType);
    p_FmPort->fifoBufs.extra                   = DEFAULT_PORT_extraSizeOfFifo(p_FmPort->portType);
    p_FmPort->openDmas.num                     = DEFAULT_PORT_numOfOpenDmas(p_FmPort->portType);
    p_FmPort->openDmas.extra                   = DEFAULT_PORT_extraNumOfOpenDmas(p_FmPort->portType);
    p_FmPort->tasks.num                        = DEFAULT_PORT_numOfTasks(p_FmPort->portType);
    p_FmPort->tasks.extra                      = DEFAULT_PORT_extraNumOfTasks(p_FmPort->portType);
    p_FmPort->numOfTasks = (uint8_t)p_FmPort->tasks.num;
#ifdef FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
        {
            p_FmPort->fifoBufs.extra           = 0;
            p_FmPort->openDmas.extra           = 0;
            p_FmPort->tasks.extra              = 0;
        }
    }
#endif /* FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16 */

    p_FmPort->p_FmPortDriverParam->color                            = DEFAULT_PORT_color;
#ifdef FM_OP_PORT_QMAN_REJECT_ERRATA_FMAN21
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0) &&
            (p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
                p_FmPort->p_FmPortDriverParam->color              = e_FM_PORT_COLOR_OVERRIDE;
    }
#endif /* FM_OP_PORT_QMAN_REJECT_ERRATA_FMAN21 */

    if (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND)
        p_FmPort->p_FmPortDriverParam->syncReq          = DEFAULT_PORT_syncReqForHc;
    else
        p_FmPort->p_FmPortDriverParam->syncReq          = DEFAULT_PORT_syncReq;

#ifdef FM_PORT_SYNC_ERRATA_FMAN6
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0) &&
            (p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
                p_FmPort->p_FmPortDriverParam->syncReq              = FALSE;
    }
#endif /* FM_PORT_SYNC_ERRATA_FMAN6 */

    /* Port type specific initialization: */
    if ((p_FmPort->portType != e_FM_PORT_TYPE_TX) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_TX_10G))
        p_FmPort->p_FmPortDriverParam->frmDiscardOverride           = DEFAULT_PORT_frmDiscardOverride;

    switch(p_FmPort->portType)
    {
    case(e_FM_PORT_TYPE_RX):
    case(e_FM_PORT_TYPE_RX_10G):
        /* Initialize FM port parameters for initialization phase only */
        p_FmPort->p_FmPortDriverParam->cutBytesFromEnd              = DEFAULT_PORT_cutBytesFromEnd;
        p_FmPort->p_FmPortDriverParam->enBufPoolDepletion           = FALSE;
        p_FmPort->p_FmPortDriverParam->frmDiscardOverride           = DEFAULT_PORT_frmDiscardOverride;
        p_FmPort->p_FmPortDriverParam->rxFifoPriElevationLevel      = DEFAULT_PORT_rxFifoPriElevationLevel;
        p_FmPort->p_FmPortDriverParam->rxFifoThreshold              = DEFAULT_PORT_rxFifoThreshold;
        p_FmPort->p_FmPortDriverParam->bufMargins.endMargins        = DEFAULT_PORT_BufMargins_endMargins;
        p_FmPort->p_FmPortDriverParam->errorsToDiscard              = DEFAULT_PORT_errorsToDiscard;
        p_FmPort->p_FmPortDriverParam->cheksumLastBytesIgnore       = DEFAULT_PORT_cheksumLastBytesIgnore;
        p_FmPort->p_FmPortDriverParam->forwardReuseIntContext       = DEFAULT_PORT_forwardIntContextReuse;
        break;

    case(e_FM_PORT_TYPE_TX):
        p_FmPort->txFifoDeqPipelineDepth                            = DEFAULT_PORT_txFifoDeqPipelineDepth_1G;
        p_FmPort->p_FmPortDriverParam->dontReleaseBuf               = FALSE;
    case(e_FM_PORT_TYPE_TX_10G):
        if(p_FmPort->portType == e_FM_PORT_TYPE_TX_10G)
            p_FmPort->txFifoDeqPipelineDepth                        = DEFAULT_PORT_txFifoDeqPipelineDepth_10G;
        p_FmPort->p_FmPortDriverParam->cheksumLastBytesIgnore       = DEFAULT_PORT_cheksumLastBytesIgnore;
        p_FmPort->p_FmPortDriverParam->txFifoMinFillLevel           = DEFAULT_PORT_txFifoMinFillLevel;
        p_FmPort->p_FmPortDriverParam->txFifoLowComfLevel           = DEFAULT_PORT_txFifoLowComfLevel;
    case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
    case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
        p_FmPort->p_FmPortDriverParam->deqHighPriority              = DEFAULT_PORT_deqHighPriority;
        p_FmPort->p_FmPortDriverParam->deqType                      = DEFAULT_PORT_deqType;
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
        p_FmPort->p_FmPortDriverParam->deqPrefetchOption            = DEFAULT_PORT_deqPrefetchOption;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */
        p_FmPort->p_FmPortDriverParam->deqByteCnt                   = DEFAULT_PORT_deqByteCnt;

        if (p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING)
            p_FmPort->p_FmPortDriverParam->errorsToDiscard          = DEFAULT_PORT_errorsToDiscard;
        break;

    default:
        XX_Free(p_FmPort->p_FmPortDriverParam);
        XX_Free(p_FmPort);
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
        return NULL;
    }
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    if (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND)
        p_FmPort->p_FmPortDriverParam->deqPrefetchOption            = DEFAULT_PORT_deqPrefetchOption_HC;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

    if ((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) ||
        (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND))
        p_FmPort->txFifoDeqPipelineDepth                            = OH_PIPELINE_DEPTH;

    p_FmPort->imEn = p_FmPortParams->independentModeEnable;

    if (p_FmPort->imEn)
    {
        if ((p_FmPort->portType == e_FM_PORT_TYPE_TX) ||
            (p_FmPort->portType == e_FM_PORT_TYPE_TX_10G))
            p_FmPort->txFifoDeqPipelineDepth = DEFAULT_PORT_txFifoDeqPipelineDepth_IM;
        FmPortConfigIM(p_FmPort, p_FmPortParams);
    }
    else
    {
        switch(p_FmPort->portType)
        {
        case(e_FM_PORT_TYPE_RX):
        case(e_FM_PORT_TYPE_RX_10G):
            /* Initialize FM port parameters for initialization phase only */
            memcpy(&p_FmPort->p_FmPortDriverParam->extBufPools,
                   &p_FmPortParams->specificParams.rxParams.extBufPools,
                   sizeof(t_FmPortExtPools));
            p_FmPort->p_FmPortDriverParam->errFqid                      = p_FmPortParams->specificParams.rxParams.errFqid;
            p_FmPort->p_FmPortDriverParam->dfltFqid                     = p_FmPortParams->specificParams.rxParams.dfltFqid;
            p_FmPort->p_FmPortDriverParam->liodnOffset                  = p_FmPortParams->specificParams.rxParams.liodnOffset;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
#ifdef FM_OP_PARTITION_ERRATA_FMANx8
        {
            t_FmRevisionInfo revInfo;
            FM_GetRevision(p_FmPort->h_Fm, &revInfo);
            if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                p_FmPort->p_FmPortDriverParam->liodnOffset              = p_FmPortParams->specificParams.nonRxParams.opLiodnOffset;
        }
#endif /* FM_OP_PARTITION_ERRATA_FMANx8 */
        case(e_FM_PORT_TYPE_TX):
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            p_FmPort->p_FmPortDriverParam->errFqid                      = p_FmPortParams->specificParams.nonRxParams.errFqid;
            p_FmPort->p_FmPortDriverParam->deqSubPortal                 =
                (uint8_t)(p_FmPortParams->specificParams.nonRxParams.qmChannel & QMI_DEQ_CFG_SUBPORTAL_MASK);
            p_FmPort->p_FmPortDriverParam->dfltFqid                     = p_FmPortParams->specificParams.nonRxParams.dfltFqid;
            break;
        default:
            XX_Free(p_FmPort->p_FmPortDriverParam);
            XX_Free(p_FmPort);
            REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
            return NULL;
        }
    }

    memset(p_FmPort->name, 0, (sizeof(char)) * MODULE_NAME_SIZE);
    if(Sprint (p_FmPort->name, "FM-%d-port-%s-%d",
               FmGetId(p_FmPort->h_Fm),
               ((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING ||
                 (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND)) ?
                "OH" : (p_FmPort->portType == e_FM_PORT_TYPE_RX ?
                        "1g-RX" : (p_FmPort->portType == e_FM_PORT_TYPE_TX ?
                                   "1g-TX" : (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G ?
                                              "10g-RX" : "10g-TX")))),
               p_FmPort->portId) == 0)
    {
        XX_Free(p_FmPort->p_FmPortDriverParam);
        XX_Free(p_FmPort);
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Sprint failed"));
        return NULL;
    }

    p_FmPort->h_Spinlock = XX_InitSpinlock();
    if (!p_FmPort->h_Spinlock)
    {
        XX_Free(p_FmPort->p_FmPortDriverParam);
        XX_Free(p_FmPort);
        REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Sprint failed"));
        return NULL;
    }

    return p_FmPort;
}

/**************************************************************************//**
 @Function      FM_PORT_Init

 @Description   Initializes the FM module

 @Param[in]     h_FmPort - FM module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PORT_Init(t_Handle h_FmPort)
{
    t_FmPort                        *p_FmPort = (t_FmPort*)h_FmPort;
    t_FmPortDriverParam             *p_Params;
    t_Error                         err = E_OK;
    t_FmInterModulePortInitParams   fmParams;
    uint32_t                        minFifoSizeRequired = 0;

    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    if ((err = BuildBufferStructure(p_FmPort)) != E_OK)
        RETURN_ERROR(MINOR, err, NO_MSG);

    CHECK_INIT_PARAMETERS(p_FmPort, CheckInitParameters);

    p_Params = p_FmPort->p_FmPortDriverParam;

    /* set memory map pointers */
    p_FmPort->p_FmPortQmiRegs     = (t_FmPortQmiRegs *)UINT_TO_PTR(p_Params->baseAddr + QMI_PORT_REGS_OFFSET);
    p_FmPort->p_FmPortBmiRegs     = (u_FmPortBmiRegs *)UINT_TO_PTR(p_Params->baseAddr + BMI_PORT_REGS_OFFSET);
    p_FmPort->p_FmPortPrsRegs     = (t_FmPortPrsRegs *)UINT_TO_PTR(p_Params->baseAddr + PRS_PORT_REGS_OFFSET);

    /* For O/H ports, check fifo size and update if necessary */
    if ((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) || (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND))
    {
        minFifoSizeRequired = (uint32_t)((p_FmPort->txFifoDeqPipelineDepth+4)*BMI_FIFO_UNITS);
        if (p_FmPort->fifoBufs.num < minFifoSizeRequired)
        {
            p_FmPort->fifoBufs.num = minFifoSizeRequired;
            DBG(WARNING, ("FIFO size enlarged to %d due to txFifoDeqPipelineDepth size", minFifoSizeRequired));
        }
    }

    /* For Rx Ports, call the external Buffer routine which also checks fifo
       size and updates it if necessary */
    if(((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
        && !p_FmPort->imEn)
    {
        /* define external buffer pools and pool depletion*/
        err = SetExtBufferPools(p_FmPort);
        if(err)
            RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    /************************************************************/
    /* Call FM module routine for communicating parameters      */
    /************************************************************/
    memset(&fmParams, 0, sizeof(fmParams));
    fmParams.hardwarePortId     = p_FmPort->hardwarePortId;
    fmParams.portType           = (e_FmPortType)p_FmPort->portType;
    fmParams.numOfTasks         = (uint8_t)p_FmPort->tasks.num;
    fmParams.numOfExtraTasks    = (uint8_t)p_FmPort->tasks.extra;
    fmParams.numOfOpenDmas      = (uint8_t)p_FmPort->openDmas.num;
    fmParams.numOfExtraOpenDmas = (uint8_t)p_FmPort->openDmas.extra;
    fmParams.sizeOfFifo         = p_FmPort->fifoBufs.num;
    fmParams.extraSizeOfFifo    = p_FmPort->fifoBufs.extra;
    fmParams.independentMode    = p_FmPort->imEn;
    fmParams.liodnOffset        = p_Params->liodnOffset;
    fmParams.liodnBase          = p_Params->liodnBase;

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            fmParams.deqPipelineDepth = p_FmPort->txFifoDeqPipelineDepth;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
    }

    err = FmGetSetPortParams(p_FmPort->h_Fm, &fmParams);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    p_FmPort->tasks.num        = fmParams.numOfTasks;
    p_FmPort->tasks.extra      = fmParams.numOfExtraTasks;
    p_FmPort->openDmas.num     = fmParams.numOfOpenDmas;
    p_FmPort->openDmas.extra   = fmParams.numOfExtraOpenDmas;
    p_FmPort->fifoBufs.num     = fmParams.sizeOfFifo;
    p_FmPort->fifoBufs.extra   = fmParams.extraSizeOfFifo;

    /* get params for use in init */
    p_Params->fmMuramPhysBaseAddr =
        (uint64_t)((uint64_t)(fmParams.fmMuramPhysBaseAddr.low) |
                   ((uint64_t)(fmParams.fmMuramPhysBaseAddr.high) << 32));

    /**********************/
    /* Init BMI Registers */
    /**********************/
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            err = BmiRxPortInit(p_FmPort);
            if(err)
                RETURN_ERROR(MAJOR, err, NO_MSG);
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
            err = BmiTxPortInit(p_FmPort);
            if(err)
                RETURN_ERROR(MAJOR, err, NO_MSG);
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            err = BmiOhPortInit(p_FmPort);
            if(err)
                RETURN_ERROR(MAJOR, err, NO_MSG);
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
    }

    /**********************/
    /* Init QMI Registers */
    /**********************/
    if (!p_FmPort->imEn && ((err = QmiInit(p_FmPort)) != E_OK))
        RETURN_ERROR(MAJOR, err, NO_MSG);

    if (p_FmPort->imEn && ((err = FmPortImInit(p_FmPort)) != E_OK))
        RETURN_ERROR(MAJOR, err, NO_MSG);

    FmPortDriverParamFree(p_FmPort);

    return E_OK;
}

/**************************************************************************//**
 @Function      FM_PORT_Free

 @Description   Frees all resources that were assigned to FM module.

                Calling this routine invalidates the descriptor.

 @Param[in]     h_FmPort - FM module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PORT_Free(t_Handle h_FmPort)
{
    t_FmPort                            *p_FmPort = (t_FmPort*)h_FmPort;
    t_FmInterModulePortFreeParams       fmParams;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);

    if(p_FmPort->pcdEngines)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Trying to free a port with PCD. FM_PORT_DeletePCD must be called first."));

    if (p_FmPort->enabled)
    {
        if (FM_PORT_Disable(p_FmPort) != E_OK)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("FM_PORT_Disable FAILED"));
    }

    FmPortDriverParamFree(p_FmPort);

    if (p_FmPort->imEn)
        FmPortImFree(p_FmPort);

    fmParams.hardwarePortId = p_FmPort->hardwarePortId;
    fmParams.portType = (e_FmPortType)p_FmPort->portType;
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    fmParams.deqPipelineDepth = p_FmPort->txFifoDeqPipelineDepth;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

    FmFreePortParams(p_FmPort->h_Fm, &fmParams);
    
    if (p_FmPort->h_Spinlock)
        XX_FreeSpinlock(p_FmPort->h_Spinlock);

    XX_Free(p_FmPort);

    return E_OK;
}


/*************************************************/
/*       API Advanced Init unit functions        */
/*************************************************/

t_Error FM_PORT_ConfigDeqHighPriority(t_Handle h_FmPort, bool highPri)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("not available for Rx ports"));

    p_FmPort->p_FmPortDriverParam->deqHighPriority = highPri;

    return E_OK;
}

t_Error FM_PORT_ConfigDeqType(t_Handle h_FmPort, e_FmPortDeqType deqType)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("not available for Rx ports"));

    p_FmPort->p_FmPortDriverParam->deqType = deqType;

    return E_OK;
}

#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
t_Error FM_PORT_ConfigDeqPrefetchOption(t_Handle h_FmPort, e_FmPortDeqPrefetchOption deqPrefetchOption)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("not available for Rx ports"));
    p_FmPort->p_FmPortDriverParam->deqPrefetchOption = deqPrefetchOption;
    return E_OK;
}
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

t_Error FM_PORT_ConfigBackupPools(t_Handle h_FmPort, t_FmPortBackupBmPools *p_BackupBmPools)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
#ifdef FM_NO_BACKUP_POOLS
    t_FmRevisionInfo    revInfo;
#endif /* FM_NO_BACKUP_POOLS */

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

#ifdef FM_NO_BACKUP_POOLS
    FM_GetRevision(p_FmPort->h_Fm, &revInfo);
    if (revInfo.majorRev != 4)
        RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("FM_PORT_ConfigBackupPools"));
#endif /* FM_NO_BACKUP_POOLS */

    p_FmPort->p_FmPortDriverParam->p_BackupBmPools = (t_FmPortBackupBmPools *)XX_Malloc(sizeof(t_FmPortBackupBmPools));
    if(!p_FmPort->p_FmPortDriverParam->p_BackupBmPools)
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("p_BackupBmPools allocation failed"));
    memcpy(p_FmPort->p_FmPortDriverParam->p_BackupBmPools, p_BackupBmPools, sizeof(t_FmPortBackupBmPools));

    return E_OK;
}

t_Error FM_PORT_ConfigDeqByteCnt(t_Handle h_FmPort, uint16_t deqByteCnt)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("not available for Rx ports"));

    p_FmPort->p_FmPortDriverParam->deqByteCnt = deqByteCnt;

    return E_OK;
}

t_Error FM_PORT_ConfigBufferPrefixContent(t_Handle h_FmPort, t_FmPortBufferPrefixContent *p_FmPortBufferPrefixContent)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    memcpy(&p_FmPort->p_FmPortDriverParam->bufferPrefixContent, p_FmPortBufferPrefixContent, sizeof(t_FmPortBufferPrefixContent));
    /* if dataAlign was not initialized by user, we return to driver's deafult */
    if (!p_FmPort->p_FmPortDriverParam->bufferPrefixContent.dataAlign)
        p_FmPort->p_FmPortDriverParam->bufferPrefixContent.dataAlign = DEFAULT_PORT_bufferPrefixContent_dataAlign;

    return E_OK;
}

t_Error FM_PORT_ConfigCheksumLastBytesIgnore(t_Handle h_FmPort, uint8_t cheksumLastBytesIgnore)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) || (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx & Tx ports only"));

    p_FmPort->p_FmPortDriverParam->cheksumLastBytesIgnore = cheksumLastBytesIgnore;

    return E_OK;
}

t_Error FM_PORT_ConfigCutBytesFromEnd(t_Handle h_FmPort, uint8_t cutBytesFromEnd)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

    p_FmPort->p_FmPortDriverParam->cutBytesFromEnd = cutBytesFromEnd;

    return E_OK;
}

t_Error FM_PORT_ConfigPoolDepletion(t_Handle h_FmPort, t_FmPortBufPoolDepletion *p_BufPoolDepletion)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

    p_FmPort->p_FmPortDriverParam->enBufPoolDepletion = TRUE;
    memcpy(&p_FmPort->p_FmPortDriverParam->bufPoolDepletion, p_BufPoolDepletion, sizeof(t_FmPortBufPoolDepletion));

    return E_OK;
}

t_Error FM_PORT_ConfigObservedPoolDepletion(t_Handle h_FmPort, t_FmPortObservedBufPoolDepletion *p_FmPortObservedBufPoolDepletion)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if(p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for OP ports only"));

    p_FmPort->p_FmPortDriverParam->enBufPoolDepletion = TRUE;
    memcpy(&p_FmPort->p_FmPortDriverParam->bufPoolDepletion, &p_FmPortObservedBufPoolDepletion->poolDepletionParams, sizeof(t_FmPortBufPoolDepletion));
    memcpy(&p_FmPort->p_FmPortDriverParam->extBufPools, &p_FmPortObservedBufPoolDepletion->poolsParams, sizeof(t_FmPortExtPools));

    return E_OK;
}

t_Error FM_PORT_ConfigExtBufPools(t_Handle h_FmPort, t_FmPortExtPools *p_FmPortExtPools)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if(p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for OP ports only"));

    memcpy(&p_FmPort->p_FmPortDriverParam->extBufPools, p_FmPortExtPools, sizeof(t_FmPortExtPools));

    return E_OK;
}

t_Error FM_PORT_ConfigRxFifoThreshold(t_Handle h_FmPort, uint32_t fifoThreshold)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

    p_FmPort->p_FmPortDriverParam->rxFifoThreshold = fifoThreshold;

    return E_OK;
}

t_Error FM_PORT_ConfigRxFifoPriElevationLevel(t_Handle h_FmPort, uint32_t priElevationLevel)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

    p_FmPort->p_FmPortDriverParam->rxFifoPriElevationLevel = priElevationLevel;

    return E_OK;
}

t_Error FM_PORT_ConfigTxFifoMinFillLevel(t_Handle h_FmPort, uint32_t minFillLevel)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_TX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_TX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Tx ports only"));

    p_FmPort->p_FmPortDriverParam->txFifoMinFillLevel = minFillLevel;

    return E_OK;
}

t_Error FM_PORT_ConfigTxFifoDeqPipelineDepth(t_Handle h_FmPort, uint8_t deqPipelineDepth)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if ((p_FmPort->portType != e_FM_PORT_TYPE_TX_10G) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_TX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Tx ports only"));
    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("Not available for IM ports!"));

    p_FmPort->txFifoDeqPipelineDepth = deqPipelineDepth;

    return E_OK;
}

t_Error FM_PORT_ConfigTxFifoLowComfLevel(t_Handle h_FmPort, uint32_t fifoLowComfLevel)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_TX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_TX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Tx ports only"));

    p_FmPort->p_FmPortDriverParam->txFifoLowComfLevel = fifoLowComfLevel;

    return E_OK;
}

t_Error FM_PORT_ConfigDontReleaseTxBufToBM(t_Handle h_FmPort)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_TX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_TX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Tx ports only"));

    p_FmPort->p_FmPortDriverParam->dontReleaseBuf = TRUE;

    return E_OK;
}

t_Error FM_PORT_ConfigDfltColor(t_Handle h_FmPort, e_FmPortColor color)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
#ifdef FM_OP_PORT_QMAN_REJECT_ERRATA_FMAN21
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("FM_PORT_ConfigDfltColor!"));
    }
#endif /* FM_OP_PORT_QMAN_REJECT_ERRATA_FMAN21 */
    p_FmPort->p_FmPortDriverParam->color = color;

    return E_OK;
}

t_Error FM_PORT_ConfigSyncReq(t_Handle h_FmPort, bool syncReq)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
#ifdef FM_PORT_SYNC_ERRATA_FMAN6
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("port-sync!"));
    }
#endif /* FM_PORT_SYNC_ERRATA_FMAN6 */

    p_FmPort->p_FmPortDriverParam->syncReq = syncReq;

    return E_OK;
}


t_Error FM_PORT_ConfigFrmDiscardOverride(t_Handle h_FmPort, bool override)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType == e_FM_PORT_TYPE_TX_10G) && (p_FmPort->portType == e_FM_PORT_TYPE_TX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("not available for Tx ports"));

    p_FmPort->p_FmPortDriverParam->frmDiscardOverride = override;

    return E_OK;
}

t_Error FM_PORT_ConfigErrorsToDiscard(t_Handle h_FmPort, fmPortFrameErrSelect_t errs)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);
    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX) &&
                                                            (p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));

    p_FmPort->p_FmPortDriverParam->errorsToDiscard = errs;

    return E_OK;
}

t_Error FM_PORT_ConfigDmaSwapData(t_Handle h_FmPort, e_FmPortDmaSwap swapData)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    p_FmPort->p_FmPortDriverParam->dmaSwapData = swapData;

    return E_OK;
}

t_Error FM_PORT_ConfigDmaIcCacheAttr(t_Handle h_FmPort, e_FmPortDmaCache intContextCacheAttr)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    p_FmPort->p_FmPortDriverParam->dmaIntContextCacheAttr = intContextCacheAttr;

    return E_OK;
}

t_Error FM_PORT_ConfigDmaHdrAttr(t_Handle h_FmPort, e_FmPortDmaCache headerCacheAttr)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    p_FmPort->p_FmPortDriverParam->dmaHeaderCacheAttr = headerCacheAttr;

    return E_OK;
}

t_Error FM_PORT_ConfigDmaScatterGatherAttr(t_Handle h_FmPort, e_FmPortDmaCache scatterGatherCacheAttr)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    p_FmPort->p_FmPortDriverParam->dmaScatterGatherCacheAttr = scatterGatherCacheAttr;

    return E_OK;
}

t_Error FM_PORT_ConfigDmaWriteOptimize(t_Handle h_FmPort, bool optimize)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    if((p_FmPort->portType == e_FM_PORT_TYPE_TX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_TX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("Not available for Tx ports"));

    p_FmPort->p_FmPortDriverParam->dmaWriteOptimize = optimize;

    return E_OK;
}

t_Error FM_PORT_ConfigForwardReuseIntContext(t_Handle h_FmPort, bool forwardReuse)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

    p_FmPort->p_FmPortDriverParam->forwardReuseIntContext = forwardReuse;

    return E_OK;
}


/****************************************************/
/*       PCD Advaced config API                     */
/****************************************************/

/****************************************************/
/*       API Run-time Control unit functions        */
/****************************************************/

t_Error FM_PORT_SetNumOfOpenDmas(t_Handle h_FmPort, t_FmPortRsrc *p_NumOfOpenDmas)
{
    t_FmPort    *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error     err;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

#ifdef FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0) &&
            (p_NumOfOpenDmas->extra))
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("excessive resources"));
    }
#endif /* FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16 */

    if((!p_NumOfOpenDmas->num) || (p_NumOfOpenDmas->num > MAX_NUM_OF_DMAS))
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("openDmas-num can't be larger than %d", MAX_NUM_OF_DMAS));
    if(p_NumOfOpenDmas->extra > MAX_NUM_OF_EXTRA_DMAS)
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("openDmas-extra can't be larger than %d", MAX_NUM_OF_EXTRA_DMAS));
    err = FmSetNumOfOpenDmas(p_FmPort->h_Fm, p_FmPort->hardwarePortId, (uint8_t)p_NumOfOpenDmas->num, (uint8_t)p_NumOfOpenDmas->extra, FALSE);
    if(err)
        RETURN_ERROR(MINOR, err, NO_MSG);

    memcpy(&p_FmPort->openDmas, p_NumOfOpenDmas, sizeof(t_FmPortRsrc));

    return E_OK;
}

t_Error FM_PORT_SetNumOfTasks(t_Handle h_FmPort, t_FmPortRsrc *p_NumOfTasks)
{
    t_FmPort    *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error     err;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    if (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND)
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("not available for host command port where number is always 1"));

#ifdef FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0) &&
            (p_NumOfTasks->extra))
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("excessive resources"));
    }
#endif /* FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16 */

    if((!p_NumOfTasks->num) || (p_NumOfTasks->num > MAX_NUM_OF_TASKS))
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("NumOfTasks-num can't be larger than %d", MAX_NUM_OF_TASKS));
    if(p_NumOfTasks->extra > MAX_NUM_OF_EXTRA_TASKS)
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("NumOfTasks-extra can't be larger than %d", MAX_NUM_OF_EXTRA_TASKS));

    err = FmSetNumOfTasks(p_FmPort->h_Fm, p_FmPort->hardwarePortId, (uint8_t)p_NumOfTasks->num, (uint8_t)p_NumOfTasks->extra, FALSE);
    if(err)
        RETURN_ERROR(MINOR, err, NO_MSG);

    /* update driver's struct */
    memcpy(&p_FmPort->tasks, p_NumOfTasks, sizeof(t_FmPortRsrc));
    return E_OK;
}

t_Error FM_PORT_SetSizeOfFifo(t_Handle h_FmPort, t_FmPortRsrc *p_SizeOfFifo)
{
    t_FmPort                            *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error                             err;
    t_FmInterModulePortRxPoolsParams    rxPoolsParams;
    uint32_t                            minFifoSizeRequired;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

#ifdef FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0) &&
            (p_SizeOfFifo->extra))
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("excessive resources"));
    }
#endif /* FM_PORT_EXCESSIVE_BUDGET_ERRATA_FMANx16 */
    if(!p_SizeOfFifo->num || (p_SizeOfFifo->num > BMI_MAX_FIFO_SIZE))
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("SizeOfFifo-num has to be in the range of 256 - %d", BMI_MAX_FIFO_SIZE));
    if(p_SizeOfFifo->num % BMI_FIFO_UNITS)
         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("SizeOfFifo-num has to be divisible by %d", BMI_FIFO_UNITS));
    if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
    {
        /* extra FIFO size (allowed only to Rx ports) */
         if(p_SizeOfFifo->extra % BMI_FIFO_UNITS)
              RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("SizeOfFifo-extra has to be divisible by %d", BMI_FIFO_UNITS));
    }
    else
        if(p_SizeOfFifo->extra)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, (" No SizeOfFifo-extra for non Rx ports"));

    /* For O/H ports, check fifo size and update if necessary */
    if((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) || (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND))
    {
        minFifoSizeRequired = (uint32_t)((p_FmPort->txFifoDeqPipelineDepth+4)*BMI_FIFO_UNITS);
        if (p_FmPort->fifoBufs.num < minFifoSizeRequired)
        {
            p_FmPort->fifoBufs.num = minFifoSizeRequired;
            DBG(INFO, ("FIFO size enlarged to %d", minFifoSizeRequired));
        }
    }
    memcpy(&rxPoolsParams, &p_FmPort->rxPoolsParams, sizeof(rxPoolsParams));
    err = FmSetSizeOfFifo(p_FmPort->h_Fm,
                            p_FmPort->hardwarePortId,
                            p_FmPort->portType,
                            p_FmPort->imEn,
                            &p_SizeOfFifo->num,
                            p_SizeOfFifo->extra,
                            p_FmPort->txFifoDeqPipelineDepth,
                            &rxPoolsParams,
                            FALSE);
    if(err)
        RETURN_ERROR(MINOR, err, NO_MSG);

    /* update driver's structure AFTER the FM routine, as it may change by the FM. */
    memcpy(&p_FmPort->fifoBufs, p_SizeOfFifo, sizeof(t_FmPortRsrc));

    return E_OK;
}

uint32_t FM_PORT_GetBufferDataOffset(t_Handle h_FmPort)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE, 0);

    return p_FmPort->bufferOffsets.dataOffset;
}

uint8_t * FM_PORT_GetBufferICInfo(t_Handle h_FmPort, char *p_Data)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE, 0);

    if(p_FmPort->bufferOffsets.pcdInfoOffset == ILLEGAL_BASE)
        return NULL;

    return (uint8_t *)PTR_MOVE(p_Data, p_FmPort->bufferOffsets.pcdInfoOffset);
}

#ifdef DEBUG
uint8_t * FM_PORT_GetBufferDebugInfo(t_Handle h_FmPort, char *p_Data)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE, 0);

    if(p_FmPort->bufferOffsets.debugOffset == ILLEGAL_BASE)
        return NULL;

    return (uint8_t *)PTR_MOVE(p_Data, p_FmPort->bufferOffsets.debugOffset);
}
#endif /* DEBUG */

t_FmPrsResult * FM_PORT_GetBufferPrsResult(t_Handle h_FmPort, char *p_Data)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, NULL);
    SANITY_CHECK_RETURN_VALUE(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE, NULL);

    if(p_FmPort->bufferOffsets.prsResultOffset == ILLEGAL_BASE)
        return NULL;

    return (t_FmPrsResult *)PTR_MOVE(p_Data, p_FmPort->bufferOffsets.prsResultOffset);
}

uint64_t * FM_PORT_GetBufferTimeStamp(t_Handle h_FmPort, char *p_Data)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, NULL);
    SANITY_CHECK_RETURN_VALUE(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE, NULL);

    if(p_FmPort->bufferOffsets.timeStampOffset == ILLEGAL_BASE)
        return NULL;

    return (uint64_t *)PTR_MOVE(p_Data, p_FmPort->bufferOffsets.timeStampOffset);
}

uint8_t * FM_PORT_GetBufferHashResult(t_Handle h_FmPort, char *p_Data)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE, 0);

    if(p_FmPort->bufferOffsets.hashResultOffset == ILLEGAL_BASE)
        return NULL;

    return (uint8_t *)PTR_MOVE(p_Data, p_FmPort->bufferOffsets.hashResultOffset);
}

t_Error FM_PORT_Disable(t_Handle h_FmPort)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t           *p_BmiCfgReg = NULL;
    volatile uint32_t           *p_BmiStatusReg = NULL;
    bool                        rxPort = FALSE;
    int                         tries;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rcfg;
            p_BmiStatusReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rst;
            rxPort = TRUE;
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
             p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_tcfg;
             p_BmiStatusReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_tst;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ocfg;
            p_BmiStatusReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ost;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }
    /* check if port is already disabled */
    if(!(GET_UINT32(*p_BmiCfgReg) & BMI_PORT_CFG_EN))
    {
        if (!rxPort && !p_FmPort->imEn)
        {
            if(!(GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc)& QMI_PORT_CFG_EN))
                /* port is disabled */
                return E_OK;
            else
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("Inconsistency: Port's QMI is enabled but BMI disabled"));
        }
        /* port is disabled */
        return E_OK;
    }

    /* Disable QMI */
    if (!rxPort && !p_FmPort->imEn)
    {
        WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc,
                     GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc) & ~QMI_PORT_CFG_EN);
        /* wait for QMI to finish Handling dequeue tnums */
        tries=1000;
        while ((GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pns) & QMI_PORT_STATUS_DEQ_FD_BSY) &&
                --tries)
            XX_UDelay(1);
        if (!tries)
            RETURN_ERROR(MINOR, E_BUSY, ("%s: can't disable!", p_FmPort->name));
    }

    /* Disable BMI */
    WRITE_UINT32(*p_BmiCfgReg, GET_UINT32(*p_BmiCfgReg) & ~BMI_PORT_CFG_EN);

    if (p_FmPort->imEn)
        FmPortImDisable(p_FmPort);

    tries=5000;
    while ((GET_UINT32(*p_BmiStatusReg) & BMI_PORT_STATUS_BSY) &&
            --tries)
        XX_UDelay(1);

    if (!tries)
        RETURN_ERROR(MINOR, E_BUSY, ("%s: can't disable!", p_FmPort->name));

    p_FmPort->enabled = 0;

    return E_OK;
}

t_Error FM_PORT_Enable(t_Handle h_FmPort)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t           *p_BmiCfgReg = NULL;
    bool                        rxPort = FALSE;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rcfg;
            rxPort = TRUE;
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
             p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_tcfg;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            p_BmiCfgReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ocfg;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    /* check if port is already enabled */
    if(GET_UINT32(*p_BmiCfgReg) & BMI_PORT_CFG_EN)
    {
        if (!rxPort && !p_FmPort->imEn)
        {
            if(GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc)& QMI_PORT_CFG_EN)
                /* port is enabled */
                return E_OK;
            else
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("Inconsistency: Port's BMI is enabled but QMI disabled"));
        }
        /* port is enabled */
        return E_OK;
    }

    if (p_FmPort->imEn)
        FmPortImEnable(p_FmPort);

    /* Enable QMI */
    if (!rxPort && !p_FmPort->imEn)
        WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc,
                     GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc) | QMI_PORT_CFG_EN);

    /* Enable BMI */
    WRITE_UINT32(*p_BmiCfgReg, GET_UINT32(*p_BmiCfgReg) | BMI_PORT_CFG_EN);

    p_FmPort->enabled = 1;

    return E_OK;
}

t_Error FM_PORT_SetRateLimit(t_Handle h_FmPort, t_FmPortRateLimit *p_RateLimit)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t            tmpRateLimit, tmpRateLimitScale;
    volatile uint32_t   *p_RateLimitReg, *p_RateLimitScaleReg;
    uint8_t             factor, countUnitBit;
    uint16_t            baseGran;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    if((p_FmPort->portType == e_FM_PORT_TYPE_RX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_RX) ||
                                                (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Tx and Offline parsing ports only"));

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
            p_RateLimitReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_trlmt;
            p_RateLimitScaleReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_trlmts;
            baseGran = 16000;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_RateLimitReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_orlmt;
            p_RateLimitScaleReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_orlmts;
            baseGran = 10000;
           break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    countUnitBit = (uint8_t)FmGetTimeStampScale(p_FmPort->h_Fm);  /* TimeStamp per nano seconds units */
    /* normally, we use 1 usec as the reference count */
    factor = 1;
    /* if ratelimit is too small for a 1usec factor, multiply the factor */
    while (p_RateLimit->rateLimit < baseGran/factor)
    {
        if (countUnitBit==31)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Rate limit is too small"));

        countUnitBit++;
        factor <<= 1;
    }
    /* if ratelimit is too large for a 1usec factor, it is also larger than max rate*/
    if (p_RateLimit->rateLimit > ((uint32_t)baseGran * (1<<10) * (uint32_t)factor))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Rate limit is too large"));

    tmpRateLimit = (uint32_t)(p_RateLimit->rateLimit*factor/baseGran - 1);

    if(!p_RateLimit->maxBurstSize || (p_RateLimit->maxBurstSize > MAX_BURST_SIZE))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("maxBurstSize must be between 1K and %dk", MAX_BURST_SIZE));

    tmpRateLimitScale = ((31 - (uint32_t)countUnitBit) << BMI_COUNT_RATE_UNIT_SHIFT) | BMI_RATE_LIMIT_EN;

    if(p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING)
        tmpRateLimit |= (uint32_t)(p_RateLimit->maxBurstSize - 1) << BMI_MAX_BURST_SHIFT;
    else
    {
#ifndef FM_NO_ADVANCED_RATE_LIMITER
        t_FmRevisionInfo    revInfo;

        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if (revInfo.majorRev == 4)
        {
            switch(p_RateLimit->rateLimitDivider)
            {
                case(e_FM_PORT_DUAL_RATE_LIMITER_NONE):
                    break;
                case(e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_2):
                    tmpRateLimitScale |= BMI_RATE_LIMIT_SCALE_BY_2;
                    break;
                case(e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_4):
                    tmpRateLimitScale |= BMI_RATE_LIMIT_SCALE_BY_4;
                    break;
                case(e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_8):
                    tmpRateLimitScale |= BMI_RATE_LIMIT_SCALE_BY_8;
                    break;
                default:
                    break;
            }
            tmpRateLimit |= BMI_RATE_LIMIT_BURST_SIZE_GRAN;
        }
        else
#endif /* ! FM_NO_ADVANCED_RATE_LIMITER */
        {
            if(p_RateLimit->rateLimitDivider != e_FM_PORT_DUAL_RATE_LIMITER_NONE)
                    RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("FM_PORT_ConfigDualRateLimitScaleDown"));

            if(p_RateLimit->maxBurstSize % 1000)
            {
                p_RateLimit->maxBurstSize = (uint16_t)((p_RateLimit->maxBurstSize/1000)+1);
                DBG(WARNING, ("rateLimit.maxBurstSize rounded up to %d", (p_RateLimit->maxBurstSize/1000+1)*1000));
            }
            else
                p_RateLimit->maxBurstSize = (uint16_t)(p_RateLimit->maxBurstSize/1000);
        }
        tmpRateLimit |= (uint32_t)(p_RateLimit->maxBurstSize - 1) << BMI_MAX_BURST_SHIFT;

    }
    WRITE_UINT32(*p_RateLimitScaleReg, tmpRateLimitScale);
    WRITE_UINT32(*p_RateLimitReg, tmpRateLimit);

    return E_OK;
}

t_Error FM_PORT_DeleteRateLimit(t_Handle h_FmPort)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t   *p_RateLimitReg, *p_RateLimitScaleReg;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_HANDLE);

    if((p_FmPort->portType == e_FM_PORT_TYPE_RX_10G) || (p_FmPort->portType == e_FM_PORT_TYPE_RX) ||
                                                (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Tx and Offline parsing ports only"));

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
            p_RateLimitReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_trlmt;
            p_RateLimitScaleReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_trlmts;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_RateLimitReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_orlmt;
            p_RateLimitScaleReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_orlmts;
           break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    WRITE_UINT32(*p_RateLimitScaleReg, 0);
    WRITE_UINT32(*p_RateLimitReg, 0);

    return E_OK;
}


t_Error FM_PORT_SetFrameQueueCounters(t_Handle h_FmPort, bool enable)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t                tmpReg;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    tmpReg = GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc);
    if(enable)
        tmpReg |= QMI_PORT_CFG_EN_COUNTERS ;
    else
        tmpReg &= ~QMI_PORT_CFG_EN_COUNTERS;

    WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc, tmpReg);

    return E_OK;
}

t_Error FM_PORT_SetPerformanceCounters(t_Handle h_FmPort, bool enable)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t       *p_BmiPcReg = NULL;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiPcReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rpc;
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
            p_BmiPcReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_tpc;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            p_BmiPcReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_opc;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    if(enable)
        WRITE_UINT32(*p_BmiPcReg, BMI_COUNTERS_EN);
    else
        WRITE_UINT32(*p_BmiPcReg, 0);

    return E_OK;
}

t_Error FM_PORT_SetPerformanceCountersParams(t_Handle h_FmPort, t_FmPortPerformanceCnt *p_FmPortPerformanceCnt)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t                tmpReg;
    volatile uint32_t       *p_BmiPcpReg = NULL;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiPcpReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rpcp;
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
            p_BmiPcpReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_tpcp;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            p_BmiPcpReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_opcp;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    /* check parameters */
    if (!p_FmPortPerformanceCnt->taskCompVal ||
        (p_FmPortPerformanceCnt->taskCompVal > p_FmPort->tasks.num))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                     ("performanceCnt.taskCompVal has to be in the range of 1 - %d (current value)!",
                      p_FmPort->tasks.num));
    if (!p_FmPortPerformanceCnt->dmaCompVal ||
        (p_FmPortPerformanceCnt->dmaCompVal > p_FmPort->openDmas.num))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                     ("performanceCnt.dmaCompVal has to be in the range of 1 - %d (current value)!",
                      p_FmPort->openDmas.num));
    if (!p_FmPortPerformanceCnt->fifoCompVal ||
        (p_FmPortPerformanceCnt->fifoCompVal > p_FmPort->fifoBufs.num))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                     ("performanceCnt.fifoCompVal has to be in the range of 256 - %d (current value)!",
                      p_FmPort->fifoBufs.num));
    if (p_FmPortPerformanceCnt->fifoCompVal % BMI_FIFO_UNITS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                     ("performanceCnt.fifoCompVal has to be divisible by %d",
                      BMI_FIFO_UNITS));
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            if (!p_FmPortPerformanceCnt->queueCompVal ||
                (p_FmPortPerformanceCnt->queueCompVal > MAX_PERFORMANCE_RX_QUEUE_COMP))
                RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                             ("performanceCnt.queueCompVal for Rx has to be in the range of 1 - %d",
                              MAX_PERFORMANCE_RX_QUEUE_COMP));
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
            if (!p_FmPortPerformanceCnt->queueCompVal ||
                (p_FmPortPerformanceCnt->queueCompVal > MAX_PERFORMANCE_TX_QUEUE_COMP))
                RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                             ("performanceCnt.queueCompVal for Tx has to be in the range of 1 - %d",
                              MAX_PERFORMANCE_TX_QUEUE_COMP));
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            if (p_FmPortPerformanceCnt->queueCompVal)
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("performanceCnt.queueCompVal is not relevant for H/O ports."));
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    tmpReg = 0;
    tmpReg |= ((uint32_t)(p_FmPortPerformanceCnt->queueCompVal - 1) << BMI_PERFORMANCE_PORT_COMP_SHIFT);
    tmpReg |= ((uint32_t)(p_FmPortPerformanceCnt->dmaCompVal- 1) << BMI_PERFORMANCE_DMA_COMP_SHIFT);
    tmpReg |= ((uint32_t)(p_FmPortPerformanceCnt->fifoCompVal/BMI_FIFO_UNITS - 1) << BMI_PERFORMANCE_FIFO_COMP_SHIFT);
    if ((p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING) && (p_FmPort->portType != e_FM_PORT_TYPE_OH_HOST_COMMAND))
        tmpReg |= ((uint32_t)(p_FmPortPerformanceCnt->taskCompVal - 1) << BMI_PERFORMANCE_TASK_COMP_SHIFT);

    WRITE_UINT32(*p_BmiPcpReg, tmpReg);

    return E_OK;
}

t_Error FM_PORT_AnalyzePerformanceParams(t_Handle h_FmPort)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    t_FmPortPerformanceCnt  currParams, savedParams;
    t_Error                 err;
    bool                    underTest, failed = FALSE;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);

    XX_Print("Analyzing Performance parameters for port (type %d, id%d)\n",
             p_FmPort->portType, p_FmPort->portId);

    currParams.taskCompVal    = (uint8_t)p_FmPort->tasks.num;
    if ((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) ||
        (p_FmPort->portType == e_FM_PORT_TYPE_OH_HOST_COMMAND))
        currParams.queueCompVal   = 0;
    else
        currParams.queueCompVal   = 1;
    currParams.dmaCompVal     =(uint8_t) p_FmPort->openDmas.num;
    currParams.fifoCompVal    = p_FmPort->fifoBufs.num;

    FM_PORT_SetPerformanceCounters(p_FmPort, FALSE);
    ClearPerfCnts(p_FmPort);
    if ((err = FM_PORT_SetPerformanceCountersParams(p_FmPort, &currParams)) != E_OK)
        RETURN_ERROR(MAJOR, err, NO_MSG);
    FM_PORT_SetPerformanceCounters(p_FmPort, TRUE);
    XX_UDelay(1000000);
    FM_PORT_SetPerformanceCounters(p_FmPort, FALSE);
    if (FM_PORT_GetCounter(p_FmPort, e_FM_PORT_COUNTERS_TASK_UTIL))
    {
        XX_Print ("Max num of defined port tasks (%d) utilized - Please enlarge\n",p_FmPort->tasks.num);
        failed = TRUE;
    }
    if (FM_PORT_GetCounter(p_FmPort, e_FM_PORT_COUNTERS_DMA_UTIL))
    {
        XX_Print ("Max num of defined port openDmas (%d) utilized - Please enlarge\n",p_FmPort->openDmas.num);
        failed = TRUE;
    }
    if (FM_PORT_GetCounter(p_FmPort, e_FM_PORT_COUNTERS_FIFO_UTIL))
    {
        XX_Print ("Max size of defined port fifo (%d) utilized - Please enlarge\n",p_FmPort->fifoBufs.num*BMI_FIFO_UNITS);
        failed = TRUE;
    }
    if (failed)
        RETURN_ERROR(MINOR, E_INVALID_STATE, NO_MSG);

    memset(&savedParams, 0, sizeof(savedParams));
    while (TRUE)
    {
        underTest = FALSE;
        if ((currParams.taskCompVal != 1) && !savedParams.taskCompVal)
        {
            currParams.taskCompVal--;
            underTest = TRUE;
        }
        if ((currParams.dmaCompVal != 1) && !savedParams.dmaCompVal)
        {
            currParams.dmaCompVal--;
            underTest = TRUE;
        }
        if ((currParams.fifoCompVal != BMI_FIFO_UNITS) && !savedParams.fifoCompVal)
        {
            currParams.fifoCompVal -= BMI_FIFO_UNITS;
            underTest = TRUE;
        }
        if (!underTest)
            break;

        ClearPerfCnts(p_FmPort);
        if ((err = FM_PORT_SetPerformanceCountersParams(p_FmPort, &currParams)) != E_OK)
            RETURN_ERROR(MAJOR, err, NO_MSG);
        FM_PORT_SetPerformanceCounters(p_FmPort, TRUE);
        XX_UDelay(1000000);
        FM_PORT_SetPerformanceCounters(p_FmPort, FALSE);

        if (!savedParams.taskCompVal && FM_PORT_GetCounter(p_FmPort, e_FM_PORT_COUNTERS_TASK_UTIL))
            savedParams.taskCompVal = (uint8_t)(currParams.taskCompVal+2);
        if (!savedParams.dmaCompVal && FM_PORT_GetCounter(p_FmPort, e_FM_PORT_COUNTERS_DMA_UTIL))
            savedParams.dmaCompVal = (uint8_t)(currParams.dmaCompVal+2);
        if (!savedParams.fifoCompVal && FM_PORT_GetCounter(p_FmPort, e_FM_PORT_COUNTERS_FIFO_UTIL))
            savedParams.fifoCompVal = currParams.fifoCompVal+2;
    }

    XX_Print("best vals: tasks %d, dmas %d, fifos %d\n",
             savedParams.taskCompVal, savedParams.dmaCompVal, savedParams.fifoCompVal);
    return E_OK;
}

t_Error FM_PORT_SetStatisticsCounters(t_Handle h_FmPort, bool enable)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t                tmpReg;
    volatile uint32_t       *p_BmiStcReg = NULL;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiStcReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rstc;
            break;
        case(e_FM_PORT_TYPE_TX_10G):
        case(e_FM_PORT_TYPE_TX):
            p_BmiStcReg = &p_FmPort->p_FmPortBmiRegs->txPortBmiRegs.fmbm_tstc;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
        case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            p_BmiStcReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ostc;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
    }

    tmpReg = GET_UINT32(*p_BmiStcReg);

    if(enable)
        tmpReg |= BMI_COUNTERS_EN;
    else
        tmpReg &= ~BMI_COUNTERS_EN;

    WRITE_UINT32(*p_BmiStcReg, tmpReg);

    return E_OK;
}

t_Error FM_PORT_SetErrorsRoute(t_Handle h_FmPort,  fmPortFrameErrSelect_t errs)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t       *p_ErrQReg, *p_ErrDiscard;

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_ErrQReg = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfsem;
            p_ErrDiscard = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfsdm;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_ErrQReg = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofsem;
            p_ErrDiscard = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofsdm;
            break;
        default:
           RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));
    }

    if (GET_UINT32(*p_ErrDiscard) & errs)
        RETURN_ERROR(MAJOR, E_INVALID_SELECTION, ("Selectd Errors that were configured to cause frame discard."));

    WRITE_UINT32(*p_ErrQReg, errs);

    return E_OK;
}

t_Error FM_PORT_SetAllocBufCounter(t_Handle h_FmPort, uint8_t poolId, bool enable)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t                tmpReg;
    int                     i;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(poolId<BM_MAX_NUM_OF_POOLS, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) && (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

    for(i=0 ; i< FM_PORT_MAX_NUM_OF_EXT_POOLS ; i++)
    {
        tmpReg = GET_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_ebmpi[i]);
        if ((uint8_t)((tmpReg & BMI_EXT_BUF_POOL_ID_MASK) >> BMI_EXT_BUF_POOL_ID_SHIFT) == poolId)
        {
            if(enable)
                tmpReg |= BMI_EXT_BUF_POOL_EN_COUNTER;
            else
                tmpReg &= ~BMI_EXT_BUF_POOL_EN_COUNTER;
            WRITE_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_ebmpi[i], tmpReg);
            break;
        }
    }
    if (i == FM_PORT_MAX_NUM_OF_EXT_POOLS)
        RETURN_ERROR(MINOR, E_INVALID_VALUE,("poolId %d is not included in this ports pools", poolId));

    return E_OK;
}

uint32_t FM_PORT_GetCounter(t_Handle h_FmPort, e_FmPortCounters counter)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    bool                bmiCounter = FALSE;
    volatile uint32_t   *p_Reg;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_DEQ_TOTAL):
        case(e_FM_PORT_COUNTERS_DEQ_FROM_DEFAULT):
        case(e_FM_PORT_COUNTERS_DEQ_CONFIRM ):
            /* check that counter is available for the port type */
            if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
            {
                REPORT_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for Rx ports"));
                return 0;
            }
            bmiCounter = FALSE;
        case(e_FM_PORT_COUNTERS_ENQ_TOTAL):
            bmiCounter = FALSE;
            break;
        default: /* BMI counters (or error - will be checked in BMI routine )*/
            bmiCounter = TRUE;
            break;
    }

    if(bmiCounter)
    {
        switch(p_FmPort->portType)
        {
            case(e_FM_PORT_TYPE_RX_10G):
            case(e_FM_PORT_TYPE_RX):
                if(BmiRxPortCheckAndGetCounterPtr(p_FmPort, counter, &p_Reg))
                {
                    REPORT_ERROR(MINOR, E_INVALID_STATE, NO_MSG);
                    return 0;
                }
                break;
            case(e_FM_PORT_TYPE_TX_10G):
            case(e_FM_PORT_TYPE_TX):
                if(BmiTxPortCheckAndGetCounterPtr(p_FmPort, counter, &p_Reg))
                {
                    REPORT_ERROR(MINOR, E_INVALID_STATE, NO_MSG);
                    return 0;
                }
                break;
            case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
                if(BmiOhPortCheckAndGetCounterPtr(p_FmPort, counter, &p_Reg))
                {
                    REPORT_ERROR(MINOR, E_INVALID_STATE, NO_MSG);
                    return 0;
                }
                break;
            default:
                REPORT_ERROR(MINOR, E_INVALID_STATE, ("Unsupported port type"));
                return 0;
        }
        return GET_UINT32(*p_Reg);
    }
    else /* QMI counter */
    {

        /* check that counters are enabled */
        if(!(GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc) & QMI_PORT_CFG_EN_COUNTERS))
        {
            REPORT_ERROR(MINOR, E_INVALID_STATE, ("Requested counter was not enabled"));
            return 0;
        }

        /* Set counter */
        switch(counter)
        {
           case(e_FM_PORT_COUNTERS_ENQ_TOTAL):
                return GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnetfc);
            case(e_FM_PORT_COUNTERS_DEQ_TOTAL):
                return GET_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndtfc);
            case(e_FM_PORT_COUNTERS_DEQ_FROM_DEFAULT):
                return GET_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndfdc);
            case(e_FM_PORT_COUNTERS_DEQ_CONFIRM):
                return GET_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndcc);
            default:
                REPORT_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available"));
                return 0;
        }
    }

    return 0;
}

t_Error FM_PORT_ModifyCounter(t_Handle h_FmPort, e_FmPortCounters counter, uint32_t value)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    bool                bmiCounter = FALSE;
    volatile uint32_t   *p_Reg;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    switch(counter)
    {
        case(e_FM_PORT_COUNTERS_DEQ_TOTAL):
        case(e_FM_PORT_COUNTERS_DEQ_FROM_DEFAULT):
        case(e_FM_PORT_COUNTERS_DEQ_CONFIRM ):
            /* check that counter is available for the port type */
            if((p_FmPort->portType == e_FM_PORT_TYPE_RX) || (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
                        RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for Rx ports"));
        case(e_FM_PORT_COUNTERS_ENQ_TOTAL):
            bmiCounter = FALSE;
            break;
        default: /* BMI counters (or error - will be checked in BMI routine )*/
            bmiCounter = TRUE;
            break;
    }

    if(bmiCounter)
    {
        switch(p_FmPort->portType)
        {
            case(e_FM_PORT_TYPE_RX_10G):
            case(e_FM_PORT_TYPE_RX):
               if(BmiRxPortCheckAndGetCounterPtr(p_FmPort, counter, &p_Reg))
                    RETURN_ERROR(MINOR, E_INVALID_STATE, NO_MSG);
                break;
            case(e_FM_PORT_TYPE_TX_10G):
            case(e_FM_PORT_TYPE_TX):
               if(BmiTxPortCheckAndGetCounterPtr(p_FmPort, counter, &p_Reg))
                    RETURN_ERROR(MINOR, E_INVALID_STATE, NO_MSG);
                break;
            case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
               if(BmiOhPortCheckAndGetCounterPtr(p_FmPort, counter, &p_Reg))
                    RETURN_ERROR(MINOR, E_INVALID_STATE, NO_MSG);
                 break;
            default:
               RETURN_ERROR(MINOR, E_INVALID_STATE, ("Unsupported port type"));
        }
        WRITE_UINT32(*p_Reg, value);
    }
    else /* QMI counter */
    {

        /* check that counters are enabled */
        if(!(GET_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnc) & QMI_PORT_CFG_EN_COUNTERS))
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter was not enabled"));

        /* Set counter */
        switch(counter)
        {
           case(e_FM_PORT_COUNTERS_ENQ_TOTAL):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnetfc, value);
                break;
            case(e_FM_PORT_COUNTERS_DEQ_TOTAL):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndtfc, value);
                break;
            case(e_FM_PORT_COUNTERS_DEQ_FROM_DEFAULT):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndfdc, value);
                break;
            case(e_FM_PORT_COUNTERS_DEQ_CONFIRM):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndcc, value);
                break;
            default:
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available"));
        }
    }

    return E_OK;
}

uint32_t FM_PORT_GetAllocBufCounter(t_Handle h_FmPort, uint8_t poolId)
{
    t_FmPort        *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t        extPoolReg;
    uint8_t         tmpPool;
    uint8_t         i;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if((p_FmPort->portType != e_FM_PORT_TYPE_RX) && (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
    {
        REPORT_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for non-Rx ports"));
        return 0;
    }

    for(i=0;i<FM_PORT_MAX_NUM_OF_EXT_POOLS;i++)
    {
        extPoolReg = GET_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_ebmpi[i]);
        if (extPoolReg & BMI_EXT_BUF_POOL_VALID)
        {
            tmpPool = (uint8_t)((extPoolReg & BMI_EXT_BUF_POOL_ID_MASK) >> BMI_EXT_BUF_POOL_ID_SHIFT);
            if(tmpPool == poolId)
            {
                if(extPoolReg & BMI_EXT_BUF_POOL_EN_COUNTER)
                    return  GET_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_acnt[i]);
                else
                {
                    REPORT_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not enabled"));
                    return 0;
                }
            }
        }
    }
    REPORT_ERROR(MINOR, E_INVALID_STATE, ("Pool %d is not used", poolId));
    return 0;
}

t_Error FM_PORT_ModifyAllocBufCounter(t_Handle h_FmPort, uint8_t poolId, uint32_t value)
{
    t_FmPort        *p_FmPort = (t_FmPort *)h_FmPort;
    uint32_t        extPoolReg;
    uint8_t         tmpPool;
    uint8_t         i;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if((p_FmPort->portType != e_FM_PORT_TYPE_RX) && (p_FmPort->portType == e_FM_PORT_TYPE_RX_10G))
        RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not available for non-Rx ports"));


    for(i=0;i<FM_PORT_MAX_NUM_OF_EXT_POOLS;i++)
    {
        extPoolReg = GET_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_ebmpi[i]);
        if (extPoolReg & BMI_EXT_BUF_POOL_VALID)
        {
            tmpPool = (uint8_t)((extPoolReg & BMI_EXT_BUF_POOL_ID_MASK) >> BMI_EXT_BUF_POOL_ID_SHIFT);
            if(tmpPool == poolId)
            {
                if(extPoolReg & BMI_EXT_BUF_POOL_EN_COUNTER)
                {
                    WRITE_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_acnt[i], value);
                    return E_OK;
                }
                else
                    RETURN_ERROR(MINOR, E_INVALID_STATE, ("Requested counter is not enabled"));
            }
        }
    }
    RETURN_ERROR(MINOR, E_INVALID_STATE, ("Pool %d is not used", poolId));
}

bool FM_PORT_IsStalled(t_Handle h_FmPort)
{
    t_FmPort    *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error     err;
    bool        isStalled;

    SANITY_CHECK_RETURN_VALUE(p_FmPort, E_INVALID_HANDLE, FALSE);
    SANITY_CHECK_RETURN_VALUE(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE, FALSE);

    err = FmIsPortStalled(p_FmPort->h_Fm, p_FmPort->hardwarePortId, &isStalled);
    if(err != E_OK)
    {
        REPORT_ERROR(MINOR, err, NO_MSG);
        return TRUE;
    }
    return isStalled;
}

t_Error FM_PORT_ReleaseStalled(t_Handle h_FmPort)
{
    t_FmPort        *p_FmPort = (t_FmPort*)h_FmPort;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    return FmResumeStalledPort(p_FmPort->h_Fm, p_FmPort->hardwarePortId);
}

t_Error FM_PORT_SetRxL4ChecksumVerify(t_Handle h_FmPort, bool l4Checksum)
{
    t_FmPort *p_FmPort = (t_FmPort*)h_FmPort;
    uint32_t tmpReg;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if ((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
        (p_FmPort->portType != e_FM_PORT_TYPE_RX))
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx ports only"));

    tmpReg = GET_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne);
    if (l4Checksum)
        tmpReg &= ~BMI_PORT_RFNE_FRWD_DCL4C;
    else
        tmpReg |= BMI_PORT_RFNE_FRWD_DCL4C;
    WRITE_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne, tmpReg);

    return E_OK;
}


/*       API Run-time PCD Control unit functions        */

t_Error FM_PORT_PcdPlcrAllocProfiles(t_Handle h_FmPort, uint16_t numOfProfiles)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error                     err = E_OK;

    p_FmPort->h_FmPcd = FmGetPcdHandle(p_FmPort->h_Fm);
    ASSERT_COND(p_FmPort->h_FmPcd);

    if(numOfProfiles)
    {
        err = FmPcdPlcrAllocProfiles(p_FmPort->h_FmPcd, p_FmPort->hardwarePortId, numOfProfiles);
        if(err)
            RETURN_ERROR(MAJOR, err,NO_MSG);
    }
    FmPcdPortRegister(p_FmPort->h_FmPcd, h_FmPort, p_FmPort->hardwarePortId);

    return E_OK;
}

t_Error FM_PORT_PcdPlcrFreeProfiles(t_Handle h_FmPort)
{
    t_FmPort                    *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error                     err = E_OK;

    err = FmPcdPlcrFreeProfiles(p_FmPort->h_FmPcd, p_FmPort->hardwarePortId);
    if(err)
        RETURN_ERROR(MAJOR, err,NO_MSG);
    return E_OK;
}

t_Error FM_PORT_PcdKgModifyInitialScheme (t_Handle h_FmPort, t_FmPcdKgSchemeSelect *p_FmPcdKgScheme)
{
    t_FmPort                *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t       *p_BmiHpnia = NULL;
    uint32_t                tmpReg;
    uint8_t                 relativeSchemeId;
    uint8_t                 physicalSchemeId;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->pcdEngines & FM_PCD_KG , E_INVALID_STATE);

    tmpReg = (uint32_t)((p_FmPort->pcdEngines & FM_PCD_CC)? NIA_KG_CC_EN:0);
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiHpnia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfpne;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiHpnia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofpne;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));
    }

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);
    /* if we want to change to direct scheme, we need to check that this scheme is valid */
    if(p_FmPcdKgScheme->direct)
    {
        physicalSchemeId = (uint8_t)(PTR_TO_UINT(p_FmPcdKgScheme->h_DirectScheme)-1);
        /* check that this scheme is bound to this port */
        if(!(p_FmPort->schemesPerPortVector &  (uint32_t)(1 << (31 - (uint32_t)physicalSchemeId))))
        {
            RELEASE_LOCK(p_FmPort->lock);
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("called with a scheme that is not bound to this port"));
        }

        relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmPort->h_FmPcd, physicalSchemeId);
        if(relativeSchemeId >= FM_PCD_KG_NUM_OF_SCHEMES)
        {
            RELEASE_LOCK(p_FmPort->lock);
            RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, ("called with invalid Scheme "));
        }

        if(!FmPcdKgIsSchemeValidSw(p_FmPort->h_FmPcd, relativeSchemeId))
        {
            RELEASE_LOCK(p_FmPort->lock);
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("called with uninitialized Scheme "));
        }

        WRITE_UINT32(*p_BmiHpnia, NIA_ENG_KG | tmpReg | NIA_KG_DIRECT | (uint32_t)physicalSchemeId);
    }
    else /* change to indirect scheme */
        WRITE_UINT32(*p_BmiHpnia, NIA_ENG_KG | tmpReg);
    RELEASE_LOCK(p_FmPort->lock);

    return E_OK;
}

t_Error     FM_PORT_PcdPlcrModifyInitialProfile (t_Handle h_FmPort, t_Handle h_Profile)
{
    t_FmPort                        *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t               *p_BmiNia;
    volatile uint32_t               *p_BmiHpnia;
    uint32_t                        tmpReg;
    uint16_t                        absoluteProfileId = (uint16_t)(PTR_TO_UINT(h_Profile)-1);

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->pcdEngines & FM_PCD_PLCR , E_INVALID_STATE);

    /* check relevancy of this routine  - only when policer is used
    directly after BMI or Parser */
    if((p_FmPort->pcdEngines & FM_PCD_KG) || (p_FmPort->pcdEngines & FM_PCD_CC))
        RETURN_ERROR(MINOR, E_INVALID_STATE, ("relevant only when PCD support mode is e_FM_PCD_SUPPORT_PLCR_ONLY or e_FM_PCD_SUPPORT_PRS_AND_PLCR"));

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne;
            p_BmiHpnia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfpne;
            tmpReg = GET_UINT32(*p_BmiNia) & BMI_RFNE_FDCS_MASK;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofne;
            p_BmiHpnia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofpne;
            tmpReg = 0;
            break;
        default:
           RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));
    }

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);
    if(!FmPcdPlcrIsProfileValid(p_FmPort->h_FmPcd, absoluteProfileId))
    {
        RELEASE_LOCK(p_FmPort->lock);
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("Invalid profile"));
    }

    tmpReg = (uint32_t)(NIA_ENG_PLCR | NIA_PLCR_ABSOLUTE | absoluteProfileId);

    if(p_FmPort->pcdEngines & FM_PCD_PRS) /* e_FM_PCD_SUPPORT_PRS_AND_PLCR */
    {
        /* update BMI HPNIA */
        WRITE_UINT32(*p_BmiHpnia, tmpReg);
    }
    else /* e_FM_PCD_SUPPORT_PLCR_ONLY */
    {
        /* rfne may contain FDCS bits, so first we read them. */
        tmpReg |= (GET_UINT32(*p_BmiNia) & BMI_RFNE_FDCS_MASK);
        /* update BMI NIA */
        WRITE_UINT32(*p_BmiNia, tmpReg);
    }
    RELEASE_LOCK(p_FmPort->lock);

    return E_OK;
}


t_Error FM_PORT_PcdCcModifyTree (t_Handle h_FmPort, t_Handle h_CcTree)
{
    t_FmPort                            *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error                             err = E_OK;
    volatile uint32_t                   *p_BmiCcBase=NULL;
    volatile uint32_t                   *p_BmiNia=NULL;
    uint32_t                            ccTreePhysOffset;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_VALUE);

    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for non-independant mode ports only"));

    /* get PCD registers pointers */
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofne;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));
    }

    /* check that current NIA is BMI to BMI */
    if((GET_UINT32(*p_BmiNia) & ~BMI_RFNE_FDCS_MASK) != (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME))
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("may be called only for ports in BMI-to-BMI state."));

/*TODO - to take care of changes due to previous tree. Maybe in the previous tree where chnged pndn, pnen ...
         it has to be returned to the default state - initially*/

    p_FmPort->requiredAction = 0;

    if(p_FmPort->pcdEngines & FM_PCD_CC)
    {
        switch(p_FmPort->portType)
        {
            case(e_FM_PORT_TYPE_RX_10G):
            case(e_FM_PORT_TYPE_RX):
                p_BmiCcBase = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rccb;
                break;
            case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
                p_BmiCcBase = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_occb;
                break;
            default:
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid port type"));
        }

        if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
            return ERROR_CODE(E_BUSY);
        err = FmPcdCcBindTree(p_FmPort->h_FmPcd, h_CcTree, &ccTreePhysOffset, h_FmPort);
        if(err)
        {
            RELEASE_LOCK(p_FmPort->lock);
            RETURN_ERROR(MINOR, err, NO_MSG);
        }
        WRITE_UINT32(*p_BmiCcBase, ccTreePhysOffset);

        p_FmPort->ccTreeId = h_CcTree;
        RELEASE_LOCK(p_FmPort->lock);
    }
    else
        RETURN_ERROR(MINOR, E_INVALID_STATE, ("Coarse CLassification not defined for this port."));

    return E_OK;
}

t_Error FM_PORT_AttachPCD(t_Handle h_FmPort)
{

    t_FmPort        *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error         err = E_OK;

    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for non-independant mode ports only"));

    /* TODO - may add here checks for:
        SP (or in sw: schemes)
        CPP (or in sw clsPlan)
        Parser enabled and configured(?)
        Tree(?)
        Profile - only if direct.
        Scheme - only if direct
    */

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);
    err = FmPortAttachPCD(h_FmPort);
    RELEASE_LOCK(p_FmPort->lock);

    return err;
}

t_Error FM_PORT_DetachPCD(t_Handle h_FmPort)
{
    t_FmPort                            *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t                   *p_BmiNia=NULL;

    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for non-independant mode ports only"));

    /* get PCD registers pointers */
    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofne;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));
    }

    WRITE_UINT32(*p_BmiNia, (p_FmPort->savedBmiNia & BMI_RFNE_FDCS_MASK) | (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME));

/*TODO - not atomic - it seems that port has to be disabled*/
    if(p_FmPort->requiredAction & UPDATE_NIA_PNEN)
    {
        switch(p_FmPort->portType)
        {
            case(e_FM_PORT_TYPE_TX_10G):
            case(e_FM_PORT_TYPE_TX):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnen, NIA_ENG_BMI | NIA_BMI_AC_TX_RELEASE);
                break;
            case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            case(e_FM_PORT_TYPE_RX):
            case(e_FM_PORT_TYPE_RX_10G):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->fmqm_pnen, NIA_ENG_BMI | NIA_BMI_AC_RELEASE);
                break;
           default:
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Can not reach this stage"));
        }
    }

    if(p_FmPort->requiredAction & UPDATE_NIA_PNDN)
    {
        switch(p_FmPort->portType)
        {
            case(e_FM_PORT_TYPE_TX_10G):
            case(e_FM_PORT_TYPE_TX):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndn, NIA_ENG_BMI | NIA_BMI_AC_TX);
                break;
            case(e_FM_PORT_TYPE_OH_HOST_COMMAND):
            case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
                WRITE_UINT32(p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs.fmqm_pndn, NIA_ENG_BMI | NIA_BMI_AC_FETCH);
                break;
            default:
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Can not reach this stage"));
        }
    }


    if(p_FmPort->requiredAction  & UPDATE_FMFP_PRC_WITH_ONE_RISC_ONLY)
        if(FmSetNumOfRiscsPerPort(p_FmPort->h_Fm, p_FmPort->hardwarePortId, 2)!= E_OK)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
    return E_OK;
}

t_Error FM_PORT_SetPCD(t_Handle h_FmPort, t_FmPortPcdParams *p_PcdParams)
{
    t_FmPort                                *p_FmPort = (t_FmPort*)h_FmPort;
    t_FmPcdKgInterModuleBindPortToSchemes   schemeBind;
    t_Error                                 err = E_OK;
    uint8_t                                 i;

    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for non-independent mode ports only"));

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);
    p_FmPort->h_FmPcd = FmGetPcdHandle(p_FmPort->h_Fm);
    ASSERT_COND(p_FmPort->h_FmPcd);

    err = SetPcd( h_FmPort, p_PcdParams);
    if(err)
    {
        RELEASE_LOCK(p_FmPort->lock);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    if(p_FmPort->pcdEngines & FM_PCD_KG)
    {
        schemeBind.netEnvId = p_FmPort->netEnvId;
        schemeBind.hardwarePortId = p_FmPort->hardwarePortId;
        schemeBind.numOfSchemes = p_PcdParams->p_KgParams->numOfSchemes;
        schemeBind.useClsPlan = p_FmPort->useClsPlan;
        for(i = 0;i<schemeBind.numOfSchemes;i++)
            schemeBind.schemesIds[i] = (uint8_t)(PTR_TO_UINT(p_PcdParams->p_KgParams->h_Schemes[i])-1);

        err = FmPcdKgBindPortToSchemes(p_FmPort->h_FmPcd, &schemeBind);
        if(err)
        {
            DeletePcd(p_FmPort);
            RELEASE_LOCK(p_FmPort->lock);
            RETURN_ERROR(MAJOR, err, NO_MSG);
        }
    }

    if ((p_FmPort->pcdEngines & FM_PCD_PRS) && (p_PcdParams->p_PrsParams->includeInPrsStatistics))
        FmPcdPrsIncludePortInStatistics(p_FmPort->h_FmPcd, p_FmPort->hardwarePortId, TRUE);

    FmPcdIncNetEnvOwners(p_FmPort->h_FmPcd, p_FmPort->netEnvId);

    err = FmPortAttachPCD(h_FmPort);
    RELEASE_LOCK(p_FmPort->lock);

    return err;
}

t_Error FM_PORT_DeletePCD(t_Handle h_FmPort)
{
    t_FmPort                                *p_FmPort = (t_FmPort*)h_FmPort;
    t_FmPcdKgInterModuleBindPortToSchemes   schemeBind;
    t_Error                                 err = E_OK;

    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);

    if (p_FmPort->imEn)
        RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for non-independant mode ports only"));

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);

    err = FM_PORT_DetachPCD(h_FmPort);
    if(err)
    {
        RELEASE_LOCK(p_FmPort->lock);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    FmPcdDecNetEnvOwners(p_FmPort->h_FmPcd, p_FmPort->netEnvId);

    /* we do it anyway, instead of checking if included */
    if (FmIsMaster(p_FmPort->h_Fm) &&
        (p_FmPort->pcdEngines & FM_PCD_PRS))
        FmPcdPrsIncludePortInStatistics(p_FmPort->h_FmPcd, p_FmPort->hardwarePortId, FALSE);

    if(p_FmPort->pcdEngines & FM_PCD_KG)
    {
        /* unbind all schemes */
        p_FmPort->schemesPerPortVector = GetPortSchemeBindParams(p_FmPort, &schemeBind);

        err = FmPcdKgUnbindPortToSchemes(p_FmPort->h_FmPcd, &schemeBind);
        if(err)
        {
            RELEASE_LOCK(p_FmPort->lock);
            RETURN_ERROR(MAJOR, err, NO_MSG);
        }
    }

    err = DeletePcd(h_FmPort);
    RELEASE_LOCK(p_FmPort->lock);

    return err;
}

t_Error  FM_PORT_PcdKgBindSchemes (t_Handle h_FmPort, t_FmPcdPortSchemesParams *p_PortScheme)
{
    t_FmPort                                *p_FmPort = (t_FmPort*)h_FmPort;
    t_FmPcdKgInterModuleBindPortToSchemes   schemeBind;
    t_Error                                 err = E_OK;
    uint32_t                                tmpScmVec=0;
    int                                     i;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->pcdEngines & FM_PCD_KG , E_INVALID_STATE);

    schemeBind.netEnvId = p_FmPort->netEnvId;
    schemeBind.hardwarePortId = p_FmPort->hardwarePortId;
    schemeBind.numOfSchemes = p_PortScheme->numOfSchemes;
    schemeBind.useClsPlan = p_FmPort->useClsPlan;
    for (i=0; i<schemeBind.numOfSchemes; i++)
    {
        schemeBind.schemesIds[i] = (uint8_t)(PTR_TO_UINT(p_PortScheme->h_Schemes[i])-1);
        /* build vector */
        tmpScmVec |= 1 << (31 - (uint32_t)schemeBind.schemesIds[i]);
    }

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);
    err = FmPcdKgBindPortToSchemes(p_FmPort->h_FmPcd, &schemeBind);
    if (err == E_OK)
        p_FmPort->schemesPerPortVector |= tmpScmVec;
    RELEASE_LOCK(p_FmPort->lock);

    return err;
}

t_Error FM_PORT_PcdKgUnbindSchemes (t_Handle h_FmPort, t_FmPcdPortSchemesParams *p_PortScheme)
{
    t_FmPort                                *p_FmPort = (t_FmPort*)h_FmPort;
    t_FmPcdKgInterModuleBindPortToSchemes   schemeBind;
    t_Error                                 err = E_OK;
    uint32_t                                tmpScmVec=0;
    int                                     i;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->pcdEngines & FM_PCD_KG , E_INVALID_STATE);

    schemeBind.netEnvId = p_FmPort->netEnvId;
    schemeBind.hardwarePortId = p_FmPort->hardwarePortId;
    schemeBind.numOfSchemes = p_PortScheme->numOfSchemes;
    for (i=0; i<schemeBind.numOfSchemes; i++)
    {
        schemeBind.schemesIds[i] = (uint8_t)(PTR_TO_UINT(p_PortScheme->h_Schemes[i])-1);
        /* build vector */
        tmpScmVec |= 1 << (31 - (uint32_t)schemeBind.schemesIds[i]);
    }

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);
    err = FmPcdKgUnbindPortToSchemes(p_FmPort->h_FmPcd, &schemeBind);
    if (err == E_OK)
        p_FmPort->schemesPerPortVector &= ~tmpScmVec;
    RELEASE_LOCK(p_FmPort->lock);

    return err;
}

t_Error FM_PORT_PcdPrsModifyStartOffset (t_Handle h_FmPort, t_FmPcdPrsStart *p_FmPcdPrsStart)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    volatile uint32_t   *p_BmiPrsStartOffset = NULL;
    volatile uint32_t   *p_BmiNia = NULL;
    uint32_t            tmpReg;
    uint8_t             hdrNum;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->pcdEngines & FM_PCD_PRS , E_INVALID_STATE);

    switch(p_FmPort->portType)
    {
        case(e_FM_PORT_TYPE_RX_10G):
        case(e_FM_PORT_TYPE_RX):
            p_BmiPrsStartOffset = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rpso;
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rfne;
            tmpReg = GET_UINT32(*p_BmiNia) & BMI_RFNE_FDCS_MASK;
            break;
        case(e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            p_BmiPrsStartOffset = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_opso;
            p_BmiNia = &p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ofne;
            tmpReg = 0;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("available for Rx and offline parsing ports only"));
    }

    /* check that current NIA is BMI to BMI */
    if((GET_UINT32(*p_BmiNia) & ~BMI_RFNE_FDCS_MASK) != (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME))
            RETURN_ERROR(MAJOR, E_INVALID_OPERATION, ("may be called only for ports in BMI-to-BMI state."));

    if (!TRY_LOCK(p_FmPort->h_Spinlock, &p_FmPort->lock))
        return ERROR_CODE(E_BUSY);
    /* set the first header */
    GET_PRS_HDR_NUM(hdrNum, p_FmPcdPrsStart->firstPrsHdr);
    if ((hdrNum == ILLEGAL_HDR_NUM) || (hdrNum == NO_HDR_NUM))
    {
        RELEASE_LOCK(p_FmPort->lock);
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Unsupported header."));
    }
    WRITE_UINT32(*p_BmiNia, (uint32_t)(NIA_ENG_PRS | (uint32_t)hdrNum | tmpReg));

    /* set start parsing offset */
    WRITE_UINT32(*p_BmiPrsStartOffset, (uint32_t)(p_FmPcdPrsStart->parsingOffset + p_FmPort->internalBufferOffset));
    RELEASE_LOCK(p_FmPort->lock);

    return E_OK;
}

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FM_PORT_DumpRegs(t_Handle h_FmPort)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    t_Error             err = E_OK;
    char                arr[30];
    uint8_t             flag;
    int                 i=0;

    DECLARE_DUMP;

    SANITY_CHECK_RETURN_ERROR(h_FmPort, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPort->p_FmPortDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortQmiRegs, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPort->p_FmPortBmiRegs, E_INVALID_HANDLE);

    switch (p_FmPort->portType)
    {
        case (e_FM_PORT_TYPE_OH_OFFLINE_PARSING):
            strcpy(arr, "PORT_TYPE_OFFLINE_PARSING");
            flag = 0;
            break;
        case (e_FM_PORT_TYPE_OH_HOST_COMMAND):
            strcpy(arr, "PORT_TYPE_HOST_COMMAND");
            flag = 0;
            break;
        case (e_FM_PORT_TYPE_RX):
            strcpy(arr, "PORT_TYPE_RX");
            flag = 1;
            break;
        case (e_FM_PORT_TYPE_RX_10G):
            strcpy(arr, "PORT_TYPE_RX_10G");
            flag = 1;
            break;
        case (e_FM_PORT_TYPE_TX):
            strcpy(arr, "PORT_TYPE_TX");
            flag = 2;
            break;
        case (e_FM_PORT_TYPE_TX_10G):
            strcpy(arr, "PORT_TYPE_TX_10G");
            flag = 2;
            break;
        default:
            return ERROR_CODE(E_INVALID_VALUE);
    }

    DUMP_TITLE(UINT_TO_PTR(p_FmPort->hardwarePortId), ("PortId for %s %d", arr, p_FmPort->portId ));
    DUMP_TITLE(p_FmPort->p_FmPortBmiRegs, ("Bmi Port Regs"));

    err = FmDumpPortRegs(p_FmPort->h_Fm, p_FmPort->hardwarePortId);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    switch(flag)
    {
        case(0):

        DUMP_SUBTITLE(("\n"));
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ocfg);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ost);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_oda);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofdne);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofne);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofca);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofpne);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_opso);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_opp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_occb);

        DUMP_TITLE(&(p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_oprai), ("fmbm_oprai"));
        DUMP_SUBSTRUCT_ARRAY(i, FM_PORT_PRS_RESULT_NUM_OF_WORDS)
        {
            DUMP_MEMORY(&(p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_oprai[i]), sizeof(uint32_t));
        }
        DUMP_SUBTITLE(("\n"));
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofqid );
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_oefqid);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofsdm );
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofsem );
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofene );
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_orlmts);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_orlmt);

        {
#ifndef FM_NO_OP_OBSERVED_POOLS
            t_FmRevisionInfo    revInfo;

            FM_GetRevision(p_FmPort->h_Fm, &revInfo);
            if (revInfo.majorRev == 4)
#endif /* !FM_NO_OP_OBSERVED_POOLS */
            {
                DUMP_TITLE(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_oebmpi, ("fmbm_oebmpi"));

                DUMP_SUBSTRUCT_ARRAY(i, FM_PORT_MAX_NUM_OF_OBSERVED_EXT_POOLS)
                {
                    DUMP_MEMORY(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_oebmpi[i], sizeof(uint32_t));
                }
                DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ocgm);
            }
        }

        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ostc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofrc );
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofdc );
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofledc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofufdc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_offc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofwdc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofldec);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_opc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_opcp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_occn);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_otuc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_oduc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs,fmbm_ofuc);
        break;
    case(1):
        DUMP_SUBTITLE(("\n"));
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rcfg);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rst);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rda);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_reth);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfed);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_ricp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rebm);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfne);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfca);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfpne);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rpso);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rpp);

        DUMP_TITLE(&(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rprai), ("fmbm_rprai"));
        DUMP_SUBSTRUCT_ARRAY(i, FM_PORT_PRS_RESULT_NUM_OF_WORDS)
        {
            DUMP_MEMORY(&(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_rprai[i]), sizeof(uint32_t));
        }
        DUMP_SUBTITLE(("\n"));
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfqid);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_refqid);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfsdm);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfsem);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfene);
        DUMP_TITLE(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_ebmpi, ("fmbm_ebmpi"));
        DUMP_SUBSTRUCT_ARRAY(i, FM_PORT_MAX_NUM_OF_EXT_POOLS)
        {
            DUMP_MEMORY(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_ebmpi[i], sizeof(uint32_t));
        }
        DUMP_TITLE(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_acnt, ("fmbm_acnt"));
        DUMP_SUBSTRUCT_ARRAY(i, FM_PORT_MAX_NUM_OF_EXT_POOLS)
        {
            DUMP_MEMORY(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_acnt[i], sizeof(uint32_t));
        }
        DUMP_TITLE(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_cgm, ("fmbm_cgm"));
        DUMP_SUBSTRUCT_ARRAY(i, FM_PORT_NUM_OF_CONGESTION_GRPS/32)
        {
            DUMP_MEMORY(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_cgm[i], sizeof(uint32_t));
        }
        DUMP_SUBTITLE(("\n"));
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_mpd);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rstc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfrc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfbc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rlfc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rffc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfcd);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfldec);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rodc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rpc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rpcp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rccn);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rtuc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rrquc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rduc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rfuc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rpac);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs,fmbm_rdbg);
        break;
    case(2):

        DUMP_SUBTITLE(("\n"));
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tcfg);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tst);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tda);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfed);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_ticp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfne);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfca);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tcfqid);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfeqid);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfene);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_trlmts);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_trlmt);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tstc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfrc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfdc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfledc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfufdc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tpc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tpcp);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tccn);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_ttuc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_ttcquc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tduc);
        DUMP_VAR(&p_FmPort->p_FmPortBmiRegs->txPortBmiRegs,fmbm_tfuc);
        break;

   default:
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Invalid flag"));
    }

    DUMP_TITLE(p_FmPort->p_FmPortQmiRegs, ("Qmi Port Regs"));

    DUMP_VAR(p_FmPort->p_FmPortQmiRegs,fmqm_pnc);
    DUMP_VAR(p_FmPort->p_FmPortQmiRegs,fmqm_pns);
    DUMP_VAR(p_FmPort->p_FmPortQmiRegs,fmqm_pnts);
    DUMP_VAR(p_FmPort->p_FmPortQmiRegs,fmqm_pnen);
    DUMP_VAR(p_FmPort->p_FmPortQmiRegs,fmqm_pnetfc);

    if(flag !=1)
    {
        DUMP_VAR(&p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs,fmqm_pndn);
        DUMP_VAR(&p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs,fmqm_pndc);
        DUMP_VAR(&p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs,fmqm_pndtfc);
        DUMP_VAR(&p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs,fmqm_pndfdc);
        DUMP_VAR(&p_FmPort->p_FmPortQmiRegs->nonRxQmiRegs,fmqm_pndcc);
    }

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && ... */

t_Error FM_PORT_AddCongestionGrps(t_Handle h_FmPort, t_FmPortCongestionGrps *p_CongestionGrps)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    bool                tmpArray[FM_PORT_NUM_OF_CONGESTION_GRPS], opPort;
    int                 i;
    uint8_t             mod;
    uint32_t            tmpReg = 0;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);

    {
#ifdef FM_NO_OP_OBSERVED_CGS
        t_FmRevisionInfo    revInfo;

        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if (revInfo.majorRev != 4)
        {
            if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
                    (p_FmPort->portType != e_FM_PORT_TYPE_RX))
                RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Available for Rx ports only"));
        }
        else
#endif /* FM_NO_OP_OBSERVED_CGS */
        if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
                (p_FmPort->portType != e_FM_PORT_TYPE_RX) &&
                (p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
            RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Available for Rx & OP ports only"));
    }

    opPort = (bool)((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) ? TRUE:FALSE);

    /* to minimize memory access (groups may belong to the same regsiter, and may
    be out of order), we first collect all information into a 256 booleans array,
    representing each possible group. */

    memset(&tmpArray, 0, FM_PORT_NUM_OF_CONGESTION_GRPS*sizeof(bool));
    for(i=0;i<p_CongestionGrps->numOfCongestionGrpsToConsider;i++)
        tmpArray[p_CongestionGrps->congestionGrpsToConsider[i]] = TRUE;

    for(i=0;i<FM_PORT_NUM_OF_CONGESTION_GRPS;i++)
    {
        mod = (uint8_t)(i%32);
        /* each 32 congestion groups are represented by a register */
        if (mod == 0) /* first in a 32 bunch of congestion groups, get the currest register state  */
            tmpReg = opPort ?   GET_UINT32(p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ocgm):
                                GET_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_cgm[7-i/32]);

        /* set in the register, the bit representing the relevant congestion group. */
        if(tmpArray[i])
            tmpReg |=  (0x00000001 << (uint32_t)mod);

        if (mod == 31) /* last in a 32 bunch of congestion groups - write the corresponding register */
        {
            if(opPort)
                WRITE_UINT32(p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ocgm, tmpReg);
            else
                WRITE_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_cgm[7-i/32], tmpReg);
        }
    }

    return  E_OK;
}

t_Error FM_PORT_RemoveCongestionGrps(t_Handle h_FmPort, t_FmPortCongestionGrps *p_CongestionGrps)
{
    t_FmPort            *p_FmPort = (t_FmPort*)h_FmPort;
    bool                tmpArray[FM_PORT_NUM_OF_CONGESTION_GRPS], opPort;
    int                 i;
    uint8_t             mod;
    uint32_t            tmpReg = 0;

    SANITY_CHECK_RETURN_ERROR(p_FmPort, E_INVALID_HANDLE);

    {
#ifdef FM_NO_OP_OBSERVED_CGS
        t_FmRevisionInfo    revInfo;

        FM_GetRevision(p_FmPort->h_Fm, &revInfo);
        if (revInfo.majorRev != 4)
        {
            if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
                    (p_FmPort->portType != e_FM_PORT_TYPE_RX))
                RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Available for Rx ports only"));
        }
        else
#endif /* FM_NO_OP_OBSERVED_CGS */
        if((p_FmPort->portType != e_FM_PORT_TYPE_RX_10G) &&
                (p_FmPort->portType != e_FM_PORT_TYPE_RX) &&
                (p_FmPort->portType != e_FM_PORT_TYPE_OH_OFFLINE_PARSING))
            RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Available for Rx & OP ports only"));
    }

    opPort = (bool)((p_FmPort->portType == e_FM_PORT_TYPE_OH_OFFLINE_PARSING) ? TRUE:FALSE);

    /* to minimize memory access (groups may belong to the same regsiter, and may
    be out of order), we first collect all information into a 256 booleans array,
    representing each possible group. */
    memset(&tmpArray, 0, FM_PORT_NUM_OF_CONGESTION_GRPS*sizeof(bool));
    for(i=0;i<p_CongestionGrps->numOfCongestionGrpsToConsider;i++)
        tmpArray[p_CongestionGrps->congestionGrpsToConsider[i]] = TRUE;

    for(i=0;i<FM_PORT_NUM_OF_CONGESTION_GRPS;i++)
    {
        mod = (uint8_t)(i%32);
        /* each 32 congestion groups are represented by a register */
        if (mod == 0) /* first in a 32 bunch of congestion groups, get the currest register state  */
            tmpReg = opPort ?   GET_UINT32(p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ocgm):
                                GET_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_cgm[7-i/32]);

        /* set in the register, the bit representing the relevant congestion group. */
        if(tmpArray[i])
            tmpReg &=  ~(0x00000001 << (uint32_t)mod);

        if (mod == 31) /* last in a 32 bunch of congestion groups - write the corresponding register */
        {
            if(opPort)
                WRITE_UINT32(p_FmPort->p_FmPortBmiRegs->ohPortBmiRegs.fmbm_ocgm, tmpReg);
            else
                WRITE_UINT32(p_FmPort->p_FmPortBmiRegs->rxPortBmiRegs.fmbm_cgm[7-i/32], tmpReg);
        }
    }

    return  E_OK;
}

