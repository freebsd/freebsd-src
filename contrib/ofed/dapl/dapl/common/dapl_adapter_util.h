/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
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
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 * 
 * HEADER: dapl_adapter_util.h
 *
 * PURPOSE: Utility defs & routines for the adapter data structure
 *
 * $Id: dapl_adapter_util.h 1317 2005-04-25 17:29:42Z jlentini $
 *
 **********************************************************************/

#ifndef _DAPL_ADAPTER_UTIL_H_
#define _DAPL_ADAPTER_UTIL_H_


typedef enum async_handler_type
{
    DAPL_ASYNC_UNAFILIATED,
	DAPL_ASYNC_CQ_ERROR,
	DAPL_ASYNC_CQ_COMPLETION,
	DAPL_ASYNC_QP_ERROR
} DAPL_ASYNC_HANDLER_TYPE;


int dapls_ib_init (void);

int dapls_ib_release (void);

DAT_RETURN dapls_ib_enum_hcas (
        IN   const char                 *vendor, 
	OUT  DAPL_HCA_NAME		**hca_names,
	OUT  DAT_COUNT			*total_hca_count);

DAT_RETURN dapls_ib_get_instance_data(
	IN  DAPL_HCA_NAME hca_name, 
	OUT char *instance);

DAT_RETURN dapls_ib_open_hca (
	IN   char	               *namestr,
	IN   DAPL_HCA		       *hca_ptr);

DAT_RETURN dapls_ib_close_hca (
	IN   DAPL_HCA		       *hca_ptr);

DAT_RETURN dapls_ib_qp_alloc (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_EP			*ep_ptr,
	IN  DAPL_EP			*ep_ctx_ptr);

DAT_RETURN dapls_ib_qp_free (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_EP			*ep_ptr);

DAT_RETURN dapls_ib_qp_modify (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_EP			*ep_ptr,
	IN  DAT_EP_ATTR			*ep_attr);

DAT_RETURN dapls_ib_connect (
	IN  DAT_EP_HANDLE		ep_handle,
	IN  DAT_IA_ADDRESS_PTR		remote_ia_address,
	IN  DAT_CONN_QUAL		remote_conn_qual,
	IN  DAT_COUNT			private_data_size,
	IN  DAT_PVOID			private_data);

DAT_RETURN dapls_ib_disconnect (
	IN  DAPL_EP			*ep_ptr,
	IN  DAT_CLOSE_FLAGS		close_flags);

DAT_RETURN dapls_ib_setup_conn_listener (
	IN  DAPL_IA			*ia_ptr,
	IN  DAT_UINT64			ServiceID,
	IN  DAPL_SP			*sp_ptr);

DAT_RETURN dapls_ib_remove_conn_listener (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_SP			*sp_ptr);

DAT_RETURN dapls_ib_accept_connection (
	IN  DAT_CR_HANDLE		cr_handle,
	IN  DAT_EP_HANDLE		ep_handle,
	IN  DAT_COUNT			private_data_size,
	IN  const DAT_PVOID		private_data);

DAT_RETURN dapls_ib_reject_connection (
	IN  dp_ib_cm_handle_t		cm_handle,
	IN  int				reject_reason,
	IN  DAT_COUNT			private_data_size,
	IN  const DAT_PVOID		private_data);

DAT_RETURN dapls_ib_setup_async_callback (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_ASYNC_HANDLER_TYPE	handler_type,
	IN  DAPL_EVD			*evd_ptr,
	IN  ib_async_handler_t		callback,
	IN  void			*context);

DAT_RETURN dapls_ib_cq_alloc (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_EVD			*evd_ptr,
	IN  DAT_COUNT			*cqlen);

DAT_RETURN dapls_ib_cq_free (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_EVD			*evd_ptr);

DAT_RETURN dapls_set_cq_notify (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_EVD			*evd_ptr);

DAT_RETURN dapls_ib_cq_resize (
	IN  DAPL_IA			*ia_ptr,
	IN  DAPL_EVD			*evd_ptr,
	IN  DAT_COUNT			*cqlen);

DAT_RETURN dapls_ib_pd_alloc (
	IN  DAPL_IA 			*ia_ptr,
	IN  DAPL_PZ 			*pz);

DAT_RETURN dapls_ib_pd_free (
	IN  DAPL_PZ	 		*pz);

DAT_RETURN dapls_ib_mr_register (
	IN  DAPL_IA 			*ia_ptr,
        IN  DAPL_LMR			*lmr,
	IN  DAT_PVOID			virt_addr,
	IN  DAT_VLEN			length,
	IN  DAT_MEM_PRIV_FLAGS		privileges,
	IN  DAT_VA_TYPE			va_type);

#if defined(__KDAPL__)
DAT_RETURN dapls_ib_mr_register_physical (
	IN  DAPL_IA                     *ia_ptr,
	INOUT  DAPL_LMR                 *lmr,
	IN  DAT_PADDR                   phys_addr,
	IN  DAT_VLEN                    length,
	IN  DAT_MEM_PRIV_FLAGS          privileges);
