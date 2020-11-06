/*-
 * SPDX-License-Identifier: (BSD-4-Clause AND MIT-CMU)
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 *
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	from: @(#)vm_fault.c	8.4 (Berkeley) 1/12/94
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	Page fault handling module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/racct.h>
#include <sys/refcount.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <vm/vm_reserv.h>

#define PFBAK 4
#define PFFOR 4

#define	VM_FAULT_READ_DEFAULT	(1 + VM_FAULT_READ_AHEAD_INIT)
#define	VM_FAULT_READ_MAX	(1 + VM_FAULT_READ_AHEAD_MAX)

#define	VM_FAULT_DONTNEED_MIN	1048576

struct faultstate {
	/* Fault parameters. */
	vm_offset_t	vaddr;
	vm_page_t	*m_hold;
	vm_prot_t	fault_type;
	vm_prot_t	prot;
	int		fault_flags;
	int		oom;
	boolean_t	wired;

	/* Page reference for cow. */
	vm_page_t m_cow;

	/* Current object. */
	vm_object_t	object;
	vm_pindex_t	pindex;
	vm_page_t	m;

	/* Top-level map object. */
	vm_object_t	first_object;
	vm_pindex_t	first_pindex;
	vm_page_t	first_m;

	/* Map state. */
	vm_map_t	map;
	vm_map_entry_t	entry;
	int		map_generation;
	bool		lookup_still_valid;

	/* Vnode if locked. */
	struct vnode	*vp;
};

static void vm_fault_dontneed(const struct faultstate *fs, vm_offset_t vaddr,
	    int ahead);
static void vm_fault_prefault(const struct faultstate *fs, vm_offset_t addra,
	    int backward, int forward, bool obj_locked);

static int vm_pfault_oom_attempts = 3;
SYSCTL_INT(_vm, OID_AUTO, pfault_oom_attempts, CTLFLAG_RWTUN,
    &vm_pfault_oom_attempts, 0,
    "Number of page allocation attempts in page fault handler before it "
    "triggers OOM handling");

static int vm_pfault_oom_wait = 10;
SYSCTL_INT(_vm, OID_AUTO, pfault_oom_wait, CTLFLAG_RWTUN,
    &vm_pfault_oom_wait, 0,
    "Number of seconds to wait for free pages before retrying "
    "the page fault handler");

static inline void
fault_page_release(vm_page_t *mp)
{
	vm_page_t m;

	m = *mp;
	if (m != NULL) {
		/*
		 * We are likely to loop around again and attempt to busy
		 * this page.  Deactivating it leaves it available for
		 * pageout while optimizing fault restarts.
		 */
		vm_page_deactivate(m);
		vm_page_xunbusy(m);
		*mp = NULL;
	}
}

static inline void
fault_page_free(vm_page_t *mp)
{
	vm_page_t m;

	m = *mp;
	if (m != NULL) {
		VM_OBJECT_ASSERT_WLOCKED(m->object);
		if (!vm_page_wired(m))
			vm_page_free(m);
		else
			vm_page_xunbusy(m);
		*mp = NULL;
	}
}

static inline void
unlock_map(struct faultstate *fs)
{

	if (fs->lookup_still_valid) {
		vm_map_lookup_done(fs->map, fs->entry);
		fs->lookup_still_valid = false;
	}
}

static void
unlock_vp(struct faultstate *fs)
{

	if (fs->vp != NULL) {
		vput(fs->vp);
		fs->vp = NULL;
	}
}

static void
fault_deallocate(struct faultstate *fs)
{

	fault_page_release(&fs->m_cow);
	fault_page_release(&fs->m);
	vm_object_pip_wakeup(fs->object);
	if (fs->object != fs->first_object) {
		VM_OBJECT_WLOCK(fs->first_object);
		fault_page_free(&fs->first_m);
		VM_OBJECT_WUNLOCK(fs->first_object);
		vm_object_pip_wakeup(fs->first_object);
	}
	vm_object_deallocate(fs->first_object);
	unlock_map(fs);
	unlock_vp(fs);
}

static void
unlock_and_deallocate(struct faultstate *fs)
{

	VM_OBJECT_WUNLOCK(fs->object);
	fault_deallocate(fs);
}

static void
vm_fault_dirty(struct faultstate *fs, vm_page_t m)
{
	bool need_dirty;

	if (((fs->prot & VM_PROT_WRITE) == 0 &&
	    (fs->fault_flags & VM_FAULT_DIRTY) == 0) ||
	    (m->oflags & VPO_UNMANAGED) != 0)
		return;

	VM_PAGE_OBJECT_BUSY_ASSERT(m);

	need_dirty = ((fs->fault_type & VM_PROT_WRITE) != 0 &&
	    (fs->fault_flags & VM_FAULT_WIRE) == 0) ||
	    (fs->fault_flags & VM_FAULT_DIRTY) != 0;

	vm_object_set_writeable_dirty(m->object);

	/*
	 * If the fault is a write, we know that this page is being
	 * written NOW so dirty it explicitly to save on
	 * pmap_is_modified() calls later.
	 *
	 * Also, since the page is now dirty, we can possibly tell
	 * the pager to release any swap backing the page.
	 */
	if (need_dirty && vm_page_set_dirty(m) == 0) {
		/*
		 * If this is a NOSYNC mmap we do not want to set PGA_NOSYNC
		 * if the page is already dirty to prevent data written with
		 * the expectation of being synced from not being synced.
		 * Likewise if this entry does not request NOSYNC then make
		 * sure the page isn't marked NOSYNC.  Applications sharing
		 * data should use the same flags to avoid ping ponging.
		 */
		if ((fs->entry->eflags & MAP_ENTRY_NOSYNC) != 0)
			vm_page_aflag_set(m, PGA_NOSYNC);
		else
			vm_page_aflag_clear(m, PGA_NOSYNC);
	}

}

/*
 * Unlocks fs.first_object and fs.map on success.
 */
static int
vm_fault_soft_fast(struct faultstate *fs)
{
	vm_page_t m, m_map;
#if VM_NRESERVLEVEL > 0
	vm_page_t m_super;
	int flags;
#endif
	int psind, rv;
	vm_offset_t vaddr;

	MPASS(fs->vp == NULL);
	vaddr = fs->vaddr;
	vm_object_busy(fs->first_object);
	m = vm_page_lookup(fs->first_object, fs->first_pindex);
	/* A busy page can be mapped for read|execute access. */
	if (m == NULL || ((fs->prot & VM_PROT_WRITE) != 0 &&
	    vm_page_busied(m)) || !vm_page_all_valid(m)) {
		rv = KERN_FAILURE;
		goto out;
	}
	m_map = m;
	psind = 0;
#if VM_NRESERVLEVEL > 0
	if ((m->flags & PG_FICTITIOUS) == 0 &&
	    (m_super = vm_reserv_to_superpage(m)) != NULL &&
	    rounddown2(vaddr, pagesizes[m_super->psind]) >= fs->entry->start &&
	    roundup2(vaddr + 1, pagesizes[m_super->psind]) <= fs->entry->end &&
	    (vaddr & (pagesizes[m_super->psind] - 1)) == (VM_PAGE_TO_PHYS(m) &
	    (pagesizes[m_super->psind] - 1)) && !fs->wired &&
	    pmap_ps_enabled(fs->map->pmap)) {
		flags = PS_ALL_VALID;
		if ((fs->prot & VM_PROT_WRITE) != 0) {
			/*
			 * Create a superpage mapping allowing write access
			 * only if none of the constituent pages are busy and
			 * all of them are already dirty (except possibly for
			 * the page that was faulted on).
			 */
			flags |= PS_NONE_BUSY;
			if ((fs->first_object->flags & OBJ_UNMANAGED) == 0)
				flags |= PS_ALL_DIRTY;
		}
		if (vm_page_ps_test(m_super, flags, m)) {
			m_map = m_super;
			psind = m_super->psind;
			vaddr = rounddown2(vaddr, pagesizes[psind]);
			/* Preset the modified bit for dirty superpages. */
			if ((flags & PS_ALL_DIRTY) != 0)
				fs->fault_type |= VM_PROT_WRITE;
		}
	}
#endif
	rv = pmap_enter(fs->map->pmap, vaddr, m_map, fs->prot, fs->fault_type |
	    PMAP_ENTER_NOSLEEP | (fs->wired ? PMAP_ENTER_WIRED : 0), psind);
	if (rv != KERN_SUCCESS)
		goto out;
	if (fs->m_hold != NULL) {
		(*fs->m_hold) = m;
		vm_page_wire(m);
	}
	if (psind == 0 && !fs->wired)
		vm_fault_prefault(fs, vaddr, PFBAK, PFFOR, true);
	VM_OBJECT_RUNLOCK(fs->first_object);
	vm_fault_dirty(fs, m);
	vm_map_lookup_done(fs->map, fs->entry);
	curthread->td_ru.ru_minflt++;

out:
	vm_object_unbusy(fs->first_object);
	return (rv);
}

