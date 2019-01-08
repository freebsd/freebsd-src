/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
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
 * MODULE: dapl_evd_util.c
 *
 * PURPOSE: Manage EVD Info structure
 *
 * $Id: dapl_evd_util.c 1410 2006-07-19 17:12:02Z ardavis $
 **********************************************************************/

#include "dapl_evd_util.h"
#include "dapl_ia_util.h"
#include "dapl_cno_util.h"
#include "dapl_ring_buffer_util.h"
#include "dapl_adapter_util.h"
#include "dapl_cookie.h"
#include "dapl.h"
#include "dapl_cr_util.h"
#include "dapl_sp_util.h"
#include "dapl_ep_util.h"

STATIC _INLINE_ void dapli_evd_eh_print_cqe(IN ib_work_completion_t * cqe);

DAT_RETURN dapli_evd_event_alloc(IN DAPL_EVD * evd_ptr, IN DAT_COUNT qlen);

char *dapl_event_str(IN DAT_EVENT_NUMBER event_num)
{
#if defined(DAPL_DBG)
	struct dat_event_str {
		char *str;
		DAT_EVENT_NUMBER num;
	};
	static struct dat_event_str events[] = {
		{"DAT_DTO_COMPLETION_EVENT", DAT_DTO_COMPLETION_EVENT},
		{"DAT_RMR_BIND_COMPLETION_EVENT",
		 DAT_RMR_BIND_COMPLETION_EVENT},
		{"DAT_CONNECTION_REQUEST_EVENT", DAT_CONNECTION_REQUEST_EVENT},
		{"DAT_CONNECTION_EVENT_ESTABLISHED",
		 DAT_CONNECTION_EVENT_ESTABLISHED},
		{"DAT_CONNECTION_EVENT_PEER_REJECTED",
		 DAT_CONNECTION_EVENT_PEER_REJECTED},
		{"DAT_CONNECTION_EVENT_NON_PEER_REJECTED",
		 DAT_CONNECTION_EVENT_NON_PEER_REJECTED},
		{"DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR",
		 DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR},
		{"DAT_CONNECTION_EVENT_DISCONNECTED",
		 DAT_CONNECTION_EVENT_DISCONNECTED},
		{"DAT_CONNECTION_EVENT_BROKEN", DAT_CONNECTION_EVENT_BROKEN},
		{"DAT_CONNECTION_EVENT_TIMED_OUT",
		 DAT_CONNECTION_EVENT_TIMED_OUT},
		{"DAT_CONNECTION_EVENT_UNREACHABLE",
		 DAT_CONNECTION_EVENT_UNREACHABLE},
		{"DAT_ASYNC_ERROR_EVD_OVERFLOW", DAT_ASYNC_ERROR_EVD_OVERFLOW},
		{"DAT_ASYNC_ERROR_IA_CATASTROPHIC",
		 DAT_ASYNC_ERROR_IA_CATASTROPHIC},
		{"DAT_ASYNC_ERROR_EP_BROKEN", DAT_ASYNC_ERROR_EP_BROKEN},
		{"DAT_ASYNC_ERROR_TIMED_OUT", DAT_ASYNC_ERROR_TIMED_OUT},
		{"DAT_ASYNC_ERROR_PROVIDER_INTERNAL_ERROR",
		 DAT_ASYNC_ERROR_PROVIDER_INTERNAL_ERROR},
		{"DAT_HA_DOWN_TO_1", DAT_HA_DOWN_TO_1},
		{"DAT_HA_UP_TO_MULTI_PATH", DAT_HA_UP_TO_MULTI_PATH},
		{"DAT_SOFTWARE_EVENT", DAT_SOFTWARE_EVENT},
#ifdef DAT_EXTENSIONS
		{"DAT_EXTENSION_EVENT", DAT_EXTENSION_EVENT},
		{"DAT_IB_EXTENSION_RANGE_BASE", DAT_IB_EXTENSION_RANGE_BASE},
		{"DAT_IB_UD_CONNECTION_REQUEST_EVENT",
		 DAT_IB_EXTENSION_RANGE_BASE + 1},
		{"DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED",
		 DAT_IB_EXTENSION_RANGE_BASE + 2},
		{"DAT_IW_EXTENSION_RANGE_BASE", DAT_IW_EXTENSION_RANGE_BASE},
#endif				/* DAT_EXTENSIONS */
		{NULL, 0},
	};
	int i;

	for (i = 0; events[i].str; i++) {
		if (events[i].num == event_num)
			return events[i].str;
	}
	return "Unknown DAT event?";
#else
	static char str[16];
	sprintf(str, "%x", event_num);
	return str;
#endif
}

/*
 * dapls_evd_internal_create
 *
 * actually create the evd.  this is called after all parameter checking
 * has been performed in dapl_ep_create.  it is also called from dapl_ia_open
 * to create the default async evd.
 *
 * Input:
 * 	ia_ptr
 *	cno_ptr
 *	qlen
 *	evd_flags
 *
 * Output:
 * 	evd_ptr_ptr
 *
 * Returns:
 * 	none
 *
 */

