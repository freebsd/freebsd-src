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
 * MODULE: dapl_srq_create.c
 *
 * PURPOSE: Shared Receive Queue management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.2 API, Chapter 6, section 5.1
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"
#include "dapl_srq_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_srq_create
 *
 * DAPL Version 1.2, 6.5.1
 *
 * Create an instance of a Shared Receive Queue that is provided to the
 * consumer at srq_handle.
 *
 * Input:
 *	ia_handle
 *	pz_handle
 *	srq_attr
 *
 * Output:
 *	srq_handle
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_PARAMETER
 *	?DAT_INVALID_ATTRIBUTE??
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_srq_create(IN DAT_IA_HANDLE ia_handle,
		IN DAT_PZ_HANDLE pz_handle,
		IN DAT_SRQ_ATTR * srq_attr, OUT DAT_SRQ_HANDLE * srq_handle)
{
	DAPL_IA *ia_ptr;
	DAPL_SRQ *srq_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_srq_create (%p, %p, %p, %p)\n",
		     ia_handle, pz_handle, srq_attr, srq_handle);

	ia_ptr = (DAPL_IA *) ia_handle;

	/*
	 * Verify parameters
	 */
	if (DAPL_BAD_HANDLE(ia_ptr, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}

	DAPL_CNTR(ia_ptr, DCNT_IA_SRQ_CREATE);

	/*
	 * Verify non-required parameters.
	 * N.B. Assumption: any parameter that can be
	 *      modified by dat_ep_modify() is not strictly
	 *      required when the EP is created
	 */
	if (pz_handle != NULL && DAPL_BAD_HANDLE(pz_handle, DAPL_MAGIC_PZ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
		goto bail;
	}

	if (srq_handle == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}
	if (DAPL_BAD_PTR(srq_attr)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	/* Allocate SRQ */
	srq_ptr = dapl_srq_alloc(ia_ptr, srq_attr);
	if (srq_ptr == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	srq_ptr->param.ia_handle = (DAT_IA_HANDLE) ia_ptr;
	srq_ptr->param.srq_state = DAT_SRQ_STATE_OPERATIONAL;
	srq_ptr->param.pz_handle = pz_handle;

	/*
	 * XXX Allocate provider resource here!!!
	 */
	/* XXX */ dat_status = DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
	/* XXX */ dapl_srq_dealloc(srq_ptr);
	/* XXX */ goto bail;

	/* Link it onto the IA */
	dapl_ia_link_srq(ia_ptr, srq_ptr);

	*srq_handle = srq_ptr;

      bail:
	return dat_status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