static void
vm_fault_restore_map_lock(struct faultstate *fs)
{

	VM_OBJECT_ASSERT_WLOCKED(fs->first_object);
	MPASS(blockcount_read(&fs->first_object->paging_in_progress) > 0);

	if (!vm_map_trylock_read(fs->map)) {
		VM_OBJECT_WUNLOCK(fs->first_object);
		vm_map_lock_read(fs->map);
		VM_OBJECT_WLOCK(fs->first_object);
	}
	fs->lookup_still_valid = true;
}

static void
vm_fault_populate_check_page(vm_page_t m)
{

	/*
	 * Check each page to ensure that the pager is obeying the
	 * interface: the page must be installed in the object, fully
	 * valid, and exclusively busied.
	 */
	MPASS(m != NULL);
	MPASS(vm_page_all_valid(m));
	MPASS(vm_page_xbusied(m));
}

static void
vm_fault_populate_cleanup(vm_object_t object, vm_pindex_t first,
    vm_pindex_t last)
{
	vm_page_t m;
	vm_pindex_t pidx;

	VM_OBJECT_ASSERT_WLOCKED(object);
	MPASS(first <= last);
	for (pidx = first, m = vm_page_lookup(object, pidx);
	    pidx <= last; pidx++, m = vm_page_next(m)) {
		vm_fault_populate_check_page(m);
		vm_page_deactivate(m);
		vm_page_xunbusy(m);
	}
}

static int
vm_fault_populate(struct faultstate *fs)
{
	vm_offset_t vaddr;
	vm_page_t m;
	vm_pindex_t map_first, map_last, pager_first, pager_last, pidx;
	int bdry_idx, i, npages, psind, rv;

	MPASS(fs->object == fs->first_object);
	VM_OBJECT_ASSERT_WLOCKED(fs->first_object);
	MPASS(blockcount_read(&fs->first_object->paging_in_progress) > 0);
	MPASS(fs->first_object->backing_object == NULL);
	MPASS(fs->lookup_still_valid);

	pager_first = OFF_TO_IDX(fs->entry->offset);
	pager_last = pager_first + atop(fs->entry->end - fs->entry->start) - 1;
	unlock_map(fs);
	unlock_vp(fs);

	/*
	 * Call the pager (driver) populate() method.
	 *
	 * There is no guarantee that the method will be called again
	 * if the current fault is for read, and a future fault is
	 * for write.  Report the entry's maximum allowed protection
	 * to the driver.
	 */
	rv = vm_pager_populate(fs->first_object, fs->first_pindex,
	    fs->fault_type, fs->entry->max_protection, &pager_first,
	    &pager_last);

	VM_OBJECT_ASSERT_WLOCKED(fs->first_object);
	if (rv == VM_PAGER_BAD) {
		/*
		 * VM_PAGER_BAD is the backdoor for a pager to request
		 * normal fault handling.
		 */
		vm_fault_restore_map_lock(fs);
		if (fs->map->timestamp != fs->map_generation)
			return (KERN_RESTART);
		return (KERN_NOT_RECEIVER);
	}
	if (rv != VM_PAGER_OK)
		return (KERN_FAILURE); /* AKA SIGSEGV */

	/* Ensure that the driver is obeying the interface. */
	MPASS(pager_first <= pager_last);
	MPASS(fs->first_pindex <= pager_last);
	MPASS(fs->first_pindex >= pager_first);
	MPASS(pager_last < fs->first_object->size);

	vm_fault_restore_map_lock(fs);
	bdry_idx = (fs->entry->eflags & MAP_ENTRY_SPLIT_BOUNDARY_MASK) >>
	    MAP_ENTRY_SPLIT_BOUNDARY_SHIFT;
	if (fs->map->timestamp != fs->map_generation) {
		if (bdry_idx == 0) {
			vm_fault_populate_cleanup(fs->first_object, pager_first,
			    pager_last);
		} else {
			m = vm_page_lookup(fs->first_object, pager_first);
			if (m != fs->m)
				vm_page_xunbusy(m);
		}
		return (KERN_RESTART);
	}

	/*
	 * The map is unchanged after our last unlock.  Process the fault.
	 *
	 * First, the special case of largepage mappings, where
	 * populate only busies the first page in superpage run.
	 */
	if (bdry_idx != 0) {
		KASSERT(PMAP_HAS_LARGEPAGES,
		    ("missing pmap support for large pages"));
		m = vm_page_lookup(fs->first_object, pager_first);
		vm_fault_populate_check_page(m);
		VM_OBJECT_WUNLOCK(fs->first_object);
		vaddr = fs->entry->start + IDX_TO_OFF(pager_first) -
		    fs->entry->offset;
		/* assert alignment for entry */
		KASSERT((vaddr & (pagesizes[bdry_idx] - 1)) == 0,
    ("unaligned superpage start %#jx pager_first %#jx offset %#jx vaddr %#jx",
		    (uintmax_t)fs->entry->start, (uintmax_t)pager_first,
		    (uintmax_t)fs->entry->offset, (uintmax_t)vaddr));
		KASSERT((VM_PAGE_TO_PHYS(m) & (pagesizes[bdry_idx] - 1)) == 0,
		    ("unaligned superpage m %p %#jx", m,
		    (uintmax_t)VM_PAGE_TO_PHYS(m)));
		rv = pmap_enter(fs->map->pmap, vaddr, m, fs->prot,
		    fs->fault_type | (fs->wired ? PMAP_ENTER_WIRED : 0) |
		    PMAP_ENTER_LARGEPAGE, bdry_idx);
		VM_OBJECT_WLOCK(fs->first_object);
		vm_page_xunbusy(m);
		if ((fs->fault_flags & VM_FAULT_WIRE) != 0) {
			for (i = 0; i < atop(pagesizes[bdry_idx]); i++)
				vm_page_wire(m + i);
		}
		if (fs->m_hold != NULL) {
			*fs->m_hold = m + (fs->first_pindex - pager_first);
			vm_page_wire(*fs->m_hold);
		}
		goto out;
	}

	/*
	 * The range [pager_first, pager_last] that is given to the
	 * pager is only a hint.  The pager may populate any range
	 * within the object that includes the requested page index.
	 * In case the pager expanded the range, clip it to fit into
	 * the map entry.
	 */
	map_first = OFF_TO_IDX(fs->entry->offset);
	if (map_first > pager_first) {
		vm_fault_populate_cleanup(fs->first_object, pager_first,
		    map_first - 1);
		pager_first = map_first;
	}
	map_last = map_first + atop(fs->entry->end - fs->entry->start) - 1;
	if (map_last < pager_last) {
		vm_fault_populate_cleanup(fs->first_object, map_last + 1,
		    pager_last);
		pager_last = map_last;
	}
	for (pidx = pager_first, m = vm_page_lookup(fs->first_object, pidx);
	    pidx <= pager_last;
	    pidx += npages, m = vm_page_next(&m[npages - 1])) {
		vaddr = fs->entry->start + IDX_TO_OFF(pidx) - fs->entry->offset;
#if defined(__aarch64__) || defined(__amd64__) || (defined(__arm__) && \
    __ARM_ARCH >= 6) || defined(__i386__) || defined(__riscv) || \
    defined(__powerpc64__)
		psind = m->psind;
		if (psind > 0 && ((vaddr & (pagesizes[psind] - 1)) != 0 ||
		    pidx + OFF_TO_IDX(pagesizes[psind]) - 1 > pager_last ||
		    !pmap_ps_enabled(fs->map->pmap) || fs->wired))
			psind = 0;
#else
		psind = 0;
#endif		
		npages = atop(pagesizes[psind]);
		for (i = 0; i < npages; i++) {
			vm_fault_populate_check_page(&m[i]);
			vm_fault_dirty(fs, &m[i]);
		}
		VM_OBJECT_WUNLOCK(fs->first_object);
		rv = pmap_enter(fs->map->pmap, vaddr, m, fs->prot, fs->fault_type |
		    (fs->wired ? PMAP_ENTER_WIRED : 0), psind);
#if defined(__amd64__)
		if (psind > 0 && rv == KERN_FAILURE) {
			for (i = 0; i < npages; i++) {
				rv = pmap_enter(fs->map->pmap, vaddr + ptoa(i),
				    &m[i], fs->prot, fs->fault_type |
				    (fs->wired ? PMAP_ENTER_WIRED : 0), 0);
				MPASS(rv == KERN_SUCCESS);
			}
		}
#else
		MPASS(rv == KERN_SUCCESS);
#endif
		VM_OBJECT_WLOCK(fs->first_object);
		for (i = 0; i < npages; i++) {
			if ((fs->fault_flags & VM_FAULT_WIRE) != 0)
				vm_page_wire(&m[i]);
			else
				vm_page_activate(&m[i]);
			if (fs->m_hold != NULL && m[i].pindex == fs->first_pindex) {
				(*fs->m_hold) = &m[i];
				vm_page_wire(&m[i]);
			}
			vm_page_xunbusy(&m[i]);
		}
	}
