/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: procfs_subr.c,v 1.4 1994/01/14 16:25:04 davidg Exp $
 */
#include "param.h"
#include "systm.h"
#include "time.h"
#include "kernel.h"
#include "ioctl.h"
#include "proc.h"
#include "buf.h"
#include "vnode.h"
#include "file.h"
#include "resourcevar.h"
#include "vm/vm.h"
#include "vm/vm_page.h"
#include "vm/vm_kern.h"
#include "vm/vm_user.h"
#include "kinfo.h"
#include "kinfo_proc.h"
#include "machine/pmap.h"

#include "procfs.h"
#include "pfsnode.h"

#include "machine/vmparam.h"

/*
 * Get process address map (PIOCGMAP)
 */
int
pfs_vmmap(procp, pfsp, pmapp)
struct proc	*procp;
struct nfsnode	*pfsp;
struct procmap	*pmapp;
{
	int		error = 0;
	vm_map_t	map;
	vm_map_entry_t	entry;
	struct procmap	prmap;

	map = &procp->p_vmspace->vm_map;
	vm_map_lock(map);
	entry = map->header.next;

	while (entry != &map->header) {
		if (entry->is_a_map) {
			vm_map_t	submap = entry->object.share_map;
			vm_map_entry_t	subentry;

			vm_map_lock(submap);
			subentry = submap->header.next;
			while (subentry != &submap->header) {
				prmap.vaddr = subentry->start;
				prmap.size = subentry->end - subentry->start;
				prmap.offset = subentry->offset;
				prmap.prot = subentry->protection;
				error = copyout(&prmap, pmapp, sizeof(prmap));
				if (error)
					break;
				pmapp++;
				subentry = subentry->next;
			}
			vm_map_unlock(submap);
			if (error)
				break;
		}
		prmap.vaddr = entry->start;
		prmap.size = entry->end - entry->start;
		prmap.offset = entry->offset;
		prmap.prot = entry->protection;
		error = copyout(&prmap, pmapp, sizeof(prmap));
		if (error)
			break;
		pmapp++;
		entry = entry->next;
	}

	vm_map_unlock(map);
	return error;
}

/*
 * Count number of VM entries of process (PIOCNMAP)
 */
int
pfs_vm_nentries(procp, pfsp)
struct proc	*procp;
struct nfsnode	*pfsp;
{
	int		count = 0;
	vm_map_t	map;
	vm_map_entry_t	entry;

	map = &procp->p_vmspace->vm_map;
	vm_map_lock(map);
	entry = map->header.next;

	while (entry != &map->header) {
		if (entry->is_a_map)
			count += entry->object.share_map->nentries;
		else
			count++;
		entry = entry->next;
	}

	vm_map_unlock(map);
	return count;
}

/*
 * Map process mapped file to file descriptor (PIOCGMAPFD)
 */
int
pfs_vmfd(procp, pfsp, vmfdp, p)
struct proc	*procp;
struct pfsnode	*pfsp;
struct vmfd	*vmfdp;
struct proc	*p;
{
	int		rv;
	vm_map_t	map;
	vm_offset_t	addr;
	vm_size_t	size;
	vm_prot_t	prot, maxprot;
	vm_inherit_t	inherit;
	boolean_t	shared;
	vm_object_t	object;
	vm_offset_t	objoff;
	struct vnode	*vp;
	struct file	*fp;
	extern struct fileops	vnops;

	map = &procp->p_vmspace->vm_map;

	addr = vmfdp->vaddr;
	rv = vm_region(map, &addr, &size, &prot, &maxprot,
			&inherit, &shared, &object, &objoff);

	if (rv != KERN_SUCCESS)
		return EINVAL;

	while (object != NULL && object->pager == NULL)
		object = object->shadow;

	if (object == NULL || object->pager == NULL
			/* Nobody seems to care || !object->pager_ready */ )
		return ENOENT;

	if (object->pager->pg_type != PG_VNODE)
		return ENOENT;

