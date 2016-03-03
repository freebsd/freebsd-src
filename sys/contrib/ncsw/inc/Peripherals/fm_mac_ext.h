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
 @File          fm_mac_ext.h

 @Description   FM MAC ...
*//***************************************************************************/
#ifndef __FM_MAC_EXT_H
#define __FM_MAC_EXT_H

#include "std_ext.h"
#include "enet_ext.h"


/**************************************************************************//**

 @Group         FM_grp Frame Manager API

 @Description   FM API functions, definitions and enums

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Group         FM_mac_grp FM MAC

 @Description   FM MAC API functions, definitions and enums

 @{
*//***************************************************************************/


/**************************************************************************//**
 @Description   FM MAC Exceptions
*//***************************************************************************/
typedef enum e_FmMacExceptions {
    e_FM_MAC_EX_10G_MDIO_SCAN_EVENTMDIO = 0
   ,e_FM_MAC_EX_10G_MDIO_CMD_CMPL
   ,e_FM_MAC_EX_10G_REM_FAULT
   ,e_FM_MAC_EX_10G_LOC_FAULT
   ,e_FM_MAC_EX_10G_1TX_ECC_ER
   ,e_FM_MAC_EX_10G_TX_FIFO_UNFL
   ,e_FM_MAC_EX_10G_TX_FIFO_OVFL
   ,e_FM_MAC_EX_10G_TX_ER
   ,e_FM_MAC_EX_10G_RX_FIFO_OVFL
   ,e_FM_MAC_EX_10G_RX_ECC_ER
   ,e_FM_MAC_EX_10G_RX_JAB_FRM
   ,e_FM_MAC_EX_10G_RX_OVRSZ_FRM
   ,e_FM_MAC_EX_10G_RX_RUNT_FRM
   ,e_FM_MAC_EX_10G_RX_FRAG_FRM
   ,e_FM_MAC_EX_10G_RX_LEN_ER
   ,e_FM_MAC_EX_10G_RX_CRC_ER
   ,e_FM_MAC_EX_10G_RX_ALIGN_ER
   ,e_FM_MAC_EX_1G_BAB_RX
   ,e_FM_MAC_EX_1G_RX_CTL
   ,e_FM_MAC_EX_1G_GRATEFUL_TX_STP_COMPLET
   ,e_FM_MAC_EX_1G_BAB_TX
   ,e_FM_MAC_EX_1G_TX_CTL
   ,e_FM_MAC_EX_1G_TX_ERR
   ,e_FM_MAC_EX_1G_LATE_COL
   ,e_FM_MAC_EX_1G_COL_RET_LMT
   ,e_FM_MAC_EX_1G_TX_FIFO_UNDRN
   ,e_FM_MAC_EX_1G_MAG_PCKT
   ,e_FM_MAC_EX_1G_MII_MNG_RD_COMPLET
   ,e_FM_MAC_EX_1G_MII_MNG_WR_COMPLET
   ,e_FM_MAC_EX_1G_GRATEFUL_RX_STP_COMPLET
   ,e_FM_MAC_EX_1G_TX_DATA_ERR
   ,e_FM_MAC_EX_1G_RX_DATA_ERR
   ,e_FM_MAC_EX_1G_1588_TS_RX_ERR
   ,e_FM_MAC_EX_1G_RX_MIB_CNT_OVFL
} e_FmMacExceptions;

/**************************************************************************//**
 @Description   TM MAC statistics level
*//***************************************************************************/
typedef enum e_FmMacStatisticsLevel {
    e_FM_MAC_NONE_STATISTICS = 0,       /**< No statistics */
    e_FM_MAC_PARTIAL_STATISTICS,        /**< Only error counters are available. Optimized for performance */
    e_FM_MAC_FULL_STATISTICS            /**< All counters available. Not optimized for performance */
} e_FmMacStatisticsLevel;


/**************************************************************************//**
 @Function      t_FmMacExceptionCallback

 @Description   Fm Mac Exception Callback from FM MAC to the user

 @Param[in]     h_App             - Handle to the upper layer handler

 @Param[in]     exceptions        - The exception that occurred


 @Return        void.
*//***************************************************************************/
typedef void (t_FmMacExceptionCallback)(t_Handle h_App, e_FmMacExceptions exceptions);


