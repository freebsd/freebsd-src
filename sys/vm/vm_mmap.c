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
 */

/*
 * Mapped file (mmap) interface to VM
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mac.h>
#include <sys/mman.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#ifndef _SYS_SYSPROTO_H_
struct sbrk_args {
	int incr;
};
#endif

static int max_proc_mmap;
SYSCTL_INT(_vm, OID_AUTO, max_proc_mmap, CTLFLAG_RW, &max_proc_mmap, 0, "");

/*
 * Set the maximum number of vm_map_entry structures per process.  Roughly
 * speaking vm_map_entry structures are tiny, so allowing them to eat 1/100
 * of our KVM malloc space still results in generous limits.  We want a 
 * default that is good enough to prevent the kernel running out of resources
 * if attacked from compromised user account but generous enough such that
 * multi-threaded processes are not unduly inconvenienced.
 */
static void vmmapentry_rsrc_init(void *);
SYSINIT(vmmersrc, SI_SUB_KVM_RSRC, SI_ORDER_FIRST, vmmapentry_rsrc_init, NULL)

static void
vmmapentry_rsrc_init(dummy)
        void *dummy;
{
    max_proc_mmap = vm_kmem_size / sizeof(struct vm_map_entry);
    max_proc_mmap /= 100;
}

/*
 * MPSAFE
 */
/* ARGSUSED */
int
sbrk(td, uap)
	struct thread *td;
	struct sbrk_args *uap;
{
	/* Not yet implemented */
	/* mtx_lock(&Giant); */
	/* mtx_unlock(&Giant); */
	return (EOPNOTSUPP);
}

#ifndef _SYS_SYSPROTO_H_
struct sstk_args {
	int incr;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
sstk(td, uap)
	struct thread *td;
	struct sstk_args *uap;
{
	/* Not yet implemented */
	/* mtx_lock(&Giant); */
	/* mtx_unlock(&Giant); */
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
ogetpagesize(td, uap)
	struct thread *td;
	struct getpagesize_args *uap;
{
	/* MP SAFE */
	td->td_retval[0] = PAGE_SIZE;
	return (0);
}
#endif				/* COMPAT_43 || COMPAT_SUNOS */


/* 
 * Memory Map (mmap) system call.  Note that the file offset
 * and address are allowed to be NOT page aligned, though if
 * the MAP_FIXED flag it set, both must have the same remainder
 * modulo the PAGE_SIZE (POSIX 1003.1b).  If the address is not
 * page-aligned, the actual mapping starts at trunc_page(addr)
 * and the return value is adjusted up by the page offset.
 *
 * Generally speaking, only character devices which are themselves
 * memory-based, such as a video framebuffer, can be mmap'd.  Otherwise
 * there would be no cache coherency between a descriptor and a VM mapping
 * both to the same character device.
 *
 * Block devices can be mmap'd no matter what they represent.  Cache coherency
 * is maintained as long as you do not write directly to the underlying
 * character device.
 */
#ifndef _SYS_SYSPROTO_H_
struct mmap_args {
	void *addr;
	size_t len;
	int prot;
	int flags;
	int fd;
	long pad;
	off_t pos;
};
#endif

/*
 * MPSAFE
 */
int
mmap(td, uap)
	struct thread *td;
	struct mmap_args *uap;
{
	struct file *fp = NULL;
	struct vnode *vp;
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_prot_t prot, maxprot;
	void *handle;
	int flags, error;
	int disablexworkaround;
	off_t pos;
	struct vmspace *vms = td->td_proc->p_vmspace;
	vm_object_t obj;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;
	prot = uap->prot & VM_PROT_ALL;
	flags = uap->flags;
	pos = uap->pos;

	vp = NULL;
	fp = NULL;
	/* make sure mapping fits into numeric range etc */
	if ((ssize_t) uap->len < 0 ||
	    ((flags & MAP_ANON) && uap->fd != -1))
		return (EINVAL);

	if (flags & MAP_STACK) {
		if ((uap->fd != -1) ||
		    ((prot & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE)))
			return (EINVAL);
		flags |= MAP_ANON;
		pos = 0;
	}

