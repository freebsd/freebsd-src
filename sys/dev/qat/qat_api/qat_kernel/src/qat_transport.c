/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"

#include "cpa.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_adf_poll.h"
#include "icp_adf_transport_dp.h"
#include "icp_sal_poll.h"

/*
 * adf_modulo
 * result = data % ( 2 ^ shift )
 */
static inline Cpa32U
adf_modulo(Cpa32U data, Cpa32U shift)
{
	Cpa32U div = data >> shift;
	Cpa32U mult = div << shift;

	return data - mult;
}

/*
 * icp_adf_transCreateHandle
 * crete transport handle for a service
 * call adf_create_ring from adf driver directly with same parameters
 */
CpaStatus
icp_adf_transCreateHandle(icp_accel_dev_t *adf,
			  icp_transport_type trans_type,
			  const char *section,
			  const uint32_t accel_nr,
			  const uint32_t bank_nr,
			  const char *service_name,
			  const icp_adf_ringInfoService_t info,
			  icp_trans_callback callback,
			  icp_resp_deliv_method resp,
			  const uint32_t num_msgs,
			  const uint32_t msg_size,
			  icp_comms_trans_handle *trans_handle)
{
	CpaStatus status;
	int error;

	ICP_CHECK_FOR_NULL_PARAM(trans_handle);
	ICP_CHECK_FOR_NULL_PARAM(adf);

	error = adf_create_ring(adf->accel_dev,
				section,
				bank_nr,
				num_msgs,
				msg_size,
				service_name,
				callback,
				((resp == ICP_RESP_TYPE_IRQ) ? 0 : 1),
				(struct adf_etr_ring_data **)trans_handle);
	if (!error)
		status = CPA_STATUS_SUCCESS;
	else
		status = CPA_STATUS_FAIL;

	return status;
}

/*
 * icp_adf_transReinitHandle
 * Reinitialize transport handle for a service
 */
CpaStatus
icp_adf_transReinitHandle(icp_accel_dev_t *adf,
			  icp_transport_type trans_type,
			  const char *section,
			  const uint32_t accel_nr,
			  const uint32_t bank_nr,
			  const char *service_name,
			  const icp_adf_ringInfoService_t info,
			  icp_trans_callback callback,
			  icp_resp_deliv_method resp,
			  const uint32_t num_msgs,
			  const uint32_t msg_size,
			  icp_comms_trans_handle *trans_handle)
{
	return CPA_STATUS_SUCCESS;
}
/*
 * icp_adf_transReleaseHandle
 * destroy a transport handle, call adf_remove_ring from adf driver directly
 */
CpaStatus
icp_adf_transReleaseHandle(icp_comms_trans_handle trans_handle)
{
	struct adf_etr_ring_data *ring = trans_handle;

	ICP_CHECK_FOR_NULL_PARAM(ring);
	adf_remove_ring(ring);

	return CPA_STATUS_SUCCESS;
}

/*
 * icp_adf_transResetHandle
 * clean a transport handle, call adf_remove_ring from adf driver directly
 */
CpaStatus
icp_adf_transResetHandle(icp_comms_trans_handle trans_handle)
{
	return CPA_STATUS_SUCCESS;
}

/*
 * icp_adf_transGetRingNum
 * get ring number from a transport handle
 */
CpaStatus
icp_adf_transGetRingNum(icp_comms_trans_handle trans_handle, uint32_t *ringNum)
{
	struct adf_etr_ring_data *ring = trans_handle;

	ICP_CHECK_FOR_NULL_PARAM(ring);
	ICP_CHECK_FOR_NULL_PARAM(ringNum);
	*ringNum = (uint32_t)(ring->ring_number);

	return CPA_STATUS_SUCCESS;
}

/*
 * icp_adf_transPutMsg
 * send a request to transport handle
 * call adf_send_message from adf driver directly
 */
CpaStatus
icp_adf_transPutMsg(icp_comms_trans_handle trans_handle,
		    uint32_t *inBuf,
		    uint32_t bufLen)
{
	struct adf_etr_ring_data *ring = trans_handle;
	CpaStatus status = CPA_STATUS_FAIL;
	int error = EFAULT;

	ICP_CHECK_FOR_NULL_PARAM(ring);

	error = adf_send_message(ring, inBuf);
	if (EAGAIN == error)
		status = CPA_STATUS_RETRY;
	else if (0 == error)
		status = CPA_STATUS_SUCCESS;
	else
		status = CPA_STATUS_FAIL;

	return status;
}

