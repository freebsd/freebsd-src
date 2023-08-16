/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_qat_hash.c
 *
 * @ingroup LacSymQatHash
 *
 * Implementation for populating QAT data structures for hash operation
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "lac_log.h"
#include "lac_mem.h"
#include "lac_sym.h"
#include "lac_common.h"
#include "lac_sym_qat.h"
#include "lac_list.h"
#include "lac_sal_types.h"
#include "lac_sal_types_crypto.h"
#include "lac_sym_qat_hash.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "sal_hw_gen.h"

/**
 * This structure contains pointers into the hash setup block of the
 * security descriptor. As the hash setup block contains fields that
 * are of variable length, pointers must be calculated to these fields
 * and the hash setup block is populated using these pointers. */
typedef struct lac_hash_blk_ptrs_s {
	icp_qat_hw_auth_setup_t *pInHashSetup;
	/**< inner hash setup */
	Cpa8U *pInHashInitState1;
	/**< inner initial state 1 */
	Cpa8U *pInHashInitState2;
	/**< inner initial state 2 */
	icp_qat_hw_auth_setup_t *pOutHashSetup;
	/**< outer hash setup */
	Cpa8U *pOutHashInitState1;
	/**< outer hash initial state */
} lac_hash_blk_ptrs_t;

typedef struct lac_hash_blk_ptrs_optimised_s {
	Cpa8U *pInHashInitState1;
	/**< inner initial state 1 */
	Cpa8U *pInHashInitState2;
	/**< inner initial state 2 */

} lac_hash_blk_ptrs_optimised_t;

/**
 * This function calculates the pointers into the hash setup block
 * based on the control block
 *
 * @param[in]  pHashControlBlock    Pointer to hash control block
 * @param[in]  pHwBlockBase         pointer to base of hardware block
 * @param[out] pHashBlkPtrs         structure containing pointers to
 *                                  various fields in the hash setup block
 *
 * @return void
 */
static void
LacSymQat_HashHwBlockPtrsInit(icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock,
			      void *pHwBlockBase,
			      lac_hash_blk_ptrs_t *pHashBlkPtrs);

static void
LacSymQat_HashSetupBlockOptimisedFormatInit(
    const CpaCySymHashSetupData *pHashSetupData,
    icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock,
    void *pHwBlockBase,
    icp_qat_hw_auth_mode_t qatHashMode,
    lac_sym_qat_hash_precompute_info_t *pPrecompute,
    lac_sym_qat_hash_defs_t *pHashDefs,
    lac_sym_qat_hash_defs_t *pOuterHashDefs);

/**
 * This function populates the hash setup block
 *
 * @param[in]  pHashSetupData             Pointer to the hash context
 * @param[in]  pHashControlBlock    Pointer to hash control block
 * @param[in]  pHwBlockBase         pointer to base of hardware block
 * @param[in]  qatHashMode          QAT hash mode
 * @param[in]  pPrecompute          For auth mode, this is the pointer
 *                                  to the precompute data. Otherwise this
 *                                  should be set to NULL
 * @param[in]  pHashDefs            Pointer to Hash definitions
 * @param[in]  pOuterHashDefs       Pointer to Outer Hash definitions.
 *                                  Required for nested hash mode only
 *
 * @return void
 */
static void
LacSymQat_HashSetupBlockInit(const CpaCySymHashSetupData *pHashSetupData,
			     icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock,
			     void *pHwBlockBase,
			     icp_qat_hw_auth_mode_t qatHashMode,
			     lac_sym_qat_hash_precompute_info_t *pPrecompute,
			     lac_sym_qat_hash_defs_t *pHashDefs,
			     lac_sym_qat_hash_defs_t *pOuterHashDefs);

/** @ingroup LacSymQatHash */
void
LacSymQat_HashGetCfgData(CpaInstanceHandle pInstance,
			 icp_qat_hw_auth_mode_t qatHashMode,
			 CpaCySymHashMode apiHashMode,
			 CpaCySymHashAlgorithm apiHashAlgorithm,
			 icp_qat_hw_auth_algo_t *pQatAlgorithm,
			 CpaBoolean *pQatNested)
{
	lac_sym_qat_hash_defs_t *pHashDefs = NULL;

	LacSymQat_HashDefsLookupGet(pInstance, apiHashAlgorithm, &pHashDefs);
	*pQatAlgorithm = pHashDefs->qatInfo->algoEnc;

	if (IS_HASH_MODE_2(qatHashMode)) {
		/* set bit for nested hashing */
		*pQatNested = ICP_QAT_FW_AUTH_HDR_FLAG_DO_NESTED;
	}
	/* Nested hash in mode 0. */
	else if (CPA_CY_SYM_HASH_MODE_NESTED == apiHashMode) {
		/* set bit for nested hashing */
		*pQatNested = ICP_QAT_FW_AUTH_HDR_FLAG_DO_NESTED;
	}
	/* mode0 - plain or mode1 - auth */
	else {
		*pQatNested = ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED;
	}
}

