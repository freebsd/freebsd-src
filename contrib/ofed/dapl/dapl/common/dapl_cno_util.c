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
 * MODULE: dapl_cno_util.c
 *
 * PURPOSE: Manage CNO Info structure
 *
 * $Id:$
 **********************************************************************/

#include "dapl_ia_util.h"
#include "dapl_cno_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_cno_alloc
 *
 * alloc and initialize an EVD struct
 *
 * Input:
 *	ia
 *
 * Returns:
 *	cno_ptr, or null on failure.
 */
#if defined(__KDAPL__)
DAPL_CNO *dapl_cno_alloc(IN DAPL_IA * ia_ptr,
			 IN const DAT_UPCALL_OBJECT * upcall)
#else
DAPL_CNO *dapl_cno_alloc(IN DAPL_IA * ia_ptr,
			 IN DAT_OS_WAIT_PROXY_AGENT wait_agent)
#endif				/* defined(__KDAPL__) */
{
	DAPL_CNO *cno_ptr;

	cno_ptr = (DAPL_CNO *) dapl_os_alloc(sizeof(DAPL_CNO));
	if (!cno_ptr) {
		return NULL;
	}

	/* zero the structure */
	dapl_os_memzero(cno_ptr, sizeof(DAPL_CNO));

	/*
	 * Initialize the header.
	 */
	cno_ptr->header.provider = ia_ptr->header.provider;
	cno_ptr->header.magic = DAPL_MAGIC_CNO;
#if !defined(__KDAPL__)
	cno_ptr->header.handle_type = DAT_HANDLE_TYPE_CNO;
#endif				/* defined(__KDAPL__) */
	cno_ptr->header.owner_ia = ia_ptr;
	cno_ptr->header.user_context.as_64 = 0;
	cno_ptr->header.user_context.as_ptr = NULL;
	dapl_llist_init_entry(&cno_ptr->header.ia_list_entry);
	dapl_os_lock_init(&cno_ptr->header.lock);

	/*
	 * Initialize the body
	 */
	cno_ptr->cno_waiters = 0;
	dapl_os_atomic_set(&cno_ptr->cno_ref_count, 0);
	cno_ptr->cno_state = DAPL_CNO_STATE_UNTRIGGERED;
	cno_ptr->cno_evd_triggered = NULL;
#if defined(__KDAPL__)
	cno_ptr->cno_upcall = *upcall;
#else
	cno_ptr->cno_wait_agent = wait_agent;
#endif				/* defined(__KDAPL__) */
	dapl_os_wait_object_init(&cno_ptr->cno_wait_object);

	return cno_ptr;
}

/*
 * dapl_cno_dealloc
 *
 * Free the passed in CNO structure.
 *
 * Input:
 * 	cno_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_cno_dealloc(IN DAPL_CNO * cno_ptr)
{
	dapl_os_assert(cno_ptr->header.magic == DAPL_MAGIC_CNO);
	dapl_os_assert(dapl_os_atomic_read(&cno_ptr->cno_ref_count) == 0);

	/*
	 * deinitialize the header
	 */
	cno_ptr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */

	dapl_os_wait_object_destroy(&cno_ptr->cno_wait_object);
	dapl_os_free(cno_ptr, sizeof(DAPL_CNO));
}

/*
 * dapl_internal_cno_trigger
 *
 * DAPL Internal routine to trigger the specified CNO.
 * Called by the callback of some EVD associated with the CNO.
 *
 *
 *
 * Input:
 *	cno_ptr
 *	evd_ptr		EVD triggering
 *
 * Output:
 *	None
 *
 * Returns:
 *	None
 */
