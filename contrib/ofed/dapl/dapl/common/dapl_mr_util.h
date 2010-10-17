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
 * HEADER: dapl_mr_util.h
 *
 * PURPOSE: Utility defs & routines for memory registration functions
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_MR_UTIL_H_
#define _DAPL_MR_UTIL_H_

#include "dapl.h"
#include "dapl_hash.h"


/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

extern DAT_VADDR
dapl_mr_get_address (
    IN 	DAT_REGION_DESCRIPTION 		desc, 
    IN 	DAT_MEM_TYPE 			type);

STATIC _INLINE_ DAT_BOOLEAN
dapl_mr_bounds_check (
    IN 	DAT_VADDR 	addr_a, 
    IN  DAT_VLEN 	length_a,
    IN 	DAT_VADDR 	addr_b, 
    IN 	DAT_VLEN 	length_b);


/*********************************************************************
 *                                                                   *
 * Inline Functions	                                             *
 *                                                                   *
 *********************************************************************/

/*
 * dapl_mr_bounds_check
 *
 * Returns true if region B is contained within region A
 * and false otherwise
 *
 */

STATIC _INLINE_ DAT_BOOLEAN
dapl_mr_bounds_check (
    IN  DAT_VADDR 	addr_a, 
    IN  DAT_VLEN 	length_a,
    IN  DAT_VADDR 	addr_b, 
    IN  DAT_VLEN 	length_b)
{
    if ( (addr_a <= addr_b) &&
         (addr_b + length_b) <= (addr_a + length_a))
    {
	return DAT_TRUE;
    }
    else
    {
	return DAT_FALSE;
    }
}

#endif /* _DAPL_MR_UTIL_H_ */
