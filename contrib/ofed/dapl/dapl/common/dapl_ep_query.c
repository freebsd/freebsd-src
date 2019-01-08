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
 * MODULE: dapl_ep_query.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_adapter_util.h"

/*
 * dapl_ep_query
 *
 * DAPL Requirements Version xxx, 6.5.5
 *
 * Provide the consumer parameters, including attributes and status of
 * the Endpoint.
 *
 * Input:
 *	ep_handle
 *	ep_param_mask
 *
 * Output:
 *	ep_param
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_ep_query(IN DAT_EP_HANDLE ep_handle,
	      IN DAT_EP_PARAM_MASK ep_param_mask, OUT DAT_EP_PARAM * ep_param)
{
	DAPL_EP *ep_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ep_query (%p, %x, %p)\n",
		     ep_handle, ep_param_mask, ep_param);

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

	if (ep_param == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	/*
	 * Fill in according to user request
	 *
	 * N.B. Just slam all values into the user structure, there
	 *      is nothing to be gained by checking for each bit.
	 */
	if (ep_param_mask & DAT_EP_FIELD_ALL) {
		/* only attempt to get remote IA address if consumer requested it */
		if (ep_param_mask & DAT_EP_FIELD_REMOTE_IA_ADDRESS_PTR) {
			if (ep_ptr->param.ep_state == DAT_EP_STATE_CONNECTED) {
				/* obtain the remote IP address */
				dat_status =
				    dapls_ib_cm_remote_addr((DAT_HANDLE)
							    ep_handle,
							    &ep_ptr->
							    remote_ia_address);
			}
			ep_ptr->param.remote_ia_address_ptr =
			    (DAT_IA_ADDRESS_PTR) & ep_ptr->remote_ia_address;
		}
		*ep_param = ep_ptr->param;
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
