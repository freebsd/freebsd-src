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
 *
 * $Id: vm_glue.c,v 1.32 1995/12/07 12:48:11 davidg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/buf.h>
#include <sys/shm.h>
#include <sys/vmmeter.h>

#include <sys/kernel.h>
#include <sys/dkstat.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_inherit.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#include <machine/stdarg.h>
#include <machine/cpu.h>

/*
 * System initialization
 *
 * Note: proc0 from proc.h
 */

static void vm_init_limits __P((void *));
SYSINIT(vm_limits, SI_SUB_VM_CONF, SI_ORDER_FIRST, vm_init_limits, &proc0)

/*
 * THIS MUST BE THE LAST INITIALIZATION ITEM!!!
 *
 * Note: run scheduling should be divorced from the vm system.
 */
static void scheduler __P((void *));
SYSINIT(scheduler, SI_SUB_RUN_SCHEDULER, SI_ORDER_FIRST, scheduler, NULL)


static void swapout __P((struct proc *));

extern char kstack[];

/* vm_map_t upages_map; */

int
kernacc(addr, len, rw)
	caddr_t addr;
	int len, rw;
{
	boolean_t rv;
	vm_offset_t saddr, eaddr;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	saddr = trunc_page(addr);
	eaddr = round_page(addr + len);
	rv = vm_map_check_protection(kernel_map, saddr, eaddr, prot);
	return (rv == TRUE);
}

int
useracc(addr, len, rw)
	caddr_t addr;
	int len, rw;
{
	boolean_t rv;
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	/*
	 * XXX - check separately to disallow access to user area and user
	 * page tables - they are in the map.
	 *
	 * XXX - VM_MAXUSER_ADDRESS is an end address, not a max.  It was once
	 * only used (as an end address) in trap.c.  Use it as an end address
	 * here too.  This bogusness has spread.  I just fixed where it was
	 * used as a max in vm_mmap.c.
	 */
	if ((vm_offset_t) addr + len > /* XXX */ VM_MAXUSER_ADDRESS
	    || (vm_offset_t) addr + len < (vm_offset_t) addr) {
		return (FALSE);
	}
	rv = vm_map_check_protection(&curproc->p_vmspace->vm_map,
	    trunc_page(addr), round_page(addr + len), prot);
	return (rv == TRUE);
}

#ifdef KGDB
/*
 * Change protections on kernel pages from addr to addr+len
 * (presumably so debugger can plant a breakpoint).
 * All addresses are assumed to reside in the Sysmap,
 */
chgkprot(addr, len, rw)
	register caddr_t addr;
	int len, rw;
{
	vm_prot_t prot = rw == B_READ ? VM_PROT_READ : VM_PROT_WRITE;

	vm_map_protect(kernel_map, trunc_page(addr),
	    round_page(addr + len), prot, FALSE);
}
#endif
void
vslock(addr, len)
	caddr_t addr;
	u_int len;
{
	vm_map_pageable(&curproc->p_vmspace->vm_map, trunc_page(addr),
	    round_page(addr + len), FALSE);
}

void
vsunlock(addr, len, dirtied)
	caddr_t addr;
	u_int len;
	int dirtied;
{
#ifdef	lint
	dirtied++;
#endif	/* lint */
	vm_map_pageable(&curproc->p_vmspace->vm_map, trunc_page(addr),
	    round_page(addr + len), TRUE);
}

/*
 * Implement fork's actions on an address space.
 * Here we arrange for the address space to be copied or referenced,
 * allocate a user struct (pcb and kernel stack), then call the
 * machine-dependent layer to fill those in and make the new process
 * ready to run.
 * NOTE: the kernel stack may be at a different location in the child
 * process, and thus addresses of automatic variables may be invalid
 * after cpu_fork returns in the child process.  We do nothing here
 * after cpu_fork returns.
 */
