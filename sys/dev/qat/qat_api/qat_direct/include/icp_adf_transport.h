/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/*****************************************************************************
 * @file icp_adf_transport.h
 *
 * @description
 *      File contains Public API Definitions for ADF transport.
 *
 *****************************************************************************/
#ifndef ICP_ADF_TRANSPORT_H
#define ICP_ADF_TRANSPORT_H

#include "cpa.h"

/*
 * Enumeration on Transport Types exposed
 */
typedef enum icp_transport_type_e {
	ICP_TRANS_TYPE_NONE = 0,
	ICP_TRANS_TYPE_ETR,
	ICP_TRANS_TYPE_DP_ETR,
	ICP_TRANS_TYPE_ADMINREG,
	ICP_TRANS_TYPE_DELIMIT
} icp_transport_type;

/*
 * Enumeration on response delivery method
 */
typedef enum icp_resp_deliv_method_e {
	ICP_RESP_TYPE_NONE = 0,
	ICP_RESP_TYPE_IRQ,
	ICP_RESP_TYPE_POLL,
	ICP_RESP_TYPE_DELIMIT
} icp_resp_deliv_method;

/*
 * Unique identifier of a transport handle
 */
typedef Cpa32U icp_trans_identifier;

/*
 * Opaque Transport Handle
 */
typedef void *icp_comms_trans_handle;

/*
 * Function Pointer invoked when a set of messages is received for the given
 * transport handle
 */
typedef void (*icp_trans_callback)(void *pMsg);

/*
 * icp_adf_getDynInstance
 *
 * Description:
 * Get an available instance from dynamic instance pool
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 *
 */
CpaStatus icp_adf_getDynInstance(icp_accel_dev_t *accel_dev,
				 adf_service_type_t stype,
				 Cpa32U *pinstance_id);

/*
 * icp_adf_putDynInstance
 *
 * Description:
 * Put back an instance to dynamic instance pool
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 *
 */
CpaStatus icp_adf_putDynInstance(icp_accel_dev_t *accel_dev,
				 adf_service_type_t stype,
				 Cpa32U instance_id);

/*
 * icp_adf_getNumAvailDynInstance
 *
 * Description:
 * Get the number of the available dynamic instances
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 *
 */
CpaStatus icp_adf_getNumAvailDynInstance(icp_accel_dev_t *accel_dev,
					 adf_service_type_t stype,
					 Cpa32U *num);

/*
 * icp_adf_transGetFdForHandle
 *
 * Description:
 * Get a file descriptor for a particular transaction handle.
 * If more than one transaction handler
 * are ever present, this will need to be refactored to
 * return the appropriate fd of the appropriate bank.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 *
 *
 */
CpaStatus icp_adf_transGetFdForHandle(icp_comms_trans_handle trans_hnd,
				      int *fd);

/*
 * icp_adf_transCreateHandle
 *
 * Description:
 * Create a transport handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 *
 *   The message size is variable: requests can be 64 or 128 bytes, responses
 * can be 16, 32 or 64 bytes.
 *   Supported num_msgs:
 *     32, 64, 128, 256, 512, 1024, 2048 number of messages.
 *
 */
CpaStatus icp_adf_transCreateHandle(icp_accel_dev_t *accel_dev,
				    icp_transport_type trans_type,
				    const char *section,
				    const Cpa32U accel_nr,
				    const Cpa32U bank_nr,
				    const char *service_name,
				    const icp_adf_ringInfoService_t info,
				    icp_trans_callback callback,
				    icp_resp_deliv_method resp,
				    const Cpa32U num_msgs,
				    const Cpa32U msg_size,
				    icp_comms_trans_handle *trans_handle);

/*
 * icp_adf_transReinitHandle
 *
 * Description:
 * Reinitialize a transport handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 *
 *   The message size is variable: requests can be 64 or 128 bytes, responses
 *   can be 16, 32 or 64 bytes.
 *   Supported num_msgs:
 *     32, 64, 128, 256, 512, 1024, 2048 number of messages.
 *
 */
CpaStatus icp_adf_transReinitHandle(icp_accel_dev_t *accel_dev,
				    icp_transport_type trans_type,
				    const char *section,
				    const Cpa32U accel_nr,
				    const Cpa32U bank_nr,
				    const char *service_name,
				    const icp_adf_ringInfoService_t info,
				    icp_trans_callback callback,
				    icp_resp_deliv_method resp,
				    const Cpa32U num_msgs,
				    const Cpa32U msg_size,
				    icp_comms_trans_handle *trans_handle);

/*
 * icp_adf_transGetHandle
 *
 * Description:
 * Gets a pointer to a previously created transport handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 *
 */
CpaStatus icp_adf_transGetHandle(icp_accel_dev_t *accel_dev,
				 icp_transport_type trans_type,
				 const char *section,
				 const Cpa32U accel_nr,
				 const Cpa32U bank_nr,
				 const char *service_name,
				 icp_comms_trans_handle *trans_handle);

/*
 * icp_adf_transReleaseHandle
 *
 * Description:
 * Release a transport handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_transReleaseHandle(icp_comms_trans_handle trans_handle);

/*
 * icp_adf_transResetHandle
 *
 * Description:
 * Reset a transport handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_transResetHandle(icp_comms_trans_handle trans_handle);

/*
 * icp_adf_transPutMsg
 *
 * Description:
 * Put Message onto the transport handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_transPutMsg(icp_comms_trans_handle trans_handle,
			      Cpa32U *inBufs,
			      Cpa32U bufLen);

/*
 * icp_adf_getInflightRequests
 *
 * Description:
 * Retrieve in flight requests from the transport handle.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_getInflightRequests(icp_comms_trans_handle trans_handle,
				      Cpa32U *maxInflightRequests,
				      Cpa32U *numInflightRequests);

/*
 * icp_adf_transPutMsgSync
 *
 * Description:
 * Put Message onto the transport handle and waits for a response.
 * Note: Not all transports support method.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_transPutMsgSync(icp_comms_trans_handle trans_handle,
				  Cpa32U *inBuf,
				  Cpa32U *outBuf,
				  Cpa32U bufsLen);

/*
 * icp_adf_transGetRingNum
 *
 * Description:
 *  Function Returns ring number of the given trans_handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_transGetRingNum(icp_comms_trans_handle trans_handle,
				  Cpa32U *ringNum);

/*
 * icp_adf_flush_requests
 *
 * Description:
 *  Function flushes the enqueued requests on the trans_handle
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_flush_requests(icp_comms_trans_handle trans_handle);

#endif /* ICP_ADF_TRANSPORT_H */
