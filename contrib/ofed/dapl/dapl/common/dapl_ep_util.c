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
 * MODULE: dapl_ep_util.c
 *
 * PURPOSE: Manage EP Info structure
 *
 * $Id:$
 **********************************************************************/

#include "dapl_ep_util.h"
#include "dapl_ring_buffer_util.h"
#include "dapl_cookie.h"
#include "dapl_adapter_util.h"
#include "dapl_evd_util.h"
#include "dapl_cr_util.h"	/* for callback routine */

/*
 * Local definitions
 */
/*
 * Default number of I/O operations on an end point
 */
#define IB_IO_DEFAULT	16
/*
 * Default number of scatter/gather entries available to a single
 * post send/recv
 */
#define IB_IOV_DEFAULT	4

/*
 * Default number of RDMA operations in progress at a time
 */
#define IB_RDMA_DEFAULT	4

extern void dapli_ep_default_attrs(IN DAPL_EP * ep_ptr);

char *dapl_get_ep_state_str(DAT_EP_STATE state)
{
#ifdef DAPL_DBG
	static char *state_str[DAT_EP_STATE_CONNECTED_MULTI_PATH + 1] = {
		"DAT_EP_STATE_UNCONNECTED",	/* quiescent state */
		"DAT_EP_STATE_UNCONFIGURED_UNCONNECTED",
		"DAT_EP_STATE_RESERVED",
		"DAT_EP_STATE_UNCONFIGURED_RESERVED",
		"DAT_EP_STATE_PASSIVE_CONNECTION_PENDING",
		"DAT_EP_STATE_UNCONFIGURED_PASSIVE",
		"DAT_EP_STATE_ACTIVE_CONNECTION_PENDING",
		"DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING",
		"DAT_EP_STATE_UNCONFIGURED_TENTATIVE",
		"DAT_EP_STATE_CONNECTED",
		"DAT_EP_STATE_DISCONNECT_PENDING",
		"DAT_EP_STATE_DISCONNECTED",
		"DAT_EP_STATE_COMPLETION_PENDING",
		"DAT_EP_STATE_CONNECTED_SINGLE_PATH",
		"DAT_EP_STATE_CONNECTED_MULTI_PATH"
	};
	return state_str[state];
#else
	static char buf[12];
	sprintf(buf, "%d", state);
	return buf;
#endif
}

/*
 * dapl_ep_alloc
 *
 * alloc and initialize an EP INFO struct
 *
 * Input:
 * 	IA INFO struct ptr
 *
 * Output:
 * 	ep_ptr
 *
 * Returns:
 * 	none
 *
 */
