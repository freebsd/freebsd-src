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
 * MODULE: dapl_lmr_free.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl_lmr_util.h"
#include "dapl_adapter_util.h"
#include "dapl_ia_util.h"

/*
 * dapl_lmr_free
 *
 * DAPL Requirements Version xxx, 6.6.3.2
 *
 * Destroy an instance of the Local Memory Region
 *
 * Input:
 * 	lmr_handle
 *
 * Output:
 *
 * Returns:
 * 	DAT_SUCCESS
 *      DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INVALID_STATE 
 */

DAT_RETURN DAT_API dapl_lmr_free(IN DAT_LMR_HANDLE lmr_handle)
{
	DAPL_LMR *lmr;
	DAPL_PZ *pz;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_lmr_free (%p)\n", lmr_handle);

	if (DAPL_BAD_HANDLE(lmr_handle, DAPL_MAGIC_LMR)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_LMR);
		goto bail;
	}

	lmr = (DAPL_LMR *) lmr_handle;
	pz = (DAPL_PZ *) lmr->param.pz_handle;

	DAPL_CNTR(pz->header.owner_ia, DCNT_IA_LMR_FREE);

	switch (lmr->param.mem_type) {
#if defined(__KDAPL__)
	case DAT_MEM_TYPE_PHYSICAL:
#else
	case DAT_MEM_TYPE_SHARED_VIRTUAL:
#endif				/* defined(__KDAPL__) */
		/* fall through */
	case DAT_MEM_TYPE_VIRTUAL:
	case DAT_MEM_TYPE_LMR:
		{
			if (0 != dapl_os_atomic_read(&lmr->lmr_ref_count)) {
				return DAT_INVALID_STATE;
			}

			dat_status =
			    dapls_hash_remove(lmr->header.owner_ia->hca_ptr->
					      lmr_hash_table,
					      lmr->param.lmr_context, NULL);
			if (dat_status != DAT_SUCCESS) {
				goto bail;
			}

			dat_status = dapls_ib_mr_deregister(lmr);

			if (dat_status == DAT_SUCCESS) {
				dapl_os_atomic_dec(&pz->pz_ref_count);
				dapl_lmr_dealloc(lmr);
			} else {
				/*
				 * Deregister failed; put it back in the
				 * hash table.
				 */
				dapls_hash_insert(lmr->header.owner_ia->
						  hca_ptr->lmr_hash_table,
						  lmr->param.lmr_context, lmr);
			}

			break;
		}
#if defined(__KDAPL__)
	case DAT_MEM_TYPE_PLATFORM:
	case DAT_MEM_TYPE_IA:
	case DAT_MEM_TYPE_BYPASS:
		{
			return DAT_ERROR(DAT_NOT_IMPLEMENTED, 0);
		}
#endif				/* defined(__KDAPL__) */
	default:
		{
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG1);
			break;
		}
	}
      bail:
	return dat_status;
}
