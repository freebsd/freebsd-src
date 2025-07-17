/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
/**
 *****************************************************************************
 * @file icp_qat_hw.h
 * @defgroup icp_qat_hw_defs ICP QAT HW definitions
 * @ingroup icp_qat_hw
 * @description
 *      This file documents definitions for the QAT HW
 *
 *****************************************************************************/

#ifndef _ICP_QAT_HW_H_
#define _ICP_QAT_HW_H_

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

/* ========================================================================= */
/*                                                  AccelerationEngine       */
/* ========================================================================= */

typedef enum {
	ICP_QAT_HW_AE_0 = 0,	     /*!< ID of AE0 */
	ICP_QAT_HW_AE_1 = 1,	     /*!< ID of AE1 */
	ICP_QAT_HW_AE_2 = 2,	     /*!< ID of AE2 */
	ICP_QAT_HW_AE_3 = 3,	     /*!< ID of AE3 */
	ICP_QAT_HW_AE_4 = 4,	     /*!< ID of AE4 */
	ICP_QAT_HW_AE_5 = 5,	     /*!< ID of AE5 */
	ICP_QAT_HW_AE_6 = 6,	     /*!< ID of AE6 */
	ICP_QAT_HW_AE_7 = 7,	     /*!< ID of AE7 */
	ICP_QAT_HW_AE_8 = 8,	     /*!< ID of AE8 */
	ICP_QAT_HW_AE_9 = 9,	     /*!< ID of AE9 */
	ICP_QAT_HW_AE_10 = 10,	     /*!< ID of AE10 */
	ICP_QAT_HW_AE_11 = 11,	     /*!< ID of AE11 */
	ICP_QAT_HW_AE_12 = 12,	     /*!< ID of AE12 */
	ICP_QAT_HW_AE_13 = 13,	     /*!< ID of AE13 */
	ICP_QAT_HW_AE_14 = 14,	     /*!< ID of AE14 */
	ICP_QAT_HW_AE_15 = 15,	     /*!< ID of AE15 */
	ICP_QAT_HW_AE_DELIMITER = 16 /**< Delimiter type */
} icp_qat_hw_ae_id_t;

/* ========================================================================= */
/*                                                                 QAT       */
/* ========================================================================= */

typedef enum {
	ICP_QAT_HW_QAT_0 = 0,	     /*!< ID of QAT0 */
	ICP_QAT_HW_QAT_1 = 1,	     /*!< ID of QAT1 */
	ICP_QAT_HW_QAT_2 = 2,	     /*!< ID of QAT2 */
	ICP_QAT_HW_QAT_3 = 3,	     /*!< ID of QAT3 */
	ICP_QAT_HW_QAT_4 = 4,	     /*!< ID of QAT4 */
	ICP_QAT_HW_QAT_5 = 5,	     /*!< ID of QAT5 */
	ICP_QAT_HW_QAT_DELIMITER = 6 /**< Delimiter type */
} icp_qat_hw_qat_id_t;

/* ========================================================================= */
/*                                                  AUTH SLICE               */
/* ========================================================================= */

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Supported Authentication Algorithm types
 * @description
 *      Enumeration which is used to define the authenticate algorithms
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_AUTH_ALGO_NULL = 0,		/*!< Null hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA1 = 1,		/*!< SHA1 hashing */
	ICP_QAT_HW_AUTH_ALGO_MD5 = 2,		/*!< MD5 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA224 = 3,	/*!< SHA-224 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA256 = 4,	/*!< SHA-256 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA384 = 5,	/*!< SHA-384 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA512 = 6,	/*!< SHA-512 hashing */
	ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC = 7,	/*!< AES-XCBC-MAC hashing */
	ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC = 8,	/*!< AES-CBC-MAC hashing */
	ICP_QAT_HW_AUTH_ALGO_AES_F9 = 9,	/*!< AES F9 hashing */
	ICP_QAT_HW_AUTH_ALGO_GALOIS_128 = 10,	/*!< Galois 128 bit hashing */
	ICP_QAT_HW_AUTH_ALGO_GALOIS_64 = 11,	/*!< Galois 64 hashing */
	ICP_QAT_HW_AUTH_ALGO_KASUMI_F9 = 12,	/*!< Kasumi F9 hashing */
	ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2 = 13, /*!< UIA2/SNOW_3G F9 hashing */
	ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3 =
	    14,				    /*!< 128_EIA3/ZUC_3G hashing */
	ICP_QAT_HW_AUTH_ALGO_SM3 = 15,	    /*!< SM3 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA3_224 = 16, /*!< SHA3-224 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA3_256 = 17, /*!< SHA3-256 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA3_384 = 18, /*!< SHA3-384 hashing */
	ICP_QAT_HW_AUTH_ALGO_SHA3_512 = 19, /*!< SHA3-512 hashing */
	ICP_QAT_HW_AUTH_RESERVED_4 = 20,    /*!< Reserved */
	ICP_QAT_HW_AUTH_RESERVED_5 = 21,    /*!< Reserved */
	ICP_QAT_HW_AUTH_ALGO_POLY = 22,	    /*!< POLY hashing */
	ICP_QAT_HW_AUTH_ALGO_DELIMITER = 23 /**< Delimiter type */
} icp_qat_hw_auth_algo_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported Authentication modes
 * @description
 *      Enumeration which is used to define the authentication slice modes.
 *      The concept of modes is very specific to the QAT implementation. Its
 *      main use is differentiate how the algorithms are used i.e. mode0 SHA1
 *      will configure the QAT Auth Slice to do plain SHA1 hashing while mode1
 *      configures it to do SHA1 HMAC with precomputes and mode2 sets up the
 *      slice to do SHA1 HMAC with no precomputes (uses key directly)
 *
 * @Note
 *      Only some algorithms are valid in some of the modes. If you dont know
 *      what you are doing then refer back to the HW documentation
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_AUTH_MODE0 = 0,	   /*!< QAT Auth Mode0 configuration */
	ICP_QAT_HW_AUTH_MODE1 = 1,	   /*!< QAT Auth Mode1 configuration */
	ICP_QAT_HW_AUTH_MODE2 = 2,	   /*!< QAT AuthMode2 configuration */
	ICP_QAT_HW_AUTH_MODE_DELIMITER = 3 /**< Delimiter type */
} icp_qat_hw_auth_mode_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Auth configuration structure
 *
 * @description
 *      Definition of the format of the authentication slice configuration
 *
 *****************************************************************************/
typedef struct icp_qat_hw_auth_config_s {
	uint32_t config;
	/**< Configuration used for setting up the slice */

	uint32_t reserved;
	/**< Reserved */
} icp_qat_hw_auth_config_t;

/* Private defines */

/* Note: Bit positions have been defined for little endian ordering */
/*
*  AUTH CONFIG WORD BITMAP
*  + ===== + ------ + ------ + ------- + ------ + ------ + ----- + ----- + ------ + ------ + ---- + ----- + ----- + ----- +
*  |  Bit  | 63:56  | 55:52  |  51:48  | 47:32  | 31:24  | 23:22 | 21:18 |   17   |   16   |  15  | 14:8  |  7:4  |  3:0  |
*  + ===== + ------ + ------ + ------- + ------ + ------ + ----- + ----- + ------ + ------ + ---- + ----- + ------+ ----- +
*  | Usage |  Prog  | Resvd  |  Prog   | Resvd  | Resvd  | Algo  | Rsvrd |  SHA3  |  SHA3  |Rsvrd |  Cmp  | Mode  | Algo  |
*  |       |padding | Bits=0 | padding | Bits=0 | Bits=0 | SHA3  |       |Padding |Padding |      |       |       |       |
*  |       |  SHA3  |        |  SHA3   |        |        |       |       |Override|Disable |      |       |       |       |
*  |       |(prefix)|        |(postfix)|        |        |       |       |        |        |      |       |       |       |
*  + ===== + ------ + ------ + ------- + ------ + ------ + ----- + ----- + ------ + ------ + ---- + ----- + ----- + ------+
*/