DAPL_EP *dapl_ep_alloc(IN DAPL_IA * ia_ptr, IN const DAT_EP_ATTR * ep_attr)
{
	DAPL_EP *ep_ptr;

	/* Allocate EP */
	ep_ptr =
	    (DAPL_EP *) dapl_os_alloc(sizeof(DAPL_EP) + sizeof(DAT_SOCK_ADDR));
	if (ep_ptr == NULL) {
		goto bail;
	}

	/* zero the structure */
	dapl_os_memzero(ep_ptr, sizeof(DAPL_EP) + sizeof(DAT_SOCK_ADDR));

#ifdef DAPL_COUNTERS
	/* Allocate counters */
	ep_ptr->cntrs =
	    dapl_os_alloc(sizeof(DAT_UINT64) * DCNT_EP_ALL_COUNTERS);
	if (ep_ptr->cntrs == NULL) {
		dapl_os_free(ep_ptr, sizeof(DAPL_EP) + sizeof(DAT_SOCK_ADDR));
		return (NULL);
	}
	dapl_os_memzero(ep_ptr->cntrs,
			sizeof(DAT_UINT64) * DCNT_EP_ALL_COUNTERS);
#endif				/* DAPL_COUNTERS */

	/*
	 * initialize the header
	 */
	ep_ptr->header.provider = ia_ptr->header.provider;
	ep_ptr->header.magic = DAPL_MAGIC_EP;
	ep_ptr->header.handle_type = DAT_HANDLE_TYPE_EP;
	ep_ptr->header.owner_ia = ia_ptr;
	ep_ptr->header.user_context.as_64 = 0;
	ep_ptr->header.user_context.as_ptr = NULL;

	dapl_llist_init_entry(&ep_ptr->header.ia_list_entry);
	dapl_os_lock_init(&ep_ptr->header.lock);

	/*
	 * Initialize the body
	 */
	/*
	 * Set up default parameters if the user passed in a NULL
	 */
	if (ep_attr == NULL) {
		dapli_ep_default_attrs(ep_ptr);
	} else {
		ep_ptr->param.ep_attr = *ep_attr;
	}

	/*
	 * IBM OS API specific fields
	 */
	ep_ptr->qp_handle = IB_INVALID_HANDLE;
	ep_ptr->qpn = 0;
	ep_ptr->qp_state = DAPL_QP_STATE_UNATTACHED;
	ep_ptr->cm_handle = IB_INVALID_HANDLE;

	if (DAT_SUCCESS != dapls_cb_create(&ep_ptr->req_buffer,
					   ep_ptr,
					   ep_ptr->param.ep_attr.
					   max_request_dtos)) {
		dapl_ep_dealloc(ep_ptr);
		ep_ptr = NULL;
		goto bail;
	}

	if (DAT_SUCCESS != dapls_cb_create(&ep_ptr->recv_buffer,
					   ep_ptr,
					   ep_ptr->param.ep_attr.max_recv_dtos))
	{
		dapl_ep_dealloc(ep_ptr);
		ep_ptr = NULL;
		goto bail;
	}

	dapls_io_trc_alloc(ep_ptr);

      bail:
	return ep_ptr;
}

/*
 * dapl_ep_dealloc
 *
 * Free the passed in EP structure.
 *
 * Input:
 * 	entry point pointer
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ep_dealloc(IN DAPL_EP * ep_ptr)
{
	dapl_os_assert(ep_ptr->header.magic == DAPL_MAGIC_EP);

	ep_ptr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */

	dapls_cb_free(&ep_ptr->req_buffer);
	dapls_cb_free(&ep_ptr->recv_buffer);

	if (NULL != ep_ptr->cxn_timer) {
		dapl_os_free(ep_ptr->cxn_timer, sizeof(DAPL_OS_TIMER));
	}
#if defined(_WIN32) || defined(_WIN64)
	if (ep_ptr->ibal_cm_handle) {
		dapl_os_free(ep_ptr->ibal_cm_handle,
			     sizeof(*ep_ptr->ibal_cm_handle));
		ep_ptr->ibal_cm_handle = NULL;
	}
#endif

#ifdef DAPL_COUNTERS
	dapl_os_free(ep_ptr->cntrs, sizeof(DAT_UINT64) * DCNT_EP_ALL_COUNTERS);
#endif				/* DAPL_COUNTERS */

	dapl_os_free(ep_ptr, sizeof(DAPL_EP) + sizeof(DAT_SOCK_ADDR));
}

