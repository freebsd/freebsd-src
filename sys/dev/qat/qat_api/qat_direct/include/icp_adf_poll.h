/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/*****************************************************************************
 * @file icp_adf_poll.h
 *
 * @description
 *      File contains Public API Definitions for the polling method.
 *
 *****************************************************************************/
#ifndef ICP_ADF_POLL_H
#define ICP_ADF_POLL_H

#include "cpa.h"
/*
 * icp_adf_pollInstance
 *
 * Description:
 * Poll an instance. In order to poll an instance
 * sal will pass in a table of trans handles from which
 * the ring to be polled can be obtained and subsequently
 * polled.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on polling a ring with data
 *   CPA_STATUS_FAIL      on failure
 *   CPA_STATUS_RETRY     if ring has no data on it
 *                        or ring is already being polled.
 */
CpaStatus icp_adf_pollInstance(icp_comms_trans_handle *trans_hnd,
			       Cpa32U num_transHandles,
			       Cpa32U response_quota);

/*
 * icp_adf_check_RespInstance
 *
 * Description:
 * Check whether an instance is empty or has remaining responses on it. In
 * order to check an instance for the remaining responses, sal will pass in
 * a table of trans handles from which the instance to be checked can be
 * obtained and subsequently checked.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS         if response ring is empty
 *   CPA_STATUS_FAIL            on failure
 *   CPA_STATUS_RETRY           if response ring is not empty
 *   CPA_STATUS_INVALID_PARAM   Invalid parameter passed in
 */
CpaStatus icp_adf_check_RespInstance(icp_comms_trans_handle *trans_hnd,
				     Cpa32U num_transHandles);

#endif /* ICP_ADF_POLL_H */
