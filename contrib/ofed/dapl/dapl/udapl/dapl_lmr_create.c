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
 * MODULE: dapl_lmr_create.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl_lmr_util.h"
#include "dapl_adapter_util.h"
#include "dapl_vendor.h"

/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

STATIC _INLINE_ DAT_RETURN
dapli_lmr_create_virtual(IN DAPL_IA * ia,
			 IN DAT_PVOID virt_addr,
			 IN DAT_VLEN length,
			 IN DAPL_PZ * pz,
			 IN DAT_MEM_PRIV_FLAGS privileges,
			 IN DAT_VA_TYPE va_type,
			 OUT DAT_LMR_HANDLE * lmr_handle,
			 OUT DAT_LMR_CONTEXT * lmr_context,
			 OUT DAT_RMR_CONTEXT * rmr_context,
			 OUT DAT_VLEN * registered_length,
			 OUT DAT_VADDR * registered_address);

STATIC _INLINE_ DAT_RETURN
dapli_lmr_create_lmr(IN DAPL_IA * ia,
		     IN DAPL_LMR * original_lmr,
		     IN DAPL_PZ * pz,
		     IN DAT_MEM_PRIV_FLAGS privileges,
		     IN DAT_VA_TYPE va_type,
		     OUT DAT_LMR_HANDLE * lmr_handle,
		     OUT DAT_LMR_CONTEXT * lmr_context,
		     OUT DAT_RMR_CONTEXT * rmr_context,
		     OUT DAT_VLEN * registered_length,
		     OUT DAT_VADDR * registered_address);

STATIC _INLINE_ DAT_RETURN
dapli_lmr_create_shared(IN DAPL_IA * ia,
			IN DAT_REGION_DESCRIPTION reg_desc,
			IN DAT_VLEN length,
			IN DAPL_PZ * pz,
			IN DAT_MEM_PRIV_FLAGS privileges,
			IN DAT_VA_TYPE va_type,
			OUT DAT_LMR_HANDLE * lmr_handle,
			OUT DAT_LMR_CONTEXT * lmr_context,
			OUT DAT_RMR_CONTEXT * rmr_context,
			OUT DAT_VLEN * registered_length,
			OUT DAT_VADDR * registered_address);

/*********************************************************************
 *                                                                   *
 * Function Definitions                                              *
 *                                                                   *
 *********************************************************************/

DAT_RETURN
dapli_lmr_create_virtual(IN DAPL_IA * ia,
			 IN DAT_PVOID virt_addr,
			 IN DAT_VLEN length,
			 IN DAPL_PZ * pz,
			 IN DAT_MEM_PRIV_FLAGS privileges,
			 IN DAT_VA_TYPE va_type,
			 OUT DAT_LMR_HANDLE * lmr_handle,
			 OUT DAT_LMR_CONTEXT * lmr_context,
			 OUT DAT_RMR_CONTEXT * rmr_context,
			 OUT DAT_VLEN * registered_length,
			 OUT DAT_VADDR * registered_address)
{
	DAPL_LMR *lmr;
	DAT_REGION_DESCRIPTION reg_desc;
	DAT_RETURN dat_status;

	reg_desc.for_va = virt_addr;
	dat_status = DAT_SUCCESS;

	lmr = dapl_lmr_alloc(ia,
			     DAT_MEM_TYPE_VIRTUAL,
			     reg_desc, length, (DAT_PZ_HANDLE) pz, privileges);

	if (NULL == lmr) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dat_status = dapls_ib_mr_register(ia,
					  lmr,
					  virt_addr,
					  length, privileges, va_type);

	if (DAT_SUCCESS != dat_status) {
		dapl_lmr_dealloc(lmr);
		goto bail;
	}

	/* if the LMR context is already in the hash table */
	dat_status = dapls_hash_search(ia->hca_ptr->lmr_hash_table,
				       lmr->param.lmr_context, NULL);
	if (dat_status == DAT_SUCCESS) {
		(void)dapls_ib_mr_deregister(lmr);
		dapl_lmr_dealloc(lmr);

		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_LMR_IN_USE);
		goto bail;
	}

	dat_status = dapls_hash_insert(ia->hca_ptr->lmr_hash_table,
				       lmr->param.lmr_context, lmr);
	if (dat_status != DAT_SUCCESS) {
		(void)dapls_ib_mr_deregister(lmr);
		dapl_lmr_dealloc(lmr);

		/* The value returned by dapls_hash_insert(.) is not    */
		/* returned to the consumer because the spec. requires */
		/* that dat_lmr_create(.) return only certain values.  */
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dapl_os_atomic_inc(&pz->pz_ref_count);
	*lmr_handle = (DAT_LMR_HANDLE) lmr;

	if (NULL != lmr_context) {
		*lmr_context = lmr->param.lmr_context;
	}
	if (NULL != rmr_context) {
		*rmr_context = lmr->param.rmr_context;
	}
	if (NULL != registered_length) {
		*registered_length = lmr->param.registered_size;
	}
	if (NULL != registered_address) {
		*registered_address = lmr->param.registered_address;
	}

      bail:
	return dat_status;
}

