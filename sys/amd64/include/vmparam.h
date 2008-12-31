/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 2003 Peter Wemm
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
 *	from: @(#)vmparam.h	5.9 (Berkeley) 5/12/91
 * $FreeBSD: src/sys/amd64/include/vmparam.h,v 1.49.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */


#ifndef _MACHINE_VMPARAM_H_
#define	_MACHINE_VMPARAM_H_ 1

/*
 * Machine dependent constants for AMD64.
 */

/*
 * Virtual memory related constants, all in bytes
 */
#define	MAXTSIZ		(128UL*1024*1024)	/* max text size */
#ifndef DFLDSIZ
#define	DFLDSIZ		(128UL*1024*1024)	/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(32768UL*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(8UL*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(512UL*1024*1024)	/* max stack size */
#endif
#ifndef SGROWSIZ
#define	SGROWSIZ	(128UL*1024)		/* amount to grow stack */
#endif

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
 * We provide a machine specific single page allocator through the use
 * of the direct mapped segment.  This uses 2MB pages for reduced
 * TLB pressure.
 */
#define	UMA_MD_SMALL_ALLOC

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
#define	VM_PHYSSEG_MAX		31

/*
 * Create three free page pools: VM_FREEPOOL_DEFAULT is the default pool
 * from which physical pages are allocated and VM_FREEPOOL_DIRECT is
 * the pool from which physical pages for page tables and small UMA
 * objects are allocated.
 */
#define	VM_NFREEPOOL		3
#define	VM_FREEPOOL_CACHE	2
#define	VM_FREEPOOL_DEFAULT	0
#define	VM_FREEPOOL_DIRECT	1

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
 * An allocation size of 16MB is supported in order to optimize the
 * use of the direct map by UMA.  Specifically, a cache line contains
 * at most 8 PDEs, collectively mapping 16MB of physical memory.  By
 * reducing the number of distinct 16MB "pages" that are used by UMA,
 * the physical memory allocator reduces the likelihood of both 2MB
 * page TLB misses and cache misses caused by 2MB page TLB misses.
 */
#define	VM_NFREEORDER		13

/*
 * Virtual addresses of things.  Derived from the page directory and
 * page table indexes from pmap.h for precision.
 *
 * 0x0000000000000000 - 0x00007fffffffffff   user map
 * 0x0000800000000000 - 0xffff7fffffffffff   does not exist (hole)
 * 0xffff800000000000 - 0xffff804020100fff   recursive page table (512GB slot)
 * 0xffff804020101000 - 0xfffffeffffffffff   unused
 * 0xffffff0000000000 - 0xffffff7fffffffff   512GB direct map mappings
 * 0xffffff8000000000 - 0xffffffff7fffffff   unused (510GB)
 * 0xffffffff80000000 - 0xffffffffffffffff   2GB kernel map
 *
 * Within the kernel map:
 * 0xffffffff80000000                        KERNBASE
 */

#define	VM_MAX_KERNEL_ADDRESS	KVADDR(KPML4I, NPDPEPG-1, NKPDE-1, NPTEPG-1)
#define	VM_MIN_KERNEL_ADDRESS	KVADDR(KPML4I, KPDPI, 0, 0)

#define	DMAP_MIN_ADDRESS	KVADDR(DMPML4I, 0, 0, 0)
#define	DMAP_MAX_ADDRESS	KVADDR(DMPML4I+1, 0, 0, 0)

#define	KERNBASE		KVADDR(KPML4I, KPDPI, 0, 0)

#define	UPT_MAX_ADDRESS		KVADDR(PML4PML4I, PML4PML4I, PML4PML4I, PML4PML4I)
#define	UPT_MIN_ADDRESS		KVADDR(PML4PML4I, 0, 0, 0)

#define	VM_MAXUSER_ADDRESS	UVADDR(NUPML4E, 0, 0, 0)

#define	USRSTACK		VM_MAXUSER_ADDRESS

#define	VM_MAX_ADDRESS		UPT_MAX_ADDRESS
#define	VM_MIN_ADDRESS		(0)

#define	PHYS_TO_DMAP(x)		((x) | DMAP_MIN_ADDRESS)
#define	DMAP_TO_PHYS(x)		((x) & ~DMAP_MIN_ADDRESS)

/* virtual sizes (bytes) for various kernel submaps */
#ifndef VM_KMEM_SIZE
#define	VM_KMEM_SIZE		(12 * 1024 * 1024)
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
#define	VM_KMEM_SIZE_MAX	(400 * 1024 * 1024)
#endif

/* initial pagein size of beginning of executable file */
#ifndef VM_INITIAL_PAGEIN
#define	VM_INITIAL_PAGEIN	16
#endif

#endif /* _MACHINE_VMPARAM_H_ */
