/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_cy_sym.h
 *
 * @defgroup cpaCySym Symmetric Cipher and Hash Cryptographic API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the Cryptographic API for symmetric cipher,
 *      hash, and combined cipher and hash operations.
 *
 *****************************************************************************/

#ifndef CPA_CY_SYM_H
#define CPA_CY_SYM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Cryptographic component symmetric session context handle.
 * @description
 *      Handle to a cryptographic session context. The memory for this handle
 *      is allocated by the client. The size of the memory that the client needs
 *      to allocate is determined by a call to the @ref
 *      cpaCySymSessionCtxGetSize or @ref cpaCySymSessionCtxGetDynamicSize
 *      functions. The session context memory is initialized with a call to
 *      the @ref cpaCySymInitSession function.
 *      This memory MUST not be freed until a call to @ref
 *      cpaCySymRemoveSession has completed successfully.
 *
 *****************************************************************************/
typedef void * CpaCySymSessionCtx;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Packet type for the cpaCySymPerformOp function
 *
 * @description
 *      Enumeration which is used to indicate to the symmetric cryptographic
 *      perform function on which type of packet the operation is required to
 *      be invoked.  Multi-part cipher and hash operations are useful when
 *      processing needs to be performed on a message which is available to
 *      the client in multiple parts (for example due to network fragmentation
 *      of the packet).
 *
 * @note
 *      There are some restrictions regarding the operations on which
 *      partial packet processing is supported.  For details, see the
 *      function @ref cpaCySymPerformOp.
 *
 * @see
 *      cpaCySymPerformOp()
 *
 *****************************************************************************/
typedef enum _CpaCySymPacketType
{
    CPA_CY_SYM_PACKET_TYPE_FULL = 1,
    /**< Perform an operation on a full packet*/
    CPA_CY_SYM_PACKET_TYPE_PARTIAL,
    /**< Perform a partial operation and maintain the state of the partial
     * operation within the session. This is used for either the first or
     * subsequent packets within a partial packet flow. */
    CPA_CY_SYM_PACKET_TYPE_LAST_PARTIAL
    /**< Complete the last part of a multi-part operation */
} CpaCySymPacketType;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Types of operations supported by the cpaCySymPerformOp function.
 * @description
 *      This enumeration lists different types of operations supported by the
 *      cpaCySymPerformOp function. The operation type is defined during
 *      session registration and cannot be changed for a session once it has
 *      been setup.
 * @see
 *      cpaCySymPerformOp
 *****************************************************************************/
typedef enum _CpaCySymOp
{
    CPA_CY_SYM_OP_NONE=0,
    /**< No operation */
    CPA_CY_SYM_OP_CIPHER,
    /**< Cipher only operation on the data */
    CPA_CY_SYM_OP_HASH,
    /**< Hash only operation on the data */
    CPA_CY_SYM_OP_ALGORITHM_CHAINING
    /**< Chain any cipher with any hash operation. The order depends on
     * the value in the CpaCySymAlgChainOrder enum.
     *
     * This value is also used for authenticated ciphers (GCM and CCM), in
     * which case the cipherAlgorithm should take one of the values @ref
     * CPA_CY_SYM_CIPHER_AES_CCM or @ref CPA_CY_SYM_CIPHER_AES_GCM, while the
     * hashAlgorithm should take the corresponding value @ref
     * CPA_CY_SYM_HASH_AES_CCM or @ref CPA_CY_SYM_HASH_AES_GCM.
     */
} CpaCySymOp;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Cipher algorithms.
 * @description
 *      This enumeration lists supported cipher algorithms and modes.
 *
 *****************************************************************************/
typedef enum _CpaCySymCipherAlgorithm
{
    CPA_CY_SYM_CIPHER_NULL = 1,
    /**< NULL cipher algorithm. No mode applies to the NULL algorithm. */
    CPA_CY_SYM_CIPHER_ARC4,
    /**< (A)RC4 cipher algorithm */
    CPA_CY_SYM_CIPHER_AES_ECB,
    /**< AES algorithm in ECB mode */
    CPA_CY_SYM_CIPHER_AES_CBC,
    /**< AES algorithm in CBC mode */
    CPA_CY_SYM_CIPHER_AES_CTR,
    /**< AES algorithm in Counter mode */
    CPA_CY_SYM_CIPHER_AES_CCM,
    /**< AES algorithm in CCM mode. This authenticated cipher is only supported
     * when the hash mode is also set to CPA_CY_SYM_HASH_MODE_AUTH. When this
     * cipher algorithm is used the CPA_CY_SYM_HASH_AES_CCM element of the
     * CpaCySymHashAlgorithm enum MUST be used to set up the related
     * CpaCySymHashSetupData structure in the session context. */
    CPA_CY_SYM_CIPHER_AES_GCM,
    /**< AES algorithm in GCM mode. This authenticated cipher is only supported
     * when the hash mode is also set to CPA_CY_SYM_HASH_MODE_AUTH. When this
     * cipher algorithm is used the CPA_CY_SYM_HASH_AES_GCM element of the
     * CpaCySymHashAlgorithm enum MUST be used to set up the related
     * CpaCySymHashSetupData structure in the session context. */
    CPA_CY_SYM_CIPHER_DES_ECB,
    /**< DES algorithm in ECB mode */
    CPA_CY_SYM_CIPHER_DES_CBC,
    /**< DES algorithm in CBC mode */
    CPA_CY_SYM_CIPHER_3DES_ECB,
    /**< Triple DES algorithm in ECB mode */
    CPA_CY_SYM_CIPHER_3DES_CBC,
    /**< Triple DES algorithm in CBC mode */
    CPA_CY_SYM_CIPHER_3DES_CTR,
    /**< Triple DES algorithm in CTR mode */
    CPA_CY_SYM_CIPHER_KASUMI_F8,
    /**< Kasumi algorithm in F8 mode */
    CPA_CY_SYM_CIPHER_SNOW3G_UEA2,
    /**< SNOW3G algorithm in UEA2 mode */
    CPA_CY_SYM_CIPHER_AES_F8,
    /**< AES algorithm in F8 mode */
    CPA_CY_SYM_CIPHER_AES_XTS,
    /**< AES algorithm in XTS mode */
    CPA_CY_SYM_CIPHER_ZUC_EEA3,
    /**< ZUC algorithm in EEA3 mode */
    CPA_CY_SYM_CIPHER_CHACHA,
    /**< ChaCha20 Cipher Algorithm. This cipher is only supported for
     * algorithm chaining. When selected, the hash algorithm must be set to
     * CPA_CY_SYM_HASH_POLY and the hash mode must be set to
     * CPA_CY_SYM_HASH_MODE_AUTH. */
    CPA_CY_SYM_CIPHER_SM4_ECB,
    /**< SM4 algorithm in ECB mode This cipher supports 128 bit keys only and
     * does not support partial processing. */
    CPA_CY_SYM_CIPHER_SM4_CBC,
    /**< SM4 algorithm in CBC mode This cipher supports 128 bit keys only and
     * does not support partial processing. */
    CPA_CY_SYM_CIPHER_SM4_CTR
    /**< SM4 algorithm in CTR mode This cipher supports 128 bit keys only and
     * does not support partial processing. */
} CpaCySymCipherAlgorithm;

/**
 * @ingroup cpaCySym
 *      Size of bitmap needed for cipher "capabilities" type.
 *
 * @description
 *      Defines the number of bits in the bitmap to represent supported
 *      ciphers in the type @ref CpaCySymCapabilitiesInfo.  Should be set to
 *      at least one greater than the largest value in the enumerated type
 *      @ref CpaCySymHashAlgorithm, so that the value of the enum constant
 *      can also be used as the bit position in the bitmap.
 *
 *      A larger value was chosen to allow for extensibility without the need
 *      to change the size of the bitmap (to ease backwards compatibility in
 *      future versions of the API).
 */
#define CPA_CY_SYM_CIPHER_CAP_BITMAP_SIZE (32)