out:
	curthread->td_ru.ru_majflt++;
	return (KERN_SUCCESS);
}

static int prot_fault_translation;
SYSCTL_INT(_machdep, OID_AUTO, prot_fault_translation, CTLFLAG_RWTUN,
    &prot_fault_translation, 0,
    "Control signal to deliver on protection fault");

/* compat definition to keep common code for signal translation */
#define	UCODE_PAGEFLT	12
#ifdef T_PAGEFLT
_Static_assert(UCODE_PAGEFLT == T_PAGEFLT, "T_PAGEFLT");
#endif

/*
 *	vm_fault_trap:
 *
 *	Handle a page fault occurring at the given address,
 *	requiring the given permissions, in the map specified.
 *	If successful, the page is inserted into the
 *	associated physical map.
 *
 *	NOTE: the given address should be truncated to the
 *	proper page address.
 *
 *	KERN_SUCCESS is returned if the page fault is handled; otherwise,
 *	a standard error specifying why the fault is fatal is returned.
 *
 *	The map in question must be referenced, and remains so.
 *	Caller may hold no locks.
 */
int
vm_fault_trap(vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
    int fault_flags, int *signo, int *ucode)
{
	int result;

	MPASS(signo == NULL || ucode != NULL);
#ifdef KTRACE
	if (map != kernel_map && KTRPOINT(curthread, KTR_FAULT))
		ktrfault(vaddr, fault_type);
#endif
	result = vm_fault(map, trunc_page(vaddr), fault_type, fault_flags,
	    NULL);
	KASSERT(result == KERN_SUCCESS || result == KERN_FAILURE ||
	    result == KERN_INVALID_ADDRESS ||
	    result == KERN_RESOURCE_SHORTAGE ||
	    result == KERN_PROTECTION_FAILURE ||
	    result == KERN_OUT_OF_BOUNDS,
	    ("Unexpected Mach error %d from vm_fault()", result));
#ifdef KTRACE
	if (map != kernel_map && KTRPOINT(curthread, KTR_FAULTEND))
		ktrfaultend(result);
#endif
	if (result != KERN_SUCCESS && signo != NULL) {
		switch (result) {
		case KERN_FAILURE:
		case KERN_INVALID_ADDRESS:
			*signo = SIGSEGV;
			*ucode = SEGV_MAPERR;
			break;
		case KERN_RESOURCE_SHORTAGE:
			*signo = SIGBUS;
			*ucode = BUS_OOMERR;
			break;
		case KERN_OUT_OF_BOUNDS:
			*signo = SIGBUS;
			*ucode = BUS_OBJERR;
			break;
		case KERN_PROTECTION_FAILURE:
			if (prot_fault_translation == 0) {
				/*
				 * Autodetect.  This check also covers
				 * the images without the ABI-tag ELF
				 * note.
				 */
				if (SV_CURPROC_ABI() == SV_ABI_FREEBSD &&
				    curproc->p_osrel >= P_OSREL_SIGSEGV) {
					*signo = SIGSEGV;
					*ucode = SEGV_ACCERR;
				} else {
					*signo = SIGBUS;
					*ucode = UCODE_PAGEFLT;
				}
			} else if (prot_fault_translation == 1) {
				/* Always compat mode. */
				*signo = SIGBUS;
				*ucode = UCODE_PAGEFLT;
			} else {
				/* Always SIGSEGV mode. */
				*signo = SIGSEGV;
				*ucode = SEGV_ACCERR;
			}
			break;
		default:
			KASSERT(0, ("Unexpected Mach error %d from vm_fault()",
			    result));
			break;
		}
	}
	return (result);
}

static int
vm_fault_lock_vnode(struct faultstate *fs, bool objlocked)
{
	struct vnode *vp;
	int error, locked;

	if (fs->object->type != OBJT_VNODE)
		return (KERN_SUCCESS);
	vp = fs->object->handle;
	if (vp == fs->vp) {
		ASSERT_VOP_LOCKED(vp, "saved vnode is not locked");
		return (KERN_SUCCESS);
	}

	/*
	 * Perform an unlock in case the desired vnode changed while
	 * the map was unlocked during a retry.
	 */
	unlock_vp(fs);

	locked = VOP_ISLOCKED(vp);
	if (locked != LK_EXCLUSIVE)
		locked = LK_SHARED;

	/*
	 * We must not sleep acquiring the vnode lock while we have
	 * the page exclusive busied or the object's
	 * paging-in-progress count incremented.  Otherwise, we could
	 * deadlock.
	 */
	error = vget(vp, locked | LK_CANRECURSE | LK_NOWAIT);
	if (error == 0) {
		fs->vp = vp;
		return (KERN_SUCCESS);
	}

	vhold(vp);
	if (objlocked)
		unlock_and_deallocate(fs);
	else
		fault_deallocate(fs);
	error = vget(vp, locked | LK_RETRY | LK_CANRECURSE);
	vdrop(vp);
	fs->vp = vp;
	KASSERT(error == 0, ("vm_fault: vget failed %d", error));
	return (KERN_RESOURCE_SHORTAGE);
}

/*
 * Calculate the desired readahead.  Handle drop-behind.
 *
 * Returns the number of readahead blocks to pass to the pager.
 */
static int
vm_fault_readahead(struct faultstate *fs)
{
	int era, nera;
	u_char behavior;

	KASSERT(fs->lookup_still_valid, ("map unlocked"));
	era = fs->entry->read_ahead;
	behavior = vm_map_entry_behavior(fs->entry);
	if (behavior == MAP_ENTRY_BEHAV_RANDOM) {
		nera = 0;
	} else if (behavior == MAP_ENTRY_BEHAV_SEQUENTIAL) {
		nera = VM_FAULT_READ_AHEAD_MAX;
		if (fs->vaddr == fs->entry->next_read)
			vm_fault_dontneed(fs, fs->vaddr, nera);
	} else if (fs->vaddr == fs->entry->next_read) {
		/*
		 * This is a sequential fault.  Arithmetically
		 * increase the requested number of pages in
		 * the read-ahead window.  The requested
		 * number of pages is "# of sequential faults
		 * x (read ahead min + 1) + read ahead min"
		 */
		nera = VM_FAULT_READ_AHEAD_MIN;
		if (era > 0) {
			nera += era + 1;
			if (nera > VM_FAULT_READ_AHEAD_MAX)
				nera = VM_FAULT_READ_AHEAD_MAX;
		}
		if (era == VM_FAULT_READ_AHEAD_MAX)
			vm_fault_dontneed(fs, fs->vaddr, nera);
	} else {
		/*
		 * This is a non-sequential fault.
		 */
		nera = 0;
	}
	if (era != nera) {
		/*
		 * A read lock on the map suffices to update
		 * the read ahead count safely.
		 */
		fs->entry->read_ahead = nera;
	}

	return (nera);
}

static int
vm_fault_lookup(struct faultstate *fs)
{
	int result;

	KASSERT(!fs->lookup_still_valid,
	   ("vm_fault_lookup: Map already locked."));
	result = vm_map_lookup(&fs->map, fs->vaddr, fs->fault_type |
	    VM_PROT_FAULT_LOOKUP, &fs->entry, &fs->first_object,
	    &fs->first_pindex, &fs->prot, &fs->wired);
	if (result != KERN_SUCCESS) {
		unlock_vp(fs);
		return (result);
	}

	fs->map_generation = fs->map->timestamp;

	if (fs->entry->eflags & MAP_ENTRY_NOFAULT) {
		panic("%s: fault on nofault entry, addr: %#lx",
		    __func__, (u_long)fs->vaddr);
	}

	if (fs->entry->eflags & MAP_ENTRY_IN_TRANSITION &&
	    fs->entry->wiring_thread != curthread) {
		vm_map_unlock_read(fs->map);
		vm_map_lock(fs->map);
		if (vm_map_lookup_entry(fs->map, fs->vaddr, &fs->entry) &&
		    (fs->entry->eflags & MAP_ENTRY_IN_TRANSITION)) {
			unlock_vp(fs);
			fs->entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			vm_map_unlock_and_wait(fs->map, 0);
		} else
			vm_map_unlock(fs->map);
		return (KERN_RESOURCE_SHORTAGE);
	}

	MPASS((fs->entry->eflags & MAP_ENTRY_GUARD) == 0);

	if (fs->wired)
		fs->fault_type = fs->prot | (fs->fault_type & VM_PROT_COPY);
	else
		KASSERT((fs->fault_flags & VM_FAULT_WIRE) == 0,
		    ("!fs->wired && VM_FAULT_WIRE"));
	fs->lookup_still_valid = true;

	return (KERN_SUCCESS);
}

