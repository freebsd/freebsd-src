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
 * MODULE: dapl_cr_util.c
 *
 * PURPOSE: Manage CR (Connection Request) structure
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_cr_util.h"

/*
 * dapls_cr_create
 *
 * Create a CR. Part of the passive side of a connection
 *
 * Input:
 * 	ia_ptr
 *
 * Returns:
 * 	DAPL_CR
 *
 */

DAPL_CR *dapls_cr_alloc(DAPL_IA * ia_ptr)
{
	DAPL_CR *cr_ptr;

	/* Allocate EP */
	cr_ptr = (DAPL_CR *) dapl_os_alloc(sizeof(DAPL_CR));
	if (cr_ptr == NULL) {
		return (NULL);
	}

	/* zero the structure */
	dapl_os_memzero(cr_ptr, sizeof(DAPL_CR));

	/*
	 * initialize the header
	 */
	cr_ptr->header.provider = ia_ptr->header.provider;
	cr_ptr->header.magic = DAPL_MAGIC_CR;
	cr_ptr->header.handle_type = DAT_HANDLE_TYPE_CR;
	cr_ptr->header.owner_ia = ia_ptr;
	cr_ptr->header.user_context.as_64 = 0;
	cr_ptr->header.user_context.as_ptr = NULL;
	dapl_llist_init_entry(&cr_ptr->header.ia_list_entry);
	dapl_os_lock_init(&cr_ptr->header.lock);

	return (cr_ptr);
}

/*
 * dapls_cr_free
 *
 * Free the passed in EP structure.
 *
 * Input:
 * 	entry point pointer
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapls_cr_free(IN DAPL_CR * cr_ptr)
{
	dapl_os_assert(cr_ptr->header.magic == DAPL_MAGIC_CR ||
		       cr_ptr->header.magic == DAPL_MAGIC_CR_DESTROYED);

	cr_ptr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */
	dapl_os_free(cr_ptr, sizeof(DAPL_CR));
}
