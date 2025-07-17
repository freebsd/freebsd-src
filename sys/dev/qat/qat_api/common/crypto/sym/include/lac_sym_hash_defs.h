/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_hash_defs.h
 *
 * @defgroup LacHashDefs Hash Definitions
 *
 * @ingroup  LacHash
 *
 * Constants for hash algorithms
 *
 ***************************************************************************/

#ifndef LAC_SYM_HASH_DEFS_H
#define LAC_SYM_HASH_DEFS_H

/* Constant for MD5 algorithm  */
#define LAC_HASH_MD5_BLOCK_SIZE 64
/**< @ingroup LacHashDefs
 * MD5 block size in bytes */
#define LAC_HASH_MD5_DIGEST_SIZE 16
/**< @ingroup LacHashDefs
 * MD5 digest length in bytes */
#define LAC_HASH_MD5_STATE_SIZE 16
/**< @ingroup LacHashDefs
 * MD5 state size */

/* Constants for SHA1 algorithm  */
#define LAC_HASH_SHA1_BLOCK_SIZE 64
/**< @ingroup LacHashDefs
 * SHA1 Block size in bytes */
#define LAC_HASH_SHA1_DIGEST_SIZE 20
/**< @ingroup LacHashDefs
 *  SHA1 digest length in bytes */
#define LAC_HASH_SHA1_STATE_SIZE 20
/**< @ingroup LacHashDefs
 *  SHA1 state size */

/* Constants for SHA224 algorithm  */
#define LAC_HASH_SHA224_BLOCK_SIZE 64
/**< @ingroup LacHashDefs
 *  SHA224 block size in bytes */
#define LAC_HASH_SHA224_DIGEST_SIZE 28
/**< @ingroup LacHashDefs
 *  SHA224 digest length in bytes */
#define LAC_HASH_SHA224_STATE_SIZE 32
/**< @ingroup LacHashDefs
 * SHA224 state size */

/* Constants for SHA256 algorithm  */
#define LAC_HASH_SHA256_BLOCK_SIZE 64
/**< @ingroup LacHashDefs
 *  SHA256 block size in bytes */
#define LAC_HASH_SHA256_DIGEST_SIZE 32
/**< @ingroup LacHashDefs
 *  SHA256 digest length */
#define LAC_HASH_SHA256_STATE_SIZE 32
/**< @ingroup LacHashDefs
 *  SHA256 state size */

/* Constants for SHA384 algorithm  */
#define LAC_HASH_SHA384_BLOCK_SIZE 128
/**< @ingroup LacHashDefs
 *  SHA384 block size in bytes */
#define LAC_HASH_SHA384_DIGEST_SIZE 48
/**< @ingroup LacHashDefs
 *  SHA384 digest length in bytes */
#define LAC_HASH_SHA384_STATE_SIZE 64
/**< @ingroup LacHashDefs
 *  SHA384 state size */

/* Constants for SHA512 algorithm  */
#define LAC_HASH_SHA512_BLOCK_SIZE 128
/**< @ingroup LacHashDefs
 *  SHA512 block size in bytes */
#define LAC_HASH_SHA512_DIGEST_SIZE 64
/**< @ingroup LacHashDefs
 *  SHA512 digest length in bytes */
#define LAC_HASH_SHA512_STATE_SIZE 64
/**< @ingroup LacHashDefs
 *  SHA512 state size */

/* Constants for SHA3_224 algorithm  */
#define LAC_HASH_SHA3_224_BLOCK_SIZE 144
/**< @ingroup LacHashDefs
 *  SHA3_224 block size in bytes */
#define LAC_HASH_SHA3_224_DIGEST_SIZE 28
/**< @ingroup LacHashDefs
 *  SHA3_224 digest length in bytes */
#define LAC_HASH_SHA3_224_STATE_SIZE 28
/**< @ingroup LacHashDefs
 *  SHA3_224 state size */

/* Constants for SHA3_256 algorithm  */
#define LAC_HASH_SHA3_256_BLOCK_SIZE 136
/**< @ingroup LacHashDefs
 *  SHA3_256 block size in bytes */
