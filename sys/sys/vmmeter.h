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
 *	@(#)vmmeter.h	8.2 (Berkeley) 7/10/94
 * $FreeBSD$
 */

#ifndef _SYS_VMMETER_H_
#define _SYS_VMMETER_H_

/*
 * System wide statistics counters.
 */
struct vmmeter {
	/*
	 * General system activity.
	 */
	u_int v_swtch;		/* context switches */
	u_int v_trap;		/* calls to trap */
	u_int v_syscall;	/* calls to syscall() */
	u_int v_intr;		/* device interrupts */
	u_int v_soft;		/* software interrupts */
	/*
	 * Virtual memory activity.
	 */
	u_int v_vm_faults;	/* number of address memory faults */
	u_int v_cow_faults;	/* number of copy-on-writes */
	u_int v_cow_optim;	/* number of optimized copy-on-writes */
	u_int v_zfod;		/* pages zero filled on demand */
	u_int v_ozfod;		/* optimized zero fill pages */
	u_int v_swapin;		/* swap pager pageins */
	u_int v_swapout;	/* swap pager pageouts */
	u_int v_swappgsin;	/* swap pager pages paged in */
	u_int v_swappgsout;	/* swap pager pages paged out */
	u_int v_vnodein;	/* vnode pager pageins */
	u_int v_vnodeout;	/* vnode pager pageouts */
	u_int v_vnodepgsin;	/* vnode_pager pages paged in */
	u_int v_vnodepgsout;	/* vnode pager pages paged out */
	u_int v_intrans;	/* intransit blocking page faults */
	u_int v_reactivated;	/* number of pages reactivated from free list */
	u_int v_pdwakeups;	/* number of times daemon has awaken from sleep */
	u_int v_pdpages;	/* number of pages analyzed by daemon */

	u_int v_dfree;		/* pages freed by daemon */
	u_int v_pfree;		/* pages freed by exiting processes */
	u_int v_tfree;		/* total pages freed */
	/*
	 * Distribution of page usages.
	 */
	u_int v_page_size;	/* page size in bytes */
	u_int v_page_count;	/* total number of pages in system */
	u_int v_free_reserved;	/* number of pages reserved for deadlock */
	u_int v_free_target;	/* number of pages desired free */
	u_int v_free_min;	/* minimum number of pages desired free */
	u_int v_free_count;	/* number of pages free */
	u_int v_wire_count;	/* number of pages wired down */
	u_int v_active_count;	/* number of pages active */
	u_int v_inactive_target; /* number of pages desired inactive */
	u_int v_inactive_count;	/* number of pages inactive */
	u_int v_cache_count;	/* number of pages on buffer cache queue */
	u_int v_cache_min;	/* min number of pages desired on cache queue */
	u_int v_cache_max;	/* max number of pages in cached obj */
	u_int v_pageout_free_min;   /* min number pages reserved for kernel */
	u_int v_interrupt_free_min; /* reserved number of pages for int code */
	u_int v_free_severe;	/* severe depletion of pages below this pt */
	/*
	 * Fork/vfork/rfork activity.
	 */
	u_int v_forks;		/* number of fork() calls */
	u_int v_vforks;		/* number of vfork() calls */
	u_int v_rforks;		/* number of rfork() calls */
	u_int v_kthreads;	/* number of fork() calls by kernel */
	u_int v_forkpages;	/* number of VM pages affected by fork() */
	u_int v_vforkpages;	/* number of VM pages affected by vfork() */
	u_int v_rforkpages;	/* number of VM pages affected by rfork() */
	u_int v_kthreadpages;	/* number of VM pages affected by fork() by kernel */
};
#ifdef _KERNEL

extern struct vmmeter cnt;

/*
 * Return TRUE if we are under our reserved low-free-pages threshold
 */

static __inline 
int
vm_page_count_reserved(void)
{
    return (cnt.v_free_reserved > (cnt.v_free_count + cnt.v_cache_count));
}

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
    return (
	(cnt.v_free_target + cnt.v_cache_min) - 
	(cnt.v_free_count + cnt.v_cache_count)
    );
}

/*
 * Return a positive number if the pagedaemon needs to be woken up.
 */

static __inline 
int
vm_paging_needed(void)
{
    return (
	(cnt.v_free_reserved + cnt.v_cache_min) >
	(cnt.v_free_count + cnt.v_cache_count)
    );
}

#endif

/* systemwide totals computed every five seconds */
struct vmtotal
{
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

/*
 * Optional instrumentation.
 */
#ifdef PGINPROF

#define	NDMON	128
#define	NSMON	128

#define	DRES	20
#define	SRES	5

#define	PMONMIN	20
#define	PRES	50
#define	NPMON	64

#define	RMONMIN	130
#define	RRES	5
#define	NRMON	64

/* data and stack size distribution counters */
u_int	dmon[NDMON+1];
u_int	smon[NSMON+1];

/* page in time distribution counters */
u_int	pmon[NPMON+2];

/* reclaim time distribution counters */
u_int	rmon[NRMON+2];

int	pmonmin;
int	pres;
int	rmonmin;
int	rres;

u_int rectime;		/* accumulator for reclaim times */
u_int pgintime;		/* accumulator for page in times */
#endif

#endif