DAT_RETURN
dapls_evd_internal_create(DAPL_IA * ia_ptr,
			  DAPL_CNO * cno_ptr,
			  DAT_COUNT min_qlen,
			  DAT_EVD_FLAGS evd_flags, DAPL_EVD ** evd_ptr_ptr)
{
	DAPL_EVD *evd_ptr;
	DAT_COUNT cq_len;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;
	*evd_ptr_ptr = NULL;
	cq_len = min_qlen;

	evd_ptr = dapls_evd_alloc(ia_ptr, cno_ptr, evd_flags, min_qlen);
	if (!evd_ptr) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	/*
	 * If we are dealing with event streams besides a CQ event stream,
	 * be conservative and set producer side locking.  Otherwise, no.
	 * Note: CNO is not considered CQ event stream.
	 */
	evd_ptr->evd_producer_locking_needed =
	    (!(evd_flags & (DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG)) ||
	     evd_ptr->cno_ptr);

	/* Before we setup any callbacks, transition state to OPEN.  */
	evd_ptr->evd_state = DAPL_EVD_STATE_OPEN;

	if (evd_flags & DAT_EVD_ASYNC_FLAG) {
		/*
		 * There is no cq associate with async evd. Set it to invalid
		 */
		evd_ptr->ib_cq_handle = IB_INVALID_HANDLE;

	} else if (0 != (evd_flags & ~(DAT_EVD_SOFTWARE_FLAG
				       | DAT_EVD_CONNECTION_FLAG
				       | DAT_EVD_CR_FLAG))) {
#if defined(_VENDOR_IBAL_)
		/* 
		 * The creation of CQ required a PD (PZ) associated with it and
		 * we do not have a PD here; therefore, the work-around is that we
		 * will postpone the creation of the cq till the creation of QP which
		 * this cq will associate with.
		 */
		evd_ptr->ib_cq_handle = IB_INVALID_HANDLE;
#else
		dat_status = dapls_ib_cq_alloc(ia_ptr, evd_ptr, &cq_len);
		if (dat_status != DAT_SUCCESS) {
			goto bail;
		}

		/* Now reset the cq_len in the attributes, it may have changed */
		evd_ptr->qlen = cq_len;

		dat_status =
		    dapls_ib_setup_async_callback(ia_ptr,
						  DAPL_ASYNC_CQ_COMPLETION,
						  evd_ptr,
						  (ib_async_handler_t)
						  dapl_evd_dto_callback,
						  evd_ptr);
		if (dat_status != DAT_SUCCESS) {
			goto bail;
		}

		dat_status = dapls_set_cq_notify(ia_ptr, evd_ptr);

		if (dat_status != DAT_SUCCESS) {
			goto bail;
		}
#endif				/* _VENDOR_IBAL_ */
	}

	/* We now have an accurate count of events, so allocate them into
	 * the EVD
	 */
	dat_status = dapli_evd_event_alloc(evd_ptr, cq_len);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}

	dapl_ia_link_evd(ia_ptr, evd_ptr);
	*evd_ptr_ptr = evd_ptr;

      bail:
	if (dat_status != DAT_SUCCESS) {
		if (evd_ptr) {
			dapls_evd_dealloc(evd_ptr);
		}
	}

	return dat_status;
}

/*
 * dapls_evd_alloc
 *
 * alloc and initialize an EVD struct
 *
 * Input:
 * 	ia
 *
 * Output:
 * 	evd_ptr
 *
 * Returns:
 * 	none
 *
 */
DAPL_EVD *dapls_evd_alloc(IN DAPL_IA * ia_ptr,
			  IN DAPL_CNO * cno_ptr,
			  IN DAT_EVD_FLAGS evd_flags, IN DAT_COUNT qlen)
{
	DAPL_EVD *evd_ptr;

	/* Allocate EVD */
	evd_ptr = (DAPL_EVD *) dapl_os_alloc(sizeof(DAPL_EVD));
	if (!evd_ptr) {
		goto bail;
	}

	/* zero the structure */
	dapl_os_memzero(evd_ptr, sizeof(DAPL_EVD));

#ifdef DAPL_COUNTERS
	/* Allocate counters */
	evd_ptr->cntrs =
	    dapl_os_alloc(sizeof(DAT_UINT64) * DCNT_EVD_ALL_COUNTERS);
	if (evd_ptr->cntrs == NULL) {
		dapl_os_free(evd_ptr, sizeof(DAPL_EVD));
		return (NULL);
	}
	dapl_os_memzero(evd_ptr->cntrs,
			sizeof(DAT_UINT64) * DCNT_EVD_ALL_COUNTERS);
#endif				/* DAPL_COUNTERS */

	/*
	 * initialize the header
	 */
	evd_ptr->header.provider = ia_ptr->header.provider;
	evd_ptr->header.magic = DAPL_MAGIC_EVD;
	evd_ptr->header.handle_type = DAT_HANDLE_TYPE_EVD;
	evd_ptr->header.owner_ia = ia_ptr;
	evd_ptr->header.user_context.as_64 = 0;
	evd_ptr->header.user_context.as_ptr = NULL;
	dapl_llist_init_entry(&evd_ptr->header.ia_list_entry);
	dapl_os_lock_init(&evd_ptr->header.lock);

	/*
	 * Initialize the body
	 */
	evd_ptr->evd_state = DAPL_EVD_STATE_INITIAL;
	evd_ptr->evd_flags = evd_flags;
	evd_ptr->evd_enabled = DAT_TRUE;
	evd_ptr->evd_waitable = DAT_TRUE;
	evd_ptr->evd_producer_locking_needed = 1;	/* Conservative value.  */
	evd_ptr->ib_cq_handle = IB_INVALID_HANDLE;
	dapl_os_atomic_set(&evd_ptr->evd_ref_count, 0);
	evd_ptr->catastrophic_overflow = DAT_FALSE;
	evd_ptr->qlen = qlen;
	evd_ptr->completion_type = DAPL_EVD_STATE_THRESHOLD;	/* FIXME: should be DAPL_EVD_STATE_INIT */
	dapl_os_wait_object_init(&evd_ptr->wait_object);

	evd_ptr->cno_active_count = 0;
	if (cno_ptr != NULL) {
		/* Take a reference count on the CNO */
		dapl_os_atomic_inc(&cno_ptr->cno_ref_count);
	}
	evd_ptr->cno_ptr = cno_ptr;

      bail:
	return evd_ptr;
}

/*
 * dapls_evd_event_alloc
 *
 * alloc events into an EVD.
 *
 * Input:
 * 	evd_ptr
 *	qlen
 *
 * Output:
 * 	NONE
 *
 * Returns:
 * 	DAT_SUCCESS
 *	ERROR
 *
 */
DAT_RETURN dapli_evd_event_alloc(IN DAPL_EVD * evd_ptr, IN DAT_COUNT qlen)
{
	DAT_EVENT *event_ptr;
	DAT_COUNT i;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	/* Allocate EVENTs */
	event_ptr =
	    (DAT_EVENT *) dapl_os_alloc(evd_ptr->qlen * sizeof(DAT_EVENT));
	if (event_ptr == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}
	evd_ptr->events = event_ptr;

	/* allocate free event queue */
	dat_status = dapls_rbuf_alloc(&evd_ptr->free_event_queue, qlen);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}

	/* allocate pending event queue */
	dat_status = dapls_rbuf_alloc(&evd_ptr->pending_event_queue, qlen);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}

	/* add events to free event queue */
	for (i = 0; i < evd_ptr->qlen; i++) {
		dapls_rbuf_add(&evd_ptr->free_event_queue, (void *)event_ptr);
		event_ptr++;
	}

	evd_ptr->cq_notified = DAT_FALSE;
	evd_ptr->cq_notified_when = 0;
	evd_ptr->threshold = 0;

      bail:
	return dat_status;
}

