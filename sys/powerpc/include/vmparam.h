/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: vmparam.h,v 1.11 2000/02/11 19:25:16 thorpej Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_VMPARAM_H_
#define	_MACHINE_VMPARAM_H_

#define	USRSTACK	SHAREDPAGE

#ifndef	MAXTSIZ
#define	MAXTSIZ		(64*1024*1024)		/* max text size */
#endif

#ifndef	DFLDSIZ
#define	DFLDSIZ		(128*1024*1024)		/* default data size */
#endif

#ifndef	MAXDSIZ
#define	MAXDSIZ		(1*1024*1024*1024)	/* max data size */
#endif

#ifndef	DFLSSIZ
#define	DFLSSIZ		(8*1024*1024)		/* default stack size */
#endif

#ifndef	MAXSSIZ
#define	MAXSSIZ		(64*1024*1024)		/* max stack size */
#endif

#ifdef AIM
#define	VM_MAXUSER_ADDRESS32	((vm_offset_t)0xfffff000)
#else
#define	VM_MAXUSER_ADDRESS32	((vm_offset_t)0x7ffff000)
#endif

/*
 * Would like to have MAX addresses = 0, but this doesn't (currently) work
 */
#if !defined(LOCORE)
#ifdef __powerpc64__
#define	VM_MIN_ADDRESS		(0x0000000000000000UL)
#define	VM_MAXUSER_ADDRESS	(0xfffffffffffff000UL)
#define	VM_MAX_ADDRESS		(0xffffffffffffffffUL)
#else
#define	VM_MIN_ADDRESS		((vm_offset_t)0)
#define	VM_MAXUSER_ADDRESS	VM_MAXUSER_ADDRESS32
#define	VM_MAX_ADDRESS		((vm_offset_t)0xffffffff)
#endif
#define	SHAREDPAGE		(VM_MAXUSER_ADDRESS - PAGE_SIZE)
#else /* LOCORE */
#if !defined(__powerpc64__) && defined(BOOKE)
#define	VM_MIN_ADDRESS		0
#define	VM_MAXUSER_ADDRESS	0x7ffff000
#endif
#endif /* LOCORE */

#define	FREEBSD32_SHAREDPAGE	(VM_MAXUSER_ADDRESS32 - PAGE_SIZE)
#define	FREEBSD32_USRSTACK	FREEBSD32_SHAREDPAGE

#ifdef AIM
#define	KERNBASE		0x00100000UL	/* start of kernel virtual */

#ifdef __powerpc64__
#define	VM_MIN_KERNEL_ADDRESS		0xc000000000000000UL
#define	VM_MAX_KERNEL_ADDRESS		0xc0000001c7ffffffUL
#define	VM_MAX_SAFE_KERNEL_ADDRESS	VM_MAX_KERNEL_ADDRESS
#else
#define	VM_MIN_KERNEL_ADDRESS	((vm_offset_t)KERNEL_SR << ADDR_SR_SHFT)
#define	VM_MAX_SAFE_KERNEL_ADDRESS (VM_MIN_KERNEL_ADDRESS + 2*SEGMENT_LENGTH -1)
#define	VM_MAX_KERNEL_ADDRESS	(VM_MIN_KERNEL_ADDRESS + 3*SEGMENT_LENGTH - 1)
#endif

/*
 * Use the direct-mapped BAT registers for UMA small allocs. This
 * takes pressure off the small amount of available KVA.
 */
#define UMA_MD_SMALL_ALLOC

#else /* Book-E */

#define	KERNBASE		0xc0000000	/* start of kernel virtual */

#define	VM_MIN_KERNEL_ADDRESS	KERNBASE
#define	VM_MAX_KERNEL_ADDRESS	0xf8000000
#define	VM_MAX_SAFE_KERNEL_ADDRESS	VM_MAX_KERNEL_ADDRESS

#endif /* AIM/E500 */

#if !defined(LOCORE)
struct pmap_physseg {
	struct pv_entry *pvent;
	char *attrs;
};
#endif

#define	VM_PHYSSEG_MAX		16	/* 1? */

/*
 * The physical address space is densely populated on 32-bit systems,
 * but may not be on 64-bit ones.
 */
#ifdef __powerpc64__
#define	VM_PHYSSEG_SPARSE
#else
#define	VM_PHYSSEG_DENSE
#endif

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
 * The largest allocation size is 4MB.
 */
#define	VM_NFREEORDER		11

/*
 * Disable superpage reservations.
 */
#ifndef	VM_NRESERVLEVEL
#define	VM_NRESERVLEVEL		0
#endif

#ifndef VM_INITIAL_PAGEIN
#define	VM_INITIAL_PAGEIN	16
#endif

#ifndef SGROWSIZ
#define	SGROWSIZ	(128UL*1024)		/* amount to grow stack */
#endif

/*
 * How many physical pages per kmem arena virtual page.
 */
#ifndef VM_KMEM_SIZE_SCALE
#define	VM_KMEM_SIZE_SCALE	(3)
#endif

/*
 * Optional floor (in bytes) on the size of the kmem arena.
 */
#ifndef VM_KMEM_SIZE_MIN
#define	VM_KMEM_SIZE_MIN	(12 * 1024 * 1024)
#endif

/*
 * Optional ceiling (in bytes) on the size of the kmem arena: 40% of the
 * usable KVA space.
 */
#ifndef VM_KMEM_SIZE_MAX
#define VM_KMEM_SIZE_MAX	((VM_MAX_SAFE_KERNEL_ADDRESS - \
    VM_MIN_KERNEL_ADDRESS + 1) * 2 / 5)
#endif

#define	ZERO_REGION_SIZE	(64 * 1024)	/* 64KB */

#endif /* _MACHINE_VMPARAM_H_ */
