/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_cy_sym_dp.h
 *
 * @defgroup cpaCySymDp Symmetric cryptographic Data Plane API
 *
 * @ingroup cpaCySym
 *
 * @description
 *      These data structures and functions specify the Data Plane API
 *      for symmetric cipher, hash, and combined cipher and hash
 *      operations.
 *
 *      This API is recommended for data plane applications, in which the
 *      cost of offload - that is, the cycles consumed by the driver in
 *      sending requests to the hardware, and processing responses - needs
 *      to be minimized.  In particular, use of this API is recommended
 *      if the following constraints are acceptable to your application:
 *
 *      - Thread safety is not guaranteed.  Each software thread should
 *        have access to its own unique instance (CpaInstanceHandle) to
 *        avoid contention.
 *      - Polling is used, rather than interrupts (which are expensive).
 *        Implementations of this API will provide a function (not
 *        defined as part of this API) to read responses from the hardware
 *        response queue and dispatch callback functions, as specified on
 *        this API.
 *      - Buffers and buffer lists are passed using physical addresses,
 *        to avoid virtual to physical address translation costs.
 *      - For GCM and CCM modes of AES, when performing decryption and
 *        verification, if verification fails, then the message buffer
 *        will NOT be zeroed.  (This is a consequence of using physical
 *        addresses for the buffers.)
 *      - The ability to enqueue one or more requests without submitting
 *        them to the hardware allows for certain costs to be amortized
 *        across multiple requests.
 *      - Only asynchronous invocation is supported.
 *      - There is no support for partial packets.
 *      - Implementations may provide certain features as optional at
 *        build time, such as atomic counters.
 *      - The "default" instance (@ref CPA_INSTANCE_HANDLE_SINGLE) is not
 *        supported on this API.  The specific handle should be obtained
 *        using the instance discovery functions (@ref cpaCyGetNumInstances,
 *        @ref cpaCyGetInstances).
 *
 * @note Performance Trade-Offs
 *      Different implementations of this API may have different performance
 *      trade-offs; please refer to the documentation for your implementation
 *      for details.  However, the following concepts informed the definition
 *      of this API.
 *
 *      The API distinguishes between <i>enqueuing</i> a request and actually
 *      <i>submitting</i> that request to the cryptographic acceleration
 *      engine to be performed.  This allows multiple requests to be enqueued
 *      (either individually or in batch), and then for all enqueued requests
 *      to be submitted in a single operation.  The rationale is that in some
 *      (especially hardware-based) implementations, the submit operation
 *      is expensive; for example, it may incur an MMIO instruction.  The
 *      API allows this cost to be amortized over a number of requests.  The
 *      precise number of such requests can be tuned for optimal
 *      performance.
 *
 *      Specifically:
 *
 *      - The function @ref cpaCySymDpEnqueueOp allows one request to be
 *        enqueued, and optionally for that request (and all previously
 *        enqueued requests) to be submitted.
 *      - The function @ref cpaCySymDpEnqueueOpBatch allows multiple
 *        requests to be enqueued, and optionally for those requests (and all
 *        previously enqueued requests) to be submitted.
 *      - The function @ref cpaCySymDpPerformOpNow enqueues no requests, but
 *        submits all previously enqueued requests.
 *****************************************************************************/

#ifndef CPA_CY_SYM_DP_H
#define CPA_CY_SYM_DP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"
#include "cpa_cy_sym.h"

/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Cryptographic component symmetric session context handle for the
 *      data plane API.
 * @description
 *      Handle to a cryptographic data plane session context. The memory for
 *      this handle is allocated by the client. The size of the memory that
 *      the client needs to allocate is determined by a call to the @ref
 *      cpaCySymDpSessionCtxGetSize or @ref cpaCySymDpSessionCtxGetDynamicSize
 *      functions. The session context memory is initialized with a call to 
 *      the @ref cpaCySymInitSession function.
 *      This memory MUST not be freed until a call to @ref
 *      cpaCySymDpRemoveSession has completed successfully.
 *
 *****************************************************************************/
typedef void * CpaCySymDpSessionCtx;

