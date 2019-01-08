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
 * MODULE: dapl_evd_modify_cno.c
 *
 * PURPOSE: Event Management
 *
 * Description: Interfaces in this file are completely described in
 * 		the DAPL 1.1 API, Chapter 6, section 3
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_adapter_util.h"

/*
 * dapl_evd_modify_cno
 *
 * DAPL Requirements Version xxx, 6.3.2.4
 *
 * Modify the CNO associated with the EVD
 *
 * Input:
 * 	evd_handle
 * 	cno_handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCSSS
 * 	DAT_INVALID_HANDLE
 */

DAT_RETURN DAT_API dapl_evd_modify_cno(IN DAT_EVD_HANDLE evd_handle,
				       IN DAT_CNO_HANDLE cno_handle)

{
	DAPL_EVD *evd_ptr;
	DAPL_CNO *cno_ptr;
	DAPL_CNO *old_cno_ptr;
	DAT_RETURN dat_status;

	evd_ptr = (DAPL_EVD *) evd_handle;
	cno_ptr = (DAPL_CNO *) cno_handle;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
		goto bail;
	}
	if (cno_handle != NULL && DAPL_BAD_HANDLE(cno_handle, DAPL_MAGIC_CNO)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CNO);
		goto bail;
	}
	dapl_os_lock(&evd_ptr->header.lock);
	old_cno_ptr = evd_ptr->cno_ptr;
	evd_ptr->cno_ptr = cno_ptr;
	dapl_os_unlock(&evd_ptr->header.lock);
	if (cno_ptr) {
		dapl_os_atomic_inc(&(cno_ptr->cno_ref_count));
	}
	if (old_cno_ptr) {
		dapl_os_atomic_dec(&(old_cno_ptr->cno_ref_count));
	}

	/* We need to enable the callback handler if the EVD is enabled.  */
	if (evd_ptr->evd_enabled &&
	    cno_handle != DAT_HANDLE_NULL &&
	    evd_ptr->ib_cq_handle != IB_INVALID_HANDLE) {
		dat_status =
		    dapls_ib_completion_notify(evd_ptr->header.owner_ia->
					       hca_ptr->ib_hca_handle, evd_ptr,
					       IB_NOTIFY_ON_NEXT_COMP);

		/* FIXME report error */
		dapl_os_assert(dat_status == DAT_SUCCESS);
	}

      bail:
	return dat_status;
}
