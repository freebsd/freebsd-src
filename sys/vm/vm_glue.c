/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)vm_glue.c	8.6 (Berkeley) 1/5/94
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"
#include "opt_kstack_pages.h"
#include "opt_kstack_max_pages.h"
#include "opt_kstack_usage_prof.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>
#include <sys/shm.h>
#include <sys/vmmeter.h>
#include <sys/vmem.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/_kstack_cache.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/unistd.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

#include <machine/cpu.h>

#ifndef NO_SWAPPING
static int swapout(struct proc *);
static void swapclear(struct proc *);
static void vm_thread_swapin(struct thread *td);
static void vm_thread_swapout(struct thread *td);
#endif

/*
 * MPSAFE
 *
 * WARNING!  This code calls vm_map_check_protection() which only checks
 * the associated vm_map_entry range.  It does not determine whether the
 * contents of the memory is actually readable or writable.  In most cases
 * just checking the vm_map_entry is sufficient within the kernel's address
 * space.
 */
int
kernacc(addr, len, rw)
	void *addr;
	int len, rw;
{
	boolean_t rv;
	vm_offset_t saddr, eaddr;
	vm_prot_t prot;

	KASSERT((rw & ~VM_PROT_ALL) == 0,
	    ("illegal ``rw'' argument to kernacc (%x)\n", rw));

	if ((vm_offset_t)addr + len > kernel_map->max_offset ||
	    (vm_offset_t)addr + len < (vm_offset_t)addr)
		return (FALSE);

	prot = rw;
	saddr = trunc_page((vm_offset_t)addr);
	eaddr = round_page((vm_offset_t)addr + len);
	vm_map_lock_read(kernel_map);
	rv = vm_map_check_protection(kernel_map, saddr, eaddr, prot);
	vm_map_unlock_read(kernel_map);
	return (rv == TRUE);
}

/*
 * MPSAFE
 *
 * WARNING!  This code calls vm_map_check_protection() which only checks
 * the associated vm_map_entry range.  It does not determine whether the
 * contents of the memory is actually readable or writable.  vmapbuf(),
 * vm_fault_quick(), or copyin()/copout()/su*()/fu*() functions should be
 * used in conjuction with this call.
 */
int
useracc(addr, len, rw)
	void *addr;
	int len, rw;
{
	boolean_t rv;
	vm_prot_t prot;
	vm_map_t map;

	KASSERT((rw & ~VM_PROT_ALL) == 0,
	    ("illegal ``rw'' argument to useracc (%x)\n", rw));
	prot = rw;
	map = &curproc->p_vmspace->vm_map;
	if ((vm_offset_t)addr + len > vm_map_max(map) ||
	    (vm_offset_t)addr + len < (vm_offset_t)addr) {
		return (FALSE);
	}
	vm_map_lock_read(map);
	rv = vm_map_check_protection(map, trunc_page((vm_offset_t)addr),
	    round_page((vm_offset_t)addr + len), prot);
	vm_map_unlock_read(map);
	return (rv == TRUE);
}