/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Symmetric Cipher Direction
 * @description
 *      This enum indicates the cipher direction (encryption or decryption).
 *
 *****************************************************************************/
typedef enum _CpaCySymCipherDirection
{
    CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT = 1,
    /**< Encrypt Data */
    CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT
    /**< Decrypt Data */
} CpaCySymCipherDirection;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Symmetric Cipher Setup Data.
 * @description
 *      This structure contains data relating to Cipher (Encryption and
 *      Decryption) to setup a session.
 *
 *****************************************************************************/
typedef struct _CpaCySymCipherSetupData {
    CpaCySymCipherAlgorithm cipherAlgorithm;
    /**< Cipher algorithm and mode */
    Cpa32U cipherKeyLenInBytes;
    /**< Cipher key length in bytes. For AES it can be 128 bits (16 bytes),
     * 192 bits (24 bytes) or 256 bits (32 bytes).
     * For the CCM mode of operation, the only supported key length is 128 bits
     * (16 bytes).
     * For the CPA_CY_SYM_CIPHER_AES_F8 mode of operation, cipherKeyLenInBytes
     * should be set to the combined length of the encryption key and the
     * keymask. Since the keymask and the encryption key are the same size,
     * cipherKeyLenInBytes should be set to 2 x the AES encryption key length.
     * For the AES-XTS mode of operation:
     * - Two keys must be provided and cipherKeyLenInBytes refers to total
     *   length of the two keys.
     * - Each key can be either 128 bits (16 bytes) or 256 bits (32 bytes).
     * - Both keys must have the same size.
     */
    Cpa8U *pCipherKey;
    /**< Cipher key
     * For the CPA_CY_SYM_CIPHER_AES_F8 mode of operation, pCipherKey will
     * point to a concatenation of the AES encryption key followed by a
     * keymask. As per RFC3711, the keymask should be padded with trailing
     * bytes to match the length of the encryption key used.
     * For AES-XTS mode of operation, two keys must be provided and pCipherKey
     * must point to the two keys concatenated together (Key1 || Key2).
     * cipherKeyLenInBytes will contain the total size of both keys.
     * These fields are set to NULL if key derivation will be used.
     */
    CpaCySymCipherDirection cipherDirection;
    /**< This parameter determines if the cipher operation is an encrypt or
     * a decrypt operation.
     * For the RC4 algorithm and the F8/CTR modes, only encrypt operations
     * are valid. */
} CpaCySymCipherSetupData;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Symmetric Hash mode
 * @description
 *      This enum indicates the Hash Mode.
 *
 *****************************************************************************/
typedef enum _CpaCySymHashMode
{
    CPA_CY_SYM_HASH_MODE_PLAIN = 1,
    /**< Plain hash.  Can be specified for MD5 and the SHA family of
     * hash algorithms. */
    CPA_CY_SYM_HASH_MODE_AUTH,
    /**< Authenticated hash.  This mode may be used in conjunction with the
     * MD5 and SHA family of algorithms to specify HMAC.  It MUST also be
     * specified with all of the remaining algorithms, all of which are in
     * fact authentication algorithms.
     */
    CPA_CY_SYM_HASH_MODE_NESTED
    /**< Nested hash.  Can be specified for MD5 and the SHA family of
     * hash algorithms. */
} CpaCySymHashMode;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Hash algorithms.
 * @description
 *      This enumeration lists supported hash algorithms.
 *
 *****************************************************************************/
typedef enum _CpaCySymHashAlgorithm
{
    CPA_CY_SYM_HASH_NONE = 0,
    /**< No hash algorithm. */
    CPA_CY_SYM_HASH_MD5,
    /**< MD5 algorithm. Supported in all 3 hash modes */
    CPA_CY_SYM_HASH_SHA1,
    /**< 128 bit SHA algorithm. Supported in all 3 hash modes */
    CPA_CY_SYM_HASH_SHA224,
    /**< 224 bit SHA algorithm. Supported in all 3 hash modes */
    CPA_CY_SYM_HASH_SHA256,
    /**< 256 bit SHA algorithm. Supported in all 3 hash modes */
    CPA_CY_SYM_HASH_SHA384,
    /**< 384 bit SHA algorithm. Supported in all 3 hash modes */
    CPA_CY_SYM_HASH_SHA512,
    /**< 512 bit SHA algorithm. Supported in all 3 hash modes */
    CPA_CY_SYM_HASH_AES_XCBC,
    /**< AES XCBC algorithm. This is only supported in the hash mode
     * CPA_CY_SYM_HASH_MODE_AUTH. */
    CPA_CY_SYM_HASH_AES_CCM,
    /**< AES algorithm in CCM mode. This authenticated cipher requires that the
     * hash mode is set to CPA_CY_SYM_HASH_MODE_AUTH. When this hash algorithm
     * is used, the CPA_CY_SYM_CIPHER_AES_CCM element of the
     * CpaCySymCipherAlgorithm enum MUST be used to set up the related
     * CpaCySymCipherSetupData structure in the session context. */
    CPA_CY_SYM_HASH_AES_GCM,
    /**< AES algorithm in GCM mode. This authenticated cipher requires that the
     * hash mode is set to CPA_CY_SYM_HASH_MODE_AUTH. When this hash algorithm
     * is used, the CPA_CY_SYM_CIPHER_AES_GCM element of the
     * CpaCySymCipherAlgorithm enum MUST be used to set up the related
     * CpaCySymCipherSetupData structure in the session context. */
    CPA_CY_SYM_HASH_KASUMI_F9,
    /**< Kasumi algorithm in F9 mode.  This is only supported in the hash
     * mode CPA_CY_SYM_HASH_MODE_AUTH. */
    CPA_CY_SYM_HASH_SNOW3G_UIA2,
    /**< SNOW3G algorithm in UIA2 mode.  This is only supported in the hash
     * mode CPA_CY_SYM_HASH_MODE_AUTH. */
    CPA_CY_SYM_HASH_AES_CMAC,
    /**< AES CMAC algorithm. This is only supported in the hash mode
     * CPA_CY_SYM_HASH_MODE_AUTH. */
    CPA_CY_SYM_HASH_AES_GMAC,
    /**< AES GMAC algorithm. This is only supported in the hash mode
     * CPA_CY_SYM_HASH_MODE_AUTH. When this hash algorithm
     * is used, the CPA_CY_SYM_CIPHER_AES_GCM element of the
     * CpaCySymCipherAlgorithm enum MUST be used to set up the related
     * CpaCySymCipherSetupData structure in the session context. */
    CPA_CY_SYM_HASH_AES_CBC_MAC,
    /**< AES-CBC-MAC algorithm. This is only supported in the hash mode
     * CPA_CY_SYM_HASH_MODE_AUTH. Only 128-bit keys are supported. */
    CPA_CY_SYM_HASH_ZUC_EIA3,
    /**< ZUC algorithm in EIA3 mode */
    CPA_CY_SYM_HASH_SHA3_256,
    /**< 256 bit SHA-3 algorithm. Only CPA_CY_SYM_HASH_MODE_PLAIN and
     * CPA_CY_SYM_HASH_MODE_AUTH are supported, that is, the hash
     * mode CPA_CY_SYM_HASH_MODE_NESTED is not supported for this algorithm.
     * Partial requests are not supported, that is, only requests
     * of CPA_CY_SYM_PACKET_TYPE_FULL are supported. */
    CPA_CY_SYM_HASH_SHA3_224,
    /**< 224 bit SHA-3 algorithm. Only CPA_CY_SYM_HASH_MODE_PLAIN and
     * CPA_CY_SYM_HASH_MODE_AUTH are supported, that is, the hash
     * mode CPA_CY_SYM_HASH_MODE_NESTED is not supported for this algorithm.
     */
    CPA_CY_SYM_HASH_SHA3_384,
    /**< 384 bit SHA-3 algorithm. Only CPA_CY_SYM_HASH_MODE_PLAIN and
     * CPA_CY_SYM_HASH_MODE_AUTH are supported, that is, the hash
     * mode CPA_CY_SYM_HASH_MODE_NESTED is not supported for this algorithm.
     * Partial requests are not supported, that is, only requests
     * of CPA_CY_SYM_PACKET_TYPE_FULL are supported. */
    CPA_CY_SYM_HASH_SHA3_512,
    /**< 512 bit SHA-3 algorithm. Only CPA_CY_SYM_HASH_MODE_PLAIN and
     * CPA_CY_SYM_HASH_MODE_AUTH are supported, that is, the hash
     * mode CPA_CY_SYM_HASH_MODE_NESTED is not supported for this algorithm.
     * Partial requests are not supported, that is, only requests
     * of CPA_CY_SYM_PACKET_TYPE_FULL are supported. */
    CPA_CY_SYM_HASH_SHAKE_128,
    /**< 128 bit SHAKE algorithm. This is only supported in the hash
     * mode CPA_CY_SYM_HASH_MODE_PLAIN. Partial requests are not
     * supported, that is, only requests of CPA_CY_SYM_PACKET_TYPE_FULL
     * are supported. */
    CPA_CY_SYM_HASH_SHAKE_256,
    /**< 256 bit SHAKE algorithm. This is only supported in the hash
     * mode CPA_CY_SYM_HASH_MODE_PLAIN. Partial requests are not
     * supported, that is, only requests of CPA_CY_SYM_PACKET_TYPE_FULL
     * are supported. */
    CPA_CY_SYM_HASH_POLY,
    /**< Poly1305 hash algorithm. This is only supported in the hash mode
     * CPA_CY_SYM_HASH_MODE_AUTH. This hash algorithm is only supported
     * as part of an algorithm chain with AES_CY_SYM_CIPHER_CHACHA to
     * implement the ChaCha20-Poly1305 AEAD algorithm. */
    CPA_CY_SYM_HASH_SM3
    /**< SM3 hash algorithm. Supported in all 3 hash modes. */
 } CpaCySymHashAlgorithm;

