/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file icp_sal_nrbg_ht.h
 *
 * @ingroup LacSym
 *
 * @description
 *      This file contains declaration of function used to test the health
 *      of NRBG entropy source.
 *
 *****************************************************************************/
#ifndef ICP_SAL_NRBG_HT_H
#define ICP_SAL_NRBG_HT_H

/**
 ******************************************************************************
 * @ingroup LacSym
 *      NRBG Health Test
 *
 * @description
 *      This function performs a check on the deterministic parts of the
 *      NRBG. It also provides the caller the value of continuous random
 *      number generator test failures for n=64 bits, refer to FIPS 140-2
 *      section 4.9.2 for details. A non-zero value for the counter does
 *      not necessarily indicate a failure; it is statistically possible
 *      that consecutive blocks of 64 bits will be identical, and the RNG
 *      will discard the identical block in such cases. This counter allows
 *      the calling application to monitor changes in this counter and to
 *      use this to decide whether to mark the NRBG as faulty, based on
 *      local policy or statistical model.
 *
 * @context
 *      MUST NOT be executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle               Instance handle.
 * @param[out] pContinuousRngTestFailures  Number of continuous random number
 *                                         generator test failures.
 *
 * @retval CPA_STATUS_SUCCESS              Health test passed.
 * @retval CPA_STATUS_FAIL                 Health test failed.
 * @retval CPA_STATUS_RETRY                Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM        Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE             Error related to system resources.
 *
 * @note
 *      The return value of this function is not impacted by the value
 *      of continuous random generator test failures.
 *
 *****************************************************************************/
CpaStatus icp_sal_nrbgHealthTest(const CpaInstanceHandle instanceHandle,
				 Cpa32U *pContinuousRngTestFailures);

#endif
