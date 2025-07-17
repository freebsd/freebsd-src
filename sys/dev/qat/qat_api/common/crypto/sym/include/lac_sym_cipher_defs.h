/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_cipher_defs.h
 *
 * @ingroup LacCipher
 *
 * @description
 *      This file defines constants for the cipher operations.
 *
 *****************************************************************************/

/***************************************************************************/

#ifndef LAC_SYM_CIPHER_DEFS_H
#define LAC_SYM_CIPHER_DEFS_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

/***************************************************************************/

/*
 * Constants value for ARC4 algorithm
 */
/* ARC4 algorithm block size */
#define LAC_CIPHER_ARC4_BLOCK_LEN_BYTES 8
/* ARC4 key matrix size (bytes) */
#define LAC_CIPHER_ARC4_KEY_MATRIX_LEN_BYTES 256
/* ARC4 256 bytes for key matrix, 2 for i and j and 6 bytes for padding */
#define LAC_CIPHER_ARC4_STATE_LEN_BYTES 264

/*
 * Constant values for CCM AAD buffer
 */
#define LAC_CIPHER_CCM_B0_SIZE 16
#define LAC_CIPHER_CCM_ENCODED_AAD_LEN_SIZE 2
#define LAC_CIPHER_CCM_AAD_OFFSET                                              \
	(LAC_CIPHER_CCM_B0_SIZE + LAC_CIPHER_CCM_ENCODED_AAD_LEN_SIZE)

#define LAC_SYM_SNOW3G_CIPHER_CONFIG_FOR_HASH_SZ 40
/* Snow3g cipher config required for performing a Snow3g hash operation.
 * It contains 8 Bytes of config for hardware, 16 Bytes of Key and requires
 * 16 Bytes for the IV.
 */

/* Key Modifier (KM) 4 bytes used in Kasumi algorithm in F8 mode to XOR
 * Cipher Key (CK) */
#define LAC_CIPHER_KASUMI_F8_KEY_MODIFIER_4_BYTES 0x55555555

/* The IV length for Kasumi Kgcore is 8 bytes */
#define LAC_CIPHER_KASUMI_F8_IV_LENGTH 8

/* The Counter length for Kasumi Kgcore is 8 bytes */
#define LAC_CIPHER_KASUMI_F8_COUNTER_LENGTH 8

/* The IV length for AES F8 is 16 bytes */
#define LAC_CIPHER_AES_F8_IV_LENGTH 16

/* The max key length for AES XTS 32 is bytes*/
#define LAC_CIPHER_AES_XTS_KEY_MAX_LENGTH 32

/* For Snow3G UEA2, need to make sure last 8 Bytes of IV buffer are
 * zero. */
#define LAC_CIPHER_SNOW3G_UEA2_IV_BUFFER_ZERO_LENGTH 8

/* Reserve enough space for max length cipher state
 * (can be IV , counter or ARC4 state)  */
#define LAC_CIPHER_STATE_SIZE_MAX LAC_CIPHER_ARC4_STATE_LEN_BYTES

/* Reserve enough space for max length cipher IV
 * (can be A value for Kasumi(passed in as IV), IV or counter,
 * but not ARC4 state)  */
#define LAC_CIPHER_IV_SIZE_MAX ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ

/* 96-bit case of IV for GCM algorithm */
#define LAC_CIPHER_IV_SIZE_GCM_12 12

/* 96-bit case of IV for CCP/GCM single pass algorithm */
#define LAC_CIPHER_SPC_IV_SIZE 12
/*
 * Constants value for NULL algorithm
 */
/* NULL algorithm block size */
#define LAC_CIPHER_NULL_BLOCK_LEN_BYTES 8

/* Macro to check if the Algorithm is SM4 */
#define LAC_CIPHER_IS_SM4(algo)                                                \
	((algo == CPA_CY_SYM_CIPHER_SM4_ECB) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_SM4_CBC) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_SM4_CTR))

/* Macro to check if the Algorithm is CHACHA */
#define LAC_CIPHER_IS_CHACHA(algo) (algo == CPA_CY_SYM_CIPHER_CHACHA)
/* Macro to check if the Algorithm is AES */
#define LAC_CIPHER_IS_AES(algo)                                                \
	((algo == CPA_CY_SYM_CIPHER_AES_ECB) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_AES_CBC) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_AES_CTR) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_AES_CCM) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_AES_GCM) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_AES_XTS))

/* Macro to check if the Algorithm is DES */
#define LAC_CIPHER_IS_DES(algo)                                                \
	((algo == CPA_CY_SYM_CIPHER_DES_ECB) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_DES_CBC))