/**
 * @ingroup cpaCySym
 *      Size of bitmap needed for hash "capabilities" type.
 *
 * @description
 *      Defines the number of bits in the bitmap to represent supported
 *      hashes in the type @ref CpaCySymCapabilitiesInfo.  Should be set to
 *      at least one greater than the largest value in the enumerated type
 *      @ref CpaCySymHashAlgorithm, so that the value of the enum constant
 *      can also be used as the bit position in the bitmap.
 *
 *      A larger value was chosen to allow for extensibility without the need
 *      to change the size of the bitmap (to ease backwards compatibility in
 *      future versions of the API).
 */
#define CPA_CY_SYM_HASH_CAP_BITMAP_SIZE (32)

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Hash Mode Nested Setup Data.
 * @description
 *      This structure contains data relating to a hash session in
 *      CPA_CY_SYM_HASH_MODE_NESTED mode.
 *
 *****************************************************************************/
typedef struct _CpaCySymHashNestedModeSetupData {
    Cpa8U *pInnerPrefixData;
    /**< A pointer to a buffer holding the Inner Prefix data. For optimal
     * performance the prefix data SHOULD be 8-byte aligned. This data is
     * prepended to the data being hashed before the inner hash operation is
     * performed. */
    Cpa32U innerPrefixLenInBytes;
    /**< The inner prefix length in bytes. The maximum size the prefix data
     * can be is 255 bytes. */
    CpaCySymHashAlgorithm outerHashAlgorithm;
    /**< The hash algorithm used for the outer hash. Note: The inner hash
     * algorithm is provided in the hash context.  */
    Cpa8U *pOuterPrefixData;
    /**< A pointer to a buffer holding the Outer Prefix data. For optimal
     * performance the prefix data SHOULD be 8-byte aligned. This data is
     * prepended to the output from the inner hash operation before the outer
     * hash operation is performed.*/
    Cpa32U outerPrefixLenInBytes;
    /**< The outer prefix length in bytes. The maximum size the prefix data
     * can be is 255 bytes. */
} CpaCySymHashNestedModeSetupData;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Hash Auth Mode Setup Data.
 * @description
 *      This structure contains data relating to a hash session in
 *      CPA_CY_SYM_HASH_MODE_AUTH mode.
 *
 *****************************************************************************/
typedef struct _CpaCySymHashAuthModeSetupData {
    Cpa8U *authKey;
    /**< Authentication key pointer.
     * For the GCM (@ref CPA_CY_SYM_HASH_AES_GCM) and CCM (@ref
     * CPA_CY_SYM_HASH_AES_CCM) modes of operation, this field is ignored;
     * the authentication key is the same as the cipher key (see
     * the field pCipherKey in struct @ref CpaCySymCipherSetupData).
     */
    Cpa32U authKeyLenInBytes;
    /**< Length of the authentication key in bytes. The key length MUST be
     * less than or equal to the block size of the algorithm. It is the client's
     * responsibility to ensure that the key length is compliant with the
     * standard being used (for example RFC 2104, FIPS 198a).
     *
     * For the GCM (@ref CPA_CY_SYM_HASH_AES_GCM) and CCM (@ref
     * CPA_CY_SYM_HASH_AES_CCM) modes of operation, this field is ignored;
     * the authentication key is the same as the cipher key, and so is its
     * length (see the field cipherKeyLenInBytes in struct @ref
     * CpaCySymCipherSetupData).
     */
    Cpa32U aadLenInBytes;
    /**< The length of the additional authenticated data (AAD) in bytes.
     * The maximum permitted value is 240 bytes, unless otherwise
     * specified below.
     *
     * This field must be specified when the hash algorithm is one of the
     * following:

     * - For SNOW3G (@ref CPA_CY_SYM_HASH_SNOW3G_UIA2), this is the
     *   length of the IV (which should be 16).
     * - For GCM (@ref CPA_CY_SYM_HASH_AES_GCM).  In this case, this is the
     *   length of the Additional Authenticated Data (called A, in NIST
     *   SP800-38D).
     * - For CCM (@ref CPA_CY_SYM_HASH_AES_CCM).  In this case, this is the
     *   length of the associated data (called A, in NIST SP800-38C).
     *   Note that this does NOT include the length of any padding, or the
     *   18 bytes reserved at the start of the above field to store the
     *   block B0 and the encoded length.  The maximum permitted value in
     *   this case is 222 bytes.
     *
     *   @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC) mode of operation
     *   this field is not used and should be set to 0. Instead the length
     *   of the AAD data is specified in the messageLenToHashInBytes field of
     *   the CpaCySymOpData structure.
     */
} CpaCySymHashAuthModeSetupData;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Hash Setup Data.
 * @description
 *      This structure contains data relating to a hash session. The fields
 *      hashAlgorithm, hashMode and digestResultLenInBytes are common to all
 *      three hash modes and MUST be set for each mode.
 *
 *****************************************************************************/
typedef struct _CpaCySymHashSetupData {
    CpaCySymHashAlgorithm hashAlgorithm;
    /**< Hash algorithm. For mode CPA_CY_SYM_MODE_HASH_NESTED, this is the
     * inner hash algorithm. */
    CpaCySymHashMode hashMode;
    /**< Mode of the hash operation. Valid options include plain, auth or
     * nested hash mode. */
    Cpa32U digestResultLenInBytes;
    /**< Length of the digest to be returned. If the verify option is set,
     * this specifies the length of the digest to be compared for the
     * session.
     *
     * For CCM (@ref CPA_CY_SYM_HASH_AES_CCM), this is the octet length
     * of the MAC, which can be one of 4, 6, 8, 10, 12, 14 or 16.
     *
     * For GCM (@ref CPA_CY_SYM_HASH_AES_GCM), this is the length in bytes
     * of the authentication tag.
     *
     * If the value is less than the maximum length allowed by the hash,
     * the result shall be truncated.  If the value is greater than the
     * maximum length allowed by the hash, an error (@ref
     * CPA_STATUS_INVALID_PARAM) is returned from the function @ref
     * cpaCySymInitSession.
     *
     * In the case of nested hash, it is the outer hash which determines
     * the maximum length allowed.  */
    CpaCySymHashAuthModeSetupData authModeSetupData;
    /**< Authentication Mode Setup Data.
     * Only valid for mode CPA_CY_SYM_MODE_HASH_AUTH */
    CpaCySymHashNestedModeSetupData nestedModeSetupData;
    /**< Nested Hash Mode Setup Data
     * Only valid for mode CPA_CY_SYM_MODE_HASH_NESTED */
} CpaCySymHashSetupData;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Algorithm Chaining Operation Ordering
 * @description
 *      This enum defines the ordering of operations for algorithm chaining.
 *
 ****************************************************************************/
