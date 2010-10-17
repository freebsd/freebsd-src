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
 * MODULE: dapl_psp_create.c
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
#include "dapl_adapter_util.h"

/*
 * dapl_psp_create
 *
 * uDAPL: User Direct Access Program Library Version 1.1, 6.4.1.1
 *
 * Create a persistent Public Service Point that can receive multiple
 * requests for connections and generate multiple connection request
 * instances that will be delivered to the specified Event Dispatcher
 * in a notification event.
 *
 * Input:
 * 	ia_handle
 * 	conn_qual
 * 	evd_handle
 * 	psp_flags
 *
 * Output:
 * 	psp_handle
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 * 	DAT_CONN_QUAL_IN_USE
 * 	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_psp_create(IN DAT_IA_HANDLE ia_handle,
		IN DAT_CONN_QUAL conn_qual,
		IN DAT_EVD_HANDLE evd_handle,
		IN DAT_PSP_FLAGS psp_flags, OUT DAT_PSP_HANDLE * psp_handle)
{
	DAPL_IA *ia_ptr;
	DAPL_SP *sp_ptr;
	DAPL_EVD *evd_ptr;
	DAT_BOOLEAN sp_found;
	DAT_RETURN dat_status;

	ia_ptr = (DAPL_IA *) ia_handle;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(ia_ptr, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}
	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EVD_CR);
		goto bail;
	}

	if (psp_handle == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG5);
		goto bail;
	}

	evd_ptr = (DAPL_EVD *) evd_handle;
	if (!(evd_ptr->evd_flags & DAT_EVD_CR_FLAG)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EVD_CR);
		goto bail;
	}

	if (psp_flags != DAT_PSP_CONSUMER_FLAG &&
	    psp_flags != DAT_PSP_PROVIDER_FLAG) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}

	DAPL_CNTR(ia_ptr, DCNT_IA_PSP_CREATE);

	/*
	 * See if we have a quiescent listener to use for this PSP, else
	 * create one and set it listening
	 */
	sp_ptr = dapls_ia_sp_search(ia_ptr, conn_qual, DAT_TRUE);
	sp_found = DAT_TRUE;
	if (sp_ptr == NULL) {
		/* Allocate PSP */
		sp_found = DAT_FALSE;
		sp_ptr = dapls_sp_alloc(ia_ptr, DAT_TRUE);
		if (sp_ptr == NULL) {
			dat_status =
			    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				      DAT_RESOURCE_MEMORY);
			goto bail;
		}
	} else if (sp_ptr->listening == DAT_TRUE) {
		dat_status = DAT_ERROR(DAT_CONN_QUAL_IN_USE, 0);
		goto bail;
	}

	/*
	 * Fill out the args for a PSP
	 */
	sp_ptr->conn_qual = conn_qual;
	sp_ptr->evd_handle = evd_handle;
	sp_ptr->psp_flags = psp_flags;
	sp_ptr->ep_handle = NULL;

	/*
	 * Take a reference on the EVD handle
	 */
	dapl_os_atomic_inc(&((DAPL_EVD *) evd_handle)->evd_ref_count);

	/* 
	 * Set up a listener for a connection. Connections can arrive
	 * even before this call returns!
	 */
	sp_ptr->state = DAPL_SP_STATE_PSP_LISTENING;
	sp_ptr->listening = DAT_TRUE;

	/*
	 * If this is a new sp we need to add it to the IA queue, and set up
	 * a conn_listener.
	 */
	if (sp_found == DAT_FALSE) {
		/* Link it onto the IA before enabling it to receive conn
		 * requests
		 */
		dapl_ia_link_psp(ia_ptr, sp_ptr);

		dat_status = dapls_ib_setup_conn_listener(ia_ptr,
							  conn_qual, sp_ptr);

		if (dat_status != DAT_SUCCESS) {
			/*
			 * Have a problem setting up the connection, something
			 * wrong!  Decrements the EVD refcount & release it.
			 */
			dapl_os_atomic_dec(&((DAPL_EVD *) evd_handle)->
					   evd_ref_count);
			sp_ptr->evd_handle = NULL;
			dapls_ia_unlink_sp(ia_ptr, sp_ptr);
			dapls_sp_free_sp(sp_ptr);

			dapl_dbg_log(DAPL_DBG_TYPE_CM,
				     "--> dapl_psp_create setup_conn_listener failed: %x\n",
				     dat_status);

			goto bail;
		}
	}

	/*
	 * Return handle to the user
	 */
	*psp_handle = (DAT_PSP_HANDLE) sp_ptr;

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
