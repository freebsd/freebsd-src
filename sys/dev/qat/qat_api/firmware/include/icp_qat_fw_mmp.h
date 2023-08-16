/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/* --- (Automatically generated (build v. 2.7), do not modify manually) --- */


/**
 * @file icp_qat_fw_mmp.h
 * @defgroup icp_qat_fw_mmp ICP QAT FW MMP Processing Definitions
 * @ingroup icp_qat_fw
 * $Revision: 0.1 $
 * @brief
 *      This file documents the external interfaces that the QAT FW running
 *      on the QAT Acceleration Engine provides to clients wanting to
 *      accelerate crypto assymetric applications
 */


#ifndef __ICP_QAT_FW_MMP__
#define __ICP_QAT_FW_MMP__


/**************************************************************************
 * Include local header files
 **************************************************************************
 */


#include "icp_qat_fw.h"


/**************************************************************************
 * Local constants
 **************************************************************************
 */
#define ICP_QAT_FW_PKE_INPUT_COUNT_MAX      7
/**< @ingroup icp_qat_fw_pke
 * Maximum number of input paramaters in all PKE request */
#define ICP_QAT_FW_PKE_OUTPUT_COUNT_MAX     5
/**< @ingroup icp_qat_fw_pke
 * Maximum number of output paramaters in all PKE request */

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P384 Variable Point Multiplication [k]P ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_EC_POINT_MULTIPLICATION_P384.
 */
typedef struct icp_qat_fw_mmp_ec_point_multiplication_p384_input_s
{
    uint64_t xp; /**< xP = affine coordinate X of point P  (6 qwords)*/
    uint64_t yp; /**< yP = affine coordinate Y of point P  (6 qwords)*/
    uint64_t k;  /**< k  = scalar  (6 qwords)*/
} icp_qat_fw_mmp_ec_point_multiplication_p384_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P384 Generator Point Multiplication [k]G ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_EC_GENERATOR_MULTIPLICATION_P384.
 */