/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Operation Data for cryptographic data plane API.
 *
 * @description
 *      This structure contains data relating to a request to perform
 *      symmetric cryptographic processing on one or more data buffers.
 *
 *      The physical memory to which this structure points needs to be
 *      at least 8-byte aligned.
 *
 *      All reserved fields SHOULD NOT be written or read by the
 *      calling code.
 *
 * @see
 *        cpaCySymDpEnqueueOp, cpaCySymDpEnqueueOpBatch
 ****************************************************************************/
typedef struct _CpaCySymDpOpData {
    Cpa64U reserved0;
    /**< Reserved for internal usage. */
    Cpa32U cryptoStartSrcOffsetInBytes;
    /**< Starting point for cipher processing, specified as number of bytes
     * from start of data in the source buffer. The result of the cipher
     * operation will be written back into the buffer starting at this
     * location in the destination buffer.
     */
    Cpa32U messageLenToCipherInBytes;
    /**< The message length, in bytes, of the source buffer on which the
     * cryptographic operation will be computed. This must be a multiple of
     * the block size if a block cipher is being used. This is also the
     * same as the result length.
     *
     * @note In the case of CCM (@ref CPA_CY_SYM_HASH_AES_CCM), this value
     * should not include the length of the padding or the length of the
     * MAC; the driver will compute the actual number of bytes over which
     * the encryption will occur, which will include these values.
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC), this field
     * should be set to 0.
     *
     * @note On some implementations, this length may be limited to a 16-bit
     * value (65535 bytes).
     */
    CpaPhysicalAddr iv;
    /**< Initialization Vector or Counter.  Specifically, this is the
     * physical address of one of the following:
     *
     * - For block ciphers in CBC mode, or for Kasumi in F8 mode, or for
     *   SNOW3G in UEA2 mode, this is the Initialization Vector (IV)
     *   value.
     * - For ARC4, this is reserved for internal usage.
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
     */
    Cpa64U reserved1;
    /**< Reserved for internal usage. */
    Cpa32U hashStartSrcOffsetInBytes;
    /**< Starting point for hash processing, specified as number of bytes
     * from start of packet in source buffer.
     *
     * @note For CCM and GCM modes of operation, this value in this field
     * is ignored, and the field is reserved for internal usage.
     * The fields @ref additionalAuthData and @ref pAdditionalAuthData
     * should be set instead.
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC) mode of
     * operation, this field specifies the start of the AAD data in
     * the source buffer.
     */
    Cpa32U messageLenToHashInBytes;
    /**< The message length, in bytes, of the source buffer that the hash
     * will be computed on.
     *
     * @note For CCM and GCM modes of operation, this value in this field
     * is ignored, and the field is reserved for internal usage.
     * The fields @ref additionalAuthData and @ref pAdditionalAuthData
     * should be set instead.
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC) mode of
     * operation, this field specifies the length of the AAD data in the
     * source buffer.
     *
     * @note On some implementations, this length may be limited to a 16-bit
     * value (65535 bytes).
     */
    CpaPhysicalAddr additionalAuthData;
    /**< Physical address of the Additional Authenticated Data (AAD),
     * which is needed for authenticated cipher mechanisms (CCM and
     * GCM), and to the IV for  SNOW3G authentication (@ref
     * CPA_CY_SYM_HASH_SNOW3G_UIA2). For other authentication
     * mechanisms, this value is ignored, and the field is reserved for
     * internal usage.
     *
     * The length of the data pointed to by this field is set up for
     * the session in the @ref CpaCySymHashAuthModeSetupData structure
     * as part of the @ref cpaCySymDpInitSession function call.  This length
     * must not exceed 240 bytes.

     * If AAD is not used, this address must be set to zero.
     *
     * Specifically for CCM (@ref CPA_CY_SYM_HASH_AES_CCM) and GCM (@ref
     * CPA_CY_SYM_HASH_AES_GCM), the caller should be setup as described in
     * the same way as the corresponding field, pAdditionalAuthData, on the
     * "traditional" API (see the @ref CpaCySymOpData).
     *
     * @note For AES-GMAC (@ref CPA_CY_SYM_HASH_AES_GMAC) mode of
     * operation, this field is not used and should be set to 0. Instead
     * the AAD data should be placed in the source buffer.
     *
     */
    CpaPhysicalAddr digestResult;
    /**<  If the digestIsAppended member of the @ref CpaCySymSessionSetupData
     * structure is NOT set then this is the physical address of the location
     * where the digest result should be inserted (in the case of digest
     * generation) or where the purported digest exists (in the case of digest
     * verification).
     *
     * At session registration time, the client specified the digest result
     * length with the digestResultLenInBytes member of the @ref
     * CpaCySymHashSetupData structure. The client must allocate at least
     * digestResultLenInBytes of physically contiguous memory at this location.
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
     */

    CpaInstanceHandle instanceHandle;
    /**< Instance to which the request is to be enqueued.
     * @note A callback function must have been registered on the instance
     * using @ref cpaCySymDpRegCbFunc.
     */
    CpaCySymDpSessionCtx sessionCtx;
    /**< Session context specifying the cryptographic parameters for this
     * request.
     * @note The session must have been created using @ref
     * cpaCySymDpInitSession.
     */
    Cpa32U ivLenInBytes;
    /**< Length of valid IV data pointed to by the pIv parameter.
     *
     * - For block ciphers in CBC mode, or for Kasumi in F8 mode, or for
     *   SNOW3G in UEA2 mode, this is the length of the IV (which
     *   must be the same as the block length of the cipher).
     * - For block ciphers in CTR mode, this is the length of the counter
     *   (which must be the same as the block length of the cipher).
     * - For GCM mode, this is either 12 (for 96-bit IVs) or 16, in which
     *   case pIv points to J0.
     * - For CCM mode, this is the length of the nonce, which can be in the
     *   range 7 to 13 inclusive.
     */
    CpaPhysicalAddr srcBuffer;
    /**< Physical address of the source buffer on which to operate.
     * This is either:
     *
     * - The location of the data, of length srcBufferLen; or,
     * - If srcBufferLen has the special value @ref CPA_DP_BUFLIST, then
     *   srcBuffer contains the location where a @ref CpaPhysBufferList is
     *   stored.  In this case, the CpaPhysBufferList MUST be aligned
     *   on an 8-byte boundary.
     * - For optimum performance, the buffer should only contain the data 
     *   region that the cryptographic operation(s) must be performed on. 
     *   Any additional data in the source buffer may be copied to the 
     *   destination buffer and this copy may degrade performance.
     */
    Cpa32U  srcBufferLen;
    /**< Length of source buffer, or @ref CPA_DP_BUFLIST. */
    CpaPhysicalAddr dstBuffer;
    /**< Physical address of the destination buffer on which to operate.
     * This is either:
     *
     * - The location of the data, of length srcBufferLen; or,
     * - If srcBufferLen has the special value @ref CPA_DP_BUFLIST, then
     *   srcBuffer contains the location where a @ref CpaPhysBufferList is
     *   stored.  In this case, the CpaPhysBufferList MUST be aligned
     *   on an 8-byte boundary.
     *
     * For "in-place" operation, the dstBuffer may be identical to the
     * srcBuffer.
     */
    Cpa32U  dstBufferLen;
    /**< Length of destination buffer, or @ref CPA_DP_BUFLIST. */

    CpaPhysicalAddr thisPhys;
    /**< Physical address of this data structure */

    Cpa8U* pIv;
    /**< Pointer to (and therefore, the virtual address of) the IV field
     * above.
     * Needed here because the driver in some cases writes to this field,
     * in addition to sending it to the accelerator.
     */
    Cpa8U *pAdditionalAuthData;
    /**< Pointer to (and therefore, the virtual address of) the
     * additionalAuthData field above.
     * Needed here because the driver in some cases writes to this field,
     * in addition to sending it to the accelerator.
     */
    void* pCallbackTag;
    /**< Opaque data that will be returned to the client in the function
     * completion callback.
     *
     * This opaque data is not used by the implementation of the API,
     * but is simply returned as part of the asynchronous response.
     * It may be used to store information that might be useful when
     * processing the response later.
     */
} CpaCySymDpOpData;

