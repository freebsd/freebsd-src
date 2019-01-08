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
 * MODULE: dapl_cr_accept.c
 *
 * PURPOSE: Connection management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_evd_util.h"

/*
 * dapl_cr_accept
 *
 * DAPL Requirements Version xxx, 6.4.2.1
 *
 * Establish a connection between active remote side requesting Endpoint
 * and passic side local Endpoint.
 *
 * Input:
 *	cr_handle
 *	ep_handle
 *	private_data_size
 *	private_data
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_ATTRIBUTE
 */
DAT_RETURN DAT_API
dapl_cr_accept(IN DAT_CR_HANDLE cr_handle,
	       IN DAT_EP_HANDLE ep_handle,
	       IN DAT_COUNT private_data_size, IN const DAT_PVOID private_data)
{
	DAPL_EP *ep_ptr;
	DAT_RETURN dat_status;
	DAPL_CR *cr_ptr;
	DAT_EP_STATE entry_ep_state;
	DAT_EP_HANDLE entry_ep_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_cr_accept (CR %p EP %p, PDsz %d PD %p)\n",
		     cr_handle, ep_handle, private_data_size, private_data);

	if (DAPL_BAD_HANDLE(cr_handle, DAPL_MAGIC_CR)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CR);
		goto bail;
	}

	cr_ptr = (DAPL_CR *) cr_handle;

	/*
	 * Return an error if we have an ep_handle and the CR already has an
	 * EP, indicating this is an RSP connection or PSP_PROVIDER_FLAG was
	 * specified.
	 */
	if (ep_handle != NULL &&
	    (DAPL_BAD_HANDLE(ep_handle, DAPL_MAGIC_EP) ||
	     cr_ptr->param.local_ep_handle != NULL)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	/* Make sure we have an EP handle in one place or another */
	if (ep_handle == NULL && cr_ptr->param.local_ep_handle == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	if ((0 != private_data_size) && (NULL == private_data)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}

	/*
	 * ep_handle is NULL if the user specified DAT_PSP_PROVIDER_FLAG
	 * OR this is an RSP connection; retrieve it from the cr.
	 */
	if (ep_handle == NULL) {
		ep_handle = cr_ptr->param.local_ep_handle;
		if (((((DAPL_EP *) ep_handle)->param.ep_state !=
		      DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING)
		     && (((DAPL_EP *) ep_handle)->param.ep_state !=
			 DAT_EP_STATE_PASSIVE_CONNECTION_PENDING))
		    && (((DAPL_EP *) ep_handle)->param.ep_attr.service_type ==
			DAT_SERVICE_TYPE_RC)) {
			return DAT_INVALID_STATE;
		}
	} else {
		/* ensure this EP isn't connected or in use */
		if ((((DAPL_EP *) ep_handle)->param.ep_state !=
		     DAT_EP_STATE_UNCONNECTED)
		    && (((DAPL_EP *) ep_handle)->param.ep_attr.service_type ==
			DAT_SERVICE_TYPE_RC)) {
			return DAT_INVALID_STATE;
		}
	}

	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Verify the attributes of the EP handle before we connect it. Test
	 * all of the handles to make sure they are currently valid.
	 * Specifically:
	 *   pz_handle              required
	 *   recv_evd_handle        optional, but must be valid
	 *   request_evd_handle     optional, but must be valid
	 *   connect_evd_handle     required
	 * We do all verification and state change under lock, at which
	 * point the EP state should protect us from most races.
	 */
	dapl_os_lock(&ep_ptr->header.lock);
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

	    || (ep_ptr->param.request_evd_handle != DAT_HANDLE_NULL &&
		(DAPL_BAD_HANDLE
		 (ep_ptr->param.request_evd_handle, DAPL_MAGIC_EVD)))) {
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	/* The qp must be attached by this point! */
	if (ep_ptr->qp_state == DAPL_QP_STATE_UNATTACHED) {
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	entry_ep_state = ep_ptr->param.ep_state;
	entry_ep_handle = cr_ptr->param.local_ep_handle;
	ep_ptr->param.ep_state = DAT_EP_STATE_COMPLETION_PENDING;
	ep_ptr->cr_ptr = cr_ptr;
	ep_ptr->param.remote_ia_address_ptr =
	    cr_ptr->param.remote_ia_address_ptr;
	cr_ptr->param.local_ep_handle = ep_handle;

	dapl_os_unlock(&ep_ptr->header.lock);

	dat_status = dapls_ib_accept_connection(cr_handle,
						ep_handle,
						private_data_size,
						private_data);

	/*
	 * If the provider failed, unwind the damage so we are back at
	 * the initial state.
	 */
	if (dat_status != DAT_SUCCESS) {
		if (DAT_GET_TYPE(dat_status) == DAT_INVALID_ADDRESS) {
			/* The remote connection request has disappeared; timeout,
			 * system error, app termination, perhaps other reasons.
			 */
			dat_status =
			    dapls_evd_post_connection_event(ep_ptr->param.
							    connect_evd_handle,
							    DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR,
							    (DAT_HANDLE) ep_ptr,
							    0, 0);

			cr_ptr->header.magic = DAPL_MAGIC_CR_DESTROYED;
		} else {
			ep_ptr->param.ep_state = entry_ep_state;
			cr_ptr->param.local_ep_handle = entry_ep_handle;
			ep_ptr->cr_ptr = NULL;
			ep_ptr->param.remote_ia_address_ptr = NULL;
		}

		/*
		 * After restoring values above, we now check if we need
		 * to translate the error
		 */
		if (DAT_GET_TYPE(dat_status) == DAT_LENGTH_ERROR) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		}

	} else {
		/* Make this CR invalid. We need to hang on to it until
		 * the connection terminates, but it's destroyed from
		 * the app point of view.
		 */
		cr_ptr->header.magic = DAPL_MAGIC_CR_DESTROYED;
	}

      bail:
	return dat_status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
