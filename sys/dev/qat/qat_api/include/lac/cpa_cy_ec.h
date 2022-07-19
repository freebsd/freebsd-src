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
 * @file cpa_cy_ec.h
 *
 * @defgroup cpaCyEc Elliptic Curve (EC) API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for Public Key Encryption
 *      (Cryptography) Elliptic Curve (EC) operations.
 *
 *      All implementations will support at least the following:
 *
 *      - "NIST RECOMMENDED ELLIPTIC CURVES FOR FEDERAL GOVERNMENT USE"
 *        as defined by
 *        http://csrc.nist.gov/groups/ST/toolkit/documents/dss/NISTReCur.pdf
 *
 *      - Random curves where the max(log2(q), log2(n) + log2(h)) <= 512
 *        where q is the modulus, n is the order of the curve and h is the
 *        cofactor
 *
 *      For Montgomery and Edwards 25519 and 448 elliptic curves,
 *      the following operations are supported:
 *           1. Montgomery 25519 Curve         | scalar point Multiplication
 *              Input:  Montgomery affine coordinate X of point P
 *                      Scalar k
 *              Output: Montgomery affine coordinate X of point [k/P
 *              Decode: Scalar k always decoded by implementation
 *
 *           2. Montgomery 25519 Curve         | generator point Multiplication
 *              Input:  Scalar k
 *              Output: Montgomery affine coordinate X of point [k]G
 *              Decode: Scalar k always decoded by implementation
 *
 *           3. Twisted Edwards 25519 Curve    | scalar point Multiplication
 *              Input:  Twisted Edwards affine coordinate X of point P
 *                      Twisted Edwards affine coordinate Y of point P
 *                      Scalar k
 *              Output: Twisted Edwards affine coordinate X of point [k]P
 *                      Twisted Edwards affine coordinate Y of point [k]P
 *              Decode: Caller must specify if decoding is required
 *
 *           4. Twisted Edwards 25519 Curve    | generator point Multiplication
 *              Input:  Scalar k
 *              Output: Twisted Edwards affine coordinate X of point [k]G
 *                      Twisted Edwards affine coordinate Y of point [k]G
 *              Decode: Caller must specify if decoding is required
 *
 *           5. Montgomery 448 Curve           | scalar point Multiplication
 *              Input:  Montgomery affine coordinate X of point P
 *                      Scalar k
 *              Output: Montgomery affine coordinate X of point [k]P
 *              Decode: Scalar k always decoded by implementation
 *
 *           6. Montgomery 448 Curve           | generator point Multiplication
 *              Input:  Scalar k
 *              Output: Montgomery affine coordinate X of point [k]G
 *              Decode: Scalar k always decoded by implementation
 *
 *           7. Edwards 448 Curve              | scalar point Multiplication
 *              Input:  Edwards affine coordinate X of point P
 *                      Edwards affine coordinate Y of point P
 *                      Scalar k
 *              Output: Edwards affine coordinate X of point [k]P
 *                      Edwards affine coordinate Y of point [k]P
 *              Decode: Caller must specify if decoding is required
 *
 *           8. Edwards 448 Curve              | generator point Multiplication
 *              Input:  Scalar k
 *              Output: Edwards affine coordinate X of point [k]G
 *                      Edwards affine coordinate Y of point [k]G
 *              Decode: Caller must specify if decoding is required
 *
 * @note
 *      Large numbers are represented on the QuickAssist API as described
 *      in the Large Number API (@ref cpaCyLn).
 *
 *      In addition, the bit length of large numbers passed to the API
 *      MUST NOT exceed 576 bits for Elliptic Curve operations.
 *****************************************************************************/

#ifndef CPA_CY_EC_H_
#define CPA_CY_EC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"

/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Field types for Elliptic Curve

 * @description
 *      As defined by FIPS-186-3, for each cryptovariable length, there are
 *      two kinds of fields.
 *      <ul>
 *        <li> A prime field is the field GF(p) which contains a prime number
 *        p of elements. The elements of this field are the integers modulo
 *        p, and the field arithmetic is implemented in terms of the
 *        arithmetic of integers modulo p.</li>
 *
 *        <li> A binary field is the field GF(2^m) which contains 2^m elements
 *        for some m (called the degree of the field). The elements of
 *        this field are the bit strings of length m, and the field
 *        arithmetic is implemented in terms of operations on the bits.</li>
 *      </ul>
 *****************************************************************************/
typedef enum _CpaCyEcFieldType
{
    CPA_CY_EC_FIELD_TYPE_PRIME = 1,
    /**< A prime field, GF(p) */
    CPA_CY_EC_FIELD_TYPE_BINARY,
    /**< A binary field, GF(2^m) */
} CpaCyEcFieldType;

/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Curve types for Elliptic Curves defined in RFC#7748

 * @description
 *      As defined by RFC 7748, there are four elliptic curves in this
 *      group. The Montgomery curves are denoted curve25519 and curve448,
 *      and the birationally equivalent Twisted Edwards curves are denoted
 *      edwards25519 and edwards448
 *
 *****************************************************************************/
typedef enum _CpaCyEcMontEdwdsCurveType
{
    CPA_CY_EC_MONTEDWDS_CURVE25519_TYPE = 1,
    /**< Montgomery 25519 curve */
    CPA_CY_EC_MONTEDWDS_ED25519_TYPE,
    /**< Twisted Edwards 25519 curve */
    CPA_CY_EC_MONTEDWDS_CURVE448_TYPE,
    /**< Montgomery 448 curve */
    CPA_CY_EC_MONTEDWDS_ED448_TYPE,
    /**< Twisted Edwards 448 curve */
} CpaCyEcMontEdwdsCurveType;

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEc
 *      EC Point Multiplication Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcPointMultiply
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
 *      structure after it has been submitted to the cpaCyEcPointMultiply
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcPointMultiply()
 *
 *****************************************************************************/
typedef struct _CpaCyEcPointMultiplyOpData {
    CpaFlatBuffer k;
    /**< scalar multiplier  (k > 0 and k < n) */
    CpaFlatBuffer xg;
    /**< x coordinate of curve point */
    CpaFlatBuffer yg;
    /**< y coordinate of curve point */
    CpaFlatBuffer a;
    /**< a elliptic curve coefficient */
    CpaFlatBuffer b;
    /**< b elliptic curve coefficient */
    CpaFlatBuffer q;
    /**< prime modulus or irreducible polynomial over GF(2^m)*/
    CpaFlatBuffer h;
    /**< cofactor of the operation.
     * If the cofactor is NOT required then set the cofactor to 1 or the
     * data pointer of the Flat Buffer to NULL. */
    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcPointMultiplyOpData;


/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      EC Point Verification Operation Data.
 *
 * @description
 *      This structure contains the operation data for the cpaCyEcPointVerify
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
 *      structure after it has been submitted to the CpaCyEcPointVerify
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcPointVerify()
 *
 *****************************************************************************/
typedef struct _CpaCyEcPointVerifyOpData {
    CpaFlatBuffer xq;
    /**< x coordinate candidate point */
    CpaFlatBuffer yq;
    /**< y coordinate candidate point */
    CpaFlatBuffer q;
    /**< prime modulus or irreducible polynomial over GF(2^m) */
    CpaFlatBuffer a;
    /**< a elliptic curve coefficient */
    CpaFlatBuffer b;
    /**< b elliptic curve coefficient */

    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
} CpaCyEcPointVerifyOpData;

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEc
 *      EC Point Multiplication Operation Data for Edwards or
 8      Montgomery curves as specificied in RFC#7748.
 *
 * @description
 *      This structure contains the operation data for the
 *      cpaCyEcMontEdwdsPointMultiply function.
 *      The client MUST allocate the memory for this structure and the
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
 *      structure after it has been submitted to the cpaCyEcPointMultiply
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 *       All buffers in this structure need to be:
 *       - 32 bytes in size for 25519 curves
 *       - 64 bytes in size for 448 curves
 *
 * @see
 *      cpaCyEcMontEdwdsPointMultiply()
 *
 *****************************************************************************/
typedef struct _CpaCyEcMontEdwdsPointMultiplyOpData {
    CpaCyEcMontEdwdsCurveType curveType;
    /**< field type for the operation */
    CpaBoolean  generator;
    /**< True if the operation is a generator multiplication (kG)
     *   False if it is a variable point multiplcation (kP). */
    CpaFlatBuffer  k;
    /**< k or generator for the operation */
    CpaFlatBuffer  x;
    /**< x value.  Used in scalar varable point multiplication operations.
     * Not required if the generator is True. Must be NULL if not required.
     * The size of the buffer MUST be 32B for 25519 curves and 64B for 448
     * curves */
    CpaFlatBuffer  y;
    /**< y value.  Used in variable point multiplication of operations.
     * Not required for curves defined only on scalar operations.
     * Not required if the generator is True.
     * Must be NULL if not required.
     * The size of the buffer MUST be 32B for 25519 curves and 64B for 448
     * curves */
} CpaCyEcMontEdwdsPointMultiplyOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Cryptographic EC Statistics.
 *
 * @description
 *      This structure contains statistics on the Cryptographic EC
 *      operations. Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyEcStats64 {
    Cpa64U numEcPointMultiplyRequests;
    /**< Total number of EC Point Multiplication operation requests. */
    Cpa64U numEcPointMultiplyRequestErrors;
    /**< Total number of EC Point Multiplication operation requests that had an
     * error and could not be processed. */
    Cpa64U numEcPointMultiplyCompleted;
    /**< Total number of EC Point Multiplication operation requests that
     * completed successfully. */
    Cpa64U numEcPointMultiplyCompletedError;
    /**< Total number of EC Point Multiplication operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcPointMultiplyCompletedOutputInvalid;
    /**< Total number of EC Point Multiplication operation requests that could
     * not be completed successfully due to an invalid output.
     * Note that this does not indicate an error. */
    Cpa64U numEcPointVerifyRequests;
    /**< Total number of EC Point Verification operation requests. */
    Cpa64U numEcPointVerifyRequestErrors;
    /**< Total number of EC Point Verification operation requests that had an
     * error and could not be processed. */
    Cpa64U numEcPointVerifyCompleted;
    /**< Total number of EC Point Verification operation requests that completed
     * successfully. */
    Cpa64U numEcPointVerifyCompletedErrors;
    /**< Total number of EC Point Verification operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcPointVerifyCompletedOutputInvalid;
    /**< Total number of EC Point Verification operation requests that had an
     * invalid output. Note that this does not indicate an error. */
} CpaCyEcStats64;


/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Definition of callback function invoked for cpaCyEcPointMultiply
 *      requests.
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
 * @param[in] pXk               x coordinate of resultant EC point.
 * @param[in] pYk               y coordinate of resultant EC point.
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
 *      cpaCyEcPointMultiply()
 *
 *****************************************************************************/
typedef void (*CpaCyEcPointMultiplyCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean multiplyStatus,
        CpaFlatBuffer *pXk,
        CpaFlatBuffer *pYk);


/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Definition of callback function invoked for cpaCyEcPointVerify
 *      requests.
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
 * @param[in] verifyStatus      Set to CPA_FALSE if the point is NOT on the
 *                              curve or at infinity. Set to CPA_TRUE if the
 *                              point is on the curve.
 *
 * @return
 *      None
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaCyEcPointVerify()
 *
 *****************************************************************************/
typedef void (*CpaCyEcPointVerifyCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean verifyStatus);


/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Perform EC Point Multiplication.
 *
 * @description
 *      This function performs Elliptic Curve Point Multiplication as per
 *      ANSI X9.63 Annex D.3.2.
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
 * @param[out] pMultiplyStatus  In synchronous mode, the multiply output is
 *                              valid (CPA_TRUE) or the output is invalid
 *                              (CPA_FALSE).
 * @param[out] pXk              Pointer to xk flat buffer.
 * @param[out] pYk              Pointer to yk flat buffer.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
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
 *      CpaCyEcPointMultiplyCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcPointMultiplyOpData,
 *      CpaCyEcPointMultiplyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcPointMultiply(const CpaInstanceHandle instanceHandle,
        const CpaCyEcPointMultiplyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcPointMultiplyOpData *pOpData,
        CpaBoolean *pMultiplyStatus,
        CpaFlatBuffer *pXk,
        CpaFlatBuffer *pYk);


/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Verify that a point is on an elliptic curve.
 *
 * @description
 *      This function performs Elliptic Curve Point Verification, as per
 *      steps a, b and c of ANSI X9.62 Annex A.4.2.  (To perform the final
 *      step d, the user can call @ref cpaCyEcPointMultiply.)
 *
 *      This function checks if the specified point satisfies the
 *      Weierstrass equation for an Elliptic Curve.
 *
 *      For GF(p):
 *          y^2 = (x^3 + ax + b) mod p
 *      For GF(2^m):
 *          y^2 + xy = x^3 + ax^2 + b mod p
 *              where p is the irreducible polynomial over GF(2^m)
 *
 *      Use this function to verify a point is in the correct range and is
 *      NOT the point at infinity.
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
 *      CpaCyEcPointVerifyCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcPointVerifyOpData,
 *      CpaCyEcPointVerifyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcPointVerify(const CpaInstanceHandle instanceHandle,
        const CpaCyEcPointVerifyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcPointVerifyOpData *pOpData,
        CpaBoolean *pVerifyStatus);

/**
 *****************************************************************************
 * @file cpa_cy_ec.h
 * @ingroup cpaCyEc
 *      Perform EC Point Multiplication on an Edwards or Montgomery curve as
 *      defined in RFC#7748.
 *
 * @description
 *      This function performs Elliptic Curve Point Multiplication as per
 *      RFC#7748
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
 * @param[out] pMultiplyStatus  In synchronous mode, the multiply output is
 *                              valid (CPA_TRUE) or the output is invalid
 *                              (CPA_FALSE).
 * @param[out] pXk              Pointer to xk flat buffer.
 * @param[out] pYk              Pointer to yk flat buffer.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter in.
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
 *      CpaCyEcPointMultiplyCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcMontEdwdsPointMultiplyOpData,
 *      CpaCyEcMontEdwdsPointMultiplyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcMontEdwdsPointMultiply(const CpaInstanceHandle instanceHandle,
        const CpaCyEcPointMultiplyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcMontEdwdsPointMultiplyOpData *pOpData,
        CpaBoolean *pMultiplyStatus,
        CpaFlatBuffer *pXk,
        CpaFlatBuffer *pYk);

/**
 *****************************************************************************
 * @ingroup cpaCyEc
 *      Query statistics for a specific EC instance.
 *
 * @description
 *      This function will query a specific instance of the EC implementation
 *      for statistics. The user MUST allocate the CpaCyEcStats64 structure
 *      and pass the reference to that structure into this function call. This
 *      function writes the statistic results into the passed in
 *      CpaCyEcStats64 structure.
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
 * @param[out] pEcStats             Pointer to memory into which the statistics
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
 *      CpaCyEcStats64
 *****************************************************************************/
CpaStatus
cpaCyEcQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCyEcStats64 *pEcStats);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /*CPA_CY_EC_H_*/
