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
 * MODULE: dapl_srq_free.c
 *
 * PURPOSE: Shared Receive Queue management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5.5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"
#include "dapl_srq_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_srq_free
 *
 * DAPL Version 1.2, 6.5.5
 *
 * Destroy an instance of an SRQ
 *
 * Input:
 *	srq_handle
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_STATE
 */
DAT_RETURN DAT_API dapl_srq_free(IN DAT_SRQ_HANDLE srq_handle)
{
	DAPL_SRQ *srq_ptr;
	DAPL_IA *ia_ptr;
	DAT_SRQ_PARAM *param;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_srq_free (%p)\n", srq_handle);

	srq_ptr = (DAPL_SRQ *) srq_handle;
	param = &srq_ptr->param;

	/*
	 * Verify parameter & state
	 */
	if (DAPL_BAD_HANDLE(srq_ptr, DAPL_MAGIC_SRQ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
		goto bail;
	}

	if (dapl_os_atomic_read(&srq_ptr->srq_ref_count) != 0) {
		/*
		 * The DAPL 1.2 spec says to return DAT_SRQ_IN_USE, which does
		 * not exist. Have filed the following as an errata.
		 */
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_SRQ_IN_USE);
		goto bail;
	}

	ia_ptr = srq_ptr->header.owner_ia;

	DAPL_CNTR(ia_ptr, DCNT_IA_SRQ_FREE);

	/*
	 * Do verification of parameters and the state change atomically.
	 */
	dapl_os_lock(&srq_ptr->header.lock);

	/* Remove the SRQ from the IA */
	dapl_ia_unlink_srq(ia_ptr, srq_ptr);

	dapl_os_unlock(&srq_ptr->header.lock);

	/*
	 * Finish tearing everything down.
	 */

	/*
	 * Take care of the transport resource
	 */

	/* XXX Put provider code here!!! */

	/* Free the resource */
	dapl_srq_dealloc(srq_ptr);

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
