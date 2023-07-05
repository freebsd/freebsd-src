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
 * @file cpa_cy_dh.h
 *
 * @defgroup cpaCyDh Diffie-Hellman (DH) API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for Public Key Encryption
 *      (Cryptography) operations for use with Diffie-Hellman algorithm.
 *
 * @note
 *      Large numbers are represented on the QuickAssist API as described
 *      in the Large Number API (@ref cpaCyLn).
 *****************************************************************************/

#ifndef CPA_CY_DH_H
#define CPA_CY_DH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"
/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Diffie-Hellman Phase 1 Key Generation Data.
 * @description
 *      This structure lists the different items that are required in the
 *      cpaCyDhKeyGenPhase1 function. The client MUST allocate the memory for
 *      this structure. When the structure is passed into the function,
 *      ownership of the memory passes to the function. Ownership of the memory
 *      returns to the client when this structure is returned with the
 *      CpaCyDhPhase1KeyGenOpData structure.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this structure
 *      after it has been submitted to the cpaCyDhKeyGenPhase1 function, and
 *      before it has been returned in the callback, undefined behavior will
 *      result.
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. primeP.pData[0] = MSB.
 *
 *****************************************************************************/
typedef struct _CpaCyDhPhase1KeyGenOpData {
    CpaFlatBuffer primeP;
    /**< Flat buffer containing a pointer to the random odd prime number (p).
     * The bit-length of this number may be one of 768, 1024, 1536, 2048,
     * 3072, 4096 or 8192.
     */
    CpaFlatBuffer baseG;
    /**< Flat buffer containing a pointer to base (g). This MUST comply with
     * the following:
     *    0 < g < p.
     */
    CpaFlatBuffer privateValueX;
    /**< Flat buffer containing a pointer to the private value (x). This is a
     * random value which MUST satisfy the following condition:
     *     0 < PrivateValueX < (PrimeP - 1)
     *
     * Refer to PKCS #3: Diffie-Hellman Key-Agreement Standard for details.
     * The client creating this data MUST ensure the compliance of this value
     * with the standard. Note: This value is also needed to complete local
     * phase 2 Diffie-Hellman operation.*/
} CpaCyDhPhase1KeyGenOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Diffie-Hellman Phase 2 Secret Key Generation Data.
 * @description
 *      This structure lists the different items that required in the
 *      cpaCyDhKeyGenPhase2Secret function. The client MUST allocate the
 *      memory for this structure. When the structure is passed into the
 *      function, ownership of the memory passes to the function. Ownership of
 *      the memory returns to the client when this structure is returned with
 *      the callback.
 * @note
 *      If the client modifies or frees the memory referenced in this structure
 *      after it has been submitted to the cpaCyDhKeyGenPhase2Secret
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. primeP.pData[0] = MSB.
 *
 *****************************************************************************/
typedef struct _CpaCyDhPhase2SecretKeyGenOpData {
    CpaFlatBuffer primeP;
    /**< Flat buffer containing a pointer to the random odd prime number (p).
     * The bit-length of this number may be one of 768, 1024, 1536, 2048,
     * 3072, 4096 or 8192.
     * This SHOULD be same prime number as was used in the phase 1 key
     * generation operation. */
    CpaFlatBuffer remoteOctetStringPV;
    /**< Flat buffer containing a pointer to the remote entity
     * octet string Public Value (PV). */
    CpaFlatBuffer privateValueX;
    /**< Flat buffer containing a pointer to the private value (x). This
     * value may have been used in a call to the cpaCyDhKeyGenPhase1 function.
     * This is a random value which MUST satisfy the following condition:
     * 0 < privateValueX < (primeP - 1). */
} CpaCyDhPhase2SecretKeyGenOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Diffie-Hellman Statistics.
 * @deprecated
 *      As of v1.3 of the Crypto API, this structure has been deprecated,
 *      replaced by @ref CpaCyDhStats64.
 * @description
 *      This structure contains statistics on the Diffie-Hellman operations.
 *      Statistics are set to zero when the component is initialized, and are
 *      collected per instance.
 ****************************************************************************/
