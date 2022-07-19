/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 ***************************************************************************
 * @file lac_sync.h
 *
 * @defgroup LacSync     LAC synchronous
 *
 * @ingroup LacCommon
 *
 * Function prototypes and defines for synchronous support
 *
 ***************************************************************************/

#ifndef LAC_SYNC_H
#define LAC_SYNC_H

#include "cpa.h"
#include "qat_utils.h"
#include "lac_mem.h"

/**
 *****************************************************************************
 * @ingroup LacSync
 *
 * @description
 *      LAC cookie for synchronous support
 *
 *****************************************************************************/
typedef struct lac_sync_op_data_s {
	struct sema *sid;
	/**< Semaphore to signal */
	CpaStatus status;
	/**< Output - Status of the QAT response */
	CpaBoolean opResult;
	/**< Output - Verification of the operation/protocol status */
	CpaBoolean complete;
	/**< Output - Operation is complete */
	CpaBoolean canceled;
	/**< Output - Operation canceled */
} lac_sync_op_data_t;

#define LAC_PKE_SYNC_CALLBACK_TIMEOUT (5000)
/**< @ingroup LacSync
 * Timeout waiting for an async callbacks in msecs.
 * This is derived from the max latency of a PKE request  + 1 sec
 */

#define LAC_SYM_DRBG_POLL_AND_WAIT_TIME_MS (10)
/**< @ingroup LacSyn
 * Default interval DRBG polling in msecs */

#define LAC_SYM_SYNC_CALLBACK_TIMEOUT (300)
/**< @ingroup LacSyn
 * Timeout for wait for symmetric response in msecs
*/

#define LAC_INIT_MSG_CALLBACK_TIMEOUT (1922)
/**< @ingroup LacSyn
 * Timeout for wait for init messages response in msecs
*/

#define DC_SYNC_CALLBACK_TIMEOUT (1000)
/**< @ingroup LacSyn
 * Timeout for wait for compression response in msecs */

#define LAC_SYN_INITIAL_SEM_VALUE (0)
/**< @ingroup LacSyn
 * Initial value of the sync waiting semaphore */

/**
 *******************************************************************************
 * @ingroup LacSync
 *      This function allocates a sync op data cookie
 *      and creates and initialises the QAT Utils semaphore
 *
 * @param[in] ppSyncCallbackCookie  Pointer to synch op data
 *
 * @retval CPA_STATUS_RESOURCE  Failed to allocate the memory for the cookie.
 * @retval CPA_STATUS_SUCCESS   Success
 *
 ******************************************************************************/
static __inline CpaStatus
LacSync_CreateSyncCookie(lac_sync_op_data_t **ppSyncCallbackCookie)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	*ppSyncCallbackCookie =
	    malloc(sizeof(lac_sync_op_data_t), M_QAT, M_WAITOK);

	if (CPA_STATUS_SUCCESS == status) {
		status = LAC_INIT_SEMAPHORE((*ppSyncCallbackCookie)->sid,
					    LAC_SYN_INITIAL_SEM_VALUE);
		(*ppSyncCallbackCookie)->complete = CPA_FALSE;
		(*ppSyncCallbackCookie)->canceled = CPA_FALSE;
	}

	if (CPA_STATUS_SUCCESS != status) {
		LAC_OS_FREE(*ppSyncCallbackCookie);
	}

	return status;
}

/**
 *******************************************************************************
 * @ingroup LacSync
 *      This macro frees a sync op data cookie and destroys the QAT Utils
 *semaphore
 *
 * @param[in] ppSyncCallbackCookie      Pointer to sync op data
 *
 * @return void
 ******************************************************************************/
