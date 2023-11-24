/*-
 * SPDX-License-Identifier: (BSD-3-Clause AND MIT-CMU)
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include "opt_vm.h"
#include "opt_kstack_pages.h"
#include "opt_kstack_max_pages.h"
#include "opt_kstack_usage_prof.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/asan.h>
#include <sys/domainset.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/racct.h>
#include <sys/refcount.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>
#include <sys/shm.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>
#include <sys/vmem.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/unistd.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_domainset.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

#include <machine/cpu.h>

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
kernacc(void *addr, int len, int rw)
{
	boolean_t rv;
	vm_offset_t saddr, eaddr;
	vm_prot_t prot;

	KASSERT((rw & ~VM_PROT_ALL) == 0,
	    ("illegal ``rw'' argument to kernacc (%x)\n", rw));

	if ((vm_offset_t)addr + len > vm_map_max(kernel_map) ||
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
 * used in conjunction with this call.
 */
int
useracc(void *addr, int len, int rw)
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
	if (npages > vm_page_max_user_wired)
		return (ENOMEM);
	error = vm_map_wire(&curproc->p_vmspace->vm_map, start, end,
	    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
	if (error == KERN_SUCCESS) {
		curthread->td_vslock_sz += len;
		return (0);
	}

	/*
	 * Return EFAULT on error to match copy{in,out}() behaviour
	 * rather than returning ENOMEM like mlock() would.
	 */
	return (EFAULT);
}

void
vsunlock(void *addr, size_t len)
{

	/* Rely on the parameter sanity checks performed by vslock(). */
	MPASS(curthread->td_vslock_sz >= len);
	curthread->td_vslock_sz -= len;
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
	vm_page_t m;
	vm_pindex_t pindex;

	pindex = OFF_TO_IDX(offset);
	(void)vm_page_grab_valid_unlocked(&m, object, pindex,
	    VM_ALLOC_NORMAL | VM_ALLOC_NOBUSY | VM_ALLOC_WIRED);
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
	vm_page_unwire(m, PQ_ACTIVE);
}

void
vm_sync_icache(vm_map_t map, vm_offset_t va, vm_offset_t sz)
{

	pmap_sync_icache(map->pmap, va, sz);
}

vm_object_t kstack_object;
static uma_zone_t kstack_cache;
static int kstack_cache_size;

static int
sysctl_kstack_cache_size(SYSCTL_HANDLER_ARGS)
{
	int error, oldsize;

	oldsize = kstack_cache_size;
	error = sysctl_handle_int(oidp, arg1, arg2, req);
	if (error == 0 && req->newptr && oldsize != kstack_cache_size)
		uma_zone_set_maxcache(kstack_cache, kstack_cache_size);
	return (error);
}
SYSCTL_PROC(_vm, OID_AUTO, kstack_cache_size,
    CTLTYPE_INT|CTLFLAG_MPSAFE|CTLFLAG_RW, &kstack_cache_size, 0,
    sysctl_kstack_cache_size, "IU", "Maximum number of cached kernel stacks");

/*
 * Create the kernel stack (including pcb for i386) for a new thread.
 */
static vm_offset_t
vm_thread_stack_create(struct domainset *ds, int pages)
{
	vm_page_t ma[KSTACK_MAX_PAGES];
	vm_offset_t ks;
	int i;

	/*
	 * Get a kernel virtual address for this thread's kstack.
	 */
	ks = kva_alloc((pages + KSTACK_GUARD_PAGES) * PAGE_SIZE);
	if (ks == 0) {
		printf("%s: kstack allocation failed\n", __func__);
		return (0);
	}

	if (KSTACK_GUARD_PAGES != 0) {
		pmap_qremove(ks, KSTACK_GUARD_PAGES);
		ks += KSTACK_GUARD_PAGES * PAGE_SIZE;
	}

	/*
	 * Allocate physical pages to back the stack.
	 */
	vm_thread_stack_back(ds, ks, ma, pages, VM_ALLOC_NORMAL);
	for (i = 0; i < pages; i++)
		vm_page_valid(ma[i]);
	pmap_qenter(ks, ma, pages);

	return (ks);
}

static void
vm_thread_stack_dispose(vm_offset_t ks, int pages)
{
	vm_page_t m;
	vm_pindex_t pindex;
	int i;

	pindex = atop(ks - VM_MIN_KERNEL_ADDRESS);

	pmap_qremove(ks, pages);
	VM_OBJECT_WLOCK(kstack_object);
	for (i = 0; i < pages; i++) {
		m = vm_page_lookup(kstack_object, pindex + i);
		if (m == NULL)
			panic("%s: kstack already missing?", __func__);
		vm_page_xbusy_claim(m);
		vm_page_unwire_noq(m);
		vm_page_free(m);
	}
	VM_OBJECT_WUNLOCK(kstack_object);
	kasan_mark((void *)ks, ptoa(pages), ptoa(pages), 0);
	kva_free(ks - (KSTACK_GUARD_PAGES * PAGE_SIZE),
	    (pages + KSTACK_GUARD_PAGES) * PAGE_SIZE);
}

/*
 * Allocate the kernel stack for a new thread.
 */
