/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

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
#include "sal_hw_gen.h"

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
	unsigned ivLenInBytes = 0;

	switch (algorithm) {
	/* Perform IV check for CTR, CBC, XTS, F8 MODE. */
	case CPA_CY_SYM_CIPHER_AES_CTR:
	case CPA_CY_SYM_CIPHER_3DES_CTR:
	case CPA_CY_SYM_CIPHER_SM4_CTR:
	case CPA_CY_SYM_CIPHER_AES_CCM:
	case CPA_CY_SYM_CIPHER_AES_GCM:
	case CPA_CY_SYM_CIPHER_CHACHA:
	case CPA_CY_SYM_CIPHER_AES_CBC:
	case CPA_CY_SYM_CIPHER_DES_CBC:
	case CPA_CY_SYM_CIPHER_3DES_CBC:
	case CPA_CY_SYM_CIPHER_SM4_CBC:
	case CPA_CY_SYM_CIPHER_AES_F8:
	case CPA_CY_SYM_CIPHER_AES_XTS: {
		ivLenInBytes = LacSymQat_CipherIvSizeBytesGet(algorithm);
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
			 * aligned on an 8 or 64 Byte address. */
			*ppIvBuffer = pOpData->pIv;
		} else {
			/* For partial packets, we use a per-session buffer to
			 * maintain the IV.  This allows us to easily pass the
			 * updated IV forward to the next partial in the
			 * sequence.  This makes internal buffering of partials
			 * easier to implement.
			 */
			*ppIvBuffer = pSessionDesc->cipherPartialOpState;

			/* Ensure that the user's IV buffer gets updated between
			 * partial requests so that they may also see the
			 * residue from the previous partial.  Not needed for
			 * final partials though.
			 */
			if ((ICP_QAT_FW_LA_PARTIAL_START == qatPacketType) ||
			    (ICP_QAT_FW_LA_PARTIAL_MID == qatPacketType)) {
				pCbCookie->updateUserIvOnRecieve = CPA_TRUE;

				if (ICP_QAT_FW_LA_PARTIAL_START ==
				    qatPacketType) {
					/* if the previous partial state was
					 * full, then this is the first partial
					 * in the sequence so we need to copy in
					 * the user's IV. But, we have to be
					 * very careful here not to overwrite
					 * the cipherPartialOpState just yet in
					 * case there's a previous partial
					 * sequence in flight, so we defer the
					 * copy for now.  This will be completed
					 * in the LacSymQueue_RequestSend()
					 * function.
					 */
					pCbCookie->updateSessionIvOnSend =
					    CPA_TRUE;
				}
				/* For subsequent partials in a sequence, we'll
				 * re-use the IV that was written back by the
				 * QAT, using internal request queueing if
				 * necessary to ensure that the next partial
				 * request isn't issued to the QAT until the
				 * previous one completes
				 */
			}
		}
	} break;
	case CPA_CY_SYM_CIPHER_KASUMI_F8: {
		LAC_CHECK_NULL_PARAM(pOpData->pIv);

		if (pOpData->ivLenInBytes != LAC_CIPHER_KASUMI_F8_IV_LENGTH) {
			LAC_INVALID_PARAM_LOG("invalid cipher IV size");
			return CPA_STATUS_INVALID_PARAM;
		}

		*ppIvBuffer = pOpData->pIv;
	} break;
	case CPA_CY_SYM_CIPHER_SNOW3G_UEA2: {
		LAC_CHECK_NULL_PARAM(pOpData->pIv);
		if (pOpData->ivLenInBytes != ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ) {
			LAC_INVALID_PARAM_LOG("invalid cipher IV size");
			return CPA_STATUS_INVALID_PARAM;
		}
		*ppIvBuffer = pOpData->pIv;
	} break;
	case CPA_CY_SYM_CIPHER_ARC4: {
		if (ICP_QAT_FW_LA_PARTIAL_NONE == qatPacketType) {
			/* For full packets, the initial ARC4 state is stored in
			 * the session descriptor.  Use it directly.
			 */
			*ppIvBuffer = pSessionDesc->cipherARC4InitialState;
		} else {
			/* For partial packets, we maintain the running ARC4
			 * state in dedicated buffer in the session descriptor
			 */
			*ppIvBuffer = pSessionDesc->cipherPartialOpState;

			if (ICP_QAT_FW_LA_PARTIAL_START == qatPacketType) {
				/* if the previous partial state was full, then
				 * this is the first partial in the sequence so
				 * we need to (re-)initialise the contents of
				 * the state buffer using the initial state that
				 * is stored in the session descriptor. But, we
				 * have to be very careful here not to overwrite
				 * the cipherPartialOpState just yet in case
				 * there's a previous partial sequence in
				 * flight, so we defer the copy for now. This
				 * will be completed in the
				 * LacSymQueue_RequestSend() function when clear
				 * to send.
				 */
				pCbCookie->updateSessionIvOnSend = CPA_TRUE;
			}
		}
	} break;
	case CPA_CY_SYM_CIPHER_ZUC_EEA3: {
		LAC_CHECK_NULL_PARAM(pOpData->pIv);
		if (pOpData->ivLenInBytes != ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ) {
			LAC_INVALID_PARAM_LOG("invalid cipher IV size");
			return CPA_STATUS_INVALID_PARAM;
		}
		*ppIvBuffer = pOpData->pIv;
	} break;
	default:
		*ppIvBuffer = NULL;
	}

	return CPA_STATUS_SUCCESS;
}