void dapl_internal_cno_trigger(IN DAPL_CNO * cno_ptr, IN DAPL_EVD * evd_ptr)
{
	DAT_RETURN dat_status;
#if defined(__KDAPL__)
	DAT_EVENT event;
#endif				/* defined(__KDAPL__) */

	dat_status = DAT_SUCCESS;

	dapl_os_assert(cno_ptr->header.magic == DAPL_MAGIC_CNO);
	/* The spec allows NULL EVDs. kDAPL doesn't have CNOs, they
	 * are strictly used behind the scenes
	 */
	dapl_os_assert(evd_ptr == NULL
		       || evd_ptr->header.magic == DAPL_MAGIC_EVD);

	dapl_os_lock(&cno_ptr->header.lock);

	/* Maybe I should just return, but this really shouldn't happen.  */
	dapl_os_assert(cno_ptr->cno_state != DAPL_CNO_STATE_DEAD);

	if (cno_ptr->cno_state == DAPL_CNO_STATE_UNTRIGGERED) {
#if !defined(__KDAPL__)
		DAT_OS_WAIT_PROXY_AGENT agent;

		/* Squirrel away wait agent, and delete link.  */
		agent = cno_ptr->cno_wait_agent;
#endif				/* !defined(__KDAPL__) */

		/* Separate assignments for windows compiler.  */
#ifndef _WIN32
#if defined(__KDAPL__)
		cno_ptr->cno_upcall = DAT_UPCALL_NULL;
#else
		cno_ptr->cno_wait_agent = DAT_OS_WAIT_PROXY_AGENT_NULL;
#endif				/* defined(__KDAPL__) */
#else
		cno_ptr->cno_wait_agent.instance_data = NULL;
		cno_ptr->cno_wait_agent.proxy_agent_func = NULL;
#endif

		cno_ptr->cno_evd_triggered = evd_ptr;

		/*
		 * Must set to triggerred and let waiter untrigger to handle
		 * timeout of waiter.
		 */
		cno_ptr->cno_state = DAPL_CNO_STATE_TRIGGERED;
		if (cno_ptr->cno_waiters > 0) {
			dapl_os_wait_object_wakeup(&cno_ptr->cno_wait_object);
		}

		dapl_os_unlock(&cno_ptr->header.lock);

		/* Trigger the OS proxy wait agent, if one exists.  */
#if defined(__KDAPL__)
		dat_status = dapl_evd_dequeue((DAT_EVD_HANDLE) evd_ptr, &event);
		while (dat_status == DAT_SUCCESS) {
			if (cno_ptr->cno_upcall.upcall_func !=
			    (DAT_UPCALL_FUNC) NULL) {
				cno_ptr->cno_upcall.upcall_func(cno_ptr->
								cno_upcall.
								instance_data,
								&event,
								DAT_FALSE);
			}
			dat_status = dapl_evd_dequeue((DAT_EVD_HANDLE) evd_ptr,
						      &event);
		}
#else
		if (agent.proxy_agent_func != (DAT_AGENT_FUNC) NULL) {
			agent.proxy_agent_func(agent.instance_data,
					       (DAT_EVD_HANDLE) evd_ptr);
		}
#endif				/* defined(__KDAPL__) */
	} else {
		dapl_os_unlock(&cno_ptr->header.lock);
#if defined(__KDAPL__)
		dat_status = dapl_evd_dequeue((DAT_EVD_HANDLE) evd_ptr, &event);
		while (dat_status == DAT_SUCCESS) {
			if (cno_ptr->cno_upcall.upcall_func !=
			    (DAT_UPCALL_FUNC) NULL) {
				cno_ptr->cno_upcall.upcall_func(cno_ptr->
								cno_upcall.
								instance_data,
								&event,
								DAT_FALSE);
			}
			dat_status = dapl_evd_dequeue((DAT_EVD_HANDLE) evd_ptr,
						      &event);
		}
#endif				/* defined(__KDAPL__) */
	}

	return;
}

/*
 * dapl_cno_fd_create
 *
 * DAPL Requirements Version 2.0, 6.3.2.x
 *
 * creates a CNO instance. Upon creation, there are no
 * Event Dispatchers feeding it. os_fd is a File Descriptor in Unix, 
 * i.e. struct pollfd or an equivalent object in other OSes that is 
 * always associates with the created CNO. Consumer can multiplex event 
 * waiting using UNIX poll or select functions. Upon creation, the CNO 
 * is not associated with any EVDs, has no waiters and has the os_fd 
 * associated with it.
 *
 * Input:
 *	ia_handle
 *
 * Output:
 *	file descripter
 *	cno_handle
 * 
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_STATE
 *	DAT_LENGTH_ERROR
 *	DAT_PROTECTION_VIOLATION
 *	DAT_PRIVILEGES_VIOLATION
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API dapl_cno_fd_create(IN DAT_IA_HANDLE ia_handle,	/* ia_handle            */
				      OUT DAT_FD * fd,	/* file_descriptor      */
				      OUT DAT_CNO_HANDLE * cno_handle)
{				/* cno_handle           */
	return DAT_MODEL_NOT_SUPPORTED;
}

/*
 * dapl_cno_fd_create
 *
 * DAPL Requirements Version 2.0, 6.3.2.x
 *
 * Returns the latest EVD that triggered the CNO.
 * 
 * Input:
 *	cno_handle
 *
 * Output:
 *	evd_handle
 * 
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_cno_trigger(IN DAT_CNO_HANDLE cno_handle, OUT DAT_EVD_HANDLE * evd_handle)
{
	return DAT_MODEL_NOT_SUPPORTED;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
