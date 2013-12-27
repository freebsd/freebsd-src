/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)vmmeter.h	8.2 (Berkeley) 7/10/94
 * $FreeBSD$
 */

#ifndef _SYS_VMMETER_H_
#define _SYS_VMMETER_H_

/*
 * This value is used by ps(1) to change sleep state flag from 'S' to
 * 'I' and by the sched process to set the alarm clock.
 */
#define	MAXSLP			20

/*
 * System wide statistics counters.
 * Locking:
 *      a - locked by atomic operations
 *      c - constant after initialization
 *      f - locked by vm_page_queue_free_mtx
 *      p - locked by being in the PCPU and atomicity respect to interrupts
 *      q - changes are synchronized by the corresponding vm_pagequeue lock
 */
struct vmmeter {
	/*
	 * General system activity.
	 */
	u_int v_swtch;		/* (p) context switches */
	u_int v_trap;		/* (p) calls to trap */
	u_int v_syscall;	/* (p) calls to syscall() */
	u_int v_intr;		/* (p) device interrupts */
	u_int v_soft;		/* (p) software interrupts */
	/*
	 * Virtual memory activity.
	 */
	u_int v_vm_faults;	/* (p) address memory faults */
	u_int v_io_faults;	/* (p) page faults requiring I/O */
	u_int v_cow_faults;	/* (p) copy-on-writes faults */
	u_int v_cow_optim;	/* (p) optimized copy-on-writes faults */
	u_int v_zfod;		/* (p) pages zero filled on demand */
	u_int v_ozfod;		/* (p) optimized zero fill pages */
	u_int v_swapin;		/* (p) swap pager pageins */
	u_int v_swapout;	/* (p) swap pager pageouts */
	u_int v_swappgsin;	/* (p) swap pager pages paged in */
	u_int v_swappgsout;	/* (p) swap pager pages paged out */
	u_int v_vnodein;	/* (p) vnode pager pageins */
	u_int v_vnodeout;	/* (p) vnode pager pageouts */
	u_int v_vnodepgsin;	/* (p) vnode_pager pages paged in */
	u_int v_vnodepgsout;	/* (p) vnode pager pages paged out */
	u_int v_intrans;	/* (p) intransit blocking page faults */
	u_int v_reactivated;	/* (f) pages reactivated from free list */
	u_int v_pdwakeups;	/* (f) times daemon has awaken from sleep */
	u_int v_pdpages;	/* (p) pages analyzed by daemon */

	u_int v_tcached;	/* (p) total pages cached */
	u_int v_dfree;		/* (p) pages freed by daemon */
	u_int v_pfree;		/* (p) pages freed by exiting processes */
	u_int v_tfree;		/* (p) total pages freed */
	/*
	 * Distribution of page usages.
	 */
	u_int v_page_size;	/* (c) page size in bytes */
	u_int v_page_count;	/* (c) total number of pages in system */
	u_int v_free_reserved;	/* (c) pages reserved for deadlock */
	u_int v_free_target;	/* (c) pages desired free */
	u_int v_free_min;	/* (c) pages desired free */
	u_int v_free_count;	/* (f) pages free */
	u_int v_wire_count;	/* (a) pages wired down */
	u_int v_active_count;	/* (q) pages active */
	u_int v_inactive_target; /* (c) pages desired inactive */
	u_int v_inactive_count;	/* (q) pages inactive */
	u_int v_cache_count;	/* (f) pages on cache queue */
	u_int v_cache_min;	/* (c) min pages desired on cache queue */
	u_int v_cache_max;	/* (c) max pages in cached obj (unused) */
	u_int v_pageout_free_min;   /* (c) min pages reserved for kernel */
	u_int v_interrupt_free_min; /* (c) reserved pages for int code */
	u_int v_free_severe;	/* (c) severe page depletion point */
	/*
	 * Fork/vfork/rfork activity.
	 */
	u_int v_forks;		/* (p) fork() calls */
	u_int v_vforks;		/* (p) vfork() calls */
	u_int v_rforks;		/* (p) rfork() calls */
	u_int v_kthreads;	/* (p) fork() calls by kernel */
	u_int v_forkpages;	/* (p) VM pages affected by fork() */
	u_int v_vforkpages;	/* (p) VM pages affected by vfork() */
	u_int v_rforkpages;	/* (p) VM pages affected by rfork() */
	u_int v_kthreadpages;	/* (p) VM pages affected by fork() by kernel */
};
#ifdef _KERNEL

extern struct vmmeter cnt;

extern int vm_pageout_wakeup_thresh;

/*
 * Return TRUE if we are under our severe low-free-pages threshold
 *
 * This routine is typically used at the user<->system interface to determine
 * whether we need to block in order to avoid a low memory deadlock.
 */

static __inline 
int
vm_page_count_severe(void)
{
    return (cnt.v_free_severe > (cnt.v_free_count + cnt.v_cache_count));
}

/*
 * Return TRUE if we are under our minimum low-free-pages threshold.
 *
 * This routine is typically used within the system to determine whether
 * we can execute potentially very expensive code in terms of memory.  It
 * is also used by the pageout daemon to calculate when to sleep, when
 * to wake waiters up, and when (after making a pass) to become more
 * desparate.
 */

static __inline 
int
vm_page_count_min(void)
{
    return (cnt.v_free_min > (cnt.v_free_count + cnt.v_cache_count));
}

/*
 * Return TRUE if we have not reached our free page target during
 * free page recovery operations.
 */

static __inline 
int
vm_page_count_target(void)
{
    return (cnt.v_free_target > (cnt.v_free_count + cnt.v_cache_count));
}

/*
 * Return the number of pages we need to free-up or cache
 * A positive number indicates that we do not have enough free pages.
 */

static __inline 
int
vm_paging_target(void)
{
    return (cnt.v_free_target - (cnt.v_free_count + cnt.v_cache_count));
}

/*
 * Returns TRUE if the pagedaemon needs to be woken up.
 */

static __inline 
int
vm_paging_needed(void)
{
    return (cnt.v_free_count + cnt.v_cache_count < vm_pageout_wakeup_thresh);
}

#endif

/* systemwide totals computed every five seconds */
struct vmtotal {
	int16_t	t_rq;		/* length of the run queue */
	int16_t	t_dw;		/* jobs in ``disk wait'' (neg priority) */
	int16_t	t_pw;		/* jobs in page wait */
	int16_t	t_sl;		/* jobs sleeping in core */
	int16_t	t_sw;		/* swapped out runnable/short block jobs */
	int32_t	t_vm;		/* total virtual memory */
	int32_t	t_avm;		/* active virtual memory */
	int32_t	t_rm;		/* total real memory in use */
	int32_t	t_arm;		/* active real memory */
	int32_t	t_vmshr;	/* shared virtual memory */
	int32_t	t_avmshr;	/* active shared virtual memory */
	int32_t	t_rmshr;	/* shared real memory */
	int32_t	t_armshr;	/* active shared real memory */
	int32_t	t_free;		/* free memory pages */
};

#endif
