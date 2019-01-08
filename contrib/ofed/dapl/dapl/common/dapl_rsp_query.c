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
 * MODULE: dapl_rsp_query.c
 *
 * PURPOSE: Connection management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_rsp_query
 *
 * uDAPL: User Direct Access Program Library Version 1.1, 6.4.1.6
 *
 * Provide arguments of the reserved service points
 *
 * Input:
 *	rsp_handle
 *	rsp_args_mask
 *
 * Output:
 *	rsp_args
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_rsp_query(IN DAT_RSP_HANDLE rsp_handle,
	       IN DAT_RSP_PARAM_MASK rsp_mask, OUT DAT_RSP_PARAM * rsp_param)
{
	DAPL_SP *sp_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(rsp_handle, DAPL_MAGIC_RSP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RSP);
		goto bail;
	}

	if (NULL == rsp_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	sp_ptr = (DAPL_SP *) rsp_handle;

	/*
	 * Fill in the RSP params
	 */
	rsp_param->ia_handle = sp_ptr->header.owner_ia;
	rsp_param->conn_qual = sp_ptr->conn_qual;
	rsp_param->evd_handle = sp_ptr->evd_handle;
	rsp_param->ep_handle = sp_ptr->ep_handle;

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
