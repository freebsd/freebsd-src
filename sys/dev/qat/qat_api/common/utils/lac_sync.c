/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file lac_sync.c Utility functions containing synchronous callback support
 *                  functions
 *
 * @ingroup LacSync
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "lac_sync.h"
#include "lac_common.h"

/*
*******************************************************************************
* Define public/global function definitions
*******************************************************************************
*/

/**
 *****************************************************************************
 * @ingroup LacSync
 *****************************************************************************/
void
LacSync_GenWakeupSyncCaller(void *pCallbackTag, CpaStatus status)
{
	lac_sync_op_data_t *pSc = (lac_sync_op_data_t *)pCallbackTag;
	if (pSc != NULL) {
		if (pSc->canceled) {
			QAT_UTILS_LOG("Synchronous operation cancelled.\n");
			return;
		}
		pSc->status = status;
		if (CPA_STATUS_SUCCESS != LAC_POST_SEMAPHORE(pSc->sid)) {
			QAT_UTILS_LOG("Failed to post semaphore.\n");
		}
	}
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *****************************************************************************/
void
LacSync_GenVerifyWakeupSyncCaller(void *pCallbackTag,
				  CpaStatus status,
				  CpaBoolean opResult)
{
	lac_sync_op_data_t *pSc = (lac_sync_op_data_t *)pCallbackTag;
	if (pSc != NULL) {
		if (pSc->canceled) {
			QAT_UTILS_LOG("Synchronous operation cancelled.\n");
			return;
		}
		pSc->status = status;
		pSc->opResult = opResult;
		if (CPA_STATUS_SUCCESS != LAC_POST_SEMAPHORE(pSc->sid)) {
			QAT_UTILS_LOG("Failed to post semaphore.\n");
		}
	}
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *****************************************************************************/
void
LacSync_GenVerifyCb(void *pCallbackTag,
		    CpaStatus status,
		    void *pOpData,
		    CpaBoolean opResult)
{
	LacSync_GenVerifyWakeupSyncCaller(pCallbackTag, status, opResult);
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *****************************************************************************/
void
LacSync_GenFlatBufCb(void *pCallbackTag,
		     CpaStatus status,
		     void *pOpData,
		     CpaFlatBuffer *pOut)
{
	LacSync_GenWakeupSyncCaller(pCallbackTag, status);
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *****************************************************************************/
void
LacSync_GenFlatBufVerifyCb(void *pCallbackTag,
			   CpaStatus status,
			   void *pOpData,
			   CpaBoolean opResult,
			   CpaFlatBuffer *pOut)
{
	LacSync_GenVerifyWakeupSyncCaller(pCallbackTag, status, opResult);
}

/**
 *****************************************************************************
 * @ingroup LacSync
 *****************************************************************************/
void
LacSync_GenDualFlatBufVerifyCb(void *pCallbackTag,
			       CpaStatus status,
			       void *pOpdata,
			       CpaBoolean opResult,
			       CpaFlatBuffer *pOut0,
			       CpaFlatBuffer *pOut1)
{
	LacSync_GenVerifyWakeupSyncCaller(pCallbackTag, status, opResult);
}