	/*
	 * Align the file position to a page boundary,
	 * and save its page offset component.
	 */
	pageoff = (pos & PAGE_MASK);
	pos -= pageoff;

	/* Adjust size for rounding (on both ends). */
	size += pageoff;			/* low end... */
	size = (vm_size_t) round_page(size);	/* hi end */

	/*
	 * Check for illegal addresses.  Watch out for address wrap... Note
	 * that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (flags & MAP_FIXED) {
		/*
		 * The specified address must have the same remainder
		 * as the file offset taken modulo PAGE_SIZE, so it
		 * should be aligned after adjustment by pageoff.
		 */
		addr -= pageoff;
		if (addr & PAGE_MASK)
			return (EINVAL);
		/* Address range must be all in user VM space. */
		if (addr < vm_map_min(&vms->vm_map) ||
		    addr + size > vm_map_max(&vms->vm_map))
			return (EINVAL);
		if (addr + size < addr)
			return (EINVAL);
	}
	/*
	 * XXX for non-fixed mappings where no hint is provided or
	 * the hint would fall in the potential heap space,
	 * place it after the end of the largest possible heap.
	 *
	 * There should really be a pmap call to determine a reasonable
	 * location.
	 */
	else if (addr == 0 ||
	    (addr >= round_page((vm_offset_t)vms->vm_taddr) &&
	     addr < round_page((vm_offset_t)vms->vm_daddr +
	      td->td_proc->p_rlimit[RLIMIT_DATA].rlim_max)))
		addr = round_page((vm_offset_t)vms->vm_daddr +
		    td->td_proc->p_rlimit[RLIMIT_DATA].rlim_max);

	mtx_lock(&Giant);	/* syscall marked mp-safe but isn't */
	do {
		if (flags & MAP_ANON) {
			/*
			 * Mapping blank space is trivial.
			 */
			handle = NULL;
			maxprot = VM_PROT_ALL;
			pos = 0;
			break;
		}
		/*
		 * Mapping file, get fp for validation. Obtain vnode and make
		 * sure it is of appropriate type.
		 * don't let the descriptor disappear on us if we block
		 */
		if ((error = fget(td, uap->fd, &fp)) != 0)
			goto done;
		if (fp->f_type != DTYPE_VNODE) {
			error = EINVAL;
			goto done;
		}

		/*
		 * POSIX shared-memory objects are defined to have
		 * kernel persistence, and are not defined to support
		 * read(2)/write(2) -- or even open(2).  Thus, we can
		 * use MAP_ASYNC to trade on-disk coherence for speed.
		 * The shm_open(3) library routine turns on the FPOSIXSHM
		 * flag to request this behavior.
		 */
		if (fp->f_flag & FPOSIXSHM)
			flags |= MAP_NOSYNC;
		vp = fp->f_vnode;
		error = vget(vp, LK_EXCLUSIVE, td);
		if (error)
			goto done;
		if (vp->v_type != VREG && vp->v_type != VCHR) {
			error = EINVAL;
			goto done;
		}
		if (vp->v_type == VREG) {
			/*
			 * Get the proper underlying object
			 */
			if (VOP_GETVOBJECT(vp, &obj) != 0) {
				error = EINVAL;
				goto done;
			}
			if (obj->handle != vp) {
				vput(vp);
				vp = (struct vnode*)obj->handle;
				vget(vp, LK_EXCLUSIVE, td);
			}
		}
		/*
		 * XXX hack to handle use of /dev/zero to map anon memory (ala
		 * SunOS).
		 */
		if ((vp->v_type == VCHR) && 
		    (vp->v_rdev->si_devsw->d_flags & D_MMAP_ANON)) {
			handle = NULL;
			maxprot = VM_PROT_ALL;
			flags |= MAP_ANON;
			pos = 0;
			break;
		}
		/*
		 * cdevs does not provide private mappings of any kind.
		 */
		/*
		 * However, for XIG X server to continue to work,
		 * we should allow the superuser to do it anyway.
		 * We only allow it at securelevel < 1.
		 * (Because the XIG X server writes directly to video
		 * memory via /dev/mem, it should never work at any
		 * other securelevel.
		 * XXX this will have to go
		 */
		if (securelevel_ge(td->td_ucred, 1))
			disablexworkaround = 1;
		else
			disablexworkaround = suser(td);
		if (vp->v_type == VCHR && disablexworkaround &&
		    (flags & (MAP_PRIVATE|MAP_COPY))) {
			error = EINVAL;
			goto done;
		}
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
		if (fp->f_flag & FREAD) {
			maxprot |= VM_PROT_READ;
		} else if (prot & PROT_READ) {
			error = EACCES;
			goto done;
		}
		/*
		 * If we are sharing potential changes (either via
		 * MAP_SHARED or via the implicit sharing of character
		 * device mappings), and we are trying to get write
		 * permission although we opened it without asking
		 * for it, bail out.  Check for superuser, only if
		 * we're at securelevel < 1, to allow the XIG X server
		 * to continue to work.
		 */
		if ((flags & MAP_SHARED) != 0 ||
		    (vp->v_type == VCHR && disablexworkaround)) {
			if ((fp->f_flag & FWRITE) != 0) {
				struct vattr va;
				if ((error =
				    VOP_GETATTR(vp, &va,
						td->td_ucred, td))) {
					goto done;
				}
				if ((va.va_flags &
				   (SF_SNAPSHOT|IMMUTABLE|APPEND)) == 0) {
					maxprot |= VM_PROT_WRITE;
				} else if (prot & PROT_WRITE) {
					error = EPERM;
					goto done;
				}
			} else if ((prot & PROT_WRITE) != 0) {
				error = EACCES;
				goto done;
			}
		} else {
			maxprot |= VM_PROT_WRITE;
		}

		handle = (void *)vp;
	} while (0);