int
vm_fork(p1, p2, isvfork)
	register struct proc *p1, *p2;
	int isvfork;
{
	register struct user *up;
	vm_offset_t addr, ptaddr;
	int error, i;
	struct vm_map *vp;

	while ((cnt.v_free_count + cnt.v_cache_count) < cnt.v_free_min) {
		VM_WAIT;
	}

	/*
	 * avoid copying any of the parent's pagetables or other per-process
	 * objects that reside in the map by marking all of them
	 * non-inheritable
	 */
	(void) vm_map_inherit(&p1->p_vmspace->vm_map,
	    UPT_MIN_ADDRESS - UPAGES * PAGE_SIZE, VM_MAX_ADDRESS, VM_INHERIT_NONE);
	p2->p_vmspace = vmspace_fork(p1->p_vmspace);

#ifdef SYSVSHM
	if (p1->p_vmspace->vm_shm)
		shmfork(p1, p2, isvfork);
#endif

	/*
	 * Allocate a wired-down (for now) pcb and kernel stack for the
	 * process
	 */

	addr = (vm_offset_t) kstack;

	vp = &p2->p_vmspace->vm_map;

	/* get new pagetables and kernel stack */
	(void) vm_map_find(vp, NULL, 0, &addr, UPT_MAX_ADDRESS - addr, FALSE);

	/* force in the page table encompassing the UPAGES */
	ptaddr = trunc_page((u_int) vtopte(addr));
	error = vm_map_pageable(vp, ptaddr, ptaddr + PAGE_SIZE, FALSE);
	if (error)
		panic("vm_fork: wire of PT failed. error=%d", error);

	/* and force in (demand-zero) the UPAGES */
	error = vm_map_pageable(vp, addr, addr + UPAGES * PAGE_SIZE, FALSE);
	if (error)
		panic("vm_fork: wire of UPAGES failed. error=%d", error);

	/* get a kernel virtual address for the UPAGES for this proc */
	up = (struct user *) kmem_alloc_pageable(u_map, UPAGES * PAGE_SIZE);
	if (up == NULL)
		panic("vm_fork: u_map allocation failed");

	/* and force-map the upages into the kernel pmap */
	for (i = 0; i < UPAGES; i++)
		pmap_kenter(((vm_offset_t) up) + PAGE_SIZE * i,
		    pmap_extract(vp->pmap, addr + PAGE_SIZE * i));

	p2->p_addr = up;

	/*
	 * p_stats and p_sigacts currently point at fields in the user struct
	 * but not at &u, instead at p_addr. Copy p_sigacts and parts of
	 * p_stats; zero the rest of p_stats (statistics).
	 */
	p2->p_stats = &up->u_stats;
	p2->p_sigacts = &up->u_sigacts;
	up->u_sigacts = *p1->p_sigacts;
	bzero(&up->u_stats.pstat_startzero,
	    (unsigned) ((caddr_t) &up->u_stats.pstat_endzero -
		(caddr_t) &up->u_stats.pstat_startzero));
	bcopy(&p1->p_stats->pstat_startcopy, &up->u_stats.pstat_startcopy,
	    ((caddr_t) &up->u_stats.pstat_endcopy -
		(caddr_t) &up->u_stats.pstat_startcopy));


	/*
	 * cpu_fork will copy and update the kernel stack and pcb, and make
	 * the child ready to run.  It marks the child so that it can return
	 * differently than the parent. It returns twice, once in the parent
	 * process and once in the child.
	 */
	return (cpu_fork(p1, p2));
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
	register struct proc *p = udata;
	int rss_limit;

	/*
	 * Set up the initial limits on process VM. Set the maximum resident
	 * set size to be half of (reasonably) available memory.  Since this
	 * is a soft limit, it comes into effect only when the system is out
	 * of memory - half of main memory helps to favor smaller processes,
	 * and reduces thrashing of the object cache.
	 */
	p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
	p->p_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
	p->p_rlimit[RLIMIT_DATA].rlim_max = MAXDSIZ;
	/* limit the limit to no less than 2MB */
	rss_limit = max(cnt.v_free_count, 512);
	p->p_rlimit[RLIMIT_RSS].rlim_cur = ptoa(rss_limit);
	p->p_rlimit[RLIMIT_RSS].rlim_max = RLIM_INFINITY;
}

void
faultin(p)
	struct proc *p;
{
	vm_offset_t i;
	vm_offset_t ptaddr;
	int s;

	if ((p->p_flag & P_INMEM) == 0) {
		vm_map_t map;
		int error;

		++p->p_lock;

		map = &p->p_vmspace->vm_map;
		/* force the page table encompassing the kernel stack (upages) */
		ptaddr = trunc_page((u_int) vtopte(kstack));
		error = vm_map_pageable(map, ptaddr, ptaddr + PAGE_SIZE, FALSE);
		if (error)
			panic("faultin: wire of PT failed. error=%d", error);

		/* wire in the UPAGES */
		error = vm_map_pageable(map, (vm_offset_t) kstack,
		    (vm_offset_t) kstack + UPAGES * PAGE_SIZE, FALSE);
		if (error)
			panic("faultin: wire of UPAGES failed. error=%d", error);

		/* and map them nicely into the kernel pmap */
		for (i = 0; i < UPAGES; i++) {
			vm_offset_t off = i * PAGE_SIZE;
			vm_offset_t pa = (vm_offset_t)
				pmap_extract(&p->p_vmspace->vm_pmap,
				    (vm_offset_t) kstack + off);

			if (pa == 0) 
				panic("faultin: missing page for UPAGES\n");
				
			pmap_kenter(((vm_offset_t) p->p_addr) + off, pa);
		}

		s = splhigh();

		if (p->p_stat == SRUN)
			setrunqueue(p);

		p->p_flag |= P_INMEM;

		/* undo the effect of setting SLOCK above */
		--p->p_lock;
		splx(s);

	}
}

/*
 * This swapin algorithm attempts to swap-in processes only if there
 * is enough space for them.  Of course, if a process waits for a long
 * time, it will be swapped in anyway.
 */