static int
vm_fault_relookup(struct faultstate *fs)
{
	vm_object_t retry_object;
	vm_pindex_t retry_pindex;
	vm_prot_t retry_prot;
	int result;

	if (!vm_map_trylock_read(fs->map))
		return (KERN_RESTART);

	fs->lookup_still_valid = true;
	if (fs->map->timestamp == fs->map_generation)
		return (KERN_SUCCESS);

	result = vm_map_lookup_locked(&fs->map, fs->vaddr, fs->fault_type,
	    &fs->entry, &retry_object, &retry_pindex, &retry_prot,
	    &fs->wired);
	if (result != KERN_SUCCESS) {
		/*
		 * If retry of map lookup would have blocked then
		 * retry fault from start.
		 */
		if (result == KERN_FAILURE)
			return (KERN_RESTART);
		return (result);
	}
	if (retry_object != fs->first_object ||
	    retry_pindex != fs->first_pindex)
		return (KERN_RESTART);

	/*
	 * Check whether the protection has changed or the object has
	 * been copied while we left the map unlocked. Changing from
	 * read to write permission is OK - we leave the page
	 * write-protected, and catch the write fault. Changing from
	 * write to read permission means that we can't mark the page
	 * write-enabled after all.
	 */
	fs->prot &= retry_prot;
	fs->fault_type &= retry_prot;
	if (fs->prot == 0)
		return (KERN_RESTART);

	/* Reassert because wired may have changed. */
	KASSERT(fs->wired || (fs->fault_flags & VM_FAULT_WIRE) == 0,
	    ("!wired && VM_FAULT_WIRE"));

	return (KERN_SUCCESS);
}

static void
vm_fault_cow(struct faultstate *fs)
{
	bool is_first_object_locked;

	/*
	 * This allows pages to be virtually copied from a backing_object
	 * into the first_object, where the backing object has no other
	 * refs to it, and cannot gain any more refs.  Instead of a bcopy,
	 * we just move the page from the backing object to the first
	 * object.  Note that we must mark the page dirty in the first
	 * object so that it will go out to swap when needed.
	 */
	is_first_object_locked = false;
	if (
	    /*
	     * Only one shadow object and no other refs.
	     */
	    fs->object->shadow_count == 1 && fs->object->ref_count == 1 &&
	    /*
	     * No other ways to look the object up
	     */
	    fs->object->handle == NULL && (fs->object->flags & OBJ_ANON) != 0 &&
	    /*
	     * We don't chase down the shadow chain and we can acquire locks.
	     */
	    (is_first_object_locked = VM_OBJECT_TRYWLOCK(fs->first_object)) &&
	    fs->object == fs->first_object->backing_object &&
	    VM_OBJECT_TRYWLOCK(fs->object)) {
		/*
		 * Remove but keep xbusy for replace.  fs->m is moved into
		 * fs->first_object and left busy while fs->first_m is
		 * conditionally freed.
		 */
		vm_page_remove_xbusy(fs->m);
		vm_page_replace(fs->m, fs->first_object, fs->first_pindex,
		    fs->first_m);
		vm_page_dirty(fs->m);
#if VM_NRESERVLEVEL > 0
		/*
		 * Rename the reservation.
		 */
		vm_reserv_rename(fs->m, fs->first_object, fs->object,
		    OFF_TO_IDX(fs->first_object->backing_object_offset));
#endif
		VM_OBJECT_WUNLOCK(fs->object);
		VM_OBJECT_WUNLOCK(fs->first_object);
		fs->first_m = fs->m;
		fs->m = NULL;
		VM_CNT_INC(v_cow_optim);
	} else {
		if (is_first_object_locked)
			VM_OBJECT_WUNLOCK(fs->first_object);
		/*
		 * Oh, well, lets copy it.
		 */
		pmap_copy_page(fs->m, fs->first_m);
		vm_page_valid(fs->first_m);
		if (fs->wired && (fs->fault_flags & VM_FAULT_WIRE) == 0) {
			vm_page_wire(fs->first_m);
			vm_page_unwire(fs->m, PQ_INACTIVE);
		}
		/*
		 * Save the cow page to be released after
		 * pmap_enter is complete.
		 */
		fs->m_cow = fs->m;
		fs->m = NULL;
	}
	/*
	 * fs->object != fs->first_object due to above 
	 * conditional
	 */
	vm_object_pip_wakeup(fs->object);

	/*
	 * Only use the new page below...
	 */
	fs->object = fs->first_object;
	fs->pindex = fs->first_pindex;
	fs->m = fs->first_m;
	VM_CNT_INC(v_cow_faults);
	curthread->td_cow++;
}

static bool
vm_fault_next(struct faultstate *fs)
{
	vm_object_t next_object;

	/*
	 * The requested page does not exist at this object/
	 * offset.  Remove the invalid page from the object,
	 * waking up anyone waiting for it, and continue on to
	 * the next object.  However, if this is the top-level
	 * object, we must leave the busy page in place to
	 * prevent another process from rushing past us, and
	 * inserting the page in that object at the same time
	 * that we are.
	 */
	if (fs->object == fs->first_object) {
		fs->first_m = fs->m;
		fs->m = NULL;
	} else
		fault_page_free(&fs->m);

	/*
	 * Move on to the next object.  Lock the next object before
	 * unlocking the current one.
	 */
	VM_OBJECT_ASSERT_WLOCKED(fs->object);
	next_object = fs->object->backing_object;
	if (next_object == NULL)
		return (false);
	MPASS(fs->first_m != NULL);
	KASSERT(fs->object != next_object, ("object loop %p", next_object));
	VM_OBJECT_WLOCK(next_object);
	vm_object_pip_add(next_object, 1);
	if (fs->object != fs->first_object)
		vm_object_pip_wakeup(fs->object);
	fs->pindex += OFF_TO_IDX(fs->object->backing_object_offset);
	VM_OBJECT_WUNLOCK(fs->object);
	fs->object = next_object;

	return (true);
}

static void
vm_fault_zerofill(struct faultstate *fs)
{

	/*
	 * If there's no object left, fill the page in the top
	 * object with zeros.
	 */
	if (fs->object != fs->first_object) {
		vm_object_pip_wakeup(fs->object);
		fs->object = fs->first_object;
		fs->pindex = fs->first_pindex;
	}
	MPASS(fs->first_m != NULL);
	MPASS(fs->m == NULL);
	fs->m = fs->first_m;
	fs->first_m = NULL;

	/*
	 * Zero the page if necessary and mark it valid.
	 */
	if ((fs->m->flags & PG_ZERO) == 0) {
		pmap_zero_page(fs->m);
	} else {
		VM_CNT_INC(v_ozfod);
	}
	VM_CNT_INC(v_zfod);
	vm_page_valid(fs->m);
}

/*
 * Allocate a page directly or via the object populate method.
 */
static int
vm_fault_allocate(struct faultstate *fs)
{
	struct domainset *dset;
	int alloc_req;
	int rv;

	if ((fs->object->flags & OBJ_SIZEVNLOCK) != 0) {
		rv = vm_fault_lock_vnode(fs, true);
		MPASS(rv == KERN_SUCCESS || rv == KERN_RESOURCE_SHORTAGE);
		if (rv == KERN_RESOURCE_SHORTAGE)
			return (rv);
	}

	if (fs->pindex >= fs->object->size)
		return (KERN_OUT_OF_BOUNDS);

	if (fs->object == fs->first_object &&
	    (fs->first_object->flags & OBJ_POPULATE) != 0 &&
	    fs->first_object->shadow_count == 0) {
		rv = vm_fault_populate(fs);
		switch (rv) {
		case KERN_SUCCESS:
		case KERN_FAILURE:
		case KERN_RESTART:
			return (rv);
		case KERN_NOT_RECEIVER:
			/*
			 * Pager's populate() method
			 * returned VM_PAGER_BAD.
			 */
			break;
		default:
			panic("inconsistent return codes");
		}
	}

	/*
	 * Allocate a new page for this object/offset pair.
	 *
	 * Unlocked read of the p_flag is harmless. At worst, the P_KILLED
	 * might be not observed there, and allocation can fail, causing
	 * restart and new reading of the p_flag.
	 */
	dset = fs->object->domain.dr_policy;
	if (dset == NULL)
		dset = curthread->td_domain.dr_policy;
	if (!vm_page_count_severe_set(&dset->ds_mask) || P_KILLED(curproc)) {
#if VM_NRESERVLEVEL > 0
		vm_object_color(fs->object, atop(fs->vaddr) - fs->pindex);
#endif
		alloc_req = P_KILLED(curproc) ?
		    VM_ALLOC_SYSTEM : VM_ALLOC_NORMAL;
		if (fs->object->type != OBJT_VNODE &&
		    fs->object->backing_object == NULL)
			alloc_req |= VM_ALLOC_ZERO;
		fs->m = vm_page_alloc(fs->object, fs->pindex, alloc_req);
	}
	if (fs->m == NULL) {
		unlock_and_deallocate(fs);
		if (vm_pfault_oom_attempts < 0 ||
		    fs->oom < vm_pfault_oom_attempts) {
			fs->oom++;
			vm_waitpfault(dset, vm_pfault_oom_wait * hz);
		} else 	{
			if (bootverbose)
				printf(
		"proc %d (%s) failed to alloc page on fault, starting OOM\n",
				    curproc->p_pid, curproc->p_comm);
			vm_pageout_oom(VM_OOM_MEM_PF);
			fs->oom = 0;
		}
		return (KERN_RESOURCE_SHORTAGE);
	}
	fs->oom = 0;

	return (KERN_NOT_RECEIVER);
}