/*
 * dapl_ep_default_attrs
 *
 * Set default values in the parameter fields
 *
 * Input:
 * 	entry point pointer
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapli_ep_default_attrs(IN DAPL_EP * ep_ptr)
{
	DAT_EP_ATTR ep_attr_limit;
	DAT_EP_ATTR *ep_attr;
	DAT_RETURN dat_status;

	ep_attr = &ep_ptr->param.ep_attr;
	/* Set up defaults */
	dapl_os_memzero(ep_attr, sizeof(DAT_EP_ATTR));

	/* mtu and rdma sizes fixed in IB as per IBTA 1.1, 9.4.3, 9.4.4, 9.7.7.  */
	ep_attr->max_mtu_size = 0x80000000;
	ep_attr->max_rdma_size = 0x80000000;

	ep_attr->qos = DAT_QOS_BEST_EFFORT;
	ep_attr->service_type = DAT_SERVICE_TYPE_RC;
	ep_attr->max_recv_dtos = IB_IO_DEFAULT;
	ep_attr->max_request_dtos = IB_IO_DEFAULT;
	ep_attr->max_recv_iov = IB_IOV_DEFAULT;
	ep_attr->max_request_iov = IB_IOV_DEFAULT;
	ep_attr->max_rdma_read_in = IB_RDMA_DEFAULT;
	ep_attr->max_rdma_read_out = IB_RDMA_DEFAULT;

	/*
	 * Configure the EP as a standard completion type, which will be
	 * used by the EVDs. A threshold of 1 is the default state of an
	 * EVD.
	 */
	ep_attr->request_completion_flags = DAT_COMPLETION_EVD_THRESHOLD_FLAG;
	ep_attr->recv_completion_flags = DAT_COMPLETION_EVD_THRESHOLD_FLAG;
	/*
	 * Unspecified defaults:
	 *    - ep_privileges: No RDMA capabilities
	 *    - num_transport_specific_params: none
	 *    - transport_specific_params: none
	 *    - num_provider_specific_params: 0
	 *    - provider_specific_params: 0
	 */

	dat_status = dapls_ib_query_hca(ep_ptr->header.owner_ia->hca_ptr,
					NULL, &ep_attr_limit, NULL);
	/* check against HCA maximums */
	if (dat_status == DAT_SUCCESS) {
		ep_ptr->param.ep_attr.max_mtu_size =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_mtu_size,
			     ep_attr_limit.max_mtu_size);
		ep_ptr->param.ep_attr.max_rdma_size =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_rdma_size,
			     ep_attr_limit.max_rdma_size);
		ep_ptr->param.ep_attr.max_recv_dtos =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_recv_dtos,
			     ep_attr_limit.max_recv_dtos);
		ep_ptr->param.ep_attr.max_request_dtos =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_request_dtos,
			     ep_attr_limit.max_request_dtos);
		ep_ptr->param.ep_attr.max_recv_iov =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_recv_iov,
			     ep_attr_limit.max_recv_iov);
		ep_ptr->param.ep_attr.max_request_iov =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_request_iov,
			     ep_attr_limit.max_request_iov);
		ep_ptr->param.ep_attr.max_rdma_read_in =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_rdma_read_in,
			     ep_attr_limit.max_rdma_read_in);
		ep_ptr->param.ep_attr.max_rdma_read_out =
		    DAPL_MIN(ep_ptr->param.ep_attr.max_rdma_read_out,
			     ep_attr_limit.max_rdma_read_out);
	}
}

DAT_RETURN dapl_ep_check_recv_completion_flags(DAT_COMPLETION_FLAGS flags)
{

	/*
	 * InfiniBand will not allow signal suppression for RECV completions,
	 * see the 1.0.1 spec section 10.7.3.1, 10.8.6.
	 * N.B. SIGNALLED has a different meaning in dapl than it does
	 *      in IB; IB SIGNALLED is the same as DAPL SUPPRESS. DAPL
	 *      SIGNALLED simply means the user will not get awakened when
	 *      an EVD completes, even though the dapl handler is invoked.
	 */

	if (flags & DAT_COMPLETION_SUPPRESS_FLAG) {
		return DAT_INVALID_PARAMETER;
	}

	return DAT_SUCCESS;
}

DAT_RETURN dapl_ep_check_request_completion_flags(DAT_COMPLETION_FLAGS flags)
{
	return DAT_SUCCESS;
}

