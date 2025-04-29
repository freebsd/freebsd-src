/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_qat_hash_defs_lookup.c      Hash Definitions Lookup
 *
 * @ingroup LacHashDefsLookup
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

#include "cpa.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "lac_common.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "icp_adf_transport.h"
#include "lac_sym.h"
#include "icp_qat_fw_la.h"
#include "lac_sym_qat_hash_defs_lookup.h"
#include "lac_sal_types_crypto.h"
#include "lac_sym_hash_defs.h"

/* state size for xcbc mac consists of 3 * 16 byte keys */
#define LAC_SYM_QAT_XCBC_STATE_SIZE ((LAC_HASH_XCBC_MAC_BLOCK_SIZE)*3)

#define LAC_SYM_QAT_CMAC_STATE_SIZE ((LAC_HASH_CMAC_BLOCK_SIZE)*3)

/* This type is used for the mapping between the hash algorithm and
 * the corresponding hash definitions structure */
typedef struct lac_sym_qat_hash_def_map_s {
	CpaCySymHashAlgorithm hashAlgorithm;
	/* hash algorithm */
	lac_sym_qat_hash_defs_t hashDefs;
	/* hash definitions pointers */
} lac_sym_qat_hash_def_map_t;

/*
*******************************************************************************
* Static Variables
*******************************************************************************
*/

/* initialisers as defined in FIPS and RFCS for digest operations */

/* md5 16 bytes - Initialiser state can be found in RFC 1321*/
static Cpa8U md5InitialState[LAC_HASH_MD5_STATE_SIZE] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
};

/* SHA1 - 20 bytes - Initialiser state can be found in FIPS stds 180-2 */
static Cpa8U sha1InitialState[LAC_HASH_SHA1_STATE_SIZE] = {
	0x67, 0x45, 0x23, 0x01, 0xef, 0xcd, 0xab, 0x89, 0x98, 0xba,
	0xdc, 0xfe, 0x10, 0x32, 0x54, 0x76, 0xc3, 0xd2, 0xe1, 0xf0
};

/* SHA 224 - 32 bytes - Initialiser state can be found in FIPS stds 180-2 */
static Cpa8U sha224InitialState[LAC_HASH_SHA224_STATE_SIZE] = {
	0xc1, 0x05, 0x9e, 0xd8, 0x36, 0x7c, 0xd5, 0x07, 0x30, 0x70, 0xdd,
	0x17, 0xf7, 0x0e, 0x59, 0x39, 0xff, 0xc0, 0x0b, 0x31, 0x68, 0x58,
	0x15, 0x11, 0x64, 0xf9, 0x8f, 0xa7, 0xbe, 0xfa, 0x4f, 0xa4
};

/* SHA 256 - 32 bytes - Initialiser state can be found in FIPS stds 180-2 */
static Cpa8U sha256InitialState[LAC_HASH_SHA256_STATE_SIZE] = {
	0x6a, 0x09, 0xe6, 0x67, 0xbb, 0x67, 0xae, 0x85, 0x3c, 0x6e, 0xf3,
	0x72, 0xa5, 0x4f, 0xf5, 0x3a, 0x51, 0x0e, 0x52, 0x7f, 0x9b, 0x05,
	0x68, 0x8c, 0x1f, 0x83, 0xd9, 0xab, 0x5b, 0xe0, 0xcd, 0x19
};

