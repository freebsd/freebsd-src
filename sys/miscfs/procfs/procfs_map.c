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
 *	$Id: procfs_map.c,v 1.12 1997/08/02 14:32:12 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>


#define MEBUFFERSIZE 256

/*
 * The map entries can *almost* be read with programs like cat.  However,
 * large maps need special programs to read.  It is not easy to implement
 * a program that can sense the required size of the buffer, and then
 * subsequently do a read with the appropriate size.  This operation cannot
 * be atomic.  The best that we can do is to allow the program to do a read
 * with an arbitrarily large buffer, and return as much as we can.  We can
 * return an error code if the buffer is too small (EFBIG), then the program
 * can try a bigger buffer.
 */
int
procfs_domap(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	int len;
	int error;
	vm_map_t map = &p->p_vmspace->vm_map;
	pmap_t pmap = &p->p_vmspace->vm_pmap;
	vm_map_entry_t entry;
	char mebuffer[MEBUFFERSIZE];

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	if (uio->uio_offset != 0)
		return (0);
	
	error = 0;
	if (map != &curproc->p_vmspace->vm_map)
		vm_map_lock_read(map);
	for (entry = map->header.next;
		((uio->uio_resid > 0) && (entry != &map->header));
		entry = entry->next) {
		vm_object_t obj, tobj, lobj;
		vm_offset_t addr;
		int resident, privateresident;
		char *type;

		if (entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP))
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

		if (lobj) switch(lobj->type) {

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
		} else {
			type = "none";
		}
			

		/*
		 * format:
		 *  start, end, resident, private resident, cow, access, type.
		 */
		sprintf(mebuffer, "0x%-8.8x 0x%-8.8x %9d %9d %s%s%s %s %s\n",
			entry->start, entry->end,
			resident, privateresident,
			(entry->protection & VM_PROT_READ)?"r":"-",
			(entry->protection & VM_PROT_WRITE)?"w":"-",
			(entry->protection & VM_PROT_EXECUTE)?"x":"-",
			(entry->eflags & MAP_ENTRY_COW)?"COW":"   ",
			type);

		len = strlen(mebuffer);
		if (len > uio->uio_resid) {
			error = EFBIG;
			break;
		}
		error = uiomove(mebuffer, len, uio);
		if (error)
			break;
	}
	if (map != &curproc->p_vmspace->vm_map)
		vm_map_unlock_read(map);
	return error;
}

int
procfs_validmap(p)
	struct proc *p;
{
	return ((p->p_flag & P_SYSTEM) == 0);
}
