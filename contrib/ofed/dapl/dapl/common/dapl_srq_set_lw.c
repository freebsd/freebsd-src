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
 * MODULE: dapl_srq_set_lw.c
 *
 * PURPOSE: Shared Receive Queue management
 *
 * Description: Interfaces in this file are completely defined in 
 *              the uDAPL 1.1 API, Chapter 6, section 5.4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_srq_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_srq_set_lw
 *
 * DAPL Requirements Version 1.2, 6.5.4
 *
 * Set the low water mark for an SRQ and arm the SRQ to generate an
 * event if it is reached.
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
 * 	DAT_MODEL_NOT_SUPPORTED
 */

DAT_RETURN DAT_API
dapl_srq_set_lw(IN DAT_SRQ_HANDLE srq_handle, IN DAT_COUNT low_watermark)
{
	DAPL_SRQ *srq_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_srq_set_lw (%p, %d)\n",
		     srq_handle, low_watermark);

	if (DAPL_BAD_HANDLE(srq_handle, DAPL_MAGIC_SRQ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
		goto bail;
	}

	srq_ptr = (DAPL_SRQ *) srq_handle;

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
