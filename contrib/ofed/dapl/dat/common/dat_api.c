/*
 * Copyright (c) 2002-2006, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under all of the following licenses:
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
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of Network Appliance, Inc. nor the names of other DAT
 * Collaborative contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 */

/**********************************************************************
 *
 * MODULE: dat_api.c
 *
 * PURPOSE: DAT Provider and Consumer registry functions.
 *	    Also provide small integers for IA_HANDLES
 *
 * $Id: dat_api.c 1326 2005-05-20 22:25:31Z jlentini $
 **********************************************************************/

#include <dat2/udat.h>
#include <dat2/dat_registry.h>
#include "dat_osd.h"
#include "dat_init.h"

/*
 * structure to deal with IA handles
 */
typedef struct {
	DAT_OS_LOCK handle_lock;
	unsigned long handle_max;
	void **handle_array;
} DAT_HANDLE_VEC;

/*
 * Number of IA Handle entries allocated at a time.
 */
#define DAT_HANDLE_ENTRY_STEP	64

static DAT_HANDLE_VEC g_hv;

/***********************************************************************
 * Function: dats_handle_init
 *
 * Initialize a handle_vector. Happens when the module initializes
 ***********************************************************************/
DAT_RETURN dats_handle_vector_init(void)
{
	DAT_RETURN dat_status;
	unsigned long i;

	dat_status = DAT_SUCCESS;

	g_hv.handle_max = DAT_HANDLE_ENTRY_STEP;

	dat_status = dat_os_lock_init(&g_hv.handle_lock);
	if (DAT_SUCCESS != dat_status) {
		return dat_status;
	}

	g_hv.handle_array =
	    dat_os_alloc(sizeof(void *) * DAT_HANDLE_ENTRY_STEP);
	if (g_hv.handle_array == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	for (i = 0; i < g_hv.handle_max; i++) {
		g_hv.handle_array[i] = NULL;
	}

      bail:
	return dat_status;
}

/***********************************************************************
 * Function: dats_set_ia_handle
 *
 * Install an ia_handle into a handle vector and return a small
 * integer.
 ***********************************************************************/
DAT_IA_HANDLE dats_set_ia_handle(IN DAT_IA_HANDLE ia_handle)
{
	unsigned long i;
	void **h;

	dat_os_lock(&g_hv.handle_lock);

	/*
	 * Don't give out handle zero since that is DAT_HANDLE_NULL!
	 */
	for (i = 1; i < g_hv.handle_max; i++) {
		if (g_hv.handle_array[i] == NULL) {
			g_hv.handle_array[i] = ia_handle;

			dat_os_unlock(&g_hv.handle_lock);

			dat_os_dbg_print(DAT_OS_DBG_TYPE_PROVIDER_API,
					 "dat_set_handle %p to %d\n", ia_handle,
					 i);
			return DAT_UL_TO_IA_HANDLE(i);
		}
	}

	/* Didn't find an entry, grow the list & try again */
	dat_os_dbg_print(DAT_OS_DBG_TYPE_PROVIDER_API,
			 "Growing the handle array from %d to %d\n",
			 g_hv.handle_max,
			 g_hv.handle_max + DAT_HANDLE_ENTRY_STEP);

	/* reallocate the handle array */
	h = dat_os_alloc(sizeof(void *) *
			 (g_hv.handle_max + DAT_HANDLE_ENTRY_STEP));
	if (h == NULL) {
		dat_os_unlock(&g_hv.handle_lock);
		return DAT_UL_TO_IA_HANDLE(-1);
	}
	/* copy old data to new area & free old memory */
	memcpy((void *)h, (void *)g_hv.handle_array,
	       sizeof(void *) * g_hv.handle_max);
	dat_os_free(g_hv.handle_array, sizeof(void *) * g_hv.handle_max);
	g_hv.handle_array = h;

	/* Initialize the new entries */
	for (i = g_hv.handle_max; i < g_hv.handle_max + DAT_HANDLE_ENTRY_STEP;
	     i++) {
		g_hv.handle_array[i] = NULL;
	}
	i = g_hv.handle_max;
	g_hv.handle_array[i] = ia_handle;
	g_hv.handle_max += DAT_HANDLE_ENTRY_STEP;
	dat_os_unlock(&g_hv.handle_lock);

	dat_os_dbg_print(DAT_OS_DBG_TYPE_PROVIDER_API,
			 "dat_set_handle x %p to %d\n", ia_handle, i);

	return DAT_UL_TO_IA_HANDLE(i);
}

/***********************************************************************
 * Function: dats_get_ia_handle(
 *
 * Get a handle from a handle vector and return it in an OUT parameter
 ***********************************************************************/
DAT_RETURN
dats_get_ia_handle(IN DAT_IA_HANDLE handle, OUT DAT_IA_HANDLE * ia_handle_p)
{
	DAT_RETURN dat_status;

	if (DAT_IA_HANDLE_TO_UL(handle) >= g_hv.handle_max) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}
	*ia_handle_p = g_hv.handle_array[DAT_IA_HANDLE_TO_UL(handle)];

	if (*ia_handle_p == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}
	dat_status = DAT_SUCCESS;

      bail:
	dat_os_dbg_print(DAT_OS_DBG_TYPE_PROVIDER_API,
			 "dat_get_ia_handle from %d to %p\n",
			 handle, *ia_handle_p);

	return dat_status;
}

