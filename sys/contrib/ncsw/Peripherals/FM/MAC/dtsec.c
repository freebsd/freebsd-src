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
 @File          dtsec.c

 @Description   FM dTSEC ...
*//***************************************************************************/

#include "std_ext.h"
#include "error_ext.h"
#include "string_ext.h"
#include "xx_ext.h"
#include "endian_ext.h"
#include "crc_mac_addr_ext.h"
#include "debug_ext.h"

#include "fm_common.h"
#include "dtsec.h"


/*****************************************************************************/
/*                      Internal routines                                    */
/*****************************************************************************/

static t_Error CheckInitParameters(t_Dtsec *p_Dtsec)
{
    if(ENET_SPEED_FROM_MODE(p_Dtsec->enetMode) >= e_ENET_SPEED_10000)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Ethernet 1G MAC driver only supports 1G or lower speeds"));
    if(p_Dtsec->macId >= FM_MAX_NUM_OF_1G_MACS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("macId can not be greater than the number of 1G MACs"));
    if(p_Dtsec->addr == 0)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Ethernet MAC Must have a valid MAC Address"));
    if(((p_Dtsec->enetMode == e_ENET_MODE_SGMII_1000) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_1000) ||
        (p_Dtsec->enetMode == e_ENET_MODE_QSGMII_1000)) &&
        p_Dtsec->p_DtsecDriverParam->halfDuplex)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Ethernet MAC 1G can't work in half duplex"));
    if(p_Dtsec->p_DtsecDriverParam->halfDuplex && (p_Dtsec->p_DtsecDriverParam)->loopback)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("LoopBack is not supported in halfDuplex mode"));
#ifdef FM_NO_RX_PREAM_ERRATA_DTSECx1
    if(p_Dtsec->p_DtsecDriverParam->preambleRxEn)
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("preambleRxEn"));
#endif /* FM_NO_RX_PREAM_ERRATA_DTSECx1 */
    if(((p_Dtsec->p_DtsecDriverParam)->preambleTxEn || (p_Dtsec->p_DtsecDriverParam)->preambleRxEn) &&( (p_Dtsec->p_DtsecDriverParam)->preambleLength != 0x7))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Preamble length should be 0x7 bytes"));
    if((p_Dtsec->p_DtsecDriverParam)->fifoTxWatermarkH<((p_Dtsec->p_DtsecDriverParam)->fifoTxThr+8))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fifoTxWatermarkH has to be at least 8 larger than fifoTxThr"));
    if((p_Dtsec->p_DtsecDriverParam)->halfDuplex &&
       (p_Dtsec->p_DtsecDriverParam->txTimeStampEn || p_Dtsec->p_DtsecDriverParam->rxTimeStampEn))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("dTSEC in half duplex mode has to be with 1588 timeStamping diable"));
    if((p_Dtsec->p_DtsecDriverParam)->actOnRxPauseFrame && (p_Dtsec->p_DtsecDriverParam)->controlFrameAccept )
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Receive control frame are not passed to the system memory so it can not be accept "));
    if((p_Dtsec->p_DtsecDriverParam)->packetAlignmentPadding  > MAX_PACKET_ALIGNMENT)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("packetAlignmentPadding can't be greater than %d ",MAX_PACKET_ALIGNMENT ));
    if(((p_Dtsec->p_DtsecDriverParam)->nonBackToBackIpg1  > MAX_INTER_PACKET_GAP) ||
        ((p_Dtsec->p_DtsecDriverParam)->nonBackToBackIpg2 > MAX_INTER_PACKET_GAP) ||
        ((p_Dtsec->p_DtsecDriverParam)->backToBackIpg > MAX_INTER_PACKET_GAP))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Inter packet gap can't be greater than %d ",MAX_INTER_PACKET_GAP ));
    if((p_Dtsec->p_DtsecDriverParam)->alternateBackoffVal > MAX_INTER_PALTERNATE_BEB)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("alternateBackoffVal can't be greater than %d ",MAX_INTER_PALTERNATE_BEB ));
    if((p_Dtsec->p_DtsecDriverParam)->maxRetransmission > MAX_RETRANSMISSION)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("maxRetransmission can't be greater than %d ",MAX_RETRANSMISSION ));
    if((p_Dtsec->p_DtsecDriverParam)->collisionWindow > MAX_COLLISION_WINDOW)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("collisionWindow can't be greater than %d ",MAX_COLLISION_WINDOW ));

    /*  If Auto negotiation process is disabled, need to */
    /*  Set up the PHY using the MII Management Interface */
    if (p_Dtsec->p_DtsecDriverParam->tbiPhyAddr > MAX_PHYS)
        RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, ("PHY address (should be 0-%d)", MAX_PHYS));
    if(!p_Dtsec->f_Exception)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("uninitialized f_Exception"));
    if(!p_Dtsec->f_Event)
        RETURN_ERROR(MAJOR, E_INVALID_HANDLE, ("uninitialized f_Event"));
    return E_OK;
}

static uint8_t GetMiiDiv(int32_t refClk)
{
    uint32_t    div,tmpClk;
    int         minRange;

    div = 1;
    minRange = (int)(refClk/40 - 1);

    tmpClk = (uint32_t)ABS(refClk/60 - 1);
    if (tmpClk < minRange)
    {
        div = 2;
        minRange = (int)tmpClk;
    }
    tmpClk = (uint32_t)ABS(refClk/60 - 1);
    if (tmpClk < minRange)
    {
        div = 3;
        minRange = (int)tmpClk;
    }
    tmpClk = (uint32_t)ABS(refClk/80 - 1);
    if (tmpClk < minRange)
    {
        div = 4;
        minRange = (int)tmpClk;
    }
    tmpClk = (uint32_t)ABS(refClk/100 - 1);
    if (tmpClk < minRange)
    {
        div = 5;
        minRange = (int)tmpClk;
    }
    tmpClk = (uint32_t)ABS(refClk/140 - 1);
    if (tmpClk < minRange)
    {
        div = 6;
        minRange = (int)tmpClk;
    }
    tmpClk = (uint32_t)ABS(refClk/280 - 1);
    if (tmpClk < minRange)
    {
        div = 7;
        minRange = (int)tmpClk;
    }

    return (uint8_t)div;
}

/* ........................................................................... */

static void SetDefaultParam(t_DtsecDriverParam *p_DtsecDriverParam)
{
    p_DtsecDriverParam->errorDisabled       = DEFAULT_errorDisabled;

    p_DtsecDriverParam->promiscuousEnable   = DEFAULT_promiscuousEnable;

    p_DtsecDriverParam->pauseExtended       = DEFAULT_pauseExtended;
    p_DtsecDriverParam->pauseTime           = DEFAULT_pauseTime;

    p_DtsecDriverParam->halfDuplex              = DEFAULT_halfDuplex;
    p_DtsecDriverParam->halfDulexFlowControlEn  = DEFAULT_halfDulexFlowControlEn;
    p_DtsecDriverParam->txTimeStampEn           = DEFAULT_txTimeStampEn;
    p_DtsecDriverParam->rxTimeStampEn           = DEFAULT_rxTimeStampEn;

    p_DtsecDriverParam->packetAlignmentPadding = DEFAULT_packetAlignment;
    p_DtsecDriverParam->controlFrameAccept     = DEFAULT_controlFrameAccept;
    p_DtsecDriverParam->groupHashExtend        = DEFAULT_groupHashExtend;
    p_DtsecDriverParam->broadcReject           = DEFAULT_broadcReject;
    p_DtsecDriverParam->rxShortFrame           = DEFAULT_rxShortFrame;
    p_DtsecDriverParam->exactMatch             = DEFAULT_exactMatch;
    p_DtsecDriverParam->debugMode              = DEFAULT_debugMode;

    p_DtsecDriverParam->loopback               = DEFAULT_loopback;
    p_DtsecDriverParam->tbiPhyAddr             = DEFAULT_tbiPhyAddr;
    p_DtsecDriverParam->actOnRxPauseFrame      = DEFAULT_actOnRxPauseFrame;
    p_DtsecDriverParam->actOnTxPauseFrame      = DEFAULT_actOnTxPauseFrame;

    p_DtsecDriverParam->preambleLength         = DEFAULT_PreAmLength;
    p_DtsecDriverParam->preambleRxEn           = DEFAULT_PreAmRxEn;
    p_DtsecDriverParam->preambleTxEn           = DEFAULT_PreAmTxEn;
    p_DtsecDriverParam->lengthCheckEnable      = DEFAULT_lengthCheckEnable;
    p_DtsecDriverParam->padAndCrcEnable        = DEFAULT_padAndCrcEnable;
    p_DtsecDriverParam->crcEnable              = DEFAULT_crcEnable;

    p_DtsecDriverParam->nonBackToBackIpg1      = DEFAULT_nonBackToBackIpg1;
    p_DtsecDriverParam->nonBackToBackIpg2      = DEFAULT_nonBackToBackIpg2;
    p_DtsecDriverParam->minIfgEnforcement      = DEFAULT_minIfgEnforcement;
    p_DtsecDriverParam->backToBackIpg          = DEFAULT_backToBackIpg;

    p_DtsecDriverParam->alternateBackoffVal    = DEFAULT_altBackoffVal;
    p_DtsecDriverParam->alternateBackoffEnable = DEFAULT_altBackoffEnable;
    p_DtsecDriverParam->backPressureNoBackoff  = DEFAULT_backPressureNoBackoff;
    p_DtsecDriverParam->noBackoff              = DEFAULT_noBackoff;
    p_DtsecDriverParam->excessDefer            = DEFAULT_excessDefer;
    p_DtsecDriverParam->maxRetransmission      = DEFAULT_maxRetransmission;
    p_DtsecDriverParam->collisionWindow        = DEFAULT_collisionWindow;

    p_DtsecDriverParam->maxFrameLength         = DEFAULT_maxFrameLength;

    p_DtsecDriverParam->fifoTxThr              = DEFAULT_fifoTxThr;
    p_DtsecDriverParam->fifoTxWatermarkH       = DEFAULT_fifoTxWatermarkH;

    p_DtsecDriverParam->fifoRxWatermarkL       = DEFAULT_fifoRxWatermarkL;
}

