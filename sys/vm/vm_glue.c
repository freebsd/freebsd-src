/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/shm.h>
#include <sys/vmmeter.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

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

#include <sys/user.h>

extern int maxslp;

/*
 * System initialization
 *
 * Note: proc0 from proc.h
 */
static void vm_init_limits(void *);
SYSINIT(vm_limits, SI_SUB_VM_CONF, SI_ORDER_FIRST, vm_init_limits, &proc0)

/*
 * THIS MUST BE THE LAST INITIALIZATION ITEM!!!
 *
 * Note: run scheduling should be divorced from the vm system.
 */
static void scheduler(void *);
SYSINIT(scheduler, SI_SUB_RUN_SCHEDULER, SI_ORDER_FIRST, scheduler, NULL)

#ifndef NO_SWAPPING
static void swapout(struct proc *);
static void vm_proc_swapin(struct proc *p);
static void vm_proc_swapout(struct proc *p);
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
	prot = rw;
	saddr = trunc_page((vm_offset_t)addr);
	eaddr = round_page((vm_offset_t)addr + len);
	rv = vm_map_check_protection(kernel_map, saddr, eaddr, prot);
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
	rv = vm_map_check_protection(map, trunc_page((vm_offset_t)addr),
	    round_page((vm_offset_t)addr + len), prot);
	return (rv == TRUE);
}

/*
 * MPSAFE
 */
void
vslock(addr, len)
	void *addr;
	u_int len;
{

	vm_map_wire(&curproc->p_vmspace->vm_map, trunc_page((vm_offset_t)addr),
	    round_page((vm_offset_t)addr + len),
	    VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES);
}

/*
 * MPSAFE
 */
void
vsunlock(addr, len)
	void *addr;
	u_int len;
{

	vm_map_unwire(&curproc->p_vmspace->vm_map,
	    trunc_page((vm_offset_t)addr),
	    round_page((vm_offset_t)addr + len),
	    VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES);
}

/*
 * Create the U area for a new process.
 * This routine directly affects the fork perf for a process.
 */
void
vm_proc_new(struct proc *p)
{
	vm_page_t ma[UAREA_PAGES];
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;
	u_int i;

	/*
	 * Allocate object for the upage.
	 */
	upobj = vm_object_allocate(OBJT_DEFAULT, UAREA_PAGES);
	p->p_upages_obj = upobj;

	/*
	 * Get a kernel virtual address for the U area for this process.
	 */
	up = kmem_alloc_nofault(kernel_map, UAREA_PAGES * PAGE_SIZE);
	if (up == 0)
		panic("vm_proc_new: upage allocation failed");
	p->p_uarea = (struct user *)up;

	for (i = 0; i < UAREA_PAGES; i++) {
		/*
		 * Get a uarea page.
		 */
		m = vm_page_grab(upobj, i,
		    VM_ALLOC_NORMAL | VM_ALLOC_RETRY | VM_ALLOC_WIRED);
		ma[i] = m;

		vm_page_lock_queues();
		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		m->valid = VM_PAGE_BITS_ALL;
		vm_page_unlock_queues();
	}

	/*
	 * Enter the pages into the kernel address space.
	 */
	pmap_qenter(up, ma, UAREA_PAGES);
}

/*
 * Dispose the U area for a process that has exited.
 * This routine directly impacts the exit perf of a process.
 * XXX proc_zone is marked UMA_ZONE_NOFREE, so this should never be called.
 */
void
vm_proc_dispose(struct proc *p)
{
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;

	upobj = p->p_upages_obj;
	VM_OBJECT_LOCK(upobj);
	if (upobj->resident_page_count != UAREA_PAGES)
		panic("vm_proc_dispose: incorrect number of pages in upobj");
	vm_page_lock_queues();
	while ((m = TAILQ_FIRST(&upobj->memq)) != NULL) {
		vm_page_busy(m);
		vm_page_unwire(m, 0);
		vm_page_free(m);
	}
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(upobj);
	up = (vm_offset_t)p->p_uarea;
	pmap_qremove(up, UAREA_PAGES);
	kmem_free(kernel_map, up, UAREA_PAGES * PAGE_SIZE);
	vm_object_deallocate(upobj);
}

#ifndef NO_SWAPPING
/*
 * Allow the U area for a process to be prejudicially paged out.
 */
