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
 * MODULE: dapl_ep_post_rdma_read.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5
 *
 * $Id:$
 **********************************************************************/

#include "dapl_ep_util.h"

/*
 * dapl_ep_post_rdma_read
 *
 * DAPL Requirements Version xxx, 6.5.12
 *
 * Request the xfer of all data specified by the remote_iov over the
 * connection of ep handle Endpint into the local_iov
 *
 * Input:
 * 	ep_handle
 * 	num_segments
 * 	local_iov
 * 	user_cookie
 * 	remote_iov
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
 * 	DAT_LENGTH_ERROR
 * 	DAT_PROTECTION_VIOLATION
 * 	DAT_PRIVILEGES_VIOLATION
 */
DAT_RETURN DAT_API
dapl_ep_post_rdma_read(IN DAT_EP_HANDLE ep_handle,
		       IN DAT_COUNT num_segments,
		       IN DAT_LMR_TRIPLET * local_iov,
		       IN DAT_DTO_COOKIE user_cookie,
		       IN const DAT_RMR_TRIPLET * remote_iov,
		       IN DAT_COMPLETION_FLAGS completion_flags)
{

	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ep_post_rdma_read (%p, %d, %p, %p, %p, %x)\n",
		     ep_handle,
		     num_segments,
		     local_iov,
		     user_cookie.as_64, remote_iov, completion_flags);

	dat_status = dapl_ep_post_send_req(ep_handle,
					   num_segments,
					   local_iov,
					   user_cookie,
					   remote_iov,
					   completion_flags,
					   DAPL_DTO_TYPE_RDMA_READ,
					   OP_RDMA_READ);

	dapl_dbg_log(DAPL_DBG_TYPE_RTN,
		     "dapl_ep_post_rdma_read () returns 0x%x\n", dat_status);

	return dat_status;
}