CpaStatus
LacCipher_SessionSetupDataCheck(const CpaCySymCipherSetupData *pCipherSetupData,
				Cpa32U capabilitiesMask)
{
	/* No key required for NULL algorithm */
	if (!LAC_CIPHER_IS_NULL(pCipherSetupData->cipherAlgorithm)) {
		LAC_CHECK_NULL_PARAM(pCipherSetupData->pCipherKey);

		/* Check that algorithm and keys passed in are correct size */
		switch (pCipherSetupData->cipherAlgorithm) {
		case CPA_CY_SYM_CIPHER_ARC4:
			if (pCipherSetupData->cipherKeyLenInBytes >
			    ICP_QAT_HW_ARC4_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid ARC4 cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_AES_CCM:
			if (!LAC_CIPHER_AES_V2(capabilitiesMask) &&
			    pCipherSetupData->cipherKeyLenInBytes !=
				ICP_QAT_HW_AES_128_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid AES CCM cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_AES_XTS:
			if ((pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_128_XTS_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_AES_256_XTS_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_UCS_AES_128_XTS_KEY_SZ) &&
			    (pCipherSetupData->cipherKeyLenInBytes !=
			     ICP_QAT_HW_UCS_AES_256_XTS_KEY_SZ)) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid AES XTS cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_AES_ECB:
		case CPA_CY_SYM_CIPHER_AES_CBC:
		case CPA_CY_SYM_CIPHER_AES_CTR:
		case CPA_CY_SYM_CIPHER_AES_GCM:
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
			break;
		case CPA_CY_SYM_CIPHER_AES_F8:
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
			break;
		case CPA_CY_SYM_CIPHER_DES_ECB:
		case CPA_CY_SYM_CIPHER_DES_CBC:
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_DES_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid DES cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_3DES_ECB:
		case CPA_CY_SYM_CIPHER_3DES_CBC:
		case CPA_CY_SYM_CIPHER_3DES_CTR:
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_3DES_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid Triple-DES cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_KASUMI_F8:
			/* QAT-FW only supports 128 bits Cipher Key size for
			 * Kasumi F8 Ref: 3GPP TS 55.216 V6.2.0 */
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_KASUMI_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid Kasumi cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_SNOW3G_UEA2:
			/* QAT-FW only supports 256 bits Cipher Key size for
			 * Snow_3G */
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid Snow_3G cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_ZUC_EEA3:
			/* ZUC EEA3 */
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_ZUC_3G_EEA3_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid ZUC cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_CHACHA:
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_CHACHAPOLY_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid CHACHAPOLY cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		case CPA_CY_SYM_CIPHER_SM4_ECB:
		case CPA_CY_SYM_CIPHER_SM4_CBC:
		case CPA_CY_SYM_CIPHER_SM4_CTR:
			if (pCipherSetupData->cipherKeyLenInBytes !=
			    ICP_QAT_HW_SM4_KEY_SZ) {
				LAC_INVALID_PARAM_LOG(
				    "Invalid SM4 cipher key length");
				return CPA_STATUS_INVALID_PARAM;
			}
			break;
		default:
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
	} else {
		/* Perform algorithm-specific checks */
		switch (algorithm) {
		case CPA_CY_SYM_CIPHER_ARC4:
		case CPA_CY_SYM_CIPHER_AES_CTR:
		case CPA_CY_SYM_CIPHER_3DES_CTR:
		case CPA_CY_SYM_CIPHER_SM4_CTR:
		case CPA_CY_SYM_CIPHER_AES_CCM:
		case CPA_CY_SYM_CIPHER_AES_GCM:
		case CPA_CY_SYM_CIPHER_CHACHA:
		case CPA_CY_SYM_CIPHER_KASUMI_F8:
		case CPA_CY_SYM_CIPHER_AES_F8:
		case CPA_CY_SYM_CIPHER_SNOW3G_UEA2:
		case CPA_CY_SYM_CIPHER_ZUC_EEA3:
			/* No action needed */
			break;
		/*
		 * XTS Mode allow for ciphers which are not multiples of
		 * the block size.
		 */
		case CPA_CY_SYM_CIPHER_AES_XTS:
			if ((pOpData->packetType ==
			     CPA_CY_SYM_PACKET_TYPE_FULL) ||
			    (pOpData->packetType ==
			     CPA_CY_SYM_PACKET_TYPE_LAST_PARTIAL)) {
				/*
				 * If this is the last of a partial request
				 */
				if (pOpData->messageLenToCipherInBytes <
				    ICP_QAT_HW_AES_BLK_SZ) {
					LAC_INVALID_PARAM_LOG(
					    "data size must be greater than block"
					    " size for last XTS partial or XTS "
					    "full packet");
					status = CPA_STATUS_INVALID_PARAM;
				}
			}
			break;
		default:
			/* Mask & check below is based on assumption that block
			 * size is a power of 2. If data size is not a multiple
			 * of the block size, the "remainder" bits selected by
			 * the mask be non-zero
			 */
			if (pOpData->messageLenToCipherInBytes &
			    (LacSymQat_CipherBlockSizeBytesGet(algorithm) -
			     1)) {
				LAC_INVALID_PARAM_LOG(
				    "data size must be block size"
				    " multiple");
				status = CPA_STATUS_INVALID_PARAM;
			}
		}
	}
	return status;
}

Cpa32U
LacCipher_GetCipherSliceType(sal_crypto_service_t *pService,
			     CpaCySymCipherAlgorithm cipherAlgorithm,
			     CpaCySymHashAlgorithm hashAlgorithm)
{
	Cpa32U sliceType = ICP_QAT_FW_LA_USE_LEGACY_SLICE_TYPE;
	Cpa32U capabilitiesMask =
	    pService->generic_service_info.capabilitiesMask;

	/* UCS Slice is supproted only in Gen4 */
	if (isCyGen4x(pService)) {
		if (LAC_CIPHER_IS_XTS_MODE(cipherAlgorithm) ||
		    LAC_CIPHER_IS_CHACHA(cipherAlgorithm) ||
		    LAC_CIPHER_IS_GCM(cipherAlgorithm)) {
			sliceType = ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE;
		} else if (LAC_CIPHER_IS_CCM(cipherAlgorithm) &&
			   LAC_CIPHER_AES_V2(capabilitiesMask)) {
			sliceType = ICP_QAT_FW_LA_USE_LEGACY_SLICE_TYPE;
		} else if (LAC_CIPHER_IS_AES(cipherAlgorithm) &&
			   LAC_CIPHER_IS_CTR_MODE(cipherAlgorithm)) {
			sliceType = ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE;
		}
	}

	return sliceType;
}
