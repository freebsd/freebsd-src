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
 * MODULE: dapl_ep_util.c
 *
 * PURPOSE: Shared Receive Queue management and support
 *
 * $Id:$
 **********************************************************************/

#include "dapl_srq_util.h"
#include "dapl_ia_util.h"
#include "dapl_cookie.h"

/*
 * dapl_srq_alloc
 *
 * alloc and initialize an SRQ struct
 *
 * Input:
 * 	IA INFO struct ptr
 *	SRQ ATTR ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	pointer to srq
 *
 */
DAPL_SRQ *dapl_srq_alloc(IN DAPL_IA * ia_ptr, IN const DAT_SRQ_ATTR * srq_attr)
{
	DAPL_SRQ *srq_ptr;

	/* Allocate SRQ */
	srq_ptr = (DAPL_SRQ *) dapl_os_alloc(sizeof(DAPL_SRQ));

	if (srq_ptr == NULL) {
		goto bail;
	}

	/* zero the structure */
	dapl_os_memzero(srq_ptr, sizeof(DAPL_SRQ));

	/*
	 * initialize the header
	 */
	srq_ptr->header.provider = ia_ptr->header.provider;
	srq_ptr->header.magic = DAPL_MAGIC_SRQ;
	srq_ptr->header.handle_type = DAT_HANDLE_TYPE_SRQ;
	srq_ptr->header.owner_ia = ia_ptr;
	srq_ptr->header.user_context.as_64 = 0;
	srq_ptr->header.user_context.as_ptr = NULL;
	dapl_os_atomic_set(&srq_ptr->srq_ref_count, 0);

	dapl_llist_init_entry(&srq_ptr->header.ia_list_entry);
	dapl_os_lock_init(&srq_ptr->header.lock);

	/*
	 * Initialize the body. 
	 * XXX Assume srq_attrs is required
	 */
	srq_ptr->param.max_recv_dtos = srq_attr->max_recv_dtos;
	srq_ptr->param.max_recv_iov = srq_attr->max_recv_iov;
	srq_ptr->param.low_watermark = srq_attr->low_watermark;

	/* Get a cookie buffer to track outstanding recvs */
	if (DAT_SUCCESS != dapls_cb_create(&srq_ptr->recv_buffer, (DAPL_EP *) srq_ptr,	/* just saves the value */
					   srq_ptr->param.max_recv_dtos)) {
		dapl_srq_dealloc(srq_ptr);
		srq_ptr = NULL;
		goto bail;
	}

      bail:
	return srq_ptr;
}

/*
 * dapl_srq_dealloc
 *
 * Free the passed in SRQ structure.
 *
 * Input:
 * 	SRQ pointer
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_srq_dealloc(IN DAPL_SRQ * srq_ptr)
{
	dapl_os_assert(srq_ptr->header.magic == DAPL_MAGIC_SRQ);

	srq_ptr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */
	dapl_ia_unlink_srq(srq_ptr->header.owner_ia, srq_ptr);
	dapls_cb_free(&srq_ptr->recv_buffer);
	dapl_os_lock_destroy(&srq_ptr->header.lock);

	dapl_os_free(srq_ptr, sizeof(DAPL_SRQ));
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
