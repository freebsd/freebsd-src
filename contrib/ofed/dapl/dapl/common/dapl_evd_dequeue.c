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
 * MODULE: dapl_evd_dequeue.c
 *
 * PURPOSE: Event Management
 *
 * Description:  Interfaces in this file are completely described in
 *               the uDAPL 1.1 API, Chapter 6, section 3
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ring_buffer_util.h"
#include "dapl_evd_util.h"

/*
 * dapl_evd_dequeue
 * 
 * DAPL Requirements Version xxx, 6.3.2.7
 * 
 * Remove first element from an event dispatcher
 * 
 * Input:
 * 	evd_handle
 * 
 * Output:
 * 	event
 * 
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INVALID_STATE
 * 	DAT_QUEUE_EMPTY
 */

DAT_RETURN DAT_API dapl_evd_dequeue(IN DAT_EVD_HANDLE evd_handle,
				    OUT DAT_EVENT * event)
{
	DAPL_EVD *evd_ptr;
	DAT_EVENT *local_event;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_evd_dequeue (%p, %p)\n", evd_handle, event);

	evd_ptr = (DAPL_EVD *) evd_handle;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
		goto bail;
	}

	if (event == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}
	DAPL_CNTR(evd_ptr, DCNT_EVD_DEQUEUE);

	/*
	 * We need to dequeue under lock, as the IB OS Access API
	 * restricts us from having multiple threads in CQ poll, and the
	 * DAPL 1.1 API allows multiple threads in dat_evd_dequeue()
	 */
	dapl_os_lock(&evd_ptr->header.lock);

	/*
	 * Make sure there are no other waiters and the evd is active.
	 * Currently this means only the OPEN state is allowed.
	 */
	if (evd_ptr->evd_state != DAPL_EVD_STATE_OPEN ||
	    evd_ptr->catastrophic_overflow) {
		dapl_os_unlock(&evd_ptr->header.lock);
		dat_status = DAT_ERROR(DAT_INVALID_STATE, 0);
		goto bail;
	}

	/*
	 * Try the EVD rbuf first; poll from the CQ only if that's empty.
	 * This keeps events in order if dat_evd_wait() has copied events
	 * from CQ to EVD.  
	 */
	local_event =
	    (DAT_EVENT *) dapls_rbuf_remove(&evd_ptr->pending_event_queue);
	if (local_event != NULL) {
		*event = *local_event;
		dat_status = dapls_rbuf_add(&evd_ptr->free_event_queue,
					    local_event);
		DAPL_CNTR(evd_ptr, DCNT_EVD_DEQUEUE_FOUND);

	} else if (evd_ptr->ib_cq_handle != IB_INVALID_HANDLE) {
		dat_status = dapls_evd_cq_poll_to_event(evd_ptr, event);
		DAPL_CNTR(evd_ptr, DCNT_EVD_DEQUEUE_POLL);
	} else {
		dat_status = DAT_ERROR(DAT_QUEUE_EMPTY, 0);
		DAPL_CNTR(evd_ptr, DCNT_EVD_DEQUEUE_NOT_FOUND);
	}

	dapl_os_unlock(&evd_ptr->header.lock);
      bail:
	dapl_dbg_log(DAPL_DBG_TYPE_RTN,
		     "dapl_evd_dequeue () returns 0x%x\n", dat_status);

	return dat_status;
}