static void DtsecException(t_Handle h_Dtsec)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    uint32_t            event;
    t_DtsecMemMap       *p_DtsecMemMap;

    ASSERT_COND(p_Dtsec);
    p_DtsecMemMap = p_Dtsec->p_MemMap;
    ASSERT_COND(p_DtsecMemMap);

    event = GET_UINT32(p_DtsecMemMap->ievent);
    /* handle only MDIO events */
    event &= (IMASK_MMRDEN | IMASK_MMWREN);
    if(event)
    {
        event &= GET_UINT32(p_DtsecMemMap->imask);

        WRITE_UINT32(p_DtsecMemMap->ievent, event);

        if(event & IMASK_MMRDEN)
            p_Dtsec->f_Event(p_Dtsec->h_App, e_FM_MAC_EX_1G_MII_MNG_RD_COMPLET);
        if(event & IMASK_MMWREN)
            p_Dtsec->f_Event(p_Dtsec->h_App, e_FM_MAC_EX_1G_MII_MNG_WR_COMPLET);
    }
}

static void UpdateStatistics(t_Dtsec *p_Dtsec)
{
    t_DtsecMemMap   *p_DtsecMemMap = p_Dtsec->p_MemMap;
    uint32_t        car1 =  GET_UINT32(p_DtsecMemMap->car1);
    uint32_t        car2 =  GET_UINT32(p_DtsecMemMap->car2);

    if(car1)
    {
        WRITE_UINT32(p_DtsecMemMap->car1, car1);
        if(car1 & CAR1_TR64)
            p_Dtsec->internalStatistics.tr64 += VAL22BIT;
        if(car1 & CAR1_TR127)
            p_Dtsec->internalStatistics.tr127 += VAL22BIT;
        if(car1 & CAR1_TR255)
            p_Dtsec->internalStatistics.tr255 += VAL22BIT;
        if(car1 & CAR1_TR511)
            p_Dtsec->internalStatistics.tr511 += VAL22BIT;
        if(car1 & CAR1_TRK1)
            p_Dtsec->internalStatistics.tr1k += VAL22BIT;
        if(car1 & CAR1_TRMAX)
            p_Dtsec->internalStatistics.trmax += VAL22BIT;
        if(car1 & CAR1_TRMGV)
            p_Dtsec->internalStatistics.trmgv += VAL22BIT;
        if(car1 & CAR1_RBYT)
            p_Dtsec->internalStatistics.rbyt += (uint64_t)VAL32BIT;
        if(car1 & CAR1_RPKT)
            p_Dtsec->internalStatistics.rpkt += VAL22BIT;
        if(car1 & CAR1_RMCA)
            p_Dtsec->internalStatistics.rmca += VAL22BIT;
        if(car1 & CAR1_RBCA)
            p_Dtsec->internalStatistics.rbca += VAL22BIT;
        if(car1 & CAR1_RXPF)
            p_Dtsec->internalStatistics.rxpf += VAL16BIT;
        if(car1 & CAR1_RALN)
            p_Dtsec->internalStatistics.raln += VAL16BIT;
        if(car1 & CAR1_RFLR)
            p_Dtsec->internalStatistics.rflr += VAL16BIT;
        if(car1 & CAR1_RCDE)
            p_Dtsec->internalStatistics.rcde += VAL16BIT;
        if(car1 & CAR1_RCSE)
            p_Dtsec->internalStatistics.rcse += VAL16BIT;
        if(car1 & CAR1_RUND)
            p_Dtsec->internalStatistics.rund += VAL16BIT;
        if(car1 & CAR1_ROVR)
            p_Dtsec->internalStatistics.rovr += VAL16BIT;
        if(car1 & CAR1_RFRG)
            p_Dtsec->internalStatistics.rfrg += VAL16BIT;
        if(car1 & CAR1_RJBR)
            p_Dtsec->internalStatistics.rjbr += VAL16BIT;
        if(car1 & CAR1_RDRP)
            p_Dtsec->internalStatistics.rdrp += VAL16BIT;
    }
    if(car2)
    {
        WRITE_UINT32(p_DtsecMemMap->car2, car2);
        if(car2  & CAR2_TFCS)
            p_Dtsec->internalStatistics.tfcs += VAL12BIT;
        if(car2  & CAR2_TBYT)
            p_Dtsec->internalStatistics.tbyt += (uint64_t)VAL32BIT;
        if(car2  & CAR2_TPKT)
            p_Dtsec->internalStatistics.tpkt += VAL22BIT;
        if(car2  & CAR2_TMCA)
            p_Dtsec->internalStatistics.tmca += VAL22BIT;
        if(car2  & CAR2_TBCA)
            p_Dtsec->internalStatistics.tbca += VAL22BIT;
        if(car2  & CAR2_TXPF)
            p_Dtsec->internalStatistics.txpf += VAL16BIT;
        if(car2  & CAR2_TDRP)
            p_Dtsec->internalStatistics.tdrp += VAL16BIT;
    }
}

/* .............................................................................. */

static uint16_t DtsecGetMaxFrameLength(t_Handle h_Dtsec)
{
    t_Dtsec              *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_VALUE(p_Dtsec, E_INVALID_HANDLE, 0);

    return (uint16_t)GET_UINT32(p_Dtsec->p_MemMap->maxfrm);
}

static void DtsecErrException(t_Handle h_Dtsec)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    uint32_t            event;
    t_DtsecMemMap       *p_DtsecMemMap = p_Dtsec->p_MemMap;

    event = GET_UINT32(p_DtsecMemMap->ievent);
    /* do not handle MDIO events */
    event &= ~(IMASK_MMRDEN | IMASK_MMWREN);

    event &= GET_UINT32(p_DtsecMemMap->imask);

    WRITE_UINT32(p_DtsecMemMap->ievent, event);

    if(event & IMASK_BREN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_BAB_RX);
    if(event & IMASK_RXCEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_RX_CTL);
    if(event & IMASK_MSROEN)
        UpdateStatistics(p_Dtsec);
    if(event & IMASK_GTSCEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_GRATEFUL_TX_STP_COMPLET);
    if(event & IMASK_BTEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_BAB_TX);
    if(event & IMASK_TXCEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_TX_CTL);
    if(event & IMASK_TXEEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_TX_ERR);
    if(event & IMASK_LCEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_LATE_COL);
    if(event & IMASK_CRLEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_COL_RET_LMT);
    if(event & IMASK_XFUNEN)
    {
#ifdef FM_TX_LOCKUP_ERRATA_DTSEC6
        uint32_t  tpkt1, tmpReg1, tpkt2, tmpReg2, i;
        /* a. Write 0x00E0_0C00 to DTSEC_ID */
        /* This is a read only regidter */

        /* b. Read and save the value of TPKT */
        tpkt1 = GET_UINT32(p_DtsecMemMap->tpkt);

        /* c. Read the register at dTSEC address offset 0x32C */
        tmpReg1 =  GET_UINT32(*(uint32_t*)((uint8_t*)p_DtsecMemMap + 0x32c));

        /* d. Compare bits [9:15] to bits [25:31] of the register at address offset 0x32C. */
        if((tmpReg1 & 0x007F0000) != (tmpReg1 & 0x0000007F))
        {
            /* If they are not equal, save the value of this register and wait for at least
             * MAXFRM*16 ns */
            XX_UDelay((uint32_t)(NCSW_MIN(DtsecGetMaxFrameLength(p_Dtsec)*16/1000, 1)));
        }

        /* e. Read and save TPKT again and read the register at dTSEC address offset
            0x32C again*/
        tpkt2 = GET_UINT32(p_DtsecMemMap->tpkt);
        tmpReg2 = GET_UINT32(*(uint32_t*)((uint8_t*)p_DtsecMemMap + 0x32c));

        /* f. Compare the value of TPKT saved in step b to value read in step e. Also
            compare bits [9:15] of the register at offset 0x32C saved in step d to the value
            of bits [9:15] saved in step e. If the two registers values are unchanged, then
            the transmit portion of the dTSEC controller is locked up and the user should
            proceed to the recover sequence. */
        if((tpkt1 == tpkt2) && ((tmpReg1 & 0x007F0000) == (tmpReg2 & 0x007F0000)))
        {
            /* recover sequence */

            /* a.Write a 1 to RCTRL[GRS]*/

            WRITE_UINT32(p_DtsecMemMap->rctrl, GET_UINT32(p_DtsecMemMap->rctrl) | RCTRL_GRS);

            /* b.Wait until IEVENT[GRSC]=1, or at least 100 us has elapsed. */
            for(i = 0 ; i < 100 ; i++ )
            {
                if(GET_UINT32(p_DtsecMemMap->ievent) & IMASK_GRSCEN)
                    break;
                XX_UDelay(1);
            }
            if(GET_UINT32(p_DtsecMemMap->ievent) & IMASK_GRSCEN)
                WRITE_UINT32(p_DtsecMemMap->ievent, IMASK_GRSCEN);
            else
                DBG(INFO,("Rx lockup due to dTSEC Tx lockup"));


            /* c.Write a 1 to bit n of FM_RSTC (offset 0x0CC of FPM)*/
            FmResetMac(p_Dtsec->fmMacControllerDriver.h_Fm, e_FM_MAC_1G, p_Dtsec->fmMacControllerDriver.macId);

            /* d.Wait 4 Tx clocks (32 ns) */
            XX_UDelay(1);

            /* e.Write a 0 to bit n of FM_RSTC. */
            /* cleared by FMAN */
        }
        else
        {
            /* If either value has changed, the dTSEC controller is not locked up and the
               controller should be allowed to proceed normally by writing the reset value
               of 0x0824_0101 to DTSEC_ID. */
            /* Register is read only */
        }
#endif /* FM_TX_LOCKUP_ERRATA_DTSEC6 */

        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_TX_FIFO_UNDRN);
    }
    if(event & IMASK_MAGEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_MAG_PCKT);
    if(event & IMASK_GRSCEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_GRATEFUL_RX_STP_COMPLET);
    if(event & IMASK_TDPEEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_TX_DATA_ERR);
    if(event & IMASK_RDPEEN)
        p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_RX_DATA_ERR);

    /*  - masked interrupts */
    ASSERT_COND(!(event & IMASK_ABRTEN));
    ASSERT_COND(!(event & IMASK_IFERREN));
}

static void Dtsec1588Exception(t_Handle h_Dtsec)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    uint32_t            event;
    t_DtsecMemMap       *p_DtsecMemMap = p_Dtsec->p_MemMap;

    if (p_Dtsec->ptpTsuEnabled)
    {
        event = GET_UINT32(p_DtsecMemMap->tmr_pevent);
        event &= GET_UINT32(p_DtsecMemMap->tmr_pemask);
        if(event)
        {
            WRITE_UINT32(p_DtsecMemMap->tmr_pevent, event);
            ASSERT_COND(event & PEMASK_TSRE);
            p_Dtsec->f_Exception(p_Dtsec->h_App, e_FM_MAC_EX_1G_1588_TS_RX_ERR);
        }
    }
}

/* ........................................................................... */