	/* We have a vnode pager, allocate file descriptor */
	vp = (struct vnode *)object->pager->pg_handle;
	if (VOP_ACCESS(vp, VREAD, p->p_ucred, p)) {
		rv = EACCES;
		goto out;
	}
	rv = falloc(p, &fp, &vmfdp->fd);
	if (rv)
		goto out;

	VREF(vp);
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)vp;
	fp->f_flag = FREAD;

out:
	vm_object_unlock(object);
	return rv;
}


/*
 * Vnode op for reading/writing.
 */
/* ARGSUSED */
int
pfs_doio(vp, uio, ioflag, cred)
	struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct pfsnode	*pfsp = VTOPFS(vp);
	struct proc	*procp;
	int		error = 0;
	long		n, on;
	vm_offset_t	kva, kbuf;
	int		pflag;

#ifdef DEBUG
	if (pfs_debug)
		printf("pfs_doio(%s): vp 0x%x, proc %x, offset %d\n",
			uio->uio_rw==UIO_READ?"R":"W", vp, uio->uio_procp, uio->uio_offset);
#endif

#ifdef DIAGNOSTIC
	if (vp->v_type != VPROC)
		panic("pfs_doio vtype");
#endif
	procp = pfsp->pfs_pid?pfind(pfsp->pfs_pid):&proc0;
	if (!procp)
		return ESRCH;

	if (procp->p_flag & SSYS)
		return EACCES;

	if (uio->uio_resid == 0) {
		return (0);
	}
	/* allocate a bounce buffer */
	/* notice that this bounce buffer bogosity is due to
	   a problem with wiring/unwiring procs pages, so
	   rather than wire the destination procs data pages
	   I used a kernel bounce buffer
	*/
	kbuf = kmem_alloc(kernel_map, NBPG); 
	if( !kbuf)
		return ENOMEM;

	/* allocate a kva */
	kva = kmem_alloc_pageable(kernel_map, NBPG);
	if( !kva) {
		kmem_free(kernel_map, kbuf, NBPG);
		return ENOMEM;
	}

	pflag = procp->p_flag & SKEEP;
	procp->p_flag |= SKEEP;

	do { /* One page at a time */
		int		rv;
		vm_map_t	map;
		vm_offset_t	offset, v, pa;
		vm_prot_t	oldprot = 0, prot, maxprot;
		vm_inherit_t	inherit;
		boolean_t	shared;
		vm_object_t	object;
		vm_offset_t	objoff;
		vm_page_t	m;
		vm_offset_t	size;
		int s;


		on = uio->uio_offset - trunc_page(uio->uio_offset);
		n = MIN(NBPG-on, uio->uio_resid);

		/* Map page into kernel space */

		/* printf("rw: offset: %d, n: %d, resid: %d\n", 
			uio->uio_offset, n, uio->uio_resid); */
		if (procp->p_vmspace != pfsp->pfs_vs) {
			error = EFAULT;
			break;
		}

		map = &procp->p_vmspace->vm_map;

		offset = trunc_page((vm_offset_t)uio->uio_offset);

/*
 * This code *fakes* the existance of the UPAGES at the address USRSTACK
 * in the process address space for versions of the kernel where the
 * UPAGES do not exist in the process map.  
 */
#if 0
#ifndef FULLSWAP
		if( offset >= USRSTACK) {
			caddr_t paddr;
			if( offset >= USRSTACK + NBPG*UPAGES) {
				error = EINVAL;
				break;
			}
			paddr = (caddr_t) procp->p_addr;
			error = uiomove(paddr + (offset - USRSTACK), (int)n, uio);
			continue;
		}
#endif
#endif

		/* make sure that the offset exists in the procs map */
		rv = vm_region(map, &offset, &size, &prot, &maxprot,
				&inherit, &shared, &object, &objoff);
		if (rv != KERN_SUCCESS) {
			error = EINVAL;
			break;
		}


		/* change protections if need be */
		if (uio->uio_rw == UIO_WRITE && (prot & VM_PROT_WRITE) == 0) {
			oldprot = prot;
			prot |= VM_PROT_WRITE;
			rv = vm_protect(map, offset, NBPG, FALSE, prot);
			if (rv != KERN_SUCCESS) {
				error = EPERM;
				break;
			}
		} 

		if( uio->uio_rw != UIO_WRITE) {
			prot &= ~VM_PROT_WRITE;
		}

		/* check for stack area -- don't fault in unused pages */
		if( (caddr_t) offset >= procp->p_vmspace->vm_maxsaddr &&
			offset < USRSTACK) {
			if( (caddr_t) offset <
				((procp->p_vmspace->vm_maxsaddr + MAXSSIZ) - ctob( procp->p_vmspace->vm_ssize))) {
				error = EFAULT;
				goto reprot;
			}
			if( (caddr_t) offset >=
				((procp->p_vmspace->vm_maxsaddr + MAXSSIZ))) {
				error = EFAULT;
				goto reprot;
			}
		}

		/* wire the page table page */
		v = trunc_page(((vm_offset_t)vtopte( offset)));
		vm_map_pageable(map, v, round_page(v+1), FALSE);

		if( uio->uio_rw == UIO_READ) {
			/* Now just fault the page table and the page */
			rv = vm_fault(map, offset, VM_PROT_READ, FALSE);

			if (rv != KERN_SUCCESS) {
				procp->p_flag = (procp->p_flag & ~SKEEP) | pflag;
				vm_map_pageable(map, v, round_page(v+1), TRUE);
				error = EFAULT;
				goto reprot;
			}


			/* get the physical address of the page */
			pa = pmap_extract( vm_map_pmap(map), offset);

			if( !pa) {
				printf("pfs: cannot get pa -- read\n");
			} else {
				/* enter the physical address into the kernel pmap */
				pmap_enter(vm_map_pmap(kernel_map), kva, pa, prot, TRUE);
				/* copy the data */
				bcopy( (caddr_t)kva, (caddr_t)kbuf, NBPG);
				/* remove the physical address from the kernel pmap */
				pmap_remove(vm_map_pmap(kernel_map), kva, round_page(kva + 1));
			}
		}

		error = uiomove((caddr_t)(kbuf + on), (int)n, uio);

		if( !error && uio->uio_rw == UIO_WRITE) {
			/* Now just fault the page table and the page */
			rv = vm_fault(map, offset, VM_PROT_READ|VM_PROT_WRITE, FALSE);

			if (rv != KERN_SUCCESS) {
				procp->p_flag = (procp->p_flag & ~SKEEP) | pflag;
				vm_map_pageable(map, v, round_page(v+1), TRUE);
				error = EFAULT;
				goto reprot;
			}

			/* get the physical address of the page */
			pa = pmap_extract( vm_map_pmap(map), offset);
			
			if( !pa) {
				printf("pfs: cannot get pa -- write\n");
			} else {
				/* enter the physical address into the kernel pmap */
				pmap_enter(vm_map_pmap(kernel_map), kva, pa, prot, TRUE);
				/* copy the data */
				bcopy( (caddr_t)kbuf, (caddr_t)kva + on, n);
				/* remove the physical address from the kernel pmap */
				pmap_remove(vm_map_pmap(kernel_map), kva, round_page(kva + 1));
			}
		}

		/* unwire the page table page */
		vm_map_pageable(map, v, round_page(v+1), TRUE);

	reprot:

		if (oldprot) {
			rv = vm_protect(map, offset, NBPG, FALSE, oldprot);
			if (rv != KERN_SUCCESS && error == 0) {
				error = EPERM;
				break;
			}
		}

	} while (error == 0 && uio->uio_resid > 0);

	procp->p_flag = (procp->p_flag & ~SKEEP) | pflag;
/* free the kva and bounce buffer */
	kmem_free_wakeup(kernel_map, kva, NBPG);
	kmem_free(kernel_map, kbuf, NBPG);

	return (error);
}
