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
 * @file cpa_cy_prime.h
 *
 * @defgroup cpaCyPrime Prime Number Test API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the API for the prime number test operations.
 *
 *      For prime number generation, this API SHOULD be used in conjunction
 *      with the Deterministic Random Bit Generation API (@ref cpaCyDrbg).
 *
 * @note
 *      Large numbers are represented on the QuickAssist API as described
 *      in the Large Number API (@ref cpaCyLn).
 *
 *      In addition, the bit length of large numbers passed to the API
 *      MUST NOT exceed 576 bits for Elliptic Curve operations.
 *****************************************************************************/

#ifndef CPA_CY_PRIME_H
#define CPA_CY_PRIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"

/**
 *****************************************************************************
 * @ingroup cpaCyPrime
 *      Prime Test Operation Data.
 * @description
 *      This structure contains the operation data for the cpaCyPrimeTest
 *      function. The client MUST allocate the memory for this structure and the
 *      items pointed to by this structure. When the structure is passed into
 *      the function, ownership of the memory passes to the function. Ownership
 *      of the memory returns to the client when this structure is returned in
 *      the callback function.
 *
 *      All values in this structure are required to be in Most Significant Byte
 *      first order, e.g. primeCandidate.pData[0] = MSB.
 *
 *      All numbers MUST be stored in big-endian order.
 *
 * @note
 *      If the client modifies or frees the memory referenced in this
 *      structure after it has been submitted to the cpaCyPrimeTest
 *      function, and before it has been returned in the callback, undefined
 *      behavior will result.
 *
 * @see
 *      cpaCyPrimeTest()
 *
 *****************************************************************************/
typedef struct _CpaCyPrimeTestOpData {
    CpaFlatBuffer primeCandidate;
    /**< The prime number candidate to test */
    CpaBoolean performGcdTest;
    /**< A value of CPA_TRUE means perform a GCD Primality Test */
    CpaBoolean performFermatTest;
    /**< A value of CPA_TRUE means perform a Fermat Primality Test */
    Cpa32U numMillerRabinRounds;
    /**<  Number of Miller Rabin Primality Test rounds. Set to 0 to perform
     * zero Miller Rabin tests. The maximum number of rounds supported is 50.
     */
    CpaFlatBuffer millerRabinRandomInput;
    /**<  Flat buffer containing a pointer to an array of n random numbers
     * for Miller Rabin Primality Tests. The size of the buffer MUST be
     *
     *         n * (MAX(64,x))
     *
     * where:
     *
     * - n is the requested number of rounds.
     * - x is the minimum number of bytes required to represent the prime
     *   candidate, i.e. x = ceiling((ceiling(log2(p)))/8).
     *
     * Each random number MUST be greater than 1 and less than the prime
     * candidate - 1, with leading zeroes as necessary.
     */
    CpaBoolean performLucasTest;
    /**< An CPA_TRUE value means perform a Lucas Primality Test */
} CpaCyPrimeTestOpData;

/**
 *****************************************************************************
 * @ingroup cpaCyPrime
 *      Prime Number Test Statistics.
 * @deprecated
 *      As of v1.3 of the Crypto API, this structure has been deprecated,
 *      replaced by @ref CpaCyPrimeStats64.
 * @description
 *      This structure contains statistics on the prime number test operations.
 *      Statistics are set to zero when the component is initialized, and are
 *      collected per instance.
 *
 ****************************************************************************/
typedef struct _CpaCyPrimeStats {
    Cpa32U numPrimeTestRequests;
    /**<  Total number of successful prime number test requests.*/
    Cpa32U numPrimeTestRequestErrors;
    /**<  Total number of prime number test requests that had an
     * error and could not be processed.  */
    Cpa32U numPrimeTestCompleted;
    /**<  Total number of prime number test operations that completed
     * successfully. */
    Cpa32U numPrimeTestCompletedErrors;
    /**<  Total number of prime number test operations that could not be
     * completed successfully due to errors. */
    Cpa32U numPrimeTestFailures;
    /**<  Total number of prime number test operations that executed
     * successfully but the outcome of the test was that the number was not
     * prime. */
} CpaCyPrimeStats CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpaCyPrime
 *      Prime Number Test Statistics (64-bit version).
 * @description
 *      This structure contains a 64-bit version of the statistics on the
 *      prime number test operations.
 *      Statistics are set to zero when the component is initialized, and are
 *      collected per instance.
 ****************************************************************************/
typedef struct _CpaCyPrimeStats64 {
    Cpa64U numPrimeTestRequests;
    /**<  Total number of successful prime number test requests.*/
    Cpa64U numPrimeTestRequestErrors;
    /**<  Total number of prime number test requests that had an
     * error and could not be processed.  */
    Cpa64U numPrimeTestCompleted;
    /**<  Total number of prime number test operations that completed
     * successfully. */
    Cpa64U numPrimeTestCompletedErrors;
    /**<  Total number of prime number test operations that could not be
     * completed successfully due to errors. */
    Cpa64U numPrimeTestFailures;
    /**<  Total number of prime number test operations that executed
     * successfully but the outcome of the test was that the number was not
     * prime. */
} CpaCyPrimeStats64;