CpaStatus
icp_adf_getInflightRequests(icp_comms_trans_handle trans_handle,
			    Cpa32U *maxInflightRequests,
			    Cpa32U *numInflightRequests)
{
	struct adf_etr_ring_data *ring = trans_handle;
	ICP_CHECK_FOR_NULL_PARAM(ring);
	ICP_CHECK_FOR_NULL_PARAM(maxInflightRequests);
	ICP_CHECK_FOR_NULL_PARAM(numInflightRequests);
	/*
	 * XXX: The qat_direct version of this routine returns max - 1, not
	 * the absolute max.
	 */
	*numInflightRequests = (*(uint32_t *)ring->inflights);
	*maxInflightRequests =
	    ADF_MAX_INFLIGHTS(ring->ring_size, ring->msg_size);
	return CPA_STATUS_SUCCESS;
}

CpaStatus
icp_adf_dp_getInflightRequests(icp_comms_trans_handle trans_handle,
			       Cpa32U *maxInflightRequests,
			       Cpa32U *numInflightRequests)
{
	ICP_CHECK_FOR_NULL_PARAM(trans_handle);
	ICP_CHECK_FOR_NULL_PARAM(maxInflightRequests);
	ICP_CHECK_FOR_NULL_PARAM(numInflightRequests);

	return icp_adf_getInflightRequests(trans_handle,
					   maxInflightRequests,
					   numInflightRequests);
}

/*
 * This function allows the user to poll the response ring. The
 * ring number to be polled is supplied by the user via the
 * trans handle for that ring. The trans_hnd is a pointer
 * to an array of trans handles. This ring is
 * only polled if it contains data.
 * This method is used as an alternative to the reading messages
 * via the ISR method.
 * This function will return RETRY if the ring is empty.
 */
CpaStatus
icp_adf_pollInstance(icp_comms_trans_handle *trans_hnd,
		     Cpa32U num_transHandles,
		     Cpa32U response_quota)
{
	Cpa32U resp_total = 0;
	Cpa32U num_resp;
	struct adf_etr_ring_data *ring = NULL;
	struct adf_etr_bank_data *bank = NULL;
	Cpa32U i;

	ICP_CHECK_FOR_NULL_PARAM(trans_hnd);

	for (i = 0; i < num_transHandles; i++) {
		ring = trans_hnd[i];
		if (!ring)
			continue;
		bank = ring->bank;

		/* If the ring in question is empty try the next ring.*/
		if (!bank || !bank->ring_mask) {
			continue;
		}

		num_resp = adf_handle_response(ring, response_quota);
		resp_total += num_resp;
	}

	/* If any of the rings in the instance had data and was polled
	 * return SUCCESS. */
	if (resp_total)
		return CPA_STATUS_SUCCESS;
	else
		return CPA_STATUS_RETRY;
}

/*
 * This function allows the user to check the response ring. The
 * ring number to be polled is supplied by the user via the
 * trans handle for that ring. The trans_hnd is a pointer
 * to an array of trans handles.
 * This function now is a empty function.
 */
CpaStatus
icp_adf_check_RespInstance(icp_comms_trans_handle *trans_hnd,
			   Cpa32U num_transHandles)
{
	return CPA_STATUS_SUCCESS;
}

/*
 * icp_sal_pollBank
 * poll bank with id bank_number inside acceleration device with id @accelId
 */
CpaStatus
icp_sal_pollBank(Cpa32U accelId, Cpa32U bank_number, Cpa32U response_quota)
{
	int ret;

	ret = adf_poll_bank(accelId, bank_number, response_quota);
	if (!ret)
		return CPA_STATUS_SUCCESS;
	else if (EAGAIN == ret)
		return CPA_STATUS_RETRY;

	return CPA_STATUS_FAIL;
}

/*
 * icp_sal_pollAllBanks
 * poll all banks inside acceleration device with id @accelId
 */
CpaStatus
icp_sal_pollAllBanks(Cpa32U accelId, Cpa32U response_quota)
{
	int ret = 0;

	ret = adf_poll_all_banks(accelId, response_quota);
	if (!ret)
		return CPA_STATUS_SUCCESS;
	else if (ret == EAGAIN)
		return CPA_STATUS_RETRY;

	return CPA_STATUS_FAIL;
}

/*
 * icp_adf_getQueueMemory
 * Data plane support function - returns the pointer to next message on the ring
 * or NULL if there is not enough space.
 */
void
icp_adf_getQueueMemory(icp_comms_trans_handle trans_handle,
		       Cpa32U numberRequests,
		       void **pCurrentQatMsg)
{
	struct adf_etr_ring_data *ring = trans_handle;
	Cpa64U flight;

	ICP_CHECK_FOR_NULL_PARAM_VOID(ring);

	/* Check if there is enough space in the ring */
	flight = atomic_add_return(numberRequests, ring->inflights);
	if (flight > ADF_MAX_INFLIGHTS(ring->ring_size, ring->msg_size)) {
		atomic_sub(numberRequests, ring->inflights);
		*pCurrentQatMsg = NULL;
		return;
	}

	/* We have enough space - get the address of next message */
	*pCurrentQatMsg = (void *)((uintptr_t)ring->base_addr + ring->tail);
}