static void
vm_proc_swapout(struct proc *p)
{
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;

	upobj = p->p_upages_obj;
	VM_OBJECT_LOCK(upobj);
	if (upobj->resident_page_count != UAREA_PAGES)
		panic("vm_proc_dispose: incorrect number of pages in upobj");
	vm_page_lock_queues();
	TAILQ_FOREACH(m, &upobj->memq, listq) {
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
	}
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(upobj);
	up = (vm_offset_t)p->p_uarea;
	pmap_qremove(up, UAREA_PAGES);
}

/*
 * Bring the U area for a specified process back in.
 */
static void
vm_proc_swapin(struct proc *p)
{
	vm_page_t ma[UAREA_PAGES];
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;
	int rv;
	int i;

	upobj = p->p_upages_obj;
	VM_OBJECT_LOCK(upobj);
	for (i = 0; i < UAREA_PAGES; i++) {
		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(upobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("vm_proc_swapin: cannot get upage");
		}
		ma[i] = m;
	}
	if (upobj->resident_page_count != UAREA_PAGES)
		panic("vm_proc_swapin: lost pages from upobj");
	vm_page_lock_queues();
	TAILQ_FOREACH(m, &upobj->memq, listq) {
		m->valid = VM_PAGE_BITS_ALL;
		vm_page_wire(m);
		vm_page_wakeup(m);
	}
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(upobj);
	up = (vm_offset_t)p->p_uarea;
	pmap_qenter(up, ma, UAREA_PAGES);
}

/*
 * Swap in the UAREAs of all processes swapped out to the given device.
 * The pages in the UAREA are marked dirty and their swap metadata is freed.
 */
void
vm_proc_swapin_all(struct swdevt *devidx)
{
	struct proc *p;
	vm_object_t object;
	vm_page_t m;

retry:
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		object = p->p_upages_obj;
		if (object != NULL) {
			VM_OBJECT_LOCK(object);
			if (swap_pager_isswapped(object, devidx)) {
				VM_OBJECT_UNLOCK(object);
				sx_sunlock(&allproc_lock);
				faultin(p);
				PROC_UNLOCK(p);
				VM_OBJECT_LOCK(object);
				vm_page_lock_queues();
				TAILQ_FOREACH(m, &object->memq, listq)
					vm_page_dirty(m);
				vm_page_unlock_queues();
				swap_pager_freespace(object, 0,
				    object->un_pager.swp.swp_bcount);
				VM_OBJECT_UNLOCK(object);
				goto retry;
			}
			VM_OBJECT_UNLOCK(object);
		}
		PROC_UNLOCK(p);
	}
	sx_sunlock(&allproc_lock);
}
#endif

#ifndef KSTACK_MAX_PAGES
#define KSTACK_MAX_PAGES 32
#endif

/*
 * Create the kernel stack (including pcb for i386) for a new thread.
 * This routine directly affects the fork perf for a process and
 * create performance for a thread.
 */
void
vm_thread_new(struct thread *td, int pages)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m, ma[KSTACK_MAX_PAGES];
	int i;

	/* Bounds check */
	if (pages <= 1)
		pages = KSTACK_PAGES;
	else if (pages > KSTACK_MAX_PAGES)
		pages = KSTACK_MAX_PAGES;
	/*
	 * Allocate an object for the kstack.
	 */
	ksobj = vm_object_allocate(OBJT_DEFAULT, pages);
	td->td_kstack_obj = ksobj;
	/*
	 * Get a kernel virtual address for this thread's kstack.
	 */
	ks = kmem_alloc_nofault(kernel_map,
	   (pages + KSTACK_GUARD_PAGES) * PAGE_SIZE);
	if (ks == 0)
		panic("vm_thread_new: kstack allocation failed");
	if (KSTACK_GUARD_PAGES != 0) {
		pmap_qremove(ks, KSTACK_GUARD_PAGES);
		ks += KSTACK_GUARD_PAGES * PAGE_SIZE;
	}
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
	VM_OBJECT_LOCK(ksobj);
	for (i = 0; i < pages; i++) {
		/*
		 * Get a kernel stack page.
		 */
		m = vm_page_grab(ksobj, i,
		    VM_ALLOC_NORMAL | VM_ALLOC_RETRY | VM_ALLOC_WIRED);
		ma[i] = m;
		vm_page_lock_queues();
		vm_page_wakeup(m);
		m->valid = VM_PAGE_BITS_ALL;
		vm_page_unlock_queues();
	}
	VM_OBJECT_UNLOCK(ksobj);
	pmap_qenter(ks, ma, pages);
}

