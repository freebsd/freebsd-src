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
 * @file cpa_cy_ln.h
 *
 * @defgroup cpaCyLn Cryptographic Large Number API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the Cryptographic API for Large Number
 *      Operations.
 *
 * @note
 *      Large numbers are represented on the QuickAssist API using octet
 *      strings, stored in structures of type @ref CpaFlatBuffer.  These
 *      octet strings are encoded as described by PKCS#1 v2.1, section 4,
 *      which is consistent with ASN.1 syntax.  The following text
 *      summarizes this.   Any exceptions to this encoding are specified
 *      on the specific data structure or function to which the exception
 *      applies.
 *
 *      An n-bit number, N, has a value in the range 2^(n-1) through 2^(n)-1.
 *      In other words, its most significant bit, bit n-1 (where bit-counting
 *      starts from zero) MUST be set to 1.  We can also state that the
 *      bit-length n of a number N is defined by n = floor(log2(N))+1.
 *
 *      The buffer, b, in which an n-bit number N is stored, must be "large
 *      enough".  In other words, b.dataLenInBytes must be at least
 *      minLenInBytes = ceiling(n/8).
 *
 *      The number is stored in a "big endian" format.  This means that the
 *      least significant byte (LSB) is b[b.dataLenInBytes-1], while the
 *      most significant byte (MSB) is b[b.dataLenInBytes-minLenInBytes].
 *      In the case where the buffer is "exactly" the right size, then the
 *      MSB is b[0].  Otherwise, all bytes from b[0] up to the MSB MUST be
 *      set to 0x00.
 *
 *      The largest bit-length we support today is 8192 bits. In other
 *      words, we can deal with numbers up to a value of (2^8192)-1.
 *
 *****************************************************************************/

#ifndef CPA_CY_LN_H
#define CPA_CY_LN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Modular Exponentiation Function Operation Data.
 * @description
 *      This structure lists the different items that are required in the
 *      cpaCyLnModExp function. The client MUST allocate the memory for
 *      this structure. When the structure is passed into the function,
 *      ownership of the memory passes to the function. Ownership of the memory
 *      returns to the client when this structure is returned in the callback.
 *      The operation size in bits is equal to the size of whichever of the
 *      following is largest: the modulus, the base or the exponent.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this structure
 *      after it has been submitted to the cpaCyLnModExp function, and
 *      before it has been returned in the callback, undefined behavior will
 *      result.

 *      The values of the base, the exponent and the modulus MUST all be less
 *      than 2^8192, and the modulus must not be equal to zero.
 *****************************************************************************/
typedef struct _CpaCyLnModExpOpData {
    CpaFlatBuffer modulus;
    /**< Flat buffer containing a pointer to the modulus.
     * This number may be up to 8192 bits in length, and MUST be greater
     * than zero.
     */
    CpaFlatBuffer base;
    /**< Flat buffer containing a pointer to the base.
     * This number may be up to 8192 bits in length.
     */
    CpaFlatBuffer exponent;
    /**< Flat buffer containing a pointer to the exponent.
     * This number may be up to 8192 bits in length.
     */
} CpaCyLnModExpOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Modular Inversion Function Operation Data.
 * @description
 *      This structure lists the different items that are required in the
 *      function @ref cpaCyLnModInv. The client MUST allocate the memory for
 *      this structure. When the structure is passed into the function,
 *      ownership of the memory passes to the function. Ownership of the
 *      memory returns to the client when this structure is returned in the
 *      callback.
 * @note
 *      If the client modifies or frees the memory referenced in this structure
 *      after it has been submitted to the cpaCyLnModInv function, and
 *      before it has been returned in the callback, undefined behavior will
 *      result.
 *
 *      Note that the values of A and B MUST NOT both be even numbers, and
 *      both MUST be less than 2^8192.
 *****************************************************************************/
