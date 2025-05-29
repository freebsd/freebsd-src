/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_dc_dp.h
 *
 * @defgroup cpaDcDp Data Compression Data Plane API
 *
 * @ingroup cpaDc
 *
 * @description
 *      These data structures and functions specify the Data Plane API
 *      for compression and decompression operations.
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
 *      - The ability to enqueue one or more requests without submitting
 *        them to the hardware allows for certain costs to be amortized
 *        across multiple requests.
 *      - Only asynchronous invocation is supported.
 *      - There is no support for partial packets.
 *      - Implementations may provide certain features as optional at
 *        build time, such as atomic counters.
 *      - There is no support for stateful operations.
 *        - The "default" instance (CPA_INSTANCE_HANDLE_SINGLE) is not
 *          supported on this API.  The specific handle should be obtained
 *          using the instance discovery functions (@ref cpaDcGetNumInstances,
 *          @ref cpaDcGetInstances).
 *
 *****************************************************************************/

#ifndef CPA_DC_DP_H
#define CPA_DC_DP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_dc.h"

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Decompression partial read data.
 * @description
 *      This structure contains configuration related to requesting
 *      specific chunk of decompression data.
 *
 ****************************************************************************/
typedef struct _CpaDcDpPartialReadData {
        Cpa32U bufferOffset;
        /**< Number of bytes to skip in a destination buffer (or buffers list)
         * before writing. At this point only zero is supported.
         */
        Cpa32U dataOffset;
        /**< The offset in the decompressed data of the first byte written to
         * the destination buffer. The data offset length should be an integer
         * multiple of 4KB in order to achieve the best performance.
         */
        Cpa32U length;
        /**< Size of requested decompressed data chunk. The length should be
         * an integer multiple of 4KB in order to achieve the best performance.
         */
} CpaDcDpPartialReadData;

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Operation Data for compression data plane API.
 *
 * @description
 *      This structure contains data relating to a request to perform
 *      compression processing on one or more data buffers.
 *
 *      The physical memory to which this structure points should be
 *      at least 8-byte aligned.
 *
 *      All reserved fields SHOULD NOT be written or read by the
 *      calling code.
 *
 * @see
 *        cpaDcDpEnqueueOp, cpaDcDpEnqueueOpBatch
 ****************************************************************************/