/**< Flag mask & bit position */

#define QAT_AUTH_MODE_BITPOS 4
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the Auth mode */

#define QAT_AUTH_MODE_MASK 0xF
/**< @ingroup icp_qat_hw_defs
 * Four bit mask used for determining the Auth mode */

#define QAT_AUTH_ALGO_BITPOS 0
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the Auth Algo  */

#define QAT_AUTH_ALGO_MASK 0xF
/**< @ingroup icp_qat_hw_defs
 * Four bit mask used for determining the Auth algo */

#define QAT_AUTH_CMP_BITPOS 8
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the Auth Compare */

#define QAT_AUTH_CMP_MASK 0x7F
/**< @ingroup icp_qat_hw_defs
 * Seven bit mask used to determine the Auth Compare */

#define QAT_AUTH_SHA3_PADDING_DISABLE_BITPOS 16
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the Auth h/w
 * padding disable for SHA3.
 * Flag set to 0 => h/w is required to pad (default)
 * Flag set to 1 => No padding in h/w
 */

#define QAT_AUTH_SHA3_PADDING_DISABLE_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Single bit mask used to determine the Auth h/w
 * padding disable for SHA3.
 */

#define QAT_AUTH_SHA3_PADDING_OVERRIDE_BITPOS 17
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the Auth h/w
 * padding override for SHA3.
 * Flag set to 0 => default padding behaviour
 * implemented in SHA3-256 slice will take effect
 * (default hardware setting upon h/w reset)
 * Flag set to 1 => SHA3-core will not use the padding
 * sequence built into the SHA3 core. Instead, the
 * padding sequence specified in bits 48-51 and 56-63
 * of the 64-bit auth config word will apply
 * (corresponds with EAS bits 32-43).
 */

#define QAT_AUTH_SHA3_PADDING_OVERRIDE_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Single bit mask used to determine the Auth h/w
 * padding override for SHA3.
 */

#define QAT_AUTH_ALGO_SHA3_BITPOS 22
/**< @ingroup icp_qat_hw_defs
 * Starting bit position for indicating the
 * SHA3 Auth Algo
 */

#define QAT_AUTH_ALGO_SHA3_MASK 0x3
/**< @ingroup icp_qat_hw_defs
 * Two bit mask used for determining the
 * SHA3 Auth algo
 */

/**< Flag mask & bit position */

#define QAT_AUTH_SHA3_PROG_PADDING_POSTFIX_BITPOS 16
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the SHA3
 * flexible programmable padding postfix.
 * Note that these bits are set using macro
 * ICP_QAT_HW_AUTH_CONFIG_BUILD_UPPER and are
 * defined relative to the 32-bit value that
 * this macro returns. In effect, therefore, this
 * defines starting bit position 48 within the
 * 64-bit auth config word.
 */

#define QAT_AUTH_SHA3_PROG_PADDING_POSTFIX_MASK 0xF
/**< @ingroup icp_qat_hw_defs
 * Four-bit mask used to determine the SHA3
 * flexible programmable padding postfix
 */

#define QAT_AUTH_SHA3_PROG_PADDING_PREFIX_BITPOS 24
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the SHA3
 * flexible programmable padding prefix
 * Note that these bits are set using macro
 * ICP_QAT_HW_AUTH_CONFIG_BUILD_UPPER and are
 * defined relative to the 32-bit value that
 * this macro returns. In effect, therefore, this
 * defines starting bit position 56 within the
 * 64-bit auth config word.
 */

#define QAT_AUTH_SHA3_PROG_PADDING_PREFIX_MASK 0xFF
/**< @ingroup icp_qat_hw_defs
 * Eight-bit mask used to determine the SHA3
 * flexible programmable padding prefix
 */

/**< Flag usage - see additional notes @description for
 * ICP_QAT_HW_AUTH_CONFIG_BUILD and
 * ICP_QAT_HW_AUTH_CONFIG_BUILD_UPPER macros.
 */

#define QAT_AUTH_SHA3_HW_PADDING_ENABLE 0
/**< @ingroup icp_qat_hw_defs
 * This setting enables h/w padding for SHA3.
 */

#define QAT_AUTH_SHA3_HW_PADDING_DISABLE 1
/**< @ingroup icp_qat_hw_defs
 * This setting disables h/w padding for SHA3.
 */

#define QAT_AUTH_SHA3_PADDING_DISABLE_USE_DEFAULT 0
/**< @ingroup icp_qat_hw_defs
 * Default value for the Auth h/w padding disable.
 * If set to 0 for SHA3-256, h/w padding is enabled.
 * Padding_Disable is undefined for all non-SHA3-256
 * algos and is consequently set to the default of 0.
 */

#define QAT_AUTH_SHA3_PADDING_OVERRIDE_USE_DEFAULT 0
/**< @ingroup icp_qat_hw_defs
 * Value for the Auth h/w padding override for SHA3.
 * Flag set to 0 => default padding behaviour
 * implemented in SHA3-256 slice will take effect
 * (default hardware setting upon h/w reset)
 * For this setting of the override flag, all the
 * bits of the padding sequence specified
 * in bits 48-51 and 56-63 of the 64-bit
 * auth config word are set to 0 (reserved).
 */

#define QAT_AUTH_SHA3_PADDING_OVERRIDE_PROGRAMMABLE 1
/**< @ingroup icp_qat_hw_defs
 * Value for the Auth h/w padding override for SHA3.
 * Flag set to 1 => SHA3-core will not use the padding
 * sequence built into the SHA3 core. Instead, the
 * padding sequence specified in bits 48-51 and 56-63
 * of the 64-bit auth config word will apply
 * (corresponds with EAS bits 32-43).
 */

#define QAT_AUTH_SHA3_PROG_PADDING_POSTFIX_RESERVED 0
/**< @ingroup icp_qat_hw_defs
 * All the bits of the padding sequence specified in
 * bits 48-51 of the 64-bit auth config word are set
 * to 0 (reserved) if the padding override bit is set
 * to 0, indicating default padding.
 */

#define QAT_AUTH_SHA3_PROG_PADDING_PREFIX_RESERVED 0
/**< @ingroup icp_qat_hw_defs
 * All the bits of the padding sequence specified in
 * bits 56-63 of the 64-bit auth config word are set
 * to 0 (reserved) if the padding override bit is set
 * to 0, indicating default padding.
 */

/**
 ***************************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      The derived configuration word for the auth slice is based on the inputs
 *      of mode, algorithm type and compare length. The total size of the auth
 *      config word in the setup block is 64 bits however the size of the value
 *      returned by this macro is assumed to be only 32 bits (for now) and sets
 *      the lower 32 bits of the auth config word. Unfortunately, changing the
 *      size of the returned value to 64 bits will also require changes to the
 *      shared RAM constants table so the macro size will remain at 32 bits.
 *      This means that the padding sequence bits specified in bits 48-51 and
 *      56-63 of the 64-bit auth config word are NOT included in the
 *      ICP_QAT_HW_AUTH_CONFIG_BUILD macro and are defined in a
 *      separate macro, namely, ICP_QAT_HW_AUTH_CONFIG_BUILD_UPPER.
 *
 *      For the digest generation case the compare length is a don't care value.
 *      Furthermore, if the client will be doing the digest validation, the
 *      compare_length will not be used.
 *      The padding and padding override bits for SHA3 are set internally
 *      by the macro.
 *      Padding_Disable is set it to 0 for SHA3-256 algo only i.e. we want to
 *      enable this to provide the ability to test with h/w padding enabled.
 *      Padding_Disable has no meaning for all non-SHA3-256 algos and is
 *      consequently set the default of 0.
 *      Padding Override is set to 0, implying that the padding behaviour
 *      implemented in the SHA3-256 slice will take effect (default hardware
 *      setting upon h/w reset).
 *      This flag has no meaning for other algos, so is also set to the default
 *      for non-SHA3-256 algos.
 *
 * @param mode      Authentication mode to use
 * @param algo      Auth Algorithm to use
 * @param cmp_len   The length of the digest if the QAT is to the check
 *
 ****************************************************************************************/
