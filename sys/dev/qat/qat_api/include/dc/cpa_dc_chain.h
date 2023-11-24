/****************************************************************************
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
 * @file cpa_dc_chain.h
 *
 * @defgroup cpaDcChain Data Compression Chaining API
 *
 * @ingroup cpaDc
 *
 * @description
 *      These functions specify the API for Data Compression Chaining operations.
 *
 * @remarks
 *
 *
 *****************************************************************************/

#ifndef CPA_DC_CHAIN_H
#define CPA_DC_CHAIN_H

#ifdef __cplusplus
extern"C" {
#endif

#include "cpa_dc.h"
#include "cpa_cy_sym.h"


/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Supported operations for compression chaining
 *
 * @description
 *      This enumeration lists the supported operations for compression chaining
 *
 *****************************************************************************/
typedef enum _CpaDcChainOperations
{
    CPA_DC_CHAIN_COMPRESS_THEN_HASH,
    /**< 2 operations for chaining:
     * 1st operation is to perform compression on plain text
     * 2nd operation is to perform hash on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for compression setup data
     * 2nd entry is for hash setup data*/
    CPA_DC_CHAIN_COMPRESS_THEN_ENCRYPT,
    /**< 2 operations for chaining:
     * 1st operation is to perform compression on plain text
     * 2nd operation is to perform encryption on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for compression setup data
     * 2nd entry is for encryption setup data*/
    CPA_DC_CHAIN_COMPRESS_THEN_HASH_ENCRYPT,
    /**< 2 operations for chaining:
     * 1st operation is to perform compression on plain text
     * 2nd operation is to perform hash on compressed text and
     * encryption on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for compression setup data
     * 2nd entry is for hash and encryption setup data*/
    CPA_DC_CHAIN_COMPRESS_THEN_ENCRYPT_HASH,
    /**< 2 operations for chaining:
     * 1st operation is to perform compression on plain text
     * 2nd operation is to perform encryption on compressed text and
     * hash on compressed & encrypted text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for compression setup data
     * 2nd entry is for encryption and hash setup data*/
    CPA_DC_CHAIN_COMPRESS_THEN_AEAD,
    /**< 2 operations for chaining:
     * 1st operation is to perform compression on plain text
     * 2nd operation is to perform AEAD encryption on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for compression setup data
     * 2nd entry is for AEAD encryption setup data*/
    CPA_DC_CHAIN_HASH_THEN_COMPRESS,
    /**< 2 operations for chaining:
     * 1st operation is to perform hash on plain text
     * 2nd operation is to perform compression on plain text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for hash setup data
     * 2nd entry is for compression setup data*/
    CPA_DC_CHAIN_HASH_VERIFY_THEN_DECOMPRESS,
    /**< 2 operations for chaining:
     * 1st operation is to perform hash verify on compressed text
     * 2nd operation is to perform decompression on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for hash setup data
     * 2nd entry is for decompression setup data*/
    CPA_DC_CHAIN_DECRYPT_THEN_DECOMPRESS,
    /**< 2 operations for chaining:
     * 1st operation is to perform decryption on compressed & encrypted text
     * 2nd operation is to perform decompression on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for decryption setup data
     * 2nd entry is for decompression setup data*/
    CPA_DC_CHAIN_HASH_VERIFY_DECRYPT_THEN_DECOMPRESS,
    /**< 2 operations for chaining:
     * 1st operation is to perform hash verify on compressed & encrypted text
     * and decryption on compressed & encrypted text
     * 2nd operation is to perform decompression on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for hash and decryption setup data
     * 2nd entry is for decompression setup data*/
    CPA_DC_CHAIN_DECRYPT_HASH_VERIFY_THEN_DECOMPRESS,
    /**< 2 operations for chaining:
     * 1st operation is to perform decryption on compressed & encrypted text
     * and hash verify on compressed text
     * 2nd operation is to perform decompression on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for decryption and hash setup data
     * 2nd entry is for decompression setup data*/
    CPA_DC_CHAIN_AEAD_THEN_DECOMPRESS,
    /**< 2 operations for chaining:
     * 1st operation is to perform AEAD decryption on compressed & encrypted text
     * 2nd operation is to perform decompression on compressed text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for AEAD decryption setup data
     * 2nd entry is for decompression setup data*/
    CPA_DC_CHAIN_DECOMPRESS_THEN_HASH_VERIFY,
    /**< 2 operations for chaining:
     * 1st operation is to perform decompression on compressed text
     * 2nd operation is to perform hash verify on plain text
     **< 2 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for decompression setup data
     * 2nd entry is for hash setup data*/
    CPA_DC_CHAIN_COMPRESS_THEN_AEAD_THEN_HASH,
    /**< 3 operations for chaining:
     * 1st operation is to perform compression on plain text
     * 2nd operation is to perform AEAD encryption compressed text
     * 3rd operation is to perfom hash on compressed & encrypted text
     **< 3 entries in CpaDcChainSessionSetupData array:
     * 1st entry is for compression setup data
     * 2nd entry is for AEAD encryption setup data
     * 3rd entry is for hash setup data*/
} CpaDcChainOperations;

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Supported session types for data compression chaining.
 *
 * @description
 *      This enumeration lists the supported session types
 *      for data compression chaining.
 *****************************************************************************/
typedef enum _CpaDcChainSessionType
{
    CPA_DC_CHAIN_COMPRESS_DECOMPRESS,
    /**< Indicate the session is for compression or decompression */
    CPA_DC_CHAIN_SYMMETRIC_CRYPTO,
    /**< Indicate the session is for symmetric crypto */
} CpaDcChainSessionType;

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Chaining Session Setup Data.
 * @description
 *      This structure contains data relating to set up chaining sessions. The
 *      client needs to complete the information in this structure in order to
 *      setup chaining sessions.
 *
 ****************************************************************************/
typedef struct _CpaDcChainSessionSetupData {
    CpaDcChainSessionType sessType;
    /**Indicate the type for this session */
    union {
        CpaDcSessionSetupData *pDcSetupData;
        /**< Pointer to compression session setup data */
        CpaCySymSessionSetupData *pCySetupData;
        /**< Pointer to symmectric crypto session setup data */
    };
} CpaDcChainSessionSetupData;

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Compression chaining request input parameters.
 * @description
 *      This structure contains the request information to use with
 *      compression chaining operations.
 *
 ****************************************************************************/
typedef struct _CpaDcChainOpData {
    CpaDcChainSessionType opType;
    /**< Indicate the type for this operation */
    union {
        CpaDcOpData *pDcOp;
        /**< Pointer to compression operation data */
        CpaCySymOpData *pCySymOp;
        /**< Pointer to symmectric crypto operation data */
    };
} CpaDcChainOpData;

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Chaining request results data
 * @description
 *      This structure contains the request results.
 *
 ****************************************************************************/
typedef struct _CpaDcChainRqResults {
    CpaDcReqStatus dcStatus;
    /**< Additional status details from compression accelerator */
    CpaStatus cyStatus;
    /**< Additional status details from symmetric crypto accelerator */
    CpaBoolean verifyResult;
    /**<  This parameter is valid when the verifyDigest option is set in the
     * CpaCySymSessionSetupData structure. A value of CPA_TRUE indicates
     * that the compare succeeded. A value of CPA_FALSE indicates that the
     * compare failed */
    Cpa32U produced;
    /**< Octets produced to the output buffer */
    Cpa32U consumed;
    /**< Octets consumed from the input buffer */
    Cpa32U crc32;
    /**< crc32 checksum produced by chaining operations */
    Cpa32U adler32;
    /**< adler32 checksum produced by chaining operations */
}CpaDcChainRqResults;

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Get the size of the memory required to hold the chaining sessions
 *      information.
 *
 * @description
 *      The client of the Data Compression API is responsible for
 *      allocating sufficient memory to hold chaining sessions information.
 *      This function provides a way for determining the size of chaining
 *      sessions.
 *
 * @context
 *      No restrictions
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] dcInstance             Instance handle.
 * @param[in] operation              The operation for chaining
 * @param[in] numSessions            Number of sessions for the chaining
 * @param[in] pSessionData           Pointer to an array of
 *                                   CpaDcChainSessionSetupData structures.
 *                                   There should be numSessions entries in
 *                                   the array.
 * @param[out] pSessionSize          On return, this parameter will be the size
 *                                   of the memory that will be required by
 *                                   cpaDcChainInitSession() for session data.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @note
 *      Only a synchronous version of this function is provided.
 *
 * @see
 *      cpaDcChainInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaDcChainGetSessionSize(CpaInstanceHandle dcInstance,
        CpaDcChainOperations operation,
        Cpa8U numSessions,
        CpaDcChainSessionSetupData *pSessionData,
        Cpa32U* pSessionSize);

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Initialize data compression chaining session
 *
 * @description
 *      This function is used to initialize compression/decompression chaining
 *      sessions.
 *      This function returns a unique session handle each time this function
 *      is invoked.
 *      If the session has been configured with a callback function, then
 *      the order of the callbacks are guaranteed to be in the same order the
 *      compression or decompression requests were submitted for each session,
 *      so long as a single thread of execution is used for job submission.
 *
 * @context
 *      This is a synchronous function and it cannot sleep. It can be executed
 *      in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]     dcInstance        Instance handle derived from discovery
 *                                  functions.
 * @param[in,out] pSessionHandle    Pointer to a session handle.
 * @param[in]     operation         The operations for chaining
 * @param[in]     numSessions       Number of sessions for chaining
 * @param[in,out] pSessionData      Pointer to an array of
 *                                  CpaDcChainSessionSetupData structures.
 *                                  There should be numSessions entries in
 *                                  the array.
 * @param[in]     callbackFn        For synchronous operation this callback
 *                                  shall be a null pointer.
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
 *      dcInstance has been started using cpaDcStartInstance.
 * @post
 *      None
 * @note
 *      Only a synchronous version of this function is provided.
 *
 *  pSessionData Setup Rules
 *  -# Each element in CpaDcChainSessionSetupData structure array provides
 *     (de)compression or a symmetric crypto session setup data.
 *
 *  -# The supported chaining operations are listed in CpaDcChainOperations.
 *     This enum indicates the number of operations in a chain and the order
 *     in which they are performed.
 *
 *  -# The order of entries in pSessionData[] should be consistent with the
 *     CpaDcChainOperations perform order.
 *     As an example, for CPA_DC_CHAIN_COMPRESS_THEN_ENCRYPT, pSessionData[0]
 *     holds the compression setup data and pSessionData[1] holds the
 *     encryption setup data..
 *
 *  -# The numSessions for each chaining operation are provided in
 *     the comments of enum CpaDcChainOperations.
 *
 *  -# For a (de)compression session, the corresponding
 *     pSessionData[]->sessType should be set to
 *     CPA_DC_CHAIN_COMPRESS_DECOMPRESS and pSessionData[]->pDcSetupData
 *     should point to a CpaDcSessionSetupData structure.
 *
 *  -# For a symmetric crypto session, the corresponding
 *     pSessionData[]->sessType should be set to CPA_DC_CHAIN_SYMMETRIC_CRYPTO
 *     and pSessionData[]->pCySetupData should point to a
 *     CpaCySymSessionSetupData structure.
 *
 *  -# Combined compression sessions are not supported for chaining.
 *
 *  -# Stateful compression is not supported for chaining.
 *
 *  -# Both CRC32 and  Adler32 over the input data are supported for chaining.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcChainInitSession(CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle,
        CpaDcChainOperations operation,
        Cpa8U numSessions,
        CpaDcChainSessionSetupData *pSessionData,
        CpaDcCallbackFn callbackFn);

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *       Reset a compression chaining session.
 *
 * @description
 *      This function will reset a previously initialized session handle.
 *      Reset will fail if outstanding calls still exist for the initialized
 *      session handle.
 *      The client needs to retry the reset function at a later time.
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
 * @param[in]      dcInstance      Instance handle.
 * @param[in,out]  pSessionHandle  Session handle.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaDcStartInstance function.
 *      The session has been initialized via cpaDcChainInitSession function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcChainInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaDcChainResetSession(const CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle);