/**************************************************************************//**
 @Description   TM MAC statistics rfc3635
*//***************************************************************************/
typedef struct t_FmMacStatistics {
/* RMON */
    uint64_t  eStatPkts64;             /**< r-10G tr-DT 64 byte frame counter */
    uint64_t  eStatPkts65to127;        /**< r-10G 65 to 127 byte frame counter */
    uint64_t  eStatPkts128to255;       /**< r-10G 128 to 255 byte frame counter */
    uint64_t  eStatPkts256to511;       /**< r-10G 256 to 511 byte frame counter */
    uint64_t  eStatPkts512to1023;      /**< r-10G 512 to 1023 byte frame counter */
    uint64_t  eStatPkts1024to1518;     /**< r-10G 1024 to 1518 byte frame counter */
    uint64_t  eStatPkts1519to1522;     /**< r-10G 1519 to 1522 byte good frame count */
/* */
    uint64_t  eStatFragments;          /**< Total number of packets that were less than 64 octets long with a wrong CRC.*/
    uint64_t  eStatJabbers;            /**< Total number of packets longer than valid maximum length octets */
    uint64_t  eStatsDropEvents;        /**< number of dropped packets due to internal errors of the MAC Client. */
    uint64_t  eStatCRCAlignErrors;     /**< Incremented when frames of correct length but with CRC error are received.*/
    uint64_t  eStatUndersizePkts;      /**< Total number of packets that were less than 64 octets long with a good CRC.*/
    uint64_t  eStatOversizePkts;       /**< T,B.D*/
/* Pause */
    uint64_t  teStatPause;             /**< Pause MAC Control received */
    uint64_t  reStatPause;             /**< Pause MAC Control sent */

/* MIB II */
    uint64_t  ifInOctets;              /**< Total number of byte received. */
    uint64_t  ifInPkts;                /**< Total number of packets received.*/
    uint64_t  ifInMcastPkts;           /**< Total number of multicast frame received*/
    uint64_t  ifInBcastPkts;           /**< Total number of broadcast frame received */
    uint64_t  ifInDiscards;            /**< Frames received, but discarded due to problems within the MAC RX. */
    uint64_t  ifInErrors;              /**< Number of frames received with error:
                                               - FIFO Overflow Error
                                               - CRC Error
                                               - Frame Too Long Error
                                               - Alignment Error
                                               - The dedicated Error Code (0xfe, not a code error) was received */
    uint64_t  ifOutOctets;             /**< Total number of byte sent. */
    uint64_t  ifOutPkts;               /**< Total number of packets sent .*/
    uint64_t  ifOutMcastPkts;          /**< Total number of multicast frame sent */
    uint64_t  ifOutBcastPkts;          /**< Total number of multicast frame sent */
    uint64_t  ifOutDiscards;           /**< Frames received, but discarded due to problems within the MAC TX N/A!.*/
    uint64_t  ifOutErrors;             /**< Number of frames transmitted with error:
                                               - FIFO Overflow Error
                                               - FIFO Underflow Error
                                               - Other */
} t_FmMacStatistics;


/**************************************************************************//**
 @Group         FM_mac_init_grp Initialization Unit

 @Description   FM MAC Initialization Unit

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   FM MAC config input
*//***************************************************************************/
typedef struct t_FmMacParams {
    uintptr_t                   baseAddr;           /**< Base of memory mapped FM MAC registers */
    t_EnetAddr                  addr;               /**< MAC address of device; First octet is sent first */
    uint8_t                     macId;              /**< MAC ID <dTSEC 0-3> <10G 0>         */
    e_EnetMode                  enetMode;           /**< Ethernet operation mode (MAC-PHY interface and speed) */
    t_Handle                    h_Fm;               /**< A handle to the FM object this port related to */
    int                         mdioIrq;            /**< MDIO exceptions interrupt source - not valid for all
                                                         MACs; MUST be set to 'NO_IRQ' for MACs that don't have
                                                         mdio-irq, or for polling */
    t_FmMacExceptionCallback    *f_Event;           /**< MDIO Events Callback Routine         */
    t_FmMacExceptionCallback    *f_Exception;       /**< Exception Callback Routine         */
    t_Handle                    h_App;              /**< A handle to an application layer object; This handle will
                                                         be passed by the driver upon calling the above callbacks */
} t_FmMacParams;