DAT_RETURN
dapli_lmr_create_lmr(IN DAPL_IA * ia,
		     IN DAPL_LMR * original_lmr,
		     IN DAPL_PZ * pz,
		     IN DAT_MEM_PRIV_FLAGS privileges,
		     IN DAT_VA_TYPE va_type,
		     OUT DAT_LMR_HANDLE * lmr_handle,
		     OUT DAT_LMR_CONTEXT * lmr_context,
		     OUT DAT_RMR_CONTEXT * rmr_context,
		     OUT DAT_VLEN * registered_length,
		     OUT DAT_VADDR * registered_address)
{
	DAPL_LMR *lmr;
	DAT_REGION_DESCRIPTION reg_desc;
	DAT_RETURN dat_status;
	DAPL_HASH_DATA hash_lmr;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_lmr_create_lmr (%p, %p, %p, %x, %x, %p, %p, %p, %p)\n",
		     ia,
		     original_lmr,
		     pz, privileges, va_type,
		     lmr_handle,
		     lmr_context, registered_length, registered_address);

	dat_status = dapls_hash_search(ia->hca_ptr->lmr_hash_table,
				       original_lmr->param.lmr_context,
				       &hash_lmr);
	if (dat_status != DAT_SUCCESS) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}
	lmr = (DAPL_LMR *) hash_lmr;
	reg_desc.for_lmr_handle = (DAT_LMR_HANDLE) original_lmr;

	lmr = dapl_lmr_alloc(ia,
			     DAT_MEM_TYPE_LMR,
			     reg_desc,
			     original_lmr->param.length,
			     (DAT_PZ_HANDLE) pz, privileges);

	if (NULL == lmr) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dat_status = dapls_ib_mr_register_shared(ia, lmr, privileges, va_type);

	if (DAT_SUCCESS != dat_status) {
		dapl_lmr_dealloc(lmr);
		goto bail;
	}

	/* if the LMR context is already in the hash table */
	dat_status = dapls_hash_search(ia->hca_ptr->lmr_hash_table,
				       lmr->param.lmr_context, NULL);
	if (dat_status == DAT_SUCCESS) {
		dapls_ib_mr_deregister(lmr);
		dapl_lmr_dealloc(lmr);

		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_LMR_IN_USE);
		goto bail;
	}

	dat_status = dapls_hash_insert(ia->hca_ptr->lmr_hash_table,
				       lmr->param.lmr_context, lmr);
	if (dat_status != DAT_SUCCESS) {
		dapls_ib_mr_deregister(lmr);
		dapl_lmr_dealloc(lmr);

		/* The value returned by dapls_hash_insert(.) is not    */
		/* returned to the consumer because the spec. requires */
		/* that dat_lmr_create(.) return only certain values.  */
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dapl_os_atomic_inc(&pz->pz_ref_count);
	*lmr_handle = (DAT_LMR_HANDLE) lmr;

	if (NULL != lmr_context) {
		*lmr_context = lmr->param.lmr_context;
	}
	if (NULL != rmr_context) {
		*rmr_context = lmr->param.rmr_context;
	}
	if (NULL != registered_length) {
		*registered_length = lmr->param.registered_size;
	}
	if (NULL != registered_address) {
		*registered_address = lmr->param.registered_address;
	}

      bail:
	return dat_status;
}

DAT_RETURN
dapli_lmr_create_shared(IN DAPL_IA * ia,
			IN DAT_REGION_DESCRIPTION reg_desc,
			IN DAT_VLEN length,
			IN DAPL_PZ * pz,
			IN DAT_MEM_PRIV_FLAGS privileges,
			IN DAT_VA_TYPE va_type,
			OUT DAT_LMR_HANDLE * lmr_handle,
			OUT DAT_LMR_CONTEXT * lmr_context,
			OUT DAT_RMR_CONTEXT * rmr_context,
			OUT DAT_VLEN * registered_length,
			OUT DAT_VADDR * registered_address)
{
	DAPL_LMR *lmr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_lmr_create_shared_virtual (ia=%p len=%x pz=%p priv=%x)\n",
		     ia, length, pz, privileges);

	lmr = dapl_lmr_alloc(ia, DAT_MEM_TYPE_LMR, reg_desc, length,	/* length is meaningless */
			     (DAT_PZ_HANDLE) pz, privileges);

	if (NULL == lmr) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	/*
	 * Added for the shared memory support - - -
	 * Save the region description.  We need to copy the shared
	 * memory id because the region_desc only contains a pointer
	 * to it.
	 */
	dapl_os_memcpy(&lmr->shmid,
		       reg_desc.for_shared_memory.shared_memory_id,
		       sizeof(lmr->shmid));
	lmr->param.region_desc = reg_desc;
	lmr->param.length = length;
	lmr->param.mem_type = DAT_MEM_TYPE_SHARED_VIRTUAL;
	lmr->param.region_desc.for_shared_memory.shared_memory_id = &lmr->shmid;

	dat_status = dapls_ib_mr_register_shared(ia, lmr, privileges, va_type);
	if (dat_status != DAT_SUCCESS) {
		dapl_lmr_dealloc(lmr);
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
			      DAT_RESOURCE_MEMORY_REGION);
		goto bail;
	}

	/* if the LMR context is already in the hash table */
	dat_status = dapls_hash_search(ia->hca_ptr->lmr_hash_table,
				       lmr->param.lmr_context, NULL);
	if (DAT_SUCCESS == dat_status) {
		(void)dapls_ib_mr_deregister(lmr);
		dapl_lmr_dealloc(lmr);

		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_LMR_IN_USE);
		goto bail;
	}

	dat_status = dapls_hash_insert(ia->hca_ptr->lmr_hash_table,
				       lmr->param.lmr_context, lmr);
	if (dat_status != DAT_SUCCESS) {
		(void)dapls_ib_mr_deregister(lmr);
		dapl_lmr_dealloc(lmr);

		/* The value returned by dapls_hash_insert(.) is not    */
		/* returned to the consumer because the spec. requires */
		/* that dat_lmr_create(.) return only certain values.  */
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dapl_os_atomic_inc(&pz->pz_ref_count);
	*lmr_handle = (DAT_LMR_HANDLE) lmr;

	if (NULL != lmr_context) {
		*lmr_context = (DAT_LMR_CONTEXT) lmr->param.lmr_context;
	}
	if (NULL != rmr_context) {
		*rmr_context = (DAT_LMR_CONTEXT) lmr->param.rmr_context;
	}
	if (NULL != registered_length) {
		*registered_length = lmr->param.length;
	}
	if (NULL != registered_address) {
		*registered_address = (DAT_VADDR) (uintptr_t)
		    lmr->param.region_desc.for_shared_memory.virtual_address;
	}
      bail:

	return dat_status;
}

