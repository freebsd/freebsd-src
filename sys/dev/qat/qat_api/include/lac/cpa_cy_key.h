/***************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2023 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *
 ***************************************************************************/

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_cy_key.h
 *
 * @defgroup cpaCyKeyGen Cryptographic Key and Mask Generation API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for key and mask generation
 *      operations.
 *
 *****************************************************************************/

#ifndef CPA_CY_KEY_H
#define CPA_CY_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"
#include "cpa_cy_sym.h" /* needed for hash algorithm, for MGF */

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      SSL or TLS key generation random number length.
 *
 * @description
 *      Defines the permitted SSL or TLS random number length in bytes that
 *      may be used with the functions @ref cpaCyKeyGenSsl and @ref
 *      cpaCyKeyGenTls.   This is the length of the client or server random
 *      number values.
 *****************************************************************************/
#define CPA_CY_KEY_GEN_SSL_TLS_RANDOM_LEN_IN_BYTES   (32)

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      SSL Operation Types
 * @description
 *      Enumeration of the different SSL operations that can be specified in
 *      the struct @ref CpaCyKeyGenSslOpData.  It identifies the label.
 *****************************************************************************/
typedef enum _CpaCyKeySslOp
{
    CPA_CY_KEY_SSL_OP_MASTER_SECRET_DERIVE = 1,
    /**< Derive the master secret */
    CPA_CY_KEY_SSL_OP_KEY_MATERIAL_DERIVE,
    /**< Derive the key material */
    CPA_CY_KEY_SSL_OP_USER_DEFINED
    /**< User Defined Operation for custom labels*/
} CpaCyKeySslOp;


/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      SSL data for key generation functions
 * @description
 *      This structure contains data for use in key generation operations for
 *      SSL. For specific SSL key generation operations, the structure fields
 *      MUST be set as follows:
 *
 *      @par SSL Master-Secret Derivation:
 *          <br> sslOp = CPA_CY_KEY_SSL_OP_MASTER_SECRET_DERIVE
 *          <br> secret = pre-master secret key
 *          <br> seed = client_random + server_random
 *          <br> userLabel = NULL
 *
 *      @par SSL Key-Material Derivation:
 *          <br> sslOp = CPA_CY_KEY_SSL_OP_KEY_MATERIAL_DERIVE
 *          <br> secret = master secret key
 *          <br> seed = server_random + client_random
 *          <br> userLabel = NULL
 *
 *          <br> Note that the client/server random order is reversed from that
 *          used for master-secret derivation.
 *
 *      @note Each of the client and server random numbers need to be of
 *      length CPA_CY_KEY_GEN_SSL_TLS_RANDOM_LEN_IN_BYTES.
 *
 *      @note In each of the above descriptions, + indicates concatenation.
 *
 *      @note The label used is predetermined by the SSL operation in line
 *      with the SSL 3.0 specification, and can be overridden by using
 *      a user defined operation CPA_CY_KEY_SSL_OP_USER_DEFINED and
 *      associated userLabel.
 *
 ****************************************************************************/
typedef struct _CpaCyKeyGenSslOpData {
    CpaCyKeySslOp sslOp;
    /**< Indicate the SSL operation to be performed */
    CpaFlatBuffer secret;
    /**<  Flat buffer containing a pointer to either the master or pre-master
     * secret key. The length field indicates the length of the secret key in
     * bytes. Implementation-specific limits may apply to this length. */
    CpaFlatBuffer seed;
    /**<  Flat buffer containing a pointer to the seed data.
     * Implementation-specific limits may apply to this length. */
    CpaFlatBuffer info;
    /**<  Flat buffer containing a pointer to the info data.
     * Implementation-specific limits may apply to this length. */
    Cpa32U generatedKeyLenInBytes;
    /**< The requested length of the generated key in bytes.
     * Implementation-specific limits may apply to this length. */
    CpaFlatBuffer userLabel;
    /**<  Optional flat buffer containing a pointer to a user defined label.
     * The length field indicates the length of the label in bytes. To use this
     * field, the sslOp must be CPA_CY_KEY_SSL_OP_USER_DEFINED,
     * or otherwise it is ignored and can be set to NULL.
     * Implementation-specific limits may apply to this length. */
} CpaCyKeyGenSslOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      TLS Operation Types
 * @description
 *      Enumeration of the different TLS operations that can be specified in
 *      the CpaCyKeyGenTlsOpData.  It identifies the label.
 *
 *      The functions @ref cpaCyKeyGenTls and @ref cpaCyKeyGenTls2
 *      accelerate the TLS PRF, which is defined as part of RFC2246 (TLS
 *      v1.0), RFC4346 (TLS v1.1), and RFC5246 (TLS v1.2).
 *      One of the inputs to each of these functions is a label.
 *      This enumerated type defines values that correspond to some of
 *      the required labels.
 *      However, for some of the operations/labels required by these RFCs,
 *      no values are specified.
 *
 *      In such cases, a user-defined value must be provided.  The client
 *      should use the enum value @ref CPA_CY_KEY_TLS_OP_USER_DEFINED, and
 *      pass the label using the userLabel field of the @ref
 *      CpaCyKeyGenTlsOpData data structure.
 *
 *****************************************************************************/
