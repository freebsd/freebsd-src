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

/* Systemwide totals computed every five seconds. */
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

#if defined(_KERNEL) || defined(_WANT_VMMETER)
#include <sys/counter.h>

#ifdef _KERNEL
#define VMMETER_ALIGNED	__aligned(CACHE_LINE_SIZE)
#else
#define VMMETER_ALIGNED
#endif

/*
 * System wide statistics counters.
 * Locking:
 *      a - locked by atomic operations
 *      c - constant after initialization
 *      f - locked by vm_page_queue_free_mtx
 *      p - uses counter(9)
 *      q - changes are synchronized by the corresponding vm_pagequeue lock
 */
struct vmmeter {
	/*
	 * General system activity.
	 */
	counter_u64_t v_swtch;		/* (p) context switches */
	counter_u64_t v_trap;		/* (p) calls to trap */
	counter_u64_t v_syscall;	/* (p) calls to syscall() */
	counter_u64_t v_intr;		/* (p) device interrupts */
	counter_u64_t v_soft;		/* (p) software interrupts */
	/*
	 * Virtual memory activity.
	 */
	counter_u64_t v_vm_faults;	/* (p) address memory faults */
	counter_u64_t v_io_faults;	/* (p) page faults requiring I/O */
	counter_u64_t v_cow_faults;	/* (p) copy-on-writes faults */
	counter_u64_t v_cow_optim;	/* (p) optimized COW faults */
	counter_u64_t v_zfod;		/* (p) pages zero filled on demand */
	counter_u64_t v_ozfod;		/* (p) optimized zero fill pages */
	counter_u64_t v_swapin;		/* (p) swap pager pageins */
	counter_u64_t v_swapout;	/* (p) swap pager pageouts */
	counter_u64_t v_swappgsin;	/* (p) swap pager pages paged in */
	counter_u64_t v_swappgsout;	/* (p) swap pager pages paged out */
	counter_u64_t v_vnodein;	/* (p) vnode pager pageins */
	counter_u64_t v_vnodeout;	/* (p) vnode pager pageouts */
	counter_u64_t v_vnodepgsin;	/* (p) vnode_pager pages paged in */
	counter_u64_t v_vnodepgsout;	/* (p) vnode pager pages paged out */
	counter_u64_t v_intrans;	/* (p) intransit blocking page faults */
	counter_u64_t v_reactivated;	/* (p) reactivated by the pagedaemon */
	counter_u64_t v_pdwakeups;	/* (p) times daemon has awaken */
	counter_u64_t v_pdpages;	/* (p) pages analyzed by daemon */
	counter_u64_t v_pdshortfalls;	/* (p) page reclamation shortfalls */

	counter_u64_t v_dfree;		/* (p) pages freed by daemon */
	counter_u64_t v_pfree;		/* (p) pages freed by processes */
	counter_u64_t v_tfree;		/* (p) total pages freed */
	/*
	 * Fork/vfork/rfork activity.
	 */
	counter_u64_t v_forks;		/* (p) fork() calls */
	counter_u64_t v_vforks;		/* (p) vfork() calls */
	counter_u64_t v_rforks;		/* (p) rfork() calls */
	counter_u64_t v_kthreads;	/* (p) fork() calls by kernel */
	counter_u64_t v_forkpages;	/* (p) pages affected by fork() */
	counter_u64_t v_vforkpages;	/* (p) pages affected by vfork() */
	counter_u64_t v_rforkpages;	/* (p) pages affected by rfork() */
	counter_u64_t v_kthreadpages;	/* (p) ... and by kernel fork() */
#define	VM_METER_NCOUNTERS	\
	(offsetof(struct vmmeter, v_page_size) / sizeof(counter_u64_t))
	/*
	 * Distribution of page usages.
	 */
	u_int v_page_size;	/* (c) page size in bytes */
	u_int v_page_count;	/* (c) total number of pages in system */
	u_int v_free_reserved;	/* (c) pages reserved for deadlock */
	u_int v_free_target;	/* (c) pages desired free */
	u_int v_free_min;	/* (c) pages desired free */
	u_int v_free_count;	/* (f) pages free */
	u_int v_inactive_target; /* (c) pages desired inactive */
	u_int v_pageout_free_min;   /* (c) min pages reserved for kernel */
	u_int v_interrupt_free_min; /* (c) reserved pages for int code */
	u_int v_free_severe;	/* (c) severe page depletion point */
	u_int v_wire_count VMMETER_ALIGNED; /* (a) pages wired down */
	u_int v_active_count VMMETER_ALIGNED; /* (a) pages active */
	u_int v_inactive_count VMMETER_ALIGNED;	/* (a) pages inactive */
	u_int v_laundry_count VMMETER_ALIGNED; /* (a) pages eligible for
						  laundering */
};
#endif /* _KERNEL || _WANT_VMMETER */

#ifdef _KERNEL

extern struct vmmeter vm_cnt;
extern u_int vm_pageout_wakeup_thresh;

#define	VM_CNT_ADD(var, x)	counter_u64_add(vm_cnt.var, x)
#define	VM_CNT_INC(var)		VM_CNT_ADD(var, 1)
#define	VM_CNT_FETCH(var)	counter_u64_fetch(vm_cnt.var)

/*
 * Return TRUE if we are under our severe low-free-pages threshold
 *
 * This routine is typically used at the user<->system interface to determine
 * whether we need to block in order to avoid a low memory deadlock.
 */
static inline int
vm_page_count_severe(void)
{

	return (vm_cnt.v_free_severe > vm_cnt.v_free_count);
}

/*
 * Return TRUE if we are under our minimum low-free-pages threshold.
 *
 * This routine is typically used within the system to determine whether
 * we can execute potentially very expensive code in terms of memory.  It
 * is also used by the pageout daemon to calculate when to sleep, when
 * to wake waiters up, and when (after making a pass) to become more
 * desperate.
 */
static inline int
vm_page_count_min(void)
{

	return (vm_cnt.v_free_min > vm_cnt.v_free_count);
}

/*
 * Return TRUE if we have not reached our free page target during
 * free page recovery operations.
 */
static inline int
vm_page_count_target(void)
{

	return (vm_cnt.v_free_target > vm_cnt.v_free_count);
}

/*
 * Return the number of pages we need to free-up or cache
 * A positive number indicates that we do not have enough free pages.
 */
static inline int
vm_paging_target(void)
{

	return (vm_cnt.v_free_target - vm_cnt.v_free_count);
}

/*
 * Returns TRUE if the pagedaemon needs to be woken up.
 */
static inline int
vm_paging_needed(void)
{

	return (vm_cnt.v_free_count < vm_pageout_wakeup_thresh);
}

/*
 * Return the number of pages we need to launder.
 * A positive number indicates that we have a shortfall of clean pages.
 */
static inline int
vm_laundry_target(void)
{

	return (vm_paging_target());
}
#endif	/* _KERNEL */
#endif	/* _SYS_VMMETER_H_ */
