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
 * MODULE: dapl_evd_wait.c
 *
 * PURPOSE: EVENT management
 *
 * Description: Interfaces in this file are completely defined in 
 *              the uDAPL 1.1 API specification
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_evd_util.h"
#include "dapl_ring_buffer_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_evd_wait
 *
 * UDAPL Requirements Version xxx, 
 *
 * Wait, up to specified timeout, for notification event on EVD.
 * Then return first available event.
 *
 * Input:
 * 	evd_handle
 * 	timeout
 *
 * Output:
 * 	event
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INVALID_STATE
 */

DAT_RETURN DAT_API dapl_evd_wait(IN DAT_EVD_HANDLE evd_handle,
				 IN DAT_TIMEOUT time_out,
				 IN DAT_COUNT threshold,
				 OUT DAT_EVENT * event, OUT DAT_COUNT * nmore)
{
	DAPL_EVD *evd_ptr;
	DAT_RETURN dat_status;
	DAT_EVENT *local_event;
	DAT_BOOLEAN notify_requested = DAT_FALSE;
	DAT_BOOLEAN waitable;
	DAPL_EVD_STATE evd_state;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_evd_wait (%p, %d, %d, %p, %p)\n",
		     evd_handle, time_out, threshold, event, nmore);

	evd_ptr = (DAPL_EVD *) evd_handle;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(evd_ptr, DAPL_MAGIC_EVD)) {
		/*
		 * We return directly rather than bailing because
		 * bailing attempts to update the evd, and we don't have
		 * one.
		 */
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
		goto bail;
	}
	if (!event) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}
	if (!nmore) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG5);
		goto bail;
	}
	if (threshold <= 0 ||
	    (threshold > 1
	     && evd_ptr->completion_type != DAPL_EVD_STATE_THRESHOLD)
	    || threshold > evd_ptr->qlen) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}
	if (evd_ptr->catastrophic_overflow) {
		dat_status = DAT_ERROR(DAT_INVALID_STATE, 0);
		goto bail;
	}
	DAPL_CNTR(evd_ptr, DCNT_EVD_WAIT);

	dapl_dbg_log(DAPL_DBG_TYPE_EVD,
		     "dapl_evd_wait: EVD %p, CQ %p\n",
		     evd_ptr, (void *)evd_ptr->ib_cq_handle);

	/*
	 * Make sure there are no other waiters and the evd is active.
	 * Currently this means only the OPEN state is allowed.
	 * Do this atomically.  We need to take a lock to synchronize
	 * with dapl_evd_dequeue(), but the atomic transition allows
	 * non-locking synchronization with dapl_evd_query() and
	 * dapl_evd_{query,enable,disable,{set,clear}_unwaitable}.
	 */

	dapl_os_lock(&evd_ptr->header.lock);
	waitable = evd_ptr->evd_waitable;

	dapl_os_assert(sizeof(DAT_COUNT) == sizeof(DAPL_EVD_STATE));
	evd_state = evd_ptr->evd_state;
	if (evd_ptr->evd_state == DAPL_EVD_STATE_OPEN)
		evd_ptr->evd_state = DAPL_EVD_STATE_WAITED;

	if (evd_state != DAPL_EVD_STATE_OPEN) {
		/* Bogus state, bail out */
		dat_status = DAT_ERROR(DAT_INVALID_STATE, 0);
		goto bail;
	}

	if (!waitable) {
		/* This EVD is not waitable, reset the state and bail */
		if (evd_ptr->evd_state == DAPL_EVD_STATE_WAITED)
			evd_ptr->evd_state = evd_state;

		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      DAT_INVALID_STATE_EVD_UNWAITABLE);
		goto bail;
	}

	/*
	 * We now own the EVD, even though we don't have the lock anymore,
	 * because we're in the WAITED state.
	 */

	evd_ptr->threshold = threshold;

	for (;;) {
		/*
		 * Ideally we'd just check the number of entries on the CQ, but
		 * we don't have a way to do that.  Because we have to set *nmore
		 * at some point in this routine, we'll need to do this copy
		 * sometime even if threshold == 1.
		 *
		 * For connection evd or async evd, the function checks and
		 * return right away if the ib_cq_handle associate with these evd
		 * equal to IB_INVALID_HANDLE
		 */
		dapl_os_unlock(&evd_ptr->header.lock);
		dapls_evd_copy_cq(evd_ptr);
		dapl_os_lock(&evd_ptr->header.lock);

		if (dapls_rbuf_count(&evd_ptr->pending_event_queue) >=
		    threshold) {
			break;
		}

		/*
		 * Do not enable the completion notification if this evd is not 
		 * a DTO_EVD or RMR_BIND_EVD
		 */
		if ((!notify_requested) &&
		    (evd_ptr->evd_flags & (DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG))) {
			dat_status =
			    dapls_ib_completion_notify(evd_ptr->header.
						       owner_ia->hca_ptr->
						       ib_hca_handle, evd_ptr,
						       (evd_ptr->
							completion_type ==
							DAPL_EVD_STATE_SOLICITED_WAIT)
						       ? IB_NOTIFY_ON_SOLIC_COMP
						       :
						       IB_NOTIFY_ON_NEXT_COMP);

			DAPL_CNTR(evd_ptr, DCNT_EVD_WAIT_NOTIFY);
			/* FIXME report error */
			dapl_os_assert(dat_status == DAT_SUCCESS);

			notify_requested = DAT_TRUE;

			/* Try again.  */
			continue;
		}

		/*
		 * Unused by poster; it has no way to tell how many
		 * items are on the queue without copying them over to the
		 * EVD queue, and we're the only ones allowed to dequeue
		 * from the CQ for synchronization/locking reasons.
		 */
		evd_ptr->threshold = threshold;

		DAPL_CNTR(evd_ptr, DCNT_EVD_WAIT_BLOCKED);
		dapl_os_unlock(&evd_ptr->header.lock);

		if ((!evd_ptr->cno_ptr) && 
		    (evd_ptr->evd_flags & (DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG))) {
			dat_status = dapls_evd_dto_wait(evd_ptr, time_out);
		} else {
			dat_status = dapl_os_wait_object_wait(&evd_ptr->wait_object, time_out);
		}

		dapl_os_lock(&evd_ptr->header.lock);

		/*
		 * FIXME: if the thread loops around and waits again
		 * the time_out value needs to be updated.
		 */

		notify_requested = DAT_FALSE;	/* We've used it up.  */

		/* See if we were awakened by evd_set_unwaitable */
		if (!evd_ptr->evd_waitable) {
			dat_status = DAT_ERROR(DAT_INVALID_STATE, 0);
		}

		if (dat_status != DAT_SUCCESS) {
			/*
			 * If the status is DAT_TIMEOUT, we'll break out of the
			 * loop, *not* dequeue an event (because dat_status
			 * != DAT_SUCCESS), set *nmore (as we should for timeout)
			 * and return DAT_TIMEOUT.
			 */
			break;
		}
	}

	evd_ptr->evd_state = DAPL_EVD_STATE_OPEN;

	if (dat_status == DAT_SUCCESS) {
		local_event = dapls_rbuf_remove(&evd_ptr->pending_event_queue);
		*event = *local_event;
		dapls_rbuf_add(&evd_ptr->free_event_queue, local_event);
	}

	/*
	 * Valid if dat_status == DAT_SUCCESS || dat_status == DAT_TIMEOUT
	 * Undefined otherwise, so ok to set it.
	 */
	*nmore = dapls_rbuf_count(&evd_ptr->pending_event_queue);

      bail:
	dapl_os_unlock(&evd_ptr->header.lock);
	if (dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_RTN,
			     "dapl_evd_wait () returns 0x%x\n", dat_status);
	}
	return dat_status;
}
