/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 *****************************************************************************
 * @file lac_sym_qat_hash.h
 *
 * @defgroup LacSymQatHash  Hash QAT
 *
 * @ingroup LacSymQat
 *
 * interfaces for populating qat structures for a hash operation
 *
 *****************************************************************************/

/*****************************************************************************/

#ifndef LAC_SYM_QAT_HASH_H
#define LAC_SYM_QAT_HASH_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "icp_qat_fw_la.h"
#include "icp_qat_hw.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "lac_common.h"

/**
 ******************************************************************************
 * @ingroup LacSymQatHash
 *      hash precomputes
 *
 * @description
 *      This structure contains infomation on the hash precomputes
 *
 *****************************************************************************/
typedef struct lac_sym_qat_hash_precompute_info_s {
	Cpa8U *pState1;
	/**< state1 pointer */
	Cpa32U state1Size;
	/**< state1 size */
	Cpa8U *pState2;
	/**< state2 pointer */
	Cpa32U state2Size;
	/**< state2 size */
} lac_sym_qat_hash_precompute_info_t;

/**
 ******************************************************************************
 * @ingroup LacSymQatHash
 *      hash state prefix buffer info
 *
 * @description
 *      This structure contains infomation on the hash state prefix aad buffer
 *
 *****************************************************************************/
typedef struct lac_sym_qat_hash_state_buffer_info_s {
	Cpa64U pDataPhys;
	/**< Physical pointer to the hash state prefix buffer */
	Cpa8U *pData;
	/**< Virtual pointer to the hash state prefix buffer */
	Cpa8U stateStorageSzQuadWords;
	/**< hash state storage size in quad words */
	Cpa8U prefixAadSzQuadWords;
	/**< inner prefix/aad and outer prefix size in quad words */
} lac_sym_qat_hash_state_buffer_info_t;

/**
 ******************************************************************************
 * @ingroup LacSymQatHash
 *      Init the hash specific part of the content descriptor.
 *
 * @description
 *      This function populates the hash specific fields of the control block
 *      and the hardware setup block for a digest session. This function sets
 *      the size param to hold the size of the hash setup block.
 *
 *      In the case of hash only, the content descriptor will contain just a
 *      hash control block and hash setup block. In the case of chaining it
 *      will contain the hash control block and setup block along with the
 *      control block and setup blocks of additional services.
 *
 *      Note: The memory for the content descriptor MUST be allocated prior to
 *      calling this function. The memory for the hash control block and hash
 *      setup block MUST be set to 0 prior to calling this function.
 *
 * @image html contentDescriptor.png "Content Descriptor"
 *
 * @param[in] pMsg                      Pointer to req Parameter Footer
 *
 * @param[in] pHashSetupData            Pointer to the hash setup data as
 *                                      defined in the LAC API.
 *
 * @param[in] pHwBlockBase              Pointer to the base of the hardware
 *                                      setup block
 *
 * @param[in] hashBlkOffsetInHwBlock    Offset in quad-words from the base of
 *                                      the hardware setup block where the
 *                                      hash block will start. This offset
 *                                      is stored in the control block. It
 *                                      is used to figure out where to write
 *                                      that hash setup block.
 *
 * @param[in] nextSlice                 SliceID for next control block
 *                                      entry This value is known only by
 *                                      the calling component
 *
 * @param[in] qatHashMode               QAT hash mode
 *
 * @param[in] useSymConstantsTable      Indicate if Shared-SRAM constants table
 *                                      is used for this session. If TRUE, the
 *                                      h/w setup block is NOT populated
 *
 * @param[in] useOptimisedContentDesc   Indicate if optimised content desc
 *                                      is used for this session.
 *
 * @param[in] pPrecompute               For auth mode, this is the pointer
 *                                      to the precompute data. Otherwise this
 *                                      should be set to NULL
 *
 * @param[out] pHashBlkSizeInBytes      size in bytes of hash setup block
 *
 * @return void
 *
 *****************************************************************************/
void
LacSymQat_HashContentDescInit(icp_qat_la_bulk_req_ftr_t *pMsg,
			      CpaInstanceHandle instanceHandle,
			      const CpaCySymHashSetupData *pHashSetupData,
			      void *pHwBlockBase,
			      Cpa32U hashBlkOffsetInHwBlock,
			      icp_qat_fw_slice_t nextSlice,
			      icp_qat_hw_auth_mode_t qatHashMode,
			      CpaBoolean useSymConstantsTable,
			      CpaBoolean useOptimisedContentDesc,
			      lac_sym_qat_hash_precompute_info_t *pPrecompute,
			      Cpa32U *pHashBlkSizeInBytes);

/**
 ******************************************************************************
 * @ingroup LacSymQatHash
 *      Calculate the size of the hash state prefix aad buffer
 *
 * @description
 *      This function inspects the hash control block and based on the values
 *      in the fields, it calculates the size of the hash state prefix aad
 *      buffer.
 *
 *      A partial packet processing request is possible at any stage during a
 *      hash session. In this case, there will always be space for the hash
 *      state storage field of the hash state prefix buffer. When there is
 *      AAD data just the inner prefix AAD data field is used.
 *
 * @param[in]  pMsg                 Pointer to the Request Message
 *
 * @param[out] pHashStateBuf        Pointer to hash state prefix buffer info
 *                                  structure.
 *
 * @return None
 *
 *****************************************************************************/
