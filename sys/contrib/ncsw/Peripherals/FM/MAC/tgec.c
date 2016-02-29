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
 @File          tgec.c

 @Description   FM 10G MAC ...
*//***************************************************************************/

#include "std_ext.h"
#include "string_ext.h"
#include "error_ext.h"
#include "xx_ext.h"
#include "endian_ext.h"
#include "crc_mac_addr_ext.h"
#include "debug_ext.h"

#include "fm_common.h"
#include "tgec.h"


/*****************************************************************************/
/*                      Internal routines                                    */
/*****************************************************************************/

static t_Error CheckInitParameters(t_Tgec    *p_Tgec)
{
    if(ENET_SPEED_FROM_MODE(p_Tgec->enetMode) < e_ENET_SPEED_10000)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Ethernet 10G MAC driver only support 10G speed"));
#if (FM_MAX_NUM_OF_10G_MACS > 0)
    if(p_Tgec->macId >= FM_MAX_NUM_OF_10G_MACS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("macId of 10G can not be greater than 0"));
#endif
    if(p_Tgec->addr == 0)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Ethernet 10G MAC Must have a valid MAC Address"));
    if(!p_Tgec->f_Exception)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("uninitialized f_Exception"));
    if(!p_Tgec->f_Event)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("uninitialized f_Event"));
    return E_OK;
}

/* .............................................................................. */

static void SetDefaultParam(t_TgecDriverParam *p_TgecDriverParam)
{
    p_TgecDriverParam->wanModeEnable            = DEFAULT_wanModeEnable;
    p_TgecDriverParam->promiscuousModeEnable    = DEFAULT_promiscuousModeEnable;
    p_TgecDriverParam->pauseForwardEnable       = DEFAULT_pauseForwardEnable;
    p_TgecDriverParam->pauseIgnore              = DEFAULT_pauseIgnore;
    p_TgecDriverParam->txAddrInsEnable          = DEFAULT_txAddrInsEnable;

    p_TgecDriverParam->loopbackEnable           = DEFAULT_loopbackEnable;
    p_TgecDriverParam->cmdFrameEnable           = DEFAULT_cmdFrameEnable;
    p_TgecDriverParam->rxErrorDiscard           = DEFAULT_rxErrorDiscard;
    p_TgecDriverParam->phyTxenaOn               = DEFAULT_phyTxenaOn;
    p_TgecDriverParam->sendIdleEnable           = DEFAULT_sendIdleEnable;
    p_TgecDriverParam->noLengthCheckEnable      = DEFAULT_noLengthCheckEnable;
    p_TgecDriverParam->lgthCheckNostdr          = DEFAULT_lgthCheckNostdr;
    p_TgecDriverParam->timeStampEnable          = DEFAULT_timeStampEnable;
    p_TgecDriverParam->rxSfdAny                 = DEFAULT_rxSfdAny;
    p_TgecDriverParam->rxPblFwd                 = DEFAULT_rxPblFwd;
    p_TgecDriverParam->txPblFwd                 = DEFAULT_txPblFwd;

    p_TgecDriverParam->txIpgLength              = DEFAULT_txIpgLength;
    p_TgecDriverParam->maxFrameLength           = DEFAULT_maxFrameLength;

    p_TgecDriverParam->debugMode                = DEFAULT_debugMode;

    p_TgecDriverParam->pauseTime                = DEFAULT_pauseTime;

#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
    p_TgecDriverParam->skipFman11Workaround     = DEFAULT_skipFman11Workaround;
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */
}

/* ........................................................................... */

static void TgecErrException(t_Handle h_Tgec)
{
    t_Tgec             *p_Tgec = (t_Tgec *)h_Tgec;
    uint32_t            event;
    t_TgecMemMap        *p_TgecMemMap = p_Tgec->p_MemMap;

    event = GET_UINT32(p_TgecMemMap->ievent);
    /* do not handle MDIO events */
    event &= ~(IMASK_MDIO_SCAN_EVENTMDIO | IMASK_MDIO_CMD_CMPL);

    event &= GET_UINT32(p_TgecMemMap->imask);

    WRITE_UINT32(p_TgecMemMap->ievent, event);

    if (event & IMASK_REM_FAULT)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_REM_FAULT);
    if (event & IMASK_LOC_FAULT)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_LOC_FAULT);
    if (event & IMASK_1TX_ECC_ER)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_1TX_ECC_ER);
    if (event & IMASK_TX_FIFO_UNFL)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_TX_FIFO_UNFL);
    if (event & IMASK_TX_FIFO_OVFL)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_TX_FIFO_OVFL);
    if (event & IMASK_TX_ER)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_TX_ER);
    if (event & IMASK_RX_FIFO_OVFL)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_FIFO_OVFL);
    if (event & IMASK_RX_ECC_ER)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_ECC_ER);
    if (event & IMASK_RX_JAB_FRM)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_JAB_FRM);
    if (event & IMASK_RX_OVRSZ_FRM)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_OVRSZ_FRM);
    if (event & IMASK_RX_RUNT_FRM)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_RUNT_FRM);
    if (event & IMASK_RX_FRAG_FRM)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_FRAG_FRM);
    if (event & IMASK_RX_LEN_ER)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_LEN_ER);
    if (event & IMASK_RX_CRC_ER)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_CRC_ER);
    if (event & IMASK_RX_ALIGN_ER)
        p_Tgec->f_Exception(p_Tgec->h_App, e_FM_MAC_EX_10G_RX_ALIGN_ER);
}

static void TgecException(t_Handle h_Tgec)
{
     t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;
     uint32_t            event;
     t_TgecMemMap        *p_TgecMemMap = p_Tgec->p_MemMap;

     event = GET_UINT32(p_TgecMemMap->ievent);
     /* handle only MDIO events */
     event &= (IMASK_MDIO_SCAN_EVENTMDIO | IMASK_MDIO_CMD_CMPL);
     event &= GET_UINT32(p_TgecMemMap->imask);

     WRITE_UINT32(p_TgecMemMap->ievent, event);

     if(event & IMASK_MDIO_SCAN_EVENTMDIO)
         p_Tgec->f_Event(p_Tgec->h_App, e_FM_MAC_EX_10G_MDIO_SCAN_EVENTMDIO);
     if(event & IMASK_MDIO_CMD_CMPL)
         p_Tgec->f_Event(p_Tgec->h_App, e_FM_MAC_EX_10G_MDIO_CMD_CMPL);
}