/* SHA 384 - 64 bytes - Initialiser state can be found in FIPS stds 180-2 */
static Cpa8U sha384InitialState[LAC_HASH_SHA384_STATE_SIZE] = {
	0xcb, 0xbb, 0x9d, 0x5d, 0xc1, 0x05, 0x9e, 0xd8, 0x62, 0x9a, 0x29,
	0x2a, 0x36, 0x7c, 0xd5, 0x07, 0x91, 0x59, 0x01, 0x5a, 0x30, 0x70,
	0xdd, 0x17, 0x15, 0x2f, 0xec, 0xd8, 0xf7, 0x0e, 0x59, 0x39, 0x67,
	0x33, 0x26, 0x67, 0xff, 0xc0, 0x0b, 0x31, 0x8e, 0xb4, 0x4a, 0x87,
	0x68, 0x58, 0x15, 0x11, 0xdb, 0x0c, 0x2e, 0x0d, 0x64, 0xf9, 0x8f,
	0xa7, 0x47, 0xb5, 0x48, 0x1d, 0xbe, 0xfa, 0x4f, 0xa4
};

/* SHA 512 - 64 bytes - Initialiser state can be found in FIPS stds 180-2 */
static Cpa8U sha512InitialState[LAC_HASH_SHA512_STATE_SIZE] = {
	0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xbc, 0xc9, 0x08, 0xbb, 0x67, 0xae,
	0x85, 0x84, 0xca, 0xa7, 0x3b, 0x3c, 0x6e, 0xf3, 0x72, 0xfe, 0x94,
	0xf8, 0x2b, 0xa5, 0x4f, 0xf5, 0x3a, 0x5f, 0x1d, 0x36, 0xf1, 0x51,
	0x0e, 0x52, 0x7f, 0xad, 0xe6, 0x82, 0xd1, 0x9b, 0x05, 0x68, 0x8c,
	0x2b, 0x3e, 0x6c, 0x1f, 0x1f, 0x83, 0xd9, 0xab, 0xfb, 0x41, 0xbd,
	0x6b, 0x5b, 0xe0, 0xcd, 0x19, 0x13, 0x7e, 0x21, 0x79
};

