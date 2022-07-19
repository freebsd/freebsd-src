/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 ***************************************************************************
 * @file lac_sym_cipher.c   Cipher
 *
 * @ingroup LacCipher
 *
 * @description Functions specific to cipher
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_cy_sym.h"

#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"

#include "icp_qat_fw_la.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "lac_sym_cipher.h"
#include "lac_session.h"
#include "lac_mem.h"
#include "lac_common.h"
#include "lac_list.h"
#include "lac_sym.h"
#include "lac_sym_key.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sal_types_crypto.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"
#include "lac_sym_cipher_defs.h"
#include "lac_sym_cipher.h"
#include "lac_sym_stats.h"
#include "lac_sym.h"
#include "lac_sym_qat_cipher.h"
#include "lac_log.h"
#include "lac_buffer_desc.h"

/*
*******************************************************************************
* Static Variables
*******************************************************************************
*/

CpaStatus
LacCipher_PerformIvCheck(sal_service_t *pService,
			 lac_sym_bulk_cookie_t *pCbCookie,
			 Cpa32U qatPacketType,
			 Cpa8U **ppIvBuffer)
{
	const CpaCySymOpData *pOpData = pCbCookie->pOpData;
	lac_session_desc_t *pSessionDesc =
	    LAC_SYM_SESSION_DESC_FROM_CTX_GET(pOpData->sessionCtx);
	CpaCySymCipherAlgorithm algorithm = pSessionDesc->cipherAlgorithm;

	/* Perform IV check. */
	if (LAC_CIPHER_IS_CTR_MODE(algorithm) ||
	    LAC_CIPHER_IS_CBC_MODE(algorithm) ||
	    LAC_CIPHER_IS_AES_F8(algorithm) ||
	    LAC_CIPHER_IS_XTS_MODE(algorithm)) {
		unsigned ivLenInBytes =
		    LacSymQat_CipherIvSizeBytesGet(algorithm);
		LAC_CHECK_NULL_PARAM(pOpData->pIv);
		if (pOpData->ivLenInBytes != ivLenInBytes) {
			if (!(/* GCM with 12 byte IV is OK */
			      (LAC_CIPHER_IS_GCM(algorithm) &&
			       pOpData->ivLenInBytes ==
				   LAC_CIPHER_IV_SIZE_GCM_12) ||
			      /* IV len for CCM has been checked before */
			      LAC_CIPHER_IS_CCM(algorithm))) {
				LAC_INVALID_PARAM_LOG("invalid cipher IV size");
				return CPA_STATUS_INVALID_PARAM;
			}
		}

		/* Always copy the user's IV into another cipher state buffer if
		 * the request is part of a partial packet sequence
		 *      (ensures that pipelined partial requests use same
		 * buffer)
		 */
		if (ICP_QAT_FW_LA_PARTIAL_NONE == qatPacketType) {
			/* Set the value of the ppIvBuffer to that supplied
			 * by the user.
			 * NOTE: There is no guarantee that this address is
			 * aligned on
			 * an 8 or 64 Byte address. */
			*ppIvBuffer = pOpData->pIv;
		} else {
			/* For partial packets, we use a per-session buffer to
			 * maintain
			 * the IV.  This allows us to easily pass the updated IV
			 * forward
			 * to the next partial in the sequence.  This makes
			 * internal
			 * buffering of partials easier to implement.
			 */
			*ppIvBuffer = pSessionDesc->cipherPartialOpState;

			/* Ensure that the user's IV buffer gets updated between
			 * partial
			 * requests so that they may also see the residue from
			 * the
			 * previous partial.  Not needed for final partials
			 * though.
			 */
			if ((ICP_QAT_FW_LA_PARTIAL_START == qatPacketType) ||
			    (ICP_QAT_FW_LA_PARTIAL_MID == qatPacketType)) {
				pCbCookie->updateUserIvOnRecieve = CPA_TRUE;

				if (ICP_QAT_FW_LA_PARTIAL_START ==
				    qatPacketType) {
					/* if the previous partial state was
					 * full, then this is
					 * the first partial in the sequence so
					 * we need to copy
					 * in the user's IV. But, we have to be
					 * very careful
					 * here not to overwrite the
					 * cipherPartialOpState just
					 * yet in case there's a previous
					 * partial sequence in
					 * flight, so we defer the copy for now.
					 * This will be
					 * completed in the
					 * LacSymQueue_RequestSend() function.
					 */
					pCbCookie->updateSessionIvOnSend =
					    CPA_TRUE;
				}
				/* For subsequent partials in a sequence, we'll
				 * re-use the
				 * IV that was written back by the QAT, using
				 * internal
				 * request queueing if necessary to ensure that
				 * the next
				 * partial request isn't issued to the QAT until
				 * the
				 * previous one completes
				 */
			}
		}
	} else if (LAC_CIPHER_IS_KASUMI(algorithm)) {
		LAC_CHECK_NULL_PARAM(pOpData->pIv);

		if (LAC_CIPHER_IS_KASUMI(algorithm) &&
		    (pOpData->ivLenInBytes != LAC_CIPHER_KASUMI_F8_IV_LENGTH)) {
			LAC_INVALID_PARAM_LOG("invalid cipher IV size");
			return CPA_STATUS_INVALID_PARAM;
		}


		*ppIvBuffer = pOpData->pIv;
	} else if (LAC_CIPHER_IS_SNOW3G_UEA2(algorithm)) {
		LAC_CHECK_NULL_PARAM(pOpData->pIv);
		if (LAC_CIPHER_IS_SNOW3G_UEA2(algorithm) &&
		    (pOpData->ivLenInBytes != ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ)) {
			LAC_INVALID_PARAM_LOG("invalid cipher IV size");
			return CPA_STATUS_INVALID_PARAM;
		}
		*ppIvBuffer = pOpData->pIv;
	} else if (LAC_CIPHER_IS_ARC4(algorithm)) {
		if (ICP_QAT_FW_LA_PARTIAL_NONE == qatPacketType) {
			/* For full packets, the initial ARC4 state is stored in
			 * the
			 * session descriptor.  Use it directly.
			 */
			*ppIvBuffer = pSessionDesc->cipherARC4InitialState;
		} else {
			/* For partial packets, we maintain the running ARC4
			 * state in
			 * dedicated buffer in the session descriptor
			 */
			*ppIvBuffer = pSessionDesc->cipherPartialOpState;

			if (ICP_QAT_FW_LA_PARTIAL_START == qatPacketType) {
				/* if the previous partial state was full, then
				 * this is the
				 * first partial in the sequence so we need to
				 * (re-)initialise
				 * the contents of the state buffer using the
				 * initial state
				 * that is stored in the session descriptor.
				 * But, we have to be
				 * very careful here not to overwrite the
				 * cipherPartialOpState
				 * just yet in case there's a previous partial
				 * sequence in
				 * flight, so we defer the copy for now. This
				 * will be completed
				 * in the LacSymQueue_RequestSend() function
				 * when clear to send.
				 */
				pCbCookie->updateSessionIvOnSend = CPA_TRUE;
			}
		}
	} else if (LAC_CIPHER_IS_ZUC_EEA3(algorithm)) {
		LAC_CHECK_NULL_PARAM(pOpData->pIv);
		if (pOpData->ivLenInBytes != ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ) {
			LAC_INVALID_PARAM_LOG("invalid cipher IV size");
			return CPA_STATUS_INVALID_PARAM;
		}
		*ppIvBuffer = pOpData->pIv;
	} else {
		*ppIvBuffer = NULL;
	}

	return CPA_STATUS_SUCCESS;
}