/***********************************************************************
 * Function: dats_is_ia_handle
 *
 * Unlike other handles, a DAT_IA_HANDLE is a small integer. Therefore, 
 * we must be able to distinguish between a DAT_IA_HANDLE and other
 * types of handles (which are actually pointers).
 * 
 * The current implementation assumes that any value for which an IA 
 * handle exists is a DAT_IA_HANDLE. Unfortunately this will result in 
 * false positives. In particular it may identify a NULL pointer as IA 
 * handle 0. An implementation that does not have this deficiency would 
 * be preferable.
 *
 ***********************************************************************/
DAT_RETURN dats_is_ia_handle(IN DAT_HANDLE dat_handle)
{
	unsigned long handle = DAT_IA_HANDLE_TO_UL((DAT_IA_HANDLE) dat_handle);

	if (g_hv.handle_max <= handle) {
		return DAT_FALSE;
	} else if (NULL == g_hv.handle_array[handle]) {
		return DAT_FALSE;
	} else {
		return DAT_TRUE;
	}
}

/***********************************************************************
 * Function: dats_free_ia_handle
 *
 * Free a handle in a handle vector
 ***********************************************************************/
DAT_RETURN dats_free_ia_handle(IN DAT_IA_HANDLE handle)
{
	DAT_RETURN dat_status;

	if (DAT_IA_HANDLE_TO_UL(handle) >= g_hv.handle_max) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}
	g_hv.handle_array[DAT_IA_HANDLE_TO_UL(handle)] = NULL;
	dat_status = DAT_SUCCESS;

	dat_os_dbg_print(DAT_OS_DBG_TYPE_PROVIDER_API,
			 "dats_free_ia_handle %d\n", handle);

      bail:
	return dat_status;
}

/**********************************************************************
 * API definitions for common API entry points
 **********************************************************************/