typedef struct _CpaDcDpOpData
{
    Cpa64U          reserved0;
    /**< Reserved for internal use.  Source code should not read or write
      * this field.
      */
    Cpa32U          bufferLenToCompress;
    /**< The number of bytes from the source buffer to compress.  This must be
     * less than, or more typically equal to, the total size of the source
     * buffer (or buffer list).
     */

    Cpa32U          bufferLenForData;
    /**< The maximum number of bytes that should be written to the destination
     * buffer.  This must be less than, or more typically equal to, the total
     * size of the destination buffer (or buffer list).
     */

    Cpa64U          reserved1;
    /**< Reserved for internal use.  Source code should not read or write */

    Cpa64U          reserved2;
    /**< Reserved for internal use.  Source code should not read or write */

    Cpa64U          reserved3;
    /**< Reserved for internal use.  Source code should not read or write */

    CpaDcRqResults      results;
    /**< Results of the operation.  Contents are valid upon completion. */

    CpaInstanceHandle   dcInstance;
    /**< Instance to which the request is to be enqueued */

    CpaDcSessionHandle  pSessionHandle;
    /**< DC Session associated with the stream of requests.
     * This field is only valid when using the session based API functions.
     * This field must be set to NULL if the application wishes to use
     * the No-Session (Ns) API.
     */

    CpaPhysicalAddr     srcBuffer;
    /**< Physical address of the source buffer on which to operate.
     * This is either the location of the data, of length srcBufferLen; or,
     * if srcBufferLen has the special value @ref CPA_DP_BUFLIST, then
     * srcBuffer contains the location where a @ref CpaPhysBufferList is
     * stored.
     */

    Cpa32U          srcBufferLen;
    /**< If the source buffer is a "flat buffer", then this field
     * specifies the size of the buffer, in bytes. If the source buffer
     * is a "buffer list" (of type @ref CpaPhysBufferList), then this field
     * should be set to the value @ref CPA_DP_BUFLIST.
     */

    CpaPhysicalAddr     destBuffer;
    /**< Physical address of the destination buffer on which to operate.
     * This is either the location of the data, of length destBufferLen; or,
     * if destBufferLen has the special value @ref CPA_DP_BUFLIST, then
     * destBuffer contains the location where a @ref CpaPhysBufferList is
     * stored.
     */

    Cpa32U          destBufferLen;
    /**< If the destination buffer is a "flat buffer", then this field
     * specifies the size of the buffer, in bytes.  If the destination buffer
     * is a "buffer list" (of type @ref CpaPhysBufferList), then this field
     * should be set to the value @ref CPA_DP_BUFLIST.
     */

    CpaDcSessionDir sessDirection;
     /**<Session direction indicating whether session is used for
      * compression, decompression.  For the DP implementation,
      * CPA_DC_DIR_COMBINED is not a valid selection.
      */

    CpaBoolean compressAndVerify;
    /**< If set to true, for compression operations, the implementation
     * will verify that compressed data, generated by the compression
     * operation, can be successfully decompressed.
     * This behavior is only supported for stateless compression.
     * This behavior is only supported on instances that support the
     * compressAndVerify capability. */

    CpaBoolean compressAndVerifyAndRecover;
    /**< If set to true, for compression operations, the implementation
     * will automatically recover from a compressAndVerify error.
     * This behavior is only supported for stateless compression.
     * This behavior is only supported on instances that support the
     * compressAndVerifyAndRecover capability.
     * The compressAndVerify field in CpaDcOpData MUST be set to CPA_TRUE
     * if compressAndVerifyAndRecover is set to CPA_TRUE. */

    CpaStatus responseStatus;
    /**< Status of the operation. Valid values are CPA_STATUS_SUCCESS,
     * CPA_STATUS_FAIL and CPA_STATUS_UNSUPPORTED.
     */

    CpaPhysicalAddr thisPhys;
    /**< Physical address of this data structure */

    void* pCallbackTag;
    /**< Opaque data that will be returned to the client in the function
     * completion callback.
     *
     * This opaque data is not used by the implementation of the API,
     * but is simply returned as part of the asynchronous response.
     * It may be used to store information that might be useful when
     * processing the response later.
     */

    CpaDcNsSetupData    *pSetupData;
    /**< Pointer to the No-session (Ns) Setup data for configuration of this
     * request.
     *
     * This @ref CpaDcNsSetupData structure must be initialised when using the
     * Data Plane No-Session (Ns) API. Otherwise it should be set to NULL.
     * When initialized, the existing Data Plane API functions can be used
     * as is.
     */

} CpaDcDpOpData;

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Definition of callback function for compression data plane API.
 *
 * @description
 *      This is the callback function prototype. The callback function is
 *      registered by the application using the @ref cpaDcDpRegCbFunc
 *      function call, and called back on completion of asynchronous
 *      requests made via calls to @ref cpaDcDpEnqueueOp or @ref
 *      cpaDcDpEnqueueOpBatch.
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
 * @param[in] pOpData           Pointer to the @ref CpaDcDpOpData object which
 *                              was supplied as part of the original request.

 * @return
 *      None
 * @pre
 *      Instance has been initialized.
 *      Callback has been registered with @ref cpaDcDpRegCbFunc.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      @ref cpaDcDpRegCbFunc
 *****************************************************************************/
