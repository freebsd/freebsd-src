/*-
 * Copyright (c) 2001 Networks Associates Technologies, Inc.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_mmap.c	8.4 (Berkeley) 1/12/94
 * $FreeBSD$
 * $Id$
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include "kernel_interface.h"
#include "kernel_mediate.h"
#include "kernel_monitor.h"
#include "kernel_util.h"
#include "lomacfs.h"

extern int max_proc_mmap;

int lomac_mmap(struct proc *, struct mmap_args *);

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

int
mmap(td, uap)
	struct thread *td;
	struct mmap_args *uap;
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp = NULL;
	struct vnode *vp, *origvp;
	vm_offset_t addr;
	vm_size_t size, pageoff;
	vm_prot_t prot, maxprot;
	void *handle;
	int flags, error;
	int disablexworkaround;
	off_t pos;
	struct vmspace *vms = p->p_vmspace;
	vm_object_t obj;
	lomac_object_t lobj;

	addr = (vm_offset_t) uap->addr;
	size = uap->len;
	prot = uap->prot & VM_PROT_ALL;
	flags = uap->flags;
	pos = uap->pos;
	origvp = NULL;

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
	 * XXX for non-fixed mappings where no hint is provided or
	 * the hint would fall in the potential heap space,
	 * place it after the end of the largest possible heap.
	 *
	 * There should really be a pmap call to determine a reasonable
	 * location.
	 */
	else if (addr == 0 ||
	    (addr >= round_page((vm_offset_t)vms->vm_taddr) &&
	     addr < round_page((vm_offset_t)vms->vm_daddr + MAXDSIZ)))
		addr = round_page((vm_offset_t)vms->vm_daddr + MAXDSIZ);

	mtx_lock(&Giant);	/* syscall marked mp-safe but isn't */
	if (flags & MAP_ANON) {
		/*
		 * Mapping blank space is trivial.
		 */
		handle = NULL;
		maxprot = VM_PROT_ALL;
		pos = 0;
	} else {
		/*
		 * Mapping file, get fp for validation. Obtain vnode and make
		 * sure it is of appropriate type.
		 */
		if (((unsigned) uap->fd) >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[uap->fd]) == NULL) {
			mtx_unlock(&Giant);
			return (EBADF);
		}
		if (fp->f_type != DTYPE_VNODE) {
			mtx_unlock(&Giant);
			return (EINVAL);
		}

		/*
		 * don't let the descriptor disappear on us if we block
		 */
		fhold(fp);

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
		vp = (struct vnode *) fp->f_data;
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
			origvp = vp;
			vp = (struct vnode*)obj->handle;
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
		} else {
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
			if (securelevel >= 1)
				disablexworkaround = 1;
			else
				disablexworkaround = suser(p);
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
						        p->p_ucred, td))) {
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
			origvp = vp;
		}
	}

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
	if (handle != NULL && VISLOMAC(origvp)) {
		lobj.lo_type = LO_TYPE_LVNODE;
		lobj.lo_object.vnode = origvp;
		if (flags & MAP_SHARED && maxprot & VM_PROT_WRITE &&
		    !mediate_subject_object("mmap", p, &lobj))
			error = EPERM;
		if (error == 0 && maxprot & VM_PROT_READ)
			error = monitor_read_object(p, &lobj);
	}
	if (error == 0)
		error = vm_mmap(&vms->vm_map, &addr, size, prot, maxprot,
		    flags, handle, pos);
	if (error == 0)
		td->td_retval[0] = (register_t) (addr + pageoff);
	mtx_lock(&Giant);
done:
	if (fp)
		fdrop(fp, td);
	mtx_unlock(&Giant);
	return (error);
}