static void FreeInitResources(t_Dtsec *p_Dtsec)
{
   /*TODO - need to ask why with mdioIrq != 0*/
    if ((p_Dtsec->mdioIrq != 0) && (p_Dtsec->mdioIrq != NO_IRQ))
    {
        XX_DisableIntr(p_Dtsec->mdioIrq);
        XX_FreeIntr(p_Dtsec->mdioIrq);
    }
    else if (p_Dtsec->mdioIrq == 0)
        FmUnregisterIntr(p_Dtsec->fmMacControllerDriver.h_Fm, e_FM_MOD_1G_MAC, p_Dtsec->macId, e_FM_INTR_TYPE_NORMAL);
    FmUnregisterIntr(p_Dtsec->fmMacControllerDriver.h_Fm, e_FM_MOD_1G_MAC, p_Dtsec->macId, e_FM_INTR_TYPE_ERR);
    FmUnregisterIntr(p_Dtsec->fmMacControllerDriver.h_Fm, e_FM_MOD_1G_MAC_TMR, p_Dtsec->macId, e_FM_INTR_TYPE_NORMAL);

    /* release the driver's group hash table */
    FreeHashTable(p_Dtsec->p_MulticastAddrHash);
    p_Dtsec->p_MulticastAddrHash =   NULL;

    /* release the driver's individual hash table */
    FreeHashTable(p_Dtsec->p_UnicastAddrHash);
    p_Dtsec->p_UnicastAddrHash =     NULL;
}

/* ........................................................................... */

static void HardwareClearAddrInPaddr(t_Dtsec *p_Dtsec, uint8_t paddrNum)
{
    WRITE_UINT32(((t_DtsecMemMap*)p_Dtsec->p_MemMap)->macaddr[paddrNum].exact_match1, 0x0);
    WRITE_UINT32(((t_DtsecMemMap*)p_Dtsec->p_MemMap)->macaddr[paddrNum].exact_match2, 0x0);
}

/* ........................................................................... */

static void HardwareAddAddrInPaddr(t_Dtsec *p_Dtsec, uint64_t *p_Addr, uint8_t paddrNum)
{
    uint32_t        tmpReg32        = 0;
    uint64_t        addr            = *p_Addr;
    t_DtsecMemMap   *p_DtsecMemMap  = (t_DtsecMemMap*)p_Dtsec->p_MemMap;

    tmpReg32 = (uint32_t)(addr);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_DtsecMemMap->macaddr[paddrNum].exact_match1, tmpReg32);

    tmpReg32 = (uint32_t)(addr>>32);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_DtsecMemMap->macaddr[paddrNum].exact_match2, tmpReg32);
}

/* ........................................................................... */

static t_Error GracefulStop(t_Dtsec *p_Dtsec, e_CommMode mode)
{
    t_DtsecMemMap   *p_MemMap;

    ASSERT_COND(p_Dtsec);

    p_MemMap= (t_DtsecMemMap*)(p_Dtsec->p_MemMap);
    ASSERT_COND(p_MemMap);

    /* Assert the graceful transmit stop bit */
    if (mode & e_COMM_MODE_RX)
        WRITE_UINT32(p_MemMap->rctrl,
                     GET_UINT32(p_MemMap->rctrl) | RCTRL_GRS);

#ifdef FM_GRS_ERRATA_DTSEC_A002
    XX_UDelay(100);
#endif /* FM_GRS_ERRATA_DTSEC_A002 */

#ifdef FM_GTS_ERRATA_DTSEC_A004
    DBG(INFO, ("GTS not supported due to DTSEC_A004 errata."));
#else  /* not FM_GTS_ERRATA_DTSEC_A004 */
    if (mode & e_COMM_MODE_TX)
        WRITE_UINT32(p_MemMap->tctrl,
                     GET_UINT32(p_MemMap->tctrl) | TCTRL_GTS);
#endif /* not FM_GTS_ERRATA_DTSEC_A004 */

    return E_OK;
}

/* .............................................................................. */

static t_Error GracefulRestart(t_Dtsec *p_Dtsec, e_CommMode mode)
{
    t_DtsecMemMap   *p_MemMap;

    ASSERT_COND(p_Dtsec);

    p_MemMap= (t_DtsecMemMap*)(p_Dtsec->p_MemMap);
    ASSERT_COND(p_MemMap);

    /* clear the graceful receive stop bit */
    if(mode & e_COMM_MODE_TX)
        WRITE_UINT32(p_MemMap->tctrl,
                      GET_UINT32(p_MemMap->tctrl) & ~TCTRL_GTS);

    if(mode & e_COMM_MODE_RX)
        WRITE_UINT32(p_MemMap->rctrl,
                      GET_UINT32(p_MemMap->rctrl) & ~RCTRL_GRS);

    return E_OK;
}


/*****************************************************************************/
/*                      dTSEC Configs modification functions                 */
/*****************************************************************************/


/* .............................................................................. */

static t_Error DtsecConfigLoopback(t_Handle h_Dtsec, bool newVal)
{

    t_Dtsec *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);

    p_Dtsec->p_DtsecDriverParam->loopback = newVal;

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecConfigMaxFrameLength(t_Handle h_Dtsec, uint16_t newVal)
{
    t_Dtsec *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);

    p_Dtsec->p_DtsecDriverParam->maxFrameLength = newVal;

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecConfigPadAndCrc(t_Handle h_Dtsec, bool newVal)
{
    t_Dtsec *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);

    p_Dtsec->p_DtsecDriverParam->padAndCrcEnable = newVal;

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecConfigHalfDuplex(t_Handle h_Dtsec, bool newVal)
{
    t_Dtsec *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);

    p_Dtsec->p_DtsecDriverParam->halfDuplex = newVal;

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecConfigLengthCheck(t_Handle h_Dtsec, bool newVal)
{
#ifdef FM_LEN_CHECK_ERRATA_FMAN_SW002
UNUSED(h_Dtsec);
    RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("LengthCheck!"));

#else
    t_Dtsec *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);

    p_Dtsec->p_DtsecDriverParam->lengthCheckEnable = newVal;

    return E_OK;
#endif /* FM_LEN_CHECK_ERRATA_FMAN_SW002 */
}

static t_Error DtsecConfigException(t_Handle h_Dtsec, e_FmMacExceptions exception, bool enable)
{
    t_Dtsec *p_Dtsec = (t_Dtsec *)h_Dtsec;
    uint32_t    bitMask = 0;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);

    if(exception != e_FM_MAC_EX_1G_1588_TS_RX_ERR)
    {
        GET_EXCEPTION_FLAG(bitMask, exception);
        if(bitMask)
        {
            if (enable)
                p_Dtsec->exceptions |= bitMask;
            else
                p_Dtsec->exceptions &= ~bitMask;
        }
        else
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));
    }
    else
    {
        if(!p_Dtsec->ptpTsuEnabled)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Exception valid for 1588 only"));
        switch(exception){
        case(e_FM_MAC_EX_1G_1588_TS_RX_ERR):
            if(enable)
                p_Dtsec->enTsuErrExeption = TRUE;
            else
                p_Dtsec->enTsuErrExeption = FALSE;
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));
        }
    }
    return E_OK;
}
/*****************************************************************************/
/*                      dTSEC Run Time API functions                         */
/*****************************************************************************/

/* .............................................................................. */

static t_Error DtsecEnable(t_Handle h_Dtsec,  e_CommMode mode)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap       *p_MemMap ;
    uint32_t            tmpReg32 = 0;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_INVALID_HANDLE);

    p_MemMap= (t_DtsecMemMap*)(p_Dtsec->p_MemMap);

    tmpReg32 = GET_UINT32(p_MemMap->maccfg1);
    if (mode & e_COMM_MODE_RX)
        tmpReg32 |= MACCFG1_RX_EN;
    if (mode & e_COMM_MODE_TX)
        tmpReg32 |= MACCFG1_TX_EN;
    WRITE_UINT32(p_MemMap->maccfg1, tmpReg32);

    GracefulRestart(p_Dtsec, mode);

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecDisable (t_Handle h_Dtsec, e_CommMode mode)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap       *p_MemMap ;
    uint32_t            tmpReg32 = 0;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_INVALID_HANDLE);

    p_MemMap = (t_DtsecMemMap*)(p_Dtsec->p_MemMap);

    GracefulStop(p_Dtsec, mode);

    tmpReg32 = GET_UINT32(p_MemMap->maccfg1);
    if (mode & e_COMM_MODE_RX)
        tmpReg32 &= ~MACCFG1_RX_EN;
    if (mode & e_COMM_MODE_TX)
        tmpReg32 &= ~MACCFG1_TX_EN;
    WRITE_UINT32(p_MemMap->maccfg1, tmpReg32);

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecTxMacPause(t_Handle h_Dtsec, uint16_t pauseTime)
{
    t_Dtsec         *p_Dtsec = (t_Dtsec *)h_Dtsec;
    uint32_t        ptv = 0;
    t_DtsecMemMap   *p_MemMap;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_INVALID_STATE);

    p_MemMap = (t_DtsecMemMap*)(p_Dtsec->p_MemMap);

    if (pauseTime)
    {
#ifdef FM_BAD_TX_TS_IN_B_2_B_ERRATA_DTSEC_A003
        {
            if (pauseTime <= 320)
                RETURN_ERROR(MINOR, E_INVALID_VALUE,
                             ("This pause-time value of %d is illegal due to errata dTSEC-A003!"
                              " value should be greater than 320."));
        }
#endif /* FM_BAD_TX_TS_IN_B_2_B_ERRATA_DTSEC_A003 */

#ifdef FM_SHORT_PAUSE_TIME_ERRATA_DTSEC1
        {
            t_FmRevisionInfo revInfo;
            FM_GetRevision(p_Dtsec->fmMacControllerDriver.h_Fm, &revInfo);
            if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
                pauseTime += 2;
        }
#endif /* FM_SHORT_PAUSE_TIME_ERRATA_DTSEC1 */

        ptv = GET_UINT32(p_MemMap->ptv);
        ptv |= pauseTime;
        WRITE_UINT32(p_MemMap->ptv, ptv);

        /* trigger the transmission of a flow-control pause frame */
        WRITE_UINT32(p_MemMap->maccfg1,
                     GET_UINT32(p_MemMap->maccfg1) | MACCFG1_TX_FLOW);
    }
    else
    {
        WRITE_UINT32(p_MemMap->maccfg1,
                     GET_UINT32(p_MemMap->maccfg1) & ~MACCFG1_TX_FLOW);
    }

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecRxIgnoreMacPause(t_Handle h_Dtsec, bool en)
{
    t_Dtsec         *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap   *p_MemMap;
    uint32_t        tmpReg32;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_INVALID_STATE);

    p_MemMap = (t_DtsecMemMap*)(p_Dtsec->p_MemMap);

    tmpReg32 = GET_UINT32(p_MemMap->maccfg1);
    if (en)
        tmpReg32 &= ~MACCFG1_RX_FLOW;
    else
        tmpReg32 |= MACCFG1_RX_FLOW;
    WRITE_UINT32(p_MemMap->maccfg1, tmpReg32);

    return E_OK;
}


