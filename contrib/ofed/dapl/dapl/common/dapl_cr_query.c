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
 * MODULE: dapl_cr_query.c
 *
 * PURPOSE: Connection management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_adapter_util.h"

/*
 * dapl_cr_query
 *
 * DAPL Requirements Version xxx, 6.4.2.1
 *
 * Return Connection Request args
 *
 * Input:
 *	cr_handle
 *	cr_param_mask
 *
 * Output:
 *	cr_param
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_HANDLE
 */
DAT_RETURN DAT_API
dapl_cr_query(IN DAT_CR_HANDLE cr_handle,
	      IN DAT_CR_PARAM_MASK cr_param_mask, OUT DAT_CR_PARAM * cr_param)
{
	DAPL_CR *cr_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_cr_query (%p, %x, %p)\n",
		     cr_handle, cr_param_mask, cr_param);

	dat_status = DAT_SUCCESS;
	if (DAPL_BAD_HANDLE(cr_handle, DAPL_MAGIC_CR)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CR);
		goto bail;
	}

	if (NULL == cr_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	cr_ptr = (DAPL_CR *) cr_handle;

	/* obtain the remote IP address */
	if (cr_param_mask & DAT_CR_FIELD_REMOTE_IA_ADDRESS_PTR) {
		dat_status = dapls_ib_cm_remote_addr((DAT_HANDLE) cr_handle,
						     &cr_ptr->
						     remote_ia_address);
	}

	/* since the arguments are easily accessible, ignore the mask */
	dapl_os_memcpy(cr_param, &cr_ptr->param, sizeof(DAT_CR_PARAM));

      bail:
	return dat_status;
}
