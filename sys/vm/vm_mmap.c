/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: vm_mmap.c 1.6 91/10/21$
 *
 *	@(#)vm_mmap.c	8.4 (Berkeley) 1/12/94
 * $Id: vm_mmap.c,v 1.55 1996/12/22 23:17:09 joerg Exp $
 */

/*
 * Mapped file (mmap) interface to VM
 */

#include "opt_rlimit.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/conf.h>
#include <sys/vmmeter.h>

#include <miscfs/specfs/specdev.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_inherit.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#ifndef _SYS_SYSPROTO_H_
struct sbrk_args {
	int incr;
};
#endif

/* ARGSUSED */
int
sbrk(p, uap, retval)
	struct proc *p;
	struct sbrk_args *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

#ifndef _SYS_SYSPROTO_H_
struct sstk_args {
	int incr;
};
#endif

/* ARGSUSED */
int
sstk(p, uap, retval)
	struct proc *p;
	struct sstk_args *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#ifndef _SYS_SYSPROTO_H_
struct getpagesize_args {
	int dummy;
};
#endif

/* ARGSUSED */
int
ogetpagesize(p, uap, retval)
	struct proc *p;
	struct getpagesize_args *uap;
	int *retval;
{

	*retval = PAGE_SIZE;
	return (0);
}
#endif				/* COMPAT_43 || COMPAT_SUNOS */

#ifndef _SYS_SYSPROTO_H_
struct mmap_args {
	caddr_t addr;
	size_t len;
	int prot;
	int flags;
	int fd;
	long pad;
	off_t pos;
};
#endif

int
mmap(p, uap, retval)
	struct proc *p;
	register struct mmap_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_prot_t prot, maxprot;
	caddr_t handle;
	int flags, error;

	prot = uap->prot & VM_PROT_ALL;
	flags = uap->flags;
	/*
	 * Address (if FIXED) must be page aligned. Size is implicitly rounded
	 * to a page boundary.
	 */
	addr = (vm_offset_t) uap->addr;
	if (((flags & MAP_FIXED) && (addr & PAGE_MASK)) ||
	    (ssize_t) uap->len < 0 || ((flags & MAP_ANON) && uap->fd != -1))
		return (EINVAL);

	/*
	 * Round page if not already disallowed by above test
	 * XXX: Is there any point in the MAP_FIXED align requirement above?
	 */
	size = uap->len;
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);

	/*
	 * Check for illegal addresses.  Watch out for address wrap... Note
	 * that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (flags & MAP_FIXED) {
		if (VM_MAXUSER_ADDRESS > 0 && addr + size > VM_MAXUSER_ADDRESS)
			return (EINVAL);
#ifndef i386
		if (VM_MIN_ADDRESS > 0 && addr < VM_MIN_ADDRESS)
			return (EINVAL);
#endif
		if (addr + size < addr)
			return (EINVAL);
	}
	/*
	 * XXX if no hint provided for a non-fixed mapping place it after the
	 * end of the largest possible heap.
	 *
	 * There should really be a pmap call to determine a reasonable location.
	 */
	if (addr == 0 && (flags & MAP_FIXED) == 0)
		addr = round_page(p->p_vmspace->vm_daddr + MAXDSIZ);
	if (flags & MAP_ANON) {
		/*
		 * Mapping blank space is trivial.
		 */
		handle = NULL;
		maxprot = VM_PROT_ALL;
	} else {
		/*
		 * Mapping file, get fp for validation. Obtain vnode and make
		 * sure it is of appropriate type.
		 */
		if (((unsigned) uap->fd) >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
			return (EBADF);
		if (fp->f_type != DTYPE_VNODE)
			return (EINVAL);
		vp = (struct vnode *) fp->f_data;
		if (vp->v_type != VREG && vp->v_type != VCHR)
			return (EINVAL);
		/*
		 * XXX hack to handle use of /dev/zero to map anon memory (ala
		 * SunOS).
		 */
		if (vp->v_type == VCHR && iszerodev(vp->v_rdev)) {
			handle = NULL;
			maxprot = VM_PROT_ALL;
			flags |= MAP_ANON;
		} else {
			/*
			 * Ensure that file and memory protections are
			 * compatible.  Note that we only worry about
			 * writability if mapping is shared; in this case,
			 * current and max prot are dictated by the open file.
			 * XXX use the vnode instead?  Problem is: what
			 * credentials do we use for determination? What if
			 * proc does a setuid?
			 */
			maxprot = VM_PROT_EXECUTE;	/* ??? */
			if (fp->f_flag & FREAD)
				maxprot |= VM_PROT_READ;
			else if (prot & PROT_READ)
				return (EACCES);
			if (flags & MAP_SHARED) {
				if (fp->f_flag & FWRITE)
					maxprot |= VM_PROT_WRITE;
				else if (prot & PROT_WRITE)
					return (EACCES);
			} else
				maxprot |= VM_PROT_WRITE;
			handle = (caddr_t) vp;
		}
	}
	error = vm_mmap(&p->p_vmspace->vm_map, &addr, size, prot, maxprot,
	    flags, handle, uap->pos);
	if (error == 0)
		*retval = (int) addr;
	return (error);
}

