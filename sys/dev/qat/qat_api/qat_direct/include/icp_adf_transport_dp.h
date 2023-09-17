/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/*****************************************************************************
 * @file icp_adf_transport_dp.h
 *
 * @description
 *      File contains Public API definitions for ADF transport for data plane.
 *
 *****************************************************************************/
#ifndef ICP_ADF_TRANSPORT_DP_H
#define ICP_ADF_TRANSPORT_DP_H

#include "cpa.h"
#include "icp_adf_transport.h"

/*
 * icp_adf_getQueueMemory
 * Data plane support function - returns the pointer to next message on the ring
 * or NULL if there is not enough space.
 */
extern void icp_adf_getQueueMemory(icp_comms_trans_handle trans_handle,
				   Cpa32U numberRequests,
				   void **pCurrentQatMsg);
/*
 * icp_adf_getSingleQueueAddr
 * Data plane support function - returns the pointer to next message on the ring
 * or NULL if there is not enough space - it also updates the shadow tail copy.
 */
extern void icp_adf_getSingleQueueAddr(icp_comms_trans_handle trans_handle,
				       void **pCurrentQatMsg);

/*
 * icp_adf_getQueueNext
 * Data plane support function - increments the tail pointer and returns
 * the pointer to next message on the ring.
 */
extern void icp_adf_getQueueNext(icp_comms_trans_handle trans_handle,
				 void **pCurrentQatMsg);

/*
 * icp_adf_updateQueueTail
 * Data plane support function - Writes the tail shadow copy to the device.
 */
extern void icp_adf_updateQueueTail(icp_comms_trans_handle trans_handle);

/*
 * icp_adf_isRingEmpty
 * Data plane support function - check if the ring is empty
 */
extern CpaBoolean icp_adf_isRingEmpty(icp_comms_trans_handle trans_handle);

/*
 * icp_adf_pollQueue
 * Data plane support function - Poll messages from the queue.
 */
extern CpaStatus icp_adf_pollQueue(icp_comms_trans_handle trans_handle,
				   Cpa32U response_quota);

/*
 * icp_adf_queueDataToSend
 * LAC lite support function - Indicates if there is data on the ring to be
 * send. This should only be called on request rings. If the function returns
 * true then it is ok to call icp_adf_updateQueueTail() function on this ring.
 */
extern CpaBoolean icp_adf_queueDataToSend(icp_comms_trans_handle trans_hnd);

/*
 * icp_adf_dp_getInflightRequests
 * Retrieve in flight requests from the transport handle.
 * Data plane API - no locks.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
extern CpaStatus
icp_adf_dp_getInflightRequests(icp_comms_trans_handle trans_handle,
			       Cpa32U *maxInflightRequests,
			       Cpa32U *numInflightRequests);

#endif /* ICP_ADF_TRANSPORT_DP_H */