	/*
	 * Do not allow more then a certain number of vm_map_entry structures
	 * per process.  Scale with the number of rforks sharing the map
	 * to make the limit reasonable for threads.
	 */
	if (max_proc_mmap && 
	    vms->vm_map.nentries >= max_proc_mmap * vms->vm_refcnt) {
		error = ENOMEM;
		goto done;
	}

	mtx_unlock(&Giant);
	error = 0;
#ifdef MAC
	if (handle != NULL && (flags & MAP_SHARED) != 0) {
		error = mac_check_vnode_mmap(td->td_ucred,
		    (struct vnode *)handle, prot);
	}
#endif
	if (error == 0)
		error = vm_mmap(&vms->vm_map, &addr, size, prot, maxprot,
		    flags, handle, pos);
	mtx_lock(&Giant);
	if (error == 0)
		td->td_retval[0] = (register_t) (addr + pageoff);
done:
	if (vp)
		vput(vp);
	mtx_unlock(&Giant);
	if (fp)
		fdrop(fp, td);

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
ommap(td, uap)
	struct thread *td;
	struct ommap_args *uap;
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
	nargs.fd = uap->fd;
	nargs.pos = uap->pos;
	return (mmap(td, &nargs));
}
#endif				/* COMPAT_43 */


#ifndef _SYS_SYSPROTO_H_
struct msync_args {
	void *addr;
	int len;
	int flags;
};
#endif
/*
 * MPSAFE
 */
int
msync(td, uap)
	struct thread *td;
	struct msync_args *uap;
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
		return (EINVAL);

	if ((flags & (MS_ASYNC|MS_INVALIDATE)) == (MS_ASYNC|MS_INVALIDATE))
		return (EINVAL);

	map = &td->td_proc->p_vmspace->vm_map;

	/*
	 * Clean the pages and interpret the return value.
	 */
	rv = vm_map_sync(map, addr, addr + size, (flags & MS_ASYNC) == 0,
	    (flags & MS_INVALIDATE) != 0);
	switch (rv) {
	case KERN_SUCCESS:
		return (0);
	case KERN_INVALID_ADDRESS:
		return (EINVAL);	/* Sun returns ENOMEM? */
	case KERN_FAILURE:
		return (EIO);
	default:
		return (EINVAL);
	}
}

#ifndef _SYS_SYSPROTO_H_
struct munmap_args {
	void *addr;
	size_t len;
};
#endif
/*
 * MPSAFE
 */