/*
 * Call the pager to retrieve the page if there is a chance
 * that the pager has it, and potentially retrieve additional
 * pages at the same time.
 */
static int
vm_fault_getpages(struct faultstate *fs, int nera, int *behindp, int *aheadp)
{
	vm_offset_t e_end, e_start;
	int ahead, behind, cluster_offset, rv;
	u_char behavior;

	/*
	 * Prepare for unlocking the map.  Save the map
	 * entry's start and end addresses, which are used to
	 * optimize the size of the pager operation below.
	 * Even if the map entry's addresses change after
	 * unlocking the map, using the saved addresses is
	 * safe.
	 */
	e_start = fs->entry->start;
	e_end = fs->entry->end;
	behavior = vm_map_entry_behavior(fs->entry);

	/*
	 * Release the map lock before locking the vnode or
	 * sleeping in the pager.  (If the current object has
	 * a shadow, then an earlier iteration of this loop
	 * may have already unlocked the map.)
	 */
	unlock_map(fs);

	rv = vm_fault_lock_vnode(fs, false);
	MPASS(rv == KERN_SUCCESS || rv == KERN_RESOURCE_SHORTAGE);
	if (rv == KERN_RESOURCE_SHORTAGE)
		return (rv);
	KASSERT(fs->vp == NULL || !fs->map->system_map,
	    ("vm_fault: vnode-backed object mapped by system map"));

	/*
	 * Page in the requested page and hint the pager,
	 * that it may bring up surrounding pages.
	 */
	if (nera == -1 || behavior == MAP_ENTRY_BEHAV_RANDOM ||
	    P_KILLED(curproc)) {
		behind = 0;
		ahead = 0;
	} else {
		/* Is this a sequential fault? */
		if (nera > 0) {
			behind = 0;
			ahead = nera;
		} else {
			/*
			 * Request a cluster of pages that is
			 * aligned to a VM_FAULT_READ_DEFAULT
			 * page offset boundary within the
			 * object.  Alignment to a page offset
			 * boundary is more likely to coincide
			 * with the underlying file system
			 * block than alignment to a virtual
			 * address boundary.
			 */
			cluster_offset = fs->pindex % VM_FAULT_READ_DEFAULT;
			behind = ulmin(cluster_offset,
			    atop(fs->vaddr - e_start));
			ahead = VM_FAULT_READ_DEFAULT - 1 - cluster_offset;
		}
		ahead = ulmin(ahead, atop(e_end - fs->vaddr) - 1);
	}
	*behindp = behind;
	*aheadp = ahead;
	rv = vm_pager_get_pages(fs->object, &fs->m, 1, behindp, aheadp);
	if (rv == VM_PAGER_OK)
		return (KERN_SUCCESS);
	if (rv == VM_PAGER_ERROR)
		printf("vm_fault: pager read error, pid %d (%s)\n",
		    curproc->p_pid, curproc->p_comm);
	/*
	 * If an I/O error occurred or the requested page was
	 * outside the range of the pager, clean up and return
	 * an error.
	 */
	if (rv == VM_PAGER_ERROR || rv == VM_PAGER_BAD)
		return (KERN_OUT_OF_BOUNDS);
	return (KERN_NOT_RECEIVER);
}

/*
 * Wait/Retry if the page is busy.  We have to do this if the page is
 * either exclusive or shared busy because the vm_pager may be using
 * read busy for pageouts (and even pageins if it is the vnode pager),
 * and we could end up trying to pagein and pageout the same page
 * simultaneously.
 *
 * We can theoretically allow the busy case on a read fault if the page
 * is marked valid, but since such pages are typically already pmap'd,
 * putting that special case in might be more effort then it is worth.
 * We cannot under any circumstances mess around with a shared busied
 * page except, perhaps, to pmap it.
 */
static void
vm_fault_busy_sleep(struct faultstate *fs)
{
	/*
	 * Reference the page before unlocking and
	 * sleeping so that the page daemon is less
	 * likely to reclaim it.
	 */
	vm_page_aflag_set(fs->m, PGA_REFERENCED);
	if (fs->object != fs->first_object) {
		fault_page_release(&fs->first_m);
		vm_object_pip_wakeup(fs->first_object);
	}
	vm_object_pip_wakeup(fs->object);
	unlock_map(fs);
	if (fs->m == vm_page_lookup(fs->object, fs->pindex))
		vm_page_busy_sleep(fs->m, "vmpfw", false);
	else
		VM_OBJECT_WUNLOCK(fs->object);
	VM_CNT_INC(v_intrans);
	vm_object_deallocate(fs->first_object);
}