DAT_RETURN DAT_API dat_ia_query(IN DAT_IA_HANDLE ia_handle,
				OUT DAT_EVD_HANDLE * async_evd_handle,
				IN DAT_IA_ATTR_MASK ia_attr_mask,
				OUT DAT_IA_ATTR * ia_attr,
				IN DAT_PROVIDER_ATTR_MASK provider_attr_mask,
				OUT DAT_PROVIDER_ATTR * provider_attr)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_IA_QUERY(dapl_ia_handle,
					  async_evd_handle,
					  ia_attr_mask,
					  ia_attr,
					  provider_attr_mask, provider_attr);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_set_consumer_context(IN DAT_HANDLE dat_handle,
					    IN DAT_CONTEXT context)
{
	if (dats_is_ia_handle(dat_handle)) {
		DAT_IA_HANDLE dapl_ia_handle;
		DAT_RETURN dat_status;

		dat_status = dats_get_ia_handle((DAT_IA_HANDLE) dat_handle,
						&dapl_ia_handle);

		/* failure to map the handle is unlikely but possible */
		/* in a mult-threaded environment                     */
		if (DAT_SUCCESS == dat_status) {
			return DAT_ERROR(DAT_INVALID_HANDLE,
					 DAT_INVALID_HANDLE1);
		}

		dat_handle = dapl_ia_handle;
	}

	return DAT_SET_CONSUMER_CONTEXT(dat_handle, context);
}

DAT_RETURN DAT_API dat_get_consumer_context(IN DAT_HANDLE dat_handle,
					    OUT DAT_CONTEXT * context)
{
	if (dats_is_ia_handle(dat_handle)) {
		DAT_IA_HANDLE dapl_ia_handle;
		DAT_RETURN dat_status;

		dat_status = dats_get_ia_handle((DAT_IA_HANDLE) dat_handle,
						&dapl_ia_handle);

		/* failure to map the handle is unlikely but possible */
		/* in a mult-threaded environment                     */
		if (DAT_SUCCESS == dat_status) {
			return DAT_ERROR(DAT_INVALID_HANDLE,
					 DAT_INVALID_HANDLE1);
		}

		dat_handle = dapl_ia_handle;
	}

	return DAT_GET_CONSUMER_CONTEXT(dat_handle, context);
}

DAT_RETURN DAT_API dat_get_handle_type(IN DAT_HANDLE dat_handle,
				       OUT DAT_HANDLE_TYPE * type)
{
	if (dats_is_ia_handle(dat_handle)) {
		DAT_IA_HANDLE dapl_ia_handle;
		DAT_RETURN dat_status;

		dat_status = dats_get_ia_handle((DAT_IA_HANDLE) dat_handle,
						&dapl_ia_handle);

		/* failure to map the handle is unlikely but possible */
		/* in a mult-threaded environment                     */
		if (DAT_SUCCESS == dat_status) {
			return DAT_ERROR(DAT_INVALID_HANDLE,
					 DAT_INVALID_HANDLE1);
		}

		dat_handle = dapl_ia_handle;
	}

	return DAT_GET_HANDLE_TYPE(dat_handle, type);
}

DAT_RETURN DAT_API dat_cr_query(IN DAT_CR_HANDLE cr_handle,
				IN DAT_CR_PARAM_MASK cr_param_mask,
				OUT DAT_CR_PARAM * cr_param)
{
	if (cr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CR);
	}
	return DAT_CR_QUERY(cr_handle, cr_param_mask, cr_param);
}

DAT_RETURN DAT_API dat_cr_accept(IN DAT_CR_HANDLE cr_handle,
				 IN DAT_EP_HANDLE ep_handle,
				 IN DAT_COUNT private_data_size,
				 IN const DAT_PVOID private_data)
{
	if (cr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CR);
	}
	return DAT_CR_ACCEPT(cr_handle,
			     ep_handle, private_data_size, private_data);
}

DAT_RETURN DAT_API dat_cr_reject(IN DAT_CR_HANDLE cr_handle,
				 IN DAT_COUNT private_data_size,
				 IN const DAT_PVOID private_data)
{
	if (cr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CR);
	}
	return DAT_CR_REJECT(cr_handle, private_data_size, private_data);
}

DAT_RETURN DAT_API dat_evd_resize(IN DAT_EVD_HANDLE evd_handle,
				  IN DAT_COUNT evd_min_qlen)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_RESIZE(evd_handle, evd_min_qlen);
}