/**************************************************************************//**
 @Function      FM_MAC_Config

 @Description   Creates descriptor for the FM MAC module.

                The routine returns a handle (descriptor) to the FM MAC object.
                This descriptor must be passed as first parameter to all other
                FM MAC function calls.

                No actual initialization or configuration of FM MAC hardware is
                done by this routine.

 @Param[in]     p_FmMacParam   - Pointer to data structure of parameters

 @Retval        Handle to FM MAC object, or NULL for Failure.
*//***************************************************************************/
t_Handle FM_MAC_Config (t_FmMacParams *p_FmMacParam);

/**************************************************************************//**
 @Function      FM_MAC_Init

 @Description   Initializes the FM MAC module

 @Param[in]     h_FmMac - FM module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error  FM_MAC_Init (t_Handle h_FmMac);

/**************************************************************************//**
 @Function      FM_Free

 @Description   Frees all resources that were assigned to FM MAC module.

                Calling this routine invalidates the descriptor.

 @Param[in]     h_FmMac - FM module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error  FM_MAC_Free (t_Handle h_FmMac);


/**************************************************************************//**
 @Group         FM_mac_advanced_init_grp    Advanced Configuration Unit

 @Description   Configuration functions used to change default values.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Function      FM_MAC_ConfigResetOnInit

 @Description   Tell the driver whether to reset the FM MAC before initialization or
                not. It changes the default configuration [FALSE].

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     enable     When TRUE, FM will be reset before any initialization.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigResetOnInit (t_Handle h_FmMac, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_ConfigLoopback

 @Description   Enable/Disable internal loopback mode

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     enable     TRUE to enable or FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigLoopback (t_Handle h_FmMac, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_ConfigMaxFrameLength

 @Description   Setup maximum Frame Length

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     newVal     MAX Frame length

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigMaxFrameLength (t_Handle h_FmMac, uint16_t newVal);

/**************************************************************************//**
 @Function      FM_MAC_ConfigWan

 @Description   ENABLE WAN mode in 10G MAC

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     enable     TRUE to enable or FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigWan (t_Handle h_FmMac, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_ConfigPadAndCrc

 @Description   Config PAD and CRC mode

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     enable     TRUE to enable or FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigPadAndCrc (t_Handle h_FmMac, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_ConfigHalfDuplex

 @Description   Config Half Duplex Mode

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     enable     TRUE to enable or FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigHalfDuplex (t_Handle h_FmMac, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_ConfigLengthCheck

 @Description   Configure thef frame length checking.

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     enable     TRUE to enable or FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigLengthCheck (t_Handle h_FmMac, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_ConfigException

 @Description   Change Exception selection from default

 @Param[in]     h_FmMac         A handle to a FM MAC Module.
 @Param[in]     ex              Type of the desired exceptions
 @Param[in]     enable          TRUE to enable the specified exception, FALSE to disable it.


 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Config() and before FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ConfigException(t_Handle h_FmMac, e_FmMacExceptions ex, bool enable);

#ifdef FM_TX_ECC_FRMS_ERRATA_10GMAC_A004
t_Error FM_MAC_ConfigSkipFman11Workaround (t_Handle h_FmMac);
#endif /* FM_TX_ECC_FRMS_ERRATA_10GMAC_A004 */
/** @} */ /* end of FM_mac_advanced_init_grp group */
/** @} */ /* end of FM_mac_init_grp group */


