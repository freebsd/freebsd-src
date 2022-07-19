/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 ***************************************************************************
 * @file lac_sym_hash_sw_precomputes.c
 *
 * @ingroup LacHashDefs
 *
 * Hash Software
 ***************************************************************************/

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"

#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_adf_debug.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

#include "qat_utils.h"
#include "lac_mem.h"
#include "lac_sym.h"
#include "lac_log.h"
#include "lac_mem_pools.h"
#include "lac_list.h"
#include "lac_sym_hash_defs.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sal_types_crypto.h"
#include "lac_sal.h"
#include "lac_session.h"
#include "lac_sym_hash_precomputes.h"

static CpaStatus
LacSymHash_Compute(CpaCySymHashAlgorithm hashAlgorithm,
		   lac_sym_qat_hash_alg_info_t *pHashAlgInfo,
		   Cpa8U *in,
		   Cpa8U *out)
{
	/*
	 * Note: from SHA hashes appropriate endian swapping is required.
	 * For sha1, sha224 and sha256 double words based swapping.
	 * For sha384 and sha512 quad words swapping.
	 * No endianes swapping for md5 is required.
	 */
	CpaStatus status = CPA_STATUS_FAIL;
	Cpa32U i = 0;
	switch (hashAlgorithm) {
	case CPA_CY_SYM_HASH_MD5:
		if (CPA_STATUS_SUCCESS != qatUtilsHashMD5(in, out)) {
			LAC_LOG_ERROR("qatUtilsHashMD5 Failed\n");
			return status;
		}
		status = CPA_STATUS_SUCCESS;
		break;
	case CPA_CY_SYM_HASH_SHA1:
		if (CPA_STATUS_SUCCESS != qatUtilsHashSHA1(in, out)) {
			LAC_LOG_ERROR("qatUtilsHashSHA1 Failed\n");
			return status;
		}
		for (i = 0; i < LAC_BYTES_TO_LONGWORDS(pHashAlgInfo->stateSize);
		     i++) {
			((Cpa32U *)(out))[i] =
			    LAC_MEM_WR_32(((Cpa32U *)(out))[i]);
		}
		status = CPA_STATUS_SUCCESS;
		break;
	case CPA_CY_SYM_HASH_SHA224:
		if (CPA_STATUS_SUCCESS != qatUtilsHashSHA224(in, out)) {
			LAC_LOG_ERROR("qatUtilsHashSHA224 Failed\n");
			return status;
		}
		for (i = 0; i < LAC_BYTES_TO_LONGWORDS(pHashAlgInfo->stateSize);
		     i++) {
			((Cpa32U *)(out))[i] =
			    LAC_MEM_WR_32(((Cpa32U *)(out))[i]);
		}
		status = CPA_STATUS_SUCCESS;
		break;
	case CPA_CY_SYM_HASH_SHA256:
		if (CPA_STATUS_SUCCESS != qatUtilsHashSHA256(in, out)) {
			LAC_LOG_ERROR("qatUtilsHashSHA256 Failed\n");
			return status;
		}
		for (i = 0; i < LAC_BYTES_TO_LONGWORDS(pHashAlgInfo->stateSize);
		     i++) {
			((Cpa32U *)(out))[i] =
			    LAC_MEM_WR_32(((Cpa32U *)(out))[i]);
		}
		status = CPA_STATUS_SUCCESS;
		break;
	case CPA_CY_SYM_HASH_SHA384:
		if (CPA_STATUS_SUCCESS != qatUtilsHashSHA384(in, out)) {
			LAC_LOG_ERROR("qatUtilsHashSHA384 Failed\n");
			return status;
		}
		for (i = 0; i < LAC_BYTES_TO_QUADWORDS(pHashAlgInfo->stateSize);
		     i++) {
			((Cpa64U *)(out))[i] =
			    LAC_MEM_WR_64(((Cpa64U *)(out))[i]);
		}
		status = CPA_STATUS_SUCCESS;
		break;
	case CPA_CY_SYM_HASH_SHA512:
		if (CPA_STATUS_SUCCESS != qatUtilsHashSHA512(in, out)) {
			LAC_LOG_ERROR("qatUtilsHashSHA512 Failed\n");
			return status;
		}
		for (i = 0; i < LAC_BYTES_TO_QUADWORDS(pHashAlgInfo->stateSize);
		     i++) {
			((Cpa64U *)(out))[i] =
			    LAC_MEM_WR_64(((Cpa64U *)(out))[i]);
		}
		status = CPA_STATUS_SUCCESS;
		break;
	default:
		return CPA_STATUS_INVALID_PARAM;
	}
	return status;
}

