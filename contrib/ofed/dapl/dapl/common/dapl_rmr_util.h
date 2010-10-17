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
 * HEADER: dapl_rmr_util.h
 *
 * PURPOSE: Utility defs & routines for the RMR data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_RMR_UTIL_H_
#define _DAPL_RMR_UTIL_H_

#include "dapl_mr_util.h"

/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

extern DAPL_RMR *
dapl_rmr_alloc (
    IN  DAPL_PZ 		*pz);

extern void
dapl_rmr_dealloc (
    IN  DAPL_RMR 		*rmr);

STATIC _INLINE_ DAT_BOOLEAN 
dapl_rmr_validate_completion_flag (
    IN  DAT_COMPLETION_FLAGS 	mask,
    IN  DAT_COMPLETION_FLAGS 	allow,
    IN  DAT_COMPLETION_FLAGS 	request);


/*********************************************************************
 *                                                                   *
 * Inline Functions                                                  *
 *                                                                   *
 *********************************************************************/

STATIC _INLINE_ DAT_BOOLEAN 
dapl_rmr_validate_completion_flag (
    IN  DAT_COMPLETION_FLAGS 	mask,
    IN  DAT_COMPLETION_FLAGS 	allow,
    IN  DAT_COMPLETION_FLAGS 	request)
{
    if ( (mask & request ) && !(mask & allow) )
    {
	return DAT_FALSE;
    }
    else
    {
	return DAT_TRUE;
    }
}

#endif /* _DAPL_RMR_UTIL_H_*/