typedef struct icp_qat_fw_mmp_ec_generator_multiplication_p384_input_s
{
    uint64_t k; /**< k  = scalar  (6 qwords)*/
} icp_qat_fw_mmp_ec_generator_multiplication_p384_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P384 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_ECDSA_SIGN_RS_P384.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_p384_input_s
{
    uint64_t k; /**< k  = random value, &gt; 0 and &lt; n (order of G for P384)
                   (6 qwords)*/
    uint64_t e; /**<  (6 qwords)*/
    uint64_t d; /**<  (6 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_p384_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P256 Variable Point Multiplication [k]P ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_EC_POINT_MULTIPLICATION_P256.
 */
typedef struct icp_qat_fw_mmp_ec_point_multiplication_p256_input_s
{
    uint64_t xp; /**< xP = affine coordinate X of point P  (4 qwords)*/
    uint64_t yp; /**< yP = affine coordinate Y of point P  (4 qwords)*/
    uint64_t k;  /**< k  = scalar  (4 qwords)*/
} icp_qat_fw_mmp_ec_point_multiplication_p256_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P256 Generator Point Multiplication [k]G ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_EC_GENERATOR_MULTIPLICATION_P256.
 */
typedef struct icp_qat_fw_mmp_ec_generator_multiplication_p256_input_s
{
    uint64_t k; /**< k  = scalar  (4 qwords)*/
} icp_qat_fw_mmp_ec_generator_multiplication_p256_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P256 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_ECDSA_SIGN_RS_P256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_p256_input_s
{
    uint64_t k; /**< k  = random value, &gt; 0 and &lt; n (order of G for P256)
                   (4 qwords)*/
    uint64_t e; /**<  (4 qwords)*/
    uint64_t d; /**<  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_p256_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC SM2 point multiply [k]G ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_ECSM2_GENERATOR_MULTIPLICATION.
 */
typedef struct icp_qat_fw_mmp_ecsm2_generator_multiplication_input_s
{
    uint64_t k; /**< k  = multiplicand  (4 qwords)*/
} icp_qat_fw_mmp_ecsm2_generator_multiplication_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Initialisation sequence ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_INIT.
 */
typedef struct icp_qat_fw_mmp_init_input_s
{
    uint64_t z; /**< zeroed quadword (1 qwords)*/
} icp_qat_fw_mmp_init_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation base 2 for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_G2_768.
 */
typedef struct icp_qat_fw_mmp_dh_g2_768_input_s
{
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^768 (12 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^767 and &lt; 2^768 (12 qwords)*/
} icp_qat_fw_mmp_dh_g2_768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_768.
 */
typedef struct icp_qat_fw_mmp_dh_768_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^768 (12 qwords)*/
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^768 (12 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^767 and &lt; 2^768 (12 qwords)*/
} icp_qat_fw_mmp_dh_768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation base 2 for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_G2_1024.
 */
typedef struct icp_qat_fw_mmp_dh_g2_1024_input_s
{
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^1024 (16 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^1023 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_dh_g2_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_1024.
 */
typedef struct icp_qat_fw_mmp_dh_1024_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^1024 (16 qwords)*/
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^1024 (16 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^1023 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_dh_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation base 2 for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_G2_1536.
 */
typedef struct icp_qat_fw_mmp_dh_g2_1536_input_s
{
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^1536 (24 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^1535 and &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_mmp_dh_g2_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_1536.
 */
typedef struct icp_qat_fw_mmp_dh_1536_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^1536 (24 qwords)*/
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^1536 (24 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^1535 and &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_mmp_dh_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation base 2 for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_G2_2048.
 */
typedef struct icp_qat_fw_mmp_dh_g2_2048_input_s
{
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^2048 (32 qwords)*/
    uint64_t m; /**< modulus  &ge; 2^2047 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_dh_g2_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_2048.
 */
typedef struct icp_qat_fw_mmp_dh_2048_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^2048 (32 qwords)*/
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^2048 (32 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^2047 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_dh_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation base 2 for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_G2_3072.
 */
typedef struct icp_qat_fw_mmp_dh_g2_3072_input_s
{
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^3072 (48 qwords)*/
    uint64_t m; /**< modulus  &ge; 2^3071 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_dh_g2_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_3072.
 */
typedef struct icp_qat_fw_mmp_dh_3072_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^3072 (48 qwords)*/
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^3072 (48 qwords)*/
    uint64_t m; /**< modulus  &ge; 2^3071 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_dh_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation base 2 for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_G2_4096.
 */
typedef struct icp_qat_fw_mmp_dh_g2_4096_input_s
{
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^4096 (64 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^4095 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_dh_g2_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DH_4096.
 */
typedef struct icp_qat_fw_mmp_dh_4096_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^4096 (64 qwords)*/
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^4096 (64 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^4095 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_dh_4096_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation base 2 for
 * 8192-bit numbers , to be used when icp_qat_fw_pke_request_s::functionalityId
 * is #PKE_DH_G2_8192.
 */
typedef struct icp_qat_fw_mmp_dh_g2_8192_input_s
{
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^8192 (128 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^8191 and &lt; 2^8192 (128 qwords)*/
} icp_qat_fw_mmp_dh_g2_8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Diffie-Hellman Modular exponentiation for
 * 8192-bit numbers , to be used when icp_qat_fw_pke_request_s::functionalityId
 * is #PKE_DH_8192.
 */
typedef struct icp_qat_fw_mmp_dh_8192_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^8192 (128 qwords)*/
    uint64_t e; /**< exponent &gt; 0 and &lt; 2^8192 (128 qwords)*/
    uint64_t m; /**< modulus   &ge; 2^8191 and &lt; 2^8192 (128 qwords)*/
} icp_qat_fw_mmp_dh_8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 512 key generation first form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP1_512.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_512_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2 &lt; p &lt; 2^256 (4 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2 &lt; q &lt; 2^256 (4 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (8 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 512 key generation second form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP2_512.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_512_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^255 &lt; p &lt; 2^256 (4 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^255 &lt; q &lt; 2^256 (4 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (8 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 512 Encryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_EP_512.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_512_input_s
{
    uint64_t m; /**< message representative, &lt; n (8 qwords)*/
    uint64_t e; /**< RSA public key, &ge; 3 and &le; n-1 (8 qwords)*/
    uint64_t n; /**< RSA key, &gt; 0 and &lt; 2^256 (8 qwords)*/
} icp_qat_fw_mmp_rsa_ep_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 512 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP1_512.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_512_input_s
{
    uint64_t c; /**< cipher text representative, &lt; n (8 qwords)*/
    uint64_t d; /**< RSA private key (RSADP first form) (8 qwords)*/
    uint64_t n; /**< RSA key &gt; 0 and &lt; 2^256 (8 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1024 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP2_512.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_512_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (8 qwords)*/
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^255 &lt; p &lt; 2^256 (4 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^255 &lt; q &lt; 2^256 (4 qwords)*/
    uint64_t dp; /**< RSA private key, 0 &lt; dp &lt; p-1 (4 qwords)*/
    uint64_t dq; /**< RSA private key 0 &lt; dq &lt; q-1 (4 qwords)*/
    uint64_t qinv; /**< RSA private key 0 &lt; qInv &lt; p (4 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1024 key generation first form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP1_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_1024_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2 &lt; p &lt; 2^512 (8 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2 &lt; q &lt; 2^512 (8 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (16 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1024 key generation second form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP2_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_1024_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^511 &lt; p &lt; 2^512 (8 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^511 &lt; q &lt; 2^512 (8 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (16 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1024 Encryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_EP_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_1024_input_s
{
    uint64_t m; /**< message representative, &lt; n (16 qwords)*/
    uint64_t e; /**< RSA public key, &ge; 3 and &le; n-1 (16 qwords)*/
    uint64_t n; /**< RSA key, &gt; 0 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_rsa_ep_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1024 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP1_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_1024_input_s
{
    uint64_t c; /**< cipher text representative, &lt; n (16 qwords)*/
    uint64_t d; /**< RSA private key (RSADP first form) (16 qwords)*/
    uint64_t n; /**< RSA key &gt; 0 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1024 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP2_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_1024_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (16 qwords)*/
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^511 &lt; p &lt; 2^512 (8 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^511 &lt; q &lt; 2^512 (8 qwords)*/
    uint64_t dp; /**< RSA private key, 0 &lt; dp &lt; p-1 (8 qwords)*/
    uint64_t dq; /**< RSA private key 0 &lt; dq &lt; q-1 (8 qwords)*/
    uint64_t qinv; /**< RSA private key 0 &lt; qInv &lt; p (8 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1536 key generation first form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP1_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_1536_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2 &lt; p &lt; 2^768 (12 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2 &lt; q &lt; 2^768 (12 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (24 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1536 key generation second form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP2_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_1536_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^767 &lt; p &lt; 2^768 (12 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^767 &lt; q &lt; 2^768 (12 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (24 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1536 Encryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_EP_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_1536_input_s
{
    uint64_t m; /**< message representative, &lt; n (24 qwords)*/
    uint64_t e; /**< RSA public key, &ge; 3 and &le; (p*q)-1 (24 qwords)*/
    uint64_t n; /**< RSA key &gt; 0 and &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_mmp_rsa_ep_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1536 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP1_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_1536_input_s
{
    uint64_t c; /**< cipher text representative, &lt; n (24 qwords)*/
    uint64_t d; /**< RSA private key (24 qwords)*/
    uint64_t n; /**< RSA key, &gt; 0 and &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1536 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP2_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_1536_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (24 qwords)*/
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^767 &lt; p &lt; 2^768 (12 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^767 &lt; p &lt; 2^768 (12 qwords)*/
    uint64_t dp; /**< RSA private key, 0 &lt; dp &lt; p-1 (12 qwords)*/
    uint64_t dq; /**< RSA private key, 0 &lt; dq &lt; q-1 (12 qwords)*/
    uint64_t qinv; /**< RSA private key, 0 &lt; qInv &lt; p (12 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 2048 key generation first form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP1_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_2048_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2 &lt; p &lt; 2^1024 (16 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2 &lt; q &lt; 2^1024 (16 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (32 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 2048 key generation second form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP2_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_2048_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^1023 &lt; p &lt; 2^1024 (16 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^1023 &lt; q &lt; 2^1024 (16 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (32 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 2048 Encryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_EP_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_2048_input_s
{
    uint64_t m; /**< message representative, &lt; n (32 qwords)*/
    uint64_t e; /**< RSA public key, &ge; 3 and &le; n-1 (32 qwords)*/
    uint64_t n; /**< RSA key &gt; 0 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_rsa_ep_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 2048 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP1_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_2048_input_s
{
    uint64_t c; /**< cipher text representative, &lt; n (32 qwords)*/
    uint64_t d; /**< RSA private key (32 qwords)*/
    uint64_t n; /**< RSA key &gt; 0 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 2048 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP2_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_2048_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (32 qwords)*/
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^1023 &lt; p &lt; 2^1024 (16 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^1023 &lt; q &lt; 2^1024 (16 qwords)*/
    uint64_t dp; /**< RSA private key, 0 &lt; dp &lt; p-1 (16 qwords)*/
    uint64_t dq; /**< RSA private key, 0 &lt; dq &lt; q-1 (16 qwords)*/
    uint64_t qinv; /**< RSA private key, 0 &lt; qInv &lt; p (16 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 3072 key generation first form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP1_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_3072_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2 &lt; p &lt; 2^1536 (24 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2 &lt; q &lt; 2^1536 (24 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (48 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 3072 key generation second form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP2_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_3072_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^1535 &lt; p &lt; 2^1536 (24 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^1535 &lt; q &lt; 2^1536 (24 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (48 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 3072 Encryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_EP_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_3072_input_s
{
    uint64_t m; /**< message representative, &lt; n (48 qwords)*/
    uint64_t e; /**< RSA public key, &ge; 3 and &le; n-1 (48 qwords)*/
    uint64_t n; /**< RSA key &gt; 0 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_rsa_ep_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 3072 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP1_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_3072_input_s
{
    uint64_t c; /**< cipher text representative, &lt; n (48 qwords)*/
    uint64_t d; /**< RSA private key (48 qwords)*/
    uint64_t n; /**< RSA key &gt; 0 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 3072 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP2_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_3072_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (48 qwords)*/
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^1535 &lt; p &lt; 2^1536 (24 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^1535 &lt; q &lt; 2^1536 (24 qwords)*/
    uint64_t dp; /**< RSA private key, 0 &lt; dp &lt; p-1 (24 qwords)*/
    uint64_t dq; /**< RSA private key, 0 &lt; dq &lt; q-1 (24 qwords)*/
    uint64_t qinv; /**< RSA private key, 0 &lt; qInv &lt; p (24 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 4096 key generation first form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP1_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_4096_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2 &lt; p &lt; 2^2048 (32 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2 &lt; q &lt; 2^2048 (32 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (64 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 4096 key generation second form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_KP2_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_4096_input_s
{
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^2047 &lt; p &lt; 2^2048 (32 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^2047 &lt; q &lt; 2^2048 (32 qwords)*/
    uint64_t e; /**< RSA public key, must be odd, &ge; 3 and &le; (p*q)-1, &nbsp;with GCD(e, p-1, q-1) = 1 (64 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 4096 Encryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_EP_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_4096_input_s
{
    uint64_t m; /**< message representative, &lt; n (64 qwords)*/
    uint64_t e; /**< RSA public key, &ge; 3 and &le; n-1 (64 qwords)*/
    uint64_t n; /**< RSA key, &gt; 0 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_rsa_ep_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 4096 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP1_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_4096_input_s
{
    uint64_t c; /**< cipher text representative, &lt; n (64 qwords)*/
    uint64_t d; /**< RSA private key (64 qwords)*/
    uint64_t n; /**< RSA key, &gt; 0 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 4096 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_RSA_DP2_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_4096_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (64 qwords)*/
    uint64_t p; /**< RSA parameter, prime, &nbsp;2^2047 &lt; p &lt; 2^2048 (32 qwords)*/
    uint64_t q; /**< RSA parameter, prime, &nbsp;2^2047 &lt; q &lt; 2^2048 (32 qwords)*/
    uint64_t dp; /**< RSA private key, 0 &lt; dp &lt; p-1 (32 qwords)*/
    uint64_t dq; /**< RSA private key, 0 &lt; dq &lt; q-1 (32 qwords)*/
    uint64_t qinv; /**< RSA private key, 0 &lt; qInv &lt; p (32 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_4096_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 8192 Encryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_RSA_EP_8192.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_8192_input_s
{
    uint64_t m; /**< message representative, &lt; n (128 qwords)*/
    uint64_t e; /**< RSA public key, &ge; 3 and &le; n-1 (128 qwords)*/
    uint64_t n; /**< RSA key, &gt; 0 and &lt; 2^8192 (128 qwords)*/
} icp_qat_fw_mmp_rsa_ep_8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 8192 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_RSA_DP1_8192.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_8192_input_s
{
    uint64_t c; /**< cipher text representative, &lt; n (128 qwords)*/
    uint64_t d; /**< RSA private key (128 qwords)*/
    uint64_t n; /**< RSA key, &gt; 0 and &lt; 2^8192 (128 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 8192 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_RSA_DP2_8192.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_8192_input_s
{
    uint64_t c;  /**< cipher text representative, &lt; (p*q) (128 qwords)*/
    uint64_t p;  /**< RSA parameter, prime, &nbsp;2^4095 &lt; p &lt; 2^4096 (64
                    qwords)*/
    uint64_t q;  /**< RSA parameter, prime, &nbsp;2^4095 &lt; q &lt; 2^4096 (64
                    qwords)*/
    uint64_t dp; /**< RSA private key, 0 &lt; dp &lt; p-1 (64 qwords)*/
    uint64_t dq; /**< RSA private key, 0 &lt; dq &lt; q-1 (64 qwords)*/
    uint64_t qinv; /**< RSA private key, 0 &lt; qInv &lt; p (64 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 192-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_192.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_192_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^192 (3 qwords)*/
} icp_qat_fw_mmp_gcd_pt_192_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 256-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_256.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_256_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^256 (4 qwords)*/
} icp_qat_fw_mmp_gcd_pt_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 384-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_384.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_384_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^384 (6 qwords)*/
} icp_qat_fw_mmp_gcd_pt_384_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_512.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_512_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^512 (8 qwords)*/
} icp_qat_fw_mmp_gcd_pt_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_768.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_768_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^768 (12 qwords)*/
} icp_qat_fw_mmp_gcd_pt_768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_1024.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_1024_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_gcd_pt_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_1536.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_1536_input_s
{
    uint64_t m; /**<  (24 qwords)*/
} icp_qat_fw_mmp_gcd_pt_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_2048.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_2048_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_gcd_pt_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_3072.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_3072_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_gcd_pt_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for GCD primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_GCD_PT_4096.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_4096_input_s
{
    uint64_t m; /**< prime candidate &gt; 1 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_gcd_pt_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 160-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_160.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_160_input_s
{
    uint64_t m; /**< prime candidate, 2^159 &lt; m &lt; 2^160 (3 qwords)*/
} icp_qat_fw_mmp_fermat_pt_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_512.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_512_input_s
{
    uint64_t m; /**< prime candidate, 2^511 &lt; m &lt; 2^512 (8 qwords)*/
} icp_qat_fw_mmp_fermat_pt_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for &lte; 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_L512.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_l512_input_s
{
    uint64_t m; /**< prime candidate, 5 &lt; m &lt; 2^512 (8 qwords)*/
} icp_qat_fw_mmp_fermat_pt_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_768.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_768_input_s
{
    uint64_t m; /**< prime candidate, 2^767 &lt; m &lt; 2^768 (12 qwords)*/
} icp_qat_fw_mmp_fermat_pt_768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_1024.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_1024_input_s
{
    uint64_t m; /**< prime candidate, 2^1023 &lt; m &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_fermat_pt_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_1536.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_1536_input_s
{
    uint64_t m; /**< prime candidate, 2^1535 &lt; m &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_mmp_fermat_pt_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_2048.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_2048_input_s
{
    uint64_t m; /**< prime candidate, 2^2047 &lt; m &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_fermat_pt_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_3072.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_3072_input_s
{
    uint64_t m; /**< prime candidate, 2^3071 &lt; m &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_fermat_pt_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Fermat primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_FERMAT_PT_4096.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_4096_input_s
{
    uint64_t m; /**< prime candidate, 2^4095 &lt; m &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_fermat_pt_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 160-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_160.
 */
typedef struct icp_qat_fw_mmp_mr_pt_160_input_s
{
    uint64_t x; /**< randomness  &gt; 1 and &lt; m-1 (3 qwords)*/
    uint64_t m; /**< prime candidate &gt; 2^159 and &lt; 2^160 (3 qwords)*/
} icp_qat_fw_mmp_mr_pt_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_512.
 */
typedef struct icp_qat_fw_mmp_mr_pt_512_input_s
{
    uint64_t x; /**< randomness   &gt; 1 and &lt; m-1 (8 qwords)*/
    uint64_t m; /**< prime candidate  &gt; 2^511 and &lt; 2^512 (8 qwords)*/
} icp_qat_fw_mmp_mr_pt_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_768.
 */
typedef struct icp_qat_fw_mmp_mr_pt_768_input_s
{
    uint64_t x; /**< randomness  &gt; 1 and &lt; m-1 (12 qwords)*/
    uint64_t m; /**< prime candidate &gt; 2^767 and &lt; 2^768 (12 qwords)*/
} icp_qat_fw_mmp_mr_pt_768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_1024.
 */
typedef struct icp_qat_fw_mmp_mr_pt_1024_input_s
{
    uint64_t x; /**< randomness &gt; 1 and &lt; m-1 (16 qwords)*/
    uint64_t m; /**< prime candidate &gt; 2^1023 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_mr_pt_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_1536.
 */
typedef struct icp_qat_fw_mmp_mr_pt_1536_input_s
{
    uint64_t x; /**< randomness &gt; 1 and &lt; m-1 (24 qwords)*/
    uint64_t m; /**< prime candidate &gt; 2^1535 and &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_mmp_mr_pt_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_2048.
 */
typedef struct icp_qat_fw_mmp_mr_pt_2048_input_s
{
    uint64_t x; /**< randomness  &gt; 1 and &lt;m-1 (32 qwords)*/
    uint64_t m; /**< prime candidate  &gt; 2^2047 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_mr_pt_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_3072.
 */
typedef struct icp_qat_fw_mmp_mr_pt_3072_input_s
{
    uint64_t x; /**< randomness  &gt; 1 and &lt; m-1 (48 qwords)*/
    uint64_t m; /**< prime candidate &gt; 2^3071 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_mr_pt_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_4096.
 */
typedef struct icp_qat_fw_mmp_mr_pt_4096_input_s
{
    uint64_t x; /**< randomness  &gt; 1 and &lt; m-1 (64 qwords)*/
    uint64_t m; /**< prime candidate &gt; 2^4095 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_mr_pt_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Miller-Rabin primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_MR_PT_L512.
 */
typedef struct icp_qat_fw_mmp_mr_pt_l512_input_s
{
    uint64_t x; /**< randomness   &gt; 1 and &lt; m-1 (8 qwords)*/
    uint64_t m; /**< prime candidate  &gt; 1 and &lt; 2^512 (8 qwords)*/
} icp_qat_fw_mmp_mr_pt_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 160-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_160.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_160_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^159 and &lt; 2^160 (3 qwords)*/
} icp_qat_fw_mmp_lucas_pt_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_512.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_512_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^511 and &lt; 2^512 (8 qwords)*/
} icp_qat_fw_mmp_lucas_pt_512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_768.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_768_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^767 and &lt; 2^768 (12 qwords)*/
} icp_qat_fw_mmp_lucas_pt_768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_1024.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_1024_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^1023 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_mmp_lucas_pt_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_1536.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_1536_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^1535 and &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_mmp_lucas_pt_1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_2048.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_2048_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^2047 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_mmp_lucas_pt_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_3072.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_3072_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^3071 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_mmp_lucas_pt_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_4096.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_4096_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 2^4096 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_mmp_lucas_pt_4096_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Lucas primality test for L512-bit numbers ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_LUCAS_PT_L512.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_l512_input_s
{
    uint64_t m; /**< odd prime candidate &gt; 5 and &lt; 2^512 (8 qwords)*/
} icp_qat_fw_mmp_lucas_pt_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 512-bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L512.
 */
typedef struct icp_qat_fw_maths_modexp_l512_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^512 (8 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^512 (8 qwords)*/
    uint64_t m; /**< modulus   &gt; 0 and &lt; 2^512 (8 qwords)*/
} icp_qat_fw_maths_modexp_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 1024-bit ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L1024.
 */
typedef struct icp_qat_fw_maths_modexp_l1024_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^1024 (16 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^1024 (16 qwords)*/
    uint64_t m; /**< modulus &gt; 0 and &lt; 2^1024 (16 qwords)*/
} icp_qat_fw_maths_modexp_l1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 1536-bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L1536.
 */
typedef struct icp_qat_fw_maths_modexp_l1536_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^1536 (24 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^1536 (24 qwords)*/
    uint64_t m; /**< modulus   &gt; 0 and &lt; 2^1536 (24 qwords)*/
} icp_qat_fw_maths_modexp_l1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 2048-bit ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L2048.
 */
typedef struct icp_qat_fw_maths_modexp_l2048_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^2048 (32 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^2048 (32 qwords)*/
    uint64_t m; /**< modulus &gt; 0 and &lt; 2^2048 (32 qwords)*/
} icp_qat_fw_maths_modexp_l2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 2560-bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L2560.
 */
typedef struct icp_qat_fw_maths_modexp_l2560_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^2560 (40 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^2560 (40 qwords)*/
    uint64_t m; /**< modulus   &gt; 0 and &lt; 2^2560 (40 qwords)*/
} icp_qat_fw_maths_modexp_l2560_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 3072-bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L3072.
 */
typedef struct icp_qat_fw_maths_modexp_l3072_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^3072 (48 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^3072 (48 qwords)*/
    uint64_t m; /**< modulus   &gt; 0 and &lt; 2^3072 (48 qwords)*/
} icp_qat_fw_maths_modexp_l3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 3584-bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L3584.
 */
typedef struct icp_qat_fw_maths_modexp_l3584_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^3584 (56 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^3584 (56 qwords)*/
    uint64_t m; /**< modulus   &gt; 0 and &lt; 2^3584 (56 qwords)*/
} icp_qat_fw_maths_modexp_l3584_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers less than 4096-bit ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODEXP_L4096.
 */
typedef struct icp_qat_fw_maths_modexp_l4096_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^4096 (64 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^4096 (64 qwords)*/
    uint64_t m; /**< modulus   &gt; 0 and &lt; 2^4096 (64 qwords)*/
} icp_qat_fw_maths_modexp_l4096_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular exponentiation for numbers up to 8192
 * bits , to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #MATHS_MODEXP_L8192.
 */
typedef struct icp_qat_fw_maths_modexp_l8192_input_s
{
    uint64_t g; /**< base &ge; 0 and &lt; 2^8192 (128 qwords)*/
    uint64_t e; /**< exponent &ge; 0 and &lt; 2^8192 (128 qwords)*/
    uint64_t m; /**< modulus   &gt; 0 and &lt; 2^8192 (128 qwords)*/
} icp_qat_fw_maths_modexp_l8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less
 * than 128 bits , to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #MATHS_MODINV_ODD_L128.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l128_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^128 (2 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^128, coprime to a (2 qwords)*/
} icp_qat_fw_maths_modinv_odd_l128_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 192 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L192.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l192_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^192 (3 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^192, coprime to a (3 qwords)*/
} icp_qat_fw_maths_modinv_odd_l192_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 256 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L256.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l256_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^256 (4 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^256, coprime to a (4 qwords)*/
} icp_qat_fw_maths_modinv_odd_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 384 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L384.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l384_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^384 (6 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^384, coprime to a (6 qwords)*/
} icp_qat_fw_maths_modinv_odd_l384_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 512 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L512.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l512_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^512 (8 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^512, coprime to a (8 qwords)*/
} icp_qat_fw_maths_modinv_odd_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 768 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L768.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l768_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^768 (12 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^768 ,coprime to a (12 qwords)*/
} icp_qat_fw_maths_modinv_odd_l768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 1024 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L1024.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l1024_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^1024 (16 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^1024, coprime to a (16 qwords)*/
} icp_qat_fw_maths_modinv_odd_l1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 1536 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L1536.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l1536_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^1536 (24 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^1536, coprime to a (24 qwords)*/
} icp_qat_fw_maths_modinv_odd_l1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 2048 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L2048.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l2048_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^2048 (32 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^2048, coprime to a (32 qwords)*/
} icp_qat_fw_maths_modinv_odd_l2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 3072 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L3072.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l3072_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^3072 (48 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^3072, coprime to a (48 qwords)*/
} icp_qat_fw_maths_modinv_odd_l3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 4096 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_ODD_L4096.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l4096_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^4096 (64 qwords)*/
    uint64_t b; /**< odd modulus &gt; 0 and &lt; 2^4096, coprime to a (64 qwords)*/
} icp_qat_fw_maths_modinv_odd_l4096_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers up to
 * 8192 bits , to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #MATHS_MODINV_ODD_L8192.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l8192_input_s
{
    uint64_t a; /**< number &gt; 0 and &lt; 2^8192 (128 qwords)*/
    uint64_t
        b; /**< odd modulus &gt; 0 and &lt; 2^8192, coprime to a (128 qwords)*/
} icp_qat_fw_maths_modinv_odd_l8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less
 * than 128 bits , to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #MATHS_MODINV_EVEN_L128.
 */
typedef struct icp_qat_fw_maths_modinv_even_l128_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^128 (2 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^128, coprime with a (2 qwords)*/
} icp_qat_fw_maths_modinv_even_l128_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 192 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L192.
 */
typedef struct icp_qat_fw_maths_modinv_even_l192_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^192 (3 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^192, coprime with a (3 qwords)*/
} icp_qat_fw_maths_modinv_even_l192_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 256 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L256.
 */
typedef struct icp_qat_fw_maths_modinv_even_l256_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^256 (4 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^256, coprime with a (4 qwords)*/
} icp_qat_fw_maths_modinv_even_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 384 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L384.
 */
typedef struct icp_qat_fw_maths_modinv_even_l384_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^384 (6 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^384, coprime with a (6 qwords)*/
} icp_qat_fw_maths_modinv_even_l384_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 512 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L512.
 */
typedef struct icp_qat_fw_maths_modinv_even_l512_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^512 (8 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^512, coprime with a (8 qwords)*/
} icp_qat_fw_maths_modinv_even_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 768 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L768.
 */
typedef struct icp_qat_fw_maths_modinv_even_l768_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^768 (12 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^768, coprime with a (12 qwords)*/
} icp_qat_fw_maths_modinv_even_l768_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 1024 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L1024.
 */
typedef struct icp_qat_fw_maths_modinv_even_l1024_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^1024 (16 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^1024, coprime with a (16 qwords)*/
} icp_qat_fw_maths_modinv_even_l1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 1536 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L1536.
 */
typedef struct icp_qat_fw_maths_modinv_even_l1536_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^1536 (24 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^1536, coprime with a (24 qwords)*/
} icp_qat_fw_maths_modinv_even_l1536_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 2048 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L2048.
 */
typedef struct icp_qat_fw_maths_modinv_even_l2048_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^2048 (32 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^2048, coprime with a (32 qwords)*/
} icp_qat_fw_maths_modinv_even_l2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 3072 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L3072.
 */
typedef struct icp_qat_fw_maths_modinv_even_l3072_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^3072 (48 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^3072, coprime with a (48 qwords)*/
} icp_qat_fw_maths_modinv_even_l3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers less than 4096 bits ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_MODINV_EVEN_L4096.
 */
typedef struct icp_qat_fw_maths_modinv_even_l4096_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^4096 (64 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^4096, coprime with a (64 qwords)*/
} icp_qat_fw_maths_modinv_even_l4096_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for Modular multiplicative inverse for numbers up to
 * 8192 bits , to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #MATHS_MODINV_EVEN_L8192.
 */
typedef struct icp_qat_fw_maths_modinv_even_l8192_input_s
{
    uint64_t a; /**< odd number &gt; 0 and &lt; 2^8192 (128 qwords)*/
    uint64_t b; /**< even modulus   &gt; 0 and &lt; 2^8192, coprime with a (128
                   qwords)*/
} icp_qat_fw_maths_modinv_even_l8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_P_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_1024_160_input_s
{
    uint64_t x; /**< DSA 1024-bit randomness  (16 qwords)*/
    uint64_t q; /**< DSA 160-bit parameter  (3 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_1024_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA key generation G ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_G_1024.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_g_1024_input_s
{
    uint64_t p; /**< DSA 1024-bit parameter  (16 qwords)*/
    uint64_t q; /**< DSA 160-bit parameter  (3 qwords)*/
    uint64_t h; /**< DSA 1024-bit parameter  (16 qwords)*/
} icp_qat_fw_mmp_dsa_gen_g_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA key generation Y ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_Y_1024.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_y_1024_input_s
{
    uint64_t p; /**< DSA 1024-bit parameter  (16 qwords)*/
    uint64_t g; /**< DSA parameter  (16 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter (160 bits),  (3 qwords)*/
} icp_qat_fw_mmp_dsa_gen_y_1024_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_1024_160_input_s
{
    uint64_t k; /**< randomly generated DSA parameter  (3 qwords)*/
    uint64_t p; /**< DSA parameter,  (16 qwords)*/
    uint64_t q; /**< DSA parameter  (3 qwords)*/
    uint64_t g; /**< DSA parameter  (16 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_1024_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_S_160.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_s_160_input_s
{
    uint64_t m; /**< digest message to be signed  (3 qwords)*/
    uint64_t k; /**< randomly generated DSA parameter  (3 qwords)*/
    uint64_t q; /**< DSA parameter  (3 qwords)*/
    uint64_t r; /**< DSA parameter  (3 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter  (3 qwords)*/
} icp_qat_fw_mmp_dsa_sign_s_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_S_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input_s
{
    uint64_t m; /**< digest of the message to be signed  (3 qwords)*/
    uint64_t k; /**< randomly generated DSA parameter  (3 qwords)*/
    uint64_t p; /**< DSA parameter  (16 qwords)*/
    uint64_t q; /**< DSA parameter  (3 qwords)*/
    uint64_t g; /**< DSA parameter  (16 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter  (3 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_VERIFY_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_1024_160_input_s
{
    uint64_t r; /**< DSA 160-bits signature  (3 qwords)*/
    uint64_t s; /**< DSA 160-bits signature  (3 qwords)*/
    uint64_t m; /**< digest of the message  (3 qwords)*/
    uint64_t p; /**< DSA parameter  (16 qwords)*/
    uint64_t q; /**< DSA parameter  (3 qwords)*/
    uint64_t g; /**< DSA parameter  (16 qwords)*/
    uint64_t y; /**< DSA parameter  (16 qwords)*/
} icp_qat_fw_mmp_dsa_verify_1024_160_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_P_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_2048_224_input_s
{
    uint64_t x; /**< DSA 2048-bit randomness  (32 qwords)*/
    uint64_t q; /**< DSA 224-bit parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_2048_224_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA key generation Y ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_Y_2048.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_y_2048_input_s
{
    uint64_t p; /**< DSA 2048-bit parameter  (32 qwords)*/
    uint64_t g; /**< DSA parameter  (32 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter (224/256 bits),  (4 qwords)*/
} icp_qat_fw_mmp_dsa_gen_y_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_2048_224_input_s
{
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t p; /**< DSA parameter,  (32 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (32 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_2048_224_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_S_224.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_s_224_input_s
{
    uint64_t m; /**< digest message to be signed  (4 qwords)*/
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t r; /**< DSA parameter  (4 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_s_224_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_S_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input_s
{
    uint64_t m; /**< digest of the message to be signed  (4 qwords)*/
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t p; /**< DSA parameter  (32 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (32 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_VERIFY_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_2048_224_input_s
{
    uint64_t r; /**< DSA 224-bits signature  (4 qwords)*/
    uint64_t s; /**< DSA 224-bits signature  (4 qwords)*/
    uint64_t m; /**< digest of the message  (4 qwords)*/
    uint64_t p; /**< DSA parameter  (32 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (32 qwords)*/
    uint64_t y; /**< DSA parameter  (32 qwords)*/
} icp_qat_fw_mmp_dsa_verify_2048_224_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_P_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_2048_256_input_s
{
    uint64_t x; /**< DSA 2048-bit randomness  (32 qwords)*/
    uint64_t q; /**< DSA 256-bit parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_2048_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA key generation G ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_G_2048.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_g_2048_input_s
{
    uint64_t p; /**< DSA 2048-bit parameter  (32 qwords)*/
    uint64_t q; /**< DSA 256-bit parameter  (4 qwords)*/
    uint64_t h; /**< DSA 2048-bit parameter  (32 qwords)*/
} icp_qat_fw_mmp_dsa_gen_g_2048_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_2048_256_input_s
{
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t p; /**< DSA parameter,  (32 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (32 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_2048_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_S_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_s_256_input_s
{
    uint64_t m; /**< digest message to be signed  (4 qwords)*/
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t r; /**< DSA parameter  (4 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_s_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_S_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input_s
{
    uint64_t m; /**< digest of the message to be signed  (4 qwords)*/
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t p; /**< DSA parameter  (32 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (32 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_VERIFY_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_2048_256_input_s
{
    uint64_t r; /**< DSA 256-bits signature  (4 qwords)*/
    uint64_t s; /**< DSA 256-bits signature  (4 qwords)*/
    uint64_t m; /**< digest of the message  (4 qwords)*/
    uint64_t p; /**< DSA parameter  (32 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (32 qwords)*/
    uint64_t y; /**< DSA parameter  (32 qwords)*/
} icp_qat_fw_mmp_dsa_verify_2048_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_P_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_3072_256_input_s
{
    uint64_t x; /**< DSA 3072-bit randomness  (48 qwords)*/
    uint64_t q; /**< DSA 256-bit parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_3072_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA key generation G ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_G_3072.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_g_3072_input_s
{
    uint64_t p; /**< DSA 3072-bit parameter  (48 qwords)*/
    uint64_t q; /**< DSA 256-bit parameter  (4 qwords)*/
    uint64_t h; /**< DSA 3072-bit parameter  (48 qwords)*/
} icp_qat_fw_mmp_dsa_gen_g_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA key generation Y ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_GEN_Y_3072.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_y_3072_input_s
{
    uint64_t p; /**< DSA 3072-bit parameter  (48 qwords)*/
    uint64_t g; /**< DSA parameter  (48 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter (3072 bits),  (4 qwords)*/
} icp_qat_fw_mmp_dsa_gen_y_3072_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_3072_256_input_s
{
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t p; /**< DSA parameter,  (48 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (48 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_3072_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_SIGN_R_S_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input_s
{
    uint64_t m; /**< digest of the message to be signed  (4 qwords)*/
    uint64_t k; /**< randomly generated DSA parameter  (4 qwords)*/
    uint64_t p; /**< DSA parameter  (48 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (48 qwords)*/
    uint64_t x; /**< randomly generated DSA parameter  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_DSA_VERIFY_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_3072_256_input_s
{
    uint64_t r; /**< DSA 256-bits signature  (4 qwords)*/
    uint64_t s; /**< DSA 256-bits signature  (4 qwords)*/
    uint64_t m; /**< digest of the message  (4 qwords)*/
    uint64_t p; /**< DSA parameter  (48 qwords)*/
    uint64_t q; /**< DSA parameter  (4 qwords)*/
    uint64_t g; /**< DSA parameter  (48 qwords)*/
    uint64_t y; /**< DSA parameter  (48 qwords)*/
} icp_qat_fw_mmp_dsa_verify_3072_256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA Sign RS for curves B/K-163 and B/K-233 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_RS_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_input_s
{
    uint64_t in; /**< concatenated input parameters (G, n, q, a, b, k, e, d)  (36 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA Sign R for curves B/K-163 and B/K-233 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_R_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input_s
{
    uint64_t xg; /**< x coordinate of base point G of B/K-163 of B/K-233  (4 qwords)*/
    uint64_t yg; /**< y coordinate of base point G of B/K-163 or B/K-233  (4 qwords)*/
    uint64_t n; /**< order of the base point of B/K-163 or B/K-233  (4 qwords)*/
    uint64_t q; /**< field polynomial of B/K-163 or B/K-233  (4 qwords)*/
    uint64_t a; /**< a equation coefficient of B/K-163 of B/K-233  (4 qwords)*/
    uint64_t b; /**< b equation coefficient of B/K-163 or B/K-233  (4 qwords)*/
    uint64_t k; /**< random value &gt; 0 and &lt; n  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA Sign S for curves with n &lt; 2^256 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_S_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input_s
{
    uint64_t e; /**< hash of message (0 &lt; e &lt; 2^256)  (4 qwords)*/
    uint64_t d; /**< private key (&gt;0 and &lt; n)  (4 qwords)*/
    uint64_t r; /**< ECDSA r signature value (&gt;0 and &lt; n)  (4 qwords)*/
    uint64_t k; /**< random value &gt; 0 and &lt; n  (4 qwords)*/
    uint64_t n; /**< order of the base point G (2 &lt; n &lt; 2^256)  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA Verify for curves B/K-163 and B/K-233 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_VERIFY_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gf2_l256_input_s
{
    uint64_t in; /**< concatenated curve parameter (e,s,r,n,G,Q,a,b,q)  (44 qwords)*/
} icp_qat_fw_mmp_ecdsa_verify_gf2_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_RS_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_input_s
{
    uint64_t in; /**< concatenated input parameters (G, n, q, a, b, k, e, d)  (72 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GF2 Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_R_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input_s
{
    uint64_t xg; /**< x coordinate of verified base point (&gt; 0 and degree(x(G)) &lt; degree(q))  (8 qwords)*/
    uint64_t yg; /**< y coordinate of verified base point (&gt; 0 and degree(y(G)) &lt; degree(q))  (8 qwords)*/
    uint64_t n; /**< order of the base point G, which must be prime and a divisor of #E and &lt; 2^512)  (8 qwords)*/
    uint64_t q; /**< field polynomial of degree &gt; 2 and &lt; 512  (8 qwords)*/
    uint64_t a; /**< a equation coefficient (degree(a) &lt; degree(q))  (8 qwords)*/
    uint64_t b; /**< b equation coefficient (degree(b) &lt; degree(q))  (8 qwords)*/
    uint64_t k; /**< random value &gt; 0 and &lt; n  (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GF2 Sign S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_S_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input_s
{
    uint64_t e; /**< hash of message (0 &lt; e &lt; 2^512)  (8 qwords)*/
    uint64_t d; /**< private key (&gt;0 and &lt; n)  (8 qwords)*/
    uint64_t r; /**< ECDSA r signature value (&gt;0 and &lt; n)  (8 qwords)*/
    uint64_t k; /**< random value &gt; 0 and &lt; n  (8 qwords)*/
    uint64_t n; /**< order of the base point G, which must be prime and a divisor of #E and &lt; 2^512)  (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GF2 Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_VERIFY_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gf2_l512_input_s
{
    uint64_t in; /**< concatenated curve parameters (e, s, r, n, xG, yG, xQ, yQ, a, b, q)  (88 qwords)*/
} icp_qat_fw_mmp_ecdsa_verify_gf2_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GF2 Sign RS for curves B-571/K-571 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_RS_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_input_s
{
    uint64_t in; /**< concatenated input parameters (x(G), y(G), n, q, a, b, k, e, d)  (81 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GF2 Sign S for curves with deg(q) &lt; 576 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_S_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input_s
{
    uint64_t e; /**< hash of message &lt; 2^576  (9 qwords)*/
    uint64_t d; /**< private key (&gt; 0 and &lt; n)  (9 qwords)*/
    uint64_t r; /**< ECDSA r signature value  (&gt; 0 and &lt; n)  (9 qwords)*/
    uint64_t k; /**< random value (&gt; 0 and &lt; n)  (9 qwords)*/
    uint64_t n; /**< order of the base point of the curve (n &lt; 2^576)  (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GF2 Sign R for degree 571 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_R_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input_s
{
    uint64_t xg; /**< x coordinate of verified base point belonging to B/K-571  (9 qwords)*/
    uint64_t yg; /**< y coordinate of verified base point belonging to B/K-571  (9 qwords)*/
    uint64_t n; /**< order of the base point G  (9 qwords)*/
    uint64_t q; /**< irreducible field polynomial of B/K-571  (9 qwords)*/
    uint64_t a; /**< a coefficient of curve B/K-571 (degree(a) &lt; degree(q))  (9 qwords)*/
    uint64_t b; /**< b coefficient of curve B/K-571 (degree(b) &lt; degree(q))  (9 qwords)*/
    uint64_t k; /**< random value &gt; 0 and &lt; n  (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GF2 Verify for degree 571 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_VERIFY_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gf2_571_input_s
{
    uint64_t in; /**< concatenated input (e, s, r, n, G, Q, a, b, q) (99 qwords)*/
} icp_qat_fw_mmp_ecdsa_verify_gf2_571_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for MATHS GF2 Point Multiplication ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GF2_L256.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gf2_l256_input_s
{
    uint64_t k; /**< scalar multiplier &gt; 0 and &lt; 2^256 (4 qwords)*/
    uint64_t xg; /**< x coordinate of curve point (degree(xG) &lt; 256) (4 qwords)*/
    uint64_t yg; /**< y coordinate of curve point (degree(yG) &lt; 256) (4 qwords)*/
    uint64_t a; /**< a equation coefficient of B/K-163 or B/K-233 (4 qwords)*/
    uint64_t b; /**< b equation coefficient of B/K-163 or B/K-233 (4 qwords)*/
    uint64_t q; /**< field polynomial of B/K-163 or B/K-233 (4 qwords)*/
    uint64_t h; /**< cofactor of B/K-163 or B/K-233 (4 qwords)*/
} icp_qat_fw_maths_point_multiplication_gf2_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for MATHS GF2 Point Verification ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_VERIFY_GF2_L256.
 */
typedef struct icp_qat_fw_maths_point_verify_gf2_l256_input_s
{
    uint64_t xq; /**< x coordinate of input point (4 qwords)*/
    uint64_t yq; /**< y coordinate of input point (4 qwords)*/
    uint64_t q; /**< field polynomial of curve, degree(q) &lt; 256 (4 qwords)*/
    uint64_t a; /**< a equation coefficient of curve, degree(a) &lt; 256 (4 qwords)*/
    uint64_t b; /**< b equation coefficient of curve, degree(b) &lt; 256 (4 qwords)*/
} icp_qat_fw_maths_point_verify_gf2_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for MATHS GF2 Point Multiplication ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GF2_L512.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gf2_l512_input_s
{
    uint64_t k; /**< scalar multiplier &gt; 0 and &lt; 2^512 (8 qwords)*/
    uint64_t xg; /**< x coordinate of curve point (degree(xG) &lt; 512) (8 qwords)*/
    uint64_t yg; /**< y coordinate of curve point (degree(yG) &lt; 512) (8 qwords)*/
    uint64_t a; /**< a equation coefficient (degree(a) &lt; 512) (8 qwords)*/
    uint64_t b; /**< b equation coefficient (degree(b) &lt; 512) (8 qwords)*/
    uint64_t q; /**< field polynomial of degree &gt; 2 and &lt; 512 (8 qwords)*/
    uint64_t h; /**< cofactor (&lt; 2^512) (8 qwords)*/
} icp_qat_fw_maths_point_multiplication_gf2_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for MATHS GF2 Point Verification ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_VERIFY_GF2_L512.
 */
typedef struct icp_qat_fw_maths_point_verify_gf2_l512_input_s
{
    uint64_t xq; /**< x coordinate of input point (8 qwords)*/
    uint64_t yq; /**< y coordinate of input point (8 qwords)*/
    uint64_t q; /**< field polynomial of degree &gt; 2 and &lt; 512 (8 qwords)*/
    uint64_t a; /**< a equation coefficient (degree(a) &lt; 512) (8 qwords)*/
    uint64_t b; /**< b equation coefficient (degree(a) &lt; 512) (8 qwords)*/
} icp_qat_fw_maths_point_verify_gf2_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GF2 Point Multiplication for curves B-571/K-571 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GF2_571.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gf2_571_input_s
{
    uint64_t k; /**< scalar value &gt; 0 and &lt; 2^576 (9 qwords)*/
    uint64_t xg; /**< x coordinate of curve point (degree(xG) &lt; degree(q)) (9 qwords)*/
    uint64_t yg; /**< y coordinate of curve point (degree(xG) &lt; degree(q)) (9 qwords)*/
    uint64_t a; /**< a equation coefficient for B/K-571 (9 qwords)*/
    uint64_t b; /**< b equation coefficient for B/K-571 (9 qwords)*/
    uint64_t q; /**< field polynomial of B/K-571 (9 qwords)*/
    uint64_t h; /**< cofactor for B/K-571 (1 qwords)*/
} icp_qat_fw_maths_point_multiplication_gf2_571_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GF2 Point Verification for degree 571 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_VERIFY_GF2_571.
 */
typedef struct icp_qat_fw_maths_point_verify_gf2_571_input_s
{
    uint64_t xq; /**< x coordinate of candidate public key (9 qwords)*/
    uint64_t yq; /**< y coordinate of candidate public key (9 qwords)*/
    uint64_t q; /**< field polynomial of B/K-571 (9 qwords)*/
    uint64_t a; /**< a equation coefficient of B/K-571 (9 qwords)*/
    uint64_t b; /**< b equation coefficient of B/K-571 (9 qwords)*/
} icp_qat_fw_maths_point_verify_gf2_571_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_R_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input_s
{
    uint64_t xg; /**< x coordinate of base point G,  (4 qwords)*/
    uint64_t yg; /**< y coordinate of base point G,  (4 qwords)*/
    uint64_t n; /**< order of the base point G, which shall be prime  (4 qwords)*/
    uint64_t q; /**< modulus  (4 qwords)*/
    uint64_t a; /**< a equation coefficient  (4 qwords)*/
    uint64_t b; /**< b equation coefficient  (4 qwords)*/
    uint64_t k; /**< random value  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_S_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input_s
{
    uint64_t e; /**< digest of the message to be signed  (4 qwords)*/
    uint64_t d; /**< private key  (4 qwords)*/
    uint64_t r; /**< DSA r signature value  (4 qwords)*/
    uint64_t k; /**< random value  (4 qwords)*/
    uint64_t n; /**< order of the base point G, which shall be prime  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_RS_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_input_s
{
    uint64_t in; /**< {xG, yG, n, q, a, b, k, e, d} concatenated  (36 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_VERIFY_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gfp_l256_input_s
{
    uint64_t in; /**< in = {e, s, r, n, xG, yG, xQ, yQ, a, b ,q} concatenated  (44 qwords)*/
} icp_qat_fw_mmp_ecdsa_verify_gfp_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_R_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input_s
{
    uint64_t xg; /**< x coordinate of base point G,  (8 qwords)*/
    uint64_t yg; /**< y coordinate of base point G,  (8 qwords)*/
    uint64_t n; /**< order of the base point G, which shall be prime  (8 qwords)*/
    uint64_t q; /**< modulus  (8 qwords)*/
    uint64_t a; /**< a equation coefficient  (8 qwords)*/
    uint64_t b; /**< b equation coefficient  (8 qwords)*/
    uint64_t k; /**< random value  (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_S_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input_s
{
    uint64_t e; /**< digest of the message to be signed  (8 qwords)*/
    uint64_t d; /**< private key  (8 qwords)*/
    uint64_t r; /**< DSA r signature value  (8 qwords)*/
    uint64_t k; /**< random value  (8 qwords)*/
    uint64_t n; /**< order of the base point G, which shall be prime  (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_RS_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_input_s
{
    uint64_t in; /**< {xG, yG, n, q, a, b, k, e, d} concatenated  (72 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_VERIFY_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gfp_l512_input_s
{
    uint64_t in; /**< in = {e, s, r, n, xG, yG, xQ, yQ, a, b ,q} concatenated  (88 qwords)*/
} icp_qat_fw_mmp_ecdsa_verify_gfp_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign R ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_R_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input_s
{
    uint64_t xg; /**< x coordinate of base point G,  (9 qwords)*/
    uint64_t yg; /**< y coordinate of base point G,  (9 qwords)*/
    uint64_t n; /**< order of the base point G, which shall be prime  (9 qwords)*/
    uint64_t q; /**< modulus  (9 qwords)*/
    uint64_t a; /**< a equation coefficient  (9 qwords)*/
    uint64_t b; /**< b equation coefficient  (9 qwords)*/
    uint64_t k; /**< random value  (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign S ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_S_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input_s
{
    uint64_t e; /**< digest of the message to be signed  (9 qwords)*/
    uint64_t d; /**< private key  (9 qwords)*/
    uint64_t r; /**< DSA r signature value  (9 qwords)*/
    uint64_t k; /**< random value  (9 qwords)*/
    uint64_t n; /**< order of the base point G, which shall be prime  (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_SIGN_RS_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_input_s
{
    uint64_t in; /**< {xG, yG, n, q, a, b, k, e, d} concatenated  (81 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECDSA GFP Verify ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #PKE_ECDSA_VERIFY_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gfp_521_input_s
{
    uint64_t in; /**< in = {e, s, r, n, xG, yG, xQ, yQ, a, b ,q} concatenated  (99 qwords)*/
} icp_qat_fw_mmp_ecdsa_verify_gfp_521_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GFP Point Multiplication ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GFP_L256.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gfp_l256_input_s
{
    uint64_t k; /**< scalar multiplier  (4 qwords)*/
    uint64_t xg; /**< x coordinate of curve point  (4 qwords)*/
    uint64_t yg; /**< y coordinate of curve point  (4 qwords)*/
    uint64_t a; /**< a equation coefficient  (4 qwords)*/
    uint64_t b; /**< b equation coefficient  (4 qwords)*/
    uint64_t q; /**< modulus  (4 qwords)*/
    uint64_t h; /**< cofactor  (4 qwords)*/
} icp_qat_fw_maths_point_multiplication_gfp_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GFP Partial Point Verification ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_VERIFY_GFP_L256.
 */
typedef struct icp_qat_fw_maths_point_verify_gfp_l256_input_s
{
    uint64_t xq; /**< x coordinate of candidate point (4 qwords)*/
    uint64_t yq; /**< y coordinate of candidate point (4 qwords)*/
    uint64_t q; /**< modulus (4 qwords)*/
    uint64_t a; /**< a equation coefficient  (4 qwords)*/
    uint64_t b; /**< b equation coefficient  (4 qwords)*/
} icp_qat_fw_maths_point_verify_gfp_l256_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GFP Point Multiplication ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GFP_L512.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gfp_l512_input_s
{
    uint64_t k; /**< scalar multiplier  (8 qwords)*/
    uint64_t xg; /**< x coordinate of curve point  (8 qwords)*/
    uint64_t yg; /**< y coordinate of curve point  (8 qwords)*/
    uint64_t a; /**< a equation coefficient  (8 qwords)*/
    uint64_t b; /**< b equation coefficient  (8 qwords)*/
    uint64_t q; /**< modulus  (8 qwords)*/
    uint64_t h; /**< cofactor  (8 qwords)*/
} icp_qat_fw_maths_point_multiplication_gfp_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GFP Partial Point ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_VERIFY_GFP_L512.
 */
typedef struct icp_qat_fw_maths_point_verify_gfp_l512_input_s
{
    uint64_t xq; /**< x coordinate of candidate point (8 qwords)*/
    uint64_t yq; /**< y coordinate of candidate point (8 qwords)*/
    uint64_t q; /**< modulus  (8 qwords)*/
    uint64_t a; /**< a equation coefficient  (8 qwords)*/
    uint64_t b; /**< b equation coefficient  (8 qwords)*/
} icp_qat_fw_maths_point_verify_gfp_l512_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GFP Point Multiplication ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GFP_521.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gfp_521_input_s
{
    uint64_t k; /**< scalar multiplier  (9 qwords)*/
    uint64_t xg; /**< x coordinate of curve point  (9 qwords)*/
    uint64_t yg; /**< y coordinate of curve point  (9 qwords)*/
    uint64_t a; /**< a equation coefficient  (9 qwords)*/
    uint64_t b; /**< b equation coefficient (9 qwords)*/
    uint64_t q; /**< modulus  (9 qwords)*/
    uint64_t h; /**< cofactor  (1 qwords)*/
} icp_qat_fw_maths_point_multiplication_gfp_521_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC GFP Partial Point Verification ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #MATHS_POINT_VERIFY_GFP_521.
 */
typedef struct icp_qat_fw_maths_point_verify_gfp_521_input_s
{
    uint64_t xq; /**< x coordinate of candidate point (9 qwords)*/
    uint64_t yq; /**< y coordinate of candidate point (9 qwords)*/
    uint64_t q; /**< modulus  (9 qwords)*/
    uint64_t a; /**< a equation coefficient  (9 qwords)*/
    uint64_t b; /**< b equation coefficient (9 qwords)*/
} icp_qat_fw_maths_point_verify_gfp_521_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC curve25519 Variable Point Multiplication [k]P(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #POINT_MULTIPLICATION_C25519.
 */
typedef struct icp_qat_fw_point_multiplication_c25519_input_s
{
    uint64_t xp; /**< xP = Montgomery affine coordinate X of point P  (4 qwords)*/
    uint64_t k; /**< k  = scalar  (4 qwords)*/
} icp_qat_fw_point_multiplication_c25519_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC curve25519 Generator Point Multiplication [k]G(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #GENERATOR_MULTIPLICATION_C25519.
 */
typedef struct icp_qat_fw_generator_multiplication_c25519_input_s
{
    uint64_t k; /**< k  = scalar  (4 qwords)*/
} icp_qat_fw_generator_multiplication_c25519_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC edwards25519 Variable Point Multiplication [k]P, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #POINT_MULTIPLICATION_ED25519.
 */
typedef struct icp_qat_fw_point_multiplication_ed25519_input_s
{
    uint64_t xp; /**< xP = Twisted Edwards affine coordinate X of point P  (4 qwords)*/
    uint64_t yp; /**< yP = Twisted Edwards affine coordinate Y of point P  (4 qwords)*/
    uint64_t k; /**< k  = scalar  (4 qwords)*/
} icp_qat_fw_point_multiplication_ed25519_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC edwards25519 Generator Point Multiplication [k]G, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #GENERATOR_MULTIPLICATION_ED25519.
 */
typedef struct icp_qat_fw_generator_multiplication_ed25519_input_s
{
    uint64_t k; /**< k  = scalar  (4 qwords)*/
} icp_qat_fw_generator_multiplication_ed25519_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC curve448 Variable Point Multiplication [k]P(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #POINT_MULTIPLICATION_C448.
 */
typedef struct icp_qat_fw_point_multiplication_c448_input_s
{
    uint64_t xp; /**< xP = Montgomery affine coordinate X of point P  (8 qwords)*/
    uint64_t k; /**< k  = scalar  (8 qwords)*/
} icp_qat_fw_point_multiplication_c448_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC curve448 Generator Point Multiplication [k]G(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #GENERATOR_MULTIPLICATION_C448.
 */
typedef struct icp_qat_fw_generator_multiplication_c448_input_s
{
    uint64_t k; /**< k  = scalar  (8 qwords)*/
} icp_qat_fw_generator_multiplication_c448_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC edwards448 Variable Point Multiplication [k]P, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #POINT_MULTIPLICATION_ED448.
 */
typedef struct icp_qat_fw_point_multiplication_ed448_input_s
{
    uint64_t xp; /**< xP = Edwards affine coordinate X of point P  (8 qwords)*/
    uint64_t yp; /**< yP = Edwards affine coordinate Y of point P  (8 qwords)*/
    uint64_t k; /**< k  = scalar  (8 qwords)*/
} icp_qat_fw_point_multiplication_ed448_input_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC edwards448 Generator Point Multiplication [k]P, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is #GENERATOR_MULTIPLICATION_ED448.
 */
typedef struct icp_qat_fw_generator_multiplication_ed448_input_s
{
    uint64_t k; /**< k  = scalar  (8 qwords)*/
} icp_qat_fw_generator_multiplication_ed448_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P521 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_ECDSA_SIGN_RS_P521.
 */
typedef struct icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p521_input_s
{
    uint64_t kpt_wrapped;          /**<  (42 qwords)*/
    uint64_t kpt_wrapping_context; /**< unwrap context (8 qwords)*/
    uint64_t e;                    /**<  (6 qwords)*/
} icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p521_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC P384 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_ECDSA_SIGN_RS_P384.
 */
typedef struct icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p384_input_s
{
    uint64_t kpt_wrapped;          /**<  (42 qwords)*/
    uint64_t kpt_wrapping_context; /**< unwrap context (8 qwords)*/
    uint64_t e;                    /**<  (6 qwords)*/
} icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p384_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ECC KPT P256 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_ECDSA_SIGN_RS_P256.
 */
typedef struct icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p256_input_s
{
    uint64_t kpt_wrapped;        /**<  (28 qwords)*/
    uint64_t key_unwrap_context; /**< unwrap context  (8 qwords)*/
    uint64_t e;                  /**<  (4 qwords)*/
} icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p256_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 512 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP1_512.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_512_input_s
{
    uint64_t c;           /**< cipher text representative, &lt; n (8 qwords)*/
    uint64_t kpt_wrapped; /**<  (16 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_512_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 1024 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP1_1024.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_1024_input_s
{
    uint64_t c;           /**< cipher text representative, &lt; n (16 qwords)*/
    uint64_t kpt_wrapped; /**<  (32 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_1024_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 1536 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP1_1536.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_1536_input_s
{
    uint64_t c;           /**< cipher text representative, &lt; n (24 qwords)*/
    uint64_t kpt_wrapped; /**<  (48 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_1536_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 2048 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP1_2048.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_2048_input_s
{
    uint64_t c;           /**< cipher text representative, &lt; n (32 qwords)*/
    uint64_t kpt_wrapped; /**<  (64 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_2048_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 3072 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP1_3072.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_3072_input_s
{
    uint64_t c;           /**< cipher text representative, &lt; n (48 qwords)*/
    uint64_t kpt_wrapped; /**<  (96 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_3072_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 4096 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP1_4096.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_4096_input_s
{
    uint64_t c;           /**< cipher text representative, &lt; n (64 qwords)*/
    uint64_t kpt_wrapped; /**<  (128 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_4096_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 8192 Decryption ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP1_8192.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_8192_input_s
{
    uint64_t c;           /**< cipher text representative, &lt; n (128 qwords)*/
    uint64_t kpt_wrapped; /**<  (256 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 512 decryption second form ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP2_512.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_512_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (8 qwords)*/
    uint64_t kpt_wrapped;        /**<  (28 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_512_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 1024 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP2_1024.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_1024_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (16 qwords)*/
    uint64_t kpt_wrapped;        /**<  (56 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_1024_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for KPT RSA 1536 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP2_1536.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_1536_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (24 qwords)*/
    uint64_t kpt_wrapped;        /**<  (84 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_1536_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 2048 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP2_2048.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_2048_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (32 qwords)*/
    uint64_t kpt_wrapped;        /**<  (112 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_2048_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP2_3072.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_3072_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (48 qwords)*/
    uint64_t kpt_wrapped;        /**<  (168 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_3072_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 4096 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP2_4096.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_4096_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (64 qwords)*/
    uint64_t kpt_wrapped;        /**<  (224 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_4096_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Input parameter list for RSA 8192 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_request_s::functionalityId is
 * #PKE_KPT_RSA_DP2_8192.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_8192_input_s
{
    uint64_t c; /**< cipher text representative, &lt; (p*q) (128 qwords)*/
    uint64_t kpt_wrapped;        /**<  (448 qwords)*/
    uint64_t kpt_unwrap_context; /**< unwrap context (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_8192_input_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    MMP input parameters
 */
typedef union icp_qat_fw_mmp_input_param_u
{
    /** Generic parameter structure : All members of this wrapper structure
     * are pointers to large integers.
     */
    uint64_t flat_array[ICP_QAT_FW_PKE_INPUT_COUNT_MAX];
    /** ECC P384 Variable Point Multiplication [k]P  */

    icp_qat_fw_mmp_ec_point_multiplication_p384_input_t
        mmp_ec_point_multiplication_p384;

    /** ECC P384 Generator Point Multiplication [k]G  */
    icp_qat_fw_mmp_ec_generator_multiplication_p384_input_t
        mmp_ec_generator_multiplication_p384;

    /** ECC P384 ECDSA Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_p384_input_t mmp_ecdsa_sign_rs_p384;

    /** ECC P256 Variable Point Multiplication [k]P  */
    icp_qat_fw_mmp_ec_point_multiplication_p256_input_t
        mmp_ec_point_multiplication_p256;

    /** ECC P256 Generator Point Multiplication [k]G  */
    icp_qat_fw_mmp_ec_generator_multiplication_p256_input_t
        mmp_ec_generator_multiplication_p256;

    /** ECC P256 ECDSA Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_p256_input_t mmp_ecdsa_sign_rs_p256;

    /** Initialisation sequence  */
    icp_qat_fw_mmp_init_input_t mmp_init;

    /** Diffie-Hellman Modular exponentiation base 2 for 768-bit numbers  */
    icp_qat_fw_mmp_dh_g2_768_input_t mmp_dh_g2_768;

    /** Diffie-Hellman Modular exponentiation for 768-bit numbers  */
    icp_qat_fw_mmp_dh_768_input_t mmp_dh_768;

    /** Diffie-Hellman Modular exponentiation base 2 for 1024-bit numbers  */
    icp_qat_fw_mmp_dh_g2_1024_input_t mmp_dh_g2_1024;

    /** Diffie-Hellman Modular exponentiation for 1024-bit numbers  */
    icp_qat_fw_mmp_dh_1024_input_t mmp_dh_1024;

    /** Diffie-Hellman Modular exponentiation base 2 for 1536-bit numbers  */
    icp_qat_fw_mmp_dh_g2_1536_input_t mmp_dh_g2_1536;

    /** Diffie-Hellman Modular exponentiation for 1536-bit numbers  */
    icp_qat_fw_mmp_dh_1536_input_t mmp_dh_1536;

    /** Diffie-Hellman Modular exponentiation base 2 for 2048-bit numbers  */
    icp_qat_fw_mmp_dh_g2_2048_input_t mmp_dh_g2_2048;

    /** Diffie-Hellman Modular exponentiation for 2048-bit numbers  */
    icp_qat_fw_mmp_dh_2048_input_t mmp_dh_2048;

    /** Diffie-Hellman Modular exponentiation base 2 for 3072-bit numbers  */
    icp_qat_fw_mmp_dh_g2_3072_input_t mmp_dh_g2_3072;

    /** Diffie-Hellman Modular exponentiation for 3072-bit numbers  */
    icp_qat_fw_mmp_dh_3072_input_t mmp_dh_3072;

    /** Diffie-Hellman Modular exponentiation base 2 for 4096-bit numbers  */
    icp_qat_fw_mmp_dh_g2_4096_input_t mmp_dh_g2_4096;

    /** Diffie-Hellman Modular exponentiation for 4096-bit numbers  */
    icp_qat_fw_mmp_dh_4096_input_t mmp_dh_4096;

    /** Diffie-Hellman Modular exponentiation base 2 for 8192-bit numbers  */
    icp_qat_fw_mmp_dh_g2_8192_input_t mmp_dh_g2_8192;

    /** Diffie-Hellman Modular exponentiation for 8192-bit numbers  */
    icp_qat_fw_mmp_dh_8192_input_t mmp_dh_8192;

    /** RSA 512 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_512_input_t mmp_rsa_kp1_512;

    /** RSA 512 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_512_input_t mmp_rsa_kp2_512;

    /** RSA 512 Encryption  */
    icp_qat_fw_mmp_rsa_ep_512_input_t mmp_rsa_ep_512;

    /** RSA 512 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_512_input_t mmp_rsa_dp1_512;

    /** RSA 1024 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_512_input_t mmp_rsa_dp2_512;

    /** RSA 1024 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_1024_input_t mmp_rsa_kp1_1024;

    /** RSA 1024 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_1024_input_t mmp_rsa_kp2_1024;

    /** RSA 1024 Encryption  */
    icp_qat_fw_mmp_rsa_ep_1024_input_t mmp_rsa_ep_1024;

    /** RSA 1024 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_1024_input_t mmp_rsa_dp1_1024;

    /** RSA 1024 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_1024_input_t mmp_rsa_dp2_1024;

    /** RSA 1536 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_1536_input_t mmp_rsa_kp1_1536;

    /** RSA 1536 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_1536_input_t mmp_rsa_kp2_1536;

    /** RSA 1536 Encryption  */
    icp_qat_fw_mmp_rsa_ep_1536_input_t mmp_rsa_ep_1536;

    /** RSA 1536 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_1536_input_t mmp_rsa_dp1_1536;

    /** RSA 1536 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_1536_input_t mmp_rsa_dp2_1536;

    /** RSA 2048 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_2048_input_t mmp_rsa_kp1_2048;

    /** RSA 2048 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_2048_input_t mmp_rsa_kp2_2048;

    /** RSA 2048 Encryption  */
    icp_qat_fw_mmp_rsa_ep_2048_input_t mmp_rsa_ep_2048;

    /** RSA 2048 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_2048_input_t mmp_rsa_dp1_2048;

    /** RSA 2048 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_2048_input_t mmp_rsa_dp2_2048;

    /** RSA 3072 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_3072_input_t mmp_rsa_kp1_3072;

    /** RSA 3072 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_3072_input_t mmp_rsa_kp2_3072;

    /** RSA 3072 Encryption  */
    icp_qat_fw_mmp_rsa_ep_3072_input_t mmp_rsa_ep_3072;

    /** RSA 3072 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_3072_input_t mmp_rsa_dp1_3072;

    /** RSA 3072 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_3072_input_t mmp_rsa_dp2_3072;

    /** RSA 4096 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_4096_input_t mmp_rsa_kp1_4096;

    /** RSA 4096 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_4096_input_t mmp_rsa_kp2_4096;

    /** RSA 4096 Encryption  */
    icp_qat_fw_mmp_rsa_ep_4096_input_t mmp_rsa_ep_4096;

    /** RSA 4096 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_4096_input_t mmp_rsa_dp1_4096;

    /** RSA 4096 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_4096_input_t mmp_rsa_dp2_4096;

    /** RSA 8192 Encryption  */
    icp_qat_fw_mmp_rsa_ep_8192_input_t mmp_rsa_ep_8192;

    /** RSA 8192 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_8192_input_t mmp_rsa_dp1_8192;

    /** RSA 8192 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_8192_input_t mmp_rsa_dp2_8192;

    /** GCD primality test for 192-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_192_input_t mmp_gcd_pt_192;

    /** GCD primality test for 256-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_256_input_t mmp_gcd_pt_256;

    /** GCD primality test for 384-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_384_input_t mmp_gcd_pt_384;

    /** GCD primality test for 512-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_512_input_t mmp_gcd_pt_512;

    /** GCD primality test for 768-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_768_input_t mmp_gcd_pt_768;

    /** GCD primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_1024_input_t mmp_gcd_pt_1024;

    /** GCD primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_1536_input_t mmp_gcd_pt_1536;

    /** GCD primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_2048_input_t mmp_gcd_pt_2048;

    /** GCD primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_3072_input_t mmp_gcd_pt_3072;

    /** GCD primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_4096_input_t mmp_gcd_pt_4096;

    /** Fermat primality test for 160-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_160_input_t mmp_fermat_pt_160;

    /** Fermat primality test for 512-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_512_input_t mmp_fermat_pt_512;

    /** Fermat primality test for &lte; 512-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_l512_input_t mmp_fermat_pt_l512;

    /** Fermat primality test for 768-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_768_input_t mmp_fermat_pt_768;

    /** Fermat primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_1024_input_t mmp_fermat_pt_1024;

    /** Fermat primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_1536_input_t mmp_fermat_pt_1536;

    /** Fermat primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_2048_input_t mmp_fermat_pt_2048;

    /** Fermat primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_3072_input_t mmp_fermat_pt_3072;

    /** Fermat primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_4096_input_t mmp_fermat_pt_4096;

    /** Miller-Rabin primality test for 160-bit numbers  */
    icp_qat_fw_mmp_mr_pt_160_input_t mmp_mr_pt_160;

    /** Miller-Rabin primality test for 512-bit numbers  */
    icp_qat_fw_mmp_mr_pt_512_input_t mmp_mr_pt_512;

    /** Miller-Rabin primality test for 768-bit numbers  */
    icp_qat_fw_mmp_mr_pt_768_input_t mmp_mr_pt_768;

    /** Miller-Rabin primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_mr_pt_1024_input_t mmp_mr_pt_1024;

    /** Miller-Rabin primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_mr_pt_1536_input_t mmp_mr_pt_1536;

    /** Miller-Rabin primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_mr_pt_2048_input_t mmp_mr_pt_2048;

    /** Miller-Rabin primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_mr_pt_3072_input_t mmp_mr_pt_3072;

    /** Miller-Rabin primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_mr_pt_4096_input_t mmp_mr_pt_4096;

    /** Miller-Rabin primality test for 512-bit numbers  */
    icp_qat_fw_mmp_mr_pt_l512_input_t mmp_mr_pt_l512;

    /** Lucas primality test for 160-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_160_input_t mmp_lucas_pt_160;

    /** Lucas primality test for 512-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_512_input_t mmp_lucas_pt_512;

    /** Lucas primality test for 768-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_768_input_t mmp_lucas_pt_768;

    /** Lucas primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_1024_input_t mmp_lucas_pt_1024;

    /** Lucas primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_1536_input_t mmp_lucas_pt_1536;

    /** Lucas primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_2048_input_t mmp_lucas_pt_2048;

    /** Lucas primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_3072_input_t mmp_lucas_pt_3072;

    /** Lucas primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_4096_input_t mmp_lucas_pt_4096;

    /** Lucas primality test for L512-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_l512_input_t mmp_lucas_pt_l512;

    /** Modular exponentiation for numbers less than 512-bits  */
    icp_qat_fw_maths_modexp_l512_input_t maths_modexp_l512;

    /** Modular exponentiation for numbers less than 1024-bit  */
    icp_qat_fw_maths_modexp_l1024_input_t maths_modexp_l1024;

    /** Modular exponentiation for numbers less than 1536-bits  */
    icp_qat_fw_maths_modexp_l1536_input_t maths_modexp_l1536;

    /** Modular exponentiation for numbers less than 2048-bit  */
    icp_qat_fw_maths_modexp_l2048_input_t maths_modexp_l2048;

    /** Modular exponentiation for numbers less than 2560-bits  */
    icp_qat_fw_maths_modexp_l2560_input_t maths_modexp_l2560;

    /** Modular exponentiation for numbers less than 3072-bits  */
    icp_qat_fw_maths_modexp_l3072_input_t maths_modexp_l3072;

    /** Modular exponentiation for numbers less than 3584-bits  */
    icp_qat_fw_maths_modexp_l3584_input_t maths_modexp_l3584;

    /** Modular exponentiation for numbers less than 4096-bit  */
    icp_qat_fw_maths_modexp_l4096_input_t maths_modexp_l4096;

    /** Modular exponentiation for numbers up to 8192 bits  */
    icp_qat_fw_maths_modexp_l8192_input_t maths_modexp_l8192;

    /** Modular multiplicative inverse for numbers less than 128 bits  */
    icp_qat_fw_maths_modinv_odd_l128_input_t maths_modinv_odd_l128;

    /** Modular multiplicative inverse for numbers less than 192 bits  */
    icp_qat_fw_maths_modinv_odd_l192_input_t maths_modinv_odd_l192;

    /** Modular multiplicative inverse for numbers less than 256 bits  */
    icp_qat_fw_maths_modinv_odd_l256_input_t maths_modinv_odd_l256;

    /** Modular multiplicative inverse for numbers less than 384 bits  */
    icp_qat_fw_maths_modinv_odd_l384_input_t maths_modinv_odd_l384;

    /** Modular multiplicative inverse for numbers less than 512 bits  */
    icp_qat_fw_maths_modinv_odd_l512_input_t maths_modinv_odd_l512;

    /** Modular multiplicative inverse for numbers less than 768 bits  */
    icp_qat_fw_maths_modinv_odd_l768_input_t maths_modinv_odd_l768;

    /** Modular multiplicative inverse for numbers less than 1024 bits  */
    icp_qat_fw_maths_modinv_odd_l1024_input_t maths_modinv_odd_l1024;

    /** Modular multiplicative inverse for numbers less than 1536 bits  */
    icp_qat_fw_maths_modinv_odd_l1536_input_t maths_modinv_odd_l1536;

    /** Modular multiplicative inverse for numbers less than 2048 bits  */
    icp_qat_fw_maths_modinv_odd_l2048_input_t maths_modinv_odd_l2048;

    /** Modular multiplicative inverse for numbers less than 3072 bits  */
    icp_qat_fw_maths_modinv_odd_l3072_input_t maths_modinv_odd_l3072;

    /** Modular multiplicative inverse for numbers less than 4096 bits  */
    icp_qat_fw_maths_modinv_odd_l4096_input_t maths_modinv_odd_l4096;

    /** Modular multiplicative inverse for numbers up to 8192 bits  */
    icp_qat_fw_maths_modinv_odd_l8192_input_t maths_modinv_odd_l8192;

    /** Modular multiplicative inverse for numbers less than 128 bits  */
    icp_qat_fw_maths_modinv_even_l128_input_t maths_modinv_even_l128;

    /** Modular multiplicative inverse for numbers less than 192 bits  */
    icp_qat_fw_maths_modinv_even_l192_input_t maths_modinv_even_l192;

    /** Modular multiplicative inverse for numbers less than 256 bits  */
    icp_qat_fw_maths_modinv_even_l256_input_t maths_modinv_even_l256;

    /** Modular multiplicative inverse for numbers less than 384 bits  */
    icp_qat_fw_maths_modinv_even_l384_input_t maths_modinv_even_l384;

    /** Modular multiplicative inverse for numbers less than 512 bits  */
    icp_qat_fw_maths_modinv_even_l512_input_t maths_modinv_even_l512;

    /** Modular multiplicative inverse for numbers less than 768 bits  */
    icp_qat_fw_maths_modinv_even_l768_input_t maths_modinv_even_l768;

    /** Modular multiplicative inverse for numbers less than 1024 bits  */
    icp_qat_fw_maths_modinv_even_l1024_input_t maths_modinv_even_l1024;

    /** Modular multiplicative inverse for numbers less than 1536 bits  */
    icp_qat_fw_maths_modinv_even_l1536_input_t maths_modinv_even_l1536;

    /** Modular multiplicative inverse for numbers less than 2048 bits  */
    icp_qat_fw_maths_modinv_even_l2048_input_t maths_modinv_even_l2048;

    /** Modular multiplicative inverse for numbers less than 3072 bits  */
    icp_qat_fw_maths_modinv_even_l3072_input_t maths_modinv_even_l3072;

    /** Modular multiplicative inverse for numbers less than 4096 bits  */
    icp_qat_fw_maths_modinv_even_l4096_input_t maths_modinv_even_l4096;

    /** Modular multiplicative inverse for numbers up to 8192 bits  */
    icp_qat_fw_maths_modinv_even_l8192_input_t maths_modinv_even_l8192;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_1024_160_input_t mmp_dsa_gen_p_1024_160;

    /** DSA key generation G  */
    icp_qat_fw_mmp_dsa_gen_g_1024_input_t mmp_dsa_gen_g_1024;

    /** DSA key generation Y  */
    icp_qat_fw_mmp_dsa_gen_y_1024_input_t mmp_dsa_gen_y_1024;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_1024_160_input_t mmp_dsa_sign_r_1024_160;

    /** DSA Sign S  */
    icp_qat_fw_mmp_dsa_sign_s_160_input_t mmp_dsa_sign_s_160;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input_t mmp_dsa_sign_r_s_1024_160;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_1024_160_input_t mmp_dsa_verify_1024_160;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_2048_224_input_t mmp_dsa_gen_p_2048_224;

    /** DSA key generation Y  */
    icp_qat_fw_mmp_dsa_gen_y_2048_input_t mmp_dsa_gen_y_2048;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_2048_224_input_t mmp_dsa_sign_r_2048_224;

    /** DSA Sign S  */
    icp_qat_fw_mmp_dsa_sign_s_224_input_t mmp_dsa_sign_s_224;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input_t mmp_dsa_sign_r_s_2048_224;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_2048_224_input_t mmp_dsa_verify_2048_224;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_2048_256_input_t mmp_dsa_gen_p_2048_256;

    /** DSA key generation G  */
    icp_qat_fw_mmp_dsa_gen_g_2048_input_t mmp_dsa_gen_g_2048;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_2048_256_input_t mmp_dsa_sign_r_2048_256;

    /** DSA Sign S  */
    icp_qat_fw_mmp_dsa_sign_s_256_input_t mmp_dsa_sign_s_256;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input_t mmp_dsa_sign_r_s_2048_256;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_2048_256_input_t mmp_dsa_verify_2048_256;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_3072_256_input_t mmp_dsa_gen_p_3072_256;

    /** DSA key generation G  */
    icp_qat_fw_mmp_dsa_gen_g_3072_input_t mmp_dsa_gen_g_3072;

    /** DSA key generation Y  */
    icp_qat_fw_mmp_dsa_gen_y_3072_input_t mmp_dsa_gen_y_3072;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_3072_256_input_t mmp_dsa_sign_r_3072_256;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input_t mmp_dsa_sign_r_s_3072_256;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_3072_256_input_t mmp_dsa_verify_3072_256;

    /** ECDSA Sign RS for curves B/K-163 and B/K-233  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_input_t mmp_ecdsa_sign_rs_gf2_l256;

    /** ECDSA Sign R for curves B/K-163 and B/K-233  */
    icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input_t mmp_ecdsa_sign_r_gf2_l256;

    /** ECDSA Sign S for curves with n &lt; 2^256  */
    icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input_t mmp_ecdsa_sign_s_gf2_l256;

    /** ECDSA Verify for curves B/K-163 and B/K-233  */
    icp_qat_fw_mmp_ecdsa_verify_gf2_l256_input_t mmp_ecdsa_verify_gf2_l256;

    /** ECDSA Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_input_t mmp_ecdsa_sign_rs_gf2_l512;

    /** ECDSA GF2 Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input_t mmp_ecdsa_sign_r_gf2_l512;

    /** ECDSA GF2 Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input_t mmp_ecdsa_sign_s_gf2_l512;

    /** ECDSA GF2 Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gf2_l512_input_t mmp_ecdsa_verify_gf2_l512;

    /** ECDSA GF2 Sign RS for curves B-571/K-571  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_input_t mmp_ecdsa_sign_rs_gf2_571;

    /** ECDSA GF2 Sign S for curves with deg(q) &lt; 576  */
    icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input_t mmp_ecdsa_sign_s_gf2_571;

    /** ECDSA GF2 Sign R for degree 571  */
    icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input_t mmp_ecdsa_sign_r_gf2_571;

    /** ECDSA GF2 Verify for degree 571  */
    icp_qat_fw_mmp_ecdsa_verify_gf2_571_input_t mmp_ecdsa_verify_gf2_571;

    /** MATHS GF2 Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gf2_l256_input_t maths_point_multiplication_gf2_l256;

    /** MATHS GF2 Point Verification  */
    icp_qat_fw_maths_point_verify_gf2_l256_input_t maths_point_verify_gf2_l256;

    /** MATHS GF2 Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gf2_l512_input_t maths_point_multiplication_gf2_l512;

    /** MATHS GF2 Point Verification  */
    icp_qat_fw_maths_point_verify_gf2_l512_input_t maths_point_verify_gf2_l512;

    /** ECC GF2 Point Multiplication for curves B-571/K-571  */
    icp_qat_fw_maths_point_multiplication_gf2_571_input_t maths_point_multiplication_gf2_571;

    /** ECC GF2 Point Verification for degree 571  */
    icp_qat_fw_maths_point_verify_gf2_571_input_t maths_point_verify_gf2_571;

    /** ECDSA GFP Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input_t mmp_ecdsa_sign_r_gfp_l256;

    /** ECDSA GFP Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input_t mmp_ecdsa_sign_s_gfp_l256;

    /** ECDSA GFP Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_input_t mmp_ecdsa_sign_rs_gfp_l256;

    /** ECDSA GFP Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gfp_l256_input_t mmp_ecdsa_verify_gfp_l256;

    /** ECDSA GFP Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input_t mmp_ecdsa_sign_r_gfp_l512;

    /** ECDSA GFP Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input_t mmp_ecdsa_sign_s_gfp_l512;

    /** ECDSA GFP Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_input_t mmp_ecdsa_sign_rs_gfp_l512;

    /** ECDSA GFP Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gfp_l512_input_t mmp_ecdsa_verify_gfp_l512;

    /** ECDSA GFP Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input_t mmp_ecdsa_sign_r_gfp_521;

    /** ECDSA GFP Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input_t mmp_ecdsa_sign_s_gfp_521;

    /** ECDSA GFP Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_input_t mmp_ecdsa_sign_rs_gfp_521;

    /** ECDSA GFP Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gfp_521_input_t mmp_ecdsa_verify_gfp_521;

    /** ECC GFP Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gfp_l256_input_t maths_point_multiplication_gfp_l256;

    /** ECC GFP Partial Point Verification  */
    icp_qat_fw_maths_point_verify_gfp_l256_input_t maths_point_verify_gfp_l256;

    /** ECC GFP Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gfp_l512_input_t maths_point_multiplication_gfp_l512;

    /** ECC GFP Partial Point  */
    icp_qat_fw_maths_point_verify_gfp_l512_input_t maths_point_verify_gfp_l512;

    /** ECC GFP Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gfp_521_input_t maths_point_multiplication_gfp_521;

    /** ECC GFP Partial Point Verification  */
    icp_qat_fw_maths_point_verify_gfp_521_input_t maths_point_verify_gfp_521;

    /** ECC curve25519 Variable Point Multiplication [k]P(x), as specified in RFC7748  */
    icp_qat_fw_point_multiplication_c25519_input_t point_multiplication_c25519;

    /** ECC curve25519 Generator Point Multiplication [k]G(x), as specified in RFC7748  */
    icp_qat_fw_generator_multiplication_c25519_input_t generator_multiplication_c25519;

    /** ECC edwards25519 Variable Point Multiplication [k]P, as specified in RFC8032  */
    icp_qat_fw_point_multiplication_ed25519_input_t point_multiplication_ed25519;

    /** ECC edwards25519 Generator Point Multiplication [k]G, as specified in RFC8032  */
    icp_qat_fw_generator_multiplication_ed25519_input_t generator_multiplication_ed25519;

    /** ECC curve448 Variable Point Multiplication [k]P(x), as specified in RFC7748  */
    icp_qat_fw_point_multiplication_c448_input_t point_multiplication_c448;

    /** ECC curve448 Generator Point Multiplication [k]G(x), as specified in RFC7748  */
    icp_qat_fw_generator_multiplication_c448_input_t generator_multiplication_c448;

    /** ECC edwards448 Variable Point Multiplication [k]P, as specified in RFC8032  */
    icp_qat_fw_point_multiplication_ed448_input_t point_multiplication_ed448;

    /** ECC edwards448 Generator Point Multiplication [k]P, as specified in RFC8032  */
    icp_qat_fw_generator_multiplication_ed448_input_t generator_multiplication_ed448;

    /** ECC P521 ECDSA Sign RS  */
    icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p521_input_t mmp_kpt_ecdsa_sign_rs_p521;

    /** ECC P384 ECDSA Sign RS  */
    icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p384_input_t mmp_kpt_ecdsa_sign_rs_p384;

    /** ECC KPT P256 ECDSA Sign RS  */
    icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p256_input_t mmp_kpt_ecdsa_sign_rs_p256;

    /** KPT RSA 512 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_512_input_t mmp_kpt_rsa_dp1_512;

    /** KPT RSA 1024 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_1024_input_t mmp_kpt_rsa_dp1_1024;

    /** KPT RSA 1536 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_1536_input_t mmp_kpt_rsa_dp1_1536;

    /** KPT RSA 2048 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_2048_input_t mmp_kpt_rsa_dp1_2048;

    /** KPT RSA 3072 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_3072_input_t mmp_kpt_rsa_dp1_3072;

    /** KPT RSA 4096 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_4096_input_t mmp_kpt_rsa_dp1_4096;

    /** KPT RSA 8192 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_8192_input_t mmp_kpt_rsa_dp1_8192;

    /** RSA 512 decryption second form  */
    icp_qat_fw_mmp_kpt_rsa_dp2_512_input_t mmp_kpt_rsa_dp2_512;

    /** RSA 1024 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_1024_input_t mmp_kpt_rsa_dp2_1024;

    /** KPT RSA 1536 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_1536_input_t mmp_kpt_rsa_dp2_1536;

    /** RSA 2048 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_2048_input_t mmp_kpt_rsa_dp2_2048;

    /**  */
    icp_qat_fw_mmp_kpt_rsa_dp2_3072_input_t mmp_kpt_rsa_dp2_3072;

    /** RSA 4096 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_4096_input_t mmp_kpt_rsa_dp2_4096;

    /** RSA 8192 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_8192_input_t mmp_kpt_rsa_dp2_8192;

} icp_qat_fw_mmp_input_param_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P384 Variable Point Multiplication [k]P ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_EC_POINT_MULTIPLICATION_P384.
 */
typedef struct icp_qat_fw_mmp_ec_point_multiplication_p384_output_s
{
    uint64_t xr; /**< xR = affine coordinate X of point [k]P  (6 qwords)*/
    uint64_t yr; /**< yR = affine coordinate Y of point [k]P  (6 qwords)*/
} icp_qat_fw_mmp_ec_point_multiplication_p384_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P384 Generator Point Multiplication [k]G ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_EC_GENERATOR_MULTIPLICATION_P384.
 */
typedef struct icp_qat_fw_mmp_ec_generator_multiplication_p384_output_s
{
    uint64_t xr; /**< xR = affine coordinate X of point [k]G  (6 qwords)*/
    uint64_t yr; /**< yR = affine coordinate Y of point [k]G  (6 qwords)*/
} icp_qat_fw_mmp_ec_generator_multiplication_p384_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P384 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_ECDSA_SIGN_RS_P384.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_p384_output_s
{
    uint64_t r; /**< ECDSA signature r  (6 qwords)*/
    uint64_t s; /**< ECDSA signature s  (6 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_p384_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P256 Variable Point Multiplication [k]P ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_EC_POINT_MULTIPLICATION_P256.
 */
typedef struct icp_qat_fw_mmp_ec_point_multiplication_p256_output_s
{
    uint64_t xr; /**< xR = affine coordinate X of point [k]P  (4 qwords)*/
    uint64_t yr; /**< yR = affine coordinate Y of point [k]P  (4 qwords)*/
} icp_qat_fw_mmp_ec_point_multiplication_p256_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P256 Generator Point Multiplication [k]G ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_EC_GENERATOR_MULTIPLICATION_P256.
 */
typedef struct icp_qat_fw_mmp_ec_generator_multiplication_p256_output_s
{
    uint64_t xr; /**< xR = affine coordinate X of point [k]G  (4 qwords)*/
    uint64_t yr; /**< yR = affine coordinate Y of point [k]G  (4 qwords)*/
} icp_qat_fw_mmp_ec_generator_multiplication_p256_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P256 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_ECDSA_SIGN_RS_P256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_p256_output_s
{
    uint64_t r; /**< ECDSA signature r  (4 qwords)*/
    uint64_t s; /**< ECDSA signature s  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_p256_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC SM2 point multiply [k]G ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_ECSM2_GENERATOR_MULTIPLICATION.
 */
typedef struct icp_qat_fw_mmp_ecsm2_generator_multiplication_output_s
{
    uint64_t xd; /**< xD = affine coordinate X of point [k]G  (4 qwords)*/
    uint64_t yd; /**< yD = affine coordinate Y of point [k]G  (4 qwords)*/
} icp_qat_fw_mmp_ecsm2_generator_multiplication_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Initialisation sequence ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_INIT.
 */
typedef struct icp_qat_fw_mmp_init_output_s
{
    uint64_t zz; /**< 1'd quadword (1 qwords)*/
} icp_qat_fw_mmp_init_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation base 2 for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_G2_768.
 */
typedef struct icp_qat_fw_mmp_dh_g2_768_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (12 qwords)*/
} icp_qat_fw_mmp_dh_g2_768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_768.
 */
typedef struct icp_qat_fw_mmp_dh_768_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (12 qwords)*/
} icp_qat_fw_mmp_dh_768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation base 2 for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_G2_1024.
 */
typedef struct icp_qat_fw_mmp_dh_g2_1024_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (16 qwords)*/
} icp_qat_fw_mmp_dh_g2_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_1024.
 */
typedef struct icp_qat_fw_mmp_dh_1024_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (16 qwords)*/
} icp_qat_fw_mmp_dh_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation base 2 for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_G2_1536.
 */
typedef struct icp_qat_fw_mmp_dh_g2_1536_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (24 qwords)*/
} icp_qat_fw_mmp_dh_g2_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_1536.
 */
typedef struct icp_qat_fw_mmp_dh_1536_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (24 qwords)*/
} icp_qat_fw_mmp_dh_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation base 2 for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_G2_2048.
 */
typedef struct icp_qat_fw_mmp_dh_g2_2048_output_s
{
    uint64_t r; /**< modular exponentiation result   &ge; 0 and &lt; m (32 qwords)*/
} icp_qat_fw_mmp_dh_g2_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_2048.
 */
typedef struct icp_qat_fw_mmp_dh_2048_output_s
{
    uint64_t r; /**< modular exponentiation result   &ge; 0 and &lt; m (32 qwords)*/
} icp_qat_fw_mmp_dh_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation base 2 for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_G2_3072.
 */
typedef struct icp_qat_fw_mmp_dh_g2_3072_output_s
{
    uint64_t r; /**< modular exponentiation result   &ge; 0 and &lt; m (48 qwords)*/
} icp_qat_fw_mmp_dh_g2_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_3072.
 */
typedef struct icp_qat_fw_mmp_dh_3072_output_s
{
    uint64_t r; /**< modular exponentiation result   &ge; 0 and &lt; m (48 qwords)*/
} icp_qat_fw_mmp_dh_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation base 2 for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_G2_4096.
 */
typedef struct icp_qat_fw_mmp_dh_g2_4096_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (64 qwords)*/
} icp_qat_fw_mmp_dh_g2_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DH_4096.
 */
typedef struct icp_qat_fw_mmp_dh_4096_output_s
{
    uint64_t r; /**< modular exponentiation result   &ge; 0 and &lt; m (64 qwords)*/
} icp_qat_fw_mmp_dh_4096_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation base 2 for
 * 8192-bit numbers , to be used when icp_qat_fw_pke_response_s::functionalityId
 * is #PKE_DH_G2_8192.
 */
typedef struct icp_qat_fw_mmp_dh_g2_8192_output_s
{
    uint64_t
        r; /**< modular exponentiation result  &ge; 0 and &lt; m (128 qwords)*/
} icp_qat_fw_mmp_dh_g2_8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Diffie-Hellman Modular exponentiation for
 * 8192-bit numbers , to be used when icp_qat_fw_pke_response_s::functionalityId
 * is #PKE_DH_8192.
 */
typedef struct icp_qat_fw_mmp_dh_8192_output_s
{
    uint64_t
        r; /**< modular exponentiation result   &ge; 0 and &lt; m (128 qwords)*/
} icp_qat_fw_mmp_dh_8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 512 key generation first form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP1_512.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_512_output_s
{
    uint64_t n; /**< RSA key (8 qwords)*/
    uint64_t d; /**< RSA private key (first form) (8 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 512 key generation second form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP2_512.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_512_output_s
{
    uint64_t n; /**< RSA key (8 qwords)*/
    uint64_t d; /**< RSA private key (second form) (8 qwords)*/
    uint64_t dp; /**< RSA private key (second form) (4 qwords)*/
    uint64_t dq; /**< RSA private key (second form) (4 qwords)*/
    uint64_t qinv; /**< RSA private key (second form) (4 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 512 Encryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_EP_512.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_512_output_s
{
    uint64_t c; /**< cipher text representative, &lt; n (8 qwords)*/
} icp_qat_fw_mmp_rsa_ep_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 512 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP1_512.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_512_output_s
{
    uint64_t m; /**< message representative, &lt; n (8 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1024 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP2_512.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_512_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (8 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1024 key generation first form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP1_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_1024_output_s
{
    uint64_t n; /**< RSA key (16 qwords)*/
    uint64_t d; /**< RSA private key (first form) (16 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1024 key generation second form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP2_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_1024_output_s
{
    uint64_t n; /**< RSA key (16 qwords)*/
    uint64_t d; /**< RSA private key (second form) (16 qwords)*/
    uint64_t dp; /**< RSA private key (second form) (8 qwords)*/
    uint64_t dq; /**< RSA private key (second form) (8 qwords)*/
    uint64_t qinv; /**< RSA private key (second form) (8 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1024 Encryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_EP_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_1024_output_s
{
    uint64_t c; /**< cipher text representative, &lt; n (16 qwords)*/
} icp_qat_fw_mmp_rsa_ep_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1024 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP1_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_1024_output_s
{
    uint64_t m; /**< message representative, &lt; n (16 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1024 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP2_1024.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_1024_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (16 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1536 key generation first form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP1_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_1536_output_s
{
    uint64_t n; /**< RSA key (24 qwords)*/
    uint64_t d; /**< RSA private key (24 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1536 key generation second form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP2_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_1536_output_s
{
    uint64_t n; /**< RSA key (24 qwords)*/
    uint64_t d; /**< RSA private key (24 qwords)*/
    uint64_t dp; /**< RSA private key (12 qwords)*/
    uint64_t dq; /**< RSA private key (12 qwords)*/
    uint64_t qinv; /**< RSA private key (12 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1536 Encryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_EP_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_1536_output_s
{
    uint64_t c; /**< cipher text representative, &lt; n (24 qwords)*/
} icp_qat_fw_mmp_rsa_ep_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1536 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP1_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_1536_output_s
{
    uint64_t m; /**< message representative, &lt; n (24 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1536 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP2_1536.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_1536_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (24 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 2048 key generation first form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP1_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_2048_output_s
{
    uint64_t n; /**< RSA key (32 qwords)*/
    uint64_t d; /**< RSA private key (32 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 2048 key generation second form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP2_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_2048_output_s
{
    uint64_t n; /**< RSA key (32 qwords)*/
    uint64_t d; /**< RSA private key (32 qwords)*/
    uint64_t dp; /**< RSA private key (16 qwords)*/
    uint64_t dq; /**< RSA private key (16 qwords)*/
    uint64_t qinv; /**< RSA private key (16 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 2048 Encryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_EP_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_2048_output_s
{
    uint64_t c; /**< cipher text representative, &lt; n (32 qwords)*/
} icp_qat_fw_mmp_rsa_ep_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 2048 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP1_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_2048_output_s
{
    uint64_t m; /**< message representative, &lt; n (32 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 2048 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP2_2048.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_2048_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (32 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 3072 key generation first form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP1_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_3072_output_s
{
    uint64_t n; /**< RSA key (48 qwords)*/
    uint64_t d; /**< RSA private key (48 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 3072 key generation second form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP2_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_3072_output_s
{
    uint64_t n; /**< RSA key (48 qwords)*/
    uint64_t d; /**< RSA private key (48 qwords)*/
    uint64_t dp; /**< RSA private key (24 qwords)*/
    uint64_t dq; /**< RSA private key (24 qwords)*/
    uint64_t qinv; /**< RSA private key (24 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 3072 Encryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_EP_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_3072_output_s
{
    uint64_t c; /**< cipher text representative, &lt; n (48 qwords)*/
} icp_qat_fw_mmp_rsa_ep_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 3072 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP1_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_3072_output_s
{
    uint64_t m; /**< message representative, &lt; n (48 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 3072 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP2_3072.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_3072_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (48 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 4096 key generation first form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP1_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_kp1_4096_output_s
{
    uint64_t n; /**< RSA key (64 qwords)*/
    uint64_t d; /**< RSA private key (64 qwords)*/
} icp_qat_fw_mmp_rsa_kp1_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 4096 key generation second form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_KP2_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_kp2_4096_output_s
{
    uint64_t n; /**< RSA key (64 qwords)*/
    uint64_t d; /**< RSA private key (64 qwords)*/
    uint64_t dp; /**< RSA private key (32 qwords)*/
    uint64_t dq; /**< RSA private key (32 qwords)*/
    uint64_t qinv; /**< RSA private key (32 qwords)*/
} icp_qat_fw_mmp_rsa_kp2_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 4096 Encryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_EP_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_4096_output_s
{
    uint64_t c; /**< cipher text representative, &lt; n (64 qwords)*/
} icp_qat_fw_mmp_rsa_ep_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 4096 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP1_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_4096_output_s
{
    uint64_t m; /**< message representative, &lt; n (64 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 4096 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_RSA_DP2_4096.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_4096_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (64 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_4096_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 8192 Encryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_RSA_EP_8192.
 */
typedef struct icp_qat_fw_mmp_rsa_ep_8192_output_s
{
    uint64_t c; /**< cipher text representative, &lt; n (128 qwords)*/
} icp_qat_fw_mmp_rsa_ep_8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 8192 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_RSA_DP1_8192.
 */
typedef struct icp_qat_fw_mmp_rsa_dp1_8192_output_s
{
    uint64_t m; /**< message representative, &lt; n (128 qwords)*/
} icp_qat_fw_mmp_rsa_dp1_8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 8192 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_RSA_DP2_8192.
 */
typedef struct icp_qat_fw_mmp_rsa_dp2_8192_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (128 qwords)*/
} icp_qat_fw_mmp_rsa_dp2_8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 192-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_192.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_192_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_192_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 256-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_256.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_256_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 384-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_384.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_384_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_384_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_512.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_768.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_768_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_1024.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_1024_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_1536.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_1536_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_2048.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_2048_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_3072.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_3072_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for GCD primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_GCD_PT_4096.
 */
typedef struct icp_qat_fw_mmp_gcd_pt_4096_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_gcd_pt_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 160-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_160.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_160_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_512.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for &lte; 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_L512.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_l512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_768.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_768_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_1024.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_1024_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_1536.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_1536_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_2048.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_2048_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_3072.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_3072_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Fermat primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_FERMAT_PT_4096.
 */
typedef struct icp_qat_fw_mmp_fermat_pt_4096_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_fermat_pt_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 160-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_160.
 */
typedef struct icp_qat_fw_mmp_mr_pt_160_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_512.
 */
typedef struct icp_qat_fw_mmp_mr_pt_512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_768.
 */
typedef struct icp_qat_fw_mmp_mr_pt_768_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_1024.
 */
typedef struct icp_qat_fw_mmp_mr_pt_1024_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_1536.
 */
typedef struct icp_qat_fw_mmp_mr_pt_1536_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_2048.
 */
typedef struct icp_qat_fw_mmp_mr_pt_2048_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_3072.
 */
typedef struct icp_qat_fw_mmp_mr_pt_3072_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_4096.
 */
typedef struct icp_qat_fw_mmp_mr_pt_4096_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Miller-Rabin primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_MR_PT_L512.
 */
typedef struct icp_qat_fw_mmp_mr_pt_l512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_mr_pt_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 160-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_160.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_160_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 512-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_512.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 768-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_768.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_768_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 1024-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_1024.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_1024_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 1536-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_1536.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_1536_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 2048-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_2048.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_2048_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 3072-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_3072.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_3072_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for 4096-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_4096.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_4096_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_4096_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Lucas primality test for L512-bit numbers ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_LUCAS_PT_L512.
 */
typedef struct icp_qat_fw_mmp_lucas_pt_l512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_lucas_pt_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 512-bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L512.
 */
typedef struct icp_qat_fw_maths_modexp_l512_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (8 qwords)*/
} icp_qat_fw_maths_modexp_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 1024-bit ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L1024.
 */
typedef struct icp_qat_fw_maths_modexp_l1024_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (16 qwords)*/
} icp_qat_fw_maths_modexp_l1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 1536-bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L1536.
 */
typedef struct icp_qat_fw_maths_modexp_l1536_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (24 qwords)*/
} icp_qat_fw_maths_modexp_l1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 2048-bit ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L2048.
 */
typedef struct icp_qat_fw_maths_modexp_l2048_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (32 qwords)*/
} icp_qat_fw_maths_modexp_l2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 2560-bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L2560.
 */
typedef struct icp_qat_fw_maths_modexp_l2560_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (40 qwords)*/
} icp_qat_fw_maths_modexp_l2560_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 3072-bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L3072.
 */
typedef struct icp_qat_fw_maths_modexp_l3072_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (48 qwords)*/
} icp_qat_fw_maths_modexp_l3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 3584-bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L3584.
 */
typedef struct icp_qat_fw_maths_modexp_l3584_output_s
{
    uint64_t r; /**< modular exponentiation result  &ge; 0 and &lt; m (56 qwords)*/
} icp_qat_fw_maths_modexp_l3584_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers less than 4096-bit ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODEXP_L4096.
 */
typedef struct icp_qat_fw_maths_modexp_l4096_output_s
{
    uint64_t r; /**< modular exponentiation result   &ge; 0 and &lt; m (64 qwords)*/
} icp_qat_fw_maths_modexp_l4096_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular exponentiation for numbers up to 8192
 * bits , to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #MATHS_MODEXP_L8192.
 */
typedef struct icp_qat_fw_maths_modexp_l8192_output_s
{
    uint64_t
        r; /**< modular exponentiation result   &ge; 0 and &lt; m (128 qwords)*/
} icp_qat_fw_maths_modexp_l8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less
 * than 128 bits , to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #MATHS_MODINV_ODD_L128.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l128_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (2 qwords)*/
} icp_qat_fw_maths_modinv_odd_l128_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 192 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L192.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l192_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (3 qwords)*/
} icp_qat_fw_maths_modinv_odd_l192_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 256 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L256.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l256_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (4 qwords)*/
} icp_qat_fw_maths_modinv_odd_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 384 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L384.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l384_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (6 qwords)*/
} icp_qat_fw_maths_modinv_odd_l384_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 512 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L512.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l512_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (8 qwords)*/
} icp_qat_fw_maths_modinv_odd_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 768 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L768.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l768_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (12 qwords)*/
} icp_qat_fw_maths_modinv_odd_l768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 1024 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L1024.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l1024_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (16 qwords)*/
} icp_qat_fw_maths_modinv_odd_l1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 1536 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L1536.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l1536_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (24 qwords)*/
} icp_qat_fw_maths_modinv_odd_l1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 2048 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L2048.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l2048_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (32 qwords)*/
} icp_qat_fw_maths_modinv_odd_l2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 3072 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L3072.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l3072_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (48 qwords)*/
} icp_qat_fw_maths_modinv_odd_l3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 4096 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_ODD_L4096.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l4096_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (64 qwords)*/
} icp_qat_fw_maths_modinv_odd_l4096_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers up to
 * 8192 bits , to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #MATHS_MODINV_ODD_L8192.
 */
typedef struct icp_qat_fw_maths_modinv_odd_l8192_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (128
                   qwords)*/
} icp_qat_fw_maths_modinv_odd_l8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less
 * than 128 bits , to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #MATHS_MODINV_EVEN_L128.
 */
typedef struct icp_qat_fw_maths_modinv_even_l128_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (2 qwords)*/
} icp_qat_fw_maths_modinv_even_l128_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 192 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L192.
 */
typedef struct icp_qat_fw_maths_modinv_even_l192_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (3 qwords)*/
} icp_qat_fw_maths_modinv_even_l192_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 256 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L256.
 */
typedef struct icp_qat_fw_maths_modinv_even_l256_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (4 qwords)*/
} icp_qat_fw_maths_modinv_even_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 384 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L384.
 */
typedef struct icp_qat_fw_maths_modinv_even_l384_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (6 qwords)*/
} icp_qat_fw_maths_modinv_even_l384_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 512 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L512.
 */
typedef struct icp_qat_fw_maths_modinv_even_l512_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (8 qwords)*/
} icp_qat_fw_maths_modinv_even_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 768 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L768.
 */
typedef struct icp_qat_fw_maths_modinv_even_l768_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (12 qwords)*/
} icp_qat_fw_maths_modinv_even_l768_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 1024 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L1024.
 */
