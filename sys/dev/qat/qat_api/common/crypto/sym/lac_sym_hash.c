/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_hash.c
 *
 * @ingroup LacHash
 *
 * Hash specific functionality
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

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

#include "lac_common.h"
#include "lac_mem.h"
#include "lac_sym.h"
#include "lac_session.h"
#include "lac_sym_hash.h"
#include "lac_log.h"
#include "lac_sym_qat_hash.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sym_cb.h"
#include "lac_sync.h"

#define LAC_HASH_ALG_MODE_NOT_SUPPORTED(alg, mode)                             \
	((((CPA_CY_SYM_HASH_KASUMI_F9 == (alg)) ||                             \
	   (CPA_CY_SYM_HASH_SNOW3G_UIA2 == (alg)) ||                           \
	   (CPA_CY_SYM_HASH_AES_XCBC == (alg)) ||                              \
	   (CPA_CY_SYM_HASH_AES_CCM == (alg)) ||                               \
	   (CPA_CY_SYM_HASH_AES_GCM == (alg)) ||                               \
	   (CPA_CY_SYM_HASH_AES_GMAC == (alg)) ||                              \
	   (CPA_CY_SYM_HASH_AES_CMAC == (alg)) ||                              \
	   (CPA_CY_SYM_HASH_ZUC_EIA3 == (alg))) &&                             \
	  (CPA_CY_SYM_HASH_MODE_AUTH != (mode))) ||                            \
	 ((LAC_HASH_IS_SHA3(alg)) && (CPA_CY_SYM_HASH_MODE_NESTED == (mode))))
/**< Macro to check for valid algorithm-mode combination */

void LacSync_GenBufListVerifyCb(void *pCallbackTag,
				CpaStatus status,
				CpaCySymOp operationType,
				void *pOpData,
				CpaBufferList *pDstBuffer,
				CpaBoolean opResult);

/**
 * @ingroup LacHash
 * This callback function will be invoked whenever a synchronous
 * hash precompute operation completes.  It will set the wait
 * queue flag for the synchronous operation.
 *
 * @param[in] pCallbackTag  Opaque value provided by user. This will
 *                         be a pointer to a wait queue flag.
 *
 * @retval
 *     None
 *
 */
static void
LacHash_SyncPrecomputeDoneCb(void *pCallbackTag)
{
	LacSync_GenWakeupSyncCaller(pCallbackTag, CPA_STATUS_SUCCESS);
}

/** @ingroup LacHash */
CpaStatus
LacHash_StatePrefixAadBufferInit(
    sal_service_t *pService,
    const CpaCySymHashSetupData *pHashSetupData,
    icp_qat_la_bulk_req_ftr_t *pReq,
    icp_qat_hw_auth_mode_t qatHashMode,
    Cpa8U *pHashStateBuffer,
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBufferInfo)
{
	/* set up the hash state prefix buffer info structure */
	pHashStateBufferInfo->pData = pHashStateBuffer;

	pHashStateBufferInfo->pDataPhys = LAC_MEM_CAST_PTR_TO_UINT64(
	    LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService), pHashStateBuffer));

	if (pHashStateBufferInfo->pDataPhys == 0) {
		LAC_LOG_ERROR("Unable to get the physical address of "
			      "the hash state buffer\n");
		return CPA_STATUS_FAIL;
	}

	LacSymQat_HashStatePrefixAadBufferSizeGet(pReq, pHashStateBufferInfo);

	/* Prefix data gets copied to the hash state buffer for nested mode */
	if (CPA_CY_SYM_HASH_MODE_NESTED == pHashSetupData->hashMode) {
		LacSymQat_HashStatePrefixAadBufferPopulate(
		    pHashStateBufferInfo,
		    pReq,
		    pHashSetupData->nestedModeSetupData.pInnerPrefixData,
		    (Cpa8U)pHashSetupData->nestedModeSetupData
			.innerPrefixLenInBytes,
		    pHashSetupData->nestedModeSetupData.pOuterPrefixData,
		    (Cpa8U)pHashSetupData->nestedModeSetupData
			.outerPrefixLenInBytes);
	}
	/* For mode2 HMAC the key gets copied into both the inner and
	 * outer prefix fields */
	else if (IS_HASH_MODE_2_AUTH(qatHashMode, pHashSetupData->hashMode)) {
		LacSymQat_HashStatePrefixAadBufferPopulate(
		    pHashStateBufferInfo,
		    pReq,
		    pHashSetupData->authModeSetupData.authKey,
		    (Cpa8U)pHashSetupData->authModeSetupData.authKeyLenInBytes,
		    pHashSetupData->authModeSetupData.authKey,
		    (Cpa8U)pHashSetupData->authModeSetupData.authKeyLenInBytes);
	}
	/* else do nothing for the other cases */
	return CPA_STATUS_SUCCESS;
}