typedef enum _CpaCyKeyTlsOp
{
    CPA_CY_KEY_TLS_OP_MASTER_SECRET_DERIVE = 1,
    /**< Derive the master secret using the TLS PRF.
     * Corresponds to RFC2246/5246 section 8.1, operation "Computing the
     * master secret", label "master secret". */
    CPA_CY_KEY_TLS_OP_KEY_MATERIAL_DERIVE,
    /**< Derive the key material using the TLS PRF.
     * Corresponds to RFC2246/5246 section 6.3, operation "Derive the key
     * material", label "key expansion". */
    CPA_CY_KEY_TLS_OP_CLIENT_FINISHED_DERIVE,
    /**< Derive the client finished tag using the TLS PRF.
     * Corresponds to RFC2246/5246 section 7.4.9, operation "Client finished",
     * label "client finished". */
    CPA_CY_KEY_TLS_OP_SERVER_FINISHED_DERIVE,
    /**< Derive the server finished tag using the TLS PRF.
     * Corresponds to RFC2246/5246 section 7.4.9, operation "Server finished",
     * label "server finished". */
    CPA_CY_KEY_TLS_OP_USER_DEFINED
    /**< User Defined Operation for custom labels. */

} CpaCyKeyTlsOp;


/**
 *****************************************************************************
 * @file cpa_cy_key.h
 * @ingroup cpaCyKeyGen
 *      TLS Operation Types
 * @description
 *      Enumeration of the different TLS operations that can be specified in
 *      the CpaCyKeyGenHKDFOpData.
 *
 *      The function @ref cpaCyKeyGenTls3
 *      accelerates the TLS HKDF, which is defined as part of RFC5869 (HKDF)
 *      and RFC8446 (TLS v1.3).
 *
 *      This enumerated type defines the support HKDF operations for
 *      extraction and expansion of keying material.
 *
 *****************************************************************************/
typedef enum _CpaCyKeyHKDFOp
{
    CPA_CY_HKDF_KEY_EXTRACT = 12,
    /**< HKDF Extract operation
     * Corresponds to RFC5869 section 2.2, step 1 "Extract" */
    CPA_CY_HKDF_KEY_EXPAND,
    /**< HKDF Expand operation
     * Corresponds to RFC5869 section 2.3, step 2 "Expand" */
    CPA_CY_HKDF_KEY_EXTRACT_EXPAND,
    /**< HKDF operation
     * This performs HKDF_EXTRACT and HKDF_EXPAND in a single
     * API invocation. */
    CPA_CY_HKDF_KEY_EXPAND_LABEL ,
    /**< HKDF Expand label operation for TLS 1.3
     * Corresponds to RFC8446 section 7.1 Key Schedule definition for
     * HKDF-Expand-Label, which refers to HKDF-Expand defined in RFC5869. */
    CPA_CY_HKDF_KEY_EXTRACT_EXPAND_LABEL
    /**< HKDF Extract plus Expand label operation for TLS 1.3
     * Corresponds to  RFC5869 section 2.2, step 1 "Extract" followed by
     * RFC8446 section 7.1 Key Schedule definition for
     * HKDF-Expand-Label, which refers to HKDF-Expand defined in RFC5869. */
} CpaCyKeyHKDFOp;


/**
 *****************************************************************************
 * @file cpa_cy_key.h
 * @ingroup cpaCyKeyGen
 *      TLS Operation Types
 * @description
 *      Enumeration of the different cipher suites that may be used in a TLS
 *      v1.3 operation.  This value is used to infer the sizes of the key
 *      and iv sublabel.
 *
 *      The function @ref cpaCyKeyGenTls3
 *      accelerates the TLS HKDF, which is defined as part of RFC5869 (HKDF)
 *      and RFC8446 (TLS v1.3).
 *
 *      This enumerated type defines the supported cipher suites in the
 *      TLS operation that require HKDF key operations.
 *
 *****************************************************************************/