/* SHA3 224 - 28 bytes  */
static Cpa8U sha3_224InitialState[LAC_HASH_SHA3_224_STATE_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* SHA3 256 - 32 bytes  */
static Cpa8U sha3_256InitialState[LAC_HASH_SHA3_256_STATE_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* SHA3 384 - 48 bytes  */
static Cpa8U sha3_384InitialState[LAC_HASH_SHA3_384_STATE_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* SHA3 512 - 64 bytes  */
static Cpa8U sha3_512InitialState[LAC_HASH_SHA3_512_STATE_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* SM3 - 32 bytes */
static Cpa8U sm3InitialState[LAC_HASH_SM3_STATE_SIZE] = {
	0x73, 0x80, 0x16, 0x6f, 0x49, 0x14, 0xb2, 0xb9, 0x17, 0x24, 0x42,
	0xd7, 0xda, 0x8a, 0x06, 0x00, 0xa9, 0x6f, 0x30, 0xbc, 0x16, 0x31,
	0x38, 0xaa, 0xe3, 0x8d, 0xee, 0x4d, 0xb0, 0xfb, 0x0e, 0x4e
};

/* Constants used in generating K1, K2, K3 from a Key for AES_XCBC_MAC
 * State defined in RFC 3566 */
static Cpa8U aesXcbcKeySeed[LAC_SYM_QAT_XCBC_STATE_SIZE] = {
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
};

static Cpa8U aesCmacKeySeed[LAC_HASH_CMAC_BLOCK_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Hash Algorithm specific structure */

static lac_sym_qat_hash_alg_info_t md5Info = { LAC_HASH_MD5_DIGEST_SIZE,
					       LAC_HASH_MD5_BLOCK_SIZE,
					       md5InitialState,
					       LAC_HASH_MD5_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t sha1Info = { LAC_HASH_SHA1_DIGEST_SIZE,
						LAC_HASH_SHA1_BLOCK_SIZE,
						sha1InitialState,
						LAC_HASH_SHA1_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t sha224Info = { LAC_HASH_SHA224_DIGEST_SIZE,
						  LAC_HASH_SHA224_BLOCK_SIZE,
						  sha224InitialState,
						  LAC_HASH_SHA224_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t sha256Info = { LAC_HASH_SHA256_DIGEST_SIZE,
						  LAC_HASH_SHA256_BLOCK_SIZE,
						  sha256InitialState,
						  LAC_HASH_SHA256_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t sha384Info = { LAC_HASH_SHA384_DIGEST_SIZE,
						  LAC_HASH_SHA384_BLOCK_SIZE,
						  sha384InitialState,
						  LAC_HASH_SHA384_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t sha512Info = { LAC_HASH_SHA512_DIGEST_SIZE,
						  LAC_HASH_SHA512_BLOCK_SIZE,
						  sha512InitialState,
						  LAC_HASH_SHA512_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t sha3_224Info = {
	LAC_HASH_SHA3_224_DIGEST_SIZE,
	LAC_HASH_SHA3_224_BLOCK_SIZE,
	sha3_224InitialState,
	LAC_HASH_SHA3_224_STATE_SIZE
};

static lac_sym_qat_hash_alg_info_t sha3_256Info = {
	LAC_HASH_SHA3_256_DIGEST_SIZE,
	LAC_HASH_SHA3_256_BLOCK_SIZE,
	sha3_256InitialState,
	LAC_HASH_SHA3_256_STATE_SIZE
};

static lac_sym_qat_hash_alg_info_t sha3_384Info = {
	LAC_HASH_SHA3_384_DIGEST_SIZE,
	LAC_HASH_SHA3_384_BLOCK_SIZE,
	sha3_384InitialState,
	LAC_HASH_SHA3_384_STATE_SIZE
};

static lac_sym_qat_hash_alg_info_t sha3_512Info = {
	LAC_HASH_SHA3_512_DIGEST_SIZE,
	LAC_HASH_SHA3_512_BLOCK_SIZE,
	sha3_512InitialState,
	LAC_HASH_SHA3_512_STATE_SIZE
};

static lac_sym_qat_hash_alg_info_t sm3Info = { LAC_HASH_SM3_DIGEST_SIZE,
					       LAC_HASH_SM3_BLOCK_SIZE,
					       sm3InitialState,
					       LAC_HASH_SM3_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t polyInfo = { LAC_HASH_POLY_DIGEST_SIZE,
						LAC_HASH_POLY_BLOCK_SIZE,
						NULL, /* initial state */
						LAC_HASH_POLY_STATE_SIZE };

static lac_sym_qat_hash_alg_info_t xcbcMacInfo = {
	LAC_HASH_XCBC_MAC_128_DIGEST_SIZE,
	LAC_HASH_XCBC_MAC_BLOCK_SIZE,
	aesXcbcKeySeed,
	LAC_SYM_QAT_XCBC_STATE_SIZE
};

static lac_sym_qat_hash_alg_info_t aesCmacInfo = {
	LAC_HASH_CMAC_128_DIGEST_SIZE,
	LAC_HASH_CMAC_BLOCK_SIZE,
	aesCmacKeySeed,
	LAC_SYM_QAT_CMAC_STATE_SIZE
};

static lac_sym_qat_hash_alg_info_t aesCcmInfo = {
	LAC_HASH_AES_CCM_DIGEST_SIZE,
	LAC_HASH_AES_CCM_BLOCK_SIZE,
	NULL, /* initial state */
	0     /* state size */
};

static lac_sym_qat_hash_alg_info_t aesGcmInfo = {
	LAC_HASH_AES_GCM_DIGEST_SIZE,
	LAC_HASH_AES_GCM_BLOCK_SIZE,
	NULL, /* initial state */
	0     /* state size */
};

static lac_sym_qat_hash_alg_info_t kasumiF9Info = {
	LAC_HASH_KASUMI_F9_DIGEST_SIZE,
	LAC_HASH_KASUMI_F9_BLOCK_SIZE,
	NULL, /* initial state */
	0     /* state size */
};

static lac_sym_qat_hash_alg_info_t snow3gUia2Info = {
	LAC_HASH_SNOW3G_UIA2_DIGEST_SIZE,
	LAC_HASH_SNOW3G_UIA2_BLOCK_SIZE,
	NULL, /* initial state */
	0     /* state size */
};

static lac_sym_qat_hash_alg_info_t aesCbcMacInfo = {
	LAC_HASH_AES_CBC_MAC_DIGEST_SIZE,
	LAC_HASH_AES_CBC_MAC_BLOCK_SIZE,
	NULL,
	0
};

static lac_sym_qat_hash_alg_info_t zucEia3Info = {
	LAC_HASH_ZUC_EIA3_DIGEST_SIZE,
	LAC_HASH_ZUC_EIA3_BLOCK_SIZE,
	NULL, /* initial state */
	0     /* state size */
};
/* Hash QAT specific structures */

static lac_sym_qat_hash_qat_info_t md5Config = { ICP_QAT_HW_AUTH_ALGO_MD5,
						 LAC_HASH_MD5_BLOCK_SIZE,
						 ICP_QAT_HW_MD5_STATE1_SZ,
						 ICP_QAT_HW_MD5_STATE2_SZ };

static lac_sym_qat_hash_qat_info_t sha1Config = { ICP_QAT_HW_AUTH_ALGO_SHA1,
						  LAC_HASH_SHA1_BLOCK_SIZE,
						  ICP_QAT_HW_SHA1_STATE1_SZ,
						  ICP_QAT_HW_SHA1_STATE2_SZ };

static lac_sym_qat_hash_qat_info_t sha224Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA224,
	LAC_HASH_SHA224_BLOCK_SIZE,
	ICP_QAT_HW_SHA224_STATE1_SZ,
	ICP_QAT_HW_SHA224_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sha256Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA256,
	LAC_HASH_SHA256_BLOCK_SIZE,
	ICP_QAT_HW_SHA256_STATE1_SZ,
	ICP_QAT_HW_SHA256_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sha384Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA384,
	LAC_HASH_SHA384_BLOCK_SIZE,
	ICP_QAT_HW_SHA384_STATE1_SZ,
	ICP_QAT_HW_SHA384_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sha512Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA512,
	LAC_HASH_SHA512_BLOCK_SIZE,
	ICP_QAT_HW_SHA512_STATE1_SZ,
	ICP_QAT_HW_SHA512_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sha3_224Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA3_224,
	LAC_HASH_SHA3_224_BLOCK_SIZE,
	ICP_QAT_HW_SHA3_224_STATE1_SZ,
	ICP_QAT_HW_SHA3_224_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sha3_256Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA3_256,
	LAC_HASH_SHA3_256_BLOCK_SIZE,
	ICP_QAT_HW_SHA3_256_STATE1_SZ,
	ICP_QAT_HW_SHA3_256_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sha3_384Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA3_384,
	LAC_HASH_SHA3_384_BLOCK_SIZE,
	ICP_QAT_HW_SHA3_384_STATE1_SZ,
	ICP_QAT_HW_SHA3_384_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sha3_512Config = {
	ICP_QAT_HW_AUTH_ALGO_SHA3_512,
	LAC_HASH_SHA3_512_BLOCK_SIZE,
	ICP_QAT_HW_SHA3_512_STATE1_SZ,
	ICP_QAT_HW_SHA3_512_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t sm3Config = { ICP_QAT_HW_AUTH_ALGO_SM3,
						 LAC_HASH_SM3_BLOCK_SIZE,
						 ICP_QAT_HW_SM3_STATE1_SZ,
						 ICP_QAT_HW_SM3_STATE2_SZ };

static lac_sym_qat_hash_qat_info_t polyConfig = { ICP_QAT_HW_AUTH_ALGO_POLY,
						  LAC_HASH_POLY_BLOCK_SIZE,
						  0,
						  0 };

static lac_sym_qat_hash_qat_info_t xcbcMacConfig = {
	ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC,
	0,
	ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ,
	LAC_SYM_QAT_XCBC_STATE_SIZE
};

static lac_sym_qat_hash_qat_info_t aesCmacConfig = {
	ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC,
	0,
	ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ,
	LAC_SYM_QAT_CMAC_STATE_SIZE
};

static lac_sym_qat_hash_qat_info_t aesCcmConfig = {
	ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC,
	0,
	ICP_QAT_HW_AES_CBC_MAC_STATE1_SZ,
	ICP_QAT_HW_AES_CBC_MAC_KEY_SZ + ICP_QAT_HW_AES_CCM_CBC_E_CTR0_SZ
};

static lac_sym_qat_hash_qat_info_t aesGcmConfig = {
	ICP_QAT_HW_AUTH_ALGO_GALOIS_128,
	0,
	ICP_QAT_HW_GALOIS_128_STATE1_SZ,
	ICP_QAT_HW_GALOIS_H_SZ + ICP_QAT_HW_GALOIS_LEN_A_SZ +
	    ICP_QAT_HW_GALOIS_E_CTR0_SZ
};

static lac_sym_qat_hash_qat_info_t kasumiF9Config = {
	ICP_QAT_HW_AUTH_ALGO_KASUMI_F9,
	0,
	ICP_QAT_HW_KASUMI_F9_STATE1_SZ,
	ICP_QAT_HW_KASUMI_F9_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t snow3gUia2Config = {
	ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2,
	0,
	ICP_QAT_HW_SNOW_3G_UIA2_STATE1_SZ,
	ICP_QAT_HW_SNOW_3G_UIA2_STATE2_SZ
};

static lac_sym_qat_hash_qat_info_t aesCbcMacConfig = {
	ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC,
	0,
	ICP_QAT_HW_AES_CBC_MAC_STATE1_SZ,
	ICP_QAT_HW_AES_CBC_MAC_STATE1_SZ + ICP_QAT_HW_AES_CBC_MAC_STATE1_SZ
};

static lac_sym_qat_hash_qat_info_t zucEia3Config = {
	ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3,
	0,
	ICP_QAT_HW_ZUC_3G_EIA3_STATE1_SZ,
	ICP_QAT_HW_ZUC_3G_EIA3_STATE2_SZ
};

/* Array of mappings between algorithm and info structure
 * This array is used to populate the lookup table */
static lac_sym_qat_hash_def_map_t lacHashDefsMapping[] = {
	{ CPA_CY_SYM_HASH_MD5, { &md5Info, &md5Config } },
	{ CPA_CY_SYM_HASH_SHA1, { &sha1Info, &sha1Config } },
	{ CPA_CY_SYM_HASH_SHA224, { &sha224Info, &sha224Config } },
	{ CPA_CY_SYM_HASH_SHA256, { &sha256Info, &sha256Config } },
	{ CPA_CY_SYM_HASH_SHA384, { &sha384Info, &sha384Config } },
	{ CPA_CY_SYM_HASH_SHA512, { &sha512Info, &sha512Config } },
	{ CPA_CY_SYM_HASH_SHA3_224, { &sha3_224Info, &sha3_224Config } },
	{ CPA_CY_SYM_HASH_SHA3_256, { &sha3_256Info, &sha3_256Config } },
	{ CPA_CY_SYM_HASH_SHA3_384, { &sha3_384Info, &sha3_384Config } },
	{ CPA_CY_SYM_HASH_SHA3_512, { &sha3_512Info, &sha3_512Config } },
	{ CPA_CY_SYM_HASH_SM3, { &sm3Info, &sm3Config } },
	{ CPA_CY_SYM_HASH_POLY, { &polyInfo, &polyConfig } },
	{ CPA_CY_SYM_HASH_AES_XCBC, { &xcbcMacInfo, &xcbcMacConfig } },
	{ CPA_CY_SYM_HASH_AES_CMAC, { &aesCmacInfo, &aesCmacConfig } },
	{ CPA_CY_SYM_HASH_AES_CCM, { &aesCcmInfo, &aesCcmConfig } },
	{ CPA_CY_SYM_HASH_AES_GCM, { &aesGcmInfo, &aesGcmConfig } },
	{ CPA_CY_SYM_HASH_KASUMI_F9, { &kasumiF9Info, &kasumiF9Config } },
	{ CPA_CY_SYM_HASH_SNOW3G_UIA2, { &snow3gUia2Info, &snow3gUia2Config } },
	{ CPA_CY_SYM_HASH_AES_GMAC, { &aesGcmInfo, &aesGcmConfig } },
	{ CPA_CY_SYM_HASH_ZUC_EIA3, { &zucEia3Info, &zucEia3Config } },
	{ CPA_CY_SYM_HASH_AES_CBC_MAC, { &aesCbcMacInfo, &aesCbcMacConfig } }
};

/*
 * LacSymQat_HashLookupInit
 */
CpaStatus
LacSymQat_HashLookupInit(CpaInstanceHandle instanceHandle)
{
	Cpa32U entry = 0;
	Cpa32U numEntries = 0;
	Cpa32U arraySize = 0;
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaCySymHashAlgorithm hashAlg = CPA_CY_SYM_HASH_NONE;
	sal_service_t *pService = (sal_service_t *)instanceHandle;
	lac_sym_qat_hash_defs_t **pLacHashLookupDefs;

	arraySize =
	    (CPA_CY_HASH_ALG_END + 1) * sizeof(lac_sym_qat_hash_defs_t *);
	/* Size round up for performance */
	arraySize = LAC_ALIGN_POW2_ROUNDUP(arraySize, LAC_64BYTE_ALIGNMENT);

	pLacHashLookupDefs = LAC_OS_MALLOC(arraySize);
	if (NULL == pLacHashLookupDefs) {
		return CPA_STATUS_RESOURCE;
	}

	LAC_OS_BZERO(pLacHashLookupDefs, arraySize);
	numEntries =
	    sizeof(lacHashDefsMapping) / sizeof(lac_sym_qat_hash_def_map_t);

	/* initialise the hash lookup definitions table so that the algorithm
	 * can be used to index into the table */
	for (entry = 0; entry < numEntries; entry++) {
		hashAlg = lacHashDefsMapping[entry].hashAlgorithm;

		pLacHashLookupDefs[hashAlg] =
		    &(lacHashDefsMapping[entry].hashDefs);
	}

	((sal_crypto_service_t *)pService)->pLacHashLookupDefs =
	    pLacHashLookupDefs;

	return status;
}

/*
 * LacSymQat_HashAlgLookupGet
 */
void
LacSymQat_HashAlgLookupGet(CpaInstanceHandle instanceHandle,
			   CpaCySymHashAlgorithm hashAlgorithm,
			   lac_sym_qat_hash_alg_info_t **ppHashAlgInfo)
{
	sal_service_t *pService = (sal_service_t *)instanceHandle;
	lac_sym_qat_hash_defs_t **pLacHashLookupDefs =
	    ((sal_crypto_service_t *)pService)->pLacHashLookupDefs;

	*ppHashAlgInfo = pLacHashLookupDefs[hashAlgorithm]->algInfo;
}

/*
 * LacSymQat_HashDefsLookupGet
 */
void
LacSymQat_HashDefsLookupGet(CpaInstanceHandle instanceHandle,
			    CpaCySymHashAlgorithm hashAlgorithm,
			    lac_sym_qat_hash_defs_t **ppHashDefsInfo)
{
	sal_service_t *pService = (sal_service_t *)instanceHandle;
	lac_sym_qat_hash_defs_t **pLacHashLookupDefs =
	    ((sal_crypto_service_t *)pService)->pLacHashLookupDefs;

	*ppHashDefsInfo = pLacHashLookupDefs[hashAlgorithm];
}