void LacSymQat_HashStatePrefixAadBufferSizeGet(
    icp_qat_la_bulk_req_ftr_t *pMsg,
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBuf);

/**
 ******************************************************************************
 * @ingroup LacSymQatHash
 *      Populate the fields of the hash state prefix buffer
 *
 * @description
 *      This function populates the inner prefix/aad fields and/or the outer
 *      prefix field of the hash state prefix buffer.
 *
 * @param[in] pHashStateBuf         Pointer to hash state prefix buffer info
 *                                  structure.
 *
 * @param[in] pMsg                  Pointer to the Request Message
 *
 * @param[in] pInnerPrefixAad       Pointer to the Inner Prefix or Aad data
 *                                  This is NULL where if the data size is 0
 *
 * @param[in] innerPrefixSize       Size of inner prefix/aad data in bytes
 *
 * @param[in] pOuterPrefix          Pointer to the Outer Prefix data. This is
 *                                  NULL where the data size is 0.
 *
 * @param[in] outerPrefixSize       Size of the outer prefix data in bytes
 *
 * @return void
 *
 *****************************************************************************/
void LacSymQat_HashStatePrefixAadBufferPopulate(
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBuf,
    icp_qat_la_bulk_req_ftr_t *pMsg,
    Cpa8U *pInnerPrefixAad,
    Cpa8U innerPrefixSize,
    Cpa8U *pOuterPrefix,
    Cpa8U outerPrefixSize);

/**
 ******************************************************************************
 * @ingroup LacSymQatHash
 *      Populate the hash request params structure
 *
 * @description
 *      This function is passed a pointer to the 128B Request block.
 *      (This memory must be allocated prior to calling this function). It
 *      populates the fields of this block using the parameters as described
 *      below. It is also expected that this structure has been set to 0
 *      prior to calling this function.
 *
 *
 * @param[in] pReq                  Pointer to 128B request block.
 *
 * @param[in] authOffsetInBytes     start offset of data that the digest is to
 *                                  be computed on.
 *
 * @param[in] authLenInBytes        Length of data digest calculated on
 *
 * @param[in] pService              Pointer to service data
 *
 * @param[in] pHashStateBuf         Pointer to hash state buffer info. This
 *                                  structure contains the pointers and sizes.
 *                                  If there is no hash state prefix buffer
 *                                  required, this parameter can be set to NULL
 *
 * @param[in] qatPacketType         Packet type using QAT macros. The hash
 *                                  state buffer pointer and state size will be
 *                                  different depending on the packet type
 *
 * @param[in] hashResultSize        Size of the final hash result in bytes.
 *
 * @param[in] digestVerify          Indicates if verify is enabled or not
 *
 * @param[in] pAuthResult           Virtual pointer to digest
 *
 * @return CPA_STATUS_SUCCESS or CPA_STATUS_FAIL
 *
 *****************************************************************************/
CpaStatus LacSymQat_HashRequestParamsPopulate(
    icp_qat_fw_la_bulk_req_t *pReq,
    Cpa32U authOffsetInBytes,
    Cpa32U authLenInBytes,
    sal_service_t *pService,
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBuf,
    Cpa32U qatPacketType,
    Cpa32U hashResultSize,
    CpaBoolean digestVerify,
    Cpa8U *pAuthResult,
    CpaCySymHashAlgorithm alg,
    void *data);

/**
 ******************************************************************************
 * @ingroup LacSymQatHash
 *
 *
 * @description
 *      This fn returns the QAT values for hash algorithm and nested fields
 *
 *
 * @param[in] pInstance              Pointer to service instance.
 *
 * @param[in] qatHashMode            value for hash mode on the fw qat
 *interface.
 *
 * @param[in] apiHashMode            value for hash mode on the QA API.
 *
 * @param[in] apiHashAlgorithm       value for hash algorithm on the QA API.
 *
 * @param[out] pQatAlgorithm         Pointer to return fw qat value for
 *algorithm.
 *
 * @param[out] pQatNested            Pointer to return fw qat value for nested.
 *
 *
 * @return
 *      none
 *
 *****************************************************************************/
void LacSymQat_HashGetCfgData(CpaInstanceHandle pInstance,
			      icp_qat_hw_auth_mode_t qatHashMode,
			      CpaCySymHashMode apiHashMode,
			      CpaCySymHashAlgorithm apiHashAlgorithm,
			      icp_qat_hw_auth_algo_t *pQatAlgorithm,
			      CpaBoolean *pQatNested);

void LacSymQat_HashSetupReqParamsMetaData(
    icp_qat_la_bulk_req_ftr_t *pMsg,
    CpaInstanceHandle instanceHandle,
    const CpaCySymHashSetupData *pHashSetupData,
    CpaBoolean hashStateBuffer,
    icp_qat_hw_auth_mode_t qatHashMode,
    CpaBoolean digestVerify);

#endif /* LAC_SYM_QAT_HASH_H */