/* .............................................................................. */

static t_Error DtsecEnable1588TimeStamp(t_Handle h_Dtsec)
{
    t_Dtsec          *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);
#ifdef FM_10_100_SGMII_NO_TS_ERRATA_DTSEC3
    if((p_Dtsec->enetMode == e_ENET_MODE_SGMII_10) || (p_Dtsec->enetMode == e_ENET_MODE_SGMII_100))
        RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("1588TimeStamp in 10/100 SGMII"));
#endif /* FM_10_100_SGMII_NO_TS_ERRATA_DTSEC3 */
    p_Dtsec->ptpTsuEnabled = TRUE;
    WRITE_UINT32(p_Dtsec->p_MemMap->rctrl, GET_UINT32(p_Dtsec->p_MemMap->rctrl) | RCTRL_RTSE);
    WRITE_UINT32(p_Dtsec->p_MemMap->tctrl, GET_UINT32(p_Dtsec->p_MemMap->tctrl) | TCTRL_TTSE);

    return E_OK;
}

static t_Error DtsecDisable1588TimeStamp(t_Handle h_Dtsec)
{
    t_Dtsec          *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);

    p_Dtsec->ptpTsuEnabled = FALSE;
    WRITE_UINT32(p_Dtsec->p_MemMap->rctrl, GET_UINT32(p_Dtsec->p_MemMap->rctrl) & ~RCTRL_RTSE);
    WRITE_UINT32(p_Dtsec->p_MemMap->tctrl, GET_UINT32(p_Dtsec->p_MemMap->tctrl) & ~TCTRL_TTSE);

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecGetStatistics(t_Handle h_Dtsec, t_FmMacStatistics *p_Statistics)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap       *p_DtsecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Statistics, E_NULL_POINTER);

    if (p_Dtsec->statisticsLevel == e_FM_MAC_NONE_STATISTICS)
        RETURN_ERROR(MINOR, E_INVALID_STATE, ("Statistics disabled"));

    p_DtsecMemMap = p_Dtsec->p_MemMap;
    memset(p_Statistics, 0xff, sizeof(t_FmMacStatistics));

    if (p_Dtsec->statisticsLevel == e_FM_MAC_FULL_STATISTICS)
    {
        p_Statistics->eStatPkts64           = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tr64))
                                                + p_Dtsec->internalStatistics.tr64;      /**< r-10G tr-DT 64 byte frame counter */
        p_Statistics->eStatPkts65to127      = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tr127))
                                                + p_Dtsec->internalStatistics.tr127;     /**< r-10G 65 to 127 byte frame counter */
        p_Statistics->eStatPkts128to255     = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tr255))
                                                + p_Dtsec->internalStatistics.tr255;     /**< r-10G 128 to 255 byte frame counter */
        p_Statistics->eStatPkts256to511     = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tr511))
                                                + p_Dtsec->internalStatistics.tr511;     /**< r-10G 256 to 511 byte frame counter */
        p_Statistics->eStatPkts512to1023    = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tr1k))
                                                + p_Dtsec->internalStatistics.tr1k;      /**< r-10G 512 to 1023 byte frame counter */
        p_Statistics->eStatPkts1024to1518   = (MASK22BIT & GET_UINT32(p_DtsecMemMap->trmax))
                                                + p_Dtsec->internalStatistics.trmax;     /**< r-10G 1024 to 1518 byte frame counter */
        p_Statistics->eStatPkts1519to1522   = (MASK22BIT & GET_UINT32(p_DtsecMemMap->trmgv))
                                                + p_Dtsec->internalStatistics.trmgv;     /**< r-10G 1519 to 1522 byte good frame count */
        /* MIB II */
        p_Statistics->ifInOctets            = GET_UINT32(p_DtsecMemMap->rbyt)
                                                + p_Dtsec->internalStatistics.rbyt;                  /**< Total number of byte received. */
        p_Statistics->ifInPkts              = (MASK22BIT & GET_UINT32(p_DtsecMemMap->rpkt))
                                                + p_Dtsec->internalStatistics.rpkt;    /**< Total number of packets received.*/
        p_Statistics->ifInMcastPkts         = (MASK22BIT & GET_UINT32(p_DtsecMemMap->rmca))
                                                + p_Dtsec->internalStatistics.rmca;    /**< Total number of multicast frame received*/
        p_Statistics->ifInBcastPkts         = (MASK22BIT & GET_UINT32(p_DtsecMemMap->rbca))
                                                + p_Dtsec->internalStatistics.rbca;    /**< Total number of broadcast frame received */
        p_Statistics->ifOutOctets           = GET_UINT32(p_DtsecMemMap->tbyt)
                                                + p_Dtsec->internalStatistics.tbyt;                  /**< Total number of byte sent. */
        p_Statistics->ifOutPkts             = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tpkt))
                                                + p_Dtsec->internalStatistics.tpkt;    /**< Total number of packets sent .*/
        p_Statistics->ifOutMcastPkts        = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tmca))
                                                + p_Dtsec->internalStatistics.tmca;    /**< Total number of multicast frame sent */
        p_Statistics->ifOutBcastPkts        = (MASK22BIT & GET_UINT32(p_DtsecMemMap->tbca))
                                                + p_Dtsec->internalStatistics.tbca;    /**< Total number of multicast frame sent */
    }
/* */
    p_Statistics->eStatFragments        = (MASK16BIT & GET_UINT32(p_DtsecMemMap->rfrg))
                                            + p_Dtsec->internalStatistics.rfrg;      /**< Total number of packets that were less than 64 octets long with a wrong CRC.*/
    p_Statistics->eStatJabbers          = (MASK16BIT & GET_UINT32(p_DtsecMemMap->rjbr))
                                            + p_Dtsec->internalStatistics.rjbr;      /**< Total number of packets longer than valid maximum length octets */

    p_Statistics->eStatsDropEvents      = (MASK16BIT & GET_UINT32(p_DtsecMemMap->rdrp))
                                            + p_Dtsec->internalStatistics.rdrp;      /**< number of dropped packets due to internal errors of the MAC Client. */
    p_Statistics->eStatCRCAlignErrors   = (MASK16BIT & GET_UINT32(p_DtsecMemMap->raln))
                                            + p_Dtsec->internalStatistics.raln;      /**< Incremented when frames of correct length but with CRC error are received.*/

    p_Statistics->eStatUndersizePkts    = (MASK16BIT & GET_UINT32(p_DtsecMemMap->rund))
                                            + p_Dtsec->internalStatistics.rund;      /**< Total number of packets that were less than 64 octets long with a good CRC.*/
    p_Statistics->eStatOversizePkts     = (MASK16BIT & GET_UINT32(p_DtsecMemMap->rovr))
                                            + p_Dtsec->internalStatistics.rovr;      /**< T,B.D*/
/* Pause */
    p_Statistics->reStatPause           = (MASK16BIT & GET_UINT32(p_DtsecMemMap->rxpf))
                                            + p_Dtsec->internalStatistics.rxpf;      /**< Pause MAC Control received */
    p_Statistics->teStatPause           = (MASK16BIT & GET_UINT32(p_DtsecMemMap->txpf))
                                            + p_Dtsec->internalStatistics.txpf;      /**< Pause MAC Control sent */

    p_Statistics->ifInDiscards          = p_Statistics->eStatsDropEvents;                    /**< Frames received, but discarded due to problems within the MAC RX. */

    p_Statistics->ifInErrors            = p_Statistics->eStatsDropEvents
                                        + p_Statistics->eStatCRCAlignErrors
                                        + (MASK16BIT & GET_UINT32(p_DtsecMemMap->rflr))
                                        + p_Dtsec->internalStatistics.rflr
                                        + (MASK16BIT & GET_UINT32(p_DtsecMemMap->rcde))
                                        + p_Dtsec->internalStatistics.rcde
                                        + (MASK16BIT & GET_UINT32(p_DtsecMemMap->rcse))
                                        + p_Dtsec->internalStatistics.rcse;

    p_Statistics->ifOutDiscards         = (MASK16BIT & GET_UINT32(p_DtsecMemMap->tdrp))
                                            + p_Dtsec->internalStatistics.tdrp;     /**< Frames received, but discarded due to problems within the MAC TX N/A!.*/
    p_Statistics->ifOutErrors           = p_Statistics->ifOutDiscards                                           /**< Number of frames transmitted with error: */
                                        + (MASK12BIT & GET_UINT32(p_DtsecMemMap->tfcs))
                                        + p_Dtsec->internalStatistics.tfcs;

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecModifyMacAddress (t_Handle h_Dtsec, t_EnetAddr *p_EnetAddr)
{
    t_Dtsec              *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap        *p_DtsecMemMap;
    uint32_t              tmpReg32 = 0;
    uint64_t              addr;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);

    p_DtsecMemMap = p_Dtsec->p_MemMap;
    /* Initialize MAC Station Address registers (1 & 2)    */
    /* Station address have to be swapped (big endian to little endian */
    addr = ((*(uint64_t *)p_EnetAddr) >> 16);
    p_Dtsec->addr = addr;

    tmpReg32 = (uint32_t)(addr);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_DtsecMemMap->macstnaddr1, tmpReg32);

    tmpReg32 = (uint32_t)(addr>>32);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_DtsecMemMap->macstnaddr2, tmpReg32);

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecResetCounters (t_Handle h_Dtsec)
{
    t_Dtsec          *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);

    /* clear HW counters */
    WRITE_UINT32(p_Dtsec->p_MemMap->ecntrl, GET_UINT32(p_Dtsec->p_MemMap->ecntrl) | ECNTRL_CLRCNT);

    /* clear SW counters holding carries */
    memset((char *)&p_Dtsec->internalStatistics, (char)0x0, sizeof(t_InternalStatistics));

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecAddExactMatchMacAddress(t_Handle h_Dtsec, t_EnetAddr *p_EthAddr)
{
    t_Dtsec   *p_Dtsec = (t_Dtsec *) h_Dtsec;
    uint64_t  ethAddr;
    uint8_t   paddrNum;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);

    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    if (ethAddr & GROUP_ADDRESS)
        /* Multicast address has no effect in PADDR */
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Multicast address"));

    /* Make sure no PADDR contains this address */
    for (paddrNum = 0; paddrNum < DTSEC_NUM_OF_PADDRS; paddrNum++)
        if (p_Dtsec->indAddrRegUsed[paddrNum])
            if (p_Dtsec->paddr[paddrNum] == ethAddr)
                RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, NO_MSG);

    /* Find first unused PADDR */
    for (paddrNum = 0; paddrNum < DTSEC_NUM_OF_PADDRS; paddrNum++)
        if (!(p_Dtsec->indAddrRegUsed[paddrNum]))
        {
            /* mark this PADDR as used */
            p_Dtsec->indAddrRegUsed[paddrNum] = TRUE;
            /* store address */
            p_Dtsec->paddr[paddrNum] = ethAddr;

            /* put in hardware */
            HardwareAddAddrInPaddr(p_Dtsec, &ethAddr, paddrNum);
            p_Dtsec->numOfIndAddrInRegs++;

            return E_OK;
        }

    /* No free PADDR */
    RETURN_ERROR(MAJOR, E_FULL, NO_MSG);
}

