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
 * MODULE: dapl_psp_free.c
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
 * dapl_psp_free
 *
 * uDAPL: User Direct Access Program Library Version 1.1, 6.4.1.2
 *
 * Destroy a specific instance of a Service Point.
 *
 * Input:
 * 	psp_handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API dapl_psp_free(IN DAT_PSP_HANDLE psp_handle)
{
	DAPL_IA *ia_ptr;
	DAPL_SP *sp_ptr;
	DAT_RETURN dat_status;
	DAPL_SP_STATE save_state;

	sp_ptr = (DAPL_SP *) psp_handle;
	dat_status = DAT_SUCCESS;
	/*
	 * Verify handle
	 */
	dapl_dbg_log(DAPL_DBG_TYPE_CM, ">>> dapl_psp_free %p\n", psp_handle);

	if (DAPL_BAD_HANDLE(sp_ptr, DAPL_MAGIC_PSP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PSP);
		goto bail;
	}

	ia_ptr = sp_ptr->header.owner_ia;

	DAPL_CNTR(ia_ptr->header.owner_ia, DCNT_IA_PSP_FREE);

	/* 
	 * Remove the connection listener if it has been established
	 * and there are no current connections in progress.
	 * If we defer removing the sp it becomes something of a zombie
	 * container until the last connection is disconnected, after
	 * which it will be cleaned up.
	 */
	dapl_os_lock(&sp_ptr->header.lock);

	sp_ptr->listening = DAT_FALSE;

	/* Release reference on EVD. If an error was encountered in a previous
	 * free the evd_handle will be NULL
	 */
	if (sp_ptr->evd_handle) {
		dapl_os_atomic_dec(&((DAPL_EVD *) sp_ptr->evd_handle)->
				   evd_ref_count);
		sp_ptr->evd_handle = NULL;
	}

	/*
	 * Release the base resource if there are no outstanding
	 * connections; else the last disconnect on this PSP will free it
	 * up. The PSP is used to contain CR records for each connection,
	 * which contain information necessary to disconnect.
	 */
	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     ">>> dapl_psp_free: state %d cr_list_count %d\n",
		     sp_ptr->state, sp_ptr->cr_list_count);

	if ((sp_ptr->state == DAPL_SP_STATE_PSP_LISTENING ||
	     sp_ptr->state == DAPL_SP_STATE_PSP_PENDING) &&
	    sp_ptr->cr_list_count == 0) {
		save_state = sp_ptr->state;
		sp_ptr->state = DAPL_SP_STATE_FREE;
		dapl_os_unlock(&sp_ptr->header.lock);

		dat_status = dapls_ib_remove_conn_listener(ia_ptr, sp_ptr);
		if (dat_status != DAT_SUCCESS) {
			/* revert to entry state on error */
			sp_ptr->state = save_state;
			goto bail;
		}
		dapls_ia_unlink_sp(ia_ptr, sp_ptr);
		dapls_sp_free_sp(sp_ptr);
	} else {
		/* The PSP is now in the pending state, where it will sit until
		 * the last connection terminates or the app uses the same
		 * ServiceID again, which will reactivate it.
		 */
		sp_ptr->state = DAPL_SP_STATE_PSP_PENDING;
		dapl_os_unlock(&sp_ptr->header.lock);
		dapl_dbg_log(DAPL_DBG_TYPE_CM,
			     ">>> dapl_psp_free: PSP PENDING\n");
	}

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
