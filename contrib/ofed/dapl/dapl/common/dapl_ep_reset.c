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
 * MODULE: dapl_ep_reset.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5.13
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"
#include "dapl_ring_buffer_util.h"

/*
 * dapl_ep_reset
 *
 * DAPL Requirements Version 1.1, 6.5.13
 *
 * Reset the QP attached to this Endpoint, transitioning back to the
 * INIT state
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
 *	DAT_INVALID_STATE
 */
DAT_RETURN DAT_API dapl_ep_reset(IN DAT_EP_HANDLE ep_handle)
{
	DAPL_EP *ep_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Verify parameter & state
	 */
	if (DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	if (ep_ptr->param.ep_state != DAT_EP_STATE_UNCONNECTED
	    && ep_ptr->param.ep_state != DAT_EP_STATE_DISCONNECTED) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      dapls_ep_state_subtype(ep_ptr));
		goto bail;
	}

	if (ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECTED) {
		dapls_ib_reinit_ep(ep_ptr);
		ep_ptr->param.ep_state = DAT_EP_STATE_UNCONNECTED;
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