/* ARGSUSED*/
static void
scheduler(dummy)
	void *dummy;
{
	register struct proc *p;
	register int pri;
	struct proc *pp;
	int ppri;

loop:
	while ((cnt.v_free_count + cnt.v_cache_count) < (cnt.v_free_reserved + UPAGES + 2)) {
		VM_WAIT;
	}

	pp = NULL;
	ppri = INT_MIN;
	for (p = (struct proc *) allproc; p != NULL; p = p->p_next) {
		if (p->p_stat == SRUN && (p->p_flag & (P_INMEM | P_SWAPPING)) == 0) {
			int mempri;

			pri = p->p_swtime + p->p_slptime - p->p_nice * 8;
			mempri = pri > 0 ? pri : 0;
			/*
			 * if this process is higher priority and there is
			 * enough space, then select this process instead of
			 * the previous selection.
			 */
			if (pri > ppri) {
				pp = p;
				ppri = pri;
			}
		}
	}

	/*
	 * Nothing to do, back to sleep
	 */
	if ((p = pp) == NULL) {
		tsleep(&proc0, PVM, "sched", 0);
		goto loop;
	}
	/*
	 * We would like to bring someone in. (only if there is space).
	 */
	faultin(p);
	p->p_swtime = 0;
	goto loop;
}

#define	swappable(p) \
	(((p)->p_lock == 0) && \
		((p)->p_flag & (P_TRACED|P_NOSWAP|P_SYSTEM|P_INMEM|P_WEXIT|P_PHYSIO|P_SWAPPING)) == P_INMEM)

extern int vm_pageout_free_min;

/*
 * Swapout is driven by the pageout daemon.  Very simple, we find eligible
 * procs and unwire their u-areas.  We try to always "swap" at least one
 * process in case we need the room for a swapin.
 * If any procs have been sleeping/stopped for at least maxslp seconds,
 * they are swapped.  Else, we swap the longest-sleeping or stopped process,
 * if any, otherwise the longest-resident process.
 */
void
swapout_procs()
{
	register struct proc *p;
	struct proc *outp, *outp2;
	int outpri, outpri2;
	int didswap = 0;

	outp = outp2 = NULL;
	outpri = outpri2 = INT_MIN;
retry:
	for (p = (struct proc *) allproc; p != NULL; p = p->p_next) {
		if (!swappable(p))
			continue;
		switch (p->p_stat) {
		default:
			continue;

		case SSLEEP:
		case SSTOP:
			/*
			 * do not swapout a realtime process
			 */
			if (p->p_rtprio.type == RTP_PRIO_REALTIME)
				continue;

			/*
			 * do not swapout a process waiting on a critical
			 * event of some kind
			 */
			if (((p->p_priority & 0x7f) < PSOCK) ||
				(p->p_slptime <= 4))
				continue;

			vm_map_reference(&p->p_vmspace->vm_map);
			/*
			 * do not swapout a process that is waiting for VM
			 * datastructures there is a possible deadlock.
			 */
			if (!lock_try_write(&p->p_vmspace->vm_map.lock)) {
				vm_map_deallocate(&p->p_vmspace->vm_map);
				continue;
			}
			vm_map_unlock(&p->p_vmspace->vm_map);
			/*
			 * If the process has been asleep for awhile and had
			 * most of its pages taken away already, swap it out.
			 */
			swapout(p);
			vm_map_deallocate(&p->p_vmspace->vm_map);
			didswap++;
			goto retry;
		}
	}
	/*
	 * If we swapped something out, and another process needed memory,
	 * then wakeup the sched process.
	 */
	if (didswap)
		wakeup(&proc0);
}

static void
swapout(p)
	register struct proc *p;
{
	vm_map_t map = &p->p_vmspace->vm_map;
	vm_offset_t ptaddr;
	int i;

	++p->p_stats->p_ru.ru_nswap;
	/*
	 * remember the process resident count
	 */
	p->p_vmspace->vm_swrss =
	    p->p_vmspace->vm_pmap.pm_stats.resident_count;

	(void) splhigh();
	p->p_flag &= ~P_INMEM;
	p->p_flag |= P_SWAPPING;
	if (p->p_stat == SRUN)
		remrq(p);
	(void) spl0();

	/*
	 * let the upages be paged
	 */
	for(i=0;i<UPAGES;i++)
		pmap_kremove( (vm_offset_t) p->p_addr + PAGE_SIZE * i);

	vm_map_pageable(map, (vm_offset_t) kstack,
	    (vm_offset_t) kstack + UPAGES * PAGE_SIZE, TRUE);

	ptaddr = trunc_page((u_int) vtopte(kstack));
	vm_map_pageable(map, ptaddr, ptaddr + PAGE_SIZE, TRUE);

	p->p_flag &= ~P_SWAPPING;
	p->p_swtime = 0;
}

#ifdef DDB
/*
 * DEBUG stuff
 */

int indent;

#include <machine/stdarg.h>	/* see subr_prf.c */

/*ARGSUSED2*/
void
#if __STDC__
iprintf(const char *fmt,...)
#else
iprintf(fmt /* , va_alist */ )
	char *fmt;

 /* va_dcl */
#endif
{
	register int i;
	va_list ap;

	for (i = indent; i >= 8; i -= 8)
		printf("\t");
	while (--i >= 0)
		printf(" ");
	va_start(ap, fmt);
	printf("%r", fmt, ap);
	va_end(ap);
}
#endif /* DDB */
