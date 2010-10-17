/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 *
 * MODULE: dapl_ep_connect.c
 *
 * PURPOSE: Endpoint management
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"
#include "dapl_evd_util.h"
#include "dapl_timer_util.h"

/*
 * dapl_ep_connect
 *
 * Request a connection be established between the local Endpoint
 * and a remote Endpoint. This operation is used by the active/client
 * side of a connection
 *
 * Input:
 *	ep_handle
 *	remote_ia_address
 *	remote_conn_qual
 *	timeout
 *	private_data_size
 *	privaet_data
 *	qos
 *	connect_flags
 *
 * Output:
 *	None
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOUCRES
 *	DAT_INVALID_PARAMETER
 *	DAT_MODLE_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_ep_connect(IN DAT_EP_HANDLE ep_handle,
		IN DAT_IA_ADDRESS_PTR remote_ia_address,
		IN DAT_CONN_QUAL remote_conn_qual,
		IN DAT_TIMEOUT timeout,
		IN DAT_COUNT private_data_size,
		IN const DAT_PVOID private_data,
		IN DAT_QOS qos, IN DAT_CONNECT_FLAGS connect_flags)
{
	DAPL_EP *ep_ptr;
	DAPL_EP alloc_ep;
	DAT_RETURN dat_status;
	DAT_COUNT req_hdr_size;
	DAT_UINT32 max_req_pdata_size;
	void *private_data_ptr;

	dapl_dbg_log(DAPL_DBG_TYPE_API | DAPL_DBG_TYPE_CM,
		     "dapl_ep_connect (%p, {%u.%u.%u.%u}, %X, %d, %d, %p, %x, %x)\n",
		     ep_handle,
		     remote_ia_address->sa_data[2],
		     remote_ia_address->sa_data[3],
		     remote_ia_address->sa_data[4],
		     remote_ia_address->sa_data[5],
		     remote_conn_qual,
		     timeout,
		     private_data_size, private_data, qos, connect_flags);

	dat_status = DAT_SUCCESS;
	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Verify parameter & state. The connection handle must be good
	 * at this point.
	 */
	if (DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	if (DAPL_BAD_HANDLE(ep_ptr->param.connect_evd_handle, DAPL_MAGIC_EVD)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EVD_CONN);
		goto bail;
	}

	/* Can't do a connection in 0 time, reject outright */
	if (timeout == 0) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}
	DAPL_CNTR(ep_ptr, DCNT_EP_CONNECT);

	/*
	 * If the endpoint needs a QP, associated the QP with it.
	 * This needs to be done carefully, in order to:
	 *  * Avoid allocating under a lock.
	 *  * Not step on data structures being altered by
	 *    routines with which we are racing.
	 * So we:
	 *  * Confirm that a new QP is needed and is not forbidden by the
	 *    current state.
	 *  * Allocate it into a separate EP.
	 *  * Take the EP lock.
	 *  * Reconfirm that the EP is in a state where it needs a QP.
	 *  * Assign the QP and release the lock.
	 */
	if (ep_ptr->qp_state == DAPL_QP_STATE_UNATTACHED) {
		if (ep_ptr->param.pz_handle == NULL
		    || DAPL_BAD_HANDLE(ep_ptr->param.pz_handle, DAPL_MAGIC_PZ))
		{
			dat_status =
			    DAT_ERROR(DAT_INVALID_STATE,
				      DAT_INVALID_STATE_EP_NOTREADY);
			goto bail;
		}
		alloc_ep = *ep_ptr;

		dat_status = dapls_ib_qp_alloc(ep_ptr->header.owner_ia,
					       &alloc_ep, ep_ptr);
		if (dat_status != DAT_SUCCESS) {
			dat_status =
			    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				      DAT_RESOURCE_MEMORY);
			goto bail;
		}

		dapl_os_lock(&ep_ptr->header.lock);
		/*
		 * PZ shouldn't have changed since we're only racing with
		 * dapl_cr_accept()
		 */
		if (ep_ptr->qp_state != DAPL_QP_STATE_UNATTACHED) {
			/* Bail, cleaning up.  */
			dapl_os_unlock(&ep_ptr->header.lock);
			dat_status = dapls_ib_qp_free(ep_ptr->header.owner_ia,
						      &alloc_ep);
			if (dat_status != DAT_SUCCESS) {
				dapl_dbg_log(DAPL_DBG_TYPE_WARN,
					     "ep_connect: ib_qp_free failed with %x\n",
					     dat_status);
			}
			dat_status =
			    DAT_ERROR(DAT_INVALID_STATE,
				      dapls_ep_state_subtype(ep_ptr));
			goto bail;
		}

		ep_ptr->qp_handle = alloc_ep.qp_handle;
		ep_ptr->qpn = alloc_ep.qpn;
		ep_ptr->qp_state = alloc_ep.qp_state;

		dapl_os_unlock(&ep_ptr->header.lock);
	}

	/*
	 * We do state checks and transitions under lock.
	 * The only code we're racing against is dapl_cr_accept.
	 */
	dapl_os_lock(&ep_ptr->header.lock);

	/*
	 * Verify the attributes of the EP handle before we connect it. Test
	 * all of the handles to make sure they are currently valid.
	 * Specifically:
	 *   pz_handle              required
	 *   recv_evd_handle        optional, but must be valid
	 *   request_evd_handle     optional, but must be valid
	 *   connect_evd_handle     required
	 */
	if (ep_ptr->param.pz_handle == NULL
	    || DAPL_BAD_HANDLE(ep_ptr->param.pz_handle, DAPL_MAGIC_PZ)
	    /* test connect handle */
	    || ep_ptr->param.connect_evd_handle == NULL
	    || DAPL_BAD_HANDLE(ep_ptr->param.connect_evd_handle, DAPL_MAGIC_EVD)
	    || !(((DAPL_EVD *) ep_ptr->param.connect_evd_handle)->
		 evd_flags & DAT_EVD_CONNECTION_FLAG)
	    /* test optional completion handles */
	    || (ep_ptr->param.recv_evd_handle != DAT_HANDLE_NULL &&
		(DAPL_BAD_HANDLE
		 (ep_ptr->param.recv_evd_handle, DAPL_MAGIC_EVD)))
	    || (ep_ptr->param.request_evd_handle != DAT_HANDLE_NULL
		&&
		(DAPL_BAD_HANDLE
		 (ep_ptr->param.request_evd_handle, DAPL_MAGIC_EVD)))) {
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_EP_NOTREADY);
		goto bail;
	}

	/* Check both the EP state and the QP state: if we don't have a QP
	 *  we need to attach one now.
	 */
	if (ep_ptr->qp_state == DAPL_QP_STATE_UNATTACHED) {
		dat_status = dapls_ib_qp_alloc(ep_ptr->header.owner_ia,
					       ep_ptr, ep_ptr);

		if (dat_status != DAT_SUCCESS) {
			dapl_os_unlock(&ep_ptr->header.lock);
			dat_status =
			    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				      DAT_RESOURCE_TEP);
			goto bail;
		}
	}

	if (ep_ptr->param.ep_state != DAT_EP_STATE_UNCONNECTED &&
	    ep_ptr->param.ep_attr.service_type == DAT_SERVICE_TYPE_RC) {
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      dapls_ep_state_subtype(ep_ptr));
		goto bail;
	}

	if (qos != DAT_QOS_BEST_EFFORT ||
	    connect_flags != DAT_CONNECT_DEFAULT_FLAG) {
		/*
		 * At this point we only support one QOS level
		 */
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status = DAT_ERROR(DAT_MODEL_NOT_SUPPORTED, 0);
		goto bail;
	}

	/*
	 * Verify the private data size doesn't exceed the max
	 * req_hdr_size will evaluate to 0 unless IBHOSTS_NAMING is enabled.
	 */
	req_hdr_size = (sizeof(DAPL_PRIVATE) - DAPL_MAX_PRIVATE_DATA_SIZE);

	max_req_pdata_size =
	    dapls_ib_private_data_size(NULL, DAPL_PDATA_CONN_REQ,
				       ep_ptr->header.owner_ia->hca_ptr);

	if (private_data_size + req_hdr_size > (DAT_COUNT) max_req_pdata_size) {
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG5);
		goto bail;
	}

	/* transition the state before requesting a connection to avoid
	 * race conditions
	 */
	ep_ptr->param.ep_state = DAT_EP_STATE_ACTIVE_CONNECTION_PENDING;

	/*
	 * At this point we're committed, and done with the endpoint
	 * except for the connect, so we can drop the lock.
	 */
	dapl_os_unlock(&ep_ptr->header.lock);