/* .............................................................................. */

static t_Error DtsecDelExactMatchMacAddress(t_Handle h_Dtsec, t_EnetAddr *p_EthAddr)
{
    t_Dtsec   *p_Dtsec = (t_Dtsec *) h_Dtsec;
    uint64_t  ethAddr;
    uint8_t   paddrNum;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);

    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    /* Find used PADDR containing this address */
    for (paddrNum = 0; paddrNum < DTSEC_NUM_OF_PADDRS; paddrNum++)
    {
        if ((p_Dtsec->indAddrRegUsed[paddrNum]) &&
            (p_Dtsec->paddr[paddrNum] == ethAddr))
        {
            /* mark this PADDR as not used */
            p_Dtsec->indAddrRegUsed[paddrNum] = FALSE;
            /* clear in hardware */
            HardwareClearAddrInPaddr(p_Dtsec, paddrNum);
            p_Dtsec->numOfIndAddrInRegs--;

            return E_OK;
        }
    }

    RETURN_ERROR(MAJOR, E_NOT_FOUND, NO_MSG);
}

/* .............................................................................. */

static t_Error DtsecAddHashMacAddress(t_Handle h_Dtsec, t_EnetAddr *p_EthAddr)
{
    t_Dtsec         *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap   *p_DtsecMemMap;
    uint32_t        crc;
    uint8_t         crcMirror, reg;
    uint32_t        bitMask;
    t_EthHashEntry  *p_HashEntry;
    uint64_t        ethAddr;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);

    p_DtsecMemMap = p_Dtsec->p_MemMap;

    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    /* CRC calculation */
    GET_MAC_ADDR_CRC(ethAddr, crc);

    /* calculate the "crc mirror" */
    crcMirror = MIRROR((uint8_t)crc);

    /* 3 MSB bits define the register */
    reg = (uint8_t)(crcMirror >> 5);
    /* 5 LSB bits define the bit within the register */
    bitMask =  0x80000000 >> (crcMirror & 0x1f);

    /* Create element to be added to the driver hash table */
    p_HashEntry = (t_EthHashEntry *)XX_Malloc(sizeof(t_EthHashEntry));
    p_HashEntry->addr = ethAddr;
    INIT_LIST(&p_HashEntry->node);

    if (ethAddr & GROUP_ADDRESS)
    {
        /* Group Address */
        LIST_AddToTail(&(p_HashEntry->node), &(p_Dtsec->p_MulticastAddrHash->p_Lsts[crcMirror]));
        /* Set the appropriate bit in GADDR0-7 */
        WRITE_UINT32(p_DtsecMemMap->gaddr[reg],
                     GET_UINT32(p_DtsecMemMap->gaddr[reg]) | bitMask);
    }
    else
    {
        LIST_AddToTail(&(p_HashEntry->node), &(p_Dtsec->p_UnicastAddrHash->p_Lsts[crcMirror]));
        /* Set the appropriate bit in IADDR0-7 */
        WRITE_UINT32(p_DtsecMemMap->igaddr[reg],
                     GET_UINT32(p_DtsecMemMap->igaddr[reg]) | bitMask);
    }

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecDelHashMacAddress(t_Handle h_Dtsec, t_EnetAddr *p_EthAddr)
{
    t_Dtsec         *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap   *p_DtsecMemMap;
    t_List          *p_Pos;
    uint32_t        crc;
    uint8_t         crcMirror, reg;
    uint32_t        bitMask;
    t_EthHashEntry  *p_HashEntry = NULL;
    uint64_t        ethAddr;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);

    p_DtsecMemMap = p_Dtsec->p_MemMap;

    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    /* CRC calculation */
    GET_MAC_ADDR_CRC(ethAddr, crc);

    /* calculate the "crc mirror" */
    crcMirror = MIRROR((uint8_t)crc);

    /* 3 MSB bits define the register */
    reg =(uint8_t)( crcMirror >> 5);
    /* 5 LSB bits define the bit within the register */
    bitMask =  0x80000000 >> (crcMirror & 0x1f);

    if (ethAddr & GROUP_ADDRESS)
    {
        /* Group Address */
        LIST_FOR_EACH(p_Pos, &(p_Dtsec->p_MulticastAddrHash->p_Lsts[crcMirror]))
        {
            p_HashEntry = ETH_HASH_ENTRY_OBJ(p_Pos);
            if(p_HashEntry->addr == ethAddr)
            {
                LIST_DelAndInit(&p_HashEntry->node);
                XX_Free(p_HashEntry);
                break;
            }
        }
        if(LIST_IsEmpty(&p_Dtsec->p_MulticastAddrHash->p_Lsts[crcMirror]))
            WRITE_UINT32(p_DtsecMemMap->gaddr[reg],
                         GET_UINT32(p_DtsecMemMap->gaddr[reg]) & ~bitMask);
    }
    else
    {
        /* Individual Address */
        LIST_FOR_EACH(p_Pos, &(p_Dtsec->p_UnicastAddrHash->p_Lsts[crcMirror]))
        {
            p_HashEntry = ETH_HASH_ENTRY_OBJ(p_Pos);
            if(p_HashEntry->addr == ethAddr)
            {
                LIST_DelAndInit(&p_HashEntry->node);
                XX_Free(p_HashEntry);
                break;
            }
        }
        if(LIST_IsEmpty(&p_Dtsec->p_UnicastAddrHash->p_Lsts[crcMirror]))
            WRITE_UINT32(p_DtsecMemMap->igaddr[reg],
                         GET_UINT32(p_DtsecMemMap->igaddr[reg]) & ~bitMask);
    }

    /* address does not exist */
    ASSERT_COND(p_HashEntry != NULL);

    return E_OK;
}


/* .............................................................................. */

static t_Error DtsecSetPromiscuous(t_Handle h_Dtsec, bool newVal)
{
    t_Dtsec         *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap   *p_DtsecMemMap;
    uint32_t        tmpReg32;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);

    p_DtsecMemMap = p_Dtsec->p_MemMap;

    tmpReg32 = GET_UINT32(p_DtsecMemMap->rctrl);

    if (newVal)
        tmpReg32 |= RCTRL_PROM;
    else
        tmpReg32 &= ~RCTRL_PROM;

    WRITE_UINT32(p_DtsecMemMap->rctrl, tmpReg32);

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecSetStatistics(t_Handle h_Dtsec, e_FmMacStatisticsLevel statisticsLevel)
{
    t_Dtsec         *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap   *p_DtsecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);

    p_DtsecMemMap = p_Dtsec->p_MemMap;

    p_Dtsec->statisticsLevel = statisticsLevel;

    switch (p_Dtsec->statisticsLevel)
    {
        case(e_FM_MAC_NONE_STATISTICS):
            WRITE_UINT32(p_DtsecMemMap->cam1,0xffffffff);
            WRITE_UINT32(p_DtsecMemMap->cam2,0xffffffff);
            WRITE_UINT32(p_DtsecMemMap->ecntrl, GET_UINT32(p_DtsecMemMap->ecntrl) & ~ECNTRL_STEN);
            WRITE_UINT32(p_DtsecMemMap->imask, GET_UINT32(p_DtsecMemMap->imask) & ~IMASK_MSROEN);
            p_Dtsec->exceptions &= ~IMASK_MSROEN;
            break;
        case(e_FM_MAC_PARTIAL_STATISTICS):
            WRITE_UINT32(p_DtsecMemMap->cam1, CAM1_ERRORS_ONLY);
            WRITE_UINT32(p_DtsecMemMap->cam2, CAM2_ERRORS_ONLY);
            WRITE_UINT32(p_DtsecMemMap->ecntrl, GET_UINT32(p_DtsecMemMap->ecntrl) | ECNTRL_STEN);
            WRITE_UINT32(p_DtsecMemMap->imask, GET_UINT32(p_DtsecMemMap->imask) | IMASK_MSROEN);
            p_Dtsec->exceptions |= IMASK_MSROEN;
            break;
        case(e_FM_MAC_FULL_STATISTICS):
            WRITE_UINT32(p_DtsecMemMap->cam1,0);
            WRITE_UINT32(p_DtsecMemMap->cam2,0);
            WRITE_UINT32(p_DtsecMemMap->ecntrl, GET_UINT32(p_DtsecMemMap->ecntrl) | ECNTRL_STEN);
            WRITE_UINT32(p_DtsecMemMap->imask, GET_UINT32(p_DtsecMemMap->imask) | IMASK_MSROEN);
            p_Dtsec->exceptions |= IMASK_MSROEN;
            break;
        default:
            RETURN_ERROR(MINOR, E_INVALID_SELECTION, NO_MSG);
    }

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecAdjustLink(t_Handle h_Dtsec, e_EnetSpeed speed, bool fullDuplex)
{
    t_Dtsec         *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap   *p_DtsecMemMap;
    uint32_t        tmpReg32;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_HANDLE);
    p_DtsecMemMap = p_Dtsec->p_MemMap;
    SANITY_CHECK_RETURN_ERROR(p_DtsecMemMap, E_INVALID_HANDLE);

    if ((!fullDuplex) && (speed >= e_ENET_SPEED_1000))
        RETURN_ERROR(MAJOR, E_CONFLICT, ("Ethernet interface does not support Half Duplex mode"));

    p_Dtsec->enetMode = MAKE_ENET_MODE(ENET_INTERFACE_FROM_MODE(p_Dtsec->enetMode), speed);
    p_Dtsec->halfDuplex = !fullDuplex;

    tmpReg32 = GET_UINT32(p_DtsecMemMap->maccfg2);
    if(p_Dtsec->halfDuplex)
        tmpReg32 &= ~MACCFG2_FULL_DUPLEX;
    else
        tmpReg32 |= MACCFG2_FULL_DUPLEX;

    tmpReg32 &= ~(MACCFG2_NIBBLE_MODE | MACCFG2_BYTE_MODE);
    if((p_Dtsec->enetMode == e_ENET_MODE_RGMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_100)||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_100))
            tmpReg32 |= MACCFG2_NIBBLE_MODE;
    else if((p_Dtsec->enetMode == e_ENET_MODE_RGMII_1000) ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_1000)||
        (p_Dtsec->enetMode == e_ENET_MODE_GMII_1000))
            tmpReg32 |= MACCFG2_BYTE_MODE;
    WRITE_UINT32(p_DtsecMemMap->maccfg2, tmpReg32);

    tmpReg32 = GET_UINT32(p_DtsecMemMap->ecntrl);
    if (!(tmpReg32 & ECNTRL_CFG_RO))
    {
        if ((p_Dtsec->enetMode == e_ENET_MODE_RGMII_100) ||
            (p_Dtsec->enetMode == e_ENET_MODE_SGMII_100))
            tmpReg32 |= ECNTRL_R100M;
        else
            tmpReg32 &= ~ECNTRL_R100M;
        WRITE_UINT32(p_DtsecMemMap->ecntrl, tmpReg32);
    }

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecGetId(t_Handle h_Dtsec, uint32_t *macId)
{
    t_Dtsec              *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_HANDLE);

    *macId = p_Dtsec->macId;

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecGetVersion(t_Handle h_Dtsec, uint32_t *macVersion)
{
    t_Dtsec              *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecMemMap        *p_DtsecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);

    p_DtsecMemMap = p_Dtsec->p_MemMap;
    *macVersion = GET_UINT32(p_DtsecMemMap->tsec_id1);

    return E_OK;
}