CpaStatus
LacSymHash_HmacPreComputes(CpaInstanceHandle instanceHandle,
			   CpaCySymHashAlgorithm hashAlgorithm,
			   Cpa32U authKeyLenInBytes,
			   Cpa8U *pAuthKey,
			   Cpa8U *pWorkingMemory,
			   Cpa8U *pState1,
			   Cpa8U *pState2,
			   lac_hash_precompute_done_cb_t callbackFn,
			   void *pCallbackTag)
{
	Cpa8U *pIpadData = NULL;
	Cpa8U *pOpadData = NULL;
	CpaStatus status = CPA_STATUS_FAIL;
	lac_sym_hash_precomp_op_data_t *pHmacIpadOpData =
	    (lac_sym_hash_precomp_op_data_t *)pWorkingMemory;
	lac_sym_hash_precomp_op_data_t *pHmacOpadOpData = pHmacIpadOpData + 1;

	/* Convenience pointers */
	lac_sym_hash_hmac_precomp_qat_t *pHmacIpadQatData =
	    &pHmacIpadOpData->u.hmacQatData;
	lac_sym_hash_hmac_precomp_qat_t *pHmacOpadQatData =
	    &pHmacOpadOpData->u.hmacQatData;

	lac_sym_qat_hash_alg_info_t *pHashAlgInfo = NULL;
	Cpa32U i = 0;
	Cpa32U padLenBytes = 0;

	LacSymQat_HashAlgLookupGet(instanceHandle,
				   hashAlgorithm,
				   &pHashAlgInfo);
	pHmacIpadOpData->stateSize = pHashAlgInfo->stateSize;
	pHmacOpadOpData->stateSize = pHashAlgInfo->stateSize;

	/* Copy HMAC key into buffers */
	if (authKeyLenInBytes > 0) {
		memcpy(pHmacIpadQatData->data, pAuthKey, authKeyLenInBytes);
		memcpy(pHmacOpadQatData->data, pAuthKey, authKeyLenInBytes);
	}

	padLenBytes = pHashAlgInfo->blockLength - authKeyLenInBytes;

	/* Clear the remaining buffer space */
	if (padLenBytes > 0) {
		LAC_OS_BZERO(pHmacIpadQatData->data + authKeyLenInBytes,
			     padLenBytes);
		LAC_OS_BZERO(pHmacOpadQatData->data + authKeyLenInBytes,
			     padLenBytes);
	}

	/* XOR Key with IPAD at 4-byte level */
	for (i = 0; i < pHashAlgInfo->blockLength; i++) {
		Cpa8U *ipad = pHmacIpadQatData->data + i;
		Cpa8U *opad = pHmacOpadQatData->data + i;

		*ipad ^= LAC_HASH_IPAD_BYTE;
		*opad ^= LAC_HASH_OPAD_BYTE;
	}
	pIpadData = (Cpa8U *)pHmacIpadQatData->data;
	pOpadData = (Cpa8U *)pHmacOpadQatData->data;

	status = LacSymHash_Compute(hashAlgorithm,
				    pHashAlgInfo,
				    (Cpa8U *)pIpadData,
				    pState1);

	if (CPA_STATUS_SUCCESS == status) {
		status = LacSymHash_Compute(hashAlgorithm,
					    pHashAlgInfo,
					    (Cpa8U *)pOpadData,
					    pState2);
	}

	if (CPA_STATUS_SUCCESS == status) {
		callbackFn(pCallbackTag);
	}
	return status;
}

