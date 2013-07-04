/*	$NetBSD: vmparam.h,v 1.26 2003/08/07 16:27:47 agc Exp $	*/

/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
 * $FreeBSD$
 */

#ifndef	_MACHINE_VMPARAM_H_
#define	_MACHINE_VMPARAM_H_

/*
 * Machine dependent constants for ARM.
 */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef	MAXTSIZ
#define	MAXTSIZ		(64UL*1024*1024)	/* max text size */
#endif
#ifndef	DFLDSIZ
#define	DFLDSIZ		(128UL*1024*1024)	/* initial data size limit */
#endif
#ifndef	MAXDSIZ
#define	MAXDSIZ		(512UL*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2UL*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(8UL*1024*1024)		/* max stack size */
#endif
#ifndef	SGROWSIZ
#define	SGROWSIZ	(128UL*1024)		/* amount to grow stack */
#endif

/*
 * Address space constants
 */

/*
 * The line between user space and kernel space
 * Mappings >= KERNEL_BASE are constant across all processes
 */
#define	KERNBASE		0xc0000000

/*
 * max number of non-contig chunks of physical RAM you can have
 */

#define	VM_PHYSSEG_MAX		32

/*
 * The physical address space is densely populated.
 */
#define	VM_PHYSSEG_DENSE

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
 * we support 2 free lists:
 *
 *	- DEFAULT for all systems
 *	- ISADMA for the ISA DMA range on Sharks only
 */

#define	VM_NFREELIST		2
#define	VM_FREELIST_DEFAULT	0
#define	VM_FREELIST_ISADMA	1

/*
 * The largest allocation size is 1MB.
 */
#define	VM_NFREEORDER		9

/*
 * Disable superpage reservations.
 */
#ifndef	VM_NRESERVLEVEL
#define	VM_NRESERVLEVEL		0
#endif

#define UPT_MAX_ADDRESS		VADDR(UPTPTDI + 3, 0)
#define UPT_MIN_ADDRESS		VADDR(UPTPTDI, 0)

#define VM_MIN_ADDRESS          (0x00001000)
#ifdef ARM_USE_SMALL_ALLOC
/*
 * ARM_KERN_DIRECTMAP is used to make sure there's enough space between
 * VM_MAXUSER_ADDRESS and KERNBASE to map the whole memory.
 * It has to be a compile-time constant, even if arm_init_smallalloc(),
 * which will do the mapping, gets the real amount of memory at runtime,
 * because VM_MAXUSER_ADDRESS is a constant.
 */
#ifndef ARM_KERN_DIRECTMAP
#define ARM_KERN_DIRECTMAP 512 * 1024 * 1024 /* 512 MB */
#endif
#define VM_MAXUSER_ADDRESS	KERNBASE - ARM_KERN_DIRECTMAP
#else /* ARM_USE_SMALL_ALLOC */
#ifndef VM_MAXUSER_ADDRESS
#define VM_MAXUSER_ADDRESS      KERNBASE
#endif /* VM_MAXUSER_ADDRESS */
#endif /* ARM_USE_SMALL_ALLOC */
#define VM_MAX_ADDRESS          VM_MAXUSER_ADDRESS

#define USRSTACK        VM_MAXUSER_ADDRESS

/* initial pagein size of beginning of executable file */
#ifndef VM_INITIAL_PAGEIN
#define VM_INITIAL_PAGEIN       16
#endif

#ifndef VM_MIN_KERNEL_ADDRESS
#define VM_MIN_KERNEL_ADDRESS KERNBASE
#endif

#define	VM_MAX_KERNEL_ADDRESS	(vm_max_kernel_address)

/*
 * Virtual size (bytes) for various kernel submaps.
 */
#ifndef VM_KMEM_SIZE
#define VM_KMEM_SIZE		(12*1024*1024)
#endif
#ifndef VM_KMEM_SIZE_SCALE
#define VM_KMEM_SIZE_SCALE	(3)
#endif

/*
 * Ceiling on the size of the kmem submap: 40% of the kernel map.
 */
#ifndef VM_KMEM_SIZE_MAX
#define	VM_KMEM_SIZE_MAX	((vm_max_kernel_address - \
    VM_MIN_KERNEL_ADDRESS + 1) * 2 / 5)
#endif

#ifdef ARM_USE_SMALL_ALLOC
#define UMA_MD_SMALL_ALLOC
#endif /* ARM_USE_SMALL_ALLOC */

extern vm_offset_t vm_max_kernel_address;

#define	ZERO_REGION_SIZE	(64 * 1024)	/* 64KB */

#ifndef VM_MAX_AUTOTUNE_MAXUSERS
#define	VM_MAX_AUTOTUNE_MAXUSERS	384
#endif

#endif	/* _MACHINE_VMPARAM_H_ */