/*
 * icp_adf_getSingleQueueAddr
 * Data plane support function - returns the pointer to next message on the ring
 * or NULL if there is not enough space - it also updates the shadow tail copy.
 */
void
icp_adf_getSingleQueueAddr(icp_comms_trans_handle trans_handle,
			   void **pCurrentQatMsg)
{
	struct adf_etr_ring_data *ring = trans_handle;
	Cpa64U flight;

	ICP_CHECK_FOR_NULL_PARAM_VOID(ring);
	ICP_CHECK_FOR_NULL_PARAM_VOID(pCurrentQatMsg);

	/* Check if there is enough space in the ring */
	flight = atomic_add_return(1, ring->inflights);
	if (flight > ADF_MAX_INFLIGHTS(ring->ring_size, ring->msg_size)) {
		atomic_dec(ring->inflights);
		*pCurrentQatMsg = NULL;
		return;
	}

	/* We have enough space - get the address of next message */
	*pCurrentQatMsg = (void *)((uintptr_t)ring->base_addr + ring->tail);

	/* Update the shadow tail */
	ring->tail =
	    adf_modulo(ring->tail + ADF_MSG_SIZE_TO_BYTES(ring->msg_size),
		       ADF_RING_SIZE_MODULO(ring->ring_size));
}

/*
 * icp_adf_getQueueNext
 * Data plane support function - increments the tail pointer and returns
 * the pointer to next message on the ring.
 */
void
icp_adf_getQueueNext(icp_comms_trans_handle trans_handle, void **pCurrentQatMsg)
{
	struct adf_etr_ring_data *ring = trans_handle;

	ICP_CHECK_FOR_NULL_PARAM_VOID(ring);
	ICP_CHECK_FOR_NULL_PARAM_VOID(pCurrentQatMsg);

	/* Increment tail to next message */
	ring->tail =
	    adf_modulo(ring->tail + ADF_MSG_SIZE_TO_BYTES(ring->msg_size),
		       ADF_RING_SIZE_MODULO(ring->ring_size));

	/* Get the address of next message */
	*pCurrentQatMsg = (void *)((uintptr_t)ring->base_addr + ring->tail);
}

/*
 * icp_adf_updateQueueTail
 * Data plane support function - Writes the tail shadow copy to the device.
 */
void
icp_adf_updateQueueTail(icp_comms_trans_handle trans_handle)
{
	struct adf_etr_ring_data *ring = trans_handle;
	struct adf_hw_csr_ops *csr_ops;

	ICP_CHECK_FOR_NULL_PARAM_VOID(ring);
	ICP_CHECK_FOR_NULL_PARAM_VOID(ring->bank);
	ICP_CHECK_FOR_NULL_PARAM_VOID(ring->bank->accel_dev);

	csr_ops = GET_CSR_OPS(ring->bank->accel_dev);

	ICP_CHECK_FOR_NULL_PARAM_VOID(csr_ops);

	csr_ops->write_csr_ring_tail(ring->bank->csr_addr,
				     ring->bank->bank_number,
				     ring->ring_number,
				     ring->tail);
	ring->csr_tail_offset = ring->tail;
}

/*
 * icp_adf_pollQueue
 * Data plane support function - Poll messages from the queue.
 */
CpaStatus
icp_adf_pollQueue(icp_comms_trans_handle trans_handle, Cpa32U response_quota)
{
	Cpa32U num_resp;
	struct adf_etr_ring_data *ring = trans_handle;

	ICP_CHECK_FOR_NULL_PARAM(ring);

	num_resp = adf_handle_response(ring, response_quota);

	if (num_resp)
		return CPA_STATUS_SUCCESS;
	else
		return CPA_STATUS_RETRY;
}

/*
 * icp_adf_queueDataToSend
 * Data-plane support function - Indicates if there is data on the ring to be
 * sent. This should only be called on request rings. If the function returns
 * true then it is ok to call icp_adf_updateQueueTail() function on this ring.
 */
CpaBoolean
icp_adf_queueDataToSend(icp_comms_trans_handle trans_handle)
{
	struct adf_etr_ring_data *ring = trans_handle;

	if (ring->tail != ring->csr_tail_offset)
		return CPA_TRUE;
	else
		return CPA_FALSE;
}

/*
 * This icp API won't be supported in kernel space currently
 */
CpaStatus
icp_adf_transGetFdForHandle(icp_comms_trans_handle trans_hnd, int *fd)
{
	return CPA_STATUS_UNSUPPORTED;
}