typedef struct _CpaCyDhStats {
    Cpa32U numDhPhase1KeyGenRequests;
    /**< Total number of successful Diffie-Hellman phase 1 key
     * generation requests. */
    Cpa32U numDhPhase1KeyGenRequestErrors;
    /**< Total number of Diffie-Hellman phase 1 key generation requests
     * that had an error and could not be processed. */
    Cpa32U numDhPhase1KeyGenCompleted;
    /**< Total number of Diffie-Hellman phase 1 key generation operations
     * that completed successfully. */
    Cpa32U numDhPhase1KeyGenCompletedErrors;
    /**< Total number of Diffie-Hellman phase 1 key generation operations
     * that could not be completed successfully due to errors. */
    Cpa32U numDhPhase2KeyGenRequests;
    /**< Total number of successful Diffie-Hellman phase 2 key
     * generation requests. */
    Cpa32U numDhPhase2KeyGenRequestErrors;
    /**< Total number of Diffie-Hellman phase 2 key generation requests
     * that had an error and could not be processed. */
    Cpa32U numDhPhase2KeyGenCompleted;
    /**< Total number of Diffie-Hellman phase 2 key generation operations
     * that completed successfully. */
    Cpa32U numDhPhase2KeyGenCompletedErrors;
    /**< Total number of Diffie-Hellman phase 2 key generation operations
     * that could not be completed successfully due to errors. */
} CpaCyDhStats CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Diffie-Hellman Statistics (64-bit version).
 * @description
 *      This structure contains the 64-bit version of the statistics on the
 *      Diffie-Hellman operations.
 *      Statistics are set to zero when the component is initialized, and are
 *      collected per instance.
 ****************************************************************************/
typedef struct _CpaCyDhStats64 {
    Cpa64U numDhPhase1KeyGenRequests;
    /**< Total number of successful Diffie-Hellman phase 1 key
     * generation requests. */
    Cpa64U numDhPhase1KeyGenRequestErrors;
    /**< Total number of Diffie-Hellman phase 1 key generation requests
     * that had an error and could not be processed. */
    Cpa64U numDhPhase1KeyGenCompleted;
    /**< Total number of Diffie-Hellman phase 1 key generation operations
     * that completed successfully. */
    Cpa64U numDhPhase1KeyGenCompletedErrors;
    /**< Total number of Diffie-Hellman phase 1 key generation operations
     * that could not be completed successfully due to errors. */
    Cpa64U numDhPhase2KeyGenRequests;
    /**< Total number of successful Diffie-Hellman phase 2 key
     * generation requests. */
    Cpa64U numDhPhase2KeyGenRequestErrors;
    /**< Total number of Diffie-Hellman phase 2 key generation requests
     * that had an error and could not be processed. */
    Cpa64U numDhPhase2KeyGenCompleted;
    /**< Total number of Diffie-Hellman phase 2 key generation operations
     * that completed successfully. */
    Cpa64U numDhPhase2KeyGenCompletedErrors;
    /**< Total number of Diffie-Hellman phase 2 key generation operations
     * that could not be completed successfully due to errors. */
} CpaCyDhStats64;

/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Function to implement Diffie-Hellman phase 1 operations.
 *
 * @description
 *      This function may be used to implement the Diffie-Hellman phase 1
 *      operations as defined in the PKCS #3 standard. It may be used to
 *      generate the (local) octet string public value (PV) key.
 *      The prime number sizes specified in RFC 2409, 4306, and part of
 *      RFC 3526 are supported (bit size 6144 from RFC 3536 is not
 *      supported).
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
 * @param[in]  instanceHandle       Instance handle.
 * @param[in]  pDhPhase1Cb          Pointer to a callback function to be invoked
 *                                  when the operation is complete. If the
 *                                  pointer is set to a NULL value the function
 *                                  will operate synchronously.
 * @param[in]  pCallbackTag         Opaque User Data for this specific call.
 *                                  Will be returned unchanged in the callback
 * @param[in]  pPhase1KeyGenData    Structure containing all the data needed
 *                                  to perform the DH Phase 1 key generation
 *                                  operation. The client code allocates the
 *                                  memory for this structure. This component
 *                                  takes ownership of the memory until it is
 *                                  returned in the callback.
 * @param[out] pLocalOctetStringPV  Pointer to memory allocated by the client
 *                                  into which the (local) octet string Public
 *                                  Value (PV) will be written. This value
 *                                  needs to be sent to the remote entity with
 *                                  which Diffie-Hellman is negotiating.
 *                                  The size of this buffer in bytes (as
 *                                  represented by the dataLenInBytes field)
 *                                  MUST be at least big enough to store
 *                                  the public value, which may have a bit
 *                                  length up to that of pPrimeP.
 *                                  On invocation the callback function
 *                                  will contain this parameter in the
 *                                  pOut parameter.
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
 *      When pDhPhase1Cb is non-NULL an asynchronous callback of type
 *      CpaCyGenFlatBufCbFunc is generated in response to this function
 *      call. Any errors generated during processing are reported in the
 *      structure returned in the callback.
 *
 * @see
 *      CpaCyGenFlatBufCbFunc,
 *      CpaCyDhPhase1KeyGenOpData
 *
 *****************************************************************************/
