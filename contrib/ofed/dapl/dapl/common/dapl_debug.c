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

#include "dapl.h"
#if !defined(__KDAPL__)
#include <stdarg.h>
#include <stdlib.h>
#endif				/* __KDAPL__ */

DAPL_DBG_TYPE g_dapl_dbg_type;	/* initialized in dapl_init.c */
DAPL_DBG_DEST g_dapl_dbg_dest;	/* initialized in dapl_init.c */

static char *_ptr_host_ = NULL;
static char _hostname_[128];

void dapl_internal_dbg_log(DAPL_DBG_TYPE type, const char *fmt, ...)
{
	va_list args;

	if (_ptr_host_ == NULL) {
		gethostname(_hostname_, sizeof(_hostname_));
		_ptr_host_ = _hostname_;
	}

	if (type & g_dapl_dbg_type) {
		if (DAPL_DBG_DEST_STDOUT & g_dapl_dbg_dest) {
			va_start(args, fmt);
			fprintf(stdout, "%s:%x: ", _ptr_host_,
				dapl_os_getpid());
			dapl_os_vprintf(fmt, args);
			va_end(args);
		}

		if (DAPL_DBG_DEST_SYSLOG & g_dapl_dbg_dest) {
			va_start(args, fmt);
			dapl_os_syslog(fmt, args);
			va_end(args);
		}
	}
}

#ifdef DAPL_COUNTERS

/*
 * The order of this list must match the DAT counter definitions 
 */
static char *ia_cntr_names[] = {
	"DCNT_IA_PZ_CREATE",
	"DCNT_IA_PZ_FREE",
	"DCNT_IA_LMR_CREATE",
	"DCNT_IA_LMR_FREE",
	"DCNT_IA_RMR_CREATE",
	"DCNT_IA_RMR_FREE",
	"DCNT_IA_PSP_CREATE",
	"DCNT_IA_PSP_CREATE_ANY",
	"DCNT_IA_PSP_FREE",
	"DCNT_IA_RSP_CREATE",
	"DCNT_IA_RSP_FREE",
	"DCNT_IA_EVD_CREATE",
	"DCNT_IA_EVD_FREE",
	"DCNT_IA_EP_CREATE",
	"DCNT_IA_EP_FREE",
	"DCNT_IA_SRQ_CREATE",
	"DCNT_IA_SRQ_FREE",
	"DCNT_IA_SP_CR",
	"DCNT_IA_SP_CR_ACCEPTED",
	"DCNT_IA_SP_CR_REJECTED",
	"DCNT_IA_MEM_ALLOC",
	"DCNT_IA_MEM_ALLOC_DATA",
	"DCNT_IA_MEM_FREE",
	"DCNT_IA_ASYNC_ERROR",
	"DCNT_IA_ASYNC_QP_ERROR",
	"DCNT_IA_ASYNC_CQ_ERROR"
};

static char *ep_cntr_names[] = {
	"DCNT_EP_CONNECT",
	"DCNT_EP_DISCONNECT",
	"DCNT_EP_POST_SEND",
	"DCNT_EP_POST_SEND_DATA",
	"DCNT_EP_POST_SEND_UD",
	"DCNT_EP_POST_SEND_UD_DATA",
	"DCNT_EP_POST_RECV",
	"DCNT_EP_POST_RECV_DATA",
	"DCNT_EP_POST_WRITE",
	"DCNT_EP_POST_WRITE_DATA",
	"DCNT_EP_POST_WRITE_IMM",
	"DCNT_EP_POST_WRITE_IMM_DATA",
	"DCNT_EP_POST_READ",
	"DCNT_EP_POST_READ_DATA",
	"DCNT_EP_POST_CMP_SWAP",
	"DCNT_EP_POST_FETCH_ADD",
	"DCNT_EP_RECV",
	"DCNT_EP_RECV_DATA",
	"DCNT_EP_RECV_UD",
	"DCNT_EP_RECV_UD_DATA",
	"DCNT_EP_RECV_IMM",
	"DCNT_EP_RECV_IMM_DATA",
	"DCNT_EP_RECV_RDMA_IMM",
	"DCNT_EP_RECV_RDMA_IMM_DATA",
};

