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
 * MODULE: dapl_ep_disconnect.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"
#include "dapl_ep_util.h"
#include "dapl_sp_util.h"
#include "dapl_evd_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_ep_disconnect
 *
 * DAPL Requirements Version xxx, 6.5.9
 *
 * Terminate a connection.
 *
 * Input:
 *	ep_handle
 *	disconnect_flags
 *
 * Output:
 *	None
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_ep_disconnect(IN DAT_EP_HANDLE ep_handle,
		   IN DAT_CLOSE_FLAGS disconnect_flags)
{
	DAPL_EP *ep_ptr;
	DAPL_EVD *evd_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API | DAPL_DBG_TYPE_CM,
		     "dapl_ep_disconnect (%p, %x)\n",
		     ep_handle, disconnect_flags);

	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Verify parameter & state
	 */
	if (DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}
	DAPL_CNTR(ep_ptr, DCNT_EP_DISCONNECT);

	/*
	 * Do the verification of parameters and the state change
	 * atomically.
	 */
	dapl_os_lock(&ep_ptr->header.lock);

	/* Disconnecting a disconnected EP is a no-op. */
	if (ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECTED ||
	    ep_ptr->param.ep_attr.service_type != DAT_SERVICE_TYPE_RC) {
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status = DAT_SUCCESS;
		goto bail;
	}

	/* Check the EP state to ensure we are queiscent. Note that
	 * we may get called in UNCONNECTED state in order to remove
	 * RECV requests from the queue prior to destroying an EP.
	 * See the states in the spec at 6.5.1 Endpont Lifecycle
	 */
	if (ep_ptr->param.ep_state != DAT_EP_STATE_CONNECTED &&
	    ep_ptr->param.ep_state != DAT_EP_STATE_ACTIVE_CONNECTION_PENDING &&
	    ep_ptr->param.ep_state != DAT_EP_STATE_COMPLETION_PENDING &&
	    ep_ptr->param.ep_state != DAT_EP_STATE_DISCONNECT_PENDING) {
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      dapls_ep_state_subtype(ep_ptr));
		goto bail;
	}

	if (ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECT_PENDING &&
	    disconnect_flags != DAT_CLOSE_ABRUPT_FLAG) {
		/*
		 * If in state DISCONNECT_PENDING then this must be an
		 * ABRUPT disconnect
		 */
		dapl_os_unlock(&ep_ptr->header.lock);
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	if (ep_ptr->param.ep_state == DAT_EP_STATE_ACTIVE_CONNECTION_PENDING ||
	    ep_ptr->param.ep_state == DAT_EP_STATE_COMPLETION_PENDING) {
		/*
		 * Beginning or waiting on a connection: abort and reset the
		 * state
		 */
		ep_ptr->param.ep_state = DAT_EP_STATE_DISCONNECTED;

		dapl_os_unlock(&ep_ptr->header.lock);
		/* disconnect and make sure we get no callbacks */
		(void)dapls_ib_disconnect(ep_ptr, DAT_CLOSE_ABRUPT_FLAG);

		/* clean up connection state */
		dapl_sp_remove_ep(ep_ptr);

		evd_ptr = (DAPL_EVD *) ep_ptr->param.connect_evd_handle;
		dapls_evd_post_connection_event(evd_ptr,
						DAT_CONNECTION_EVENT_DISCONNECTED,
						(DAT_HANDLE) ep_ptr, 0, 0);
		dat_status = DAT_SUCCESS;
		goto bail;
	}

	/*
	 * Transition the EP state to DISCONNECT_PENDING if we are
	 * CONNECTED. Otherwise we do not get a disconnect event and will be
	 * stuck in DISCONNECT_PENDING.
	 *
	 * If the user specifies a graceful disconnect, the underlying
	 * provider should complete all DTOs before disconnecting; in IB
	 * terms, this means setting the QP state to SQD before completing
	 * the disconnect state transitions.
	 */
	if (ep_ptr->param.ep_state == DAT_EP_STATE_CONNECTED) {
		ep_ptr->param.ep_state = DAT_EP_STATE_DISCONNECT_PENDING;
	}
	dapl_os_unlock(&ep_ptr->header.lock);
	dat_status = dapls_ib_disconnect(ep_ptr, disconnect_flags);

      bail:
	dapl_dbg_log(DAPL_DBG_TYPE_RTN | DAPL_DBG_TYPE_CM,
		     "dapl_ep_disconnect (EP %p) returns 0x%x\n",
		     ep_ptr, dat_status);

	return dat_status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