/*
 * Dispose of a thread's kernel stack.
 */
void
vm_thread_dispose(struct thread *td)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	int i, pages;

	pages = td->td_kstack_pages;
	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	pmap_qremove(ks, pages);
	VM_OBJECT_LOCK(ksobj);
	for (i = 0; i < pages; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("vm_thread_dispose: kstack already missing?");
		vm_page_lock_queues();
		vm_page_busy(m);
		vm_page_unwire(m, 0);
		vm_page_free(m);
		vm_page_unlock_queues();
	}
	VM_OBJECT_UNLOCK(ksobj);
	vm_object_deallocate(ksobj);
	kmem_free(kernel_map, ks - (KSTACK_GUARD_PAGES * PAGE_SIZE),
	    (pages + KSTACK_GUARD_PAGES) * PAGE_SIZE);
}

/*
 * Allow a thread's kernel stack to be paged out.
 */
void
vm_thread_swapout(struct thread *td)
{
	vm_object_t ksobj;
	vm_page_t m;
	int i, pages;

#ifdef	__alpha__
	/*
	 * Make sure we aren't fpcurthread.
	 */
	alpha_fpstate_save(td, 1);
#endif
	pages = td->td_kstack_pages;
	ksobj = td->td_kstack_obj;
	pmap_qremove(td->td_kstack, pages);
	VM_OBJECT_LOCK(ksobj);
	for (i = 0; i < pages; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("vm_thread_swapout: kstack already missing?");
		vm_page_lock_queues();
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
		vm_page_unlock_queues();
	}
	VM_OBJECT_UNLOCK(ksobj);
}

/*
 * Bring the kernel stack for a specified thread back in.
 */
