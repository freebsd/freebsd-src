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
 * MODULE: dapl_ep_set_watermark.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.2 API, Chapter 6, section 6.13
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ep_util.h"

/*
 * dapl_ep_set_watermark
 *
 * DAPL Requirements Version 1.2, 6.6.13
 *
 * Sets the watermark values for an EP for generating asynchronous
 * events.
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
 *	DAT_INVALID_HANDLE
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_ep_set_watermark(IN DAT_EP_HANDLE ep_handle,
		      IN DAT_COUNT soft_high_watermark,
		      IN DAT_COUNT hard_high_watermark)
{
	DAPL_EP *ep_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API, "dapl_ep_set_watermark (%p, %d, %d)\n",
		     ep_handle, soft_high_watermark, hard_high_watermark);

	ep_ptr = (DAPL_EP *) ep_handle;

	/*
	 * Verify parameter & state
	 */
	if (DAPL_BAD_HANDLE(ep_ptr, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	dat_status = DAT_NOT_IMPLEMENTED;

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