/*
 * dapls_evd_event_realloc
 *
 * realloc events into an EVD.
 *
 * Input:
 * 	evd_ptr
 *	qlen
 *
 * Output:
 * 	NONE
 *
 * Returns:
 * 	DAT_SUCCESS
 *	ERROR
 *
 */
DAT_RETURN dapls_evd_event_realloc(IN DAPL_EVD * evd_ptr, IN DAT_COUNT qlen)
{
	DAT_EVENT *events;
	DAT_COUNT old_qlen;
	DAT_COUNT i;
	intptr_t diff;
	DAT_RETURN dat_status;

	/* Allocate EVENTs */
	events = (DAT_EVENT *) dapl_os_realloc(evd_ptr->events,
					       qlen * sizeof(DAT_EVENT));
	if (NULL == events) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	diff = events - evd_ptr->events;
	evd_ptr->events = events;

	old_qlen = evd_ptr->qlen;
	evd_ptr->qlen = qlen;

	/* reallocate free event queue */
	dat_status = dapls_rbuf_realloc(&evd_ptr->free_event_queue, qlen);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}
	dapls_rbuf_adjust(&evd_ptr->free_event_queue, diff);

	/* reallocate pending event queue */
	dat_status = dapls_rbuf_realloc(&evd_ptr->pending_event_queue, qlen);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}
	dapls_rbuf_adjust(&evd_ptr->pending_event_queue, diff);

	/*
	 * add new events to free event queue. 
	 */
	for (i = old_qlen; i < qlen; i++) {
		dapls_rbuf_add(&evd_ptr->free_event_queue, (void *)&events[i]);
	}

      bail:
	return dat_status;
}

/*
 * dapls_evd_dealloc
 *
 * Free the passed in EVD structure. If an error occurs, this function
 * will clean up all of the internal data structures and report the
 * error.
 *
 * Input:
 * 	evd_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	status
 *
 */
DAT_RETURN dapls_evd_dealloc(IN DAPL_EVD * evd_ptr)
{
	DAT_RETURN dat_status;
	DAPL_IA *ia_ptr;

	dat_status = DAT_SUCCESS;

	dapl_os_assert(evd_ptr->header.magic == DAPL_MAGIC_EVD);
	dapl_os_assert(dapl_os_atomic_read(&evd_ptr->evd_ref_count) == 0);

	/*
	 * Destroy the CQ first, to keep any more callbacks from coming
	 * up from it.
	 */
	evd_ptr->evd_enabled = DAT_FALSE;
	if (evd_ptr->ib_cq_handle != IB_INVALID_HANDLE) {
		ia_ptr = evd_ptr->header.owner_ia;

		dat_status = dapls_ib_cq_free(ia_ptr, evd_ptr);
		if (dat_status != DAT_SUCCESS) {
			goto bail;
		}
	}

	/*
	 * We should now be safe to invalidate the EVD; reset the
	 * magic to prevent reuse.
	 */
	evd_ptr->header.magic = DAPL_MAGIC_INVALID;

	/* Release reference on the CNO if it exists */
	if (evd_ptr->cno_ptr != NULL) {
		dapl_os_atomic_dec(&evd_ptr->cno_ptr->cno_ref_count);
		evd_ptr->cno_ptr = NULL;
	}

	/* If the ring buffer allocation failed, then the dapls_rbuf_destroy   */
	/* function will detect that the ring buffer's internal data (ex. base */
	/* pointer) are invalid and will handle the situation appropriately    */
	dapls_rbuf_destroy(&evd_ptr->free_event_queue);
	dapls_rbuf_destroy(&evd_ptr->pending_event_queue);

	if (evd_ptr->events) {
		dapl_os_free(evd_ptr->events,
			     evd_ptr->qlen * sizeof(DAT_EVENT));
	}

	dapl_os_wait_object_destroy(&evd_ptr->wait_object);

#ifdef DAPL_COUNTERS
	dapl_os_free(evd_ptr->cntrs,
		     sizeof(DAT_UINT64) * DCNT_EVD_ALL_COUNTERS);
#endif				/* DAPL_COUNTERS */

	dapl_os_free(evd_ptr, sizeof(DAPL_EVD));

      bail:
	return dat_status;
}

STATIC _INLINE_ char *DAPL_GET_DTO_OP_STR(int op)
{
	static char *dto_ops[] = {
		"OP_SEND",
		"OP_RECEIVE",
		"OP_RDMA_WRITE",
		"OP_RDMA_READ"
	};
	return ((op < 0 || op > 3) ? "Invalid DTO OP?" : dto_ops[op]);
}

#if !defined(DAPL_GET_CQE_OP_STR)
#define DAPL_GET_CQE_OP_STR(e) "Unknown CEQ OP String?"
#endif
#if !defined(DAPL_GET_CQE_VENDOR_ERR)
#define DAPL_GET_CQE_VENDOR_ERR(e) 0
#endif

/*
 * dapli_evd_eh_print_cqe
 *
 * Input:
 *	cqe_ptr
 *
 * Output:
 *	none
 *
 * Prints out a CQE for debug purposes
 *
 */

void dapli_evd_eh_print_cqe(IN ib_work_completion_t * cqe_ptr)
{
#ifdef DAPL_DBG
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "\t >>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<\n");
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "\t dapl_evd_dto_callback : CQE \n");
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "\t\t work_req_id %lli\n", DAPL_GET_CQE_WRID(cqe_ptr));
	if (DAPL_GET_CQE_STATUS(cqe_ptr) == 0) {
		dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
			     "\t\t op_type: %s\n",
			     DAPL_GET_CQE_OP_STR(cqe_ptr));
		dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
			     "\t\t bytes_num %d\n",
			     DAPL_GET_CQE_BYTESNUM(cqe_ptr));
	}
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "\t\t status %d vendor_err 0x%x\n",
		     DAPL_GET_CQE_STATUS(cqe_ptr),
		     DAPL_GET_CQE_VENDOR_ERR(cqe_ptr));
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "\t >>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<\n");
#endif
	return;
}

