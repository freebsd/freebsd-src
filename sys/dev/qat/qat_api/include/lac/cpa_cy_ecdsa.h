/***************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2022 Intel Corporation. All rights reserved.
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
 * @file cpa_cy_ecdsa.h
 *
 * @defgroup cpaCyEcdsa Elliptic Curve Digital Signature Algorithm (ECDSA) API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for Public Key Encryption
 *      (Cryptography) Elliptic Curve Digital Signature Algorithm (ECDSA)
 *      operations.
 *
 * @note
 *      Large numbers are represented on the QuickAssist API as described
 *      in the Large Number API (@ref cpaCyLn).
 *
 *      In addition, the bit length of large numbers passed to the API
 *      MUST NOT exceed 576 bits for Elliptic Curve operations.
 *****************************************************************************/

#ifndef CPA_CY_ECDSA_H_
#define CPA_CY_ECDSA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"
#include "cpa_cy_ec.h"

/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      ECDSA Sign R Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyEcdsaSignR
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
 *      structure after it has been submitted to the cpaCyEcdsaSignR
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcdsaSignR()
 *
 *****************************************************************************/
typedef struct _CpaCyEcdsaSignROpData {
    CpaFlatBuffer xg;
    /**< x coordinate of base point G */
    CpaFlatBuffer yg;
    /**< y coordinate of base point G */
    CpaFlatBuffer n;
    /**< order of the base point G, which shall be prime */
    CpaFlatBuffer q;
    /**< prime modulus or irreducible polynomial over GF(2^r) */
    CpaFlatBuffer a;
    /**< a elliptic curve coefficient */
    CpaFlatBuffer b;
    /**< b elliptic curve coefficient */
    CpaFlatBuffer k;
    /**< random value (k > 0 and k < n) */

    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcdsaSignROpData;


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      ECDSA Sign S Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyEcdsaSignS
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
 *      structure after it has been submitted to the cpaCyEcdsaSignS
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcdsaSignS()
 *
 *****************************************************************************/
typedef struct _CpaCyEcdsaSignSOpData {
    CpaFlatBuffer m;
    /**< digest of the message to be signed */
    CpaFlatBuffer d;
    /**< private key */
    CpaFlatBuffer r;
    /**< Ecdsa r signature value  */
    CpaFlatBuffer k;
    /**< random value (k > 0 and k < n) */
    CpaFlatBuffer n;
    /**< order of the base point G, which shall be prime */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcdsaSignSOpData;


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      ECDSA Sign R & S Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyEcdsaSignRS
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
 *      structure after it has been submitted to the cpaCyEcdsaSignRS
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcdsaSignRS()
 *
 *****************************************************************************/
typedef struct _CpaCyEcdsaSignRSOpData {
    CpaFlatBuffer xg;
    /**< x coordinate of base point G */
    CpaFlatBuffer yg;
    /**< y coordinate of base point G */
    CpaFlatBuffer n;
    /**< order of the base point G, which shall be prime */
    CpaFlatBuffer q;
    /**< prime modulus or irreducible polynomial over GF(2^r) */
    CpaFlatBuffer a;
    /**< a elliptic curve coefficient */
    CpaFlatBuffer b;
    /**< b elliptic curve coefficient */
    CpaFlatBuffer k;
    /**< random value (k > 0 and k < n) */
    CpaFlatBuffer m;
    /**< digest of the message to be signed */
    CpaFlatBuffer d;
    /**< private key */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcdsaSignRSOpData;


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      ECDSA Verify Operation Data, for Public Key.

 * @description
 *      This structure contains the operation data for the CpaCyEcdsaVerify
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
 *      structure after it has been submitted to the cpaCyEcdsaVerify
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      CpaCyEcdsaVerify()
 *
 *****************************************************************************/
typedef struct _CpaCyEcdsaVerifyOpData {
    CpaFlatBuffer xg;
    /**< x coordinate of base point G */
    CpaFlatBuffer yg;
    /**< y coordinate of base point G */
    CpaFlatBuffer n;
    /**< order of the base point G, which shall be prime */
    CpaFlatBuffer q;
    /**< prime modulus or irreducible polynomial over GF(2^r) */
    CpaFlatBuffer a;
    /**< a elliptic curve coefficient */
    CpaFlatBuffer b;
    /**< b elliptic curve coefficient */
    CpaFlatBuffer m;
    /**< digest of the message to be signed */
    CpaFlatBuffer r;
    /**< ECDSA r signature value (r > 0 and r < n) */
    CpaFlatBuffer s;
    /**< ECDSA s signature value (s > 0 and s < n) */
    CpaFlatBuffer xp;
    /**< x coordinate of point P (public key) */
    CpaFlatBuffer yp;
    /**< y coordinate of point P (public key) */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcdsaVerifyOpData;


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Cryptographic ECDSA Statistics.
 * @description
 *      This structure contains statistics on the Cryptographic ECDSA
 *      operations. Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyEcdsaStats64 {
    Cpa64U numEcdsaSignRRequests;
    /**< Total number of ECDSA Sign R operation requests. */
    Cpa64U numEcdsaSignRRequestErrors;
    /**< Total number of ECDSA Sign R operation requests that had an error and
     * could not be processed. */
    Cpa64U numEcdsaSignRCompleted;
    /**< Total number of ECDSA Sign R operation requests that completed
     * successfully. */
    Cpa64U numEcdsaSignRCompletedErrors;
    /**< Total number of ECDSA Sign R operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcdsaSignRCompletedOutputInvalid;
    /**< Total number of ECDSA Sign R operation requests could not be completed
     * successfully due to an invalid output.
     * Note that this does not indicate an error. */
    Cpa64U numEcdsaSignSRequests;
    /**< Total number of ECDSA Sign S operation requests. */
    Cpa64U numEcdsaSignSRequestErrors;
    /**< Total number of ECDSA Sign S operation requests that had an error and
     * could not be processed. */
    Cpa64U numEcdsaSignSCompleted;
    /**< Total number of ECDSA Sign S operation requests that completed
     * successfully. */
    Cpa64U numEcdsaSignSCompletedErrors;
    /**< Total number of ECDSA Sign S operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcdsaSignSCompletedOutputInvalid;
    /**< Total number of ECDSA Sign S operation requests could not be completed
     * successfully due to an invalid output.
     * Note that this does not indicate an error. */
    Cpa64U numEcdsaSignRSRequests;
    /**< Total number of ECDSA Sign R & S operation requests. */
    Cpa64U numEcdsaSignRSRequestErrors;
    /**< Total number of ECDSA Sign R & S operation requests that had an
     * error and could not be processed. */
    Cpa64U numEcdsaSignRSCompleted;
    /**< Total number of ECDSA Sign R & S operation requests that completed
     * successfully. */
    Cpa64U numEcdsaSignRSCompletedErrors;
    /**< Total number of ECDSA Sign R & S operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcdsaSignRSCompletedOutputInvalid;
    /**< Total number of ECDSA Sign R & S operation requests could not be
     * completed successfully due to an invalid output.
     * Note that this does not indicate an error. */
    Cpa64U numEcdsaVerifyRequests;
    /**< Total number of ECDSA Verification operation requests. */
    Cpa64U numEcdsaVerifyRequestErrors;
    /**< Total number of ECDSA Verification operation requests that had an
     * error and could not be processed. */
    Cpa64U numEcdsaVerifyCompleted;
    /**< Total number of ECDSA Verification operation requests that completed
     * successfully. */
    Cpa64U numEcdsaVerifyCompletedErrors;
    /**< Total number of ECDSA Verification operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcdsaVerifyCompletedOutputInvalid;
    /**< Total number of ECDSA Verification operation requests that resulted
     * in an invalid output.
     * Note that this does not indicate an error. */
} CpaCyEcdsaStats64;


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Definition of a generic callback function invoked for a number of the
 *      ECDSA Sign API functions.
 *
 * @description
 *      This is the prototype for the CpaCyEcdsaGenSignCbFunc callback function.
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
 *                              CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                              CPA_STATUS_UNSUPPORTED.
 * @param[in] pOpData           Opaque pointer to Operation data supplied in
 *                              request.
 * @param[in] multiplyStatus    Status of the point multiplication.
 * @param[in] pOut              Output data from the request.
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
 *      cpaCyEcdsaSignR()
 *      cpaCyEcdsaSignS()
 *
 *****************************************************************************/
typedef void (*CpaCyEcdsaGenSignCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean multiplyStatus,
        CpaFlatBuffer *pOut);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Definition of callback function invoked for cpaCyEcdsaSignRS
 *      requests.
 *
 * @description
 *      This is the prototype for the CpaCyEcdsaSignRSCbFunc callback function,
 *      which will provide the ECDSA message signature r and s parameters.
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
 *                              CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                              CPA_STATUS_UNSUPPORTED.
 * @param[in] pOpData           Operation data pointer supplied in request.
 * @param[in] multiplyStatus    Status of the point multiplication.
 * @param[in] pR                Ecdsa message signature r.
 * @param[in] pS                Ecdsa message signature s.
 *
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
 *      cpaCyEcdsaSignRS()
 *
 *****************************************************************************/
typedef void (*CpaCyEcdsaSignRSCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean multiplyStatus,
        CpaFlatBuffer *pR,
        CpaFlatBuffer *pS);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Definition of callback function invoked for cpaCyEcdsaVerify requests.
 *
 * @description
 *      This is the prototype for the CpaCyEcdsaVerifyCbFunc callback function.
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
 *                              CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                              CPA_STATUS_UNSUPPORTED.
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
 *      cpaCyEcdsaVerify()
 *
 *****************************************************************************/
typedef void (*CpaCyEcdsaVerifyCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean verifyStatus);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Generate ECDSA Signature R.
 *
 * @description
 *      This function generates ECDSA Signature R as per ANSI X9.62 2005
 *      section 7.3.
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
 * @param[in]  pCb              Callback function pointer. If this is set to a
 *                              NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pSignStatus      In synchronous mode, the multiply output is
 *                              valid (CPA_TRUE) or the output is invalid
 *                              (CPA_FALSE).
 * @param[out] pR               ECDSA message signature r.
 *
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback is generated in response
 *      to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      None
 *****************************************************************************/
CpaStatus
cpaCyEcdsaSignR(const CpaInstanceHandle instanceHandle,
        const CpaCyEcdsaGenSignCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcdsaSignROpData *pOpData,
        CpaBoolean *pSignStatus,
        CpaFlatBuffer *pR);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Generate ECDSA Signature S.
 *
 * @description
 *      This function generates ECDSA Signature S as per ANSI X9.62 2005
 *      section 7.3.
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
 * @param[in]  pCb              Callback function pointer. If this is set to a
 *                              NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pSignStatus      In synchronous mode, the multiply output is
 *                              valid (CPA_TRUE) or the output is invalid
 *                              (CPA_FALSE).
 * @param[out] pS               ECDSA message signature s.
 *
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback is generated in response
 *      to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      None
 *****************************************************************************/
CpaStatus
cpaCyEcdsaSignS(const CpaInstanceHandle instanceHandle,
        const CpaCyEcdsaGenSignCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcdsaSignSOpData *pOpData,
        CpaBoolean *pSignStatus,
        CpaFlatBuffer *pS);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Generate ECDSA Signature R & S.
 *
 * @description
 *      This function generates ECDSA Signature R & S as per ANSI X9.62 2005
 *      section 7.3.
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
 * @param[in]  pCb              Callback function pointer. If this is set to a
 *                              NULL value the function will operate
 *                              synchronously.
 * @param[in]  pCallbackTag     User-supplied value to help identify request.
 * @param[in]  pOpData          Structure containing all the data needed to
 *                              perform the operation. The client code
 *                              allocates the memory for this structure. This
 *                              component takes ownership of the memory until
 *                              it is returned in the callback.
 * @param[out] pSignStatus      In synchronous mode, the multiply output is
 *                              valid (CPA_TRUE) or the output is invalid
 *                              (CPA_FALSE).
 * @param[out] pR               ECDSA message signature r.
 * @param[out] pS               ECDSA message signature s.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback is generated in response
 *      to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      None
 *****************************************************************************/
CpaStatus
cpaCyEcdsaSignRS(const CpaInstanceHandle instanceHandle,
        const CpaCyEcdsaSignRSCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcdsaSignRSOpData *pOpData,
        CpaBoolean *pSignStatus,
        CpaFlatBuffer *pR,
        CpaFlatBuffer *pS);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Verify ECDSA Public Key.
 *
 * @description
 *      This function performs ECDSA Verify as per ANSI X9.62 2005 section 7.4.
 *
 *      A response status of ok (verifyStatus == CPA_TRUE) means that the
 *      signature was verified
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
 * @param[out] pVerifyStatus    In synchronous mode, set to CPA_FALSE if the
 *                              point is NOT on the curve or at infinity. Set
 *                              to CPA_TRUE if the point is on the curve.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance function.
 * @post
 *      None
 * @note
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyEcdsaVerifyCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcdsaVerifyOpData,
 *      CpaCyEcdsaVerifyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcdsaVerify(const CpaInstanceHandle instanceHandle,
        const CpaCyEcdsaVerifyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcdsaVerifyOpData *pOpData,
        CpaBoolean *pVerifyStatus);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdsa
 *      Query statistics for a specific ECDSA instance.
 *
 * @description
 *      This function will query a specific instance of the ECDSA implementation
 *      for statistics. The user MUST allocate the CpaCyEcdsaStats64 structure
 *      and pass the reference to that structure into this function call. This
 *      function writes the statistic results into the passed in
 *      CpaCyEcdsaStats64 structure.
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
 * @param[out] pEcdsaStats          Pointer to memory into which the statistics
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
 *      CpaCyEcdsaStats64
 *****************************************************************************/
CpaStatus
cpaCyEcdsaQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCyEcdsaStats64 *pEcdsaStats);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /*CPA_CY_ECDSA_H_*/