typedef void (*CpaDcDpCallbackFn)(CpaDcDpOpData *pOpData);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Get the size of the memory required to hold the data plane
 *      session information.
 *
 * @description
 *
 *      The client of the Data Compression API is responsible for
 *      allocating sufficient memory to hold session information. This
 *      function provides a means for determining the size of the session
 *      information and statistics information.
 *
 * @context
 *      No restrictions
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] dcInstance            Instance handle.
 * @param[in] pSessionData          Pointer to a user instantiated structure
 *                                  containing session data.
 * @param[out] pSessionSize         On return, this parameter will be the size
 *                                  of the memory that will be
 *                                  required by cpaDcInitSession() for session
 *                                  data.
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
 *      Session data is expected to include interim checksum values, various
 *      counters and other session related data that needs to persist between
 *      invocations.
 *      For a given implementation of this API, it is safe to assume that
 *      cpaDcDpGetSessionSize() will always return the same session size and
 *      that the size will not be different for different setup data
 *      parameters. However, it should be noted that the size may change:
 *       (1) between different implementations of the API (e.g. between software
 *           and hardware implementations or between different hardware
 *           implementations)
 *       (2) between different releases of the same API implementation
 *
 * @see
 *      cpaDcDpInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaDcDpGetSessionSize(CpaInstanceHandle dcInstance,
        CpaDcSessionSetupData* pSessionData,
        Cpa32U* pSessionSize );


/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Initialize compression or decompression data plane session.
 *
 * @description
 *      This function is used to initialize a compression/decompression session.
 *      A single session can be used for both compression and decompression
 *      requests.  Clients MUST register a callback
 *      function for the compression service using this function.
 *      This function returns a unique session handle each time this function
 *      is invoked.
 *      The order of the callbacks are guaranteed to be in the same order the
 *      compression or decompression requests were submitted for each session,
 *      so long as a single thread of execution is used for job submission.
 *
 * @context
 *      This function may be called from any context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]       dcInstance      Instance handle derived from discovery
 *                                  functions.
 * @param[in,out]   pSessionHandle  Pointer to a session handle.
 * @param[in,out]   pSessionData    Pointer to a user instantiated structure
 *                                  containing session data.
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
 *      dcInstance has been started using @ref cpaDcStartInstance.
 * @post
 *      None
 * @note
 *      Only a synchronous version of this function is provided.
 *
 *      This initializes opaque data structures in the session handle. Data
 *      compressed under this session will be compressed to the level
 *      specified in the pSessionData structure. Lower compression level
 *      numbers indicate a request for faster compression at the
 *      expense of compression ratio.  Higher compression level numbers
 *      indicate a request for higher compression ratios at the expense of
 *      execution time.
 *
 *      The session is opaque to the user application and the session handle
 *      contains job specific data.
 *
 *      The window size specified in the pSessionData must match exactly
 *      one of the supported window sizes specified in the capability
 *      structure.  If a bi-directional session is being initialized, then
 *      the window size must be valid for both compress and decompress.
 *
 *      Note stateful sessions are not supported by this API.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcDpInitSession( CpaInstanceHandle       dcInstance,
        CpaDcSessionHandle              pSessionHandle,
        CpaDcSessionSetupData           *pSessionData );


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression Session Update Function.
 *
 * @description
 *      This function is used to modify some select compression parameters
 *      of a previously initialized session handlei for a data plane session.
 *      Th update will fail if resources required for the new session settings
 *      are not available. Specifically, this function may fail if no
 *      intermediate buffers are associated with the instance, and the
 *      intended change would require these buffers.
 *      This function can be called at any time after a successful call of
 *      cpaDcDpInitSession().
 *      This function does not change the parameters to compression request
 *      already in flight.
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
 *      No
 *
 * @param[in]      dcInstance            Instance handle.
 * @param[in,out]  pSessionHandle        Session handle.
 * @param[in]      pSessionUpdateData    Session Data.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting.
 *                                   Resubmit the request
 *
 * @pre
 *      The component has been initialized via cpaDcStartInstance function.
 *      The session has been initialized via cpaDcDpInitSession function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcDpInitSession()
 *
 *****************************************************************************/
