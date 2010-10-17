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
 * MODULE: dapl_cno_create.c
 *
 * PURPOSE: Consumer Notification Object creation
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 3.2.1
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_cno_util.h"
#include "dapl_ia_util.h"

/*
 * dapl_cno_create
 *
 * DAPL Requirements Version xxx, 6.3.4.1
 *
 * Create a consumer notification object instance
 *
 * Input:
 *	ia_handle
 *	wait_agent
 *	cno_handle
 *
 * Output:
 *	cno_handle
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API dapl_cno_create(IN DAT_IA_HANDLE ia_handle,	/* ia_handle */
				   IN DAT_OS_WAIT_PROXY_AGENT wait_agent,	/* agent */
				   OUT DAT_CNO_HANDLE * cno_handle)
{				/* cno_handle */
	DAPL_IA *ia_ptr;
	DAPL_CNO *cno_ptr;
	DAT_RETURN dat_status;

	ia_ptr = (DAPL_IA *) ia_handle;
	cno_ptr = NULL;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(ia_handle, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}

	cno_ptr = dapl_cno_alloc(ia_ptr, wait_agent);

	if (!cno_ptr) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	cno_ptr->cno_state = DAPL_CNO_STATE_UNTRIGGERED;

	dapl_ia_link_cno(ia_ptr, cno_ptr);

	*cno_handle = cno_ptr;

      bail:
	if (dat_status != DAT_SUCCESS && cno_ptr != NULL) {
		dapl_cno_dealloc(cno_ptr);
	}
	return dat_status;
}