int
munmap(td, uap)
	struct thread *td;
	struct munmap_args *uap;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_map_t map;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;
	if (size == 0)
		return (EINVAL);

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if (addr + size < addr)
		return (EINVAL);

	/*
	 * Check for illegal addresses.  Watch out for address wrap...
	 */
	map = &td->td_proc->p_vmspace->vm_map;
	if (addr < vm_map_min(map) || addr + size > vm_map_max(map))
		return (EINVAL);
	vm_map_lock(map);
	/*
	 * Make sure entire range is allocated.
	 */
	if (!vm_map_check_protection(map, addr, addr + size, VM_PROT_NONE)) {
		vm_map_unlock(map);
		return (EINVAL);
	}
	/* returns nothing but KERN_SUCCESS anyway */
	vm_map_delete(map, addr, addr + size);
	vm_map_unlock(map);
	return (0);
}

#if 0
void
munmapfd(td, fd)
	struct thread *td;
	int fd;
{
	/*
	 * XXX should unmap any regions mapped to this file
	 */
	FILEDESC_LOCK(p->p_fd);
	td->td_proc->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
	FILEDESC_UNLOCK(p->p_fd);
}
#endif

#ifndef _SYS_SYSPROTO_H_
struct mprotect_args {
	const void *addr;
	size_t len;
	int prot;
};
#endif
/*
 * MPSAFE
 */
int
mprotect(td, uap)
	struct thread *td;
	struct mprotect_args *uap;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_prot_t prot;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;
	prot = uap->prot & VM_PROT_ALL;
#if defined(VM_PROT_READ_IS_EXEC)
	if (prot & VM_PROT_READ)
		prot |= VM_PROT_EXECUTE;
