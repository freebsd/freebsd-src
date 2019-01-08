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
 * MODULE: dapl_osd.c
 *
 * PURPOSE: Operating System Dependent layer
 * Description: 
 *	Provide OS dependent functions with a canonical DAPL
 *	interface. Designed to be portable and hide OS specific quirks
 *	of common functions.
 *		
 *
 * $Id:$
 **********************************************************************/

#include "dapl_osd.h"
#include "dapl.h"
#include "dapl_hca_util.h"
#include "dapl_ia_util.h"
#include "dapl_rmr_util.h"
#include "dapl_lmr_util.h"
#include "dapl_pz_util.h"
#include "dapl_ep_util.h"
#include "dapl_cr_util.h"
#include "dapl_evd_util.h"
#include "dapl_sp_util.h"
#include "dapl_adapter_util.h"
#include "dapl_provider.h"
#include "dapl_hash.h"
#include "dapl_timer_util.h"
#include "dapl_debug.h"

#include <sys/time.h>
#include <stdlib.h>		/* needed for getenv() */
#include <pthread.h>		/* needed for pthread_atfork() */
#include <signal.h>		/* needed for thread setup */

static void dapls_osd_fork_cleanup(void);

/*
 * dapl_osd_init
 *
 * Do Linux initialization:
 * - Set up fork handler to clean up DAPL resources in the child
 *   process after a fork().
 *
 * Input:
 *      none
 *
 * Returns:
 *	DAT_SUCCESS
 */
void dapl_os_init()
{
	int status;

	/*
	 * Set up fork control
	 */
	status = pthread_atfork(NULL, NULL, dapls_osd_fork_cleanup);
	if (status != 0) {
		dapl_os_printf("WARNING: pthread_atfork %d\n", status);
	}
}

/*
 * dapl_os_get_time
 *
 * Return 64 bit value of current time in microseconds.
 *
 * Input:
 *      loc       User location to place current time
 *
 * Returns:
 *	DAT_SUCCESS
 */

DAT_RETURN dapl_os_get_time(OUT DAPL_OS_TIMEVAL * loc)
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	*loc = ((DAT_UINT64) (tv.tv_sec) * 1000000L) + (DAT_UINT64) tv.tv_usec;

	return DAT_SUCCESS;
}

/*
 * dapl_os_get__env_bool
 *
 * Return boolean value of passed in environment variable: 1 if present,
 * 0 if not
 *
 * Input:
 *      
 *
 * Returns:
 *	TRUE or FALSE
 */
int dapl_os_get_env_bool(char *env_str)
{
	char *env_var;

	env_var = getenv(env_str);
	if (env_var != NULL) {
		return 1;
	}

	return 0;
}

/*
 * dapl_os_get_env_val
 *
 * Update val to  value of passed in environment variable if present
 *
 * Input:
 *      env_str
 *	def_val		default value if environment variable does not exist
 *
 * Returns:
 *	TRUE or FALSE
 */
int dapl_os_get_env_val(char *env_str, int def_val)
{
	char *env_var;

	env_var = getenv(env_str);
	if (env_var != NULL) {
		def_val = strtol(env_var, NULL, 0);
	}

	return def_val;
}

/*
 * Wait object routines
 */

/*
 * dapl_os_wait_object_init
 *
 * Initialize a wait object
 *
 * Input:
 *	wait_obj
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INTERNAL_ERROR
 */
DAT_RETURN dapl_os_wait_object_init(IN DAPL_OS_WAIT_OBJECT * wait_obj)
{
	wait_obj->signaled = DAT_FALSE;
	if (0 != pthread_cond_init(&wait_obj->cv, NULL)) {
		return DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}

	/* Always returns 0.  */
	pthread_mutex_init(&wait_obj->lock, NULL);

	return DAT_SUCCESS;
}

/* Wait on the supplied wait object, up to the specified time_out.
 * A timeout of DAT_TIMEOUT_INFINITE will wait indefinitely.
 * Timeout should be specified in micro seconds.
 *
 * Functional returns:
 *	DAT_SUCCESS -- another thread invoked dapl_os_wait object_wakeup
 * 	DAT_INVALID_STATE -- someone else is already waiting in this wait
 * 	object.
 *			     only one waiter is allowed at a time.
 *	DAT_ABORT -- another thread invoked dapl_os_wait_object_destroy
 *	DAT_TIMEOUT -- the specified time limit was reached.
 */

