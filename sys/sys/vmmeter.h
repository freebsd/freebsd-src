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
 *	@(#)vmmeter.h	8.1 (Berkeley) 6/2/93
 * $Id: vmmeter.h,v 1.7 1995/01/09 16:05:15 davidg Exp $
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
	unsigned v_swtch;	/* context switches */
	unsigned v_trap;	/* calls to trap */
	unsigned v_syscall;	/* calls to syscall() */
	unsigned v_intr;	/* device interrupts */
	unsigned v_soft;	/* software interrupts */
	/*
	 * Virtual memory activity.
	 */
	unsigned v_lookups;	/* object cache lookups */
	unsigned v_hits;	/* object cache hits */
	unsigned v_vm_faults;	/* number of address memory faults */
	unsigned v_cow_faults;	/* number of copy-on-writes */
	unsigned v_swapin;	/* swap pager pageins */
	unsigned v_swapout;	/* swap pager pageouts */
	unsigned v_swappgsin;	/* swap pager pages paged in */
	unsigned v_swappgsout;	/* swap pager pages paged out */
	unsigned v_vnodein;	/* vnode pager pageins */
	unsigned v_vnodeout;	/* vnode pager pageouts */
	unsigned v_vnodepgsin;	/* vnode_pager pages paged in */
	unsigned v_vnodepgsout;	/* vnode pager pages paged out */
	unsigned v_intrans;	/* intransit blocking page faults */
	unsigned v_reactivated;	/* number of pages reactivated from free list */
	unsigned v_pdwakeups;	/* number of times daemon has awaken from sleep */
	unsigned v_pdpages;	/* number of pages analyzed by daemon */
	unsigned v_dfree;	/* pages freed by daemon */
	unsigned v_pfree;	/* pages freed by exiting processes */
	unsigned v_tfree;	/* total pages freed */
	unsigned v_zfod;	/* pages zero filled on demand */
	unsigned v_nzfod;	/* number of zfod's created */
	/*
	 * Distribution of page usages.
	 */
	unsigned v_page_size;	/* page size in bytes */
	unsigned v_kernel_pages;/* number of pages in use by kernel */
	unsigned v_page_count;	/* total number of pages in system */
	unsigned v_free_reserved; /* number of pages reserved for deadlock */
	unsigned v_free_target;	/* number of pages desired free */
	unsigned v_free_min;	/* minimum number of pages desired free */
	unsigned v_free_count;	/* number of pages free */
	unsigned v_wire_count;	/* number of pages wired down */
	unsigned v_active_count;/* number of pages active */
	unsigned v_inactive_target; /* number of pages desired inactive */
	unsigned v_inactive_count;  /* number of pages inactive */
	unsigned v_cache_count;		/* number of pages on buffer cache queue */
	unsigned v_cache_min;		/* min number of pages desired on cache queue */
	unsigned v_cache_max;		/* max number of pages in cached obj */
	unsigned v_pageout_free_min;	/* min number pages reserved for kernel */
	unsigned v_interrupt_free_min;	/* reserved number of pages for int code */
};
#ifdef KERNEL
struct	vmmeter cnt;
#endif

/* systemwide totals computed every five seconds */
struct vmtotal
{
	short	t_rq;		/* length of the run queue */
	short	t_dw;		/* jobs in ``disk wait'' (neg priority) */
	short	t_pw;		/* jobs in page wait */
	short	t_sl;		/* jobs sleeping in core */
	short	t_sw;		/* swapped out runnable/short block jobs */
	long	t_vm;		/* total virtual memory */
	long	t_avm;		/* active virtual memory */
	long	t_rm;		/* total real memory in use */
	long	t_arm;		/* active real memory */
	long	t_vmshr;	/* shared virtual memory */
	long	t_avmshr;	/* active shared virtual memory */
	long	t_rmshr;	/* shared real memory */
	long	t_armshr;	/* active shared real memory */
	long	t_free;		/* free memory pages */
};
#ifdef KERNEL
struct	vmtotal total;
#endif

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
unsigned int	dmon[NDMON+1];
unsigned int	smon[NSMON+1];

/* page in time distribution counters */
unsigned int	pmon[NPMON+2];

/* reclaim time distribution counters */
unsigned int	rmon[NRMON+2];

int	pmonmin;
int	pres;
int	rmonmin;
int	rres;

unsigned rectime;		/* accumulator for reclaim times */
unsigned pgintime;		/* accumulator for page in times */
#endif

#endif
