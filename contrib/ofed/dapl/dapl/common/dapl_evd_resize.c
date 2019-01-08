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
 * MODULE: dapl_evd_resize.c
 *
 * PURPOSE: EVENT management
 *
 * Description: Interfaces in this file are completely defined in 
 *              the uDAPL 1.1 API, Chapter 6, section 3
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_evd_util.h"
#include "dapl_ring_buffer_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_evd_resize
 *
 * DAPL Requirements Version xxx, 6.3.2.5
 *
 * Modify the size of the event queue of an Event Dispatcher
 *
 * Input:
 * 	evd_handle
 * 	evd_qlen
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_STATE
 */

DAT_RETURN DAT_API dapl_evd_resize(IN DAT_EVD_HANDLE evd_handle,
				   IN DAT_COUNT evd_qlen)
{
	DAPL_IA *ia_ptr;
	DAPL_EVD *evd_ptr;
	DAT_COUNT pend_cnt;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_evd_resize (%p, %d)\n",
		     evd_handle, evd_qlen);

	if (DAPL_BAD_HANDLE(evd_handle, DAPL_MAGIC_EVD)) {
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE1);
		goto bail;
	}

	evd_ptr = (DAPL_EVD *) evd_handle;
	ia_ptr = evd_ptr->header.owner_ia;

	if (evd_qlen == evd_ptr->qlen) {
		dat_status = DAT_SUCCESS;
		goto bail;
	}

	if (evd_qlen > ia_ptr->hca_ptr->ia_attr.max_evd_qlen) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	dapl_os_lock(&evd_ptr->header.lock);

	/* Don't try to resize if we are actively waiting */
	if (evd_ptr->evd_state == DAPL_EVD_STATE_WAITED) {
		dapl_os_unlock(&evd_ptr->header.lock);
		dat_status = DAT_ERROR(DAT_INVALID_STATE, 0);
		goto bail;
	}

	pend_cnt = dapls_rbuf_count(&evd_ptr->pending_event_queue);
	if (pend_cnt > evd_qlen) {
		dapl_os_unlock(&evd_ptr->header.lock);
		dat_status = DAT_ERROR(DAT_INVALID_STATE, 0);
		goto bail;
	}

	dat_status = dapls_ib_cq_resize(evd_ptr->header.owner_ia,
					evd_ptr, &evd_qlen);
	if (dat_status != DAT_SUCCESS) {
		dapl_os_unlock(&evd_ptr->header.lock);
		goto bail;
	}

	dat_status = dapls_evd_event_realloc(evd_ptr, evd_qlen);
	if (dat_status != DAT_SUCCESS) {
		dapl_os_unlock(&evd_ptr->header.lock);
		goto bail;
	}

	dapl_os_unlock(&evd_ptr->header.lock);

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