/** @ingroup LacHash */
CpaStatus
LacHash_PrecomputeDataCreate(const CpaInstanceHandle instanceHandle,
			     CpaCySymSessionSetupData *pSessionSetup,
			     lac_hash_precompute_done_cb_t callbackFn,
			     void *pCallbackTag,
			     Cpa8U *pWorkingBuffer,
			     Cpa8U *pState1,
			     Cpa8U *pState2)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa8U *pAuthKey = NULL;
	Cpa32U authKeyLenInBytes = 0;
	CpaCySymHashAlgorithm hashAlgorithm =
	    pSessionSetup->hashSetupData.hashAlgorithm;
	CpaCySymHashAuthModeSetupData *pAuthModeSetupData =
	    &pSessionSetup->hashSetupData.authModeSetupData;

	/* synchronous operation */
	if (NULL == callbackFn) {
		lac_sync_op_data_t *pSyncCallbackData = NULL;

		status = LacSync_CreateSyncCookie(&pSyncCallbackData);

		if (CPA_STATUS_SUCCESS == status) {
			status = LacHash_PrecomputeDataCreate(
			    instanceHandle,
			    pSessionSetup,
			    LacHash_SyncPrecomputeDoneCb,
			    /* wait queue condition from sync cookie */
			    pSyncCallbackData,
			    pWorkingBuffer,
			    pState1,
			    pState2);
		} else {
			return status;
		}

		if (CPA_STATUS_SUCCESS == status) {
			CpaStatus syncStatus = CPA_STATUS_SUCCESS;

			syncStatus = LacSync_WaitForCallback(
			    pSyncCallbackData,
			    LAC_SYM_SYNC_CALLBACK_TIMEOUT,
			    &status,
			    NULL);

			/* If callback doesn't come back */
			if (CPA_STATUS_SUCCESS != syncStatus) {
				QAT_UTILS_LOG(
				    "callback functions for precomputes did not return\n");
				status = syncStatus;
			}
		} else {
			/* As the Request was not sent the Callback will never
			 * be called, so need to indicate that we're finished
			 * with cookie so it can be destroyed. */
			LacSync_SetSyncCookieComplete(pSyncCallbackData);
		}
		LacSync_DestroySyncCookie(&pSyncCallbackData);

		return status;
	}

	/* set up convenience pointers */
	pAuthKey = pAuthModeSetupData->authKey;
	authKeyLenInBytes = pAuthModeSetupData->authKeyLenInBytes;

	/* Pre-compute data state pointers must already be set up
	 * by LacSymQat_HashSetupBlockInit()
	 */

	/* state1 is not allocated for AES XCBC/CCM/GCM/Kasumi/UIA2
	 * so for these algorithms set state2 only */
	if (CPA_CY_SYM_HASH_AES_XCBC == hashAlgorithm) {
		status = LacSymHash_AesECBPreCompute(instanceHandle,
						     hashAlgorithm,
						     authKeyLenInBytes,
						     pAuthKey,
						     pWorkingBuffer,
						     pState2,
						     callbackFn,
						     pCallbackTag);
	} else if (CPA_CY_SYM_HASH_AES_CMAC == hashAlgorithm) {
		/* First, copy the original key to pState2 */
		memcpy(pState2, pAuthKey, authKeyLenInBytes);
		/* Then precompute */
		status = LacSymHash_AesECBPreCompute(instanceHandle,
						     hashAlgorithm,
						     authKeyLenInBytes,
						     pAuthKey,
						     pWorkingBuffer,
						     pState2,
						     callbackFn,
						     pCallbackTag);
	} else if (CPA_CY_SYM_HASH_AES_CCM == hashAlgorithm) {
		/*
		 * The Inner Hash Initial State2 block is 32 bytes long.
		 * Therefore, for keys bigger than 128 bits (16 bytes),
		 * there is no space for 16 zeroes.
		 */
		if (pSessionSetup->cipherSetupData.cipherKeyLenInBytes ==
		    ICP_QAT_HW_AES_128_KEY_SZ) {
			/*
			 * The Inner Hash Initial State2 block must contain K
			 * (the cipher key) and 16 zeroes which will be replaced
			 * with EK(Ctr0) by the QAT-ME.
			 */

			/* write the auth key which for CCM is equivalent to
			 * cipher key
			 */
			memcpy(
			    pState2,
			    pSessionSetup->cipherSetupData.pCipherKey,
			    pSessionSetup->cipherSetupData.cipherKeyLenInBytes);

			/* initialize remaining buffer space to all zeroes */
			LAC_OS_BZERO(pState2 +
					 pSessionSetup->cipherSetupData
					     .cipherKeyLenInBytes,
				     ICP_QAT_HW_AES_CCM_CBC_E_CTR0_SZ);
		}

		/* There is no request sent to the QAT for this operation,
		 * so just invoke the user's callback directly to signal
		 * completion of the precompute
		 */
		callbackFn(pCallbackTag);
	} else if (CPA_CY_SYM_HASH_AES_GCM == hashAlgorithm ||
		   CPA_CY_SYM_HASH_AES_GMAC == hashAlgorithm) {
		/*
		 * The Inner Hash Initial State2 block contains the following
		 *      H (the Galois Hash Multiplier)
		 *      len(A) (the length of A), (length before padding)
		 *      16 zeroes which will be replaced with EK(Ctr0) by the
		 * QAT.
		 */

		/* Memset state2 to 0 */
		LAC_OS_BZERO(pState2,
			     ICP_QAT_HW_GALOIS_H_SZ +
				 ICP_QAT_HW_GALOIS_LEN_A_SZ +
				 ICP_QAT_HW_GALOIS_E_CTR0_SZ);

		/* write H (the Galois Hash Multiplier) where H = E(K, 0...0)
		 * This will only write bytes 0-15 of pState2
		 */
		status = LacSymHash_AesECBPreCompute(
		    instanceHandle,
		    hashAlgorithm,
		    pSessionSetup->cipherSetupData.cipherKeyLenInBytes,
		    pSessionSetup->cipherSetupData.pCipherKey,
		    pWorkingBuffer,
		    pState2,
		    callbackFn,
		    pCallbackTag);

		if (CPA_STATUS_SUCCESS == status) {
			/* write len(A) (the length of A) into bytes 16-19 of
			 * pState2 in big-endian format. This field is 8 bytes
			 */
			*(Cpa32U *)&pState2[ICP_QAT_HW_GALOIS_H_SZ] =
			    LAC_MEM_WR_32(pAuthModeSetupData->aadLenInBytes);
		}
	} else if (CPA_CY_SYM_HASH_KASUMI_F9 == hashAlgorithm) {
		Cpa32U wordIndex = 0;
		Cpa32U *pTempKey = (Cpa32U *)(pState2 + authKeyLenInBytes);
		/*
		 * The Inner Hash Initial State2 block must contain IK
		 * (Initialisation Key), followed by IK XOR-ed with KM
		 * (Key Modifier): IK||(IK^KM).
		 */

		/* write the auth key */
		memcpy(pState2, pAuthKey, authKeyLenInBytes);
		/* initialise temp key with auth key */
		memcpy(pTempKey, pAuthKey, authKeyLenInBytes);

		/* XOR Key with KASUMI F9 key modifier at 4 bytes level */
		for (wordIndex = 0;
		     wordIndex < LAC_BYTES_TO_LONGWORDS(authKeyLenInBytes);
		     wordIndex++) {
			pTempKey[wordIndex] ^=
			    LAC_HASH_KASUMI_F9_KEY_MODIFIER_4_BYTES;
		}
		/* There is no request sent to the QAT for this operation,
		 * so just invoke the user's callback directly to signal
		 * completion of the precompute
		 */
		callbackFn(pCallbackTag);
	} else if (CPA_CY_SYM_HASH_SNOW3G_UIA2 == hashAlgorithm) {
		/*
		 * The Inner Hash Initial State2 should be all zeros
		 */
		LAC_OS_BZERO(pState2, ICP_QAT_HW_SNOW_3G_UIA2_STATE2_SZ);

		/* There is no request sent to the QAT for this operation,
		 * so just invoke the user's callback directly to signal
		 * completion of the precompute
		 */
		callbackFn(pCallbackTag);
	} else if (CPA_CY_SYM_HASH_ZUC_EIA3 == hashAlgorithm) {
		/*
		 * The Inner Hash Initial State2 should contain the key
		 * and zero the rest of the state.
		 */
		LAC_OS_BZERO(pState2, ICP_QAT_HW_ZUC_3G_EIA3_STATE2_SZ);
		memcpy(pState2, pAuthKey, authKeyLenInBytes);

		/* There is no request sent to the QAT for this operation,
		 * so just invoke the user's callback directly to signal
		 * completion of the precompute
		 */
		callbackFn(pCallbackTag);
	} else if (CPA_CY_SYM_HASH_POLY == hashAlgorithm) {
		/* There is no request sent to the QAT for this operation,
		 * so just invoke the user's callback directly to signal
		 * completion of the precompute
		 */
		callbackFn(pCallbackTag);
	} else /* For Hmac Precomputes */
	{
		status = LacSymHash_HmacPreComputes(instanceHandle,
						    hashAlgorithm,
						    authKeyLenInBytes,
						    pAuthKey,
						    pWorkingBuffer,
						    pState1,
						    pState2,
						    callbackFn,
						    pCallbackTag);
	}

	return status;
}