DAT_RETURN
dapl_os_wait_object_wait(IN DAPL_OS_WAIT_OBJECT * wait_obj,
			 IN DAT_TIMEOUT timeout_val)
{
	DAT_RETURN dat_status;
	int pthread_status;
	struct timespec future;

	dat_status = DAT_SUCCESS;
	pthread_status = 0;

	if (timeout_val != DAT_TIMEOUT_INFINITE) {
		struct timeval now;
		struct timezone tz;
		unsigned int microsecs;

		gettimeofday(&now, &tz);
#define USEC_PER_SEC 1000000
		microsecs = now.tv_usec + timeout_val;
		now.tv_sec = now.tv_sec + microsecs / USEC_PER_SEC;
		now.tv_usec = microsecs % USEC_PER_SEC;

		/* Convert timeval to timespec */
		future.tv_sec = now.tv_sec;
		future.tv_nsec = now.tv_usec * 1000;

		pthread_mutex_lock(&wait_obj->lock);
		while (wait_obj->signaled == DAT_FALSE && pthread_status == 0) {
			pthread_status =
			    pthread_cond_timedwait(&wait_obj->cv,
						   &wait_obj->lock, &future);

			/*
			 * No need to reset &future if we go around the loop;
			 * It's an absolute time.
			 */
		}
		/* Reset the signaled status if we were woken up.  */
		if (pthread_status == 0) {
			wait_obj->signaled = DAT_FALSE;
		}
		pthread_mutex_unlock(&wait_obj->lock);
	} else {
		pthread_mutex_lock(&wait_obj->lock);
		while (wait_obj->signaled == DAT_FALSE && pthread_status == 0) {
			pthread_status =
			    pthread_cond_wait(&wait_obj->cv, &wait_obj->lock);
		}
		/* Reset the signaled status if we were woken up.  */
		if (pthread_status == 0) {
			wait_obj->signaled = DAT_FALSE;
		}
		pthread_mutex_unlock(&wait_obj->lock);
	}

	if (ETIMEDOUT == pthread_status) {
		dat_status = DAT_ERROR(DAT_TIMEOUT_EXPIRED, 0);
	} else if (EINTR == pthread_status) {
		dat_status = DAT_ERROR(DAT_INTERRUPTED_CALL, 0);
	} else if (0 != pthread_status) {
		dat_status = DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}

	return dat_status;
}

/*
 * dapl_os_wait_object_wakeup
 *
 * Wakeup a thread waiting on a wait object
 *
 * Input:
 *      wait_obj
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INTERNAL_ERROR
 */
DAT_RETURN dapl_os_wait_object_wakeup(IN DAPL_OS_WAIT_OBJECT * wait_obj)
{
	pthread_mutex_lock(&wait_obj->lock);
	wait_obj->signaled = DAT_TRUE;
	pthread_mutex_unlock(&wait_obj->lock);
	if (0 != pthread_cond_signal(&wait_obj->cv)) {
		return DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}

	return DAT_SUCCESS;
}

/*
 * dapl_os_wait_object_destroy
 *
 * Destroy a wait object
 *
 * Input:
 *      wait_obj
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INTERNAL_ERROR
 */
DAT_RETURN dapl_os_wait_object_destroy(IN DAPL_OS_WAIT_OBJECT * wait_obj)
{
	if (0 != pthread_cond_destroy(&wait_obj->cv)) {
		return DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}
	if (0 != pthread_mutex_destroy(&wait_obj->lock)) {
		return DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}

	return DAT_SUCCESS;
}

/*
 * dapls_osd_fork_cleanup
 *
 * Update val to  value of passed in environment variable if present
 *
 * Input:
 *      env_str
 *	val		Updated if environment variable exists
 *
 * Returns:
 *	TRUE or FALSE
 */