void
vm_thread_swapin(struct thread *td)
{
	vm_object_t ksobj;
	vm_page_t m, ma[KSTACK_MAX_PAGES];
	int i, pages, rv;

	pages = td->td_kstack_pages;
	ksobj = td->td_kstack_obj;
	VM_OBJECT_LOCK(ksobj);
	for (i = 0; i < pages; i++) {
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(ksobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("vm_thread_swapin: cannot get kstack for proc: %d", td->td_proc->p_pid);
			m = vm_page_lookup(ksobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}
		ma[i] = m;
		vm_page_lock_queues();
		vm_page_wire(m);
		vm_page_wakeup(m);
		vm_page_unlock_queues();
	}
	VM_OBJECT_UNLOCK(ksobj);
	pmap_qenter(td->td_kstack, ma, pages);
#ifdef	__alpha__
	/*
	 * The pcb may be at a different physical address now so cache the
	 * new address.
	 */
	td->td_md.md_pcbpaddr = (void *)vtophys((vm_offset_t)td->td_pcb);
#endif
}

/*
 * Set up a variable-sized alternate kstack.
 */
void
vm_thread_new_altkstack(struct thread *td, int pages)
{

	td->td_altkstack = td->td_kstack;
	td->td_altkstack_obj = td->td_kstack_obj;
	td->td_altkstack_pages = td->td_kstack_pages;

	vm_thread_new(td, pages);
}

/*
 * Restore the original kstack.
 */
void
vm_thread_dispose_altkstack(struct thread *td)
{

	vm_thread_dispose(td);

	td->td_kstack = td->td_altkstack;
	td->td_kstack_obj = td->td_altkstack_obj;
	td->td_kstack_pages = td->td_altkstack_pages;
	td->td_altkstack = 0;
	td->td_altkstack_obj = NULL;
	td->td_altkstack_pages = 0;
}

/*
 * Implement fork's actions on an address space.
 * Here we arrange for the address space to be copied or referenced,
 * allocate a user struct (pcb and kernel stack), then call the
 * machine-dependent layer to fill those in and make the new process
 * ready to run.  The new process is set up so that it returns directly
 * to user mode to avoid stack copying and relocation problems.
 */
void
vm_forkproc(td, p2, td2, flags)
	struct thread *td;
	struct proc *p2;
	struct thread *td2;
	int flags;
{
	struct proc *p1 = td->td_proc;
	struct user *up;

	GIANT_REQUIRED;

	if ((flags & RFPROC) == 0) {
		/*
		 * Divorce the memory, if it is shared, essentially
		 * this changes shared memory amongst threads, into
		 * COW locally.
		 */
		if ((flags & RFMEM) == 0) {
			if (p1->p_vmspace->vm_refcnt > 1) {
				vmspace_unshare(p1);
			}
		}
		cpu_fork(td, p2, td2, flags);
		return;
	}

	if (flags & RFMEM) {
		p2->p_vmspace = p1->p_vmspace;
		p1->p_vmspace->vm_refcnt++;
	}

	while (vm_page_count_severe()) {
		VM_WAIT;
	}

	if ((flags & RFMEM) == 0) {
		p2->p_vmspace = vmspace_fork(p1->p_vmspace);

		pmap_pinit2(vmspace_pmap(p2->p_vmspace));

		if (p1->p_vmspace->vm_shm)
			shmfork(p1, p2);
	}

	/* XXXKSE this is unsatisfactory but should be adequate */
	up = p2->p_uarea;
	MPASS(p2->p_sigacts != NULL);

	/*
	 * p_stats currently points at fields in the user struct
	 * but not at &u, instead at p_addr. Copy parts of
	 * p_stats; zero the rest of p_stats (statistics).
	 */
	p2->p_stats = &up->u_stats;
	bzero(&up->u_stats.pstat_startzero,
	    (unsigned) ((caddr_t) &up->u_stats.pstat_endzero -
		(caddr_t) &up->u_stats.pstat_startzero));
	bcopy(&p1->p_stats->pstat_startcopy, &up->u_stats.pstat_startcopy,
	    ((caddr_t) &up->u_stats.pstat_endcopy -
		(caddr_t) &up->u_stats.pstat_startcopy));

	/*
	 * cpu_fork will copy and update the pcb, set up the kernel stack,
	 * and make the child ready to run.
	 */
	cpu_fork(td, p2, td2, flags);
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

	GIANT_REQUIRED;
	vmspace_exitfree(p);		/* and clean-out the vmspace */
}

/*
 * Set default limits for VM system.
 * Called for proc 0, and then inherited by all others.
 *
 * XXX should probably act directly on proc0.
 */
static void
vm_init_limits(udata)
	void *udata;
{
	struct proc *p = udata;
	int rss_limit;

	/*
	 * Set up the initial limits on process VM. Set the maximum resident
	 * set size to be half of (reasonably) available memory.  Since this
	 * is a soft limit, it comes into effect only when the system is out
	 * of memory - half of main memory helps to favor smaller processes,
	 * and reduces thrashing of the object cache.
	 */
	p->p_rlimit[RLIMIT_STACK].rlim_cur = dflssiz;
	p->p_rlimit[RLIMIT_STACK].rlim_max = maxssiz;
	p->p_rlimit[RLIMIT_DATA].rlim_cur = dfldsiz;
	p->p_rlimit[RLIMIT_DATA].rlim_max = maxdsiz;
	/* limit the limit to no less than 2MB */
	rss_limit = max(cnt.v_free_count, 512);
	p->p_rlimit[RLIMIT_RSS].rlim_cur = ptoa(rss_limit);
	p->p_rlimit[RLIMIT_RSS].rlim_max = RLIM_INFINITY;
}

void
faultin(p)
	struct proc *p;
{
#ifdef NO_SWAPPING

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((p->p_sflag & PS_INMEM) == 0)
		panic("faultin: proc swapped out with NO_SWAPPING!");
#else /* !NO_SWAPPING */
	struct thread *td;

	GIANT_REQUIRED;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	/*
	 * If another process is swapping in this process,
	 * just wait until it finishes.
	 */
	if (p->p_sflag & PS_SWAPPINGIN)
		msleep(&p->p_sflag, &p->p_mtx, PVM, "faultin", 0);
	else if ((p->p_sflag & PS_INMEM) == 0) {
		/*
		 * Don't let another thread swap process p out while we are
		 * busy swapping it in.
		 */
		++p->p_lock;
		mtx_lock_spin(&sched_lock);
		p->p_sflag |= PS_SWAPPINGIN;
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);

		vm_proc_swapin(p);
		FOREACH_THREAD_IN_PROC(p, td)
			vm_thread_swapin(td);

		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
		p->p_sflag &= ~PS_SWAPPINGIN;
		p->p_sflag |= PS_INMEM;
		FOREACH_THREAD_IN_PROC(p, td) {
			TD_CLR_SWAPPED(td);
			if (TD_CAN_RUN(td))
				setrunnable(td);
		}
		mtx_unlock_spin(&sched_lock);

		wakeup(&p->p_sflag);

		/* Allow other threads to swap p out now. */
		--p->p_lock;
	}
#endif /* NO_SWAPPING */
}