static void FreeInitResources(t_Tgec *p_Tgec)
{
    if ((p_Tgec->mdioIrq != 0) && (p_Tgec->mdioIrq != NO_IRQ))
    {
        XX_DisableIntr(p_Tgec->mdioIrq);
        XX_FreeIntr(p_Tgec->mdioIrq);
    }
    else if (p_Tgec->mdioIrq == 0)
        REPORT_ERROR(MINOR, E_NOT_SUPPORTED, (NO_MSG));
    FmUnregisterIntr(p_Tgec->fmMacControllerDriver.h_Fm, e_FM_MOD_10G_MAC, p_Tgec->macId, e_FM_INTR_TYPE_ERR);

    /* release the driver's group hash table */
    FreeHashTable(p_Tgec->p_MulticastAddrHash);
    p_Tgec->p_MulticastAddrHash =   NULL;

    /* release the driver's individual hash table */
    FreeHashTable(p_Tgec->p_UnicastAddrHash);
    p_Tgec->p_UnicastAddrHash =     NULL;
}

/* .............................................................................. */

static void HardwareClearAddrInPaddr(t_Tgec   *p_Tgec, uint8_t paddrNum)
{
    if (paddrNum != 0)
        return;             /* At this time MAC has only one address */

    WRITE_UINT32(p_Tgec->p_MemMap->mac_addr_2, 0x0);
    WRITE_UINT32(p_Tgec->p_MemMap->mac_addr_3, 0x0);
}

/* ........................................................................... */

static void HardwareAddAddrInPaddr(t_Tgec   *p_Tgec, uint64_t *p_Addr, uint8_t paddrNum)
{
    uint32_t        tmpReg32 = 0;
    uint64_t        addr = *p_Addr;
    t_TgecMemMap    *p_TgecMemMap = p_Tgec->p_MemMap;

    if (paddrNum != 0)
        return;             /* At this time MAC has only one address */

    tmpReg32 = (uint32_t)(addr>>16);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_TgecMemMap->mac_addr_2, tmpReg32);

    tmpReg32 = (uint32_t)(addr);
    SwapUint32P(&tmpReg32);
    tmpReg32 >>= 16;
    WRITE_UINT32(p_TgecMemMap->mac_addr_3, tmpReg32);
}

/*****************************************************************************/
/*                     10G MAC API routines                                  */
/*****************************************************************************/

/* .............................................................................. */

static t_Error TgecEnable(t_Handle h_Tgec,  e_CommMode mode)
{
    t_Tgec *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap       *p_MemMap ;
    uint32_t            tmpReg32 = 0;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_INVALID_HANDLE);

    p_MemMap= (t_TgecMemMap*)(p_Tgec->p_MemMap);

    tmpReg32 = GET_UINT32(p_MemMap->cmd_conf_ctrl);

    switch (mode)
    {
        case e_COMM_MODE_NONE:
            tmpReg32 &= ~(CMD_CFG_TX_EN | CMD_CFG_RX_EN);
            break;
        case e_COMM_MODE_RX :
            tmpReg32 |= CMD_CFG_RX_EN ;
            break;
        case e_COMM_MODE_TX :
            tmpReg32 |= CMD_CFG_TX_EN ;
            break;
        case e_COMM_MODE_RX_AND_TX:
            tmpReg32 |= (CMD_CFG_TX_EN | CMD_CFG_RX_EN);
            break;
    }

    WRITE_UINT32(p_MemMap->cmd_conf_ctrl, tmpReg32);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecDisable (t_Handle h_Tgec, e_CommMode mode)
{
    t_Tgec *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap       *p_MemMap ;
    uint32_t            tmpReg32 = 0;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_INVALID_HANDLE);

    p_MemMap= (t_TgecMemMap*)(p_Tgec->p_MemMap);

    tmpReg32 = GET_UINT32(p_MemMap->cmd_conf_ctrl);
    switch (mode)
    {
        case e_COMM_MODE_RX:
            tmpReg32 &= ~CMD_CFG_RX_EN;
            break;
        case e_COMM_MODE_TX:
            tmpReg32 &= ~CMD_CFG_TX_EN;
            break;
        case e_COMM_MODE_RX_AND_TX:
            tmpReg32 &= ~(CMD_CFG_TX_EN | CMD_CFG_RX_EN);
        break;
        default:
            RETURN_ERROR(MINOR, E_INVALID_SELECTION, NO_MSG);
    }
    WRITE_UINT32(p_MemMap->cmd_conf_ctrl, tmpReg32);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecSetPromiscuous(t_Handle h_Tgec, bool newVal)
{
    t_Tgec       *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap *p_TgecMemMap;
    uint32_t     tmpReg32;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_NULL_POINTER);

    p_TgecMemMap = p_Tgec->p_MemMap;

    tmpReg32 = GET_UINT32(p_TgecMemMap->cmd_conf_ctrl);

    if (newVal)
        tmpReg32 |= CMD_CFG_PROMIS_EN;
    else
        tmpReg32 &= ~CMD_CFG_PROMIS_EN;

    WRITE_UINT32(p_TgecMemMap->cmd_conf_ctrl, tmpReg32);

    return E_OK;
}


/*****************************************************************************/
/*                      Tgec Configs modification functions                 */
/*****************************************************************************/

/* .............................................................................. */

static t_Error TgecConfigLoopback(t_Handle h_Tgec, bool newVal)
{
    t_Tgec *p_Tgec = (t_Tgec *)h_Tgec;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_TgecDriverParam, E_INVALID_STATE);

#ifdef FM_NO_TGEC_LOOPBACK
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Tgec->fmMacControllerDriver.h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("no loopback in this chip rev!"));
    }
