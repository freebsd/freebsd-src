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
 * MODULE: dapl_hca_util.c
 *
 * PURPOSE: Manage HCA structure
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_provider.h"
#include "dapl_hca_util.h"
#include "dapl_hash.h"

/*
 * dapl_hca_alloc
 *
 * alloc and initialize an HCA struct
 *
 * Input:
 * 	name
 *      port
 *
 * Output:
 * 	hca_ptr
 *
 * Returns:
 * 	none
 *
 */
DAPL_HCA *dapl_hca_alloc(char *name, char *port)
{
	DAPL_HCA *hca_ptr;

	hca_ptr = dapl_os_alloc(sizeof(DAPL_HCA));
	if (NULL == hca_ptr) {
		goto bail;
	}

	dapl_os_memzero(hca_ptr, sizeof(DAPL_HCA));

	if (DAT_SUCCESS !=
	    dapls_hash_create(DAPL_HASH_TABLE_DEFAULT_CAPACITY,
			      &hca_ptr->lmr_hash_table)) {
		goto bail;
	}

	dapl_os_lock_init(&hca_ptr->lock);
	dapl_llist_init_head(&hca_ptr->ia_list_head);

	hca_ptr->name = dapl_os_strdup(name);
	if (NULL == hca_ptr->name) {
		goto bail;
	}

	hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
	hca_ptr->port_num = dapl_os_strtol(port, NULL, 0);

	return (hca_ptr);

      bail:
	if (NULL != hca_ptr) {
		if (NULL != hca_ptr->lmr_hash_table) {
			dapls_hash_free(hca_ptr->lmr_hash_table);
		}

		dapl_os_free(hca_ptr, sizeof(DAPL_HCA));
	}

	return NULL;
}

/*
 * dapl_hca_free
 *
 * free an IA INFO struct
 *
 * Input:
 * 	hca_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_hca_free(DAPL_HCA * hca_ptr)
{
	(void)dapls_hash_free(hca_ptr->lmr_hash_table);
	dapl_os_free(hca_ptr->name, dapl_os_strlen(hca_ptr->name) + 1);
	dapl_os_free(hca_ptr, sizeof(DAPL_HCA));
}

/*
 * dapl_hca_link_ia
 *
 * Add an ia to the HCA structure
 *
 * Input:
 *	hca_ptr
 *	ia_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_hca_link_ia(IN DAPL_HCA * hca_ptr, IN DAPL_IA * ia_ptr)
{
	dapl_os_lock(&hca_ptr->lock);
	dapl_llist_add_head(&hca_ptr->ia_list_head,
			    &ia_ptr->hca_ia_list_entry, ia_ptr);
	dapl_os_unlock(&hca_ptr->lock);
}

/*
 * dapl_hca_unlink_ia
 *
 * Remove an ia from the hca info structure
 *
 * Input:
 *	hca_ptr
 *	ia_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_hca_unlink_ia(IN DAPL_HCA * hca_ptr, IN DAPL_IA * ia_ptr)
{
	dapl_os_lock(&hca_ptr->lock);
	/*
	 * If an error occurred when we were opening the IA it
	 * will not be linked on the list; don't unlink an unlinked
	 * list!
	 */
	if (!dapl_llist_is_empty(&hca_ptr->ia_list_head)) {
		dapl_llist_remove_entry(&hca_ptr->ia_list_head,
					&ia_ptr->hca_ia_list_entry);
	}
	dapl_os_unlock(&hca_ptr->lock);
}
