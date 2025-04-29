/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_qat.h
 *
 * @defgroup LacSymQat  Symmetric QAT
 *
 * @ingroup LacSym
 *
 * Interfaces for populating the qat structures for a symmetric operation
 *
 * @lld_start
 *
 * @lld_overview
 * This file documents the interfaces for populating the qat structures
 * that are common for all symmetric operations.
 *
 * @lld_dependencies
 * - \ref LacSymQatHash "Hash QAT Comms" Sym Qat commons for Hash
 * - \ref LacSymQat_Cipher "Cipher QAT Comms" Sym Qat commons for Cipher
 * - OSAL: logging
 * - \ref LacMem "Memory" - Inline memory functions
 *
 * @lld_initialisation
 * This component is initialized during the LAC initialisation sequence. It
 * is called by the Symmetric Initialisation function.
 *
 * @lld_module_algorithms
 *
 * @lld_process_context
 * Refer to \ref LacHash "Hash" and \ref LacCipher "Cipher" for sequence
 * diagrams to see their interactions with this code.
 *
 *
 * @lld_end
 *
 *****************************************************************************/

/*****************************************************************************/

#ifndef LAC_SYM_QAT_H
#define LAC_SYM_QAT_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "icp_accel_devices.h"
#include "icp_qat_fw_la.h"
#include "icp_qat_hw.h"
#include "lac_session.h"
#include "sal_qat_cmn_msg.h"
#include "lac_common.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

#define LAC_SYM_DEFAULT_QAT_PTR_TYPE QAT_COMN_PTR_TYPE_SGL
#define LAC_SYM_DP_QAT_PTR_TYPE QAT_COMN_PTR_TYPE_FLAT
#define LAC_SYM_KEY_QAT_PTR_TYPE QAT_COMN_PTR_TYPE_FLAT
/**< @ingroup LacSymQat
 * LAC SYM Source & Destination buffer type (FLAT/SGL) */

#define LAC_QAT_SYM_REQ_SZ_LW 32
#define SYM_TX_MSG_SIZE (LAC_QAT_SYM_REQ_SZ_LW * LAC_LONG_WORD_IN_BYTES)
#define NRBG_TX_MSG_SIZE (LAC_QAT_SYM_REQ_SZ_LW * LAC_LONG_WORD_IN_BYTES)

#define LAC_QAT_SYM_RESP_SZ_LW 8
#define SYM_RX_MSG_SIZE (LAC_QAT_SYM_RESP_SZ_LW * LAC_LONG_WORD_IN_BYTES)
#define NRBG_RX_MSG_SIZE (LAC_QAT_SYM_RESP_SZ_LW * LAC_LONG_WORD_IN_BYTES)

/**
 *******************************************************************************
 * @ingroup LacSymQat
 *      Symmetric crypto response handler
 *
 * @description
 *      This function handles the symmetric crypto response
 *
 * @param[in] trans_handle          transport handle (if ICP_QAT_DBG set)
 * @param[in] instanceHandle        void* pRespMsg
 *
 *
 *****************************************************************************/
void LacSymQat_SymRespHandler(void *pRespMsg);

/**
 *******************************************************************************
 * @ingroup LacSymQat
 *      Initialise the Symmetric QAT code
 *
 * @description
 *      This function initialises the symmetric QAT code
 *
 * @param[in] device                Pointer to the acceleration device
 *                                  structure
 * @param[in] instanceHandle        Instance handle
 * @param[in] numSymRequests        Number of concurrent requests a pair
 *                                  (tx and rx) need to support
 *
 * @return CPA_STATUS_SUCCESS       Operation successful
 * @return CPA_STATUS_FAIL          Initialisation Failed
 *
 *****************************************************************************/
