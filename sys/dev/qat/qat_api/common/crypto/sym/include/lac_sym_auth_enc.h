/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_auth_enc.h
 *
 * @defgroup LacAuthEnc Authenticated Encryption
 *
 * @ingroup LacSym
 *
 * @description
 *  Authenticated encryption specific functionality.
 *  For CCM related code NIST SP 800-38C is followed.
 *  For GCM related code NIST SP 800-38D is followed.
 *
 ***************************************************************************/
#ifndef LAC_SYM_AUTH_ENC_H_
#define LAC_SYM_AUTH_ENC_H_

/* This define for CCM describes constant sum of n and q */
#define LAC_ALG_CHAIN_CCM_NQ_CONST 15

/* These defines for CCM describe maximum and minimum
 * length of nonce in bytes*/
#define LAC_ALG_CHAIN_CCM_N_LEN_IN_BYTES_MAX 13
#define LAC_ALG_CHAIN_CCM_N_LEN_IN_BYTES_MIN 7

/**
 * @ingroup LacAuthEnc
 * This function applies any necessary padding to additional authentication data
 * pointed by pAdditionalAuthData field of pOpData as described in
 * NIST SP 800-38D
 *
 * @param[in] pSessionDesc              Pointer to the session descriptor
 * @param[in,out] pAdditionalAuthData   Pointer to AAD
 *
 * @retval CPA_STATUS_SUCCESS          Operation finished successfully
 *
 * @pre pAdditionalAuthData has been param checked
 *
 */
void LacSymAlgChain_PrepareGCMData(lac_session_desc_t *pSessionDesc,
				   Cpa8U *pAdditionalAuthData);

/**
 * @ingroup LacAuthEnc
 * This function prepares param checks iv and aad for CCM
 *
 * @param[in,out] pAdditionalAuthData   Pointer to AAD
 * @param[in,out] pIv                   Pointer to IV
 * @param[in] messageLenToCipherInBytes Size of the message to cipher
 * @param[in] ivLenInBytes              Size of the IV
 *
 * @retval CPA_STATUS_SUCCESS          Operation finished successfully
 * @retval CPA_STATUS_INVALID_PARAM    Invalid parameter passed
 *
 */
CpaStatus LacSymAlgChain_CheckCCMData(Cpa8U *pAdditionalAuthData,
				      Cpa8U *pIv,
				      Cpa32U messageLenToCipherInBytes,
				      Cpa32U ivLenInBytes);

/**
 * @ingroup LacAuthEnc
 * This function prepares Ctr0 and B0-Bn blocks for CCM algorithm as described
 * in NIST SP 800-38C. Ctr0 block is placed in pIv field of pOpData and B0-BN
 * blocks are placed in pAdditionalAuthData.
 *
 * @param[in] pSessionDesc              Pointer to the session descriptor
 * @param[in,out] pAdditionalAuthData   Pointer to AAD
 * @param[in,out] pIv                   Pointer to IV
 * @param[in] messageLenToCipherInBytes Size of the message to cipher
 * @param[in] ivLenInBytes              Size of the IV
 *
 * @retval none
 *
 * @pre parameters have been checked using LacSymAlgChain_CheckCCMData()
 */
void LacSymAlgChain_PrepareCCMData(lac_session_desc_t *pSessionDesc,
				   Cpa8U *pAdditionalAuthData,
				   Cpa8U *pIv,
				   Cpa32U messageLenToCipherInBytes,
				   Cpa32U ivLenInBytes);

#endif /* LAC_SYM_AUTH_ENC_H_ */