/**
 *****************************************************************************
 * @ingroup cpaCyPrime
 *      Definition of callback function invoked for cpaCyPrimeTest
 *      requests.
 *
 * @description
 *      This is the prototype for the cpaCyPrimeTest callback function.
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
 * @param[in] pCallbackTag    User-supplied value to help identify request.
 * @param[in] status          Status of the operation. Valid values are
 *                            CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                            CPA_STATUS_UNSUPPORTED.
 * @param[in] pOpData         Opaque pointer to the Operation data pointer
 *                            supplied in request.
 * @param[in] testPassed      A value of CPA_TRUE means the prime candidate
 *                            is probably prime.
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
 *      cpaCyPrimeTest()
 *
 *****************************************************************************/
typedef void (*CpaCyPrimeTestCbFunc)(void *pCallbackTag,
        CpaStatus status,
        void *pOpData,
        CpaBoolean testPassed);

/**
 *****************************************************************************
 * @ingroup cpaCyPrime
 *      Prime Number Test Function.
 *
 * @description
 *      This function will test probabilistically if a number is prime. Refer
 *      to ANSI X9.80 2005 for details. The primality result will be returned
 *      in the asynchronous callback.
 *
 *      The following combination of GCD, Fermat, Miller-Rabin, and Lucas
 *      testing is supported:
 *      (up to 1x GCD) + (up to 1x Fermat) + (up to 50x Miller-Rabin rounds) +
 *      (up to 1x Lucas)
 *      For example:
 *      (1x GCD) + (25x Miller-Rabin) + (1x Lucas);
 *      (1x GCD) + (1x Fermat);
 *      (50x Miller-rabin);
 *
 *      Tests are always performed in order of increasing complexity, for
 *      example GCD first, then Fermat, then Miller-Rabin, and finally Lucas.
 *
 *      For all of the primality tests, the following prime number "sizes"
 *      (length in bits) are supported: all sizes up to and including 512
 *      bits, as well as sizes 768, 1024, 1536, 2048, 3072 and 4096.
 *
 *      Candidate prime numbers MUST match these sizes accordingly, with
 *      leading zeroes present where necessary.
 *
 *      When this prime number test is used in conjunction with combined
 *      Miller-Rabin and Lucas tests, it may be used as a means of performing
 *      a self test operation on the random data generator.
 *
 *      A response status of ok (pass == CPA_TRUE) means all requested
 *      primality tests passed, and the prime candidate is probably prime
 *      (the exact probability depends on the primality tests requested).
 *      A response status of not ok (pass == CPA_FALSE) means one of the
 *      requested primality tests failed (the prime candidate has been found
 *      to be composite).
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
 * @param[in]  instanceHandle    Instance handle.
 * @param[in]  pCb               Callback function pointer. If this is  set to
 *                               a NULL value the function will operate
 *                               synchronously.
 * @param[in]  pCallbackTag      User-supplied value to help identify request.
 * @param[in]  pOpData           Structure containing all the data needed to
 *                               perform the operation. The client code
 *                               allocates the memory for this structure. This
 *                               component takes ownership of the memory until
 *                               it is returned in the callback.
 * @param[out] pTestPassed       A value of CPA_TRUE means the prime candidate
 *                               is probably prime.
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
 *      When pCb is non-NULL an asynchronous callback of type
 *      CpaCyPrimeTestCbFunc is generated in response to this function call.
 *      For optimal performance, data pointers SHOULD be 8-byte aligned.
 *
 * @see
 *      CpaCyPrimeTestOpData, CpaCyPrimeTestCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaCyPrimeTest(const CpaInstanceHandle instanceHandle,
        const CpaCyPrimeTestCbFunc pCb,
        void *pCallbackTag,
        const CpaCyPrimeTestOpData *pOpData,
        CpaBoolean *pTestPassed);

/******************************************************************************
 * @ingroup cpaCyPrime
 *      Query prime number statistics specific to an instance.
 *
 * @deprecated
 *      As of v1.3 of the Crypto API, this function has been deprecated,
 *      replaced by @ref cpaCyPrimeQueryStats64().
 *
 * @description
 *      This function will query a specific instance for prime number
 *      statistics. The user MUST allocate the CpaCyPrimeStats structure
 *      and pass the reference to that into this function call. This function
 *      will write the statistic results into the passed in
 *      CpaCyPrimeStats structure.
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
 * @param[out] pPrimeStats          Pointer to memory into which the statistics
 *                                  will be written.
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
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 *
 *****************************************************************************/
CpaStatus CPA_DEPRECATED
cpaCyPrimeQueryStats(const CpaInstanceHandle instanceHandle,
        struct _CpaCyPrimeStats *pPrimeStats);


/******************************************************************************
 * @ingroup cpaCyPrime
 *      Query prime number statistics specific to an instance.
 *
 * @description
 *      This function will query a specific instance for the 64-bit
 *      version of the prime number statistics.
 *      The user MUST allocate the CpaCyPrimeStats64 structure
 *      and pass the reference to that into this function call. This function
 *      will write the statistic results into the passed in
 *      CpaCyPrimeStats64 structure.
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
 * @param[out] pPrimeStats          Pointer to memory into which the statistics
 *                                  will be written.
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
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated.
 *****************************************************************************/
CpaStatus
cpaCyPrimeQueryStats64(const CpaInstanceHandle instanceHandle,
        CpaCyPrimeStats64 *pPrimeStats);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_CY_PRIME_H */