/** @ingroup LacSymQatHash */
void
LacSymQat_HashContentDescInit(icp_qat_la_bulk_req_ftr_t *pMsg,
			      CpaInstanceHandle instanceHandle,
			      const CpaCySymHashSetupData *pHashSetupData,
			      void *pHwBlockBase,
			      Cpa32U hwBlockOffsetInQuadWords,
			      icp_qat_fw_slice_t nextSlice,
			      icp_qat_hw_auth_mode_t qatHashMode,
			      CpaBoolean useSymConstantsTable,
			      CpaBoolean useOptimisedContentDesc,
			      CpaBoolean useStatefulSha3ContentDesc,
			      lac_sym_qat_hash_precompute_info_t *pPrecompute,
			      Cpa32U *pHashBlkSizeInBytes)
{
	icp_qat_fw_auth_cd_ctrl_hdr_t *cd_ctrl =
	    (icp_qat_fw_auth_cd_ctrl_hdr_t *)&(pMsg->cd_ctrl);
	lac_sym_qat_hash_defs_t *pHashDefs = NULL;
	lac_sym_qat_hash_defs_t *pOuterHashDefs = NULL;
	Cpa32U hashSetupBlkSize = 0;

	/* setup the offset in QuadWords into the hw blk */
	cd_ctrl->hash_cfg_offset = (Cpa8U)hwBlockOffsetInQuadWords;

	ICP_QAT_FW_COMN_NEXT_ID_SET(cd_ctrl, nextSlice);
	ICP_QAT_FW_COMN_CURR_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_AUTH);

	LacSymQat_HashDefsLookupGet(instanceHandle,
				    pHashSetupData->hashAlgorithm,
				    &pHashDefs);

	/* Hmac in mode 2 TLS */
	if (IS_HASH_MODE_2(qatHashMode)) {
		if (isCyGen4x((sal_crypto_service_t *)instanceHandle)) {
			/* CPM2.0 has a dedicated bit for HMAC mode2 */
			ICP_QAT_FW_HASH_FLAG_MODE2_SET(cd_ctrl->hash_flags,
						       QAT_FW_LA_MODE2);
		} else {
			/* Set bit for nested hashing.
			 * Make sure not to overwrite other flags in hash_flags
			 * byte.
			 */
			ICP_QAT_FW_HASH_FLAG_AUTH_HDR_NESTED_SET(
			    cd_ctrl->hash_flags,
			    ICP_QAT_FW_AUTH_HDR_FLAG_DO_NESTED);
		}
	}
	/* Nested hash in mode 0 */
	else if (CPA_CY_SYM_HASH_MODE_NESTED == pHashSetupData->hashMode) {
		/* Set bit for nested hashing.
		 * Make sure not to overwrite other flags in hash_flags byte.
		 */
		ICP_QAT_FW_HASH_FLAG_AUTH_HDR_NESTED_SET(
		    cd_ctrl->hash_flags, ICP_QAT_FW_AUTH_HDR_FLAG_DO_NESTED);
	}
	/* mode0 - plain or mode1 - auth */
	else {
		ICP_QAT_FW_HASH_FLAG_AUTH_HDR_NESTED_SET(
		    cd_ctrl->hash_flags, ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED);
	}

	/* Set skip state load flags */
	if (useStatefulSha3ContentDesc) {
		/* Here both skip state load flags are set. FW reads them based
		 * on partial packet type. */
		ICP_QAT_FW_HASH_FLAG_SKIP_INNER_STATE1_LOAD_SET(
		    cd_ctrl->hash_flags, QAT_FW_LA_SKIP_INNER_STATE1_LOAD);
		ICP_QAT_FW_HASH_FLAG_SKIP_OUTER_STATE1_LOAD_SET(
		    cd_ctrl->hash_flags, QAT_FW_LA_SKIP_OUTER_STATE1_LOAD);
	}

	/* set the final digest size */
	cd_ctrl->final_sz = (Cpa8U)pHashSetupData->digestResultLenInBytes;

	/* set the state1 size */
	if (useStatefulSha3ContentDesc) {
		cd_ctrl->inner_state1_sz =
		    LAC_ALIGN_POW2_ROUNDUP(LAC_HASH_SHA3_STATEFUL_STATE_SIZE,
					   LAC_QUAD_WORD_IN_BYTES);
	} else {
		cd_ctrl->inner_state1_sz =
		    LAC_ALIGN_POW2_ROUNDUP(pHashDefs->qatInfo->state1Length,
					   LAC_QUAD_WORD_IN_BYTES);
	}

	/* set the inner result size to the digest length */
	cd_ctrl->inner_res_sz = (Cpa8U)pHashDefs->algInfo->digestLength;

	/* set the state2 size - only for mode 1 Auth algos and AES CBC MAC */
	if (IS_HASH_MODE_1(qatHashMode) ||
	    pHashSetupData->hashAlgorithm == CPA_CY_SYM_HASH_AES_CBC_MAC ||
	    pHashSetupData->hashAlgorithm == CPA_CY_SYM_HASH_ZUC_EIA3) {
		cd_ctrl->inner_state2_sz =
		    LAC_ALIGN_POW2_ROUNDUP(pHashDefs->qatInfo->state2Length,
					   LAC_QUAD_WORD_IN_BYTES);
	} else {
		cd_ctrl->inner_state2_sz = 0;
	}

	if (useSymConstantsTable) {
		cd_ctrl->inner_state2_offset =
		    LAC_BYTES_TO_QUADWORDS(cd_ctrl->inner_state1_sz);

		/* size of inner part of hash setup block */
		hashSetupBlkSize =
		    cd_ctrl->inner_state1_sz + cd_ctrl->inner_state2_sz;
	} else {
		cd_ctrl->inner_state2_offset = cd_ctrl->hash_cfg_offset +
		    LAC_BYTES_TO_QUADWORDS(sizeof(icp_qat_hw_auth_setup_t) +
					   cd_ctrl->inner_state1_sz);

		/* size of inner part of hash setup block */
		hashSetupBlkSize = sizeof(icp_qat_hw_auth_setup_t) +
		    cd_ctrl->inner_state1_sz + cd_ctrl->inner_state2_sz;
	}

	/* For nested hashing - Fill in the outer fields */
	if (CPA_CY_SYM_HASH_MODE_NESTED == pHashSetupData->hashMode ||
	    IS_HASH_MODE_2(qatHashMode)) {
		/* For nested - use the outer algorithm. This covers TLS and
		 * nested hash. For HMAC mode2 use inner algorithm again */
		CpaCySymHashAlgorithm outerAlg =
		    (CPA_CY_SYM_HASH_MODE_NESTED == pHashSetupData->hashMode) ?
		    pHashSetupData->nestedModeSetupData.outerHashAlgorithm :
		    pHashSetupData->hashAlgorithm;

		LacSymQat_HashDefsLookupGet(instanceHandle,
					    outerAlg,
					    &pOuterHashDefs);

		/* outer config offset */
		cd_ctrl->outer_config_offset = cd_ctrl->inner_state2_offset +
		    LAC_BYTES_TO_QUADWORDS(cd_ctrl->inner_state2_sz);

		if (useStatefulSha3ContentDesc) {
			cd_ctrl->outer_state1_sz = LAC_ALIGN_POW2_ROUNDUP(
			    LAC_HASH_SHA3_STATEFUL_STATE_SIZE,
			    LAC_QUAD_WORD_IN_BYTES);
		} else {
			cd_ctrl->outer_state1_sz = LAC_ALIGN_POW2_ROUNDUP(
			    pOuterHashDefs->algInfo->stateSize,
			    LAC_QUAD_WORD_IN_BYTES);
		}

		/* outer result size */
		cd_ctrl->outer_res_sz =
		    (Cpa8U)pOuterHashDefs->algInfo->digestLength;

		/* outer_prefix_offset will be the size of the inner prefix data
		 * plus the hash state storage size. */
		/* The prefix buffer is part of the ReqParams, so this param
		 * will be setup where ReqParams are set up */

		/* add on size of outer part of hash block */
		hashSetupBlkSize +=
		    sizeof(icp_qat_hw_auth_setup_t) + cd_ctrl->outer_state1_sz;
	} else {
		cd_ctrl->outer_config_offset = 0;
		cd_ctrl->outer_state1_sz = 0;
		cd_ctrl->outer_res_sz = 0;
	}

	if (CPA_CY_SYM_HASH_SNOW3G_UIA2 == pHashSetupData->hashAlgorithm) {
		/* add the size for the cipher config word, the key and the IV*/
		hashSetupBlkSize += sizeof(icp_qat_hw_cipher_config_t) +
		    pHashSetupData->authModeSetupData.authKeyLenInBytes +
		    ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ;
	}

	*pHashBlkSizeInBytes = hashSetupBlkSize;

	if (useOptimisedContentDesc) {
		LacSymQat_HashSetupBlockOptimisedFormatInit(pHashSetupData,
							    cd_ctrl,
							    pHwBlockBase,
							    qatHashMode,
							    pPrecompute,
							    pHashDefs,
							    pOuterHashDefs);
	} else if (!useSymConstantsTable) {
		/*****************************************************************************
		 *                        Populate Hash Setup block *
		 *****************************************************************************/
		LacSymQat_HashSetupBlockInit(pHashSetupData,
					     cd_ctrl,
					     pHwBlockBase,
					     qatHashMode,
					     pPrecompute,
					     pHashDefs,
					     pOuterHashDefs);
	}
}