/*
 * Event posting code follows.
 */

/*
 * These next two functions (dapli_evd_get_event and dapli_evd_post_event)
 * are a pair.  They are always called together, from one of the functions
 * at the end of this file (dapl_evd_post_*_event).
 *
 * Note that if producer side locking is enabled, the first one takes the
 * EVD lock and the second releases it.
 */

/* dapli_evd_get_event
 *
 * Get an event struct from the evd.  The caller should fill in the event
 * and call dapl_evd_post_event.
 *
 * If there are no events available, an overflow event is generated to the
 * async EVD handler.
 *
 * If this EVD required producer locking, a successful return implies
 * that the lock is held.
 *
 * Input:
 * 	evd_ptr
 *
 * Output:
 *	event
 *
 */

static DAT_EVENT *dapli_evd_get_event(DAPL_EVD * evd_ptr)
{
	DAT_EVENT *event;

	if (evd_ptr->evd_producer_locking_needed) {
		dapl_os_lock(&evd_ptr->header.lock);
	}

	event = (DAT_EVENT *) dapls_rbuf_remove(&evd_ptr->free_event_queue);

	/* Release the lock if it was taken and the call failed.  */
	if (!event && evd_ptr->evd_producer_locking_needed) {
		dapl_os_unlock(&evd_ptr->header.lock);
	}

	return event;
}

/* dapli_evd_post_event
 *
 * Post the <event> to the evd.  If possible, invoke the evd's CNO.
 * Otherwise post the event on the pending queue.
 *
 * If producer side locking is required, the EVD lock must be held upon
 * entry to this function.
 *
 * Input:
 * 	evd_ptr
 * 	event
 *
 * Output:
 *	none
 *
 */

static void
dapli_evd_post_event(IN DAPL_EVD * evd_ptr, IN const DAT_EVENT * event_ptr)
{
	DAT_RETURN dat_status;
	DAPL_CNO *cno_to_trigger = NULL;

	dapl_dbg_log(DAPL_DBG_TYPE_EVD, "%s: %s evd %p state %d\n",
		     __FUNCTION__, dapl_event_str(event_ptr->event_number), 
		     evd_ptr, evd_ptr->evd_state);

	dat_status = dapls_rbuf_add(&evd_ptr->pending_event_queue,
				    (void *)event_ptr);
	dapl_os_assert(dat_status == DAT_SUCCESS);

	dapl_os_assert(evd_ptr->evd_state == DAPL_EVD_STATE_WAITED
		       || evd_ptr->evd_state == DAPL_EVD_STATE_OPEN);

	if (evd_ptr->evd_state == DAPL_EVD_STATE_OPEN) {
		/* No waiter.  Arrange to trigger a CNO if it exists.  */

		if (evd_ptr->evd_enabled) {
			cno_to_trigger = evd_ptr->cno_ptr;
		}
		if (evd_ptr->evd_producer_locking_needed) {
			dapl_os_unlock(&evd_ptr->header.lock);
		}
	} else {
		/*
		 * We're in DAPL_EVD_STATE_WAITED.  Take the lock if
		 * we don't have it, recheck, and signal.
		 */
		if (!evd_ptr->evd_producer_locking_needed) {
			dapl_os_lock(&evd_ptr->header.lock);
		}

		if (evd_ptr->evd_state == DAPL_EVD_STATE_WAITED
		    && (dapls_rbuf_count(&evd_ptr->pending_event_queue)
			>= evd_ptr->threshold)) {
			dapl_os_unlock(&evd_ptr->header.lock);

			if (evd_ptr->evd_flags & (DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG)) {
				dapls_evd_dto_wakeup(evd_ptr);
			} else {
				dapl_os_wait_object_wakeup(&evd_ptr->wait_object);
			}

		} else {
			dapl_os_unlock(&evd_ptr->header.lock);
		}
	}

	if (cno_to_trigger != NULL) {
		dapl_internal_cno_trigger(cno_to_trigger, evd_ptr);
	}
}

/* dapli_evd_post_event_nosignal
 *
 * Post the <event> to the evd.  Do not do any wakeup processing.
 * This function should only be called if it is known that there are
 * no waiters that it is appropriate to wakeup on this EVD.  An example
 * of such a situation is during internal dat_evd_wait() processing.
 *
 * If producer side locking is required, the EVD lock must be held upon
 * entry to this function.
 *
 * Input:
 * 	evd_ptr
 * 	event
 *
 * Output:
 *	none
 *
 */

static void
dapli_evd_post_event_nosignal(IN DAPL_EVD * evd_ptr,
			      IN const DAT_EVENT * event_ptr)
{
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_EVD, "%s: Called with event %s\n",
		     __FUNCTION__, dapl_event_str(event_ptr->event_number));

	dat_status = dapls_rbuf_add(&evd_ptr->pending_event_queue,
				    (void *)event_ptr);
	dapl_os_assert(dat_status == DAT_SUCCESS);

	dapl_os_assert(evd_ptr->evd_state == DAPL_EVD_STATE_WAITED
		       || evd_ptr->evd_state == DAPL_EVD_STATE_OPEN);

	if (evd_ptr->evd_producer_locking_needed) {
		dapl_os_unlock(&evd_ptr->header.lock);
	}
}

/* dapli_evd_format_overflow_event
 *
 * format an overflow event for posting
 *
 * Input:
 * 	evd_ptr
 * 	event_ptr
 *
 * Output:
 *	none
 *
 */
static void
dapli_evd_format_overflow_event(IN DAPL_EVD * evd_ptr,
				OUT DAT_EVENT * event_ptr)
{
	DAPL_IA *ia_ptr;

	ia_ptr = evd_ptr->header.owner_ia;

	event_ptr->evd_handle = (DAT_EVD_HANDLE) evd_ptr;
	event_ptr->event_number = DAT_ASYNC_ERROR_EVD_OVERFLOW;
	event_ptr->event_data.asynch_error_event_data.dat_handle =
	    (DAT_HANDLE) ia_ptr;
}

