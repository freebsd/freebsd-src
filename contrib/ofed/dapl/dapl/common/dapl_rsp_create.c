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
 * MODULE: dapl_rsp_create.c
 *
 * PURPOSE: Connection management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_sp_util.h"
#include "dapl_ia_util.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_rsp_create
 *
 * uDAPL: User Direct Access Program Library Version 1.1, 6.4.3.4.1
 *
 * Create a Resereved Service Point with the specified Endpoint
 * that generates at most one Connection Request that is
 * delivered to the specified Event Dispatcher in a notification
 * event
 *
 * Input:
 *	ia_handle
 *	conn_qual
 *	ep_handle
 *	evd_handle
 *
 * Output:
 *	rsp_handle
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_STATE
 *	DAT_CONN_QUAL_IN_USE
 */
DAT_RETURN DAT_API
dapl_rsp_create(IN DAT_IA_HANDLE ia_handle,
		IN DAT_CONN_QUAL conn_qual,
		IN DAT_EP_HANDLE ep_handle,
		IN DAT_EVD_HANDLE evd_handle, OUT DAT_RSP_HANDLE * rsp_handle)
{
	DAPL_IA *ia_ptr;
	DAPL_SP *sp_ptr;
	DAPL_EVD *evd_ptr;
	DAPL_EP *ep_ptr;
	DAT_BOOLEAN sp_found;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;
	ia_ptr = (DAPL_IA *) ia_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     ">>> dapl_rsp_free conn_qual: %x EP: %p\n",
		     conn_qual, ep_handle);

	if (DAPL_BAD_HANDLE(ia_ptr, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}
	if (DAPL_BAD_HANDLE(ep_handle, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}
	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EVD_CR);
		goto bail;
	}

	if (rsp_handle == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG5);
		goto bail;
	}

	ep_ptr = (DAPL_EP *) ep_handle;
	if (ep_ptr->param.ep_state != DAT_EP_STATE_UNCONNECTED) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      dapls_ep_state_subtype(ep_ptr));
		goto bail;
	}

	evd_ptr = (DAPL_EVD *) evd_handle;
	if (!(evd_ptr->evd_flags & DAT_EVD_CR_FLAG)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EVD_CR);
		goto bail;
	}

	DAPL_CNTR(ia_ptr, DCNT_IA_RSP_CREATE);

	sp_ptr = dapls_ia_sp_search(ia_ptr, conn_qual, DAT_FALSE);
	sp_found = DAT_TRUE;
	if (sp_ptr == NULL) {
		sp_found = DAT_FALSE;

		/* Allocate RSP */
		sp_ptr = dapls_sp_alloc(ia_ptr, DAT_FALSE);
		if (sp_ptr == NULL) {
			dat_status =
			    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				      DAT_RESOURCE_MEMORY);
			goto bail;
		}
	}

	/*
	 * Fill out the RSP args
	 */
	sp_ptr->conn_qual = conn_qual;
	sp_ptr->evd_handle = evd_handle;
	sp_ptr->psp_flags = 0;
	sp_ptr->ep_handle = ep_handle;

	/*
	 * Take a reference on the EVD handle
	 */
	dapl_os_atomic_inc(&((DAPL_EVD *) evd_handle)->evd_ref_count);

	/*
	 * Update the EP state indicating the provider now owns it
	 */
	ep_ptr->param.ep_state = DAT_EP_STATE_RESERVED;

	/* 
	 * Set up a listener for a connection. Connections can arrive
	 * even before this call returns!
	 */
	sp_ptr->state = DAPL_SP_STATE_RSP_LISTENING;
	sp_ptr->listening = DAT_TRUE;

	if (sp_found == DAT_FALSE) {
		/* Link it onto the IA */
		dapl_ia_link_rsp(ia_ptr, sp_ptr);

		dat_status = dapls_ib_setup_conn_listener(ia_ptr,
							  conn_qual, sp_ptr);

		if (dat_status != DAT_SUCCESS) {
			/*
			 * Have a problem setting up the connection, something
			 * wrong!  Decrements the EVD refcount & release it. Set 
			 * the state to FREE, so we know the call failed.
			 */
			dapl_os_atomic_dec(&((DAPL_EVD *) evd_handle)->
					   evd_ref_count);
			sp_ptr->evd_handle = NULL;
			sp_ptr->state = DAPL_SP_STATE_FREE;
			dapls_ia_unlink_sp(ia_ptr, sp_ptr);
			dapls_sp_free_sp(sp_ptr);

			dapl_dbg_log(DAPL_DBG_TYPE_CM,
				     "--> dapl_rsp_create setup_conn_listener failed: %x\n",
				     dat_status);

			goto bail;
		}
	}

	/*
	 * Return handle to the user
	 */
	*rsp_handle = (DAT_RSP_HANDLE) sp_ptr;

      bail:
	return dat_status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  c-brace-offset: -4
 *  tab-width: 8
 * End:
 */