int
vm_thread_new(struct thread *td, int pages)
{
	vm_offset_t ks;

	/* Bounds check */
	if (pages <= 1)
		pages = kstack_pages;
	else if (pages > KSTACK_MAX_PAGES)
		pages = KSTACK_MAX_PAGES;

	ks = 0;
	if (pages == kstack_pages && kstack_cache != NULL)
		ks = (vm_offset_t)uma_zalloc(kstack_cache, M_NOWAIT);

	/*
	 * Ensure that kstack objects can draw pages from any memory
	 * domain.  Otherwise a local memory shortage can block a process
	 * swap-in.
	 */
	if (ks == 0)
		ks = vm_thread_stack_create(DOMAINSET_PREF(PCPU_GET(domain)),
		    pages);
	if (ks == 0)
		return (0);
	td->td_kstack = ks;
	td->td_kstack_pages = pages;
	kasan_mark((void *)ks, ptoa(pages), ptoa(pages), 0);
	kmsan_mark((void *)ks, ptoa(pages), KMSAN_STATE_UNINIT);
	return (1);
}

/*
 * Dispose of a thread's kernel stack.
 */
void
vm_thread_dispose(struct thread *td)
{
	vm_offset_t ks;
	int pages;

	pages = td->td_kstack_pages;
	ks = td->td_kstack;
	td->td_kstack = 0;
	td->td_kstack_pages = 0;
	kasan_mark((void *)ks, 0, ptoa(pages), KASAN_KSTACK_FREED);
	if (pages == kstack_pages)
		uma_zfree(kstack_cache, (void *)ks);
	else
		vm_thread_stack_dispose(ks, pages);
}

/*
 * Allocate physical pages, following the specified NUMA policy, to back a
 * kernel stack.
 */
void
vm_thread_stack_back(struct domainset *ds, vm_offset_t ks, vm_page_t ma[],
    int npages, int req_class)
{
	vm_pindex_t pindex;
	int n;

	pindex = atop(ks - VM_MIN_KERNEL_ADDRESS);

	VM_OBJECT_WLOCK(kstack_object);
	for (n = 0; n < npages;) {
		if (vm_ndomains > 1)
			kstack_object->domain.dr_policy = ds;

		/*
		 * Use WAITFAIL to force a reset of the domain selection policy
		 * if we had to sleep for pages.
		 */
		n += vm_page_grab_pages(kstack_object, pindex + n,
		    req_class | VM_ALLOC_WIRED | VM_ALLOC_WAITFAIL,
		    &ma[n], npages - n);
	}
	VM_OBJECT_WUNLOCK(kstack_object);
}

static int
kstack_import(void *arg, void **store, int cnt, int domain, int flags)
{
	struct domainset *ds;
	int i;

	if (domain == UMA_ANYDOMAIN)
		ds = DOMAINSET_RR();
	else
		ds = DOMAINSET_PREF(domain);

	for (i = 0; i < cnt; i++) {
		store[i] = (void *)vm_thread_stack_create(ds, kstack_pages);
		if (store[i] == NULL)
			break;
	}
	return (i);
}

static void
kstack_release(void *arg, void **store, int cnt)
{
	vm_offset_t ks;
	int i;

	for (i = 0; i < cnt; i++) {
		ks = (vm_offset_t)store[i];
		vm_thread_stack_dispose(ks, kstack_pages);
	}
}

static void
kstack_cache_init(void *null)
{
	kstack_object = vm_object_allocate(OBJT_SWAP,
	    atop(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS));
	kstack_cache = uma_zcache_create("kstack_cache",
	    kstack_pages * PAGE_SIZE, NULL, NULL, NULL, NULL,
	    kstack_import, kstack_release, NULL,
	    UMA_ZONE_FIRSTTOUCH);
	kstack_cache_size = imax(128, mp_ncpus * 4);
	uma_zone_set_maxcache(kstack_cache, kstack_cache_size);
}
SYSINIT(vm_kstacks, SI_SUB_KMEM, SI_ORDER_ANY, kstack_cache_init, NULL);

#ifdef KSTACK_USAGE_PROF
/*
 * Track maximum stack used by a thread in kernel.
 */
static int max_kstack_used;

SYSCTL_INT(_debug, OID_AUTO, max_kstack_used, CTLFLAG_RD,
    &max_kstack_used, 0,
    "Maximum stack depth used by a thread in kernel");

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

/*
 * Implement fork's actions on an address space.
 * Here we arrange for the address space to be copied or referenced,
 * allocate a user struct (pcb and kernel stack), then call the
 * machine-dependent layer to fill those in and make the new process
 * ready to run.  The new process is set up so that it returns directly
 * to user mode to avoid stack copying and relocation problems.
 */
int
vm_forkproc(struct thread *td, struct proc *p2, struct thread *td2,
    struct vmspace *vm2, int flags)
{
	struct proc *p1 = td->td_proc;
	struct domainset *dset;
	int error;

	if ((flags & RFPROC) == 0) {
		/*
		 * Divorce the memory, if it is shared, essentially
		 * this changes shared memory amongst threads, into
		 * COW locally.
		 */
		if ((flags & RFMEM) == 0) {
			error = vmspace_unshare(p1);
			if (error)
				return (error);
		}
		cpu_fork(td, p2, td2, flags);
		return (0);
	}

	if (flags & RFMEM) {
		p2->p_vmspace = p1->p_vmspace;
		refcount_acquire(&p1->p_vmspace->vm_refcnt);
	}
	dset = td2->td_domain.dr_policy;
	while (vm_page_count_severe_set(&dset->ds_mask)) {
		vm_wait_doms(&dset->ds_mask, 0);
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
 * Called after process has been wait(2)'ed upon and is being reaped.
 * The idea is to reclaim resources that we could not reclaim while
 * the process was still executing.
 */
void
vm_waitproc(struct proc *p)
{

	vmspace_exitfree(p);		/* and clean-out the vmspace */
}

void
kick_proc0(void)
{

	wakeup(&proc0);
}