typedef enum _CpaCySymAlgChainOrder
{
    CPA_CY_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER = 1,
    /**< Perform the hash operation followed by the cipher operation. If it is
     * required that the result of the hash (i.e. the digest) is going to be
     * included in the data to be ciphered, then:
     *
     * <ul>
     * <li> The digest MUST be placed in the destination buffer at the
     *    location corresponding to the end of the data region to be hashed
     *    (hashStartSrcOffsetInBytes + messageLenToHashInBytes),
     *    i.e.  there must be no gaps between the start of the digest and the
     *    end of the data region to be hashed.</li>
     * <li> The messageLenToCipherInBytes member of the CpaCySymOpData
     *    structure must be equal to the overall length of the plain text,
     *    the digest length and any (optional) trailing data that is to be
     *    included.</li>
     * <li> The messageLenToCipherInBytes must be a multiple to the block
     *    size if a block cipher is being used.</li>
     * </ul>
     *
     * The following is an example of the layout of the buffer before the
     * operation, after the hash, and after the cipher:

@verbatim

+-------------------------+---------------+
|         Plaintext       |     Tail      |
+-------------------------+---------------+
<-messageLenToHashInBytes->

+-------------------------+--------+------+
|         Plaintext       | Digest | Tail |
+-------------------------+--------+------+
<--------messageLenToCipherInBytes-------->

+-----------------------------------------+
|               Cipher Text               |
+-----------------------------------------+

@endverbatim
     */
    CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH
    /**< Perform the cipher operation followed by the hash operation.
     * The hash operation will be performed on the ciphertext resulting from
     * the cipher operation.
     *
     * The following is an example of the layout of the buffer before the
     * operation, after the cipher, and after the hash:

@verbatim

+--------+---------------------------+---------------+
|  Head  |         Plaintext         |    Tail       |
+--------+---------------------------+---------------+
         <-messageLenToCipherInBytes->

+--------+---------------------------+---------------+
|  Head  |         Ciphertext        |    Tail       |
+--------+---------------------------+---------------+
<------messageLenToHashInBytes------->

+--------+---------------------------+--------+------+
|  Head  |         Ciphertext        | Digest | Tail |
+--------+---------------------------+--------+------+

@endverbatim
     *
     */
} CpaCySymAlgChainOrder;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Session Setup Data.
 * @description
 *      This structure contains data relating to setting up a session. The
 *      client needs to complete the information in this structure in order to
 *      setup a session.
 *
 ****************************************************************************/
typedef struct _CpaCySymSessionSetupData {
    CpaCyPriority sessionPriority;
    /**< Priority of this session */
    CpaCySymOp symOperation;
    /**< Operation to perform */
    CpaCySymCipherSetupData cipherSetupData;
    /**< Cipher Setup Data for the session. This member is ignored for the
     * CPA_CY_SYM_OP_HASH operation. */
    CpaCySymHashSetupData hashSetupData;
    /**< Hash Setup Data for a session. This member is ignored for the
     * CPA_CY_SYM_OP_CIPHER operation. */
    CpaCySymAlgChainOrder algChainOrder;
    /**< If this operation data structure relates to an algorithm chaining
     * session then this parameter determines the order in which the chained
     * operations are performed. If this structure does not relate to an
     * algorithm chaining session then this parameter will be ignored.
     *
     * @note In the case of authenticated ciphers (GCM and CCM), which are
     * also presented as "algorithm chaining", this value is also ignored.
     * The chaining order is defined by the authenticated cipher, in those
     * cases. */
    CpaBoolean digestIsAppended;
    /**< Flag indicating whether the digest is appended immediately following
     * the region over which the digest is computed. This is true for both
     * IPsec packets and SSL/TLS records.
     *
     * If this flag is set, then the value of the pDigestResult field of
     * the structure @ref CpaCySymOpData is ignored.
     *
     * @note The value of this field is ignored for the authenticated cipher
     * AES_CCM as the digest must be appended in this case.
     *
     * @note Setting digestIsAppended for hash only operations when
     * verifyDigest is also set is not supported. For hash only operations
     * when verifyDigest is set, digestIsAppended should be set to CPA_FALSE.
     */
    CpaBoolean verifyDigest;
    /**< This flag is relevant only for operations which generate a message
     * digest. If set to true, the computed digest will not be written back
     * to the buffer location specified by other parameters, but instead will
     * be verified (i.e. compared to the value passed in at that location).
     * The number of bytes to be written or compared is indicated by the
     * digest output length for the session.
     * @note This option is only valid for full packets and for final
     * partial packets when using partials without algorithm chaining.
     * @note The value of this field is ignored for the authenticated ciphers
     * (AES_CCM and AES_GCM). Digest verification is always done for these
     * (when the direction is decrypt) and unless the DP API is used,
     * the message buffer will be zeroed if verification fails. When using the
     * DP API, it is the API clients responsibility to clear the message
     * buffer when digest verification fails.
     */
    CpaBoolean partialsNotRequired;
    /**< This flag indicates if partial packet processing is required for this
     * session. If set to true, partial packet processing will not be enabled
     * for this session and any calls to cpaCySymPerformOp() with the
     * packetType parameter set to a value other than
     * CPA_CY_SYM_PACKET_TYPE_FULL will fail.
     */
} CpaCySymSessionSetupData ;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Session Update Data.
 * @description
 *      This structure contains data relating to resetting a session.
 ****************************************************************************/
typedef struct _CpaCySymSessionUpdateData  {
    Cpa32U flags;
    /**< Flags indicating which fields to update.
      * All bits should be set to 0 except those fields to be updated.
      */
#define CPA_CY_SYM_SESUPD_CIPHER_KEY    1 << 0
#define CPA_CY_SYM_SESUPD_CIPHER_DIR    1 << 1
#define CPA_CY_SYM_SESUPD_AUTH_KEY      1 << 2
    Cpa8U *pCipherKey;
    /**< Cipher key.
     * The same restrictions apply as described in the corresponding field
     * of the data structure @ref CpaCySymCipherSetupData.
     */
    CpaCySymCipherDirection cipherDirection;
    /**< This parameter determines if the cipher operation is an encrypt or
     * a decrypt operation.
     * The same restrictions apply as described in the corresponding field
     * of the data structure @ref CpaCySymCipherSetupData.
     */
    Cpa8U *authKey;
    /**< Authentication key pointer.
     * The same restrictions apply as described in the corresponding field
     * of the data structure @ref CpaCySymHashAuthModeSetupData.
     */
} CpaCySymSessionUpdateData;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Cryptographic Component Operation Data.
 * @description
 *      This structure contains data relating to performing cryptographic
 *      processing on a data buffer. This request is used with
 *      cpaCySymPerformOp() call for performing cipher, hash, auth cipher
 *      or a combined hash and cipher operation.
 *
 * @see
 *      CpaCySymPacketType
 *
 * @note
 *      If the client modifies or frees the memory referenced in this structure
 *      after it has been submitted to the cpaCySymPerformOp function, and
 *      before it has been returned in the callback, undefined behavior will
 *      result.
 ****************************************************************************/