/* dapli_evd_post_overflow_event
 *
 * post an overflow event
 *
 * Input:
 * 	async_evd_ptr
 * 	evd_ptr
 *
 * Output:
 *	none
 *
 */
static void
dapli_evd_post_overflow_event(IN DAPL_EVD * async_evd_ptr,
			      IN DAPL_EVD * overflow_evd_ptr)
{
	DAT_EVENT *overflow_event;

	/* The overflow_evd_ptr mght be the same as evd.
	 * In that case we've got a catastrophic overflow.
	 */
	dapl_log(DAPL_DBG_TYPE_WARN,
		 " WARNING: overflow event on EVD %p/n", overflow_evd_ptr);

	if (async_evd_ptr == overflow_evd_ptr) {
		async_evd_ptr->catastrophic_overflow = DAT_TRUE;
		async_evd_ptr->evd_state = DAPL_EVD_STATE_DEAD;
		return;
	}

	overflow_event = dapli_evd_get_event(overflow_evd_ptr);
	if (!overflow_event) {
		/* this is not good */
		overflow_evd_ptr->catastrophic_overflow = DAT_TRUE;
		overflow_evd_ptr->evd_state = DAPL_EVD_STATE_DEAD;
		return;
	}
	dapli_evd_format_overflow_event(overflow_evd_ptr, overflow_event);
	dapli_evd_post_event(overflow_evd_ptr, overflow_event);

	return;
}

static DAT_EVENT *dapli_evd_get_and_init_event(IN DAPL_EVD * evd_ptr,
					       IN DAT_EVENT_NUMBER event_number)
{
	DAT_EVENT *event_ptr;

	event_ptr = dapli_evd_get_event(evd_ptr);
	if (NULL == event_ptr) {
		dapli_evd_post_overflow_event(evd_ptr->header.owner_ia->
					      async_error_evd, evd_ptr);
	} else {
		event_ptr->evd_handle = (DAT_EVD_HANDLE) evd_ptr;
		event_ptr->event_number = event_number;
	}

	return event_ptr;
}

DAT_RETURN
dapls_evd_post_cr_arrival_event(IN DAPL_EVD * evd_ptr,
				IN DAT_EVENT_NUMBER event_number,
				IN DAT_SP_HANDLE sp_handle,
				DAT_IA_ADDRESS_PTR ia_address_ptr,
				DAT_CONN_QUAL conn_qual,
				DAT_CR_HANDLE cr_handle)
{
	DAT_EVENT *event_ptr;
	event_ptr = dapli_evd_get_and_init_event(evd_ptr, event_number);
	/*
	 * Note event lock may be held on successful return
	 * to be released by dapli_evd_post_event(), if provider side locking
	 * is needed.
	 */

	if (event_ptr == NULL) {
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);
	}

	event_ptr->event_data.cr_arrival_event_data.sp_handle = sp_handle;
	event_ptr->event_data.cr_arrival_event_data.local_ia_address_ptr
	    = ia_address_ptr;
	event_ptr->event_data.cr_arrival_event_data.conn_qual = conn_qual;
	event_ptr->event_data.cr_arrival_event_data.cr_handle = cr_handle;

	dapli_evd_post_event(evd_ptr, event_ptr);

	return DAT_SUCCESS;
}

DAT_RETURN
dapls_evd_post_connection_event(IN DAPL_EVD * evd_ptr,
				IN DAT_EVENT_NUMBER event_number,
				IN DAT_EP_HANDLE ep_handle,
				IN DAT_COUNT private_data_size,
				IN DAT_PVOID private_data)
{
	DAT_EVENT *event_ptr;
	event_ptr = dapli_evd_get_and_init_event(evd_ptr, event_number);
	/*
	 * Note event lock may be held on successful return
	 * to be released by dapli_evd_post_event(), if provider side locking
	 * is needed.
	 */

	if (event_ptr == NULL) {
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);
	}

	event_ptr->event_data.connect_event_data.ep_handle = ep_handle;
	event_ptr->event_data.connect_event_data.private_data_size
	    = private_data_size;
	event_ptr->event_data.connect_event_data.private_data = private_data;

	dapli_evd_post_event(evd_ptr, event_ptr);

	return DAT_SUCCESS;
}

DAT_RETURN
dapls_evd_post_async_error_event(IN DAPL_EVD * evd_ptr,
				 IN DAT_EVENT_NUMBER event_number,
				 IN DAT_IA_HANDLE ia_handle)
{
	DAT_EVENT *event_ptr;
	event_ptr = dapli_evd_get_and_init_event(evd_ptr, event_number);
	/*
	 * Note event lock may be held on successful return
	 * to be released by dapli_evd_post_event(), if provider side locking
	 * is needed.
	 */
	dapl_log(DAPL_DBG_TYPE_WARN,
		 " WARNING: async event - %s evd=%p/n",
		 dapl_event_str(event_number), evd_ptr);

	if (event_ptr == NULL) {
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);
	}

	event_ptr->event_data.asynch_error_event_data.dat_handle =
	    (DAT_HANDLE) ia_handle;

	dapli_evd_post_event(evd_ptr, event_ptr);

	return DAT_SUCCESS;
}

DAT_RETURN
dapls_evd_post_software_event(IN DAPL_EVD * evd_ptr,
			      IN DAT_EVENT_NUMBER event_number,
			      IN DAT_PVOID pointer)
{
	DAT_EVENT *event_ptr;
	event_ptr = dapli_evd_get_and_init_event(evd_ptr, event_number);
	/*
	 * Note event lock may be held on successful return
	 * to be released by dapli_evd_post_event(), if provider side locking
	 * is needed.
	 */

	if (event_ptr == NULL) {
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);
	}

	event_ptr->event_data.software_event_data.pointer = pointer;

	dapli_evd_post_event(evd_ptr, event_ptr);

	return DAT_SUCCESS;
}

/*
 * dapls_evd_post_generic_event
 *
 * Post a generic event type. Not used by all providers
 *
 * Input:
 *	evd_ptr
 * 	event_number
 *	data
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *
 */