#endif /* FM_NO_TGEC_LOOPBACK */

    p_Tgec->p_TgecDriverParam->loopbackEnable = newVal;

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecConfigWan(t_Handle h_Tgec, bool newVal)
{
    t_Tgec *p_Tgec = (t_Tgec *)h_Tgec;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_TgecDriverParam, E_INVALID_STATE);

    p_Tgec->p_TgecDriverParam->wanModeEnable = newVal;

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecConfigMaxFrameLength(t_Handle h_Tgec, uint16_t newVal)
{
    t_Tgec *p_Tgec = (t_Tgec *)h_Tgec;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_TgecDriverParam, E_INVALID_STATE);

    p_Tgec->p_TgecDriverParam->maxFrameLength = newVal;

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecConfigLengthCheck(t_Handle h_Tgec, bool newVal)
{
#ifdef FM_LEN_CHECK_ERRATA_FMAN_SW002
UNUSED(h_Tgec);
    RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("LengthCheck!"));

#else
    t_Tgec *p_Tgec = (t_Tgec *)h_Tgec;

    UNUSED(newVal);

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_TgecDriverParam, E_INVALID_STATE);

    p_Tgec->p_TgecDriverParam->noLengthCheckEnable = !newVal;

    return E_OK;
#endif /* FM_LEN_CHECK_ERRATA_FMAN_SW002 */
}

/* .............................................................................. */

static t_Error TgecConfigException(t_Handle h_Tgec, e_FmMacExceptions exception, bool enable)
{
    t_Tgec      *p_Tgec = (t_Tgec *)h_Tgec;
    uint32_t    bitMask = 0;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_TgecDriverParam, E_INVALID_STATE);
#ifdef FM_10G_REM_N_LCL_FLT_EX_ERRATA_10GMAC001
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Tgec->fmMacControllerDriver.h_Fm, &revInfo);
        if((revInfo.majorRev <=2) &&
            enable &&
            ((exception == e_FM_MAC_EX_10G_LOC_FAULT) || (exception == e_FM_MAC_EX_10G_REM_FAULT)))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_MAC_EX_10G_LOC_FAULT and e_FM_MAC_EX_10G_REM_FAULT !"));
    }
#endif   /* FM_10G_REM_N_LCL_FLT_EX_ERRATA_10GMAC001 */

    GET_EXCEPTION_FLAG(bitMask, exception);
    if(bitMask)
    {
        if (enable)
            p_Tgec->exceptions |= bitMask;
        else
            p_Tgec->exceptions &= ~bitMask;
    }
    else
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));

    return E_OK;
}

#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
/* .............................................................................. */

static t_Error TgecConfigSkipFman11Workaround(t_Handle h_Tgec)
{
    t_Tgec      *p_Tgec = (t_Tgec *)h_Tgec;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_TgecDriverParam, E_INVALID_STATE);

    p_Tgec->p_TgecDriverParam->skipFman11Workaround     = TRUE;

    return E_OK;
}
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */


/*****************************************************************************/
/*                      Tgec Run Time API functions                         */
/*****************************************************************************/

/* .............................................................................. */

static t_Error TgecTxMacPause(t_Handle h_Tgec, uint16_t pauseTime)
{
    t_Tgec          *p_Tgec = (t_Tgec *)h_Tgec;
    uint32_t        ptv = 0;
    t_TgecMemMap    *p_MemMap;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_INVALID_STATE);

    p_MemMap = (t_TgecMemMap*)(p_Tgec->p_MemMap);

    ptv = (uint32_t)pauseTime;

    WRITE_UINT32(p_MemMap->pause_quant, ptv);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecRxIgnoreMacPause(t_Handle h_Tgec, bool en)
{
    t_Tgec          *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap    *p_MemMap;
    uint32_t        tmpReg32;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_INVALID_STATE);

    p_MemMap = (t_TgecMemMap*)(p_Tgec->p_MemMap);
    tmpReg32 = GET_UINT32(p_MemMap->cmd_conf_ctrl);
    if (en)
        tmpReg32 |= CMD_CFG_PAUSE_IGNORE;
    else
        tmpReg32 &= ~CMD_CFG_PAUSE_IGNORE;
    WRITE_UINT32(p_MemMap->cmd_conf_ctrl, tmpReg32);

    return E_OK;
}

/* Counters handling */
/* .............................................................................. */

static t_Error TgecGetStatistics(t_Handle h_Tgec, t_FmMacStatistics *p_Statistics)
{
    t_Tgec          *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap    *p_TgecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Statistics, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_NULL_POINTER);

    p_TgecMemMap = p_Tgec->p_MemMap;

    p_Statistics->eStatPkts64           = GET_UINT64(p_TgecMemMap->R64);
    p_Statistics->eStatPkts65to127      = GET_UINT64(p_TgecMemMap->R127);
    p_Statistics->eStatPkts128to255     = GET_UINT64(p_TgecMemMap->R255);
    p_Statistics->eStatPkts256to511     = GET_UINT64(p_TgecMemMap->R511);
    p_Statistics->eStatPkts512to1023    = GET_UINT64(p_TgecMemMap->R1023);
    p_Statistics->eStatPkts1024to1518   = GET_UINT64(p_TgecMemMap->R1518);
    p_Statistics->eStatPkts1519to1522   = GET_UINT64(p_TgecMemMap->R1519X);
/* */
    p_Statistics->eStatFragments        = GET_UINT64(p_TgecMemMap->TRFRG);
    p_Statistics->eStatJabbers          = GET_UINT64(p_TgecMemMap->TRJBR);

    p_Statistics->eStatsDropEvents      = GET_UINT64(p_TgecMemMap->RDRP);
    p_Statistics->eStatCRCAlignErrors   = GET_UINT64(p_TgecMemMap->RALN);

    p_Statistics->eStatUndersizePkts    = GET_UINT64(p_TgecMemMap->TRUND);
    p_Statistics->eStatOversizePkts     = GET_UINT64(p_TgecMemMap->TROVR);
/* Pause */
    p_Statistics->reStatPause           = GET_UINT64(p_TgecMemMap->RXPF);
    p_Statistics->teStatPause           = GET_UINT64(p_TgecMemMap->TXPF);