int
vslock(void *addr, size_t len)
{
	vm_offset_t end, last, start;
	vm_size_t npages;
	int error;

	last = (vm_offset_t)addr + len;
	start = trunc_page((vm_offset_t)addr);
	end = round_page(last);
	if (last < (vm_offset_t)addr || end < (vm_offset_t)addr)
		return (EINVAL);
	npages = atop(end - start);
	if (npages > vm_page_max_wired)
		return (ENOMEM);
#if 0
	/*
	 * XXX - not yet
	 *
	 * The limit for transient usage of wired pages should be
	 * larger than for "permanent" wired pages (mlock()).
	 *
	 * Also, the sysctl code, which is the only present user
	 * of vslock(), does a hard loop on EAGAIN.
	 */
	if (npages + cnt.v_wire_count > vm_page_max_wired)
		return (EAGAIN);
#endif
	error = vm_map_wire(&curproc->p_vmspace->vm_map, start, end,
	    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
	/*
	 * Return EFAULT on error to match copy{in,out}() behaviour
	 * rather than returning ENOMEM like mlock() would.
	 */
	return (error == KERN_SUCCESS ? 0 : EFAULT);
}

void
vsunlock(void *addr, size_t len)
{

	/* Rely on the parameter sanity checks performed by vslock(). */
	(void)vm_map_unwire(&curproc->p_vmspace->vm_map,
	    trunc_page((vm_offset_t)addr), round_page((vm_offset_t)addr + len),
	    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
}

/*
 * Pin the page contained within the given object at the given offset.  If the
 * page is not resident, allocate and load it using the given object's pager.
 * Return the pinned page if successful; otherwise, return NULL.
 */
static vm_page_t
vm_imgact_hold_page(vm_object_t object, vm_ooffset_t offset)
{
	vm_page_t m, ma[1];
	vm_pindex_t pindex;
	int rv;

	VM_OBJECT_WLOCK(object);
	pindex = OFF_TO_IDX(offset);
	m = vm_page_grab(object, pindex, VM_ALLOC_NORMAL);
	if (m->valid != VM_PAGE_BITS_ALL) {
		ma[0] = m;
		rv = vm_pager_get_pages(object, ma, 1, 0);
		m = vm_page_lookup(object, pindex);
		if (m == NULL)
			goto out;
		if (rv != VM_PAGER_OK) {
			vm_page_lock(m);
			vm_page_free(m);
			vm_page_unlock(m);
			m = NULL;
			goto out;
		}
	}
	vm_page_xunbusy(m);
	vm_page_lock(m);
	vm_page_hold(m);
	vm_page_activate(m);
	vm_page_unlock(m);
out:
	VM_OBJECT_WUNLOCK(object);
	return (m);
}

/*
 * Return a CPU private mapping to the page at the given offset within the
 * given object.  The page is pinned before it is mapped.
 */
struct sf_buf *
vm_imgact_map_page(vm_object_t object, vm_ooffset_t offset)
{
	vm_page_t m;

	m = vm_imgact_hold_page(object, offset);
	if (m == NULL)
		return (NULL);
	sched_pin();
	return (sf_buf_alloc(m, SFB_CPUPRIVATE));
}

/*
 * Destroy the given CPU private mapping and unpin the page that it mapped.
 */
void
vm_imgact_unmap_page(struct sf_buf *sf)
{
	vm_page_t m;

	m = sf_buf_page(sf);
	sf_buf_free(sf);
	sched_unpin();
	vm_page_lock(m);
	vm_page_unhold(m);
	vm_page_unlock(m);
}

void
vm_sync_icache(vm_map_t map, vm_offset_t va, vm_offset_t sz)
{

	pmap_sync_icache(map->pmap, va, sz);
}

struct kstack_cache_entry *kstack_cache;
static int kstack_cache_size = 128;
static int kstacks;
static struct mtx kstack_cache_mtx;
MTX_SYSINIT(kstack_cache, &kstack_cache_mtx, "kstkch", MTX_DEF);

SYSCTL_INT(_vm, OID_AUTO, kstack_cache_size, CTLFLAG_RW, &kstack_cache_size, 0,
    "");
SYSCTL_INT(_vm, OID_AUTO, kstacks, CTLFLAG_RD, &kstacks, 0,
    "");

#ifndef KSTACK_MAX_PAGES
#define KSTACK_MAX_PAGES 32
#endif

/*
 * Create the kernel stack (including pcb for i386) for a new thread.
 * This routine directly affects the fork perf for a process and
 * create performance for a thread.
 */
int
vm_thread_new(struct thread *td, int pages)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m, ma[KSTACK_MAX_PAGES];
	struct kstack_cache_entry *ks_ce;
	int i;

	/* Bounds check */
	if (pages <= 1)
		pages = KSTACK_PAGES;
	else if (pages > KSTACK_MAX_PAGES)
		pages = KSTACK_MAX_PAGES;

	if (pages == KSTACK_PAGES) {
		mtx_lock(&kstack_cache_mtx);
		if (kstack_cache != NULL) {
			ks_ce = kstack_cache;
			kstack_cache = ks_ce->next_ks_entry;
			mtx_unlock(&kstack_cache_mtx);

			td->td_kstack_obj = ks_ce->ksobj;
			td->td_kstack = (vm_offset_t)ks_ce;
			td->td_kstack_pages = KSTACK_PAGES;
			return (1);
		}
		mtx_unlock(&kstack_cache_mtx);
	}

	/*
	 * Allocate an object for the kstack.
	 */
	ksobj = vm_object_allocate(OBJT_DEFAULT, pages);
	
	/*
	 * Get a kernel virtual address for this thread's kstack.
	 */
#if defined(__mips__)
	/*
	 * We need to align the kstack's mapped address to fit within
	 * a single TLB entry.
	 */
	if (vmem_xalloc(kernel_arena, (pages + KSTACK_GUARD_PAGES) * PAGE_SIZE,
	    PAGE_SIZE * 2, 0, 0, VMEM_ADDR_MIN, VMEM_ADDR_MAX,
	    M_BESTFIT | M_NOWAIT, &ks)) {
		ks = 0;
	}
#else
	ks = kva_alloc((pages + KSTACK_GUARD_PAGES) * PAGE_SIZE);
#endif
	if (ks == 0) {
		printf("vm_thread_new: kstack allocation failed\n");
		vm_object_deallocate(ksobj);
		return (0);
	}

	atomic_add_int(&kstacks, 1);
	if (KSTACK_GUARD_PAGES != 0) {
		pmap_qremove(ks, KSTACK_GUARD_PAGES);
		ks += KSTACK_GUARD_PAGES * PAGE_SIZE;
	}
	td->td_kstack_obj = ksobj;
	td->td_kstack = ks;
	/*
	 * Knowing the number of pages allocated is useful when you
	 * want to deallocate them.
	 */
	td->td_kstack_pages = pages;
	/* 
	 * For the length of the stack, link in a real page of ram for each
	 * page of stack.
	 */
	VM_OBJECT_WLOCK(ksobj);
	for (i = 0; i < pages; i++) {
		/*
		 * Get a kernel stack page.
		 */
		m = vm_page_grab(ksobj, i, VM_ALLOC_NOBUSY |
		    VM_ALLOC_NORMAL | VM_ALLOC_WIRED);
		ma[i] = m;
		m->valid = VM_PAGE_BITS_ALL;
	}
	VM_OBJECT_WUNLOCK(ksobj);
	pmap_qenter(ks, ma, pages);
	return (1);
}

static void
vm_thread_stack_dispose(vm_object_t ksobj, vm_offset_t ks, int pages)
{
	vm_page_t m;
	int i;

	atomic_add_int(&kstacks, -1);
	pmap_qremove(ks, pages);
	VM_OBJECT_WLOCK(ksobj);
	for (i = 0; i < pages; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("vm_thread_dispose: kstack already missing?");
		vm_page_lock(m);
		vm_page_unwire(m, 0);
		vm_page_free(m);
		vm_page_unlock(m);
	}
	VM_OBJECT_WUNLOCK(ksobj);
	vm_object_deallocate(ksobj);
	kva_free(ks - (KSTACK_GUARD_PAGES * PAGE_SIZE),
	    (pages + KSTACK_GUARD_PAGES) * PAGE_SIZE);
}

/*
 * Dispose of a thread's kernel stack.
 */
void
vm_thread_dispose(struct thread *td)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	struct kstack_cache_entry *ks_ce;
	int pages;

	pages = td->td_kstack_pages;
	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	td->td_kstack = 0;
	td->td_kstack_pages = 0;
	if (pages == KSTACK_PAGES && kstacks <= kstack_cache_size) {
		ks_ce = (struct kstack_cache_entry *)ks;
		ks_ce->ksobj = ksobj;
		mtx_lock(&kstack_cache_mtx);
		ks_ce->next_ks_entry = kstack_cache;
		kstack_cache = ks_ce;
		mtx_unlock(&kstack_cache_mtx);
		return;
	}
	vm_thread_stack_dispose(ksobj, ks, pages);
}