typedef struct _CpaCySymOpData {
    CpaCySymSessionCtx sessionCtx;
    /**< Handle for the initialized session context */
    CpaCySymPacketType packetType;
    /**< Selects the packet type */
    Cpa8U *pIv;
    /**< Initialization Vector or Counter.
     *
     * - For block ciphers in CBC or F8 mode, or for Kasumi in F8 mode, or for
     *   SNOW3G in UEA2 mode, this is the Initialization Vector (IV)
     *   value.
     * - For block ciphers in CTR mode, this is the counter.
     * - For GCM mode, this is either the IV (if the length is 96 bits) or J0
     *   (for other sizes), where J0 is as defined by NIST SP800-38D.
     *   Regardless of the IV length, a full 16 bytes needs to be allocated.
     * - For CCM mode, the first byte is reserved, and the nonce should be
     *   written starting at &pIv[1] (to allow space for the implementation
     *   to write in the flags in the first byte).  Note that a full 16 bytes
     *   should be allocated, even though the ivLenInBytes field will have
     *   a value less than this.
     *   The macro @ref CPA_CY_SYM_CCM_SET_NONCE may be used here.
     * - For AES-XTS, this is the 128bit tweak, i, from IEEE Std 1619-2007.
     *
     * For optimum performance, the data pointed to SHOULD be 8-byte
     * aligned.
     *
     * The IV/Counter will be updated after every partial cryptographic
     * operation.
     */
    Cpa32U ivLenInBytes;
    /**< Length of valid IV data pointed to by the pIv parameter.
     *
     * - For block ciphers in CBC or F8 mode, or for Kasumi in F8 mode, or for
     *   SNOW3G in UEA2 mode, this is the length of the IV (which
     *   must be the same as the block length of the cipher).
     * - For block ciphers in CTR mode, this is the length of the counter
     *   (which must be the same as the block length of the cipher).
     * - For GCM mode, this is either 12 (for 96-bit IVs) or 16, in which
     *   case pIv points to J0.
     * - For CCM mode, this is the length of the nonce, which can be in the
     *   range 7 to 13 inclusive.
     */
    Cpa32U cryptoStartSrcOffsetInBytes;
    /**< Starting point for cipher processing, specified as number of bytes
     * from start of data in the source buffer. The result of the cipher
     * operation will be written back into the output buffer starting
     * at this location.
     */
    Cpa32U messageLenToCipherInBytes;
    /**< The message length, in bytes, of the source buffer on which the
     * cryptographic operation will be computed. This must be a multiple of
     * the block size if a block cipher is being used. This is also the same
     * as the result length.
     *
     * @note In the case of CCM (@ref CPA_CY_SYM_HASH_AES_CCM), this value
     * should not include the length of the padding or the length of the
     * MAC; the driver will compute the actual number of bytes over which
     * the encryption will occur, which will include these values.
     *
     * @note There are limitations on this length for partial
     * operations. Refer to the cpaCySymPerformOp function description for
     * details.
     *
     * @note On some implementations, this length may be limited to a 16-bit
     * value (65535 bytes).
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC), this field
     * should be set to 0.
     */
    Cpa32U hashStartSrcOffsetInBytes;
    /**< Starting point for hash processing, specified as number of bytes
     * from start of packet in source buffer.
     *
     * @note For CCM and GCM modes of operation, this field is ignored.
     * The field @ref pAdditionalAuthData field should be set instead.
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC) mode of
     * operation, this field specifies the start of the AAD data in
     * the source buffer.
     */
    Cpa32U messageLenToHashInBytes;
    /**< The message length, in bytes, of the source buffer that the hash
     * will be computed on.
     *
     * @note There are limitations on this length for partial operations.
     * Refer to the @ref cpaCySymPerformOp function description for details.
     *
     * @note For CCM and GCM modes of operation, this field is ignored.
     * The field @ref pAdditionalAuthData field should be set instead.
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC) mode of
     * operation, this field specifies the length of the AAD data in the
     * source buffer. The maximum length supported for AAD data for AES-GMAC
     * is 16383 bytes.
     *
     * @note On some implementations, this length may be limited to a 16-bit
     * value (65535 bytes).
     */
    Cpa8U *pDigestResult;
    /**<  If the digestIsAppended member of the @ref CpaCySymSessionSetupData
     * structure is NOT set then this is a pointer to the location where the
     * digest result should be inserted (in the case of digest generation)
     * or where the purported digest exists (in the case of digest verification).
     *
     * At session registration time, the client specified the digest result
     * length with the digestResultLenInBytes member of the @ref
     * CpaCySymHashSetupData structure. The client must allocate at least
     * digestResultLenInBytes of physically contiguous memory at this location.
     *
     * For partial packet processing without algorithm chaining, this pointer
     * will be ignored for all but the final partial operation.
     *
     * For digest generation, the digest result will overwrite any data
     * at this location.
     *
     * @note For GCM (@ref CPA_CY_SYM_HASH_AES_GCM), for "digest result"
     * read "authentication tag T".
     *
     * If the digestIsAppended member of the @ref CpaCySymSessionSetupData
     * structure is set then this value is ignored and the digest result
     * is understood to be in the destination buffer for digest generation,
     * and in the source buffer for digest verification. The location of the
     * digest result in this case is immediately following the region over
     * which the digest is computed.
     *
     */
    Cpa8U *pAdditionalAuthData;
    /**< Pointer to Additional Authenticated Data (AAD) needed for
     * authenticated cipher mechanisms (CCM and GCM), and to the IV for
     * SNOW3G authentication (@ref CPA_CY_SYM_HASH_SNOW3G_UIA2).
     * For other authentication mechanisms this pointer is ignored.
     *
     * The length of the data pointed to by this field is set up for
     * the session in the @ref CpaCySymHashAuthModeSetupData structure
     * as part of the @ref cpaCySymInitSession function call.  This length
     * must not exceed 240 bytes.
     *
     * Specifically for CCM (@ref CPA_CY_SYM_HASH_AES_CCM), the caller
     * should setup this field as follows:
     *
     * - the nonce should be written starting at an offset of one byte
     *   into the array, leaving room for the implementation to write in
     *   the flags to the first byte.  For example,
     *   <br>
     *   memcpy(&pOpData->pAdditionalAuthData[1], pNonce, nonceLen);
     *   <br>
     *   The macro @ref CPA_CY_SYM_CCM_SET_NONCE may be used here.
     *
     * - the additional  authentication data itself should be written
     *   starting at an offset of 18 bytes into the array, leaving room for
     *   the length encoding in the first two bytes of the second block.
     *   For example,
     *   <br>
     *   memcpy(&pOpData->pAdditionalAuthData[18], pAad, aadLen);
     *   <br>
     *   The macro @ref CPA_CY_SYM_CCM_SET_AAD may be used here.
     *
     * - the array should be big enough to hold the above fields, plus
     *   any padding to round this up to the nearest multiple of the
     *   block size (16 bytes).  Padding will be added by the
     *   implementation.
     *
     * Finally, for GCM (@ref CPA_CY_SYM_HASH_AES_GCM), the caller
     * should setup this field as follows:
     *
     * - the AAD is written in starting at byte 0
     * - the array must be big enough to hold the AAD, plus any padding
     *   to round this up to the nearest multiple of the block size (16
     *   bytes).  Padding will be added by the implementation.
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC) mode of
     * operation, this field is not used and should be set to 0. Instead
     * the AAD data should be placed in the source buffer.
     */

} CpaCySymOpData;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Setup the nonce for CCM.
 * @description
 *      This macro sets the nonce in the appropriate locations of the
 *      @ref CpaCySymOpData struct for the authenticated encryption
 *      algorithm @ref CPA_CY_SYM_HASH_AES_CCM.
 ****************************************************************************/
#define CPA_CY_SYM_CCM_SET_NONCE(pOpData, pNonce, nonceLen) do { \
    memcpy(&pOpData->pIv[1], pNonce, nonceLen); \
    memcpy(&pOpData->pAdditionalAuthData[1], pNonce, nonceLen); \
    } while (0)

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Setup the additional authentication data for CCM.
 * @description
 *      This macro sets the additional authentication data in the
 *      appropriate location of the@ref CpaCySymOpData struct for the
 *      authenticated encryption algorithm @ref CPA_CY_SYM_HASH_AES_CCM.
 ****************************************************************************/