/* .............................................................................. */

static t_Error DtsecSetException(t_Handle h_Dtsec, e_FmMacExceptions exception, bool enable)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    uint32_t            tmpReg, bitMask = 0;
    t_DtsecMemMap       *p_DtsecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Dtsec->p_DtsecDriverParam, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_NULL_POINTER);

    p_DtsecMemMap = p_Dtsec->p_MemMap;

    if(exception != e_FM_MAC_EX_1G_1588_TS_RX_ERR)
    {
        GET_EXCEPTION_FLAG(bitMask, exception);
        if(bitMask)
        {
            if (enable)
                p_Dtsec->exceptions |= bitMask;
            else
                p_Dtsec->exceptions &= ~bitMask;
       }
        else
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));

        tmpReg = GET_UINT32(p_DtsecMemMap->imask);
        if(enable)
            tmpReg |= bitMask;
        else
            tmpReg &= ~bitMask;
        WRITE_UINT32(p_DtsecMemMap->imask, tmpReg);

        /* warn if MIB OVFL is disabled and statistic gathering is enabled */
        if((exception == e_FM_MAC_EX_1G_RX_MIB_CNT_OVFL) &&
                !enable &&
                (p_Dtsec->statisticsLevel != e_FM_MAC_NONE_STATISTICS))
            DBG(WARNING, ("Disabled MIB counters overflow exceptions. Counters value may be inaccurate due to unregistered overflow"));

    }
    else
    {
        if(!p_Dtsec->ptpTsuEnabled)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Exception valid for 1588 only"));
        tmpReg = GET_UINT32(p_DtsecMemMap->tmr_pemask);
        switch(exception){
        case(e_FM_MAC_EX_1G_1588_TS_RX_ERR):
            if(enable)
            {
                p_Dtsec->enTsuErrExeption = TRUE;
                WRITE_UINT32(p_DtsecMemMap->tmr_pemask, tmpReg | PEMASK_TSRE);
            }
            else
            {
                p_Dtsec->enTsuErrExeption = FALSE;
                WRITE_UINT32(p_DtsecMemMap->tmr_pemask, tmpReg & ~PEMASK_TSRE);
            }
            break;
        default:
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));
        }
    }

    return E_OK;
}

/* ........................................................................... */

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
static t_Error DtsecDumpRegs(t_Handle h_Dtsec)
{
    t_Dtsec *p_Dtsec = (t_Dtsec *)h_Dtsec;
    int i = 0;

    DECLARE_DUMP;

    if (p_Dtsec->p_MemMap)
    {

        DUMP_TITLE(p_Dtsec->p_MemMap, ("MAC %d: ", p_Dtsec->macId));
        DUMP_VAR(p_Dtsec->p_MemMap, tsec_id1);
        DUMP_VAR(p_Dtsec->p_MemMap, tsec_id2);
        DUMP_VAR(p_Dtsec->p_MemMap, ievent);
        DUMP_VAR(p_Dtsec->p_MemMap, imask);
        DUMP_VAR(p_Dtsec->p_MemMap, edis);
        DUMP_VAR(p_Dtsec->p_MemMap, ecntrl);
        DUMP_VAR(p_Dtsec->p_MemMap, ptv);
        DUMP_VAR(p_Dtsec->p_MemMap, tmr_ctrl);
        DUMP_VAR(p_Dtsec->p_MemMap, tmr_pevent);
        DUMP_VAR(p_Dtsec->p_MemMap, tmr_pemask);
        DUMP_VAR(p_Dtsec->p_MemMap, tctrl);
        DUMP_VAR(p_Dtsec->p_MemMap, rctrl);
        DUMP_VAR(p_Dtsec->p_MemMap, maccfg1);
        DUMP_VAR(p_Dtsec->p_MemMap, maccfg2);
        DUMP_VAR(p_Dtsec->p_MemMap, ipgifg);
        DUMP_VAR(p_Dtsec->p_MemMap, hafdup);
        DUMP_VAR(p_Dtsec->p_MemMap, maxfrm);

        DUMP_VAR(p_Dtsec->p_MemMap, macstnaddr1);
        DUMP_VAR(p_Dtsec->p_MemMap, macstnaddr2);

        DUMP_SUBSTRUCT_ARRAY(i, 8)
        {
            DUMP_VAR(p_Dtsec->p_MemMap, macaddr[i].exact_match1);
            DUMP_VAR(p_Dtsec->p_MemMap, macaddr[i].exact_match2);
        }
    }

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && ... */


/*****************************************************************************/
/*                      FM Init & Free API                                   */
/*****************************************************************************/

/* .............................................................................. */

static t_Error DtsecInit(t_Handle h_Dtsec)
{
    t_Dtsec             *p_Dtsec = (t_Dtsec *)h_Dtsec;
    t_DtsecDriverParam  *p_DtsecDriverParam;
    t_DtsecMemMap       *p_DtsecMemMap;
    int                 i;
    uint32_t            tmpReg32;
    uint64_t            addr;
    t_Error             err;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_DtsecDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_Dtsec->p_MemMap, E_INVALID_STATE);

    CHECK_INIT_PARAMETERS(p_Dtsec, CheckInitParameters);

    p_DtsecDriverParam  = p_Dtsec->p_DtsecDriverParam;
    p_Dtsec->halfDuplex = p_DtsecDriverParam->halfDuplex;
    p_Dtsec->debugMode  = p_DtsecDriverParam->debugMode;
    p_DtsecMemMap       = p_Dtsec->p_MemMap;

    /*************dtsec_id2******************/
    tmpReg32 =  GET_UINT32(p_DtsecMemMap->tsec_id2);

    if ((p_Dtsec->enetMode == e_ENET_MODE_RGMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_100) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_1000) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RMII_100))
        if(tmpReg32 & ID2_INT_REDUCED_OFF)
        {
             RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("no support for reduced interface in current DTSEC version"));
        }

    if ((p_Dtsec->enetMode == e_ENET_MODE_SGMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_100) ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_1000)||
        (p_Dtsec->enetMode == e_ENET_MODE_MII_10)    ||
        (p_Dtsec->enetMode == e_ENET_MODE_MII_100))
        if(tmpReg32 & ID2_INT_NORMAL_OFF)
        {
             RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("no support for normal interface in current DTSEC version"));
        }
    /*************dtsec_id2******************/

    /***************EDIS************************/
    WRITE_UINT32(p_DtsecMemMap->edis, p_DtsecDriverParam->errorDisabled);
    /***************EDIS************************/

    /***************ECNTRL************************/
    tmpReg32 = 0;
    if ((p_Dtsec->enetMode == e_ENET_MODE_RGMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_100) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_1000) ||
        (p_Dtsec->enetMode == e_ENET_MODE_GMII_1000))
        tmpReg32 |= ECNTRL_GMIIM;
    if ((p_Dtsec->enetMode == e_ENET_MODE_SGMII_10)   ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_100)  ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_1000))
        tmpReg32 |= (ECNTRL_SGMIIM | ECNTRL_TBIM);
    if (p_Dtsec->enetMode == e_ENET_MODE_QSGMII_1000)
        tmpReg32 |= (ECNTRL_SGMIIM | ECNTRL_TBIM | ECNTRL_QSGMIIM);
    if ((p_Dtsec->enetMode == e_ENET_MODE_RGMII_1000) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_10)||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_100))
        tmpReg32 |= ECNTRL_RPM;
    if ((p_Dtsec->enetMode == e_ENET_MODE_RGMII_100) ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_100) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RMII_100))
        tmpReg32 |= ECNTRL_R100M;
    if ((p_Dtsec->enetMode == e_ENET_MODE_RMII_10) || (p_Dtsec->enetMode == e_ENET_MODE_RMII_100))
        tmpReg32 |= ECNTRL_RMM;
    WRITE_UINT32(p_DtsecMemMap->ecntrl, tmpReg32);
    /***************ECNTRL************************/

    /***************PTV************************/
    tmpReg32 = 0;
#ifdef FM_SHORT_PAUSE_TIME_ERRATA_DTSEC1
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Dtsec->fmMacControllerDriver.h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            p_DtsecDriverParam->pauseTime += 2;
    }
