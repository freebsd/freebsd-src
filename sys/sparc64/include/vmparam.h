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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *	from: @(#)vmparam.h     5.9 (Berkeley) 5/12/91
 *	from: FreeBSD: src/sys/i386/include/vmparam.h,v 1.33 2000/03/30
 * $FreeBSD$
 */


#ifndef	_MACHINE_VMPARAM_H_
#define	_MACHINE_VMPARAM_H_

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(1*1024*1024*1024)	/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(128*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(1*1024*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(128*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(1*1024*1024*1024)	/* max stack size */
#endif
#ifndef	SGROWSIZ
#define	SGROWSIZ	(128*1024)		/* amount to grow stack */
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
#define	MAXSLP			20

/*
 * Highest user address.  Also address of initial user stack.  This is
 * arbitrary, neither the structure or size of the user page table (tsb)
 * nor the location or size of the kernel virtual address space have any
 * bearing on what we use for user addresses.  We want something relatively
 * high to give a large address space, but we also have to take the out of
 * range va hole into account.  So we pick an address just before the start
 * of the hole, which gives a user address space of just under 8TB.  Note
 * that if this moves above the va hole, we will have to deal with sign
 * extension of virtual addresses.
 */
#define	VM_MAXUSER_ADDRESS	((vm_offset_t)0x7fe00000000)

#define	VM_MIN_ADDRESS		((vm_offset_t)0)
#define	VM_MAX_ADDRESS		(VM_MAXUSER_ADDRESS)

/*
 * Initial user stack address for 64 bit processes.  Should be highest user
 * virtual address.
 */
#define	USRSTACK		VM_MAXUSER_ADDRESS

/*
 * Virtual size (bytes) for various kernel submaps.
 */
#ifndef	VM_KMEM_SIZE
#define	VM_KMEM_SIZE		(16*1024*1024)
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
 * Number of 4 meg pages to use for the kernel tsb.
 */
#ifndef	KVA_PAGES
#define	KVA_PAGES		(1)
#endif

/*
 * Range of kernel virtual addresses.  max = min + range.
 */
#define	KVA_RANGE \
	((KVA_PAGES * PAGE_SIZE_4M) << (PAGE_SHIFT - TTE_SHIFT))

/*
 * Lowest kernel virtual address, where the kernel is loaded.
 *
 * If we are using less than 4 super pages for the kernel tsb, the address
 * space is less than 4 gigabytes, so put it at the end of the first 4
 * gigbytes.  This allows the kernel and the firmware mappings to be mapped
 * with a single contiguous tsb.  Otherwise start at 0, we'll cover them
 * anyway.
 *
 * ie:
 * kva_pages = 1
 *	vm_max_kernel_address	0xffffe000
 *	openfirmware		0xf0000000
 *	kernbase		0xc0000000
 * kva_pages = 8
 *	vm_max_kernel_address	0x1ffffe000
 *	openfirmware		0xf0000000
 *	kernbase		0x0
 *
 * There are at least 4 pages of dynamic linker junk before kernel text begins,
 * so starting at zero is fairly safe (if the firmware will let us).
 */
#if KVA_PAGES < 4
#define	VM_MIN_KERNEL_ADDRESS	((1UL << 32) - KVA_RANGE)
#else
#define	VM_MIN_KERNEL_ADDRESS	(0)
#endif

#define	VM_MAX_KERNEL_ADDRESS	(VM_MIN_KERNEL_ADDRESS + KVA_RANGE - PAGE_SIZE)
#define	KERNBASE		(VM_MIN_KERNEL_ADDRESS)

/*
 * Initial pagein size of beginning of executable file.
 */
#ifndef	VM_INITIAL_PAGEIN
#define	VM_INITIAL_PAGEIN	16
#endif

#endif /* !_MACHINE_VMPARAM_H_ */