/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Definition of callback function for cryptographic data plane API.
 *
 * @description
 *      This is the callback function prototype. The callback function is
 *      registered by the application using the @ref cpaCySymDpRegCbFunc
 *      function call, and called back on completion of asynchronous
 *      requests made via calls to @ref cpaCySymDpEnqueueOp or @ref
 *      cpaCySymDpEnqueueOpBatch.
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
 *      No
 *
 * @param[in] pOpData           Pointer to the CpaCySymDpOpData object which
 *                              was supplied as part of the original request.
 * @param[in] status            Status of the operation. Valid values are
 *                              CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                              CPA_STATUS_UNSUPPORTED.
 * @param[in] verifyResult      This parameter is valid when the verifyDigest
 *                              option is set in the CpaCySymSessionSetupData
 *                              structure. A value of CPA_TRUE indicates that
 *                              the compare succeeded. A value of CPA_FALSE
 *                              indicates that the compare failed.
 *
 * @return
 *      None
 * @pre
 *      Component has been initialized.
 *      Callback has been registered with @ref cpaCySymDpRegCbFunc.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaCySymDpRegCbFunc
 *****************************************************************************/
typedef void (*CpaCySymDpCbFunc)(CpaCySymDpOpData *pOpData,
        CpaStatus status,
        CpaBoolean verifyResult);


