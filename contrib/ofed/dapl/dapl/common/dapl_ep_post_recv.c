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
 * MODULE: dapl_ep_post_recv.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_cookie.h"
#include "dapl_adapter_util.h"

/*
 * dapl_ep_post_recv
 *
 * DAPL Requirements Version xxx, 6.5.11
 *
 * Request to receive data over the connection of ep handle into
 * local_iov
 *
 * Input:
 * 	ep_handle
 * 	num_segments
 * 	local_iov
 * 	user_cookie
 * 	completion_flags
 *
 * Output:
 * 	None.
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INVALID_STATE
 * 	DAT_PROTECTION_VIOLATION
 * 	DAT_PROVILEGES_VIOLATION
 */
DAT_RETURN DAT_API
dapl_ep_post_recv(IN DAT_EP_HANDLE ep_handle,
		  IN DAT_COUNT num_segments,
		  IN DAT_LMR_TRIPLET * local_iov,
		  IN DAT_DTO_COOKIE user_cookie,
		  IN DAT_COMPLETION_FLAGS completion_flags)
{
	DAPL_EP *ep_ptr;
	DAPL_COOKIE *cookie;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ep_post_recv (%p, %d, %p, %p, %x)\n",
		     ep_handle,
		     num_segments,
		     local_iov, user_cookie.as_64, completion_flags);

	if (DAPL_BAD_HANDLE(ep_handle, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Synchronization ok since this buffer is only used for receive
	 * requests, which aren't allowed to race with each other.
	 */
	dat_status = dapls_dto_cookie_alloc(&ep_ptr->recv_buffer,
					    DAPL_DTO_TYPE_RECV,
					    user_cookie, &cookie);
	if (DAT_SUCCESS != dat_status) {
		goto bail;
	}

	/*
	 * Invoke provider specific routine to post DTO
	 */
	dat_status =
	    dapls_ib_post_recv(ep_ptr, cookie, num_segments, local_iov);

	if (dat_status != DAT_SUCCESS) {
		dapls_cookie_dealloc(&ep_ptr->recv_buffer, cookie);
	}

      bail:
	dapl_dbg_log(DAPL_DBG_TYPE_RTN,
		     "dapl_ep_post_recv () returns 0x%x\n", dat_status);

	return dat_status;
}
