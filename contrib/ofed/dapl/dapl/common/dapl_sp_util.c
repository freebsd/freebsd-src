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
 * MODULE: dapl_sp_util.c
 *
 * PURPOSE: Manage PSP Info structure
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ep_util.h"
#include "dapl_sp_util.h"
#include "dapl_cr_util.h"

/*
 * Local definitions
 */

/*
 * dapl_sp_alloc
 *
 * alloc and initialize a PSP INFO struct
 *
 * Input:
 * 	IA INFO struct ptr
 *
 * Output:
 * 	sp_ptr
 *
 * Returns:
 * 	NULL
 *	pointer to sp info struct
 *
 */
DAPL_SP *dapls_sp_alloc(IN DAPL_IA * ia_ptr, IN DAT_BOOLEAN is_psp)
{
	DAPL_SP *sp_ptr;

	/* Allocate EP */
	sp_ptr = (DAPL_SP *) dapl_os_alloc(sizeof(DAPL_SP));
	if (sp_ptr == NULL) {
		return (NULL);
	}

	/* zero the structure */
	dapl_os_memzero(sp_ptr, sizeof(DAPL_SP));

	/*
	 * initialize the header
	 */
	sp_ptr->header.provider = ia_ptr->header.provider;
	if (is_psp) {
		sp_ptr->header.magic = DAPL_MAGIC_PSP;
		sp_ptr->header.handle_type = DAT_HANDLE_TYPE_PSP;
	} else {
		sp_ptr->header.magic = DAPL_MAGIC_RSP;
		sp_ptr->header.handle_type = DAT_HANDLE_TYPE_RSP;
	}
	sp_ptr->header.owner_ia = ia_ptr;
	sp_ptr->header.user_context.as_64 = 0;
	sp_ptr->header.user_context.as_ptr = NULL;
	dapl_llist_init_entry(&sp_ptr->header.ia_list_entry);
	dapl_os_lock_init(&sp_ptr->header.lock);

#if defined(_VENDOR_IBAL_)
	dapl_os_wait_object_init(&sp_ptr->wait_object);
#endif
	/*
	 * Initialize the Body (set to NULL above)
	 */
	dapl_llist_init_head(&sp_ptr->cr_list_head);

	return (sp_ptr);
}

/*
 * dapl_sp_free
 *
 * Free the passed in PSP structure.
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
void dapls_sp_free_sp(IN DAPL_SP * sp_ptr)
{
	dapl_os_assert(sp_ptr->header.magic == DAPL_MAGIC_PSP ||
		       sp_ptr->header.magic == DAPL_MAGIC_RSP);
	dapl_os_assert(dapl_llist_is_empty(&sp_ptr->cr_list_head));

#if defined(_VENDOR_IBAL_)
	dapl_os_wait_object_destroy(&sp_ptr->wait_object);
#endif
	dapl_os_lock(&sp_ptr->header.lock);
	sp_ptr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */
	dapl_os_unlock(&sp_ptr->header.lock);
	dapl_os_free(sp_ptr, sizeof(DAPL_SP));
}

/*
 * dapl_cr_link_cr
 *
 * Add a cr to a PSP structure
 *
 * Input:
 *	sp_ptr
 *	cr_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_sp_link_cr(IN DAPL_SP * sp_ptr, IN DAPL_CR * cr_ptr)
{
	dapl_os_lock(&sp_ptr->header.lock);
	dapl_llist_add_tail(&sp_ptr->cr_list_head,
			    &cr_ptr->header.ia_list_entry, cr_ptr);
	sp_ptr->cr_list_count++;
	dapl_os_unlock(&sp_ptr->header.lock);
}

/*
 * dapl_sp_search_cr
 *
 * Search for a CR on the PSP cr_list with a matching cm_handle. When
 * found, remove it from the list and update fields.
 *
 * Must be called with the sp_ptr lock taken.
 *
 * Input:
 *	sp_ptr
 *	ib_cm_handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	cr_ptr_fnd	Pointer to matching DAPL_CR
 *
 */
DAPL_CR *dapl_sp_search_cr(IN DAPL_SP * sp_ptr,
			   IN dp_ib_cm_handle_t ib_cm_handle)
{
	DAPL_CR *cr_ptr;
	DAPL_CR *cr_ptr_fnd;

	if (dapl_llist_is_empty(&sp_ptr->cr_list_head)) {
		return NULL;
	}
	cr_ptr_fnd = NULL;
	cr_ptr = (DAPL_CR *) dapl_llist_peek_head(&sp_ptr->cr_list_head);
	dapl_os_assert(cr_ptr);

	do {
		if (cr_ptr->ib_cm_handle == ib_cm_handle) {
			cr_ptr_fnd = cr_ptr;

			break;
		}
		cr_ptr = cr_ptr->header.ia_list_entry.flink->data;
	} while ((void *)cr_ptr != (void *)sp_ptr->cr_list_head->data);

	return cr_ptr_fnd;
}

/*
 * dapl_sp_remove_cr
 *
 * Remove the CR from the PSP. Done prior to freeing the CR resource.
 *
 * Must be called with the sp_ptr lock taken.
 *
 * Input:
 *	sp_ptr
 *	cr_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	void
 *
 */
void dapl_sp_remove_cr(IN DAPL_SP * sp_ptr, IN DAPL_CR * cr_ptr)
{
	if (dapl_llist_is_empty(&sp_ptr->cr_list_head)) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "**dapl_sp_remove_cr: removing from empty queue! sp %p\n",
			     sp_ptr);
		return;
	}

	dapl_llist_remove_entry(&sp_ptr->cr_list_head,
				&cr_ptr->header.ia_list_entry);
	sp_ptr->cr_list_count--;
}

/*
 * dapl_sp_remove_ep
 *
 * Remove a CR from a PSP, given an EP.
 *
 *
 * Input:
 *	ep_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	void
 *
 */
void dapl_sp_remove_ep(IN DAPL_EP * ep_ptr)
{
	DAPL_SP *sp_ptr;
	DAPL_CR *cr_ptr;

	cr_ptr = ep_ptr->cr_ptr;

	if (cr_ptr != NULL) {
		sp_ptr = cr_ptr->sp_ptr;

		dapl_os_lock(&sp_ptr->header.lock);

		/* Remove the CR from the queue */
		dapl_sp_remove_cr(sp_ptr, cr_ptr);

		dapl_os_unlock(&sp_ptr->header.lock);

		/* free memory outside of the lock */
		dapls_cr_free(cr_ptr);

		return;
	}
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
