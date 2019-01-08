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
 * MODULE: dapl_rmr_query.c
 *
 * PURPOSE: Memory management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 6
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_rmr_query
 *
 * DAPL Requirements Version xxx, 6.6.4.3
 *
 * Provide the RMR arguments.
 *
 * Input:
 * 	rmr_handle
 * 	rmr_args_mask
 *
 * Output:
 * 	rmr_args
 *
 * Returns:
 * 	DAT_SUCCESS
 *      DAT_INVALID_HANDLE
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_rmr_query(IN DAT_RMR_HANDLE rmr_handle,
	       IN DAT_RMR_PARAM_MASK rmr_param_mask,
	       IN DAT_RMR_PARAM * rmr_param)
{
	DAPL_RMR *rmr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(rmr_handle, DAPL_MAGIC_RMR)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RMR);
		goto bail;
	}
	if (NULL == rmr_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	rmr = (DAPL_RMR *) rmr_handle;

	/* If the RMR is unbound, there is no LMR triplet associated with   */
	/* this RMR.  If the consumer requests this field, return an error. */
	if ((rmr_param_mask & DAT_RMR_FIELD_LMR_TRIPLET) && (NULL == rmr->lmr)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	dapl_os_memcpy(rmr_param, &rmr->param, sizeof(DAT_RMR_PARAM));

      bail:
	return dat_status;
}
