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
 * MODULE: dapl_srq_query.c
 *
 * PURPOSE: Shared Receive Queue management
 *
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.2 API, Chapter 6, section 5.6
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_srq_query
 *
 * DAPL Requirements Version 1.2, 6.5.6
 *
 * Return SRQ parameters to the consumer
 *
 * Input:
 * 	srq_handle
 *      srq_param_mask
 *
 * Output:
 * 	srq_param
 *
 * Returns:
 * 	DAT_SUCCESS
 *      DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_srq_query(IN DAT_SRQ_HANDLE srq_handle,
	       IN DAT_SRQ_PARAM_MASK srq_param_mask,
	       OUT DAT_SRQ_PARAM * srq_param)
{
	DAPL_SRQ *srq_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_srq_query (%p, %x, %p)\n",
		     srq_handle, srq_param_mask, srq_param);

	if (DAPL_BAD_HANDLE(srq_handle, DAPL_MAGIC_SRQ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
		goto bail;
	}
	if (srq_param == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	srq_ptr = (DAPL_SRQ *) srq_handle;

	/*
	 * XXX Need to calculate available_dto_count and outstanding_dto_count
	 */
	srq_ptr->param.available_dto_count = DAT_VALUE_UNKNOWN;
	srq_ptr->param.outstanding_dto_count = DAT_VALUE_UNKNOWN;

	*srq_param = srq_ptr->param;

      bail:
	return dat_status;
}
