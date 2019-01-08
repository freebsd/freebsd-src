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
 * MODULE: dapl_srq_post_recv.c
 *
 * PURPOSE: Shared Receive Queue management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5.8
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_cookie.h"
#include "dapl_adapter_util.h"

/*
 * dapl_srq_post_recv
 *
 * DAPL Requirements Version 1.2, 6.5.8
 *
 * Post a receive buffer that can be used by any incoming
 * message by any connected EP using the SRQ. Request to receive data
 * over a connection of any ep handle into local_iov
 *
 * Input:
 * 	srq_handle
 * 	num_segments
 * 	local_iov
 * 	user_cookie
 *
 * Output:
 * 	None.
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INVALID_HANDLE
 * 	DAT_INVALID_STATE
 * 	DAT_PROTECTION_VIOLATION
 * 	DAT_PROVILEGES_VIOLATION
 */
DAT_RETURN DAT_API
dapl_srq_post_recv(IN DAT_SRQ_HANDLE srq_handle,
		   IN DAT_COUNT num_segments,
		   IN DAT_LMR_TRIPLET * local_iov,
		   IN DAT_DTO_COOKIE user_cookie)
{
	DAPL_SRQ *srq_ptr;
	DAPL_COOKIE *cookie;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_srq_post_recv (%p, %d, %p, %p)\n",
		     srq_handle, num_segments, local_iov, user_cookie.as_64);

	if (DAPL_BAD_HANDLE(srq_handle, DAPL_MAGIC_SRQ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
		goto bail;
	}

	srq_ptr = (DAPL_SRQ *) srq_handle;

	/*
	 * Synchronization ok since this buffer is only used for receive
	 * requests, which aren't allowed to race with each other. The
	 * app must syncronize access to the SRQ.
	 */
	dat_status = dapls_dto_cookie_alloc(&srq_ptr->recv_buffer,
					    DAPL_DTO_TYPE_RECV,
					    user_cookie, &cookie);
	if (DAT_SUCCESS != dat_status) {
		goto bail;
	}

	/*
	 * Take reference before posting to avoid race conditions with
	 * completions
	 */
	dapl_os_atomic_inc(&srq_ptr->recv_count);

	/*
	 * Invoke provider specific routine to post DTO
	 */
	/* XXX Put code here XXX */
	/* XXX */ dat_status = DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);

	if (dat_status != DAT_SUCCESS) {
		dapl_os_atomic_dec(&srq_ptr->recv_count);
		dapls_cookie_dealloc(&srq_ptr->recv_buffer, cookie);
	}

      bail:
	return dat_status;
}