#endif /* FM_SHORT_PAUSE_TIME_ERRATA_DTSEC1 */
    if (p_DtsecDriverParam->pauseTime)
        tmpReg32 |= (uint32_t)p_DtsecDriverParam->pauseTime;

    if (p_DtsecDriverParam->pauseExtended)
        tmpReg32 |= ((uint32_t)p_DtsecDriverParam->pauseExtended) << PTV_PTE_OFST;
    WRITE_UINT32(p_DtsecMemMap->ptv, tmpReg32);
    /***************PTV************************/

    /***************TCTRL************************/
    tmpReg32 = 0;
    if(p_DtsecDriverParam->halfDuplex)
    {
        if(p_DtsecDriverParam->halfDulexFlowControlEn)
            tmpReg32 |= TCTRL_THDF;
    }
    else
    {
        if(p_DtsecDriverParam->txTimeStampEn)
            tmpReg32 |= TCTRL_TTSE;
    }
    WRITE_UINT32(p_DtsecMemMap->tctrl, tmpReg32);
    /***************TCTRL************************/

    /***************RCTRL************************/
    tmpReg32 = 0;
    if (p_DtsecDriverParam->packetAlignmentPadding)
        tmpReg32 |= ((uint32_t)(0x0000001f & p_DtsecDriverParam->packetAlignmentPadding)) << 16;
    if (p_DtsecDriverParam->controlFrameAccept)
        tmpReg32 |= RCTRL_CFA;
    if (p_DtsecDriverParam->groupHashExtend)
        tmpReg32 |= RCTRL_GHTX;
    if(p_DtsecDriverParam->rxTimeStampEn)
        tmpReg32 |= RCTRL_RTSE;
    if (p_DtsecDriverParam->broadcReject)
        tmpReg32 |= RCTRL_BC_REJ;
    if (p_DtsecDriverParam->rxShortFrame)
        tmpReg32 |= RCTRL_RSF;
    if (p_DtsecDriverParam->promiscuousEnable)
        tmpReg32 |= RCTRL_PROM;
    if (p_DtsecDriverParam->exactMatch)
        tmpReg32 |= RCTRL_EMEN;

    WRITE_UINT32(p_DtsecMemMap->rctrl, tmpReg32);
    /***************RCTRL************************/

    /* Assign a Phy Address to the TBI (TBIPA).            */
    /* Done also in case that TBI is not selected to avoid */
    /* conflict with the external PHY’s Physical address   */
    WRITE_UINT32(p_DtsecMemMap->tbipa, p_DtsecDriverParam->tbiPhyAddr);

    /* Reset the management interface */
    WRITE_UINT32(p_Dtsec->p_MiiMemMap->miimcfg, MIIMCFG_RESET_MGMT);
    WRITE_UINT32(p_Dtsec->p_MiiMemMap->miimcfg, ~MIIMCFG_RESET_MGMT);
    /* Setup the MII Mgmt clock speed */
    WRITE_UINT32(p_Dtsec->p_MiiMemMap->miimcfg,
                 (uint32_t)GetMiiDiv((int32_t)(((p_Dtsec->fmMacControllerDriver.clkFreq*10)/2)/8)));

    if(p_Dtsec->enetMode == e_ENET_MODE_SGMII_1000)
    {
        uint16_t            tmpReg16;

        /* Configure the TBI PHY Control Register */
        tmpReg16 = PHY_TBICON_SPEED2 | PHY_TBICON_SRESET;

        DTSEC_MII_WritePhyReg(p_Dtsec, p_DtsecDriverParam->tbiPhyAddr, 17, tmpReg16);

        tmpReg16 = PHY_TBICON_SPEED2;

        DTSEC_MII_WritePhyReg(p_Dtsec, p_DtsecDriverParam->tbiPhyAddr, 17, tmpReg16);

        if(!p_DtsecDriverParam->halfDuplex)
            tmpReg16 |= PHY_CR_FULLDUPLEX | 0x8000 | PHY_CR_ANE;

        DTSEC_MII_WritePhyReg(p_Dtsec, p_DtsecDriverParam->tbiPhyAddr, 0, tmpReg16);

        tmpReg16 = 0x01a0;
        DTSEC_MII_WritePhyReg(p_Dtsec, p_DtsecDriverParam->tbiPhyAddr, 4, tmpReg16);

        tmpReg16 = 0x1340;
        DTSEC_MII_WritePhyReg(p_Dtsec, p_DtsecDriverParam->tbiPhyAddr, 0, tmpReg16);
    }

    /***************TMR_CTL************************/
    WRITE_UINT32(p_DtsecMemMap->tmr_ctrl, 0);

    if(p_Dtsec->ptpTsuEnabled)
    {
        tmpReg32 = 0;
        if (p_Dtsec->enTsuErrExeption)
            tmpReg32 |= PEMASK_TSRE;
        WRITE_UINT32(p_DtsecMemMap->tmr_pemask, tmpReg32);
        WRITE_UINT32(p_DtsecMemMap->tmr_pevent, tmpReg32);
    }

    /***************DEBUG************************/
    tmpReg32 = 0;
    if(p_DtsecDriverParam->debugMode)
        WRITE_UINT32(p_DtsecMemMap->tsec_id1, TSEC_ID1_DEBUG);
    /***************DEBUG************************/

    /***************MACCFG1***********************/
    WRITE_UINT32(p_DtsecMemMap->maccfg1, MACCFG1_SOFT_RESET);
    WRITE_UINT32(p_DtsecMemMap->maccfg1, 0);
    tmpReg32 = 0;
    if(p_DtsecDriverParam->loopback)
        tmpReg32 |= MACCFG1_LOOPBACK;
    if(p_DtsecDriverParam->actOnRxPauseFrame)
        tmpReg32 |= MACCFG1_RX_FLOW;
    if(p_DtsecDriverParam->actOnTxPauseFrame)
        tmpReg32 |= MACCFG1_TX_FLOW;
    WRITE_UINT32(p_DtsecMemMap->maccfg1, tmpReg32);
    /***************MACCFG1***********************/

    /***************MACCFG2***********************/
    tmpReg32 = 0;
    if( (p_Dtsec->enetMode == e_ENET_MODE_RMII_10)  ||
        (p_Dtsec->enetMode == e_ENET_MODE_RMII_100) ||
        (p_Dtsec->enetMode == e_ENET_MODE_MII_10)   ||
        (p_Dtsec->enetMode == e_ENET_MODE_MII_100)  ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_RGMII_100)||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_10) ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_100))
            tmpReg32 |= MACCFG2_NIBBLE_MODE;
    else if((p_Dtsec->enetMode == e_ENET_MODE_RGMII_1000) ||
        (p_Dtsec->enetMode == e_ENET_MODE_SGMII_1000)||
        (p_Dtsec->enetMode == e_ENET_MODE_GMII_1000)||
        (p_Dtsec->enetMode == e_ENET_MODE_QSGMII_1000))
            tmpReg32 |= MACCFG2_BYTE_MODE;

    tmpReg32 |= (((uint32_t)p_DtsecDriverParam->preambleLength) & 0x0000000f)<< PREAMBLE_LENGTH_SHIFT;

    if(p_DtsecDriverParam->preambleRxEn)
        tmpReg32 |= MACCFG2_PRE_AM_Rx_EN;
    if(p_DtsecDriverParam->preambleTxEn)
        tmpReg32 |= MACCFG2_PRE_AM_Tx_EN;
    if(p_DtsecDriverParam->lengthCheckEnable)
        tmpReg32 |= MACCFG2_LENGTH_CHECK;
    if(p_DtsecDriverParam->padAndCrcEnable)
        tmpReg32 |=  MACCFG2_PAD_CRC_EN;
    if(p_DtsecDriverParam->crcEnable)
        tmpReg32 |= MACCFG2_CRC_EN;
    if(!p_DtsecDriverParam->halfDuplex)
        tmpReg32 |= MACCFG2_FULL_DUPLEX;
    WRITE_UINT32(p_DtsecMemMap->maccfg2, tmpReg32);
    /***************MACCFG2***********************/

    /***************IPGIFG************************/
    tmpReg32 = 0;
    ASSERT_COND(p_DtsecDriverParam->nonBackToBackIpg1 <= p_DtsecDriverParam->nonBackToBackIpg2);
    tmpReg32 = (uint32_t)((((uint32_t)p_DtsecDriverParam->nonBackToBackIpg1 <<
               IPGIFG_NON_BACK_TO_BACK_IPG_1_SHIFT) & IPGIFG_NON_BACK_TO_BACK_IPG_1) |
              (((uint32_t)p_DtsecDriverParam->nonBackToBackIpg2  <<
                IPGIFG_NON_BACK_TO_BACK_IPG_2_SHIFT) & IPGIFG_NON_BACK_TO_BACK_IPG_2) |
              (((uint32_t)p_DtsecDriverParam->minIfgEnforcement <<
                IPGIFG_MIN_IFG_ENFORCEMENT_SHIFT) & IPGIFG_MIN_IFG_ENFORCEMENT) |
              ((uint32_t)p_DtsecDriverParam->backToBackIpg & IPGIFG_BACK_TO_BACK_IPG));
    WRITE_UINT32(p_DtsecMemMap->ipgifg, tmpReg32);
    /***************IPGIFG************************/

    /***************HAFDUP************************/
    tmpReg32 = 0;
    if(p_DtsecDriverParam->alternateBackoffEnable)
    {
        tmpReg32 = (uint32_t) (HAFDUP_ALT_BEB  | (((uint32_t)p_DtsecDriverParam->alternateBackoffVal & 0x0000000f) <<
                                    HAFDUP_ALTERNATE_BEB_TRUNCATION_SHIFT));
    }

    if(p_DtsecDriverParam->backPressureNoBackoff)
        tmpReg32 |= HAFDUP_BP_NO_BACKOFF;
    if(p_DtsecDriverParam->noBackoff)
        tmpReg32 |= HAFDUP_NO_BACKOFF;
    if(p_DtsecDriverParam->excessDefer)
        tmpReg32 |= HAFDUP_EXCESS_DEFER;
    tmpReg32 |= (((uint32_t)p_DtsecDriverParam->maxRetransmission <<
                HAFDUP_RETRANSMISSION_MAX_SHIFT )& HAFDUP_RETRANSMISSION_MAX);
    tmpReg32|= ((uint32_t)p_DtsecDriverParam->collisionWindow & HAFDUP_COLLISION_WINDOW);

    WRITE_UINT32(p_DtsecMemMap->hafdup, tmpReg32);
    /***************HAFDUP************************/

    /***************MAXFRM************************/
    /* Initialize MAXFRM */
    WRITE_UINT32(p_DtsecMemMap->maxfrm,
                 p_DtsecDriverParam->maxFrameLength);
    err = FmSetMacMaxFrame(p_Dtsec->fmMacControllerDriver.h_Fm,
                           e_FM_MAC_1G,
                           p_Dtsec->fmMacControllerDriver.macId,
                           p_DtsecDriverParam->maxFrameLength);
    if (err)
        RETURN_ERROR(MAJOR, err, NO_MSG);
    /***************MAXFRM************************/

    /***************CAM1************************/
    WRITE_UINT32(p_DtsecMemMap->cam1,0xffffffff);
    WRITE_UINT32(p_DtsecMemMap->cam2,0xffffffff);

    /***************IMASK************************/
    WRITE_UINT32(p_DtsecMemMap->imask, p_Dtsec->exceptions);
    /***************IMASK************************/

    /***************IEVENT************************/
    WRITE_UINT32(p_DtsecMemMap->ievent, EVENTS_MASK);

    /***************MACSTNADDR1/2*****************/
    /*  Initialize MAC Station Address registers (1 & 2)    */
    /*  Station address have to be swapped (big endian to little endian */
    addr = p_Dtsec->addr;

    tmpReg32 = (uint32_t)(addr);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_DtsecMemMap->macstnaddr1, tmpReg32);

    tmpReg32 = (uint32_t)(addr>>32);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_DtsecMemMap->macstnaddr2, tmpReg32);
    /***************MACSTNADDR1/2*****************/

    /***************DEBUG*****************/
    WRITE_UINT32(p_DtsecMemMap->tx_threshold,       (uint32_t)(p_DtsecDriverParam->fifoTxThr & 0x7f));
    WRITE_UINT32(p_DtsecMemMap->tx_watermark_high,  (uint32_t)(p_DtsecDriverParam->fifoTxWatermarkH & 0x7f));
    WRITE_UINT32(p_DtsecMemMap->rx_watermark_low,   (uint32_t)(p_DtsecDriverParam->fifoRxWatermarkL & 0x7f));
    /***************DEBUG*****************/

    /*****************HASH************************/
    for(i=0 ; i<NUM_OF_HASH_REGS ; i++)
    {
        /* Initialize IADDRx */
        WRITE_UINT32(p_DtsecMemMap->igaddr[i], 0);
        /* Initialize GADDRx */
        WRITE_UINT32(p_DtsecMemMap->gaddr[i], 0);
    }

    p_Dtsec->p_MulticastAddrHash = AllocHashTable(HASH_TABLE_SIZE);
    if(!p_Dtsec->p_MulticastAddrHash)
    {
        FreeInitResources(p_Dtsec);
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("MC hash table is FAILED"));
    }

    p_Dtsec->p_UnicastAddrHash = AllocHashTable(HASH_TABLE_SIZE);
    if(!p_Dtsec->p_UnicastAddrHash)
    {
        FreeInitResources(p_Dtsec);
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("UC hash table is FAILED"));
    }

    /* register err intr handler for dtsec to FPM (err)*/
    FmRegisterIntr(p_Dtsec->fmMacControllerDriver.h_Fm, e_FM_MOD_1G_MAC, p_Dtsec->macId, e_FM_INTR_TYPE_ERR, DtsecErrException , p_Dtsec);
    /* register 1588 intr handler for TMR to FPM (normal)*/
    FmRegisterIntr(p_Dtsec->fmMacControllerDriver.h_Fm, e_FM_MOD_1G_MAC_TMR, p_Dtsec->macId, e_FM_INTR_TYPE_NORMAL, Dtsec1588Exception , p_Dtsec);
    /* register normal intr handler for dtsec to main interrupt controller. */
    if (p_Dtsec->mdioIrq != NO_IRQ)
    {
        XX_SetIntr(p_Dtsec->mdioIrq, DtsecException, p_Dtsec);
        XX_EnableIntr(p_Dtsec->mdioIrq);
    }

    XX_Free(p_DtsecDriverParam);
    p_Dtsec->p_DtsecDriverParam = NULL;

    err = DtsecSetStatistics(p_Dtsec, e_FM_MAC_FULL_STATISTICS);
    if(err)
    {
        FreeInitResources(p_Dtsec);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    return E_OK;
}