DAT_RETURN DAT_API dat_evd_post_se(IN DAT_EVD_HANDLE evd_handle,
				   IN const DAT_EVENT * event)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_POST_SE(evd_handle, event);
}

DAT_RETURN DAT_API dat_evd_dequeue(IN DAT_EVD_HANDLE evd_handle,
				   OUT DAT_EVENT * event)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_DEQUEUE(evd_handle, event);
}

DAT_RETURN DAT_API dat_evd_free(IN DAT_EVD_HANDLE evd_handle)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_FREE(evd_handle);
}

DAT_RETURN DAT_API dat_evd_query(IN DAT_EVD_HANDLE evd_handle,
				 IN DAT_EVD_PARAM_MASK evd_param_mask,
				 OUT DAT_EVD_PARAM * evd_param)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_QUERY(evd_handle, evd_param_mask, evd_param);
}

DAT_RETURN DAT_API dat_ep_create(IN DAT_IA_HANDLE ia_handle,
				 IN DAT_PZ_HANDLE pz_handle,
				 IN DAT_EVD_HANDLE recv_completion_evd_handle,
				 IN DAT_EVD_HANDLE
				 request_completion_evd_handle,
				 IN DAT_EVD_HANDLE connect_evd_handle,
				 IN const DAT_EP_ATTR * ep_attributes,
				 OUT DAT_EP_HANDLE * ep_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_EP_CREATE(dapl_ia_handle,
					   pz_handle,
					   recv_completion_evd_handle,
					   request_completion_evd_handle,
					   connect_evd_handle,
					   ep_attributes, ep_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_ep_query(IN DAT_EP_HANDLE ep_handle,
				IN DAT_EP_PARAM_MASK ep_param_mask,
				OUT DAT_EP_PARAM * ep_param)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_QUERY(ep_handle, ep_param_mask, ep_param);
}

DAT_RETURN DAT_API dat_ep_modify(IN DAT_EP_HANDLE ep_handle,
				 IN DAT_EP_PARAM_MASK ep_param_mask,
				 IN const DAT_EP_PARAM * ep_param)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_MODIFY(ep_handle, ep_param_mask, ep_param);
}

DAT_RETURN DAT_API dat_ep_connect(IN DAT_EP_HANDLE ep_handle,
				  IN DAT_IA_ADDRESS_PTR remote_ia_address,
				  IN DAT_CONN_QUAL remote_conn_qual,
				  IN DAT_TIMEOUT timeout,
				  IN DAT_COUNT private_data_size,
				  IN const DAT_PVOID private_data,
				  IN DAT_QOS quality_of_service,
				  IN DAT_CONNECT_FLAGS connect_flags)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_CONNECT(ep_handle,
			      remote_ia_address,
			      remote_conn_qual,
			      timeout,
			      private_data_size,
			      private_data, quality_of_service, connect_flags);
}

DAT_RETURN DAT_API dat_ep_common_connect(IN DAT_EP_HANDLE ep_handle,
					 IN DAT_IA_ADDRESS_PTR
					 remote_ia_address,
					 IN DAT_TIMEOUT timeout,
					 IN DAT_COUNT private_data_size,
					 IN const DAT_PVOID private_data)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_COMMON_CONNECT(ep_handle,
				     remote_ia_address,
				     timeout, private_data_size, private_data);
}

DAT_RETURN DAT_API dat_ep_dup_connect(IN DAT_EP_HANDLE ep_handle,
				      IN DAT_EP_HANDLE ep_dup_handle,
				      IN DAT_TIMEOUT timeout,
				      IN DAT_COUNT private_data_size,
				      IN const DAT_PVOID private_data,
				      IN DAT_QOS quality_of_service)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_DUP_CONNECT(ep_handle,
				  ep_dup_handle,
				  timeout,
				  private_data_size,
				  private_data, quality_of_service);
}

DAT_RETURN DAT_API dat_ep_disconnect(IN DAT_EP_HANDLE ep_handle,
				     IN DAT_CLOSE_FLAGS close_flags)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_DISCONNECT(ep_handle, close_flags);
}