static __inline CpaStatus
LacSync_DestroySyncCookie(lac_sync_op_data_t **ppSyncCallbackCookie)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	/*
	 * If the operation has not completed, cancel it instead of destroying
	 * the
	 * cookie. Otherwise, the callback might panic. In this case, the cookie
	 * will leak, but it's better than a panic.
	 */
	if (!(*ppSyncCallbackCookie)->complete) {
		QAT_UTILS_LOG(
		    "Attempting to destroy an incomplete sync cookie\n");
		(*ppSyncCallbackCookie)->canceled = CPA_TRUE;
		return CPA_STATUS_FAIL;
	}

	status = LAC_DESTROY_SEMAPHORE((*ppSyncCallbackCookie)->sid);
	LAC_OS_FREE(*ppSyncCallbackCookie);
	return status;
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Function which will wait for a sync callback on a given cookie.
 *
 * @param[in] pSyncCallbackCookie       Pointer to sync op data
 * @param[in] timeOut                   Time to wait for callback (msec)
 * @param[out] pStatus                  Status returned by the callback
 * @param[out] pOpStatus                Operation status returned by callback.
 *
 * @retval CPA_STATUS_SUCCESS   Success
 * @retval CPA_STATUS_SUCCESS   Fail waiting for a callback
 *
 *****************************************************************************/
static __inline CpaStatus
LacSync_WaitForCallback(lac_sync_op_data_t *pSyncCallbackCookie,
			Cpa32S timeOut,
			CpaStatus *pStatus,
			CpaBoolean *pOpStatus)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	status = LAC_WAIT_SEMAPHORE(pSyncCallbackCookie->sid, timeOut);

	if (CPA_STATUS_SUCCESS == status) {
		*pStatus = pSyncCallbackCookie->status;
		if (NULL != pOpStatus) {
			*pOpStatus = pSyncCallbackCookie->opResult;
		}
		pSyncCallbackCookie->complete = CPA_TRUE;
	}

	return status;
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Function which will check for a sync callback on a given cookie.
 *      Returns whether the callback has happened or not, no timeout.
 *
 * @param[in] pSyncCallbackCookie       Pointer to sync op data
 * @param[in] timeOut                   Time to wait for callback (msec)
 * @param[out] pStatus                  Status returned by the callback
 * @param[out] pOpStatus                Operation status returned by callback.
 *
 * @retval CPA_STATUS_SUCCESS           Success
 * @retval CPA_STATUS_FAIL              Fail waiting for a callback
 *
 *****************************************************************************/
static __inline CpaStatus
LacSync_CheckForCallback(lac_sync_op_data_t *pSyncCallbackCookie,
			 CpaStatus *pStatus,
			 CpaBoolean *pOpStatus)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	status = LAC_CHECK_SEMAPHORE(pSyncCallbackCookie->sid);

	if (CPA_STATUS_SUCCESS == status) {
		*pStatus = pSyncCallbackCookie->status;
		if (NULL != pOpStatus) {
			*pOpStatus = pSyncCallbackCookie->opResult;
		}
		pSyncCallbackCookie->complete = CPA_TRUE;
	}

	return status;
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Function which will mark a sync cookie as complete.
 *      If it's known that the callback will not happen it's necessary
 *      to call this, else the cookie can't be destroyed.
 *
 * @param[in] pSyncCallbackCookie       Pointer to sync op data
 *
 * @retval CPA_STATUS_SUCCESS           Success
 * @retval CPA_STATUS_FAIL              Failed to mark as complete
 *
 *****************************************************************************/
static __inline CpaStatus
LacSync_SetSyncCookieComplete(lac_sync_op_data_t *pSyncCallbackCookie)
{
	CpaStatus status = CPA_STATUS_FAIL;

	if (NULL != pSyncCallbackCookie) {
		pSyncCallbackCookie->complete = CPA_TRUE;
		status = CPA_STATUS_SUCCESS;
	}
	return status;
}
/**
 *****************************************************************************
 * @ingroup LacSync
 *      Generic verify callback function.
 * @description
 *      This function is used when the API is called in synchronous mode.
 *      It's assumed the callbackTag holds a lac_sync_op_data_t type
 *      and when the callback is received, this callback shall set the
 *      status element of that cookie structure and kick the sid.
 *      This function may be used directly as a callback function.
 *
 * @param[in]  callbackTag       Callback Tag
 * @param[in]  status            Status of callback
 * @param[out] pOpdata           Pointer to the Op Data
 * @param[out] opResult          Boolean to indicate the result of the operation
 *
 * @return void
 *****************************************************************************/
