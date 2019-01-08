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
 * MODULE: dapl_evd_post_se.c
 *
 * PURPOSE: Event Management
 *
 * Description: Interfaces in this file are completely defined in 
 *              the uDAPL 1.1 API, Chapter 6, section 3
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_evd_util.h"
#include "dapl_ia_util.h"

/*
 * dapl_evd_post_se
 *
 * DAPL Requirements Version xxx, 6.3.2.7
 *
 * Post a software event to the Event Dispatcher event queue.
 *
 * Input:
 * 	evd_handle
 * 	event
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 */

DAT_RETURN DAT_API dapl_evd_post_se(DAT_EVD_HANDLE evd_handle,
				    const DAT_EVENT * event)
{
	DAPL_EVD *evd_ptr;
	DAT_RETURN dat_status;

	evd_ptr = (DAPL_EVD *) evd_handle;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
		goto bail;
	}
	/* Only post to EVDs that are specific to software events */
	if (!(evd_ptr->evd_flags & DAT_EVD_SOFTWARE_FLAG)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG1);
		goto bail;
	}

	if (!event) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}
	if (event->event_number != DAT_SOFTWARE_EVENT) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	dat_status = dapls_evd_post_software_event(evd_ptr,
						   DAT_SOFTWARE_EVENT,
						   event->event_data.
						   software_event_data.pointer);

      bail:
	return dat_status;
}
