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
 * HEADER: dapl_cookie.h
 *
 * PURPOSE: Utility defs & routines for the cookie data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_COOKIE_H
#define _DAPL_COOKIE_H_

#include "dapl.h"

extern DAT_RETURN 
dapls_cb_create (
    DAPL_COOKIE_BUFFER		*buffer,
    DAPL_EP			*ep,
    DAT_COUNT			size );

extern void 
dapls_cb_free (
    DAPL_COOKIE_BUFFER		*buffer );

extern DAT_COUNT 
dapls_cb_pending (
    DAPL_COOKIE_BUFFER		*buffer );

extern DAT_RETURN
dapls_rmr_cookie_alloc (
    IN  DAPL_COOKIE_BUFFER	*buffer,
    IN 	DAPL_RMR		*rmr,
    IN 	DAT_RMR_COOKIE          user_cookie,
    OUT DAPL_COOKIE 		**cookie_ptr );

extern DAT_RETURN
dapls_dto_cookie_alloc (
    IN  DAPL_COOKIE_BUFFER	*buffer,
    IN  DAPL_DTO_TYPE		type,
    IN 	DAT_DTO_COOKIE	   	user_cookie,
    OUT DAPL_COOKIE 		**cookie_ptr );

extern void
dapls_cookie_dealloc (
    IN  DAPL_COOKIE_BUFFER	*buffer,
    IN 	DAPL_COOKIE		*cookie );

#endif /* _DAPL_COOKIE_H_ */