#define LAC_HASH_SHA3_256_DIGEST_SIZE 32
/**< @ingroup LacHashDefs
 *  SHA3_256 digest length in bytes */
#define LAC_HASH_SHA3_256_STATE_SIZE 32
/**< @ingroup LacHashDefs
 *  SHA3_256 state size */

/* Constants for SHA3_384 algorithm  */
#define LAC_HASH_SHA3_384_BLOCK_SIZE 104
/**< @ingroup LacHashDefs
 *  SHA3_384 block size in bytes */
#define LAC_HASH_SHA3_384_DIGEST_SIZE 48
/**< @ingroup LacHashDefs
 *  SHA3_384 digest length in bytes */
#define LAC_HASH_SHA3_384_STATE_SIZE 48
/**< @ingroup LacHashDefs
 *  SHA3_384 state size */

/* Constants for SHA3_512 algorithm  */
#define LAC_HASH_SHA3_512_BLOCK_SIZE 72
/**< @ingroup LacHashDefs
 *  SHA3_512 block size in bytes */
#define LAC_HASH_SHA3_512_DIGEST_SIZE 64
/**< @ingroup LacHashDefs
 *  SHA3_512 digest length in bytes */
#define LAC_HASH_SHA3_512_STATE_SIZE 64
/**< @ingroup LacHashDefs
 *  SHA3_512 state size */

#define LAC_HASH_SHA3_STATEFUL_STATE_SIZE 200

/* Constants for SM3 algorithm  */
#define LAC_HASH_SM3_BLOCK_SIZE 64
/**< @ingroup LacHashDefs
 *  SM3 block size in bytes */
#define LAC_HASH_SM3_DIGEST_SIZE 32
/**< @ingroup LacHashDefs
 *  SM3 digest length */
#define LAC_HASH_SM3_STATE_SIZE 32
/**< @ingroup LacHashDefs
 *  SM3 state size */

/* Constants for POLY algorithm  */
#define LAC_HASH_POLY_BLOCK_SIZE 64
/**< @ingroup LacHashDefs
 *  POLY block size in bytes */
#define LAC_HASH_POLY_DIGEST_SIZE 16
/**< @ingroup LacHashDefs
 *  POLY digest length */
#define LAC_HASH_POLY_STATE_SIZE 0
/**< @ingroup LacHashDefs
 *  POLY state size */

/* Constants for XCBC precompute algorithm  */
#define LAC_HASH_XCBC_PRECOMP_KEY_NUM 3
/**< @ingroup LacHashDefs
 *  The Pre-compute operation involves deriving 3 128-bit
 *  keys (K1, K2 and K3) */

/* Constants for XCBC MAC algorithm  */
#define LAC_HASH_XCBC_MAC_BLOCK_SIZE 16
/**< @ingroup LacHashDefs
 *  XCBC_MAC block size in bytes */
#define LAC_HASH_XCBC_MAC_128_DIGEST_SIZE 16
/**< @ingroup LacHashDefs
 *  XCBC_MAC_PRF_128 digest length in bytes */

/* Constants for AES CMAC algorithm  */
#define LAC_HASH_CMAC_BLOCK_SIZE 16
/**< @ingroup LacHashDefs
 *  AES CMAC block size in bytes */
#define LAC_HASH_CMAC_128_DIGEST_SIZE 16
/**< @ingroup LacHashDefs
 *  AES CMAC digest length in bytes */

/* constants for AES CCM */
#define LAC_HASH_AES_CCM_BLOCK_SIZE 16
/**< @ingroup LacHashDefs
 *  block size for CBC-MAC part of CCM */
#define LAC_HASH_AES_CCM_DIGEST_SIZE 16
/**< @ingroup LacHashDefs
 *  untruncated size of authentication field */

/* constants for AES GCM */
#define LAC_HASH_AES_GCM_BLOCK_SIZE 16
/**< @ingroup LacHashDefs
 *  block size for Galois Hash 128 part of CCM */
#define LAC_HASH_AES_GCM_DIGEST_SIZE 16
/**< @ingroup LacHashDefs
 *  untruncated size of authentication field */