/**************************************************************************//**
 @Group         FM_mac_runtime_control_grp Runtime Control Unit

 @Description   FM MAC Runtime control unit API functions, definitions and enums.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Function      FM_MAC_Enable

 @Description   Enable the MAC

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     mode       Mode of operation (RX, TX, Both)

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_Enable  (t_Handle h_FmMac,  e_CommMode mode);

/**************************************************************************//**
 @Function      FM_MAC_Disable

 @Description   DISABLE the MAC

 @Param[in]     h_FmMac    A handle to a FM MAC Module.
 @Param[in]     mode       Define what part to Disable (RX,  TX or BOTH)

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_Disable (t_Handle h_FmMac, e_CommMode mode);

/**************************************************************************//**
 @Function      FM_MAC_Enable1588TimeStamp

 @Description   Enables the TSU operation.

 @Param[in]     h_Fm   - Handle to the PTP as returned from the FM_MAC_PtpConfig.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_Enable1588TimeStamp(t_Handle h_Fm);

/**************************************************************************//**
 @Function      FM_MAC_Disable1588TimeStamp

 @Description   Disables the TSU operation.

 @Param[in]     h_Fm   - Handle to the PTP as returned from the FM_MAC_PtpConfig.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_Disable1588TimeStamp(t_Handle h_Fm);

/**************************************************************************//**
 @Function      FM_MAC_SetTxAutoPauseFrames

 @Description   Enable/Disable transmition of Pause-Frames.

 @Param[in]     h_FmMac     A handle to a FM MAC Module.
 @Param[in]     pauseTime   Pause quanta value used with transmitted pause frames.
                            Each quanta represents a 512 bit-times; Note that '0'
                            as an input here will be used as disabling the
                            transmission of the pause-frames.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_SetTxAutoPauseFrames (t_Handle h_FmMac, uint16_t pauseTime);

/**************************************************************************//**
 @Function      FM_MAC_SetRxIgnorePauseFrames

 @Description   Enable/Disable ignoring of Pause-Frames.

 @Param[in]     h_FmMac     A handle to a FM MAC Module.
 @Param[in]     en          boolean indicates whether to ignore the incoming pause
                            frames or not.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_SetRxIgnorePauseFrames (t_Handle h_FmMac, bool en);

/**************************************************************************//**
 @Function      FM_MAC_ResetCounters

 @Description   reset all statistics counters

 @Param[in]     h_FmMac     A handle to a FM MAC Module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ResetCounters (t_Handle h_FmMac);

/**************************************************************************//**
 @Function      FM_MAC_SetException

 @Description   Enable/Disable a specific Exception

 @Param[in]     h_FmMac         A handle to a FM MAC Module.
 @Param[in]     ex              Type of the desired exceptions
 @Param[in]     enable          TRUE to enable the specified exception, FALSE to disable it.


 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_SetException(t_Handle h_FmMac, e_FmMacExceptions ex, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_SetStatistics

 @Description   Define Statistics level.
                                Where applicable, the routine also enables the MIB counters
                                overflow interrupt in order to keep counters accurate
                                and account for overflows.

 @Param[in]     h_FmMac         A handle to a FM MAC Module.
 @Param[in]     statisticsLevel Full statistics level provides all standard counters but may
                                reduce performance. Partial statistics provides only special
                                event counters (errors etc.). If selected, regular counters (such as
                                byte/packet) will be invalid and will return -1.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_SetStatistics (t_Handle h_FmMac, e_FmMacStatisticsLevel statisticsLevel);

/**************************************************************************//**
 @Function      FM_MAC_GetStatistics

 @Description   get all statistics counters

 @Param[in]     h_FmMac         A handle to a FM MAC Module.
 @Param[in]     p_Statistics    Staructure with statistics

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_Init().
*//***************************************************************************/
t_Error FM_MAC_GetStatistics (t_Handle h_FmMac, t_FmMacStatistics *p_Statistics);

/**************************************************************************//**
 @Function      FM_MAC_ModifyMacAddr

 @Description   Replace the main MAC Address

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[in]     p_EnetAddr  -   Ethernet Mac address

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_ModifyMacAddr (t_Handle h_FmMac, t_EnetAddr *p_EnetAddr);

/**************************************************************************//**
 @Function      FM_MAC_AddHashMacAddr

 @Description   Add an Address to the hash table. This is for filter purpose only.

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[in]     p_EnetAddr  -   Ethernet Mac address

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init(). It is a filter only address.
 @Cautions      Some address need to be filterd out in upper FM blocks.
*//***************************************************************************/
t_Error FM_MAC_AddHashMacAddr (t_Handle h_FmMac, t_EnetAddr *p_EnetAddr);