/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Remove a compression chaining session.
 *
 * @description
 *      This function will remove a previously initialized session handle
 *      and the installed callback handler function. Removal will fail if
 *      outstanding calls still exist for the initialized session handle.
 *      The client needs to retry the remove function at a later time.
 *      The memory for the session handle MUST not be freed until this call
 *      has completed successfully.
 *
 * @context
 *      This is a synchronous function that cannot sleep. It can be executed
 *      in a context that does not permit sleeping.
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
 * @param[in]      dcInstance      Instance handle.
 * @param[in,out]  pSessionHandle  Session handle.
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
 *      The component has been initialized via cpaDcStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcChainInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaDcChainRemoveSession(const CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle);

/**
 *****************************************************************************
 * @ingroup cpaDcChain
 *      Submit a request to perform chaining operations.
 *
 * @description
 *      This function is used to perform chaining operations over data from
 *      the source buffer.
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
 * @param[in]     dcInstance        Target service instance.
 * @param[in,out] pSessionHandle    Session handle.
 * @param[in]     pSrcBuff          Pointer to input data buffer.
 * @param[out]    pDestBuff         Pointer to output data buffer.
 * @param[in]     operation         Operation for the chaining request
 * @param[in]     numOpDatas        The entries size CpaDcChainOpData array
 * @param[in]     pChainOpData      Pointer to an array of CpaDcChainOpData
 *                                  structures. There should be numOpDatas
 *                                  entries in the array.
 * @param[in,out] pResults          Pointer to CpaDcChainRqResults structure.
 * @param[in]     callbackTag       User supplied value to help correlate
 *                                  the callback with its associated request.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_DC_BAD_DATA          The input data was not properly formed.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *     pSessionHandle has been setup using cpaDcChainInitSession()
 * @post
 *     pSessionHandle has session related state information
 * @note
 *     This function passes control to the compression service for chaining
 *     processing, the supported chaining operations are described in
 *     CpaDcChainOperations.
 *
 *  pChainOpData Setup Rules
 *  -# Each element in CpaDcChainOpData structure array holds either a
 *     (de)compression or a symmetric crypto operation data.
 *
 *  -# The order of entries in pChainOpData[] must be consistent with the
 *     order of operations described for the chaining operation in
 *     CpaDcChainOperations.
 *     As an example, for CPA_DC_CHAIN_COMPRESS_THEN_ENCRYPT, pChainOpData[0]
 *     must contain the compression operation data and pChainOpData[1] must
 *     contain the encryption operation data.
 *
 *  -# The numOpDatas for each chaining operation are specified in the
 *     comments for the operation in CpaDcChainOperations.
 *
 *  -# For a (de)compression operation, the corresponding
 *     pChainOpData[]->opType should be set to
 *     CPA_DC_CHAIN_COMPRESS_DECOMPRESS and pChainOpData[]->pDcOp should
 *     point to a CpaDcOpData structure.
 *
 *  -# For a symmetric crypto operation,  the corresponding
 *     pChainOpData[]->opType should be set to
 *     CPA_DC_CHAIN_SYMMETRIC_CRYPTO and pChainOpData[]->pCySymOp should
 *     point to a CpaCySymOpData structure.
 *
 *   -# Stateful compression is not supported for chaining.
 *
 *   -# Partial packet processing is not supported.
 *
 *   This function has identical buffer processing rules as
 *   cpaDcCompressData().
 *
 *   This function has identical checksum processing rules as
 *   cpaDcCompressData(), except:
 *   -# pResults->crc32 is available to application if
 *      CpaDcSessionSetupData->checksum is set to CPA_DC_CRC32
 *
 *   -# pResults->adler32 is available to application if
 *      CpaDcSessionSetupData->checksum is set to CPA_DC_ADLER32
 *
 *   -# Both pResults->crc32 and pResults->adler32 are available if
 *      CpaDcSessionSetupData->checksum is set to CPA_DC_CRC32_ADLER32
 *
 *  Synchronous or asynchronous operation of the API is determined by
 *  the value of the callbackFn parameter passed to cpaDcChainInitSession()
 *  when the sessionHandle was setup. If a non-NULL value was specified
 *  then the supplied callback function will be invoked asynchronously
 *  with the response of this request.
 *
 *  This function has identical response ordering rules as
 *  cpaDcCompressData().
 *
 * @see
 *      cpaDcCompressData
 *
 *****************************************************************************/
CpaStatus
cpaDcChainPerformOp(CpaInstanceHandle dcInstance,
        CpaDcSessionHandle   pSessionHandle,
        CpaBufferList        *pSrcBuff,
        CpaBufferList        *pDestBuff,
        CpaDcChainOperations operation,
        Cpa8U                numOpDatas,
        CpaDcChainOpData     *pChainOpData,
        CpaDcChainRqResults  *pResults,
        void                 *callbackTag );

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_DC_CHAIN_H */
