/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_qat_cipher.h
 *
 * @defgroup LacSymQat_Cipher  Cipher QAT
 *
 * @ingroup LacSymQat
 *
 * external interfaces for populating QAT structures for cipher operations.
 *
 *****************************************************************************/

/*****************************************************************************/

#ifndef LAC_SYM_QAT_CIPHER_H
#define LAC_SYM_QAT_CIPHER_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa_cy_sym.h"
#include "icp_qat_fw_la.h"
#include "lac_session.h"
#include "lac_sal_types_crypto.h"

/*
 **************************************************************************
 * @ingroup LacSymQat_Cipher
 *
 * @description
 * Defines for building the cipher request params cache
 *
 ************************************************************************** */

#define LAC_SYM_QAT_CIPHER_NEXT_ID_BIT_OFFSET 24
#define LAC_SYM_QAT_CIPHER_CURR_ID_BIT_OFFSET 16
#define LAC_SYM_QAT_CIPHER_STATE_SIZE_BIT_OFFSET 8
#define LAC_SYM_QAT_CIPHER_GCM_SPC_OFFSET_IN_DRAM 9
#define LAC_SYM_QAT_CIPHER_CCM_SPC_OFFSET_IN_DRAM 8
#define LAC_SYM_QAT_CIPHER_CHACHA_SPC_OFFSET_IN_DRAM 2
#define LAC_SYM_QAT_CIPHER_SPC_STATE_SIZE 48

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      Retrieve the cipher block size in bytes for a given algorithm
 *
 * @description
 *      This function returns a hard-coded block size for the specific cipher
 *      algorithm
 *
 * @param[in] cipherAlgorithm   Cipher algorithm for the current session
 *
 * @retval The block size, in bytes, for the given cipher algorithm
 *
 *****************************************************************************/
Cpa8U
LacSymQat_CipherBlockSizeBytesGet(CpaCySymCipherAlgorithm cipherAlgorithm);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      Retrieve the cipher IV/state size in bytes for a given algorithm
 *
 * @description
 *      This function returns a hard-coded IV/state size for the specific cipher
 *      algorithm
 *
 * @param[in] cipherAlgorithm   Cipher algorithm for the current session
 *
 * @retval The IV/state size, in bytes, for the given cipher algorithm
 *
 *****************************************************************************/
Cpa32U LacSymQat_CipherIvSizeBytesGet(CpaCySymCipherAlgorithm cipherAlgorithm);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      Populate the cipher request params structure
 *
 * @description
 *      This function is passed a pointer to the 128B request block.
 *      (This memory must be allocated prior to calling this function). It
 *      populates:
 *        - the cipher fields of the req_params block in the request. No
 *          need to zero this first, all fields will be populated.
 *        - the corresponding CIPH_IV_FLD flag in the serv_specif_flags field
 *          of the common header.
 *      To do this it uses the parameters described below and the following
 *fields from the request block which must be populated prior to calling this
 *function:
 *        - cd_ctrl.cipher_state_sz
 *        - UPDATE_STATE flag in comn_hdr.serv_specif_flags
 *
 *
 * @param[in] pReq                  Pointer to request block.
 * *
 * @param[in] cipherOffsetInBytes   Offset to cipher data in user data buffer
 *
 * @param[in] cipherLenInBytes      Length of cipher data in buffer
 *
 * @param[in] ivBufferPhysAddr      Physical address of aligned IV/state
 *                                   buffer
 * @param[in] pIvBufferVirt         Virtual address of aligned IV/state
 *                                   buffer
 * @retval void
 *
 *****************************************************************************/
CpaStatus
LacSymQat_CipherRequestParamsPopulate(lac_session_desc_t *pSessionDesc,
				      icp_qat_fw_la_bulk_req_t *pReq,
				      Cpa32U cipherOffsetInBytes,
				      Cpa32U cipherLenInBytes,
				      Cpa64U ivBufferPhysAddr,
				      Cpa8U *pIvBufferVirt);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *       Derive initial ARC4 cipher state from a base key
 *
 * @description
 *       An initial state for an ARC4 cipher session is derived from the base
 *       key provided by the user, using the ARC4 Key Scheduling Algorithm (KSA)
 *
 * @param[in] pKey              The base key provided by the user
 *
 * @param[in] keyLenInBytes     The length of the base key provided.
 *                              The range of valid values is 1-256 bytes
 *
 * @param[out] pArc4CipherState The initial state is written to this buffer,
 *                              including i and j values, and 6 bytes of padding
 *                              so 264 bytes must be allocated for this buffer
 *                              by the caller
 *
 * @retval void
 *
 *****************************************************************************/
void LacSymQat_CipherArc4StateInit(const Cpa8U *pKey,
				   Cpa32U keyLenInBytes,
				   Cpa8U *pArc4CipherState);

