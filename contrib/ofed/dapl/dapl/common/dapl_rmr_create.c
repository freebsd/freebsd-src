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
 * MODULE: dapl_rmr_create.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl_rmr_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_rmr_create
 *
 * Create a remote memory region for the specified protection zone
 *
 * Input:
 * 	pz_handle
 *
 * Output:
 * 	rmr_handle
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_rmr_create(IN DAT_PZ_HANDLE pz_handle, OUT DAT_RMR_HANDLE * rmr_handle)
{
	DAPL_PZ *pz;
	DAPL_RMR *rmr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(pz_handle, DAPL_MAGIC_PZ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
		goto bail;
	}

	pz = (DAPL_PZ *) pz_handle;
	rmr = dapl_rmr_alloc(pz);

	if (rmr == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}
	DAPL_CNTR(pz->header.owner_ia, DCNT_IA_RMR_CREATE);

	dat_status = dapls_ib_mw_alloc(rmr);

	if (dat_status != DAT_SUCCESS) {
		dapl_rmr_dealloc(rmr);
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
			      DAT_RESOURCE_MEMORY_REGION);
		goto bail;
	}

	dapl_os_atomic_inc(&pz->pz_ref_count);

	*rmr_handle = rmr;

      bail:
	return dat_status;
}

/*
 * dapl_rmr_create_for_ep
 *
 * DAPL Requirements Version 2.0, 6.7.3.x
 *
 * Creates an RMR that is specific to a single connection at a time.
 * 
 * This operation is relatively heavy. The created RMR can be bound to a
 * memory region within the LMR through a lightweight dat_rmr_bind
 * operation for EPs that use the pz_handle that generates rmr_context.
 * 
 * If the operation fails (does not return DAT_SUCCESS), the return values
 * of rmr_handle are undefined and Consumers should not use it.
 * pz_handle provide Consumers a way to restrict access to an RMR by
 * authorized connections only.
 * 
 *
 * Input:
 *	pz_handle
 *
 * Output:
 *	rmr_handle
 * 
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_HANDLE
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API dapl_rmr_create_for_ep(IN DAT_PZ_HANDLE pz_handle,	/* pz_handle            */
					  OUT DAT_RMR_HANDLE * rmr_handle)
{				/* rmr_handle           */
	return DAT_MODEL_NOT_SUPPORTED;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