/* This fn populates fields in both the CD ctrl block and the ReqParams block
 * which describe the Hash ReqParams:
 * cd_ctrl.outer_prefix_offset
 * cd_ctrl.outer_prefix_sz
 * req_params.inner_prefix_sz/aad_sz
 * req_params.hash_state_sz
 * req_params.auth_res_sz
 *
 */
void
LacSymQat_HashSetupReqParamsMetaData(
    icp_qat_la_bulk_req_ftr_t *pMsg,
    CpaInstanceHandle instanceHandle,
    const CpaCySymHashSetupData *pHashSetupData,
    CpaBoolean hashStateBuffer,
    icp_qat_hw_auth_mode_t qatHashMode,
    CpaBoolean digestVerify)
{
	icp_qat_fw_auth_cd_ctrl_hdr_t *cd_ctrl = NULL;
	icp_qat_la_auth_req_params_t *pHashReqParams = NULL;
	lac_sym_qat_hash_defs_t *pHashDefs = NULL;

	cd_ctrl = (icp_qat_fw_auth_cd_ctrl_hdr_t *)&(pMsg->cd_ctrl);
	pHashReqParams =
	    (icp_qat_la_auth_req_params_t *)(&(pMsg->serv_specif_rqpars));

	LacSymQat_HashDefsLookupGet(instanceHandle,
				    pHashSetupData->hashAlgorithm,
				    &pHashDefs);

	/* Hmac in mode 2 TLS */
	if (IS_HASH_MODE_2(qatHashMode)) {
		/* Inner and outer prefixes are the block length */
		pHashReqParams->u2.inner_prefix_sz =
		    (Cpa8U)pHashDefs->algInfo->blockLength;
		cd_ctrl->outer_prefix_sz =
		    (Cpa8U)pHashDefs->algInfo->blockLength;
		cd_ctrl->outer_prefix_offset = LAC_BYTES_TO_QUADWORDS(
		    LAC_ALIGN_POW2_ROUNDUP((pHashReqParams->u2.inner_prefix_sz),
					   LAC_QUAD_WORD_IN_BYTES));
	}
	/* Nested hash in mode 0 */
	else if (CPA_CY_SYM_HASH_MODE_NESTED == pHashSetupData->hashMode) {

		/* set inner and outer prefixes */
		pHashReqParams->u2.inner_prefix_sz =
		    (Cpa8U)pHashSetupData->nestedModeSetupData
			.innerPrefixLenInBytes;
		cd_ctrl->outer_prefix_sz =
		    (Cpa8U)pHashSetupData->nestedModeSetupData
			.outerPrefixLenInBytes;
		cd_ctrl->outer_prefix_offset = LAC_BYTES_TO_QUADWORDS(
		    LAC_ALIGN_POW2_ROUNDUP((pHashReqParams->u2.inner_prefix_sz),
					   LAC_QUAD_WORD_IN_BYTES));
	}
	/* mode0 - plain or mode1 - auth */
	else {
		Cpa16U aadDataSize = 0;

		/* For Auth Encrypt set the aad size */
		if (CPA_CY_SYM_HASH_AES_CCM == pHashSetupData->hashAlgorithm) {
			/* at the beginning of the buffer there is B0 block */
			aadDataSize = LAC_HASH_AES_CCM_BLOCK_SIZE;

			/* then, if there is some 'a' data, the buffer will
			 * store encoded
			 * length of 'a' and 'a' itself */
			if (pHashSetupData->authModeSetupData.aadLenInBytes >
			    0) {
				/* as the QAT API puts the requirement on the
				 * pAdditionalAuthData not to be bigger than 240
				 * bytes then we
				 * just need 2 bytes to store encoded length of
				 * 'a' */
				aadDataSize += sizeof(Cpa16U);
				aadDataSize +=
				    (Cpa16U)pHashSetupData->authModeSetupData
					.aadLenInBytes;
			}

			/* round the aad size to the multiple of CCM block
			 * size.*/
			pHashReqParams->u2.aad_sz =
			    LAC_ALIGN_POW2_ROUNDUP(aadDataSize,
						   LAC_HASH_AES_CCM_BLOCK_SIZE);
		} else if (CPA_CY_SYM_HASH_AES_GCM ==
			   pHashSetupData->hashAlgorithm) {
			aadDataSize =
			    (Cpa16U)
				pHashSetupData->authModeSetupData.aadLenInBytes;

			/* round the aad size to the multiple of GCM hash block
			 * size. */
			pHashReqParams->u2.aad_sz =
			    LAC_ALIGN_POW2_ROUNDUP(aadDataSize,
						   LAC_HASH_AES_GCM_BLOCK_SIZE);
		} else {
			pHashReqParams->u2.aad_sz = 0;
		}

		cd_ctrl->outer_prefix_sz = 0;
		cd_ctrl->outer_prefix_offset = 0;
	}

	/* If there is a hash state prefix buffer */
	if (CPA_TRUE == hashStateBuffer) {
		/* Note, this sets up size for both aad and non-aad cases */
		pHashReqParams->hash_state_sz = LAC_BYTES_TO_QUADWORDS(
		    LAC_ALIGN_POW2_ROUNDUP(pHashReqParams->u2.inner_prefix_sz,
					   LAC_QUAD_WORD_IN_BYTES) +
		    LAC_ALIGN_POW2_ROUNDUP(cd_ctrl->outer_prefix_sz,
					   LAC_QUAD_WORD_IN_BYTES));
	} else {
		pHashReqParams->hash_state_sz = 0;
	}

	if (CPA_TRUE == digestVerify) {
		/* auth result size in bytes to be read in for a verify
		 * operation */
		pHashReqParams->auth_res_sz =
		    (Cpa8U)pHashSetupData->digestResultLenInBytes;
	} else {
		pHashReqParams->auth_res_sz = 0;
	}

	pHashReqParams->resrvd1 = 0;
}