typedef enum _CpaCyKeyHKDFCipherSuite
{
    CPA_CY_HKDF_TLS_AES_128_GCM_SHA256 = 1,
    CPA_CY_HKDF_TLS_AES_256_GCM_SHA384,
    CPA_CY_HKDF_TLS_CHACHA20_POLY1305_SHA256 ,
    CPA_CY_HKDF_TLS_AES_128_CCM_SHA256,
    CPA_CY_HKDF_TLS_AES_128_CCM_8_SHA256
} CpaCyKeyHKDFCipherSuite;


/**
 *****************************************************************************
 * @file cpa_cy_key.h
 * @ingroup cpaCyKeyGen
 *      TLS Operation Types
 * @description
 *      Bitwise constants for HKDF sublabels
 *
 *      These definitions provide bit settings for sublabels for
 *      HKDF-ExpandLabel operations.
 *
 *      <br> key             sublabel to generate "key" keying material
 *      <br> iv              sublabel to generate "iv" keying material
 *      <br> resumption      sublabel to generate "resumption" keying material
 *      <br> finished        sublabel to generate "finished" keying material
 *
 *****************************************************************************/

#define    CPA_CY_HKDF_SUBLABEL_KEY                   ((Cpa16U)0x0001)
        /**< Bit for creation of key material for 'key' sublabel */
#define    CPA_CY_HKDF_SUBLABEL_IV                    ((Cpa16U)0x0002)
        /**< Bit for creation of key material for 'iv' sublabel */
#define    CPA_CY_HKDF_SUBLABEL_RESUMPTION            ((Cpa16U)0x0004)
        /**< Bit for creation of key material for 'resumption' sublabel */
#define    CPA_CY_HKDF_SUBLABEL_FINISHED              ((Cpa16U)0x0008)
        /**< Bit for creation of key material for 'finished' sublabel */

#define CPA_CY_HKDF_KEY_MAX_SECRET_SZ   ((Cpa8U)80)
        /** space in bytes PSK or (EC)DH */
#define CPA_CY_HKDF_KEY_MAX_HMAC_SZ     ((Cpa8U)48)
        /** space in bytes of CPA_CY_SYM_HASH_SHA384 result */
#define CPA_CY_HKDF_KEY_MAX_INFO_SZ     ((Cpa8U)80)
        /** space in bytes of largest info needed for TLS 1.3,
          * rounded up to multiple of 8 */
#define CPA_CY_HKDF_KEY_MAX_LABEL_SZ    ((Cpa8U)78)
        /** space in bytes of largest label for TLS 1.3 */
#define CPA_CY_HKDF_KEY_MAX_LABEL_COUNT ((Cpa8U)4)
        /** Maximum number of labels in op structure */

/**
 *****************************************************************************
 * @file cpa_cy_key.h
 * @ingroup cpaCyKeyGen
 *      TLS data for key generation functions
 * @description
 *      This structure contains data for describing label for the
 *      HKDF Extract Label function
 *
 *      @par Extract Label Function
 *          <br> labelLen = length of the label field
 *          <br> contextLen = length of the context field
 *          <br> sublabelFlag = Mask of sub labels required for this label.
 *          <br> label = label as defined in RFC8446
 *          <br> context = context as defined in RFC8446
 *
 ****************************************************************************/
typedef struct _CpaCyKeyGenHKDFExpandLabel
{
    Cpa8U label[CPA_CY_HKDF_KEY_MAX_LABEL_SZ];
    /**< HKDFLabel field as defined in RFC8446 sec 7.1.
      */
    Cpa8U   labelLen;
    /**< The length, in bytes of the label */
    Cpa8U   sublabelFlag;
    /**< mask of sublabels to be generated.
      *  This flag is composed of zero or more of:
      *    CPA_CY_HKDF_SUBLABEL_KEY
      *    CPA_CY_HKDF_SUBLABEL_IV
      *    CPA_CY_HKDF_SUBLABEL_RESUMPTION
      *    CPA_CY_HKDF_SUBLABEL_FINISHED
      */
} CpaCyKeyGenHKDFExpandLabel;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      TLS data for key generation functions
 * @description
 *      This structure contains data for all HKDF operations:
 *          <br> HKDF Extract
 *          <br> HKDF Expand
 *          <br> HKDF Expand Label
 *          <br> HKDF Extract and Expand
 *          <br> HKDF Extract and Expand Label
 *
 *      @par HKDF Map Structure Elements
 *          <br> secret - IKM value for extract operations or PRK for expand
 *                        or expand operations.
 *          <br> seed -   contains the salt for extract
 *                        operations
 *          <br> info -   contains the info data for extract operations
 *          <br> labels - See notes above
 *
 ****************************************************************************/