CpaStatus
LacSymHash_AesECBPreCompute(CpaInstanceHandle instanceHandle,
			    CpaCySymHashAlgorithm hashAlgorithm,
			    Cpa32U authKeyLenInBytes,
			    Cpa8U *pAuthKey,
			    Cpa8U *pWorkingMemory,
			    Cpa8U *pState,
			    lac_hash_precompute_done_cb_t callbackFn,
			    void *pCallbackTag)
{
	CpaStatus status = CPA_STATUS_FAIL;
	Cpa32U stateSize = 0, x = 0;
	lac_sym_qat_hash_alg_info_t *pHashAlgInfo = NULL;

	if (CPA_CY_SYM_HASH_AES_XCBC == hashAlgorithm) {
		Cpa8U *in = pWorkingMemory;
		Cpa8U *out = pState;
		LacSymQat_HashAlgLookupGet(instanceHandle,
					   hashAlgorithm,
					   &pHashAlgInfo);
		stateSize = pHashAlgInfo->stateSize;
		memcpy(pWorkingMemory, pHashAlgInfo->initState, stateSize);

		for (x = 0; x < LAC_HASH_XCBC_PRECOMP_KEY_NUM; x++) {
			if (CPA_STATUS_SUCCESS !=
			    qatUtilsAESEncrypt(
				pAuthKey, authKeyLenInBytes, in, out)) {
				return status;
			}
			in += LAC_HASH_XCBC_MAC_BLOCK_SIZE;
			out += LAC_HASH_XCBC_MAC_BLOCK_SIZE;
		}
		status = CPA_STATUS_SUCCESS;
	} else if (CPA_CY_SYM_HASH_AES_CMAC == hashAlgorithm) {
		Cpa8U *out = pState;
		Cpa8U k1[LAC_HASH_CMAC_BLOCK_SIZE],
		    k2[LAC_HASH_CMAC_BLOCK_SIZE];
		Cpa8U *ptr = NULL, i = 0;
		stateSize = LAC_HASH_CMAC_BLOCK_SIZE;
		LacSymQat_HashAlgLookupGet(instanceHandle,
					   hashAlgorithm,
					   &pHashAlgInfo);
		/* Original state size includes K, K1 and K2 which are of equal
		 * length.
		 * For precompute state size is only of the length of K which is
		 * equal
		 * to the block size for CPA_CY_SYM_HASH_AES_CMAC.
		 * The algorithm is described in rfc4493
		 * K is just copeid, K1 and K2 need to be single inplace encrypt
		 * with AES.
		 * */
		memcpy(out, pHashAlgInfo->initState, stateSize);
		memcpy(out, pAuthKey, authKeyLenInBytes);
		out += LAC_HASH_CMAC_BLOCK_SIZE;

		for (x = 0; x < LAC_HASH_XCBC_PRECOMP_KEY_NUM - 1; x++) {
			if (CPA_STATUS_SUCCESS !=
			    qatUtilsAESEncrypt(
				pAuthKey, authKeyLenInBytes, out, out)) {
				return status;
			}
			out += LAC_HASH_CMAC_BLOCK_SIZE;
		}

		ptr = pState + LAC_HASH_CMAC_BLOCK_SIZE;

		/* Derived keys (k1 and k2), copy them to
		 * pPrecompOpData->pState,
		 * but remember that at the beginning is original key (K0)
		 */
		/* Calculating K1 */
		for (i = 0; i < LAC_HASH_CMAC_BLOCK_SIZE; i++, ptr++) {
			k1[i] = (*ptr) << 1;
			if (i != 0) {
				k1[i - 1] |=
				    (*ptr) >> (LAC_NUM_BITS_IN_BYTE - 1);
			}
			if (i + 1 == LAC_HASH_CMAC_BLOCK_SIZE) {
				/* If msb of pState + LAC_HASH_CMAC_BLOCK_SIZE
				   is set xor
				   with RB. Because only the final byte of RB is
				   non-zero
				   this is all we need to xor */
				if ((*(pState + LAC_HASH_CMAC_BLOCK_SIZE)) &
				    LAC_SYM_HASH_MSBIT_MASK) {
					k1[i] ^= LAC_SYM_AES_CMAC_RB_128;
				}
			}
		}

		/* Calculating K2 */
		for (i = 0; i < LAC_HASH_CMAC_BLOCK_SIZE; i++) {
			k2[i] = (k1[i]) << 1;
			if (i != 0) {
				k2[i - 1] |=
				    (k1[i]) >> (LAC_NUM_BITS_IN_BYTE - 1);
			}
			if (i + 1 == LAC_HASH_CMAC_BLOCK_SIZE) {
				/* If msb of k1 is set xor last byte with RB */
				if (k1[0] & LAC_SYM_HASH_MSBIT_MASK) {
					k2[i] ^= LAC_SYM_AES_CMAC_RB_128;
				}
			}
		}
		/* Now, when we have K1 & K2 lets copy them to the state2 */
		ptr = pState + LAC_HASH_CMAC_BLOCK_SIZE;
		memcpy(ptr, k1, LAC_HASH_CMAC_BLOCK_SIZE);
		ptr += LAC_HASH_CMAC_BLOCK_SIZE;
		memcpy(ptr, k2, LAC_HASH_CMAC_BLOCK_SIZE);
		status = CPA_STATUS_SUCCESS;
	} else if (CPA_CY_SYM_HASH_AES_GCM == hashAlgorithm ||
		   CPA_CY_SYM_HASH_AES_GMAC == hashAlgorithm) {
		Cpa8U *in = pWorkingMemory;
		Cpa8U *out = pState;
		LAC_OS_BZERO(pWorkingMemory, ICP_QAT_HW_GALOIS_H_SZ);

		if (CPA_STATUS_SUCCESS !=
		    qatUtilsAESEncrypt(pAuthKey, authKeyLenInBytes, in, out)) {
			return status;
		}
		status = CPA_STATUS_SUCCESS;
	} else {
		return CPA_STATUS_INVALID_PARAM;
	}
	callbackFn(pCallbackTag);
	return status;
}

CpaStatus
LacSymHash_HmacPrecompInit(CpaInstanceHandle instanceHandle)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	return status;
}

void
LacSymHash_HmacPrecompShutdown(CpaInstanceHandle instanceHandle)
{
	return;
}