void
LacSymQat_HashHwBlockPtrsInit(icp_qat_fw_auth_cd_ctrl_hdr_t *cd_ctrl,
			      void *pHwBlockBase,
			      lac_hash_blk_ptrs_t *pHashBlkPtrs)
{
	/* encoded offset for inner config is converted to a byte offset. */
	pHashBlkPtrs->pInHashSetup =
	    (icp_qat_hw_auth_setup_t *)((Cpa8U *)pHwBlockBase +
					(cd_ctrl->hash_cfg_offset *
					 LAC_QUAD_WORD_IN_BYTES));

	pHashBlkPtrs->pInHashInitState1 = (Cpa8U *)pHashBlkPtrs->pInHashSetup +
	    sizeof(icp_qat_hw_auth_setup_t);

	pHashBlkPtrs->pInHashInitState2 =
	    (Cpa8U *)(pHashBlkPtrs->pInHashInitState1) +
	    cd_ctrl->inner_state1_sz;

	pHashBlkPtrs->pOutHashSetup =
	    (icp_qat_hw_auth_setup_t *)((Cpa8U *)(pHashBlkPtrs
						      ->pInHashInitState2) +
					cd_ctrl->inner_state2_sz);

	pHashBlkPtrs->pOutHashInitState1 =
	    (Cpa8U *)(pHashBlkPtrs->pOutHashSetup) +
	    sizeof(icp_qat_hw_auth_setup_t);
}