/* MIB II */
    p_Statistics->ifInOctets            = GET_UINT64(p_TgecMemMap->ROCT);
    p_Statistics->ifInMcastPkts         = GET_UINT64(p_TgecMemMap->RMCA);
    p_Statistics->ifInBcastPkts         = GET_UINT64(p_TgecMemMap->RBCA);
    p_Statistics->ifInPkts              = GET_UINT64(p_TgecMemMap->RUCA)
                                        + p_Statistics->ifInMcastPkts
                                        + p_Statistics->ifInBcastPkts;
    p_Statistics->ifInDiscards          = 0;
    p_Statistics->ifInErrors            = GET_UINT64(p_TgecMemMap->RERR);

    p_Statistics->ifOutOctets           = GET_UINT64(p_TgecMemMap->TOCT);
    p_Statistics->ifOutMcastPkts        = GET_UINT64(p_TgecMemMap->TMCA);
    p_Statistics->ifOutBcastPkts        = GET_UINT64(p_TgecMemMap->TBCA);
    p_Statistics->ifOutPkts             = GET_UINT64(p_TgecMemMap->TUCA);
    p_Statistics->ifOutDiscards         = 0;
    p_Statistics->ifOutErrors           = GET_UINT64(p_TgecMemMap->TERR);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecEnable1588TimeStamp(t_Handle h_Tgec)
{
    t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap        *p_TgecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_INVALID_STATE);

    p_TgecMemMap = p_Tgec->p_MemMap;
    SANITY_CHECK_RETURN_ERROR(p_TgecMemMap, E_INVALID_HANDLE);

    WRITE_UINT32(p_TgecMemMap->cmd_conf_ctrl, GET_UINT32(p_TgecMemMap->cmd_conf_ctrl) | CMD_CFG_EN_TIMESTAMP);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecDisable1588TimeStamp(t_Handle h_Tgec)
{
    t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap        *p_TgecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_INVALID_STATE);

    p_TgecMemMap = p_Tgec->p_MemMap;
    SANITY_CHECK_RETURN_ERROR(p_TgecMemMap, E_INVALID_HANDLE);

    WRITE_UINT32(p_TgecMemMap->cmd_conf_ctrl, GET_UINT32(p_TgecMemMap->cmd_conf_ctrl) & ~CMD_CFG_EN_TIMESTAMP);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecModifyMacAddress (t_Handle h_Tgec, t_EnetAddr *p_EnetAddr)
{
    t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap        *p_TgecMemMap;
    uint32_t            tmpReg32 = 0;
    uint64_t            addr;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_NULL_POINTER);

    p_TgecMemMap = p_Tgec->p_MemMap;

    /*  Initialize MAC Station Address registers (1 & 2)    */
    /*  Station address have to be swapped (big endian to little endian */

    addr = ((*(uint64_t *)p_EnetAddr) >> 16);
    p_Tgec->addr = addr;

    tmpReg32 = (uint32_t)(addr>>16);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_TgecMemMap->mac_addr_0, tmpReg32);

    tmpReg32 = (uint32_t)(addr);
    SwapUint32P(&tmpReg32);
    tmpReg32 >>= 16;
    WRITE_UINT32(p_TgecMemMap->mac_addr_1, tmpReg32);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecResetCounters (t_Handle h_Tgec)
{
    t_Tgec *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap       *p_MemMap ;
    uint32_t            tmpReg32, cmdConfCtrl;
    int i;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_INVALID_HANDLE);

    p_MemMap= (t_TgecMemMap*)(p_Tgec->p_MemMap);

    cmdConfCtrl = GET_UINT32(p_MemMap->cmd_conf_ctrl);

    cmdConfCtrl |= CMD_CFG_STAT_CLR;

    WRITE_UINT32(p_MemMap->cmd_conf_ctrl, cmdConfCtrl);

    for (i=0; i<1000; i++)
    {
        tmpReg32 = GET_UINT32(p_MemMap->cmd_conf_ctrl);
        if (!(tmpReg32 & CMD_CFG_STAT_CLR))
            break;
    }

    cmdConfCtrl &= ~CMD_CFG_STAT_CLR;
    WRITE_UINT32(p_MemMap->cmd_conf_ctrl, cmdConfCtrl);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecAddExactMatchMacAddress(t_Handle h_Tgec, t_EnetAddr *p_EthAddr)
{
    t_Tgec   *p_Tgec = (t_Tgec *) h_Tgec;
    uint64_t  ethAddr;
    uint8_t   paddrNum;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);

    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    if (ethAddr & GROUP_ADDRESS)
        /* Multicast address has no effect in PADDR */
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Multicast address"));

    /* Make sure no PADDR contains this address */
    for (paddrNum = 0; paddrNum < TGEC_NUM_OF_PADDRS; paddrNum++)
    {
        if (p_Tgec->indAddrRegUsed[paddrNum])
        {
            if (p_Tgec->paddr[paddrNum] == ethAddr)
            {
                RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, NO_MSG);
            }
        }
    }

    /* Find first unused PADDR */
    for (paddrNum = 0; paddrNum < TGEC_NUM_OF_PADDRS; paddrNum++)
    {
        if (!(p_Tgec->indAddrRegUsed[paddrNum]))
        {
            /* mark this PADDR as used */
            p_Tgec->indAddrRegUsed[paddrNum] = TRUE;
            /* store address */
            p_Tgec->paddr[paddrNum] = ethAddr;

            /* put in hardware */
            HardwareAddAddrInPaddr(p_Tgec, &ethAddr, paddrNum);
            p_Tgec->numOfIndAddrInRegs++;

            return E_OK;
        }
    }

    /* No free PADDR */
    RETURN_ERROR(MAJOR, E_FULL, NO_MSG);
}

/* .............................................................................. */

static t_Error TgecDelExactMatchMacAddress(t_Handle h_Tgec, t_EnetAddr *p_EthAddr)
{
    t_Tgec   *p_Tgec = (t_Tgec *) h_Tgec;
    uint64_t  ethAddr;
    uint8_t   paddrNum;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_INVALID_HANDLE);

    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    /* Find used PADDR containing this address */
    for (paddrNum = 0; paddrNum < TGEC_NUM_OF_PADDRS; paddrNum++)
    {
        if ((p_Tgec->indAddrRegUsed[paddrNum]) &&
            (p_Tgec->paddr[paddrNum] == ethAddr))
        {
            /* mark this PADDR as not used */
            p_Tgec->indAddrRegUsed[paddrNum] = FALSE;
            /* clear in hardware */
            HardwareClearAddrInPaddr(p_Tgec, paddrNum);
            p_Tgec->numOfIndAddrInRegs--;

            return E_OK;
        }
    }

    RETURN_ERROR(MAJOR, E_NOT_FOUND, NO_MSG);
}