/*
 * This swapin algorithm attempts to swap-in processes only if there
 * is enough space for them.  Of course, if a process waits for a long
 * time, it will be swapped in anyway.
 *
 *  XXXKSE - process with the thread with highest priority counts..
 *
 * Giant is still held at this point, to be released in tsleep.
 */
/* ARGSUSED*/
static void
scheduler(dummy)
	void *dummy;
{
	struct proc *p;
	struct thread *td;
	int pri;
	struct proc *pp;
	int ppri;

	mtx_assert(&Giant, MA_OWNED | MA_NOTRECURSED);
	/* GIANT_REQUIRED */

loop:
	if (vm_page_count_min()) {
		VM_WAIT;
		goto loop;
	}

	pp = NULL;
	ppri = INT_MIN;
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		struct ksegrp *kg;
		if (p->p_sflag & (PS_INMEM | PS_SWAPPINGOUT | PS_SWAPPINGIN)) {
			continue;
		}
		mtx_lock_spin(&sched_lock);
		FOREACH_THREAD_IN_PROC(p, td) {
			/*
			 * An otherwise runnable thread of a process
			 * swapped out has only the TDI_SWAPPED bit set.
			 * 
			 */
			if (td->td_inhibitors == TDI_SWAPPED) {
				kg = td->td_ksegrp;
				pri = p->p_swtime + kg->kg_slptime;
				if ((p->p_sflag & PS_SWAPINREQ) == 0) {
					pri -= kg->kg_nice * 8;
				}

				/*
				 * if this ksegrp is higher priority
				 * and there is enough space, then select
				 * this process instead of the previous
				 * selection.
				 */
				if (pri > ppri) {
					pp = p;
					ppri = pri;
				}
			}
		}
		mtx_unlock_spin(&sched_lock);
	}
	sx_sunlock(&allproc_lock);

	/*
	 * Nothing to do, back to sleep.
	 */
	if ((p = pp) == NULL) {
		tsleep(&proc0, PVM, "sched", maxslp * hz / 2);
		goto loop;
	}
	PROC_LOCK(p);

	/*
	 * Another process may be bringing or may have already
	 * brought this process in while we traverse all threads.
	 * Or, this process may even be being swapped out again.
	 */
	if (p->p_sflag & (PS_INMEM | PS_SWAPPINGOUT | PS_SWAPPINGIN)) {
		PROC_UNLOCK(p);
		goto loop;
	}

	mtx_lock_spin(&sched_lock);
	p->p_sflag &= ~PS_SWAPINREQ;
	mtx_unlock_spin(&sched_lock);

	/*
	 * We would like to bring someone in. (only if there is space).
	 * [What checks the space? ]
	 */
	faultin(p);
	PROC_UNLOCK(p);
	mtx_lock_spin(&sched_lock);
	p->p_swtime = 0;
	mtx_unlock_spin(&sched_lock);
	goto loop;
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
 * Swapout is driven by the pageout daemon.  Very simple, we find eligible
 * procs and unwire their u-areas.  We try to always "swap" at least one
 * process in case we need the room for a swapin.
 * If any procs have been sleeping/stopped for at least maxslp seconds,
 * they are swapped.  Else, we swap the longest-sleeping or stopped process,
 * if any, otherwise the longest-resident process.
 */