/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Registration of the operation completion callback function.
 *
 * @description
 *      This function allows a completion callback function to be registered.
 *      The registered callback function is invoked on completion of
 *      asynchronous requests made via calls to @ref cpaCySymDpEnqueueOp
 *      or @ref cpaCySymDpEnqueueOpBatch.
 *
 *      If a callback function was previously registered, it is overwritten.
 *
 * @context
 *      This is a synchronous function and it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] instanceHandle    Instance on which the callback function is to be
 *                                 registered.
 * @param[in] pSymNewCb         Callback function for this instance.

 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      CpaCySymDpCbFunc
 *****************************************************************************/
CpaStatus cpaCySymDpRegCbFunc(const CpaInstanceHandle instanceHandle,
        const CpaCySymDpCbFunc pSymNewCb);

/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Gets the size required to store a session context for the data plane
 *      API.
 *
 * @description
 *      This function is used by the client to determine the size of the memory
 *      it must allocate in order to store the session context. This MUST be
 *      called before the client allocates the memory for the session context
 *      and before the client calls the @ref cpaCySymDpInitSession function.
 *
 *      For a given implementation of this API, it is safe to assume that
 *      cpaCySymDpSessionCtxGetSize() will always return the same size and that
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
 *      context size. The alternate cpaCySymDpSessionCtxGetDynamicSize() 
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
 *      No
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
 *      The component has been initialized.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 * @see
 *      CpaCySymSessionSetupData
 *      cpaCySymDpSessionCtxGetDynamicSize()
 *      cpaCySymDpInitSession()
 *****************************************************************************/
CpaStatus
cpaCySymDpSessionCtxGetSize(const CpaInstanceHandle instanceHandle,
        const CpaCySymSessionSetupData *pSessionSetupData,
        Cpa32U *pSessionCtxSizeInBytes);

/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Gets the minimum size required to store a session context for the data 
 *      plane API.
 *
 * @description
 *      This function is used by the client to determine the smallest size of
 *      the memory it must allocate in order to store the session context. 
 *      This MUST be called before the client allocates the memory for the 
 *      session context and before the client calls the 
 *      @ref cpaCySymDpInitSession function.
 *
 *      This function is an alternate to cpaCySymDpSessionGetSize().
 *      cpaCySymDpSessionCtxGetSize() will return a fixed size which is the 
 *      minimum memory size needed to support all possible setup data parameter 
 *      combinations. cpaCySymDpSessionCtxGetDynamicSize() will return the 
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
 *      No
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
 *      The component has been initialized.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 * @see
 *      CpaCySymSessionSetupData
 *      cpaCySymDpSessionCtxGetSize()
 *      cpaCySymDpInitSession()
 *****************************************************************************/
CpaStatus
cpaCySymDpSessionCtxGetDynamicSize(const CpaInstanceHandle instanceHandle,
        const CpaCySymSessionSetupData *pSessionSetupData,
        Cpa32U *pSessionCtxSizeInBytes);

