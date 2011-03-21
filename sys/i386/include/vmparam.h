/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)vmparam.h	5.9 (Berkeley) 5/12/91
 * $FreeBSD$
 */


#ifndef _MACHINE_VMPARAM_H_
#define _MACHINE_VMPARAM_H_ 1

/*
 * Machine dependent constants for 386.
 */

/*
 * Virtual memory related constants, all in bytes
 */
#define	MAXTSIZ		(128UL*1024*1024)	/* max text size */
#ifndef DFLDSIZ
#define	DFLDSIZ		(128UL*1024*1024)	/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(512UL*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(8UL*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(64UL*1024*1024)	/* max stack size */
#endif
#ifndef SGROWSIZ
#define SGROWSIZ	(128UL*1024)		/* amount to grow stack */
#endif

/*
 * The physical address space is densely populated.
 */
#define	VM_PHYSSEG_DENSE

/*
 * The number of PHYSSEG entries must be one greater than the number
 * of phys_avail entries because the phys_avail entry that spans the
 * largest physical address that is accessible by ISA DMA is split
 * into two PHYSSEG entries. 
 */
#define	VM_PHYSSEG_MAX		17

/*
 * Create two free page pools.  Since the i386 kernel virtual address
 * space does not include a mapping onto the machine's entire physical
 * memory, VM_FREEPOOL_DIRECT is defined as an alias for the default
 * pool, VM_FREEPOOL_DEFAULT.
 */
#define	VM_NFREEPOOL		2
#define	VM_FREEPOOL_CACHE	1
#define	VM_FREEPOOL_DEFAULT	0
#define	VM_FREEPOOL_DIRECT	0

/*
 * Create two free page lists: VM_FREELIST_DEFAULT is for physical
 * pages that are above the largest physical address that is
 * accessible by ISA DMA and VM_FREELIST_ISADMA is for physical pages
 * that are below that address.
 */
#define	VM_NFREELIST		2
#define	VM_FREELIST_DEFAULT	0
#define	VM_FREELIST_ISADMA	1

/*
 * The largest allocation size is 2MB under PAE and 4MB otherwise.
 */
#ifdef PAE
#define	VM_NFREEORDER		10
#else
#define	VM_NFREEORDER		11
#endif

/*
 * Only one memory domain.
 */
#ifndef VM_NDOMAIN
#define	VM_NDOMAIN		1
#endif

/*
 * Enable superpage reservations: 1 level.
 */
#ifndef	VM_NRESERVLEVEL
#define	VM_NRESERVLEVEL		1
#endif

/*
 * Level 0 reservations consist of 512 pages under PAE and 1024 pages
 * otherwise.
 */
#ifndef	VM_LEVEL_0_ORDER
#ifdef PAE
#define	VM_LEVEL_0_ORDER	9
#else
#define	VM_LEVEL_0_ORDER	10
#endif
#endif

/*
 * Kernel physical load address.
 */
#ifndef KERNLOAD
#if defined(XEN) && !defined(XEN_PRIVILEGED_GUEST)
#define	KERNLOAD		0
#else
#define	KERNLOAD		(1 << PDRSHIFT)
#endif
#endif /* !defined(KERNLOAD) */

/*
 * Virtual addresses of things.  Derived from the page directory and
 * page table indexes from pmap.h for precision.
 * Because of the page that is both a PD and PT, it looks a little
 * messy at times, but hey, we'll do anything to save a page :-)
 */

#ifdef XEN
#define VM_MAX_KERNEL_ADDRESS	HYPERVISOR_VIRT_START
#else
#define VM_MAX_KERNEL_ADDRESS	VADDR(KPTDI+NKPDE-1, NPTEPG-1)
#endif

#define VM_MIN_KERNEL_ADDRESS	VADDR(PTDPTDI, PTDPTDI)

#define	KERNBASE		VADDR(KPTDI, 0)

#define UPT_MAX_ADDRESS		VADDR(PTDPTDI, PTDPTDI)
#define UPT_MIN_ADDRESS		VADDR(PTDPTDI, 0)

#define VM_MAXUSER_ADDRESS	VADDR(PTDPTDI, 0)

#define USRSTACK		VM_MAXUSER_ADDRESS

#define VM_MAX_ADDRESS		VADDR(PTDPTDI, PTDPTDI)
#define VM_MIN_ADDRESS		((vm_offset_t)0)

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
#define	VM_KMEM_SIZE_SCALE	(3)
#endif

/*
 * Ceiling on amount of kmem_map kva space.
 */
#ifndef VM_KMEM_SIZE_MAX
#define	VM_KMEM_SIZE_MAX	(320 * 1024 * 1024)
#endif

/* initial pagein size of beginning of executable file */
#ifndef VM_INITIAL_PAGEIN
#define	VM_INITIAL_PAGEIN	16
#endif

#endif /* _MACHINE_VMPARAM_H_ */