typedef struct _CpaCyKeyGenHKDFOpData
{
    CpaCyKeyHKDFOp hkdfKeyOp;
    /**< Keying operation to be performed. */
    Cpa8U secretLen;
    /**< Length of secret field */
    Cpa16U seedLen;
    /**< Length of seed field */
    Cpa16U infoLen;
    /**< Length of info field */
    Cpa16U numLabels;
    /**< Number of filled CpaCyKeyGenHKDFExpandLabel elements */
    Cpa8U secret[CPA_CY_HKDF_KEY_MAX_SECRET_SZ];
    /**< Input Key Material or PRK */
    Cpa8U seed[CPA_CY_HKDF_KEY_MAX_HMAC_SZ];
    /**< Input salt */
    Cpa8U info[CPA_CY_HKDF_KEY_MAX_INFO_SZ];
    /**< info field */
    CpaCyKeyGenHKDFExpandLabel label[CPA_CY_HKDF_KEY_MAX_LABEL_COUNT];
    /**< array of Expand Label structures */
} CpaCyKeyGenHKDFOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      TLS data for key generation functions
 * @description
 *      This structure contains data for use in key generation operations for
 *      TLS. For specific TLS key generation operations, the structure fields
 *      MUST be set as follows:
 *
 *      @par TLS Master-Secret Derivation:
 *          <br> tlsOp = CPA_CY_KEY_TLS_OP_MASTER_SECRET_DERIVE
 *          <br> secret = pre-master secret key
 *          <br> seed = client_random + server_random
 *          <br> userLabel = NULL
 *
 *      @par TLS Key-Material Derivation:
 *          <br> tlsOp = CPA_CY_KEY_TLS_OP_KEY_MATERIAL_DERIVE
 *          <br> secret = master secret key
 *          <br> seed = server_random + client_random
 *          <br> userLabel = NULL
 *
 *          <br> Note that the client/server random order is reversed from
 *          that used for Master-Secret Derivation.
 *
 *      @par TLS Client finished/Server finished tag Derivation:
 *          <br> tlsOp = CPA_CY_KEY_TLS_OP_CLIENT_FINISHED_DERIVE  (client)
 *          <br>      or CPA_CY_KEY_TLS_OP_SERVER_FINISHED_DERIVE  (server)
 *          <br> secret = master secret key
 *          <br> seed = MD5(handshake_messages) + SHA-1(handshake_messages)
 *          <br> userLabel = NULL
 *
 *      @note Each of the client and server random seeds need to be of
 *      length CPA_CY_KEY_GEN_SSL_TLS_RANDOM_LEN_IN_BYTES.
 *      @note In each of the above descriptions, + indicates concatenation.
 *      @note The label used is predetermined by the TLS operation in line
 *      with the TLS specifications, and can be overridden by using
 *      a user defined operation CPA_CY_KEY_TLS_OP_USER_DEFINED
 *      and associated userLabel.
 *
 ****************************************************************************/
typedef struct _CpaCyKeyGenTlsOpData {
    CpaCyKeyTlsOp tlsOp;
    /**< TLS operation to be performed */
    CpaFlatBuffer secret;
    /**< Flat buffer containing a pointer to either the master or pre-master
     * secret key. The length field indicates the length of the secret in
     * bytes.  */
    CpaFlatBuffer seed;
    /**< Flat buffer containing a pointer to the seed data.
     * Implementation-specific limits may apply to this length. */
    Cpa32U generatedKeyLenInBytes;
    /**< The requested length of the generated key in bytes.
     * Implementation-specific limits may apply to this length. */
    CpaFlatBuffer userLabel;
    /**< Optional flat buffer containing a pointer to a user defined label.
     * The length field indicates the length of the label in bytes. To use this
     * field, the tlsOp must be CPA_CY_KEY_TLS_OP_USER_DEFINED.
         * Implementation-specific limits may apply to this length. */
} CpaCyKeyGenTlsOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Key Generation Mask Generation Function (MGF) Data
 * @description
 *      This structure contains data relating to Mask Generation Function
 *      key generation operations.
 *
 *      @note The default hash algorithm used by the MGF is SHA-1.  If a
 *      different hash algorithm is preferred, then see the extended
 *      version of this structure, @ref CpaCyKeyGenMgfOpDataExt.
 * @see
 *        cpaCyKeyGenMgf
 ****************************************************************************/
