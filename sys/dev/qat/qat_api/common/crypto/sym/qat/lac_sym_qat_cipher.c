/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_qat_cipher.c      QAT-related support functions for Cipher
 *
 * @ingroup LacSymQat_Cipher
 *
 * @description Functions to support the QAT related operations for Cipher
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

#include "cpa.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "lac_sym_qat.h"
#include "lac_sym_qat_cipher.h"
#include "lac_mem.h"
#include "lac_common.h"
#include "cpa_cy_sym.h"
#include "lac_sym_qat.h"
#include "lac_sym_cipher_defs.h"
#include "icp_qat_hw.h"
#include "icp_qat_fw_la.h"
#include "sal_hw_gen.h"

#define LAC_UNUSED_POS_MASK 0x3

/*****************************************************************************
 *  Internal data
 *****************************************************************************/

typedef enum _icp_qat_hw_key_depend {
	IS_KEY_DEP_NO = 0,
	IS_KEY_DEP_YES,
} icp_qat_hw_key_depend;

/* LAC_CIPHER_IS_XTS_MODE */
static const uint8_t key_size_xts[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES128, /* ICP_QAT_HW_AES_128_XTS_KEY_SZ */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES256 /* ICP_QAT_HW_AES_256_XTS_KEY_SZ */
};
/* LAC_CIPHER_IS_AES */
static const uint8_t key_size_aes[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES128, /* ICP_QAT_HW_AES_128_KEY_SZ */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES192, /* ICP_QAT_HW_AES_192_KEY_SZ */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES256 /* ICP_QAT_HW_AES_256_KEY_SZ */
};
/* LAC_CIPHER_IS_AES_F8 */
static const uint8_t key_size_f8[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES128, /* ICP_QAT_HW_AES_128_F8_KEY_SZ */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES192, /* ICP_QAT_HW_AES_192_F8_KEY_SZ */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ICP_QAT_HW_CIPHER_ALGO_AES256 /* ICP_QAT_HW_AES_256_F8_KEY_SZ */
};

/* This array must be kept aligned with CpaCySymCipherAlgorithm enum but
 * offset by -1 as that enum starts at 1. LacSymQat_CipherGetCfgData()
 * below relies on that alignment and uses that enum -1 to index into this
 * array.
 */
typedef struct _icp_qat_hw_cipher_info {
	icp_qat_hw_cipher_algo_t algorithm;
	icp_qat_hw_cipher_mode_t mode;
	icp_qat_hw_cipher_convert_t key_convert[2];
	icp_qat_hw_cipher_dir_t dir[2];
	icp_qat_hw_key_depend isKeyLenDepend;
	const uint8_t *pAlgByKeySize;
} icp_qat_hw_cipher_info;

