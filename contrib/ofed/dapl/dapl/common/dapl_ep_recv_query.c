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
 * MODULE: dapl_ep_recv_query.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.2 API, Chapter 6, section 6.11
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ep_util.h"

/*
 * dapl_ep_recv_query
 *
 * uDAPL Version 1.2, 6.6.11
 *
 * Destroy an instance of the Endpoint
 *
 * Input:
 *	ep_handle
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_HANDLE
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_ep_recv_query(IN DAT_EP_HANDLE ep_handle,
		   OUT DAT_COUNT * nbufs_allocate,
		   OUT DAT_COUNT * bufs_alloc_span)
{
	DAPL_EP *ep_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_ep_recv_query (%p, %p, %p)\n",
		     ep_handle, nbufs_allocate, bufs_alloc_span);

	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Verify parameter & state
	 */
	if (DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	dat_status = DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);

      bail:
	return dat_status;

}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