static char *evd_cntr_names[] = {
	"DCNT_EVD_WAIT",
	"DCNT_EVD_WAIT_BLOCKED",
	"DCNT_EVD_WAIT_NOTIFY",
	"DCNT_EVD_DEQUEUE",
	"DCNT_EVD_DEQUEUE_FOUND",
	"DCNT_EVD_DEQUEUE_NOT_FOUND",
	"DCNT_EVD_DEQUEUE_POLL",
	"DCNT_EVD_DEQUEUE_POLL_FOUND",
	"DCNT_EVD_CONN_CALLBACK",
	"DCNT_EVD_DTO_CALLBACK",
};

DAT_RETURN dapl_query_counter(DAT_HANDLE dh,
			      int counter, void *p_cntrs_out, int reset)
{
	int i, max;
	DAT_UINT64 *p_cntrs;
	DAT_HANDLE_TYPE type = 0;

	dat_get_handle_type(dh, &type);

	switch (type) {
	case DAT_HANDLE_TYPE_IA:
		max = DCNT_IA_ALL_COUNTERS;
		p_cntrs = ((DAPL_IA *) dh)->cntrs;
		break;
	case DAT_HANDLE_TYPE_EP:
		max = DCNT_EP_ALL_COUNTERS;
		p_cntrs = ((DAPL_EP *) dh)->cntrs;
		break;
	case DAT_HANDLE_TYPE_EVD:
		max = DCNT_EVD_ALL_COUNTERS;
		p_cntrs = ((DAPL_EVD *) dh)->cntrs;
		break;
	default:
		return DAT_INVALID_HANDLE;
	}

	for (i = 0; i < max; i++) {
		if ((counter == i) || (counter == max)) {
			((DAT_UINT64 *) p_cntrs_out)[i] = p_cntrs[i];
			if (reset)
				p_cntrs[i] = 0;
		}
	}
	return DAT_SUCCESS;
}

char *dapl_query_counter_name(DAT_HANDLE dh, int counter)
{
	DAT_HANDLE_TYPE type = 0;

	dat_get_handle_type(dh, &type);

	switch (type) {
	case DAT_HANDLE_TYPE_IA:
		if (counter < DCNT_IA_ALL_COUNTERS)
			return ia_cntr_names[counter];
		break;
	case DAT_HANDLE_TYPE_EP:
		if (counter < DCNT_EP_ALL_COUNTERS)
			return ep_cntr_names[counter];
		break;
	case DAT_HANDLE_TYPE_EVD:
		if (counter < DCNT_EVD_ALL_COUNTERS)
			return evd_cntr_names[counter];
		break;
	default:
		return NULL;
	}
	return NULL;
}

void dapl_print_counter(DAT_HANDLE dh, int counter, int reset)
{
	int i, max;
	DAT_UINT64 *p_cntrs;
	DAT_HANDLE_TYPE type = 0;

	dat_get_handle_type(dh, &type);

	switch (type) {
	case DAT_HANDLE_TYPE_IA:
		max = DCNT_IA_ALL_COUNTERS;
		p_cntrs = ((DAPL_IA *) dh)->cntrs;
		break;
	case DAT_HANDLE_TYPE_EP:
		max = DCNT_EP_ALL_COUNTERS;
		p_cntrs = ((DAPL_EP *) dh)->cntrs;
		break;
	case DAT_HANDLE_TYPE_EVD:
		max = DCNT_EVD_ALL_COUNTERS;
		p_cntrs = ((DAPL_EVD *) dh)->cntrs;
		break;
	default:
		return;
	}

	for (i = 0; i < max; i++) {
		if ((counter == i) || (counter == max)) {
			printf(" %s = " F64u " \n",
			       dapl_query_counter_name(dh, i), p_cntrs[i]);
			if (reset)
				p_cntrs[i] = 0;
		}
	}

	/* Print in process CR's for this IA, if debug type set */
	if ((type == DAT_HANDLE_TYPE_IA) && 
	    (g_dapl_dbg_type & DAPL_DBG_TYPE_CM_LIST)) {
		dapls_print_cm_list((DAPL_IA*)dh);
	}
	return;
}

#endif				/* DAPL_COUNTERS */
