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
 * MODULE: dapl_rmr_free.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl_rmr_util.h"
#include "dapl_adapter_util.h"
#include "dapl_ia_util.h"

/*
 * dapl_rmr_free
 *
 * Destroy an instance of the Remote Memory Region
 *
 * Input:
 * 	rmr_handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API dapl_rmr_free(IN DAT_RMR_HANDLE rmr_handle)
{
	DAPL_RMR *rmr;
	DAPL_PZ *pz;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(rmr_handle, DAPL_MAGIC_RMR)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RMR);
		goto bail;
	}

	rmr = (DAPL_RMR *) rmr_handle;
	pz = (DAPL_PZ *) rmr->param.pz_handle;
	DAPL_CNTR(pz->header.owner_ia, DCNT_IA_RMR_FREE);

	/*
	 * If the user did not perform an unbind op, release
	 * counts here.
	 */
	if (rmr->param.lmr_triplet.virtual_address != 0) {
		dapl_os_atomic_dec(&rmr->lmr->lmr_ref_count);
		rmr->param.lmr_triplet.virtual_address = 0;
	}

	dat_status = dapls_ib_mw_free(rmr);

	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}

	dapl_os_atomic_dec(&rmr->pz->pz_ref_count);

	dapl_rmr_dealloc(rmr);

      bail:
	return dat_status;
}