static void
vm_thread_stack_lowmem(void *nulll)
{
	struct kstack_cache_entry *ks_ce, *ks_ce1;

	mtx_lock(&kstack_cache_mtx);
	ks_ce = kstack_cache;
	kstack_cache = NULL;
	mtx_unlock(&kstack_cache_mtx);

	while (ks_ce != NULL) {
		ks_ce1 = ks_ce;
		ks_ce = ks_ce->next_ks_entry;

		vm_thread_stack_dispose(ks_ce1->ksobj, (vm_offset_t)ks_ce1,
		    KSTACK_PAGES);
	}
}

static void
kstack_cache_init(void *nulll)
{

	EVENTHANDLER_REGISTER(vm_lowmem, vm_thread_stack_lowmem, NULL,
	    EVENTHANDLER_PRI_ANY);
}

SYSINIT(vm_kstacks, SI_SUB_KTHREAD_INIT, SI_ORDER_ANY, kstack_cache_init, NULL);

#ifdef KSTACK_USAGE_PROF
/*
 * Track maximum stack used by a thread in kernel.
 */
static int max_kstack_used;

SYSCTL_INT(_debug, OID_AUTO, max_kstack_used, CTLFLAG_RD,
    &max_kstack_used, 0,
    "Maxiumum stack depth used by a thread in kernel");

