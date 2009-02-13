/* $FreeBSD$ */
/* From: NetBSD: vmparam.h,v 1.6 1997/09/23 23:23:23 mjacob Exp */
#ifndef	_MACHINE_VMPARAM_H
#define	_MACHINE_VMPARAM_H
/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah $Hdr: vmparam.h 1.16 91/01/18$
 *
 *	@(#)vmparam.h	8.2 (Berkeley) 4/22/94
 */

/*
 * Machine dependent constants for ia64.
 */
/*
 * USRSTACK is the top (end) of the user stack.  Immediately above the user
 * stack resides the syscall gateway page.
 */
#define	USRSTACK	VM_MAX_ADDRESS

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(1<<30)			/* max text size (1G) */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(1<<27)			/* initial data size (128M) */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(1<<30)			/* max data size (1G) */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(1<<21)			/* initial stack size (2M) */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(1<<28)			/* max stack size (256M) */
#endif
#ifndef SGROWSIZ
#define SGROWSIZ	(128UL*1024)		/* amount to grow stack */
#endif

/*
 * Boundary at which to place first MAPMEM segment if not explicitly
 * specified.  Should be a power of two.  This allows some slop for
 * the data segment to grow underneath the first mapped segment.
 */
#define MMSEG		0x200000

/*
 * The size of the clock loop.
 */
#define	LOOPPAGES	(maxfree - firstfree)

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP 		20

/*
 * A swapped in process is given a small amount of core without being bothered
 * by the page replacement algorithm.  Basically this says that if you are
 * swapped in you deserve some resources.  We protect the last SAFERSS
 * pages against paging and will just swap you out rather than paging you.
 * Note that each process has at least UPAGES pages which are not
 * paged anyways, in addition to SAFERSS.
 */
#define	SAFERSS		10		/* nominal ``small'' resident set size
					   protected against replacement */

/*
 * We need region 7 virtual addresses for pagetables.
 */
#define UMA_MD_SMALL_ALLOC

/*
 * The physical address space is sparsely populated.
 */
#define	VM_PHYSSEG_SPARSE

/*
 * The number of PHYSSEG entries is equal to the number of phys_avail
 * entries.
 */
#define	VM_PHYSSEG_MAX		49

/*
 * Create three free page pools: VM_FREEPOOL_DEFAULT is the default pool
 * from which physical pages are allocated and VM_FREEPOOL_DIRECT is
 * the pool from which physical pages for small UMA objects are
 * allocated.
 */
#define	VM_NFREEPOOL		3
#define	VM_FREEPOOL_CACHE	2
#define	VM_FREEPOOL_DEFAULT	0
#define	VM_FREEPOOL_DIRECT	1

/*
 * Create one free page list.
 */
#define	VM_NFREELIST		1
#define	VM_FREELIST_DEFAULT	0

/*
 * An allocation size of 256MB is supported in order to optimize the
 * use of the identity mappings in region 7 by UMA.
 */
#define	VM_NFREEORDER		16

/*
 * Manipulating region bits of an address.
 */
#define IA64_RR_BASE(n)         (((u_int64_t) (n)) << 61)
#define IA64_RR_MASK(x)         ((x) & ((1L << 61) - 1))

#define IA64_PHYS_TO_RR6(x)     ((x) | IA64_RR_BASE(6))
#define IA64_PHYS_TO_RR7(x)     ((x) | IA64_RR_BASE(7))

/*
 * Page size of the identity mappings in region 7.
 */
#ifndef LOG2_ID_PAGE_SIZE
#define	LOG2_ID_PAGE_SIZE	28		/* 256M */
#endif

#define	IA64_ID_PAGE_SHIFT	(LOG2_ID_PAGE_SIZE)
#define	IA64_ID_PAGE_SIZE	(1<<(LOG2_ID_PAGE_SIZE))
#define	IA64_ID_PAGE_MASK	(IA64_ID_PAGE_SIZE-1)

#define	IA64_BACKINGSTORE	IA64_RR_BASE(4)

/*
 * Mach derived constants
 */

/* user/kernel map constants */
#define VM_MIN_ADDRESS		0
#define	VM_MAX_ADDRESS		IA64_RR_BASE(5)
#define	VM_GATEWAY_SIZE		PAGE_SIZE
#define	VM_MAXUSER_ADDRESS	(VM_MAX_ADDRESS + VM_GATEWAY_SIZE)
#define	VM_MIN_KERNEL_ADDRESS	VM_MAXUSER_ADDRESS
#define VM_MAX_KERNEL_ADDRESS	(IA64_RR_BASE(6) - 1)

#define	KERNBASE		VM_MAX_ADDRESS

/* virtual sizes (bytes) for various kernel submaps */
#ifndef VM_KMEM_SIZE
#define VM_KMEM_SIZE		(12 * 1024 * 1024)
#endif

/*
 * How many physical pages per KVA page allocated.
 * min(max(max(VM_KMEM_SIZE, Physical memory/VM_KMEM_SIZE_SCALE),
 *     VM_KMEM_SIZE_MIN), VM_KMEM_SIZE_MAX)
 * is the total KVA space allocated for kmem_map.
 */
#ifndef VM_KMEM_SIZE_SCALE
#define	VM_KMEM_SIZE_SCALE	(4) /* XXX 8192 byte pages */
#endif

/* initial pagein size of beginning of executable file */
#ifndef VM_INITIAL_PAGEIN
#define	VM_INITIAL_PAGEIN	16
#endif

#endif	/* !_MACHINE_VMPARAM_H */