/**
 ******************************************************************************
 * @ingroup LacSymQat_CipherXTSModeUpdateKeyLen
 *       Update the initial XTS key after the first partial has been received.
 *
 * @description
 *       For XTS mode using partial packets, after the first partial response
 *       has been received, the key length needs to be halved for subsequent
 *       partials.
 *
 * @param[in] pSessionDesc      The session descriptor.
 *
 * @param[in] newKeySizeInBytes The new key size..
 *
 * @retval void
 *
 *****************************************************************************/
void LacSymQat_CipherXTSModeUpdateKeyLen(lac_session_desc_t *pSessionDesc,
					 Cpa32U newKeySizeInBytes);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      LacSymQat_CipherCtrlBlockInitialize()
 *
 * @description
 *      intialize the cipher control block with all zeros
 *
 * @param[in]  pMsg                     Pointer to the common request message
 *
 * @retval void
 *
 *****************************************************************************/
void LacSymQat_CipherCtrlBlockInitialize(icp_qat_fw_la_bulk_req_t *pMsg);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      LacSymQat_CipherCtrlBlockWrite()
 *
 * @description
 *      This function populates the cipher control block of the common request
 *      message
 *
 * @param[in]  pMsg                      Pointer to the common request message
 *
 * @param[in] cipherAlgorithm            Cipher Algorithm to be used
 *
 * @param[in] targetKeyLenInBytes        cipher key length in bytes of selected
 *                                       algorithm
 *
 * @param[in] sliceType                  Cipher slice type to be used
 *
 * @param[out] nextSlice                 SliceID for next control block
 *                                       entry.  This value is known only by
 *                                       the calling component
 *
 * @param[out] cipherCfgOffsetInQuadWord Offset into the config table in QW
 *
 * @retval void
 *
 *****************************************************************************/
void LacSymQat_CipherCtrlBlockWrite(icp_qat_la_bulk_req_ftr_t *pMsg,
				    Cpa32U cipherAlgorithm,
				    Cpa32U targetKeyLenInBytes,
				    Cpa32U sliceType,
				    icp_qat_fw_slice_t nextSlice,
				    Cpa8U cipherCfgOffsetInQuadWord);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      LacSymQat_CipherHwBlockPopulateCfgData()
 *
 * @description
 *      Populate the physical HW block with config data
 *
 * @param[in]  pSession                Pointer to the session data
 *
 * @param[in] pCipherHwBlock           pointer to the hardware control block
 *                                     in the common message
 *
 * @param[in] pSizeInBytes
 *
 * @retval void
 *
 *****************************************************************************/
void LacSymQat_CipherHwBlockPopulateCfgData(lac_session_desc_t *pSession,
					    const void *pCipherHwBlock,
					    Cpa32U *pSizeInBytes);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      LacSymQat_CipherGetCfgData()
 *
 * @description
 *      setup the config data for cipher
 *
 * @param[in]  pSession                Pointer to the session data
 *
 * @param[in] pAlgorithm           *
 * @param[in] pMode
 * @param[in] pDir
 * @param[in] pKey_convert
 *
 * @retval void
 *
 *****************************************************************************/
void LacSymQat_CipherGetCfgData(lac_session_desc_t *pSession,
				icp_qat_hw_cipher_algo_t *pAlgorithm,
				icp_qat_hw_cipher_mode_t *pMode,
				icp_qat_hw_cipher_dir_t *pDir,
				icp_qat_hw_cipher_convert_t *pKey_convert);

/**
 ******************************************************************************
 * @ingroup LacSymQat_Cipher
 *      LacSymQat_CipherHwBlockPopulateKeySetup()
 *
 * @description
 *      populate the key setup data in the cipher hardware control block
 *      in the common request message
 *
 * param[in] pCipherSetupData          Pointer to cipher setup data
 *
 * @param[in] targetKeyLenInBytes       Target key length.  If key length given
 *                                      in cipher setup data is less that this,
 *                                      the key will be "rounded up" to this
 *                                      target length by padding it with 0's.
 *                                      In normal no-padding case, the target
 *                                      key length MUST match the key length
 *                                      in the cipher setup data.
 *
 * @param[in] sliceType                 Cipher slice type to be used
 *
 * @param[in] pCipherHwBlock            Pointer to the cipher hardware block
 *
 * @param[out] pCipherHwBlockSizeBytes  Size in bytes of cipher setup block
 *
 *
 * @retval void
 *
 *****************************************************************************/
void LacSymQat_CipherHwBlockPopulateKeySetup(
    lac_session_desc_t *pSessionDesc,
    const CpaCySymCipherSetupData *pCipherSetupData,
    Cpa32U targetKeyLenInBytes,
    Cpa32U sliceType,
    const void *pCipherHwBlock,
    Cpa32U *pCipherHwBlockSizeBytes);

#endif /* LAC_SYM_QAT_CIPHER_H */
