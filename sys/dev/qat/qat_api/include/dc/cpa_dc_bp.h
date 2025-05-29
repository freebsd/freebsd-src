/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_dc_bp.h
 *
 * @defgroup cpaDcBp Data Compression Batch and Pack API
 *
 * @ingroup cpaDc
 *
 * @description
 *      These functions specify the API for Data Compression operations related
 *      to the 'Batch and Pack' mode of operation.
 *
 * @remarks
 *
 *
 *****************************************************************************/

#ifndef CPA_DC_BP_H
#define CPA_DC_BP_H

#ifdef __cplusplus
extern"C" {
#endif


#include "cpa_dc.h"

/**
 *****************************************************************************
 * @ingroup cpaDcBp
 *      Batch request input parameters.
 * @description
 *      This structure contains the request information for use with batched
 *      compression operations.
 *
 *
 ****************************************************************************/
typedef struct _CpaDcBatchOpData  {
        CpaDcOpData opData;
        /**< Compression input parameters */
        CpaBufferList *pSrcBuff;
        /**< Input buffer list containing the data to be compressed. */
        CpaBoolean resetSessionState;
        /**< Reset the session state at the beginning of this request within
         * the batch. Only applies to stateful sessions. When this flag is
         * set, the history from previous requests in this session will not be
         * used when compressing the input data for this request in the batch.
         * */
} CpaDcBatchOpData ;

/**
 *****************************************************************************
 * @ingroup cpaDcBp
 *      Submit a batch of requests to compress a batch of input buffers into
 *      a common output buffer. The same output buffer is used for each request
 *      in the batch. This is termed 'batch and pack'.
 *
 * @description
 *      This API consumes data from the input buffer and generates compressed
 *      data in the output buffer.
 *      This API compresses a batch of input buffers and concatenates the
 *      compressed data into the output buffer. A results structure is also
 *      generated for each request in the batch.
 *
 * @context
 *      When called as an asynchronous funnction it cannot sleep. It can be
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
 * @param[in]       dcInstance          Target service instance.
 * @param[in,out]   pSessionHandle      Session handle.
 * @param[in]       numRequests         Number of requests in the batch.
 * @param[in]       pBatchOpData        Pointer to an array of CpaDcBatchOpData
 *                                      structures which contain the input buffers
 *                                      and parameters for each request in the
 *                                      batch. There should be numRequests entries
 *                                      in the array.
 * @param[in]       pDestBuff           Pointer to buffer space for data after
 *                                      compression.
 * @param[in,out]   pResults            Pointer to an array of results structures.
 *                                      There should be numRequests entries in the
 *                                      array.
 * @param[in]       callbackTag         User supplied value to help correlate
 *                                      the callback with its associated
 *                                      request.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_DC_BAD_DATA          The input data was not properly formed.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 *
 * @pre
 *      pSessionHandle has been setup using cpaDcInitSession()
 *      Session must be setup as a stateless session.
 * @note
 *     This function passes control to the compression service for processing
 *
 *  In synchronous mode the function returns the error status returned from the
 *  service. In asynchronous mode the status is returned by the callback
 *  function.
 *
 *  This function may be called repetitively with input until all of the input
 *  has been consumed by the compression service and all the output has been
 *  produced.
 *
 *  When this function returns, it may be that all of the available buffers in
 *  the input list has not been compressed.  This situation will occur when
 *  there is insufficient space in the output buffer.  The calling application
 *  should note the amount of buffers processed, and then submit the request
 *  again, with a new output buffer and with the input buffer list containing
 *  the buffers  that were not previously compressed.
 *
 *  Relationship between input buffers and results buffers.
 *  -# Implementations of this API must not modify the individual
 *     flat buffers of the input buffer list.
 *  -# The implementation communicates the number of buffers
 *     consumed from the source buffer list via pResults->consumed arg.
 *  -# The implementation communicates the amount of data in the
 *     destination buffer list via pResults->produced arg.
 *
 *  Source Buffer Setup Rules
 *  -# The buffer list must have the correct number of flat buffers. This
 *     is specified by the numBuffers element of the CpaBufferList.
 *  -# Each flat buffer must have a pointer to contiguous memory that has
 *     been allocated by the calling application.  The number of octets to be
 *     compressed or decompressed must be stored in the dataLenInBytes element
 *     of the flat buffer.
 *  -# It is permissible to have one or more flat buffers with a zero length
 *     data store.  This function will process all flat buffers until the
 *     destination buffer is full or all source data has been processed.
 *     If a buffer has zero length, then no data will be processed from
 *     that buffer.
 *
 *  Source Buffer Processing Rules.
 *  -# The buffer list is processed in index order - SrcBuff->pBuffers[0]
 *     will be completely processed before SrcBuff->pBuffers[1] begins to
 *     be processed.
 *  -# The application must drain the destination buffers.
 *     If the source data was not completely consumed, the application
 *     must resubmit the request.
 *  -# On return, the pResults->consumed will indicate the number of buffers
 *     consumed from the input buffer list.
 *
 *  Destination Buffer Setup Rules
 *  -# The destination buffer list must have storage for processed data and
 *     for the packed header information.
 *     This means that least two flat buffer must exist in the buffer list.
 *     The first buffer entry will be used for the header information.
 *     Subsequent entries will be used for the compressed data.
 *  -# For each flat buffer in the buffer list, the dataLenInBytes element
 *     must be set to the size of the buffer space.
 *  -# It is permissible to have one or more flat buffers with a zero length
 *     data store.
 *     If a buffer has zero length, then no data will be added to
 *     that buffer.
 *
 *  Destination Buffer Processing Rules.
 *  -# The buffer list is processed in index order.
 *  -# On return, the pResults->produced will indicate the number of bytes
 *     of compressed data written to the output buffers. Note that this
 *     will not include the header information buffer.
 *  -# If processing has not been completed, the application must drain the
 *     destination buffers and resubmit the request. The application must reset
 *     the dataLenInBytes for each flat buffer in the destination buffer list.
 *
 *  Synchronous or Asynchronous operation of the API is determined by
 *  the value of the callbackFn parameter passed to cpaDcInitSession()
 *  when the sessionHandle was setup. If a non-NULL value was specified
 *  then the supplied callback function will be invoked asynchronously
 *  with the response of this request.
 *
 *  Response ordering:
 *  For each session, the implementation must maintain the order of
 *  responses.  That is, if in asynchronous mode, the order of the callback
 *  functions must match the order of jobs submitted by this function.
 *  In a simple synchronous mode implementation, the practice of submitting
 *  a request and blocking on its completion ensure ordering is preserved.
 *
 *  This limitation does not apply if the application employs multiple
 *  threads to service a single session.
 *
 *  If this API is invoked asynchronous, the return code represents
 *  the success or not of asynchronously scheduling the request.
 *  The results of the operation, along with the amount of data consumed
 *  and produced become available when the callback function is invoked.
 *  As such, pResults->consumed and pResults->produced are available
 *  only when the operation is complete.
 *
 *  The application must not use either the source or destination buffers
 *  until the callback has completed.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcBPCompressData( CpaInstanceHandle dcInstance,
        CpaDcSessionHandle  pSessionHandle,
        const Cpa32U        numRequests,
        CpaDcBatchOpData    *pBatchOpData,
        CpaBufferList       *pDestBuff,
        CpaDcRqResults      *pResults,
        void                *callbackTag );

/**
*****************************************************************************
* @ingroup cpaDcBp
*      Function to return the size of the memory which must be allocated for
*      the pPrivateMetaData member of CpaBufferList contained within
*       CpaDcBatchOpData.
*
* @description
*      This function is used to obtain the size (in bytes) required to allocate
*      a buffer descriptor for the pPrivateMetaData member in the
*      CpaBufferList structure when Batch and Pack API are used.
*      Should the function return zero then no meta data is required for the
*      buffer list.
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
* @param[in]  numJobs             The number of jobs defined in the CpaDcBatchOpData
*                                 table.
* @param[out] pSizeInBytes        Pointer to the size in bytes of memory to be
*                                 allocated when the client wishes to allocate
*                                 a cpaFlatBuffer and the Batch and Pack OP data.
*
* @retval CPA_STATUS_SUCCESS        Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
*
* @pre
*      None
* @post
*      None
* @note
*      None
* @see
*      cpaDcBPCompressData()
*
*****************************************************************************/
CpaStatus
cpaDcBnpBufferListGetMetaSize(const CpaInstanceHandle instanceHandle,
        Cpa32U numJobs,
        Cpa32U *pSizeInBytes);


#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_DC_BP_H */
