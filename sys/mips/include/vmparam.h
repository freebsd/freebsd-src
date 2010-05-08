/*	$OpenBSD: vmparam.h,v 1.2 1998/09/15 10:50:12 pefo Exp $	*/
/*	$NetBSD: vmparam.h,v 1.5 1994/10/26 21:10:10 cgd Exp $	*/

/*
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
 *	from: Utah Hdr: vmparam.h 1.16 91/01/18
 *	@(#)vmparam.h	8.2 (Berkeley) 4/22/94
 *	JNPR: vmparam.h,v 1.3.2.1 2007/09/10 06:01:28 girish
 * $FreeBSD$
 */

#ifndef _MACHINE_VMPARAM_H_
#define	_MACHINE_VMPARAM_H_

/*
 * Machine dependent constants mips processors.
 */
/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.
 */
#define	USRTEXT		(1*PAGE_SIZE)
/*
 * USRSTACK needs to start a little below 0x8000000 because the R8000
 * and some QED CPUs perform some virtual address checks before the
 * offset is calculated.
 */
#define	USRSTACK	0x7ffff000	/* Start of user stack */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(128UL*1024*1024)	/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(128UL*1024*1024)	/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(1*1024UL*1024*1024)	/* max data size */
#endif
#ifndef DFLSSIZ
#define	DFLSSIZ		(8UL*1024*1024)		/* initial stack size limit */
#endif
#ifndef MAXSSIZ
#define	MAXSSIZ		(64UL*1024*1024)	/* max stack size */
#endif
#ifndef SGROWSIZ
#define	SGROWSIZ	(128UL*1024)		/* amount to grow stack */
#endif

/* 
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.	 You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP		20

/*
 * Mach derived constants
 */

/* user/kernel map constants */
#define	VM_MIN_ADDRESS		((vm_offset_t)0x00000000)
#define	VM_MAX_ADDRESS		((vm_offset_t)(intptr_t)(int32_t)0xffffffff)

#define	VM_MINUSER_ADDRESS	((vm_offset_t)0x00000000)
#define	VM_MAXUSER_ADDRESS	((vm_offset_t)0x80000000)
#define	VM_MAX_MMAP_ADDR	VM_MAXUSER_ADDRESS

#define	VM_MIN_KERNEL_ADDRESS		((vm_offset_t)0xC0000000)
#define	VM_MAX_KERNEL_ADDRESS		((vm_offset_t)0xFFFFC000)
#if 0
#define	KERNBASE		(VM_MIN_KERNEL_ADDRESS)
#else
#define	KERNBASE		((vm_offset_t)(intptr_t)(int32_t)0x80000000)
#endif

/*
 * Disable superpage reservations. (not sure if this is right
 * I copied it from ARM)
 */
#ifndef	VM_NRESERVLEVEL
#define	VM_NRESERVLEVEL		0
#endif


/* virtual sizes (bytes) for various kernel submaps */
#ifndef VM_KMEM_SIZE
#define	VM_KMEM_SIZE		(12 * 1024 * 1024)
#endif

/*
 * How many physical pages per KVA page allocated.
 * min(max(VM_KMEM_SIZE, Physical memory/VM_KMEM_SIZE_SCALE), VM_KMEM_SIZE_MAX)
 * is the total KVA space allocated for kmem_map.
 */
#ifndef VM_KMEM_SIZE_SCALE
#define	VM_KMEM_SIZE_SCALE	(3)
#endif

/*
 * Ceiling on amount of kmem_map kva space.
 */
#ifndef VM_KMEM_SIZE_MAX
#define	VM_KMEM_SIZE_MAX	(200 * 1024 * 1024)
#endif

/* initial pagein size of beginning of executable file */
#ifndef VM_INITIAL_PAGEIN
#define	VM_INITIAL_PAGEIN	16
#endif

/*
 * max number of non-contig chunks of physical RAM you can have
 */
#define	VM_PHYSSEG_MAX		32

/*
 * The physical address space is sparsely populated.
 */
#define	VM_PHYSSEG_SPARSE

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
 * we support 1 free list:
 *
 *	- DEFAULT for all systems
 */

#define	VM_NFREELIST		1
#define	VM_FREELIST_DEFAULT	0

/*
 * The largest allocation size is 1MB.
 */
#define	VM_NFREEORDER		9

#define SEGSHIFT	22		/* LOG2(NBSEG) */
#define NBSEG		(1 << SEGSHIFT)	/* bytes/segment */
#define SEGOFSET	(NBSEG-1)	/* byte offset into segment */

#endif /* !_MACHINE_VMPARAM_H_ */