DAT_RETURN DAT_API dat_ep_post_send(IN DAT_EP_HANDLE ep_handle,
				    IN DAT_COUNT num_segments,
				    IN DAT_LMR_TRIPLET * local_iov,
				    IN DAT_DTO_COOKIE user_cookie,
				    IN DAT_COMPLETION_FLAGS completion_flags)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_POST_SEND(ep_handle,
				num_segments,
				local_iov, user_cookie, completion_flags);
}

DAT_RETURN DAT_API dat_ep_post_send_with_invalidate(IN DAT_EP_HANDLE ep_handle,
						    IN DAT_COUNT num_segments,
						    IN DAT_LMR_TRIPLET *
						    local_iov,
						    IN DAT_DTO_COOKIE
						    user_cookie,
						    IN DAT_COMPLETION_FLAGS
						    completion_flags,
						    IN DAT_BOOLEAN
						    invalidate_flag,
						    IN DAT_RMR_CONTEXT
						    rmr_context)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_POST_SEND_WITH_INVALIDATE(ep_handle,
						num_segments,
						local_iov,
						user_cookie,
						completion_flags,
						invalidate_flag, rmr_context);
}

DAT_RETURN DAT_API dat_ep_post_recv(IN DAT_EP_HANDLE ep_handle,
				    IN DAT_COUNT num_segments,
				    IN DAT_LMR_TRIPLET * local_iov,
				    IN DAT_DTO_COOKIE user_cookie,
				    IN DAT_COMPLETION_FLAGS completion_flags)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_POST_RECV(ep_handle,
				num_segments,
				local_iov, user_cookie, completion_flags);
}

DAT_RETURN DAT_API dat_ep_post_rdma_read(IN DAT_EP_HANDLE ep_handle,
					 IN DAT_COUNT num_segments,
					 IN DAT_LMR_TRIPLET * local_iov,
					 IN DAT_DTO_COOKIE user_cookie,
					 IN const DAT_RMR_TRIPLET * remote_iov,
					 IN DAT_COMPLETION_FLAGS
					 completion_flags)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_POST_RDMA_READ(ep_handle,
				     num_segments,
				     local_iov,
				     user_cookie, remote_iov, completion_flags);
}

DAT_RETURN DAT_API dat_ep_post_rdma_read_to_rmr(IN DAT_EP_HANDLE ep_handle,
						IN const DAT_RMR_TRIPLET *
						local_iov,
						IN DAT_DTO_COOKIE user_cookie,
						IN const DAT_RMR_TRIPLET *
						remote_iov,
						IN DAT_COMPLETION_FLAGS
						completion_flags)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_POST_RDMA_READ_TO_RMR(ep_handle,
					    local_iov,
					    user_cookie,
					    remote_iov, completion_flags);
}

DAT_RETURN DAT_API dat_ep_post_rdma_write(IN DAT_EP_HANDLE ep_handle,
					  IN DAT_COUNT num_segments,
					  IN DAT_LMR_TRIPLET * local_iov,
					  IN DAT_DTO_COOKIE user_cookie,
					  IN const DAT_RMR_TRIPLET * remote_iov,
					  IN DAT_COMPLETION_FLAGS
					  completion_flags)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_POST_RDMA_WRITE(ep_handle,
				      num_segments,
				      local_iov,
				      user_cookie,
				      remote_iov, completion_flags);
}

DAT_RETURN DAT_API dat_ep_get_status(IN DAT_EP_HANDLE ep_handle,
				     OUT DAT_EP_STATE * ep_state,
				     OUT DAT_BOOLEAN * recv_idle,
				     OUT DAT_BOOLEAN * request_idle)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_GET_STATUS(ep_handle, ep_state, recv_idle, request_idle);
}

DAT_RETURN DAT_API dat_ep_free(IN DAT_EP_HANDLE ep_handle)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_FREE(ep_handle);
}