/**************************************************************************//**
 @Function      FM_MAC_RemoveHashMacAddr

 @Description   Delete an Address to the hash table. This is for filter purpose only.

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[in]     p_EnetAddr  -   Ethernet Mac address

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_RemoveHashMacAddr (t_Handle h_FmMac, t_EnetAddr *p_EnetAddr);

/**************************************************************************//**
 @Function      FM_MAC_AddExactMatchMacAddr

 @Description   Add a unicast or multicast mac address for exact-match filtering
                (8 on dTSEC, 2 for 10G-MAC)

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[in]     p_EnetAddr  -   MAC Address to ADD

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_AddExactMatchMacAddr (t_Handle h_FmMac, t_EnetAddr *p_EnetAddr);

/**************************************************************************//**
 @Function      FM_MAC_RemovelExactMatchMacAddr

 @Description   Remove a uni cast or multi cast mac address.

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[in]     p_EnetAddr  -   MAC Address to remove

 @Return        E_OK on success; Error code otherwise..

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_RemovelExactMatchMacAddr (t_Handle h_FmMac, t_EnetAddr *p_EnetAddr);

/**************************************************************************//**
 @Function      FM_MAC_SetPromiscuous

 @Description   Enable/Disable MAC Promiscuous mode for ALL mac addresses.

 @Param[in]     h_FmMac    - A handle to a FM MAC Module.
 @Param[in]     enable     - TRUE to enable or FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_SetPromiscuous (t_Handle h_FmMac, bool enable);

/**************************************************************************//**
 @Function      FM_MAC_AdjustLink

 @Description   Adjusts the Ethernet link with new speed/duplex setup.

 @Param[in]     h_FmMac     - A handle to a FM Module.
 @Param[in]     speed       - Ethernet speed.
 @Param[in]     fullDuplex  - TRUE for Full-Duplex mode;
                              FALSE for Half-Duplex mode.

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_MAC_AdjustLink(t_Handle h_FmMac, e_EnetSpeed speed, bool fullDuplex);

/**************************************************************************//**
 @Function      FM_MAC_GetId

 @Description   Return the MAC ID

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[out]    p_MacId     -   MAC ID of device

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_GetId (t_Handle h_FmMac, uint32_t *p_MacId);

/**************************************************************************//**
 @Function      FM_MAC_GetVesrion

 @Description   Return Mac HW chip version

 @Param[in]     h_FmMac      -   A handle to a FM Module.
 @Param[out]    p_MacVresion -   Mac version as defined by the chip

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_GetVesrion (t_Handle h_FmMac, uint32_t *p_MacVresion);

/**************************************************************************//**
 @Function      FM_MAC_MII_WritePhyReg

 @Description   Write data into Phy Register

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[in]     phyAddr     -   Phy Address on the MII bus
 @Param[in]     reg         -   Register Number.
 @Param[in]     data        -   Data to write.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_MII_WritePhyReg (t_Handle h_FmMac, uint8_t phyAddr, uint8_t reg, uint16_t data);

/**************************************************************************//**
 @Function      FM_MAC_MII_ReadPhyReg

 @Description   Read data from Phy Register

 @Param[in]     h_FmMac     -   A handle to a FM Module.
 @Param[in]     phyAddr     -   Phy Address on the MII bus
 @Param[in]     reg         -   Register Number.
 @Param[out]    p_Data      -   Data from PHY.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_MII_ReadPhyReg(t_Handle h_FmMac,  uint8_t phyAddr, uint8_t reg, uint16_t *p_Data);

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
/**************************************************************************//**
 @Function      FM_MAC_DumpRegs

 @Description   Dump internal registers

 @Param[in]     h_FmMac     -   A handle to a FM Module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only after FM_MAC_Init().
*//***************************************************************************/
t_Error FM_MAC_DumpRegs(t_Handle h_FmMac);
#endif /* (defined(DEBUG_ERRORS) && ... */

/** @} */ /* end of FM_mac_runtime_control_grp group */
/** @} */ /* end of FM_mac_grp group */
/** @} */ /* end of FM_grp group */



#endif /* __FM_MAC_EXT_H */
