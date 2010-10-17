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
 * MODULE: dapls_cr_callback.c
 *
 * PURPOSE: implements passive side connection callbacks
 *
 * Description: Accepts asynchronous callbacks from the Communications Manager
 *              for EVDs that have been specified as the connection_evd.
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_evd_util.h"
#include "dapl_cr_util.h"
#include "dapl_ia_util.h"
#include "dapl_sp_util.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"

/*
 * Prototypes
 */
DAT_RETURN dapli_connection_request(IN dp_ib_cm_handle_t ib_cm_handle,
				    IN DAPL_SP * sp_ptr,
				    IN DAPL_PRIVATE * prd_ptr,
				    IN DAPL_EVD * evd_ptr);

DAPL_EP *dapli_get_sp_ep(IN dp_ib_cm_handle_t ib_cm_handle,
			 IN DAPL_SP * sp_ptr,
			 IN DAT_EVENT_NUMBER dat_event_num);

/*
 * dapls_cr_callback
 *
 * The callback function registered with verbs for passive side of
 * connection requests. The interface is specified by cm_api.h
 *
 *
 * Input:
 * 	ib_cm_handle,		Handle to CM
 * 	ib_cm_event		Specific CM event
 *	instant_data		Private data with DAT ADDRESS header
 * 	context			SP pointer
 *
 * Output:
 * 	None
 *
 */