#define ICP_QAT_HW_AUTH_CONFIG_BUILD(mode, algo, cmp_len)                                    \
	((((mode)&QAT_AUTH_MODE_MASK) << QAT_AUTH_MODE_BITPOS) |                             \
	 (((algo)&QAT_AUTH_ALGO_MASK) << QAT_AUTH_ALGO_BITPOS) |                             \
	 (((algo >> 4) & QAT_AUTH_ALGO_SHA3_MASK)                                            \
	  << QAT_AUTH_ALGO_SHA3_BITPOS) |                                                    \
	 (((QAT_AUTH_SHA3_PADDING_DISABLE_USE_DEFAULT)&QAT_AUTH_SHA3_PADDING_DISABLE_MASK)   \
	  << QAT_AUTH_SHA3_PADDING_DISABLE_BITPOS) |                                         \
	 (((QAT_AUTH_SHA3_PADDING_OVERRIDE_USE_DEFAULT)&QAT_AUTH_SHA3_PADDING_OVERRIDE_MASK) \
	  << QAT_AUTH_SHA3_PADDING_OVERRIDE_BITPOS) |                                        \
	 (((cmp_len)&QAT_AUTH_CMP_MASK) << QAT_AUTH_CMP_BITPOS))

/**
 ***************************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      This macro sets the upper 32 bits of the 64-bit auth config word.
 *      The sequence bits specified in bits 48-51 and 56-63 of the 64-bit auth
 *      config word are included in this macro, which is therefore assumed to
 *      return a 32-bit value.
 *      Note that the Padding Override bit is set in macro
 *      ICP_QAT_HW_AUTH_CONFIG_BUILD.
 *      Since the Padding Override is set to 0 regardless, for now, all the bits
 *      of the padding sequence specified in bits 48-51 and 56-63 of the 64-bit
 *      auth config word are set to 0 (reserved). Note that the bit positions of
 *      the padding sequence bits are defined relative to the 32-bit value that
 *      this macro returns.
 *
 ****************************************************************************************/
#define ICP_QAT_HW_AUTH_CONFIG_BUILD_UPPER                                                        \
	((((QAT_AUTH_SHA3_PROG_PADDING_POSTFIX_RESERVED)&QAT_AUTH_SHA3_PROG_PADDING_POSTFIX_MASK) \
	  << QAT_AUTH_SHA3_PROG_PADDING_POSTFIX_BITPOS) |                                         \
	 (((QAT_AUTH_SHA3_PROG_PADDING_PREFIX_RESERVED)&QAT_AUTH_SHA3_PROG_PADDING_PREFIX_MASK)   \
	  << QAT_AUTH_SHA3_PROG_PADDING_PREFIX_BITPOS))

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Auth Counter structure
 *
 * @description
 *      32 bit counter that tracks the number of data bytes passed through
 *      the slice. This is used by the padding logic for some algorithms. Note
 *      only the upper 32 bits are set.
 *
 *****************************************************************************/
typedef struct icp_qat_hw_auth_counter_s {
	uint32_t counter;
	/**< Counter value */
	uint32_t reserved;
	/**< Reserved */
} icp_qat_hw_auth_counter_t;

/* Private defines */
#define QAT_AUTH_COUNT_MASK 0xFFFFFFFF
/**< @ingroup icp_qat_hw_defs
 * Thirty two bit mask used for determining the Auth count */

#define QAT_AUTH_COUNT_BITPOS 0
/**< @ingroup icp_qat_hw_defs
 * Starting bit position indicating the Auth count.  */

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Macro to build the auth counter quad word
 *
 * @param val      Counter value to set
 *
 *****************************************************************************/
#define ICP_QAT_HW_AUTH_COUNT_BUILD(val)                                       \
	(((val)&QAT_AUTH_COUNT_MASK) << QAT_AUTH_COUNT_BITPOS)

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the common auth parameters
 * @description
 *      This part of the configuration is constant for each service
 *
 *****************************************************************************/
typedef struct icp_qat_hw_auth_setup_s {
	icp_qat_hw_auth_config_t auth_config;
	/**< Configuration word for the auth slice */
	icp_qat_hw_auth_counter_t auth_counter;
	/**< Auth counter value for this request */
} icp_qat_hw_auth_setup_t;

/* ************************************************************************* */
/* ************************************************************************* */

#define QAT_HW_DEFAULT_ALIGNMENT 8
#define QAT_HW_ROUND_UP(val, n) (((val) + ((n)-1)) & (~(n - 1)))

/* State1 */
#define ICP_QAT_HW_NULL_STATE1_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State1 block size for NULL hashing */
#define ICP_QAT_HW_MD5_STATE1_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State1 block size for MD5 */
#define ICP_QAT_HW_SHA1_STATE1_SZ 20
/**< @ingroup icp_qat_hw_defs
 * Define the state1 block size for SHA1 - Note that for the QAT HW the state
 * is rounded to the nearest 8 byte multiple */
#define ICP_QAT_HW_SHA224_STATE1_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA24 */
#define ICP_QAT_HW_SHA3_224_STATE1_SZ 28
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA3_224 */
#define ICP_QAT_HW_SHA256_STATE1_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA256 */
#define ICP_QAT_HW_SHA3_256_STATE1_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA3_256 */
#define ICP_QAT_HW_SHA384_STATE1_SZ 64
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA384 */
#define ICP_QAT_HW_SHA3_384_STATE1_SZ 48
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA3_384 */
#define ICP_QAT_HW_SHA512_STATE1_SZ 64
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA512 */
#define ICP_QAT_HW_SHA3_512_STATE1_SZ 64
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SHA3_512 */
#define ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State1 block size for XCBC */
#define ICP_QAT_HW_AES_CBC_MAC_STATE1_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State1 block size for CBC */
#define ICP_QAT_HW_AES_F9_STATE1_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State1 block size for AES F9 */
#define ICP_QAT_HW_KASUMI_F9_STATE1_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State1 block size for Kasumi F9 */
#define ICP_QAT_HW_GALOIS_128_STATE1_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State1 block size for Galois128 */
#define ICP_QAT_HW_SNOW_3G_UIA2_STATE1_SZ 8
/**< @ingroup icp_cpm_hw_defs
 * State1 block size for UIA2 */
#define ICP_QAT_HW_ZUC_3G_EIA3_STATE1_SZ 8
/**< @ingroup icp_cpm_hw_defs
 * State1 block size for EIA3 */
#define ICP_QAT_HW_SM3_STATE1_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State1 block size for SM3 */
#define ICP_QAT_HW_SHA3_STATEFUL_STATE1_SZ 200
/** <@ingroup icp_cpm_hw_defs
 * State1 block size for stateful SHA3 processing*/

/* State2 */
#define ICP_QAT_HW_NULL_STATE2_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State2 block size for NULL hashing */
#define ICP_QAT_HW_MD5_STATE2_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for MD5 */
#define ICP_QAT_HW_SHA1_STATE2_SZ 20
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA1 - Note that for the QAT HW the state  is rounded
 * to the nearest 8 byte multiple */
