/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)procfs_status.c	8.3 (Berkeley) 2/17/94
 *
 *	$Id: procfs_map.c,v 1.1 1996/06/17 22:53:27 dyson Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <miscfs/procfs/procfs.h>
#include <sys/queue.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_inherit.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <vm/default_pager.h>


#define MAXKBUFFER 16384

int
procfs_domap(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	int len;
	int error;
	/*
	 * dynamically allocated buffer for entire snapshot
	 */
	char *kbuffer, *kbufferp;
	/*
	 * buffer for each map entry
	 */
	char mebuffer[256];
	vm_map_t map = &p->p_vmspace->vm_map;
	pmap_t pmap = &p->p_vmspace->vm_pmap;
	vm_map_entry_t entry;

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	if (uio->uio_offset != 0)
		return (0);
	
	kbuffer = (char *)kmem_alloc_pageable(kernel_map, MAXKBUFFER);
	kbufferp = kbuffer;
	vm_map_lock(map);
	for (entry = map->header.next; entry != &map->header;
		entry = entry->next) {
		vm_object_t obj, tobj, lobj;
		vm_offset_t addr;
		int resident, privateresident;
		char *type;

		if (entry->is_a_map || entry->is_sub_map)
			continue;

		obj = entry->object.vm_object;
		if (obj && (obj->ref_count == 1))
			privateresident = obj->resident_page_count;
		else
			privateresident = 0;

		resident = 0;
		addr = entry->start;
		while (addr < entry->end) {
			if (pmap_extract( pmap, addr))
				resident++;
			addr += PAGE_SIZE;
		}

		for( lobj = tobj = obj; tobj; tobj = tobj->backing_object)
			lobj = tobj;

		switch(lobj->type) {

default:
case OBJT_DEFAULT:
			type = "default";
			break;
case OBJT_VNODE:
			type = "vnode";
			break;
case OBJT_SWAP:
			type = "swap";
			break;
case OBJT_DEVICE:
			type = "device";
			break;
		}

		/*
		 * format:
		 *  start, end, resident, private resident, cow, access, type.
		 */
		sprintf(mebuffer, "0x%-8.8x 0x%-8.8x %9d %9d %s %s %s\n",
			entry->start, entry->end,
			resident, privateresident,
			(entry->protection & VM_PROT_WRITE)?"RW":"RO",
			entry->copy_on_write?"COW":"   ",
			type);

		len = strlen(mebuffer);
		if (len + (kbufferp - kbuffer) < MAXKBUFFER) {
			memcpy(kbufferp, mebuffer, len);
			kbufferp += len;
		}
	}
	vm_map_unlock(map);
	error = uiomove(kbuffer, (kbufferp - kbuffer), uio);
	kmem_free(kernel_map, (vm_offset_t)kbuffer, MAXKBUFFER);
	return error;
}

int
procfs_validmap(p)
	struct proc *p;
{
	return ((p->p_flag & P_SYSTEM) == 0);
}