void
swapout_procs(action)
int action;
{
	struct proc *p;
	struct thread *td;
	struct ksegrp *kg;
	int didswap = 0;

	GIANT_REQUIRED;

retry:
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		struct vmspace *vm;
		int minslptime = 100000;
		
		/*
		 * Watch out for a process in
		 * creation.  It may have no
		 * address space or lock yet.
		 */
		mtx_lock_spin(&sched_lock);
		if (p->p_state == PRS_NEW) {
			mtx_unlock_spin(&sched_lock);
			continue;
		}
		mtx_unlock_spin(&sched_lock);

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
		PROC_LOCK(p);
		vm = p->p_vmspace;
		KASSERT(vm != NULL,
			("swapout_procs: a process has no address space"));
		++vm->vm_refcnt;
		PROC_UNLOCK(p);
		if (!vm_map_trylock(&vm->vm_map))
			goto nextproc1;

		PROC_LOCK(p);
		if (p->p_lock != 0 ||
		    (p->p_flag & (P_STOPPED_SINGLE|P_TRACED|P_SYSTEM|P_WEXIT)
		    ) != 0) {
			goto nextproc2;
		}
		/*
		 * only aiod changes vmspace, however it will be
		 * skipped because of the if statement above checking 
		 * for P_SYSTEM
		 */
		if ((p->p_sflag & (PS_INMEM|PS_SWAPPINGOUT|PS_SWAPPINGIN)) != PS_INMEM)
			goto nextproc2;

		switch (p->p_state) {
		default:
			/* Don't swap out processes in any sort
			 * of 'special' state. */
			break;

		case PRS_NORMAL:
			mtx_lock_spin(&sched_lock);
			/*
			 * do not swapout a realtime process
			 * Check all the thread groups..
			 */
			FOREACH_KSEGRP_IN_PROC(p, kg) {
				if (PRI_IS_REALTIME(kg->kg_pri_class))
					goto nextproc;

				/*
				 * Guarantee swap_idle_threshold1
				 * time in memory.
				 */
				if (kg->kg_slptime < swap_idle_threshold1)
					goto nextproc;

				/*
				 * Do not swapout a process if it is
				 * waiting on a critical event of some
				 * kind or there is a thread whose
				 * pageable memory may be accessed.
				 *
				 * This could be refined to support
				 * swapping out a thread.
				 */
				FOREACH_THREAD_IN_GROUP(kg, td) {
					if ((td->td_priority) < PSOCK ||
					    !thread_safetoswapout(td))
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
				    (kg->kg_slptime < swap_idle_threshold2)))
					goto nextproc;

				if (minslptime > kg->kg_slptime)
					minslptime = kg->kg_slptime;
			}

			/*
			 * If the process has been asleep for awhile and had
			 * most of its pages taken away already, swap it out.
			 */
			if ((action & VM_SWAP_NORMAL) ||
				((action & VM_SWAP_IDLE) &&
				 (minslptime > swap_idle_threshold2))) {
				swapout(p);
				didswap++;
				mtx_unlock_spin(&sched_lock);
				PROC_UNLOCK(p);
				vm_map_unlock(&vm->vm_map);
				vmspace_free(vm);
				sx_sunlock(&allproc_lock);
				goto retry;
			}
nextproc:			
			mtx_unlock_spin(&sched_lock);
		}
nextproc2:
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
swapout(p)
	struct proc *p;
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED | MA_NOTRECURSED);
#if defined(SWAP_DEBUG)
	printf("swapping out %d\n", p->p_pid);
#endif

	/*
	 * The states of this process and its threads may have changed
	 * by now.  Assuming that there is only one pageout daemon thread,
	 * this process should still be in memory.
	 */
	KASSERT((p->p_sflag & (PS_INMEM|PS_SWAPPINGOUT|PS_SWAPPINGIN)) == PS_INMEM,
		("swapout: lost a swapout race?"));

#if defined(INVARIANTS)
	/*
	 * Make sure that all threads are safe to be swapped out.
	 *
	 * Alternatively, we could swap out only safe threads.
	 */
	FOREACH_THREAD_IN_PROC(p, td) {
		KASSERT(thread_safetoswapout(td),
			("swapout: there is a thread not safe for swapout"));
	}
#endif /* INVARIANTS */

	++p->p_stats->p_ru.ru_nswap;
	/*
	 * remember the process resident count
	 */
	p->p_vmspace->vm_swrss = vmspace_resident_count(p->p_vmspace);

	p->p_sflag &= ~PS_INMEM;
	p->p_sflag |= PS_SWAPPINGOUT;
	PROC_UNLOCK(p);
	FOREACH_THREAD_IN_PROC(p, td)
		TD_SET_SWAPPED(td);
	mtx_unlock_spin(&sched_lock);

	vm_proc_swapout(p);
	FOREACH_THREAD_IN_PROC(p, td)
		vm_thread_swapout(td);

	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	p->p_sflag &= ~PS_SWAPPINGOUT;
	p->p_swtime = 0;
}
#endif /* !NO_SWAPPING */
