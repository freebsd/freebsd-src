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
 *	@(#)procfs_mem.c	8.5 (Berkeley) 6/15/94
 *
 *	$Id: procfs_mem.c,v 1.31 1998/04/17 22:36:55 des Exp $
 */

/*
 * This is a lightly hacked and merged version
 * of sef's pread/pwrite functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <sys/user.h>
#include <sys/ptrace.h>

static int	procfs_rwmem __P((struct proc *curp,
				  struct proc *p, struct uio *uio));

static int
procfs_rwmem(curp, p, uio)
	struct proc *curp;
	struct proc *p;
	struct uio *uio;
{
	int error;
	int writing;
	struct vmspace *vm;
	vm_map_t map;
	vm_object_t object = NULL;
	vm_offset_t pageno = 0;		/* page number */
	vm_prot_t reqprot;
	vm_offset_t kva;

	/*
	 * if the vmspace is in the midst of being deallocated or the
	 * process is exiting, don't try to grab anything.  The page table
	 * usage in that process can be messed up.
	 */
	vm = p->p_vmspace;
	if ((p->p_flag & P_WEXIT) || (vm->vm_refcnt < 1))
		return EFAULT;
	++vm->vm_refcnt;
	/*
	 * The map we want...
	 */
	map = &vm->vm_map;

	writing = uio->uio_rw == UIO_WRITE;
	reqprot = writing ? (VM_PROT_WRITE | VM_PROT_OVERRIDE_WRITE) : VM_PROT_READ;

	kva = kmem_alloc_pageable(kernel_map, PAGE_SIZE);

	/*
	 * Only map in one page at a time.  We don't have to, but it
	 * makes things easier.  This way is trivial - right?
	 */
	do {
		vm_map_t tmap;
		vm_offset_t uva;
		int page_offset;		/* offset into page */
		vm_map_entry_t out_entry;
		vm_prot_t out_prot;
		boolean_t wired;
		vm_pindex_t pindex;
		u_int len;
		vm_page_t m;

		object = NULL;

		uva = (vm_offset_t) uio->uio_offset;

		/*
		 * Get the page number of this segment.
		 */
		pageno = trunc_page(uva);
		page_offset = uva - pageno;

		/*
		 * How many bytes to copy
		 */
		len = min(PAGE_SIZE - page_offset, uio->uio_resid);

		if (uva >= VM_MAXUSER_ADDRESS) {
			vm_offset_t tkva;

			if (writing || 
			    uva >= VM_MAXUSER_ADDRESS + UPAGES * PAGE_SIZE ||
			    (ptrace_read_u_check(p,
						 uva - (vm_offset_t) VM_MAXUSER_ADDRESS,
						 (size_t) len) &&
			     !procfs_kmemaccess(curp))) {
				error = 0;
				break;
			}

			/* we are reading the "U area", force it into core */
			PHOLD(p);

			/* sanity check */
			if (!(p->p_flag & P_INMEM)) {
				/* aiee! */
				PRELE(p);
				error = EFAULT;
				break;
			}

			/* populate the ptrace/procfs area */
			p->p_addr->u_kproc.kp_proc = *p;
			fill_eproc (p, &p->p_addr->u_kproc.kp_eproc);

			/* locate the in-core address */
			tkva = (u_int)p->p_addr + uva - VM_MAXUSER_ADDRESS;

			/* transfer it */
			error = uiomove((caddr_t)tkva, len, uio);

			/* let the pages go */
			PRELE(p);

			continue;
		}

		/*
		 * Fault the page on behalf of the process
		 */
		error = vm_fault(map, pageno, reqprot, FALSE);
		if (error) {
			error = EFAULT;
			break;
		}

		/*
		 * Now we need to get the page.  out_entry, out_prot, wired,
		 * and single_use aren't used.  One would think the vm code
		 * would be a *bit* nicer...  We use tmap because
		 * vm_map_lookup() can change the map argument.
		 */
		tmap = map;
		error = vm_map_lookup(&tmap, pageno, reqprot,
			      &out_entry, &object, &pindex, &out_prot,
			      &wired);

		if (error) {
			error = EFAULT;

			/*
			 * Make sure that there is no residue in 'object' from
			 * an error return on vm_map_lookup.
			 */
			object = NULL;

			break;
		}

		m = vm_page_lookup(object, pindex);

		/* Allow fallback to backing objects if we are reading */

		while (m == NULL && !writing && object->backing_object) {

		  pindex += OFF_TO_IDX(object->backing_object_offset);
		  object = object->backing_object;

		  m = vm_page_lookup(object, pindex);
		}

		if (m == NULL) {
			error = EFAULT;

			/*
			 * Make sure that there is no residue in 'object' from
			 * an error return on vm_map_lookup.
			 */
			object = NULL;

			vm_map_lookup_done(tmap, out_entry);

			break;
		}

		/*
		 * Wire the page into memory
		 */
		vm_page_wire(m);

		/*
		 * We're done with tmap now.
		 * But reference the object first, so that we won't loose
		 * it.
		 */
		vm_object_reference(object);
		vm_map_lookup_done(tmap, out_entry);

		pmap_kenter(kva, VM_PAGE_TO_PHYS(m));

		/*
		 * Now do the i/o move.
		 */
		error = uiomove((caddr_t)(kva + page_offset), len, uio);

		pmap_kremove(kva);

		/*
		 * release the page and the object
		 */
		vm_page_unwire(m);
		vm_object_deallocate(object);

		object = NULL;

	} while (error == 0 && uio->uio_resid > 0);

	if (object)
		vm_object_deallocate(object);

	kmem_free(kernel_map, kva, PAGE_SIZE);
	vmspace_free(vm);
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

	if (uio->uio_resid == 0)
		return (0);

 	/*
 	 * XXX
 	 * We need to check for KMEM_GROUP because ps is sgid kmem;
 	 * not allowing it here causes ps to not work properly.  Arguably,
 	 * this is a bug with what ps does.  We only need to do this
 	 * for Pmem nodes, and only if it's reading.  This is still not
 	 * good, as it may still be possible to grab illicit data if
 	 * a process somehow gets to be KMEM_GROUP.  Note that this also
 	 * means that KMEM_GROUP can't change without editing procfs.h!
 	 * All in all, quite yucky.
 	 */
 
 	if (!CHECKIO(curp, p) &&
	    !(uio->uio_rw == UIO_READ &&
	      procfs_kmemaccess(curp)))
 		return EPERM;

	return (procfs_rwmem(curp, p, uio));
}

/*
 * Given process (p), find the vnode from which
 * its text segment is being executed.
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

int procfs_kmemaccess(curp)
	struct proc *curp;
{
	int i;
	struct ucred *cred;

	cred = curp->p_cred->pc_ucred;
	if (suser(cred, &curp->p_acflag))
		return 1;
	
	for (i = 0; i < cred->cr_ngroups; i++)
		if (cred->cr_groups[i] == KMEM_GROUP)
			return 1;
	
	return 0;
}
