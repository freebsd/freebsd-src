/*
 * Copyright (c) 2005-2007 Intel Corporation.  All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
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
#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_lmr_util.h"

/*
 * dapls_convert_privileges
 *
 * Convert LMR privileges to provider  
 *
 * Input:
 *	DAT_MEM_PRIV_FLAGS
 *
 * Output:
 *	none
 *
 * Returns:
 *	ibv_access_flags
 *
 */
STATIC _INLINE_ int dapls_convert_privileges(IN DAT_MEM_PRIV_FLAGS privileges)
{
	int access = 0;

	/*
	 * if (DAT_MEM_PRIV_LOCAL_READ_FLAG & privileges) do nothing
	 */
	if (DAT_MEM_PRIV_LOCAL_WRITE_FLAG & privileges)
		access |= IBV_ACCESS_LOCAL_WRITE;
	if (DAT_MEM_PRIV_REMOTE_WRITE_FLAG & privileges)
		access |= IBV_ACCESS_REMOTE_WRITE;
	if (DAT_MEM_PRIV_REMOTE_READ_FLAG & privileges)
		access |= IBV_ACCESS_REMOTE_READ;
#ifdef DAT_EXTENSIONS
	if (DAT_IB_MEM_PRIV_REMOTE_ATOMIC & privileges)
		access |= IBV_ACCESS_REMOTE_ATOMIC;
#endif

	return access;
}

/*
 * dapl_ib_pd_alloc
 *
 * Alloc a PD
 *
 * Input:
 *	ia_handle	IA handle
 *	pz		pointer to PZ struct
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN dapls_ib_pd_alloc(IN DAPL_IA * ia_ptr, IN DAPL_PZ * pz)
{
	/* get a protection domain */
	pz->pd_handle = ibv_alloc_pd(ia_ptr->hca_ptr->ib_hca_handle);
	if (!pz->pd_handle)
		return (dapl_convert_errno(ENOMEM, "alloc_pd"));

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " pd_alloc: pd_handle=%p\n", pz->pd_handle);

	return DAT_SUCCESS;
}

/*
 * dapl_ib_pd_free
 *
 * Free a PD
 *
 * Input:
 *	ia_handle	IA handle
 *	PZ_ptr		pointer to PZ struct
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *      DAT_INVALID_STATE
 *
 */
DAT_RETURN dapls_ib_pd_free(IN DAPL_PZ * pz)
{
	if (pz->pd_handle != IB_INVALID_HANDLE) {
		ibv_dealloc_pd(pz->pd_handle);
		pz->pd_handle = IB_INVALID_HANDLE;
	}
	return DAT_SUCCESS;
}

/*
 * dapl_ib_mr_register
 *
 * Register a virtual memory region
 *
 * Input:
 *	ia_handle	IA handle
 *	lmr		pointer to dapl_lmr struct
 *	virt_addr	virtual address of beginning of mem region
 *	length		length of memory region
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN
dapls_ib_mr_register(IN DAPL_IA * ia_ptr,
		     IN DAPL_LMR * lmr,
		     IN DAT_PVOID virt_addr,
		     IN DAT_VLEN length,
		     IN DAT_MEM_PRIV_FLAGS privileges, IN DAT_VA_TYPE va_type)
{
	ib_pd_handle_t ib_pd_handle;
	struct ibv_device *ibv_dev = ia_ptr->hca_ptr->ib_hca_handle->device;

	ib_pd_handle = ((DAPL_PZ *) lmr->param.pz_handle)->pd_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " mr_register: ia=%p, lmr=%p va=%p ln=%d pv=0x%x\n",
		     ia_ptr, lmr, virt_addr, length, privileges);

	/* TODO: shared memory */
	if (lmr->param.mem_type == DAT_MEM_TYPE_SHARED_VIRTUAL) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     " mr_register_shared: NOT IMPLEMENTED\n");
		return DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
	}

	/* iWARP only support */
	if ((va_type == DAT_VA_TYPE_ZB) &&
	    (ibv_dev->transport_type != IBV_TRANSPORT_IWARP)) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     " va_type == DAT_VA_TYPE_ZB: NOT SUPPORTED\n");
		return DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
	}

	/* local read is default on IB */
	lmr->mr_handle =
	    ibv_reg_mr(((DAPL_PZ *) lmr->param.pz_handle)->pd_handle,
		       virt_addr, length, dapls_convert_privileges(privileges));

	if (!lmr->mr_handle)
		return (dapl_convert_errno(ENOMEM, "reg_mr"));

	lmr->param.lmr_context = lmr->mr_handle->lkey;
	lmr->param.rmr_context = lmr->mr_handle->rkey;
	lmr->param.registered_size = length;
	lmr->param.registered_address = (DAT_VADDR) (uintptr_t) virt_addr;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " mr_register: mr=%p addr=%p pd %p ctx %p "
		     "lkey=0x%x rkey=0x%x priv=%x\n",
		     lmr->mr_handle, lmr->mr_handle->addr,
		     lmr->mr_handle->pd, lmr->mr_handle->context,
		     lmr->mr_handle->lkey, lmr->mr_handle->rkey,
		     length, dapls_convert_privileges(privileges));

	return DAT_SUCCESS;
}