static void
LacSymQat_HashSetupBlockInit(const CpaCySymHashSetupData *pHashSetupData,
			     icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock,
			     void *pHwBlockBase,
			     icp_qat_hw_auth_mode_t qatHashMode,
			     lac_sym_qat_hash_precompute_info_t *pPrecompute,
			     lac_sym_qat_hash_defs_t *pHashDefs,
			     lac_sym_qat_hash_defs_t *pOuterHashDefs)
{
	Cpa32U innerConfig = 0;
	lac_hash_blk_ptrs_t hashBlkPtrs = { 0 };
	Cpa32U aedHashCmpLength = 0;

	LacSymQat_HashHwBlockPtrsInit(pHashControlBlock,
				      pHwBlockBase,
				      &hashBlkPtrs);

	innerConfig = ICP_QAT_HW_AUTH_CONFIG_BUILD(
	    qatHashMode,
	    pHashDefs->qatInfo->algoEnc,
	    pHashSetupData->digestResultLenInBytes);

	/* Set the Inner hash configuration */
	hashBlkPtrs.pInHashSetup->auth_config.config = innerConfig;
	hashBlkPtrs.pInHashSetup->auth_config.reserved = 0;

	/* For mode 1 pre-computes for auth algorithms */
	if (IS_HASH_MODE_1(qatHashMode) ||
	    CPA_CY_SYM_HASH_AES_CBC_MAC == pHashSetupData->hashAlgorithm ||
	    CPA_CY_SYM_HASH_ZUC_EIA3 == pHashSetupData->hashAlgorithm) {
		/* for HMAC in mode 1 authCounter is the block size
		 * else the authCounter is 0. The firmware expects the counter
		 * to be
		 * big endian */
		LAC_MEM_SHARED_WRITE_SWAP(
		    hashBlkPtrs.pInHashSetup->auth_counter.counter,
		    pHashDefs->qatInfo->authCounter);

		/* state 1 is set to 0 for the following algorithms */
		if ((CPA_CY_SYM_HASH_AES_XCBC ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_AES_CMAC ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_AES_CBC_MAC ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_KASUMI_F9 ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_SNOW3G_UIA2 ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_AES_CCM ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_AES_GMAC ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_AES_GCM ==
		     pHashSetupData->hashAlgorithm) ||
		    (CPA_CY_SYM_HASH_ZUC_EIA3 ==
		     pHashSetupData->hashAlgorithm)) {
			LAC_OS_BZERO(hashBlkPtrs.pInHashInitState1,
				     pHashDefs->qatInfo->state1Length);
		}

		/* Pad remaining bytes of sha1 precomputes */
		if (CPA_CY_SYM_HASH_SHA1 == pHashSetupData->hashAlgorithm) {
			Cpa32U state1PadLen = 0;
			Cpa32U state2PadLen = 0;

			if (pHashControlBlock->inner_state1_sz >
			    pHashDefs->algInfo->stateSize) {
				state1PadLen =
				    pHashControlBlock->inner_state1_sz -
				    pHashDefs->algInfo->stateSize;
			}

			if (pHashControlBlock->inner_state2_sz >
			    pHashDefs->algInfo->stateSize) {
				state2PadLen =
				    pHashControlBlock->inner_state2_sz -
				    pHashDefs->algInfo->stateSize;
			}

			if (state1PadLen > 0) {

				LAC_OS_BZERO(hashBlkPtrs.pInHashInitState1 +
						 pHashDefs->algInfo->stateSize,
					     state1PadLen);
			}

			if (state2PadLen > 0) {
				LAC_OS_BZERO(hashBlkPtrs.pInHashInitState2 +
						 pHashDefs->algInfo->stateSize,
					     state2PadLen);
			}
		}

		pPrecompute->state1Size = pHashDefs->qatInfo->state1Length;
		pPrecompute->state2Size = pHashDefs->qatInfo->state2Length;

		/* Set the destination for pre-compute state1 data to be written
		 */
		pPrecompute->pState1 = hashBlkPtrs.pInHashInitState1;

		/* Set the destination for pre-compute state1 data to be written
		 */
		pPrecompute->pState2 = hashBlkPtrs.pInHashInitState2;
	}
	/* For digest and nested digest */
	else {
		Cpa32U padLen = pHashControlBlock->inner_state1_sz -
		    pHashDefs->algInfo->stateSize;

		/* counter set to 0 */
		hashBlkPtrs.pInHashSetup->auth_counter.counter = 0;

		/* set the inner hash state 1 */
		memcpy(hashBlkPtrs.pInHashInitState1,
		       pHashDefs->algInfo->initState,
		       pHashDefs->algInfo->stateSize);

		if (padLen > 0) {
			LAC_OS_BZERO(hashBlkPtrs.pInHashInitState1 +
					 pHashDefs->algInfo->stateSize,
				     padLen);
		}
	}

	hashBlkPtrs.pInHashSetup->auth_counter.reserved = 0;

	/* Fill in the outer part of the hash setup block */
	if ((CPA_CY_SYM_HASH_MODE_NESTED == pHashSetupData->hashMode ||
	     IS_HASH_MODE_2(qatHashMode)) &&
	    (NULL != pOuterHashDefs)) {
		Cpa32U outerConfig = ICP_QAT_HW_AUTH_CONFIG_BUILD(
		    qatHashMode,
		    pOuterHashDefs->qatInfo->algoEnc,
		    pHashSetupData->digestResultLenInBytes);

		Cpa32U padLen = pHashControlBlock->outer_state1_sz -
		    pOuterHashDefs->algInfo->stateSize;

		/* populate the auth config */
		hashBlkPtrs.pOutHashSetup->auth_config.config = outerConfig;
		hashBlkPtrs.pOutHashSetup->auth_config.reserved = 0;

		/* outer Counter set to 0 */
		hashBlkPtrs.pOutHashSetup->auth_counter.counter = 0;
		hashBlkPtrs.pOutHashSetup->auth_counter.reserved = 0;

		/* set outer hash state 1 */
		memcpy(hashBlkPtrs.pOutHashInitState1,
		       pOuterHashDefs->algInfo->initState,
		       pOuterHashDefs->algInfo->stateSize);

		if (padLen > 0) {
			LAC_OS_BZERO(hashBlkPtrs.pOutHashInitState1 +
					 pOuterHashDefs->algInfo->stateSize,
				     padLen);
		}
	}

	if (CPA_CY_SYM_HASH_SNOW3G_UIA2 == pHashSetupData->hashAlgorithm) {
		icp_qat_hw_cipher_config_t *pCipherConfig =
		    (icp_qat_hw_cipher_config_t *)hashBlkPtrs.pOutHashSetup;

		pCipherConfig->val = ICP_QAT_HW_CIPHER_CONFIG_BUILD(
		    ICP_QAT_HW_CIPHER_ECB_MODE,
		    ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2,
		    ICP_QAT_HW_CIPHER_KEY_CONVERT,
		    ICP_QAT_HW_CIPHER_ENCRYPT,
		    aedHashCmpLength);

		pCipherConfig->reserved = 0;

		memcpy((Cpa8U *)pCipherConfig +
			   sizeof(icp_qat_hw_cipher_config_t),
		       pHashSetupData->authModeSetupData.authKey,
		       pHashSetupData->authModeSetupData.authKeyLenInBytes);

		LAC_OS_BZERO(
		    (Cpa8U *)pCipherConfig +
			sizeof(icp_qat_hw_cipher_config_t) +
			pHashSetupData->authModeSetupData.authKeyLenInBytes,
		    ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ);
	} else if (CPA_CY_SYM_HASH_ZUC_EIA3 == pHashSetupData->hashAlgorithm) {
		icp_qat_hw_cipher_config_t *pCipherConfig =
		    (icp_qat_hw_cipher_config_t *)hashBlkPtrs.pOutHashSetup;

		pCipherConfig->val = ICP_QAT_HW_CIPHER_CONFIG_BUILD(
		    ICP_QAT_HW_CIPHER_ECB_MODE,
		    ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3,
		    ICP_QAT_HW_CIPHER_KEY_CONVERT,
		    ICP_QAT_HW_CIPHER_ENCRYPT,
		    aedHashCmpLength);

		pCipherConfig->reserved = 0;

		memcpy((Cpa8U *)pCipherConfig +
			   sizeof(icp_qat_hw_cipher_config_t),
		       pHashSetupData->authModeSetupData.authKey,
		       pHashSetupData->authModeSetupData.authKeyLenInBytes);

		LAC_OS_BZERO(
		    (Cpa8U *)pCipherConfig +
			sizeof(icp_qat_hw_cipher_config_t) +
			pHashSetupData->authModeSetupData.authKeyLenInBytes,
		    ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ);
	}
}

