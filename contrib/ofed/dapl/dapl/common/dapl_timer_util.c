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
 * MODULE: dapl_timer_util.c
 *
 * PURPOSE: DAPL timer management
 * Description: Routines to add and cancel timer records. A timer record
 *		is put on the global timer queue. If the timer thread is
 *		not running, start it. The timer thread will sleep
 *		until a timer event or until a process wakes it up
 *		to notice a new timer is available; we use a DAPL_WAIT_OBJ
 *		for synchronization.
 *
 *		If a timer is cancelled, it is simlpy removed from the
 *		queue. The timer may wakeup and notice there is no timer
 *		record to awaken at this time, so it will reset for the
 *		next entry. When there are no timer records to manage,
 *		the timer thread just sleeps until awakened.
 *
 *		This file also contains the timer handler thread,
 *		embodied in dapls_timer_thread().
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_timer_util.h"

#define DAPL_TIMER_INIT    0
#define DAPL_TIMER_RUN     1
#define DAPL_TIMER_DESTROY 2
#define DAPL_TIMER_EXIT    3

struct timer_head {
	DAPL_LLIST_HEAD timer_list_head;
	DAPL_OS_LOCK lock;
	DAPL_OS_WAIT_OBJECT wait_object;
	DAPL_OS_THREAD timeout_thread_handle;
	int state;
} g_daplTimerHead;

typedef struct timer_head DAPL_TIMER_HEAD;

void dapls_timer_thread(void *arg);

void dapls_timer_init()
{
	/*
	 * Set up the timer thread elements. The timer thread isn't
	 * started until it is actually needed
	 */
	g_daplTimerHead.timer_list_head = NULL;
	dapl_os_lock_init(&g_daplTimerHead.lock);
	dapl_os_wait_object_init(&g_daplTimerHead.wait_object);
	g_daplTimerHead.timeout_thread_handle = 0;
	g_daplTimerHead.state = DAPL_TIMER_INIT;
}

void dapls_timer_release()
{
	dapl_os_lock(&g_daplTimerHead.lock);
	if (g_daplTimerHead.state != DAPL_TIMER_RUN) {
		dapl_os_unlock(&g_daplTimerHead.lock);
		return;
	}

	g_daplTimerHead.state = DAPL_TIMER_DESTROY;
	dapl_os_unlock(&g_daplTimerHead.lock);
	while (g_daplTimerHead.state != DAPL_TIMER_EXIT) {
		dapl_os_wait_object_wakeup(&g_daplTimerHead.wait_object);
		dapl_os_sleep_usec(2000);
	}
}

/*
 * dapls_timer_set
 *
 * Set a timer. The timer will invoke the specified function
 * after a number of useconds expires.
 *
 * Input:
 *      timer    User provided timer structure
 *      func     Function to invoke when timer expires
 *      data     Argument passed to func()
 *      expires  microseconds until timer fires
 *
 * Returns:
 *	no return value
 *
 */
DAT_RETURN
dapls_timer_set(IN DAPL_OS_TIMER * timer,
		IN void (*func) (uintptr_t),
		IN void *data, IN DAPL_OS_TIMEVAL expires)
{
	DAPL_OS_TIMER *list_ptr;
	DAPL_OS_TIMEVAL cur_time;
	DAT_BOOLEAN wakeup_tmo_thread;

	/*
	 * Start the timer thread the first time we need a timer
	 */
	if (g_daplTimerHead.timeout_thread_handle == 0) {
		dapl_os_thread_create(dapls_timer_thread,
				      &g_daplTimerHead,
				      &g_daplTimerHead.timeout_thread_handle);
		
		while (g_daplTimerHead.state != DAPL_TIMER_RUN) 
			dapl_os_sleep_usec(2000);
	}

	dapl_llist_init_entry(&timer->list_entry);
	wakeup_tmo_thread = DAT_FALSE;
	dapl_os_get_time(&cur_time);
	timer->expires = cur_time + expires;	/* calculate future time */
	timer->function = func;
	timer->data = data;

	/*
	 * Put the element on the queue: sorted by wakeup time, eariliest
	 * first.
	 */
	dapl_os_lock(&g_daplTimerHead.lock);

	if (g_daplTimerHead.state != DAPL_TIMER_RUN) {
		dapl_os_unlock(&g_daplTimerHead.lock);
		return DAT_INVALID_STATE;
	}

	/*
	 * Deal with 3 cases due to our list structure:
	 * 1) list is empty: become the list head
	 * 2) New timer is sooner than list head: become the list head
	 * 3) otherwise, sort the timer into the list, no need to wake
	 *    the timer thread up
	 */
	if (dapl_llist_is_empty(&g_daplTimerHead.timer_list_head)) {
		/* Case 1: add entry to head of list */
		dapl_llist_add_head(&g_daplTimerHead.timer_list_head,
				    (DAPL_LLIST_ENTRY *) & timer->list_entry,
				    timer);
		wakeup_tmo_thread = DAT_TRUE;
	} else {
		list_ptr = (DAPL_OS_TIMER *)
		    dapl_llist_peek_head(&g_daplTimerHead.timer_list_head);

		if (timer->expires < list_ptr->expires) {
			/* Case 2: add entry to head of list */
			dapl_llist_add_head(&g_daplTimerHead.timer_list_head,
					    (DAPL_LLIST_ENTRY *) & timer->
					    list_entry, timer);
			wakeup_tmo_thread = DAT_TRUE;
		} else {
			/* Case 3: figure out where entry goes in sorted list */
			list_ptr =
			    dapl_llist_next_entry(&g_daplTimerHead.
						  timer_list_head,
						  (DAPL_LLIST_ENTRY *) &
						  list_ptr->list_entry);

			while (list_ptr != NULL) {
				if (timer->expires < list_ptr->expires) {
					dapl_llist_add_entry(&g_daplTimerHead.
							     timer_list_head,
							     (DAPL_LLIST_ENTRY
							      *) & list_ptr->
							     list_entry,
							     (DAPL_LLIST_ENTRY
							      *) & timer->
							     list_entry, timer);
					break;

				}
				list_ptr =
				    dapl_llist_next_entry(&g_daplTimerHead.
							  timer_list_head,
							  (DAPL_LLIST_ENTRY *) &
							  list_ptr->list_entry);
			}
			if (list_ptr == NULL) {
				/* entry goes to the end of the list */
				dapl_llist_add_tail(&g_daplTimerHead.
						    timer_list_head,
						    (DAPL_LLIST_ENTRY *) &
						    timer->list_entry, timer);
			}
		}

	}
	dapl_os_unlock(&g_daplTimerHead.lock);

	if (wakeup_tmo_thread == DAT_TRUE) {
		dapl_os_wait_object_wakeup(&g_daplTimerHead.wait_object);
	}

	return DAT_SUCCESS;
}

