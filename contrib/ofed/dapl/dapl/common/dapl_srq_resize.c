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
 * MODULE: dapl_srq_resize.c
 *
 * PURPOSE: Shared Receive Queue management
 *
 * Description: Interfaces in this file are completely defined in 
 *              the uDAPL 1.1 API, Chapter 6, section 5.7
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_srq_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_srq_resize
 *
 * DAPL Requirements Version 1.2, 6.5.7
 *
 * Modify the size of the event queue of a Shared Receive Queue
 *
 * Input:
 * 	srq_handle
 * 	srq_max_recv_dto
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_STATE
 */

DAT_RETURN DAT_API
dapl_srq_resize(IN DAT_SRQ_HANDLE srq_handle, IN DAT_COUNT srq_max_recv_dto)
{
	DAPL_IA *ia_ptr;
	DAPL_SRQ *srq_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_srq_resize (%p, %d)\n",
		     srq_handle, srq_max_recv_dto);

	if (DAPL_BAD_HANDLE(srq_handle, DAPL_MAGIC_SRQ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
		goto bail;
	}

	srq_ptr = (DAPL_SRQ *) srq_handle;
	ia_ptr = srq_ptr->header.owner_ia;

	/*
	 * Check for nonsense requests per the spec
	 */
	if (srq_max_recv_dto <= srq_ptr->param.low_watermark) {
		dat_status = DAT_ERROR(DAT_INVALID_STATE, DAT_NO_SUBTYPE);
		goto bail;
	}

	/* XXX Put implementation here XXX */

	/* XXX */ dat_status = DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);

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