static void
LacSymQat_HashOpHwBlockPtrsInit(icp_qat_fw_auth_cd_ctrl_hdr_t *cd_ctrl,
				void *pHwBlockBase,
				lac_hash_blk_ptrs_optimised_t *pHashBlkPtrs)
{
	pHashBlkPtrs->pInHashInitState1 = (((Cpa8U *)pHwBlockBase) + 16);
	pHashBlkPtrs->pInHashInitState2 =
	    (Cpa8U *)(pHashBlkPtrs->pInHashInitState1) +
	    cd_ctrl->inner_state1_sz;
}

static void
LacSymQat_HashSetupBlockOptimisedFormatInit(
    const CpaCySymHashSetupData *pHashSetupData,
    icp_qat_fw_auth_cd_ctrl_hdr_t *pHashControlBlock,
    void *pHwBlockBase,
    icp_qat_hw_auth_mode_t qatHashMode,
    lac_sym_qat_hash_precompute_info_t *pPrecompute,
    lac_sym_qat_hash_defs_t *pHashDefs,
    lac_sym_qat_hash_defs_t *pOuterHashDefs)
{

	Cpa32U state1PadLen = 0;
	Cpa32U state2PadLen = 0;

	lac_hash_blk_ptrs_optimised_t pHashBlkPtrs = { 0 };

	LacSymQat_HashOpHwBlockPtrsInit(pHashControlBlock,
					pHwBlockBase,
					&pHashBlkPtrs);

	if (pHashControlBlock->inner_state1_sz >
	    pHashDefs->algInfo->stateSize) {
		state1PadLen = pHashControlBlock->inner_state1_sz -
		    pHashDefs->algInfo->stateSize;
	}

	if (pHashControlBlock->inner_state2_sz >
	    pHashDefs->algInfo->stateSize) {
		state2PadLen = pHashControlBlock->inner_state2_sz -
		    pHashDefs->algInfo->stateSize;
	}

	if (state1PadLen > 0) {

		LAC_OS_BZERO(pHashBlkPtrs.pInHashInitState1 +
				 pHashDefs->algInfo->stateSize,
			     state1PadLen);
	}

	if (state2PadLen > 0) {

		LAC_OS_BZERO(pHashBlkPtrs.pInHashInitState2 +
				 pHashDefs->algInfo->stateSize,
			     state2PadLen);
	}
	pPrecompute->state1Size = pHashDefs->qatInfo->state1Length;
	pPrecompute->state2Size = pHashDefs->qatInfo->state2Length;

	/* Set the destination for pre-compute state1 data to be written */
	pPrecompute->pState1 = pHashBlkPtrs.pInHashInitState1;

	/* Set the destination for pre-compute state1 data to be written */
	pPrecompute->pState2 = pHashBlkPtrs.pInHashInitState2;
}

