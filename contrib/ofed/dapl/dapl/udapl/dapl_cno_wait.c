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
 * MODULE: dapl_cno_wait.c
 *
 * PURPOSE: Wait for a consumer notification event
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 3.2.3
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_cno_wait
 *
 * DAPL Requirements Version xxx, 6.3.2.3
 *
 * Wait for a consumer notification event
 *
 * Input:
 *	cno_handle
 *	timeout
 *	evd_handle
 *
 * Output:
 *	evd_handle
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *	DAT_QUEUE_EMPTY
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API dapl_cno_wait(IN DAT_CNO_HANDLE cno_handle,	/* cno_handle */
				 IN DAT_TIMEOUT timeout,	/* agent */
				 OUT DAT_EVD_HANDLE * evd_handle)
{				/* ia_handle */
	DAPL_CNO *cno_ptr;
	DAT_RETURN dat_status;

	if (DAPL_BAD_HANDLE(cno_handle, DAPL_MAGIC_CNO)) {
		dat_status = DAT_INVALID_HANDLE | DAT_INVALID_HANDLE_CNO;
		goto bail;
	}

	dat_status = DAT_SUCCESS;

	cno_ptr = (DAPL_CNO *) cno_handle;

	if (cno_ptr->cno_state == DAPL_CNO_STATE_DEAD) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_CNO_DEAD);
		goto bail;
	}

	dapl_os_lock(&cno_ptr->header.lock);
	if (cno_ptr->cno_state == DAPL_CNO_STATE_TRIGGERED) {
		cno_ptr->cno_state = DAPL_CNO_STATE_UNTRIGGERED;
		*evd_handle = cno_ptr->cno_evd_triggered;
		cno_ptr->cno_evd_triggered = NULL;
		dapl_os_unlock(&cno_ptr->header.lock);
		goto bail;
	}

	while (cno_ptr->cno_state == DAPL_CNO_STATE_UNTRIGGERED
	       && DAT_GET_TYPE(dat_status) != DAT_TIMEOUT_EXPIRED) {
		cno_ptr->cno_waiters++;
		dapl_os_unlock(&cno_ptr->header.lock);
		dat_status = dapl_os_wait_object_wait(&cno_ptr->cno_wait_object,
						      timeout);
		dapl_os_lock(&cno_ptr->header.lock);
		cno_ptr->cno_waiters--;
	}

	if (cno_ptr->cno_state == DAPL_CNO_STATE_DEAD) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_CNO_DEAD);
	} else if (dat_status == DAT_SUCCESS) {
		/*
		 * After the first triggering, this will be a valid handle.
		 * If we're racing with wakeups of other CNO waiters,
		 * that's ok.
		 */
		dapl_os_assert(cno_ptr->cno_state == DAPL_CNO_STATE_TRIGGERED);
		cno_ptr->cno_state = DAPL_CNO_STATE_UNTRIGGERED;
		*evd_handle = cno_ptr->cno_evd_triggered;
		cno_ptr->cno_evd_triggered = NULL;
	} else if (DAT_GET_TYPE(dat_status) == DAT_TIMEOUT_EXPIRED) {
		cno_ptr->cno_state = DAPL_CNO_STATE_UNTRIGGERED;
		*evd_handle = NULL;
		dat_status = DAT_QUEUE_EMPTY;
	} else {
		/*
		 * The only other reason we could have made it out of
		 * the loop is an interrupted system call.
		 */
		dapl_os_assert(DAT_GET_TYPE(dat_status) ==
			       DAT_INTERRUPTED_CALL);
	}
	dapl_os_unlock(&cno_ptr->header.lock);

      bail:
	return dat_status;
}