DAT_RETURN
dapl_ep_post_send_req(IN DAT_EP_HANDLE ep_handle,
		      IN DAT_COUNT num_segments,
		      IN DAT_LMR_TRIPLET * local_iov,
		      IN DAT_DTO_COOKIE user_cookie,
		      IN const DAT_RMR_TRIPLET * remote_iov,
		      IN DAT_COMPLETION_FLAGS completion_flags,
		      IN DAPL_DTO_TYPE dto_type, IN int op_type)
{
	DAPL_EP *ep_ptr;
	DAPL_COOKIE *cookie;
	DAT_RETURN dat_status;

	if (DAPL_BAD_HANDLE(ep_handle, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Synchronization ok since this buffer is only used for send
	 * requests, which aren't allowed to race with each other.
	 */
	dat_status = dapls_dto_cookie_alloc(&ep_ptr->req_buffer,
					    dto_type, user_cookie, &cookie);
	if (dat_status != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " dapl_post_req resource ERR:"
			 " dtos pending = %d, max_dtos %d, max_cb %d hd %d tl %d\n",
			 dapls_cb_pending(&ep_ptr->req_buffer),
			 ep_ptr->param.ep_attr.max_request_dtos,
			 ep_ptr->req_buffer.pool_size,
			 ep_ptr->req_buffer.head, ep_ptr->req_buffer.tail);

		goto bail;
	}

	/*
	 * Invoke provider specific routine to post DTO
	 */
	dat_status = dapls_ib_post_send(ep_ptr,
					op_type,
					cookie,
					num_segments,
					local_iov,
					remote_iov, completion_flags);

	if (dat_status != DAT_SUCCESS) {
		dapls_cookie_dealloc(&ep_ptr->req_buffer, cookie);
	}

      bail:
	return dat_status;
}

/*
 * dapli_ep_timeout
 *
 * If this routine is invoked before a connection occurs, generate an
 * event
 */
void dapls_ep_timeout(uintptr_t arg)
{
	DAPL_EP *ep_ptr;
	ib_cm_events_t ib_cm_event;

	dapl_dbg_log(DAPL_DBG_TYPE_CM, "--> dapls_ep_timeout! ep %lx\n", arg);

	ep_ptr = (DAPL_EP *) arg;

	/* reset the EP state */
	ep_ptr->param.ep_state = DAT_EP_STATE_DISCONNECTED;

	/* Clean up the EP and put the underlying QP into the ERROR state.
	 * The disconnect_clean interface requires the provided dependent 
	 *cm event number.
	 */
	ib_cm_event = dapls_ib_get_cm_event(DAT_CONNECTION_EVENT_DISCONNECTED);
	dapls_ib_disconnect_clean(ep_ptr, DAT_TRUE, ib_cm_event);

	(void)dapls_evd_post_connection_event((DAPL_EVD *) ep_ptr->param.
					      connect_evd_handle,
					      DAT_CONNECTION_EVENT_TIMED_OUT,
					      (DAT_HANDLE) ep_ptr, 0, 0);
}

/*
 * dapls_ep_state_subtype
 *
 * Return the INVALID_STATE connection subtype associated with an
 * INVALID_STATE on an EP. Strictly for error reporting.
 */
DAT_RETURN_SUBTYPE dapls_ep_state_subtype(IN DAPL_EP * ep_ptr)
{
	DAT_RETURN_SUBTYPE dat_status;

	switch (ep_ptr->param.ep_state) {
	case DAT_EP_STATE_UNCONNECTED:
		{
			dat_status = DAT_INVALID_STATE_EP_UNCONNECTED;
			break;
		}
	case DAT_EP_STATE_RESERVED:
		{
			dat_status = DAT_INVALID_STATE_EP_RESERVED;
			break;
		}
	case DAT_EP_STATE_PASSIVE_CONNECTION_PENDING:
		{
			dat_status = DAT_INVALID_STATE_EP_PASSCONNPENDING;
			break;
		}
	case DAT_EP_STATE_ACTIVE_CONNECTION_PENDING:
		{
			dat_status = DAT_INVALID_STATE_EP_ACTCONNPENDING;
			break;
		}
	case DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING:
		{
			dat_status = DAT_INVALID_STATE_EP_TENTCONNPENDING;
			break;
		}
	case DAT_EP_STATE_CONNECTED:
		{
			dat_status = DAT_INVALID_STATE_EP_CONNECTED;
			break;
		}
	case DAT_EP_STATE_DISCONNECT_PENDING:
		{
			dat_status = DAT_INVALID_STATE_EP_DISCPENDING;
			break;
		}
	case DAT_EP_STATE_DISCONNECTED:
		{
			dat_status = DAT_INVALID_STATE_EP_DISCONNECTED;
			break;
		}
	case DAT_EP_STATE_COMPLETION_PENDING:
		{
			dat_status = DAT_INVALID_STATE_EP_COMPLPENDING;
			break;
		}

	default:
		{
			dat_status = 0;
			break;
		}
	}

	return dat_status;
}

#ifdef DAPL_DBG_IO_TRC
/* allocate trace buffer */
void dapls_io_trc_alloc(DAPL_EP * ep_ptr)
{
	DAT_RETURN dat_status;
	int i;
	struct io_buf_track *ibt;

	ep_ptr->ibt_dumped = 0;	/* bool to control how often we print */
	dat_status = dapls_rbuf_alloc(&ep_ptr->ibt_queue, DBG_IO_TRC_QLEN);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}
	ibt =
	    (struct io_buf_track *)dapl_os_alloc(sizeof(struct io_buf_track) *
						 DBG_IO_TRC_QLEN);

	if (dat_status != DAT_SUCCESS) {
		dapls_rbuf_destroy(&ep_ptr->ibt_queue);
		goto bail;
	}
	ep_ptr->ibt_base = ibt;
	dapl_os_memzero(ibt, sizeof(struct io_buf_track) * DBG_IO_TRC_QLEN);

	/* add events to free event queue */
	for (i = 0; i < DBG_IO_TRC_QLEN; i++) {
		dapls_rbuf_add(&ep_ptr->ibt_queue, ibt++);
	}
      bail:
	return;
}
#endif				/* DAPL_DBG_IO_TRC */