#endif

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if (addr + size < addr)
		return (EINVAL);

	switch (vm_map_protect(&td->td_proc->p_vmspace->vm_map, addr,
	    addr + size, prot, FALSE)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

#ifndef _SYS_SYSPROTO_H_
struct minherit_args {
	void *addr;
	size_t len;
	int inherit;
};
#endif
/*
 * MPSAFE
 */
int
minherit(td, uap)
	struct thread *td;
	struct minherit_args *uap;
{
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_inherit_t inherit;

	addr = (vm_offset_t)uap->addr;
	size = uap->len;
	inherit = uap->inherit;

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vm_size_t) round_page(size);
	if (addr + size < addr)
		return (EINVAL);

	switch (vm_map_inherit(&td->td_proc->p_vmspace->vm_map, addr,
	    addr + size, inherit)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

#ifndef _SYS_SYSPROTO_H_
struct madvise_args {
	void *addr;
	size_t len;
	int behav;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
madvise(td, uap)
	struct thread *td;
	struct madvise_args *uap;
{
	vm_offset_t start, end;
	vm_map_t map;
	struct proc *p;
	int error;

	/*
	 * Check for our special case, advising the swap pager we are
	 * "immortal."
	 */
	if (uap->behav == MADV_PROTECT) {
		error = suser(td);
		if (error == 0) {
			p = td->td_proc;
			PROC_LOCK(p);
			p->p_flag |= P_PROTECTED;
			PROC_UNLOCK(p);
		}
		return (error);
	}
	/*
	 * Check for illegal behavior
	 */
	if (uap->behav < 0 || uap->behav > MADV_CORE)
		return (EINVAL);
	/*
	 * Check for illegal addresses.  Watch out for address wrap... Note
	 * that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	map = &td->td_proc->p_vmspace->vm_map;
	if ((vm_offset_t)uap->addr < vm_map_min(map) ||
	    (vm_offset_t)uap->addr + uap->len > vm_map_max(map))
		return (EINVAL);
	if (((vm_offset_t) uap->addr + uap->len) < (vm_offset_t) uap->addr)
		return (EINVAL);

	/*
	 * Since this routine is only advisory, we default to conservative
	 * behavior.
	 */
	start = trunc_page((vm_offset_t) uap->addr);
	end = round_page((vm_offset_t) uap->addr + uap->len);
	
	if (vm_map_madvise(map, start, end, uap->behav))
		return (EINVAL);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct mincore_args {
	const void *addr;
	size_t len;
	char *vec;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
mincore(td, uap)
	struct thread *td;
	struct mincore_args *uap;
{
	vm_offset_t addr, first_addr;
	vm_offset_t end, cend;
	pmap_t pmap;
	vm_map_t map;
	char *vec;
	int error = 0;
	int vecindex, lastvecindex;
	vm_map_entry_t current;
	vm_map_entry_t entry;
	int mincoreinfo;
	unsigned int timestamp;

	/*
	 * Make sure that the addresses presented are valid for user
	 * mode.
	 */
	first_addr = addr = trunc_page((vm_offset_t) uap->addr);
	end = addr + (vm_size_t)round_page(uap->len);
	map = &td->td_proc->p_vmspace->vm_map;
	if (end > vm_map_max(map) || end < addr)
		return (EINVAL);

	/*
	 * Address of byte vector
	 */
	vec = uap->vec;

	pmap = vmspace_pmap(td->td_proc->p_vmspace);

	vm_map_lock_read(map);
RestartScan:
	timestamp = map->timestamp;

	if (!vm_map_lookup_entry(map, addr, &entry))
		entry = entry->next;

	/*
	 * Do this on a map entry basis so that if the pages are not
	 * in the current processes address space, we can easily look
	 * up the pages elsewhere.
	 */
	lastvecindex = -1;
	for (current = entry;
	    (current != &map->header) && (current->start < end);
	    current = current->next) {

		/*
		 * ignore submaps (for now) or null objects
		 */
		if ((current->eflags & MAP_ENTRY_IS_SUB_MAP) ||
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
		while (addr < cend) {
			/*
			 * Check pmap first, it is likely faster, also
			 * it can provide info as to whether we are the
			 * one referencing or modifying the page.
			 */
			mtx_lock(&Giant);
			mincoreinfo = pmap_mincore(pmap, addr);
			mtx_unlock(&Giant);
			if (!mincoreinfo) {
				vm_pindex_t pindex;
				vm_ooffset_t offset;
				vm_page_t m;
				/*
				 * calculate the page index into the object
				 */
				offset = current->offset + (addr - current->start);
				pindex = OFF_TO_IDX(offset);
				VM_OBJECT_LOCK(current->object.vm_object);
				m = vm_page_lookup(current->object.vm_object,
					pindex);
				/*
				 * if the page is resident, then gather information about
				 * it.
				 */
				if (m) {
					mincoreinfo = MINCORE_INCORE;
					vm_page_lock_queues();
					if (m->dirty ||
						pmap_is_modified(m))
						mincoreinfo |= MINCORE_MODIFIED_OTHER;
					if ((m->flags & PG_REFERENCED) ||
						pmap_ts_referenced(m)) {
						vm_page_flag_set(m, PG_REFERENCED);
						mincoreinfo |= MINCORE_REFERENCED_OTHER;
					}
					vm_page_unlock_queues();
				}
				VM_OBJECT_UNLOCK(current->object.vm_object);
			}

			/*
			 * subyte may page fault.  In case it needs to modify
			 * the map, we release the lock.
			 */
			vm_map_unlock_read(map);

			/*
			 * calculate index into user supplied byte vector
			 */
			vecindex = OFF_TO_IDX(addr - first_addr);

			/*
			 * If we have skipped map entries, we need to make sure that
			 * the byte vector is zeroed for those skipped entries.
			 */
			while ((lastvecindex + 1) < vecindex) {
				error = subyte(vec + lastvecindex, 0);
				if (error) {
					error = EFAULT;
					goto done2;
				}
				++lastvecindex;
			}

			/*
			 * Pass the page information to the user
			 */
			error = subyte(vec + vecindex, mincoreinfo);
			if (error) {
				error = EFAULT;
				goto done2;
			}

			/*
			 * If the map has changed, due to the subyte, the previous
			 * output may be invalid.
			 */
			vm_map_lock_read(map);
			if (timestamp != map->timestamp)
				goto RestartScan;

			lastvecindex = vecindex;
			addr += PAGE_SIZE;
		}
	}

	/*
	 * subyte may page fault.  In case it needs to modify
	 * the map, we release the lock.
	 */
	vm_map_unlock_read(map);

	/*
	 * Zero the last entries in the byte vector.
	 */
	vecindex = OFF_TO_IDX(end - first_addr);
	while ((lastvecindex + 1) < vecindex) {
		error = subyte(vec + lastvecindex, 0);
		if (error) {
			error = EFAULT;
			goto done2;
		}
		++lastvecindex;
	}
	
	/*
	 * If the map has changed, due to the subyte, the previous
	 * output may be invalid.
	 */
	vm_map_lock_read(map);
	if (timestamp != map->timestamp)
		goto RestartScan;
	vm_map_unlock_read(map);
done2:
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct mlock_args {
	const void *addr;
	size_t len;
};
#endif
/*
 * MPSAFE
 */
int
mlock(td, uap)
	struct thread *td;
	struct mlock_args *uap;
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

#if 0
	if (size + ptoa(pmap_wired_count(vm_map_pmap(&td->td_proc->p_vmspace->vm_map))) >
	    td->td_proc->p_rlimit[RLIMIT_MEMLOCK].rlim_cur)
		return (ENOMEM);
#else
	error = suser(td);
	if (error)
		return (error);
#endif

	error = vm_map_wire(&td->td_proc->p_vmspace->vm_map, addr,
		     addr + size, VM_MAP_WIRE_USER|VM_MAP_WIRE_NOHOLES);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

#ifndef _SYS_SYSPROTO_H_
struct mlockall_args {
	int	how;
};
#endif

/*
 * MPSAFE
 */
int
mlockall(td, uap)
	struct thread *td;
	struct mlockall_args *uap;
{
	vm_map_t map;
	int error;

	map = &td->td_proc->p_vmspace->vm_map;
	error = 0;

	if ((uap->how == 0) || ((uap->how & ~(MCL_CURRENT|MCL_FUTURE)) != 0))
		return (EINVAL);

#if 0
	/*
	 * If wiring all pages in the process would cause it to exceed
	 * a hard resource limit, return ENOMEM.
	 */
	if (map->size - ptoa(pmap_wired_count(vm_map_pmap(map)) >
		td->td_proc->p_rlimit[RLIMIT_MEMLOCK].rlim_cur))
		return (ENOMEM);
#else
	error = suser(td);
	if (error)
		return (error);
#endif

	if (uap->how & MCL_FUTURE) {
		vm_map_lock(map);
		vm_map_modflags(map, MAP_WIREFUTURE, 0);
		vm_map_unlock(map);
		error = 0;
	}

	if (uap->how & MCL_CURRENT) {
		/*
		 * P1003.1-2001 mandates that all currently mapped pages
		 * will be memory resident and locked (wired) upon return
		 * from mlockall(). vm_map_wire() will wire pages, by
		 * calling vm_fault_wire() for each page in the region.
		 */
		error = vm_map_wire(map, vm_map_min(map), vm_map_max(map),
		    VM_MAP_WIRE_USER|VM_MAP_WIRE_HOLESOK);
		error = (error == KERN_SUCCESS ? 0 : EAGAIN);
	}

	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct munlockall_args {
	register_t dummy;
};
#endif

/*
 * MPSAFE
 */
int
munlockall(td, uap)
	struct thread *td;
	struct munlockall_args *uap;
{
	vm_map_t map;
	int error;

	map = &td->td_proc->p_vmspace->vm_map;
	error = suser(td);
	if (error)
		return (error);

	/* Clear the MAP_WIREFUTURE flag from this vm_map. */
	vm_map_lock(map);
	vm_map_modflags(map, 0, MAP_WIREFUTURE);
	vm_map_unlock(map);

	/* Forcibly unwire all pages. */
	error = vm_map_unwire(map, vm_map_min(map), vm_map_max(map),
	    VM_MAP_WIRE_USER|VM_MAP_WIRE_HOLESOK);

	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct munlock_args {
	const void *addr;
	size_t len;
};
#endif
/*
 * MPSAFE
 */
int
munlock(td, uap)
	struct thread *td;
	struct munlock_args *uap;
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

	error = suser(td);
	if (error)
		return (error);

	error = vm_map_unwire(&td->td_proc->p_vmspace->vm_map, addr,
		     addr + size, VM_MAP_WIRE_USER|VM_MAP_WIRE_NOHOLES);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

/*
 * vm_mmap()
 *
 * MPSAFE
 *
 * Internal version of mmap.  Currently used by mmap, exec, and sys5
 * shared memory.  Handle is either a vnode pointer or NULL for MAP_ANON.
 */
int
vm_mmap(vm_map_t map, vm_offset_t *addr, vm_size_t size, vm_prot_t prot,
	vm_prot_t maxprot, int flags,
	void *handle,
	vm_ooffset_t foff)
{
	boolean_t fitit;
	vm_object_t object;
	struct vnode *vp = NULL;
	objtype_t type;
	int rv = KERN_SUCCESS;
	vm_ooffset_t objsize;
	int docow;
	struct thread *td = curthread;

	if (size == 0)
		return (0);

	objsize = size = round_page(size);

	if (td->td_proc->p_vmspace->vm_map.size + size >
	    td->td_proc->p_rlimit[RLIMIT_VMEM].rlim_cur) {
		return(ENOMEM);
	}

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
		mtx_lock(&Giant);
		ASSERT_VOP_LOCKED(vp, "vm_mmap");
		if (vp->v_type == VCHR) {
			type = OBJT_DEVICE;
			handle = vp->v_rdev;
		} else {
			struct vattr vat;
			int error;

			error = VOP_GETATTR(vp, &vat, td->td_ucred, td);
			if (error) {
				mtx_unlock(&Giant);
				return (error);
			}
			objsize = round_page(vat.va_size);
			type = OBJT_VNODE;
			/*
			 * if it is a regular file without any references
			 * we do not need to sync it.
			 */
			if (vp->v_type == VREG && vat.va_nlink == 0) {
				flags |= MAP_NOSYNC;
			}
		}
		mtx_unlock(&Giant);
	}

	if (handle == NULL) {
		object = NULL;
		docow = 0;
	} else {
		object = vm_pager_allocate(type,
			handle, objsize, prot, foff);
		if (object == NULL) {
			return (type == OBJT_DEVICE ? EINVAL : ENOMEM);
		}
		docow = MAP_PREFAULT_PARTIAL;
	}

	/*
	 * Force device mappings to be shared.
	 */
	if (type == OBJT_DEVICE) {
		flags &= ~(MAP_PRIVATE|MAP_COPY);
		flags |= MAP_SHARED;
	}

	if ((flags & (MAP_ANON|MAP_SHARED)) == 0)
		docow |= MAP_COPY_ON_WRITE;
	if (flags & MAP_NOSYNC)
		docow |= MAP_DISABLE_SYNCER;
	if (flags & MAP_NOCORE)
		docow |= MAP_DISABLE_COREDUMP;

#if defined(VM_PROT_READ_IS_EXEC)
	if (prot & VM_PROT_READ)
		prot |= VM_PROT_EXECUTE;

	if (maxprot & VM_PROT_READ)
		maxprot |= VM_PROT_EXECUTE;
#endif

	if (fitit)
		*addr = pmap_addr_hint(object, *addr, size);

	if (flags & MAP_STACK)
		rv = vm_map_stack(map, *addr, size, prot, maxprot,
		    docow | MAP_STACK_GROWS_DOWN);
	else
		rv = vm_map_find(map, object, foff, addr, size, fitit,
				 prot, maxprot, docow);

	if (rv != KERN_SUCCESS) {
		/*
		 * Lose the object reference. Will destroy the
		 * object if it's an unnamed anonymous mapping
		 * or named anonymous without other references.
		 */
		vm_object_deallocate(object);
	} else if (flags & MAP_SHARED) {
		/*
		 * Shared memory is also shared with children.
		 */
		rv = vm_map_inherit(map, *addr, *addr + size, VM_INHERIT_SHARE);
		if (rv != KERN_SUCCESS)
			(void) vm_map_remove(map, *addr, *addr + size);
	}

	/*
	 * If the process has requested that all future mappings
	 * be wired, then heed this.
	 */
	if ((rv == KERN_SUCCESS) && (map->flags & MAP_WIREFUTURE))
		vm_map_wire(map, *addr, *addr + size,
		    VM_MAP_WIRE_USER|VM_MAP_WIRE_NOHOLES);

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
