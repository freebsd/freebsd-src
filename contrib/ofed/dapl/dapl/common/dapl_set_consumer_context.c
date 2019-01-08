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
 * MODULE: dapl_set_consumer_context.c
 *
 * PURPOSE: Interface Adapter management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 2
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"

/*
 * dapl_set_consumer_context
 *
 * DAPL Requirements Version xxx, 6.2.2.1
 *
 * Set a consumer context in the provided dat_handle
 *
 * Input:
 *	dat_handle
 *	context
 *
 * Output:
 *	none
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_HANDLE
 */
DAT_RETURN DAT_API
dapl_set_consumer_context(IN DAT_HANDLE dat_handle, IN DAT_CONTEXT context)
{
	DAT_RETURN dat_status;
	DAPL_HEADER *header;

	dat_status = DAT_SUCCESS;

	header = (DAPL_HEADER *) dat_handle;
	if (((header) == NULL) ||
	    DAPL_BAD_PTR(header) ||
	    (header->magic != DAPL_MAGIC_IA &&
	     header->magic != DAPL_MAGIC_EVD &&
	     header->magic != DAPL_MAGIC_EP &&
	     header->magic != DAPL_MAGIC_LMR &&
	     header->magic != DAPL_MAGIC_RMR &&
	     header->magic != DAPL_MAGIC_PZ &&
	     header->magic != DAPL_MAGIC_PSP &&
	     header->magic != DAPL_MAGIC_RSP &&
	     header->magic != DAPL_MAGIC_CR)) {
		dat_status = DAT_ERROR(DAT_INVALID_HANDLE, 0);
		goto bail;
	}
	header->user_context = context;

      bail:
	return dat_status;
}