CpaStatus cpaDcDpUpdateSession( const CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle,
        CpaDcSessionUpdateData *pSessionUpdateData );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression Data Plane Session Remove Function.
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
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via @ref cpaDcStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      @ref cpaDcDpInitSession
 *
 *****************************************************************************/
CpaStatus
cpaDcDpRemoveSession(const CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle );

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Registration of the operation completion callback function.
 *
 * @description
 *      This function allows a completion callback function to be registered.
 *      The registered callback function is invoked on completion of
 *      asynchronous requests made via calls to @ref cpaDcDpEnqueueOp
 *      or @ref cpaDcDpEnqueueOpBatch.
 * @context
 *      This is a synchronous function and it cannot sleep. It can be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] dcInstance     Instance on which the callback function is to be
 *                           registered.
 * @param[in] pNewCb         Callback function for this instance.
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
 *      Instance has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaDcDpCbFunc
 *****************************************************************************/
CpaStatus cpaDcDpRegCbFunc(const CpaInstanceHandle dcInstance,
        const CpaDcDpCallbackFn pNewCb);

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Enqueue a single compression or decompression request.
 *
 * @description
 *      This function enqueues a single request to perform a compression,
 *      decompression operation.
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted.  On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaDcDpRegCbFunc) to be invoked.
 *      Callbacks within a session are guaranteed
 *      to be in the same order in which they were submitted.
 *
 *      The following restrictions apply to the pOpData parameter:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The reserved fields of the structure MUST NOT be written to
 *        or read from.
 *      - The structure MUST reside in physically contiguous memory.
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
 *                              on the instance via @ref cpaDcDpRegCbFunc.
 *                              See the above Description for some restrictions
 *                              that apply to this parameter.
 * @param[in] performOpNow      Flag to indicate whether the operation should be
 *                              performed immediately (CPA_TRUE), or simply
 *                              enqueued to be performed later (CPA_FALSE).
 *                              In the latter case, the request is submitted
 *                              to be performed either by calling this function
 *                              again with this flag set to CPA_TRUE, or by
 *                              invoking the function @ref
 *                              cpaDcDpPerformOpNow.
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
 *      The session identified by pOpData->pSessionHandle was setup using
 *      @ref cpaDcDpInitSession OR pOpData->pSetupData data structure was
 *      initialized for No-Session (Ns) usage.
 *      The instance identified by pOpData->dcInstance has had a
 *      callback function registered via @ref cpaDcDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      A callback of type @ref CpaDcDpCallbackFn is generated in
 *      response to this function call. Any errors generated during
 *      processing are reported as part of the callback status code.
 *
 * @see
 *      @ref cpaDcDpPerformOpNow
 *****************************************************************************/
CpaStatus
cpaDcDpEnqueueOp(CpaDcDpOpData *pOpData,
        const CpaBoolean performOpNow);

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Enqueue a single decompression request with partial read configuration.
 *      See @CpaDcDpPartialReadData for more details.
 *
 * @description
 *      This function enqueues a single request to perform a decompression
 *      operation and allows to specify particular region of decompressed
 *      data to be placed in to the destination buffer (or buffer list).
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted. On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaDcDpRegCbFunc) to be invoked.
 *      Callbacks within a session are guaranteed to be in the same order
 *      in which they were submitted.
 *
 *      The following restrictions apply to the pOpData parameter:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The reserved fields of the structure MUST NOT be written to
 *        or read from.
 *      - The structure MUST reside in physically contiguous memory.
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
 * @param[in,out] pOpData       See @ref cpaDcDpEnqueueOp pOpData description.
 *
 * @param[in] pPartReadData     Pointer to a structure containing the partial
 *                              read configuration parameters.
 *                              See @CpaDcDpPartialReadData for more details.
 *
 * @param[in] performOpNow      See @ref cpaDcDpEnqueueOp performOpNow input
 *                              parameter.
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
 *      The session identified by pOpData->pSessionHandle was setup using
 *      @ref cpaDcDpInitSession. The instance identified by pOpData->dcInstance
 *      has had a callback function registered via @ref cpaDcDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      A callback of type @ref CpaDcDpCallbackFn is generated in
 *      response to this function call. Any errors generated during
 *      processing are reported as part of the callback status code.
 *
 * @see
 *      @ref cpaDcDpPerformOpNow
 *****************************************************************************/