/* .............................................................................. */

static t_Error TgecAddHashMacAddress(t_Handle h_Tgec, t_EnetAddr *p_EthAddr)
{
    t_Tgec          *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap    *p_TgecMemMap;
    t_EthHashEntry  *p_HashEntry;
    uint32_t        crc;
    uint32_t        hash;
    uint64_t        ethAddr;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_NULL_POINTER);

    p_TgecMemMap = p_Tgec->p_MemMap;
    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    if (!(ethAddr & GROUP_ADDRESS))
        /* Unicast addresses not supported in hash */
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Unicast Address"));

    /* CRC calculation */
    GET_MAC_ADDR_CRC(ethAddr, crc);
    crc = MIRROR_32(crc);

    hash = (crc >> HASH_CTRL_MCAST_SHIFT) & HASH_ADDR_MASK;        /* Take 9 MSB bits */

    /* Create element to be added to the driver hash table */
    p_HashEntry = (t_EthHashEntry *)XX_Malloc(sizeof(t_EthHashEntry));
    p_HashEntry->addr = ethAddr;
    INIT_LIST(&p_HashEntry->node);

    LIST_AddToTail(&(p_HashEntry->node), &(p_Tgec->p_MulticastAddrHash->p_Lsts[hash]));
    WRITE_UINT32(p_TgecMemMap->hashtable_ctrl, (hash | HASH_CTRL_MCAST_EN));

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecDelHashMacAddress(t_Handle h_Tgec, t_EnetAddr *p_EthAddr)
{
    t_Tgec          *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap    *p_TgecMemMap;
    t_EthHashEntry  *p_HashEntry = NULL;
    t_List          *p_Pos;
    uint32_t        crc;
    uint32_t        hash;
    uint64_t        ethAddr;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_NULL_POINTER);

    p_TgecMemMap = p_Tgec->p_MemMap;
    ethAddr = ((*(uint64_t *)p_EthAddr) >> 16);

    /* CRC calculation */
    GET_MAC_ADDR_CRC(ethAddr, crc);
    crc = MIRROR_32(crc);

    hash = (crc >> HASH_CTRL_MCAST_SHIFT) & HASH_ADDR_MASK;        /* Take 9 MSB bits */

    LIST_FOR_EACH(p_Pos, &(p_Tgec->p_MulticastAddrHash->p_Lsts[hash]))
    {

        p_HashEntry = ETH_HASH_ENTRY_OBJ(p_Pos);
        if(p_HashEntry->addr == ethAddr)
        {
            LIST_DelAndInit(&p_HashEntry->node);
            XX_Free(p_HashEntry);
            break;
        }
    }
    if(LIST_IsEmpty(&p_Tgec->p_MulticastAddrHash->p_Lsts[hash]))
        WRITE_UINT32(p_TgecMemMap->hashtable_ctrl, (hash & ~HASH_CTRL_MCAST_EN));

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecGetId(t_Handle h_Tgec, uint32_t *macId)
{
    t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_NULL_POINTER);

    UNUSED(p_Tgec);
    UNUSED(macId);
    RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("TgecGetId Not Supported"));
}

/* .............................................................................. */

static t_Error TgecGetVersion(t_Handle h_Tgec, uint32_t *macVersion)
{
    t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecMemMap        *p_TgecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_NULL_POINTER);

    p_TgecMemMap = p_Tgec->p_MemMap;
    *macVersion = GET_UINT32(p_TgecMemMap->tgec_id);

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecSetExcpetion(t_Handle h_Tgec, e_FmMacExceptions exception, bool enable)
{
    t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;
    uint32_t            bitMask = 0, tmpReg;
    t_TgecMemMap        *p_TgecMemMap;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_Tgec->p_TgecDriverParam, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_NULL_POINTER);

    p_TgecMemMap = p_Tgec->p_MemMap;
#ifdef FM_10G_REM_N_LCL_FLT_EX_ERRATA_10GMAC001
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Tgec->fmMacControllerDriver.h_Fm, &revInfo);
        if((revInfo.majorRev <=2) &&
            enable &&
            ((exception == e_FM_MAC_EX_10G_LOC_FAULT) || (exception == e_FM_MAC_EX_10G_REM_FAULT)))
                RETURN_ERROR(MINOR, E_NOT_SUPPORTED, ("e_FM_MAC_EX_10G_LOC_FAULT and e_FM_MAC_EX_10G_REM_FAULT !"));
    }
#endif   /* FM_10G_REM_N_LCL_FLT_EX_ERRATA_10GMAC001 */

    GET_EXCEPTION_FLAG(bitMask, exception);
    if(bitMask)
    {
        if (enable)
            p_Tgec->exceptions |= bitMask;
        else
            p_Tgec->exceptions &= ~bitMask;
   }
    else
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Undefined exception"));

    tmpReg = GET_UINT32(p_TgecMemMap->imask);
    if(enable)
        tmpReg |= bitMask;
    else
        tmpReg &= ~bitMask;
    WRITE_UINT32(p_TgecMemMap->imask, tmpReg);
    return E_OK;
}

/* .............................................................................. */

static uint16_t TgecGetMaxFrameLength(t_Handle h_Tgec)
{
    t_Tgec              *p_Tgec = (t_Tgec *)h_Tgec;

    SANITY_CHECK_RETURN_VALUE(p_Tgec, E_INVALID_HANDLE, 0);

    return (uint16_t)GET_UINT32(p_Tgec->p_MemMap->maxfrm);
}

/* .............................................................................. */