void dapls_osd_fork_cleanup(void)
{
	DAPL_PROVIDER_LIST_NODE *cur_node;
	DAPL_HCA *hca_ptr;
	DAPL_IA *ia_ptr;
	DAPL_LMR *lmr_ptr;
	DAPL_RMR *rmr_ptr;
	DAPL_PZ *pz_ptr;
	DAPL_CR *cr_ptr;
	DAPL_EP *ep_ptr;
	DAPL_EVD *evd_ptr;
	DAT_EP_PARAM *param;
	DAPL_SP *sp_ptr;

	while (NULL != g_dapl_provider_list.head) {
		cur_node = g_dapl_provider_list.head;
		g_dapl_provider_list.head = cur_node->next;

		hca_ptr = (DAPL_HCA *) cur_node->data.extension;

		/*
		 * Walk the list of IA ptrs & clean up. This is purposely
		 * a destructive list walk, we really don't want to preserve
		 * any of it.
		 */
		while (!dapl_llist_is_empty(&hca_ptr->ia_list_head)) {
			ia_ptr = (DAPL_IA *)
			    dapl_llist_peek_head(&hca_ptr->ia_list_head);

			/*
			 * The rest of the cleanup code is similar to dapl_ia_close,
			 * the big difference is that we don't release IB resources,
			 * only memory; the underlying IB subsystem doesn't deal
			 * with fork at all, so leave IB handles alone.
			 */
			while (!dapl_llist_is_empty(&ia_ptr->rmr_list_head)) {
				rmr_ptr = (DAPL_RMR *)
				    dapl_llist_peek_head(&ia_ptr->
							 rmr_list_head);
				if (rmr_ptr->param.lmr_triplet.
				    virtual_address != 0) {
					dapl_os_atomic_dec(&rmr_ptr->lmr->
							   lmr_ref_count);
					rmr_ptr->param.lmr_triplet.
					    virtual_address = 0;
				}
				dapl_os_atomic_dec(&rmr_ptr->pz->pz_ref_count);
				dapl_ia_unlink_rmr(rmr_ptr->header.owner_ia,
						   rmr_ptr);
				dapl_rmr_dealloc(rmr_ptr);
			}

			while (!dapl_llist_is_empty(&ia_ptr->rsp_list_head)) {
				sp_ptr = (DAPL_SP *)
				    dapl_llist_peek_head(&ia_ptr->
							 rsp_list_head);
				dapl_os_atomic_dec(&
						   ((DAPL_EVD *) sp_ptr->
						    evd_handle)->evd_ref_count);
				dapls_ia_unlink_sp(ia_ptr, sp_ptr);
				dapls_sp_free_sp(sp_ptr);
			}

			while (!dapl_llist_is_empty(&ia_ptr->ep_list_head)) {
				ep_ptr = (DAPL_EP *)
				    dapl_llist_peek_head(&ia_ptr->ep_list_head);
				param = &ep_ptr->param;
				if (param->pz_handle != NULL) {
					dapl_os_atomic_dec(&
							   ((DAPL_PZ *) param->
							    pz_handle)->
							   pz_ref_count);
				}
				if (param->recv_evd_handle != NULL) {
					dapl_os_atomic_dec(&
							   ((DAPL_EVD *) param->
							    recv_evd_handle)->
							   evd_ref_count);
				}
				if (param->request_evd_handle) {
					dapl_os_atomic_dec(&
							   ((DAPL_EVD *) param->
							    request_evd_handle)->
							   evd_ref_count);
				}
				if (param->connect_evd_handle != NULL) {
					dapl_os_atomic_dec(&
							   ((DAPL_EVD *) param->
							    connect_evd_handle)->
							   evd_ref_count);
				}

				/* ...and free the resource */
				dapl_ia_unlink_ep(ia_ptr, ep_ptr);
				dapl_ep_dealloc(ep_ptr);
			}

			while (!dapl_llist_is_empty(&ia_ptr->lmr_list_head)) {
				lmr_ptr = (DAPL_LMR *)
				    dapl_llist_peek_head(&ia_ptr->
							 lmr_list_head);

				(void)dapls_hash_remove(lmr_ptr->header.
							owner_ia->hca_ptr->
							lmr_hash_table,
							lmr_ptr->param.
							lmr_context, NULL);

				pz_ptr = (DAPL_PZ *) lmr_ptr->param.pz_handle;
				dapl_os_atomic_dec(&pz_ptr->pz_ref_count);
				dapl_ia_unlink_lmr(lmr_ptr->header.owner_ia,
						   lmr_ptr);
				dapl_lmr_dealloc(lmr_ptr);
			}

			while (!dapl_llist_is_empty(&ia_ptr->psp_list_head)) {
				sp_ptr = (DAPL_SP *)
				    dapl_llist_peek_head(&ia_ptr->
							 psp_list_head);
				while (!dapl_llist_is_empty
				       (&sp_ptr->cr_list_head)) {
					cr_ptr = (DAPL_CR *)
					    dapl_llist_peek_head(&sp_ptr->
								 cr_list_head);
					dapl_sp_remove_cr(sp_ptr, cr_ptr);
					dapls_cr_free(cr_ptr);
				}

				dapls_ia_unlink_sp(ia_ptr, sp_ptr);
				dapl_os_atomic_dec(&
						   ((DAPL_EVD *) sp_ptr->
						    evd_handle)->evd_ref_count);
				dapls_sp_free_sp(sp_ptr);
			}

			while (!dapl_llist_is_empty(&ia_ptr->pz_list_head)) {
				pz_ptr = (DAPL_PZ *)
				    dapl_llist_peek_head(&ia_ptr->pz_list_head);
				dapl_ia_unlink_pz(pz_ptr->header.owner_ia,
						  pz_ptr);
				dapl_pz_dealloc(pz_ptr);
			}

			while (!dapl_llist_is_empty(&ia_ptr->evd_list_head)) {
				evd_ptr = (DAPL_EVD *)
				    dapl_llist_peek_head(&ia_ptr->
							 evd_list_head);
				dapl_ia_unlink_evd(evd_ptr->header.owner_ia,
						   evd_ptr);
				/* reset the cq_handle to avoid having it removed */
				evd_ptr->ib_cq_handle = IB_INVALID_HANDLE;
				dapls_evd_dealloc(evd_ptr);
			}

			dapl_hca_unlink_ia(ia_ptr->hca_ptr, ia_ptr);
			/* asycn error evd was taken care of above, reset the pointer */
			ia_ptr->async_error_evd = NULL;
			dapls_ia_free(ia_ptr);
		}		/* end while ( ia_ptr != NULL ) */

		dapl_os_free(cur_node, sizeof(DAPL_PROVIDER_LIST_NODE));
	}			/* end while (NULL != g_dapl_provider_list.head) */
}