CpaStatus
cpaDcDpEnqueueOpWithPartRead(CpaDcDpOpData *pOpData,
        CpaDcDpPartialReadData *pPartReadData,
        const CpaBoolean performOpNow);

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Enqueue a single compression request with an option set to zero-fill
 *      data after the compression output in the leftover bytes.
 *
 * @description
 *      This function enqueues a single request to perform a compression
 *      operation with zero-filling leftover bytes with 4KB alignment
 *      in the destination buffer (or buffer list).
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted. On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaDcDpRegCbFunc) to be invoked.
 *      Callbacks within a session are guaranteed to be in the same order
 *      in which they were submitted.
 *
 *      The following restrictions apply to the pOpData parameter:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The reserved fields of the structure MUST NOT be written to
 *        or read from.
 *      - The structure MUST reside in physically contiguous memory.
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
 * @param[in,out] pOpData       See @ref cpaDcDpEnqueueOp pOpData description.
 *
 * @param[in] performOpNow      See @ref cpaDcDpEnqueueOp performOpNow input
 *                              parameter.
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
 *      The session identified by pOpData->pSessionHandle was setup using
 *      @ref cpaDcDpInitSession. The instance identified by pOpData->dcInstance
 *      has had a callback function registered via @ref cpaDcDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      A callback of type @ref CpaDcDpCallbackFn is generated in
 *      response to this function call. Any errors generated during
 *      processing are reported as part of the callback status code.
 *
 * @see
 *      @ref cpaDcDpPerformOpNow
 *****************************************************************************/
CpaStatus
cpaDcDpEnqueueOpWithZeroPad(CpaDcDpOpData *pOpData,
        const CpaBoolean performOpNow);

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Enqueue multiple requests to the compression data plane API.
 *
 * @description
 *      This function enqueues multiple requests to perform compression or
 *      decompression operations.
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted.  On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaDcDpRegCbFunc) to be invoked.
 *      Separate callbacks will be invoked for each request.
 *      Callbacks within a session and at the same priority are guaranteed
 *      to be in the same order in which they were submitted.
 *
 *      The following restrictions apply to each element of the pOpData
 *      array:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The reserved fields of the structure MUST be set to zero.
 *      - The structure MUST reside in physically contiguous memory.
 *
 * @context
 *      This function will not sleep, and hence can be executed in a context
 *      that does not permit sleeping.
 *
 * @assumptions
 *      Client MUST allocate the request parameters to 8 byte alignment.
 *      Reserved elements of the CpaDcDpOpData structure MUST not used
 *      The CpaDcDpOpData structure MUST reside in physically
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
 *                              CpaDcDpOpData structures.
 * @param[in] pOpData           An array of pointers to CpaDcDpOpData
 *                              structures.  Each CpaDcDpOpData
 *                              structure contains the request parameters for
 *                              that request. The client code allocates the
 *                              memory for this structure. This component takes
 *                              ownership of the memory until it is returned in
 *                              the callback, which was registered on the
 *                              instance via @ref cpaDcDpRegCbFunc.
 *                              See the above Description for some restrictions
 *                              that apply to this parameter.
 * @param[in] performOpNow      Flag to indicate whether the operation should be
 *                              performed immediately (CPA_TRUE), or simply
 *                              enqueued to be performed later (CPA_FALSE).
 *                              In the latter case, the request is submitted
 *                              to be performed either by calling this function
 *                              again with this flag set to CPA_TRUE, or by
 *                              invoking the function @ref
 *                              cpaDcDpPerformOpNow.
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
 *      The session identified by pOpData[i]->pSessionHandle was setup using
 *      @ref cpaDcDpInitSession OR pOpData[i]->pSetupData data structure was
 *      initialized for No-Session (Ns) usage.
 *      The instance identified by pOpData[i]->dcInstance has had a
 *      callback function registered via @ref cpaDcDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      Multiple callbacks of type @ref CpaDcDpCallbackFn are generated in
 *      response to this function call (one per request).  Any errors
 *      generated during processing are reported as part of the callback
 *      status code.
 *
 * @see
 *      cpaDcDpEnqueueOp
 *****************************************************************************/