CpaStatus
cpaCyDhKeyGenPhase1(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pDhPhase1Cb,
        void *pCallbackTag,
        const CpaCyDhPhase1KeyGenOpData *pPhase1KeyGenData,
        CpaFlatBuffer *pLocalOctetStringPV);

/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Function to implement Diffie-Hellman phase 2 operations.
 *
 * @description
 *      This function may be used to implement the Diffie-Hellman phase 2
 *      operation as defined in the PKCS #3 standard. It may be used to
 *      generate the Diffie-Hellman shared secret key.
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
 * @param[in]  instanceHandle           Instance handle.
 * @param[in]  pDhPhase2Cb              Pointer to a callback function to be
 *                                      invoked when the operation is complete.
 *                                      If the pointer is set to a NULL value
 *                                      the function will operate synchronously.
 * @param[in]  pCallbackTag             Opaque User Data for this specific
 *                                      call. Will be returned unchanged in
 *                                      the callback.
 * @param[in]  pPhase2SecretKeyGenData  Structure containing all the data
 *                                      needed to perform the DH Phase 2
 *                                      secret key generation operation. The
 *                                      client code allocates the memory for
 *                                      this structure. This component takes
 *                                      ownership of the memory until it is
 *                                      returned in the callback.
 * @param[out] pOctetStringSecretKey    Pointer to memory allocated by the
 *                                      client into which the octet string
 *                                      secret key will be written.
 *                                      The size of this buffer in bytes (as
 *                                      represented by the dataLenInBytes field)
 *                                      MUST be at least big enough to store
 *                                      the public value, which may have a bit
 *                                      length up to that of pPrimeP.
 *                                      On invocation the callback function
 *                                      will contain this parameter in the
 *                                      pOut parameter.
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
 *      When pDhPhase2Cb is non-NULL an asynchronous callback of type
 *      CpaCyGenFlatBufCbFunc is generated in response to this function
 *      call. Any errors generated during processing are reported in the
 *      structure returned in the callback.
 *
 * @see
 *      CpaCyGenFlatBufCbFunc,
 *      CpaCyDhPhase2SecretKeyGenOpData
 *
 *****************************************************************************/
CpaStatus
cpaCyDhKeyGenPhase2Secret(const CpaInstanceHandle instanceHandle,
        const CpaCyGenFlatBufCbFunc pDhPhase2Cb,
        void *pCallbackTag,
        const CpaCyDhPhase2SecretKeyGenOpData *pPhase2SecretKeyGenData,
        CpaFlatBuffer *pOctetStringSecretKey);

/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Query statistics for Diffie-Hellman operations
 *
 * @deprecated
 *      As of v1.3 of the Crypto API, this function has been deprecated,
 *      replaced by @ref cpaCyDhQueryStats64().
 *
 * @description
 *      This function will query a specific Instance handle for Diffie-
 *      Hellman statistics. The user MUST allocate the CpaCyDhStats
 *      structure and pass the reference to that structure into this function
 *      call. This function writes the statistic results into the passed in
 *      CpaCyDhStats structure.
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
 * @param[in]  instanceHandle        Instance handle.
 * @param[out] pDhStats              Pointer to memory into which the statistics
 *                                   will be written.
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
 *
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 * @see
 *      CpaCyDhStats
 *****************************************************************************/
CpaStatus CPA_DEPRECATED
cpaCyDhQueryStats(const CpaInstanceHandle instanceHandle,
        struct _CpaCyDhStats *pDhStats);

/**
 *****************************************************************************
 * @ingroup cpaCyDh
 *      Query statistics (64-bit version) for Diffie-Hellman operations
 *
 * @description
 *      This function will query a specific Instance handle for the 64-bit
 *      version of the Diffie-Hellman statistics. The user MUST allocate the
 *      CpaCyDhStats64 structure and pass the reference to that structure into
 *      this function call. This function writes the statistic results into
 *      the passed in CpaCyDhStats64 structure.
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
 * @param[in]  instanceHandle        Instance handle.
 * @param[out] pDhStats              Pointer to memory into which the statistics
 *                                   will be written.
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
 *
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 * @see
 *      CpaCyDhStats64
 *****************************************************************************/
CpaStatus
cpaCyDhQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCyDhStats64 *pDhStats);

/*****************************************************************************/

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_CY_DH_H */