CpaStatus
LacCipher_SessionSetupDataCheck(const CpaCySymCipherSetupData *pCipherSetupData)
{
	/* No key required for NULL algorithm */
	if (!LAC_CIPHER_IS_NULL(pCipherSetupData->cipherAlgorithm)) {
		LAC_CHECK_NULL_PARAM(pCipherSetupData->pCipherKey);

		/* Check that algorithm and keys passed in are correct size */
		if (LAC_CIPHER_IS_ARC4(pCipherSetupData->cipherAlgorithm)) {
			if (pCipherSetupData->cipherKeyLenInBytes >
			    ICP_QAT_HW_ARC4_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid ARC4 cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_CCM(
			       pCipherSetupData->cipherAlgorithm)) {
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_AES_128_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid AES CCM cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_XTS_MODE(
			       pCipherSetupData->cipherAlgorithm)) {
			if ((pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_128_XTS_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_256_XTS_KEY_SZ)) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid AES XTS cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_AES(
			       pCipherSetupData->cipherAlgorithm)) {
			if ((pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_128_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_192_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_256_KEY_SZ)) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid AES cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_AES_F8(
			       pCipherSetupData->cipherAlgorithm)) {
			if ((pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_128_F8_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_192_F8_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_256_F8_KEY_SZ)) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid AES cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_DES(
			       pCipherSetupData->cipherAlgorithm)) {
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_DES_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid DES cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_TRIPLE_DES(
			       pCipherSetupData->cipherAlgorithm)) {
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_3DES_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid Triple-DES cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_KASUMI(
			       pCipherSetupData->cipherAlgorithm)) {
			/* QAT-FW only supports 128 bits Cipher Key size for
			 * Kasumi F8
			 *  Ref: 3GPP TS 55.216 V6.2.0 */
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_KASUMI_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid Kasumi cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_SNOW3G_UEA2(
			       pCipherSetupData->cipherAlgorithm)) {
			/* QAT-FW only supports 256 bits Cipher Key size for
			 * Snow_3G */
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid Snow_3G cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_ZUC_EEA3(
			       pCipherSetupData->cipherAlgorithm)) {
			/* ZUC EEA3 */
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_ZUC_3G_EEA3_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid ZUC cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_CHACHA(
			       pCipherSetupData->cipherAlgorithm)) {
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_CHACHAPOLY_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid CHACHAPOLY cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else if (LAC_CIPHER_IS_SM4(
			       pCipherSetupData->cipherAlgorithm)) {
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_SM4_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid SM4 cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
		} else {
			LAC_INVALID_PARAM_LOG("Invalid cipher algorithm");
			return CPA_STATUS_INVALID_PARAM;
		}
	}
	return CPA_STATUS_SUCCESS;
}

CpaStatus
LacCipher_PerformParamCheck(CpaCySymCipherAlgorithm algorithm,
			    const CpaCySymOpData *pOpData,
			    const Cpa64U packetLen)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	/* The following check will cover the dstBuffer as well, since
	 * the dstBuffer cannot be smaller than the srcBuffer (checked in
	 * LacSymPerform_BufferParamCheck() called from LacSym_Perform())
	 */
	if ((pOpData->messageLenToCipherInBytes +
	     pOpData->cryptoStartSrcOffsetInBytes) > packetLen) {
		LAC_INVALID_PARAM_LOG("cipher len + offset greater than "
				      "srcBuffer packet len");
		status = CPA_STATUS_INVALID_PARAM;
	}

	if (CPA_STATUS_SUCCESS == status) {
		/*
		 * XTS Mode allow for ciphers which are not multiples of
		 * the block size.
		 */
		/* Perform algorithm-specific checks */
		if (LAC_CIPHER_IS_XTS_MODE(algorithm) &&
		    ((pOpData->packetType == CPA_CY_SYM_PACKET_TYPE_FULL) ||
		     (pOpData->packetType ==
		      CPA_CY_SYM_PACKET_TYPE_LAST_PARTIAL))) {
			/*
			 * If this is the last of a partial request
			 */
			if (pOpData->messageLenToCipherInBytes <
			    ICP_QAT_HW_AES_BLK_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "data size must be greater than block "
				    "size for last XTS partial or XTS "
				    "full packet");
				status = CPA_STATUS_INVALID_PARAM;
			}
		} else if (!(LAC_CIPHER_IS_ARC4(algorithm) ||
			     LAC_CIPHER_IS_CTR_MODE(algorithm) ||
			     LAC_CIPHER_IS_F8_MODE(algorithm) ||
			     LAC_CIPHER_IS_SNOW3G_UEA2(algorithm) ||
			     LAC_CIPHER_IS_XTS_MODE(algorithm) ||
			     LAC_CIPHER_IS_CHACHA(algorithm) ||
			     LAC_CIPHER_IS_ZUC_EEA3(algorithm))) {
			/* Mask & check below is based on assumption that block
			 * size is
			 * a power of 2. If data size is not a multiple of the
			 * block size,
			 * the "remainder" bits selected by the mask be non-zero
			 */
			if (pOpData->messageLenToCipherInBytes &
			    (LacSymQat_CipherBlockSizeBytesGet(algorithm) -
			     1)) {
				LAC_INVALID_PARAM_LOG(
				    "data size must be block size multiple");
				status = CPA_STATUS_INVALID_PARAM;
			}
		}
	}

	return status;
}