/** @ingroup LacHash */
CpaStatus
LacHash_HashContextCheck(CpaInstanceHandle instanceHandle,
			 const CpaCySymHashSetupData *pHashSetupData)
{
	lac_sym_qat_hash_alg_info_t *pHashAlgInfo = NULL;
	lac_sym_qat_hash_alg_info_t *pOuterHashAlgInfo = NULL;
	CpaCySymCapabilitiesInfo capInfo;

	/*Protect against value of hash outside the bitmap*/
	if (pHashSetupData->hashAlgorithm >= CPA_CY_SYM_HASH_CAP_BITMAP_SIZE) {
		LAC_INVALID_PARAM_LOG("hashAlgorithm");
		return CPA_STATUS_INVALID_PARAM;
	}

	cpaCySymQueryCapabilities(instanceHandle, &capInfo);
	if (!CPA_BITMAP_BIT_TEST(capInfo.hashes,
				 pHashSetupData->hashAlgorithm) &&
	    pHashSetupData->hashAlgorithm != CPA_CY_SYM_HASH_AES_CBC_MAC) {
		LAC_INVALID_PARAM_LOG("hashAlgorithm");
		return CPA_STATUS_INVALID_PARAM;
	}

	switch (pHashSetupData->hashMode) {
	case CPA_CY_SYM_HASH_MODE_PLAIN:
	case CPA_CY_SYM_HASH_MODE_AUTH:
	case CPA_CY_SYM_HASH_MODE_NESTED:
		break;

	default: {
		LAC_INVALID_PARAM_LOG("hashMode");
		return CPA_STATUS_INVALID_PARAM;
	}
	}

	if (LAC_HASH_ALG_MODE_NOT_SUPPORTED(pHashSetupData->hashAlgorithm,
					    pHashSetupData->hashMode)) {
		LAC_UNSUPPORTED_PARAM_LOG(
		    "hashAlgorithm and hashMode combination");
		return CPA_STATUS_UNSUPPORTED;
	}

	LacSymQat_HashAlgLookupGet(instanceHandle,
				   pHashSetupData->hashAlgorithm,
				   &pHashAlgInfo);

	/* note: nested hash mode checks digest length against outer algorithm
	 */
	if ((CPA_CY_SYM_HASH_MODE_PLAIN == pHashSetupData->hashMode) ||
	    (CPA_CY_SYM_HASH_MODE_AUTH == pHashSetupData->hashMode)) {
		/* Check Digest Length is permitted by the algorithm  */
		if ((0 == pHashSetupData->digestResultLenInBytes) ||
		    (pHashSetupData->digestResultLenInBytes >
		     pHashAlgInfo->digestLength)) {
			LAC_INVALID_PARAM_LOG("digestResultLenInBytes");
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	if (CPA_CY_SYM_HASH_MODE_AUTH == pHashSetupData->hashMode) {
		if (CPA_CY_SYM_HASH_AES_GCM == pHashSetupData->hashAlgorithm ||
		    CPA_CY_SYM_HASH_AES_GMAC == pHashSetupData->hashAlgorithm) {
			Cpa32U aadDataSize = 0;

			/* RFC 4106: Implementations MUST support a full-length
			 * 16-octet ICV, and MAY support 8 or 12 octet ICVs, and
			 * MUST NOT support other ICV lengths. */
			if ((pHashSetupData->digestResultLenInBytes !=
			     LAC_HASH_AES_GCM_ICV_SIZE_8) &&
			    (pHashSetupData->digestResultLenInBytes !=
			     LAC_HASH_AES_GCM_ICV_SIZE_12) &&
			    (pHashSetupData->digestResultLenInBytes !=
			     LAC_HASH_AES_GCM_ICV_SIZE_16)) {
				LAC_INVALID_PARAM_LOG("digestResultLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}

			/* ensure aadLen is within maximum limit imposed by QAT
			 */
			aadDataSize =
			    pHashSetupData->authModeSetupData.aadLenInBytes;

			/* round the aad size to the multiple of GCM hash block
			 * size. */
			aadDataSize =
			    LAC_ALIGN_POW2_ROUNDUP(aadDataSize,
						   LAC_HASH_AES_GCM_BLOCK_SIZE);

			if (aadDataSize > ICP_QAT_FW_CCM_GCM_AAD_SZ_MAX &&
			    CPA_CY_SYM_HASH_AES_GMAC !=
				pHashSetupData->hashAlgorithm) {
				LAC_INVALID_PARAM_LOG("aadLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (CPA_CY_SYM_HASH_AES_CCM ==
			   pHashSetupData->hashAlgorithm) {
			Cpa32U aadDataSize = 0;

			/* RFC 3610: Valid values are 4, 6, 8, 10, 12, 14, and
			 * 16 octets */
			if ((pHashSetupData->digestResultLenInBytes >=
			     LAC_HASH_AES_CCM_ICV_SIZE_MIN) &&
			    (pHashSetupData->digestResultLenInBytes <=
			     LAC_HASH_AES_CCM_ICV_SIZE_MAX)) {
				if ((pHashSetupData->digestResultLenInBytes &
				     0x01) != 0) {
					LAC_INVALID_PARAM_LOG(
					    "digestResultLenInBytes must be a multiple of 2");
					return CPA_STATUS_INVALID_PARAM;
				}
			} else {
				LAC_INVALID_PARAM_LOG("digestResultLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}

			/* ensure aadLen is within maximum limit imposed by QAT
			 */
			/* at the beginning of the buffer there is B0 block */
			aadDataSize = LAC_HASH_AES_CCM_BLOCK_SIZE;

			/* then, if there is some 'a' data, the buffer will
			 * store encoded length of 'a' and 'a' itself */
			if (pHashSetupData->authModeSetupData.aadLenInBytes >
			    0) {
				/* as the QAT API puts the requirement on the
				 * pAdditionalAuthData not to be bigger than 240
				 * bytes then we just need 2 bytes to store
				 * encoded length of 'a' */
				aadDataSize += sizeof(Cpa16U);
				aadDataSize += pHashSetupData->authModeSetupData
						   .aadLenInBytes;
			}

			/* round the aad size to the multiple of CCM block
			 * size.*/
			aadDataSize =
			    LAC_ALIGN_POW2_ROUNDUP(aadDataSize,
						   LAC_HASH_AES_CCM_BLOCK_SIZE);
			if (aadDataSize > ICP_QAT_FW_CCM_GCM_AAD_SZ_MAX) {
				LAC_INVALID_PARAM_LOG("aadLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (CPA_CY_SYM_HASH_KASUMI_F9 ==
			   pHashSetupData->hashAlgorithm) {
			/* QAT-FW only supports 128 bit Integrity Key size for
			 * Kasumi f9
			 *  Ref: 3GPP TS 35.201 version 7.0.0 Release 7 */
			if (pHashSetupData->authModeSetupData
				.authKeyLenInBytes !=
			    ICP_QAT_HW_KASUMI_KEY_SZ) {
				LAC_INVALID_PARAM_LOG("authKeyLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (CPA_CY_SYM_HASH_SNOW3G_UIA2 ==
			   pHashSetupData->hashAlgorithm) {

			/* QAT-FW only supports 128 bits Integrity Key size for
			 * Snow3g */
			if (pHashSetupData->authModeSetupData
				.authKeyLenInBytes !=
			    ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ) {
				LAC_INVALID_PARAM_LOG("authKeyLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
			/* For Snow3g hash aad field contains IV - it needs to
			 * be 16 bytes long
			 */
			if (pHashSetupData->authModeSetupData.aadLenInBytes !=
			    ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ) {
				LAC_INVALID_PARAM_LOG("aadLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (CPA_CY_SYM_HASH_AES_XCBC ==
			       pHashSetupData->hashAlgorithm ||
			   CPA_CY_SYM_HASH_AES_CMAC ==
			       pHashSetupData->hashAlgorithm ||
			   CPA_CY_SYM_HASH_AES_CBC_MAC ==
			       pHashSetupData->hashAlgorithm) {
			/* ensure auth key len is valid (128-bit keys supported)
			 */
			if ((pHashSetupData->authModeSetupData
				 .authKeyLenInBytes !=
			     ICP_QAT_HW_AES_128_KEY_SZ)) {
				LAC_INVALID_PARAM_LOG("authKeyLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (CPA_CY_SYM_HASH_ZUC_EIA3 ==
			   pHashSetupData->hashAlgorithm) {

			/* QAT-FW only supports 128 bits Integrity Key size for
			 * ZUC */
			if (pHashSetupData->authModeSetupData
				.authKeyLenInBytes !=
			    ICP_QAT_HW_ZUC_3G_EEA3_KEY_SZ) {
				LAC_INVALID_PARAM_LOG("authKeyLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
			/* For ZUC EIA3 hash aad field contains IV - it needs to
			 * be 16 bytes long
			 */
			if (pHashSetupData->authModeSetupData.aadLenInBytes !=
			    ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ) {
				LAC_INVALID_PARAM_LOG("aadLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (CPA_CY_SYM_HASH_POLY ==
			   pHashSetupData->hashAlgorithm) {
			if (pHashSetupData->digestResultLenInBytes !=
			    ICP_QAT_HW_SPC_CTR_SZ) {
				LAC_INVALID_PARAM_LOG("Digest Length for CCP");
				return CPA_STATUS_INVALID_PARAM;
			}
			if (pHashSetupData->authModeSetupData.aadLenInBytes >
			    ICP_QAT_FW_CCM_GCM_AAD_SZ_MAX) {
				LAC_INVALID_PARAM_LOG("AAD Length for CCP");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else {
			/* The key size must be less than or equal the block
			 * length */
			if (pHashSetupData->authModeSetupData
				.authKeyLenInBytes >
			    pHashAlgInfo->blockLength) {
				LAC_INVALID_PARAM_LOG("authKeyLenInBytes");
				return CPA_STATUS_INVALID_PARAM;
			}
		}

		/* when the key size is greater than 0 check pointer is not null
		 */
		if (CPA_CY_SYM_HASH_AES_CCM != pHashSetupData->hashAlgorithm &&
		    CPA_CY_SYM_HASH_AES_GCM != pHashSetupData->hashAlgorithm &&
		    pHashSetupData->authModeSetupData.authKeyLenInBytes > 0) {
			LAC_CHECK_NULL_PARAM(
			    pHashSetupData->authModeSetupData.authKey);
		}
	} else if (CPA_CY_SYM_HASH_MODE_NESTED == pHashSetupData->hashMode) {
		if (!CPA_BITMAP_BIT_TEST(capInfo.hashes,
					 pHashSetupData->nestedModeSetupData
					     .outerHashAlgorithm)) {
			LAC_INVALID_PARAM_LOG("outerHashAlgorithm");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (LAC_HASH_ALG_MODE_NOT_SUPPORTED(
			pHashSetupData->nestedModeSetupData.outerHashAlgorithm,
			pHashSetupData->hashMode)) {
			LAC_INVALID_PARAM_LOG(
			    "outerHashAlgorithm and hashMode combination");
			return CPA_STATUS_INVALID_PARAM;
		}

		LacSymQat_HashAlgLookupGet(
		    instanceHandle,
		    pHashSetupData->nestedModeSetupData.outerHashAlgorithm,
		    &pOuterHashAlgInfo);

		/* Check Digest Length is permitted by the algorithm  */
		if ((0 == pHashSetupData->digestResultLenInBytes) ||
		    (pHashSetupData->digestResultLenInBytes >
		     pOuterHashAlgInfo->digestLength)) {
			LAC_INVALID_PARAM_LOG("digestResultLenInBytes");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (pHashSetupData->nestedModeSetupData.innerPrefixLenInBytes >
		    LAC_MAX_INNER_OUTER_PREFIX_SIZE_BYTES) {
			LAC_INVALID_PARAM_LOG("innerPrefixLenInBytes");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (pHashSetupData->nestedModeSetupData.innerPrefixLenInBytes >
		    0) {
			LAC_CHECK_NULL_PARAM(pHashSetupData->nestedModeSetupData
						 .pInnerPrefixData);
		}

		if (pHashSetupData->nestedModeSetupData.outerPrefixLenInBytes >
		    LAC_MAX_INNER_OUTER_PREFIX_SIZE_BYTES) {
			LAC_INVALID_PARAM_LOG("outerPrefixLenInBytes");
			return CPA_STATUS_INVALID_PARAM;
		}

		if (pHashSetupData->nestedModeSetupData.outerPrefixLenInBytes >
		    0) {
			LAC_CHECK_NULL_PARAM(pHashSetupData->nestedModeSetupData
						 .pOuterPrefixData);
		}
	}

	return CPA_STATUS_SUCCESS;
}

/** @ingroup LacHash */
CpaStatus
LacHash_PerformParamCheck(CpaInstanceHandle instanceHandle,
			  lac_session_desc_t *pSessionDesc,
			  const CpaCySymOpData *pOpData,
			  Cpa64U srcPktSize,
			  const CpaBoolean *pVerifyResult)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	lac_sym_qat_hash_alg_info_t *pHashAlgInfo = NULL;
	CpaBoolean digestIsAppended = pSessionDesc->digestIsAppended;
	CpaBoolean digestVerify = pSessionDesc->digestVerify;
	CpaCySymOp symOperation = pSessionDesc->symOperation;
	CpaCySymHashAlgorithm hashAlgorithm = pSessionDesc->hashAlgorithm;

	/* digestVerify and digestIsAppended on Hash-Only operation not
	 * supported */
	if (digestIsAppended && digestVerify &&
	    (CPA_CY_SYM_OP_HASH == symOperation)) {
		LAC_INVALID_PARAM_LOG(
		    "digestVerify and digestIsAppended set "
		    "on Hash-Only operation is not supported");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* check the digest result pointer */
	if ((CPA_CY_SYM_PACKET_TYPE_PARTIAL != pOpData->packetType) &&
	    !digestIsAppended && (NULL == pOpData->pDigestResult)) {
		LAC_INVALID_PARAM_LOG("pDigestResult is NULL");
		return CPA_STATUS_INVALID_PARAM;
	}

	/*
	 * Check if the pVerifyResult pointer is not null for hash operation
	 * when the packet is the last one and user has set verifyDigest flag
	 * Also, this is only needed for symchronous operation, so check if the
	 * callback pointer is the internal synchronous one rather than a user-
	 * supplied one.
	 */
	if ((CPA_TRUE == digestVerify) &&
	    (CPA_CY_SYM_PACKET_TYPE_PARTIAL != pOpData->packetType) &&
	    (LacSync_GenBufListVerifyCb == pSessionDesc->pSymCb)) {
		if (NULL == pVerifyResult) {
			LAC_INVALID_PARAM_LOG(
			    "Null pointer pVerifyResult for hash op");
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	/* verify start offset + messageLenToDigest is inside the source packet.
	 * this also verifies that the start offset is inside the packet
	 * Note: digest is specified as a pointer therefore it can be
	 * written anywhere so we cannot check for this been inside a buffer
	 * CCM/GCM specify the auth region using just the cipher params as this
	 * region is the same for auth and cipher. It is not checked here */
	if ((CPA_CY_SYM_HASH_AES_CCM == hashAlgorithm) ||
	    (CPA_CY_SYM_HASH_AES_GCM == hashAlgorithm)) {
		/* ensure AAD data pointer is non-NULL if AAD len > 0 */
		if ((pSessionDesc->aadLenInBytes > 0) &&
		    (NULL == pOpData->pAdditionalAuthData)) {
			LAC_INVALID_PARAM_LOG("pAdditionalAuthData is NULL");
			return CPA_STATUS_INVALID_PARAM;
		}
	} else {
		if ((pOpData->hashStartSrcOffsetInBytes +
		     pOpData->messageLenToHashInBytes) > srcPktSize) {
			LAC_INVALID_PARAM_LOG(
			    "hashStartSrcOffsetInBytes + "
			    "messageLenToHashInBytes > Src Buffer Packet Length");
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	/* For Snow3g & ZUC hash pAdditionalAuthData field
	 * of OpData should contain IV */
	if ((CPA_CY_SYM_HASH_SNOW3G_UIA2 == hashAlgorithm) ||
	    (CPA_CY_SYM_HASH_ZUC_EIA3 == hashAlgorithm)) {
		if (NULL == pOpData->pAdditionalAuthData) {
			LAC_INVALID_PARAM_LOG("pAdditionalAuthData is NULL");
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	/* partial packets need to be multiples of the algorithm block size in
	 * hash only mode (except for final partial packet) */
	if ((CPA_CY_SYM_PACKET_TYPE_PARTIAL == pOpData->packetType) &&
	    (CPA_CY_SYM_OP_HASH == symOperation)) {
		LacSymQat_HashAlgLookupGet(instanceHandle,
					   hashAlgorithm,
					   &pHashAlgInfo);

		/* check if the message is a multiple of the block size. */
		if (pOpData->messageLenToHashInBytes %
			pHashAlgInfo->blockLength !=
		    0) {
			LAC_INVALID_PARAM_LOG2(
			    "message(%d) not block-size(%d) multiple",
			    pOpData->messageLenToHashInBytes,
			    pHashAlgInfo->blockLength);
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	return status;
}

