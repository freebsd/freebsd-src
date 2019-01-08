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
 * HEADER: dapl_cr_util.h
 *
 * PURPOSE: Utility defs & routines for the CR data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_CR_UTIL_H_
#define _DAPL_CR_UTIL_H_

#include "dapl.h" 

DAPL_CR	*
dapls_cr_alloc (
	DAPL_IA		*ia_ptr );

void
dapls_cr_free (
	IN DAPL_CR		*cr_ptr );

void
dapls_cr_callback (
    IN    dp_ib_cm_handle_t     ib_cm_handle,
    IN    const ib_cm_events_t  ib_cm_event,
    IN	  const void 		*instant_data_p,
    IN    const void         	*context);

#endif /* _DAPL_CR_UTIL_H_ */