#ifdef COMPAT_43
#ifndef _SYS_SYSPROTO_H_
struct ommap_args {
	caddr_t addr;
	int len;
	int prot;
	int flags;
	int fd;
	long pos;
};
#endif
int
ommap(p, uap, retval)
	struct proc *p;
	register struct ommap_args *uap;
	int *retval;
{
	struct mmap_args nargs;
	static const char cvtbsdprot[8] = {
		0,
		PROT_EXEC,
		PROT_WRITE,
		PROT_EXEC | PROT_WRITE,
		PROT_READ,
		PROT_EXEC | PROT_READ,
		PROT_WRITE | PROT_READ,
		PROT_EXEC | PROT_WRITE | PROT_READ,
	};

#define	OMAP_ANON	0x0002
#define	OMAP_COPY	0x0020
#define	OMAP_SHARED	0x0010
#define	OMAP_FIXED	0x0100
#define	OMAP_INHERIT	0x0800

	nargs.addr = uap->addr;
	nargs.len = uap->len;
	nargs.prot = cvtbsdprot[uap->prot & 0x7];
	nargs.flags = 0;
	if (uap->flags & OMAP_ANON)
		nargs.flags |= MAP_ANON;
	if (uap->flags & OMAP_COPY)
		nargs.flags |= MAP_COPY;
	if (uap->flags & OMAP_SHARED)
		nargs.flags |= MAP_SHARED;
	else
		nargs.flags |= MAP_PRIVATE;
	if (uap->flags & OMAP_FIXED)
		nargs.flags |= MAP_FIXED;
	if (uap->flags & OMAP_INHERIT)
		nargs.flags |= MAP_INHERIT;
	nargs.fd = uap->fd;
	nargs.pos = uap->pos;
	return (mmap(p, &nargs, retval));
}
#endif				/* COMPAT_43 */


#ifndef _SYS_SYSPROTO_H_
struct msync_args {
	caddr_t addr;
	int len;
	int flags;
};
#endif
int
msync(p, uap, retval)
	struct proc *p;
	struct msync_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	int flags;
	vm_map_t map;
	int rv;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;
	flags = uap->flags;

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if (addr + size < addr)
		return(EINVAL);

	if ((flags & (MS_ASYNC|MS_INVALIDATE)) == (MS_ASYNC|MS_INVALIDATE))
		return (EINVAL);

	map = &p->p_vmspace->vm_map;

	/*
	 * XXX Gak!  If size is zero we are supposed to sync "all modified
	 * pages with the region containing addr".  Unfortunately, we don't
	 * really keep track of individual mmaps so we approximate by flushing
	 * the range of the map entry containing addr. This can be incorrect
	 * if the region splits or is coalesced with a neighbor.
	 */
	if (size == 0) {
		vm_map_entry_t entry;

		vm_map_lock_read(map);
		rv = vm_map_lookup_entry(map, addr, &entry);
		vm_map_unlock_read(map);
		if (rv == FALSE)
			return (EINVAL);
		addr = entry->start;
		size = entry->end - entry->start;
	}

	/*
	 * Clean the pages and interpret the return value.
	 */
	rv = vm_map_clean(map, addr, addr + size, (flags & MS_ASYNC) == 0,
	    (flags & MS_INVALIDATE) != 0);

	switch (rv) {
	case KERN_SUCCESS:
		break;
	case KERN_INVALID_ADDRESS:
		return (EINVAL);	/* Sun returns ENOMEM? */
	case KERN_FAILURE:
		return (EIO);
	default:
		return (EINVAL);
	}

	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct munmap_args {
	caddr_t addr;
	size_t len;
};
#endif
int
munmap(p, uap, retval)
	register struct proc *p;
	register struct munmap_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_map_t map;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if (addr + size < addr)
		return(EINVAL);

	if (size == 0)
		return (0);

	/*
	 * Check for illegal addresses.  Watch out for address wrap... Note
	 * that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (VM_MAXUSER_ADDRESS > 0 && addr + size > VM_MAXUSER_ADDRESS)
		return (EINVAL);
#ifndef i386
	if (VM_MIN_ADDRESS > 0 && addr < VM_MIN_ADDRESS)
		return (EINVAL);
#endif
	if (addr + size < addr)
		return (EINVAL);
	map = &p->p_vmspace->vm_map;
	/*
	 * Make sure entire range is allocated.
	 */
	if (!vm_map_check_protection(map, addr, addr + size, VM_PROT_NONE))
		return (EINVAL);
	/* returns nothing but KERN_SUCCESS anyway */
	(void) vm_map_remove(map, addr, addr + size);
	return (0);
}