typedef struct icp_qat_fw_maths_modinv_even_l1024_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (16 qwords)*/
} icp_qat_fw_maths_modinv_even_l1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 1536 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L1536.
 */
typedef struct icp_qat_fw_maths_modinv_even_l1536_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (24 qwords)*/
} icp_qat_fw_maths_modinv_even_l1536_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 2048 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L2048.
 */
typedef struct icp_qat_fw_maths_modinv_even_l2048_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (32 qwords)*/
} icp_qat_fw_maths_modinv_even_l2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 3072 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L3072.
 */
typedef struct icp_qat_fw_maths_modinv_even_l3072_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (48 qwords)*/
} icp_qat_fw_maths_modinv_even_l3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers less than 4096 bits ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_MODINV_EVEN_L4096.
 */
typedef struct icp_qat_fw_maths_modinv_even_l4096_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (64 qwords)*/
} icp_qat_fw_maths_modinv_even_l4096_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for Modular multiplicative inverse for numbers up to
 * 8192 bits , to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #MATHS_MODINV_EVEN_L8192.
 */
typedef struct icp_qat_fw_maths_modinv_even_l8192_output_s
{
    uint64_t c; /**< modular multiplicative inverse of a, &gt; 0 and &lt; b (128
                   qwords)*/
} icp_qat_fw_maths_modinv_even_l8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_P_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_1024_160_output_s
{
    uint64_t p; /**< candidate for DSA parameter p  (16 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_1024_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA key generation G ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_G_1024.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_g_1024_output_s
{
    uint64_t g; /**< DSA parameter  (16 qwords)*/
} icp_qat_fw_mmp_dsa_gen_g_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA key generation Y ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_Y_1024.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_y_1024_output_s
{
    uint64_t y; /**< DSA parameter (16 qwords)*/
} icp_qat_fw_mmp_dsa_gen_y_1024_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_1024_160_output_s
{
    uint64_t r; /**< DSA 160-bits signature  (3 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_1024_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_S_160.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_s_160_output_s
{
    uint64_t s; /**< s DSA 160-bits signature  (3 qwords)*/
} icp_qat_fw_mmp_dsa_sign_s_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_S_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_1024_160_output_s
{
    uint64_t r; /**< DSA 160-bits signature  (3 qwords)*/
    uint64_t s; /**< DSA 160-bits signature  (3 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_1024_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_VERIFY_1024_160.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_1024_160_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_dsa_verify_1024_160_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_P_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_2048_224_output_s
{
    uint64_t p; /**< candidate for DSA parameter p  (32 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_2048_224_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA key generation Y ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_Y_2048.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_y_2048_output_s
{
    uint64_t y; /**< DSA parameter (32 qwords)*/
} icp_qat_fw_mmp_dsa_gen_y_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_2048_224_output_s
{
    uint64_t r; /**< DSA 224-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_2048_224_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_S_224.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_s_224_output_s
{
    uint64_t s; /**< s DSA 224-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_s_224_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_S_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_2048_224_output_s
{
    uint64_t r; /**< DSA 224-bits signature  (4 qwords)*/
    uint64_t s; /**< DSA 224-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_2048_224_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_VERIFY_2048_224.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_2048_224_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_dsa_verify_2048_224_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_P_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_2048_256_output_s
{
    uint64_t p; /**< candidate for DSA parameter p  (32 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_2048_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA key generation G ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_G_2048.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_g_2048_output_s
{
    uint64_t g; /**< DSA parameter  (32 qwords)*/
} icp_qat_fw_mmp_dsa_gen_g_2048_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_2048_256_output_s
{
    uint64_t r; /**< DSA 256-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_2048_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_S_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_s_256_output_s
{
    uint64_t s; /**< s DSA 256-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_s_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_S_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_2048_256_output_s
{
    uint64_t r; /**< DSA 256-bits signature  (4 qwords)*/
    uint64_t s; /**< DSA 256-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_2048_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_VERIFY_2048_256.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_2048_256_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_dsa_verify_2048_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA parameter generation P ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_P_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_p_3072_256_output_s
{
    uint64_t p; /**< candidate for DSA parameter p  (48 qwords)*/
} icp_qat_fw_mmp_dsa_gen_p_3072_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA key generation G ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_G_3072.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_g_3072_output_s
{
    uint64_t g; /**< DSA parameter  (48 qwords)*/
} icp_qat_fw_mmp_dsa_gen_g_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA key generation Y ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_GEN_Y_3072.
 */
