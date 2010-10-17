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
 * HEADER: dapl_evd_util.h
 *
 * PURPOSE: Utility defs & routines for the EVD data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_EVD_UTIL_H_
#define _DAPL_EVD_UTIL_H_

#include "dapl.h"

DAT_RETURN
dapls_evd_internal_create (
    IN DAPL_IA		*ia_ptr, 
    IN DAPL_CNO		*cno_ptr,
    IN DAT_COUNT	min_qlen,
    IN DAT_EVD_FLAGS	evd_flags,
    OUT DAPL_EVD	**evd_ptr_ptr) ;

DAPL_EVD *
dapls_evd_alloc ( 
    IN DAPL_IA		*ia_ptr,
    IN DAPL_CNO		*cno_ptr,
    IN DAT_EVD_FLAGS	evd_flags,
    IN DAT_COUNT	qlen) ;

DAT_RETURN
dapls_evd_dealloc ( 
    IN DAPL_EVD 	*evd_ptr) ;

DAT_RETURN dapls_evd_event_realloc (
    IN DAPL_EVD		*evd_ptr,
    IN DAT_COUNT	qlen);

/*
 * Each of these functions will retrieve a free event from
 * the specified EVD, fill in the elements of that event, and
 * post the event back to the EVD.  If there is no EVD available,
 * an overflow event will be posted to the async EVD associated
 * with the EVD.
 *
 * DAT_INSUFFICIENT_RESOURCES will be returned on overflow,
 * DAT_SUCCESS otherwise.
 */

DAT_RETURN
dapls_evd_post_cr_arrival_event (
    IN DAPL_EVD				*evd_ptr,
    IN DAT_EVENT_NUMBER			event_number,
    IN DAT_SP_HANDLE			sp_handle,
    DAT_IA_ADDRESS_PTR			ia_address_ptr,
    DAT_CONN_QUAL			conn_qual,
    DAT_CR_HANDLE			cr_handle);
    
DAT_RETURN
dapls_evd_post_connection_event (
    IN DAPL_EVD				*evd_ptr,
    IN DAT_EVENT_NUMBER			event_number,
    IN DAT_EP_HANDLE               	ep_handle,
    IN DAT_COUNT                   	private_data_size,
    IN DAT_PVOID                   	private_data);

DAT_RETURN
dapls_evd_post_async_error_event (
    IN DAPL_EVD				*evd_ptr,
    IN DAT_EVENT_NUMBER			event_number,
    IN DAT_IA_HANDLE			ia_handle);

DAT_RETURN
dapls_evd_post_software_event (
    IN DAPL_EVD				*evd_ptr,
    IN DAT_EVENT_NUMBER			event_number,
    IN DAT_PVOID			pointer);

DAT_RETURN
dapls_evd_post_generic_event (
    IN DAPL_EVD				*evd_ptr,
    IN DAT_EVENT_NUMBER			event_number,
    IN DAT_EVENT_DATA			*data);

#ifdef DAT_EXTENSIONS
DAT_RETURN
dapls_evd_post_cr_event_ext (
    IN DAPL_SP				*sp_ptr,
    IN DAT_EVENT_NUMBER			event_number,
    IN dp_ib_cm_handle_t		ib_cm_handle,
    IN DAT_COUNT			p_size,
    IN DAT_PVOID			p_data,
    IN DAT_PVOID			ext_data);

DAT_RETURN
dapls_evd_post_connection_event_ext (
    IN DAPL_EVD				*evd_ptr,
    IN DAT_EVENT_NUMBER			event_number,
    IN DAT_EP_HANDLE               	ep_handle,
    IN DAT_COUNT                   	private_data_size,
    IN DAT_PVOID                   	private_data,
    IN DAT_PVOID			ext_data);
#endif

/*************************************
 * dapl internal callbacks functions *
 *************************************/

/* connection verb callback */
extern void dapl_evd_connection_callback (
    IN	dp_ib_cm_handle_t	ib_cm_handle,
    IN	const ib_cm_events_t	ib_cm_events,
    IN	const void 		*instant_data_p,
    IN	const void *		context );

/* dto verb callback */
extern void dapl_evd_dto_callback (
    IN  ib_hca_handle_t 	ib_hca_handle, 
    IN  ib_cq_handle_t 		ib_cq_handle, 
    IN  void* 			context);

/* async verb callbacks */
extern void dapl_evd_un_async_error_callback (
    IN	ib_hca_handle_t		ib_hca_handle,
    IN	ib_error_record_t *	cause_ptr,
    IN	void *			context);

extern void dapl_evd_cq_async_error_callback (
    IN	ib_hca_handle_t 	ib_hca_handle,
    IN	ib_cq_handle_t		ib_cq_handle,
    IN	ib_error_record_t *	cause_ptr,
    IN	void *			context);

extern void dapl_evd_qp_async_error_callback (
    IN	ib_hca_handle_t 	ib_hca_handle,
    IN	ib_qp_handle_t		ib_qp_handle,
    IN	ib_error_record_t *	cause_ptr,
    IN	void *			context);

extern void dapls_evd_copy_cq (
    DAPL_EVD 			*evd_ptr);

extern DAT_RETURN dapls_evd_cq_poll_to_event (
    IN DAPL_EVD 		*evd_ptr,
    OUT DAT_EVENT		*event);

#endif
