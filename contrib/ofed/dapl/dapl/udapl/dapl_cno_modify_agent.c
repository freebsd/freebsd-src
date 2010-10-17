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
 * MODULE: dapl_cno_modify_agent.c
 *
 * PURPOSE: Modify the wait proxy agent associted with the CNO
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 3.2.4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_cno_modify_agent
 *
 * DAPL Requirements Version xxx, 6.3.2.4
 *
 * Modify the wait proxy agent associted with the CNO
 *
 * Input:
 *	cno_handle
 *	prx_agent
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API dapl_cno_modify_agent(IN DAT_CNO_HANDLE cno_handle,	/* cno_handle */
					 IN DAT_OS_WAIT_PROXY_AGENT prx_agent)
{				/* agent */
	DAPL_CNO *cno_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;
	if (DAPL_BAD_HANDLE(cno_handle, DAPL_MAGIC_CNO)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CNO);
		goto bail;
	}

	cno_ptr = (DAPL_CNO *) cno_handle;
	dapl_os_lock(&cno_ptr->header.lock);
	cno_ptr->cno_wait_agent = prx_agent;
	dapl_os_unlock(&cno_ptr->header.lock);

      bail:
	return dat_status;
}