CpaStatus LacSymQat_Init(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacSymQat
 *      Register a response handler function for a symmetric command ID
 *
 * @description
 *      This function registers a response handler function for a symmetric
 *      operation.
 *
 *      Note: This operation should only be performed once by the init function
 *      of a component. There is no corresponding deregister function, but
 *      registering a NULL function pointer will have the same effect. There
 *      MUST not be any requests in flight when calling this function.
 *
 * @param[in] lacCmdId          Command Id of operation
 * @param[in] pCbHandler        callback handler function
 *
 * @return None
 *
 *****************************************************************************/
void LacSymQat_RespHandlerRegister(icp_qat_fw_la_cmd_id_t lacCmdId,
				   sal_qat_resp_handler_func_t pCbHandler);

/**
 ******************************************************************************
 * @ingroup LacSymQat
 *      get the QAT packet type
 *
 * @description
 *      This function returns the QAT packet type for a LAC packet type. The
 *      LAC packet type does not indicate a first partial. therefore for a
 *      partial request, the previous packet type needs to be looked at to
 *      figure out if the current partial request is a first partial.
 *
 *
 * @param[in] packetType          LAC Packet type
 * @param[in] packetState         LAC Previous Packet state
 * @param[out] pQatPacketType     Packet type using the QAT macros
 *
 * @return none
 *
 *****************************************************************************/
void LacSymQat_packetTypeGet(CpaCySymPacketType packetType,
			     CpaCySymPacketType packetState,
			     Cpa32U *pQatPacketType);

/**
 ******************************************************************************
 * @ingroup LacSymQat
 *      Populate the command flags based on the packet type
 *
 * @description
 *      This function populates the following flags in the Symmetric Crypto
 *      service_specif_flags field of the common header of the request:
 *          - LA_PARTIAL
 *          - UPDATE_STATE
 *          - RET_AUTH_RES
 *          - CMP_AUTH_RES
 *          based on looking at the input params listed below.
 *
 * @param[in] qatPacketType          Packet type
 * @param[in] cmdId                  Command Id
 * @param[in] cipherAlgorithm        Cipher Algorithm
 * @param[out] pLaCommandFlags       Command Flags
 *
 * @return none
 *
 *****************************************************************************/
void LacSymQat_LaPacketCommandFlagSet(Cpa32U qatPacketType,
				      icp_qat_fw_la_cmd_id_t laCmdId,
				      CpaCySymCipherAlgorithm cipherAlgorithm,
				      Cpa16U *pLaCommandFlags,
				      Cpa32U ivLenInBytes);

/**
 ******************************************************************************
 * @ingroup LacSymQat
 *
 *
 * @description
 *             defaults the common request service specific flags
 *
 * @param[in] laCmdFlags          Common request service specific flags
 * @param[in] symOp               Type of operation performed e.g hash or cipher
 *
 * @return none
 *
 *****************************************************************************/

void LacSymQat_LaSetDefaultFlags(icp_qat_fw_serv_specif_flags *laCmdFlags,
				 CpaCySymOp symOp);

/**
 ******************************************************************************
 * @ingroup LacSymQat
 *
 *
 * @description
 *      this function defines whether the shared constants table can be used
 *      for a particular cipher and hash algorithm
 *
 * @param[in]   ptr to session

 * @param[in]   ptr to return offset into table for cipher config

 * @param[in]   ptr to return offset into table for hash config
 *
 * @return CPA_TRUE if Constants table is available for use, CPA_FALSE if it's
 *         not.
 *
 *****************************************************************************/
CpaBoolean LacSymQat_UseSymConstantsTable(lac_session_desc_t *pSession,
					  Cpa8U *cipherOffset,
					  Cpa8U *hashOffset);

/**
 ******************************************************************************
 * @ingroup LacSymQat
 *
 *
 * @description
 *      this function calculates whether the optimized content descriptor can
 *      be used for a particular chained cipher and hash algorithm
 *
 * @param[in]   ptr to session
 *
 * @return CPA_TRUE if optimized CD can be used, CPA_FALSE if it's not.
 *
 *****************************************************************************/
CpaBoolean LacSymQat_UseOptimisedContentDesc(lac_session_desc_t *pSession);

#endif /* LAC_SYM_QAT_H */
