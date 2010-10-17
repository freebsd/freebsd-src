/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
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
 * MODULE: dapl_cno_query.c
 *
 * PURPOSE: Return the consumer parameters of the CNO
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 3.2.5
 *
 * $Id: dapl_cno_query.c 1301 2005-03-24 05:58:55Z jlentini $
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_cno_query
 *
 * DAPL Requirements Version xxx, 6.3.2.5
 *
 * Return the consumer parameters of the CNO
 *
 * Input:
 *	cno_handle
 *	cno_param_mask
 *	cno_param
 *
 * Output:
 *	cno_param
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API dapl_cno_query(IN DAT_CNO_HANDLE cno_handle,	/* cno_handle */
				  IN DAT_CNO_PARAM_MASK cno_param_mask,	/* cno_param_mask */
				  OUT DAT_CNO_PARAM * cno_param)

{				/* cno_param */
	DAPL_CNO *cno_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(cno_handle, DAPL_MAGIC_CNO)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CNO);
		goto bail;
	}

	if (NULL == cno_param) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	cno_ptr = (DAPL_CNO *) cno_handle;
	cno_param->ia_handle = cno_ptr->header.owner_ia;
	cno_param->proxy_type = DAT_PROXY_TYPE_AGENT;
	cno_param->proxy.agent = cno_ptr->cno_wait_agent;

      bail:
	return dat_status;
}