typedef struct icp_qat_fw_mmp_dsa_gen_y_3072_output_s
{
    uint64_t y; /**< DSA parameter (48 qwords)*/
} icp_qat_fw_mmp_dsa_gen_y_3072_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_3072_256_output_s
{
    uint64_t r; /**< DSA 256-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_3072_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Sign R S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_SIGN_R_S_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_sign_r_s_3072_256_output_s
{
    uint64_t r; /**< DSA 256-bits signature  (4 qwords)*/
    uint64_t s; /**< DSA 256-bits signature  (4 qwords)*/
} icp_qat_fw_mmp_dsa_sign_r_s_3072_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for DSA Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_DSA_VERIFY_3072_256.
 */
typedef struct icp_qat_fw_mmp_dsa_verify_3072_256_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_dsa_verify_3072_256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA Sign RS for curves B/K-163 and B/K-233 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_RS_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_output_s
{
    uint64_t r; /**< ECDSA signature r &gt; 0 and &lt; n (4 qwords)*/
    uint64_t s; /**< ECDSA signature s &gt; 0 and &lt; n (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA Sign R for curves B/K-163 and B/K-233 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_R_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_output_s
{
    uint64_t r; /**< ECDSA signature r &gt; 0 and &lt; n (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA Sign S for curves with n &lt; 2^256 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_S_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_output_s
{
    uint64_t s; /**< ECDSA signature s &gt; 0 and &lt; n (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA Verify for curves B/K-163 and B/K-233 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_VERIFY_GF2_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gf2_l256_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_ecdsa_verify_gf2_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_RS_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_output_s
{
    uint64_t r; /**<  (8 qwords)*/
    uint64_t s; /**< ECDSA signature r &gt; 0 and &lt; n ECDSA signature s &gt; 0 and &lt; n (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GF2 Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_R_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_output_s
{
    uint64_t r; /**< ECDSA signature r &gt; 0 and &lt; n (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GF2 Sign S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_S_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_output_s
{
    uint64_t s; /**< ECDSA signature s &gt; 0 and &lt; n (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GF2 Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_VERIFY_GF2_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gf2_l512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_ecdsa_verify_gf2_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GF2 Sign RS for curves B-571/K-571 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_RS_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_output_s
{
    uint64_t r; /**<  (9 qwords)*/
    uint64_t s; /**< ECDSA signature r &gt; 0 and &lt; n ECDSA signature s &gt; 0 and &lt; n (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GF2 Sign S for curves with deg(q) &lt; 576 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_S_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_output_s
{
    uint64_t s; /**< ECDSA signature s &gt; 0 and &lt; n (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GF2 Sign R for degree 571 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_R_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_output_s
{
    uint64_t r; /**< ECDSA signature r &gt; 0 and &lt; n (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GF2 Verify for degree 571 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_VERIFY_GF2_571.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gf2_571_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_ecdsa_verify_gf2_571_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for MATHS GF2 Point Multiplication ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GF2_L256.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gf2_l256_output_s
{
    uint64_t xk; /**< x coordinate of resultant point (&lt; degree(q)) (4 qwords)*/
    uint64_t yk; /**< y coordinate of resultant point (&lt; degree(q)) (4 qwords)*/
} icp_qat_fw_maths_point_multiplication_gf2_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for MATHS GF2 Point Verification ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_VERIFY_GF2_L256.
 */
