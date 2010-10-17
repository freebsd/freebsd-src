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
 * HEADER: dapl_ep_util.h
 *
 * PURPOSE: Utility defs & routines for the EP data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_EP_UTIL_H_
#define _DAPL_EP_UTIL_H_

#include "dapl.h"
#include "dapl_adapter_util.h"

/* function prototypes */

extern DAPL_EP * 
dapl_ep_alloc (
    IN DAPL_IA			*ia,
    IN const DAT_EP_ATTR	*ep_attr );

extern void 
dapl_ep_dealloc (
    IN DAPL_EP			*ep_ptr );

extern DAT_RETURN 
dapl_ep_check_recv_completion_flags (
    DAT_COMPLETION_FLAGS        flags );

extern DAT_RETURN 
dapl_ep_check_request_completion_flags (
    DAT_COMPLETION_FLAGS        flags );

extern DAT_RETURN
dapl_ep_post_send_req (
    IN	DAT_EP_HANDLE		ep_handle,
    IN	DAT_COUNT		num_segments,
    IN	DAT_LMR_TRIPLET		*local_iov,
    IN	DAT_DTO_COOKIE		user_cookie,
    IN	const DAT_RMR_TRIPLET	*remote_iov,
    IN	DAT_COMPLETION_FLAGS	completion_flags,
    IN  DAPL_DTO_TYPE 		dto_type,
    IN  int			op_type );

void dapls_ep_timeout (uintptr_t	arg );

DAT_RETURN_SUBTYPE
dapls_ep_state_subtype(
    IN  DAPL_EP			*ep_ptr );

extern void
dapl_ep_legacy_post_disconnect(
    DAPL_EP		*ep_ptr,
    DAT_CLOSE_FLAGS	disconnect_flags);

extern char *dapl_get_ep_state_str(DAT_EP_STATE state);
 
#endif /*  _DAPL_EP_UTIL_H_ */
