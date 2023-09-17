/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_qat_key.h
 *
 * @defgroup LacSymQatKey  Key QAT
 *
 * @ingroup LacSymQat
 *
 * interfaces for populating qat structures for a key operation
 *
 *****************************************************************************/

#ifndef LAC_SYM_QAT_KEY_H
#define LAC_SYM_QAT_KEY_H

#include "cpa.h"
#include "lac_sym.h"
#include "icp_qat_fw_la.h"

/**
******************************************************************************
* @ingroup LacSymQatKey
*      Number of bytes generated per iteration
* @description
*      This define is the number of bytes generated per iteration
*****************************************************************************/
#define LAC_SYM_QAT_KEY_SSL_BYTES_PER_ITERATION (16)

/**
******************************************************************************
* @ingroup LacSymQatKey
*      Shift to calculate the number of iterations
* @description
*      This define is the shift to calculate the number of iterations
*****************************************************************************/
#define LAC_SYM_QAT_KEY_SSL_ITERATIONS_SHIFT LAC_16BYTE_ALIGNMENT_SHIFT

/**
*******************************************************************************
* @ingroup LacSymKey
*      Populate the SSL request
*
* @description
*      Populate the SSL request
*
* @param[out] pKeyGenReqHdr            Pointer to Key Generation request Header
* @param[out] pKeyGenReqMid            Pointer to LW's 14/15 of Key Gen request
* @param[in] generatedKeyLenInBytes    Length of Key generated
* @param[in] labelLenInBytes           Length of Label
* @param[in] secretLenInBytes          Length of Secret
* @param[in] iterations                Number of iterations. This is related
*                                      to the label length.
*
* @return None
*
*****************************************************************************/
void
LacSymQat_KeySslRequestPopulate(icp_qat_la_bulk_req_hdr_t *pKeyGenReqHdr,
				icp_qat_fw_la_key_gen_common_t *pKeyGenReqMid,
				Cpa32U generatedKeyLenInBytes,
				Cpa32U labelLenInBytes,
				Cpa32U secretLenInBytes,
				Cpa32U iterations);

/**
*******************************************************************************
* @ingroup LacSymKey
*      Populate the TLS request
*
* @description
*      Populate the TLS request
*
* @param[out] pKeyGenReq               Pointer to Key Generation request
* @param[in] generatedKeyLenInBytes    Length of Key generated
* @param[in] labelLenInBytes           Length of Label
* @param[in] secretLenInBytes          Length of Secret
* @param[in] seedLenInBytes            Length of Seed
* @param[in] cmdId                     Command Id to differentiate TLS versions
*
* @return None
*
*****************************************************************************/
void LacSymQat_KeyTlsRequestPopulate(
    icp_qat_fw_la_key_gen_common_t *pKeyGenReqParams,
    Cpa32U generatedKeyLenInBytes,
    Cpa32U labelLenInBytes,
    Cpa32U secretLenInBytes,
    Cpa8U seedLenInBytes,
    icp_qat_fw_la_cmd_id_t cmdId);

/**
*******************************************************************************
* @ingroup LacSymKey
*      Populate MGF request
*
* @description
*      Populate MGF request
*
* @param[out] pKeyGenReqHdr      Pointer to Key Generation request Header
* @param[out] pKeyGenReqMid      Pointer to LW's 14/15 of Key Gen request
* @param[in] seedLenInBytes      Length of Seed
* @param[in] maskLenInBytes      Length of Mask
* @param[in] hashLenInBytes      Length of hash
*
* @return None
*
*****************************************************************************/
void
LacSymQat_KeyMgfRequestPopulate(icp_qat_la_bulk_req_hdr_t *pKeyGenReqHdr,
				icp_qat_fw_la_key_gen_common_t *pKeyGenReqMid,
				Cpa8U seedLenInBytes,
				Cpa16U maskLenInBytes,
				Cpa8U hashLenInBytes);

/**
*******************************************************************************
* @ingroup LacSymKey
*      Populate the SSL key material input
*
* @description
*      Populate the SSL key material input
*
* @param[in] pService                  Pointer to service
* @param[out] pSslKeyMaterialInput     Pointer to SSL key material input
* @param[in] pSeed                     Pointer to Seed
* @param[in] labelPhysAddr             Physical address of the label
* @param[in] pSecret                   Pointer to Secret
*
* @return None
*
*****************************************************************************/
void LacSymQat_KeySslKeyMaterialInputPopulate(
    sal_service_t *pService,
    icp_qat_fw_la_ssl_key_material_input_t *pSslKeyMaterialInput,
    void *pSeed,
    Cpa64U labelPhysAddr,
    void *pSecret);

/**
*******************************************************************************
* @ingroup LacSymKey
*      Populate the TLS key material input
*
* @description
*      Populate the TLS key material input
*
* @param[in] pService                  Pointer to service
* @param[out] pTlsKeyMaterialInput   Pointer to TLS key material input
* @param[in] pSeed                   Pointer to Seed
* @param[in] labelPhysAddr             Physical address of the label
*
* @return None
*
*****************************************************************************/
void LacSymQat_KeyTlsKeyMaterialInputPopulate(
    sal_service_t *pService,
    icp_qat_fw_la_tls_key_material_input_t *pTlsKeyMaterialInput,
    void *pSeed,
    Cpa64U labelPhysAddr);

/**
*******************************************************************************
* @ingroup LacSymKey
*      Populate the TLS HKDF key material input
*
* @description
*      Populate the TLS HKDF key material input
*
* @param[in] pService                  Pointer to service
* @param[out] pTlsKeyMaterialInput     Pointer to TLS key material input
* @param[in] pSeed                     Pointer to Seed
* @param[in] labelPhysAddr             Physical address of the label
* @param[in] cmdId                     Command ID
*
* @return None
*
*****************************************************************************/
void LacSymQat_KeyTlsHKDFKeyMaterialInputPopulate(
    sal_service_t *pService,
    icp_qat_fw_la_hkdf_key_material_input_t *pTlsKeyMaterialInput,
    CpaCyKeyGenHKDFOpData *pKeyGenTlsOpData,
    Cpa64U subLabelsPhysAddr,
    icp_qat_fw_la_cmd_id_t cmdId);

#endif /* LAC_SYM_QAT_KEY_H */