void
intr_prof_stack_use(struct thread *td, struct trapframe *frame)
{
	vm_offset_t stack_top;
	vm_offset_t current;
	int used, prev_used;

	/*
	 * Testing for interrupted kernel mode isn't strictly
	 * needed. It optimizes the execution, since interrupts from
	 * usermode will have only the trap frame on the stack.
	 */
	if (TRAPF_USERMODE(frame))
		return;

	stack_top = td->td_kstack + td->td_kstack_pages * PAGE_SIZE;
	current = (vm_offset_t)(uintptr_t)&stack_top;

	/*
	 * Try to detect if interrupt is using kernel thread stack.
	 * Hardware could use a dedicated stack for interrupt handling.
	 */
	if (stack_top <= current || current < td->td_kstack)
		return;

	used = stack_top - current;
	for (;;) {
		prev_used = max_kstack_used;
		if (prev_used >= used)
			break;
		if (atomic_cmpset_int(&max_kstack_used, prev_used, used))
			break;
	}
}
#endif /* KSTACK_USAGE_PROF */

#ifndef NO_SWAPPING
/*
 * Allow a thread's kernel stack to be paged out.
 */
static void
vm_thread_swapout(struct thread *td)
{
	vm_object_t ksobj;
	vm_page_t m;
	int i, pages;

	cpu_thread_swapout(td);
	pages = td->td_kstack_pages;
	ksobj = td->td_kstack_obj;
	pmap_qremove(td->td_kstack, pages);
	VM_OBJECT_WLOCK(ksobj);
	for (i = 0; i < pages; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("vm_thread_swapout: kstack already missing?");
		vm_page_dirty(m);
		vm_page_lock(m);
		vm_page_unwire(m, 0);
		vm_page_unlock(m);
	}
	VM_OBJECT_WUNLOCK(ksobj);
}

/*
 * Bring the kernel stack for a specified thread back in.
 */
static void
vm_thread_swapin(struct thread *td)
{
	vm_object_t ksobj;
	vm_page_t ma[KSTACK_MAX_PAGES];
	int i, j, k, pages, rv;

	pages = td->td_kstack_pages;
	ksobj = td->td_kstack_obj;
	VM_OBJECT_WLOCK(ksobj);
	for (i = 0; i < pages; i++)
		ma[i] = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL |
		    VM_ALLOC_WIRED);
	for (i = 0; i < pages; i++) {
		if (ma[i]->valid != VM_PAGE_BITS_ALL) {
			vm_page_assert_xbusied(ma[i]);
			vm_object_pip_add(ksobj, 1);
			for (j = i + 1; j < pages; j++) {
				if (ma[j]->valid != VM_PAGE_BITS_ALL)
					vm_page_assert_xbusied(ma[j]);
				if (ma[j]->valid == VM_PAGE_BITS_ALL)
					break;
			}
			rv = vm_pager_get_pages(ksobj, ma + i, j - i, 0);
			if (rv != VM_PAGER_OK)
	panic("vm_thread_swapin: cannot get kstack for proc: %d",
				    td->td_proc->p_pid);
			vm_object_pip_wakeup(ksobj);
			for (k = i; k < j; k++)
				ma[k] = vm_page_lookup(ksobj, k);
			vm_page_xunbusy(ma[i]);
		} else if (vm_page_xbusied(ma[i]))
			vm_page_xunbusy(ma[i]);
	}
	VM_OBJECT_WUNLOCK(ksobj);
	pmap_qenter(td->td_kstack, ma, pages);
	cpu_thread_swapin(td);
}
#endif /* !NO_SWAPPING */

/*
 * Implement fork's actions on an address space.
 * Here we arrange for the address space to be copied or referenced,
 * allocate a user struct (pcb and kernel stack), then call the
 * machine-dependent layer to fill those in and make the new process
 * ready to run.  The new process is set up so that it returns directly
 * to user mode to avoid stack copying and relocation problems.
 */