void dapls_cr_callback(IN dp_ib_cm_handle_t ib_cm_handle, IN const ib_cm_events_t ib_cm_event, IN const void *private_data_ptr,	/* event data */
		       IN const void *context)
{
	DAPL_EP *ep_ptr;
	DAPL_EVD *evd_ptr;
	DAPL_SP *sp_ptr;
	DAPL_PRIVATE *prd_ptr;
	DAT_EVENT_NUMBER dat_event_num;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_CM | DAPL_DBG_TYPE_CALLBACK,
		     "--> dapl_cr_callback! context: %p event: %x cm_handle %p\n",
		     context, ib_cm_event, (void *)ib_cm_handle);

	/*
	 * Passive side of the connection, context is a SP and
	 * we need to look up the EP.
	 */
	sp_ptr = (DAPL_SP *) context;
	/*
	 * The context pointer could have been cleaned up in a racing
	 * CM callback, check to see if we should just exit here
	 */
	if (sp_ptr->header.magic == DAPL_MAGIC_INVALID) {
		return;
	}
	dapl_os_assert(sp_ptr->header.magic == DAPL_MAGIC_PSP ||
		       sp_ptr->header.magic == DAPL_MAGIC_RSP);

	/* Obtain the event number from the provider layer */
	dat_event_num = dapls_ib_get_dat_event(ib_cm_event, DAT_FALSE);

	/*
	 * CONNECT_REQUEST events create an event on the PSP
	 * EVD, which will trigger connection processing. The
	 * sequence is:
	 *    CONNECT_REQUEST         Event to SP
	 *    CONNECTED               Event to EP
	 *    DISCONNECT              Event to EP
	 *
	 * Obtain the EP if required and set an event up on the correct
	 * EVD.
	 */
	if (dat_event_num == DAT_CONNECTION_REQUEST_EVENT) {
		ep_ptr = NULL;
		evd_ptr = sp_ptr->evd_handle;
	} else {
		/* see if there is an EP connected with this CM handle */
		ep_ptr = dapli_get_sp_ep(ib_cm_handle, sp_ptr, dat_event_num);

		/* if we lost a race with the CM just exit. */
		if (ep_ptr == NULL) {
			return;
		}

		evd_ptr = (DAPL_EVD *) ep_ptr->param.connect_evd_handle;
		/* if something has happened to our EVD, bail. */
		if (evd_ptr == NULL) {
			return;
		}
	}

	prd_ptr = (DAPL_PRIVATE *) private_data_ptr;

	dat_status = DAT_INTERNAL_ERROR;	/* init to ERR */

	switch (dat_event_num) {
	case DAT_CONNECTION_REQUEST_EVENT:
		{
			/*
			 * Requests arriving on a disabled SP are immediatly rejected
			 */

			dapl_os_lock(&sp_ptr->header.lock);
			if (sp_ptr->listening == DAT_FALSE) {
				dapl_os_unlock(&sp_ptr->header.lock);
				dapl_dbg_log(DAPL_DBG_TYPE_CM,
					     "---> dapls_cr_callback: conn event on down SP\n");
				(void)dapls_ib_reject_connection(ib_cm_handle,
								 DAT_CONNECTION_EVENT_UNREACHABLE,
								 0, NULL);

				return;
			}

			if (sp_ptr->header.handle_type == DAT_HANDLE_TYPE_RSP) {
				/*
				 * RSP connections only allow a single connection. Close
				 * it down NOW so we reject any further connections.
				 */
				sp_ptr->listening = DAT_FALSE;
			}
			dapl_os_unlock(&sp_ptr->header.lock);

			/*
			 * Only occurs on the passive side of a connection
			 * dapli_connection_request will post the connection
			 * event if appropriate.
			 */
			dat_status = dapli_connection_request(ib_cm_handle,
							      sp_ptr,
							      prd_ptr, evd_ptr);
			/* Set evd_ptr = NULL so we don't generate an event below */
			evd_ptr = NULL;

			break;
		}
	case DAT_CONNECTION_EVENT_ESTABLISHED:
		{
			/* This is just a notification the connection is now
			 * established, there isn't any private data to deal with.
			 *
			 * Update the EP state and cache a copy of the cm handle,
			 * then let the user know we are ready to go.
			 */
			dapl_os_lock(&ep_ptr->header.lock);
			if (ep_ptr->header.magic != DAPL_MAGIC_EP ||
			    ep_ptr->param.ep_state !=
			    DAT_EP_STATE_COMPLETION_PENDING) {
				/* If someone pulled the plug on the EP or connection,
				 * just exit
				 */
				dapl_os_unlock(&ep_ptr->header.lock);
				dat_status = DAT_SUCCESS;
				/* Set evd_ptr = NULL so we don't generate an event below */
				evd_ptr = NULL;

				break;
			}

			ep_ptr->param.ep_state = DAT_EP_STATE_CONNECTED;
			ep_ptr->cm_handle = ib_cm_handle;
			dapl_os_unlock(&ep_ptr->header.lock);

			break;
		}
	case DAT_CONNECTION_EVENT_DISCONNECTED:
		{
			/*
			 * EP is now fully disconnected; initiate any post processing
			 * to reset the underlying QP and get the EP ready for
			 * another connection
			 */
			dapl_os_lock(&ep_ptr->header.lock);
			if (ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECTED) {
				/* The disconnect has already occurred, we are now
				 * cleaned up and ready to exit
				 */
				dapl_os_unlock(&ep_ptr->header.lock);
				return;
			}
			ep_ptr->param.ep_state = DAT_EP_STATE_DISCONNECTED;
			dapls_ib_disconnect_clean(ep_ptr, DAT_FALSE,
						  ib_cm_event);
			dapl_os_unlock(&ep_ptr->header.lock);

			break;
		}
	case DAT_CONNECTION_EVENT_NON_PEER_REJECTED:
	case DAT_CONNECTION_EVENT_PEER_REJECTED:
	case DAT_CONNECTION_EVENT_UNREACHABLE:
		{
			/*
			 * After posting an accept the requesting node has
			 * stopped talking.
			 */
			dapl_os_lock(&ep_ptr->header.lock);
			ep_ptr->param.ep_state = DAT_EP_STATE_DISCONNECTED;
			ep_ptr->cm_handle = IB_INVALID_HANDLE;
			dapls_ib_disconnect_clean(ep_ptr, DAT_FALSE,
						  ib_cm_event);
			dapl_os_unlock(&ep_ptr->header.lock);

			break;
		}
	case DAT_CONNECTION_EVENT_BROKEN:
		{
			dapl_os_lock(&ep_ptr->header.lock);
			ep_ptr->param.ep_state = DAT_EP_STATE_DISCONNECTED;
			dapls_ib_disconnect_clean(ep_ptr, DAT_FALSE,
						  ib_cm_event);
			dapl_os_unlock(&ep_ptr->header.lock);

			break;
		}
	default:
		{
			evd_ptr = NULL;
			dapl_os_assert(0);	/* shouldn't happen */
			break;
		}
	}

	if (evd_ptr != NULL) {
		dat_status = dapls_evd_post_connection_event(evd_ptr,
							     dat_event_num,
							     (DAT_HANDLE)
							     ep_ptr, 0, NULL);
	}

	if (dat_status != DAT_SUCCESS) {
		/* The event post failed; take appropriate action.  */
		(void)dapls_ib_reject_connection(ib_cm_handle,
						 DAT_CONNECTION_EVENT_BROKEN,
						 0, NULL);

		return;
	}
}

/*
 * dapli_connection_request
 *
 * Process a connection request on the Passive side of a connection.
 * Create a CR record and link it on to the SP so we can update it
 * and free it later. Create an EP if specified by the PSP flags.
 *
 * Input:
 * 	ib_cm_handle,
 * 	sp_ptr
 * 	event_ptr
 *	prd_ptr
 *
 * Output:
 * 	None
 *
 * Returns
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_SUCCESS
 *
 */