#ifdef IBHOSTS_NAMING
	/*
	 * Special case: put the remote HCA address into the private data
	 * prefix. This is a spec violation as it introduces a protocol, but
	 * some implementations may find it necessary for a time.
	 * Copy the private data into the EP area so the data is contiguous.
	 * If the provider needs to pad the buffer with NULLs, it happens at
	 * the provider layer.
	 */
	dapl_os_memcpy(&ep_ptr->hca_address,
		       &ep_ptr->header.owner_ia->hca_ptr->hca_address,
		       sizeof(DAT_SOCK_ADDR));
	dapl_os_memcpy(ep_ptr->private.private_data, private_data,
		       private_data_size);
	private_data_ptr = (void *)&ep_ptr->private.private_data;
#else
	private_data_ptr = private_data;
#endif				/* IBHOSTS_NAMING */

	/* Copy the connection qualifiers */
	dapl_os_memcpy(ep_ptr->param.remote_ia_address_ptr,
		       remote_ia_address, sizeof(DAT_SOCK_ADDR));
	ep_ptr->param.remote_port_qual = remote_conn_qual;

	dat_status = dapls_ib_connect(ep_handle,
				      remote_ia_address,
				      remote_conn_qual,
				      private_data_size + req_hdr_size,
				      private_data_ptr);

	if (dat_status != DAT_SUCCESS) {
		ep_ptr->param.ep_state = DAT_EP_STATE_UNCONNECTED;

		/*
		 * Some implementations provide us with an error code that the
		 * remote destination is unreachable, but DAT doesn't have a
		 * synchronous error code to communicate this. So the provider
		 * layer generates an INTERNAL_ERROR with a subtype; when
		 * this happens, return SUCCESS and generate the event
		 */
		if (dat_status == DAT_ERROR(DAT_INTERNAL_ERROR, 1)) {
			dapls_evd_post_connection_event((DAPL_EVD *) ep_ptr->
							param.
							connect_evd_handle,
							DAT_CONNECTION_EVENT_UNREACHABLE,
							(DAT_HANDLE) ep_ptr, 0,
							0);
			dat_status = DAT_SUCCESS;
		}
	} else {
		/*
		 * Acquire the lock and recheck the state of the EP; this
		 * thread could have been descheduled after issuing the connect
		 * request and the EP is now connected. Set up a timer if
		 * necessary.
		 */
		dapl_os_lock(&ep_ptr->header.lock);
		if (ep_ptr->param.ep_state ==
		    DAT_EP_STATE_ACTIVE_CONNECTION_PENDING
		    && timeout != DAT_TIMEOUT_INFINITE) {
			ep_ptr->cxn_timer =
			    (DAPL_OS_TIMER *)
			    dapl_os_alloc(sizeof(DAPL_OS_TIMER));

			dapls_timer_set(ep_ptr->cxn_timer,
					dapls_ep_timeout, ep_ptr, timeout);
		}
		dapl_os_unlock(&ep_ptr->header.lock);
	}

      bail:
	dapl_dbg_log(DAPL_DBG_TYPE_RTN | DAPL_DBG_TYPE_CM,
		     "dapl_ep_connect () returns 0x%x\n", dat_status);

	return dat_status;
}

