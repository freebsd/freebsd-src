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
 * @file cpa_cy_ecdh.h
 *
 * @defgroup cpaCyEcdh Elliptic Curve Diffie-Hellman (ECDH) API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for Public Key Encryption
 *      (Cryptography) Elliptic Curve Diffie-Hellman (ECDH) operations.
 *
 * @note
 *      Large numbers are represented on the QuickAssist API as described
 *      in the Large Number API (@ref cpaCyLn).
 *
 *      In addition, the bit length of large numbers passed to the API
 *      MUST NOT exceed 576 bits for Elliptic Curve operations.
 *****************************************************************************/

#ifndef CPA_CY_ECDH_H_
#define CPA_CY_ECDH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"
#include "cpa_cy_ec.h"

/**
 *****************************************************************************
 * @ingroup cpaCyEcdh
 *      ECDH Point Multiplication Operation Data.
 *
 * @description
 *      This structure contains the operation data for the
 *      cpaCyEcdhPointMultiply function. The client MUST allocate the memory
 *      for this structure and the items pointed to by this structure. When
 *      the structure is passed into the function, ownership of the memory
 *      passes to the function. Ownership of the memory returns to the client
 *      when this structure is returned in the callback function.
 *
 *      For optimal performance all data buffers SHOULD be 8-byte aligned.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. a.pData[0] = MSB.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyEcdhPointMultiply
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyEcdhPointMultiply()
 *
 *****************************************************************************/
typedef struct _CpaCyEcdhPointMultiplyOpData {
    CpaFlatBuffer k;
    /**< scalar multiplier (k > 0 and k < n) */
    CpaFlatBuffer xg;
    /**< x coordinate of curve point */
    CpaFlatBuffer yg;
    /**< y coordinate of curve point */
    CpaFlatBuffer a;
    /**< a equation coefficient */
    CpaFlatBuffer b;
    /**< b equation coefficient */
    CpaFlatBuffer q;
    /**< prime modulus or irreducible polynomial over GF(2^r) */
    CpaFlatBuffer h;
    /**< cofactor of the operation.
     * If the cofactor is NOT required then set the cofactor to 1 or the
     * data pointer of the Flat Buffer to NULL.
     * There are some restrictions on the value of the cofactor.
     * Implementations of this API will support at least the following:
     * <ul>
     *   <li>NIST standard curves and their cofactors (1, 2 and 4)</li>
     *
     *   <li>Random curves where max(log2(p), log2(n)+log2(h)) <= 512, where
     *   p is the modulus, n is the order of the curve and h is the cofactor
     *   </li>
     * </ul>
     */

    CpaCyEcFieldType fieldType;
    /**< field type for the operation */
    CpaBoolean pointVerify;
    /**< set to CPA_TRUE to do a verification before the multiplication */
} CpaCyEcdhPointMultiplyOpData;


/**
 *****************************************************************************
 * @ingroup cpaCyEcdh
 *      Cryptographic ECDH Statistics.
 * @description
 *      This structure contains statistics on the Cryptographic ECDH
 *      operations. Statistics are set to zero when the component is
 *      initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyEcdhStats64 {
    Cpa64U numEcdhPointMultiplyRequests;
    /**< Total number of ECDH Point Multiplication operation requests. */
    Cpa64U numEcdhPointMultiplyRequestErrors;
    /**< Total number of ECDH Point Multiplication operation requests that had
     * an error and could not be processed. */
    Cpa64U numEcdhPointMultiplyCompleted;
    /**< Total number of ECDH Point Multiplication operation requests that
     * completed successfully. */
    Cpa64U numEcdhPointMultiplyCompletedError;
    /**< Total number of ECDH Point Multiplication operation requests that could
     * not be completed successfully due to errors. */
    Cpa64U numEcdhRequestCompletedOutputInvalid;
    /**< Total number of ECDH Point Multiplication or Point Verify operation
     * requests that could not be completed successfully due to an invalid
     * output.
     * Note that this does not indicate an error. */
} CpaCyEcdhStats64;


/**
 *****************************************************************************
 * @ingroup cpaCyEcdh
 *      Definition of callback function invoked for cpaCyEcdhPointMultiply
 *      requests.
 *
 * @description
 *      This is the prototype for the CpaCyEcdhPointMultiplyCbFunc callback
 *      function
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
 * @param[in] pXk               Output x coordinate from the request.
 * @param[in] pYk               Output y coordinate from the request.
 * @param[in] multiplyStatus    Status of the point multiplication and the
 *                              verification when the pointVerify bit is set
 *                              in the CpaCyEcdhPointMultiplyOpData structure.
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
 *      cpaCyEcdhPointMultiply()
 *
 *****************************************************************************/
typedef void (*CpaCyEcdhPointMultiplyCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean multiplyStatus,
        CpaFlatBuffer *pXk,
        CpaFlatBuffer *pYk);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdh
 *      ECDH Point Multiplication.
 *
 * @description
 *      This function performs ECDH Point Multiplication as defined in
 *      ANSI X9.63 2001 section 5.4
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
 * @param[out] pMultiplyStatus  In synchronous mode, the status of the point
 *                              multiplication and the verification when the
 *                              pointVerify bit is set in the
 *                              CpaCyEcdhPointMultiplyOpData structure. Set to
 *                              CPA_FALSE if the point is NOT on the curve or
 *                              at infinity. Set to CPA_TRUE if the point is
 *                              on the curve.
 * @param[out] pXk              Pointer to x coordinate flat buffer.
 * @param[out] pYk              Pointer to y coordinate flat buffer.
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
 *      CpaCyEcdhPointMultiplyCbFunc is generated in response to this function
 *      call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyEcdhPointMultiplyOpData,
 *      CpaCyEcdhPointMultiplyCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyEcdhPointMultiply(const CpaInstanceHandle instanceHandle,
        const CpaCyEcdhPointMultiplyCbFunc pCb,
        void *pCallbackTag,
        const CpaCyEcdhPointMultiplyOpData *pOpData,
        CpaBoolean *pMultiplyStatus,
        CpaFlatBuffer *pXk,
        CpaFlatBuffer *pYk);


/**
 *****************************************************************************
 * @ingroup cpaCyEcdh
 *      Query statistics for a specific ECDH instance.
 *
 * @description
 *      This function will query a specific instance of the ECDH implementation
 *      for statistics. The user MUST allocate the CpaCyEcdhStats64 structure
 *      and pass the reference to that structure into this function call. This
 *      function writes the statistic results into the passed in
 *      CpaCyEcdhStats64 structure.
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
 * @param[out] pEcdhStats           Pointer to memory into which the statistics
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
 *      CpaCyEcdhStats64
 *****************************************************************************/
CpaStatus
cpaCyEcdhQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCyEcdhStats64 *pEcdhStats);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /*CPA_CY_ECDH_H_*/