DAT_RETURN
dapli_connection_request(IN dp_ib_cm_handle_t ib_cm_handle,
			 IN DAPL_SP * sp_ptr,
			 IN DAPL_PRIVATE * prd_ptr, IN DAPL_EVD * evd_ptr)
{
	DAT_RETURN dat_status;

	DAPL_CR *cr_ptr;
	DAPL_EP *ep_ptr;
	DAPL_IA *ia_ptr;
	DAT_SP_HANDLE sp_handle;

	cr_ptr = dapls_cr_alloc(sp_ptr->header.owner_ia);
	if (cr_ptr == NULL) {
		/* Invoking function will call dapls_ib_cm_reject() */
		return DAT_INSUFFICIENT_RESOURCES;
	}

	/*
	 * Set up the CR
	 */
	cr_ptr->sp_ptr = sp_ptr;	/* maintain sp_ptr in case of reject */
	cr_ptr->param.remote_port_qual = 0;
	cr_ptr->ib_cm_handle = ib_cm_handle;
#ifdef IBHOSTS_NAMING
	/*
	 * Special case: pull the remote HCA address from the private data
	 * prefix. This is a spec violation as it introduces a protocol, but
	 * some implementations may find it necessary for a time.
	 */
	cr_ptr->remote_ia_address = prd_ptr->hca_address;
#endif				/* IBHOSTS_NAMING */
	cr_ptr->param.remote_ia_address_ptr =
	    (DAT_IA_ADDRESS_PTR) & cr_ptr->remote_ia_address;
	/*
	 * Copy the remote address and private data out of the private_data
	 * payload and put them in a local structure
	 */

	/* Private data size will be determined by the provider layer */
	cr_ptr->param.private_data = cr_ptr->private_data;
	if (prd_ptr == NULL) {
		cr_ptr->param.private_data_size = 0;
	} else {
		cr_ptr->param.private_data_size =
		    dapls_ib_private_data_size(prd_ptr, DAPL_PDATA_CONN_REQ,
					       sp_ptr->header.owner_ia->
					       hca_ptr);
	}
	if (cr_ptr->param.private_data_size > 0) {
		dapl_os_memcpy(cr_ptr->private_data,
			       prd_ptr->private_data,
			       DAPL_MIN(cr_ptr->param.private_data_size,
					DAPL_MAX_PRIVATE_DATA_SIZE));
	}

	/* EP will be NULL unless RSP service point */
	ep_ptr = (DAPL_EP *) sp_ptr->ep_handle;

	if (sp_ptr->psp_flags == DAT_PSP_PROVIDER_FLAG) {
		/*
		 * Never true for RSP connections
		 *
		 * Create an EP for the user. If we can't allocate an
		 * EP we are out of resources and need to tell the
		 * requestor that we cant help them.
		 */
		ia_ptr = sp_ptr->header.owner_ia;
		ep_ptr = dapl_ep_alloc(ia_ptr, NULL);
		if (ep_ptr == NULL) {
			dapls_cr_free(cr_ptr);
			/* Invoking function will call dapls_ib_cm_reject() */
			return DAT_INSUFFICIENT_RESOURCES;
		}
		ep_ptr->param.ia_handle = ia_ptr;
		ep_ptr->param.local_ia_address_ptr =
		    (DAT_IA_ADDRESS_PTR) & ia_ptr->hca_ptr->hca_address;

		/* Link the EP onto the IA */
		dapl_ia_link_ep(ia_ptr, ep_ptr);
	}

	cr_ptr->param.local_ep_handle = ep_ptr;

	if (ep_ptr != NULL) {
		/* Assign valid EP fields: RSP and PSP_PROVIDER_FLAG only */
		if (sp_ptr->psp_flags == DAT_PSP_PROVIDER_FLAG) {
			ep_ptr->param.ep_state =
			    DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING;
		} else {
			/* RSP */
			dapl_os_assert(sp_ptr->header.handle_type ==
				       DAT_HANDLE_TYPE_RSP);
			ep_ptr->param.ep_state =
			    DAT_EP_STATE_PASSIVE_CONNECTION_PENDING;
		}
		ep_ptr->cm_handle = ib_cm_handle;
	}

	/* link the CR onto the SP so we can pick it up later */
	dapl_sp_link_cr(sp_ptr, cr_ptr);

	/* Post the event.  */
	/* assign sp_ptr to union to avoid typecast errors from some compilers */
	sp_handle.psp_handle = (DAT_PSP_HANDLE) sp_ptr;

	dat_status = dapls_evd_post_cr_arrival_event(evd_ptr,
						     DAT_CONNECTION_REQUEST_EVENT,
						     sp_handle,
						     (DAT_IA_ADDRESS_PTR)
						     & sp_ptr->header.owner_ia->
						     hca_ptr->hca_address,
						     sp_ptr->conn_qual,
						     (DAT_CR_HANDLE) cr_ptr);

	if (dat_status != DAT_SUCCESS) {
		dapls_cr_free(cr_ptr);
		(void)dapls_ib_reject_connection(ib_cm_handle,
						 DAT_CONNECTION_EVENT_BROKEN,
						 0, NULL);

		/* Take the CR off the list, we can't use it */
		dapl_os_lock(&sp_ptr->header.lock);
		dapl_sp_remove_cr(sp_ptr, cr_ptr);
		dapl_os_unlock(&sp_ptr->header.lock);
		return DAT_INSUFFICIENT_RESOURCES;
	}

	return DAT_SUCCESS;
}