void
LacSymQat_HashStatePrefixAadBufferSizeGet(
    icp_qat_la_bulk_req_ftr_t *pMsg,
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBuf)
{
	const icp_qat_fw_auth_cd_ctrl_hdr_t *cd_ctrl;
	icp_qat_la_auth_req_params_t *pHashReqParams;

	cd_ctrl = (icp_qat_fw_auth_cd_ctrl_hdr_t *)&(pMsg->cd_ctrl);
	pHashReqParams =
	    (icp_qat_la_auth_req_params_t *)(&(pMsg->serv_specif_rqpars));

	/* hash state storage needed to support partial packets. Space reserved
	 * for this in all cases */
	pHashStateBuf->stateStorageSzQuadWords = LAC_BYTES_TO_QUADWORDS(
	    sizeof(icp_qat_hw_auth_counter_t) + cd_ctrl->inner_state1_sz);

	pHashStateBuf->prefixAadSzQuadWords = pHashReqParams->hash_state_sz;
}

void
LacSymQat_HashStatePrefixAadBufferPopulate(
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBuf,
    icp_qat_la_bulk_req_ftr_t *pMsg,
    Cpa8U *pInnerPrefixAad,
    Cpa8U innerPrefixSize,
    Cpa8U *pOuterPrefix,
    Cpa8U outerPrefixSize)
{
	const icp_qat_fw_auth_cd_ctrl_hdr_t *cd_ctrl =
	    (icp_qat_fw_auth_cd_ctrl_hdr_t *)&(pMsg->cd_ctrl);

	icp_qat_la_auth_req_params_t *pHashReqParams =
	    (icp_qat_la_auth_req_params_t *)(&(pMsg->serv_specif_rqpars));

	/*
	 * Let S be the supplied secret
	 * S1 = S/2 if S is even and (S/2 + 1) if S is odd.
	 * Set length S2 (inner prefix) = S1 and the start address
	 * of S2 is S[S1/2] i.e. if S is odd then S2 starts at the last byte of
	 * S1
	 * _____________________________________________________________
	 * |  outer prefix  |                padding                    |
	 * |________________|                                           |
	 * |                                                            |
	 * |____________________________________________________________|
	 * |  inner prefix  |                padding                    |
	 * |________________|                                           |
	 * |                                                            |
	 * |____________________________________________________________|
	 *
	 */
	if (NULL != pInnerPrefixAad) {
		Cpa8U *pLocalInnerPrefix =
		    (Cpa8U *)(pHashStateBuf->pData) +
		    LAC_QUADWORDS_TO_BYTES(
			pHashStateBuf->stateStorageSzQuadWords);
		Cpa8U padding =
		    pHashReqParams->u2.inner_prefix_sz - innerPrefixSize;
		/* copy the inner prefix or aad data */
		memcpy(pLocalInnerPrefix, pInnerPrefixAad, innerPrefixSize);

		/* Reset with zeroes any area reserved for padding in this block
		 */
		if (0 < padding) {
			LAC_OS_BZERO(pLocalInnerPrefix + innerPrefixSize,
				     padding);
		}
	}

	if (NULL != pOuterPrefix) {
		Cpa8U *pLocalOuterPrefix =
		    (Cpa8U *)pHashStateBuf->pData +
		    LAC_QUADWORDS_TO_BYTES(
			pHashStateBuf->stateStorageSzQuadWords +
			cd_ctrl->outer_prefix_offset);
		Cpa8U padding = LAC_QUADWORDS_TO_BYTES(
				    pHashStateBuf->prefixAadSzQuadWords) -
		    pHashReqParams->u2.inner_prefix_sz - outerPrefixSize;

		/* copy the outer prefix */
		memcpy(pLocalOuterPrefix, pOuterPrefix, outerPrefixSize);

		/* Reset with zeroes any area reserved for padding in this block
		 */
		if (0 < padding) {
			LAC_OS_BZERO(pLocalOuterPrefix + outerPrefixSize,
				     padding);
		}
	}
}