/* ........................................................................... */

static t_Error DtsecFree(t_Handle h_Dtsec)
{
    t_Dtsec      *p_Dtsec = (t_Dtsec *)h_Dtsec;

    SANITY_CHECK_RETURN_ERROR(p_Dtsec, E_INVALID_HANDLE);

    FreeInitResources(p_Dtsec);

    if (p_Dtsec->p_DtsecDriverParam)
    {
        XX_Free(p_Dtsec->p_DtsecDriverParam);
        p_Dtsec->p_DtsecDriverParam = NULL;
    }
    XX_Free (h_Dtsec);

    return E_OK;
}

/* .............................................................................. */

static void InitFmMacControllerDriver(t_FmMacControllerDriver *p_FmMacControllerDriver)
{
    p_FmMacControllerDriver->f_FM_MAC_Init                      = DtsecInit;
    p_FmMacControllerDriver->f_FM_MAC_Free                      = DtsecFree;

    p_FmMacControllerDriver->f_FM_MAC_SetStatistics             = DtsecSetStatistics;
    p_FmMacControllerDriver->f_FM_MAC_ConfigLoopback            = DtsecConfigLoopback;
    p_FmMacControllerDriver->f_FM_MAC_ConfigMaxFrameLength      = DtsecConfigMaxFrameLength;

    p_FmMacControllerDriver->f_FM_MAC_ConfigWan                 = NULL; /* Not supported on dTSEC */

    p_FmMacControllerDriver->f_FM_MAC_ConfigPadAndCrc           = DtsecConfigPadAndCrc;
    p_FmMacControllerDriver->f_FM_MAC_ConfigHalfDuplex          = DtsecConfigHalfDuplex;
    p_FmMacControllerDriver->f_FM_MAC_ConfigLengthCheck         = DtsecConfigLengthCheck;
    p_FmMacControllerDriver->f_FM_MAC_ConfigException           = DtsecConfigException;

    p_FmMacControllerDriver->f_FM_MAC_Enable                    = DtsecEnable;
    p_FmMacControllerDriver->f_FM_MAC_Disable                   = DtsecDisable;

    p_FmMacControllerDriver->f_FM_MAC_SetException              = DtsecSetException;

    p_FmMacControllerDriver->f_FM_MAC_SetPromiscuous            = DtsecSetPromiscuous;
    p_FmMacControllerDriver->f_FM_MAC_AdjustLink                = DtsecAdjustLink;

    p_FmMacControllerDriver->f_FM_MAC_Enable1588TimeStamp       = DtsecEnable1588TimeStamp;
    p_FmMacControllerDriver->f_FM_MAC_Disable1588TimeStamp      = DtsecDisable1588TimeStamp;

    p_FmMacControllerDriver->f_FM_MAC_SetTxAutoPauseFrames      = DtsecTxMacPause;
    p_FmMacControllerDriver->f_FM_MAC_SetRxIgnorePauseFrames    = DtsecRxIgnoreMacPause;

    p_FmMacControllerDriver->f_FM_MAC_ResetCounters             = DtsecResetCounters;
    p_FmMacControllerDriver->f_FM_MAC_GetStatistics             = DtsecGetStatistics;

    p_FmMacControllerDriver->f_FM_MAC_ModifyMacAddr             = DtsecModifyMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_AddHashMacAddr            = DtsecAddHashMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_RemoveHashMacAddr         = DtsecDelHashMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_AddExactMatchMacAddr      = DtsecAddExactMatchMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_RemovelExactMatchMacAddr  = DtsecDelExactMatchMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_GetId                     = DtsecGetId;
    p_FmMacControllerDriver->f_FM_MAC_GetVersion                = DtsecGetVersion;
    p_FmMacControllerDriver->f_FM_MAC_GetMaxFrameLength         = DtsecGetMaxFrameLength;

    p_FmMacControllerDriver->f_FM_MAC_MII_WritePhyReg           = DTSEC_MII_WritePhyReg;
    p_FmMacControllerDriver->f_FM_MAC_MII_ReadPhyReg            = DTSEC_MII_ReadPhyReg;

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
    p_FmMacControllerDriver->f_FM_MAC_DumpRegs                  = DtsecDumpRegs;
#endif /* (defined(DEBUG_ERRORS) && ... */
}


/*****************************************************************************/
/*                      dTSEC Config  Main Entry                             */
/*****************************************************************************/

/* .............................................................................. */

t_Handle  DTSEC_Config(t_FmMacParams *p_FmMacParam)
{
    t_Dtsec             *p_Dtsec;
    t_DtsecDriverParam  *p_DtsecDriverParam;
    uintptr_t           baseAddr;
    uint8_t             i;

    SANITY_CHECK_RETURN_VALUE(p_FmMacParam, E_NULL_POINTER, NULL);

    baseAddr = p_FmMacParam->baseAddr;
    /* allocate memory for the UCC GETH data structure. */
    p_Dtsec = (t_Dtsec *) XX_Malloc(sizeof(t_Dtsec));
    if (!p_Dtsec)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("dTSEC driver structure"));
        return NULL;
    }
    /* Zero out * p_Dtsec */
    memset(p_Dtsec, 0, sizeof(t_Dtsec));
    InitFmMacControllerDriver(&p_Dtsec->fmMacControllerDriver);

    /* allocate memory for the dTSEC driver parameters data structure. */
    p_DtsecDriverParam = (t_DtsecDriverParam *) XX_Malloc(sizeof(t_DtsecDriverParam));
    if (!p_DtsecDriverParam)
    {
        XX_Free(p_Dtsec);
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("dTSEC driver parameters"));
        return NULL;
    }
    /* Zero out */
    memset(p_DtsecDriverParam, 0, sizeof(t_DtsecDriverParam));

    /* Plant parameter structure pointer */
    p_Dtsec->p_DtsecDriverParam = p_DtsecDriverParam;

    SetDefaultParam(p_DtsecDriverParam);

    for (i=0; i < sizeof(p_FmMacParam->addr); i++)
        p_Dtsec->addr |= ((uint64_t)p_FmMacParam->addr[i] << ((5-i) * 8));

    p_Dtsec->p_MemMap           = (t_DtsecMemMap *)UINT_TO_PTR(baseAddr);
    p_Dtsec->p_MiiMemMap        = (t_MiiAccessMemMap *)UINT_TO_PTR(baseAddr + DTSEC_TO_MII_OFFSET);
    p_Dtsec->enetMode           = p_FmMacParam->enetMode;
    p_Dtsec->macId              = p_FmMacParam->macId;
    p_Dtsec->exceptions         = DEFAULT_exceptions;
    p_Dtsec->mdioIrq            = p_FmMacParam->mdioIrq;
    p_Dtsec->f_Exception        = p_FmMacParam->f_Exception;
    p_Dtsec->f_Event            = p_FmMacParam->f_Event;
    p_Dtsec->h_App              = p_FmMacParam->h_App;

    return p_Dtsec;
}