#define CPA_CY_SYM_CCM_SET_AAD(pOpData, pAad, aadLen) do { \
    memcpy(&pOpData->pAdditionalAuthData[18], pAad, aadLen); \
    } while (0)


/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Cryptographic Component Statistics.
 * @deprecated
 *      As of v1.3 of the cryptographic API, this structure has been
 *      deprecated, replaced by @ref CpaCySymStats64.
 * @description
 *      This structure contains statistics on the Symmetric Cryptographic
 *      operations. Statistics are set to zero when the component is
 *      initialized.
 ****************************************************************************/
typedef struct _CpaCySymStats {
    Cpa32U numSessionsInitialized;
    /**<  Number of session initialized */
    Cpa32U numSessionsRemoved;
    /**<  Number of sessions removed */
    Cpa32U numSessionErrors;
    /**<  Number of session initialized and removed errors. */
    Cpa32U numSymOpRequests;
    /**<  Number of successful symmetric operation requests. */
    Cpa32U numSymOpRequestErrors;
    /**<  Number of operation requests that had an error and could
     * not be processed. */
    Cpa32U numSymOpCompleted;
    /**<  Number of operations that completed successfully. */
    Cpa32U numSymOpCompletedErrors;
    /**<  Number of operations that could not be completed
     * successfully due to errors. */
    Cpa32U numSymOpVerifyFailures;
    /**<  Number of operations that completed successfully, but the
     * result of the digest verification test was that it failed.
     * Note that this does not indicate an error condition. */
} CpaCySymStats CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Cryptographic Component Statistics (64-bit version).
 * @description
 *      This structure contains a 64-bit version of the statistics on
 *      the Symmetric Cryptographic operations.
 *      Statistics are set to zero when the component is initialized.
 ****************************************************************************/
typedef struct _CpaCySymStats64 {
    Cpa64U numSessionsInitialized;
    /**<  Number of session initialized */
    Cpa64U numSessionsRemoved;
    /**<  Number of sessions removed */
    Cpa64U numSessionErrors;
    /**<  Number of session initialized and removed errors. */
    Cpa64U numSymOpRequests;
    /**<  Number of successful symmetric operation requests. */
    Cpa64U numSymOpRequestErrors;
    /**<  Number of operation requests that had an error and could
     * not be processed. */
    Cpa64U numSymOpCompleted;
    /**<  Number of operations that completed successfully. */
    Cpa64U numSymOpCompletedErrors;
    /**<  Number of operations that could not be completed
     * successfully due to errors. */
    Cpa64U numSymOpVerifyFailures;
    /**<  Number of operations that completed successfully, but the
     * result of the digest verification test was that it failed.
     * Note that this does not indicate an error condition. */
} CpaCySymStats64;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Definition of callback function
 *
 * @description
 *      This is the callback function prototype. The callback function is
 *      registered by the application using the cpaCySymInitSession()
 *      function call.
 *
 * @context
 *      This callback function can be executed in a context that DOES NOT
 *      permit sleeping to occur.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] pCallbackTag      Opaque value provided by user while making
 *                              individual function call.
 * @param[in] status            Status of the operation. Valid values are
 *                              CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                              CPA_STATUS_UNSUPPORTED.
 * @param[in] operationType     Identifies the operation type that was
 *                              requested in the cpaCySymPerformOp function.
 * @param[in] pOpData           Pointer to structure with input parameters.
 * @param[in] pDstBuffer        Caller MUST allocate a sufficiently sized
 *                              destination buffer to hold the data output. For
 *                              out-of-place processing the data outside the
 *                              cryptographic regions in the source buffer are
 *                              copied into the destination buffer. To perform
 *                              "in-place" processing set the pDstBuffer
 *                              parameter in cpaCySymPerformOp function to point
 *                              at the same location as pSrcBuffer. For optimum
 *                              performance, the data pointed to SHOULD be
 *                              8-byte aligned.
 * @param[in] verifyResult      This parameter is valid when the verifyDigest
 *                              option is set in the CpaCySymSessionSetupData
 *                              structure. A value of CPA_TRUE indicates that
 *                              the compare succeeded. A value of CPA_FALSE
 *                              indicates that the compare failed for an
 *                              unspecified reason.
 *
 * @retval
 *      None
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaCySymInitSession(),
 *      cpaCySymRemoveSession()
 *
 *****************************************************************************/
typedef void (*CpaCySymCbFunc)(void *pCallbackTag,
        CpaStatus status,
        const CpaCySymOp operationType,
        void *pOpData,
        CpaBufferList *pDstBuffer,
        CpaBoolean verifyResult);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Gets the size required to store a session context.
 *
 * @description
 *      This function is used by the client to determine the size of the memory
 *      it must allocate in order to store the session context. This MUST be
 *      called before the client allocates the memory for the session context
 *      and before the client calls the @ref cpaCySymInitSession function.
 *
 *      For a given implementation of this API, it is safe to assume that
 *      cpaCySymSessionCtxGetSize() will always return the same size and that
 *      the size will not be different for different setup data parameters.
 *      However, it should be noted that the size may change:
 *        (1) between different implementations of the API (e.g. between software
 *            and hardware implementations or between different hardware
 *            implementations)
 *        (2) between different releases of the same API implementation.
 *
 *      The size returned by this function is the smallest size needed to
 *      support all possible combinations of setup data parameters. Some
 *      setup data parameter combinations may fit within a smaller session
 *      context size. The alternate cpaCySymSessionCtxGetDynamicSize()
 *      function will return the smallest size needed to fit the
 *      provided setup data parameters.
 *
 * @context
 *      This is a synchronous function that cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle            Instance handle.
 * @param[in]  pSessionSetupData         Pointer to session setup data which
 *                                       contains parameters which are static
 *                                       for a given cryptographic session such
 *                                       as operation type, mechanisms, and keys
 *                                       for cipher and/or hash operations.
 * @param[out] pSessionCtxSizeInBytes    The amount of memory in bytes required
 *                                       to hold the Session Context.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 * @see
 *      CpaCySymSessionSetupData
 *      cpaCySymInitSession()
 *      cpaCySymSessionCtxGetDynamicSize()
 *      cpaCySymPerformOp()
 *
 *****************************************************************************/
CpaStatus
cpaCySymSessionCtxGetSize(const CpaInstanceHandle instanceHandle,
        const CpaCySymSessionSetupData *pSessionSetupData,
        Cpa32U *pSessionCtxSizeInBytes);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Gets the minimum size required to store a session context.
 *
 * @description
 *      This function is used by the client to determine the smallest size of
 *      the memory it must allocate in order to store the session context.
 *      This MUST be called before the client allocates the memory for the
 *      session context and before the client calls the @ref cpaCySymInitSession
 *      function.
 *
 *      This function is an alternate to cpaCySymSessionGetSize().
 *      cpaCySymSessionCtxGetSize() will return a fixed size which is the
 *      minimum memory size needed to support all possible setup data parameter
 *      combinations. cpaCySymSessionCtxGetDynamicSize() will return the
 *      minimum memory size needed to support the specific session setup
 *      data parameters provided. This size may be different for different setup
 *      data parameters.
 *
 * @context
 *      This is a synchronous function that cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle            Instance handle.
 * @param[in]  pSessionSetupData         Pointer to session setup data which
 *                                       contains parameters which are static
 *                                       for a given cryptographic session such
 *                                       as operation type, mechanisms, and keys
 *                                       for cipher and/or hash operations.
 * @param[out] pSessionCtxSizeInBytes    The amount of memory in bytes required
 *                                       to hold the Session Context.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 * @see
 *      CpaCySymSessionSetupData
 *      cpaCySymInitSession()
 *      cpaCySymSessionCtxGetSize()
 *      cpaCySymPerformOp()
 *
 *****************************************************************************/