DAT_RETURN DAT_API dat_ep_reset(IN DAT_EP_HANDLE ep_handle)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_RESET(ep_handle);
}

DAT_RETURN DAT_API dat_lmr_free(IN DAT_LMR_HANDLE lmr_handle)
{
	if (lmr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_LMR);
	}
	return DAT_LMR_FREE(lmr_handle);
}

DAT_RETURN DAT_API dat_rmr_create(IN DAT_PZ_HANDLE pz_handle,
				  OUT DAT_RMR_HANDLE * rmr_handle)
{
	if (pz_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
	}
	return DAT_RMR_CREATE(pz_handle, rmr_handle);
}

DAT_RETURN DAT_API dat_rmr_create_for_ep(IN DAT_PZ_HANDLE pz_handle,
					 OUT DAT_RMR_HANDLE * rmr_handle)
{
	if (pz_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
	}
	return DAT_RMR_CREATE_FOR_EP(pz_handle, rmr_handle);
}
DAT_RETURN DAT_API dat_rmr_query(IN DAT_RMR_HANDLE rmr_handle,
				 IN DAT_RMR_PARAM_MASK rmr_param_mask,
				 OUT DAT_RMR_PARAM * rmr_param)
{
	if (rmr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RMR);
	}
	return DAT_RMR_QUERY(rmr_handle, rmr_param_mask, rmr_param);
}

DAT_RETURN DAT_API dat_rmr_bind(IN DAT_RMR_HANDLE rmr_handle,
				IN DAT_LMR_HANDLE lmr_handle,
				IN const DAT_LMR_TRIPLET * lmr_triplet,
				IN DAT_MEM_PRIV_FLAGS mem_priv,
				IN DAT_VA_TYPE va_type,
				IN DAT_EP_HANDLE ep_handle,
				IN DAT_RMR_COOKIE user_cookie,
				IN DAT_COMPLETION_FLAGS completion_flags,
				OUT DAT_RMR_CONTEXT * context)
{
	if (rmr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RMR);
	}
	return DAT_RMR_BIND(rmr_handle,
			    lmr_handle,
			    lmr_triplet,
			    mem_priv,
			    va_type,
			    ep_handle, user_cookie, completion_flags, context);
}

DAT_RETURN DAT_API dat_rmr_free(IN DAT_RMR_HANDLE rmr_handle)
{
	if (rmr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RMR);
	}
	return DAT_RMR_FREE(rmr_handle);
}

DAT_RETURN DAT_API dat_lmr_sync_rdma_read(IN DAT_IA_HANDLE ia_handle,
					  IN const DAT_LMR_TRIPLET *
					  local_segments,
					  IN DAT_VLEN num_segments)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_LMR_SYNC_RDMA_READ(dapl_ia_handle,
						    local_segments,
						    num_segments);

	}

	return dat_status;
}