#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
static t_Error TgecTxEccWorkaround(t_Tgec *p_Tgec)
{
    t_Error err;

    XX_Print("Applying 10G tx-ecc error workaround (10GMAC-A004) ...");
    /* enable and set promiscuous */
    WRITE_UINT32(p_Tgec->p_MemMap->cmd_conf_ctrl, CMD_CFG_PROMIS_EN | CMD_CFG_TX_EN | CMD_CFG_RX_EN);
    err = Fm10GTxEccWorkaround(p_Tgec->fmMacControllerDriver.h_Fm, p_Tgec->macId);
    /* disable */
    WRITE_UINT32(p_Tgec->p_MemMap->cmd_conf_ctrl, 0);
    if (err)
        XX_Print("FAILED!\n");
    else
        XX_Print("done.\n");
    TgecResetCounters (p_Tgec);

    return err;
}
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */

/* .............................................................................. */

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
static t_Error TgecDumpRegs(t_Handle h_Tgec)
{
    t_Tgec    *p_Tgec = (t_Tgec *)h_Tgec;

    DECLARE_DUMP;

    if (p_Tgec->p_MemMap)
    {
        DUMP_TITLE(p_Tgec->p_MemMap, ("10G MAC %d: ", p_Tgec->macId));
        DUMP_VAR(p_Tgec->p_MemMap, tgec_id);
        DUMP_VAR(p_Tgec->p_MemMap, scratch);
        DUMP_VAR(p_Tgec->p_MemMap, cmd_conf_ctrl);
        DUMP_VAR(p_Tgec->p_MemMap, mac_addr_0);
        DUMP_VAR(p_Tgec->p_MemMap, mac_addr_1);
        DUMP_VAR(p_Tgec->p_MemMap, maxfrm);
        DUMP_VAR(p_Tgec->p_MemMap, pause_quant);
        DUMP_VAR(p_Tgec->p_MemMap, rx_fifo_sections);
        DUMP_VAR(p_Tgec->p_MemMap, tx_fifo_sections);
        DUMP_VAR(p_Tgec->p_MemMap, rx_fifo_almost_f_e);
        DUMP_VAR(p_Tgec->p_MemMap, tx_fifo_almost_f_e);
        DUMP_VAR(p_Tgec->p_MemMap, hashtable_ctrl);
        DUMP_VAR(p_Tgec->p_MemMap, mdio_cfg_status);
        DUMP_VAR(p_Tgec->p_MemMap, mdio_command);
        DUMP_VAR(p_Tgec->p_MemMap, mdio_data);
        DUMP_VAR(p_Tgec->p_MemMap, mdio_regaddr);
        DUMP_VAR(p_Tgec->p_MemMap, status);
        DUMP_VAR(p_Tgec->p_MemMap, tx_ipg_len);
        DUMP_VAR(p_Tgec->p_MemMap, mac_addr_2);
        DUMP_VAR(p_Tgec->p_MemMap, mac_addr_3);
        DUMP_VAR(p_Tgec->p_MemMap, rx_fifo_ptr_rd);
        DUMP_VAR(p_Tgec->p_MemMap, rx_fifo_ptr_wr);
        DUMP_VAR(p_Tgec->p_MemMap, tx_fifo_ptr_rd);
        DUMP_VAR(p_Tgec->p_MemMap, tx_fifo_ptr_wr);
        DUMP_VAR(p_Tgec->p_MemMap, imask);
        DUMP_VAR(p_Tgec->p_MemMap, ievent);
        DUMP_VAR(p_Tgec->p_MemMap, udp_port);
        DUMP_VAR(p_Tgec->p_MemMap, type_1588v2);
    }

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && ... */


/*****************************************************************************/
/*                      FM Init & Free API                                   */
/*****************************************************************************/

/* .............................................................................. */

static t_Error TgecInit(t_Handle h_Tgec)
{
    t_Tgec                  *p_Tgec = (t_Tgec *)h_Tgec;
    t_TgecDriverParam       *p_TgecDriverParam;
    t_TgecMemMap            *p_MemMap;
    uint64_t                addr;
    uint32_t                tmpReg32;
    t_Error                 err;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_TgecDriverParam, E_INVALID_STATE);
    SANITY_CHECK_RETURN_ERROR(p_Tgec->p_MemMap, E_INVALID_HANDLE);

#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
    if (!p_Tgec->p_TgecDriverParam->skipFman11Workaround &&
        ((err = TgecTxEccWorkaround(p_Tgec)) != E_OK))
#ifdef NCSW_LINUX
    {
        /* the workaround fails in simics, just report and continue initialization */
        REPORT_ERROR(MAJOR, err, ("TgecTxEccWorkaround FAILED, skipping workaround"));
    }
#else
    {
        FreeInitResources(p_Tgec);
        RETURN_ERROR(MAJOR, err, ("TgecTxEccWorkaround FAILED"));
    }
#endif
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */

    CHECK_INIT_PARAMETERS(p_Tgec, CheckInitParameters);

    p_TgecDriverParam = p_Tgec->p_TgecDriverParam;
    p_MemMap = p_Tgec->p_MemMap;

    /* MAC Address */
    addr = p_Tgec->addr;
    tmpReg32 = (uint32_t)(addr>>16);
    SwapUint32P(&tmpReg32);
    WRITE_UINT32(p_MemMap->mac_addr_0, tmpReg32);

    tmpReg32 = (uint32_t)(addr);
    SwapUint32P(&tmpReg32);
    tmpReg32 >>= 16;
    WRITE_UINT32(p_MemMap->mac_addr_1, tmpReg32);

    /* Config */
    tmpReg32 = 0;
    if (p_TgecDriverParam->wanModeEnable)
        tmpReg32 |= CMD_CFG_WAN_MODE;
    if (p_TgecDriverParam->promiscuousModeEnable)
        tmpReg32 |= CMD_CFG_PROMIS_EN;
    if (p_TgecDriverParam->pauseForwardEnable)
        tmpReg32 |= CMD_CFG_PAUSE_FWD;
    if (p_TgecDriverParam->pauseIgnore)
        tmpReg32 |= CMD_CFG_PAUSE_IGNORE;
    if (p_TgecDriverParam->txAddrInsEnable)
        tmpReg32 |= CMD_CFG_TX_ADDR_INS;
    if (p_TgecDriverParam->loopbackEnable)
        tmpReg32 |= CMD_CFG_LOOPBACK_EN;
    if (p_TgecDriverParam->cmdFrameEnable)
        tmpReg32 |= CMD_CFG_CMD_FRM_EN;
    if (p_TgecDriverParam->rxErrorDiscard)
        tmpReg32 |= CMD_CFG_RX_ER_DISC;
    if (p_TgecDriverParam->phyTxenaOn)
        tmpReg32 |= CMD_CFG_PHY_TX_EN;
    if (p_TgecDriverParam->sendIdleEnable)
        tmpReg32 |= CMD_CFG_SEND_IDLE;
    if (p_TgecDriverParam->noLengthCheckEnable)
        tmpReg32 |= CMD_CFG_NO_LEN_CHK;
    if (p_TgecDriverParam->lgthCheckNostdr)
        tmpReg32 |= CMD_CFG_LEN_CHK_NOSTDR;
    if (p_TgecDriverParam->timeStampEnable)
        tmpReg32 |= CMD_CFG_EN_TIMESTAMP;
    if (p_TgecDriverParam->rxSfdAny)
        tmpReg32 |= RX_SFD_ANY;
    if (p_TgecDriverParam->rxPblFwd)
        tmpReg32 |= CMD_CFG_RX_PBL_FWD;
    if (p_TgecDriverParam->txPblFwd)
        tmpReg32 |= CMD_CFG_TX_PBL_FWD;
    tmpReg32 |= 0x40;
    WRITE_UINT32(p_MemMap->cmd_conf_ctrl, tmpReg32);

    /* Max Frame Length */
    WRITE_UINT32(p_MemMap->maxfrm, (uint32_t)p_TgecDriverParam->maxFrameLength);
    err = FmSetMacMaxFrame(p_Tgec->fmMacControllerDriver.h_Fm, e_FM_MAC_10G, p_Tgec->fmMacControllerDriver.macId, p_TgecDriverParam->maxFrameLength);
    if(err)
    {
        FreeInitResources(p_Tgec);
        RETURN_ERROR(MAJOR, err, NO_MSG);
    }

    /* Pause Time */
    WRITE_UINT32(p_MemMap->pause_quant, p_TgecDriverParam->pauseTime);

#ifdef FM_TX_FIFO_CORRUPTION_ERRATA_10GMAC_A007
    WRITE_UINT32(p_Tgec->p_MemMap->tx_ipg_len,
        (GET_UINT32(p_Tgec->p_MemMap->tx_ipg_len) & ~TX_IPG_LENGTH_MASK) | DEFAULT_txIpgLength);
#endif /* FM_TX_FIFO_CORRUPTION_ERRATA_10GMAC_A007 */

    /* Configure MII */
    tmpReg32  = GET_UINT32(p_Tgec->p_MiiMemMap->mdio_cfg_status);
#ifdef FM_10G_MDIO_HOLD_ERRATA_XAUI3
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Tgec->fmMacControllerDriver.h_Fm, &revInfo);
        if ((revInfo.majorRev == 1) && (revInfo.minorRev == 0))
            tmpReg32 |= (MIIMCOM_MDIO_HOLD_4_REG_CLK << 2);
    }