/*
 * dapl_ep_common_connect
 *
 * DAPL Requirements Version 2.0, 6.6.x
 *
 * Requests that a connection be established
 * between the local Endpoint and a remote Endpoint specified by the
 * remote_ia_address. This operation is used by the active/client side
 * Consumer of the Connection establishment model.
 *
 * EP must be properly configured for this operation. The EP Communicator
 * must be specified. As part of the successful completion of this operation,
 * the local Endpoint is bound to a local IA Address if it had these assigned
 * before.
 * 
 * The local IP Address, port and protocol are passed to the remote side of
 * the requested connection and is available to the remote Consumer in the
 * Connection Request of the DAT_CONNECTION_REQUEST_EVENT.
 * 
 * The Consumer-provided private_data is passed to the remote side and is
 * provided to the remote Consumer in the Connection Request. Consumers
 * can encapsulate any local Endpoint attributes that remote Consumers
 * need to know as part of an upper-level protocol.
 *
 * Input:
 *	ep_handle
 *	remote_ia_address
 *	timeout
 *	private_data_size
 *	private_date pointer
 *
 * Output:
 *	none
 * 
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_STATE
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API dapl_ep_common_connect(IN DAT_EP_HANDLE ep,	/* ep_handle            */
					  IN DAT_IA_ADDRESS_PTR remote_addr,	/* remote_ia_address    */
					  IN DAT_TIMEOUT timeout,	/* timeout              */
					  IN DAT_COUNT pdata_size,	/* private_data_size    */
					  IN const DAT_PVOID pdata)
{				/* private_data         */
	return DAT_MODEL_NOT_SUPPORTED;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