int
vm_forkproc(td, p2, td2, vm2, flags)
	struct thread *td;
	struct proc *p2;
	struct thread *td2;
	struct vmspace *vm2;
	int flags;
{
	struct proc *p1 = td->td_proc;
	int error;

	if ((flags & RFPROC) == 0) {
		/*
		 * Divorce the memory, if it is shared, essentially
		 * this changes shared memory amongst threads, into
		 * COW locally.
		 */
		if ((flags & RFMEM) == 0) {
			if (p1->p_vmspace->vm_refcnt > 1) {
				error = vmspace_unshare(p1);
				if (error)
					return (error);
			}
		}
		cpu_fork(td, p2, td2, flags);
		return (0);
	}

	if (flags & RFMEM) {
		p2->p_vmspace = p1->p_vmspace;
		atomic_add_int(&p1->p_vmspace->vm_refcnt, 1);
	}

	while (vm_page_count_severe()) {
		VM_WAIT;
	}

	if ((flags & RFMEM) == 0) {
		p2->p_vmspace = vm2;
		if (p1->p_vmspace->vm_shm)
			shmfork(p1, p2);
	}

	/*
	 * cpu_fork will copy and update the pcb, set up the kernel stack,
	 * and make the child ready to run.
	 */
	cpu_fork(td, p2, td2, flags);
	return (0);
}

/*
 * Called after process has been wait(2)'ed apon and is being reaped.
 * The idea is to reclaim resources that we could not reclaim while
 * the process was still executing.
 */
void
vm_waitproc(p)
	struct proc *p;
{

	vmspace_exitfree(p);		/* and clean-out the vmspace */
}

void
faultin(p)
	struct proc *p;
{
#ifdef NO_SWAPPING

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((p->p_flag & P_INMEM) == 0)
		panic("faultin: proc swapped out with NO_SWAPPING!");
#else /* !NO_SWAPPING */
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	/*
	 * If another process is swapping in this process,
	 * just wait until it finishes.
	 */
	if (p->p_flag & P_SWAPPINGIN) {
		while (p->p_flag & P_SWAPPINGIN)
			msleep(&p->p_flag, &p->p_mtx, PVM, "faultin", 0);
		return;
	}
	if ((p->p_flag & P_INMEM) == 0) {
		/*
		 * Don't let another thread swap process p out while we are
		 * busy swapping it in.
		 */
		++p->p_lock;
		p->p_flag |= P_SWAPPINGIN;
		PROC_UNLOCK(p);

		/*
		 * We hold no lock here because the list of threads
		 * can not change while all threads in the process are
		 * swapped out.
		 */
		FOREACH_THREAD_IN_PROC(p, td)
			vm_thread_swapin(td);
		PROC_LOCK(p);
		swapclear(p);
		p->p_swtick = ticks;

		wakeup(&p->p_flag);

		/* Allow other threads to swap p out now. */
		--p->p_lock;
	}
#endif /* NO_SWAPPING */
}

/*
 * This swapin algorithm attempts to swap-in processes only if there
 * is enough space for them.  Of course, if a process waits for a long
 * time, it will be swapped in anyway.
 */
void
swapper(void)
{
	struct proc *p;
	struct thread *td;
	struct proc *pp;
	int slptime;
	int swtime;
	int ppri;
	int pri;

loop:
	if (vm_page_count_min()) {
		VM_WAIT;
		goto loop;
	}

	pp = NULL;
	ppri = INT_MIN;
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		if (p->p_state == PRS_NEW ||
		    p->p_flag & (P_SWAPPINGOUT | P_SWAPPINGIN | P_INMEM)) {
			PROC_UNLOCK(p);
			continue;
		}
		swtime = (ticks - p->p_swtick) / hz;
		FOREACH_THREAD_IN_PROC(p, td) {
			/*
			 * An otherwise runnable thread of a process
			 * swapped out has only the TDI_SWAPPED bit set.
			 * 
			 */
			thread_lock(td);
			if (td->td_inhibitors == TDI_SWAPPED) {
				slptime = (ticks - td->td_slptick) / hz;
				pri = swtime + slptime;
				if ((td->td_flags & TDF_SWAPINREQ) == 0)
					pri -= p->p_nice * 8;
				/*
				 * if this thread is higher priority
				 * and there is enough space, then select
				 * this process instead of the previous
				 * selection.
				 */
				if (pri > ppri) {
					pp = p;
					ppri = pri;
				}
			}
			thread_unlock(td);
		}
		PROC_UNLOCK(p);
	}
	sx_sunlock(&allproc_lock);

	/*
	 * Nothing to do, back to sleep.
	 */
	if ((p = pp) == NULL) {
		tsleep(&proc0, PVM, "swapin", MAXSLP * hz / 2);
		goto loop;
	}
	PROC_LOCK(p);

	/*
	 * Another process may be bringing or may have already
	 * brought this process in while we traverse all threads.
	 * Or, this process may even be being swapped out again.
	 */
	if (p->p_flag & (P_INMEM | P_SWAPPINGOUT | P_SWAPPINGIN)) {
		PROC_UNLOCK(p);
		goto loop;
	}

	/*
	 * We would like to bring someone in. (only if there is space).
	 * [What checks the space? ]
	 */
	faultin(p);
	PROC_UNLOCK(p);
	goto loop;
}