/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Initialize a session for the symmetric cryptographic data plane API.
 *
 * @description
 *      This function is used by the client to initialize an asynchronous
 *      session context for symmetric cryptographic data plane operations.
 *      The returned session context is the handle to the session and needs to
 *      be passed when requesting cryptographic operations to be performed.
 *
 *      Only sessions created using this function may be used when
 *      invoking functions on this API
 *
 *      The session can be removed using @ref cpaCySymDpRemoveSession.
 *
 * @context
 *      This is a synchronous function and it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] instanceHandle        Instance to which the requests will be
 *                                  submitted.
 * @param[in]  pSessionSetupData    Pointer to session setup data which
 *                                     contains parameters that are static
 *                                     for a given cryptographic session such
 *                                     as operation type, algorithm, and keys
 *                                     for cipher and/or hash operations.
 * @param[out] sessionCtx           Pointer to the memory allocated by the
 *                                  client to store the session context. This
 *                                  memory must be physically contiguous, and
 *                                  its length (in bytes) must be at least as
 *                                  big as specified by a call to @ref
 *                                  cpaCySymDpSessionCtxGetSize.  This memory
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
 *      The component has been initialized.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 * @see
 *      cpaCySymDpSessionCtxGetSize, cpaCySymDpRemoveSession
 *****************************************************************************/
CpaStatus
cpaCySymDpInitSession(CpaInstanceHandle instanceHandle,
        const CpaCySymSessionSetupData *pSessionSetupData,
        CpaCySymDpSessionCtx sessionCtx);

/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Remove (delete) a symmetric cryptographic session for the data plane
 *      API.
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
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in]      instanceHandle    Instance handle.
 * @param[in,out]  sessionCtx        Session context to be removed.
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
 *      Note that this is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      CpaCySymDpSessionCtx,
 *      cpaCySymDpInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaCySymDpRemoveSession(const CpaInstanceHandle instanceHandle,
        CpaCySymDpSessionCtx sessionCtx);


/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *         Enqueue a single symmetric cryptographic request.
 *
 * @description
 *      This function enqueues a single request to perform a cipher,
 *      hash or combined (cipher and hash) operation.  Optionally, the
 *      request is also submitted to the cryptographic engine to be
 *      performed.
 *
 *      See note about performance trade-offs on the @ref cpaCySymDp API.
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted.  On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaCySymDpRegCbFunc) to be invoked.
 *      Callbacks within a session are guaranteed to be in the same order
 *      in which they were submitted.
 *
 *      The following restrictions apply to the pOpData parameter:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The structure MUST reside in physically contiguous memory.
 *      - The reserved fields of the structure SHOULD NOT be written
 *        or read by the calling code.
 *
 * @context
 *      This function will not sleep, and hence can be executed in a context
 *      that does not permit sleeping.
 *
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] pOpData           Pointer to a structure containing the
 *                              request parameters. The client code allocates
 *                              the memory for this structure. This component
 *                              takes ownership of the memory until it is
 *                              returned in the callback, which was registered
 *                              on the instance via @ref cpaCySymDpRegCbFunc.
 *                              See the above Description for restrictions
 *                              that apply to this parameter.
 * @param[in] performOpNow      Flag to specify whether the operation should be
 *                                 performed immediately (CPA_TRUE), or simply
 *                                 enqueued to be performed later (CPA_FALSE).
 *                                 In the latter case, the request is submitted
 *                                 to be performed either by calling this function
 *                                 again with this flag set to CPA_TRUE, or by
 *                                 invoking the function @ref
 *                                 cpaCySymDpPerformOpNow.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The session identified by pOpData->sessionCtx was setup using
 *      @ref cpaCySymDpInitSession.
 *      The instance identified by pOpData->instanceHandle has had a
 *      callback function registered via @ref cpaCySymDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      A callback of type @ref CpaCySymDpCbFunc is generated in response to
 *      this function call. Any errors generated during processing are
 *      reported as part of the callback status code.
 *
 * @see
 *      cpaCySymDpInitSession,
 *      cpaCySymDpPerformOpNow
 *****************************************************************************/
