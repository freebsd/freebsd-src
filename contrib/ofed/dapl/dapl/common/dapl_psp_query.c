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
 * MODULE: dapl_psp_query.c
 *
 * PURPOSE: Connection management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_psp_query
 *
 * uDAPL: User Direct Access Program Library Version 1.1, 6.4.1.3
 *
 * Provide arguments of the public service points
 *
 * Input:
 *	psp_handle
 *	psp_args_mask
 *
 * Output:
 * 	psp_args
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_psp_query(IN DAT_PSP_HANDLE psp_handle,
	       IN DAT_PSP_PARAM_MASK psp_args_mask,
	       OUT DAT_PSP_PARAM * psp_param)
{
	DAPL_SP *sp_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(psp_handle, DAPL_MAGIC_PSP) ||
	    ((DAPL_SP *) psp_handle)->listening != DAT_TRUE) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PSP);
		goto bail;
	}

	if (NULL == psp_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	sp_ptr = (DAPL_SP *) psp_handle;

	/*
	 * Fill in the PSP params
	 */
	psp_param->ia_handle = sp_ptr->header.owner_ia;
	psp_param->conn_qual = sp_ptr->conn_qual;
	psp_param->evd_handle = sp_ptr->evd_handle;
	psp_param->psp_flags = sp_ptr->psp_flags;

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
