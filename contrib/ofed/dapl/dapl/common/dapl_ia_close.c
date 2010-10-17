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
 * MODULE: dapl_ia_close.c
 *
 * PURPOSE: Interface Adapter management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 2
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"

/*
 * dapl_ia_close
 *
 * DAPL Requirements Version xxx, 6.2.1.2
 *
 * Close a provider, clean up resources, etc.
 *
 * Input:
 *	ia_handle
 *
 * Output:
 *	none
 *
 * Return Values:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_ia_close(IN DAT_IA_HANDLE ia_handle, IN DAT_CLOSE_FLAGS ia_flags)
{
	DAPL_IA *ia_ptr;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ia_close (%p, %d)\n", ia_handle, ia_flags);

	ia_ptr = (DAPL_IA *) ia_handle;

	if (DAPL_BAD_HANDLE(ia_ptr, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}

	if (DAT_CLOSE_ABRUPT_FLAG == ia_flags) {
		dat_status = dapl_ia_abrupt_close(ia_ptr);
	} else if (DAT_CLOSE_GRACEFUL_FLAG == ia_flags) {
		dat_status = dapl_ia_graceful_close(ia_ptr);
	} else {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
	}

      bail:
	return dat_status;
}