/* constants for KASUMI F9 */
#define LAC_HASH_KASUMI_F9_BLOCK_SIZE 8
/**< @ingroup LacHashDefs
 *  KASUMI_F9 block size in bytes */
#define LAC_HASH_KASUMI_F9_DIGEST_SIZE 4
/**< @ingroup LacHashDefs
 *  KASUMI_F9 digest size in bytes */

/* constants for SNOW3G UIA2 */
#define LAC_HASH_SNOW3G_UIA2_BLOCK_SIZE 8
/**< @ingroup LacHashDefs
 *  SNOW3G UIA2 block size in bytes */
#define LAC_HASH_SNOW3G_UIA2_DIGEST_SIZE 4
/**< @ingroup LacHashDefs
 *  SNOW3G UIA2 digest size in bytes */

/* constants for AES CBC MAC */
#define LAC_HASH_AES_CBC_MAC_BLOCK_SIZE 16
/**< @ingroup LacHashDefs
 *  AES CBC MAC block size in bytes */
#define LAC_HASH_AES_CBC_MAC_DIGEST_SIZE 16
/**< @ingroup LacHashDefs
 *  AES CBC MAC digest size in bytes */

#define LAC_HASH_ZUC_EIA3_BLOCK_SIZE 4
/**< @ingroup LacHashDefs
 *  ZUC EIA3 block size in bytes */
#define LAC_HASH_ZUC_EIA3_DIGEST_SIZE 4
/**< @ingroup LacHashDefs
 *  ZUC EIA3 digest size in bytes */

/* constants for AES GCM ICV allowed sizes */
#define LAC_HASH_AES_GCM_ICV_SIZE_8 8
#define LAC_HASH_AES_GCM_ICV_SIZE_12 12
#define LAC_HASH_AES_GCM_ICV_SIZE_16 16

/* constants for AES CCM ICV allowed sizes */
#define LAC_HASH_AES_CCM_ICV_SIZE_MIN 4
#define LAC_HASH_AES_CCM_ICV_SIZE_MAX 16

/* constants for authentication algorithms */
#define LAC_HASH_IPAD_BYTE 0x36
/**< @ingroup LacHashDefs
 *  Ipad Byte */
#define LAC_HASH_OPAD_BYTE 0x5c
/**< @ingroup LacHashDefs
 *  Opad Byte */

#define LAC_HASH_IPAD_4_BYTES 0x36363636
/**< @ingroup LacHashDefs
 *  Ipad for 4 Bytes */
#define LAC_HASH_OPAD_4_BYTES 0x5c5c5c5c
/**< @ingroup LacHashDefs
 *  Opad for 4 Bytes */

/* Key Modifier (KM) value used in Kasumi algorithm in F9 mode to XOR
 * Integrity Key (IK) */
#define LAC_HASH_KASUMI_F9_KEY_MODIFIER_4_BYTES 0xAAAAAAAA
/**< @ingroup LacHashDefs
 *  Kasumi F9 Key Modifier for 4 bytes */

#define LAC_SYM_QAT_HASH_IV_REQ_MAX_SIZE_QW 2
/**< @ingroup LacSymQatHash
 * Maximum size of IV embedded in the request.
 * This is set to 2, namely 4 LONGWORDS. */

#define LAC_SYM_QAT_HASH_STATE1_MAX_SIZE_BYTES LAC_HASH_SHA512_BLOCK_SIZE
/**< @ingroup LacSymQatHash
 * Maximum size of state1 in the hash setup block of the content descriptor.
 * This is set to the block size of SHA512. */

#define LAC_SYM_QAT_HASH_STATE2_MAX_SIZE_BYTES LAC_HASH_SHA512_BLOCK_SIZE
/**< @ingroup LacSymQatHash
 * Maximum size of state2 in the hash setup block of the content descriptor.
 * This is set to the block size of SHA512. */

#define LAC_MAX_INNER_OUTER_PREFIX_SIZE_BYTES 255
/**< Maximum size of the inner and outer prefix for nested hashing operations.
 * This is got from the maximum size supported by the accelerator which stores
 * the size in an 8bit field */

