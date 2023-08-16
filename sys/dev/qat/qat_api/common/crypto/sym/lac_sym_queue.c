/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_queue.c     Functions for sending/queuing symmetric requests
 *
 * @ingroup LacSym
 *
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_cy_sym.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_debug.h"
#include "icp_adf_transport.h"
#include "lac_sym_queue.h"
#include "lac_sym_qat.h"
#include "lac_session.h"
#include "lac_sym.h"
#include "lac_log.h"
#include "icp_qat_fw_la.h"
#include "lac_sal_types_crypto.h"

#define GetSingleBitFromByte(byte, bit) ((byte) & (1 << (bit)))

/*
*******************************************************************************
* Define public/global function definitions
*******************************************************************************
*/

CpaStatus
LacSymQueue_RequestSend(const CpaInstanceHandle instanceHandle,
			lac_sym_bulk_cookie_t *pRequest,
			lac_session_desc_t *pSessionDesc)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaBoolean enqueued = CPA_FALSE;
	sal_crypto_service_t *pService = (sal_crypto_service_t *)instanceHandle;
	/* Enqueue the message instead of sending directly if:
	 * (i) a blocking operation is in progress
	 * (ii) there are previous requests already in the queue
	 */
	if ((CPA_FALSE == pSessionDesc->nonBlockingOpsInProgress) ||
	    (NULL != pSessionDesc->pRequestQueueTail)) {
		LAC_SPINLOCK(&pSessionDesc->requestQueueLock);

		/* Re-check blockingOpsInProgress and pRequestQueueTail in case
		 * either
		 * changed before the lock was acquired.  The lock is shared
		 * with
		 * the callback context which drains this queue
		 */
		if ((CPA_FALSE == pSessionDesc->nonBlockingOpsInProgress) ||
		    (NULL != pSessionDesc->pRequestQueueTail)) {
			/* Enqueue the message and exit */
			/* The FIFO queue is made up of a head and tail pointer.
			 * The head pointer points to the first/oldest, entry
			 * in the queue, and the tail pointer points to the
			 * last/newest
			 * entry in the queue
			 */

			if (NULL != pSessionDesc->pRequestQueueTail) {
				/* Queue is non-empty. Add this request to the
				 * list */
				pSessionDesc->pRequestQueueTail->pNext =
				    pRequest;
			} else {
				/* Queue is empty. Initialise the head pointer
				 * as well */
				pSessionDesc->pRequestQueueHead = pRequest;
			}

			pSessionDesc->pRequestQueueTail = pRequest;

			/* request is queued, don't send to QAT here */
			enqueued = CPA_TRUE;
		}
		LAC_SPINUNLOCK(&pSessionDesc->requestQueueLock);
	}

	if (CPA_FALSE == enqueued) {
		/* If we send a partial packet request, set the
		 * blockingOpsInProgress
		 * flag for the session to indicate that subsequent requests
		 * must be
		 * queued up until this request completes
		 *
		 * @assumption
		 * If we have got here it means that there were no previous
		 * blocking
		 * operations in progress and, since multiple partial packet
		 * requests
		 * on a given session cannot be issued concurrently, there
		 * should be
		 * no need for a critical section around the following code
		 */
		if (CPA_CY_SYM_PACKET_TYPE_FULL !=
		    pRequest->pOpData->packetType) {
			/* Select blocking operations which this reqest will
			 * complete */
			pSessionDesc->nonBlockingOpsInProgress = CPA_FALSE;
		}

		/* At this point, we're clear to send the request.  For cipher
		 * requests,
		 * we need to check if the session IV needs to be updated.  This
		 * can
		 * only be done when no other partials are in flight for this
		 * session,
		 * to ensure the cipherPartialOpState buffer in the session
		 * descriptor
		 * is not currently in use
		 */
		if (CPA_TRUE == pRequest->updateSessionIvOnSend) {
			if (LAC_CIPHER_IS_ARC4(pSessionDesc->cipherAlgorithm)) {
				memcpy(pSessionDesc->cipherPartialOpState,
				       pSessionDesc->cipherARC4InitialState,
				       LAC_CIPHER_ARC4_STATE_LEN_BYTES);
			} else {
				memcpy(pSessionDesc->cipherPartialOpState,
				       pRequest->pOpData->pIv,
				       pRequest->pOpData->ivLenInBytes);
			}
		}

		/* Send to QAT */
		status = icp_adf_transPutMsg(pService->trans_handle_sym_tx,
					     (void *)&(pRequest->qatMsg),
					     LAC_QAT_SYM_REQ_SZ_LW);

		/* if fail to send request, we need to change
		 * nonBlockingOpsInProgress
		 * to CPA_TRUE
		 */
		if ((CPA_STATUS_SUCCESS != status) &&
		    (CPA_CY_SYM_PACKET_TYPE_FULL !=
		     pRequest->pOpData->packetType)) {
			pSessionDesc->nonBlockingOpsInProgress = CPA_TRUE;
		}
	}
	return status;
}