static const icp_qat_hw_cipher_info icp_qat_alg_info[] = {
	/* CPA_CY_SYM_CIPHER_NULL */
	{
	    ICP_QAT_HW_CIPHER_ALGO_NULL,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_ARC4 */
	{
	    ICP_QAT_HW_CIPHER_ALGO_ARC4,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    /* Streaming ciphers are a special case. Decrypt = encrypt */
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_AES_ECB */
	{
	    ICP_QAT_HW_CIPHER_ALGO_AES128,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    /* AES decrypt key needs to be reversed.  Instead of reversing the
	     * key at session registration, it is instead reversed on-the-fly by
	     * setting the KEY_CONVERT bit here
	     */
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_KEY_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_YES,
	    key_size_aes,
	},
	/* CPA_CY_SYM_CIPHER_AES_CBC */
	{
	    ICP_QAT_HW_CIPHER_ALGO_AES128,
	    ICP_QAT_HW_CIPHER_CBC_MODE,
	    /* AES decrypt key needs to be reversed.  Instead of reversing the
	     * key at session registration, it is instead reversed on-the-fly by
	     * setting the KEY_CONVERT bit here
	     */
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_KEY_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_YES,
	    key_size_aes,
	},
	/* CPA_CY_SYM_CIPHER_AES_CTR */
	{
	    ICP_QAT_HW_CIPHER_ALGO_AES128,
	    ICP_QAT_HW_CIPHER_CTR_MODE,
	    /* AES decrypt key needs to be reversed.  Instead of reversing the
	     * key at session registration, it is instead reversed on-the-fly by
	     * setting the KEY_CONVERT bit here
	     */
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    /* Streaming ciphers are a special case. Decrypt = encrypt
	     * Overriding default values previously set for AES
	     */
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_YES,
	    key_size_aes,
	},
	/* CPA_CY_SYM_CIPHER_AES_CCM */
	{
	    ICP_QAT_HW_CIPHER_ALGO_AES128,
	    ICP_QAT_HW_CIPHER_CTR_MODE,
	    /* AES decrypt key needs to be reversed.  Instead of reversing the
	     * key at session registration, it is instead reversed on-the-fly by
	     * setting the KEY_CONVERT bit here
	     */
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    /* Streaming ciphers are a special case. Decrypt = encrypt
	     * Overriding default values previously set for AES
	     */
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_YES,
	    key_size_aes,
	},
	/* CPA_CY_SYM_CIPHER_AES_GCM */
	{
	    ICP_QAT_HW_CIPHER_ALGO_AES128,
	    ICP_QAT_HW_CIPHER_CTR_MODE,
	    /* AES decrypt key needs to be reversed.  Instead of reversing the
	     * key at session registration, it is instead reversed on-the-fly by
	     * setting the KEY_CONVERT bit here
	     */
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    /* Streaming ciphers are a special case. Decrypt = encrypt
	     * Overriding default values previously set for AES
	     */
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_YES,
	    key_size_aes,
	},
	/* CPA_CY_SYM_CIPHER_DES_ECB */
	{
	    ICP_QAT_HW_CIPHER_ALGO_DES,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_DES_CBC */
	{
	    ICP_QAT_HW_CIPHER_ALGO_DES,
	    ICP_QAT_HW_CIPHER_CBC_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_3DES_ECB */
	{
	    ICP_QAT_HW_CIPHER_ALGO_3DES,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_3DES_CBC */
	{
	    ICP_QAT_HW_CIPHER_ALGO_3DES,
	    ICP_QAT_HW_CIPHER_CBC_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_3DES_CTR */
	{
	    ICP_QAT_HW_CIPHER_ALGO_3DES,
	    ICP_QAT_HW_CIPHER_CTR_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    /* Streaming ciphers are a special case. Decrypt = encrypt
	     * Overriding default values previously set for AES
	     */
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_KASUMI_F8 */
	{
	    ICP_QAT_HW_CIPHER_ALGO_KASUMI,
	    ICP_QAT_HW_CIPHER_F8_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    /* Streaming ciphers are a special case. Decrypt = encrypt */
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_SNOW3G_UEA2 */
	{
	    /* The KEY_CONVERT bit has to be set for Snow_3G operation */
	    ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    { ICP_QAT_HW_CIPHER_KEY_CONVERT, ICP_QAT_HW_CIPHER_KEY_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_AES_F8 */
	{
	    ICP_QAT_HW_CIPHER_ALGO_AES128,
	    ICP_QAT_HW_CIPHER_F8_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    /* Streaming ciphers are a special case. Decrypt = encrypt */
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_YES,
	    key_size_f8,
	},
	/* CPA_CY_SYM_CIPHER_AES_XTS */
	{
	    ICP_QAT_HW_CIPHER_ALGO_AES128,
	    ICP_QAT_HW_CIPHER_XTS_MODE,
	    /* AES decrypt key needs to be reversed.  Instead of reversing the
	     * key at session registration, it is instead reversed on-the-fly by
	     * setting the KEY_CONVERT bit here
	     */
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_KEY_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_YES,
	    key_size_xts,
	},
	/* CPA_CY_SYM_CIPHER_ZUC_EEA3 */
	{
	    ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    { ICP_QAT_HW_CIPHER_KEY_CONVERT, ICP_QAT_HW_CIPHER_KEY_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_CHACHA */
	{
	    ICP_QAT_HW_CIPHER_ALGO_CHACHA20_POLY1305,
	    ICP_QAT_HW_CIPHER_CTR_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_SM4_ECB */
	{
	    ICP_QAT_HW_CIPHER_ALGO_SM4,
	    ICP_QAT_HW_CIPHER_ECB_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_KEY_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_SM4_CBC */
	{
	    ICP_QAT_HW_CIPHER_ALGO_SM4,
	    ICP_QAT_HW_CIPHER_CBC_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_KEY_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_DECRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
	/* CPA_CY_SYM_CIPHER_SM4_CTR */
	{
	    ICP_QAT_HW_CIPHER_ALGO_SM4,
	    ICP_QAT_HW_CIPHER_CTR_MODE,
	    { ICP_QAT_HW_CIPHER_NO_CONVERT, ICP_QAT_HW_CIPHER_NO_CONVERT },
	    { ICP_QAT_HW_CIPHER_ENCRYPT, ICP_QAT_HW_CIPHER_ENCRYPT },
	    IS_KEY_DEP_NO,
	    NULL,
	},
};

/*****************************************************************************
 *  Internal functions
 *****************************************************************************/

void
LacSymQat_CipherCtrlBlockWrite(icp_qat_la_bulk_req_ftr_t *pMsg,
			       Cpa32U cipherAlgorithm,
			       Cpa32U targetKeyLenInBytes,
			       Cpa32U sliceType,
			       icp_qat_fw_slice_t nextSlice,
			       Cpa8U cipherCfgOffsetInQuadWord)
{
	icp_qat_fw_cipher_cd_ctrl_hdr_t *cd_ctrl =
	    (icp_qat_fw_cipher_cd_ctrl_hdr_t *)&(pMsg->cd_ctrl);

	/* state_padding_sz is nonzero for f8 mode only */
	cd_ctrl->cipher_padding_sz = 0;

	/* Special handling of AES 192 key for UCS slice.
	   UCS requires it to have 32 bytes - set is as targetKeyLen
	   in this case, and add padding. It makes no sense
	   to force applications to provide such key length for couple reasons:
	   1. It won't be possible to distinguish between AES 192 and 256 based
	      on key length only
	   2. Only some modes of AES will use UCS slice, then application will
	      have to know which ones */
	if (ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE == sliceType &&
	    ICP_QAT_HW_AES_192_KEY_SZ == targetKeyLenInBytes) {
		targetKeyLenInBytes = ICP_QAT_HW_UCS_AES_192_KEY_SZ;
	}

	switch (cipherAlgorithm) {
	/* Base Key is not passed down to QAT in the case of ARC4 or NULL */
	case CPA_CY_SYM_CIPHER_ARC4:
	case CPA_CY_SYM_CIPHER_NULL:
		cd_ctrl->cipher_key_sz = 0;
		break;
	case CPA_CY_SYM_CIPHER_KASUMI_F8:
		cd_ctrl->cipher_key_sz =
		    LAC_BYTES_TO_QUADWORDS(ICP_QAT_HW_KASUMI_F8_KEY_SZ);
		cd_ctrl->cipher_padding_sz =
		    ICP_QAT_HW_MODE_F8_NUM_REG_TO_CLEAR;
		break;
	/* For Snow3G UEA2 content descriptor key size is
	   key size plus iv size */
	case CPA_CY_SYM_CIPHER_SNOW3G_UEA2:
		cd_ctrl->cipher_key_sz =
		    LAC_BYTES_TO_QUADWORDS(ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ +
					   ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ);
		break;
	case CPA_CY_SYM_CIPHER_AES_F8:
		cd_ctrl->cipher_key_sz =
		    LAC_BYTES_TO_QUADWORDS(targetKeyLenInBytes);
		cd_ctrl->cipher_padding_sz =
		    (2 * ICP_QAT_HW_MODE_F8_NUM_REG_TO_CLEAR);
		break;
	/* For ZUC EEA3 content descriptor key size is
	   key size plus iv size */
	case CPA_CY_SYM_CIPHER_ZUC_EEA3:
		cd_ctrl->cipher_key_sz =
		    LAC_BYTES_TO_QUADWORDS(ICP_QAT_HW_ZUC_3G_EEA3_KEY_SZ +
					   ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ);
		break;
	default:
		cd_ctrl->cipher_key_sz =
		    LAC_BYTES_TO_QUADWORDS(targetKeyLenInBytes);
	}

	cd_ctrl->cipher_state_sz = LAC_BYTES_TO_QUADWORDS(
	    LacSymQat_CipherIvSizeBytesGet(cipherAlgorithm));

	cd_ctrl->cipher_cfg_offset = cipherCfgOffsetInQuadWord;

	ICP_QAT_FW_COMN_NEXT_ID_SET(cd_ctrl, nextSlice);
	ICP_QAT_FW_COMN_CURR_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
}

void
LacSymQat_CipherGetCfgData(lac_session_desc_t *pSession,
			   icp_qat_hw_cipher_algo_t *pAlgorithm,
			   icp_qat_hw_cipher_mode_t *pMode,
			   icp_qat_hw_cipher_dir_t *pDir,
			   icp_qat_hw_cipher_convert_t *pKey_convert)
{
	sal_crypto_service_t *pService =
	    (sal_crypto_service_t *)pSession->pInstance;

	int cipherIdx = 0;
	icp_qat_hw_cipher_dir_t cipherDirection = 0;

	/* Set defaults */
	*pKey_convert = ICP_QAT_HW_CIPHER_NO_CONVERT;
	*pAlgorithm = ICP_QAT_HW_CIPHER_ALGO_NULL;
	*pMode = ICP_QAT_HW_CIPHER_ECB_MODE;
	*pDir = ICP_QAT_HW_CIPHER_ENCRYPT;

	/* offset index as CpaCySymCipherAlgorithm enum starts from 1, not from
	 * 0 */
	cipherIdx = pSession->cipherAlgorithm - 1;
	cipherDirection =
	    pSession->cipherDirection == CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT ?
		  ICP_QAT_HW_CIPHER_ENCRYPT :
		  ICP_QAT_HW_CIPHER_DECRYPT;

	/* Boundary check against the last value in the algorithm enum */
	if (!(pSession->cipherAlgorithm <= CPA_CY_SYM_CIPHER_SM4_CTR)) {
		QAT_UTILS_LOG("Invalid cipherAlgorithm value\n");
		return;
	}

	if (!(cipherDirection <= ICP_QAT_HW_CIPHER_DECRYPT)) {
		QAT_UTILS_LOG("Invalid cipherDirection value\n");
		return;
	}

	*pAlgorithm = icp_qat_alg_info[cipherIdx].algorithm;
	*pMode = icp_qat_alg_info[cipherIdx].mode;
	*pDir = icp_qat_alg_info[cipherIdx].dir[cipherDirection];
	*pKey_convert =
	    icp_qat_alg_info[cipherIdx].key_convert[cipherDirection];

	if (IS_KEY_DEP_NO != icp_qat_alg_info[cipherIdx].isKeyLenDepend) {
		*pAlgorithm = icp_qat_alg_info[cipherIdx]
				  .pAlgByKeySize[pSession->cipherKeyLenInBytes];
	}

	/* CCP and AES_GCM single pass, despite being limited to CTR/AEAD mode,
	 * support both Encrypt/Decrypt modes - this is because of the
	 * differences in the hash computation/verification paths in
	 * encrypt/decrypt modes respectively.
	 * By default CCP is set as CTR Mode.Set AEAD Mode for AES_GCM.
	 */
	if (SPC == pSession->singlePassState) {
		if (LAC_CIPHER_IS_GCM(pSession->cipherAlgorithm))
			*pMode = ICP_QAT_HW_CIPHER_AEAD_MODE;
		else if (isCyGen4x(pService) &&
			 LAC_CIPHER_IS_CCM(pSession->cipherAlgorithm))
			*pMode = ICP_QAT_HW_CIPHER_CCM_MODE;

		if (cipherDirection == ICP_QAT_HW_CIPHER_DECRYPT)
			*pDir = ICP_QAT_HW_CIPHER_DECRYPT;
	}
}

void
LacSymQat_CipherHwBlockPopulateCfgData(lac_session_desc_t *pSession,
				       const void *pCipherHwBlock,
				       Cpa32U *pSizeInBytes)
{
	icp_qat_hw_cipher_algo_t algorithm = ICP_QAT_HW_CIPHER_ALGO_NULL;
	icp_qat_hw_cipher_mode_t mode = ICP_QAT_HW_CIPHER_ECB_MODE;
	icp_qat_hw_cipher_dir_t dir = ICP_QAT_HW_CIPHER_ENCRYPT;
	icp_qat_hw_cipher_convert_t key_convert;
	icp_qat_hw_cipher_config_t *pCipherConfig =
	    (icp_qat_hw_cipher_config_t *)pCipherHwBlock;
	icp_qat_hw_ucs_cipher_config_t *pUCSCipherConfig =
	    (icp_qat_hw_ucs_cipher_config_t *)pCipherHwBlock;

	Cpa32U val, reserved;
	Cpa32U aed_hash_cmp_length = 0;

	*pSizeInBytes = 0;

	LacSymQat_CipherGetCfgData(
	    pSession, &algorithm, &mode, &dir, &key_convert);

	/* Build the cipher config into the hardware setup block */
	if (SPC == pSession->singlePassState) {
		aed_hash_cmp_length = pSession->hashResultSize;
		reserved = ICP_QAT_HW_CIPHER_CONFIG_BUILD_UPPER(
		    pSession->aadLenInBytes);
	} else {
		reserved = 0;
	}

	val = ICP_QAT_HW_CIPHER_CONFIG_BUILD(
	    mode, algorithm, key_convert, dir, aed_hash_cmp_length);

	/* UCS slice has 128-bit configuration register.
	   Leacy cipher slice has 64-bit config register */
	if (ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE == pSession->cipherSliceType) {
		pUCSCipherConfig->val = val;
		pUCSCipherConfig->reserved[0] = reserved;
		pUCSCipherConfig->reserved[1] = 0;
		pUCSCipherConfig->reserved[2] = 0;
		*pSizeInBytes = sizeof(icp_qat_hw_ucs_cipher_config_t);
	} else {
		pCipherConfig->val = val;
		pCipherConfig->reserved = reserved;
		*pSizeInBytes = sizeof(icp_qat_hw_cipher_config_t);
	}
}

void
LacSymQat_CipherHwBlockPopulateKeySetup(
    lac_session_desc_t *pSessionDesc,
    const CpaCySymCipherSetupData *pCipherSetupData,
    Cpa32U targetKeyLenInBytes,
    Cpa32U sliceType,
    const void *pCipherHwBlock,
    Cpa32U *pSizeInBytes)
{
	Cpa8U *pCipherKey = (Cpa8U *)pCipherHwBlock;
	Cpa32U actualKeyLenInBytes = pCipherSetupData->cipherKeyLenInBytes;

	*pSizeInBytes = 0;

	/* Key is copied into content descriptor for all cases except for
	 * Arc4 and Null cipher */
	if (!(LAC_CIPHER_IS_ARC4(pCipherSetupData->cipherAlgorithm) ||
	      LAC_CIPHER_IS_NULL(pCipherSetupData->cipherAlgorithm))) {
		/* Special handling of AES 192 key for UCS slice.
		   UCS requires it to have 32 bytes - set is as targetKeyLen
		   in this case, and add padding. It makes no sense
		   to force applications to provide such key length for couple reasons:
		   1. It won't be possible to distinguish between AES 192 and 256 based
		      on key length only
		   2. Only some modes of AES will use UCS slice, then application will
		      have to know which ones */
		if (ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE == sliceType &&
		    ICP_QAT_HW_AES_192_KEY_SZ == targetKeyLenInBytes) {
			targetKeyLenInBytes = ICP_QAT_HW_UCS_AES_192_KEY_SZ;
		}

		/* Set the Cipher key field in the cipher block */
		memcpy(pCipherKey,
		       pCipherSetupData->pCipherKey,
		       actualKeyLenInBytes);
		/* Pad the key with 0's if required */
		if (0 < (targetKeyLenInBytes - actualKeyLenInBytes)) {
			LAC_OS_BZERO(pCipherKey + actualKeyLenInBytes,
				     targetKeyLenInBytes - actualKeyLenInBytes);
		}
		*pSizeInBytes += targetKeyLenInBytes;

		switch (pCipherSetupData->cipherAlgorithm) {
			/* For Kasumi in F8 mode Cipher Key is concatenated with
			 * Cipher Key XOR-ed with Key Modifier (CK||CK^KM) */
		case CPA_CY_SYM_CIPHER_KASUMI_F8: {
			Cpa32U wordIndex = 0;
			Cpa32U *pu32CipherKey =
			    (Cpa32U *)pCipherSetupData->pCipherKey;
			Cpa32U *pTempKey =
			    (Cpa32U *)(pCipherKey + targetKeyLenInBytes);

			/* XOR Key with KASUMI F8 key modifier at 4 bytes level
			 */
			for (wordIndex = 0; wordIndex <
			     LAC_BYTES_TO_LONGWORDS(targetKeyLenInBytes);
			     wordIndex++) {
				pTempKey[wordIndex] = pu32CipherKey[wordIndex] ^
				    LAC_CIPHER_KASUMI_F8_KEY_MODIFIER_4_BYTES;
			}

			*pSizeInBytes += targetKeyLenInBytes;

			/* also add padding for F8 */
			*pSizeInBytes += LAC_QUADWORDS_TO_BYTES(
			    ICP_QAT_HW_MODE_F8_NUM_REG_TO_CLEAR);
			LAC_OS_BZERO((Cpa8U *)pTempKey + targetKeyLenInBytes,
				     LAC_QUADWORDS_TO_BYTES(
					 ICP_QAT_HW_MODE_F8_NUM_REG_TO_CLEAR));
		} break;
			/* For AES in F8 mode Cipher Key is concatenated with
			 * Cipher Key XOR-ed with Key Mask (CK||CK^KM) */
		case CPA_CY_SYM_CIPHER_AES_F8: {
			Cpa32U index = 0;
			Cpa8U *pTempKey =
			    pCipherKey + (targetKeyLenInBytes / 2);
			*pSizeInBytes += targetKeyLenInBytes;
			/* XOR Key with key Mask */
			for (index = 0; index < targetKeyLenInBytes; index++) {
				pTempKey[index] =
				    pCipherKey[index] ^ pTempKey[index];
			}
			pTempKey = (pCipherKey + targetKeyLenInBytes);
			/* also add padding for AES F8 */
			*pSizeInBytes += 2 * targetKeyLenInBytes;
			LAC_OS_BZERO(pTempKey, 2 * targetKeyLenInBytes);
		} break;
		case CPA_CY_SYM_CIPHER_SNOW3G_UEA2: {
			/* For Snow3G zero area after the key for FW */
			LAC_OS_BZERO(pCipherKey + targetKeyLenInBytes,
				     ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ);

			*pSizeInBytes += ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ;
		} break;
		case CPA_CY_SYM_CIPHER_ZUC_EEA3: {
			/* For ZUC zero area after the key for FW */
			LAC_OS_BZERO(pCipherKey + targetKeyLenInBytes,
				     ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ);

			*pSizeInBytes += ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ;
		} break;
		case CPA_CY_SYM_CIPHER_AES_XTS: {
			/* For AES in XTS mode Cipher Key is concatenated with
			 * second Cipher Key which is used for tweak calculation
			 * (CK1||CK2). For decryption Cipher Key needs to be
			 * converted to reverse key.*/
			if (ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE == sliceType) {
				Cpa32U key_len =
				    pCipherSetupData->cipherKeyLenInBytes / 2;
				memcpy(pSessionDesc->cipherAesXtsKey1Forward,
				       pCipherSetupData->pCipherKey,
				       key_len);

				qatUtilsAESKeyExpansionForward(
				    pSessionDesc->cipherAesXtsKey1Forward,
				    key_len,
				    (uint32_t *)
					pSessionDesc->cipherAesXtsKey1Reverse);

				memcpy(pSessionDesc->cipherAesXtsKey2,
				       pCipherSetupData->pCipherKey + key_len,
				       key_len);

				if (CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT ==
				    pCipherSetupData->cipherDirection) {
					memcpy(pCipherKey,
					       pSessionDesc
						   ->cipherAesXtsKey1Reverse,
					       key_len);
				} else {
					memcpy(pCipherKey,
					       pSessionDesc
						   ->cipherAesXtsKey1Forward,
					       key_len);
				}
			}
		} break;
		default:
			break;
		}
	}
}

/*****************************************************************************
 *  External functions
 *****************************************************************************/

Cpa8U
LacSymQat_CipherBlockSizeBytesGet(CpaCySymCipherAlgorithm cipherAlgorithm)
{
	Cpa8U blockSize = 0;
	switch (cipherAlgorithm) {
	case CPA_CY_SYM_CIPHER_ARC4:
		blockSize = LAC_CIPHER_ARC4_BLOCK_LEN_BYTES;
		break;
	/* Handle AES or AES_F8 */
	case CPA_CY_SYM_CIPHER_AES_ECB:
	case CPA_CY_SYM_CIPHER_AES_CBC:
	case CPA_CY_SYM_CIPHER_AES_CTR:
	case CPA_CY_SYM_CIPHER_AES_CCM:
	case CPA_CY_SYM_CIPHER_AES_GCM:
	case CPA_CY_SYM_CIPHER_AES_XTS:
	case CPA_CY_SYM_CIPHER_AES_F8:
		blockSize = ICP_QAT_HW_AES_BLK_SZ;
		break;
	/* Handle DES */
	case CPA_CY_SYM_CIPHER_DES_ECB:
	case CPA_CY_SYM_CIPHER_DES_CBC:
		blockSize = ICP_QAT_HW_DES_BLK_SZ;
		break;
	/* Handle TRIPLE DES */
	case CPA_CY_SYM_CIPHER_3DES_ECB:
	case CPA_CY_SYM_CIPHER_3DES_CBC:
	case CPA_CY_SYM_CIPHER_3DES_CTR:
		blockSize = ICP_QAT_HW_3DES_BLK_SZ;
		break;
	case CPA_CY_SYM_CIPHER_KASUMI_F8:
		blockSize = ICP_QAT_HW_KASUMI_BLK_SZ;
		break;
	case CPA_CY_SYM_CIPHER_SNOW3G_UEA2:
		blockSize = ICP_QAT_HW_SNOW_3G_BLK_SZ;
		break;
	case CPA_CY_SYM_CIPHER_ZUC_EEA3:
		blockSize = ICP_QAT_HW_ZUC_3G_BLK_SZ;
		break;
	case CPA_CY_SYM_CIPHER_NULL:
		blockSize = LAC_CIPHER_NULL_BLOCK_LEN_BYTES;
		break;
	case CPA_CY_SYM_CIPHER_CHACHA:
		blockSize = ICP_QAT_HW_CHACHAPOLY_BLK_SZ;
		break;
	case CPA_CY_SYM_CIPHER_SM4_ECB:
	case CPA_CY_SYM_CIPHER_SM4_CBC:
	case CPA_CY_SYM_CIPHER_SM4_CTR:
		blockSize = ICP_QAT_HW_SM4_BLK_SZ;
		break;
	default:
		QAT_UTILS_LOG("Algorithm not supported in Cipher");
	}
	return blockSize;
}

Cpa32U
LacSymQat_CipherIvSizeBytesGet(CpaCySymCipherAlgorithm cipherAlgorithm)
{
	Cpa32U ivSize = 0;
	switch (cipherAlgorithm) {
	case CPA_CY_SYM_CIPHER_ARC4:
		ivSize = LAC_CIPHER_ARC4_STATE_LEN_BYTES;
		break;
	case CPA_CY_SYM_CIPHER_KASUMI_F8:
		ivSize = ICP_QAT_HW_KASUMI_BLK_SZ;
		break;
	case CPA_CY_SYM_CIPHER_SNOW3G_UEA2:
		ivSize = ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ;
		break;
	case CPA_CY_SYM_CIPHER_ZUC_EEA3:
		ivSize = ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ;
		break;
	case CPA_CY_SYM_CIPHER_CHACHA:
		ivSize = ICP_QAT_HW_CHACHAPOLY_IV_SZ;
		break;
	case CPA_CY_SYM_CIPHER_AES_ECB:
	case CPA_CY_SYM_CIPHER_DES_ECB:
	case CPA_CY_SYM_CIPHER_3DES_ECB:
	case CPA_CY_SYM_CIPHER_SM4_ECB:
	case CPA_CY_SYM_CIPHER_NULL:
		/* for all ECB Mode IV size is 0 */
		break;
	default:
		ivSize = LacSymQat_CipherBlockSizeBytesGet(cipherAlgorithm);
	}
	return ivSize;
}

inline CpaStatus
LacSymQat_CipherRequestParamsPopulate(lac_session_desc_t *pSessionDesc,
				      icp_qat_fw_la_bulk_req_t *pReq,
				      Cpa32U cipherOffsetInBytes,
				      Cpa32U cipherLenInBytes,
				      Cpa64U ivBufferPhysAddr,
				      Cpa8U *pIvBufferVirt)
{
	icp_qat_fw_la_cipher_req_params_t *pCipherReqParams;
	icp_qat_fw_cipher_cd_ctrl_hdr_t *pCipherCdCtrlHdr;
	icp_qat_fw_serv_specif_flags *pCipherSpecificFlags;
	Cpa32U usedBufSize = 0;
	Cpa32U totalBufSize = 0;

	pCipherReqParams = (icp_qat_fw_la_cipher_req_params_t
				*)((Cpa8U *)&(pReq->serv_specif_rqpars) +
				   ICP_QAT_FW_CIPHER_REQUEST_PARAMETERS_OFFSET);
	pCipherCdCtrlHdr = (icp_qat_fw_cipher_cd_ctrl_hdr_t *)&(pReq->cd_ctrl);
	pCipherSpecificFlags = &(pReq->comn_hdr.serv_specif_flags);

	pCipherReqParams->cipher_offset = cipherOffsetInBytes;
	pCipherReqParams->cipher_length = cipherLenInBytes;

	/* Don't copy the buffer into the Msg if
	 * it's too big for the cipher_IV_array
	 * OR if the FW needs to update it
	 * OR if there's no buffer supplied
	 * OR if last partial
	 */
	if ((pCipherCdCtrlHdr->cipher_state_sz >
	     LAC_SYM_QAT_HASH_IV_REQ_MAX_SIZE_QW) ||
	    (ICP_QAT_FW_LA_UPDATE_STATE_GET(*pCipherSpecificFlags) ==
	     ICP_QAT_FW_LA_UPDATE_STATE) ||
	    (pIvBufferVirt == NULL) ||
	    (ICP_QAT_FW_LA_PARTIAL_GET(*pCipherSpecificFlags) ==
	     ICP_QAT_FW_LA_PARTIAL_END)) {
		/* Populate the field with a ptr to the flat buffer */
		pCipherReqParams->u.s.cipher_IV_ptr = ivBufferPhysAddr;
		pCipherReqParams->u.s.resrvd1 = 0;
		/* Set the flag indicating the field format */
		ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(
		    *pCipherSpecificFlags, ICP_QAT_FW_CIPH_IV_64BIT_PTR);
	} else {
		/* Populate the field with the contents of the buffer,
		 * zero field first as data may be smaller than the field */

		/* In case of XTS mode using UCS slice always encrypt the embedded IV.
		 * IV provided by user needs to be encrypted to calculate initial tweak,
		 * use pCipherReqParams->u.cipher_IV_array as destination buffer for
		 * tweak value */
		if (ICP_QAT_FW_LA_USE_UCS_SLICE_TYPE ==
			pSessionDesc->cipherSliceType &&
		    LAC_CIPHER_IS_XTS_MODE(pSessionDesc->cipherAlgorithm)) {
			memset(pCipherReqParams->u.cipher_IV_array,
			       0,
			       LAC_LONGWORDS_TO_BYTES(
				   ICP_QAT_FW_NUM_LONGWORDS_4));
			qatUtilsAESEncrypt(
			    pSessionDesc->cipherAesXtsKey2,
			    pSessionDesc->cipherKeyLenInBytes / 2,
			    pIvBufferVirt,
			    (Cpa8U *)pCipherReqParams->u.cipher_IV_array);
		} else {
			totalBufSize =
			    LAC_LONGWORDS_TO_BYTES(ICP_QAT_FW_NUM_LONGWORDS_4);
			usedBufSize = LAC_QUADWORDS_TO_BYTES(
			    pCipherCdCtrlHdr->cipher_state_sz);
			/* Only initialise unused buffer if applicable*/
			if (usedBufSize < totalBufSize) {
				memset(
				    (&pCipherReqParams->u.cipher_IV_array
					  [usedBufSize & LAC_UNUSED_POS_MASK]),
				    0,
				    totalBufSize - usedBufSize);
			}
			memcpy(pCipherReqParams->u.cipher_IV_array,
			       pIvBufferVirt,
			       usedBufSize);
		}
		/* Set the flag indicating the field format */
		ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(
		    *pCipherSpecificFlags, ICP_QAT_FW_CIPH_IV_16BYTE_DATA);
	}

	return CPA_STATUS_SUCCESS;
}

void
LacSymQat_CipherArc4StateInit(const Cpa8U *pKey,
			      Cpa32U keyLenInBytes,
			      Cpa8U *pArc4CipherState)
{
	Cpa32U i = 0;
	Cpa32U j = 0;
	Cpa32U k = 0;

	for (i = 0; i < LAC_CIPHER_ARC4_KEY_MATRIX_LEN_BYTES; ++i) {
		pArc4CipherState[i] = (Cpa8U)i;
	}

	for (i = 0, k = 0; i < LAC_CIPHER_ARC4_KEY_MATRIX_LEN_BYTES; ++i, ++k) {
		Cpa8U swap = 0;

		if (k >= keyLenInBytes)
			k -= keyLenInBytes;

		j = (j + pArc4CipherState[i] + pKey[k]);
		if (j >= LAC_CIPHER_ARC4_KEY_MATRIX_LEN_BYTES)
			j %= LAC_CIPHER_ARC4_KEY_MATRIX_LEN_BYTES;

		/* Swap state[i] & state[j] */
		swap = pArc4CipherState[i];
		pArc4CipherState[i] = pArc4CipherState[j];
		pArc4CipherState[j] = swap;
	}

	/* Initialise i & j values for QAT */
	pArc4CipherState[LAC_CIPHER_ARC4_KEY_MATRIX_LEN_BYTES] = 0;
	pArc4CipherState[LAC_CIPHER_ARC4_KEY_MATRIX_LEN_BYTES + 1] = 0;
}

/* Update the cipher_key_sz in the Request cache prepared and stored
 * in the session */
void
LacSymQat_CipherXTSModeUpdateKeyLen(lac_session_desc_t *pSessionDesc,
				    Cpa32U newKeySizeInBytes)
{
	icp_qat_fw_cipher_cd_ctrl_hdr_t *pCipherControlBlock = NULL;

	pCipherControlBlock = (icp_qat_fw_cipher_cd_ctrl_hdr_t *)&(
	    pSessionDesc->reqCacheFtr.cd_ctrl);

	pCipherControlBlock->cipher_key_sz =
	    LAC_BYTES_TO_QUADWORDS(newKeySizeInBytes);
}