typedef struct _CpaCyKeyGenMgfOpData {
    CpaFlatBuffer seedBuffer;
    /**<  Caller MUST allocate a buffer and populate with the input seed
     * data. For optimal performance the start of the seed SHOULD be allocated
     * on an 8-byte boundary. The length field represents the seed length in
     * bytes.  Implementation-specific limits may apply to this length. */
    Cpa32U maskLenInBytes;
    /**< The requested length of the generated mask in bytes.
     * Implementation-specific limits may apply to this length. */
} CpaCyKeyGenMgfOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Extension to the original Key Generation Mask Generation Function
 *      (MGF) Data
 * @description
 *      This structure is an extension to the original MGF data structure.
 *      The extension allows the hash function to be specified.
 * @note
 *      This structure is separate from the base @ref CpaCyKeyGenMgfOpData
 *      structure in order to retain backwards compatibility with the
 *      original version of the API.
 * @see
 *        cpaCyKeyGenMgfExt
 ****************************************************************************/
typedef struct _CpaCyKeyGenMgfOpDataExt {
    CpaCyKeyGenMgfOpData baseOpData;
    /**<  "Base" operational data for MGF generation */
    CpaCySymHashAlgorithm hashAlgorithm;
    /**< Specifies the hash algorithm to be used by the Mask Generation
     * Function */
} CpaCyKeyGenMgfOpDataExt;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Key Generation Statistics.
 * @deprecated
 *      As of v1.3 of the Crypto API, this structure has been deprecated,
 *      replaced by @ref CpaCyKeyGenStats64.
 * @description
 *      This structure contains statistics on the key and mask generation
 *      operations. Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyKeyGenStats {
    Cpa32U numSslKeyGenRequests;
    /**< Total number of successful SSL key generation requests. */
    Cpa32U numSslKeyGenRequestErrors;
    /**< Total number of SSL key generation requests that had an error and
     *   could not be processed. */
    Cpa32U numSslKeyGenCompleted;
    /**< Total number of SSL key generation operations that completed
     *   successfully. */
    Cpa32U numSslKeyGenCompletedErrors;
    /**< Total number of SSL key generation operations that could not be
     *   completed successfully due to errors. */
    Cpa32U numTlsKeyGenRequests;
    /**< Total number of successful TLS key generation requests. */
    Cpa32U numTlsKeyGenRequestErrors;
    /**< Total number of TLS key generation requests that had an error and
     *   could not be processed. */
    Cpa32U numTlsKeyGenCompleted;
    /**< Total number of TLS key generation operations that completed
     *   successfully. */
    Cpa32U numTlsKeyGenCompletedErrors;
    /**< Total number of TLS key generation operations that could not be
     *   completed successfully due to errors. */
    Cpa32U numMgfKeyGenRequests;
    /**< Total number of successful MGF key generation requests (including
     *   "extended" MGF requests). */
    Cpa32U numMgfKeyGenRequestErrors;
    /**< Total number of MGF key generation requests that had an error and
     *   could not be processed. */
    Cpa32U numMgfKeyGenCompleted;
    /**< Total number of MGF key generation operations that completed
     *   successfully. */
    Cpa32U numMgfKeyGenCompletedErrors;
    /**< Total number of MGF key generation operations that could not be
     *   completed successfully due to errors. */
} CpaCyKeyGenStats CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Key Generation Statistics (64-bit version).
 * @description
 *      This structure contains the 64-bit version of the statistics
 *      on the key and mask generation operations.
 *      Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyKeyGenStats64 {
    Cpa64U numSslKeyGenRequests;
    /**< Total number of successful SSL key generation requests. */
    Cpa64U numSslKeyGenRequestErrors;
    /**< Total number of SSL key generation requests that had an error and
     *   could not be processed. */
    Cpa64U numSslKeyGenCompleted;
    /**< Total number of SSL key generation operations that completed
     *   successfully. */
    Cpa64U numSslKeyGenCompletedErrors;
    /**< Total number of SSL key generation operations that could not be
     *   completed successfully due to errors. */
    Cpa64U numTlsKeyGenRequests;
    /**< Total number of successful TLS key generation requests. */
    Cpa64U numTlsKeyGenRequestErrors;
    /**< Total number of TLS key generation requests that had an error and
     *   could not be processed. */
    Cpa64U numTlsKeyGenCompleted;
    /**< Total number of TLS key generation operations that completed
     *   successfully. */
    Cpa64U numTlsKeyGenCompletedErrors;
    /**< Total number of TLS key generation operations that could not be
     *   completed successfully due to errors. */
    Cpa64U numMgfKeyGenRequests;
    /**< Total number of successful MGF key generation requests (including
     *   "extended" MGF requests). */
    Cpa64U numMgfKeyGenRequestErrors;
    /**< Total number of MGF key generation requests that had an error and
     *   could not be processed. */
    Cpa64U numMgfKeyGenCompleted;
    /**< Total number of MGF key generation operations that completed
     *   successfully. */
    Cpa64U numMgfKeyGenCompletedErrors;
    /**< Total number of MGF key generation operations that could not be
     *   completed successfully due to errors. */
} CpaCyKeyGenStats64;

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      SSL Key Generation Function.
 * @description
 *      This function is used for SSL key generation.  It implements the key
 *      generation function defined in section 6.2.2 of the SSL 3.0
 *      specification as described in
 *      http://www.mozilla.org/projects/security/pki/nss/ssl/draft302.txt.
 *
 *      The input seed is taken as a flat buffer and the generated key is
 *      returned to caller in a flat destination data buffer.
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
 * @param[in] instanceHandle             Instance handle.
 * @param[in] pKeyGenCb                  Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in] pCallbackTag               Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in] pKeyGenSslOpData           Structure containing all the data
 *                                       needed to perform the SSL key
 *                                       generation operation. The client code
 *                                       allocates the memory for this
 *                                       structure. This component takes
 *                                       ownership of the memory until it is
 *                                       returned in the callback.
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 * @retval CPA_STATUS_RESTARTING         API implementation is restarting.
 *                                       Resubmit the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @see
 *      CpaCyKeyGenSslOpData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyKeyGenSsl(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pKeyGenCb,
        void *pCallbackTag,
        const CpaCyKeyGenSslOpData *pKeyGenSslOpData,
        CpaFlatBuffer *pGeneratedKeyBuffer);

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      TLS Key Generation Function.
 * @description
 *      This function is used for TLS key generation.  It implements the
 *      TLS PRF (Pseudo Random Function) as defined by RFC2246 (TLS v1.0)
 *      and RFC4346 (TLS v1.1).
 *
 *      The input seed is taken as a flat buffer and the generated key is
 *      returned to caller in a flat destination data buffer.
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
 * @param[in]  instanceHandle            Instance handle.
 * @param[in]  pKeyGenCb                 Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in]  pCallbackTag              Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in]  pKeyGenTlsOpData          Structure containing all the data
 *                                       needed to perform the TLS key
 *                                       generation operation. The client code
 *                                       allocates the memory for this
 *                                       structure. This component takes
 *                                       ownership of the memory until it is
 *                                       returned in the callback.
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 * @retval CPA_STATUS_RESTARTING         API implementation is restarting.
 *                                       Resubmit the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @see
 *      CpaCyKeyGenTlsOpData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyKeyGenTls(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pKeyGenCb,
        void *pCallbackTag,
        const CpaCyKeyGenTlsOpData *pKeyGenTlsOpData,
        CpaFlatBuffer *pGeneratedKeyBuffer);

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      TLS Key Generation Function version 2.
 * @description
 *      This function is used for TLS key generation.  It implements the
 *      TLS PRF (Pseudo Random Function) as defined by RFC5246 (TLS v1.2).
 *
 *      The input seed is taken as a flat buffer and the generated key is
 *      returned to caller in a flat destination data buffer.
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
 * @param[in]  instanceHandle            Instance handle.
 * @param[in]  pKeyGenCb                 Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in]  pCallbackTag              Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in]  pKeyGenTlsOpData          Structure containing all the data
 *                                       needed to perform the TLS key
 *                                       generation operation. The client code
 *                                       allocates the memory for this
 *                                       structure. This component takes
 *                                       ownership of the memory until it is
 *                                       returned in the callback.
 * @param[in]  hashAlgorithm             Specifies the hash algorithm to use.
 *                                       According to RFC5246, this should be
 *                                       "SHA-256 or a stronger standard hash
 *                                       function."
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 * @retval CPA_STATUS_RESTARTING         API implementation is restarting.
 *                                       Resubmit the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @see
 *      CpaCyKeyGenTlsOpData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyKeyGenTls2(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pKeyGenCb,
        void *pCallbackTag,
        const CpaCyKeyGenTlsOpData *pKeyGenTlsOpData,
        CpaCySymHashAlgorithm hashAlgorithm,
        CpaFlatBuffer *pGeneratedKeyBuffer);


