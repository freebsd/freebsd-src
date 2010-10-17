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
 * MODULE: dapl_ia_query.c
 *
 * PURPOSE: Interface Adapter management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 2
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_vendor.h"

/*
 * dapl_ia_query
 *
 * DAPL Requirements Version xxx, 6.2.1.3
 *
 * Provide the consumer with Interface Adapter and Provider parameters.
 *
 * Input:
 *	ia_handle
 *	ia_mask
 *	provider_mask
 *
 * Output:
 *	async_evd_handle
 *	ia_parameters
 *	provider_parameters
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 */
DAT_RETURN DAT_API
dapl_ia_query(IN DAT_IA_HANDLE ia_handle,
	      OUT DAT_EVD_HANDLE * async_evd_handle,
	      IN DAT_IA_ATTR_MASK ia_attr_mask,
	      OUT DAT_IA_ATTR * ia_attr,
	      IN DAT_PROVIDER_ATTR_MASK provider_attr_mask,
	      OUT DAT_PROVIDER_ATTR * provider_attr)
{
	DAPL_IA *ia_ptr;
	DAT_RETURN dat_status;
	struct evd_merge_type {
		DAT_BOOLEAN array[6][6];
	} *evd_merge;
	DAT_BOOLEAN val;
	int i;
	int j;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ia_query (%p, %p, 0x%llx, %p, 0x%x, %p)\n",
		     ia_handle,
		     async_evd_handle,
		     ia_attr_mask, ia_attr, provider_attr_mask, provider_attr);

	ia_ptr = (DAPL_IA *) ia_handle;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(ia_ptr, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}

	if (NULL != async_evd_handle) {
		*async_evd_handle = ia_ptr->async_error_evd;
	}

	if (ia_attr_mask & DAT_IA_FIELD_ALL) {
		if (NULL == ia_attr) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
			goto bail;
		}

		/*
		 * Obtain parameters from the HCA.  Protect against multiple
		 * IAs beating on the HCA at the same time.
		 */
		dat_status =
		    dapls_ib_query_hca(ia_ptr->hca_ptr, ia_attr, NULL, NULL);
		if (dat_status != DAT_SUCCESS) {
			goto bail;
		}
	}

	if (ia_attr_mask & ~DAT_IA_FIELD_ALL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	if (provider_attr_mask & DAT_PROVIDER_FIELD_ALL) {
		if (NULL == provider_attr) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG6);
			goto bail;
		}

		strncpy(provider_attr->provider_name,
			ia_ptr->header.provider->device_name,
			DAT_NAME_MAX_LENGTH);
		provider_attr->provider_version_major = VN_PROVIDER_MAJOR;
		provider_attr->provider_version_minor = VN_PROVIDER_MINOR;
		provider_attr->dapl_version_major = DAT_VERSION_MAJOR;
		provider_attr->dapl_version_minor = DAT_VERSION_MINOR;
		provider_attr->lmr_mem_types_supported =
		    DAT_MEM_TYPE_VIRTUAL | DAT_MEM_TYPE_LMR;
#if VN_MEM_SHARED_VIRTUAL_SUPPORT > 0 && !defined(__KDAPL__)
		provider_attr->lmr_mem_types_supported |=
		    DAT_MEM_TYPE_SHARED_VIRTUAL;
#endif
		provider_attr->iov_ownership_on_return = DAT_IOV_CONSUMER;
		provider_attr->dat_qos_supported = DAT_QOS_BEST_EFFORT;
		provider_attr->completion_flags_supported =
		    DAT_COMPLETION_DEFAULT_FLAG;
		provider_attr->is_thread_safe = DAT_FALSE;
		/*
		 * N.B. The second part of the following equation will evaluate
		 *      to 0 unless IBHOSTS_NAMING is enabled.
		 */
		provider_attr->max_private_data_size =
		    dapls_ib_private_data_size(NULL, DAPL_PDATA_CONN_REQ,
					       ia_ptr->hca_ptr) -
		    (sizeof(DAPL_PRIVATE) - DAPL_MAX_PRIVATE_DATA_SIZE);
		provider_attr->supports_multipath = DAT_FALSE;
		provider_attr->ep_creator = DAT_PSP_CREATES_EP_NEVER;
		provider_attr->optimal_buffer_alignment = DAT_OPTIMAL_ALIGNMENT;
		/* The value of pz_support may vary by transport */
		provider_attr->num_provider_specific_attr = 0;
		provider_attr->provider_specific_attr = NULL;
#if !defined(__KDAPL__)
		provider_attr->pz_support = DAT_PZ_UNIQUE;
#endif				/* !KDAPL */

		/*
		 *  Query for provider specific attributes
		 */
		dapls_query_provider_specific_attr(ia_ptr, provider_attr);

		/*
		 * Set up evd_stream_merging_supported options. Note there is
		 * one bit per allowable combination, using the ordinal
		 * position of the DAT_EVD_FLAGS as positions in the
		 * array. e.g.
		 * [0][0] is DAT_EVD_SOFTWARE_FLAG | DAT_EVD_SOFTWARE_FLAG,
		 * [0][1] is DAT_EVD_SOFTWARE_FLAG | DAT_EVD_CR_FLAG, and
		 * [2][4] is DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG
		 *
		 * Most combinations are true, so initialize the array that way.
		 * Then finish by resetting the bad combinations.
		 *
		 * DAT_EVD_ASYNC_FLAG is not supported. InfiniBand only allows
		 * a single asynchronous event handle per HCA, and the first
		 * dat_ia_open forces the creation of the only one that can be
		 * used. We disallow the user from creating an ASYNC EVD here.
		 */

		evd_merge =
		    (struct evd_merge_type *)&provider_attr->
		    evd_stream_merging_supported[0][0];
		val = DAT_TRUE;
		for (i = 0; i < 6; i++) {
			if (i > 4) {
				/* ASYNC EVD is 5, so entire row will be 0 */
				val = DAT_FALSE;
			}
			for (j = 0; j < 5; j++) {
				evd_merge->array[i][j] = val;
			}
			/* Set the ASYNC_EVD column to FALSE */
			evd_merge->array[i][5] = DAT_FALSE;
		}

#ifndef DAPL_MERGE_CM_DTO
		/*
		 * If an implementation supports CM and DTO completions on
		 * the same EVD then DAPL_MERGE_CM_DTO should be set to
		 * skip the following code
		 */
		/* DAT_EVD_DTO_FLAG | DAT_EVD_CONNECTION_FLAG */
		evd_merge->array[2][3] = DAT_FALSE;
		/* DAT_EVD_CONNECTION_FLAG | DAT_EVD_DTO_FLAG */
		evd_merge->array[3][2] = DAT_FALSE;
#endif				/* DAPL_MERGE_CM_DTO */
	}

      bail:
	if (dat_status != DAT_SUCCESS) {
		dapl_dbg_log(DAPL_DBG_TYPE_RTN,
			     "dapl_ia_query () returns 0x%x\n", dat_status);
	}

	return dat_status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