DAT_RETURN
dapls_evd_post_generic_event(IN DAPL_EVD * evd_ptr,
			     IN DAT_EVENT_NUMBER event_number,
			     IN DAT_EVENT_DATA * data)
{
	DAT_EVENT *event_ptr;

	event_ptr = dapli_evd_get_and_init_event(evd_ptr, event_number);
	/*
	 * Note event lock may be held on successful return
	 * to be released by dapli_evd_post_event(), if provider side locking
	 * is needed.
	 */

	if (event_ptr == NULL) {
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);
	}

	event_ptr->event_data = *data;

	dapli_evd_post_event(evd_ptr, event_ptr);

	return DAT_SUCCESS;
}

#ifdef DAT_EXTENSIONS
DAT_RETURN
dapls_evd_post_cr_event_ext(IN DAPL_SP * sp_ptr,
			    IN DAT_EVENT_NUMBER event_number,
			    IN dp_ib_cm_handle_t ib_cm_handle,
			    IN DAT_COUNT p_size,
			    IN DAT_PVOID p_data, IN DAT_PVOID ext_data)
{
	DAPL_CR *cr_ptr;
	DAPL_EP *ep_ptr;
	DAT_EVENT *event_ptr;
	DAT_SP_HANDLE sp_handle;

	dapl_os_lock(&sp_ptr->header.lock);
	if (sp_ptr->listening == DAT_FALSE) {
		dapl_os_unlock(&sp_ptr->header.lock);
		dapl_dbg_log(DAPL_DBG_TYPE_CM,
			     "---> post_cr_event_ext: conn event on down SP\n");
		(void)dapls_ib_reject_connection(ib_cm_handle,
						 DAT_CONNECTION_EVENT_UNREACHABLE,
						 0, NULL);
		return DAT_CONN_QUAL_UNAVAILABLE;
	}

	/*
	 * RSP connections only allow a single connection. Close
	 * it down NOW so we reject any further connections.
	 */
	if (sp_ptr->header.handle_type == DAT_HANDLE_TYPE_RSP)
		sp_ptr->listening = DAT_FALSE;

	dapl_os_unlock(&sp_ptr->header.lock);

	/* allocate new connect request */
	cr_ptr = dapls_cr_alloc(sp_ptr->header.owner_ia);
	if (cr_ptr == NULL)
		return DAT_INSUFFICIENT_RESOURCES;

	/* Set up the CR */
	cr_ptr->sp_ptr = sp_ptr;	/* maintain sp_ptr in case of reject */
	cr_ptr->param.remote_port_qual = 0;
	cr_ptr->ib_cm_handle = ib_cm_handle;
	cr_ptr->param.remote_ia_address_ptr =
	    (DAT_IA_ADDRESS_PTR) & cr_ptr->remote_ia_address;

	/*
	 * Copy the remote address and private data out of the private_data
	 */
	cr_ptr->param.private_data = cr_ptr->private_data;
	cr_ptr->param.private_data_size = p_size;
	if (p_size)
		dapl_os_memcpy(cr_ptr->private_data, p_data, p_size);

	/* EP will be NULL unless RSP service point */
	ep_ptr = (DAPL_EP *) sp_ptr->ep_handle;

	if (sp_ptr->psp_flags == DAT_PSP_PROVIDER_FLAG) {
		DAPL_IA *ia_ptr;
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

	/* assign sp_ptr to union to avoid typecast errors from some compilers */
	sp_handle.psp_handle = (DAT_PSP_HANDLE) sp_ptr;

	/* Post the event.  */

	/*
	 * Note event lock may be held on successful return
	 * to be released by dapli_evd_post_event(), if provider side locking
	 * is needed.
	 */
	event_ptr = dapli_evd_get_and_init_event(sp_ptr->evd_handle,
						 event_number);
	if (event_ptr == NULL)
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);

	event_ptr->event_data.cr_arrival_event_data.sp_handle = sp_handle;
	event_ptr->event_data.cr_arrival_event_data.local_ia_address_ptr =
	    (DAT_IA_ADDRESS_PTR) & sp_ptr->header.owner_ia->hca_ptr->
	    hca_address;
	event_ptr->event_data.cr_arrival_event_data.conn_qual =
	    sp_ptr->conn_qual;
	event_ptr->event_data.cr_arrival_event_data.cr_handle =
	    (DAT_HANDLE) cr_ptr;

	dapl_os_memcpy(&event_ptr->event_extension_data[0], ext_data, 64);

	dapli_evd_post_event(sp_ptr->evd_handle, event_ptr);

	return DAT_SUCCESS;
}

DAT_RETURN
dapls_evd_post_connection_event_ext(IN DAPL_EVD * evd_ptr,
				    IN DAT_EVENT_NUMBER event_number,
				    IN DAT_EP_HANDLE ep_handle,
				    IN DAT_COUNT private_data_size,
				    IN DAT_PVOID private_data,
				    IN DAT_PVOID ext_data)
{
	DAT_EVENT *event_ptr;
	event_ptr = dapli_evd_get_and_init_event(evd_ptr, event_number);
	/*
	 * Note event lock may be held on successful return
	 * to be released by dapli_evd_post_event(), if provider side locking
	 * is needed.
	 */
	if (event_ptr == NULL)
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);

	event_ptr->event_data.connect_event_data.ep_handle = ep_handle;
	event_ptr->event_data.connect_event_data.private_data_size
	    = private_data_size;
	event_ptr->event_data.connect_event_data.private_data = private_data;

	dapl_os_memcpy(&event_ptr->event_extension_data[0], ext_data, 64);

	dapli_evd_post_event(evd_ptr, event_ptr);

	return DAT_SUCCESS;
}
#endif

/*
 * dapli_evd_cqe_to_event
 *
 * Convert a CQE into an event structure.
 *
 * Input:
 *	evd_ptr
 * 	cqe_ptr
 *
 * Output:
 * 	event_ptr
 *
 * Returns:
 * 	none
 *
 */