void
munmapfd(p, fd)
	struct proc *p;
	int fd;
{
	/*
	 * XXX should unmap any regions mapped to this file
	 */
	p->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
}

#ifndef _SYS_SYSPROTO_H_
struct mprotect_args {
	caddr_t addr;
	size_t len;
	int prot;
};
#endif
int
mprotect(p, uap, retval)
	struct proc *p;
	struct mprotect_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	register vm_prot_t prot;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;
	prot = uap->prot & VM_PROT_ALL;

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if (addr + size < addr)
		return(EINVAL);

	switch (vm_map_protect(&p->p_vmspace->vm_map, addr, addr + size, prot,
		FALSE)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

#ifndef _SYS_SYSPROTO_H_
struct minherit_args {
	caddr_t addr;
	size_t len;
	int inherit;
};
#endif
int
minherit(p, uap, retval)
	struct proc *p;
	struct minherit_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	register vm_inherit_t inherit;

	addr = (vm_offset_t)uap->addr;
	size = uap->len;
	inherit = uap->inherit;

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if (addr + size < addr)
		return(EINVAL);

	switch (vm_map_inherit(&p->p_vmspace->vm_map, addr, addr+size,
	    inherit)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

#ifndef _SYS_SYSPROTO_H_
struct madvise_args {
	caddr_t addr;
	size_t len;
	int behav;
};
#endif

/* ARGSUSED */
int
madvise(p, uap, retval)
	struct proc *p;
	struct madvise_args *uap;
	int *retval;
{
	vm_map_t map;
	pmap_t pmap;
	vm_offset_t start, end;
	/*
	 * Check for illegal addresses.  Watch out for address wrap... Note
	 * that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (VM_MAXUSER_ADDRESS > 0 &&
		((vm_offset_t) uap->addr + uap->len) > VM_MAXUSER_ADDRESS)
		return (EINVAL);
#ifndef i386
	if (VM_MIN_ADDRESS > 0 && uap->addr < VM_MIN_ADDRESS)
		return (EINVAL);
#endif
	if (((vm_offset_t) uap->addr + uap->len) < (vm_offset_t) uap->addr)
		return (EINVAL);

	/*
	 * Since this routine is only advisory, we default to conservative
	 * behavior.
	 */
	start = trunc_page((vm_offset_t) uap->addr);
	end = round_page((vm_offset_t) uap->addr + uap->len);
	
	map = &p->p_vmspace->vm_map;
	pmap = &p->p_vmspace->vm_pmap;

	vm_map_madvise(map, pmap, start, end, uap->behav);

	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct mincore_args {
	caddr_t addr;
	size_t len;
	char *vec;
};
#endif

/* ARGSUSED */
int
mincore(p, uap, retval)
	struct proc *p;
	struct mincore_args *uap;
	int *retval;
{
	vm_offset_t addr, first_addr;
	vm_offset_t end, cend;
	pmap_t pmap;
	vm_map_t map;
	char *vec;
	int error;
	int vecindex, lastvecindex;
	register vm_map_entry_t current;
	vm_map_entry_t entry;
	int mincoreinfo;

	/*
	 * Make sure that the addresses presented are valid for user
	 * mode.
	 */
	first_addr = addr = trunc_page((vm_offset_t) uap->addr);
	end = addr + (vm_size_t)round_page(uap->len);
	if (VM_MAXUSER_ADDRESS > 0 && end > VM_MAXUSER_ADDRESS)
		return (EINVAL);
	if (end < addr)
		return (EINVAL);

	/*
	 * Address of byte vector
	 */
	vec = uap->vec;

	map = &p->p_vmspace->vm_map;
	pmap = &p->p_vmspace->vm_pmap;

	vm_map_lock(map);

	/*
	 * Not needed here
	 */
#if 0
	VM_MAP_RANGE_CHECK(map, addr, end);
#endif

	if (!vm_map_lookup_entry(map, addr, &entry))
		entry = entry->next;

	/*
	 * Do this on a map entry basis so that if the pages are not
	 * in the current processes address space, we can easily look
	 * up the pages elsewhere.
	 */
	lastvecindex = -1;
	for(current = entry;
		(current != &map->header) && (current->start < end);
		current = current->next) {

		/*
		 * ignore submaps (for now) or null objects
		 */
		if (current->is_a_map || current->is_sub_map ||
			current->object.vm_object == NULL)
			continue;
		
		/*
		 * limit this scan to the current map entry and the
		 * limits for the mincore call
		 */
		if (addr < current->start)
			addr = current->start;
		cend = current->end;
		if (cend > end)
			cend = end;

		/*
		 * scan this entry one page at a time
		 */
		while(addr < cend) {
			/*
			 * Check pmap first, it is likely faster, also
			 * it can provide info as to whether we are the
			 * one referencing or modifying the page.
			 */
			mincoreinfo = pmap_mincore(pmap, addr);
			if (!mincoreinfo) {
				vm_pindex_t pindex;
				vm_ooffset_t offset;
				vm_page_t m;
				/*
				 * calculate the page index into the object
				 */
				offset = current->offset + (addr - current->start);
				pindex = OFF_TO_IDX(offset);
				m = vm_page_lookup(current->object.vm_object,
					pindex);
				/*
				 * if the page is resident, then gather information about
				 * it.
				 */
				if (m) {
					mincoreinfo = MINCORE_INCORE;
					if (m->dirty ||
						pmap_is_modified(VM_PAGE_TO_PHYS(m)))
						mincoreinfo |= MINCORE_MODIFIED_OTHER;
					if ((m->flags & PG_REFERENCED) ||
						pmap_is_referenced(VM_PAGE_TO_PHYS(m)))
						mincoreinfo |= MINCORE_REFERENCED_OTHER;
				}
			}

			/*
			 * calculate index into user supplied byte vector
			 */
			vecindex = OFF_TO_IDX(addr - first_addr);

			/*
			 * If we have skipped map entries, we need to make sure that
			 * the byte vector is zeroed for those skipped entries.
			 */
			while((lastvecindex + 1) < vecindex) {
				error = subyte( vec + lastvecindex, 0);
				if (error) {
					vm_map_unlock(map);
					return (EFAULT);
				}
				++lastvecindex;
			}

			/*
			 * Pass the page information to the user
			 */
			error = subyte( vec + vecindex, mincoreinfo);
			if (error) {
				vm_map_unlock(map);
				return (EFAULT);
			}
			lastvecindex = vecindex;
			addr += PAGE_SIZE;
		}
	}

	/*
	 * Zero the last entries in the byte vector.
	 */
	vecindex = OFF_TO_IDX(end - first_addr);
	while((lastvecindex + 1) < vecindex) {
		error = subyte( vec + lastvecindex, 0);
		if (error) {
			vm_map_unlock(map);
			return (EFAULT);
		}
		++lastvecindex;
	}
	
	vm_map_unlock(map);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct mlock_args {
	caddr_t addr;
	size_t len;
};
#endif
int
mlock(p, uap, retval)
	struct proc *p;
	struct mlock_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	int error;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);

	/* disable wrap around */
	if (addr + size < addr)
		return (EINVAL);

	if (atop(size) + cnt.v_wire_count > vm_page_max_wired)
		return (EAGAIN);

#ifdef pmap_wired_count
	if (size + ptoa(pmap_wired_count(vm_map_pmap(&p->p_vmspace->vm_map))) >
	    p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur)
		return (EAGAIN);
#else
	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
#endif

	error = vm_map_user_pageable(&p->p_vmspace->vm_map, addr, addr + size, FALSE);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

#ifndef _SYS_SYSPROTO_H_
struct munlock_args {
	caddr_t addr;
	size_t len;
};
#endif
int
munlock(p, uap, retval)
	struct proc *p;
	struct munlock_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	int error;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);

	/* disable wrap around */
	if (addr + size < addr)
		return (EINVAL);

#ifndef pmap_wired_count
	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
#endif

	error = vm_map_user_pageable(&p->p_vmspace->vm_map, addr, addr + size, TRUE);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

/*
 * Internal version of mmap.
 * Currently used by mmap, exec, and sys5 shared memory.
 * Handle is either a vnode pointer or NULL for MAP_ANON.
 */
int
vm_mmap(map, addr, size, prot, maxprot, flags, handle, foff)
	register vm_map_t map;
	register vm_offset_t *addr;
	register vm_size_t size;
	vm_prot_t prot, maxprot;
	register int flags;
	caddr_t handle;		/* XXX should be vp */
	vm_ooffset_t foff;
{
	boolean_t fitit;
	vm_object_t object;
	struct vnode *vp = NULL;
	objtype_t type;
	int rv = KERN_SUCCESS;
	vm_ooffset_t objsize;
	int docow;
	struct proc *p = curproc;

	if (size == 0)
		return (0);

	objsize = size = round_page(size);

	/*
	 * We currently can only deal with page aligned file offsets.
	 * The check is here rather than in the syscall because the
	 * kernel calls this function internally for other mmaping
	 * operations (such as in exec) and non-aligned offsets will
	 * cause pmap inconsistencies...so we want to be sure to
	 * disallow this in all cases.
	 */
	if (foff & PAGE_MASK)
		return (EINVAL);

	if ((flags & MAP_FIXED) == 0) {
		fitit = TRUE;
		*addr = round_page(*addr);
	} else {
		if (*addr != trunc_page(*addr))
			return (EINVAL);
		fitit = FALSE;
		(void) vm_map_remove(map, *addr, *addr + size);
	}

	/*
	 * Lookup/allocate object.
	 */
	if (flags & MAP_ANON) {
		type = OBJT_DEFAULT;
		/*
		 * Unnamed anonymous regions always start at 0.
		 */
		if (handle == 0)
			foff = 0;
	} else {
		vp = (struct vnode *) handle;
		if (vp->v_type == VCHR) {
			type = OBJT_DEVICE;
			handle = (caddr_t) vp->v_rdev;
		} else {
			struct vattr vat;
			int error;

			error = VOP_GETATTR(vp, &vat, p->p_ucred, p);
			if (error)
				return (error);
			objsize = round_page(vat.va_size);
			type = OBJT_VNODE;
		}
	}

	if (handle == NULL) {
		object = NULL;
	} else {
		object = vm_pager_allocate(type, handle, OFF_TO_IDX(objsize), prot, foff);
		if (object == NULL)
			return (type == OBJT_DEVICE ? EINVAL : ENOMEM);
	}

	/*
	 * Force device mappings to be shared.
	 */
	if (type == OBJT_DEVICE) {
		flags &= ~(MAP_PRIVATE|MAP_COPY);
		flags |= MAP_SHARED;
	}

	docow = 0;
	if ((flags & (MAP_ANON|MAP_SHARED)) == 0) {
		docow = MAP_COPY_ON_WRITE | MAP_COPY_NEEDED;
	}

	rv = vm_map_find(map, object, foff, addr, size, fitit,
			prot, maxprot, docow);


	if (rv != KERN_SUCCESS) {
		/*
		 * Lose the object reference. Will destroy the
		 * object if it's an unnamed anonymous mapping
		 * or named anonymous without other references.
		 */
		vm_object_deallocate(object);
		goto out;
	}

	/*
	 * "Pre-fault" resident pages.
	 */
	if ((type == OBJT_VNODE) && (map->pmap != NULL) && (object != NULL)) {
		pmap_object_init_pt(map->pmap, *addr,
			object, (vm_pindex_t) OFF_TO_IDX(foff), size, 1);
	}

	/*
	 * Shared memory is also shared with children.
	 */
	if (flags & (MAP_SHARED|MAP_INHERIT)) {
		rv = vm_map_inherit(map, *addr, *addr + size, VM_INHERIT_SHARE);
		if (rv != KERN_SUCCESS) {
			(void) vm_map_remove(map, *addr, *addr + size);
			goto out;
		}
	}
out:
	switch (rv) {
	case KERN_SUCCESS:
		return (0);
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return (ENOMEM);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	default:
		return (EINVAL);
	}
}