/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      TLS Key Generation Function version 3.
 * @description
 *      This function is used for TLS key generation.  It implements the
 *      TLS HKDF (HMAC Key Derivation Function) as defined by
 *      RFC5689 (HKDF) and RFC8446 (TLS 1.3).
 *
 *      The input seed is taken as a flat buffer and the generated key is
 *      returned to caller in a flat destination data buffer.
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
 * @param[in]  instanceHandle            Instance handle.
 * @param[in]  pKeyGenCb                 Pointer to callback function to be
 *                                       invoked when the operation is complete.
 *                                       If this is set to a NULL value the
 *                                       function will operate synchronously.
 * @param[in]  pCallbackTag              Opaque User Data for this specific
 *                                       call. Will be returned unchanged in the
 *                                       callback.
 * @param[in]  pKeyGenTlsOpData          Structure containing all the data
 *                                       needed to perform the TLS key
 *                                       generation operation. The client code
 *                                       allocates the memory for this
 *                                       structure. This component takes
 *                                       ownership of the memory until it is
 *                                       returned in the callback. The memory
 *                                       must be pinned and contiguous, suitable
 *                                       for DMA operations.
 * @param[in]  hashAlgorithm             Specifies the hash algorithm to use.
 *                                       According to RFC5246, this should be
 *                                       "SHA-256 or a stronger standard hash
 *                                       function."
 * @param[out] pGeneratedKeyBuffer       Caller MUST allocate a sufficient
 *                                       buffer to hold the key generation
 *                                       output. The data pointer SHOULD be
 *                                       aligned on an 8-byte boundary. The
 *                                       length field passed in represents the
 *                                       size of the buffer in bytes. The value
 *                                       that is returned is the size of the
 *                                       result key in bytes.
 *                                       On invocation the callback function
 *                                       will contain this parameter in the
 *                                       pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS            Function executed successfully.
 * @retval CPA_STATUS_FAIL               Function failed.
 * @retval CPA_STATUS_RETRY              Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM      Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE           Error related to system resources.
 * @retval CPA_STATUS_RESTARTING         API implementation is restarting.
 *                                       Resubmit the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @see
 *      CpaCyGenFlatBufCbFunc
 *      CpaCyKeyGenHKDFOpData
 *
 *****************************************************************************/