inline CpaStatus
LacSymQat_HashRequestParamsPopulate(
    icp_qat_fw_la_bulk_req_t *pReq,
    Cpa32U authOffsetInBytes,
    Cpa32U authLenInBytes,
    sal_service_t *pService,
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBuf,
    Cpa32U packetType,
    Cpa32U hashResultSize,
    CpaBoolean digestVerify,
    Cpa8U *pAuthResult,
    CpaCySymHashAlgorithm alg,
    void *pHKDFSecret)
{
	Cpa64U authResultPhys = 0;
	icp_qat_fw_la_auth_req_params_t *pHashReqParams;

	pHashReqParams = (icp_qat_fw_la_auth_req_params_t
			      *)((Cpa8U *)&(pReq->serv_specif_rqpars) +
				 ICP_QAT_FW_HASH_REQUEST_PARAMETERS_OFFSET);

	pHashReqParams->auth_off = authOffsetInBytes;
	pHashReqParams->auth_len = authLenInBytes;

	/* Set the physical location of secret for HKDF */
	if (NULL != pHKDFSecret) {
		LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(
		    (*pService), pHashReqParams->u1.aad_adr, pHKDFSecret);

		if (0 == pHashReqParams->u1.aad_adr) {
			LAC_LOG_ERROR(
			    "Unable to get the physical address of the"
			    " HKDF secret\n");
			return CPA_STATUS_FAIL;
		}
	}

	/* For a Full packet or last partial need to set the digest result
	 * pointer
	 * and the auth result field */
	if (NULL != pAuthResult) {
		authResultPhys =
		    LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
						 (void *)pAuthResult);

		if (authResultPhys == 0) {
			LAC_LOG_ERROR(
			    "Unable to get the physical address of the"
			    " auth result\n");
			return CPA_STATUS_FAIL;
		}

		pHashReqParams->auth_res_addr = authResultPhys;
	} else {
		pHashReqParams->auth_res_addr = 0;
	}

	if (CPA_TRUE == digestVerify) {
		/* auth result size in bytes to be read in for a verify
		 *  operation */
		pHashReqParams->auth_res_sz = (Cpa8U)hashResultSize;
	} else {
		pHashReqParams->auth_res_sz = 0;
	}

	/* If there is a hash state prefix buffer */
	if (NULL != pHashStateBuf) {
		/* Only write the pointer to the buffer if the size is greater
		 * than 0
		 * this will be the case for plain and auth mode due to the
		 * state storage required for partial packets and for nested
		 * mode (when
		 * the prefix data is > 0) */
		if ((pHashStateBuf->stateStorageSzQuadWords +
		     pHashStateBuf->prefixAadSzQuadWords) > 0) {
			/* For the first partial packet, the QAT expects the
			 * pointer to the
			 * inner prefix even if there is no memory allocated for
			 * this. The
			 * QAT will internally calculate where to write the
			 * state back. */
			if ((ICP_QAT_FW_LA_PARTIAL_START == packetType) ||
			    (ICP_QAT_FW_LA_PARTIAL_NONE == packetType)) {
				// prefix_addr changed to auth_partial_st_prefix
				pHashReqParams->u1.auth_partial_st_prefix =
				    ((pHashStateBuf->pDataPhys) +
				     LAC_QUADWORDS_TO_BYTES(
					 pHashStateBuf
					     ->stateStorageSzQuadWords));
			} else {
				pHashReqParams->u1.auth_partial_st_prefix =
				    pHashStateBuf->pDataPhys;
			}
		}
		/* nested mode when the prefix data is 0 */
		else {
			pHashReqParams->u1.auth_partial_st_prefix = 0;
		}

		/* For middle & last partial, state size is the hash state
		 * storage
		 * if hash mode 2 this will include the prefix data */
		if ((ICP_QAT_FW_LA_PARTIAL_MID == packetType) ||
		    (ICP_QAT_FW_LA_PARTIAL_END == packetType)) {
			pHashReqParams->hash_state_sz =
			    (pHashStateBuf->stateStorageSzQuadWords +
			     pHashStateBuf->prefixAadSzQuadWords);
		}
		/* For full packets and first partials set the state size to
		 * that of
		 * the prefix/aad. prefix includes both the inner and  outer
		 * prefix */
		else {
			pHashReqParams->hash_state_sz =
			    pHashStateBuf->prefixAadSzQuadWords;
		}
	} else {
		pHashReqParams->u1.auth_partial_st_prefix = 0;
		pHashReqParams->hash_state_sz = 0;
	}

	/*  GMAC only */
	if (CPA_CY_SYM_HASH_AES_GMAC == alg) {
		pHashReqParams->hash_state_sz = 0;
		pHashReqParams->u1.aad_adr = 0;
	}

	/* This field is only used by TLS requests */
	/* In TLS case this is set after this function is called */
	pHashReqParams->resrvd1 = 0;
	return CPA_STATUS_SUCCESS;
}