DAT_RETURN DAT_API dat_lmr_sync_rdma_write(IN DAT_IA_HANDLE ia_handle,
					   IN const DAT_LMR_TRIPLET *
					   local_segments,
					   IN DAT_VLEN num_segments)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_LMR_SYNC_RDMA_WRITE(dapl_ia_handle,
						     local_segments,
						     num_segments);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_psp_create(IN DAT_IA_HANDLE ia_handle,
				  IN DAT_CONN_QUAL conn_qual,
				  IN DAT_EVD_HANDLE evd_handle,
				  IN DAT_PSP_FLAGS psp_flags,
				  OUT DAT_PSP_HANDLE * psp_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_PSP_CREATE(dapl_ia_handle,
					    conn_qual,
					    evd_handle, psp_flags, psp_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_psp_create_any(IN DAT_IA_HANDLE ia_handle,
				      OUT DAT_CONN_QUAL * conn_qual,
				      IN DAT_EVD_HANDLE evd_handle,
				      IN DAT_PSP_FLAGS psp_flags,
				      OUT DAT_PSP_HANDLE * psp_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_PSP_CREATE_ANY(dapl_ia_handle,
						conn_qual,
						evd_handle,
						psp_flags, psp_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_psp_query(IN DAT_PSP_HANDLE psp_handle,
				 IN DAT_PSP_PARAM_MASK psp_param_mask,
				 OUT DAT_PSP_PARAM * psp_param)
{
	if (psp_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PSP);
	}
	return DAT_PSP_QUERY(psp_handle, psp_param_mask, psp_param);
}

DAT_RETURN DAT_API dat_psp_free(IN DAT_PSP_HANDLE psp_handle)
{
	if (psp_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PSP);
	}
	return DAT_PSP_FREE(psp_handle);
}

DAT_RETURN DAT_API dat_csp_create(IN DAT_IA_HANDLE ia_handle,
				  IN DAT_COMM * comm,
				  IN DAT_IA_ADDRESS_PTR address,
				  IN DAT_EVD_HANDLE evd_handle,
				  OUT DAT_CSP_HANDLE * csp_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_CSP_CREATE(dapl_ia_handle,
					    comm,
					    address, evd_handle, csp_handle);
	}
	return dat_status;
}

DAT_RETURN DAT_API dat_csp_query(IN DAT_CSP_HANDLE csp_handle,
				 IN DAT_CSP_PARAM_MASK csp_param_mask,
				 OUT DAT_CSP_PARAM * csp_param)
{
	if (csp_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CSP);
	}
	return DAT_CSP_QUERY(csp_handle, csp_param_mask, csp_param);
}

DAT_RETURN DAT_API dat_csp_free(IN DAT_CSP_HANDLE csp_handle)
{
	if (csp_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CSP);
	}
	return DAT_CSP_FREE(csp_handle);
}

DAT_RETURN DAT_API dat_rsp_create(IN DAT_IA_HANDLE ia_handle,
				  IN DAT_CONN_QUAL conn_qual,
				  IN DAT_EP_HANDLE ep_handle,
				  IN DAT_EVD_HANDLE evd_handle,
				  OUT DAT_RSP_HANDLE * rsp_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_RSP_CREATE(dapl_ia_handle,
					    conn_qual,
					    ep_handle, evd_handle, rsp_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_rsp_query(IN DAT_RSP_HANDLE rsp_handle,
				 IN DAT_RSP_PARAM_MASK rsp_param_mask,
				 OUT DAT_RSP_PARAM * rsp_param)
{
	if (rsp_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RSP);
	}
	return DAT_RSP_QUERY(rsp_handle, rsp_param_mask, rsp_param);
}

DAT_RETURN DAT_API dat_rsp_free(IN DAT_RSP_HANDLE rsp_handle)
{
	if (rsp_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RSP);
	}
	return DAT_RSP_FREE(rsp_handle);
}