/*
 * Generate a disconnect event on abruct close for older verbs providers 
 * that do not do it automatically.
 */

void
dapl_ep_legacy_post_disconnect(DAPL_EP * ep_ptr,
			       DAT_CLOSE_FLAGS disconnect_flags)
{
	ib_cm_events_t ib_cm_event;
	DAPL_CR *cr_ptr;

	/*
	 * Acquire the lock and make sure we didn't get a callback
	 * that cleaned up.
	 */
	dapl_os_lock(&ep_ptr->header.lock);
	if (disconnect_flags == DAT_CLOSE_ABRUPT_FLAG &&
	    ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECT_PENDING) {
		/*
		 * If this is an ABRUPT close, the provider will not generate
		 * a disconnect message so we do it manually here. Just invoke
		 * the CM callback as it will clean up the appropriate
		 * data structures, reset the state, and generate the event
		 * on the way out. Obtain the provider dependent cm_event to 
		 * pass into the callback for a disconnect.
		 */
		ib_cm_event =
		    dapls_ib_get_cm_event(DAT_CONNECTION_EVENT_DISCONNECTED);

		cr_ptr = ep_ptr->cr_ptr;
		dapl_os_unlock(&ep_ptr->header.lock);

		if (cr_ptr != NULL) {
			dapl_dbg_log(DAPL_DBG_TYPE_API | DAPL_DBG_TYPE_CM,
				     "    dapl_ep_disconnect force callback on EP %p CM handle %x\n",
				     ep_ptr, cr_ptr->ib_cm_handle);

			dapls_cr_callback(cr_ptr->ib_cm_handle,
					  ib_cm_event, NULL, cr_ptr->sp_ptr);
		} else {
			dapl_evd_connection_callback(ep_ptr->cm_handle,
						     ib_cm_event,
						     NULL, (void *)ep_ptr);
		}
	} else {
		dapl_os_unlock(&ep_ptr->header.lock);
	}
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