CpaStatus
cpaCyKeyGenTls3(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pKeyGenCb,
        void *pCallbackTag,
        const CpaCyKeyGenHKDFOpData *pKeyGenTlsOpData,
        CpaCyKeyHKDFCipherSuite cipherSuite,
        CpaFlatBuffer *pGeneratedKeyBuffer);


/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Mask Generation Function.
 * @description
 *      This function implements the mask generation function MGF1 as
 *      defined by PKCS#1 v2.1, and RFC3447.  The input seed is taken
 *      as a flat buffer and the generated mask is returned to caller in a
 *      flat destination data buffer.
 *
 *      @note The default hash algorithm used by the MGF is SHA-1.  If a
 *      different hash algorithm is preferred, then see the "extended"
 *      version of this function, @ref cpaCyKeyGenMgfExt.
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
 * @param[in]  instanceHandle        Instance handle.
 * @param[in]  pKeyGenCb             Pointer to callback function to be
 *                                   invoked when the operation is complete.
 *                                   If this is set to a NULL value the
 *                                   function will operate synchronously.
 * @param[in]  pCallbackTag          Opaque User Data for this specific call.
 *                                   Will be returned unchanged in the
 *                                   callback.
 * @param[in]  pKeyGenMgfOpData      Structure containing all the data needed
 *                                   to perform the MGF key generation
 *                                   operation. The client code allocates the
 *                                   memory for this structure. This
 *                                   component takes ownership of the memory
 *                                   until it is returned in the callback.
 * @param[out] pGeneratedMaskBuffer  Caller MUST allocate a sufficient buffer
 *                                   to hold the generated mask. The data
 *                                   pointer SHOULD be aligned on an 8-byte
 *                                   boundary. The length field passed in
 *                                   represents the size of the buffer in
 *                                   bytes. The value that is returned is the
 *                                   size of the generated mask in bytes.
 *                                   On invocation the callback function
 *                                   will contain this parameter in the
 *                                   pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS           Function executed successfully.
 * @retval CPA_STATUS_FAIL              Function failed.
 * @retval CPA_STATUS_RETRY             Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM     Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE          Error related to system resources.
 * @retval CPA_STATUS_RESTARTING        API implementation is restarting.
 *                                      Resubmit the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @see
 *      CpaCyKeyGenMgfOpData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyKeyGenMgf(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pKeyGenCb,
        void *pCallbackTag,
        const CpaCyKeyGenMgfOpData *pKeyGenMgfOpData,
        CpaFlatBuffer *pGeneratedMaskBuffer);

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Extended Mask Generation Function.
 * @description
 *      This function is used for mask generation. It differs from the "base"
 *      version of the function (@ref cpaCyKeyGenMgf) in that it allows
 *      the hash function used by the Mask Generation Function to be
 *      specified.
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
 * @param[in]  instanceHandle        Instance handle.
 * @param[in]  pKeyGenCb             Pointer to callback function to be
 *                                   invoked when the operation is complete.
 *                                   If this is set to a NULL value the
 *                                   function will operate synchronously.
 * @param[in]  pCallbackTag          Opaque User Data for this specific call.
 *                                   Will be returned unchanged in the
 *                                   callback.
 * @param[in]  pKeyGenMgfOpDataExt   Structure containing all the data needed
 *                                   to perform the extended MGF key generation
 *                                   operation. The client code allocates the
 *                                   memory for this structure. This
 *                                   component takes ownership of the memory
 *                                   until it is returned in the callback.
 * @param[out] pGeneratedMaskBuffer  Caller MUST allocate a sufficient buffer
 *                                   to hold the generated mask. The data
 *                                   pointer SHOULD be aligned on an 8-byte
 *                                   boundary. The length field passed in
 *                                   represents the size of the buffer in
 *                                   bytes. The value that is returned is the
 *                                   size of the generated mask in bytes.
 *                                   On invocation the callback function
 *                                   will contain this parameter in the
 *                                   pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS           Function executed successfully.
 * @retval CPA_STATUS_FAIL              Function failed.
 * @retval CPA_STATUS_RETRY             Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM     Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE          Error related to system resources.
 * @retval CPA_STATUS_RESTARTING        API implementation is restarting.
 *                                      Resubmit the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      This function is only used to generate a mask keys from seed
 *      material.
 * @see
 *      CpaCyKeyGenMgfOpData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyKeyGenMgfExt(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pKeyGenCb,
        void *pCallbackTag,
        const CpaCyKeyGenMgfOpDataExt *pKeyGenMgfOpDataExt,
        CpaFlatBuffer *pGeneratedMaskBuffer);

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Queries the Key and Mask generation statistics specific to
 *      an instance.
 *
 * @deprecated
 *      As of v1.3 of the Crypto API, this function has been deprecated,
 *      replaced by @ref cpaCyKeyGenQueryStats64().
 *
 * @description
 *      This function will query a specific instance for key and mask
 *      generation statistics. The user MUST allocate the CpaCyKeyGenStats
 *      structure and pass the reference to that into this function call. This
 *      function will write the statistic results into the passed in
 *      CpaCyKeyGenStats structure.
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
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle       Instance handle.
 * @param[out] pKeyGenStats         Pointer to memory into which the statistics
 *                                  will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting.
 *                                   Resubmit the request.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 *
 * @see
 *      CpaCyKeyGenStats
 *
 *****************************************************************************/
CpaStatus CPA_DEPRECATED
cpaCyKeyGenQueryStats(const CpaInstanceHandle instanceHandle,
        struct _CpaCyKeyGenStats *pKeyGenStats);

/**
 *****************************************************************************
 * @ingroup cpaCyKeyGen
 *      Queries the Key and Mask generation statistics (64-bit version)
 *      specific to an instance.
 *
 * @description
 *      This function will query a specific instance for key and mask
 *      generation statistics. The user MUST allocate the CpaCyKeyGenStats64
 *      structure and pass the reference to that into this function call. This
 *      function will write the statistic results into the passed in
 *      CpaCyKeyGenStats64 structure.
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
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle       Instance handle.
 * @param[out] pKeyGenStats         Pointer to memory into which the statistics
 *                                  will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting.
 *                                   Resubmit the request.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 *
 * @see
 *      CpaCyKeyGenStats64
 *****************************************************************************/
CpaStatus
cpaCyKeyGenQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCyKeyGenStats64 *pKeyGenStats);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_CY_KEY_H */
