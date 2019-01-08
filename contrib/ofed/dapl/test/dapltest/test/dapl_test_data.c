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

#include "dapl_proto.h"

DT_Mdep_LockType g_PerfTestLock;
/*
 * check memory leaking int             alloc_count; DT_Mdep_LockType
 * Alloc_Count_Lock;
 */

Per_Test_Data_t *DT_Alloc_Per_Test_Data(DT_Tdep_Print_Head * phead)
{
	Per_Test_Data_t *pt_ptr;
	pt_ptr = 0;

	pt_ptr = DT_Mdep_Malloc(sizeof(Per_Test_Data_t));
	if (!pt_ptr) {
		DT_Tdep_PT_Printf(phead,
				  "No Memory to create per_test_data!\n");
	}

	return (pt_ptr);
}

void DT_Free_Per_Test_Data(Per_Test_Data_t * pt_ptr)
{
	DT_Mdep_Free(pt_ptr);
}