typedef struct icp_qat_fw_maths_point_verify_gf2_l256_output_s
{
    /* no output parameters */
} icp_qat_fw_maths_point_verify_gf2_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for MATHS GF2 Point Multiplication ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GF2_L512.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gf2_l512_output_s
{
    uint64_t xk; /**< x coordinate of resultant point (&lt; q) (8 qwords)*/
    uint64_t yk; /**< y coordinate of resultant point (&lt; q) (8 qwords)*/
} icp_qat_fw_maths_point_multiplication_gf2_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for MATHS GF2 Point Verification ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_VERIFY_GF2_L512.
 */
typedef struct icp_qat_fw_maths_point_verify_gf2_l512_output_s
{
    /* no output parameters */
} icp_qat_fw_maths_point_verify_gf2_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GF2 Point Multiplication for curves B-571/K-571 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GF2_571.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gf2_571_output_s
{
    uint64_t xk; /**< x coordinate of resultant point (degree &lt; degree(q)) (9 qwords)*/
    uint64_t yk; /**< y coordinate of resultant point (degree &lt; degree(q)) (9 qwords)*/
} icp_qat_fw_maths_point_multiplication_gf2_571_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GF2 Point Verification for degree 571 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_VERIFY_GF2_571.
 */
typedef struct icp_qat_fw_maths_point_verify_gf2_571_output_s
{
    /* no output parameters */
} icp_qat_fw_maths_point_verify_gf2_571_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_R_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_output_s
{
    uint64_t r; /**< ECDSA signature  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_S_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_output_s
{
    uint64_t s; /**< ECDSA signature s  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_RS_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_output_s
{
    uint64_t r; /**< ECDSA signature r  (4 qwords)*/
    uint64_t s; /**< ECDSA signature s  (4 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_VERIFY_GFP_L256.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gfp_l256_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_ecdsa_verify_gfp_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_R_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_output_s
{
    uint64_t r; /**< ECDSA signature  (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_S_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_output_s
{
    uint64_t s; /**< ECDSA signature s  (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_RS_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_output_s
{
    uint64_t r; /**< ECDSA signature r  (8 qwords)*/
    uint64_t s; /**< ECDSA signature s  (8 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_VERIFY_GFP_L512.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gfp_l512_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_ecdsa_verify_gfp_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign R ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_R_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_output_s
{
    uint64_t r; /**< ECDSA signature  (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign S ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_S_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_output_s
{
    uint64_t s; /**< ECDSA signature s  (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_SIGN_RS_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_output_s
{
    uint64_t r; /**< ECDSA signature r  (9 qwords)*/
    uint64_t s; /**< ECDSA signature s  (9 qwords)*/
} icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECDSA GFP Verify ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #PKE_ECDSA_VERIFY_GFP_521.
 */
