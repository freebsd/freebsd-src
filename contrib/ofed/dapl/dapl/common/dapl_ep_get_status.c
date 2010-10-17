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
 * MODULE: dapl_ep_get_status.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ring_buffer_util.h"
#include "dapl_cookie.h"

/*
 * dapl_ep_get_status
 *
 * DAPL Requirements Version xxx, 6.5.4
 *
 * Provide the consumer with a quick snapshot of the Endpoint.
 * The snapshot consists of Endpoint state and DTO information.
 *
 * Input:
 *	ep_handle
 *
 * Output:
 *	ep_state
 *	in_dto_idle
 *	out_dto_idle
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_ep_get_status(IN DAT_EP_HANDLE ep_handle,
		   OUT DAT_EP_STATE * ep_state,
		   OUT DAT_BOOLEAN * in_dto_idle,
		   OUT DAT_BOOLEAN * out_dto_idle)
{
	DAPL_EP *ep_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ep_get_status (%p, %p, %p, %p)\n",
		     ep_handle, ep_state, in_dto_idle, out_dto_idle);

	ep_ptr = (DAPL_EP *) ep_handle;
	dat_status = DAT_SUCCESS;

	/*
	 * Verify parameter & state
	 */
	if (DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	/*
	 * Gather state info for user
	 */
	if (ep_state != NULL) {
		*ep_state = ep_ptr->param.ep_state;
	}

	if (in_dto_idle != NULL) {
		*in_dto_idle = dapls_cb_pending(&ep_ptr->recv_buffer);
	}

	if (out_dto_idle != NULL) {
		*out_dto_idle = dapls_cb_pending(&ep_ptr->req_buffer);
	}

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
