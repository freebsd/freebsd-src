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
 * MODULE: dapl_pz_create.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_pz_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_pz_create
 *
 * Create an instance of a protection zone
 *
 * Input:
 *      ia_handle
 *
 * Output:
 *      pz_handle
 *
 * Returns:
 *      DAT_SUCCESS
 *      DAT_INSUFFICIENT_RESOURCES
 *      DAT_INVALID_PARAMETER
 *      DAT_INVLAID_HANDLE
 */
DAT_RETURN DAT_API
dapl_pz_create(IN DAT_IA_HANDLE ia_handle, OUT DAT_PZ_HANDLE * pz_handle)
{
	DAPL_IA *ia;
	DAPL_PZ *pz;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_pz_create (%p, %p)\n", ia_handle, pz_handle);

	dat_status = DAT_SUCCESS;
	if (DAPL_BAD_HANDLE(ia_handle, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}

	if (NULL == pz_handle) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	ia = (DAPL_IA *) ia_handle;

	pz = dapl_pz_alloc(ia);
	if (NULL == pz) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}
	DAPL_CNTR(ia, DCNT_IA_PZ_CREATE);

	dat_status = dapls_ib_pd_alloc(ia, pz);
	if (DAT_SUCCESS != dat_status) {
		dapl_pz_dealloc(pz);
		pz = NULL;
	}

	*pz_handle = pz;

      bail:
	return dat_status;
}
