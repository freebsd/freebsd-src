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
 * MODULE: dapl_evd_cq_async_error_callback.c
 *
 * PURPOSE: implements CQ async_callbacks from verbs
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_evd_util.h"

/*
 * dapl_evd_cq_async_error_callback
 *
 * The callback function registered with verbs for cq async errors
 *
 * Input:
 * 	ib_cm_handle,
 * 	ib_cm_event
 * 	cause_ptr
 * 	context (evd)
 *
 * Output:
 * 	None
 *
 */

void
dapl_evd_cq_async_error_callback(IN ib_hca_handle_t ib_hca_handle,
				 IN ib_cq_handle_t ib_cq_handle,
				 IN ib_error_record_t * cause_ptr,
				 IN void *context)
{
	DAPL_EVD *async_evd;
	DAPL_EVD *evd;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK | DAPL_DBG_TYPE_EXCEPTION,
		     "dapl_evd_cq_async_error_callback (%p, %p, %p, %p)\n",
		     ib_hca_handle, ib_cq_handle, cause_ptr, context);

	if (NULL == context) {
		dapl_os_panic("NULL == context\n");
	}

	evd = (DAPL_EVD *) context;
	async_evd = evd->header.owner_ia->async_error_evd;
	DAPL_CNTR(evd->header.owner_ia, DCNT_IA_ASYNC_CQ_ERROR);

	dat_status = dapls_evd_post_async_error_event(async_evd,
						      DAT_ASYNC_ERROR_EVD_OVERFLOW,
						      (DAT_IA_HANDLE)
						      async_evd->header.
						      owner_ia);

	if (dat_status != DAT_SUCCESS) {
		dapl_os_panic("async EVD overflow\n");
	}

	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK | DAPL_DBG_TYPE_EXCEPTION,
		     "dapl_evd_cq_async_error_callback () returns\n");
}