void
kick_proc0(void)
{

	wakeup(&proc0);
}

#ifndef NO_SWAPPING

/*
 * Swap_idle_threshold1 is the guaranteed swapped in time for a process
 */
static int swap_idle_threshold1 = 2;
SYSCTL_INT(_vm, OID_AUTO, swap_idle_threshold1, CTLFLAG_RW,
    &swap_idle_threshold1, 0, "Guaranteed swapped in time for a process");

/*
 * Swap_idle_threshold2 is the time that a process can be idle before
 * it will be swapped out, if idle swapping is enabled.
 */
static int swap_idle_threshold2 = 10;
SYSCTL_INT(_vm, OID_AUTO, swap_idle_threshold2, CTLFLAG_RW,
    &swap_idle_threshold2, 0, "Time before a process will be swapped out");

/*
 * First, if any processes have been sleeping or stopped for at least
 * "swap_idle_threshold1" seconds, they are swapped out.  If, however,
 * no such processes exist, then the longest-sleeping or stopped
 * process is swapped out.  Finally, and only as a last resort, if
 * there are no sleeping or stopped processes, the longest-resident
 * process is swapped out.
 */
void
swapout_procs(action)
int action;
{
	struct proc *p;
	struct thread *td;
	int didswap = 0;

retry:
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		struct vmspace *vm;
		int minslptime = 100000;
		int slptime;
		
		/*
		 * Watch out for a process in
		 * creation.  It may have no
		 * address space or lock yet.
		 */
		if (p->p_state == PRS_NEW)
			continue;
		/*
		 * An aio daemon switches its
		 * address space while running.
		 * Perform a quick check whether
		 * a process has P_SYSTEM.
		 */
		if ((p->p_flag & P_SYSTEM) != 0)
			continue;
		/*
		 * Do not swapout a process that
		 * is waiting for VM data
		 * structures as there is a possible
		 * deadlock.  Test this first as
		 * this may block.
		 *
		 * Lock the map until swapout
		 * finishes, or a thread of this
		 * process may attempt to alter
		 * the map.
		 */
		vm = vmspace_acquire_ref(p);
		if (vm == NULL)
			continue;
		if (!vm_map_trylock(&vm->vm_map))
			goto nextproc1;

		PROC_LOCK(p);
		if (p->p_lock != 0 ||
		    (p->p_flag & (P_STOPPED_SINGLE|P_TRACED|P_SYSTEM|P_WEXIT)
		    ) != 0) {
			goto nextproc;
		}
		/*
		 * only aiod changes vmspace, however it will be
		 * skipped because of the if statement above checking 
		 * for P_SYSTEM
		 */
		if ((p->p_flag & (P_INMEM|P_SWAPPINGOUT|P_SWAPPINGIN)) != P_INMEM)
			goto nextproc;

		switch (p->p_state) {
		default:
			/* Don't swap out processes in any sort
			 * of 'special' state. */
			break;

		case PRS_NORMAL:
			/*
			 * do not swapout a realtime process
			 * Check all the thread groups..
			 */
			FOREACH_THREAD_IN_PROC(p, td) {
				thread_lock(td);
				if (PRI_IS_REALTIME(td->td_pri_class)) {
					thread_unlock(td);
					goto nextproc;
				}
				slptime = (ticks - td->td_slptick) / hz;
				/*
				 * Guarantee swap_idle_threshold1
				 * time in memory.
				 */
				if (slptime < swap_idle_threshold1) {
					thread_unlock(td);
					goto nextproc;
				}

				/*
				 * Do not swapout a process if it is
				 * waiting on a critical event of some
				 * kind or there is a thread whose
				 * pageable memory may be accessed.
				 *
				 * This could be refined to support
				 * swapping out a thread.
				 */
				if (!thread_safetoswapout(td)) {
					thread_unlock(td);
					goto nextproc;
				}
				/*
				 * If the system is under memory stress,
				 * or if we are swapping
				 * idle processes >= swap_idle_threshold2,
				 * then swap the process out.
				 */
				if (((action & VM_SWAP_NORMAL) == 0) &&
				    (((action & VM_SWAP_IDLE) == 0) ||
				    (slptime < swap_idle_threshold2))) {
					thread_unlock(td);
					goto nextproc;
				}

				if (minslptime > slptime)
					minslptime = slptime;
				thread_unlock(td);
			}

			/*
			 * If the pageout daemon didn't free enough pages,
			 * or if this process is idle and the system is
			 * configured to swap proactively, swap it out.
			 */
			if ((action & VM_SWAP_NORMAL) ||
				((action & VM_SWAP_IDLE) &&
				 (minslptime > swap_idle_threshold2))) {
				if (swapout(p) == 0)
					didswap++;
				PROC_UNLOCK(p);
				vm_map_unlock(&vm->vm_map);
				vmspace_free(vm);
				sx_sunlock(&allproc_lock);
				goto retry;
			}
		}
nextproc:
		PROC_UNLOCK(p);
		vm_map_unlock(&vm->vm_map);
nextproc1:
		vmspace_free(vm);
		continue;
	}
	sx_sunlock(&allproc_lock);
	/*
	 * If we swapped something out, and another process needed memory,
	 * then wakeup the sched process.
	 */
	if (didswap)
		wakeup(&proc0);
}