int
vm_fault(vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
    int fault_flags, vm_page_t *m_hold)
{
	struct faultstate fs;
	int ahead, behind, faultcount;
	int nera, result, rv;
	bool dead, hardfault;

	VM_CNT_INC(v_vm_faults);

	if ((curthread->td_pflags & TDP_NOFAULTING) != 0)
		return (KERN_PROTECTION_FAILURE);

	fs.vp = NULL;
	fs.vaddr = vaddr;
	fs.m_hold = m_hold;
	fs.fault_flags = fault_flags;
	fs.map = map;
	fs.lookup_still_valid = false;
	fs.oom = 0;
	faultcount = 0;
	nera = -1;
	hardfault = false;

RetryFault:
	fs.fault_type = fault_type;

	/*
	 * Find the backing store object and offset into it to begin the
	 * search.
	 */
	result = vm_fault_lookup(&fs);
	if (result != KERN_SUCCESS) {
		if (result == KERN_RESOURCE_SHORTAGE)
			goto RetryFault;
		return (result);
	}

	/*
	 * Try to avoid lock contention on the top-level object through
	 * special-case handling of some types of page faults, specifically,
	 * those that are mapping an existing page from the top-level object.
	 * Under this condition, a read lock on the object suffices, allowing
	 * multiple page faults of a similar type to run in parallel.
	 */
	if (fs.vp == NULL /* avoid locked vnode leak */ &&
	    (fs.entry->eflags & MAP_ENTRY_SPLIT_BOUNDARY_MASK) == 0 &&
	    (fs.fault_flags & (VM_FAULT_WIRE | VM_FAULT_DIRTY)) == 0) {
		VM_OBJECT_RLOCK(fs.first_object);
		rv = vm_fault_soft_fast(&fs);
		if (rv == KERN_SUCCESS)
			return (rv);
		if (!VM_OBJECT_TRYUPGRADE(fs.first_object)) {
			VM_OBJECT_RUNLOCK(fs.first_object);
			VM_OBJECT_WLOCK(fs.first_object);
		}
	} else {
		VM_OBJECT_WLOCK(fs.first_object);
	}

	/*
	 * Make a reference to this object to prevent its disposal while we
	 * are messing with it.  Once we have the reference, the map is free
	 * to be diddled.  Since objects reference their shadows (and copies),
	 * they will stay around as well.
	 *
	 * Bump the paging-in-progress count to prevent size changes (e.g. 
	 * truncation operations) during I/O.
	 */
	vm_object_reference_locked(fs.first_object);
	vm_object_pip_add(fs.first_object, 1);

	fs.m_cow = fs.m = fs.first_m = NULL;

	/*
	 * Search for the page at object/offset.
	 */
	fs.object = fs.first_object;
	fs.pindex = fs.first_pindex;

	if ((fs.entry->eflags & MAP_ENTRY_SPLIT_BOUNDARY_MASK) != 0) {
		rv = vm_fault_allocate(&fs);
		switch (rv) {
		case KERN_RESTART:
			unlock_and_deallocate(&fs);
			/* FALLTHROUGH */
		case KERN_RESOURCE_SHORTAGE:
			goto RetryFault;
		case KERN_SUCCESS:
		case KERN_FAILURE:
		case KERN_OUT_OF_BOUNDS:
			unlock_and_deallocate(&fs);
			return (rv);
		case KERN_NOT_RECEIVER:
			break;
		default:
			panic("vm_fault: Unhandled rv %d", rv);
		}
	}

	while (TRUE) {
		KASSERT(fs.m == NULL,
		    ("page still set %p at loop start", fs.m));
		/*
		 * If the object is marked for imminent termination,
		 * we retry here, since the collapse pass has raced
		 * with us.  Otherwise, if we see terminally dead
		 * object, return fail.
		 */
		if ((fs.object->flags & OBJ_DEAD) != 0) {
			dead = fs.object->type == OBJT_DEAD;
			unlock_and_deallocate(&fs);
			if (dead)
				return (KERN_PROTECTION_FAILURE);
			pause("vmf_de", 1);
			goto RetryFault;
		}

		/*
		 * See if page is resident
		 */
		fs.m = vm_page_lookup(fs.object, fs.pindex);
		if (fs.m != NULL) {
			if (vm_page_tryxbusy(fs.m) == 0) {
				vm_fault_busy_sleep(&fs);
				goto RetryFault;
			}

			/*
			 * The page is marked busy for other processes and the
			 * pagedaemon.  If it still is completely valid we
			 * are done.
			 */
			if (vm_page_all_valid(fs.m)) {
				VM_OBJECT_WUNLOCK(fs.object);
				break; /* break to PAGE HAS BEEN FOUND. */
			}
		}
		VM_OBJECT_ASSERT_WLOCKED(fs.object);

		/*
		 * Page is not resident.  If the pager might contain the page
		 * or this is the beginning of the search, allocate a new
		 * page.  (Default objects are zero-fill, so there is no real
		 * pager for them.)
		 */
		if (fs.m == NULL && (fs.object->type != OBJT_DEFAULT ||
		    fs.object == fs.first_object)) {
			rv = vm_fault_allocate(&fs);
			switch (rv) {
			case KERN_RESTART:
				unlock_and_deallocate(&fs);
				/* FALLTHROUGH */
			case KERN_RESOURCE_SHORTAGE:
				goto RetryFault;
			case KERN_SUCCESS:
			case KERN_FAILURE:
			case KERN_OUT_OF_BOUNDS:
				unlock_and_deallocate(&fs);
				return (rv);
			case KERN_NOT_RECEIVER:
				break;
			default:
				panic("vm_fault: Unhandled rv %d", rv);
			}
		}

		/*
		 * Default objects have no pager so no exclusive busy exists
		 * to protect this page in the chain.  Skip to the next
		 * object without dropping the lock to preserve atomicity of
		 * shadow faults.
		 */
		if (fs.object->type != OBJT_DEFAULT) {
			/*
			 * At this point, we have either allocated a new page
			 * or found an existing page that is only partially
			 * valid.
			 *
			 * We hold a reference on the current object and the
			 * page is exclusive busied.  The exclusive busy
			 * prevents simultaneous faults and collapses while
			 * the object lock is dropped.
		 	 */
			VM_OBJECT_WUNLOCK(fs.object);

			/*
			 * If the pager for the current object might have
			 * the page, then determine the number of additional
			 * pages to read and potentially reprioritize
			 * previously read pages for earlier reclamation.
			 * These operations should only be performed once per
			 * page fault.  Even if the current pager doesn't
			 * have the page, the number of additional pages to
			 * read will apply to subsequent objects in the
			 * shadow chain.
			 */
			if (nera == -1 && !P_KILLED(curproc))
				nera = vm_fault_readahead(&fs);

			rv = vm_fault_getpages(&fs, nera, &behind, &ahead);
			if (rv == KERN_SUCCESS) {
				faultcount = behind + 1 + ahead;
				hardfault = true;
				break; /* break to PAGE HAS BEEN FOUND. */
			}
			if (rv == KERN_RESOURCE_SHORTAGE)
				goto RetryFault;
			VM_OBJECT_WLOCK(fs.object);
			if (rv == KERN_OUT_OF_BOUNDS) {
				fault_page_free(&fs.m);
				unlock_and_deallocate(&fs);
				return (rv);
			}
		}

		/*
		 * The page was not found in the current object.  Try to
		 * traverse into a backing object or zero fill if none is
		 * found.
		 */
		if (vm_fault_next(&fs))
			continue;
		if ((fs.fault_flags & VM_FAULT_NOFILL) != 0) {
			if (fs.first_object == fs.object)
				fault_page_free(&fs.first_m);
			unlock_and_deallocate(&fs);
			return (KERN_OUT_OF_BOUNDS);
		}
		VM_OBJECT_WUNLOCK(fs.object);
		vm_fault_zerofill(&fs);
		/* Don't try to prefault neighboring pages. */
		faultcount = 1;
		break;	/* break to PAGE HAS BEEN FOUND. */
	}

	/*
	 * PAGE HAS BEEN FOUND.  A valid page has been found and exclusively
	 * busied.  The object lock must no longer be held.
	 */
	vm_page_assert_xbusied(fs.m);
	VM_OBJECT_ASSERT_UNLOCKED(fs.object);

	/*
	 * If the page is being written, but isn't already owned by the
	 * top-level object, we have to copy it into a new page owned by the
	 * top-level object.
	 */
	if (fs.object != fs.first_object) {
		/*
		 * We only really need to copy if we want to write it.
		 */
		if ((fs.fault_type & (VM_PROT_COPY | VM_PROT_WRITE)) != 0) {
			vm_fault_cow(&fs);
			/*
			 * We only try to prefault read-only mappings to the
			 * neighboring pages when this copy-on-write fault is
			 * a hard fault.  In other cases, trying to prefault
			 * is typically wasted effort.
			 */
			if (faultcount == 0)
				faultcount = 1;

		} else {
			fs.prot &= ~VM_PROT_WRITE;
		}
	}

	/*
	 * We must verify that the maps have not changed since our last
	 * lookup.
	 */
	if (!fs.lookup_still_valid) {
		result = vm_fault_relookup(&fs);
		if (result != KERN_SUCCESS) {
			fault_deallocate(&fs);
			if (result == KERN_RESTART)
				goto RetryFault;
			return (result);
		}
	}
	VM_OBJECT_ASSERT_UNLOCKED(fs.object);

	/*
	 * If the page was filled by a pager, save the virtual address that
	 * should be faulted on next under a sequential access pattern to the
	 * map entry.  A read lock on the map suffices to update this address
	 * safely.
	 */
	if (hardfault)
		fs.entry->next_read = vaddr + ptoa(ahead) + PAGE_SIZE;

	/*
	 * Page must be completely valid or it is not fit to
	 * map into user space.  vm_pager_get_pages() ensures this.
	 */
	vm_page_assert_xbusied(fs.m);
	KASSERT(vm_page_all_valid(fs.m),
	    ("vm_fault: page %p partially invalid", fs.m));

	vm_fault_dirty(&fs, fs.m);

	/*
	 * Put this page into the physical map.  We had to do the unlock above
	 * because pmap_enter() may sleep.  We don't put the page
	 * back on the active queue until later so that the pageout daemon
	 * won't find it (yet).
	 */
	pmap_enter(fs.map->pmap, vaddr, fs.m, fs.prot,
	    fs.fault_type | (fs.wired ? PMAP_ENTER_WIRED : 0), 0);
	if (faultcount != 1 && (fs.fault_flags & VM_FAULT_WIRE) == 0 &&
	    fs.wired == 0)
		vm_fault_prefault(&fs, vaddr,
		    faultcount > 0 ? behind : PFBAK,
		    faultcount > 0 ? ahead : PFFOR, false);

	/*
	 * If the page is not wired down, then put it where the pageout daemon
	 * can find it.
	 */
	if ((fs.fault_flags & VM_FAULT_WIRE) != 0)
		vm_page_wire(fs.m);
	else
		vm_page_activate(fs.m);
	if (fs.m_hold != NULL) {
		(*fs.m_hold) = fs.m;
		vm_page_wire(fs.m);
	}
	vm_page_xunbusy(fs.m);
	fs.m = NULL;

	/*
	 * Unlock everything, and return
	 */
	fault_deallocate(&fs);
	if (hardfault) {
		VM_CNT_INC(v_io_faults);
		curthread->td_ru.ru_majflt++;
#ifdef RACCT
		if (racct_enable && fs.object->type == OBJT_VNODE) {
			PROC_LOCK(curproc);
			if ((fs.fault_type & (VM_PROT_COPY | VM_PROT_WRITE)) != 0) {
				racct_add_force(curproc, RACCT_WRITEBPS,
				    PAGE_SIZE + behind * PAGE_SIZE);
				racct_add_force(curproc, RACCT_WRITEIOPS, 1);
			} else {
				racct_add_force(curproc, RACCT_READBPS,
				    PAGE_SIZE + ahead * PAGE_SIZE);
				racct_add_force(curproc, RACCT_READIOPS, 1);
			}
			PROC_UNLOCK(curproc);
		}
#endif
	} else 
		curthread->td_ru.ru_minflt++;

	return (KERN_SUCCESS);
}