/*
 * dapli_get_sp_ep
 *
 * Passive side of a connection is now fully established. Clean
 * up resources and obtain the EP pointer associated with a CR in
 * the SP
 *
 * Input:
 * 	ib_cm_handle,
 * 	sp_ptr
 *	connection_event
 *
 * Output:
 *	none
 *
 * Returns
 * 	ep_ptr
 *
 */
DAPL_EP *dapli_get_sp_ep(IN dp_ib_cm_handle_t ib_cm_handle,
			 IN DAPL_SP * sp_ptr, IN DAT_EVENT_NUMBER dat_event_num)
{
	DAPL_CR *cr_ptr;
	DAPL_EP *ep_ptr;

	/*
	 * acquire the lock, we may be racing with other threads here
	 */
	dapl_os_lock(&sp_ptr->header.lock);

	/* Verify under lock that the SP is still valid */
	if (sp_ptr->header.magic == DAPL_MAGIC_INVALID) {
		dapl_os_unlock(&sp_ptr->header.lock);
		return NULL;
	}
	/*
	 * There are potentially multiple connections in progress. Need to
	 * go through the list and find the one we are interested
	 * in. There is no guarantee of order. dapl_sp_search_cr
	 * leaves the CR on the SP queue.
	 */
	cr_ptr = dapl_sp_search_cr(sp_ptr, ib_cm_handle);
	if (cr_ptr == NULL) {
		dapl_os_unlock(&sp_ptr->header.lock);
		return NULL;
	}

	ep_ptr = (DAPL_EP *) cr_ptr->param.local_ep_handle;

	/* Quick check to ensure our EP is still valid */
	if ((DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP))) {
		ep_ptr = NULL;
	}

	/* The CR record is discarded in all except for the CONNECTED case,
	 * as it will have no further relevance.
	 */
	if (dat_event_num != DAT_CONNECTION_EVENT_ESTABLISHED) {
		/* Remove the CR from the queue */
		dapl_sp_remove_cr(sp_ptr, cr_ptr);

		if (ep_ptr != NULL) {
			ep_ptr->cr_ptr = NULL;
		}

		/*
		 * If this SP has been removed from service, free it
		 * up after the last CR is removed
		 */
		if (sp_ptr->listening != DAT_TRUE && sp_ptr->cr_list_count == 0
		    && sp_ptr->state != DAPL_SP_STATE_FREE) {
			dapl_dbg_log(DAPL_DBG_TYPE_CM,
				     "--> dapli_get_sp_ep! disconnect dump sp: %p \n",
				     sp_ptr);
			/* Decrement the ref count on the EVD */
			if (sp_ptr->evd_handle) {
				dapl_os_atomic_dec(&
						   ((DAPL_EVD *) sp_ptr->
						    evd_handle)->evd_ref_count);
				sp_ptr->evd_handle = NULL;
			}
			sp_ptr->state = DAPL_SP_STATE_FREE;
			dapl_os_unlock(&sp_ptr->header.lock);
			(void)dapls_ib_remove_conn_listener(sp_ptr->header.
							    owner_ia, sp_ptr);
			dapls_ia_unlink_sp((DAPL_IA *) sp_ptr->header.owner_ia,
					   sp_ptr);
			dapls_sp_free_sp(sp_ptr);
			dapls_cr_free(cr_ptr);
			goto skip_unlock;
		}

		dapl_os_unlock(&sp_ptr->header.lock);
		/* free memory outside of the lock */
		dapls_cr_free(cr_ptr);
	} else {
		dapl_os_unlock(&sp_ptr->header.lock);
	}

      skip_unlock:
	return ep_ptr;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  c-brace-offset: -4
 *  tab-width: 8
 * End:
 */