void LacSync_GenVerifyCb(void *callbackTag,
			 CpaStatus status,
			 void *pOpdata,
			 CpaBoolean opResult);

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Generic flatbuffer callback function.
 * @description
 *      This function is used when the API is called in synchronous mode.
 *      It's assumed the callbackTag holds a lac_sync_op_data_t type
 *      and when the callback is received, this callback shall set the
 *      status element of that cookie structure and kick the sid.
 *      This function may be used directly as a callback function.
 *
 * @param[in]  callbackTag       Callback Tag
 * @param[in]  status            Status of callback
 * @param[in]  pOpdata           Pointer to the Op Data
 * @param[out] pOut              Pointer to the flat buffer
 *
 * @return void
 *****************************************************************************/
void LacSync_GenFlatBufCb(void *callbackTag,
			  CpaStatus status,
			  void *pOpdata,
			  CpaFlatBuffer *pOut);

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Generic flatbuffer verify callback function.
 * @description
 *      This function is used when the API is called in synchronous mode.
 *      It's assumed the callbackTag holds a lac_sync_op_data_t type
 *      and when the callback is received, this callback shall set the
 *      status and opResult element of that cookie structure and
 *      kick the sid.
 *      This function may be used directly as a callback function.
 *
 * @param[in]  callbackTag       Callback Tag
 * @param[in]  status            Status of callback
 * @param[in]  pOpdata           Pointer to the Op Data
 * @param[out] opResult          Boolean to indicate the result of the operation
 * @param[out] pOut              Pointer to the flat buffer
 *
 * @return void
 *****************************************************************************/
void LacSync_GenFlatBufVerifyCb(void *callbackTag,
				CpaStatus status,
				void *pOpdata,
				CpaBoolean opResult,
				CpaFlatBuffer *pOut);

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Generic dual flatbuffer verify callback function.
 * @description
 *      This function is used when the API is called in synchronous mode.
 *      It's assumed the callbackTag holds a lac_sync_op_data_t type
 *      and when the callback is received, this callback shall set the
 *      status and opResult element of that cookie structure and
 *      kick the sid.
 *      This function may be used directly as a callback function.
 *
 * @param[in]  callbackTag       Callback Tag
 * @param[in]  status            Status of callback
 * @param[in]  pOpdata           Pointer to the Op Data
 * @param[out] opResult          Boolean to indicate the result of the operation
 * @param[out] pOut0             Pointer to the flat buffer
 * @param[out] pOut1             Pointer to the flat buffer
 *
 * @return void
 *****************************************************************************/
void LacSync_GenDualFlatBufVerifyCb(void *callbackTag,
				    CpaStatus status,
				    void *pOpdata,
				    CpaBoolean opResult,
				    CpaFlatBuffer *pOut0,
				    CpaFlatBuffer *pOut1);

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Generic wake up function.
 * @description
 *      This function is used when the API is called in synchronous
 *      mode.
 *      It's assumed the callbackTag holds a lac_sync_op_data_t type
 *      and when the callback is received, this callback shall set
 *      the status element of that cookie structure and kick the
 *      sid.
 *      This function maybe called from an async callback.
 *
 * @param[in] callbackTag       Callback Tag
 * @param[in] status            Status of callback
 *
 * @return void
 *****************************************************************************/
void LacSync_GenWakeupSyncCaller(void *callbackTag, CpaStatus status);

/**
 *****************************************************************************
 * @ingroup LacSync
 *      Generic wake up verify function.
 * @description
 *      This function is used when the API is called in synchronous
 *      mode.
 *      It's assumed the callbackTag holds a lac_sync_op_data_t type
 *      and when the callback is received, this callback shall set
 *      the status element and the opResult of that cookie structure
 *      and kick the sid.
 *      This function maybe called from an async callback.
 *
 * @param[in]  callbackTag       Callback Tag
 * @param[in]  status            Status of callback
 * @param[out] opResult          Boolean to indicate the result of the operation
 *
 * @return void
 *****************************************************************************/
void LacSync_GenVerifyWakeupSyncCaller(void *callbackTag,
				       CpaStatus status,
				       CpaBoolean opResult);

#endif /*LAC_SYNC_H*/