static void
dapli_evd_cqe_to_event(IN DAPL_EVD * evd_ptr,
		       IN void *cqe_ptr, OUT DAT_EVENT * event_ptr)
{
	DAPL_EP *ep_ptr;
	DAPL_COOKIE *cookie;
	DAT_DTO_COMPLETION_STATUS dto_status;
	DAPL_COOKIE_BUFFER *buffer;

	/*
	 * All that can be relied on if the status is bad is the status
	 * and WRID.
	 */
	dto_status = dapls_ib_get_dto_status(cqe_ptr);

	cookie = (DAPL_COOKIE *) (uintptr_t) DAPL_GET_CQE_WRID(cqe_ptr);
	dapl_os_assert((NULL != cookie));

	ep_ptr = cookie->ep;
	dapl_os_assert((NULL != ep_ptr));
	if (ep_ptr->header.magic != DAPL_MAGIC_EP) {
		/* ep may have been freed, just return */
		return;
	}

	dapls_io_trc_update_completion(ep_ptr, cookie, dto_status);

	event_ptr->evd_handle = (DAT_EVD_HANDLE) evd_ptr;

	switch (cookie->type) {
	case DAPL_COOKIE_TYPE_DTO:
		{
#ifdef DAT_EXTENSIONS
			/* Extended via request post or message receive */
			if ((cookie->val.dto.type == DAPL_DTO_TYPE_EXTENSION) ||
			    (cookie->val.dto.type == DAPL_DTO_TYPE_RECV &&
			     DAPL_GET_CQE_OPTYPE(cqe_ptr) != OP_RECEIVE)) {
				dapls_cqe_to_event_extension(ep_ptr, cookie,
							     cqe_ptr,
							     event_ptr);
				if (cookie->val.dto.type == DAPL_DTO_TYPE_RECV)
					dapls_cookie_dealloc(&ep_ptr->
							     recv_buffer,
							     cookie);
				else
					dapls_cookie_dealloc(&ep_ptr->
							     req_buffer,
							     cookie);
				break;
			}
#endif

			if (DAPL_DTO_TYPE_RECV == cookie->val.dto.type)
				buffer = &ep_ptr->recv_buffer;
			else
				buffer = &ep_ptr->req_buffer;

			event_ptr->event_number = DAT_DTO_COMPLETION_EVENT;
			event_ptr->event_data.dto_completion_event_data.
			    ep_handle = cookie->ep;
			event_ptr->event_data.dto_completion_event_data.
			    user_cookie = cookie->val.dto.cookie;
			event_ptr->event_data.dto_completion_event_data.status =
			    dto_status;

			if (cookie->val.dto.type == DAPL_DTO_TYPE_SEND ||
			    cookie->val.dto.type == DAPL_DTO_TYPE_RDMA_WRITE) {
				/* Get size from DTO; CQE value may be off.  */
				event_ptr->event_data.dto_completion_event_data.
				    transfered_length = cookie->val.dto.size;
			} else {
				event_ptr->event_data.dto_completion_event_data.
				    transfered_length =
				    DAPL_GET_CQE_BYTESNUM(cqe_ptr);
			}

			dapls_cookie_dealloc(buffer, cookie);
			break;
		}

	case DAPL_COOKIE_TYPE_RMR:
		{
			event_ptr->event_number = DAT_RMR_BIND_COMPLETION_EVENT;

			event_ptr->event_data.rmr_completion_event_data.
			    rmr_handle = cookie->val.rmr.rmr;
			event_ptr->event_data.rmr_completion_event_data.
			    user_cookie = cookie->val.rmr.cookie;
			if (dto_status == DAT_DTO_SUCCESS) {
				event_ptr->event_data.rmr_completion_event_data.
				    status = DAT_RMR_BIND_SUCCESS;
				dapl_os_assert((DAPL_GET_CQE_OPTYPE(cqe_ptr)) ==
					       OP_BIND_MW);
			} else {
				dapl_dbg_log(DAPL_DBG_TYPE_DTO_COMP_ERR,
					     " MW bind completion ERROR: %d: op %#x ep: %p\n",
					     dto_status,
					     DAPL_GET_CQE_OPTYPE(cqe_ptr),
					     ep_ptr);
				event_ptr->event_data.rmr_completion_event_data.
				    status = DAT_RMR_OPERATION_FAILED;
				dapl_os_atomic_dec(&cookie->val.rmr.rmr->lmr->
						   lmr_ref_count);
			}

			dapls_cookie_dealloc(&ep_ptr->req_buffer, cookie);
			break;
		}
	default:
		{
			dapl_os_assert(!"Invalid Operation type");
			break;
		}
	}			/* end switch */

	/*
	 * Most error DTO ops result in disconnecting the EP. See
	 * IBTA Vol 1.1, Chapter 10,Table 68, for expected effect on
	 * state.
	 */
	if ((dto_status != DAT_DTO_SUCCESS) &&
	    (dto_status != DAT_DTO_ERR_FLUSHED)) {
		DAPL_EVD *evd_ptr;

		/*
		 * If we are connected, generate disconnect and generate an
		 * event. We may be racing with other disconnect ops, so we
		 * need to check. We may also be racing CM connection events,
		 * requiring us to check for connection pending states too.
		 */
		dapl_os_lock(&ep_ptr->header.lock);
		if (ep_ptr->param.ep_state == DAT_EP_STATE_CONNECTED ||
		    ep_ptr->param.ep_state ==
		    DAT_EP_STATE_ACTIVE_CONNECTION_PENDING
		    || ep_ptr->param.ep_state ==
		    DAT_EP_STATE_PASSIVE_CONNECTION_PENDING
		    || ep_ptr->param.ep_state ==
		    DAT_EP_STATE_COMPLETION_PENDING)
		{
			ep_ptr->param.ep_state = DAT_EP_STATE_DISCONNECTED;
			dapl_os_unlock(&ep_ptr->header.lock);
			dapls_io_trc_dump(ep_ptr, cqe_ptr, dto_status);

			/* Let the other side know we have disconnected */
			(void)dapls_ib_disconnect(ep_ptr,
						  DAT_CLOSE_ABRUPT_FLAG);

			/* ... and clean up the local side */
			evd_ptr = (DAPL_EVD *) ep_ptr->param.connect_evd_handle;
			if (evd_ptr != NULL) {
				dapls_evd_post_connection_event(evd_ptr,
								DAT_CONNECTION_EVENT_BROKEN,
								(DAT_HANDLE)
								ep_ptr, 0, 0);
			}
		} else {
			dapl_os_unlock(&ep_ptr->header.lock);
		}

		dapl_log(DAPL_DBG_TYPE_ERR,
			 "DTO completion ERR: status %d, op %s, vendor_err 0x%x - %s\n",
			 DAPL_GET_CQE_STATUS(cqe_ptr),
			 DAPL_GET_DTO_OP_STR(cookie->val.dto.type),
			 DAPL_GET_CQE_VENDOR_ERR(cqe_ptr),
			 inet_ntoa(((struct sockaddr_in *)&ep_ptr->
				    remote_ia_address)->sin_addr));
	}
}

