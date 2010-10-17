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
 * MODULE: dapl_evd_free.c
 *
 * PURPOSE: Event management
 * Description: Interfaces in this file are completely described in
 *        the DAPL 1.1 API, Chapter 6, section 3
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_evd_util.h"
#include "dapl_ia_util.h"
#include "dapl_cno_util.h"	/* for __KDAPL__ */

/*
 * dapl_evd_free
 *
 * DAPL Requirements Version xxx, 6.3.2.2
 *
 * Destroy a specific instance of the Event Dispatcher
 *
 * Input:
 *     evd_handle
 *
 * Output:
 *     None
 *
 * Returns:
 *     DAT_SUCCESS
 *     DAT_INVALID_HANDLE
 *     DAT_INVALID_STATE
 */
DAT_RETURN DAT_API dapl_evd_free(IN DAT_EVD_HANDLE evd_handle)
{
	DAPL_EVD *evd_ptr;
	DAPL_CNO *cno_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_evd_free (%p)\n", evd_handle);

	dat_status = DAT_SUCCESS;
	evd_ptr = (DAPL_EVD *) evd_handle;

	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
		goto bail;
	}

	DAPL_CNTR(evd_ptr->header.owner_ia, DCNT_IA_EVD_FREE);

	if (dapl_os_atomic_read(&evd_ptr->evd_ref_count) != 0) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_EVD_IN_USE);
		goto bail;
	}

	/* obtain the cno_ptr before the evd is released, which must occur
	 * before deallocating the CNO
	 */
	cno_ptr = evd_ptr->cno_ptr;

	dapl_ia_unlink_evd(evd_ptr->header.owner_ia, evd_ptr);

	dat_status = dapls_evd_dealloc(evd_ptr);
	if (dat_status != DAT_SUCCESS) {
		dapl_ia_link_evd(evd_ptr->header.owner_ia, evd_ptr);
	}
#if defined(__KDAPL__)
	if (cno_ptr != NULL) {
		if (dapl_os_atomic_read(&cno_ptr->cno_ref_count) > 0
		    || cno_ptr->cno_waiters > 0) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_STATE,
				      DAT_INVALID_STATE_EVD_IN_USE);
			goto bail;
		}
		dapl_ia_unlink_cno(cno_ptr->header.owner_ia, cno_ptr);
		dapl_cno_dealloc(cno_ptr);
	}
#else
	if (cno_ptr != NULL) {
		if (dapl_os_atomic_read(&cno_ptr->cno_ref_count) == 0
		    && cno_ptr->cno_waiters > 0) {
			/*
			 * Last reference on the CNO, trigger a notice. See
			 * uDAPL 1.1 spec 6.3.2.3
			 */
			dapl_internal_cno_trigger(cno_ptr, NULL);
		}
	}
#endif				/* defined(__KDAPL__) */

      bail:
	if (dat_status)
		dapl_dbg_log(DAPL_DBG_TYPE_RTN,
			     "dapl_evd_free () returns 0x%x\n", dat_status);

	return dat_status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
