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
 * MODULE: dapl_evd_clear_unwaitable.c
 *
 * PURPOSE: EVENT management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 3.4.8
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_evd_clear_unwaitable
 *
 * DAPL Requirements Version 1.1, 6.3.4.8
 *
 * Transition the Event Dispatcher into a waitable state
 *
 * Input:
 * 	evd_handle
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 */
DAT_RETURN DAT_API dapl_evd_clear_unwaitable(IN DAT_EVD_HANDLE evd_handle)
{
	DAPL_EVD *evd_ptr;
	DAT_RETURN dat_status;

	evd_ptr = (DAPL_EVD *) evd_handle;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD))
	{
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
		goto bail;
	}
	dapl_os_lock(&evd_ptr->header.lock);
	evd_ptr->evd_waitable = DAT_TRUE;
	dapl_os_unlock(&evd_ptr->header.lock);

	dat_status = DAT_SUCCESS;

      bail:
	return dat_status;
}
