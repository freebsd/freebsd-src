/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993 Sean Eric Fagan
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry and Sean Eric Fagan.
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
 *	@(#)procfs_mem.c	8.4 (Berkeley) 1/21/94
 *
 *	$Id: procfs_mem.c,v 1.11 1995/12/03 14:54:35 bde Exp $
 */

/*
 * This is a lightly hacked and merged version
 * of sef's pread/pwrite functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

static int	procfs_rwmem __P((struct proc *p, struct uio *uio));

static int
procfs_rwmem(p, uio)
	struct proc *p;
	struct uio *uio;
{
	int error;
	int writing;

	writing = uio->uio_rw == UIO_WRITE;

	/*
	 * Only map in one page at a time.  We don't have to, but it
	 * makes things easier.  This way is trivial - right?
	 */
	do {
		vm_map_t map, tmap;
		vm_object_t object;
		vm_offset_t kva = 0;
		vm_offset_t uva;
		int page_offset;		/* offset into page */
		vm_offset_t pageno;		/* page number */
		vm_map_entry_t out_entry;
		vm_prot_t out_prot;
		vm_page_t m;
		boolean_t wired, single_use;
		vm_offset_t off;
		u_int len;
		int fix_prot;

		uva = (vm_offset_t) uio->uio_offset;
		if (uva >= VM_MAXUSER_ADDRESS) {
			if (writing || (uva >= (VM_MAXUSER_ADDRESS + UPAGES * PAGE_SIZE))) {
				error = 0;
				break;
			}
		}

		/*
		 * Get the page number of this segment.
		 */
		pageno = trunc_page(uva);
		page_offset = uva - pageno;

		/*
		 * How many bytes to copy
		 */
		len = min(PAGE_SIZE - page_offset, uio->uio_resid);

		/*
		 * The map we want...
		 */
		map = &p->p_vmspace->vm_map;

		/*
		 * Check the permissions for the area we're interested
		 * in.
		 */
		fix_prot = 0;
		if (writing)
			fix_prot = !vm_map_check_protection(map, pageno,
					pageno + PAGE_SIZE, VM_PROT_WRITE);

		if (fix_prot) {
			/*
			 * If the page is not writable, we make it so.
			 * XXX It is possible that a page may *not* be
			 * read/executable, if a process changes that!
			 * We will assume, for now, that a page is either
			 * VM_PROT_ALL, or VM_PROT_READ|VM_PROT_EXECUTE.
			 */
			error = vm_map_protect(map, pageno,
					pageno + PAGE_SIZE, VM_PROT_ALL, 0);
			if (error)
				break;
		}

		/*
		 * Now we need to get the page.  out_entry, out_prot, wired,
		 * and single_use aren't used.  One would think the vm code
		 * would be a *bit* nicer...  We use tmap because
		 * vm_map_lookup() can change the map argument.
		 */
		tmap = map;
		error = vm_map_lookup(&tmap, pageno,
				      writing ? VM_PROT_WRITE : VM_PROT_READ,
				      &out_entry, &object, &off, &out_prot,
				      &wired, &single_use);
		/*
		 * We're done with tmap now.
		 */
		if (!error)
			vm_map_lookup_done(tmap, out_entry);

		/*
		 * Fault the page in...
		 */
		if (!error && writing && object->backing_object) {
			m = vm_page_lookup(object, off);
			if (m == 0)
				error = vm_fault(map, pageno,
							VM_PROT_WRITE, FALSE);
		}

		/* Find space in kernel_map for the page we're interested in */
		if (!error)
			error = vm_map_find(kernel_map, object, off, &kva,
					PAGE_SIZE, 1);

		if (!error) {
			/*
			 * Neither vm_map_lookup() nor vm_map_find() appear
			 * to add a reference count to the object, so we do
			 * that here and now.
			 */
			vm_object_reference(object);

			/*
			 * Mark the page we just found as pageable.
			 */
			error = vm_map_pageable(kernel_map, kva,
				kva + PAGE_SIZE, 0);

			/*
			 * Now do the i/o move.
			 */
			if (!error)
				error = uiomove((caddr_t)(kva + page_offset),
						len, uio);

			vm_map_remove(kernel_map, kva, kva + PAGE_SIZE);
		}
		if (fix_prot)
			vm_map_protect(map, pageno, pageno + PAGE_SIZE,
					VM_PROT_READ|VM_PROT_EXECUTE, 0);
	} while (error == 0 && uio->uio_resid > 0);

	return (error);
}

/*
 * Copy data in and out of the target process.
 * We do this by mapping the process's page into
 * the kernel and then doing a uiomove direct
 * from the kernel address space.
 */
int
procfs_domem(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	int error;

	if (uio->uio_resid == 0)
		return (0);

	error = procfs_rwmem(p, uio);

	return (error);
}

/*
 * Given process (p), find the vnode from which
 * it's text segment is being executed.
 *
 * It would be nice to grab this information from
 * the VM system, however, there is no sure-fire
 * way of doing that.  Instead, fork(), exec() and
 * wait() all maintain the p_textvp field in the
 * process proc structure which contains a held
 * reference to the exec'ed vnode.
 */
struct vnode *
procfs_findtextvp(p)
	struct proc *p;
{
	return (p->p_textvp);
}
