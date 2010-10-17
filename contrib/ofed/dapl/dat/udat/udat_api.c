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
 * MODULE: udat.c
 *
 * PURPOSE: DAT Provider and Consumer registry functions.
 *
 * $Id: udat_api.c 1301 2005-03-24 05:58:55Z jlentini $
 **********************************************************************/

#include <dat2/udat.h>
#include <dat2/dat_registry.h>
#include "dat_osd.h"
#include "dat_init.h"

#define UDAT_IS_BAD_HANDLE(h) ( NULL == (p) )

DAT_RETURN DAT_API dat_lmr_create(IN DAT_IA_HANDLE ia_handle,
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
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_LMR_CREATE(dapl_ia_handle,
					    mem_type,
					    region_description,
					    length,
					    pz_handle,
					    privileges,
					    va_type,
					    lmr_handle,
					    lmr_context,
					    rmr_context,
					    registered_length,
					    registered_address);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_evd_create(IN DAT_IA_HANDLE ia_handle,
				  IN DAT_COUNT evd_min_qlen,
				  IN DAT_CNO_HANDLE cno_handle,
				  IN DAT_EVD_FLAGS evd_flags,
				  OUT DAT_EVD_HANDLE * evd_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_EVD_CREATE(dapl_ia_handle,
					    evd_min_qlen,
					    cno_handle, evd_flags, evd_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_evd_modify_cno(IN DAT_EVD_HANDLE evd_handle,
				      IN DAT_CNO_HANDLE cno_handle)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_MODIFY_CNO(evd_handle, cno_handle);
}

DAT_RETURN DAT_API dat_cno_create(IN DAT_IA_HANDLE ia_handle,
				  IN DAT_OS_WAIT_PROXY_AGENT agent,
				  OUT DAT_CNO_HANDLE * cno_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_CNO_CREATE(dapl_ia_handle, agent, cno_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_cno_fd_create(IN DAT_IA_HANDLE ia_handle,
				     OUT DAT_FD * fd,
				     OUT DAT_CNO_HANDLE * cno_handle)
{
	DAT_IA_HANDLE dapl_ia_handle;
	DAT_RETURN dat_status;

	dat_status = dats_get_ia_handle(ia_handle, &dapl_ia_handle);
	if (dat_status == DAT_SUCCESS) {
		dat_status = DAT_CNO_FD_CREATE(dapl_ia_handle,
					       (DAT_FD *) fd, cno_handle);
	}

	return dat_status;
}

DAT_RETURN DAT_API dat_cno_modify_agent(IN DAT_CNO_HANDLE cno_handle,
					IN DAT_OS_WAIT_PROXY_AGENT agent)
{
	if (cno_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CNO);
	}
	return DAT_CNO_MODIFY_AGENT(cno_handle, agent);
}

DAT_RETURN DAT_API dat_cno_query(IN DAT_CNO_HANDLE cno_handle,
				 IN DAT_CNO_PARAM_MASK cno_param_mask,
				 OUT DAT_CNO_PARAM * cno_param)
{
	return DAT_CNO_QUERY(cno_handle, cno_param_mask, cno_param);
}

DAT_RETURN DAT_API dat_cno_free(IN DAT_CNO_HANDLE cno_handle)
{
	if (cno_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CNO);
	}
	return DAT_CNO_FREE(cno_handle);
}

DAT_RETURN DAT_API dat_cno_wait(IN DAT_CNO_HANDLE cno_handle,
				IN DAT_TIMEOUT timeout,
				OUT DAT_EVD_HANDLE * evd_handle)
{
	if (cno_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CNO);
	}
	return DAT_CNO_WAIT(cno_handle, timeout, evd_handle);
}

DAT_RETURN DAT_API dat_evd_enable(IN DAT_EVD_HANDLE evd_handle)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_ENABLE(evd_handle);
}

DAT_RETURN DAT_API dat_evd_wait(IN DAT_EVD_HANDLE evd_handle,
				IN DAT_TIMEOUT Timeout,
				IN DAT_COUNT Threshold,
				OUT DAT_EVENT * event,
				OUT DAT_COUNT * n_more_events)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_WAIT(evd_handle,
			    Timeout, Threshold, event, n_more_events);
}

DAT_RETURN DAT_API dat_evd_disable(IN DAT_EVD_HANDLE evd_handle)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_DISABLE(evd_handle);
}

DAT_RETURN DAT_API dat_evd_set_unwaitable(IN DAT_EVD_HANDLE evd_handle)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_SET_UNWAITABLE(evd_handle);
}

DAT_RETURN DAT_API dat_evd_clear_unwaitable(IN DAT_EVD_HANDLE evd_handle)
{
	if (evd_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE,
				 DAT_INVALID_HANDLE_EVD_REQUEST);
	}
	return DAT_EVD_CLEAR_UNWAITABLE(evd_handle);
}

DAT_RETURN DAT_API dat_cr_handoff(IN DAT_CR_HANDLE cr_handle,
				  IN DAT_CONN_QUAL handoff)
{
	if (cr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_CR);
	}
	return DAT_CR_HANDOFF(cr_handle, handoff);
}

DAT_RETURN DAT_API dat_lmr_query(IN DAT_LMR_HANDLE lmr_handle,
				 IN DAT_LMR_PARAM_MASK lmr_param_mask,
				 OUT DAT_LMR_PARAM * lmr_param)
{
	if (lmr_handle == NULL) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_LMR);
	}
	return DAT_LMR_QUERY(lmr_handle, lmr_param_mask, lmr_param);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