/*
 * Structure to contain all elements of a thread in order to enable a
 * local routine to intercept and do some necessary initialization.
 */
struct thread_draft {
	void (*func) (void *);	/* start routine */
	void *data;		/* argument to start routine */
};

void dapli_thread_init(struct thread_draft *thread_draft);

/*
 * dapls_os_thread_create
 *
 * Create a thread for dapl
 *
 * Input:
 *	func		function to invoke thread
 *	f_arg		argument to pass to function
 *
 * Output
 *	thread_id	handle for thread
 *
 * Returns:
 *	DAT_SUCCESS
 */
DAT_RETURN
dapl_os_thread_create(IN void (*func) (void *),
		      IN void *data, OUT DAPL_OS_THREAD * thread_id)
{
	pthread_attr_t thread_attr;
	struct thread_draft *thread_draft;
	int status;

	/*
	 * Get default set of thread attributes
	 */
	status = pthread_attr_init(&thread_attr);
	if (status != 0) {
		return DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}

	/* Create dapl threads as detached from this process */
	status = pthread_attr_setdetachstate(&thread_attr,
					     PTHREAD_CREATE_DETACHED);
	if (status != 0) {
		return DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}

	thread_draft = dapl_os_alloc(sizeof(struct thread_draft));

	thread_draft->func = func;
	thread_draft->data = data;

	/* Create the thread. Observe that we first invoke a local
	 * routine to set up OS parameters, before invoking the user
	 * specified routine.
	 */
	status = pthread_create(thread_id,
				&thread_attr,
				(void *(*)(void *))dapli_thread_init,
				(void *)thread_draft);

	/* clean up resources */
	(void)pthread_attr_destroy(&thread_attr);

	if (status != 0) {
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, 0);
	}

	return DAT_SUCCESS;
}

/*
 * dapli_thread_init
 *
 * Need to mask all signals from this thread in order to be a good
 * citizen. Signals will arrive randomly and will be processed by
 * whatever thread is running unless they are specifically blocked; and
 * this should be a user thread, not a dapl thread
 */

void dapli_thread_init(struct thread_draft *thread_draft)
{
	sigset_t sigset;
	void (*func) (void *);
	void *data;

	sigfillset(&sigset);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	func = thread_draft->func;
	data = thread_draft->data;
	dapl_os_free(thread_draft, sizeof(struct thread_draft));

	(*func) (data);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