/*
 * dapl_lmr_create
 *
 * Register a memory region with an Interface Adaptor.
 *
 * Input:
 *	ia_handle
 *	mem_type
 *	region_description
 *	length
 *	pz_handle
 *	privileges
 *
 * Output:
 *	lmr_handle
 *	lmr_context
 *	registered_length
 *	registered_address
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INVALID_HANDLE
 * 	DAT_INVALID_STATE
 * 	DAT_MODEL_NOT_SUPPORTED
 *
 */
DAT_RETURN DAT_API
dapl_lmr_create(IN DAT_IA_HANDLE ia_handle,
		IN DAT_MEM_TYPE mem_type,
		IN DAT_REGION_DESCRIPTION region_description,
		IN DAT_VLEN length,
		IN DAT_PZ_HANDLE pz_handle,
		IN DAT_MEM_PRIV_FLAGS privileges,
		IN DAT_VA_TYPE va_type,
		OUT DAT_LMR_HANDLE * lmr_handle,
		OUT DAT_LMR_CONTEXT * lmr_context,
		OUT DAT_RMR_CONTEXT * rmr_context,
		OUT DAT_VLEN * registered_length,
		OUT DAT_VADDR * registered_address)
{
	DAPL_IA *ia;
	DAPL_PZ *pz;
	DAT_RETURN dat_status;

	if (DAPL_BAD_HANDLE(ia_handle, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}
	if (DAPL_BAD_HANDLE(pz_handle, DAPL_MAGIC_PZ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
		goto bail;
	}
	if (NULL == lmr_handle) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG7);
		goto bail;
	}

	ia = (DAPL_IA *) ia_handle;
	pz = (DAPL_PZ *) pz_handle;

	DAPL_CNTR(ia, DCNT_IA_LMR_CREATE);

	switch (mem_type) {
	case DAT_MEM_TYPE_VIRTUAL:
		{
			dat_status =
			    dapli_lmr_create_virtual(ia,
						     region_description.for_va,
						     length, pz, privileges,
						     va_type, lmr_handle,
						     lmr_context, rmr_context,
						     registered_length,
						     registered_address);
			break;
		}
	case DAT_MEM_TYPE_LMR:
		{
			DAPL_LMR *lmr;

			if (DAPL_BAD_HANDLE
			    (region_description.for_lmr_handle,
			     DAPL_MAGIC_LMR)) {
				dat_status =
				    DAT_ERROR(DAT_INVALID_HANDLE,
					      DAT_INVALID_HANDLE_LMR);
				goto bail;
			}

			lmr = (DAPL_LMR *) region_description.for_lmr_handle;

			dat_status =
			    dapli_lmr_create_lmr(ia, lmr, pz, privileges,
						 va_type, lmr_handle,
						 lmr_context, rmr_context,
						 registered_length,
						 registered_address);
			break;
		}
	case DAT_MEM_TYPE_SHARED_VIRTUAL:
		{
#if (VN_MEM_SHARED_VIRTUAL_SUPPORT > 0)
			dat_status =
			    dapli_lmr_create_shared(ia, region_description,
						    length, pz, privileges,
						    va_type, lmr_handle,
						    lmr_context, rmr_context,
						    registered_length,
						    registered_address);
#else
			dat_status =
			    DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
#endif
			break;
		}
	default:
		{
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
			break;
		}
	}

      bail:
	return dat_status;
}