typedef struct icp_qat_fw_mmp_ecdsa_verify_gfp_521_output_s
{
    /* no output parameters */
} icp_qat_fw_mmp_ecdsa_verify_gfp_521_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GFP Point Multiplication ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GFP_L256.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gfp_l256_output_s
{
    uint64_t xk; /**< x coordinate of resultant EC point  (4 qwords)*/
    uint64_t yk; /**< y coordinate of resultant EC point  (4 qwords)*/
} icp_qat_fw_maths_point_multiplication_gfp_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GFP Partial Point Verification ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_VERIFY_GFP_L256.
 */
typedef struct icp_qat_fw_maths_point_verify_gfp_l256_output_s
{
    /* no output parameters */
} icp_qat_fw_maths_point_verify_gfp_l256_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GFP Point Multiplication ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GFP_L512.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gfp_l512_output_s
{
    uint64_t xk; /**< x coordinate of resultant EC point  (8 qwords)*/
    uint64_t yk; /**< y coordinate of resultant EC point  (8 qwords)*/
} icp_qat_fw_maths_point_multiplication_gfp_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GFP Partial Point ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_VERIFY_GFP_L512.
 */
typedef struct icp_qat_fw_maths_point_verify_gfp_l512_output_s
{
    /* no output parameters */
} icp_qat_fw_maths_point_verify_gfp_l512_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GFP Point Multiplication ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_MULTIPLICATION_GFP_521.
 */
