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
 * MODULE: dapl_cr_reject.c
 *
 * PURPOSE: Connection management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_cr_util.h"
#include "dapl_sp_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_cr_reject
 *
 * DAPL Requirements Version xxx, 6.4.2.2
 *
 * Reject a connection request from the active remote side requesting
 * an Endpoint.
 *
 * Input:
 *	cr_handle
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API dapl_cr_reject(IN DAT_CR_HANDLE cr_handle,	/* cr_handle            */
				  IN DAT_COUNT pdata_size,	/* private_data_size    */
				  IN const DAT_PVOID pdata)
{				/* private_data         */
	DAPL_CR *cr_ptr;
	DAPL_EP *ep_ptr;
	DAT_EP_STATE entry_ep_state;
	DAT_EP_HANDLE entry_ep_handle;
	DAPL_SP *sp_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_cr_reject (%p)\n", cr_handle);

	if (DAPL_BAD_HANDLE(cr_handle, DAPL_MAGIC_CR)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CR);
		goto bail;
	}

	cr_ptr = (DAPL_CR *) cr_handle;

	/*
	 * Clean up provider created EP if there is one: only if
	 * DAT_PSP_PROVIDER_FLAG was set on the PSP
	 */
	ep_ptr = (DAPL_EP *) cr_ptr->param.local_ep_handle;
	entry_ep_handle = cr_ptr->param.local_ep_handle;
	entry_ep_state = 0;
	if (ep_ptr != NULL) {
		entry_ep_state = ep_ptr->param.ep_state;
		ep_ptr->param.ep_state = DAT_EP_STATE_UNCONNECTED;
		cr_ptr->param.local_ep_handle = NULL;
	}

	dat_status = dapls_ib_reject_connection(cr_ptr->ib_cm_handle,
						IB_CM_REJ_REASON_CONSUMER_REJ,
						pdata_size, pdata);

	if (dat_status != DAT_SUCCESS) {
		if (ep_ptr != NULL) {
			/* Revert our state to the beginning */
			ep_ptr->param.ep_state = entry_ep_state;
			cr_ptr->param.local_ep_handle = entry_ep_handle;
			cr_ptr->param.local_ep_handle = (DAT_EP_HANDLE) ep_ptr;
		}
	} else {
		/* 
		 * If this EP has been allocated by the provider, clean it up;
		 * see DAT 1.1 spec, page 100, lines 3-4 (section 6.4.3.1.1.1).
		 * RSP and user-provided EPs are in the control of the user.
		 */
		sp_ptr = cr_ptr->sp_ptr;
		if (ep_ptr != NULL &&
		    sp_ptr->psp_flags == DAT_PSP_PROVIDER_FLAG) {
			(void)dapl_ep_free(ep_ptr);
		}

		/* Remove the CR from the queue, then free it */
		dapl_os_lock(&sp_ptr->header.lock);
		dapl_sp_remove_cr(sp_ptr, cr_ptr);
		dapl_os_unlock(&sp_ptr->header.lock);

		dapls_cr_free(cr_ptr);
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