/*
 * Speed up the reclamation of pages that precede the faulting pindex within
 * the first object of the shadow chain.  Essentially, perform the equivalent
 * to madvise(..., MADV_DONTNEED) on a large cluster of pages that precedes
 * the faulting pindex by the cluster size when the pages read by vm_fault()
 * cross a cluster-size boundary.  The cluster size is the greater of the
 * smallest superpage size and VM_FAULT_DONTNEED_MIN.
 *
 * When "fs->first_object" is a shadow object, the pages in the backing object
 * that precede the faulting pindex are deactivated by vm_fault().  So, this
 * function must only be concerned with pages in the first object.
 */
static void
vm_fault_dontneed(const struct faultstate *fs, vm_offset_t vaddr, int ahead)
{
	vm_map_entry_t entry;
	vm_object_t first_object, object;
	vm_offset_t end, start;
	vm_page_t m, m_next;
	vm_pindex_t pend, pstart;
	vm_size_t size;

	object = fs->object;
	VM_OBJECT_ASSERT_UNLOCKED(object);
	first_object = fs->first_object;
	/* Neither fictitious nor unmanaged pages can be reclaimed. */
	if ((first_object->flags & (OBJ_FICTITIOUS | OBJ_UNMANAGED)) == 0) {
		VM_OBJECT_RLOCK(first_object);
		size = VM_FAULT_DONTNEED_MIN;
		if (MAXPAGESIZES > 1 && size < pagesizes[1])
			size = pagesizes[1];
		end = rounddown2(vaddr, size);
		if (vaddr - end >= size - PAGE_SIZE - ptoa(ahead) &&
		    (entry = fs->entry)->start < end) {
			if (end - entry->start < size)
				start = entry->start;
			else
				start = end - size;
			pmap_advise(fs->map->pmap, start, end, MADV_DONTNEED);
			pstart = OFF_TO_IDX(entry->offset) + atop(start -
			    entry->start);
			m_next = vm_page_find_least(first_object, pstart);
			pend = OFF_TO_IDX(entry->offset) + atop(end -
			    entry->start);
			while ((m = m_next) != NULL && m->pindex < pend) {
				m_next = TAILQ_NEXT(m, listq);
				if (!vm_page_all_valid(m) ||
				    vm_page_busied(m))
					continue;

				/*
				 * Don't clear PGA_REFERENCED, since it would
				 * likely represent a reference by a different
				 * process.
				 *
				 * Typically, at this point, prefetched pages
				 * are still in the inactive queue.  Only
				 * pages that triggered page faults are in the
				 * active queue.  The test for whether the page
				 * is in the inactive queue is racy; in the
				 * worst case we will requeue the page
				 * unnecessarily.
				 */
				if (!vm_page_inactive(m))
					vm_page_deactivate(m);
			}
		}
		VM_OBJECT_RUNLOCK(first_object);
	}
}

/*
 * vm_fault_prefault provides a quick way of clustering
 * pagefaults into a processes address space.  It is a "cousin"
 * of vm_map_pmap_enter, except it runs at page fault time instead
 * of mmap time.
 */
static void
vm_fault_prefault(const struct faultstate *fs, vm_offset_t addra,
    int backward, int forward, bool obj_locked)
{
	pmap_t pmap;
	vm_map_entry_t entry;
	vm_object_t backing_object, lobject;
	vm_offset_t addr, starta;
	vm_pindex_t pindex;
	vm_page_t m;
	int i;

	pmap = fs->map->pmap;
	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace))
		return;

	entry = fs->entry;

	if (addra < backward * PAGE_SIZE) {
		starta = entry->start;
	} else {
		starta = addra - backward * PAGE_SIZE;
		if (starta < entry->start)
			starta = entry->start;
	}

	/*
	 * Generate the sequence of virtual addresses that are candidates for
	 * prefaulting in an outward spiral from the faulting virtual address,
	 * "addra".  Specifically, the sequence is "addra - PAGE_SIZE", "addra
	 * + PAGE_SIZE", "addra - 2 * PAGE_SIZE", "addra + 2 * PAGE_SIZE", ...
	 * If the candidate address doesn't have a backing physical page, then
	 * the loop immediately terminates.
	 */
	for (i = 0; i < 2 * imax(backward, forward); i++) {
		addr = addra + ((i >> 1) + 1) * ((i & 1) == 0 ? -PAGE_SIZE :
		    PAGE_SIZE);
		if (addr > addra + forward * PAGE_SIZE)
			addr = 0;

		if (addr < starta || addr >= entry->end)
			continue;

		if (!pmap_is_prefaultable(pmap, addr))
			continue;

		pindex = ((addr - entry->start) + entry->offset) >> PAGE_SHIFT;
		lobject = entry->object.vm_object;
		if (!obj_locked)
			VM_OBJECT_RLOCK(lobject);
		while ((m = vm_page_lookup(lobject, pindex)) == NULL &&
		    lobject->type == OBJT_DEFAULT &&
		    (backing_object = lobject->backing_object) != NULL) {
			KASSERT((lobject->backing_object_offset & PAGE_MASK) ==
			    0, ("vm_fault_prefault: unaligned object offset"));
			pindex += lobject->backing_object_offset >> PAGE_SHIFT;
			VM_OBJECT_RLOCK(backing_object);
			if (!obj_locked || lobject != entry->object.vm_object)
				VM_OBJECT_RUNLOCK(lobject);
			lobject = backing_object;
		}
		if (m == NULL) {
			if (!obj_locked || lobject != entry->object.vm_object)
				VM_OBJECT_RUNLOCK(lobject);
			break;
		}
		if (vm_page_all_valid(m) &&
		    (m->flags & PG_FICTITIOUS) == 0)
			pmap_enter_quick(pmap, addr, m, entry->protection);
		if (!obj_locked || lobject != entry->object.vm_object)
			VM_OBJECT_RUNLOCK(lobject);
	}
}

/*
 * Hold each of the physical pages that are mapped by the specified range of
 * virtual addresses, ["addr", "addr" + "len"), if those mappings are valid
 * and allow the specified types of access, "prot".  If all of the implied
 * pages are successfully held, then the number of held pages is returned
 * together with pointers to those pages in the array "ma".  However, if any
 * of the pages cannot be held, -1 is returned.
 */
int
vm_fault_quick_hold_pages(vm_map_t map, vm_offset_t addr, vm_size_t len,
    vm_prot_t prot, vm_page_t *ma, int max_count)
{
	vm_offset_t end, va;
	vm_page_t *mp;
	int count;
	boolean_t pmap_failed;

	if (len == 0)
		return (0);
	end = round_page(addr + len);
	addr = trunc_page(addr);

	if (!vm_map_range_valid(map, addr, end))
		return (-1);

	if (atop(end - addr) > max_count)
		panic("vm_fault_quick_hold_pages: count > max_count");
	count = atop(end - addr);

	/*
	 * Most likely, the physical pages are resident in the pmap, so it is
	 * faster to try pmap_extract_and_hold() first.
	 */
	pmap_failed = FALSE;
	for (mp = ma, va = addr; va < end; mp++, va += PAGE_SIZE) {
		*mp = pmap_extract_and_hold(map->pmap, va, prot);
		if (*mp == NULL)
			pmap_failed = TRUE;
		else if ((prot & VM_PROT_WRITE) != 0 &&
		    (*mp)->dirty != VM_PAGE_BITS_ALL) {
			/*
			 * Explicitly dirty the physical page.  Otherwise, the
			 * caller's changes may go unnoticed because they are
			 * performed through an unmanaged mapping or by a DMA
			 * operation.
			 *
			 * The object lock is not held here.
			 * See vm_page_clear_dirty_mask().
			 */
			vm_page_dirty(*mp);
		}
	}
	if (pmap_failed) {
		/*
		 * One or more pages could not be held by the pmap.  Either no
		 * page was mapped at the specified virtual address or that
		 * mapping had insufficient permissions.  Attempt to fault in
		 * and hold these pages.
		 *
		 * If vm_fault_disable_pagefaults() was called,
		 * i.e., TDP_NOFAULTING is set, we must not sleep nor
		 * acquire MD VM locks, which means we must not call
		 * vm_fault().  Some (out of tree) callers mark
		 * too wide a code area with vm_fault_disable_pagefaults()
		 * already, use the VM_PROT_QUICK_NOFAULT flag to request
		 * the proper behaviour explicitly.
		 */
		if ((prot & VM_PROT_QUICK_NOFAULT) != 0 &&
		    (curthread->td_pflags & TDP_NOFAULTING) != 0)
			goto error;
		for (mp = ma, va = addr; va < end; mp++, va += PAGE_SIZE)
			if (*mp == NULL && vm_fault(map, va, prot,
			    VM_FAULT_NORMAL, mp) != KERN_SUCCESS)
				goto error;
	}
	return (count);
error:	
	for (mp = ma; mp < ma + count; mp++)
		if (*mp != NULL)
			vm_page_unwire(*mp, PQ_INACTIVE);
	return (-1);
}