/* Macro to check if the Algorithm is Triple DES */
#define LAC_CIPHER_IS_TRIPLE_DES(algo)                                         \
	((algo == CPA_CY_SYM_CIPHER_3DES_ECB) ||                               \
	 (algo == CPA_CY_SYM_CIPHER_3DES_CBC) ||                               \
	 (algo == CPA_CY_SYM_CIPHER_3DES_CTR))

/* Macro to check if the Algorithm is Kasumi */
#define LAC_CIPHER_IS_KASUMI(algo) (algo == CPA_CY_SYM_CIPHER_KASUMI_F8)

/* Macro to check if the Algorithm is Snow3G UEA2 */
#define LAC_CIPHER_IS_SNOW3G_UEA2(algo) (algo == CPA_CY_SYM_CIPHER_SNOW3G_UEA2)

/* Macro to check if the Algorithm is ARC4 */
#define LAC_CIPHER_IS_ARC4(algo) (algo == CPA_CY_SYM_CIPHER_ARC4)

/* Macro to check if the Algorithm is ZUC EEA3 */
#define LAC_CIPHER_IS_ZUC_EEA3(algo) (algo == CPA_CY_SYM_CIPHER_ZUC_EEA3)

/* Macro to check if the Algorithm is NULL */
#define LAC_CIPHER_IS_NULL(algo) (algo == CPA_CY_SYM_CIPHER_NULL)

/* Macro to check if the Mode is CTR */
#define LAC_CIPHER_IS_CTR_MODE(algo)                                           \
	((algo == CPA_CY_SYM_CIPHER_AES_CTR) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_3DES_CTR) || (LAC_CIPHER_IS_CCM(algo)) ||  \
	 (LAC_CIPHER_IS_GCM(algo)) || (LAC_CIPHER_IS_CHACHA(algo)) ||          \
	 (algo == CPA_CY_SYM_CIPHER_SM4_CTR))

/* Macro to check if the Algorithm is ECB */
#define LAC_CIPHER_IS_ECB_MODE(algo)                                           \
	((algo == CPA_CY_SYM_CIPHER_AES_ECB) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_DES_ECB) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_3DES_ECB) ||                               \
	 (algo == CPA_CY_SYM_CIPHER_NULL) ||                                   \
	 (algo == CPA_CY_SYM_CIPHER_SNOW3G_UEA2) ||                            \
	 (algo == CPA_CY_SYM_CIPHER_SM4_ECB))

/* Macro to check if the Algorithm Mode is F8 */
#define LAC_CIPHER_IS_F8_MODE(algo)                                            \
	((algo == CPA_CY_SYM_CIPHER_KASUMI_F8) ||                              \
	 (algo == CPA_CY_SYM_CIPHER_AES_F8))

/* Macro to check if the Algorithm is CBC */
#define LAC_CIPHER_IS_CBC_MODE(algo)                                           \
	((algo == CPA_CY_SYM_CIPHER_AES_CBC) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_DES_CBC) ||                                \
	 (algo == CPA_CY_SYM_CIPHER_3DES_CBC) ||                               \
	 (algo == CPA_CY_SYM_CIPHER_SM4_CBC))

/* Macro to check if the Algorithm is CCM */
#define LAC_CIPHER_IS_CCM(algo) (algo == CPA_CY_SYM_CIPHER_AES_CCM)

/* Macro to check if the Algorithm is GCM */
#define LAC_CIPHER_IS_GCM(algo) (algo == CPA_CY_SYM_CIPHER_AES_GCM)

/* Macro to check if the Algorithm is AES-F8 */
#define LAC_CIPHER_IS_AES_F8(algo) (algo == CPA_CY_SYM_CIPHER_AES_F8)

/* Macro to check if the Algorithm Mode is XTS */
#define LAC_CIPHER_IS_XTS_MODE(algo) (algo == CPA_CY_SYM_CIPHER_AES_XTS)

/* Macro to check if the accelerator has AES V2 capability */
#define LAC_CIPHER_AES_V2(mask) ((mask)&ICP_ACCEL_CAPABILITIES_AES_V2)

/* Macro to check if the Algorithm is single pass */
#define LAC_CIPHER_IS_SPC(cipher, hash, mask)                                  \
	(((mask)&ICP_ACCEL_CAPABILITIES_CHACHA_POLY &&                         \
	  LAC_CIPHER_IS_CHACHA(cipher) && (CPA_CY_SYM_HASH_POLY == hash)) ||   \
	 (((mask)&ICP_ACCEL_CAPABILITIES_AESGCM_SPC) &&                        \
	  LAC_CIPHER_IS_GCM(cipher) &&                                         \
	  ((CPA_CY_SYM_HASH_AES_GCM == hash) ||                                \
	   (CPA_CY_SYM_HASH_AES_GMAC == hash))) ||                             \
	 (LAC_CIPHER_IS_CCM(cipher) && LAC_CIPHER_AES_V2(mask)))

#endif /* LAC_CIPHER_DEFS_H */