CpaStatus
cpaCySymSessionCtxGetDynamicSize(const CpaInstanceHandle instanceHandle,
        const CpaCySymSessionSetupData *pSessionSetupData,
        Cpa32U *pSessionCtxSizeInBytes);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Initialize a session for symmetric cryptographic API.
 *
 * @description
 *      This function is used by the client to initialize an asynchronous
 *      completion callback function for the symmetric cryptographic
 *      operations.  Clients MAY register multiple callback functions using
 *      this function.
 *      The callback function is identified by the combination of userContext,
 *      pSymCb and session context (sessionCtx).  The session context is the
 *      handle to the session and needs to be passed when processing calls.
 *      Callbacks on completion of operations within a session are guaranteed
 *      to be in the same order they were submitted in.
 *
 * @context
 *      This is a synchronous function and it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle       Instance handle.
 * @param[in]  pSymCb               Pointer to callback function to be
 *                                  registered. Set to NULL if the
 *                                  cpaCySymPerformOp function is required to
 *                                  work in a synchronous manner.
 * @param[in]  pSessionSetupData    Pointer to session setup data which contains
 *                                  parameters which are static for a given
 *                                  cryptographic session such as operation
 *                                  type, mechanisms, and keys for cipher and/or
 *                                  hash operations.
 * @param[out] sessionCtx           Pointer to the memory allocated by the
 *                                  client to store the session context. This
 *                                  will be initialized with this function. This
 *                                  value needs to be passed to subsequent
 *                                  processing calls.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 * @see
 *      CpaCySymSessionCtx,
 *      CpaCySymCbFunc,
 *      CpaCySymSessionSetupData,
 *      cpaCySymRemoveSession(),
 *      cpaCySymPerformOp()
 *
 *****************************************************************************/
CpaStatus
cpaCySymInitSession(const CpaInstanceHandle instanceHandle,
        const CpaCySymCbFunc pSymCb,
        const CpaCySymSessionSetupData *pSessionSetupData,
        CpaCySymSessionCtx sessionCtx);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Remove (delete) a symmetric cryptographic session.
 *
 * @description
 *      This function will remove a previously initialized session context
 *      and the installed callback handler function. Removal will fail if
 *      outstanding calls still exist for the initialized session handle.
 *      The client needs to retry the remove function at a later time.
 *      The memory for the session context MUST not be freed until this call
 *      has completed successfully.
 *
 * @context
 *      This is a synchronous function that cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]      instanceHandle    Instance handle.
 * @param[in,out]  pSessionCtx       Session context to be removed.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      Note that this is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      CpaCySymSessionCtx,
 *      cpaCySymInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaCySymRemoveSession(const CpaInstanceHandle instanceHandle,
        CpaCySymSessionCtx pSessionCtx);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Update a session.
 *
 * @description
 *      This function is used to update certain parameters of a session, as
 *      specified by the CpaCySymSessionUpdateData data structure.
 *
 *      It can be used on sessions created with either the so-called
 *      Traditional API (@ref cpaCySymInitSession) or the Data Plane API
 *      (@ref cpaCySymDpInitSession).
 *
 *      In order for this function to operate correctly, two criteria must
 *      be met:
 *
 *      - In the case of sessions created with the Traditional API, the
 *        session must be stateless, i.e. the field partialsNotRequired of
 *        the CpaCySymSessionSetupData data structure must be FALSE.
 *        (Sessions created using the Data Plane API are always stateless.)
 *
 *      - There must be no outstanding requests in flight for the session.
 *        The application can call the function @ref cpaCySymSessionInUse
 *        to test for this.
 *
 *        Note that in the case of multi-threaded applications (which are
 *        supported using the Traditional API only), this function may fail
 *        even if a previous invocation of the function @ref
 *        cpaCySymSessionInUse indicated that there were no outstanding
 *        requests.
 *
 * @param[in]  sessionCtx           Identifies the session to be reset.
 * @param[in]  pSessionUpdateData   Pointer to session data which contains
 * 	                                the parameters to be updated.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *****************************************************************************/
CpaStatus
cpaCySymUpdateSession(CpaCySymSessionCtx sessionCtx,
        const CpaCySymSessionUpdateData *pSessionUpdateData);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Indicates whether there are outstanding requests on a given
 *      session.
 *
 * @description
 *      This function is used to test whether there are outstanding
 *      requests in flight for a specified session. This may be used
 *      before resetting session parameters using the function @ref
 *      cpaCySymResetSession. See some additional notes on
 *      multi-threaded applications described on that function.
 *
 * @param[in]  sessionCtx            Identifies the session to be reset.
 * @param[out] pSessionInUse         Returns CPA_TRUE if there are
 *                                   outstanding requests on the session,
 *                                   or CPA_FALSE otherwise.
*****************************************************************************/
CpaStatus
cpaCySymSessionInUse(CpaCySymSessionCtx sessionCtx,
          CpaBoolean* pSessionInUse);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Perform a symmetric cryptographic operation on an existing session.
 *
 * @description
 *      Performs a cipher, hash or combined (cipher and hash) operation on
 *      the source data buffer using supported symmetric key algorithms and
 *      modes.
 *
 *      This function maintains cryptographic state between calls for
 *      partial cryptographic operations. If a partial cryptographic
 *      operation is being performed, then on a per-session basis, the next
 *      part of the multi-part message can be submitted prior to previous
 *      parts being completed, the only limitation being that all parts
 *      must be performed in sequential order.
 *
 *      If for any reason a client wishes to terminate the partial packet
 *      processing on the session (for example if a packet fragment was lost)
 *      then the client MUST remove the session.
 *
 *      When using partial packet processing with algorithm chaining, only
 *      the cipher state is maintained between calls. The hash state is
 *      not be maintained between calls. Instead the hash digest will be
 *      generated/verified for each call. If both the cipher state and
 *      hash state need to be maintained between calls, algorithm chaining
 *      cannot be used.

 *      The following restrictions apply to the length:
 *
 *      - When performing block based operations on a partial packet
 *        (excluding the final partial packet), the data that is to be
 *        operated on MUST be a multiple of the block size of the algorithm
 *        being used. This restriction only applies to the cipher state
 *        when using partial packets with algorithm chaining.
 *
 *      - The final block must not be of length zero (0) if the operation
 *        being performed is the authentication algorithm @ref
 *        CPA_CY_SYM_HASH_AES_XCBC.  This is because this algorithm requires
 *        that the final block be XORed with another value internally.
 *        If the length is zero, then the return code @ref
 *        CPA_STATUS_INVALID_PARAM will be returned.
 *
 *      - The length of the final block must be greater than or equal to
 *        16 bytes when using the @ref CPA_CY_SYM_CIPHER_AES_XTS cipher
 *        algorithm.
 *
 *      Partial packet processing is supported only when the following
 *      conditions are true:
 *
 *      - The cipher, hash or authentication operation is "in place" (that is,
 *        pDstBuffer == pSrcBuffer)
 *
 *      - The cipher or hash algorithm is NOT one of Kasumi or SNOW3G
 *
 *      - The cipher mode is NOT F8 mode.
 *
 *      - The hash algorithm is NOT SHAKE
 *
 *      - The cipher algorithm is not SM4
 *
 *      - The cipher algorithm is not CPA_CY_SYM_CIPHER_CHACHA and the hash
 *        algorithm is not CPA_CY_SYM_HASH_POLY.
 *
 *      - The cipher algorithm is not CPA_CY_SYM_CIPHER_AES_GCM and the hash
 *        algorithm is not CPA_CY_SYM_HASH_AES_GCM.
 *
 *      - The instance/implementation supports partial packets as one of
 *        its capabilities (see @ref CpaCySymCapabilitiesInfo).
 *
 *      The term "in-place" means that the result of the cryptographic
 *      operation is written into the source buffer.  The term "out-of-place"
 *      means that the result of the cryptographic operation is written into
 *      the destination buffer.  To perform "in-place" processing, set the
 *      pDstBuffer parameter to point at the same location as the pSrcBuffer
 *      parameter.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle   Instance handle.
 * @param[in]  pCallbackTag     Opaque data that will be returned to the client
 *                              in the callback.
 * @param[in]  pOpData          Pointer to a structure containing request
 *                              parameters. The client code allocates the memory
 *                              for this structure. This component takes
 *                              ownership of the memory until it is returned in
 *                              the callback.
 * @param[in]  pSrcBuffer       The source buffer.  The caller MUST allocate
 *                              the source buffer and populate it
 *                              with data. For optimum performance, the data
 *                              pointed to SHOULD be 8-byte aligned. For
 *                              block ciphers, the data passed in MUST be
 *                              a multiple of the relevant block size.
 *                              i.e. padding WILL NOT be applied to the data.
 *                              For optimum performance, the buffer should
 *                              only contain the data region that the
 *                              cryptographic operation(s) must be performed on.
 *                              Any additional data in the source buffer may be
 *                              copied to the destination buffer and this copy
 *                              may degrade performance.
 * @param[out] pDstBuffer       The destination buffer.  The caller MUST
 *                              allocate a sufficiently sized destination
 *                              buffer to hold the data output (including
 *                              the authentication tag in the case of CCM).
 *                              Furthermore, the destination buffer must be the
 *                              same size as the source buffer (i.e. the sum of
 *                              lengths of the buffers in the buffer list must
 *                              be the same).  This effectively means that the
 *                              source buffer must in fact be big enough to hold
 *                              the output data, too.  This is because,
 *                              for out-of-place processing, the data outside the
 *                              regions in the source buffer on which
 *                              cryptographic operations are performed are copied
 *                              into the destination buffer. To perform
 *                              "in-place" processing set the pDstBuffer
 *                              parameter in cpaCySymPerformOp function to point
 *                              at the same location as pSrcBuffer. For optimum
 *                              performance, the data pointed to SHOULD be
 *                              8-byte aligned.
 * @param[out] pVerifyResult    In synchronous mode, this parameter is returned
 *                              when the verifyDigest option is set in the
 *                              CpaCySymSessionSetupData structure. A value of
 *                              CPA_TRUE indicates that the compare succeeded. A
 *                              value of CPA_FALSE indicates that the compare
 *                              failed for an unspecified reason.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resource.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 *      A Cryptographic session has been previously setup using the
 *      @ref cpaCySymInitSession function call.
 * @post
 *      None
 *
 * @note
 *      When in asynchronous mode, a callback of type CpaCySymCbFunc is
 *      generated in response to this function call. Any errors generated during
 *      processing are reported as part of the callback status code.
 *
 * @see
 *      CpaCySymOpData,
 *      cpaCySymInitSession(),
 *      cpaCySymRemoveSession()
 *****************************************************************************/
