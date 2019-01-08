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

#include "dapl_mr_util.h"

/**********************************************************************
 * 
 * MODULE: dapl_mr_util.c
 *
 * PURPOSE: Common Memory Management functions and data structures
 *
 * $Id:$
 **********************************************************************/

/*********************************************************************
 *                                                                   *
 * Function Definitions	                                             *
 *                                                                   *
 *********************************************************************/

/*
 * dapl_mr_get_address
 *
 * Returns the memory address associated with the given memory descriptor
 *
 * Input:
 * 	desc		memory descriptor
 * 	type		type of memory represented by desc
 *
 * Output:
 * 	None
 *
 */

DAT_VADDR
dapl_mr_get_address(IN DAT_REGION_DESCRIPTION desc, IN DAT_MEM_TYPE type)
{
	switch (type) {
	case DAT_MEM_TYPE_VIRTUAL:
		{
			return (DAT_VADDR) (uintptr_t) desc.for_va;
		}
	case DAT_MEM_TYPE_LMR:
		{
			DAPL_LMR *lmr;

			lmr = (DAPL_LMR *) desc.for_lmr_handle;

			/* Since this function is recoursive we cannot inline it */
			return dapl_mr_get_address(lmr->param.region_desc,
						   lmr->param.mem_type);
		}
#if defined(__KDAPL__)
	case DAT_MEM_TYPE_PHYSICAL:
		{
			return desc.for_pa;
		}
#else
	case DAT_MEM_TYPE_SHARED_VIRTUAL:
		{
			/* multi-cast necessary to convert a pvoid to a DAT_VADDR on
			 * all architectures
			 */
			return (DAT_VADDR) (uintptr_t) desc.for_shared_memory.
			    virtual_address;
		}
#endif				/* defined(__KDAPL__) */
	default:
		{
			/*
			 * The following kDAPL memory types have not been implemented:
			 *    DAT_MEM_TYPE_PLATFORM
			 *    DAT_MEM_TYPE_IA
			 *    DAT_MEM_TYPE_BYPASS
			 */
			dapl_os_assert(0);
			return 0;
		}
	}
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  c-brace-offset: -4
 *  tab-width: 8
 * End:
 */