/*
 * dapl_ib_mr_deregister
 *
 * Free a memory region
 *
 * Input:
 *	lmr			pointer to dapl_lmr struct
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_STATE
 *
 */
DAT_RETURN dapls_ib_mr_deregister(IN DAPL_LMR * lmr)
{
	if (lmr->mr_handle != IB_INVALID_HANDLE) {
		if (ibv_dereg_mr(lmr->mr_handle))
			return (dapl_convert_errno(errno, "dereg_pd"));
		lmr->mr_handle = IB_INVALID_HANDLE;
	}
	return DAT_SUCCESS;
}

/*
 * dapl_ib_mr_register_shared
 *
 * Register a virtual memory region
 *
 * Input:
 *	ia_ptr		IA handle
 *	lmr		pointer to dapl_lmr struct
 *	privileges	
 *	va_type		
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN
dapls_ib_mr_register_shared(IN DAPL_IA * ia_ptr,
			    IN DAPL_LMR * lmr,
			    IN DAT_MEM_PRIV_FLAGS privileges,
			    IN DAT_VA_TYPE va_type)
{
	dapl_dbg_log(DAPL_DBG_TYPE_ERR,
		     " mr_register_shared: NOT IMPLEMENTED\n");

	return DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
}

/*
 * dapls_ib_mw_alloc
 *
 * Bind a protection domain to a memory window
 *
 * Input:
 *	rmr	Initialized rmr to hold binding handles
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN dapls_ib_mw_alloc(IN DAPL_RMR * rmr)
{

	dapl_dbg_log(DAPL_DBG_TYPE_ERR, " mw_alloc: NOT IMPLEMENTED\n");

	return DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
}

/*
 * dapls_ib_mw_free
 *
 * Release bindings of a protection domain to a memory window
 *
 * Input:
 *	rmr	Initialized rmr to hold binding handles
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_STATE
 *
 */
DAT_RETURN dapls_ib_mw_free(IN DAPL_RMR * rmr)
{
	dapl_dbg_log(DAPL_DBG_TYPE_ERR, " mw_free: NOT IMPLEMENTED\n");

	return DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
}

/*
 * dapls_ib_mw_bind
 *
 * Bind a protection domain to a memory window
 *
 * Input:
 *	rmr	Initialized rmr to hold binding handles
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER;
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN
dapls_ib_mw_bind(IN DAPL_RMR * rmr,
		 IN DAPL_LMR * lmr,
		 IN DAPL_EP * ep,
		 IN DAPL_COOKIE * cookie,
		 IN DAT_VADDR virtual_address,
		 IN DAT_VLEN length,
		 IN DAT_MEM_PRIV_FLAGS mem_priv, IN DAT_BOOLEAN is_signaled)
{
	dapl_dbg_log(DAPL_DBG_TYPE_ERR, " mw_bind: NOT IMPLEMENTED\n");

	return DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
}

/*
 * dapls_ib_mw_unbind
 *
 * Unbind a protection domain from a memory window
 *
 * Input:
 *	rmr	Initialized rmr to hold binding handles
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *    	DAT_INVALID_PARAMETER;
 *   	DAT_INVALID_STATE;
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN
dapls_ib_mw_unbind(IN DAPL_RMR * rmr,
		   IN DAPL_EP * ep,
		   IN DAPL_COOKIE * cookie, IN DAT_BOOLEAN is_signaled)
{
	dapl_dbg_log(DAPL_DBG_TYPE_ERR, " mw_unbind: NOT IMPLEMENTED\n");

	return DAT_ERROR(DAT_NOT_IMPLEMENTED, DAT_NO_SUBTYPE);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
