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
 * MODULE: dapl_lmr_query.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_lmr_query
 *
 * Provide the LMR arguments.
 *
 * Input:
 * 	lmr_handle
 * 	lmr_param_mask
 *	lmr_param
 *
 * Output:
 * 	lmr_param
 *
 * Returns:
 * 	DAT_SUCCESS
 *      DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_lmr_query(IN DAT_LMR_HANDLE lmr_handle,
	       IN DAT_LMR_PARAM_MASK lmr_param_mask,
	       IN DAT_LMR_PARAM * lmr_param)
{
	DAPL_LMR *lmr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_lmr_query (%p, 0x%x, %p)\n",
		     lmr_handle, lmr_param_mask, lmr_param);

	if (DAPL_BAD_HANDLE(lmr_handle, DAPL_MAGIC_LMR)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_LMR);
		goto bail;
	}
	if (NULL == lmr_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	dat_status = DAT_SUCCESS;
	lmr = (DAPL_LMR *) lmr_handle;

	dapl_os_memcpy(lmr_param, &lmr->param, sizeof(DAT_LMR_PARAM));

      bail:
	return dat_status;
}