#define ICP_QAT_HW_SHA224_STATE2_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA224 */
#define ICP_QAT_HW_SHA3_224_STATE2_SZ 0
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA3_224 */
#define ICP_QAT_HW_SHA256_STATE2_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA256 */
#define ICP_QAT_HW_SHA3_256_STATE2_SZ 0
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA3_256 */
#define ICP_QAT_HW_SHA384_STATE2_SZ 64
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA384 */
#define ICP_QAT_HW_SHA3_384_STATE2_SZ 0
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA3_384 */
#define ICP_QAT_HW_SHA512_STATE2_SZ 64
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA512 */
#define ICP_QAT_HW_SHA3_512_STATE2_SZ 0
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SHA3_512 */
#define ICP_QAT_HW_AES_XCBC_MAC_KEY_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for XCBC */
#define ICP_QAT_HW_AES_CBC_MAC_KEY_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for CBC */
#define ICP_QAT_HW_AES_CCM_CBC_E_CTR0_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for AES Encrypted Counter 0 */
#define ICP_QAT_HW_F9_IK_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for F9 IK */
#define ICP_QAT_HW_F9_FK_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for F9 FK */
#define ICP_QAT_HW_KASUMI_F9_STATE2_SZ                                         \
	(ICP_QAT_HW_F9_IK_SZ + ICP_QAT_HW_F9_FK_SZ)
/**< @ingroup icp_qat_hw_defs
 * State2 complete size for Kasumi F9 */
#define ICP_QAT_HW_AES_F9_STATE2_SZ ICP_QAT_HW_KASUMI_F9_STATE2_SZ
/**< @ingroup icp_qat_hw_defs
 * State2 complete size for AES F9 */
#define ICP_QAT_HW_SNOW_3G_UIA2_STATE2_SZ 24
/**< @ingroup icp_cpm_hw_defs
 * State2 block size for UIA2 */
#define ICP_QAT_HW_ZUC_3G_EIA3_STATE2_SZ 32
/**< @ingroup icp_cpm_hw_defs
 * State2 block size for EIA3 */
#define ICP_QAT_HW_GALOIS_H_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for Galois Multiplier H */
#define ICP_QAT_HW_GALOIS_LEN_A_SZ 8
/**< @ingroup icp_qat_hw_defs
 * State2 block size for Galois AAD length */
#define ICP_QAT_HW_GALOIS_E_CTR0_SZ 16
/**< @ingroup icp_qat_hw_defs
 * State2 block size for Galois Encrypted Counter 0 */
#define ICP_QAT_HW_SM3_STATE2_SZ 32
/**< @ingroup icp_qat_hw_defs
 * State2 block size for SM3 */
#define ICP_QAT_HW_SHA3_STATEFUL_STATE2_SZ 208
/** <@ingroup icp_cpm_hw_defs
 * State2 block size for stateful SHA3 processing*/

/* ************************************************************************* */
/* ************************************************************************* */

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of SHA512 auth algorithm processing struct
 * @description
 *      This structs described the parameters to pass to the slice for
 *      configuring it for SHA512 processing. This is the largest possible
 *      setup block for authentication
 *
 *****************************************************************************/
typedef struct icp_qat_hw_auth_sha512_s {
	icp_qat_hw_auth_setup_t inner_setup;
	/**< Inner loop configuration word for the slice */

	uint8_t state1[ICP_QAT_HW_SHA512_STATE1_SZ];
	/**< Slice state1 variable */

	icp_qat_hw_auth_setup_t outer_setup;
	/**< Outer configuration word for the slice */

	uint8_t state2[ICP_QAT_HW_SHA512_STATE2_SZ];
	/**< Slice state2 variable */

} icp_qat_hw_auth_sha512_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of SHA3_512 auth algorithm processing struct
 * @description
 *      This structs described the parameters to pass to the slice for
 *      configuring it for SHA3_512 processing. This is the largest possible
 *      setup block for authentication
 *
 *****************************************************************************/
typedef struct icp_qat_hw_auth_sha3_512_s {
	icp_qat_hw_auth_setup_t inner_setup;
	/**< Inner loop configuration word for the slice */

	uint8_t state1[ICP_QAT_HW_SHA3_512_STATE1_SZ];
	/**< Slice state1 variable */

	icp_qat_hw_auth_setup_t outer_setup;
	/**< Outer configuration word for the slice */

} icp_qat_hw_auth_sha3_512_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of stateful SHA3 auth algorithm processing struct
 * @description
 *      This structs described the parameters to pass to the slice for
 *      configuring it for stateful SHA3 processing. This is the largest
 *      possible setup block for authentication
 *
 *****************************************************************************/
typedef struct icp_qat_hw_auth_sha3_stateful_s {
	icp_qat_hw_auth_setup_t inner_setup;
	/**< Inner loop configuration word for the slice */

	uint8_t inner_state1[ICP_QAT_HW_SHA3_STATEFUL_STATE1_SZ];
	/**< Inner hash block */

	icp_qat_hw_auth_setup_t outer_setup;
	/**< Outer configuration word for the slice */

	uint8_t outer_state1[ICP_QAT_HW_SHA3_STATEFUL_STATE1_SZ];
	/**< Outer hash block */

} icp_qat_hw_auth_sha3_stateful_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Supported hardware authentication algorithms
 * @description
 *      Common grouping of the auth algorithm types supported by the QAT
 *
 *****************************************************************************/
typedef union icp_qat_hw_auth_algo_blk_u {
	icp_qat_hw_auth_sha512_t sha512;
	/**< SHA512 Hashing */
	icp_qat_hw_auth_sha3_stateful_t sha3_stateful;
	/**< Stateful SHA3 Hashing */

} icp_qat_hw_auth_algo_blk_t;

#define ICP_QAT_HW_GALOIS_LEN_A_BITPOS 0
/**< @ingroup icp_qat_hw_defs
 * Bit position of the 32 bit A value in the 64 bit A configuration sent to
 * the QAT */

#define ICP_QAT_HW_GALOIS_LEN_A_MASK 0xFFFFFFFF
/**< @ingroup icp_qat_hw_defs
 * Mask value for A value */

/* ========================================================================= */
/*                                                CIPHER SLICE */
/* ========================================================================= */

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported Cipher Algorithm types
 * @description
 *      Enumeration used to define the cipher algorithms
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_CIPHER_ALGO_NULL = 0,	    /*!< Null ciphering */
	ICP_QAT_HW_CIPHER_ALGO_DES = 1,		    /*!< DES ciphering */
	ICP_QAT_HW_CIPHER_ALGO_3DES = 2,	    /*!< 3DES ciphering */
	ICP_QAT_HW_CIPHER_ALGO_AES128 = 3,	    /*!< AES-128 ciphering */
	ICP_QAT_HW_CIPHER_ALGO_AES192 = 4,	    /*!< AES-192 ciphering */
	ICP_QAT_HW_CIPHER_ALGO_AES256 = 5,	    /*!< AES-256 ciphering */
	ICP_QAT_HW_CIPHER_ALGO_ARC4 = 6,	    /*!< ARC4 ciphering */
	ICP_QAT_HW_CIPHER_ALGO_KASUMI = 7,	    /*!< Kasumi */
	ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2 = 8,    /*!< Snow_3G */
	ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3 = 9, /*!< ZUC_3G */
	ICP_QAT_HW_CIPHER_ALGO_SM4 = 10,	    /*!< SM4 ciphering */
	ICP_QAT_HW_CIPHER_ALGO_CHACHA20_POLY1305 =
	    11,				 /*!< CHACHA POLY SPC AEAD */
	ICP_QAT_HW_CIPHER_DELIMITER = 12 /**< Delimiter type */
} icp_qat_hw_cipher_algo_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported cipher modes of operation
 * @description
 *      Enumeration used to define the cipher slice modes.
 *
 * @Note
 *      Only some algorithms are valid in some of the modes. If you dont know
 *      what you are doing then refer back to the EAS
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_CIPHER_ECB_MODE = 0,      /*!< ECB mode */
	ICP_QAT_HW_CIPHER_CBC_MODE = 1,      /*!< CBC more */
	ICP_QAT_HW_CIPHER_CTR_MODE = 2,      /*!< CTR mode */
	ICP_QAT_HW_CIPHER_F8_MODE = 3,       /*!< F8 mode */
	ICP_QAT_HW_CIPHER_AEAD_MODE = 4,     /*!< AES-GCM SPC AEAD mode */
	ICP_QAT_HW_CIPHER_CCM_MODE = 5,      /*!< AES-CCM SPC AEAD mode */
	ICP_QAT_HW_CIPHER_XTS_MODE = 6,      /*!< XTS mode */
	ICP_QAT_HW_CIPHER_MODE_DELIMITER = 7 /**< Delimiter type */
} icp_qat_hw_cipher_mode_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Cipher Configuration Struct
 *
 * @description
 *      Configuration data used for setting up the QAT Cipher Slice
 *
 *****************************************************************************/

