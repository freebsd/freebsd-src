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
 * MODULE: dapl_lmr_sync_rdma_read.c
 *
 * PURPOSE: Interface Adapter management
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"

/*
 * dat_lmr_sync_rdma_read
 *
 * Ensure a region of memory is consistent by locally flushing
 * non-coherent cache
 *
 * Input:
 *	ia_handle
 *	local_segments		Array of buffer segments
 *	num_segments		Number of segments in local_segments
 *
 * Output:
 *	none
 *
 * Return Values:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_lmr_sync_rdma_read(IN DAT_IA_HANDLE ia_handle,
			IN const DAT_LMR_TRIPLET * local_segments,
			IN DAT_VLEN num_segments)
{
	DAPL_IA *ia_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dat_lmr_sync_rdma_read (%p, %p, %ld)\n",
		     ia_handle, local_segments, num_segments);

	ia_ptr = (DAPL_IA *) ia_handle;

	if (DAPL_BAD_HANDLE(ia_ptr, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}

	dat_status = dapl_os_sync_rdma_read(local_segments, num_segments);

      bail:
	return dat_status;
}