/*
 *	Routine:
 *		vm_fault_copy_entry
 *	Function:
 *		Create new shadow object backing dst_entry with private copy of
 *		all underlying pages. When src_entry is equal to dst_entry,
 *		function implements COW for wired-down map entry. Otherwise,
 *		it forks wired entry into dst_map.
 *
 *	In/out conditions:
 *		The source and destination maps must be locked for write.
 *		The source map entry must be wired down (or be a sharing map
 *		entry corresponding to a main map entry that is wired down).
 */
void
vm_fault_copy_entry(vm_map_t dst_map, vm_map_t src_map,
    vm_map_entry_t dst_entry, vm_map_entry_t src_entry,
    vm_ooffset_t *fork_charge)
{
	vm_object_t backing_object, dst_object, object, src_object;
	vm_pindex_t dst_pindex, pindex, src_pindex;
	vm_prot_t access, prot;
	vm_offset_t vaddr;
	vm_page_t dst_m;
	vm_page_t src_m;
	boolean_t upgrade;

#ifdef	lint
	src_map++;
#endif	/* lint */

	upgrade = src_entry == dst_entry;
	access = prot = dst_entry->protection;

	src_object = src_entry->object.vm_object;
	src_pindex = OFF_TO_IDX(src_entry->offset);

	if (upgrade && (dst_entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0) {
		dst_object = src_object;
		vm_object_reference(dst_object);
	} else {
		/*
		 * Create the top-level object for the destination entry.
		 * Doesn't actually shadow anything - we copy the pages
		 * directly.
		 */
		dst_object = vm_object_allocate_anon(atop(dst_entry->end -
		    dst_entry->start), NULL, NULL, 0);
#if VM_NRESERVLEVEL > 0
		dst_object->flags |= OBJ_COLORED;
		dst_object->pg_color = atop(dst_entry->start);
#endif
		dst_object->domain = src_object->domain;
		dst_object->charge = dst_entry->end - dst_entry->start;
	}

	VM_OBJECT_WLOCK(dst_object);
	KASSERT(upgrade || dst_entry->object.vm_object == NULL,
	    ("vm_fault_copy_entry: vm_object not NULL"));
	if (src_object != dst_object) {
		dst_entry->object.vm_object = dst_object;
		dst_entry->offset = 0;
		dst_entry->eflags &= ~MAP_ENTRY_VN_EXEC;
	}
	if (fork_charge != NULL) {
		KASSERT(dst_entry->cred == NULL,
		    ("vm_fault_copy_entry: leaked swp charge"));
		dst_object->cred = curthread->td_ucred;
		crhold(dst_object->cred);
		*fork_charge += dst_object->charge;
	} else if ((dst_object->type == OBJT_DEFAULT ||
	    dst_object->type == OBJT_SWAP) &&
	    dst_object->cred == NULL) {
		KASSERT(dst_entry->cred != NULL, ("no cred for entry %p",
		    dst_entry));
		dst_object->cred = dst_entry->cred;
		dst_entry->cred = NULL;
	}

	/*
	 * If not an upgrade, then enter the mappings in the pmap as
	 * read and/or execute accesses.  Otherwise, enter them as
	 * write accesses.
	 *
	 * A writeable large page mapping is only created if all of
	 * the constituent small page mappings are modified. Marking
	 * PTEs as modified on inception allows promotion to happen
	 * without taking potentially large number of soft faults.
	 */
	if (!upgrade)
		access &= ~VM_PROT_WRITE;

	/*
	 * Loop through all of the virtual pages within the entry's
	 * range, copying each page from the source object to the
	 * destination object.  Since the source is wired, those pages
	 * must exist.  In contrast, the destination is pageable.
	 * Since the destination object doesn't share any backing storage
	 * with the source object, all of its pages must be dirtied,
	 * regardless of whether they can be written.
	 */
	for (vaddr = dst_entry->start, dst_pindex = 0;
	    vaddr < dst_entry->end;
	    vaddr += PAGE_SIZE, dst_pindex++) {
again:
		/*
		 * Find the page in the source object, and copy it in.
		 * Because the source is wired down, the page will be
		 * in memory.
		 */
		if (src_object != dst_object)
			VM_OBJECT_RLOCK(src_object);
		object = src_object;
		pindex = src_pindex + dst_pindex;
		while ((src_m = vm_page_lookup(object, pindex)) == NULL &&
		    (backing_object = object->backing_object) != NULL) {
			/*
			 * Unless the source mapping is read-only or
			 * it is presently being upgraded from
			 * read-only, the first object in the shadow
			 * chain should provide all of the pages.  In
			 * other words, this loop body should never be
			 * executed when the source mapping is already
			 * read/write.
			 */
			KASSERT((src_entry->protection & VM_PROT_WRITE) == 0 ||
			    upgrade,
			    ("vm_fault_copy_entry: main object missing page"));

			VM_OBJECT_RLOCK(backing_object);
			pindex += OFF_TO_IDX(object->backing_object_offset);
			if (object != dst_object)
				VM_OBJECT_RUNLOCK(object);
			object = backing_object;
		}
		KASSERT(src_m != NULL, ("vm_fault_copy_entry: page missing"));

		if (object != dst_object) {
			/*
			 * Allocate a page in the destination object.
			 */
			dst_m = vm_page_alloc(dst_object, (src_object ==
			    dst_object ? src_pindex : 0) + dst_pindex,
			    VM_ALLOC_NORMAL);
			if (dst_m == NULL) {
				VM_OBJECT_WUNLOCK(dst_object);
				VM_OBJECT_RUNLOCK(object);
				vm_wait(dst_object);
				VM_OBJECT_WLOCK(dst_object);
				goto again;
			}
			pmap_copy_page(src_m, dst_m);
			VM_OBJECT_RUNLOCK(object);
			dst_m->dirty = dst_m->valid = src_m->valid;
		} else {
			dst_m = src_m;
			if (vm_page_busy_acquire(dst_m, VM_ALLOC_WAITFAIL) == 0)
				goto again;
			if (dst_m->pindex >= dst_object->size) {
				/*
				 * We are upgrading.  Index can occur
				 * out of bounds if the object type is
				 * vnode and the file was truncated.
				 */
				vm_page_xunbusy(dst_m);
				break;
			}
		}
		VM_OBJECT_WUNLOCK(dst_object);

		/*
		 * Enter it in the pmap. If a wired, copy-on-write
		 * mapping is being replaced by a write-enabled
		 * mapping, then wire that new mapping.
		 *
		 * The page can be invalid if the user called
		 * msync(MS_INVALIDATE) or truncated the backing vnode
		 * or shared memory object.  In this case, do not
		 * insert it into pmap, but still do the copy so that
		 * all copies of the wired map entry have similar
		 * backing pages.
		 */
		if (vm_page_all_valid(dst_m)) {
			pmap_enter(dst_map->pmap, vaddr, dst_m, prot,
			    access | (upgrade ? PMAP_ENTER_WIRED : 0), 0);
		}

		/*
		 * Mark it no longer busy, and put it on the active list.
		 */
		VM_OBJECT_WLOCK(dst_object);
		
		if (upgrade) {
			if (src_m != dst_m) {
				vm_page_unwire(src_m, PQ_INACTIVE);
				vm_page_wire(dst_m);
			} else {
				KASSERT(vm_page_wired(dst_m),
				    ("dst_m %p is not wired", dst_m));
			}
		} else {
			vm_page_activate(dst_m);
		}
		vm_page_xunbusy(dst_m);
	}
	VM_OBJECT_WUNLOCK(dst_object);
	if (upgrade) {
		dst_entry->eflags &= ~(MAP_ENTRY_COW | MAP_ENTRY_NEEDS_COPY);
		vm_object_deallocate(src_object);
	}
}

/*
 * Block entry into the machine-independent layer's page fault handler by
 * the calling thread.  Subsequent calls to vm_fault() by that thread will
 * return KERN_PROTECTION_FAILURE.  Enable machine-dependent handling of
 * spurious page faults. 
 */
int
vm_fault_disable_pagefaults(void)
{

	return (curthread_pflags_set(TDP_NOFAULTING | TDP_RESETSPUR));
}

void
vm_fault_enable_pagefaults(int save)
{

	curthread_pflags_restore(save);
}
