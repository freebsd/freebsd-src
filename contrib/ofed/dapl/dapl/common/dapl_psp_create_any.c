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
 * dapl_psp_create_any
 *
 * uDAPL: User Direct Access Program Library Version 1.1, 6.4.3.3
 *
 * Create a persistent Public Service Point that can receive multiple
 * requests for connections and generate multiple connection request
 * instances that will be delivered to the specified Event Dispatcher
 * in a notification event. Differs from dapl_psp_create() in that
 * the conn_qual is selected by the implementation and returned to
 * the user.
 *
 * Input:
 * 	ia_handle
 * 	evd_handle
 * 	psp_flags
 *
 * Output:
 * 	conn_qual
 * 	psp_handle
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 * 	DAT_CONN_QUAL_IN_USE
 * 	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_psp_create_any(IN DAT_IA_HANDLE ia_handle,
		    OUT DAT_CONN_QUAL * conn_qual,
		    IN DAT_EVD_HANDLE evd_handle,
		    IN DAT_PSP_FLAGS psp_flags, OUT DAT_PSP_HANDLE * psp_handle)
{
	DAPL_IA *ia_ptr;
	DAPL_SP *sp_ptr;
	DAPL_EVD *evd_ptr;
	DAT_RETURN dat_status;
	static DAT_CONN_QUAL hint_conn_qual = 1024;	/* seed value */
	DAT_CONN_QUAL lcl_conn_qual;
	DAT_CONN_QUAL limit_conn_qual;

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
	if (conn_qual == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
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

	/* Allocate PSP */
	sp_ptr = dapls_sp_alloc(ia_ptr, DAT_TRUE);
	if (sp_ptr == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	DAPL_CNTR(ia_ptr, DCNT_IA_PSP_CREATE_ANY);

	/*
	 * Fill out the args for a PSP
	 */
	sp_ptr->evd_handle = evd_handle;
	sp_ptr->psp_flags = psp_flags;
	sp_ptr->ep_handle = NULL;

	/*
	 * Take a reference on the EVD handle
	 */
	dapl_os_atomic_inc(&((DAPL_EVD *) evd_handle)->evd_ref_count);

	/* Link it onto the IA */
	dapl_ia_link_psp(ia_ptr, sp_ptr);

	/* 
	 * Set up a listener for a connection. Connections can arrive
	 * even before this call returns!
	 */
	sp_ptr->state = DAPL_SP_STATE_PSP_LISTENING;
	sp_ptr->listening = DAT_TRUE;

	limit_conn_qual = 0;
	lcl_conn_qual = hint_conn_qual;
	dat_status = ~DAT_SUCCESS;

	while (dat_status != DAT_SUCCESS) {
		dat_status = dapls_ib_setup_conn_listener(ia_ptr,
							  lcl_conn_qual,
							  sp_ptr);

		lcl_conn_qual++;

		if (dat_status == DAT_CONN_QUAL_IN_USE) {
			/*
			 * If we have a big number of tries and we still haven't
			 * found a service_ID we can use, bail out with an error,
			 * something is wrong!
			 */
			if (limit_conn_qual++ > 100000) {
				dat_status = DAT_CONN_QUAL_UNAVAILABLE;
				break;
			}
		}
	}
	hint_conn_qual = lcl_conn_qual;

	if (dat_status != DAT_SUCCESS) {
		/*
		 * Have a problem setting up the connection, something wrong!
		 */
		dapl_os_atomic_dec(&((DAPL_EVD *) evd_handle)->evd_ref_count);
		sp_ptr->evd_handle = NULL;
		dapls_ia_unlink_sp(ia_ptr, sp_ptr);
		dapls_sp_free_sp(sp_ptr);

		dapl_os_printf
		    ("--> dapl_psp_create cannot set up conn listener: %x\n",
		     dat_status);

		goto bail;
	}

	sp_ptr->conn_qual = lcl_conn_qual - 1;

	/*
	 * Return handle to the user
	 */
	*conn_qual = sp_ptr->conn_qual;
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