typedef struct icp_qat_hw_cipher_config_s {
	uint32_t val;
	/**< Cipher slice configuration */

	uint32_t reserved;
	/**< Reserved */
} icp_qat_hw_cipher_config_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Cipher Configuration Struct
 *
 * @description
 *      Configuration data used for setting up the QAT UCS Cipher Slice
 *
 *****************************************************************************/
typedef struct icp_qat_hw_ucs_cipher_config_s {
	uint32_t val;
	/**< Cipher slice configuration */

	uint32_t reserved[3];
	/**< Reserved */
} icp_qat_hw_ucs_cipher_config_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the cipher direction
 * @description
 *      Enumeration which is used to define the cipher direction to apply
 *
 *****************************************************************************/

typedef enum {
	/*!< Flag to indicate that encryption is required */
	ICP_QAT_HW_CIPHER_ENCRYPT = 0,
	/*!< Flag to indicate that decryption is required */
	ICP_QAT_HW_CIPHER_DECRYPT = 1,

} icp_qat_hw_cipher_dir_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the cipher key conversion modes
 * @description
 *      Enumeration which is used to define if cipher key conversion is needed
 *
 *****************************************************************************/

typedef enum {
	/*!< Flag to indicate that no key convert is required */
	ICP_QAT_HW_CIPHER_NO_CONVERT = 0,
	/*!< Flag to indicate that key conversion is required */
	ICP_QAT_HW_CIPHER_KEY_CONVERT = 1,
} icp_qat_hw_cipher_convert_t;

/* Private defines */

/* Note: Bit positions have been arranged for little endian ordering */

#define QAT_CIPHER_MODE_BITPOS 4
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher mode bit position */

#define QAT_CIPHER_MODE_MASK 0xF
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher mode mask (four bits) */

#define QAT_CIPHER_ALGO_BITPOS 0
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher algo bit position */

#define QAT_CIPHER_ALGO_MASK 0xF
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher algo mask (four bits) */

#define QAT_CIPHER_CONVERT_BITPOS 9
/**< @ingroup icp_qat_hw_defs
 * Define the cipher convert key bit position */

#define QAT_CIPHER_CONVERT_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher convert key mask (one bit)*/

#define QAT_CIPHER_DIR_BITPOS 8
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher direction bit position */

#define QAT_CIPHER_DIR_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher direction mask (one bit) */

#define QAT_CIPHER_AEAD_HASH_CMP_LEN_MASK 0x1F
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher AEAD Hash compare length  mask (5 bits)*/

#define QAT_CIPHER_AEAD_HASH_CMP_LEN_BITPOS 10
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher AEAD Hash compare length  (5 bits)*/

#define QAT_CIPHER_AEAD_AAD_SIZE_LOWER_MASK 0xFF
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher AEAD AAD size lower byte mask */

#define QAT_CIPHER_AEAD_AAD_SIZE_UPPER_MASK 0x3F
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher AEAD AAD size upper 6 bits mask */

#define QAT_CIPHER_AEAD_AAD_UPPER_SHIFT 8
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher AEAD AAD size Upper byte shift */

#define QAT_CIPHER_AEAD_AAD_LOWER_SHIFT 24
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher AEAD AAD size Lower byte shift */

#define QAT_CIPHER_AEAD_AAD_SIZE_BITPOS 16
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher AEAD AAD size  (14 bits)*/

#define QAT_CIPHER_MODE_F8_KEY_SZ_MULT 2
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher mode F8 key size */

#define QAT_CIPHER_MODE_XTS_KEY_SZ_MULT 2
/**< @ingroup icp_qat_hw_defs
 * Define for the cipher XTS mode key size */

#define QAT_CIPHER_MODE_UCS_XTS_KEY_SZ_MULT 1
/**< @ingroup icp_qat_hw_defs
 * Define for the UCS cipher XTS mode key size */

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Build the cipher configuration field
 *
 * @param mode      Cipher Mode to use
 * @param algo      Cipher Algorithm to use
 * @param convert   Specify if the key is to be converted
 * @param dir       Specify the cipher direction either encrypt or decrypt
 *
 *****************************************************************************/
#define ICP_QAT_HW_CIPHER_CONFIG_BUILD(                                        \
    mode, algo, convert, dir, aead_hash_cmp_len)                               \
	((((mode)&QAT_CIPHER_MODE_MASK) << QAT_CIPHER_MODE_BITPOS) |           \
	 (((algo)&QAT_CIPHER_ALGO_MASK) << QAT_CIPHER_ALGO_BITPOS) |           \
	 (((convert)&QAT_CIPHER_CONVERT_MASK) << QAT_CIPHER_CONVERT_BITPOS) |  \
	 (((dir)&QAT_CIPHER_DIR_MASK) << QAT_CIPHER_DIR_BITPOS) |              \
	 (((aead_hash_cmp_len)&QAT_CIPHER_AEAD_HASH_CMP_LEN_MASK)              \
	  << QAT_CIPHER_AEAD_HASH_CMP_LEN_BITPOS))

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Build the second QW of cipher slice config
 *
 * @param aad_size  Specify the size of associated authentication data
 *                  for AEAD processing
 *
 ******************************************************************************/
#define ICP_QAT_HW_CIPHER_CONFIG_BUILD_UPPER(aad_size)                         \
	(((((aad_size) >> QAT_CIPHER_AEAD_AAD_UPPER_SHIFT) &                   \
	   QAT_CIPHER_AEAD_AAD_SIZE_UPPER_MASK)                                \
	  << QAT_CIPHER_AEAD_AAD_SIZE_BITPOS) |                                \
	 (((aad_size)&QAT_CIPHER_AEAD_AAD_SIZE_LOWER_MASK)                     \
	  << QAT_CIPHER_AEAD_AAD_LOWER_SHIFT))

#define ICP_QAT_HW_DES_BLK_SZ 8
/**< @ingroup icp_qat_hw_defs
 * Define the block size for DES.
 * This used as either the size of the IV or CTR input value */
#define ICP_QAT_HW_3DES_BLK_SZ 8
/**< @ingroup icp_qat_hw_defs
 * Define the processing block size for 3DES */
#define ICP_QAT_HW_NULL_BLK_SZ 8
/**< @ingroup icp_qat_hw_defs
 * Define the processing block size for NULL */
#define ICP_QAT_HW_AES_BLK_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the processing block size for AES 128, 192 and 256 */
#define ICP_QAT_HW_KASUMI_BLK_SZ 8
/**< @ingroup icp_qat_hw_defs
 * Define the processing block size for KASUMI */
#define ICP_QAT_HW_SNOW_3G_BLK_SZ 8
/**< @ingroup icp_qat_hw_defs
 * Define the processing block size for SNOW_3G */
#define ICP_QAT_HW_ZUC_3G_BLK_SZ 8
/**< @ingroup icp_qat_hw_defs
 * Define the processing block size for ZUC_3G */
