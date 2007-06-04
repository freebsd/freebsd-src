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


/*#include <arm/arm32/vmparam.h>
*/
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
 * Create two free page pools: VM_FREEPOOL_DEFAULT is the default pool
 * from which physical pages are allocated and VM_FREEPOOL_DIRECT is
 * the pool from which physical pages for small UMA objects are
 * allocated.
 */
#define	VM_NFREEPOOL		2
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

#define UPT_MAX_ADDRESS		VADDR(UPTPTDI + 3, 0)
#define UPT_MIN_ADDRESS		VADDR(UPTPTDI, 0)

#define VM_MIN_ADDRESS          (0x00001000)
#ifdef ARM_USE_SMALL_ALLOC
#ifndef ARM_KERN_DIRECTMAP
#define ARM_KERN_DIRECTMAP 512 * 1024 * 1024 /* 512 MB */
#endif
#define VM_MAXUSER_ADDRESS	KERNBASE - ARM_KERN_DIRECTMAP
#else /* ARM_USE_SMALL_ALLOC */
#define VM_MAXUSER_ADDRESS      KERNBASE
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

#define VM_MAX_KERNEL_ADDRESS	0xffffffff
/*
 * Virtual size (bytes) for various kernel submaps.
 */

#ifndef VM_KMEM_SIZE
#define VM_KMEM_SIZE            (12*1024*1024)
#endif

#define MAXTSIZ 	(16*1024*1024)
#define DFLDSIZ         (128*1024*1024)
#define MAXDSIZ         (512*1024*1024)
#define DFLSSIZ         (2*1024*1024)
#define MAXSSIZ         (8*1024*1024)
#define SGROWSIZ        (128*1024)
#define MAXSLP		20

#define VM_PROT_READ_IS_EXEC

#ifdef ARM_USE_SMALL_ALLOC
#define UMA_MD_SMALL_ALLOC
#endif /* ARM_USE_SMALL_ALLOC */
#endif	/* _MACHINE_VMPARAM_H_ */