CpaStatus
cpaDcDpEnqueueOpBatch(const Cpa32U numberRequests,
        CpaDcDpOpData *pOpData[],
        const CpaBoolean performOpNow);

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Enqueue multiple decompression request with partial read configuration.
 *      See @CpaDcDpPartialReadData for more details.
 *
 * @description
 *      This function enqueues multiple requests to perform decompression
 *      operations and allows to specify particular region of decompressed
 *      data to be placed in to the destination buffer (or buffer list) for
 *      each individual request.
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted.  On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaDcDpRegCbFunc) to be invoked.
 *      Separate callbacks will be invoked for each request.
 *      Callbacks within a session and at the same priority are guaranteed
 *      to be in the same order in which they were submitted.
 *
 *      The following restrictions apply to each element of the pOpData
 *      array:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The reserved fields of the structure MUST be set to zero.
 *      - The structure MUST reside in physically contiguous memory.
 *
 * @context
 *      See @ref cpaDcDpEnqueueOpBatch context.
 *
 * @assumptions
 *      See @ref cpaDcDpEnqueueOpBatch assumptions.
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
 *                              CpaDcDpOpData structures.
 *
 * @param[in,out] pOpData       See @ref cpaDcDpEnqueueOpBatch pOpData for more
 *                              details.
 *
 * @param[in] pPartReadData     An array of pointers to a structures containing
 *                              the partial read configuration parameters.
 *                              See @CpaDcDpPartialReadData for more details.
 *
 * @param[in] performOpNow      See @ref cpaDcDpEnqueueOpBatch performOpNow
 *                              input parameter.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 *
 * @pre
 *      The session identified by pOpData[i]->pSessionHandle was setup using
 *      @ref cpaDcDpInitSession. The instance identified by
 *      pOpData[i]->dcInstance has had a callback function registered via
 *      @ref cpaDcDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      Multiple callbacks of type @ref CpaDcDpCallbackFn are generated in
 *      response to this function call (one per request).  Any errors
 *      generated during processing are reported as part of the callback
 *      status code.
 *
 * @see
 *      @ref cpaDcDpEnqueueOp
 *****************************************************************************/