typedef struct _CpaCyLnModInvOpData {
    CpaFlatBuffer A;
    /**< Flat buffer containing a pointer to the value that will be
     * inverted.
     * This number may be up to 8192 bits in length, it MUST NOT be zero,
     * and it MUST be co-prime with B.
     */
    CpaFlatBuffer B;
    /**< Flat buffer containing a pointer to the value that will be used as
     * the modulus.
     * This number may be up to 8192 bits in length, it MUST NOT be zero,
     * and it MUST be co-prime with A.
     */
} CpaCyLnModInvOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Look Aside Cryptographic large number Statistics.
 * @deprecated
 *      As of v1.3 of the Crypto API, this structure has been deprecated,
 *      replaced by @ref CpaCyLnStats64.
 * @description
 *      This structure contains statistics on the Look Aside Cryptographic
 *      large number operations. Statistics are set to zero when the component
 *      is initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyLnStats {
    Cpa32U numLnModExpRequests;
    /**< Total number of successful large number modular exponentiation
     * requests.*/
    Cpa32U numLnModExpRequestErrors;
    /**< Total number of large number modular exponentiation requests that
     * had an error and could not be processed.  */
    Cpa32U numLnModExpCompleted;
    /**< Total number of large number modular exponentiation operations
     * that completed successfully. */
    Cpa32U numLnModExpCompletedErrors;
    /**< Total number of large number modular exponentiation operations
     * that could not be completed successfully due to errors. */
    Cpa32U numLnModInvRequests;
    /**< Total number of successful large number modular inversion
     * requests.*/
    Cpa32U numLnModInvRequestErrors;
    /**< Total number of large number modular inversion requests that
     * had an error and could not be processed.  */
    Cpa32U numLnModInvCompleted;
    /**< Total number of large number modular inversion operations
     * that completed successfully. */
    Cpa32U numLnModInvCompletedErrors;
    /**< Total number of large number modular inversion operations
     * that could not be completed successfully due to errors. */
} CpaCyLnStats CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Look Aside Cryptographic large number Statistics.
 * @description
 *      This structure contains statistics on the Look Aside Cryptographic
 *      large number operations. Statistics are set to zero when the component
 *      is initialized, and are collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyLnStats64 {
    Cpa64U numLnModExpRequests;
    /**< Total number of successful large number modular exponentiation
     * requests.*/
    Cpa64U numLnModExpRequestErrors;
    /**< Total number of large number modular exponentiation requests that
     * had an error and could not be processed.  */
    Cpa64U numLnModExpCompleted;
    /**< Total number of large number modular exponentiation operations
     * that completed successfully. */
    Cpa64U numLnModExpCompletedErrors;
    /**< Total number of large number modular exponentiation operations
     * that could not be completed successfully due to errors. */
    Cpa64U numLnModInvRequests;
    /**< Total number of successful large number modular inversion
     * requests.*/
    Cpa64U numLnModInvRequestErrors;
    /**< Total number of large number modular inversion requests that
     * had an error and could not be processed.  */
    Cpa64U numLnModInvCompleted;
    /**< Total number of large number modular inversion operations
     * that completed successfully. */
    Cpa64U numLnModInvCompletedErrors;
    /**< Total number of large number modular inversion operations
     * that could not be completed successfully due to errors. */
} CpaCyLnStats64;

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Perform modular exponentiation operation.
 *
 * @description
 *      This function performs modular exponentiation. It computes the
 *      following result based on the inputs:
 *
 *      result = (base ^ exponent) mod modulus
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
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle        Instance handle.
 * @param[in]  pLnModExpCb           Pointer to callback function to be
 *                                   invoked when the operation is complete.
 * @param[in]  pCallbackTag          Opaque User Data for this specific call.
 *                                   Will be returned unchanged in the callback.
 * @param[in]  pLnModExpOpData       Structure containing all the data needed
 *                                   to perform the LN modular exponentiation
 *                                   operation. The client code allocates
 *                                   the memory for this structure. This
 *                                   component takes ownership of the memory
 *                                   until it is returned in the callback.
 * @param[out] pResult               Pointer to a flat buffer containing a
 *                                   pointer to memory allocated by the client
 *                                   into which the result will be written.
 *                                   The size of the memory required MUST be
 *                                   larger than or equal to the size
 *                                   required to store the modulus.
 *                                   On invocation the callback function
 *                                   will contain this parameter in the
 *                                   pOut parameter.
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
 *      The component has been initialized.
 * @post
 *      None
 * @note
 *      When pLnModExpCb is non null, an asynchronous callback of type
 *      CpaCyLnModExpCbFunc is generated in response to this function call.
 *      Any errors generated during processing are reported in the structure
 *      returned in the callback.
 *
 * @see
 *      CpaCyLnModExpOpData, CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyLnModExp(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pLnModExpCb,
        void *pCallbackTag,
        const CpaCyLnModExpOpData *pLnModExpOpData,
        CpaFlatBuffer *pResult);

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Perform modular inversion operation.
 *
 * @description
 *      This function performs modular inversion. It computes the following
 *      result based on the inputs:
 *
 *      result = (1/A) mod B.
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
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle          Instance handle.
 * @param[in]  pLnModInvCb             Pointer to callback function to be
 *                                     invoked when the operation is complete.
 * @param[in]  pCallbackTag            Opaque User Data for this specific call.
 *                                     Will be returned unchanged in the
 *                                     callback.
 * @param[in]  pLnModInvOpData         Structure containing all the data
 *                                     needed to perform the LN modular
 *                                     inversion operation. The client code
 *                                     allocates the memory for this structure.
 *                                     This component takes ownership of the
 *                                     memory until it is returned in the
 *                                     callback.
 * @param[out] pResult                 Pointer to a flat buffer containing a
 *                                     pointer to memory allocated by the client
 *                                     into which the result will be written.
 *                                     The size of the memory required MUST be
 *                                     larger than or equal to the size
 *                                     required to store the modulus.
 *                                     On invocation the callback function
 *                                     will contain this parameter in the
 *                                     pOut parameter.
 *
 * @retval CPA_STATUS_SUCCESS          Function executed successfully.
 * @retval CPA_STATUS_FAIL             Function failed.
 * @retval CPA_STATUS_RETRY            Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM    Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE         Error related to system resources.
 * @retval CPA_STATUS_RESTARTING       API implementation is restarting. Resubmit
 *                                     the request.
 * @retval CPA_STATUS_UNSUPPORTED      Function is not supported.
 *
 * @pre
 *      The component has been initialized.
 * @post
 *      None
 * @note
 *      When pLnModInvCb is non null, an asynchronous callback of type
 *      CpaCyLnModInvCbFunc is generated in response to this function call.
 *      Any errors generated during processing are reported in the structure
 *      returned in the callback.
 *
 * @see
 *      CpaCyLnModInvOpData,
 *      CpaCyGenFlatBufCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyLnModInv(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pLnModInvCb,
        void *pCallbackTag,
        const CpaCyLnModInvOpData *pLnModInvOpData,
        CpaFlatBuffer *pResult);

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Query statistics for large number operations
 *
 * @deprecated
 *      As of v1.3 of the Crypto API, this function has been deprecated,
 *      replaced by @ref cpaCyLnStatsQuery64().
 *
 * @description
 *      This function will query a specific instance handle for large number
 *      statistics. The user MUST allocate the CpaCyLnStats structure and pass
 *      the reference to that structure into this function call. This function
 *      writes the statistic results into the passed in CpaCyLnStats structure.
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
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle            Instance handle.
 * @param[out] pLnStats                  Pointer to memory into which the
 *                                      statistics will be written.
 *
 * @retval CPA_STATUS_SUCCESS           Function executed successfully.
 * @retval CPA_STATUS_FAIL              Function failed.
 * @retval CPA_STATUS_INVALID_PARAM     Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE          Error related to system resources.
 * @retval CPA_STATUS_RESTARTING        API implementation is restarting. Resubmit
 *                                      the request.
 * @retval CPA_STATUS_UNSUPPORTED       Function is not supported.
 *
 * @pre
 *      Acceleration Services unit has been initialized.
 *
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 *
 * @see
 *      CpaCyLnStats
 *
 *****************************************************************************/