#endif /* FM_10G_MDIO_HOLD_ERRATA_XAUI3 */
    tmpReg32 &= ~MIIMCOM_DIV_MASK;
     /* (one half of fm clock => 2.5Mhz) */
    tmpReg32 |=((((p_Tgec->fmMacControllerDriver.clkFreq*10)/2)/25) << MIIMCOM_DIV_SHIFT);
    WRITE_UINT32(p_Tgec->p_MiiMemMap->mdio_cfg_status, tmpReg32);

    p_Tgec->p_MulticastAddrHash = AllocHashTable(HASH_TABLE_SIZE);
    if(!p_Tgec->p_MulticastAddrHash)
    {
        FreeInitResources(p_Tgec);
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("allocation hash table is FAILED"));
    }

    p_Tgec->p_UnicastAddrHash = AllocHashTable(HASH_TABLE_SIZE);
    if(!p_Tgec->p_UnicastAddrHash)
    {
        FreeInitResources(p_Tgec);
        RETURN_ERROR(MAJOR, E_NO_MEMORY, ("allocation hash table is FAILED"));
    }

    /* interrupts */
#ifdef FM_10G_REM_N_LCL_FLT_EX_ERRATA_10GMAC001
    {
        t_FmRevisionInfo revInfo;
        FM_GetRevision(p_Tgec->fmMacControllerDriver.h_Fm, &revInfo);
        if (revInfo.majorRev <=2)
            p_Tgec->exceptions &= ~(IMASK_REM_FAULT | IMASK_LOC_FAULT);
    }
#endif /* FM_10G_REM_N_LCL_FLT_EX_ERRATA_10GMAC001 */
    WRITE_UINT32(p_MemMap->ievent, EVENTS_MASK);
    WRITE_UINT32(p_MemMap->imask, p_Tgec->exceptions);

    FmRegisterIntr(p_Tgec->fmMacControllerDriver.h_Fm, e_FM_MOD_10G_MAC, p_Tgec->macId, e_FM_INTR_TYPE_ERR, TgecErrException , p_Tgec);
    if ((p_Tgec->mdioIrq != 0) && (p_Tgec->mdioIrq != NO_IRQ))
    {
        XX_SetIntr(p_Tgec->mdioIrq, TgecException, p_Tgec);
        XX_EnableIntr(p_Tgec->mdioIrq);
    }
    else if (p_Tgec->mdioIrq == 0)
        REPORT_ERROR(MINOR, E_NOT_SUPPORTED, (NO_MSG));

    XX_Free(p_TgecDriverParam);
    p_Tgec->p_TgecDriverParam = NULL;

    return E_OK;
}

/* .............................................................................. */

static t_Error TgecFree(t_Handle h_Tgec)
{
    t_Tgec       *p_Tgec = (t_Tgec *)h_Tgec;

    SANITY_CHECK_RETURN_ERROR(p_Tgec, E_INVALID_HANDLE);

    FreeInitResources(p_Tgec);

    if (p_Tgec->p_TgecDriverParam)
    {
        XX_Free(p_Tgec->p_TgecDriverParam);
        p_Tgec->p_TgecDriverParam = NULL;
    }
    XX_Free (p_Tgec);

    return E_OK;
}

/* .............................................................................. */

