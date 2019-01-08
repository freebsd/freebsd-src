/*
 * Copyright (c) 2007 Intel Corporation.  All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
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
 * MODULE: dapl_ep_post_send_invalidate.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 2.0 API, Chapter 6, section 6
 *
 * $Id:$
 **********************************************************************/

#include "dapl_ep_util.h"

/*
 * dapl_ep_post_send_invalidate
 *
 * DAPL Requirements Version xxx, 6.6.21
 *
 * Requests a transfer of all the data from the local_iov over the connection
 * of the ep_handle Endpoint to the remote side and invalidates the Remote 
 * Memory Region context.
 *
 * Input:
 *	ep_handle
 *	num_segments
 *	local_iov
 *	user_cookie
 *	completion_flags
 *	invalidate flag
 *	RMR context  
 *
 * Output:
 *	None
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_STATE
 *	DAT_PROTECTION_VIOLATION
 *	DAT_PRIVILEGES_VIOLATION
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API dapl_ep_post_send_with_invalidate(IN DAT_EP_HANDLE ep_handle,	/* ep_handle            */
						     IN DAT_COUNT num_segments,	/* num_segments         */
						     IN DAT_LMR_TRIPLET * local_iov,	/* local_iov            */
						     IN DAT_DTO_COOKIE user_cookie,	/* user_cookie          */
						     IN DAT_COMPLETION_FLAGS flags,	/* completion_flags     */
						     IN DAT_BOOLEAN invalidate_flag,	/* invalidate_flag      */
						     IN DAT_RMR_CONTEXT
						     rmr_context)
{				/* RMR context          */
	return DAT_MODEL_NOT_SUPPORTED;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