static void
vm_drop_perms_recurse(struct thread *td, struct vm_map *map, lattr_t *lattr) {
	struct vm_map_entry *vme;

 	for (vme = map->header.next; vme != &map->header; vme = vme->next) {
		if (vme->eflags & MAP_ENTRY_IS_SUB_MAP) {
			vm_map_lock_read(vme->object.sub_map);
			vm_drop_perms_recurse(td, vme->object.sub_map,
			    lattr);
			vm_map_unlock_read(vme->object.sub_map);
			continue;
		}
		if ((vme->eflags & (MAP_ENTRY_COW | MAP_ENTRY_NOSYNC)) == 0 &&
		    vme->max_protection & VM_PROT_WRITE) {
			vm_object_t object;
			vm_ooffset_t offset;
			lomac_object_t lobj;
			struct vnode *vp;
			lattr_t olattr;

			offset = vme->offset;
			object = vme->object.vm_object;
			if (object == NULL)
				continue;
			while (object->backing_object) {
				object = object->backing_object;
				offset += object->backing_object_offset;
			}
			/*
			 * Regular objects (swap, etc.) inherit from
			 * their creator.  Vnodes inherit from their
			 * underlying on-disk object.
			 */
			if (object->type == OBJT_DEVICE)
				continue;
			if (object->type == OBJT_VNODE) {
				vp = lobj.lo_object.vnode = object->handle;
				/*
				 * For the foreseeable future, an OBJT_VNODE
				 * is always !VISLOMAC().
				 */
				lobj.lo_type = VISLOMAC(vp) ?
				    LO_TYPE_LVNODE : LO_TYPE_UVNODE;
			} else {
				vp = NULL;
				lobj.lo_object.vm_object = object;
				lobj.lo_type = LO_TYPE_VM_OBJECT;
			}
			get_object_lattr(&lobj, &olattr);
			/*
			 * Revoke write access only to files with a higher
			 * level than the process or which have a possibly-
			 * undeterminable level (interpreted as "lowest").
			 */
			if (lomac_must_deny(lattr, &olattr))
				continue;
			vm_map_lock_upgrade(map);
			/*
			 * If it's a private, non-file-backed mapping and
			 * not mapped anywhere else, we can just take it
			 * down with us.
			 */
			if (vp == NULL && object->flags & OBJ_ONEMAPPING) {
				olattr.level = lattr->level;
				set_object_lattr(&lobj, olattr);
				goto downgrade;
			}
			if ((vme->protection & VM_PROT_WRITE) == 0)
				vme->max_protection &= ~VM_PROT_WRITE;
			else {
				vm_object_reference(object);
				if (vp != NULL)
					vn_lock(vp, LK_EXCLUSIVE | LK_RETRY,
					    td);
				vm_object_page_clean(object,
				    OFF_TO_IDX(offset),
				    OFF_TO_IDX(offset + vme->end - vme->start +
					PAGE_MASK),
				    OBJPC_SYNC);
				if (vp != NULL)
					VOP_UNLOCK(vp, 0, td);
				vm_object_deallocate(object);
				vme->eflags |= MAP_ENTRY_COW |
				    MAP_ENTRY_NEEDS_COPY;
				pmap_protect(map->pmap, vme->start, vme->end,
				    vme->protection & ~VM_PROT_WRITE);
				vm_map_simplify_entry(map, vme);
			}
		downgrade:
			vm_map_lock_downgrade(map);
		}
	}
}

void
kernel_vm_drop_perms(struct thread *td, lattr_t *newlattr) {
	struct vm_map *map = &td->td_proc->p_vmspace->vm_map;

	mtx_lock(&Giant);
	vm_map_lock_read(map);
	vm_drop_perms_recurse(td, map, newlattr);
	vm_map_unlock_read(map);
	mtx_unlock(&Giant);
}

/*
 * Take the level of new vm_objects from the parent subject's level.
 */
static void
vm_object_init_lattr(vm_object_t object) {
	lomac_object_t lobj;
	lattr_t lattr;

	get_subject_lattr(curthread->td_proc, &lattr);
	lattr.flags = 0;
	lobj.lo_type = LO_TYPE_VM_OBJECT;
	lobj.lo_object.vm_object = object;
	set_object_lattr(&lobj, lattr);
}


#define	PGO_ALLOC_REPLACEMENT(n)					\
static vm_object_t (*old_pgo_alloc_##n)(void *, vm_ooffset_t,		\
    vm_prot_t, vm_ooffset_t);						\
									\
static vm_object_t							\
pgo_alloc_##n(void *handle, vm_ooffset_t size, vm_prot_t prot,		\
    vm_ooffset_t off) {							\
	vm_object_t newobj = NULL;					\
									\
	newobj = old_pgo_alloc_##n(handle, size, prot, off);		\
	if (newobj != NULL)						\
		vm_object_init_lattr(newobj);				\
	return (newobj);						\
}

#define	PGO_ALLOC_REPLACE(n)						\
	do {								\
		old_pgo_alloc_##n = pagertab[n]->pgo_alloc;		\
		if (pagertab[n]->pgo_alloc != NULL)			\
			pagertab[n]->pgo_alloc = pgo_alloc_##n;		\
	} while (0)

#define	PGO_ALLOC_UNREPLACE(n)						\
	do {								\
		pagertab[n]->pgo_alloc = old_pgo_alloc_##n;		\
	} while (0)

PGO_ALLOC_REPLACEMENT(0);
PGO_ALLOC_REPLACEMENT(1);
PGO_ALLOC_REPLACEMENT(2);
PGO_ALLOC_REPLACEMENT(3);
PGO_ALLOC_REPLACEMENT(4);
PGO_ALLOC_REPLACEMENT(5);

extern int npagers;

int
lomac_initialize_vm(void) {
	GIANT_REQUIRED;

	if (npagers != 6) {
		printf("LOMAC: number of pagers %d not expected 6!\n", npagers);
		return (EDOM);
	}
	PGO_ALLOC_REPLACE(0);
	PGO_ALLOC_REPLACE(1);
	PGO_ALLOC_REPLACE(2);
	PGO_ALLOC_REPLACE(3);
	PGO_ALLOC_REPLACE(4);
	PGO_ALLOC_REPLACE(5);
	return (0);
}

int
lomac_uninitialize_vm(void) {
	GIANT_REQUIRED;

	PGO_ALLOC_UNREPLACE(0);
	PGO_ALLOC_UNREPLACE(1);
	PGO_ALLOC_UNREPLACE(2);
	PGO_ALLOC_UNREPLACE(3);
	PGO_ALLOC_UNREPLACE(4);
	PGO_ALLOC_UNREPLACE(5);
	return (0);
}