CpaStatus CPA_DEPRECATED
cpaCyLnStatsQuery(const CpaInstanceHandle instanceHandle,
        struct _CpaCyLnStats *pLnStats);

/**
 *****************************************************************************
 * @ingroup cpaCyLn
 *      Query statistics (64-bit version) for large number operations
 *
 * @description
 *      This function will query a specific instance handle for the 64-bit
 *      version of the large number statistics.
 *      The user MUST allocate the CpaCyLnStats64 structure and pass
 *      the reference to that structure into this function call. This function
 *      writes the statistic results into the passed in CpaCyLnStats64
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
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle            Instance handle.
 * @param[out] pLnStats                 Pointer to memory into which the
 *                                      statistics will be written.
 *
 * @retval CPA_STATUS_SUCCESS           Function executed successfully.
 * @retval CPA_STATUS_FAIL              Function failed.
 * @retval CPA_STATUS_INVALID_PARAM     Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE          Error related to system resources.
 * @retval CPA_STATUS_RESTARTING        API implementation is restarting. Resubmit
 *                                      the request.
 * @retval CPA_STATUS_UNSUPPORTED       Function is not supported.
 *
 * @pre
 *      Acceleration Services unit has been initialized.
 *
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 *
 * @see
 *      CpaCyLnStats
 *****************************************************************************/
CpaStatus
cpaCyLnStatsQuery64(const CpaInstanceHandle instanceHandle,
        CpaCyLnStats64 *pLnStats);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_CY_LN_H */
