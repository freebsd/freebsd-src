/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_cb.h
 *
 * @defgroup LacSymCb Symmetric callback functions
 *
 * @ingroup LacSym
 *
 * Functions to assist with callback processing for the symmetric component
 ***************************************************************************/

#ifndef LAC_SYM_CB_H
#define LAC_SYM_CB_H

/**
 *****************************************************************************
 * @ingroup LacSym
 *      Dequeue pending requests
 * @description
 *      This function is called by a callback function of a blocking
 *      operation (either a partial packet or a hash precompute operation)
 *      in softIRQ context. It dequeues requests for the following reasons:
 *          1. All pre-computes that happened when initialising a session
 *             have completed. Dequeue any requests that were queued on the
 *             session while waiting for the precompute operations to complete.
 *          2. A partial packet request has completed. Dequeue any partials
 *             that were queued for this session while waiting for a previous
 *             partial to complete.
 *
 * @param[in] pSessionDesc  Pointer to the session descriptor
 *
 * @return CpaStatus
 *
 ****************************************************************************/
CpaStatus LacSymCb_PendingReqsDequeue(lac_session_desc_t *pSessionDesc);

/**
 *****************************************************************************
 * @ingroup LacSym
 *      Register symmetric callback function handlers
 *
 * @description
 *      This function registers the symmetric callback handler functions with
 *      the main symmetric callback handler function
 *
 * @return None
 *
 ****************************************************************************/
void LacSymCb_CallbacksRegister(void);

#endif /* LAC_SYM_CB_H */
