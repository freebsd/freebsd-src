/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_auth_enc.c
 *
 * @ingroup LacAuthEnc
 *
 * @description
 *  Authenticated encryption specific functionality.
 *  For CCM related code NIST SP 800-38C is followed.
 *  For GCM related code NIST SP 800-38D is followed.
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
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
#include "lac_log.h"
#include "lac_common.h"
#include "lac_session.h"
#include "lac_sym_auth_enc.h"

/* These defines describe position of the flag fields
 * in B0 block for CCM algorithm*/
#define LAC_ALG_CHAIN_CCM_B0_FLAGS_ADATA_SHIFT 6
#define LAC_ALG_CHAIN_CCM_B0_FLAGS_T_SHIFT 3

/* This macro builds flags field to be put in B0 block for CCM algorithm */
#define LAC_ALG_CHAIN_CCM_BUILD_B0_FLAGS(Adata, t, q)                          \
	((((Adata) > 0 ? 1 : 0) << LAC_ALG_CHAIN_CCM_B0_FLAGS_ADATA_SHIFT) |   \
	 ((((t)-2) >> 1) << LAC_ALG_CHAIN_CCM_B0_FLAGS_T_SHIFT) | ((q)-1))

/**
 * @ingroup LacAuthEnc
 */
CpaStatus
LacSymAlgChain_CheckCCMData(Cpa8U *pAdditionalAuthData,
			    Cpa8U *pIv,
			    Cpa32U messageLenToCipherInBytes,
			    Cpa32U ivLenInBytes)
{
	Cpa8U q = 0;

	LAC_CHECK_NULL_PARAM(pIv);
	LAC_CHECK_NULL_PARAM(pAdditionalAuthData);

	/* check if n is within permitted range */
	if (ivLenInBytes < LAC_ALG_CHAIN_CCM_N_LEN_IN_BYTES_MIN ||
	    ivLenInBytes > LAC_ALG_CHAIN_CCM_N_LEN_IN_BYTES_MAX) {
		LAC_INVALID_PARAM_LOG2("ivLenInBytes for CCM algorithm  "
				       "must be between %d and %d inclusive",
				       LAC_ALG_CHAIN_CCM_N_LEN_IN_BYTES_MIN,
				       LAC_ALG_CHAIN_CCM_N_LEN_IN_BYTES_MAX);
		return CPA_STATUS_INVALID_PARAM;
	}

	q = LAC_ALG_CHAIN_CCM_NQ_CONST - ivLenInBytes;

	/* Check if q is big enough to hold actual length of message to cipher
	 * if q = 8 -> maxlen = 2^64 always good as
	 * messageLenToCipherInBytes is 32 bits
	 * if q = 7 -> maxlen = 2^56 always good
	 * if q = 6 -> maxlen = 2^48 always good
	 * if q = 5 -> maxlen = 2^40 always good
	 * if q = 4 -> maxlen = 2^32 always good.
	 */
	if ((messageLenToCipherInBytes >= (1 << (q * LAC_NUM_BITS_IN_BYTE))) &&
	    (q < sizeof(Cpa32U))) {
		LAC_INVALID_PARAM_LOG(
		    "messageLenToCipherInBytes too long for the given"
		    " ivLenInBytes for CCM algorithm\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	return CPA_STATUS_SUCCESS;
}


/**
 * @ingroup LacAuthEnc
 */
void
LacSymAlgChain_PrepareCCMData(lac_session_desc_t *pSessionDesc,
			      Cpa8U *pAdditionalAuthData,
			      Cpa8U *pIv,
			      Cpa32U messageLenToCipherInBytes,
			      Cpa32U ivLenInBytes)
{
	Cpa8U n =
	    ivLenInBytes; /* assumes ivLenInBytes has been param checked */
	Cpa8U q = LAC_ALG_CHAIN_CCM_NQ_CONST - n;
	Cpa8U lenOfEncodedLen = 0;
	Cpa16U lenAEncoded = 0;
	Cpa32U bitStrQ = 0;

	/* populate Ctr0 block - stored in pIv */
	pIv[0] = (q - 1);
	/* bytes 1 to n are already set with nonce by the user */
	/* set last q bytes with 0 */
	memset(pIv + n + 1, 0, q);

	/* Encode the length of associated data 'a'. As the API limits the
	 * length
	 * of an array pointed by pAdditionalAuthData to be 240 bytes max, the
	 * maximum length of 'a' might be 240 - 16 - 2 = 222. Hence the encoding
	 * below is simplified. */
	if (pSessionDesc->aadLenInBytes > 0) {
		lenOfEncodedLen = sizeof(Cpa16U);
		lenAEncoded = QAT_UTILS_HOST_TO_NW_16(
		    (Cpa16U)pSessionDesc->aadLenInBytes);
	}

	/* populate B0 block */
	/* first, set the flags field */
	pAdditionalAuthData[0] =
	    LAC_ALG_CHAIN_CCM_BUILD_B0_FLAGS(lenOfEncodedLen,
					     pSessionDesc->hashResultSize,
					     q);
	/* bytes 1 to n are already set with nonce by the user*/
	/* put Q in bytes 16-q...15 */
	bitStrQ = QAT_UTILS_HOST_TO_NW_32(messageLenToCipherInBytes);

	if (q > sizeof(bitStrQ)) {
		memset(pAdditionalAuthData + n + 1, 0, q);
		memcpy(pAdditionalAuthData + n + 1 + (q - sizeof(bitStrQ)),
		       (Cpa8U *)&bitStrQ,
		       sizeof(bitStrQ));
	} else {
		memcpy(pAdditionalAuthData + n + 1,
		       ((Cpa8U *)&bitStrQ) + (sizeof(bitStrQ) - q),
		       q);
	}

	/* populate B1-Bn blocks */
	if (lenAEncoded > 0) {
		*(Cpa16U
		      *)(&pAdditionalAuthData[1 + LAC_ALG_CHAIN_CCM_NQ_CONST]) =
		    lenAEncoded;
		/* Next bytes are already set by the user with
		 * the associated data 'a' */

		/* Check if padding is required */
		if (((pSessionDesc->aadLenInBytes + lenOfEncodedLen) %
		     LAC_HASH_AES_CCM_BLOCK_SIZE) != 0) {
			Cpa8U paddingLen = 0;
			Cpa8U paddingIndex = 0;

			paddingLen = LAC_HASH_AES_CCM_BLOCK_SIZE -
			    ((pSessionDesc->aadLenInBytes + lenOfEncodedLen) %
			     LAC_HASH_AES_CCM_BLOCK_SIZE);

			paddingIndex = 1 + LAC_ALG_CHAIN_CCM_NQ_CONST;
			paddingIndex +=
			    lenOfEncodedLen + pSessionDesc->aadLenInBytes;

			memset(&pAdditionalAuthData[paddingIndex],
			       0,
			       paddingLen);
		}
	}
}

/**
 * @ingroup LacAuthEnc
 */
void
LacSymAlgChain_PrepareGCMData(lac_session_desc_t *pSessionDesc,
			      Cpa8U *pAdditionalAuthData)
{
	Cpa8U paddingLen = 0;

	if ((pSessionDesc->aadLenInBytes % LAC_HASH_AES_GCM_BLOCK_SIZE) != 0) {
		paddingLen = LAC_HASH_AES_GCM_BLOCK_SIZE -
		    (pSessionDesc->aadLenInBytes % LAC_HASH_AES_GCM_BLOCK_SIZE);

		memset(&pAdditionalAuthData[pSessionDesc->aadLenInBytes],
		       0,
		       paddingLen);
	}
}
