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
 * MODULE: dapl_evd_query.c
 *
 * PURPOSE: Event management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 3
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_evd_query
 *
 * DAPL Requirements Version xxx, 6.3.2.3
 *
 * Provides the consumer with arguments of the Event Dispatcher.
 *
 * Input:
 * 	evd_handle
 * 	evd_mask
 *
 * Output:
 * 	evd_param
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_evd_query(IN DAT_EVD_HANDLE evd_handle,
	       IN DAT_EVD_PARAM_MASK evd_param_mask,
	       OUT DAT_EVD_PARAM * evd_param)
{
	DAPL_EVD *evd_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	if (NULL == evd_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	/* Note: the spec. allows for events to be directed to a NULL EVD */
	/* with handle of type DAT_HANDLE_NULL. See 6.3.1                 */
	if (DAT_HANDLE_NULL == evd_handle) {
		dapl_os_memzero(evd_param, sizeof(DAT_EVD_PARAM));
	} else {
		if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
			dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
			goto bail;
		}

		evd_ptr = (DAPL_EVD *) evd_handle;

		/*
		 * We may be racing against the thread safe modify
		 * calls here (dat_evd_{enable,disable,{set,clear}_unwaitable}).
		 * They are thread safe, so our reads need to be atomic with
		 * regard to those calls.  The below is ok (a single bit
		 * read counts as atomic; if it's in transition you'll get one
		 * of the correct values) but we'll need to be careful
		 * about reading the state variable atomically when we add
		 * in waitable/unwaitable.
		 */
		evd_param->evd_state =
		    (evd_ptr->
		     evd_enabled ? DAT_EVD_STATE_ENABLED :
		     DAT_EVD_STATE_DISABLED);
		evd_param->evd_state |=
		    (evd_ptr->
		     evd_waitable ? DAT_EVD_STATE_WAITABLE :
		     DAT_EVD_STATE_UNWAITABLE);
		evd_param->ia_handle = evd_ptr->header.owner_ia;
		evd_param->evd_qlen = evd_ptr->qlen;
		evd_param->cno_handle = (DAT_CNO_HANDLE) evd_ptr->cno_ptr;
		evd_param->evd_flags = evd_ptr->evd_flags;
	}

      bail:
	return dat_status;
}