static void InitFmMacControllerDriver(t_FmMacControllerDriver *p_FmMacControllerDriver)
{
    p_FmMacControllerDriver->f_FM_MAC_Init                      = TgecInit;
    p_FmMacControllerDriver->f_FM_MAC_Free                      = TgecFree;

    p_FmMacControllerDriver->f_FM_MAC_ConfigLoopback            = TgecConfigLoopback;
    p_FmMacControllerDriver->f_FM_MAC_ConfigMaxFrameLength      = TgecConfigMaxFrameLength;

    p_FmMacControllerDriver->f_FM_MAC_ConfigWan                 = TgecConfigWan;

    p_FmMacControllerDriver->f_FM_MAC_ConfigPadAndCrc           = NULL; /* TGEC always works with pad+crc */
    p_FmMacControllerDriver->f_FM_MAC_ConfigHalfDuplex          = NULL; /* half-duplex is not supported in xgec */
    p_FmMacControllerDriver->f_FM_MAC_ConfigLengthCheck         = TgecConfigLengthCheck;
    p_FmMacControllerDriver->f_FM_MAC_ConfigException           = TgecConfigException;

#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
    p_FmMacControllerDriver->f_FM_MAC_ConfigSkipFman11Workaround= TgecConfigSkipFman11Workaround;
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */

    p_FmMacControllerDriver->f_FM_MAC_SetException              = TgecSetExcpetion;

    p_FmMacControllerDriver->f_FM_MAC_Enable1588TimeStamp       = TgecEnable1588TimeStamp;
    p_FmMacControllerDriver->f_FM_MAC_Disable1588TimeStamp      = TgecDisable1588TimeStamp;

    p_FmMacControllerDriver->f_FM_MAC_SetPromiscuous            = TgecSetPromiscuous;
    p_FmMacControllerDriver->f_FM_MAC_AdjustLink                = NULL;

    p_FmMacControllerDriver->f_FM_MAC_Enable                    = TgecEnable;
    p_FmMacControllerDriver->f_FM_MAC_Disable                   = TgecDisable;

    p_FmMacControllerDriver->f_FM_MAC_SetTxAutoPauseFrames      = TgecTxMacPause;
    p_FmMacControllerDriver->f_FM_MAC_SetRxIgnorePauseFrames    = TgecRxIgnoreMacPause;

    p_FmMacControllerDriver->f_FM_MAC_ResetCounters             = TgecResetCounters;
    p_FmMacControllerDriver->f_FM_MAC_GetStatistics             = TgecGetStatistics;

    p_FmMacControllerDriver->f_FM_MAC_ModifyMacAddr             = TgecModifyMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_AddHashMacAddr            = TgecAddHashMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_RemoveHashMacAddr         = TgecDelHashMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_AddExactMatchMacAddr      = TgecAddExactMatchMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_RemovelExactMatchMacAddr  = TgecDelExactMatchMacAddress;
    p_FmMacControllerDriver->f_FM_MAC_GetId                     = TgecGetId;
    p_FmMacControllerDriver->f_FM_MAC_GetVersion                = TgecGetVersion;
    p_FmMacControllerDriver->f_FM_MAC_GetMaxFrameLength         = TgecGetMaxFrameLength;

    p_FmMacControllerDriver->f_FM_MAC_MII_WritePhyReg           = TGEC_MII_WritePhyReg;
    p_FmMacControllerDriver->f_FM_MAC_MII_ReadPhyReg            = TGEC_MII_ReadPhyReg;

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
    p_FmMacControllerDriver->f_FM_MAC_DumpRegs                  = TgecDumpRegs;
#endif /* (defined(DEBUG_ERRORS) && ... */
}


/*****************************************************************************/
/*                      Tgec Config  Main Entry                             */
/*****************************************************************************/

/* .............................................................................. */

t_Handle TGEC_Config(t_FmMacParams *p_FmMacParam)
{
    t_Tgec                  *p_Tgec;
    t_TgecDriverParam       *p_TgecDriverParam;
    uintptr_t               baseAddr;
    uint8_t                 i;

    SANITY_CHECK_RETURN_VALUE(p_FmMacParam, E_NULL_POINTER, NULL);

    baseAddr = p_FmMacParam->baseAddr;
    /* allocate memory for the UCC GETH data structure. */
    p_Tgec = (t_Tgec *) XX_Malloc(sizeof(t_Tgec));
    if (!p_Tgec)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("10G MAC driver structure"));
        return NULL;
    }
    /* Zero out * p_Tgec */
    memset(p_Tgec, 0, sizeof(t_Tgec));
    InitFmMacControllerDriver(&p_Tgec->fmMacControllerDriver);

    /* allocate memory for the 10G MAC driver parameters data structure. */
    p_TgecDriverParam = (t_TgecDriverParam *) XX_Malloc(sizeof(t_TgecDriverParam));
    if (!p_TgecDriverParam)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("10G MAC driver parameters"));
        TgecFree(p_Tgec);
        return NULL;
    }
    /* Zero out */
    memset(p_TgecDriverParam, 0, sizeof(t_TgecDriverParam));

    /* Plant parameter structure pointer */
    p_Tgec->p_TgecDriverParam = p_TgecDriverParam;

    SetDefaultParam(p_TgecDriverParam);

    for (i=0; i < sizeof(p_FmMacParam->addr); i++)
        p_Tgec->addr |= ((uint64_t)p_FmMacParam->addr[i] << ((5-i) * 8));

    p_Tgec->p_MemMap        = (t_TgecMemMap *)UINT_TO_PTR(baseAddr);
    p_Tgec->p_MiiMemMap     = (t_TgecMiiAccessMemMap *)UINT_TO_PTR(baseAddr + TGEC_TO_MII_OFFSET);
    p_Tgec->enetMode        = p_FmMacParam->enetMode;
    p_Tgec->macId           = p_FmMacParam->macId;
    p_Tgec->exceptions      = DEFAULT_exceptions;
    p_Tgec->mdioIrq         = p_FmMacParam->mdioIrq;
    p_Tgec->f_Exception     = p_FmMacParam->f_Exception;
    p_Tgec->f_Event         = p_FmMacParam->f_Event;
    p_Tgec->h_App           = p_FmMacParam->h_App;

    return p_Tgec;
}