#define ICP_QAT_HW_NULL_KEY_SZ 256
/**< @ingroup icp_qat_hw_defs
 * Define the key size for NULL */
#define ICP_QAT_HW_DES_KEY_SZ 8
/**< @ingroup icp_qat_hw_defs
 * Define the key size for DES */
#define ICP_QAT_HW_3DES_KEY_SZ 24
/**< @ingroup icp_qat_hw_defs
 * Define the key size for 3DES */
#define ICP_QAT_HW_AES_128_KEY_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES128 */
#define ICP_QAT_HW_AES_192_KEY_SZ 24
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES192 */
#define ICP_QAT_HW_AES_256_KEY_SZ 32
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES256 */
/* AES UCS */
#define ICP_QAT_HW_UCS_AES_128_KEY_SZ ICP_QAT_HW_AES_128_KEY_SZ
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES128 for UCS slice*/
#define ICP_QAT_HW_UCS_AES_192_KEY_SZ 32
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES192 for UCS slice*/
#define ICP_QAT_HW_UCS_AES_256_KEY_SZ ICP_QAT_HW_AES_256_KEY_SZ
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES256 for UCS slice*/
#define ICP_QAT_HW_AES_128_F8_KEY_SZ                                           \
	(ICP_QAT_HW_AES_128_KEY_SZ * QAT_CIPHER_MODE_F8_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES128 F8 */
#define ICP_QAT_HW_AES_192_F8_KEY_SZ                                           \
	(ICP_QAT_HW_AES_192_KEY_SZ * QAT_CIPHER_MODE_F8_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES192 F8 */
#define ICP_QAT_HW_AES_256_F8_KEY_SZ                                           \
	(ICP_QAT_HW_AES_256_KEY_SZ * QAT_CIPHER_MODE_F8_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES256 F8 */
#define ICP_QAT_HW_AES_128_XTS_KEY_SZ                                          \
	(ICP_QAT_HW_AES_128_KEY_SZ * QAT_CIPHER_MODE_XTS_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES128 XTS */
#define ICP_QAT_HW_AES_256_XTS_KEY_SZ                                          \
	(ICP_QAT_HW_AES_256_KEY_SZ * QAT_CIPHER_MODE_XTS_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES256 XTS */
#define ICP_QAT_HW_UCS_AES_128_XTS_KEY_SZ                                      \
	(ICP_QAT_HW_UCS_AES_128_KEY_SZ * QAT_CIPHER_MODE_UCS_XTS_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES128 XTS for the UCS Slice*/
#define ICP_QAT_HW_UCS_AES_256_XTS_KEY_SZ                                      \
	(ICP_QAT_HW_UCS_AES_256_KEY_SZ * QAT_CIPHER_MODE_UCS_XTS_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES256 XTS for the UCS Slice*/
#define ICP_QAT_HW_KASUMI_KEY_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the key size for Kasumi */
#define ICP_QAT_HW_KASUMI_F8_KEY_SZ                                            \
	(ICP_QAT_HW_KASUMI_KEY_SZ * QAT_CIPHER_MODE_F8_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for Kasumi F8 */
#define ICP_QAT_HW_AES_128_XTS_KEY_SZ                                          \
	(ICP_QAT_HW_AES_128_KEY_SZ * QAT_CIPHER_MODE_XTS_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES128 XTS */
#define ICP_QAT_HW_AES_256_XTS_KEY_SZ                                          \
	(ICP_QAT_HW_AES_256_KEY_SZ * QAT_CIPHER_MODE_XTS_KEY_SZ_MULT)
/**< @ingroup icp_qat_hw_defs
 * Define the key size for AES256 XTS */
#define ICP_QAT_HW_ARC4_KEY_SZ 256
/**< @ingroup icp_qat_hw_defs
 * Define the key size for ARC4 */
#define ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ 16
/**< @ingroup icp_cpm_hw_defs
 * Define the key size for SNOW_3G_UEA2 */
#define ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ 16
/**< @ingroup icp_cpm_hw_defs
 * Define the iv size for SNOW_3G_UEA2 */
#define ICP_QAT_HW_ZUC_3G_EEA3_KEY_SZ 16
/**< @ingroup icp_cpm_hw_defs
 * Define the key size for ZUC_3G_EEA3 */
#define ICP_QAT_HW_ZUC_3G_EEA3_IV_SZ 16
/**< @ingroup icp_cpm_hw_defs
 * Define the iv size for ZUC_3G_EEA3 */
#define ICP_QAT_HW_MODE_F8_NUM_REG_TO_CLEAR 2
/**< @ingroup icp_cpm_hw_defs
 * Number of the HW register to clear in F8 mode */
/**< @ingroup icp_qat_hw_defs
 * Define the State/ Initialization Vector size for CHACHAPOLY */
#define ICP_QAT_HW_CHACHAPOLY_KEY_SZ 32
/**< @ingroup icp_qat_hw_defs
 * Define the key size for CHACHA20-Poly1305*/
#define ICP_QAT_HW_CHACHAPOLY_IV_SZ 12
/**< @ingroup icp_qat_hw_defs
 * Define the block size for CHACHA20-Poly1305*/
#define ICP_QAT_HW_CHACHAPOLY_BLK_SZ 64
/**< @ingroup icp_qat_hw_defs
 * Define the State/ Initialization Vector size for CHACHA20-Poly1305 */
#define ICP_QAT_HW_CHACHAPOLY_CTR_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the key size for CHACHA20-Poly1305*/
#define ICP_QAT_HW_SPC_CTR_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the Single Pass tag size*/
#define ICP_QAT_HW_CHACHAPOLY_ICV__SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the key size for CHACHA20-Poly1305*/
#define ICP_QAT_HW_CHACHAPOLY_AAD_MAX_LOG 14
/**< @ingroup icp_qat_hw_defs
 * Define the key size for CHACHA20-Poly1305*/
#define ICP_QAT_HW_SM4_BLK_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the processing block size for SM4 */
#define ICP_QAT_HW_SM4_KEY_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Number of the HW register to clear in F8 mode */
#define ICP_QAT_HW_SM4_IV_SZ 16
/**< @ingroup icp_qat_hw_defs
 * Define the key size for SM4 */

/*
 * SHRAM constants definitions
 */
#define INIT_SHRAM_CONSTANTS_TABLE_SZ (1024)
#define SHRAM_CONSTANTS_TABLE_SIZE_QWS (INIT_SHRAM_CONSTANTS_TABLE_SZ / 4 / 2)

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of AES-256 F8 cipher algorithm processing struct
 * @description
 *      This structs described the parameters to pass to the slice for
 *      configuring it for AES-256 F8 processing
 *
 *****************************************************************************/
typedef struct icp_qat_hw_cipher_aes256_f8_s {
	icp_qat_hw_cipher_config_t cipher_config;
	/**< Cipher configuration word for the slice set to
	 * AES-256 and the F8 mode */

	uint8_t key[ICP_QAT_HW_AES_256_F8_KEY_SZ];
	/**< Cipher key */

} icp_qat_hw_cipher_aes256_f8_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Supported hardware cipher algorithms
 * @description
 *      Common grouping of the cipher algorithm types supported by the QAT.
 *      This is the largest possible cipher setup block size
 *
 *****************************************************************************/
typedef union icp_qat_hw_cipher_algo_blk_u {

	icp_qat_hw_cipher_aes256_f8_t aes256_f8;
	/**< AES-256 F8 Cipher */

} icp_qat_hw_cipher_algo_blk_t;

/* ========================================================================= */
/*                                                  TRNG SLICE */
/* ========================================================================= */

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported TRNG configuration modes
 * @description
 *      Enumeration used to define the TRNG modes. Used by clients when
 *      configuring the TRNG for use
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_TRNG_DBL = 0,      /*!< TRNG Disabled mode */
	ICP_QAT_HW_TRNG_NHT = 1,      /*!< TRNG Normal Health Test mode */
	ICP_QAT_HW_TRNG_KAT = 4,      /*!< TRNG Known Answer Test mode */
	ICP_QAT_HW_TRNG_DELIMITER = 8 /**< Delimiter type */
} icp_qat_hw_trng_cfg_mode_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported TRNG KAT (known answer test) modes
 * @description
 *      Enumeration which is used to define the TRNG KAT modes. Used by clients
 *      when configuring the TRNG for testing
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_TRNG_NEG_0 = 0,	  /*!< TRNG Neg Zero Test */
	ICP_QAT_HW_TRNG_NEG_1 = 1,	  /*!< TRNG Neg One Test */
	ICP_QAT_HW_TRNG_POS = 2,	  /*!< TRNG POS Test */
	ICP_QAT_HW_TRNG_POS_VNC = 3,	  /*!< TRNG POS VNC Test */
	ICP_QAT_HW_TRNG_KAT_DELIMITER = 4 /**< Delimiter type */
} icp_qat_hw_trng_kat_mode_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      TRNG mode configuration structure.
 *
 * @description
 *      Definition of the format of the TRNG slice configuration. Used
 *      internally by the QAT FW for configuration of the KAT unit or the
 *      TRNG depending on the slice command i.e. either a set_slice_config or
 *      slice_wr_KAT_type
 *
 *****************************************************************************/

typedef struct icp_qat_hw_trng_config_s {
	uint32_t val;
	/**< Configuration used for setting up the TRNG slice */

	uint32_t reserved;
	/**< Reserved */
} icp_qat_hw_trng_config_t;

/* Private Defines */

/* Note: Bit positions have been arranged for little endian ordering */

#define QAT_TRNG_CONFIG_MODE_MASK 0x7
/**< @ingroup icp_qat_hw_defs
 * Mask for the TRNG configuration mode. (Three bits) */

#define QAT_TRNG_CONFIG_MODE_BITPOS 5
/**< @ingroup icp_qat_hw_defs
 * TRNG configuration mode bit positions start */

#define QAT_TRNG_KAT_MODE_MASK 0x3
/**< @ingroup icp_qat_hw_defs
 * Mask of two bits for the TRNG known answer test mode */

#define QAT_TRNG_KAT_MODE_BITPOS 6
/**< @ingroup icp_qat_hw_defs
 * TRNG known answer test mode bit positions start */

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Build the configuration byte for the TRNG slice based on the mode
 *
 * @param mode   Configuration mode parameter
 *
 *****************************************************************************/
#define ICP_QAT_HW_TRNG_CONFIG_MODE_BUILD(mode)                                \
	(((mode)&QAT_TRNG_CONFIG_MODE_MASK) << QAT_TRNG_CONFIG_MODE_BITPOS)

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Build the configuration byte for the TRNG KAT based on the mode
 *
 * @param mode   Configuration mode parameter
 *
 *****************************************************************************/
#define ICP_QAT_HW_TRNG_KAT_MODE_BUILD(mode)                                   \
	((((mode)&QAT_TRNG_KAT_MODE_MASK) << QAT_TRNG_KAT_MODE_BITPOS))

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      TRNG test status structure.
 *
 * @description
 *      Definition of the format of the TRNG slice test status structure. Used
 *      internally by the QAT FW.
 *
 *****************************************************************************/

typedef struct icp_qat_hw_trng_test_status_s {

	uint32_t status;
	/**< Status used for setting up the TRNG slice */

	uint32_t fail_count;
	/**< Comparator fail count */
} icp_qat_hw_trng_test_status_t;

#define ICP_QAT_HW_TRNG_TEST_NO_FAILURES 1
/**< @ingroup icp_qat_hw_defs
 * Flag to indicate that there were no Test Failures */

#define ICP_QAT_HW_TRNG_TEST_FAILURES_FOUND 0
/**< @ingroup icp_qat_hw_defs
 * Flag to indicate that there were Test Failures */

#define ICP_QAT_HW_TRNG_TEST_STATUS_VALID 1
/**< @ingroup icp_qat_hw_defs
 * Flag to indicate that there is no valid Test output */

#define ICP_QAT_HW_TRNG_TEST_STATUS_INVALID 0
/**< @ingroup icp_qat_hw_defs
 * Flag to indicate that the Test output is still invalid */

/* Private defines */
#define QAT_TRNG_TEST_FAILURE_FLAG_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Mask of one bit used to determine the TRNG Test pass/fail */

#define QAT_TRNG_TEST_FAILURE_FLAG_BITPOS 4
/**< @ingroup icp_qat_hw_defs
 * Flag position to indicate that the TRNG Test status is pass of fail */

#define QAT_TRNG_TEST_STATUS_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Mask of one bit used to determine the TRNG Test status */

#define QAT_TRNG_TEST_STATUS_BITPOS 1
/**< @ingroup icp_qat_hw_defs
 * Flag position to indicate the TRNG Test status */

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Extract the fail bit for the TRNG slice
 *
 * @param status   TRNG status value
 *
 *****************************************************************************/

#define ICP_QAT_HW_TRNG_FAIL_FLAG_GET(status)                                  \
	(((status) >> QAT_TRNG_TEST_FAILURE_FLAG_BITPOS) &                     \
	 QAT_TRNG_TEST_FAILURE_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Extract the status valid bit for the TRNG slice
 *
 * @param status   TRNG status value
 *
 *****************************************************************************/
#define ICP_QAT_HW_TRNG_STATUS_VALID_GET(status)                               \
	(((status) >> QAT_TRNG_TEST_STATUS_BITPOS) & QAT_TRNG_TEST_STATUS_MASK)

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      TRNG entropy counters
 *
 * @description
 *      Definition of the format of the TRNG entropy counters. Used internally
 *      by the QAT FW.
 *
 *****************************************************************************/

typedef struct icp_qat_hw_trng_entropy_counts_s {
	uint64_t raw_ones_count;
	/**< Count of raw ones of entropy */

	uint64_t raw_zeros_count;
	/**< Count of raw zeros of entropy */

	uint64_t cond_ones_count;
	/**< Count of conditioned ones entropy */

	uint64_t cond_zeros_count;
	/**< Count of conditioned zeros entropy */
} icp_qat_hw_trng_entropy_counts_t;

/* Private defines */
#define QAT_HW_TRNG_ENTROPY_STS_RSVD_SZ 4
/**< @ingroup icp_qat_hw_defs
 * TRNG entropy status reserved size in bytes */

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      TRNG entropy available status.
 *
 * @description
 *      Definition of the format of the TRNG slice entropy status available.
 *      struct. Used internally by the QAT FW.
 *
 *****************************************************************************/
typedef struct icp_qat_hw_trng_entropy_status_s {
	uint32_t status;
	/**< Entropy status in the TRNG */

	uint8_t reserved[QAT_HW_TRNG_ENTROPY_STS_RSVD_SZ];
	/**< Reserved */
} icp_qat_hw_trng_entropy_status_t;

#define ICP_QAT_HW_TRNG_ENTROPY_AVAIL 1
/**< @ingroup icp_qat_hw_defs
 * Flag indicating that entropy data is available in the QAT TRNG slice */

#define ICP_QAT_HW_TRNG_ENTROPY_NOT_AVAIL 0
/**< @ingroup icp_qat_hw_defs
 * Flag indicating that no entropy data is available in the QAT TRNG slice */

/* Private defines */
#define QAT_TRNG_ENTROPY_STATUS_MASK 1
/**< @ingroup icp_qat_hw_defs
 * Mask of one bit used to determine the TRNG Entropy status */

#define QAT_TRNG_ENTROPY_STATUS_BITPOS 0
/**< @ingroup icp_qat_hw_defs
 * Starting bit position for TRNG Entropy status. */

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Extract the entropy available status bit
 *
 * @param status   TRNG status value
 *
 *****************************************************************************/
#define ICP_QAT_HW_TRNG_ENTROPY_STATUS_GET(status)                             \
	(((status) >> QAT_TRNG_ENTROPY_STATUS_BITPOS) &                        \
	 QAT_TRNG_ENTROPY_STATUS_MASK)

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Entropy seed data
 *
 * @description
 *      This type is used for the definition of the entropy generated by a read
 *      of the TRNG slice
 *
 *****************************************************************************/
typedef uint64_t icp_qat_hw_trng_entropy;

/* ========================================================================= */
/*                                            COMPRESSION SLICE */
/* ========================================================================= */

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported compression directions
 * @description
 *      Enumeration used to define the compression directions
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_COMPRESSION_DIR_COMPRESS = 0,   /*!< Compression */
	ICP_QAT_HW_COMPRESSION_DIR_DECOMPRESS = 1, /*!< Decompression */
	ICP_QAT_HW_COMPRESSION_DIR_DELIMITER = 2   /**< Delimiter type */
} icp_qat_hw_compression_direction_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported delayed match modes
 * @description
 *      Enumeration used to define whether delayed match is enabled
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_DISABLED = 0,
	/*!< Delayed match disabled */

	ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_ENABLED = 1,
	/*!< Delayed match enabled
	     Note: This is the only valid mode - refer to CPM1.6 SAS */

	ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_DELIMITER = 2
	/**< Delimiter type */

} icp_qat_hw_compression_delayed_match_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported compression algorithms
 * @description
 *      Enumeration used to define the compression algorithms
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_COMPRESSION_ALGO_DEFLATE = 0,  /*!< Deflate compression */
	ICP_QAT_HW_COMPRESSION_DEPRECATED = 1,	  /*!< Deprecated */
	ICP_QAT_HW_COMPRESSION_ALGO_DELIMITER = 2 /**< Delimiter type */
} icp_qat_hw_compression_algo_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported compression depths
 * @description
 *      Enumeration used to define the compression slice depths.
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_COMPRESSION_DEPTH_1 = 0,
	/*!< Search depth 1 (Fastest least exhaustive) */

	ICP_QAT_HW_COMPRESSION_DEPTH_4 = 1,
	/*!< Search depth 4 */

	ICP_QAT_HW_COMPRESSION_DEPTH_8 = 2,
	/*!< Search depth 8 */

	ICP_QAT_HW_COMPRESSION_DEPTH_16 = 3,
	/*!< Search depth 16 */

	ICP_QAT_HW_COMPRESSION_DEPTH_128 = 4,
	/*!< Search depth 128 (Slowest, most exhaustive) */

	ICP_QAT_HW_COMPRESSION_DEPTH_DELIMITER = 5
	/**< Delimiter type */

} icp_qat_hw_compression_depth_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Definition of the supported file types
 * @description
 *      Enumeration used to define the compression file types.
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_HW_COMPRESSION_FILE_TYPE_0 = 0,
	/*!< Use Static Trees */

	ICP_QAT_HW_COMPRESSION_FILE_TYPE_1 = 1,
	/*!< Use Semi-Dynamic Trees at offset 0 */

	ICP_QAT_HW_COMPRESSION_FILE_TYPE_2 = 2,
	/*!< Use Semi-Dynamic Trees at offset 320 */

	ICP_QAT_HW_COMPRESSION_FILE_TYPE_3 = 3,
	/*!< Use Semi-Dynamic Trees at offset 640 */

	ICP_QAT_HW_COMPRESSION_FILE_TYPE_4 = 4,
	/*!< Use Semi-Dynamic Trees at offset 960 */

	ICP_QAT_HW_COMPRESSION_FILE_TYPE_DELIMITER = 5
	/**< Delimiter type */

} icp_qat_hw_compression_file_type_t;

typedef enum {
	BNP_SKIP_MODE_DISABLED = 0,
	BNP_SKIP_MODE_AT_START = 1,
	BNP_SKIP_MODE_AT_END = 2,
	BNP_SKIP_MODE_STRIDE = 3
} icp_qat_bnp_skip_mode_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_hw_defs
 *      Compression Configuration Struct
 *
 * @description
 *      Configuration data used for setting up the QAT Compression Slice
 *
 *****************************************************************************/

typedef struct icp_qat_hw_compression_config_s {
	uint32_t lower_val;
	/**< Compression slice configuration lower LW */

	uint32_t upper_val;
	/**< Compression slice configuration upper LW */
} icp_qat_hw_compression_config_t;

/* Private defines */
#define QAT_COMPRESSION_DIR_BITPOS 4
/**< @ingroup icp_qat_hw_defs
 * Define for the compression direction bit position */

#define QAT_COMPRESSION_DIR_MASK 0x7
/**< @ingroup icp_qat_hw_defs
 * Define for the compression direction mask (three bits) */

#define QAT_COMPRESSION_DELAYED_MATCH_BITPOS 16
/**< @ingroup icp_qat_hw_defs
 * Define for the compression delayed match bit position */

#define QAT_COMPRESSION_DELAYED_MATCH_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Define for the delayed match mask (one bit) */

#define QAT_COMPRESSION_ALGO_BITPOS 31
/**< @ingroup icp_qat_hw_defs
 * Define for the compression algorithm bit position */

#define QAT_COMPRESSION_ALGO_MASK 0x1
/**< @ingroup icp_qat_hw_defs
 * Define for the compression algorithm mask (one bit) */

#define QAT_COMPRESSION_DEPTH_BITPOS 28
/**< @ingroup icp_qat_hw_defs
 * Define for the compression depth bit position */

#define QAT_COMPRESSION_DEPTH_MASK 0x7
/**< @ingroup icp_qat_hw_defs
 * Define for the compression depth mask (three bits) */

#define QAT_COMPRESSION_FILE_TYPE_BITPOS 24
/**< @ingroup icp_qat_hw_defs
 * Define for the compression file type bit position */

#define QAT_COMPRESSION_FILE_TYPE_MASK 0xF
/**< @ingroup icp_qat_hw_defs
 * Define for the compression file type mask (four bits) */

/**
 ******************************************************************************
 * @ingroup icp_qat_hw_defs
 *
 * @description
 *      Build the compression slice configuration field
 *
 * @param dir      Compression Direction to use, compress or decompress
 * @param delayed  Specify if delayed match should be enabled
 * @param algo     Compression algorithm to use
 * @param depth    Compression search depth to use
 * @param filetype Compression file type to use, static or semi dynamic trees
 *
 *****************************************************************************/
#define ICP_QAT_HW_COMPRESSION_CONFIG_BUILD(                                   \
    dir, delayed, algo, depth, filetype)                                       \
	((((dir)&QAT_COMPRESSION_DIR_MASK) << QAT_COMPRESSION_DIR_BITPOS) |    \
	 (((delayed)&QAT_COMPRESSION_DELAYED_MATCH_MASK)                       \
	  << QAT_COMPRESSION_DELAYED_MATCH_BITPOS) |                           \
	 (((algo)&QAT_COMPRESSION_ALGO_MASK) << QAT_COMPRESSION_ALGO_BITPOS) | \
	 (((depth)&QAT_COMPRESSION_DEPTH_MASK)                                 \
	  << QAT_COMPRESSION_DEPTH_BITPOS) |                                   \
	 (((filetype)&QAT_COMPRESSION_FILE_TYPE_MASK)                          \
	  << QAT_COMPRESSION_FILE_TYPE_BITPOS))

/* ========================================================================= */
/*                                            TRANSLATOR SLICE */
/* ========================================================================= */

/**< Translator slice configuration is set internally by the firmware */

#endif /* _ICP_QAT_HW_H_ */
