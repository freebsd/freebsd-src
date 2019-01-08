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
 * HEADER: dapl_lmr_util.h
 *
 * PURPOSE: Utility defs & routines for the LMR data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_LMR_UTIL_H_
#define _DAPL_LMR_UTIL_H_

#include "dapl_mr_util.h"

/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

extern DAPL_LMR *
dapl_lmr_alloc (
    IN DAPL_IA 			*ia, 
    IN DAT_MEM_TYPE	 	mem_type, 
    IN DAT_REGION_DESCRIPTION	region_desc,
    IN DAT_VLEN			length,
    IN DAT_PZ_HANDLE		pz_handle,
    IN DAT_MEM_PRIV_FLAGS	mem_priv);

extern void
dapl_lmr_dealloc (
    IN DAPL_LMR 		*lmr);


#endif /* _DAPL_LMR_UTIL_H_*/