/*
 * dapls_os_timer_cancel
 *
 * Cancel a timer. Simply deletes the timer with no function invocations
 *
 * Input:
 *      timer    User provided timer structure
 *
 * Returns:
 *	no return value
 */
void dapls_timer_cancel(IN DAPL_OS_TIMER * timer)
{
	dapl_os_lock(&g_daplTimerHead.lock);
	/*
	 * make sure the entry has not been removed by another thread
	 */
	if (!dapl_llist_is_empty(&g_daplTimerHead.timer_list_head) &&
	    timer->list_entry.list_head == &g_daplTimerHead.timer_list_head) {
		dapl_llist_remove_entry(&g_daplTimerHead.timer_list_head,
					(DAPL_LLIST_ENTRY *) & timer->
					list_entry);
	}
	/*
	 * If this was the first entry on the queue we could awaken the
	 * thread and have it reset the list; but it will just wake up
	 * and find that the timer entry has been removed, then go back
	 * to sleep, so don't bother.
	 */
	dapl_os_unlock(&g_daplTimerHead.lock);
}

/*
 * dapls_timer_thread
 *
 * Core worker thread dealing with all timers. Basic algorithm:
 *	- Sleep until work shows up
 *	- Take first element of sorted timer list and wake
 *	  invoke the callback if expired
 *	- Sleep for the timeout period if not expired
 *
 * Input:
 *      timer_head    Timer head structure to manage timer lists
 *
 * Returns:
 *	no return value
 */
void dapls_timer_thread(void *arg)
{
	DAPL_OS_TIMER *list_ptr;
	DAPL_OS_TIMEVAL cur_time;
	DAT_RETURN dat_status;
	DAPL_TIMER_HEAD *timer_head;

	timer_head = arg;

	dapl_os_lock(&timer_head->lock);
	timer_head->state = DAPL_TIMER_RUN;
	dapl_os_unlock(&timer_head->lock);

	for (;;) {
		if (dapl_llist_is_empty(&timer_head->timer_list_head)) {
			dat_status =
			    dapl_os_wait_object_wait(&timer_head->wait_object,
						     DAT_TIMEOUT_INFINITE);
		}

		/*
		 * Lock policy:
		 * While this thread is accessing the timer list, it holds the
		 * lock.  Otherwise, it doesn't.
		 */
		dapl_os_lock(&timer_head->lock);
		while (!dapl_llist_is_empty(&timer_head->timer_list_head)) {
			list_ptr = (DAPL_OS_TIMER *)
			    dapl_llist_peek_head(&g_daplTimerHead.
						 timer_list_head);
			dapl_os_get_time(&cur_time);

			if (list_ptr->expires <= cur_time || 
			    timer_head->state == DAPL_TIMER_DESTROY) {

				/*
				 * Remove the entry from the list. Sort out how much
				 * time we need to sleep for the next one
				 */
				list_ptr =
				    dapl_llist_remove_head(&timer_head->
							   timer_list_head);
				dapl_os_unlock(&timer_head->lock);

				/*
				 * Invoke the user callback
				 */
				list_ptr->function((uintptr_t) list_ptr->data);
				/* timer structure was allocated by caller, we don't
				 * free it here.
				 */

				/* reacquire the lock */
				dapl_os_lock(&timer_head->lock);
			} else {
				dapl_os_unlock(&timer_head->lock);
				dat_status =
				    dapl_os_wait_object_wait(&timer_head->
							     wait_object,
							     (DAT_TIMEOUT)
							     (list_ptr->
							      expires -
							      cur_time));
				dapl_os_lock(&timer_head->lock);
			}
		}

		/* Destroy - all timers triggered and list is empty */
		if (timer_head->state == DAPL_TIMER_DESTROY) {
			timer_head->state = DAPL_TIMER_EXIT;
			dapl_os_unlock(&timer_head->lock);
			break;
		}

		/*
		 * release the lock before going back to the top to sleep
		 */
		dapl_os_unlock(&timer_head->lock);

		if (dat_status == DAT_INTERNAL_ERROR) {
			/*
			 * XXX What do we do here?
			 */
		}
	}			/* for (;;) */
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
