/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 ******************************************************************************
 * @file lac_sym_queue.h
 *
 * @defgroup LacSymQueue Symmetric request queueing functions
 *
 * @ingroup LacSym
 *
 * Function prototypes for sending/queuing symmetric requests
 *****************************************************************************/

#ifndef LAC_SYM_QUEUE_H
#define LAC_SYM_QUEUE_H

#include "cpa.h"
#include "lac_session.h"
#include "lac_sym.h"

/**
*******************************************************************************
* @ingroup LacSymQueue
*      Send a request message to the QAT, or queue it if necessary
*
* @description
*      This function will send a request message to the QAT.  However, if a
*      blocking condition exists on the session (e.g. partial packet in flight,
*      precompute in progress), then the message will instead be pushed on to
*      the request queue for the session and will be sent later to the QAT
*      once the blocking condition is cleared.
*
* @param[in]  instanceHandle       Handle for instance of QAT
* @param[in]  pRequest             Pointer to request cookie
* @param[out] pSessionDesc         Pointer to session descriptor
*
*
* @retval CPA_STATUS_SUCCESS        Success
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_RESOURCE       Problem Acquiring system resource
* @retval CPA_STATUS_RETRY          Failed to send message to QAT due to queue
*                                   full condition
*
*****************************************************************************/
CpaStatus LacSymQueue_RequestSend(const CpaInstanceHandle instanceHandle,
				  lac_sym_bulk_cookie_t *pRequest,
				  lac_session_desc_t *pSessionDesc);

#endif /* LAC_SYM_QUEUE_H */