CpaStatus
cpaCySymPerformOp(const CpaInstanceHandle instanceHandle,
        void *pCallbackTag,
        const CpaCySymOpData *pOpData,
        const CpaBufferList *pSrcBuffer,
        CpaBufferList *pDstBuffer,
        CpaBoolean *pVerifyResult);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Query symmetric cryptographic statistics for a specific instance.
 *
 * @deprecated
 *      As of v1.3 of the cryptographic API, this function has been
 *      deprecated, replaced by @ref cpaCySymQueryStats64().
 *
 * @description
 *      This function will query a specific instance for statistics. The
 *      user MUST allocate the CpaCySymStats structure and pass the
 *      reference to that into this function call. This function will write
 *      the statistic results into the passed in CpaCySymStats
 *      structure.
 *
 *      Note: statistics returned by this function do not interrupt current data
 *      processing and as such can be slightly out of sync with operations that
 *      are in progress during the statistics retrieval process.
 *
 * @context
 *      This is a synchronous function and it can sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle         Instance handle.
 * @param[out] pSymStats             Pointer to memory into which the
 *                                   statistics will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner, i.e. no asynchronous
 *      callback will be generated.
 * @see
 *      CpaCySymStats
 *****************************************************************************/
CpaStatus CPA_DEPRECATED
cpaCySymQueryStats(const CpaInstanceHandle instanceHandle,
        struct _CpaCySymStats *pSymStats);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Query symmetric cryptographic statistics (64-bit version) for a
 *      specific instance.
 *
 * @description
 *      This function will query a specific instance for statistics. The
 *      user MUST allocate the CpaCySymStats64 structure and pass the
 *      reference to that into this function call. This function will write
 *      the statistic results into the passed in CpaCySymStats64
 *      structure.
 *
 *      Note: statistics returned by this function do not interrupt current data
 *      processing and as such can be slightly out of sync with operations that
 *      are in progress during the statistics retrieval process.
 *
 * @context
 *      This is a synchronous function and it can sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle         Instance handle.
 * @param[out] pSymStats             Pointer to memory into which the
 *                                   statistics will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner, i.e. no asynchronous
 *      callback will be generated.
 * @see
 *      CpaCySymStats64
 *****************************************************************************/
CpaStatus
cpaCySymQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCySymStats64 *pSymStats);

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Symmetric Capabilities Info
 *
 * @description
 *      This structure contains the capabilities that vary across
 *      implementations of the symmetric sub-API of the cryptographic API.
 *      This structure is used in conjunction with @ref
 *      cpaCySymQueryCapabilities() to determine the capabilities supported
 *      by a particular API implementation.
 *
 *      For example, to see if an implementation supports cipher
 *      @ref CPA_CY_SYM_CIPHER_AES_CBC, use the code
 *
 * @code

if (CPA_BITMAP_BIT_TEST(capInfo.ciphers, CPA_CY_SYM_CIPHER_AES_CBC))
{
    // algo is supported
}
else
{
    // algo is not supported
}
 * @endcode
 *
 *      The client MUST allocate memory for this structure and any members
 *      that require memory.  When the structure is passed into the function
 *      ownership of the memory passes to the function. Ownership of the
 *      memory returns to the client when the function returns.
 *****************************************************************************/
typedef struct _CpaCySymCapabilitiesInfo
{
    CPA_BITMAP(ciphers, CPA_CY_SYM_CIPHER_CAP_BITMAP_SIZE);
    /**< Bitmap representing which cipher algorithms (and modes) are
     * supported by the instance.
     * Bits can be tested using the macro @ref CPA_BITMAP_BIT_TEST.
     * The bit positions are those specified in the enumerated type
     * @ref CpaCySymCipherAlgorithm. */
    CPA_BITMAP(hashes, CPA_CY_SYM_HASH_CAP_BITMAP_SIZE);
    /**< Bitmap representing which hash/authentication algorithms are
     * supported by the instance.
     * Bits can be tested using the macro @ref CPA_BITMAP_BIT_TEST.
     * The bit positions are those specified in the enumerated type
     * @ref CpaCySymHashAlgorithm. */
    CpaBoolean partialPacketSupported;
    /**< CPA_TRUE if instance supports partial packets.
     * See @ref CpaCySymPacketType. */
} CpaCySymCapabilitiesInfo;

/**
 *****************************************************************************
 * @ingroup cpaCySym
 *      Returns capabilities of the symmetric API group of a Cryptographic
 *      API instance.
 *
 * @description
 *      This function is used to determine which specific capabilities are
 *      supported within the symmetric sub-group of the Cryptographic API.
 *
 * @context
 *      The function shall not be called in an interrupt context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle        Handle to an instance of this API.
 * @param[out] pCapInfo              Pointer to capabilities info structure.
 *                                   All fields in the structure
 *                                   are populated by the API instance.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The instance has been initialized via the @ref cpaCyStartInstance
 *      function.
 * @post
 *      None
 *****************************************************************************/
CpaStatus
cpaCySymQueryCapabilities(const CpaInstanceHandle instanceHandle,
        CpaCySymCapabilitiesInfo * pCapInfo);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_CY_SYM_H */