static void
swapclear(p)
	struct proc *p;
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		td->td_flags |= TDF_INMEM;
		td->td_flags &= ~TDF_SWAPINREQ;
		TD_CLR_SWAPPED(td);
		if (TD_CAN_RUN(td))
			if (setrunnable(td)) {
#ifdef INVARIANTS
				/*
				 * XXX: We just cleared TDI_SWAPPED
				 * above and set TDF_INMEM, so this
				 * should never happen.
				 */
				panic("not waking up swapper");
#endif
			}
		thread_unlock(td);
	}
	p->p_flag &= ~(P_SWAPPINGIN|P_SWAPPINGOUT);
	p->p_flag |= P_INMEM;
}

static int
swapout(p)
	struct proc *p;
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
#if defined(SWAP_DEBUG)
	printf("swapping out %d\n", p->p_pid);
#endif

	/*
	 * The states of this process and its threads may have changed
	 * by now.  Assuming that there is only one pageout daemon thread,
	 * this process should still be in memory.
	 */
	KASSERT((p->p_flag & (P_INMEM|P_SWAPPINGOUT|P_SWAPPINGIN)) == P_INMEM,
		("swapout: lost a swapout race?"));

	/*
	 * remember the process resident count
	 */
	p->p_vmspace->vm_swrss = vmspace_resident_count(p->p_vmspace);
	/*
	 * Check and mark all threads before we proceed.
	 */
	p->p_flag &= ~P_INMEM;
	p->p_flag |= P_SWAPPINGOUT;
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		if (!thread_safetoswapout(td)) {
			thread_unlock(td);
			swapclear(p);
			return (EBUSY);
		}
		td->td_flags &= ~TDF_INMEM;
		TD_SET_SWAPPED(td);
		thread_unlock(td);
	}
	td = FIRST_THREAD_IN_PROC(p);
	++td->td_ru.ru_nswap;
	PROC_UNLOCK(p);

	/*
	 * This list is stable because all threads are now prevented from
	 * running.  The list is only modified in the context of a running
	 * thread in this process.
	 */
	FOREACH_THREAD_IN_PROC(p, td)
		vm_thread_swapout(td);

	PROC_LOCK(p);
	p->p_flag &= ~P_SWAPPINGOUT;
	p->p_swtick = ticks;
	return (0);
}
#endif /* !NO_SWAPPING */
