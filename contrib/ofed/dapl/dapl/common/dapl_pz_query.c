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
 * MODULE: dapl_pz_query.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_pz_query
 *
 * Return the ia associated with the protection zone pz
 *
 * Input:
 * 	pz_handle
 *      pz_param_mask
 *
 * Output:
 * 	pz_param
 *
 * Returns:
 * 	DAT_SUCCESS
 *      DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_pz_query(IN DAT_PZ_HANDLE pz_handle,
	      IN DAT_PZ_PARAM_MASK pz_param_mask, OUT DAT_PZ_PARAM * pz_param)
{
	DAPL_PZ *pz;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_pz_query (%p, %x, %p)\n",
		     pz_handle, pz_param_mask, pz_param);

	if (DAPL_BAD_HANDLE(pz_handle, DAPL_MAGIC_PZ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
		goto bail;
	}
	if (NULL == pz_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	dat_status = DAT_SUCCESS;
	pz = (DAPL_PZ *) pz_handle;

	/* Since the DAT_PZ_ARGS values are easily accessible, */
	/* don't bother checking the DAT_PZ_ARGS_MASK value    */
	pz_param->ia_handle = (DAT_IA_HANDLE) pz->header.owner_ia;

      bail:
	return dat_status;
}
