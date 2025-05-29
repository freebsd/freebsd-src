/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 *
 * @defgroup cpaCyEcsm2 Elliptic Curve SM2 (ECSM2) API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for Public Key Encryption
 *      (Cryptography) SM2 operations.
 *
 *      Chinese Public Key Algorithm based on Elliptic Curve Theory
 *
 * @note
 *      The naming, terms, and reference on SM2 elliptic curve, and their
 *      flow of algorithms inside this API header file are from the link
 *      below, please kindly refer to it for details.
 *      https://tools.ietf.org/html/draft-shen-sm2-ecdsa-02
 *
 *****************************************************************************/

#ifndef CPA_CY_ECSM2_H_
#define CPA_CY_ECSM2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"
#include "cpa_cy_ec.h"

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Encryption Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2Encrypt
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2Encrypt
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2Encrypt()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2EncryptOpData {
    CpaFlatBuffer k;
    /**< scalar multiplier  (k > 0 and k < n) */
    CpaFlatBuffer xP;
    /**< x coordinate of public key */
    CpaFlatBuffer yP;
    /**< y coordinate of public key */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2EncryptOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Decryption Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2Decrypt
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2Decrypt
 *      function, and before it has been returned in the callback, undefined
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2Decrypt
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2Decrypt()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2DecryptOpData {
    CpaFlatBuffer d;
    /**< private key  (d > 0 and d < n) */
    CpaFlatBuffer x1;
    /**< x coordinate of [k]G */
    CpaFlatBuffer y1;
    /**< y coordinate of [k]G */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2DecryptOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Point Multiplication Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2PointMultiply
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2PointMultiply
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2PointMultiply()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2PointMultiplyOpData {
    CpaFlatBuffer k;
    /**< scalar multiplier  (k > 0 and k < n) */
    CpaFlatBuffer x;
    /**< x coordinate of a point on the curve */
    CpaFlatBuffer y;
    /**< y coordinate of a point on the curve */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2PointMultiplyOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Generator Multiplication Operation Data.
 *
 * @description
 *      This structure contains the operation data for the
 *      cpaCyEcsm2GeneratorMultiply function. The client MUST allocate the
 *      memory for this structure and the items pointed to by this structure.
 *      When the structure is passed into the function, ownership of the
 *      memory passes to the function. Ownership of the memory returns to the
 *      client when this structure is returned in the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2GeneratorMultiply
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2GeneratorMultiply()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2GeneratorMultiplyOpData {
    CpaFlatBuffer k;
    /**< scalar multiplier  (k > 0 and k < n) */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2GeneratorMultiplyOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Point Verify Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2PointVerify
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2PointVerify
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2PointVerify()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2PointVerifyOpData {
    CpaFlatBuffer x;
    /**< x coordinate of a point on the curve */
    CpaFlatBuffer y;
    /**< y coordinate of a point on the curve */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2PointVerifyOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Signature Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2Sign
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2Sign
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2Sign()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2SignOpData {
    CpaFlatBuffer k;
    /**< scalar multiplier (k > 0 and k < n) */
    CpaFlatBuffer e;
    /**< digest of the message */
    CpaFlatBuffer d;
    /**< private key (d > 0 and d < n) */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2SignOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Signature Verify Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2Verify
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2Verify
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2Verify()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2VerifyOpData {
    CpaFlatBuffer e;
    /**< digest of the message */
    CpaFlatBuffer r;
    /**< signature r */
    CpaFlatBuffer s;
    /**< signature s */
    CpaFlatBuffer xP;
    /**< x coordinate of public key */
    CpaFlatBuffer yP;
    /**< y coordinate of public key */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2VerifyOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Key Exchange Phase 1 Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2KeyExPhase1
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2KeyExPhase1
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2KeyExPhase1()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2KeyExPhase1OpData {
    CpaFlatBuffer r;
    /**< scalar multiplier  (r > 0 and r < n) */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2KeyExPhase1OpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Key Exchange Phase 2 Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcsm2KeyExPhase2
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcsm2KeyExPhase2
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcsm2KeyExPhase2()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2KeyExPhase2OpData {
    CpaFlatBuffer r;
    /**< scalar multiplier  (r > 0 and r < n) */
    CpaFlatBuffer d;
    /**< private key (d > 0 and d < n) */
    CpaFlatBuffer x1;
    /**< x coordinate of a point on the curve from other side */
    CpaFlatBuffer x2;
    /**< x coordinate of a point on the curve from phase 1 */
    CpaFlatBuffer y2;
    /**< y coordinate of a point on the curve from phase 1 */
    CpaFlatBuffer xP;
    /**< x coordinate of public key from other side */
    CpaFlatBuffer yP;
    /**< y coordinate of public key from other side */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcsm2KeyExPhase2OpData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Encryption Output Data.
 *
 * @description
 *      This structure contains the output data of the cpaCyEcsm2Encrypt
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 * @see
 *      cpaCyEcsm2Encrypt()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2EncryptOutputData {
    CpaFlatBuffer x1;
    /**< x coordinate of [k]G */
    CpaFlatBuffer y1;
    /**< y coordinate of [k]G */
    CpaFlatBuffer x2;
    /**< x coordinate of [k]Pb */
    CpaFlatBuffer y2;
    /**< y coordinate of [k]Pb */
} CpaCyEcsm2EncryptOutputData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Decryption Output Data.
 *
 * @description
 *      This structure contains the output data of the cpaCyEcsm2Decrypt
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 * @see
 *      cpaCyEcsm2Decrypt()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2DecryptOutputData {
    CpaFlatBuffer x2;
    /**< x coordinate of [k]Pb */
    CpaFlatBuffer y2;
    /**< y coordinate of [k]Pb */
} CpaCyEcsm2DecryptOutputData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      SM2 Key Exchange (Phase 1 & Phase 2) Output Data.
 *
 * @description
 *      This structure contains the output data of the key exchange(phase 1 & 2)
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 * @see
 *      cpaCyEcsm2KeyExPhase1(),cpaCyEcsm2KeyExPhase2()
 *
 *****************************************************************************/
typedef struct _CpaCyEcsm2KeyExOutputData {
    CpaFlatBuffer x;
    /**< x coordinate of a point on the curve */
    CpaFlatBuffer y;
    /**< y coordinate of a point on the curve */
} CpaCyEcsm2KeyExOutputData;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      Cryptographic ECSM2 Statistics.
 * @description
 *      This structure contains statistics on the Cryptographic ECSM2
 *      operations. Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyEcsm2Stats64 {
    Cpa64U numEcsm2PointMultiplyRequests;
    /**< Total number of ECSM2 Point Multiplication operation requests. */
    Cpa64U numEcsm2PointMultiplyRequestErrors;
    /**< Total number of ECSM2 Point Multiplication operation requests that
     * had an error and could not be processed. */
    Cpa64U numEcsm2PointMultiplyCompleted;
    /**< Total number of ECSM2 Point Multiplication operation requests that
     * completed successfully. */
    Cpa64U numEcsm2PointMultiplyCompletedError;
    /**< Total number of ECSM2 Point Multiplication operation requests that
     * could not be completed successfully due to errors. */
    Cpa64U numEcsm2PointMultiplyCompletedOutputInvalid;
    /**< Total number of ECSM2 Point Multiplication or Point Verify operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */

    Cpa64U numEcsm2GeneratorMultiplyRequests;
    /**< Total number of ECSM2 Generator Multiplication operation requests. */
    Cpa64U numEcsm2GeneratorMultiplyRequestErrors;
    /**< Total number of ECSM2 Generator Multiplication operation requests that
     * had an error and could not be processed. */
    Cpa64U numEcsm2GeneratorMultiplyCompleted;
    /**< Total number of ECSM2 Generator Multiplication operation requests that
     * completed successfully. */
    Cpa64U numEcsm2GeneratorMultiplyCompletedError;
    /**< Total number of ECSM2 Generator Multiplication operation requests that
     * could not be completed successfully due to errors. */
    Cpa64U numEcsm2GeneratorMultiplyCompletedOutputInvalid;
    /**< Total number of ECSM2 Generator Multiplication or Point Verify
     * operation requests that could not be completed successfully due to an
     * invalid output. Note that this does not indicate an error. */

    Cpa64U numEcsm2PointVerifyRequests;
    /**< Total number of ECSM2 Point Verify operation requests. */
    Cpa64U numEcsm2PointVerifyRequestErrors;
    /**< Total number of ECSM2 Point Verify operation requests that had
     * an error and could not be processed. */
    Cpa64U numEcsm2PointVerifyCompleted;
    /**< Total number of ECSM2 Point Verify operation requests that
     * completed successfully. */
    Cpa64U numEcsm2PointVerifyCompletedError;
    /**< Total number of ECSM2 Point Verify operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcsm2PointVerifyCompletedOutputInvalid;
    /**< Total number of ECSM2 Point Verify operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */

    Cpa64U numEcsm2SignRequests;
    /**< Total number of ECSM2 Sign operation requests. */
    Cpa64U numEcsm2SignRequestErrors;
    /**< Total number of ECSM2 Sign operation requests that had an error
    * and could not be processed. */
    Cpa64U numEcsm2SignCompleted;
    /**< Total number of ECSM2 Sign operation requests that completed
     * successfully. */
    Cpa64U numEcsm2SignCompletedError;
    /**< Total number of ECSM2 Sign operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcsm2SignCompletedOutputInvalid;
    /**< Total number of ECSM2 Sign operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */

    Cpa64U numEcsm2VerifyRequests;
    /**< Total number of ECSM2 Verify operation requests. */
    Cpa64U numEcsm2VerifyRequestErrors;
    /**< Total number of ECSM2 Verify operation requests that had an error
     * and could not be processed. */
    Cpa64U numEcsm2VerifyCompleted;
    /**< Total number of ECSM2 Verify operation requests that completed
     * successfully. */
    Cpa64U numEcsm2VerifyCompletedError;
    /**< Total number of ECSM2 Verify operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcsm2VerifyCompletedOutputInvalid;
    /**< Total number of ECSM2 Verify operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */

    Cpa64U numEcsm2EncryptRequests;
    /**< Total number of ECSM2 Encryption requests. */
    Cpa64U numEcsm2EncryptRequestErrors;
    /**< Total number of ECSM2 Point Encryption requests that had
     * an error and could not be processed. */
    Cpa64U numEcsm2EncryptCompleted;
    /**< Total number of ECSM2 Encryption operation requests that
     * completed successfully. */
    Cpa64U numEcsm2EncryptCompletedError;
    /**< Total number of ECSM2 Encryption operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcsm2EncryptCompletedOutputInvalid;
    /**< Total number of ECSM2 Encryption operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */

    Cpa64U numEcsm2DecryptRequests;
    /**< Total number of ECSM2 Decryption operation requests. */
    Cpa64U numEcsm2DecryptRequestErrors;
    /**< Total number of ECSM2 Point Decryption requests that had
     * an error and could not be processed. */
    Cpa64U numEcsm2DecryptCompleted;
    /**< Total number of ECSM2 Decryption operation requests that
     * completed successfully. */
    Cpa64U numEcsm2DecryptCompletedError;
    /**< Total number of ECSM2 Decryption operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcsm2DecryptCompletedOutputInvalid;
    /**< Total number of ECSM2 Decryption operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */

    Cpa64U numEcsm2KeyExPhase1Requests;
    /**< Total number of ECSM2 Key Exchange Phase1 operation requests. */
    Cpa64U numEcsm2KeyExPhase1RequestErrors;
    /**< Total number of ECSM2 Key Exchange Phase1 operation requests that
     * had an error and could not be processed. */
    Cpa64U numEcsm2KeyExPhase1Completed;
    /**< Total number of ECSM2 Key Exchange Phase1 operation requests that
     * completed successfully. */
    Cpa64U numEcsm2KeyExPhase1CompletedError;
    /**< Total number of ECSM2 Key Exchange Phase1 operation requests that
     * could not be completed successfully due to errors. */
    Cpa64U numEcsm2KeyExPhase1CompletedOutputInvalid;
    /**< Total number of ECSM2 Key Exchange Phase1 operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */

    Cpa64U numEcsm2KeyExPhase2Requests;
    /**< Total number of ECSM2 Key Exchange Phase2 operation requests. */
    Cpa64U numEcsm2KeyExPhase2RequestErrors;
    /**< Total number of ECSM2 Key Exchange Phase2 operation requests that
     * had an error and could not be processed. */
    Cpa64U numEcsm2KeyExPhase2Completed;
    /**< Total number of ECSM2 Key Exchange Phase2 operation requests that
     * completed successfully. */
    Cpa64U numEcsm2KeyExPhase2CompletedError;
    /**< Total number of ECSM2 Key Exchange Phase2 operation requests that
     * could not be completed successfully due to errors. */
    Cpa64U numEcsm2KeyExPhase2CompletedOutputInvalid;
    /**< Total number of ECSM2 Key Exchange Phase2 operation
     * requests that could not be completed successfully due to an invalid
     * output. Note that this does not indicate an error. */
} CpaCyEcsm2Stats64;

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      Definition of callback function invoked for cpaCyEcsm2Sign
 *      requests.
 *
 * @description
 *      This is the callback function for:
 *      cpaCyEcsm2Sign
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
 * @param[in] pCallbackTag      User-supplied value to help identify request.
 * @param[in] status            Status of the operation. Valid values are
 *                              CPA_STATUS_SUCCESS and CPA_STATUS_FAIL.
 * @param[in] pOpData           A pointer to Operation data supplied in
 *                              request.
 * @param[in] pass              Indicate whether pOut is valid or not.
 *                              CPA_TRUE  == pass, pOut is valid
 *                              CPA_FALSE == pass, pOut is invalid
 * @param[in] pR                Ecsm2 message signature r.
 * @param[in] pS                Ecsm2 message signature s.
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
 *      cpaCyEcsm2GeneratorMultiply()
 *
 *****************************************************************************/
typedef void (*CpaCyEcsm2SignCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean pass,
        CpaFlatBuffer *pR,
        CpaFlatBuffer *pS);

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      Definition of callback function invoked for cpaCyEcsm2Verify requests.
 *
 * @description
 *      This is the prototype for the CpaCyEcsm2VerifyCbFunc callback function.
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
 * @param[in] pCallbackTag      User-supplied value to help identify request.
 * @param[in] status            Status of the operation. Valid values are
 *                              CPA_STATUS_SUCCESS and CPA_STATUS_FAIL.
 * @param[in] pOpData           Operation data pointer supplied in request.
 * @param[in] verifyStatus      The verification status.
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
 *      cpaCyEcsm2Verify()
 *
 *****************************************************************************/
typedef void (*CpaCyEcsm2VerifyCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean verifyStatus);

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Point Multiplication.
 *
 * @description
 *      This function performs SM2 Point Multiplication, multiply
 *      a point (P) by k (scalar) ([k]P).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pMultiplyStatus  Multiply status
 *                              CPA_TRUE  == pOutputData is valid
 *                              CPA_FALSE == pOutputData is invalid
 * @param[out] pXk              x coordinate of the resulting point
 *                              multiplication
 * @param[out] pYk              y coordinate of the resulting point
 *                              multiplication
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyEcsm2PointMultiplyCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2PointMultiplyOpData,
 *      CpaCyEcPointMultiplyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2PointMultiply(const CpaInstanceHandle instanceHandle,
                    const CpaCyEcPointMultiplyCbFunc pCb,
                    void *pCallbackTag,
                    const CpaCyEcsm2PointMultiplyOpData *pOpData,
                    CpaBoolean *pMultiplyStatus,
                    CpaFlatBuffer *pXk,
                    CpaFlatBuffer *pYk);

/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Generator Multiplication.
 *
 * @description
 *      This function performs SM2 Generator Multiplication, multiply the
 *      generator point (G) by k (scalar) ([k]G).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pMultiplyStatus  Multiply status
 *                              CPA_TRUE  == pOutputData is valid
 *                              CPA_FALSE == pOutputData is invalid
 * @param[out] pXk              x coordinate of the resulting point
 *                              multiplication
 * @param[out] pYk              y coordinate of the resulting point
 *                              multiplication
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyEcPointMultiplyCbFunc is generated in response to this function
 *      call. For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2GeneratorMultiplyOpData,
 *      CpaCyEcPointMultiplyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2GeneratorMultiply(const CpaInstanceHandle instanceHandle,
                    const CpaCyEcPointMultiplyCbFunc pCb,
                    void *pCallbackTag,
                    const CpaCyEcsm2GeneratorMultiplyOpData *pOpData,
                    CpaBoolean *pMultiplyStatus,
                    CpaFlatBuffer *pXk,
                    CpaFlatBuffer *pYk);

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Point Verify.
 *
 * @description
 *      This function performs SM2 Point Verify, to check if the input point
 *      on the curve or not.
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pVerifyStatus    Verification status
 *                              CPA_TRUE  == verify pass
 *                              CPA_FALSE == verify fail
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyEcsm2VerifyCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2PointVerifyOpData,
 *      CpaCyEcPointVerifyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2PointVerify(const CpaInstanceHandle instanceHandle,
        const CpaCyEcPointVerifyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcsm2PointVerifyOpData *pOpData,
        CpaBoolean *pVerifyStatus);

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Signature (Step A4 to A7).
 *
 * @description
 *      This function implements step A4 to A7 (in Section 5.2 in "Generation
 *      of Signature" Part 1).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pSignStatus      Signature status
 *                              CPA_TRUE  = pOutputData is valid
 *                              CPA_FALSE = pOutputData is invalid
 * @param[out] pR               R output of the resulting signature operation
 * @param[out] pS               S output of the resulting signature operation
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyEcsm2SignCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2SignOpData,
 *      CpaCyEcsm2SignCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2Sign(const CpaInstanceHandle instanceHandle,
               const CpaCyEcsm2SignCbFunc pCb,
               void *pCallbackTag,
               const CpaCyEcsm2SignOpData *pOpData,
               CpaBoolean *pSignStatus,
               CpaFlatBuffer *pR,
               CpaFlatBuffer *pS);

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Signature Verify (Step B5 to B7).
 *
 * @description
 *      This function implements step B5 to B7 (in Section 5.3 in "Verification
 *      of Signature" Part 1).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pVerifyStatus    Status of the signature verification
 *                              CPA_TRUE  == verify pass
 *                              CPA_FALSE == verify fail
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyEcsm2VerifyCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2VerifyOpData,
 *      CpaCyEcsm2VerifyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2Verify(const CpaInstanceHandle instanceHandle,
        const CpaCyEcsm2VerifyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcsm2VerifyOpData *pOpData,
        CpaBoolean *pVerifyStatus);

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Encryption (Step A2 to A4).
 *
 * @description
 *      This function implements step A2 to A4 (in Section 7.2 in
 *      "Algorithm for Encryption and the Flow Chart" Part 1).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pOutputData      Ecrypted message
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyGenFlatBufCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2EncryptOpData,
 *      CpaCyEcsm2EncryptOutputData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2Encrypt(const CpaInstanceHandle instanceHandle,
              const CpaCyGenFlatBufCbFunc pCb,
              void *pCallbackTag,
              const CpaCyEcsm2EncryptOpData *pOpData,
              CpaCyEcsm2EncryptOutputData *pOutputData);

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Decryption (Step B1 to B3).
 *
 * @description
 *      This function implements step B1 to B3 (in Section 7.3 in "Algorithm
 *      for Decryption and the Flow Chart" Part 1).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pOutputData      Decrypted message
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyGenFlatBufCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2DecryptOpData,
 *      CpaCyEcsm2DecryptOutputData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2Decrypt(const CpaInstanceHandle instanceHandle,
                    const CpaCyGenFlatBufCbFunc pCb,
                    void *pCallbackTag,
                    const CpaCyEcsm2DecryptOpData *pOpData,
                    CpaCyEcsm2DecryptOutputData *pOutputData);

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Key Exchange Phase 1 (Step A2/B2).
 *
 * @description
 *      This function implements step A2 (User A) or B2 (User B)
 *      (in Section 6.2 in "Key Exchange Protocol and the Flow Chart" Part 1).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pOutputData      Output of key exchange phase 1 ([r]G)
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyGenFlatBufCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2KeyExPhase1OpData,
 *      CpaCyEcsm2KeyExOutputData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2KeyExPhase1(const CpaInstanceHandle instanceHandle,
                    const CpaCyGenFlatBufCbFunc pCb,
                    void *pCallbackTag,
                    const CpaCyEcsm2KeyExPhase1OpData *pOpData,
                    CpaCyEcsm2KeyExOutputData *pOutputData);
/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEcsm2
 *      Perform SM2 Key Exchange Phase 2 (Step A4 to A7, B3 to B6).
 *
 * @description
 *      This function implements steps A4 to A7(User A) or B3 to B6(User B)
 *      (in Section 6.2 in "Key Exchange Protocol and the Flow Chart" Part 1).
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
 * @param[in]  pCb              Callback function pointer. If this is set to
 *                              a NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pOutputData      Output of key exchange phase2.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                     the request.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyGenFlatBufCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcsm2KeyExPhase2OpData,
 *      CpaCyEcsm2KeyExOutputData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcsm2KeyExPhase2(const CpaInstanceHandle instanceHandle,
                    const CpaCyGenFlatBufCbFunc pCb,
                    void *pCallbackTag,
                    const CpaCyEcsm2KeyExPhase2OpData *pOpData,
                    CpaCyEcsm2KeyExOutputData *pOutputData);
/**
 *****************************************************************************
 * @file cpa_cy_ecsm2.h
 * @ingroup cpaCyEcsm2
 *      Query statistics for a specific ECSM2 instance.
 *
 * @description
 *      This function will query a specific instance of the ECSM2 implementation
 *      for statistics. The user MUST allocate the CpaCyEcsm2Stats64 structure
 *      and pass the reference to that structure into this function call. This
 *      function writes the statistic results into the passed in
 *      CpaCyEcsm2Stats64 structure.
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
 * @param[out] pEcsm2Stats          Pointer to memory into which the statistics
 *                                  will be written.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 * @see
 *      CpaCyEcsm2Stats64
 *****************************************************************************/

CpaStatus
cpaCyEcsm2QueryStats64(const CpaInstanceHandle instanceHandle_in,
		CpaCyEcsm2Stats64 *pEcsm2Stats);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /*CPA_CY_ECSM2_H_*/
