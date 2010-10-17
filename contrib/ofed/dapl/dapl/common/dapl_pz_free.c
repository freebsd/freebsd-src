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
 * MODULE: dapl_pz_free.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_pz_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_pz_free
 *
 * Remove an instance of a protection zone
 *
 * Input:
 * 	pz_handle
 *
 * Output:
 * 	None.
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_STATE
 * 	DAT_INVALID_HANDLE
 */
DAT_RETURN DAT_API dapl_pz_free(IN DAT_PZ_HANDLE pz_handle)
{
	DAPL_PZ *pz;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_pz_free (%p)\n", pz_handle);

	dat_status = DAT_SUCCESS;
	if (DAPL_BAD_HANDLE(pz_handle, DAPL_MAGIC_PZ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
		goto bail;
	}

	pz = (DAPL_PZ *) pz_handle;

	DAPL_CNTR(pz->header.owner_ia, DCNT_IA_PZ_FREE);

	if (0 != dapl_os_atomic_read(&pz->pz_ref_count)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_PZ_IN_USE);
		goto bail;
	}

	dat_status = dapls_ib_pd_free(pz);
	if (DAT_SUCCESS == dat_status) {
		dapl_pz_dealloc(pz);
	}

      bail:
	return dat_status;
}
