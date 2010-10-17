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
 * MODULE: dapl_pz_util.c
 *
 * PURPOSE: Manage PZ structure
 *
 * $Id:$
 **********************************************************************/

#include "dapl_pz_util.h"
#include "dapl_ia_util.h"

/*
 * dapl_pz_alloc
 *
 * alloc and initialize an PZ struct
 *
 * Input:
 * 	none
 *
 * Output:
 * 	pz_ptr
 *
 * Returns:
 * 	none
 *
 */
DAPL_PZ *dapl_pz_alloc(IN DAPL_IA * ia)
{
	DAPL_PZ *pz;

	/* Allocate PZ */
	pz = (DAPL_PZ *) dapl_os_alloc(sizeof(DAPL_PZ));
	if (NULL == pz) {
		return (NULL);
	}

	/* zero the structure */
	dapl_os_memzero(pz, sizeof(DAPL_PZ));

	/*
	 * initialize the header
	 */
	pz->header.provider = ia->header.provider;
	pz->header.magic = DAPL_MAGIC_PZ;
	pz->header.handle_type = DAT_HANDLE_TYPE_PZ;
	pz->header.owner_ia = ia;
	pz->header.user_context.as_64 = 0;
	pz->header.user_context.as_ptr = NULL;
	dapl_llist_init_entry(&pz->header.ia_list_entry);
	dapl_ia_link_pz(ia, pz);
	dapl_os_lock_init(&pz->header.lock);

	/* 
	 * initialize the body 
	 */
	dapl_os_atomic_set(&pz->pz_ref_count, 0);

	return (pz);
}

/*
 * dapl_pz_free
 *
 * free an PZ struct
 *
 * Input:
 * 	pz_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_pz_dealloc(IN DAPL_PZ * pz)
{
	pz->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */
	dapl_ia_unlink_pz(pz->header.owner_ia, pz);
	dapl_os_lock_destroy(&pz->header.lock);

	dapl_os_free(pz, sizeof(DAPL_PZ));
}
