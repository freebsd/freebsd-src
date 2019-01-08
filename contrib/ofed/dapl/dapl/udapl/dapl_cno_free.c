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
 * MODULE: dapl_cno_free.c
 *
 * PURPOSE: Consumer Notification Object destruction
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 3.2.2
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"
#include "dapl_cno_util.h"

/*
 * dapl_cno_free
 *
 * DAPL Requirements Version xxx, 6.3.2.2
 *
 * Destroy a consumer notification object instance
 *
 * Input:
 *	cno_handle
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_STATE
 */
DAT_RETURN DAT_API dapl_cno_free(IN DAT_CNO_HANDLE cno_handle)
{				/* cno_handle */
	DAPL_CNO *cno_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;
	cno_ptr = (DAPL_CNO *) cno_handle;

	if (DAPL_BAD_HANDLE(cno_handle, DAPL_MAGIC_CNO)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CNO);
		goto bail;
	}

	if (dapl_os_atomic_read(&cno_ptr->cno_ref_count) != 0
	    || cno_ptr->cno_waiters != 0) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_CNO_IN_USE);
		goto bail;
	}

	dapl_ia_unlink_cno(cno_ptr->header.owner_ia, cno_ptr);
	dapl_cno_dealloc(cno_ptr);

      bail:
	return dat_status;
}
