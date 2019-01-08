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
 * MODULE: dapl_ep_free.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5.4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"
#include "dapl_ring_buffer_util.h"
#include "dapl_timer_util.h"

/*
 * dapl_ep_free
 *
 * DAPL Requirements Version xxx, 6.5.3
 *
 * Destroy an instance of the Endpoint
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
DAT_RETURN DAT_API dapl_ep_free(IN DAT_EP_HANDLE ep_handle)
{
	DAPL_EP *ep_ptr;
	DAPL_IA *ia_ptr;
	DAT_EP_PARAM *param;
	ib_qp_state_t save_qp_state;
	DAT_RETURN dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API | DAPL_DBG_TYPE_CM,
		     "dapl_ep_free (%p)\n", ep_handle);

	ep_ptr = (DAPL_EP *) ep_handle;
	param = &ep_ptr->param;

	/*
	 * Verify parameter & state
	 */
	if (DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}
	DAPL_CNTR(ep_ptr->header.owner_ia, DCNT_IA_EP_FREE);

	if (ep_ptr->param.ep_state == DAT_EP_STATE_RESERVED ||
	    ep_ptr->param.ep_state == DAT_EP_STATE_PASSIVE_CONNECTION_PENDING ||
	    ep_ptr->param.ep_state == DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING)
	{
		dapl_dbg_log(DAPL_DBG_TYPE_WARN,
			     "--> dapl_ep_free: invalid state: %x, ep %p\n",
			     ep_ptr->param.ep_state, ep_ptr);
		dat_status = DAT_ERROR(DAT_INVALID_STATE,
				       dapls_ep_state_subtype(ep_ptr));
		goto bail;
	}

	ia_ptr = ep_ptr->header.owner_ia;

	/* If we are connected, issue a disconnect. If we are in the
	 * disconnect_pending state, disconnect with the ABRUPT flag
	 * set.
	 */

	/*
	 * Invoke ep_disconnect to clean up outstanding connections
	 */
	(void)dapl_ep_disconnect(ep_ptr, DAT_CLOSE_ABRUPT_FLAG);

	/*
	 * Do verification of parameters and the state change atomically.
	 */
	dapl_os_lock(&ep_ptr->header.lock);

#ifdef DAPL_DBG
	/* check if event pending and warn, don't assert, state is valid */
	if (ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECT_PENDING) {
		dapl_dbg_log(DAPL_DBG_TYPE_WARN, " dat_ep_free WARNING: "
			     "EVENT PENDING on ep %p, disconnect "
			     "and wait before calling dat_ep_free\n", ep_ptr);
	}
#endif

	if (ep_ptr->cxn_timer != NULL) {
		dapls_timer_cancel(ep_ptr->cxn_timer);
		dapl_os_free(ep_ptr->cxn_timer, sizeof(DAPL_OS_TIMER));
		ep_ptr->cxn_timer = NULL;
	}

	/* Remove the EP from the IA */
	dapl_ia_unlink_ep(ia_ptr, ep_ptr);

	/*
	 * Update ref counts. Note the user may have used ep_modify
	 * to set handles to NULL. Set handles to NULL so this routine
	 * is idempotent.
	 */
	if (param->pz_handle != NULL) {
		dapl_os_atomic_dec(&((DAPL_PZ *) param->pz_handle)->
				   pz_ref_count);
		param->pz_handle = NULL;
	}
	if (param->recv_evd_handle != NULL) {
		dapl_os_atomic_dec(&((DAPL_EVD *) param->recv_evd_handle)->
				   evd_ref_count);
		param->recv_evd_handle = NULL;
	}
	if (param->request_evd_handle != NULL) {
		dapl_os_atomic_dec(&((DAPL_EVD *) param->request_evd_handle)->
				   evd_ref_count);
		param->request_evd_handle = NULL;
	}
	if (param->connect_evd_handle != NULL) {
		dapl_os_atomic_dec(&((DAPL_EVD *) param->connect_evd_handle)->
				   evd_ref_count);
		param->connect_evd_handle = NULL;
	}

	/*
	 * Finish tearing everything down.
	 */
	dapl_dbg_log(DAPL_DBG_TYPE_EP | DAPL_DBG_TYPE_CM,
		     "dapl_ep_free: Free EP: %x, ep %p qp_state %x qp_handle %x\n",
		     ep_ptr->param.ep_state,
		     ep_ptr, ep_ptr->qp_state, ep_ptr->qp_handle);
	/*
	 * Take care of the transport resource. Make a copy of the qp_state
	 * to prevent race conditions when we exit the lock.
	 */
	save_qp_state = ep_ptr->qp_state;
	ep_ptr->qp_state = DAPL_QP_STATE_UNATTACHED;
	dapl_os_unlock(&ep_ptr->header.lock);

	/* Free the QP. If the EP has never been used, the QP is invalid */
	if (save_qp_state != DAPL_QP_STATE_UNATTACHED) {
		dat_status = dapls_ib_qp_free(ia_ptr, ep_ptr);
		/* This should always succeed, but report to the user if
		 * there is a problem. The qp_state must be restored so
		 * they can try it again in the face of EINTR or similar
		 * where the QP is OK but the call couldn't complete.
		 */
		if (dat_status != DAT_SUCCESS) {
			ep_ptr->qp_state = save_qp_state;
			goto bail;
		}
	}

	/* Free the resource */
	dapl_ep_dealloc(ep_ptr);

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