#define LAC_MAX_HASH_STATE_STORAGE_SIZE                                        \
	(sizeof(icp_qat_hw_auth_counter_t) + LAC_HASH_SHA3_STATEFUL_STATE_SIZE)
/**< Maximum size of the hash state storage section of the hash state prefix
 * buffer */

#define LAC_MAX_HASH_STATE_BUFFER_SIZE_BYTES                                   \
	LAC_MAX_HASH_STATE_STORAGE_SIZE +                                      \
	    (LAC_ALIGN_POW2_ROUNDUP(LAC_MAX_INNER_OUTER_PREFIX_SIZE_BYTES,     \
				    LAC_QUAD_WORD_IN_BYTES) *                  \
	     2)
/**< Maximum size of the hash state prefix buffer will be for nested hash when
 * there is the maximum sized inner prefix and outer prefix */

#define LAC_MAX_AAD_SIZE_BYTES 256
/**< Maximum size of AAD in bytes */

#define IS_HMAC_ALG(algorithm)                                                 \
	((algorithm == CPA_CY_SYM_HASH_MD5) ||                                 \
	 (algorithm == CPA_CY_SYM_HASH_SHA1) ||                                \
	 (algorithm == CPA_CY_SYM_HASH_SHA224) ||                              \
	 (algorithm == CPA_CY_SYM_HASH_SHA256) ||                              \
	 (algorithm == CPA_CY_SYM_HASH_SHA384) ||                              \
	 (algorithm == CPA_CY_SYM_HASH_SHA512) ||                              \
	 (algorithm == CPA_CY_SYM_HASH_SM3)) ||                                \
	    (LAC_HASH_IS_SHA3(algorithm))
/**< @ingroup LacSymQatHash
 * Macro to detect if the hash algorithm is a HMAC algorithm */

#define IS_HASH_MODE_1(qatHashMode) (ICP_QAT_HW_AUTH_MODE1 == qatHashMode)
/**< @ingroup LacSymQatHash
 * Macro to detect is qat hash mode is set to 1 (precompute mode)
 * only used with algorithms in hash mode CPA_CY_SYM_HASH_MODE_AUTH */

#define IS_HASH_MODE_2(qatHashMode) (ICP_QAT_HW_AUTH_MODE2 == qatHashMode)
/**< @ingroup LacSymQatHash
 * Macro to detect is qat hash mode is set to 2. This is used for TLS and
 * mode 2 HMAC (no preompute mode) */

#define IS_HASH_MODE_2_AUTH(qatHashMode, hashMode)                             \
	((IS_HASH_MODE_2(qatHashMode)) &&                                      \
	 (CPA_CY_SYM_HASH_MODE_AUTH == hashMode))
/**< @ingroup LacSymQatHash
 * Macro to check for qat hash mode is set to 2 and the hash mode is
 * Auth. This applies to HMAC algorithms (no pre compute). This is used
 * to differentiate between TLS and HMAC */

#define IS_HASH_MODE_2_NESTED(qatHashMode, hashMode)                           \
	((IS_HASH_MODE_2(qatHashMode)) &&                                      \
	 (CPA_CY_SYM_HASH_MODE_NESTED == hashMode))
/**< @ingroup LacSymQatHash
 * Macro to check for qat hash mode is set to 2 and the LAC hash mode is
 * Nested. This applies to TLS. This is used to differentiate between
 * TLS and HMAC */

#define LAC_HASH_IS_SHA3(algo)                                                 \
	((algo == CPA_CY_SYM_HASH_SHA3_224) ||                                 \
	 (algo == CPA_CY_SYM_HASH_SHA3_256) ||                                 \
	 (algo == CPA_CY_SYM_HASH_SHA3_384) ||                                 \
	 (algo == CPA_CY_SYM_HASH_SHA3_512))
/**< @ingroup LacSymQatHash
 * Macro to check if the hash algorithm is SHA3 */

#endif /* LAC_SYM_HASH_DEFS_H */