/*
 * dapls_evd_copy_cq
 *
 * Copy all entries on a CQ associated with the EVD onto that EVD
 * Up to caller to handle races, if any.  Note that no EVD waiters will
 * be awoken by this copy.
 *
 * Input:
 *	evd_ptr
 *
 * Output:
 * 	None
 *
 * Returns:
 * 	none
 *
 */
void dapls_evd_copy_cq(DAPL_EVD * evd_ptr)
{
	ib_work_completion_t cur_cqe;
	DAT_RETURN dat_status;
	DAT_EVENT *event;

	if (evd_ptr->ib_cq_handle == IB_INVALID_HANDLE) {
		/* Nothing to do if no CQ.  */
		return;
	}

	while (1) {
		dat_status =
		    dapls_ib_completion_poll(evd_ptr->header.owner_ia->hca_ptr,
					     evd_ptr, &cur_cqe);

		if (dat_status != DAT_SUCCESS) {
			break;
		}

		/* For debugging.  */
		dapli_evd_eh_print_cqe(&cur_cqe);

		/*
		 * Can use DAT_DTO_COMPLETION_EVENT because dapli_evd_cqe_to_event
		 * will overwrite.
		 */

		event =
		    dapli_evd_get_and_init_event(evd_ptr,
						 DAT_DTO_COMPLETION_EVENT);
		if (event == NULL) {
			/* We've already attempted the overflow post; return.  */
			return;
		}

		dapli_evd_cqe_to_event(evd_ptr, &cur_cqe, event);

		dapli_evd_post_event_nosignal(evd_ptr, event);
	}

	if (DAT_GET_TYPE(dat_status) != DAT_QUEUE_EMPTY) {
		dapl_dbg_log(DAPL_DBG_TYPE_EVD,
			     "dapls_evd_copy_cq: dapls_ib_completion_poll returned 0x%x\n",
			     dat_status);
		dapl_os_assert(!"Bad return from dapls_ib_completion_poll");
	}
}

/*
 * dapls_evd_cq_poll_to_event
 *
 * Attempt to dequeue a single CQE from a CQ and turn it into
 * an event.
 *
 * Input:
 *	evd_ptr
 *
 * Output:
 * 	event
 *
 * Returns:
 * 	Status of operation
 *
 */
DAT_RETURN
dapls_evd_cq_poll_to_event(IN DAPL_EVD * evd_ptr, OUT DAT_EVENT * event)
{
	DAT_RETURN dat_status;
	ib_work_completion_t cur_cqe;

	dat_status = dapls_ib_completion_poll(evd_ptr->header.owner_ia->hca_ptr,
					      evd_ptr, &cur_cqe);
	if (dat_status == DAT_SUCCESS) {
		/* For debugging.  */
		dapli_evd_eh_print_cqe(&cur_cqe);

		dapli_evd_cqe_to_event(evd_ptr, &cur_cqe, event);
	}

	return dat_status;
}

#ifdef DAPL_DBG_IO_TRC
/*
 * Update I/O completions in the I/O trace buffer. I/O is posted to
 * the buffer, then we find it here using the cookie and mark it
 * completed with the completion status
 */
void
dapls_io_trc_update_completion(DAPL_EP * ep_ptr,
			       DAPL_COOKIE * cookie,
			       DAT_DTO_COMPLETION_STATUS dto_status)
{
	int i;
	static unsigned int c_cnt = 1;

	for (i = 0; i < DBG_IO_TRC_QLEN; i++) {
		if (ep_ptr->ibt_base[i].cookie == cookie) {
			ep_ptr->ibt_base[i].status = dto_status;
			ep_ptr->ibt_base[i].done = c_cnt++;
		}
	}
}

/*
 * Dump the I/O trace buffers
 */
void
dapls_io_trc_dump(DAPL_EP * ep_ptr,
		  void *cqe_ptr, DAT_DTO_COMPLETION_STATUS dto_status)
{
	struct io_buf_track *ibt;
	int i;
	int cnt;

	dapl_os_printf("DISCONNECTING: dto_status     = %x\n", dto_status);
	dapl_os_printf("               OpType        = %x\n",
		       DAPL_GET_CQE_OPTYPE(cqe_ptr));
	dapl_os_printf("               Bytes         = %x\n",
		       DAPL_GET_CQE_BYTESNUM(cqe_ptr));
	dapl_os_printf("               WRID (cookie) = %llx\n",
		       DAPL_GET_CQE_WRID(cqe_ptr));

	if (ep_ptr->ibt_dumped == 0) {

		dapl_os_printf("EP %p (qpn %d) I/O trace buffer\n",
			       ep_ptr, ep_ptr->qpn);

		ep_ptr->ibt_dumped = 1;
		ibt =
		    (struct io_buf_track *)dapls_rbuf_remove(&ep_ptr->
							     ibt_queue);
		cnt = DBG_IO_TRC_QLEN;
		while (ibt != NULL && cnt > 0) {
			dapl_os_printf
			    ("%2d. %3s (%2d, %d) OP: %x cookie %p wqe %p rmv_target_addr %llx rmv_rmr_context %x\n",
			     cnt, ibt->done == 0 ? "WRK" : "DON", ibt->status,
			     ibt->done, ibt->op_type, ibt->cookie, ibt->wqe,
			     ibt->remote_iov.target_address,
			     ibt->remote_iov.rmr_context);
			for (i = 0; i < 3; i++) {
				if (ibt->iov[i].segment_length != 0) {
					dapl_os_printf
					    ("     (%4llx, %8x, %8llx)\n",
					     ibt->iov[i].segment_length,
					     ibt->iov[i].lmr_context,
					     ibt->iov[i].virtual_address);
				}
			}
			ibt =
			    (struct io_buf_track *)dapls_rbuf_remove(&ep_ptr->
								     ibt_queue);
			cnt--;
		}
	}
}
#endif				/* DAPL_DBG_IO_TRC */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
