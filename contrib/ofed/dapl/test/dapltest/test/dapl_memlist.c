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

void DT_MemListInit(Per_Test_Data_t * pt_ptr)
{
	DT_Mdep_LockInit(&pt_ptr->MemListLock);
	pt_ptr->MemListHead = 0;
}

void *DT_MemListAlloc(Per_Test_Data_t * pt_ptr,
		      char *file, mem_type_e t, int size)
{
	void *buffptr;
	MemListEntry_t *entry_ptr;
	buffptr = 0;
	entry_ptr = 0;

	buffptr = DT_Mdep_Malloc(size);
	if (buffptr == 0) {
		return 0;
	}
	if (pt_ptr == 0) {	/* not use mem_list */
		return buffptr;
	}
	entry_ptr = (MemListEntry_t *) DT_Mdep_Malloc(sizeof(MemListEntry_t));
	if (entry_ptr == 0) {
		DT_Mdep_Free(buffptr);
		return 0;
	}
	strcpy(entry_ptr->filename, file);
	entry_ptr->MemType = t;
	entry_ptr->mem_ptr = buffptr;

	DT_Mdep_Lock(&pt_ptr->MemListLock);
	entry_ptr->next = pt_ptr->MemListHead;
	pt_ptr->MemListHead = entry_ptr;
	DT_Mdep_Unlock(&pt_ptr->MemListLock);

	return buffptr;
}

void DT_MemListFree(Per_Test_Data_t * pt_ptr, void *ptr)
{
	MemListEntry_t *pre, *cur;
	if (pt_ptr == 0) {	/* not use mem_list */
		DT_Mdep_Free(ptr);
		return;
	}
	DT_Mdep_Lock(&pt_ptr->MemListLock);
	pre = 0;
	cur = pt_ptr->MemListHead;
	while (cur) {
		if (cur->mem_ptr == ptr) {
			if (!pre) {	/* first entry */
				pt_ptr->MemListHead = cur->next;
				cur->next = 0;
			} else {
				pre->next = cur->next;
				cur->next = 0;
			}
			DT_Mdep_Free(ptr);
			DT_Mdep_Free(cur);
			goto unlock_and_return;
		}
		pre = cur;
		cur = cur->next;
	}
      unlock_and_return:
	DT_Mdep_Unlock(&pt_ptr->MemListLock);
}

void DT_PrintMemList(Per_Test_Data_t * pt_ptr)
{
	char *type[10] = {
		"BPOOL", "BUFF", "PERTESTDATA", "NIC", "NETADDRESS",
		"TRANSACTIONTEST", "THREAD", "EPCONTEXT"
	};
	DT_Tdep_Print_Head *phead;
	MemListEntry_t *cur;

	phead = pt_ptr->Params.phead;
	DT_Mdep_Lock(&pt_ptr->MemListLock);
	cur = pt_ptr->MemListHead;
	if (cur != 0) {
		DT_Tdep_PT_Printf(phead,
				  "the allocated memory that have not been returned are:\n");
	}
	while (cur) {
		DT_Tdep_PT_Printf(phead, "file: dapl_%s, \tMemType:%s\n",
				  cur->filename, type[cur->MemType]);
		cur = cur->next;
	}
	DT_Mdep_Unlock(&pt_ptr->MemListLock);
}