DAT_RETURN DAT_API dat_pz_create(IN DAT_IA_HANDLE ia_handle,
				 OUT DAT_PZ_HANDLE * pz_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_PZ_CREATE(dapl_ia_handle, pz_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_pz_query(IN DAT_PZ_HANDLE pz_handle,
				IN DAT_PZ_PARAM_MASK pz_param_mask,
				OUT DAT_PZ_PARAM * pz_param)
{
	if (pz_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
	}
	return DAT_PZ_QUERY(pz_handle, pz_param_mask, pz_param);
}

DAT_RETURN DAT_API dat_pz_free(IN DAT_PZ_HANDLE pz_handle)
{
	if (pz_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
	}
	return DAT_PZ_FREE(pz_handle);
}

DAT_RETURN DAT_API dat_ep_create_with_srq(IN DAT_IA_HANDLE ia_handle,
					  IN DAT_PZ_HANDLE pz_handle,
					  IN DAT_EVD_HANDLE recv_evd_handle,
					  IN DAT_EVD_HANDLE request_evd_handle,
					  IN DAT_EVD_HANDLE connect_evd_handle,
					  IN DAT_SRQ_HANDLE srq_handle,
					  IN const DAT_EP_ATTR * ep_attributes,
					  OUT DAT_EP_HANDLE * ep_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_EP_CREATE_WITH_SRQ(dapl_ia_handle,
						    pz_handle,
						    recv_evd_handle,
						    request_evd_handle,
						    connect_evd_handle,
						    srq_handle,
						    ep_attributes, ep_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_ep_recv_query(IN DAT_EP_HANDLE ep_handle,
				     OUT DAT_COUNT * nbufs_allocated,
				     OUT DAT_COUNT * bufs_alloc_span)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_RECV_QUERY(ep_handle, nbufs_allocated, bufs_alloc_span);
}

DAT_RETURN DAT_API dat_ep_set_watermark(IN DAT_EP_HANDLE ep_handle,
					IN DAT_COUNT soft_high_watermark,
					IN DAT_COUNT hard_high_watermark)
{
	if (ep_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}
	return DAT_EP_SET_WATERMARK(ep_handle,
				    soft_high_watermark, hard_high_watermark);
}

/* SRQ functions */

DAT_RETURN DAT_API dat_srq_create(IN DAT_IA_HANDLE ia_handle,
				  IN DAT_PZ_HANDLE pz_handle,
				  IN DAT_SRQ_ATTR * srq_attr,
				  OUT DAT_SRQ_HANDLE * srq_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_SRQ_CREATE(dapl_ia_handle,
					    pz_handle, srq_attr, srq_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_srq_free(IN DAT_SRQ_HANDLE srq_handle)
{
	return DAT_SRQ_FREE(srq_handle);
}

DAT_RETURN DAT_API dat_srq_post_recv(IN DAT_SRQ_HANDLE srq_handle,
				     IN DAT_COUNT num_segments,
				     IN DAT_LMR_TRIPLET * local_iov,
				     IN DAT_DTO_COOKIE user_cookie)
{
	if (srq_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
	}
	return DAT_SRQ_POST_RECV(srq_handle,
				 num_segments, local_iov, user_cookie);
}

DAT_RETURN DAT_API dat_srq_query(IN DAT_SRQ_HANDLE srq_handle,
				 IN DAT_SRQ_PARAM_MASK srq_param_mask,
				 OUT DAT_SRQ_PARAM * srq_param)
{
	if (srq_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
	}
	return DAT_SRQ_QUERY(srq_handle, srq_param_mask, srq_param);
}

DAT_RETURN DAT_API dat_srq_resize(IN DAT_SRQ_HANDLE srq_handle,
				  IN DAT_COUNT srq_max_recv_dto)
{
	if (srq_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
	}
	return DAT_SRQ_RESIZE(srq_handle, srq_max_recv_dto);
}

DAT_RETURN DAT_API dat_srq_set_lw(IN DAT_SRQ_HANDLE srq_handle,
				  IN DAT_COUNT low_watermark)
{
	if (srq_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
	}
	return DAT_SRQ_SET_LW(srq_handle, low_watermark);
}

#ifdef DAT_EXTENSIONS

extern int g_dat_extensions;

DAT_RETURN DAT_API dat_extension_op(IN DAT_HANDLE handle,
				    IN DAT_EXTENDED_OP ext_op, IN ...)
{
	DAT_RETURN status;
	DAT_IA_HANDLE dapl_handle;
	va_list args;

	/* If not IA handle then just passthrough */
	if (dats_get_ia_handle(handle, &dapl_handle) != DAT_SUCCESS) {
		dapl_handle = handle;
	}

	/* verify provider extension support */
	if (!g_dat_extensions) {
		return DAT_ERROR(DAT_NOT_IMPLEMENTED, 0);
	}

	/* extension will validate the handle based on op */
	va_start(args, ext_op);
	status = DAT_HANDLE_EXTENDEDOP(dapl_handle, ext_op, args);
	va_end(args);

	return status;
}
#endif

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