typedef struct icp_qat_fw_maths_point_multiplication_gfp_521_output_s
{
    uint64_t xk; /**< x coordinate of resultant EC point  (9 qwords)*/
    uint64_t yk; /**< y coordinate of resultant EC point  (9 qwords)*/
} icp_qat_fw_maths_point_multiplication_gfp_521_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC GFP Partial Point Verification ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #MATHS_POINT_VERIFY_GFP_521.
 */
typedef struct icp_qat_fw_maths_point_verify_gfp_521_output_s
{
    /* no output parameters */
} icp_qat_fw_maths_point_verify_gfp_521_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC curve25519 Variable Point Multiplication [k]P(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #POINT_MULTIPLICATION_C25519.
 */
typedef struct icp_qat_fw_point_multiplication_c25519_output_s
{
    uint64_t xr; /**< xR = Montgomery affine coordinate X of point [k]P  (4 qwords)*/
} icp_qat_fw_point_multiplication_c25519_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC curve25519 Generator Point Multiplication [k]G(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #GENERATOR_MULTIPLICATION_C25519.
 */
typedef struct icp_qat_fw_generator_multiplication_c25519_output_s
{
    uint64_t xr; /**< xR = Montgomery affine coordinate X of point [k]G  (4 qwords)*/
} icp_qat_fw_generator_multiplication_c25519_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC edwards25519 Variable Point Multiplication [k]P, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #POINT_MULTIPLICATION_ED25519.
 */
typedef struct icp_qat_fw_point_multiplication_ed25519_output_s
{
    uint64_t xr; /**< xR = Twisted Edwards affine coordinate X of point [k]P  (4 qwords)*/
    uint64_t yr; /**< yR = Twisted Edwards affine coordinate Y of point [k]P  (4 qwords)*/
} icp_qat_fw_point_multiplication_ed25519_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC edwards25519 Generator Point Multiplication [k]G, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #GENERATOR_MULTIPLICATION_ED25519.
 */
typedef struct icp_qat_fw_generator_multiplication_ed25519_output_s
{
    uint64_t xr; /**< xR = Twisted Edwards affine coordinate X of point [k]G  (4 qwords)*/
    uint64_t yr; /**< yR = Twisted Edwards affine coordinate Y of point [k]G  (4 qwords)*/
} icp_qat_fw_generator_multiplication_ed25519_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC curve448 Variable Point Multiplication [k]P(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #POINT_MULTIPLICATION_C448.
 */
typedef struct icp_qat_fw_point_multiplication_c448_output_s
{
    uint64_t xr; /**< xR = Montgomery affine coordinate X of point [k]P  (8 qwords)*/
} icp_qat_fw_point_multiplication_c448_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC curve448 Generator Point Multiplication [k]G(x), as specified in RFC7748 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #GENERATOR_MULTIPLICATION_C448.
 */
typedef struct icp_qat_fw_generator_multiplication_c448_output_s
{
    uint64_t xr; /**< xR = Montgomery affine coordinate X of point [k]G  (8 qwords)*/
} icp_qat_fw_generator_multiplication_c448_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC edwards448 Variable Point Multiplication [k]P, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #POINT_MULTIPLICATION_ED448.
 */
typedef struct icp_qat_fw_point_multiplication_ed448_output_s
{
    uint64_t xr; /**< xR = Edwards affine coordinate X of point [k]P  (8 qwords)*/
    uint64_t yr; /**< yR = Edwards affine coordinate Y of point [k]P  (8 qwords)*/
} icp_qat_fw_point_multiplication_ed448_output_t;



/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC edwards448 Generator Point Multiplication [k]P, as specified in RFC8032 ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is #GENERATOR_MULTIPLICATION_ED448.
 */
typedef struct icp_qat_fw_generator_multiplication_ed448_output_s
{
    uint64_t xr; /**< xR = Edwards affine coordinate X of point [k]G  (8 qwords)*/
    uint64_t yr; /**< yR = Edwards affine coordinate Y of point [k]G  (8 qwords)*/
} icp_qat_fw_generator_multiplication_ed448_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P521 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_ECDSA_SIGN_RS_P521.
 */
typedef struct icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p521_output_s
{
    uint64_t r; /**< ECDSA signature r  (6 qwords)*/
    uint64_t s; /**< ECDSA signature s  (6 qwords)*/
} icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p521_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC P384 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_ECDSA_SIGN_RS_P384.
 */
typedef struct icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p384_output_s
{
    uint64_t r; /**< ECDSA signature r  (6 qwords)*/
    uint64_t s; /**< ECDSA signature s  (6 qwords)*/
} icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p384_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ECC KPT P256 ECDSA Sign RS ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_ECDSA_SIGN_RS_P256.
 */
typedef struct icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p256_output_s
{
    uint64_t r; /**< ECDSA signature r  (4 qwords)*/
    uint64_t s; /**< ECDSA signature s  (4 qwords)*/
} icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p256_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 512 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP1_512.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_512_output_s
{
    uint64_t m; /**< message representative, &lt; n (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_512_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 1024 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP1_1024.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_1024_output_s
{
    uint64_t m; /**< message representative, &lt; n (16 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_1024_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 1536 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP1_1536.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_1536_output_s
{
    uint64_t m; /**< message representative, &lt; n (24 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_1536_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 2048 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP1_2048.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_2048_output_s
{
    uint64_t m; /**< message representative, &lt; n (32 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_2048_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 3072 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP1_3072.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_3072_output_s
{
    uint64_t m; /**< message representative, &lt; n (48 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_3072_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 4096 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP1_4096.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_4096_output_s
{
    uint64_t m; /**< message representative, &lt; n (64 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_4096_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 8192 Decryption ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP1_8192.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp1_8192_output_s
{
    uint64_t m; /**< message representative, &lt; n (128 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp1_8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 512 decryption second form ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP2_512.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_512_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (8 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_512_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 1024 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP2_1024.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_1024_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (16 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_1024_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for KPT RSA 1536 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP2_1536.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_1536_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (24 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_1536_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 2048 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP2_2048.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_2048_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (32 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_2048_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP2_3072.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_3072_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (48 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_3072_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 4096 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP2_4096.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_4096_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (64 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_4096_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    Output parameter list for RSA 8192 Decryption with CRT ,
 *      to be used when icp_qat_fw_pke_response_s::functionalityId is
 * #PKE_KPT_RSA_DP2_8192.
 */
typedef struct icp_qat_fw_mmp_kpt_rsa_dp2_8192_output_s
{
    uint64_t m; /**< message representative, &lt; (p*q) (128 qwords)*/
} icp_qat_fw_mmp_kpt_rsa_dp2_8192_output_t;

/**
 * @ingroup icp_qat_fw_mmp
 * @brief
 *    MMP output parameters
 */
typedef union icp_qat_fw_mmp_output_param_u
{
    /** Generic parameter structure : All members of this wrapper structure
     * are pointers to large integers.
     */
    uint64_t flat_array[ICP_QAT_FW_PKE_OUTPUT_COUNT_MAX];

    /** ECC P384 Variable Point Multiplication [k]P  */
    icp_qat_fw_mmp_ec_point_multiplication_p384_output_t
        mmp_ec_point_multiplication_p384;

    /** ECC P384 Generator Point Multiplication [k]G  */
    icp_qat_fw_mmp_ec_generator_multiplication_p384_output_t
        mmp_ec_generator_multiplication_p384;

    /** ECC P384 ECDSA Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_p384_output_t mmp_ecdsa_sign_rs_p384;

    /** ECC P256 Variable Point Multiplication [k]P  */
    icp_qat_fw_mmp_ec_point_multiplication_p256_output_t
        mmp_ec_point_multiplication_p256;

    /** ECC P256 Generator Point Multiplication [k]G  */
    icp_qat_fw_mmp_ec_generator_multiplication_p256_output_t
        mmp_ec_generator_multiplication_p256;

    /** ECC P256 ECDSA Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_p256_output_t mmp_ecdsa_sign_rs_p256;

    /** ECC SM2 point multiply [k]G  */
    icp_qat_fw_mmp_ecsm2_generator_multiplication_output_t
        mmp_ecsm2_generator_multiplication;

    /** ECC curve25519 Variable Point Multiplication [k]P(x), as specified in
     * RFC7748  */
    icp_qat_fw_point_multiplication_c25519_output_t point_multiplication_c25519;

    /** ECC curve25519 Generator Point Multiplication [k]G(x), as specified in
     * RFC7748  */
    icp_qat_fw_generator_multiplication_c25519_output_t
        generator_multiplication_c25519;

    /** ECC edwards25519 Variable Point Multiplication [k]P, as specified in
     * RFC8032  */
    icp_qat_fw_point_multiplication_ed25519_output_t
        point_multiplication_ed25519;

    /** ECC edwards25519 Generator Point Multiplication [k]G, as specified in
     * RFC8032  */
    icp_qat_fw_generator_multiplication_ed25519_output_t
        generator_multiplication_ed25519;

    /** ECC curve448 Variable Point Multiplication [k]P(x), as specified in
     * RFC7748  */
    icp_qat_fw_point_multiplication_c448_output_t point_multiplication_c448;

    /** ECC curve448 Generator Point Multiplication [k]G(x), as specified in
     * RFC7748  */
    icp_qat_fw_generator_multiplication_c448_output_t
        generator_multiplication_c448;

    /** ECC edwards448 Variable Point Multiplication [k]P, as specified in
     * RFC8032  */
    icp_qat_fw_point_multiplication_ed448_output_t point_multiplication_ed448;

    /** ECC edwards448 Generator Point Multiplication [k]G, as specified in
     * RFC8032  */
    icp_qat_fw_generator_multiplication_ed448_output_t
        generator_multiplication_ed448;

    /** Initialisation sequence  */
    icp_qat_fw_mmp_init_output_t mmp_init;

    /** Diffie-Hellman Modular exponentiation base 2 for 768-bit numbers  */
    icp_qat_fw_mmp_dh_g2_768_output_t mmp_dh_g2_768;

    /** Diffie-Hellman Modular exponentiation for 768-bit numbers  */
    icp_qat_fw_mmp_dh_768_output_t mmp_dh_768;

    /** Diffie-Hellman Modular exponentiation base 2 for 1024-bit numbers  */
    icp_qat_fw_mmp_dh_g2_1024_output_t mmp_dh_g2_1024;

    /** Diffie-Hellman Modular exponentiation for 1024-bit numbers  */
    icp_qat_fw_mmp_dh_1024_output_t mmp_dh_1024;

    /** Diffie-Hellman Modular exponentiation base 2 for 1536-bit numbers  */
    icp_qat_fw_mmp_dh_g2_1536_output_t mmp_dh_g2_1536;

    /** Diffie-Hellman Modular exponentiation for 1536-bit numbers  */
    icp_qat_fw_mmp_dh_1536_output_t mmp_dh_1536;

    /** Diffie-Hellman Modular exponentiation base 2 for 2048-bit numbers  */
    icp_qat_fw_mmp_dh_g2_2048_output_t mmp_dh_g2_2048;

    /** Diffie-Hellman Modular exponentiation for 2048-bit numbers  */
    icp_qat_fw_mmp_dh_2048_output_t mmp_dh_2048;

    /** Diffie-Hellman Modular exponentiation base 2 for 3072-bit numbers  */
    icp_qat_fw_mmp_dh_g2_3072_output_t mmp_dh_g2_3072;

    /** Diffie-Hellman Modular exponentiation for 3072-bit numbers  */
    icp_qat_fw_mmp_dh_3072_output_t mmp_dh_3072;

    /** Diffie-Hellman Modular exponentiation base 2 for 4096-bit numbers  */
    icp_qat_fw_mmp_dh_g2_4096_output_t mmp_dh_g2_4096;

    /** Diffie-Hellman Modular exponentiation for 4096-bit numbers  */
    icp_qat_fw_mmp_dh_4096_output_t mmp_dh_4096;

    /** Diffie-Hellman Modular exponentiation base 2 for 8192-bit numbers  */
    icp_qat_fw_mmp_dh_g2_8192_output_t mmp_dh_g2_8192;

    /** Diffie-Hellman Modular exponentiation for 8192-bit numbers  */
    icp_qat_fw_mmp_dh_8192_output_t mmp_dh_8192;

    /** RSA 512 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_512_output_t mmp_rsa_kp1_512;

    /** RSA 512 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_512_output_t mmp_rsa_kp2_512;

    /** RSA 512 Encryption  */
    icp_qat_fw_mmp_rsa_ep_512_output_t mmp_rsa_ep_512;

    /** RSA 512 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_512_output_t mmp_rsa_dp1_512;

    /** RSA 1024 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_512_output_t mmp_rsa_dp2_512;

    /** RSA 1024 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_1024_output_t mmp_rsa_kp1_1024;

    /** RSA 1024 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_1024_output_t mmp_rsa_kp2_1024;

    /** RSA 1024 Encryption  */
    icp_qat_fw_mmp_rsa_ep_1024_output_t mmp_rsa_ep_1024;

    /** RSA 1024 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_1024_output_t mmp_rsa_dp1_1024;

    /** RSA 1024 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_1024_output_t mmp_rsa_dp2_1024;

    /** RSA 1536 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_1536_output_t mmp_rsa_kp1_1536;

    /** RSA 1536 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_1536_output_t mmp_rsa_kp2_1536;

    /** RSA 1536 Encryption  */
    icp_qat_fw_mmp_rsa_ep_1536_output_t mmp_rsa_ep_1536;

    /** RSA 1536 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_1536_output_t mmp_rsa_dp1_1536;

    /** RSA 1536 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_1536_output_t mmp_rsa_dp2_1536;

    /** RSA 2048 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_2048_output_t mmp_rsa_kp1_2048;

    /** RSA 2048 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_2048_output_t mmp_rsa_kp2_2048;

    /** RSA 2048 Encryption  */
    icp_qat_fw_mmp_rsa_ep_2048_output_t mmp_rsa_ep_2048;

    /** RSA 2048 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_2048_output_t mmp_rsa_dp1_2048;

    /** RSA 2048 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_2048_output_t mmp_rsa_dp2_2048;

    /** RSA 3072 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_3072_output_t mmp_rsa_kp1_3072;

    /** RSA 3072 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_3072_output_t mmp_rsa_kp2_3072;

    /** RSA 3072 Encryption  */
    icp_qat_fw_mmp_rsa_ep_3072_output_t mmp_rsa_ep_3072;

    /** RSA 3072 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_3072_output_t mmp_rsa_dp1_3072;

    /** RSA 3072 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_3072_output_t mmp_rsa_dp2_3072;

    /** RSA 4096 key generation first form  */
    icp_qat_fw_mmp_rsa_kp1_4096_output_t mmp_rsa_kp1_4096;

    /** RSA 4096 key generation second form  */
    icp_qat_fw_mmp_rsa_kp2_4096_output_t mmp_rsa_kp2_4096;

    /** RSA 4096 Encryption  */
    icp_qat_fw_mmp_rsa_ep_4096_output_t mmp_rsa_ep_4096;

    /** RSA 4096 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_4096_output_t mmp_rsa_dp1_4096;

    /** RSA 4096 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_4096_output_t mmp_rsa_dp2_4096;

    /** RSA 8192 Encryption  */
    icp_qat_fw_mmp_rsa_ep_8192_output_t mmp_rsa_ep_8192;

    /** RSA 8192 Decryption  */
    icp_qat_fw_mmp_rsa_dp1_8192_output_t mmp_rsa_dp1_8192;

    /** RSA 8192 Decryption with CRT  */
    icp_qat_fw_mmp_rsa_dp2_8192_output_t mmp_rsa_dp2_8192;

    /** GCD primality test for 192-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_192_output_t mmp_gcd_pt_192;

    /** GCD primality test for 256-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_256_output_t mmp_gcd_pt_256;

    /** GCD primality test for 384-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_384_output_t mmp_gcd_pt_384;

    /** GCD primality test for 512-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_512_output_t mmp_gcd_pt_512;

    /** GCD primality test for 768-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_768_output_t mmp_gcd_pt_768;

    /** GCD primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_1024_output_t mmp_gcd_pt_1024;

    /** GCD primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_1536_output_t mmp_gcd_pt_1536;

    /** GCD primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_2048_output_t mmp_gcd_pt_2048;

    /** GCD primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_3072_output_t mmp_gcd_pt_3072;

    /** GCD primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_gcd_pt_4096_output_t mmp_gcd_pt_4096;

    /** Fermat primality test for 160-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_160_output_t mmp_fermat_pt_160;

    /** Fermat primality test for 512-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_512_output_t mmp_fermat_pt_512;

    /** Fermat primality test for &lte; 512-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_l512_output_t mmp_fermat_pt_l512;

    /** Fermat primality test for 768-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_768_output_t mmp_fermat_pt_768;

    /** Fermat primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_1024_output_t mmp_fermat_pt_1024;

    /** Fermat primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_1536_output_t mmp_fermat_pt_1536;

    /** Fermat primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_2048_output_t mmp_fermat_pt_2048;

    /** Fermat primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_3072_output_t mmp_fermat_pt_3072;

    /** Fermat primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_fermat_pt_4096_output_t mmp_fermat_pt_4096;

    /** Miller-Rabin primality test for 160-bit numbers  */
    icp_qat_fw_mmp_mr_pt_160_output_t mmp_mr_pt_160;

    /** Miller-Rabin primality test for 512-bit numbers  */
    icp_qat_fw_mmp_mr_pt_512_output_t mmp_mr_pt_512;

    /** Miller-Rabin primality test for 768-bit numbers  */
    icp_qat_fw_mmp_mr_pt_768_output_t mmp_mr_pt_768;

    /** Miller-Rabin primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_mr_pt_1024_output_t mmp_mr_pt_1024;

    /** Miller-Rabin primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_mr_pt_1536_output_t mmp_mr_pt_1536;

    /** Miller-Rabin primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_mr_pt_2048_output_t mmp_mr_pt_2048;

    /** Miller-Rabin primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_mr_pt_3072_output_t mmp_mr_pt_3072;

    /** Miller-Rabin primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_mr_pt_4096_output_t mmp_mr_pt_4096;

    /** Miller-Rabin primality test for 512-bit numbers  */
    icp_qat_fw_mmp_mr_pt_l512_output_t mmp_mr_pt_l512;

    /** Lucas primality test for 160-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_160_output_t mmp_lucas_pt_160;

    /** Lucas primality test for 512-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_512_output_t mmp_lucas_pt_512;

    /** Lucas primality test for 768-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_768_output_t mmp_lucas_pt_768;

    /** Lucas primality test for 1024-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_1024_output_t mmp_lucas_pt_1024;

    /** Lucas primality test for 1536-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_1536_output_t mmp_lucas_pt_1536;

    /** Lucas primality test for 2048-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_2048_output_t mmp_lucas_pt_2048;

    /** Lucas primality test for 3072-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_3072_output_t mmp_lucas_pt_3072;

    /** Lucas primality test for 4096-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_4096_output_t mmp_lucas_pt_4096;

    /** Lucas primality test for L512-bit numbers  */
    icp_qat_fw_mmp_lucas_pt_l512_output_t mmp_lucas_pt_l512;

    /** Modular exponentiation for numbers less than 512-bits  */
    icp_qat_fw_maths_modexp_l512_output_t maths_modexp_l512;

    /** Modular exponentiation for numbers less than 1024-bit  */
    icp_qat_fw_maths_modexp_l1024_output_t maths_modexp_l1024;

    /** Modular exponentiation for numbers less than 1536-bits  */
    icp_qat_fw_maths_modexp_l1536_output_t maths_modexp_l1536;

    /** Modular exponentiation for numbers less than 2048-bit  */
    icp_qat_fw_maths_modexp_l2048_output_t maths_modexp_l2048;

    /** Modular exponentiation for numbers less than 2560-bits  */
    icp_qat_fw_maths_modexp_l2560_output_t maths_modexp_l2560;

    /** Modular exponentiation for numbers less than 3072-bits  */
    icp_qat_fw_maths_modexp_l3072_output_t maths_modexp_l3072;

    /** Modular exponentiation for numbers less than 3584-bits  */
    icp_qat_fw_maths_modexp_l3584_output_t maths_modexp_l3584;

    /** Modular exponentiation for numbers less than 4096-bit  */
    icp_qat_fw_maths_modexp_l4096_output_t maths_modexp_l4096;

    /** Modular exponentiation for numbers up to 8192 bits  */
    icp_qat_fw_maths_modexp_l8192_output_t maths_modexp_l8192;

    /** Modular multiplicative inverse for numbers less than 128 bits  */
    icp_qat_fw_maths_modinv_odd_l128_output_t maths_modinv_odd_l128;

    /** Modular multiplicative inverse for numbers less than 192 bits  */
    icp_qat_fw_maths_modinv_odd_l192_output_t maths_modinv_odd_l192;

    /** Modular multiplicative inverse for numbers less than 256 bits  */
    icp_qat_fw_maths_modinv_odd_l256_output_t maths_modinv_odd_l256;

    /** Modular multiplicative inverse for numbers less than 384 bits  */
    icp_qat_fw_maths_modinv_odd_l384_output_t maths_modinv_odd_l384;

    /** Modular multiplicative inverse for numbers less than 512 bits  */
    icp_qat_fw_maths_modinv_odd_l512_output_t maths_modinv_odd_l512;

    /** Modular multiplicative inverse for numbers less than 768 bits  */
    icp_qat_fw_maths_modinv_odd_l768_output_t maths_modinv_odd_l768;

    /** Modular multiplicative inverse for numbers less than 1024 bits  */
    icp_qat_fw_maths_modinv_odd_l1024_output_t maths_modinv_odd_l1024;

    /** Modular multiplicative inverse for numbers less than 1536 bits  */
    icp_qat_fw_maths_modinv_odd_l1536_output_t maths_modinv_odd_l1536;

    /** Modular multiplicative inverse for numbers less than 2048 bits  */
    icp_qat_fw_maths_modinv_odd_l2048_output_t maths_modinv_odd_l2048;

    /** Modular multiplicative inverse for numbers less than 3072 bits  */
    icp_qat_fw_maths_modinv_odd_l3072_output_t maths_modinv_odd_l3072;

    /** Modular multiplicative inverse for numbers less than 4096 bits  */
    icp_qat_fw_maths_modinv_odd_l4096_output_t maths_modinv_odd_l4096;

    /** Modular multiplicative inverse for numbers up to 8192 bits  */
    icp_qat_fw_maths_modinv_odd_l8192_output_t maths_modinv_odd_l8192;

    /** Modular multiplicative inverse for numbers less than 128 bits  */
    icp_qat_fw_maths_modinv_even_l128_output_t maths_modinv_even_l128;

    /** Modular multiplicative inverse for numbers less than 192 bits  */
    icp_qat_fw_maths_modinv_even_l192_output_t maths_modinv_even_l192;

    /** Modular multiplicative inverse for numbers less than 256 bits  */
    icp_qat_fw_maths_modinv_even_l256_output_t maths_modinv_even_l256;

    /** Modular multiplicative inverse for numbers less than 384 bits  */
    icp_qat_fw_maths_modinv_even_l384_output_t maths_modinv_even_l384;

    /** Modular multiplicative inverse for numbers less than 512 bits  */
    icp_qat_fw_maths_modinv_even_l512_output_t maths_modinv_even_l512;

    /** Modular multiplicative inverse for numbers less than 768 bits  */
    icp_qat_fw_maths_modinv_even_l768_output_t maths_modinv_even_l768;

    /** Modular multiplicative inverse for numbers less than 1024 bits  */
    icp_qat_fw_maths_modinv_even_l1024_output_t maths_modinv_even_l1024;

    /** Modular multiplicative inverse for numbers less than 1536 bits  */
    icp_qat_fw_maths_modinv_even_l1536_output_t maths_modinv_even_l1536;

    /** Modular multiplicative inverse for numbers less than 2048 bits  */
    icp_qat_fw_maths_modinv_even_l2048_output_t maths_modinv_even_l2048;

    /** Modular multiplicative inverse for numbers less than 3072 bits  */
    icp_qat_fw_maths_modinv_even_l3072_output_t maths_modinv_even_l3072;

    /** Modular multiplicative inverse for numbers less than 4096 bits  */
    icp_qat_fw_maths_modinv_even_l4096_output_t maths_modinv_even_l4096;

    /** Modular multiplicative inverse for numbers up to 8192 bits  */
    icp_qat_fw_maths_modinv_even_l8192_output_t maths_modinv_even_l8192;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_1024_160_output_t mmp_dsa_gen_p_1024_160;

    /** DSA key generation G  */
    icp_qat_fw_mmp_dsa_gen_g_1024_output_t mmp_dsa_gen_g_1024;

    /** DSA key generation Y  */
    icp_qat_fw_mmp_dsa_gen_y_1024_output_t mmp_dsa_gen_y_1024;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_1024_160_output_t mmp_dsa_sign_r_1024_160;

    /** DSA Sign S  */
    icp_qat_fw_mmp_dsa_sign_s_160_output_t mmp_dsa_sign_s_160;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_1024_160_output_t mmp_dsa_sign_r_s_1024_160;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_1024_160_output_t mmp_dsa_verify_1024_160;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_2048_224_output_t mmp_dsa_gen_p_2048_224;

    /** DSA key generation Y  */
    icp_qat_fw_mmp_dsa_gen_y_2048_output_t mmp_dsa_gen_y_2048;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_2048_224_output_t mmp_dsa_sign_r_2048_224;

    /** DSA Sign S  */
    icp_qat_fw_mmp_dsa_sign_s_224_output_t mmp_dsa_sign_s_224;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_2048_224_output_t mmp_dsa_sign_r_s_2048_224;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_2048_224_output_t mmp_dsa_verify_2048_224;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_2048_256_output_t mmp_dsa_gen_p_2048_256;

    /** DSA key generation G  */
    icp_qat_fw_mmp_dsa_gen_g_2048_output_t mmp_dsa_gen_g_2048;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_2048_256_output_t mmp_dsa_sign_r_2048_256;

    /** DSA Sign S  */
    icp_qat_fw_mmp_dsa_sign_s_256_output_t mmp_dsa_sign_s_256;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_2048_256_output_t mmp_dsa_sign_r_s_2048_256;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_2048_256_output_t mmp_dsa_verify_2048_256;

    /** DSA parameter generation P  */
    icp_qat_fw_mmp_dsa_gen_p_3072_256_output_t mmp_dsa_gen_p_3072_256;

    /** DSA key generation G  */
    icp_qat_fw_mmp_dsa_gen_g_3072_output_t mmp_dsa_gen_g_3072;

    /** DSA key generation Y  */
    icp_qat_fw_mmp_dsa_gen_y_3072_output_t mmp_dsa_gen_y_3072;

    /** DSA Sign R  */
    icp_qat_fw_mmp_dsa_sign_r_3072_256_output_t mmp_dsa_sign_r_3072_256;

    /** DSA Sign R S  */
    icp_qat_fw_mmp_dsa_sign_r_s_3072_256_output_t mmp_dsa_sign_r_s_3072_256;

    /** DSA Verify  */
    icp_qat_fw_mmp_dsa_verify_3072_256_output_t mmp_dsa_verify_3072_256;

    /** ECDSA Sign RS for curves B/K-163 and B/K-233  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_output_t mmp_ecdsa_sign_rs_gf2_l256;

    /** ECDSA Sign R for curves B/K-163 and B/K-233  */
    icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_output_t mmp_ecdsa_sign_r_gf2_l256;

    /** ECDSA Sign S for curves with n &lt; 2^256  */
    icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_output_t mmp_ecdsa_sign_s_gf2_l256;

    /** ECDSA Verify for curves B/K-163 and B/K-233  */
    icp_qat_fw_mmp_ecdsa_verify_gf2_l256_output_t mmp_ecdsa_verify_gf2_l256;

    /** ECDSA Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_output_t mmp_ecdsa_sign_rs_gf2_l512;

    /** ECDSA GF2 Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_output_t mmp_ecdsa_sign_r_gf2_l512;

    /** ECDSA GF2 Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_output_t mmp_ecdsa_sign_s_gf2_l512;

    /** ECDSA GF2 Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gf2_l512_output_t mmp_ecdsa_verify_gf2_l512;

    /** ECDSA GF2 Sign RS for curves B-571/K-571  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_output_t mmp_ecdsa_sign_rs_gf2_571;

    /** ECDSA GF2 Sign S for curves with deg(q) &lt; 576  */
    icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_output_t mmp_ecdsa_sign_s_gf2_571;

    /** ECDSA GF2 Sign R for degree 571  */
    icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_output_t mmp_ecdsa_sign_r_gf2_571;

    /** ECDSA GF2 Verify for degree 571  */
    icp_qat_fw_mmp_ecdsa_verify_gf2_571_output_t mmp_ecdsa_verify_gf2_571;

    /** MATHS GF2 Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gf2_l256_output_t maths_point_multiplication_gf2_l256;

    /** MATHS GF2 Point Verification  */
    icp_qat_fw_maths_point_verify_gf2_l256_output_t maths_point_verify_gf2_l256;

    /** MATHS GF2 Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gf2_l512_output_t maths_point_multiplication_gf2_l512;

    /** MATHS GF2 Point Verification  */
    icp_qat_fw_maths_point_verify_gf2_l512_output_t maths_point_verify_gf2_l512;

    /** ECC GF2 Point Multiplication for curves B-571/K-571  */
    icp_qat_fw_maths_point_multiplication_gf2_571_output_t maths_point_multiplication_gf2_571;

    /** ECC GF2 Point Verification for degree 571  */
    icp_qat_fw_maths_point_verify_gf2_571_output_t maths_point_verify_gf2_571;

    /** ECDSA GFP Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_output_t mmp_ecdsa_sign_r_gfp_l256;

    /** ECDSA GFP Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_output_t mmp_ecdsa_sign_s_gfp_l256;

    /** ECDSA GFP Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_output_t mmp_ecdsa_sign_rs_gfp_l256;

    /** ECDSA GFP Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gfp_l256_output_t mmp_ecdsa_verify_gfp_l256;

    /** ECDSA GFP Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_output_t mmp_ecdsa_sign_r_gfp_l512;

    /** ECDSA GFP Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_output_t mmp_ecdsa_sign_s_gfp_l512;

    /** ECDSA GFP Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_output_t mmp_ecdsa_sign_rs_gfp_l512;

    /** ECDSA GFP Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gfp_l512_output_t mmp_ecdsa_verify_gfp_l512;

    /** ECDSA GFP Sign R  */
    icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_output_t mmp_ecdsa_sign_r_gfp_521;

    /** ECDSA GFP Sign S  */
    icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_output_t mmp_ecdsa_sign_s_gfp_521;

    /** ECDSA GFP Sign RS  */
    icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_output_t mmp_ecdsa_sign_rs_gfp_521;

    /** ECDSA GFP Verify  */
    icp_qat_fw_mmp_ecdsa_verify_gfp_521_output_t mmp_ecdsa_verify_gfp_521;

    /** ECC GFP Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gfp_l256_output_t maths_point_multiplication_gfp_l256;

    /** ECC GFP Partial Point Verification  */
    icp_qat_fw_maths_point_verify_gfp_l256_output_t maths_point_verify_gfp_l256;

    /** ECC GFP Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gfp_l512_output_t maths_point_multiplication_gfp_l512;

    /** ECC GFP Partial Point  */
    icp_qat_fw_maths_point_verify_gfp_l512_output_t maths_point_verify_gfp_l512;

    /** ECC GFP Point Multiplication  */
    icp_qat_fw_maths_point_multiplication_gfp_521_output_t maths_point_multiplication_gfp_521;

    /** ECC GFP Partial Point Verification  */
    icp_qat_fw_maths_point_verify_gfp_521_output_t maths_point_verify_gfp_521;

    /** ECC P521 ECDSA Sign RS  */
    icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p521_output_t mmp_kpt_ecdsa_sign_rs_p521;

    /** ECC P384 ECDSA Sign RS  */
    icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p384_output_t mmp_kpt_ecdsa_sign_rs_p384;

    /** ECC KPT P256 ECDSA Sign RS  */
    icp_qat_fw_mmp_kpt_ecdsa_sign_rs_p256_output_t mmp_kpt_ecdsa_sign_rs_p256;

    /** KPT RSA 512 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_512_output_t mmp_kpt_rsa_dp1_512;

    /** KPT RSA 1024 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_1024_output_t mmp_kpt_rsa_dp1_1024;

    /** KPT RSA 1536 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_1536_output_t mmp_kpt_rsa_dp1_1536;

    /** KPT RSA 2048 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_2048_output_t mmp_kpt_rsa_dp1_2048;

    /** KPT RSA 3072 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_3072_output_t mmp_kpt_rsa_dp1_3072;

    /** KPT RSA 4096 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_4096_output_t mmp_kpt_rsa_dp1_4096;

    /** KPT RSA 8192 Decryption  */
    icp_qat_fw_mmp_kpt_rsa_dp1_8192_output_t mmp_kpt_rsa_dp1_8192;

    /** RSA 512 decryption second form  */
    icp_qat_fw_mmp_kpt_rsa_dp2_512_output_t mmp_kpt_rsa_dp2_512;

    /** RSA 1024 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_1024_output_t mmp_kpt_rsa_dp2_1024;

    /** KPT RSA 1536 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_1536_output_t mmp_kpt_rsa_dp2_1536;

    /** RSA 2048 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_2048_output_t mmp_kpt_rsa_dp2_2048;

    /**  */
    icp_qat_fw_mmp_kpt_rsa_dp2_3072_output_t mmp_kpt_rsa_dp2_3072;

    /** RSA 4096 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_4096_output_t mmp_kpt_rsa_dp2_4096;

    /** RSA 8192 Decryption with CRT  */
    icp_qat_fw_mmp_kpt_rsa_dp2_8192_output_t mmp_kpt_rsa_dp2_8192;

} icp_qat_fw_mmp_output_param_t;



#endif /* __ICP_QAT_FW_MMP__ */


/* --- (Automatically generated (build v. 2.7), do not modify manually) --- */

/* --- end of file --- */