CpaStatus
cpaDcDpEnqueueOpWithPartReadBatch(const Cpa32U numberRequests,
        CpaDcDpOpData *pOpData[],
        CpaDcDpPartialReadData *pPartReadData[],
        const CpaBoolean performOpNow);

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Enqueue multiple compression requests with an option set to zero-fill
 *      data after the compression output in the leftover bytes.
 *
 * @description
 *      This function enqueues multiple requests to perform compression
 *      operations with an option set to zero-fill leftover bytes in the
 *      destination buffer (of buffer list) for each individual request.
 *      Please note that optional zero-filling leftover output buffer bytes
 *      is aligned to 4KB.
 *
 *      The function is asynchronous; control is returned to the user once
 *      the request has been submitted.  On completion of the request, the
 *      application may poll for responses, which will cause a callback
 *      function (registered via @ref cpaDcDpRegCbFunc) to be invoked.
 *      Separate callbacks will be invoked for each request.
 *      Callbacks within a session and at the same priority are guaranteed
 *      to be in the same order in which they were submitted.
 *
 *      The following restrictions apply to each element of the pOpData
 *      array:
 *
 *      - The memory MUST be aligned on an 8-byte boundary.
 *      - The reserved fields of the structure MUST be set to zero.
 *      - The structure MUST reside in physically contiguous memory.
 *
 * @context
 *      See @ref cpaDcDpEnqueueOpBatch context.
 *
 * @assumptions
 *      See @ref cpaDcDpEnqueueOpBatch assumptions.
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
 *                              CpaDcDpOpData structures.
 *
 * @param[in,out] pOpData       See @ref cpaDcDpEnqueueOpBatch pOpData for more
 *                              details.
 *
 * @param[in] performOpNow      See @ref cpaDcDpEnqueueOpBatch performOpNow
 *                              input parameter.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 *
 * @pre
 *      The session identified by pOpData[i]->pSessionHandle was setup using
 *      @ref cpaDcDpInitSession. The instance identified by
 *      pOpData[i]->dcInstance has had a callback function registered via
 *      @ref cpaDcDpRegCbFunc.
 *
 * @post
 *      None
 *
 * @note
 *      Multiple callbacks of type @ref CpaDcDpCallbackFn are generated in
 *      response to this function call (one per request).  Any errors
 *      generated during processing are reported as part of the callback
 *      status code.
 *
 * @see
 *      @ref cpaDcDpEnqueueOp
 *****************************************************************************/
CpaStatus
cpaDcDpEnqueueOpWithZeroPadBatch(const Cpa32U numberRequests,
        CpaDcDpOpData *pOpData[],
        const CpaBoolean performOpNow);

/**
 *****************************************************************************
 * @ingroup cpaDcDp
 *      Submit any previously enqueued requests to be performed now on the
 *      compression data plane API.
 *
 * @description
 *      This function triggers processing of previously enqueued requests on the
 *      referenced instance.
 *
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
 * @param[in] dcInstance        Instance to which the requests will be
 *                                  submitted.
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
 *      The component has been initialized via @ref cpaDcStartInstance function.
 *      A compression session has been previously setup using the
 *      @ref cpaDcDpInitSession function call.
 *
 * @post
 *      None
 *
 * @see
 *      cpaDcDpEnqueueOp, cpaDcDpEnqueueOpBatch
 *****************************************************************************/
CpaStatus
cpaDcDpPerformOpNow(CpaInstanceHandle dcInstance);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Function to return the "partial read" feature support.
 *
 * @description
 *      This function is used to determine if given instance supports
 *      "partial read" feature.
 *
 * @context
 *      This function may be called from any context.
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
 * @param[in]  instanceHandle      Handle to an instance of this API.
 * @param[out] pFlag               Pointer to boolean flag which indicates
 *                                 whether a feature is supported.
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
 *      None
 * @see
 *      cpaDcQueryCapabilities()
 *
 *****************************************************************************/
CpaStatus
cpaDcDpIsPartReadSupported(const CpaInstanceHandle instanceHandle,
        CpaBoolean *pFlag);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Function to return the "zero pad" feature support.
 *
 * @description
 *      This function is used to determine if given instance supports
 *      "zero pad" feature.
 *
 * @context
 *      This function may be called from any context.
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
 * @param[in]  instanceHandle      Handle to an instance of this API.
 * @param[out] pFlag               Pointer to boolean flag which indicates
 *                                 whether a feature is supported.
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
 *      None
 * @see
 *      cpaDcQueryCapabilities()
 *
 *****************************************************************************/
CpaStatus
cpaDcDpIsZeroPadSupported(const CpaInstanceHandle instanceHandle,
        CpaBoolean *pFlag);


#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_DC_DP_H */