#endif /* __KDAPL__ */

DAT_RETURN dapls_ib_mr_deregister (
	IN  DAPL_LMR			*lmr);

DAT_RETURN dapls_ib_mr_register_shared (
	IN  DAPL_IA 			*ia_ptr,
        IN  DAPL_LMR			*lmr,
	IN  DAT_MEM_PRIV_FLAGS		privileges,
	IN  DAT_VA_TYPE			va_type);

DAT_RETURN dapls_ib_mw_alloc (
	IN  DAPL_RMR 			*rmr);

DAT_RETURN dapls_ib_mw_free (
	IN  DAPL_RMR			*rmr);

DAT_RETURN dapls_ib_mw_bind (
	IN  DAPL_RMR			*rmr,
	IN  DAPL_LMR			*lmr,
	IN  DAPL_EP			*ep,
	IN  DAPL_COOKIE			*cookie,
	IN  DAT_VADDR			virtual_address,
	IN  DAT_VLEN			length,
	IN  DAT_MEM_PRIV_FLAGS		mem_priv,
	IN  DAT_BOOLEAN			is_signaled);

DAT_RETURN dapls_ib_mw_unbind (
	IN  DAPL_RMR			*rmr,
	IN  DAPL_EP			*ep,
	IN  DAPL_COOKIE			*cookie,
	IN  DAT_BOOLEAN			is_signaled);

DAT_RETURN dapls_ib_query_hca (
	IN  DAPL_HCA			*hca_ptr,
	OUT DAT_IA_ATTR	   		*ia_attr,
	OUT DAT_EP_ATTR			*ep_attr,
	OUT DAT_SOCK_ADDR6		*ip_addr);

DAT_RETURN dapls_ib_completion_poll (
	IN  DAPL_HCA			*hca_ptr,
	IN  DAPL_EVD			*evd_ptr,
	IN  ib_work_completion_t	*cqe_ptr);

DAT_RETURN dapls_ib_completion_notify (
	IN  ib_hca_handle_t		hca_handle,
	IN  DAPL_EVD			*evd_ptr,
	IN  ib_notification_type_t	type);

DAT_DTO_COMPLETION_STATUS dapls_ib_get_dto_status (
	IN  ib_work_completion_t	*cqe_ptr);

void dapls_ib_reinit_ep (
	IN  DAPL_EP			*ep_ptr);

void dapls_ib_disconnect_clean (
	IN  DAPL_EP			*ep_ptr,
	IN  DAT_BOOLEAN			passive,
	IN  const ib_cm_events_t	ib_cm_event);

DAT_RETURN dapls_ib_get_async_event (
	IN  ib_error_record_t		*cause_ptr,
	OUT DAT_EVENT_NUMBER		*async_event);

DAT_EVENT_NUMBER dapls_ib_get_dat_event (
	IN  const ib_cm_events_t	ib_cm_event,
	IN  DAT_BOOLEAN			active);

ib_cm_events_t dapls_ib_get_cm_event (
	IN  DAT_EVENT_NUMBER		dat_event_num);

DAT_RETURN dapls_ib_cm_remote_addr (
	IN  DAT_HANDLE			dat_handle,
	OUT DAT_SOCK_ADDR6		*remote_ia_address);

int dapls_ib_private_data_size (
	IN  DAPL_PRIVATE		*prd_ptr,
	IN  DAPL_PDATA_OP		conn_op,
	IN  DAPL_HCA			*hca_ptr);

void 
dapls_query_provider_specific_attr(
   	IN DAPL_IA			*ia_ptr,
	IN DAT_PROVIDER_ATTR		*attr_ptr );

DAT_RETURN
dapls_evd_dto_wakeup (
	IN DAPL_EVD			*evd_ptr);

DAT_RETURN
dapls_evd_dto_wait (
	IN DAPL_EVD			*evd_ptr,
	IN uint32_t 			timeout);

#ifdef DAT_EXTENSIONS
void
dapls_cqe_to_event_extension(
	IN DAPL_EP			*ep_ptr,
	IN DAPL_COOKIE			*cookie,
	IN ib_work_completion_t		*cqe_ptr,
	IN DAT_EVENT			*event_ptr);
#endif

/*
 * Values for provider DAT_NAMED_ATTR
 */
#define IB_QP_STATE		1	/* QP state change request */


#ifdef	IBAPI
#include "dapl_ibapi_dto.h"
#elif VAPI
#include "dapl_vapi_dto.h"
#elif __OPENIB__
#include "dapl_openib_dto.h"
#elif DUMMY
#include "dapl_dummy_dto.h"
#elif OPENIB
#include "dapl_ib_dto.h"
#else
#include "dapl_ibal_dto.h"
#endif


#endif	/*  _DAPL_ADAPTER_UTIL_H_ */