CpaStatus
cpaCySymDpEnqueueOp(CpaCySymDpOpData *pOpData,
        const CpaBoolean performOpNow);


/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *         Enqueue multiple requests to the symmetric cryptographic data plane
 *      API.
 *
 * @description
 *      This function enqueues multiple requests to perform cipher, hash
 *      or combined (cipher and hash) operations.

 *      See note about performance trade-offs on the @ref cpaCySymDp API.
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted.  On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaCySymDpRegCbFunc) to be invoked.
 *      Separate callbacks will be invoked for each request.
 *      Callbacks within a session are guaranteed to be in the same order
 *      in which they were submitted.
 *
 *      The following restrictions apply to each element of the pOpData
 *      array:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The structure MUST reside in physically contiguous memory.
 *      - The reserved fields of the structure SHOULD NOT be
 *        written or read by the calling code.
 *
 * @context
 *      This function will not sleep, and hence can be executed in a context
 *      that does not permit sleeping.
 *
 * @assumptions
 *      Client MUST allocate the request parameters to 8 byte alignment.
 *      Reserved elements of the CpaCySymDpOpData structure MUST be 0.
 *      The CpaCySymDpOpData structure MUST reside in physically
 *      contiguous memory.
 *
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] numberRequests    The number of requests in the array of
 *                              CpaCySymDpOpData structures.
 * @param[in] pOpData           An array of pointers to CpaCySymDpOpData
 *                              structures.  Each of the CpaCySymDpOpData
 *                              structure contains the request parameters for
 *                              that request. The client code allocates the
 *                              memory for this structure. This component takes
 *                              ownership of the memory until it is returned in
 *                              the callback, which was registered on the
 *                              instance via @ref cpaCySymDpRegCbFunc.
 *                              See the above Description for restrictions
 *                              that apply to this parameter.
 * @param[in] performOpNow      Flag to specify whether the operation should be
 *                                 performed immediately (CPA_TRUE), or simply
 *                                 enqueued to be performed later (CPA_FALSE).
 *                                 In the latter case, the request is submitted
 *                                 to be performed either by calling this function
 *                                 again with this flag set to CPA_TRUE, or by
 *                                 invoking the function @ref
 *                                 cpaCySymDpPerformOpNow.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The session identified by pOpData[i]->sessionCtx was setup using
 *      @ref cpaCySymDpInitSession.
 *      The instance identified by pOpData->instanceHandle[i] has had a
 *      callback function registered via @ref cpaCySymDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      Multiple callbacks of type @ref CpaCySymDpCbFunc are generated in
 *      response to this function call (one per request).  Any errors
 *      generated during processing are reported as part of the callback
 *      status code.
 *
 * @see
 *      cpaCySymDpInitSession,
 *      cpaCySymDpEnqueueOp
 *****************************************************************************/
CpaStatus
cpaCySymDpEnqueueOpBatch(const Cpa32U numberRequests,
        CpaCySymDpOpData *pOpData[],
        const CpaBoolean performOpNow);


/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *         Submit any previously enqueued requests to be performed now on the
 *         symmetric cryptographic data plane API.
 *
 * @description
 *      If any requests/operations were enqueued via calls to @ref
 *      cpaCySymDpEnqueueOp and/or @ref cpaCySymDpEnqueueOpBatch, but with
 *      the flag performOpNow set to @ref CPA_FALSE, then these operations
 *      will now be submitted to the accelerator to be performed.
 *
 *      See note about performance trade-offs on the @ref cpaCySymDp API.
 *
 * @context
 *      Will not sleep. It can be executed in a context that does not
 *      permit sleeping.
 *
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] instanceHandle        Instance to which the requests will be
 *                                     submitted.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized.
 *      A cryptographic session has been previously setup using the
 *      @ref cpaCySymDpInitSession function call.
 *
 * @post
 *      None
 *
 * @see
 *      cpaCySymDpEnqueueOp, cpaCySymDpEnqueueOpBatch
 *****************************************************************************/
CpaStatus
cpaCySymDpPerformOpNow(CpaInstanceHandle instanceHandle);


#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_CY_SYM_DP_H */
